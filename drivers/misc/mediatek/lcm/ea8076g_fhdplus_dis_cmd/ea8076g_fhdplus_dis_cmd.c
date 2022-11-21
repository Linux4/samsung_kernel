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
#define FRAME_HEIGHT			(2340)      // 2220
//#define FRAME_HEIGHT        (2220)

#define LCD_EN_1P8 (GPIO25 | 0x80000000)
#define LCD_EN_3P0 (GPIO153 | 0x80000000)
#define OLED_RST_N (GPIO160 | 0x80000000)
#define TSP_LDO_EN (GPIO165 | 0x80000000)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH		(64500)
#define LCM_PHYSICAL_HEIGHT		(129000)
#define LCM_DENSITY			(480)

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

static struct LCM_setting_table display_on_setting_cmd[] = {
	{0x29, 0x00, {} },
	{REGFLAG_DELAY, 20, {} },
};

static struct LCM_setting_table
__maybe_unused display_off_setting_cmd[] = {
	{0x28, 0x00, {} },
	{REGFLAG_DELAY, 20, {} },
};

static struct LCM_setting_table init_setting_cmd[] = {
	/* 8.1.2 PAGE ADDRESS SET : SEQ_PAGE_ADDR_SETTING */
	{0x2B, 0x04, {0x00, 0x00, 0x09, 0x23} },
	/* Testkey Enable */
	{0xF0, 0x02, {0x5A, 0x5A} },
	{0xFC, 0x02, {0x5A, 0x5A} },

	/* 8.1.3 FFC SET : SEQ_FFC_SET */
	{0xE9, 0x0B, {0x11, 0x55, 0x98, 0x96, 0x80, 0xB2, 0x41, 0xC3, 0x00, 0x1A,
				0xB8} },

	/* 8.1.4 ERR FG SET : SEQ_ERR_FG_SET */
	{0xE1, 0x0D, {0x00, 0x00, 0x02, 0x10, 0x10, 0x10, 0x00, 0x00, 0x20, 0x00,
				0x00, 0x01, 0x19 } },
	/* SEQ_VSYNC_SET */
	{0xE0, 0x01, {0x01} },
	/* SEQ_ASWIRE_OFF */
	{0xD5, 0x0B, {0x83, 0xFF, 0x5C, 0x44, 0x89, 0x89, 0x00, 0x00, 0x00, 0x00,
				0x00} },
	/* SEQ_ACL_SETTING_1 */
	{0xB0, 0x01, {0xD7} },
	/* SEQ_ACL_SETTING_2 */
	{0xB9, 0x04, {0x02, 0xA1, 0x8C, 0x4B} },

	/* 10. Brightness Setting : SEQ_HBM_OFF*/
	{0x53, 0x01, {0x20} },
	/* SEQ_ELVSS_SET */
	{0xB7, 0x07, {0x01, 0x53, 0x28, 0x4D, 0x00, 0x90, 0x04} },
	/* SEQ_BRIGHTNESS */
	{0x51, 0x02, {0x01, 0xBD} },
	/* SEQ_ACL_OFF */
	{0x55, 0x01, {0x00} },

	/* Testkey Disable */
	/* SEQ_TEST_KEY_OFF_F0 */
	{0xF0, 0x02, {0xA5, 0xA5} },
	/* SEQ_TEST_KEY_OFF_FC */
	{0xFC, 0x02, {0xA5, 0xA5} },

	/* 8.1.1 TE(Vsync) ON/OFF */
	/* SEQ_TEST_KEY_ON_F0 */
	{0xF0, 0x02, {0x5A, 0x5A} },
	/* SEQ_TE_ON */
	{0x35, 0x02, {0x00, 0x00} },
	/* SEQ_TEST_KEY_OFF_F0 */
	{0xF0, 0x02, {0xA5, 0xA5} },

	{REGFLAG_DELAY, 110, {} },
};

static struct LCM_setting_table
__maybe_unused sleep_mode_in_setting_cmd[] = {
	{0x10, 0x01, {0x00} },
	{REGFLAG_DELAY, 150, {} },
};

