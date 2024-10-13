/*
 * Calibration support for Cirrus Logic Smart Amplifiers
 *
 * Copyright 2017 Cirrus Logic
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/fs.h>

#include <sound/cirrus/core.h>
#include <sound/cirrus/calibration.h>

#include "wmfw.h"
#include "wm_adsp.h"

#define CIRRUS_CAL_VERSION "5.01.18"

#define CIRRUS_CAL_DIR_NAME			"cirrus_cal"
#define CIRRUS_CAL_RDC_SAVE_LOCATION		"/efs/cirrus/rdc_cal"
#define CIRRUS_CAL_TEMP_SAVE_LOCATION		"/efs/cirrus/temp_cal"
#define CIRRUS_CAL_VSC_SAVE_LOCATION		"/efs/cirrus/vsc_cal"
#define CIRRUS_CAL_ISC_SAVE_LOCATION		"/efs/cirrus/isc_cal"

#define CIRRUS_CAL_AMBIENT_DEFAULT	23
#define CIRRUS_CAL_COMPLETE_DELAY_MS	1250

int cirrus_cal_apply(const char *mfd_suffix)
{
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(mfd_suffix);

	return amp_group->amps[0].cal_ops->cal_apply(amp);
}
EXPORT_SYMBOL_GPL(cirrus_cal_apply);

int cirrus_cal_read_temp(const char *mfd_suffix)
{
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(mfd_suffix);

	return amp_group->amps[0].cal_ops->read_temp(amp);
}
EXPORT_SYMBOL_GPL(cirrus_cal_read_temp);

int cirrus_cal_set_surface_temp(const char *suffix, int temperature)
{
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	return amp_group->amps[0].cal_ops->set_temp(amp, temperature);
}
EXPORT_SYMBOL_GPL(cirrus_cal_set_surface_temp);

void cirrus_cal_complete_work(struct work_struct *work)
{
	amp_group->amps[0].cal_ops->cal_complete();
}


/***** SYSFS Interfaces *****/

static ssize_t cirrus_cal_version_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, CIRRUS_CAL_VERSION "\n");
}

static ssize_t cirrus_cal_version_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_cal_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n", amp_group->cal_running ?
			       "Enabled" : "Disabled");
}

static ssize_t cirrus_cal_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int ret = 0, prepare;

	if (amp_group->cal_running) {
		dev_err(amp_group->cal_dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	mutex_lock(&amp_group->cal_lock);

	ret = kstrtos32(buf, 10, &prepare);
	if (ret != 0 || prepare != 1)
		goto err;

	amp_group->cal_running = true;
	amp_group->cal_retry = 0;

	amp_group->amps[0].cal_ops->cal_start();

	dev_dbg(amp_group->cal_dev, "Calibration prepare complete\n");

	queue_delayed_work(system_unbound_wq, &amp_group->cal_complete_work,
			   msecs_to_jiffies(CIRRUS_CAL_COMPLETE_DELAY_MS));

err:
	mutex_unlock(&amp_group->cal_lock);
	if (ret < 0)
		amp_group->cal_running = false;

	return size;
}

static ssize_t cirrus_cal_v_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n", amp_group->cal_running ?
			       "Enabled" : "Disabled");
}

static ssize_t cirrus_cal_v_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int ret = 0, prepare, num_amps;
	const char *suffix;
	struct cirrus_amp *amps;
	bool separate = false;

	if (amp_group->cal_running) {
		dev_err(amp_group->cal_dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	mutex_lock(&amp_group->cal_lock);

	ret = kstrtos32(buf, 10, &prepare);
	if (ret != 0 || prepare != 1)
		goto err;

	amp_group->cal_running = true;

	if (strlen(attr->attr.name) > strlen("v_status")) {
		suffix = &(attr->attr.name[strlen("v_status")]);
		amps = cirrus_get_amp_from_suffix(suffix);
		if (amps) {
			dev_info(dev, "V-validation for amp: %s (%s)\n",
					amps->dsp_part_name, suffix);
			num_amps = 1;
			separate = true;
		} else {
			mutex_unlock(&amp_group->cal_lock);
			return size;
		}
	} else {
		num_amps = amp_group->num_amps;
		amps = amp_group->amps;
		separate = false;
	}

	amps[0].cal_ops->v_val(amps, num_amps, separate);

err:
	amp_group->cal_running = false;
	mutex_unlock(&amp_group->cal_lock);

	return size;
}

static ssize_t cirrus_cal_irq_enable_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "\n");
}

