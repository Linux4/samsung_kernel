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

#include "base/mfc_queue.h"
#include "base/mfc_utils.h"
#include "base/mfc_regs.h"

int mfc_plugin_run_wait_for_ready_to_run(struct mfc_core *core,
			struct mfc_core_ctx *core_ctx, unsigned long *flags)
{
	unsigned long timeout;

	timeout = jiffies + msecs_to_jiffies(MFC_BW_TIMEOUT);

	while (!mfc_fg_is_shadow_avail(core, core_ctx)) {
		spin_unlock_irqrestore(&core->fg_q_handle->lock, *flags);

		if (time_after(jiffies, timeout)) {
			mfc_core_err("Timeout while waiting idle of fg\n");
			spin_lock_irqsave(&core->fg_q_handle->lock, *flags);
			return -EIO;
		}

		spin_lock_irqsave(&core->fg_q_handle->lock, *flags);
	}

	return 0;
}

int mfc_plugin_run_frame(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_dec *dec = ctx->dec_priv;
	struct mfc_buf *mfc_buf;
	int index;
	int idx_shadow = 0;
	int ret = 0;

	mfc_buf = mfc_get_buf_no_used(ctx, &ctx->plugin_buf_queue, MFC_BUF_SET_USED);
	if (!mfc_buf) {
		mfc_err("[PLUGIN] there is no dst buffer\n");
		mfc_print_dpb_queue_with_lock(core->core_ctx[ctx->num], dec);
		return -EINVAL;
	}
	index = mfc_buf->vb.vb2_buf.index;

	/* reset */
	if (core->fg_q_enable == false ||
	    ((core->dev->pdata->support_fg_shadow) &&
	     (core->fg_q_handle->queue_count == core->fg_q_handle->exe_count))) {
		ret = mfc_fg_reset(core);
		if (ret) {
			mfc_err("[PLUGIN] failed to reset fg\n");
			return ret;
		}

		if (core->dev->pdata->support_fg_shadow) {
			core->fg_q_handle->queue_count = 0;
			core->fg_q_handle->exe_count = 0;
		}
	}

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
		call_dop(core, dump_info, core);
		core_ctx->check_dump = 1;
	}

	mfc_fg_enable_irq(0x3);

	/* START */
	if (core->dev->pdata->support_fg_shadow) {
		idx_shadow = core->fg_q_handle->queue_count % MFC_FG_NUM_SHADOW_REGS;
		core->fg_q_handle->ctx_num_table[idx_shadow] = core_ctx->num;

		if (core->fg_q_handle->queue_count == 0)
			mfc_fg_run_cmd();

		if (core->fg_q_handle->queue_count == UINT_MAX)
			core->fg_q_handle->queue_count = 1;
		else
			core->fg_q_handle->queue_count++;
		mfc_fg_update_run_cnt(core->fg_q_handle->queue_count);

	} else {
		mfc_fg_picture_start();
	}

	mfc_plugin_perf_measure_on(core);

	if (core->dev->pdata->support_fg_shadow) {
		mfc_debug(1, "Issue the command: queue start(idx: %d, queue_cnt: %d)\n",
			  idx_shadow, core->fg_q_handle->queue_count);
		MFC_TRACE_CORE_CTX(">> CMD : queue start(idx: %d, queue_cnt: %d)\n",
				   idx_shadow, core->fg_q_handle->queue_count);
	} else {
		mfc_debug(1, "Issue the command: start\n");
		MFC_TRACE_CORE_CTX(">> CMD : start\n");
	}

	return 0;
}
