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

struct truly {
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

#define truly_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		truly_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define truly_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		truly_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct truly *panel_to_truly(struct drm_panel *panel)
{
	return container_of(panel, struct truly, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int truly_dcs_read(struct truly *ctx, u8 cmd, void *data, size_t len)
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

static void truly_panel_get_data(struct truly *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = truly_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void truly_dcs_write(struct truly *ctx, const void *data, size_t len)
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

static void truly_panel_init(struct truly *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%s+\n", __func__);

    //CMD2 ENABLE
    truly_dcs_write_seq_static(ctx,0X00,0x00);
    truly_dcs_write_seq_static(ctx,0XFF,0x87,0x22,0x01);
    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XFF,0x87,0x22);

    //Panel size
    truly_dcs_write_seq_static(ctx,0X00,0xa3);
    truly_dcs_write_seq_static(ctx,0XB3,0x09,0x68); //2408

    //==============================================
    //TCON
    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x5f,0x00,0x2E,0x00,0x14);

    truly_dcs_write_seq_static(ctx,0X00,0x90);
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x61,0x00,0x2E,0x00,0x14);

    truly_dcs_write_seq_static(ctx,0X00,0xA0);
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x92,0x00,0x2E,0x00,0x14);

    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x61,0x02,0xCE,0x14);

    truly_dcs_write_seq_static(ctx,0X00,0xC1);
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x8e,0x00,0x7a,0x00,0x5f,0x00,0xb8);

    truly_dcs_write_seq_static(ctx,0X00,0x70);
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x61,0x00,0x2E,0x00,0x14);

    truly_dcs_write_seq_static(ctx,0X00,0xA3);
    truly_dcs_write_seq_static(ctx,0XC1,0x00,0x22,0x00,0x3d,0x00,0x02);

    truly_dcs_write_seq_static(ctx,0X00,0xB7);
    truly_dcs_write_seq_static(ctx,0XC1,0x00,0x22);

    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XCE,0x01,0x81,0xFF,0xFF,0x00,0x85,0x00,0x85,0x00,0xC8,0x00,0xC8,0x00,0x50,0x00,0x88);

    truly_dcs_write_seq_static(ctx,0X00,0x90);
    truly_dcs_write_seq_static(ctx,0XCE,0x00,0xA7,0x10,0x43,0x00,0xA7,0x80,0xFF,0xFF,0x00,0x06,0x00,0x0a,0x1a,0x0f);

    truly_dcs_write_seq_static(ctx,0X00,0xA0);
    truly_dcs_write_seq_static(ctx,0XCE,0x00,0x00,0x00);

    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XCE,0x22,0x00,0x00);

    truly_dcs_write_seq_static(ctx,0X00,0xD1);
    truly_dcs_write_seq_static(ctx,0XCE,0x00,0x00,0x01,0x00,0x00,0x00,0x00);

    truly_dcs_write_seq_static(ctx,0X00,0xE1);
    truly_dcs_write_seq_static(ctx,0XCE,0x0A,0x02,0xB6,0x02,0xB6,0x02,0xB6,0x00,0x00,0x00,0x00);

    truly_dcs_write_seq_static(ctx,0X00,0xF1);
    truly_dcs_write_seq_static(ctx,0XCE,0x15,0x15,0x1F,0x01,0x21,0x00,0xED,0x01,0x2C);

    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XCF,0x00,0x00,0x4b,0x4e);

    truly_dcs_write_seq_static(ctx,0X00,0xB5);
    truly_dcs_write_seq_static(ctx,0XCF,0x05,0x05,0x13,0x16);

    truly_dcs_write_seq_static(ctx,0X00,0xC0);
    truly_dcs_write_seq_static(ctx,0XCF,0x09,0x09,0x63,0x67);

    truly_dcs_write_seq_static(ctx,0X00,0xC5);
    truly_dcs_write_seq_static(ctx,0XCF,0x09,0x09,0x69,0x6D);

    truly_dcs_write_seq_static(ctx,0X00,0x60);
    truly_dcs_write_seq_static(ctx,0XCF,0x00,0x00,0x4b,0x4e,0x05,0x05,0x13,0x16);

    truly_dcs_write_seq_static(ctx,0X00,0x70);
    truly_dcs_write_seq_static(ctx,0XCF,0x00,0x00,0xc1,0xC6,0x05,0x05,0x71,0x76);

    //Qsync Detect
    truly_dcs_write_seq_static(ctx,0X00,0xD1);
    truly_dcs_write_seq_static(ctx,0XC1,0x08,0x0a,0x0B,0x49,0x13,0x68,0x05,0x6d,0x07,0x76,0x0C,0xd9);

    truly_dcs_write_seq_static(ctx,0X00,0xE1);
    truly_dcs_write_seq_static(ctx,0XC1,0x0B,0x49);

    truly_dcs_write_seq_static(ctx,0X00,0xE4);
    truly_dcs_write_seq_static(ctx,0XCF,0x09,0xFA,0x09,0xF9,0x09,0xF9,0x09,0xF9,0x09,0xF9,0x09,0xF9);

    //OSC
    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XC1,0x00,0x00);

    truly_dcs_write_seq_static(ctx,0X00,0x90);
    truly_dcs_write_seq_static(ctx,0XC1,0x03);

    //Line rate for TP
    truly_dcs_write_seq_static(ctx,0X00,0xF5);
    truly_dcs_write_seq_static(ctx,0XCF,0x01);

    //tp frame rate
    truly_dcs_write_seq_static(ctx,0X00,0xF6);
    truly_dcs_write_seq_static(ctx,0XCF,0x5a);

    //tcon frame rate
    truly_dcs_write_seq_static(ctx,0X00,0xF1);
    truly_dcs_write_seq_static(ctx,0XCF,0x5a);

    //mode switch by CMD2
    truly_dcs_write_seq_static(ctx,0X00,0xF7);
    truly_dcs_write_seq_static(ctx,0XCF,0x11);

    truly_dcs_write_seq_static(ctx,0X00,0x8f);
    truly_dcs_write_seq_static(ctx,0XC5,0x20);

    //========================================
    //LTPS Initial Code
    //STV1 & STV2 Setting
    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XC2,0x82,0x01,0x31,0x31,0x00,0x00,0x00,0x00);

    //VEND Setting
    truly_dcs_write_seq_static(ctx,0X00,0x90);
    truly_dcs_write_seq_static(ctx,0XC2,0x00,0x00,0x00,0x00);

    //CKV1-3 setting
    truly_dcs_write_seq_static(ctx,0X00,0xA0);
    truly_dcs_write_seq_static(ctx,0XC2,0x00,0x80,0x00,0x21,0x8B,0x01,0x00,0x00,0x21,0x8B,0x02,0x01,0x00,0x21,0x8B);

    //CKV4-5 setting
    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XC2,0x03,0x02,0x00,0x21,0x8B,0x80,0x08,0x03,0x02,0x02);

    //CKV9 setting
    truly_dcs_write_seq_static(ctx,0X00,0xCA);
    truly_dcs_write_seq_static(ctx,0XC2,0x80,0x08,0x03,0x02,0x02);

    //CKV width setting
    truly_dcs_write_seq_static(ctx,0X00,0xE0);
    truly_dcs_write_seq_static(ctx,0XC2,0x33,0x33,0x70,0x00,0x70);

    //Rst1 Setting
    truly_dcs_write_seq_static(ctx,0X00,0xE8);
    truly_dcs_write_seq_static(ctx,0XC2,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

    //power off enmode setting
    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XCB,0x00,0x01,0x00,0x03,0xFD,0x01,0x02,0x00,0x00,0x00,0xFD,0x01,0x00,0x03,0x00,0x00);

    //power on enmode setting
    truly_dcs_write_seq_static(ctx,0X00,0x90);
    truly_dcs_write_seq_static(ctx,0XCB,0x00,0x00,0x00,0x0C,0xF0,0x00,0x00,0x00,0x00,0x00,0xfc,0x00,0x00,0x00,0x00,0x00);

    //skip & powr on1 enmode setting
    truly_dcs_write_seq_static(ctx,0X00,0xA0);
    truly_dcs_write_seq_static(ctx,0XCB,0x00,0x00,0x00,0x00,0x03,0x00,0x0C,0x00);

    //power off blank enmode setting
    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XCB,0x13,0x58,0x05,0x30);

    //power on blank enmode setting
    truly_dcs_write_seq_static(ctx,0X00,0xC0);
    truly_dcs_write_seq_static(ctx,0XCB,0x13,0x58,0x05,0x30);

    //power on blank enmode setting
    truly_dcs_write_seq_static(ctx,0X00,0xD5);
    truly_dcs_write_seq_static(ctx,0XCB,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00);

    truly_dcs_write_seq_static(ctx,0X00,0xE0);
    truly_dcs_write_seq_static(ctx,0XCB,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01);

    //panel mapping setting
    //u2d_L
    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XCC,0x2C,0x12,0x2C,0x22,0x2C,0x0A,0x2C,0x2C,0x09,0x08,0x07,0x06,0x2C,0x2C,0x2C,0x2C);

    truly_dcs_write_seq_static(ctx,0X00,0x90);
    truly_dcs_write_seq_static(ctx,0XCC,0x2C,0x18,0x16,0x17,0x2C,0x1C,0x1D,0x1E);

    //d2u_L
    truly_dcs_write_seq_static(ctx,0X00,0xA0);
    truly_dcs_write_seq_static(ctx,0XCC,0x2C,0x12,0x2C,0x23,0x2C,0x0E,0x2C,0x2C,0x06,0x07,0x08,0x09,0x2C,0x2C,0x2C,0x2C);

    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XCC,0x2C,0x18,0x16,0x17,0x2C,0x1C,0x1D,0x1E);

    //u2d_R
    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XCD,0x2C,0x2C,0x2C,0x02,0x2C,0x0A,0x2C,0x2C,0x09,0x08,0x07,0x06,0x2C,0x2C,0x2C,0x2C);

    truly_dcs_write_seq_static(ctx,0X00,0x90);
    truly_dcs_write_seq_static(ctx,0XCD,0x2C,0x18,0x16,0x17,0x2C,0x1C,0x1D,0x1E);

    //d2u_R
    truly_dcs_write_seq_static(ctx,0X00,0xA0);
    truly_dcs_write_seq_static(ctx,0XCD,0x2C,0x2C,0x2C,0x02,0x2C,0x0E,0x2C,0x2C,0x06,0x07,0x08,0x09,0x2C,0x2C,0x2C,0x2C);

    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XCD,0x2C,0x18,0x16,0x17,0x2C,0x1C,0x1D,0x1E);
    //==============================================

    //ckh
    truly_dcs_write_seq_static(ctx,0X00,0x86);//Normal
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x00,0x00,0x01,0x18,0x18,0x18,0x04);

    truly_dcs_write_seq_static(ctx,0X00,0x96);//IDLE
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x00,0x00,0x01,0x18,0x18,0x18,0x04);

    truly_dcs_write_seq_static(ctx,0X00,0xA6);//LPF
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x00,0x00,0x01,0x28,0x28,0x28,0x04);

    truly_dcs_write_seq_static(ctx,0X00,0xA3);//PER_DMY
    truly_dcs_write_seq_static(ctx,0XCE,0x00,0x00,0x00,0x01,0x18,0x04);

    truly_dcs_write_seq_static(ctx,0X00,0xB3);//POS_DMY
    truly_dcs_write_seq_static(ctx,0XCE,0x00,0x00,0x00,0x01,0x18,0x04);

    truly_dcs_write_seq_static(ctx,0X00,0x76);//FIFO
    truly_dcs_write_seq_static(ctx,0XC0,0x00,0x00,0x00,0x01,0x18,0x18,0x18,0x04);

    //CKH_dummy
    truly_dcs_write_seq_static(ctx,0X00,0x82);
    truly_dcs_write_seq_static(ctx,0XA7,0x10,0x00);

    truly_dcs_write_seq_static(ctx,0X00,0x8d);
    truly_dcs_write_seq_static(ctx,0XA7,0x01);

    truly_dcs_write_seq_static(ctx,0X00,0x8f);
    truly_dcs_write_seq_static(ctx,0XA7,0x01);

    //CKH Rotate
    truly_dcs_write_seq_static(ctx,0X00,0xA0);
    truly_dcs_write_seq_static(ctx,0XC3,0x35,0x21);
    truly_dcs_write_seq_static(ctx,0X00,0xA4);
    truly_dcs_write_seq_static(ctx,0XC3,0x01,0x20);
    truly_dcs_write_seq_static(ctx,0X00,0xAA);
    truly_dcs_write_seq_static(ctx,0XC3,0x21);
    truly_dcs_write_seq_static(ctx,0X00,0xAd);
    truly_dcs_write_seq_static(ctx,0XC3,0x01);
    truly_dcs_write_seq_static(ctx,0X00,0xAe);
    truly_dcs_write_seq_static(ctx,0XC3,0x20);
    truly_dcs_write_seq_static(ctx,0X00,0xb3);
    truly_dcs_write_seq_static(ctx,0XC3,0x21);
    truly_dcs_write_seq_static(ctx,0X00,0xb6);
    truly_dcs_write_seq_static(ctx,0XC3,0x01,0x20);

    //CKH/XCKH =>AVDD/AVEE
    truly_dcs_write_seq_static(ctx,0X00,0xC3);
    truly_dcs_write_seq_static(ctx,0XC5,0xFF);
    truly_dcs_write_seq_static(ctx,0X00,0xA9);
    truly_dcs_write_seq_static(ctx,0XF5,0x8E);

    //analog setting
    //vgh
    truly_dcs_write_seq_static(ctx,0X00,0x93);
    truly_dcs_write_seq_static(ctx,0XC5,0x25);

    truly_dcs_write_seq_static(ctx,0X00,0x97);
    truly_dcs_write_seq_static(ctx,0XC5,0x25);

    //vgl
    truly_dcs_write_seq_static(ctx,0X00,0x9A);
    truly_dcs_write_seq_static(ctx,0XC5,0x21);

    truly_dcs_write_seq_static(ctx,0X00,0x9C);
    truly_dcs_write_seq_static(ctx,0XC5,0x21);

    //vgho1, vglo1, vgho2, vglo2 setting
    truly_dcs_write_seq_static(ctx,0X00,0xB6);
    truly_dcs_write_seq_static(ctx,0XC5,0x10,0x10,0x0E,0x0E,0x10,0x10,0x0E,0x0E);

    // detail of CIC //
    truly_dcs_write_seq_static(ctx,0X00,0x90);
    truly_dcs_write_seq_static(ctx,0XC3,0x08);

    truly_dcs_write_seq_static(ctx,0X00,0xfa);
    truly_dcs_write_seq_static(ctx,0XC2,0x23);

    truly_dcs_write_seq_static(ctx,0X00,0xca);
    truly_dcs_write_seq_static(ctx,0XC0,0x80);

    // five command //
    truly_dcs_write_seq_static(ctx,0X00,0x82);
    truly_dcs_write_seq_static(ctx,0XF5,0x01);

    truly_dcs_write_seq_static(ctx,0X00,0x93);
    truly_dcs_write_seq_static(ctx,0XF5,0x01);

    truly_dcs_write_seq_static(ctx,0X00,0x9b);
    truly_dcs_write_seq_static(ctx,0XF5,0x49);

    truly_dcs_write_seq_static(ctx,0X00,0x9d);
    truly_dcs_write_seq_static(ctx,0XF5,0x49);

    //Modify C5BE=0xF0,0xF0 for slpin VGHO1/VGLO1 abnormal
    truly_dcs_write_seq_static(ctx,0X00,0xbe);
    truly_dcs_write_seq_static(ctx,0XC5,0xF0,0xF0);

    truly_dcs_write_seq_static(ctx,0X00,0x85);
    truly_dcs_write_seq_static(ctx,0XA7,0x01);

    truly_dcs_write_seq_static(ctx,0X00,0xdc);
    truly_dcs_write_seq_static(ctx,0XC3,0x37);

    truly_dcs_write_seq_static(ctx,0X00,0x8A);
    truly_dcs_write_seq_static(ctx,0XF5,0xC7);

    truly_dcs_write_seq_static(ctx,0X00,0xB1);
    truly_dcs_write_seq_static(ctx,0XF5,0x1F);
    //source pull low sel
    truly_dcs_write_seq_static(ctx,0X00,0xB7);
    truly_dcs_write_seq_static(ctx,0XF5,0x1F);

    //AC MODE GIP toggle
    truly_dcs_write_seq_static(ctx,0X00,0x99);
    truly_dcs_write_seq_static(ctx,0XCF,0x50);

    truly_dcs_write_seq_static(ctx,0X00,0x9C);
    truly_dcs_write_seq_static(ctx,0XF5,0x00);

    truly_dcs_write_seq_static(ctx,0X00,0x9E);
    truly_dcs_write_seq_static(ctx,0XF5,0x00);

    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XC5,0xC0,0x4A,0x3D,0xC0,0x4A,0x0F);

    truly_dcs_write_seq_static(ctx,0X00,0xC2);
    truly_dcs_write_seq_static(ctx,0XF5,0x42);

    //VDD=LVDSVDD=1.25V
    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XC5,0x77);

    //mirror_x2=1
    truly_dcs_write_seq_static(ctx,0X00,0xE8);
    truly_dcs_write_seq_static(ctx,0XC0,0x40);

    //Gamma setting
    //truly_dcs_write_seq_static(ctx,0X00,0x00);
    //truly_dcs_write_seq_static(ctx,0XE1,0x00,0x02,0x07,0x0F,0x36,0x1A,0x22,0x29,0x34,0xBA,0x3D,0x44,0x4A,0x4F,0x1B,0x54,0x5D,0x65,0x6C,0xD6,0x73,0x7B,0x83,0x8C,0xD4,0x96,0x9C,0xA3,0xAA,0x93,0xB3,0xBE,0xCD,0xD5,0xF3,0xE0,0xEF,0xF9,0xFF,0x84);
    //truly_dcs_write_seq_static(ctx,0X00,0x30);
    //truly_dcs_write_seq_static(ctx,0XE1,0x00,0x02,0x07,0x0F,0x36,0x1A,0x22,0x29,0x34,0xBA,0x3D,0x44,0x4A,0x4F,0x1B,0x54,0x5D,0x65,0x6C,0xD6,0x73,0x7B,0x83,0x8C,0xD4,0x96,0x9C,0xA3,0xAA,0x93,0xB3,0xBE,0xCD,0xD5,0xF3,0xE0,0xEF,0xF9,0xFF,0x84);
    //truly_dcs_write_seq_static(ctx,0X00,0x60);
    //truly_dcs_write_seq_static(ctx,0XE1,0x00,0x02,0x07,0x0F,0x1E,0x1A,0x22,0x29,0x34,0xBA,0x3D,0x44,0x4A,0x4F,0x1B,0x54,0x5D,0x65,0x6C,0xD6,0x73,0x7B,0x83,0x8C,0xD4,0x96,0x9C,0xA3,0xAA,0x93,0xB3,0xBE,0xCD,0xD5,0xF3,0xE0,0xEF,0xF9,0xFF,0x84);
    //truly_dcs_write_seq_static(ctx,0X00,0x00,0x90);
    //truly_dcs_write_seq_static(ctx,0XE1,0x00,0x02,0x07,0x0F,0x1E,0x1A,0x22,0x29,0x34,0xBA,0x3D,0x44,0x4A,0x4F,0x1B,0x54,0x5D,0x65,0x6C,0xD6,0x73,0x7B,0x83,0x8C,0xD4,0x96,0x9C,0xA3,0xAA,0x93,0xB3,0xBE,0xCD,0xD5,0xF3,0xE0,0xEF,0xF9,0xFF,0x84);
    //truly_dcs_write_seq_static(ctx,0X00,0xC0);
    //truly_dcs_write_seq_static(ctx,0XE1,0x00,0x02,0x07,0x0F,0x36,0x1A,0x22,0x29,0x34,0xBA,0x3D,0x44,0x4A,0x4F,0x1B,0x54,0x5D,0x65,0x6C,0xD6,0x73,0x7B,0x83,0x8C,0xD4,0x96,0x9C,0xA3,0xAA,0x93,0xB3,0xBE,0xCD,0xD5,0xF3,0xE0,0xEF,0xF9,0xFF,0x84);
    //truly_dcs_write_seq_static(ctx,0X00,0xF0);
    //truly_dcs_write_seq_static(ctx,0XE1,0x00,0x02,0x07,0x0F,0x36,0x1A,0x22,0x29,0x34,0xBA,0x3D,0x44,0x4A,0x4F,0x1B,0x54);
    //truly_dcs_write_seq_static(ctx,0X00,0x00);
    //truly_dcs_write_seq_static(ctx,0XE2,0x5D,0x65,0x6C,0xD6,0x73,0x7B,0x83,0x8C,0xD4,0x96,0x9C,0xA3,0xAA,0x93,0xB3,0xBE,0xCD,0xD5,0xF3,0xE0,0xEF,0xF9,0xFF,0x84);

    truly_dcs_write_seq_static(ctx,0X00,0xE0);
    truly_dcs_write_seq_static(ctx,0XCF,0x34);

    truly_dcs_write_seq_static(ctx,0X00,0x85);
    truly_dcs_write_seq_static(ctx,0XA7,0x41);

    //ESD disable reg_21h_rev_disable。disable cmd1 21h=反色cmd
    truly_dcs_write_seq_static(ctx,0X00,0x80);
    truly_dcs_write_seq_static(ctx,0XB3,0x22);

    //HS lock CMD1. B3B0h=0x01->0x00。
    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XB3,0x00);

    //packet loss cover. B083h=0x61->0x63。
    truly_dcs_write_seq_static(ctx,0X00,0x83);
    truly_dcs_write_seq_static(ctx,0XB0,0x63);

    //MIPI_CH0_DLY_SEL B0A1h=0x04->0x02
    truly_dcs_write_seq_static(ctx,0X00,0xA1);
    truly_dcs_write_seq_static(ctx,0XB0,0x02);
    //cdphy_pdly。
    truly_dcs_write_seq_static(ctx,0X00,0xa9);
    truly_dcs_write_seq_static(ctx,0XB0,0xaa,0x0a);
    //dim 64 frame
    truly_dcs_write_seq_static(ctx,0X00,0xb5);
    truly_dcs_write_seq_static(ctx,0Xca,0x08);
    //pwm 12bit 54.2k
    truly_dcs_write_seq_static(ctx,0X00,0xB0);
    truly_dcs_write_seq_static(ctx,0XCA,0x0f,0x0f,0x09);

    truly_dcs_write_seq_static(ctx,0X1C,0x02);

    //CMD2 disable
    truly_dcs_write_seq_static(ctx,0X00,0x00);
    truly_dcs_write_seq_static(ctx,0XFF,0xFF,0xFF,0xFF);

    truly_dcs_write_seq_static(ctx,0X51,0xff);
    truly_dcs_write_seq_static(ctx,0X53,0x2c);
    truly_dcs_write_seq_static(ctx,0X55,0x00);
    truly_dcs_write_seq_static(ctx,0X35,0x00);
    //----------------------LCD initial code End----------------------//
    //SLPOUT and DISPON
	truly_dcs_write_seq_static(ctx, 0x11);
	msleep(100);
	/* Display On*/
	truly_dcs_write_seq_static(ctx, 0x29);
    msleep(20);
	pr_info("%s-\n", __func__);
}

