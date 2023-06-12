/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
 
#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#endif
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include "lcm_drv.h"
#include "ddp_hal.h"
#include <linux/spinlock.h>


#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define FRAME_WIDTH  (540)
#define FRAME_HEIGHT (960)
/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */
static LCM_UTIL_FUNCS lcm_util;
#define UDELAY(n)		(lcm_util.udelay(n))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define REGFLAG_DELAY		(0XFD)
#define REGFLAG_END_OF_TABLE	(0xFE)	/* END OF REGISTERS MARKER */
/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)\
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)\
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)\
	lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)\
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
/* #define read_reg lcm_util.dsi_read_reg() */
#define read_reg_v2(cmd, buffer, buffer_size)\
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

/*lcd & blic control*/
#define set_gpio_lcd_rst(cmd)\
		lcm_util.set_gpio_lcd_rst_enp(cmd)
#define set_gpio_lcd_bias(cmd)\
			lcm_util.set_gpio_lcd_bias_enp(cmd)
#define set_gpio_blic_ctl(cmd)\
				lcm_util.set_gpio_blic_ctl_enp(cmd)	
#define set_gpio_blic_en(cmd)\
			lcm_util.set_gpio_blic_en_enp(cmd)
/* #define LCM_DSI_CMD_MODE */
struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};
static struct LCM_setting_table lcm_initialization_setting[] = {
	/*
	Note :

	Data ID will depends on the following rule.

	count of parameters > 1	=> Data ID = 0x39
	count of parameters = 1	=> Data ID = 0x15
	count of parameters = 0	=> Data ID = 0x05

	Structure Format :

	{DCS command, count of parameters, {parameter list}}
	{REGFLAG_DELAY, milliseconds of time, {}},

	...

	Setting ending by predefined flag

	{REGFLAG_END_OF_TABLE, 0x00, {}}
	*/


	/*must use 0x39 for init setting for all register.*/
	/* Start - Initializing Sequence (1)*/
	{ 0xF0, 2, 	{0x5A, 0x5A} },
	{ 0xF1, 2, 	{0x5A, 0x5A} },
	{ 0xFC, 2, 	{0xA5, 0xA5} },

	{ 0x11, 0, 	{0x00} },
	{ REGFLAG_DELAY, 120, {} },

	/* Start - Initializing Sequence (3)*/
	{ 0xB1, 1,	{0x93} },
	{ 0xB5, 1,	{0x10} },
	{ 0xF4, 17,	{0x01, 0x10, 0x32 ,0x00, 0x24, 0x26, 0x28, 0x27, 0x27,
			0x27, 0xB7, 0x2B, 0x2C, 0x65, 0x6A, 0x34, 0x20} },
	{ 0xEF, 20,	{0x01, 0x01, 0x81 ,0x22, 0x83, 0x04, 0x05, 0x00, 0x00,
			0x00, 0x28, 0x81, 0x00, 0x21, 0x21, 0x03, 0x03, 0x40,
			0x00, 0x10} },
	{ 0xF2, 8,	{0x19, 0x04, 0x08 ,0x08, 0x08, 0x44, 0x28, 0x00} },
	{ 0xF6, 6,	{0x63, 0x23, 0x15 ,0x00, 0x00, 0x06} },
	{ 0xE1, 6,	{0x01, 0xFF, 0x01 ,0x1B, 0x20, 0x17} },
	{ 0xE2, 3,	{0xED, 0xC7, 0x23} },
	{ 0xF7, 38,	{0x01, 0x01, 0x01, 0x0A, 0x0B, 0x00, 0x1A, 0x05, 0x1B,
			0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x01, 0x01, 0x01, 0x08, 0x09, 0x00, 0x1A, 0x04,
			0x1B, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x01} },
	{ 0x36, 1,	{0X10} },

	/* Start - Gamma Setting Sequence (4)*/
	{ 0xFA , 39,	{0x11, 0x3B, 0x33, 0x2A, 0x1A, 0x09, 0x08, 0x04, 0x03,
			0x07, 0x05, 0x06, 0x17, 0x12, 0x3B, 0x33, 0x2A, 0x1D,
			0x0B, 0x07, 0x04, 0x03, 0x07, 0x05, 0x07, 0x17, 0x10,
			0x3B, 0x33, 0x2A, 0x1A, 0x07, 0x05, 0x02, 0x02, 0x05,
			0x03, 0x04, 0x15} },

	{ 0xFB, 39,	{0x11, 0x3B, 0x33, 0x2A, 0x1A, 0x0A, 0x08, 0x04, 0x03,
			0x06, 0x05, 0x06, 0x17, 0x12, 0x3B, 0x33, 0x2A, 0x1D, 
			0x0B, 0x07, 0x04, 0x03, 0x07, 0x05, 0x07, 0x17, 0x10,
			0x3B, 0x33, 0x2A, 0x1A, 0x08, 0x05, 0x02, 0x02, 0x05,
			0x03, 0x04, 0x14} },

