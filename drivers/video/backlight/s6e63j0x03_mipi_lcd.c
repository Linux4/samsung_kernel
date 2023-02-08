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


#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

static struct mipi_dsim_device *dsim_base;
static struct backlight_device *bd;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    s6e63j0x03_early_suspend;
#endif


static const unsigned char TEST_KEY_ON_1[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char TEST_KEY_ON_2[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char PORCH_ADJUSTMENT[] = {
	0xF2,
	0x1C, 0x28
};

static const unsigned char FRAME_FREQ_SET[] = {
	0xB5,
	0x00, 0x01, 0x00
};

static const unsigned char MEM_ADDR_SET_0[] = {
	0x2A,
	0x00, 0x14, 0x01, 0x53
};

static const unsigned char MEM_ADDR_SET_1[] = {
	0x2B,
	0x00, 0x00, 0x01, 0x3F
};

static const unsigned char LPTS_TIMMING_SET_0[] = {
	0xF8,
	0x08, 0x08, 0x08, 0x17,
	0x00, 0x2A, 0x02, 0x26,
	0x00, 0x00, 0x02, 0x00,
	0x00
};

static const unsigned char LPTS_TIMMING_SET_1[] = {
	0xF7,
	0x02, 0x00
};

static const unsigned char SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char ELVSS_COND[] = {
	0xB1, 
	0x00, 0x09,
};

static const unsigned char DEFAULT_WHITE_BRIGHTNESS[] = {
	0x51,
	0xFF, 0x00
};

static const unsigned char DEFAULT_WHITE_CTRL[] = {
	0x53,
	0x20, 0x00
};

static const unsigned char ACL_OFF[] = {
	0x55,
	0x00, 0x00
};

static const unsigned char TEST_KEY_OFF_2[] = {
	0xF1,
	0xA5, 0xA5
};

static const unsigned char TE_ON[] = {
	0x35,
	0x00, 0x00
};

static const unsigned char SET_POS[] = {
	0x36,
	0x40, 0x00
};


static const unsigned char DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};



static int s6e63j0x03_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int get_backlight_level(int brightness)
{

	return brightness;
}

static int update_brightness(int brightness)
{
	int backlightlevel;

	backlightlevel = get_backlight_level(brightness);

	return 1;
}

static int s6e63j0x03_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		return -EINVAL;
	}

	update_brightness(brightness);

	return 1;
}

static const struct backlight_ops s6e63j0x03_backlight_ops = {
	.get_brightness = s6e63j0x03_get_brightness,
	.update_status = s6e63j0x03_set_brightness,
};

static int s6e63j0x03_probe(struct mipi_dsim_device *dsim)
{
	dsim_base = dsim;

	printk("%s was called\n", __func__);

	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &s6e63j0x03_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 1;
}

static void init_lcd(struct mipi_dsim_device *dsim)
{
	printk("%s was called\n", __func__);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_1, ARRAY_SIZE(TEST_KEY_ON_1)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			PORCH_ADJUSTMENT, ARRAY_SIZE(PORCH_ADJUSTMENT)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			FRAME_FREQ_SET, ARRAY_SIZE(FRAME_FREQ_SET)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			MEM_ADDR_SET_0, ARRAY_SIZE(MEM_ADDR_SET_0)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			MEM_ADDR_SET_1, ARRAY_SIZE(MEM_ADDR_SET_1)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			LPTS_TIMMING_SET_0, ARRAY_SIZE(LPTS_TIMMING_SET_0)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			LPTS_TIMMING_SET_1, ARRAY_SIZE(LPTS_TIMMING_SET_1)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			SLEEP_OUT, ARRAY_SIZE(SLEEP_OUT)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	usleep_range(120000, 120000);

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			DEFAULT_WHITE_BRIGHTNESS, ARRAY_SIZE(DEFAULT_WHITE_BRIGHTNESS)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			DEFAULT_WHITE_CTRL, ARRAY_SIZE(DEFAULT_WHITE_CTRL)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			ACL_OFF, ARRAY_SIZE(ACL_OFF)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			ELVSS_COND, ARRAY_SIZE(ELVSS_COND)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");
	
	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			SET_POS, ARRAY_SIZE(SET_POS)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			TE_ON, ARRAY_SIZE(TE_ON)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");

	if (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
			DISPLAY_ON, ARRAY_SIZE(DISPLAY_ON)) == -1)
	dev_err(dsim->dev, "fail to send panel_condition_set command.\n");



}

static int s6e63j0x03_displayon(struct mipi_dsim_device *dsim)
{
	printk("%s was called\n", __func__);

	init_lcd(dsim);
	return 1;
}

static int s6e63j0x03_suspend(struct mipi_dsim_device *dsim)
{
	return 1;
}

static int s6e63j0x03_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver s6e63j0x03_mipi_lcd_driver = {
	.probe		= s6e63j0x03_probe,
	.displayon	= s6e63j0x03_displayon,
	.suspend	= s6e63j0x03_suspend,
	.resume		= s6e63j0x03_resume,
};
