// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif


enum panel_version{
	PANEL_V1 = 1,
	PANEL_V2,
	PANEL_V3,
};

unsigned int nt37707_cmd_fhd_buf_thresh[14] = {
	896, 1792, 2688, 3584, 4480,
	5376, 6272, 6720, 7168, 7616,
	7744, 7872, 8000, 8064};
unsigned int nt37707_cmd_fhd_range_min_qp[15] = {
	0, 4, 5, 5, 7, 7,
	7, 7, 7, 7, 9, 9,
	9, 11, 17};
unsigned int nt37707_cmd_fhd_range_max_qp[15] = {
	8, 8, 9, 10, 11, 11,
	11, 12, 13, 14, 15, 16,
	17, 17, 19};
int nt37707_cmd_fhd_range_bpg_ofs[15] = {
	2, 0, 0, -2, -4, -6,
	-8, -8, -8, -10, -10, -12,
	-12, -12, -12};

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	bool prepared;
	bool enabled;

	int error;
	unsigned int hbm_mode;
	unsigned int dc_mode;
	unsigned int current_bl;
	unsigned int current_fps;
	enum panel_version version;
};

struct lcm *g_ctx;
static atomic_t current_backlight;

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)  \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
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

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
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

static void lcm_panel_init(struct lcm *ctx)
{
	char bl_tb[] = {0x51, 0x0f, 0xff};
	unsigned int level = 0;

	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x07);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x00, 0x2F);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x00, 0x2D, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x18);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x07, 0x19);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x07, 0x1A, 0x00, 0x00, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x6F);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x02, 0x1C, 0x04, 0x38, 0x02, 0x1C, 0x0A, 0xE0, 0x60);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xFD, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x70, 0x70, 0x70, 0x70, 0x70);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x16);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x02, 0x74);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x01, 0x01, 0x02, 0x01, 0x01,
		0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x00, 0x01, 0x00, 0x01, 0x00,
		0x03, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x3C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x4C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x5C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x6C);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x74);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x01, 0x03, 0x04, 0x0B, 0x77,
		0x02, 0x08, 0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x84);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x94);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0xA4);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x03, 0x02, 0x01, 0x00,
		0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0xB4);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x70, 0xF9, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0xB8);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x22, 0x22, 0x42, 0x44, 0x00,
		0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x00, 0x00, 0x10, 0x11, 0x00,
		0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x1C);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x05, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0A, 0x4F);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x5F, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x09, 0x60);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x00, 0x04, 0x04, 0x0C, 0x04,
		0x0C, 0x07, 0x60, 0x07, 0x60, 0x08, 0xE0, 0x08, 0xE0, 0x0C,
		0x28, 0x0C, 0x28, 0x11, 0x10, 0x11, 0x10, 0x14, 0xA8, 0x14, 0xA8);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x14, 0xAC, 0x14, 0xAC, 0x1A,
		0x3C, 0x1A, 0x3C, 0x24, 0x20, 0x24, 0x20, 0x36, 0xE8, 0x36, 0xE8, 0x3F, 0xFC);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x0A, 0x4F);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x43);
	lcm_dcs_write_seq_static(ctx, 0x91, 0xAB, 0x28, 0x00, 0x0C, 0xC2,
		0x00, 0x02, 0x0E, 0x01, 0x1F, 0x00, 0x07, 0x08, 0xBB, 0x08, 0x7A, 0x10, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x00);
	//backlight
	level = atomic_read(&current_backlight);
	bl_tb[1] = (char)((level >> 8) & 0xFF);
	bl_tb[2] = (char)(level & 0xFF);
	lcm_dcs_write(ctx, bl_tb, ARRAY_SIZE(bl_tb));

	lcm_dcs_write_seq_static(ctx, 0x11);
	usleep_range(120 * 1000, 121 * 1000);
	lcm_dcs_write_seq_static(ctx, 0x29);

	pr_info("%s-\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int panel_ext_init_power(struct drm_panel *panel);//
static int panel_ext_powerdown(struct drm_panel *panel);//
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);


	pr_info("%s+\n", __func__);
	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	lcm_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	ctx->error = 0;
	ctx->prepared = false;

	panel_ext_powerdown(panel);//

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	panel_ext_init_power(panel);//

	// lcd reset L->H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(11000, 11001);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(1000, 1001);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1000, 1001);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(11000, 11001);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		goto error;

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
error:
	lcm_unprepare(panel);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (40)
