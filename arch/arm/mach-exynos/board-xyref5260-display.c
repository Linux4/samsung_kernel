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
#include <mach/setup-disp-clock.h>

#ifdef CONFIG_FB_MIPI_DSIM
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/regs-mipidsim.h>
#endif

#if defined(CONFIG_LCD_MIPI_S6E8AA0)
static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
			unsigned int power)
{
	if (power) {
		/* Reset */
		gpio_request_one(EXYNOS5260_GPX2(4),
				GPIOF_OUT_INIT_HIGH, "GPX2");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5260_GPX2(4), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5260_GPX2(4), 1);
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5260_GPX2(4));
	} else {
		/* Reset */
		gpio_request_one(EXYNOS5260_GPX2(4),
				GPIOF_OUT_INIT_LOW, "GPX2");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5260_GPX2(4));
	}
	return 1;
}

#define SMDK5260_HBP (150)
#define SMDK5260_HFP (26)
#define SMDK5260_HFP_DSIM (26)
#define SMDK5260_HSP (4)
#define SMDK5260_VBP (1)
#define SMDK5260_VFP (13)
#define SMDK5260_VSW (2)
#define SMDK5260_XRES (800)
#define SMDK5260_YRES (1280)
#define SMDK5260_VIRTUAL_X (800)
#define SMDK5260_VIRTUAL_Y (1280 * 2)
#define SMDK5260_WIDTH (71)
#define SMDK5260_HEIGHT (114)
#define SMDK5260_MAX_BPP (32)
#define SMDK5260_DEFAULT_BPP (24)

static struct s3c_fb_pd_win smdk5260_fb_win0 = {
	.win_mode = {
		.left_margin	= SMDK5260_HBP,
		.right_margin	= SMDK5260_HFP,
		.upper_margin	= SMDK5260_VBP,
		.lower_margin	= SMDK5260_VFP,
		.hsync_len	= SMDK5260_HSP,
		.vsync_len	= SMDK5260_VSW,
		.xres		= SMDK5260_XRES,
		.yres		= SMDK5260_YRES,
	},
	.virtual_x		= SMDK5260_VIRTUAL_X,
	.virtual_y		= SMDK5260_VIRTUAL_Y,
	.width			= SMDK5260_WIDTH,
	.height			= SMDK5260_HEIGHT,
	.max_bpp		= SMDK5260_MAX_BPP,
	.default_bpp		= SMDK5260_DEFAULT_BPP,
};

#elif defined(CONFIG_LCD_MIPI_S6E3FA0)
static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
			unsigned int power)
{
	if (power) {
		/* Reset */
		gpio_request_one(EXYNOS5260_GPB4(7),
				GPIOF_OUT_INIT_HIGH, "GPB4");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5260_GPB4(7), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5260_GPB4(7), 1);
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5260_GPB4(7));

		/* Power */
		if (gpio_request_one(EXYNOS5260_GPK0(1), GPIOF_IN, "GPK0_1")) {
			pr_err("failed to request GPK0_1\n");
		} else {
			s3c_gpio_cfgpin(EXYNOS5260_GPK0(1), S3C_GPIO_SFN(0x2));
			gpio_free(EXYNOS5260_GPK0(1));
		}

		if (gpio_request_one(EXYNOS5260_GPK0(0), GPIOF_IN, "GPK0_0")) {
			pr_err("failed to request GPK0_0\n");
		} else {
			s3c_gpio_cfgpin(EXYNOS5260_GPK0(0), S3C_GPIO_SFN(0xF));
			gpio_free(EXYNOS5260_GPK0(0));
		}
	} else {
		/* Reset */
		gpio_request_one(EXYNOS5260_GPB4(7),
				GPIOF_OUT_INIT_LOW, "GPB4");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5260_GPB4(7));
	}
	return 1;
}

#define SMDK5260_HBP (1)
#define SMDK5260_HFP (1)
#define SMDK5260_HFP_DSIM (1)
#define SMDK5260_HSP (1)
#define SMDK5260_VBP (12)
#define SMDK5260_VFP (1)
#define SMDK5260_VSW (1)
#define SMDK5260_XRES (1080)
#define SMDK5260_YRES (1920)
#define SMDK5260_VIRTUAL_X (1080)
#define SMDK5260_VIRTUAL_Y (1920 * 2)
#define SMDK5260_WIDTH (71)
#define SMDK5260_HEIGHT (114)
#define SMDK5260_MAX_BPP (32)
#define SMDK5260_DEFAULT_BPP (24)

