// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/vibrator/sec_vibrator_inputff.h>
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
#include <linux/usb_notify.h>
#endif
#if defined(CONFIG_SEC_KUNIT)
#include "kunit_test/sec_vibrator_inputff_test.h"
#else
#define __visible_for_testing static
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

__visible_for_testing int sec_vib_inputff_get_i2s_test(
	struct sec_vib_inputff_drvdata *ddata)
{
	int ret = 0;

	if (ddata->vib_ops->get_i2s_test)
		ret = ddata->vib_ops->get_i2s_test(ddata->input);
	dev_info(ddata->dev, "%s ret : %d\n", __func__, ret);
	return ret;
}

static ssize_t i2s_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", sec_vib_inputff_get_i2s_test(ddata));
}
static DEVICE_ATTR_RO(i2s_test);

__visible_for_testing int sec_vib_inputff_load_firmware(
	struct sec_vib_inputff_drvdata *ddata, int fw_id)
{
	int ret = 0;
	int retry = 0;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif
	if (!ddata->vib_ops || !ddata->vib_ops->fw_load) {
		dev_err(ddata->dev, "%s Fail to find pointer\n", __func__);
		ret = -ENOENT;
		goto err;
	}

	dev_info(ddata->dev, "%s ret(%d), fw_id(%d) retry(%d) stat(%d)\n",
		__func__, ret, ddata->fw.id, ddata->fw.retry, ddata->fw.stat);
	mutex_lock(&ddata->fw.stat_lock);

	retry = ddata->fw.retry;
	__pm_stay_awake(&ddata->fw.ws);
	ret = ddata->vib_ops->fw_load(ddata->input, ddata->fw.id);
	__pm_relax(&ddata->fw.ws);
	if (!ret) {
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		event = NOTIFY_EXTRA_VIB_FW_LOAD_SUCCESS;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
		ddata->fw.ret[retry] = FW_LOAD_SUCCESS;
		ddata->fw.stat = FW_LOAD_SUCCESS;
		if (ddata->f0_stored && ddata->fw.id == 0)
			ddata->vib_ops->set_f0_stored(ddata->input, ddata->f0_stored);
	} else {
		if (retry < FWLOAD_TRY-1) {
			ddata->fw.ret[retry] = ret;
			queue_delayed_work(ddata->fw.fw_workqueue,
				&ddata->fw.retry_wk, msecs_to_jiffies(3000));
			ddata->fw.retry++;
		} else {
			dev_err(ddata->dev, "%s firmware load retry fail\n", __func__);
#if IS_ENABLED(CONFIG_SEC_ABC)
			sec_abc_send_event("MODULE=vib@WARN=fw_load_fail");
#endif
		}
	}
err:
	mutex_unlock(&ddata->fw.stat_lock);
	dev_info(ddata->dev, "%s done\n", __func__);
	return ret;
}

static void sec_vib_inputff_load_firmware_work(struct work_struct *work)
{
	struct sec_vib_inputff_fwdata *fdata
		= container_of(work, struct sec_vib_inputff_fwdata, wk);
	struct sec_vib_inputff_drvdata *ddata
		= container_of(fdata, struct sec_vib_inputff_drvdata, fw);
	int ret = 0;

	dev_info(ddata->dev, "%s\n", __func__);
	ret = sec_vib_inputff_load_firmware(ddata, fdata->id);
	if (ret < 0)
		dev_err(ddata->dev, "%s fail(%d)\n", __func__, ret);
}

static void sec_vib_inputff_load_firmware_retry(struct work_struct *work)
{
	struct sec_vib_inputff_fwdata *fdata
		= container_of(to_delayed_work(work), struct sec_vib_inputff_fwdata, retry_wk);
	struct sec_vib_inputff_drvdata *ddata
		= container_of(fdata, struct sec_vib_inputff_drvdata, fw);
	int ret = 0;

	dev_info(ddata->dev, "%s\n", __func__);
	ret = sec_vib_inputff_load_firmware(ddata, fdata->id);
	if (ret < 0)
		dev_err(ddata->dev, "%s fail(%d)\n", __func__, ret);
}

static void firmware_load_store_work(struct work_struct *work)
{
	struct sec_vib_inputff_fwdata *fdata
		= container_of(to_delayed_work(work), struct sec_vib_inputff_fwdata, store_wk);
	struct sec_vib_inputff_drvdata *ddata
		= container_of(fdata, struct sec_vib_inputff_drvdata, fw);
	unsigned int i;

	dev_info(ddata->dev, "%s stat(%d) retry(%d)\n", __func__,
			ddata->fw.stat, ddata->fw.retry);

	cancel_work_sync(&ddata->fw.wk);
	cancel_delayed_work_sync(&ddata->fw.retry_wk);
	dev_info(ddata->dev, "%s clear retry\n", __func__);
	ddata->fw.retry = 0;
	for (i = 0; i < FWLOAD_TRY; i++)
		ddata->fw.ret[i] = FW_LOAD_STORE;
	ddata->fw.stat = FW_LOAD_STORE;

	queue_work(ddata->fw.fw_workqueue, &ddata->fw.wk);
}

