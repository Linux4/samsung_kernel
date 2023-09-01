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
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/kernel.h>

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#endif
#endif
#ifdef CONFIG_MTK_LEGACY
#include <cust_gpio_usage.h>
#endif
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
#include <cust_i2c.h>
#endif
#endif


#ifdef BUILD_LK
 #define LCD_DEBUG(fmt)  dprintf(CRITICAL, fmt)
#else
 #define LCD_DEBUG(fmt)  pr_debug(fmt)
#endif
/**
 *  Local Constants
 */
#define LCM_DSI_CMD_MODE	1
#define FRAME_WIDTH		(1080)
#define FRAME_HEIGHT	(2400)

#define LCM_PHYSICAL_WIDTH		(64500)
#define LCM_PHYSICAL_HEIGHT		(129000)
#define LCM_DENSITY			(480)

#define GPIO_LCD_EN_1P8				(GPIO25 | 0x80000000)
#define GPIO_LCD_EN_1P8_MODE		GPIO_MODE_00
#define GPIO_LCD_EN_3P0				(GPIO153 | 0x80000000)
#define GPIO_LCD_EN_3P0_MODE		GPIO_MODE_00

#ifndef TRUE
 #define TRUE 1
#endif

#ifndef FALSE
 #define FALSE 0
#endif

static bool hbm_en;
static bool hbm_wait;

/**
 *  Local Variables
 */
static struct LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

/**
 * Local Functions
 */
#define dsi_set_cmd_by_cmdq_dual(handle, cmd, count, ppara, force_update) \
		lcm_util.dsi_set_cmdq_V23(handle, cmd, (unsigned char)(count),\
					  (unsigned char *)(ppara), (unsigned char)(force_update))
#define dsi_set_cmdq_V3(para_tbl, size, force_update) \
			lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
			lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
			lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
			lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
			lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
/*DynFPS*/
#define dfps_dsi_send_cmd( \
			cmdq, cmd, count, para_list, force_update, sendmode) \
			lcm_util.dsi_dynfps_send_cmd( \
			cmdq, cmd, count, para_list, force_update, sendmode)


#define REGFLAG_DELAY		0xFE
#define REGFLAG_END_OF_TABLE	0xFF /* END OF REGISTERS MARKER */

struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[510];
};

