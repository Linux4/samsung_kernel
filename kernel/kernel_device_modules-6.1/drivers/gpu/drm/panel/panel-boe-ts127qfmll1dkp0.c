// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#include "../../../misc/mediatek/gate_ic/gate_i2c.h"

static char bl_tb0[] = {0x51, 0xf, 0xff};

static int current_fps = 144;
#define SUPPORT_90Hz 0

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int bpc;

	/**
	 * @width_mm: width of the panel's active display area
	 * @height_mm: height of the panel's active display area
	 */
	struct {
		unsigned int width_mm;
		unsigned int height_mm;
	} size;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	const struct panel_init_cmd *init_cmds;
	unsigned int lanes;
};

struct boe {
	struct device *dev;
	struct mipi_dsi_device *dsi;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	struct regulator *reg;
	bool prepared;
	bool enabled;
	int error;
	unsigned int gate_ic;
	bool display_dual_swap;
};

#define boe_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		boe_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})
#define boe_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		boe_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

static inline struct boe *panel_to_boe(struct drm_panel *panel)
{
	return container_of(panel, struct boe, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int boe_dcs_read(struct boe *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void boe_panel_get_data(struct boe *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = boe_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void boe_dcs_write(struct boe *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);

	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

/* rc_buf_thresh will right shift 6bits (which means the values here will be divided by 64)
 * when setting to PPS8~PPS11 registers in mtk_dsc_config() function, so the original values
 * need left sihft 6bit (which means the original values are multiplied by 64), so that
 * PPS8~PPS11 registers can get right setting
 */
static unsigned int rc_buf_thresh[14] = {
//The original values VS values multiplied by 64
//14, 28,  42,	 56,   70,	 84,   98,	 105,  112,  119,  121,  123,  125,  126
896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616, 7744, 7872, 8000, 8064};
static unsigned int range_min_qp[15] = {0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 13};
static unsigned int range_max_qp[15] = {4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11, 12, 13, 13, 15};
static int range_bpg_ofs[15] = {2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12};

static void boe_panel_init(struct boe *ctx)
{
	pr_info("%s +\n", __func__);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x20);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0x08, 0x23);
	boe_dcs_write_seq_static(ctx, 0x10, 0x46);
	boe_dcs_write_seq_static(ctx, 0x44, 0x02);
	boe_dcs_write_seq_static(ctx, 0x65, 0xEE);
	boe_dcs_write_seq_static(ctx, 0x6D, 0xEE);
	boe_dcs_write_seq_static(ctx, 0x78, 0x93);
	boe_dcs_write_seq_static(ctx, 0x89, 0x73);
	boe_dcs_write_seq_static(ctx, 0x8A, 0x73);
	boe_dcs_write_seq_static(ctx, 0x8D, 0xAF);
	boe_dcs_write_seq_static(ctx, 0x95, 0xEB);
	boe_dcs_write_seq_static(ctx, 0x96, 0xEB);
	boe_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x29, 0x00, 0x5F, 0x00,
			0x87, 0x00, 0xA6, 0x00, 0xC1, 0x00, 0xD8, 0x00, 0xEC);
	boe_dcs_write_seq_static(ctx, 0xB1, 0x00, 0xFE, 0x01, 0x38, 0x01, 0x63, 0x01,
			0xA3, 0x01, 0xD3, 0x02, 0x1B, 0x02, 0x54, 0x02, 0x55);
	boe_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x8C, 0x02, 0xC9, 0x02, 0xF1, 0x03,
			0x24, 0x03, 0x45, 0x03, 0x71, 0x03, 0x7F, 0x03, 0x8E);
	boe_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x9E, 0x03, 0xAE, 0x03, 0xD0, 0x03,
			0xF0, 0x03, 0xFE, 0x03, 0xFF, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x00, 0x00, 0x29, 0x00, 0x5F, 0x00,
			0x87, 0x00, 0xA6, 0x00, 0xC1, 0x00, 0xD8, 0x00, 0xEC);
	boe_dcs_write_seq_static(ctx, 0xB5, 0x00, 0xFE, 0x01, 0x38, 0x01, 0x63, 0x01,
			0xA3, 0x01, 0xD3, 0x02, 0x1B, 0x02, 0x54, 0x02, 0x55);
	boe_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x8C, 0x02, 0xC9, 0x02, 0xF1, 0x03,
			0x24, 0x03, 0x45, 0x03, 0x71, 0x03, 0x7F, 0x03, 0x8E);
	boe_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x9E, 0x03, 0xAE, 0x03, 0xD0, 0x03,
			0xF0, 0x03, 0xFE, 0x03, 0xFF, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x00, 0x00, 0x29, 0x00, 0x5F, 0x00,
			0x87, 0x00, 0xA6, 0x00, 0xC1, 0x00, 0xD8, 0x00, 0xEC);
	boe_dcs_write_seq_static(ctx, 0xB9, 0x00, 0xFE, 0x01, 0x38, 0x01, 0x63, 0x01,
			0xA3, 0x01, 0xD3, 0x02, 0x1B, 0x02, 0x54, 0x02, 0x55);
	boe_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x8C, 0x02, 0xC9, 0x02, 0xF1, 0x03,
			0x24, 0x03, 0x45, 0x03, 0x71, 0x03, 0x7F, 0x03, 0x8E);
	boe_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x9E, 0x03, 0xAE, 0x03, 0xD0, 0x03,
			0xF0, 0x03, 0xFE, 0x03, 0xFF, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xC6, 0xD8);
	boe_dcs_write_seq_static(ctx, 0xC7, 0x54);
	boe_dcs_write_seq_static(ctx, 0xC8, 0x53);
	boe_dcs_write_seq_static(ctx, 0xC9, 0x42);
	boe_dcs_write_seq_static(ctx, 0xCA, 0x31);
	boe_dcs_write_seq_static(ctx, 0xCC, 0x22);
	boe_dcs_write_seq_static(ctx, 0xCD, 0x53);
	boe_dcs_write_seq_static(ctx, 0xCE, 0x54);
	boe_dcs_write_seq_static(ctx, 0xCF, 0xA6);
	boe_dcs_write_seq_static(ctx, 0xD0, 0xA4);
	boe_dcs_write_seq_static(ctx, 0xD1, 0xF8);
	boe_dcs_write_seq_static(ctx, 0xD2, 0xD8);
	boe_dcs_write_seq_static(ctx, 0xD3, 0x54);
	boe_dcs_write_seq_static(ctx, 0xD4, 0x53);
	boe_dcs_write_seq_static(ctx, 0xD5, 0x42);
	boe_dcs_write_seq_static(ctx, 0xD6, 0x31);
	boe_dcs_write_seq_static(ctx, 0xD8, 0x22);
	boe_dcs_write_seq_static(ctx, 0xD9, 0x53);
	boe_dcs_write_seq_static(ctx, 0xDA, 0x54);
	boe_dcs_write_seq_static(ctx, 0xDB, 0xA6);
	boe_dcs_write_seq_static(ctx, 0xDC, 0xA4);
	boe_dcs_write_seq_static(ctx, 0xDD, 0xF8);
	boe_dcs_write_seq_static(ctx, 0xDE, 0xD8);
	boe_dcs_write_seq_static(ctx, 0xDF, 0x54);
	boe_dcs_write_seq_static(ctx, 0xE0, 0x53);
	boe_dcs_write_seq_static(ctx, 0xE1, 0x42);
	boe_dcs_write_seq_static(ctx, 0xE2, 0x31);
	boe_dcs_write_seq_static(ctx, 0xE4, 0x22);
	boe_dcs_write_seq_static(ctx, 0xE5, 0x53);
	boe_dcs_write_seq_static(ctx, 0xE6, 0x54);
	boe_dcs_write_seq_static(ctx, 0xE7, 0xA6);
	boe_dcs_write_seq_static(ctx, 0xE8, 0xA4);
	boe_dcs_write_seq_static(ctx, 0xE9, 0xF8);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x21);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x00, 0x00, 0x29, 0x00, 0x5F, 0x00,
			0x87, 0x00, 0xA6, 0x00, 0xC1, 0x00, 0xD8, 0x00, 0xEC);
	boe_dcs_write_seq_static(ctx, 0xB1, 0x00, 0xFE, 0x01, 0x38, 0x01, 0x63, 0x01,
			0xA3, 0x01, 0xD3, 0x02, 0x1B, 0x02, 0x54, 0x02, 0x55);
	boe_dcs_write_seq_static(ctx, 0xB2, 0x02, 0x8C, 0x02, 0xC9, 0x02, 0xF1, 0x03,
			0x24, 0x03, 0x45, 0x03, 0x71, 0x03, 0x7F, 0x03, 0x8E);
	boe_dcs_write_seq_static(ctx, 0xB3, 0x03, 0x9E, 0x03, 0xAE, 0x03, 0xD0, 0x03,
			0xF0, 0x03, 0xFE, 0x03, 0xFF, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xB4, 0x00, 0x00, 0x00, 0x29, 0x00, 0x5F, 0x00,
			0x87, 0x00, 0xA6, 0x00, 0xC1, 0x00, 0xD8, 0x00, 0xEC);
	boe_dcs_write_seq_static(ctx, 0xB5, 0x00, 0xFE, 0x01, 0x38, 0x01, 0x63, 0x01,
			0xA3, 0x01, 0xD3, 0x02, 0x1B, 0x02, 0x54, 0x02, 0x55);
	boe_dcs_write_seq_static(ctx, 0xB6, 0x02, 0x8C, 0x02, 0xC9, 0x02, 0xF1, 0x03,
			0x24, 0x03, 0x45, 0x03, 0x71, 0x03, 0x7F, 0x03, 0x8E);
	boe_dcs_write_seq_static(ctx, 0xB7, 0x03, 0x9E, 0x03, 0xAE, 0x03, 0xD0, 0x03,
			0xF0, 0x03, 0xFE, 0x03, 0xFF, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xB8, 0x00, 0x00, 0x00, 0x29, 0x00, 0x5F, 0x00,
			0x87, 0x00, 0xA6, 0x00, 0xC1, 0x00, 0xD8, 0x00, 0xEC);
	boe_dcs_write_seq_static(ctx, 0xB9, 0x00, 0xFE, 0x01, 0x38, 0x01, 0x63, 0x01,
			0xA3, 0x01, 0xD3, 0x02, 0x1B, 0x02, 0x54, 0x02, 0x55);
	boe_dcs_write_seq_static(ctx, 0xBA, 0x02, 0x8C, 0x02, 0xC9, 0x02, 0xF1, 0x03,
			0x24, 0x03, 0x45, 0x03, 0x71, 0x03, 0x7F, 0x03, 0x8E);
	boe_dcs_write_seq_static(ctx, 0xBB, 0x03, 0x9E, 0x03, 0xAE, 0x03, 0xD0, 0x03,
			0xF0, 0x03, 0xFE, 0x03, 0xFF, 0x00, 0x00);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x23);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0x75, 0x03, 0x00);
	boe_dcs_write_seq_static(ctx, 0x76, 0x07, 0x02);
	boe_dcs_write_seq_static(ctx, 0x77, 0x03, 0x05);
	boe_dcs_write_seq_static(ctx, 0x78, 0x01, 0x02);
	boe_dcs_write_seq_static(ctx, 0x7A, 0xCD, 0x63);
	boe_dcs_write_seq_static(ctx, 0x7B, 0xA0, 0x70);
	boe_dcs_write_seq_static(ctx, 0x7C, 0x2D, 0x32);
	boe_dcs_write_seq_static(ctx, 0x7E, 0x00, 0x10);
	boe_dcs_write_seq_static(ctx, 0xBA, 0x3A, 0x7A);
	boe_dcs_write_seq_static(ctx, 0xBB, 0x39, 0x7C);
	boe_dcs_write_seq_static(ctx, 0xBC, 0x04, 0x00, 0x3C);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x24);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0x00, 0x27, 0x0D, 0x0C, 0x0F, 0x0E, 0x28, 0x28,
			0x26, 0x26, 0x3A, 0x04, 0x05, 0x23, 0x03, 0x22, 0x29);
	boe_dcs_write_seq_static(ctx, 0x01, 0x29, 0x08, 0x30, 0x2E, 0x2C, 0x30, 0x2E,
			0x2C);
	boe_dcs_write_seq_static(ctx, 0x02, 0x27, 0x0D, 0x0C, 0x0F, 0x0E, 0x28, 0x28,
			0x26, 0x26, 0x3A, 0x04, 0x05, 0x25, 0x03, 0x24, 0x29);
	boe_dcs_write_seq_static(ctx, 0x03, 0x29, 0x08, 0x30, 0x2E, 0x2C, 0x30, 0x2E,
			0x2C);
	boe_dcs_write_seq_static(ctx, 0x17, 0x41, 0x25, 0x63, 0xA7, 0x8B, 0xC9, 0x41,
			0x25, 0x63, 0xA7, 0x8B, 0xC9);
	boe_dcs_write_seq_static(ctx, 0x1C, 0x80);
	boe_dcs_write_seq_static(ctx, 0x2F, 0x02);
	boe_dcs_write_seq_static(ctx, 0x30, 0x01);
	boe_dcs_write_seq_static(ctx, 0x31, 0x02);
	boe_dcs_write_seq_static(ctx, 0x32, 0x03);
	boe_dcs_write_seq_static(ctx, 0x33, 0x04);
	boe_dcs_write_seq_static(ctx, 0x34, 0x05);
	boe_dcs_write_seq_static(ctx, 0x35, 0x06);
	boe_dcs_write_seq_static(ctx, 0x36, 0x07);
	boe_dcs_write_seq_static(ctx, 0x37, 0x22);
	boe_dcs_write_seq_static(ctx, 0x3A, 0x05);
	boe_dcs_write_seq_static(ctx, 0x3B, 0x61);
	boe_dcs_write_seq_static(ctx, 0x3D, 0x02);
	boe_dcs_write_seq_static(ctx, 0x3F, 0x0B);
	boe_dcs_write_seq_static(ctx, 0x47, 0x24);
	boe_dcs_write_seq_static(ctx, 0x4B, 0x61);
	boe_dcs_write_seq_static(ctx, 0x4C, 0x01);
	boe_dcs_write_seq_static(ctx, 0x55, 0x0A, 0x0A);
	boe_dcs_write_seq_static(ctx, 0x56, 0x04);
	boe_dcs_write_seq_static(ctx, 0x58, 0x10);
	boe_dcs_write_seq_static(ctx, 0x59, 0x10);
	boe_dcs_write_seq_static(ctx, 0x5A, 0x05);
	boe_dcs_write_seq_static(ctx, 0x5B, 0x63);
	boe_dcs_write_seq_static(ctx, 0x60, 0x71, 0x70);
	boe_dcs_write_seq_static(ctx, 0x61, 0x30);
	boe_dcs_write_seq_static(ctx, 0x72, 0x80);
	boe_dcs_write_seq_static(ctx, 0x92, 0x66, 0x01, 0x24);
	boe_dcs_write_seq_static(ctx, 0x93, 0x1A, 0x00);
	boe_dcs_write_seq_static(ctx, 0x94, 0xB8, 0x00);
	boe_dcs_write_seq_static(ctx, 0x95, 0x09);
	boe_dcs_write_seq_static(ctx, 0x96, 0x80, 0xBA, 0x00);
	boe_dcs_write_seq_static(ctx, 0x9A, 0x0B);
	boe_dcs_write_seq_static(ctx, 0xA5, 0x00);
	boe_dcs_write_seq_static(ctx, 0xAA, 0x92, 0x92, 0x24);
	boe_dcs_write_seq_static(ctx, 0xAB, 0x22);
	boe_dcs_write_seq_static(ctx, 0xC2, 0xC4, 0x00, 0x01);
	boe_dcs_write_seq_static(ctx, 0xC4, 0x2A);
	boe_dcs_write_seq_static(ctx, 0xC8, 0x03);
	boe_dcs_write_seq_static(ctx, 0xC9, 0x08);
	boe_dcs_write_seq_static(ctx, 0xCA, 0x03);
	boe_dcs_write_seq_static(ctx, 0xD4, 0x03);
	boe_dcs_write_seq_static(ctx, 0xD6, 0x46);
	boe_dcs_write_seq_static(ctx, 0xD7, 0x35);
	boe_dcs_write_seq_static(ctx, 0xD8, 0x25);
	boe_dcs_write_seq_static(ctx, 0xD9, 0x12);
	boe_dcs_write_seq_static(ctx, 0xDB, 0x08);
	boe_dcs_write_seq_static(ctx, 0xDD, 0x45);
	boe_dcs_write_seq_static(ctx, 0xDE, 0x02);
	boe_dcs_write_seq_static(ctx, 0xDF, 0x08);
	boe_dcs_write_seq_static(ctx, 0xE1, 0x08);
	boe_dcs_write_seq_static(ctx, 0xE3, 0x08);
	boe_dcs_write_seq_static(ctx, 0xE5, 0x08);
	boe_dcs_write_seq_static(ctx, 0xE9, 0x08);
	boe_dcs_write_seq_static(ctx, 0xEB, 0x08);
	boe_dcs_write_seq_static(ctx, 0xEF, 0x08);
	boe_dcs_write_seq_static(ctx, 0xF1, 0x23);
	boe_dcs_write_seq_static(ctx, 0xF2, 0x23);
	boe_dcs_write_seq_static(ctx, 0xF3, 0x23);
	boe_dcs_write_seq_static(ctx, 0xF4, 0x22);
	boe_dcs_write_seq_static(ctx, 0xF5, 0x23);
	boe_dcs_write_seq_static(ctx, 0xF6, 0x23);
	boe_dcs_write_seq_static(ctx, 0xF7, 0x24);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x25);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0x05, 0x00);
	boe_dcs_write_seq_static(ctx, 0x14, 0x49);
	boe_dcs_write_seq_static(ctx, 0x15, 0x01);
	boe_dcs_write_seq_static(ctx, 0x16, 0xE7);
	boe_dcs_write_seq_static(ctx, 0x1F, 0x05);
	boe_dcs_write_seq_static(ctx, 0x20, 0x63);
	boe_dcs_write_seq_static(ctx, 0x23, 0x09);
	boe_dcs_write_seq_static(ctx, 0x24, 0x17);
	boe_dcs_write_seq_static(ctx, 0x26, 0x05);
	boe_dcs_write_seq_static(ctx, 0x27, 0x63);
	boe_dcs_write_seq_static(ctx, 0x2A, 0x09);
	boe_dcs_write_seq_static(ctx, 0x2B, 0x17);
	boe_dcs_write_seq_static(ctx, 0x33, 0x05);
	boe_dcs_write_seq_static(ctx, 0x34, 0x63);
	boe_dcs_write_seq_static(ctx, 0x37, 0x09);
	boe_dcs_write_seq_static(ctx, 0x38, 0x17);
	boe_dcs_write_seq_static(ctx, 0x39, 0x09);
	boe_dcs_write_seq_static(ctx, 0x3B, 0x00);
	boe_dcs_write_seq_static(ctx, 0x3F, 0x20);
	boe_dcs_write_seq_static(ctx, 0x40, 0x00);
	boe_dcs_write_seq_static(ctx, 0x42, 0x04);
	boe_dcs_write_seq_static(ctx, 0x44, 0x05);
	boe_dcs_write_seq_static(ctx, 0x45, 0x61);
	boe_dcs_write_seq_static(ctx, 0x47, 0x61);
	boe_dcs_write_seq_static(ctx, 0x48, 0x05);
	boe_dcs_write_seq_static(ctx, 0x49, 0x63);
	boe_dcs_write_seq_static(ctx, 0x4C, 0x09);
	boe_dcs_write_seq_static(ctx, 0x4D, 0x17);
	boe_dcs_write_seq_static(ctx, 0x4E, 0x09);
	boe_dcs_write_seq_static(ctx, 0x50, 0x05);
	boe_dcs_write_seq_static(ctx, 0x51, 0x63);
	boe_dcs_write_seq_static(ctx, 0x54, 0x09);
	boe_dcs_write_seq_static(ctx, 0x55, 0x17);
	boe_dcs_write_seq_static(ctx, 0x56, 0x09);
	boe_dcs_write_seq_static(ctx, 0x5B, 0xA0);
	boe_dcs_write_seq_static(ctx, 0x5D, 0x05);
	boe_dcs_write_seq_static(ctx, 0x5E, 0x61);
	boe_dcs_write_seq_static(ctx, 0x60, 0x61);
	boe_dcs_write_seq_static(ctx, 0x61, 0x05);
	boe_dcs_write_seq_static(ctx, 0x62, 0x63);
	boe_dcs_write_seq_static(ctx, 0x65, 0x09);
	boe_dcs_write_seq_static(ctx, 0x66, 0x17);
	boe_dcs_write_seq_static(ctx, 0x67, 0x09);
	boe_dcs_write_seq_static(ctx, 0x6A, 0x01);
	boe_dcs_write_seq_static(ctx, 0x6B, 0x44);
	boe_dcs_write_seq_static(ctx, 0x6C, 0x0D);
	boe_dcs_write_seq_static(ctx, 0x6D, 0x0D);
	boe_dcs_write_seq_static(ctx, 0x6E, 0x1A);
	boe_dcs_write_seq_static(ctx, 0x6F, 0x1A);
	boe_dcs_write_seq_static(ctx, 0x79, 0x77);
	boe_dcs_write_seq_static(ctx, 0x7B, 0x00);
	boe_dcs_write_seq_static(ctx, 0x7D, 0x77);
	boe_dcs_write_seq_static(ctx, 0x7F, 0x00);
	boe_dcs_write_seq_static(ctx, 0x81, 0x77);
	boe_dcs_write_seq_static(ctx, 0x83, 0x00);
	boe_dcs_write_seq_static(ctx, 0xC6, 0x10);
	boe_dcs_write_seq_static(ctx, 0xDC, 0x9B);
	boe_dcs_write_seq_static(ctx, 0xDD, 0x02);
	boe_dcs_write_seq_static(ctx, 0xDE, 0x2C);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x26);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0x04, 0x75);
	boe_dcs_write_seq_static(ctx, 0x0A, 0x05);
	boe_dcs_write_seq_static(ctx, 0x0C, 0x08);
	boe_dcs_write_seq_static(ctx, 0x0D, 0x00);
	boe_dcs_write_seq_static(ctx, 0x0F, 0x03);
	boe_dcs_write_seq_static(ctx, 0x13, 0xC8);
	boe_dcs_write_seq_static(ctx, 0x14, 0xCD);
	boe_dcs_write_seq_static(ctx, 0x16, 0x10);
	boe_dcs_write_seq_static(ctx, 0x19, 0x1C, 0x1D, 0x16, 0x1D);
	boe_dcs_write_seq_static(ctx, 0x1A, 0xDB, 0xB1, 0xA4, 0xB1);
	boe_dcs_write_seq_static(ctx, 0x1B, 0x1B, 0x1C, 0x1C, 0x1C);
	boe_dcs_write_seq_static(ctx, 0x1C, 0xFB, 0xD1, 0xD1, 0xD1);
	boe_dcs_write_seq_static(ctx, 0x1D, 0x00);
	boe_dcs_write_seq_static(ctx, 0x1E, 0x88);
	boe_dcs_write_seq_static(ctx, 0x1F, 0x66);
	boe_dcs_write_seq_static(ctx, 0x24, 0x01);
	boe_dcs_write_seq_static(ctx, 0x25, 0x4A);
	boe_dcs_write_seq_static(ctx, 0x2A, 0x1C, 0x1D, 0x16, 0x1D);
	boe_dcs_write_seq_static(ctx, 0x2B, 0xD3, 0xA9, 0x9C, 0xA9);
	boe_dcs_write_seq_static(ctx, 0x2D, 0x00, 0x00, 0x00, 0x0E, 0x00, 0xC0, 0x0F,
			0x04, 0x00);
	boe_dcs_write_seq_static(ctx, 0x2F, 0x0B);
	boe_dcs_write_seq_static(ctx, 0x30, 0x66);
	boe_dcs_write_seq_static(ctx, 0x32, 0x66);
	boe_dcs_write_seq_static(ctx, 0x34, 0x11);
	boe_dcs_write_seq_static(ctx, 0x35, 0x11);
	boe_dcs_write_seq_static(ctx, 0x36, 0x91);
	boe_dcs_write_seq_static(ctx, 0x37, 0x78);
	boe_dcs_write_seq_static(ctx, 0x38, 0x06);
	boe_dcs_write_seq_static(ctx, 0x3A, 0x66);
	boe_dcs_write_seq_static(ctx, 0x3D, 0x00, 0x80, 0x28, 0x20);
	boe_dcs_write_seq_static(ctx, 0x3F, 0x00);
	boe_dcs_write_seq_static(ctx, 0x40, 0x64);
	boe_dcs_write_seq_static(ctx, 0x41, 0x86);
	boe_dcs_write_seq_static(ctx, 0x42, 0x64);
	boe_dcs_write_seq_static(ctx, 0x43, 0x01);
	boe_dcs_write_seq_static(ctx, 0x44, 0x4C);
	boe_dcs_write_seq_static(ctx, 0x45, 0x0B);
	boe_dcs_write_seq_static(ctx, 0x46, 0x64);
	boe_dcs_write_seq_static(ctx, 0x48, 0x64);
	boe_dcs_write_seq_static(ctx, 0x4A, 0x64);
	boe_dcs_write_seq_static(ctx, 0x4E, 0x61);
	boe_dcs_write_seq_static(ctx, 0x4F, 0x02);
	boe_dcs_write_seq_static(ctx, 0x50, 0x61);
	boe_dcs_write_seq_static(ctx, 0x52, 0x63);
	boe_dcs_write_seq_static(ctx, 0x53, 0x02);
	boe_dcs_write_seq_static(ctx, 0x54, 0x72);
	boe_dcs_write_seq_static(ctx, 0x58, 0x63);
	boe_dcs_write_seq_static(ctx, 0x5C, 0x63);
	boe_dcs_write_seq_static(ctx, 0x60, 0x05);
	boe_dcs_write_seq_static(ctx, 0x61, 0x63);
	boe_dcs_write_seq_static(ctx, 0x64, 0x05);
	boe_dcs_write_seq_static(ctx, 0x65, 0x63);
	boe_dcs_write_seq_static(ctx, 0x69, 0x05);
	boe_dcs_write_seq_static(ctx, 0x6A, 0x63);
	boe_dcs_write_seq_static(ctx, 0x6F, 0x61);
	boe_dcs_write_seq_static(ctx, 0x70, 0x02);
	boe_dcs_write_seq_static(ctx, 0x71, 0x61);
	boe_dcs_write_seq_static(ctx, 0x73, 0x62);
	boe_dcs_write_seq_static(ctx, 0x74, 0x02);
	boe_dcs_write_seq_static(ctx, 0x75, 0x72);
	boe_dcs_write_seq_static(ctx, 0x7C, 0x61);
	boe_dcs_write_seq_static(ctx, 0x7E, 0x02);
	boe_dcs_write_seq_static(ctx, 0x7F, 0x61);
	boe_dcs_write_seq_static(ctx, 0x80, 0xFF);
	boe_dcs_write_seq_static(ctx, 0x81, 0xFF);
	boe_dcs_write_seq_static(ctx, 0x82, 0x00, 0x4C, 0x4C, 0x4C);
	boe_dcs_write_seq_static(ctx, 0x84, 0x17, 0x17, 0x17);
	boe_dcs_write_seq_static(ctx, 0x8B, 0x8C);
	boe_dcs_write_seq_static(ctx, 0x8C, 0x04);
	boe_dcs_write_seq_static(ctx, 0x8D, 0x00);
	boe_dcs_write_seq_static(ctx, 0x8F, 0x00);
	boe_dcs_write_seq_static(ctx, 0x92, 0x51);
	boe_dcs_write_seq_static(ctx, 0x93, 0x4A);
	boe_dcs_write_seq_static(ctx, 0x94, 0xC2);
	boe_dcs_write_seq_static(ctx, 0x96, 0x11);
	boe_dcs_write_seq_static(ctx, 0x97, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x99, 0x15, 0x1E, 0x04, 0x18);
	boe_dcs_write_seq_static(ctx, 0x9A, 0x30, 0x94, 0x3B, 0x29);
	boe_dcs_write_seq_static(ctx, 0x9B, 0x14, 0x1D, 0x07, 0x17);
	boe_dcs_write_seq_static(ctx, 0x9C, 0x52, 0xB4, 0x06, 0x4A);
	boe_dcs_write_seq_static(ctx, 0x9D, 0x15, 0x1E, 0x04, 0x18);
	boe_dcs_write_seq_static(ctx, 0x9E, 0x2A, 0x8C, 0x00, 0x22);
	boe_dcs_write_seq_static(ctx, 0xA7, 0x05);
	boe_dcs_write_seq_static(ctx, 0xC9, 0x00);
	boe_dcs_write_seq_static(ctx, 0xCD, 0x4A, 0x69);
	boe_dcs_write_seq_static(ctx, 0xCE, 0x48, 0x6B);
	boe_dcs_write_seq_static(ctx, 0xCF, 0x01, 0x00, 0x32);
	boe_dcs_write_seq_static(ctx, 0xD0, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xD1, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xD2, 0x2D);
	boe_dcs_write_seq_static(ctx, 0xD6, 0x01);
	boe_dcs_write_seq_static(ctx, 0xD7, 0x02);
	boe_dcs_write_seq_static(ctx, 0xD8, 0x03);
	boe_dcs_write_seq_static(ctx, 0xD9, 0x04);
	boe_dcs_write_seq_static(ctx, 0xDA, 0x05);
	boe_dcs_write_seq_static(ctx, 0xDB, 0x06);
	boe_dcs_write_seq_static(ctx, 0xDC, 0x07);
	boe_dcs_write_seq_static(ctx, 0xDD, 0x08);
	boe_dcs_write_seq_static(ctx, 0xDE, 0x09);
	boe_dcs_write_seq_static(ctx, 0xDF, 0x0A);
	boe_dcs_write_seq_static(ctx, 0xE0, 0x0B);
	boe_dcs_write_seq_static(ctx, 0xE1, 0x0C);
	boe_dcs_write_seq_static(ctx, 0xE2, 0x0D);
	boe_dcs_write_seq_static(ctx, 0xE3, 0x0E);
	boe_dcs_write_seq_static(ctx, 0xE4, 0x0F);
	boe_dcs_write_seq_static(ctx, 0xE5, 0x10);
	boe_dcs_write_seq_static(ctx, 0xE6, 0x11);
	boe_dcs_write_seq_static(ctx, 0xE7, 0x12);
	boe_dcs_write_seq_static(ctx, 0xE8, 0x13);
	boe_dcs_write_seq_static(ctx, 0xE9, 0x14);
	boe_dcs_write_seq_static(ctx, 0xEA, 0x15);
	boe_dcs_write_seq_static(ctx, 0xEB, 0x16);
	boe_dcs_write_seq_static(ctx, 0xEC, 0x17);
	boe_dcs_write_seq_static(ctx, 0xED, 0x18);
	boe_dcs_write_seq_static(ctx, 0xEE, 0x19);
	boe_dcs_write_seq_static(ctx, 0xEF, 0x1A);
	boe_dcs_write_seq_static(ctx, 0xF0, 0x1B);
	boe_dcs_write_seq_static(ctx, 0xF1, 0x1C);
	boe_dcs_write_seq_static(ctx, 0xF2, 0x1D);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x27);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0x13, 0x06);
	boe_dcs_write_seq_static(ctx, 0x50, 0x00);
	boe_dcs_write_seq_static(ctx, 0x58, 0x80);
	boe_dcs_write_seq_static(ctx, 0x59, 0x00);
	boe_dcs_write_seq_static(ctx, 0x5A, 0x00);
	boe_dcs_write_seq_static(ctx, 0x5B, 0xB5);
	boe_dcs_write_seq_static(ctx, 0x5C, 0x00);
	boe_dcs_write_seq_static(ctx, 0x5D, 0x00);
	boe_dcs_write_seq_static(ctx, 0x5E, 0x00);
	boe_dcs_write_seq_static(ctx, 0x5F, 0x00);
	boe_dcs_write_seq_static(ctx, 0x60, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x61, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x62, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x63, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x64, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x65, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x66, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x67, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x68, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x69, 0x80);
	boe_dcs_write_seq_static(ctx, 0x76, 0x80);
	boe_dcs_write_seq_static(ctx, 0x77, 0x2C, 0x00);
	boe_dcs_write_seq_static(ctx, 0x78, 0x80);
	boe_dcs_write_seq_static(ctx, 0x79, 0xE9);
	boe_dcs_write_seq_static(ctx, 0x7A, 0x00);
	boe_dcs_write_seq_static(ctx, 0x7B, 0xB6);
	boe_dcs_write_seq_static(ctx, 0x7D, 0x04);
	boe_dcs_write_seq_static(ctx, 0x7E, 0x53);
	boe_dcs_write_seq_static(ctx, 0x7F, 0x04);
	boe_dcs_write_seq_static(ctx, 0x80, 0x1D, 0x08);
	boe_dcs_write_seq_static(ctx, 0x81, 0x2D, 0x00);
	boe_dcs_write_seq_static(ctx, 0x82, 0x7B, 0x40);
	boe_dcs_write_seq_static(ctx, 0x83, 0x13, 0x00);
	boe_dcs_write_seq_static(ctx, 0x84, 0x35, 0x00);
	boe_dcs_write_seq_static(ctx, 0x85, 0x28, 0x00);
	boe_dcs_write_seq_static(ctx, 0x86, 0x7B, 0x40);
	boe_dcs_write_seq_static(ctx, 0x87, 0x13, 0x00);
	boe_dcs_write_seq_static(ctx, 0x88, 0x35, 0x00);
	boe_dcs_write_seq_static(ctx, 0x89, 0x80);
	boe_dcs_write_seq_static(ctx, 0x96, 0x80);
	boe_dcs_write_seq_static(ctx, 0x97, 0x2C, 0x00);
	boe_dcs_write_seq_static(ctx, 0xC0, 0x00);
	boe_dcs_write_seq_static(ctx, 0xC1, 0x00);
	boe_dcs_write_seq_static(ctx, 0xD1, 0x04);
	boe_dcs_write_seq_static(ctx, 0xD2, 0x3C);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x2A);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0x00, 0x23);
	boe_dcs_write_seq_static(ctx, 0x01, 0x08);
	boe_dcs_write_seq_static(ctx, 0x02, 0x88);
	boe_dcs_write_seq_static(ctx, 0x03, 0x08);
	boe_dcs_write_seq_static(ctx, 0x05, 0x08);
	boe_dcs_write_seq_static(ctx, 0x07, 0x7F);
	boe_dcs_write_seq_static(ctx, 0x08, 0x08);
	boe_dcs_write_seq_static(ctx, 0x0A, 0x08);
	boe_dcs_write_seq_static(ctx, 0x0C, 0x08);
	boe_dcs_write_seq_static(ctx, 0x0E, 0x23);
	boe_dcs_write_seq_static(ctx, 0x0F, 0x23);
	boe_dcs_write_seq_static(ctx, 0x10, 0x23);
	boe_dcs_write_seq_static(ctx, 0x11, 0x23);
	boe_dcs_write_seq_static(ctx, 0x12, 0x23);
	boe_dcs_write_seq_static(ctx, 0x13, 0x23);
	boe_dcs_write_seq_static(ctx, 0x14, 0x09);
	boe_dcs_write_seq_static(ctx, 0x15, 0x09);
	boe_dcs_write_seq_static(ctx, 0x16, 0x17);
	boe_dcs_write_seq_static(ctx, 0x17, 0x09);
	boe_dcs_write_seq_static(ctx, 0x18, 0x17);
	boe_dcs_write_seq_static(ctx, 0x19, 0x09);
	boe_dcs_write_seq_static(ctx, 0x1A, 0x17);
	boe_dcs_write_seq_static(ctx, 0x1B, 0x09);
	boe_dcs_write_seq_static(ctx, 0x1C, 0x17);
	boe_dcs_write_seq_static(ctx, 0x1D, 0x09);
	boe_dcs_write_seq_static(ctx, 0x1E, 0x09);
	boe_dcs_write_seq_static(ctx, 0x1F, 0x09);
	boe_dcs_write_seq_static(ctx, 0x25, 0x15);
	boe_dcs_write_seq_static(ctx, 0x27, 0x5F);
	boe_dcs_write_seq_static(ctx, 0x28, 0x78);
	boe_dcs_write_seq_static(ctx, 0x2F, 0x44, 0x21, 0x02);
	boe_dcs_write_seq_static(ctx, 0x30, 0x02);
	boe_dcs_write_seq_static(ctx, 0x32, 0x86);
	boe_dcs_write_seq_static(ctx, 0x33, 0xAB);
	boe_dcs_write_seq_static(ctx, 0x35, 0x00);
	boe_dcs_write_seq_static(ctx, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0x64, 0x96);
	boe_dcs_write_seq_static(ctx, 0x67, 0x9E);
	boe_dcs_write_seq_static(ctx, 0x68, 0x00);
	boe_dcs_write_seq_static(ctx, 0x69, 0x00);
	boe_dcs_write_seq_static(ctx, 0x6A, 0x9E);
	boe_dcs_write_seq_static(ctx, 0x6B, 0x00);
	boe_dcs_write_seq_static(ctx, 0x6C, 0x00);
	boe_dcs_write_seq_static(ctx, 0x79, 0x96);
	boe_dcs_write_seq_static(ctx, 0x7C, 0x96);
	boe_dcs_write_seq_static(ctx, 0x7F, 0x96);
	boe_dcs_write_seq_static(ctx, 0x82, 0x96);
	boe_dcs_write_seq_static(ctx, 0x85, 0x96);
	boe_dcs_write_seq_static(ctx, 0x88, 0x9E);
	boe_dcs_write_seq_static(ctx, 0x89, 0x00);
	boe_dcs_write_seq_static(ctx, 0x8A, 0x00);
	boe_dcs_write_seq_static(ctx, 0x8B, 0x16);
	boe_dcs_write_seq_static(ctx, 0x8C, 0x00);
	boe_dcs_write_seq_static(ctx, 0x8D, 0x00);
	boe_dcs_write_seq_static(ctx, 0x8E, 0x16);
	boe_dcs_write_seq_static(ctx, 0x8F, 0x00);
	boe_dcs_write_seq_static(ctx, 0x90, 0x00);
	boe_dcs_write_seq_static(ctx, 0x92, 0x00);
	boe_dcs_write_seq_static(ctx, 0x93, 0x00);
	boe_dcs_write_seq_static(ctx, 0x94, 0x06);
	boe_dcs_write_seq_static(ctx, 0x99, 0x91);
	boe_dcs_write_seq_static(ctx, 0x9A, 0x0A);
	boe_dcs_write_seq_static(ctx, 0xA2, 0x15);
	boe_dcs_write_seq_static(ctx, 0xA3, 0x50);
	boe_dcs_write_seq_static(ctx, 0xA4, 0x15);
	boe_dcs_write_seq_static(ctx, 0xA5, 0x41);
	boe_dcs_write_seq_static(ctx, 0xA6, 0x10);
	boe_dcs_write_seq_static(ctx, 0xA9, 0x21);
	boe_dcs_write_seq_static(ctx, 0xB1, 0x00, 0x01);
	boe_dcs_write_seq_static(ctx, 0xB3, 0x16, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xB7, 0xFF);
	boe_dcs_write_seq_static(ctx, 0xB8, 0x40);
	boe_dcs_write_seq_static(ctx, 0xB9, 0x98, 0x00, 0x6F, 0x00, 0xCB, 0xB7, 0x14,
			0x5F, 0x41);
	boe_dcs_write_seq_static(ctx, 0xBA, 0xAA, 0xAA, 0xA5, 0xA5, 0xE1, 0xE1, 0xFF,
			0xFF);
	boe_dcs_write_seq_static(ctx, 0xBB, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0xAA,
			0x55, 0x38, 0xC7, 0x38, 0xC7, 0xFF, 0xFF, 0xFF, 0xFF);
	boe_dcs_write_seq_static(ctx, 0xBC, 0x66, 0x06);
	boe_dcs_write_seq_static(ctx, 0xC4, 0x12);
	boe_dcs_write_seq_static(ctx, 0xC5, 0x09);
	boe_dcs_write_seq_static(ctx, 0xC6, 0x17);
	boe_dcs_write_seq_static(ctx, 0xC8, 0x09);
	boe_dcs_write_seq_static(ctx, 0xCA, 0x01);
	boe_dcs_write_seq_static(ctx, 0xCB, 0x10, 0x00, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xCC, 0xF5, 0xAA, 0x5F, 0x55, 0x00, 0x00);
	boe_dcs_write_seq_static(ctx, 0xD0, 0x04);
	boe_dcs_write_seq_static(ctx, 0xD1, 0x01);
	boe_dcs_write_seq_static(ctx, 0xE8, 0x0A);
	boe_dcs_write_seq_static(ctx, 0xF4, 0x91);
	boe_dcs_write_seq_static(ctx, 0xF5, 0x0A);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x10);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0x3B, 0x03, 0xB8, 0x1A, 0x0A, 0x0A, 0x00);
	boe_dcs_write_seq_static(ctx, 0x90, 0x03);
	boe_dcs_write_seq_static(ctx, 0x91, 0x89, 0x28, 0x00, 0x14, 0xD2, 0x00, 0x00,
			0x00, 0x02, 0x19, 0x00, 0x0A, 0x05, 0x7A, 0x03, 0xAC);
	boe_dcs_write_seq_static(ctx, 0x92, 0x10, 0xD0);
	boe_dcs_write_seq_static(ctx, 0x9D, 0x01);
	boe_dcs_write_seq_static(ctx, 0xB2, 0x80);
	boe_dcs_write_seq_static(ctx, 0xB3, 0x00);

	msleep(20);
	boe_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	boe_dcs_write_seq_static(ctx, 0x29);

	pr_info("%s -\n", __func__);
}

