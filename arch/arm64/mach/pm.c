/*
 * linux/arch/arm/mach-mmp/pm.c
 *
 * Author:      Fan Wu <fwu@marvell.com>
 * Copyright:   (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/mcpm.h>
#include <linux/suspend.h>
#include <asm/suspend.h>
#include <asm/cputype.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/clk/mmpfuse.h>
#ifdef CONFIG_SEC_GPIO_DVS
#include <linux/secgpio_dvs.h>
#endif

#include "pm.h"

struct platform_suspend *mmp_suspend;
static int fuse_suspd_volt;
u32 detect_wakeup_status;
EXPORT_SYMBOL(detect_wakeup_status);
/*
static int notrace mcpm_powerdown_finisher(unsigned long arg)
{
	u32 mpidr = read_cpuid_mpidr();
	u32 cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	u32 this_cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	mcpm_set_entry_vector(cpu, this_cluster, cpu_resume);
	mcpm_cpu_suspend(arg);
	return 1;
}
*/
static int mmp_pm_enter(suspend_state_t state)
{
	unsigned int real_idx = mmp_suspend->suspend_state;

#ifdef CONFIG_SEC_GPIO_DVS
	/************************ Caution !!! ****************************
	This function must be located in appropriate SLEEP position
	in accordance with the specification of each BB vendor.
	************************ Caution !!! ****************************/
	gpio_dvs_check_sleepgpio();
#endif

	if (mmp_suspend->ops->pre_suspend_check) {
		if (mmp_suspend->ops->pre_suspend_check())
			return -EAGAIN;
	}

	cpu_suspend((unsigned long)&real_idx);

	if (mmp_suspend->ops->post_chk_wakeup)
		detect_wakeup_status = mmp_suspend->ops->post_chk_wakeup();

	if (mmp_suspend->ops->post_clr_wakeup)
		mmp_suspend->ops->post_clr_wakeup(detect_wakeup_status);

	if (real_idx != mmp_suspend->suspend_state)
		pr_info("WARNING!!! Suspend Didn't enter the Expected Low power mode\n");

	mcpm_cpu_powered_up();

	return 0;
}

static int mmp_pm_valid(suspend_state_t state)
{
	return (state == PM_SUSPEND_STANDBY) || (state == PM_SUSPEND_MEM);
}

/* Called after devices suspend, before noirq devices suspend */
static int mmp_pm_prepare(void)
{
	return 0;
}

/* Called after noirq devices resume, before devices resume */
static void mmp_pm_finish(void)
{
}

/* Called after Non-boot CPUs have been Hotplug in if necessary */
static void mmp_pm_wake(void)
{
}

static const struct platform_suspend_ops mmp_pm_ops = {
	.valid          = mmp_pm_valid,
	.prepare        = mmp_pm_prepare,
	.enter          = mmp_pm_enter,
	.finish         = mmp_pm_finish,
	.wake           = mmp_pm_wake,
};

int mmp_set_wake(struct irq_data *data, unsigned int on)
{
	int irq = data->irq;
	struct irq_desc *desc = irq_to_desc(data->irq);

	if (!mmp_suspend->ops->set_wake) {
		WARN_ON("Platform set_wake function is not Existed!!");
		return -EINVAL;
	}
	if (!desc) {
		pr_err("irq_desc is NULL\n");
		return -EINVAL;
	}

	if (on) {
		if (desc->action)
			desc->action->flags |= IRQF_NO_SUSPEND;
	} else {
		if (desc->action)
			desc->action->flags &= ~IRQF_NO_SUSPEND;
	}

	mmp_suspend->ops->set_wake(irq, on);

	return 0;
}

int __init mmp_platform_suspend_register(struct platform_suspend *plt_suspend)
{
	if (mmp_suspend)
		return -EBUSY;

	detect_wakeup_status = 0;

	mmp_suspend = plt_suspend;

	suspend_set_ops(&mmp_pm_ops);

	gic_arch_extn.irq_set_wake = mmp_set_wake;

	if (mmp_suspend->ops->plt_suspend_init)
		mmp_suspend->ops->plt_suspend_init();

	if (mmp_suspend->ops->get_suspend_voltage)
		fuse_suspd_volt = mmp_suspend->ops->get_suspend_voltage();

	return 0;
}

int get_fuse_suspd_voltage(void)
{
	return fuse_suspd_volt;
}

