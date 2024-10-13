/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_sync.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_PLUGIN_SYNC_H
#define __MFC_PLUGIN_SYNC_H __FILE__

#include "../base/mfc_common.h"

/* It defined for SW plugin */
#define MFC_PLUGIN_FRAME_DONE_RET	100

void mfc_wake_up_plugin(struct mfc_core *core, unsigned int reason, unsigned int err);
int mfc_wait_for_done_plugin(struct mfc_core *core, int command);

int mfc_plugin_get_new_ctx(struct mfc_core *core);
int mfc_plugin_ready_set_bit(struct mfc_core_ctx *core_ctx, struct mfc_bits *data);
int mfc_plugin_ready_clear_bit(struct mfc_core_ctx *core_ctx, struct mfc_bits *data);

#endif /* __MFC_PLUGIN_SYNC_H */