static struct LCM_setting_table hbm_on[] = {
	{0xF0, 2, {0x5A, 0x5A} },
	{0xB0, 3, {0x00, 0x02, 0x90} },
	{0x90, 1, {0x14} },
	{0x53, 1, {0xE0} },
	{0x51, 2, {0x03, 0xFF} }, // 800nit
	{0xF7, 1, {0x0F} },
	{0xF0, 2, {0xA5, 0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table hbm_off[] = {
	{0xF0, 2, {0x5A, 0x5A} },
	{0xB0, 3, {0x00, 0x02, 0x90} },
	{0x90, 1, {0x1C} },
	{0x53, 1, {0x20} },
	{0x51, 2, {0x01, 0xC0} }, // 183nit
	{0xF7, 1, {0x0F} },
	{0xF0, 2, {0xA5, 0xA5} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0x9F, 2, {0xA5, 0xA5} },

	/* Sleep Out */
	{0x11, 0, {} },
	{REGFLAG_DELAY, 20, {} }, /* 20ms */

	/* Common Setting */
	/* 9.Global Para. Setting */
	{0xF0, 2, {0x5A, 0x5A} },
	{0xF2, 41, { 0x00, 0x05, 0x0E, 0x58, 0x50, 0x00, 0x0C, 0x00,
				0x04, 0x30, 0xB1, 0x30, 0xB1, 0x0C, 0x04, 0xBC,
				0x26, 0xE9, 0x0C, 0x00, 0x04, 0x10, 0x00, 0x10,
				0x26, 0xA8, 0x10, 0x00, 0x10, 0x10, 0x3C, 0x10,
				0x00, 0x40, 0x30, 0xC8, 0x00, 0xC8, 0x00, 0x00,
				0xCE} },
	{0xF7, 1, {0x0F} },
	{0xF0, 2, {0xA5, 0xA5} },

	/* 1.TE(Vsync) ON/OFF */
	{0xF0, 2, {0x5A, 0x5A} },
	{0x35, 1, {0x00} }, /* TE ON */
	{0xF0, 2, {0xA5, 0xA5} },

	/* 2.PAGE ADDRESS SET */
	{0x2A, 4, {0x00, 0x00, 0x04, 0x37} },
	{0x2B, 4, {0x00, 0x00, 0x09, 0x5F} },

	/* 4.ERR_FG Setting */
	{0xF0, 2, {0x5A, 0x5A} },
	{0xE5, 1, {0x05} },
	{0xED, 3, {0x44, 0x4C, 0x20} },
	{0xF0, 2, {0xA5, 0xA5} },

	/* 6.PCD Setting */
	{0xF0, 2, {0x5A, 0x5A} },
	{0xCD, 2, {0x5C, 0x51} },
	{0xF0, 2, {0xA5, 0xA5} },

	/* 7.ACL Setting */
	{0xF0, 2, {0x5A, 0x5A} },
	{0xB0, 3, {0x03, 0xB3, 0x65} },
	{0x65, 17, {0x55, 0x00, 0xB0, 0x51, 0x66, 0x98, 0x15, 0x55,
				0x55, 0x55, 0x08, 0xF1, 0xC6, 0x48, 0x40, 0x00,
				0x20} },
	{0xF7, 1, {0x0F} },
	{0xF0, 2, {0xA5, 0xA5} },

	/* 8.Freq. Setting */
	{0xF0, 2, {0x5A, 0x5A} },
	{0xB0, 3, {0x00, 0x27, 0xF2} },
	{0xF2, 1, {0x00} },
	{0xF7, 1, {0x0F} },
	{0xF0, 2, {0xA5, 0xA5} },

	/* Brightness Setting */
	/* 1.Max. & Dimming */
	{0xF0, 2, {0x5A, 0x5A} },
	{0x60, 3, {0x00, 0x00, 0x00} },
	{0x53, 1, {0x28} }, /* Dimming Control */
	{0x51, 2, {0x03, 0xFF} }, /* Luminance Setting */
	{0xF7, 1, {0x0F} },
	{0xF0, 2, {0xA5, 0xA5} },

	/* 2.HBM & Interpolation Dimming */
	{0xF0, 2, {0x5A, 0x5A} },
	{0x60, 3, {0x00, 0x00, 0x00} },
	{0x53, 1, {0xE0} }, /* Dimming Control */
	{0x51, 2, {0x03, 0xFF} }, /* Luminance Setting */
	{0xF7, 1, {0x0F} },
	{0xF0, 2, {0xA5, 0xA5} },

	/* 3.ACL ON/OFF */
	{0xF0, 2, {0x5A, 0x5A} },
	{0x55, 1, {0x03} },
	{0xF0, 2, {0xA5, 0xA5} },

	{REGFLAG_DELAY, 120, {} }, /* 120ms */

	{0x29, 0, {} }, /* Display On */

	/* Setting ending by predefined flag */
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x00, 0, {} },  /* NOP */
	{0x28, 0, {} },
	{REGFLAG_DELAY, 30, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 130, {} }
};

/*******************Dynfps start*************************/
#ifdef CONFIG_MTK_HIGH_FRAME_RATE

#define DFPS_MAX_CMD_NUM 10

struct LCM_dfps_cmd_table {
	bool need_send_cmd;
	enum LCM_Send_Cmd_Mode sendmode;
	struct LCM_setting_table prev_f_cmd[DFPS_MAX_CMD_NUM];
};

static struct LCM_dfps_cmd_table
	dfps_cmd_table[DFPS_LEVELNUM][DFPS_LEVELNUM] = {

	/**********level 0 to 0,1 cmd*********************/
	[DFPS_LEVEL0][DFPS_LEVEL0] = {
		false,
		LCM_SEND_IN_CMD,
		/*prev_frame cmd*/
		{
		{REGFLAG_END_OF_TABLE, 0x00, {} },
		},
	},
	/*60->90*/
	[DFPS_LEVEL0][DFPS_LEVEL1] = {
		true,
		LCM_SEND_IN_CMD,
		/*prev_frame cmd*/
		{
			{0xF0, 2, {0x5A, 0x5A} },
			{0x60, 3, {0x08, 0x00, 0x00} },
			{0xF7, 1, {0x0F} },
			{0xF0, 2, {0x5A, 0x5A} },
			{REGFLAG_END_OF_TABLE, 0x00, {} },
		},
	},

	/**********level 1 to 0,1 cmd*********************/
	/*90->60*/
	[DFPS_LEVEL1][DFPS_LEVEL0] = {
		true,
		LCM_SEND_IN_CMD,
		/*prev_frame cmd*/
		{
			{0xF0, 2, {0x5A, 0x5A} },
			{0x60, 3, {0x00, 0x00, 0x00} },
			{0xF7, 1, {0x0F} },
			{0xF0, 2, {0x5A, 0x5A} },
			{REGFLAG_END_OF_TABLE, 0x00, {} },
		},
	},

	[DFPS_LEVEL1][DFPS_LEVEL1] = {
		false,
		LCM_SEND_IN_CMD,
		/*prev_frame cmd*/
		{
			{REGFLAG_END_OF_TABLE, 0x00, {} },
		},

	},
};
#endif
/*******************Dynfps end*************************/


/**
 *  LCM Driver Implementations
 */
static void push_table(void *cmdq, struct LCM_setting_table *table,
				unsigned int count, unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
#ifdef BUILD_LK
			dprintf(0, "[LK]REGFLAG_DELAY\n");
#endif
			MDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
#ifdef BUILD_LK
			dprintf(0, "[LK]push_table end\n");
#endif
			break;
		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
				table[i].para_list, force_update);
		}
	}
}

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
static void lcm_dfps_int(struct LCM_DSI_PARAMS *dsi)
{
	struct dfps_info *dfps_params = dsi->dfps_params;

	dsi->dfps_enable = 1;
	dsi->dfps_default_fps = 6000;/*real fps * 100, to support float*/
	dsi->dfps_def_vact_tim_fps = 9000;/*real vact timing fps * 100*/

	/* traversing array must less than DFPS_LEVELS */
	/* DPFS_LEVEL0 */
	dfps_params[0].level = DFPS_LEVEL0;
	dfps_params[0].fps = 6000;/*real fps * 100, to support float*/
	dfps_params[0].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	dfps_params[0].PLL_CLOCK = 814;
	/* dfps_params[0].data_rate = xx; */

	/* DPFS_LEVEL1 */
	dfps_params[1].level = DFPS_LEVEL1;
	dfps_params[1].fps = 9000;/*real fps * 100, to support float*/
	dfps_params[1].vact_timing_fps = 9000;/*real vact timing fps * 100*/
	/* if mipi clock solution */
	dfps_params[1].PLL_CLOCK = 814;
	/* dfps_params[1].data_rate = xx; */

	dsi->dfps_num = 2;
}
#endif

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;

	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;

	params->lcm_if = LCM_INTERFACE_DSI0;
	params->lcm_cmd_if = LCM_INTERFACE_DSI0;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	lcm_dsi_mode = CMD_MODE;
