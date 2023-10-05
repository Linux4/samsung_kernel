/*
 * drivers/media/platform/exynos/mfc/mfc_core_sched_prio.c
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

#include "base/mfc_sched.h"
#include "base/mfc_common.h"
#include "base/mfc_qos.h"
#include "base/mfc_utils.h"
#include "base/mfc_rate_calculate.h"

static inline void __mfc_print_workbits(struct mfc_core *core, int prio, int num)
{
	int i;

	mfc_core_debug(4, "[PRIO][c:%d] prio %d\n", num, prio);
	for (i = 0; i < core->total_num_prio; i++)
		mfc_core_debug(4, "[PRIO] MFC-%d P[%d] bits %08lx\n",
				core->id, i, core->prio_work_bits[i]);
}

static inline void __mfc_clear_all_prio_bits(struct mfc_core *core)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&core->prio_work_lock, flags);
	for (i = 0; i < core->total_num_prio; i++)
		core->prio_work_bits[i] = 0;
	spin_unlock_irqrestore(&core->prio_work_lock, flags);
}

static int __mfc_ctx_ready_set_bit_prio(struct mfc_core_ctx *core_ctx, bool set)

{
	struct mfc_core *core = core_ctx->core;
	struct mfc_ctx *ctx = core_ctx->ctx;
	unsigned long flags;
	int p, is_ready;

	mfc_qos_update_boosting(core, core_ctx);

	spin_lock_irqsave(&core->prio_work_lock, flags);

	p = mfc_get_prio(core, ctx->rt, ctx->prio);
	is_ready = mfc_ctx_ready_set_bit_raw(core_ctx, &core->prio_work_bits[p], set);
	__mfc_print_workbits(core, p, core_ctx->num);

	spin_unlock_irqrestore(&core->prio_work_lock, flags);

	return is_ready;
}

static void mfc_create_work_prio(struct mfc_core *core)
{
	int num_prio = core->dev->pdata->pbs_num_prio;

	spin_lock_init(&core->prio_work_lock);

	core->sched_type = MFC_SCHED_PRIO;
	core->num_prio = num_prio ? num_prio : 1;
	core->total_num_prio = core->num_prio * 2 + 2;

	__mfc_clear_all_prio_bits(core);
}

static void mfc_init_work_prio(struct mfc_core *core)
{
	core->sched_type = MFC_SCHED_PRIO;
	__mfc_clear_all_prio_bits(core);
	mfc_core_debug(2, "[SCHED][PRIO] Scheduler type is PBS\n");
}

static void mfc_clear_all_work_prio(struct mfc_core *core)
{
	__mfc_clear_all_prio_bits(core);
}

static int mfc_is_work_prio(struct mfc_core *core)
{
	unsigned long flags;
	int i, ret = 0;

	spin_lock_irqsave(&core->prio_work_lock, flags);
	for (i = 0; i < core->total_num_prio; i++) {
		if (core->prio_work_bits[i]) {
			ret = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&core->prio_work_lock, flags);
	return ret;
}

static void mfc_queue_work_prio(struct mfc_core *core)
{
	queue_work(core->butler_wq, &core->butler_work);
}

static void mfc_set_work_prio(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	unsigned long flags;
	int p;

	spin_lock_irqsave(&core->prio_work_lock, flags);

	p = mfc_get_prio(core, ctx->rt, ctx->prio);
	__set_bit(core_ctx->num, &core->prio_work_bits[p]);
	__mfc_print_workbits(core, p, core_ctx->num);

	spin_unlock_irqrestore(&core->prio_work_lock, flags);
}

static void mfc_clear_work_prio(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	unsigned long flags;
	int p;

	spin_lock_irqsave(&core->prio_work_lock, flags);

	p = mfc_get_prio(core, ctx->rt, ctx->prio);
	__clear_bit(core_ctx->num, &core->prio_work_bits[p]);
	__mfc_print_workbits(core, p, core_ctx->num);

	spin_unlock_irqrestore(&core->prio_work_lock, flags);
}

static int mfc_enqueue_work_prio(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	return __mfc_ctx_ready_set_bit_prio(core_ctx, true);
}

static int mfc_enqueue_otf_work_prio(struct mfc_core *core, struct mfc_core_ctx *core_ctx, bool flag)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	unsigned long flags;
	int p, ret;

	spin_lock_irqsave(&core->prio_work_lock, flags);

	p = mfc_get_prio(core, ctx->rt, ctx->prio);
	ret = mfc_core_otf_ctx_ready_set_bit_raw(core_ctx, &core->prio_work_bits[p], flag);
	__mfc_print_workbits(core, p, core_ctx->num);

	spin_unlock_irqrestore(&core->prio_work_lock, flags);

	return ret;
}

static int mfc_dequeue_work_prio(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	return __mfc_ctx_ready_set_bit_prio(core_ctx, false);
}

static void mfc_yield_work_prio(struct mfc_core *core, struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	unsigned long flags;
	int p;

	spin_lock_irqsave(&core->prio_work_lock, flags);

	p = mfc_get_prio(core, ctx->rt, ctx->prio);
	__clear_bit(core_ctx->num, &core->prio_work_bits[p]);
	__mfc_print_workbits(core, p, core_ctx->num);

	spin_unlock_irqrestore(&core->prio_work_lock, flags);

	mfc_core_try_run(core);
}

static unsigned long __mfc_check_lower_prio_inst(struct mfc_core *core, int prio)
{
	unsigned long sum = 0;
	int i;

	for (i = prio + 1; i < core->total_num_prio; i++)
		sum |= core->prio_work_bits[i];

	return sum;
}

static void __mfc_update_max_runtime(struct mfc_core *core, unsigned long bits)
{
	struct mfc_core_ctx *core_ctx;
	int max_runtime = 0, runtime;
	int i, num;

	if (core->dev->debugfs.sched_perf_disable)
		return;

	for (i = 0; i < MFC_NUM_CONTEXTS; i++) {
		if (test_bit(i, &bits)) {
			core_ctx = core->core_ctx[i];
			if (!core_ctx)
				continue;
			/* if runtime is 0, use default 30fps (33msec) */
			runtime = core_ctx->avg_runtime ? core_ctx->avg_runtime :
				MFC_DEFAULT_RUNTIME;
			if (runtime > max_runtime) {
				max_runtime = runtime;
				num = i;
			}
		}
	}

	if (max_runtime)
		mfc_core_debug(2, "[PRIO][c:%d] max runtime is %lld usec\n", num, max_runtime);

	core->max_runtime = max_runtime;
}

