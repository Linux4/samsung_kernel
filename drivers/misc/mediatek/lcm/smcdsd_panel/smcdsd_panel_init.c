// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "lcm_drv.h"
#include "smcdsd_board.h"
#include "smcdsd_panel.h"

#define dbg_info(fmt, ...)		pr_info(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)
#define dbg_none(fmt, ...)		pr_debug(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)

#define STREQ(a, b)			(a && b && (*(a) == *(b)) && (strcmp((a), (b)) == 0))
#define STRNEQ(a, b)			(a && b && (strncmp((a), (b), (strlen(a))) == 0))

#define PANEL_DTS_NAME			"smcdsd_panel"

/* -------------------------------------------------------------------------- */
/* boot param */
/* -------------------------------------------------------------------------- */
unsigned int lcdtype = 1;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	dbg_info("%s: lcdtype: %6X\n", __func__, lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

/* -------------------------------------------------------------------------- */
/* helper to parse dt */
/* -------------------------------------------------------------------------- */
static void smcdsd_of_property_read_u32(const struct device_node *np, const char *propname, u32 *out_value)
{
	int ret = 0, count = 0, i;
	char print_buf[32] = {0, };
	struct seq_file m = {
		.buf = print_buf,
		.size = sizeof(print_buf) - 1,
	};

	if (!np || !propname || !out_value)
		dbg_none("of_property_read_u32 null\n");

	count = of_property_count_u32_elems(np, propname);
	if (count < 1) {
		dbg_none("of_property_count_u32_elems fail for %s(%d)\n", propname, count);
		return;
	}

	ret = of_property_read_u32_array(np, propname, out_value, count);
	if (ret < 0)
		dbg_none("of_property_read_u32_array fail for %s(%d)\n", propname, ret);
	else {
		for (i = 0; i < count; i++)
			seq_printf(&m, "%d, ", *(out_value + i));

		dbg_info("of_property_read_u32_array %s(%s)\n", propname, m.buf);
	}
}

int __smcdsd_panel_get_params_from_dt(struct LCM_PARAMS *lcm_params)
{
	struct device_node *np = NULL;
	int ret = 0;

	np = of_find_recommend_lcd_info(NULL);
	if (!np) {
		dbg_info("of_find_recommend_lcd_info fail\n");
		return -EINVAL;
	}

	smcdsd_of_property_read_u32(np, "lcm_params-types",
		&lcm_params->type);
	smcdsd_of_property_read_u32(np, "lcm_params-resolution",
		&lcm_params->width);
	smcdsd_of_property_read_u32(np, "lcm_params-io_select_mode",
		&lcm_params->io_select_mode);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-mode",
		&lcm_params->dsi.mode);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-lane_num",
		&lcm_params->dsi.LANE_NUM);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-packet_size",
		&lcm_params->dsi.packet_size);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-ps",
		&lcm_params->dsi.PS);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-ssc_disable",
		&lcm_params->dsi.ssc_disable);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-vertical_sync_active",
		&lcm_params->dsi.vertical_sync_active);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-vertical_backporch",
		&lcm_params->dsi.vertical_backporch);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-vertical_frontporch",
		&lcm_params->dsi.vertical_frontporch);
	smcdsd_of_property_read_u32(np,
		"lcm_params-dsi-vertical_frontporch_for_low_power",
		&lcm_params->dsi.vertical_frontporch_for_low_power);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-vertical_active_line",
		&lcm_params->dsi.vertical_active_line);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_sync_active",
		&lcm_params->dsi.horizontal_sync_active);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_backporch",
		&lcm_params->dsi.horizontal_backporch);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_frontporch",
		&lcm_params->dsi.horizontal_frontporch);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_blanking_pixel",
		&lcm_params->dsi.horizontal_blanking_pixel);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_active_pixel",
		&lcm_params->dsi.horizontal_active_pixel);
