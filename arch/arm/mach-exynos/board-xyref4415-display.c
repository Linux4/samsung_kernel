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
#elif defined(CONFIG_S5P_DP)

static void s5p_dp_backlight_on(void);
static void s5p_dp_backlight_off(void);

#define LCD_POWER_OFF_TIME_US	(500 * USEC_PER_MSEC)

static ktime_t lcd_on_time;

static void xyref4415_lcd_on(void)
{
	s64 us = ktime_us_delta(lcd_on_time, ktime_get_boottime());

	if (us > LCD_POWER_OFF_TIME_US) {
		pr_warn("lcd on sleep time too long\n");
		us = LCD_POWER_OFF_TIME_US;
	}

	if (us > 0)
		usleep_range(us, us);

	s3c_gpio_setpull(EXYNOS4415_GPB2(0), S3C_GPIO_PULL_NONE);

	s3c_gpio_setpull(EXYNOS4415_GPD2(2), S3C_GPIO_PULL_NONE);
	gpio_request_one(EXYNOS4415_GPD2(2),
			GPIOF_OUT_INIT_HIGH, "GPD2");
	usleep_range(5000, 6000);
	gpio_free(EXYNOS4415_GPD2(2));

#ifndef CONFIG_BACKLIGHT_PWM
	s3c_gpio_setpull(EXYNOS4415_GPD2(1), S3C_GPIO_PULL_NONE);
	gpio_request_one(EXYNOS4415_GPD2(1),
			GPIOF_OUT_INIT_HIGH, "GPD2");
	usleep_range(5000, 6000);
	gpio_free(EXYNOS4415_GPD2(1));
#endif
}

static void xyref4415_lcd_off(void)
{
	gpio_request_one(EXYNOS4415_GPD2(2),
			GPIOF_OUT_INIT_LOW, "GPD2");
	gpio_free(EXYNOS4415_GPD2(2));
	usleep_range(5000, 6000);

#ifndef CONFIG_BACKLIGHT_PWM
	gpio_request_one(EXYNOS4415_GPD2(1),
			GPIOF_OUT_INIT_LOW, "GPD2");
	usleep_range(5000, 6000);
	gpio_free(EXYNOS4415_GPD2(1));
#endif
	lcd_on_time = ktime_add_us(ktime_get_boottime(), LCD_POWER_OFF_TIME_US);
}

static void dp_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	if (power)
		xyref4415_lcd_on();
	else
		xyref4415_lcd_off();
}

static struct plat_lcd_data xyref4415_dp_lcd_data = {
	.set_power	= dp_lcd_set_power,
};

static struct platform_device xyref4415_dp_lcd = {
	.name	= "platform-lcd",
	.dev	= {
		.parent		= &s5p_device_fimd1.dev,
		.platform_data	= &xyref4415_dp_lcd_data,
	},
};

static struct s3c_fb_pd_win xyref4415_fb_win0 = {
	.win_mode = {
		.left_margin	= 80,
		.right_margin	= 48,
		.upper_margin	= 37,
		.lower_margin	= 3,
		.hsync_len	= 32,
		.vsync_len	= 6,
		.xres		= 2560,
		.yres		= 1600,
	},
	.virtual_x		= 2560,
	.virtual_y		= 1640 * 2,
	.width			= 218,
	.height			= 136,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#endif
static void xyref4415_fimd_gpio_setup_24bpp(void)
{
	void __iomem *regs;
	regs = ioremap(0x10010210, 0x4);
	writel(0x02, regs);
	iounmap(regs);
}

static struct s3c_fb_platdata smdk4415_lcd1_pdata __initdata = {
#if defined(CONFIG_S5P_DP)
	.win[0]		= &xyref4415_fb_win0,
	.win[1]		= &xyref4415_fb_win0,
	.win[2]		= &xyref4415_fb_win0,
	.win[3]		= &xyref4415_fb_win0,
	.win[4]		= &xyref4415_fb_win0,
#else
	.win[0]		= &smdk4415_fb_win0,
	.win[1]		= &smdk4415_fb_win0,
	.win[2]		= &smdk4415_fb_win0,
	.win[3]		= &smdk4415_fb_win0,
	.win[4]		= &smdk4415_fb_win0,
#endif
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
#if defined(CONFIG_S5P_DP)
	.vidcon1	= 0,
	.backlight_off	= s5p_dp_backlight_off,
	.lcd_off	= xyref4415_lcd_off,
#else
	.vidcon1	= VIDCON1_INV_VCLK,
#endif
	.setup_gpio	= xyref4415_fimd_gpio_setup_24bpp,
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
	.rgb_timing.cmd_allow		= 1,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
};

#if defined(CONFIG_LCD_MIPI_S6E3FA0)
static struct mipi_dsim_config dsim_info = {
	.e_interface	= DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush	= false,
	.eot_disable	= true,
	.auto_vertical_cnt = false,
	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	.p = 2,
	.m = 46,
	.s = 1,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = DPHY_PLL_STABLE_TIME,

