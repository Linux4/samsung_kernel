/* drivers/gpu/drm/panel/mcd_panel/mcd_panel_adapter_helper.c
 *
 * Samsung SoC display driver.
 *
 * Copyright (c) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/sort.h>
#include <linux/module.h>
#include <drm/drm_modes.h>
#include <drm/drm_dsc.h>

#include "mcd_panel_adapter_helper.h"

static int compare_mtk_panel_mode(const void *a, const void *b)
{
	int v1, v2;

	/* compare width_mm */
	v1 = ((struct mtk_panel_mode *)a)->mode.width_mm;
	v2 = ((struct mtk_panel_mode *)b)->mode.width_mm;
	if (v1 != v2)
		return v2 - v1;

	/* compare height_mm */
	v1 = ((struct mtk_panel_mode *)a)->mode.height_mm;
	v2 = ((struct mtk_panel_mode *)b)->mode.height_mm;
	if (v1 != v2)
		return v2 - v1;

	/* compare horizontal display */
	v1 = ((struct mtk_panel_mode *)a)->mode.hdisplay;
	v2 = ((struct mtk_panel_mode *)b)->mode.hdisplay;
	if (v1 != v2)
		return v2 - v1;

	/* compare vertical display */
	v1 = ((struct mtk_panel_mode *)a)->mode.vdisplay;
	v2 = ((struct mtk_panel_mode *)b)->mode.vdisplay;
	if (v1 != v2)
		return v2 - v1;

	/* compare vertical refresh rate */
	v1 = drm_mode_vrefresh(&((struct mtk_panel_mode *)a)->mode);
	v2 = drm_mode_vrefresh(&((struct mtk_panel_mode *)b)->mode);
	if (v1 != v2)
		return v2 - v1;

	/* compare clock */
	v1 = ((struct mtk_panel_mode *)a)->mode.clock;
	v2 = ((struct mtk_panel_mode *)b)->mode.clock;
	if (v1 != v2)
		return v2 - v1;

	/* compare vtotal */
	v1 = ((struct mtk_panel_mode *)a)->mode.vtotal;
	v2 = ((struct mtk_panel_mode *)b)->mode.vtotal;
	if (v1 != v2)
		return v2 - v1;

	/* compare vscan */
	v1 = ((struct mtk_panel_mode *)a)->mode.vscan;
	v2 = ((struct mtk_panel_mode *)b)->mode.vscan;
	if (v1 != v2)
		return v2 - v1;

	return 0;
}

static int mtk_panel_mode_snprintf(const struct mtk_panel_mode *mode, char *buf, size_t size)
{
	if (!mode || !buf || !size)
		return 0;

	return snprintf(buf, size, "Exynos Panel Modeline (DRM)" DRM_MODE_FMT " (MTK_EXT)" MTK_MODE_FMT "\n",
			DRM_MODE_ARG(&mode->mode), MTK_MODE_ARG(&mode->mtk_ext));
}

void mtk_panel_mode_debug_printmodeline(const struct mtk_panel_mode *mode)
{
	char buf[128];

	mtk_panel_mode_snprintf(mode, buf, sizeof(buf));
	pr_debug("%s", buf);
}

void mtk_panel_mode_info_printmodeline(const struct mtk_panel_mode *mode)
{
	char buf[128];

	mtk_panel_mode_snprintf(mode, buf, sizeof(buf));
	pr_info("%s", buf);
}

struct mtk_panel_desc *mtk_panel_desc_create(struct mtk_panel *ctx)
{
	struct mtk_panel_desc *ndesc;

	ndesc = kzalloc(sizeof(struct mtk_panel_desc), GFP_KERNEL);
	if (!ndesc)
		return NULL;

	return ndesc;
}
EXPORT_SYMBOL(mtk_panel_desc_create);


/**
 * mtk_panel_desc_destroy - remove a desc
 * @ctx: mtk_panel
 * @desc: desc to remove
 *
 * Release @desc's unique ID, then free it @desc structure itself using kfree.
 */
void mtk_panel_desc_destroy(struct mtk_panel *ctx, struct mtk_panel_desc *desc)
{
	if (!desc)
		return;

	kfree(desc->modes);
//	kfree(desc->lp_modes);
	kfree(desc);
}
EXPORT_SYMBOL(mtk_panel_desc_destroy);

/**
 * exynos_drm_mode_set_name - set the name on a mode
 * @mode: name will be set in this mode
 * @refresh_mode : extension of exynos_drm_mode for mcd-panel (e.g. ns, hs, phs)
 *
 * Set the name of @mode to a exynos drm format which is
 * <hdisplay>x<vdisplay>@<vrefresh><refreshmode>.
 */
