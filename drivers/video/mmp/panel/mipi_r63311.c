/*
 * drivers/video/mmp/panel/mipi_r63311.c
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors: Xiaolong Ye<yexl@marvell.com>
 *          Yu Xu <yuxu@marvell.com>
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
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <video/mmp_disp.h>
#include <video/mipi_display.h>

/* set r63311 max brightness level received from user space */
#define R6311_BACKLIGHT_MAX_BRIGHTNESS	100

/* define min brightness level to avoid blank screen for user */
#define R6311_BACKLIGHT_MIN_BRIGHTNESS_DEFAULT	1

struct r63311_plat_data {
	struct mmp_panel *panel;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};
static char manufacturer_cmd_access_protect[] = {0xB0, 0x04};
static char backlight_ctrl[] = {0xCE, 0x00, 0x01, 0x88, 0xC1, 0x00, 0x1E, 0x04};
static char nop[] = {0x0};
static char seq_test_ctrl[] = {0xD6, 0x01};
static char write_display_brightness[] = {0x51, 0x0F, 0xFF};
static char write_ctrl_display[] = {0x53, 0x24};

static struct mmp_dsi_cmd_desc r63311_display_on_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0,
		sizeof(manufacturer_cmd_access_protect),
		manufacturer_cmd_access_protect},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0, sizeof(nop),
		nop},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0, sizeof(nop),
		nop},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(backlight_ctrl),
		backlight_ctrl},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(seq_test_ctrl),
		seq_test_ctrl},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(write_display_brightness),
		write_display_brightness},
	{MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0, 0, sizeof(write_ctrl_display),
		write_ctrl_display},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0, sizeof(display_on),
		display_on},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0, sizeof(exit_sleep),
		exit_sleep},
};

static void r63311_set_brightness(struct mmp_panel *panel, int level)
{
	struct mmp_dsi_cmd_desc brightness_cmds;
	unsigned int modified_level;
	char cmds[3];

	/*Prepare cmds for brightness control*/
	cmds[0] = 0x51;

	/* brightness level smaller than min_brightness is not allowed */
	if (level < panel->min_brightness)
		level = panel->min_brightness;
	/*
	 * input brightness level from upper layer ranges from 0-100.
	 * to convert to HW level (range 0- 4095) we need to perform few steps
	 * 1. scale the level to meet the power constraints according to the
	 *    parameter max_brightness (note the max_brightness <= 100)
	 *    scaled_level = (level * max_brightness) / 100
	 * 2. convert the scaled level to HW level by
	 *    hw_level = (scaled_level * 4095) / 100
	 */
	modified_level = (4095 * panel->max_brightness * level) / 10000;
	cmds[1] = (modified_level & 0xf00) >> 8;
	cmds[2] = modified_level & 0xff;

	/*Prepare dsi commands to send*/
	brightness_cmds.data_type = MIPI_DSI_DCS_LONG_WRITE;
	brightness_cmds.lp = 0;
	brightness_cmds.delay = 0;
	brightness_cmds.length = sizeof(cmds);
	brightness_cmds.data = cmds;

	mmp_panel_dsi_tx_cmd_array(panel, &brightness_cmds, 1);
}

static void r63311_panel_on(struct mmp_panel *panel)
{
	mmp_panel_dsi_tx_cmd_array(panel, r63311_display_on_cmds,
		ARRAY_SIZE(r63311_display_on_cmds));
}

