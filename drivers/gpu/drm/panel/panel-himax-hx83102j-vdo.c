// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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

#define ENABLE_MTK_LCD_DTS_CHECK_HIMAX
/*******************************************************/
/* Backlight Function                                  */
/* Device Name : KTD3156                               */
/* Interface   : I2C                                   */
/* Enable Mode : 1 PIN for 5P9, 5N9                    */
/*******************************************************/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#define LCM_I2C_COMPATIBLE	"mediatek,i2c_lcd_bias"
#define LCM_I2C_ID_NAME		"hx83102j_ktd3156"

static struct i2c_client *_lcm_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);
static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value);
static int _lcm_isl98608_i2c_write_bytes(unsigned char addr, unsigned char value);
static int _lcm_isl98608_i2c_read_bytes(unsigned char addr);

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

static void _ktd3156_init(void)
{
	/* Set Vout 5V */
	_lcm_i2c_write_bytes(0x04, 0x78);
}

static void _isl98608_init(void)
{
	/* Set VBST, VSP, VSN 5.8V */
	_lcm_isl98608_i2c_write_bytes(0x06, 0x06);
	_lcm_isl98608_i2c_write_bytes(0x08, 0x06);
	_lcm_isl98608_i2c_write_bytes(0x09, 0x06);
}

/*****************************************************************************
 * I2C Interface Function
 *****************************************************************************/
static int _lcm_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device_node *lcd_comp;
	int ret;

	pr_debug("[LCM][I2C][hx83102j] %s\n", __func__);
	pr_debug("[LCM][I2C][hx83102j] HX: info==>name=%s addr=0x%x\n", client->name,
		 client->addr);
	_lcm_i2c_client = client;

#ifdef ENABLE_MTK_LCD_DTS_CHECK_HIMAX
	lcd_comp = of_find_compatible_node(NULL, NULL, "himax,hx83102j,vdo");
	if (lcd_comp) {
		pr_info("hx83102j : %s : panel compatible match\n", __func__);
		return 0;
	}

	pr_info("hx83102j : %s : panel compatible doesn't match\n", __func__);
	return -EPERM;
#else
	return 0;
#endif
}

static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_info("[LCM][I2C][hx83102j] %s\n", __func__);
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
		pr_debug("ERROR!![hx83102j] _lcm_i2c_client is null\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_debug("[LCM][ERROR][hx83102j] _lcm_i2c write data fail !!\n");

	return ret;
}

static int _lcm_isl98608_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };
	unsigned char i2c_addr;

	if (client == NULL) {
		pr_debug("ERROR!![hx83102j] _lcm_i2c_client is null\n");
		return 0;
	}

	i2c_addr = client->addr;
	client->addr = 0x29;

	write_data[0] = addr;
	write_data[1] = value;

	pr_info("[LCM][ERROR][hx83102j] isl98608_i2c [0x%x] W(0x%x):0x%x\n"
						, client->addr, write_data[0], write_data[1]);
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_debug("[LCM][ERROR][hx83102j] _lcm_isl98608_i2c write data fail !!\n");

	client->addr = i2c_addr;

	return ret;
}

static int _lcm_isl98608_i2c_read_bytes(unsigned char addr)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char comm_data[2] = { 0 };
	unsigned char i2c_addr;

	if (client == NULL) {
		pr_debug("ERROR!![hx83102j] _lcm_i2c_client is null\n");
		return 0;
	}

	i2c_addr = client->addr;
	client->addr = 0x29;

	comm_data[0] = addr;

	ret = i2c_master_send(client, &comm_data[0], 1);
	if (ret < 0)
		pr_debug("[LCM][ERROR][hx83102j] _lcm_isl98608_i2c write data fail !!\n");

	ret = i2c_master_recv(client, &comm_data[1], 1);
	if (ret < 0)
		pr_debug("[LCM][ERROR][hx83102j] _lcm_isl98608_i2c write data fail !!\n");

	pr_info("[LCM][ERROR][hx83102j] isl98608_i2c [0x%x] R(0x%x):0x%x\n"
							, client->addr, comm_data[0], comm_data[1]);
	client->addr = i2c_addr;

	return ret;
}

