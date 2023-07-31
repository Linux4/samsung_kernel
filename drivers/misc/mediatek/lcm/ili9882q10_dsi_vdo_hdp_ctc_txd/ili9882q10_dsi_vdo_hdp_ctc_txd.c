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
#include "lcm_cust_common.h"

#include "panel_notifier.h"  //tp suspend before lcd suspend

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

#define LCM_DSI_CMD_MODE									0
#define FRAME_WIDTH										(720)
#define FRAME_HEIGHT									(1600)

#define LCM_PHYSICAL_WIDTH								(67932)
#define LCM_PHYSICAL_HEIGHT								(150960)

#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW	0xFFFE
#define REGFLAG_RESET_HIGH	0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	// SLEEP IN + DISPLAY OFF
	{0x28, 0,{0x00}},
	{REGFLAG_DELAY, 20,{}},
	{0x10, 0,{0x00}},
	{REGFLAG_DELAY, 100,{}},
};

static struct LCM_setting_table init_setting_vdo[] = {
#if 1
{0xFF,3,{0x98,0x82,0x01}},
{0x00,1,{0x85}},
{0x01,1,{0x32}},
{0x02,1,{0x00}},
{0x03,1,{0x00}},
{0x04,1,{0xCC}},
{0x05,1,{0x35}},
{0x06,1,{0x00}},
{0x07,1,{0x00}},
{0x08,1,{0x85}},
{0x09,1,{0x02}},
{0x0A,1,{0x72}},
{0x0B,1,{0x00}},
{0x28,1,{0x86}},
{0x29,1,{0x4E}},
{0xEE,1,{0x17}},
{0x2A,1,{0x85}},
{0x2B,1,{0x4D}},
{0xF0,1,{0x00}},
{0xEF,1,{0x45}},
{0x31,1,{0x2A}},
{0x32,1,{0x07}},
{0x33,1,{0x07}},
{0x34,1,{0x07}},
{0x35,1,{0x00}},
{0x36,1,{0x07}},
{0x37,1,{0x01}},
{0x38,1,{0x22}},
{0x39,1,{0x23}},
{0x3A,1,{0x07}},
{0x3B,1,{0x10}},
{0x3C,1,{0x12}},
{0x3D,1,{0x14}},
{0x3E,1,{0x16}},
{0x3F,1,{0x07}},
{0x40,1,{0x0C}},
{0x41,1,{0x0E}},
{0x42,1,{0x07}},
{0x43,1,{0x2A}},
{0x44,1,{0x2A}},
{0x45,1,{0x2A}},
{0x46,1,{0x07}},
{0x47,1,{0x2A}},
{0x48,1,{0x07}},
{0x49,1,{0x07}},
{0x4A,1,{0x07}},
{0x4B,1,{0x00}},
{0x4C,1,{0x07}},
{0x4D,1,{0x01}},
{0x4E,1,{0x22}},
{0x4F,1,{0x23}},
{0x50,1,{0x07}},
{0x51,1,{0x11}},
{0x52,1,{0x13}},
{0x53,1,{0x15}},
{0x54,1,{0x17}},
{0x55,1,{0x07}},
{0x56,1,{0x0D}},
{0x57,1,{0x0F}},
{0x58,1,{0x07}},
{0x59,1,{0x2A}},
{0x5A,1,{0x2A}},
{0x5B,1,{0x2A}},
{0x5C,1,{0x07}},
{0x61,1,{0x2A}},
{0x62,1,{0x07}},
{0x63,1,{0x07}},
{0x64,1,{0x07}},
{0x65,1,{0x00}},
{0x66,1,{0x07}},
{0x67,1,{0x01}},
{0x68,1,{0x22}},
{0x69,1,{0x23}},
{0x6A,1,{0x07}},
{0x6B,1,{0x17}},
{0x6C,1,{0x15}},
{0x6D,1,{0x13}},
{0x6E,1,{0x11}},
{0x6F,1,{0x07}},
{0x70,1,{0x0F}},
{0x71,1,{0x0D}},
{0x72,1,{0x07}},
{0x73,1,{0x2A}},
{0x74,1,{0x2A}},
{0x75,1,{0x2A}},
{0x76,1,{0x07}},
{0x77,1,{0x2A}},
{0x78,1,{0x07}},
{0x79,1,{0x07}},
{0x7A,1,{0x07}},
{0x7B,1,{0x00}},
{0x7C,1,{0x07}},
{0x7D,1,{0x01}},
{0x7E,1,{0x22}},
{0x7F,1,{0x23}},
{0x80,1,{0x07}},
{0x81,1,{0x16}},
{0x82,1,{0x14}},
{0x83,1,{0x12}},
{0x84,1,{0x10}},
{0x85,1,{0x07}},
{0x86,1,{0x0E}},
{0x87,1,{0x0C}},
{0x88,1,{0x07}},
{0x89,1,{0x2A}},
{0x8A,1,{0x2A}},
{0x8B,1,{0x2A}},
{0x8C,1,{0x07}},
{0xA0,1,{0x01}},
{0xA1,1,{0x00}},
{0xA2,1,{0x00}},
{0xA3,1,{0x00}},
{0xA4,1,{0x00}},
{0xA7,1,{0x00}},
{0xA8,1,{0x00}},
{0xA9,1,{0x00}},
{0xAA,1,{0x00}},
{0xB0,1,{0x34}},
{0xB2,1,{0x04}},
{0xB9,1,{0x10}},
{0xBA,1,{0x02}},
{0xC1,1,{0x10}},
{0xD0,1,{0x01}},
{0xD1,1,{0x11}},
{0xD2,1,{0x40}},
{0xD5,1,{0x56}},
{0xD6,1,{0x91}},
{0xD8,1,{0x05}},
{0xD9,1,{0x11}},
{0xDA,1,{0x54}},
{0xDc,1,{0x40}},
{0xE2,1,{0x45}},
{0xE6,1,{0x33}},
{0xEA,1,{0x0F}},
{0xE7,1,{0x54}},
{0xE0,1,{0x7E}},
{0xFF,3,{0x98,0x82,0x02}},
{0xF1,1,{0x1C}},
{0x4B,1,{0x5A}},
{0x50,1,{0xCA}},
{0x51,1,{0x00}},
{0x06,1,{0x8C}},
{0x0B,1,{0xA0}},
{0x0C,1,{0x00}},
{0x0D,1,{0x22}},
{0x0E,1,{0xFA}},
{0x4E,1,{0x11}},
{0x4D,1,{0xCE}},
{0xF2,1,{0x4A}},
{0x40,1,{0x4A}},
{0xFF,3,{0x98,0x82,0x03}},
{0x83,1,{0x30}},
{0x84,1,{0x00}},
{0xFF,3,{0x98,0x82,0x05}},
{0x03,1,{0x01}},
{0x04,1,{0x1A}},
{0x58,1,{0x61}},
{0x63,1,{0x88}},
{0x64,1,{0x88}},
{0x68,1,{0xB5}},
{0x69,1,{0xBB}},
{0x6A,1,{0x8D}},
{0x6B,1,{0x7F}},
{0x85,1,{0x77}},
{0x47,1,{0x0A}},
{0xC8,1,{0xF1}},
{0xC9,1,{0xF1}},
{0xCA,1,{0xB5}},
{0xCB,1,{0xB5}},
{0xD0,1,{0xF7}},
{0xD1,1,{0xF7}},
{0xD2,1,{0xBB}},
{0xD3,1,{0xBB}},
{0xFF,3,{0x98,0x82,0x06}},
{0xD9,1,{0x1F}},
{0xC0,1,{0x40}},
{0xC1,1,{0x16}},
{0x80,1,{0x08}},
{0x06,1,{0xA4}},
{0xFF,3,{0x98,0x82,0x07}},
{0xC0,1,{0x01}},
{0xCB,1,{0xC0}},
{0xCC,1,{0xBB}},
{0xCD,1,{0x83}},
{0xCE,1,{0x83}},
{0xFF,3,{0x98,0x82,0x08}},
{0xE0,27,{0x00,0x00,0x37,0x63,0x9E,0x50,0xD2,0xFB,0x2D,0x55,0x95,0x95,0xCA,0xF9,0x28,0xAA,0x58,0x92,0xB6,0xE3,0xFF,0x09,0x39,0x73,0xA0,0x03,0xA3}},
{0xE1,27,{0x00,0x00,0x37,0x63,0x9E,0x50,0xD2,0xFB,0x2D,0x55,0x95,0x95,0xCA,0xF9,0x28,0xAA,0x58,0x92,0xB6,0xE3,0xFF,0x09,0x39,0x73,0xA0,0x03,0xE6}},
{0xFF,3,{0x98,0x82,0x0B}},
{0x9A,1,{0x44}},
{0x9B,1,{0x6C}},
{0x9C,1,{0x03}},
{0x9D,1,{0x03}},
{0x9E,1,{0x6E}},
{0x9F,1,{0x6E}},
{0xAB,1,{0xE0}},
{0xAC,1,{0x7F}},
{0xAD,1,{0x3F}},
{0xFF,3,{0x98,0x82,0x0E}},
{0x11,1,{0x10}},
{0x13,1,{0x05}},
{0x00,1,{0xA0}},
{0xFF,3,{0x98,0x82,0x00}},
{0x51,2,{0x00,0x00}},
{0x68,2,{0x04,0x01}},
{0x53,1,{0x2c}},
{0x11,1,{0x00}},
{REGFLAG_DELAY, 60, {}},
{0x29,1,{0x00}},
{REGFLAG_DELAY, 50, {}},
{0x35,1,{0x00}},
#endif
};

