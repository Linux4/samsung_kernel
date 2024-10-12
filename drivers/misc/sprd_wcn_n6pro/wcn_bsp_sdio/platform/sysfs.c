/*
 * This file is part of wcn sysfs debug
 */

#define DRIVER_AUTHOR "Songhe Wei<songhe.wei@unisoc.com>"
#define DRIVER_DESC "support sysfs and uevent for debug wcn"
#define WCN_SYSFS_VERSION	"v0.0"

#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <misc/wcn_bus.h>

#ifdef CONFIG_WCN_PCIE_REMOVE
#include "pcie.h"
#endif
#ifdef CONFIG_WCN_BSP_SDIO
#include "../sdio/sdiohal.h"
#endif
#include "wcn_dbg.h"
#include "wcn_glb.h"

struct wcn_sysfs_info {
	void *p;
	unsigned char len;
	struct mutex	mutex;
	struct completion cmd_completion;
	atomic_t set_mask;
	/* 0:dumpmem; 1:reset */
	atomic_t is_reset;
	char sw_ver_buf[128];
	size_t sw_ver_len;
	unsigned char armlog_status;
	char loglevel_buf[128];
	size_t loglevel_len;
	unsigned char loglevel;
};

static struct wcn_sysfs_info sysfs_info;

int notify_at_cmd_finish(void *buf, unsigned char len)
{
	sysfs_info.p = buf;
	sysfs_info.len = len;
	complete(&sysfs_info.cmd_completion);

	return 0;
}

static int wcn_send_atcmd(void *cmd, unsigned char cmd_len,
			  void *response, size_t *response_len)
{
	struct mbuf_t *head = NULL;
	struct mbuf_t *tail = NULL;
	int num = 1;
	int ret;
	unsigned long timeleft;
#ifdef CONFIG_WCN_PCIE_REMOVE
	/* dma_buf for dma */
	static struct dma_buf dma_buf;
	static int at_buf_flag;
	struct wcn_pcie_info *pcie_dev;
#else
	/* common buf for kmalloc */
	unsigned char *com_buf = NULL;
#endif

#ifdef CONFIG_WCN_PCIE_REMOVE
	pcie_dev = get_wcn_device_info();
	if (!pcie_dev) {
		WCN_ERR("%s:PCIE device link error\n", __func__);
		return -1;
	}
#endif

#ifdef CONFIG_WCN_BSP_SDIO
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (!WCN_CARD_EXIST(&p_data->xmit_cnt)) {
		WCN_INFO("%s:already power off\n", __func__);
		return 0;
	}
#endif
	mutex_lock(&sysfs_info.mutex);
	ret = sprdwcn_bus_list_alloc(0, &head, &tail, &num);
	if (ret || head == NULL || tail == NULL) {
		WCN_ERR("%s:%d mbuf_link_alloc fail\n", __func__, __LINE__);
		mutex_unlock(&sysfs_info.mutex);
		return -1;
	}

#ifdef CONFIG_WCN_PCIE_REMOVE
	if (at_buf_flag == 0) {
		ret = dmalloc(pcie_dev, &dma_buf, 128);
		if (ret != 0) {
			mutex_unlock(&sysfs_info.mutex);
			return -1;
		}
		at_buf_flag = 1;
	}
	head->buf = (unsigned char *)(dma_buf.vir);
	head->phy = (unsigned long)(dma_buf.phy);
	head->len = cmd_len;
	memset(head->buf, 0x0, head->len);
	memcpy(head->buf, cmd, cmd_len);
	head->next = NULL;
#else
	com_buf = kzalloc(128 + PUB_HEAD_RSV + 1, GFP_KERNEL);
	if (!com_buf) {
		mutex_unlock(&sysfs_info.mutex);
		return -ENOMEM;
	}
	memcpy(com_buf + PUB_HEAD_RSV, cmd, cmd_len);
	head->buf = com_buf;
	head->len = cmd_len;
	head->next = NULL;
#endif
	ret = sprdwcn_bus_push_list(0, head, tail, num);
	if (ret)
		WCN_INFO("sprdwcn_bus_push_list error=%d\n", ret);
	reinit_completion(&sysfs_info.cmd_completion);
	timeleft = wait_for_completion_timeout(&sysfs_info.cmd_completion,
					       3 * HZ);
#ifdef CONFIG_WCN_BSP_SDIO
	if (!WCN_CARD_EXIST(&p_data->xmit_cnt)) {
		WCN_INFO("%s:already power off\n", __func__);
		mutex_unlock(&sysfs_info.mutex);
		return 0;
	}
#endif
	if (!timeleft) {
		WCN_ERR("%s,Timeout(%d sec),didn't get at cmd(%s) response\n",
			__func__, jiffies_to_msecs(3*HZ) / 1000, (char *)cmd);
		mutex_unlock(&sysfs_info.mutex);
		return -ETIMEDOUT;
	}
	if (!response) {
		mutex_unlock(&sysfs_info.mutex);
		return 0;
	}

	*response_len = sysfs_info.len;
	scnprintf(response, (size_t)sysfs_info.len, "%s",
		  (char *)sysfs_info.p);
	WCN_DBG("len=%zu, buf=%s\n", *response_len, (char *)(response));
	mutex_unlock(&sysfs_info.mutex);

	return 0;
}

