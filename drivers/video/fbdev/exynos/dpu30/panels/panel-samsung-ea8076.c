/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung ea8076 Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <video/mipi_display.h>
#include "exynos_panel_drv.h"
#include "../dsim.h"

int ea8076_suspend(struct exynos_panel_device *panel)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);

	mutex_lock(&panel->ops_lock);
	dsim_write_data_seq_delay(dsim, 20, 0x28, 0x00, 0x00);
	dsim_write_data_seq_delay(dsim, 120, 0x10, 0x00, 0x00);
	mutex_unlock(&panel->ops_lock);

	return 0;
}

static int ea8076_cabc_mode_unlocked(int mode)
{
	int ret = 0;
	int count;
	unsigned char buf[] = {0x0, 0x0};
	unsigned char SEQ_CABC_CMD[] = {0x55, 0x00, 0x00};
	unsigned char cmd = MIPI_DCS_WRITE_POWER_SAVE; /* 0x55 */
	struct dsim_device *dsim = get_dsim_drvdata(0);

	DPU_DEBUG_PANEL("%s: CABC mode[%d] write/read\n", __func__, mode);

	switch (mode) {
	/* read */
	case POWER_MODE_READ:
		cmd = MIPI_DCS_GET_POWER_SAVE; /* 0x56 */
		ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, cmd, 0x1, buf);
		if (ret < 0) {
			DPU_ERR_PANEL("CABC REG(0x%02X) read failure!\n", cmd);
			count = 0;
		} else {
			DPU_INFO_PANEL("CABC REG(0x%02X) read success: 0x%02x\n",
				cmd, *(unsigned int *)buf & 0xFF);
			count = 1;
		}
		return count;

	/* write */
	case POWER_SAVE_OFF:
		SEQ_CABC_CMD[1] = CABC_OFF;
		break;
	case POWER_SAVE_LOW:
		SEQ_CABC_CMD[1] = CABC_USER_IMAGE;
		break;
	case POWER_SAVE_MEDIUM:
		SEQ_CABC_CMD[1] = CABC_STILL_PICTURE;
		break;
	case POWER_SAVE_HIGH:
		SEQ_CABC_CMD[1] = CABC_MOVING_IMAGE;
		break;
	default:
		DPU_ERR_PANEL("Unavailable CABC mode(%d)!\n", mode);
		return -EINVAL;
	}

	dsim_write_data_table(dsim, SEQ_CABC_CMD);

	return ret;
}

static int ea8076_cabc_mode(struct exynos_panel_device *panel, int mode)
{
	int ret = 0;

	if (!panel->cabc_enabled) {
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&panel->ops_lock);
	ea8076_cabc_mode_unlocked(mode);
	mutex_unlock(&panel->ops_lock);
	return ret;
}

int ea8076_displayon(struct exynos_panel_device *panel)
{
	struct dsim_device *dsim = get_dsim_drvdata(0);

	mutex_lock(&panel->ops_lock);

	/* Sleep Out (11h) */
	dsim_write_data_seq_delay(dsim, 20, 0x11);

	/* Common Setting */
	/* PAGE ADDRESS SET */
	dsim_write_data_seq_delay(dsim, 12, 0x2B, 0x00, 0x00, 0x09, 0x23);

	/* Testkey Enable */
	dsim_write_data_seq(dsim, false, 0xF0, 0x5A, 0x5A);
	dsim_write_data_seq(dsim, false, 0xFC, 0x5A, 0x5A);

	/* Ignoring dealing with ERR in DDI IC
	 *  This is setting for ignoring dealing with error in DDI IC
	 * Without this setting, Displaying is not normal.
	 * so DDI vendor recommended us to use this setting temporally.
	 * After launching official project, DDI vendor need to check this again.
	 */
	dsim_write_data_seq_delay(dsim, 12, 0xDD, 0x4A, 0x00);

	/* FFC SET */
	dsim_write_data_seq_delay(dsim, 12, 0xE9, 0x11, 0x55, 0x98, 0x96, 0x80,
			0xB2, 0x41, 0xC3, 0x00, 0x1A,
			0xB8  /* MIPI Speed 1.2Gbps */ );

	/* ERR FG SET */
	dsim_write_data_seq_delay(dsim, 12, 0xE1, 0x00, 0x00, 0x02, 0x10, 0x10,
			0x10, 0x00, 0x00, 0x20, 0x00, 0x00, 0x01, 0x19);
	/* VSYNC SET */
	dsim_write_data_seq_delay(dsim, 12, 0xE0, 0x01);
	/* ASWIRE_OFF */
	dsim_write_data_seq_delay(dsim, 12, 0xD5, 0x00, 0x01, 0x19,/* 10th para 0x00 FD Normal */
			0x00);

	/* ACL SETTING 1/2 */
	dsim_write_data_seq_delay(dsim, 12, 0xB0, 0xD7);
	dsim_write_data_seq_delay(dsim, 12, 0xB9, 0x02, 0xA1, 0x8C, 0x4B);

	/* Brightness Setting */
	/* HBM OFF */
	dsim_write_data_seq_delay(dsim, 12, 0x53, 0x20, 0x00);
	/* ELVSS_SET */
	dsim_write_data_seq_delay(dsim, 12, 0xB7, 0x01, 0x53, 0x28,
			0x4D, 0x00, 0x90, 0x04	/* ELVSS Return */);
	/* BRIGHTNESS */
	dsim_write_data_seq_delay(dsim, 12, 0x51, 0x01, 0xBD);
	/* ACL OFF */
	dsim_write_data_seq_delay(dsim, 12, 0x55, 0x00);

	/* TE on */
	dsim_write_data_seq_delay(dsim, 110, 0x35, 0x00, 0x00);

	/* display on */
	dsim_write_data_seq(dsim, false, 0x29);
	mutex_unlock(&panel->ops_lock);

	return 0;
}

int ea8076_mres(struct exynos_panel_device *panel, int mres_idx)
{
	return 0;
}

int ea8076_doze(struct exynos_panel_device *panel)
{
	return 0;
}

int ea8076_doze_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

int ea8076_dump(struct exynos_panel_device *panel)
{
	return 0;
}

int ea8076_read_state(struct exynos_panel_device *panel)
{
	return 0;
}

struct exynos_panel_ops panel_ea8076_ops = {
	.id		= {0x00034090, 0xffffff, 0xffffff},
	.suspend	= ea8076_suspend,
	.displayon	= ea8076_displayon,
	.mres		= ea8076_mres,
	.doze		= ea8076_doze,
	.doze_suspend	= ea8076_doze_suspend,
	.dump		= ea8076_dump,
	.read_state	= ea8076_read_state,
	.set_cabc_mode	= ea8076_cabc_mode,
};
