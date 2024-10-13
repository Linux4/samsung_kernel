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

#define FRAME_WIDTH            (1080)
#define FRAME_HEIGHT          (2408)

/* physical size in um */
#define LCM_PHYSICAL_WIDTH        (68430)
#define LCM_PHYSICAL_HEIGHT      (152570)
#define LCM_DENSITY            (450)

#define REGFLAG_DELAY            0xFFFC
#define REGFLAG_UDELAY            0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD
#define REGFLAG_RESET_LOW        0xFFFE
#define REGFLAG_RESET_HIGH        0xFFFF

#define LCM_ID_HX83112F 0x13

/*hs14 code for SR-AL6528A-01-435 by duanyaoming at 20220914 start*/
/*backlight from 23ma to 20ma*/
#define BACKLIGHT_MAX_REG_VAL   (4095/15*20)
#define BACKLIGHT_MAX_APP_VAL   (255/15*23)
/*hs14 code for SR-AL6528A-01-435 by duanyaoming at 20220914 end*/
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
/*hs14 code for SR-AL6528A-01-423 by duanyaoming at 20220926 start*/
static struct LCM_setting_table lcm_diming_disable_setting[] = {
    {0x53, 0x01, {0x24}},
    {REGFLAG_DELAY, 10, {} },
};
/*hs14 code for SR-AL6528A-01-423 by duanyaoming at 20220926 end*/
static struct LCM_setting_table lcm_aot_suspend_setting[] = {
    {0x28, 0, {} },
    {REGFLAG_DELAY, 20, {} },
    {0x10, 0, {} },
    {REGFLAG_DELAY, 120, {} },

