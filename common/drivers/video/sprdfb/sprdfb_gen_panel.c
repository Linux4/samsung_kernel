/* drivers/video/mmp/panel/mmp-dsi-panel.c
 *
 * Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/lcd.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <mach/i2c-sprd.h>
#include <linux/backlight.h>
#include <linux/platform_data/gen-panel.h>
#include "sprdfb.h"
#include "sprdfb_panel.h"
#ifdef CONFIG_LCD_ESD_RECOVERY
#include "esd_detect.h"
#endif
#include "sprdfb_dispc_reg.h"
#include <mach/pinmap.h>

static struct i2c_client *g_client;
static int32_t sprd_panel_init(struct panel_spec *self);
static int32_t sprd_panel_enter_sleep(struct panel_spec *self,
		uint8_t is_sleep);
static int32_t sprd_panel_start(struct panel_spec *self);
static int32_t sprd_set_rst_mode(uint32_t val);
static int32_t ili9486s1_backlight_ctrl(struct panel_spec *self, uint16_t bl_on);


typedef struct {
	uint32_t reg;
	uint32_t val;
} panel_pinmap_t;

panel_pinmap_t panel_rstpin_map[] = {
	{REG_PIN_LCD_RSTN, BITS_PIN_DS(1)|BITS_PIN_AF(0)|BIT_PIN_WPU|BIT_PIN_WPU|BIT_PIN_SLP_OE},
	{REG_PIN_LCD_RSTN, BITS_PIN_DS(1)|BITS_PIN_AF(3)|BIT_PIN_WPU|BIT_PIN_WPU|BIT_PIN_SLP_OE},
};

static struct panel_operations sprd_panel_ops = {
	.panel_init = sprd_panel_init,
	.panel_enter_sleep = sprd_panel_enter_sleep,
	.panel_set_start = sprd_panel_start,
	.panel_pin_init = sprd_set_rst_mode,	
#ifdef CONFIG_FB_BL_EVENT_CTRL
	.panel_set_brightness = ili9486s1_backlight_ctrl,
#endif	
};

#ifdef CONFIG_LCD_ESD_RECOVERY
struct esd_det_info sprd_panel_esd_info;
#endif
static struct timing_rgb sprd_panel_timing;
static struct info_mipi sprd_panel_mipi_info = {
	.timing = &sprd_panel_timing,
};
struct panel_spec sprd_panel_spec = {
	.ops = &sprd_panel_ops,
	.info.mipi = &sprd_panel_mipi_info,
#ifdef CONFIG_LCD_ESD_RECOVERY
	.esd_info = &sprd_panel_esd_info,
#endif
};
struct panel_cfg sprd_panel_cfg = {
	.panel = &sprd_panel_spec,
};

static int32_t sprd_set_rst_mode(uint32_t val)
{
	int i = 0;

	if(sprd_panel_spec.rst_mode){
		if (val){
			panel_rstpin_map[0].val = sci_glb_read(CTL_PIN_BASE+REG_PIN_LCD_RSTN, 0xffffffff);
			i = 1;
		}else
			i = 0;

		sci_glb_write(CTL_PIN_BASE + panel_rstpin_map[i].reg,  panel_rstpin_map[i].val, 0xffffffff);
	}
	return 0;
}

static inline struct lcd *panel_to_lcd(const struct panel_spec *panel)
{
	return (struct lcd *)panel->plat_data;
}

static inline struct panel_spec *lcd_to_panel(const struct lcd *lcd)
{
	return (struct panel_spec *)lcd->pdata;
}

void debug_printout(void)
{
	pr_info("[gen_panel] hfp %d\n", sprd_panel_timing.hfp);
	pr_info("[gen_panel] hbp %d\n", sprd_panel_timing.hbp);
	pr_info("[gen_panel] hsync %d\n", sprd_panel_timing.hsync);
	pr_info("[gen_panel] vfp %d\n", sprd_panel_timing.vfp);
	pr_info("[gen_panel] vbp %d\n", sprd_panel_timing.vbp);
	pr_info("[gen_panel] vsync %d\n", sprd_panel_timing.vsync);

	pr_info("[gen_panel] work_mode %d\n", sprd_panel_mipi_info.work_mode);
	pr_info("[gen_panel] video_bus_width %d\n",
			sprd_panel_mipi_info.video_bus_width);
	pr_info("[gen_panel] lan_number %d\n", sprd_panel_mipi_info.lan_number);
	pr_info("[gen_panel] phy_feq %d\n", sprd_panel_mipi_info.phy_feq);
	pr_info("[gen_panel] h_sync_pol %d\n", sprd_panel_mipi_info.h_sync_pol);
	pr_info("[gen_panel] v_sync_pol %d\n", sprd_panel_mipi_info.v_sync_pol);
	pr_info("[gen_panel] de_pol %d\n", sprd_panel_mipi_info.de_pol);
	pr_info("[gen_panel] te_pol %d\n", sprd_panel_mipi_info.te_pol);
	pr_info("[gen_panel] color_mode_pol %d\n",
			sprd_panel_mipi_info.color_mode_pol);
	pr_info("[gen_panel] shut_down_pol %d\n",
			sprd_panel_mipi_info.shut_down_pol);

	pr_info("[gen_panel] width %d\n", sprd_panel_spec.width);
	pr_info("[gen_panel] height %d\n", sprd_panel_spec.height);
	pr_info("[gen_panel] fps %d\n", sprd_panel_spec.fps);
	pr_info("[gen_panel] type %d\n", sprd_panel_spec.type);
	pr_info("[gen_panel] direction %d\n", sprd_panel_spec.direction);
	pr_info("[gen_panel] suspend_mode %d\n",
			sprd_panel_spec.suspend_mode);

	pr_info("[gen_panel] dev_id %d\n", sprd_panel_cfg.dev_id);
	pr_info("[gen_panel] lcd_id %x\n", sprd_panel_cfg.lcd_id);
	pr_info("[gen_panel] name %s\n", sprd_panel_cfg.lcd_name);
	pr_info("[gen_panel] rst_mode %d\n", sprd_panel_spec.rst_mode);

	return;
}

static void gpmode_to_sprdmode(struct lcd *gplcd)
{
	struct gen_panel_mode *gpmode = &gplcd->mode;

	sprd_panel_timing.hfp = gpmode->right_margin;
	sprd_panel_timing.hbp = gpmode->left_margin;
	sprd_panel_timing.hsync = gpmode->hsync_len;
	sprd_panel_timing.vfp = gpmode->lower_margin;
	sprd_panel_timing.vbp = gpmode->upper_margin;
	sprd_panel_timing.vsync = gpmode->vsync_len;

	sprd_panel_spec.width = gpmode->xres;
	sprd_panel_spec.height = gpmode->yres;
	sprd_panel_spec.display_width = gpmode->width;
	sprd_panel_spec.display_height = gpmode->height;
	sprd_panel_spec.fps = gpmode->refresh;

	sprd_panel_cfg.lcd_id = gplcd->id;
	sprd_panel_cfg.lcd_name = gplcd->panel_name;

	debug_printout();
}
#if defined(CONFIG_MACH_YOUNG23GDTV) || defined(CONFIG_MACH_J13G)
static inline int sprd_panel_dsi_tx_cmd_array(const struct lcd *lcd,
		const void *cmds, const int count)
{
	struct panel_spec *panel = lcd_to_panel(lcd);
	struct gen_cmd_desc *desc = (struct gen_cmd_desc *)cmds;
	int i, ret,retry=3;

	mipi_dcs_write_t mipi_dcs_write =
		panel->info.mipi->ops->mipi_dcs_write;
	mipi_set_cmd_mode_t mipi_set_cmd_mode =
		panel->info.mipi->ops->mipi_set_cmd_mode;
	mipi_eotp_set_t mipi_eotp_set =
		panel->info.mipi->ops->mipi_eotp_set;

	for (i = 0; i < count; i++) {
		do {
			ret = mipi_dcs_write(desc[i].data, desc[i].length);
		} while (retry-- && ret < 0);

		udelay(40);
		if (unlikely(ret)) {
			pr_err("%s, dsi write failed\n", __func__);
			pr_err("%s, idx:%d, data:0x%02X, len:%d\n", __func__, i,
					desc[i].data[0], desc[i].length);
			return ret;
		}
		if (desc[i].delay)
			msleep(desc[i].delay);
	}
	return count;
}
#else
static inline int sprd_panel_dsi_tx_cmd_array(const struct lcd *lcd,
		const void *cmds, const int count)
{
	struct panel_spec *panel = lcd_to_panel(lcd);
	struct gen_cmd_desc *desc = (struct gen_cmd_desc *)cmds;
	int i, ret;

	mipi_write_t mipi_write =
		panel->info.mipi->ops->mipi_write;

	for (i = 0; i < count; i++) {
		ret = mipi_write(desc[i].data_type,
				desc[i].data, desc[i].length);
		if (unlikely(ret)) {
			pr_err("%s, dsi write failed\n", __func__);
			pr_err("%s, idx:%d, data:0x%02X, len:%d\n", __func__, i,
					desc[i].data[0], desc[i].length);
			return ret;
		}
		if (desc[i].delay)
			msleep(desc[i].delay);
	}

	return count;
}
#endif

static inline int sprd_panel_i2c_tx_cmd_array(const struct lcd *lcd,
		const void *cmds, const int count)
{
	struct gen_cmd_desc *desc = (struct gen_cmd_desc *)cmds;
	int i, ret, retry = 3;

	for (i = 0; i < count; i++) {
		do {
			ret = i2c_master_send(g_client, desc[i].data,
					desc[i].length);
		} while (retry-- && ret < 0);
		if (unlikely(ret < 0)) {
			pr_err("%s, i2c write failed\n", __func__);
			pr_err("%s, idx:%d, data:0x%02X, len:%d\n", __func__, i,
					desc[i].data[0], desc[i].length);
			return ret;
		}
		if (desc[i].delay)
			msleep(desc[i].delay);
	}

	return count;
}

static inline int sprd_panel_rx_cmd_array(const struct lcd *lcd,
		u8 *buf, const void *cmds,
		const int count)
{
	return count;
}

#if CONFIG_OF
int sprd_panel_parse_dt(const struct device_node *np)
{
	u32 val;

	if (!np)
		return -EINVAL;

	/*
	   of_property_read_u32(np, "gen-panel-id", &val);
	   sprd_panel_cfg.lcd_id = val;
	   */
	of_property_read_u32(np, "gen-panel-dev-id", &val);
	sprd_panel_cfg.dev_id = val;
	of_property_read_u32(np, "gen-panel-type", &val);
	sprd_panel_spec.type = val;
	/*
	   of_property_read_u32(np, "gen-panel-refresh", &val);
	   sprd_panel_spec.fps = val;
	   of_property_read_u32(np, "gen-panel-xres", &val);
	   sprd_panel_spec.width = val;
	   of_property_read_u32(np, "gen-panel-yres", &val);
	   sprd_panel_spec.height = val;
	   of_property_read_u32(np, "gen-panel-width", &val);
	   sprd_panel_spec.display_width = val;
	   of_property_read_u32(np, "gen-panel-height", &val);
	   sprd_panel_spec.display_height = val;
	   */
	of_property_read_u32(np, "gen-panel-work-mode", &val);
	sprd_panel_mipi_info.work_mode = val;
	of_property_read_u32(np, "gen-panel-direction", &val);
	sprd_panel_spec.direction = val;
	of_property_read_u32(np, "gen-panel-suspend-mode", &val);
	sprd_panel_spec.suspend_mode = val;
	of_property_read_u32(np, "gen-panel-bus-width", &val);
	sprd_panel_mipi_info.video_bus_width = val;
	of_property_read_u32(np, "gen-panel-lanes", &val);
	sprd_panel_mipi_info.lan_number = val;
	of_property_read_u32(np, "gen-panel-phy-feq", &val);
	sprd_panel_mipi_info.phy_feq = val;
	of_property_read_u32(np, "gen-panel-h-sync-pol", &val);
	sprd_panel_mipi_info.h_sync_pol = val;
	of_property_read_u32(np, "gen-panel-v-sync-pol", &val);
	sprd_panel_mipi_info.v_sync_pol = val;
	of_property_read_u32(np, "gen-panel-de-pol", &val);
	sprd_panel_mipi_info.de_pol = val;
	of_property_read_u32(np, "gen-panel-te-pol", &val);
	sprd_panel_mipi_info.te_pol = val;
	of_property_read_u32(np, "gen-panel-color-mode-pol", &val);
	sprd_panel_mipi_info.color_mode_pol = val;
	of_property_read_u32(np, "gen-panel-shut-down-pol", &val);
	sprd_panel_mipi_info.shut_down_pol = val;
	/*
	   of_property_read_u32(np, "gen-panel-h-front-porch", &val);
	   sprd_panel_timing.hfp = val;
	   of_property_read_u32(np, "gen-panel-h-back-porch", &val);
	   sprd_panel_timing.hbp = val;
	   of_property_read_u32(np, "gen-panel-h-sync-width", &val);
	   sprd_panel_timing.hsync = val;
	   of_property_read_u32(np, "gen-panel-v-front-porch", &val);
	   sprd_panel_timing.vfp = val;
	   of_property_read_u32(np, "gen-panel-v-back-porch", &val);
	   sprd_panel_timing.vbp = val;
	   of_property_read_u32(np, "gen-panel-v-sync-width", &val);
	   sprd_panel_timing.vsync = val;
	   of_property_read_u32(np, "gen-panel-bl-control", &val);
	   */
	sprd_panel_spec.rst_mode = of_property_read_bool(np, "gen-panel-rst-high-in-sleep");

	return 0;
}
#endif

