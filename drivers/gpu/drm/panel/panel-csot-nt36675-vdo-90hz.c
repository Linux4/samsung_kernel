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

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>
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
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#include "../mediatek/mtk_cust.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define ENABLE_MTK_LCD_HWID_CHECK_NVT
/*******************************************************/
/* Backlight Function                                  */
/* Device Name : LM36274                               */
/* Interface   : I2C                                   */
/* Enable Mode : 1 PIN for 5P9, 5N9                    */
/*******************************************************/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#define LCM_I2C_COMPATIBLE 	"mediatek,i2c_lcd_bias"
#define LCM_I2C_ID_NAME 	"nt36675_lm36274"

static struct i2c_client *_lcm_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);
static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value);

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct _lcm_i2c_dev {
	struct i2c_client *client;
};

static const struct of_device_id _lcm_i2c_of_match[] = {
	{ .compatible = LCM_I2C_COMPATIBLE,	},
	{},
};

static const struct i2c_device_id _lcm_i2c_id[] = {
	{ LCM_I2C_ID_NAME, 0 },
	{},
};

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},
};

/*****************************************************************************
 * Device Control Function
 *****************************************************************************/
#ifdef ENABLE_MTK_LCD_HWID_CHECK_NVT
static unsigned int _lcm_lcd_id_dt(void)
{
	struct device_node *root = of_find_node_by_path("/");
	int _lcd_id_gpio1, _lcd_id_gpio2, _lcd_id_gpio3;
	int _lcd_id;

	pr_info("%s\n", __func__);

	if (IS_ERR_OR_NULL(root)) {
		pr_info("root dev node is NULL\n");
		return -1;
	}

	_lcd_id_gpio1 = of_get_named_gpio(root, "dtbo-lcd_id_1", 0);
	_lcd_id_gpio2 = of_get_named_gpio(root, "dtbo-lcd_id_2", 0);
	_lcd_id_gpio3 = of_get_named_gpio(root, "dtbo-lcd_id_3", 0);

	/* TO DO : read gpio value */
	_lcd_id = (gpio_get_value(_lcd_id_gpio3) << 2) |
				(gpio_get_value(_lcd_id_gpio2) << 1) |
				(gpio_get_value(_lcd_id_gpio1));

	pr_info("[LCM][I2C][nt36675] lcd_id 0x%x\n", _lcd_id);

	return _lcd_id;
}
#endif

static void _lm36274_init(void)
{
	_lcm_i2c_write_bytes(0x0C, 0x2C);
	_lcm_i2c_write_bytes(0x0D, 0x26);
	_lcm_i2c_write_bytes(0x0E, 0x26);
	_lcm_i2c_write_bytes(0x09, 0xBE);
	_lcm_i2c_write_bytes(0x02, 0x6B);
	_lcm_i2c_write_bytes(0x03, 0x0D);
	_lcm_i2c_write_bytes(0x11, 0x74);
	_lcm_i2c_write_bytes(0x04, 0x05);
	_lcm_i2c_write_bytes(0x05, 0xCC);
	_lcm_i2c_write_bytes(0x10, 0x67);
	_lcm_i2c_write_bytes(0x08, 0x13);

	msleep(2);
}

/*****************************************************************************
 * I2C Interface Function
 *****************************************************************************/
/* bitfield: GPIO173, GPIO174, GPIO175, GPIO176 */
#define BOARD_HW_REV_BRINGUP1     (0b0000)
#define BOARD_HW_REV_BRINGUP2     (0b0001)
#define BOARD_HW_REV_BRINGUP3     (0b0010)
#define BOARD_HW_REV_00           (0b0011)
#define BOARD_HW_REV_01           (0b0100)
#define BOARD_HW_REV_02           (0b0101)