	/*Sleep out*/
	{ 0x29, 0,	{0x00} },
	{ REGFLAG_DELAY, 20, {} },

	/* Start - Initializing Sequence (2)*/
	{ 0xF0, 2,	{0xA5, 0xA5} },
	{ 0xF1, 2,	{0xA5, 0xA5} },
	{ 0xFC, 2,	{0x5A, 0x5A} },
	{ REGFLAG_DELAY, 40, {} },

	/* Setting ending by predefined flag*/
	{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static struct LCM_setting_table lcm_sleep_in_setting[] = {
	{0x28, 0,	{0x00}},
	{ REGFLAG_DELAY, 15, {} },
	{0x10, 0,	{0x00}},
	{ REGFLAG_DELAY, 150, {} },

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

void push_table(struct LCM_setting_table *table,\
				unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;
		switch (cmd) {
			case REGFLAG_DELAY:
				MDELAY(table[i].count);
			break;

			case REGFLAG_END_OF_TABLE:
			break;

			default:
				dsi_set_cmdq_V2(cmd, table[i].count,\
					table[i].para_list, force_update);
		}
	}
}
/* ---------------------------------------------------------------------------*/
/* LCM Driver Implementations */
/* ---------------------------------------------------------------------------*/
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	printk("lcm_set_util_funcs \n");
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}
static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));
	params->type			= LCM_TYPE_DSI;
	params->width			= FRAME_WIDTH;
	params->height			= FRAME_HEIGHT;
	/* enable tearing-free */
	params->dbi.te_mode		= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity	= LCM_POLARITY_RISING;

	params->dsi.mode		= SYNC_PULSE_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM		= LCM_TWO_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order	= LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq	= LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding		= LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format		= LCM_DSI_FORMAT_RGB888;
	/* Highly depends on LCD driver capability. */
	/* Not support in MT6573 */
	params->dsi.DSI_WMEM_CONTI	= 0x3C;
	params->dsi.DSI_RMEM_CONTI	= 0x3E;
	params->dsi.packet_size		= 256;
	/* Video mode setting */
	params->dsi.intermediat_buffer_num 	= 2;
	params->dsi.PS				= LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active	= 2;
	params->dsi.vertical_backporch		= 6;
	params->dsi.vertical_frontporch		= 8;
	params->dsi.vertical_active_line	= FRAME_HEIGHT;
	params->dsi.horizontal_sync_active	= 28;
	params->dsi.horizontal_backporch	= 35;
	params->dsi.horizontal_frontporch	= 35;
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH;
	/* Bit rate calculation */
	params->dsi.PLL_CLOCK = 227;
	params->dsi.ssc_disable = 1;

	params->dsi.clk_lp_per_line_enable	= 0;


	params->dsi.WaitingTimeOnHS = 5;

	/*ESD configuration */
	params->dsi.customization_esd_check_enable	= 1;
	params->dsi.esd_check_enable			= 0;
	#if 0
	params->dsi.lcm_esd_check_table[0].cmd		= 0x0A;
	params->dsi.lcm_esd_check_table[0].count	= 1;
	params->dsi.lcm_esd_check_table[0].para_list[0]	= 0x9C;
	#endif
}	
static unsigned int lcm_compare_id(void)
{
	int array[4];
	char buffer[3];
	char id0 = 0;
	char id1 = 0;
	char id2 = 0;

	set_gpio_lcd_rst(0);
	MDELAY(5);
	set_gpio_lcd_rst(1);
	MDELAY(10);

	array[0] = 0x00033700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xDA, buffer, 1);
	array[0] = 0x00033700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xDB, buffer + 1, 1);
	array[0] = 0x00033700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0xDC, buffer + 2, 1);

	id0 = buffer[0];
	id1 = buffer[1];
	id2 = buffer[2];

	return 1;
}
struct brt_value{
	int level;/* Platform setting values */
	int tune_level;/* Chip Setting values*/
};

struct brt_value brt_table_ktd[] = {
	{ 20,  2 },
	{ 39,  3 }, 
	{ 48,  4 }, 
	{ 57,  5 }, 
	{ 66,  6 }, 
	{ 75,  7 }, 
	{ 84,  8 },  
	{ 93,  9 }, 
	{ 102,  10 }, 
	{ 111,  11 },   
	{ 120,  12 }, 
	{ 129,  13 }, 
	{ 138,  14 }, 
	{ 147,  15 },
	{ 155,  16 },
	{ 163,  17 },  
	{ 170,  18 },  
	{ 178,  19 }, 
	{ 186,  20 },
	{ 194,  21 }, 
	{ 202,  22 },
	{ 210,  23 },  
	{ 219,  24 }, 
	{ 228,  25 }, 
	{ 237,  26 },  
	{ 246,  27 }, 
	{ 255,  28 },
};
static DEFINE_SPINLOCK(bl_ctrl_lock);
#define MAX_BRT_STAGE_KTD (int)(sizeof(brt_table_ktd)/sizeof(struct brt_value))

