/*
 * drivers/video/mmp/panel/mipi_JD9161_HSD5.c
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 * Authors:  Yonghai Huang <huangyh@marvell.com>
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

#define SUNLIGHT_ENALBE 1
#define SUNLIGHT_UPDATE_PEROID HZ

#ifdef SUNLIGHT_ENALBE
struct JD9161_slr {
	struct delayed_work work;
	u32 update;
	u32 enable;
	u32 lux_value;
	struct mutex lock;
};
#endif

struct JD9161_plat_data {
	struct mmp_panel *panel;
#ifdef SUNLIGHT_ENALBE
	struct JD9161_slr slr;
#endif
	u32 esd_enable;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static char password[] = {0xbf, 0x91, 0x61, 0xf2};
static char vcom[] = {0xb3, 0x00, 0x75};
static char vcom_r[] = {0xb4, 0x00, 0x75};
static char power_ctr1[] = {0xb8, 0x00, 0xaf, 0x01, 0x00, 0xaf, 0x01};
static char power_ctr2[] = {0xba, 0x3e, 0x23, 0x00};
static char disp_ctr1[] = {0xc3, 0x02};
static char disp_ctr2[] = {0xc4, 0x30, 0x6a};
static char power_ctr3[] = {0xc7, 0x00, 0x01, 0x32, 0x05, 0x65, 0x2b, 0x12,
	0xa5, 0xa5};
static char gamma[] = {0xC8, 0x7E, 0x76, 0x5F, 0x4B, 0x39, 0x25, 0x26, 0x10,
	0x2C, 0x2E, 0x33, 0x57, 0x4D, 0x5E, 0x58, 0x60, 0x5C, 0x56, 0x4E, 0x7E,
	0x76, 0x5F, 0x4B, 0x39, 0x25, 0x26, 0x10, 0x2C, 0x2E, 0x33, 0x57, 0x4D,
	0x5E, 0x58, 0x60, 0x5C, 0x56, 0x4E};
static char gip1[] = {0xD4, 0x1E, 0x1F, 0x17, 0x37, 0x06, 0x04, 0x0A, 0x08,
	0x00, 0x02, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
static char gip2[] = {0xD5, 0x1E, 0x1F, 0x17, 0x37, 0x07, 0x05, 0x0B, 0x09,
	0x01, 0x03, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
static char gip3[] = {0xD6, 0x1F, 0x1E, 0x17, 0x17, 0x07, 0x09, 0x0B, 0x05,
	0x03, 0x01, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
static char gip4[] = {0xD7, 0x1F, 0x1E, 0x17, 0x17, 0x06, 0x08, 0x0A, 0x04,
	0x02, 0x00, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
static char gip5[] = {0xD8, 0x20, 0x00, 0x00, 0x30, 0x03, 0x30, 0x01, 0x02,
	0x00, 0x01, 0x02, 0x06, 0x70, 0x00, 0x00, 0x73, 0x07, 0x06, 0x70, 0x08};

static char gip6[] = {0xD9, 0x00, 0x0A, 0x0A, 0x80, 0x00, 0x00, 0x06, 0x7b,
	0x00, 0x80, 0x00, 0x33, 0x6A, 0x1F, 0x00, 0x00, 0x00, 0x03, 0x7b};

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};

static struct mmp_dsi_cmd_desc JD9161_display_on_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(password), password},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(vcom), vcom},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(vcom_r), vcom_r},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(power_ctr1), power_ctr1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(power_ctr2), power_ctr2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(disp_ctr1), disp_ctr1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(disp_ctr2), disp_ctr2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(power_ctr3), power_ctr3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(gamma), gamma},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(gip1), gip1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(gip2), gip2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(gip3), gip3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(gip4), gip4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(gip5), gip5},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(gip6), gip6},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 120, sizeof(exit_sleep), exit_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0, sizeof(display_on), display_on},
};

static char enter_sleep[] = {0x10};
static char display_off[] = {0x28};

static struct mmp_dsi_cmd_desc JD9161_display_off_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 200,
		sizeof(enter_sleep), enter_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, 0,
		sizeof(display_off), display_off},
};


#ifdef SUNLIGHT_ENALBE
static char slr1[] = {0xE7, 0x01, 0x35, 0x2A};
static char slr2[] = {0x55, 0x70};
static char slr_value1[] = {0xE3, 0x00, 0x00, 0x3F, 0xFF};

static struct mmp_dsi_cmd_desc JD9161_sunlight_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr_value1), slr_value1},
};

static struct mmp_dsi_cmd_desc JD9161_sunlight_onoff_cmds[] = {
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr1), slr1},
	{MIPI_DSI_GENERIC_LONG_WRITE, 0, 0, sizeof(slr2), slr2},
};

static void JD9161_slr_onoff(struct mmp_panel *panel, int onoff)
{
	struct JD9161_plat_data *plat = panel->plat_data;

	if (onoff)
		JD9161_sunlight_onoff_cmds[1].data[1] = 0x70;
	else
		JD9161_sunlight_onoff_cmds[1].data[1] = 0x00;

	mmp_panel_dsi_tx_cmd_array(panel, JD9161_sunlight_onoff_cmds,
		ARRAY_SIZE(JD9161_sunlight_onoff_cmds));

	plat->slr.enable = onoff;
}

static void JD9161_slr_set(struct mmp_panel *panel, int level)
{
	JD9161_sunlight_cmds[0].data[3] = (level & 0xFFFF) >> 8;
	JD9161_sunlight_cmds[0].data[4] = level & 0xFF;

	mmp_panel_dsi_tx_cmd_array(panel, JD9161_sunlight_cmds,
		ARRAY_SIZE(JD9161_sunlight_cmds));
}

static int JD9161_slr_report(struct notifier_block *b, unsigned long state, void *val)
{
	struct mmp_path *path = mmp_get_path("mmp_pnpath");
	struct mmp_panel *panel = (path) ? path->panel : NULL;
	struct JD9161_plat_data *plat = (panel) ? panel->plat_data : NULL;

	if (path->status && plat && !plat->slr.enable)
		return NOTIFY_OK;

	if (path->status && plat && !plat->slr.update) {
		mutex_lock(&plat->slr.lock);
		plat->slr.update = 1;
		plat->slr.lux_value = *((u32 *)val);
		mutex_unlock(&plat->slr.lock);
		schedule_delayed_work(&plat->slr.work, SUNLIGHT_UPDATE_PEROID);
	}

	return NOTIFY_OK;
}

static struct notifier_block JD9161_slr_notifier = {
	.notifier_call = JD9161_slr_report,
};

static inline void JD9161_slr_work(struct work_struct *work)
{
	struct mmp_path *path = mmp_get_path("mmp_pnpath");
	struct mmp_panel *panel = (path) ? path->panel : NULL;
	struct JD9161_plat_data *plat = (panel) ? panel->plat_data : NULL;

	if (panel && plat && plat->slr.update) {
		mutex_lock(&plat->slr.lock);
		JD9161_slr_set(panel, plat->slr.lux_value);
		plat->slr.update = 0;
		mutex_unlock(&plat->slr.lock);
	}
}

ssize_t JD9161_slr_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct mmp_panel *panel = dev_get_drvdata(dev);
	struct JD9161_plat_data *plat_data = panel->plat_data;

	return sprintf(buf, "%d\n", plat_data->slr.enable);
}

ssize_t JD9161_slr_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct mmp_panel *panel = dev_get_drvdata(dev);
	struct JD9161_plat_data *plat_data = panel->plat_data;
	unsigned long val, ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0) {
		dev_err(dev, "strtoul err.\n");
		return ret;
	}

	mutex_lock(&plat_data->slr.lock);
	plat_data->slr.enable = val;
	if (val)
		JD9161_slr_onoff(panel, 1);
	else
		JD9161_slr_onoff(panel, 0);
	mutex_unlock(&plat_data->slr.lock);

	return size;
}

static DEVICE_ATTR(slr, S_IRUGO | S_IWUSR, JD9161_slr_show,
	JD9161_slr_store);
#endif

static void JD9161_set_brightness(struct mmp_panel *panel, int level)
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

static void JD9161_panel_on(struct mmp_panel *panel, int status)
{
	if (status) {
		mmp_panel_dsi_ulps_set_on(panel, 0);
		mmp_panel_dsi_tx_cmd_array(panel, JD9161_display_on_cmds,
			ARRAY_SIZE(JD9161_display_on_cmds));
	} else {
		mmp_panel_dsi_tx_cmd_array(panel, JD9161_display_off_cmds,
			ARRAY_SIZE(JD9161_display_off_cmds));
		mmp_panel_dsi_ulps_set_on(panel, 1);
	}
}

#ifdef CONFIG_OF
static void JD9161_panel_power(struct mmp_panel *panel, int skip_on, int on)
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
static void JD9161_panel_power(struct mmp_panel *panel, int skip_on, int on) {}
#endif

static void JD9161_set_status(struct mmp_panel *panel, int status)
{
	struct JD9161_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			JD9161_panel_power(panel, skip_on, 1);
		if (!skip_on)
			JD9161_panel_on(panel, 1);
	} else if (status_is_off(status)) {
		JD9161_panel_on(panel, 0);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			JD9161_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static void JD9161_esd_onoff(struct mmp_panel *panel, int status)
{
	struct JD9161_plat_data *plat = panel->plat_data;

	if (status) {
		if (plat->esd_enable)
			esd_start(&panel->esd);
	} else {
		if (plat->esd_enable)
			esd_stop(&panel->esd);
	}
}

static void JD9161_esd_recover(struct mmp_panel *panel)
{
	struct mmp_path *path = mmp_get_path(panel->plat_path_name);
	esd_panel_recover(path);
}

static char pkt_size_cmd[] = {0x1};
static char read_status[] = {0xdb};

static struct mmp_dsi_cmd_desc JD9161_read_status_cmds[] = {
	{MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, 0, 0, sizeof(pkt_size_cmd),
		pkt_size_cmd},
	{MIPI_DSI_DCS_READ, 0, 0, sizeof(read_status), read_status},
};

static int JD9161_get_status(struct mmp_panel *panel)
{
	struct mmp_dsi_buf dbuf;
	u8 read_status = 0;
	int ret;

	ret = mmp_panel_dsi_rx_cmd_array(panel, &dbuf,
		JD9161_read_status_cmds,
		ARRAY_SIZE(JD9161_read_status_cmds));
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

static struct mmp_mode mmp_modes_JD9161[] = {
	[0] = {
		.pixclock_freq = 26500000,
		.refresh = 60,
		.real_xres = 480,
		.real_yres = 854,
		.hsync_len = 8,
		.left_margin = 8,
		.right_margin = 136,
		.vsync_len = 2,
		.upper_margin = 12,
		.lower_margin = 6,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
		.height = 110,
		.width = 62,
	},
};

static int JD9161_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_JD9161;
	return 1;
}

static struct mmp_panel panel_JD9161 = {
	.name = "JD9161",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.is_iovdd = 0,
	.is_avdd = 0,
	.get_modelist = JD9161_get_modelist,
	.set_status = JD9161_set_status,
	.set_brightness = JD9161_set_brightness,
	.get_status = JD9161_get_status,
	.panel_esd_recover = JD9161_esd_recover,
	.esd_set_onoff = JD9161_esd_onoff,
};

static int JD9161_bl_update_status(struct backlight_device *bl)
{
	struct JD9161_plat_data *data = dev_get_drvdata(&bl->dev);
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

static int JD9161_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops JD9161_bl_ops = {
	.get_brightness = JD9161_bl_get_brightness,
	.update_status  = JD9161_bl_update_status,
};

static int JD9161_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct JD9161_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
	struct backlight_properties props;
	struct backlight_device *bl;
	int ret;
	u32 esd_enable = 0;

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
		panel_JD9161.plat_path_name = path_name;

		if (of_find_property(np, "iovdd-supply", NULL))
			panel_JD9161.is_iovdd = 1;

		if (of_find_property(np, "avdd-supply", NULL))
			panel_JD9161.is_avdd = 1;
		if (of_property_read_u32(np, "panel_esd", &esd_enable))
			plat_data->esd_enable = 0;
		else
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
		panel_JD9161.plat_path_name = mi->plat_path_name;
		plat_data->plat_set_backlight = mi->plat_set_backlight;
		plat_data->esd_enable = mi->esd_enable;
	}

	plat_data->panel = &panel_JD9161;
	panel_JD9161.plat_data = plat_data;
	panel_JD9161.dev = &pdev->dev;
	mmp_register_panel(&panel_JD9161);
	if (plat_data->esd_enable)
		esd_init(&panel_JD9161);

#ifdef SUNLIGHT_ENALBE
	mutex_init(&plat_data->slr.lock);
	INIT_DEFERRABLE_WORK(&plat_data->slr.work, JD9161_slr_work);
	/*
	 * disable slr feature on default, enable it by "echo 1 >
	 * /sys/devices/platform/JD9161* /slr" in console
	 */
	plat_data->slr.enable = 0;

	/*
	 * notify slr when light sensor is updated, if use different light sensor,
	 * need register notifier accordingly.
	 */
	mmp_register_notifier(&JD9161_slr_notifier);

	platform_set_drvdata(pdev, &panel_JD9161);
	device_create_file(&pdev->dev, &dev_attr_slr);
#endif

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!panel_JD9161.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 100;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&JD9161_bl_ops, &props);
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

static int JD9161_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_JD9161);
	kfree(panel_JD9161.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_JD9161_dt_match[] = {
	{ .compatible = "marvell,mmp-JD9161" },
	{},
};
#endif

static struct platform_driver JD9161_driver = {
	.driver		= {
		.name	= "mmp-JD9161",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_JD9161_dt_match),
	},
	.probe		= JD9161_probe,
	.remove		= JD9161_remove,
};

static int JD9161_init(void)
{
	return platform_driver_register(&JD9161_driver);
}
static void JD9161_exit(void)
{
	platform_driver_unregister(&JD9161_driver);
}
module_init(JD9161_init);
module_exit(JD9161_exit);

MODULE_AUTHOR("Xiaolong Ye <yexl@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel LG4591");
MODULE_LICENSE("GPL");
