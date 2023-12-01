/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_hwlock.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_plugin_hwlock.h"
#include "mfc_plugin_run.h"
#include "mfc_plugin_sync.h"
#include "mfc_plugin_pm.h"

#include "../base/mfc_queue.h"
#include "../base/mfc_utils.h"

static inline void __mfc_plugin_print_hwlock(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;

	mfc_debug(3, "[c:%d] hwlock.bits = 0x%lx, wl_count = %d, transfer_owner = %d\n",
			core_ctx->num, core->hwlock.bits,
			core->hwlock.wl_count, core->hwlock.transfer_owner);
}

void mfc_plugin_init_hwlock(struct mfc_core *core)
{
	unsigned long flags;

	spin_lock_init(&core->hwlock.lock);

	spin_lock_irqsave(&core->hwlock.lock, flags);

	INIT_LIST_HEAD(&core->hwlock.waiting_list);
	core->hwlock.wl_count = 0;
	core->hwlock.bits = 0;
	core->hwlock.transfer_owner = 0;

	spin_unlock_irqrestore(&core->hwlock.lock, flags);
}

static void __mfc_plugin_remove_listable_wq_ctx(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_listable_wq *listable_wq;
	unsigned long flags;

	spin_lock_irqsave(&core->hwlock.lock, flags);

	list_for_each_entry(listable_wq, &core->hwlock.waiting_list, list) {
		if (!listable_wq->core_ctx)
			continue;

		if (listable_wq->core_ctx->num == core_ctx->num) {
			mfc_debug(2, "Found ctx and will delete it (%d)!\n", core_ctx->num);

			list_del(&listable_wq->list);
			core->hwlock.wl_count--;
			break;
		}
	}

	spin_unlock_irqrestore(&core->hwlock.lock, flags);
}

int mfc_plugin_get_hwlock(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	unsigned long flags;
	int ret;

	mutex_lock(&core_ctx->hwlock_wq.wait_mutex);

	spin_lock_irqsave(&core->hwlock.lock, flags);

	if (core->shutdown) {
		mfc_info("Couldn't lock HW. Shutdown was called\n");
		spin_unlock_irqrestore(&core->hwlock.lock, flags);
		mutex_unlock(&core_ctx->hwlock_wq.wait_mutex);
		return -EINVAL;
	}

	if (core->hwlock.bits != 0) {
		list_add_tail(&core_ctx->hwlock_wq.list, &core->hwlock.waiting_list);
		core->hwlock.wl_count++;

		spin_unlock_irqrestore(&core->hwlock.lock, flags);

		mfc_debug(2, "core_ctx[%d] Waiting for hwlock to be released\n",
				core_ctx->num);

		ret = wait_event_timeout(core_ctx->hwlock_wq.wait_queue,
				((core->hwlock.transfer_owner == 1) &&
				 (test_bit(core_ctx->num, &core->hwlock.bits))),
				msecs_to_jiffies(MFC_HWLOCK_TIMEOUT));

		core->hwlock.transfer_owner = 0;
		__mfc_plugin_remove_listable_wq_ctx(core_ctx);
		if (ret == 0) {
			mfc_err("Woken up but timed out\n");
			__mfc_plugin_print_hwlock(core_ctx);
			mutex_unlock(&core_ctx->hwlock_wq.wait_mutex);
			return -EIO;
		} else {
			mfc_debug(2, "Woken up and got hwlock\n");
			mutex_unlock(&core_ctx->hwlock_wq.wait_mutex);
		}
	} else {
		core->hwlock.bits = 0;
		set_bit(core_ctx->num, &core->hwlock.bits);

		__mfc_plugin_print_hwlock(core_ctx);
		spin_unlock_irqrestore(&core->hwlock.lock, flags);
		mutex_unlock(&core_ctx->hwlock_wq.wait_mutex);
	}

	return 0;
}

void __mfc_plugin_transfer_hwlock(struct mfc_core *core, int ctx_index)
{
	core->hwlock.bits = 0;
	set_bit(ctx_index, &core->hwlock.bits);
}