static ssize_t cirrus_cal_irq_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int irq_enable, i;
	int ret = kstrtos32(buf, 10, &irq_enable);

	if (amp_group->cal_running) {
		dev_err(amp_group->cal_dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	if (ret == 0) {
		mutex_lock(&amp_group->cal_lock);

		for (i = 0; i < amp_group->num_amps; i++) {
			if (amp_group->amps[i].irq != 0) {
				if (irq_enable)
					enable_irq(amp_group->amps[i].irq);
				else
					disable_irq(amp_group->amps[i].irq);
			}
		}

		mutex_unlock(&amp_group->cal_lock);
	}

	return size;
}

#ifdef CONFIG_SND_SOC_CIRRUS_REINIT_SYSFS
static ssize_t cirrus_cal_reinit_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "\n");
}

static ssize_t cirrus_cal_reinit_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int reinit, i;
	int ret = kstrtos32(buf, 10, &reinit);

	if (amp_group->cal_running) {
		dev_err(amp_group->cal_dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	if (ret == 0 && reinit == 1) {
		mutex_lock(&amp_group->cal_lock);

		for (i = 0; i < amp_group->num_amps; i++) {
			if (amp_group->amps[i].amp_reinit != NULL)
				amp_group->amps[i].amp_reinit(
					amp_group->amps[i].component);
		}

		mutex_unlock(&amp_group->cal_lock);
	}

	return size;
}
#endif /* CONFIG_SND_SOC_CIRRUS_REINIT_SYSFS*/

static ssize_t cirrus_cal_vval_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("v_validation")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	dev_info(dev, "%s\n", __func__);

	return sprintf(buf, "%d", amp->cal.v_validation);
}

