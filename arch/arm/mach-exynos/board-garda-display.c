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
#ifdef CONFIG_FB_MIPI_DSIM
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/regs-mipidsim.h>
#endif

#ifdef CONFIG_FB_S5P_MDNIE
#include <linux/mdnie.h>
#endif

#include <mach/map.h>
#include <mach/spi-clocks.h>

#include <video/platform_lcd.h>

phys_addr_t bootloaderfb_start = 0;
phys_addr_t bootloaderfb_size = 0;
EXPORT_SYMBOL(bootloaderfb_start);
EXPORT_SYMBOL(bootloaderfb_size);
static int __init bootloaderfb_start_setup(char *str)
{
	get_option(&str, &bootloaderfb_start);
#if defined(CONFIG_LCD_MIPI_NT35510)
	bootloaderfb_size = 480 * 800 * 4;
#else
	bootloaderfb_start = 0; // disable for copying bootloaderfb
	bootloaderfb_size = 0;
#endif
	return 1;
}

__setup("s3cfb.bootloaderfb=", bootloaderfb_start_setup);

#if defined(CONFIG_LCD_MIPI_HX8369B)
#if defined(CONFIG_LCD_MIPI_HX8369B)
#define SMDK4270_HBP		49
#define SMDK4270_HFP		68
#define SMDK4270_VBP		9
#define SMDK4270_VFP		10
#define SMDK4270_HSP		17
#define SMDK4270_VSW		6
#define SMDK4270_XRES		480
#define SMDK4270_YRES		800
#define SMDK4270_VIRTUAL_X	480
#define SMDK4270_VIRTUAL_Y	800 * 2
#define SMDK4270_WIDTH		56
#define SMDK4270_HEIGHT		94
#define SMDK4270_MAX_BPP	32
#define SMDK4270_DEFAULT_BPP	24

static struct s3c_fb_pd_win smdk4270_fb_win0 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win1 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win2 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win3 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win4 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static void s5p_mipi_dsi_bl_on_off(unsigned int power)
{
	if (power) {
		printk( KERN_DEBUG "%s.%d - ignore LCD Backlight ON\n", __func__, __LINE__ );
	} else {
		gpio_request_one(EXYNOS4_GPD0(1), GPIOF_OUT_INIT_LOW, "GPD0");
		gpio_set_value(EXYNOS4_GPD0(1), 0);
		gpio_free(EXYNOS4_GPD0(1));
		printk( KERN_INFO "%s.%d : LCD Backlight - OFF\n", __func__, __LINE__ );
	}
}

static void s5p_mipi_dsi_lcd_reset(void)
{
	gpio_request_one(EXYNOS4_GPX2(4), GPIOF_OUT_INIT_HIGH, "GPX2");
/*	udelay(200); */
/*	gpio_set_value(EXYNOS4_GPX2(4), 0); */
	udelay(200);
	gpio_set_value(EXYNOS4_GPX2(4), 1);
	udelay(200);
	gpio_set_value(EXYNOS4_GPX2(4), 0);
	udelay(200);
	gpio_set_value(EXYNOS4_GPX2(4), 1);
	gpio_free(EXYNOS4_GPX2(4));

	/* long delay is needed by Reset Timing Spec 
	 * Mipi-Data is generated after 120ms(min) from Reset-High
	 */
	mdelay(130);
}

static void s5p_mipi_dsi_lcd_power_on_off(unsigned int power)
{
	struct regulator *regulator_vlcd_1v8;
	struct regulator *regulator_vlcd_3v0;
	static int power_checked = 0;
	const int use_vlcd_1v8 = false;

	regulator_vlcd_1v8 = regulator_get(NULL, "vlcd_1v8");
	if (IS_ERR(regulator_vlcd_1v8)) {
		pr_info("%s: failed to get %s\n", __func__, "vlcd_1v8");
		return;
	}
	regulator_vlcd_3v0 = regulator_get(NULL, "vlcd_3v0");
	if (IS_ERR(regulator_vlcd_3v0)) {
		pr_info("%s: failed to get %s\n", __func__, "vlcd_3v0");
		return;
	}

	if (power) {
		pr_info("%s : LCD On : %s VLCD_3V0\n", __func__, use_vlcd_1v8 ? "VLCD_1V8," : "");
		if (use_vlcd_1v8) {
			regulator_enable(regulator_vlcd_1v8);
			msleep(10);
		}
		regulator_enable(regulator_vlcd_3v0);
		power_checked = true;
	} else {
		if (!power_checked) {
			/* at first after booting, regulator_disable make warning because it is not turned-on */
			regulator_enable(regulator_vlcd_1v8);
			regulator_enable(regulator_vlcd_3v0);
			power_checked = true;
		}

		pr_info("%s : LCD off : %s VLCD_3V0\n", __func__, use_vlcd_1v8 ? "VLCD_1V8," : "");
		regulator_disable(regulator_vlcd_3v0);
		if (use_vlcd_1v8)
			regulator_disable(regulator_vlcd_1v8);
		/*LCD RESET low */
		gpio_request_one(EXYNOS4_GPX2(4), GPIOF_OUT_INIT_LOW, "GPX2");
		gpio_free(EXYNOS4_GPX2(4));
	}
	regulator_put(regulator_vlcd_3v0);
	regulator_put(regulator_vlcd_1v8);
	msleep(1);
}

