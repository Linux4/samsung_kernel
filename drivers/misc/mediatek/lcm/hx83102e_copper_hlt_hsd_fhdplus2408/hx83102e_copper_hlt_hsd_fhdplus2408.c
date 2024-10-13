// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/**
*Name：<hx83102e_copper_hlt_hsd_fhdplus2408>
*Author：<huangzhongjie>
*Date：<2021/10/21>
*Purpose：<LCD bring up>
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
/*TabA7 Lite code for OT8-4085 by weiqiang at 20210319 start*/
#include <linux/regulator/consumer.h>
/*TabA7 Lite code for OT8-4085 by weiqiang at 20210319 end*/

#ifndef MACH_FPGA
#include <lcm_pmic.h>
#endif
#define LCM_NAME "hx83102e"
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

#define GPIO_OUT_LOW 0
#define GPIO_OUT_ONE 1
#define GPIO_OUT_ZERO 0
extern void lcm_set_gpio_output(unsigned int GPIO, unsigned int output);
extern unsigned int GPIO_LCD_RST;
extern struct pinctrl *lcd_pinctrl1;
extern struct pinctrl_state *lcd_disp_pwm;
extern struct pinctrl_state *lcd_disp_pwm_gpio;

extern void himax_resume_by_ddi(void);

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
#define LCM_PHYSICAL_HEIGHT      (172224)

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
    {0x53, 0x01, {0x24}}
};

static struct LCM_setting_table lcm_normal_suspend_setting[] = {
    {0x28, 0, {} },
    {REGFLAG_DELAY, 20, {} },
    {0x10, 0, {} },
    {REGFLAG_DELAY, 120, {} },
    {0xB9,0x03,{0x83,0x10,0x2E}},
    {0xB1,0x01,{0x11}},
    {0xB9,0x03,{0x00,0x00,0x00}}
};