static ssize_t firmware_load_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);
	bool fw_load = false;

	if (ddata->fw.stat == FW_LOAD_SUCCESS)
		fw_load = true;

	return sprintf(buf, "%d\n", fw_load);
}

static ssize_t firmware_load_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);
	u32 fw_id = 0;
	int ret = 0;

	ret = kstrtou32(buf, 0, &fw_id);
	if (ret < 0) {
		dev_err(ddata->dev, "%s kstrtou32 error : %d\n", __func__, ret);
		goto err;
	}
	dev_info(ddata->dev, "%s id(%d) stat(%d) retry(%d)\n", __func__,
			fw_id, ddata->fw.stat, ddata->fw.retry);
	ddata->fw.id = fw_id;
	schedule_delayed_work(&ddata->fw.store_wk, msecs_to_jiffies(0));
	dev_info(ddata->dev, "%s done\n", __func__);
err:
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(firmware_load);

int sec_vib_inputff_get_current_temp(struct sec_vib_inputff_drvdata *ddata)
{
	dev_info(ddata->dev, "%s temperature : %d\n", __func__, ddata->temperature);

	return ddata->temperature;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_get_current_temp);

static ssize_t current_temp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", ddata->temperature);
}

static ssize_t current_temp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);
	int temp, ret = 0;

	ret = kstrtos32(buf, 0, &temp);
	if (ret < 0) {
		dev_err(ddata->dev, "%s kstrtos32 error : %d\n", __func__, ret);
		ddata->temperature = INT_MIN;
		goto err;
	}

	ddata->temperature = temp;
	dev_info(ddata->dev, "%s temperature : %d\n", __func__, ddata->temperature);
err:
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(current_temp);

int sec_vib_inputff_get_ach_percent(struct sec_vib_inputff_drvdata *ddata)
{
	dev_info(ddata->dev, "%s ach_percent : %d\n", __func__, ddata->ach_percent);

	return ddata->ach_percent;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_get_ach_percent);

static ssize_t ach_percent_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", ddata->ach_percent);
}

static ssize_t ach_percent_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);
	int percent, ret = 0;

	ret = kstrtos32(buf, 0, &percent);
	if (ret < 0) {
		dev_err(ddata->dev, "%s kstrtos32 error : %d\n", __func__, ret);
		ddata->ach_percent = 100;
		goto err;
	}

	if (percent >= 0 && percent <= 100)
		ddata->ach_percent = percent;
	else
		ddata->ach_percent = 100;
	dev_info(ddata->dev, "%s ach_percent : %d\n", __func__, ddata->ach_percent);
err:
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(ach_percent);

static ssize_t trigger_calibration_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);
	int ret;
	u32 trigger_calibration;

	ret = kstrtos32(buf, 16, &trigger_calibration);
	if (ret < 0) {
		dev_err(ddata->dev, "%s kstrtos32 error : %d\n", __func__, ret);
		return ret;
	}
	if (ddata->vib_ops->set_trigger_cal && ddata->is_f0_tracking) {
		ret = ddata->vib_ops->set_trigger_cal(ddata->input, trigger_calibration);
		if (ret) {
			dev_err(ddata->dev, "%s set_trigger_cal error : %d\n", __func__, ret);
			return -EINVAL;
		}
		dev_info(ddata->dev, "%s trigger_calibration : %u, ret : %d\n", __func__, trigger_calibration, ret);
	} else {
		dev_err(ddata->dev, "%s set_trigger_cal doesn't support\n", __func__);
		return -EOPNOTSUPP;
	}
	return count;
}
static DEVICE_ATTR_WO(trigger_calibration);

static ssize_t f0_measured_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);

	if (!ddata->vib_ops->get_f0_measured || !ddata->is_f0_tracking) {
		dev_err(ddata->dev, "%s get_f0_measured doesn't support\n", __func__);
		return -EOPNOTSUPP;
	}

	return snprintf(buf, PAGE_SIZE, "%08X\n", ddata->vib_ops->get_f0_measured(ddata->input));
}
static DEVICE_ATTR_RO(f0_measured);

static ssize_t f0_offset_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);

	if (!ddata->vib_ops->get_f0_offset || !ddata->is_f0_tracking) {
		dev_err(ddata->dev, "%s get_f0_offset doesn't support\n", __func__);
		return -EOPNOTSUPP;
	}

	return snprintf(buf, PAGE_SIZE, "%08X\n", ddata->vib_ops->get_f0_offset(ddata->input));
}
static DEVICE_ATTR_RO(f0_offset);

static ssize_t f0_stored_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);

	if (!ddata->vib_ops->set_f0_stored || !ddata->is_f0_tracking) {
		dev_err(ddata->dev, "%s f0_stored read doesn't support\n", __func__);
		return -EOPNOTSUPP;
	}

	return sprintf(buf, "%08X\n", ddata->f0_stored);
}

