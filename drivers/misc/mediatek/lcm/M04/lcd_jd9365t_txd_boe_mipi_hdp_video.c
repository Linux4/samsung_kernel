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
/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 start */
static struct LCM_setting_table lcm_diming_disable_setting[] = {
    {0x53, 0x01, {0x24}},
};
/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 end */

/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 start */
static struct LCM_setting_table lcm_aot_suspend_setting[] = {
    {REGFLAG_DELAY, 5, {} },
    {0x28, 0, {} },
    {REGFLAG_DELAY, 50, {} },
    {0x10, 0, {} },
    {REGFLAG_DELAY, 120, {} },
    {0xBA, 1,{0x01}},
    {REGFLAG_DELAY, 10, {} },
};
/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 end */

/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 start */
static struct LCM_setting_table lcm_suspend_setting[] = {
    {REGFLAG_DELAY, 5, {} },
    {0x28, 0, {} },
    {REGFLAG_DELAY, 50, {} },
    {0x10, 0, {} },
    {REGFLAG_DELAY, 120, {} },
};
/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 end */

/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 start */
static struct LCM_setting_table init_setting_vdo[] = {
    {0xDF, 3, {0x93,0x83,0x16}},
    {0xDE, 1, {0x00}},
    {0xB2, 5, {0x01,0x23,0x60,0x88,0x24}},
    {0xB6, 3, {0x0C,0x99,0xAE}},
    {0xB9, 3, {0x3C,0x33,0x35}},
    {0xBB, 7, {0x0C,0x22,0x43,0x3C,0x5A,0x20,0x20}},
    {0xBC, 4, {0x2E,0x40,0x43,0x00}},
    {0xBD, 2, {0x00,0xCC}},
    {0xC0, 5, {0x00,0xDD,0x1A,0xDD,0x0A}},
    {0xCB, 42,{0x6C,0x66,0x60,0x52,0x47,0x46,0x35,0x3A,0x25,0x3C,0x38,0x35,0x4D,0x38,0x3C,0x2B,0x28,0x1A,0x08,0x04,0x00,0x6C,0x63,0x5B,0x50,0x46,0x42,0x34,0x38,0x20,0x37,0x33,0x2F,0x48,0x31,0x34,0x24,0x1F,0x13,0x06,0x02,0x00}},
    {0xCC, 1, {0x32}},
    {0xCD, 2, {0x10,0x10}},
    {0xBF, 4, {0x32,0x5A,0x33,0xC3}},
    {0xC3, 13,{0x13,0x00,0x06,0x16,0x9B,0x00,0x9D,0x08,0x16,0x04,0x06,0x08,0x16}},
    {0xC5, 2, {0x30,0x20}},
    {0xC6, 4, {0x00,0xBE,0xC8,0x14}},
    {0xCE, 23,{0x01,0x14,0x3C,0x3F,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14,0x14}},
    {0xCF, 15,{0x00,0x00,0xC0,0xC3,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0x00,0x00}},
    {0xD0, 29,{0x00,0x1F,0xDF,0xD5,0xDF,0xC0,0xC2,0xD5,0xD5,0xC8,0xCA,0xC4,0xC6,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x00,0x00,0x00,0x55,0x55,0x54}},
    {0xD1, 29,{0x00,0x1F,0xDF,0xD5,0xDF,0xC1,0xC3,0xD5,0xD5,0xC9,0xCB,0xC5,0xC7,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x00,0x00,0x00,0x55,0x55,0x54}},
    {0xD2, 29,{0x00,0x1F,0x15,0x15,0x1F,0x03,0x01,0x1F,0x15,0x07,0x05,0x0B,0x09,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x00,0x00,0x00,0x00,0x00,0x00}},
    {0xD3, 29,{0x00,0x1F,0x15,0x15,0x1F,0x02,0x00,0x1F,0x15,0x06,0x04,0x0A,0x08,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x00,0x00,0x00,0x00,0x00,0x00}},
    {0xD4, 62,{0x30,0x00,0x00,0x09,0x00,0x0B,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x00,0x10,0x60,0x01,0x00,0x04,0x01,0x01,0x00,0x01,0x00,0x04,0x01,0x01,0x11,0x60,0x03,0x00,0x05,0x01,0x01,0x00,0x00,0x00,0x04,0x0A,0x06,0x61,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x81,0x03,0xD4}},
    {0xD5, 27,{0x02,0x10,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x43,0x00,0x00,0x00,0x07,0x32,0x5A,0x00,0x48,0x3B,0x00,0x02,0x1E,0x02,0x74,0x00,0x0D,0x08}},
    {0xD7, 19,{0x00,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71}},
    {0xDE, 1, {0x01}},
    {0xB5, 1, {0x6F}},
    {0xC5, 2, {0x22,0x12}},
    {0xDE, 1, {0x02}},
    {0xB3, 5, {0x20,0xA0,0xEA,0x5F,0x4B}},
    {0xB7, 8, {0x44,0x00,0x88,0x00,0x03,0x00,0x6E,0x10}},
    {0xBD, 1, {0x14}},
    {0xC1, 8, {0x00,0x40,0x20,0x00,0x14,0x14,0x14,0x14}},
    {0xC2, 7, {0x42,0x70,0x01,0x01,0xE0,0x73,0xF8}},
    {0xC3, 6, {0x20,0xFB,0x00,0xA0,0x20,0x62}},
    {0xC6, 1, {0x05}},
    {0xE5, 10,{0x00,0xE6,0xE5,0x02,0x27,0x42,0x27,0x42,0x09,0x00}},
    {0xE6, 3, {0x10,0x10,0xB5}},
    {0xEC, 3, {0x01,0x7F,0x22}},
    {0xDE, 1, {0x03}},
    {0xD1, 1, {0x00}},
    {0xDE, 1, {0x00}},
    {0x35, 0, {0x00}},
};
/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 end */

