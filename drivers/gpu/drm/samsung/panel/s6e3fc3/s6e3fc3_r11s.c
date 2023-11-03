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
#include "s6e3fc3_r11s_panel.h"

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
/*
 * s6e3fc3_decoder_test - test ddi's decoder function
 *
 * description of state values:
 * [0](14h 1st): 0xEB (OK) other (NG)
 * [1](14h 2nd): 0x21 (OK) other (NG)
 *
 * [0](15h 1st): 0x96 (OK) other (NG)
 * [1](15h 2nd): 0xAF (OK) other (NG)
 */
int s6e3fc3_r11s_decoder_test(struct panel_device *panel, void *data, u32 len)
{
	int ret = 0;
	u8 read_buf1[S6E3FC3_DECODER_TEST1_LEN] = { -1, -1 };
	u8 read_buf2[S6E3FC3_DECODER_TEST2_LEN] = { -1, -1 };
	u8 read_buf3[S6E3FC3_DECODER_TEST3_LEN] = { -1, -1 };
	u8 read_buf4[S6E3FC3_DECODER_TEST4_LEN] = { -1, -1 };

	if (!panel)
		return -EINVAL;

	ret = panel_do_seqtbl_by_name_nolock(panel, PANEL_DECODER_TEST_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write decoder-test seq\n");
		return ret;
	}

	ret = panel_resource_copy(panel, read_buf1, "decoder_test1"); // 0x14 in normal voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test1 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf2, "decoder_test2"); // 0x15 in normal voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test2 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf3, "decoder_test3"); // 0x14 in low voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test1 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf4, "decoder_test4"); // 0x15 in low voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test2 copy failed\n");
		return -ENODATA;
	}

	if ((read_buf1[0] == 0xEB) && (read_buf1[1] == 0x21) &&
		(read_buf2[0] == 0x96) && (read_buf2[1] == 0xAF) &&
		(read_buf3[0] == 0xEB) && (read_buf3[1] == 0x21) &&
		(read_buf4[0] == 0x96) && (read_buf4[1] == 0xAF)) {
		ret = PANEL_DECODER_TEST_PASS;
		panel_info("Fail [normal]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, [low]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, ret: %d\n",
			read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
			read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1],	ret);
	} else {
		ret = PANEL_DECODER_TEST_FAIL;
		panel_info("Fail [normal]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, [low]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, ret: %d\n",
			read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
			read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1],	ret);
	}

	snprintf((char *)data, len, "%02x %02x %02x %02x %02x %02x %02x %02x",
		read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
		read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1]);

	return ret;
}
#endif

static int s6e3fc3_r11s_hs_clk_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_info *panel_data = &panel->panel_data;
	u32 dsi_clk = panel_data->props.dsi_freq;
	int index;

	switch (dsi_clk) {
	case 1328000:
		index = S6E3FC3_R11S_HS_CLK_1328;
		break;
	case 1362000:
		index = S6E3FC3_R11S_HS_CLK_1362;
		break;
	case 1368000:
		index = S6E3FC3_R11S_HS_CLK_1368;
		break;
	default:
		panel_err("invalid dsi clock: %d\n", dsi_clk);
		BUG();
	}

	return panel_property_set_value(prop, index);
}

static struct panel_prop_enum_item s6e3fc3_r11s_hs_clk_enum_items[MAX_S6E3FC3_R11S_HS_CLK] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FC3_R11S_HS_CLK_1328),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FC3_R11S_HS_CLK_1362),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(S6E3FC3_R11S_HS_CLK_1368),
};

static int s6e3fc3_r11s_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	return panel_property_set_value(prop,
			get_brightness_pac_step_by_subdev_id(panel_bl,
				PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness));
}

static int s6e3fc3_r11s_lpm_br_index_property_update(struct panel_property *prop)
{
	struct panel_device *panel = prop->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	panel->panel_data.props.lpm_brightness =
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;

	return panel_property_set_value(prop,
			get_subdev_actual_brightness_index(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD,
				panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness));
}

static struct panel_prop_list s6e3fc3_r11s_property_array[] = {
	/* enum property */
	__DIMEN_PROPERTY_ENUM_INITIALIZER(S6E3FC3_R11S_HS_CLK_PROPERTY,
			S6E3FC3_R11S_HS_CLK_1328, s6e3fc3_r11s_hs_clk_enum_items,
			s6e3fc3_r11s_hs_clk_property_update),
	/* range property */
	__DIMEN_PROPERTY_RANGE_INITIALIZER(S6E3FC3_R11S_BR_INDEX_PROPERTY,
			0, 0, S6E3FC3_R11S_TOTAL_NR_LUMINANCE,
			s6e3fc3_r11s_br_index_property_update),
	__DIMEN_PROPERTY_RANGE_INITIALIZER(S6E3FC3_R11S_LPM_BR_INDEX_PROPERTY,
			0, 0, S6E3FC3_R11S_AOD_NR_LUMINANCE,
			s6e3fc3_r11s_lpm_br_index_property_update),
};

#if 0
struct pnobj_func s6e3fc3_r11s_function_table[MAX_S6E3FC3_R11S_FUNCTION] = {
};
#endif

static int __init s6e3fc3_r11s_panel_init(void)
{
	struct common_panel_info *cpi = &s6e3fc3_r11s_panel_info;

	s6e3fc3_init(cpi);
	/* panel_function_insert_array(s6e3fc3_r11s_function_table, ARRAY_SIZE(s6e3fc3_r11s_function_table)); */
	cpi->prop_lists[USDM_DRV_LEVEL_MODEL] = s6e3fc3_r11s_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_MODEL] = ARRAY_SIZE(s6e3fc3_r11s_property_array);
	register_common_panel(cpi);

	return 0;
}

static void __exit s6e3fc3_r11s_panel_exit(void)
{
	deregister_common_panel(&s6e3fc3_r11s_panel_info);
}

module_init(s6e3fc3_r11s_panel_init);
module_exit(s6e3fc3_r11s_panel_exit);
MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
