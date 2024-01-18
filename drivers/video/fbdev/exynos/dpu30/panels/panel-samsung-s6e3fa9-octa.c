/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung S6E3HA8 Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <video/mipi_display.h>
#include "exynos_panel_drv.h"
#include "../dsim.h"

static unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x20
};

static unsigned char SEQ_BRIGHTNESS[] = {
	0x51,
	0x01, 0xBD
};

static unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static unsigned char SEQ_PAGE_ADDR_SET_2A[] = {
	0x2A,
	0x00, 0x00, 0x04, 0x37
};

static unsigned char SEQ_PAGE_ADDR_SET_2B[] = {
	0x2B,
	0x00, 0x00, 0x09, 0x5F
};

static unsigned char SEQ_TSP_VSYNC_ON[] = {
	0xB9,
	0x00, 0x00, 0x14, 0x18, 0x00, 0x00, 0x00, 0x10
};

static unsigned char SEQ_ELVSS_SET[] = {
	0xB5,
	0x19, 0x8D, 0x16		/* ELVSS Return */
};

static int s6e3fa9octa_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa9octa_displayon(struct exynos_panel_device *panel)
{
	int ret = 0;
	u8 buf[4] = {0x0, };
	struct dsim_device *dsim = get_dsim_drvdata(0);

	DPU_INFO_PANEL("%s +\n", __func__);

	mutex_lock(&panel->ops_lock);

	/* 6. Sleep Out(11h) */
	/* 7. Wait 10ms */
	dsim_write_data_seq_delay(dsim, 10, 0x11); /* sleep out: 10ms delay */

	/* ID READ */
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,
			MIPI_DCS_GET_DISPLAY_ID, DSIM_DDI_ID_LEN, buf);
	if (ret < 0)
		dsim_err("failed to read panel id(%d)\n", ret);
	else
		dsim_info("suceeded to read panel id : 0x%06x\n", *(u32 *)buf);

	/* 8. Common Setting */
	/* Testkey Enable */
	dsim_write_data_seq(dsim, false, 0xf0, 0x5a, 0x5a);
	dsim_write_data_seq(dsim, false, 0xfc, 0x5a, 0x5a);
	dsim_write_data_table(dsim, SEQ_PAGE_ADDR_SET_2A);
	dsim_write_data_table(dsim, SEQ_PAGE_ADDR_SET_2B);
	dsim_write_data_table(dsim, SEQ_TSP_VSYNC_ON);

	/* 10. Brightness Setting */
	dsim_write_data_table(dsim, SEQ_HBM_OFF);
	dsim_write_data_table(dsim, SEQ_ELVSS_SET);
	dsim_write_data_table(dsim, SEQ_BRIGHTNESS);
	dsim_write_data_table(dsim, SEQ_ACL_OFF);

	/* Testkey Disable */
	dsim_write_data_seq(dsim, false, 0xf0, 0xa5, 0xa5);
	dsim_write_data_seq(dsim, false, 0xfc, 0xa5, 0xa5);

	/* 8.1.1 TE(Vsync) ON/OFF */
	dsim_write_data_seq(dsim, false, 0xf0, 0x5a, 0x5a);
	dsim_write_data_seq(dsim, false, 0x35, 0x00, 0x00);
	dsim_write_data_seq(dsim, false, 0xf0, 0xa5, 0xa5);

	/* 11. Wait 110ms */
	msleep(110);

	/* 12. Display On(29h) */
	dsim_write_data_seq(dsim, false, 0x29);

	mutex_unlock(&panel->ops_lock);

	DPU_INFO_PANEL("%s -\n", __func__);
	return 0;
}

static int s6e3fa9octa_mres(struct exynos_panel_device *panel, int mres_idx)
{
	return 0;
}


static int s6e3fa9octa_doze(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa9octa_doze_suspend(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa9octa_dump(struct exynos_panel_device *panel)
{
	return 0;
}

static int s6e3fa9octa_read_state(struct exynos_panel_device *panel)
{
	return 0;
}

struct exynos_panel_ops panel_s6e3fa9octa_ops = {
	.id		= {0x420080, 0xffffff, 0xffffff},
	.suspend	= s6e3fa9octa_suspend,
	.displayon	= s6e3fa9octa_displayon,
	.mres		= s6e3fa9octa_mres,
	.doze		= s6e3fa9octa_doze,
	.doze_suspend	= s6e3fa9octa_doze_suspend,
	.dump		= s6e3fa9octa_dump,
	.read_state	= s6e3fa9octa_read_state,
};