/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 start */
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    {REGFLAG_DELAY,30,{}},
    {0x11, 0, {0x00}},
    {REGFLAG_DELAY,120,{}},

    {0x29, 0, {0x00}},
    {REGFLAG_DELAY,10,{}},

    {0x51, 2, {0x00,0x00}},

    {0x55, 1, {0x00}},
    {REGFLAG_DELAY, 5, {}},
    {0x53, 1, {0x2C}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 end */

/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 start */
static struct LCM_setting_table bl_level[] = {
    {0x51, 2, {0x00,0x00} },
    {REGFLAG_END_OF_TABLE, 0x00, {} }
};
/* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 end */

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
    params->dsi.vertical_backporch = 19;
    params->dsi.vertical_frontporch = 200;
    /* disable dynamic frame rate */
    /* params->dsi.vertical_frontporch_for_low_power = 540; */
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 160;
    params->dsi.horizontal_backporch = 170;
    params->dsi.horizontal_frontporch = 200;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;
    params->dsi.ssc_range = 4;
    params->dsi.ssc_disable = 1;
    /* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
    params->dsi.PLL_CLOCK = 270;
#else
    params->dsi.PLL_CLOCK = 572;
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
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1C;


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

    /* HS03S code added for SR-AL5625-01-428 by gaozhengwei at 20210601 start */
    if ((lcd_pinctrl1 == NULL) ||
        (lcd_disp_pwm_gpio == NULL)) {
        pr_err("lcd_pinctrl1 is invaild\n");
        return;
    }
    /* HS03S code added for SR-AL5625-01-428 by gaozhengwei at 20210601 end */

    /* after 6ms reset HLH */
    MDELAY(9);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(5);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
    /* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 start */
    MDELAY(2);
    lcm_set_gpio_output(GPIO_TSP_RST, GPIO_OUT_ZERO);
    MDELAY(3);
    /* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 end */
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(5);

    pinctrl_select_state(lcd_pinctrl1, lcd_disp_pwm);

    push_table(NULL,
        init_setting_vdo,
        sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table),
        1);

    /* HS03S code added for SR-AL5625-01-428 by gaozhengwei at 20210601 start */
    /* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 start */
    MDELAY(5);
    lcm_set_gpio_output(GPIO_TSP_RST, GPIO_OUT_ONE);
    /* hs03s_NM code added for SR-AL5625-01-609 by fengzhigang at 20220424 end */
    push_table(NULL,
        lcm_sleep_out_setting,
        sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table),
        1);
    /* HS03S code added for SR-AL5625-01-428 by gaozhengwei at 20210601 end */

    pr_notice("[Kernel/LCM] %s exit\n", __func__);
}

static void lcm_suspend(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    /* HS03S code added for DEVAL5626-286 by fengzhigang at 20210709 start */
    if (!mtk_tpd_smart_wakeup_support()) {
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
    /* HS03S code added for DEVAL5626-286 by fengzhigang at 20210709 end */

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
    LCM_LOGI("%s,[M04] jd9365t_txd_boe backlight: level = %d\n", __func__, level);

    if (level == 0) {
        pr_notice("[LCM],[M04] jd9365t_txd_boe backlight off, diming disable first\n");
        push_table(NULL,
            lcm_diming_disable_setting,
            sizeof(lcm_diming_disable_setting) / sizeof(struct LCM_setting_table),
            1);
    }

    /* Backlight data is mapped from 8 bits to 12 bits,The default scale is 17(255):273(4095) */
    /* The initial backlight current is 23mA */
    level = mult_frac(level, 273, 17);
    /* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 start */
    bl_level[0].para_list[0] = level >> 8;
    bl_level[0].para_list[1] = level & 0xFF;
    /* hs03s_NM code added for DEVAL5626-976 by huangzhongjie at 20220608 end */

    push_table(handle, bl_level,
        sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

struct LCM_DRIVER lcd_jd9365t_txd_boe_mipi_hdp_video_lcm_drv = {
    .name = "lcd_jd9365t_txd_boe_mipi_hdp_video",
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
