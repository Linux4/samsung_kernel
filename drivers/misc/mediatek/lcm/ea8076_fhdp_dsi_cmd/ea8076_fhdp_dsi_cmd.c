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
//#include <linux/delay.h>
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "data_hw_roundedpattern.h"
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
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID_EA8067 0x83
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
#define read_reg(cmd)	lcm_util.dsi_dcs_read_lcm_reg(cmd)
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
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif


#define FRAME_WIDTH			(1080)
#define FRAME_HEIGHT			(2340)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(64500)
#define LCM_PHYSICAL_HEIGHT		(139750)
#define LCM_DENSITY			(480)

#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE		0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


#define LDI_ID_REG	0x04
#define LDI_ID_LEN	3
#define LDI_RX_MAX	LDI_ID_LEN

int lcd_type;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/


/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_displayon_setting[] = {
	{0x29, 0, {} },
};

static struct LCM_setting_table lcm_displayoff_setting[] = {
	{0x28, 0, {} },
};

#if 0
static struct LCM_setting_table lcm_sleepout_setting[] = {
	{0x11, 0, {} },
};
#endif

static struct LCM_setting_table lcm_test_key_on_f0_setting[] = {
	{0xF0, 0x02, {0x5A, 0x5A} },
};

static struct LCM_setting_table lcm_test_key_off_f0_setting[] = {
	{0xF0, 0x02, {0xA5, 0xA5} },
};

static struct LCM_setting_table lcm_hbm_off_setting[] = {
	{0x53, 0x01, {0x20} },
};

static struct LCM_setting_table lcm_brightness_setting[] = {
	{0x51, 0x02, {0x01, 0xBD} },
};

static struct LCM_setting_table lcm_brightness_10_setting[] = {
	{0x51, 0x02, {0x00, 0x03} },
};

static struct LCM_setting_table lcm_brightness_20_setting[] = {
	{0x51, 0x01, {0x00, 0x30} },
};

static struct LCM_setting_table init_setting_cmd[] = {
	/* 8. Common Setting */
	/* 8.1.2 PAGE ADDRESS SET */
	//(id, SEQ_PAGE_ADDR_SETTING, ARRAY_SIZE(SEQ_PAGE_ADDR_SETTING));
	{0x2B, 0x04, {0x00, 0x00, 0x09, 0x23} },

	/* Testkey Enable */
	//(id, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	//(id, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	{0xF0, 0x02, {0x5A, 0x5A} },
	{0xFC, 0x02, {0x5A, 0x5A} },

	/* 8.1.3 FFC SET */
	//(id, SEQ_FFC_SET, ARRAY_SIZE(SEQ_FFC_SET) );
	{0xE9, 0x0B, {0x11, 0x55, 0x98, 0x96, 0x80, 0xB2, 0x41,
			0xC3, 0x00, 0x1A, 0xB8} },

	/* 8.1.4 ERR FG SET */
	//(id, SEQ_ERR_FG_SET, ARRAY_SIZE(SEQ_ERR_FG_SET));
	{0xE1, 0x0D, {0x00, 0x00, 0x02, 0x10, 0x10, 0x10, 0x00,
			0x00, 0x20, 0x00, 0x00, 0x01, 0x19} },
	//(id, SEQ_VSYNC_SET, ARRAY_SIZE(SEQ_VSYNC_SET));
	{0xE0, 0x01, {0x01} },
	//(id, SEQ_ASWIRE_OFF, ARRAY_SIZE(SEQ_ASWIRE_OFF));
	{0xD5, 0x0B, {0x83, 0xFF, 0x5C, 0x44, 0x89, 0x89, 0x00,
			0x00, 0x00, 0x00, 0x00} },

	//(id, SEQ_ACL_SETTING_1, ARRAY_SIZE(SEQ_ACL_SETTING_1));
	{0xB0, 0x01, {0xD7} },
	//(id, SEQ_ACL_SETTING_2, ARRAY_SIZE(SEQ_ACL_SETTING_2));
	{0xB9, 0x04, {0x02, 0xA1, 0x8C, 0x4B} },

	/* 10. Brightness Setting */
	//(id, SEQ_HBM_OFF, ARRAY_SIZE(SEQ_HBM_OFF));
	{0x53, 0x01, {0x20} },
	//(id, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
	{0xB7, 0x07, {0x01, 0x53, 0x28, 0x4D, 0x00, 0x90, 0x04} },
	//(id, SEQ_BRIGHTNESS, ARRAY_SIZE(SEQ_BRIGHTNESS));
	{0x51, 0x02, {0x01, 0xBD} },
	//(id, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
	{0x55, 0x01, {0x00} },

	/* Testkey Disable */
	//(id, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	//(id, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	{0xF0, 0x02, {0xA5, 0xA5} },
	{0xFC, 0x02, {0xA5, 0xA5} },

	/* 8.1.1 TE(Vsync) ON/OFF */
	//(id, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	//(id, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	//(id, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	{0xF0, 0x02, {0x5A, 0x5A} },
	{0x35, 0x02, {0x00, 0x00} },
	{0xF0, 0x02, {0xA5, 0xA5} },
};

static void push_table(void *cmdq, struct LCM_setting_table *table,
		       unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

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
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
					 table[i].para_list, force_update);
			break;
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
	params->physical_width = LCM_PHYSICAL_WIDTH / 1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	params->density = LCM_DENSITY;

	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
	LCM_LOGI("%s: lcm_dsi_mode %d\n", __func__, lcm_dsi_mode);
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

	params->dsi.vertical_sync_active = 1;
	params->dsi.vertical_backporch = 15;
	params->dsi.vertical_frontporch = 3;
	params->dsi.vertical_frontporch_for_low_power = 620;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 2;
	params->dsi.horizontal_backporch = 2;
	params->dsi.horizontal_frontporch = 2;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* this value must be in MTK suggested table */
	params->dsi.PLL_CLOCK = 480;
	params->dsi.PLL_CK_VDO = 440;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9d;


