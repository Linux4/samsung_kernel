/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_pm.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <soc/samsung/exynos/exynos-hvc.h>

#include "mfc_plugin_pm.h"

void mfc_plugin_protection_on(struct mfc_core *core)
{
#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&core->pm.clklock, flags);
	mfc_core_debug(3, "Begin: enable protection\n");
	ret = exynos_hvc(HVC_PROTECTION_SET, 0,	PROT_FGN, HVC_PROTECTION_ENABLE, 0);
	if (ret != HVC_OK) {
		mfc_core_err("Protection Enable failed! ret(%u)\n", ret);
		call_dop(core, dump_and_stop_debug_mode, core);
		spin_unlock_irqrestore(&core->pm.clklock, flags);
		return;
	}
	MFC_TRACE_CORE("protection\n");
	mfc_core_debug(3, "End: enable protection\n");
	spin_unlock_irqrestore(&core->pm.clklock, flags);
#endif
}

void mfc_plugin_protection_off(struct mfc_core *core)
{
#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&core->pm.clklock, flags);
	mfc_core_debug(3, "Begin: disable protection\n");
	ret = exynos_hvc(HVC_PROTECTION_SET, 0, PROT_FGN, HVC_PROTECTION_DISABLE, 0);
	if (ret != HVC_OK) {
		mfc_core_err("Protection Disable failed! ret(%u)\n", ret);
		call_dop(core, dump_and_stop_debug_mode, core);
		spin_unlock_irqrestore(&core->pm.clklock, flags);
		return;
	}
	mfc_core_debug(3, "End: disable protection\n");
	MFC_TRACE_CORE("un-protection\n");
	spin_unlock_irqrestore(&core->pm.clklock, flags);
#endif
}

int mfc_plugin_pm_init(struct mfc_core *core)
{
	int ret = 0;

	spin_lock_init(&core->pm.clklock);
	atomic_set(&core->clk_ref, 0);

	core->pm.device = core->device;
	core->pm.clock_on_steps = 0;
	core->pm.clock_off_steps = 0;

	core->pm.clock = clk_get(core->device, "aclk_fg");
	if (IS_ERR(core->pm.clock)) {
		mfc_core_err("failed to get parent clock: %ld\n", PTR_ERR(core->pm.clock));
		return PTR_ERR(core->pm.clock);
	} else {
		ret = clk_prepare(core->pm.clock);
		if (ret) {
			mfc_core_err("clk_prepare() failed: ret(%d)\n", ret);
			clk_put(core->pm.clock);
		}
	}

	return ret;
}

void mfc_plugin_pm_deinit(struct mfc_core *core)
{
	int state;

	state = atomic_read(&core->clk_ref);
	if (state > 0) {
		mfc_core_info("plugin clock is still enabled (%d)\n", state);
		mfc_plugin_pm_clock_off(core);
	}

	if (!IS_ERR(core->pm.clock)) {
		clk_unprepare(core->pm.clock);
		clk_put(core->pm.clock);
	}
}

void mfc_plugin_pm_clock_on(struct mfc_core *core)
{
	int ret = 0;
	int state;

	core->pm.clock_on_steps = 1;
	state = atomic_read(&core->clk_ref);

	core->pm.clock_on_steps |= 0x1 << 1;

	if (!IS_ERR(core->pm.clock)) {
		core->pm.clock_on_steps |= 0x1 << 2;
		ret = clk_enable(core->pm.clock);
		if (ret < 0)
			mfc_core_err("clk_enable failed (%d)\n", ret);
	}

	core->pm.clock_on_steps |= 0x1 << 3;
	state = atomic_inc_return(&core->clk_ref);

	mfc_core_debug(2, "+ %d\n", state);
}

void mfc_plugin_pm_clock_off(struct mfc_core *core)
{
	int state;

	core->pm.clock_off_steps = 1;
	state = atomic_dec_return(&core->clk_ref);
	if (state < 0) {
		mfc_core_info("MFC clock is already disabled (%d)\n", state);
		atomic_set(&core->clk_ref, 0);
		core->pm.clock_off_steps |= 0x1 << 2;
	} else {
		core->pm.clock_off_steps |= 0x1 << 3;
		if (!IS_ERR(core->pm.clock)) {
			clk_disable(core->pm.clock);
			core->pm.clock_off_steps |= 0x1 << 4;
		}
	}

	core->pm.clock_off_steps |= 0x1 << 5;
	state = atomic_read(&core->clk_ref);

	mfc_core_debug(2, "- %d\n", state);
}
