/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_run.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_PLUGIN_RUN_H
#define __MFC_PLUGIN_RUN_H __FILE__

#include "../base/mfc_common.h"

int mfc_plugin_run_frame(struct mfc_core *core, struct mfc_core_ctx *core_ctx);

#endif /* __MFC_PLUGIN_RUN_H */
