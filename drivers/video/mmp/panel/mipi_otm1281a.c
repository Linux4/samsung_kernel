/*
 * drivers/video/mmp/panel/mipi_otm1281a.c
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

#define OTM1281_SLEEP_OUT_DELAY 100
#define OTM1281_DISP_ON_DELAY   100
#define SWRESET_DELAY   120

struct otm1281_plat_data {
	void (*plat_onoff)(int status);
};

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};
static char reg_shift_00[] = {0x0, 0x0};
static char enable_cmd2[] = {0xff, 0x12, 0x80, 0x1};
static char reg_shift_80[] = {0x0, 0x80};
static char reg_shift_81[] = {0x0, 0x81};
static char reg_shift_82[] = {0x0, 0x82};
static char reg_shift_b0[] = {0x0, 0xb0};
/*
 * increase frame rate to 70 Hz
 * it should be larger than host output(60Hz)
 */
static char reg_osc_adj[] = {0xc1, 0x9};
static char enable_orise[] = {0xff, 0x12, 0x80};
static char reg_shift_b8[] = {0x0, 0xb8};
static char f5_set[] = {0xf5, 0x0c, 0x12};
static char reg_shift_90[] = {0x0, 0x90};
static char pwr_ctrl3[] = {0xc5, 0x10, 0x6F, 0x02, 0x88, 0x1D, 0x15, 0x00,
	0x04};
static char reg_shift_a0[] = {0x0, 0xa0};
static char pwr_ctrl4[] = {0xc5, 0x10, 0x6F, 0x02, 0x88, 0x1D, 0x15, 0x00,
	0x04};
static char pwr_ctrl12[] = {0xc5, 0x20, 0x01, 0x00, 0xb0, 0xb0, 0x00, 0x04,
	0x00};
static char pwr_vdd[] = {0xd8, 0x58, 0x00, 0x58, 0x00};
static char set_vcomdc[] = {0xd9, 0x94};
static char reg_test[] = {0xb0, 0x20};
static char disable_orise[] = {0xff, 0x00, 0x00};
static char disable_cmd2[] = {0xff, 0x00, 0x00, 0x00};
static char swreset[] = {0x1};

static struct mmp_dsi_cmd_desc otm1281_display_on_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 1, SWRESET_DELAY, sizeof(swreset), swreset},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_00), reg_shift_00},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(enable_cmd2), enable_cmd2},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_80), reg_shift_80},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(enable_orise), enable_orise},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_81), reg_shift_81},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_osc_adj), reg_osc_adj},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_82), reg_shift_82},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_osc_adj), reg_osc_adj},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_b8), reg_shift_b8},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(f5_set), f5_set},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_b0), reg_shift_b0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_test), reg_test},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_90), reg_shift_90},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(pwr_ctrl3), pwr_ctrl3},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_a0), reg_shift_a0},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(pwr_ctrl4), pwr_ctrl4},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_80), reg_shift_80},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(pwr_ctrl12), pwr_ctrl12},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_00), reg_shift_00},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(pwr_vdd), pwr_vdd},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_00), reg_shift_00},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(set_vcomdc), set_vcomdc},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_80), reg_shift_80},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(disable_orise),
		disable_orise},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(reg_shift_00), reg_shift_00},
	{MIPI_DSI_GENERIC_LONG_WRITE, 1, 0, sizeof(disable_cmd2), disable_cmd2},

	{MIPI_DSI_DCS_SHORT_WRITE, 1, OTM1281_SLEEP_OUT_DELAY,
		sizeof(exit_sleep), exit_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 1, OTM1281_DISP_ON_DELAY, sizeof(display_on),
		display_on},
};

static void otm1281_panel_on(struct mmp_panel *panel)
{
	mmp_panel_dsi_tx_cmd_array(panel, otm1281_display_on_cmds,
		ARRAY_SIZE(otm1281_display_on_cmds));
}

#ifdef CONFIG_OF
static void otm1281_panel_power(struct mmp_panel *panel, int skip_on, int on)
{
	static struct regulator *lcd_iovdd;
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
	if (!lcd_iovdd) {
		lcd_iovdd = regulator_get(panel->dev, "iovdd");
		if (IS_ERR(lcd_iovdd)) {
			pr_err("%s regulator get error!\n", __func__);
			ret = -EIO;
			goto regu_lcd_iovdd;
		}
	}
	if (on) {
		regulator_set_voltage(lcd_iovdd, 1800000, 1800000);
		ret = regulator_enable(lcd_iovdd);
		if (ret < 0)
			goto regu_lcd_iovdd;
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
		regulator_disable(lcd_iovdd);
	}

regu_lcd_iovdd:
	gpio_free(lcd_rst_n);
	if (ret < 0)
		lcd_iovdd = NULL;
}
#else
static void otm1281_panel_power(struct mmp_panel *panel, int skip_on, int on) {}
#endif
static void otm1281_set_status(struct mmp_panel *panel, int status)
{
	struct otm1281_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);

	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			otm1281_panel_power(panel, skip_on, 1);
		if (!skip_on)
			otm1281_panel_on(panel);
	} else if (status_is_off(status)) {
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			otm1281_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static struct mmp_mode mmp_modes_otm1281[] = {
	[0] = {
		.pixclock_freq = 69273600,
		.refresh = 60,
		.real_xres = 720,
		.real_yres = 1280,
		.hsync_len = 2,
		.left_margin = 20,
		.right_margin = 138,
		.vsync_len = 2,
		.upper_margin = 10,
		.lower_margin = 20,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
		.height = 110,
		.width = 62,
	},
};

static int otm1281_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_otm1281;
	return 1;
}

static struct mmp_panel panel_otm1281 = {
	.name = "otm1281",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.get_modelist = otm1281_get_modelist,
	.set_status = otm1281_set_status,
};

static int otm1281_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct otm1281_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
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
		panel_otm1281.plat_path_name = path_name;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			kfree(plat_data);
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_otm1281.plat_path_name = mi->plat_path_name;
	}
	panel_otm1281.plat_data = plat_data;
	panel_otm1281.dev = &pdev->dev;
	mmp_register_panel(&panel_otm1281);

	return 0;
}

static int otm1281_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_otm1281);
	kfree(panel_otm1281.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_otm1281_dt_match[] = {
	{ .compatible = "marvell,mmp-otm1281" },
	{},
};
#endif

static struct platform_driver otm1281_driver = {
	.driver		= {
		.name	= "mmp-otm1281",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_otm1281_dt_match),
	},
	.probe		= otm1281_probe,
	.remove		= otm1281_remove,
};

static int otm1281_init(void)
{
	return platform_driver_register(&otm1281_driver);
}
static void otm1281_exit(void)
{
	platform_driver_unregister(&otm1281_driver);
}
module_init(otm1281_init);
module_exit(otm1281_exit);

MODULE_AUTHOR("Xiaolong Ye <yexl@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel OTM1281A");
MODULE_LICENSE("GPL");
