/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DSIM_PANEL__
#define __DSIM_PANEL__


extern unsigned int lcdtype;

extern struct mipi_dsim_lcd_driver *mipi_lcd_driver;

#if defined(CONFIG_PANEL_S6E8AA5X01)
extern struct mipi_dsim_lcd_driver s6e8aa5x01_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_TD4100)
extern struct mipi_dsim_lcd_driver td4100_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_S6D7AA0_XCOVER4)
extern struct mipi_dsim_lcd_driver s6d7aa0_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_TD4100_J3POP)
extern struct mipi_dsim_lcd_driver td4100_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_TD4100_J3Y17)
extern struct mipi_dsim_lcd_driver td4100_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_S6D7AA0_GTESVE)
extern struct mipi_dsim_lcd_driver s6d7aa0_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_TD4100_J3TOP)
extern struct mipi_dsim_lcd_driver td4100_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_EA8061S_J4LTE)
extern struct mipi_dsim_lcd_driver ea8061s_mipi_lcd_driver;
#elif defined(CONFIG_PANEL_TD4101_J2CORELTE)
extern struct mipi_dsim_lcd_driver td4101_mipi_lcd_driver;
#endif

extern int dsim_panel_ops_init(struct dsim_device *dsim);
extern int register_lcd_driver(struct mipi_dsim_lcd_driver *drv);

#endif
