/*
 * Spreadtrum ARM64 CPU idle driver.
 *
 * Copyright (C) 2014 Spreadtrum Ltd.
 * Author: Icy Zhu <icy.zhu@spreadtrum.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/time.h>

#include <asm/psci.h>
#include <asm/suspend.h>

#include <soc/sprd/hardware.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/cpuidle.h>

#include "of_idle_states.h"

enum {
	STANDBY = 0,  /* WFI */
	L_SLEEP,      /* Light Sleep, WFI & DDR Self-refresh & MCU_SYS_SLEEP */
	CORE_PD,	  /* Core power down & Lightsleep */
	CLUSTER_PD,   /* Cluster power down & Lightsleep */
	TOP_PD,	      /* Top Power Down & Lightsleep */
};
#define LITTLE_CORE_STATUS_MASK   0x000ffff0
#define BIG_CORE_STATUS_MASK      0x0ffff000

static int idx_max = 3;
static int cpuidle_debug = 0;
static int cpuidle_count;
static volatile void *pmu_status;

/* Cpuidle states debug, set a max state for every cpu can be enter */
module_param_named(idx_max, idx_max, int, S_IRUGO | S_IWUSR);
/* For cpuidle debug, set 1 to view every cpu idle states */
module_param_named(cpuidle_debug, cpuidle_debug, int, S_IRUGO | S_IWUSR);
extern void light_sleep_en(void);
extern void light_sleep_dis(void);
static int just_core_0_power_on(void)
{
	if ((sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG, LITTLE_CORE_STATUS_MASK) == 0x77700) &&
			(sci_glb_read(REG_PMU_APB_PWR_STATUS3_DBG, BIG_CORE_STATUS_MASK) == 0x7777000))
		return 1;
	else
		return 0;
}
/*
 * arm_enter_idle_state - Programs CPU to enter the specified state
 *
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @idx: state index
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int arm_enter_idle_state(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int idx)
{
	int ret = 0;
	struct timeval start_time, end_time;
	long usec_elapsed;
	do_gettimeofday(&start_time);
	cpuidle_count++;
	if (idx > idx_max) {
		idx = idx_max;
	}
	switch (idx) {
	case STANDBY:
		cpu_do_idle();
		break;
	case L_SLEEP:
		light_sleep_en();
		cpu_do_idle();
		light_sleep_dis();
		break;
	case CORE_PD:
		light_sleep_en();
		cpu_pm_enter();
		ret = cpu_suspend(idx);
		cpu_pm_exit();
		light_sleep_dis();
		break;
	case CLUSTER_PD:
		light_sleep_en();
		cpu_pm_enter();
#ifdef CONFIG_ARCH_SCX35L64
		cpu_cluster_pm_enter();
#endif
		ret = cpu_suspend(idx);
#ifdef CONFIG_ARCH_SCX35L64
		cpu_cluster_pm_exit();
#endif
		cpu_pm_exit();
		light_sleep_dis();
		break;
	case TOP_PD:
		light_sleep_en();
		cpu_pm_enter();
		if (just_core_0_power_on()) {
			cpu_cluster_pm_enter();
			ret = cpu_suspend(idx);
			cpu_cluster_pm_exit();
		} else
			ret = cpu_suspend(--idx);
		cpu_pm_exit();
		light_sleep_dis();
		break;
	default:
		cpu_do_idle();
		WARN(1, "[CPUIDLE]: NO THIS IDLE LEVEL!!!");
	}
	if (cpuidle_count % 5000 == 0) {
		do_gettimeofday(&end_time);
		usec_elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
			(end_time.tv_usec - start_time.tv_usec);
		pr_info("[CPUIDLE] Enter idle state: %d ,usec_elapsed = %ld \n",
				idx, usec_elapsed);
	 }
	if (cpuidle_debug) {
		do_gettimeofday(&end_time);
		usec_elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000 +
			(end_time.tv_usec - start_time.tv_usec);
		pr_info("[CPUIDLE] Enter idle state: %d ,usec_elapsed = %ld \n",
				idx, usec_elapsed);
		if (cpuidle_count % 500 == 0) {
			cpuidle_debug = 0;
		}
	}
	return ret ? -1 : idx;
}

struct cpuidle_driver arm64_idle_driver = {
	.name = "arm64_idle_sprd",
	.owner = THIS_MODULE,
};

static struct device_node *state_nodes[CPUIDLE_STATE_MAX] __initdata;

/*
 * arm64_idle_sprd_init
 *
 * Registers the arm64 specific cpuidle driver with the cpuidle
 * framework. It relies on core code to parse the idle states
 * and initialize them using driver data structures accordingly.
 */
int __init arm64_idle_sprd_init(void)
{
	int i, ret;
	const char *entry_method;
	struct device_node *idle_states_node;
	const struct cpu_suspend_ops *suspend_init;
	struct cpuidle_driver *drv = &arm64_idle_driver;

	idle_states_node = of_find_node_by_path("/cpus/idle-states");
	if (!idle_states_node) {
		return -ENOENT;
	}
	if (of_property_read_string(idle_states_node, "entry-method",
				    &entry_method)) {
		pr_warn(" * %s missing entry-method property\n",
			    idle_states_node->full_name);
		of_node_put(idle_states_node);
		return -EOPNOTSUPP;
	}
	/*
	 * State at index 0 is standby wfi and considered standard
	 * on all ARM platforms. If in some platforms simple wfi
	 * can't be used as "state 0", DT bindings must be implemented
	 * to work around this issue and allow installing a special
	 * handler for idle state index 0.
	 */
	drv->states[0].exit_latency = 1;
	drv->states[0].target_residency = 1;
#ifdef CONFIG_ARCH_SCX35LT8
	drv->states[0].flags = CPUIDLE_FLAG_TIME_VALID;
#else
	drv->states[0].flags = CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_TIMER_STOP;
#endif
	strncpy(drv->states[0].name, "ARM WFI", CPUIDLE_NAME_LEN);
	strncpy(drv->states[0].desc, "ARM WFI", CPUIDLE_DESC_LEN);

	drv->cpumask = (struct cpumask *) cpu_possible_mask;
	/*
	 * Start at index 1, request idle state nodes to be filled
	 */
	ret = of_init_idle_driver(drv, state_nodes, 1, true);
	if (ret) {
		return ret;
	}

	for (i = 0; i < drv->state_count; i++)
		drv->states[i].enter = arm_enter_idle_state;
	printk("[CPUIDLE]<arm64_idle_sprd_init> init OK. \n");
	return cpuidle_register(drv, NULL);
}
device_initcall(arm64_idle_sprd_init);
