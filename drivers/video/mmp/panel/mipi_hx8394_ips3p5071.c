/*
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 *
 * Authors: Qiang Liu <qiangliu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <video/mmp_disp.h>
#include <video/mipi_display.h>

struct hx8394_ips3p5071_plat_data {
	struct mmp_panel *panel;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};
static char set_password[] = {0xB9, 0xFF, 0x83, 0x94};
static char set_lane[] = {0xBA, 0x13};
static char set_power[] = {0xB1, 0x01, 0x00, 0x04, 0x87, 0x03, 0x11,
	0x11, 0x2A, 0x34, 0x3F, 0x3F, 0x47, 0x02, 0x01, 0xE6, 0xE2};
static char set_cyc[] = {0xB4, 0x80, 0x08, 0x32, 0x10, 0x00, 0x32,
	0x15, 0x08, 0x32, 0x10, 0x08, 0x33, 0x04, 0x43, 0x04, 0x37, 0x04,
	0x3F, 0x06, 0x61, 0x61, 0x06};
static char set_rtl_display_reg[] = {0xB2, 0x00, 0xC8, 0x08, 0x04,
	0x00, 0x22};
static char set_gip[] = {0xD5, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00,
	0x01, 0x00, 0xCC, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x88, 0x99,
	0x88, 0x88, 0x88, 0xAA, 0xBB, 0x23, 0x01, 0x67, 0x45, 0x01, 0x23,
	0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x99, 0x88,
	0x88, 0xBB, 0xAA, 0x54, 0x76, 0x10, 0x32, 0x32, 0x10, 0x88, 0x88,
	0x88, 0x88, 0x3C, 0x01};
static char set_tcon[] = {0xC7, 0x00, 0x10, 0x00, 0x10};
static char set_init0[] = {0xBF, 0x06, 0x00, 0x00, 0x04};
static char set_panel[] = {0xCC, 0x09};
static char set_gamma[] = {0xE0, 0x00, 0x04, 0x0A, 0x2D, 0x33,
	0x3F, 0x18, 0x3B, 0x08, 0x0E, 0x0D, 0x10, 0x11, 0x0F, 0x11, 0x0F,
	0x18, 0x00, 0x04, 0x0A, 0x2D, 0x33, 0x3F, 0x18, 0x3B, 0x08, 0x0E,
	0x0D, 0x10, 0x11, 0x0F, 0x11, 0x0F, 0x18, 0x0E, 0x17, 0x08, 0x12,
	0x06, 0x17, 0x08, 0x12};
static char set_vcom[] = {0xB6, 0x24};
static char set_init1[] = {0xD4, 0x32};
static char address_mode[] = {0x36, 0x00};

static struct mmp_dsi_cmd_desc hx8394_ips3p5071_display_on_cmds[] = {
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_password), set_password},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_lane), set_lane},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_power), set_power},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_cyc), set_cyc},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_rtl_display_reg),
		set_rtl_display_reg},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_gip), set_gip},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_tcon), set_tcon},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_init0), set_init0},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_vcom), set_vcom},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_panel), set_panel},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_gamma), set_gamma},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_init1), set_init1},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, 200, sizeof(exit_sleep), exit_sleep},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(address_mode), address_mode},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, 50, sizeof(display_on), display_on},
};

static char enter_sleep[] = {0x10};
static char display_off[] = {0x28};

static struct mmp_dsi_cmd_desc hx8394_ips3p5071_display_off_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 200,
		sizeof(enter_sleep), enter_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(display_off), display_off},
};

static void hx8394_ips3p5071_panel_on(struct mmp_dsi *dsi, int status)
{
	if (status) {
		mmp_dsi_ulps_set_on(dsi, 0);
		mmp_dsi_tx_cmd_array(dsi,
				hx8394_ips3p5071_display_on_cmds,
				ARRAY_SIZE(hx8394_ips3p5071_display_on_cmds));
	} else {
		mmp_dsi_tx_cmd_array(dsi, hx8394_ips3p5071_display_off_cmds,
			ARRAY_SIZE(hx8394_ips3p5071_display_off_cmds));
		mmp_dsi_ulps_set_on(dsi, 1);
	}
}

#ifdef CONFIG_OF
static void hx8394_ips3p5071_panel_power(struct mmp_panel *panel,
		int skip_on, int on)
{
	static struct regulator *lcd_iovdd, *lcd_avdd;
	int lcd_rst_n, ret = 0;

	lcd_rst_n = of_get_named_gpio(panel->dev->of_node, "rst_gpio", 0);
	if (lcd_rst_n < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return;
	}

	if (gpio_request(lcd_rst_n, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_rst_n);
		return;
	}

	/* FIXME: LCD_IOVDD, 1.8V from buck2 */
	if (panel->is_iovdd && (!lcd_iovdd)) {
		lcd_iovdd = regulator_get(panel->dev, "iovdd");
		if (IS_ERR(lcd_iovdd)) {
			pr_err("%s regulator get error!\n", __func__);
			ret = -EIO;
			goto regu_lcd_iovdd;
		}
	}
	if (panel->is_avdd && (!lcd_avdd)) {
		lcd_avdd = regulator_get(panel->dev, "avdd");
		if (IS_ERR(lcd_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			ret = -EIO;
			goto regu_lcd_iovdd;
		}
	}
	if (on) {
		if (panel->is_avdd && lcd_avdd) {
			regulator_set_voltage(lcd_avdd, 2800000, 2800000);
			ret = regulator_enable(lcd_avdd);
			if (ret < 0)
				goto regu_lcd_iovdd;
		}

		if (panel->is_iovdd && lcd_iovdd) {
			regulator_set_voltage(lcd_iovdd, 1800000, 1800000);
			ret = regulator_enable(lcd_iovdd);
			if (ret < 0)
				goto regu_lcd_iovdd;
		}
		usleep_range(10000, 12000);
		if (!skip_on) {
			gpio_direction_output(lcd_rst_n, 0);
			usleep_range(10000, 12000);
			gpio_direction_output(lcd_rst_n, 1);
			usleep_range(10000, 12000);
		}
	} else {
		/* set panel reset */
		gpio_direction_output(lcd_rst_n, 0);

		/* disable LCD_IOVDD 1.8v */
		if (panel->is_iovdd && lcd_iovdd)
			regulator_disable(lcd_iovdd);
		if (panel->is_avdd && lcd_avdd)
			regulator_disable(lcd_avdd);
	}

regu_lcd_iovdd:
	gpio_free(lcd_rst_n);
	if (ret < 0) {
		lcd_iovdd = NULL;
		lcd_avdd = NULL;
	}
}
#else
static void hx8394_ips3p5071_panel_power(struct mmp_panel *panel,
		int skip_on, int on) {}
