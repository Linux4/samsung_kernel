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
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID (0x78)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))


#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH										(720)
#define FRAME_HEIGHT										(1600)

#define LCM_PHYSICAL_WIDTH									(68260)
#define LCM_PHYSICAL_HEIGHT									(151680)


#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY		0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

static struct LCM_DSI_MODE_SWITCH_CMD lcm_switch_mode_cmd;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern void ilitek_resume_by_ddi(void);

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{ 0xFF, 0x03, {0x78, 0x07, 0x00} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 60, {} }

};

//static struct LCM_setting_table init_setting_cmd[] = {
//	{ 0xFF, 0x03, {0x78, 0x07, 0x03} },
//};

static struct LCM_setting_table init_setting_vdo[] = {
//平台AVDD=5.8V,AVEE=-5.8V
//平台Vsync=4,Vbp=8,Vfp=32,Hsync=12,Hbp=116,Hbp=112,CLK=720Mbps
//GIP timing
{0xB9,0x03,{0x83, 0x11, 0x2A}}, 
{0xBA,0X01,{0X72}},
{0xCF,0x04,{0x00, 0x14, 0x00, 0xC0}},
{0xC4,0x04,{0x06, 0x04, 0x04, 0x81}},
{0xCC,0x01,{0x08}},
{0xC9,0x04,{0x04, 0x0A, 0x8C, 0x01}},
{0xC7,0x06,{0x00, 0x00, 0x04, 0xE0, 0x33, 0x00}},
{0xB1,0x08,{0x08, 0x29, 0x29, 0x80, 0x80, 0x4F, 0x4A, 0xAA}},
{0xB2,0x0F,{0x00, 0x02, 0x03, 0x62, 0x40, 0x00, 0x08, 0x30, 0x14, 0x11, 0x15, 0x00, 0x10, 0xA3, 0x87}},
{0xD2,0x02,{0x2C, 0x2C}},
{0xB4,0x1B,{0x0B, 0xE1, 0x0B, 0xE1, 0x0B, 0xE1, 0x0B, 0xE1, 0x0B, 0xE1, 0x0B, 0xE1, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x0C, 0x0F, 0x00, 0x3A, 0x08, 0x0D, 0x0F, 0x00, 0x3A}},
{0xB6,0x03,{0x7B, 0x7B, 0xE3}},
{0xD3,0x2A,{0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x05, 0x04, 0x04, 0x00, 0x1F, 0x0B, 0x0B, 0x0B, 0x0B, 0x32, 0x10, 0x0A, 0x00, 0x0A, 0x32, 0x16, 0x23, 0x06, 0x23, 0x32, 0x10, 0x06, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x06, 0x23}},
{0xBD,0x01,{0x01}},
{0xCB,0x03,{0x25, 0x11, 0x01}},
{0xBD,0x01,{0x00}},
{0xD5,0x30,{0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x37, 0x31, 0x36, 0x30, 0x35, 0x2F, 0x20, 0x20, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x18, 0x18, 0x31, 0x37, 0x30, 0x36, 0x2F, 0x35, 0x19, 0x19, 0x24, 0x24, 0x28, 0x28, 0x04, 0x04}},
{0xD6,0x30,{0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x37, 0x31, 0x36, 0x30, 0x35, 0x2F, 0x24, 0x24, 0x04, 0x04, 0x03, 0x03, 0x02, 0x02, 0x01, 0x01, 0x18, 0x18, 0x31, 0x37, 0x30, 0x36, 0x2F, 0x35, 0x19, 0x19, 0x20, 0x20, 0x28, 0x28, 0x00, 0x00}},
{0xD8,0x18,{0xAA, 0xAA, 0xFE, 0xAA, 0xAA, 0xEE, 0xAA, 0xAA, 0xAA, 0xAA, 0xBF, 0xEE, 0xAA, 0xAA, 0xFE, 0xAA, 0xAA, 0xEA, 0xAA, 0xAA, 0xAA, 0xAA, 0xBF, 0xEA}},
{0xBD,0x01,{0x01}},
{0xD8,0x18,{0xAA, 0xAA, 0xAA, 0xAA, 0xBF, 0xAA, 0xAA, 0xAA, 0xFE, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xBF, 0xAA, 0xAA, 0xAA, 0xFE, 0xAA, 0xAA, 0xAA}},
{0xBD,0x01,{0x02}},
{0xD8,0x0C,{0xAA, 0xAA, 0xAA, 0xAA, 0xBF, 0xAA, 0xAA, 0xAA, 0xFE, 0xAA, 0xAA, 0xAA}},
{0xBD,0x01,{0x03}},
{0xD8,0x18,{0xAA, 0xAA, 0xFE, 0xAA, 0xAA, 0xEE, 0xAA, 0xAA, 0xAA, 0xAA, 0xBF, 0xEE, 0xAA, 0xAA, 0xAA, 0xAA, 0xBF, 0xAA, 0xAA, 0xAA, 0xFE, 0xAA, 0xAA, 0xAA}},
{0xBD,0x01,{0x00}},
{0xE7,0x17,{0x0F, 0x0F, 0x1E, 0x75, 0x1E, 0x75, 0x00, 0x50, 0x02, 0x02, 0x00, 0x00, 0x02, 0x02, 0x02, 0x05, 0x14, 0x14, 0x32, 0xB9, 0x23, 0xB9, 0x08}},
{0xBD,0x01,{0x01}},
{0xE7,0x08,{0x02, 0x00, 0x64, 0x01, 0x6E, 0x0E, 0x4B, 0x0F}},
{0xBD,0x01,{0x02}},
{0xE7,0x1D,{0x00, 0x00, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00}},
{0xBD,0x01,{0x00}},
{0xE9,0x01,{0xC3}},
{0xCB,0x02,{0xD2, 0x28}},
{0xE9,0x01,{0x3F}},
{0xC1,0x01,{0x01}},
{0xBD,0x01,{0x01}},
{0xE9,0x01,{0xC8}},
{0xD3,0x01,{0x81}},
{0xE9,0x01,{0x3F}},
{0xBD,0x01,{0x00}},
{0xC1,0x02,{0x01, 0x00}},
{0xBD,0x01,{0x01}},
{0xC1,0x39,{0xF8, 0xF7, 0xF3, 0xF0, 0xEC, 0xE7, 0xE3, 0xDA, 0xD5, 0xD1, 0xCC, 0xC7, 0xC3, 0xBE, 0xBA, 0xB5, 0xB0, 0xAC, 0xA7, 0x9E, 0x95, 0x8D, 0x85, 0x7D, 0x75, 0x6D, 0x65, 0x5E, 0x57, 0x4F, 0x48, 0x42, 0x3B, 0x34, 0x2D, 0x26, 0x1F, 0x18, 0x11, 0x0A, 0x06, 0x05, 0x03, 0x01, 0x00, 0xAD, 0x30, 0x95, 0x65, 0x89, 0x91, 0xFE, 0x6E, 0x80, 0x29, 0xE3, 0x80}},
{0xBD,0x01,{0x02}},
{0xC1,0x39,{0xF8, 0xF7, 0xF3, 0xF0, 0xEC, 0xE7, 0xE3, 0xDA, 0xD5, 0xD1, 0xCC, 0xC7, 0xC3, 0xBE, 0xBA, 0xB5, 0xB0, 0xAC, 0xA7, 0x9E, 0x95, 0x8D, 0x85, 0x7D, 0x75, 0x6D, 0x65, 0x5E, 0x57, 0x4F, 0x48, 0x42, 0x3B, 0x34, 0x2D, 0x26, 0x1F, 0x18, 0x11, 0x0A, 0x06, 0x05, 0x03, 0x01, 0x00, 0xAD, 0x30, 0x95, 0x65, 0x89, 0x91, 0xFE, 0x6E, 0x80, 0x29, 0xE3, 0x80}},
{0xBD,0x01,{0x03}},
{0xC1,0x39,{0xF8, 0xF7, 0xF3, 0xF0, 0xEC, 0xE7, 0xE3, 0xDA, 0xD5, 0xD1, 0xCC, 0xC7, 0xC3, 0xBE, 0xBA, 0xB5, 0xB0, 0xAC, 0xA7, 0x9E, 0x95, 0x8D, 0x85, 0x7D, 0x75, 0x6D, 0x65, 0x5E, 0x57, 0x4F, 0x48, 0x42, 0x3B, 0x34, 0x2D, 0x26, 0x1F, 0x18, 0x11, 0x0A, 0x06, 0x05, 0x03, 0x01, 0x00, 0xAD, 0x30, 0x95, 0x65, 0x89, 0x91, 0xFE, 0x6E, 0x80, 0x29, 0xE3, 0x80}},
{0xBD,0x01,{0x00}},
{0xE3,0x19,{0x01, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}},
{0xBD,0x01,{0x01}},
{0xE3,0x39,{0xFD, 0xFB, 0xF6, 0xF0, 0xEC, 0xE7, 0xE3, 0xDA, 0xD5, 0xD1, 0xCC, 0xC7, 0xC3, 0xBE, 0xBA, 0xB5, 0xB0, 0xAC, 0xA7, 0x9E, 0x95, 0x8D, 0x85, 0x7D, 0x75, 0x6D, 0x65, 0x5E, 0x57, 0x4F, 0x48, 0x42, 0x3B, 0x34, 0x2D, 0x26, 0x1F, 0x18, 0x11, 0x0A, 0x06, 0x05, 0x03, 0x01, 0x00, 0x95, 0x30, 0x95, 0x65, 0x89, 0x91, 0xFE, 0x6E, 0x80, 0x29, 0xE3, 0x80}},
{0xBD,0x01,{0x02}},
{0xE3,0x39,{0xFD, 0xFB, 0xF6, 0xF0, 0xEC, 0xE7, 0xE3, 0xDA, 0xD5, 0xD1, 0xCC, 0xC7, 0xC3, 0xBE, 0xBA, 0xB5, 0xB0, 0xAC, 0xA7, 0x9E, 0x95, 0x8D, 0x85, 0x7D, 0x75, 0x6D, 0x65, 0x5E, 0x57, 0x4F, 0x48, 0x42, 0x3B, 0x34, 0x2D, 0x26, 0x1F, 0x18, 0x11, 0x0A, 0x06, 0x05, 0x03, 0x01, 0x00, 0x95, 0x30, 0x95, 0x65, 0x89, 0x91, 0xFE, 0x6E, 0x80, 0x29, 0xE3, 0x80}},
{0xBD,0x01,{0x03}},
{0xE3,0x39,{0xFD, 0xFB, 0xF6, 0xF0, 0xEC, 0xE7, 0xE3, 0xDA, 0xD5, 0xD1, 0xCC, 0xC7, 0xC3, 0xBE, 0xBA, 0xB5, 0xB0, 0xAC, 0xA7, 0x9E, 0x95, 0x8D, 0x85, 0x7D, 0x75, 0x6D, 0x65, 0x5E, 0x57, 0x4F, 0x48, 0x42, 0x3B, 0x34, 0x2D, 0x26, 0x1F, 0x18, 0x11, 0x0A, 0x06, 0x05, 0x03, 0x01, 0x00, 0x95, 0x30, 0x95, 0x65, 0x89, 0x91, 0xFE, 0x6E, 0x80, 0x29, 0xE3, 0x80}},
{0xBD,0x01,{0x00}},
{0x35,0x01,{0x00}},
{0x51,0x02,{0x00,0x00}},
{0x53,0x01,{0x2C}},
{0x55,0x01,{0x01}},
{0x11,0x01,{0x11}},
{REGFLAG_DELAY,85,{}},
{0x29,0x01,{0x29}},
{REGFLAG_DELAY,20,{}},

};

