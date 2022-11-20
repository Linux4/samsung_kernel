/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <video/platform_lcd.h>
#include <video/s5p-dp.h>

#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/fb-core.h>
#include <plat/regs-fb-v4.h>
#include <plat/dp.h>
#include <plat/backlight.h>
#include <plat/gpio-cfg.h>

#include <mach/map.h>

#ifdef CONFIG_FB_MIPI_DSIM
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/regs-mipidsim.h>
#endif

#if defined(CONFIG_LCD_MIPI_S6E3FA0)
static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
			unsigned int power)
{
	if (power) {
		/* Reset */
		gpio_request_one(EXYNOS4_GPY3(6),
				GPIOF_OUT_INIT_HIGH, "GPX2");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS4_GPY3(6), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS4_GPY3(6), 1);
		usleep_range(5000, 6000);
		gpio_free(EXYNOS4_GPY3(6));
	} else {
		/* Reset */
		gpio_request_one(EXYNOS4_GPY3(6),
				GPIOF_OUT_INIT_LOW, "GPY3");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS4_GPY3(6));
	}
	return 1;
}

#define SMDK4415_HBP (1)
#define SMDK4415_HFP (1)
#define SMDK4415_HFP_DSIM (1)
#define SMDK4415_HSP (1)
#define SMDK4415_VBP (12)
#define SMDK4415_VFP (1)
#define SMDK4415_VSW (1)
#define SMDK4415_XRES (1080)
#define SMDK4415_YRES (1920)
#define SMDK4415_VIRTUAL_X (1080)
#define SMDK4415_VIRTUAL_Y (1920 * 2)
#define SMDK4415_WIDTH (71)
#define SMDK4415_HEIGHT (114)
#define SMDK4415_MAX_BPP (32)
#define SMDK4415_DEFAULT_BPP (24)

static struct s3c_fb_pd_win smdk4415_fb_win0 = {
	.win_mode = {
		.left_margin	= SMDK4415_HBP,
		.right_margin	= SMDK4415_HFP,
		.upper_margin	= SMDK4415_VBP,
		.lower_margin	= SMDK4415_VFP,
		.hsync_len	= SMDK4415_HSP,
		.vsync_len	= SMDK4415_VSW,
		.xres		= SMDK4415_XRES,
		.yres		= SMDK4415_YRES,
	},
	.virtual_x		= SMDK4415_VIRTUAL_X,
	.virtual_y		= SMDK4415_VIRTUAL_Y,
	.width			= SMDK4415_WIDTH,
	.height			= SMDK4415_HEIGHT,
	.max_bpp		= SMDK4415_MAX_BPP,
	.default_bpp		= SMDK4415_DEFAULT_BPP,
};
#elif defined(CONFIG_LCD_MIPI_AMS480GYXX)
static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
			unsigned int power)
{
	if (power) {
		/* LED_VDD_EN */
#if 0
		gpio_request_one(EXYNOS4_GPK3(4),
				GPIOF_OUT_INIT_HIGH, "GPK3");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS4_GPK3(4), 1);
		usleep_range(5000, 6000);
#endif
		/* LCD_2.2V_EN */
		gpio_request_one(EXYNOS4_GPK3(3),
				GPIOF_OUT_INIT_HIGH, "GPK3");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS4_GPK3(3), 1);
		usleep_range(5000, 6000);

		/* Reset */
		gpio_request_one(EXYNOS4_GPK3(6),
				GPIOF_OUT_INIT_HIGH, "GPK3");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS4_GPK3(6), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS4_GPK3(6), 1);
		usleep_range(5000, 6000);
		gpio_free(EXYNOS4_GPK3(6));
	} else {
		/* Reset */
		gpio_request_one(EXYNOS4_GPK3(6),
				GPIOF_OUT_INIT_LOW, "GPK3");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS4_GPK3(6));
		/* LCD_2.2V_EN */
		gpio_request_one(EXYNOS4_GPK3(3),
				GPIOF_OUT_INIT_HIGH, "GPK3");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS4_GPK3(3), 0);
		usleep_range(5000, 6000);
		/* LED_VDD_EN */
		gpio_request_one(EXYNOS4_GPK3(4),
				GPIOF_OUT_INIT_HIGH, "GPK3");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS4_GPK3(4), 0);
		usleep_range(5000, 6000);
	}
	return 1;
}

#define SMDK4415_HBP (5)
#define SMDK4415_HFP (5)
#define SMDK4415_HFP_DSIM (5)
#define SMDK4415_HSP (5)
#define SMDK4415_VBP (1)
#define SMDK4415_VFP (13)
#define SMDK4415_VSW (2)
#define SMDK4415_XRES (720)
#define SMDK4415_YRES (1280)
#define SMDK4415_VIRTUAL_X (720)
#define SMDK4415_VIRTUAL_Y (1280 * 2)
#define SMDK4415_WIDTH (61)
#define SMDK4415_HEIGHT (107)
#define SMDK4415_MAX_BPP (32)
#define SMDK4415_DEFAULT_BPP (24)

