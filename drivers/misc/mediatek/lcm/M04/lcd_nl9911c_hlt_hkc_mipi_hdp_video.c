// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define LOG_TAG "LCM"
#define F6_VALUE 3
#define F6_CODELINE 31
#define F6_CHARLEN 22

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
    {0x53, 0x01, {0x24}}
};

static struct LCM_setting_table lcm_suspend_setting[] = {
    {0x26, 1,{0x08}},
    {0x28, 0, {} },
    {REGFLAG_DELAY, 20, {} },
    {0x10, 0, {} },
    {REGFLAG_DELAY, 120, {} }
};
/* hs04 code for DEAL6398A-397 by suyurui at 20220803 start */
static struct LCM_setting_table init_setting_vdo[] = {
    {0xF0, 2,{0x5A,0x59}},
    {0xF1, 2,{0xA5,0xA6}},
    {0xB0,30,{0x81,0x80,0x05,0x04,0x86,0x87,0x04,0x05,0xBB,0x55,0x22,0x55,0x80,0x00,0x00,0x64,0x00,0x00,0x0F,0x05,0x04,0x03,0x02,0x01,0x02,0x03,0x04,0x00,0x00,0x00}},
    {0xB1,29,{0x53,0x42,0x85,0x00,0x80,0x00,0x00,0x64,0x00,0x00,0x04,0x08,0x54,0x00,0x00,0x00,0x44,0x40,0x02,0x01,0x40,0x02,0x01,0x40,0x02,0x01,0x40,0x02,0x01}},//OE3.08/DUTY30%
    {0xB2,17,{0x54,0x84,0x85,0x05,0x40,0x02,0x01,0x40,0x02,0x01,0x05,0x05,0x54,0x0C,0x0C,0x0D,0x0B}},
    {0xB3,31,{0x02,0x0D,0x07,0x0D,0x07,0x26,0x26,0x91,0xA2,0x33,0x44,0x00,0x26,0x00,0x18,0x01,0x02,0x08,0x20,0x30,0x08,0x09,0x44,0x20,0x40,0x20,0x40,0x08,0x09,0x22,0x33}},
    // FORWORD SCAN
    {0xB4,28,{0x02,0x02,0x00,0x00,0xE3,0x09,0x22,0x00,0x22,0x0D,0x0F,0x11,0x13,0x05,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0xFF,0xFF,0xFC,0x00,0x00,0x00}},
    {0xB5,28,{0x00,0x00,0x00,0x00,0xE3,0x08,0x22,0x00,0x22,0x0C,0x0E,0x10,0x12,0x04,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0xFF,0xFF,0xFC,0x00,0x00,0x00}},
    // BACKWORD SCAN
    //{0x36, 1,{0x03}},
    //{0xB4,28,{0x02,0x02,0x00,0x00,0xE3,0x04,0x00,0x22,0x22,0x12,0x10,0x0E,0x0C,0x08,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0xFF,0xFF,0xFC,0x00,0x00,0x00}},
    //{0xB5,28,{0x00,0x00,0x00,0x00,0xE3,0x05,0x00,0x22,0x22,0x13,0x11,0x0F,0x0D,0x09,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0xFF,0xFF,0xFC,0x00,0x00,0x00}},
    {0xB8,24,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xAA,0xAA,0xAA,0xAA,0xAA,0xA0,0xAA,0xAA,0xAA,0xAA,0xAA,0xA0}},
    //{0xB9, 5,{0x99,0x11,0x01,0xFF,0x//ID}},
    //{0xBA, 2,{0x62,0x62//OTPVCOM}},
    {0xBB,13,{0x01,0x05,0x09,0x11,0x0D,0x19,0x1D,0x55,0x25,0x69,0x00,0x21,0x25}},
    {0xBC,14,{0x00,0x00,0x00,0x00,0x02,0x20,0xFF,0x00,0x03,0x33,0x01,0x73,0x33,0x02}},
    {0xBD,10,{0xE9,0x02,0x4E,0xCF,0x72,0xA4,0x08,0x44,0xAE,0x15}},
    {0xBE,10,{0x68,0x7C,0x54,0x51,0x0C,0x77,0x43,0x07,0x0E,0x0E}},//VOP5.3-5.7/VGH15.4/VGL-13
    {0xBF, 8,{0x07,0x25,0x07,0x25,0x7F,0x00,0x11,0x04}},
    {0xC0, 9,{0x10,0x00,0x1F,0xFF,0xFF,0xFF,0x00,0xFF,0x00}},
    {0xC1,19,{0xC0,0x20,0x20,0x96,0x04,0x28,0x28,0x04,0x2A,0x40,0x36,0x00,0x07,0xC0,0x10,0xFF,0x95,0x01,0xC0}},//LONGV
    {0xC2, 1,{0x00}},
    {0xC3, 9,{0x06,0x00,0xFF,0x00,0xFF,0x00,0x00,0x81,0x01}},
    {0xC4,10,{0x84,0x01,0x2B,0x41,0x00,0x3C,0x00,0x03,0x03,0x2E}},
    {0xC5,11,{0x03,0x1C,0xC0,0xC0,0x40,0x10,0x42,0x44,0x0F,0x0F,0x14}},
    {0xC6,10,{0x87,0x96,0x2A,0x29,0x29,0x33,0x7F,0x34,0x08,0x04}},
    //GAMMA2.2
    {0xC7,22,{0xFC,0xC7,0xA4,0x8A,0x5C,0x3B,0x0A,0x5B,0x22,0xF5,0xC9,0x96,0xEE,0xC2,0xA7,0x7E,0x66,0x43,0x1A,0x7F,0xC0,0x00}},
    {0xC8,22,{0xFC,0xC7,0xA4,0x8A,0x5C,0x3B,0x0A,0x5B,0x22,0xF5,0xC9,0x96,0xEE,0xC2,0xA7,0x7E,0x66,0x43,0x1A,0x7F,0xC0,0x00}},
    {0xCB, 1,{0x00}},
    {0xD0, 8,{0x80,0x0D,0xFF,0x0F,0x61,0x0B,0x08,0x04}},
    {0xD2, 1,{0x42}},
    {0xFE, 4,{0xFF,0xFF,0xFF,0x40}},//64M
    {0xD7, 1,{0xDE}},
    {0xE0,26,{0x30,0x00,0x80,0x88,0x11,0x3F,0x22,0x62,0xDF,0xA0,0x04,0xCC,0x01,0xFF,0xF6,0xFF,0xF0,0xFD,0xFF,0xFD,0xF8,0xF5,0xFC,0xFC,0xFD,0xFF}},
    {0xE1,23,{0xEF,0xFE,0xFE,0xFE,0xFE,0xEE,0xF0,0x20,0x33,0xFF,0x00,0x00,0x6A,0x90,0xC0,0x0D,0x6A,0xF0,0x3E,0xFF,0x00,0x06,0x40}},//20KHZ
    {0xFA, 3,{0x45,0x93,0x01}},
    {0xF6, 1,{0x3F}},
    {0xF1, 2,{0x5A,0x59}},
    {0xF0, 2,{0xA5,0xA6}},
    {0x51, 2,{0x00,0x00}},
    {0x53, 1,{0x2C}},
    //{0x55, 1,{0x00}},
    {0x35, 1,{0x00}},
    {0x11, 0,{0x00}},
    {REGFLAG_DELAY, 120,{}},
    {0x29, 0,{0x00}},
    {REGFLAG_DELAY, 20,{}},
    {0x26, 1,{0x01}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
/* hs04 code for DEAL6398A-397 by suyurui at 20220803 end */
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

    params->dsi.vertical_sync_active = 4;
    params->dsi.vertical_backporch = 32;
    params->dsi.vertical_frontporch = 150;
    /* disable dynamic frame rate */
    /* params->dsi.vertical_frontporch_for_low_power = 540; */
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 4;
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
    params->dsi.PLL_CLOCK = 362;
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

    /* HS03S code for SR-AL5625-01-197 by gaozhengwei at 2021/04/25 start */
    if (g_system_is_shutdown) {
        lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
        MDELAY(3);
    }
    /* HS03S code for SR-AL5625-01-197 by gaozhengwei at 2021/04/25 end */

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

/* hs04 code for DEAL6398A-397 by suyurui at 20220803 start */
static unsigned char char1tonum(unsigned char ch)
{
    if((ch >= '0') && (ch <= '9')) {
        return ch - '0';
    } else if ((ch >= 'a') && (ch <= 'f')) {
        return ch - 'a' + 10;
    } else if ((ch >= 'A') && (ch <= 'F')) {
        return ch - 'A' + 10;
    } else {
        return 0xff;
    }
}

static unsigned char char2tonum(unsigned char hch, unsigned char lch)
{
    return ((char1tonum(hch) << 4) | char1tonum(lch));
}

static void kernel_f6_reg_update(void)
{
    char *r = NULL;
    unsigned char tempbuf_f6[F6_VALUE] = {0};
    unsigned char temp_f6 = 0x0;

    r = strstr(saved_command_line, "androidboot.temp_f6=0x");
    snprintf(tempbuf_f6, F6_VALUE, "%s", (r+F6_CHARLEN));
    temp_f6 = char2tonum(tempbuf_f6[0], tempbuf_f6[1]);
    init_setting_vdo[F6_CODELINE].para_list[0] = temp_f6;
}
/* hs04 code for DEAL6398A-397 by suyurui at 20220803 end */

static void lcm_init(void)
{
    pr_notice("[Kernel/LCM] %s enter\n", __func__);

    /* after 6ms reset HLH */
    MDELAY(9);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(5);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ZERO);
    MDELAY(5);
    lcm_set_gpio_output(GPIO_LCD_RST, GPIO_OUT_ONE);
    MDELAY(50);

    if ((lcd_pinctrl1 == NULL) || (lcd_disp_pwm_gpio == NULL)) {
        pr_err("lcd_pinctrl1/lcd_disp_pwm_gpio is invaild\n");
        return;
    }

    pinctrl_select_state(lcd_pinctrl1, lcd_disp_pwm);

    /* hs04 code for DEAL6398A-397 by suyurui at 20220803 start */
    kernel_f6_reg_update();
    /* hs04 code for DEAL6398A-397 by suyurui at 20220803 end */

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
    LCM_LOGI("%s,nl9911c_hlt_hkc backlight: level = %d\n", __func__, level);

    if (level == 0) {
        pr_notice("[LCM],nl9911c_hlt_hkc backlight off, diming disable first\n");
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

struct LCM_DRIVER lcd_nl9911c_hlt_hkc_mipi_hdp_video_lcm_drv = {
    .name = "lcd_nl9911c_hlt_hkc_mipi_hdp_video",
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