#define HSA (10)
#define HBP (20)
#define HACT (1080)
#define VFP (50)
#define VSA (2)
#define VBP (8)
#define VACT (2640)

#define DATA_RATE_120	(900)
#define DATA_RATE_90	(899) //Wait DDIC Vendor fix mode switch TE abnormal issue
#define DATA_RATE_60	(898) //Wait DDIC Vendor fix mode switch TE abnormal issue

static const struct drm_display_mode switch_mode_120hz = {
	.clock		= 373000,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};

static const struct drm_display_mode switch_mode_90hz = {
	.clock = 279750,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};

static const struct drm_display_mode switch_mode_60hz = {
	.clock = 186500,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};

static const struct drm_display_mode switch_mode_30hz = {
	.clock = 93250,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};
static const struct drm_display_mode switch_mode_24hz = {
	.clock = 74600,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};
static const struct drm_display_mode switch_mode_10hz = {
	.clock = 31100,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};
static const struct drm_display_mode switch_mode_1hz = {
	.clock = 3110,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,
};


#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_30hz = {
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.data_rate = DATA_RATE_120,
	},
	.data_rate = DATA_RATE_120,
	.lp_perline_en = 1,

	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68256,
	.physical_height_um = 151680,
	.lcm_index = 0,

	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2640,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37707_cmd_fhd_buf_thresh,
			.range_min_qp = nt37707_cmd_fhd_range_min_qp,
			.range_max_qp = nt37707_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37707_cmd_fhd_range_bpg_ofs,
		},
	},
	//.max_bl_level = 16380,
	//.hbm_type = HBM_MODE_DCS_ONLY,
	//.te_delay = 1,
};

static struct mtk_panel_params ext_params_60hz = {
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
		.data_rate = DATA_RATE_60,
	},
	.data_rate = DATA_RATE_120,
	.lp_perline_en = 1,

	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68256,
	.physical_height_um = 151680,
	.lcm_index = 0,

	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2640,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37707_cmd_fhd_buf_thresh,
			.range_min_qp = nt37707_cmd_fhd_range_min_qp,
			.range_max_qp = nt37707_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37707_cmd_fhd_range_bpg_ofs,
		},
	},
	//.max_bl_level = 16380,
	//.hbm_type = HBM_MODE_DCS_ONLY,
	//.te_delay = 1,
};


static struct mtk_panel_params ext_params_90hz = {
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.data_rate = DATA_RATE_90,
	},
	.data_rate = DATA_RATE_120,
	.lp_perline_en = 1,

	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68256,
	.physical_height_um = 151680,
	.lcm_index = 0,

	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2640,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37707_cmd_fhd_buf_thresh,
			.range_min_qp = nt37707_cmd_fhd_range_min_qp,
			.range_max_qp = nt37707_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37707_cmd_fhd_range_bpg_ofs,
		},
	},
	//.max_bl_level = 16380,
	//.hbm_type = HBM_MODE_DCS_ONLY,
	//.te_delay = 1,
};

static struct mtk_panel_params ext_params_120hz = {
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.data_rate = DATA_RATE_120,
	},
	.data_rate = DATA_RATE_120,
	.lp_perline_en = 1,

	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68256,
	.physical_height_um = 151680,
	.lcm_index = 0,

	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2640,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37707_cmd_fhd_buf_thresh,
			.range_min_qp = nt37707_cmd_fhd_range_min_qp,
			.range_max_qp = nt37707_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37707_cmd_fhd_range_bpg_ofs,
		},
	},
	//.max_bl_level = 16380,
	//.hbm_type = HBM_MODE_DCS_ONLY,
	//.te_delay = 1,
};