static int boe_disable(struct drm_panel *panel)
{
	struct boe *ctx = panel_to_boe(panel);

	pr_info("%s+++\n", __func__);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	pr_info("%s---\n", __func__);

	return 0;
}

static int boe_unprepare(struct drm_panel *panel)
{
	struct boe *ctx = panel_to_boe(panel);

	pr_info("%s+++\n", __func__);

	if (!ctx->prepared)
		return 0;

	boe_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(120);
	boe_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(20);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5 * 1000, 5 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	if (ctx->gate_ic == 0) {
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		usleep_range(2000, 2001);

		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	} else if (ctx->gate_ic == 4831) {
		pr_info("%s, Using gate-ic 4831\n", __func__);
		_gate_ic_i2c_panel_bias_enable(0);
		_gate_ic_Power_off();
	}

	ctx->error = 0;
	ctx->prepared = false;

	pr_info("%s---\n", __func__);

	return 0;
}
static int boe_prepare(struct drm_panel *panel)
{
	struct boe *ctx = panel_to_boe(panel);
	int ret;

	pr_info("%s+++\n", __func__);

	if (ctx->prepared)
		return 0;

	if (ctx->gate_ic == 0) {
		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		usleep_range(2000, 2001);
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
	} else if (ctx->gate_ic == 4831) {
		pr_info("%s, Using gate-ic 4831\n", __func__);
		_gate_ic_Power_on();
		/*rt4831a co-work with leds_i2c*/
		_gate_ic_i2c_panel_bias_enable(1);
	}

	boe_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0) {
		pr_info("Send initial code error!\n");
		boe_unprepare(panel);
	}

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s---\n", __func__);

	return ret;
}

