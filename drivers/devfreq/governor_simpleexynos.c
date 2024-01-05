/*
 *  linux/drivers/devfreq/governor_simpleexynos.c
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
#include <linux/pm_qos.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "governor.h"

struct devfreq_simple_exynos_notifier_block {
	struct list_head node;
	struct notifier_block nb;
	struct devfreq *df;
};

static LIST_HEAD(devfreq_simple_exynos_list);
static DEFINE_MUTEX(devfreq_simple_exynos_mutex);

static int gov_simple_exynos = 0;

static int devfreq_simple_exynos_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_simple_exynos_notifier_block *pq_nb;
	struct devfreq_simple_exynos_data *simple_exynos_data;

	pq_nb = container_of(nb, struct devfreq_simple_exynos_notifier_block, nb);

	simple_exynos_data = pq_nb->df->data;

	mutex_lock(&pq_nb->df->lock);
	update_devfreq(pq_nb->df);
	mutex_unlock(&pq_nb->df->lock);

	return NOTIFY_OK;
}

/* Default constants for DevFreq-Simple-Exynos(DFE) */
#define DFE_URGENTTHRESHOLD	(65)
#define DFE_UPTHRESHOLD		(60)
#define DFE_DOWNTHRESHOLD	(45)
#define DFE_IDLETHRESHOLD	(30)

static int devfreq_simple_exynos_func(struct devfreq *df,
					unsigned long *freq)
{
	struct devfreq_dev_status stat;
	unsigned int dfe_urgentthreshold = DFE_URGENTTHRESHOLD;
	unsigned int dfe_upthreshold = DFE_UPTHRESHOLD;
	unsigned int dfe_downthreshold = DFE_DOWNTHRESHOLD;
	unsigned int dfe_idlethreshold = DFE_IDLETHRESHOLD;
	struct devfreq_simple_exynos_data *data = df->data;
	unsigned long max = (df->max_freq) ? df->max_freq : UINT_MAX;
	unsigned long pm_qos_min = 0, cal_qos_max = 0;
	unsigned long usage_rate;
	int err;

	err = df->profile->get_dev_status(df->dev.parent, &stat);
	if (err)
		return err;

	if (data) {
		pm_qos_min = pm_qos_request(data->pm_qos_class);
		if (unlikely(gov_simple_exynos))
			printk("pm_qos: %lu\n", pm_qos_min);

		if (data->urgentthreshold)
			dfe_urgentthreshold = data->urgentthreshold;
		if (data->upthreshold)
			dfe_upthreshold = data->upthreshold;
		if (data->downthreshold)
			dfe_downthreshold = data->downthreshold;
		if (data->idlethreshold)
			dfe_idlethreshold = data->idlethreshold;

		if (data->cal_qos_max) {
			cal_qos_max = data->cal_qos_max;
			max = (df->max_freq) ? df->max_freq : 0;
		} else {
			cal_qos_max = df->max_freq;
		}
	}

	/* Assume MAX if it is going to be divided by zero */
	if (stat.total_time == 0) {
		*freq = max3(max, cal_qos_max, pm_qos_min);
		return 0;
	}

	/* Set MAX if we do not know the initial frequency */
	if (stat.current_frequency == 0) {
		*freq = max3(max, cal_qos_max, pm_qos_min);
		return 0;
	}

	usage_rate = div64_u64(stat.busy_time * 100, stat.total_time);

	/* Set MAX if it's busy enough */
	if (usage_rate > dfe_urgentthreshold)
		*freq = max3(max, cal_qos_max, pm_qos_min);
	else if (usage_rate >= dfe_upthreshold)
		*freq = data->above_freq;
	else if (usage_rate > dfe_downthreshold)
		*freq = stat.current_frequency;
	else if (usage_rate > dfe_idlethreshold)
		*freq = data->below_freq;
	else
		*freq = 0;

	if (*freq > cal_qos_max)
		*freq = cal_qos_max;

	if (pm_qos_min)
		*freq = max(pm_qos_min, *freq);

	if (df->min_freq && *freq < df->min_freq)
		*freq = df->min_freq;
	if (df->max_freq && *freq > df->max_freq)
		*freq = df->max_freq;

	if (unlikely(gov_simple_exynos))
		printk("Usage: %lu, freq: %lu, old: %lu\n", usage_rate, *freq, stat.current_frequency);

	return 0;
}

static int devfreq_simple_exynos_init(struct devfreq *df)
{
	int ret;
	struct devfreq_simple_exynos_notifier_block *pq_nb;
	struct devfreq_simple_exynos_data *data = df->data;

	if (!data)
		return -EINVAL;

	pq_nb = kzalloc(sizeof(*pq_nb), GFP_KERNEL);
	if (!pq_nb)
		return -ENOMEM;

	pq_nb->df = df;
	pq_nb->nb.notifier_call = devfreq_simple_exynos_notifier;
	INIT_LIST_HEAD(&pq_nb->node);

	ret = pm_qos_add_notifier(data->pm_qos_class, &pq_nb->nb);
	if (ret < 0)
		goto err;

	mutex_lock(&devfreq_simple_exynos_mutex);
	list_add_tail(&pq_nb->node, &devfreq_simple_exynos_list);
	mutex_unlock(&devfreq_simple_exynos_mutex);

	return 0;

err:
	kfree(pq_nb);
	return ret;
}

struct devfreq_governor devfreq_simple_exynos = {
	.name = "simple_exynos",
	.get_target_freq = devfreq_simple_exynos_func,
	.init = devfreq_simple_exynos_init,
};

MODULE_LICENSE("GPL");
