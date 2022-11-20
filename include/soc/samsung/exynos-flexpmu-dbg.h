/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Debug-SnapShot: Debug Framework for Ramdump based debugging method
 * The original code is Exynos-Snapshot for Exynos SoC
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 * Author: Changki Kim <changki.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef EXYNOS_FLEXPMU_DEBUG_H
#define EXYNOS_FLEXPMU_DEBUG_H

#ifdef CONFIG_EXYNOS_FLEXPMU_DBG
extern int exynos_acpm_pasr_mask_read(void);
extern int exynos_acpm_pasr_size_read(void);
extern int exynos_acpm_pasr_write(u32 mask, u32 size);
extern void exynos_flexpmu_dbg_log_stop(void);
extern void exynos_flexpmu_dbg_suspend_mif_req(void);
extern void exynos_flexpmu_dbg_resume_mif_req(void);
#else
#define exynos_flexpmu_dbg_log_stop()		do { } while(0)
static inline int exynos_acpm_pasr_mask_read(void)
{
	return 0;
}
static inline int exynos_acpm_pasr_size_read(void)
{
	return 0;
}
static inline int exynos_acpm_pasr_write(u32 mask, u32 size)
{
	return 0;
}
#endif

#endif
