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

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drmP.h>

#include <video/mipi_display.h>

#include "lcm_drv.h"
#include "smcdsd_board.h"
#include "smcdsd_notify.h"
#include "smcdsd_panel.h"
#include "panels/dd.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../../mediatek/mtk_panel_ext.h"
#include "../../mediatek/mtk_log.h"
#include "../../mediatek/mtk_drm_graphics_base.h"
#include "../../mediatek/mtk_cust.h"
#endif

#define dbg_info(fmt, ...)		pr_info(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)
#define dbg_none(fmt, ...)		pr_debug(pr_fmt("smcdsd: "fmt), ##__VA_ARGS__)

/* -------------------------------------------------------------------------- */
/* Global Variables */
/* -------------------------------------------------------------------------- */
struct mipi_dsi_lcd_common *g_lcd_common;
unsigned int islcmconnected = 1;

/* -------------------------------------------------------------------------- */
/* LCD Driver Implementations */
/* -------------------------------------------------------------------------- */
static inline struct mipi_dsi_lcd_common *panel_to_common_lcd(struct drm_panel *p)
{
	return container_of(p, struct mipi_dsi_lcd_common, panel);
}

static inline struct mipi_dsi_lcd_common *bridge_to_common_lcd(struct drm_bridge *b)
{
	return container_of(b, struct mipi_dsi_lcd_common, bridge);
}

/* bridge disable -> panel disable -> panel unprepare -> bridge post_disable */
static void common_lcd_bridge_disable(struct drm_bridge *bridge)
{
	struct mipi_dsi_lcd_common *plcd = bridge_to_common_lcd(bridge);

	smcdsd_fb_simple_notifier_call_chain(SMCDSD_EARLY_EVENT_BLANK, FB_BLANK_POWERDOWN);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));
}

static int common_lcd_disable(struct drm_panel *panel)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s: enabled(%d)\n", __func__, plcd->enabled);

	if (!plcd->enabled)
		return 0;

	call_drv_ops(plcd, exit);

	plcd->enabled = false;

	return 0;
}

static int common_lcd_unprepare(struct drm_panel *panel)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s: prepared(%d)\n", __func__, plcd->prepared);

	if (!plcd->prepared)
		return 0;

	plcd->error = 0;
	plcd->prepared = false;

	return 0;
}

static void common_lcd_bridge_post_disable(struct drm_bridge *bridge)
{
	struct mipi_dsi_lcd_common *plcd = bridge_to_common_lcd(bridge);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));

	call_drv_ops(plcd, panel_reset, 0);
	call_drv_ops(plcd, panel_power, 0);

	smcdsd_fb_simple_notifier_call_chain(SMCDSD_EVENT_BLANK, FB_BLANK_POWERDOWN);

	plcd->cmdq = 0;
}

/* bridge pre_enable -> panel prepare -> panel enable -> bridge enable */
static void common_lcd_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct mipi_dsi_lcd_common *plcd = bridge_to_common_lcd(bridge);

	smcdsd_fb_simple_notifier_call_chain(SMCDSD_EARLY_EVENT_BLANK, FB_BLANK_UNBLANK);

	if (plcd->enabled)
		return;

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));

	call_drv_ops(plcd, panel_power, 1);
}

static int common_lcd_prepare(struct drm_panel *panel)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);
	int ret;

	dbg_info("%s: prepared(%d)\n", __func__, plcd->prepared);

	if (plcd->prepared)
		return 0;

	call_drv_ops(plcd, panel_reset, 1);

	call_drv_ops(plcd, init);

	ret = plcd->error;
	if (ret < 0) {
		dbg_info("%s: error(%d)\n", plcd->error);
		common_lcd_unprepare(panel);
	}

	plcd->prepared = true;

	return ret;
}

static int common_lcd_enable(struct drm_panel *panel)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s: enabled(%d)\n", __func__, plcd->enabled);

	if (plcd->enabled)
		return 0;

	plcd->enabled = true;

	return 0;
}

static void common_lcd_bridge_enable(struct drm_bridge *bridge)
{
	struct mipi_dsi_lcd_common *plcd = bridge_to_common_lcd(bridge);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));

	smcdsd_fb_simple_notifier_call_chain(SMCDSD_EVENT_BLANK, FB_BLANK_UNBLANK);

	plcd->cmdq = 1;
}

static const struct drm_bridge_funcs common_lcd_bridge_funcs = {
	.pre_enable = common_lcd_bridge_pre_enable,
	.enable = common_lcd_bridge_enable,
	.disable = common_lcd_bridge_disable,
	.post_disable = common_lcd_bridge_post_disable,
};

static int common_lcd_drm_attach_bridge(struct drm_bridge *bridge,
				struct drm_encoder *encoder)
{
	int ret = 0;

	if (!bridge)
		return -ENOENT;

	encoder->bridge = bridge;
	bridge->encoder = encoder;
	ret = drm_bridge_attach(encoder, bridge, NULL);
	if (ret) {
		dbg_info("failed to attach common lcm bridge to drm\n");
		encoder->bridge = NULL;
		bridge->encoder = NULL;
	}