static int boe_enable(struct drm_panel *panel)
{
	struct boe *ctx = panel_to_boe(panel);

	pr_info("%s+++\n", __func__);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	pr_info("%s---\n", __func__);

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 886486,
	.hdisplay = 2944,
	.hsync_start = 2944 + 25,
	.hsync_end = 2944 + 25 + 10,
	.htotal = 2944 + 25 + 10 + 24,
	.vdisplay = 1840,
	.vsync_start = 1840 + 26,
	.vsync_end = 1840 + 26 + 2,
	.vtotal = 1840 + 26 + 2 + 182,
};

#if SUPPORT_90Hz
static const struct drm_display_mode performance_mode_90hz = {
	.clock = 886486,
	.hdisplay = 2944,
	.hsync_start = 2944 + 25,
	.hsync_end = 2944 + 25 + 10,
	.htotal = 2944 + 25 + 10 + 24,
	.vdisplay = 1840,
	.vsync_start = 1840 + 1256,
	.vsync_end = 1840 + 1256 + 2,
	.vtotal = 1840 + 1256 + 2 + 182,
};
#endif

static const struct drm_display_mode performance_mode_120hz = {
	.clock = 766044,
	.hdisplay = 2944,
	.hsync_start = 2944 + 80,
	.hsync_end = 2944 + 80 + 10,
	.htotal = 2944 + 80 + 10 + 80,
	.vdisplay = 1840,
	.vsync_start = 1840 + 26,
	.vsync_end = 1840 + 26 + 2,
	.vtotal = 1840 + 26 + 2 + 182,
};

