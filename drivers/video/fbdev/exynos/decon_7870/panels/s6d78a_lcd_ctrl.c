/* s6d78a_lcd_ctrl.c
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

#include "s6d78a_param.h"
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

#define ID		0

void lcd_init(struct decon_lcd * lcd)
{
	/* Initializing  Sequence(1-1) */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT1_1,
		ARRAY_SIZE(SEQ_INIT1_1)) == -1)
		dsim_err("failed to send init_seq1_level_1_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT1_2,
		ARRAY_SIZE(SEQ_INIT1_2)) == -1)
		dsim_err("failed to send init_seq1_level_2_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT1_3,
		ARRAY_SIZE(SEQ_INIT1_3)) == -1)
		dsim_err("failed to send init_seq1_level_3_command.\n");

	/* sleep out */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_SLEEP_OUT,
		ARRAY_SIZE(SEQ_SLEEP_OUT)) == -1)
		dsim_err("failed to send sleep_exit_command.\n");

	/* 120ms delay */
	msleep(120);

	/* Initializing  Sequence(3) */
#if 0
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_INT_CLK,
		ARRAY_SIZE(SEQ_INIT3_INT_CLK)) == -1)
		dsim_err("failed to send internal_clk_command.\n");
#endif
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_PAN_PROTECT,
		ARRAY_SIZE(SEQ_INIT3_PAN_PROTECT)) == -1)
		dsim_err("failed to send panel_protection_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_INT_POW,
		ARRAY_SIZE(SEQ_INIT3_INT_POW)) == -1)
		dsim_err("failed to send internal_power_seq_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_GOA,
		ARRAY_SIZE(SEQ_INIT3_GOA)) == -1)
		dsim_err("failed to send goa_timing_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_INT_PORCH,
		ARRAY_SIZE(SEQ_INIT3_INT_PORCH)) == -1)
		dsim_err("failed to send internal_porch_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_SRC_CONT,
		ARRAY_SIZE(SEQ_INIT3_SRC_CONT)) == -1)
		dsim_err("failed to send source_control_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_MIPI_ABNORMAL,
		ARRAY_SIZE(SEQ_INIT3_MIPI_ABNORMAL)) == -1)
		dsim_err("failed to send mipi_abnormal_detect_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_MIPI_AUTO_RECOVERY,
		ARRAY_SIZE(SEQ_INIT3_MIPI_AUTO_RECOVERY)) == -1)
		dsim_err("failed to send mipi_auto_recover_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_GOA_OUTPUT,
		ARRAY_SIZE(SEQ_INIT3_GOA_OUTPUT)) == -1)
		dsim_err("failed to send goa_output_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT3_MEM_ACC_CONT,
		ARRAY_SIZE(SEQ_INIT3_MEM_ACC_CONT)) == -1)
		dsim_err("failed to memory_data_access_command.\n");

	/* Gamma Setting Sequence(4) */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_GAMMA_POSITIVE_CONT,
		ARRAY_SIZE(SEQ_GAMMA_POSITIVE_CONT)) == -1)
		dsim_err("failed to send positive_gama_control_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_GAMMA_NEGATIVE_CONT,
		ARRAY_SIZE(SEQ_GAMMA_NEGATIVE_CONT)) == -1)
		dsim_err("failed to send negative_gama_control_command.\n");

	/* display_on */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_DISPLAY_ON,
		ARRAY_SIZE(SEQ_DISPLAY_ON)) == -1)
		dsim_err("failed to display_on_command.\n");

	/* 20ms delay */
	msleep(20);

	/* Initializing  Sequence(2) */
	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT2_1,
		ARRAY_SIZE(SEQ_INIT2_1)) == -1)
		dsim_err("failed to send init_seq2_level_1_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT2_2,
		ARRAY_SIZE(SEQ_INIT2_2)) == -1)
		dsim_err("failed to send init_seq2_level_2_command.\n");

	while(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int) SEQ_INIT2_3,
		ARRAY_SIZE(SEQ_INIT2_3)) == -1)
		dsim_err("failed to send init_seq2_level_3_command.\n");
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
