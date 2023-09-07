/*
 * Copyright (C) 2017 Samsung Electronics Co, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/fb.h>
#include <linux/debugfs.h>
#include <video/mipi_display.h>

#include "lcm_drv.h"
#include "sec_panel.h"
#include "sec_board.h"

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util = {0};

/* --------------------------------------------------------------------------*/
/* Boot Params */
/* --------------------------------------------------------------------------*/
unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	pr_info("lcdtype: %08x\n", lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

/* --------------------------------------------------------------------------*/
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------*/
#define SEC_PANEL_DTS_NAME	"sec_panel"
static void sec_panel_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static int sec_panel_get_params_from_dt(LCM_PARAMS *params)
{
	struct device_node *np = NULL;
	int ret = 0;

	np = of_find_node_with_property(NULL, SEC_PANEL_DTS_NAME);
	if (!np) {
		pr_warn("%s property does not exist, so create dummy\n", SEC_PANEL_DTS_NAME);
		return -EINVAL;
	}

	np = of_parse_phandle(np, SEC_PANEL_DTS_NAME, 0);
	if (!np) {
		pr_warn("%s does not exist, so create dummy\n", SEC_PANEL_DTS_NAME);
		return -EINVAL;
	}

	ret += of_property_read_u32(np, "panel-type", &params->type);
	ret += of_property_read_u32(np, "panel-lcm_if", &params->lcm_if);
	ret += of_property_read_u32(np, "panel-lcm_cmd_if", &params->lcm_cmd_if);
	ret += of_property_read_u32(np, "panel-width", &params->width);
	ret += of_property_read_u32(np, "panel-height", &params->height);
	ret += of_property_read_u32(np, "panel-physical_width", &params->physical_width);
	ret += of_property_read_u32(np, "panel-physical_height", &params->physical_height);
	ret += of_property_read_u32(np, "panel-dbi-te_mode", &params->dbi.te_mode);
	ret += of_property_read_u32(np, "panel-dbi-te_edge_polarity", &params->dbi.te_edge_polarity);
	ret += of_property_read_u32(np, "panel-dsi-mode", &params->dsi.mode);
	ret += of_property_read_u32(np, "panel-dsi-switch_mode_enable", &params->dsi.switch_mode_enable);
	ret += of_property_read_u32(np, "panel-dsi-lane_num", &params->dsi.LANE_NUM);
	ret += of_property_read_u32(np, "panel-dsi-data_format-color_order", &params->dsi.data_format.color_order);
	ret += of_property_read_u32(np, "panel-dsi-data_format-trans_seq", &params->dsi.data_format.trans_seq);
	ret += of_property_read_u32(np, "panel-dsi-data_format-padding", &params->dsi.data_format.padding);
	ret += of_property_read_u32(np, "panel-dsi-data_format-format", &params->dsi.data_format.format);
	ret += of_property_read_u32(np, "panel-dsi-ps", &params->dsi.PS);
	ret += of_property_read_u32(np, "panel-dsi-packet_size", &params->dsi.packet_size);
	ret += of_property_read_u32(np, "panel-dsi-pll_clock", &params->dsi.PLL_CLOCK);
	ret += of_property_read_u32(np, "panel-dsi-ssc_disable", &params->dsi.ssc_disable);
	ret += of_property_read_u32(np, "panel-dsi-cont_clock", &params->dsi.cont_clock);
	ret += of_property_read_u32(np, "panel-dsi-clk_lp_per_line_enable", &params->dsi.clk_lp_per_line_enable);
	ret += of_property_read_u32(np, "panel-dsi-customization_esd_check_enable", &params->dsi.customization_esd_check_enable);
	ret += of_property_read_u32(np, "panel-dsi-esd_check_enable", &params->dsi.esd_check_enable);

	return ret;
}

static void sec_panel_get_params(LCM_PARAMS *params)
{
	pr_info("%s +\n", __func__);

	memset(params, 0, sizeof(LCM_PARAMS));

	sec_panel_get_params_from_dt(params);
}

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
static LIST_HEAD(lcd_list);

struct mipi_dsi_lcd *g_panel;

static void sec_panel_init(void)
{
	pr_info("%s\n", __func__);
}

static void sec_panel_init_power(void)
{
	pr_info("%s\n", __func__);
	/* Add regulator refer count */
	run_list(&g_panel->pdev->dev, "panel_power_enable");
}

static void sec_panel_resume_power(void)
{
	struct mipi_dsi_lcd *panel = g_panel;

	pr_info("%s: %d\n", __func__, panel->power);

	if (panel->power)
		return;

	panel->power = 1;
	run_list(&g_panel->pdev->dev, "panel_power_enable");
	run_list(&g_panel->pdev->dev, "panel_reset");
}

static void sec_panel_resume(void)
{
	struct mipi_dsi_lcd *panel = g_panel;

	mutex_lock(&panel->lock);
	call_drv_ops(init);
	mutex_unlock(&panel->lock);
}

static int sec_panel_display_on(void)
{
	struct mipi_dsi_lcd *panel = g_panel;

	mutex_lock(&panel->lock);
	call_drv_ops(displayon_late);
	mutex_unlock(&panel->lock);

	return 0;
}

static void sec_panel_suspend_power(void)
{
	struct mipi_dsi_lcd *panel = g_panel;

	pr_info("%s: %d\n", __func__, panel->power);

	if (!panel->power)
		return;

	panel->power = 0;

	run_list(&g_panel->pdev->dev, "panel_power_disable");
}

static void sec_panel_suspend(void)
{
	struct mipi_dsi_lcd *panel = g_panel;

	mutex_lock(&panel->lock);
	call_drv_ops(exit);
	mutex_unlock(&panel->lock);
}

#ifdef CONFIG_SEC_PANEL_DOZE_MODE
static void sec_panel_aod(int enter)
{
	struct mipi_dsi_lcd *panel = g_panel;

	mutex_lock(&panel->lock);
	if (enter)
		call_drv_ops(doze_init);
	else
		call_drv_ops(doze_exit);
	mutex_unlock(&panel->lock);
}
#endif

static void sec_panel_cmd_q(int enable)
{
	struct mipi_dsi_lcd *panel = g_panel;

	pr_info("%s : %d\n", __func__, enable);

	mutex_lock(&panel->lock);

	if (enable)
		panel->cmd_q = true;
	else
		panel->cmd_q = false;

	mutex_unlock(&panel->lock);
}

static void mtk_dsi_set_withrawcmdq(void *cmdq, unsigned int *pdata,
			unsigned int queue_size, unsigned char force_update)
{
	lcm_util.dsi_set_cmdq_V11(cmdq, pdata, queue_size, force_update);
}

static int lcm_read_by_cmdq(void *handle, unsigned data_id, unsigned offset, unsigned cmd,
					unsigned char *buffer, unsigned char size)
{
	return lcm_util.dsi_dcs_read_cmdq_lcm_reg_v2_1(handle, data_id, offset, cmd, buffer, size);
}

LCM_DRIVER sec_panel_drv = {
	.set_util_funcs = sec_panel_set_util_funcs,
	.get_params = sec_panel_get_params,
	.read_by_cmdq = lcm_read_by_cmdq,
	.dsi_set_withrawcmdq = mtk_dsi_set_withrawcmdq,
	.init = sec_panel_init,
	.suspend = sec_panel_suspend,
	.init_power = sec_panel_init_power,
	.resume = sec_panel_resume,
	.set_display_on = sec_panel_display_on,
	.suspend_power = sec_panel_suspend_power,
	.resume_power = sec_panel_resume_power,
#ifdef CONFIG_SEC_PANEL_DOZE_MODE
	.aod = sec_panel_aod,
#endif
	.cmd_q = sec_panel_cmd_q,
};

static unsigned char data[259];
int sec_panel_dsi_command_rx(unsigned int id, u8 cmd, unsigned int len, u8 *buf)
{
	struct mipi_dsi_lcd *panel = g_panel;
	int ret = 0;

	if (unlikely(!lcm_util.dsi_dcs_read_cmdq_lcm_reg_v2_1))
		pr_err("%s: dsi_dcs_read_cmdq_lcm_reg_v2_1 is not exist\n", __func__);
	else if (unlikely(!lcm_util.dsi_dcs_read_lcm_reg_v2))
		pr_err("%s: dsi_dcs_read_lcm_reg_v2 is not exist\n", __func__);

	if (panel->cmd_q)
		ret = primary_display_dsi_read_cmdq_cmd(id, 0, cmd, buf, len);
	else
		ret = lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buf, len);

	return ret;
}

