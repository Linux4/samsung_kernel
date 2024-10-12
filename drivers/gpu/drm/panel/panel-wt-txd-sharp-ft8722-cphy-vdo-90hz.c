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
extern u8 rec_esd;	//ESD recover flag global value

struct txdsharp {
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

#define txdsharp_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		txdsharp_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define txdsharp_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		txdsharp_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct txdsharp *panel_to_txdsharp(struct drm_panel *panel)
{
	return container_of(panel, struct txdsharp, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int txdsharp_dcs_read(struct txdsharp *ctx, u8 cmd, void *data, size_t len)
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

static void txdsharp_panel_get_data(struct txdsharp *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = txdsharp_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void txdsharp_dcs_write(struct txdsharp *ctx, const void *data, size_t len)
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

static int last_brightness = 0;	//the last backlight level values
u8 esd_bl6[] = {0x00, 0x00};

static void txdsharp_panel_init(struct txdsharp *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%sft8722 1074-1066-vfp\n", __func__);

	txdsharp_dcs_write_seq_static(ctx,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0xFF,0x87,0x22,0x01);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xFF,0x87,0x22);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA3);
	txdsharp_dcs_write_seq_static(ctx,0xB3,0x09,0x68);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0x5E,0x00,0x2C,0x00,0x14);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x90);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0x61,0x00,0x2C,0x00,0x14);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA0);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0x90,0x00,0x2C,0x00,0x14);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB0);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0xBB,0x00,0x2C,0x14);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xC1);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0x8D,0x00,0x7A,0x00,0x5E,0x00,0xB7);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x70);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0x61,0x00,0x2C,0x00,0x14);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA3);
	txdsharp_dcs_write_seq_static(ctx,0xC1,0x00,0x50,0x00,0x46,0x00,0x02);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB7);
	txdsharp_dcs_write_seq_static(ctx,0xC1,0x00,0x38);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x01,0x81,0xFF,0xFF,0x00,0x75,0x00,0x85,0x00,0xC8,0x00,0xC8,0x00,0x84,0x00,0x84);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x90);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x00,0xA7,0x10,0x43,0x00,0xA7,0x80,0xFF,0xFF,0x00,0x06,0x40,0x0D,0x0E,0x0E);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA0);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x00,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB0);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x22,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xD1);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x00,0x00,0x01,0x00,0x00,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xE1);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x07,0x02,0xB6,0x02,0xB6,0x02,0xB6,0x00,0x00,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xF1);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x15,0x15,0x1F,0x01,0x30,0x00,0xF3,0x01,0x28);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB0);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x00,0x00,0xDE,0xE2);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB5);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x05,0x05,0x94,0x98);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xC0);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x09,0x09,0x63,0x67);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xC5);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x09,0x09,0x69,0x6D);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x60);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x00,0x00,0x7F,0x83,0x05,0x05,0x23,0x27);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x70);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x00,0x00,0xC1,0xC5,0x05,0x05,0x71,0x75);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xD1);
	txdsharp_dcs_write_seq_static(ctx,0xC1,0x08,0x10,0x0B,0x52,0x13,0x70,0x05,0x5D,0x07,0x8B,0x0C,0xE4);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xE1);
	txdsharp_dcs_write_seq_static(ctx,0xC1,0x0B,0x52);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xE4);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x09,0xF8,0x09,0xF7,0x09,0xF7,0x09,0xF7,0x09,0xF7,0x09,0xF7);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xC1,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x90);
	txdsharp_dcs_write_seq_static(ctx,0xC1,0x03);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xF5);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x01);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xF6);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x5A);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xF1);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x5A);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xF7);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x11);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x8F);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x20);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xC2,0x82,0x00,0x01,0x8F,0x81,0x00,0x01,0x8F);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA0);
	txdsharp_dcs_write_seq_static(ctx,0xC2,0x82,0x03,0x00,0x01,0x8F,0x81,0x04,0x00,0x01,0x8F,0x80,0x05,0x00,0x01,0x8F);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB0);
	txdsharp_dcs_write_seq_static(ctx,0xC2,0x01,0x06,0x00,0x01,0x8F,0x02,0x06,0x00,0x01,0x8F,0x03,0x07,0x00,0x01,0x8F);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xC0);
	txdsharp_dcs_write_seq_static(ctx,0xC2,0x04,0x08,0x00,0x01,0x8F,0x05,0x09,0x00,0x01,0x8F);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xE0);
	txdsharp_dcs_write_seq_static(ctx,0xC2,0x77,0x77,0x77,0x77);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xE8);
	txdsharp_dcs_write_seq_static(ctx,0xC2,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xCB,0x00,0x01,0x00,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0xFD,0x01,0x03,0x00,0x00,0xFC);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x90);
	txdsharp_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFC,0x00,0x00,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA0);
	txdsharp_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xC0);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB0);
	txdsharp_dcs_write_seq_static(ctx,0xCB,0x10,0x50,0x05,0xC0);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xC0);
	txdsharp_dcs_write_seq_static(ctx,0xCB,0x10,0x50,0x05,0xC0);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xD5);
	txdsharp_dcs_write_seq_static(ctx,0xCB,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xE0);
	txdsharp_dcs_write_seq_static(ctx,0xCB,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xCC,0x22,0x2C,0x2C,0x2C,0x2C,0x2C,0x2C,0x1E,0x1D,0x1C,0x18,0x17,0x16,0x23,0x06,0x0C);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x90);
	txdsharp_dcs_write_seq_static(ctx,0xCC,0x0A,0x08,0x02,0x29,0x29,0x29,0x29,0x29);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA0);
	txdsharp_dcs_write_seq_static(ctx,0xCC,0x22,0x2C,0x2C,0x2C,0x2C,0x2C,0x2C,0x1E,0x1D,0x1C,0x18,0x17,0x16,0x23,0x0D,0x07);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB0);
	txdsharp_dcs_write_seq_static(ctx,0xCC,0x09,0x0B,0x03,0x29,0x29,0x29,0x29,0x29);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xCD,0x22,0x2C,0x2C,0x2C,0x2C,0x2C,0x2C,0x1E,0x1D,0x1C,0x18,0x17,0x16,0x23,0x07,0x0D);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x90);
	txdsharp_dcs_write_seq_static(ctx,0xCD,0x0B,0x09,0x03,0x29,0x29,0x29,0x29,0x29);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA0);
	txdsharp_dcs_write_seq_static(ctx,0xCD,0x22,0x2C,0x2C,0x2C,0x2C,0x2C,0x2C,0x1E,0x1D,0x1C,0x18,0x17,0x16,0x23,0x0C,0x06);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB0);
	txdsharp_dcs_write_seq_static(ctx,0xCD,0x08,0x0A,0x02,0x29,0x29,0x29,0x29,0x29);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x86);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0x00,0x00,0x01,0x11,0x11,0x11,0x05);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x96);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0x00,0x00,0x01,0x11,0x11,0x11,0x05);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA6);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0x00,0x00,0x01,0x11,0x11,0x11,0x05);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA3);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x00,0x00,0x00,0x01,0x11,0x05);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB3);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x00,0x00,0x00,0x01,0x11,0x05);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x76);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x00,0x00,0x00,0x01,0x15,0x15,0x15,0x05);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x82);
	txdsharp_dcs_write_seq_static(ctx,0xA7,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x8D);
	txdsharp_dcs_write_seq_static(ctx,0xA7,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x8F);
	txdsharp_dcs_write_seq_static(ctx,0xA7,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x93);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x33);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x97);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x33);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x9A);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x19);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x9C);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x19);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB6);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x17,0x17,0x0A,0x0A,0x17,0x17,0x0A,0x0A);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0xD8,0x2F,0x2F);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA0);
	txdsharp_dcs_write_seq_static(ctx,0xC3,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xA7,0x03);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB1);
	txdsharp_dcs_write_seq_static(ctx,0xF5,0x1F);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xCB);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x11);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x88);
	txdsharp_dcs_write_seq_static(ctx,0xC4,0x08);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x9A);
	txdsharp_dcs_write_seq_static(ctx,0xC4,0x11);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x82);
	txdsharp_dcs_write_seq_static(ctx,0xF5,0x01);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x93);
	txdsharp_dcs_write_seq_static(ctx,0xF5,0x01);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x99);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x50);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x9C);
	txdsharp_dcs_write_seq_static(ctx,0xF5,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x9E);
	txdsharp_dcs_write_seq_static(ctx,0xF5,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB0);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0xD0,0x4A,0x39,0xD0,0x4A,0x0E);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xE8);
	txdsharp_dcs_write_seq_static(ctx,0xC0,0x40);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x09,0x10,0x12,0x1A,0x22,0x29,0x33,0xE0,0x3B,0x41,0x47,0x4C,0x3A,0x51,0x59,0x60,0x67,0x59,0x6E,0x74,0x7C,0x84,0x31,0x8D,0x92,0x99,0xA0,0xB0,0xA8,0xB2,0xBF,0xC8,0x6E,0xD4,0xE5,0xF3,0xFF,0x08);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x30);
	txdsharp_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x09,0x10,0x12,0x1A,0x22,0x29,0x33,0xE0,0x3B,0x41,0x47,0x4C,0x3A,0x51,0x59,0x60,0x67,0x59,0x6E,0x74,0x7C,0x84,0x31,0x8D,0x92,0x99,0xA0,0xB0,0xA8,0xB2,0xBF,0xC8,0x6E,0xD4,0xE5,0xF3,0xFF,0x08);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x60);
	txdsharp_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x09,0x10,0x12,0x1A,0x22,0x29,0x33,0xE0,0x3B,0x41,0x47,0x4C,0x3A,0x51,0x59,0x60,0x67,0x59,0x6E,0x74,0x7C,0x84,0x31,0x8D,0x92,0x99,0xA0,0xB0,0xA8,0xB2,0xBF,0xC8,0x6E,0xD4,0xE5,0xF3,0xFF,0x08);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x90);
	txdsharp_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x09,0x10,0x12,0x1A,0x22,0x29,0x33,0xE0,0x3B,0x41,0x47,0x4C,0x3A,0x51,0x59,0x60,0x67,0x59,0x6E,0x74,0x7C,0x84,0x31,0x8D,0x92,0x99,0xA0,0xB0,0xA8,0xB2,0xBF,0xC8,0x6E,0xD4,0xE5,0xF3,0xFF,0x08);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xC0);
	txdsharp_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x09,0x10,0x12,0x1A,0x22,0x29,0x33,0xE0,0x3B,0x41,0x47,0x4C,0x3A,0x51,0x59,0x60,0x67,0x59,0x6E,0x74,0x7C,0x84,0x31,0x8D,0x92,0x99,0xA0,0xB0,0xA8,0xB2,0xBF,0xC8,0x6E,0xD4,0xE5,0xF3,0xFF,0x08);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xF0);
	txdsharp_dcs_write_seq_static(ctx,0xE1,0x00,0x04,0x09,0x10,0x12,0x1A,0x22,0x29,0x33,0xE0,0x3B,0x41,0x47,0x4C,0x3A,0x51);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0xE2,0x59,0x60,0x67,0x59,0x6E,0x74,0x7C,0x84,0x31,0x8D,0x92,0x99,0xA0,0xB0,0xA8,0xB2,0xBF,0xC8,0x6E,0xD4,0xE5,0xF3,0xFF,0x08);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xE0);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x34);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x85);
	txdsharp_dcs_write_seq_static(ctx,0xA7,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x80);
	txdsharp_dcs_write_seq_static(ctx,0xB3,0x22);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xB0);
	txdsharp_dcs_write_seq_static(ctx,0xB3,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x83);
	txdsharp_dcs_write_seq_static(ctx,0xB0,0x63);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA1);
	txdsharp_dcs_write_seq_static(ctx,0xB0,0x02);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA9);
	txdsharp_dcs_write_seq_static(ctx,0xB0,0xAA,0x0A);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xE0);
	txdsharp_dcs_write_seq_static(ctx,0xCF,0x34);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x92);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA0);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x40);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x98);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x24);
	txdsharp_dcs_write_seq_static(ctx,0x00,0xA1);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x40);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x9D);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x44);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x94);
	txdsharp_dcs_write_seq_static(ctx,0xC5,0x04);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x86);
	txdsharp_dcs_write_seq_static(ctx,0xA4,0xF1);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x82);
	txdsharp_dcs_write_seq_static(ctx,0xCE,0x11,0x11);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x90);
	txdsharp_dcs_write_seq_static(ctx,0xA7,0x1D);
	txdsharp_dcs_write_seq_static(ctx,0x1C,0x02);
	txdsharp_dcs_write_seq_static(ctx,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0xFF,0xFF,0xFF,0xFF);
	txdsharp_dcs_write_seq_static(ctx,0x51,0x00,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x53,0x2C);
	txdsharp_dcs_write_seq_static(ctx,0x55,0x00);
	txdsharp_dcs_write_seq_static(ctx,0x35,0x00);
	if(1 == rec_esd) {
		pr_info("recover_esd_set_backlight\n");
		esd_bl6[0] = ((last_brightness >> 4) & 0xff);
		esd_bl6[1] = (last_brightness & 0x0f);
		txdsharp_dcs_write_seq(ctx, 0x51, esd_bl6[0], esd_bl6[1]);
	}
    //----------------------LCD initial code End----------------------//
    //SLPOUT and DISPON
	txdsharp_dcs_write_seq_static(ctx, 0x11);
	msleep(100);
	/* Display On*/
	txdsharp_dcs_write_seq_static(ctx, 0x29);
    msleep(20);
	pr_info("%s-\n", __func__);
}

