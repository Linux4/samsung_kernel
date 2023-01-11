/*
 * linux/arch/arm/mach-mmp/include/mach/mmp_cpuidle.h
 *
 * Author:	Fangsuo Wu <fswu@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __MMP_MACH_MMP_CPUIDLE_H__
#define __MMP_MACH_MMP_CPUIDLE_H__

struct platform_power_ops {
	void (*set_pmu)(u32 cpu, u32 power_state);
	void (*clr_pmu)(u32 cpu);
	void (*save_wakeup)(void);
	void (*restore_wakeup)(void);
	void (*power_up_setup)(unsigned int);
};

struct platform_idle {
	u32 cpudown_state;	/* hardware state when cpu be shutdown */
	u32 wakeup_state;	/* hardware state when wakeup be set */
	u32 hotplug_state;	/* hardware state when cpu hotplug out */
	u32 l2_flush_state;	/* hardware state when l2 flush is need */
	u32 state_count;	/* number of hardware states */
	struct platform_power_ops *ops;
	struct cpuidle_state *states;
};

void ca7_power_up_setup(unsigned int r);

int __init mmp_platform_power_register(struct platform_idle *idle);

extern int __init mcpm_platform_state_register
		(struct cpuidle_state *states, int count);
#endif
