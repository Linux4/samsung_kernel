/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#include <asm/gpio.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/fb.h>

#include "sprdfb.h"
#include "sprdfb_panel.h"
#include "sprdfb_dispc_reg.h"

static LIST_HEAD(panel_list_main);/* for main_lcd*/
static LIST_HEAD(panel_list_sub);/* for sub_lcd */
static DEFINE_MUTEX(panel_mutex);

static uint32_t lcd_id_from_uboot = 0;
unsigned long lcd_base_from_uboot = 0;

extern struct panel_if_ctrl sprdfb_mcu_ctrl;
extern struct panel_if_ctrl sprdfb_rgb_ctrl;
#ifndef CONFIG_FB_SCX15
extern struct panel_if_ctrl sprdfb_mipi_ctrl;
#endif
extern void sprdfb_panel_remove(struct sprdfb_device *dev);
#ifdef CONFIG_GEN_PANEL_OCTA
extern void dispc_stop(struct sprdfb_device *dev);
#endif

static int __init lcd_id_get(char *str)
{
	if ((str != NULL) && (str[0] == 'I') && (str[1] == 'D')) {
		sscanf(&str[2], "%x", &lcd_id_from_uboot);
	}
	printk(KERN_INFO "sprdfb: [%s]LCD Panel ID from uboot: 0x%x\n", __FUNCTION__, lcd_id_from_uboot);
	return 1;
}
__setup("lcd_id=", lcd_id_get);
static int __init lcd_base_get(char *str)
{
	if (str != NULL) {
		sscanf(&str[0], "%lx", &lcd_base_from_uboot);
	}
	printk(KERN_INFO "sprdfb: [%s]LCD Panel Base from uboot: 0x%lx\n", __FUNCTION__, lcd_base_from_uboot);
	return 1;
}
__setup("lcd_base=", lcd_base_get);

static int32_t panel_reset_dispc(struct panel_spec *self)
{
	uint16_t timing1, timing2, timing3;
	if (unlikely(!self)) {
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -EINVAL;
	}
	pr_info("[LCD] %s, enter\n",__func__);

	if((self->reset_timing.time1) &&
			(self->reset_timing.time2) &&
			(self->reset_timing.time3)) {
	    timing1 = self->reset_timing.time1;
	    timing2 = self->reset_timing.time2;
	    timing3 = self->reset_timing.time3;
	}else {
	    timing1 = 20;
	    timing2 = 20;
#ifdef CONFIG_GEN_PANEL_OCTA
		timing3 = 10;
#else
		timing3 = 120;
#endif
	}

	if (self->rst_gpio_en){
		gpio_set_value(self->rst_gpio, 0);
		usleep_range(timing2*1000, timing2*1000+500);
		gpio_set_value(self->rst_gpio, 1);
		usleep_range(timing3*1000, timing3*1000+500);
	}else{
		dispc_write(1, DISPC_RSTN);
		usleep_range(timing1*1000, timing1*1000+500);
		dispc_write(0, DISPC_RSTN);
		usleep_range(timing2*1000, timing2*1000+500);
		dispc_write(1, DISPC_RSTN);
	}
	/* wait 10ms util the lcd is stable */
	usleep_range(timing3*1000, timing3*1000+500);

	return 0;
}

static int32_t panel_set_resetpin_dispc( uint32_t status)
{
	if(0 == status){
		dispc_write(0, DISPC_RSTN);
	}else{
		dispc_write(1, DISPC_RSTN);
	}
	return 0;
}

static int panel_reset(struct sprdfb_device *dev)
{
	if (unlikely(!dev || !dev->panel)) {
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_info("[LCD] %s, enter\n",__func__);

	/* clk/data lane enter LP */
	if (dev->panel->if_ctrl &&
			dev->panel->if_ctrl->panel_if_before_panel_reset)
		dev->panel->if_ctrl->panel_if_before_panel_reset(dev);
	usleep_range(5000, 5500);

	//reset panel
	if (dev->panel->ops &&
			dev->panel->ops->panel_reset)
		dev->panel->ops->panel_reset(dev->panel);

	return 0;
}

static int panel_sleep(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], enter\n",__FUNCTION__);

	//send sleep cmd to lcd
	if (dev->panel->ops->panel_enter_sleep != NULL)
		dev->panel->ops->panel_enter_sleep(dev->panel,1);

	//clk/data lane enter LP
	if((NULL != dev->panel->if_ctrl->panel_if_before_panel_reset)
		&&(SPRDFB_PANEL_TYPE_MIPI == dev->panel->type))
		dev->panel->if_ctrl->panel_if_before_panel_reset(dev);

	/* 3 frame delay before reset for stablity of ldi side */
	msleep(50);
	return 0;
}

