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
#include <linux/err.h>
#ifdef CONFIG_OF
#include <linux/fb.h>
#endif

#include "sprdfb.h"
#include "sprdfb_panel.h"
#include "sprdfb_dispc_reg.h"
#include "sprdfb_lcdc_reg.h"

//#define  CONFIG_FB_NO_FMARK    /*Jessica for FPGA test*/

static int32_t lcdc_mcu_send_cmd(uint32_t cmd)
{
	int i = 0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting!\n", __FUNCTION__);
		}
	}

	lcdc_write(cmd, LCM_CMD);

	return 0;
}

static int32_t lcdc_mcu_send_cmd_data(uint32_t cmd, uint32_t data)
{
	int i = 0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting 1!\n", __FUNCTION__);
		}
	}

	lcdc_write(cmd, LCM_CMD);

	i = 0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting 2!\n", __FUNCTION__);
		}
	}

	lcdc_write(data, LCM_DATA);

	return 0;
}

static int32_t lcdc_mcu_send_data(uint32_t data)
{
	int i = 0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting!\n", __FUNCTION__);
		}
	}

	lcdc_write(data, LCM_DATA);

	return 0;
}

static uint32_t lcdc_mcu_read_data(void)
{
	int i = 0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(lcdc_read(LCM_CTRL) & BIT(20)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting!\n", __FUNCTION__);
		}
	}
	lcdc_write(1 << 24, LCM_DATA);
	udelay(50);
	return lcdc_read(LCM_RDDATA);
}

static int32_t dispc_mcu_send_cmd(uint32_t cmd)
{
	int i = 0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting!\n", __FUNCTION__);
		}
	}

	dispc_write(cmd, DISPC_DBI_CMD);

	return 0;
}

static int32_t dispc_mcu_send_cmd_data(uint32_t cmd, uint32_t data)
{
	int i=0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting 1!\n", __FUNCTION__);
		}
	}

	dispc_write(cmd, DISPC_DBI_CMD);

	i = 0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting 2!\n", __FUNCTION__);
		}
	}

	dispc_write(data, DISPC_DBI_DATA);

	return 0;
}

static int32_t dispc_mcu_send_data(uint32_t data)
{
	int i = 0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting!\n", __FUNCTION__);
		}
	}

	dispc_write(data, DISPC_DBI_DATA);

	return 0;
}

static uint32_t dispc_mcu_read_data(void)
{
	int i = 0;
	/* busy wait for ahb fifo full sign's disappearance */
	while(dispc_read(DISPC_DBI_QUEUE) & BIT(5)){
		if(0x0 == ++i%10000){
			printk("sprdfb: [%s] warning: busy waiting!\n", __FUNCTION__);
		}
	}
	dispc_write(1 << 24, DISPC_DBI_DATA);
	udelay(50);
	return dispc_read(DISPC_DBI_RDATA);
}

static struct ops_mcu dispc_mcu_ops = {
	.send_cmd = dispc_mcu_send_cmd,
	.send_cmd_data = dispc_mcu_send_cmd_data,
	.send_data = dispc_mcu_send_data,
	.read_data = dispc_mcu_read_data,
};

static struct ops_mcu lcdc_mcu_ops = {
	.send_cmd = lcdc_mcu_send_cmd,
	.send_cmd_data = lcdc_mcu_send_cmd_data,
	.send_data = lcdc_mcu_send_data,
	.read_data = lcdc_mcu_read_data,
};