static int __mfc_core_get_new_ctx_prio(struct mfc_core *core, int prio, int *highest,
					bool predict)
{
	struct mfc_core_ctx *core_ctx;
	unsigned long bits = 0;
	int new_ctx_index;
	int i, perf = 0, skip_curr;

	new_ctx_index = (core->last_core_ctx[prio] + 1) % MFC_NUM_CONTEXTS;

	for (i = 0; i < MFC_NUM_CONTEXTS; i++) {
		if (test_bit(new_ctx_index, &core->prio_work_bits[prio])) {
			if (*highest < 0)
				*highest = new_ctx_index;

			/* If predict, do not get curr ctx */
			skip_curr = 0;
			if (predict && (new_ctx_index == core->curr_core_ctx)) {
				mfc_core_debug(2, "[PRIO][PREDICT](%d) skip curr ctx %d\n",
						prio, new_ctx_index);
				skip_curr = 1;
			}

			/* If the performance is insufficient, run this */
			core_ctx = core->core_ctx[new_ctx_index];
			perf = mfc_rate_check_perf_ctx(core_ctx->ctx, core->max_runtime);
			if (!perf && !skip_curr) {
				mfc_core_debug(2, "[PRIO]%s(%d) perf insufficient: %d\n",
						(predict ? "[PREDICT]" : ""),
						prio, new_ctx_index);
				return new_ctx_index;
			}

			/* If the perf of all instances is satisfied, run the highest */
			bits |= (1 << new_ctx_index);
			if (((core->prio_work_bits[prio] & ~(bits)) == 0) &&
					!__mfc_check_lower_prio_inst(core, prio)) {
				mfc_core_debug(2, "[PRIO]%s(%d) the highest priority %d\n",
						(predict ? "[PREDICT]" : ""),
						prio, *highest);
				return *highest;
			}
		}
		/* If there is no ready context in prio, return */
		if ((core->prio_work_bits[prio] & ~(bits)) == 0)
			return -1;

		new_ctx_index = (new_ctx_index + 1) % MFC_NUM_CONTEXTS;
	}

	return -1;
}

