/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_sync.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_plugin_sync.h"

#include "../base/mfc_queue.h"

void mfc_wake_up_plugin(struct mfc_core *core, unsigned int reason, unsigned int err)
{
	core->int_condition = 1;
	core->int_reason = reason;
	core->int_err = err;
	wake_up(&core->cmd_wq);
}

int mfc_wait_for_done_plugin(struct mfc_core *core, int command)
{
	int ret;

	if (core->state == MFCCORE_ERROR) {
		mfc_core_info("[MSR] Couldn't run HW. It's Error state\n");
		return 0;
	}

	ret = wait_event_timeout(core->cmd_wq,
			core->int_condition && (core->int_reason == command),
			msecs_to_jiffies(MFC_INT_TIMEOUT));
	if (ret == 0) {
		mfc_core_err("Interrupt (core->int_reason:%d, command:%d) timed out\n",
							core->int_reason, command);
		call_dop(core, dump_and_stop_debug_mode, core);
		return 1;
	}

	mfc_core_debug(2, "Finished waiting (core->int_reason:%d, command: %d)\n",
			core->int_reason, command);
	return 0;
}

int mfc_plugin_get_new_ctx(struct mfc_core *core)
{
	unsigned long wflags;
	int new_ctx_index = 0;
	int cnt = 0;

	spin_lock_irqsave(&core->work_bits.lock, wflags);

	mfc_core_debug(2, "Previous context: %d (bits %08lx)\n",
			core->curr_core_ctx, core->work_bits.bits);

	new_ctx_index = (core->curr_core_ctx + 1) % MFC_NUM_CONTEXTS;
	while (!test_bit(new_ctx_index, &core->work_bits.bits)) {
		new_ctx_index = (new_ctx_index + 1) % MFC_NUM_CONTEXTS;
		cnt++;
		if (cnt > MFC_NUM_CONTEXTS) {
			/* No contexts to run */
			spin_unlock_irqrestore(&core->work_bits.lock, wflags);
			return -EAGAIN;
		}
	}

	spin_unlock_irqrestore(&core->work_bits.lock, wflags);
	return new_ctx_index;
}

static int __mfc_plugin_ready_set_bit(struct mfc_core_ctx *core_ctx,
		struct mfc_bits *data, bool set)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_ctx *ctx = core_ctx->ctx;
	unsigned long flags;
	int buf_queue_greater_than_0 = 0;
	int is_ready = 0;

	mfc_debug(1, "[PLUGIN][c:%d] buf = %d\n", ctx->num,
			mfc_get_queue_count(&ctx->buf_queue_lock, &ctx->plugin_buf_queue));

	/* If shutdown is called, do not try any cmd */
	if (core->shutdown)
		return 0;

	spin_lock_irqsave(&data->lock, flags);

	buf_queue_greater_than_0
		= mfc_is_queue_count_greater(&ctx->buf_queue_lock, &ctx->plugin_buf_queue, 0);

	if (((core_ctx->state == MFCINST_RUNNING) || (core_ctx->state == MFCINST_INIT)) &&
			buf_queue_greater_than_0)
		is_ready = 1;

	if ((is_ready == 1) && (set == true)) {
		/* if the ctx is ready and request set_bit, set the work_bit */
		__set_bit(ctx->num, &data->bits);
	} else if ((is_ready == 0) && (set == false)) {
		/* if the ctx is not ready and request clear_bit, clear the work_bit */
		__clear_bit(ctx->num, &data->bits);
	} else {
		if (set == true)
			mfc_debug(2, "ctx is not ready\n");
	}

	spin_unlock_irqrestore(&data->lock, flags);

	return is_ready;
}

int mfc_plugin_ready_set_bit(struct mfc_core_ctx *core_ctx, struct mfc_bits *data)
{
	return __mfc_plugin_ready_set_bit(core_ctx, data, true);
}

int mfc_plugin_ready_clear_bit(struct mfc_core_ctx *core_ctx, struct mfc_bits *data)
{
	return __mfc_plugin_ready_set_bit(core_ctx, data, false);
}