#define LCD_CSOT_NTW36675_ID      (0b101)
static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device_node *root = of_find_node_by_path("/");
	unsigned int hw_version;
	int ret;

	pr_err("[LCM][I2C][nt36675] %s\n", __func__);
	pr_debug("[LCM][I2C][nt36675] NT: info==>name=%s addr=0x%x\n", client->name,
		 client->addr);
	_lcm_i2c_client = client;

	if (IS_ERR_OR_NULL(root)) {
		pr_info("root dev node is NULL\n");
		return -1;
	}

	ret = of_property_read_u32(root, "dtbo-hw_rev", &hw_version);
	if (ret < 0) {
		pr_info("get dtbo-hw_rev fail:%d\n", ret);
		hw_version = 0;
	} else {
		pr_info("Get HW version = %d\n", hw_version);
	}

#ifdef ENABLE_MTK_LCD_HWID_CHECK_NVT
	if ((hw_version >= BOARD_HW_REV_02) && (_lcm_lcd_id_dt() == LCD_CSOT_NTW36675_ID)) {
		pr_info("nt36675 : %s : hw board revision and LCM match\n", __func__);
		return 0;
	}
	
	pr_info("nt36675 : %s : hw board revision and LCM doesn't match.\n", __func__);
	return -EPERM;
#else
	return 0;
#endif
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_info("[LCM][I2C][nt36675] %s\n", __func__);
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}

