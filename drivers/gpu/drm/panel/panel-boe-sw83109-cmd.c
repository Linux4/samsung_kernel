// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_mode.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "../mediatek/mtk_disp_recovery.h"

#include "../mediatek/mtk_cust.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define ENABLE_MTK_LCD_DTS_CHECK

#define REGFLAG_DELAY 0
#define ENABLE 1
#define DISABLE 0

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *dvdd_en_gpio;
	struct gpio_desc *lcd_3p0_en_gpio;

	bool prepared;
	bool enabled;

	int error;
};

struct LCM_setting_table {
	unsigned char count;
	unsigned char para_list[129];
};

#define lcm_dcs_write_set_lcm(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	struct mtk_ddic_dsi_msg *cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));\
	cmd_msg->channel = 0;\
	cmd_msg->flags = 0;\
	cmd_msg->tx_cmd_num = 1;\
	cmd_msg->tx_buf[0] = d;\
	cmd_msg->tx_len[0] = (sizeof(d) / sizeof(u8));\
	set_lcm(cmd_msg);\
	vfree(cmd_msg);\
})

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_pps_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;

	if (len < 128)
		return;

	ret = mipi_dsi_picture_parameter_set(dsi, data);
	if (ret < 0) {
		pr_info("[%s] error %zd writing seq: %ph\n", __func__, ret, data);
		ctx->error = ret;
	}
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
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

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
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

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static struct regulator *disp_bias_vcamio;

