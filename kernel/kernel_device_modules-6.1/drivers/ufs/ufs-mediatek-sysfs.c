// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 *	Peter Wang <peter.wang@mediatek.com>
 */

#include <linux/async.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include "ufshcd-priv.h"
#include "ufs-mediatek.h"
#include "ufs-mediatek-dbg.h"

/**
 * ufs_mtk_query_ioctl - perform user read queries
 * @hba: per-adapter instance
 * @lun: used for lun specific queries
 * @buffer: user space buffer for reading and submitting query data and params
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_query_data.
 * It will read the opcode, idn and buf_length parameters, and, put the
 * response in the buffer field while updating the used size in buf_length.
 */
static int
ufs_mtk_query_ioctl(struct ufs_hba *hba, u8 lun, void __user *buffer)
{
	struct ufs_ioctl_query_data *ioctl_data;
	int err = 0;
	int length = 0;
	void *data_ptr;
	bool flag;
	u32 att = 0;
	u8 index = 0;
	u8 *desc = NULL;

	ioctl_data = kzalloc(sizeof(*ioctl_data), GFP_KERNEL);
	if (!ioctl_data) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(ioctl_data, buffer,
			     sizeof(struct ufs_ioctl_query_data));
	if (err) {
		dev_info(hba->dev,
			"%s: Failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

	/* verify legal parameters & send query */
	switch (ioctl_data->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		switch (ioctl_data->idn) {
		case QUERY_DESC_IDN_DEVICE:
		case QUERY_DESC_IDN_CONFIGURATION:
		case QUERY_DESC_IDN_INTERCONNECT:
		case QUERY_DESC_IDN_GEOMETRY:
		case QUERY_DESC_IDN_POWER:
			index = 0;
			break;
		case QUERY_DESC_IDN_UNIT:
			if (!ufs_is_valid_unit_desc_lun(&hba->dev_info, lun)) {
				dev_info(hba->dev,
					"%s: No unit descriptor for lun 0x%x\n",
					__func__, lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			index = lun;
			break;
		default:
			goto out_einval;
		}
		length = min_t(int, QUERY_DESC_MAX_SIZE,
			       ioctl_data->buf_size);
		desc = kzalloc(length, GFP_KERNEL);
		if (!desc) {
			dev_info(hba->dev, "%s: Failed allocating %d bytes\n",
				__func__, length);
			err = -ENOMEM;
			goto out_release_mem;
		}
		err = ufshcd_query_descriptor_retry(hba, ioctl_data->opcode,
						    ioctl_data->idn, index, 0,
						    desc, &length);
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		switch (ioctl_data->idn) {
		case QUERY_ATTR_IDN_BOOT_LU_EN:
		case QUERY_ATTR_IDN_POWER_MODE:
		case QUERY_ATTR_IDN_ACTIVE_ICC_LVL:
		case QUERY_ATTR_IDN_OOO_DATA_EN:
		case QUERY_ATTR_IDN_BKOPS_STATUS:
		case QUERY_ATTR_IDN_PURGE_STATUS:
		case QUERY_ATTR_IDN_MAX_DATA_IN:
		case QUERY_ATTR_IDN_MAX_DATA_OUT:
		case QUERY_ATTR_IDN_REF_CLK_FREQ:
		case QUERY_ATTR_IDN_CONF_DESC_LOCK:
		case QUERY_ATTR_IDN_MAX_NUM_OF_RTT:
		case QUERY_ATTR_IDN_EE_CONTROL:
		case QUERY_ATTR_IDN_EE_STATUS:
		case QUERY_ATTR_IDN_SECONDS_PASSED:
			index = 0;
			break;
		case QUERY_ATTR_IDN_DYN_CAP_NEEDED:
		case QUERY_ATTR_IDN_CORR_PRG_BLK_NUM:
			index = lun;
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_attr(hba, ioctl_data->opcode,
					ioctl_data->idn, index, 0, &att);
		break;

	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		err = copy_from_user(&att,
				     ioctl_data->buffer,
				     min_t(u32, sizeof(u32), ioctl_data->buf_size));
		if (err) {
			dev_info(hba->dev,
				"%s: Failed copying buffer from user, err %d\n",
				__func__, err);
			goto out_release_mem;
		}

		switch (ioctl_data->idn) {
		case QUERY_ATTR_IDN_BOOT_LU_EN:
			index = 0;
			if (!att) {
				dev_info(hba->dev,
					"%s: Illegal ufs query ioctl data, opcode 0x%x, idn 0x%x, att 0x%x\n",
					__func__, ioctl_data->opcode,
					(unsigned int)ioctl_data->idn, att);
				err = -EINVAL;
				goto out_release_mem;
			}
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_attr(hba, ioctl_data->opcode,
					ioctl_data->idn, index, 0, &att);
		break;

	case UPIU_QUERY_OPCODE_READ_FLAG:
		switch (ioctl_data->idn) {
		case QUERY_FLAG_IDN_FDEVICEINIT:
		case QUERY_FLAG_IDN_PERMANENT_WPE:
		case QUERY_FLAG_IDN_PWR_ON_WPE:
		case QUERY_FLAG_IDN_BKOPS_EN:
		case QUERY_FLAG_IDN_PURGE_ENABLE:
		case QUERY_FLAG_IDN_FPHYRESOURCEREMOVAL:
		case QUERY_FLAG_IDN_BUSY_RTC:
			break;
		default:
			goto out_einval;
		}
		err = ufshcd_query_flag(hba, ioctl_data->opcode,
					ioctl_data->idn, 0, &flag);
		break;
	default:
		goto out_einval;
	}

	if (err) {
		dev_info(hba->dev, "%s: Query for idn %d failed\n", __func__,
			ioctl_data->idn);
		goto out_release_mem;
	}

	/*
	 * copy response data
	 * As we might end up reading less data than what is specified in
	 * "ioctl_data->buf_size". So we are updating "ioctl_data->
	 * buf_size" to what exactly we have read.
	 */
	switch (ioctl_data->opcode) {
	case UPIU_QUERY_OPCODE_READ_DESC:
		ioctl_data->buf_size = min_t(int, ioctl_data->buf_size, length);
		data_ptr = desc;
		break;
	case UPIU_QUERY_OPCODE_READ_ATTR:
		ioctl_data->buf_size = sizeof(u32);
		data_ptr = &att;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		ioctl_data->buf_size = 1;
		data_ptr = &flag;
		break;
	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		goto out_release_mem;
	default:
		goto out_einval;
	}

	/* copy to user */
	err = copy_to_user(buffer, ioctl_data,
			   sizeof(struct ufs_ioctl_query_data));
	if (err)
		dev_info(hba->dev, "%s: Failed copying back to user.\n",
			__func__);
	err = copy_to_user(buffer + sizeof(struct ufs_ioctl_query_data),
			   data_ptr, ioctl_data->buf_size);
	if (err)
		dev_info(hba->dev, "%s: err %d copying back to user.\n",
			__func__, err);
	goto out_release_mem;

out_einval:
	dev_info(hba->dev,
		"%s: illegal ufs query ioctl data, opcode 0x%x, idn 0x%x\n",
		__func__, ioctl_data->opcode, (unsigned int)ioctl_data->idn);
	err = -EINVAL;
out_release_mem:
	kfree(ioctl_data);
	kfree(desc);
out:
	return err;
}

/**
 * ufs_mtk_ioctl - ufs ioctl callback registered in scsi_host
 * @dev: scsi device required for per LUN queries
 * @cmd: command opcode
 * @buffer: user space buffer for transferring data
 *
 * Supported commands:
 * UFS_IOCTL_QUERY
 */
static int
ufs_mtk_ioctl(struct scsi_device *dev, unsigned int cmd, void __user *buffer)
{
	struct ufs_hba *hba = shost_priv(dev->host);
	int err = 0;

	if (!buffer) {
		dev_info(hba->dev, "%s: User buffer is NULL!\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case UFS_IOCTL_QUERY:
		pm_runtime_get_sync(hba->dev);
		err = ufs_mtk_query_ioctl(hba,
					  ufshcd_scsi_to_upiu_lun(dev->lun),
					  buffer);
		pm_runtime_put_sync(hba->dev);
		break;
	default:
		err = -ENOIOCTLCMD;
		dev_dbg(hba->dev, "%s: Unsupported ioctl cmd %d\n", __func__,
			cmd);
		break;
	}

	return err;
}

static ssize_t downdifferential_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", hba->vps->ondemand_data.downdifferential);
}

static ssize_t downdifferential_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int err = 0;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	mutex_lock(&hba->devfreq->lock);
	if (value > 100 || value > hba->vps->ondemand_data.upthreshold) {
		err = -EINVAL;
		goto out;
	}
	hba->vps->ondemand_data.downdifferential = value;

out:
	mutex_unlock(&hba->devfreq->lock);
	return err ? err : count;
}

static ssize_t upthreshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", hba->vps->ondemand_data.upthreshold);
}

static ssize_t upthreshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int err = 0;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	mutex_lock(&hba->devfreq->lock);
	if (value > 100 || value < hba->vps->ondemand_data.downdifferential) {
		err = -EINVAL;
		goto out;
	}
	hba->vps->ondemand_data.upthreshold = value;

out:
	mutex_unlock(&hba->devfreq->lock);
	return err ? err : count;
}

static ssize_t clkscale_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	ssize_t size = 0;
	int value;

	value = atomic_read((&host->clkscale_control));

	size += sprintf(buf + size, "current: %d\n", value);
	size += sprintf(buf + size, "===== control manual =====\n");
	size += sprintf(buf + size, "0: free run\n");
	size += sprintf(buf + size, "1: scale down\n");
	size += sprintf(buf + size, "2: scale up\n");
	size += sprintf(buf + size, "3: scale down and frobid change\n");
	size += sprintf(buf + size, "4: scale up and frobid change\n");
	size += sprintf(buf + size, "5: allow change and free run\n");

	return size;
}

static ssize_t clkscale_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	const char *opcode = buf;
	u32 value;

	if (!strncmp(buf, "powerhal_set: ", 14))
		opcode = buf + 14;

	if (kstrtou32(opcode, 0, &value) || value > 5)
		return -EINVAL;

	/* Only UFS 4.0 device need  */
	if (hba->dev_info.wspecversion < 0x0400)
		return count;

	atomic_set(&host->clkscale_control, value);

	switch (value) {
	case 0: /* free run */
		ufs_mtk_dynamic_clock_scaling(hba, CLK_SCALE_FREE_RUN);
		break;

	case 1: /* scale down */
		ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_DOWN);
		break;

	case 2: /* scale up */
		ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_UP);
		break;

	case 3: /* scale down and not allow change anymore */
		ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_DOWN);
		host->clk_scale_forbid = true;
		break;

	case 4: /* scale up and not allow change anymore */
		ufs_mtk_dynamic_clock_scaling(hba, CLK_FORCE_SCALE_UP);
		host->clk_scale_forbid = true;
		break;

	case 5: /* free run and allow change */
		host->clk_scale_forbid = false;
		ufs_mtk_dynamic_clock_scaling(hba, CLK_SCALE_FREE_RUN);
		break;

	default:
		break;
	}

	return count;
}

