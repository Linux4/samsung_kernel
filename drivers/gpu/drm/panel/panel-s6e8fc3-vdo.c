// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
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

//#define ENABLE_MTK_LCD_DTS_CHECK

#define REGFLAG_DELAY 0
#define ENABLE 1
#define DISABLE 0

struct lcm {
	struct device *dev;
	struct drm_panel panel;
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
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

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

static struct LCM_setting_table init_setting_cmd[] = {
	{3, {0x9F, 0xA5, 0xA5}},

	/* Sleep Out */
	{1, {0x11} },
	{REGFLAG_DELAY, {110}}, /* Delay 110ms */

	/* 4 Operating Setting */
	/* 4.1 Common Setting */
	{2, {0x9D, 0x01}},
	/* 4.1.1 DSC Setting */
	{90, {0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
				0x04, 0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C,
				0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0xDD,
				0x00, 0x07, 0x00, 0x0C, 0x02, 0x77, 0x02, 0x8B,
				0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
				0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
				0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
				0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
				0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
				0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
				0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
				0x00}},

	/* 4.1.2 Freq. Setting */
	{3, {0xF0, 0x5A, 0x5A}},
	{2, {0xF2, 0x0C}},
	{2, {0xF7, 0x0B}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.1.3 FFC SET, 898Mbps */
	{3, {0xF0, 0x5A, 0x5A}},
	{8, {0xDF, 0x09, 0x30 , 0x95, 0x2F, 0xD6, 0x4B, 0x51}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.1.4 TSP Vsync Setting */
	{3, {0xF0, 0x5A, 0x5A}},
	{11,{0xE8, 0x00, 0x00, 0x14, 0x18, 0x00, 0x00, 0x00, 0x01,
				0x01, 0x01}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.1.5 ACL Setting */
	{3, {0xF0, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x0D, 0xB4}},
	{18,{0xB4, 0x44, 0x00, 0xB0, 0x51, 0x66, 0x98, 0x15, 0x55,
				0x55, 0x55, 0x08, 0xF1, 0xC6, 0x48, 0x40, 0x00,
				0x00}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.1.6 ETC Setting */
	{3, {0xF0, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x15, 0xF6}},
	{2, {0xF6, 0xF0}},
	{4, {0xB0, 0x00, 0x28, 0xF6}},
	{2, {0xF6, 0xF0}},
	{4, {0xB0, 0x00, 0x3B, 0xF6}},
	{2, {0xF6, 0xF0}},
	{2, {0xF7, 0x0B}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.1.7 SEED Setting */
	{3, {0xF0, 0x5A, 0x5A}},
	{9, {0xB8, 0x00, 0x00, 0x00, 0x5F, 0x8F, 0x00, 0x5F, 0x8F}},
	{2, {0xF7, 0x0B}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.2 Brightness Setting */
	/* 4.2.1 Max & Dimming */
	{3, {0xF0, 0x5A, 0x5A}},
	{3, {0xFC, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x1B, 0xD7}},
	{2, {0xD7, 0x90}},
	{2, {0xFE, 0xB0}},
	{2, {0xFE, 0x30}},
	{3, {0x60, 0x01, 0x00}},
	{2, {0x53, 0x20}},
	{3, {0x51, 0x03, 0xFF}},
	{2, {0xF7, 0x0B}},
	{3, {0xFC, 0xA5, 0xA5}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.2.2 HBM & Interpolation Dimming 8 00~420nit) */
	{3, {0xF0, 0x5A, 0x5A}},
	{3, {0xFC, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x1B, 0xD7}},
	{2, {0xD7, 0x90}},
	{2, {0xFE, 0xB0}},
	{2, {0xFE, 0x30}},
	{3, {0x60, 0x01, 0x00}},
	{2, {0x53, 0xE0}},
	{3, {0x51, 0x03, 0xFF}},
	{2, {0xF7, 0x0B}},
	{3, {0xFC, 0xA5, 0xA5}},
	{3, {0xF0, 0xA5, 0xA5}},

	/* 4.2.3 ACL ON/OFF */
	{3, {0xF0, 0x5A, 0x5A}},
	{2, {0x55, 0x00}},
	{3, {0xF0, 0xA5, 0xA5}},

	{REGFLAG_DELAY, {70}}, /* Delay 70ms */
	{1, {0x29}}, /* Display On */
	{REGFLAG_DELAY, {40}}, /* Delay 40ms */

	/* OSC Setting */
	{3, {0xFC, 0x5A, 0x5A}},
	{4, {0xB0, 0x00, 0x1B, 0xD7}},
	{2, {0xD7, 0x90}},
	{2, {0xFE, 0xB0}},
	{2, {0xFE, 0x30}},
	{2, {0xF7, 0x0B}},
	{3, {0xFC, 0xA5, 0xA5}},
};

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	int i;
	int size;

	for (i = 0; i < count; i++) {
		size = table[i].count;

		switch (size) {
		case REGFLAG_DELAY:
			mdelay(table[i].para_list[0]);
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
	push_table(ctx, init_setting_cmd,
			sizeof(init_setting_cmd) / sizeof(struct LCM_setting_table));
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->enabled = false;
	pr_info("[%s][s6e8fc3] finished\n", __func__);

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	mdelay(25);
	lcm_dcs_write_seq_static(ctx, 0x10);
	mdelay(130);

	ctx->error = 0;
	ctx->prepared = false;

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->lcd_3p0_en_gpio = devm_gpiod_get(ctx->dev, "lcd-3p0-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcd_3p0_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get lcd-3p0-en_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcd_3p0_en_gpio));
		return 0;
	}
	gpiod_set_value(ctx->lcd_3p0_en_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->lcd_3p0_en_gpio);

	//VDDI: VCAMIO OFF
	lcm_panel_bias_disable();
	pr_info("[%s][s6e8fc3] finished\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);

	if (ctx->prepared)
		return 0;

	//VDDI: VCAMIO ON
	lcm_panel_bias_enable();

	ctx->lcd_3p0_en_gpio = devm_gpiod_get(ctx->dev, "lcd-3p0-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcd_3p0_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get lcd-3p0-en_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcd_3p0_en_gpio));
		return 0;
	}
	gpiod_set_value(ctx->lcd_3p0_en_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->lcd_3p0_en_gpio);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return 0;
	}

	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(1);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

	pr_info("[%s][s6e8fc3] finished\n", __func__);

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->enabled = true;

	pr_info("[%s][s6e8fc3] finished\n", __func__);

	return 0;
}

#define HFP (250)//(70)
#define HSA (70)//(20)
#define HBP (110)
#define VFP (48)
#define VSA (2)
#define VBP (14)
#define VAC (2400)
#define HAC (1080)

static struct drm_display_mode default_mode = {
	.clock = 334857,// Pixel Clock = vtotal(2464) * htotal * vrefresh(90) / 1000;
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 90,
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
	unsigned char id[3] = {0xDA, 0xDB, 0xDC};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0)
		pr_info("%s error\n", __func__);

	pr_info("ATA read data %x %x %x\n",	__func__, data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;
	pr_info("[%s][s6e8fc3] ATA expect read data is %x %x %x\n",
		__func__, id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	unsigned int mapping_level;
	char enable_cmd1[] = { 0xf0, 0x5a, 0x5a };
	char enable_cmd2[] = { 0xfc, 0x5a, 0x5a };
	char global_para[] = { 0xb0, 0x00, 0x1B, 0xD7};
	char OSC_setting1[] = { 0xd7, 0x90 };
	char OSC_setting2[] = { 0xfe, 0xb0 };
	char OSC_setting3[] = { 0xfe, 0x30 };
	char freq_setting[] = { 0x60, 0x01, 0x00 };
	char dimming_speed[] = { 0x53, 0x20 };
	char bl_tb0[] = {0x51, 0x03, 0xFF};
	char ltps_cmd[] = { 0xf7, 0x0b };
	char disable_cmd1[] = { 0xfc, 0xa5, 0xa5 };
	char disable_cmd2[] = { 0xf0, 0xa5, 0xa5 };

	if(level > 255 )
		level = 255;

	mapping_level = level * 1023 / 255;
	bl_tb0[1] = ((mapping_level >> 8) & 0xf);
	bl_tb0[2] = (mapping_level & 0xff);

	pr_info("[%s][s6e8fc3] bl_tb0[0] = 0x%x, bl_tb0[1] = 0x%x, level = %d(0x%x)\n",
		__func__, bl_tb0[1], bl_tb0[2], level, mapping_level);

	if (!cb)
		return -1;

	cb(dsi, handle, enable_cmd1, ARRAY_SIZE(enable_cmd1));
	cb(dsi, handle, enable_cmd2, ARRAY_SIZE(enable_cmd2));
	cb(dsi, handle, global_para, ARRAY_SIZE(global_para));
	cb(dsi, handle, OSC_setting1, ARRAY_SIZE(OSC_setting1));
	cb(dsi, handle, OSC_setting2, ARRAY_SIZE(OSC_setting2));
	cb(dsi, handle, OSC_setting3, ARRAY_SIZE(OSC_setting3));
	cb(dsi, handle, freq_setting, ARRAY_SIZE(freq_setting));
	cb(dsi, handle, dimming_speed, ARRAY_SIZE(dimming_speed));
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	cb(dsi, handle, ltps_cmd, ARRAY_SIZE(ltps_cmd));
	cb(dsi, handle, disable_cmd1, ARRAY_SIZE(disable_cmd1));
	cb(dsi, handle, disable_cmd2, ARRAY_SIZE(disable_cmd2));

	return 0;
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
		.slice_height = 40,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 989,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 651,
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
	.data_rate = 1174,//795,//898,
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
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

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

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
	struct lcm *ctx;
	int ret;
#ifdef ENABLE_MTK_LCD_DTS_CHECK
	struct device_node *lcd_comp;

	lcd_comp = of_find_compatible_node(NULL, NULL,
	"s6e8fc3,fhdp,vdo");
	if (!lcd_comp) {
		pr_info("[%s][s6e8fc3] panel compatible doesn't match\n", __func__);
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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
					MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
					| MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->lcd_3p0_en_gpio = devm_gpiod_get(ctx->dev, "lcd-3p0-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->lcd_3p0_en_gpio)) {
		dev_info(ctx->dev, "%s: cannot get lcd_3p0_en_gpio %ld\n",
			__func__, PTR_ERR(ctx->lcd_3p0_en_gpio));
		return PTR_ERR(ctx->lcd_3p0_en_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->lcd_3p0_en_gpio);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
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

	pr_info("[%s][s6e8fc3] probe success\n", __func__);

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
	{ .compatible = "s6e8fc3,fhdp,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-s6e8fc3-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_DESCRIPTION("s6e8fc3 fhdp LCD Panel Driver");
MODULE_LICENSE("GPL v2");
