// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif


/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

#define HFP_SUPPORT 1

#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"
static int current_fps = 60;
extern int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value);
/*Tab A9 code for SR-AX6739A-01-654 by suyurui at 20230605 start*/
extern bool g_smart_wakeup_flag;
/*Tab A9 code for SR-AX6739A-01-654 by suyurui at 20230605 end*/

/*Tab A9 code for SR-AX6739A-01-716 by suyurui at 20230605 start*/
#include <linux/string.h>
static char gs_bl_tb0[3] = {0x51, 0x06, 0x66};//4096 * 40% = 1638
/*Tab A9 code for SR-AX6739A-01-716 by suyurui at 20230605 end*/

#ifdef VENDOR_EDIT
// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
extern void lcd_queue_load_tp_fw(void);
#endif /*VENDOR_EDIT*/

struct lcm {
    struct device *dev;
    struct drm_panel panel;
    struct backlight_device *backlight;
    struct gpio_desc *reset_gpio;
    struct gpio_desc *bias_pos;
    struct gpio_desc *bias_neg;
    bool prepared;
    bool enabled;

    unsigned int gate_ic;

    int error;
};

#define lcm_dcs_write_seq(ctx, seq...)                                         \
    ({                                                                     \
        const u8 d[] = { seq };                                        \
        BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
                 "DCS sequence too big for stack");            \
        lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
    })

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
    ({                                                                     \
        static const u8 d[] = { seq };                                 \
        lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
    })

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
    return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
    struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
    ssize_t ret = 0;
    char *addr = NULL;

    if (ctx->error < 0) {
        return;
    }

    addr = (char *)data;
    if ((int)*addr < 0xB0)
        ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
    else
        ret = mipi_dsi_generic_write(dsi, data, len);
    if (ret < 0) {
        dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
        ctx->error = ret;
    }
}
/*Tab A9 code for SR-AX6739A-01-718 by suyurui at 20230606 start*/
static void lcm_deep_suspend_setting(struct lcm *ctx)
{
    lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x59);
    lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA6);
    lcm_dcs_write_seq_static(ctx, 0x6D, 0x25, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x28, 0x00, 0x00);
    mdelay(10);
    lcm_dcs_write_seq_static(ctx, 0x10, 0x00, 0x00);
    mdelay(120);
    lcm_dcs_write_seq_static(ctx, 0xC9, 0x01, 0x00);
    mdelay(10);
};
/*Tab A9 code for SR-AX6739A-01-718 by suyurui at 20230606 end*/
/*Tab A9 code for SR-AX6739A-01-654 by suyurui at 20230606 start*/
static void lcm_suspend_setting(struct lcm *ctx)
{
    lcm_dcs_write_seq_static(ctx, 0x6D, 0x25, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x28, 0x00, 0x00);
    mdelay(20);
    lcm_dcs_write_seq_static(ctx, 0x10, 0x00, 0x00);
    mdelay(120);
};
/*Tab A9 code for SR-AX6739A-01-654 by suyurui at 20230606 end*/