#if defined(THIS_IS_REDUNDANT_CODE)
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-port",
		&lcm_params->dbi.port);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-clock_freq",
		&lcm_params->dbi.clock_freq);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-data_width",
		&lcm_params->dbi.data_width);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-data_format",
		(u32 *) (&lcm_params->dbi.data_format));
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-cpu_write_bits",
		&lcm_params->dbi.cpu_write_bits);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-io_driving_current",
		&lcm_params->dbi.io_driving_current);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-msb_io_driving_current",
		&lcm_params->dbi.msb_io_driving_current);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-ctrl_io_driving_current",
		&lcm_params->dbi.ctrl_io_driving_current);

	smcdsd_of_property_read_u32(np, "lcm_params-dbi-te_mode",
		&lcm_params->dbi.te_mode);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-te_edge_polarity",
		&lcm_params->dbi.te_edge_polarity);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-te_hs_delay_cnt",
		&lcm_params->dbi.te_hs_delay_cnt);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-te_vs_width_cnt",
		&lcm_params->dbi.te_vs_width_cnt);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-te_vs_width_cnt_div",
		&lcm_params->dbi.te_vs_width_cnt_div);

	smcdsd_of_property_read_u32(np, "lcm_params-dbi-serial-params0",
		&lcm_params->dbi.serial.cs_polarity);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-serial-params1",
		&lcm_params->dbi.serial.css);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-serial-params2",
		&lcm_params->dbi.serial.sif_3wire);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-parallel-params0",
		&lcm_params->dbi.parallel.write_setup);
	smcdsd_of_property_read_u32(np, "lcm_params-dbi-parallel-params1",
		&lcm_params->dbi.parallel.read_hold);

	smcdsd_of_property_read_u32(np, "lcm_params-dpi-mipi_pll_clk_ref",
		&lcm_params->dpi.mipi_pll_clk_ref);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-mipi_pll_clk_div1",
		&lcm_params->dpi.mipi_pll_clk_div1);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-mipi_pll_clk_div2",
		&lcm_params->dpi.mipi_pll_clk_div2);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-mipi_pll_clk_fbk_div",
		&lcm_params->dpi.mipi_pll_clk_fbk_div);

	smcdsd_of_property_read_u32(np, "lcm_params-dpi-dpi_clk_div",
		&lcm_params->dpi.dpi_clk_div);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-dpi_clk_duty",
		&lcm_params->dpi.dpi_clk_duty);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-PLL_CLOCK",
		&lcm_params->dpi.PLL_CLOCK);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-dpi_clock",
		&lcm_params->dpi.dpi_clock);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-ssc_disable",
		&lcm_params->dpi.ssc_disable);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-ssc_range",
		&lcm_params->dpi.ssc_range);

	smcdsd_of_property_read_u32(np, "lcm_params-dpi-width",
		&lcm_params->dpi.width);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-height",
		&lcm_params->dpi.height);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-bg_width",
		&lcm_params->dpi.bg_width);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-bg_height",
		&lcm_params->dpi.bg_height);

	smcdsd_of_property_read_u32(np, "lcm_params-dpi-clk_pol",
		&lcm_params->dpi.clk_pol);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-de_pol",
		&lcm_params->dpi.de_pol);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-vsync_pol",
		&lcm_params->dpi.vsync_pol);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-hsync_pol",
		&lcm_params->dpi.hsync_pol);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-hsync_pulse_width",
		&lcm_params->dpi.hsync_pulse_width);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-hsync_back_porch",
		&lcm_params->dpi.hsync_back_porch);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-hsync_front_porch",
		&lcm_params->dpi.hsync_front_porch);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-vsync_pulse_width",
		&lcm_params->dpi.vsync_pulse_width);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-vsync_back_porch",
		&lcm_params->dpi.vsync_back_porch);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-vsync_front_porch",
		&lcm_params->dpi.vsync_front_porch);

	smcdsd_of_property_read_u32(np, "lcm_params-dpi-format",
		&lcm_params->dpi.format);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-rgb_order",
		&lcm_params->dpi.rgb_order);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-is_serial_output",
		&lcm_params->dpi.is_serial_output);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-i2x_en",
		&lcm_params->dpi.i2x_en);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-i2x_edge",
		&lcm_params->dpi.i2x_edge);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-embsync",
		&lcm_params->dpi.embsync);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-lvds_tx_en",
		&lcm_params->dpi.lvds_tx_en);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-bit_swap",
		&lcm_params->dpi.bit_swap);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-intermediat_buffer_num",
		&lcm_params->dpi.intermediat_buffer_num);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-io_driving_current",
		&lcm_params->dpi.io_driving_current);
	smcdsd_of_property_read_u32(np, "lcm_params-dpi-lsb_io_driving_current",
		&lcm_params->dpi.lsb_io_driving_current);

	smcdsd_of_property_read_u32(np, "lcm_params-dsi-mode",
		&lcm_params->dsi.mode);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-switch_mode",
		&lcm_params->dsi.switch_mode);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-DSI_WMEM_CONTI",
		&lcm_params->dsi.DSI_WMEM_CONTI);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-DSI_RMEM_CONTI",
		&lcm_params->dsi.DSI_RMEM_CONTI);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-VC_NUM",
		&lcm_params->dsi.VC_NUM);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-lane_num",
		&lcm_params->dsi.LANE_NUM);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-data_format",
		(u32 *) (&lcm_params->dsi.data_format));
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-intermediat_buffer_num",
		&lcm_params->dsi.intermediat_buffer_num);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-ps",
		&lcm_params->dsi.PS);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-word_count",
		&lcm_params->dsi.word_count);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-packet_size",
		&lcm_params->dsi.packet_size);

	smcdsd_of_property_read_u32(np, "lcm_params-dsi-vertical_sync_active",
		&lcm_params->dsi.vertical_sync_active);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-vertical_backporch",
		&lcm_params->dsi.vertical_backporch);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-vertical_frontporch",
		&lcm_params->dsi.vertical_frontporch);
	smcdsd_of_property_read_u32(np,
		"lcm_params-dsi-vertical_frontporch_for_low_power",
		&lcm_params->dsi.vertical_frontporch_for_low_power);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-vertical_active_line",
		&lcm_params->dsi.vertical_active_line);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_sync_active",
		&lcm_params->dsi.horizontal_sync_active);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_backporch",
		&lcm_params->dsi.horizontal_backporch);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_frontporch",
		&lcm_params->dsi.horizontal_frontporch);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_blanking_pixel",
		&lcm_params->dsi.horizontal_blanking_pixel);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_active_pixel",
		&lcm_params->dsi.horizontal_active_pixel);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_bllp",
		&lcm_params->dsi.horizontal_bllp);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-line_byte",
		&lcm_params->dsi.line_byte);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_sync_active_byte",
		&lcm_params->dsi.horizontal_sync_active_byte);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_backportch_byte",
		&lcm_params->dsi.horizontal_backporch_byte);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-horizontal_frontporch_byte",
		&lcm_params->dsi.horizontal_frontporch_byte);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-rgb_byte",
		&lcm_params->dsi.rgb_byte);
	smcdsd_of_property_read_u32(np,
		"lcm_params-dsi-horizontal_sync_active_word_count",
		&lcm_params->dsi.horizontal_sync_active_word_count);
	smcdsd_of_property_read_u32(np,
		"lcm_params-dsi-horizontal_backporch_word_count",
		&lcm_params->dsi.horizontal_backporch_word_count);
	smcdsd_of_property_read_u32(np,
		"lcm_params-dsi-horizontal_frontporch_word_count",
		&lcm_params->dsi.horizontal_frontporch_word_count);

	smcdsd_of_property_read_u8(np, "lcm_params-dsi-HS_TRAIL",
		&lcm_params->dsi.HS_TRAIL);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-ZERO",
		&lcm_params->dsi.HS_ZERO);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-HS_PRPR",
		&lcm_params->dsi.HS_PRPR);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-LPX",
		&lcm_params->dsi.LPX);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-TA_SACK",
		&lcm_params->dsi.TA_SACK);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-TA_GET",
		&lcm_params->dsi.TA_GET);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-TA_SURE",
		&lcm_params->dsi.TA_SURE);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-TA_GO",
		&lcm_params->dsi.TA_GO);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-CLK_TRAIL",
		&lcm_params->dsi.CLK_TRAIL);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-CLK_ZERO",
		&lcm_params->dsi.CLK_ZERO);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-LPX_WAIT",
		&lcm_params->dsi.LPX_WAIT);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-CONT_DET",
		&lcm_params->dsi.CONT_DET);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-CLK_HS_PRPR",
		&lcm_params->dsi.CLK_HS_PRPR);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-CLK_HS_POST",
		&lcm_params->dsi.CLK_HS_POST);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-DA_HS_EXIT",
		&lcm_params->dsi.DA_HS_EXIT);
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-CLK_HS_EXIT",
		&lcm_params->dsi.CLK_HS_EXIT);

	smcdsd_of_property_read_u32(np, "lcm_params-dsi-pll_select",
		&lcm_params->dsi.pll_select);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-pll_div1",
		&lcm_params->dsi.pll_div1);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-pll_div2",
		&lcm_params->dsi.pll_div2);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-fbk_div",
		&lcm_params->dsi.fbk_div);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-fbk_sel",
		&lcm_params->dsi.fbk_sel);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-rg_bir",
		&lcm_params->dsi.rg_bir);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-rg_bic",
		&lcm_params->dsi.rg_bic);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-rg_bp",
		&lcm_params->dsi.rg_bp);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-pll_clock",
		&lcm_params->dsi.PLL_CLOCK);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-dsi_clock",
		&lcm_params->dsi.dsi_clock);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-ssc_disable",
		&lcm_params->dsi.ssc_disable);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-ssc_range",
		&lcm_params->dsi.ssc_range);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-compatibility_for_nvk",
			    &lcm_params->dsi.compatibility_for_nvk);