static const struct drm_display_mode performance_mode_60hz = {
	.clock = 766044,
	.hdisplay = 2944,
	.hsync_start = 2944 + 80,
	.hsync_end = 2944 + 80 + 10,
	.htotal = 2944 + 80 + 10 + 80,
	.vdisplay = 1840,
	.vsync_start = 1840 + 2076,
	.vsync_end = 1840 + 2076 + 2,
	.vtotal = 1840 + 2076 + 2 + 182,
};

static const struct drm_display_mode performance_mode_30hz = {
	.clock = 766044,
	.hdisplay = 2944,
	.hsync_start = 2944 + 80,
	.hsync_end = 2944 + 80 + 10,
	.htotal = 2944 + 80 + 10 + 80,
	.vdisplay = 1840,
	.vsync_start = 1840 + 6176,
	.vsync_end = 1840 + 6176 + 2,
	.vtotal = 1840 + 6176 + 2 + 182,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 491,
	.data_rate = 982,
	.data_rate_khz = 976137,
	.physical_width_um = 273615,
	.physical_height_um = 171009,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dual_swap = false,
	.vdo_per_frame_lp_enable = 1,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x53,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.dsc_params = {
		.enable = 1,
		.dual_dsc_enable = 1,
		.ver = 0x11, /* [7:4] major [3:0] minor */
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 1840, /* need to check */
		.pic_width = 1472,  /* need to check */
		.slice_height = 20,
		.slice_width = 736,
		.chunk_size = 736,
		.xmit_delay = 512,
		.dec_delay = 656,
		.scale_value = 32,
		.increment_interval = 537,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1402,
		.slice_bpg_offset = 940,
		.initial_offset = 6144,
		.final_offset = 4304,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 144,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0xB2, 0x80} },
		.dfps_cmd_table[3] = {0, 2, {0xB3, 0x00} },
	},
	.dyn = {
		.switch_en = 1,
		.vfp = 26,
		.hfp = 25,
		.hbp = 24,
	},
};