static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_err("ERROR!![nt36675] _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_err("[LCM][ERROR][nt36675] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_info("[LCM][I2C][nt36675] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_info("[LCM][I2C][nt36675] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_info("[LCM][I2C][nt36675] %s\n", __func__);
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);

/*******************************************************/
/* End Backlight function                              */
/*******************************************************/

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_set_lcm(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	struct mtk_ddic_dsi_msg *cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));\
	cmd_msg->channel = 0;\
	cmd_msg->flags = 0;\
	cmd_msg->tx_cmd_num = 1;\
	cmd_msg->tx_buf[0] = d;\
	cmd_msg->tx_len[0] = (sizeof(d) / sizeof(u8));\
	set_lcm(cmd_msg);\
	vfree(cmd_msg);\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

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
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
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
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{
	_lm36274_init();

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(10 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x64);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x69, 0x92);
	lcm_dcs_write_seq_static(ctx, 0x95, 0xD1);
	lcm_dcs_write_seq_static(ctx, 0x96, 0xD1); /* 10 */
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x54);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x54);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x54);
	lcm_dcs_write_seq_static(ctx, 0xF8, 0x54);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x24);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x02, 0x17); /* 20 */
	lcm_dcs_write_seq_static(ctx, 0x03, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x04, 0x27);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x0A, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x03); /* 30 */
	lcm_dcs_write_seq_static(ctx, 0x11, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x13, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x19, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x1A, 0x17);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0x26);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x27); /* 40 */
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x24, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x28, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x05); /* 50 */
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0x31, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x32, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x33, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x34, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x36, 0x68);
	lcm_dcs_write_seq_static(ctx, 0x45, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x47, 0x04); /* 60 */
	lcm_dcs_write_seq_static(ctx, 0x4B, 0x49);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0x6B);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0x6B);
	lcm_dcs_write_seq_static(ctx, 0x50, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x6B);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x7A, 0x83);
	lcm_dcs_write_seq_static(ctx, 0x7B, 0x8F);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x80, 0x02); /* 70 */
	lcm_dcs_write_seq_static(ctx, 0x81, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x82, 0x16);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x25);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x34);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x43);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x52);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x61);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x43);
	lcm_dcs_write_seq_static(ctx, 0x91, 0x52);
	lcm_dcs_write_seq_static(ctx, 0x92, 0x61); /* 80 */
	lcm_dcs_write_seq_static(ctx, 0x93, 0x16);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x25);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x34);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xA0, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xA2, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xA3, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA4, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xA5, 0x02); /* 90 */
	lcm_dcs_write_seq_static(ctx, 0xAA, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xAD, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xB6, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0xBC, 0x17);
	lcm_dcs_write_seq_static(ctx, 0xBF, 0x14);
	lcm_dcs_write_seq_static(ctx, 0xC4, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0xC9); /* 100 */
	lcm_dcs_write_seq_static(ctx, 0xD9, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xE5, 0x06);
	lcm_dcs_write_seq_static(ctx, 0xE6, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xED, 0xC0);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x67, 0x21);
	lcm_dcs_write_seq_static(ctx, 0x68, 0x58);
	lcm_dcs_write_seq_static(ctx, 0x69, 0x10); /* 110 */
	lcm_dcs_write_seq_static(ctx, 0x6B, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x6D);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x72);
	lcm_dcs_write_seq_static(ctx, 0x7E, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x07);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x26);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x2B); /* 120 */
	lcm_dcs_write_seq_static(ctx, 0x04, 0x2B);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0x74, 0xAF);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x01); /* 130 */
	lcm_dcs_write_seq_static(ctx, 0x87, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0x54);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x63);
	lcm_dcs_write_seq_static(ctx, 0x8E, 0x72);
	lcm_dcs_write_seq_static(ctx, 0x8F, 0x27);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x36);
	lcm_dcs_write_seq_static(ctx, 0x91, 0x45);
	lcm_dcs_write_seq_static(ctx, 0x9A, 0x80); /* 140 */
	lcm_dcs_write_seq_static(ctx, 0x9B, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x27);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x62);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x02, 0xD0);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x80); /* 150 */
	lcm_dcs_write_seq_static(ctx, 0x21, 0x92);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x81);
	lcm_dcs_write_seq_static(ctx, 0x26, 0xD3);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xF9, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x2A);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x64); /* 160 */
	lcm_dcs_write_seq_static(ctx, 0x0A, 0x30);
	lcm_dcs_write_seq_static(ctx, 0x11, 0xA0);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x12);
	lcm_dcs_write_seq_static(ctx, 0x16, 0xB6);
	lcm_dcs_write_seq_static(ctx, 0x19, 0x12);
	lcm_dcs_write_seq_static(ctx, 0x1A, 0x8A);
	lcm_dcs_write_seq_static(ctx, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0x1D);
	lcm_dcs_write_seq_static(ctx, 0x1E, 0x70);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x70); /* 170 */
	lcm_dcs_write_seq_static(ctx, 0x20, 0x70);
	lcm_dcs_write_seq_static(ctx, 0x45, 0xA0);
	lcm_dcs_write_seq_static(ctx, 0x75, 0x6A);
	lcm_dcs_write_seq_static(ctx, 0xA5, 0x50);
	lcm_dcs_write_seq_static(ctx, 0xD8, 0x40);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xE0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xF0); /* 180 */
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x40);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xD0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x20, 0x00, 0x5F, 0x00, 0x8A, 0x00, 0xAB,
										0x00, 0xC5, 0x00, 0xDB, 0x00, 0xEF);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x01, 0x00, 0x01, 0x36, 0x01, 0x60, 0x01, 0x9D, 0x01, 0xCB,
										0x02, 0x10, 0x02, 0x47, 0x02, 0x49);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x7D, 0x02, 0xB9, 0x02, 0xDE, 0x03, 0x0E, 0x03, 0x2D,
										0x03, 0x55, 0x03, 0x62, 0x03, 0x6E);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x7F, 0x03, 0x8D, 0x03, 0xA7, 0x03, 0xB8, 0x03, 0xD2,
										0x03, 0xD8);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x00, 0x00, 0x20, 0x00, 0x5F, 0x00, 0x8A, 0x00, 0xAB,
										0x00, 0xC5, 0x00, 0xDB, 0x00, 0xEF);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x01, 0x00, 0x01, 0x36, 0x01, 0x60, 0x01, 0x9D, 0x01, 0xCB,
										0x02, 0x10, 0x02, 0x47, 0x02, 0x49);
	lcm_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x7D, 0x02, 0xB9, 0x02, 0xDE, 0x03, 0x0E, 0x03, 0x2D,
										0x03, 0x55, 0x03, 0x62, 0x03, 0x6E);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x7F, 0x03, 0x8D, 0x03, 0xA7, 0x03, 0xB8, 0x03, 0xD2,
										0x03, 0xD8);
	lcm_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x00, 0x00, 0x20, 0x00, 0x5F, 0x00, 0x8A, 0x00, 0xAB,
										0x00, 0xC5, 0x00, 0xDB, 0x00, 0xEF);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x01, 0x00, 0x01, 0x36, 0x01, 0x60, 0x01, 0x9D, 0x01, 0xCB,
										0x02, 0x10, 0x02, 0x47, 0x02, 0x49);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x7D, 0x02, 0xB9, 0x02, 0xDE, 0x03, 0x0E, 0x03, 0x2D,
										0x03, 0x55, 0x03, 0x62, 0x03, 0x6E);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x7F, 0x03, 0x8D, 0x03, 0xA7, 0x03, 0xB8, 0x03, 0xD2,
										0x03, 0xD8);
	lcm_dcs_write_seq_static(ctx, 0xC6, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xC8, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xCC, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xCD, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xCE, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xCF, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD0, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD1, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD2, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD3, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD4, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD5, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD6, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD7, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD8, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xD9, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xDA, 0x11);