static struct LCM_setting_table lcm_aot_suspend_setting[] = {
    {0x28, 0, {} },
    {REGFLAG_DELAY, 20, {} },
    {0x10, 0, {} },
    {REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table init_setting_vdo[] = {
    {0xB9,0x03,{0x83,0x10,0x2E}},
    {0xE9,0x01,{0xCD}},
    {0xBB,0x01,{0x01}},
    {0xE9,0x01,{0x00}},
    {0xD1,0x04,{0x67,0x0C,0xFF,0x05}},
    {0xB1,0x11,{0x10,0xFA,0xAF,0xAF,0x30,0x30,0xB1,0x4D,0x2F,0x36,0x36,0x36,0x36,0x22,0x21,0x15,0x00}},
    {0xB2,0x10,{0x00,0x20,0x35,0x3C,0x00,0x0C,0xD4,0x4D,0x00,0x00,0x00,0x00,0x15,0x20,0x57,0x00}},
    {0xB4,0x10,{0x10,0x10,0x01,0x01,0x88,0x88,0x68,0x50,0x01,0x8F,0x01,0x58,0x00,0xFF,0x00,0xFF}},
    {0xD2,0x02,{0x30,0x30}},
    {0xBF,0x07,{0xFC,0x85,0x80,0x9C,0x36,0x00,0x04}},
    {0xD3,0x2B,{0x00,0x00,0x00,0x00,0x3C,0x3D,0x00,0x00,0x00,0x27,0x27,0x22,0x0F,0x0D,0x0D,0x04,0x00,0x32,0x10,0x0C,0x00,0x0C,0x32,0x15,0x4C,0x05,0x4C,0x32,0x10,0x08,0x00,0x00,0x00,0x00,0x2B,0x50,0xB6,0xC8,0x2C,0x4F,0xB6,0xC8,0x0F}},
    {0xE0,0x2E,{0x00,0x06,0x0E,0x14,0x1C,0x2E,0x47,0x4F,0x57,0x53,0x6D,0x73,0x78,0x88,0x88,0x93,0x9E,0xB2,0xB1,0x57,0x5E,0x68,0x7A,0x00,0x06,0x0E,0x14,0x1C,0x2E,0x47,0x4F,0x57,0x53,0x6D,0x73,0x78,0x88,0x88,0x93,0x9E,0xB2,0xB1,0x57,0x5E,0x68,0x7A}},
    {0xBD,0x01,{0x01}},
    {0xCB,0x09,{0x80,0x36,0x12,0x16,0xC0,0x28,0x40,0x84,0x22}},
    {0xD3,0x0B,{0x01,0x00,0xFC,0x00,0x00,0x11,0x10,0x00,0x0A,0x00,0x01}},
    {0xBD,0x01,{0x02}},
    {0xB4,0x06,{0x4E,0x00,0x33,0x11,0x33,0x88}},
    {0xBF,0x03,{0xF2,0x00,0x02}},
    {0xBD,0x01,{0x00}},
    {0xC0,0x0E,{0x23,0x23,0x22,0x11,0xA2,0x10,0x00,0x80,0x00,0x00,0x08,0x00,0x63,0x63}},
    {0xD5,0x2C,{0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x19,0x19,0x18,0x18,0x1A,0x1A,0x1B,0x1B,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x20,0x21,0x24,0x25,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
    {0xD6,0x2C,{0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x19,0x19,0x1A,0x1A,0x1B,0x1B,0x03,0x02,0x01,0x00,0x07,0x06,0x05,0x04,0x25,0x24,0x21,0x20,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
    {0xD0,0x03,{0x07,0x04,0x05}},
    {0xE7,0x17,{0x12,0x13,0x02,0x02,0x55,0x00,0x0E,0x0E,0x00,0x26,0x29,0x72,0x1C,0x72,0x01,0x27,0x00,0x00,0x00,0x00,0x17,0x00,0x68}},
    {0xBD,0x01,{0x01}},
    {0xE7,0x07,{0x01,0x30,0x01,0x94,0x0D,0xB5,0x0E}},
    {0xBD,0x01,{0x02}},
    {0xE7,0x1C,{0xFF,0x01,0xFD,0x01,0x00,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x81,0x00,0x02,0x40}},
    {0xBD,0x01,{0x02}},
    {0xD8,0x0C,{0xFF,0xFF,0xFF,0xFF,0xFF,0xF0,0xFF,0xFF,0xFF,0xFF,0xFF,0xF0}},
    {0xBD,0x01,{0x00}},
    {0xCC,0x01,{0x02}},
    {0xBA,0x08,{0x70,0x23,0xA8,0x83,0xF2,0x80,0xC0,0x0D}},
    {0xBD,0x01,{0x03}},
    {0xB2,0x01,{0x80}},
    {0xBD,0x01,{0x00}},
    {0xC8,0x06,{0x00,0x04,0x04,0x00,0x00,0x82}},
    {0x11,0,{0x00}},
    {REGFLAG_DELAY, 70, {}},
    {0x29,0,{0x00}},
    {REGFLAG_DELAY, 20, {}},
    {0x51,0x02,{0x00,0x00}},
    {REGFLAG_DELAY, 5, {}},
    {0xC9,0x04,{0x00,0x0D,0xF0,0x00}},
    {REGFLAG_DELAY, 5, {}},
    {0x53,0x01,{0x2C}},
    {REGFLAG_DELAY, 5, {}},
    {0xB9,0x03,{0x00,0x00,0x00}},
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

    params->dsi.vertical_sync_active = 4;
    params->dsi.vertical_backporch = 10;
    params->dsi.vertical_frontporch = 212;
    /* disable dynamic frame rate */
    /* params->dsi.vertical_frontporch_for_low_power = 540; */
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 8;
    params->dsi.horizontal_backporch = 28;
    params->dsi.horizontal_frontporch = 28;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;
    params->dsi.ssc_range = 4;
    params->dsi.ssc_disable = 1;
    /* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
    params->dsi.PLL_CLOCK = 260;
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
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9D;

    params->dsi.lcm_esd_check_table[1].cmd = 0x09;
    params->dsi.lcm_esd_check_table[1].count = 3;
    params->dsi.lcm_esd_check_table[1].para_list[0] = 0x80;
    params->dsi.lcm_esd_check_table[1].para_list[1] = 0x73;
    params->dsi.lcm_esd_check_table[1].para_list[2] = 0x04;
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
    MDELAY(9);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(5);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
    MDELAY(5);
    MDELAY(5);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    if(tp_is_used == Himax) {
        himax_resume_by_ddi();
    }
    MDELAY(50);

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

extern int mtk_tpd_smart_wakeup_support(void);

static void lcm_suspend(void)
{
    pr_notice("%s [Kernel/LCM] %s enter\n", LCM_NAME, __func__);

    if (!mtk_tpd_smart_wakeup_support()) {
        push_table(NULL,
            lcm_normal_suspend_setting,
            sizeof(lcm_normal_suspend_setting) / sizeof(struct LCM_setting_table),
            1);
    } else {
        push_table(NULL,
            lcm_aot_suspend_setting,
            sizeof(lcm_aot_suspend_setting) / sizeof(struct LCM_setting_table),
            1);
    }

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

    pr_notice("[LCM]%s,%s backlight: level = %d\n", __func__, LCM_NAME, level);

    if (level == 0) {
        pr_notice("[LCM],HLT_HSD backlight off,diming disable first\n");
        push_table(NULL,
            lcm_diming_disable_setting,
            sizeof(lcm_diming_disable_setting) / sizeof(struct LCM_setting_table),
            1);
    }

    /* Backlight data is mapped from 8 bits to 12 bits,The default scale is 1:16 */
    /* The initial backlight current is 22mA */
    level = mult_frac(level, 273, 17);
    bl_level[0].para_list[0] = level >> 8;
    bl_level[0].para_list[1] = level & 0xFF;

    push_table(handle, bl_level,
        sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER hx83102e_copper_hlt_hsd_fhdplus2408_lcm_drv = {
    .name = "hx83102e_copper_hlt_hsd_fhdplus2408",
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