static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_vcamio = regulator_get(NULL, "vcamio");
	if (IS_ERR(disp_bias_vcamio)) { /* handle return value */
		ret = PTR_ERR(disp_bias_vcamio);
		pr_info("get vcamio fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;

	return ret; /* must be 0 */

}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_vcamio, 1800000, 1800000);
	if (ret < 0)
		pr_info("set voltage disp_bias_vcamio fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_vcamio);
	if (ret < 0)
		pr_info("enable regulator disp_bias_vcamio fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_vcamio);
	if (ret < 0)
		pr_info("disable regulator disp_bias_vcamio fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

// picture parameter setting (0x0A) 128 bytes
static char pps_setting_table[128] = {
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60, 0x04, 0x38,
	0x00, 0x3C, 0x02, 0x1C, 0x02, 0x1C, 0x02, 0x00, 0x02, 0x0E,
	0x00, 0x20, 0x05, 0xD2, 0x00, 0x07, 0x00, 0x0C, 0x01, 0xA1,
	0x01, 0xB2, 0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54,
	0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01, 0x02,
	0x01, 0x00, 0x09, 0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA,
	0x19, 0xF8, 0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static struct LCM_setting_table init_setting_cmd[] = {
	{2,   {0xB0, 0xA1}},
	{7,   {0xB1, 0x11, 0x0C, 0x00, 0x80, 0x00, 0x00}}, // 0.806GHz
	{44,  {0xB3, 0x23, 0x22, 0x23, 0x22, 0x50, 0x4F, 0x23, 0x22, 0x10,
		0x00, 0x0F, 0x00, 0x90, 0x09, 0x0F, 0x00, 0x10, 0x00, 0x0F,
		0x00, 0x10, 0x00, 0x0F, 0x00, 0x3B, 0x3B, 0x3C, 0x3C, 0x5F,
		0x5F, 0x78, 0x78, 0x10, 0x00, 0x0F, 0x00, 0x1E, 0x22, 0x14,
		0x00, 0x0B, 0x00, 0x78}}, // 0.806GHz
	{32,  {0xC1, 0xC0, 0x60, 0xB1, 0x3F, 0x1A, 0x99, 0x99, 0x00, 0x15,
		0x00, 0x39, 0x07, 0x00, 0x80, 0x05, 0x0F, 0x02, 0x99, 0x99,
		0x99, 0x00, 0x00, 0x00, 0x05, 0x05, 0x05, 0x01, 0x00, 0x00,
		0x00, 0x40}}, // 0.806GHz
	{16,  {0xCA, 0x00, 0xA0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x60, 0x00, 0x01, 0x00}}, // 0.806GHz
	{2,	  {0xB6, 0x01}},
	{2,   {0xB0, 0xA6}},
	{2,   {0xF7, 0xAC}}, // 0.8006GHz
	{8,   {0xFC, 0x00, 0x00, 0x00, 0x81, 0x80, 0xAD, 0x44}},
	{2,   {0xB0, 0xCA}},
	{5,   {0x2A, 0x00, 0x00, 0x04, 0x37}},
	{5,   {0x2B, 0x00, 0x00, 0x09, 0x5F}},
	{13,  {0x30, 0x00, 0x00, 0x09, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00}},
	{5,   {0x31, 0x00, 0x00, 0x04, 0x37}},
	{2,   {0x35, 0x00}},
	{2,   {0x4D, 0x19}},
	{3,   {0x3D, 0x00, 0X01}},
	{3,	  {0x53, 0x00, 0X00}},
	{17,  {0x55, 0x08, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10,
		0x00, 0x72, 0xD0, 0x1C, 0x70, 0xD0, 0xE1}},
	{3,   {0x68, 0x08, 0x02}}, // 0.806GHz
	{5,   {0x69, 0x00, 0x00, 0x00, 0x00}},
	{1,	  {0x11}},
	{REGFLAG_DELAY, {50}}, /* Delay 50ms */

	{2,   {0xB0, 0xCA}},
	{3,	  {0x53, 0x20, 0X00}},
	{2,	  {0xB0, 0xA3}},
	{9,	  {0xB3, 0x20, 0xE2, 0x40, 0x00, 0x08, 0x06, 0x40, 0x00}},
	{2,	  {0xB0, 0xA4}},
	{46,  {0xB1, 0x07, 0xF4, 0xFF, 0x3F, 0xFF, 0x3F, 0x7F, 0x87, 0x20,
		0x80, 0x68, 0x88, 0x68, 0x89, 0x89, 0x69, 0x2F, 0xFF, 0x4F,
		0xFF, 0x6E, 0xEE, 0xC8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0x24, 0xFF, 0xFF, 0x04, 0x04, 0x00, 0x37,
		0x09, 0x00, 0x5F, 0xC0, 0xC0, 0x20}},
	{2,	  {0xB0, 0xA1}},
	{14,  {0xF0, 0x32, 0x03, 0x8B, 0x03, 0x8C, 0x01, 0x0E, 0x01, 0x0F,
		0x80, 0xDD, 0x01, 0x00}}, // 0.806GHz
	{2,	  {0xB0, 0xA7}},
	{3,	  {0xB1, 0xE0, 0x00}},
	{2,	  {0xB0, 0xCA}},
	{2,	  {0x57, 0x01}},
	{3,	  {0x51, 0x0F, 0xFF}},
	{REGFLAG_DELAY, {50}}, /* Delay 50ms */

	{1,	  {0x29}}, /* Display On */
};

static struct LCM_setting_table mode_switch_60_to_120_cmd[] = {
	{2,   {0xB0, 0xA6}},
	{8,	  {0xFC, 0x00, 0x00, 0x00, 0x81, 0x80, 0x8B, 0x44}},
	{2,   {0xB0, 0xCA}},
	{2,	  {0x57, 0x03}},
};

static struct LCM_setting_table mode_switch_120_to_60_cmd[] = {
	{2,   {0xB0, 0xA6}},
	{8,	  {0xFC, 0x00, 0x00, 0x00, 0x81, 0x80, 0xAD, 0x44}},
	{2,   {0xB0, 0xCA}},
	{2,	  {0x57, 0x01}},
};

static void puch_pps_table(struct lcm *ctx, char *data, unsigned int count)
{
	lcm_pps_write(ctx, data, count);
}

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	int i;
	int size;

	for (i = 0; i < count; i++) {
		size = table[i].count;

		switch (size) {
		case REGFLAG_DELAY:
			msleep(table[i].para_list[0]);
			break;
		default:
			lcm_dcs_write(ctx, table[i].para_list, table[i].count);
			break;
		}
	}
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s\n", __func__);
	puch_pps_table(ctx, pps_setting_table, sizeof(pps_setting_table));
	push_table(ctx, init_setting_cmd,
		sizeof(init_setting_cmd) / sizeof(struct LCM_setting_table));
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->enabled = false;

	pr_info("[%s][sw83109] finished\n", __func__);

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(85);

	ctx->error = 0;
	ctx->prepared = false;


	ctx->lcd_3p0_en_gpio = devm_gpiod_get(ctx->dev, "lcd-3p0-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcd_3p0_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get lcd-3p0-en_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcd_3p0_en_gpio));
		return 0;
	}
	gpiod_set_value(ctx->lcd_3p0_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->lcd_3p0_en_gpio);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_panel_bias_disable();
	pr_info("[%s][sw83109] finished\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);

	if (ctx->prepared)
		return 0;

	lcm_panel_bias_enable();

	ctx->lcd_3p0_en_gpio = devm_gpiod_get(ctx->dev, "lcd-3p0-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcd_3p0_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get lcd-3p0-en_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcd_3p0_en_gpio));
		return 0;
	}
	gpiod_set_value(ctx->lcd_3p0_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->lcd_3p0_en_gpio);

	ctx->dvdd_en_gpio = devm_gpiod_get(ctx->dev, "dvdd-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_en_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_en_gpio));
		return 0;
	}
	gpiod_set_value(ctx->dvdd_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->dvdd_en_gpio);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return 0;
	}

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(4000, 5000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 3000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(4000, 5000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("[%s][sw83109] finished\n", __func__);

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->enabled = true;

	pr_info("[%s][sw83109] finished\n", __func__);

	return 0;
}

#define HFP (60)
#define HSA (4)
#define HBP (60)
#define VFP (40)
#define VSA (4)
#define VBP (20)
#define VAC (2400)
#define HAC (1080)

static struct drm_display_mode default_mode = {
	.clock = 177999,	// Pixel Clock = vtotal * htotal * vrefresh / 1000;
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 60,
};

static struct drm_display_mode performance_mode = {
	.clock = 355999,	// Pixel Clock = vtotal * htotal * vrefresh / 1000;
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 120,
};
#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x00, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0)
		pr_err("%s error\n", __func__);

	pr_info("ATA read data %x %x %x\n",	__func__, data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;
	pr_info("[%s][sw83109] ATA expect read data is %x %x %x\n",
		__func__, id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x00, 0x00};

	bl_tb0[1] = (level >> 8) & 0x07;
	bl_tb0[2] = level & 0xFF;

	pr_info("[%s][sw83109] bl_tb0[0] = 0x%x, bl_tb0[1] = 0x%x, level = %d\n",
		__func__, bl_tb0[1], bl_tb0[2], level);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = {
//	.pll_clk = 210,
	//.vfp_low_power = 750,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},

	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 60,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 1490,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 417,
		.slice_bpg_offset = 434,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.data_rate = 806,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
	},
};

static struct mtk_panel_params ext_params_120hz = {
//	.pll_clk = 210,
	//.vfp_low_power = 750,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},

	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 60,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 1490,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 417,
		.slice_bpg_offset = 434,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
	.data_rate = 806,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
	},
};

static struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);

	if (m->vrefresh == 60)
		ext->params = &ext_params;
	else if (m->vrefresh == 120)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_60_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == AFTER_DSI_POWERON) {
		lcm_dcs_write_set_lcm(ctx, 0xB0, 0xA6);
		lcm_dcs_write_set_lcm(ctx, 0xFC, 0x00, 0x00, 0x00, 0x81, 0x80, 0x8B, 0x44);
		lcm_dcs_write_set_lcm(ctx, 0xB0, 0xCA);
		lcm_dcs_write_set_lcm(ctx, 0x57, 0x03);

	}
}

static void mode_switch_120_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (stage == AFTER_DSI_POWERON) {
		lcm_dcs_write_set_lcm(ctx, 0xB0, 0xA6);
		lcm_dcs_write_set_lcm(ctx, 0xFC, 0x00, 0x00, 0x00, 0x81, 0x80, 0xAD, 0x44);
		lcm_dcs_write_set_lcm(ctx, 0xB0, 0xCA);
		lcm_dcs_write_set_lcm(ctx, 0x57, 0x01);
	}
}
static int mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;

	if (cur_mode == 0 && dst_mode == 1) { /* 60 switch to 120 */
		mode_switch_60_to_120(panel, stage);
	} else if (cur_mode == 1 && dst_mode == 0) { /* 120 switch to 60 */
		mode_switch_120_to_60(panel, stage);
	} else
		ret = 1;

	return ret;
}
static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay,
			performance_mode.vdisplay,
			performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	panel->connector->display_info.width_mm = 70;				// Physical width
	panel->connector->display_info.height_mm = 140;				// Physical height

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
	//struct drm_bridge *bridge;
	struct lcm *ctx;
	int ret;
#ifdef ENABLE_MTK_LCD_DTS_CHECK
	struct device_node *lcd_comp;

	lcd_comp = of_find_compatible_node(NULL, NULL,
	"sw83109,fhdplus,cmd");
	if (!lcd_comp) {
		pr_info("[%s][sw83109] panel compatible doesn't match\n", __func__);
		return -1;
	}
#endif

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->lcd_3p0_en_gpio = devm_gpiod_get(ctx->dev, "lcd-3p0-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcd_3p0_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get lcd_3p0_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcd_3p0_en_gpio));
		return PTR_ERR(ctx->lcd_3p0_en_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->lcd_3p0_en_gpio);


	ctx->dvdd_en_gpio = devm_gpiod_get(ctx->dev, "dvdd-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_en_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_en_gpio));
		return PTR_ERR(ctx->dvdd_en_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->dvdd_en_gpio);


	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("[%s][sw83109] probe success\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "sw83109,fhdplus,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-boe-sw83109-cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Jenny Kim <jenny.kim@mediatek.com>");
MODULE_DESCRIPTION("BOE sw83109 CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
