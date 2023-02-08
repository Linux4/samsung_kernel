/* linux/drivers/video/backlight/ams480gyxx_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * Haowei Li, <haowei.li@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <video/mipi_display.h>

#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <linux/errno.h>

#include "../s5p_mipi_dsi.h"

#include "ams480gyxx_gamma.h"

#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

static struct mipi_dsim_device *dsim_base;
static struct backlight_device *bd;

static const u8 gamma_update[2] = {
	0xF7,
	0x03,
};

static const u8 aInitCode_F0[] = {0xF0, 0x5A, 0x5A};

static const u8 aInitCode_F8[] = {
	0xf8,
	0x19, 0x35, 0x00, 0x00, 0x00, 0x93, 0x00, 0x3c,
	0x7d, 0x08, 0x27, 0x7D, 0x3F, 0x00, 0x00, 0x00, 0x20, 0x04,
	0x08, 0x6E, 0x00, 0x00, 0x00, 0x02, 0x08, 0x08, 0x23, 0x23,
	0xC0, 0xC1, 0x01, 0x41, 0xC1, 0x00, 0xC1, 0xF6, 0xF6, 0xC1
};/* Panel Condition Set */

static const u8 aInitCode_F2[] = {0xf2, 0x80, 0x03, 0x0d};

static const u8 aInitCode_FA[] = {
	0xfa,
	0x01, 0x0f, 0x0f, 0x0f, 0xe7, 0xc1, 0xeb, 0xd0, 0xca, 0xcd,
	0xda, 0xd8, 0xd8, 0xb8, 0xb7, 0xb3, 0xc7, 0xc8, 0xc2, 0x00,
	0x92, 0x00, 0x8a, 0x00, 0xc4
};/* Gamma Condition Set */

static const u8 aInitCode_F7[] = {0xf7, 0x02}; /* Gamma / LTPS Set Update */

static const u8 aInitCode_F6[] = { 0xf6, 0x00, 0x02, 0x00}; /* ETC Source Control */

static const u8 aInitCode_B6[] = {0xb6, 0x63, 0x02, 0x03, 0x32, 0xff, 0x44, 0x44, 0xc0, 0x00}; /* ETC Pentile Control */

static const u8 aInitCode_D9[] = {0xd9,
	0x14, 0x40, 0x0c, 0xcb, 0xce, 0x6e, 0xc4, 0x07, 0x40,
	0x40, 0xd0, 0x00, 0x60, 0x19
}; /* ETC NVM Setting */

static const u8 aInitCode_E1[] = {0xe1, 0x10, 0x1c, 0x17, 0x08, 0x1d,}; /* MIPI Control1 */

static const u8 aInitCode_E2[] = {0xe2, 0xed, 0x07, 0xc3, 0x13, 0x0d, 0x03}; /* MIPI Control 2 */

static const u8 aInitCode_E3[] = {0xe3, 0x40}; /* MIPI Control 3 */

static const u8 aInitCode_E4[] = {0xe4, 0x00, 0x00, 0x14, 0x80, 0x00, 0x00, 0x00,}; /* MIPI Control 4 */

static const u8 aInitCode_C1[] = {
	0xc1,
	0x47, 0x53, 0x13, 0x53, 0x00, 0x00, 0x02, 0x57, 0x00,
	0x00, 0x03, 0xff, 0x0a, 0x15, 0x20, 0x2b, 0x36, 0x41, 0x4c,
	0x57, 0x62, 0x6d, 0x78, 0x83, 0x8e, 0x99, 0xa4, 0xaf
}; /* ACL Control Set */

static const u8 aInitCode_C0[] = {0xc0, 0x01};/* ACL On/Off Command */

static const u8 sleep_out[] = {0x11};

static const u8 disp_on[] = {0x29};

static int ams480gyxx_get_brightness(struct backlight_device *bd)
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

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			gamma22_table[backlightlevel],
				GAMMA_PARAM_SIZE) == -1)
		printk(KERN_ERR "LCD: fail to write gamma value.\n");

	if (s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			gamma_update,
				ARRAY_SIZE(gamma_update)) == -1)
		printk(KERN_ERR "LCD: fail to update gamma value.\n");

	return 1;
}

static int ams480gyxx_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(brightness);

	return 1;
}

static const struct backlight_ops ams480gyxx_backlight_ops = {
	.get_brightness = ams480gyxx_get_brightness,
	.update_status = ams480gyxx_set_brightness,
};

static int ams480gyxx_probe(struct mipi_dsim_device *dsim)
{
	dsim_base = dsim;

	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &ams480gyxx_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 1;
}

static void init_lcd(struct mipi_dsim_device *dsim)
{
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		aInitCode_F0, ARRAY_SIZE(aInitCode_F0)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_F0 command.\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		sleep_out, 1);

	msleep(200);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_F8, ARRAY_SIZE(aInitCode_F8)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_F8 command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_F2, ARRAY_SIZE(aInitCode_F2)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_F2 command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_FA, ARRAY_SIZE(aInitCode_FA)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_FA command.\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		aInitCode_F7, 2);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_F6, ARRAY_SIZE(aInitCode_F6)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_F6 command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_B6, ARRAY_SIZE(aInitCode_B6)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_B6 command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_D9, ARRAY_SIZE(aInitCode_D9)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_D9 command.\n");

	msleep(200);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_E1, ARRAY_SIZE(aInitCode_E1)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_E1 command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_E2, ARRAY_SIZE(aInitCode_E2)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_E2 command.\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		aInitCode_E3, 2);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_E4, ARRAY_SIZE(aInitCode_E4)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_E4 command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			aInitCode_C1, ARRAY_SIZE(aInitCode_C1)) == -1)
		dev_err(dsim->dev, "fail to send aInitCode_C1 command.\n");

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		aInitCode_C0, 2);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
		disp_on, 1);

	msleep(120);
}

static int ams480gyxx_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);
	return 1;
}

static int ams480gyxx_suspend(struct mipi_dsim_device *dsim)
{
	return 1;
}

static int ams480gyxx_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver ams480gyxx_mipi_lcd_driver = {
	.probe		= ams480gyxx_probe,
	.displayon	= ams480gyxx_displayon,
	.suspend	= ams480gyxx_suspend,
	.resume		= ams480gyxx_resume,
};
