/*
 * linux/drivers/video/fbdev/exynos/panel/dummy_lcd/dummy_lcd_panel.h
 *
 * Header file for DUMMY_LCD Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DUMMY_LCD_V320_PANEL_H__
#define __DUMMY_LCD_V320_PANEL_H__
#include "../panel_drv.h"
#include "dummy_lcd.h"

#include "dummy_lcd_v320_panel_dimming.h"
#include "dummy_lcd_v320_panel_i2c.h"

#undef __pn_name__
#define __pn_name__	v320

#undef __PN_NAME__
#define __PN_NAME__	V320

static struct seqinfo v320_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [DUMMY_LCD READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [DUMMY_LCD RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [DUMMY_LCD MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 v320_brt_table[DUMMY_TOTAL_NR_LUMINANCE][2] = {
	{0x64, 0x0B}, /* DEFAULT */
};


static struct maptbl v320_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(v320_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [DUMMY_LCD COMMAND TABLE] ============================== */
/* ===================================================================================== */

static DEFINE_PKTUI(v320_brightness, &v320_maptbl[BRT_MAPTBL], 1);

static void *v320_init_cmdtbl[] = {
};

static void *v320_res_init_cmdtbl[] = {
};

static void *v320_set_bl_cmdtbl[] = {
};

static void *v320_display_on_cmdtbl[] = {
};

static void *v320_display_off_cmdtbl[] = {
};

static void *v320_exit_cmdtbl[] = {
};

static struct seqinfo v320_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", v320_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", v320_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", v320_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", v320_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", v320_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", v320_exit_cmdtbl),
};

struct common_panel_info dummy_lcd_default_panel_info = {
	.ldi_name = "dummy_lcd",	
	.name = "dummy_lcd_default",
	.model = "dummy_0_inch",
	.vendor = "dummy",
	.id = 0xffffff,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
		//.delay_cmd_pre = 0x00,
		//.delay_duration_pre = 1,
	},
	.maptbl = v320_maptbl,
	.nr_maptbl = ARRAY_SIZE(v320_maptbl),
	.seqtbl = v320_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(v320_seqtbl),
	.panel_dim_info = {
		&dummy_lcd_v320_panel_dimming_info,
	},
	.i2c_data = &dummy_lcd_v320_i2c_data,
};

static int __init dummy_lcd_panel_init(void)
{
	register_common_panel(&dummy_lcd_default_panel_info);
	return 0;
}
arch_initcall(dummy_lcd_panel_init)
#endif /* __DUMMY_LCD_V320_PANEL_H__ */


