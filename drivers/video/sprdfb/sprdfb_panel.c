/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/fb.h>

#include "sprdfb.h"
#include "sprdfb_panel.h"
#include "sprdfb_dispc_reg.h"
#include "sprdfb_lcdc_reg.h"
#include <linux/gpio.h>

static LIST_HEAD(panel_list_main);
static LIST_HEAD(panel_list_sub);
static DEFINE_MUTEX(panel_mutex);

static struct panel_spec *g_panel;

uint32_t lcd_id_from_uboot;
uint32_t lcd_base_from_uboot;

extern struct panel_if_ctrl sprdfb_mcu_ctrl;
extern struct panel_if_ctrl sprdfb_rgb_ctrl;
#ifndef CONFIG_FB_SCX15
extern struct panel_if_ctrl sprdfb_mipi_ctrl;
#endif
extern void sprdfb_panel_remove(struct sprdfb_device *dev);

#ifdef CONFIG_FB_SC8825
typedef struct {
	uint32_t reg;
	uint32_t val;
} panel_pinmap_t;

panel_pinmap_t panel_rstpin_map[] = {
	{REG_PIN_LCD_RSTN, BITS_PIN_DS(1) | BITS_PIN_AF(0) |
		BIT_PIN_NUL | BIT_PIN_SLP_WPU | BIT_PIN_SLP_OE},
	{REG_PIN_LCD_RSTN, BITS_PIN_DS(3) | BITS_PIN_AF(3) |
		BIT_PIN_NUL | BIT_PIN_NUL | BIT_PIN_SLP_OE},
};

static void sprd_panel_set_rstn_prop(unsigned int if_slp)
{
	int i = 0;

	if (if_slp) {
		panel_rstpin_map[0].val =
			sci_glb_read(CTL_PIN_BASE + REG_PIN_LCD_RSTN,
					0xffffffff);
		i = 1;
	} else {
		i = 0;
	}

	sci_glb_write(CTL_PIN_BASE + panel_rstpin_map[i].reg,
			panel_rstpin_map[i].val, 0xffffffff);
}
#endif

static int __init lcd_id_get(char *str)
{
	if ((str) && (str[0] == 'I') && (str[1] == 'D'))
		sscanf(&str[2], "%x", &lcd_id_from_uboot);

	pr_info("%s, lcd_id from bootloader = 0x%x\n",
			__func__, lcd_id_from_uboot);
	return 1;
}
__setup("lcd_id=", lcd_id_get);
static int __init lcd_base_get(char *str)
{
	if (str)
		sscanf(&str[0], "%x", &lcd_base_from_uboot);

	pr_info("%s, fb_base from bootloader = 0x%x\n",
			__func__, lcd_base_from_uboot);
	return 1;
}
__setup("lcd_base=", lcd_base_get);

static int32_t panel_reset_dispc(struct panel_spec *self)
{
	uint16_t timing1, timing2, timing3;

	if (self && self->reset_timing.time1 &&
			self->reset_timing.time2 && self->reset_timing.time3) {
		timing1 = self->reset_timing.time1;
		timing2 = self->reset_timing.time2;
		timing3 = self->reset_timing.time3;
	} else {
		timing1 = 20;
		timing2 = 20;
		timing3 = 120;
	}

	if(self->rst_gpio_en){
		gpio_request(self->rst_gpio,"LCD_RST_GPIO");
		gpio_direction_output(self->rst_gpio, 1);
		usleep_range(timing1 * 1000, timing1 * 1000 + 500);
		gpio_direction_output(self->rst_gpio, 0);
		usleep_range(timing2 * 1000, timing2 * 1000 + 500);
		gpio_direction_output(self->rst_gpio, 1);
		usleep_range(timing3 * 1000, timing3 * 1000 + 500);
	}else{
		dispc_write(1, DISPC_RSTN);
		usleep_range(timing1 * 1000, timing1 * 1000 + 500);
		dispc_write(0, DISPC_RSTN);
		usleep_range(timing2 * 1000, timing2 * 1000 + 500);
		dispc_write(1, DISPC_RSTN);

		/* wait 10ms util the lcd is stable */
		usleep_range(timing3 * 1000, timing3 * 1000 + 500);
	}
	return 0;
}

