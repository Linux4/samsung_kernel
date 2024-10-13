// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#  include <linux/string.h>
#  include <linux/kernel.h>
#endif

#include "lcm_drv.h"
#include <linux/regulator/consumer.h>
#include <linux/string.h>
#include <linux/kernel.h>

#ifdef CONFIG_HQ_SET_LCD_BIAS
#include <lcm_bias.h>
#endif

#ifdef BUILD_LK
#  include <platform/upmu_common.h>
#  include <platform/mt_gpio.h>
#  include <platform/mt_i2c.h>
#  include <platform/mt_pmic.h>
#  include <string.h>
#elif defined(BUILD_UBOOT)
#  include <asm/arch/mt_gpio.h>
#endif

#include <linux/pinctrl/consumer.h>

#ifdef BUILD_LK
#  define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#  define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#  define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#  define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define MDELAY(n)        (lcm_util.mdelay(n))
#define UDELAY(n)        (lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
        lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
        lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)    lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
        lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#  include <linux/kernel.h>
#  include <linux/module.h>
#  include <linux/fs.h>
#  include <linux/slab.h>
#  include <linux/init.h>
#  include <linux/list.h>
#  include <linux/i2c.h>
#  include <linux/irq.h>
#  include <linux/uaccess.h>
#  include <linux/interrupt.h>
#  include <linux/io.h>
#  include <linux/platform_device.h>
#endif

#define LCM_DSI_CMD_MODE  0

#define FRAME_WIDTH            (720)
#define FRAME_HEIGHT          (1600)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH        (70308)
#define LCM_PHYSICAL_HEIGHT      (156240)
#define LCM_DENSITY            (300)

#define REGFLAG_DELAY            0xFFFC
#define REGFLAG_UDELAY            0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD
#define REGFLAG_RESET_LOW        0xFFFE
#define REGFLAG_RESET_HIGH        0xFFFF

#define LCM_ID_FT8057P_BOE 0x60

/*A06 code for AL7160A-1673 by yuli at 20240619 start*/
/*backlight from 46ma to 41ma*/
#define BACKLIGHT_MAX_REG_VAL   (65295*41)
#define BACKLIGHT_MAX_APP_VAL   (255*46)
/*A06 code for AL7160A-1673 by yuli at 20240619 end*/

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[256];
};

static struct LCM_setting_table lcm_diming_disable_setting[] = {
    {0x53, 0x01, {0x24}},
    {REGFLAG_DELAY, 10, {} },
};

static struct LCM_setting_table lcm_aot_suspend_setting[] = {
    {0x28, 0, {} },
    {REGFLAG_DELAY, 20, {} },
    {0x10, 0, {} },
    {REGFLAG_DELAY, 120, {} },

    {0x00, 1, {0x00} },
    {0xF7, 4, {0x5a,0xa5,0x95,0x27} },
    {REGFLAG_DELAY, 2, {} },
};