#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	params->round_corner_en = 1;
	params->corner_pattern_height = ROUND_CORNER_H_TOP;
	params->corner_pattern_height_bot = ROUND_CORNER_H_BOT;
	params->corner_pattern_tp_size = sizeof(top_rc_pattern);
	params->corner_pattern_lt_addr = (void *)top_rc_pattern;
#endif
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_init_power(void)
{
	//TBD
}

static void lcm_suspend_power(void)
{
	//TBD
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
	//TBD
}

int ea8076_read_id(void)
{
	unsigned int ret = 0;
	unsigned int data_array[3];
	unsigned char read_buf[3];

	data_array[0] = 0x00033700; /* set max return size = 3 */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(LDI_ID_REG, read_buf, LDI_ID_LEN); /* read lcm id */

	LCM_LOGI("read_id read = 0x%x, 0x%x, 0x%x\n",
		 read_buf[0], read_buf[1], read_buf[2]);

	lcd_type = read_buf[0]<<16 | read_buf[1]<<8 | read_buf[2];

	return ret;
}

static void lcm_init(void)
{
	/* Sleep Out(11h) */
	//(id, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	/* Wait 10ms */
	MDELAY(10);

	/* ID READ */
	ea8076_read_id();
	if (!lcd_type) {
		LCM_LOGI("%s: connected lcd invalid\n", __func__);
		return;
	}

	push_table(NULL, init_setting_cmd, ARRAY_SIZE(init_setting_cmd), 1);
	LCM_LOGI("ea8076 lcm mode = cmd mode :%d----\n", lcm_dsi_mode);

	/* 11. Wait 110ms */
	MDELAY(110);

	/* 12. Display On(29h) */
	//(dsim->id, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	push_table(NULL, lcm_displayon_setting,
		ARRAY_SIZE(lcm_displayon_setting), 1);
	MDELAY(20);
}

static void lcm_suspend(void)
{
	/* 2. Display Off (28h) */
	push_table(NULL, lcm_displayoff_setting,
		ARRAY_SIZE(lcm_displayoff_setting), 1);

	/* 3. Wait 20ms */
	MDELAY(20);
}

static void lcm_resume(void)
{
	lcm_init();
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
#ifndef BUILD_LK
	unsigned int ret = 0;
	unsigned int id[3] = {0x83, 0x11, 0x2B};
	unsigned int data_array[3];
	unsigned char read_buf[3];

	data_array[0] = 0x00033700; /* set max return size = 3 */
	dsi_set_cmdq(data_array, 1, 1);

	read_reg_v2(0x04, read_buf, 3); /* read lcm id */

	LCM_LOGI("ATA read = 0x%x, 0x%x, 0x%x\n",
		 read_buf[0], read_buf[1], read_buf[2]);

	if ((read_buf[0] == id[0]) &&
	    (read_buf[1] == id[1]) &&
	    (read_buf[2] == id[2]))
		ret = 1;
	else
		ret = 0;

	return ret;
#else
	return 0;
#endif
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	LCM_LOGI("%s,ea8076 backlight: level = %d\n", __func__, level);

	//(dsim->id, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	push_table(handle, lcm_test_key_on_f0_setting,
		ARRAY_SIZE(lcm_test_key_on_f0_setting), 1);

	//(dsim->id, SEQ_HBM_OFF, ARRAY_SIZE(SEQ_HBM_OFF));
	push_table(handle, lcm_test_key_off_f0_setting,
		ARRAY_SIZE(lcm_test_key_off_f0_setting), 1);

	if (level < 10)
		//(dsim->id, SEQ_BRIGHTNESS_10, ARRAY_SIZE(SEQ_BRIGHTNESS_10));
		push_table(handle, lcm_brightness_10_setting,
			ARRAY_SIZE(lcm_brightness_10_setting), 1);
	else if (level < 20)
		//(dsim->id, SEQ_BRIGHTNESS_20, ARRAY_SIZE(SEQ_BRIGHTNESS_20));
		push_table(handle, lcm_brightness_20_setting,
			ARRAY_SIZE(lcm_brightness_20_setting), 1);
	else
		//(dsim->id, SEQ_BRIGHTNESS, ARRAY_SIZE(SEQ_BRIGHTNESS));
		push_table(handle, lcm_brightness_setting,
			ARRAY_SIZE(lcm_brightness_setting), 1);

	//(dsim->id, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	push_table(handle, lcm_hbm_off_setting,
		ARRAY_SIZE(lcm_hbm_off_setting), 1);

}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width,
	unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

#ifdef LCM_SET_DISPLAY_ON_DELAY
	lcm_set_display_on();
#endif

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[1];
	unsigned int array[16];

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	array[0] = 0x00013700;  /* read id return 1byte */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x04, buffer, 3);
	id = buffer[0];     /* we only need ID */

	LCM_LOGI("%s,ea8076 id = 0x%08x\n", __func__, id);

	if (id == LCM_ID_EA8067)
		return 1;
	else
		return 0;

}
struct LCM_DRIVER ea8076_fhdp_dsi_cmd_lcm_drv = {
	.name = "ea8076_fhdp_dsi_cmd_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.compare_id = lcm_compare_id,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
};

