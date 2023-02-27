/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
#include <linux/input/input_boost_multi.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

struct delayed_work boost_work;
#ifdef CONFIG_SPRD_CPU_DYNAMIC_HOTPLUG
extern int _store_cpu_num_min_limit(unsigned int input);
#endif
struct cpufreq_limit_handle *min_handle = NULL;
static u32 cpufreq_lock;
static u32 core_num;

static void boost_disable(struct work_struct *work)
{
	if (!min_handle)
		return;
	cpufreq_limit_put(min_handle);
	min_handle = NULL;
#ifdef CONFIG_SPRD_CPU_DYNAMIC_HOTPLUG
	_store_cpu_num_min_limit(1);
#endif
	pr_debug("[cpu hotplug] cpu freq off!\n");

}

#ifdef CONFIG_INPUT_BOOSTER /* Handled by SSRM */
void input_boost_enable(void)
{
	input_booster_send_event(BOOSTER_DEVICE_TOUCH, BOOSTER_MODE_ON);
}
EXPORT_SYMBOL(input_boost_enable);

void input_boost_disable(void)
{
	input_booster_send_event(BOOSTER_DEVICE_TOUCH, BOOSTER_MODE_OFF);
}
EXPORT_SYMBOL(input_boost_disable);

#else /* use sprd cpuhotplug */
void input_boost_enable(void)
{
	if (!min_handle && cpufreq_lock) {
		min_handle = cpufreq_limit_min_freq(cpufreq_lock, "INPUT");
		if (IS_ERR(min_handle)) {
			pr_err("[cpu hotplug] can't get cpufreq_min lock\n");
			min_handle = NULL;
		}
#ifdef CONFIG_SPRD_CPU_DYNAMIC_HOTPLUG
		_store_cpu_num_min_limit(core_num);
#endif
		pr_debug("[cpu hotplug] cpu freq on!\n");
	}
}
EXPORT_SYMBOL(input_boost_enable);

void input_boost_disable(void)
{
	if (!cpufreq_lock)
		return;

	cancel_delayed_work(&boost_work);
	schedule_delayed_work(&boost_work, 1*HZ);
}
EXPORT_SYMBOL(input_boost_disable);
#endif /* CONFIG_INPUT_BOOSTER */

static void init_boost(void)
{
#if defined(CONFIG_OF) && !defined(CONFIG_INPUT_BOOSTER)
	struct device_node *np;
	int ret;

	np = of_find_node_by_name(NULL, "sprd-cpuhotplug");
	if (!np) {
		pr_debug("[cpu hotplug]can't find sprd-hotplug\n");
	} else {
		ret = of_property_read_u32(np, "min_cpufreq", &cpufreq_lock);
		if (ret < 0) {
			pr_err("[cpu hotplug] failed to get min_cpufreq_lock\n");
			return;
		}
		ret = of_property_read_u32(np, "core_num", &core_num);
		if (ret < 0) {
			pr_err("[cpu hotplug] failed to get core_num\n");
			return;
		}
	}
	INIT_DELAYED_WORK(&boost_work, boost_disable);
#endif
}

static int __init input_booster_init(void)
{
	init_boost();
	return 0;
}

static void __exit input_booster_exit(void)
{
	destroy_workqueue(boost_work.wq);
}

module_init(input_booster_init);
module_exit(input_booster_exit);

MODULE_DESCRIPTION("Input Device Driver booster interface");
MODULE_AUTHOR("System S/W 2G");
MODULE_LICENSE("GPL");
