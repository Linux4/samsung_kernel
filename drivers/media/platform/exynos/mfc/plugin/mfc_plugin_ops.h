/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_ops.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "../base/mfc_common.h"

int mfc_plugin_instance_init(struct mfc_core *core, struct mfc_ctx *ctx);
int mfc_plugin_instance_deinit(struct mfc_core *core, struct mfc_ctx *ctx);
int mfc_plugin_request_work(struct mfc_core *core, enum mfc_request_work work,
		struct mfc_ctx *ctx);
