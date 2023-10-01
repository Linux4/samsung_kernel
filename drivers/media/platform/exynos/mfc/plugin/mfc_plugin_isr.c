/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_isr.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_plugin_isr.h"

#include "mfc_plugin_hwlock.h"

#include "mfc_plugin_perf_measure.h"
#include "mfc_plugin_reg_api.h"

#include "../base/mfc_queue.h"
#include "../base/mfc_debug.h"

irqreturn_t mfc_plugin_irq(int irq, void *priv)
{
	struct mfc_core *core = priv;
	struct mfc_core_ctx *core_ctx;
	struct mfc_ctx *ctx;
	struct mfc_dec *dec;
	struct mfc_buf *mfc_buf = NULL;
	unsigned int err = 0;
	dma_addr_t wdma_y_addr;

	mfc_core_debug_enter();

	core_ctx = core->core_ctx[core->curr_core_ctx];
	if (!core_ctx) {
		mfc_core_err("no film grain core context to run\n");
		mfc_fg_clear_int();
		goto irq_end;
	}

	ctx = core_ctx->ctx;
	if (!ctx) {
		mfc_core_err("no film grain context to run\n");
		mfc_fg_clear_int();
		goto irq_end;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		mfc_fg_clear_int();
		goto irq_end;
	}

	err = mfc_fg_get_rdma_sbwc_err();
	mfc_debug(1, "[c:%d] Int reason: done (err: %d)\n", core->curr_core_ctx, err);
	MFC_TRACE_CORE_CTX("<< INT: done (err: %d)\n", err);

	mfc_plugin_perf_measure_off(core);

	/* SFR dump */
	if (core->dev->debugfs.sfr_dump & MFC_DUMP_FILMGRAIN)
		call_dop(core, dump_regs, core);

	/* Handle buffer */
	wdma_y_addr = mfc_fg_get_wdma_y_addr();
	mfc_buf = mfc_find_del_buf(ctx, &ctx->plugin_buf_queue, wdma_y_addr);
	if (!mfc_buf) {
		mfc_err("[PLUGIN] there is no frame done buffer %#llx\n", wdma_y_addr);
		mfc_print_dpb_queue(core->core_ctx[ctx->num], dec);
		goto irq_handle;
	}

	/* TODO: couldn't use mutex lock in ISR */
	/* FW can use internal DPB(SRC) */
	dec->dynamic_set |= (1UL << mfc_buf->dpb_index);
	/* clear plugin used internal DPB(SRC) */
	dec->dpb_table_used &= ~(1UL << mfc_buf->dpb_index);
	/* clear plugin used DST buffer */
	clear_bit(mfc_buf->vb.vb2_buf.index, &dec->plugin_used);
	/* clear queued DST buffer */
	clear_bit(mfc_buf->vb.vb2_buf.index, &dec->queued_dpb);

	mfc_debug(2, "[PLUGIN] SRC(FW available: %#lx, used: %#lx), DST(used: %#lx, queued: %#lx)\n",
			dec->dynamic_set, dec->dpb_table_used,
			dec->plugin_used, dec->queued_dpb);

	mfc_debug(2, "[PLUGIN] dst index [%d] fd: %d is buffer done\n",
			mfc_buf->vb.vb2_buf.index,
			mfc_buf->vb.vb2_buf.planes[0].m.fd);
	vb2_buffer_done(&mfc_buf->vb.vb2_buf, (mfc_buf->vb.flags & V4L2_BUF_FLAG_ERROR) ?
			VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE);

irq_handle:
	mfc_fg_clear_int();

	core->sched->dequeue_work(core, core_ctx);

	mfc_plugin_hwlock_handler(core_ctx);

irq_end:
	mfc_core_debug_leave();
	return IRQ_HANDLED;
}