static int sec_panel_send_command_queue(unsigned int *pdata,
			unsigned int queue_size, unsigned char force_update)
{
	struct mipi_dsi_lcd *panel = g_panel;
	int ret = 0;

	if (unlikely(!lcm_util.dsi_set_cmdq_V11))
		pr_err("%s: dsi_set_cmdq_V11 is not exist\n", __func__);
	else if (unlikely(!lcm_util.dsi_set_cmdq))
		pr_err("%s: dsi_set_cmdq is not exist\n", __func__);

	if (panel->cmd_q)
		primary_display_dsi_set_withrawcmdq(pdata, queue_size, force_update);
	else
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update);

	return ret;
}

int sec_panel_dsi_command_tx(unsigned int id, unsigned long data0, unsigned int data1)
{
	int ret = 0;

	switch (id) {
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	{
		data[0] = 0x00;
		data[1] = id;
		data[2] = data0;
		data[3] = data1;
		ret = sec_panel_send_command_queue((unsigned int *)data, 1, 1);
		break;
	}

	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	{
		WARN(data1 > 0xff, "length(%d) is too big\n", data1);
		BUG_ON(data1 > 0xff);
		data[0] = 0x02;
		data[1] = id;
		data[2] = data1;
		data[3] = 0;

		memcpy(&data[4], (unsigned char *)data0, data1);

		/* queue_size = 1(config) + cmdlist by 4byte (ex: 5 byte cmdlist = 1(config) + 2 = 3 */
		ret = sec_panel_send_command_queue((unsigned int *)data, (data1 + 7) >> 2, 1);
		break;
	}

	default:
		pr_info("%s: id(%2x) is not supported\n", __func__, id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct mipi_dsi_lcd *panel;

	switch (event) {
	case FB_EVENT_FB_REGISTERED:
		break;
	default:
		return NOTIFY_DONE;
	}

	panel = container_of(self, struct mipi_dsi_lcd, fb_notifier);

	dev_info(&panel->pdev->dev, "%s: %02lx\n", __func__, event);

	if (evdata->info->node != 0)
		return NOTIFY_DONE;

	if (!panel->probe) {
		call_drv_ops(probe);
		panel->probe = 1;

		fb_unregister_client(&panel->fb_notifier);
	}

	return NOTIFY_DONE;
}

static int sec_panel_probe(struct platform_device *pdev)
{
	struct mipi_dsi_lcd *panel = NULL;
	struct mipi_dsi_lcd_driver *lcd_drv = NULL;
	int ret = 0;

	dev_info(&pdev->dev, "%s: %s\n", __func__, of_node_full_name(pdev->dev.of_node));

	g_panel = panel = kzalloc(sizeof(struct mipi_dsi_lcd), GFP_KERNEL);

	panel->pdev = pdev;
	panel->cmd_q = true;
	panel->power = 1;
	panel->tx = sec_panel_dsi_command_tx;
	panel->rx = sec_panel_dsi_command_rx;
	mutex_init(&panel->lock);

	/* should be determined which drv is real hyosun43.lee */
	list_for_each_entry(lcd_drv, &lcd_list, node) {
		if (lcd_drv && lcd_drv->driver.name) {
			dev_info(&pdev->dev, "%s: %s\n", __func__, lcd_drv->driver.name);
			panel->drv = lcd_drv;
			sec_panel_drv.name = kstrdup(lcd_drv->driver.name, GFP_KERNEL);
		}
	};

	if (panel->drv)
		dev_info(&pdev->dev, "%s: drv is found\n", __func__);

	platform_device_add_data(pdev, panel, sizeof(struct mipi_dsi_lcd));

	panel->fb_notifier.notifier_call = fb_notifier_callback;
	fb_register_client(&panel->fb_notifier);

	return ret;
}

static const struct of_device_id sec_panel_device_table[] = {
	{ .compatible = "samsung,mtk-dsi-panel", },
	{},
};

static struct platform_driver sec_panel_driver = {
	.driver = {
		.name = "panel",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_panel_device_table),
		.suppress_bind_attrs = true,
	},
	.probe = sec_panel_probe,
};

static int sec_panel_module_init(void)
{
	return platform_driver_register(&sec_panel_driver);
}

int sec_panel_register_driver(struct mipi_dsi_lcd_driver *drv)
{
	if (drv && drv->driver.name) {
		list_add_tail(&drv->node, &lcd_list);
		pr_info("%s: %s\n", __func__, drv->driver.name);
	}

	return 0;
}

device_initcall_sync(sec_panel_module_init);
MODULE_DESCRIPTION("Samsung Panel Driver");
MODULE_LICENSE("GPL");

