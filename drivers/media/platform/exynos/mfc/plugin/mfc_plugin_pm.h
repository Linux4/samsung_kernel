/*
 * drivers/media/platform/exynos/mfc/plugin/mfc_plugin_pm.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_PLUGIN_PM_H
#define __MFC_PLUGIN_PM_H __FILE__

#include "../base/mfc_common.h"

void mfc_plugin_protection_on(struct mfc_core *core);
void mfc_plugin_protection_off(struct mfc_core *core);
int mfc_plugin_pm_init(struct mfc_core *core);
void mfc_plugin_pm_deinit(struct mfc_core *core);
void mfc_plugin_pm_clock_on(struct mfc_core *core);
void mfc_plugin_pm_clock_off(struct mfc_core *core);

#endif /* __MFC_PLUGIN_PM_H */
