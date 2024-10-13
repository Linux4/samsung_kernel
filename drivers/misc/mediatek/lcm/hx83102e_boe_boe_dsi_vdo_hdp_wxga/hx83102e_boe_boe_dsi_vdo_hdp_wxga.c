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
#define FRAME_HEIGHT          (1280)
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

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {
	{REGFLAG_DELAY, 120, {} },
	{0xB9,0x03,{0x83,0x10,0x2E}},
	{0x11, 0x01, {0x00} },
	{REGFLAG_DELAY, 120, {} },
        {0x51,0x02,{0x0F,0xFF}},
        {0x53,0x01,{0x2C}},
        {0xC9,0x04,{0x04,0x0D,0xF0,0x00}},

	{0x29, 0x01, {0x00} },
	{REGFLAG_DELAY, 20, {} },

	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table bl_level[] = {
	{0x51, 2, {0xF,0xFF} },
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
	params->dsi.vertical_frontporch = 80;
	/* disable dynamic frame rate */
	/* params->dsi.vertical_frontporch_for_low_power = 540; */
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 14;
	params->dsi.horizontal_backporch = 26;
	params->dsi.horizontal_frontporch = 32;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_range = 4;
	params->dsi.ssc_disable = 1;
	/* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 270;
#else
	params->dsi.PLL_CLOCK = 218;
#endif
	/* params->dsi.PLL_CK_CMD = 220; */
	/* params->dsi.PLL_CK_VDO = 255; */
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


	/* params->use_gpioID = 1; */
	/* params->gpioID_value = 0; */
}

static unsigned int lcm_esd_check(void)
{
        char buffer[3];
        int array[4];

        array[0] = 0x00013700;
        dsi_set_cmdq(array, 1, 1);

        read_reg_v2(0x0A, buffer, 1);

        if (buffer[0] != 0x9C) {
                pr_debug("[LCM ERROR] [0x53]=0x%02x\n", buffer[0]);
                return TRUE;
        }
        pr_debug("[LCM NORMAL] [0x53]=0x%02x\n", buffer[0]);
        return FALSE;
}


static void lcm_init_power(void)
{
	display_bias_enable();
}

static void lcm_suspend_power(void)
{
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
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
	MDELAY(50);

	pinctrl_select_state(lcd_pinctrl1, lcd_disp_pwm);

	push_table(NULL,
		init_setting_vdo,
		sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table),
		1);
	pr_notice("[Kernel/LCM] %s exit\n", __func__);
}

static void lcm_suspend(void)
{
	pr_notice("hx83102E [Kernel/LCM] %s enter\n", __func__);

	push_table(NULL,
		lcm_suspend_setting,
		sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
		1);
	MDELAY(10);

	pinctrl_select_state(lcd_pinctrl1, lcd_disp_pwm_gpio);

	MDELAY(2);
	lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
}

static void lcm_resume(void)
{
	pr_notice("hx83102E [Kernel/LCM] %s enter\n", __func__);

	lcm_init();
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	printk("[LCM]%s,hx83102e backlight: level = %d\n", __func__, level);

	level = ((level + 1) << 4 ) - 1;
        bl_level[0].para_list[0] = level >> 8;
        bl_level[0].para_list[1] = level & 0xFF;

	push_table(handle, bl_level,
		sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}


static unsigned int lcm_ata_check(unsigned char *buffer)
{
        unsigned int ret = 0;
        unsigned int x0 = FRAME_WIDTH / 4;

        unsigned char x0_MSB = (x0 & 0xFF);

        unsigned int data_array[2];
        unsigned char read_buf[2];
        unsigned int num1 = 0, num2 = 0;

        struct LCM_setting_table switch_table_page0[] = {
                { 0xFF, 0x03, {0x98, 0x81, 0x00} }
        };

        struct LCM_setting_table switch_table_page2[] = {
                { 0xFF, 0x03, {0x98, 0x81, 0x02} }
        };


        MDELAY(20);

        num1 = sizeof(switch_table_page2) / sizeof(struct LCM_setting_table);

        push_table(NULL, switch_table_page2, num1, 1);
        pr_debug("before read ATA check x0_MSB = 0x%x\n", x0_MSB);
        pr_debug("before read ATA check read buff = 0x%x\n", read_buf[0]);

        data_array[0] = 0x00023902;
        data_array[1] = x0_MSB << 8 | 0x30;

        dsi_set_cmdq(data_array, 2, 1);

        data_array[0] = 0x00013700;
        dsi_set_cmdq(data_array, 1, 1);

        read_reg_v2(0x30, read_buf, 1);

        pr_debug("after read ATA check size = 0x%x\n", read_buf[0]);

        num2 = sizeof(switch_table_page0) / sizeof(struct LCM_setting_table);

        push_table(NULL, switch_table_page0, num2, 1);

        if (read_buf[0] == x0_MSB)
                ret = 1;
        else
                ret = 0;

        return ret;
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

struct LCM_DRIVER hx83102e_boe_boe_dsi_vdo_hdp_wxga_lcm_drv = {
	.name = "hx83102e_boe_boe_dsi_vdo_hdp_wxga",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.ata_check = lcm_ata_check,
	.esd_check = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.update = lcm_update,
};

