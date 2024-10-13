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

#define LCM_ID_GC7272 0x50

/*A06 code for AL7160A-1673 by yuli at 20240619 start*/
/*backlight from 46ma to 44ma*/
#define BACKLIGHT_MAX_REG_VAL   (4095*44)
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
    //gesture disable
    {0xFF,3,{0x55,0xAA,0x66}},
    {0xFF,1,{0x10}},
    {0x28,1,{0x00}},
    {REGFLAG_DELAY, 50, {}},
    // Sleep Mode On
    {0x10,1,{0x00}},
    {REGFLAG_DELAY, 120, {}}, //120ms
    {0x4F,1,{0x01}},//DSTB
    {REGFLAG_DELAY, 10, {}},

};

static struct LCM_setting_table lcm_suspend_setting[] = {
    //gesture enable
    {0xFF,3,{0x55,0xAA,0x66}},

    {0xFF,1,{0x10}},
    {0x28,1,{0x00}},
    {REGFLAG_DELAY, 50, {}},
    // Sleep Mode On
    {0x10,1,{0x00}},
    {REGFLAG_DELAY, 120, {}}, //120ms

};

static struct LCM_setting_table init_setting_cmd[] =
{
    {0xFF,3,{0x55,0xAA,0x66}},
    {0xFF,1,{0x10}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x20}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x21}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x22}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x23}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x24}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x25}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x27}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x26}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x28}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0xB3}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0xA3}},
    {0xFB,1,{0x00}},
    //红区
    {0xFF,1,{0x20}},
    {0x2D,1,{0xAA}},
    {0xA3,1,{0x12}},
    {0xA7,1,{0x12}},
    {0xFF,1,{0xB3}},
    {0x4E,1,{0x46}},
    {0xFF,1,{0xB3}},
    {0x3F,1,{0x37}},
    {0x50,1,{0x10}},
    {0xFF,1,{0x20}},
    {0x29,1,{0xE0}},
    {0x20,1,{0x84}},
    {0x21,1,{0x00}},
    {0x22,1,{0x80}},
    {0x2E,1,{0x00}},
    {0xFF,1,{0xA3}},
    {0x58,1,{0xAA}},
    {0xFF,1,{0x26}},
    {0x43,1,{0x00}},
    {0xFF,1,{0x22}},
    {0x0E,1,{0x33}},
    {0x0C,1,{0x00}},
    {0x0B,1,{0x00}},
    {0xFF,1,{0x28}},
    {0x7D,1,{0x1F}},
    {0x7B,1,{0x40}},
    {0xFF,1,{0xB3}},
    {0x47,1,{0x01}},
    //黄区
    {0xFF,1,{0x22}},
    {0xE4,1,{0x00}},
    {0x01,1,{0x06}},
    {0x02,1,{0x40}},
    {0x03,1,{0x00}},
    {0xFF,1,{0x20}},
    {0xC3,1,{0x00}},
    {0xC4,1,{0xA7}},
    {0xC5,1,{0x00}},
    {0xC6,1,{0xA7}},
    {0xB3,1,{0x00}},
    {0xB6,1,{0xC8}},
    {0xB4,1,{0x10}},
    {0xB5,1,{0x00}},
    {0xD3,1,{0x13}},
    {0xFF,1,{0x22}},
    {0x25,1,{0x06}},
    {0x26,1,{0x00}},
    {0x2E,1,{0x6E}},
    {0x2F,1,{0x00}},
    {0x36,1,{0x07}},
    {0x37,1,{0x00}},
    {0x3F,1,{0x6E}},
    {0x40,1,{0x00}},
    {0xFF,1,{0x28}},
    {0x01,1,{0x1A}},
    {0x02,1,{0x1A}},
    {0x03,1,{0x1A}},
    {0x04,1,{0x25}},
    {0x05,1,{0x18}},
    {0x06,1,{0x19}},
    {0x07,1,{0x02}},
    {0x08,1,{0x21}},
    {0x09,1,{0x20}},
    {0x0A,1,{0x08}},
    {0x0B,1,{0x0A}},
    {0x0C,1,{0x0C}},
    {0x0D,1,{0x25}},
    {0x0E,1,{0x0E}},
    {0x0F,1,{0x00}},
    {0x10,1,{0x1A}},
    {0x11,1,{0x1A}},
    {0x12,1,{0x1A}},
    {0x13,1,{0x1A}},
    {0x14,1,{0x25}},
    {0x15,1,{0x25}},
    {0x16,1,{0x25}},
    {0x17,1,{0x1A}},
    {0x18,1,{0x1A}},
    {0x19,1,{0x1A}},
    {0x1A,1,{0x25}},
    {0x1B,1,{0x18}},
    {0x1C,1,{0x19}},
    {0x1D,1,{0x03}},
    {0x1E,1,{0x21}},
    {0x1F,1,{0x20}},
    {0x20,1,{0x09}},
    {0x21,1,{0x0B}},
    {0x22,1,{0x0D}},
    {0x23,1,{0x25}},
    {0x24,1,{0x0F}},
    {0x25,1,{0x01}},
    {0x26,1,{0x1A}},
    {0x27,1,{0x1A}},
    {0x28,1,{0x1A}},
    {0x29,1,{0x1A}},
    {0x2A,1,{0x25}},
    {0x2B,1,{0x25}},
    {0x2D,1,{0x25}},
    {0xFF,1,{0x28}},
    {0x30,1,{0x00}},
    {0x31,1,{0x55}},
    {0x34,1,{0x00}},
    {0x35,1,{0x55}},
    {0x36,1,{0x00}},
    {0x37,1,{0x55}},
    {0x38,1,{0x00}},
    {0x39,1,{0x0a}},
    {0x3A,1,{0x01}},
    {0x3B,1,{0x01}},
    {0x2F,1,{0x00}},
    {0xFF,1,{0x21}},
    {0x7E,1,{0x03}},
    {0x7F,1,{0x22}},
    {0x8B,1,{0x22}},
    {0x80,1,{0x02}},
    {0x8C,1,{0x19}},
    {0x81,1,{0x19}},
    {0x8D,1,{0x02}},
    {0xAF,1,{0x70}},
    {0xB0,1,{0x70}},
    {0x83,1,{0x03}},
    {0x8F,1,{0x03}},
    {0x87,1,{0x21}},
    {0x93,1,{0x00}},
    {0x82,1,{0x70}},
    {0x8E,1,{0x70}},
    {0x2B,1,{0x00}},
    {0x2E,1,{0x00}},
    {0x88,1,{0xB7}},
    {0x89,1,{0x20}},
    {0x8A,1,{0x23}},
    {0x94,1,{0xB7}},
    {0x95,1,{0x20}},
    {0x96,1,{0x23}},
    {0x45,1,{0x0f}},
    {0x46,1,{0x73}},
    {0x4C,1,{0x73}},
    {0x52,1,{0x73}},
    {0x58,1,{0x73}},
    {0x47,1,{0x03}},
    {0x4D,1,{0x02}},
    {0x53,1,{0x20}},
    {0x59,1,{0x21}},
    {0x48,1,{0x21}},
    {0x4E,1,{0x20}},
    {0x54,1,{0x02}},
    {0x5a,1,{0x03}},
    {0x76,1,{0x40}},
    {0x77,1,{0x40}},
    {0x78,1,{0x40}},
    {0x79,1,{0x40}},
    {0x49,1,{0x04}},
    {0x4A,1,{0x98}},
    {0x4F,1,{0x04}},
    {0x50,1,{0x98}},
    {0x55,1,{0x04}},
    {0x56,1,{0x98}},
    {0x5b,1,{0x04}},
    {0x5c,1,{0x98}},
    {0x84,1,{0x04}},
    {0x90,1,{0x04}},
    {0x85,1,{0x98}},
    {0x91,1,{0x98}},
    {0x08,1,{0x03}},
    {0xDC,1,{0xA0}},
    {0xE0,1,{0x80}},
    {0xE1,1,{0xA0}},
    {0xE2,1,{0x80}},
    {0xbe,1,{0x03}},
    {0xbf,1,{0x77}},
    {0xc0,1,{0x53}},
    {0xc1,1,{0x40}},
    {0xC2,1,{0x44}},
    {0xCA,1,{0x03}},
    {0xCB,1,{0x3c}},
    {0xCD,1,{0x3c}},
    {0xCC,1,{0x70}},
    {0xCE,1,{0x70}},
    {0xCF,1,{0x72}},
    {0xD0,1,{0x75}},
    {0xC4,1,{0x87}},
    {0xC5,1,{0x87}},
    {0x18,1,{0x40}},
    {0xFF,1,{0x22}},
    {0x05,1,{0x00}},
    {0x08,1,{0x22}},
    //绿区
    {0xFF,1,{0x28}},
    {0x3D,1,{0x70}},
    {0x3E,1,{0x70}},
    {0x3F,1,{0x30}},
    {0x40,1,{0x30}},
    {0x45,1,{0x75}},
    {0x46,1,{0x75}},
    {0x47,1,{0x2c}},
    {0x48,1,{0x2c}},
    {0x4D,1,{0xA2}},
    {0x50,1,{0x2a}},
    {0x52,1,{0x71}},
    {0x53,1,{0x22}},
    {0x56,1,{0x12}},
    {0x57,1,{0x20}},
    {0x5A,1,{0x98}},
    {0x5B,1,{0x9C}},
    // {0x62,1,{0x9c}},
    // {0x63,1,{0x9c}},
    {0xFF,1,{0x20}},
    {0x7E,1,{0x03}},
    {0x7F,1,{0x00}},
    {0x80,1,{0x64}},
    {0x81,1,{0x00}},
    {0x82,1,{0x00}},
    {0x83,1,{0x64}},
    {0x84,1,{0x64}},
    {0x85,1,{0x41}},
    {0x86,1,{0x40}},
    {0x87,1,{0x41}},
    {0x88,1,{0x40}},
    {0x8A,1,{0x0A}},
    {0x8B,1,{0x0A}},
    {0xFF,1,{0x25}},
    {0x75,1,{0x00}},
    {0x76,1,{0x00}},
    {0x77,1,{0x64}},
    {0x78,1,{0x00}},
    {0x79,1,{0x64}},
    {0x7A,1,{0x64}},
    {0x7B,1,{0x64}},
    {0x7C,1,{0x32}},
    {0x7D,1,{0x9B}},
    {0x7E,1,{0x32}},
    {0x7F,1,{0x9B}},
    {0x80,1,{0x00}},
    {0x81,1,{0x03}},
    {0x82,1,{0x03}},
    {0xFF,1,{0x23}},
    {0xEF,1,{0x0B}},//V0B
    {0x29,1,{0x03}},
    {0x01,16,{0x00,0x22,0x00,0x34,0x00,0x4f,0x00,0x6c,0x00,0x84,0x00,0x9a,0x00,0xaa,0x00,0xbb}},
    {0x02,16,{0x00,0xCa,0x00,0xFa,0x01,0x18,0x01,0x48,0x01,0x6F,0x01,0xA8,0x01,0xE3,0x01,0xE5}},
    {0x03,16,{0x02,0x14,0x02,0x57,0x02,0x87,0x02,0xC8,0x02,0xF2,0x03,0x29,0x03,0x42,0x03,0x51}},
    {0x04,12,{0x03,0x5C,0x03,0x79,0x03,0x97,0x03,0xB8,0x03,0xE8,0x03,0xFF}},
    {0x0D,16,{0x00,0x22,0x00,0x34,0x00,0x4f,0x00,0x6c,0x00,0x84,0x00,0x9a,0x00,0xaa,0x00,0xbb}},
    {0x0E,16,{0x00,0xCa,0x00,0xFa,0x01,0x18,0x01,0x48,0x01,0x6F,0x01,0xA8,0x01,0xE3,0x01,0xE5}},
    {0x0F,16,{0x02,0x14,0x02,0x57,0x02,0x87,0x02,0xC8,0x02,0xF2,0x03,0x29,0x03,0x42,0x03,0x51}},
    {0x10,12,{0x03,0x5C,0x03,0x79,0x03,0x97,0x03,0xB8,0x03,0xE8,0x03,0xFF}},
    {0xF0,1,{0x01}},
    {0x2D,1,{0x65}},
    {0x2E,1,{0x00}},
    {0x2B,1,{0x41}},
    {0x32,1,{0x02}},
    {0x33,1,{0x18}},
    {0xFF,1,{0x20}},
    {0xF1,1,{0xE0}},
    {0xF2,1,{0x45}},
    {0xFF,1,{0x22}},
    {0xDC,1,{0x00}},
    {0xE0,1,{0x01}},
    {0xFF,1,{0xA3}},
    {0x45,1,{0x11}},
    {0xFF,1,{0x20}},
    {0x4A,1,{0x04}},
    {0x24,1,{0x0A}},
    {0x48,1,{0x10}},
    {0x49,1,{0x00}},
    {0xFF,1,{0x22}},
    {0x0D,1,{0x02}},
    {0xFF,1,{0xB3}},
    {0x42,1,{0x05}},
    {0x43,1,{0x12}},
    {0x44,1,{0x52}},
    {0xFF,1,{0x10}},
    {0x51,2,{0x00,0x00}},
    {0x53,1,{0x2c}},
    {0x55,1,{0x00}},
    {0x36,1,{0x08}},
    {0x69,1,{0x00}},
    {0x35,1,{0x00}},
    {0xBA,1,{0x03}},
    {0x11,0,{}},
    {REGFLAG_DELAY,120, {}},
    {0x29,0,{}},
    {REGFLAG_DELAY,20, {}},
};