#ifdef CONFIG_OF
static void r63311_panel_power(struct mmp_panel *panel, int skip_on, int on)
{
	static struct regulator *lcd_iovdd;
	static struct regulator *vcc_io_wb;
	int lcd_rst_n, boost_en_5v, ret = 0;

	lcd_rst_n = of_get_named_gpio(panel->dev->of_node, "rst_gpio", 0);
	boost_en_5v = of_get_named_gpio(panel->dev->of_node, "power_gpio", 0);

	if (lcd_rst_n < 0 || boost_en_5v < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return;
	}

	if (gpio_request(lcd_rst_n, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_rst_n);
		return;
	}

	if (gpio_request(boost_en_5v, "5v boost en")) {
		pr_err("gpio %d request failed\n", boost_en_5v);
		gpio_free(lcd_rst_n);
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

	if (panel->is_avdd && (!vcc_io_wb)) {
		vcc_io_wb = regulator_get(panel->dev, "avdd");
		if (IS_ERR(vcc_io_wb)) {
			pr_err("%s regulator get error!\n", __func__);
			ret = -EIO;
			goto regu_lcd_iovdd;
		}
	}

	if (on) {
		if (panel->is_avdd && vcc_io_wb) {
			regulator_set_voltage(vcc_io_wb, 1800000, 1800000);
			ret = regulator_enable(vcc_io_wb);
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

		/* LCD_AVDD+ and LCD_AVDD- ON */
		gpio_direction_output(boost_en_5v, 1);
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

		/* disable AVDD+/- */
		gpio_direction_output(boost_en_5v, 0);

		/* disable LCD_IOVDD 1.8v */
		if (panel->is_iovdd && lcd_iovdd)
			regulator_disable(lcd_iovdd);
		if (panel->is_avdd && vcc_io_wb)
			regulator_disable(vcc_io_wb);
	}

regu_lcd_iovdd:
	gpio_free(lcd_rst_n);
	gpio_free(boost_en_5v);
	if (ret < 0) {
		lcd_iovdd = NULL;
		vcc_io_wb = NULL;
	}
}

static DEFINE_SPINLOCK(bl_lock);
static void r63311_panel_set_bl(struct mmp_panel *panel, int intensity)
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
	pr_debug("%s, intensity:%d\n", __func__, intensity);
}
#else
static void r63311_panel_power(struct mmp_panel *panel, int skip_on, int on) {}
#endif

static void r63311_set_status(struct mmp_panel *panel, int status)
{
	struct r63311_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
#ifdef CONFIG_DDR_DEVFREQ
		if (panel->ddrfreq_qos != PM_QOS_DEFAULT_VALUE)
			pm_qos_update_request(&panel->ddrfreq_qos_req_min,
				panel->ddrfreq_qos);
#endif
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			r63311_panel_power(panel, skip_on, 1);
		if (!skip_on)
			r63311_panel_on(panel);
	} else if (status_is_off(status)) {
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			r63311_panel_power(panel, 0, 0);
#ifdef CONFIG_DDR_DEVFREQ
		if (panel->ddrfreq_qos != PM_QOS_DEFAULT_VALUE)
			pm_qos_update_request(&panel->ddrfreq_qos_req_min,
				PM_QOS_DEFAULT_VALUE);
#endif
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
				status_name(status));
}

static struct mmp_mode mmp_modes_r63311[] = {
	[0] = {
		.pixclock_freq = 148455600,
		.refresh = 57,
		.real_xres = 1080,
		.real_yres = 1920,
		.hsync_len = 2,
		.left_margin = 40,
		.right_margin = 132,
		.vsync_len = 2,
		.upper_margin = 2,
		.lower_margin = 30,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
		.height = 110,
		.width = 62,
	},
};

static int r63311_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_r63311;
	return 1;
}

static struct mmp_panel panel_r63311 = {
	.name = "r63311",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.is_iovdd = 0,
	.is_avdd = 0,
	.get_modelist = r63311_get_modelist,
	.set_status = r63311_set_status,
	.set_brightness = r63311_set_brightness,
};