/*Tab A9 code for AX6739A-2489 by suyurui at 20230724 start*/
static void lcm_panel_init(struct lcm *ctx)
{
    pr_err("%s+\n", __func__);
    lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A,0x59);
    lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5,0xA6);
    lcm_dcs_write_seq_static(ctx, 0xB2, 0x06,0x01,0x05,0x8D,0x47,0x44,0x09,0x03,0x66,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0x00,0x00,0x00,0x00,0x55,0x55,0x10,0x20,0x11,0x00);
    lcm_dcs_write_seq_static(ctx, 0xB3, 0x72,0x05,0x01,0x0D,0x81,0xFB,0x00,0x00,0xDB,0x00,0x00,0x44,0x40,0x40,0x00,0xF0,0x00,0xF0,0x00,0x00,0x00,0xF0,0x00,0xF0,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xF0);
    lcm_dcs_write_seq_static(ctx, 0xB4, 0x34,0x01,0x01,0x01,0x81,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x40,0x00,0xF0,0x00,0xF0,0x00,0x00,0x00,0xF0,0x00,0xF0,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xF0);
    lcm_dcs_write_seq_static(ctx, 0xB5, 0x10,0x0F,0x0E,0x0B,0x0F,0x26,0x26,0x33,0xA2,0x33,0x44,0x95,0x26,0x00,0x9F,0x3C,0x02,0x08,0x20,0x30,0x00,0x20,0x40,0x12,0x3C,0x00,0x00,0x00);
    lcm_dcs_write_seq_static(ctx, 0xB6, 0xAC,0xAC,0xA7,0xA7,0xAF,0xAF,0xA4,0xA4,0x80,0x80,0xA9,0xA9,0xA8,0xA8,0x8F,0x8F,0x8D,0x8D,0x93,0x93,0x91,0x91,0x00,0xF0,0x00,0xF0,0x00,0x00,0xFF,0xFF,0xFC);
    lcm_dcs_write_seq_static(ctx, 0xB7, 0xAC,0xAC,0xA7,0xA7,0xAD,0xAD,0xA2,0xA2,0x80,0x80,0xA9,0xA9,0xA8,0xA8,0x8E,0x8E,0x8C,0x8C,0x92,0x92,0x90,0x90,0x00,0xF0,0x00,0xF0,0x00,0x00,0xFF,0xFF,0xFC);
    //BACKWARD
    //{0xB6,31,{0xA6,0xA6,0xAC,0xAC,0xAD,0xAD,0xA2,0xA2,0x80,0x80,0xA9,0xA9,0xA8,0xA8,0x8C,0x8C,0x8E,0x8E,0x90,0x90,0x92,0x92,0x00,0xF0,0x00,0xF0,0x00,0x00,0xFF,0xFF,0xFC}},
    //{0xB7,31,{0xA6,0xA6,0xAC,0xAC,0xAF,0xAF,0xA4,0xA4,0x80,0x80,0xA9,0xA9,0xA8,0xA8,0x8D,0x8D,0x8F,0x8F,0x91,0x91,0x93,0x93,0x00,0xF0,0x00,0xF0,0x00,0x00,0xFF,0xFF,0xFC}},
    //GOUT_GOFF
    lcm_dcs_write_seq_static(ctx, 0xB8, 0xFF,0xFF,0xFF,0xFF,0xFF,0xF0,0xFF,0xFF,0xFF,0xFF,0xFF,0xF0,0x00,0xF0,0x00,0xF0,0x00,0x00,0x00,0xF0,0x00,0xF0,0x00,0x00,0x00,0x00,0x80,0x00,0x20,0x10,0x80,0x00);
    // GOUT_SLPIN
    lcm_dcs_write_seq_static(ctx, 0xB9, 0x01,0x55,0x55,0x55,0x55,0x55,0x50,0x55,0x55,0x55,0x55,0x55,0x50,0x00,0xF0,0x00,0x00,0x00,0x00,0x00,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
    //PWRCON_SEQ
    lcm_dcs_write_seq_static(ctx, 0xBB, 0x01,0x02,0x03,0x0A,0x04,0x13,0x14,0x12,0x16,0x46,0x00,0x15,0x16,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
    //PWRCON
    lcm_dcs_write_seq_static(ctx, 0xBC, 0x00,0x00,0x00,0x00,0x04,0x00,0xFF,0xF8,0x0B,0x24,0x50,0x5B,0xD9,0x99,0x40,0x99,0x00);
    //PWRCON_MODE
    lcm_dcs_write_seq_static(ctx, 0xBD, 0xA1,0xA2,0x52,0x2E,0x00,0x8F,0xC4,0x09,0xC3,0x86,0x03,0x2E,0x15,0x00,0x00);
    //PWRCON_REG
    lcm_dcs_write_seq_static(ctx, 0xBE, 0x0A,0x88,0x93,0x35,0x33,0x00,0x00,0x38,0x00,0xB2,0xAF,0xB2,0xAF,0x00,0x00,0x33);
    //POWER_TPS
    lcm_dcs_write_seq_static(ctx, 0xBF, 0x0C,0x19,0x0C,0x19,0x00,0x11,0x04,0x1D,0x17,0x22);
    //TEMP_SENSOR
    lcm_dcs_write_seq_static(ctx, 0xCA, 0x00,0x40,0x00,0x19,0x46,0x94,0x41,0x8F,0x00,0x00,0x50,0x50,0x5A,0x5A,0x64,0x64,0x32,0x32,0x11,0x00,0x01,0x01,0x0A,0x06,0x02,0x02,0x05,0x00,0x00,0x8C,0x3C,0x04);//VGHO=19VVGLO=-11V
    //POWER TRIM2
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x86,0x40);//VGH=20VVGL=-12V
    //BIST
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x40,0x13,0xFF,0xFF,0xFF,0xFF,0xFF,0x3F,0xFF,0x00,0xFF,0x00,0xCC);
    //TCON
    lcm_dcs_write_seq_static(ctx, 0xC1, 0x00,0x28,0x96,0x96,0x04,0x2C,0x28,0x04,0x3C,0x05,0x22,0x7F,0x02,0x22,0x77,0x11,0xFF,0xFF,0x00,0xE3,0x00);//GON_S1/GOFF_S1=2
    //TE
    lcm_dcs_write_seq_static(ctx, 0xC2, 0x00,0x05,0x01,0x10,0x00,0x01,0x00,0x02,0x21,0x43);
    //SRC_TIM
    lcm_dcs_write_seq_static(ctx, 0xC3, 0x00,0xFF,0x40,0x4D,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xDC,0x20,0x20,0x20,0x20,0x20,0x2A,0x2A,0x2A,0x2A,0x2A,0x2A);
    //SRCCON
    lcm_dcs_write_seq_static(ctx, 0xC4, 0x0C,0x77,0x28,0x49,0x00,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x20,0xF0,0x00);
    //H_SCAN
    lcm_dcs_write_seq_static(ctx, 0xC5, 0x03,0x2F,0x7D,0x78,0x23,0x80,0x04,0x05,0x07,0x07,0x04,0x18,0x22,0x05,0x00,0x20,0x11,0x0D,0x46,0x03,0x64,0xFF,0x01,0x14,0x38,0x20,0x00,0x83,0x20,0xFF);
    //H_SCAN
    lcm_dcs_write_seq_static(ctx, 0xC6, 0xA0,0x19,0x28,0x2B,0x2B,0x28,0x00,0x00,0x04,0x2E,0x00,0x11,0x40,0x00,0x98,0x98,0x60,0x80);
    //SET_GAMMA
    lcm_dcs_write_seq_static(ctx, 0xC7, 0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x20,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x20,0x42,0x00,0x21,0xFF,0xFF,0x11,0x11,0x03,0x0E,0x07,0x00,0x00,0x07,0x0E);
    lcm_dcs_write_seq_static(ctx, 0x80, 0xE8,0xE6,0xE2,0xDE,0xDA,0xD7,0xD4,0xD1,0xCE,0xC4,0xBB,0xB3,0xAC,0xA6,0xA0,0x96,0x8D,0x84,0x7C,0x7C,0x74,0x6C,0x63,0x59,0x4D,0x3C,0x31,0x22,0x1E,0x19,0x14,0x0F);
    lcm_dcs_write_seq_static(ctx, 0x81, 0xE8,0xE6,0xE2,0xDE,0xDA,0xD7,0xD3,0xD0,0xCD,0xC3,0xBA,0xB2,0xAB,0xA5,0x9F,0x95,0x8C,0x83,0x7B,0x7B,0x73,0x6B,0x62,0x59,0x4C,0x3C,0x31,0x22,0x1E,0x19,0x14,0x0E);
    lcm_dcs_write_seq_static(ctx, 0x82, 0xE8,0xE6,0xE2,0xDE,0xDA,0xD7,0xD4,0xD1,0xCE,0xC3,0xBA,0xB3,0xAC,0xA6,0xA0,0x95,0x8C,0x84,0x7C,0x7B,0x73,0x6B,0x63,0x59,0x4C,0x3C,0x31,0x22,0x1E,0x19,0x14,0x0E);
    lcm_dcs_write_seq_static(ctx, 0x83, 0x01,0x0C,0x07,0x01,0x00,0x0B,0x07,0x02,0x00,0x0B,0x07,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
    lcm_dcs_write_seq_static(ctx, 0x84, 0xEB,0xFA,0xD7,0xED,0x14,0x05,0x1F,0xF0,0x00,0xD1,0x8F,0xDA,0xEE,0x7D,0xAC,0xD3,0x67,0xB8,0xD6,0x90,0x7C,0x47,0x82,0xF0,0xD7,0xA3,0xDC);
    //TCON_OPT
    lcm_dcs_write_seq_static(ctx, 0xC8, 0x46,0x00,0xC9,0x0D,0xF0,0x00,0x00,0x00,0x00,0x23);
    lcm_dcs_write_seq_static(ctx, 0xD5, 0x01,0XF3,0X10);
    //PWRCON_VCOM
    //{0x89, 7,{0x90,0x90,0x11,0x00,0x90,0x90,0x11}},
    lcm_dcs_write_seq_static(ctx, 0xD0, 0x0C,0x23,0x18,0xFF,0xFF,0x00,0x80,0x0C,0xFF,0x0F,0x40,0x00,0x08,0x10);
    //PWM
    lcm_dcs_write_seq_static(ctx, 0xE0, 0x0C,0x00,0xB8,0x10,0x00,0x86,0x06,0x00,0x0B,0x05,0x01,0x00,0x1F,0x00,0x00,0x00,0x00,0x24,0x12,0x88,0x11,0x48,0x42,0x00,0x00);//PWM12BIT/DIMMING4FRAME
    lcm_dcs_write_seq_static(ctx, 0x51, 0x00,0x00);
    lcm_dcs_write_seq_static(ctx, 0x53, 0x2C,0x00);
    lcm_dcs_write_seq_static(ctx, 0x55, 0x00,0x00);
    lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A,0x59);
    lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5,0xA6);
    lcm_dcs_write_seq_static(ctx, 0x35, 0x00,0x00);
    lcm_dcs_write_seq_static(ctx, 0x11, 0x00,0x00);
    mdelay(115);
    lcm_dcs_write_seq_static(ctx, 0x29, 0x00,0x00);
    mdelay(10);
    lcm_dcs_write(ctx, gs_bl_tb0, sizeof(gs_bl_tb0));
    mdelay(5);
    lcm_dcs_write_seq_static(ctx, 0x6D, 0x02,0x00);

    pr_err("%s-\n", __func__);
}
/*Tab A9 code for AX6739A-2489 by suyurui at 20230724 end*/
static int lcm_disable(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    pr_err("%s\n", __func__);

    if (!ctx->enabled)
        return 0;

    if (ctx->backlight) {
        ctx->backlight->props.power = FB_BLANK_POWERDOWN;
        backlight_update_status(ctx->backlight);
    }

    ctx->enabled = false;

    return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{

    struct lcm *ctx = panel_to_lcm(panel);

    if (!ctx->prepared)
        return 0;

    pr_err("%s+\n", __func__);
    /*Tab A9 code for SR-AX6739A-01-654 by suyurui at 20230605 start*/
    if (g_smart_wakeup_flag) {
        lcm_suspend_setting(ctx);
    } else {
        lcm_deep_suspend_setting(ctx);
        ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
        gpiod_set_value(ctx->bias_neg, 0);
        devm_gpiod_put(ctx->dev, ctx->bias_neg);
        /*Tab A9 code for AX6739A-769 by suyurui at 20230607 start*/
        mdelay(7);
        /*Tab A9 code for AX6739A-769 by suyurui at 20230607 end*/
        ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
        gpiod_set_value(ctx->bias_pos, 0);
        devm_gpiod_put(ctx->dev, ctx->bias_pos);
    }
    /*Tab A9 code for SR-AX6739A-01-654 by suyurui at 20230605 end*/

    ctx->error = 0;
    ctx->prepared = false;
    pr_err("%s-\n", __func__);
    return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    int ret = 0;

    pr_err("%s+\n", __func__);
    if (ctx->prepared) {
        return 0;
    }
    /*Tab A9 code for SR-AX6739A-01-717 by suyurui at 20230609 start*/
    ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
    gpiod_set_value(ctx->reset_gpio, 0);
    pr_err("lcm reset pull low \n");
    mdelay(5);
    /*Tab A9 code for SR-AX6739A-01-717 by suyurui at 20230609 end*/

    ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->bias_pos)) {
      dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n", __func__, PTR_ERR(ctx->bias_pos));
      return PTR_ERR(ctx->bias_pos);
    }
    gpiod_set_value(ctx->bias_pos, 1);
    devm_gpiod_put(ctx->dev, ctx->bias_pos);
    _lcm_i2c_write_bytes(0x0, 0x14);

    mdelay(5);
    ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->bias_neg)) {
        dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n", __func__, PTR_ERR(ctx->bias_neg));
        return PTR_ERR(ctx->bias_neg);
    }
    gpiod_set_value(ctx->bias_neg, 1);
    devm_gpiod_put(ctx->dev, ctx->bias_neg);

    _lcm_i2c_write_bytes(0x1, 0x14);
    /*Tab A9 code for SR-AX6739A-01-717 by suyurui at 20230609 start*/
    mdelay(10);
    /*Tab A9 code for SR-AX6739A-01-717 by suyurui at 20230609 end*/
    gpiod_set_value(ctx->reset_gpio, 1);
    /*Tab A9 code for SR-AX6739A-01-717 by suyurui at 20230603 start*/
    mdelay(10);
    /*Tab A9 code for SR-AX6739A-01-717 by suyurui at 20230603 end*/
    gpiod_set_value(ctx->reset_gpio, 0);
    /*Tab A9 code for SR-AX6739A-01-715 by suyurui at 20230613 start*/
    mdelay(2);
    gpiod_set_value(ctx->reset_gpio, 1);
    devm_gpiod_put(ctx->dev, ctx->reset_gpio);
    mdelay(25);
    /*Tab A9 code for SR-AX6739A-01-715 by suyurui at 20230613 end*/

    lcm_panel_init(ctx);

    ret = ctx->error;
    if (ret < 0) {
        lcm_unprepare(panel);
    }

    ctx->prepared = true;

