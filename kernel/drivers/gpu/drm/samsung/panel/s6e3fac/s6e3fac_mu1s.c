/*
 * linux/drivers/gpu/drm/samsung/panel/s6e3fac/s6e3fac_mu1s.c
 *
 * s6e3fac_mu1s Driver
 *
 * Copyright (c) 2023 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../panel_debug.h"
#include "s6e3fac_mu1s.h"
#include "s6e3fac_mu1s_panel.h"
#include "s6e3fac_dimming.h"

static int s6e3fac_mu1s_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	return panel_property_set_value(prop,
			get_brightness_pac_step_by_subdev_id(panel_bl,
				PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness));
}

static int s6e3fac_mu1s_hmd_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	return panel_property_set_value(prop,
			get_brightness_pac_step(panel_bl, panel_bl->props.brightness));
}

static int s6e3fac_mu1s_lpm_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	panel->panel_data.props.lpm_brightness =
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;

	return panel_property_set_value(prop,
			get_subdev_actual_brightness_index(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD,
				panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness));
}

static int s6e3fac_mu1s_hs_clk_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_info *panel_data = &panel->panel_data;
	u32 dsi_clk = panel_data->props.dsi_freq;
	int index;

	switch (dsi_clk) {
	case 1328000:
		index = S6E3FAC_MU1S_HS_CLK_1328;
		break;
	case 1362000:
		index = S6E3FAC_MU1S_HS_CLK_1362;
		break;
	case 1368000:
		index = S6E3FAC_MU1S_HS_CLK_1368;
		break;
	default:
		panel_err("invalid dsi clock: %d\n", dsi_clk);
		BUG();
	}

	return panel_property_set_value(prop, index);
}

static struct panel_prop_enum_item s6e3fac_mu1s_hs_clk_enum_items[MAX_S6E3FAC_MU1S_HS_CLK] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_MU1S_HS_CLK_1328),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_MU1S_HS_CLK_1362),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FAC_MU1S_HS_CLK_1368),
};

static struct panel_prop_list s6e3fac_mu1s_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3FAC_MU1S_HS_CLK_PROPERTY,
			S6E3FAC_MU1S_HS_CLK_1328, s6e3fac_mu1s_hs_clk_enum_items,
			s6e3fac_mu1s_hs_clk_property_update),
	/* range property */
	__DIMEN_PROPERTY_RANGE_INITIALIZER(S6E3FAC_MU1S_BR_INDEX_PROPERTY,
			0, 0, S6E3FAC_MU1S_TOTAL_NR_LUMINANCE,
			s6e3fac_mu1s_br_index_property_update),
	__DIMEN_PROPERTY_RANGE_INITIALIZER(S6E3FAC_MU1S_HMD_BR_INDEX_PROPERTY,
			0, 0, S6E3FAC_MU1S_HMD_NR_LUMINANCE,
			s6e3fac_mu1s_hmd_br_index_property_update),
	__DIMEN_PROPERTY_RANGE_INITIALIZER(S6E3FAC_MU1S_LPM_BR_INDEX_PROPERTY,
			0, 0, S6E3FAC_AOD_NR_LUMINANCE,
			s6e3fac_mu1s_lpm_br_index_property_update),
};

__visible_for_testing int __init s6e3fac_mu1s_panel_init(void)
{
	struct common_panel_info *cpi = &s6e3fac_mu1s_panel_info;

	s6e3fac_init(cpi);
	cpi->prop_lists[USDM_DRV_LEVEL_MODEL] = s6e3fac_mu1s_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_MODEL] = ARRAY_SIZE(s6e3fac_mu1s_property_array);
	register_common_panel(cpi);

	return 0;
}

__visible_for_testing void __exit s6e3fac_mu1s_panel_exit(void)
{
	deregister_common_panel(&s6e3fac_mu1s_panel_info);
}

module_init(s6e3fac_mu1s_panel_init)
module_exit(s6e3fac_mu1s_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
