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

#define LCM_ID (0x98)

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
#define FRAME_HEIGHT									(1520)

#define LCM_PHYSICAL_WIDTH									(67608)
#define LCM_PHYSICAL_HEIGHT									(142728)


#define REGFLAG_DELAY		0xFFFC
#define REGFLAG_UDELAY	0xFFFB
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


struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0xB9, 3, {0x83,0x11,0x2A}},
	{0x28, 0, {} },
	{REGFLAG_DELAY,20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 80, {} }

};

//static struct LCM_setting_table init_setting_cmd[] = {
//	{ 0xFF, 0x03, {0x98, 0x81, 0x03} },
//};

static struct LCM_setting_table init_setting_vdo[] = {
//-------------  Display Initial Code Setting  -------------------------
{0xB9, 3, {0x83,0x11,0x2A}},
{0xBA, 1, {0x72}},
//{0xB6, 3, {0x64,0x64,0xE3}},   //1
{0xB1, 8, {0x08,0x17,0x17,0x80,0x80,0x4D,0x4F,0xAA}},
{0xB2, 14, {0x00,0x02,0x00,0x52,0xF0,0x00,0x08,0x26,0x23,0x11,0x15,0x00,0x11,0xA3}},
{0xB4, 27, {0x87,0x5F,0x68,0x07,0x08,0xEF,0x08,0xE6,0x08,0xE6,0x08,0xE6,0x00,0xFF,0x00,0xFF,0x00,0x00,0x0E,0x0F,0x00,0x30,0x15,0x07,0x16,0x00,0x30}},
{0xC7, 6, {0x70,0x00,0x04,0xE0,0x33,0x00}},
{0xE9, 1, {0xC3}},
{REGFLAG_DELAY, 1, {}},
{0xCB, 2, {0xD2,0x48}},
{0xE9, 1, {0x3F}},
{REGFLAG_DELAY, 1, {}},

{0xCC, 1, {0x08}},
{0xD2, 2, {0x2B,0x2B}},
{0xD3, 49, {0xC0,0x00,0x00,0x00,0x00,0x01,0x00,0x08,0x08,0x03,0x03,0x22,0x18,0x07,0x07,0x07,0x07,0x32,0x10,0x06,0x00,0x06,0x32,0x10,0x07,0x00,0x07,0x32,0x15,0xFD,0x05,0xFD,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x05,0xEA,0x00,0x00,0x00,0x06,0x0D,0x2E,0x08}},
{REGFLAG_DELAY, 5, {}},
{0xD5, 48, {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xC0,0xC0,0x18,0x18,0x19,0x19,0x18,0x18,0x40,0x40,0x18,0x18,0x18,0x18,0x3F,0x3F,0x28,0x28,0x24,0x24,0x02,0x03,0x02,0x03,0x00,0x01,0x00,0x01,0x31,0x31,0x31,0x31,0x30,0x30,0x30,0x30,0x2F,0x2F,0x2F,0x2F}},
{REGFLAG_DELAY, 5, {}},
{0xD6, 48, {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x40,0x40,0x18,0x18,0x18,0x18,0x19,0x19,0x40,0x40,0x18,0x18,0x18,0x18,0x3F,0x3F,0x24,0x24,0x28,0x28,0x01,0x00,0x01,0x00,0x03,0x02,0x03,0x02,0x31,0x31,0x31,0x31,0x30,0x30,0x30,0x30,0x2F,0x2F,0x2F,0x2F}},
{REGFLAG_DELAY, 5, {}},
{0xD8, 24, {0xAA,0xEA,0xAA,0xAA,0xAA,0xAA,0xAA,0xEA,0xAA,0xAA,0xAA,0xAA,0xAA,0xEA,0xAB,0xAA,0xAA,0xAA,0xAA,0xEA,0xAB,0xAA,0xAA,0xAA}},
{0xE7, 23, {0x0E,0x0E,0x1E,0x7B,0x1D,0x7B,0x00,0x32,0x60,0x60,0x00,0x00,0x02,0x02,0x02,0x05,0x14,0x14,0x32,0xB9,0x23,0xB9,0x08}},
{0xBD, 1, {0x01}},
{0xCB, 3, {0x25,0x11,0x01}},
{0xD3, 8, {0x00,0x00,0x19,0x10,0x00,0x0A,0x00,0x81}},
{REGFLAG_DELAY, 5, {}},
{0xD8, 24, {0xAA,0x2E,0x28,0x00,0x00,0x00,0xAA,0x2E,0x28,0x00,0x00,0x00,0xAA,0xEE,0xAA,0xAA,0xAA,0xAA,0xAA,0xEE,0xAA,0xAA,0xAA,0xAA}},
{0xE7, 8, {0x02,0x00,0x68,0x01,0x70,0x0D,0x50,0x0E}},
{0xBD, 1, {0x02}},
{0xB4, 9, {0x00,0x92,0x12,0x11,0x88,0x12,0x12,0x00,0x53}},
{0xD8, 12, {0xAA,0xFF,0xFF,0xFF,0xFF,0xFF,0xAA,0xFF,0xFF,0xFF,0xFF,0xFF}},
{0xE7, 28, {0x00,0x00,0x08,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x02}},
{0xBD, 1, {0x03}},
{0xD8, 24, {0xAA,0xAA,0xEA,0xAA,0xAA,0xAA,0xAA,0xAA,0xEA,0xAA,0xAA,0xAA,0xAA,0xEF,0xFF,0xFF,0xFF,0xFF,0xAA,0xEF,0xFF,0xFF,0xFF,0xFF}},
{0xBD, 1, {0x00}},
{0xC1, 1, {0x01}},
{0xBD, 1, {0x01}},
{0xC1, 57, {0xFC,0xF8,0xF2,0xEC,0xE7,0xE1,0xDB,0xCF,0xC9,0xC3,0xBD,0xB7,0xB1,0xAC,0xA7,0xA1,0x9C,0x97,0x93,0x8A,0x82,0x7A,0x73,0x6B,0x63,0x5D,0x56,0x4F,0x48,0x43,0x3D,0x36,0x30,0x2A,0x24,0x1E,0x19,0x13,0x0E,0x09,0x06,0x05,0x03,0x02,0x01,0xC7,0x5F,0xAF,0xC2,0xA1,0x20,0xC5,0xCF,0x87,0x7A,0xF9,0x80}},  //3
{0xBD, 1, {0x02}},
{0xC1, 57, {0xFC,0xF8,0xF2,0xEC,0xE7,0xE1,0xDB,0xCF,0xC9,0xC3,0xBD,0xB7,0xB1,0xAC,0xA7,0xA1,0x9C,0x97,0x93,0x8A,0x82,0x7A,0x73,0x6B,0x63,0x5D,0x56,0x4F,0x48,0x43,0x3D,0x36,0x30,0x2A,0x24,0x1E,0x19,0x13,0x0E,0x09,0x06,0x05,0x03,0x02,0x01,0xC7,0x5F,0xAF,0xC2,0xA1,0x20,0xC5,0xCF,0x87,0x7A,0xF9,0x80}},  //4
{0xBD, 1, {0x03}},
{0xC1, 57, {0xFC,0xF8,0xF2,0xEC,0xE7,0xE1,0xDB,0xCF,0xC9,0xC3,0xBD,0xB7,0xB1,0xAC,0xA7,0xA1,0x9C,0x97,0x93,0x8A,0x82,0x7A,0x73,0x6B,0x63,0x5D,0x56,0x4F,0x48,0x43,0x3D,0x36,0x30,0x2A,0x24,0x1E,0x19,0x13,0x0E,0x09,0x06,0x05,0x03,0x02,0x01,0xC7,0x5F,0xAF,0xC2,0xA1,0x20,0xC5,0xCF,0x87,0x7A,0xF9,0x80}},  //5
{0xBD, 1, {0x00}},
{0xE9, 1, {0xC4}},
{REGFLAG_DELAY, 1, {}},

{0xCF, 1, {0xC0}},
{0xE9, 1, {0x3F}},
{REGFLAG_DELAY, 1, {}},

{0xC9, 4, {0x04,0x0E,0x52,0x01}}, //2-PWM fre=30KHZ->15KHZ
{REGFLAG_DELAY, 5, {}},
{0x55, 1, {0x00}},
{REGFLAG_DELAY, 5, {}},
{0x53, 1, {0x2C}},
{REGFLAG_DELAY, 5, {}},

