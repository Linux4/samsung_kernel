/*
 *  linux/drivers/devfreq/governor_throughput.c
 *
 *  Copyright (C) 2013 Marvell
 *	Leo Song <liangs@marvell.com>
 *	Qiming Wu <wuqm@marvell.com>
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


/* Default constants for DevFreq-Throughput */
#define DFSO_UPTHRESHOLD	(90)
#define DFSO_DOWNDIFFERENCTIAL	(5)
#define DFSO_MAX_UPTHRESHOLD	(100)
#define MAX_TABLE_ITEM		(20)

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
	 * the governor that the throughput data may not correct this time.
	 * This is not fatal error, just end this time's caculation, and do it
	 * next time.
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

static int devfreq_throughput_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_START:
		devfreq_monitor_start(devfreq);
		break;

	case DEVFREQ_GOV_STOP:
		devfreq_monitor_stop(devfreq);
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

static struct devfreq_governor devfreq_throughput = {
	.name = "throughput",
	.get_target_freq = devfreq_throughput_func,
	.event_handler = devfreq_throughput_handler,
};

static int __init devfreq_throughput_init(void)
{
	return devfreq_add_governor(&devfreq_throughput);
}
subsys_initcall(devfreq_throughput_init);

static void __exit devfreq_throughput_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_throughput);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);

	return;
}
module_exit(devfreq_throughput_exit);
MODULE_LICENSE("GPL");