static int mipi_lcd_power_control(struct mipi_dsim_device *dsim, unsigned int power)
{
	/* LCD LDO */
	s5p_mipi_dsi_lcd_power_on_off(power);

	/* Backlight */
	s5p_mipi_dsi_bl_on_off(power);
	/* reset */
	usleep_range(20000, 21000);

	if (power) {
/*		s5p_mipi_dsi_lcd_reset(); */
	} else {
		gpio_request_one(EXYNOS4_GPX2(4), GPIOF_OUT_INIT_HIGH, "GPX2");
		gpio_set_value(EXYNOS4_GPX2(4), 0);
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS4_GPX2(4), 1);
		usleep_range(20000, 21000);
		gpio_free(EXYNOS4_GPX2(4));
	}
	usleep_range(20000, 21000);

	return 1;
}

static void mipi_lcd_set_power(struct plat_lcd_data *pd, unsigned int power)
{
	/* power */
	gpio_request_one(EXYNOS4_GPM1(6), GPIOF_OUT_INIT_LOW, "GPM1");
	if (power) {
		gpio_set_value(EXYNOS4_GPM1(6), 1);
		gpio_free(EXYNOS4_GPM1(6));
	} else {
		gpio_set_value(EXYNOS4_GPM1(6), 0);
		gpio_free(EXYNOS4_GPM1(6));
	}
	gpio_request_one(EXYNOS4_GPM4(5), GPIOF_OUT_INIT_LOW, "GPM4");
	if (power) {
		gpio_set_value(EXYNOS4_GPM4(5), 1);
		gpio_free(EXYNOS4_GPM4(5));
	} else {
		gpio_set_value(EXYNOS4_GPM4(5), 0);
		gpio_free(EXYNOS4_GPM4(5));
	}
	/* Backlight */
	s5p_mipi_dsi_bl_on_off(power);
	/* reset */
	gpio_request_one(EXYNOS4_GPX2(4), GPIOF_OUT_INIT_HIGH, "GPX2");
	usleep_range(20000, 21000);
	if (power) {
		gpio_set_value(EXYNOS4_GPX2(4), 0);
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS4_GPX2(4), 1);
		msleep(50);
		gpio_free(EXYNOS4_GPX2(4));
	} else {
		gpio_set_value(EXYNOS4_GPX2(4), 0);
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS4_GPX2(4), 1);
		usleep_range(20000, 21000);
		gpio_free(EXYNOS4_GPX2(4));
	}
	usleep_range(20000, 21000);
}

static struct plat_lcd_data smdk4270_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device smdk4270_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &smdk4270_mipi_lcd_data,
};
#endif

#ifdef CONFIG_FB_S5P_MDNIE
static struct platform_mdnie_data mdnie_data = {
	.display_type	= -1,
};

struct platform_device mdnie_device = {
	.name		 = "mdnie",
	.id	 = -1,
	.dev		 = {
		.parent		= &s5p_device_fimd0.dev,
		.platform_data = &mdnie_data,
	},
};

static void __init mdnie_device_register(void)
{
	int ret;

	ret = platform_device_register(&mdnie_device);
	if (ret)
		printk(KERN_ERR "failed to register mdnie device: %d\n",
				ret);
}

