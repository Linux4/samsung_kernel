
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

extern char *saved_command_line;

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
//+S96818AA1-1936,liyuhong1.wt,modify,2023/06/14,nt36528 modify the physical size of the screen
#define LCM_PHYSICAL_WIDTH								(70380)
#define LCM_PHYSICAL_HEIGHT								(156240)
//-S96818AA1-1936,liyuhong1.wt,modify,2023/06/14,nt36528 modify the physical size of the screen
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE	0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[256];
};
static struct LCM_setting_table lcm_suspend_setting[] = {
	{0xFF,3,{0x55,0xAA,0x66}},

	{0xFF,1,{0x10}},
	{0x28,1,{0x00}},
	{REGFLAG_DELAY, 50, {}},
	// Sleep Mode On
	{0x10,1,{0x00}},
	{REGFLAG_DELAY, 120, {}}, //120ms
	{0x4F,1,{0x01}},//DSTB
};

static struct LCM_setting_table lcm_aot_suspend_setting[] = {
	{0xFF,3,{0x55,0xAA,0x66}},

	{0xFF,1,{0x10}},
	{0x28,1,{0x00}},
	{REGFLAG_DELAY, 50, {}},

	// Sleep Mode On
	{0x10,1,{0x00}},
	{REGFLAG_DELAY, 120, {}}, //120ms

	//{0xFF,1,{0x26}},
	//{0x1F,1,{0x01}},
};


