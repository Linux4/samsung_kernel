/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_hwlock.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_PLUGIN_HWLOCK_H
#define __MFC_PLUGIN_HWLOCK_H __FILE__

#include "../base/mfc_common.h"

static inline void mfc_plugin_init_listable_wq(struct mfc_core_ctx *core_ctx)
{
	if (!core_ctx) {
		mfc_pr_err("no mfc core context to run\n");
		return;
	}

	INIT_LIST_HEAD(&core_ctx->hwlock_wq.list);
	init_waitqueue_head(&core_ctx->hwlock_wq.wait_queue);
	mutex_init(&core_ctx->hwlock_wq.wait_mutex);
	core_ctx->hwlock_wq.core_ctx = core_ctx;
	core_ctx->hwlock_wq.core = NULL;
}

static inline void mfc_plugin_destroy_listable_wq(struct mfc_core_ctx *core_ctx)
{
	if (!core_ctx) {
		mfc_pr_err("no mfc core context to run\n");
		return;
	}

	mutex_destroy(&core_ctx->hwlock_wq.wait_mutex);
}

void mfc_plugin_init_hwlock(struct mfc_core *core);
int mfc_plugin_get_hwlock(struct mfc_core_ctx *core_ctx);
void mfc_plugin_release_hwlock(struct mfc_core_ctx *core_ctx);

int mfc_plugin_just_run(struct mfc_core_ctx *core_ctx);
void mfc_plugin_try_run(struct mfc_core *core);

void mfc_plugin_hwlock_handler(struct mfc_core_ctx *core_ctx);

#endif /* __MFC_PLUGIN_HWLOCK_H */