static struct LCM_setting_table bl_level[] = {
    {0x51, 2, {0x0F,0xF0} },//{0x51, 2, {0x0F,0xFF} }
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
    params->dsi.mode = SYNC_PULSE_VDO_MODE;
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

    params->dsi.vertical_sync_active = 6;
    params->dsi.vertical_backporch = 28;
    params->dsi.vertical_frontporch = 140;
    //params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 310;
    params->dsi.horizontal_backporch = 300;
    params->dsi.horizontal_frontporch = 315;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;
    // params->dsi.noncont_clock           = 1;
    // params->dsi.noncont_clock_period    = 1;
    params->dsi.ssc_range = 4;
    params->dsi.ssc_disable = 1;
    /* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
    /* this value must be in MTK suggested table */
    params->dsi.PLL_CLOCK = 552;//504
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
        /*A06 code for AL7160A-65 by wenghailong at 20240415 start*/
        MDELAY(50);
        /*A06 code for AL7160A-65 by wenghailong at 20240415 end*/
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
    MDELAY(10);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
    MDELAY(10);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(10);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
    MDELAY(10);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(10);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
    MDELAY(10);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(10);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
    MDELAY(10);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(25);


    push_table(NULL, init_setting_cmd, ARRAY_SIZE(init_setting_cmd), 1);
    LCM_LOGI("gc7272_xx_hsd----tps6132----lcm mode = vdo mode :%d----\n",lcm_dsi_mode);
}

static void lcm_suspend(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    if (!smart_wakeup_open_flag) {
        push_table(NULL,
        lcm_aot_suspend_setting,
        sizeof(lcm_aot_suspend_setting) / sizeof(struct LCM_setting_table),
        1);
    } else {
        push_table(NULL,
        lcm_suspend_setting,
        sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table),
        1);
    }

    MDELAY(10);
}

static void lcm_resume(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);
    lcm_init();
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
    LCM_LOGI("%s,gc7272_xx_hsd backlight: level = %d\n", __func__, level);

    if (level == 0) {
        pr_notice("[LCM],gc7272_xx_hsd backlight off, diming disable first\n");
        push_table(NULL,
            lcm_diming_disable_setting,
            sizeof(lcm_diming_disable_setting) / sizeof(struct LCM_setting_table),
            1);
    }

    level = mult_frac(level, BACKLIGHT_MAX_REG_VAL, BACKLIGHT_MAX_APP_VAL);
    bl_level[0].para_list[0] = level >> 8;
    bl_level[0].para_list[1] = level & 0xFF;
    push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

struct LCM_DRIVER lcd_gc7272_xx_hsd_mipi_hd_video_lcm_drv = {
    .name = "lcd_gc7272_xx_hsd_mipi_hd_video",
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

