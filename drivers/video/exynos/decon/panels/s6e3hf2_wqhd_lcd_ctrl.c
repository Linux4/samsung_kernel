/*
 * drivers/video/decon/panels/s6e3hf2_wqhd_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s6e3hf2_wqhd_param.h"
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

/* Porch values. It depends on command or video mode */
#define S6E3HF2_WQHD_CMD_VBP	15
#define S6E3HF2_WQHD_CMD_VFP	1
#define S6E3HF2_WQHD_CMD_VSA	1
#define S6E3HF2_WQHD_CMD_HBP	1
#define S6E3HF2_WQHD_CMD_HFP	1
#define S6E3HF2_WQHD_CMD_HSA	1

/* These need to define */
#define S6E3HF2_WQHD_VIDEO_VBP	15
#define S6E3HF2_WQHD_VIDEO_VFP	1
#define S6E3HF2_WQHD_VIDEO_VSA	1
#define S6E3HF2_WQHD_VIDEO_HBP	20
#define S6E3HF2_WQHD_VIDEO_HFP	20
#define S6E3HF2_WQHD_VIDEO_HSA	20

#define S6E3HF2_WQHD_HORIZONTAL	1440
#define S6E3HF2_WQHD_VERTICAL	2560

#ifdef FW_TEST /* This information is moved to DT */
#define CONFIG_FB_I80_COMMAND_MODE

struct decon_lcd s6e3hf2_lcd_info = {
#ifdef CONFIG_FB_I80_COMMAND_MODE
	.mode = DECON_MIPI_COMMAND_MODE,
	.vfp = S6E3HF2_WQHD_CMD_VFP,
	.vbp = S6E3HF2_WQHD_CMD_VBP,
	.hfp = S6E3HF2_WQHD_CMD_HFP,
	.hbp = S6E3HF2_WQHD_CMD_HBP,
	.vsa = S6E3HF2_WQHD_CMD_VSA,
	.hsa = S6E3HF2_WQHD_CMD_HSA,
#else
	.mode = DECON_VIDEO_MODE,
	.vfp = S6E3HF2_WQHD_VIDEO_VFP,
	.vbp = S6E3HF2_WQHD_VIDEO_VBP,
	.hfp = S6E3HF2_WQHD_VIDEO_HFP,
	.hbp = S6E3HF2_WQHD_VIDEO_HBP,
	.vsa = S6E3HF2_WQHD_VIDEO_VSA,
	.hsa = S6E3HF2_WQHD_VIDEO_HSA,
#endif
	.xres = S6E3HF2_WQHD_HORIZONTAL,
	.yres = S6E3HF2_WQHD_VERTICAL,

	/* Maybe, width and height will be removed */
	.width = 70,
	.height = 121,

	/* Mhz */
	.hs_clk = 1100,
	.esc_clk = 20,

	.fps = 60,
	.mic_enabled = 1,
	.mic_ver = MIC_VER_1_2,
};
#endif

/*
 * 3HF2 lcd init sequence
 *
 * Parameters
 *	- mic_enabled : if mic is enabled, MIC_ENABLE command must be sent
 *	- mode : LCD init sequence depends on command or video mode
 */