void exynos_drm_mode_set_name(struct drm_display_mode *mode, int refresh_mode)
{
	drm_mode_set_name(mode);

	snprintf(mode->name + strlen(mode->name),
			DRM_DISPLAY_MODE_LEN - strlen(mode->name), "@%d%s",
			drm_mode_vrefresh(mode), refresh_mode_to_str(refresh_mode));
}

int drm_display_mode_from_panel_display_mode(struct panel_display_mode *pdm, struct drm_display_mode *ddm)
{
	int vscan;

	if (!pdm | !ddm)
		return -EINVAL;

	ddm->hdisplay = pdm->width;
	ddm->hsync_start = ddm->hdisplay + pdm->panel_hporch[PANEL_PORCH_HFP];
	ddm->hsync_end = ddm->hsync_start + pdm->panel_hporch[PANEL_PORCH_HSA];
	ddm->htotal = ddm->hsync_end + pdm->panel_hporch[PANEL_PORCH_HBP];

	ddm->vdisplay = pdm->height;
	ddm->vsync_start = ddm->vdisplay + pdm->panel_vporch[PANEL_PORCH_VFP];
	ddm->vsync_end = ddm->vsync_start + pdm->panel_vporch[PANEL_PORCH_VSA];
	ddm->vtotal = ddm->vsync_end + pdm->panel_vporch[PANEL_PORCH_VBP];

	/*
	 * 'vscan' decide how many times real display(e.g. AMOLED-PANEL)
	 * vertical scan while one frame update.
	 */
	vscan = panel_mode_vscan(pdm);
	if (vscan > 1)
		ddm->vscan = vscan;

	ddm->clock = ddm->htotal * ddm->vtotal * pdm->refresh_rate * vscan / 1000;

	ddm->type = DRM_MODE_TYPE_DRIVER;

	exynos_drm_mode_set_name(ddm, pdm->refresh_mode);

	return 0;
}

struct panel_display_mode *mtk_panel_find_panel_mode(
		struct panel_display_modes *pdms, const struct drm_display_mode *pmode)
{
	struct drm_display_mode t_pmode;
	struct panel_display_mode *pdm = NULL;
	int i, ret;

	for (i = 0; i < pdms->num_modes; i++) {
		memset(&t_pmode, 0, sizeof(t_pmode));
		ret = drm_display_mode_from_panel_display_mode(pdms->modes[i], &t_pmode);
		if (ret < 0)
			continue;

		if (!strcmp(t_pmode.name, pmode->name)) {
			pdm = pdms->modes[i];
			break;
		}
	}

	return pdm;
}
EXPORT_SYMBOL(mtk_panel_find_panel_mode);

#define PPS_B(v0, v0_start, v0_end) \
	((GENMASK(v0_start, v0_end) & v0) >> v0_end)

#define PPS_M(v1, v1_start, v1_end, v0, v0_start, v0_end) \
	((PPS_B(v1, v1_start, v1_end) << DSC_PPS_MSB_SHIFT) |\
	 PPS_B(v0, v0_start, v0_end))

#define PRINT_MTK_PPS(str) pr_info("%s = %d\n", #str, str)

