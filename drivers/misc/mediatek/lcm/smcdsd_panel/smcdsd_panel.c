// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <video/mipi_display.h>

#include "lcm_drv.h"
#include "smcdsd_board.h"
#include "smcdsd_panel.h"
#include "panels/dd.h"

#define dbg_info(fmt, ...)		pr_info(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)
#define dbg_none(fmt, ...)		pr_debug(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)

/* -------------------------------------------------------------------------- */
/* Global Variables */
/* -------------------------------------------------------------------------- */
struct mipi_dsi_lcd_common *g_lcd_common;
unsigned char rx_offset;
unsigned char data_type;

/* -------------------------------------------------------------------------- */
/* Local Functions */
/* -------------------------------------------------------------------------- */
static struct LCM_UTIL_FUNCS lcm_util = {0};

/* -------------------------------------------------------------------------- */
/* LCD Driver Implementations */
/* -------------------------------------------------------------------------- */
static void smcdsd_panel_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void smcdsd_panel_get_params(struct LCM_PARAMS *params)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	dbg_info("%s +\n", __func__);

	smcdsd_panel_get_params_from_dt(params);

	plcd->lcm_params = params;

	dbg_info("%s -\n", __func__);
}

static void smcdsd_panel_init(void)
{
	dbg_info("%s\n", __func__);
}

/* disp_lcm_disable(vdo lcd exit) > _cmdq_stop_trigger_loop > dpmgr_path_stop >  disp_lcm_suspend(cmd lcd exit) > dpmgr_path_power_off > panel_power,reset off */
/* panel_power on > dpmgr_path_power_on > lp11 > disp_lcm_resume > panel_reset on > send lcd init > dpmgr_path_start > _cmdq_build_trigger_loop > disp_lcm_set_display_on */
static void __smcdsd_panel_reset_enable(unsigned int enable)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	dbg_info("%s: %u->%u%s\n", __func__, plcd->reset, enable, (enable == !!plcd->reset) ? " skip" : "");

	if (enable == !!plcd->reset)
		return;

	call_drv_ops(plcd, panel_reset, !!enable);

	plcd->reset = enable;
}

static void smcdsd_panel_power_enable(unsigned int enable)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	dbg_info("%s: %u->%u%s\n", __func__, plcd->power, enable, (enable == !!plcd->power) ? " skip" : "");

	if (enable == !!plcd->power)
		return;

	if (enable) {
		call_drv_ops(plcd, panel_power, 1);
	} else {
		__smcdsd_panel_reset_enable(0);
		call_drv_ops(plcd, panel_power, 0);
	}

	plcd->power = enable;
}

static void smcdsd_panel_resume(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	__smcdsd_panel_reset_enable(1);

	call_drv_ops(plcd, init);
}

static void smcdsd_panel_disable(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	if (plcd->lcm_params && plcd->lcm_params->dsi.mode != CMD_MODE)
		call_drv_ops(plcd, exit);
}

static void smcdsd_panel_suspend(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	if (plcd->lcm_params && plcd->lcm_params->dsi.mode == CMD_MODE)
		call_drv_ops(plcd, exit);
}

static void smcdsd_set_backlight(void *handle, unsigned int level)
{
	/* unused. just prevent useless message */
}

static void smcdsd_partial_update(unsigned int x, unsigned int y, unsigned int width,
			unsigned int height)
{
	/* unused. just prevent useless message */
}

struct LCM_DRIVER smcdsd_panel_drv = {
	.set_util_funcs = smcdsd_panel_set_util_funcs,
	.get_params = smcdsd_panel_get_params,
	.init = smcdsd_panel_init,
	.disable = smcdsd_panel_disable,
	.suspend = smcdsd_panel_suspend,
	.resume = smcdsd_panel_resume,
	.power_enable = smcdsd_panel_power_enable,
	.set_backlight_cmdq = smcdsd_set_backlight,
	.update = smcdsd_partial_update,
};