static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF,3,{0x55,0xAA,0x66}},
	{0xFF,1,{0x10}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0x20}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0x21}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0x22}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0x23}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0x24}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0x27}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0x26}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0x28}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0xB3}},
	{0xFB,1,{0x00}},
	{0xFF,1,{0x20}},
	{0x2D,1,{0xAA}},
	{0xA3,1,{0x12}},
	{0xA7,1,{0x12}},
	{0xFF,1,{0xB3}},
	{0x4E,1,{0x46}},
	{0xFF,1,{0xB3}},
	{0x3F,1,{0x37}},
	{0x50,1,{0x10}},
	{0xFF,1,{0x20}},
	{0x29,1,{0xe2}},
	{0x20,1,{0x84}},
	{0x21,1,{0x00}},
	{0x22,1,{0x80}},
	{0x2E,1,{0x0F}},
	{0xFF,1,{0xA3}},
	{0x58,1,{0xAA}},
	{0x5C,1,{0x00}},
	{0x62,1,{0x80}},
	{0xFF,1,{0x26}},
	{0x43,1,{0x00}},
	{0xFF,1,{0x22}},
	{0x0E,1,{0x33}},
	{0x0C,1,{0x01}},
	{0x0B,1,{0x2A}},
	{0xFF,1,{0x28}},
	{0x7D,1,{0x1F}},
	{0x7b,1,{0x40}},
	{0xFF,1,{0x22}},
	{0xE4,1,{0x00}},
	{0x01,1,{0x06}},
	{0x02,1,{0x40}},
	{0x03,1,{0x00}},
	{0xFF,1,{0x20}},
	{0xC3,1,{0x00}},
	{0xC4,1,{0xA3}},
	{0xC5,1,{0x00}},
	{0xC6,1,{0xA3}},
	{0xB3,1,{0x00}},
	{0xB4,1,{0x20}},
	{0xB5,1,{0x00}},
	{0xB6,1,{0xA0}},
	{0xD3,1,{0x24}},
	{0xFF,1,{0x22}},
	{0x25,1,{0x15}},
	{0x26,1,{0x00}},
	{0x2E,1,{0x6f}},
	{0x2F,1,{0x00}},
	{0x36,1,{0x16}},
	{0x37,1,{0x00}},
	{0x3F,1,{0x6f}},
	{0x40,1,{0x00}},
	{0xFF,1,{0x22}},
	{0xDC,1,{0x00}},
	{0xE0,1,{0x01}},
	{0xFF,1,{0x28}},
	{0x01,1,{0x23}},
	{0x02,1,{0x23}},
	{0x03,1,{0x19}},
	{0x04,1,{0x19}},
	{0x05,1,{0x18}},
	{0x06,1,{0x18}},
	{0x07,1,{0x1A}},
	{0x08,1,{0x23}},
	{0x09,1,{0x23}},
	{0x0A,1,{0x23}},
	{0x0B,1,{0x1A}},
	{0x0C,1,{0x09}},
	{0x0D,1,{0x0B}},
	{0x0E,1,{0x0D}},
	{0x0F,1,{0x0F}},
	{0x10,1,{0x11}},
	{0x11,1,{0x13}},
	{0x12,1,{0x05}},
	{0x13,1,{0x03}},
	{0x14,1,{0x01}},
	{0x15,1,{0x25}},
	{0x16,1,{0x25}},
	{0x17,1,{0x23}},
	{0x18,1,{0x23}},
	{0x19,1,{0x19}},
	{0x1A,1,{0x19}},
	{0x1B,1,{0x18}},
	{0x1C,1,{0x18}},
	{0x1D,1,{0x1A}},
	{0x1E,1,{0x23}},
	{0x1F,1,{0x23}},
	{0x20,1,{0x23}},
	{0x21,1,{0x1A}},
	{0x22,1,{0x08}},
	{0x23,1,{0x0A}},
	{0x24,1,{0x0C}},
	{0x25,1,{0x0E}},
	{0x26,1,{0x10}},
	{0x27,1,{0x12}},
	{0x28,1,{0x04}},
	{0x29,1,{0x02}},
	{0x2A,1,{0x00}},
	{0x2B,1,{0x25}},
	{0x2D,1,{0x25}},
	{0x30,1,{0x03}},
	{0x31,1,{0x00}},
	{0x32,1,{0x03}},
	{0x33,1,{0x00}},
	{0x34,1,{0x03}},
	{0x35,1,{0x00}},
	{0x36,1,{0x0C}},
	{0x38,1,{0x01}},
	{0x39,1,{0x00}},
	{0x2F,1,{0x0A}},
	{0xFF,1,{0x21}},
	{0x7E,1,{0x07}},
	{0x7F,1,{0x26}},
	{0x8B,1,{0x26}},
	{0x97,1,{0x26}},
	{0x80,1,{0x0C}},
	{0x8C,1,{0x08}},
	{0x98,1,{0x04}},
	{0x81,1,{0x04}},
	{0x8D,1,{0x08}},
	{0x99,1,{0x0C}},
	{0xAF,1,{0x40}},
	{0xB0,1,{0x40}},
	{0xB1,1,{0x40}},
	{0x83,1,{0x03}},
	{0x8F,1,{0x03}},
	{0x9B,1,{0x03}},
	{0x84,1,{0x86}},
	{0x90,1,{0x86}},
	{0x9C,1,{0x86}},
	{0x85,1,{0x86}},
	{0x91,1,{0x86}},
	{0x9D,1,{0x86}},
	{0x87,1,{0x02}},
	{0x93,1,{0x06}},
	{0x9F,1,{0x0A}},
	{0x82,1,{0xB0}},
	{0x8E,1,{0xB0}},
	{0x9A,1,{0xB0}},
	{0x2B,1,{0x00}},
	{0x2E,1,{0x00}},
	{0x88,1,{0xB7}},
	{0x89,1,{0x20}},
	{0x8A,1,{0x23}},
	{0x94,1,{0xB7}},
	{0x95,1,{0x20}},
	{0x96,1,{0x23}},
	{0xA0,1,{0xB7}},
	{0xA1,1,{0x20}},
	{0xA2,1,{0x23}},
	{0xAC,1,{0xB7}},
	{0xAD,1,{0x20}},
	{0xAE,1,{0x23}},
	{0x45,1,{0x3F}},
	{0x46,1,{0xB6}},
	{0x4C,1,{0xB6}},
	{0x52,1,{0xB6}},
	{0x58,1,{0xB6}},
	{0x5E,1,{0xB6}},
	{0x64,1,{0xB6}},
	{0x47,1,{0x0F}},
	{0x4D,1,{0x0E}},
	{0x53,1,{0x0D}},
	{0x59,1,{0x0C}},
	{0x5F,1,{0x0B}},
	{0x65,1,{0x0A}},
	{0x48,1,{0x0A}},
	{0x4E,1,{0x0B}},
	{0x54,1,{0x0C}},
	{0x5A,1,{0x0D}},
	{0x60,1,{0x0E}},
	{0x66,1,{0x0F}},
	{0x76,1,{0x40}},
	{0x77,1,{0x40}},
	{0x78,1,{0x40}},
	{0x79,1,{0x40}},
	{0x7A,1,{0x40}},
	{0x7B,1,{0x40}},
	{0x49,1,{0x86}},
	{0x4F,1,{0x86}},
	{0x55,1,{0x86}},
	{0x5B,1,{0x86}},
	{0x61,1,{0x86}},
	{0x67,1,{0x86}},
	{0x4A,1,{0x86}},
	{0x50,1,{0x86}},
	{0x56,1,{0x86}},
	{0x5C,1,{0x86}},
	{0x62,1,{0x86}},
	{0x68,1,{0x86}},
	{0xBE,1,{0x03}},
	{0xC0,1,{0x40}},
	{0xC1,1,{0x42}},
	{0xBF,1,{0xBB}},
	{0xC2,1,{0x8B}},
	{0xC6,1,{0x53}},
	{0x29,1,{0x00}},
	{0xFF,1,{0x22}},
	{0x05,1,{0x00}},
	{0x08,1,{0x22}},

	{0xFF,1,{0x28}},
	{0x3D,1,{0x63}},
	{0x3E,1,{0x63}},
	{0x3F,1,{0x60}},
	{0x40,1,{0x60}},
	{0x45,1,{0x50}},
	{0x46,1,{0x50}},
	{0x47,1,{0x3E}},
	{0x48,1,{0x3E}},
	{0x4D,1,{0xA2}},
	{0x50,1,{0xAA}},
	{0x52,1,{0x71}},
	{0x53,1,{0x22}},
	{0x56,1,{0x12}},
	{0x57,1,{0x20}},
	{0x5A,1,{0x9C}},
	{0x5B,1,{0x9C}},
	{0x62,1,{0xc3}},//VCOM
	{0x63,1,{0xc3}},
	{0xFF,1,{0x20}},
	{0x7E,1,{0x03}},
	{0x7F,1,{0x00}},
	{0x80,1,{0x64}},
	{0x81,1,{0x00}},
	{0x82,1,{0x00}},
	{0x83,1,{0x64}},
	{0x84,1,{0x64}},
	{0x85,1,{0x40}},
	{0x86,1,{0x39}},
	{0x87,1,{0x40}},
	{0x88,1,{0x39}},
	{0x8A,1,{0x0A}},
	{0x8B,1,{0x0A}},
	{0xFF,1,{0x25}},
	{0x75,1,{0x00}},
	{0x76,1,{0x00}},
	{0x77,1,{0x64}},
	{0x78,1,{0x00}},
	{0x79,1,{0x64}},
	{0x7A,1,{0x64}},
	{0x7B,1,{0x64}},
	{0x7C,1,{0x32}},
	{0x7D,1,{0x76}},
	{0x7E,1,{0x32}},
	{0x7F,1,{0x76}},
	{0x80,1,{0x00}},
	{0x81,1,{0x03}},
	{0x82,1,{0x03}},
	{0xFF,1,{0x23}},
	{0xEF,1,{0x02}},
	{0x29,1,{0x03}},
	{0x01,16,{0x00,0x00,0x00,0x0C,0x00,0x42,0x00,0x61,0x00,0x79,0x00,0x8E,0x00,0x9D,0x00,0xAC}},
	{0x02,16,{0x00,0xBB,0x00,0xE7,0x01,0x0B,0x01,0x43,0x01,0x6C,0x01,0xAF,0x01,0xEA,0x01,0xEB}},
	{0x03,16,{0x02,0x2A,0x02,0x7B,0x02,0xAC,0x02,0xEA,0x03,0x14,0x03,0x4A,0x03,0x5A,0x03,0x6C}},
	{0x04,12,{0x03,0x7F,0x03,0x94,0x03,0xAD,0x03,0xC9,0x03,0xEB,0x03,0xFF}},
	{0x0D,16,{0x00,0x00,0x00,0x0C,0x00,0x42,0x00,0x61,0x00,0x79,0x00,0x8E,0x00,0x9D,0x00,0xAC}},
	{0x0E,16,{0x00,0xBB,0x00,0xE7,0x01,0x0B,0x01,0x43,0x01,0x6C,0x01,0xAF,0x01,0xEA,0x01,0xEB}},
	{0x0F,16,{0x02,0x2A,0x02,0x7B,0x02,0xAC,0x02,0xEA,0x03,0x14,0x03,0x4A,0x03,0x5A,0x03,0x6C}},
	{0x10,12,{0x03,0x7F,0x03,0x94,0x03,0xAD,0x03,0xC9,0x03,0xEB,0x03,0xFF}},
	{0x2B,1,{0x41}},
	{0xF0,1,{0x01}},
	{0x2D,1,{0x65}},
	{0x2E,1,{0x00}},
	{0x32,1,{0x02}},
	{0x33,1,{0x1A}},//1C
	{0xFF,1,{0x20}},
	{0xF1,1,{0xE0}},
	{0xF2,1,{0x03}},
	{0xFF,1,{0x20}},
	{0xF5,1,{0xE0}},
	{0xF6,1,{0x0B}},
	{0xFF,1,{0x10}},
	{0x51,2,{0x00,0x00}},
	{0x53,1,{0x2c}},
	{0x55,1,{0x00}},
	{0x36,1,{0x08}},
	{0x69,1,{0x00}},
	{0x35,1,{0x00}},
	{0x11, 0, {0x00}},
	{REGFLAG_DELAY, 120,{}},
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 20,{}},
	{ REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table bl_level[] = {
	{ 0x51, 0x02, {0x0F,0xE9} },
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
	//+BUG S96818AA1-1936,liuzhizun2.wt,modify,2023/05/26,fix display missing
	params->dsi.vertical_sync_active = 6;
	params->dsi.vertical_backporch = 28;
	params->dsi.vertical_frontporch = 140;
	//params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 8;
	params->dsi.horizontal_backporch = 130;
	params->dsi.horizontal_frontporch = 122;
	//-BUG S96818AA1-1936,liuzhizun2.wt,modify,2023/05/26,fix display missing
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 330;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 330;	/* this value must be in MTK suggested table */
	params->dsi.data_rate = 660;
#endif
	//params->dsi.PLL_CK_CMD = 220;
	//params->dsi.PLL_CK_VDO = 255;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	/* mipi hopping setting params */
	params->dsi.dynamic_switch_mipi = 0;	/* turn off frequency hopping */
	params->dsi.data_rate_dyn = 610;
	params->dsi.PLL_CLOCK_dyn = 305;
	params->dsi.horizontal_sync_active_dyn = 4;
	params->dsi.horizontal_backporch_dyn = 84;
	params->dsi.horizontal_frontporch_dyn  = 88;

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 0;
	params->corner_pattern_width = 720;
	params->corner_pattern_height = 32;
#endif
}
enum Color {
	LOW,
	HIGH
};
static void lcm_init_power(void)
{
	int ret = 0;
	pr_err("[LCM]gc7272_xinxian_hkc lcm_init_power !\n");
	ret = lcm_power_enable();
 //   MDELAY(11);
}
//+S96818AA1-1936,daijun1.wt,modify,2023/05/23,n28-tp add SS func node and double click to wakeup
extern bool gc_gestrue_status;
static void lcm_suspend_power(void)
{
    if (gc_gestrue_status == 1) {
        pr_err("[LCM] gc7272_xinxian_hkc lcm_suspend_power gc_gestrue_status = 1!\n");
	} else {
		int ret  = 0;
        pr_err("[LCM] gc7272_xinxian_hkc lcm_suspend_power gc_gestrue_status = 0!\n");
		//lcm_reset_pin(LOW); //reduce flight standby power consumption
		MDELAY(2);
		ret = lcm_power_disable();
		MDELAY(10);
	}
}
//-S96818AA1-1936,daijun1.wt,modify,2023/05/23,n28-tp add SS func node and double click to wakeup
static void lcm_resume_power(void)
{
	pr_err("[LCM]gc7272_xinxian_hkc lcm_resume_power !\n");
	lcm_init_power();
}

static void lcm_init(void)
{
	pr_err("[LCM] gc7272_xinxian_hkc lcm_init\n");
	//lcm_reset_pin(HIGH);
	//MDELAY(10);
	lcm_reset_pin(LOW);
	MDELAY(10);
	lcm_reset_pin(HIGH);
	MDELAY(10);
	lcm_reset_pin(LOW);
	MDELAY(10);
	lcm_reset_pin(HIGH);
	MDELAY(10);
	lcm_reset_pin(LOW);
	MDELAY(10);
	lcm_reset_pin(HIGH);
	MDELAY(10);
	lcm_reset_pin(LOW);
	MDELAY(10);
	lcm_reset_pin(HIGH);
	MDELAY(25);
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	pr_err("[LCM] gc7272_xinxian_hkc----init success ----\n");
}
static void lcm_suspend(void)
{ 
	if (gc_gestrue_status == 1) {
		push_table(NULL, lcm_aot_suspend_setting, sizeof(lcm_aot_suspend_setting) / sizeof(struct LCM_setting_table), 1);
		pr_err("[LCM] gc7272_xinxian_hkc lcm_suspend_gesture_flag = 1!\n");
	}
	else{
		pr_err("[LCM] lcm_suspend\n");
		push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	}
}
static void lcm_resume(void)
{
	pr_err("[LCM] lcm_resume\n");
	lcm_init();
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
	pr_err("ATA check size = 0x%x,0x%x,0x%x,0x%x\n", x0_MSB, x0_LSB, x1_MSB, x1_LSB);
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
	bl_lvl = wingtech_bright_to_bl(level,255,10,4095,48);
	if(bl_lvl > 4073)
		bl_lvl = 4073;
	pr_err("%s,gc7272_xinxian_hkc backlight: level = %d,bl_lvl=%d\n", __func__, level,bl_lvl);
	// set 12bit
	bl_level[0].para_list[0] = (bl_lvl & 0xF00) >> 8;
	bl_level[0].para_list[1] = (bl_lvl & 0xFF);
	pr_err("gc7272_xinxian_hkc backlight: para_list[0]=%x,para_list[1]=%x\n",bl_level[0].para_list[0],bl_level[0].para_list[1]);
	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}
struct LCM_DRIVER n28_gc7272_dsi_vdo_hdp_xinxian_hkc_drv = {
	.name = "n28_gc7272_dsi_vdo_hdp_xinxian_hkc",
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
