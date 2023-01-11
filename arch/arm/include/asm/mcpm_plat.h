/*
 * linux/arch/arm64/include/asm/mcpm_plat.h
 *
 * Author:	Fangsuo Wu <fswu@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MCPM_PLAT_H__
#define __MCPM_PLAT_H__

#ifdef CONFIG_CPU_IDLE
extern int cpuidle_simple_enter(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index);
#else
static inline int cpuidle_simple_enter(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index) { return -ENODEV; }
#endif

struct platform_power_ops {
	void (*release)(u32 cpu);
	void (*set_pmu)(u32 cpu, u32 power_state, u32 vote_state);
	void (*clr_pmu)(u32 cpu);
	void (*save_wakeup)(void);
	void (*restore_wakeup)(void);
	void (*power_up_setup)(unsigned int);
};

struct platform_idle {
	u32 cpudown_state;	/* hardware state when cpu be shutdown */
	u32 clusterdown_state;	/* hardware state when cluster be shutdown */
	u32 wakeup_state;	/* hardware state when wakeup be set */
	u32 hotplug_state;	/* hardware state when cpu hotplug out */
	u32 l2_flush_state;	/* hardware state when l2 flush is need */
	u32 state_count;	/* number of hardware states */
	struct platform_power_ops *ops;
	struct cpuidle_state *states;
};

int __init mcpm_plat_power_register(struct platform_idle *idle);
int __init mcpm_plat_get_cpuidle_states(struct cpuidle_state *states);

#endif /* __MCPM_PLAT_H__ */
