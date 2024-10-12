/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>

#include <linux/gpio/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_drm_graphics_base.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_panel_ext.h"
#endif

#include "lcm_cust_common.h"
#include "panel_notifier.h"

extern u8 rec_esd;	//ESD recover flag global value

struct tianma {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
    //struct gpio_desc *pm_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;

	int error;
};

#define tianma_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define tianma_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

static inline struct tianma *panel_to_tianma(struct drm_panel *panel)
{
	return container_of(panel, struct tianma, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int tianma_dcs_read(struct tianma *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void tianma_panel_get_data(struct tianma *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = tianma_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void tianma_dcs_write(struct tianma *ctx, const void *data, size_t len)
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

static int last_brightness = 0;	//the last backlight level values
u8 esd_bl2[] = {0x00, 0x00};
extern void ili_resume_by_ddi(void);
static void tianma_panel_init(struct tianma *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("mipi_1096-1086 %s+\n", __func__);

	ili_resume_by_ddi();
	printk("[ILITEK]ilitek_resume in tianma_panel_init\n ");
	tianma_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x07, 0x02);
	tianma_dcs_write_seq_static(ctx, 0x1B, 0x01);			//frame rate 90
	tianma_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x07, 0x03);
	tianma_dcs_write_seq_static(ctx, 0x83, 0xB8);			//PWM 12 bit 28.3K
	tianma_dcs_write_seq_static(ctx, 0x84, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x07, 0x06);
	tianma_dcs_write_seq_static(ctx, 0x08, 0x20);
	tianma_dcs_write_seq_static(ctx, 0x80, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x4E, 0x40);			//1 bit esd disable page6
	tianma_dcs_write_seq_static(ctx, 0x4D, 0x01);			//1 bit esd disable page8
	tianma_dcs_write_seq_static(ctx, 0x48, 0x09);			//1 bit esd enable
	tianma_dcs_write_seq_static(ctx, 0xC7, 0x05);
	tianma_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x07, 0x07);
	tianma_dcs_write_seq_static(ctx, 0x11, 0x16);
	tianma_dcs_write_seq_static(ctx, 0x82, 0x20);
	pr_info("%s tianma: write 0x82 value to 0x20 in kernel init\n", __func__);
	tianma_dcs_write_seq_static(ctx, 0xFF, 0x78, 0x07, 0x00);
	//tianma_dcs_write_seq_static(ctx, 0x51, 0x0F,0xFF);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x2C);
	tianma_dcs_write_seq_static(ctx, 0x68, 0x04,0x00);
	tianma_dcs_write_seq_static(ctx, 0x35, 0x00);

	tianma_dcs_write_seq_static(ctx, 0x11);
	msleep(70);
	/* Display On*/
	tianma_dcs_write_seq_static(ctx, 0x29);
	msleep(20);

	if(1 == rec_esd) {
		pr_info("recover_esd_set_backlight\n");
		msleep(200);
		esd_bl2[0] = ((last_brightness >> 8) & 0xf);
		esd_bl2[1] = (last_brightness & 0xff);
		tianma_dcs_write_seq(ctx, 0x51, esd_bl2[0], esd_bl2[1]);
	}

	pr_info("%s-\n", __func__);
}

static int tianma_disable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}
extern int ili_sleep_handler(int mode);
extern bool  gestrue_status;
extern bool  gestrue_spay;
static int tianma_unprepare(struct drm_panel *panel)
{

	struct tianma *ctx = panel_to_tianma(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;

	//add for TP suspend
	//panel_notifier_call_chain(PANEL_UNPREPARE, NULL);
	
	ili_sleep_handler(1);
	printk("[ILITEK]ilitek_suspend in tianma-unprepare\n ");
	usleep_range(6000, 6001);
	tianma_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(20);
	tianma_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);

	msleep(200);
	if((gestrue_status == 1 ) || ( gestrue_spay == 1)){
		printk("[ILITEK]tianma_unprepare gesture_status = 1\n ");
	} else {
		printk("[ILITEK]tianma_unprepare gesture_flag = 0\n ");
#if 0
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		usleep_range(2000, 2001);
#endif
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		usleep_range(2000, 2001);

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		usleep_range(2000, 2001);
#if 0
		ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->pm_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->pm_gpio);
#endif
	}
	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