static int wcn_get_sw_ver(void)
{
	char a[] = "at+spatgetcp2info\r\n";

	/*
	 * len is 64bit, belows function 32bit, need assigned 0 to len,
	 * make sure high 32bit=0
	 */
	wcn_send_atcmd(a, strlen(a), &sysfs_info.sw_ver_buf,
		       &sysfs_info.sw_ver_len);

	WCN_DBG("show:len=%zd, buf=%s\n", sysfs_info.sw_ver_len,
		sysfs_info.sw_ver_buf);

	return 0;
}

static int wcn_set_armlog_status(void)
{
	char a[16];

	scnprintf(a, (size_t)sizeof(a), "%s%d%s", "at+armlog=",
		  sysfs_info.armlog_status, "\r\n");
	WCN_INFO("%s:%s", __func__, a);
	wcn_send_atcmd(a, strlen(a), NULL, NULL);

	return 0;
}

static int wcn_set_loglevel(void)
{
	char a[64];

	if (atomic_read(&sysfs_info.set_mask)
		& WCN_SYSFS_LOGLEVEL_SET_BIT) {
		scnprintf(a, (size_t)sizeof(a), "%s%d%s", "at+loglevel=",
			  sysfs_info.loglevel, "\r\n");
		WCN_INFO("%s:%s\n", __func__, a);
		wcn_send_atcmd(a, strlen(a), NULL, NULL);
		atomic_and((unsigned int)(~(WCN_SYSFS_LOGLEVEL_SET_BIT)),
			   &sysfs_info.set_mask);
	}

	return 0;
}
static int wcn_get_loglevel(void)
{
	unsigned long tmp;
	int ret;
	char a[] = "at+loglevel?\r\n";

	wcn_send_atcmd(a, strlen(a), &sysfs_info.loglevel_buf,
		       &sysfs_info.loglevel_len);

	ret = kstrtoul(sysfs_info.loglevel_buf + 11, 10, &tmp);
	if (ret < 0) {
		WCN_ERR("incorrect get loglevel\n");
		return -EINVAL;
	}

	WCN_DBG("show:len=%zd, buf=%s, level=%d\n", sysfs_info.loglevel_len,
		sysfs_info.loglevel_buf, sysfs_info.loglevel);

	return 0;
}

void wcn_firmware_init(void)
{
	wcn_get_sw_ver();
	wcn_set_armlog_status();
	wcn_set_loglevel();
	wcn_get_loglevel();
	/* TODO: set can pass functionmask */
	/* wcn_set_loglevel, etc */
}

void wcn_firmware_init_wq(struct work_struct *work)
{
	wcn_firmware_init();
}

static ssize_t wcn_sysfs_show_sleep_state(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", "not support to read");
}

static ssize_t wcn_sysfs_store_sleep_state(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	unsigned long res;
	int ret;
	char a[64];

	ret = kstrtoul(buf, 10, &res);
	if (ret < 0) {
		WCN_ERR("incorrect value written to sleep\n");
		return -EINVAL;
	}

	if (res == 1) {
		/* close CP2 sleep function */
		scnprintf(a, (size_t)sizeof(a), "%s", "at+debug=1\r\n");
		wcn_send_atcmd(a, strlen(a), NULL, NULL);
	} else if (res == 0) {
		/* open CP2 sleep function */
		scnprintf(a, (size_t)sizeof(a), "%s", "at+debug=0\r\n");
		wcn_send_atcmd(a, strlen(a), NULL, NULL);
	} else {
		WCN_ERR("incorrect value written to sleep\n");
		return -EINVAL;
	}

	return count;
}