static struct mtk_panel_params ext_params_24hz = {
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.data_rate = DATA_RATE_120,
	},
	.data_rate = DATA_RATE_120,
	.lp_perline_en = 1,

	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68256,
	.physical_height_um = 151680,
	.lcm_index = 0,

	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2640,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37707_cmd_fhd_buf_thresh,
			.range_min_qp = nt37707_cmd_fhd_range_min_qp,
			.range_max_qp = nt37707_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37707_cmd_fhd_range_bpg_ofs,
		},
	},
	//.max_bl_level = 16380,
	//.hbm_type = HBM_MODE_DCS_ONLY,
	//.te_delay = 1,
};

static struct mtk_panel_params ext_params_10hz = {
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.data_rate = DATA_RATE_120,
	},
	.data_rate = DATA_RATE_120,
	.lp_perline_en = 1,

	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68256,
	.physical_height_um = 151680,
	.lcm_index = 0,

	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2640,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37707_cmd_fhd_buf_thresh,
			.range_min_qp = nt37707_cmd_fhd_range_min_qp,
			.range_max_qp = nt37707_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37707_cmd_fhd_range_bpg_ofs,
		},
	},
	//.max_bl_level = 16380,
	//.hbm_type = HBM_MODE_DCS_ONLY,
	//.te_delay = 1,
};

static struct mtk_panel_params ext_params_1hz = {
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.data_rate = DATA_RATE_120,
	},
	.data_rate = DATA_RATE_120,
	.lp_perline_en = 1,

	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = 68256,
	.physical_height_um = 151680,
	.lcm_index = 0,

	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 2088,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2640,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = nt37707_cmd_fhd_buf_thresh,
			.range_min_qp = nt37707_cmd_fhd_range_min_qp,
			.range_max_qp = nt37707_cmd_fhd_range_max_qp,
			.range_bpg_ofs = nt37707_cmd_fhd_range_bpg_ofs,
		},
	},
	//.max_bl_level = 16380,
	//.hbm_type = HBM_MODE_DCS_ONLY,
	//.te_delay = 1,
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = { 0x51, 0x0f, 0xff};
	struct lcm *ctx = g_ctx;

	if (ctx->hbm_mode) {
		pr_info("hbm_mode = %d, skip backlight(%d)\n", ctx->hbm_mode, level);
		return 0;
	}

	if (!(ctx->current_bl && level))
		pr_info("backlight changed from %u to %u\n", ctx->current_bl, level);
	else
		pr_info("backlight changed from %u to %u\n", ctx->current_bl, level);

	bl_tb0[1] = (u8)((level>>8)&0xFF);
	bl_tb0[2] = (u8)(level&0xFF);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	ctx->current_bl = level;
	atomic_set(&current_backlight, level);
	return 0;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (drm_mode_vrefresh(m) == 1)
		ext->params = &ext_params_1hz;
	else if (drm_mode_vrefresh(m) == 10)
		ext->params = &ext_params_10hz;
	else if (drm_mode_vrefresh(m) == 24)
		ext->params = &ext_params_24hz;
	else if (drm_mode_vrefresh(m) == 30)
		ext->params = &ext_params_30hz;
	else if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params_60hz;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x53, 0x20);

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x04);

		ctx->current_fps = 90;
	}
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
		lcm_dcs_write_seq_static(ctx, 0x5A, 0x01);

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);

		ctx->current_fps = 120;
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
		lcm_dcs_write_seq_static(ctx, 0x5A, 0x02);//01

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x30);
		lcm_dcs_write_seq_static(ctx, 0x6D, 0x00, 0x00);

		lcm_dcs_write_seq_static(ctx, 0x6F, 0x06);
		lcm_dcs_write_seq_static(ctx, 0x6D, 0x03);

		lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);

		lcm_dcs_write_seq_static(ctx, 0xB2, 0x80);

		ctx->current_fps = 60;
	}
}

static void mode_switch_to_30(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
		lcm_dcs_write_seq_static(ctx, 0x5A, 0x02);

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x30);
		lcm_dcs_write_seq_static(ctx, 0x6D, 0x01, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x6F, 0x06);
		lcm_dcs_write_seq_static(ctx, 0x6D, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);

		lcm_dcs_write_seq_static(ctx, 0xB2, 0x80);
		lcm_dcs_write_seq_static(ctx, 0x6F, 0xD9);
		lcm_dcs_write_seq_static(ctx, 0xBA, 0x04);

		ctx->current_fps = 30;
	}
}