static const struct gen_panel_ops gp_dsi_ops = {
	.tx_cmds = sprd_panel_dsi_tx_cmd_array,
	.rx_cmds = sprd_panel_rx_cmd_array,
#if CONFIG_OF
	.parse_dt = sprd_panel_parse_dt,
#endif
};

static const struct gen_panel_ops gp_i2c_ops = {
	.tx_cmds = sprd_panel_i2c_tx_cmd_array,
	.rx_cmds = sprd_panel_rx_cmd_array,
#if CONFIG_OF
	.parse_dt = sprd_panel_parse_dt,
#endif
};
#ifdef CONFIG_FB_BL_EVENT_CTRL
static int32_t ili9486s1_backlight_ctrl(struct panel_spec *self, uint16_t bl_on)
{
	if (bl_on)
		fb_bl_notifier_call_chain(FB_BL_EVENT_RESUME, NULL);
	else
		fb_bl_notifier_call_chain(FB_BL_EVENT_SUSPEND, NULL);

	return 0;
}
#endif


static int32_t sprd_panel_init(struct panel_spec *self)
{
	struct lcd *lcd = panel_to_lcd(self);
	gen_panel_set_status(lcd, GEN_PANEL_ON);

	return 0;
}

static int32_t sprd_panel_start(struct panel_spec *self)
{
	struct lcd *lcd = panel_to_lcd(self);

	gen_panel_start(lcd, GEN_PANEL_ON);

	return 0;
}