static struct LCM_setting_table bl_level[] = {
	{ 0x51, 0x02, {0x0F,0xFF} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};

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
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
#endif
	params->dsi.switch_mode_enable = 0;

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 32;
	params->dsi.vertical_frontporch = 250;
	//params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 18;
	params->dsi.horizontal_backporch = 100;
	params->dsi.horizontal_frontporch = 100;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 339;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 339;	/* this value must be in MTK suggested table */
#endif
    params->dsi.data_rate = 677;
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
	params->dsi.lcm_esd_check_table[0].cmd = 0x09;
	params->dsi.lcm_esd_check_table[0].count = 4;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x80;
	params->dsi.lcm_esd_check_table[0].para_list[1] = 0x03;
	params->dsi.lcm_esd_check_table[0].para_list[2] = 0x06;
	params->dsi.lcm_esd_check_table[0].para_list[3] = 0x00;

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 0;
	params->corner_pattern_width = 720;
	params->corner_pattern_height = 32;
#endif
}

static void lcm_init_power(void)
{
	pr_debug("[LCM]ili9882q10_txd lcm_init_power !\n");
    lcm_reset_pin(LOW);
    MDELAY(2);
//    lcm_vio_enable(HIGH);
//    MDELAY(10);
    lcm_bais_enp_enable(HIGH);
    MDELAY(5);
    lcm_bais_enn_enable(HIGH);
    MDELAY(2);
    lcm_set_power_reg(AVDD_REG,0x14,(0x1F << 0));
    lcm_set_power_reg(AVEE_REG,0x14,(0x1F << 0));
    lcm_set_power_reg(ENABLE_REG,((1<<0) | (1<<1)),((1<<0) | (1<<1)));
    MDELAY(2);
}