#endif
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-cont_clock",
		&lcm_params->dsi.cont_clock);
#if defined(THIS_IS_REDUNDANT_CODE)
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-ufoe_enable",
		&lcm_params->dsi.ufoe_enable);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-ufoe_params",
			    (u32 *) (&lcm_params->dsi.ufoe_params));
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-edp_panel",
		&lcm_params->dsi.edp_panel);

	smcdsd_of_property_read_u32(np, "lcm_params-dsi-customization_esd_check_enable",
			    &lcm_params->dsi.customization_esd_check_enable);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-esd_check_enable",
			    &lcm_params->dsi.esd_check_enable);

	smcdsd_of_property_read_u32(np, "lcm_params-dsi-lcm_int_te_monitor",
			    &lcm_params->dsi.lcm_int_te_monitor);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-lcm_int_te_period",
			    &lcm_params->dsi.lcm_int_te_period);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-lcm_ext_te_monitor",
			    &lcm_params->dsi.lcm_ext_te_monitor);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-lcm_ext_te_enable",
			    &lcm_params->dsi.lcm_ext_te_enable);

	smcdsd_of_property_read_u32(np, "lcm_params-dsi-noncont_clock",
		&lcm_params->dsi.noncont_clock);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-noncont_clock_period",
			    &lcm_params->dsi.noncont_clock_period);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-clk_lp_per_line_enable",
			    &lcm_params->dsi.clk_lp_per_line_enable);

	smcdsd_of_property_read_u8(np, "lcm_params-dsi-lcm_esd_check_table0",
			   (u8 *) (&(lcm_params->dsi.lcm_esd_check_table[0])));
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-lcm_esd_check_table1",
			   (u8 *) (&(lcm_params->dsi.lcm_esd_check_table[1])));
	smcdsd_of_property_read_u8(np, "lcm_params-dsi-lcm_esd_check_table2",
			   (u8 *) (&(lcm_params->dsi.lcm_esd_check_table[2])));
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-switch_mode_enable",
			    &lcm_params->dsi.switch_mode_enable);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-dual_dsi_type",
		&lcm_params->dsi.dual_dsi_type);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-lane_swap_en",
		&lcm_params->dsi.lane_swap_en);
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-lane_swap0",
	    (u32 *) (&(lcm_params->dsi.lane_swap[0][0])));
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-lane_swap1",
	    (u32 *) (&(lcm_params->dsi.lane_swap[1][0])));
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-vertical_vfp_lp",
		&lcm_params->dsi.vertical_vfp_lp);