static int get_dsc_params_from_pps_table(u8 *pps, struct mtk_panel_dsc_params *dsc_params)
{
	dsc_params->ver = pps[0];

	if (PPS_B(pps[3], 7, 4) == 8)
		dsc_params->dsc_cfg = 34;
	else if (PPS_B(pps[3], 7, 4) == 10)
		dsc_params->dsc_cfg = 40;
	else
		pr_err("%s: can't define dsc_params.dsc_cfg\n", __func__);

	dsc_params->rct_on =			PPS_B(pps[4], 4, 4);
	dsc_params->bit_per_channel =		PPS_B(pps[3], 7, 4);
	dsc_params->dsc_line_buf_depth =		PPS_B(pps[3], 3, 0);
	dsc_params->bp_enable =			PPS_B(pps[4], 5, 5);
	dsc_params->bit_per_pixel =		PPS_M(pps[4], 1, 0, pps[5], 7, 0);
	dsc_params->pic_height =			PPS_M(pps[6], 7, 0, pps[7], 7, 0);
	dsc_params->pic_width =			PPS_M(pps[8], 7, 0, pps[9], 7, 0);
	dsc_params->slice_height =			PPS_M(pps[10], 7, 0, pps[11], 7, 0);
	dsc_params->slice_width =			PPS_M(pps[12], 7, 0, pps[13], 7, 0);
	dsc_params->chunk_size =			PPS_M(pps[14], 7, 0, pps[15], 7, 0);
	dsc_params->xmit_delay =			PPS_M(pps[16], 1, 0, pps[17], 7, 0);
	dsc_params->dec_delay =			PPS_M(pps[18], 7, 0, pps[19], 7, 0);
	dsc_params->scale_value =			PPS_B(pps[21], 5, 0);
	dsc_params->increment_interval =		PPS_M(pps[22], 7, 0, pps[23], 7, 0);
	dsc_params->decrement_interval =		PPS_M(pps[24], 3, 0, pps[25], 7, 0);
	dsc_params->line_bpg_offset =		PPS_B(pps[27], 4, 0);
	dsc_params->nfl_bpg_offset =		PPS_M(pps[28], 7, 0, pps[29], 7, 0);
	dsc_params->slice_bpg_offset =		PPS_M(pps[30], 7, 0, pps[31], 7, 0);
	dsc_params->initial_offset =		PPS_M(pps[32], 7, 0, pps[33], 7, 0);
	dsc_params->final_offset =			PPS_M(pps[34], 7, 0, pps[35], 7, 0);
	dsc_params->flatness_minqp =		PPS_B(pps[36], 4, 0);
	dsc_params->flatness_maxqp =		PPS_B(pps[37], 4, 0);
	dsc_params->rc_model_size =		PPS_M(pps[38], 7, 0, pps[39], 7, 0);
	dsc_params->rc_edge_factor =		PPS_B(pps[40], 3, 0);
	dsc_params->rc_quant_incr_limit0 =	PPS_B(pps[41], 4, 0);
	dsc_params->rc_quant_incr_limit1 =	PPS_B(pps[42], 4, 0);
	dsc_params->rc_tgt_offset_hi =		PPS_B(pps[43], 7, 4);
	dsc_params->rc_tgt_offset_lo =		PPS_B(pps[43], 3, 0);

	return 0;
}

void print_dsc_params(struct mtk_panel_dsc_params *dsc_params)
{
	PRINT_MTK_PPS(dsc_params->enable);
	PRINT_MTK_PPS(dsc_params->ver);
	PRINT_MTK_PPS(dsc_params->slice_mode);
	PRINT_MTK_PPS(dsc_params->rgb_swap);
	PRINT_MTK_PPS(dsc_params->dsc_cfg);
	PRINT_MTK_PPS(dsc_params->rct_on);
	PRINT_MTK_PPS(dsc_params->bit_per_channel);
	PRINT_MTK_PPS(dsc_params->dsc_line_buf_depth);
	PRINT_MTK_PPS(dsc_params->bp_enable);
	PRINT_MTK_PPS(dsc_params->bit_per_pixel);
	PRINT_MTK_PPS(dsc_params->pic_height);
	PRINT_MTK_PPS(dsc_params->pic_width);
	PRINT_MTK_PPS(dsc_params->slice_height);
	PRINT_MTK_PPS(dsc_params->slice_width);
	PRINT_MTK_PPS(dsc_params->chunk_size);
	PRINT_MTK_PPS(dsc_params->xmit_delay);
	PRINT_MTK_PPS(dsc_params->dec_delay);
	PRINT_MTK_PPS(dsc_params->scale_value);
	PRINT_MTK_PPS(dsc_params->increment_interval);
	PRINT_MTK_PPS(dsc_params->decrement_interval);
	PRINT_MTK_PPS(dsc_params->line_bpg_offset);
	PRINT_MTK_PPS(dsc_params->nfl_bpg_offset);
	PRINT_MTK_PPS(dsc_params->slice_bpg_offset);
	PRINT_MTK_PPS(dsc_params->initial_offset);
	PRINT_MTK_PPS(dsc_params->final_offset);
	PRINT_MTK_PPS(dsc_params->flatness_minqp);
	PRINT_MTK_PPS(dsc_params->flatness_maxqp);
	PRINT_MTK_PPS(dsc_params->rc_model_size);
	PRINT_MTK_PPS(dsc_params->rc_edge_factor);
	PRINT_MTK_PPS(dsc_params->rc_quant_incr_limit0);
	PRINT_MTK_PPS(dsc_params->rc_quant_incr_limit1);
	PRINT_MTK_PPS(dsc_params->rc_tgt_offset_hi);
	PRINT_MTK_PPS(dsc_params->rc_tgt_offset_lo);
}

