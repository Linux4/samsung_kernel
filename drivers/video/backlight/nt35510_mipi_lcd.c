/* linux/drivers/video/backlight/nt35510_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/backlight.h>

#include <video/mipi_display.h>
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>

#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 130

#if defined(CONFIG_FB_I80_COMMAND_MODE)
#define BOE_WVGA_GAMMA_CTR1 \
0x00, 0x01, 0x00, 0x43, 0x00, \
0x6B, 0x00, 0x87, 0x00, 0xA3, \
0x00, 0xCE, 0x00, 0xF1, 0x01, \
0x27, 0x01, 0x53, 0x01, 0x98, \
0x01, 0xCE, 0x02, 0x22, 0x02, \
0x83, 0x02, 0x78, 0x02, 0x9E, \
0x02, 0xDD, 0x03, 0x00, 0x03, \
0x2E, 0x03, 0x54, 0x03, 0x7F, \
0x03, 0x95, 0x03, 0xB3, 0x03, \
0xC2, 0x03, 0xE1, 0x03, 0xF1, \
0x03, 0xFE
#define BOE_WVGA_GAMMA_CTR2 \
0x00, 0x01, 0x00, 0x43, 0x00, \
0x6B, 0x00, 0x87, 0x00, 0xA3, \
0x00, 0xCE, 0x00, 0xF1, 0x01, \
0x27, 0x01, 0x53, 0x01, 0x98, \
0x01, 0xCE, 0x02, 0x22, 0x02, \
0x43, 0x02, 0x50, 0x02, 0x9E, \
0x02, 0xDD, 0x03, 0x00, 0x03, \
0x2E, 0x03, 0x54, 0x03, 0x7F, \
0x03, 0x95, 0x03, 0xB3, 0x03, \
0xC2, 0x03, 0xE1, 0x03, 0xF1, \
0x03, 0xFE
#else
#define BOE_WVGA_GAMMA_CTR1 \
0x00, 0x37, 0x00, 0x51, 0x00, \
0x71, 0x00, 0x96, 0x00, 0xAA, \
0x00, 0xD3, 0x00, 0xF0, 0x01, \
0x1D, 0x01, 0x45, 0x01, 0x84, \
0x01, 0xB5, 0x02, 0x02, 0x02, \
0x46, 0x02, 0x48, 0x02, 0x80, \
0x02, 0xC0, 0x02, 0xE8, 0x03, \
0x14, 0x03, 0x32, 0x03, 0x5D, \
0x03, 0x73, 0x03, 0x91, 0x03, \
0xA0, 0x03, 0xBF, 0x03, 0xCF, \
0x03, 0xEF

#define BOE_WVGA_GAMMA_CTR2 \
0x00, 0x37, 0x00, 0x51, 0x00, \
0x71, 0x00, 0x96, 0x00, 0xAA, \
0x00, 0xD3, 0x00, 0xF0, 0x01, \
0x1D, 0x01, 0x45, 0x01, 0x84, \
0x01, 0xB5, 0x02, 0x02, 0x02, \
0x46, 0x02, 0x48, 0x02, 0x80, \
0x02, 0xC0, 0x02, 0xE8, 0x03, \
0x14, 0x03, 0x32, 0x03, 0x5D, \
0x03, 0x73, 0x03, 0x91, 0x03, \
0xA0, 0x03, 0xBF, 0x03, 0xCF, \
0x03, 0xEF
#endif
struct lcd_info {
	unsigned int			bl;
	unsigned int			current_bl;
	struct backlight_device		*bd;
	unsigned int			ldi_enable;
	struct mutex                    bl_lock;
};
static struct lcd_info *g_lcd;

static int update_brightness(struct lcd_info *lcd, u8 force)
{
	mutex_lock(&lcd->bl_lock);

	lcd->bl = ((32 * lcd->bd->props.brightness * 210) / MAX_BRIGHTNESS) / MAX_BRIGHTNESS;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {

		/* Control PWM */
		unsigned int gpio = EXYNOS4_GPD0(1);
		int cnt;

		gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, "GPD0");
		gpio_set_value(gpio, 0);
		mdelay(2);

		for (cnt = lcd->bl; cnt < 32; cnt ++) {
			gpio_set_value(gpio, 1);
			udelay(1);
			gpio_set_value(gpio, 0);
			udelay(1);
		}

		gpio_set_value(gpio, 1);
		gpio_free(gpio);

		lcd->current_bl = lcd->bl;
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int ktd283_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	if (brightness < MIN_BRIGHTNESS ||
			brightness > bd->props.max_brightness) {
		printk(KERN_ERR "lcd brightness should be %d to %d. now %d\n",
				MIN_BRIGHTNESS, MAX_BRIGHTNESS, brightness);
		return -EINVAL;
	}

	ret = update_brightness(lcd, lcd->ldi_enable);
	if (ret < 0) {
		printk(KERN_ERR "err in %s\n", __func__);
		return -EINVAL;
	}

	return ret;
}

