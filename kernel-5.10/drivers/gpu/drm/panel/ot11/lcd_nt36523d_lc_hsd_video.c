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

extern bool g_smart_wakeup_flag;

static int current_fps = 60;
extern int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value);
/*Tab A9 code for SR-AX6739A-01-734 by yuli at 20230606 start*/
#include <linux/string.h>
static char gs_bl_tb0[3] = {0x51, 0x06, 0x66};//4096 * 40% = 1638
/*Tab A9 code for SR-AX6739A-01-734 by yuli at 20230606 end*/

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
    if ((int)*addr < 0xB0) {
        ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
    } else {
        ret = mipi_dsi_generic_write(dsi, data, len);
    }
    if (ret < 0) {
        dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
        ctx->error = ret;
    }
}

static void lcm_suspend_setting(struct lcm *ctx)
{
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x28);
    mdelay(20);
    lcm_dcs_write_seq_static(ctx, 0x10);
    mdelay(120);
};

static void lcm_panel_init(struct lcm *ctx)
{
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x05, 0xA9);
    lcm_dcs_write_seq_static(ctx, 0x06, 0xC0);
    lcm_dcs_write_seq_static(ctx, 0x07, 0x69);
    lcm_dcs_write_seq_static(ctx, 0x08, 0x37);
    lcm_dcs_write_seq_static(ctx, 0x0D, 0x63);
    lcm_dcs_write_seq_static(ctx, 0x0E, 0x87);
    lcm_dcs_write_seq_static(ctx, 0x0F, 0x55);
    lcm_dcs_write_seq_static(ctx, 0x69, 0x90);
    lcm_dcs_write_seq_static(ctx, 0x75, 0x30);
    lcm_dcs_write_seq_static(ctx, 0x77, 0xB3);
    lcm_dcs_write_seq_static(ctx, 0x94, 0xC0);
    lcm_dcs_write_seq_static(ctx, 0x95, 0x09);
    lcm_dcs_write_seq_static(ctx, 0x96, 0x09);

    //CMD2_Page0
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    //R(+)
    lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x08, 0x00, 0x21, 0x00, 0x49, 0x00, 0x69, 0x00, 0x84, 0x00, 0x9B, 0x00, 0xB0, 0x00, 0xC2);
    lcm_dcs_write_seq_static(ctx, 0xB1, 0x00, 0xD3, 0x01, 0x0C, 0x01, 0x34, 0x01, 0x75, 0x01, 0xA2, 0x01, 0xEC, 0x02, 0x22, 0x02, 0x24);
    lcm_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x5B, 0x02, 0x9C, 0x02, 0xCB, 0x03, 0x01, 0x03, 0x27, 0x03, 0x52, 0x03, 0x62, 0x03, 0x71);
    lcm_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x83, 0x03, 0x98, 0x03, 0xB0, 0x03, 0xC8, 0x03, 0xD8, 0x03, 0xDB);
    //G(+)
    lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x08, 0x00, 0x21, 0x00, 0x49, 0x00, 0x69, 0x00, 0x84, 0x00, 0x9B, 0x00, 0xB0, 0x00, 0xC2);
    lcm_dcs_write_seq_static(ctx, 0xB5, 0x00, 0xD3, 0x01, 0x0C, 0x01, 0x34, 0x01, 0x75, 0x01, 0xA2, 0x01, 0xEC, 0x02, 0x22, 0x02, 0x24);
    lcm_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x5B, 0x02, 0x9C, 0x02, 0xCB, 0x03, 0x01, 0x03, 0x27, 0x03, 0x52, 0x03, 0x62, 0x03, 0x71);
    lcm_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x83, 0x03, 0x98, 0x03, 0xB0, 0x03, 0xC8, 0x03, 0xD8, 0x03, 0xDB);
    //B(+)
    lcm_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x08, 0x00, 0x21, 0x00, 0x49, 0x00, 0x69, 0x00, 0x84, 0x00, 0x9B, 0x00, 0xB0, 0x00, 0xC2);
    lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0xD3, 0x01, 0x0C, 0x01, 0x34, 0x01, 0x75, 0x01, 0xA2, 0x01, 0xEC, 0x02, 0x22, 0x02, 0x24);
    lcm_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x5B, 0x02, 0x9C, 0x02, 0xCB, 0x03, 0x01, 0x03, 0x27, 0x03, 0x52, 0x03, 0x62, 0x03, 0x71);
    lcm_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x83, 0x03, 0x98, 0x03, 0xB0, 0x03, 0xC8, 0x03, 0xD8, 0x03, 0xDB);
    //R+C1
    lcm_dcs_write_seq_static(ctx, 0xC6, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xC7, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xC8, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xC9, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xCA, 0x00);
    //R-C1
    lcm_dcs_write_seq_static(ctx, 0xCB, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xCC, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xCD, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xCE, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xCF, 0x00);
    //G+C1
    lcm_dcs_write_seq_static(ctx, 0xD0, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD1, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD2, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD3, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD4, 0x00);
    //G-C1
    lcm_dcs_write_seq_static(ctx, 0xD5, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD6, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD7, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD8, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xD9, 0x00);
    //B+C1
    lcm_dcs_write_seq_static(ctx, 0xDA, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xDB, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xDC, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xDD, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xDE, 0x00);
    //B-C1
    lcm_dcs_write_seq_static(ctx, 0xDF, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE0, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE1, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE2, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xE3, 0x00);
    //CMD2_Page1
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x21);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    //R(-)
    lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x19, 0x00, 0x41, 0x00, 0x61, 0x00, 0x7C, 0x00, 0x93, 0x00, 0xA8, 0x00, 0xBA);
    lcm_dcs_write_seq_static(ctx, 0xB1, 0x00, 0xCB, 0x01, 0x04, 0x01, 0x2C, 0x01, 0x6D, 0x01, 0x9A, 0x01, 0xE4, 0x02, 0x1A, 0x02, 0x1C);
    lcm_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x53, 0x02, 0x94, 0x02, 0xC3, 0x02, 0xF9, 0x03, 0x1F, 0x03, 0x4A, 0x03, 0x5A, 0x03, 0x69);
    lcm_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x7B, 0x03, 0x90, 0x03, 0xA8, 0x03, 0xC0, 0x03, 0xD0, 0x03, 0xD3);
    //G(-)
    lcm_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x00, 0x00, 0x19, 0x00, 0x41, 0x00, 0x61, 0x00, 0x7C, 0x00, 0x93, 0x00, 0xA8, 0x00, 0xBA);
    lcm_dcs_write_seq_static(ctx, 0xB5, 0x00, 0xCB, 0x01, 0x04, 0x01, 0x2C, 0x01, 0x6D, 0x01, 0x9A, 0x01, 0xE4, 0x02, 0x1A, 0x02, 0x1C);
    lcm_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x53, 0x02, 0x94, 0x02, 0xC3, 0x02, 0xF9, 0x03, 0x1F, 0x03, 0x4A, 0x03, 0x5A, 0x03, 0x69);
    lcm_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x7B, 0x03, 0x90, 0x03, 0xA8, 0x03, 0xC0, 0x03, 0xD0, 0x03, 0xD3);
    //B(-)
    lcm_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x00, 0x00, 0x19, 0x00, 0x41, 0x00, 0x61, 0x00, 0x7C, 0x00, 0x93, 0x00, 0xA8, 0x00, 0xBA);
    lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0xCB, 0x01, 0x04, 0x01, 0x2C, 0x01, 0x6D, 0x01, 0x9A, 0x01, 0xE4, 0x02, 0x1A, 0x02, 0x1C);
    lcm_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x53, 0x02, 0x94, 0x02, 0xC3, 0x02, 0xF9, 0x03, 0x1F, 0x03, 0x4A, 0x03, 0x5A, 0x03, 0x69);
    lcm_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x7B, 0x03, 0x90, 0x03, 0xA8, 0x03, 0xC0, 0x03, 0xD0, 0x03, 0xD3);

    lcm_dcs_write_seq_static(ctx, 0xFF, 0x23);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x00, 0x80);
    lcm_dcs_write_seq_static(ctx, 0x07, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x08, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x09, 0x00);

    lcm_dcs_write_seq_static(ctx, 0xFF, 0x24);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x60, 0xC8);
    lcm_dcs_write_seq_static(ctx, 0x61, 0x3C);
    lcm_dcs_write_seq_static(ctx, 0x63, 0x50);
    lcm_dcs_write_seq_static(ctx, 0x91, 0x20);
    lcm_dcs_write_seq_static(ctx, 0x92, 0x4E);
    lcm_dcs_write_seq_static(ctx, 0x93, 0x82);
    lcm_dcs_write_seq_static(ctx, 0x94, 0x13);
    lcm_dcs_write_seq_static(ctx, 0xC2, 0xC6, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x01, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x02, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x03, 0x03);
    lcm_dcs_write_seq_static(ctx, 0x04, 0x03);
    lcm_dcs_write_seq_static(ctx, 0x05, 0x22);
    lcm_dcs_write_seq_static(ctx, 0x06, 0x23);
    lcm_dcs_write_seq_static(ctx, 0x07, 0x1C);
    lcm_dcs_write_seq_static(ctx, 0x08, 0x1D);
    lcm_dcs_write_seq_static(ctx, 0x09, 0x0C);
    lcm_dcs_write_seq_static(ctx, 0x0A, 0x0D);
    lcm_dcs_write_seq_static(ctx, 0x0B, 0x0E);
    lcm_dcs_write_seq_static(ctx, 0x0C, 0x0F);
    lcm_dcs_write_seq_static(ctx, 0x0D, 0x04);
    lcm_dcs_write_seq_static(ctx, 0x0E, 0x05);
    lcm_dcs_write_seq_static(ctx, 0x0F, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x10, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x11, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x12, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x13, 0x03);
    lcm_dcs_write_seq_static(ctx, 0x14, 0x03);
    lcm_dcs_write_seq_static(ctx, 0x15, 0x03);
    lcm_dcs_write_seq_static(ctx, 0x16, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x17, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x18, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x19, 0x03);
    lcm_dcs_write_seq_static(ctx, 0x1A, 0x03);
    lcm_dcs_write_seq_static(ctx, 0x1B, 0x22);
    lcm_dcs_write_seq_static(ctx, 0x1C, 0x23);
    lcm_dcs_write_seq_static(ctx, 0x1D, 0x1C);
    lcm_dcs_write_seq_static(ctx, 0x1E, 0x1D);
    lcm_dcs_write_seq_static(ctx, 0x1F, 0x0C);
    lcm_dcs_write_seq_static(ctx, 0x20, 0x0D);
    lcm_dcs_write_seq_static(ctx, 0x21, 0x0E);
    lcm_dcs_write_seq_static(ctx, 0x22, 0x0F);
    lcm_dcs_write_seq_static(ctx, 0x23, 0x04);
    lcm_dcs_write_seq_static(ctx, 0x24, 0x05);
    lcm_dcs_write_seq_static(ctx, 0x25, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x26, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x27, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x28, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x29, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x2A, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x2B, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
    lcm_dcs_write_seq_static(ctx, 0x30, 0x40);
    lcm_dcs_write_seq_static(ctx, 0x33, 0x40);
    lcm_dcs_write_seq_static(ctx, 0x34, 0x02);
    lcm_dcs_write_seq_static(ctx, 0x37, 0x22);
    lcm_dcs_write_seq_static(ctx, 0x39, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x3A, 0x05);
    lcm_dcs_write_seq_static(ctx, 0x3B, 0x4A);
    lcm_dcs_write_seq_static(ctx, 0x3D, 0x52);
    lcm_dcs_write_seq_static(ctx, 0xAB, 0x22);
    lcm_dcs_write_seq_static(ctx, 0x4D, 0x21);
    lcm_dcs_write_seq_static(ctx, 0x4E, 0x43);
    lcm_dcs_write_seq_static(ctx, 0x51, 0x12);
    lcm_dcs_write_seq_static(ctx, 0x52, 0x34);
    lcm_dcs_write_seq_static(ctx, 0x55, 0x41, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x56, 0x04);
    lcm_dcs_write_seq_static(ctx, 0x58, 0x21);
    lcm_dcs_write_seq_static(ctx, 0x59, 0x21);
    lcm_dcs_write_seq_static(ctx, 0x5A, 0x05);
    lcm_dcs_write_seq_static(ctx, 0x5B, 0x1A);
    lcm_dcs_write_seq_static(ctx, 0x5C, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x5D, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x5E, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x5F, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x65, 0xC2);
    lcm_dcs_write_seq_static(ctx, 0x7A, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x7B, 0x10);
    lcm_dcs_write_seq_static(ctx, 0x7E, 0x20);
    lcm_dcs_write_seq_static(ctx, 0x7F, 0x3C);
    lcm_dcs_write_seq_static(ctx, 0x80, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x82, 0x88);
    lcm_dcs_write_seq_static(ctx, 0x97, 0xC2);
    lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xC4, 0x20);
    lcm_dcs_write_seq_static(ctx, 0xDB, 0xF2);
    lcm_dcs_write_seq_static(ctx, 0xDC, 0x4D);
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x02, 0x00, 0x00, 0x41);
    lcm_dcs_write_seq_static(ctx, 0x05, 0x04);
    lcm_dcs_write_seq_static(ctx, 0x13, 0x04);
    lcm_dcs_write_seq_static(ctx, 0x14, 0x31);
    lcm_dcs_write_seq_static(ctx, 0x19, 0x07);
    lcm_dcs_write_seq_static(ctx, 0x1B, 0x11);
    lcm_dcs_write_seq_static(ctx, 0x3D, 0x00, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00);

    lcm_dcs_write_seq_static(ctx, 0xFF, 0x26);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x00, 0xA0);
    lcm_dcs_write_seq_static(ctx, 0x02, 0x09);
    lcm_dcs_write_seq_static(ctx, 0x04, 0x48);
    lcm_dcs_write_seq_static(ctx, 0x06, 0x30);
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x27);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x58, 0xA4);
    lcm_dcs_write_seq_static(ctx, 0x59, 0x00, 0x09);
    lcm_dcs_write_seq_static(ctx, 0x5A, 0x00, 0x88);
    lcm_dcs_write_seq_static(ctx, 0x5B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x5C, 0x00, 0x00, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x5D, 0x00, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x5E, 0x00, 0x7C);
    lcm_dcs_write_seq_static(ctx, 0x5F, 0x05);
    lcm_dcs_write_seq_static(ctx, 0x60, 0x05, 0x64);
    lcm_dcs_write_seq_static(ctx, 0x61, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x62, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x63, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x64, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x65, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x66, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x67, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x68, 0x00,  0x00);
    lcm_dcs_write_seq_static(ctx, 0xC0, 0x08);
    lcm_dcs_write_seq_static(ctx, 0xC1, 0x03);
    lcm_dcs_write_seq_static(ctx, 0xD1, 0x24);
    lcm_dcs_write_seq_static(ctx, 0xD2, 0x32);
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x2A);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x64, 0x16);
    lcm_dcs_write_seq_static(ctx, 0x6A, 0x16);
    lcm_dcs_write_seq_static(ctx, 0x70, 0x16);
    lcm_dcs_write_seq_static(ctx, 0x79, 0x16);
    lcm_dcs_write_seq_static(ctx, 0x7C, 0x16);
    lcm_dcs_write_seq_static(ctx, 0x97, 0x3C);
    lcm_dcs_write_seq_static(ctx, 0x98, 0x02);
    lcm_dcs_write_seq_static(ctx, 0x99, 0x95);
    lcm_dcs_write_seq_static(ctx, 0x9A, 0x06);
    lcm_dcs_write_seq_static(ctx, 0x9B, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x9C, 0x0B);
    lcm_dcs_write_seq_static(ctx, 0x9D, 0x0A);
    lcm_dcs_write_seq_static(ctx, 0x9E, 0x90);
    lcm_dcs_write_seq_static(ctx, 0xA2, 0x33);
    lcm_dcs_write_seq_static(ctx, 0xA3, 0xF0);
    lcm_dcs_write_seq_static(ctx, 0xA4, 0xC0);
    lcm_dcs_write_seq_static(ctx, 0xE8, 0x00);
    lcm_dcs_write_seq_static(ctx, 0xFF, 0xD0);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x45, 0x41);
    lcm_dcs_write_seq_static(ctx, 0xFF, 0xF0);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x9E, 0x03);
    lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
    lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
    lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x3B, 0x03, 0x14,  0x82, 0x0A, 0x0A);
    lcm_dcs_write_seq_static(ctx, 0xBB, 0x13);
    lcm_dcs_write_seq_static(ctx, 0xE9, 0x02);

    lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x53, 0x24);
    lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
    lcm_dcs_write_seq_static(ctx, 0x68, 0x04, 0x01);

    lcm_dcs_write_seq_static(ctx, 0x11);
    mdelay(105);
    lcm_dcs_write_seq_static(ctx, 0x29);
    mdelay(40);
    lcm_dcs_write(ctx, gs_bl_tb0, sizeof(gs_bl_tb0));

    pr_info("%s-\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    pr_info("%s\n", __func__);

    if (!ctx->enabled) {
        return 0;
    }

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

    if (!ctx->prepared) {
        return 0;
    }

    pr_info("%s+\n", __func__);

    lcm_suspend_setting(ctx);
    if (!g_smart_wakeup_flag) {
        ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
        gpiod_set_value(ctx->bias_neg, 0);
        devm_gpiod_put(ctx->dev, ctx->bias_neg);
        mdelay(5);
        ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
        gpiod_set_value(ctx->bias_pos, 0);
        devm_gpiod_put(ctx->dev, ctx->bias_pos);
    }

    ctx->error = 0;
    ctx->prepared = false;

    pr_info("%s-\n", __func__);

    return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    int ret = 0;

    pr_info("%s+\n", __func__);
    if (ctx->prepared) {
        return 0;
    }

    ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
    gpiod_set_value(ctx->reset_gpio, 0);
    pr_info("lcm reset pull low\n");
    mdelay(5);

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
    /*Tab A9 code for SR-AX6739A-01-733 by yuli at 20230619 start*/
    mdelay(12);
    /*Tab A9 code for SR-AX6739A-01-733 by yuli at 20230619 end*/

    gpiod_set_value(ctx->reset_gpio, 1);
    mdelay(5);
    gpiod_set_value(ctx->reset_gpio, 0);
    mdelay(5);
    gpiod_set_value(ctx->reset_gpio, 1);
    mdelay(5);
    gpiod_set_value(ctx->reset_gpio, 0);
    mdelay(5);
    gpiod_set_value(ctx->reset_gpio, 1);
    devm_gpiod_put(ctx->dev, ctx->reset_gpio);
    /*Tab A9 code for SR-AX6739A-01-733 by yuli at 20230619 start*/
    mdelay(12);
    /*Tab A9 code for SR-AX6739A-01-733 by yuli at 20230619 end*/

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

    pr_info("%s-\n", __func__);
    return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
    struct lcm *ctx = panel_to_lcm(panel);
    pr_info("%s\n", __func__);

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

/*Tab A9 code for AX6739A-1906 by yuli at 20230703 start*/
#define HAC (800)
#define HFP (88)
#define HSA (10)
#define HBP (84)
#define VAC (1340)
#define VFP (130)
#define VSA (4)
#define VBP (16)
/*Tab A9 code for AX6739A-447 by wenghailong at 20230608 start*/
#define PHYSICAL_WIDTH                  (113040)
#define PHYSICAL_HEIGHT                 (189342)
/*Tab A9 code for AX6739A-447 by wenghailong at 20230608 end*/
#define HTOTAL (HAC + HFP + HSA + HBP)
#define VTOTAL (VAC + VFP+  VSA + VBP)
#define CURRENT_FPS  60

static const struct drm_display_mode default_mode = {
    .clock = HTOTAL * VTOTAL * CURRENT_FPS / 1000, //.clock=htotal*vtotal*fps/1000
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
    .pll_clk = 276,
    .cust_esd_check = 1,
    .esd_check_enable = 1,
    .lcm_esd_check_table[0] = {
        .cmd = 0x0A,
        .count = 1,
        .para_list[0] = 0x9C,
    },
    /*Tab A9 code for SR-AX6739A-01-734 by yuli at 20230606 start*/
    .lcm_esd_check_table[1] = {
        .cmd = 0xAB,
        .count = 2,
        .para_list[0] = 0x00,
        .para_list[1] = 0x00,
    },
    /*Tab A9 code for SR-AX6739A-01-734 by yuli at 20230606 end*/
    /*Tab A9 code for AX6739A-447 by wenghailong at 20230608 start*/
    .physical_width_um = PHYSICAL_WIDTH,
    .physical_height_um = PHYSICAL_HEIGHT,
    /*Tab A9 code for AX6739A-447 by wenghailong at 20230608 end*/
    .data_rate = 552,
};
/*Tab A9 code for AX6739A-1906 by yuli at 20230703 end*/

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
                 unsigned int level)
{

    char bl_tb0[] = {0x51, 0x0f, 0xff};
    char bl_tb1[] = {0x53, 0x24};
    /*Tab A9 code for AX6739A-1497 by yuli at 20230626 start*/
    char bl_tb2[] = {0xFF, 0x10};
    char bl_tb3[] = {0xFB, 0x01};
    char bl_tb4[] = {0x53, 0x2C};

    cb(dsi, handle, bl_tb2, ARRAY_SIZE(bl_tb2));
    cb(dsi, handle, bl_tb3, ARRAY_SIZE(bl_tb3));

    /*Tab A9 code for AX6739A-1497 by yuli at 20230626 end*/

    level = level * 4095 / 255;

    /*Tab A9 code for AX6739A-1983 by hehaoran5 at 20230705 start*/
    level = mult_frac(level,BACKLIGHt_ELECTRICAL_NUMER0,BACKLIGHt_ELECTRICAL_DENOM);
    /*Tab A9 code for AX6739A-1983 by hehaoran5 at 20230705 end*/

    bl_tb0[1] = level >> 8;
    bl_tb0[2] = level & 0xFF;
    /*Tab A9 code for SR-AX6739A-01-734 by yuli at 20230606 start*/
    if (level != 0) {
        memcpy(gs_bl_tb0,bl_tb0,sizeof(bl_tb0));
    }
    /*Tab A9 code for SR-AX6739A-01-734 by yuli at 20230606 end*/
    pr_info("%s bl_tb0[1] = 0x%x,bl_tb0[2]= 0x%x\n", __func__, bl_tb0[1], bl_tb0[2]);

    if (!cb) {
        return -1;
        pr_err("cb error\n");
    }

    cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
    /*Tab A9 code for AX6739A-1808 by yuli at 20230705 start*/
    mdelay(20);
    if (level == 0) {
        cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
        pr_info("close diming\n");
    } else {
        cb(dsi, handle, bl_tb4, ARRAY_SIZE(bl_tb4));
    }
    /*Tab A9 code for AX6739A-1808 by yuli at 20230705 start*/
    return 0;
}

