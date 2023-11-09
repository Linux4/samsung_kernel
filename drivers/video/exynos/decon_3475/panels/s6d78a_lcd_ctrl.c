/*
 * drivers/video/decon_3475/panels/s6d78a_lcd_ctrl.c
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

#include <video/mipi_display.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "../dsim.h"


static int s6d78a_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	msleep(12);

displayon_err:
	return ret;

}

static int s6d78a_exit(struct dsim_device *dsim)
{
	int ret = 0;
	dsim_info("MDD : %s was called\n", __func__);
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	msleep(10);

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(150);

exit_err:
	return ret;
}


static int s6d78a_init(struct dsim_device *dsim)
{
	int ret;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_INIT1_1,	ARRAY_SIZE(SEQ_INIT1_1));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_2,	ARRAY_SIZE(SEQ_INIT1_2));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_3,	ARRAY_SIZE(SEQ_INIT1_3));

		/* sleep out */
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

		/* 120ms delay */
	msleep(120);

		/* Initializing  Sequence(3) */

	ret = dsim_write_hl_data(dsim, SEQ_INIT3_PAN_PROTECT, ARRAY_SIZE(SEQ_INIT3_PAN_PROTECT));
	ret = dsim_write_hl_data(dsim, SEQ_INIT3_INT_POW, ARRAY_SIZE(SEQ_INIT3_INT_POW));
	ret = dsim_write_hl_data(dsim, SEQ_INIT3_GOA, ARRAY_SIZE(SEQ_INIT3_GOA));
	ret = dsim_write_hl_data(dsim, SEQ_INIT3_INT_PORCH, ARRAY_SIZE(SEQ_INIT3_INT_PORCH));
	ret = dsim_write_hl_data(dsim, SEQ_INIT3_SRC_CONT, ARRAY_SIZE(SEQ_INIT3_SRC_CONT));
	ret = dsim_write_hl_data(dsim, SEQ_INIT3_MIPI_ABNORMAL, ARRAY_SIZE(SEQ_INIT3_MIPI_ABNORMAL));
	ret = dsim_write_hl_data(dsim, SEQ_INIT3_MIPI_AUTO_RECOVERY, ARRAY_SIZE(SEQ_INIT3_MIPI_AUTO_RECOVERY));
	ret = dsim_write_hl_data(dsim, SEQ_INIT3_GOA_OUTPUT, ARRAY_SIZE(SEQ_INIT3_GOA_OUTPUT));
	ret = dsim_write_hl_data(dsim, SEQ_INIT3_MEM_ACC_CONT, ARRAY_SIZE(SEQ_INIT3_MEM_ACC_CONT));
		/* Gamma Setting Sequence(4) */
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_POSITIVE_CONT, ARRAY_SIZE(SEQ_GAMMA_POSITIVE_CONT));
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_NEGATIVE_CONT,	ARRAY_SIZE(SEQ_GAMMA_NEGATIVE_CONT));
		/* display_on */
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
		/* 20ms delay */
	msleep(20);

		/* Initializing  Sequence(2) */
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_1, ARRAY_SIZE(SEQ_INIT2_1));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_2,	ARRAY_SIZE(SEQ_INIT2_2));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_3, ARRAY_SIZE(SEQ_INIT2_3));

	return ret;
}

static int s6d78a_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;

	}

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

probe_exit:
	return ret;
}

struct dsim_panel_ops s6d78a_panel_ops = {
	.early_probe = NULL,
	.probe		= s6d78a_probe,
	.displayon	= s6d78a_displayon,
	.exit		= s6d78a_exit,
	.init		= s6d78a_init,
};

