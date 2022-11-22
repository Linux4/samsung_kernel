/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung S6E3HA9 Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <video/mipi_display.h>
#include "exynos_panel_drv.h"
#include "../dsim.h"

static int s6e3fa7_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa7_displayon(struct exynos_panel_device *panel)
{
	struct dsim_device *dsim = get_dsim_drvdata(1);

	DPU_INFO_PANEL("%s +\n", __func__);

	mutex_lock(&panel->ops_lock);
	dsim_write_data_seq(dsim, false, 0xf0, 0x5a, 0x5a);
	dsim_write_data_seq(dsim, false, 0xfc, 0x5a, 0x5a);

	/* sleep out: 120ms delay */
	dsim_write_data_seq_delay(dsim, 120, 0x11);

	/* resolusion setting 720 * 1680 */
	dsim_write_data_seq(dsim, false, 0x2a, 0x00, 0x00, 0x02, 0xcf);
	dsim_write_data_seq(dsim, false, 0x2b, 0x00, 0x00, 0x06, 0x8f);

	/* TE on */
	dsim_write_data_seq(dsim, false, 0x35);

	/* ERR_FG */
	dsim_write_data_seq(dsim, false, 0xED, 0x44);

	/* AVC 2.0 */
	dsim_write_data_seq(dsim, false, 0xBB, 0x1e, 0x07, 0x3a, 0x9f, 0x0f, 0x13,
			0xc0);

	/* TSP SYNC */
	dsim_write_data_seq(dsim, false, 0xB9, 0x00, 0x00, 0x14, 0x00, 0x18, 0x00,
			0x00, 0x00, 0x00, 0x11, 0x03, 0x02, 0x40, 0x02, 0x40);

	/* FFC for 897.6Mbps */
	dsim_write_data_seq(dsim, false, 0xC5, 0x09, 0x10, 0xC8, 0x25, 0x36, 0x13,
			0x26, 0xE6, 0x24, 0xD2, 0x7A, 0x77, 0x73, 0x00, 0xFF, 0x78,
			0x8B, 0x08, 0x00);

	/* display on */
	dsim_write_data_seq(dsim, false, 0x29);
	mutex_unlock(&panel->ops_lock);

	DPU_INFO_PANEL("%s -\n", __func__);
	return 0;
}

static int s6e3fa7_mres(struct exynos_panel_device *panel, int mres_idx)
{
	return 0;
}

static int s6e3fa7_doze(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa7_doze_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa7_dump(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa7_read_state(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa7_set_light(struct exynos_panel_device *panel, u32 br_val)
{
	u8 data[2] = {0, };
	struct dsim_device *dsim = get_dsim_drvdata(0);

	DPU_DEBUG_PANEL("%s +\n", __func__);

	mutex_lock(&panel->ops_lock);

	DPU_INFO_PANEL("requested brightness value = %d\n", br_val);
#if 1
	/* 8-bit : BCCTL(B1h) 48th - D5(BIT_EXT_SEL) = 1 {[7:0]<<8 | [7:0]} */
	data[0] = br_val;
	dsim_write_data_seq(dsim, false, 0x51, data[0]);
#else
	/* 16-bit : BCCTL(B1h) 48th - D5(BIT_EXT_SEL) = 0 */
	DPU_DEBUG_PANEL("(I: 8bit) br_val = %d\n", br_val);
	br_val = (br_val << 8) | (br_val & 0x03);
	DPU_DEBUG_PANEL("(O: 16bit) br_val = %d\n", br_val);

	/* WRDISBV: 1st DBV[7:0], 2nd DBV[15:8] */
	data[0] = (br_val >> 0) & 0xFF;
	data[1] = (br_val >> 8) & 0xFF;
	dsim_write_data_seq(dsim, false, 0x51, data[0], data[1]);
#endif

	mutex_unlock(&panel->ops_lock);

	DPU_DEBUG_PANEL("%s -\n", __func__);
	return 0;
}

struct exynos_panel_ops panel_s6e3fa7_ops = {
	.id		= {0xdddddd},
	.suspend	= s6e3fa7_suspend,
	.displayon	= s6e3fa7_displayon,
	.mres		= s6e3fa7_mres,
	.doze		= s6e3fa7_doze,
	.doze_suspend	= s6e3fa7_doze_suspend,
	.dump		= s6e3fa7_dump,
	.read_state	= s6e3fa7_read_state,
	.set_light	= s6e3fa7_set_light,
};
