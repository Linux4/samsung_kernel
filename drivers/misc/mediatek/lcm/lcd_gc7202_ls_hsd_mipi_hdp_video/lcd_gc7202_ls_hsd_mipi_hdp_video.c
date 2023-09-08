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
#define LCM_PHYSICAL_HEIGHT      (150960)

#define REGFLAG_DELAY          0xFFFC
#define REGFLAG_UDELAY          0xFFFB
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
    {0x53, 0x01, {0x28}}
};

/* HS03S code added for DEVAL5626-286 by fengzhigang at 20210709 start */
static struct LCM_setting_table lcm_aot_suspend_setting[] = {
    {0xFF,3,{0x55,0xAA,0x66}},
    {0xFF,1,{0x27}},
    {0x03,6,{0x40,0x10,0x08,0x00,0x00,0x00}},
    {0x04,13,{0x03,0x24,0x80,0x44,0x4C,0x54,0xAD,0x05,0x00,0x00,0x1C,0x00,0x00}},
    {0xFF,1,{0x20}},
    {0x4A,1,{0x01}},
    {0x48,1,{0x10}},
    {0x49,1,{0x00}},
    {0xFF,1,{0xA3}},
    {0x0F,1,{0x01}},
    {0x0E,1,{0x03}},
    {0xFF,1,{0x10}},
    {0x28,1,{0x00}},
    //{REGFLAG_DELAY, 50, {} },
    {0x10,1,{0x00}},
    {REGFLAG_DELAY, 120, {} },
    {0xFF,1,{0xA3}},
    {0x0F,1,{0x00}},
    {0x0E,1,{0x00}},
    {REGFLAG_DELAY, 10, {} }
};
/* HS03S code added for DEVAL5626-286 by fengzhigang at 20210709 end */

