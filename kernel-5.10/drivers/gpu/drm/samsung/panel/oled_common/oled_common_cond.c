/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include "../panel_kunit.h"
#include "../panel.h"
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../panel_vrr.h"

bool oled_cond_is_panel_display_mode_changed(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (panel->panel_data.props.vrr_idx !=
			panel->panel_data.props.vrr_origin_idx);
}
EXPORT_SYMBOL(oled_cond_is_panel_display_mode_changed);

bool oled_cond_is_panel_refresh_rate_changed(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (panel->panel_data.props.vrr_fps !=
			panel->panel_data.props.vrr_origin_fps);
}
EXPORT_SYMBOL(oled_cond_is_panel_refresh_rate_changed);

bool oled_cond_is_panel_refresh_mode_changed(struct panel_device *panel)
{
	if (!panel)
		return false;

	return (panel->panel_data.props.vrr_mode !=
			panel->panel_data.props.vrr_origin_mode);
}
EXPORT_SYMBOL(oled_cond_is_panel_refresh_mode_changed);

MODULE_DESCRIPTION("oled_common driver");
MODULE_LICENSE("GPL");
