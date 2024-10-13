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

struct chuangwei {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
    //struct gpio_desc *pm_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;

	int error;
};

#define chuangwei_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		chuangwei_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define chuangwei_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		chuangwei_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct chuangwei *panel_to_chuangwei(struct drm_panel *panel)
{
	return container_of(panel, struct chuangwei, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int chuangwei_dcs_read(struct chuangwei *ctx, u8 cmd, void *data, size_t len)
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

static void chuangwei_panel_get_data(struct chuangwei *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = chuangwei_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void chuangwei_dcs_write(struct chuangwei *ctx, const void *data, size_t len)
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
u8 esd_bl5[] = {0x00, 0x00};

extern void ili_resume_by_ddi(void);
static void chuangwei_panel_init(struct chuangwei *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%s+\n", __func__);

	ili_resume_by_ddi();
	printk("[ILITEK]resume in chuangwei_panel_init\n");

	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x00,0x40);
	chuangwei_dcs_write_seq_static(ctx,0x01,0x40);
	chuangwei_dcs_write_seq_static(ctx,0x02,0x0B);
	chuangwei_dcs_write_seq_static(ctx,0x03,0x31);
	chuangwei_dcs_write_seq_static(ctx,0x04,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x05,0x40);
	chuangwei_dcs_write_seq_static(ctx,0x06,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x07,0x31);
	chuangwei_dcs_write_seq_static(ctx,0x08,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x09,0x05);
	chuangwei_dcs_write_seq_static(ctx,0x0A,0x40);
	chuangwei_dcs_write_seq_static(ctx,0x0B,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x0C,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x0E,0x61);
	chuangwei_dcs_write_seq_static(ctx,0x31,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x32,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x33,0x3F);
	chuangwei_dcs_write_seq_static(ctx,0x34,0x29);
	chuangwei_dcs_write_seq_static(ctx,0x35,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x36,0x0C);
	chuangwei_dcs_write_seq_static(ctx,0x37,0x08);
	chuangwei_dcs_write_seq_static(ctx,0x38,0x14);
	chuangwei_dcs_write_seq_static(ctx,0x39,0x13);
	chuangwei_dcs_write_seq_static(ctx,0x3A,0x12);
	chuangwei_dcs_write_seq_static(ctx,0x3B,0x11);
	chuangwei_dcs_write_seq_static(ctx,0x3C,0x10);
	chuangwei_dcs_write_seq_static(ctx,0x3D,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x3E,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x3F,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x40,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x41,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x42,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x43,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x44,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x45,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x46,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x47,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x48,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x49,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x4A,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x4B,0x3F);
	chuangwei_dcs_write_seq_static(ctx,0x4C,0x29);
	chuangwei_dcs_write_seq_static(ctx,0x4D,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x4E,0x0C);
	chuangwei_dcs_write_seq_static(ctx,0x4F,0x08);
	chuangwei_dcs_write_seq_static(ctx,0x50,0x14);
	chuangwei_dcs_write_seq_static(ctx,0x51,0x13);
	chuangwei_dcs_write_seq_static(ctx,0x52,0x12);
	chuangwei_dcs_write_seq_static(ctx,0x53,0x11);
	chuangwei_dcs_write_seq_static(ctx,0x54,0x10);
	chuangwei_dcs_write_seq_static(ctx,0x55,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x56,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x57,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x58,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x59,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x5A,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x5B,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x5C,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x5D,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x5E,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x5F,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x60,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x61,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x62,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x63,0x3F);
	chuangwei_dcs_write_seq_static(ctx,0x64,0x29);
	chuangwei_dcs_write_seq_static(ctx,0x65,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x66,0x08);
	chuangwei_dcs_write_seq_static(ctx,0x67,0x0C);
	chuangwei_dcs_write_seq_static(ctx,0x68,0x13);
	chuangwei_dcs_write_seq_static(ctx,0x69,0x14);
	chuangwei_dcs_write_seq_static(ctx,0x6A,0x10);
	chuangwei_dcs_write_seq_static(ctx,0x6B,0x11);
	chuangwei_dcs_write_seq_static(ctx,0x6C,0x12);
	chuangwei_dcs_write_seq_static(ctx,0x6D,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x6E,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x6F,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x70,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x71,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x72,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x73,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x74,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x75,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x76,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x77,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x78,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x79,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x7A,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x7B,0x3F);
	chuangwei_dcs_write_seq_static(ctx,0x7C,0x29);
	chuangwei_dcs_write_seq_static(ctx,0x7D,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x7E,0x08);
	chuangwei_dcs_write_seq_static(ctx,0x7F,0x0C);
	chuangwei_dcs_write_seq_static(ctx,0x80,0x13);
	chuangwei_dcs_write_seq_static(ctx,0x81,0x14);
	chuangwei_dcs_write_seq_static(ctx,0x82,0x10);
	chuangwei_dcs_write_seq_static(ctx,0x83,0x11);
	chuangwei_dcs_write_seq_static(ctx,0x84,0x12);
	chuangwei_dcs_write_seq_static(ctx,0x85,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x86,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x87,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x88,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x89,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x8A,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x8B,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x8C,0x2E);
	chuangwei_dcs_write_seq_static(ctx,0x8D,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x8E,0x2F);
	chuangwei_dcs_write_seq_static(ctx,0x8F,0x30);
	chuangwei_dcs_write_seq_static(ctx,0x90,0x30);
	chuangwei_dcs_write_seq_static(ctx,0xB2,0x01);
	chuangwei_dcs_write_seq_static(ctx,0xB3,0x20);
	chuangwei_dcs_write_seq_static(ctx,0xD1,0x11);
	chuangwei_dcs_write_seq_static(ctx,0xD2,0x10);
	chuangwei_dcs_write_seq_static(ctx,0xD3,0x02);
	chuangwei_dcs_write_seq_static(ctx,0xD4,0x08);
	chuangwei_dcs_write_seq_static(ctx,0xD5,0x12);
	chuangwei_dcs_write_seq_static(ctx,0xD6,0x25);
	chuangwei_dcs_write_seq_static(ctx,0xD8,0x62);
	chuangwei_dcs_write_seq_static(ctx,0xD9,0x96);
	chuangwei_dcs_write_seq_static(ctx,0xDA,0x05);
	chuangwei_dcs_write_seq_static(ctx,0xDB,0x02);
	chuangwei_dcs_write_seq_static(ctx,0xDC,0x0A);
	chuangwei_dcs_write_seq_static(ctx,0xDD,0x98);
	chuangwei_dcs_write_seq_static(ctx,0xDF,0x40);
	chuangwei_dcs_write_seq_static(ctx,0xE0,0x40);
	chuangwei_dcs_write_seq_static(ctx,0xE1,0x68);
	chuangwei_dcs_write_seq_static(ctx,0xE3,0x10);
	chuangwei_dcs_write_seq_static(ctx,0xE4,0x22);
	chuangwei_dcs_write_seq_static(ctx,0xE5,0x0F);
	chuangwei_dcs_write_seq_static(ctx,0xE6,0x22);
	chuangwei_dcs_write_seq_static(ctx,0xED,0x6D);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x1B,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x36,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x46,0x11);
	chuangwei_dcs_write_seq_static(ctx,0x47,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x4F,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x06,0x69);
	chuangwei_dcs_write_seq_static(ctx,0x0E,0x41);
	chuangwei_dcs_write_seq_static(ctx,0x0F,0x32);
	chuangwei_dcs_write_seq_static(ctx,0x24,0x16);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x03);
	chuangwei_dcs_write_seq_static(ctx,0x00,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x01,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x02,0xEA);
	chuangwei_dcs_write_seq_static(ctx,0x08,0x21);
	chuangwei_dcs_write_seq_static(ctx,0x09,0x21);
	chuangwei_dcs_write_seq_static(ctx,0x0A,0x22);
	chuangwei_dcs_write_seq_static(ctx,0x0B,0x22);
	chuangwei_dcs_write_seq_static(ctx,0x0C,0x22);
	chuangwei_dcs_write_seq_static(ctx,0x0D,0x02);
	chuangwei_dcs_write_seq_static(ctx,0x0E,0x65);
	chuangwei_dcs_write_seq_static(ctx,0x14,0x21);
	chuangwei_dcs_write_seq_static(ctx,0x15,0x21);
	chuangwei_dcs_write_seq_static(ctx,0x16,0x22);
	chuangwei_dcs_write_seq_static(ctx,0x17,0x22);
	chuangwei_dcs_write_seq_static(ctx,0x18,0x22);
	chuangwei_dcs_write_seq_static(ctx,0x19,0xF8);
	chuangwei_dcs_write_seq_static(ctx,0x30,0x01);
	chuangwei_dcs_write_seq_static(ctx,0x31,0x05);
	chuangwei_dcs_write_seq_static(ctx,0x32,0xC7);
	chuangwei_dcs_write_seq_static(ctx,0x83,0x20);
	chuangwei_dcs_write_seq_static(ctx,0x84,0x00);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x04);
	chuangwei_dcs_write_seq_static(ctx,0xBD,0x01);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x05);
	chuangwei_dcs_write_seq_static(ctx,0x1D,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x1E,0x91);
	chuangwei_dcs_write_seq_static(ctx,0x72,0x6C);
	chuangwei_dcs_write_seq_static(ctx,0x74,0x45);
	chuangwei_dcs_write_seq_static(ctx,0x76,0x69);
	chuangwei_dcs_write_seq_static(ctx,0x7A,0x3F);
	chuangwei_dcs_write_seq_static(ctx,0x7B,0x88);
	chuangwei_dcs_write_seq_static(ctx,0x7C,0x88);
	chuangwei_dcs_write_seq_static(ctx,0x46,0x4F);
	chuangwei_dcs_write_seq_static(ctx,0x47,0x4F);
	chuangwei_dcs_write_seq_static(ctx,0xB5,0x44);
	chuangwei_dcs_write_seq_static(ctx,0xB7,0x44);
	chuangwei_dcs_write_seq_static(ctx,0xC6,0x1B);
	chuangwei_dcs_write_seq_static(ctx,0x3E,0x50);
	chuangwei_dcs_write_seq_static(ctx,0x56,0xFF);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x06);
	chuangwei_dcs_write_seq_static(ctx,0xC0,0x68);
	chuangwei_dcs_write_seq_static(ctx,0xC1,0x19);
	chuangwei_dcs_write_seq_static(ctx,0xC3,0x06);
	chuangwei_dcs_write_seq_static(ctx,0x13,0x13);
	chuangwei_dcs_write_seq_static(ctx,0xA3,0x00);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x07);
	chuangwei_dcs_write_seq_static(ctx,0x11,0x16);
	chuangwei_dcs_write_seq_static(ctx,0x29,0x00);
	chuangwei_dcs_write_seq_static(ctx, 0x82, 0x20);
	pr_info("%s chuangwei: write 0x82 value to 0x20 in kernel init\n", __func__);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x08);
	chuangwei_dcs_write_seq_static(ctx,0xE0,0x00,0x00,0x24,0x58,0x00,0x9E,0xCE,0xF3,0x15,0x2C,0x58,0x96,0x29,0xC6,0x0C,0x3F,0x2A,0x72,0xAA,0xCE,0x3E,0xFE,0x1F,0x4C,0x3F,0x63,0x83,0xAB,0x0F,0xBF,0xEA);
	chuangwei_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x24,0x58,0x00,0x9E,0xCE,0xF3,0x15,0x2C,0x58,0x96,0x29,0xC6,0x0C,0x3F,0x2A,0x72,0xAA,0xCE,0x3E,0xFE,0x1F,0x4C,0x3F,0x63,0x83,0xAB,0x0F,0xBF,0x9D);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x11);
	chuangwei_dcs_write_seq_static(ctx,0x38,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x39,0x31);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x12);
	chuangwei_dcs_write_seq_static(ctx,0x48,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x49,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x4A,0x04);
	chuangwei_dcs_write_seq_static(ctx,0x4B,0x1C);
	chuangwei_dcs_write_seq_static(ctx,0x4E,0x06);
	chuangwei_dcs_write_seq_static(ctx,0x52,0x1F);
	chuangwei_dcs_write_seq_static(ctx,0x53,0x25);
	chuangwei_dcs_write_seq_static(ctx,0xC8,0x47);
	chuangwei_dcs_write_seq_static(ctx,0xC9,0x00);
	chuangwei_dcs_write_seq_static(ctx,0xCA,0x26);
	chuangwei_dcs_write_seq_static(ctx,0xCB,0x28);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x0B);
	chuangwei_dcs_write_seq_static(ctx,0x94,0x88);
	chuangwei_dcs_write_seq_static(ctx,0x95,0x22);
	chuangwei_dcs_write_seq_static(ctx,0x96,0x06);
	chuangwei_dcs_write_seq_static(ctx,0x97,0x06);
	chuangwei_dcs_write_seq_static(ctx,0x98,0xCB);
	chuangwei_dcs_write_seq_static(ctx,0x99,0xCB);
	chuangwei_dcs_write_seq_static(ctx,0x9A,0x06);
	chuangwei_dcs_write_seq_static(ctx,0x9B,0xAD);
	chuangwei_dcs_write_seq_static(ctx,0x9C,0x05);
	chuangwei_dcs_write_seq_static(ctx,0x9D,0x05);
	chuangwei_dcs_write_seq_static(ctx,0x9E,0xA7);
	chuangwei_dcs_write_seq_static(ctx,0x9F,0xA7);
	chuangwei_dcs_write_seq_static(ctx,0xC6,0x85);
	chuangwei_dcs_write_seq_static(ctx,0xC7,0x6B);
	chuangwei_dcs_write_seq_static(ctx,0xC8,0x04);
	chuangwei_dcs_write_seq_static(ctx,0xC9,0x04);
	chuangwei_dcs_write_seq_static(ctx,0xCA,0x87);
	chuangwei_dcs_write_seq_static(ctx,0xCB,0x87);
	chuangwei_dcs_write_seq_static(ctx,0xD8,0x44);
	chuangwei_dcs_write_seq_static(ctx,0xD9,0x77);
	chuangwei_dcs_write_seq_static(ctx,0xDA,0x03);
	chuangwei_dcs_write_seq_static(ctx,0xDB,0x03);
	chuangwei_dcs_write_seq_static(ctx,0xDC,0x6F);
	chuangwei_dcs_write_seq_static(ctx,0xDD,0x6F);
	chuangwei_dcs_write_seq_static(ctx,0xAA,0x12);
	chuangwei_dcs_write_seq_static(ctx,0xAB,0xE0);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x0C);
	chuangwei_dcs_write_seq_static(ctx,0x40,0x1C);
	chuangwei_dcs_write_seq_static(ctx,0x41,0x5C);
	chuangwei_dcs_write_seq_static(ctx,0x42,0x1C);
	chuangwei_dcs_write_seq_static(ctx,0x43,0x5B);
	chuangwei_dcs_write_seq_static(ctx,0x44,0x1C);
	chuangwei_dcs_write_seq_static(ctx,0x45,0x5D);
	chuangwei_dcs_write_seq_static(ctx,0x46,0x1C);
	chuangwei_dcs_write_seq_static(ctx,0x47,0x5A);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x0E);
	chuangwei_dcs_write_seq_static(ctx,0x00,0xA3);
	chuangwei_dcs_write_seq_static(ctx,0x02,0x0F);
	chuangwei_dcs_write_seq_static(ctx,0x04,0x06);
	chuangwei_dcs_write_seq_static(ctx,0x13,0x08);
	chuangwei_dcs_write_seq_static(ctx,0xB0,0x01);
	chuangwei_dcs_write_seq_static(ctx,0xC0,0x12);
	chuangwei_dcs_write_seq_static(ctx,0x05,0x20);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x1E);
	chuangwei_dcs_write_seq_static(ctx,0x20,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x21,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x22,0x04);
	chuangwei_dcs_write_seq_static(ctx,0x23,0x1C);
	chuangwei_dcs_write_seq_static(ctx,0x24,0x08);
	chuangwei_dcs_write_seq_static(ctx,0xBD,0x01);
	chuangwei_dcs_write_seq_static(ctx,0xB1,0x1F);
	chuangwei_dcs_write_seq_static(ctx,0x61,0x1E);
	chuangwei_dcs_write_seq_static(ctx,0x62,0x03);
	chuangwei_dcs_write_seq_static(ctx,0x63,0x1E);
	chuangwei_dcs_write_seq_static(ctx,0x64,0x83);
	chuangwei_dcs_write_seq_static(ctx,0x60,0x07);
	chuangwei_dcs_write_seq_static(ctx,0x65,0x0A);
	chuangwei_dcs_write_seq_static(ctx,0x66,0xF9);
	chuangwei_dcs_write_seq_static(ctx,0x67,0x10);
	chuangwei_dcs_write_seq_static(ctx,0x69,0x2D);
	chuangwei_dcs_write_seq_static(ctx,0x16,0x3E);
	chuangwei_dcs_write_seq_static(ctx,0x18,0x3E);
	chuangwei_dcs_write_seq_static(ctx,0x19,0x3E);
	chuangwei_dcs_write_seq_static(ctx,0x1A,0x3E);
	chuangwei_dcs_write_seq_static(ctx,0x1B,0x3E);
	chuangwei_dcs_write_seq_static(ctx,0x1C,0x3E);
	chuangwei_dcs_write_seq_static(ctx,0x1D,0x3E);
	chuangwei_dcs_write_seq_static(ctx,0x1E,0x3E);
	chuangwei_dcs_write_seq_static(ctx,0x1F,0x3E);
	chuangwei_dcs_write_seq_static(ctx,0x6D,0x7A);
	chuangwei_dcs_write_seq_static(ctx,0x70,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x6B,0x03);
	chuangwei_dcs_write_seq_static(ctx,0xB4,0x00);
	chuangwei_dcs_write_seq_static(ctx,0xB5,0x39);
	chuangwei_dcs_write_seq_static(ctx,0xB6,0x39);
	chuangwei_dcs_write_seq_static(ctx,0xB7,0x27);
	chuangwei_dcs_write_seq_static(ctx,0xBA,0x00);
	chuangwei_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x53,0x2C);
	chuangwei_dcs_write_seq_static(ctx,0x35,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x68,0x04,0x00);
	chuangwei_dcs_write_seq_static(ctx,0x11,0x00);
	msleep(70);
	chuangwei_dcs_write_seq_static(ctx,0x29,0x00);
	msleep(20);

	if(1 == rec_esd) {
		pr_info("recover_esd_set_backlight\n");
		msleep(200);
		esd_bl5[0] = ((last_brightness >> 8) & 0xf);
		esd_bl5[1] = (last_brightness & 0xff);
		chuangwei_dcs_write_seq(ctx, 0x51, esd_bl5[0], esd_bl5[1]);
	}

	pr_info("%s-\n", __func__);
}

