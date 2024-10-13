
/* linux/drivers/video/exynos_decon/panel/dsim_panel.c
 *
 * Header file for Samsung MIPI-DSI LCD Panel driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/module.h>
#include "../dsim.h"
#include "dsim_panel.h"


#if defined(CONFIG_EXYNOS3475_DECON_LCD_SC7798D)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &sc7798d_mipi_lcd_driver;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_S6E88A0)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &s6e88a0_mipi_lcd_driver;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_EA8061S_J1)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &ea8061s_mipi_lcd_driver;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_S6D7AA0)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &s6d7aa0_mipi_lcd_driver;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_ILI9881C)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &ili9881c_mipi_lcd_driver;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_S6E8AA5X01)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &s6e8aa5x01_mipi_lcd_driver;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_SC7798D_XCOVER3)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &sc7798d_mipi_lcd_driver;
#endif

unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

int dsim_panel_ops_init(struct dsim_device *dsim)
{
	int ret = 0;

#if defined(CONFIG_EXYNOS3475_DECON_LCD_S6E88A0)
	if (dsim) {
		if (dsim->octa_id)
			mipi_lcd_driver = &ea8061s_mipi_lcd_driver;
	}
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_ILI9881C)
	if (dsim) {
		if (lcdtype == 0x59b810)
			mipi_lcd_driver = &s6d7aa0x62_mipi_lcd_driver;
	}
#endif

	if (dsim)
		dsim->panel_ops = mipi_lcd_driver;

	return ret;
}


