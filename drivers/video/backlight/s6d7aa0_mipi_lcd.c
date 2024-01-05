/* linux/drivers/video/backlight/s6d7aa0_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>

#include <plat/dsim.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <plat/mipi_dsi.h>

#include "s6d7aa0_gamma.h"

#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

static struct mipi_dsim_device *dsim_base;
static struct backlight_device *bd;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    s6d7aa0_early_suspend;
#endif

u8 aInitCode_F0[] = {0xF0, 0x5A, 0x5A};
u8 aInitCode_F1[] = {0xF1, 0x5A, 0x5A};
u8 aInitCode_FC[] = {0xFC, 0xA5, 0xA5};

u8 aInitCode_D0[] = {0xD0, 0x00, 0x10};
u8 aInitCode_B6[] = {0xB6, 0x10};

u8 aInitCode_C3[] = {0xC3, 0x40, 0x00, 0x28};

u8 aInitCode_BC[] = {0xBC, 0x00, 0x4E, 0xA2};
u8 aInitCode_FD[] = {0xFD, 0x16, 0x10, 0x11, 0x23};
u8 aInitCode_FE[] = {0xFE, 0x00, 0x02, 0x03, 0x21, 0x00, 0x70};
u8 aInitCode_RB3[] = {0xB3, 0x51};
u8 aInitCode_R53[] = {0x53, 0x2C};
u8 aInitCode_R51[] = {0x51, 0x5F};

u8 aExitSleep1[] = {0x11};

u8 aInitCode_F0_1[] = {0xF0, 0xA5, 0xA5};
u8 aInitCode_F1_1[] = {0xF1, 0xA5, 0xA5};
u8 aInitCode_FC_1[] = {0xFC, 0x5A, 0x5A};
u8 aDispalyOn[] = {0x29};

int s6d7aa0_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int get_backlight_level(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0:
		backlightlevel = 0;
		break;
	case 1 ... 29:
		backlightlevel = 0;
		break;
	case 30 ... 34:
		backlightlevel = 1;
		break;
	case 35 ... 39:
		backlightlevel = 2;
		break;
	case 40 ... 44:
		backlightlevel = 3;
		break;
	case 45 ... 49:
		backlightlevel = 4;
		break;
	case 50 ... 54:
		backlightlevel = 5;
		break;
	case 55 ... 64:
		backlightlevel = 6;
		break;
	case 65 ... 74:
		backlightlevel = 7;
		break;
	case 75 ... 83:
		backlightlevel = 8;
		break;
	case 84 ... 93:
		backlightlevel = 9;
		break;
	case 94 ... 103:
		backlightlevel = 10;
		break;
	case 104 ... 113:
		backlightlevel = 11;
		break;
	case 114 ... 122:
		backlightlevel = 12;
		break;
	case 123 ... 132:
		backlightlevel = 13;
		break;
	case 133 ... 142:
		backlightlevel = 14;
		break;
	case 143 ... 152:
		backlightlevel = 15;
		break;
	case 153 ... 162:
		backlightlevel = 16;
		break;
	case 163 ... 171:
		backlightlevel = 17;
		break;
	case 172 ... 181:
		backlightlevel = 18;
		break;
	case 182 ... 191:
		backlightlevel = 19;
		break;
	case 192 ... 201:
		backlightlevel = 20;
		break;
	case 202 ... 210:
		backlightlevel = 21;
		break;
	case 211 ... 220:
		backlightlevel = 22;
		break;
	case 221 ... 230:
		backlightlevel = 23;
		break;
	case 231 ... 240:
		backlightlevel = 24;
		break;
	case 241 ... 250:
		backlightlevel = 25;
		break;
	case 251 ... 255:
		backlightlevel = 26;
		break;
	default:
		backlightlevel = 12;
		break;
	}

	return backlightlevel;
}

static int update_brightness(int brightness)
{
	int backlightlevel;

	backlightlevel = get_backlight_level(brightness);
	if (!(soc_is_exynos5260())) {
	while (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			gamma22_table[backlightlevel],
				GAMMA_PARAM_SIZE) == -1)
		printk(KERN_ERR "fail to write gamma value.\n");
	}
	return 1;
}

int s6d7aa0_set_brightness(struct backlight_device *bd)
{
	int brightness;
	brightness = bd->props.brightness;

	return 1;
	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(brightness);

	return 1;
}

static const struct backlight_ops s6d7aa0_backlight_ops = {
	.get_brightness = s6d7aa0_get_brightness,
	.update_status = s6d7aa0_set_brightness,
};

int s6d7aa0_probe(struct mipi_dsim_device *dsim)
{
	dsim_base = dsim;

	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &s6d7aa0_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 1;
}

static void init_lcd(struct mipi_dsim_device *dsim)
{
	printk("@@@ Start to send LCD init commands\n");
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_F0,
			ARRAY_SIZE(aInitCode_F0)) == -1)
	dev_err(dsim->dev, "LCD init command 1 failed\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_F1,
			ARRAY_SIZE(aInitCode_F1)) == -1)
	dev_err(dsim->dev, "LCD init command 2 failed\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_FC,
			ARRAY_SIZE(aInitCode_FC)) == -1)
	dev_err(dsim->dev, "LCD init command 3 failed\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_D0,
			ARRAY_SIZE(aInitCode_D0)) == -1)
	dev_err(dsim->dev, "LCD init command D0 failed\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		aInitCode_B6, 2);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_C3,
			ARRAY_SIZE(aInitCode_C3)) == -1)
	dev_err(dsim->dev, "LCD init command 4 failed\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_BC,
			ARRAY_SIZE(aInitCode_BC)) == -1)
	dev_err(dsim->dev, "LCD init command BC failed\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_FD,
			ARRAY_SIZE(aInitCode_FD)) == -1)
	dev_err(dsim->dev, "LCD init command FD failed\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_FE,
			ARRAY_SIZE(aInitCode_FE)) == -1)
	dev_err(dsim->dev, "LCD init command FE failed\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			aInitCode_RB3, 2);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			aInitCode_R53, 2);
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			aInitCode_R51, 2);
	msleep(5);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		aExitSleep1, 1);
	msleep(120);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_F0_1,
			ARRAY_SIZE(aInitCode_F0_1)) == -1)
	dev_err(dsim->dev, "LCD init command 5 failed\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_F1_1,
			ARRAY_SIZE(aInitCode_F1_1)) == -1)
	dev_err(dsim->dev, "LCD init command 6 failed\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_FC_1,
			ARRAY_SIZE(aInitCode_FC_1)) == -1)
	dev_err(dsim->dev, "LCD init command 7 failed\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		aDispalyOn, 1);
}

int s6d7aa0_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);
	return 1;
}

int s6d7aa0_suspend(struct mipi_dsim_device *dsim)
{
	return 1;
}

int s6d7aa0_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver s6d7aa0_mipi_lcd_driver = {
	.probe		= s6d7aa0_probe,
	.displayon	= s6d7aa0_displayon,
	.suspend	= s6d7aa0_suspend,
	.resume		= s6d7aa0_resume,
};