#if SUPPORT_90Hz
static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 491,
	.data_rate = 982,
	.data_rate_khz = 976137,
	.physical_width_um = 273615,
	.physical_height_um = 171009,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dual_swap = false,
	.vdo_per_frame_lp_enable = 1,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x53,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.dsc_params = {
		.enable = 1,
		.dual_dsc_enable = 1,
		.ver = 0x11, /* [7:4] major [3:0] minor */
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 1840, /* need to check */
		.pic_width = 1472,  /* need to check */
		.slice_height = 20,
		.slice_width = 736,
		.chunk_size = 736,
		.xmit_delay = 512,
		.dec_delay = 656,
		.scale_value = 32,
		.increment_interval = 537,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1402,
		.slice_bpg_offset = 940,
		.initial_offset = 6144,
		.final_offset = 4304,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 144,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0xB2, 0x80} },
		.dfps_cmd_table[3] = {0, 2, {0xB3, 0x00} },
	},
	.dyn = {
		.switch_en = 1,
		.vfp = 1256,
		.hfp = 25,
		.hbp = 24,
	},
};
#endif

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 491,
	.data_rate = 982,
	.data_rate_khz = 976137,
	.physical_width_um = 273615,
	.physical_height_um = 171009,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dual_swap = false,
	.vdo_per_frame_lp_enable = 1,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x53,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.dsc_params = {
		.enable = 1,
		.dual_dsc_enable = 1,
		.ver = 0x11, /* [7:4] major [3:0] minor */
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 1840, /* need to check */
		.pic_width = 1472,  /* need to check */
		.slice_height = 20,
		.slice_width = 736,
		.chunk_size = 736,
		.xmit_delay = 512,
		.dec_delay = 656,
		.scale_value = 32,
		.increment_interval = 537,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1402,
		.slice_bpg_offset = 940,
		.initial_offset = 6144,
		.final_offset = 4304,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0xB2, 0x91} },
		.dfps_cmd_table[3] = {0, 2, {0xB3, 0x40} },
	},
	.dyn = {
		.switch_en = 1,
		.vfp = 26,
		.hfp = 80,
		.hbp = 80,
	},
};

