/*
 * drivers/gpu/drm/panel/mcd_panel/mcd_panel_adapter_helper.h
 *
 * Header file for Samsung Common LCD Driver.
 *
 * Copyright (c) 2020 Samsung Electronics
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MTK_PANEL_MODES_H__
#define __MTK_PANEL_MODES_H__

#include "mcd_panel_adapter.h"

struct mtk_panel_desc *
mtk_panel_desc_create_from_panel_display_modes(struct mtk_panel *ctx,
		struct panel_display_modes *pdms);
void mtk_panel_desc_destroy(struct mtk_panel *ctx, struct mtk_panel_desc *desc);
struct panel_display_mode *mtk_panel_find_panel_mode(
		struct panel_display_modes *pdms, const struct drm_display_mode *pmode);
#endif /* __MTK_PANEL_MODES_H__ */