	dbg_info("%s: dev_name(%s) encoder_name(%s)\n", __func__, dev_name(bridge->dev->dev), encoder->name);

	return ret;
}

static int common_lcd_bridge_add(struct drm_panel *panel)
{
	struct drm_encoder *encoder = NULL;
	struct drm_connector *connector = NULL;
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);
	u32 i;

	if (plcd->bridge.of_node) {
		dbg_info("%s: %s is already registered\n", __func__, of_node_full_name(plcd->bridge.of_node));
		return 0;
	}

	connector = panel->connector;
	for (i = 0; i < DRM_CONNECTOR_MAX_ENCODER; i++) {
		if (connector->encoder_ids[i]) {
			encoder = drm_encoder_find(panel->drm, NULL, connector->encoder_ids[i]);
			if (encoder)
				break;
		}
	}

	if (!encoder) {
		dbg_info("%s: fail to find encoder\n", __func__);
		return -EINVAL;
	}

	drm_bridge_add(&plcd->bridge);
	plcd->bridge.of_node = plcd->dev->of_node;
	plcd->bridge.funcs = &common_lcd_bridge_funcs;

	common_lcd_drm_attach_bridge(&plcd->bridge, encoder);

	return 0;
}

static int common_lcd_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);
	int i, count = 0;

	common_lcd_bridge_add(panel);

	for (i = 0; i < LCD_CONFIG_MAX; i++) {
		if (!plcd->config[i].drm.vrefresh)
			continue;

		mode = drm_mode_duplicate(panel->drm, &plcd->config[i].drm);
		if (!mode) {
			dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
				plcd->config[i].drm.hdisplay, plcd->config[i].drm.vdisplay,
				plcd->config[i].drm.vrefresh);
			return -ENOMEM;
		}

		drm_mode_set_name(mode);
		mode->type = plcd->config[i].drm.type;
		drm_mode_probed_add(panel->connector, mode);
		count++;

		dbg_info("%s: %dth Modeline " DRM_MODE_FMT "\n", __func__, i, DRM_MODE_ARG(mode));
	}

	panel->connector->display_info.width_mm = plcd->config[LCD_CONFIG_DFT].lcm.physical_width;
	panel->connector->display_info.height_mm = plcd->config[LCD_CONFIG_DFT].lcm.physical_height;

	return 1;	/* this is count */
}

static const struct drm_panel_funcs common_lcd_drm_funcs = {
	.disable = common_lcd_disable,
	.unprepare = common_lcd_unprepare,
	.prepare = common_lcd_prepare,
	.enable = common_lcd_enable,
	.get_modes = common_lcd_get_modes,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 1, i = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	for (i = 0; i < LCD_CONFIG_MAX; i++) {
		if (m->vrefresh == plcd->config[i].drm.vrefresh) {
			ext->params = &plcd->config[i].ext;
			ret = 0;
		}
	}

	dbg_info("%s: mode(%u) vrefresh(%dHz) match(%d)\n", __func__, mode, m->vrefresh, !ret);

	return ret;
}

static void common_lcd_path_lock(bool need_lock)
{
	struct mipi_dsi_lcd_common *plcd = g_lcd_common;

	dbg_info("%s: enabled(%d)\n", __func__, need_lock);

	call_drv_ops(plcd, lcm_path_lock, need_lock);
}

static struct mtk_panel_funcs ext_funcs = {
	.ext_param_set = mtk_panel_ext_param_set,
	.lcm_path_lock = common_lcd_path_lock,
};
#endif

static int dsi_data_type_is_dcs(u8 type)
{
	switch (type) {
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_DCS_READ:
	case MIPI_DSI_DCS_LONG_WRITE:
		return 1;
	}

	return 0;
}

struct mtk_ddic_dsi_msg rx_cmd_msg;
unsigned char rx_buf[RT_MAX_NUM * 2];
int smcdsd_panel_dsi_command_rx(void *drvdata,
	unsigned int id, unsigned int offset, u8 cmd, unsigned int len, u8 *buf, bool need_lock)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(plcd->dev);
	unsigned int i = 0, j = 0;
	unsigned int ret_dlen = 0;
	int ret;
	struct mtk_ddic_dsi_msg *cmd_msg = &rx_cmd_msg;
	u8 tx[10] = {0};

	if (!need_lock) {
		mipi_dsi_set_maximum_return_packet_size(dsi, len);

		ret = mipi_dsi_dcs_read(dsi, cmd, buf, len);
		if (ret < 0)
			dbg_info("%s: error %d reading dcs seq:(%#x)\n", ret, cmd);

		return ret;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = id;
	tx[0] = cmd;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;

	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = rx_buf;
	memset(cmd_msg->rx_buf[0], 0, sizeof(rx_buf));
	cmd_msg->rx_len[0] = len;

	ret = read_lcm(cmd_msg);
	if (ret != 0) {
		dbg_info("%s error\n", __func__);
		goto  done;
	}

	for (i = 0; i < cmd_msg->rx_cmd_num; i++) {
		ret_dlen = cmd_msg->rx_len[i];
		dbg_info("read lcm addr:0x%x--dlen:%d--cmd_idx:%d\n",
			*(char *)(cmd_msg->tx_buf[i]), ret_dlen, i);
		for (j = 0; j < ret_dlen; j++) {
			dbg_info("read lcm addr:0x%x--byte:%d,val:0x%x\n",
				*(char *)(cmd_msg->tx_buf[i]), j,
				*(char *)(cmd_msg->rx_buf[i] + j));

				buf[j] = *(char *)(cmd_msg->rx_buf[i] + j);
		}
	}

	ret = ret_dlen;
done:
	return ret;
}

