/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/lcd.h>
#include <linux/clk.h>

#include <plat/backlight.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-serial.h>
#include <plat/fb.h>
#include <plat/fb-core.h>
#include <plat/regs-fb.h>
#include <plat/backlight.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_FB_SMIES
#include <plat/smies.h>
#endif
#ifdef CONFIG_FB_MIPI_DSIM
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#endif

#include <mach/map.h>
#include <mach/spi-clocks.h>
#include <mach/exynos-scaler.h>

#include <video/platform_lcd.h>

#if defined(CONFIG_LCD_MIPI_NT35510)
#if defined(CONFIG_FB_I80_COMMAND_MODE)
#define ESPRESSO3250_HBP		1
#define ESPRESSO3250_HFP		1
#define ESPRESSO3250_VBP		1
#define ESPRESSO3250_VFP		1
#define ESPRESSO3250_HSP		1
#define ESPRESSO3250_VSW		1
#define ESPRESSO3250_XRES		480
#define ESPRESSO3250_YRES		800
#define ESPRESSO3250_VIRTUAL_X	480
#define ESPRESSO3250_VIRTUAL_Y	800 * 2
#define ESPRESSO3250_WIDTH		56
#define ESPRESSO3250_HEIGHT		94
#define ESPRESSO3250_MAX_BPP	32
#define ESPRESSO3250_DEFAULT_BPP	24

static struct s3c_fb_pd_win espresso3250_fb_win0 = {
	.win_mode = {
		.left_margin	= ESPRESSO3250_HBP,
		.right_margin	= ESPRESSO3250_HFP,
		.upper_margin	= ESPRESSO3250_VBP,
		.lower_margin	= ESPRESSO3250_VFP,
		.xres		= ESPRESSO3250_XRES,
		.yres		= ESPRESSO3250_YRES,
		.hsync_len	= ESPRESSO3250_HSP,
		.vsync_len	= ESPRESSO3250_VSW,
		.cs_setup_time  = 1,
		.wr_setup_time  = 0,
		.wr_act_time    = 1,
		.wr_hold_time   = 0,
		.rs_pol         = 0,
		.i80en          = 1,
	},
	.virtual_x		= ESPRESSO3250_VIRTUAL_X,
	.virtual_y		= ESPRESSO3250_VIRTUAL_Y,
	.width			= ESPRESSO3250_WIDTH,
	.height			= ESPRESSO3250_HEIGHT,
	.max_bpp		= ESPRESSO3250_MAX_BPP,
	.default_bpp		= ESPRESSO3250_DEFAULT_BPP,
};

#else
#define ESPRESSO3250_HBP		8
#define ESPRESSO3250_HFP		8
#define ESPRESSO3250_VBP		8
#define ESPRESSO3250_VFP		27
#define ESPRESSO3250_HSP		2
#define ESPRESSO3250_VSW		2
#define ESPRESSO3250_XRES		480
#define ESPRESSO3250_YRES		800
#define ESPRESSO3250_VIRTUAL_X	480
#define ESPRESSO3250_VIRTUAL_Y	800 * 2
#define ESPRESSO3250_WIDTH		56
#define ESPRESSO3250_HEIGHT		94
#define ESPRESSO3250_MAX_BPP	32
#define ESPRESSO3250_DEFAULT_BPP	24

static struct s3c_fb_pd_win espresso3250_fb_win0 = {
	.win_mode = {
		.left_margin	= ESPRESSO3250_HBP,
		.right_margin	= ESPRESSO3250_HFP,
		.upper_margin	= ESPRESSO3250_VBP,
		.lower_margin	= ESPRESSO3250_VFP,
		.hsync_len	= ESPRESSO3250_HSP,
		.vsync_len	= ESPRESSO3250_VSW,
		.xres		= ESPRESSO3250_XRES,
		.yres		= ESPRESSO3250_YRES,
	},
	.virtual_x		= ESPRESSO3250_VIRTUAL_X,
	.virtual_y		= ESPRESSO3250_VIRTUAL_Y,
	.width			= ESPRESSO3250_WIDTH,
	.height			= ESPRESSO3250_HEIGHT,
	.max_bpp		= ESPRESSO3250_MAX_BPP,
	.default_bpp		= ESPRESSO3250_DEFAULT_BPP,
};

