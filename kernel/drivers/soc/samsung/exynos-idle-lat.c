/*
 * Copyright (c) 2018 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos CPU Power Management driver implementation
 */

#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/cpu_pm.h>
#include <linux/cpuidle.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>

#include <linux/ems.h>

/******************************************************************************
 *                        idle latency measuring gadget                       *
 ******************************************************************************/
struct {
	struct delayed_work work;

	struct cpumask target;
	int measurer_cpu;

	int measuring;
	int count;
	u64 latency;

	ktime_t t;
} idle_lmg;

static void idle_latency_work_kick(unsigned long jiffies)
{
	schedule_delayed_work_on(idle_lmg.measurer_cpu, &idle_lmg.work, jiffies);
}

static void idle_latency_measure_start(int target_cpu)
{
	struct cpumask mask;

	cpumask_copy(&mask, cpu_active_mask);
	cpumask_clear_cpu(target_cpu, &mask);
	idle_lmg.measurer_cpu = cpumask_first(&mask);

	/* keep target cpu dle */
	ecs_request("idle_latency", &mask, ECS_MAX);

	cpumask_clear(&idle_lmg.target);
	cpumask_set_cpu(target_cpu, &idle_lmg.target);

	idle_lmg.count = 0;
	idle_lmg.latency = 0;
	idle_lmg.measuring = 1;

	idle_latency_work_kick(0);
}

static void idle_latency_ipi_fn(void *data)
{
	/*
	 * calculating time between sending and receiving IPI
	 * and accumulated in latency.
	 */
	idle_lmg.latency += ktime_to_ns(ktime_sub(ktime_get(), idle_lmg.t));
}

static void idle_latency_work_fn(struct work_struct *work)
{
	/* save current time and send ipi to target cpu */
	idle_lmg.t = ktime_get();
	smp_call_function_many(&idle_lmg.target,
			idle_latency_ipi_fn, NULL, 1);

	/* measuring count = 1000 */
	if (++idle_lmg.count < 1000) {
		/* measuring period = 3 jiffies (8~12ms) */
		idle_latency_work_kick(3);
		return;
	}

	ecs_request("idle_latency", cpu_possible_mask, ECS_MAX);
	idle_lmg.measuring = 0;
}

/******************************************************************************
 *                               sysfs interface                              *
 ******************************************************************************/
static ssize_t idle_wakeup_latency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (cpumask_empty(&idle_lmg.target))
		return scnprintf(buf, PAGE_SIZE, "no measurement result\n");

	return scnprintf(buf, PAGE_SIZE,
			"idle latency(cpu%d) : [%4d/1000] total=%lluns avg=%lluns\n",
			cpumask_any(&idle_lmg.target),
			idle_lmg.count, idle_lmg.latency,
			idle_lmg.latency / idle_lmg.count);
}

static ssize_t idle_wakeup_latency_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int cpu;

	if (!sscanf(buf, "%u", &cpu))
		return -EINVAL;

	if (cpu >= nr_cpu_ids)
		return -EINVAL;

	if (idle_lmg.measuring)
		return count;

	idle_latency_measure_start(cpu);

	return count;
}
DEVICE_ATTR_RW(idle_wakeup_latency);

static struct attribute *exynos_idle_lat_attrs[] = {
	&dev_attr_idle_wakeup_latency.attr,
	NULL,
};

static struct attribute_group exynos_idle_lat_group = {
	.name = "idle-lat",
	.attrs = exynos_idle_lat_attrs,
};

static int exynos_idle_lat_probe(struct platform_device *pdev)
{
	int ret;

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_idle_lat_group);
	if (ret)
		pr_warn("Failed to create sysfs for CPUIDLE latency\n");

	if (sysfs_create_link(&cpu_subsys.dev_root->kobj,
				&pdev->dev.kobj, "idle-lat"))
		pr_err("Failed to link IDLE latency sysfs to cpu\n");

	INIT_DELAYED_WORK(&idle_lmg.work, idle_latency_work_fn);
	ecs_request_register("idle_latency", cpu_possible_mask, ECS_MAX);

	return 0;
}

static const struct of_device_id of_exynos_idle_lat_match[] = {
	{ .compatible = "samsung,exynos-idle-lat", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_idle_lat_match);

static struct platform_driver exynos_idle_lat_driver = {
	.driver = {
		.name = "exynos-idle-lat",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_idle_lat_match,
	},
	.probe = exynos_idle_lat_probe,
};

static int __init exynos_idle_lat_driver_init(void)
{
	return platform_driver_register(&exynos_idle_lat_driver);
}
arch_initcall(exynos_idle_lat_driver_init);

static void __exit exynos_idle_lat_driver_exit(void)
{
	platform_driver_unregister(&exynos_idle_lat_driver);
}
module_exit(exynos_idle_lat_driver_exit);

MODULE_DESCRIPTION("Exynos IDLE latency driver");
MODULE_LICENSE("GPL");
