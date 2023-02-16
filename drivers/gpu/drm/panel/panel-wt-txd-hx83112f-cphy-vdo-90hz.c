/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>

#include <linux/gpio/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_drm_graphics_base.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_panel_ext.h"
#endif

#include "lcm_cust_common.h"

extern u8 rec_esd;				//ESD recover flag global value

struct txd {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *pm_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;

	int error;
};

#define txd_dcs_write_seq(ctx, seq...)                                  \
	({                                                              \
	 const u8 d[] = { seq };                                        \
	 BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
		 "DCS sequence too big for stack");                     \
	 txd_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	 })

#define txd_dcs_write_seq_static(ctx, seq...)                           \
	({                                                              \
	 static const u8 d[] = { seq };                                 \
	 txd_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	 })

static inline struct txd *panel_to_txd(struct drm_panel *panel)
{
	return container_of(panel, struct txd, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int txd_dcs_read(struct txd *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
				cmd);
		ctx->error = ret;
	}

	return ret;
}

static void txd_panel_get_data(struct txd *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = txd_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
				ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void txd_dcs_write(struct txd *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static u8 last_level[1];		//the last backlight level value

static void txd_panel_init(struct txd *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%s+\n", __func__);

	txd_dcs_write_seq_static(ctx, 0xB9, 0x83, 0x11, 0x2F);
	txd_dcs_write_seq_static(ctx, 0xCE, 0x00, 0x50, 0x00, 0x01);
	txd_dcs_write_seq_static(ctx, 0xB1, 0x40, 0x2A, 0x2A, 0x80, 0x80, 0xB8, 0x9E, 0x38, 0x1E, 0x06, 0x06);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x01);
	txd_dcs_write_seq_static(ctx, 0xB1, 0x9B, 0x00, 0x81);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x02);
	txd_dcs_write_seq_static(ctx, 0xB1, 0x2D, 0x2D);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x00);
	txd_dcs_write_seq_static(ctx, 0xB2, 0x00, 0x02, 0x08, 0x90, 0x68, 0x00, 0x1C, 0x4C, 0x10,
			0x08, 0x00, 0x6E, 0x22);
	txd_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x01, 0x02, 0x00, 0x1A, 0x00,
			0x0C, 0x00, 0x0D);
	txd_dcs_write_seq_static(ctx, 0xE9, 0xC5);
	txd_dcs_write_seq_static(ctx, 0xBA, 0x11);
	txd_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x01);
	txd_dcs_write_seq_static(ctx, 0xBF, 0x0B);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x00);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x01);
	txd_dcs_write_seq_static(ctx, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x33,
			0x33, 0x33, 0x33, 0x33);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x00);
	txd_dcs_write_seq_static(ctx, 0xC7, 0xF0, 0x20);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x01);
	txd_dcs_write_seq_static(ctx, 0xC7, 0x44);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x00);
	txd_dcs_write_seq_static(ctx, 0xCB, 0x5F, 0x19, 0x06, 0xE6);
	txd_dcs_write_seq_static(ctx, 0xCC, 0x0A);
	txd_dcs_write_seq_static(ctx, 0xD3, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
			0x00, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x02, 0x21, 0x32, 0x10, 0x1C, 0x00, 0x1C);
	txd_dcs_write_seq_static(ctx, 0xD5, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
			0x18, 0x18, 0x18, 0x18, 0x18, 0x37, 0x37, 0x36, 0x36, 0x35, 0x35, 0x31,
			0x31, 0x30, 0x30, 0x2F, 0x2F, 0x18, 0x18, 0x00, 0x01, 0x06, 0x07, 0x04,
			0x05, 0x02, 0x03, 0x20, 0x21, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
			0x18, 0x18, 0x18);
	txd_dcs_write_seq_static(ctx, 0xD6, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
			0x18, 0x18, 0x18, 0x18, 0x18, 0x37, 0x37, 0x36, 0x36, 0x35, 0x35, 0x31,
			0x31, 0x30, 0x30, 0x2F, 0x2F, 0x18, 0x18, 0x03, 0x02, 0x05, 0x04, 0x07,
			0x06, 0x01, 0x00, 0x21, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
			0x18, 0x18, 0x18);
	txd_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAB, 0xFA, 0xBA, 0xAA, 0xAA, 0xAA, 0xAB, 0xFA,
			0xBA, 0xAA, 0xAA);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x01);
	txd_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xAF, 0xEA, 0xAB, 0xFF, 0xAA, 0xAA, 0xAF,
			0xEA, 0xAB, 0xFF);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x02);
	txd_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xAF, 0xEA, 0xAB, 0xFF, 0xAA, 0xAA, 0xAF,
			0xEA, 0xAB, 0xFF);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x03);
	txd_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xAF, 0xEA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAF,
			0xEA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAF, 0xEA, 0xAB, 0xFF, 0xAA, 0xAA, 0xAF,
			0xEA, 0xAB, 0xFF);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x00);
	txd_dcs_write_seq_static(ctx, 0xE7, 0x4D, 0x2F, 0x0C, 0x0C, 0x23, 0x82, 0x35, 0x82, 0x39, 0x86, 0x04, 0x02);
	txd_dcs_write_seq_static(ctx, 0xE9, 0xD9);
	txd_dcs_write_seq_static(ctx, 0xE7, 0x08, 0x00, 0x00, 0x48, 0x17, 0x03);
	txd_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x01);
	txd_dcs_write_seq_static(ctx, 0xE7, 0x02, 0x50, 0xBA, 0x01, 0x3F, 0x07, 0x3A, 0x0C);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x02);
	txd_dcs_write_seq_static(ctx, 0xE7, 0x02, 0x02, 0x02, 0x02, 0x72, 0x01, 0x03, 0x01,
			0x03, 0x01, 0x03, 0x22, 0x20, 0x28, 0x40, 0x01, 0x00);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x03);
	txd_dcs_write_seq_static(ctx, 0xE7, 0x01, 0x01);
	txd_dcs_write_seq_static(ctx, 0xE1, 0x00);
	txd_dcs_write_seq_static(ctx, 0xBA, 0x00, 0x00, 0x20, 0xC0);
	txd_dcs_write_seq_static(ctx, 0xBD, 0x00);
	txd_dcs_write_seq_static(ctx, 0xC9, 0x00, 0x1E, 0x07, 0x34);	//25khz
	txd_dcs_write_seq_static(ctx, 0x53, 0x2C);
	//----------------------LCD initial code End----------------------//
	//SLPOUT and DISPON
	txd_dcs_write_seq_static(ctx, 0x11, 0x00);
	msleep(80);
	/* Display On*/
	txd_dcs_write_seq_static(ctx, 0x29, 0x00);
	msleep(20);
	txd_dcs_write_seq_static(ctx, 0xCD, 0x00, 0x3D, 0x00, 0x00, 0x01, 0x77, 0x10, 0x60, 0x10, 0x60, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3D, 0x58, 0x01);
	txd_dcs_write_seq_static(ctx, 0xCD, 0x01);
	txd_dcs_write_seq_static(ctx, 0x35, 0x00);

	if(1 == rec_esd) {
		pr_info("recover_esd_set_backlight last_level = %x\n", last_level[0]);
		msleep(200);
		txd_dcs_write_seq(ctx, 0x51, last_level[0]);
	}

	pr_info("%s-\n", __func__);
}