static void panel_set_resetpin(uint16_t dev_id,  uint32_t status, struct panel_spec *panel )
{
	pr_debug("sprdfb: [%s].\n",__FUNCTION__);

	/*panel set reset pin status*/
	if(SPRDFB_MAINLCD_ID == dev_id){
		panel_set_resetpin_dispc(status);
	}
}


static int32_t panel_before_resume(struct sprdfb_device *dev)
{
	/*restore  the reset pin to high*/
	panel_set_resetpin(dev->dev_id, 1, dev->panel);
	return 0;
}

static int32_t panel_after_suspend(struct sprdfb_device *dev)
{
	/*set the reset pin to low*/
	panel_set_resetpin(dev->dev_id, 0, dev->panel);
	return 0;
}

static bool panel_check(struct panel_cfg *cfg)
{
	bool rval = true;

	if(NULL == cfg || NULL == cfg->panel){
		printk(KERN_ERR "sprdfb: [%s] :Invalid Param!\n", __FUNCTION__);
		return false;
	}

	pr_debug("sprdfb: [%s], dev_id = %d, lcd_id = 0x%x, type = %d\n",__FUNCTION__, cfg->dev_id, cfg->lcd_id, cfg->panel->type);

	switch(cfg->panel->type){
	case SPRDFB_PANEL_TYPE_MCU:
		cfg->panel->if_ctrl = &sprdfb_mcu_ctrl;
		break;
	case SPRDFB_PANEL_TYPE_RGB:
	case SPRDFB_PANEL_TYPE_LVDS:
		cfg->panel->if_ctrl = &sprdfb_rgb_ctrl;
		break;
#ifndef CONFIG_FB_SCX15
	case SPRDFB_PANEL_TYPE_MIPI:
		cfg->panel->if_ctrl = &sprdfb_mipi_ctrl;
		break;
#endif
	default:
		printk("sprdfb: [%s]: erro panel type.(%d,%d, %d)",__FUNCTION__, cfg->dev_id, cfg->lcd_id, cfg->panel->type);
		cfg->panel->if_ctrl = NULL;
		break;
	};

	if(cfg->panel->if_ctrl->panel_if_check){
		rval = cfg->panel->if_ctrl->panel_if_check(cfg->panel);
	}
	return rval;
}

static int panel_mount(struct sprdfb_device *dev, struct panel_spec *panel)
{
	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	/* TODO: check whether the mode/res are supported */
	dev->panel = panel;

	if(NULL == dev->panel->ops->panel_reset){
		if(SPRDFB_MAINLCD_ID == dev->dev_id){
			dev->panel->ops->panel_reset = panel_reset_dispc;
		}
	}

	panel->if_ctrl->panel_if_mount(dev);
#ifdef CONFIG_LCD_ESD_RECOVERY
	dev->panel->esd_info = panel->esd_info;
#endif
	return 0;
}

