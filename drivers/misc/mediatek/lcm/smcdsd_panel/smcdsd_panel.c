/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/fb.h>
#include <video/mipi_display.h>

#include "lcm_drv.h"
#include "smcdsd_panel.h"
#include "smcdsd_board.h"
#include "panels/dd.h"

#define dbg_info(fmt, ...)		pr_info(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)
#define dbg_none(fmt, ...)		pr_debug(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
static struct LCM_UTIL_FUNCS lcm_util = {0};

/* --------------------------------------------------------------------------*/
/* Boot Params */
/* --------------------------------------------------------------------------*/
unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	dbg_info("%s: lcdtype: %6X\n", __func__, lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

/* --------------------------------------------------------------------------*/
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------*/
#define PANEL_DTS_NAME	"smcdsd_panel"
static void smcdsd_panel_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

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

		dbg_info("of_property_read_u32_array done for %s(%s)\n", propname, m.buf);
	}
}

static int smcdsd_panel_get_params_from_dt(struct LCM_PARAMS *lcm_params)
{
	struct device_node *np = NULL;
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);
	int ret = 0;
	u32 res[2];

	np = of_find_lcd_info(&plcd->pdev->dev);

	smcdsd_of_property_read_u32(np, "lcm_params-types",
		&lcm_params->type);

	smcdsd_of_property_read_u32(np, "lcm_params-resolution",
		res);
	lcm_params->width = res[0];
	lcm_params->height = res[1];

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
#if 0
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
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-cont_clock",
		&lcm_params->dsi.cont_clock);	

	/* Get MIPI CLK table from DT array */
	smcdsd_of_property_read_u32(np, "lcm_params-dsi-data_rate", plcd->data_rate);
	/* First CLK is default CLK */
	lcm_params->dsi.data_rate = plcd->data_rate[0];
	smcdsd_of_property_read_u32(np, "lcm_params-hbm_enable_wait_frame", &lcm_params->hbm_enable_wait_frame);
	smcdsd_of_property_read_u32(np, "lcm_params-hbm_disable_wait_frame", &lcm_params->hbm_disable_wait_frame);

	return ret;
}

static void smcdsd_panel_get_params(struct LCM_PARAMS *params)
{
	dbg_info("%s +\n", __func__);

	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	smcdsd_panel_get_params_from_dt(params);

	params->dsi.vertical_active_line = params->dsi.vertical_active_line ? params->dsi.vertical_active_line : params->height;
	params->dsi.horizontal_active_pixel = params->dsi.horizontal_active_pixel ? params->dsi.horizontal_active_pixel : params->width;

	params->dsi.PLL_CLOCK = params->dsi.PLL_CLOCK ? params->dsi.PLL_CLOCK : params->dsi.data_rate >> 1;

	dbg_info("%s -\n", __func__);
}

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
struct mipi_dsi_lcd_parent *g_smcdsd;

static void smcdsd_panel_init(void)
{
	dbg_info("%s\n", __func__);
}

static void smcdsd_panel_init_power(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	dbg_info("%s\n", __func__);

	run_list(&plcd->pdev->dev, "panel_power_enable");
}

static void smcdsd_panel_resume_power(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	dbg_info("%s: %d\n", __func__, plcd->power);

	if (plcd->power)
		return;

	plcd->power = 1;
	run_list(&plcd->pdev->dev, "panel_power_enable");

	mutex_lock(&plcd->lock);
	call_drv_ops(panel_i2c_init);
	mutex_unlock(&plcd->lock);
}

static void smcdsd_panel_resume(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	mutex_lock(&plcd->lock);
	call_drv_ops(init);
	mutex_unlock(&plcd->lock);
}

static int smcdsd_panel_display_on(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	mutex_lock(&plcd->lock);
	call_drv_ops(displayon_late);
	mutex_unlock(&plcd->lock);

	return 0;
}

static void smcdsd_panel_reset_disable(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	dbg_info("%s: +\n", __func__);

	run_list(&plcd->pdev->dev, "panel_reset_disable");
}

static void smcdsd_panel_reset_enable(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	dbg_info("%s: +\n", __func__);

	run_list(&plcd->pdev->dev, "panel_reset_enable");
}

static void smcdsd_panel_suspend_power(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	dbg_info("%s: %d\n", __func__, plcd->power);

	if (!plcd->power)
		return;

	plcd->power = 0;

	mutex_lock(&plcd->lock);
	call_drv_ops(panel_i2c_exit);
	mutex_unlock(&plcd->lock);

	run_list(&plcd->pdev->dev, "panel_power_disable");
}

static void smcdsd_panel_suspend(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	mutex_lock(&plcd->lock);
	call_drv_ops(exit);
	mutex_unlock(&plcd->lock);
}