/* Keep on clock of FIMD during boot time  */
static int keep_lcd_clk(struct device *dev)
{
	struct clk *lcd_clk;

	lcd_clk = clk_get(dev, "lcd");
	if (IS_ERR(lcd_clk)) {
		pr_err("failed to get fimd clock for keep screen on\n");
		return PTR_ERR(lcd_clk);
	}
	clk_enable(lcd_clk);
	return 0;
}
#endif

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 * ---------------------
	 *  0 | MIE/MDNIE
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0210);
#ifdef CONFIG_FB_S5P_MDNIE
	reg &= ~(1 << 23); /*FIFO software Reset*/
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
	reg &= ~(7 << 29);	/*sync*/
	reg &= ~(0x3 << 10); /*LCD0 RGB select*/
	reg &= ~(1 << 1); /*MDNIE path*/
	reg |= (1 << 0); /*MDNIE*/
#else
	reg &= ~(1 << 1);
	reg |= (1 << 1);
#endif
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
}

static void s5p_mipi_dsi_lcd_power_off(void)
{
	s5p_mipi_dsi_bl_on_off(0);
}

static struct s5p_platform_mipi_dsim dsim_platform_data;
static struct s3c_fb_platdata smdk4270_lcd0_pdata __initdata = {
	.win[0]		= &smdk4270_fb_win0,
	.win[1]		= &smdk4270_fb_win1,
	.win[2]		= &smdk4270_fb_win2,
	.win[3]		= &smdk4270_fb_win3,
	.win[4]		= &smdk4270_fb_win4,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_VCLK,
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
	.backlight_ctrl = s5p_mipi_dsi_bl_on_off,
	.dsim_on        = s5p_mipi_dsi_enable_by_fimd,
	.dsim_off       = s5p_mipi_dsi_disable_by_fimd,
	.dsim0_device   = &s5p_device_mipi_dsim0.dev,
	.dsim_pd		= &dsim_platform_data,
	.bootlogo = true,
};

#ifdef CONFIG_FB_MIPI_DSIM
#define DSIM_L_MARGIN	SMDK4270_HBP
#define DSIM_R_MARGIN	SMDK4270_HFP
#define DSIM_UP_MARGIN	SMDK4270_VBP
#define DSIM_LOW_MARGIN	SMDK4270_VFP
#define DSIM_HSYNC_LEN	SMDK4270_HSP
#define DSIM_VSYNC_LEN	SMDK4270_VSW
#define DSIM_WIDTH	SMDK4270_XRES
#define DSIM_HEIGHT	SMDK4270_YRES

#if defined(CONFIG_LCD_MIPI_HX8369B)
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

	/* bit clock 408Mbps (1Lane 204Mhz) */
	.p = 3,
	.m = 102, /* redhawk.choi(HW) request 13.10.12 to 408Mbps. last is 400 */
	.s = 1,

	.esc_clk	= 20 * 1000000, /* escape clk : 20MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0xfff,
	.bta_timeout		= 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout		= 0xffff,	/* lp rx timeout 0 ~ 0xffff */
	.pll_stable_time	= DPHY_PLL_STABLE_TIME,

	.dsim_ddi_pd = &hx8369b_mipi_lcd_driver,
};

static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= DSIM_L_MARGIN,
	.rgb_timing.right_margin	= DSIM_R_MARGIN,
	.rgb_timing.upper_margin	= DSIM_UP_MARGIN,
	.rgb_timing.lower_margin	= DSIM_LOW_MARGIN,
	.rgb_timing.hsync_len		= DSIM_HSYNC_LEN,
	.rgb_timing.vsync_len		= DSIM_VSYNC_LEN,
	/* original : stable_vfp = 1, cmd_allow = 0
	 * non-continuous mode : 2, 6
	 * reduce flickering in booting : stable_vfp =1 */
	.rgb_timing.stable_vfp		= 1,
	.rgb_timing.cmd_allow		= 6,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
};

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim0",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,
	.mipi_power   = mipi_lcd_power_control,
	.init_d_phy		= s5p_dsim_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,

	.backlight_on_off	= s5p_mipi_dsi_bl_on_off,
	.lcd_reset		= s5p_mipi_dsi_lcd_reset,
	.lcd_power_on_off	= s5p_mipi_dsi_lcd_power_on_off,

	.bootlogo = true,
	.fb_activate_vsync = 0,
	.fb_draw_done = 0,
	.common_mode = false,
	.lcd_connected = false,
};
#endif
#endif

static struct platform_device *smdk4270_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&smdk4270_mipi_lcd,
	&s5p_device_mipi_dsim0,
