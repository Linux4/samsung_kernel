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
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <video/mipi_display.h>

#include "lcm_drv.h"
#include "smcdsd_board.h"
#include "smcdsd_panel.h"
#include "panels/dd.h"
#include "../../video/mt6768/videox/disp_cust.h"

#define dbg_info(fmt, ...)		pr_info(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)
#define dbg_none(fmt, ...)		pr_debug(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)


#define REGFLAG_DELAY		0xFE
#define REGFLAG_END_OF_TABLE	0xFF /* END OF REGISTERS MARKER */

struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};

/* -------------------------------------------------------------------------- */
/* Global Variables */
/* -------------------------------------------------------------------------- */
struct mipi_dsi_lcd_common *g_lcd_common;
unsigned char rx_offset;
unsigned char data_type;
extern struct mipi_dsi_lcd_driver *mipi_lcd_driver;

/* -------------------------------------------------------------------------- */
/* Local Functions */
/* -------------------------------------------------------------------------- */
static struct LCM_UTIL_FUNCS lcm_util = {0};
#if 0
/*DynFPS*/
#define dfps_dsi_send_cmd( \
			cmdq, cmd, count, para_list, force_update, sendmode) \
			lcm_util.dsi_dynfps_send_cmd( \
			cmdq, cmd, count, para_list, force_update, sendmode)
#endif
/*******************Dynfps start*************************/
#ifdef CONFIG_MTK_HIGH_FRAME_RATE

#define DFPS_MAX_CMD_NUM 10

struct LCM_dfps_cmd_table {
	bool need_send_cmd;
	enum LCM_Send_Cmd_Mode sendmode;
	struct LCM_setting_table prev_f_cmd[DFPS_MAX_CMD_NUM];
};

static struct LCM_dfps_cmd_table
	dfps_cmd_table[DFPS_LEVELNUM][DFPS_LEVELNUM] = {

	/**********level 0 to 0,1 cmd*********************/
	[DFPS_LEVEL0][DFPS_LEVEL0] = {
		false,
		LCM_SEND_IN_CMD,
		/*prev_frame cmd*/
		{
		{REGFLAG_END_OF_TABLE, 0x00, {} },
		},
	},
	/*60->90*/
	[DFPS_LEVEL0][DFPS_LEVEL1] = {
		true,
		LCM_SEND_IN_CMD,
		/*prev_frame cmd*/
		{
			{0xF0, 2, {0x5A, 0x5A} },
			{0x60, 3, {0x08, 0x00, 0x00} },
			{0xF7, 1, {0x0F} },
			{0xF0, 2, {0x5A, 0x5A} },
			{REGFLAG_END_OF_TABLE, 0x00, {} },
		},
	},

	/**********level 1 to 0,1 cmd*********************/
	/*90->60*/
	[DFPS_LEVEL1][DFPS_LEVEL0] = {
		true,
		LCM_SEND_IN_CMD,
		/*prev_frame cmd*/
		{
			{0xF0, 2, {0x5A, 0x5A} },
			{0x60, 3, {0x00, 0x00, 0x00} },
			{0xF7, 1, {0x0F} },
			{0xF0, 2, {0x5A, 0x5A} },
			{REGFLAG_END_OF_TABLE, 0x00, {} },
		},
	},

	[DFPS_LEVEL1][DFPS_LEVEL1] = {
		false,
		LCM_SEND_IN_CMD,
		/*prev_frame cmd*/
		{
			{REGFLAG_END_OF_TABLE, 0x00, {} },
		},

	},
};
#endif
/*******************Dynfps end*************************/