static int ktd283_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return lcd->current_bl;
}

static const struct backlight_ops ktd283_backlight_ops  = {
	.get_brightness = ktd283_get_brightness,
	.update_status = ktd283_set_brightness,
};

int nt35510_probe(struct mipi_dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	g_lcd = lcd;

	lcd->bd = backlight_device_register("pwm-backlight.0", dsim->dev,
			lcd, &ktd283_backlight_ops, NULL);
	if (IS_ERR(lcd->bd))
		dev_err(dsim->dev, "failed to register backlight device\n");

	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = 0;
	lcd->current_bl = lcd->bl;
	lcd->ldi_enable = 1;

	mutex_init(&lcd->bl_lock);

	return 0;

err_alloc:
	return ret;
}

void init_lcd(struct mipi_dsim_device *dsim)
{
#if defined(CONFIG_FB_I80_COMMAND_MODE)
	unsigned char sleepout[] = {0x11};
	/* Power Sequence */
	unsigned char initcode_0[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01};
	unsigned char initcode_1[] = {0xB0, 0x09, 0x09, 0x09};
	unsigned char initcode_2[] = {0xB6, 0x34, 0x34, 0x34};
	unsigned char initcode_3[] = {0xB1, 0x09, 0x09, 0x09};
	unsigned char initcode_4[] = {0xB7, 0x24, 0x24, 0x24};
	unsigned char initcode_5[] = {0xB3, 0x05, 0x05, 0x05};
	unsigned char initcode_6[] = {0xB9, 0x24, 0x24, 0x24};
	unsigned char initcode_7[] = {0xBF, 0x01};
	unsigned char initcode_8[] = {0xB5, 0x0b, 0x0b, 0x0b};
	unsigned char initcode_9[] = {0xBA, 0x24, 0x24, 0x24};
	unsigned char initcode_10[] = {0xBC, 0x00, 0xA3, 0x00};
	unsigned char initcode_11[] = {0xBD, 0x00, 0xA3, 0x00};

	/* Display Parameter */
	unsigned char initcode2_0[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	unsigned char initcode2_1[] = {0xB1, 0x4C, 0x04};
	unsigned char initcode2_2[] = {0x36, 0x02};
	unsigned char initcode2_3[] = {0xB6, 0x0A};
	unsigned char initcode2_4[] = {0xB7, 0x00, 0x00};
	unsigned char initcode2_5[] = {0xB8, 0x01, 0x05, 0x05, 0x05};
	unsigned char initcode2_6[] = {0xBA, 0x01};
	unsigned char initcode2_7[] = {0xBD, 0x01, 0x84, 0x07, 0x32, 0x00};
	unsigned char initcode2_8[] = {0xBE, 0x01, 0x84, 0x07, 0x31, 0x00};
	unsigned char initcode2_9[] = {0xBF, 0x01, 0x84, 0x07, 0x31, 0x00};
	unsigned char initcode2_11[] = {0xCC, 0x03, 0x00, 0x00};

	unsigned char te_on[] = {0x35};
	/* Gamma Setting Seguece */
	unsigned char initcode_d1[] = {0xD1, BOE_WVGA_GAMMA_CTR1};
	unsigned char initcode_d2[] = {0xD2, BOE_WVGA_GAMMA_CTR1};
	unsigned char initcode_d3[] = {0xD3, BOE_WVGA_GAMMA_CTR1};
	unsigned char initcode_d4[] = {0xD4, BOE_WVGA_GAMMA_CTR2};
	unsigned char initcode_d5[] = {0xD5, BOE_WVGA_GAMMA_CTR2};
	unsigned char initcode_d6[] = {0xD6, BOE_WVGA_GAMMA_CTR2};

	unsigned char displayon[] = {0x29};

	/* sleep out */
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
				sleepout,
				ARRAY_SIZE(sleepout));
	msleep(120);

	/* power setting */
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_0,
				ARRAY_SIZE(initcode_0)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD0\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_1,
				ARRAY_SIZE(initcode_1)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD1\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_2,
				ARRAY_SIZE(initcode_2)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD2\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_3,
				ARRAY_SIZE(initcode_3)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD3\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_4,
				ARRAY_SIZE(initcode_4)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD4\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_5,
				ARRAY_SIZE(initcode_5)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD5\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_6,
				ARRAY_SIZE(initcode_6)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD6\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				initcode_7,
				ARRAY_SIZE(initcode_7));

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_8,
				ARRAY_SIZE(initcode_8)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD8\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_9,
				ARRAY_SIZE(initcode_9)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD9\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_10,
				ARRAY_SIZE(initcode_10)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD10\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_11,
				ARRAY_SIZE(initcode_11)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD11\n");
	msleep(120);

	/* display parameter */
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_0,
				ARRAY_SIZE(initcode2_0)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD2_0\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_1,
				ARRAY_SIZE(initcode2_1)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD2_1\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				initcode2_2,
				ARRAY_SIZE(initcode2_2));

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				initcode2_3,
				ARRAY_SIZE(initcode2_3));

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_4,
				ARRAY_SIZE(initcode2_4)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD2_4\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_5,
				ARRAY_SIZE(initcode2_5)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD2_5\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				initcode2_6,
				ARRAY_SIZE(initcode2_6));

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_7,
				ARRAY_SIZE(initcode2_7)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD2_7\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_8,
				ARRAY_SIZE(initcode2_8)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD2_8\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_9,
				ARRAY_SIZE(initcode2_9)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD2_9\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
				te_on,
				ARRAY_SIZE(te_on));

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_11,
				ARRAY_SIZE(initcode2_11)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD2_11\n");

	/* gamma setting */
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d1,
				ARRAY_SIZE(initcode_d1)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMDd_1\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d2,
				ARRAY_SIZE(initcode_d2)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMDd_2\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d3,
				ARRAY_SIZE(initcode_d3)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMDd_3\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d4,
				ARRAY_SIZE(initcode_d4)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMDd_4\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d5,
				ARRAY_SIZE(initcode_d5)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMDd_5\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d6,
				ARRAY_SIZE(initcode_d6)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMDd_6\n");
	/* display on */
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
				displayon,
				ARRAY_SIZE(displayon));

	msleep(10);
