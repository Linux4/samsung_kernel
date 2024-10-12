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
/*TabA7 Lite code for SR-AX3565-01-45 by gaozhengwei at 20201229 start*/
#include <linux/touchscreen_info.h>
/*TabA7 Lite code for SR-AX3565-01-45 by gaozhengwei at 20201229 end*/
/*TabA7 Lite code for OT8-4085 by weiqiang at 20210319 start*/
#include <linux/regulator/consumer.h>
/*TabA7 Lite code for OT8-4085 by weiqiang at 20210319 end*/

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

#define GPIO_OUT_LOW 0
#define GPIO_OUT_ONE 1
#define GPIO_OUT_ZERO 0
extern void lcm_set_gpio_output(unsigned int GPIO, unsigned int output);
extern unsigned int GPIO_LCD_RST;
extern struct pinctrl *lcd_pinctrl1;
extern struct pinctrl_state *lcd_disp_pwm;
extern struct pinctrl_state *lcd_disp_pwm_gpio;

/*TabA7 Lite code for SR-AX3565-01-45 by gaozhengwei at 20201229 start*/
extern void ili_resume_by_ddi(void);
/*TabA7 Lite code for SR-AX3565-01-45 by gaozhengwei at 20201229 end*/

extern bool g_system_is_shutdown;

/* ----------------------------------------------------------------- */
/*  Local Variables */
/* ----------------------------------------------------------------- */



/* ----------------------------------------------------------------- */
/* Local Constants */
/* ----------------------------------------------------------------- */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE      0
#define FRAME_WIDTH           (800)
#define FRAME_HEIGHT          (1340)
#define LCM_DENSITY           (213)

