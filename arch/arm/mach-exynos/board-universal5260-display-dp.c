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
#include <linux/lcd.h>

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

#include "board-universal5260.h"

#if defined(CONFIG_S5P_DP)
static void s5p_dp_backlight_on(void);
static void s5p_dp_backlight_off(void);

#define LCD_POWER_OFF_TIME_US	(500 * USEC_PER_MSEC)

static ktime_t lcd_on_time;

static void universal5260_lcd_on(void)
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

	s3c_gpio_setpull(EXYNOS5260_GPD2(1), S3C_GPIO_PULL_NONE);
	gpio_request_one(EXYNOS5260_GPD2(1),
			GPIOF_OUT_INIT_HIGH, "GPD2");
	usleep_range(5000, 6000);
	gpio_free(EXYNOS5260_GPD2(1));
}

static void universal5260_lcd_off(void)
{
	gpio_request_one(EXYNOS5260_GPD2(2),
			GPIOF_OUT_INIT_LOW, "GPD2");
	gpio_free(EXYNOS5260_GPD2(2));
	usleep_range(5000, 6000);

	gpio_request_one(EXYNOS5260_GPD2(1),
			GPIOF_OUT_INIT_LOW, "GPD2");
	usleep_range(5000, 6000);
	gpio_free(EXYNOS5260_GPD2(1));

	lcd_on_time = ktime_add_us(ktime_get_boottime(), LCD_POWER_OFF_TIME_US);
}

static void dp_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	if (power)
		universal5260_lcd_on();
	else
		universal5260_lcd_off();
}

static struct plat_lcd_data universal5260_dp_lcd_data = {
	.set_power	= dp_lcd_set_power,
};

static struct platform_device universal5260_dp_lcd = {
	.name	= "platform-lcd",
	.dev	= {
		.parent		= &s5p_device_fimd1.dev,
		.platform_data	= &universal5260_dp_lcd_data,
	},
};

static struct s3c_fb_pd_win universal5260_fb_win0 = {
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

static void universal5260_fimd_gpio_setup_24bpp(void)
{
	gpio_request(EXYNOS5260_GPX0(7), "GPX0");
	s3c_gpio_cfgpin(EXYNOS5260_GPX0(7), S3C_GPIO_SFN(3));
}

static struct s3c_fb_platdata universal5260_lcd1_pdata __initdata = {
	.win[0]		= &universal5260_fb_win0,
	.win[1]		= &universal5260_fb_win0,
	.win[2]		= &universal5260_fb_win0,
	.win[3]		= &universal5260_fb_win0,
	.win[4]		= &universal5260_fb_win0,
	.default_win	= 0,
	.clock_init	= s3c_fb_clock_init,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
#if defined(CONFIG_S5P_DP)
	.vidcon1	= 0,
	.backlight_off	= s5p_dp_backlight_off,
	.lcd_off	= universal5260_lcd_off,
#else
	.vidcon1	= VIDCON1_INV_VCLK,
#endif
	.setup_gpio	= universal5260_fimd_gpio_setup_24bpp,
	.ip_version	= EXYNOS5_813,
};

#ifdef CONFIG_S5P_DP
static struct video_info universal5260_dp_config = {
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

static struct s5p_dp_platdata universal5260_dp_data __initdata = {
	.video_info	= &universal5260_dp_config,
	.phy_init	= s5p_dp_phy_init,
	.phy_exit	= s5p_dp_phy_exit,
	.backlight_on	= s5p_dp_backlight_on,
	.backlight_off	= s5p_dp_backlight_off,
	.clock_init	= s5p_dp_clock_init,
};
#endif

static struct platform_device *universal5260_display_devices[] __initdata = {
	&s5p_device_fimd1,
#ifdef CONFIG_S5P_DP
	&s5p_device_dp,
	&universal5260_dp_lcd,
#endif
};

#ifdef CONFIG_BACKLIGHT_PWM
/* LCD Backlight data */
static struct samsung_bl_gpio_info universal5260_bl_gpio_info = {
	.no = EXYNOS5420_GPB2(0),
	.func = S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data universal5260_bl_data = {
	.pwm_id = 0,
	.pwm_period_ns = 30000,
};
#endif

void __init exynos5_universal5260_display_init(void)
{
	clk_add_alias("sclk_fimd", "exynos5-fb.1", "sclk_fimd1_128_extclkpl",
			&s5p_device_fimd1.dev);
#ifdef CONFIG_S5P_DP
	s5p_dp_set_platdata(&universal5260_dp_data);
#endif
	s5p_fimd1_set_platdata(&universal5260_lcd1_pdata);
#ifdef CONFIG_BACKLIGHT_PWM
	samsung_bl_set(&universal5260_bl_gpio_info, &universal5260_bl_data);
#endif
	platform_add_devices(universal5260_display_devices,
			ARRAY_SIZE(universal5260_display_devices));
#ifdef CONFIG_S5P_DP
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "sclk_disp_pixel", 267 * MHZ);
#endif
}
