/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS IPs Traffic Monitor Driver for Samsung EXYNOS SoC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef EXYNOS_ITMON__H
#define EXYNOS_ITMON__H

#include <dt-bindings/soc/samsung/exynos/debug-snapshot-def.h>
#include <linux/notifier.h>
#include "exynos-itmon-history.h"

struct itmon_notifier {
	char *port;			/* The block to which the master IP belongs */
	char *master;			/* The master's name which problem occurred */
	char *dest;			/* The destination which the master tried to access */
	bool read;			/* Transaction Type */
	unsigned long target_addr;	/* The physical address which the master tried to access */
	unsigned int errcode;		/* The error code which the problem occurred */
	bool onoff;			/* Target Block on/off */
	char *pd_name;			/* Target Block power domain name */
};

#define M_NODE			(0)
#define T_S_NODE		(1)
#define T_M_NODE		(2)
#define S_NODE			(3)
#define NODE_TYPE		(4)

#define ITMON_NOTIFY_MASK	((0x800) | NOTIFY_STOP_MASK)
#define ITMON_SKIP_MASK		(ITMON_NOTIFY_MASK | GO_DEFAULT_ID)
#define ITMON_PANIC_MASK	(ITMON_NOTIFY_MASK | GO_PANIC_ID)
#define ITMON_WATCHDOG_MASK	(ITMON_NOTIFY_MASK | GO_WATCHDOG_ID)
#define ITMON_S2D_MASK		(ITMON_NOTIFY_MASK | GO_S2D_ID)

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
extern void itmon_notifier_chain_register(struct notifier_block *n);
extern void itmon_en(bool en);
extern int itmon_en_by_name(const char *name, bool en);
extern int itmon_set_nodepolicy_by_name(const char *name, u64 end, bool now);
extern void itmon_pd_sync(const char *pd_name, bool end);
extern void itmon_print_history(void);
#else
static inline void itmon_en(bool en) {}
#define itmon_notifier_chain_register(x)		do { } while (0)
#define itmon_en(x)					do { } while (0)
#define itmon_pd_sync(x,y)				do { } while (0)
static inline int itmon_en_by_name(const char *name, bool en)
{
	return -1;
}
static inline int itmon_set_nodepolicy_by_name(const char *name, u64 end, bool now)
{
	return -1;
}
#endif

#endif
