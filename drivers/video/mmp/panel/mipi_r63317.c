/*
 * drivers/video/mmp/panel/mipi_r63317.c
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Xiaolong Ye <yexl@marvell.com>
 *			Guoqing Li <ligq@marvell.com>
 *			Lisa Du <cldu@marvell.com>
 *			Zhou Zhu <zzhu3@marvell.com>
 *			Jing Xiang <jxiang@marvell.com>
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
#include "panel_r63317_param.h"

struct r63317_plat_data {
	struct mmp_panel *panel;
	void (*plat_onoff)(int status);
	void (*plat_set_backlight)(struct mmp_panel *panel, int level);
};

static void r63317_panel_on(struct mmp_panel *panel,int on)
{
	printk("%s %d\n",__func__,on);
	if(on)
		mmp_panel_dsi_tx_cmd_array(panel, r63317_display_on_cmds,
		ARRAY_SIZE(r63317_display_on_cmds));
	else
		mmp_panel_dsi_tx_cmd_array(panel, r63317_display_off_cmds,
		ARRAY_SIZE(r63317_display_off_cmds));
}

/*this panel requires panel init after vido mode on, so skip panel on ,but add panel_start here*/
static void __attribute__((unused)) r63317_panel_start(struct mmp_panel *panel)
{
	usleep_range(100000, 120000);
	printk("%s\n",__func__);
		mmp_panel_dsi_tx_cmd_array(panel, r63317_display_on_cmds,
			ARRAY_SIZE(r63317_display_on_cmds));
}

int orig_bl;
static void r63317_set_brightness(struct mmp_panel *panel, int level)
{
	int lcd_blic_on_gpio;	

	struct mmp_dsi_cmd_desc brightness_cmds;
	char cmds[2];
#if 0
	if(level == orig_bl)
		return;
#endif	
		lcd_blic_on_gpio = of_get_named_gpio(panel->dev->of_node, "lcd_blic_on_gpio", 0);
		if (lcd_blic_on_gpio < 0) {
			pr_err("%s: of_get_named_gpio failed\n", __func__);
			return;
		}
		
		if (gpio_request(lcd_blic_on_gpio, "lcd_blic_on_gpio")) {
				pr_err("gpio %d request failed\n", lcd_blic_on_gpio);
				return;
		}

	/*Prepare cmds for brightness control*/
	cmds[0] = 0x51;	
	cmds[1] = level;

	/*Prepare dsi commands to send*/
	brightness_cmds.data_type = MIPI_DSI_DCS_LONG_WRITE;
	brightness_cmds.lp = 1;
	brightness_cmds.delay = 0;
	brightness_cmds.length = sizeof(cmds);
	brightness_cmds.data = cmds;
	
	if(level == 0)
	{
		/*to turn off bl, first send dsi cmd to then pulldown gpio96*/
		mmp_panel_dsi_tx_cmd_array(panel, &brightness_cmds, 1);
		gpio_direction_output(lcd_blic_on_gpio, 0);
	}
	else
	{	
		/*when turn on bl, first pullup gpio96,then send BL ctrl cmd*/
			gpio_direction_output(lcd_blic_on_gpio, 1);			
			mmp_panel_dsi_tx_cmd_array(panel, &brightness_cmds, 1);
	}
	gpio_free(lcd_blic_on_gpio);	
}

#ifdef CONFIG_OF
static void r63317_panel_power(struct mmp_panel *panel, int skip_on, int on)
{
	static struct regulator *lcd_iovdd;
	int lcd_rst_n,panel_enp,panel_enn, ret = 0;

	lcd_rst_n = of_get_named_gpio(panel->dev->of_node, "rst_gpio", 0);
	if (lcd_rst_n < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return;
	}

	if (gpio_request(lcd_rst_n, "lcd reset gpio")) {
		pr_err("gpio %d request failed\n", lcd_rst_n);
		return;
	}
	
	panel_enp = of_get_named_gpio(panel->dev->of_node, "panel_enp_gpio", 0);
	if (panel_enp < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return;
	}

	if (gpio_request(panel_enp, "panel_enp gpio")) {
		pr_err("gpio %d request failed\n", panel_enp);
		goto panel_enp_fail;
	}
	
	panel_enn = of_get_named_gpio(panel->dev->of_node, "panel_enn_gpio", 0);
	if (panel_enn < 0) {
		pr_err("%s: of_get_named_gpio failed\n", __func__);
		return;
	}

	if (gpio_request(panel_enn, "panel_enn gpio")) {
		pr_err("gpio %d request failed\n", panel_enn);
		goto panel_enn_fail;
	}

	if (!lcd_iovdd) {
		lcd_iovdd = regulator_get(panel->dev, "vdd");
		if (IS_ERR(lcd_iovdd)) {
			pr_err("%s regulator get error!\n", __func__);
			lcd_iovdd = NULL;
			ret = -EIO;
			goto regu_lcd_iovdd;
		}
	}
	if (on) {

		//if(!regulator_is_enabled(lcd_iovdd))
			{
			
			regulator_set_voltage(lcd_iovdd, 1800000, 1800000);		
		
			ret = regulator_enable(lcd_iovdd);
			if (ret < 0){
				printk("ret %d\n",ret);
				pr_err("%s: regu_lcd_iovdd_en_fail\n", __func__);
				goto regu_lcd_iovdd_en_fail;
				}
			usleep_range(1000, 2000);
		}
		
		if (!skip_on) {
			/*turn on vsp & vsn*/
			gpio_direction_output(panel_enp, 1);
			usleep_range(1000, 2000);
			gpio_direction_output(panel_enn, 1);
			usleep_range(1000, 2000);

			/*panel hw reset*/
			gpio_direction_output(lcd_rst_n, 1);
			usleep_range(10, 20);
			gpio_direction_output(lcd_rst_n, 0);
			usleep_range(120000, 140000);
			gpio_direction_output(lcd_rst_n, 1);
			usleep_range(110000, 120000);
		}
	} else {
		/*required panel off sequence
		1) display off sequence
		2)stop video mode
		3) hw power off
		now need to add 2)*/

		/* set panel reset */		
		gpio_direction_output(lcd_rst_n, 0);
		usleep_range(10000, 12000);
		gpio_direction_output(panel_enn, 0);
		usleep_range(1000, 2000);
		gpio_direction_output(panel_enp, 0);
		usleep_range(1000, 2000);
		/* disable LCD_IOVDD 1.8v */
		regulator_disable(lcd_iovdd);
		usleep_range(1000, 2000);
	}
	goto regu_lcd_iovdd;
	 
regu_lcd_iovdd_en_fail:	
	regulator_put(lcd_iovdd);
	lcd_iovdd = NULL;
regu_lcd_iovdd:	
	gpio_free(panel_enn);

panel_enn_fail:
	gpio_free(panel_enp);

panel_enp_fail:	
	gpio_free(lcd_rst_n);	

}
#else
static void r63317_panel_power(struct mmp_panel *panel, int skip_on, int on) {}
#endif

