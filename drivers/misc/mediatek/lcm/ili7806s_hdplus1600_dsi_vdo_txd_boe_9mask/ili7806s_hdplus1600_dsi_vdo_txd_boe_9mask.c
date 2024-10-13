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
#include "lcm_define.h"

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

#include <linux/pinctrl/consumer.h>
#include <linux/touchscreen_info.h>

#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif

/* HS03S code added for SR-AL5625-01-506 by gaozhengwei at 20210526 start */
#ifdef CONFIG_HQ_SET_LCD_BIAS
#include <lcm_bias.h>
#endif
/* HS03S code added for SR-AL5625-01-506 by gaozhengwei at 20210526 end */

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

/*AL5625 code for SR-AL5625-01-105 by wangdeyan at 20210507 start*/
extern enum tp_module_used tp_is_used;
extern void ili_resume_by_ddi(void);
/*AL5625 code for SR-AL5625-01-105 by wangdeyan at 20210507 end*/

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


/* ----------------------------------------------------------------- */
/*  Local Variables */
/* ----------------------------------------------------------------- */



/* ----------------------------------------------------------------- */
/* Local Constants */
/* ----------------------------------------------------------------- */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE      0
#define FRAME_WIDTH           (720)
#define FRAME_HEIGHT          (1600)
#define LCM_DENSITY           (270)

#define LCM_PHYSICAL_WIDTH    (67930)
#define LCM_PHYSICAL_HEIGHT	  (150960)

#define REGFLAG_DELAY		  0xFFFC
#define REGFLAG_UDELAY	      0xFFFB
#define REGFLAG_END_OF_TABLE  0xFFFD
#define REGFLAG_RESET_LOW     0xFFFE
#define REGFLAG_RESET_HIGH    0xFFFF

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

