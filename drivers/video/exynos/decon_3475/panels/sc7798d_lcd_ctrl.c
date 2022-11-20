/*
 * drivers/video/decon_3475/panels/sc7798d_lcd_ctrl.c
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

#include "panel_info.h"
#include "dsim_backlight.h"

static int sc7798d_read_init_info(struct dsim_device *dsim)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	// id
	ret = dsim_read_hl_data(dsim, SC7798D_ID_REG, SC7798D_ID_LEN, dsim->priv.id);

	if (ret != SC7798D_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for(i = 0; i < SC7798D_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

read_exit:
	return 0;
}

static int sc7798d_displayon(struct dsim_device *dsim)
{
	return 0;
}

static int sc7798d_displayon_late(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	msleep(12);

	ret = dsim_write_hl_data(dsim, SEQ_INIT4_4, ARRAY_SIZE(SEQ_INIT4_3));

	dsim_panel_set_brightness(dsim, 1);

displayon_err:
	return ret;

}

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct dsim_device *dsim = NULL;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	dsim = container_of(self, struct dsim_device, fb_notif_panel);

	fb_blank = *(int *)evdata->data;

	dsim_info("%s: %d\n", __func__, fb_blank);

	if (evdata->info->node != 0)
		return 0;

	if (fb_blank == FB_BLANK_UNBLANK)
		sc7798d_displayon_late(dsim);

	return 0;
}


static int sc7798d_exit(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = NULL;
	panel = &dsim->priv;

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


static int sc7798d_init(struct dsim_device *dsim)
{
	int ret;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_INIT1_1, ARRAY_SIZE(SEQ_INIT1_1));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_2, ARRAY_SIZE(SEQ_INIT1_2));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_3, ARRAY_SIZE(SEQ_INIT1_3));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_4, ARRAY_SIZE(SEQ_INIT1_4));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_5, ARRAY_SIZE(SEQ_INIT1_5));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_6, ARRAY_SIZE(SEQ_INIT1_6));

	msleep(12);

	ret = dsim_write_hl_data(dsim, SEQ_INIT2_1, ARRAY_SIZE(SEQ_INIT2_1));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_2, ARRAY_SIZE(SEQ_INIT2_2));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_3, ARRAY_SIZE(SEQ_INIT2_3));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_4, ARRAY_SIZE(SEQ_INIT2_4));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_5, ARRAY_SIZE(SEQ_INIT2_5));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_6, ARRAY_SIZE(SEQ_INIT2_6));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_7, ARRAY_SIZE(SEQ_INIT2_7));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_8, ARRAY_SIZE(SEQ_INIT2_8));

	ret = dsim_write_hl_data(dsim, SEQ_INIT3_1, ARRAY_SIZE(SEQ_INIT3_1));
	ret = dsim_write_hl_data(dsim, SEQ_INIT3_2, ARRAY_SIZE(SEQ_INIT3_2));

	ret = dsim_write_hl_data(dsim, SEQ_INIT4_1, ARRAY_SIZE(SEQ_INIT4_1));
	ret = dsim_write_hl_data(dsim, SEQ_INIT4_2, ARRAY_SIZE(SEQ_INIT4_2));
	ret = dsim_write_hl_data(dsim, SEQ_INIT4_3, ARRAY_SIZE(SEQ_INIT4_3));

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(150);

	return ret;
}

static int sc7798d_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;
	panel->acl_enable_force = ACL_TEST_DO_NOT_WORK;

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

	sc7798d_read_init_info(dsim);
	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}


	memset(&dsim->fb_notif_panel, 0, sizeof(dsim->fb_notif_panel));
	dsim->fb_notif_panel.notifier_call = fb_notifier_callback;
	fb_register_client(&dsim->fb_notif_panel);

probe_exit:
	return ret;
}

struct dsim_panel_ops sc7798d_panel_ops = {
	.early_probe = NULL,
	.probe		= sc7798d_probe,
	.displayon	= sc7798d_displayon,
	.exit		= sc7798d_exit,
	.init		= sc7798d_init,
};

struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	return &sc7798d_panel_ops;
}