static struct mtk_panel_params ext_params_60hz = {
	.pll_clk = 491,
	.data_rate = 982,
	.data_rate_khz = 976137,
	.physical_width_um = 273615,
	.physical_height_um = 171009,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dual_swap = false,
	.vdo_per_frame_lp_enable = 1,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x53,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.dsc_params = {
		.enable = 1,
		.dual_dsc_enable = 1,
		.ver = 0x11, /* [7:4] major [3:0] minor */
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 1840, /* need to check */
		.pic_width = 1472,  /* need to check */
		.slice_height = 20,
		.slice_width = 736,
		.chunk_size = 736,
		.xmit_delay = 512,
		.dec_delay = 656,
		.scale_value = 32,
		.increment_interval = 537,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1402,
		.slice_bpg_offset = 940,
		.initial_offset = 6144,
		.final_offset = 4304,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0xB2, 0x91} },
		.dfps_cmd_table[3] = {0, 2, {0xB3, 0x40} },
	},
	.dyn = {
		.switch_en = 1,
		.vfp = 2076,
		.hfp = 80,
		.hbp = 80,
	},
};

static struct mtk_panel_params ext_params_30hz = {
	.pll_clk = 491,
	.data_rate = 982,
	.data_rate_khz = 976137,
	.physical_width_um = 273615,
	.physical_height_um = 171009,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dual_swap = false,
	.vdo_per_frame_lp_enable = 1,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x53,
		.count = 1,
		.para_list[0] = 0x00,
	},
	.dsc_params = {
		.enable = 1,
		.dual_dsc_enable = 1,
		.ver = 0x11, /* [7:4] major [3:0] minor */
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 1840, /* need to check */
		.pic_width = 1472,  /* need to check */
		.slice_height = 20,
		.slice_width = 736,
		.chunk_size = 736,
		.xmit_delay = 512,
		.dec_delay = 656,
		.scale_value = 32,
		.increment_interval = 537,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1402,
		.slice_bpg_offset = 940,
		.initial_offset = 6144,
		.final_offset = 4304,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		.ext_pps_cfg = {
			.enable = 1,
			.rc_buf_thresh = rc_buf_thresh,
			.range_min_qp = range_min_qp,
			.range_max_qp = range_max_qp,
			.range_bpg_ofs = range_bpg_ofs,
		},
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0xB2, 0x91} },
		.dfps_cmd_table[3] = {0, 2, {0xB3, 0x40} },
	},
	.dyn = {
		.switch_en = 1,
		.vfp = 6176,
		.hfp = 80,
		.hbp = 80,
	},
};

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct boe *ctx = panel_to_boe(panel);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int boe_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};

	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
	}
	bl_tb[1] = (level >> 8) & 0xFF;
	bl_tb[2] = level & 0xFF;

	if (!cb)
		return -1;
	pr_info("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
	return 0;
}

