/*
 *  linux/drivers/devfreq/governor_simpleondemand.c
 *
 *  Copyright (C) 2011 Samsung Electronics
 *	MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include "governor.h"

/* Default constants for DevFreq-Simple-Ondemand (DFSO) */
#define DFSO_UPTHRESHOLD	(90)
#define DFSO_DOWNDIFFERENCTIAL	(5)
#define DFSO_MAX_UPTHRESHOLD	(100)
static int devfreq_simple_ondemand_func(struct devfreq *df,
					unsigned long *freq)
{
	struct devfreq_dev_status stat;
	int err;
	unsigned long long a, b;
	unsigned int dfso_upthreshold = DFSO_UPTHRESHOLD;
	unsigned int dfso_downdifferential = DFSO_DOWNDIFFERENCTIAL;
	struct devfreq_simple_ondemand_data *data;
	unsigned long max = (df->max_freq) ? df->max_freq : UINT_MAX;

	if (!df->data) {
		pr_err("simple_ondemand_data is NULL!!\n");
		BUG_ON(1);
	}

	data = df->data;
	err = df->profile->get_dev_status(df->dev.parent, &stat);

	if (err) {
		/* Used to represent ignoring the profiling result */
		if (err == -EINVAL) {
			*freq = stat.current_frequency;
			return 0;
		} else
			return err;
	}

	if (data) {
		if (data->upthreshold)
			dfso_upthreshold = data->upthreshold;
		if (data->downdifferential)
			dfso_downdifferential = data->downdifferential;
	}
	if (dfso_upthreshold > 100 ||
	    dfso_upthreshold < dfso_downdifferential)
		return -EINVAL;

	/* Assume MAX if it is going to be divided by zero */
	if (stat.total_time == 0) {
		*freq = max;
		return 0;
	}

	/* Prevent overflow */
	if (stat.busy_time >= (1 << 24) || stat.total_time >= (1 << 24)) {
		stat.busy_time >>= 7;
		stat.total_time >>= 7;
	}

	/* Set MAX if it's busy enough */
	if (stat.busy_time * 100 >
	    stat.total_time * dfso_upthreshold) {
		*freq = max;
		return 0;
	}

	/* Set MAX if we do not know the initial frequency */
	if (stat.current_frequency == 0) {
		*freq = max;
		return 0;
	}

	/* Keep the current frequency */
	if (stat.busy_time * 100 >
	    stat.total_time * (dfso_upthreshold - dfso_downdifferential)) {
		*freq = stat.current_frequency;
		return 0;
	}

	/* Set the desired frequency based on the load */
	a = stat.busy_time;
	a *= stat.current_frequency;
	b = div_u64(a, stat.total_time);
	b *= 100;
	b = div_u64(b, (dfso_upthreshold - dfso_downdifferential / 2));
	*freq = (unsigned long) b;

	if (df->min_freq && *freq < df->min_freq)
		*freq = df->min_freq;
	if (df->max_freq && *freq > df->max_freq)
		*freq = df->max_freq;

	return 0;
}

static ssize_t upthreshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct devfreq_simple_ondemand_data *data;
	int rc = 0;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	rc = sprintf(buf, "%u\n", data->upthreshold);
	mutex_unlock(&devfreq->lock);

	return rc;
}

static ssize_t upthreshold_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct devfreq_simple_ondemand_data *data;
	int ret;
	unsigned int input;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > DFSO_MAX_UPTHRESHOLD ||
			input <= data->downdifferential) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}
	data->upthreshold = input;
	mutex_unlock(&devfreq->lock);

	return count;
}
static DEVICE_ATTR_RW(upthreshold);

static ssize_t downdifferential_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct devfreq_simple_ondemand_data *data;
	int rc = 0;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;
	rc = sprintf(buf, "%u\n", data->downdifferential);
	mutex_unlock(&devfreq->lock);
	return rc;
}

static ssize_t downdifferential_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct devfreq_simple_ondemand_data *data;
	int ret;
	unsigned int input;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > data->upthreshold) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}
	data->downdifferential = input;
	mutex_unlock(&devfreq->lock);

	return count;
}
static DEVICE_ATTR_RW(downdifferential);

static struct attribute *dev_entries[] = {
	&dev_attr_upthreshold.attr,
	&dev_attr_downdifferential.attr,
	NULL,
};

static struct attribute_group simpondemand_attributes = {
	.name	= "ondemand",
	.attrs	= dev_entries,
};

static int devfreq_simpleondemand_init(struct devfreq *devfreq)
{
	return sysfs_create_group(&devfreq->dev.kobj,
			&simpondemand_attributes);
}

static void devfreq_simpleondemand_exit(struct devfreq *devfreq)
{
	sysfs_remove_group(&devfreq->dev.kobj, &simpondemand_attributes);
}

static int devfreq_simple_ondemand_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_START:
		devfreq_monitor_start(devfreq);
		devfreq_simpleondemand_init(devfreq);
		break;

	case DEVFREQ_GOV_STOP:
		devfreq_monitor_stop(devfreq);
		devfreq_simpleondemand_exit(devfreq);
		break;

	case DEVFREQ_GOV_INTERVAL:
		devfreq_interval_update(devfreq, (unsigned int *)data);
		break;

	case DEVFREQ_GOV_SUSPEND:
		devfreq_monitor_suspend(devfreq);
		break;

	case DEVFREQ_GOV_RESUME:
		devfreq_monitor_resume(devfreq);
		break;

	default:
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_simple_ondemand = {
	.name = "simple_ondemand",
	.get_target_freq = devfreq_simple_ondemand_func,
	.event_handler = devfreq_simple_ondemand_handler,
};

static int __init devfreq_simple_ondemand_init(void)
{
	return devfreq_add_governor(&devfreq_simple_ondemand);
}
subsys_initcall(devfreq_simple_ondemand_init);

static void __exit devfreq_simple_ondemand_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_simple_ondemand);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);

	return;
}
module_exit(devfreq_simple_ondemand_exit);
MODULE_LICENSE("GPL");