static struct LCM_setting_table init_setting_cmd[] =
{
    {0x00,1,{0x00}},
    {0xFF,3,{0x80,0x57,0x01}},
    {0x00,1,{0x80}},
    {0xFF,2,{0x80,0x57}},
    {0x00,1,{0x00}},
    {0x2A,4,{0x00,0x00,0x02,0xCF}},
    {0x00,1,{0x00}},
    {0x2B,4,{0x00,0x00,0x06,0x3F}},
    {0x00,1,{0xA3}},
    {0xB3,4,{0x06,0x40,0x00,0x18}},
    {0x00,1,{0x93}},
    {0xC5,1,{0x61}},
    {0x00,1,{0x97}},
    {0xC5,1,{0x61}},
    {0x00,1,{0x9A}},
    {0xC5,1,{0xE9}},
    {0x00,1,{0x9C}},
    {0xC5,1,{0xE9}},
    {0x00,1,{0xB6}},
    {0xC5,2,{0x43,0x43}},
    {0x00,1,{0xB8}},
    {0xC5,2,{0x55,0x55}},
    {0x00,1,{0x00}},
    {0xD8,2,{0x34,0x39}},
    {0x00,1,{0x82}},
    {0xC5,1,{0x55}},
    {0x00,1,{0x83}},
    {0xC5,1,{0x07}},
    {0x00,1,{0x96}},
    {0xF5,1,{0x0D}},
    {0x00,1,{0x86}},
    {0xF5,1,{0x0D}},
    {0x00,1,{0x94}},
    {0xC5,1,{0x15}},
    {0x00,1,{0x9B}},
    {0xC5,1,{0x51}},
    {0x00,1,{0xA3}},
    {0xA5,1,{0x04}},
    {0x00,1,{0x99}},
    {0xCF,1,{0x56}},
    {0x00,1,{0x86}},
    {0xB7,1,{0x80}},
    {0x00,1,{0x00}},
    {0xE1,16,{0x00,0x0C,0x15,0x25,0x2E,0x39,0x4A,0x58,0x5A,0x67,0x6A,0x81,0x81,0x6D,0x6D,0x62}},
    {0x00,1,{0x10}},
    {0xE1,8,{0x5A,0x4E,0x3F,0x35,0x2C,0x1C,0x0F,0x00}},
    {0x00,1,{0x00}},
    {0xE2,16,{0x00,0x0C,0x15,0x25,0x2E,0x39,0x4A,0x58,0x5A,0x67,0x6A,0x81,0x81,0x6D,0x6D,0x62}},
    {0x00,1,{0x10}},
    {0xE2,8,{0x5A,0x4E,0x3F,0x35,0x2C,0x1C,0x0F,0x00}},
    {0x00,1,{0x80}},
    {0xCE,1,{0x00}},
    {0x00,1,{0xD0}},
    {0xCE,1,{0x01}},
    {0x00,1,{0xE0}},
    {0xCE,1,{0x00}},
    {0x00,1,{0xA1}},
    {0xC1,1,{0xCC}},
    {0x00,1,{0xA5}},
    {0xC1,2,{0x00,0x28}},
    {0x00,1,{0x70}},
    {0xC0,6,{0x00,0xFD,0x00,0xB1,0x00,0x1E}},
    {0x00,1,{0xD1}},
    {0xCE,7,{0x00,0x14,0x01,0x01,0x00,0x98,0x01}},
    {0x00,1,{0xE8}},
    {0xCE,4,{0x00,0x98,0x00,0x98}},
    {0x00,1,{0xB0}},
    {0xC0,5,{0x00,0xFD,0x00,0xB2,0x1E}},
    {0x00,1,{0x80}},
    {0xC1,2,{0x00,0x00}},
    {0x00,1,{0x90}},
    {0xC1,1,{0x01}},
    {0x00,1,{0xF1}},
    {0xCF,1,{0x3C}},
    {0x00,1,{0xF5}},
    {0xCF,1,{0x02}},
    {0x00,1,{0xF6}},
    {0xCF,1,{0x3C}},
    {0x00,1,{0xF7}},
    {0xCF,1,{0x11}},
    {0x00,1,{0x70}},
    {0xCF,8,{0x06,0x06,0x62,0x66,0x02,0x02,0x3E,0x42}},
    {0x00,1,{0xC0}},
    {0xCF,4,{0x04,0x04,0x16,0x1A}},
    {0x00,1,{0xC5}},
    {0xCF,4,{0x06,0x06,0x4A,0x4E}},
    {0x00,1,{0x80}},
    {0xCC,16,{0x00,0x26,0x01,0x26,0x02,0x04,0x06,0x08,0x0A,0x0C,0x25,0x1C,0x00,0x00,0x00,0x00}},
    {0x00,1,{0x90}},
    {0xCC,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {0x00,1,{0x80}},
    {0xCD,16,{0x00,0x26,0x01,0x26,0x03,0x05,0x07,0x09,0x0B,0x0D,0x25,0x1C,0x00,0x00,0x00,0x00}},
    {0x00,1,{0x90}},
    {0xCD,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {0x00,1,{0xA0}},
    {0xCC,16,{0x00,0x25,0x01,0x26,0x05,0x03,0x0D,0x0B,0x09,0x07,0x26,0x1C,0x00,0x00,0x00,0x00}},
    {0x00,1,{0xB0}},
    {0xCC,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {0x00,1,{0xA0}},
    {0xCD,16,{0x00,0x25,0x01,0x26,0x04,0x02,0x0C,0x0A,0x08,0x06,0x26,0x1C,0x00,0x00,0x00,0x00}},
    {0x00,1,{0xB0}},
    {0xCD,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {0x00,1,{0x80}},
    {0xCB,16,{0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1}},
    {0x00,1,{0xED}},
    {0xCB,1,{0xCD}},
    {0x00,1,{0x90}},
    {0xCB,16,{0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFC,0x00,0x00,0x00,0x00,0x00}},
    {0x00,1,{0xEE}},
    {0xCB,1,{0x00}},
    {0x00,1,{0x90}},
    {0xC3,1,{0x00}},
    {0x00,1,{0xA0}},
    {0xCB,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {0x00,1,{0xB0}},
    {0xCB,4,{0x55,0x55,0x55,0x55}},
    {0x00,1,{0xC0}},
    {0xCB,4,{0x55,0x55,0x55,0x55}},
    {0x00,1,{0xD2}},
    {0xCB,11,{0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00}},
    {0x00,1,{0xE0}},
    {0xCB,13,{0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83}},
    {0x00,1,{0xFA}},
    {0xCB,2,{0x83,0x00}},
    {0x00,1,{0xEF}},
    {0xCB,1,{0x05}},
    {0x00,1,{0x68}},
    {0xC2,4,{0x8A,0x03,0x00,0x81}},
    {0x00,1,{0x6C}},
    {0xC2,4,{0x89,0x03,0x00,0x81}},
    {0x00,1,{0x70}},
    {0xC2,4,{0x88,0x03,0x00,0x81}},
    {0x00,1,{0x74}},
    {0xC2,4,{0x87,0x03,0x00,0x81}},
    {0x00,1,{0x8C}},
    {0xC2,5,{0x83,0x04,0x14,0x01,0xB2}},
    {0x00,1,{0x91}},
    {0xC2,5,{0x82,0x05,0x14,0x01,0xB2}},
    {0x00,1,{0x96}},
    {0xC2,5,{0x81,0x06,0x14,0x01,0xB2}},
    {0x00,1,{0x9B}},
    {0xC2,5,{0x80,0x07,0x14,0x01,0xB2}},
    {0x00,1,{0xA0}},
    {0xC2,5,{0x87,0x00,0x14,0x01,0xB2}},
    {0x00,1,{0xA5}},
    {0xC2,5,{0x86,0x00,0x14,0x01,0xB2}},
    {0x00,1,{0xAA}},
    {0xC2,5,{0x85,0x00,0x14,0x01,0xB2}},
    {0x00,1,{0xAF}},
    {0xC2,5,{0x84,0x01,0x14,0x01,0xB2}},
    {0x00,1,{0xDC}},
    {0xC2,8,{0x77,0x77,0x77,0x77,0x00,0x00,0x00,0x00}},
    {0x00,1,{0xEA}},
    {0xC2,6,{0x12,0x00,0x0B,0x06,0x02,0x80}},
    {0x00,1,{0x60}},
    {0xC2,6,{0x8A,0x06,0x0B,0x46,0x01,0x80}},
    {0x00,1,{0xFC}},
    {0xCB,2,{0x08,0x40}},
    {0x00,1,{0xFB}},
    {0xC3,4,{0x8C,0x05,0x8C,0x05}},
    {0x00,1,{0x98}},
    {0xC4,1,{0x08}},
    {0x00,1,{0x91}},
    {0xE9,4,{0xFF,0xFF,0xFF,0x00}},
    {0x00,1,{0x85}},
    {0xC4,1,{0x80}},
    {0x00,1,{0x86}},
    {0xA4,1,{0xB6}},
    {0x00,1,{0x95}},
    {0xC4,1,{0x80}},
    {0x00,1,{0xB0}},
    {0xC5,1,{0x00}},
    {0x00,1,{0xB3}},
    {0xC5,1,{0x00}},
    {0x00,1,{0xB2}},
    {0xC5,1,{0x0D}},
    {0x00,1,{0xB5}},
    {0xC5,1,{0x02}},
    {0x00,1,{0xC2}},
    {0xF5,1,{0x42}},
    {0x00,1,{0x81}},
    {0xA4,1,{0x73}},
    {0x00,1,{0x93}},
    {0xC4,1,{0x8F}},
    {0x00,1,{0x80}},
    {0xB3,1,{0x17}},
    {0x00,1,{0xA5}},
    {0xB0,1,{0x1D}},
    {0x00,1,{0xB7}},
    {0xF5,1,{0x1F}},
    {0x00,1,{0xB1}},
    {0xF5,1,{0x3F}},
    {0x00,1,{0xBC}},
    {0xC5,1,{0x20}},
    {0x00,1,{0x96}},
    {0xF5,1,{0x00}},
    {0x00,1,{0xB0}},
    {0xCA,3,{0x00,0x00,0x0C}},
    {0x00,1,{0xB5}},
    {0xCA,1,{0x04}}, // PWM 12bit 26Khz dimming 16frame
    {0x00,1,{0x00}},
    {0xFF,3,{0x00,0x00,0x00}},
    {0x00,1,{0x80}},
    {0xFF,2,{0x00,0x00}},
    {0x51,2,{0x00,0x00}},
    {0x53,1,{0x2C}},
    {0x11,0,{}},
    {REGFLAG_DELAY,90,{}},
    {0x29,0,{}},
    {0x35,1,{0x00}},
    {REGFLAG_DELAY,2,{}},
};

static struct LCM_setting_table bl_level[] = {
    {0x51, 2, {0xFF,0x0F} },
    {REGFLAG_END_OF_TABLE, 0x00, {} }
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
    params->physical_width = LCM_PHYSICAL_WIDTH/1000;
    params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
    params->physical_width_um = LCM_PHYSICAL_WIDTH;
    params->physical_height_um = LCM_PHYSICAL_HEIGHT;
    params->density = LCM_DENSITY;

#if (LCM_DSI_CMD_MODE)
    params->dsi.mode = CMD_MODE;
    params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
    //lcm_dsi_mode = CMD_MODE;
#else
    params->dsi.mode = SYNC_EVENT_VDO_MODE;
    //params->dsi.switch_mode = CMD_MODE;
    //lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
    //params->dsi.mode   = SYNC_PULSE_VDO_MODE;    //SYNC_EVENT_VDO_MODE
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

    params->dsi.vertical_sync_active = 4;
    params->dsi.vertical_backporch = 26;
    /*A06 code for AL7160A-1245 by yuli at 20240520 start*/
    params->dsi.vertical_frontporch = 168;
    //params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 10;
    params->dsi.horizontal_backporch = 422;
    params->dsi.horizontal_frontporch = 470;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;
    // params->dsi.noncont_clock           = 1;
    // params->dsi.noncont_clock_period    = 1;
    params->dsi.ssc_range = 4;
    params->dsi.ssc_disable = 1;
    /* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
    /* this value must be in MTK suggested table */
    params->dsi.PLL_CLOCK = 552;
    /*A06 code for AL7160A-1245 by yuli at 20240520 end*/
    // params->dsi.PLL_CK_VDO = 540;
#else
    params->dsi.pll_div1 = 0;
    params->dsi.pll_div2 = 0;
    params->dsi.fbk_div = 0x1;
#endif

    params->dsi.clk_lp_per_line_enable = 0;
    params->dsi.esd_check_enable = 1;
    params->dsi.customization_esd_check_enable = 1;
    params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
    params->dsi.lcm_esd_check_table[0].count = 1;
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

    params->max_refresh_rate = 60;
    params->min_refresh_rate = 60;
}

static void lcm_bias_enable(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);
    /*A06 code for SR-AL7160A-01-219 by yuli at 20240416 start*/
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
    MDELAY(5);
    /*A06 code for SR-AL7160A-01-219 by yuli at 20240416 end*/
    lcd_bias_set_vspn(ON, VSP_FIRST_VSN_AFTER, 6000);  //open lcd bias
    MDELAY(1);

}

static void lcm_bias_disable(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    lcd_bias_set_vspn(OFF, VSN_FIRST_VSP_AFTER, 6000);  //open lcd bias

}

static void lcm_init_power(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);
    if ((lcd_pinctrl1 == NULL) || (tp_rst_high == NULL)) {
        pr_err("lcd_pinctrl1/tp_rst_high is invaild\n");
        return;
    }

    // pinctrl_select_state(lcd_pinctrl1, tp_rst_high);
    // MDELAY(5);
    lcm_bias_enable();

}

static void lcm_suspend_power(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    if (g_system_is_shutdown) {
        lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
        MDELAY(3);
    }

    lcm_bias_disable();
}

static void lcm_resume_power(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);
    lcm_init_power();
}

static void lcm_init(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(5);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
    /*A06 code for SR-AL7160A-01-219 by yuli at 20240416 start*/
    MDELAY(11);
    /*A06 code for SR-AL7160A-01-219 by yuli at 20240416 end*/
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(20);

    push_table(NULL, init_setting_cmd, ARRAY_SIZE(init_setting_cmd), 1);
    LCM_LOGI("ft8057p_txd_boe----tps6132----lcm mode = vdo mode :%d----\n",lcm_dsi_mode);
}

static void lcm_suspend(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    push_table(NULL, lcm_aot_suspend_setting,
        sizeof(lcm_aot_suspend_setting) / sizeof(struct LCM_setting_table), 1);

    MDELAY(10);
}

static void lcm_resume(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);
    lcm_init();
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
    LCM_LOGI("%s,ft8057p_txd_boe backlight: level = %d\n", __func__, level);

    if (level == 0) {
        pr_notice("[LCM],ft8057p_txd_boe backlight off, diming disable first\n");
        push_table(NULL,
            lcm_diming_disable_setting,
            sizeof(lcm_diming_disable_setting) / sizeof(struct LCM_setting_table),
            1);
    }

    /*A06 code for AL7160A-1673 by yuli at 20240619 start*/
    level = mult_frac(level, BACKLIGHT_MAX_REG_VAL, BACKLIGHT_MAX_APP_VAL);
    bl_level[0].para_list[0] = level >> 8;
    bl_level[0].para_list[1] = level & 0xFF;
    /*A06 code for AL7160A-1673 by yuli at 20240619 end*/
    push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

struct LCM_DRIVER lcd_ft8057p_txd_boe_mipi_hd_video_lcm_drv = {
    .name = "lcd_ft8057p_txd_boe_mipi_hd_video",
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

