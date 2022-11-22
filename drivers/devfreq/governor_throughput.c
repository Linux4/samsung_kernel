/*
 *  linux/drivers/devfreq/governor_throughput.c
 *
 *  Copyright (C) 2013 Marvell
 *	Leo Song <liangs@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/device.h>
#include "governor.h"


/* Default constants for DevFreq-Throughput */
#define DFSO_UPTHRESHOLD	(90)
#define DFSO_DOWNDIFFERENCTIAL	(5)
#define DFSO_MAX_UPTHRESHOLD	(100)
#define MAX_TABLE_ITEM		(8)

static int devfreq_throughput_func(struct devfreq *df, unsigned long *freq)
{
	struct devfreq_throughput_data *data = df->data;
	struct throughput_threshold *throughput_table = data->throughput_table;
	u32 *freq_table = data->freq_table;
	u32 table_len = data->table_len;
	struct devfreq_dev_status stat;
	int i;
	int err;

	/* only 1 freq point can not use this governor */
	if ((data->table_len <= 1) || (data->table_len > MAX_TABLE_ITEM)) {
		dev_err(&df->dev, "freq table too small or too big!\n");
		return -EINVAL;
	}

	err = df->profile->get_dev_status(df->dev.parent, &stat);
	if (err) {
		/* Used to represent ignoring the profiling result */
		if (err == -EINVAL) {
			*freq = stat.current_frequency;
			return 0;
		} else
		  return err;
	}

	/*
	 * make sure get_dev_status() has updated the current_frequency;
	 * at least set *freq to a reansonable values
	 */
	*freq = stat.current_frequency;

	/*
	 * throughput == -1 is set by low level driver deliberately, to inform
	 * the governor not to trust the throughput data. This is not fatal
	 * error, just end this time's caculation, and do it next time.
	 */
	if (stat.throughput == -1) {
		dev_dbg(&df->dev, "throughput is not suitable!\n");
		return -EAGAIN;
	} else if (stat.throughput < -1) {
		dev_err(&df->dev, "fatal: throughput is not as expected!\n");
		return -EINVAL;
	}

	if ((stat.throughput >= 0)
	 && (stat.throughput < throughput_table[0].down)) {
		/* set to lowest freq */
		*freq = freq_table[0];
	} else if (stat.throughput > throughput_table[table_len - 2].up) {
		/* set to highest freq */
		*freq = freq_table[table_len - 1];
	} else {
		for (i = 0; i <= (table_len - 2); i++) {
			if ((stat.throughput >= throughput_table[i].down)
			 && (stat.throughput <= throughput_table[i].up)) {
				/*
				 * If cur_speed is between UP and DOWN
				 * keep freq unchanged in most cases
				 */
				*freq = stat.current_frequency;
				/* Handle corner case */
				if ((*freq != freq_table[i])
				 && (*freq != freq_table[i + 1])) {
					/* also can choose freq_table[i + 1] */
					*freq = freq_table[i];
				}
				break;
			} else if ((stat.throughput > throughput_table[i].up)
			&& (stat.throughput < throughput_table[i + 1].down)) {
				*freq = freq_table[i + 1];
				break;
			}
		}
	}

	if (df->min_freq && *freq < df->min_freq)
		*freq = df->min_freq;
	if (df->max_freq && *freq > df->max_freq)
		*freq = df->max_freq;

	dev_dbg(&df->dev, "speed=%d, *freq=%lu\n", stat.throughput, *freq);

	return 0;
}


static ssize_t threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct devfreq_throughput_data *data;
	int rc = 0;
	int i;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;

	rc = sprintf(buf, "down_threshold = %u%%, up_threshold = %u%%\n",
		data->upthreshold - data->downdifferential, data->upthreshold);

	rc += sprintf(buf + rc,
			"  freq_point \t  down_level \t    up_level\n");

	for (i = 0; i < data->table_len; i++) {
		rc += sprintf(buf + rc, "%12d\t%12d\t%12d\n",
			data->freq_table[i], data->throughput_table[i].down,
			data->throughput_table[i].up);
	}

	mutex_unlock(&devfreq->lock);

	return rc;
}

static ssize_t threshold_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct devfreq_throughput_data *data;
	int ret;
	int i;
	unsigned int up, down;

	mutex_lock(&devfreq->lock);
	data = devfreq->data;

	ret = sscanf(buf, "%u,%u", &down, &up);
	if (ret != 2 || up > DFSO_MAX_UPTHRESHOLD ||
			down > up) {
		dev_warn(dev, "input threshold error!\n");
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}

	data->upthreshold = up;
	data->downdifferential = up - down;
	for (i = 0; i < data->table_len; i++) {
		data->throughput_table[i].up =
					up * data->freq_table[i] / 100;
		data->throughput_table[i].down =
					down * data->freq_table[i] / 100;
	}
	mutex_unlock(&devfreq->lock);
	return count;
}

static ssize_t help_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc += sprintf(buf + rc, "Tips: echo xx,yy > threshold\n"
			"to change down and up threshold, limit is 100\n"
			"xx is the down level, yy is the up level\n\n");
	return rc;
}

static DEVICE_ATTR(help, 0444,
	help_show, NULL);

static DEVICE_ATTR(threshold, 0644,
	threshold_show, threshold_store);

static struct attribute *dev_entries[] = {
	&dev_attr_threshold.attr,
	&dev_attr_help.attr,
	NULL,
};

static struct attribute_group throughput_attributes = {
	.name	= "throughput",
	.attrs	= dev_entries,
};

static int devfreq_throughput_init(struct devfreq *devfreq)
{
	return sysfs_create_group(&devfreq->dev.kobj,
			&throughput_attributes);
}

static void devfreq_throughput_exit(struct devfreq *devfreq)
{
	sysfs_remove_group(&devfreq->dev.kobj, &throughput_attributes);
}


const struct devfreq_governor devfreq_throughput = {
	.name = "throughput",
	.get_target_freq = devfreq_throughput_func,
	.init = devfreq_throughput_init,
	.exit = devfreq_throughput_exit,
};