static void r63317_set_status(struct mmp_panel *panel, int status)
{
	struct r63317_plat_data *plat = panel->plat_data;
	int skip_on = (status == MMP_ON_REDUCED);
	
	if (status_is_on(status)) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);
		else
			r63317_panel_power(panel, skip_on, 1);
		if (!skip_on)
			r63317_panel_on(panel,1);
	} else if (status_is_off(status)) {
		r63317_panel_on(panel,0);
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
		else
			r63317_panel_power(panel, 0, 0);
	} else
		dev_warn(panel->dev, "set status %s not supported\n",
					status_name(status));
}

static struct mmp_mode mmp_modes_r63317[] = {
	[0] = {
		.pixclock_freq = 161766720,//69273600,
		.refresh = 60,
		.real_xres = 720,
		.real_yres = 1280,
		.hsync_len = 4,
		.left_margin = 52,
		.right_margin = 116,
		.vsync_len = 2,
		.upper_margin = 6,
		.lower_margin = 8,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
		.height = 110,
		.width = 62,
	},
};

static int r63317_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_r63317;
	return 1;
}

static struct mmp_panel panel_r63317 = {
	.name = "r63317",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.get_modelist = r63317_get_modelist,
	.set_status = r63317_set_status,
//	.panel_start = r63317_panel_start,
	.set_brightness = r63317_set_brightness,
};

static int r63317_bl_update_status(struct backlight_device *bl)
{
	struct r63317_plat_data *data = dev_get_drvdata(&bl->dev);
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

static int r63317_bl_get_brightness(struct backlight_device *bl)
{
	if (bl->props.fb_blank == FB_BLANK_UNBLANK &&
			bl->props.power == FB_BLANK_UNBLANK)
		return bl->props.brightness;

	return 0;
}

static const struct backlight_ops r63317_bl_ops = {
	.get_brightness = r63317_bl_get_brightness,
	.update_status	= r63317_bl_update_status,
};

static int r63317_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct r63317_plat_data *plat_data;
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
		panel_r63317.plat_path_name = path_name;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			kfree(plat_data);
			return -EINVAL;
		}
		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_r63317.plat_path_name = mi->plat_path_name;
	}
	plat_data->panel = &panel_r63317;
	panel_r63317.plat_data = plat_data;
	panel_r63317.dev = &pdev->dev;
	mmp_register_panel(&panel_r63317);

	/*
	 * if no panel or plat associate backlight control,
	 * don't register backlight device here.
	 */
	if (!panel_r63317.set_brightness && !plat_data->plat_set_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 255;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register("lcd-bl", &pdev->dev, plat_data,
			&r63317_bl_ops, &props);
	if (IS_ERR(bl)) {
		ret = PTR_ERR(bl);
		dev_err(&pdev->dev, "failed to register lcd-backlight\n");
		return ret;
	}

	bl->props.fb_blank = FB_BLANK_UNBLANK;
	bl->props.power = FB_BLANK_UNBLANK;
	bl->props.brightness = 0xff;
	orig_bl = bl->props.brightness;

	return 0;
}

static int r63317_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_r63317);
	kfree(panel_r63317.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_r63317_dt_match[] = {
	{ .compatible = "marvell,mmp-r63317" },
	{},
};
#endif

static struct platform_driver r63317_driver = {
	.driver		= {
		.name	= "mmp-r63317",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_r63317_dt_match),
	},
	.probe		= r63317_probe,
	.remove		= r63317_remove,
};

static int r63317_init(void)
{
	return platform_driver_register(&r63317_driver);
}
static void r63317_exit(void)
{
	platform_driver_unregister(&r63317_driver);
}
module_init(r63317_init);
module_exit(r63317_exit);

MODULE_AUTHOR("liping zhang <lipingz@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel r63317");
MODULE_LICENSE("GPL");
