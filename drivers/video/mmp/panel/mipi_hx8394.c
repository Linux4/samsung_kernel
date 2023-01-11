/*
 * drivers/video/mmp/panel/mipi_hx8394.c
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors: Yonghai Huang <huangyh@marvell.com>
 *		Xiaolong Ye <yexl@marvell.com>
 *          Guoqing Li <ligq@marvell.com>
 *          Lisa Du <cldu@marvell.com>
 *          Zhou Zhu <zzhu3@marvell.com>
 *          Jing Xiang <jxiang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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

struct hx8394_plat_data {
	struct mmp_panel *panel;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};
static char set_password[] = {0xb9, 0xff, 0x83, 0x94};
static char set_lane[] = {0xba, 0x13};
static char set_power[] = {0xb1, 0x01, 0x00, 0x07, 0x87, 0x01, 0x11, 0x11, 0x2a, 0x30, 0x3f,
	0x3f, 0x47, 0x12, 0x01, 0xe6, 0xe2};
static char set_cyc[] = {0xb4, 0x80, 0x06, 0x32, 0x10, 0x03, 0x32, 0x15, 0x08, 0x32, 0x10, 0x08,
	0x33, 0x04, 0x43, 0x05, 0x37, 0x04, 0x3F, 0x06, 0x61, 0x61, 0x06};
static char set_rtl_display_reg[] = {0xb2, 0x00, 0xc8, 0x08, 0x04, 0x00, 0x22};
static char set_gip[] = {0xD5, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00, 0xCC, 0x00, 0x00,
	0x00, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x01, 0x67, 0x45, 0x23,
	0x01, 0x23, 0x88, 0x88, 0x88, 0x88};
static char set_tcon[] = {0xC7, 0x00, 0x10, 0x00, 0x10};
static char set_init0[] = {0xBF, 0x06, 0x00, 0x10, 0x04};
static char set_panel[] = {0xCC, 0x09};
static char set_gamma[] = {0xE0, 0x00, 0x04, 0x06, 0x2B, 0x33, 0x3F, 0x11, 0x34, 0x0A, 0x0E,
	0x0D, 0x11, 0x13, 0x11, 0x13, 0x10, 0x17, 0x00, 0x04, 0x06, 0x2B, 0x33, 0x3F, 0x11, 0x34,
	0x0A, 0x0E, 0x0D, 0x11, 0x13, 0x11, 0x13, 0x10, 0x17, 0x0B, 0x17, 0x07, 0x11, 0x0B, 0x17,
	0x07, 0x11};
static char set_dgc[] = {0xC1, 0x01, 0x00, 0x07, 0x0E, 0x15, 0x1D, 0x25, 0x2D, 0x34, 0x3C, 0x42,
	0x49, 0x51, 0x58, 0x5F, 0x67, 0x6F, 0x77, 0x80, 0x87, 0x8F, 0x98, 0x9F, 0xA7, 0xAF, 0xB7,
	0xC1, 0xCB, 0xD3, 0xDD, 0xE6, 0xEF, 0xF6, 0xFF, 0x16, 0x25, 0x7C, 0x62, 0xCA, 0x3A, 0xC2,
	0x1F, 0xC0, 0x00, 0x07, 0x0E, 0x15, 0x1D, 0x25, 0x2D, 0x34, 0x3C, 0x42, 0x49, 0x51, 0x58,
	0x5F, 0x67, 0x6F, 0x77, 0x80, 0x87, 0x8F, 0x98, 0x9F, 0xA7, 0xAF, 0xB7, 0xC1, 0xCB, 0xD3,
	0xDD, 0xE6, 0xEF, 0xF6, 0xFF, 0x16, 0x25, 0x7C, 0x62, 0xCA, 0x3A, 0xC2, 0x1F, 0xC0, 0x00,
	0x07, 0x0E, 0x15, 0x1D, 0x25, 0x2D, 0x34, 0x3C, 0x42, 0x49, 0x51, 0x58, 0x5F, 0x67, 0x6F,
	0x77, 0x80, 0x87, 0x8F, 0x98, 0x9F, 0xA7, 0xAF, 0xB7, 0xC1, 0xCB, 0xD3, 0xDD, 0xE6, 0xEF,
	0xF6, 0xFF, 0x16, 0x25, 0x7C, 0x62, 0xCA, 0x3A, 0xC2, 0x1F, 0xC0};
static char set_vcom[] = {0xB6, 0x0C};
static char set_init1[] = {0xD4, 0x32};
static char brightness_ctrl[] = {0x51, 0x2d};
static char display_ctrl[] = {0x53, 0x24};
static char set_cabc[] = {0xC9, 0x0F, 0x2, 0x1E, 0x1E, 0x00, 0x00, 0x00, 0x01, 0x3E, 0x00, 0x00};

static struct mmp_dsi_cmd_desc hx8394_display_on_cmds[] = {
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_password), set_password},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_lane), set_lane},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_power), set_power},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_cyc), set_cyc},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_rtl_display_reg), set_rtl_display_reg},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_gip), set_gip},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_tcon), set_tcon},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_init0), set_init0},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_panel), set_panel},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_gamma), set_gamma},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_dgc), set_dgc},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_vcom), set_vcom},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_init1), set_init1},
	{MIPI_DSI_DCS_LONG_WRITE, 1, 0, sizeof(set_cabc), set_cabc},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, 200, sizeof(exit_sleep), exit_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 1, 0, sizeof(display_ctrl), display_ctrl},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 1, 0, sizeof(brightness_ctrl), brightness_ctrl},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, 50, sizeof(display_on), display_on},
};

static char enter_sleep[] = {0x10};
static char display_off[] = {0x28};

static struct mmp_dsi_cmd_desc hx8394_display_off_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 200,
		sizeof(enter_sleep), enter_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(display_off), display_off},
};

static void hx8394_set_brightness(struct mmp_panel *panel, int level)
{
	struct mmp_dsi_cmd_desc brightness_cmds;
	char cmds[2];

	/*Prepare cmds for brightness control*/
	cmds[0] = 0x51;
	/* birghtness 1~4 is too dark, add 5 to correctness */
	if (level)
		level += 5;
	cmds[1] = level;

	/*Prepare dsi commands to send*/
	brightness_cmds.data_type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
	brightness_cmds.lp = 0;
	brightness_cmds.delay = 0;
	brightness_cmds.length = sizeof(cmds);
	brightness_cmds.data = cmds;

	mmp_panel_dsi_tx_cmd_array(panel, &brightness_cmds, 1);
}