static struct LCM_setting_table lcm_diming_disable_setting[] = {
	{0x53, 0x01, {0x24}}
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{REGFLAG_DELAY, 1, {} },
	{0xFF,03,{0x78,0x07,0x00}},
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0xFF,03,{0x78,0x07,0x01}},
	{0x00,01,{0x42}},
	{0x01,01,{0x51}},
	{0x02,01,{0x00}},
	{0x03,01,{0x9F}},
	{0x04,01,{0x00}},
	{0x05,01,{0x00}},
	{0x06,01,{0x00}},
	{0x07,01,{0x00}},
	{0x08,01,{0x89}},
	{0x09,01,{0x0A}},
	{0x0A,01,{0x30}},
	{0x0B,01,{0x00}},
	{0x0C,01,{0x04}},
	{0x0E,01,{0x04}},
	{0xFF,03,{0x78,0x07,0x11}},
	{0x00,01,{0x04}},
	{0x01,01,{0x04}},
	{0x18,01,{0x5A}},
	{0x19,01,{0x00}},
	{0x1A,01,{0x5A}},
	{0x1B,01,{0x6A}},
	{0x1C,01,{0x64}},
	{0x1D,01,{0x00}},
	{0xFF,03,{0x78,0x07,0x0E}},
	{0x02,01,{0x0D}},
	{0xFF,03,{0x78,0x07,0x01}},
	{0x31,01,{0x07}},
	{0x32,01,{0x07}},
	{0x33,01,{0x07}},
	{0x34,01,{0x07}},
	{0x35,01,{0x07}},
	{0x36,01,{0x07}},
	{0x37,01,{0x07}},
	{0x38,01,{0x40}},
	{0x39,01,{0x40}},
	{0x3A,01,{0x02}},
	{0x3B,01,{0x24}},
	{0x3C,01,{0x00}},
	{0x3D,01,{0x00}},
	{0x3E,01,{0x30}},
	{0x3F,01,{0x2F}},
	{0x40,01,{0x2E}},
	{0x41,01,{0x10}},
	{0x42,01,{0x12}},
	{0x43,01,{0x2D}},
	{0x44,01,{0x41}},
	{0x45,01,{0x41}},
	{0x46,01,{0x01}},
	{0x47,01,{0x24}},
	{0x48,01,{0x08}},
	{0x49,01,{0x07}},
	{0x4A,01,{0x07}},
	{0x4B,01,{0x07}},
	{0x4C,01,{0x07}},
	{0x4D,01,{0x07}},
	{0x4E,01,{0x07}},
	{0x4F,01,{0x07}},
	{0x50,01,{0x40}},
	{0x51,01,{0x40}},
	{0x52,01,{0x02}},
	{0x53,01,{0x24}},
	{0x54,01,{0x00}},
	{0x55,01,{0x00}},
	{0x56,01,{0x30}},
	{0x57,01,{0x2F}},
	{0x58,01,{0x2E}},
	{0x59,01,{0x11}},
	{0x5A,01,{0x13}},
	{0x5B,01,{0x2D}},
	{0x5C,01,{0x41}},
	{0x5D,01,{0x41}},
	{0x5E,01,{0x01}},
	{0x5F,01,{0x24}},
	{0x60,01,{0x09}},
	{0x61,01,{0x07}},
	{0x62,01,{0x07}},
	{0x63,01,{0x07}},
	{0x64,01,{0x07}},
	{0x65,01,{0x07}},
	{0x66,01,{0x07}},
	{0x67,01,{0x07}},
	{0x68,01,{0x40}},
	{0x69,01,{0x40}},
	{0x6A,01,{0x02}},
	{0x6B,01,{0x24}},
	{0x6C,01,{0x01}},
	{0x6D,01,{0x01}},
	{0x6E,01,{0x30}},
	{0x6F,01,{0x2F}},
	{0x70,01,{0x2E}},
	{0x71,01,{0x13}},
	{0x72,01,{0x11}},
	{0x73,01,{0x2D}},
	{0x74,01,{0x41}},
	{0x75,01,{0x41}},
	{0x76,01,{0x24}},
	{0x77,01,{0x00}},
	{0x78,01,{0x09}},
	{0x79,01,{0x07}},
	{0x7A,01,{0x07}},
	{0x7B,01,{0x07}},
	{0x7C,01,{0x07}},
	{0x7D,01,{0x07}},
	{0x7E,01,{0x07}},
	{0x7F,01,{0x07}},
	{0x80,01,{0x40}},
	{0x81,01,{0x40}},
	{0x82,01,{0x02}},
	{0x83,01,{0x24}},
	{0x84,01,{0x01}},
	{0x85,01,{0x01}},
	{0x86,01,{0x30}},
	{0x87,01,{0x2F}},
	{0x88,01,{0x2E}},
	{0x89,01,{0x12}},
	{0x8A,01,{0x10}},
	{0x8B,01,{0x2D}},
	{0x8C,01,{0x41}},
	{0x8D,01,{0x41}},
	{0x8E,01,{0x24}},
	{0x8F,01,{0x00}},
	{0x90,01,{0x08}},
	{0x92,01,{0x10}},
	{0xA2,01,{0x4C}},
	{0xA3,01,{0x49}},
	{0xA7,01,{0x10}},
	{0xAE,01,{0x00}},
	{0xB0,01,{0x20}},
	{0xB4,01,{0x05}},
	{0xC0,01,{0x0C}},
	{0xC6,01,{0x64}},
	{0xC7,01,{0x00}},
	{0xD1,01,{0x00}},
	{0xD2,01,{0x10}},
	{0xD4,01,{0x14}},
	{0xD5,01,{0x01}},
	{0xD7,01,{0x80}},
	{0xD8,01,{0x40}},
	{0xD9,01,{0x04}},
	{0xDF,01,{0x4A}},
	{0xDE,01,{0x03}},
	{0xDD,01,{0x08}},
	{0xE0,01,{0x00}},
	{0xE1,01,{0x28}},
	{0xE2,01,{0x06}},
	{0xE3,01,{0x31}},
	{0xE4,01,{0x02}},
	{0xE5,01,{0x6D}},
	{0xE6,01,{0x12}},
	{0xE7,01,{0x0C}},
	{0xED,01,{0x56}},
	{0xF4,01,{0x54}},
	{0xFF,03,{0x78,0x07,0x02}},
	{0x01,01,{0xD5}},
	{0x47,01,{0x00}},
	{0x4F,01,{0x01}},
	{0x6B,01,{0x11}},
	{0xFF,03,{0x78,0x07,0x12}},
	{0x10,01,{0x10}},
	{0x12,01,{0x08}},
	{0x13,01,{0x4F}},
	{0x16,01,{0x08}},
	{0x3A,01,{0x05}},
	{0xC0,01,{0xA1}},
	{0xC2,01,{0x12}},
	{0xC3,01,{0x20}},
	{0x1A,01,{0x3F}},
	{0xFF,03,{0x78,0x07,0x04}},
	{0xBD,01,{0x01}},
	{0xFF,03,{0x78,0x07,0x04}},
	{0xB7,01,{0xCF}},
	{0xB8,01,{0x45}},
	{0xBA,01,{0x74}},
	{0xFF,03,{0x78,0x07,0x05}},
	{0x61,01,{0xCB}},
	{0xFF,03,{0x78,0x07,0x07}},
	{0x07,01,{0x4C}},
	{0x11,01,{0x16}},
	{0x29,01,{0x00}},
	{0xFF,03,{0x78,0x07,0x05}},
	{0x1C,01,{0x85}},
	{0x72,01,{0x56}},
	{0x74,01,{0x56}},
	{0x76,01,{0x51}},
	{0x7A,01,{0x51}},
	{0x7B,01,{0x88}},
	{0x7C,01,{0x88}},
	{0xAE,01,{0x29}},
	{0xB1,01,{0x39}},
	{0x46,01,{0x58}},
	{0x47,01,{0x78}},
	{0xB5,01,{0x58}},
	{0xB7,01,{0x78}},
	{0x3E,01,{0x50}},
	{0xC9,01,{0x90}},
	{0x56,01,{0xFF}},
	{0xFF,03,{0x78,0x07,0x06}},
	{0xC0,01,{0x40}},
	{0xC1,01,{0x16}},
	{0xC2,01,{0xFA}},
	{0xC3,01,{0x06}},
	{0x96,01,{0x50}},
	{0xDD,01,{0x17}},
	{0xD6,01,{0x55}},
	{0xCD,01,{0x68}},
	{0x13,01,{0x13}},
	{0xB4,01,{0xDC}},
	{0xB5,01,{0x23}},
	{0xFF,03,{0x78,0x07,0x08}},
	{0xE0,31,{0x00,0x00,0x1A,0x46,0x00,0x84,0xB1,0xD6,0x15,0x0F,0x3B,0x7D,0x25,0xAD,0xF8,0x33,0x2A,0x6B,0xAB,0xD3,0x3F,0x06,0x2B,0x53,0x3F,0x6F,0x90,0xBE,0x0F,0xD7,0xD9}},
	{0xE1,31,{0x00,0x00,0x1A,0x46,0x00,0x84,0xB1,0xD6,0x15,0x0F,0x3B,0x7D,0x25,0xAD,0xF8,0x33,0x2A,0x6B,0xAB,0xD3,0x3F,0x06,0x2B,0x53,0x3F,0x6F,0x90,0xBE,0x0F,0xD7,0xD9}},
	{0xFF,03,{0x78,0x07,0x0B}},
	{0xC0,01,{0x88}},
	{0xC1,01,{0x29}},
	{0xC2,01,{0x06}},
	{0xC3,01,{0x06}},
	{0xC4,01,{0xCB}},
	{0xC5,01,{0xCB}},
	{0xD2,01,{0x45}},
	{0xD3,01,{0x10}},
	{0xD4,01,{0x04}},
	{0xD5,01,{0x04}},
	{0xD6,01,{0x7E}},
	{0xD7,01,{0x7E}},
	{0xAB,01,{0xE0}},
	{0xFF,03,{0x78,0x07,0x0C}},
	{0x00,01,{0x27}},
	{0x01,01,{0x9D}},
	{0x02,01,{0x27}},
	{0x03,01,{0x9E}},
	{0x04,01,{0x27}},
	{0x05,01,{0x9E}},
	{0x06,01,{0x27}},
	{0x07,01,{0xA5}},
	{0x08,01,{0x27}},
	{0x09,01,{0xA3}},
	{0x0A,01,{0x27}},
	{0x0B,01,{0xA0}},
	{0x0C,01,{0x27}},
	{0x0D,01,{0xA1}},
	{0x0E,01,{0x27}},
	{0x0F,01,{0xA4}},
	{0x10,01,{0x27}},
	{0x11,01,{0xA7}},
	{0x12,01,{0x27}},
	{0x13,01,{0xA6}},
	{0x14,01,{0x27}},
	{0x15,01,{0xA2}},
	{0xFF,03,{0x78,0x07,0x0E}},
	{0x00,01,{0xA3}},
	{0x04,01,{0x00}},
	{0x20,01,{0x07}},
	{0x27,01,{0x00}},
	{0x29,01,{0xC8}},
	{0x25,01,{0x0B}},
	{0x26,01,{0x0C}},
	{0x2D,01,{0x21}},
	{0x30,01,{0x04}},
	{0x21,01,{0x24}},
	{0x23,01,{0x44}},
	{0xB0,01,{0x01}},
	{0xC0,01,{0x12}},
	{0x05,01,{0x20}},
	{0x2B,01,{0x0A}},
	{0xFF,03,{0x78,0x07,0x1E}},
	{0xA2,01,{0x00}},
	{0xAD,01,{0x02}},
	{0x00,01,{0x91}},
	{0x08,01,{0x91}},
	{0x09,01,{0x91}},
	{0x0A,01,{0x10}},
	{0x0C,01,{0x08}},
	{0x0D,01,{0x4F}},
	{0xFF,03,{0x78,0x07,0x03}},
	{0x83,01,{0x20}},
	{0x84,01,{0x00}},
	{0xFF,03,{0x78,0x07,0x00}},
	{0x68,02,{0x04,0x00}},
	{0x51,02,{0x00,0x00}},
	{0x53,01,{0x2C}},
	{0x55,01,{0x00}},
	{0xFF,03,{0x78,0x07,0x00}},
	{0x11,01,{0x00}},
	{REGFLAG_DELAY,80,{}},
	{0x29,01,{0x00}},
	{REGFLAG_DELAY,20,{}},
	{0x35,01,{0x00}},


	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table bl_level[] = {
	{0xFF,03,{0x78,0x07,0x00}},
	{0x51, 2, {0x00,0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
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
	params->density = LCM_DENSITY;
	/* params->physical_width_um = LCM_PHYSICAL_WIDTH; */
	/* params->physical_height_um = LCM_PHYSICAL_HEIGHT; */

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
	params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	/* lcm_dsi_mode = CMD_MODE; */
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
	/* params->dsi.switch_mode = CMD_MODE; */
	/* lcm_dsi_mode = SYNC_PULSE_VDO_MODE; */
#endif
	/* LCM_LOGI("lcm_get_params lcm_dsi_mode %d\n", lcm_dsi_mode); */
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

	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 16;
	params->dsi.vertical_frontporch = 32;
	/* disable dynamic frame rate */
	/* params->dsi.vertical_frontporch_for_low_power = 540; */
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 12;
	params->dsi.horizontal_backporch = 292;
	params->dsi.horizontal_frontporch = 300;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
	/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 270;
#else
	params->dsi.PLL_CLOCK = 550;
#endif
	/* params->dsi.PLL_CK_CMD = 220; */
	/* params->dsi.PLL_CK_VDO = 255; */
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


	/* params->use_gpioID = 1; */
	/* params->gpioID_value = 0; */
}

static void lcm_init_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

/* HS03S code added for SR-AL5625-01-506 by gaozhengwei at 20210526 start */
#ifdef CONFIG_HQ_SET_LCD_BIAS
	lcd_bias_set_vspn(ON, VSP_FIRST_VSN_AFTER, 6000);  //open lcd bias
	MDELAY(15);
#else
	MDELAY(2);
	display_bias_enable();
#endif
/* HS03S code added for SR-AL5625-01-506 by gaozhengwei at 20210526 end */
}

static void lcm_suspend_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	/* HS03S code for SR-AL5625-01-181 by gaozhengwei at 2021/04/25 start */
	if (g_system_is_shutdown) {
		lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
		MDELAY(3);
	}
	/* HS03S code for SR-AL5625-01-181 by gaozhengwei at 2021/04/25 end */

/* HS03S code added for SR-AL5625-01-506 by gaozhengwei at 20210526 start */
#ifdef CONFIG_HQ_SET_LCD_BIAS
	lcd_bias_set_vspn(OFF, VSN_FIRST_VSP_AFTER, 6000);  //close lcd bias
#else
	display_bias_disable();
#endif
/* HS03S code added for SR-AL5625-01-506 by gaozhengwei at 20210526 end */
}