static struct LCM_setting_table bl_level[] = {
	{ 0xFF, 0x03, {0x78, 0x07, 0x00} },
	{ 0x51, 0x02, {0x0F, 0xFF} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};
//+add by songbinbo.wt for himax tp reset sequence 20191122
extern void himax_lcd_resume_func(void);
//-add by songbinbo.wt for himax tp reset sequence 20191122
static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}


static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	//params->physical_width_um = LCM_PHYSICAL_WIDTH;
	//params->physical_height_um = LCM_PHYSICAL_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	//lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	//params->dsi.switch_mode = CMD_MODE;
	//lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
	//params->dsi.mode   = SYNC_PULSE_VDO_MODE;	//SYNC_EVENT_VDO_MODE
#endif
	//LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode);
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_THREE_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

params->dsi.vertical_sync_active = 5;
	params->dsi.vertical_backporch = 5;
	params->dsi.vertical_frontporch = 50;
	//params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 5;
	params->dsi.horizontal_backporch = 36;
	params->dsi.horizontal_frontporch = 30;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
	/*params->dsi.ssc_disable = 1;*/
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 270;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 336;	/* this value must be in MTK suggested table */
#endif
	//params->dsi.PLL_CK_CMD = 220;
	//params->dsi.PLL_CK_VDO = 255;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9D;

	params->backlight_cust_count=1;
	params->backlight_cust[0].max_brightness = 255;
	params->backlight_cust[0].min_brightness = 10;
	params->backlight_cust[0].max_bl_lvl = 4095;
	params->backlight_cust[0].min_bl_lvl = 50;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 0;
	params->corner_pattern_width = 720;
	params->corner_pattern_height = 32;
