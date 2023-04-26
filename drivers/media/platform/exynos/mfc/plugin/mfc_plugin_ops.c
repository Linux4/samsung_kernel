/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_ops.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_plugin_ops.h"

#include "mfc_plugin_hwlock.h"
#include "mfc_plugin_sync.h"
#include "mfc_plugin_pm.h"

#include "mfc_plugin_perf_measure.h"

#include "../base/mfc_queue.h"
#include "../base/mfc_utils.h"

int mfc_plugin_instance_init(struct mfc_core *core, struct mfc_ctx *ctx)
{
	struct mfc_core_ctx *core_ctx = NULL;
	int ret = 0;

	mfc_core_debug_enter();

	core->num_inst++;
	if (ctx->is_drm)
		core->num_drm_inst++;

	/* Allocate memory for core context */
	core_ctx = kzalloc(sizeof(*core_ctx), GFP_KERNEL);
	if (!core_ctx) {
		mfc_core_err("Not enough memory\n");
		ret = -ENOMEM;
		goto err_init_inst;
	}

	core_ctx->core = core;
	core_ctx->ctx = ctx;
	core_ctx->num = ctx->num;
	core_ctx->is_drm = ctx->is_drm;
	core->core_ctx[core_ctx->num] = core_ctx;
	core_ctx->is_plugin = 1;
	core->curr_core_ctx = ctx->num;

	mfc_plugin_init_listable_wq(core_ctx);
	core->sched->clear_work(core, core_ctx);

	if (core->num_inst == 1) {
		if (ctx->plugin_type == MFC_PLUGIN_FILM_GRAIN) {
			ret = mfc_plugin_pm_init(core);
			if (ret)
				goto err_power_on;
		}
		// TODO: meerkat timer
		mfc_plugin_perf_init(core);
	}

	mfc_change_state(core_ctx, MFCINST_INIT);

	core->sched->enqueue_work(core, core_ctx);
	if (core->sched->is_work(core))
		core->sched->queue_work(core);

	mfc_core_debug_leave();

	return ret;

err_power_on:
	core->core_ctx[ctx->num] = 0;
	kfree(core->core_ctx[ctx->num]);

err_init_inst:
	core->num_inst--;
	if (ctx->is_drm)
		core->num_drm_inst--;

	return ret;
}

int mfc_plugin_instance_deinit(struct mfc_core *core, struct mfc_ctx *ctx)
{
	struct mfc_core_ctx *core_ctx = core->core_ctx[ctx->num];
	int cnt = 0, ret = 0;

	mfc_debug_enter();

	if (core_ctx->state == MFCINST_FREE) {
		mfc_debug(2, "[PLUGIN] instance already deinit\n");
		return 0;
	}

	ret = mfc_plugin_get_hwlock(core_ctx);
	if (ret < 0) {
		mfc_err("[PLUGIN] failed to get hwlock\n");
		goto cleanup;
	}

	/* Pending work */
	mfc_change_state(core_ctx, MFCINST_FINISHING);
	cnt = mfc_get_queue_count(&ctx->buf_queue_lock, &ctx->plugin_buf_queue);
	mfc_debug(2, "[PLUGIN] plug-in job remained %d\n", cnt);
	while (cnt--) {
		ret = mfc_plugin_just_run(core_ctx);
		if (ret < 0) {
			mfc_err("[PLUGIN] failed to just run\n");
			continue;
		}
		if (mfc_wait_for_done_plugin(core, MFC_PLUGIN_FRAME_DONE_RET)) {
			mfc_err("[PLUGIN] Failed to wait plugin job\n");
			mfc_core_clean_dev_int_flags(core);
			ret = -EAGAIN;
			break;
		}
	}

	mfc_plugin_release_hwlock(core_ctx);

cleanup:
	mfc_plugin_destroy_listable_wq(core_ctx);

	mfc_cleanup_queue(&ctx->buf_queue_lock, &ctx->plugin_buf_queue);

	if (ctx->is_drm)
		core->num_drm_inst--;
	core->num_inst--;

	mfc_change_state(core_ctx, MFCINST_FREE);
	core->core_ctx[core_ctx->num] = 0;
	kfree(core_ctx);

	if (core->num_inst == 0) {
		//TODO: del meerkat_timer
		if (ctx->plugin_type == MFC_PLUGIN_FILM_GRAIN)
			mfc_plugin_pm_deinit(core);
		flush_workqueue(core->butler_wq);
	}

	mfc_plugin_perf_print(core);
	mfc_ctx_debug_leave();

	return ret;
}

int mfc_plugin_request_work(struct mfc_core *core, enum mfc_request_work work,
		struct mfc_ctx *ctx)
{
	int ret = 0;

	mfc_core_debug_enter();

	core->sched->queue_work(core);

	mfc_core_debug_leave();

	return ret;
}