/* -------------------------------------------------------------------------- */
/* LCD Driver Implementations */
/* -------------------------------------------------------------------------- */
static void smcdsd_panel_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
static void lcm_dfps_int(struct LCM_DSI_PARAMS *dsi)
{
	struct dfps_info *dfps_params = dsi->dfps_params;

	dsi->dfps_enable = 1;
	dsi->dfps_default_fps = 6000;/*real fps * 100, to support float*/
	dsi->dfps_def_vact_tim_fps = 9000;/*real vact timing fps * 100*/

	/* traversing array must less than DFPS_LEVELS */
	/* DPFS_LEVEL0 */
	dfps_params[0].level = DFPS_LEVEL0;
	dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
	dfps_params[0].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	dfps_params[0].PLL_CLOCK = dsi->PLL_CLOCK;
	/* dfps_params[0].data_rate = xx; */

	/* DPFS_LEVEL1 */
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	dfps_params[1].PLL_CLOCK = dsi->PLL_CLOCK;
	/* dfps_params[1].data_rate = xx; */

	dsi->dfps_num = 2;
	dbg_info("%s %d %d\n", __func__, dfps_params[0].PLL_CLOCK, dfps_params[1].PLL_CLOCK);
}
#endif


static void smcdsd_panel_get_params(struct LCM_PARAMS *params)
{
	dbg_info("%s +\n", __func__);

	smcdsd_panel_get_params_from_dt(params);
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/****DynFPS start****/
	lcm_dfps_int(&(params->dsi));
	/****DynFPS end****/
#endif

	params->hbm_enable_wait_frame = 2;
	params->hbm_disable_wait_frame = 0;

	dbg_info("%s -\n", __func__);
}


static void smcdsd_panel_init(void)
{
	dbg_info("%s\n", __func__);
}

static void smcdsd_panel_resume(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	call_drv_ops(plcd, init);
}

static void smcdsd_panel_reset_enable(unsigned int enable)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	dbg_info("%s: %u\n", __func__, enable);

	if (enable)
		call_drv_ops(plcd, panel_reset, 1);
	else
		call_drv_ops(plcd, panel_reset, 0);
}

static void smcdsd_panel_resume_power(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	dbg_info("%s: %u\n", __func__, plcd->power);

	if (plcd->power)
		return;

	plcd->power = 1;

	call_drv_ops(plcd, panel_power, 1);
}

static void smcdsd_panel_suspend_power(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	dbg_info("%s: %u\n", __func__, plcd->power);

	if (!plcd->power)
		return;

	plcd->power = 0;

	call_drv_ops(plcd, panel_power, 0);
}

static void smcdsd_panel_suspend(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

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

static bool smcdsd_set_mask_wait(bool wait)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	plcd->mask_wait = wait;

	return 0;
}

static bool smcdsd_get_mask_wait(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	return plcd->mask_wait;
}

static bool smcdsd_framedone_notify(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);
	pr_info("%s\n", __func__);
	call_drv_ops(plcd, framedone_notify);

	return 0;
}

static bool smcdsd_set_mask_cmdq(bool en, void *qhandle)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

//	plcd->mask_qhandle = qhandle;		// a31

	/* HBM CMDQ */
//	plcd->cmdq_index = CMDQ_MASK_BRIGHTNESS;

	call_drv_ops(plcd, set_mask, !!en);

//	plcd->cmdq_index = CMDQ_PRIMARY_DISPLAY;

	smcdsd_set_mask_wait(true);
	return 0;
}

static bool smcdsd_get_mask_state(void)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	return call_drv_ops(plcd, get_mask);
}

static bool smcdsd_lcm_path_lock(bool locking)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	plcd->lcm_path_lock = locking;
	call_drv_ops(plcd, lock);

	return 0;
}

/*******************Dynfps start*************************/
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
static bool lcm_dfps_need_inform_lcm(
	unsigned int from_level, unsigned int to_level,
	struct LCM_PARAMS *params)
{
	struct LCM_dfps_cmd_table *p_dfps_cmds = NULL;

	if (from_level == to_level) {
		pr_debug("%s,same level\n", __func__);
		return false;
	}
	p_dfps_cmds =
		&(dfps_cmd_table[from_level][to_level]);
	params->sendmode = p_dfps_cmds->sendmode;

	return p_dfps_cmds->need_send_cmd;
}

static void lcm_dfps_inform_lcm(void *cmdq_handle,
unsigned int from_level, unsigned int to_level,
struct LCM_PARAMS *params)
{
//	struct LCM_dfps_cmd_table *p_dfps_cmds = NULL;
	enum LCM_Send_Cmd_Mode sendmode = LCM_SEND_IN_CMD;
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	if (from_level == to_level)
		goto done;
#if 0
	p_dfps_cmds =
		&(dfps_cmd_table[from_level][to_level]);

