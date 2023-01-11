/*
 * drivers/video/mmp/panel/mipi_fl10802.c
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

struct fl10802_plat_data {
	struct mmp_panel *panel;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};

#define FL10802_ID 0x0307
#define HSM 0
#define LPM 1

static char cpt_cmd0[] = {0xB9, 0xF1, 0x08, 0x01};
static char cpt_cmd1[] = {0xB1, 0x22, 0x1D, 0x1D, 0x87};
static char cpt_cmd2[] = {0xB2, 0x22};
static char cpt_cmd3[] = {0xB3, 0x01, 0x00, 0x06, 0x06, 0x18, 0x13, 0x39, 0x35};
static char cpt_cmd4[] = {
	0xBA, 0x31, 0x00, 0x44, 0x25,
	0x91, 0x0A, 0x00, 0x00, 0xC1,
	0x00, 0x00, 0x00, 0x0D, 0x02,
	0x4F, 0xB9, 0xEE
};
static char cpt_cmd5[] = {0xE3, 0x09, 0x09, 0x03, 0x03, 0x00};
static char cpt_cmd6[] = {0xB4, 0x02};
static char cpt_cmd7[] = {0xB5, 0x07,  0x07};
static char cpt_cmd8[] = {0xB6, 0x30, 0x30};
static char cpt_cmd9[] = {0xB8, 0x64, 0x28};
static char cpt_cmd10[] = {0xCC, 0x00};
static char cpt_cmd11[] = {0xBC, 0x47};
static char cpt_cmd12[] = {
	0xE9, 0x00, 0x00, 0x0F, 0x03,
	0x6C, 0x0A, 0x8A, 0x10, 0x01,
	0x00, 0x37, 0x13, 0x0A, 0x76,
	0x37, 0x00, 0x00, 0x18, 0x00,
	0x00, 0x00, 0x25, 0x09, 0x80,
	0x40, 0x00, 0x42, 0x60, 0x00,
	0x00, 0x00, 0x09, 0x81, 0x50,
	0x01, 0x53, 0x70, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00
};
static char cpt_cmd13[] = {
	0xEA, 0x94, 0x00, 0x00, 0x00,
	0x08, 0x95, 0x10, 0x07, 0x35,
	0x10, 0x00, 0x00, 0x00, 0x08,
	0x94, 0x00, 0x06, 0x24, 0x00,
	0x00, 0x00, 0x00
};
static char cpt_cmd14[] = {
	0xE0, 0x00, 0x0C, 0x14, 0x0E,
	0x0E, 0x3F, 0x27, 0x31, 0x06,
	0x0E, 0x10, 0x15, 0x16, 0x16,
	0x16, 0x13, 0x17, 0x00, 0x0C,
	0x14, 0x0E, 0x0E, 0x37, 0x27,
	0x31, 0x06, 0x0E, 0x10, 0x15,
	0x16, 0x16, 0x16, 0x13, 0x17
};

static struct mmp_dsi_cmd_desc fl10802_cpt_display_on_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd0), cpt_cmd0},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd1), cpt_cmd1},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd2), cpt_cmd2},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd3), cpt_cmd3},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd4), cpt_cmd4},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd5), cpt_cmd5},

	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd7), cpt_cmd7},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd8), cpt_cmd8},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd9), cpt_cmd9},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd6), cpt_cmd6},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd10), cpt_cmd10},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd11), cpt_cmd11},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd12), cpt_cmd12},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd13), cpt_cmd13},
	{MIPI_DSI_GENERIC_LONG_WRITE, LPM, 0, sizeof(cpt_cmd14), cpt_cmd14},

	{MIPI_DSI_DCS_SHORT_WRITE, LPM, 150, sizeof(exit_sleep), exit_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, LPM, 100, sizeof(display_on), display_on}
};

static char enter_sleep[] = {0x10};
static char display_off[] = {0x28};

static struct mmp_dsi_cmd_desc fl10802_display_off_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 200,
		sizeof(enter_sleep), enter_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(display_off), display_off},
};

static void fl10802_panel_on(struct mmp_panel *panel, int status)
{
	if (status) {
		mmp_panel_dsi_ulps_set_on(panel, 0);
		mmp_panel_dsi_tx_cmd_array(panel, fl10802_cpt_display_on_cmds,
			ARRAY_SIZE(fl10802_cpt_display_on_cmds));
	} else {
		mmp_panel_dsi_tx_cmd_array(panel, fl10802_display_off_cmds,
			ARRAY_SIZE(fl10802_display_off_cmds));
		mmp_panel_dsi_ulps_set_on(panel, 1);
	}
}

#ifdef CONFIG_OF
static void fl10802_panel_power(struct mmp_panel *panel, int skip_on, int on)
{
	static struct regulator *lcd_iovdd, *lcd_avdd;
	int lcd_rst_n, ret = 0;

	pr_debug("%s on=%d skip=%d +\n", __func__, on, skip_on);

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
		usleep_range(20000, 21000);

		if (!skip_on) {
			gpio_direction_output(lcd_rst_n, 1);
			usleep_range(10000, 11000);
			gpio_direction_output(lcd_rst_n, 0);
			usleep_range(10000, 12000);
			gpio_direction_output(lcd_rst_n, 1);
			msleep(120);
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
static void fl10802_panel_power(struct mmp_panel *panel, int skip_on, int on) {}
#endif

static void fl10802_set_status(struct mmp_panel *panel, int status)
{
	struct fl10802_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			fl10802_panel_power(panel, skip_on, 1);
		if (!skip_on)
			fl10802_panel_on(panel, 1);
	} else if (status_is_off(status)) {
		fl10802_panel_on(panel, 0);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			fl10802_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static struct mmp_mode mmp_modes_fl10802[] = {
	[0] = {
		.pixclock_freq = 34651440,
		.refresh = 60,
		.xres = 480,
		.yres = 854,
		.real_xres = 480,
		.real_yres = 854,
		.hsync_len = 10,
		.left_margin = 78,
		.right_margin = 78,
		.vsync_len = 15,
		.upper_margin = 7,
		.lower_margin = 18,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
		.height = 90,
		.width = 50,
	},
};

static int fl10802_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_fl10802;
	return 1;
}

static struct mmp_panel panel_fl10802 = {
	.name = "fl10802",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.is_iovdd = 0,
	.is_avdd = 0,
	.get_modelist = fl10802_get_modelist,
	.set_status = fl10802_set_status,
};

static int fl10802_bl_update_status(struct backlight_device *bl)
{
	struct fl10802_plat_data *data = dev_get_drvdata(&bl->dev);
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

static int fl10802_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static DEFINE_SPINLOCK(bl_lock);
static void fl10802_panel_set_bl(struct mmp_panel *panel, int intensity)
{
	int gpio_bl, bl_level, p_num;
	unsigned long flags;
	/*
	 * FIXME
	 * the initial value of bl_level_last is the
	 * uboot backlight level, it should be aligned.
	 */
	static int bl_level_last = 17;

	gpio_bl = of_get_named_gpio(panel->dev->of_node, "bl_gpio", 0);
	if (gpio_bl < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return;
	}

	if (gpio_request(gpio_bl, "lcd backlight")) {
		pr_err("gpio %d request failed\n", gpio_bl);
		return;
	}

	/*
	 * Brightness is controlled by a series of pulses
	 * generated by gpio. It has 32 leves and level 1
	 * is the brightest. Pull low for 3ms makes
	 * backlight shutdown
	 */
	bl_level = (100 - intensity) * 32 / 100 + 1;

	if (bl_level == bl_level_last)
		goto set_bl_return;

	if (bl_level == 33) {
		/* shutdown backlight */
		gpio_direction_output(gpio_bl, 0);
		goto set_bl_return;
	}

	if (bl_level > bl_level_last)
		p_num = bl_level - bl_level_last;
	else
		p_num = bl_level + 32 - bl_level_last;

	while (p_num--) {
		spin_lock_irqsave(&bl_lock, flags);
		gpio_direction_output(gpio_bl, 0);
		udelay(1);
		gpio_direction_output(gpio_bl, 1);
		spin_unlock_irqrestore(&bl_lock, flags);
		udelay(1);
	}

