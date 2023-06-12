/*
 * Copyright (C) 2008 MediaTek Inc.
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

#ifdef BUILD_LK
#include <string.h>
#include <mt_gpio.h>
#include <platform/mt_pmic.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "upmu_common.h"
#include <linux/string.h>
#include "mt_gpio.h"
#include <mach/gpio_const.h>

#endif

#include "lcm_drv.h"
#if defined(BUILD_LK)
#define LCM_PRINT printf
#elif defined(BUILD_UBOOT)
#define LCM_PRINT printf
#else
#define LCM_PRINT printk
#endif
#if defined(BUILD_LK)
#include <boot_mode.h>
#else
#include <upmu_hw.h>
#include <linux/types.h>
#include "sm5107.h"
#endif

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
/* pixel */
#define FRAME_WIDTH			(720)
#define FRAME_HEIGHT			(1280)


/* physical dimension */
#define PHYSICAL_WIDTH        (66)
#define PHYSICAL_HEIGHT         (117)

#define LCM_ID       (0xb9)
#define LCM_DSI_CMD_MODE		0

#define REGFLAG_DELAY 0xAB
#define REGFLAG_END_OF_TABLE 0xAA	/* END OF REGISTERS MARKER */

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

static LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define UDELAY(n)								(lcm_util.udelay(n))
#define MDELAY(n)								(lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

#define dsi_set_cmdq_V3(para_tbl, size, force_update)\
lcm_util.dsi_set_cmdq_V3(para_tbl, size, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)\
lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)\
lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)\
lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)\
lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)\
lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)\
lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

static unsigned int need_set_lcm_addr = 1;

#ifndef GPIO_LCM_RST
#define GPIO_LCM_RST         (GPIO146 | 0x80000000)
#define GPIO_LCM_RST_M_GPIO   GPIO_MODE_00
#define GPIO_LCM_RST_M_LCM_RST   GPIO_MODE_01
#endif

#ifndef GPIO_TOUCH_RESET
#define GPIO_TOUCH_RESET         (GPIO62 | 0x80000000)
#define GPIO_TOUCH_RESET_M_GPIO   GPIO_MODE_00
#define GPIO_TOUCH_RESET_M_LCM_RST   GPIO_MODE_01
#endif

#define LGE_LPWG_SUPPORT