static int32_t panel_reset_lcdc(struct panel_spec *self)
{
	lcdc_write(0, LCM_RSTN);
	msleep(20);
	lcdc_write(1, LCM_RSTN);

	/* wait 10ms util the lcd is stable */
	msleep(20);
	return 0;
}

static int32_t panel_set_resetpin_dispc(uint32_t status)
{
	struct panel_spec *self = g_panel;

	if(self->rst_gpio_en){
		gpio_request(self->rst_gpio,"LCD_RST_GPIO");

		if (!status)
			gpio_direction_output(self->rst_gpio, 0);
		else
			gpio_direction_output(self->rst_gpio, 1);
	}else{
		if (!status)
			dispc_write(0, DISPC_RSTN);
		else
			dispc_write(1, DISPC_RSTN);
	}

	return 0;
}

#ifdef CONFIG_FB_SC8825
static int32_t panel_set_resetpin_lcdc(uint32_t status)
{
	if (!status)
		lcdc_write(0, LCM_RSTN);
	else
		lcdc_write(1, LCM_RSTN);

	return 0;
}
#endif

static int panel_reset(struct sprdfb_device *dev)
{
	if (!dev || !dev->panel) {
		pr_err("%s, no dev\n", __func__);
		return -1;
	}

	pr_debug("%s, enter\n", __func__);

	/* clk/data lane enter LP */
	if (dev->panel->if_ctrl->panel_if_before_panel_reset)
		dev->panel->if_ctrl->panel_if_before_panel_reset(dev);

	usleep_range(5000, 5500);
	dev->panel->ops->panel_reset(dev->panel);

	return 0;
}

static int panel_sleep(struct sprdfb_device *dev)
{
	if (!dev || !dev->panel) {
		pr_err("%s, no dev\n", __func__);
		return -1;
	}

	pr_debug("%s, enter\n", __func__);

	if (dev->panel->ops->panel_enter_sleep)
		dev->panel->ops->panel_enter_sleep(dev->panel, 1);

	/* clk/data lane enter LP */
	if (dev->panel->if_ctrl->panel_if_before_panel_reset &&
			SPRDFB_PANEL_TYPE_MIPI == dev->panel->type)
		dev->panel->if_ctrl->panel_if_before_panel_reset(dev);
	return 0;
}

static void panel_set_resetpin(uint16_t dev_id, uint32_t status,
		struct panel_spec *panel)
{
	pr_debug("%s, enter\n", __func__);

	/* panel set reset pin status */
	if (SPRDFB_MAINLCD_ID == dev_id) {
		panel_set_resetpin_dispc(status);
	} else {
#ifdef CONFIG_FB_SC8825
		panel_set_resetpin_lcdc(status);
#endif
	}
}

static int32_t panel_before_resume(struct sprdfb_device *dev)
{
#ifdef CONFIG_FB_SC8825
	/*restore the reset pin status*/
	sprd_panel_set_rstn_prop(0);
#endif
	if (dev->panel->ops->panel_pin_init)
		dev->panel->ops->panel_pin_init(0);

	/*restore the reset pin to high*/
	panel_set_resetpin(dev->dev_id, 1, dev->panel);
	return 0;
}

static int32_t panel_after_suspend(struct sprdfb_device *dev)
{
	/*set the reset pin to low*/
	panel_set_resetpin(dev->dev_id,dev->panel->rst_mode, dev->panel);

	if (dev->panel->ops->panel_pin_init)
		dev->panel->ops->panel_pin_init(1);
#ifdef CONFIG_FB_SC8825
	/*set the reset pin status and set */
	sprd_panel_set_rstn_prop(1);
#endif
	return 0;
}

