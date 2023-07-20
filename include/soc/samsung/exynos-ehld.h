/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos Early Hardlockup Detector for Samsung EXYNOS SoC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef EXYNOS_EHLD__H
#define EXYNOS_EHLD__H

#define NUM_TRACE			(16)

#define EHLD_STAT_NORMAL		(0)
#define EHLD_STAT_LOCKUP_WARN		(1)
#define EHLD_STAT_LOCKUP_SW		(2)
#define EHLD_STAT_LOCKUP_HW		(3)

#ifdef CONFIG_EXYNOS_EHLD
extern void exynos_ehld_event_raw_update(int cpu);
extern void exynos_ehld_event_raw_dump(int cpu);
extern void exynos_ehld_event_raw_update_allcpu(void);
extern void exynos_ehld_event_raw_dump_allcpu(void);
extern void exynos_ehld_do_policy(void);
extern int exynos_ehld_start(void);
extern void exynos_ehld_stop(void);
extern void exynos_ehld_do_ipi(int cpu, unsigned int ipinr);
extern void exynos_ehld_prepare_panic(void);
#else
#define exynos_ehld_event_raw_update(a)			do { } while (0)
#define exynos_ehld_event_raw_dump(a)			do { } while (0)
#define exynos_ehld_event_raw_update_allcpu(void)	do { } while (0)
#define exynos_ehld_event_raw_dump_allcpu(void)		do { } while (0)
#define exynos_ehld_do_policy(void)			do { } while (0)
#define exynos_ehld_start(void)				do { } while (0)
#define exynos_ehld_stop(void)				do { } while (0)
#define exynos_ehld_do_ipi(a,b)				do { } while (0)
#define exynos_ehld_prepare_panic(void)			do { } while (0)
#endif

#endif
