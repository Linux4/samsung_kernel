/*
 * drivers/video/decon_7580/panels/lago_dummy_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <video/mipi_display.h>
#include "../dsim.h"
#include "dsim_panel.h"

#include "lago_dummy_param.h"


static int dsim_panel_early_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	panel->ops = dsim_panel_get_priv_ops(dsim);

	if (panel->ops->early_probe)
		ret = panel->ops->early_probe(dsim);
	return ret;
}

static int dsim_panel_probe(struct dsim_device *dsim)
{
	return 0;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	return 0;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	return 0;
}

static int dsim_panel_resume(struct dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver lago_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
};


static int lago_dummy_exit(struct dsim_device *dsim)
{
	int ret = 0;

	return ret;
}

static int lago_dummy_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	return ret;
}

static int lago_dummy_probe(struct dsim_device *dsim)
{
	int ret = 0;

	return ret;
}

static int lago_dummy_early_probe(struct dsim_device *dsim)
{
	int ret = 0;

	return ret;
}

struct dsim_panel_ops lago_panel_ops = {
	.early_probe = lago_dummy_early_probe,
	.probe		= lago_dummy_probe,
	.displayon	= lago_dummy_displayon,
	.exit		= lago_dummy_exit,
	.init		= NULL,
};

