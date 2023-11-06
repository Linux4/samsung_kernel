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

extern struct dsim_lcd_driver *mipi_lcd_driver;

#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FA0)
extern struct dsim_lcd_driver s6e3fa0_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FA7)
extern struct dsim_lcd_driver s6e3fa7_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FA7_JACKPOT2)
extern struct dsim_lcd_driver s6e3fa7_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FA7_JACKPOT)
extern struct dsim_lcd_driver s6e3fa7_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_TD4100_J3TOPE)
extern struct dsim_lcd_driver td4100_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6D7AT0B_J7TOPE)
extern struct dsim_lcd_driver s6d7at0b_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_EA8061S_J7DUO)
extern struct dsim_lcd_driver ea8061s_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E8AA5_A6ELTE)
extern struct dsim_lcd_driver s6e8aa5_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E8AA5_FEEL2)
extern struct dsim_lcd_driver s6e8aa5_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_SN65DSI86_GVIEW2)
extern struct dsim_lcd_driver sn65dsi86_hx8876_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FA7_A7Y18)
extern struct dsim_lcd_driver s6e3fa7_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_TD4101_J2COREPELTE)
extern struct dsim_lcd_driver td4101_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_HX8279D_GTA3XLLTE)
extern struct dsim_lcd_driver hx8279d_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_NT36672A_M20)
extern struct dsim_lcd_driver nt36672a_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_HX83112A_M20)
extern struct dsim_lcd_driver hx83112a_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_EA8076_M30)
extern struct dsim_lcd_driver ea8076_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_EA8076_A30)
extern struct dsim_lcd_driver ea8076_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3AA2_A40)
extern struct dsim_lcd_driver s6e3aa2_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_HX8279D_WISDOM)
extern struct dsim_lcd_driver hx8279d_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E8FC0_A20)
extern struct dsim_lcd_driver s6e8fc0_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E8FC0_A40)
extern struct dsim_lcd_driver s6e8fc0_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6D7AT0B_A10)
extern struct dsim_lcd_driver s6d7at0b_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_HX83102D_A10E)
extern struct dsim_lcd_driver hx83102d_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_HX83102D_A20E)
extern struct dsim_lcd_driver hx83102d_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6D7AA0_XCOVER4S)
extern struct dsim_lcd_driver s6d7aa0_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E8FC0_A30C)
extern struct dsim_lcd_driver s6e8fc0_mipi_lcd_driver;
#else
extern struct dsim_lcd_driver s6e3fa3_mipi_lcd_driver;
#endif

struct dsim_device;
extern void dsim_register_panel(struct dsim_device *dsim);
extern int register_lcd_driver(struct dsim_lcd_driver *drv);

#endif