#if 1 /* SEQ_NT36675_001 ~ SEQ_NT36675_219 are used. unused from SEQ_NT36675_220 */
	lcm_dcs_write_seq_static(ctx, 0xDB, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xDC, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xDD, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xDE, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE0, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE2, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE3, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE4, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE5, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE6, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE7, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE8, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x11);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x20, 0x00, 0x5F, 0x00, 0x8A, 0x00, 0xAB,
										0x00, 0xC5, 0x00, 0xDB, 0x00, 0xEF);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x01, 0x00, 0x01, 0x36, 0x01, 0x60, 0x01, 0x9D, 0x01, 0xCB,
										0x02, 0x10, 0x02, 0x47, 0x02, 0x49);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x7D, 0x02, 0xB9, 0x02, 0xDE, 0x03, 0x0E, 0x03, 0x2D,
										0x03, 0x55, 0x03, 0x62, 0x03, 0x6E);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x7F, 0x03, 0x8D, 0x03, 0xA7, 0x03, 0xB8, 0x03, 0xD2,
										0x03, 0xD8);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x00, 0x00, 0x20, 0x00, 0x5F, 0x00, 0x8A, 0x00, 0xAB,
										0x00, 0xC5, 0x00, 0xDB, 0x00, 0xEF);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x01, 0x00, 0x01, 0x36, 0x01, 0x60, 0x01, 0x9D, 0x01, 0xCB,
										0x02, 0x10, 0x02, 0x47, 0x02, 0x49);
	lcm_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x7D, 0x02, 0xB9, 0x02, 0xDE, 0x03, 0x0E, 0x03, 0x2D,
										0x03, 0x55, 0x03, 0x62, 0x03, 0x6E);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x7F, 0x03, 0x8D, 0x03, 0xA7, 0x03, 0xB8, 0x03, 0xD2,
										0x03, 0xD8);
	lcm_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x00, 0x00, 0x20, 0x00, 0x5F, 0x00, 0x8A, 0x00, 0xAB,
										0x00, 0xC5, 0x00, 0xDB, 0x00, 0xEF);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x01, 0x00, 0x01, 0x36, 0x01, 0x60, 0x01, 0x9D, 0x01, 0xCB,
										0x02, 0x10, 0x02, 0x47, 0x02, 0x49);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x7D, 0x02, 0xB9, 0x02, 0xDE, 0x03, 0x0E, 0x03, 0x2D,
										0x03, 0x55, 0x03, 0x62, 0x03, 0x6E);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x7F, 0x03, 0x8D, 0x03, 0xA7, 0x03, 0xB8, 0x03, 0xD2,
										0x03, 0xD8);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
#endif /* SEQ_NT36675_001 ~ SEQ_NT36675_219 are used. unused from SEQ_NT36675_220 */

	lcm_dcs_write_seq_static(ctx, 0x51, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x24); /* brightness_mode */
	lcm_dcs_write_seq_static(ctx, 0x11); /* sleep_out */
	msleep(150);
	lcm_dcs_write_seq_static(ctx, 0x29); /* display_on */

}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	pr_info("%s\n", __func__);

	ctx->error = 0;
	ctx->prepared = false;

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);


	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	udelay(1000);
#if 0
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
#endif
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

#if 0
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);
#endif
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

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