#endif
}

static void lcm_init_power(void)
{
	/*pr_debug("lcm_init_power\n");*/
	display_bias_enable();
	MDELAY(15);
}

extern uint8_t himax_gesture_status ;
static void lcm_suspend_power(void)
{
	/*pr_debug("lcm_suspend_power\n");*/
	SET_RESET_PIN(0);
	MDELAY(2);
	if(himax_gesture_status == 0)
	{
		display_bias_disable();
		pr_debug("[LCM] lcm suspend power down.\n");
	}
}

static void lcm_resume_power(void)
{
	/*pr_debug("lcm_resume_power\n");*/
	display_bias_enable();
}

#ifdef BUILD_LK
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
   mt_set_gpio_mode(GPIO, GPIO_MODE_00);
   mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
   mt_set_gpio_out(GPIO, (output>0)? GPIO_OUT_ONE: GPIO_OUT_ZERO);
}
#endif

static void lcm_init(void)
{
	//pr_debug("[LCM] lcm_init\n");
	/*lcm_reset_pin(1);
	MDELAY(10);*/
	MDELAY(10);
	lcm_reset_pin(0);
	MDELAY(1);
	lcm_reset_pin(1);
	MDELAY(15);
	//+add by songbinbo.wt for himax tp reset sequence 20191122
	//pr_debug("[LCM before lcd_load_ili_fw.\n]");
	 himax_lcd_resume_func();
	 //pr_debug("[LCM after himax_lcd_resume_func.\n]");
	//-add by songbinbo.wt for himax tp reset sequence 20191122
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	//pr_debug("[LCM] ili7807g--tcl--sm5109----init success ----\n");

}

