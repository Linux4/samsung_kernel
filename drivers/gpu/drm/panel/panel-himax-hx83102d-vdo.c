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

#define ENABLE_MTK_BOARD_ID_CHECK_HX

/*******************************************************/
/* Backlight Function                                  */
/* Device Name : LM36274                               */
/* Interface   : I2C                                   */
/* Enable Mode : 1 PIN for 5P9, 5N9                    */
/*******************************************************/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#define LCM_I2C_COMPATIBLE 	"mediatek,i2c_lcd_bias"
#define LCM_I2C_ID_NAME 	"hx83102d_lm36274"

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
#define BOARD_HW_REV_00     (0b0000)
#define BOARD_HW_REV_01     (0b0001)
#define BOARD_HW_REV_02     (0b0010)

static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device_node *root = of_find_node_by_path("/");
	unsigned int hw_version;
	int ret;

	pr_debug("[LCM][I2C][hx83102d] %s\n", __func__);
	pr_debug("[LCM][I2C][hx83102d] HX: info==>name=%s addr=0x%x\n", client->name,
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

#ifdef ENABLE_MTK_BOARD_ID_CHECK_HX
	if (hw_version != BOARD_HW_REV_00) {
		pr_info("hx83102d : %s : hw board revision	match\n", __func__);
		return 0;
	}

	pr_info("hx83102d : %s : hw board revision and LCM doesn't match.\n", __func__);
	return -EPERM;
#else
	return 0;
#endif
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_info("[LCM][I2C][hx83102d] %s\n", __func__);
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
		pr_debug("ERROR!![hx83102d] _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_debug("[LCM][ERROR][hx83102d] _lcm_i2c write data fail !!\n");

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_info("[LCM][I2C][hx83102d] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_info("[LCM][I2C][hx83102d] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_info("[LCM][I2C][hx83102d] %s\n", __func__);
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

	lcm_dcs_write_seq_static(ctx, 0xB9,0x83, 0x10, 0x2D); /* Set extended command acceess */
	lcm_dcs_write_seq_static(ctx, 0xB1,0x22, 0x44, 0x31, 0x31, 0x22,
		0x34, 0x2F, 0x57, 0x08, 0x08, 0x08, 0x67, 0x67, 0x71); /* Set Power */
	lcm_dcs_write_seq_static(ctx, 0xB2,0x00, 0x00, 0x06, 0x18, 0x00,
		0x0E, 0xFE, 0x40, 0x00, 0x00, 0x00, 0x00, 0xF4, 0xA0); /* Set Display */
	lcm_dcs_write_seq_static(ctx, 0xB4,0x19, 0x59, 0x19, 0x59, 0x19,
		0x59, 0x19, 0x59, 0x06, 0xFF, 0x06, 0x20, 0x00, 0xFF); /* Set panel driving timing */
	lcm_dcs_write_seq_static(ctx, 0xCC,0x02); /* Set Panel type */
	lcm_dcs_write_seq_static(ctx, 0xD3,0x00, 0x00, 0x3C, 0x03, 0x00,
		0x08, 0x00, 0x37, 0x00, 0x33, 0x33, 0x08, 0x08, 0x00, 0x00,
		0x32, 0x10, 0x06, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x05, 0x12, 0x00, 0x00); /* Set GIP Option */
	lcm_dcs_write_seq_static(ctx, 0xD5,0x18, 0x18, 0x18, 0x18, 0x39,
		0x39, 0x18, 0x18, 0x20, 0x21, 0x22, 0x23, 0x19, 0x19, 0x19,
		0x19, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18); /* Set GIP output 1 */
	lcm_dcs_write_seq_static(ctx, 0xD6,0x18, 0x18, 0x19, 0x19, 0x39,
 		0x39, 0x18, 0x18, 0x23, 0x22, 0x21, 0x20, 0x18, 0x18, 0x19,
 		0x19, 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x18,
 		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
 		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18); /* Set GIP output 2 */
	lcm_dcs_write_seq_static(ctx, 0xE7,0xFF, 0x0F, 0x00, 0x00); /* Set TP */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x01); /* Switch to Bank1 */
	lcm_dcs_write_seq_static(ctx, 0xE7,0x01); /* Set TP */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x00); /* Switch to Bank0 */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x02); /* Switch to Bank2 */
	lcm_dcs_write_seq_static(ctx, 0xD8,0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0); /* Set GIP output 3 */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x03); /* Switch to Bank3 */
	lcm_dcs_write_seq_static(ctx, 0xD8,0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
		0xA0, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0xAA, 0xAA, 0xAA,
		0xAA, 0xAA, 0xA0, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0); /* Set GIP output 3 */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x00); /* Switch to Bank0 */
	lcm_dcs_write_seq_static(ctx, 0xE0,0x00, 0x01, 0x06, 0x0A, 0x0F,
		0x11, 0x27, 0x2F, 0x39, 0x39, 0x5C, 0x64, 0x72, 0x82, 0x87,
		0x96, 0xA3, 0xC4, 0xC6, 0x63, 0x6C, 0x77, 0x7F, 0x00, 0x01,
		0x06, 0x0A, 0x0F, 0x11, 0x27, 0x2F, 0x39, 0x39, 0x5C, 0x6E,
		0x7A, 0x98, 0x9B, 0xAA, 0xB7, 0xC4, 0xC6, 0x63, 0x6C, 0x77,
		0x7F); /* Set Gamma */
	lcm_dcs_write_seq_static(ctx, 0xBA,0x70, 0x23, 0xA8, 0x8B, 0xB2,
		0x80, 0x80, 0x01, 0x10, 0x00, 0x00, 0x00, 0x0D, 0x3D, 0x02,
		0x77, 0x04, 0x01, 0x00); /* Set DSI */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x01); /* Switch to Bank1 */
	lcm_dcs_write_seq_static(ctx, 0xCB,0x01); /* Set OSC option */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x00); /* Switch to Bank0 */
	lcm_dcs_write_seq_static(ctx, 0xCB,0x00, 0x53, 0x00, 0x02, 0x6C); /* Set OSC option */
	lcm_dcs_write_seq_static(ctx, 0xBF,0xFC, 0x00, 0x24, 0x9E, 0xF6,
		0x00, 0x5D); /* Set power option */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x02); /* Switch to Bank2 */
	lcm_dcs_write_seq_static(ctx, 0xB4,0x42, 0x00, 0x33, 0x00, 0x33,
		0x88, 0xB3, 0x00);/* Set power option */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x00); /* Switch to Bank0 */
	lcm_dcs_write_seq_static(ctx, 0xD1,0x20, 0x01); /* Set TP option */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x02); /* Switch to Bank2 */
	lcm_dcs_write_seq_static(ctx, 0xB1,0x7F, 0x03, 0xFF); /* Set Power Option */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x00); /* Switch to Bank0 */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x01); /* Switch to Bank1 */
 	lcm_dcs_write_seq_static(ctx, 0xD3,0x01, 0x00, 0x39); /* Set GIP Option 1 */
	lcm_dcs_write_seq_static(ctx, 0xBD,0x00); /* Switch to Bank0 */
	lcm_dcs_write_seq_static(ctx, 0xC9,0x04, 0x65, 0x90, 0x01); /* Set CABC PWM frequency to 2.5KHz */
	lcm_dcs_write_seq_static(ctx, 0x51,0x0F, 0xFF); /* Set PWM duty to 100% */
	lcm_dcs_write_seq_static(ctx, 0x53,0x24); /* Eable PWM out */

	lcm_dcs_write_seq_static(ctx, 0x11, 0x00);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x00);
	msleep(50);
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

#define HFP (105)
#define HSA (12)
#define HBP (26)
#define VFP (254)
#define VSA (2)
#define VBP (12)
#define VAC (1560)
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
	struct device_node *root = of_find_node_by_path("/");
	unsigned int hw_version;
	int ret;

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

#ifdef ENABLE_MTK_BOARD_ID_CHECK_HX
		if (hw_version == BOARD_HW_REV_00) {
		pr_info("[NVT]%s: board ID not match\n", __func__);
		return -1;
	}
#endif

	pr_info("%s-hx83102d IN\n", __func__);
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
	pr_info("%s-hx83102d\n", __func__);

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
	{ .compatible = "auo,hx83102d,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-auo-hx83102d-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Yi-Lun Wang <Yi-Lun.Wang@mediatek.com>");
MODULE_DESCRIPTION("hx83102d auo VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");

