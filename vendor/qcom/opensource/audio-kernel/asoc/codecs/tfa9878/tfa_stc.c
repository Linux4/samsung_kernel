/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020-2021 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "inc/tfa_sysfs.h"

#define TFA_STC_DEV_NAME	"tfa_stc"
#define FILESIZE_STC	(10)

/* ---------------------------------------------------------------------- */

static ssize_t power_state_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t power_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(power_state);

#if defined(TFA_STEREO_NODE)
static ssize_t power_state_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t power_state_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(power_state_r);
#endif /* TFA_STEREO_NODE */

static struct attribute *tfa_stc_attr[] = {
	&dev_attr_power_state.attr,
#if defined(TFA_STEREO_NODE)
	&dev_attr_power_state_r.attr,
#endif /* TFA_STEREO_NODE */
	NULL,
};

static struct attribute_group tfa_stc_attr_grp = {
	.attrs = tfa_stc_attr,
};

/* ---------------------------------------------------------------------- */

static struct device *tfa_stc_dev;

/* ---------------------------------------------------------------------- */

static ssize_t power_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = tfa_get_dev_idx_from_inchannel(0);
	int value, size;
	char pstate_result[FILESIZE_STC] = {0};

	value = tfa_get_power_state(idx);
	pr_info("%s: tfa_stc - dev %d - power state (%d)\n",
		__func__, idx, value);

	snprintf(pstate_result, FILESIZE_STC,
		"%d", value);

	if (pstate_result[0] == 0)
		size = snprintf(buf, 1 + 1, "0"); /* no data */
	else
		size = snprintf(buf, strlen(pstate_result) + 1,
			"%s", pstate_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t power_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write power state\n",
		__func__, tfa_get_dev_idx_from_inchannel(0));

	return size;
}

#if defined(TFA_STEREO_NODE)
static ssize_t power_state_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = tfa_get_dev_idx_from_inchannel(1);
	int value, size;
	char pstate_result[FILESIZE_STC] = {0};

	value = tfa_get_power_state(idx);
	pr_info("%s: tfa_stc - dev %d - power state (%d)\n",
		__func__, idx, value);

	snprintf(pstate_result, FILESIZE_STC,
		"%d", value);

	if (pstate_result[0] == 0)
		size = snprintf(buf, 1 + 1, "0"); /* no data */
	else
		size = snprintf(buf, strlen(pstate_result) + 1,
			"%s", pstate_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t power_state_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write power state\n",
		__func__, tfa_get_dev_idx_from_inchannel(1));

	return size;
}
#endif /* TFA_STEREO_NODE */

int tfa98xx_stc_init(struct class *tfa_class)
{
	int ret = 0;

	if (tfa_class) {
		tfa_stc_dev = device_create(tfa_class,
			NULL, DEV_ID_TFA_STC, NULL, TFA_STC_DEV_NAME);
		if (!IS_ERR(tfa_stc_dev)) {
			ret = sysfs_create_group(&tfa_stc_dev->kobj,
				&tfa_stc_attr_grp);
			if (ret)
				pr_err("%s: failed to create sysfs group. ret (%d)\n",
					__func__, ret);
		}
	}

	pr_info("%s: initialized (%d)\n", __func__,
		(tfa_class != NULL) ? 1 : 0);

	return ret;
}

void tfa98xx_stc_exit(struct class *tfa_class)
{
	device_destroy(tfa_class, DEV_ID_TFA_STC);
	pr_info("exited\n");
}
