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

#define txd_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		txd_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define txd_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
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

static u8 last_level[1];	//the last backlight level values

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

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x01);
	txd_dcs_write_seq_static(ctx,0x00,0x41);   //STVA
	txd_dcs_write_seq_static(ctx,0x01,0x42);   //STVA
	txd_dcs_write_seq_static(ctx,0x02,0x25);   //STVA_RISE_FTI 0710
	txd_dcs_write_seq_static(ctx,0x03,0x18);   //STVA_FALL_FTI 0710
	txd_dcs_write_seq_static(ctx,0x04,0xc3);   //STVB
	txd_dcs_write_seq_static(ctx,0x05,0x03);   //STVB
	txd_dcs_write_seq_static(ctx,0x06,0x00);   //STVB_RISE_FTI
	txd_dcs_write_seq_static(ctx,0x07,0x0a);   //STVB_FALL_FTI

	txd_dcs_write_seq_static(ctx,0x08,0x01);   //CLK_CKV 0722
	txd_dcs_write_seq_static(ctx,0x09,0x01);   //CLK_CKV 0722
	txd_dcs_write_seq_static(ctx,0x0a,0x73);   //CLK_CKV 0722
	txd_dcs_write_seq_static(ctx,0x0b,0x00);   //CLK_CKV 0722
	txd_dcs_write_seq_static(ctx,0x0c,0x00);   //CLK_CKV_RISE_CLW 0722
	txd_dcs_write_seq_static(ctx,0x0e,0x00);   //CLK_CKV_FALL_CLW 0722

	txd_dcs_write_seq_static(ctx,0x16,0x01);   //CLK_ENB 0722
	txd_dcs_write_seq_static(ctx,0x17,0x00);   //CLK_ENB 0722
	txd_dcs_write_seq_static(ctx,0x18,0x30);   //CLK_ENB 0722
	txd_dcs_write_seq_static(ctx,0x19,0x00);   //CLK_ENB 0722
	txd_dcs_write_seq_static(ctx,0x1a,0x20);   //CLK_ENB_RISE_CLW 0722
	txd_dcs_write_seq_static(ctx,0x1c,0x12);   //CLK_ENB_FALL_CLW 0722

	txd_dcs_write_seq_static(ctx,0x31,0x07);   //GOUTR_01	DUMMY
	txd_dcs_write_seq_static(ctx,0x32,0x07);   //GOUTR_02	DUMMY
	txd_dcs_write_seq_static(ctx,0x33,0x07);   //GOUTR_03	DUMMY
	txd_dcs_write_seq_static(ctx,0x34,0x08);   //GOUTR_04	STV
	txd_dcs_write_seq_static(ctx,0x35,0x07);   //GOUTR_05	DUMMY
	txd_dcs_write_seq_static(ctx,0x36,0x10);   //GOUTR_06	CKV
	txd_dcs_write_seq_static(ctx,0x37,0x07);   //GOUTR_07	DUMMY
	txd_dcs_write_seq_static(ctx,0x38,0x07);   //GOUTR_08	DUMMY
	txd_dcs_write_seq_static(ctx,0x39,0x1b);   //GOUTR_09	ENB4
	txd_dcs_write_seq_static(ctx,0x3a,0x1a);   //GOUTR_10	ENB3
	txd_dcs_write_seq_static(ctx,0x3b,0x19);   //GOUTR_11	ENB2
	txd_dcs_write_seq_static(ctx,0x3c,0x18);   //GOUTR_12	ENB1
	txd_dcs_write_seq_static(ctx,0x3d,0x07);   //GOUTR_13	DUMMY
	txd_dcs_write_seq_static(ctx,0x3e,0x07);   //GOUTR_14	DUMMY
	txd_dcs_write_seq_static(ctx,0x3f,0x07);   //GOUTR_15	DUMMY
	txd_dcs_write_seq_static(ctx,0x40,0x07);   //GOUTR_16	DUMMY
	txd_dcs_write_seq_static(ctx,0x41,0x07);   //GOUTR_17	DUMMY
	txd_dcs_write_seq_static(ctx,0x42,0x30);   //GOUTR_18	ASW3
	txd_dcs_write_seq_static(ctx,0x43,0x2e);   //GOUTR_19 	ASW1
	txd_dcs_write_seq_static(ctx,0x44,0x2f);   //GOUTR_20	ASW2
	txd_dcs_write_seq_static(ctx,0x45,0x07);   //GOUTR_21	DUMMY
	txd_dcs_write_seq_static(ctx,0x46,0x34);   //GOUTR_22	XASW1
	txd_dcs_write_seq_static(ctx,0x47,0x35);   //GOUTR_23	XASW2
	txd_dcs_write_seq_static(ctx,0x48,0x36);   //GOUTR_24	XASW3

	txd_dcs_write_seq_static(ctx,0x49,0x07);   //GOUTL_01	DUMMY
	txd_dcs_write_seq_static(ctx,0x4a,0x29);   //GOUTL_02  XDISC
	txd_dcs_write_seq_static(ctx,0x4b,0x07);   //GOUTL_03  DUMMY
	txd_dcs_write_seq_static(ctx,0x4c,0x00);   //GOUTL_04  UP
	txd_dcs_write_seq_static(ctx,0x4d,0x07);   //GOUTL_05  DUMMY
	txd_dcs_write_seq_static(ctx,0x4e,0x10);   //GOUTL_06  CKV
	txd_dcs_write_seq_static(ctx,0x4f,0x07);   //GOUTL_07  DUMMY
	txd_dcs_write_seq_static(ctx,0x50,0x07);   //GOUTL_08  DUMMY
	txd_dcs_write_seq_static(ctx,0x51,0x1b);   //GOUTL_09  ENB4
	txd_dcs_write_seq_static(ctx,0x52,0x1a);   //GOUTL_10  ENB3
	txd_dcs_write_seq_static(ctx,0x53,0x19);   //GOUTL_11  ENB2
	txd_dcs_write_seq_static(ctx,0x54,0x18);   //GOUTL_12  ENB1
	txd_dcs_write_seq_static(ctx,0x55,0x07);   //GOUTL_13  DUMMY
	txd_dcs_write_seq_static(ctx,0x56,0x07);   //GOUTL_14  DUMMY
	txd_dcs_write_seq_static(ctx,0x57,0x07);   //GOUTL_15  DUMMY
	txd_dcs_write_seq_static(ctx,0x58,0x07);   //GOUTL_16  DUMMY
	txd_dcs_write_seq_static(ctx,0x59,0x07);   //GOUTL_17  DUMMY
	txd_dcs_write_seq_static(ctx,0x5a,0x30);   //GOUTL_18  ASW3
	txd_dcs_write_seq_static(ctx,0x5b,0x2e);   //GOUTL_19  ASW1
	txd_dcs_write_seq_static(ctx,0x5c,0x2f);   //GOUTL_20  ASW2
	txd_dcs_write_seq_static(ctx,0x5d,0x07);   //GOUTL_21  DUMMY
	txd_dcs_write_seq_static(ctx,0x5e,0x34);   //GOUTL_22  XASW1
	txd_dcs_write_seq_static(ctx,0x5f,0x35);   //GOUTL_23  XASW2
	txd_dcs_write_seq_static(ctx,0x60,0x36);   //GOUTL_24  XASW3

	txd_dcs_write_seq_static(ctx,0x61,0x07);   //GOUTR_01	DUMMY
	txd_dcs_write_seq_static(ctx,0x62,0x07);   //GOUTR_02  DUMMY
	txd_dcs_write_seq_static(ctx,0x63,0x07);   //GOUTR_03  DUMMY
	txd_dcs_write_seq_static(ctx,0x64,0x08);   //GOUTR_04  STV
	txd_dcs_write_seq_static(ctx,0x65,0x07);   //GOUTR_05  DUMMY
	txd_dcs_write_seq_static(ctx,0x66,0x16);   //GOUTR_06  CKV
	txd_dcs_write_seq_static(ctx,0x67,0x07);   //GOUTR_07  DUMMY
	txd_dcs_write_seq_static(ctx,0x68,0x07);   //GOUTR_08  DUMMY
	txd_dcs_write_seq_static(ctx,0x69,0x18);   //GOUTR_09  ENB4
	txd_dcs_write_seq_static(ctx,0x6a,0x19);   //GOUTR_10  ENB3
	txd_dcs_write_seq_static(ctx,0x6b,0x1a);   //GOUTR_11  ENB2
	txd_dcs_write_seq_static(ctx,0x6c,0x1b);   //GOUTR_12  ENB1
	txd_dcs_write_seq_static(ctx,0x6d,0x07);   //GOUTR_13  DUMMY
	txd_dcs_write_seq_static(ctx,0x6e,0x07);   //GOUTR_14  DUMMY
	txd_dcs_write_seq_static(ctx,0x6f,0x07);   //GOUTR_15  DUMMY
	txd_dcs_write_seq_static(ctx,0x70,0x07);   //GOUTR_16  DUMMY
	txd_dcs_write_seq_static(ctx,0x71,0x07);   //GOUTR_17  DUMMY
	txd_dcs_write_seq_static(ctx,0x72,0x30);   //GOUTR_18  ASW3
	txd_dcs_write_seq_static(ctx,0x73,0x2e);   //GOUTR_19  ASW1
	txd_dcs_write_seq_static(ctx,0x74,0x2f);   //GOUTR_20  ASW2
	txd_dcs_write_seq_static(ctx,0x75,0x07);   //GOUTR_21  DUMMY
	txd_dcs_write_seq_static(ctx,0x76,0x34);   //GOUTR_22  XASW1
	txd_dcs_write_seq_static(ctx,0x77,0x35);   //GOUTR_23  XASW2
	txd_dcs_write_seq_static(ctx,0x78,0x36);   //GOUTR_24  XASW3

	txd_dcs_write_seq_static(ctx,0x79,0x07);   //GOUTL_01  DUMMY
	txd_dcs_write_seq_static(ctx,0x7a,0x29);   //GOUTL_02  XDISC
	txd_dcs_write_seq_static(ctx,0x7b,0x07);   //GOUTL_03  DUMMY
	txd_dcs_write_seq_static(ctx,0x7c,0x00);   //GOUTL_04  UP
	txd_dcs_write_seq_static(ctx,0x7d,0x07);   //GOUTL_05  DUMMY
	txd_dcs_write_seq_static(ctx,0x7e,0x16);   //GOUTL_06  CKV
	txd_dcs_write_seq_static(ctx,0x7f,0x07);   //GOUTL_07  DUMMY
	txd_dcs_write_seq_static(ctx,0x80,0x07);   //GOUTL_08  DUMMY
	txd_dcs_write_seq_static(ctx,0x81,0x18);   //GOUTL_09  ENB4
	txd_dcs_write_seq_static(ctx,0x82,0x19);   //GOUTL_10  ENB3
	txd_dcs_write_seq_static(ctx,0x83,0x1a);   //GOUTL_11  ENB2
	txd_dcs_write_seq_static(ctx,0x84,0x1b);   //GOUTL_12  ENB1
	txd_dcs_write_seq_static(ctx,0x85,0x07);   //GOUTL_13  DUMMY
	txd_dcs_write_seq_static(ctx,0x86,0x07);   //GOUTL_14  DUMMY
	txd_dcs_write_seq_static(ctx,0x87,0x07);   //GOUTL_15  DUMMY
	txd_dcs_write_seq_static(ctx,0x88,0x07);   //GOUTL_16  DUMMY
	txd_dcs_write_seq_static(ctx,0x89,0x07);   //GOUTL_17  DUMMY
	txd_dcs_write_seq_static(ctx,0x8a,0x30);   //GOUTL_18  ASW3
	txd_dcs_write_seq_static(ctx,0x8b,0x2e);   //GOUTL_19  ASW1
	txd_dcs_write_seq_static(ctx,0x8c,0x2f);   //GOUTL_20  ASW2
	txd_dcs_write_seq_static(ctx,0x8d,0x07);   //GOUTL_21  DUMMY
	txd_dcs_write_seq_static(ctx,0x8e,0x34);   //GOUTL_22  XASW1
	txd_dcs_write_seq_static(ctx,0x8f,0x35);   //GOUTL_23  XASW2
	txd_dcs_write_seq_static(ctx,0x90,0x36);   //GOUTL_24  XASW3

	txd_dcs_write_seq_static(ctx,0xa0,0x43);
	txd_dcs_write_seq_static(ctx,0xa1,0x00);
	txd_dcs_write_seq_static(ctx,0xa2,0x00);
	txd_dcs_write_seq_static(ctx,0xa3,0x00);
	txd_dcs_write_seq_static(ctx,0xa4,0x00);
	txd_dcs_write_seq_static(ctx,0xa5,0x00);
	txd_dcs_write_seq_static(ctx,0xa6,0x0a);
	txd_dcs_write_seq_static(ctx,0xa7,0x40);
	txd_dcs_write_seq_static(ctx,0xa8,0x10);
	txd_dcs_write_seq_static(ctx,0xb0,0x00);
	txd_dcs_write_seq_static(ctx,0xb1,0x04);     //0722
	txd_dcs_write_seq_static(ctx,0xb2,0x00);
	txd_dcs_write_seq_static(ctx,0xb3,0x08);     //0722
	txd_dcs_write_seq_static(ctx,0xc5,0x0a);
	txd_dcs_write_seq_static(ctx,0xc6,0x00);
	txd_dcs_write_seq_static(ctx,0xc7,0x00);
	txd_dcs_write_seq_static(ctx,0xc8,0x00);
	txd_dcs_write_seq_static(ctx,0xc9,0x00);
	txd_dcs_write_seq_static(ctx,0xca,0x71);
	txd_dcs_write_seq_static(ctx,0xd1,0x10);
	txd_dcs_write_seq_static(ctx,0xd2,0x20);
	txd_dcs_write_seq_static(ctx,0xd3,0x00);
	txd_dcs_write_seq_static(ctx,0xd4,0x01);
	txd_dcs_write_seq_static(ctx,0xd5,0x20);
	txd_dcs_write_seq_static(ctx,0xd6,0x21);
	txd_dcs_write_seq_static(ctx,0xd7,0x03);
	txd_dcs_write_seq_static(ctx,0xd8,0x80);
	txd_dcs_write_seq_static(ctx,0xd9,0x24);
	txd_dcs_write_seq_static(ctx,0xda,0x01);
	txd_dcs_write_seq_static(ctx,0xdb,0x09);
	txd_dcs_write_seq_static(ctx,0xdc,0x08);
	txd_dcs_write_seq_static(ctx,0xdd,0xc4);
	txd_dcs_write_seq_static(ctx,0xde,0x00);
	txd_dcs_write_seq_static(ctx,0xdf,0x80);
	txd_dcs_write_seq_static(ctx,0xe0,0x40);
	txd_dcs_write_seq_static(ctx,0xe1,0x48);
	txd_dcs_write_seq_static(ctx,0xe2,0x10);
	txd_dcs_write_seq_static(ctx,0xe3,0x20);
	txd_dcs_write_seq_static(ctx,0xe4,0x00);
	txd_dcs_write_seq_static(ctx,0xe5,0x09);
	txd_dcs_write_seq_static(ctx,0xe6,0x21);
	txd_dcs_write_seq_static(ctx,0xed,0x55);
	txd_dcs_write_seq_static(ctx,0xef,0x30);
	txd_dcs_write_seq_static(ctx,0xf0,0x24);      	//0709
	txd_dcs_write_seq_static(ctx,0xf4,0x54);

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x11);
	txd_dcs_write_seq_static(ctx,0x00,0x38);       //0722
	txd_dcs_write_seq_static(ctx,0x01,0x39);		//0722
	txd_dcs_write_seq_static(ctx,0x02,0x3a);		//0722
	txd_dcs_write_seq_static(ctx,0x03,0x3b);		//0722

	txd_dcs_write_seq_static(ctx,0x38,0x00);
	txd_dcs_write_seq_static(ctx,0x39,0x00);
	txd_dcs_write_seq_static(ctx,0x3a,0x10);
	txd_dcs_write_seq_static(ctx,0x3b,0x07);

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x02);
	txd_dcs_write_seq_static(ctx,0x1B,0x01);
	txd_dcs_write_seq_static(ctx,0x36,0x00);
	txd_dcs_write_seq_static(ctx,0x46,0x11);      	//DUMMY CKH  0924
	txd_dcs_write_seq_static(ctx,0x47,0x03);		//CKH CONNECT
	txd_dcs_write_seq_static(ctx,0x4F,0x01);		//CKH CONNECT
	txd_dcs_write_seq_static(ctx,0x5B,0x22);      	//DUMMY SRC 0924
	txd_dcs_write_seq_static(ctx,0xF9,0x34);

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x12);
	txd_dcs_write_seq_static(ctx,0x48,0x05);      //90_t8_de
	txd_dcs_write_seq_static(ctx,0x49,0x00);      //90_t7p_de
	txd_dcs_write_seq_static(ctx,0x4A,0x06);      //90_t9_de
	txd_dcs_write_seq_static(ctx,0x4B,0x1b);      //90_t7_de
	txd_dcs_write_seq_static(ctx,0x4E,0x03);      //90 sdt
	txd_dcs_write_seq_static(ctx,0x52,0x13);		//save power SRC bias through rate
	txd_dcs_write_seq_static(ctx,0x53,0x22);		//save power SRC bias current
	txd_dcs_write_seq_static(ctx,0xC8,0x46); 		//90 Hz BIST RTN
	txd_dcs_write_seq_static(ctx,0xCA,0x21); 		//90 Hz BIST VBP
	txd_dcs_write_seq_static(ctx,0xCB,0x32); 		//90 Hz BIST VFP


	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x04);
	txd_dcs_write_seq_static(ctx,0xB7,0xCF);
	txd_dcs_write_seq_static(ctx,0xB8,0x45);
	txd_dcs_write_seq_static(ctx,0xBA,0x74);
	txd_dcs_write_seq_static(ctx,0xBD,0x01);

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x05);
	txd_dcs_write_seq_static(ctx,0x1D,0x00);
	txd_dcs_write_seq_static(ctx,0x1E,0x7D);		//VCOM=-0.1V
	txd_dcs_write_seq_static(ctx,0x61,0xCB);
	txd_dcs_write_seq_static(ctx,0x72,0x57); 		//VGH=9.5V
	txd_dcs_write_seq_static(ctx,0x74,0x56); 		//VGL=-9V
	txd_dcs_write_seq_static(ctx,0x76,0x55); 		//VGHO=8.2V  0720
	txd_dcs_write_seq_static(ctx,0x7A,0x4D); 		//VGLO=-7.8V 0720
	txd_dcs_write_seq_static(ctx,0x7B,0x7E); 		//GVDDP=5V
	txd_dcs_write_seq_static(ctx,0x7C,0x7E); 		//GVDDN=-5V
	txd_dcs_write_seq_static(ctx,0x46,0x45); 		//PWR_TCON_VGHO_EN
	txd_dcs_write_seq_static(ctx,0x47,0x55); 		//PWR_TCON_VGLO_EN
	txd_dcs_write_seq_static(ctx,0xB5,0x45); 		//PWR_D2A_HVREG_VGHO_EN
	txd_dcs_write_seq_static(ctx,0xB7,0x55); 		//PWR_D2A_HVREG_VGLO_EN
	txd_dcs_write_seq_static(ctx,0xC6,0x1B);
	txd_dcs_write_seq_static(ctx,0x56,0xFF);       //0915 TD P2P


	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x06);
	txd_dcs_write_seq_static(ctx,0xC0,0x68);		//Res=10802408
	txd_dcs_write_seq_static(ctx,0xC1,0x19);		//Res=10802408
	txd_dcs_write_seq_static(ctx,0xC3,0x06); 		//SS_REG
	txd_dcs_write_seq_static(ctx,0x13,0x13);		//force otp DDI
	txd_dcs_write_seq_static(ctx,0xD6,0x55);
	txd_dcs_write_seq_static(ctx,0xCD,0x88);
	txd_dcs_write_seq_static(ctx,0x08,0x20);
	txd_dcs_write_seq_static(ctx,0x80,0x00);

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x07);
	txd_dcs_write_seq_static(ctx,0x03,0x20);
	txd_dcs_write_seq_static(ctx,0x07,0x4C);
	txd_dcs_write_seq_static(ctx,0x12,0x00);

	//Gamma Register
	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x08);
	txd_dcs_write_seq_static(ctx,0xE0,0x00,0x00,0x17,0x3e,0x00,0x77,0xa2,0xc6,0x14,0xfc,0x26,0x66,0x25,0x94,0xdc,0x17,0x2A,0x4f,0x8f,0xb9,0x3e,0xee,0x13,0x41,0x3F,0x5a,0x81,0xB2,0x0F,0xD6,0xD9);
	txd_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x17,0x3e,0x00,0x77,0xa2,0xc6,0x14,0xfc,0x26,0x66,0x25,0x94,0xdc,0x17,0x2A,0x4f,0x8f,0xb9,0x3e,0xee,0x13,0x41,0x3F,0x5a,0x81,0xB2,0x0F,0xD6,0xD9);

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x0B);	// AUTOTRIM
	txd_dcs_write_seq_static(ctx,0xC6,0x85);
	txd_dcs_write_seq_static(ctx,0xC7,0x6C);
	txd_dcs_write_seq_static(ctx,0xC8,0x04);
	txd_dcs_write_seq_static(ctx,0xC9,0x04);
	txd_dcs_write_seq_static(ctx,0xCA,0x87);
	txd_dcs_write_seq_static(ctx,0xCB,0x87);
	txd_dcs_write_seq_static(ctx,0xD8,0x04);
	txd_dcs_write_seq_static(ctx,0xD9,0x83);
	txd_dcs_write_seq_static(ctx,0xDA,0x03);
	txd_dcs_write_seq_static(ctx,0xDB,0x03);
	txd_dcs_write_seq_static(ctx,0xDC,0x71);
	txd_dcs_write_seq_static(ctx,0xDD,0x71);
	txd_dcs_write_seq_static(ctx,0xAB,0xE0);

	//TP MODULATION
	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x0C);
	txd_dcs_write_seq_static(ctx,0x40,0x26);
	txd_dcs_write_seq_static(ctx,0x41,0xAA);
	txd_dcs_write_seq_static(ctx,0x42,0x24);
	txd_dcs_write_seq_static(ctx,0x43,0xB2);
	txd_dcs_write_seq_static(ctx,0x44,0x26);
	txd_dcs_write_seq_static(ctx,0x45,0xB2);
	txd_dcs_write_seq_static(ctx,0x46,0x25);
	txd_dcs_write_seq_static(ctx,0x47,0xA2);
	txd_dcs_write_seq_static(ctx,0x48,0x24);
	txd_dcs_write_seq_static(ctx,0x49,0x9A);
	txd_dcs_write_seq_static(ctx,0x4A,0x26);
	txd_dcs_write_seq_static(ctx,0x4B,0xAE);
	txd_dcs_write_seq_static(ctx,0x4C,0x25);
	txd_dcs_write_seq_static(ctx,0x4D,0x9E);
	txd_dcs_write_seq_static(ctx,0x4E,0x26);
	txd_dcs_write_seq_static(ctx,0x4F,0xB6);
	txd_dcs_write_seq_static(ctx,0x50,0x26);
	txd_dcs_write_seq_static(ctx,0x51,0xA6);
	txd_dcs_write_seq_static(ctx,0x03,0x46);

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x0E);
	txd_dcs_write_seq_static(ctx,0x00,0xA3); 		//LH MODE
	txd_dcs_write_seq_static(ctx,0x02,0x0F);
	txd_dcs_write_seq_static(ctx,0x04,0x06); 		//TSHD_1VP Off & TSVD FREE RUN

	txd_dcs_write_seq_static(ctx,0xB0,0x01); 		//TP1 ASW prescan off 0722
	txd_dcs_write_seq_static(ctx,0xB3,0x00); 		//TP1 ASW ON 0722
	txd_dcs_write_seq_static(ctx,0xB4,0x33); 		//TP1 ASW ON 0722
	txd_dcs_write_seq_static(ctx,0xD7,0xAB); 		//TP1 ASW ON 0924
	txd_dcs_write_seq_static(ctx,0xB9,0xCC); 		//TSHD PS OFF
	txd_dcs_write_seq_static(ctx,0xBD,0x00); 		//TP2 ASW VGHO

	txd_dcs_write_seq_static(ctx,0xC0,0x12); 		//TP3 UNIT ASW prescan off 0903
	txd_dcs_write_seq_static(ctx,0xC1,0x80); 		//TP3 ASW star_high OPT
	txd_dcs_write_seq_static(ctx,0x21,0x85); 		//TSVD1 RISING POSITION SHIFT
	txd_dcs_write_seq_static(ctx,0x23,0x85); 		//TSVD2 RISING POSITION SHIFT
	txd_dcs_write_seq_static(ctx,0x41,0x44); 		//60HZ TSVD POSITION
	txd_dcs_write_seq_static(ctx,0x05,0x20); 		//TP modulation off 24h , on = 20

	// 90 HZ TABLE
	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x1E);
	txd_dcs_write_seq_static(ctx,0x16,0x06); 		//90Hz TP1-1
	txd_dcs_write_seq_static(ctx,0x1E,0x3E); 		//90Hz TP3-2
	txd_dcs_write_seq_static(ctx,0x1F,0x3E); 		//90Hz TP3-1

	txd_dcs_write_seq_static(ctx,0x20,0x05); 		//90Hz TP1-1
	txd_dcs_write_seq_static(ctx,0x22,0x06); 		//90Hz TP3-2
	txd_dcs_write_seq_static(ctx,0x23,0x1B); 		//90Hz TP3-1

	txd_dcs_write_seq_static(ctx,0x6B,0x08);       //TP modulation
	txd_dcs_write_seq_static(ctx,0x60,0x07);		//TP unit=8
	txd_dcs_write_seq_static(ctx,0x65,0x0A);		//TP2_unit0=174.1us
	txd_dcs_write_seq_static(ctx,0x66,0xE2);		//TP2_unit0=174.1us
	txd_dcs_write_seq_static(ctx,0x67,0x10);		//unit line=301
	txd_dcs_write_seq_static(ctx,0x69,0x2D);		//unit line=301
	txd_dcs_write_seq_static(ctx,0x6D,0x7C); 		//RTN=3.906us
	txd_dcs_write_seq_static(ctx,0x61,0x85);		//TSVD1 position
	txd_dcs_write_seq_static(ctx,0x63,0x85);		//TSVD2 position

	txd_dcs_write_seq_static(ctx,0xA4,0x1D); 		//90 T1
	txd_dcs_write_seq_static(ctx,0xA5,0x41); 		//90 T3
	txd_dcs_write_seq_static(ctx,0xA6,0x31); 		//90 T4
	txd_dcs_write_seq_static(ctx,0xA7,0x25); 		//90 T2
	//txd_dcs_write_seq_static(ctx,0xC8,0x21); 		//60HZ TSVD period

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x03);
	txd_dcs_write_seq_static(ctx,0x83,0xE0);
	txd_dcs_write_seq_static(ctx,0x84,0x10);//PAGE0

	txd_dcs_write_seq_static(ctx,0xFF,0x78,0x07,0x00);
	txd_dcs_write_seq_static(ctx,0x53,0x2c);//PAGE0
	txd_dcs_write_seq_static(ctx,0x68,0x04,0x00);

	txd_dcs_write_seq_static(ctx, 0x11);
	msleep(60);
	/* Display On*/
	txd_dcs_write_seq_static(ctx, 0x29);
	msleep(20);
	txd_dcs_write_seq_static(ctx,0x35,0x00);

	if(1 == rec_esd) {
		pr_info("recover_esd_set_backlight last_level=%x\n", last_level[0]);
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
//#+bug773028, fangzhihua.wt,mod, 20220628,TP bringup
extern bool  gestrue_status;
extern bool  gestrue_spay;
//#+bug773028, fangzhihua.wt,mod, 20220628,TP bringup
static int txd_unprepare(struct drm_panel *panel)
{

	struct txd *ctx = panel_to_txd(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;

	//add for TP suspend
	panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	printk("[ILITEK]tpd_ilitek_notifier_callback in txd-unprepare\n ");

	txd_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
    msleep(20);
	txd_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);

	msleep(200);
	if((gestrue_status == 1)||(gestrue_spay == 1)){
		printk("[ILITEK]TXD_unprepare gesture_status = 1\n ");
	} else {
		printk("[ILITEK]TXD_unprepare gesture_flag = 0\n ");
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

extern void ili_resume_by_ddi(void);//bug773028, fangzhihua.wt,mod, 20220628,TP bringup
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

	usleep_range(3000, 3001);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	_lcm_i2c_write_bytes(AVDD_REG, 0xf);
	_lcm_i2c_write_bytes(AVEE_REG, 0xf);

	txd_panel_init(ctx);

	ili_resume_by_ddi();//bug773028, fangzhihua.wt,mod, 20220628,TP bringup

	//add for TP resume
	panel_notifier_call_chain(PANEL_PREPARE, NULL);
	printk("[ILITEK]tpd_ilitek_notifier_callback in txd_prepare\n ");

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
#define VFP_90HZ (30)
#define VFP_60HZ (1250)
#define VSA (4)
#define VBP (20)
#define VAC (2408)
#define HAC (1080)
static const struct drm_display_mode default_mode = {
	.clock = 282736,
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
	.clock = 282736,
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
	.pll_clk = 505,
	//.vfp_low_power = VFP_45HZ,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
    .dyn_fps = {
        .switch_en = 1,
        .vact_timing_fps = 90,
    },
	.phy_timcon = {
		.hs_zero = 25,
		.hs_trail = 8,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 505,
	//.vfp_low_power = VFP_45HZ,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
    .dyn_fps = {
        .switch_en = 1,
        .vact_timing_fps = 90,
    },
	.phy_timcon = {
		.hs_zero = 25,
		.hs_trail = 8,
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
	bl_tb0[1] = ((level >> 8) & 0xf);
	bl_tb0[2] = (level & 0xff);
    pr_info("%s backlight = -%d,bl_tb0[1] = %x,bl_tb0[2] = %x\n", __func__, level,bl_tb0[1],bl_tb0[2]);
#endif
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

	pr_info("%s- wt,txd,ili7807s,cphy,vdo,90hz\n", __func__);

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
	    .compatible = "wt,ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_txd",
	},
	{}
};

MODULE_DEVICE_TABLE(of, txd_of_match);

static struct mipi_dsi_driver txd_driver = {
	.probe = txd_probe,
	.remove = txd_remove,
	.driver = {
		.name = "ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_txd",
		.owner = THIS_MODULE,
		.of_match_table = txd_of_match,
	},
};

module_mipi_dsi_driver(txd_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt txd ili7807s vdo Panel Driver");
MODULE_LICENSE("GPL v2");
