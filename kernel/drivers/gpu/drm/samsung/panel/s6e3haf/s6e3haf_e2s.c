/*
 * linux/drivers/gpu/drm/samsung/panel/s6e3haf/s6e3haf_e2s.c
 *
 * s6e3haf_e2s Driver
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
#include "oled_property.h"
#include "s6e3haf_e2s.h"
#include "s6e3haf_e2s_panel.h"
#include "s6e3haf_dimming.h"
#include "s6e3haf_e2s_ezop.h"

static int oled_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	return panel_property_set_value(prop,
			get_brightness_pac_step_by_subdev_id(panel_bl,
				PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness));
}

static int oled_hmd_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	return panel_property_set_value(prop,
			get_brightness_pac_step(panel_bl, panel_bl->props.brightness));
}

static int oled_lpm_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	panel->panel_data.props.lpm_brightness =
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;

	return panel_property_set_value(prop,
			get_subdev_actual_brightness_index(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD,
				panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness));
}

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

static int oled_hs_clk_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_info *panel_data = &panel->panel_data;
	u32 dsi_clk = panel_data->props.dsi_freq;
	int index;

	switch (dsi_clk) {
		case 1328000:
			index = S6E3HAF_E2S_HS_CLK_1328;
			break;
		case 1362000:
			index = S6E3HAF_E2S_HS_CLK_1362;
			break;
		case 1368000:
			index = S6E3HAF_E2S_HS_CLK_1368;
			break;
		default:
			panel_err("invalid dsi clock: %d\n", dsi_clk);
			BUG();
	}

	return panel_property_set_value(prop, index);
}

static struct panel_prop_enum_item oled_hs_clk_enum_items[MAX_S6E3HAF_E2S_HS_CLK] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_E2S_HS_CLK_1328),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_E2S_HS_CLK_1362),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3HAF_E2S_HS_CLK_1368),
};

static struct panel_prop_list s6e3haf_e2s_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(OLED_HS_CLK_PROPERTY,
			S6E3HAF_E2S_HS_CLK_1328, oled_hs_clk_enum_items,
			oled_hs_clk_property_update),
	/* range property */
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_NRM_BR_INDEX_PROPERTY,
			0, 0, S6E3HAF_E2S_TOTAL_NR_LUMINANCE,
			oled_br_index_property_update),
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_HMD_BR_INDEX_PROPERTY,
			0, 0, S6E3HAF_E2S_HMD_NR_LUMINANCE,
			oled_hmd_br_index_property_update),
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_LPM_BR_INDEX_PROPERTY,
			0, 0, S6E3HAF_AOD_NR_LUMINANCE,
			oled_lpm_br_index_property_update),
#ifdef CONFIG_USDM_MDNIE
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_MDNIE_NIGHT_LEVEL_PROPERTY,
			0, 0, S6E3HAF_E2S_MAX_NIGHT_LEVEL,
			oled_mdnie_night_level_property_update),
	__DIMEN_PROPERTY_RANGE_INITIALIZER(OLED_MDNIE_HBM_CE_LEVEL_PROPERTY,
			0, 0, S6E3HAF_E2S_MAX_HBM_CE_LEVEL,
			oled_mdnie_hbm_ce_level_property_update),
#endif
};

static struct dump_expect s6e3haf_e2s_self_mask_checksum_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0xF1, .msg = "Self Mask CHKSUM[0] Error(NG)" },
	{ .offset = 1, .mask = 0xFF, .value = 0x38, .msg = "Self Mask CHKSUM[1] Error(NG)" },
	{ .offset = 2, .mask = 0xFF, .value = 0x8B, .msg = "Self Mask CHKSUM[2] Error(NG)" },
	{ .offset = 3, .mask = 0xFF, .value = 0xBB, .msg = "Self Mask CHKSUM[3] Error(NG)" },
};
static struct dumpinfo s6e3haf_e2s_self_mask_checksum = DUMPINFO_INIT_V2(self_mask_checksum,
		&s6e3haf_restbl[RES_SELF_MASK_CHECKSUM], &OLED_FUNC(OLED_DUMP_SHOW_EXPECTS), s6e3haf_e2s_self_mask_checksum_expects);

static struct dump_expect s6e3haf_e2s_self_mask_factory_checksum_expects[] = {
	{ .offset = 0, .mask = 0xFF, .value = 0x68, .msg = "Self Mask FACTORY CHKSUM[0] Error(NG)" },
	{ .offset = 1, .mask = 0xFF, .value = 0x00, .msg = "Self Mask FACTORY CHKSUM[1] Error(NG)" },
	{ .offset = 2, .mask = 0xFF, .value = 0xA8, .msg = "Self Mask FACTORY CHKSUM[2] Error(NG)" },
	{ .offset = 3, .mask = 0xFF, .value = 0x00, .msg = "Self Mask FACTORY CHKSUM[3] Error(NG)" },
};
static struct dumpinfo s6e3haf_e2s_self_mask_factory_checksum = DUMPINFO_INIT_V2(self_mask_factory_checksum,
		&s6e3haf_restbl[RES_SELF_MASK_CHECKSUM], &OLED_FUNC(OLED_DUMP_SHOW_EXPECTS), s6e3haf_e2s_self_mask_factory_checksum_expects);

__visible_for_testing int __init s6e3haf_e2s_panel_init(void)
{
	struct common_panel_info *cpi = &s6e3haf_e2s_panel_info;

	s6e3haf_init(cpi);
	memcpy(&cpi->dumpinfo[DUMP_SELF_MASK_CHECKSUM],
		&s6e3haf_e2s_self_mask_checksum, sizeof(struct dumpinfo));
	memcpy(&cpi->dumpinfo[DUMP_SELF_MASK_FACTORY_CHECKSUM],
		&s6e3haf_e2s_self_mask_factory_checksum, sizeof(struct dumpinfo));
	cpi->prop_lists[USDM_DRV_LEVEL_MODEL] = s6e3haf_e2s_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_MODEL] = ARRAY_SIZE(s6e3haf_e2s_property_array);
	cpi->ezop_json = EZOP_JSON_BUFFER;
	register_common_panel(cpi);

	return 0;
}

__visible_for_testing void __exit s6e3haf_e2s_panel_exit(void)
{
	deregister_common_panel(&s6e3haf_e2s_panel_info);
}

module_init(s6e3haf_e2s_panel_init)
module_exit(s6e3haf_e2s_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
