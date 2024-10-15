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

#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <soc/samsung/exynos/exynos-hvc.h>

#include "mfc_plugin_pm.h"
#include "base/mfc_utils.h"

void mfc_plugin_protection_switch(struct mfc_core *core, bool is_drm)
{
#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&core->pm.clklock, flags);
	mfc_core_debug(3, "Begin: %s protection\n", is_drm ? "enable" : "disable");
	ret = exynos_hvc(HVC_PROTECTION_SET, 0, PROT_FGN,
			 is_drm ? HVC_PROTECTION_ENABLE : HVC_PROTECTION_DISABLE, 0);
	if (ret != HVC_OK) {
		mfc_core_err("Protection %s failed! ret(%u)\n", is_drm ? "Enable" : "Disable", ret);
		call_dop(core, dump_and_stop_debug_mode, core);
		spin_unlock_irqrestore(&core->pm.clklock, flags);
		return;
	}
	mfc_core_change_attribute(core, is_drm);

	MFC_TRACE_CORE("%s\n", is_drm ? "protection" : "un-protection");
	mfc_core_debug(3, "End: %s protection\n", is_drm ? "enable" : "disable");
	spin_unlock_irqrestore(&core->pm.clklock, flags);
#endif
}

int mfc_plugin_pm_init(struct mfc_core *core)
{
	int ret;

	spin_lock_init(&core->pm.clklock);
	atomic_set(&core->clk_ref, 0);

	core->pm.device = core->device;
	core->pm.clock_on_steps = 0;
	core->pm.clock_off_steps = 0;

	pm_runtime_enable(core->pm.device);

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

	return 0;
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

	pm_runtime_disable(core->pm.device);
}

int mfc_plugin_pm_power_on(struct mfc_core *core)
{
	int ret = 0;
	int state;

	mfc_core_debug_enter();

	state = atomic_read(&core->pm.pwr_ref);

	MFC_TRACE_CORE("++ Power on: ref(%d)\n", state);
	mfc_core_debug(3, "[PLUGIN] ++ power on: ref(%d)\n", state);
	ret = pm_runtime_get_sync(core->pm.device);
	if (ret < 0) {
		mfc_core_err("Failed to get power: ret(%d)\n", ret);
		call_dop(core, dump_and_stop_debug_mode, core);
		return ret;
	}

	state = atomic_inc_return(&core->pm.pwr_ref);

	MFC_TRACE_CORE("-- Power on: ref(%d)\n", state);
	mfc_core_debug(3, "[PLUGIN] -- power on: ref(%d)\n", state);

	mfc_core_debug_leave();

	return ret;
}

void mfc_plugin_pm_power_off(struct mfc_core *core)
{
	int ret = 0;
	int state;

	mfc_core_debug_enter();

	state = atomic_read(&core->pm.pwr_ref);

	MFC_TRACE_CORE("++ Power off: ref(%d)\n", state);
	mfc_core_debug(3, "[PLUGIN] ++ power off: ref(%d)\n", state);
	ret = pm_runtime_put(core->pm.device);
	if (ret < 0) {
		mfc_core_err("Failed to put power: ret(%d)\n", ret);
		call_dop(core, dump_and_stop_debug_mode, core);
	}

	state = atomic_dec_return(&core->pm.pwr_ref);

	MFC_TRACE_CORE("-- Power off: ref(%d)\n", state);
	mfc_core_debug(3, "[PLUGIN] -- power off: ref(%d)\n", state);

	mfc_core_debug_leave();
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