static struct s3c_fb_pd_win smdk4415_fb_win0 = {
	.win_mode = {
		.left_margin	= SMDK4415_HBP,
		.right_margin	= SMDK4415_HFP,
		.upper_margin	= SMDK4415_VBP,
		.lower_margin	= SMDK4415_VFP,
		.hsync_len	= SMDK4415_HSP,
		.vsync_len	= SMDK4415_VSW,
		.xres		= SMDK4415_XRES,
		.yres		= SMDK4415_YRES,
	},
	.virtual_x		= SMDK4415_VIRTUAL_X,
	.virtual_y		= SMDK4415_VIRTUAL_Y,
	.width			= SMDK4415_WIDTH,
	.height			= SMDK4415_HEIGHT,
	.max_bpp		= SMDK4415_MAX_BPP,
	.default_bpp		= SMDK4415_DEFAULT_BPP,
};
#endif
static void universal4415_fimd_gpio_setup_24bpp(void)
{
	/* need to change */
	void __iomem *regs;
	regs = ioremap(0x10010210, 0x4);
	writel(0x02, regs);
	iounmap(regs);
}

static struct s3c_fb_platdata smdk4415_lcd1_pdata __initdata = {
	.win[0]		= &smdk4415_fb_win0,
	.win[1]		= &smdk4415_fb_win0,
	.win[2]		= &smdk4415_fb_win0,
	.win[3]		= &smdk4415_fb_win0,
	.win[4]		= &smdk4415_fb_win0,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_VCLK,
	.setup_gpio	= universal4415_fimd_gpio_setup_24bpp,
	.ip_version	= FIMD_VERSION,
};

#ifdef CONFIG_FB_MIPI_DSIM
#define DSIM_L_MARGIN SMDK4415_HBP
#define DSIM_R_MARGIN SMDK4415_HFP_DSIM
#define DSIM_UP_MARGIN SMDK4415_VBP
#define DSIM_LOW_MARGIN SMDK4415_VFP
#define DSIM_HSYNC_LEN SMDK4415_HSP
#define DSIM_VSYNC_LEN SMDK4415_VSW
#define DSIM_WIDTH SMDK4415_XRES
#define DSIM_HEIGHT SMDK4415_YRES

static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= DSIM_L_MARGIN,
	.rgb_timing.right_margin	= DSIM_R_MARGIN,
	.rgb_timing.upper_margin	= DSIM_UP_MARGIN,
	.rgb_timing.lower_margin	= DSIM_LOW_MARGIN,
	.rgb_timing.hsync_len		= DSIM_HSYNC_LEN,
	.rgb_timing.vsync_len		= DSIM_VSYNC_LEN,
	.rgb_timing.stable_vfp		= 2,
	.rgb_timing.cmd_allow		= 0,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
};

static struct mipi_dsim_config dsim_info = {
	.e_interface	= DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush	= false,
	.eot_disable	= true,
	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

#if defined(CONFIG_LCD_MIPI_S6E3FA0)
	.auto_vertical_cnt = false,
	.p = 2,
	.m = 46,
	.s = 1,
#elif defined(CONFIG_LCD_MIPI_AMS480GYXX)
	.auto_vertical_cnt = true,
	.p = 4,
	.m = 84,
	.s = 2,
#endif

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = DPHY_PLL_STABLE_TIME,

	.esc_clk = 7 * 1000000, /* escape clk : 8MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x0fff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

#if defined(CONFIG_LCD_MIPI_S6E3FA0)
	.dsim_ddi_pd = &s6e3fa0_mipi_lcd_driver,
#elif defined(CONFIG_LCD_MIPI_AMS480GYXX)
	.dsim_ddi_pd = &ams480gyxx_mipi_lcd_driver,
#endif
};

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim1",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,

	.mipi_power		= mipi_lcd_power_control,
	.init_d_phy		= exynos_dsim_phy_enable,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,
};
#endif

static struct platform_device *smdk4415_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim1,
#endif
	&s5p_device_fimd1,
};

#ifdef CONFIG_BACKLIGHT_PWM
/* LCD Backlight data */
static struct samsung_bl_gpio_info smdk4415_bl_gpio_info = {
	.no = EXYNOS4415_GPB2(0),
	.func = S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data smdk4415_bl_data = {
	.pwm_id = 0,
	.pwm_period_ns = 30000,
};
#endif

void __init exynos4_universal4415_display_init(void)
{
#ifdef CONFIG_FB_MIPI_DSIM
	s5p_dsim1_set_platdata(&dsim_platform_data);
#endif
	s5p_fimd1_set_platdata(&smdk4415_lcd1_pdata);
#ifdef CONFIG_BACKLIGHT_PWM
	samsung_bl_set(&smdk4415_bl_gpio_info, &smdk4415_bl_data);
#endif
	platform_add_devices(smdk4415_display_devices,
			ARRAY_SIZE(smdk4415_display_devices));
#ifdef CONFIG_FB_MIPI_DSIM
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_disp_pll", 53 * MHZ);
#endif
}
