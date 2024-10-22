// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include "adsp_helper.h"
#include "scp_audio_ipi.h"
#include "scp_audio_logger.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp_helper.h"
#endif

#define LOGGER_DEBUG 0
#if (LOGGER_DEBUG == 1)
#define LOGGER_D(...)		pr_debug(...)
#else
#define LOGGER_D(...)
#endif

#define ROUNDUP(a, b)		(((a) + ((b)-1)) & ~((b)-1))
#define PLT_LOG_ENABLE		0x504C5402 /*magic*/

static unsigned int logger_inited;
static struct mutex logger_lock;
static struct log_ctrl_s *logger_ctrl;
static struct buffer_info_s *buf_info;

static unsigned int logger_enable;

static ssize_t scp_audio_logger_read(struct file *file, char __user *data,
				     size_t len, loff_t *ppos)
{
	unsigned int w_pos, r_pos, datalen = 0;
	void *buf;

	LOGGER_D("[SCP AUDIO] %s()\n", __func__);

	if (!logger_inited)
		return 0;

	mutex_lock(&logger_lock);

	r_pos = buf_info->r_pos;
	w_pos = buf_info->w_pos;

	if (r_pos == w_pos)
		goto EXIT;
	else if (r_pos > w_pos)
		datalen = logger_ctrl->buff_size - r_pos;
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	if (r_pos >= logger_ctrl->buff_size) {
		pr_err("[SCP AUDIO] %s() r_pos 0x%x >= buff_size 0x%x\n",
		       __func__, r_pos, logger_ctrl->buff_size);
		datalen = 0;
		goto EXIT;
	}

	buf = ((char *)logger_ctrl) + logger_ctrl->buff_ofs + r_pos;
	if (copy_to_user(data, buf, datalen))
		pr_err("[SCP AUDIO] copy to user buf failed!\n");

	r_pos += datalen;
	if (r_pos >= logger_ctrl->buff_size)
		r_pos -= logger_ctrl->buff_size;

	buf_info->r_pos = r_pos;
EXIT:
	LOGGER_D("[SCP AUDIO] %s() r_pos 0x%x w_pos 0x%x datalen 0x%x\n",
		 __func__, r_pos, w_pos, datalen);
	mutex_unlock(&logger_lock);
	return datalen;
}

static int scp_audio_logger_open(struct inode *inode, struct file *file)
{
	LOGGER_D("[SCP AUDIO] %s()\n", __func__);
	return nonseekable_open(inode, file);
}

static unsigned int scp_audio_logger_poll(struct file *file,
					  struct poll_table_struct *poll)
{
	LOGGER_D("[SCP AUDIO] %s()\n", __func__);

	if (!logger_inited || !(file->f_mode & FMODE_READ))
		return 0;

	if (!logger_ctrl || !buf_info)
		return POLLERR;

	if (buf_info->r_pos >= logger_ctrl->buff_size ||
	    buf_info->w_pos >= logger_ctrl->buff_size)
		return POLLERR;

	if (buf_info->r_pos != buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	return 0;
}

const struct file_operations scp_audio_logger_file_ops = {
	.owner = THIS_MODULE,
	.read = scp_audio_logger_read,
	.open = scp_audio_logger_open,
	.poll = scp_audio_logger_poll,
};

static struct miscdevice mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "adsp_0", // reuse adsp_0 node
	.fops = &scp_audio_logger_file_ops,
};

static void scp_audio_logger_init_handler(int id, void *data, unsigned int len)
{
	LOGGER_D("[SCP AUDIO] %s()\n", __func__);
	logger_inited = 1;
}