#endif

static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
		unsigned int power)
{
	if (power) {
		/* Select Mux
		 * 0: 2-lane connector; 1: 1-lane connector;
		 */
		gpio_request_one(EXYNOS3_GPC0(0), GPIOF_OUT_INIT_LOW, "GPC0");
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS3_GPC0(0), 0);
		usleep_range(20000, 21000);
		gpio_free(EXYNOS3_GPC0(0));

		/* Power */
		gpio_request_one(EXYNOS3_GPC0(4), GPIOF_OUT_INIT_HIGH, "GPC0");
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS3_GPC0(4), 1);
		usleep_range(20000, 21000);
		gpio_free(EXYNOS3_GPC0(4));

		/* Reset */
		gpio_request_one(EXYNOS3_GPC0(1), GPIOF_OUT_INIT_HIGH, "GPC0");
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS3_GPC0(1), 0);
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS3_GPC0(1), 1);
		gpio_free(EXYNOS3_GPC0(1));

	} else {
		gpio_request_one(EXYNOS3_GPC0(1), GPIOF_OUT_INIT_HIGH, "GPC0");
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS3_GPC0(1), 0);
		usleep_range(20000, 21000);
		gpio_free(EXYNOS3_GPC0(1));

		gpio_request_one(EXYNOS3_GPC0(4), GPIOF_OUT_INIT_LOW, "GPC0");
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS3_GPC0(4), 0);
		usleep_range(20000, 21000);
		gpio_free(EXYNOS3_GPC0(4));
	}
	usleep_range(20000, 21000);
	return 1;
}

static void s5p_mipi_dsi_bl_on_off(unsigned int power)
{
}
#endif

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;
#ifdef CONFIG_FB_MIPI_DSIM
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 * ---------------------
	 *  0 | MIE/MDNIE/SMIES
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0210);
	reg &= ~(1 << 1);
#if !defined(CONFIG_FB_SMIES_ENABLE_BOOTTIME)
	reg |= (1 << 1);
#endif
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
#endif
#if defined(CONFIG_FB_I80_COMMAND_MODE)
	reg = __raw_readl(S3C_VA_SYS + 0x0210);
	reg &= ~(3 << 10);
	reg |= (1 << 10);
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
#endif
}

static struct s3c_fb_platdata espresso3250_lcd0_pdata __initdata = {
	.win[0]		= &espresso3250_fb_win0,
	.win[1]		= &espresso3250_fb_win0,
	.win[2]		= &espresso3250_fb_win0,
	.win[3]		= &espresso3250_fb_win0,
	.win[4]		= &espresso3250_fb_win0,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_VCLK,
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
	.backlight_ctrl = s5p_mipi_dsi_bl_on_off,
	.dsim0_device   = &s5p_device_mipi_dsim0.dev,
	.bootlogo = false,
	.ip_version     = EXYNOS5_813,
#ifdef CONFIG_FB_SMIES
	.smies_on	= smies_enable_by_fimd,
	.smies_off	= smies_disable_by_fimd,
	.smies_device	= &s5p_device_smies.dev,
#endif
};

#ifdef CONFIG_FB_MIPI_DSIM
#define DSIM_L_MARGIN	ESPRESSO3250_HBP
#define DSIM_R_MARGIN	ESPRESSO3250_HFP
#define DSIM_UP_MARGIN	ESPRESSO3250_VBP
#define DSIM_LOW_MARGIN	ESPRESSO3250_VFP
#define DSIM_HSYNC_LEN	ESPRESSO3250_HSP
#define DSIM_VSYNC_LEN	ESPRESSO3250_VSW
#define DSIM_WIDTH	ESPRESSO3250_XRES
#define DSIM_HEIGHT	ESPRESSO3250_YRES

#if defined(CONFIG_LCD_MIPI_NT35510)
#if defined(CONFIG_FB_I80_COMMAND_MODE)
static struct mipi_dsim_config dsim_info = {
	.e_interface		= DSIM_COMMAND,
	.e_pixel_format		= DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush		= false,
	.eot_disable		= false,
	.auto_vertical_cnt	= false,

	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_2,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	.p = 2,
	.m = 42,
	.s = 0,