	if (p_dfps_cmds &&
		!(p_dfps_cmds->need_send_cmd))
		goto done;

	sendmode = params->sendmode;

	dfps_dsi_push_table(
		cmdq_handle, p_dfps_cmds->prev_f_cmd,
		ARRAY_SIZE(p_dfps_cmds->prev_f_cmd), 1, sendmode);
#endif
	call_drv_ops(plcd, set_fps, to_level);


done:
	pr_info("%s,done %d->%d\n",
		__func__, from_level, to_level, sendmode);

}
#endif
/*******************Dynfps end*************************/

#if defined(CONFIG_SMCDSD_DOZE)
static void smcdsd_panel_aod(int enter)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);
#if defined(CONFIG_SMCDSD_DOZE_USE_NEEDLOCK)
	call_drv_ops(plcd, doze_init, enter, false);
#else
	call_drv_ops(plcd, doze_init, enter);
#endif
}

#endif

struct LCM_DRIVER smcdsd_panel_drv = {
	.set_util_funcs = smcdsd_panel_set_util_funcs,
	.get_params = smcdsd_panel_get_params,
	.init = smcdsd_panel_init,
	.suspend = smcdsd_panel_suspend,
	.resume = smcdsd_panel_resume,
	.reset_enable = smcdsd_panel_reset_enable,
	.suspend_power = smcdsd_panel_suspend_power,
	.resume_power = smcdsd_panel_resume_power,
	.set_backlight_cmdq = smcdsd_set_backlight,
	.update = smcdsd_partial_update,
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/*DynFPS*/
	.dfps_send_lcm_cmd = lcm_dfps_inform_lcm,
	.dfps_need_send_cmd = lcm_dfps_need_inform_lcm,
#endif
#if defined(CONFIG_SMCDSD_DOZE)
	.aod = smcdsd_panel_aod,
#endif
	.get_hbm_state = smcdsd_get_mask_state,
	.set_hbm_cmdq = smcdsd_set_mask_cmdq,
	.get_hbm_wait = smcdsd_get_mask_wait,
	.set_hbm_wait = smcdsd_set_mask_wait,
	.framedone_notify = smcdsd_framedone_notify,
	.lcm_path_lock = smcdsd_lcm_path_lock,
};

int smcdsd_panel_dsi_command_rx(void *drvdata,
	unsigned int id, unsigned int offset, u8 cmd, unsigned int len, u8 *buf, bool need_lock)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	int ret = 0;

	if (!plcd->lcdconnected)
		return ret;

	rx_offset = offset;
	data_type = id;

	ret = read_lcm(cmd, buf, len, 0, need_lock, offset);

	return ret;
}

int smcdsd_panel_dsi_command_tx(void *drvdata,
	unsigned int id, unsigned long data0, unsigned int data1, bool need_lock)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	struct LCM_setting_table_V3 tx_table = {0, };
	int ret = 0;

	if (!plcd->lcdconnected)
		return ret;

	dsi_write_data_dump(id, data0, data1);

	switch (id) {
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	{
		tx_table.id = id;
		tx_table.cmd = data0;
		tx_table.count = 0;
		set_lcm(&tx_table, 1, 0, need_lock);
		break;
	}
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	{
		tx_table.id = id;
		tx_table.cmd = data0;
		tx_table.count = 1;
		tx_table.para_list[0] = data1;
		set_lcm(&tx_table, 1, 0, need_lock);
		break;
	}

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	{
		tx_table.id = id;
		tx_table.count = data1 - 1;
		memcpy(&tx_table.cmd, (unsigned char *)data0, 1);
		tx_table.para_list[0] = data1;
		memcpy(&tx_table.para_list, (unsigned char *)data0 + 1, data1 - 1);
		set_lcm(&tx_table, 1, 0, need_lock);
		break;
	}

	default:
		dbg_info("%s: id(%2x) is not supported\n", __func__, id);
		ret = -EINVAL;
		break;
	}

	return ret;
}


