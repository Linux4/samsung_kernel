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
#include <linux/lcd.h>

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

#ifdef CONFIG_FB_S5P_MDNIE
#include <linux/mdnie.h>
#endif

#define GPIO_PANEL_ENP	EXYNOS4_GPD0(0)
#define GPIO_PANEL_ENN	EXYNOS4_GPD0(1)
#define GPIO_LCD_ON		EXYNOS4_GPK3(3)
#define GPIO_MLCD_RST	EXYNOS4_GPK3(6)

unsigned int lcdtype;

EXPORT_SYMBOL(lcdtype);
static int __init lcdtype_setup(char *str)
{
	get_option(&str, &lcdtype);
	return 1;
}
__setup("lcdtype=", lcdtype_setup);

phys_addr_t bootloaderfb_start;
EXPORT_SYMBOL(bootloaderfb_start);
phys_addr_t bootloaderfb_size;
EXPORT_SYMBOL(bootloaderfb_size);

static int __init bootloaderfb_start_setup(char *str)
{
	get_option(&str, &bootloaderfb_start);
	bootloaderfb_size = 720 * 1280 * 4;
	return 1;
}
__setup("s3cfb.bootloaderfb=", bootloaderfb_start_setup);

#ifdef CONFIG_FB_S5P_MDNIE
static struct platform_mdnie_data mdnie_data = {
	.display_type	= -1,
};

struct platform_device mdnie_device = {
		.name		 = "mdnie",
		.id	 = -1,
		.dev		 = {
			.parent		= &s5p_device_fimd1.dev,
			.platform_data = &mdnie_data,
	},
};

static void __init mdnie_device_register(void)
{
	int ret;

	ret = platform_device_register(&mdnie_device);
	if (ret)
		printk(KERN_ERR "failed to register mdnie device: %d\n", ret);
}
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
//		gpio_request_one(EXYNOS4_GPK3(4),
//				GPIOF_OUT_INIT_HIGH, "GPK3");
//		usleep_range(5000, 6000);
//		gpio_set_value(EXYNOS4_GPK3(4), 1);
//		usleep_range(5000, 6000);
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

#elif defined(CONFIG_LCD_MIPI_HX8394)
static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
			unsigned int power)
{
	return 1;
}

void __init exynos_keep_disp_clock(struct device *dev)
{
	struct clk *clk = clk_get(dev, "lcd");

	if (IS_ERR(clk)) {
		pr_err("failed to get lcd clock\n");
		return;
	}

	clk_enable(clk);
}

#define SMDK4415_HBP (62)
#define SMDK4415_HFP (58)
#define SMDK4415_HFP_DSIM (58)
#define SMDK4415_HSP (6)
#define SMDK4415_VBP (20)
#define SMDK4415_VFP (9)
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

static struct regulator_bulk_data panel_supplies[] = {
	{ .supply = "vhsif_1.8v_ap" },
	{ .supply = "lcd_vdd_1.8v" },
};

static int lcd_power_on(struct lcd_device *ld, int enable)
{
	int ret;

	ret = regulator_bulk_get(NULL, ARRAY_SIZE(panel_supplies), panel_supplies);
	if (ret) {
		pr_err("%s: failed to get regulators: %d\n", __func__, ret);
		return ret;
	}

	if (enable) {
		regulator_bulk_enable(ARRAY_SIZE(panel_supplies), panel_supplies);
		usleep_range(2000, 2000);
		gpio_request_one(GPIO_PANEL_ENP, GPIOF_OUT_INIT_HIGH, "GPD11");
		gpio_free(GPIO_PANEL_ENP);
		usleep_range(2000, 2000);
		gpio_request_one(GPIO_PANEL_ENN, GPIOF_OUT_INIT_HIGH, "GPD12");
		gpio_free(GPIO_PANEL_ENN);
		usleep_range(2000, 2000);
		gpio_request_one(GPIO_LCD_ON, GPIOF_OUT_INIT_HIGH, "GPC42");
		gpio_free(GPIO_LCD_ON);
		}
	else {
#if defined(GPIO_MLCD_RST)
		usleep_range(1000, 1000);
		gpio_request_one(GPIO_MLCD_RST, GPIOF_OUT_INIT_LOW, "GPD1");
		gpio_free(GPIO_MLCD_RST);
#endif
		gpio_request_one(GPIO_PANEL_ENN, GPIOF_OUT_INIT_LOW, "GPD12");
		gpio_free(GPIO_PANEL_ENN);
		usleep_range(2000, 2000);
		gpio_request_one(GPIO_PANEL_ENP, GPIOF_OUT_INIT_LOW, "GPD11");
		gpio_free(GPIO_PANEL_ENP);
		usleep_range(2000, 2000);
		gpio_request_one(GPIO_LCD_ON, GPIOF_OUT_INIT_LOW, "GPC42");
		gpio_free(GPIO_LCD_ON);

		regulator_bulk_disable(ARRAY_SIZE(panel_supplies), panel_supplies);
	}

	regulator_bulk_free(ARRAY_SIZE(panel_supplies), panel_supplies);

	return 0;
}