#endif
	smcdsd_of_property_read_u32(np, "lcm_params-physical_width",
		&lcm_params->physical_width);
	smcdsd_of_property_read_u32(np, "lcm_params-physical_height",
		&lcm_params->physical_height);
	smcdsd_of_property_read_u32(np, "lcm_params-physical_width_um",
		&lcm_params->physical_width_um);
	smcdsd_of_property_read_u32(np, "lcm_params-physical_height_um",
		&lcm_params->physical_height_um);
	smcdsd_of_property_read_u32(np, "lcm_params-od_table_size",
		&lcm_params->od_table_size);
	smcdsd_of_property_read_u32(np, "lcm_params-od_table",
		(u32 *) (&lcm_params->od_table));

	smcdsd_of_property_read_u32(np, "lcm_params-dsi-data_rate",
		&lcm_params->dsi.data_rate);

	return ret;
}

int smcdsd_panel_get_params_from_dt(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->lcm_if = LCM_INTERFACE_DSI0;

	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size = 256;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	__smcdsd_panel_get_params_from_dt(params);

	params->dsi.vertical_active_line = params->dsi.vertical_active_line ? params->dsi.vertical_active_line : params->height;
	params->dsi.horizontal_active_pixel = params->dsi.horizontal_active_pixel ? params->dsi.horizontal_active_pixel : params->width;

	params->dsi.PLL_CLOCK = params->dsi.PLL_CLOCK ? params->dsi.PLL_CLOCK : params->dsi.data_rate >> 1;

	return 0;
}