/* S_IRUGO | S_IWUSR */
static DEVICE_ATTR(sleep_state, 0644,
		   wcn_sysfs_show_sleep_state,
		   wcn_sysfs_store_sleep_state);

static ssize_t wcn_sysfs_show_sw_ver(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	size_t len = 0;
	char a[] = "at+spatgetcp2info\r\n";

	if (!marlin_get_module_status()) {
		memcpy(buf, sysfs_info.sw_ver_buf, sysfs_info.sw_ver_len);
		return sysfs_info.sw_ver_len;
	}

	/*
	 * len is 64bit, belows function 32bit, need assigned 0 to len,
	 * make sure high 32bit=0
	 */
	wcn_send_atcmd(a, strlen(a), buf, &len);

	WCN_INFO("show:len=%zd, buf=%s\n", len, buf);
	/* because cp2 pass wrong len */
	len = strlen(buf);
	WCN_INFO("show:len=%zd\n", len);

	return len;
}

/* S_IRUGO */
static DEVICE_ATTR(sw_ver, 0444,
		   wcn_sysfs_show_sw_ver, NULL);

static ssize_t wcn_sysfs_read_fwlog(struct file *filp, struct kobject *kobj,
				    struct bin_attribute *bin_attr,
				    char *buffer, loff_t pos, size_t count)
{
	return 0;
}

/* S_IRUSR */
static const struct bin_attribute fwlog_attr = {
	.attr = {.name = "fwlog", .mode = 0400},
	.read = wcn_sysfs_read_fwlog,
};

static ssize_t wcn_sysfs_show_hw_ver(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	ssize_t len;

	len = PAGE_SIZE;
	mutex_lock(&sysfs_info.mutex);
	len = snprintf(buf, len, "%s\n", wcn_get_chip_name());
	mutex_unlock(&sysfs_info.mutex);

	return len;
}