    {0xB9, 3, {0x83,0x11,0x2F}},
    {0xBD, 1, {0x00}},
    {0xB1, 1, {0x01}},
    {REGFLAG_DELAY, 20, {} },
};
/*hs14 code for SR-AL6528A-01-457 by duanyaoming at 20220911 start*/
static struct LCM_setting_table lcm_suspend_setting[] = {
    {0x28, 0, {} },
    {REGFLAG_DELAY, 20, {} },
    {0x10, 0, {} },
    {REGFLAG_DELAY, 120, {} },

};
/*hs14 code for SR-AL6528A-01-457 by duanyaoming at 20220911 end*/
static struct LCM_setting_table init_setting_cmd[] =
{
    {0xB9, 3, {0x83,0x11,0x2F}},
    {0xB1, 12, {0x40,0x27,0x27,0x80,0x80,0xA8,0xA8,0x38,0x1E,0x06,0x06,0x1A}},
    {0xBD, 1, {0x01}},
    {0xB1, 2, {0xA4,0x01}},
    {0xBD, 1, {0x02}},
    {0xB1, 2, {0x31,0x31}},
    {0xBD, 1, {0x00}},
    {0xB2, 20, {0x00,0x02,0x08,0x90,0x68,0x00,0x1C,0x4C,0x10,0x08,0x00,0xBC,0x11,0x00,0x00,0x00,0x00,0x00,0x80,0x14}},
    {0xB4, 36, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xBF,0x00,0xBF,0x00,0xBF,0x00,0xBF,0x04,0x00,0x03,0x04,0x00,0x37,0x00,0x03,0x05,0x06,0x00,0x00,0x00,0x00,0x84,0x00,0xFF,0x00,0xFF,0x00,0x01,0x01}},
    {0xBD, 1, {0x02}},
    {0xB4, 11, {0x92,0x44,0x12,0x00,0x22,0x22,0x11,0x12,0x88,0x12,0x42}},
    {0xBD, 1, {0x00}},
    {0xE9, 1, {0xC4}},
    {0xBA, 2, {0x9A,0x01}},
    {0xE9, 1, {0xC9}},
    {0xBA, 2, {0x09,0x3D}},
    {0xE9, 1, {0x3F}},
    {0xE9, 1, {0xCF}},
    {0xBA, 1, {0x01}},
    {0xE9, 1, {0x3F}},
    {0xBD, 1, {0x01}},
    {0xE9, 1, {0xC5}},
    {0xBA, 1, {0x04}},
    {0xE9, 1, {0x3F}},
    {0xBD, 1, {0x00}},
    {0xC7, 2, {0xF0,0x20}},
    {0xCB, 4, {0x5F,0x21,0x0A,0x59}},
    {0xCC, 1, {0x0A}},
    {0xD1, 1, {0x07}},
    {0xD3, 38, {0x81,0x14,0x00,0x00,0x00,0x14,0x00,0x00,0x03,0x03,0x14,0x14,0x08,0x08,0x08,0x08,0x22,0x18,0x32,0x10,0x1B,0x00,0x1B,0x32,0x20,0x08,0x10,0x08,0x32,0x10,0x1B,0x00,0x1B,0x00,0x00,0x1E,0x09,0x85}},
    {0xBD, 1, {0x01}},
    {0xD2, 12, {0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30}},
    {0xD3, 10, {0x00,0x00,0x40,0x00,0x00,0x00,0x24,0x08,0x01,0x19}},
    {0xBD, 1, {0x02}},
    {0xD3, 23, {0x01,0x01,0x00,0x01,0x00,0x0A,0x0A,0x0A,0x0A,0x01,0x03,0x00,0xC8,0x08,0x01,0x11,0x01,0x00,0x00,0x01,0x05,0x78,0x10}},
    {0xBD, 1, {0x00}},
    {0xD5, 48, {0x18,0x18,0x18,0x18,0x41,0x41,0x41,0x41,0x18,0x18,0x18,0x18,0x31,0x31,0x30,0x30,0x2F,0x2F,0x31,0x31,0x30,0x30,0x2F,0x2F,0x40,0x40,0x01,0x01,0x00,0x00,0x03,0x03,0x02,0x02,0x24,0x24,0xCE,0xCE,0xCE,0xCE,0x41,0x41,0x18,0x18,0x21,0x21,0x20,0x20}},
    {0xD6, 48, {0x18,0x18,0x18,0x18,0x41,0x41,0x41,0x41,0x18,0x18,0x18,0x18,0x31,0x31,0x30,0x30,0x2F,0x2F,0x31,0x31,0x30,0x30,0x2F,0x2F,0x40,0x40,0x02,0x02,0x03,0x03,0x00,0x00,0x01,0x01,0x24,0x24,0xCE,0xCE,0xCE,0xCE,0x18,0x18,0x41,0x41,0x20,0x20,0x21,0x21}},

    {0xD8, 36, {0xAA,0xAA,0xAA,0xBF,0xFF,0xAA,0xAA,0xAA,0xAA,0xBF,0xFF,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA}},
    {0xBD, 1, {0x01}},
    {0xD8, 24, {0xAF,0xFF,0xFF,0xFF,0xFF,0xFF,0xAF,0xFF,0xFF,0xFF,0xFF,0xFF,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA,0xAA}},
    {0xBD, 1, {0x02}},
    {0xD8, 12, {0xAF,0xFF,0xFF,0xFF,0xFF,0xFF,0xAF,0xFF,0xFF,0xFF,0xFF,0xFF}},
    {0xBD, 1, {0x03}},
    {0xD8, 24, {0xAA,0xAF,0xFF,0xEA,0xAA,0xAA,0xAA,0xAF,0xFF,0xEA,0xAA,0xAA,0xEF,0xFF,0xFF,0xFF,0xFF,0xFF,0xEF,0xFF,0xFF,0xFF,0xFF,0xFF}},
    {0xBD, 1, {0x00}},
    {0xE0, 46, {0x00,0x11,0x2B,0x2E,0x37,0x70,0x81,0x88,0x8B,0x8B,0x8C,0x8A,0x87,0x86,0x84,0x83,0x83,0x85,0x87,0xA0,0xB3,0x4D,0x76,0x00,0x11,0x2B,0x2E,0x37,0x70,0x81,0x88,0x8B,0x8B,0x8C,0x8A,0x87,0x86,0x84,0x83,0x83,0x85,0x87,0xA0,0xB3,0x4D,0x76}},
    {0xE7, 44, {0x44,0x26,0x08,0x08,0x20,0xD2,0x22,0xD2,0x1F,0xD2,0x0F,0x02,0x00,0x00,0x02,0x02,0x12,0x05,0xFF,0xFF,0x22,0xD2,0x1F,0xD2,0x08,0x00,0x00,0x48,0x17,0x03,0x69,0x00,0x00,0x00,0x4C,0x4C,0x1E,0x00,0x40,0x01,0x00,0x0C,0x0A,0x04}},
    {0xBD, 1, {0x01}},
    {0xE7, 12, {0x12,0x50,0x00,0x01,0x38,0x07,0x2C,0x08,0x00,0x00,0x00,0x02}},
    {0xBD, 1, {0x02}},
    {0xE7, 17, {0x02,0x02,0x02,0x00,0xBF,0x01,0x03,0x01,0x03,0x01,0x03,0x22,0x20,0x28,0x40,0x01,0x00}},
    {0xBD, 1, {0x03}},
    {0xE7, 2, {0x01,0x01}},
    {0xE1, 1, {0x00}},
    {0xBD, 1, {0x00}},
    {0xC0, 8, {0x35,0x35,0x00,0x00,0x2D,0x1A,0x66,0x81}},
    {0xBD, 1, {0x01}},
    {0xC7, 1, {0x44}},
    {0xBF, 1, {0x0B}},
    {0xBD, 1, {0x00}},

    {0xC9, 5, {0x00,0x1E,0x09,0xC4,0x01}},
    {0x11, 0, {}},
    {REGFLAG_DELAY, 100, {}},
    {0x51, 2, {0x00,0x00} },

    {0x29, 0, {}},
    {REGFLAG_DELAY, 10, {}},

    {0x55, 1, {0x00}},
    {0x53, 1, {0x2C}},
    {0xB9, 3, {0x00,0x00,0x00}},
};