/* -------------------------------------------------------------------------- */
/* helper to find lcd driver */
/* -------------------------------------------------------------------------- */
struct mipi_dsi_lcd_driver *mipi_lcd_driver;

static int __lcd_driver_update_by_lcd_info_name(struct mipi_dsi_lcd_driver *drv)
{
	struct device_node *node;
	int count = 0, ret = 0;

	node = of_find_node_with_property(NULL, PANEL_DTS_NAME);
	if (!node) {
		dbg_info("%s: %s of_find_node_with_property fail\n", __func__, PANEL_DTS_NAME);
		ret = -EINVAL;
		goto exit;
	}

	count = of_count_phandle_with_args(node, PANEL_DTS_NAME, NULL);
	if (count < 0) {
		dbg_info("%s: %s of_count_phandle_with_args fail: %d\n", __func__, PANEL_DTS_NAME, count);
		ret = -EINVAL;
		goto exit;
	}

	node = of_parse_phandle(node, PANEL_DTS_NAME, 0);
	if (!node) {
		dbg_info("%s: %s of_parse_phandle fail\n", __func__, PANEL_DTS_NAME);
		ret = -EINVAL;
		goto exit;
	}

	if (IS_ERR_OR_NULL(drv) || IS_ERR_OR_NULL(drv->driver.name)) {
		dbg_info("%s: we need lcd_driver name to compare with dts node name(%s)\n", __func__, node->name);
		ret = -EINVAL;
		goto exit;
	}

	if (!strcmp(node->name, drv->driver.name) && mipi_lcd_driver != drv) {
		mipi_lcd_driver = drv;
		dbg_info("%s: driver(%s) is updated\n", __func__, mipi_lcd_driver->driver.name);
	} else {
		dbg_none("%s: driver(%s) is diffferent with %s node(%s)\n", __func__, PANEL_DTS_NAME, node->name, drv->driver.name);
	}

exit:
	return ret;
}