#endif
	&s5p_device_fimd0,
};

static int __init setup_early_bootlogo(char *str)
{
	dsim_platform_data.bootlogo = true;
	smdk4270_lcd0_pdata.bootlogo = true;

	printk( KERN_ERR "%s(mipi) : bootlogo = %d\n", __func__, dsim_platform_data.bootlogo );
	return 0;
}

early_param("bootlogo", setup_early_bootlogo);

static int __init setup_early_bootsleepcheck(char *str)
{
	dsim_platform_data.lcd_connected = true;
	printk(KERN_ERR "%s(mipi) : lcd_connected = %d\n", __func__, dsim_platform_data.lcd_connected);
	return 0;
}

early_param("tspconnect", setup_early_bootsleepcheck);

int get_fps(void)
{
	u64 value_x10;
	u32 div;
	static int value = 0;
	struct fb_videomode *mode = &(smdk4270_fb_win0.win_mode);

	if (value != 0)
		return value;

	/* 24000000L = constant value, 24 = bits per pixel */
	value_x10 = 24000000L / dsim_info.p * dsim_info.m / dsim_info.s / 24 * 10;
	div = mode->left_margin + mode->hsync_len + mode->right_margin + mode->xres;
	div *= mode->upper_margin + mode->vsync_len + mode->lower_margin + mode->yres;
	do_div(value_x10, div);
	value_x10 += 5;		/* rounds up */

	value = (int)value_x10 / 10;

	return value;
}

void __init exynos4_smdk4270_display_init(void)
{
	s5p_dsim0_set_platdata(&dsim_platform_data);

	s5p_fimd0_set_platdata(&smdk4270_lcd0_pdata);

	platform_add_devices(smdk4270_display_devices,
			ARRAY_SIZE(smdk4270_display_devices));

	exynos4_fimd_setup_clock(&s5p_device_fimd0.dev, "sclk_fimd", \
				"mout_mpll_user_top", 100 * MHZ);
#ifdef CONFIG_FB_S5P_MDNIE
	mdnie_device_register();
#endif
}
#else
#if defined(CONFIG_LCD_MIPI_NT35510)
#if defined(CONFIG_FB_I80_COMMAND_MODE)
#define SMDK4270_HBP		1
#define SMDK4270_HFP		1
#define SMDK4270_VBP		1
#define SMDK4270_VFP		1
#define SMDK4270_HSP		1
#define SMDK4270_VSW		1
#define SMDK4270_XRES		480
#define SMDK4270_YRES		800
#define SMDK4270_VIRTUAL_X	480
#define SMDK4270_VIRTUAL_Y	800 * 2
#define SMDK4270_WIDTH		56
#define SMDK4270_HEIGHT		94
#define SMDK4270_MAX_BPP	32
#define SMDK4270_DEFAULT_BPP	24