#else
	unsigned char sleepout[] = {0x11};

	/* Power Sequence */
	unsigned char initcode_0[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01};
	unsigned char initcode_1[] = {0xB0, 0x09, 0x09, 0x09};
	unsigned char initcode_2[] = {0xB6, 0x34, 0x34, 0x34};
	unsigned char initcode_3[] = {0xB1, 0x09, 0x09, 0x09};
	unsigned char initcode_4[] = {0xB7, 0x24, 0x24, 0x24};
	unsigned char initcode_5[] = {0xB3, 0x05, 0x05, 0x05};
	unsigned char initcode_6[] = {0xB9, 0x24, 0x24, 0x24};
	unsigned char initcode_7[] = {0xBF, 0x01};
	unsigned char initcode_8[] = {0xB5, 0x0b, 0x0b, 0x0b};
	unsigned char initcode_9[] = {0xBA, 0x24, 0x24, 0x24};
	unsigned char initcode_10[] = {0xC2, 0x01};
	unsigned char initcode_11[] = {0xBC, 0x00, 0x90, 0x00};
	unsigned char initcode_12[] = {0xBD, 0x00, 0x90, 0x00};

	/* Display Parameter */
	unsigned char initcode2_0[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	unsigned char initcode2_1[] = {0xB6, 0x06};
	unsigned char initcode2_2[] = {0xB7, 0x00, 0x00};
	unsigned char initcode2_3[] = {0xB8, 0x01, 0x05, 0x05, 0x05};
	unsigned char initcode2_4[] = {0xBA, 0x01};
	unsigned char initcode2_5[] = {0xBC, 0x00, 0x00, 0x0};
	unsigned char initcode2_6[] = {0xBD, 0x01, 0x84, 0x07, 0x32, 0x00};
	unsigned char initcode2_7[] = {0xBE, 0x01, 0x84, 0x07, 0x31, 0x00};
	unsigned char initcode2_8[] = {0xBF, 0x01, 0x84, 0x07, 0x31, 0x00};
	unsigned char initcode2_9[] = {0xCC, 0x03, 0x00, 0x00};
	unsigned char initcode2_10[] = {0xB1, 0xF8, 0x06};
	unsigned char initcode2_11[] = {0x36, 0x08};

	unsigned char initcode3_0[] = {0xF0, 0x55, 0xAA, 0x52, 0x00, 0x00};

	/* Gamma Setting Seguece */
	unsigned char initcode_d1[] = {0xD1, BOE_WVGA_GAMMA_CTR1};
	unsigned char initcode_d2[] = {0xD2, BOE_WVGA_GAMMA_CTR1};
	unsigned char initcode_d3[] = {0xD3, BOE_WVGA_GAMMA_CTR1};
	unsigned char initcode_d4[] = {0xD4, BOE_WVGA_GAMMA_CTR2};
	unsigned char initcode_d5[] = {0xD5, BOE_WVGA_GAMMA_CTR2};
	unsigned char initcode_d6[] = {0xD6, BOE_WVGA_GAMMA_CTR2};

	unsigned char displayon[] = {0x29};

	/* sleep out */
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				sleepout,
				ARRAY_SIZE(sleepout)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write SLEEP_OUT CMD\n");
	msleep(120);

	/* power setting */
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_0,
				ARRAY_SIZE(initcode_0)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_0\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_1,
				ARRAY_SIZE(initcode_1)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_1\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_2,
				ARRAY_SIZE(initcode_2)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_3,
				ARRAY_SIZE(initcode_3)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_3\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_4,
				ARRAY_SIZE(initcode_4)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_4\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_5,
				ARRAY_SIZE(initcode_5)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_5\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_6,
				ARRAY_SIZE(initcode_6)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_6\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_7,
				ARRAY_SIZE(initcode_7)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_7\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_8,
				ARRAY_SIZE(initcode_8)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_8\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_9,
				ARRAY_SIZE(initcode_9)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_9\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_10,
				ARRAY_SIZE(initcode_10)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_10\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_11,
				ARRAY_SIZE(initcode_11)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_11\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_12,
				ARRAY_SIZE(initcode_12)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_12\n");
	msleep(120);

	/* gamma setting */
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d1,
				ARRAY_SIZE(initcode_d1)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_d1\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d2,
				ARRAY_SIZE(initcode_d2)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_d2\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d3,
				ARRAY_SIZE(initcode_d3)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_d3\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d4,
				ARRAY_SIZE(initcode_d4)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_d4\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d5,
				ARRAY_SIZE(initcode_d5)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_d5\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode_d6,
				ARRAY_SIZE(initcode_d6)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_d6\n");

	/* display parameter */
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_0,
				ARRAY_SIZE(initcode2_0)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_0\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_1,
				ARRAY_SIZE(initcode2_1)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_1\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_2,
				ARRAY_SIZE(initcode2_2)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_2\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_3,
				ARRAY_SIZE(initcode2_3)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_3\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_4,
				ARRAY_SIZE(initcode2_4)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_4\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_5,
				ARRAY_SIZE(initcode2_5)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_5\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_6,
				ARRAY_SIZE(initcode2_6)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_6\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_7,
				ARRAY_SIZE(initcode2_7)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_7\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_8,
				ARRAY_SIZE(initcode2_8)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_8\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_9,
				ARRAY_SIZE(initcode2_9)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_9\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_10,
				ARRAY_SIZE(initcode2_10)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_10\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode2_11,
				ARRAY_SIZE(initcode2_11)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_2_11\n");

	/* seq disable cmd */
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				initcode3_0,
				ARRAY_SIZE(initcode3_0)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write CMD_3_0\n");

	/* display on */
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				displayon,
				ARRAY_SIZE(displayon)) == -1)
	dev_err(dsim->dev, "MIPI DSI failed to write DISPLAY_ON CMD\n");
	msleep(10);
#endif
}

int nt35510_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);

	return 1;
}

int nt35510_suspend(struct mipi_dsim_device *dsim)
{
	unsigned char displayoff[] = {0x28};
	unsigned char sleepin[] = {0x10};
	struct lcd_info *lcd = g_lcd;
	int ret = 0;

	lcd->ldi_enable = 0;
	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)displayoff, ARRAY_SIZE(displayoff));
	if (ret)
		printk("MIPI DSI failed to write display off CMD, ret = %d\n", ret);

	msleep(10);
	ret = s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned int)sleepin, ARRAY_SIZE(sleepin));
	if (ret)
		printk("MIPI DSI failed to write sleepin CMD, ret = %d\n", ret);

	msleep(120);

	return 1;
}

int nt35510_resume(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	lcd->ldi_enable = 1;

	return 1;
}

struct mipi_dsim_lcd_driver nt35510_mipi_lcd_driver = {
	.probe		= nt35510_probe,
	.displayon	= nt35510_displayon,
	.suspend	= nt35510_suspend,
	.resume		= nt35510_resume,
};