	.esc_clk	= 20 * 1000000, /* escape clk : 20MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0xfff,
	.bta_timeout		= 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout		= 0xffff,	/* lp rx timeout 0 ~ 0xffff */
	.pll_stable_time	= 500,

	.dsim_ddi_pd = &nt35510_mipi_lcd_driver,

};
#else
static struct mipi_dsim_config dsim_info = {
	.e_interface		= DSIM_VIDEO,
	.e_pixel_format		= DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush		= false,
	.eot_disable		= true,
	.auto_vertical_cnt	= false,

	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_2,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	.p = 2,
	.m = 42,
	.s = 0,

	.esc_clk	= 20 * 1000000, /* escape clk : 20MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0xfff,
	.bta_timeout		= 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout		= 0xffff,	/* lp rx timeout 0 ~ 0xffff */
	.pll_stable_time	= 0xffffff,

	.dsim_ddi_pd = &nt35510_mipi_lcd_driver,
};
#endif

#if defined(CONFIG_FB_I80_COMMAND_MODE)
static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
};
#else
static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= DSIM_L_MARGIN,
	.rgb_timing.right_margin	= DSIM_R_MARGIN,
	.rgb_timing.upper_margin	= DSIM_UP_MARGIN,
	.rgb_timing.lower_margin	= DSIM_LOW_MARGIN,
	.rgb_timing.hsync_len		= DSIM_HSYNC_LEN,
	.rgb_timing.vsync_len		= DSIM_VSYNC_LEN,
	.rgb_timing.stable_vfp		= 1,
	.rgb_timing.cmd_allow		= 0,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
};
#endif

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim0",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,
	.mipi_power   = mipi_lcd_power_control,
	.init_d_phy		= s5p_dsim_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,
};
#endif
#endif

#if defined(CONFIG_FB_SMIES)
static struct s5p_smies_platdata espresso3250_smies_pdata __initdata = {
	.sae_on			= 1,
	.scr_on			= 1,
	.gamma_on		= 1,
	.dither_on		= 0,
	.sae_skin_check		= 0,
	.sae_gain		= 0,
	.scr_r			= 0xff0000,
	.scr_g			= 0x00ff00,
	.scr_b			= 0x0000ff,
	.scr_c			= 0x00ffff,
	.scr_m			= 0xff00ff,
	.scr_y			= 0xffff00,
	.scr_white		= 0xffffff,
	.scr_black		= 0x000000,
	.width			= ESPRESSO3250_XRES,
	.height			= ESPRESSO3250_YRES,
};
#endif

static struct platform_device *espresso3250_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim0,
#endif
	&s5p_device_fimd0,
	&exynos5_device_scaler0,
#if defined(CONFIG_FB_SMIES)
	&s5p_device_smies,
#endif
};

static int __init setup_early_bootlogo(char *str)
{
	espresso3250_lcd0_pdata.bootlogo = false;
	return 0;
}
early_param("bootlogo", setup_early_bootlogo);

static struct exynos_scaler_platdata exynos3250_scaler_pd __initdata;

void __init exynos3_espresso3250_display_init(void)
{
	s3c_set_platdata(&exynos3250_scaler_pd, sizeof(exynos3250_scaler_pd),
			&exynos5_device_scaler0);

#if defined(CONFIG_FB_SMIES)
	s5p_smies_set_platdata(&espresso3250_smies_pdata);
#endif

	s5p_dsim0_set_platdata(&dsim_platform_data);

	s5p_fimd0_set_platdata(&espresso3250_lcd0_pdata);

	platform_add_devices(espresso3250_display_devices,
			ARRAY_SIZE(espresso3250_display_devices));

#if defined(CONFIG_LCD_MIPI_NT35510)
#if defined(CONFIG_FB_I80_COMMAND_MODE)
	exynos4_fimd_setup_clock(&s5p_device_fimd0.dev, "sclk_fimd", \
				"sclk_mpll_pre_div", 100 * MHZ);
#else
	exynos4_fimd_setup_clock(&s5p_device_fimd0.dev, "sclk_fimd", \
				"sclk_mpll_pre_div", 25* MHZ);
#endif
#endif
}