/* This should be called with prio_work_lock */
static int __mfc_core_get_new_ctx(struct mfc_core *core, bool predict)
{
	struct mfc_dev *dev = core->dev;
	int new_ctx_index = 0, highest = -1;
	int i;

	for (i = 0; i < core->total_num_prio; i++) {
		if (!core->prio_work_bits[i])
			continue;

		mfc_core_debug(2, "[PRIO]%s(%d) Previous context: %d (bits %08lx)\n",
				(predict ? "[PREDICT]" : ""),
				i, core->last_core_ctx[i], core->prio_work_bits[i]);

		if (dev->otf_inst_bits && (dev->otf_inst_bits & core->prio_work_bits[i])) {
			new_ctx_index = __ffs(dev->otf_inst_bits);
			mfc_core_debug(2, "[PRIO]%s(%d) OTF %d\n",
					(predict ? "[PREDICT]" : ""), i, new_ctx_index);
			return new_ctx_index;
		}

		new_ctx_index = __mfc_core_get_new_ctx_prio(core, i, &highest, predict);
		if (new_ctx_index >= 0) {
			mfc_core_debug(2, "[PRIO]%s(%d) get new_ctx %d\n",
					(predict ? "[PREDICT]" : ""), i, new_ctx_index);
			return new_ctx_index;
		}
	}

	return -1;
}

static int mfc_pick_next_work_prio(struct mfc_core *core)
{
	unsigned long flags, work_bits, hweight;
	int new_ctx_index = -1;

	if (!mfc_is_work_prio(core)) {
		mfc_core_debug(2, "[PRIO] No ctx to run\n");
		return -EAGAIN;
	}

	if (core->preempt_core_ctx > MFC_NO_INSTANCE_SET) {
		new_ctx_index = core->preempt_core_ctx;
		mfc_core_debug(2, "[PRIO] preempt_core_ctx %d\n", new_ctx_index);
		return new_ctx_index;
	}

	spin_lock_irqsave(&core->prio_work_lock, flags);

	work_bits = __mfc_check_lower_prio_inst(core, -1);
	mfc_core_debug(2, "[PRIO] all work_bits %#lx\n", work_bits);

	/* if single instance is ready, run it */
	hweight = hweight64(work_bits);
	if (hweight == 1) {
		new_ctx_index = __ffs(work_bits);
		mfc_core_debug(2, "[PRIO] new_ctx_idx %d (single)\n", new_ctx_index);
		spin_unlock_irqrestore(&core->prio_work_lock, flags);
		return new_ctx_index;
	}

	__mfc_update_max_runtime(core, work_bits);

	/* if there is predicted next ctx, run it */
	if ((core->next_ctx_idx >= 0) && test_bit(core->next_ctx_idx, &work_bits)) {
		new_ctx_index = core->next_ctx_idx;
		mfc_core_debug(2, "[PRIO] new_ctx_idx %d (predict)\n", new_ctx_index);
		spin_unlock_irqrestore(&core->prio_work_lock, flags);
		return new_ctx_index;
	}

	new_ctx_index = __mfc_core_get_new_ctx(core, 0);
	mfc_core_debug(2, "[PRIO] new_ctx_idx %d\n", new_ctx_index);

	spin_unlock_irqrestore(&core->prio_work_lock, flags);

	return new_ctx_index;
}

