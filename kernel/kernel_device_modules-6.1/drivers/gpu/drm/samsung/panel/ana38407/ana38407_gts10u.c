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
#include "ana38407_gts10u.h"
#include "ana38407_gts10u_panel.h"
#include "ana38407_gts10u_ezop.h"

#ifdef CONFIG_USDM_MDNIE
static int oled_mdnie_night_level_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct mdnie_info *mdnie = &panel->mdnie;

	return panel_property_set_value(prop, mdnie->props.night_level);
}

static int oled_mdnie_hbm_ce_level_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct mdnie_info *mdnie = &panel->mdnie;

	return panel_property_set_value(prop, mdnie->props.hbm_ce_level);
}
#endif

static struct panel_prop_list ana38407_gts10u_property_array[] = {
#ifdef CONFIG_USDM_MDNIE
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_MDNIE_NIGHT_LEVEL_PROPERTY,
			0, 0, ANA38407_GTS10U_MAX_NIGHT_LEVEL,
			oled_mdnie_night_level_property_update),
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_MDNIE_HBM_CE_LEVEL_PROPERTY,
			0, 0, ANA38407_GTS10U_MAX_HBM_CE_LEVEL,
			oled_mdnie_hbm_ce_level_property_update),
#endif
};

__visible_for_testing int __init ana38407_gts10u_panel_init(void)
{
	struct common_panel_info *cpi = &ana38407_gts10u_panel_info;

	ana38407_init(cpi);

	cpi->ezop_json = EZOP_JSON_BUFFER;
	cpi->prop_lists[USDM_DRV_LEVEL_MODEL] = ana38407_gts10u_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_MODEL] = ARRAY_SIZE(ana38407_gts10u_property_array);

	register_common_panel(cpi);

	panel_vote_up_to_probe(NULL);

	return 0;
}

__visible_for_testing void __exit ana38407_gts10u_panel_exit(void)
{
	deregister_common_panel(&ana38407_gts10u_panel_info);
}

module_init(ana38407_gts10u_panel_init)
module_exit(ana38407_gts10u_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
