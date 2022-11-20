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
#if defined(CONFIG_EXYNOS3475_DECON_LCD_S6D78A)
extern struct mipi_dsim_lcd_driver s6d78a_mipi_lcd_driver;
extern struct dsim_panel_ops s6d78a_panel_ops;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_SC7798D)
extern struct mipi_dsim_lcd_driver sc7798d_mipi_lcd_driver;
extern struct dsim_panel_ops sc7798d_panel_ops;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_SC7798D_XCOVER3)
extern struct mipi_dsim_lcd_driver sc7798d_mipi_lcd_driver;
extern struct dsim_panel_ops sc7798d_panel_ops;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_S6E88A0)
extern struct mipi_dsim_lcd_driver s6e88a0_mipi_lcd_driver;
extern struct mipi_dsim_lcd_driver ea8061s_mipi_lcd_driver;
extern struct dsim_panel_ops s6e88a0_panel_ops;
extern struct dsim_panel_ops ea8061s_panel_ops;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_EA8061S_J1)
extern struct mipi_dsim_lcd_driver ea8061s_mipi_lcd_driver;
extern struct dsim_panel_ops ea8061s_panel_ops;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_S6D7AA0)
extern struct mipi_dsim_lcd_driver s6d7aa0_mipi_lcd_driver;
extern struct dsim_panel_ops s6d7aa0_panel_ops;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_ILI9881C)
extern struct mipi_dsim_lcd_driver ili9881c_mipi_lcd_driver;
extern struct mipi_dsim_lcd_driver s6d7aa0x62_mipi_lcd_driver;
extern struct dsim_panel_ops ili9881c_panel_ops;
extern struct dsim_panel_ops s6d7aa0x62_panel_ops;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_S6E8AA5X01)
extern struct mipi_dsim_lcd_driver s6e8aa5x01_mipi_lcd_driver;
extern struct dsim_panel_ops s6e8aa5x01_panel_ops;
#elif defined(CONFIG_EXYNOS3475_DECON_LCD_S6D7AA0X62)
extern struct mipi_dsim_lcd_driver s6d7aa0x62_mipi_lcd_driver;
extern struct dsim_panel_ops s6d7aa0x62_panel_ops;
#endif

int dsim_panel_ops_init(struct dsim_device *dsim);

/* dsim_panel_get_priv_ops() this function comes from XXXX_mipi_lcd.c */
struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim);

#endif
