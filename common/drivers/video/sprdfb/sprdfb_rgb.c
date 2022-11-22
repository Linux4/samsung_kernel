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

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include "sprdfb.h"
#include "sprdfb_panel.h"
#include "sprdfb_dispc_reg.h"

extern struct ops_i2c sprdfb_i2c_ops;
extern struct ops_spi sprdfb_spi_ops;

extern bool sprdfb_i2c_init(struct sprdfb_device *dev);
extern bool sprdfb_i2c_uninit(struct sprdfb_device *dev);
extern bool sprdfb_spi_init(struct sprdfb_device *dev);
extern bool sprdfb_spi_uninit(struct sprdfb_device *dev);


static uint32_t rgb_readid(struct panel_spec *self)
{
	uint32_t id = 0;
	struct info_rgb *rgb = self->info.rgb;

	/* default id reg is 0 */
	if(SPRDFB_RGB_BUS_TYPE_I2C == rgb->cmd_bus_mode){
		rgb->bus_info.i2c->ops->i2c_read_16bits(0x0, false, (uint16_t*)&id, false);
	}else{
		rgb->bus_info.spi->ops->spi_send_cmd(0x0);
		rgb->bus_info.spi->ops->spi_read(&id);
	}

	return id;
}

static void rgb_dispc_init_config(struct panel_spec *panel)
{
	uint32_t reg_val = dispc_read(DISPC_DPI_CTRL);

	pr_debug("sprdfb: [%s]\n", __FUNCTION__);

	if(NULL == panel){
		printk(KERN_ERR "sprdfb: [%s] fail.(Invalid Param)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_PANEL_TYPE_RGB != panel->type){
		printk(KERN_ERR "sprdfb: [%s] fail.(not  mcu panel)\n", __FUNCTION__);
		return;
	}

	/*use dpi as interface*/
	dispc_clear_bits((3<<1), DISPC_CTRL);

	/*h sync pol*/
	if(SPRDFB_POLARITY_NEG == panel->info.rgb->h_sync_pol){
		reg_val |= (1<<0);
	}

	/*v sync pol*/
	if(SPRDFB_POLARITY_NEG == panel->info.rgb->v_sync_pol){
		reg_val |= (1<<1);
	}

	/*de sync pol*/
	if(SPRDFB_POLARITY_NEG == panel->info.rgb->de_pol){
		reg_val |= (1<<2);
	}

#ifdef CONFIG_DPI_SINGLE_RUN
	/*single run mode*/
	reg_val |= (1<<3);
#endif

	/*dpi bits*/
	switch(panel->info.rgb->video_bus_width){
	case 16:
		break;
	case 18:
		reg_val |= (1 << 6);
		break;
	case 24:
		reg_val |= (2 << 6);
		break;
	default:
		break;
	}

	dispc_write(reg_val, DISPC_DPI_CTRL);

	pr_debug("sprdfb: [%s] DISPC_DPI_CTRL = %d\n", __FUNCTION__, dispc_read(DISPC_DPI_CTRL));
}

static void rgb_dispc_set_timing(struct sprdfb_device *dev)
{
	pr_debug("sprdfb: [%s]\n", __FUNCTION__);

	dispc_write(dev->panel_timing.rgb_timing[RGB_LCD_H_TIMING], DISPC_DPI_H_TIMING);
	dispc_write(dev->panel_timing.rgb_timing[RGB_LCD_V_TIMING], DISPC_DPI_V_TIMING);
}

uint32_t rgb_calc_h_timing(struct timing_rgb *timing)
{
#if 0
	uint32_t  clk_rate;
	uint32_t  hsync, hbp, hfp;
	struct clk * clk = NULL;

	if(NULL == timing){
		printk(KERN_ERR "sprdfb: [%s]: Invalid Param\n", __FUNCTION__);
		return 0;
	}

	clk_get(NULL,"clk_dispc_dpi");
	clk_rate = clk_get_rate(clk) / 1000000;

	pr_debug(KERN_INFO "sprdfb: [%s] clk_rate: 0x%x\n", __FUNCTION__, clk_rate);

/********************************************************
	* we assume : t = ? ns, dispc_dpi = ? MHz   so
	*      1ns need cycle  :  dispc_dpi /1000
	*      tns need cycles :  t * dispc_dpi / 1000
	*
	********************************************************/
#define MAX_DPI_HSYNC_TIMING_VALUE	255
#define MAX_DPI_HBP_TIMING_VALUE	4095
#define MAX_DPI_HFP_TIMING_VALUE	4095
#define DPI_CYCLES(ns) (( (ns) * clk_rate + 1000 - 1)/ 1000)

	/* ceiling*/
	hsync = DPI_CYCLES(timing->hsync);
	if (hsync > MAX_DPI_HSYNC_TIMING_VALUE) {
		hsync = MAX_DPI_HSYNC_TIMING_VALUE ;
	}

	hbp = DPI_CYCLES(timing->hbp);
	if (hbp > MAX_DPI_HSYNC_TIMING_VALUE) {
		hbp = MAX_DPI_HSYNC_TIMING_VALUE ;
	}

	hfp = DPI_CYCLES (timing->hfp);
	if (hfp > MAX_DPI_HFP_TIMING_VALUE) {
		hfp = MAX_DPI_HFP_TIMING_VALUE ;
	}

	return (hsync | (hbp << 8) | (hfp << 20));
#else
	return  (timing->hsync | (timing->hbp << 8) | (timing->hfp << 20));
#endif
}


uint32_t rgb_calc_v_timing(struct timing_rgb *timing)
{
	return (timing->vsync| (timing->vbp << 8) | (timing->vfp << 20));
}

static int32_t sprdfb_rgb_panel_check(struct panel_spec *panel)
{
	if(NULL == panel){
		printk("sprdfb: [%s] fail. (Invalid param)\n", __FUNCTION__);
		return false;
	}

	if(SPRDFB_PANEL_TYPE_RGB != panel->type){
		printk("sprdfb: [%s] fail. (not rgb param)\n", __FUNCTION__);
		return false;
	}

	pr_debug("sprdfb: [%s]\n",__FUNCTION__);

	return true;
}

static void sprdfb_rgb_panel_mount(struct sprdfb_device *dev)
{
	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid Param\n", __FUNCTION__);
		return;
	}

	pr_debug(KERN_INFO "sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	dev->panel_if_type = SPRDFB_PANEL_IF_DPI;

	if(SPRDFB_RGB_BUS_TYPE_I2C == dev->panel->info.rgb->cmd_bus_mode){
		dev->panel->info.rgb->bus_info.i2c->ops = &sprdfb_i2c_ops;
	}else{
		dev->panel->info.rgb->bus_info.spi->ops = &sprdfb_spi_ops;
	}

	if(NULL == dev->panel->ops->panel_readid){
		dev->panel->ops->panel_readid = rgb_readid;
	}

	dev->panel_timing.rgb_timing[RGB_LCD_H_TIMING] = rgb_calc_h_timing(dev->panel->info.rgb->timing);
	dev->panel_timing.rgb_timing[RGB_LCD_V_TIMING] = rgb_calc_v_timing(dev->panel->info.rgb->timing);
}

static bool sprdfb_rgb_panel_init(struct sprdfb_device *dev)
{
	bool ret = false;
	if(SPRDFB_RGB_BUS_TYPE_I2C == dev->panel->info.rgb->cmd_bus_mode){
		ret = sprdfb_i2c_init(dev);
	}else if(SPRDFB_RGB_BUS_TYPE_SPI == dev->panel->info.rgb->cmd_bus_mode) {
		ret = sprdfb_spi_init(dev);
	}

	if(!ret) {
		printk(KERN_ERR "sprdfb: [%s]: bus init fail!\n", __FUNCTION__);
		return false;
	}

	rgb_dispc_init_config(dev->panel);
	rgb_dispc_set_timing(dev);

	if(! dev->panel_ready){
	/*Nt35516 need vsync before panel init*/
		dispc_set_bits(BIT(4), DISPC_CTRL);
		udelay(100);
		dispc_clear_bits(BIT(4), DISPC_CTRL);
	}
	return true;
}

static void sprdfb_rgb_panel_uninit(struct sprdfb_device *dev)
{
	if(SPRDFB_RGB_BUS_TYPE_I2C == dev->panel->info.rgb->cmd_bus_mode){
		sprdfb_i2c_uninit(dev);
	}else if(SPRDFB_RGB_BUS_TYPE_SPI == dev->panel->info.rgb->cmd_bus_mode) {
		sprdfb_spi_uninit(dev);
	}
}

struct panel_if_ctrl sprdfb_rgb_ctrl = {
	.if_name		= "rgb",
	.panel_if_check		= sprdfb_rgb_panel_check,
	.panel_if_mount		 	= sprdfb_rgb_panel_mount,
	.panel_if_init		= sprdfb_rgb_panel_init,
	.panel_if_uninit		= sprdfb_rgb_panel_uninit,
	.panel_if_before_refresh	= NULL,
	.panel_if_after_refresh	= NULL,
};