static int txdsharp_disable(struct drm_panel *panel)
{
	struct txdsharp *ctx = panel_to_txdsharp(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

extern bool fts_gestrue_status;
static int txdsharp_unprepare(struct drm_panel *panel)
{

	struct txdsharp *ctx = panel_to_txdsharp(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;
	panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	printk("[FTS] tpd_focaltech_notifier_callback in txdsharp-unprepare\n ");

	txdsharp_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(50);
	txdsharp_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(110);

	if (fts_gestrue_status == 1) {
		printk("[FTS-lcm]txdsharp_unprepare fts_gestrue_status = 1\n ");
	} else {
		printk("[FTS-lcm]txdsharp_unprepare fts_gestrue_status = 0\n ");
#if 0
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		usleep_range(2000, 2001);
#endif
		txdsharp_dcs_write_seq_static(ctx,0X00,0x00);
        txdsharp_dcs_write_seq_static(ctx,0Xff,0x87,0x22,0x01);
        txdsharp_dcs_write_seq_static(ctx,0X00,0x80);
        txdsharp_dcs_write_seq_static(ctx,0Xff,0x87,0x22);

        txdsharp_dcs_write_seq_static(ctx,0X00,0x00);
        txdsharp_dcs_write_seq_static(ctx,0Xf7,0x5a,0xa5,0x95,0x27);

        txdsharp_dcs_write_seq_static(ctx,0X00,0x00);
        txdsharp_dcs_write_seq_static(ctx,0Xff,0xff,0xff,0xff);
        msleep(1);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	usleep_range(2000, 2001);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	usleep_range(2000, 2001);

	//ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
	//gpiod_set_value(ctx->pm_gpio, 0);
	//devm_gpiod_put(ctx->dev, ctx->pm_gpio);
	}
	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int txdsharp_prepare(struct drm_panel *panel)
{
	struct txdsharp *ctx = panel_to_txdsharp(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// reset  L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	usleep_range(5000, 5001);

	//ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
	//gpiod_set_value(ctx->pm_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->pm_gpio);
	//usleep_range(5000, 5001);

	// end
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	msleep(2);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	_lcm_i2c_write_bytes(AVDD_REG, 0xf);
	_lcm_i2c_write_bytes(AVEE_REG, 0xf);
	msleep(2);
	txdsharp_panel_init(ctx);
	
	//add for TP resume
	panel_notifier_call_chain(PANEL_PREPARE, NULL);
	printk("[FTS] tpd_focaltech_notifier_callback in txdsharp_prepare\n ");
	
	ret = ctx->error;
	if (ret < 0)
		txdsharp_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	txdsharp_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int txdsharp_enable(struct drm_panel *panel)
{
	struct txdsharp *ctx = panel_to_txdsharp(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (408)
#define HSA (8)
#define HBP (38)
#define VFP_90HZ (30)
#define VFP_60HZ (1260)
#define VSA (4)
#define VBP (30)
#define VAC (2408)
#define HAC (1080)
static const struct drm_display_mode default_mode = {
	.clock = 340732,
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
	.clock = 341284,
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
	.pll_clk = 604,
	//.vfp_low_power = VFP_45HZ,
	.physical_width_um = 68430,
	.physical_height_um = 152570,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0E, .count = 1, .para_list[0] = 0x80,
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
	.pll_clk = 604,
	.vfp_low_power = VFP_60HZ,
	.physical_width_um = 68430,
	.physical_height_um = 152570,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.lcm_esd_check_table[1] = {
		.cmd = 0x0E, .count = 1, .para_list[0] = 0x80,
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

static int txdsharp_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = {0x51, 0xff, 0x0f, 0x00};

	pr_info("%s backlight = -%d\n", __func__, level);
	if (level > 255)
		level = 255;

	level = level * 4095 / 255;
	bl_tb0[1] = ((level >> 4) & 0xff);
	bl_tb0[2] = (level & 0x0f);
	pr_info("%s backlight = -%d,bl_tb0[1] = %x,bl_tb0[2] = %x\n", __func__, level,bl_tb0[1],bl_tb0[2]);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	last_brightness = level;
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
    pr_info("samir:%s+ mode:%d\n", __func__,m->vrefresh);
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
	struct txdsharp *ctx = panel_to_txdsharp(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = txdsharp_setbacklight_cmdq,
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

static int txdsharp_get_modes(struct drm_panel *panel)
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

	//panel->connector->display_info.width_mm = (68430/1000);
	//panel->connector->display_info.height_mm = (152570/1000);

	return 1;
}

static const struct drm_panel_funcs txdsharp_drm_funcs = {
	.disable = txdsharp_disable,
	.unprepare = txdsharp_unprepare,
	.prepare = txdsharp_prepare,
	.enable = txdsharp_enable,
	.get_modes = txdsharp_get_modes,
};

static int txdsharp_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct txdsharp *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

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
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct txdsharp), GFP_KERNEL);
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
	//ctx->pm_gpio = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
	//if (IS_ERR(ctx->pm_gpio)) {
	//	dev_info(dev, "cannot get pm_gpio %ld\n",
	//		 PTR_ERR(ctx->pm_gpio));
	//	return PTR_ERR(ctx->pm_gpio);
	//}
	//devm_gpiod_put(dev, ctx->pm_gpio);
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
	ctx->panel.funcs = &txdsharp_drm_funcs;

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

	pr_info("%s- wt,txd,sharp,ft8722,cphy,vdo,90hz\n", __func__);

	return ret;
}

static int txdsharp_remove(struct mipi_dsi_device *dsi)
{
	struct txdsharp *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static void txdsharp_panel_shutdown(struct mipi_dsi_device *dsi)
{
	pr_info("%s++\n", __func__);

	fts_gestrue_status = 0;
	pr_info("optimize shutdown sequence fts_gestrue_status is 0 %s++\n", __func__);
}

static const struct of_device_id txdsharp_of_match[] = {
	{
	    .compatible = "wt,ft8722_fhdp_wt_dsi_vdo_cphy_90hz_txd_sharp",
	},
	{}
};

MODULE_DEVICE_TABLE(of, txdsharp_of_match);

static struct mipi_dsi_driver txdsharp_driver = {
	.probe = txdsharp_probe,
	.remove = txdsharp_remove,
	.shutdown = txdsharp_panel_shutdown,
	.driver = {
		.name = "ft8722_fhdp_wt_dsi_vdo_cphy_90hz_txd_sharp",
		.owner = THIS_MODULE,
		.of_match_table = txdsharp_of_match,
	},
};

module_mipi_dsi_driver(txdsharp_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt txd sharp ft8722 vdo Panel Driver");
MODULE_LICENSE("GPL v2");