#else
	params->dsi.mode = BURST_VDO_MODE;
#endif

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM		= LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	/* Video mode setting */
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.packet_size = 256;
	params->dsi.ssc_disable = 1;
	/* params->dsi.ssc_range = 3; */

	params->dsi.vertical_sync_active	= 2;
	params->dsi.vertical_backporch		= 12;
	params->dsi.vertical_frontporch		= 5;
	params->dsi.vertical_active_line	= FRAME_HEIGHT;

	params->dsi.horizontal_sync_active	= 10;
	params->dsi.horizontal_backporch	  = 14;
	params->dsi.horizontal_frontporch	  = 30;
	params->dsi.horizontal_active_pixel	= FRAME_WIDTH;

	params->dsi.cont_clock = 1;

	/* Bit rate calculation */
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	params->dsi.PLL_CLOCK = 814;
#else
	params->dsi.PLL_CLOCK = 489;
#endif

#ifndef BUILD_LK
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;/*Donglei*/
	params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
#endif

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/****DynFPS start****/
	lcm_dfps_int(&(params->dsi));
	/****DynFPS end****/
#endif
	/* HBM: High Backlight Mode */
	params->hbm_en_time = 2;
	params->hbm_dis_time = 0;
}

static void lcm_init(void)
{
	/* System Power ON */
	/* VBAT-VDDI(1.8v)-VCI(3v) */
	MDELAY(20);

	SET_RESET_PIN(1);
	MDELAY(5);

	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	push_table(NULL, lcm_initialization_setting,
			ARRAY_SIZE(lcm_initialization_setting), 1);

#ifdef BUILD_LK
	dprintf(0, "[LK]push_table end\n");
#endif
	hbm_en = false;
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting, ARRAY_SIZE(lcm_suspend_setting), 1);
	/*SET_RESET_PIN(0);*/
	hbm_en = false;
}

static void lcm_resume(void)
{
	lcm_init();
}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
		       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);

}
#endif

#define LCM_ID_S6E3FC3_ID (0x800000) /* ID1:0x80 ID2: 0x00 ID3:0x00 */

