/*
 * drivers/video/decon_3475/panels/s6e3ha2k_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * Manseok Kim, <Manseoks.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s6e8aa0_param.h"
#include "lcd_ctrl.h"

/* use FW_TEST definition when you test CAL on firmware */
/* #define FW_TEST */
#ifdef FW_TEST
#include "../dsim_fw.h"
#include "mipi_display.h"
#else
#include "../dsim.h"
#include <video/mipi_display.h>
#endif

#define VIDEO_MODE      1
#define COMMAND_MODE    0

#define ID		0

struct decon_lcd s6e8aa0_lcd_info = {
	/* Only availaable VIDEO MODE */
	.mode = VIDEO_MODE,

	.vfp = 0x1,
	.vbp = 0xD,
	.hfp = 0x18,
	.hbp = 0x18,

	.vsa = 0x02,
	.hsa = 0x02,

	.xres = 800,
	.yres = 1280,

	.width = 71,
	.height = 114,

	/* Mhz */
	.hs_clk = 480,
	.esc_clk = 20,

	.fps = 60,
};

struct decon_lcd *decon_get_lcd_info(void)
{
	return &s6e8aa0_lcd_info;
}

void lcd_init(struct decon_lcd * lcd)
{
	/*level1_command (to be added)*/
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) apply_level_1_key,
		ARRAY_SIZE(apply_level_1_key)) == -1)
		dsim_err("failed to send apply_level_1_key_command.\n");

	/*level2_command*/
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) apply_level_2_key,
		ARRAY_SIZE(apply_level_2_key)) == -1)
		dsim_err("failed to send apply_level_2_key_command.\n");

	/*sleep out */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) sleep_out,
		ARRAY_SIZE(sleep_out)) == -1)
		dsim_err("failed to send sleep_out_command.\n");

	/*5ms delay */
	msleep(5);

	/*panel_condition_set */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) panel_condition_set,
		ARRAY_SIZE(panel_condition_set)) == -1)
		dsim_err("failed to send panel_condition_set_command.\n");

	/*panel_condition_update */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) panel_condition_update,
		ARRAY_SIZE(panel_condition_update)) == -1)
		dsim_err("failed to send panel_condition_update_command.\n");

	/*gamma_condition_set */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) gamma_condition_set,
		ARRAY_SIZE(gamma_condition_set)) == -1)
		dsim_err("failed to send gamma_condition_set_command.\n");

	/*gamma_condition_update */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) gamma_update,
		ARRAY_SIZE(gamma_update)) == -1)
		dsim_err("failed to send gamma_update_command.\n");

	/*source control */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) etc_set_source_ctrl,
		ARRAY_SIZE(etc_set_source_ctrl)) == -1)
		dsim_err("failed to etc_set_source_ctrl_command.\n");

	/*pentile control */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) etc_set_pentile_ctrl,
		ARRAY_SIZE(etc_set_pentile_ctrl)) == -1)
		dsim_err("failed to send etc_set_pentile_ctrl_command.\n");

	/*nvm setting */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) elvss_NVM_set,
		ARRAY_SIZE(elvss_NVM_set)) == -1)
		dsim_err("failed to send elvss_NVM_set_command.\n");

	/*power control */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) etc_set_power_ctrl,
		ARRAY_SIZE(etc_set_power_ctrl)) == -1)
		dsim_err("failed to send etc_set_power_ctrl_command.\n");

	/*dynamic elvss control */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) elvss_ctrl_set,
		ARRAY_SIZE(elvss_ctrl_set)) == -1)
		dsim_err("failed to send dyanmic_elvss_ctrl_set_command.\n");

	/*acl control1 */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) acl_ctrl1,
		ARRAY_SIZE(acl_ctrl1)) == -1)
		dsim_err("failed to send acl_ctrl1_command.\n");

	/*acl control2 */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) acl_ctrl2,
		ARRAY_SIZE(acl_ctrl2)) == -1)
		dsim_err("failed to send acl_ctrl2_command.\n");

	/* 120ms delay */
	msleep(120);

	/* display on */
	dsim_wr_data(ID, MIPI_DSI_DCS_SHORT_WRITE,
		0x29, 0);
}


void lcd_enable(void)
{
}

void lcd_disable(void)
{
}

int lcd_gamma_ctrl(u32 backlightlevel)
{
	return 0;
}

int lcd_gamma_update(void)
{
	return 0;
}
