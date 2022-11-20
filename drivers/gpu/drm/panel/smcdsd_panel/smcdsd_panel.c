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

#include <drm/drm_atomic.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drmP.h>

#include <video/mipi_display.h>

#include "lcm_drv.h"
#include "smcdsd_board.h"
#include "smcdsd_dsi_msg.h"
#include "smcdsd_notify.h"
#include "smcdsd_panel.h"
#include "panels/dd.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../../mediatek/mtk_debug.h"
#include "../../mediatek/mtk_drm_fbdev.h"
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

static int common_lcd_set_power(struct drm_panel *panel, int power)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s: dev_name(%s) power(%d)\n", __func__, dev_name(plcd->dev), power);

	if (power) {
		call_drv_ops(plcd, panel_power, 1);
	} else {
		call_drv_ops(plcd, panel_reset, 0);
		call_drv_ops(plcd, panel_power, 0);
	}

	return 0;
}

/* bridge disable -> panel disable -> panel unprepare -> set_power -> bridge post_disable */
static void common_lcd_bridge_disable(struct drm_bridge *bridge)
{
	struct mipi_dsi_lcd_common *plcd = bridge_to_common_lcd(bridge);

	//smcdsd_fb_simple_notifier_call_chain(SMCDSD_EARLY_EVENT_BLANK, FB_BLANK_POWERDOWN);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));
}

static int common_lcd_disable(struct drm_panel *panel)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));

	call_drv_ops(plcd, exit);

	return 0;
}

static int common_lcd_unprepare(struct drm_panel *panel)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));

	return 0;
}

static void common_lcd_bridge_post_disable(struct drm_bridge *bridge)
{
	struct mipi_dsi_lcd_common *plcd = bridge_to_common_lcd(bridge);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));

	//smcdsd_fb_simple_notifier_call_chain(SMCDSD_EVENT_BLANK, FB_BLANK_POWERDOWN);
}

/* bridge pre_enable -> set_power -> panel prepare -> panel enable -> bridge enable */
static void common_lcd_bridge_pre_enable(struct drm_bridge *bridge)
{
	struct mipi_dsi_lcd_common *plcd = bridge_to_common_lcd(bridge);

	//smcdsd_fb_simple_notifier_call_chain(SMCDSD_EARLY_EVENT_BLANK, FB_BLANK_UNBLANK);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));
}

static int common_lcd_prepare(struct drm_panel *panel)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));

	call_drv_ops(plcd, panel_reset, 1);

	call_drv_ops(plcd, init);

	return 0;
}

static int common_lcd_enable(struct drm_panel *panel)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));

	call_drv_ops(plcd, enable);

	return 0;
}

static void common_lcd_bridge_enable(struct drm_bridge *bridge)
{
	struct mipi_dsi_lcd_common *plcd = bridge_to_common_lcd(bridge);

	dbg_info("%s: dev_name(%s)\n", __func__, dev_name(plcd->dev));

	//smcdsd_fb_simple_notifier_call_chain(SMCDSD_EVENT_BLANK, FB_BLANK_UNBLANK);
}

static int common_framedone_notify(struct drm_panel *panel)
{
	int ret = 0;
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);
	ret = call_drv_ops(plcd, framedone_notify);
	return ret;
}

static int common_lcd_set_dispon_cmdq(void *dsi, dcs_write_gce cb,
		void *handle)
{
	char disp_on[] = {0x29};

	dbg_info("%s\n", __func__);

//	call_drv_ops(plcd, displayon_late);
	cb(dsi, handle, disp_on, ARRAY_SIZE(disp_on));

	return 0;
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

	plcd->encoder = encoder;

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

static int common_lcd_ext_param_set(struct drm_panel *panel, unsigned int mode)
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

static int common_lcd_mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, dst_mode);
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	if (cur_mode == dst_mode) {
		dbg_info("%s: cur_mode(%u) dst_mode(%u)\n", __func__, cur_mode, dst_mode);
		return ret;
	}

	ret = call_drv_ops(plcd, set_mode, m, stage);

	return ret;
}

static int common_lcd_late_register(struct drm_panel *panel)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s\n", __func__);

	islcmconnected = mtk_drm_lcm_is_connect();
	lcdtype = lcdtype ? lcdtype : islcmconnected;
	dbg_info("%s: lcdtype(%6x) islcmconnected %d\n", __func__, lcdtype, islcmconnected);
	call_drv_ops(plcd, probe);
	plcd->probe = 1;

	common_lcd_bridge_add(panel);

	return 0;
}

#if defined(CONFIG_SMCDSD_DOZE)
static unsigned long common_lcd_doze_get_mode_flags(struct drm_panel *panel,
	int doze_en)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	unsigned long mode_flags;

	dbg_info("%s: doze_en(%d)\n", __func__, doze_en);

	mode_flags = plcd->config[LCD_CONFIG_DFT].dsi.mode_flags;

	return mode_flags;
}

#if 0
static int common_lcd_doze_enable_start(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	dbg_info("%s\n", __func__);

	return 0;
}
#endif

static int common_lcd_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s\n", __func__);

	call_drv_ops(plcd, doze, 1);

	return 0;
}

static int common_lcd_doze_post_disp_on(struct drm_panel *panel,
		void *dsi, dcs_write_gce cb, void *handle)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s\n", __func__);

	call_drv_ops(plcd, doze_post_disp_on);

	return 0;
}

static int common_lcd_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s\n", __func__);

	call_drv_ops(plcd, doze, 0);

	return 0;
}

#if 0
static int common_lcd_set_aod_light_mode(void *dsi,
	dcs_write_gce cb, void *handle, unsigned int mode)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