static ssize_t wcn_sysfs_store_hw_ver(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(hw_ver, 0644,
		   wcn_sysfs_show_hw_ver,
		   wcn_sysfs_store_hw_ver);

static ssize_t wcn_sysfs_show_watchdog_state(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	ssize_t len;

	len = PAGE_SIZE;
	mutex_lock(&sysfs_info.mutex);

	/* len:4 */
	len = snprintf(buf, len, "not support\n");
	mutex_unlock(&sysfs_info.mutex);

	return len;
}

static ssize_t wcn_sysfs_store_watchdog_state(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t count)
{
	size_t len = 0;
	char a[] = "at+cp2_enter_user=?\r\n";

	WCN_INFO("%s: buf=%s\n", __func__, buf);

	if (strncmp(buf, "1", 1) == 0) {
		scnprintf(a, (size_t)sizeof(a), "%s",
			  "at+cp2_enter_user=1\r\n");
		wcn_send_atcmd(a, strlen(a), NULL, NULL);
	} else if (strncmp(buf, "0", 1) == 0) {
		scnprintf(a, (size_t)sizeof(a), "%s",
			  "at+cp2_enter_user=0\r\n");
		wcn_send_atcmd(a, strlen(a), NULL, NULL);
	} else
		return -EINVAL;

	len = strlen(buf);
	WCN_INFO("show:len=%zd, count=%zd\n", len, count);

	return count;
}

static DEVICE_ATTR(watchdog_state, 0644,
		   wcn_sysfs_show_watchdog_state,
		   wcn_sysfs_store_watchdog_state);

static ssize_t wcn_sysfs_show_armlog_status(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	size_t len = 0;
	char a[] = "at+armlog?\r\n";

	wcn_send_atcmd(a, strlen(a), buf, &len);

	WCN_INFO("%s:len=%zd, buf=%s\n", __func__, len, buf);
	len = strlen(buf);
	WCN_INFO("show:len=%zd\n", len);

	return len;
}

static ssize_t wcn_sysfs_store_armlog_status(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	unsigned long res;
	int ret;
	char a[64];

	ret = kstrtoul(buf, 10, &res);
	if (ret < 0) {
		WCN_ERR("incorrect value written to armlog\n");
		return -EINVAL;
	}

	WCN_INFO("%s:set armlog = %ld\n", __func__, res);
	sysfs_info.armlog_status = res;

	if (!marlin_get_module_status())
		return count;

	if (res == 1) {
		/* open CP2 armlog */
		scnprintf(a, (size_t)sizeof(a), "%s", "at+armlog=1\r\n");
		wcn_send_atcmd(a, strlen(a), NULL, NULL);
	} else if (res == 0) {
		/* close CP2 armlog */
		scnprintf(a, (size_t)sizeof(a), "%s", "at+armlog=0\r\n");
		wcn_send_atcmd(a, strlen(a), NULL, NULL);
	} else {
		WCN_ERR("incorrect value(%ld)written to armlog\n", res);
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(armlog_status, 0644,
		   wcn_sysfs_show_armlog_status,
		   wcn_sysfs_store_armlog_status);
/* wcn_sysfs_show_loglevel:len=128, buf=+LOGLEVEL: 0 */
static ssize_t wcn_sysfs_show_loglevel(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	size_t len = 0;
	char a[] = "at+loglevel?\r\n";

	WCN_INFO("%s:buf=%s\n", __func__, buf);
	if (!marlin_get_module_status()) {
		if (sysfs_info.loglevel >= 6)
			sysfs_info.loglevel = 0;
		scnprintf(buf, PAGE_SIZE, "%s: %d", "+LOGLEVEL",
			  sysfs_info.loglevel);
		len = strlen(buf);

		return len;
	}

	wcn_send_atcmd(a, strlen(a), buf, &len);

	WCN_INFO("%s:len=%zd, buf=%s\n", __func__, len, buf);
	len = strlen(buf);
	WCN_INFO("show:len=%zd\n", len);

	return len;
}

static ssize_t wcn_sysfs_store_loglevel(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long res;
	int ret;
	char a[64];

	WCN_INFO("%s:buf=%s\n", __func__, buf);
	ret = kstrtoul(buf, 10, &res);
	if (ret < 0) {
		WCN_ERR("incorrect value written to loglevel\n");
		return -EINVAL;
	}

	if (res >= 6) {
		WCN_ERR("incorrect value(%ld)written to loglevel\n", res);
		return -EINVAL;
	}

	sysfs_info.loglevel = res;

	if (marlin_get_module_status()) {
		scnprintf(a, (size_t)sizeof(a), "%s%ld%s", "at+loglevel=",
			  res, "\r\n");
		WCN_INFO("%s:buf=%s\n", __func__, a);
		wcn_send_atcmd(a, strlen(a), NULL, NULL);
		atomic_and((unsigned int)(~(WCN_SYSFS_LOGLEVEL_SET_BIT)),
			   &sysfs_info.set_mask);
	} else {
		atomic_or(WCN_SYSFS_LOGLEVEL_SET_BIT, &sysfs_info.set_mask);
	}
	WCN_INFO("%s:set_mask:%d\n", __func__,
		 atomic_read(&sysfs_info.set_mask));

	return count;
}

static DEVICE_ATTR(loglevel, 0644,
		   wcn_sysfs_show_loglevel,
		   wcn_sysfs_store_loglevel);

static ssize_t wcn_sysfs_show_reset_dump(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	ssize_t len = PAGE_SIZE;

	if (wcn_sysfs_get_reset_prop())
		len = snprintf(buf, len, "reset\n");
	else
		len = snprintf(buf, len, "dump\n");

	return len;
}

static ssize_t wcn_sysfs_store_reset_dump(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	WCN_INFO("%s: buf=%s\n", __func__, buf);

	if (strncmp(buf, "reset", 5) == 0) {
		atomic_set(&sysfs_info.is_reset, 0x1);
	} else if (strncmp(buf, "dump", 4) == 0) {
		atomic_set(&sysfs_info.is_reset, 0x0);
	} else if (strncmp(buf, "manual_dump", 11) == 0) {
		sprdwcn_bus_set_carddump_status(true);
		wcn_assert_interface(WCN_SOURCE_BTWF, "dumpmem");
	} else
		return -EINVAL;

	return count;
}

int wcn_sysfs_get_reset_prop(void)
{
	return atomic_read(&sysfs_info.is_reset);
}

static DEVICE_ATTR(reset_dump, 0644,
		   wcn_sysfs_show_reset_dump,
		   wcn_sysfs_store_reset_dump);

static ssize_t wcn_sysfs_show_atcmd(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", "not support to read");
}

static ssize_t wcn_sysfs_store_atcmd(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int ret;

	WCN_INFO("%s: buf=%s\n", __func__, buf);

	ret = wcn_send_atcmd((void *)buf, count, NULL, NULL);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(atcmd, 0644,
		   wcn_sysfs_show_atcmd,
		   wcn_sysfs_store_atcmd);

/*
 * ud710_3h10:/sys/devices/platform/sprd-marlin3 # ls
 * sleep_state driver driver_override fwlog hw_pg_ver modalias of_node power
 * subsystem uevent
 * char device: /sys/class/slog_wcn/slog_wcn0
 * misc device: /sys/class/misc/slog_gnss
 */

#if 0
int wcn_sysfs_init(struct marlin_device *mdev)
{
	int ret;

	/* Create sysfs file to control bt coex state */
	ret = device_create_file(mdev->dev[0]->dev, &dev_attr_sleep_state);
	if (ret < 0) {
		WCN_ERR("failed to create sysfs file sleep_state\n");
		goto out;
	}

	/* Create sysfs file to get SW version */
	ret = device_create_file(mdev->dev[0]->dev, &dev_attr_sw_ver);
	if (ret < 0) {
		WCN_ERR("failed to create sysfs file sw_ver\n");
		goto out_sleep_state;
	}

	/* Create sysfs file to get HW version(chipid) */
	ret = device_create_file(mdev->dev[0]->dev, &dev_attr_hw_ver);
	if (ret < 0) {
		WCN_ERR("failed to create sysfs file hw_ver\n");
		goto out_sw_ver;
	}

	/* Create sysfs file to get watchdog status */
	ret = device_create_file(mdev->dev[0]->dev, &dev_attr_watchdog_state);
	if (ret < 0) {
		WCN_ERR("failed to create sysfs file watchdog_state\n");
		goto out_hw_ver;
	}

	/* Create sysfs file to get/change armlog status */
	ret = device_create_file(mdev->dev[0]->dev, &dev_attr_armlog_status);
	if (ret < 0) {
		WCN_ERR("failed to create sysfs file armlog_status\n");
		goto out_watchdog_state;
	}

	/* Create sysfs file to get/set loglevel */
	ret = device_create_file(mdev->dev[0]->dev, &dev_attr_loglevel);
	if (ret < 0) {
		WCN_ERR("failed to create sysfs file armlog_status\n");
		goto out_armlog_status;
	}

	/*
	 * Create sysfs file to get TARGET_BUILD_VARIANT=userdebug for ap
	 * user: reset cp2
	 * userdebug: dumpmem
	 */
	ret = device_create_file(mdev->dev[0]->dev, &dev_attr_reset_dump);
	if (ret < 0) {
		WCN_ERR("failed to create sysfs file userdebug\n");
		goto out_loglevel;
	}

	/* Create sysfs file for the FW log */
	ret = device_create_bin_file(mdev->dev[0]->dev, &fwlog_attr);
	if (ret < 0) {
		WCN_ERR("failed to create sysfs file fwlog\n");
		goto out_reset_dump;
	}

	init_completion(&mdev->sysfs_info.cmd_completion);
	mutex_init(&mdev->sysfs_info.mutex);

	goto out;

out_reset_dump:
	device_remove_file(&mdev->dev[0]->dev, &dev_attr_reset_dump);

out_loglevel:
	device_remove_file(&mdev->dev[0]->dev, &dev_attr_loglevel);

out_armlog_status:
	device_remove_file(&mdev->dev[0]->dev, &dev_attr_armlog_status);

out_watchdog_state:
	device_remove_file(&mdev->dev[0]->dev, &dev_attr_watchdog_state);

out_hw_ver:
	device_remove_file(&mdev->dev[0]->dev, &dev_attr_hw_ver);

out_sw_ver:
	device_remove_file(&mdev->dev[0]->dev, &dev_attr_sw_ver);

out_sleep_state:
	device_remove_file(&mdev->dev[0]->dev, &dev_attr_sleep_state);

out:
	return ret;
}

void wcn_sysfs_free(struct marlin_device *mdev)
{
	device_remove_bin_file(mdev->dev[0]->dev, &fwlog_attr);

	device_remove_file(mdev->dev[0]->dev, &dev_attr_reset_dump);

	device_remove_file(mdev->dev[0]->dev, &dev_attr_loglevel);

	device_remove_file(mdev->dev[0]->dev, &dev_attr_armlog_status);

	device_remove_file(mdev->dev[0]->dev, &dev_attr_watchdog_state);

	device_remove_file(mdev->dev[0]->dev, &dev_attr_hw_ver);

	device_remove_file(mdev->dev[0]->dev, &dev_attr_sw_ver);

	device_remove_file(mdev->dev[0]->dev, &dev_attr_sleep_state);
}
#endif

static long wcn_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static int wcn_open(struct inode *inode, struct file *filep)
{
	return 0;
}
static ssize_t wcn_read_data(struct file *filp, char __user *buf,
			     size_t count, loff_t *pos)
{
	return 0;
}

static int wcn_release(struct inode *inode, struct file *filep)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long wcn_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long data)
{
	cmd = cmd & 0xFFF0FFFF;
	cmd = cmd | 0x00080000;
	return wcn_ioctl(file, cmd, data);
}
#endif

static const struct file_operations wcn_misc_fops = {
	.owner = THIS_MODULE,
	.open = wcn_open,
	.read = wcn_read_data,
	.unlocked_ioctl = wcn_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = wcn_compat_ioctl,
#endif
	.release = wcn_release,
};

static struct attribute *wcn_attrs[] = {
	&dev_attr_sleep_state.attr,
	&dev_attr_sw_ver.attr,
	&dev_attr_hw_ver.attr,
	&dev_attr_watchdog_state.attr,
	&dev_attr_armlog_status.attr,
	&dev_attr_loglevel.attr,
	&dev_attr_reset_dump.attr,
	&dev_attr_atcmd.attr,
	NULL,
};


static struct attribute_group wcn_attribute_group = {
	.name = "devices",
	.attrs = wcn_attrs,
};

static struct miscdevice wcn_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "wcn",
	.fops = &wcn_misc_fops,
};

void wcn_notify_fw_error(enum wcn_source_type type, char *buf)
{
	int len;
	char *pbuf;
	char *envp[4];

	WCN_ERR("Notify firmware error:%s\n", buf);
	len = strlen(buf) + strlen(WCN_UEVENT_REASON) + 1;
	if (len > 256)
		len = 256;
	pbuf = kmalloc(len, GFP_KERNEL);
	if (!pbuf) {
		WCN_ERR("no mem for notify fw error\n");
		pbuf = kmalloc(64, GFP_KERNEL);
		if (!pbuf) {
			WCN_ERR("notify fw error faile\n");
			return;
		}
	}

	snprintf(pbuf, len, "%s%s", WCN_UEVENT_REASON, buf);

	if (type == WCN_SOURCE_BTWF)
		envp[0] = "SOURCE=WCN-CP2-EXCEPTION";
	else if (type == WCN_SOURCE_GNSS)
		envp[0] = "SOURCE=WCN-GE2-EXCEPTION";
	else
		envp[0] = "SOURCE=WCN";
	envp[1] = WCN_UEVENT_FW_ERRO;
	envp[2] = pbuf;
	envp[3] = NULL;

	kobject_uevent_env(&wcn_misc_device.this_device->kobj,
			   KOBJ_CHANGE, envp);

	kfree(pbuf);
}

//static int __init init_wcn_sysfs(void)
int init_wcn_sysfs(void)
{
	int ret;

	WCN_INFO("%s\n", __func__);
	ret = misc_register(&wcn_misc_device);
	if (ret)
		WCN_ERR("wcn misc dev register fail\n");

	ret = sysfs_create_group(&wcn_misc_device.this_device->kobj,
			&wcn_attribute_group);
	if (ret) {
		WCN_ERR("%s failed to create device group.\n",
			__func__);
		return -1;
	}

	ret = sysfs_create_bin_file(&wcn_misc_device.this_device->kobj,
				&fwlog_attr);
	if (ret) {
		WCN_ERR("%s failed to create bin file.\n",
			__func__);
		return -1;
	}

	init_completion(&sysfs_info.cmd_completion);
	mutex_init(&sysfs_info.mutex);
	atomic_set(&sysfs_info.set_mask, 0x0);

#ifdef CONFIG_WCN_USER
		atomic_set(&sysfs_info.is_reset, 0x1);
		sysfs_info.armlog_status = 0;
#else
		atomic_set(&sysfs_info.is_reset, 0x0);
		sysfs_info.armlog_status = 1;
#endif

	return 0;
}

//static void __exit exit_wcn_sysfs(void)
void exit_wcn_sysfs(void)
{
	misc_deregister(&wcn_misc_device);
}

//module_init(init_wcn_sysfs);
//module_exit(exit_wcn_sysfs);
//MODULE_DESCRIPTION(DRIVER_DESC);
//MODULE_AUTHOR(DRIVER_AUTHOR);
//MODULE_LICENSE("GPL");
//MODULE_VERSION(WCN_SYSFS_VERSION);

