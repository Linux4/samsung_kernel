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
#include "../panel_property.h"
#include "ana6710_r12s.h"
#include "ana6710_r12s_panel.h"
#include "ana6710_r12s_ezop.h"


struct pnobj_func ana6710_r12s_function_table[MAX_ANA6710_R12S_FUNCTION] = {
};

static struct panel_prop_enum_item ana6710_r12s_hs_clk_enum_items[MAX_ANA6710_R12S_HS_CLK] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ANA6710_R12S_HS_CLK_1328),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ANA6710_R12S_HS_CLK_1362),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ANA6710_R12S_HS_CLK_1368),
};

static int ana6710_r12s_hs_clk_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	u32 dsi_clk = panel_get_property_value(panel, PANEL_PROPERTY_DSI_FREQ);
	int index;

	switch (dsi_clk) {
		case 1328000:
			index = ANA6710_R12S_HS_CLK_1328;
			break;
		case 1362000:
			index = ANA6710_R12S_HS_CLK_1362;
			break;
		case 1368000:
			index = ANA6710_R12S_HS_CLK_1368;
			break;
		default:
			panel_err("invalid dsi clock: %d, use default clk 1362000\n", dsi_clk);
			index = ANA6710_R12S_HS_CLK_1362;
			break;
	}
	panel_dbg("dsi clock: %d, index: %d\n", dsi_clk, index);

	return panel_property_set_value(prop, index);
}

static struct panel_prop_enum_item ana6710_r12s_osc_clk_enum_items[MAX_ANA6710_R12S_OSC_CLK] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ANA6710_R12S_OSC_CLK_181300),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(ANA6710_R12S_OSC_CLK_178900),
};

static struct panel_prop_list ana6710_r12s_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(ANA6710_R12S_HS_CLK_PROPERTY,
			ANA6710_R12S_HS_CLK_1362, ana6710_r12s_hs_clk_enum_items,
			ana6710_r12s_hs_clk_property_update),
	__DIMEN_PROPERTY_ENUM_INITIALIZER(ANA6710_R12S_OSC_CLK_PROPERTY,
			ANA6710_R12S_OSC_CLK_181300, ana6710_r12s_osc_clk_enum_items,
			NULL),
};

int ana6710_r12s_osc_update(struct panel_device *panel)
{
	u32 osc_clk, old_idx, new_idx;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	osc_clk = panel_get_property_value(panel, PANEL_PROPERTY_OSC_FREQ);

	switch (osc_clk) {
		case 181300:
			new_idx = ANA6710_R12S_OSC_CLK_181300;
			break;
		case 178900:
			new_idx = ANA6710_R12S_OSC_CLK_178900;
			break;
		default:
			panel_err("invalid osc clock: %d, use default clk 181300\n", osc_clk);
			new_idx = ANA6710_R12S_OSC_CLK_181300;
			break;
	}
	panel_dbg("osc clock: %d, index: %d\n", osc_clk, new_idx);

	old_idx = panel_get_property_value(panel, ANA6710_R12S_OSC_CLK_PROPERTY);
	if (old_idx != new_idx) {
		panel_info("osc clock idx updated: %d -> %d\n", old_idx, new_idx);
		panel_set_property_value(panel, ANA6710_R12S_OSC_CLK_PROPERTY, new_idx);
	}

	return 0;
}


int ana6710_r12s_ddi_init(struct panel_device *panel, void *buf, u32 len)
{
	return ana6710_r12s_osc_update(panel);
}

__visible_for_testing int __init ana6710_r12s_panel_init(void)
{
	struct common_panel_info *cpi = &ana6710_r12s_panel_info;
	int ret;

	ana6710_init(cpi);
	cpi->ezop_json = EZOP_JSON_BUFFER;

	ret = panel_function_insert_array(ana6710_r12s_function_table,
			ARRAY_SIZE(ana6710_r12s_function_table));
	if (ret < 0)
		panel_err("failed to insert ana6710_r12s_function_table\n");

	cpi->prop_lists[USDM_DRV_LEVEL_MODEL] = ana6710_r12s_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_MODEL] = ARRAY_SIZE(ana6710_r12s_property_array);

	register_common_panel(cpi);

	panel_vote_up_to_probe(NULL);

	return 0;
}

__visible_for_testing void __exit ana6710_r12s_panel_exit(void)
{
	deregister_common_panel(&ana6710_r12s_panel_info);
}

module_init(ana6710_r12s_panel_init)
module_exit(ana6710_r12s_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