static DEVICE_ATTR_RW(downdifferential);
static DEVICE_ATTR_RW(upthreshold);
static DEVICE_ATTR_RW(clkscale_control);

static struct attribute *ufs_mtk_sysfs_clkscale_attrs[] = {
	&dev_attr_downdifferential.attr,
	&dev_attr_upthreshold.attr,
	&dev_attr_clkscale_control.attr,
	NULL
};

struct attribute_group ufs_mtk_sysfs_clkscale_group = {
	.name = "clkscale",
	.attrs = ufs_mtk_sysfs_clkscale_attrs,
};

static void init_clk_scaling_sysfs(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	atomic_set(&host->clkscale_control, 0);
	if (sysfs_create_group(&hba->dev->kobj, &ufs_mtk_sysfs_clkscale_group))
		dev_info(hba->dev, "Failed to create sysfs for clkscale_control\n");
}

static void remove_clk_scaling_sysfs(struct ufs_hba *hba)
{
	sysfs_remove_group(&hba->dev->kobj, &ufs_mtk_sysfs_clkscale_group);
}

static int write_irq_affinity(unsigned int irq, const char *buf)
{
	cpumask_var_t new_mask;
	int ret;

	if (!zalloc_cpumask_var(&new_mask, GFP_KERNEL))
		return -ENOMEM;

	ret = cpumask_parse(buf, new_mask);
	if (ret)
		goto free;

	if (!cpumask_intersects(new_mask, cpu_online_mask)) {
		ret = -EINVAL;
		goto free;
	}

	ret = irq_set_affinity(irq, new_mask);

free:
	free_cpumask_var(new_mask);
	return ret;
}