/*
 * module load/unload record keeping
 */
static int __init _lcm_i2c_init(void)
{
	pr_info("[LCM][I2C][hx83102j] %s\n", __func__);
	i2c_add_driver(&_lcm_i2c_driver);
	pr_info("[LCM][I2C][hx83102j] %s success\n", __func__);
	return 0;
}

static void __exit _lcm_i2c_exit(void)
{
	pr_info("[LCM][I2C][hx83102j] %s\n", __func__);
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
	struct gpio_desc *bl_en_gpio;
	struct gpio_desc *tsp_rst_gpio;
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
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}

	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(3 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(3 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_dcs_write_seq_static(ctx, 0xB9, 0x83, 0x10, 0x21, 0x55, 0x00); /* Password */
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x2C, 0xB3, 0xB3, 0x2F, 0xEF,
		0x42, 0xE1, 0x4D, 0x36, 0x36, 0x36, 0x36); /* Power */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xD9); /* VSPR/VSNR HiZ in TPEN */
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x9A, 0x33); /* VSPR/VSNR HiZ in TPEN */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F); /* VSPR/VSNR HiZ in TPEN */
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x00, 0x59, 0xA0, 0x00, 0x00,
		0x12, 0x5E, 0x3C, 0x84, 0x22); /* Display setting */
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x4B, 0x4B, 0x50, 0x50, 0x50,
		0x50, 0x06, 0x74, 0x06, 0x74, 0x01, 0x8A); /* GIP ON/OFF time */
	lcm_dcs_write_seq_static(ctx, 0xBC, 0x1B, 0x04); /* VDDD */
	lcm_dcs_write_seq_static(ctx, 0xBE, 0x20); /* SETDEBUG */
	lcm_dcs_write_seq_static(ctx, 0xBF, 0xFC, 0x84); /* PTBA */
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x32, 0x32, 0x22, 0x11, 0xA2,
		0xA0, 0x61, 0x08, 0xF5, 0x03); /* SAP */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xCC); /* TCON_OPT */
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x80); /* TCON_OPT */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F); /* TCON_OPT */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xC6); /* Pattern detect */
	lcm_dcs_write_seq_static(ctx, 0xC8, 0x97); /* Pattern detect */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F); /* Pattern detect */
	lcm_dcs_write_seq_static(ctx, 0x51, 0x0F, 0xFF); /* PWM duty 100% */
	lcm_dcs_write_seq_static(ctx, 0x53, 0x24); /* PWM dimming off */
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x00, 0x1E, 0x0F, 0xA0, 0x01); /* PWM 25KHz */
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x08, 0x13, 0x07, 0x00, 0x0C,
		0xE9); /* OSC tracking */
	lcm_dcs_write_seq_static(ctx, 0xCC, 0x02); /* Source shift direction */
	lcm_dcs_write_seq_static(ctx, 0xD1, 0x37, 0x06, 0x00, 0x02, 0x04,
		0x0C, 0xFF); /* TP_CTRL */
	lcm_dcs_write_seq_static(ctx, 0xD3, 0x06, 0x00, 0x00, 0x00, 0x00,
		0x04, 0x08, 0x04, 0x08, 0x27, 0x47, 0x64, 0x4B, 0x12, 0x12,
		0x03, 0x03, 0x32, 0x10, 0x0D, 0x00, 0x0D, 0x32, 0x10, 0x07,
		0x00, 0x07, 0x32, 0x19, 0x18, 0x09, 0x18, 0x00, 0x00); /* GIP CHR/SHR */
	lcm_dcs_write_seq_static(ctx, 0xD5, 0x24, 0x24, 0x24, 0x24, 0x07,
		0x06, 0x07, 0x06, 0x05, 0x04, 0x05, 0x04, 0x03, 0x02, 0x03,
		0x02, 0x01, 0x00, 0x01, 0x00, 0x21, 0x20, 0x21, 0x20, 0x18,
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F, 0x1F, 0x1F,
		0x1F, 0x1E, 0x1E, 0x1E, 0x1E, 0x18, 0x18, 0x18, 0x18); /* GIP forward scan */
	lcm_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xAA, 0xAA, 0xFA,
		0xA0, 0xAA, 0xAA, 0xAA, 0xAA, 0xFA, 0xA0); /* Initial frame setting */
	lcm_dcs_write_seq_static(ctx, 0xE0, 0x00, 0x04, 0x0C, 0x13, 0x1A,
		0x2D, 0x48, 0x52, 0x5E, 0x5D, 0x7C, 0x85, 0x8D, 0x9E, 0x9C,
		0xA4, 0xAC, 0xBD, 0xBA, 0x5A, 0x61, 0x6A, 0x75, 0x00, 0x04,
		0x0C, 0x13, 0x1A, 0x2D, 0x48, 0x52, 0x5E, 0x5D, 0x7C, 0x85,
		0x8D, 0x9E, 0x9C, 0xA4, 0xAC, 0xBD, 0xBA, 0x5A, 0x61, 0x6A,
		0x75); /* gamma test-jim */
	lcm_dcs_write_seq_static(ctx, 0xE7, 0x15, 0x0E, 0x0E, 0x28, 0x23,
		0x8D, 0x00, 0x20, 0x8D, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00,
		0x12, 0x05, 0x02, 0x02, 0x0E); /* Long-H setting */
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x01); /* Bank1 */
	lcm_dcs_write_seq_static(ctx, 0xD2, 0xB4); /* SETGIP */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xC9); /* GIP option */
	lcm_dcs_write_seq_static(ctx, 0xD3, 0x84); /* GIP option */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F); /* GIP option */
	lcm_dcs_write_seq_static(ctx, 0xD8, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF,
		0xA0, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF, 0xA0); /* End frame setting */
	lcm_dcs_write_seq_static(ctx, 0xE7, 0x02, 0x00, 0x9C, 0x01, 0xA6,
		0x0D, 0xAA, 0x0E, 0xA0, 0x00, 0x00); /* Long-H setting */
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x02); /* Bank2 */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xC4); /* CPHY IHSRB */
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x8A); /* CPHY IHSRB */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F); /* CPHY IHSRB */
	lcm_dcs_write_seq_static(ctx, 0xD8, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF,
		0xA0, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF, 0xA0); /* GIP GAS */
	lcm_dcs_write_seq_static(ctx, 0xE7, 0xFF, 0x01, 0xFF, 0x01, 0xFF,
		0x01, 0x00, 0x00, 0x00, 0x22, 0x00, 0x22, 0x81, 0x02, 0x40,
		0x00, 0x20, 0x8A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x01, 0x00); /* TP Rescan */
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x03); /* Bank3 */
	lcm_dcs_write_seq_static(ctx, 0xD8, 0xAA, 0xAA, 0xAA, 0xAA, 0xFA,
		0xA0, 0xAA, 0xAA, 0xAA, 0xAA, 0xFA, 0xA0, 0xFF, 0xFF, 0xFA,
		0xAF, 0xFF, 0xA0, 0xFF, 0xFF, 0xFA, 0xAF, 0xFF, 0xA0, 0x55,
		0x55, 0x55, 0x55, 0x55, 0x50, 0x55, 0x55, 0x55, 0x55, 0x55,
		0x50); /* LPWG/DCHG */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0xC5); /* CPHY EQ/MASK optimized */
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x4F, 0x70); /* CPHY EQ/MASK optimized */
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x3F); /* CPHY EQ/MASK optimized */
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x00); /* Bank0 */
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x00, 0x00); /* Closed Password */

	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	msleep(40);
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

	pr_info("%s\n", __func__);

	if (!ctx->prepared)
		return 0;

	ctx->error = 0;
	ctx->prepared = false;

	/* LCD BL OFF */
	ctx->bl_en_gpio =
		devm_gpiod_get(ctx->dev, "backlight", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bl_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get bl_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->bl_en_gpio));
		return PTR_ERR(ctx->bl_en_gpio);
	}

	gpiod_set_value(ctx->bl_en_gpio, 0);
	udelay(1000);
	devm_gpiod_put(ctx->dev, ctx->bl_en_gpio);

	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x10);
	mdelay(100);


	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	udelay(1000);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	udelay(1000);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	/* TOUCH RST OFF */
	ctx->tsp_rst_gpio =
		devm_gpiod_get(ctx->dev, "tsprst", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->tsp_rst_gpio)) {
		dev_info(ctx->dev, "%s: cannot get tsp_rst_gpio %ld\n",
			__func__, PTR_ERR(ctx->tsp_rst_gpio));
		return PTR_ERR(ctx->tsp_rst_gpio);
	}

	gpiod_set_value(ctx->tsp_rst_gpio, 0);
	udelay(1000);
	devm_gpiod_put(ctx->dev, ctx->tsp_rst_gpio);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	/* TOUCH RST */
	ctx->tsp_rst_gpio = devm_gpiod_get(ctx->dev, "tsprst", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->tsp_rst_gpio)) {
		dev_info(ctx->dev, "%s: cannot get tsp_rst_gpio %ld\n",
			__func__, PTR_ERR(ctx->tsp_rst_gpio));
		return PTR_ERR(ctx->tsp_rst_gpio);
	}

	gpiod_set_value(ctx->tsp_rst_gpio, 1);
	udelay(5 * 1000);
	devm_gpiod_put(ctx->dev, ctx->tsp_rst_gpio);

	/* LCD BL ON */
	ctx->bl_en_gpio = devm_gpiod_get(ctx->dev, "backlight", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bl_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get bl_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->bl_en_gpio));
		return PTR_ERR(ctx->bl_en_gpio);
	}

	gpiod_set_value(ctx->bl_en_gpio, 1);
	udelay(5 * 1000);
	devm_gpiod_put(ctx->dev, ctx->bl_en_gpio);

	_ktd3156_init();

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(3 * 1000);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);


	_isl98608_init();

	mdelay(3);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	lcm_panel_init(ctx);

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

