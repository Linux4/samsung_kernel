/*
 * arch/arm/mach-mmp/include/mach/pm.h
 *
 * Author:	Fan Wu <fwu@marvell.com>
 * Copyright:	(C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __MACH_PM_H__
#define __MACH_PM_H__

/* MMP suspend API Function Notes:
 * 1) set_wake:
 *    offer interrupt clients to enable itself as wakeup source
 * 2) pre_suspend_check:
 *    Anything need to do before entering suspend.
 *    return 0 if there is nothing wrong or unexpected.
 * 3) post_chk_wakeup:
 *    Wakeup source check after suspend, often for debug usage.
 *    return the wakeup source bit map to the caller.
 * 4) post_clr_wakeup:
 *    Clear the possible wakeup source after suspend.
 * 5) plt_suspend_init:
 *    Platform specified suspend init operation can be included in
 *    this function.
 */
struct suspend_ops {
	void (*set_wake)(int irq, unsigned int on);
	int (*pre_suspend_check)(void);
	u32 (*post_chk_wakeup)(void);
	void (*post_clr_wakeup)(u32 wakeup_src_stat);
	void (*plt_suspend_init)(void);
	int (*get_suspend_voltage)(void);
};

struct platform_suspend {
	unsigned int suspend_state;
	struct suspend_ops *ops;
};

extern int mmp_platform_suspend_register(struct platform_suspend *plt_suspend);
#endif