static int reset_lcd(struct lcd_device *ld)
{

#if defined(GPIO_MLCD_RST)
	usleep_range(10000, 11000);
	gpio_request_one(GPIO_MLCD_RST, GPIOF_OUT_INIT_HIGH, "GPD1");
	usleep_range(10000, 11000);
	gpio_set_value(GPIO_MLCD_RST, 0);
	usleep_range(10000, 11000);
	gpio_set_value(GPIO_MLCD_RST, 1);
	gpio_free(GPIO_MLCD_RST);
#endif
	return 0;
}

static struct lcd_platform_data panel_pdata = {
	.reset = reset_lcd,
	.power_on = lcd_power_on,
	.reset_delay = 50000,
	.power_on_delay = 0,
};

#endif
static void universal4415_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;

	reg = __raw_readl(S3C_VA_SYS + 0x0214);
#ifdef CONFIG_FB_S5P_MDNIE
	reg &= ~(1<<13);
	reg &= ~(1<<12);
	reg &= ~(3<<10);
	reg |= (1<<0);
	reg &= ~(1<<1);
#else
	reg |= (1<<1);
#endif
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
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
	.dsim0_device   = &s5p_device_mipi_dsim0.dev,
	.dsim_on        = s5p_mipi_dsi_enable_by_fimd,
	.dsim_off       = s5p_mipi_dsi_disable_by_fimd,
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
	.rgb_timing.cmd_allow		= 4,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
#if defined(CONFIG_LCD_MIPI_HX8394)
	.mipi_ddi_pd			= &panel_pdata,
#endif
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
	.s = 0,
#elif defined(CONFIG_LCD_MIPI_AMS480GYXX)
	.auto_vertical_cnt = true,
	.p = 4,
	.m = 84,
	.s = 1,
#elif defined(CONFIG_LCD_MIPI_HX8394)
#ifdef CONFIG_MACH_MEGA2LTE_USA_ATT
	.auto_vertical_cnt = false,
	.p = 4,
	.m = 72,
	.s = 1,
#else
	.auto_vertical_cnt = false,
	.p = 4,
	.m = 82,
	.s = 1,
#endif
#endif

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = DPHY_PLL_STABLE_TIME,

	.esc_clk = 11 * 1000000, /* escape clk : 10MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0xa,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

#if defined(CONFIG_LCD_MIPI_S6E3FA0)
	.dsim_ddi_pd = &s6e3fa0_mipi_lcd_driver,
#elif defined(CONFIG_LCD_MIPI_AMS480GYXX)
	.dsim_ddi_pd = &ams480gyxx_mipi_lcd_driver,
#elif defined(CONFIG_LCD_MIPI_HX8394)
	.dsim_ddi_pd = &hx8394_mipi_lcd_driver,

#endif
};

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim1",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,

	.mipi_power		= mipi_lcd_power_control,
	.init_d_phy		= s5p_dsim_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,
};
#endif

static struct platform_device *smdk4415_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim0,
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

#if defined(CONFIG_FB_LCD_FREQ_SWITCH)
struct platform_device lcdfreq_device = {
		.name		= "lcdfreq",
		.id		= -1,
		.dev		= {
			.parent	= &s5p_device_fimd1.dev,
		},
};

static void __init lcdfreq_device_register(void)
{
	int ret;

	lcdfreq_device.dev.platform_data = (void *)48;
	ret = platform_device_register(&lcdfreq_device);
	if (ret)
		pr_err("failed to register %s: %d\n", __func__, ret);
}
#endif

void __init exynos4_universal4415_display_init(void)
{
	struct resource *res;

#ifdef CONFIG_FB_MIPI_DSIM
	s5p_dsim0_set_platdata(&dsim_platform_data);
#endif
	s5p_fimd1_set_platdata(&smdk4415_lcd1_pdata);
#ifdef CONFIG_BACKLIGHT_PWM
	samsung_bl_set(&smdk4415_bl_gpio_info, &smdk4415_bl_data);
#endif
	platform_add_devices(smdk4415_display_devices,
			ARRAY_SIZE(smdk4415_display_devices));
#ifdef CONFIG_FB_MIPI_DSIM
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_disp_pll", 67 * MHZ);
#endif

	res = platform_get_resource(&s5p_device_fimd1, IORESOURCE_MEM, 1);
	if (res && bootloaderfb_start) {
		res->start = bootloaderfb_start;
		res->end = res->start + bootloaderfb_size - 1;
		pr_info("bootloader fb located at %8X-%8X\n", res->start,
				res->end);
	} else {
		pr_err("failed to find bootloader fb resource\n");
	}

#if !defined(CONFIG_S5P_LCD_INIT)
	exynos_keep_disp_clock(&s5p_device_fimd1.dev);
#endif

#ifdef CONFIG_FB_S5P_MDNIE
	mdnie_device_register();
#endif

#if defined(CONFIG_FB_LCD_FREQ_SWITCH)
	lcdfreq_device_register();
#endif
}