 //CABC-GAMMA START
 //{0xBD, 1, {0x00}},
 //{0xE4, 48, {0x25,0x01,0x2C,0x00,0x08,0x00,0x10,0x08,0x04,0x04,
 //            0x00,0x00,0x00,0x32,0x32,0x32,0xFF,0xFF,0xFF,0xFF,
 //            0xFF,0xEF,0x10,0x38,0x10,0x05,0x00,0x00,0x01,0x00,
 //            0x10,0x80,0x00,0x00,0x90,0x3C,0x1E,0x0F,0x01,0x00,
 //            0x02,0x00,0x00,0x05,0x10,0x10,0x80,0x00}},//CABC dimming ,CABC PWM DUTY
 //CABC-GAMMA END
{0x11, 1, {0x00}},
{REGFLAG_DELAY, 40, {}},
{0x29, 1, {0x00}},
{REGFLAG_DELAY, 60, {}}
};

static struct LCM_setting_table bl_level[] = {
	{ 0x51, 0x02, {0xF, 0xFF} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};
//+add by songbinbo.wt for ILITEK tp reset sequence 20190417
extern void lcd_load_ili_fw(void);
//-add by songbinbo.wt for ILITEK tp reset sequence 20190417
extern void himax_lcd_resume_func(void);
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
			dsi_set_cmdq_V22(cmdq,cmd, table[i].count, table[i].para_list, force_update);
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
	params->dsi.vertical_frontporch = 120;
	//params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 28;
	params->dsi.horizontal_backporch = 42;
	params->dsi.horizontal_frontporch = 174;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
	/*params->dsi.ssc_disable = 1;*/
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 270;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 400;	/* this value must be in MTK suggested table */
#endif
	//params->dsi.PLL_CK_CMD = 220;
	//params->dsi.PLL_CK_VDO = 255;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

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