static int32_t sprd_panel_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	struct lcd *lcd = panel_to_lcd(self);
	gen_panel_set_status(lcd, GEN_PANEL_OFF);

	return 0;
}

static int __init sprd_panel_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	pr_info("%s\n", __func__);
	g_client = client;
	sprd_i2c_ctl_chg_clk(3, 400000);

	return 0;
}
static int __exit sprd_panel_i2c_remove(struct i2c_client *client)
{
	return 0;
}

#define SPRD_PANEL_I2C_DEV_NAME ("bridge")
static struct i2c_device_id sprd_panel_i2c_id_table[] = {
	{ SPRD_PANEL_I2C_DEV_NAME, 0 },
	{},
};

MODULE_DEVICE_TABLE(i2c, sprd_panel_i2c_id_table);

#ifdef CONFIG_HAS_EARLYSUSPEND
static const struct dev_pm_ops sprd_panel_i2c_pm_ops = {
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id sprd_panel_i2c_of_match[] = {
	{ .compatible = "sprd_panel,i2c", },
	{ }
};
#endif

static struct i2c_driver sprd_panel_i2c_driver = {
	.id_table	= sprd_panel_i2c_id_table,
	.probe		= sprd_panel_i2c_probe,
	.remove		= __exit_p(sprd_panel_i2c_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= SPRD_PANEL_I2C_DEV_NAME,
#ifdef CONFIG_HAS_EARLYSUSPEND
		.pm	= &sprd_panel_i2c_pm_ops,
#endif

#ifdef CONFIG_OF
		.of_match_table = sprd_panel_i2c_of_match,
#endif
	},
};

static const struct of_device_id sprd_panel_dt_match[];

static int sprd_panel_probe(struct platform_device *pdev)
{
	int ret;
	struct lcd *lcd;
	const struct of_device_id *match;
	struct device_node *np = pdev->dev.of_node;
	const struct gen_panel_ops *ops = &gp_dsi_ops;

	pr_info("called %s\n", __func__);

	lcd = kzalloc(sizeof(*lcd), GFP_KERNEL);
	if (unlikely(!lcd))
		return -ENOMEM;

	match = of_match_node(sprd_panel_dt_match, np);
	if (match) {
		ops = (struct gen_panel_ops *)match->data;
		if (!strncmp(match->compatible, "sprd,sprdfb-i2c-panel",
					strlen("sprd,sprdfb-i2c-panel") + 1)) {
			pr_info("%s, add i2c driver\n", __func__);
			i2c_add_driver(&sprd_panel_i2c_driver);
		}
	} else {
		pr_err("%s, no matched node\n", __func__);
		kfree(lcd);
		return -ENODEV;
	}
	lcd->ops = ops;
	lcd->pdata = (void *)&sprd_panel_spec;

	ret = gen_panel_probe(pdev->dev.of_node, lcd);
	if (unlikely(ret))
		goto err_gen_panel_probe;

	gpmode_to_sprdmode(lcd);

	if (lcd->esd_en) {
#ifdef CONFIG_LCD_ESD_RECOVERY
		sprd_panel_spec.esd_info->type = lcd->esd_type;
		sprd_panel_spec.esd_info->gpio = lcd->esd_gpio;
#endif
	}
	if( lcd->rst_gpio_en){
		sprd_panel_spec.rst_gpio_en = lcd->rst_gpio_en;
		sprd_panel_spec.rst_gpio = lcd->rst_gpio;
	}
	sprd_panel_spec.plat_data = lcd;
	sprdfb_panel_register(&sprd_panel_cfg);

	gen_panel_set_status(lcd, GEN_PANEL_ON_REDUCED);

	return 0;

err_gen_panel_probe:
	kfree(lcd);
	return ret;
}

static void sprd_panel_shutdown(struct platform_device *pdev)
{
	struct lcd *lcd = (struct lcd *)sprd_panel_spec.plat_data;
	struct backlight_device *bd;

	if (lcd->bd) {
		bd = lcd->bd;
		bd->props.brightness = 0;
		bd->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(bd);
	}

	gen_panel_set_status(lcd, GEN_PANEL_OFF);
}

static int sprd_panel_remove(struct platform_device *pdev)
{
	struct lcd *lcd = (struct lcd *)sprd_panel_spec.plat_data;

	gen_panel_remove(lcd);

	if (g_client) {
		i2c_del_driver(&sprd_panel_i2c_driver);
		g_client = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sprd_panel_dt_match[] = {
	{
		.compatible = "sprd,sprdfb-dsi-panel",
		.data = &gp_dsi_ops,
	},
	{
		.compatible = "sprd,sprdfb-i2c-panel",
		.data = &gp_i2c_ops,
	},
	{},
};
#endif
static struct platform_driver sprd_panel_driver = {
	.driver = {
		.name = "sprdfb-dsi-panel",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(sprd_panel_dt_match),
#endif
	},
	.probe = sprd_panel_probe,
	.remove = sprd_panel_remove,
	/*.shutdown = sprd_panel_shutdown,*/
};

static int sprd_panel_module_init(void)
{
	return platform_driver_register(&sprd_panel_driver);
}

static void sprd_panel_module_exit(void)
{
	platform_driver_unregister(&sprd_panel_driver);
}

module_init(sprd_panel_module_init);
module_exit(sprd_panel_module_exit);

MODULE_DESCRIPTION("SPRD DSI PANEL DRIVER");
MODULE_LICENSE("GPL");