static void mode_switch_to_24(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x04);
		lcm_dcs_write_seq_static(ctx, 0x53, 0x28);
		lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x30);
		lcm_dcs_write_seq_static(ctx, 0x6D, 0x02, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);

		lcm_dcs_write_seq_static(ctx, 0xB2, 0XC0);

		ctx->current_fps = 24;
	}
}

static void mode_switch_to_10(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
		lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x30);
		lcm_dcs_write_seq_static(ctx, 0x6D, 0x03, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);

		lcm_dcs_write_seq_static(ctx, 0xB2, 0xC0);

		ctx->current_fps = 10;
	}
}

static void mode_switch_to_1(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
		lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
		lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x30);
		lcm_dcs_write_seq_static(ctx, 0x6D, 0x04, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);

		lcm_dcs_write_seq_static(ctx, 0xB2, 0xC0);

		ctx->current_fps = 1;
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);

	if (cur_mode == dst_mode)
		return ret;

	if (drm_mode_vrefresh(m) == 1) { /*switch to 1 */
		mode_switch_to_1(panel, stage);
	} else if (drm_mode_vrefresh(m) == 10) { /*switch to 10 */
		mode_switch_to_10(panel, stage);
	} else if (drm_mode_vrefresh(m) == 24) { /*switch to 24 */
		mode_switch_to_24(panel, stage);
	} else if (drm_mode_vrefresh(m) == 30) { /*switch to 30 */
		mode_switch_to_30(panel, stage);
	} else if (drm_mode_vrefresh(m) == 60) { /*switch to 60 */
		mode_switch_to_60(panel, stage);
	} else if (drm_mode_vrefresh(m) == 90) { /*switch to 90 */
		mode_switch_to_90(panel, stage);
	} else if (drm_mode_vrefresh(m) == 120) { /*switch to 120 */
		mode_switch_to_120(panel, stage);
	} else
		ret = 1;

	return ret;
}

static int panel_ext_init_power(struct drm_panel *panel)
{
	return 0;
}

static int panel_ext_powerdown(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}