#define HFP (60)
#define HSA (20)
#define HBP (40)
#define VFP (96)
#define VSA (8)
#define VBP (12)
#define VAC (2304)
#define HAC (1440)
static u32 fake_heigh = VAC;
static u32 fake_width = HAC;
static bool need_fake_resolution;

static struct drm_display_mode default_mode = {
	.clock = 226512,	/*2420*1560*60/1000*/
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

static char bl_tb0[] = {0x51, 0x0F, 0xFF};
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	unsigned int mapping_level;
	char bank0_cmd[] = { 0xbd, 0x00 };
	char close_cmd[] = { 0xb9, 0x00, 0x00, 0x00 };
	// char test1[] = { 0xC9, 0x00, 0x1E, 0x0F, 0xA0, 0x01 };  /* PWM 25KHz */

	if(level > 255 )
		level = 255;

	mapping_level = level * 4095 / 255;
	bl_tb0[1] = ((mapping_level >> 8) & 0xf);
	bl_tb0[2] = (mapping_level & 0xff);

	pr_info("lcm_setbacklight_cmdq bl_tb0 (0x%x): 0x%x, 0x%x, 0x%x\n"
					, mapping_level, bl_tb0[0], bl_tb0[1], bl_tb0[2]);

	if (!cb)
		return -1;

	cb(dsi, handle, bank0_cmd, ARRAY_SIZE(bank0_cmd));
	cb(dsi, handle, close_cmd, ARRAY_SIZE(close_cmd));
	// cb(dsi, handle, test1, ARRAY_SIZE(test1));
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
	.pll_clk = 437,//445,//398,
	.vfp_low_power = 384,//96,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 1,
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
	struct device_node *lcd_comp;
	int lcd_id_pins = 0;
	int ret;

#ifdef ENABLE_MTK_LCD_DTS_CHECK_HIMAX
	lcd_comp = of_find_compatible_node(NULL, NULL, "himax,hx83102j,vdo");
	if (!lcd_comp) {
		pr_info("hx83102j : %s : panel compatible doesn't match\n", __func__);
		return -1;
	}
#endif

	pr_info("%s-hx83102j IN\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
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
	pr_info("%s-hx83102j\n", __func__);

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
	{ .compatible = "himax,hx83102j,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-himax-hx83102j-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Yi-Lun Wang <Yi-Lun.Wang@mediatek.com>");
MODULE_DESCRIPTION("hx83102j himax VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");

