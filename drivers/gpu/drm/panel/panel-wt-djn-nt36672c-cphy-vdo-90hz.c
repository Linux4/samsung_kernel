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
#include "panel_notifier.h"

struct djn {
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

#define djn_dcs_write_seq(ctx, seq...)                                  \
	({                                                              \
	 const u8 d[] = { seq };                                        \
	 BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
		 "DCS sequence too big for stack");                     \
	 djn_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	 })

#define djn_dcs_write_seq_static(ctx, seq...)                           \
	({                                                              \
	 static const u8 d[] = { seq };                                 \
	 djn_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	 })

static inline struct djn *panel_to_djn(struct drm_panel *panel)
{
	return container_of(panel, struct djn, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int djn_dcs_read(struct djn *ctx, u8 cmd, void *data, size_t len)
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

static void djn_panel_get_data(struct djn *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = djn_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
				ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void djn_dcs_write(struct djn *ctx, const void *data, size_t len)
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

static void djn_panel_init(struct djn *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%s+\n", __func__);

	djn_dcs_write_seq_static(ctx,0XFF,0X10);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0XB0,0X00);
	djn_dcs_write_seq_static(ctx,0XC0,0X00);
	djn_dcs_write_seq_static(ctx,0XC1,0X89,0X28,0X00,0X08,0X00,0XAA,0X02,0X0E,0X00,0X2B,0X00,0X07,0X0D,0XB7,0X0C,0XB7);
	djn_dcs_write_seq_static(ctx,0XC2,0X1B,0XA0);

	djn_dcs_write_seq_static(ctx,0XFF,0X20);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X01,0X66);
	djn_dcs_write_seq_static(ctx,0X06,0X40);
	djn_dcs_write_seq_static(ctx,0X07,0X38);
	djn_dcs_write_seq_static(ctx,0X1B,0X01);
	djn_dcs_write_seq_static(ctx,0X5C,0X90);
	djn_dcs_write_seq_static(ctx,0X5E,0XB2);
	djn_dcs_write_seq_static(ctx,0X69,0XD1);
	djn_dcs_write_seq_static(ctx,0X95,0XD1);
	djn_dcs_write_seq_static(ctx,0X96,0XD1);
	djn_dcs_write_seq_static(ctx,0XF2,0X64);
	djn_dcs_write_seq_static(ctx,0XF4,0X64);
	djn_dcs_write_seq_static(ctx,0XF6,0X64);
	djn_dcs_write_seq_static(ctx,0XF8,0X64);

	djn_dcs_write_seq_static(ctx,0XFF,0X21);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);

	djn_dcs_write_seq_static(ctx,0XFF,0X24);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X01,0X0F);
	djn_dcs_write_seq_static(ctx,0X03,0X0C);
	djn_dcs_write_seq_static(ctx,0X05,0X1D);
	djn_dcs_write_seq_static(ctx,0X08,0X2F);
	djn_dcs_write_seq_static(ctx,0X09,0X2E);
	djn_dcs_write_seq_static(ctx,0X0A,0X2D);
	djn_dcs_write_seq_static(ctx,0X0B,0X2C);
	djn_dcs_write_seq_static(ctx,0X11,0X17);
	djn_dcs_write_seq_static(ctx,0X12,0X13);
	djn_dcs_write_seq_static(ctx,0X13,0X15);
	djn_dcs_write_seq_static(ctx,0X15,0X14);
	djn_dcs_write_seq_static(ctx,0X16,0X16);
	djn_dcs_write_seq_static(ctx,0X17,0X18);
	djn_dcs_write_seq_static(ctx,0X1B,0X01);
	djn_dcs_write_seq_static(ctx,0X1D,0X1D);
	djn_dcs_write_seq_static(ctx,0X20,0X2F);
	djn_dcs_write_seq_static(ctx,0X21,0X2E);
	djn_dcs_write_seq_static(ctx,0X22,0X2D);
	djn_dcs_write_seq_static(ctx,0X23,0X2C);
	djn_dcs_write_seq_static(ctx,0X29,0X17);
	djn_dcs_write_seq_static(ctx,0X2A,0X13);
	djn_dcs_write_seq_static(ctx,0X2B,0X15);
	djn_dcs_write_seq_static(ctx,0X2F,0X14);
	djn_dcs_write_seq_static(ctx,0X30,0X16);
	djn_dcs_write_seq_static(ctx,0X31,0X18);
	djn_dcs_write_seq_static(ctx,0X32,0X04);
	djn_dcs_write_seq_static(ctx,0X34,0X10);
	djn_dcs_write_seq_static(ctx,0X35,0X1F);
	djn_dcs_write_seq_static(ctx,0X36,0X1F);
	djn_dcs_write_seq_static(ctx,0X4D,0X19);
	djn_dcs_write_seq_static(ctx,0X4E,0X45);
	djn_dcs_write_seq_static(ctx,0X4F,0X45);
	djn_dcs_write_seq_static(ctx,0X53,0X45);
	djn_dcs_write_seq_static(ctx,0X71,0X30);
	djn_dcs_write_seq_static(ctx,0X79,0X11);
	djn_dcs_write_seq_static(ctx,0X7A,0X82);
	djn_dcs_write_seq_static(ctx,0X7B,0X94);
	djn_dcs_write_seq_static(ctx,0X7D,0X04);
	djn_dcs_write_seq_static(ctx,0X80,0X04);
	djn_dcs_write_seq_static(ctx,0X81,0X04);
	djn_dcs_write_seq_static(ctx,0X82,0X13);
	djn_dcs_write_seq_static(ctx,0X84,0X31);
	djn_dcs_write_seq_static(ctx,0X85,0X00);
	djn_dcs_write_seq_static(ctx,0X86,0X00);
	djn_dcs_write_seq_static(ctx,0X87,0X00);
	djn_dcs_write_seq_static(ctx,0X90,0X13);
	djn_dcs_write_seq_static(ctx,0X92,0X31);
	djn_dcs_write_seq_static(ctx,0X93,0X00);
	djn_dcs_write_seq_static(ctx,0X94,0X00);
	djn_dcs_write_seq_static(ctx,0X95,0X00);
	djn_dcs_write_seq_static(ctx,0X9C,0XF4);
	djn_dcs_write_seq_static(ctx,0X9D,0X01);
	djn_dcs_write_seq_static(ctx,0XA0,0X14);
	djn_dcs_write_seq_static(ctx,0XA2,0X14);
	djn_dcs_write_seq_static(ctx,0XA3,0X02);
	djn_dcs_write_seq_static(ctx,0XA4,0X04);
	djn_dcs_write_seq_static(ctx,0XA5,0X04);
	djn_dcs_write_seq_static(ctx,0XC6,0XC0);
	djn_dcs_write_seq_static(ctx,0XC9,0X00);
	djn_dcs_write_seq_static(ctx,0XD9,0X80);
	djn_dcs_write_seq_static(ctx,0XE9,0X02);

	djn_dcs_write_seq_static(ctx,0XFF,0X25);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X0F,0X1B);
	djn_dcs_write_seq_static(ctx,0X19,0XE4);
	djn_dcs_write_seq_static(ctx,0X21,0X40);
	djn_dcs_write_seq_static(ctx,0X66,0XD8);
	djn_dcs_write_seq_static(ctx,0X68,0X50);
	djn_dcs_write_seq_static(ctx,0X69,0X10);
	djn_dcs_write_seq_static(ctx,0X6B,0X00);
	djn_dcs_write_seq_static(ctx,0X6D,0X0D);
	djn_dcs_write_seq_static(ctx,0X6E,0X48);
	djn_dcs_write_seq_static(ctx,0X72,0X41);
	djn_dcs_write_seq_static(ctx,0X73,0X4A);
	djn_dcs_write_seq_static(ctx,0X74,0XD0);
	djn_dcs_write_seq_static(ctx,0X77,0X62);
	djn_dcs_write_seq_static(ctx,0X7D,0X40);
	djn_dcs_write_seq_static(ctx,0X7F,0X00);
	djn_dcs_write_seq_static(ctx,0X80,0X04);
	djn_dcs_write_seq_static(ctx,0X84,0X0D);
	djn_dcs_write_seq_static(ctx,0XCF,0X80);
	djn_dcs_write_seq_static(ctx,0XD6,0X80);
	djn_dcs_write_seq_static(ctx,0XD7,0X80);
	djn_dcs_write_seq_static(ctx,0XEF,0X20);
	djn_dcs_write_seq_static(ctx,0XF0,0X84);

	djn_dcs_write_seq_static(ctx,0XFF,0X26);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X15,0X04);
	djn_dcs_write_seq_static(ctx,0X81,0X14);
	djn_dcs_write_seq_static(ctx,0X83,0X02);
	djn_dcs_write_seq_static(ctx,0X84,0X03);
	djn_dcs_write_seq_static(ctx,0X85,0X01);
	djn_dcs_write_seq_static(ctx,0X86,0X03);
	djn_dcs_write_seq_static(ctx,0X87,0X01);
	djn_dcs_write_seq_static(ctx,0X88,0X06);
	djn_dcs_write_seq_static(ctx,0X8A,0X1A);
	djn_dcs_write_seq_static(ctx,0X8B,0X11);
	djn_dcs_write_seq_static(ctx,0X8C,0X24);
	djn_dcs_write_seq_static(ctx,0X8E,0X42);
	djn_dcs_write_seq_static(ctx,0X8F,0X11);
	djn_dcs_write_seq_static(ctx,0X90,0X11);
	djn_dcs_write_seq_static(ctx,0X91,0X11);
	djn_dcs_write_seq_static(ctx,0X9A,0X81);
	djn_dcs_write_seq_static(ctx,0X9B,0X03);
	djn_dcs_write_seq_static(ctx,0X9C,0X00);
	djn_dcs_write_seq_static(ctx,0X9D,0X00);
	djn_dcs_write_seq_static(ctx,0X9E,0X00);

	djn_dcs_write_seq_static(ctx,0XFF,0X27);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X01,0X68);
	djn_dcs_write_seq_static(ctx,0X20,0X81);
	djn_dcs_write_seq_static(ctx,0X21,0XE9);
	djn_dcs_write_seq_static(ctx,0X25,0X82);
	djn_dcs_write_seq_static(ctx,0X26,0X1E);
	djn_dcs_write_seq_static(ctx,0X6E,0X00);
	djn_dcs_write_seq_static(ctx,0X6F,0X00);
	djn_dcs_write_seq_static(ctx,0X70,0X00);
	djn_dcs_write_seq_static(ctx,0X71,0X00);
	djn_dcs_write_seq_static(ctx,0X72,0X00);
	djn_dcs_write_seq_static(ctx,0X75,0X00);
	djn_dcs_write_seq_static(ctx,0X76,0X00);
	djn_dcs_write_seq_static(ctx,0X77,0X00);
	djn_dcs_write_seq_static(ctx,0X7D,0X09);
	djn_dcs_write_seq_static(ctx,0X7E,0X67);
	djn_dcs_write_seq_static(ctx,0X80,0X23);
	djn_dcs_write_seq_static(ctx,0X82,0X09);
	djn_dcs_write_seq_static(ctx,0X83,0X67);
	djn_dcs_write_seq_static(ctx,0X88,0X01);
	djn_dcs_write_seq_static(ctx,0X89,0X10);
	djn_dcs_write_seq_static(ctx,0XA6,0X23);
	djn_dcs_write_seq_static(ctx,0XA7,0X01);
	djn_dcs_write_seq_static(ctx,0XB6,0X40);

	djn_dcs_write_seq_static(ctx,0XFF,0X2A);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X00,0X91);
	djn_dcs_write_seq_static(ctx,0X03,0X20);
	djn_dcs_write_seq_static(ctx,0X07,0X5A);
	djn_dcs_write_seq_static(ctx,0X0A,0X70);
	djn_dcs_write_seq_static(ctx,0X0D,0X40);
	djn_dcs_write_seq_static(ctx,0X0E,0X02);
	djn_dcs_write_seq_static(ctx,0X11,0XF0);
	djn_dcs_write_seq_static(ctx,0X15,0X0F);
	djn_dcs_write_seq_static(ctx,0X16,0X1A);
	djn_dcs_write_seq_static(ctx,0X19,0X0E);
	djn_dcs_write_seq_static(ctx,0X1A,0XEE);
	djn_dcs_write_seq_static(ctx,0X1B,0X14);
	djn_dcs_write_seq_static(ctx,0X1D,0X36);
	djn_dcs_write_seq_static(ctx,0X1E,0X4F);
	djn_dcs_write_seq_static(ctx,0X1F,0X4F);
	djn_dcs_write_seq_static(ctx,0X20,0X4F);
	djn_dcs_write_seq_static(ctx,0X28,0XF7);
	djn_dcs_write_seq_static(ctx,0X29,0X06);
	djn_dcs_write_seq_static(ctx,0X2A,0X03);
	djn_dcs_write_seq_static(ctx,0X2F,0X01);
	djn_dcs_write_seq_static(ctx,0X30,0X45);
	djn_dcs_write_seq_static(ctx,0X31,0XBD);
	djn_dcs_write_seq_static(ctx,0X34,0XF9);
	djn_dcs_write_seq_static(ctx,0X35,0X31);
	djn_dcs_write_seq_static(ctx,0X36,0X05);
	djn_dcs_write_seq_static(ctx,0X37,0XF4);
	djn_dcs_write_seq_static(ctx,0X38,0X35);
	djn_dcs_write_seq_static(ctx,0X39,0X01);
	djn_dcs_write_seq_static(ctx,0X3A,0X45);

	djn_dcs_write_seq_static(ctx,0XFF,0X2C);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);

	djn_dcs_write_seq_static(ctx,0XFF,0XE0);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X35,0X82);

	djn_dcs_write_seq_static(ctx,0XFF,0XF0);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X1C,0X01);
	djn_dcs_write_seq_static(ctx,0X33,0X01);
	djn_dcs_write_seq_static(ctx,0X5A,0X00);
	djn_dcs_write_seq_static(ctx,0XD2,0X52);

	djn_dcs_write_seq_static(ctx,0XFF,0XD0);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X53,0X22);
	djn_dcs_write_seq_static(ctx,0X54,0X02);

	djn_dcs_write_seq_static(ctx,0XFF,0XC0);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0X9C,0X11);
	djn_dcs_write_seq_static(ctx,0X9D,0X11);

	djn_dcs_write_seq_static(ctx,0XFF,0X2B);
	djn_dcs_write_seq_static(ctx,0XFB,0X01);
	djn_dcs_write_seq_static(ctx,0XB7,0X1A);
	djn_dcs_write_seq_static(ctx,0XB8,0X1D);
	djn_dcs_write_seq_static(ctx,0XC0,0X01);

	djn_dcs_write_seq_static(ctx,0xFF,0x23);
	djn_dcs_write_seq_static(ctx,0xFB,0x01);
	djn_dcs_write_seq_static(ctx,0x00,0x00);

	djn_dcs_write_seq_static(ctx,0x07,0x20);
	djn_dcs_write_seq_static(ctx,0x08,0x03);
	djn_dcs_write_seq_static(ctx,0x09,0x2E);

	djn_dcs_write_seq_static(ctx,0XFF,0X10);
	djn_dcs_write_seq_static(ctx,0X53,0x2C);
	djn_dcs_write_seq_static(ctx,0x68,0x02,0x01);
	djn_dcs_write_seq_static(ctx,0X11,0x00);
	msleep(120);
	djn_dcs_write_seq_static(ctx,0X29,0x00);
	pr_info("%s-\n", __func__);
}