static ssize_t cirrus_cal_vval_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static ssize_t cirrus_cal_rdc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int rdc;
	const char *suffix = &(attr->attr.name[strlen("rdc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		rdc = amp->cal.efs_cache_rdc;
		return sprintf(buf, "%d", rdc);
	} else
		return 0;
}

static ssize_t cirrus_cal_rdc_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int rdc, ret;
	const char *suffix = &(attr->attr.name[strlen("rdc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	bool vimon_valid;

	ret = kstrtos32(buf, 10, &rdc);
	if (ret == 0 && amp) {
		if (rdc < 0) {
			amp->cal.efs_cache_vsc = 0;
			amp->cal.efs_cache_isc = 0;
			amp->cal.efs_cache_rdc = 0;
			amp->cal.efs_cache_valid = 0;
			return size;
		}

		amp->cal.efs_cache_rdc = rdc;

		dev_info(dev, "EFS Cache RDC set: 0x%x\n", rdc);
		vimon_valid = (!amp->perform_vimon_cal) || (amp->cal.efs_cache_vsc &&
				amp->cal.efs_cache_isc);
		if (amp->cal.efs_cache_rdc && amp_group->efs_cache_temp &&
				vimon_valid)
			amp->cal.efs_cache_valid = 1;
	}
	return size;
}

static ssize_t cirrus_cal_vsc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int vsc;
	const char *suffix = &(attr->attr.name[strlen("vsc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		vsc = amp->cal.efs_cache_vsc;
		return sprintf(buf, "%d", vsc);
	} else
		return 0;
}

static ssize_t cirrus_cal_vsc_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int vsc, ret;
	const char *suffix = &(attr->attr.name[strlen("vsc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	bool vimon_valid;

	ret = kstrtos32(buf, 10, &vsc);
	if (ret == 0 && amp) {
		if (vsc < 0) {
			amp->cal.efs_cache_vsc = 0;
			amp->cal.efs_cache_isc = 0;
			amp->cal.efs_cache_rdc = 0;
			amp->cal.efs_cache_valid = 0;
			return size;
		}

		amp->cal.efs_cache_vsc = vsc;

		dev_info(dev, "EFS Cache VSC set: 0x%x\n", vsc);
		vimon_valid = (!amp->perform_vimon_cal) || (amp->cal.efs_cache_vsc &&
				amp->cal.efs_cache_isc);
		if (amp->cal.efs_cache_rdc && amp_group->efs_cache_temp &&
				vimon_valid)
			amp->cal.efs_cache_valid = 1;
	}
	return size;
}

static ssize_t cirrus_cal_isc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int isc;
	const char *suffix = &(attr->attr.name[strlen("isc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		isc = amp->cal.efs_cache_isc;
		return sprintf(buf, "%d", isc);
	} else
		return 0;
}

static ssize_t cirrus_cal_isc_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int isc, ret;
	const char *suffix = &(attr->attr.name[strlen("isc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	bool vimon_valid;

	ret = kstrtos32(buf, 10, &isc);
	if (ret == 0 && amp) {
		if (isc < 0) {
			amp->cal.efs_cache_vsc = 0;
			amp->cal.efs_cache_isc = 0;
			amp->cal.efs_cache_rdc = 0;
			amp->cal.efs_cache_valid = 0;
			return size;
		}

		amp->cal.efs_cache_isc = isc;

		dev_info(dev, "EFS Cache ISC set: 0x%x\n", isc);
		vimon_valid = (!amp->perform_vimon_cal) || (amp->cal.efs_cache_vsc &&
				amp->cal.efs_cache_isc);
		if (amp->cal.efs_cache_rdc && amp_group->efs_cache_temp &&
				vimon_valid)
			amp->cal.efs_cache_valid = 1;
	}
	return size;
}

static ssize_t cirrus_cal_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int temp;
	const char *suffix = &(attr->attr.name[strlen("temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		temp = amp_group->efs_cache_temp;
		return sprintf(buf, "%d", temp);
	} else
		return 0;
}

static ssize_t cirrus_cal_temp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int temp, ret;
	const char *suffix = &(attr->attr.name[strlen("temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	bool vimon_valid;

	ret = kstrtos32(buf, 10, &temp);
	if (ret == 0 && amp) {
		amp_group->efs_cache_temp = temp;

		dev_info(dev, "EFS Cache temp set: %d\n", temp);
		vimon_valid = (!amp->perform_vimon_cal) || (amp->cal.efs_cache_vsc &&
				amp->cal.efs_cache_isc);
		if (amp->cal.efs_cache_rdc && amp_group->efs_cache_temp &&
				vimon_valid)
			amp->cal.efs_cache_valid = 1;
	}
	return size;
}

static ssize_t cirrus_cal_checksum_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int checksum;
	const char *suffix = &(attr->attr.name[strlen("checksum")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		cirrus_amp_read_ctl(amp, amp->cal_ops->controls.cal_checksum.name,
				WMFW_ADSP2_XM, amp->cal_ops->controls.cal_checksum.alg_id, &checksum);
		return sprintf(buf, "%d", checksum);
	} else
		return 0;
}

static ssize_t cirrus_cal_checksum_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int checksum, ret;
	const char *suffix = &(attr->attr.name[strlen("checksum")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	ret = kstrtos32(buf, 10, &checksum);
	if (ret == 0 && amp)
		cirrus_amp_write_ctl(amp, amp->cal_ops->controls.cal_checksum.name,
				WMFW_ADSP2_XM, amp->cal_ops->controls.cal_checksum.alg_id, checksum);
	return size;
}

static ssize_t cirrus_cal_set_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int set_status;
	const char *suffix = &(attr->attr.name[strlen("set_status")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		cirrus_amp_read_ctl(amp, amp->cal_ops->controls.cal_set_status.name,
				WMFW_ADSP2_XM, amp->cal_ops->controls.cal_checksum.alg_id, &set_status);
		return sprintf(buf, "%d", set_status);
	} else
		return 0;
}

static ssize_t cirrus_cal_set_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static DEVICE_ATTR(version, 0444, cirrus_cal_version_show,
				cirrus_cal_version_store);
static DEVICE_ATTR(status, 0664, cirrus_cal_status_show,
				cirrus_cal_status_store);
static DEVICE_ATTR(v_status, 0664, cirrus_cal_v_status_show,
				cirrus_cal_v_status_store);
static DEVICE_ATTR(irq_enable, 0664, cirrus_cal_irq_enable_show,
				cirrus_cal_irq_enable_store);
#ifdef CONFIG_SND_SOC_CIRRUS_REINIT_SYSFS
static DEVICE_ATTR(reinit, 0664, cirrus_cal_reinit_show,
				cirrus_cal_reinit_store);
#endif /* CONFIG_SND_SOC_CIRRUS_REINIT_SYSFS */

static struct device_attribute v_val_attribute = {
	.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
	.show = cirrus_cal_v_status_show,
	.store = cirrus_cal_v_status_store,
};

static struct device_attribute generic_amp_attrs[CIRRUS_CAL_NUM_ATTRS_AMP] = {
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_cal_vval_show,
		.store = cirrus_cal_vval_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_rdc_show,
		.store = cirrus_cal_rdc_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_vsc_show,
		.store = cirrus_cal_vsc_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_isc_show,
		.store = cirrus_cal_isc_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_temp_show,
		.store = cirrus_cal_temp_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_checksum_show,
		.store = cirrus_cal_checksum_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_cal_set_status_show,
		.store = cirrus_cal_set_status_store,
	},
};

static const char *generic_amp_attr_names[CIRRUS_CAL_NUM_ATTRS_AMP] = {
	"v_validation",
	"rdc",
	"vsc",
	"isc",
	"temp",
	"checksum",
	"set_status"
};

static struct attribute *cirrus_cal_attr_base[] = {
	&dev_attr_version.attr,
	&dev_attr_status.attr,
	&dev_attr_v_status.attr,
	&dev_attr_irq_enable.attr,
#ifdef CONFIG_SND_SOC_CIRRUS_REINIT_SYSFS
	&dev_attr_reinit.attr,
#endif /* CONFIG_SND_SOC_CIRRUS_REINIT_SYSFS */
	NULL,
};

/* Kernel does not allow attributes to be dynamically allocated */
static struct attribute_group cirrus_cal_attr_grp;
static struct device_attribute
		amp_attrs_prealloc[CIRRUS_MAX_AMPS][CIRRUS_CAL_NUM_ATTRS_AMP];
static char attr_names_prealloc[CIRRUS_MAX_AMPS][CIRRUS_CAL_NUM_ATTRS_AMP][20];
static char v_val_attr_names_prealloc[CIRRUS_MAX_AMPS][20];
static struct device_attribute v_val_attrs_prealloc[CIRRUS_MAX_AMPS];

struct device_attribute *cirrus_cal_create_amp_attrs(const char *mfd_suffix,
							int index)
{
	struct device_attribute *amp_attrs_new;
	int i, suffix_len = strlen(mfd_suffix);

	if (index >= CIRRUS_MAX_AMPS)
		return NULL;

	amp_attrs_new = &(amp_attrs_prealloc[index][0]);

	memcpy(amp_attrs_new, &generic_amp_attrs,
		sizeof(struct device_attribute) *
		CIRRUS_CAL_NUM_ATTRS_AMP);

	for (i = 0; i < CIRRUS_CAL_NUM_ATTRS_AMP; i++) {
		amp_attrs_new[i].attr.name = attr_names_prealloc[index][i];
		snprintf((char *)amp_attrs_new[i].attr.name,
			strlen(generic_amp_attr_names[i]) + suffix_len + 1,
			"%s%s", generic_amp_attr_names[i], mfd_suffix);
	}

	return amp_attrs_new;
}

int cirrus_cal_init(void)
{
	struct device_attribute *new_attrs;
	int ret = 0, i, j, num_amps, v_val_num_attrs = 0;

	if (!amp_group) {
		pr_err("%s: Empty amp group\n", __func__);
		return -ENODATA;
	}

	amp_group->cal_dev = device_create(cirrus_amp_class, NULL, 1, NULL,
					   CIRRUS_CAL_DIR_NAME);
	if (IS_ERR(amp_group->cal_dev)) {
		ret = PTR_ERR(amp_group->cal_dev);
		pr_err("%s: Failed to create CAL device (%d)\n", __func__, ret);
		return ret;
	}

	dev_set_drvdata(amp_group->cal_dev, amp_group);

	num_amps = amp_group->num_amps;

	for (i = 0; i < num_amps; i++) {
		if (amp_group->amps[i].v_val_separate)
			v_val_num_attrs++;
	}

	cirrus_cal_attr_grp.attrs = kzalloc(sizeof(struct attribute *) *
					(CIRRUS_CAL_NUM_ATTRS_AMP * num_amps +
					v_val_num_attrs +
					CIRRUS_CAL_NUM_ATTRS_BASE + 1),
								GFP_KERNEL);
	for (i = 0; i < num_amps; i++) {
		new_attrs = cirrus_cal_create_amp_attrs(
				    amp_group->amps[i].mfd_suffix, i);
		for (j = 0; j < CIRRUS_CAL_NUM_ATTRS_AMP; j++) {
			dev_dbg(amp_group->cal_dev, "New attribute: %s\n",
				new_attrs[j].attr.name);
			cirrus_cal_attr_grp.attrs[i * CIRRUS_CAL_NUM_ATTRS_AMP
						  + j] = &new_attrs[j].attr;
		}
	}

	for (i = j = 0; i < num_amps; i++) {
		if (amp_group->amps[i].v_val_separate) {
			memcpy(&v_val_attrs_prealloc[j],
				&v_val_attribute, sizeof(struct device_attribute));

			v_val_attrs_prealloc[j].attr.name =
				v_val_attr_names_prealloc[j];
			snprintf((char *)v_val_attrs_prealloc[j].attr.name,
				strlen("v_status") +
				strlen(amp_group->amps[i].mfd_suffix) + 1,
				"v_status%s", amp_group->amps[i].mfd_suffix);
			dev_info(amp_group->cal_dev, "New attribute: %s\n",
				v_val_attrs_prealloc[j].attr.name);
			cirrus_cal_attr_grp.attrs[num_amps * CIRRUS_CAL_NUM_ATTRS_AMP
						  + j] = &v_val_attrs_prealloc[j].attr;
			j++;
		}
	}

	memcpy(&cirrus_cal_attr_grp.attrs[num_amps * CIRRUS_CAL_NUM_ATTRS_AMP +
					v_val_num_attrs],
		cirrus_cal_attr_base, sizeof(struct attribute *) *
					CIRRUS_CAL_NUM_ATTRS_BASE);
	cirrus_cal_attr_grp.attrs[num_amps * CIRRUS_CAL_NUM_ATTRS_AMP +
			CIRRUS_CAL_NUM_ATTRS_BASE + v_val_num_attrs] = NULL;

	ret = sysfs_create_group(&amp_group->cal_dev->kobj,
				 &cirrus_cal_attr_grp);
	if (ret) {
		dev_err(amp_group->cal_dev, "Failed to create sysfs group\n");
		device_del(amp_group->bd_dev);
		return ret;
	}

	mutex_init(&amp_group->cal_lock);
	INIT_DELAYED_WORK(&amp_group->cal_complete_work, cirrus_cal_complete_work);

	return ret;
}

void cirrus_cal_exit(void)
{
	flush_work(&amp_group->cal_complete_work.work);
	mutex_destroy(&amp_group->cal_lock);
	kfree(cirrus_cal_attr_grp.attrs);
	device_del(amp_group->bd_dev);
}