#ifdef VENDOR_EDIT
    // shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
    lcd_queue_load_tp_fw();
#endif

    pr_err("%s-\n", __func__);
    return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    pr_err("%s\n", __func__);

    if (ctx->enabled) {
        return 0;
    }

    if (ctx->backlight) {
        ctx->backlight->props.power = FB_BLANK_UNBLANK;
        backlight_update_status(ctx->backlight);
    }

    ctx->enabled = true;

    return 0;
}
/*Tab A9 code for AX6739A-1174 by suyurui at 20230615 start*/
#define HAC (800)
#define HFP (40)
#define HSA (4)
#define HBP (44)
#define VAC (1340)
#define VFP (150)
#define VSA (4)
#define VBP (40)
/*Tab A9 code for AX6739A-447 by wenghailong at 20230608 start*/
#define PHYSICAL_WIDTH                  (113040)
#define PHYSICAL_HEIGHT                 (189342)
/*Tab A9 code for AX6739A-447 by wenghailong at 20230608 end*/

static const struct drm_display_mode default_mode = {
    /*Tab A9 code for AX6739A-447 by wenghailong at 20230608 start*/
    .clock = 81732, //.clock=htotal*vtotal*fps/1000
    /*Tab A9 code for AX6739A-447 by wenghailong at 20230608 end*/
    .hdisplay = HAC,
    .hsync_start = HAC + HFP,
    .hsync_end = HAC + HFP + HSA,
    .htotal = HAC + HFP + HSA + HBP,
    .vdisplay = VAC,
    .vsync_start = VAC + VFP,
    .vsync_end = VAC + VFP + VSA,
    .vtotal = VAC + VFP + VSA + VBP,
    .width_mm = PHYSICAL_WIDTH / 1000,
    .height_mm = PHYSICAL_HEIGHT / 1000,
};

