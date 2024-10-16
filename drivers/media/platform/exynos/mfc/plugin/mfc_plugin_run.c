/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_run.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_plugin_run.h"

#include "mfc_plugin_perf_measure.h"
#include "mfc_plugin_reg_api.h"

#include "../base/mfc_queue.h"
#include "../base/mfc_utils.h"
#include "../base/mfc_regs.h"

int mfc_plugin_run_frame(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_dec *dec = ctx->dec_priv;
	struct mfc_buf *mfc_buf;
	int index;

	mfc_buf = mfc_get_buf(ctx, &ctx->plugin_buf_queue, MFC_BUF_NO_TOUCH_USED);
	if (!mfc_buf) {
		mfc_err("[PLUGIN] there is no dst buffer\n");
		mfc_print_dpb_queue(core->core_ctx[ctx->num], dec);
		return -EINVAL;
	}
	index = mfc_buf->vb.vb2_buf.index;

	/* reset */
	mfc_fg_reset();

	/* resolution */
	mfc_fg_set_geometry(core, ctx);

	/* SEI film grain info */
	mfc_fg_set_sei_info(core, ctx, index);

	/* RDMA, WDMA */
	mfc_fg_set_rdma(core, ctx, mfc_buf);
	mfc_fg_set_wdma(core, ctx, mfc_buf);

	/* SFR dump */
	if (core->dev->debugfs.sfr_dump & MFC_DUMP_FILMGRAIN)
		call_dop(core, dump_regs, core);
	if ((core->dev->debugfs.sfr_dump & MFC_DUMP_ALL_INFO) && !core_ctx->check_dump) {
		call_dop(core, dump_and_stop_debug_mode, core);
		core_ctx->check_dump = 1;
	}

	/* START */
	mfc_fg_picture_start();

	mfc_plugin_perf_measure_on(core);

	mfc_debug(1, "Issue the command: start\n");
	MFC_TRACE_CORE_CTX(">> CMD : start\n");

	return 0;
}
