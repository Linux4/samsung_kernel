/*
 * drivers/video/mmp/panel/mipi_lg4591.c
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Xiaolong Ye <yexl@marvell.com>
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
#include <video/mmp_esd.h>

/* FIX ME: This is no panel ID for panel lg4591,
set 0 for customer reference */
#define MIPI_DCS_NORMAL_STATE_LG4591 0x0

struct lg4591_plat_data {
	struct mmp_panel *panel;
	u32 esd_enable;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};
static char dsi_config[] = {0xe0, 0x43, 0x00, 0x00, 0x00, 0x00};
static char dsi_ctr1[] = {0xB5, 0x34, 0x20, 0x40, 0x00, 0x20};
static char dsi_ctr2[] = {0xB6, 0x04, 0x74, 0x0f, 0x16, 0x13};
static char osc_setting[] = {0xc0, 0x01, 0x08};
static char power_ctr1[] = {0xc1, 0x00};
static char power_ctr3[] = {0xc3, 0x00, 0x09, 0x10, 0x02, 0x00, 0x66, 0x20,
	0x33, 0x00};
static char power_ctr4[] = {0xc4, 0x23, 0x24, 0x17, 0x17, 0x59};
static char pos_gamma_red[] = {0xd0, 0x21, 0x13, 0x67, 0x37, 0x0c, 0x06,
	0x62, 0x23, 0x03};
static char neg_gamma_red[] = {0xd1, 0x32, 0x13, 0x66, 0x37, 0x02, 0x06,
	0x62, 0x23, 0x03};
static char pos_gamma_green[] = {0xd2, 0x41, 0x14, 0x56, 0x37, 0x0c, 0x06,
	0x62, 0x23, 0x03};
static char neg_gamma_green[] = {0xd3, 0x52, 0x14, 0x55, 0x37, 0x02, 0x06,
	0x62, 0x23, 0x03};
static char pos_gamma_blue[] = {0xd4, 0x41, 0x14, 0x56, 0x37, 0x0c, 0x06,
	0x62, 0x23, 0x03};
static char neg_gamma_blue[] = {0xd5, 0x52, 0x14, 0x55, 0x37, 0x02, 0x06,
	0x62, 0x23, 0x03};
static char set_addr_mode[] = {0x36, 0x08};
static char opt2[] = {0xf9, 0x00};
static char power_ctr2_1[] = {0xc2, 0x02};
static char power_ctr2_2[] = {0xc2, 0x06};
static char power_ctr2_3[] = {0xc2, 0x4e};
static char brightness_ctrl[] = {0x51, 0x00};
static char display_ctrl[] = {0x53, 0x24};
static char pwm_ctrl[] = {0xc8, 0x82, 0x85, 0x1, 0x11};

static struct mmp_dsi_cmd_desc lg4591_display_on_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(dsi_config), dsi_config},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(dsi_ctr1), dsi_ctr1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(dsi_ctr2), dsi_ctr2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(osc_setting), osc_setting},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(power_ctr1), power_ctr1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(power_ctr3), power_ctr3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(power_ctr4), power_ctr4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(pos_gamma_red),
		pos_gamma_red},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(neg_gamma_red),
		neg_gamma_red},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(pos_gamma_green),
		pos_gamma_green},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(neg_gamma_green),
		neg_gamma_green},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(pos_gamma_blue),
		pos_gamma_blue},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(neg_gamma_blue),
		neg_gamma_blue},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(set_addr_mode),
		set_addr_mode},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(opt2), opt2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 10, sizeof(power_ctr2_1),
		power_ctr2_1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 10, sizeof(power_ctr2_2),
		power_ctr2_2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 80, sizeof(power_ctr2_3),
		power_ctr2_3},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 10, sizeof(exit_sleep), exit_sleep},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 10, sizeof(opt2), opt2},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(display_ctrl),
		display_ctrl},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(brightness_ctrl),
		brightness_ctrl},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(pwm_ctrl), pwm_ctrl},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0, sizeof(display_on), display_on},
};

static char enter_sleep[] = {0x10};
static char display_off[] = {0x28};

static struct mmp_dsi_cmd_desc lg4591_display_off_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 200,
		sizeof(enter_sleep), enter_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(display_off), display_off},
};

static void lg4591_set_brightness(struct mmp_panel *panel, int level)
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

static void lg4591_panel_on(struct mmp_panel *panel, int status)
{
	if (status) {
		mmp_panel_dsi_ulps_set_on(panel, 0);
		mmp_panel_dsi_tx_cmd_array(panel, lg4591_display_on_cmds,
			ARRAY_SIZE(lg4591_display_on_cmds));
	} else {
		mmp_panel_dsi_tx_cmd_array(panel, lg4591_display_off_cmds,
			ARRAY_SIZE(lg4591_display_off_cmds));
		mmp_panel_dsi_ulps_set_on(panel, 1);
	}
}

