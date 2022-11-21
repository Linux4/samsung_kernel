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
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define ENABLE_MTK_LCD_HWID_CHECK_HX
#define ENABLE_MTK_LCD_DTS_CHECK_HX

/*******************************************************/
/* Backlight Function                                  */
/* Device Name : LM36274                               */
/* Interface   : I2C                                   */
/* Enable Mode : 1 PIN for 5P9, 5N9                    */
/*******************************************************/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#define LCM_I2C_COMPATIBLE 	"mediatek,i2c_lcd_bias"
#define LCM_I2C_ID_NAME 	"hx83108a_lm36274"

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
#ifdef ENABLE_MTK_LCD_HWID_CHECK_HX
static int _lcm_lcd_id_dt(void)
{
	struct device_node *root = of_find_node_by_path("/");
	int _lcd_id_gpio1, _lcd_id_gpio2, _lcd_id_gpio3;
	int _lcd_id = 0;

	pr_info("%s\n", __func__);

	if (IS_ERR_OR_NULL(root)) {
		pr_info("root dev node is NULL\n");
		return -1;
	}

	_lcd_id_gpio1 = of_get_named_gpio(root, "dtbo-lcd_id_1", 0);
	_lcd_id_gpio2 = of_get_named_gpio(root, "dtbo-lcd_id_2", 0);
	_lcd_id_gpio3 = of_get_named_gpio(root, "dtbo-lcd_id_3", 0);

	if ((_lcd_id_gpio1 < 0) || (_lcd_id_gpio2 < 0) || (_lcd_id_gpio3 < 0)) {
		pr_info("%s, _lcd_id_gpio is invalid\n", __func__);
		return _lcd_id;
	}

	/* TO DO : read gpio value */
	_lcd_id = (gpio_get_value(_lcd_id_gpio3) << 2) |
				(gpio_get_value(_lcd_id_gpio2) << 1) |
				(gpio_get_value(_lcd_id_gpio1));

	pr_info("[LCM][I2C][nt36525c] lcd_id 0x%x\n", _lcd_id);

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
#define LCD_HIMAX_HX83108A_ID      (0x5)
static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
#ifdef ENABLE_MTK_LCD_DTS_CHECK_HX
	struct device_node *lcd_comp;
#endif
	int ret;

	pr_debug("[LCM][I2C][hx83108a] %s\n", __func__);
	pr_debug("[LCM][I2C][hx83108a] HX: info==>name=%s addr=0x%x\n", client->name,
		 client->addr);
	_lcm_i2c_client = client;

#ifdef ENABLE_MTK_LCD_HWID_CHECK_HX
	ret = _lcm_lcd_id_dt();

	if (ret != LCD_HIMAX_HX83108A_ID && ret != 0) {
		pr_info("hx83108a : %s : LCD HW ID doesn't match\n", __func__);
		return -1;
	}
#endif

#ifdef ENABLE_MTK_LCD_DTS_CHECK_HX
	lcd_comp = of_find_compatible_node(NULL, NULL,
	"k6833v1,hx83108a,vdo");
	if (lcd_comp) {
		pr_info("hx83108a : %s : panel compatible match\n", __func__);
		return 0;
	}

	pr_info("hx83108a : %s : panel compatible doesn't match\n", __func__);
	return -EPERM;
#else
	return 0;
#endif
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_info("[LCM][I2C][hx83108a] %s\n", __func__);
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
		pr_debug("ERROR!![hx83108a] _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_debug("[LCM][ERROR][hx83108a] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_info("[LCM][I2C][hx83108a] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_info("[LCM][I2C][hx83108a] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_info("[LCM][I2C][hx83108a] %s\n", __func__);
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
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
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
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
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
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
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

	lcm_dcs_write_seq_static(ctx, 0xB9, 0x83, 0x10, 0x8A);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x2C, 0x35, 0x35, 0x2F, 0x33,
		0x22, 0x43, 0x5C, 0x16, 0x16, 0x0C, 0x98);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xD0);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x34);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x00, 0x26, 0xD0, 0x40, 0x00,
		0x1A, 0xC6, 0x00, 0x65, 0x11, 0x00, 0x00, 0x01, 0x14, 0x22);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x5D, 0x51, 0x5D, 0x51, 0x5D,
		0x51, 0x5D, 0x51, 0x5D, 0x51, 0x02, 0x59, 0x01, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x73, 0x03, 0xA8, 0x95);
	lcm_dcs_write_seq_static(ctx, 0xBF, 0xFC, 0x40);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x34, 0x34, 0x11, 0x00, 0xC4,
		0x5F);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xC5);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x88, 0xCA, 0x08, 0x14, 0x02,
		0x04, 0x04, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xC6);
	lcm_dcs_write_seq_static(ctx, 0xC8, 0x87, 0x13, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x13, 0x08, 0xE0, 0x09, 0xA6);
	lcm_dcs_write_seq_static(ctx, 0xCC, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD1, 0x27, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD2, 0x00, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xD3, 0xC0, 0x00, 0x08, 0x08, 0x00,
		0x00, 0x47, 0x47, 0x44, 0x43, 0x14, 0x14, 0x14, 0x14, 0x32,
		0x10, 0x10, 0x00, 0x10, 0x32, 0x16, 0x51, 0x06, 0x51, 0x32,
		0x10, 0x08, 0x00, 0x08, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xD5, 0x18, 0x18, 0x18, 0x18, 0xBE, 
		0xBE, 0x18, 0x18, 0x20, 0x21, 0x22, 0x23, 0x04, 0x05, 0x06,
		0x07, 0x00, 0x01, 0x02, 0x03, 0x3E, 0x3E, 0x3E, 0x3E, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18);
	lcm_dcs_write_seq_static(ctx, 0xD6, 0x18, 0x18, 0x3E, 0x3E, 0xBE,
		0xBE, 0x18, 0x18, 0x23, 0x22, 0x21, 0x20, 0x03, 0x02, 0x01,
		0x00, 0x07, 0x06, 0x05, 0x04, 0x18, 0x18, 0x3E, 0x3E, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18);
	lcm_dcs_write_seq_static(ctx, 0xD8, 0x2E, 0xAA, 0xAF, 0x00, 0x00,
		0x00, 0x2E, 0xAA, 0xAF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00);
	lcm_dcs_write_seq_static(ctx, 0xE0, 0x00, 0x03, 0x0A, 0x0F, 0x15,
		0x20, 0x34, 0x3B, 0x42, 0x3F, 0x5B, 0x63, 0x6B, 0x7D, 0x7F,
		0x8A, 0x95, 0xA9, 0xAA, 0x54, 0x5D, 0x68, 0x73, 0x00, 0x03,
		0x0A, 0x0F, 0x15, 0x20, 0x34, 0x3B, 0x42, 0x3F, 0x5B, 0x63,
		0x6B, 0x7D, 0x7F, 0x8A, 0x95, 0xA9, 0xAA, 0x54, 0x5D, 0x68,
		0x73);
	lcm_dcs_write_seq_static(ctx, 0xE7, 0x07, 0x14, 0x14, 0x0D, 0x0F,
		0x84, 0x00, 0x3C, 0x3C, 0x32, 0x32, 0x00, 0x00, 0x00, 0x00,
		0xBA, 0x07, 0x01, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x01, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0xBF, 0x0E);
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xC6);
	lcm_dcs_write_seq_static(ctx, 0xD2, 0x10, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xC4);
	lcm_dcs_write_seq_static(ctx, 0xD3, 0x28);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0xD8, 0x2A, 0xAA, 0xAF, 0x00, 0x00,
		0x00, 0x2A, 0xAA, 0xAF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00);
	lcm_dcs_write_seq_static(ctx, 0xE7, 0x01, 0x00, 0x20, 0x01, 0x00,
		0x00, 0x29, 0x02, 0x33, 0x04, 0x1B, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xBF, 0xFA);
	lcm_dcs_write_seq_static(ctx, 0xD8, 0x3F, 0xFF, 0xFF, 0x00, 0x00,
		0x00, 0x3F, 0xFF, 0xFF, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xE7, 0xFD, 0x01, 0xFD, 0x01, 0x00,
		0x00, 0x04, 0x00, 0x00, 0x81, 0x02, 0x40, 0x00, 0x20, 0x5E,
		0x03, 0x02, 0x01, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xD8, 0xAE, 0xAA, 0xAA, 0xAA, 0xAA,
		0xA0, 0xAE, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0xAE, 0xAA, 0xAA,
		0xAA, 0xAA, 0xA0, 0xAE, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xCE, 0x00, 0x89, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x00, 0x1E, 0x18, 0xCE);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x0F, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);

	msleep(11);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x00);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x00);
	msleep(20);
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

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);


	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	udelay(1000);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(ctx->dev, "%s: cannot get bias_neg %ld\n",
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

#define HFP (48)
#define HSA (20)
#define HBP (20)
#define VFP (130)
#define VSA (8)
#define VBP (20)
#define VAC (1600)
#define HAC (720)
static u32 fake_heigh = VAC;
static u32 fake_width = HAC;
static bool need_fake_resolution;

static struct drm_display_mode default_mode = {
	.clock = 163406,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 60,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
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

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_debug("%s error\n", __func__);
		return 0;
	}

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x0F, 0xFF};

	bl_tb0[2] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 320,
	.vfp_low_power = 254,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
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
		mode->vdisplay = fake_heigh;
		mode->vsync_start = fake_heigh + VFP;
		mode->vsync_end = fake_heigh + VFP + VSA;
		mode->vtotal = fake_heigh + VFP + VSA + VBP;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hdisplay = fake_width;
		mode->hsync_start = fake_width + HFP;
		mode->hsync_end = fake_width + HFP + HSA;
		mode->htotal = fake_width + HFP + HSA + HBP;
	}
}

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	if (need_fake_resolution)
		change_drm_disp_mode_params(&default_mode);
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
#ifdef ENABLE_MTK_LCD_DTS_CHECK_HX
	struct device_node *lcd_comp;
#endif
	int ret;

#ifdef ENABLE_MTK_LCD_HWID_CHECK_HX
	ret = _lcm_lcd_id_dt();

	if (ret != LCD_HIMAX_HX83108A_ID && ret != 0) {
		pr_info("hx83108a : %s : LCD HW ID doesn't match\n", __func__);
		return -1;
	}
#endif

	pr_info("%s-hx83108a IN\n", __func__);
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
		dev_info(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(dev, "%s: cannot get bias-neg 1 %ld\n",
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
	pr_info("%s-hx83108a\n", __func__);

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
	{ .compatible = "k6833v1,hx83108a,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-k6833v1-hx83108a-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Jenny Kim <Jenny.Kim@mediatek.com>");
MODULE_DESCRIPTION("hx83108a auo VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");