#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {
    .pll_clk = 257,
    .cust_esd_check = 1,
    .esd_check_enable = 1,
    .lcm_esd_check_table[0] = {
        .cmd = 0x0A,
        .count = 1,
        .para_list[0] = 0x9C,
    },
    /*Tab A9 code for AX6739A-1470 by suyurui at 20230625 start*/
    .lcm_esd_check_table[1] = {
        .cmd = 0x09,
        .count = 3,
        .para_list[0] = 0x80,
        .para_list[1] = 0x73,
        .para_list[2] = 0x06,
    },
    /*Tab A9 code for AX6739A-1470 by suyurui at 20230625 end*/
    /*Tab A9 code for AX6739A-447 by wenghailong at 20230608 start*/
    .physical_width_um = PHYSICAL_WIDTH,
    .physical_height_um = PHYSICAL_HEIGHT,
    /*Tab A9 code for AX6739A-447 by wenghailong at 20230608 end*/
    .data_rate = 514,
};
/*Tab A9 code for AX6739A-1174 by suyurui at 20230615 end*/

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
                 unsigned int level)
{

    char bl_tb0[] = {0x51, 0x0f, 0xff};
    char bl_tb1[] = {0x53, 0x24};

    if (level == 0) {
        cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
        pr_err("close diming\n");
    }

    level = level * 4095 / 255;
    bl_tb0[1] = level >> 8;
    bl_tb0[2] = level & 0xFF;
    /*Tab A9 code for SR-AX6739A-01-716 by suyurui at 20230605 start*/
    if (level != 0) {
        memcpy(gs_bl_tb0,bl_tb0,sizeof(bl_tb0));
    }
    /*Tab A9 code for SR-AX6739A-01-716 by suyurui at 20230605 end*/
    pr_err("%s bl_tb0[1] = 0x%x,bl_tb0[2]= 0x%x\n", __func__, bl_tb0[1], bl_tb0[2]);

    if (!cb) {
        return -1;
        pr_err("cb error\n");
    }

    cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
    return 0;
}

