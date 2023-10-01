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

static unsigned char SEQ_PPS_SLICE2[] = {
	// WQHD+ :1440x3040
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0B, 0xE0,
	0x05, 0xA0, 0x00, 0x28, 0x02, 0xD0, 0x02, 0xD0,
	0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x04, 0x6C,
	0x00, 0x0A, 0x00, 0x0C, 0x02, 0x77, 0x01, 0xE9,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static int s6e3ha9_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3ha9_displayon(struct exynos_panel_device *panel)
{
	struct exynos_panel_info *lcd = &panel->lcd_info;
	struct dsim_device *dsim = get_dsim_drvdata(0);

	DPU_INFO_PANEL("%s +\n", __func__);

	mutex_lock(&panel->ops_lock);

	dsim_write_data_seq(dsim, false, 0xf0, 0x5a, 0x5a);
	dsim_write_data_seq(dsim, false, 0xfc, 0x5a, 0x5a);

	/* DSC related configuration */
	dsim_write_data_type_seq(dsim, MIPI_DSI_DSC_PRA, 0x1);
	if (lcd->dsc.slice_num == 2)
		dsim_write_data_type_table(dsim, MIPI_DSI_DSC_PPS, SEQ_PPS_SLICE2);
	else
		DPU_ERR_PANEL("fail to set MIPI_DSI_DSC_PPS command\n");

	dsim_write_data_seq_delay(dsim, 120, 0x11); /* sleep out: 120ms delay */
	dsim_write_data_seq(dsim, false, 0xB9, 0x00, 0xB0, 0xE5, 0x09, 0x00, 0x00,
			0x00, 0x11, 0x03);
	dsim_write_data_seq(dsim, false, 0x35); /* TE on */
	dsim_write_data_seq(dsim, false, 0xED, 0x04, 0x44);

#if !defined(CONFIG_EXYNOS_EWR)
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
	/* TE start timing is advanced due to latency for the PLL_SLEEP
	 *      default value : 3040(active line) + 7(vbp) - 2 = 0xBE5
	 *      modified value : default value - 11(modifying line) = 0xBDA
	 */

	dsim_write_data_seq(dsim, false, 0xB9, 0x01, 0xB0, 0xE1, 0x09);
#else
	dsim_write_data_seq(dsim, false, 0xB9, 0x00, 0xB0, 0xEC, 0x09);
#endif
#endif
	dsim_write_data_seq(dsim, false, 0xC5, 0x0D, 0x10, 0xB4, 0x3E, 0x01);
	dsim_write_data_seq(dsim, false, 0x29); /* display on */

	mutex_unlock(&panel->ops_lock);

	DPU_INFO_PANEL("%s -\n", __func__);
	return 0;
}

static int s6e3ha9_mres(struct exynos_panel_device *panel, int mres_idx)
{
	return 0;
}

static int s6e3ha9_doze(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3ha9_doze_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3ha9_dump(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3ha9_read_state(struct exynos_panel_device *panel)
{
	return 0;
}

struct exynos_panel_ops panel_s6e3ha9_ops = {
	.id		= {0x001090, 0x1310a1, 0x1333a1},
	.suspend	= s6e3ha9_suspend,
	.displayon	= s6e3ha9_displayon,
	.mres		= s6e3ha9_mres,
	.doze		= s6e3ha9_doze,
	.doze_suspend	= s6e3ha9_doze_suspend,
	.dump		= s6e3ha9_dump,
	.read_state	= s6e3ha9_read_state,
};