extern bool  gestrue_status;
static void lcm_suspend_power(void)
{
	pr_debug("[LCM] ili9882q10_txd lcm_suspend_power !\n");
	if(gestrue_status == 1){
		printk("[ILITEK]lcm_suspend_power gesture_status = 1\n ");
	} else {
		printk("[ILITEK]lcm_suspend_power gestrue_status = 0\n ");
		//lcm_reset_pin(LOW);
		//MDELAY(2);
		lcm_bais_enn_enable(LOW);
		MDELAY(2);
		lcm_bais_enp_enable(LOW);
		MDELAY(2);
		//lcm_vio_enable(LOW);
		//MDELAY(10);
	}
}

static void lcm_resume_power(void)
{
	pr_debug("[LCM]ili9882q10_txd lcm_resume_power !\n");
	lcm_init_power();
}

extern void ili_resume_by_ddi(void);
static void lcm_init(void)
{
	pr_debug("[LCM] ili9882q10_txd lcm_init\n");
	//lcm_reset_pin(HIGH);
	//MDELAY(10);
	//lcm_reset_pin(LOW);
	MDELAY(2);
	lcm_reset_pin(HIGH);
	MDELAY(12);
	ili_resume_by_ddi();
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	pr_debug("[LCM]ili9882q10_txd----init success ----\n");
}

static void lcm_suspend(void)
{
	pr_debug("[LCM] lcm_suspend\n");

	//add for TP suspend
	panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	printk("[ILITEK]tpd_ilitek_notifier_callback in lcm_suspend\n ");
	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
}


static void lcm_resume(void)
{
	pr_debug("[LCM] lcm_resume\n");
	lcm_init();
	//add for TP resume
	panel_notifier_call_chain(PANEL_PREPARE, NULL);
	printk("[ILITEK]tpd_ilitek_notifier_callback in lcm_resume\n ");
}

static unsigned int lcm_compare_id(void)
{
	return 1;
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

	pr_debug("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
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
static void lcm_setbacklight_cmdq(void* handle,unsigned int level)
{
	unsigned int bl_lvl;
	bl_lvl = wingtech_bright_to_bl(level,255,10,4095,50);
	pr_debug("%s,ili9882q10_txd backlight: level = %d,bl_lvl=%d\n", __func__, level,bl_lvl);
	// set 12bit
	bl_level[0].para_list[0] = (bl_lvl&0xF00)>>8;
	bl_level[0].para_list[1] = (bl_lvl&0xFF);
	pr_debug("ili9882q10_txd backlight: para_list[0]=%x,para_list[1]=%x\n",bl_level[0].para_list[0],bl_level[0].para_list[1]);
	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER ili9882q10_dsi_vdo_hdp_ctc_txd_drv = {
	.name = "ili9882q10_dsi_vdo_hdp_ctc_txd",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
};