static bool panel_check(struct panel_cfg *cfg)
{
	bool rval = true;

	if (!cfg || !cfg->panel) {
		pr_err("%s, no cfg or no panel\n", __func__);
		return false;
	}

	pr_debug("%s, %s, id:0x%x, type:%d\n", __func__,
			(cfg->dev_id == SPRDFB_MAINLCD_ID) ? "main" : "sub",
			cfg->lcd_id, cfg->panel->type);

	switch (cfg->panel->type) {
	case SPRDFB_PANEL_TYPE_MCU:
		cfg->panel->if_ctrl = &sprdfb_mcu_ctrl;
		break;
	case SPRDFB_PANEL_TYPE_RGB:
		cfg->panel->if_ctrl = &sprdfb_rgb_ctrl;
		break;
#ifndef CONFIG_FB_SCX15
	case SPRDFB_PANEL_TYPE_MIPI:
		cfg->panel->if_ctrl = &sprdfb_mipi_ctrl;
		break;
#endif
	default:
		pr_warn("%s, invalid type:%d\n", __func__, cfg->panel->type);
		cfg->panel->if_ctrl = NULL;
		break;
	};

	if (cfg->panel->if_ctrl->panel_if_check)
		rval = cfg->panel->if_ctrl->panel_if_check(cfg->panel);

	return rval;
}

static int panel_mount(struct sprdfb_device *dev, struct panel_spec *panel)
{
	/* TODO: check whether the mode/res are supported */
	dev->panel = panel;

	if (!dev->panel->ops->panel_reset) {
		if (SPRDFB_MAINLCD_ID == dev->dev_id)
			dev->panel->ops->panel_reset = panel_reset_dispc;
		else
			dev->panel->ops->panel_reset = panel_reset_lcdc;
	}

	panel->if_ctrl->panel_if_mount(dev);

#ifdef CONFIG_LCD_ESD_RECOVERY
	dev->panel->esd_info = panel->esd_info;
#endif

	return 0;
}


int panel_init(struct sprdfb_device *dev)
{
	if (!dev || !dev->panel) {
		pr_err("%s, no dev\n", __func__);
		return -1;
	}

	if (!dev->panel->if_ctrl->panel_if_init(dev)) {
		pr_err("%s, panel_if_init failed\n", __func__);
		return -1;
	}

	return 0;
}

int panel_ready(struct sprdfb_device *dev)
{
	if (!dev || !dev->panel) {
		pr_err("%s, no dev\n", __func__);
		return -1;
	}

	if (dev->panel->if_ctrl->panel_if_ready)
		dev->panel->if_ctrl->panel_if_ready(dev);

	return 0;
}


static struct panel_spec *adapt_panel_from_uboot(uint16_t dev_id)
{
	struct panel_cfg *cfg;
	struct list_head *panel_list;

	if (!lcd_id_from_uboot) {
		pr_err("%s, no panel from bootloader\n", __func__);
		return NULL;
	}

	if (SPRDFB_MAINLCD_ID == dev_id)
		panel_list = &panel_list_main;
	else
		panel_list = &panel_list_sub;

	list_for_each_entry(cfg, panel_list, list) {
		if (lcd_id_from_uboot == cfg->lcd_id) {
			pr_info("%s, panel found (id:0x%x)\n",
					__func__, cfg->lcd_id);
			return cfg->panel;
		}
	}
	pr_err("%s, panel not found (id:0x%x)\n",
			__func__, lcd_id_from_uboot);

	return NULL;
}

static struct panel_spec *adapt_panel_from_readid(struct sprdfb_device *dev)
{
	struct panel_cfg *cfg;
	struct panel_cfg *dummy_cfg = NULL;
	struct list_head *panel_list;
	int id;

	if (SPRDFB_MAINLCD_ID == dev->dev_id)
		panel_list = &panel_list_main;
	else
		panel_list = &panel_list_sub;

	list_for_each_entry(cfg, panel_list, list) {
		if (0xFFFFFFFF == cfg->lcd_id) {
			dummy_cfg = cfg;
			continue;
		}
		pr_debug("%s, try to find panel 0x%x\n",
				__func__, cfg->lcd_id);
		panel_mount(dev, cfg->panel);
#ifndef CONFIG_MACH_SPX15FPGA
		dev->ctrl->update_clk(dev);
#endif
		panel_init(dev);
		panel_reset(dev);
		id = dev->panel->ops->panel_readid(dev->panel);
		if (id == cfg->lcd_id) {
			pr_info("%s, panel found (id:0x%x)\n",
					__func__, cfg->lcd_id);
			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
			if (dev->panel->ops->panel_set_start)
				dev->panel->ops->panel_set_start(dev->panel);
			return cfg->panel;
		}
		sprdfb_panel_remove(dev);
	}