static int txd_disable(struct drm_panel *panel)
{
	struct txd *ctx = panel_to_txd(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

extern bool himax_gesture_status;//bug773028, fangzhihua.wt,mod, 20220628,TP bringup
static int txd_unprepare(struct drm_panel *panel)
{

	struct txd *ctx = panel_to_txd(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;

	txd_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	txd_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(110);

	if (himax_gesture_status == 1) {
		printk("[HXTP]txd_unprepare himax_gesture_status = 1\n ");
	} else {
		printk("[HXTP]txd_unprepare himax_gesture_status = 0\n ");
		txd_dcs_write_seq_static(ctx, 0xB9, 0x83, 0x11, 0x2F);	//himax_txd_deep_standby
		txd_dcs_write_seq_static(ctx, 0xB1, 0x01);
#if 0
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		usleep_range(2000, 2001);
#endif
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		usleep_range(2000, 2001);

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		usleep_range(2000, 2001);

		ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->pm_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->pm_gpio);
	}
	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int txd_prepare(struct drm_panel *panel)
{
	struct txd *ctx = panel_to_txd(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// reset  L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	usleep_range(1000, 1001);

	ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->pm_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->pm_gpio);
	usleep_range(5000, 5001);

	// end
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	usleep_range(2000, 2001);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	msleep(2);
	txd_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		txd_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	txd_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int txd_enable(struct drm_panel *panel)
{
	struct txd *ctx = panel_to_txd(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (174)
#define HSA (20)
#define HBP (22)
#define VFP_90HZ (78)
#define VFP_60HZ (1350)
#define VSA (15)
#define VBP (15)
#define VAC (2408)
#define HAC (1080)
static const struct drm_display_mode default_mode = {
	.clock = 301036,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60HZ,
	.vsync_end = VAC + VFP_60HZ + VSA,
	.vtotal = VAC + VFP_60HZ + VSA + VBP,
	.vrefresh = 60,
};

static const struct drm_display_mode performance_mode = {
	.clock = 301036,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90HZ,
	.vsync_end = VAC + VFP_90HZ + VSA,
	.vtotal = VAC + VFP_90HZ + VSA + VBP,
	.vrefresh = 90,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 530,
	//.vfp_low_power = VFP_45HZ,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x1D,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x09, .count = 1, .para_list[0] = 0x80,
	},
	.is_cphy = 1,
	.dyn = {
		.switch_en = 1,
		.data_rate = 1053,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	.phy_timcon = {
		.hs_zero = 35,
		.hs_trail = 26,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 530,
	//.vfp_low_power = VFP_45HZ,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x1D,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x09, .count = 1, .para_list[0] = 0x80,
	},
	.is_cphy = 1,
	.dyn = {
		.switch_en = 1,
		.data_rate = 1053,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
	},
	.phy_timcon = {
		.hs_zero = 35,
		.hs_trail = 26,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int txd_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
		unsigned int level)
{
	char bl_tb0[] = {0x51, 0xff};

	pr_info("%s backlight = -%d\n", __func__, level);

	if (level > 255)
		level = 255;

	bl_tb0[1] = (u8)level & 0xFF;
	pr_info("%s backlight = -%d, bl_tb0[1] = %x\n", __func__, level, bl_tb0[1]);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	last_level[0] = (u8)bl_tb0[1];

	return 0;
}

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
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);
	pr_info("samir:%s+ mode:%d\n", __func__, m->vrefresh);
	if (m->vrefresh == 60)
		ext->params = &ext_params;
	else if (m->vrefresh == 90)
		ext->params = &ext_params_90hz;
	else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct txd *ctx = panel_to_txd(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = txd_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	//.ext_param_get = mtk_panel_ext_param_get,
	//.mode_switch = mode_switch,
	.ata_check = panel_ata_check,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int txd_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	pr_info("samir:%s+ \n", __func__);
	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
				default_mode.hdisplay, default_mode.vdisplay,
				default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
				performance_mode.hdisplay,
				performance_mode.vdisplay,
				performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	panel->connector->display_info.width_mm = (68430/1000);
	panel->connector->display_info.height_mm = (152570/1000);

	return 1;
}

static const struct drm_panel_funcs txd_drm_funcs = {
	.disable = txd_disable,
	.unprepare = txd_unprepare,
	.prepare = txd_prepare,
	.enable = txd_enable,
	.get_modes = txd_get_modes,
};

static int txd_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct txd *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected, skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct txd), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 3;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
		MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
		MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
				PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	ctx->pm_gpio = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->pm_gpio)) {
		dev_info(dev, "cannot get pm_gpio %ld\n",
				PTR_ERR(ctx->pm_gpio));
		return PTR_ERR(ctx->pm_gpio);
	}
	devm_gpiod_put(dev, ctx->pm_gpio);
	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "cannot get bias-gpios 0 %ld\n",
				PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(dev, "cannot get bias-gpios 1 %ld\n",
				PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &txd_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;


	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	pr_info("%s- wt, txd, hx83112f, cphy, vdo, 90hz\n", __func__);

	return ret;
}

static int txd_remove(struct mipi_dsi_device *dsi)
{
	struct txd *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id txd_of_match[] = {
	{
		.compatible = "wt,hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_txd",
	},
	{}
};

MODULE_DEVICE_TABLE(of, txd_of_match);

static struct mipi_dsi_driver txd_driver = {
	.probe = txd_probe,
	.remove = txd_remove,
	.driver = {
		.name = "hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_txd",
		.owner = THIS_MODULE,
		.of_match_table = txd_of_match,
	},
};

module_mipi_dsi_driver(txd_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt txd hx83112f vdo Panel Driver");
MODULE_LICENSE("GPL v2");