static ssize_t f0_stored_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);
	int ret;
	u32 f0_stored;

	ret = kstrtos32(buf, 16, &f0_stored);
	if (ret < 0) {
		dev_err(ddata->dev, "%s kstrtos32 error : %d\n", __func__, ret);
		return ret;
	}

	if (ddata->vib_ops->set_f0_stored && ddata->is_f0_tracking) {
		if (ddata->fw.stat == FW_LOAD_SUCCESS && ddata->fw.id == 0) {
			ret = ddata->vib_ops->set_f0_stored(ddata->input, f0_stored);
			if (ret) {
				dev_err(ddata->dev, "%s set_f0_stored error : %d\n", __func__, ret);
				return -EINVAL;
			}
		}
		ddata->f0_stored = f0_stored;
		dev_info(ddata->dev, "%s f0_stored : %u, ret : %d\n", __func__, f0_stored, ret);
	} else {
		dev_err(ddata->dev, "%s set_f0_stored doesn't support\n", __func__);
		return -EOPNOTSUPP;
	}
	return count;
}
static DEVICE_ATTR_RW(f0_stored);

static struct attribute *vib_inputff_sys_attr[] = {
	&dev_attr_i2s_test.attr,
	&dev_attr_firmware_load.attr,
	&dev_attr_current_temp.attr,
	&dev_attr_ach_percent.attr,
	&dev_attr_trigger_calibration.attr,
	&dev_attr_f0_measured.attr,
	&dev_attr_f0_offset.attr,
	&dev_attr_f0_stored.attr,
	NULL,
};

static struct attribute_group vib_inputff_sys_attr_group = {
	.attrs = vib_inputff_sys_attr,
};

static void sec_vib_inputff_fw_init(struct sec_vib_inputff_drvdata *ddata)
{
	unsigned int i;

	mutex_init(&ddata->fw.stat_lock);
	for (i = 0; i < FWLOAD_TRY; i++)
		ddata->fw.ret[i] = FW_LOAD_INIT;
	ddata->fw.stat = FW_LOAD_INIT;
	ddata->fw.id = 0;
	ddata->fw.ws.name = "sec_vib_inputff";
	wakeup_source_add(&ddata->fw.ws);
	ddata->fw.fw_workqueue = alloc_ordered_workqueue("vib_workqueue",
					WQ_FREEZABLE | WQ_MEM_RECLAIM);
	INIT_WORK(&ddata->fw.wk, sec_vib_inputff_load_firmware_work);
	INIT_DELAYED_WORK(&ddata->fw.retry_wk, sec_vib_inputff_load_firmware_retry);
	INIT_DELAYED_WORK(&ddata->fw.store_wk, firmware_load_store_work);
	queue_work(ddata->fw.fw_workqueue, &ddata->fw.wk);
	dev_info(ddata->dev, "%s done\n", __func__);
}

int sec_vib_inputff_sysfs_init(struct sec_vib_inputff_drvdata *ddata)
{
	int ret = 0;

	dev_info(ddata->dev, "%s\n", __func__);

	if (ddata->support_fw)
		sec_vib_inputff_fw_init(ddata);

	ddata->sec_vib_inputff_class = class_create(THIS_MODULE, "sec_vib_inputff");
	if (IS_ERR(ddata->sec_vib_inputff_class)) {
		ret = PTR_ERR(ddata->sec_vib_inputff_class);
		goto err1;
	}
	ddata->virtual_dev = device_create(ddata->sec_vib_inputff_class,
			NULL, MKDEV(0, 0), ddata, "control");
	if (IS_ERR(ddata->virtual_dev)) {
		ret = PTR_ERR(ddata->virtual_dev);
		goto err2;
	}

	ret = sysfs_create_group(&ddata->virtual_dev->kobj,
			&vib_inputff_sys_attr_group);
	if (ret) {
		ret = -ENODEV;
		dev_err(ddata->dev, "Failed to create sysfs %d\n", ret);
		goto err3;
	}
	dev_info(ddata->dev, "%s done\n", __func__);

	return 0;
err3:
	device_destroy(ddata->sec_vib_inputff_class, MKDEV(0, 0));
err2:
	class_destroy(ddata->sec_vib_inputff_class);
err1:
	return ret;
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_sysfs_init);

void sec_vib_inputff_fw_exit(struct sec_vib_inputff_drvdata *ddata)
{
	cancel_work_sync(&ddata->fw.wk);
	cancel_delayed_work_sync(&ddata->fw.retry_wk);
	destroy_workqueue(ddata->fw.fw_workqueue);
	cancel_delayed_work_sync(&ddata->fw.store_wk);
	wakeup_source_remove(&ddata->fw.ws);
}

void sec_vib_inputff_sysfs_exit(struct sec_vib_inputff_drvdata *ddata)
{
	if (ddata->support_fw)
		sec_vib_inputff_fw_exit(ddata);
	sysfs_remove_group(&ddata->virtual_dev->kobj, &vib_inputff_sys_attr_group);
	device_destroy(ddata->sec_vib_inputff_class, MKDEV(0, 0));
	class_destroy(ddata->sec_vib_inputff_class);
}
EXPORT_SYMBOL_GPL(sec_vib_inputff_sysfs_exit);