void lcd_init(int id, struct decon_lcd *lcd)
{
	/* 7. Sleep Out(11h) */
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, SEQ_SLEEP_OUT[0], 0) < 0)
		dsim_err("fail to write sleep out command.\n");

	msleep(5);

	/* 9. Interface Setting */
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("fail to write F0 on command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)SEQ_SINGLE_DSI_1[0],
			(unsigned int)SEQ_SINGLE_DSI_1[1]) < 0)
		dsim_err("fail to write single dis 1 command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)SEQ_SINGLE_DSI_2[0],
			(unsigned int)SEQ_SINGLE_DSI_2[1]) < 0)
		dsim_err("fail to write single dis 1 command.\n");

	msleep(120);
	/* Common Setting */
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)SEQ_TE_ON[0],
			(unsigned int)SEQ_TE_ON[1]) < 0)
		dsim_err("fail to write single dis 1 command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TSP_TE,
			ARRAY_SIZE(SEQ_TSP_TE)) < 0)
		dsim_err("fail to write tsp te command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_PENTILE_SETTING,
			ARRAY_SIZE(SEQ_PENTILE_SETTING)) < 0)
		dsim_err("fail to write pentile setting 1 command.\n");

	/* POC setting */
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_FC,
			ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("fail to write test on fc command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_POC_SETTING1,
			ARRAY_SIZE(SEQ_POC_SETTING1)) < 0)
		dsim_err("fail to write poc setting 1 command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)SEQ_POC_SETTING2[0],
			(unsigned int)SEQ_POC_SETTING2[1]) < 0)
		dsim_err("fail to write poc setting 2 command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)SEQ_POC_SETTING3[0],
			(unsigned int)SEQ_POC_SETTING3[1]) < 0)
		dsim_err("fail to write poc setting 3 command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_OFF_FC,
			ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
		dsim_err("fail to write key off fc command.\n");

	/* PCD setting */
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_PCD_SETTING,
			ARRAY_SIZE(SEQ_PCD_SETTING)) < 0)
		dsim_err("fail to write pcd setting command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)SEQ_ERR_FG_SETTING[0],
			(unsigned int)SEQ_ERR_FG_SETTING[1]) < 0)
		dsim_err("fail to write fg setting command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TE_START_SETTING,
			ARRAY_SIZE(SEQ_TE_START_SETTING)) < 0)
		dsim_err("fail to write te start command.\n");

	/* Brightness Setting */
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_GAMMA_CONDITION_SET,
			ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET)) < 0)
		dsim_err("fail to write gamma condition set command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_AOR_CONTROL,
			ARRAY_SIZE(SEQ_AOR_CONTROL)) < 0)
		dsim_err("fail to write aor control command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_ELVSS_SET,
			ARRAY_SIZE(SEQ_ELVSS_SET)) < 0)
		dsim_err("fail to write elvss set command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_GAMMA_UPDATE,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("fail to write gamma update command.\n");

	/* ALC Setting */
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_ACL_OFF,
			ARRAY_SIZE(SEQ_ACL_OFF)) < 0)
		dsim_err("fail to write acl off command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)SEQ_ACL_OFF_OPR[0],
			(unsigned int)SEQ_ACL_OFF_OPR[1]) < 0)
		dsim_err("fail to write acl off opr command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)SEQ_HBM_OFF[0],
			(unsigned int)SEQ_HBM_OFF[1]) < 0)
		dsim_err("fail to write hbm off command.\n");


	/*Temp Compensation Setting*/
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TSET_GLOBAL,
			ARRAY_SIZE(SEQ_TSET_GLOBAL)) < 0)
		dsim_err("fail to write test global command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TSET,
			ARRAY_SIZE(SEQ_TSET)) < 0)
		dsim_err("fail to write tset command.\n");
	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_OFF_F0,
			ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("fail to write test key off f0 command.\n");

}

void lcd_enable(int id)
{
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, SEQ_DISPLAY_ON[0], 0) < 0)
		dsim_err("fail to write DISPLAY_ON command.\n");
}

void lcd_disable(int id)
{
	/* This function needs to implement */
}

/*
 * Set gamma values
 *
 * Parameter
 *	- backlightlevel : It is from 0 to 26.
 */
int lcd_gamma_ctrl(int id, u32 backlightlevel)
{
/* This will be implemented
	int ret;
	ret = dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (u32)gamma22_table[backlightlevel],
			GAMMA_PARAM_SIZE);
	if (ret) {
		dsim_err("fail to write gamma value.\n");
		return ret;
	}
*/
	return 0;
}

int lcd_gamma_update(int id)
{
/* This will be implemented
	int ret;
	ret = dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (u32)SEQ_GAMMA_UPDATE,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret) {
		dsim_err("fail to update gamma value.\n");
		return ret;
	}
*/
	return 0;
}