struct drm_display_mode *get_mode_by_id_hfp(struct drm_connector *connector,
    unsigned int mode)
{
    struct drm_display_mode *m = NULL;
    unsigned int i = 0;

    list_for_each_entry(m, &connector->modes, head) {
        if (i == mode) {
            return m;
        }
        i++;
    }
    return NULL;
}
static int mtk_panel_ext_param_set(struct drm_panel *panel,
            struct drm_connector *connector, unsigned int mode)
{
    struct mtk_panel_ext *ext = find_panel_ext(panel);
    int ret = 0;
    struct drm_display_mode *m = get_mode_by_id_hfp(connector, mode);

    if (drm_mode_vrefresh(m) == 60) {
        ext->params = &ext_params;
    } else {
        ret = 1;
    }

    if (!ret) {
        current_fps = drm_mode_vrefresh(m);
    }

    return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
    struct lcm *ctx = panel_to_lcm(panel);

    ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
    gpiod_set_value(ctx->reset_gpio, on);
    devm_gpiod_put(ctx->dev, ctx->reset_gpio);

    return 0;
}

static struct mtk_panel_funcs ext_funcs = {
    .reset = panel_ext_reset,
    .set_backlight_cmdq = lcm_setbacklight_cmdq,
    .ext_param_set = mtk_panel_ext_param_set,
};
#endif