static inline int get_dsc_slice_num(struct mtk_panel_dsc_params *dsc_params)
{
	if (dsc_params->slice_width == 0) {
		pr_err("%s invalid slice_width(%d).\n", __func__, dsc_params->slice_width);
		return 0;
	}

	return dsc_params->pic_width / dsc_params->slice_width;
}

static int exynos_display_mode_get_dsc_setting_from_pps_table(struct panel_display_mode *pdm, struct mtk_panel_params *ext,
				struct mtk_panel *ctx)
{
	int ret = 0;
	static bool already_printed;

	/* How to parse : ALPS07381161 */
	ext->dsc_params.enable = pdm->dsc_en;
	if (!ext->dsc_params.enable) {
		pr_info("dsc is disabled\n");
		return 0;
	}

	ext->output_mode = MTK_PANEL_DSC_SINGLE_PORT;

	ret = get_dsc_params_from_pps_table(
			pdm->dsc_picture_parameter_set, &ext->dsc_params);

	ext->dsc_params.rgb_swap = 0;

	/* multi drop DDI has half pic_width in PPS table, but AP should set as double. */
	if (ctx->spec.multi_drop)
		ext->dsc_params.pic_width = ext->dsc_params.pic_width << 1;

	ext->dsc_params.slice_mode = get_dsc_slice_num(&ext->dsc_params) >> 1;

	/* Print out once for first display_mode's dcs param only */
	if (!already_printed) {
		print_dsc_params(&ext->dsc_params);
		already_printed = true;
	}

	return ret;
}

int  exynos_display_mode_get_disp_qos_fps(struct panel_display_mode *pdm)
{
	int disp_qos_fps = 0;
	unsigned int htotal = 0;
	unsigned int vtotal = 0;
	unsigned int vtotal_without_vfp = 0;
	unsigned int total_pixel_during_one_sec = 0;

	if (!pdm) {
		pr_err("%s pdm is null.\n", __func__);
		return -EINVAL;
	}

	htotal += pdm->width;
	htotal += pdm->panel_hporch[PANEL_PORCH_HBP];
	htotal += pdm->panel_hporch[PANEL_PORCH_HFP];
	htotal += pdm->panel_hporch[PANEL_PORCH_HSA];

	vtotal += pdm->height;
	vtotal += pdm->panel_vporch[PANEL_PORCH_VBP];
	vtotal += pdm->panel_vporch[PANEL_PORCH_VFP];
	vtotal += pdm->panel_vporch[PANEL_PORCH_VSA];

	total_pixel_during_one_sec = htotal * vtotal * pdm->refresh_rate;

	vtotal_without_vfp = (vtotal > pdm->panel_vporch[PANEL_PORCH_VFP]) ?
		(vtotal - pdm->panel_vporch[PANEL_PORCH_VFP]) : 0;

	disp_qos_fps = total_pixel_during_one_sec / (vtotal_without_vfp * htotal);

	pr_debug("%s (debug) %d %d %d %d %d\n", __func__,
		htotal, vtotal, pdm->refresh_rate, total_pixel_during_one_sec, vtotal_without_vfp);

	pr_info("%s disp_qos_fps:(%d)\n", __func__, disp_qos_fps);

	return disp_qos_fps;
}

int exynos_display_mode_from_panel_display_mode(struct panel_display_mode *pdm, struct mtk_panel_params *ext, struct mtk_panel *ctx)
{
	struct device_node *np;

	if (!pdm | !ext)
		return -EINVAL;

	np = ctx->mcd_panel_dev->ap_vendor_setting_node;
	if (!np) {
		dev_err(ctx->dev, "%s: mcd_panel ddi-node is null", __func__);
		return -EINVAL;
	}

	/* To refer drm_panel from ext panel */
	ext->drm_panel = &ctx->panel;

	ext->output_mode = MTK_PANEL_DSC_SINGLE_PORT;

	/* Todo: move to DT and parse. */

	ext->data_rate = ctx->spec.data_rate;
	ext->pll_clk = ctx->spec.data_rate >> 1;

	ext->dyn_fps.switch_en = 1;

	if (pdm->disp_qos_fps > 0)
		ext->dyn_fps.vact_timing_fps = pdm->disp_qos_fps;
	else
		ext->dyn_fps.vact_timing_fps = exynos_display_mode_get_disp_qos_fps(pdm);

	dev_info(ctx->dev, "%s: ext->dyn_fps.vact_timing_fps : %d\n", __func__, ext->dyn_fps.vact_timing_fps);

	exynos_display_mode_get_dsc_setting_from_pps_table(pdm, ext, ctx);

	return 0;
}