static int truly_disable(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

//extern bool fts_gestrue_status;
static int truly_unprepare(struct drm_panel *panel)
{

	struct truly *ctx = panel_to_truly(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;

	truly_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
    msleep(50);
	truly_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(110);

	//if (fts_gestrue_status == 1) {
	//	printk("[FTS-lcm]truly_unprepare fts_gestrue_status = 1\n ");
	//} else {
	//	printk("[FTS-lcm]truly_unprepare fts_gestrue_status = 0\n ");
#if 0
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		usleep_range(2000, 2001);
#endif
        truly_dcs_write_seq_static(ctx,0X00,0x00);
        truly_dcs_write_seq_static(ctx,0Xff,0x87,0x22,0x01);
        truly_dcs_write_seq_static(ctx,0X00,0x80);
        truly_dcs_write_seq_static(ctx,0Xff,0x87,0x22);

        truly_dcs_write_seq_static(ctx,0X00,0x00);
        truly_dcs_write_seq_static(ctx,0Xf7,0x5a,0xa5,0x95,0x27);

        truly_dcs_write_seq_static(ctx,0X00,0x00);
        truly_dcs_write_seq_static(ctx,0Xff,0xff,0xff,0xff);
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
	//}
	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int truly_prepare(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// reset  L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
    devm_gpiod_put(ctx->dev, ctx->reset_gpio);
    usleep_range(1000, 1001);

    //ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
    //gpiod_set_value(ctx->pm_gpio, 1);
    //devm_gpiod_put(ctx->dev, ctx->pm_gpio);
    usleep_range(5000, 5001);

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
	truly_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		truly_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	truly_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int truly_enable(struct drm_panel *panel)
{
	struct truly *ctx = panel_to_truly(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (230)
#define HSA (32)
#define HBP (22)
#define VFP_90HZ (46)
#define VFP_60HZ (1282)
#define VSA (2)
#define VBP (18)
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
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x05, .count = 1, .para_list[0] = 0xFF,
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
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x05, .count = 1, .para_list[0] = 0xFF,
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

static int truly_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
#if 1
    char bl_tb0[] = {0x51, 0xff};
#else
    char bl_tb0[] = {0x51, 0xf, 0xff};
#endif
	if (level > 255)
		level = 255;
	pr_info("%s backlight = -%d\n", __func__, level);
#if 1
	bl_tb0[1] = (u8)level;
    pr_info("%s backlight = -%d,bl_tb0[1] = %x\n", __func__, level,bl_tb0[1]);
#else
	level = level * 4095 / 255;
	bl_tb0[1] = ((level >> 4) & 0xff);
	bl_tb0[2] = (level & 0x0f);
    pr_info("%s backlight = -%d,bl_tb0[1] = %x,bl_tb0[2] = %x\n", __func__, level,bl_tb0[1],bl_tb0[2]);
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
	struct truly *ctx = panel_to_truly(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = truly_setbacklight_cmdq,
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

static int truly_get_modes(struct drm_panel *panel)
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

static const struct drm_panel_funcs truly_drm_funcs = {
	.disable = truly_disable,
	.unprepare = truly_unprepare,
	.prepare = truly_prepare,
	.enable = truly_enable,
	.get_modes = truly_get_modes,
};

static int truly_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct truly *ctx;
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
	ctx = devm_kzalloc(dev, sizeof(struct truly), GFP_KERNEL);
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
	ctx->panel.funcs = &truly_drm_funcs;

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

	pr_info("%s- wt,truly,ft8722,cphy,vdo,90hz\n", __func__);

	return ret;
}

static int truly_remove(struct mipi_dsi_device *dsi)
{
	struct truly *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id truly_of_match[] = {
	{
	    .compatible = "wt,ft8722_fhdp_wt_dsi_vdo_cphy_90hz_truly",
	},
	{}
};

MODULE_DEVICE_TABLE(of, truly_of_match);

static struct mipi_dsi_driver truly_driver = {
	.probe = truly_probe,
	.remove = truly_remove,
	.driver = {
		.name = "ft8722_fhdp_wt_dsi_vdo_cphy_90hz_truly",
		.owner = THIS_MODULE,
		.of_match_table = truly_of_match,
	},
};

module_mipi_dsi_driver(truly_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt truly ft8722 vdo Panel Driver");
MODULE_LICENSE("GPL v2");