static ssize_t smp_affinity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct irq_desc *desc = irq_to_desc(hba->irq);
	const struct cpumask *mask;

	mask = desc->irq_common_data.affinity;
#ifdef CONFIG_GENERIC_PENDING_IRQ
	if (irqd_is_setaffinity_pending(&desc->irq_data))
		mask = desc->pending_mask;
#endif

	return sprintf(buf, "%*pb\n", cpumask_pr_args(mask));
}

static ssize_t smp_affinity_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	unsigned long mask;
	int ret = count;

	if (kstrtoul(buf, 16, &mask) || mask >= 256)
		return -EINVAL;

	ret = write_irq_affinity(hba->irq, buf);
	if (!ret)
		ret = count;

	dev_info(hba->dev, "set irq affinity %lx\n", mask);

	return count;
}

static DEVICE_ATTR_RW(smp_affinity);

static struct attribute *ufs_mtk_sysfs_irq_attrs[] = {
	&dev_attr_smp_affinity.attr,
	NULL
};

struct attribute_group ufs_mtk_sysfs_irq_group = {
	.name = "irq",
	.attrs = ufs_mtk_sysfs_irq_attrs,
};

void ufs_mtk_init_ioctl(struct ufs_hba *hba)
{
	/* Provide SCSI host ioctl API */
	hba->host->hostt->ioctl = (int (*)(struct scsi_device *,
				   unsigned int,
				   void __user *))ufs_mtk_ioctl;
#ifdef CONFIG_COMPAT
	hba->host->hostt->compat_ioctl = (int (*)(struct scsi_device *,
					  unsigned int,
					  void __user *))ufs_mtk_ioctl;
#endif
}
EXPORT_SYMBOL_GPL(ufs_mtk_init_ioctl);