static unsigned int lcm_compare_id(void)
{
	unsigned char buffer[3] = {0};
	unsigned int array[16];
	unsigned int lcd_id = 0;

	SET_RESET_PIN(1);
	MDELAY(20);
	SET_RESET_PIN(0);
	MDELAY(20);
	SET_RESET_PIN(1);
	MDELAY(150);

	array[0] = 0x00033700; /*read id return two byte, version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x04, buffer, 3);
	MDELAY(20);
	lcd_id = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];

#ifdef BUILD_LK
	dprintf(0, "%s, LK s6e3ha3 debug: s6e3fc3 id = 0x%08x\n", __func__, lcd_id);
#else
	pr_debug("%s, kernel s6e3ha3 horse debug: s6e3fc3 id = 0x%08x\n", __func__, lcd_id);
#endif

	if (buffer[0] != 0)
		return 1;
	else
		return 0;

}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
#if 0
	/* Refresh value of backlight level. */
	unsigned int cmd_1 = 0x51;
	unsigned int count_1 = 1;
	unsigned int value_1 = level;
#if (LCM_DSI_CMD_MODE) /* NOP */
	unsigned int cmd = 0x00;
	unsigned int count = 0;
	unsigned int value = 0x00;

	dsi_set_cmd_by_cmdq_dual(handle, cmd, count, &value, 1);
#endif

#ifdef BUILD_LK
	dprintf(0, "%s,lk s6e3fc3 brightness: level = %d\n", __func__, level);
#else
	pr_debug("%s, kernel s6e3fc3 brightness: level = %d\n", __func__, level);
#endif

	dsi_set_cmd_by_cmdq_dual(handle, cmd_1, count_1, &value_1, 1);
#endif
	pr_info("%s, [warning] ignore kernel s6e3fc3 brightness: level = %d\n", __func__, level);

	return;
}

/*******************Dynfps start*************************/
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
static void dfps_dsi_push_table(
	void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update,
	enum LCM_Send_Cmd_Mode sendmode)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_END_OF_TABLE:
			return;
		default:
			dfps_dsi_send_cmd(
				cmdq, cmd, table[i].count,
				table[i].para_list, force_update, sendmode);
			break;
		}
	}

}
static bool lcm_dfps_need_inform_lcm(
	unsigned int from_level, unsigned int to_level,
	struct LCM_PARAMS *params)
{
	struct LCM_dfps_cmd_table *p_dfps_cmds = NULL;

	if (from_level == to_level) {
		pr_debug("%s,same level\n", __func__);
		return false;
	}
	p_dfps_cmds =
		&(dfps_cmd_table[from_level][to_level]);
	params->sendmode = p_dfps_cmds->sendmode;

	return p_dfps_cmds->need_send_cmd;
}

static void lcm_dfps_inform_lcm(void *cmdq_handle,
unsigned int from_level, unsigned int to_level,
struct LCM_PARAMS *params)
{
	struct LCM_dfps_cmd_table *p_dfps_cmds = NULL;
	enum LCM_Send_Cmd_Mode sendmode = LCM_SEND_IN_CMD;

	if (from_level == to_level)
		goto done;

	p_dfps_cmds =
		&(dfps_cmd_table[from_level][to_level]);

	if (p_dfps_cmds &&
		!(p_dfps_cmds->need_send_cmd))
		goto done;

	sendmode = params->sendmode;

	dfps_dsi_push_table(
		cmdq_handle, p_dfps_cmds->prev_f_cmd,
		ARRAY_SIZE(p_dfps_cmds->prev_f_cmd), 1, sendmode);
done:
	pr_debug("%s,done %d->%d\n",
		__func__, from_level, to_level, sendmode);

}
#endif
/*******************Dynfps end*************************/

static bool lcm_get_hbm_state(void)
{
	return hbm_en;
}

static bool lcm_get_hbm_wait(void)
{
	return hbm_wait;
}

static bool lcm_set_hbm_wait(bool wait)
{
	bool old = hbm_wait;

	hbm_wait = wait;
	return old;
}

static bool lcm_set_hbm_cmdq(bool en, void *qhandle)
{
	bool old = hbm_en;

	if (hbm_en == en)
		goto done;

	if (en)
		push_table(NULL, hbm_on, ARRAY_SIZE(hbm_on), 1);
	else
		push_table(NULL, hbm_off, ARRAY_SIZE(hbm_off), 1);

	hbm_en = en;
	lcm_set_hbm_wait(true);

done:
	return old;
}

struct LCM_DRIVER s6e3fc3_fhdp_cmd_a22_lte_lcm_drv = {
	.name				= "s6e3fc3_fhdp_cmd_a22_lte",
	.set_util_funcs			= lcm_set_util_funcs,
	.get_params			= lcm_get_params,
	.init				= lcm_init,
	.suspend			= lcm_suspend,
	.resume				= lcm_resume,
	.set_backlight_cmdq		= lcm_setbacklight_cmdq,
	.compare_id			= lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
	.update				= lcm_update,
#endif
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/*DynFPS*/
	.dfps_send_lcm_cmd = lcm_dfps_inform_lcm,
	.dfps_need_send_cmd = lcm_dfps_need_inform_lcm,
#endif
	.get_hbm_state = lcm_get_hbm_state,
	.set_hbm_cmdq = lcm_set_hbm_cmdq,
	.get_hbm_wait = lcm_get_hbm_wait,
	.set_hbm_wait = lcm_set_hbm_wait,
};