struct drm_display_mode *get_mode_by_id_hfp(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
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

	if (drm_mode_vrefresh(m) == 144) {
		ext->params = &ext_params;
		current_fps = 144;
	}
#if SUPPORT_90Hz
	else if (drm_mode_vrefresh(m) == 90) {
		ext->params = &ext_params_90hz;
		current_fps = 90;
	}
#endif
	else if (drm_mode_vrefresh(m) == 120) {
		ext->params = &ext_params_120hz;
		current_fps = 120;
	} else if (drm_mode_vrefresh(m) == 60) {
		ext->params = &ext_params_60hz;
		current_fps = 60;
	} else if (drm_mode_vrefresh(m) == 30) {
		ext->params = &ext_params_30hz;
		current_fps = 30;
	} else
		ret = 1;

	return ret;
}

static void mode_switch_to_144(struct drm_panel *panel)
{
	struct boe *ctx = panel_to_boe(panel);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x10);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0xB2, 0x80);//144hz
	boe_dcs_write_seq_static(ctx, 0xB3, 0x00);
}

static void mode_switch_to_120(struct drm_panel *panel)
{
	struct boe *ctx = panel_to_boe(panel);

	boe_dcs_write_seq_static(ctx, 0xFF, 0x10);
	boe_dcs_write_seq_static(ctx, 0xFB, 0x01);
	boe_dcs_write_seq_static(ctx, 0xB2, 0x91);//120hz
	boe_dcs_write_seq_static(ctx, 0xB3, 0x40);
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(connector, dst_mode);

	if (!m) {
		pr_info("ERROR!! drm_display_mode m is null\n");
		return -ENOMEM;
	}

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (drm_mode_vrefresh(m) == 144)
		mode_switch_to_144(panel);
#if SUPPORT_90Hz
	else if (drm_mode_vrefresh(m) == 90)
		mode_switch_to_144(panel);
#endif
	else if (drm_mode_vrefresh(m) == 120)
		mode_switch_to_120(panel);
	else if (drm_mode_vrefresh(m) == 60)
		mode_switch_to_120(panel);
	else if (drm_mode_vrefresh(m) == 30)
		mode_switch_to_120(panel);
	else
		ret = 1;

	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = boe_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,

};
#endif

