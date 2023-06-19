/* linux/drivers/video/exynos_decon/panel/dsim_panel.h
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

#ifndef __DSIM_PANEL__
#define __DSIM_PANEL__


extern unsigned int lcdtype;
#if defined(CONFIG_PANEL_S6E8AA5X01)
extern struct mipi_dsim_lcd_driver s6e8aa5x01_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_TD4100)
extern struct mipi_dsim_lcd_driver td4100_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_TD4100_J3POP)
extern struct mipi_dsim_lcd_driver td4100_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_S6D7AA0_XCOVER4)
extern struct mipi_dsim_lcd_driver s6d7aa0_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_TD4100_J3Y17)
extern struct mipi_dsim_lcd_driver td4100_mipi_lcd_driver;
#endif

int dsim_panel_ops_init(struct dsim_device *dsim);


#endif