	.esc_clk = 7 * 1000000, /* escape clk : 8MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x0fff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &s6e3fa0_mipi_lcd_driver,
};
#endif

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

#ifdef CONFIG_S5P_DP
static struct video_info xyref4415_dp_config = {
	.name			= "WQXGA(2560x1600) LCD, for XYREF TEST",

	.h_sync_polarity	= 0,
	.v_sync_polarity	= 0,
	.interlaced		= 0,

	.color_space		= COLOR_RGB,
	.dynamic_range		= VESA,
	.ycbcr_coeff		= COLOR_YCBCR601,
	.color_depth		= COLOR_8,

	.link_rate		= LINK_RATE_2_70GBPS,
	.lane_count		= LANE_COUNT4,
};

static void s5p_dp_backlight_on(void)
{
	usleep_range(97000, 97000);

	/* LED_BACKLIGHT_RESET: GPD2_0 */
	gpio_request_one(EXYNOS4415_GPD2(0),
			GPIOF_OUT_INIT_HIGH, "GPD2");
	gpio_free(EXYNOS4415_GPD2(0));
}

static void s5p_dp_backlight_off(void)
{
	usleep_range(97000, 97000);

	/* LED_BACKLIGHT_RESET: GPD2_0 */
	gpio_request_one(EXYNOS4415_GPD2(0),
			GPIOF_OUT_INIT_LOW, "GPD2");
	gpio_free(EXYNOS4415_GPD2(0));
}

static struct s5p_dp_platdata xyref4415_dp_data __initdata = {
	.video_info	= &xyref4415_dp_config,
	.phy_init	= s5p_dp_phy_init,
	.phy_exit	= s5p_dp_phy_exit,
	.backlight_on	= s5p_dp_backlight_on,
	.backlight_off	= s5p_dp_backlight_off,
};
#endif

static struct platform_device *smdk4415_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim1,
#endif
	&s5p_device_fimd1,
#ifdef CONFIG_S5P_DP
	&s5p_device_dp,
	&xyref4415_dp_lcd,
#endif
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

void __init exynos4_xyref4415_display_init(void)
{
#ifdef CONFIG_FB_MIPI_DSIM
	s5p_dsim1_set_platdata(&dsim_platform_data);
#endif
#ifdef CONFIG_S5P_DP
	s5p_dp_set_platdata(&xyref4415_dp_data);
#endif
	s5p_fimd1_set_platdata(&smdk4415_lcd1_pdata);
#ifdef CONFIG_BACKLIGHT_PWM
	samsung_bl_set(&smdk4415_bl_gpio_info, &smdk4415_bl_data);
#endif
	platform_add_devices(smdk4415_display_devices,
			ARRAY_SIZE(smdk4415_display_devices));
#ifdef CONFIG_S5P_DP
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_disp_pll", 267 * MHZ);
#endif
#ifdef CONFIG_FB_MIPI_DSIM
	/* RPLL rate is 300Mhz, 300/5=60Hz */
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_disp_pll", 134 * MHZ);
#endif
}
