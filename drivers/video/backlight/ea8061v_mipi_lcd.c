/* linux/drivers/video/backlight/ea8061v_mipi_lcd.c
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


#define GAMMA_PARAM_SIZE 26
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

static struct mipi_dsim_device *dsim_base;
static struct backlight_device *bd;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    ea8061v_early_suspend;
#endif

static const unsigned char SEQ_APPLY_LEVEL_2_KEY[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_LEVEL_3_KEY_ENABLE[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char SEQ_LEVEL_3_KEY_DISABLE[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_APPLY_MTP_KEY_ENABLE[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};

static const unsigned char SEQ_COMMON_SETTING0[] = {
	0xB8,
	0x00, 0x10
};

static const unsigned char SEQ_COMMON_SETTING1[] = {
	0xBA,
	0x5A, 0x00
};

static const unsigned char SEQ_PANEL_CONDITION_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, /* V255 R,G,B*/
	0x80, 0x80, 0x80,       /* V203 R,G,B*/
	0x80, 0x80, 0x80,      /* V151 R,G,B*/
	0x80, 0x80, 0x80,      /*  V87 R,G,B*/
	0x80, 0x80, 0x80,      /*  V51 R,G,B*/
	0x80, 0x80, 0x80,      /*  V35 R,G,B*/
	0x80, 0x80, 0x80,      /*  V23 R,G,B*/
	0x80, 0x80, 0x80,      /*  V11 R,G,B*/
	0x80, 0x80, 0x80,      /*   V3 R,G,B*/
	0x00, 0x00, 0x00      /*   VT R,G,B*/
};

static const unsigned char SEQ_SCAN_DIRECTION[] = {
	0x36,
	0x02, 0x00
};

static const unsigned char SEQ_GAMMA_UPDATE_OFF[] = {
	0xF7,
	0x5A, 0x5A
};

static const unsigned char SEQ_GAMMA_CONDITION_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x00, 0x00
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x01, 0x00
};

static const unsigned char SEQ_MPS_ELVSS_SET[] = {
	0xB6,
	0x48, 0x8A
};

static const unsigned char SEQ_ELVSS_SETTING0[] = {
	0xB8,
	0x14, 0x00
};

static const unsigned char SEQ_ELVSS_SETTING1[] = {
	0xB6,
	0x48, 0x8C
};

static const unsigned char SEQ_AID_SET[] = {
	0xB2,
	0x00, 0x06, 0x00, 0x10
};

static const unsigned char SEQ_SLEW_CONTROL[] = {
	0xB4,
	0x33, 0x0A, 0x00
};

static const unsigned char SEQ_ACL_SET[] = {
	0x55,
	0x02, 0x00
};

static const unsigned char SEQ_OTP_KEY_UNLOCK[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char SEQ_OTP_KEY_LOCK[] = {
	0xF1,
	0xA5, 0xA5
};

static const struct backlight_ops ea8061v_backlight_ops = {
};

static int ea8061v_probe(struct mipi_dsim_device *dsim)
{
	dsim_base = dsim;

	bd = backlight_device_register("panel", NULL,
		NULL, &ea8061v_backlight_ops, NULL);
	if (IS_ERR(bd))
		printk(KERN_ALERT "failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 1;
}

static void init_lcd(struct mipi_dsim_device *dsim)
{
	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_APPLY_LEVEL_2_KEY,
			ARRAY_SIZE(SEQ_APPLY_LEVEL_2_KEY)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_APPLY_LEVEL_2_KEY command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_APPLY_MTP_KEY_ENABLE,
			ARRAY_SIZE(SEQ_APPLY_MTP_KEY_ENABLE)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_APPLY_MTP_KEY_ENABLE command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_LEVEL_3_KEY_ENABLE,
			ARRAY_SIZE(SEQ_LEVEL_3_KEY_ENABLE)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_SCAN_DIRECTION command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_AID_SET,
			ARRAY_SIZE(SEQ_AID_SET)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_AID_SET command.\n");


	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_COMMON_SETTING0,
			ARRAY_SIZE(SEQ_COMMON_SETTING0)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_COMMON_SETTING0 command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_COMMON_SETTING1,
			ARRAY_SIZE(SEQ_COMMON_SETTING1)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_COMMON_SETTING1 command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_GAMMA_UPDATE,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_GAMMA_UPDATE command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_SLEEP_OUT,
			ARRAY_SIZE(SEQ_SLEEP_OUT)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_SLEEP_OUT command.\n");

	msleep(200);

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_DISPLAY_ON,
			ARRAY_SIZE(SEQ_DISPLAY_ON)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_DISPLAY_ON command.\n");

	msleep(200);

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_LEVEL_3_KEY_DISABLE,
			ARRAY_SIZE(SEQ_LEVEL_3_KEY_DISABLE)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_LEVEL_3_KEY_DISABLE command.\n");

/*
	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_GAMMA_UPDATE_OFF,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE_OFF)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_GAMMA_UPDATE_OFF command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_GAMMA_CONDITION_SET,
			ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_GAMMA_CONDITION_SET command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_GAMMA_UPDATE,
			ARRAY_SIZE(SEQ_GAMMA_UPDATE)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_GAMMA_UPDATE command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_AID_SET,
			ARRAY_SIZE(SEQ_AID_SET)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_AID_SET command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_ELVSS_SET,
			ARRAY_SIZE(SEQ_ELVSS_SET)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_ELVSS_SET command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_SLEW_CONTROL,
			ARRAY_SIZE(SEQ_SLEW_CONTROL)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_SLEW_CONTROL command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_ACL_SET,
			ARRAY_SIZE(SEQ_ACL_SET)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_ACL_SET command.\n");

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_SLEEP_OUT,
			ARRAY_SIZE(SEQ_SLEEP_OUT)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_SLEEP_OUT command.\n");

	msleep(200);

	while (s5p_mipi_dsi_wr_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
		SEQ_DISPLAY_ON,
			ARRAY_SIZE(SEQ_DISPLAY_ON)) == -1)
		dev_err(dsim->dev, "fail to send SEQ_DISPLAY_ON command.\n");*/
}

static int ea8061v_displayon(struct mipi_dsim_device *dsim)
{
	init_lcd(dsim);
	return 1;
}

static int ea8061v_suspend(struct mipi_dsim_device *dsim)
{
	return 1;
}

static int ea8061v_resume(struct mipi_dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver ea8061v_mipi_lcd_driver = {
	.probe		= ea8061v_probe,
	.displayon	= ea8061v_displayon,
	.suspend	= ea8061v_suspend,
	.resume		= ea8061v_resume,
};