#endif

static void hx8394_ips3p5071_set_status(struct mmp_panel *panel, int status)
{
	struct hx8394_ips3p5071_plat_data *plat = panel->plat_data;
	struct mmp_dsi *dsi = mmp_panel_to_dsi(panel);
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			hx8394_ips3p5071_panel_power(panel, skip_on, 1);
		if (!skip_on)
			hx8394_ips3p5071_panel_on(dsi, 1);
	} else if (status_is_off(status)) {
		hx8394_ips3p5071_panel_on(dsi, 0);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			hx8394_ips3p5071_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static struct mmp_mode mmp_modes_hx8394_ips3p5071[] = {
	[0] = {
		.pixclock_freq = 69264000,
		.refresh = 60,
		.xres = 720,
		.yres = 1280,
		.real_xres = 720,
		.real_yres = 1280,
		.hsync_len = 2,
		.left_margin = 30,
		.right_margin = 136,
		.vsync_len = 2,
		.upper_margin = 8,
		.lower_margin = 10,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
		.height = 121,
		.width = 68,
	},
};

static int hx8394_ips3p5071_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_hx8394_ips3p5071;
	return 1;
}

static struct mmp_panel panel_hx8394_ips3p5071 = {
	.name = "hx8394_ips3p5071",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.is_iovdd = 0,
	.is_avdd = 0,
	.get_modelist = hx8394_ips3p5071_get_modelist,
	.set_status = hx8394_ips3p5071_set_status,
};

static int hx8394_ips3p5071_bl_update_status(struct backlight_device *bl)
{
	struct hx8394_ips3p5071_plat_data *data = dev_get_drvdata(&bl->dev);
	struct mmp_panel *panel = data->panel;
	int level;

	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		level = bl->props.brightness;
	else
		level = 0;

	/* If there is backlight function of board, use it */
	if (data && data->plat_set_backlight) {
		data->plat_set_backlight(panel, level);
		return 0;
	}

	if (panel && panel->set_brightness)
		panel->set_brightness(panel, level);

	return 0;
}

static int hx8394_ips3p5071_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops hx8394_ips3p5071_bl_ops = {
	.get_brightness = hx8394_ips3p5071_bl_get_brightness,
	.update_status  = hx8394_ips3p5071_bl_update_status,
};

static int hx8394_ips3p5071_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct hx8394_ips3p5071_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
	struct backlight_properties props;
	struct backlight_device *bl;
	struct mmp_panel *panel;
	int ret;

	plat_data = kzalloc(sizeof(*plat_data), GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	panel = &panel_hx8394_ips3p5071;

	if (IS_ENABLED(CONFIG_OF)) {
		ret = of_property_read_string(np, "marvell,path-name",
				&path_name);
		if (ret < 0) {
			kfree(plat_data);
			return ret;
		}
		panel->plat_path_name = path_name;

		if (of_find_property(np, "iovdd-supply", NULL))
			panel->is_iovdd = 1;

		if (of_find_property(np, "avdd-supply", NULL))
			panel->is_avdd = 1;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			kfree(plat_data);
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel->plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
	}

	plat_data->panel = panel;
	panel->plat_data = plat_data;
	panel->dev = &pdev->dev;
	mmp_register_panel(panel);

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!panel->set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 100;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&hx8394_ips3p5071_bl_ops, &props);
	if (IS_ERR(bl)) {
		ret = PTR_ERR(bl);
		dev_err(&pdev->dev, "failed to register lcd-backlight\n");
		return ret;
	}

	bl->props.fb_blank = FB_BLANK_UNBLANK;
	bl->props.power = FB_BLANK_UNBLANK;
	bl->props.brightness = 40;

	return 0;
}

static int hx8394_ips3p5071_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_hx8394_ips3p5071);
	kfree(panel_hx8394_ips3p5071.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_hx8394_ips3p5071_dt_match[] = {
	{ .compatible = "marvell,mmp-hx8394-ips3p5071" },
	{},
};
#endif

static struct platform_driver hx8394_ips3p5071_driver = {
	.driver		= {
		.name	= "mmp-hx8394-ips3p5071",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_hx8394_ips3p5071_dt_match),
	},
	.probe		= hx8394_ips3p5071_probe,
	.remove		= hx8394_ips3p5071_remove,
};

static int hx8394_ips3p5071_init(void)
{
	return platform_driver_register(&hx8394_ips3p5071_driver);
}
static void hx8394_ips3p5071_exit(void)
{
	platform_driver_unregister(&hx8394_ips3p5071_driver);
}
module_init(hx8394_ips3p5071_init);
module_exit(hx8394_ips3p5071_exit);

MODULE_AUTHOR("Qiang Liu <qiangliu@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel HX8394-IPS3P5071");
MODULE_LICENSE("GPL");