void mfc_plugin_release_hwlock(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_listable_wq *listable_wq;
	unsigned long flags;

	spin_lock_irqsave(&core->hwlock.lock, flags);

	clear_bit(core_ctx->num, &core->hwlock.bits);

	if (core->shutdown) {
		mfc_debug(2, "Couldn't wakeup module. Shutdown was called\n");
		spin_unlock_irqrestore(&core->hwlock.lock, flags);
		return;
	}

	if (list_empty(&core->hwlock.waiting_list)) {
		mfc_debug(2, "No waiting module\n");
	} else {
		mfc_debug(2, "There is a waiting module\n");
		listable_wq = list_entry(core->hwlock.waiting_list.next,
				struct mfc_listable_wq, list);
		list_del(&listable_wq->list);
		core->hwlock.wl_count--;

		mfc_debug(2, "Waking up another ctx %d\n", listable_wq->core_ctx->num);
		set_bit(listable_wq->core_ctx->num, &core->hwlock.bits);

		core->hwlock.transfer_owner = 1;

		wake_up(&listable_wq->wait_queue);
	}

	__mfc_plugin_print_hwlock(core_ctx);
	spin_unlock_irqrestore(&core->hwlock.lock, flags);
}

static int __mfc_plugin_get_new_ctx_protected(struct mfc_core *core)
{
	struct mfc_dev *dev = core->dev;
	struct mfc_ctx *new_ctx;
	int index;

	if (core->shutdown) {
		mfc_core_info("Couldn't lock HW. Shutdown was called\n");
		return -EINVAL;
	}

	if (core->sleep) {
		mfc_core_info("Couldn't lock HW. Sleep was called\n");
		return -EINVAL;
	}

	/* Check whether hardware is not running */
	if (core->hwlock.bits != 0) {
		/* This is perfectly ok, the scheduled ctx should wait */
		mfc_core_debug(2, "Couldn't lock HW\n");
		return -1;
	}

	/* Choose the context to run */
	index = core->sched->pick_next_work(core);
	if (index < 0) {
		/*
		 * This is perfectly ok, the scheduled ctx should wait
		 * No contexts to run
		 */
		mfc_core_debug(2, "No ctx is scheduled to be run\n");
		return -1;
	}

	new_ctx = dev->ctx[index];
	if (!new_ctx) {
		mfc_core_err("no mfc context to run\n");
		return -1;
	}

	set_bit(new_ctx->num, &core->hwlock.bits);

	return index;
}

static void __mfc_plugin_handle_buffer(struct mfc_ctx *ctx, struct mfc_buf *mfc_buf)
{
	struct mfc_dec *dec = ctx->dec_priv;

	mutex_lock(&dec->dpb_mutex);
	/* FW can use internal DPB(SRC) */
	dec->dynamic_set |= (1UL << mfc_buf->dpb_index);
	/* clear plugin used internal DPB(SRC) */
	dec->dpb_table_used &= ~(1UL << mfc_buf->dpb_index);
	/* clear plugin used DST buffer */
	clear_bit(mfc_buf->vb.vb2_buf.index, &dec->plugin_used);
	/* clear queued DST buffer */
	clear_bit(mfc_buf->vb.vb2_buf.index, &dec->queued_dpb);

	mfc_ctx_debug(2, "[PLUGIN] SRC(FW available: %#lx, used: %#lx), DST(used: %#lx, queued: %#lx)\n",
			dec->dynamic_set, dec->dpb_table_used,
			dec->plugin_used, dec->queued_dpb);
	mutex_unlock(&dec->dpb_mutex);

	mfc_ctx_debug(2, "[PLUGIN] dst index [%d] fd: %d is buffer done\n",
			mfc_buf->vb.vb2_buf.index,
			mfc_buf->vb.vb2_buf.planes[0].m.fd);
	vb2_buffer_done(&mfc_buf->vb.vb2_buf, (mfc_buf->vb.flags & V4L2_BUF_FLAG_ERROR) ?
			VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE);
}