static void init_irq_sysfs(struct ufs_hba *hba)
{
	if (sysfs_create_group(&hba->dev->kobj, &ufs_mtk_sysfs_irq_group))
		dev_info(hba->dev, "Failed to create sysfs for irq\n");
}

static void remove_irq_sysfs(struct ufs_hba *hba)
{
	sysfs_remove_group(&hba->dev->kobj, &ufs_mtk_sysfs_irq_group);
}

static ssize_t dbg_tp_unregister_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	ssize_t size = 0;
	int value;

	value = atomic_read(&host->dbg_tp_unregister);
	size += sprintf(buf + size, "%d\n", value);

	return size;
}

static ssize_t dbg_tp_unregister_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	const char *opcode = buf;
	u32 value;

	if (kstrtou32(opcode, 0, &value) || value > 1)
		return -EINVAL;

	if (value == atomic_xchg(&host->dbg_tp_unregister, value))
		return count;

	if (!is_mcq_enabled(hba))
		return count;

	if (value)
		ufs_mtk_dbg_tp_unregister();
	else
		ufs_mtk_dbg_tp_register();

	return count;
}

static ssize_t skip_blocktag_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	ssize_t size = 0;
	int value;

	value = atomic_read((&host->skip_btag));
	size += sprintf(buf + size, "%d\n", value);

	return size;
}

static ssize_t skip_blocktag_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	const char *opcode = buf;
	u32 value;

	if (kstrtou32(opcode, 0, &value) || value > 1)
		return -EINVAL;

	if (is_mcq_enabled(hba))
		atomic_set(&host->skip_btag, value);

	return count;
}

static DEVICE_ATTR_RW(skip_blocktag);
static DEVICE_ATTR_RW(dbg_tp_unregister);

static struct attribute *ufs_mtk_sysfs_attrs[] = {
	&dev_attr_skip_blocktag.attr,
	&dev_attr_dbg_tp_unregister.attr,
	NULL
};

struct attribute_group ufs_mtk_sysfs_group = {
	.attrs = ufs_mtk_sysfs_attrs,
};

void ufs_mtk_init_sysfs(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	atomic_set(&host->skip_btag, 0);
	atomic_set(&host->dbg_tp_unregister, 0);
	if (sysfs_create_group(&hba->dev->kobj, &ufs_mtk_sysfs_group))
		dev_info(hba->dev, "Failed to create sysfs for btag\n");

	if (hba->caps & UFSHCD_CAP_CLK_SCALING)
		init_clk_scaling_sysfs(hba);

	init_irq_sysfs(hba);
}
EXPORT_SYMBOL_GPL(ufs_mtk_init_sysfs);

void ufs_mtk_remove_sysfs(struct ufs_hba *hba)
{
	sysfs_remove_group(&hba->dev->kobj, &ufs_mtk_sysfs_group);

	if (hba->caps & UFSHCD_CAP_CLK_SCALING)
		remove_clk_scaling_sysfs(hba);

	remove_irq_sysfs(hba);
}
EXPORT_SYMBOL_GPL(ufs_mtk_remove_sysfs);
