/*
 * drivers/media/platform/exynos/mfc/mfc_core_pm.h
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_CORE_PM_H
#define __MFC_CORE_PM_H __FILE__

#include <linux/clk.h>

#include "base/mfc_common.h"

static inline void mfc_core_pm_clock_get(struct mfc_core *core)
{
	/* This should be called after clock was enabled by mfc_core_pm_clock_on() */
#ifndef PERF_MEASURE
	mfc_core_debug(2, "MFC frequency : %ld\n", clk_get_rate(core->pm.clock));
#else
	mfc_core_info("MFC frequency : %ld\n", clk_get_rate(core->pm.clock));
#endif
}

void mfc_core_pm_init(struct mfc_core *core);
void mfc_core_pm_final(struct mfc_core *core);

void mfc_core_protection_on(struct mfc_core *core, int pending_check);
void mfc_core_protection_off(struct mfc_core *core, int pending_check);
int mfc_core_pm_clock_on(struct mfc_core *core);
void mfc_core_pm_clock_off(struct mfc_core *core);
int mfc_core_pm_power_on(struct mfc_core *core);
int mfc_core_pm_power_off(struct mfc_core *core);

#endif /* __MFC_CORE_PM_H */