static struct LCM_setting_table
__maybe_unused sleep_out_setting_cmd[] = {
	{0x11, 0x01, {0x00} },
	{REGFLAG_DELAY, 120, {} },
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 1, {0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

#if 0
static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output,
			unsigned int pullen, unsigned int pull, unsigned int smt)
{
	mt_set_gpio_mode(GPIO, GPIO_MODE_00);
	mt_set_gpio_dir(GPIO, GPIO_DIR_OUT);
	mt_set_gpio_pull_enable(GPIO, pullen);
	mt_set_gpio_pull_select(GPIO, pull);
	mt_set_gpio_out(GPIO, (output > 0) ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
	mt_set_gpio_smt(GPIO, smt);
}
#endif /* 0 */

static void push_table(struct LCM_setting_table *table,
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
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
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
	params->dsi.PLL_CLOCK = 497;    //497;    //472
	params->dsi.PLL_CK_VDO = 440;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.CLK_HS_POST = 36;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9d;

}

/* turn on gate ic & control voltage to 5.5V */
/* equle display_bais_enable ,mt6768 need +/-5.5V */
static void lcm_init_power(void)
{
}

static void lcm_suspend_power(void)
{
}

/* turn on gate ic & control voltage to 5.5V */
static void lcm_resume_power(void)
{
}

//#define LCM_ID_READ

#ifdef LCM_ID_READ
#define LDI_ID_REG	0x04
#define LDI_ID_LEN	3
#define LDI_RX_MAX	LDI_ID_LEN

int g_lcd_type;

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

	g_lcd_type = read_buf[0]<<16 | read_buf[1]<<8 | read_buf[2];

	return ret;
}
#endif /* LCM_ID_READ */

#if 0
static void lcm_power_on(void)
{
    // OLED_RST_N (GPIO160)
	lcm_set_gpio_output(OLED_RST_N, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);

    // LCD_EN_1P8 (GPIO25) -> VDD_LCD_1P8    
	lcm_set_gpio_output(LCD_EN_1P8, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
    // LCD_EN_3P0 (GPIO153) -> VDD_LCD_3P0
	lcm_set_gpio_output(LCD_EN_3P0, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);
    // TSP_LDO_EN (GPIO165) -> TSP_AVDD_3P0
	lcm_set_gpio_output(TSP_LDO_EN, GPIO_OUT_ONE, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);

    // OLED_RST_N (GPIO160)
	lcm_set_gpio_output(OLED_RST_N, GPIO_OUT_ZERO, GPIO_PULL_ENABLE, GPIO_PULL_DOWN, GPIO_SMT_DISABLE);

    return;
}
#endif /* 0 */

static void lcm_init(void)
{
#if 0
	// ldo pin enable and reset
	lcm_power_on();
#endif /* 0 */

	/* 6. Sleep Out(11h) */
	push_table(sleep_out_setting_cmd, ARRAY_SIZE(sleep_out_setting_cmd), 1);

	/* 7. Wait 10ms */
	UDELAY(10000);

	/* ID READ */
	#ifdef LCM_ID_READ
	ea8076_read_id();
	if (!g_lcd_type) {		
		LCM_LOGI("%s: connected lcd invalid\n", __func__);
		return;
	}
	#endif /* LCM_ID_READ */

	/* 8. Common Setting */
	push_table(init_setting_cmd, ARRAY_SIZE(init_setting_cmd), 1);
	LCM_LOGI("lcm dis mode :%d----\n", lcm_dsi_mode);

	/* 11. Wait 110ms */
	MDELAY(110);

#ifndef LCM_SET_DISPLAY_ON_DELAY
	/* display on */
	push_table(display_on_setting_cmd, ARRAY_SIZE(display_on_setting_cmd), 1);
#endif

}

#ifdef LCM_SET_DISPLAY_ON_DELAY
static int lcm_set_display_on(void)
{
	push_table(display_on_setting_cmd, ARRAY_SIZE(display_on_setting_cmd),
		1);
	return 0;
}
#endif

static void lcm_suspend(void)
{
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

#if 0
static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	LCM_LOGI("%s,hx83112b backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	push_table(bl_level, ARRAY_SIZE(bl_level), 1);
}
#endif /* 0 */

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

#ifndef LCM_SET_DISPLAY_ON_DELAY
	lcm_set_display_on();
#endif

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

/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int x1, w;

	x1 = 0;
	w = FRAME_WIDTH;

	*x = x1;
	*width = w;
}

static struct LCM_setting_table seq_test_key_on_f0[] = {
	{ 0xF0, 0x02, {0x5A, 0x5A} },
};

static struct LCM_setting_table seq_hbm_off[] = {
	{0x53, 0x01, {0x20} },
};

static struct LCM_setting_table seq_brightness_10[] = {
	{0x51, 0x02, {0x00, 0x03} },
};

static struct LCM_setting_table seq_brightness_20[] = {
	{0x51, 0x02, {0x00, 0x30} },
};


static struct LCM_setting_table seq_brightness[] = {
	{0x51, 0x02, {0x01, 0xBD} },
};

static struct LCM_setting_table seq_test_key_off_f0[] = {
	{0xF0, 0x02, {0xA5, 0xA5} },
};

static void lcm_setbacklight(void *handle, unsigned int level)
{
	LCM_LOGI("%s, backlight: level = %d\n", __func__, level);

	bl_level[0].para_list[0] = level;

	/* SEQ_TEST_KEY_ON_F0 */
	push_table(seq_test_key_on_f0, ARRAY_SIZE(seq_test_key_on_f0), 1);
	/* SEQ_HBM_OFF */
	push_table(seq_hbm_off, ARRAY_SIZE(seq_hbm_off), 1);

	if (level < 10)
		push_table(seq_brightness_10, ARRAY_SIZE(seq_brightness_10), 1);	/* SEQ_BRIGHTNESS_10 */
	else if (level < 20)
		push_table(seq_brightness_20, ARRAY_SIZE(seq_brightness_20), 1);	/* SEQ_BRIGHTNESS_20 */
	else
		push_table(seq_brightness, ARRAY_SIZE(seq_brightness), 1);	/* SEQ_BRIGHTNESS */

	/* SEQ_TEST_KEY_OFF_F0 */
	push_table(seq_test_key_off_f0, ARRAY_SIZE(seq_test_key_off_f0), 1);

}

static int lcm_read_by_cmdq(void *handle, unsigned int data_id,
	unsigned int offset, unsigned int cmd, unsigned char *buffer,
	unsigned char size)
{
	return lcm_util.dsi_dcs_read_cmdq_lcm_reg_v2_1(handle, data_id, offset,
		cmd, buffer, size);
}

struct LCM_DRIVER ea8076g_fhdplus_dis_cmd_lcm_drv = {
	.name = "ea8076g_fhdplus_dis_cmd_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.validate_roi = lcm_validate_roi,
	.read_by_cmdq = lcm_read_by_cmdq,
	.set_display_on = lcm_set_display_on,
};