static void __lcd_driver_dts_update(void)
{
	struct device_node *nplcd = NULL, *np = NULL;
	int i = 0, count = 0, ret = -EINVAL;
	unsigned int id_index, mask, expect;
	u32 id_match_info[10] = {0, };

	nplcd = of_find_node_with_property(NULL, PANEL_DTS_NAME);
	if (!nplcd) {
		dbg_info("%s: %s property does not exist\n", __func__, PANEL_DTS_NAME);
		return;
	}

	count = of_count_phandle_with_args(nplcd, PANEL_DTS_NAME, NULL);
	if (count < 2) {
		/* dbg_info("%s: %s property phandle count is %d. so no need to update check\n", __func__, PANEL_DTS_NAME, count); */
		return;
	}

	for (i = 0; i < count; i++) {
		np = of_parse_phandle(nplcd, PANEL_DTS_NAME, i);
		dbg_info("%s: %dth dts is %s\n", __func__, i, (np && np->name) ? np->name : "null");
		if (!np || !of_get_property(np, "id_match", NULL))
			continue;

		count = of_property_count_u32_elems(np, "id_match");
		if (count < 0 || count > ARRAY_SIZE(id_match_info) || count % 2) {
			dbg_info("%s: %dth dts(%s) has invalid id_match count(%d)\n", __func__, i, np->name, count);
			continue;
		}

		memset(id_match_info, 0, sizeof(id_match_info));
		ret = of_property_read_u32_array(np, "id_match", id_match_info, count);
		if (ret < 0) {
			dbg_info("%s: of_property_read_u32_array fail. ret(%d)\n", __func__, ret);
			continue;
		}

		for (id_index = 0; id_index < count; id_index += 2) {
			mask = id_match_info[id_index];
			expect = id_match_info[id_index + 1];

			if ((lcdtype & mask) == (expect & mask)) {
				dbg_info("%s: %dth dts(%s) id_match. lcdtype(%06X) mask(%06X) expect(%06X)\n",
					__func__, i, np->name, lcdtype, mask, expect);
				if (i > 0)
					ret = of_update_phandle_property(NULL, PANEL_DTS_NAME, np->name);
				return;
			}
		}
	}
}

static int lcd_driver_is_valid(struct mipi_dsi_lcd_driver *lcd_driver)
{
	if (!lcd_driver)
		return 0;

	if (!lcd_driver->driver.name)
		return 0;

	return 1;
}

static int lcd_driver_need_update_dts(const char *driver_name)
{
	struct device_node *nplcd = NULL, *np = NULL;
	int need_update = 0;

	nplcd = of_find_node_with_property(NULL, PANEL_DTS_NAME);
	if (!nplcd) {
		dbg_info("%s: %s property does not exist\n", __func__, PANEL_DTS_NAME);
		return need_update;
	}

	np = of_parse_phandle(nplcd, PANEL_DTS_NAME, 0);
	if (!np) {
		dbg_info("%s: %s of_parse_phandle fail\n", __func__, PANEL_DTS_NAME);
		return need_update;
	}

	if (!STREQ(np->name, driver_name))
		need_update = 1;

	return need_update;
}

static void __lcd_driver_update_by_id_match(void)
{
	struct mipi_dsi_lcd_driver **p_lcd_driver = &__start___lcd_driver;
	struct mipi_dsi_lcd_driver *lcd_driver = NULL;
	struct device_node *np = NULL;
	int i = 0, count = 0, ret = -EINVAL, do_match = 0;
	unsigned int id_index, mask, expect;
	u32 id_match_info[10] = {0, };

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (lcd_driver && lcd_driver->driver.name) {
			np = of_find_node_by_name(NULL, lcd_driver->driver.name);
			if (np && of_get_property(np, "id_match", NULL)) {
				dbg_info("%s: %dth lcd_driver(%s) has dts id_match property\n", __func__, i, lcd_driver->driver.name);
				do_match++;
			}
		}
	}

	if (!do_match)
		return;

	if (i != do_match)
		of_update_phandle_property(NULL, PANEL_DTS_NAME, __start___lcd_driver->driver.name);

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (!lcd_driver_is_valid(lcd_driver)) {
			dbg_info("%dth lcd_driver is invalid\n", i);
			continue;
		}

		np = of_find_node_by_name(NULL, lcd_driver->driver.name);
		if (!np || !of_get_property(np, "id_match", NULL)) {
			/* dbg_info("%dth lcd_driver(%s) has no id_match property\n", lcd_driver->driver.name); */
			continue;
		}

		count = of_property_count_u32_elems(np, "id_match");
		if (count < 0 || count > ARRAY_SIZE(id_match_info) || count % 2) {
			dbg_info("%s: %dth lcd_driver(%s) has invalid id_match count(%d)\n", __func__, i, lcd_driver->driver.name, count);
			continue;
		}

		memset(id_match_info, 0, sizeof(id_match_info));
		ret = of_property_read_u32_array(np, "id_match", id_match_info, count);
		if (ret < 0) {
			dbg_info("%s: of_property_read_u32_array fail. ret(%d)\n", __func__, ret);
			continue;
		}

		for (id_index = 0; id_index < count; id_index += 2) {
			mask = id_match_info[id_index];
			expect = id_match_info[id_index + 1];

			if ((lcdtype & mask) == (expect & mask)) {
				dbg_info("%s: %dth lcd_driver(%s) id_match. lcdtype(%06X) mask(%06X) expect(%06X)\n",
					__func__, i, lcd_driver->driver.name, lcdtype, mask, expect);

				if (mipi_lcd_driver != lcd_driver)
					mipi_lcd_driver = lcd_driver;

				if (lcd_driver_need_update_dts(lcd_driver->driver.name))
					of_update_phandle_property(NULL, PANEL_DTS_NAME, lcd_driver->driver.name);

				return;
			}
		}
	}
}