static int r63311_ls_bl_update_status(struct backlight_device *bl)
{
	struct r63311_plat_data *data = dev_get_drvdata(&bl->dev);
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

static int r63311_ls_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops r63311_ls_bl_ops = {
	.get_brightness = r63311_ls_bl_get_brightness,
	.update_status  = r63311_ls_bl_update_status,
};

static int r63311_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct r63311_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
	struct backlight_properties props;
	struct backlight_device *bl;
	int ret;
	int mipi_backlight = 0;

	plat_data = kzalloc(sizeof(*plat_data), GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF)) {
		ret = of_property_read_string(np, "marvell,path-name",
				&path_name);
		if (ret < 0)
			return ret;
		panel_r63311.plat_path_name = path_name;

		if (of_find_property(np, "iovdd-supply", NULL))
			panel_r63311.is_iovdd = 1;

		if (of_find_property(np, "avdd-supply", NULL))
			panel_r63311.is_avdd = 1;

		if (of_find_property(np, "marvell,mipi-backlight", NULL))
			mipi_backlight = 1;

		if (of_get_named_gpio(np, "bl_gpio", 0) < 0)
			pr_debug("%s: get bl_gpio failed\n", __func__);
		else
			plat_data->plat_set_backlight = r63311_panel_set_bl;
#ifdef CONFIG_DDR_DEVFREQ
		ret = of_property_read_u32(np, "marvell,ddrfreq-qos",
				&panel_r63311.ddrfreq_qos);
		if (ret < 0) {
			panel_r63311.ddrfreq_qos = PM_QOS_DEFAULT_VALUE;
			pr_debug("panel %s didn't has ddrfreq min request\n",
				panel_r63311.name);
		} else {
			panel_r63311.ddrfreq_qos_req_min.name = "lcd";
			pm_qos_add_request(&panel_r63311.ddrfreq_qos_req_min,
					PM_QOS_DDR_DEVFREQ_MIN,
					PM_QOS_DEFAULT_VALUE);
			pr_debug("panel %s has ddrfreq min request: %u\n",
				 panel_r63311.name, panel_r63311.ddrfreq_qos);
		}
		ret = of_property_read_u32(np, "mipi-backlight-max_brightness",
				&panel_r63311.max_brightness);
		if (ret < 0) {
			panel_r63311.max_brightness =
					R6311_BACKLIGHT_MAX_BRIGHTNESS;
			dev_info(&pdev->dev,
			"max brightness not found, set default as %u\n",
					panel_r63311.max_brightness);
		}
		ret = of_property_read_u32(np, "mipi-backlight-min_brightness",
				&panel_r63311.min_brightness);
		if (ret < 0) {
			panel_r63311.min_brightness =
					R6311_BACKLIGHT_MIN_BRIGHTNESS_DEFAULT;
			dev_info(&pdev->dev,
			"min brightness not found, set default as %u\n",
					panel_r63311.min_brightness);
		}
#endif
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_r63311.plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
	}

	panel_r63311.plat_data = plat_data;
	panel_r63311.dev = &pdev->dev;
	plat_data->panel = &panel_r63311;
	mmp_register_panel(&panel_r63311);

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!mipi_backlight)
		return 0;

	if (!panel_r63311.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = R6311_BACKLIGHT_MAX_BRIGHTNESS;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&r63311_ls_bl_ops, &props);
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

static int r63311_remove(struct platform_device *dev)
{
#ifdef CONFIG_DDR_DEVFREQ
	if (panel_r63311.ddrfreq_qos != PM_QOS_DEFAULT_VALUE)
		pm_qos_remove_request(&panel_r63311.ddrfreq_qos_req_min);
#endif
	mmp_unregister_panel(&panel_r63311);
	kfree(panel_r63311.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_r63311_dt_match[] = {
	{ .compatible = "marvell,mmp-r63311" },
	{},
};
#endif

static struct platform_driver r63311_driver = {
	.driver		= {
		.name	= "mmp-r63311",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_r63311_dt_match),
	},
	.probe		= r63311_probe,
	.remove		= r63311_remove,
};

static int r63311_init(void)
{
	return platform_driver_register(&r63311_driver);
}
static void r63311_exit(void)
{
	platform_driver_unregister(&r63311_driver);
}
module_init(r63311_init);
module_exit(r63311_exit);

MODULE_AUTHOR("Xiaolong Ye <yexl@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel R63311");
MODULE_LICENSE("GPL");