int smcdsd_panel_dsi_command_rx_with_framedone(void *drvdata,
	unsigned int id, unsigned int offset, u8 cmd, unsigned int len, u8 *buf,
	bool need_lock, bool wait_framedone)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	int ret = 0;

	if (!plcd->lcdconnected)
		return ret;

	rx_offset = offset;
	data_type = id;

	ret = read_lcm_with_check_framedone(cmd, buf, len, 0, need_lock, offset, wait_framedone);

	return ret;
}

int smcdsd_panel_dsi_command_tx_with_framedone(void *drvdata,
	unsigned int id, unsigned long data0, unsigned int data1,
	bool need_lock, bool wait_framedone)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	struct LCM_setting_table_V3 tx_table = {0, };
	int ret = 0;

	if (!plcd->lcdconnected)
		return ret;

	dsi_write_data_dump(id, data0, data1);

	switch (id) {
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	{
		tx_table.id = id;
		tx_table.cmd = data0;
		tx_table.count = 0;
		set_lcm_with_check_framedone(&tx_table, 1, 0, need_lock, wait_framedone);
		break;
	}
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	{
		tx_table.id = id;
		tx_table.cmd = data0;
		tx_table.count = 1;
		tx_table.para_list[0] = data1;
		set_lcm_with_check_framedone(&tx_table, 1, 0, need_lock, wait_framedone);
		break;
	}

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	{
		tx_table.id = id;
		tx_table.count = data1 - 1;
		memcpy(&tx_table.cmd, (unsigned char *)data0, 1);
		tx_table.para_list[0] = data1;
		memcpy(&tx_table.para_list, (unsigned char *)data0 + 1, data1 - 1);
		set_lcm_with_check_framedone(&tx_table, 1, 0, need_lock, wait_framedone);
		break;
	}

	default:
		dbg_info("%s: id(%2x) is not supported\n", __func__, id);
		ret = -EINVAL;
		break;
	}

	return ret;
}


static struct of_device_id smcdsd_match[] = {
	{ .compatible = "samsung,mtk-dsi-panel", },
	{},
};

static int smcdsd_probe(struct platform_device *p)
{
	struct mipi_dsi_lcd_common *plcd = NULL;

	dbg_info("%s: node(%s)\n", __func__, of_node_full_name(p->dev.of_node));

	lcd_driver_init();

	plcd = kzalloc(sizeof(struct mipi_dsi_lcd_common), GFP_KERNEL);

	plcd->pdev = p;
	plcd->drv = mipi_lcd_driver;
	plcd->tx = smcdsd_panel_dsi_command_tx;
	plcd->rx = smcdsd_panel_dsi_command_rx;

	// add from jihoonn.kim in samsung because ALPS05482657
	plcd->tx_with_framedone = smcdsd_panel_dsi_command_tx_with_framedone;
	plcd->rx_with_framedone = smcdsd_panel_dsi_command_rx_with_framedone;
	plcd->power = 1;

	if (!plcd->drv) {
		dbg_info("%s: lcd_driver invalid\n", __func__);
		kfree(plcd);
		return -EINVAL;
	}

	dbg_info("%s: driver is found(%s)\n", __func__, plcd->drv->driver.name);

	smcdsd_panel_drv.name = kstrdup(plcd->drv->driver.name, GFP_KERNEL);

	platform_set_drvdata(p, plcd);

	g_lcd_common = platform_get_drvdata(p);

	plcd->drv->pdev = platform_device_register_resndata(&p->dev, "panel_drv", PLATFORM_DEVID_NONE, NULL, 0, NULL, 0);
	if (!plcd->drv->pdev) {
		dbg_info("%s: platform_device_register invalid\n", __func__);
		return -EINVAL;
	}

	return 0;
}

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
		dbg_info("%s: islcmconnected %d\n", __func__, islcmconnected);
		call_drv_ops(plcd, probe);
		plcd->probe = 1;
	}

	run_list(&plcd->pdev->dev, "panel_regulator_init");

	return 0;
}
late_initcall(smcdsd_late_initcall);