static int mfc_get_next_work_prio(struct mfc_core *core)
{
	unsigned long flags, work_bits, hweight;
	int new_ctx_index;

	/*
	 * Predict is required for DRM <-> Normal switching.
	 * So it is not predicted when there is no DRM inst or only DRM inst.
	 */
	if (!core->dev->num_drm_inst ||
			(core->dev->num_inst == core->dev->num_drm_inst))
		return -1;

	spin_lock_irqsave(&core->prio_work_lock, flags);

	work_bits = __mfc_check_lower_prio_inst(core, -1);
	hweight = hweight64(work_bits);
	/* Nothing to predict because only one instance is ready */
	if (hweight <= 1) {
		spin_unlock_irqrestore(&core->prio_work_lock, flags);
		return -1;
	}

	new_ctx_index = __mfc_core_get_new_ctx(core, 1);
	core->next_ctx_idx = new_ctx_index;
	mfc_core_debug(2, "[PRIO][PREDICT] new_ctx_idx %d\n", new_ctx_index);

	spin_unlock_irqrestore(&core->prio_work_lock, flags);

	return new_ctx_index;
}

static int mfc_change_prio_work_prio(struct mfc_core *core, struct mfc_ctx *ctx, int rt, int prio)
{
	unsigned long flags;
	int cur_p, new_p;

	spin_lock_irqsave(&core->prio_work_lock, flags);

	if (prio > core->num_prio)
		prio = core->num_prio;

	/* Check current priority's work_bit */
	cur_p = mfc_get_prio(core, ctx->rt, ctx->prio);
	new_p = mfc_get_prio(core, rt, prio);
	if (cur_p == new_p) {
		ctx->prio = prio;
		ctx->rt = rt;
		spin_unlock_irqrestore(&core->prio_work_lock, flags);
		return 0;
	}

	mfc_core_debug(2, "[PRIO] cur_p %d -> new_p %d is changed (%d %d -> %d %d)\n",
			cur_p, new_p, ctx->rt, ctx->prio, rt, prio);

	if (test_bit(ctx->num, &core->prio_work_bits[cur_p])) {
		__clear_bit(ctx->num, &core->prio_work_bits[cur_p]);
		mfc_core_debug(2, "[PRIO][c:%d] prio change from %d (bits %08lx)\n",
				ctx->num, cur_p, core->prio_work_bits[cur_p]);

		/* Update current priority's work_bit */
		__set_bit(ctx->num, &core->prio_work_bits[new_p]);
		mfc_core_debug(2, "[PRIO][c:%d] prio change to %d (bits %08lx)\n",
				ctx->num, new_p, core->prio_work_bits[new_p]);
	}

	/* These must be updated within the spin_lock for synchronization. */
	ctx->prio = prio;
	ctx->rt = rt;

	/* In the NALQ (linked list) mode, if the priority is changed it should be stopped */
	if (core->nal_q_handle && core->nal_q_handle->nal_q_ll &&
			(core->nal_q_handle->nal_q_state == NAL_Q_STATE_STARTED)) {
		core->nal_q_stop_cause |= (1 << NALQ_EXCEPTION_PRIO_CHANGE);
		mfc_ctx_info("[NALQ][PRIO] nal_q_exception is set (priority change)\n");
		core->nal_q_handle->nal_q_exception = 1;
	}

	spin_unlock_irqrestore(&core->prio_work_lock, flags);

	return 0;
}

struct mfc_sched_class mfc_sched_prio = {
	.create_work		= mfc_create_work_prio,
	.init_work		= mfc_init_work_prio,
	.clear_all_work		= mfc_clear_all_work_prio,
	.queue_work		= mfc_queue_work_prio,
	.is_work		= mfc_is_work_prio,
	.pick_next_work		= mfc_pick_next_work_prio,
	.get_next_work		= mfc_get_next_work_prio,
	.set_work		= mfc_set_work_prio,
	.clear_work		= mfc_clear_work_prio,
	.enqueue_work		= mfc_enqueue_work_prio,
	.enqueue_otf_work	= mfc_enqueue_otf_work_prio,
	.dequeue_work		= mfc_dequeue_work_prio,
	.yield_work		= mfc_yield_work_prio,
	.change_prio_work	= mfc_change_prio_work_prio,
};