int mtk_panel_mode_from_panel_display_mode(struct panel_display_mode *pdm, struct mtk_panel_mode *epm, struct mtk_panel *ctx)
{
	if (!pdm | !epm)
		return -EINVAL;

	drm_display_mode_from_panel_display_mode(pdm, &epm->mode);
	exynos_display_mode_from_panel_display_mode(pdm, &epm->mtk_ext, ctx);

	return 0;
}


struct mtk_panel_desc *
mtk_panel_desc_create_from_panel_display_modes(struct mtk_panel *ctx,
		struct panel_display_modes *pdms)
{
	struct mtk_panel_desc *desc;
	struct panel_device *panel;
	struct mtk_panel_mode *modes = NULL;
	struct mtk_panel_mode *unique_modes = NULL;
	struct mtk_panel_mode *temp_mode = NULL;
	struct mtk_panel_mode *native_mode;

	int num_modes = 0, temp_num_modes = 0;
	int num_unique_modes = 0;
	int i, j;

	desc = mtk_panel_desc_create(ctx);
	if (!desc) {
		dev_err(ctx->dev, "%s: could not allocate mtk_panel_desc\n", __func__);
		return NULL;
	}

	panel = ctx->mcd_panel_dev;
	if (!panel) {
		dev_err(ctx->dev, "%s panel is null\n", __func__);
		goto modefail;
	}

	/* create mtk_panel_mode */
	unique_modes = kcalloc(pdms->num_modes, sizeof(*unique_modes), GFP_KERNEL);
	if (!unique_modes) {
		dev_err(ctx->dev, "%s: could not allocate mtk_panel_mode array\n", __func__);
		goto modefail;
	}

	temp_mode = kzalloc(sizeof(*temp_mode), GFP_KERNEL);
	if (!temp_mode) {
		dev_err(ctx->dev, "%s: could not allocate mtk_panel_mode\n", __func__);
		goto modefail;
	}

	/* store unique mtk_panel_mode */
	for (i = 0; i < pdms->num_modes; i++) {
		memset(temp_mode, 0, sizeof(*temp_mode));
		mtk_panel_mode_from_panel_display_mode(pdms->modes[i], temp_mode, ctx);

		/* push unique mtk_panel_mode */
		for (j = 0; j < num_unique_modes; j++)
			if (!memcmp(&unique_modes[j], temp_mode, sizeof(*temp_mode)))
				break;

		if (j != num_unique_modes)
			continue;

		/* copy unique mode */
		memcpy(&unique_modes[num_unique_modes++], temp_mode, sizeof(*temp_mode));

		num_modes++;
	}

	/* create modes array */
	modes = kcalloc(num_modes, sizeof(*modes), GFP_KERNEL);
	if (!modes) {
		dev_err(ctx->dev, "%s: could not allocate mtk_panel_mode array\n", __func__);
		goto modefail;
	}

	for (i = 0; i < num_unique_modes; i++) {
		memcpy(&modes[temp_num_modes++], &unique_modes[i], sizeof(struct mtk_panel_params));
	}

	/*
	 * sorting mtk_panel_mode list
	 */
	sort(modes, num_modes, sizeof(modes[0]),
			compare_mtk_panel_mode, NULL);

	/*
	 * print sorted mtk_panel_mode list
	 */
	for (i = 0; i < num_modes; i++)
		mtk_panel_mode_info_printmodeline(&modes[i]);

	/* find default mode in sorted mtk_panel_mode */
	native_mode = kzalloc(sizeof(*native_mode), GFP_KERNEL);
	mtk_panel_mode_from_panel_display_mode(pdms->modes[pdms->native_mode], native_mode, ctx);

	for (i = 0; i < num_modes; i++) {
		if (!memcmp(&modes[i], native_mode,
					sizeof(*native_mode))) {
			/* set native-mode as prefered mode */
			modes[i].mode.type |= DRM_MODE_TYPE_PREFERRED;
			ctx->current_mode = &modes[i];
			break;
		}
	}

	desc->modes = modes;
	desc->num_modes = num_modes;

	kfree(unique_modes);
	kfree(native_mode);
	kfree(temp_mode);

	return desc;

modefail:
	kfree(temp_mode);
	kfree(modes);
	kfree(unique_modes);
	kfree(desc);
	return NULL;
}
EXPORT_SYMBOL(mtk_panel_desc_create_from_panel_display_modes);

MODULE_AUTHOR("Gwanghui Lee <gwanghui.lee@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based mcd panel helper driver");
MODULE_LICENSE("GPL");