static unsigned char data[259];
int smcdsd_panel_dsi_command_rx(void *drvdata,
	unsigned int id, unsigned int offset, u8 cmd, unsigned int len, u8 *buf, bool need_lock)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	int ret = 0;

	if (!plcd->lcdconnected)
		return ret;

	rx_offset = offset;
	data_type = id;

	ret = lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buf, len);

	return ret;
}

int smcdsd_panel_dsi_command_tx(void *drvdata,
	unsigned int id, unsigned long data0, unsigned int data1, bool need_lock)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	int ret = 0;

	if (!plcd->lcdconnected)
		return ret;

	dsi_write_data_dump(id, data0, data1);

	if (unlikely(!lcm_util.dsi_set_cmdq_V2))
		dbg_info("%s: dsi_set_cmdq_V2 is not exist\n", __func__);

	switch (id) {
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	{
		lcm_util.dsi_set_cmdq_V2((unsigned int)data0, 0, NULL, 1);
		break;
	}
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	{
		data[0] = data1;
		lcm_util.dsi_set_cmdq_V2((unsigned int)data0, 1, &data[0], 1);
		break;
	}

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	{
		memcpy(&data[0], (unsigned char *)data0, data1);
		lcm_util.dsi_set_cmdq_V2((unsigned int)data[0], data1 - 1, &data[1], 1);
		break;
	}

	default:
		dbg_info("%s: id(%2x) is not supported\n", __func__, id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int smcdsd_probe(struct platform_device *p)
{
	struct mipi_dsi_lcd_common *plcd = NULL;

	dbg_info("%s: node(%s)\n", __func__, of_node_full_name(p->dev.of_node));

	lcd_driver_init();
	if (!mipi_lcd_driver) {
		dbg_info("%s: lcd_driver invalid\n", __func__);
		return -EINVAL;
	}

	plcd = (struct mipi_dsi_lcd_common *)of_device_get_match_data(&p->dev);

	plcd->pdev = p;
	plcd->drv = mipi_lcd_driver;
	plcd->power = plcd->reset = 1;
	plcd->tx = smcdsd_panel_dsi_command_tx;
	plcd->rx = smcdsd_panel_dsi_command_rx;

	dbg_info("%s: driver is found(%s)\n", __func__, plcd->drv->driver.name);

	platform_set_drvdata(p, plcd);

	g_lcd_common = platform_get_drvdata(p);

	plcd->drv->pdev = platform_device_register_resndata(&p->dev, "panel_drv", PLATFORM_DEVID_NONE, NULL, 0, NULL, 0);
	if (!plcd->drv->pdev) {
		dbg_info("%s: platform_device_register invalid\n", __func__);
		return -EINVAL;
	}

	smcdsd_panel_drv.name = kstrdup(plcd->drv->driver.name, GFP_KERNEL);

	return 0;
}

struct mipi_dsi_lcd_common lcd_common;

static const struct of_device_id smcdsd_match[] = {
	{ .compatible = "samsung,mtk-dsi-panel", .data = &lcd_common},
	{},
};

static struct platform_driver smcdsd_driver = {
	.driver = {
		.name = "panel",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(smcdsd_match),
		.suppress_bind_attrs = true,
	},
	.probe = smcdsd_probe,
};

builtin_platform_driver(smcdsd_driver);

static int __init smcdsd_late_initcall(void)
{
	struct platform_device *pdev;
	struct mipi_dsi_lcd_common *plcd;

	pdev = of_find_device_by_path("/panel");
	if (!pdev) {
		dbg_info("%s: of_find_device_by_path fail\n", __func__);
		return -EINVAL;
	}

	plcd = platform_get_drvdata(pdev);
	if (!plcd) {
		dbg_info("%s: platform_get_drvdata fail\n", __func__);
		return -EINVAL;
	}

	if (!plcd->probe) {
		lcdtype = lcdtype ? lcdtype : islcmconnected;
		dbg_info("%s: lcdtype(%6x) islcmconnected %d\n", __func__, lcdtype, islcmconnected);
		call_drv_ops(plcd, probe);
		plcd->probe = 1;
	}

	run_list(&plcd->pdev->dev, "panel_regulator_init");

	return 0;
}
late_initcall(smcdsd_late_initcall);