static void hx8394_panel_on(struct mmp_panel *panel, int status)
{
	if (status) {
		mmp_panel_dsi_ulps_set_on(panel, 0);
		mmp_panel_dsi_tx_cmd_array(panel, hx8394_display_on_cmds,
			ARRAY_SIZE(hx8394_display_on_cmds));
	} else {
		mmp_panel_dsi_tx_cmd_array(panel, hx8394_display_off_cmds,
			ARRAY_SIZE(hx8394_display_off_cmds));
		mmp_panel_dsi_ulps_set_on(panel, 1);
	}
}

#ifdef CONFIG_OF
static void hx8394_panel_power(struct mmp_panel *panel, int skip_on, int on)
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
static void hx8394_panel_power(struct mmp_panel *panel, int skip_on, int on) {}
#endif

static void hx8394_set_status(struct mmp_panel *panel, int status)
{
	struct hx8394_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			hx8394_panel_power(panel, skip_on, 1);
		if (!skip_on)
			hx8394_panel_on(panel, 1);
	} else if (status_is_off(status)) {
		hx8394_panel_on(panel, 0);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			hx8394_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static struct mmp_mode mmp_modes_hx8394[] = {
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
		.lower_margin = 30,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
		.height = 110,
		.width = 62,
	},
};

static int hx8394_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_hx8394;
	return 1;
}

static struct mmp_panel panel_hx8394 = {
	.name = "hx8394",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.is_iovdd = 0,
	.is_avdd = 0,
	.get_modelist = hx8394_get_modelist,
	.set_status = hx8394_set_status,
	.set_brightness = hx8394_set_brightness,
};

static int hx8394_bl_update_status(struct backlight_device *bl)
{
	struct hx8394_plat_data *data = dev_get_drvdata(&bl->dev);
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

static int hx8394_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops hx8394_bl_ops = {
	.get_brightness = hx8394_bl_get_brightness,
	.update_status  = hx8394_bl_update_status,
};

static int hx8394_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct hx8394_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
	struct backlight_properties props;
	struct backlight_device *bl;
	int ret;

	plat_data = kzalloc(sizeof(*plat_data), GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF)) {
		ret = of_property_read_string(np, "marvell,path-name",
				&path_name);
		if (ret < 0) {
			kfree(plat_data);
			return ret;
		}
		panel_hx8394.plat_path_name = path_name;

		if (of_find_property(np, "iovdd-supply", NULL))
			panel_hx8394.is_iovdd = 1;

		if (of_find_property(np, "avdd-supply", NULL))
			panel_hx8394.is_avdd = 1;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			kfree(plat_data);
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_hx8394.plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
	}

	plat_data->panel = &panel_hx8394;
	panel_hx8394.plat_data = plat_data;
	panel_hx8394.dev = &pdev->dev;
	mmp_register_panel(&panel_hx8394);

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!panel_hx8394.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 100;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&hx8394_bl_ops, &props);
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

static int hx8394_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_hx8394);
	kfree(panel_hx8394.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_hx8394_dt_match[] = {
	{ .compatible = "marvell,mmp-hx8394" },
	{},
};
#endif

static struct platform_driver hx8394_driver = {
	.driver		= {
		.name	= "mmp-hx8394",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_hx8394_dt_match),
	},
	.probe		= hx8394_probe,
	.remove		= hx8394_remove,
};

static int hx8394_init(void)
{
	return platform_driver_register(&hx8394_driver);
}
static void hx8394_exit(void)
{
	platform_driver_unregister(&hx8394_driver);
}
module_init(hx8394_init);
module_exit(hx8394_exit);

MODULE_AUTHOR("Yonghai Huang <huangyh@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel HX8394");
MODULE_LICENSE("GPL");