set_bl_return:
	if (bl_level == 33)
		bl_level_last = 0;
	else
		bl_level_last = bl_level;
	gpio_free(gpio_bl);
}

static const struct backlight_ops fl10802_bl_ops = {
	.get_brightness = fl10802_bl_get_brightness,
	.update_status  = fl10802_bl_update_status,
};

static int fl10802_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct fl10802_plat_data *plat_data;
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
		panel_fl10802.plat_path_name = path_name;

		if (of_find_property(np, "iovdd-supply", NULL))
			panel_fl10802.is_iovdd = 1;

		if (of_find_property(np, "avdd-supply", NULL))
			panel_fl10802.is_avdd = 1;

                if (of_get_named_gpio(np, "bl_gpio", 0) < 0)
                        pr_debug("%s: get bl_gpio failed\n", __func__);
                else
                        plat_data->plat_set_backlight = fl10802_panel_set_bl;

	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			kfree(plat_data);
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_fl10802.plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
	}

	plat_data->panel = &panel_fl10802;
	panel_fl10802.plat_data = plat_data;
	panel_fl10802.dev = &pdev->dev;
	mmp_register_panel(&panel_fl10802);

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!panel_fl10802.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 100;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&fl10802_bl_ops, &props);
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

static int fl10802_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_fl10802);
	kfree(panel_fl10802.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_fl10802_dt_match[] = {
	{ .compatible = "marvell,mmp-fl10802" },
	{},
};
#endif

static struct platform_driver fl10802_driver = {
	.driver		= {
		.name	= "mmp-fl10802",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_fl10802_dt_match),
	},
	.probe		= fl10802_probe,
	.remove		= fl10802_remove,
};

static int fl10802_init(void)
{
	return platform_driver_register(&fl10802_driver);
}
static void fl10802_exit(void)
{
	platform_driver_unregister(&fl10802_driver);
}
module_init(fl10802_init);
module_exit(fl10802_exit);

MODULE_AUTHOR("Yonghai Huang <huangyh@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel FL10802");
MODULE_LICENSE("GPL");
