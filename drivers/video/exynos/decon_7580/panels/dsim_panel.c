
/* linux/drivers/video/exynos/decon/panels/dsim_panel.c
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/lcd.h>
#include "../dsim.h"

#include "dsim_panel.h"


#if defined(CONFIG_PANEL_EA8064G_DYNAMIC)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &ea8064g_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_LTM184HL01)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &ltm184hl01_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_S6E3AA2_A3XE)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &s6e3aa2_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_S6E3FA3_A5XE)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &s6e3fa3_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_S6E3FA3_A7XE)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &s6e3fa3_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_EA8061_DYNAMIC)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &ea8061_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_S6E3FA3_J7XE)
struct mipi_dsim_lcd_driver *mipi_lcd_driver = &s6e3fa3_mipi_lcd_driver;
#endif

int dsim_panel_ops_init(struct dsim_device *dsim)
{
	int ret = 0;

	if (dsim)
		dsim->panel_ops = mipi_lcd_driver;

	return ret;
}


unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %08x\n", lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