extern void ili_resume_by_ddi(void);
static int tianma_prepare(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// reset  L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	usleep_range(5000, 5001);

	//ctx->pm_gpio = devm_gpiod_get(ctx->dev, "pm-enable", GPIOD_OUT_HIGH);
	//gpiod_set_value(ctx->pm_gpio, 1);
	//devm_gpiod_put(ctx->dev, ctx->pm_gpio);
	//usleep_range(5000, 5001);

	// end
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	usleep_range(3000, 3001);
	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	_lcm_i2c_write_bytes(AVDD_REG, 0xf);
	_lcm_i2c_write_bytes(AVEE_REG, 0xf);

	tianma_panel_init(ctx);

	//ili_resume_by_ddi();

	//add for TP resume
	//panel_notifier_call_chain(PANEL_PREPARE, NULL);
	//printk("[ILITEK]tpd_ilitek_notifier_callback in tianma_prepare\n ");

	ret = ctx->error;
	if (ret < 0)
		tianma_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	tianma_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int tianma_enable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (20)
#define HSA (4)
#define HBP (30)
#define VFP_90HZ (20)
#define VFP_60HZ (1308)
#define VSA (2)
#define VBP (60)
#define VAC (2408)
#define HAC (1080)
static const struct drm_display_mode default_mode = {
	.clock = 257055,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60HZ,
	.vsync_end = VAC + VFP_60HZ + VSA,
	.vtotal = VAC + VFP_60HZ + VSA + VBP,
	.vrefresh = 60,
};

static const struct drm_display_mode performance_mode = {
	.clock = 255922,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90HZ,
	.vsync_end = VAC + VFP_90HZ + VSA,
	.vtotal = VAC + VFP_90HZ + VSA + VBP,
	.vrefresh = 90,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 530,
	//.vfp_low_power = VFP_45HZ,
	.physical_width_um = 68430,
	.physical_height_um = 152570,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.dyn = {
		.switch_en = 1,
		.pll_clk = 533,
		.hbp = 38,
		.vfp = VFP_60HZ,
	},
    .dyn_fps = {
        .switch_en = 1,
        .vact_timing_fps = 90,
    },
	.phy_timcon = {
		.hs_zero = 34,
		.hs_trail = 26,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 530,
	.vfp_low_power = VFP_60HZ,
	.physical_width_um = 68430,
	.physical_height_um = 152570,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.dyn = {
		.switch_en = 1,
		.pll_clk = 533,
		.hbp = 38,
		.vfp = VFP_90HZ,
	},
    .dyn_fps = {
        .switch_en = 1,
        .vact_timing_fps = 90,
    },
	.phy_timcon = {
		.hs_zero = 34,
		.hs_trail = 26,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int tianma_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb0[] = {0x51, 0x0f, 0xff,0x00};
	//+S96901AA4-194 liuxueyou.wt, 20231016, add, screen backlight flashing during sleep
	char bl_tb1[] = {0x53, 0x24,0x00,0x00};
	char bl_tb2[] = {0x53, 0x2c,0x00,0x00};
	pr_info("%s backlight = -%d\n", __func__, level);
	if (level > 255)
		level = 255;

	level = level * 2047 / 255;
	bl_tb0[1] = ((level >> 8) & 0xf);
	bl_tb0[2] = (level & 0xff);
	pr_info("%s tianma backlight = -%d,bl_tb0[1] = %x,bl_tb0[2] = %x\n", __func__, level,bl_tb0[1],bl_tb0[2]);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	if(level == 0){
		cb(dsi, handle, bl_tb1, ARRAY_SIZE(bl_tb1));
	}else if(last_brightness == 0 && level > 0){
		cb(dsi, handle, bl_tb2, ARRAY_SIZE(bl_tb2));
	}
	//-S96901AA4-194 liuxueyou.wt, 20231016, add, screen backlight flashing during sleep
	last_brightness = level;

	return 0;
}
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
    pr_info("samir:%s+ mode:%d\n", __func__,m->vrefresh);
	if (m->vrefresh == 60)
		ext->params = &ext_params;
	else if (m->vrefresh == 90)
		ext->params = &ext_params_90hz;
	else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct tianma *ctx = panel_to_tianma(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = tianma_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	//.ext_param_get = mtk_panel_ext_param_get,
	//.mode_switch = mode_switch,
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

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int tianma_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
    pr_info("samir:%s+ \n", __func__);
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

	//panel->connector->display_info.width_mm = (68430/1000);
	//panel->connector->display_info.height_mm = (152570/1000);

	return 1;
}

static const struct drm_panel_funcs tianma_drm_funcs = {
	.disable = tianma_disable,
	.unprepare = tianma_unprepare,
	.prepare = tianma_prepare,
	.enable = tianma_enable,
	.get_modes = tianma_get_modes,
};

static int tianma_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct tianma *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct tianma), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 3;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
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
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	//ctx->pm_gpio = devm_gpiod_get(dev, "pm-enable", GPIOD_OUT_HIGH);
	//if (IS_ERR(ctx->pm_gpio)) {
	//	dev_info(dev, "cannot get pm_gpio %ld\n",
	//		 PTR_ERR(ctx->pm_gpio));
	//	return PTR_ERR(ctx->pm_gpio);
	//}
	//devm_gpiod_put(dev, ctx->pm_gpio);
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
	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &tianma_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;


	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	pr_info("%s- wt,tianma,ili7807s,cphy,vdo,90hz\n", __func__);

	return ret;
}

static int tianma_remove(struct mipi_dsi_device *dsi)
{
	struct tianma *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static void tianma_panel_shutdown(struct mipi_dsi_device *dsi)
{
	pr_info("ili7807s tianma%s++\n", __func__);
	gestrue_status = 0;
	gestrue_spay = 0;
	pr_info("optimize shutdown sequence gestrue_status gestrue_spay is 0 %s++\n", __func__);
}

static const struct of_device_id tianma_of_match[] = {
	{
	    .compatible = "wt,ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_tianma",
	},
	{}
};

MODULE_DEVICE_TABLE(of, tianma_of_match);

static struct mipi_dsi_driver tianma_driver = {
	.probe = tianma_probe,
	.remove = tianma_remove,
	.shutdown = tianma_panel_shutdown,
	.driver = {
		.name = "ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_tianma",
		.owner = THIS_MODULE,
		.of_match_table = tianma_of_match,
	},
};

module_mipi_dsi_driver(tianma_driver);

MODULE_AUTHOR("samir.liu <samir.liu@mediatek.com>");
MODULE_DESCRIPTION("wt tianma ili7807s vdo Panel Driver");
MODULE_LICENSE("GPL v2");