static int lcm_get_modes(struct drm_panel *panel,
                    struct drm_connector *connector)
{
    struct drm_display_mode *mode = NULL;

    mode = drm_mode_duplicate(connector->dev, &default_mode);
    if (!mode) {
        dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
             default_mode.hdisplay, default_mode.vdisplay,
             drm_mode_vrefresh(&default_mode));
        return -ENOMEM;
    }

    drm_mode_set_name(mode);
    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
    drm_mode_probed_add(connector, mode);

    connector->display_info.width_mm = 10;
    connector->display_info.height_mm = 17;

    return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
    .disable = lcm_disable,
    .unprepare = lcm_unprepare,
    .prepare = lcm_prepare,
    .enable = lcm_enable,
    .get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
    struct device *dev = &dsi->dev;
    struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
    struct lcm *ctx;
    struct device_node *backlight;
    int ret;

    pr_err("%s+ lcm,nl9951 gx boe\n", __func__);

    /*Tab A9 code for SR-AX6739A-01-768 by yuli at 20230718 start*/
    if (lcm_detect_panel("nl9951_gx_boe")) {
        pr_err("%s- lcm,nl9951_gx_boe\n", __func__);
        return -ENODEV;
    }
    /*Tab A9 code for SR-AX6739A-01-768 by yuli at 20230718 end*/

    dsi_node = of_get_parent(dev->of_node);
    if (dsi_node) {
        endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
        if (endpoint) {
            remote_node = of_graph_get_remote_port_parent(endpoint);
            if (!remote_node) {
                pr_err("No panel connected,skip probe lcm\n");
                return -ENODEV;
            }
            pr_err("device node name:%s\n", remote_node->name);
        }
    }
    if (remote_node != dev->of_node) {
        pr_err("%s+ skip probe due to not current lcm\n", __func__);
        return -ENODEV;
    }

    ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    mipi_dsi_set_drvdata(dsi, ctx);

    ctx->dev = dev;
    dsi->lanes = 4;
    dsi->format = MIPI_DSI_FMT_RGB888;
    dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
            MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_BURST|
            MIPI_DSI_CLOCK_NON_CONTINUOUS;

    backlight = of_parse_phandle(dev->of_node, "backlight", 0);
    if (backlight) {
        ctx->backlight = of_find_backlight_by_node(backlight);
        of_node_put(backlight);

        if (!ctx->backlight)
            return -EPROBE_DEFER;
    }

    ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio)) {
        dev_info(dev, "cannot get lcd reset-gpios %ld\n",
             PTR_ERR(ctx->reset_gpio));
        return PTR_ERR(ctx->reset_gpio);
    }
    devm_gpiod_put(dev, ctx->reset_gpio);

    // ctx->ctp_reset_gpio = devm_gpiod_get_index(dev, "reset", 1,GPIOD_OUT_HIGH);
    // if (IS_ERR(ctx->ctp_reset_gpio)) {
    //     dev_info(dev, "cannot get ctp reset-gpios %ld\n",
    //          PTR_ERR(ctx->ctp_reset_gpio));
    //     return PTR_ERR(ctx->ctp_reset_gpio);
    // }
    // devm_gpiod_put(dev, ctx->ctp_reset_gpio);

    pr_err("%s+ parse dtbo\n", __func__);
    ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->bias_pos)) {
        dev_info(dev, "cannot get bias-gpios 0 %ld\n",
                PTR_ERR(ctx->bias_pos));
        return PTR_ERR(ctx->bias_pos);
    }
    devm_gpiod_put(dev, ctx->bias_pos);

    ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->bias_neg)) {
        dev_info(dev, "cannot get bias-gpios 1 %ld\n",
                PTR_ERR(ctx->bias_neg));
        return PTR_ERR(ctx->bias_neg);
    }
    devm_gpiod_put(dev, ctx->bias_neg);


    ctx->prepared = true;
    ctx->enabled = true;
    drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

    drm_panel_add(&ctx->panel);

    ret = mipi_dsi_attach(dsi);
    if (ret < 0)
        drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
    mtk_panel_tch_handle_reg(&ctx->panel);
    ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
    if (ret < 0)
        return ret;

#endif

    pr_err("%s- lcm,nl9951_gx_boe\n", __func__);

    return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
    struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
    struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

    mipi_dsi_detach(dsi);
    drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
    mtk_panel_detach(ext_ctx);
    mtk_panel_remove(ext_ctx);
#endif

    return 0;
}

/*Tab A9 code for SR-AX6739A-01-768 by yuli at 20230718 start*/
static const struct of_device_id lcm_of_match[] = {
    {
        .compatible = "panel,lcm,video",
    },
    {}
};
/*Tab A9 code for SR-AX6739A-01-768 by yuli at 20230718 end*/

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
    .probe = lcm_probe,
    .remove = lcm_remove,
    .driver = {
        .name = "lcd_nl9951_gx_boe_video",
        .owner = THIS_MODULE,
        .of_match_table = lcm_of_match,
    },
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Elon Hsu <elon.hsu@mediatek.com>");
MODULE_DESCRIPTION("lcm r66451 CMD AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