/*This value should be same as bootloader*/
#define START_BRIGHTNESS 10

static int lcd_first_pwron = 1;
int CurrDimmingPulse;
int PrevDimmingPulse = START_BRIGHTNESS;

static void lcd_backlight_control(int num)
{
	int limit;
	unsigned long flags;

	limit = num;
	spin_lock_irqsave(&bl_ctrl_lock, flags);
	for( ; limit > 0; limit--) {
		UDELAY(10);
		set_gpio_blic_en(0);
		UDELAY(40); 
		set_gpio_blic_en(1);
	}
	spin_unlock_irqrestore(&bl_ctrl_lock, flags);
}

static void lcm_setbacklight(unsigned int level)
{
/* Temporary backlight control function for feature re-define for samsung's product.
     This code should be changed after normal porting for samsung's platform for lcd dirver. 
*/
	int user_intensity = level;

	int pulse = 0;
	int i;

	if(user_intensity > 0) {
		if(user_intensity < 10)
			CurrDimmingPulse = 2;
		else if (user_intensity == 255)
			CurrDimmingPulse = brt_table_ktd[MAX_BRT_STAGE_KTD - 1].tune_level;
		else {
			for(i = 0; i < MAX_BRT_STAGE_KTD; i++) {
				if(user_intensity <= brt_table_ktd[i].level ) {
					CurrDimmingPulse = brt_table_ktd[i].tune_level;
					break;
				}
			}
		}
	} else {
		CurrDimmingPulse = 0;
		set_gpio_blic_en(0);
		set_gpio_blic_ctl(0);
		MDELAY(10);
		PrevDimmingPulse = CurrDimmingPulse;
		return;
	}

	if (PrevDimmingPulse == CurrDimmingPulse) {
		PrevDimmingPulse = CurrDimmingPulse;
		return;
	} else {
		if (PrevDimmingPulse == 0) {
			set_gpio_blic_en(1);
			MDELAY(10);
			set_gpio_blic_ctl(1);
			MDELAY(5);
		}

		if( PrevDimmingPulse < CurrDimmingPulse)
			pulse = (32 + PrevDimmingPulse) - CurrDimmingPulse;
		else if(PrevDimmingPulse > CurrDimmingPulse)
			pulse = PrevDimmingPulse - CurrDimmingPulse;

		lcd_backlight_control(pulse);	
		PrevDimmingPulse = CurrDimmingPulse;

		return;
	}
	PrevDimmingPulse = CurrDimmingPulse;
}

static void lcm_init(void)
{
	/*LP11*/
	/*SET_RESET_PIN(0);
	MDELAY(5);
	set_gpio_lcd_enp(0);
	MDELAY(5);
	MDELAY(5);
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	MDELAY(30);*/


	push_table(lcm_initialization_setting,
		sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

}

static void lcm_init_power(void)
{
	SET_RESET_PIN(0);
	MDELAY(5);
	set_gpio_lcd_enp(0);
	MDELAY(5);
	set_gpio_lcd_enp(1);
	MDELAY(5);
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(30);
}

static void lcm_resume_power(void)
{
	SET_RESET_PIN(0);
	MDELAY(5);
	set_gpio_lcd_enp(0);
	MDELAY(5);
	set_gpio_lcd_enp(1);
	MDELAY(5);
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(30);
}
static void lcm_suspend(void)
{
	LCM_LOGI("%s, \n", __func__);
	push_table(lcm_sleep_in_setting, sizeof(lcm_sleep_in_setting) / \
		sizeof(struct LCM_setting_table), 1);

	set_gpio_blic_en(0);
	set_gpio_blic_ctl(0);
	MDELAY(3);
	set_gpio_lcd_bias(0);
	set_gpio_lcd_rst(0);
	lcd_first_pwron = 0;
}
static void lcm_resume(void)
{
	LCM_LOGI("%s, \n", __func__);

	if (lcd_first_pwron == 1) {
		printk("pre booting return \n");
		return;

	}
	lcm_init();
}

LCM_DRIVER s6d78a0_qhd_dsi_vdo_drv = {
	.name = "s6d78a0_qhd_dsi_vdo",
	.set_util_funcs = lcm_set_util_funcs,
	.compare_id = lcm_compare_id,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.set_backlight  = lcm_setbacklight,
#if defined(LCM_DSI_CMD_MODE)
	.update = lcm_update,
#endif
};