static struct s3c_fb_pd_win smdk5260_fb_win0 = {
	.win_mode = {
		.left_margin	= SMDK5260_HBP,
		.right_margin	= SMDK5260_HFP,
		.upper_margin	= SMDK5260_VBP,
		.lower_margin	= SMDK5260_VFP,
		.hsync_len	= SMDK5260_HSP,
		.vsync_len	= SMDK5260_VSW,
		.xres		= SMDK5260_XRES,
		.yres		= SMDK5260_YRES,
	},
	.virtual_x		= SMDK5260_VIRTUAL_X,
	.virtual_y		= SMDK5260_VIRTUAL_Y,
	.width			= SMDK5260_WIDTH,
	.height			= SMDK5260_HEIGHT,
	.max_bpp		= SMDK5260_MAX_BPP,
	.default_bpp		= SMDK5260_DEFAULT_BPP,
};

#elif defined(CONFIG_S5P_DP)
static void s5p_dp_backlight_on(void);
static void s5p_dp_backlight_off(void);

#define LCD_POWER_OFF_TIME_US	(500 * USEC_PER_MSEC)

static ktime_t lcd_on_time;

static void xyref5260_lcd_on(void)
{
	s64 us = ktime_us_delta(lcd_on_time, ktime_get_boottime());

	if (us > LCD_POWER_OFF_TIME_US) {
		pr_warn("lcd on sleep time too long\n");
		us = LCD_POWER_OFF_TIME_US;
	}

	if (us > 0)
		usleep_range(us, us);

	s3c_gpio_setpull(EXYNOS5260_GPB2(0), S3C_GPIO_PULL_NONE);

	s3c_gpio_setpull(EXYNOS5260_GPD2(2), S3C_GPIO_PULL_NONE);
	gpio_request_one(EXYNOS5260_GPD2(2),
			GPIOF_OUT_INIT_HIGH, "GPD2");
	usleep_range(5000, 6000);
	gpio_free(EXYNOS5260_GPD2(2));

#ifndef CONFIG_BACKLIGHT_PWM
	s3c_gpio_setpull(EXYNOS5260_GPD2(1), S3C_GPIO_PULL_NONE);
	gpio_request_one(EXYNOS5260_GPD2(1),
			GPIOF_OUT_INIT_HIGH, "GPD2");
	usleep_range(5000, 6000);
	gpio_free(EXYNOS5260_GPD2(1));
#endif
}

static void xyref5260_lcd_off(void)
{
	gpio_request_one(EXYNOS5260_GPD2(2),
			GPIOF_OUT_INIT_LOW, "GPD2");
	gpio_free(EXYNOS5260_GPD2(2));
	usleep_range(5000, 6000);

#ifndef CONFIG_BACKLIGHT_PWM
	gpio_request_one(EXYNOS5260_GPD2(1),
			GPIOF_OUT_INIT_LOW, "GPD2");
	usleep_range(5000, 6000);
	gpio_free(EXYNOS5260_GPD2(1));
#endif
	lcd_on_time = ktime_add_us(ktime_get_boottime(), LCD_POWER_OFF_TIME_US);
}

static void dp_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	if (power)
		xyref5260_lcd_on();
	else
		xyref5260_lcd_off();
}

static struct plat_lcd_data xyref5260_dp_lcd_data = {
	.set_power	= dp_lcd_set_power,
};

static struct platform_device xyref5260_dp_lcd = {
	.name	= "platform-lcd",
	.dev	= {
		.parent		= &s5p_device_fimd1.dev,
		.platform_data	= &xyref5260_dp_lcd_data,
	},
};

static struct s3c_fb_pd_win xyref5260_fb_win0 = {
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
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#endif

static void xyref5260_fimd_gpio_setup_24bpp(void)
{
#if defined(CONFIG_S5P_DP)
	gpio_request(EXYNOS5260_GPX0(7), "GPX0");
	s3c_gpio_cfgpin(EXYNOS5260_GPX0(7), S3C_GPIO_SFN(3));
#endif
}

static struct s3c_fb_platdata smdk5260_lcd1_pdata __initdata = {
#if defined(CONFIG_S5P_DP)
	.win[0]		= &xyref5260_fb_win0,
	.win[1]		= &xyref5260_fb_win0,
	.win[2]		= &xyref5260_fb_win0,
	.win[3]		= &xyref5260_fb_win0,
	.win[4]		= &xyref5260_fb_win0,
#else
	.win[0]		= &smdk5260_fb_win0,
	.win[1]		= &smdk5260_fb_win0,
	.win[2]		= &smdk5260_fb_win0,
	.win[3]		= &smdk5260_fb_win0,
	.win[4]		= &smdk5260_fb_win0,
#endif
	.clock_init	= s3c_fb_clock_init,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB | VIDCON0_HIVCLK,
#if defined(CONFIG_S5P_DP)
	.vidcon1	= 0,
	.backlight_off	= s5p_dp_backlight_off,
	.lcd_off	= xyref5260_lcd_off,
#else
	.vidcon1	= VIDCON1_INV_VCLK,
#endif
	.setup_gpio	= xyref5260_fimd_gpio_setup_24bpp,
	.ip_version	= EXYNOS5_813,
};

#ifdef CONFIG_FB_MIPI_DSIM
#define DSIM_L_MARGIN SMDK5260_HBP
#define DSIM_R_MARGIN SMDK5260_HFP_DSIM
#define DSIM_UP_MARGIN SMDK5260_VBP
#define DSIM_LOW_MARGIN SMDK5260_VFP
#define DSIM_HSYNC_LEN SMDK5260_HSP
#define DSIM_VSYNC_LEN SMDK5260_VSW
#define DSIM_WIDTH SMDK5260_XRES
#define DSIM_HEIGHT SMDK5260_YRES

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

#if defined(CONFIG_LCD_MIPI_S6E8AA0)
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