static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.set_aod_light_mode = lcm_setbacklight_cmdq,
	//.init_power = panel_ext_init_power,
	//.power_down = panel_ext_powerdown,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	//.panel_feature_set = panel_feature_set,
	//.panel_hbm_waitfor_fps_valid = panel_hbm_waitfor_fps_valid,
};
#endif

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
	struct drm_display_mode *mode_3;
	struct drm_display_mode *mode_4;
	struct drm_display_mode *mode_5;
	struct drm_display_mode *mode_6;

	mode_5 = drm_mode_duplicate(connector->dev, &switch_mode_1hz);
	if (!mode_5) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_1hz.hdisplay, switch_mode_1hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_1hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_5);
	mode_5->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_5);

	mode_4 = drm_mode_duplicate(connector->dev, &switch_mode_10hz);
	if (!mode_4) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_10hz.hdisplay, switch_mode_10hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_10hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_4);
	mode_4->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_4);

	mode_3 = drm_mode_duplicate(connector->dev, &switch_mode_24hz);
	if (!mode_3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_24hz.hdisplay, switch_mode_24hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_24hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_3);
	mode_3->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_3);

	mode_2 = drm_mode_duplicate(connector->dev, &switch_mode_30hz);
	if (!mode_2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_30hz.hdisplay, switch_mode_30hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_30hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_2);


	mode_1 = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_60hz.hdisplay, switch_mode_60hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

	mode = drm_mode_duplicate(connector->dev, &switch_mode_120hz);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_120hz.hdisplay, switch_mode_120hz.vdisplay,
			drm_mode_vrefresh(&switch_mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode_6 = drm_mode_duplicate(connector->dev, &switch_mode_90hz);
	if (!mode_6) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 switch_mode_90hz.hdisplay, switch_mode_90hz.vdisplay,
			 drm_mode_vrefresh(&switch_mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_6);
	mode_6->type = DRM_MODE_TYPE_DRIVER ;
	drm_mode_probed_add(connector, mode_6);

	connector->display_info.width_mm = 68;
	connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret, i;
	const u32 *val;
	unsigned int lane_swap[6] = {0};
	unsigned int pn_swap[6] = {0};

	pr_info("%s+\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm(node: %s)\n", __func__, dev->of_node->name);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	g_ctx = ctx;
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET// MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

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

	if (of_property_read_bool(dev->of_node, "swap-from-dts")) {
		ret = of_property_read_u32_array(dev->of_node, "lane-swap-setting", lane_swap, 6);
		if (ret == 0) {
			pr_info("r66451 dsi node:%s set Lane Swap from dts\n",
						dsi_node->full_name);
			ext_params_120hz.lane_swap_en = 1;
			ext_params_90hz.lane_swap_en = 1;
			ext_params_60hz.lane_swap_en = 1;
			ext_params_30hz.lane_swap_en = 1;
			ext_params_24hz.lane_swap_en = 1;
			ext_params_10hz.lane_swap_en = 1;
			ext_params_1hz.lane_swap_en = 1;
			for (i = 0; i < 6; i++) {
				ext_params_120hz.lane_swap[0][i] = lane_swap[i];
				ext_params_120hz.lane_swap[1][i] = lane_swap[i];
				ext_params_90hz.lane_swap[0][i] = lane_swap[i];
				ext_params_90hz.lane_swap[1][i] = lane_swap[i];
				ext_params_60hz.lane_swap[0][i] = lane_swap[i];
				ext_params_60hz.lane_swap[1][i] = lane_swap[i];
				ext_params_30hz.lane_swap[0][i] = lane_swap[i];
				ext_params_30hz.lane_swap[1][i] = lane_swap[i];
				ext_params_24hz.lane_swap[0][i] = lane_swap[i];
				ext_params_24hz.lane_swap[1][i] = lane_swap[i];
				ext_params_10hz.lane_swap[0][i] = lane_swap[i];
				ext_params_10hz.lane_swap[1][i] = lane_swap[i];
				ext_params_1hz.lane_swap[0][i] = lane_swap[i];
				ext_params_1hz.lane_swap[1][i] = lane_swap[i];
			}
		}
		ret = of_property_read_u32_array(dev->of_node, "pn-swap-setting", pn_swap, 6);
		if (ret == 0) {
			pr_info("r66451 dsi node:%s set PN Swap from dts\n",
						dsi_node->full_name);
			for (i = 0; i < 6; i++) {
				ext_params_120hz.pn_swap[0][i] = pn_swap[i];
				ext_params_120hz.pn_swap[1][i] = pn_swap[i];
				ext_params_90hz.pn_swap[0][i] = pn_swap[i];
				ext_params_90hz.pn_swap[1][i] = pn_swap[i];
				ext_params_60hz.pn_swap[0][i] = pn_swap[i];
				ext_params_60hz.pn_swap[1][i] = pn_swap[i];
				ext_params_30hz.pn_swap[0][i] = pn_swap[i];
				ext_params_30hz.pn_swap[1][i] = pn_swap[i];
				ext_params_24hz.pn_swap[0][i] = pn_swap[i];
				ext_params_24hz.pn_swap[1][i] = pn_swap[i];
				ext_params_10hz.pn_swap[0][i] = pn_swap[i];
				ext_params_10hz.pn_swap[1][i] = pn_swap[i];
				ext_params_1hz.pn_swap[0][i] = pn_swap[i];
				ext_params_1hz.pn_swap[1][i] = pn_swap[i];
			}
		}
	}

	ctx->prepared = true;
	ctx->enabled = true;

	/* Set backlight default value */
	atomic_set(&current_backlight, 8190);

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	val = of_get_property(dev->of_node, "reg", NULL);
	ctx->version = val ? be32_to_cpup(val) : 1;

	pr_info("%s: panel version 0x%x\n", __func__, ctx->version);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_120hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif
	ctx->hbm_mode = 0;
	ctx->dc_mode = 0;

	ctx->current_fps = 120;

	return ret;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

}

static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "nt37707,cmd,667,fhdp",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-nt37707-667-fhdp-dphy-cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("nt37707 AMOLED CMD Panel Driver");
MODULE_LICENSE("GPL");
