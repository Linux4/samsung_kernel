
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

#include "panel_notifier.h"  //tp suspend before lcd suspend

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

#define LCM_DSI_CMD_MODE		0
#define FRAME_WIDTH				(720)
#define FRAME_HEIGHT			(1600)

#define LCM_PHYSICAL_WIDTH		(67932)
#define LCM_PHYSICAL_HEIGHT		(150960)

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

extern void tddi_lcm_tp_reset_output(int level);

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting1[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
};

static struct LCM_setting_table lcm_suspend_setting2[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0xB9, 3, {0x83, 0x10, 0x8A} },
	{0xBD, 1, {0x00} },
	{0xB1, 1, {0x2D} },
	{REGFLAG_DELAY, 50, {} },
};

static struct LCM_setting_table r4f_setting_vdo[] = {
	{0x4f, 0, {}},
	{REGFLAG_DELAY, 30, {} },
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xB9,0x03,{0x83, 0x10, 0x8A}},
	{0xBD,0x01,{0x00}},
	{0xB1,0x12,{0x2C, 0x35, 0x35, 0x31, 0x31, 0x22, 0x43, 0x57, 0x16, 0x16, 0x0C, 0x98, 0x21, 0x11, 0x46, 0x34, 0x0C, 0x04}},
	{0xB2,0x0F,{0x00, 0x26, 0xD0, 0x40, 0x00, 0x24, 0xC6, 0x00, 0x98, 0x11, 0x00, 0x00, 0x01, 0x14, 0x02}},
	{0xB4,0x0E,{0xA0, 0x5A, 0xA0, 0x5A, 0xA0, 0x5A, 0xA0, 0x5A, 0xA0, 0x5A, 0x02, 0x8C, 0x01, 0xFF}},
	{0xB6,0x03,{0xD0, 0xD0, 0x00}},
	{0xBA,0x04,{0x73, 0x03, 0xA8, 0x95}}, 
	{0xBF,0x02,{0xFC, 0x40}},
	{0xC0,0x06,{0x33, 0x33, 0x11, 0x00, 0xB3, 0x5F}},
	{0xE9,0x01,{0xC5}},
	{0xC7,0x08,{0x88, 0xCA, 0x08, 0x14, 0x02, 0x04, 0x04, 0x04}},
	{0xE9,0x01,{0x3F}},
	{0xCB,0x05,{0x13, 0x08, 0xE0, 0x0E, 0x65}}, 
	{0xCC,0x01,{0x02}},
	{0xD1,0x02,{0x27, 0x02}},
	{0xD2,0x04,{0x00, 0x00, 0x00, 0x00}},
	{0xD3,0x1F,{0xC0, 0x00, 0x08, 0x08, 0x00, 0x00, 0x47, 0x47, 0x44, 0x4B, 0x1E, 0x1E, 0x1E, 0x1E, 0x32, 0x10, 0x1A, 0x00, 0x1A, 0x32, 0x10, 0x15, 0x00, 0x15, 0x32, 0x16, 0x6A, 0x06, 0x6A, 0x00, 0x00}}, 
	{0xD5,0x2C,{0x18, 0x18, 0x18, 0x18, 0x24, 0x24, 0x18, 0x18, 0x20, 0x21, 0x22, 0x23, 0x19, 0x19, 0x19, 0x19, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}},  
	{0xD6,0x2C,{0x18, 0x18, 0x19, 0x19, 0x24, 0x24, 0x18, 0x18, 0x23, 0x22, 0x21, 0x20, 0x18, 0x18, 0x19, 0x19, 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}},
	{0xD8,0x24,{0x2A, 0xAB, 0xAA, 0x00, 0x00, 0x00, 0x2A, 0xAB, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xE0,0x2E,{0x00, 0x03, 0x0A, 0x0F, 0x15, 0x20, 0x34, 0x3B, 0x42, 0x3F, 0x5B, 0x63, 0x6B, 0x7D, 0x7F, 0x8A, 0x95, 0xA9, 0xAA, 0x54, 0x5D, 0x68, 0x73, 0x00, 0x03, 0x0A, 0x0F, 0x15, 0x20, 0x34, 0x3B, 0x42, 0x3F, 0x5B, 0x63, 0x6B, 0x7D, 0x7F, 0x8A, 0x95, 0xA9, 0xAA, 0x54, 0x5D, 0x68, 0x73}},
	{0xE7,0x13,{0x07, 0x14, 0x14, 0x15, 0x0F, 0x84, 0x00, 0x3C, 0x3C, 0x32, 0x32, 0x00, 0x00, 0x00, 0x00, 0x7A, 0x11, 0x01, 0x02}}, 
	{0xBD,0x01,{0x01}}, 
	{0xB1,0x02,{0x01, 0x1B}},  
	{0xBF,0x01,{0x0E}}, 
	{0xE9,0x01,{0xC8}}, 
	{0xD3,0x01,{0x14}}, 
	{0xE9,0x01,{0x3F}}, 
	{0xD8,0x24,{0x15, 0x55, 0x55, 0x00, 0x00, 0x00, 0x15, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xE7,0x0D,{0x01, 0x00, 0x20, 0x01, 0x00, 0x00, 0x29, 0x02, 0x33, 0x04, 0x1B, 0x00, 0x00}},
	{0xBD,0x01,{0x02}}, 
	{0xD8,0x0C,{0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0}},
	{0xBD,0x01,{0x03}},
	{0xD8,0x18,{0xAE, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0xAE, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0xAE, 0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0xAE, 0xAA,0xAA, 0xAA, 0xAA, 0xA0}},
	{0xBD,0x01,{0x00}},
	{0xC9,0x05,{0x00, 0x1E, 0x08, 0x20, 0x01}},
	{0x53,0x01,{0x2C}},
	{0x11, 0, {}},
	{REGFLAG_DELAY, 100, {}},
	{0xE9,0x01,{0xFF}},
	{0xBF,0x01,{0x02}},
	{0xE9,0x01,{0x3F}},
	{0xB9,0x03,{0x00, 0x00, 0x00}},
	{0x29, 0, {}},
	{REGFLAG_DELAY, 1, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
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
	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 30;
	params->dsi.vertical_frontporch = 200;
	/* params->dsi.vertical_frontporch_for_low_power = 540; */
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 20;
	params->dsi.horizontal_backporch = 20;
	params->dsi.horizontal_frontporch = 80;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;

#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 350;	/* this value must be in MTK suggested table */
#else
	params->dsi.PLL_CLOCK = 295;	/* this value must be in MTK suggested table */
#endif
	params->dsi.data_rate = 590;
	/* mipi hopping setting params */
	params->dsi.dynamic_switch_mipi = 1;
	params->dsi.data_rate_dyn = 610;
	params->dsi.PLL_CLOCK_dyn = 305;
	params->dsi.horizontal_sync_active_dyn = 20;
	params->dsi.horizontal_backporch_dyn = 20;
	params->dsi.horizontal_frontporch_dyn  = 110;

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9D;

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
	pr_debug("[LCM]hx83108 boe lcm_init_power !\n");
	tddi_lcm_tp_reset_output(LOW);
	lcm_reset_pin(LOW);
	ret = lcm_power_enable();
	MDELAY(5);
}

extern int wt_display_recover_flag;
extern bool himax_gesture_status;
static void lcm_suspend_power(void)
{
	if (himax_gesture_status == 1 && wt_display_recover_flag == 0) {
		pr_debug("[LCM] hx83108 boe lcd lcm_suspend_power himax_gesture_status = 1 wt_display_recover_flag=0!\n");
	} else {
		int ret  = 0;
		pr_debug("[LCM] hx83108 boe lcd lcm_suspend_power himax_gestrue_status = 0!\n");
		//lcm_reset_pin(LOW);
		MDELAY(2);
		ret = lcm_power_disable();
		MDELAY(10);
	}
}

static void lcm_resume_power(void)
{
	pr_debug("[LCM]hx83108 boe lcm_resume_power !\n");
	lcm_init_power();
}

extern void himax_common_resume(struct device *dev);
static void lcm_init(void)
{
	pr_debug("[LCM]hx83108 boe lcm_init\n");
	lcm_reset_pin(HIGH);
	MDELAY(6);
	lcm_reset_pin(LOW);
	MDELAY(3);
	tddi_lcm_tp_reset_output(HIGH);
	MDELAY(1);
	lcm_reset_pin(HIGH);
	MDELAY(50);
	push_table(NULL, r4f_setting_vdo, sizeof(r4f_setting_vdo) / sizeof(struct LCM_setting_table), 1);

	lcm_reset_pin(HIGH);
	MDELAY(6);
	lcm_reset_pin(LOW);
	MDELAY(3);
	lcm_reset_pin(HIGH);
	MDELAY(20);

	push_table(NULL, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table), 1);
	pr_debug("[LCM] hx83108 boe--init success ----\n");

	if(wt_display_recover_flag == 1){
		pr_debug("wt_display_recover_flag == 1 ;do himax_common_resume");
		himax_common_resume(NULL);
	}
}


extern void himax_common_suspend(struct device *dev);
static void lcm_suspend(void)
{
	pr_debug("[LCM] lcm_suspend\n");
	if(wt_display_recover_flag == 1){
		pr_debug("wt_display_recover_flag == 1 ;do himax_common_suspend");
		himax_common_suspend(NULL);
	}
	pr_debug("[FTS]tpd_ilitek_notifier_callback in lcm_suspend\n ");
	if (himax_gesture_status == 1 && wt_display_recover_flag == 0) {
		pr_debug("open gesture : lcm_suspend_setting1\n ");
		push_table(NULL, lcm_suspend_setting1, sizeof(lcm_suspend_setting1) / sizeof(struct LCM_setting_table), 1);
	} else {
		pr_debug("close gesture : lcm_suspend_setting2\n ");
		push_table(NULL, lcm_suspend_setting2, sizeof(lcm_suspend_setting2) / sizeof(struct LCM_setting_table), 1);
	}
	/* lcm_set_gpio_output(GPIO_LCM_RST,0); */
}

static void lcm_resume(void)
{
	pr_debug("[LCM] lcm_resume\n");
	lcm_init();
	//add for TP resume
	panel_notifier_call_chain(PANEL_PREPARE, NULL);
	pr_debug("[FTS]tpd_ilitek_notifier_callback in lcm_resume\n ");
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
	bl_lvl = wingtech_bright_to_bl(level,255,10,4095,32);
	pr_debug("%s,hx83108 boe backlight: level = %d,bl_lvl=%d\n", __func__, level,bl_lvl);
	// set 12bit
	bl_level[0].para_list[0] = (bl_lvl >> 8) & 0xF;
	bl_level[0].para_list[1] = bl_lvl & 0xFF;
	pr_debug("hx83108 boe backlight: para_list[0]=%x,para_list[1]=%x\n",bl_level[0].para_list[0],bl_level[0].para_list[1]);
	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER hx83108_dsi_vdo_hdp_boe_boe_drv = {
	.name = "hx83108_dsi_vdo_hdp_boe_boe",
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
