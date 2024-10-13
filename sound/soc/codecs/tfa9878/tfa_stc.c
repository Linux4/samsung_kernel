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

#define FILESIZE_STC_TUNING	(100)

/* ---------------------------------------------------------------------- */

static ssize_t spkt_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t spkt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(spkt);

static ssize_t sknt_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sknt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(sknt);

#if defined(TFA_STEREO_NODE)
static ssize_t spkt_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t spkt_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(spkt_r);

static ssize_t sknt_r_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sknt_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static DEVICE_ATTR_RW(sknt_r);
#endif /* TFA_STEREO_NODE */

static struct attribute *tfa_stc_attr[] = {
	&dev_attr_spkt.attr,
	&dev_attr_sknt.attr,
#if defined(TFA_STEREO_NODE)
	&dev_attr_spkt_r.attr,
	&dev_attr_sknt_r.attr,
#endif /* TFA_STEREO_NODE */
	NULL,
};

static struct attribute_group tfa_stc_attr_grp = {
	.attrs = tfa_stc_attr,
};

/* ---------------------------------------------------------------------- */

static struct device *tfa_stc_dev;

static int sknt_data[MAX_HANDLES];

/* ---------------------------------------------------------------------- */

static ssize_t update_sknt_control(int idx, char *buf)
{
	struct tfa_device *tfa = NULL;
	int size;
	char sknt_result[FILESIZE_STC] = {0};

	tfa = tfa98xx_get_tfa_device_from_index(0);
	if (tfa == NULL)
		return -EINVAL; /* unused device */

	if (idx < 0 || idx >= tfa->dev_count)
		return -EINVAL;

	snprintf(sknt_result, FILESIZE_STC,
		"%d", sknt_data[idx]);

	if (sknt_result[0] == 0)
		size = snprintf(buf, 7 + 1, "no_data"); /* no data */
	else
		size = snprintf(buf, strlen(sknt_result) + 1,
			"%s", sknt_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t spkt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = tfa_get_dev_idx_from_inchannel(0);
	int value = 0, size;
	char spkt_result[FILESIZE_STC] = {0};

	value = tfa98xx_update_spkt_data(idx);
	pr_info("%s: tfa_stc - dev %d - speaker temperature (%d)\n",
		__func__, idx, value);

	snprintf(spkt_result, FILESIZE_STC,
		"%d", value);

	if (spkt_result[0] == 0)
		size = snprintf(buf, 1 + 1, "0"); /* no data */
	else
		size = snprintf(buf, strlen(spkt_result) + 1,
			"%s", spkt_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t spkt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write speaker temperature\n",
		__func__, tfa_get_dev_idx_from_inchannel(0));

	return size;
}

static ssize_t sknt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = tfa_get_dev_idx_from_inchannel(0);
	int ret;

	ret = update_sknt_control(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_stc - dev %d - surface temperature (%d)\n",
			__func__, idx, sknt_data[idx]);
	else
		pr_err("%s: tfa_stc dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t sknt_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int idx = tfa_get_dev_idx_from_inchannel(0);
	int ret;
	int value = 0;

	ret = kstrtou32(buf, 10, &value);
	if (!value) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}

	ret = tfa98xx_write_sknt_control(idx, value);
	if (!ret) {
		pr_info("%s: tfa_stc - dev %d - surface temperature (%d)\n",
			__func__, idx, value);
		sknt_data[idx] = value;
	} else {
		pr_err("%s: tfa_stc dev %d - error %d\n",
			__func__, idx, ret);
	}

	return size;
}

#if defined(TFA_STEREO_NODE)
static ssize_t spkt_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = tfa_get_dev_idx_from_inchannel(1);
	int value = 0, size;
	char spkt_result[FILESIZE_STC] = {0};

	value = tfa98xx_update_spkt_data(idx);
	pr_info("%s: tfa_stc - dev %d - speaker temperature (%d)\n",
		__func__, idx, value);

	snprintf(spkt_result, FILESIZE_STC,
		"%d", value);

	if (spkt_result[0] == 0)
		size = snprintf(buf, 1 + 1, "0"); /* no data */
	else
		size = snprintf(buf, strlen(spkt_result) + 1,
			"%s", spkt_result);

	if (size <= 0) {
		pr_err("%s: tfa_stc failed to show in sysfs file\n", __func__);
		return -EINVAL;
	}

	return size;
}

static ssize_t spkt_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s: dev %d - not allowed to write speaker temperature\n",
		__func__, tfa_get_dev_idx_from_inchannel(1));

	return size;
}

static ssize_t sknt_r_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int idx = tfa_get_dev_idx_from_inchannel(1);
	int ret;

	ret = update_sknt_control(idx, buf);
	if (ret > 0)
		pr_info("%s: tfa_stc - dev %d - surface temperature (%d)\n",
			__func__, idx, sknt_data[idx]);
	else
		pr_err("%s: tfa_stc dev %d - error %d\n",
			__func__, idx, ret);

	return ret;
}

static ssize_t sknt_r_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int idx = tfa_get_dev_idx_from_inchannel(1);
	int ret;
	int value = 0;

	ret = kstrtou32(buf, 10, &value);
	if (!value) {
		pr_info("%s: do nothing\n", __func__);
		return -EINVAL;
	}

	ret = tfa98xx_write_sknt_control(idx, value);
	if (!ret) {
		pr_info("%s: tfa_stc - dev %d - surface temperature (%d)\n",
			__func__, idx, value);
		sknt_data[idx] = value;
	} else {
		pr_err("%s: tfa_stc dev %d - error %d\n",
			__func__, idx, ret);
	}

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