#if defined(CONFIG_SMCDSD_DOZE)
static void smcdsd_panel_aod(int enter)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	mutex_lock(&plcd->lock);
	if (enter)
		call_drv_ops(doze_init);
	else
		call_drv_ops(doze_exit);
	mutex_unlock(&plcd->lock);
}
#endif

static void smcdsd_panel_cmd_q(unsigned int enable)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	dbg_info("%s : %d\n", __func__, enable);

	mutex_lock(&plcd->lock);

	plcd->cmd_q = !!enable;

	mutex_unlock(&plcd->lock);
}

static void mtk_dsi_set_withrawcmdq(void *cmdq, unsigned int *pdata,
		unsigned int queue_size, unsigned char force_update)
{
	lcm_util.dsi_set_cmdq_V11(cmdq, pdata, queue_size, force_update);
}

static int lcm_read_by_cmdq(void *cmdq, unsigned int data_id,
		unsigned int offset, unsigned int cmd, unsigned char *buffer,
		unsigned char size)
{
	return lcm_util.dsi_dcs_read_cmdq_lcm_reg_v2_1(cmdq, data_id, offset, cmd, buffer, size);
}

static void smcdsd_set_backlight(void *handle, unsigned int level)
{
	/* unused. just prevent useless message */
}

static void smcdsd_partial_update(unsigned int x, unsigned int y, unsigned int width,
			unsigned int height)
{
	/* unused. just prevent useless message */
}

static bool smcdsd_set_mask_wait(bool wait)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	plcd->mask_wait = wait;

	return 0;
}

static bool smcdsd_get_mask_wait(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	return plcd->mask_wait;
}

static bool smcdsd_set_mask_cmdq(bool en, void *qhandle)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	plcd->mask_qhandle = qhandle;

	/* HBM CMDQ */
	plcd->cmdq_index = CMDQ_MASK_BRIGHTNESS;

	call_drv_ops(set_mask, !!en);

	plcd->cmdq_index = CMDQ_PRIMARY_DISPLAY;

	return 0;
}

static bool smcdsd_get_mask_state(void)
{
	return call_drv_ops(get_mask);
}
#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
static int smcdsd_get_protos_mode(void)
{
	return call_drv_ops(get_curr_protos_mode);
}
#endif
struct LCM_DRIVER smcdsd_panel_drv = {
	.set_util_funcs = smcdsd_panel_set_util_funcs,
	.get_params = smcdsd_panel_get_params,
	.read_by_cmdq = lcm_read_by_cmdq,
	.dsi_set_withrawcmdq = mtk_dsi_set_withrawcmdq,
	.init = smcdsd_panel_init,
	.suspend = smcdsd_panel_suspend,
	.init_power = smcdsd_panel_init_power,
	.resume = smcdsd_panel_resume,
	.set_display_on = smcdsd_panel_display_on,
	.reset_disable = smcdsd_panel_reset_disable,
	.reset_enable = smcdsd_panel_reset_enable,
	.suspend_power = smcdsd_panel_suspend_power,
	.resume_power = smcdsd_panel_resume_power,
#if defined(CONFIG_SMCDSD_DOZE)
	.aod = smcdsd_panel_aod,
#endif
	.cmd_q = smcdsd_panel_cmd_q,
	.set_backlight_cmdq = smcdsd_set_backlight,
	.update = smcdsd_partial_update,
	.get_hbm_state = smcdsd_get_mask_state,
	.set_hbm_cmdq = smcdsd_set_mask_cmdq,
	.get_hbm_wait = smcdsd_get_mask_wait,
	.set_hbm_wait = smcdsd_set_mask_wait,
#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
	.get_protos_mode = smcdsd_get_protos_mode,
#endif
};

static unsigned char para_list[259];
int smcdsd_panel_dsi_command_rx(unsigned int id, unsigned int offset, u8 cmd, unsigned int len, u8 *buf)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);
	int ret = 0;

	if (!plcd->lcdconnected)
		return ret;

	if (unlikely(!lcm_util.dsi_dcs_read_lcm_reg_v2))
		pr_err("%s: dsi_dcs_read_lcm_reg_v2 is not exist\n", __func__);

	ret = lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buf, len);

	return ret;
}