	.p = 4,
	.m = 84,
	.s = 1,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = DPHY_PLL_STABLE_TIME,

	.esc_clk = 8 * 1000000, /* escape clk : 8MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x0fff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &s6e8aa0_mipi_lcd_driver,
};
#endif

#if defined(CONFIG_LCD_MIPI_S6E3FA0)
static struct mipi_dsim_config dsim_info = {
#ifdef CONFIG_FB_I80_COMMAND_MODE
	.e_interface	= DSIM_COMMAND,
#else
	.e_interface	= DSIM_VIDEO,
#endif
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
	.p = 4,
	.m = 91,
	.s = 0,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = DPHY_PLL_STABLE_TIME,

	.esc_clk = 20 * 1000000, /*escape clk : 7MHz */

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
	.clock_init		= mipi_dsi_clock_init,
};
#endif

#ifdef CONFIG_S5P_DP
static struct video_info xyref5260_dp_config = {
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
	gpio_request_one(EXYNOS5260_GPD2(0),
			GPIOF_OUT_INIT_HIGH, "GPD2");
	gpio_free(EXYNOS5260_GPD2(0));
}

static void s5p_dp_backlight_off(void)
{
	usleep_range(97000, 97000);

	/* LED_BACKLIGHT_RESET: GPD2_0 */
	gpio_request_one(EXYNOS5260_GPD2(0),
			GPIOF_OUT_INIT_LOW, "GPD2");
	gpio_free(EXYNOS5260_GPD2(0));
}

static struct s5p_dp_platdata xyref5260_dp_data __initdata = {
	.video_info	= &xyref5260_dp_config,
	.phy_init	= s5p_dp_phy_init,
	.phy_exit	= s5p_dp_phy_exit,
	.backlight_on	= s5p_dp_backlight_on,
	.backlight_off	= s5p_dp_backlight_off,
	.clock_init	= s5p_dp_clock_init,
};
#endif

static struct platform_device *smdk5260_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim1,
#endif
	&s5p_device_fimd1,
#ifdef CONFIG_S5P_DP
	&s5p_device_dp,
	&xyref5260_dp_lcd,
#endif
};

#ifdef CONFIG_BACKLIGHT_PWM
/* LCD Backlight data */
static struct samsung_bl_gpio_info smdk5260_bl_gpio_info = {
	.no = EXYNOS5260_GPB2(0),
	.func = S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data smdk5260_bl_data = {
	.pwm_id = 0,
	.pwm_period_ns = 30000,
};
#endif

void __init exynos5_xyref5260_display_init(void)
{
#ifdef CONFIG_S5P_DP
	int irq;
#endif
	clk_add_alias("sclk_fimd", "exynos5-fb.1", "sclk_fimd1_128_extclkpl",
			&s5p_device_fimd1.dev);
#ifdef CONFIG_FB_MIPI_DSIM
	s5p_dsim1_set_platdata(&dsim_platform_data);
#endif
#ifdef CONFIG_S5P_DP
	s5p_dp_set_platdata(&xyref5260_dp_data);
#endif
	s5p_fimd1_set_platdata(&smdk5260_lcd1_pdata);
#ifdef CONFIG_BACKLIGHT_PWM
	samsung_bl_set(&smdk5260_bl_gpio_info, &smdk5260_bl_data);
#endif
	platform_add_devices(smdk5260_display_devices,
			ARRAY_SIZE(smdk5260_display_devices));

#ifdef CONFIG_S5P_DP
	irq = s5p_register_gpio_interrupt(EXYNOS5260_GPK0(0));
	if (IS_ERR_VALUE(irq)) {
		pr_err("%s: Failed to configure GPK0(0) GPIO\n", __func__);
		return;
	}

	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "sclk_disp_pixel", 267 * MHZ);
#endif
#ifdef CONFIG_FB_MIPI_DSIM
	/* RPLL rate is 300Mhz, 300/5=60Hz */
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "sclk_disp_pixel", 266 * MHZ);
#endif
}