static int __mfc_plugin_fg_just_run(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_dec *dec = ctx->dec_priv;
	struct mfc_buf *mfc_buf;
	int ret = 0;

	mfc_core_clean_dev_int_flags(core);
	core->curr_core_ctx = core_ctx->num;

	mfc_buf = mfc_get_buf(ctx, &ctx->plugin_buf_queue, MFC_BUF_NO_TOUCH_USED);
	if (!mfc_buf) {
		mfc_err("[PLUGIN] there is no dst buffer\n");
		mfc_print_dpb_queue(core->core_ctx[ctx->num], dec);
		return -EINVAL;
	}

	/* Last frame only handles the buffer without H/W operation */
	if (mfc_buf->dpb_index == DEFAULT_TAG) {
		mfc_info("[PLUGIN] empty EOS\n");
		mfc_buf = mfc_find_del_buf(ctx, &ctx->plugin_buf_queue, mfc_buf->addr[0][0]);
		if (mfc_buf) {
			__mfc_plugin_handle_buffer(ctx, mfc_buf);
			mfc_plugin_release_hwlock(core_ctx);
			core->sched->clear_work(core, core_ctx);
			return 0;
		}
	}

	if (ctx->is_drm)
		mfc_plugin_protection_on(core);

	mfc_plugin_pm_clock_on(core);

	ret = mfc_plugin_run_frame(core, core_ctx);
	if (ret) {
		mfc_err("[PLUGIN] failed to run frame\n");
		mfc_plugin_pm_clock_off(core);
		if (ctx->is_drm)
			mfc_plugin_protection_off(core);
	}

	return ret;
}

static int __mfc_plugin_sw_just_run(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_dec *dec = ctx->dec_priv;
	struct mfc_raw_info *raw;
	struct mfc_buf *mfc_buf;
	void *src_addr, *dst_addr;
	int i, offset = 0, size;

	mfc_debug_enter();

	mfc_core_clean_dev_int_flags(core);
	core->curr_core_ctx = core_ctx->num;

	mfc_buf = mfc_get_del_buf(ctx, &ctx->plugin_buf_queue, MFC_BUF_RESET_USED);
	if (!mfc_buf) {
		mfc_err("[PLUGIN] there is no dst buffer\n");
		mfc_print_dpb_queue(core->core_ctx[ctx->num], dec);
		return -EINVAL;
	}

	if (mfc_buf->dpb_index == DEFAULT_TAG) {
		mfc_debug(2, "[PLUGIN] empty EOS\n");
		goto buffer_done;
	}

	raw = &ctx->internal_raw_buf;
	if (IS_SINGLE_FD(ctx, ctx->internal_fmt)) {
		src_addr = dec->internal_dpb[mfc_buf->dpb_index].vaddr;
		dst_addr = mfc_buf->vir_addr[0];
		memcpy(dst_addr, src_addr, raw->total_plane_size);
		mfc_debug(2, "[PLUGIN] single-fd src index %d, addr %#llx, dst index %d, addr %#llx, size %d\n",
				mfc_buf->dpb_index, src_addr,
				mfc_buf->vb.vb2_buf.index, dst_addr, raw->total_plane_size);
	} else {
		for (i = 0; i < raw->num_planes; i++) {
			src_addr = dec->internal_dpb[mfc_buf->dpb_index].vaddr + offset;
			dst_addr = mfc_buf->vir_addr[i];
			size = raw->plane_size[i] + raw->plane_size_2bits[i];
			offset += size;
			memcpy(dst_addr, src_addr, size);
			mfc_debug(2, "[PLUGIN] plane[%d] src index %d, addr %#llx, dst index %d, addr %#llx, size %d\n",
					i, mfc_buf->dpb_index, src_addr,
					mfc_buf->vb.vb2_buf.index, dst_addr, size);
		}
	}

buffer_done:
	__mfc_plugin_handle_buffer(ctx, mfc_buf);

	mfc_plugin_hwlock_handler(core_ctx);

	mfc_debug_leave();

	return 0;
}

