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

#if IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3HA2K)
extern struct dsim_lcd_driver s6e3ha2k_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3HF4)
extern struct dsim_lcd_driver s6e3hf4_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3HA6)
extern struct dsim_lcd_driver s6e3ha6_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3HA8)
extern struct dsim_lcd_driver s6e3ha8_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3AA2)
extern struct dsim_lcd_driver s6e3aa2_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FA0)
extern struct dsim_lcd_driver s6e3fa0_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FA7)
extern struct dsim_lcd_driver s6e3fa7_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_EMUL_DISP)
extern struct dsim_lcd_driver emul_disp_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FA7_VOGUE)
extern struct dsim_lcd_driver s6e3fa7_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_EA8076_A50)
extern struct dsim_lcd_driver ea8076_mipi_lcd_driver;
#elif IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_S6E3FC2_A80)
extern struct dsim_lcd_driver s6e3fc2_mipi_lcd_driver;
#else
extern struct dsim_lcd_driver s6e3ha2k_mipi_lcd_driver;
#endif

struct dsim_device;
extern void dsim_register_panel(struct dsim_device *dsim);
extern int register_lcd_driver(struct dsim_lcd_driver *drv);

#endif