static int scp_audio_logger_init_message(void)
{
	int ret;
	unsigned int val = 0;
	unsigned int retry_count = 10;

	LOGGER_D("[SCP AUDIO] %s() logger_inited %d\n", __func__, logger_inited);

	if (logger_inited)
		return 0;

	do {
		retry_count--;
		ret = scp_send_message(SCP_AUDIO_IPI_LOGGER_INIT, &val, sizeof(val), 20, 0);
		if (ret != ADSP_IPI_DONE)
			usleep_range(1000, 1500);
	} while ((retry_count > 0) && (ret != ADSP_IPI_DONE));

	if (ret != ADSP_IPI_DONE)
		pr_err("[SCP AUDIO] %s() ret %d\n", __func__, ret);

	return ret;
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
/* SCP reboot */
static int audio_logger_ctrl_event_receive_scp(
	struct notifier_block *this,
	unsigned long event,
	void *ptr)
{
	switch (event) {
	case SCP_EVENT_STOP:
		logger_inited = 0;
		break;
	case SCP_EVENT_READY:
		scp_audio_logger_init_message();
		break;
	default:
		pr_info("event %lu err", event);
	}
	return 0;
}

static struct notifier_block audio_logger_ctrl_notifier_scp = {
	.notifier_call = audio_logger_ctrl_event_receive_scp,
};
#endif

int scp_audio_logger_init(struct platform_device *pdev)
{

	int ret = -1;
	int last_ofs = 0;
	void *addr = NULL;
	uint64_t size = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)

	mutex_init(&logger_lock);

	/* setup scp audio logger */
	addr = adsp_get_reserve_mem_virt(ADSP_A_LOGGER_MEM_ID);
	size = adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);
	if (!addr || size == 0) {
		pr_err("%s, get reserved memory failed, addr 0x%llx size 0x%llx\n",
		       __func__, (uint64_t)addr, size);
		ret = -ENOMEM;
		goto ERROR;
	}

	logger_ctrl = (struct log_ctrl_s *)addr;
	logger_ctrl->base = PLT_LOG_ENABLE;
	logger_ctrl->enable = 0;
	logger_ctrl->size = sizeof(*logger_ctrl);

	last_ofs += logger_ctrl->size;
	logger_ctrl->info_ofs = last_ofs;

	last_ofs += sizeof(*buf_info);
	last_ofs = ROUNDUP(last_ofs, 4);
	logger_ctrl->buff_ofs = last_ofs;
	logger_ctrl->buff_size = size - last_ofs;

	buf_info = (struct buffer_info_s *)
		(((unsigned char *) logger_ctrl) + logger_ctrl->info_ofs);
	buf_info->r_pos = 0;
	buf_info->w_pos = 0;

	last_ofs += logger_ctrl->buff_size;
	if (last_ofs > size) {
		pr_err("%s fail! last_ofs:0x%x, size:0x%llx\n", __func__, last_ofs, size);
		ret = -EINVAL;
		goto ERROR;
	}

	/* register logger IPI */
	scp_audio_ipi_registration(SCP_AUDIO_IPI_LOGGER_INIT,
				   scp_audio_logger_init_handler,
				   "logger init");

	scp_A_register_notify(&audio_logger_ctrl_notifier_scp);

	if (is_scp_ready(SCP_A_ID))
		scp_audio_logger_init_message();

	ret = misc_register(&mdev);
	if (ret) {
		pr_err("%s, cannot register misc device\n", __func__);
		goto ERROR;
	}

	ret = device_create_file(mdev.this_device, &dev_attr_log_enable);
	if (ret) {
		pr_err("%s, cannot create dev_attr_log_enable\n", __func__);
		goto ERROR;
	}

	pr_info("%s, done ret:%d", __func__, ret);
	return ret;

#endif /* CONFIG_MTK_TINYSYS_SCP_SUPPORT */

ERROR:
	logger_inited = 0;
	logger_ctrl = NULL;
	buf_info = NULL;
	pr_err("%s, failed ret:%d", __func__, ret);
	return ret;
}

static inline ssize_t log_enable_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	unsigned int n = 0;
	unsigned int status;

	status = logger_inited && logger_enable;
	n = snprintf(buf, 128, "[SCP AUDIO] mobile log is %s\n",
		     (status == 0x1) ? "enabled" : "disabled");
	if (n > 128)
		n = 128;
	return n;
}

static inline ssize_t log_enable_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret = -1;
	unsigned int enable = 0;
	unsigned int retry_count = 10;

	if (!logger_inited)
		return -EINVAL;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&logger_lock);
	logger_enable = enable;
	logger_ctrl->enable = enable;

	do {
		retry_count--;
		ret = scp_send_message(SCP_AUDIO_IPI_LOGGER_ENABLE, &enable, sizeof(enable), 20, 0);
		if (ret != ADSP_IPI_DONE)
			usleep_range(1000, 1500);
	} while ((retry_count > 0) && (ret != ADSP_IPI_DONE));

	if (ret != ADSP_IPI_DONE)
		pr_err("%s scp_send_message failed, ret = %d\n", __func__, ret);

	mutex_unlock(&logger_lock);

	LOGGER_D("%s, [SCP AUDIO] enable = %d\n", __func__, enable);
	return count;
}
DEVICE_ATTR_RW(log_enable);