static void lcm_suspend(void)
{
	//pr_debug("[LCM] lcm_suspend\n");

	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);

}

static void lcm_resume(void)
{
	//pr_debug("[LCM] lcm_resume\n");

	lcm_init();
}

#if 1
static unsigned int lcm_compare_id(void)
{

	return 1;
}
#endif

/* return TRUE: need recovery */
/* return FALSE: No need recovery */
static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9c) {
		LCM_LOGI("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
		return TRUE;
	}
	LCM_LOGI("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
	return FALSE;
#else
	return FALSE;
#endif

}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned int x1 = FRAME_WIDTH * 3 / 4;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);

	unsigned int data_array[3];
	unsigned char read_buf[4];

	LCM_LOGI("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00043700;	/* read id return two byte,version and id */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x2A, read_buf, 4);

	if ((read_buf[0] == x0_MSB) && (read_buf[1] == x0_LSB)
	    && (read_buf[2] == x1_MSB) && (read_buf[3] == x1_LSB))
		ret = 1;
	else
		ret = 0;

	x0 = 0;
	x1 = FRAME_WIDTH - 1;

	x0_MSB = ((x0 >> 8) & 0xFF);
	x0_LSB = (x0 & 0xFF);
	x1_MSB = ((x1 >> 8) & 0xFF);
	x1_LSB = (x1 & 0xFF);

	data_array[0] = 0x0005390A;	/* HS packet */
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	return ret;
#else
	return 0;
#endif
}
static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	// set 12bit
	bl_level[1].para_list[0] = (level&0xF00)>>8;
	bl_level[1].para_list[1] = (level&0xFF);
	pr_err("[tcl_ili7807g]para_list[0]=%x,para_list[1]=%x\n",bl_level[1].para_list[0],bl_level[1].para_list[1]);

	//bl_level[1].para_list[0] = 0x0F;
	//bl_level[1].para_list[1] = 0xFF;

	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void *lcm_switch_mode(int mode)
{
#ifndef BUILD_LK
/* customization: 1. V2C config 2 values, C2V config 1 value; 2. config mode control register */
	if (mode == 0) {	/* V2C */
		lcm_switch_mode_cmd.mode = CMD_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;	/* mode control addr */
		lcm_switch_mode_cmd.val[0] = 0x13;	/* enabel GRAM firstly, ensure writing one frame to GRAM */
		lcm_switch_mode_cmd.val[1] = 0x10;	/* disable video mode secondly */
	} else {		/* C2V */
		lcm_switch_mode_cmd.mode = SYNC_PULSE_VDO_MODE;
		lcm_switch_mode_cmd.addr = 0xBB;
		lcm_switch_mode_cmd.val[0] = 0x03;	/* disable GRAM and enable video mode */
	}
	return (void *)(&lcm_switch_mode_cmd);
#else
	return NULL;
#endif
}