int smcdsd_panel_dsi_command_tx(void *drvdata,
	unsigned int id, unsigned long data0, unsigned int data1, bool need_lock)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(plcd->dev);
	ssize_t ret = 0;
	struct mtk_ddic_dsi_msg dsi_msg;

	if (!plcd->lcdconnected)
		return ret;

	dsi_write_data_dump(id, data0, data1);

	if (plcd->error < 0)
		return ret;

	memset(&dsi_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	switch (data1) {
	case 1:
		dsi_msg.type[0] = MIPI_DSI_DCS_SHORT_WRITE;
		break;

	case 2:
		dsi_msg.type[0] = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		break;

	default:
		dsi_msg.type[0] = MIPI_DSI_DCS_LONG_WRITE;
		break;
	}

	dsi_msg.tx_len[0] = data1;
	dsi_msg.tx_buf[0] = data0;
	dsi_msg.tx_cmd_num = 1;

	/* CMDQ */
	ret = set_lcm(&dsi_msg);

	if (ret < 0) {
		plcd->error = ret;
		return -EINVAL;
	}

	return 0;
}

struct mipi_dsi_lcd_common lcd_common;

static const struct of_device_id smcdsd_match[] = {
	{ .compatible = "samsung,mtk-dsi-panel", .data = &lcd_common},
	{},
};

static int smcdsd_probe(struct platform_device *p)
{
	struct mipi_dsi_lcd_common *plcd = NULL;

	dbg_info("%s: node(%s)\n", __func__, of_node_full_name(p->dev.of_node));

	plcd = (struct mipi_dsi_lcd_common *)of_device_get_match_data(&p->dev);

	plcd->pdev = p;
	plcd->drv = mipi_lcd_driver;
	plcd->tx = smcdsd_panel_dsi_command_tx;
	plcd->rx = smcdsd_panel_dsi_command_rx;
	plcd->power = 1;
	plcd->cmdq = 1;

	if (!plcd->drv) {
		dbg_info("%s: lcd_driver invalid\n", __func__);
		kfree(plcd);
		return -EINVAL;
	}

	dbg_info("%s: driver is found(%s)\n", __func__, plcd->drv->driver.name);

	platform_set_drvdata(p, plcd);

	g_lcd_common = platform_get_drvdata(p);

	plcd->drv->pdev = platform_device_register_resndata(&p->dev, "panel_drv", PLATFORM_DEVID_NONE, NULL, 0, NULL, 0);
	if (!plcd->drv->pdev) {
		dbg_info("%s: platform_device_register invalid\n", __func__);
		return -EINVAL;
	}

	if (!plcd->probe) {
		//lcdtype = lcdtype ? lcdtype : islcmconnected;
		dbg_info("%s: lcdtype(%6x) islcmconnected %d\n", __func__, lcdtype, islcmconnected);
		call_drv_ops(plcd, probe);
		plcd->probe = 1;
	}

	run_list(&plcd->pdev->dev, "panel_regulator_init");

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

static int common_lcd_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct mipi_dsi_lcd_common *plcd = &lcd_common;
	int ret;

	dbg_info("%s: node(%s)\n", __func__, of_node_full_name(dev->of_node));

	mipi_dsi_set_drvdata(dsi, plcd);

	plcd->dev = dev;

	lcd_driver_init();

	platform_driver_register(&smcdsd_driver);

	ret = smcdsd_panel_get_config(plcd);
	if (ret <= 1)
		ext_funcs.ext_param_set = NULL;

	plcd->prepared = true;
	plcd->enabled = true;

	drm_panel_init(&plcd->panel);
	plcd->panel.dev = dev;
	plcd->panel.funcs = &common_lcd_drm_funcs;

	ret = drm_panel_add(&plcd->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&plcd->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &plcd->config[LCD_CONFIG_DFT].ext, &ext_funcs, &plcd->panel);
	if (ret < 0)
		return ret;
#endif

	return ret;
}

static int common_lcd_remove(struct mipi_dsi_device *dsi)
{
	struct mipi_dsi_lcd_common *plcd = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&plcd->panel);

	return 0;
}

static const struct of_device_id common_lcd_of_match[] = {
	{ .compatible = "smcdsd_panel", },
	{ }
};

MODULE_DEVICE_TABLE(of, common_lcd_of_match);

static struct mipi_dsi_driver common_lcd_driver = {
	.probe = common_lcd_probe,
	.remove = common_lcd_remove,
	.driver = {
		.name = "smcdsd_panel",
		.owner = THIS_MODULE,
		.of_match_table = common_lcd_of_match,
	},
};

module_mipi_dsi_driver(common_lcd_driver);