static struct s3c_fb_pd_win smdk4270_fb_win0 = {
	.win_mode = {
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
		.cs_setup_time  = 1,
		.wr_setup_time  = 0,
		.wr_act_time    = 1,
		.wr_hold_time   = 0,
		.rs_pol         = 0,
		.i80en          = 1,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win1 = {
	.win_mode = {
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
		.cs_setup_time  = 1,
		.wr_setup_time  = 0,
		.wr_act_time    = 1,
		.wr_hold_time   = 0,
		.rs_pol         = 0,
		.i80en          = 1,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win2 = {
	.win_mode = {
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
		.cs_setup_time  = 1,
		.wr_setup_time  = 0,
		.wr_act_time    = 1,
		.wr_hold_time   = 0,
		.rs_pol         = 0,
		.i80en          = 1,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win3 = {
	.win_mode = {
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
		.cs_setup_time  = 1,
		.wr_setup_time  = 0,
		.wr_act_time    = 1,
		.wr_hold_time   = 0,
		.rs_pol         = 0,
		.i80en          = 1,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win4 = {
	.win_mode = {
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
		.cs_setup_time  = 1,
		.wr_setup_time  = 0,
		.wr_act_time    = 1,
		.wr_hold_time   = 0,
		.rs_pol         = 0,
		.i80en          = 1,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};
#else
#define SMDK4270_HBP		8
#define SMDK4270_HFP		8
#define SMDK4270_VBP		8
#define SMDK4270_VFP		27
#define SMDK4270_HSP		2
#define SMDK4270_VSW		2
#define SMDK4270_XRES		480
#define SMDK4270_YRES		800
#define SMDK4270_VIRTUAL_X	480
#define SMDK4270_VIRTUAL_Y	800 * 2
#define SMDK4270_WIDTH		56
#define SMDK4270_HEIGHT		94
#define SMDK4270_MAX_BPP	32
#define SMDK4270_DEFAULT_BPP	24

static struct s3c_fb_pd_win smdk4270_fb_win0 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win1 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win2 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win3 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win4 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};
#endif

static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
		unsigned int power)
{
	return 0;
}

static void s5p_mipi_dsi_bl_on_off(unsigned int power)
{
	gpio_request_one(EXYNOS4_GPD0(1), GPIOF_OUT_INIT_LOW, "GPD0");
	if (power) {
		gpio_set_value(EXYNOS4_GPD0(1), 1);
		gpio_free(EXYNOS4_GPD0(1));
	} else {
		gpio_set_value(EXYNOS4_GPD0(1), 0);
		gpio_free(EXYNOS4_GPD0(1));
	}
}

static int lcd_power_on(struct lcd_device *ld, int enable)

{
	struct regulator *regulator_vlcd_1v8;
	struct regulator *regulator_vlcd_3v0;
	
	regulator_vlcd_1v8 = regulator_get(NULL, "vlcd_1v8");
	if (IS_ERR(regulator_vlcd_1v8)) {
		pr_info("%s: failed to get %s\n", __func__, "vlcd_1v8");
		return PTR_ERR(regulator_vlcd_1v8);
	}
	regulator_vlcd_3v0 = regulator_get(NULL, "vlcd_3v0");
	if (IS_ERR(regulator_vlcd_3v0)) {
		pr_info("%s: failed to get %s\n", __func__, "vlcd_3v0");
		return PTR_ERR(regulator_vlcd_3v0);
	}
		printk("%s : enable =%d\n",__func__, enable);
	if (enable){
		regulator_enable(regulator_vlcd_1v8);
		msleep(10);
		regulator_enable(regulator_vlcd_3v0);
	}else {
		regulator_disable(regulator_vlcd_3v0);
		regulator_disable(regulator_vlcd_1v8);
	/*LCD RESET low*/
		gpio_request_one(EXYNOS4_GPX2(4),GPIOF_OUT_INIT_LOW, "GPX2");
		gpio_free(EXYNOS4_GPX2(4));

	}
	regulator_put(regulator_vlcd_3v0);
	regulator_put(regulator_vlcd_1v8);

	return 0;
}

static int reset_lcd(struct lcd_device *ld)
{
	gpio_request_one(EXYNOS4_GPX2(4), GPIOF_OUT_INIT_HIGH, "GPX2");
	udelay(200);
	gpio_set_value(EXYNOS4_GPX2(4), 0);
	udelay(200);
	gpio_set_value(EXYNOS4_GPX2(4), 1);
	udelay(200);
	gpio_set_value(EXYNOS4_GPX2(4), 0);
	udelay(200);
	gpio_set_value(EXYNOS4_GPX2(4), 1);
	udelay(200);
	gpio_request_one(EXYNOS4_GPX2(4),GPIOF_OUT_INIT_LOW, "GPX2");
	gpio_free(EXYNOS4_GPX2(4));
	return 0;
}

static void mipi_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
}

static struct plat_lcd_data smdk4270_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device smdk4270_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &smdk4270_mipi_lcd_data,
};
#endif

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 * ---------------------
	 *  0 | MIE/MDNIE
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0210);
	reg &= ~(1 << 1);
	reg |= (1 << 1);
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
}

static struct s5p_platform_mipi_dsim dsim_platform_data;
static struct s3c_fb_platdata smdk4270_lcd0_pdata __initdata = {
	.win[0]		= &smdk4270_fb_win0,
	.win[1]		= &smdk4270_fb_win1,
	.win[2]		= &smdk4270_fb_win2,
	.win[3]		= &smdk4270_fb_win3,
	.win[4]		= &smdk4270_fb_win4,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_VCLK,
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
	.backlight_ctrl = s5p_mipi_dsi_bl_on_off,
	.dsim_hs_ctrl        = s5p_mipi_dsi_hs_ctrl_by_fimd,
	.dsim_on        = s5p_mipi_dsi_enable_by_fimd,
	.dsim_off       = s5p_mipi_dsi_disable_by_fimd,
	.dsim_displayon	= s5p_mipi_dsi_displayon_by_fimd,
	.dsim0_device   = &s5p_device_mipi_dsim0.dev,
	.dsim_pd		= &dsim_platform_data,
	.bootlogo = false,
};

static struct lcd_platform_data panel_pdata = {
	.reset = reset_lcd,
	.power_on = lcd_power_on,
	.reset_delay = 20000,
	.power_on_delay = 1000,
};

#ifdef CONFIG_FB_MIPI_DSIM
#define DSIM_L_MARGIN	SMDK4270_HBP
#define DSIM_R_MARGIN	SMDK4270_HFP
#define DSIM_UP_MARGIN	SMDK4270_VBP
#define DSIM_LOW_MARGIN	SMDK4270_VFP
#define DSIM_HSYNC_LEN	SMDK4270_HSP
#define DSIM_VSYNC_LEN	SMDK4270_VSW
#define DSIM_WIDTH	SMDK4270_XRES
#define DSIM_HEIGHT	SMDK4270_YRES

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

	/* bit clock 480Hz */
	.p = 3,  
	.m = 125, 
	.s = 1,

	.esc_clk	= 20 * 1000000, /* escape clk : 20MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0xfff,
	.bta_timeout		= 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout		= 0xffff,	/* lp rx timeout 0 ~ 0xffff */
	.pll_stable_time	= DPHY_PLL_STABLE_TIME,

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

	/* bit clock 480Hz */
	.p = 2,
	.m = 60,
	.s = 1,

	.esc_clk	= 20 * 1000000, /* escape clk : 20MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0xfff,
	.bta_timeout		= 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout		= 0xffff,	/* lp rx timeout 0 ~ 0xffff */
	.pll_stable_time	= DPHY_PLL_STABLE_TIME,

	.dsim_ddi_pd = &nt35510_mipi_lcd_driver,
};
#endif

#if defined(CONFIG_FB_I80_COMMAND_MODE)
static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
	.mipi_ddi_pd			= &panel_pdata,
};
#else
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
	.mipi_ddi_pd			= &panel_pdata,
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

