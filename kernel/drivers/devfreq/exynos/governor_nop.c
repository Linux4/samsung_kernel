/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/pm_opp.h>
#include "../governor.h"

#include <soc/samsung/exynos-devfreq.h>
#include <linux/devfreq.h>

struct devfreq_nop_gov_data {
	struct exynos_pm_qos_request		sysbusy_pm_qos;
};

static int devfreq_governor_nop_func(struct devfreq *df,
					unsigned long *freq)
{
	struct exynos_devfreq_data *data = df->data;

	if (data->devfreq_disabled)
		return -EAGAIN;

	*freq = exynos_devfreq_policy_update(data, 0);

	return 0;
}

static int devfreq_governor_sysbusy_callback(struct devfreq *df,
		enum sysbusy_state state)
{
	struct exynos_devfreq_data *data = df->data;
	struct devfreq_nop_gov_data *gov_data = df->governor_data;

	switch (state) {
	case SYSBUSY_STATE0:
		exynos_pm_qos_update_request_nosync(&gov_data->sysbusy_pm_qos, 0);
		break;
	default:
		exynos_pm_qos_update_request_nosync(&gov_data->sysbusy_pm_qos, data->max_freq);
		break;
	}
	return 0;
}

static int devfreq_governor_nop_start(struct devfreq *df)
{
	struct exynos_devfreq_data *data;
	struct devfreq_nop_gov_data *gov_data;

	if (!df)
		return -EINVAL;
	if (!df->data)
		return -EINVAL;

	data = df->data;

	gov_data = kzalloc(sizeof(struct devfreq_nop_gov_data), GFP_KERNEL);
	if (!gov_data)
		return -ENOMEM;

	if (data->sysbusy) {
		data->sysbusy_gov_callback = devfreq_governor_sysbusy_callback;
		exynos_pm_qos_add_request(&gov_data->sysbusy_pm_qos,
				(int)data->pm_qos_class, 0);
	}

	df->governor_data = gov_data;
	return 0;
}

static int devfreq_governor_nop_stop(struct devfreq *df)
{
	struct exynos_devfreq_data *data = df->data;
	struct devfreq_nop_gov_data *gov_data = df->governor_data;
	if (data->sysbusy) {
		data->sysbusy_gov_callback = NULL;
		exynos_pm_qos_remove_request(&gov_data->sysbusy_pm_qos);
	}
	kfree(gov_data);
	return 0;
}

static int devfreq_governor_nop_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	int ret;

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = devfreq_governor_nop_start(devfreq);
		if (ret)
			return ret;
		break;
	case DEVFREQ_GOV_STOP:
		ret = devfreq_governor_nop_stop(devfreq);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_governor_nop = {
	.name = "nop",
	.get_target_freq = devfreq_governor_nop_func,
	.event_handler = devfreq_governor_nop_handler,
};

int exynos_devfreq_governor_nop_init(void)
{
	return devfreq_add_governor(&devfreq_governor_nop);
}

MODULE_LICENSE("GPL");
