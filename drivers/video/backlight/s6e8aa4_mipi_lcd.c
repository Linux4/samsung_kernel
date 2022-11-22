/* linux/drivers/video/backlight/s6e8aa4_mipi_lcd.c
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
#include <plat/gpio-cfg.h>

#include "s6e8aa4_gamma.h"

#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

static struct mipi_dsim_device *dsim_base;
static struct backlight_device *bd;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    s6e8aa4_early_suspend;
#endif

unsigned int lcdtype;
static int __init lcdtype_setup(char *str)
{
	get_option(&str, &lcdtype);
	return 1;
}
__setup("lcdtype=", lcdtype_setup);


static const unsigned char SEQ_APPLY_LEVEL_2_KEY_ENABLE[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_APPLY_MTP_KEY_ENABLE[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_GAMMA_CONDITION_SET_REV00[] = {
	0xCA,
	0x01, 0x2c, 0x01, 0x0d, 0x01, 0x62, /* V255 R,G,B*/
	0xdf, 0xdf, 0xde,			   /* V203 R,G,B*/
	0xda, 0xdb, 0xd9,			   /* V151 R,G,B*/
	0xc6, 0xcb, 0xc5,			   /*  V87 R,G,B*/
	0xd2, 0xd5, 0xd1,			   /*  V51 R,G,B*/
	0xdd, 0xe5, 0xe4,			   /*  V35 R,G,B*/
	0xde, 0xe1, 0xe0,			   /*  V23 R,G,B*/
	0xc1, 0xd4, 0xca,			   /*  V11 R,G,B*/
	0xbe, 0xda, 0xca,			   /*   V3 R,G,B*/
	0x02, 0x03, 0x02			   /*   VT R,G,B*/
};

static const unsigned char SEQ_GAMMA_CONDITION_SET_REV01[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7, 0x03,
	0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};

static const unsigned char SEQ_POWER_CONTROL_F4[] = {
	0xF4,
	0xA7, 0x10, 0x99, 0x00, 0x09, 0x8C, 0x00
};



static int s6e8aa4_get_brightness(struct backlight_device *bd)
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

	s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			gamma22_table[backlightlevel],
				GAMMA_PARAM_SIZE);

	s5p_mipi_dsi_wr_data(dsim_base, MIPI_DSI_DCS_LONG_WRITE,
			SEQ_GAMMA_UPDATE,
				ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	return 1;
}

static int s6e8aa4_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(brightness);

	return 1;
}

static const struct backlight_ops s6e8aa4_backlight_ops = {
	.get_brightness = s6e8aa4_get_brightness,
	.update_status = s6e8aa4_set_brightness,
};

static int s6e8aa4_probe(struct mipi_dsim_device *dsim)
{
	dsim_base = dsim;

	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &s6e8aa4_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

/*
#if !defined(CONFIG_S3C_BOOT_LOGO)
	if (dsim->pd->lcd_reset)
		dsim->pd->lcd_reset();
	if (dsim->pd->lcd_power_on_off)
		dsim->pd->lcd_power_on_off(true);
#endif
*/
	return 1;
}

static void init_lcd(struct mipi_dsim_device *dsim)
{
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_APPLY_LEVEL_2_KEY_ENABLE,
			ARRAY_SIZE(SEQ_APPLY_LEVEL_2_KEY_ENABLE));

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_APPLY_MTP_KEY_ENABLE,
			ARRAY_SIZE(SEQ_APPLY_MTP_KEY_ENABLE));

	usleep_range(16000, 16000);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_SLEEP_OUT,
			ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(120);

	if ((lcdtype&0xFF)==0x00) {
		s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				SEQ_GAMMA_CONDITION_SET_REV00,
					ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET_REV00));
			printk("SEQ_GAMMA_CONDITION_SET_REV00=%d\n", lcdtype);
	} else {
		s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				SEQ_GAMMA_CONDITION_SET_REV01,
					ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET_REV01));
		printk("SEQ_GAMMA_CONDITION_SET_REV01=%d\n", lcdtype);
	}
	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_GAMMA_UPDATE,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE));

	//update_brightness(bd->props.brightness);

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_POWER_CONTROL_F4,
			ARRAY_SIZE(SEQ_POWER_CONTROL_F4));
}

static int s6e8aa4_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);
	return 1;
}

static int s6e8aa4_suspend(struct mipi_dsim_device *dsim)
{
	return 1;
}

static int s6e8aa4_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver s6e8aa4_mipi_lcd_driver = {
	.probe		= s6e8aa4_probe,
	.displayon	= s6e8aa4_displayon,
	.suspend	= s6e8aa4_suspend,
	.resume		= s6e8aa4_resume,
};