static struct LCM_setting_table bl_level[] = {
    {0x51, 2, {0x0F,0xFF} },
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
    params->dsi.mode = BURST_VDO_MODE;
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

    params->dsi.vertical_sync_active = 15;
    params->dsi.vertical_backporch = 15;
    params->dsi.vertical_frontporch = 78;
    //params->dsi.vertical_frontporch_for_low_power = 540;/*disable dynamic frame rate*/
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 8;
    params->dsi.horizontal_backporch = 16;
    params->dsi.horizontal_frontporch = 16;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;
    // params->dsi.noncont_clock           = 1;
    // params->dsi.noncont_clock_period    = 1;
    params->dsi.ssc_range = 4;
    params->dsi.ssc_disable = 1;
    /* params->dsi.ssc_disable = 1; */
#ifndef CONFIG_FPGA_EARLY_PORTING
    /* this value must be in MTK suggested table */
    params->dsi.PLL_CLOCK = 550;
    // params->dsi.PLL_CK_VDO = 540;
#else
    params->dsi.pll_div1 = 0;
    params->dsi.pll_div2 = 0;
    params->dsi.fbk_div = 0x1;
#endif
    /*hs14 code for SR-AL6528A-01-408 by tangzhen at 20220913 start*/
    params->dsi.clk_lp_per_line_enable = 0;
    params->dsi.esd_check_enable = 1;
    params->dsi.customization_esd_check_enable = 1;
    params->dsi.lcm_esd_check_table[0].cmd = 0x0a;
    params->dsi.lcm_esd_check_table[0].count = 1;
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x1D;
    /*hs14 code for SR-AL6528A-01-408 by tangzhen at 20220913 end*/
    params->max_refresh_rate = 60;
    params->min_refresh_rate = 60;
}

static void lcm_bias_enable(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    lcd_bias_set_vspn(ON, VSP_FIRST_VSN_AFTER, 5500);  //open lcd bias
    /*hs14 code for P221103-05645 by duanyaoming at 20221107 start*/
    MDELAY(1);
    /*hs14 code for P221103-05645 by duanyaoming at 20221107 end*/

}

static void lcm_bias_disable(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    lcd_bias_set_vspn(OFF, VSN_FIRST_VSP_AFTER, 5500);  //open lcd bias

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
    /*hs14 code for SR-AL6528A-01-423 by duanyaoming at 202201001 start*/
    if (g_system_is_shutdown) {
        lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
        MDELAY(3);
    }
    /*hs14 code for SR-AL6528A-01-423 by duanyaoming at 202201001 end*/
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
    /*hs14 code for P221103-05645 by duanyaoming at 20221107 start*/
    MDELAY(2);
    /*hs14 code for P221103-05645 by duanyaoming at 20221107 end*/
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(50);

    push_table(NULL, init_setting_cmd, ARRAY_SIZE(init_setting_cmd), 1);
    LCM_LOGI("hx83112f_txd_boe----tps6132----lcm mode = vdo mode :%d----\n",lcm_dsi_mode);
}

static void lcm_suspend(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);
    /*hs14 code for SR-AL6528A-01-457 by duanyaoming at 20220911 start*/
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
    /*hs14 code for SR-AL6528A-01-457 by duanyaoming at 20220911 end*/
    MDELAY(10);
}

static void lcm_resume(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);
    lcm_init();
}

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
    LCM_LOGI("%s,hx83112f_txd_boe backlight: level = %d\n", __func__, level);
    /*hs14 code for SR-AL6528A-01-423 by duanyaoming at 20220926 start*/
    if (level == 0) {
        pr_notice("[LCM],hx83112f_txd_boe backlight off, diming disable first\n");
        push_table(NULL,
            lcm_diming_disable_setting,
            sizeof(lcm_diming_disable_setting) / sizeof(struct LCM_setting_table),
            1);
    }
    /*hs14 code for SR-AL6528A-01-423 by duanyaoming at 20220926 end*/
    level = mult_frac(level, BACKLIGHT_MAX_REG_VAL, BACKLIGHT_MAX_APP_VAL);
    bl_level[0].para_list[0] = level >> 8;
    bl_level[0].para_list[1] = level & 0xFF;
    push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

struct LCM_DRIVER lcd_hx83112f_txd_boe_mipi_fhd_video_lcm_drv = {
    .name = "lcd_hx83112f_txd_boe_mipi_fhd_video",
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