	//pr_err("[LCM][GPIO]lcm_init_power !\n");
	lcm_power_enable();
}

static void lcm_suspend_power(void)
{
	//pr_err("[LCM][GPIO]lcm_suspend_power !\n");
	//lcm_reset_pin(0);
	MDELAY(2);
	lcm_power_disable();
}

static void lcm_resume_power(void)
{

	//pr_err("[LCM][GPIO]lcm_resume_power !\n");
	lcm_power_enable();
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
	MDELAY(5);
	lcm_reset_pin(0);
	MDELAY(2);
	lcm_reset_pin(1);
	MDELAY(50);
	//+add by songbinbo.wt for himax tp reset sequence 20190417
	 pr_debug("[LCM before himax_lcd_resume_func.\n]");
	himax_lcd_resume_func();
	pr_debug("[LCM after himax_lcd_resume_func.\n]");
	//-add by songbinbo.wt for himax tp reset sequence 20190417
	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	//pr_err("[LCM] hx83112a--lide--sm5109----init success ----\n");

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
		bl_level[0].para_list[0] = (level&0xF00)>>8;
		bl_level[0].para_list[1] = (level&0xFF);
	pr_debug("[lide_hx83112a]para_list[0]=%x,para_list[1]=%x\n",bl_level[0].para_list[0],bl_level[0].para_list[1]);
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
struct boe_panel_white_point{
	unsigned short int white_x;
	unsigned short int white_y;
};

//static struct boe_panel_white_point white_point;
static struct LCM_setting_table set_cabc[] = {
	{0xB9, 3, {0x83,0x11,0x2A}},
	{0x55, 1, {0x00}},
	{ REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int cabc_status;
static void lcm_set_cabc_cmdq(void *handle, unsigned int enable)
{
	pr_err("[lide_hx83112a] cabc set to %d\n", enable);
	if (enable==1){
		set_cabc[1].para_list[0]=0x01;
		push_table(handle, set_cabc, sizeof(set_cabc) / sizeof(struct LCM_setting_table), 1);
		pr_err("[lide_hx83112a] cabc set enable, set_cabc[0x%2x]=%x \n",set_cabc[1].cmd,set_cabc[1].para_list[0]);
	}else if (enable == 0){
		set_cabc[1].para_list[0]=0x00;
		push_table(handle, set_cabc, sizeof(set_cabc) / sizeof(struct LCM_setting_table), 1);
		pr_err("[lide_hx83112a] cabc set disable, set_cabc[0x%2x]=%x \n",set_cabc[1].cmd,set_cabc[1].para_list[0]);
	}
	cabc_status = enable;
}

static void lcm_get_cabc_status(int *status)
{
	pr_err("[lide_hx83112a] cabc get to %d\n", cabc_status);
	*status = cabc_status;
}

//#define WHITE_POINT_BASE_X 167
//#define WHITE_POINT_BASE_Y 192
#if (LCM_DSI_CMD_MODE)
struct LCM_DRIVER ili9881c_hdp_dsi_cmd_ilitek_rt5081_lcm_drv = {
	.name = "hx83112a_hd_plus_dsi_cmd_lide_lcm_drv",
#else

struct LCM_DRIVER hx83112a_hd_plus_dsi_vdo_lide_lcm_drv = {
	.name = "hx83112a_hd_plus_vdo_lide",
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
	.set_cabc_cmdq = lcm_set_cabc_cmdq,
	.get_cabc_status = lcm_get_cabc_status,
#if (LCM_DSI_CMD_MODE)
	.validate_roi = lcm_validate_roi,
#endif

};
