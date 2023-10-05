/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_meerkat.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_plugin_meerkat.h"
#include "mfc_plugin_pm.h"

#include "../base/mfc_regs.h"
#include "../base/mfc_queue.h"
#include "../base/mfc_utils.h"

#define FG_SFR_AREA_COUNT	4

static void __mfc_plugin_dump_regs(struct mfc_core *core)
{
	struct mfc_core *core_mfd = core->dev->core[MFC_OP_CORE_FIXED_1];
	int i, clk_enable = 0;
	int addr[FG_SFR_AREA_COUNT][2] = {
		{ REG_FG_PICTURE_START, 0xD4 },
		{ REG_FG_RDMA_ENABLE, 0x1DC },
		{ REG_FG_WDMA_LUM_ENABLE, 0x1DC },
		{ REG_FG_WDMA_CHR_ENABLE, 0x1DC },
	};

	dev_err(core->device, "-----------dumping Filmgrain registers\n");

	if (!mfc_core_get_pwr_ref_cnt(core_mfd)) {
		dev_err(core->device, "MFD Power(%d) is not enabled\n",
				mfc_core_get_pwr_ref_cnt(core_mfd));
		return;
	}

	if (!mfc_core_get_clk_ref_cnt(core)) {
		dev_info(core->device, "Filmgrain clock(%d) is not enabled\n",
				mfc_core_get_clk_ref_cnt(core));
		mfc_plugin_pm_clock_on(core);
		clk_enable = 1;
	}

	for (i = 0; i < FG_SFR_AREA_COUNT; i++) {
		dev_err(core->device, "[%04X .. %04X]\n", addr[i][0], addr[i][0] + addr[i][1]);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_OFFSET, 32, 4,
				core->regs_base + addr[i][0], addr[i][1], false);
		dev_err(core->device, "...\n");
	}

	if (clk_enable)
		mfc_plugin_pm_clock_off(core);
}

static void __mfc_plugin_dump_info_without_regs(struct mfc_core *core)
{
	struct mfc_core *core_mfd = core->dev->core[MFC_OP_CORE_FIXED_1];
	struct mfc_core_ctx *core_ctx = core->core_ctx[core->curr_core_ctx];
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_dec *dec = ctx->dec_priv;

	dev_err(core->device, "-----------dumping Film grain info-----------\n");
	dev_err(core->device, "power(MFD):%d, clock:%d, num_inst:%d, num_drm_inst:%d\n",
			mfc_core_get_pwr_ref_cnt(core_mfd),
			mfc_core_get_clk_ref_cnt(core),
			core->num_inst, core->num_drm_inst);
	dev_err(core->device, "hwlock bits:%#lx, curr_ctx:%d (is_drm:%d), work_bits:%#lx\n",
			core->hwlock.bits, core->curr_core_ctx, ctx->is_drm,
			mfc_get_bits(&core->work_bits));

	mfc_print_dpb_queue(core_ctx, dec);
	/* TODO: print trace */
}

static void __mfc_plugin_dump_info(struct mfc_core *core)
{
	__mfc_plugin_dump_info_without_regs(core);
	__mfc_plugin_dump_regs(core);
}

static void __mfc_plugin_dump_info_and_stop_hw(struct mfc_core *core)
{
	dev_err(core->device, "** %s will stop!!!\n", core->name);

	__mfc_plugin_dump_info(core);

	if (core->dev->debugfs.sfr_dump & MFC_DUMP_ALL_INFO)
		return;

	dbg_snapshot_expire_watchdog();
	BUG();
}

static void __mfc_plugin_dump_info_and_stop_hw_debug(struct mfc_core *core)
{
	struct mfc_dev *dev = core->dev;

	if (!dev->pdata->debug_mode && !dev->debugfs.debug_mode_en &&
			!(dev->debugfs.sfr_dump & MFC_DUMP_ALL_INFO))
		return;

	__mfc_plugin_dump_info_and_stop_hw(core);
}

struct mfc_core_dump_ops mfc_plugin_dump_ops = {
	.dump_regs			= __mfc_plugin_dump_regs,
	.dump_info			= __mfc_plugin_dump_info,
	.dump_info_without_regs		= __mfc_plugin_dump_info_without_regs,
	.dump_and_stop_always		= __mfc_plugin_dump_info_and_stop_hw,
	.dump_and_stop_debug_mode	= __mfc_plugin_dump_info_and_stop_hw_debug,
};