static void lcm_resume_power(void)
{
	lcm_init_power();
}

static void lcm_init(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	/* after 6ms reset HLH */
	MDELAY(9);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(4);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(5);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(10);

	/*AL5625 code for SR-AL5625-01-105 by wangdeyan at 20210507 start*/
	if (tp_is_used == ILITEK_ILI7807S) {
		ili_resume_by_ddi();
	}
	/*AL5625 code for SR-AL5625-01-105 by wangdeyan at 20210507 end*/

	if ((lcd_pinctrl1 == NULL) || (lcd_disp_pwm_gpio == NULL)) {
		pr_err("lcd_pinctrl1/lcd_disp_pwm_gpio is invaild\n");
		return;
	}

	pinctrl_select_state(lcd_pinctrl1, lcd_disp_pwm);

	push_table(NULL,
		init_setting_vdo,
		sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table),
		1);
	pr_notice("[Kernel/LCM] %s exit\n", __func__);
}

static void lcm_suspend(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	push_table(NULL,
		lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
		1);
	MDELAY(10);

	if ((lcd_pinctrl1 == NULL) || (lcd_disp_pwm_gpio == NULL)) {
		pr_err("lcd_pinctrl1/lcd_disp_pwm_gpio is invaild\n");
		return;
	}

	pinctrl_select_state(lcd_pinctrl1, lcd_disp_pwm_gpio);

}

static void lcm_resume(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	lcm_init();
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	LCM_LOGI("%s,ili7806s_txd_boe backlight: level = %d\n", __func__, level);

	if (level == 0) {
		pr_notice("[LCM],ili7806s_txd_boe backlight off, diming disable first\n");
		push_table(NULL,
			lcm_diming_disable_setting,
			sizeof(lcm_diming_disable_setting) / sizeof(struct LCM_setting_table),
			1);
	}

	/* Backlight data is mapped from 8 bits to 12 bits,The default scale is 17(255):273(4095) */
	/* The initial backlight current is 23mA */
	level = mult_frac(level, 273, 17);
	bl_level[1].para_list[0] = level >> 8;
	bl_level[1].para_list[1] = level & 0xFF;

	push_table(handle, bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER ili7806s_hdplus1600_dsi_vdo_txd_boe_9mask_lcm_drv = {
	.name = "ili7806s_hdplus1600_dsi_vdo_txd_boe_9mask",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
};

