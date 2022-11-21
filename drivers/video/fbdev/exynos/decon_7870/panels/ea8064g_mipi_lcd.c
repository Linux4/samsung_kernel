/* drivers/video/fbdev/exynos/decon_7870/panels/ea8064g_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>

#include "../dsim.h"
#include "lcd_ctrl.h"
#include "decon_lcd.h"

#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 0

static struct backlight_device *bd;

static int ea8064g_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int ea8064g_set_brightness(struct backlight_device *bd)
{
	return 1;
}

static const struct backlight_ops ea8064g_backlight_ops = {
	.get_brightness = ea8064g_get_brightness,
	.update_status = ea8064g_set_brightness,
};

static int ea8064g_probe(struct dsim_device *dsim)
{
	bd = backlight_device_register("pwm-backlight.0", NULL,
		NULL, &ea8064g_backlight_ops, NULL);
	if (IS_ERR(bd))
		pr_alert("failed to register backlight device!\n");

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = DEFAULT_BRIGHTNESS;

	return 1;
}

static int ea8064g_displayon(struct dsim_device *dsim)
{
	lcd_init(&dsim->lcd_info);
	lcd_enable();
	return 1;
}

static int ea8064g_suspend(struct dsim_device *dsim)
{
	return 1;
}

static int ea8064g_resume(struct dsim_device *dsim)
{
	return 1;
}

struct mipi_dsim_lcd_driver ea8064g_mipi_lcd_driver = {
	.probe		= ea8064g_probe,
	.displayon	= ea8064g_displayon,
	.suspend	= ea8064g_suspend,
	.resume		= ea8064g_resume,
};