struct LCM_setting_table {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static LCM_setting_table_V3 lcm_initialization_setting_V3[] = {
	{0x15, 0x36, 1, {0x00} },	/* set address mode */
	{0x15, 0xB0, 1, {0x00} },	/* Manufacturer Cmd Access Protect */
	{0x29, 0xB1, 2, {0x00, 0x46} },	/* RGB interface Set */
	{0x29, 0xB2, 6, {0x50, 0x00, 0x0C, 0x00, 0x00, 0x00} },	/* Module Characteristics Set */
	{0x29, 0XB3, 4, {0x4F, 0x80, 0x0A, 0x28} },	/* Internal Timing Set */
	{0x29, 0XB4, 6, {0x05, 0x0C, 0x1F, 0x40, 0x11, 0x00} },	/* Channel Control */
	{0x29, 0XB5, 51,
	{0x19, 0x04, 0x1C, 0x22, 0x2F, 0x80, 0x2F, 0x1F, 0x04, 0x40, 0x05, 0x00, 0x20, 0x09, 0x04,
	 0x40,
	 0x05, 0x00, 0xA5, 0x0D, 0x2F, 0x2F, 0x3F, 0xFF, 0x3F, 0xFF, 0x01, 0x7C, 0x00, 0x02, 0x50,
	 0x23, 0x40, 0x15, 0x6C, 0xCB,
	 0xBA, 0xA9, 0x97, 0x8D, 0x1F, 0x23, 0x40, 0x15,
		0x6C, 0xCB, 0xBA, 0xA9, 0x97, 0x8D, 0x1F} },	/* GIP Control Set */
	{0x29, 0XB6, 2, {0x01, 0x01} },	/* Touch Enable */
	{0x29, 0XB7, 3, {0x00, 0x32, 0x33} },	/* GVDDP control */
	{0x29, 0XBB, 15, {0x00, 0x90, 0x90, 0x30, 0x30, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13} },	/* VGHL Control */
	{0x29, 0XBD, 2, {0x03, 0x33} },	/* Bias control */
	{0x15, 0XBE, 1, {0x03} },	/* VCI1 Control */
	{0x29, 0X85, 25,
	 {0x1F, 0x6E, 0x22, 0x5A, 0x19, 0x44, 0x28, 0x7F, 0x4D, 0xAD, 0x85, 0x6A, 0x96, 0x9B, 0x8C,
	  0x65,
	  0x51, 0x97, 0x60, 0xB0, 0x9C, 0x65, 0x51, 0xA5, 0x62} },	/* Color Enhance Control */
	{0x29, 0X95, 6, {0x00, 0x08, 0x10, 0x00, 0x00, 0x00} },	/* Write DSI Control */
	{0x29, 0xC6, 2, {0x0A, 0x00} },	/* MIPI Parameter Setting */
	{0x15, 0xCF, 1, {0x07} },	/* Touch Control Set */
	{0x29, 0xD7, 26,
	 {0x00, 0x13, 0xFF, 0x39, 0x0B, 0x04, 0x14, 0xF4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x40, 0x01,
	  0x57,
	  0x57, 0x2F, 0x28, 0x28, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00} },	/* Touch GIP control */
	{0x29, 0XF5, 5, {0x00, 0x06, 0x00, 0x00, 0x80} },	/* Touch LFD control */
	{0x15, 0XF6, 1, {0x06} },	/* VDD1 Set */
	{0x29, 0xF0, 35,
	{0x18, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x2B, 0x01, 0x02, 0x53, 0x58,
		0x5D,
		0x62, 0xA6, 0xAB, 0xB0, 0xB0, 0xB0, 0xAF, 0xB0, 0xAD,
		0x85, 0xB0, 0xB0, 0x5D, 0x58, 0x53, 0x4E, 0x0A, 0x05, 0x00} },	/* Power Sequence control */
	{0x29, 0xD0, 9, {0x62, 0x67, 0x54, 0x44, 0x01, 0x04, 0x60, 0x24, 0x03} },	/* Gamma */
	{0x29, 0xD0, 4, {0x62, 0x67, 0x54, 0x44} },
	{0x29, 0xD1, 9, {0x62, 0x67, 0x54, 0x44, 0x01, 0x06, 0x60, 0x24, 0x03} },
	{0x29, 0xD1, 4, {0x62, 0x67, 0x54, 0x44} },
	{0x29, 0xD2, 9, {0x62, 0x67, 0x54, 0x44, 0x01, 0x04, 0x60, 0x24, 0x03} },
	{0x29, 0xD2, 4, {0x62, 0x67, 0x54, 0x44} },
	{0x29, 0xD3, 9, {0x62, 0x67, 0x54, 0x44, 0x01, 0x06, 0x60, 0x24, 0x03} },
	{0x29, 0xD3, 4, {0x62, 0x67, 0x54, 0x44} },
	{0x29, 0xD4, 9, {0x62, 0x67, 0x54, 0x44, 0x01, 0x04, 0x60, 0x24, 0x03} },
	{0x29, 0xD4, 4, {0x62, 0x67, 0x54, 0x44} },
	{0x29, 0xD5, 9, {0x62, 0x67, 0x54, 0x44, 0x01, 0x06, 0x60, 0x24, 0x03} },
	{0x29, 0xD5, 4, {0x62, 0x67, 0x54, 0x44} },
	{0x05, 0x11, 1, {0x00} },
/* {REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 120,{}}, */
/* {0x05, 0x29,  1, {0x00}}, */
	{REGFLAG_END_OF_TABLE, 0x00, 1, {} },
};

static LCM_setting_table_V3 lcm_initialization_setting_V3_Disp_On[] = {
	{0x05, 0x29, 1, {0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, 1, {} },
};

static LCM_setting_table_V3 lcm_initialization_for_sleep_in_pre[] = {
	{0x05, 0x28, 1, {0x00} },
	{REGFLAG_ESCAPE_ID, REGFLAG_DELAY_MS_V3, 60, {} },
	{0x05, 0x10, 1, {0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, 1, {} },
};

static LCM_setting_table_V3 lcm_initialization_for_sleep_in_post[] = {
	{0x39, 0XB5, 51, {0x19, 0x04, 0x1C, 0x22, 0x2F, 0x80, 0x2F, 0x1F, 0x04, 0x40, 0x05, 0x00,
			  0x20, 0x09, 0x04, 0x40, 0x05, 0x00, 0xA5, 0x0D, 0x2F, 0x1F, 0x04, 0x00,
			  0x04, 0x00, 0x00, 0x7C, 0x00, 0x02, 0x50, 0x23, 0x40, 0x15, 0x6C, 0xCB,
			  0xBA, 0xA9, 0x97, 0x8D, 0x1F, 0x23, 0x40, 0x15, 0x6C, 0xCB, 0xBA, 0xA9,
			  0x97, 0x8D, 0x1F} },
	{0x39, 0XF5, 5, {0x00, 0x06, 0x00, 0x00, 0x80} },
	{0x39, 0XC8, 2, {0x01, 0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, 1, {} },
};

static struct LCM_setting_table __attribute__ ((unused)) lcm_backlight_level_setting[] = {
	{
		0x51, 1, {
	0xFF} }, {
		REGFLAG_END_OF_TABLE, 0x00, {
	} }
};

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy((void *)&lcm_util, (void *)util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	/* physical size */
	params->physical_width = PHYSICAL_WIDTH;
	params->physical_height = PHYSICAL_HEIGHT;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;	/* SYNC_EVENT_VDO_MODE; //BURST_VDO_MODE;//SYNC_PULSE_VDO_MODE; */
	/* enable tearing-free */
	params->dbi.te_mode = LCM_DBI_TE_MODE_DISABLED;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;

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
	/* params->dsi.intermediat_buffer_num = 0; */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.cont_clock = 0;

	params->dsi.vertical_sync_active = 2;	/* 20150824 , 1->2 */
	params->dsi.vertical_backporch = 11;
	params->dsi.vertical_frontporch = 580;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 4;
	params->dsi.horizontal_backporch = 120;
	params->dsi.horizontal_frontporch = 72;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.HS_TRAIL = 10;
	params->dsi.HS_ZERO = 20;
	params->dsi.LPX = 20;
	params->dsi.HS_PRPR = 8;

	params->dsi.PLL_CLOCK = 338;	/* 312; // 234; */

}

static void init_lcm_registers(void)
{
	dsi_set_cmdq_V3(lcm_initialization_setting_V3,
			sizeof(lcm_initialization_setting_V3) / sizeof(LCM_setting_table_V3), 1);
	MDELAY(120);
	dsi_set_cmdq_V3(lcm_initialization_setting_V3_Disp_On,
			sizeof(lcm_initialization_setting_V3_Disp_On) /
			sizeof(LCM_setting_table_V3), 1);
	LCM_PRINT("[LCD] init_lcm_registers\n");
}

static void init_lcm_registers_sleep(void)
{
	dsi_set_cmdq_V3(lcm_initialization_for_sleep_in_pre,
			sizeof(lcm_initialization_for_sleep_in_pre) / sizeof(LCM_setting_table_V3),
			1);
	LCM_PRINT("[LCD] init_lcm_registers_sleep\n");
}

void init_lcm_registers_sleep_2nd(void)
{
	dsi_set_cmdq_V3(lcm_initialization_for_sleep_in_post,
			sizeof(lcm_initialization_for_sleep_in_post) / sizeof(LCM_setting_table_V3),
			1);
	LCM_PRINT("[LCD] init_lcm_registers_sleep 2nd\n");
}

/* 1.8v LDO is always on */
static void ldo_1v8io_on(void)
{
#if 0
	mt6328_set_register_value(PMIC_RG_VRF18_1_VOSEL, 3);
	mt6328_set_register_value(PMIC_RG_VRF18_1_EN, 1);
#else
	hwPowerOn(MT6328_POWER_LDO_VRF18_1, VOL_1800, "LCD");
#endif
}

/* 1.8v LDO is always on */
static void ldo_1v8io_off(void)
{
#if 0
	mt6328_set_register_value(PMIC_RG_VRF18_1_EN, 0);
#else
	hwPowerDown(MT6328_POWER_LDO_VRF18_1, "LCD");
#endif
}

/* VGP1 3.0v LDO enable */
static void ldo_3v0_on(void)
{
#if 0
	mt6328_set_register_value(PMIC_RG_VGP1_VOSEL, 6);
	mt6328_set_register_value(PMIC_RG_VGP1_EN, 1);
#else
	hwPowerOn(MT6328_POWER_LDO_VGP1, VOL_3000, "LCD");
#endif
}

/* VGP1 3.0v LDO disable */
static void ldo_3v0_off(void)
{
#if 0
	mt6328_set_register_value(PMIC_RG_VGP1_EN, 0);
#else
	hwPowerDown(MT6328_POWER_LDO_VGP1, "LCD");
#endif
}

/* DSV power +5.8V,-5.8v */
static void ldo_p5m5_dsv_5v5_on(void)
{
#if defined(BUILD_LK)
	chargepump_DSV_on();
#else
	sm5107_dsv_ctrl(1);
#endif
}

/* DSV power turn off */
static void ldo_p5m5_dsv_5v5_off(void)
{
#if defined(BUILD_LK)
	chargepump_DSV_off();
#else
	sm5107_dsv_ctrl(0);
#endif

}

static void reset_lcd_module(unsigned char reset)
{
	mt_set_gpio_mode(GPIO_LCM_RST, GPIO_LCM_RST_M_GPIO);
	/* mt_set_gpio_pull_enable(GPIO_LCM_RST, GPIO_PULL_ENABLE); */
	mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);

	if (reset) {
		mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
		LCM_PRINT("[LCD] Reset High\n");
	} else {
		mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
		LCM_PRINT("[LCD] Reset Low\n");
	}
}

static void touch_reset_pin(int mode)
{
	mt_set_gpio_mode(GPIO_TOUCH_RESET, GPIO_TOUCH_RESET_M_GPIO);
	mt_set_gpio_dir(GPIO_TOUCH_RESET, GPIO_DIR_OUT);

	if (mode == 1)
		mt_set_gpio_out(GPIO_TOUCH_RESET, GPIO_OUT_ONE);
	else if (mode == 0)
		mt_set_gpio_out(GPIO_TOUCH_RESET, GPIO_OUT_ZERO);
}

static void lcm_init(void)
{
	ldo_1v8io_on();
	MDELAY(10);
	ldo_3v0_on();
	MDELAY(10);

	reset_lcd_module(1);
	MDELAY(5);
	reset_lcd_module(0);
	MDELAY(5);
	reset_lcd_module(1);
	MDELAY(50);

	touch_reset_pin(1);
	MDELAY(10);

	ldo_p5m5_dsv_5v5_on();
	MDELAY(10);

	init_lcm_registers();
	MDELAY(120);
	need_set_lcm_addr = 1;
	LCM_PRINT("[LCD] lcm_init\n");
}

static void lcm_suspend(void)
{
	init_lcm_registers_sleep();
	sm5107_dsv_toggle_ctrl();

	LCM_PRINT("[LCD] lcm_suspend\n");
}

static void lcm_resume(void)
{
	ldo_1v8io_off();
	ldo_3v0_off();
	ldo_p5m5_dsv_5v5_off();
	touch_reset_pin(0);
	reset_lcd_module(0);
	MDELAY(20);
	lcm_init();
	need_set_lcm_addr = 1;
	LCM_PRINT("[LCD] lcm_resume\n");
}

#if 0
static void lcm_esd_recover(void)
{
	lcm_suspend();
	lcm_resume();

	LCM_PRINT("[LCD] lcm_esd_recover\n");
}
#endif

/* --------------------------------------------------------------------------- */
/* Get LCM Driver Hooks */
/* --------------------------------------------------------------------------- */
LCM_DRIVER db7400_hd720_dsi_vdo_dongbu_m4_drv = {
	.name = "db7400_hd720_dsi_vdo_dongbu_m4",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
/* .suspend_2nd_cmd = init_lcm_registers_sleep_2nd, */
/* .set_pwm_for_mix = lcm_set_pwm_for_mix, */
/* .compare_id = lcm_compare_id, */
/* .update = lcm_update, */
#if (!defined(BUILD_UBOOT) && !defined(BUILD_LK))
/* .esd_recover = lcm_esd_recover, */
#endif
};