	.backlight_on_off	= s5p_mipi_dsi_bl_on_off,

	.bootlogo = false,
	.fb_activate_vsync = 0,
	.fb_draw_done = 0,
	.common_mode = true,
	.lcd_connected = false,
};
#endif
#endif

static struct platform_device *smdk4270_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&smdk4270_mipi_lcd,
	&s5p_device_mipi_dsim0,
#endif
	&s5p_device_fimd0,
};

static int __init setup_early_bootlogo(char *str)
{
	dsim_platform_data.bootlogo = true;
	smdk4270_lcd0_pdata.bootlogo = true;
	return 0;
}
early_param("bootlogo", setup_early_bootlogo);

static int __init setup_early_factorymode(char *str)
{
	dsim_platform_data.common_mode = false;
	return 0;
}
early_param("bootmode", setup_early_factorymode);

static int __init setup_early_bootsleepcheck(char *str)
{
	dsim_platform_data.lcd_connected= true;
	return 0;
}
early_param("tspconnect", setup_early_bootsleepcheck);

void __init exynos4_smdk4270_display_init(void)
{
	struct resource *res;

	s5p_dsim0_set_platdata(&dsim_platform_data);

	s5p_fimd0_set_platdata(&smdk4270_lcd0_pdata);

	platform_add_devices(smdk4270_display_devices,
			ARRAY_SIZE(smdk4270_display_devices));

#if defined(CONFIG_LCD_MIPI_NT35510)
#if defined(CONFIG_FB_I80_COMMAND_MODE)
	exynos4_fimd_setup_clock(&s5p_device_fimd0.dev, "sclk_fimd", \
				"mout_mpll_user_top", 72 * MHZ); 
#else
	exynos4_fimd_setup_clock(&s5p_device_fimd0.dev, "sclk_fimd", \
				"mout_mpll_user_top", 80 * MHZ);
#endif
#endif
	res = platform_get_resource(&s5p_device_fimd0, IORESOURCE_MEM, 1);
	if (res && bootloaderfb_start) {
		res->start = bootloaderfb_start;
		res->end = res->start + bootloaderfb_size - 1;
		pr_info("bootloader fb located at %8X-%8X\n", res->start,
				res->end);
	} else {
		pr_err("failed to find bootloader fb resource\n");
	}
}
#endif