struct drm_display_mode *get_mode_by_id_hfp(struct drm_connector *connector,
    unsigned int mode)
{
    struct drm_display_mode *m = NULL;
    unsigned int i = 0;

    list_for_each_entry(m, &connector->modes, head) {
        if (i == mode)
            return m;
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
    struct device_node *dsi_node = NULL, *remote_node = NULL, *endpoint = NULL;
    struct lcm *ctx = NULL;
    struct device_node *backlight = NULL;

    int ret = 0;

    pr_info("%s+ lcm, nt36523d_lc_hsd\n", __func__);

    /*Tab A9 code for SR-AX6739A-01-768 by yuli at 20230718 start*/
    if (lcm_detect_panel("nt36523d_lc_hsd")) {
        pr_info("%s- lcm,nt36523d_lc_hsd\n", __func__);
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
            pr_info("device node name:%s\n", remote_node->name);
        }
    }
    if (remote_node != dev->of_node) {
        pr_err("%s+ skip probe due to not current lcm\n", __func__);
        return -ENODEV;
    }

    ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
    if (!ctx) {
        return -ENOMEM;
    }

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

        if (!ctx->backlight) {
            return -EPROBE_DEFER;
        }
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

    pr_info("%s+ it is official\n", __func__);
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
    if (ret < 0) {
        drm_panel_remove(&ctx->panel);
    }

#if defined(CONFIG_MTK_PANEL_EXT)
    mtk_panel_tch_handle_reg(&ctx->panel);
    ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
    if (ret < 0) {
        return ret;
    }
#endif

    pr_info("%s- lcm,nt36523d_lc_hsd\n", __func__);

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
        .name = "lcd_nt36523d_lc_hsd_video",
        .owner = THIS_MODULE,
        .of_match_table = lcm_of_match,
    },
};

module_mipi_dsi_driver(lcm_driver);

MODULE_DESCRIPTION("lcm nt36523d video lcd Panel Driver");
MODULE_LICENSE("GPL v2");