#if (LCM_DSI_CMD_MODE)

/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty. In this case we need to use minimu roi */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		LCM_LOGD("%s calc error,assign full roi:y=%d,h=%d\n", __func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	/*LCM_LOGD("lcm_validate_roi (%d,%d,%d,%d) to (%d,%d,%d,%d)\n",*/
	/*	*x, *y, *width, *height, x1, y1, w, h);*/

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}
#endif
/*bug 350122 - add white point reading function in lk , houbenzhong.wt, 20180411, begin*/
/*struct boe_panel_white_point{
	unsigned short int white_x;
	unsigned short int white_y;
};

//static struct boe_panel_white_point white_point;
*/
/*
static struct LCM_setting_table set_cabc[] = {
	{ 0xFF, 0x03, {0x78, 0x07, 0x00} },
	{ 0x55, 0x01, {0x02} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int cabc_status = 0;
static void lcm_set_cabc_cmdq(void *handle, unsigned int enable)
{
	pr_err("[truly_ili7807g] cabc set to %d\n", enable);
	if (enable==1){
		set_cabc[1].para_list[0]=0x02;
		push_table(handle, set_cabc, sizeof(set_cabc) / sizeof(struct LCM_setting_table), 1);
		pr_err("[truly_ili7807g] cabc set enable, set_cabc[0x%2x]=%x \n",set_cabc[1].cmd,set_cabc[1].para_list[0]);
	}else if (enable == 0){
		set_cabc[1].para_list[0]=0x00;
		push_table(handle, set_cabc, sizeof(set_cabc) / sizeof(struct LCM_setting_table), 1);
		pr_err("[truly_ili7807g] cabc set disable, set_cabc[0x%2x]=%x \n",set_cabc[1].cmd,set_cabc[1].para_list[0]);
	}
	cabc_status = enable;
}

static void lcm_get_cabc_status(int *status)
{
	pr_err("[truly_ili7807g] cabc get to %d\n", cabc_status);
	*status = cabc_status;
}
 */
//#define WHITE_POINT_BASE_X 167
//#define WHITE_POINT_BASE_Y 192
#if (LCM_DSI_CMD_MODE)
struct LCM_DRIVER hx83104a_hd_plus_dsi_incell_inx_row_lcm_drv = {
	.name = "hx83104a_hd_plus_dsi_incell_inx_row_lcm_drv",
#else

struct LCM_DRIVER hx83104a_hd_plus_dsi_incell_inx_row_lcm_drv = {
	.name = "hx83104a_hd_plus_dsi_incell_inx_row",
#endif
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
/*bug 350122 - add white point reading function in lk , houbenzhong.wt, 20180411, begin*/
//	.get_white_point = lcm_white_x_y,
/*bug 350122 - add white point reading function in lk , houbenzhong.wt, 20180411, end*/
	.ata_check = lcm_ata_check,
	.switch_mode = lcm_switch_mode,
	//.set_cabc_cmdq = lcm_set_cabc_cmdq,
	//.get_cabc_status = lcm_get_cabc_status,
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif

};