	if (dummy_cfg) {
		pr_info("%s, use dummy panel\n", __func__);
		panel_mount(dev, dummy_cfg->panel);
#ifndef CONFIG_MACH_SPX15FPGA
		dev->ctrl->update_clk(dev);
#endif
		panel_init(dev);
		panel_reset(dev);
		id = dev->panel->ops->panel_readid(dev->panel);
		if (id == dummy_cfg->lcd_id) {
			pr_info("%s, dummy panel found (id:0x%x)\n",
					__func__, dummy_cfg->lcd_id);
			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
			if (dev->panel->ops->panel_set_start)
				dev->panel->ops->panel_set_start(dev->panel);
			return dummy_cfg->panel;
		}
		sprdfb_panel_remove(dev);
	}
	pr_err("%s, panel not found\n", __func__);

	return NULL;
}


bool sprdfb_panel_get(struct sprdfb_device *dev)
{
	struct panel_spec *panel = NULL;

	if (!dev) {
		pr_err("%s, no dev\n", __func__);
		return false;
	}

	pr_info("%s, %s lcd\n", __func__,
			(dev->dev_id == SPRDFB_MAINLCD_ID) ? "main" : "sub");
	panel = adapt_panel_from_uboot(dev->dev_id);
	if (panel) {
		g_panel = panel;
		dev->panel_ready = true;
		panel_mount(dev, panel);
		panel_init(dev);
		pr_info("%s, got panel\n", __func__);
		return true;
	}

	pr_info("%s, failed to get panel\n", __func__);

	return false;
}

bool sprdfb_panel_probe(struct sprdfb_device *dev)
{
	struct panel_spec *panel;

	if (!dev) {
		pr_err("%s, no dev\n", __func__);
		return false;
	}

	pr_info("%s, %s lcd\n", __func__,
			(dev->dev_id == SPRDFB_MAINLCD_ID) ? "main" : "sub");
	panel = adapt_panel_from_readid(dev);
	if (panel) {
		pr_info("%s, got panel\n", __func__);
		return true;
	}
	pr_info("%s, failed to get panel\n", __func__);

	return false;
}

void sprdfb_panel_invalidate_rect(struct panel_spec *self,
		uint16_t left, uint16_t top,
		uint16_t right, uint16_t bottom)
{
	/*Jessica TODO: */
	if (self->ops->panel_invalidate_rect)
		self->ops->panel_invalidate_rect(self, left, top, right, bottom);

	/*Jessica TODO: Need set timing to GRAM timing*/
}

void sprdfb_panel_invalidate(struct panel_spec *self)
{
	/*Jessica TODO:*/
	if (self->ops->panel_invalidate)
		self->ops->panel_invalidate(self);

	/*Jessica TODO: Need set timing to GRAM timing*/
}

void sprdfb_panel_before_refresh(struct sprdfb_device *dev)
{
	if (dev->panel->if_ctrl->panel_if_before_refresh)
		dev->panel->if_ctrl->panel_if_before_refresh(dev);
}

void sprdfb_panel_after_refresh(struct sprdfb_device *dev)
{
	if (dev->panel->if_ctrl->panel_if_after_refresh)
		dev->panel->if_ctrl->panel_if_after_refresh(dev);
}

#ifdef CONFIG_FB_DYNAMIC_FREQ_SCALING
void sprdfb_panel_change_fps(struct sprdfb_device *dev, int fps_level)
{
	if (dev->panel->ops->panel_change_fps) {
		pr_info("%s, fps_level:%d\n", __func__, fps_level);
		dev->panel->ops->panel_change_fps(dev->panel, fps_level);
	}
}
#endif

