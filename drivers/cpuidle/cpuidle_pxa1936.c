/*
 * Pxa1936(helan3) CPU idle driver.
 *
 * Copyright (C) 2014 Marvell Ltd.
 * Author: Xiaoguang Chen <chenxg@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pxa1936_powermode.h>
#include <asm/suspend.h>
#include <asm/proc-fns.h>
#include <asm/psci.h>
#include <linux/clk/mmpdcstat.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/helanx_smc.h>

static struct notifier_block mfp_edge_wakeup_notifier;

static int arm64_enter_state(struct cpuidle_device *dev,
			     struct cpuidle_driver *drv, int idx);

struct cpuidle_driver arm64_idle_driver = {
	.name = "arm64_idle",
	.owner = THIS_MODULE,
	.states[POWER_MODE_CORE_INTIDLE] = {
		.enter = arm64_enter_state,
		.exit_latency = 18,
		.target_residency = 36,
		/*
		 * Use CPUIDLE_FLAG_TIMER_STOP flag to let the cpuidle
		 * framework handle the CLOCK_EVENT_NOTIFY_BROADCAST_
		 * ENTER/EXIT when entering idle states.
		 */
		.flags = CPUIDLE_FLAG_TIME_VALID,
		.name = "C1",
		.desc = "C1: Core internal clock gated",
	},
	.states[POWER_MODE_CORE_EXTIDLE] = {
		.disabled = 1,
	},
	.states[POWER_MODE_CORE_POWERDOWN] = {
		.enter = arm64_enter_state,
		.exit_latency = 20,
		.target_residency = 40,
		.flags = CPUIDLE_FLAG_TIME_VALID |
			 CPUIDLE_FLAG_TIMER_STOP,
		.name = "C2",
		.desc = "C2: Core power down",
	},
	.states[POWER_MODE_MP_IDLE_CORE_EXTIDLE] = {
		.disabled = 1,
	},
	.states[POWER_MODE_MP_IDLE_CORE_POWERDOWN] = {
		.disabled = 1,
	},
	.states[POWER_MODE_MP_POWERDOWN_L2_ON] = {
		.disabled = 1,
	},
	.states[POWER_MODE_MP_POWERDOWN_L2_OFF] = {
		.enter = arm64_enter_state,
		.exit_latency = 450,
		.target_residency = 900,
		.flags = CPUIDLE_FLAG_TIME_VALID |
			 CPUIDLE_FLAG_TIMER_STOP,
		.name = "MP2",
		.desc = "MP2: Core subsystem power down",
	},
	.states[POWER_MODE_APPS_IDLE] = {
		.enter = arm64_enter_state,
		.exit_latency = 500,
		.target_residency = 1000,
		.flags = CPUIDLE_FLAG_TIME_VALID |
			 CPUIDLE_FLAG_TIMER_STOP,
		.name = "D1p",
		.desc = "D1p: AP idle state",
	},
	.states[POWER_MODE_APPS_IDLE_DDR] = {
		.disabled = 1,
	},
	.states[POWER_MODE_APPS_SLEEP] = {
		.disabled = 1,
	},
	.states[POWER_MODE_APPS_SLEEP_UDR] = {
		.disabled = 1,
	},
	.states[POWER_MODE_SYS_SLEEP_VCTCXO] = {
		.disabled = 1,
	},
	.states[POWER_MODE_SYS_SLEEP_VCTCXO_OFF] = {
		.enter = arm64_enter_state,
		.exit_latency = 600,
		.target_residency = 1200,
		.flags = CPUIDLE_FLAG_TIME_VALID |
			 CPUIDLE_FLAG_TIMER_STOP,
		.name = "D1",
		.desc = "D1: Chip idle state",
	},

	.state_count = 13,
};

static unsigned int states_disabled_cpu0;

/* FIXME: Support only one component to lock/unlock same index */
void cpuidle_c2_lock(void)
{
	int i;
	struct cpuidle_device *dev = per_cpu(cpuidle_devices, 0);
	struct cpuidle_state_usage *su = dev->states_usage;

	states_disabled_cpu0 = 0;
	for (i = 0; i < arm64_idle_driver.state_count; i++)
		states_disabled_cpu0 |= (su[i].disable << i);

	su[POWER_MODE_CORE_POWERDOWN].disable = 1;
	su[POWER_MODE_MP_POWERDOWN_L2_OFF].disable = 1;
	su[POWER_MODE_APPS_IDLE].disable = 1;
	su[POWER_MODE_SYS_SLEEP_VCTCXO_OFF].disable = 1;
}
EXPORT_SYMBOL_GPL(cpuidle_c2_lock);

void cpuidle_c2_unlock(void)
{
	int i;
	struct cpuidle_device *dev = per_cpu(cpuidle_devices, 0);
	struct cpuidle_state_usage *su = dev->states_usage;

	for (i = 0; i < arm64_idle_driver.state_count; i++)
		su[i].disable = (states_disabled_cpu0 & (1 << i))?1:0;
}
EXPORT_SYMBOL_GPL(cpuidle_c2_unlock);

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
	int cpu = dev->cpu;

	if (!idx) {
		/*
		 * C1 is just standby wfi, does not require CPU
		 * to be suspended
		 */
		cpu_dcstat_event(cpu_dcstat_clk, cpu, CPU_IDLE_ENTER, 0);
		cpu_do_idle();
		cpu_dcstat_event(cpu_dcstat_clk, cpu, CPU_IDLE_EXIT, MAX_LPM_INDEX);
		return idx;
	}

	cpu_pm_enter();
	/*
	 * Pass C-state index to cpu_suspend which in turn will call
	 * the CPU ops suspend protocol with index as a parameter
	 */
	ret = cpu_suspend((unsigned long)&idx);
	if (ret)
		pr_warn_once("returning from cpu_suspend %s %d\n",
			     __func__, ret);

	/* add cpuidle exit cpu_dc_stat */
	cpu_dcstat_event(cpu_dcstat_clk, cpu, CPU_IDLE_EXIT, MAX_LPM_INDEX);
#ifdef CONFIG_VOLDC_STAT
	vol_dcstat_event(VLSTAT_LPM_EXIT, 0, 0);
#endif
	/*
	 * Trigger notifier only if cpu_suspend succeeded
	 */
	if (!ret)
		cpu_pm_exit();

	return idx;
}

static const struct of_device_id psci_of_match[] __initconst = {
	{ .compatible = "arm,psci",},
	{},
};

static int __init check_platform(void)
{
	struct device_node *np;

	if (!of_machine_is_compatible("marvell,pxa1936"))
		return -ENODEV;

	np = of_find_matching_node(NULL, psci_of_match);
	if (!np || !of_device_is_available(np))
		return -ENODEV;
	return 0;
}

int __init setup_mfp_notify(void)
{
	int err = check_platform();
	if (err)
		return err;

	mfp_edge_wakeup_notifier.notifier_call = mfp_edge_wakeup_notify;

	return register_mfp_edge_wakup_notifier(&mfp_edge_wakeup_notifier);
}
core_initcall(setup_mfp_notify);

/*
 * arm64_idle_init
 *
 * Registers the arm specific cpuidle driver with the cpuidle
 * framework. It relies on core code to set-up the driver cpumask
 * and initialize it to online CPUs.
 */
int __init arm64_idle_init(void)
{
	int err = check_platform();
	if (err)
		return err;

	return cpuidle_register(&arm64_idle_driver, NULL);
}
device_initcall(arm64_idle_init);