#define HFP_60Hz (100)
#define HSA_60Hz (22)
#define HBP_60Hz (46)
#define VFP_60Hz (950)
#define VSA_60Hz (2)
#define VBP_60Hz (8)

#define HFP_90Hz (100)
#define HSA_90Hz (22)
#define HBP_90Hz (46)
#define VFP_90Hz (97)
#define VSA_90Hz (2)
#define VBP_90Hz (8)

#define VAC (1600)
#define HAC (720)
static u32 fake_heigh = 1600;
static u32 fake_width = 720;
static bool need_fake_resolution;

static struct drm_display_mode default_mode = {
	.clock = 163406,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP_60Hz,
	.hsync_end = HAC + HFP_60Hz + HSA_60Hz,
	.htotal = HAC + HFP_60Hz + HSA_60Hz + HBP_60Hz,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60Hz,
	.vsync_end = VAC + VFP_60Hz + VSA_60Hz,
	.vtotal = VAC + VFP_60Hz + VSA_60Hz + VBP_60Hz,
	.vrefresh = 60,
};

static struct drm_display_mode performance_mode = {
	.clock = 163406,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP_90Hz,
	.hsync_end = HAC + HFP_90Hz + HSA_90Hz,
	.htotal = HAC + HFP_90Hz + HSA_90Hz + HBP_90Hz,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90Hz,
	.vsync_end = VAC + VFP_90Hz + VSA_90Hz,
	.vtotal = VAC + VFP_90Hz + VSA_90Hz + VBP_90Hz,
	.vrefresh = 90,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 440,
	.vfp_low_power = 1294,//60hz
//	.vfp_low_power = 2533, blink 
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = 880,
	.dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 90,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 440,
	.vfp_low_power = 1294,//60hz
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = 880,
	.dyn_fps = {
		.switch_en = 1, .vact_timing_fps = 90,
	},
};

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x00, 0x00, 0x00};
	ssize_t ret;

	pr_info("%s success\n", __func__);
#if 0
	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);
#endif
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

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

	if (m->vrefresh == 60)
		ext->params = &ext_params;
	else if (m->vrefresh == 90)
		ext->params = &ext_params_90hz;
	else
		ret = 1;

	pr_info("mtk_panel_ext_param_set: %dHz\n", m->vrefresh);

	return ret;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
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

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	if (fake_heigh > 0 && fake_heigh < VAC) {
		mode->vsync_start = mode->vsync_start - mode->vdisplay
					+ fake_heigh;
		mode->vsync_end = mode->vsync_end - mode->vdisplay + fake_heigh;
		mode->vtotal = mode->vtotal - mode->vdisplay + fake_heigh;
		mode->vdisplay = fake_heigh;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hsync_start = mode->hsync_start - mode->hdisplay
					+ fake_width;
		mode->hsync_end = mode->hsync_end - mode->hdisplay + fake_width;
		mode->htotal = mode->htotal - mode->hdisplay + fake_width;
		mode->hdisplay = fake_width;
	}
}

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;

	if (need_fake_resolution) {
		change_drm_disp_mode_params(&default_mode);
		change_drm_disp_mode_params(&performance_mode);
	}
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

	panel->connector->display_info.width_mm = 64;
	panel->connector->display_info.height_mm = 129;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	if (ret)
		need_fake_resolution = false;
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret)
		need_fake_resolution = false;
	if (fake_heigh > 0 && fake_heigh < VAC)
		need_fake_resolution = true;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int lcd_id_pins = 0;
	int ret;

#ifdef ENABLE_MTK_LCD_HWID_CHECK_NVT
	if (_lcm_lcd_id_dt() != LCD_CSOT_NTW36675_ID) {
		pr_info("[NVT]%s: LCD ID not match\n", __func__);
		return -1;
	}
#endif

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
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
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
#if 0
	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);
#endif
	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

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
	check_is_need_fake_resolution(dev);
	pr_info("%s-nt36675\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "k6853v1,nt36675,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-k6853v1_64-nt36675-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Rio Moon <rio.moon@mediatek.com>");
MODULE_DESCRIPTION("Novatek nt36675 VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");

