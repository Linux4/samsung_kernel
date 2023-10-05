/*
 * drivers/media/platform/exynos/mfc/mfc_core_sched_rr.c
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_core_hwlock.h"
#include "mfc_core_otf.h"
#include "mfc_core_sync.h"

#include "plugin/mfc_plugin_sync.h"

#include "base/mfc_common.h"
#include "base/mfc_utils.h"

static void mfc_create_work_rr(struct mfc_core *core)
{
	core->sched_type = MFC_SCHED_RR;
	mfc_create_bits(&core->work_bits);
}

static void mfc_init_work_rr(struct mfc_core *core)
{
	core->sched_type = MFC_SCHED_RR;
	mfc_clear_all_bits(&core->work_bits);
	mfc_core_debug(2, "[SCHED][RR] Scheduler type is RR\n");
}

static void mfc_clear_all_work_rr(struct mfc_core *core)
{
	mfc_clear_all_bits(&core->work_bits);
}

static int mfc_is_work_rr(struct mfc_core *core)
{
	return (!mfc_is_all_bits_cleared(&core->work_bits));
}

static void mfc_queue_work_rr(struct mfc_core *core)
{
	queue_work(core->butler_wq, &core->butler_work);
}

static void mfc_set_work_rr(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	mfc_set_bit(core_ctx->num, &core->work_bits);
}

static void mfc_clear_work_rr(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	mfc_clear_bit(core_ctx->num, &core->work_bits);
}

static int mfc_enqueue_work_rr(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	if (core_ctx->is_plugin)
		return mfc_plugin_ready_set_bit(core_ctx, &core->work_bits);
	else
		return mfc_ctx_ready_set_bit(core_ctx, &core->work_bits);
}

static int mfc_enqueue_otf_work_rr(struct mfc_core *core, struct mfc_core_ctx *core_ctx, bool flag)
{
	return mfc_core_otf_ctx_ready_set_bit(core_ctx, &core->work_bits, flag);
}

static int mfc_dequeue_work_rr(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	if (core_ctx->is_plugin)
		return mfc_plugin_ready_clear_bit(core_ctx, &core->work_bits);
	else
		return mfc_ctx_ready_clear_bit(core_ctx, &core->work_bits);
}

void __mfc_core_cleanup_work_bit_and_try_run(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;

	mfc_clear_bit(core_ctx->num, &core->work_bits);

	mfc_core_try_run(core);
}

static void mfc_yield_work_rr(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	__mfc_core_cleanup_work_bit_and_try_run(core_ctx);
}

static int __mfc_core_get_new_ctx(struct mfc_core *core)
{
	struct mfc_dev *dev = core->dev;
	unsigned long wflags;
	int new_ctx_index = 0;
	int cnt = 0;
	int i;

	spin_lock_irqsave(&core->work_bits.lock, wflags);

	mfc_core_debug(2, "Previous context: %d (bits %08lx)\n",
			core->curr_core_ctx, core->work_bits.bits);

	if (core->preempt_core_ctx > MFC_NO_INSTANCE_SET) {
		new_ctx_index = core->preempt_core_ctx;
		mfc_core_debug(2, "preempt_core_ctx is : %d\n", new_ctx_index);
	} else {
		for (i = 0; i < MFC_NUM_CONTEXTS; i++) {
			if (test_bit(i, &dev->otf_inst_bits)) {
				if (test_bit(i, &core->work_bits.bits)) {
					spin_unlock_irqrestore(&core->work_bits.lock, wflags);
					return i;
				}
				break;
			}
		}

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
	}

	spin_unlock_irqrestore(&core->work_bits.lock, wflags);
	return new_ctx_index;
}

static int mfc_pick_next_work_rr(struct mfc_core *core)
{
	if (core == core->dev->plugin)
		return mfc_plugin_get_new_ctx(core);
	else
		return __mfc_core_get_new_ctx(core);
}

static int __mfc_core_get_next_ctx(struct mfc_core *core)
{
	unsigned long wflags;
	int curr_ctx_index = core->curr_core_ctx;
	int next_ctx_index = (curr_ctx_index + 1) % MFC_NUM_CONTEXTS;

	spin_lock_irqsave(&core->work_bits.lock, wflags);

	mfc_core_debug(2, "Current context: %d (bits %08lx)\n",
			core->curr_core_ctx, core->work_bits.bits);

	while (!test_bit(next_ctx_index, &core->work_bits.bits)) {
		next_ctx_index = (next_ctx_index + 1) % MFC_NUM_CONTEXTS;
		if (next_ctx_index == curr_ctx_index) {
			/* No other context to run */
			spin_unlock_irqrestore(&core->work_bits.lock, wflags);
			return -EAGAIN;
		}
	}

	spin_unlock_irqrestore(&core->work_bits.lock, wflags);
	return next_ctx_index;
}


static int mfc_get_next_work_rr(struct mfc_core *core)
{
	return __mfc_core_get_next_ctx(core);
}

static int mfc_change_prio_work_rr(struct mfc_core *core, struct mfc_ctx *ctx, int rt, int prio)
{
	/* RR doesn't support priority change */
	return -EINVAL;
}

struct mfc_sched_class mfc_sched_rr = {
	.create_work		= mfc_create_work_rr,
	.init_work		= mfc_init_work_rr,
	.clear_all_work		= mfc_clear_all_work_rr,
	.queue_work		= mfc_queue_work_rr,
	.is_work		= mfc_is_work_rr,
	.pick_next_work		= mfc_pick_next_work_rr,
	.get_next_work		= mfc_get_next_work_rr,
	.set_work		= mfc_set_work_rr,
	.clear_work		= mfc_clear_work_rr,
	.enqueue_work		= mfc_enqueue_work_rr,
	.enqueue_otf_work	= mfc_enqueue_otf_work_rr,
	.dequeue_work		= mfc_dequeue_work_rr,
	.yield_work		= mfc_yield_work_rr,
	.change_prio_work	= mfc_change_prio_work_rr,
};