#ifdef CONFIG_FB_ESD_SUPPORT
/*return value: 0--panel OK.1-panel has been reset*/
uint32_t sprdfb_panel_ESD_check(struct sprdfb_device *dev)
{
	int32_t result = 0;
	uint32_t if_status = 0;

	dev->check_esd_time++;

	if (SPRDFB_PANEL_IF_EDPI == dev->panel_if_type) {
		if (dev->panel->ops->panel_esd_check) {
			result = dev->panel->ops->panel_esd_check(dev->panel);
			pr_debug("%s, panel status : %d\n", __func__, result);
		}
	} else if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
#ifdef FB_CHECK_ESD_BY_TE_SUPPORT
		dev->esd_te_waiter++;
		dev->esd_te_done = 0;
		dispc_set_bits(BIT(1), DISPC_INT_EN);
		result = wait_event_interruptible_timeout(dev->esd_te_queue,
				dev->esd_te_done, msecs_to_jiffies(600));
		pr_debug("sprdfb: after wait (%d)\n", result);
		dispc_clear_bits(BIT(1), DISPC_INT_EN);
		if (!result) { /*time out*/
			pr_warn("%s, te signal timeout!!\n", __func__);
			dev->esd_te_waiter = 0;
			result = 0;
		} else {
			pr_debug("%s, te signal\n", __func__);
			result = 1;
		}
#else
		if (dev->panel->ops->panel_esd_check)
			result = dev->panel->ops->panel_esd_check(dev->panel);
#endif
	}

	if (!dev->enable) {
		pr_err("%s, device not enabled\n", __func__);
		return 0;
	}

	if (!result) {
		dev->panel_reset_time++;

		if (SPRDFB_PANEL_IF_EDPI == dev->panel_if_type) {
			if (dev->panel->if_ctrl->panel_if_get_status)
				if_status = dev->panel->if_ctrl->panel_if_get_status(dev);
		} else if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
			/*need reset dsi as default for dpi mode*/
			if_status = 2;
		}

		if (!if_status) {
			pr_info("%s, need reset panel(%d, %d, %d)\n", __func__,
					dev->check_esd_time,
					dev->panel_reset_time,
					dev->reset_dsi_time);
			panel_reset(dev);
			if (!dev->enable) {
				pr_err("%s, device not enabled after reset\n",
						__func__);
				return 0;
			}

			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
			if (dev->panel->ops->panel_set_start)
				dev->panel->ops->panel_set_start(dev->panel);

		} else {
			pr_info("%s, need reset panel(%d, %d, %d)\n", __func__,
					dev->check_esd_time,
					dev->panel_reset_time,
					dev->reset_dsi_time);
			dev->reset_dsi_time++;
			if (dev->panel->if_ctrl->panel_if_suspend)
				dev->panel->if_ctrl->panel_if_suspend(dev);

			mdelay(10);

			if (!dev->enable) {
				pr_err("%s, device not enabled\n", __func__);
				return 0;
			}

			panel_init(dev);
			panel_reset(dev);

			if (!dev->enable) {
				pr_err("%s, device not enabled after reset\n",
						__func__);
				return 0;
			}

			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
			if (dev->panel->ops->panel_set_start)
				dev->panel->ops->panel_set_start(dev->panel);

		}
		pr_debug("%s panel has been reset\n", __func__);
		return 1;
	}
	return 0;
}
#endif

#ifdef CONFIG_LCD_ESD_RECOVERY
void sprdfb_panel_ESD_reset(struct sprdfb_device *dev)
{
	if (dev->panel->if_ctrl->panel_if_suspend)
		dev->panel->if_ctrl->panel_if_suspend(dev);
	mdelay(10);

	panel_init(dev);
	panel_reset(dev);
	dev->panel->ops->panel_init(dev->panel);
	panel_ready(dev);
}
#endif