//todo: how dsi -> panel
//todo: change cb parameter structure

	dbg_info("%s\n", __func__);

	return 0;
}

static int common_lcd_doze_area(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mipi_dsi_lcd_common *plcd = panel_to_common_lcd(panel);

	dbg_info("%s\n", __func__);

	return call_drv_ops(plcd, doze_area);
}
#endif
#endif

static int common_lcd_crtc_state_notify(struct drm_encoder *encoder,
	int active, int prepare)
{
	struct drm_bridge *bridge;
	struct mipi_dsi_lcd_common *plcd;

	if (!encoder->bridge) {
		dbg_info("%s: bridge is null\n", __func__);
		return 0;
	}

	bridge = encoder->bridge;
	plcd = bridge_to_common_lcd(bridge);

	smcdsd_fb_simple_notifier_call_chain(prepare ? SMCDSD_EARLY_EVENT_BLANK : SMCDSD_EVENT_BLANK, active ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);

	return 0;
}

#if defined(CONFIG_SMCDSD_DYNAMIC_MIPI)
extern void mtk_disp_mipi_ccci_callback(unsigned int en, unsigned int usrdata);
int smcdsd_panel_dsi_clk_change(void *drvdata, unsigned int index)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	struct drm_panel *panel = &(plcd->panel);
	struct mtk_panel_ext *ext = find_panel_ext(panel);

	ext->params->dyn.pll_clk = plcd->config[LCD_CONFIG_DFT].data_rate[index] / 2;
	ext->params->dyn.switch_en = 1;
	ext->params->dyn.data_rate = plcd->config[LCD_CONFIG_DFT].data_rate[index];
	dbg_info("%s: index : %d, data rate: %d\n",
		__func__, index, ext->params->dyn.data_rate);
	mtk_disp_mipi_ccci_callback(1, 0);

	return 0;
}
#endif

static struct mtk_panel_funcs ext_funcs = {
	.framedone_notify = common_framedone_notify,
	.late_register = common_lcd_late_register,
	.set_power = common_lcd_set_power,
	.ext_param_set = common_lcd_ext_param_set,
	.mode_switch = common_lcd_mode_switch,
	.crtc_state_notify = common_lcd_crtc_state_notify,
	.set_dispon_cmdq = common_lcd_set_dispon_cmdq,
#if defined(CONFIG_SMCDSD_DOZE)
//	.doze_get_mode_flags = common_lcd_doze_get_mode_flags,
//	.doze_enable_start = common_lcd_doze_enable_start,
	.doze_enable = common_lcd_doze_enable,
//	.doze_area = common_lcd_doze_area,
	.doze_disable = common_lcd_doze_disable,
//	.doze_post_disp_on = common_lcd_doze_post_disp_on,
//	.set_aod_light_mode = common_lcd_set_aod_light_mode,
#endif
};
#endif

int smcdsd_panel_dsi_command_tx(void *drvdata,
	unsigned int id, unsigned long data0, unsigned int data1, bool need_lock)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	ssize_t ret = 0;
	struct mtk_ddic_dsi_msg dsi_msg;

	if (!plcd->lcdconnected)
		return ret;

	dsi_write_data_dump(id, data0, data1);

	memset(&dsi_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	dsi_msg.type[0] = id;
	dsi_msg.tx_len[0] = data1;
	dsi_msg.tx_buf[0] = (void *)data0;
	dsi_msg.tx_cmd_num = 1;

	dsi_msg.flags |= plcd->config[LCD_CONFIG_DFT].dsi.mode_flags & MIPI_DSI_MODE_LPM ? MIPI_DSI_MSG_USE_LPM : 0;

	ret = set_lcm_wrapper(&dsi_msg, 1);
	if (ret < 0) {
		dbg_info("%s: error(%d)\n", __func__, ret);
		plcd->error = ret;
		return -EINVAL;
	}

	return 0;
}

int smcdsd_dsi_msg_tx(void *drvdata, unsigned long data0, int blocking)
{
	struct mipi_dsi_lcd_common *plcd = drvdata ? drvdata : get_lcd_common(0);
	struct mtk_ddic_dsi_msg *dsi_msg = (struct mtk_ddic_dsi_msg *)data0;
	int ret = 0;

	if (!plcd->lcdconnected)
		return ret;

	dump_dsi_msg_tx(data0);

	ret = set_lcm_wrapper(dsi_msg, blocking);
	if (ret < 0) {
		dbg_info("%s: error(%d)\n", __func__, ret);
		plcd->error = ret;
		return -EINVAL;
	}

	return ret;
}

struct mtk_ddic_dsi_msg rx_cmd_msg;
unsigned char rx_buf[RT_MAX_NUM * 2];
int smcdsd_panel_dsi_command_rx(void *drvdata,
	unsigned int id, unsigned int offset, u8 cmd, unsigned int len, u8 *buf, bool need_lock)
{
	unsigned int i = 0, j = 0;
	unsigned long ret_dlen = 0;
	int ret;
	struct mtk_ddic_dsi_msg *cmd_msg = &rx_cmd_msg;
	u8 tx[10] = {0};

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

	ret = read_lcm_wrapper(cmd_msg);
	if (ret != 0) {
		dbg_info("%s error\n", __func__);
		goto  done;
	}

	for (i = 0; i < (u16)cmd_msg->rx_cmd_num; i++) {
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

int smcdsd_panel_commit_lock(void *drvdata, bool need_lock)
{
	int ret = 0;

	ret = mtk_crtc_lock_control(need_lock);

	return ret;
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
	plcd->commit_lock= smcdsd_panel_commit_lock;

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

	run_list(&plcd->drv->pdev->dev, "panel_regulator_init");

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

