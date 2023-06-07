/* linux/drivers/video/fbdev/exynos/decon/panel/dsim_panel.h
 *
 * Header file for Samsung MIPI-DSI LCD Panel driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DSIM_PANEL__
#define __DSIM_PANEL__


extern unsigned int lcdtype;

extern struct mipi_dsim_lcd_driver *mipi_lcd_driver;

#if defined(CONFIG_PANEL_S6E3FA3_A7Y17)
extern struct mipi_dsim_lcd_driver s6e3fa3_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_S6E3FA3_A5Y17)
extern struct mipi_dsim_lcd_driver s6e3fa3_mipi_lcd_driver;
#endif

extern int dsim_panel_ops_init(struct dsim_device *dsim);
extern int register_lcd_driver(struct mipi_dsim_lcd_driver *drv);


#endif