int smcdsd_panel_dsi_command_tx(unsigned int id, unsigned long data0, unsigned int data1)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);
	int ret = 0;

	if (!plcd->lcdconnected)
		return ret;

	dsi_write_data_dump(id, data0, data1);

	if (unlikely(!lcm_util.dsi_set_cmdq_V2))
		pr_err("%s: dsi_set_cmdq_V2 is not exist\n", __func__);

	switch (id) {
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	{
		lcm_util.dsi_set_cmdq_V2((unsigned int)data0, 0, NULL, 1);
		break;
	}
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	{
		para_list[0] = data1;
		lcm_util.dsi_set_cmdq_V2((unsigned int)data0, 1, &para_list[0], 1);
		break;
	}

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	{
		memcpy(&para_list[0], (unsigned char *)data0, data1);
		lcm_util.dsi_set_cmdq_V2((unsigned int)para_list[0], data1-1, &para_list[1], 1);
		break;
	}

	default:
		dbg_info("%s: id(%2x) is not supported\n", __func__, id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

struct mipi_dsi_lcd_driver *smcdsd_driver_list[] = {
#if defined(CONFIG_SMCDSD_TD4150_A01CORE)
	&td4150_mipi_lcd_driver,
#endif

#if defined(CONFIG_SMCDSD_NT36525B_A01CORE)
	&nt36525b_mipi_lcd_driver,
#endif
};
#if defined(CONFIG_SMCDSD_PROTOS_PLUS)
int smcdsd_set_doze(enum doze_mode mode)
{
	int ret = 0;

	dbg_info("%s + doze_mode: %s\n", __func__, mode ? "DOZE_SUSPEND" : "DOZE");

	if (mode == DOZE)
		lcm_util.set_aod(0);
	else if (mode == DOZE_SUSPEND)
		lcm_util.set_aod(1);

	return ret;
}
#endif
static int smcdsd_probe(struct platform_device *pdev)
{
	struct mipi_dsi_lcd_parent *plcd = NULL;
	struct mipi_dsi_lcd_driver *lcd_drv = NULL;
	unsigned int i = 0;
	int ret = 0;
	char *lcm_name;
	unsigned int smcdsd_lcm_count = sizeof(smcdsd_driver_list) / sizeof(struct mipi_dsi_lcd_driver *);

	lcm_name = mtkfb_find_lcm_driver();

	dbg_info("%s: node_full_name: %s smcdsd_lcm_count =%d \n", __func__, of_node_full_name(pdev->dev.of_node),smcdsd_lcm_count);

	plcd = kzalloc(sizeof(struct mipi_dsi_lcd_parent), GFP_KERNEL);

	plcd->pdev = pdev;
	plcd->cmd_q = 0;
	plcd->power = 1;
	plcd->tx = smcdsd_panel_dsi_command_tx;
	plcd->rx = smcdsd_panel_dsi_command_rx;
	mutex_init(&plcd->lock);

	for (i = 0; i < smcdsd_lcm_count;  i++) {
		lcd_drv = smcdsd_driver_list[i];
		dbg_info("%s: checking driver name (%s) with (%s)\n", __func__, lcd_drv->driver.name,lcm_name);
		if (lcd_drv && !strcmp(lcd_drv->driver.name,lcm_name)) {
			dbg_info("%s: %s\n", __func__, lcd_drv->driver.name);
			plcd->drv = lcd_drv;
			smcdsd_panel_drv.name = kstrdup(lcd_drv->driver.name, GFP_KERNEL);
			break;
		}
	}

	if (plcd->drv)
		dbg_info("%s: driver is found(%s)\n", __func__, plcd->drv->driver.name);

	ret = platform_device_add_data(pdev, plcd, sizeof(struct mipi_dsi_lcd_parent));
	if (ret < 0) {
		kfree(plcd);
		dbg_info("%s: platform_device_add_data fail(%d)\n", __func__, ret);
		return ret;
	}

	g_smcdsd = dev_get_platdata(&pdev->dev);

	kfree(plcd);

	return 0;
}

static const struct of_device_id smcdsd_panel_device_table[] = {
	{ .compatible = "samsung,mtk-dsi-panel", },
	{},
};

static struct platform_driver smcdsd_panel_driver = {
	.driver = {
		.name = "panel",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(smcdsd_panel_device_table),
		.suppress_bind_attrs = true,
	},
	.probe = smcdsd_probe,
};

static int __init smcdsd_module_init(void)
{
	return platform_driver_register(&smcdsd_panel_driver);
}
module_init(smcdsd_module_init);

static int __init smcdsd_late_initcall(void)
{
	struct mipi_dsi_lcd_parent *plcd = get_smcdsd_drvdata(0);

	if (!plcd->probe) {
		lcdtype = lcdtype ? lcdtype : islcmconnected;
		dbg_info("%s: islcmconnected %d\n", __func__, islcmconnected);
		call_drv_ops(probe);
		plcd->probe = 1;
	}

	return 0;
}
late_initcall(smcdsd_late_initcall);