static int chuangwei_disable(struct drm_panel *panel)
{
	struct chuangwei *ctx = panel_to_chuangwei(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}
extern int ili_sleep_handler(int mode);
extern bool  gestrue_status;
extern bool  gestrue_spay;
static int chuangwei_unprepare(struct drm_panel *panel)
{

	struct chuangwei *ctx = panel_to_chuangwei(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;

	//add for TP suspend
	//panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	ili_sleep_handler(1);
	printk("[ILITEK]ilitek_suspend in chuangwei-unprepare\n ");
	usleep_range(6000, 6001);
	chuangwei_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(20);
	chuangwei_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);

	msleep(200);
	if((gestrue_status == 1)||(gestrue_spay == 1)){
		printk("[ILITEK]chuangwei_unprepare gesture_status = 1\n ");
	} else {
		printk("[ILITEK]chuangwei_unprepare gesture_flag = 0\n ");
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
#if 0
		ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->pm_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->pm_gpio);
#endif
	}
	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int chuangwei_prepare(struct drm_panel *panel)
{
	struct chuangwei *ctx = panel_to_chuangwei(panel);
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

	usleep_range(3000, 3001);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	_lcm_i2c_write_bytes(AVDD_REG, 0xf);
	_lcm_i2c_write_bytes(AVEE_REG, 0xf);

	chuangwei_panel_init(ctx);

	//ili_resume_by_ddi();

	//add for TP resume
	//panel_notifier_call_chain(PANEL_PREPARE, NULL);
	//printk("[ILITEK]tpd_ilitek_notifier_callback in chuangwei_prepare\n ");

	ret = ctx->error;
	if (ret < 0)
		chuangwei_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	chuangwei_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int chuangwei_enable(struct drm_panel *panel)
{
	struct chuangwei *ctx = panel_to_chuangwei(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (17)
#define HSA (4)
#define HBP (31)
#define VFP_90HZ (40)
#define VFP_60HZ (1335)
#define VSA (2)
#define VBP (36)
#define VAC (2408)
#define HAC (1080)
static const struct drm_display_mode default_mode = {
	.clock = 256805,
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
	.clock = 253273,
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
	.physical_width_um = 68430,
	.physical_height_um = 152570,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.dyn = {
		.switch_en = 1,
		.pll_clk = 533,
		.hbp = 38,
		.vfp = VFP_60HZ,
	},
    .dyn_fps = {
        .switch_en = 1,
        .vact_timing_fps = 90,
    },
	.phy_timcon = {
		.hs_zero = 34,
		.hs_trail =26,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 530,
	.vfp_low_power = VFP_60HZ,
	.physical_width_um = 68430,
	.physical_height_um = 152570,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.dyn = {
		.switch_en = 1,
		.pll_clk = 533,
		.hbp = 38,
		.vfp = VFP_90HZ,
	},
    .dyn_fps = {
        .switch_en = 1,
        .vact_timing_fps = 90,
    },
	.phy_timcon = {
		.hs_zero = 34,
		.hs_trail =26,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int chuangwei_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = {0x51, 0x0f, 0xff,0x00};
	//+S96901AA4-194 liuxueyou.wt, 20231016, add, screen backlight flashing during sleep
	char bl_tb1[] = {0x53, 0x24,0x00,0x00};
	char bl_tb2[] = {0x53, 0x2c,0x00,0x00};
	pr_info("%s backlight = -%d\n", __func__, level);
	if (level > 255)
		level = 255;

	level = level * 4095 / 255;
	bl_tb0[1] = ((level >> 8) & 0xf);
	bl_tb0[2] = (level & 0xff);
	pr_info("%s chuangwei backlight = -%d,bl_tb0[1] = %x,bl_tb0[2] = %x\n", __func__, level,bl_tb0[1],bl_tb0[2]);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	if(level == 0){
		cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	}else if(last_brightness == 0 && level > 0){
		cb(dsi, handle, bl_tb2, ARRAY_SIZE(bl_tb2));
	}
	//-S96901AA4-194 liuxueyou.wt, 20231016, add, screen backlight flashing during sleep
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
	struct chuangwei *ctx = panel_to_chuangwei(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = chuangwei_setbacklight_cmdq,
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

static int chuangwei_get_modes(struct drm_panel *panel)
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

static const struct drm_panel_funcs chuangwei_drm_funcs = {
	.disable = chuangwei_disable,
	.unprepare = chuangwei_unprepare,
	.prepare = chuangwei_prepare,
	.enable = chuangwei_enable,
	.get_modes = chuangwei_get_modes,
};

static int chuangwei_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct chuangwei *ctx;
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
	ctx = devm_kzalloc(dev, sizeof(struct chuangwei), GFP_KERNEL);
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
	ctx->panel.funcs = &chuangwei_drm_funcs;

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

	pr_info("%s- wt,chuangwei,ili7807s,cphy,vdo,90hz\n", __func__);

	return ret;
}

static int chuangwei_remove(struct mipi_dsi_device *dsi)
{
	struct chuangwei *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static void chuangwei_panel_shutdown(struct mipi_dsi_device *dsi)
{
	pr_info("ili7807s chuangwei%s++\n", __func__);

	gestrue_status = 0;
	gestrue_spay = 0;
	pr_info("optimize shutdown sequence gestrue_status gestrue_spay is 0 %s++\n", __func__);
}

static const struct of_device_id chuangwei_of_match[] = {
	{
	    .compatible = "wt,ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_chuangwei",
	},
	{}
};

MODULE_DEVICE_TABLE(of, chuangwei_of_match);

static struct mipi_dsi_driver chuangwei_driver = {
	.probe = chuangwei_probe,
	.remove = chuangwei_remove,
	.shutdown = chuangwei_panel_shutdown,
	.driver = {
		.name = "ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_chuangwei",
		.owner = THIS_MODULE,
		.of_match_table = chuangwei_of_match,
	},
};

module_mipi_dsi_driver(chuangwei_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt chuangwei ili7807s vdo Panel Driver");
MODULE_LICENSE("GPL v2");
