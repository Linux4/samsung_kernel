/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef _WATCHDOG_H_
#define _WATCHDOG_H_

#include <linux/platform_device.h>

struct timer_operation;

struct cp_watchdog {
	int irq;
	u32 type; /* watchdog type */

	u32 timer_num;
	u32 match_num;

	void __iomem *base;
	void *data;

	const struct timer_operation *ops;
};

extern struct cp_watchdog *cp_watchdog;

extern int cp_watchdog_probe(struct platform_device *pdev);
extern int cp_watchdog_remove(struct platform_device *pdev);

extern void watchdog_count_stop(void);
extern void watchdog_interrupt_clear(void);
extern void watchdog_hw_kick(void);
extern bool watchdog_deactive(void);
extern int watchdog_suspend(void);
extern int watchdog_resume(void);

#endif