int mfc_plugin_just_run(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	int ret;

	if (ctx->plugin_type == MFC_PLUGIN_FILM_GRAIN) {
		ret = __mfc_plugin_fg_just_run(core_ctx);
	} else if (ctx->plugin_type == MFC_PLUGIN_SW_MEMCPY) {
		ret = __mfc_plugin_sw_just_run(core_ctx);
	} else {
		ret = -EINVAL;
		mfc_err("[PLUGIN] invalid plugin type %d\n", ctx->plugin_type);
	}

	return ret;
}

void mfc_plugin_try_run(struct mfc_core *core)
{
	struct mfc_ctx *ctx;
	struct mfc_core_ctx *core_ctx;
	unsigned long flags;
	int new_ctx_index, ret;

	mfc_core_debug_enter();

	spin_lock_irqsave(&core->hwlock.lock, flags);
	new_ctx_index = __mfc_plugin_get_new_ctx_protected(core);
	if (new_ctx_index < 0) {
		mfc_core_debug(2, "[PLUGIN] Failed to get new context to run\n");
		spin_unlock_irqrestore(&core->hwlock.lock, flags);
		return;
	}
	spin_unlock_irqrestore(&core->hwlock.lock, flags);

	core_ctx = core->core_ctx[new_ctx_index];
	if (!core_ctx) {
		mfc_core_err("[PLUGIN] There is no core_ctx\n");
		return;
	}

	ctx = core_ctx->ctx;
	if (!ctx) {
		mfc_core_err("[PLUGIN] There is no ctx\n");
		return;
	}

	if (core_ctx->state == MFCINST_INIT)
		mfc_change_state(core_ctx, MFCINST_RUNNING);

	ret = mfc_plugin_just_run(core_ctx);
	if (ret < 0) {
		mfc_err("[PLUGIN] failed to just run\n");
		mfc_plugin_release_hwlock(core_ctx);
	}

	mfc_core_debug_leave();
}

void mfc_plugin_hwlock_handler(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_ctx *ctx = core_ctx->ctx;
	unsigned long flags;
	int new_ctx_index, ret;

	if (ctx->plugin_type == MFC_PLUGIN_FILM_GRAIN)
		mfc_plugin_pm_clock_off(core);

	if (ctx->is_drm)
		mfc_plugin_protection_off(core);

	spin_lock_irqsave(&core->hwlock.lock, flags);

	new_ctx_index = core->sched->pick_next_work(core);

	/* should be release hwlock */
	if (!list_empty(&core->hwlock.waiting_list) || (new_ctx_index < 0) ||
			(ctx->plugin_type == MFC_PLUGIN_SW_MEMCPY)) {
		mfc_debug(2, "There is a waiting module or should be release hwlock\n");
		spin_unlock_irqrestore(&core->hwlock.lock, flags);

		mfc_wake_up_plugin(core, MFC_PLUGIN_FRAME_DONE_RET, 0);
		mfc_plugin_release_hwlock(core_ctx);

		core->sched->clear_work(core, core_ctx);

		core->sched->enqueue_work(core, core_ctx);
		if (core->sched->is_work(core))
			core->sched->queue_work(core);
		return;
	}

	/* transfer hwlock */
	if (new_ctx_index >= 0) {
		mfc_debug(2, "[PLUGIN] transfer hwlock\n");
		__mfc_plugin_transfer_hwlock(core, new_ctx_index);
		spin_unlock_irqrestore(&core->hwlock.lock, flags);

		ret = mfc_plugin_just_run(core->core_ctx[new_ctx_index]);
		if (ret) {
			mfc_debug(2, "[PLUGIN] failed to just_run\n");
			mfc_plugin_release_hwlock(core_ctx);
			core->sched->clear_work(core, core_ctx);

			core->sched->enqueue_work(core, core_ctx);
			if (core->sched->is_work(core))
				core->sched->queue_work(core);
		}
	}
}