#define LCM_PHYSICAL_WIDTH    (107640)
#define LCM_PHYSICAL_HEIGHT	  (172224)

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
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {
	// GIP Setting
	{0xFF, 0x03, {0x98,0x81,0x01}},
	{0x00,1,{0x44}},			//STVA rise
	{0x01,1,{0x13}},			//STVA width
	{0x02,1,{0x01}},			//STVA FTI rise
	{0x03,1,{0x00}},			//STVA FTI fall
	{0x04,1,{0x02}},			//STVB rise
	{0x05,1,{0x13}},			//STVB width
	{0x06,1,{0x00}},		  //STVB FTI rise
	{0x07,1,{0x00}},     //STVB FTI fall
	{0x08,1,{0x82}},			//CLKA rise
	{0x09,1,{0x09}},			//CLKA Fall
	{0x0a,1,{0xb3}},			//CLK Width
	{0x0b,1,{0x00}},
	{0x0c,1,{0x0A}},			//CLKA1 rise
	{0x0d,1,{0x0A}},			//CLKA2	rise
	{0x0e,1,{0x00}},			//CLKA1 fall
	{0x0f,1,{0x00}},			//CLKA2 fall
	{0x16,1,{0x82}},
	{0x17,1,{0x09}},
	{0x18,1,{0x33}},
	{0x19,1,{0x00}},
	{0x1A,1,{0x0A}},
	{0x1B,1,{0x0A}},
	{0x1C,1,{0x00}},
	{0x1D,1,{0x00}},
	{0x24,1,{0x42}},			//STVC rise
	{0x25,1,{0x0b}},			//STVC width
	{0x26,1,{0x00}},			//STVC FTI rise
	{0x27,1,{0x00}},     //STVC FTI fall
	{0x28,1,{0x88}},
	{0x29,1,{0x88}},
	{0x2a,1,{0x4a}},
	{0x2b,1,{0x85}},
	{0x2c,1,{0x34}},
	{0x2d,1,{0x43}},

	{0x31,1,{0x07}},     // CGOUTR[1]    open
	{0x32,1,{0x07}},     // CGOUTR[2]    open
	{0x33,1,{0x07}},     // CGOUTR[3]    open
	{0x34,1,{0x07}},     // CGOUTR[4]    open
	{0x35,1,{0x07}},     // CGOUTR[5]    open
	{0x36,1,{0x07}},     // CGOUTR[6]    open
	{0x37,1,{0x07}},     // CGOUTR[7]    open
	{0x38,1,{0x07}},     // CGOUTR[8]    open
	{0x39,1,{0x20}},     // CGOUTR[9]    STV2_ODD
	{0x3A,1,{0x2A}},     // CGOUTR[10]   VSSG_ODD
	{0x3B,1,{0x0C}},     // CGOUTR[11]   RESET_ODD
	{0x3C,1,{0x29}},     // CGOUTR[12]   VDD2_ODD
	{0x3D,1,{0x28}},     // CGOUTR[13]   VDD1_ODD
	{0x3E,1,{0x1A}},     // CGOUTR[14]   CK11
	{0x3F,1,{0x18}},     // CGOUTR[15]   CK9
	{0x40,1,{0x16}},     // CGOUTR[16]   CK7
	{0x41,1,{0x14}},     // CGOUTR[17]   CK5
	{0x42,1,{0x12}},     // CGOUTR[18]   CK3
	{0x43,1,{0x10}},     // CGOUTR[19]   CK1
	{0x44,1,{0x08}},     // CGOUTR[20]   STV1_ODD
	{0x45,1,{0x07}},     // CGOUTR[21]   open
	{0x46,1,{0x07}},     // CGOUTR[22]   open

	{0x47,1,{0x07}},     // CGOUTL[1]   open
	{0x48,1,{0x07}},     // CGOUTL[2]   open
	{0x49,1,{0x07}},     // CGOUTL[3]   open
	{0x4A,1,{0x07}},     // CGOUTL[4]   open
	{0x4B,1,{0x07}},     // CGOUTL[5]   open
	{0x4C,1,{0x07}},     // CGOUTL[6]   open
	{0x4D,1,{0x07}},     // CGOUTL[7]   open
	{0x4E,1,{0x07}},     // CGOUTL[8]   open
	{0x4F,1,{0x21}},     // CGOUTL[9]   STV2_EVEN
	{0x50,1,{0x2A}},     // CGOUTL[10]  VSSG_EVEN
	{0x51,1,{0x0D}},     // CGOUTL[11]  RESET_EVEN
	{0x52,1,{0x29}},     // CGOUTL[12]  VDD2_EVEN
	{0x53,1,{0x28}},     // CGOUTL[13]  VDD1_EVEN
	{0x54,1,{0x1B}},     // CGOUTL[14]  CK12
	{0x55,1,{0x19}},     // CGOUTL[15]  CK10
	{0x56,1,{0x17}},     // CGOUTL[16]  CK8
	{0x57,1,{0x15}},     // CGOUTL[17]  CK6
	{0x58,1,{0x13}},     // CGOUTL[18]  CK4
	{0x59,1,{0x11}},     // CGOUTL[19]  CK2
	{0x5A,1,{0x09}},     // CGOUTL[20]  STV1_EVEN
	{0x5B,1,{0x07}},     // CGOUTL[21]  open
	{0x5C,1,{0x07}},     // CGOUTL[22]  open

	{0x61,1,{0x07}},     // CGOUTR[1]   open
	{0x62,1,{0x07}},     // CGOUTR[2]   open
	{0x63,1,{0x07}},     // CGOUTR[3]   open
	{0x64,1,{0x07}},     // CGOUTR[4]   open
	{0x65,1,{0x07}},     // CGOUTR[5]   open
	{0x66,1,{0x07}},     // CGOUTR[6]   open
	{0x67,1,{0x07}},     // CGOUTR[7]   open
	{0x68,1,{0x07}},     // CGOUTR[8]   open
	{0x69,1,{0x09}},     // CGOUTR[9]   STV2_ODD
	{0x6A,1,{0x2A}},     // CGOUTR[10]  VSSG_ODD
	{0x6B,1,{0x0D}},     // CGOUTR[11]  RESET_ODD
	{0x6C,1,{0x29}},     // CGOUTR[12]  VDD2_ODD
	{0x6D,1,{0x28}},     // CGOUTR[13]  VDD1_ODD
	{0x6E,1,{0x19}},     // CGOUTR[14]  CK11
	{0x6F,1,{0x1B}},     // CGOUTR[15]  CK9
	{0x70,1,{0x11}},     // CGOUTR[16]  CK7
	{0x71,1,{0x13}},     // CGOUTR[17]  CK5
	{0x72,1,{0x15}},     // CGOUTR[18]  CK3
	{0x73,1,{0x17}},     // CGOUTR[19]  CK1
	{0x74,1,{0x21}},     // CGOUTR[20]  STV1_ODD
	{0x75,1,{0x07}},     // CGOUTR[21]  open
	{0x76,1,{0x07}},     // CGOUTR[22]  open

	{0x77,1,{0x07}},     // CGOUTL[1]   open
	{0x78,1,{0x07}},     // CGOUTL[2]   open
	{0x79,1,{0x07}},     // CGOUTL[3]   open
	{0x7A,1,{0x07}},     // CGOUTL[4]   open
	{0x7B,1,{0x07}},     // CGOUTL[5]   open
	{0x7C,1,{0x07}},     // CGOUTL[6]   open
	{0x7D,1,{0x07}},     // CGOUTL[7]   open
	{0x7E,1,{0x07}},     // CGOUTL[8]   open
	{0x7F,1,{0x08}},     // CGOUTL[9]   STV2_EVEN
	{0x80,1,{0x2A}},     // CGOUTL[10]  VSSG_EVEN
	{0x81,1,{0x0C}},     // CGOUTL[11]  RESET_EVEN
	{0x82,1,{0x29}},     // CGOUTL[12]  VDD2_EVEN
	{0x83,1,{0x28}},     // CGOUTL[13]  VDD1_EVEN
	{0x84,1,{0x18}},     // CGOUTL[14]  CK12
	{0x85,1,{0x1A}},     // CGOUTL[15]  CK10
	{0x86,1,{0x10}},     // CGOUTL[16]  CK8
	{0x87,1,{0x12}},     // CGOUTL[17]  CK6
	{0x88,1,{0x14}},     // CGOUTL[18]  CK4
	{0x89,1,{0x16}},     // CGOUTL[19]  CK2
	{0x8A,1,{0x20}},     // CGOUTL[20]  STV1_EVEN
	{0x8B,1,{0x07}},     // CGOUTL[21]  open
	{0x8C,1,{0x07}},     // CGOUTL[22]  open

	{0xb0,1,{0x33}},
	{0xb8,1,{0x00}},
	{0xb9,1,{0x10}},
	{0xba,1,{0x06}},
	{0xc0,1,{0x07}},
	{0xc1,1,{0x00}},
	{0xc2,1,{0x00}},
	{0xc3,1,{0x00}},
	{0xc4,1,{0x00}},
	{0xca,1,{0x44}},
	{0xd0,1,{0x01}},
	{0xd1,1,{0x10}},
	{0xd2,1,{0x40}},
	{0xd3,1,{0x21}},
	{0xd5,1,{0x5d}},
	{0xd6,1,{0x60}},
	{0xd7,1,{0x00}},
	{0xd8,1,{0x00}},
	{0xdc,1,{0xff}},
	{0xdd,1,{0x7f}},
	{0xdf,1,{0xb7}},
	{0xe0,1,{0x5e}},
	{0xe2,1,{0x55}},
	{0xe7,1,{0x54}},
	{0xe6,1,{0x21}},
	{0xee,1,{0x11}},
	{0xf0,1,{0x68}},
	{0xf1,1,{0x00}},
	{0xf2,1,{0x00}},
	{0xf6,1,{0x00}},

	// RTN. Internal VBP, Internal VFP
	{0xFF, 0x03, {0x98,0x81,0x02}},
	{0x01,1,{0x00}},		//mipi timeout 补黑
	{0x06,1,{0xA0}},     // Internal Line Time (RTN)
	{0x0A,1,{0x1B}},     // Internal VFP[9:8]
	{0x0C,1,{0x00}},     // Internal VBP[8]
	{0x0D,1,{0x1C}},     // Internal VBP
	{0x0E,1,{0xC8}},     // Internal VFP
	{0x39,1,{0x01}},     // VBP[8],RTN[8],VFP[9:8]
	{0x3A,1,{0x1C}},     // BIST VBP
	{0x3B,1,{0xC8}},     // BIST VFP
	{0x3C,1,{0xA2}},     // BIST RTN
	{0x3D,1,{0x00}},     // BIST RTN[8] (Line time change)
	{0x3E,1,{0xA1}},     // BIST RTN (Line time change)
	{0xF0,1,{0x00}},     // BIST VFP[12:8] (Line time change)
	{0xF1,1,{0xC9}},     // BIST VFP (Line time change)

	// Power Setting
	{0xFF, 0x03, {0x98,0x81,0x05}},
	{0x03,1,{0x00}}, 		//VCOM
	{0x04,1,{0xBE}}, 		//VCOM -0.5V
	{0x69,1,{0x92}}, 		//GVDDN  5.4V
	{0x6A,1,{0x92}}, 		//GVDDP -5.4V
	{0x6C,1,{0x8D}}, 		//VGHO   14V
	{0x72,1,{0xA7}}, 		//VGH    16V
	{0x78,1,{0xC9}}, 		//VGLO   -14V
	{0x7E,1,{0xCF}}, 		//VGL    -16V

	// Resolution
	{0xFF, 0x03, {0x98,0x81,0x06}},
	{0xD9,1,{0x1F}},     // 4Lane
	{0xC0,1,{0x3C}},     // NL_Y = 1340
	{0xC1,1,{0x05}},     // NL_Y = 1340
	{0xCA,1,{0x20}},     // NL_X = 800
	{0xCB,1,{0x03}},     // NL_X = 800

	//Gamma Register
	{0xFF, 0x03, {0x98,0x81,0x08}},
	{0xE0,27,{0x00,0x1F,0x50,0x76,0xAD,0x54,0xDC,0x04,0x32,0x58,0xA5,0x9C,0xD1,0x00,0x2E,0xAA,0x5E,0x97,0xBB,0xE8,0xFF,0x0C,0x39,0x71,0x9E,0x03,0xEC}},
	{0xE1,27,{0x00,0x1F,0x50,0x76,0xAD,0x54,0xDC,0x04,0x32,0x58,0xA5,0x9C,0xD1,0x00,0x2E,0xAA,0x5E,0x97,0xBB,0xE8,0xFF,0x0C,0x39,0x71,0x9E,0x03,0xEC}},

	// OSC Auto Trim Setting
	{0xFF, 0x03, {0x98,0x81,0x0B}},
	{0x9A,1,{0x45}},
	{0x9B,1,{0x0F}},
	{0x9C,1,{0x04}},
	{0x9D,1,{0x04}},
	{0x9E,1,{0x7E}},
	{0x9F,1,{0x7E}},
	{0xAB,1,{0xE0}},     // AutoTrimType

	{0xFF, 0x03, {0x98,0x81,0x0E}},
	{0x11,1,{0x4D}},      // TSVD_LONGV_RISE [4:0]
	{0x13,1,{0x10}},     // LV mode TSHD Rise position [4:0]
	{0x00,1,{0xA0}},      // LV mode

	//PWM 12bit 15.6K
	{0xFF, 0x03, {0x98,0x81,0x03}},
	{0x83,1,{0x20}},
	{0x84,1,{0x00}},
	{0xFF, 0x03, {0x98,0x81,0x06}},
	{0x06,1,{0xA4}},

	{0xFF, 0x03, {0x98,0x81,0x00}},
	{0x68,2,{0x04,0x00}},
	{0x51,2,{0x00,0x00}},
	{0x53,1,{0x2C}},
	{0x35,1,{0x00}},
	//Sleep Out
	{0x11,1,{0x00}},
	{REGFLAG_DELAY,80,{}},
	//Display On
	{0x29,1,{0x00}},
	{REGFLAG_DELAY,0,{}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table bl_level[] = {
	{0xFF,3,{0x98,0x81,0x00}},
	{0x51,2,{0x00,0x00}},
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
	params->dsi.vertical_backporch = 20;
	params->dsi.vertical_frontporch = 200;
	/* disable dynamic frame rate */
	/* params->dsi.vertical_frontporch_for_low_power = 540; */
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 7;
	params->dsi.horizontal_backporch = 32;
	params->dsi.horizontal_frontporch = 32;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
	/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 270;
#else
	params->dsi.PLL_CLOCK = 260;
#endif
	/* params->dsi.PLL_CK_CMD = 220; */
	/* params->dsi.PLL_CK_VDO = 255; */
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif
	params->dsi.clk_lp_per_line_enable = 0;
	/*TabA7 Lite code for SR-AX3565-01-72 by gaozhengwei at 20201217 start*/
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
	/*TabA7 Lite code for SR-AX3565-01-72 by gaozhengwei at 20201217 end*/


	/* params->use_gpioID = 1; */
	/* params->gpioID_value = 0; */
}


static void lcm_init_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	display_bias_enable();
}

static void lcm_suspend_power(void)
{
	pr_notice("[Kernel/LCM] %s enter\n", __func__);

	/*TabA7 Lite code for OT8-4010 by gaozhengwei at 20210319 start*/
	if (g_system_is_shutdown) {
		lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
		MDELAY(3);
	}
	/*TabA7 Lite code for OT8-4010 by gaozhengwei at 20210319 end*/

	display_bias_disable();
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
	MDELAY(1);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(1);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(10);

	/*TabA7 Lite code for SR-AX3565-01-45 by gaozhengwei at 20201229 start*/
	if (tp_is_used == Ilitek) {
		ili_resume_by_ddi();
	}
	/*TabA7 Lite code for SR-AX3565-01-45 by gaozhengwei at 20201229 end*/

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
	pr_notice("ILI9881T [Kernel/LCM] %s enter\n", __func__);

	push_table(NULL,
		lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
		1);
	MDELAY(10);

	/*TabA7 Lite code for OT8-701 by gaozhengwei at 20210111 start*/
	if ((lcd_pinctrl1 == NULL) || (lcd_disp_pwm_gpio == NULL)) {
		pr_err("lcd_pinctrl1/lcd_disp_pwm_gpio is invaild\n");
		return;
	}
	/*TabA7 Lite code for OT8-701 by gaozhengwei at 20210111 end*/

	pinctrl_select_state(lcd_pinctrl1, lcd_disp_pwm_gpio);
}

static void lcm_resume(void)
{
	pr_notice("ILI9881T [Kernel/LCM] %s enter\n", __func__);

	lcm_init();
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	pr_notice("%s,ILI9881T backlight: level = %d\n", __func__, level);

	if (level == 0) {
		pr_notice("[LCM],LS_INX backlight off,diming disable first\n");
		push_table(NULL,
			lcm_diming_disable_setting,
			sizeof(lcm_diming_disable_setting) / sizeof(struct LCM_setting_table),
			1);
	}

	/* Backlight data is mapped from 8 bits to 12 bits,The default scale is 1:16 */
	/* The initial backlight current is 22mA */
	level = mult_frac(level, 2457, 187); //Backlight current setting 22mA -> 18mA
	bl_level[1].para_list[0] = level >> 8;
	bl_level[1].para_list[1] = level & 0xFF;

	push_table(handle, bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER ili9881t_liansi_inx_incell_vdo_lcm_drv = {
	.name = "ili9881t_liansi_inx_incell_vdo",
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