static struct LCM_setting_table lcm_suspend_setting[] = {
    {0xFF,3,{0x55,0xAA,0x66}},
    {0xFF,1,{0x27}},
    {0x03,6,{0x40,0x10,0x08,0x00,0x00,0x00}},
    {0x04,13,{0x03,0x24,0x80,0x44,0x4C,0x54,0xAD,0x05,0x00,0x00,0x1C,0x00,0x00}},
    {0xFF,1,{0x20}},
    {0x4A,1,{0x01}},
    {0x48,1,{0x10}},
    {0x49,1,{0x00}},
    {0xFF,1,{0xA3}},
    {0x0F,1,{0x01}},
    {0x0E,1,{0x03}},
    {0xFF,1,{0x10}},
    {0x28,1,{0x00}},
    {0x10,1,{0x00}},
    {REGFLAG_DELAY, 120, {} },
    {0xFF,1,{0x26}},
    {0x1D,2,{0x2A,0x80}},
    {0xFF,1,{0xB3}},
    {0x04,1,{0x16}},
    {0xFF,1,{0xB3}},
    {0x01,1,{0x99}},
    {0x10,1,{0x09}},
    {0xFF,1,{0xC3}},
    {0x12,1,{0x01}},
    {REGFLAG_DELAY, 10, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {
    {0xFF,3,{0x55,0xAA,0x66}},
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
    {0xFF,1,{0x26}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x27}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0xA3}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0xB3}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0xC3}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x10}},
    {0xFB,1,{0x00}},
    {0xFF,1,{0x20}},
    {0x18,1,{0x44}},
    {0x17,1,{0x51}},
    {0x15,1,{0x5D}},
    {0x12,1,{0xA1}},
    {0xA5,1,{0x00}},
    {0xA6,1,{0xFF}},
    {0xA9,1,{0x00}},
    {0xAA,1,{0xFF}},
    {0xD3,1,{0x05}},
    {0x2D,1,{0x1F}},
    {0xFF,1,{0xB3}},
    {0x58,1,{0x84}},
    {0x82,1,{0x1A}},
    {0xFF,1,{0x22}},
    {0x1F,1,{0x06}},
    {0xFF,1,{0xC3}},
    {0x06,1,{0x44}},
    {0xFF,1,{0x20}},
    {0xA3,1,{0x44}},
    {0xA7,1,{0x44}},
    {0xFF,1,{0xB3}},
    {0x3F,1,{0x37}},
    {0x5E,1,{0x10}},
    {0xFF,1,{0x22}},
    {0xE4,1,{0x00}},
    {0x01,1,{0x06}},
    {0x02,1,{0x40}},
    {0x25,1,{0x08}},
    {0x26,1,{0x00}},
    {0x2E,1,{0x85}},
    {0x2F,1,{0x00}},
    {0x36,1,{0x09}},
    {0x37,1,{0x00}},
    {0x3F,1,{0x85}},
    {0x40,1,{0x00}},
    {0xFF,1,{0x21}},
    {0x01,1,{0x1A}},
    {0x02,1,{0x1A}},
    {0x03,1,{0x1A}},
    {0x04,1,{0x25}},
    {0x05,1,{0x25}},
    {0x06,1,{0x18}},
    {0x07,1,{0x19}},
    {0x08,1,{0x20}},
    {0x09,1,{0x21}},
    {0x0A,1,{0x08}},
    {0x0B,1,{0x0A}},
    {0x0C,1,{0x0C}},
    {0x0D,1,{0x0E}},
    {0x0E,1,{0x00}},
    {0x0F,1,{0x02}},
    {0x10,1,{0x1B}},
    {0x11,1,{0x1B}},
    {0x12,1,{0x1B}},
    {0x13,1,{0x1B}},
    {0x14,1,{0x25}},
    {0x15,1,{0x25}},
    {0x16,1,{0x25}},
    {0x17,1,{0x1A}},
    {0x18,1,{0x1A}},
    {0x19,1,{0x1A}},
    {0x1A,1,{0x25}},
    {0x1B,1,{0x25}},
    {0x1C,1,{0x18}},
    {0x1D,1,{0x19}},
    {0x1E,1,{0x20}},
    {0x1F,1,{0x21}},
    {0x20,1,{0x09}},
    {0x21,1,{0x0B}},
    {0x22,1,{0x0D}},
    {0x23,1,{0x0F}},
    {0x24,1,{0x01}},
    {0x25,1,{0x03}},
    {0x26,1,{0x1B}},
    {0x27,1,{0x1B}},
    {0x28,1,{0x1B}},
    {0x29,1,{0x1B}},
    {0x2A,1,{0x25}},
    {0x2B,1,{0x25}},
    {0x2D,1,{0x25}},
    {0x7E,1,{0x03}},
    {0x7F,1,{0x23}},
    {0x8B,1,{0x23}},
    {0x80,1,{0x03}},
    {0x8C,1,{0x18}},
    {0x81,1,{0x58}},
    {0x8D,1,{0x43}},
    {0xAF,1,{0x40}},
    {0xB0,1,{0x40}},
    {0x83,1,{0x02}},
    {0x8F,1,{0x02}},
    {0x84,1,{0x47}},
    {0x90,1,{0x47}},
    {0x85,1,{0x55}},
    {0x91,1,{0x55}},
    {0x87,1,{0x22}},
    {0x93,1,{0x02}},
    {0x82,1,{0x70}},
    {0x8E,1,{0x70}},
    {0x2E,1,{0x00}},
    {0x88,1,{0xB7}},
    {0x89,1,{0x22}},
    {0x8A,1,{0x53}},
    {0x94,1,{0xB7}},
    {0x95,1,{0x22}},
    {0x96,1,{0x53}},
    {0x45,1,{0x0F}},
    {0x46,1,{0x73}},
    {0x4C,1,{0x73}},
    {0x52,1,{0x73}},
    {0x58,1,{0x73}},
    {0x47,1,{0x04}},
    {0x4D,1,{0x03}},
    {0x53,1,{0x28}},
    {0x59,1,{0x20}},
    {0x48,1,{0x60}},
    {0x4E,1,{0x68}},
    {0x54,1,{0x43}},
    {0x5A,1,{0x44}},
    {0x76,1,{0x40}},
    {0x77,1,{0x40}},
    {0x78,1,{0x40}},
    {0x79,1,{0x40}},
    {0x49,1,{0x55}},
    {0x4A,1,{0x55}},
    {0x4F,1,{0x55}},
    {0x50,1,{0x55}},
    {0x55,1,{0x55}},
    {0x56,1,{0x55}},
    {0x5B,1,{0x55}},
    {0x5C,1,{0x55}},
    {0xBE,1,{0x53}},
    {0xC0,1,{0x54}},
    {0xC1,1,{0x40}},
    {0xBF,1,{0x77}},
    {0xC2,1,{0x97}},
    {0xC3,1,{0x97}},
    {0xC6,1,{0x44}},
    {0xC7,1,{0x44}},
    {0xCA,1,{0x53}},
    {0xCB,1,{0x3C}},
    {0xCD,1,{0x3C}},
    {0xCC,1,{0x20}},
    {0xCE,1,{0x20}},
    {0xCF,1,{0x75}},
    {0xD0,1,{0x70}},
    {0xD1,1,{0x1D}},
    {0xFF,1,{0x22}},
    {0x05,1,{0x10}},
    {0x08,1,{0x32}},
    {0xFF,1,{0x20}},
    {0x25,1,{0x12}},
    {0xFF,1,{0x20}},
    {0xC3,1,{0x00}},
    {0xC4,1,{0x5F}},
    {0xC5,1,{0x00}},
    {0xC6,1,{0x5F}},
    {0xB3,1,{0x00}},
    {0xB4,1,{0x20}},
    {0xB5,1,{0x00}},
    {0xB6,1,{0xDC}},
    {0xFF,1,{0x20}},
    {0x02,1,{0x46}},
    {0x03,1,{0x46}},
    {0x04,1,{0x32}},
    {0x05,1,{0x32}},
    {0x0A,1,{0x44}},
    {0x0B,1,{0x44}},
    {0x0C,1,{0x30}},
    {0x0D,1,{0x30}},
    {0x77,1,{0x96}},
    {0x78,1,{0x98}},
    {0x7E,1,{0x01}},
    {0x7F,1,{0x00}},
    {0x80,1,{0x01}},
    {0x81,1,{0x00}},
    {0x82,1,{0x00}},
    {0x83,1,{0x04}},
    {0x84,1,{0x04}},
    {0x85,1,{0x01}},
    {0x86,1,{0x87}},
    {0x87,1,{0x01}},
    {0x88,1,{0x87}},
    {0x8A,1,{0x03}},
    {0x8B,1,{0x03}},
    {0xFF,1,{0x23}},
    {0x25,1,{0x03}},
    {0x01,16,{0x00,0x00,0x00,0x21,0x00,0x54,0x00,0x72,0x00,0x88,0x00,0x9A,0x00,0xAA,0x00,0xBB}},
    {0x02,16,{0x00,0xC6,0x00,0xEC,0x01,0x1C,0x01,0x4c,0x01,0x70,0x01,0xA5,0x01,0xD2,0x01,0xD4}},
    {0x03,16,{0x02,0x06,0x02,0x41,0x02,0x72,0x02,0xA8,0x02,0xD0,0x03,0x07,0x03,0x1D,0x03,0x2e}},
    {0x04,12,{0x03,0x44,0x03,0x5e,0x03,0x7d,0x03,0xAB,0x03,0xD7,0x03,0xFF}},
    {0x0D,16,{0x00,0x00,0x00,0x21,0x00,0x54,0x00,0x72,0x00,0x88,0x00,0x9A,0x00,0xAA,0x00,0xBB}},
    {0x0E,16,{0x00,0xC6,0x00,0xEC,0x01,0x1C,0x01,0x4c,0x01,0x70,0x01,0xA5,0x01,0xD2,0x01,0xD4}},
    {0x0F,16,{0x02,0x06,0x02,0x41,0x02,0x72,0x02,0xA8,0x02,0xD0,0x03,0x07,0x03,0x1D,0x03,0x2e}},
    {0x10,12,{0x03,0x44,0x03,0x5e,0x03,0x7d,0x03,0xAB,0x03,0xD7,0x03,0xFF}},
    {0x2D,1,{0x65}},
    {0x37,1,{0x01}},
    {0x2E,1,{0x00}},
    {0x32,1,{0x01}},
    {0x33,1,{0x05}},
    {0xB9,1,{0x31}},
    {0xB8,1,{0xb7}},
    {0x35,1,{0x16}},
    {0x36,1,{0x0C}},
    {0x30,1,{0x00}},
    {0x1F,16,{0x0a,0x00,0x20,0x00,0x44,0x00,0x70,0x00,0xA8,0x00,0xe6,0x00,0x0a,0x00,0x20,0x00}},
    {0x20,8,{0x44,0x00,0x70,0x00,0xA8,0x00,0xFF,0x00}},
    {0x21,16,{0xa7,0x00,0xb2,0x00,0xc0,0x00,0xcd,0x00,0xdA,0x00,0xf3,0x00,0xD8,0x00,0xD8,0x00}},
    {0x22,8,{0xd8,0x00,0xd8,0x00,0xd8,0x00,0xff,0x00}},
    {0x23,16,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {0x24,8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {0xFF,1,{0xA3}},
    {0xFA,1,{0x01}},
    {0x05,1,{0x05}},
    {0xFF,1,{0x24}},
    {0x03,2,{0x01,0xdd}},
    {0x50,1,{0xaa}},
    {0xFF,1,{0x22}},
    {0xEF,1,{0x0a}},
    {0xFF,1,{0x20}},
    {0x50,1,{0x0e}},
    {0x51,1,{0x22}},
    {0xFF,1,{0xC3}},
    {0x0D,1,{0x00}},
    {0x0E,1,{0x8D}},
    {0x13,1,{0x07}},
    {0xFF,1,{0x10}},
    {0x51,2,{0x00,0x00}},
    {0x53,1,{0x2C}},
    {0x55,1,{0x00}},
    {0x36,1,{0x08}},
    {0x69,1,{0x00}},
    {0x35,1,{0x00}},
    {0xBA,1,{0x03}},
    {0x11, 0, {0x00}},
    {REGFLAG_DELAY,120,{}},

    {0xFF,1,{0x27}},
    {0x03,6,{0x40,0x10,0x08,0x00,0xF3,0x0C}},
    {0x04,13,{0x03,0x24,0x80,0x44,0x4C,0x54,0xAD,0x05,0x00,0x00,0x1C,0xE0,0x10}},

    {0xFF,1,{0x10}},
    {0x29, 0, {0x00}},
    {REGFLAG_DELAY,20,{}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table bl_level[] = {
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
            if (table[i].count <= 10) {
                MDELAY(table[i].count);
            } else {
                MDELAY(table[i].count);
            }
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
    params->dsi.switch_mode = BURST_VDO_MODE;
    /* lcm_dsi_mode = CMD_MODE; */
#else
    params->dsi.mode = BURST_VDO_MODE;
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

    params->dsi.vertical_sync_active = 6;
    params->dsi.vertical_backporch = 28;
    params->dsi.vertical_frontporch = 160;
    /* disable dynamic frame rate */
    /* params->dsi.vertical_frontporch_for_low_power = 540; */
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 8;
    params->dsi.horizontal_backporch = 40;
    params->dsi.horizontal_frontporch = 40;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;
    params->dsi.ssc_range = 4;
    params->dsi.ssc_disable = 1;
    /* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
    params->dsi.PLL_CLOCK = 270;
#else
    params->dsi.PLL_CLOCK = 276;
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
    params->dsi.customization_esd_check_enable = 0;
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
#else
    MDELAY(2);
    display_bias_enable();
#endif
    MDELAY(15);
/* HS03S code added for SR-AL5625-01-506 by gaozhengwei at 20210526 end */
}

static void lcm_suspend_power(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    if (g_system_is_shutdown) {
        lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
        MDELAY(3);
    }

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

    /* hs03s_NM code added for SR-AL5625-01-627 by fengzhigang at 20220419 start */
    if (mtk_tpd_smart_wakeup_support()) {
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
    /* hs03s_NM code added for SR-AL5625-01-627 by fengzhigang at 20220419 end */

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
    LCM_LOGI("%s,gc7202_ls_hsd backlight: level = %d\n", __func__, level);

    if (level == 0) {
        pr_notice("[LCM],gc7202_ls_hsd backlight off, diming disable first\n");
        push_table(NULL,
            lcm_diming_disable_setting,
            sizeof(lcm_diming_disable_setting) / sizeof(struct LCM_setting_table),
            1);
    }

    /* Backlight data is mapped from 8 bits to 12 bits,The default scale is 17(255):273(4095) */
    /* The initial backlight current is 23mA */
    level = mult_frac(level, 273, 17);
    bl_level[0].para_list[0] = level >> 8;
    bl_level[0].para_list[1] = level & 0xFF;

    push_table(handle, bl_level,
        sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER lcd_gc7202_ls_hsd_mipi_hdp_video_lcm_drv = {
    .name = "lcd_gc7202_ls_hsd_mipi_hdp_video",
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