static int boe_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
#if SUPPORT_90Hz
	struct drm_display_mode *mode2;
#endif
	struct drm_display_mode *mode3;
	struct drm_display_mode *mode4;
	struct drm_display_mode *mode5;

	pr_info("%s+++\n", __func__);

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		pr_info("failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

#if SUPPORT_90Hz
	mode2 = drm_mode_duplicate(connector->dev, &performance_mode_90hz);
	if (!mode2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_90hz.hdisplay, performance_mode_90hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);
#endif

	mode3 = drm_mode_duplicate(connector->dev, &performance_mode_120hz);
	if (!mode3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_120hz.hdisplay, performance_mode_120hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);

	mode4 = drm_mode_duplicate(connector->dev, &performance_mode_60hz);
	if (!mode4) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_60hz.hdisplay, performance_mode_60hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode4);
	mode4->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode4);

	mode5 = drm_mode_duplicate(connector->dev, &performance_mode_30hz);
	if (!mode5) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_30hz.hdisplay, performance_mode_30hz.vdisplay,
			 drm_mode_vrefresh(&performance_mode_30hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode5);
	mode5->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode5);

	connector->display_info.width_mm = 273;
	connector->display_info.height_mm = 171;

	return 1;
}

static const struct drm_panel_funcs boe_drm_funcs = {
	.disable = boe_disable,
	.unprepare = boe_unprepare,
	.prepare = boe_prepare,
	.enable = boe_enable,
	.get_modes = boe_get_modes,
};

static int boe_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *backlight;
	struct boe *ctx;
	unsigned int value;
	int ret;

	pr_info("%s+++\n", __func__);

	ctx = devm_kzalloc(dev, sizeof(struct boe), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);
		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->display_dual_swap = of_property_read_bool(dev->of_node,
					      "display-dual-swap");
	pr_notice("ctx->display_dual_swap=%d\n", ctx->display_dual_swap);
	if (ctx->display_dual_swap) {
		ext_params.dual_swap = true;
#if SUPPORT_90Hz
		ext_params_90hz.dual_swap = true;
#endif
		ext_params_120hz.dual_swap = true;
		ext_params_60hz.dual_swap = true;
		ext_params_30hz.dual_swap = true;
	}

	ret = of_property_read_u32(dev->of_node, "gate-ic", &value);
	if (ret < 0) {
		pr_info("%s, Failed to find gate-ic\n", __func__);
		value = 0;
	} else {
		pr_info("%s, Find gate-ic, value=%d\n", __func__, value);
		ctx->gate_ic = value;
	}

	if (ctx->gate_ic == 0) {
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
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	/*
	 * ctx->hwen = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
	 * if (IS_ERR(ctx->hwen)) {
	 *	dev_err(dev, "cannot get hwen-gpios %ld\n",
	 *	PTR_ERR(ctx->hwen));
	 *	return PTR_ERR(ctx->hwen);
	 * }
	 * devm_gpiod_put(dev, ctx->hwen);
	 */

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &boe_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	ctx->panel.dev = dev;
	ctx->panel.funcs = &boe_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		dev_err(dev, "mipi_dsi_attach fail, ret=%d\n", ret);
		return -EPROBE_DEFER;
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s-\n", __func__);

	return ret;
}
static void boe_remove(struct mipi_dsi_device *dsi)
{
	struct boe *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	if (ext_ctx != NULL) {
		mtk_panel_detach(ext_ctx);
		mtk_panel_remove(ext_ctx);
	}
#endif
}
static const struct of_device_id boe_of_match[] = {
	{
		.compatible = "boe,ts127qfmll1dkp0",
	},
	{}
};
MODULE_DEVICE_TABLE(of, boe_of_match);
static struct mipi_dsi_driver boe_driver = {
	.probe = boe_probe,
	.remove = boe_remove,
	.driver = {
			.name = "panel-boe-ts127qfmll1dkp0",
			.owner = THIS_MODULE,
			.of_match_table = boe_of_match,
		},
};
module_mipi_dsi_driver(boe_driver);
MODULE_AUTHOR("Huijuan Xie <huijuan.xie@mediatek.com>");
MODULE_DESCRIPTION("boe ts127qfmll1dkp0 wqxga2944 Panel Driver");
MODULE_LICENSE("GPL");

