
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

extern void lcm_bais_enn_enable(unsigned int mode);
extern void lcm_bais_enp_enable(unsigned int mode);

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	//{ 0xFF, 0x03, {0x98, 0x82, 0x00} },
	{REGFLAG_DELAY, 5, {} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {

	{0xFF, 3,{0x98,0x82,0x01}},//3H
	{0x00, 1,{0x47}},//STVA
	{0x01, 1,{0x32}},//STVA Duty 4H
	{0x02, 1,{0x00}},//45%CLK duty
	{0x03, 1,{0x00}},//45%CLK duty
	{0x04, 1,{0x04}},//STVB
	{0x05, 1,{0x32}},//STV Duty 4H
	{0x06, 1,{0x00}},//45%CLK duty
	{0x07, 1,{0x00}},//45%CLK duty
	{0x08, 1,{0x85}},//CLK RISE
	{0x09, 1,{0x04}},//CLK FALL
	{0x0A, 1,{0x72}},//CLK Duty 4H
	{0x0B, 1,{0x00}},
	{0x0C, 1,{0x00}},//45%CLK duty
	{0x0D, 1,{0x00}},//45%CLK duty
	{0x0E, 1,{0x00}},
	{0x0F, 1,{0x00}},
	{0x28, 1,{0x48}},//STCH1
	{0x29, 1,{0x88}},
	{0x2A, 1,{0x48}},//STCH2
	{0x2B, 1,{0x88}},

	{0x31, 1,{0x0C}},// RST_L
	{0x32, 1,{0x02}},// VGL
	{0x33, 1,{0x02}},// VGL
	{0x34, 1,{0x23}},// GLV
	{0x35, 1,{0x08}},// STV1_L
	{0x36, 1,{0x0A}},// STV2_L
	{0x37, 1,{0x06}},// VDD
	{0x38, 1,{0x06}},// VDD
	{0x39, 1,{0x10}},// CLK1_L
	{0x3A, 1,{0x10}},// CLK1_L
	{0x3B, 1,{0x12}},// CLK2_L
	{0x3C, 1,{0x12}},// CLK2_L
	{0x3D, 1,{0x14}},// CLK1B_L
	{0x3E, 1,{0x14}},// CLK1B_L
	{0x3F, 1,{0x16}},// CLK2B_L
	{0x40, 1,{0x16}},// CLK2B_L
	{0x41, 1,{0x02}},// VGL
	{0x42, 1,{0x07}},// HIZ
	{0x43, 1,{0x07}},// HIZ
	{0x44, 1,{0x07}},// HIZ
	{0x45, 1,{0x07}},// HIZ
	{0x46, 1,{0x07}},// HIZ

	{0x47, 1,{0x0D}},// RST_R
	{0x48, 1,{0x02}},// VGL
	{0x49, 1,{0x02}},// VGL
	{0x4A, 1,{0x23}},// GLV
	{0x4B, 1,{0x09}},// STV1_R
	{0x4C, 1,{0x0B}},// STV2_R
	{0x4D, 1,{0x06}},// VDD
	{0x4E, 1,{0x06}},// VDD
	{0x4F, 1,{0x11}},// CLK1_R
	{0x50, 1,{0x11}},// CLK1_R
	{0x51, 1,{0x13}},// CLK2_R
	{0x52, 1,{0x13}},// CLK2_R
	{0x53, 1,{0x15}},// CLK1B_R
	{0x54, 1,{0x15}},// CLK1B_R
	{0x55, 1,{0x17}},// CLK2B_R
	{0x56, 1,{0x17}},// CLK2B_R
	{0x57, 1,{0x02}},// VGL
	{0x58, 1,{0x07}},// HIZ
	{0x59, 1,{0x07}},// HIZ
	{0x5A, 1,{0x07}},// HIZ
	{0x5B, 1,{0x07}},// HIZ
	{0x5C, 1,{0x07}},// HIZ

	{0x61, 1,{0x0C}},// RST_L
	{0x62, 1,{0x02}},// VGL
	{0x63, 1,{0x02}},// VGL
	{0x64, 1,{0x23}},// GLV
	{0x65, 1,{0x08}},// STV1_L
	{0x66, 1,{0x0A}},// STV2_L
	{0x67, 1,{0x06}},// VDD
	{0x68, 1,{0x06}},// VDD
	{0x69, 1,{0x10}},// CLK1_L
	{0x6A, 1,{0x10}},// CLK1_L
	{0x6B, 1,{0x12}},// CLK2_L
	{0x6C, 1,{0x12}},// CLK2_L
	{0x6D, 1,{0x14}},// CLK1B_L
	{0x6E, 1,{0x14}},// CLK1B_L
	{0x6F, 1,{0x16}},// CLK2B_L
	{0x70, 1,{0x16}},// CLK2B_L
	{0x71, 1,{0x02}},// VGL
	{0x72, 1,{0x07}},// HIZ
	{0x73, 1,{0x07}},// HIZ
	{0x74, 1,{0x07}},// HIZ
	{0x75, 1,{0x07}},// HIZ
	{0x76, 1,{0x07}},// HIZ

	{0x77, 1,{0x0D}},// RST_R
	{0x78, 1,{0x02}},// VGL
	{0x79, 1,{0x02}},// VGL
	{0x7A, 1,{0x23}},// GLV
	{0x7B, 1,{0x09}},// STV1_R
	{0x7C, 1,{0x0B}},// STV2_R
	{0x7D, 1,{0x06}},// VDD
	{0x7E, 1,{0x06}},// VDD
	{0x7F, 1,{0x11}},// CLK1_R
	{0x80, 1,{0x11}},// CLK1_R
	{0x81, 1,{0x13}},// CLK2_R
	{0x82, 1,{0x13}},// CLK2_R
	{0x83, 1,{0x15}},// CLK1B_R
	{0x84, 1,{0x15}},// CLK1B_R
	{0x85, 1,{0x17}},// CLK2B_R
	{0x86, 1,{0x17}},// CLK2B_R
	{0x87, 1,{0x02}},// VGL
	{0x88, 1,{0x07}},// HIZ
	{0x89, 1,{0x07}},// HIZ
	{0x8A, 1,{0x07}},// HIZ
	{0x8B, 1,{0x07}},// HIZ
	{0x8C, 1,{0x07}},// HIZ
	{0xB0, 1,{0x33}},
	{0xB1, 1,{0x33}},
	{0xB2, 1,{0x00}},
	{0xD0, 1,{0x01}},
	{0xD1, 1,{0x00}},
	{0xE2, 1,{0x00}},
	{0xE6, 1,{0x22}},
	{0xE7, 1,{0x54}},

	{0xFF, 3,{0x98,0x82,0x02}},
	{0xF1, 1,{0x1C}},// Tcon ESD option
	{0x4B, 1,{0x5A}},// line_chopper
	{0x50, 1,{0xCA}},// line_chopper
	{0x51, 1,{0x00}},// line_chopper
	{0x06, 1,{0x8F}},// Internal Line Time (RTN)
	{0x0B, 1,{0xA0}},// Internal VFP[9]
	{0x0C, 1,{0x00}},// Internal VFP[8]
	{0x0D, 1,{0x14}},// Internal VBP
	{0x0E, 1,{0xE6}},// Internal VFP
	{0x4E, 1,{0x11}},// SRC BIAS
	{0xF2, 1,{0x4A}},
	{0x40, 1,{0x45}},// sdt
	{0x43, 1,{0x02}},// Tcon
	{0x47, 1,{0x13}},// Tcon
	{0x49, 1,{0x03}},// Tcon
	{0x4D, 1,{0xCF}},// PST enable

	{0xFF, 3,{0x98,0x82,0x03}},
	{0x83, 1,{0x20}},//12bit dbv,7.81kHz ledpwm
	{0x84, 1,{0x00}},

	{0xFF, 3,{0x98,0x82,0x05}},
	{0x58, 1,{0x61}},// VGL 2x
	{0x63, 1,{0x97}},// GVDDN = -5.5V
	{0x64, 1,{0x97}},// GVDDP = 5.5V
	{0x68, 1,{0xA1}},// VGHO = 15V
	{0x69, 1,{0xA7}},// VGH = 16V
	{0x6A, 1,{0x79}},// VGLO = -10V
	{0x6B, 1,{0x6B}},// VGL = -11V
	{0x85, 1,{0x37}},// HW RESET option
	{0x46, 1,{0x00}},// LVD HVREG option

	{0xFF, 3,{0x98,0x82,0x06}},
	{0xD9, 1,{0x1F}},// 4Lane
	{0xC0, 1,{0x40}},// NL = 1600
	{0xC1, 1,{0x16}},// NL = 1600
	{0x80, 1,{0x09}},
	//{0xB6, 1,{0x30}},

	{0xFF, 3,{0x98,0x82,0x07}},
	{0x00, 1,{0x08}},

	{0xFF, 3,{0x98,0x82,0x08}},
	{0xE0, 27,{0x40,0x24,0x94,0xD5,0x1B,0x55,0x52,0x7B,0xAC,0xD2,0xAA,0x0C,0x3B,0x64,0x8B,0xEA,0xB3,0xE1,0xFE,0x21,0xFF,0x3F,0x65,0x94,0xBC,0x03,0xEC}},
	{0xE1, 27,{0x40,0x24,0x94,0xD5,0x1B,0x55,0x52,0x7B,0xAC,0xD2,0xAA,0x0C,0x3B,0x64,0x8B,0xEA,0xB3,0xE1,0xFE,0x21,0xFF,0x3F,0x65,0x94,0xBC,0x03,0xEC}},

	{0xFF, 3,{0x98,0x82,0x0B}},
	{0x9A, 1,{0x89}},
	{0x9B, 1,{0x02}},
	{0x9C, 1,{0x06}},
	{0x9D, 1,{0x06}},
	{0x9E, 1,{0xE1}},
	{0x9F, 1,{0xE1}},
	{0xAA, 1,{0x22}},
	{0xAB, 1,{0xE0}},// AutoTrimType
	{0xAC, 1,{0x7F}},// trim_osc_max
	{0xAD, 1,{0x3F}},// trim_osc_min

	{0xFF, 3,{0x98,0x82,0x0E}},
	{0x11, 1,{0x10}},//TSVD Rise Poisition
	{0x12, 1,{0x08}},// LV mode TSHD Rise position
	{0x13, 1,{0x10}},// LV mode TSHD Rise position
	{0x00, 1,{0xA0}},// LV mode
	{0xFF, 3,{0x98,0x82,0x00}},

	{0x35,1,{0x00}},
	{0x51,2,{0x00,0x00}},
	{0x53,1,{0x2C}},
	{0x55,1,{0x00}},

	{0x11,0,{}},
	{REGFLAG_DELAY, 62, {}},
	{0x29,0,{}},
	{REGFLAG_DELAY, 5, {}},
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table bl_level[] = {

	{ 0x51, 0x02, {0x0F, 0xFF} },
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
	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 16;
	params->dsi.vertical_frontporch = 230;
	//params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 20;
	params->dsi.horizontal_backporch = 30;
	params->dsi.horizontal_frontporch = 32;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 282;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 284;	/* this value must be in MTK suggested table */

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
	pr_debug("[LCM]ili9882q truly lcm_init_power !\n");
    lcm_reset_pin(LOW);
	ret = lcm_power_enable();
    MDELAY(10);
}

extern bool  gestrue_status;
static void lcm_suspend_power(void)
{
	pr_debug("[LCM] ili9882q_txd lcm_suspend_power !\n");
	if(gestrue_status == 1){
		printk("[ILITEK]lcm_suspend_power gesture_status = 1\n ");
	} else {
		//int ret  = 0;
		printk("[ILITEK]lcm_suspend_power gestrue_status = 0\n ");
		pr_debug("[LCM] ili9882q truly lcm_suspend_power !\n");
		//lcm_reset_pin(LOW);
		MDELAY(2);
		//ret = lcm_power_disable();
		lcm_bais_enn_enable(LOW);
		MDELAY(10);
		lcm_bais_enp_enable(LOW);
		MDELAY(10);
	}
}

static void lcm_resume_power(void)
{
	pr_debug("[LCM]ili9882q truly lcm_resume_power !\n");
	lcm_init_power();
}

extern void ili_resume_by_ddi(void);
static void lcm_init(void)
{
	pr_debug("[LCM] ili9882q truly lcm_init\n");
	lcm_reset_pin(HIGH);
	MDELAY(5);
	lcm_reset_pin(LOW);
	MDELAY(5);
	lcm_reset_pin(HIGH);
	MDELAY(5);
	ili_resume_by_ddi();
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	pr_debug("[LCM] ili9882q truly----init success ----\n");
}

static void lcm_suspend(void)
{
	pr_debug("[LCM] lcm_suspend\n");

	//add for TP suspend
	panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	printk("[ILITEK]tpd_ilitek_notifier_callback in lcm_suspend\n ");
	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);
	/* lcm_set_gpio_output(GPIO_LCM_RST,0); */
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
	bl_lvl = wingtech_bright_to_bl(level,255,10,4095,48);
	pr_debug("%s,ili9882q truly backlight: level = %d,bl_lvl=%d\n", __func__, level,bl_lvl);
	// set 12bit
	bl_level[0].para_list[0] = (bl_lvl & 0xF00) >> 8;
	bl_level[0].para_list[1] = (bl_lvl & 0xFF);
	pr_debug("ili9882q truly backlight: para_list[0]=%x,para_list[1]=%x\n",bl_level[0].para_list[0],bl_level[0].para_list[1]);
	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER ili9882q_dsi_vdo_hdp_truly_truly_drv = {
	.name = "ili9882q_dsi_vdo_hdp_truly_truly",
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