void sprdfb_panel_suspend(struct sprdfb_device *dev)
{

	pr_info("%s +\n", __func__);

	if (!dev || !dev->panel) {
		pr_err("%s, no dev\n", __func__);
		return;
	}

	/* 1. send lcd sleep cmd or reset panel directly */
	if (dev->panel->suspend_mode == SEND_SLEEP_CMD)
		panel_sleep(dev);
	else if (!dev->panel->ops->panel_pin_init)
		panel_reset(dev);

	/* 2. clk/data lane enter ulps */
	if (dev->panel->if_ctrl->panel_if_enter_ulps)
		dev->panel->if_ctrl->panel_if_enter_ulps(dev);

	/* 3. turn off mipi */
	if (dev->panel->if_ctrl->panel_if_suspend)
		dev->panel->if_ctrl->panel_if_suspend(dev);

	/* 4. reset pin to low */
	if (dev->panel->ops->panel_after_suspend)
		dev->panel->ops->panel_after_suspend(dev->panel);
	else
		panel_after_suspend(dev);

	pr_info("%s -\n", __func__);
}

void sprdfb_panel_start(struct sprdfb_device *dev)
{
	if (!dev || !dev->panel) {
		pr_err("%s, no dev\n", __func__);
		return;
	}

	if (dev->panel->ops->panel_set_start)
		dev->panel->ops->panel_set_start(dev->panel);

	return;
}

void sprdfb_panel_resume(struct sprdfb_device *dev, bool from_deep_sleep)
{
	if (!dev || !dev->panel) {
		pr_err("%s, no dev\n", __func__);
		return;
	}

	pr_info("%s, + enable:%d, from_deep_sleep:%d\n",
			__func__, dev->enable, from_deep_sleep);

	if (from_deep_sleep) {
		/* 1. turn on mipi */
		panel_init(dev);

		/* 2. reset pin to high */
		if (dev->panel->ops->panel_before_resume)
			dev->panel->ops->panel_before_resume(dev->panel);
		else
			panel_before_resume(dev);

		/* 3. step3 reset panel */
		panel_reset(dev);

		/* 4. step4 panel init */
		dev->panel->ops->panel_init(dev->panel);

		/* 5. clk/data lane enter HS */
		panel_ready(dev);
	} else {
		/* 1. turn on mipi */
		/*Jessica TODO: resume i2c, spi, mipi*/
		if (dev->panel->if_ctrl->panel_if_resume)
			dev->panel->if_ctrl->panel_if_resume(dev);

		/* reset pin to high */
		if (dev->panel->ops->panel_before_resume)
			dev->panel->ops->panel_before_resume(dev->panel);
		else
			panel_before_resume(dev);

		/* sleep out */
		if (dev->panel->ops->panel_enter_sleep)
			dev->panel->ops->panel_enter_sleep(dev->panel, 0);

		/* clk/data lane enter HS */
		panel_ready(dev);
	}
	pr_info("%s -\n", __func__);

	return;
}

void sprdfb_panel_remove(struct sprdfb_device *dev)
{
	if (!dev || !dev->panel) {
		pr_err("%s, no dev\n", __func__);
		return;
	}

	/*Jessica TODO:close panel, i2c, spi, mipi*/
	if (dev->panel->if_ctrl->panel_if_uninit)
		dev->panel->if_ctrl->panel_if_uninit(dev);

	dev->panel = NULL;
}

int sprdfb_panel_register(struct panel_cfg *cfg)
{
	if (!panel_check(cfg)) {
		pr_err("%s, invalid cfg\n", __func__);
		return -1;
	}

	mutex_lock(&panel_mutex);
	if (cfg->dev_id == SPRDFB_MAINLCD_ID) {
		list_add_tail(&cfg->list, &panel_list_main);
	} else if (cfg->dev_id == SPRDFB_SUBLCD_ID) {
		list_add_tail(&cfg->list, &panel_list_sub);
	} else {
		list_add_tail(&cfg->list, &panel_list_main);
		list_add_tail(&cfg->list, &panel_list_sub);
	}
	mutex_unlock(&panel_mutex);

	pr_info("%s, register panel(%s, id:0x%x) on %s lcd list\n", __func__,
			cfg->lcd_name ? cfg->lcd_name : "unknown", cfg->lcd_id,
			(cfg->dev_id == SPRDFB_MAINLCD_ID) ? "main" : "sub");

	return 0;
}