#ifdef CONFIG_OF
static uint32_t mcu_calc_timing(struct timing_mcu *timing, struct sprdfb_device *dev)
#else
static uint32_t mcu_calc_timing(struct timing_mcu *timing, uint16_t dev_id)
#endif
{
	uint32_t  clk_rate;
	uint32_t  rcss, rlpw, rhpw, wcss, wlpw, whpw;
	struct clk * clk = NULL;

	if(NULL == timing){
		printk(KERN_ERR "sprdfb: [%s]: Invalid Param\n", __FUNCTION__);
		return 0;
	}

#ifdef CONFIG_OF
	if(SPRDFB_MAINLCD_ID == dev->dev_id){
		clk = of_clk_get_by_name(dev->of_dev->of_node,"dispc_dbi_clk");
#else
	if(SPRDFB_MAINLCD_ID == dev_id){
		clk = clk_get(NULL,"clk_dispc_dbi");
#endif
		if (IS_ERR(clk)) {
			printk(KERN_WARNING "sprdfb: get clk_dispc_dbi fail!\n");
		} else {
			pr_debug(KERN_INFO "sprdfb: get clk_dispc_dbi ok!\n");
		}
	}else{
	#ifdef CONFIG_FB_SC8825
		clk = clk_get(NULL, "clk_ahb");
		if (IS_ERR(clk)) {
			printk(KERN_WARNING "sprdfb: get clk_ahb fail!\n");
		} else {
			pr_debug(KERN_INFO "sprdfb: get clk_ahb ok!\n");
		}
	#endif
	}
//	clk_rate = clk_get_rate(clk) / 1000000;
	clk_rate = 250;	// dummy 250M Hz

#ifdef CONFIG_OF
	pr_debug(KERN_INFO "sprdfb: [%s] clk_rate: 0x%x, dev_id = %d\n", __FUNCTION__, clk_rate, dev->dev_id);
#else
	pr_debug(KERN_INFO "sprdfb: [%s] clk_rate: 0x%x, dev_id = %d\n", __FUNCTION__, clk_rate, dev_id);
#endif
	/********************************************************
	* we assume : t = ? ns, dispc_dbi = ? MHz   so
	*      1ns need cycle  :  dispc_dbi /1000
	*      tns need cycles :  t * dispc_dbi / 1000
	*
	********************************************************/
#define MAX_DBI_RWCSS_TIMING_VALUE	15
#define MAX_DBI_RWLPW_TIMING_VALUE	63
#define MAX_DBI_RWHPW_TIMING_VALUE	63
#define DBI_CYCLES(ns) (( (ns) * clk_rate + 1000 - 1)/ 1000)

	/* ceiling*/
	rcss = DBI_CYCLES(timing->rcss);
	if (rcss > MAX_DBI_RWCSS_TIMING_VALUE) {
		rcss = MAX_DBI_RWCSS_TIMING_VALUE ;
	}

	rlpw = DBI_CYCLES(timing->rlpw);
	if (rlpw > MAX_DBI_RWLPW_TIMING_VALUE) {
		rlpw = MAX_DBI_RWLPW_TIMING_VALUE ;
	}

	rhpw = DBI_CYCLES (timing->rhpw);
	if (rhpw > MAX_DBI_RWHPW_TIMING_VALUE) {
		rhpw = MAX_DBI_RWHPW_TIMING_VALUE ;
	}

	wcss = DBI_CYCLES(timing->wcss);
	if (wcss > MAX_DBI_RWCSS_TIMING_VALUE) {
		wcss = MAX_DBI_RWCSS_TIMING_VALUE ;
	}

	wlpw = DBI_CYCLES(timing->wlpw);
	if (wlpw > MAX_DBI_RWLPW_TIMING_VALUE) {
		wlpw = MAX_DBI_RWLPW_TIMING_VALUE ;
	}

#ifndef CONFIG_LCD_CS_ALWAYS_LOW
	 /* dispc/lcdc will waste one cycle because CS pulse will use one cycle*/
	whpw = DBI_CYCLES (timing->whpw) - 1;
#else
	whpw = DBI_CYCLES (timing->whpw) ;
#endif
	if (whpw > MAX_DBI_RWHPW_TIMING_VALUE) {
		whpw = MAX_DBI_RWHPW_TIMING_VALUE ;
	}

	return (whpw | (wlpw << 6) | (wcss << 12)
			| (rhpw << 16) |(rlpw << 22) | (rcss << 28));
}

static uint32_t mcu_readid(struct panel_spec *self)
{
	uint32_t id = 0;

	/* default id reg is 0 */
	self->info.mcu->ops->send_cmd(0x0);

	if(self->info.mcu->bus_width == 8) {
		id = (self->info.mcu->ops->read_data()) & 0xff;
		id <<= 8;
		id |= (self->info.mcu->ops->read_data()) & 0xff;
	} else {
		id = self->info.mcu->ops->read_data();
	}

	return id;
}

#ifdef CONFIG_FB_LCD_CS1
/*cs1*/
void mcu_dispc_init_config(struct panel_spec *panel)
{
	uint32_t reg_val = 0;

	pr_debug("sprdfb: [%s] for cs1\n", __FUNCTION__);

	if(NULL == panel){
		printk(KERN_ERR "sprdfb: [%s] fail.(Invalid Param)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_PANEL_TYPE_MCU != panel->type){
		printk(KERN_ERR "sprdfb: [%s] fail.(not mcu panel)\n", __FUNCTION__);
		return;
	}

	/*use dbi as interface*/
	dispc_set_bits((2<<1), DISPC_CTRL);

	/* CS1 bus mode [BIT8]: 8080/6800 */
	switch (panel->info.mcu->bus_mode) {
	case LCD_BUS_8080:
		break;
	case LCD_BUS_6800:
		reg_val |= (1<<8);
		break;
	default:
		break;
	}
	/* CS1 bus width [BIT11:9] */
	switch (panel->info.mcu->bus_width) {
	case 8:
		break;
	case 9:
		reg_val |= (1 << 9);
		break;
	case 16:
		reg_val |= (2 << 9);
		break;
	case 18:
		reg_val |= (3 << 9) ;
		break;
	case 24:
		reg_val |= (4 << 9);
		break;
	default:
		break;
	}

	/*CS1 pixel bits [BIT13:12]*/
	switch (panel->info.mcu->bpp) {
	case 16:
		break;
	case 18:
		reg_val |= (1 << 12) ;
		break;
	case 24:
		reg_val |= (2 << 12);
		break;
	default:
		break;
	}

#ifndef CONFIG_FB_NO_FMARK
	/*TE enable*/
	reg_val |= (1 << 16);
	if(SPRDFB_POLARITY_NEG == panel->info.mcu->te_pol){
		reg_val |= (1<< 17);
	}
	dispc_write(panel->info.mcu->te_sync_delay, DISPC_TE_SYNC_DELAY);
#endif

#ifdef CONFIG_LCD_CS_ALWAYS_LOW
	/*CS alway low mode*/
	reg_val |= (1<<21);
#else
	/*CS not alway low mode*/
#endif

	/*CS1 selected*/
	reg_val |= (1 << 20);

	dispc_write(reg_val, DISPC_DBI_CTRL);

	pr_debug("sprdfb: [%s] DISPC_DBI_CTRL = %d\n", __FUNCTION__, dispc_read(DISPC_DBI_CTRL));
}

void mcu_dispc_set_timing(struct sprdfb_device *dev, uint32_t type)
{
	pr_debug("sprdfb: [%s] for cs1, type = %d\n", __FUNCTION__, type);

	switch (type)
	{
	case MCU_LCD_REGISTER_TIMING:
		dispc_write(dev->panel_timing.mcu_timing[MCU_LCD_REGISTER_TIMING],DISPC_DBI_TIMING1);
		break;

	case MCU_LCD_GRAM_TIMING:
		dispc_write(dev->panel_timing.mcu_timing[MCU_LCD_GRAM_TIMING],DISPC_DBI_TIMING1);
		break;
	default:
		break;
	}
}

void mcu_lcdc_init_config(struct panel_spec *panel)
{
	uint32_t reg_val = 0;

	pr_debug("sprdfb: [%s] for cs1\n", __FUNCTION__);

	if(NULL == panel){
		printk(KERN_ERR "sprdfb: [%s] fail.(Invalid Param)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_PANEL_TYPE_MCU != panel->type){
		printk(KERN_ERR "sprdfb: [%s] fail.(not mcu panel)\n", __FUNCTION__);
		return;
	}

	/* CS1 bus mode [BIT8]: 8080/6800 */
	switch (panel->info.mcu->bus_mode) {
	case LCD_BUS_8080:
		break;
	case LCD_BUS_6800:
		reg_val |= (1<<8);
		break;
	default:
		break;
	}
	/* CS1 bus width [BIT11:9] */
	switch (panel->info.mcu->bus_width) {
	case 8:
		break;
	case 9:
		reg_val |= (1 << 9);
		break;
	case 16:
		reg_val |= (2 << 9);
		break;
	case 18:
		reg_val |= (3 << 9) ;
		break;
	case 24:
		reg_val |= (4 << 9);
		break;
	default:
		break;
	}

	/*CS1 pixel bits [BIT13:12]*/
	switch (panel->info.mcu->bpp) {
	case 16:
		break;
	case 18:
		reg_val |= (1 << 12) ;
		break;
	case 24:
		reg_val |= (2 << 12);
		break;
	default:
		break;
	}

#ifdef CONFIG_FB_NO_FMARK
	/*TE enable*/
	lcdc_set_bits(BIT(1), LCDC_CTRL);
#else
	if(SPRDFB_POLARITY_NEG == panel->info.mcu->te_pol){
		lcdc_set_bits(BIT(2), LCDC_CTRL);
	}
	dispc_write(panel->info.mcu->te_sync_delay, LCDC_SYNC_DELAY);
#endif

	/*CS alway low mode*/
	reg_val |= (1<<17);

	/*CS1 selected*/
	reg_val |= (1 << 16);

	lcdc_write(reg_val, LCM_CTRL);

	pr_debug("sprdfb: [%s] LCM_CTRL = %d\n", __FUNCTION__, lcdc_read(LCM_CTRL));
}

void mcu_lcdc_set_timing(struct sprdfb_device *dev, uint32_t type)
{
	pr_debug("sprdfb: [%s] for cs1, type = %d\n", __FUNCTION__, type);

	switch (type)
	{
	case MCU_LCD_REGISTER_TIMING:
		lcdc_write(dev->panel_timing.mcu_timing[MCU_LCD_REGISTER_TIMING],LCM_TIMING1);
		break;

	case MCU_LCD_GRAM_TIMING:
		lcdc_write(dev->panel_timing.mcu_timing[MCU_LCD_GRAM_TIMING],LCM_TIMING1);
		break;
	default:
		break;
	}
}

#else
/*cs0*/
void mcu_dispc_init_config(struct panel_spec *panel)
{
	uint32_t reg_val = 0;

	pr_debug("sprdfb: [%s] for cs0\n", __FUNCTION__);

	if(NULL == panel){
		printk(KERN_ERR "sprdfb: [%s] fail.(Invalid Param)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_PANEL_TYPE_MCU != panel->type){
		printk(KERN_ERR "sprdfb: [%s] fail.(not mcu panel)\n", __FUNCTION__);
		return;
	}

	/*use dbi as interface*/
	dispc_set_bits((2<<1), DISPC_CTRL);

	/* CS0 bus mode [BIT0]: 8080/6800 */
	switch (panel->info.mcu->bus_mode) {
	case LCD_BUS_8080:
		break;
	case LCD_BUS_6800:
		reg_val |= 1;
		break;
	default:
		break;
	}
	/* CS0 bus width [BIT3:1] */
	switch (panel->info.mcu->bus_width) {
	case 8:
		break;
	case 9:
		reg_val |= (1 << 1);
		break;
	case 16:
		reg_val |= (2 << 1);
		break;
	case 18:
		reg_val |= (3 << 1) ;
		break;
	case 24:
		reg_val |= (4 << 1);
		break;
	default:
		break;
	}

	/*CS0 pixel bits [BIT5:4]*/
	switch (panel->info.mcu->bpp) {
	case 16:
		break;
	case 18:
		reg_val |= (1 << 4) ;
		break;
	case 24:
		reg_val |= (2 << 4);
		break;
	default:
		break;
	}

#ifndef CONFIG_FB_NO_FMARK
	/*TE enable*/
	reg_val |= (1 << 16);
	if(SPRDFB_POLARITY_NEG == panel->info.mcu->te_pol){
		reg_val |= (1<< 17);
	}
	dispc_write(panel->info.mcu->te_sync_delay, DISPC_TE_SYNC_DELAY);
#endif

#ifdef CONFIG_LCD_CS_ALWAYS_LOW
	/*CS alway low mode*/
	reg_val |= (1<<21);
#else
	/*CS not alway low mode*/
#endif

	/*CS0 selected*/

	dispc_write(reg_val, DISPC_DBI_CTRL);

	pr_debug("sprdfb: [%s] DISPC_DBI_CTRL = %d\n", __FUNCTION__, dispc_read(DISPC_DBI_CTRL));
}

void mcu_dispc_set_timing(struct sprdfb_device *dev, uint32_t type)
{
	pr_debug("sprdfb: [%s] for cs0, type = %d\n", __FUNCTION__, type);

	switch (type)
	{
	case MCU_LCD_REGISTER_TIMING:
		dispc_write(dev->panel_timing.mcu_timing[MCU_LCD_REGISTER_TIMING],DISPC_DBI_TIMING0);
		break;

	case MCU_LCD_GRAM_TIMING:
		dispc_write(dev->panel_timing.mcu_timing[MCU_LCD_GRAM_TIMING],DISPC_DBI_TIMING0);
		break;
	default:
		break;
	}
}

void mcu_lcdc_init_config(struct panel_spec *panel)
{
	uint32_t reg_val = 0;

	pr_debug("sprdfb: [%s] for cs0\n", __FUNCTION__);

	if(NULL == panel){
		printk(KERN_ERR "sprdfb: [%s] fail.(Invalid Param)\n", __FUNCTION__);
		return;
	}

	if(SPRDFB_PANEL_TYPE_MCU != panel->type){
		printk(KERN_ERR "sprdfb: [%s] fail.(not mcu panel)\n", __FUNCTION__);
		return;
	}

	/* CS1 bus mode [BIT8]: 8080/6800 */
	switch (panel->info.mcu->bus_mode) {
	case LCD_BUS_8080:
		break;
	case LCD_BUS_6800:
		reg_val |= 1;
		break;
	default:
		break;
	}
	/* CS1 bus width [BIT11:9] */
	switch (panel->info.mcu->bus_width) {
	case 8:
		break;
	case 9:
		reg_val |= (1 <<0);
		break;
	case 16:
		reg_val |= (2 << 1);
		break;
	case 18:
		reg_val |= (3 << 1) ;
		break;
	case 24:
		reg_val |= (4 << 1);
		break;
	default:
		break;
	}

	/*CS1 pixel bits [BIT13:12]*/
	switch (panel->info.mcu->bpp) {
	case 16:
		break;
	case 18:
		reg_val |= (1 << 4) ;
		break;
	case 24:
		reg_val |= (2 << 4);
		break;
	default:
		break;
	}

#ifdef CONFIG_FB_NO_FMARK
	/*TE enable*/
	lcdc_set_bits(BIT(1), LCDC_CTRL);
#else
	if(SPRDFB_POLARITY_NEG == panel->info.mcu->te_pol){
		lcdc_set_bits(BIT(2), LCDC_CTRL);
	}
	dispc_write(panel->info.mcu->te_sync_delay, LCDC_SYNC_DELAY);
#endif

	/*CS alway low mode*/
	reg_val |= (1<<17);

	lcdc_write(reg_val, LCM_CTRL);

	pr_debug("sprdfb: [%s] LCM_CTRL = %d\n", __FUNCTION__, lcdc_read(LCM_CTRL));
}

void mcu_lcdc_set_timing(struct sprdfb_device *dev, uint32_t type)
{
	pr_debug("sprdfb: [%s] for cs0, type = %d\n", __FUNCTION__, type);

	switch (type)
	{
	case MCU_LCD_REGISTER_TIMING:
		lcdc_write(dev->panel_timing.mcu_timing[MCU_LCD_REGISTER_TIMING],LCM_TIMING0);
		break;

	case MCU_LCD_GRAM_TIMING:
		lcdc_write(dev->panel_timing.mcu_timing[MCU_LCD_GRAM_TIMING],LCM_TIMING0);
		break;
	default:
		break;
	}
}

#endif

static int32_t sprdfb_mcu_panel_check(struct panel_spec *panel)
{
	struct info_mcu* mcu_info = NULL;
	bool rval = true;

	if(NULL == panel){
		printk("sprdfb: [%s] fail. (Invalid param)\n", __FUNCTION__);
		return false;
	}

	if(SPRDFB_PANEL_TYPE_MCU != panel->type){
		printk("sprdfb: [%s] fail. (not mcu param)\n", __FUNCTION__);
		return false;
	}

	mcu_info = panel->info.mcu;

	pr_debug("sprdfb: [%s]: bus width= %d, bpp = %d\n",__FUNCTION__, mcu_info->bus_width, mcu_info->bpp);

	switch(mcu_info->bus_width){
	case 8:
		if((16 != mcu_info->bpp) && (24 != mcu_info->bpp)){
			rval = false;
		}
		break;
	case 9:
		if(18 != mcu_info->bpp) {
			rval = false;
		}
		break;
	case 16:
		if((16 != mcu_info->bpp) && (18 != mcu_info->bpp) &&
			(24 != mcu_info->bpp)){
			rval = false;
		}
		break;
	case 18:
		if(18 != mcu_info->bpp){
			rval = false;
		}
		break;
	case 24:
		if(24 != mcu_info->bpp){
			rval = false;
		}
		break;
	default:
		rval = false;
		break;
	}

	if(!rval){
		printk(KERN_ERR "sprdfb: mcu_panel_check return false!\n");
	}

	return rval;
}

static void sprdfb_mcu_panel_mount(struct sprdfb_device *dev)
{
	struct timing_mcu* timing = NULL;

	if((NULL == dev) || (NULL == dev->panel)){
		printk(KERN_ERR "sprdfb: [%s]: Invalid Param\n", __FUNCTION__);
		return;
	}

	pr_debug(KERN_INFO "sprdfb: [%s], dev_id = %d\n",__FUNCTION__, dev->dev_id);

	dev->panel_if_type = SPRDFB_PANEL_IF_DBI;

	if(SPRDFB_MAINLCD_ID == dev->dev_id){
		dev->panel->info.mcu->ops = &dispc_mcu_ops;
	}else{
	#ifdef CONFIG_FB_SC8825
		dev->panel->info.mcu->ops = &lcdc_mcu_ops;
	#endif
	}

	if(NULL == dev->panel->ops->panel_readid){
		dev->panel->ops->panel_readid = mcu_readid;
	}

	timing = dev->panel->info.mcu->timing;
#ifdef CONFIG_OF
	dev->panel_timing.mcu_timing[MCU_LCD_REGISTER_TIMING] = mcu_calc_timing(timing, dev);
#else
	dev->panel_timing.mcu_timing[MCU_LCD_REGISTER_TIMING] = mcu_calc_timing(timing, dev->dev_id);
#endif
	timing++;
#ifdef CONFIG_OF
	dev->panel_timing.mcu_timing[MCU_LCD_GRAM_TIMING] = mcu_calc_timing(timing, dev);
#else
	dev->panel_timing.mcu_timing[MCU_LCD_GRAM_TIMING] = mcu_calc_timing(timing, dev->dev_id);
#endif
}

static bool sprdfb_mcu_panel_init(struct sprdfb_device *dev)
{
	if(SPRDFB_MAINLCD_ID == dev->dev_id){
		mcu_dispc_init_config(dev->panel);
		mcu_dispc_set_timing(dev, MCU_LCD_REGISTER_TIMING);
	}else{
	#ifdef CONFIG_FB_SC8825
		mcu_lcdc_init_config(dev->panel);
		mcu_lcdc_set_timing(dev, MCU_LCD_REGISTER_TIMING);
	#endif
	}
	return true;
}

static void sprdfb_mcu_panel_before_refresh(struct sprdfb_device *dev)
{
	if(SPRDFB_MAINLCD_ID == dev->dev_id){
		mcu_dispc_set_timing(dev, MCU_LCD_GRAM_TIMING);
	}else{
	#ifdef CONFIG_FB_SC8825
		mcu_lcdc_set_timing(dev, MCU_LCD_GRAM_TIMING);
	#endif
	}
}

static void sprdfb_mcu_panel_after_refresh(struct sprdfb_device *dev)
{
	if(SPRDFB_MAINLCD_ID == dev->dev_id){
		mcu_dispc_set_timing(dev, MCU_LCD_REGISTER_TIMING);
	}else{
	#ifdef CONFIG_FB_SC8825
		mcu_lcdc_set_timing(dev, MCU_LCD_REGISTER_TIMING);
	#endif
	}
}

struct panel_if_ctrl sprdfb_mcu_ctrl = {
	.if_name		= "mcu",
	.panel_if_check		= sprdfb_mcu_panel_check,
	.panel_if_mount		= sprdfb_mcu_panel_mount,
	.panel_if_init		= sprdfb_mcu_panel_init,
	.panel_if_before_refresh	= sprdfb_mcu_panel_before_refresh,
	.panel_if_after_refresh	= sprdfb_mcu_panel_after_refresh,
};
