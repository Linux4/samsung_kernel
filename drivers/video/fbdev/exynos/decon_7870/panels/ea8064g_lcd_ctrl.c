/*
 * drivers/video/decon_7580/panels/ea8064g_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "ea8064g_param.h"

#include <video/mipi_display.h>
#include "../dsim.h"

static int dsim_write_hl_data(u32 id, const u8 *cmd, u32 cmdSize)
{
	int ret;
	int retry;

	retry = 5;

try_write:
	if (cmdSize == 1)
		ret = dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, cmd[0], 0);
	else if (cmdSize == 2)
		ret = dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd[0], cmd[1]);
	else
		ret = dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)cmd,
cmdSize);

	if (ret != 0) {
		if (--retry)
			goto try_write;
		else
			dsim_err("dsim write failed,  cmd : %x\n", cmd[0]);
	}
	return ret;
}

void lcd_enable(void)
{
	int id = 0;
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(id, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
}

void lcd_disable(struct dsim_device *dsim)
{
	/* This function needs to implement */
}

void lcd_init(struct decon_lcd *lcd)
{
	int id = 0;
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	/* set_brightness */
	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(id, SEQ_BRIGHTNESS_1, ARRAY_SIZE(SEQ_BRIGHTNESS_1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_BRIGHTNESS_1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(id, SEQ_BRIGHTNESS_2, ARRAY_SIZE(SEQ_BRIGHTNESS_2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_BRIGHTNESS_2\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(id, SEQ_BRIGHTNESS_3, ARRAY_SIZE(SEQ_BRIGHTNESS_3));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_BRIGHTNESS_3\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(id, SEQ_GAMMA_UPDATE_EA8064G, ARRAY_SIZE(SEQ_GAMMA_UPDATE_EA8064G));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE_EA8064G\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_F1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

	/* panel init */
	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(id, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(25);

	ret = dsim_write_hl_data(id, SEQ_DCDC1_GP, ARRAY_SIZE(SEQ_DCDC1_GP));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SOURCE_CONTROL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(id, SEQ_DCDC1_SET, ARRAY_SIZE(SEQ_DCDC1_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SOURCE_CONTROL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(id, SEQ_SOURCE_CONTROL, ARRAY_SIZE(SEQ_SOURCE_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SOURCE_CONTROL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(id, SEQ_PCD_CONTROL, ARRAY_SIZE(SEQ_PCD_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_PCD_CONTROL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

	msleep(120);

	ret = dsim_write_hl_data(id, SEQ_TE_OUT, ARRAY_SIZE(SEQ_TE_OUT));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TE_OUT\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(id, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return;
}
