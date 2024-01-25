
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
//+S96818AA1-1936,liyuhong1.wt,modify,2023/06/14,nt36528 modify the physical size of the screen
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
	unsigned char para_list[64];
};
static struct LCM_setting_table lcm_suspend_setting[] = {
	// SLEEP IN + DISPLAY OFF
	{0x6D, 2,{0x25, 0x00}},
	{0x28, 2,{0x00, 0x00}},
	{REGFLAG_DELAY, 10,{}},
	{0x10, 2,{0x00, 0x00}},
	{REGFLAG_DELAY, 120,{}},
};
static struct LCM_setting_table init_setting_vdo[] = {
	{0xF0, 3,{0x99,0x16,0x0C}},
	{0xC1,20,{0x00,0x20,0x20,0xBE,0x04,0x6E,0x74,0x04,0x40,0x06,0x22,0x70,0x35,0x20,0x07,0x11,0x84,0x4C,0x00,0x93}},
	{0xC3, 6,{0x06,0x00,0xFF,0x00,0xFF,0x5C}},
	{0xC4,12,{0x04,0x31,0xB8,0x40,0x00,0xBC,0x00,0x00,0x00,0x00,0x00,0xF0}},
	{0xC5,23,{0x03,0x21,0x96,0xC8,0x3E,0x00,0x04,0x01,0x14,0x04,0x19,0x0B,0xC6,0x03,0x64,0xFF,0x01,0x04,0x18,0x22,0x45,0x14,0x38}},
	{0xC6,13,{0x89,0x24,0x17,0x2B,0x2B,0x28,0x3F,0x03,0x16,0xA5,0x00,0x00,0x00}},
	{0xB2,29,{0x03,0x02,0x97,0x98,0x99,0x99,0x8B,0x01,0x49,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0xB3,0x00,0x00,0x00,0x55,0x55,0x00,0x00,0x00}},
	{0xB3,25,{0xF2,0x01,0x02,0x09,0x81,0xAC,0x05,0x02,0xB3,0x04,0x80,0x00,0xB3,0xAC,0xB3,0xF2,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00}},
	{0xB5,32,{0x00,0x04,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0xC1,0x00,0x2A,0x1A,0x18,0x16,0x14,0x12,0x10,0x0E,0x0C,0x0A,0x28,0x60,0xFF,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xB6,32,{0x00,0x05,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0xC1,0x00,0x2B,0x1B,0x19,0x17,0x15,0x13,0x11,0x0F,0x0D,0x0B,0x29,0x60,0xFF,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xB7,22,{0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A}},
	{0xBC,19,{0x00,0x00,0x00,0x00,0x04,0x00,0xFF,0xF0,0x0B,0x13,0x5C,0x5B,0x33,0x33,0x00,0x64,0x3C,0x5E,0x36}},
	{0xBF, 9,{0x0C,0x19,0x0C,0x19,0x00,0x11,0x04,0x18,0x50}},
	{0xC0,20,{0x40,0x93,0xFF,0xFF,0xFF,0x3F,0xFF,0x00,0xFF,0x00,0xCC,0xB1,0x23,0x45,0x67,0x89,0xAD,0xFF,0xFF,0xF0}},
	{0xC7,32,{0x76,0x54,0x32,0x22,0x23,0x45,0x67,0x76,0x30,0x76,0x54,0x32,0x22,0x23,0x45,0x67,0x76,0x30,0x31,0x00,0x01,0xFF,0xFF,0x40,0x6E,0x6E,0x40,0x00,0x00,0x00,0x00,0x00}},
	{0x80,32,{0xFF,0xF7,0xEC,0xE3,0xDD,0xD7,0xD3,0xCF,0xCB,0xC0,0xB7,0xAF,0xA8,0xA2,0x9C,0x92,0x8A,0x82,0x7A,0x79,0x70,0x67,0x5D,0x51,0x43,0x30,0x26,0x1B,0x18,0x14,0x11,0x0D}},
	{0x81,32,{0xFF,0xF7,0xEC,0xE3,0xDD,0xD7,0xD3,0xCF,0xCB,0xC0,0xB7,0xAF,0xA8,0xA2,0x9C,0x92,0x8A,0x82,0x7A,0x79,0x70,0x67,0x5D,0x51,0x43,0x30,0x26,0x1B,0x18,0x14,0x11,0x0D}},
	{0x82,32,{0xFF,0xF7,0xEC,0xE3,0xDD,0xD7,0xD3,0xCF,0xCB,0xC0,0xB7,0xAF,0xA8,0xA2,0x9C,0x92,0x8A,0x82,0x7A,0x79,0x70,0x67,0x5D,0x51,0x43,0x30,0x26,0x1B,0x18,0x14,0x11,0x0D}},
	{0x83,25,{0x01,0x0A,0x05,0x01,0x00,0x0A,0x05,0x01,0x00,0x0A,0x05,0x01,0x00,0x0A,0x06,0x02,0x00,0x0A,0x06,0x02,0x00,0x0A,0x06,0x02,0x00}},
	{0x84,27,{0x13,0x70,0xC7,0x9A,0x16,0x97,0xEE,0x72,0x30,0x13,0x70,0xC7,0x9A,0x16,0x97,0xEE,0x72,0x30,0x13,0x70,0xC7,0x9A,0x16,0x97,0xEE,0x72,0x30}},
	{0xC2, 1,{0x00}},
	{0xC8, 4,{0x46,0x00,0x88,0xF8}},
	{0xCA,30,{0x25,0x40,0x00,0x19,0x46,0x94,0x41,0x8F,0x44,0x44,0x68,0x68,0x40,0x40,0x6E,0x6E,0x40,0x40,0x33,0x00,0x01,0x01,0x0F,0x0B,0x02,0x02,0x05,0x00,0x00,0x04}},
	{0xE0, 7,{0x0C,0x00,0xB0,0x10,0x00,0x0A,0x8C}},
	{0xBD, 4,{0xA1,0x0A,0x52,0xA6}},
	{0x35, 2,{0x00,0x00}},
	{0x51, 2,{0x00,0x00}},
	{0x53, 2,{0x2C,0x00}},
	{0x11, 2,{0x00,0x00}},
	{REGFLAG_DELAY, 120,{}},
	{0x29, 2,{0x00,0x00}},
	{REGFLAG_DELAY, 20,{}},
	{0x6D, 2,{0x02,0x00}},
	{0xBD, 4,{0xA1,0x0A,0x52,0xAE}},
	{0xF0, 3,{0x00,0x00,0x00}},
	{REGFLAG_DELAY, 1,{}},
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
	params->dsi.mode = BURST_VDO_MODE;
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
	params->dsi.vertical_backporch = 32;
	params->dsi.vertical_frontporch = 190;
	//params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active = 4;
	params->dsi.horizontal_backporch = 110;
	params->dsi.horizontal_frontporch = 116;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 282;	/* this value must be in MTK suggested table */
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
	params->dsi.dynamic_switch_mipi = 0;
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
	pr_debug("[LCM]icnl9916c_dsbj lcm_init_power !\n");
    //lcm_reset_pin(LOW);
	ret = lcm_power_enable();
    MDELAY(11);
}

extern bool chipone_gestrue_status;
static void lcm_suspend_power(void)
{
	if (chipone_gestrue_status == 1) {
		pr_debug("[LCM] icnl9916c_dsbj lcm_suspend_power chipone_gestrue_status = 1!\n");
	} else {
		//int ret  = 0;
		pr_debug("[LCM] icnl9916c_dsbj lcm_suspend_power chipone_gestrue_status = 0!\n");
		//lcm_reset_pin(LOW);
		MDELAY(2);
		lcm_bais_enn_enable(LOW);
		MDELAY(8);
		lcm_bais_enp_enable(LOW);
		MDELAY(10);
	}
}
static void lcm_resume_power(void)
{
	pr_debug("[LCM]icnl9916c_dsbj lcm_resume_power !\n");
	lcm_init_power();
}

static void lcm_init(void)
{
	pr_debug("[LCM] icnl9916c_dsbj lcm_init\n");
	lcm_reset_pin(HIGH);
	MDELAY(5);
	lcm_reset_pin(LOW);
	MDELAY(5);
	lcm_reset_pin(HIGH);
	MDELAY(12);
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	pr_debug("[LCM] icnl9916c_dsbj----init success ----\n");
}
static void lcm_suspend(void)
{
	pr_debug("[LCM] lcm_suspend\n");
	push_table(NULL, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
}
static void lcm_resume(void)
{
	pr_debug("[LCM] lcm_resume\n");
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
	pr_debug("%s,icl9916c_xinian backlight: level = %d,bl_lvl=%d\n", __func__, level,bl_lvl);
	// set 12bit
	bl_level[0].para_list[0] = (bl_lvl & 0xF00) >> 8;
	bl_level[0].para_list[1] = (bl_lvl & 0xFF);
	pr_debug("icl9916c_xinxian backlight: para_list[0]=%x,para_list[1]=%x\n",bl_level[0].para_list[0],bl_level[0].para_list[1]);
	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}
struct LCM_DRIVER n28_icnl9916c_dsi_vdo_hdp_dsbj_mdt_drv = {
	.name = "n28_icnl9916c_dsi_vdo_hdp_dsbj_mdt",
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