static int djn_disable(struct drm_panel *panel)
{
	struct djn *ctx = panel_to_djn(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int djn_unprepare(struct drm_panel *panel)
{

	struct djn *ctx = panel_to_djn(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;
	//add for TP suspend
	panel_notifier_call_chain(PANEL_UNPREPARE, NULL);

	djn_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	djn_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(110);

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

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int djn_prepare(struct drm_panel *panel)
{
	struct djn *ctx = panel_to_djn(panel);
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
	djn_panel_init(ctx);

	panel_notifier_call_chain(PANEL_PREPARE, NULL);

	ret = ctx->error;
	if (ret < 0)
		djn_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	djn_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int djn_enable(struct drm_panel *panel)
{
	struct djn *ctx = panel_to_djn(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (56)
#define HSA (12)
#define HBP (46)
#define VFP_90HZ (54)
#define VFP_60HZ (1295)
#define VSA (4)
#define VBP (16)
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
	.pll_clk = 533,
	//.vfp_low_power = VFP_45HZ,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x1D,
	},
	.is_cphy = 1,
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
	.pll_clk = 533,
	//.vfp_low_power = VFP_45HZ,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x1D,
	},
	.is_cphy = 1,
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

static int djn_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
		unsigned int level)
{

#if 1
	char bl_tb0[] = {0x51, 0xff};
#else
	char bl_tb0[] = {0x51, 0x0f, 0xf0};
#endif
	pr_info("%s backlight = -%d\n", __func__, level);

	if (level > 255)
		level = 255;

#if 1
	bl_tb0[1] = (u8)level;
	pr_info("%s backlight = -%d,bl_tb0[1] = %x\n", __func__, level, bl_tb0[1]);
#else
	level = level << 4;
	bl_tb0[1] = (u8)((level & 0x0F00) >> 8);
	bl_tb0[2] = (u8)level & 0xFF;
	pr_info("%s backlight = -%d, bl_tb0[1] = %x bl_tb0[2] = %x\n", __func__, level, bl_tb0[1], bl_tb0[2]);
#endif

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

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
	struct djn *ctx = panel_to_djn(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = djn_setbacklight_cmdq,
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

static int djn_get_modes(struct drm_panel *panel)
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

static const struct drm_panel_funcs djn_drm_funcs = {
	.disable = djn_disable,
	.unprepare = djn_unprepare,
	.prepare = djn_prepare,
	.enable = djn_enable,
	.get_modes = djn_get_modes,
};

static int djn_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct djn *ctx;
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
	ctx = devm_kzalloc(dev, sizeof(struct djn), GFP_KERNEL);
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
	ctx->panel.funcs = &djn_drm_funcs;

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

	pr_info("%s- wt, djn, nt36672c, cphy, vdo, 90hz\n", __func__);

	return ret;
}

static int djn_remove(struct mipi_dsi_device *dsi)
{
	struct djn *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id djn_of_match[] = {
	{
		.compatible = "wt,nt36672c_fhdp_wt_dsi_vdo_cphy_90hz_djn",
	},
	{}
};

MODULE_DEVICE_TABLE(of, djn_of_match);

static struct mipi_dsi_driver djn_driver = {
	.probe = djn_probe,
	.remove = djn_remove,
	.driver = {
		.name = "nt36672c_fhdp_wt_dsi_vdo_cphy_90hz_djn",
		.owner = THIS_MODULE,
		.of_match_table = djn_of_match,
	},
};

module_mipi_dsi_driver(djn_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt djn nt36672c vdo Panel Driver");
MODULE_LICENSE("GPL v2");