#ifdef CONFIG_OF
static void lg4591_panel_power(struct mmp_panel *panel, int skip_on, int on)
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
static void lg4591_panel_power(struct mmp_panel *panel, int skip_on, int on) {}
#endif

static void lg4591_set_status(struct mmp_panel *panel, int status)
{
	struct lg4591_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			lg4591_panel_power(panel, skip_on, 1);
		if (!skip_on)
			lg4591_panel_on(panel, 1);
	} else if (status_is_off(status)) {
		lg4591_panel_on(panel, 0);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			lg4591_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static void lg4591_esd_onoff(struct mmp_panel *panel, int status)
{
	struct lg4591_plat_data *plat = panel->plat_data;

	if (status) {
		if (plat->esd_enable)
			esd_start(&panel->esd);
	} else {
		if (plat->esd_enable)
			esd_stop(&panel->esd);
	}
}

static void lg4591_esd_recover(struct mmp_panel *panel)
{
	struct mmp_path *path = mmp_get_path(panel->plat_path_name);
	esd_panel_recover(path);
}

static char pkt_size_cmd[] = {0x1};
static char read_status[] = {0xdb};

static struct mmp_dsi_cmd_desc lg4591_read_status_cmds[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 0, 0, sizeof(read_status), read_status},
};

static int lg4591_get_status(struct mmp_panel *panel)
{
	struct mmp_dsi_buf dbuf;
	u8 read_status = 0;
	int ret;

	ret = mmp_panel_dsi_rx_cmd_array(panel, &dbuf,
		lg4591_read_status_cmds,
		ARRAY_SIZE(lg4591_read_status_cmds));
	if (ret < 0) {
		pr_err("[ERROR] DSI receive failure!\n");
		return 1;
	}

	read_status = dbuf.data[0];
	if (read_status != MIPI_DCS_NORMAL_STATE_LG4591) {
		pr_err("[ERROR] panel status is 0x%x\n", read_status);
		return 1;
	} else {
		pr_debug("panel status is 0x%x\n", read_status);
	}

	return 0;
}

static struct mmp_mode mmp_modes_lg4591[] = {
	[0] = {
		.pixclock_freq = 69264000,
		.refresh = 60,
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
		.height = 110,
		.width = 62,
	},
};

static int lg4591_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_lg4591;
	return 1;
}

static struct mmp_panel panel_lg4591 = {
	.name = "lg4591",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.is_iovdd = 0,
	.is_avdd = 0,
	.get_modelist = lg4591_get_modelist,
	.set_status = lg4591_set_status,
	.set_brightness = lg4591_set_brightness,
	.get_status = lg4591_get_status,
	.panel_esd_recover = lg4591_esd_recover,
	.esd_set_onoff = lg4591_esd_onoff,
};

static int lg4591_bl_update_status(struct backlight_device *bl)
{
	struct lg4591_plat_data *data = dev_get_drvdata(&bl->dev);
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

static int lg4591_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops lg4591_bl_ops = {
	.get_brightness = lg4591_bl_get_brightness,
	.update_status  = lg4591_bl_update_status,
};

static int lg4591_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct lg4591_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
	struct backlight_properties props;
	struct backlight_device *bl;
	int ret;
	u32 esd_enable;

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
		panel_lg4591.plat_path_name = path_name;

		if (of_find_property(np, "iovdd-supply", NULL))
			panel_lg4591.is_iovdd = 1;

		if (of_find_property(np, "avdd-supply", NULL))
			panel_lg4591.is_avdd = 1;
		if (of_property_read_u32(np, "panel_esd", &esd_enable))
			plat_data->esd_enable = 0;

		plat_data->esd_enable = esd_enable;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			kfree(plat_data);
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_lg4591.plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
		plat_data->esd_enable = mi->esd_enable;
	}

	plat_data->panel = &panel_lg4591;
	panel_lg4591.plat_data = plat_data;
	panel_lg4591.dev = &pdev->dev;
	mmp_register_panel(&panel_lg4591);
	if (plat_data->esd_enable)
		esd_init(&panel_lg4591);

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!panel_lg4591.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 100;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&lg4591_bl_ops, &props);
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

static int lg4591_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_lg4591);
	kfree(panel_lg4591.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_lg4591_dt_match[] = {
	{ .compatible = "marvell,mmp-lg4591" },
	{},
};
#endif

static struct platform_driver lg4591_driver = {
	.driver		= {
		.name	= "mmp-lg4591",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_lg4591_dt_match),
	},
	.probe		= lg4591_probe,
	.remove		= lg4591_remove,
};

static int lg4591_init(void)
{
	return platform_driver_register(&lg4591_driver);
}
static void lg4591_exit(void)
{
	platform_driver_unregister(&lg4591_driver);
}
module_init(lg4591_init);
module_exit(lg4591_exit);

MODULE_AUTHOR("Xiaolong Ye <yexl@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel LG4591");
MODULE_LICENSE("GPL");