int panel_init(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], dev_id= %d, type = %d\n",__FUNCTION__, dev->dev_id, dev->panel->type);

	if(!dev->panel->if_ctrl->panel_if_init(dev)){
		printk(KERN_ERR "sprdfb: [%s]: panel_if_init fail!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

int panel_ready(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return -1;
	}

	pr_debug("sprdfb: [%s], dev_id= %d, type = %d\n",__FUNCTION__, dev->dev_id, dev->panel->type);

	if(NULL != dev->panel->if_ctrl->panel_if_ready){
		dev->panel->if_ctrl->panel_if_ready(dev);
	}

	return 0;
}


static struct panel_spec *adapt_panel_from_uboot(uint16_t dev_id)
{
	struct panel_cfg *cfg;
	struct list_head *panel_list;

	pr_debug("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev_id);

	if (lcd_id_from_uboot == 0) {
		printk("sprdfb: [%s]: Not got lcd id from uboot\n", __FUNCTION__);
		return NULL;
	}

	if(SPRDFB_MAINLCD_ID == dev_id){
		panel_list = &panel_list_main;
	}else{
		panel_list = &panel_list_sub;
	}

	list_for_each_entry(cfg, panel_list, list) {
		if(lcd_id_from_uboot == cfg->lcd_id) {
			printk(KERN_INFO "sprdfb: [%s]: LCD Panel 0x%x is attached!\n", __FUNCTION__,cfg->lcd_id);
			return cfg->panel;
		}
	}
	printk(KERN_ERR "sprdfb: [%s]: Failed to match LCD Panel from uboot!\n", __FUNCTION__);

	return NULL;
}

static struct panel_spec *adapt_panel_from_readid(struct sprdfb_device *dev)
{
	struct panel_cfg *cfg;
	struct panel_cfg *dummy_cfg = NULL;
	struct list_head *panel_list;
	uint32_t id;

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	if(SPRDFB_MAINLCD_ID == dev->dev_id){
		panel_list = &panel_list_main;
	}else{
		panel_list = &panel_list_sub;
	}

	list_for_each_entry(cfg, panel_list, list) {
		if(0xFFFFFFFF == cfg->lcd_id){
			dummy_cfg = cfg;
			continue;
		}
		printk("sprdfb: [%s]: try panel 0x%x\n", __FUNCTION__, cfg->lcd_id);
		panel_mount(dev, cfg->panel);
#ifndef CONFIG_SC_FPGA
		dev->ctrl->update_clk(dev);
#endif
		panel_init(dev);
		panel_reset(dev);
		id = dev->panel->ops->panel_readid(dev->panel);
		if(id == cfg->lcd_id) {
			pr_debug(KERN_INFO "sprdfb: [%s]: LCD Panel 0x%x is attached!\n", __FUNCTION__, cfg->lcd_id);
			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
			return cfg->panel;
		}
		sprdfb_panel_remove(dev);
	}
	if(dummy_cfg != NULL){
		printk("sprdfb: [%s]: Can't find read panel, Use dummy panel!\n", __FUNCTION__);
		panel_mount(dev, dummy_cfg->panel);
#ifndef CONFIG_SC_FPGA
		dev->ctrl->update_clk(dev);
#endif
		panel_init(dev);
		panel_reset(dev);
		id = dev->panel->ops->panel_readid(dev->panel);
		if(id == dummy_cfg->lcd_id) {
			pr_debug(KERN_INFO "sprdfb: [%s]: LCD Panel 0x%x is attached!\n", __FUNCTION__, dummy_cfg->lcd_id);
			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
			return dummy_cfg->panel;
		}
		sprdfb_panel_remove(dev);
	}
	printk(KERN_ERR "sprdfb:  [%s]: failed to attach LCD Panel!\n", __FUNCTION__);
	return NULL;
}


bool sprdfb_panel_get(struct sprdfb_device *dev)
{
	struct panel_spec *panel = NULL;

	if(NULL == dev){
		printk("sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return false;
	}

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	panel = adapt_panel_from_uboot(dev->dev_id);
	if (panel) {
		dev->panel_ready = true;
		panel_mount(dev, panel);
		panel_init(dev);
		printk("sprdfb: [%s] got panel\n", __FUNCTION__);
		return true;
	}

	printk("sprdfb: [%s] can not got panel\n", __FUNCTION__);

	return false;
}


bool sprdfb_panel_probe(struct sprdfb_device *dev)
{
	struct panel_spec *panel;

	if(NULL == dev){
		printk("sprdfb: [%s]: Invalid param\n", __FUNCTION__);
		return false;
	}

	printk("sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	/* can not be here in normal; we should get correct device id from uboot */
	panel = adapt_panel_from_readid(dev);

	if (panel) {
		printk("sprdfb: [%s] got panel\n", __FUNCTION__);
		return true;
	}

	printk("sprdfb: [%s] can not got panel\n", __FUNCTION__);

	return false;
}

void sprdfb_panel_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	/*Jessica TODO: */
	if(NULL != self->ops->panel_invalidate_rect){
		self->ops->panel_invalidate_rect(self, left, top, right, bottom);
	}
	/*Jessica TODO: Need set timing to GRAM timing*/
}

void sprdfb_panel_invalidate(struct panel_spec *self)
{
	/*Jessica TODO:*/
	if(NULL != self->ops->panel_invalidate){
		self->ops->panel_invalidate(self);
	}
	/*Jessica TODO: Need set timing to GRAM timing*/
}

void sprdfb_panel_before_refresh(struct sprdfb_device *dev)
{
	if(NULL != dev->panel->if_ctrl->panel_if_before_refresh){
		dev->panel->if_ctrl->panel_if_before_refresh(dev);
	}
}

void sprdfb_panel_after_refresh(struct sprdfb_device *dev)
{
	if(NULL != dev->panel->if_ctrl->panel_if_after_refresh){
		dev->panel->if_ctrl->panel_if_after_refresh(dev);
	}
}

#ifdef CONFIG_FB_DYNAMIC_FREQ_SCALING
void sprdfb_panel_change_fps(struct sprdfb_device *dev, int fps_level)
{
	if (dev->panel->ops->panel_change_fps!= NULL) {
		printk("sprdfb: [%s] fps_level= %d\n", __FUNCTION__,fps_level);
		dev->panel->ops->panel_change_fps(dev->panel,fps_level);
	}
}
#endif

#ifdef CONFIG_FB_ESD_SUPPORT
/*return value:  0--panel OK.1-panel has been reset*/
uint32_t sprdfb_panel_ESD_check(struct sprdfb_device *dev)
{
	int32_t result = 0;
	uint32_t if_status = 0;

//	printk("sprdfb: [%s] (%d, %d, %d)\n",__FUNCTION__, dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);

	dev->check_esd_time++;

	if(SPRDFB_PANEL_IF_EDPI == dev->panel_if_type){
		if (dev->panel->ops->panel_esd_check != NULL) {
			result = dev->panel->ops->panel_esd_check(dev->panel);
			pr_debug("sprdfb: [%s] panel check return %d\n", __FUNCTION__, result);
		}
	}else if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
#ifdef FB_CHECK_ESD_BY_TE_SUPPORT
		dev->esd_te_waiter++;
		dev->esd_te_done = 0;
		dispc_set_bits(BIT(1), DISPC_INT_EN);
		result  = wait_event_interruptible_timeout(dev->esd_te_queue,
			          dev->esd_te_done, msecs_to_jiffies(600));
		pr_debug("sprdfb: after wait (%d)\n", result);
		dispc_clear_bits(BIT(1), DISPC_INT_EN);
		if(!result){ /*time out*/
			printk("sprdfb: [%s] esd check  not got te signal!!!!\n", __FUNCTION__);
			dev->esd_te_waiter = 0;
			result = 0;
		}else{
			pr_debug("sprdfb: [%s] esd check  got te signal!\n", __FUNCTION__);
			result = 1;
		}
#else
		if (dev->panel->ops->panel_esd_check != NULL) {
			result = dev->panel->ops->panel_esd_check(dev->panel);
//			pr_debug("sprdfb: [%s] panel check return %d\n", __FUNCTION__, result);
		}

#endif
	}


	if(0 == dev->enable){
		printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
		return 0;
	}

	if(result == 0){
		dev->panel_reset_time++;

		if(SPRDFB_PANEL_IF_EDPI == dev->panel_if_type){
			if(NULL != dev->panel->if_ctrl->panel_if_get_status){
				if_status = dev->panel->if_ctrl->panel_if_get_status(dev);
			}
		}else if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
			if_status = 2; /*need reset dsi as default for dpi mode*/
		}

		if(0 == if_status){
			printk("sprdfb: [%s] fail! Need reset panel.(%d,%d,%d)\n",	__FUNCTION__,
                            dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);
			panel_reset(dev);

			if(0 == dev->enable){
				printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
				return 0;
			}

			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
		}else{
			printk("sprdfb: [%s] fail! Need reset panel and panel if!!!!(%d,%d,%d)\n",__FUNCTION__,
                            dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);
			dev->reset_dsi_time++;
			if(NULL != dev->panel->if_ctrl->panel_if_suspend){
				dev->panel->if_ctrl->panel_if_suspend(dev);
			}

			mdelay(10);

			if(0 == dev->enable){
				printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
				return 0;
			}

			panel_init(dev);
			panel_reset(dev);

			if(0 == dev->enable){
				printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
				return 0;
			}

			dev->panel->ops->panel_init(dev->panel);
			panel_ready(dev);
		}
		pr_debug("sprdfb: [%s]return 1\n",__FUNCTION__);
		return 1;
	}
//	pr_debug("sprdfb: [%s]return 0\n",__FUNCTION__);
	return 0;
}
#endif

void sprdfb_panel_shutdown(struct sprdfb_device *dev)
{
	dispc_write(0, DISPC_RSTN);
	mdelay(120);
}

#ifdef CONFIG_LCD_ESD_RECOVERY
void sprdfb_panel_ESD_reset(struct sprdfb_device *dev)
{
	if(NULL != dev->panel->if_ctrl->panel_if_suspend)
		dev->panel->if_ctrl->panel_if_suspend(dev);

	mdelay(10);

	/* 0. power on */
	if (dev->panel->ops->panel_power)
        	dev->panel->ops->panel_power(dev->panel, 1);
	/* 1. turn on mipi */
	panel_init(dev);
	/* 2. reset pin to high */
	if (dev->panel->ops->panel_before_resume)
		dev->panel->ops->panel_before_resume(dev->panel);
	else
		panel_before_resume(dev);

	panel_reset(dev);
	dev->panel->ops->panel_init(dev->panel);
	panel_ready(dev);
}
#endif

void sprdfb_panel_suspend(struct sprdfb_device *dev)
{
	if(unlikely(!dev || !dev->panel)) {
		pr_err("sprdfb: [%s], no dev\n", __func__);
		return;
	}

	pr_info("sprdfb: [%s], dev_id = %d\n", __func__, dev->dev_id);

	/* 1 send lcd sleep cmd or reset panel directly */
	if(dev->panel->suspend_mode == SEND_SLEEP_CMD){
		panel_sleep(dev);
	}else if (!dev->panel->ops->panel_pin_init){
		panel_reset(dev);
	}
#ifdef CONFIG_GEN_PANEL_OCTA
	dispc_stop(dev);
#endif
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

#ifdef CONFIG_GEN_PANEL_OCTA
	usleep_range(10000, 11000);
#endif

	if (dev->panel->ops->panel_power)
		dev->panel->ops->panel_power(dev->panel, 0);

	pr_info("[LCD] %s -\n", __func__);
}

void sprdfb_panel_start(struct sprdfb_device *dev)
{
	if (!dev || !dev->panel) {
		pr_err("[LCD] %s, no dev\n", __func__);
		return;
	}

	if (dev->panel->ops->panel_set_start)
		dev->panel->ops->panel_set_start(dev->panel);

	return;
}

void sprdfb_panel_resume(struct sprdfb_device *dev, bool from_deep_sleep)
{
	if (!dev || !dev->panel) {
		pr_err("[LCD] %s, no dev\n", __func__);
		return;
	}

	printk("[LCD] %s, + enable:%d, from_deep_sleep:%d\n",
			__func__, dev->enable, from_deep_sleep);

	if (from_deep_sleep) {
		/* 0. power on */
		if (dev->panel->ops->panel_power)
			dev->panel->ops->panel_power(dev->panel, 1);

		/* 1. turn on mipi */
		panel_init(dev);

		/* 2. reset pin to high */
		if (dev->panel->ops->panel_before_resume)
			dev->panel->ops->panel_before_resume(dev->panel);
		else
			panel_before_resume(dev);

		/* reset panel */
		panel_reset(dev);

#ifdef CONFIG_GEN_PANEL_OCTA
		/* clk/data lane enter HS */
		panel_ready(dev);
		usleep_range(5000, 6000);
		/* panel init */
		if (dev->panel->ops->panel_init)
			dev->panel->ops->panel_init(dev->panel);
#else
		/* panel init */
		if (dev->panel->ops->panel_init)
			dev->panel->ops->panel_init(dev->panel);
		/* clk/data lane enter HS */
		panel_ready(dev);
#endif
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
	pr_info("[LCD] %s -\n", __func__);

	return;
}

void sprdfb_panel_remove(struct sprdfb_device *dev)
{
	if(NULL == dev->panel){
		return;
	}

	/*Jessica TODO:close panel, i2c, spi, mipi*/
	if(NULL != dev->panel->if_ctrl->panel_if_uninit){
		dev->panel->if_ctrl->panel_if_uninit(dev);
	}
	dev->panel = NULL;
}


int sprdfb_panel_register(struct panel_cfg *cfg)
{
	pr_debug("sprdfb: [%s], panel id = %d\n",__FUNCTION__, cfg->dev_id);

	if(!panel_check(cfg)){
		printk("[LCD] %s : panel check fail!id = %d\n",__FUNCTION__,  cfg->dev_id);
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

	return 0;
}