static void __lcd_driver_update_by_function(void)
{
	struct mipi_dsi_lcd_driver **p_lcd_driver = &__start___lcd_driver;
	struct mipi_dsi_lcd_driver *lcd_driver = NULL;
	int i = 0, ret = -EINVAL, do_match = 0;

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (lcd_driver && lcd_driver->match) {
			dbg_info("%s: %dth lcd_driver %s and has driver match function\n", __func__, i, lcd_driver->driver.name);
			do_match++;
		}
	}

	if (!do_match)
		return;

	if (i != do_match)
		of_update_phandle_property(NULL, PANEL_DTS_NAME, __start___lcd_driver->driver.name);

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (!lcd_driver_is_valid(lcd_driver)) {
			dbg_info("%dth lcd_driver is invalid\n", i);
			continue;
		}

		if (!lcd_driver->driver.name)
			continue;

		ret = lcd_driver->match(NULL);
		if (ret & NOTIFY_OK) {
			dbg_info("%s: %dth lcd_driver(%s) NOTIFY_OK(%04X)\n", __func__, i, lcd_driver->driver.name, ret);

			if (mipi_lcd_driver != lcd_driver)
				mipi_lcd_driver = lcd_driver;

			if (lcd_driver_need_update_dts(lcd_driver->driver.name))
				of_update_phandle_property(NULL, PANEL_DTS_NAME, lcd_driver->driver.name);

			return;
		}

		if (ret & NOTIFY_STOP_MASK) {
			dbg_info("%s: %dth lcd_driver(%s) NOTIFY_STOP_MASK(%04X)\n", __func__, i, lcd_driver->driver.name, ret);
			return;
		}
	}
}

int lcd_driver_init(void)
{
	struct mipi_dsi_lcd_driver **p_lcd_driver = &__start___lcd_driver;
	struct mipi_dsi_lcd_driver *lcd_driver = NULL;
	int i = 0, ret = -EINVAL;

	__lcd_driver_dts_update();

	if (mipi_lcd_driver && mipi_lcd_driver->driver.name) {
		dbg_info("%s: %s driver is registered\n", __func__, mipi_lcd_driver->driver.name);
		return 0;
	}

	if (++p_lcd_driver == &__stop___lcd_driver) {
		/* dbg_info("%s: lcd_driver is only one\n", __func__); */
		mipi_lcd_driver = __start___lcd_driver;
		return 0;
	}

	__lcd_driver_update_by_id_match();
	__lcd_driver_update_by_function();

	for (i = 0, p_lcd_driver = &__start___lcd_driver; p_lcd_driver < &__stop___lcd_driver; p_lcd_driver++, i++) {
		lcd_driver = *p_lcd_driver;

		if (!lcd_driver || !lcd_driver->driver.name)
			continue;

		dbg_info("%s: %dth lcd_driver is %s\n", __func__, i, lcd_driver->driver.name);
		ret = __lcd_driver_update_by_lcd_info_name(lcd_driver);
	}

	WARN_ON(!mipi_lcd_driver);
	mipi_lcd_driver = mipi_lcd_driver ? mipi_lcd_driver : __start___lcd_driver;

	dbg_info("%s: %s driver is registered\n", __func__, mipi_lcd_driver->driver.name ? mipi_lcd_driver->driver.name : "null");

	return 0;
}

