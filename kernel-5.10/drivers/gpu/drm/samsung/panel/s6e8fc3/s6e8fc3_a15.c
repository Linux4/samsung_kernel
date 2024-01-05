/*
 * linux/drivers/video/fbdev/exynos/panel/tft_common/tft_common.c
 *
 * TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../maptbl.h"
#include "../panel.h"
#include "../panel_debug.h"
#include "../panel_function.h"
#include "s6e8fc3_a15_panel.h"

static int s6e8fc3_a15_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	return panel_property_set_value(prop,
			get_brightness_pac_step_by_subdev_id(panel_bl,
				PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness));
}

#ifdef CONFIG_USDM_MDNIE
static int oled_mdnie_night_level_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct mdnie_info *mdnie = &panel->mdnie;

	return panel_property_set_value(prop, mdnie->props.night_level);
}
#endif

static struct panel_prop_list s6e8fc3_a15_property_array[] = {
	/* range property */
	__DIMEN_PROPERTY_RANGE_INITIALIZER(S6E8FC3_A15_BR_INDEX_PROPERTY,
			0, 0, S6E8FC3_A15_TOTAL_NR_LUMINANCE,
			s6e8fc3_a15_br_index_property_update),
#ifdef CONFIG_USDM_MDNIE
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_MDNIE_NIGHT_LEVEL_PROPERTY,
			0, 0, S6E8FC3_A15_MAX_NIGHT_LEVEL,
			oled_mdnie_night_level_property_update),
#endif
};

static int __init s6e8fc3_a15_panel_init(void)
{
	struct common_panel_info *cpi = &s6e8fc3_a15_panel_info;
	s6e8fc3_init(cpi);
	cpi->prop_lists[USDM_DRV_LEVEL_MODEL] = s6e8fc3_a15_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_MODEL] = ARRAY_SIZE(s6e8fc3_a15_property_array);
	register_common_panel(&s6e8fc3_a15_panel_info);

	return 0;
}

static void __exit s6e8fc3_a15_panel_exit(void)
{
	deregister_common_panel(&s6e8fc3_a15_panel_info);
}

module_init(s6e8fc3_a15_panel_init);
module_exit(s6e8fc3_a15_panel_exit);
MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
