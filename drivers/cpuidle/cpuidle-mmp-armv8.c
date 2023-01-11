/*
 * MMP CPU idle driver
 *
 * Author:	Fangsuo Wu <fswu@marvell.com>
 * Copyright:	(C) 2013 marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/clockchips.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/cputype.h>
#include <asm/mcpm.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include <asm/mcpm_plat.h>
#include <linux/clk/mmpdcstat.h>

static int mmp_enter_powerdown(struct cpuidle_device *dev,
				 struct cpuidle_driver *drv, int idx);

static struct cpuidle_driver mmp_idle_driver = {
	.name = "mcpm_idle",
	.owner = THIS_MODULE,
};

/*
 * Support CPU to enter the "WFI" state
 */
int cpuidle_simple_enter(struct cpuidle_device *dev,
			 struct cpuidle_driver *drv, int idx)
{
	cpu_dcstat_event(cpu_dcstat_clk, dev->cpu, CPU_IDLE_ENTER, idx);
	cpu_do_idle();
	cpu_dcstat_event(cpu_dcstat_clk, dev->cpu, CPU_IDLE_EXIT,
		 MAX_LPM_INDEX);

	return idx;
}

/*
 * mcpm_enter_powerdown - Programs CPU to enter the specified state
 *
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @idx: state index
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int mmp_enter_powerdown(struct cpuidle_device *dev,
			       struct cpuidle_driver *drv, int idx)
{
	int ret;

	BUG_ON(!idx);

	cpu_pm_enter();

	ret = cpu_suspend((unsigned long)&idx);
	if (ret)
		pr_err("cpu%d failed to enter power down!", dev->cpu);

	mcpm_cpu_powered_up();

	cpu_pm_exit();

	return idx;
}

/*
 * mcpm_idle_init
 *
 * Registers the mcpm specific cpuidle driver with the cpuidle
 * framework with the valid set of states.
 */
static int __init mmp_cpuidle_init(void)
{
	struct cpuidle_driver *drv = &mmp_idle_driver;
	int i;

	drv->state_count = mcpm_plat_get_cpuidle_states(drv->states);
	if (drv->state_count < 0)
		return drv->state_count;

	for (i = 0; i < drv->state_count; i++) {
		if (!drv->states[i].enter)
			drv->states[i].enter = mmp_enter_powerdown;
	}

	return cpuidle_register(&mmp_idle_driver, NULL);
}

device_initcall(mmp_cpuidle_init);
