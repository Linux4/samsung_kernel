/*
 * ARM64 generic CPU idle driver.
 *
 * Copyright (C) 2013 ARM Ltd.
 * Author: Lorenzo Pieralisi <lorenzo.pieralisi@arm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>

static int arm64_enter_state(struct cpuidle_device *dev,
			    struct cpuidle_driver *drv, int idx);

struct cpuidle_driver arm64_idle_driver = {
	.name = "arm64_idle",
	.owner = THIS_MODULE,
	.states[0] = {
		.enter                  = arm64_enter_state,
		.exit_latency           = 1,
		.target_residency       = 1,
		.power_usage		= UINT_MAX,
		.flags                  = CPUIDLE_FLAG_TIME_VALID,
		.name                   = "C1",
		.desc                   = "ARM WFI",
	},
	.states[1] = {
		.enter			= arm64_enter_state,
		/*
		 * These values are completely arbitrary for now. They will
		 * be properly defined once DT bindings for ARM C-states
		 * are standardized and generic drivers can be initialized
		 * using DT/ACPI configuration values.
		 */
		.exit_latency		= 100,
		.target_residency	= 500,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C2",
		.desc			= "ARM64 affinity 0",
	},
	.states[2] = {
		.enter			= arm64_enter_state,
		/*
		 * These values are completely arbitrary for now. They will
		 * be properly defined once DT bindings for ARM C-states
		 * are standardized and generic drivers can be initialized
		 * using DT/ACPI configuration values.
		 */
		.exit_latency		= 300,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C3",
		.desc			= "ARM64 affinity 1",
	},
	.state_count = 3,
};

/*
 * arm64_enter_state - Programs CPU to enter the specified state
 *
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @idx: state index
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int arm64_enter_state(struct cpuidle_device *dev,
			     struct cpuidle_driver *drv, int idx)
{
	int ret;

	if (!idx) {
		/*
		 * C1 is just standby wfi, does not require CPU
		 * to be suspended
		 */
		cpu_do_idle();
		return idx;
	}

	cpu_pm_enter();
	/*
	 * Pass C-state index to cpu_suspend which in turn will call
	 * the CPU ops suspend protocol with index as a parameter
	 */
	ret = cpu_suspend(idx);
	if (ret)
		pr_warn_once("returning from cpu_suspend %s %d\n",
			     __func__, ret);
	/*
	 * Trigger notifier only if cpu_suspend succeeded
	 */
	if (!ret)
		cpu_pm_exit();

	return idx;
}

/*
 * arm64_idle_init
 *
 * Registers the arm specific cpuidle driver with the cpuidle
 * framework. It relies on core code to set-up the driver cpumask
 * and initialize it to online CPUs.
 */
int __init arm64_idle_init(void)
{
	return cpuidle_register(&arm64_idle_driver, NULL);
}
device_initcall(arm64_idle_init);
