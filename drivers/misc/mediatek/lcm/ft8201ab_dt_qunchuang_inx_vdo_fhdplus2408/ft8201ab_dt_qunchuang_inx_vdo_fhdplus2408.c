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
/*TabA7 Lite code for OT8-4085 by weiqiang at 20210319 start*/
#include <linux/regulator/consumer.h>
/*TabA7 Lite code for OT8-4085 by weiqiang at 20210319 end*/

#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif
#define LCM_NAME "ft8201ab"
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
	{REGFLAG_DELAY, 140, {} },
	{0x00,1,{0x00}},
	{0xF7,4,{0x5A,0xA5,0x95,0x27}}
};

static struct LCM_setting_table init_setting_vdo[] = {
	{0x00,1,{0x00}},
	{0xFF,3,{0x82,0x01,0x01}},
	{0x00,1,{0x80}},
	{0xFF,2,{0x82,0x01}},
	{0x00,1,{0x93}},
	{0xC5,1,{0x52}},
	{0x00,1,{0x97}},
	{0xC5,1,{0x52}},
	{0x00,1,{0x9E}},
	{0xC5,1,{0x05}},
	{0x00,1,{0x9A}},
	{0xC5,1,{0xE1}},
	{0x00,1,{0x9C}},
	{0xC5,1,{0xE1}},
	{0x00,1,{0xB6}},
	{0xC5,2,{0x43,0x43}},
	{0x00,1,{0xB8}},
	{0xC5,2,{0x57,0x57}},
	{0x00,1,{0x00}},
	{0xD8,2,{0xD2,0xD2}},
	{0x00,1,{0x82}},
	{0xC5,1,{0x95}},
	{0x00,1,{0x83}},
	{0xC5,1,{0x07}},
	{0x00,1,{0x80}},
	{0xA4,1,{0x8C}},
	{0x00,1,{0xA0}},
	{0xF3,1,{0x10}},
	{0x00,1,{0xA1}},
	{0xB3,2,{0x03,0x20}},
	{0x00,1,{0xA3}},
	{0xB3,2,{0x05,0x3C}},
	{0x00,1,{0xA5}},
	{0xB3,2,{0x00,0x13}},
	{0x00,1,{0xD0}},
	{0xC1,1,{0x30}},
	{0x00,1,{0x80}},
	{0xCB,7,{0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F}},
	{0x00,1,{0x87}},
	{0xCB,1,{0x3F}},
	{0x00,1,{0x88}},
	{0xCB,8,{0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F}},
	{0x00,1,{0x90}},
	{0xCB,7,{0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F}},
	{0x00,1,{0x97}},
	{0xCB,1,{0x3F}},
	{0x00,1,{0x98}},
	{0xCB,8,{0xC4,0xC4,0xC4,0xC4,0xC4,0xC4,0xC4,0xC4}},
	{0x00,1,{0xA0}},
	{0xCB,8,{0xC4,0xC4,0xCB,0xC8,0xC4,0xC4,0xC4,0xC4}},
	{0x00,1,{0xA8}},
	{0xCB,8,{0xC4,0xC4,0xC4,0xC4,0xC4,0xC4,0xC4,0xC4}},
	{0x00,1,{0xB0}},
	{0xCB,7,{0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x00,1,{0xB7}},
	{0xCB,1,{0x00}},
	{0x00,1,{0xB8}},
	{0xCB,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x00,1,{0xC0}},
	{0xCB,7,{0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x00,1,{0xC7}},
	{0xCB,1,{0x00}},
	{0x00,1,{0x80}},
	{0xCC,8,{0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38}},
	{0x00,1,{0x88}},
	{0xCC,8,{0x20,0x2B,0x22,0x35,0x34,0x1A,0x18,0x16}},
	{0x00,1,{0x90}},
	{0xCC,6,{0x14,0x12,0x10,0x08,0x38,0x38}},
	{0x00,1,{0x80}},
	{0xCD,8,{0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38}},
	{0x00,1,{0x88}},
	{0xCD,8,{0x1F,0x2B,0x21,0x35,0x34,0x19,0x17,0x15}},
	{0x00,1,{0x90}},
	{0xCD,6,{0x13,0x11,0x0F,0x07,0x38,0x38}},
	{0x00,1,{0xA0}},
	{0xCC,8,{0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38}},
	{0x00,1,{0xA8}},
	{0xCC,8,{0x07,0x2B,0x21,0x35,0x34,0x17,0x19,0x0F}},
	{0x00,1,{0xB0}},
	{0xCC,6,{0x11,0x13,0x15,0x1F,0x38,0x38}},
	{0x00,1,{0xA0}},
	{0xCD,8,{0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38}},
	{0x00,1,{0xA8}},
	{0xCD,8,{0x08,0x2B,0x22,0x35,0x34,0x18,0x1A,0x10}},
	{0x00,1,{0xB0}},
	{0xCD,6,{0x12,0x14,0x16,0x20,0x38,0x38}},
	{0x00,1,{0x81}},
	{0xC2,1,{0x40}},
	{0x00,1,{0x90}},
	{0xC2,4,{0x85,0x03,0x14,0x9B}},
	{0x00,1,{0x94}},
	{0xC2,4,{0x84,0x03,0x14,0x9B}},
	{0x00,1,{0xB0}},
	{0xC2,4,{0x03,0x43,0x14,0x9B}},
	{0x00,1,{0xB4}},
	{0xC2,4,{0x02,0x43,0x14,0x9B}},
	{0x00,1,{0xB8}},
	{0xC2,4,{0x01,0x03,0x14,0x9B}},
	{0x00,1,{0xBC}},
	{0xC2,4,{0x02,0x03,0x14,0x9B}},
	{0x00,1,{0xE0}},
	{0xC2,8,{0x83,0x05,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0xE8}},
	{0xC2,8,{0x82,0x06,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0xF0}},
	{0xC2,8,{0x81,0x07,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0xF8}},
	{0xC2,8,{0x00,0x08,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0x80}},
	{0xC3,8,{0x01,0x09,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0x88}},
	{0xC3,8,{0x02,0x0A,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0x90}},
	{0xC3,8,{0x03,0x0B,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0x98}},
	{0xC3,8,{0x04,0x0C,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0xC0}},
	{0xCD,8,{0x05,0x01,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0xC8}},
	{0xCD,8,{0x06,0x02,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0xD0}},
	{0xCD,8,{0x07,0x03,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0xD8}},
	{0xCD,8,{0x08,0x04,0x03,0x14,0x9B,0x00,0x02,0x0B}},
	{0x00,1,{0xF0}},
	{0xCC,6,{0x3D,0x88,0x88,0x14,0x14,0x80}},
	{0x00,1,{0x80}},
	{0xC0,6,{0x00,0xC7,0x00,0xF8,0x00,0x10}},
	{0x00,1,{0x90}},
	{0xC0,6,{0x00,0xC7,0x00,0xF8,0x00,0x10}},
	{0x00,1,{0xA0}},
	{0xC0,6,{0x01,0x8E,0x00,0xF8,0x00,0x10}},
	{0x00,1,{0xB0}},
	{0xC0,5,{0x00,0xC7,0x00,0xF8,0x10}},
	{0x00,1,{0xA3}},
	{0xC1,3,{0x24,0x1A,0x04}},
	{0x00,1,{0x80}},
	{0xCE,1,{0x00}},
	{0x00,1,{0x90}},
	{0xCE,1,{0x00}},
	{0x00,1,{0xD0}},
	{0xCE,8,{0x01,0x00,0x10,0x01,0x01,0x00,0xAD,0x00}},
	{0x00,1,{0xE0}},
	{0xCE,1,{0x00}},
	{0x00,1,{0xF0}},
	{0xCE,1,{0x00}},
	{0x00,1,{0xB0}},
	{0xCF,4,{0x00,0x00,0x3E,0x42}},
	{0x00,1,{0xB5}},
	{0xCF,4,{0x02,0x02,0x9A,0x9E}},
	{0x00,1,{0xC0}},
	{0xCF,4,{0x04,0x04,0xEC,0xF0}},
	{0x00,1,{0xC5}},
	{0xCF,4,{0x00,0x05,0x08,0x3C}},
	{0x00,1,{0x90}},
	{0xC4,1,{0x88}},
	{0x00,1,{0x92}},
	{0xC4,1,{0xC0}},
	{0x00,1,{0xA8}},
	{0xC5,1,{0x09}},
	{0x00,1,{0xCB}},
	{0xC5,1,{0x01}},
	{0x00,1,{0xFD}},
	{0xCB,1,{0x82}},
	{0x00,1,{0x9F}},
	{0xC5,1,{0x00}},
	{0x00,1,{0x91}},
	{0xC5,1,{0x4C}},
	{0x00,1,{0xD7}},
	{0xCE,1,{0x01}},
	{0x00,1,{0x94}},
	{0xC5,1,{0x46}},
	{0x00,1,{0x98}},
	{0xC5,1,{0x64}},
	{0x00,1,{0x9B}},
	{0xC5,1,{0x65}},
	{0x00,1,{0x9D}},
	{0xC5,1,{0x65}},
	{0x00,1,{0x8C}},
	{0xCF,2,{0x40,0x40}},
	{0x00,1,{0x9A}},
	{0xF5,1,{0x35}},
	{0x00,1,{0xA2}},
	{0xF5,1,{0x1F}},
	{0x00,1,{0xA0}},
	{0xC1,1,{0xE0}},
	{0x1C,1,{0x00}},
	{0x00,1,{0xC9}},
	{0xCE,1,{0x00}},
	{0x00,1,{0xB0}},
	{0xCA,5,{0x00,0x00,0x04,0x00,0x06}},//12bit 29.3khz diming32frame
	{0x51,2,{0x00,0x00}},
	{0x53,1,{0x2C}},
	{0x55,1,{0x00}},
	{0x11,0,{}},
	{REGFLAG_DELAY,90, {}},
	{0x29,0,{}},
	{0x35,1,{0x00}},
	{REGFLAG_DELAY,10, {}},
};

static struct LCM_setting_table bl_level[] = {
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
	params->dsi.vertical_backporch = 8;
	params->dsi.vertical_frontporch = 248;
	/* disable dynamic frame rate */
	/* params->dsi.vertical_frontporch_for_low_power = 540; */
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 6;
	params->dsi.horizontal_backporch = 8;
	params->dsi.horizontal_frontporch = 7;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
	/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
	params->dsi.PLL_CLOCK = 251;
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
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(15);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(5);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(5);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(20);

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
	pr_notice("ft8201ab [Kernel/LCM] %s enter\n", __func__);

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
	pr_notice("%s [Kernel/LCM] %s enter\n", LCM_NAME, __func__);

	lcm_init();
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	pr_notice("[LCM]%s,ft8201ab backlight: level = %d\n", __func__, level);

	if (level == 0) {
		pr_notice("[LCM],QC_INX backlight off,diming disable first\n");
		push_table(NULL,
			lcm_diming_disable_setting,
			sizeof(lcm_diming_disable_setting) / sizeof(struct LCM_setting_table),
			1);
	}

	/* Backlight data is mapped from 8 bits to 12 bits,The default scale is 1:16 */
	/* The initial backlight current is 22mA */
	level = mult_frac(level, 2457, 187); //Backlight current setting 22mA -> 18mA
	bl_level[0].para_list[0] = level >> 4;
	bl_level[0].para_list[1] = level & 0xFF;

	push_table(handle, bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER ft8201ab_dt_qunchuang_inx_vdo_fhdplus2408_lcm_drv = {
	.name = "ft8201ab_dt_qunchuang_inx_vdo_fhdplus2408",
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
