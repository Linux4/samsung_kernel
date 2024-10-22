// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_color.h"
#include "mtk_dump.h"
#include "platform/mtk_drm_platform.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_pq_helper.h"
#include "mtk_debug.h"

#define UNUSED(expr) (void)(expr)
#define PQ_MODULE_NUM 9

#define CCORR_REG(idx) (idx * 4 + 0x80)

#define C1_OFFSET (0)
#define color_get_offset(module) (0)
#define is_color1_module(module) (0)

enum COLOR_USER_CMD {
	SET_PQPARAM = 0,
	SET_COLOR_REG,
	WRITE_REG,
	BYPASS_COLOR,
	PQ_SET_WINDOW,
};

struct mtk_disp_color_tile_overhead {
	unsigned int in_width;
	unsigned int overhead;
	unsigned int comp_overhead;
};

struct mtk_disp_color_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};

struct color_backup {
	unsigned int COLOR_CFG_MAIN;
};

struct mtk_disp_color_primary {
	struct DISP_PQ_PARAM color_param;
	int ncs_tuning_mode;
	unsigned int split_en;
	unsigned int split_window_x_start;
	unsigned int split_window_y_start;
	unsigned int split_window_x_end;
	unsigned int split_window_y_end;
	int color_bypass;
	struct DISPLAY_COLOR_REG color_reg;
	int color_reg_valid;
	unsigned int width;
	//for DISP_COLOR_TUNING
	bool legacy_color_cust;
	struct MDP_COLOR_CAP mdp_color_cap;
	struct DISP_PQ_DC_PARAM pq_dc_param;
	struct DISP_PQ_DS_PARAM pq_ds_param;
	struct DISPLAY_PQ_T color_index;
	struct color_backup color_backup;
	struct DISP_AAL_DRECOLOR_PARAM drecolor_param;
	struct mutex reg_lock;
};

/**
 * struct mtk_disp_color - DISP_COLOR driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_color {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_color_data *data;
	bool is_right_pipe;
	int path_order;
	struct mtk_ddp_comp *companion;
	struct mtk_disp_color_primary *primary_data;
	struct mtk_disp_color_tile_overhead tile_overhead;
	struct mtk_disp_color_tile_overhead_v tile_overhead_v;
	unsigned long color_dst_w;
	unsigned long color_dst_h;
	atomic_t color_is_clock_on;
	spinlock_t clock_lock;
	bool set_partial_update;
	unsigned int roi_height;
};

static inline struct mtk_disp_color *comp_to_color(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_color, ddp_comp);
}

static void ddp_color_cal_split_window(struct mtk_ddp_comp *comp,
	unsigned int *p_split_window_x, unsigned int *p_split_window_y)
{
	unsigned int split_window_x = 0xFFFF0000;
	unsigned int split_window_y = 0xFFFF0000;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary =
		color->primary_data;

	/* save to global, can be applied on following PQ param updating. */
	if (comp->mtk_crtc->is_dual_pipe) {
		if (color->color_dst_w == 0 || color->color_dst_h == 0) {
			DDPINFO("color_dst_w/h not init, return default settings\n");
		} else if (primary->split_en) {
			/* TODO: CONFIG_MTK_LCM_PHYSICAL_ROTATION other case */
			if (!color->is_right_pipe) {
				if (primary->split_window_x_start > color->color_dst_w)
					primary->split_en = 0;
				if (primary->split_window_x_start <= color->color_dst_w) {
					if (primary->split_window_x_end >= color->color_dst_w)
						split_window_x = (color->color_dst_w << 16) |
							primary->split_window_x_start;
					else
						split_window_x =
							(primary->split_window_x_end << 16) |
							primary->split_window_x_start;
					split_window_y = (primary->split_window_y_end << 16) |
						primary->split_window_y_start;
				}
			} else {
				if (primary->split_window_x_start > color->color_dst_w) {
					split_window_x =
					    ((primary->split_window_x_end - color->color_dst_w)
					     << 16) |
					    (primary->split_window_x_start - color->color_dst_w);
				} else if (primary->split_window_x_start <= color->color_dst_w &&
						primary->split_window_x_end > color->color_dst_w){
					split_window_x = ((primary->split_window_x_end -
								color->color_dst_w) << 16) | 0;
				}
				split_window_y =
				    (primary->split_window_y_end << 16) |
				    primary->split_window_y_start;

				if (primary->split_window_x_end <= color->color_dst_w)
					primary->split_en = 0;
			}
		}
	} else if (color->color_dst_w == 0 || color->color_dst_h == 0) {
		DDPINFO("g_color0_dst_w/h not init, return default settings\n");
	} else if (primary->split_en) {
		/* TODO: CONFIG_MTK_LCM_PHYSICAL_ROTATION other case */
		split_window_y =
		    (primary->split_window_y_end << 16) | primary->split_window_y_start;
		split_window_x = (primary->split_window_x_end << 16) |
			primary->split_window_x_start;
	}

	*p_split_window_x = split_window_x;
	*p_split_window_y = split_window_y;
}

bool disp_color_reg_get(struct mtk_ddp_comp *comp,
	unsigned long addr, int *value)
{
	struct mtk_disp_color *color_data = comp_to_color(comp);
	unsigned long flags;

	DDPDBG("%s @ %d......... spin_trylock_irqsave ++ ",
		__func__, __LINE__);
	if (spin_trylock_irqsave(&color_data->clock_lock, flags)) {
		DDPDBG("%s @ %d......... spin_trylock_irqsave -- ",
			__func__, __LINE__);
		*value = readl(comp->regs + addr);
		spin_unlock_irqrestore(&color_data->clock_lock, flags);
	} else {
		DDPINFO("%s @ %d......... Failed to spin_trylock_irqsave ",
			__func__, __LINE__);
	}

	return true;
}

static void ddp_color_set_window(struct mtk_ddp_comp *comp,
	struct DISP_PQ_WIN_PARAM *win_param, struct cmdq_pkt *handle)
{
	unsigned int split_window_x, split_window_y;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	/* save to global, can be applied on following PQ param updating. */
	if (win_param->split_en) {
		primary_data->split_en = 1;
		primary_data->split_window_x_start = win_param->start_x;
		primary_data->split_window_y_start = win_param->start_y;
		primary_data->split_window_x_end = win_param->end_x;
		primary_data->split_window_y_end = win_param->end_y;
	} else {
		primary_data->split_en = 0;
		primary_data->split_window_x_start = 0x0000;
		primary_data->split_window_y_start = 0x0000;
		primary_data->split_window_x_end = 0xFFFF;
		primary_data->split_window_y_end = 0xFFFF;
	}

	DDPINFO("%s: input: id[%d], en[%d], x[0x%x], y[0x%x]\n",
		__func__, comp->id, primary_data->split_en,
		((win_param->end_x << 16) | win_param->start_x),
		((win_param->end_y << 16) | win_param->start_y));

	ddp_color_cal_split_window(comp, &split_window_x, &split_window_y);

	DDPINFO("%s: current window setting: en[%d], x[0x%x], y[0x%x]",
		__func__,
		(readl(comp->regs+DISP_COLOR_DBG_CFG_MAIN)&0x00000008)>>3,
		readl(comp->regs+DISP_COLOR_WIN_X_MAIN),
		readl(comp->regs+DISP_COLOR_WIN_Y_MAIN));

	DDPINFO("%s: output: x[0x%x], y[0x%x]",
		__func__, split_window_x, split_window_y);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_DBG_CFG_MAIN,
		(primary_data->split_en << 3), 0x00000008);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_X_MAIN, split_window_x, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_Y_MAIN, split_window_y, ~0);
}

struct DISPLAY_PQ_T *get_Color_index(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	primary_data->legacy_color_cust = true;
	return &primary_data->color_index;
}

void DpEngine_COLORonInit(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	unsigned int split_window_x, split_window_y;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	ddp_color_cal_split_window(comp, &split_window_x, &split_window_y);

	DDPINFO("%s: id[%d], en[%d], x[0x%x], y[0x%x]\n",
		__func__, comp->id, primary_data->split_en, split_window_x, split_window_y);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_DBG_CFG_MAIN,
		(primary_data->split_en << 3), 0x00000008);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_X_MAIN, split_window_x, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_WIN_Y_MAIN, split_window_y, ~0);

#ifdef IF_ZERO /* enable only if irq can be handled */
	/* enable interrupt */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_INTEN(color),
		0x00000007, 0x00000007);
#endif

	/* Set 10bit->8bit Rounding */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_OUT_SEL(color), 0x333, 0x00000333);
}

void DpEngine_COLORonConfig(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	int index = 0;
	unsigned int u4Temp = 0;
	unsigned int u4SatAdjPurp, u4SatAdjSkin, u4SatAdjGrass, u4SatAdjSky;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	struct mtk_disp_color *color = comp_to_color(comp);
	struct DISP_PQ_PARAM *pq_param_p = &primary_data->color_param;
	int i, j, reg_index;
	unsigned int pq_index;
	int wide_gamut_en = 0;
	/* unmask s_gain_by_y lsp when drecolor enable */
	int drecolor_sel = primary_data->drecolor_param.drecolor_sel;
	unsigned int drecolor_unmask = ~((drecolor_sel << 15) | (drecolor_sel << 20));

	if (pq_param_p->u4Brightness >= BRIGHTNESS_SIZE ||
		pq_param_p->u4Contrast >= CONTRAST_SIZE ||
		pq_param_p->u4SatGain >= GLOBAL_SAT_SIZE ||
		pq_param_p->u4HueAdj[PURP_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4HueAdj[SKIN_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4HueAdj[GRASS_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4HueAdj[SKY_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4SatAdj[PURP_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4SatAdj[SKIN_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4SatAdj[GRASS_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4SatAdj[SKY_TONE] >= COLOR_TUNING_INDEX ||
		pq_param_p->u4ColorLUT >= COLOR_3D_CNT) {
		DRM_ERROR("[PQ][COLOR] Tuning index range error !\n");
		return;
	}

	if (primary_data->color_bypass == 0) {
		if (color->data->support_color21 == true) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CFG_MAIN,
				(1 << 21)
				| (primary_data->color_index.LSP_EN << 20)
				| (primary_data->color_index.S_GAIN_BY_Y_EN << 15)
				| (wide_gamut_en << 8)
				| (0 << 7),
				0x003081FF & drecolor_unmask);
		} else {
			/* disable wide_gamut */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CFG_MAIN,
				(0 << 8) | (0 << 7), 0x00001FF);
		}

		/* color start */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x3);

		/* enable R2Y/Y2R in Color Wrapper */
		if (color->data->support_color21 == true) {
			/* RDMA & OVL will enable wide-gamut function */
			/* disable rgb clipping function in CM1 */
			/* to keep the wide-gamut range */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CM1_EN(color),
				0x03, 0x03);
		} else {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CM1_EN(color),
				0x03, 0x03);
		}

		/* also set no rounding on Y2R */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM2_EN(color), 0x01, 0x01);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x1);
	}

	/* for partial Y contour issue */
	if (wide_gamut_en == 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x40, 0x7F);
	else if (wide_gamut_en == 1)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x0, 0x7F);

	/* config parameter from customer color_index.h */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_1,
		(primary_data->color_index.BRIGHTNESS[pq_param_p->u4Brightness] << 16) |
		primary_data->color_index.CONTRAST[pq_param_p->u4Contrast], 0x07FF03FF);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_2,
		primary_data->color_index.GLOBAL_SAT[pq_param_p->u4SatGain], 0x000003FF);

	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index,
			(primary_data->color_index.PARTIAL_Y
				[pq_param_p->u4PartialY][2 * index] |
			 primary_data->color_index.PARTIAL_Y
				[pq_param_p->u4PartialY][2 * index + 1]
			 << 16), 0x00FF00FF);
	}

	if (color->data->support_color21 == false)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN,
			0 << 13, 0x00002000);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN_2,
			0x40 << 24,	0xFF000000);

	/* Partial Saturation Function */
	u4SatAdjPurp = pq_param_p->u4SatAdj[PURP_TONE];
	u4SatAdjSkin = pq_param_p->u4SatAdj[SKIN_TONE];
	u4SatAdjGrass = pq_param_p->u4SatAdj[GRASS_TONE];
	u4SatAdjSky = pq_param_p->u4SatAdj[SKY_TONE];

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG1][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG1][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG1][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG1][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG1][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG1][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG1][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG2][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG2][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG2][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG2][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG2][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG2][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG2][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG2][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG3][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG3][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SG3][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SG3][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SG3][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG3][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG3][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SG3][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP1][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP1][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP1][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP1][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP1][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP1][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP1][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_0,
		(primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP2][0] |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP2][1] << 8 |
		primary_data->color_index.PURP_TONE_S[u4SatAdjPurp][SP2][2] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_1,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][1] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][2] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][3] << 16 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_2,
		(primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][5] |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][6] << 8 |
		primary_data->color_index.SKIN_TONE_S[u4SatAdjSkin][SP2][7] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_3,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][1] |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][2] << 8 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][3] << 16 |
		primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][4] << 24), ~0);


	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_4,
		(primary_data->color_index.GRASS_TONE_S[u4SatAdjGrass][SP2][5] |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP2][0] << 8 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP2][1] << 16 |
		primary_data->color_index.SKY_TONE_S[u4SatAdjSky][SP2][2] << 24), ~0);

	for (index = 0; index < 3; index++) {
		u4Temp = pq_param_p->u4HueAdj[PURP_TONE];
		h_series[index + PURP_TONE_START] =
			primary_data->color_index.PURP_TONE_H[u4Temp][index];
	}

	for (index = 0; index < 8; index++) {
		u4Temp = pq_param_p->u4HueAdj[SKIN_TONE];
		h_series[index + SKIN_TONE_START] =
		    primary_data->color_index.SKIN_TONE_H[u4Temp][index];
	}

	for (index = 0; index < 6; index++) {
		u4Temp = pq_param_p->u4HueAdj[GRASS_TONE];
		h_series[index + GRASS_TONE_START] =
			primary_data->color_index.GRASS_TONE_H[u4Temp][index];
	}

	for (index = 0; index < 3; index++) {
		u4Temp = pq_param_p->u4HueAdj[SKY_TONE];
		h_series[index + SKY_TONE_START] =
		    primary_data->color_index.SKY_TONE_H[u4Temp][index];
	}

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) +
		    (h_series[4 * index + 1] << 8) +
		    (h_series[4 * index + 2] << 16) +
		    (h_series[4 * index + 3] << 24);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LOCAL_HUE_CD_0 + 4 * index,
			u4Temp, ~0);
	}

	if (color->data->support_color21 == true) {
		/* S Gain By Y */
		u4Temp = 0;

		reg_index = 0;
		for (i = 0; i < S_GAIN_BY_Y_CONTROL_CNT && !drecolor_sel; i++) {
			for (j = 0; j < S_GAIN_BY_Y_HUE_PHASE_CNT; j += 4) {
				u4Temp = (primary_data->color_index.S_GAIN_BY_Y[i][j]) +
					(primary_data->color_index.S_GAIN_BY_Y[i][j + 1]
					<< 8) +
					(primary_data->color_index.S_GAIN_BY_Y[i][j + 2]
					<< 16) +
					(primary_data->color_index.S_GAIN_BY_Y[i][j + 3]
					<< 24);

				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa +
					DISP_COLOR_S_GAIN_BY_Y0_0 + reg_index,
					u4Temp, ~0);
				reg_index += 4;
			}
		}
		if (!drecolor_sel) {
			/* LSP */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_LSP_1,
				(primary_data->color_index.LSP[3] << 0) |
				(primary_data->color_index.LSP[2] << 7) |
				(primary_data->color_index.LSP[1] << 14) |
				(primary_data->color_index.LSP[0] << 22), 0x1FFFFFFF);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_LSP_2,
				(primary_data->color_index.LSP[7] << 0) |
				(primary_data->color_index.LSP[6] << 8) |
				(primary_data->color_index.LSP[5] << 16) |
				(primary_data->color_index.LSP[4] << 23), 0x3FFF7F7F);
		}
	}

	/* color window */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_TWO_D_WINDOW_1,
		color->data->color_window, ~0);

	if (color->data->support_color30 == true) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM_CONTROL,
			0x0 |
			(0x3 << 1) |	/* enable window 1 */
			(0x3 << 4) |	/* enable window 2 */
			(0x3 << 7)		/* enable window 3 */
			, 0x1B7);

		pq_index = pq_param_p->u4ColorLUT;
		for (i = 0; i < WIN_TOTAL; i++) {
			reg_index = i * 4 * (LUT_TOTAL * 5);
			for (j = 0; j < LUT_TOTAL; j++) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa +
						DISP_COLOR_CM_W1_HUE_0 +
						reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_L] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_U] << 10) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT0] << 20),
						~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_1
						+ reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT1] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT2] << 10) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT3] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_2
						+ reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_POINT4] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE0] << 10) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE1] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_3
						+ reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE2] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE3] << 8) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE4] << 16) |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_SLOPE5] << 24),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_4
						+ reg_index,
					primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_WGT_LSLOPE] |
					(primary_data->color_index.COLOR_3D[pq_index]
					[i][j*LUT_REG_TOTAL+REG_WGT_USLOPE]
					<< 16),	~0);

				reg_index += (4 * 5);
			}
		}
	}
}

static void disp_color_write_drecolor_hw_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_AAL_DRECOLOR_REG *param)
{
	int i, cnt = DRECOLOR_SGY_Y_ENTRY * DRECOLOR_SGY_HUE_NUM / 4;
	uint32_t value = 0, mask = 0;
	unsigned int *sgy_data = (unsigned int *)param->sgy_out_gain;

	SET_VAL_MASK(value, mask, param->sgy_en, FLD_S_GAIN_BY_Y_EN);
	SET_VAL_MASK(value, mask, param->lsp_en, FLD_LSP_EN);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_CFG_MAIN, value, mask);

	/* SGY */
	for (i = 0; i < cnt; i++) {
		value = sgy_data[4 * i] |
			(sgy_data[4 * i + 1]  << 8) |
			(sgy_data[4 * i + 2] << 16) |
			(sgy_data[4 * i + 3] << 24);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_S_GAIN_BY_Y0_0 + i * 4, value, ~0);
	}

	/* LSP */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_LSP_1,
		(param->lsp_out_setting[3] << 0) |
		(param->lsp_out_setting[2] << 7) |
		(param->lsp_out_setting[1] << 14) |
		(param->lsp_out_setting[0] << 22), 0x1FFFFFFF);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_LSP_2,
		(param->lsp_out_setting[7] << 0) |
		(param->lsp_out_setting[6] << 8) |
		(param->lsp_out_setting[5] << 16) |
		(param->lsp_out_setting[4] << 23), 0x3FFF7F7F);
}

static void color_write_hw_reg(struct mtk_ddp_comp *comp,
	const struct DISPLAY_COLOR_REG *color_reg, struct cmdq_pkt *handle)
{
	int index = 0;
	unsigned char h_series[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned int u4Temp = 0;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	int i, j, reg_index;
	int wide_gamut_en = 0;
	/* unmask s_gain_by_y lsp when drecolor enable */
	int drecolor_sel = primary_data->drecolor_param.drecolor_sel;
	unsigned int drecolor_unmask = ~((drecolor_sel << 15) | (drecolor_sel << 20));

	DDPINFO("%s,SET COLOR REG id(%d) drecolor_sel %d\n", __func__, comp->id, drecolor_sel);

	if (primary_data->color_bypass == 0) {
		if (color->data->support_color21 == true) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CFG_MAIN,
				(1 << 21)
				| (color_reg->LSP_EN << 20)
				| (color_reg->S_GAIN_BY_Y_EN << 15)
				| (wide_gamut_en << 8)
				| (0 << 7),
				0x003081FF | drecolor_unmask);
		} else {
			/* disable wide_gamut */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CFG_MAIN,
				(0 << 8) | (0 << 7), 0x00001FF);
		}

		/* color start */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x3);

		/* enable R2Y/Y2R in Color Wrapper */
		if (color->data->support_color21 == true) {
			/* RDMA & OVL will enable wide-gamut function */
			/* disable rgb clipping function in CM1 */
			/* to keep the wide-gamut range */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CM1_EN(color),
				0x03, 0x03);
		} else {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_CM1_EN(color),
				0x03, 0x03);
		}

		/* also set no rounding on Y2R */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM2_EN(color), 0x01, 0x01);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_START(color), 0x1, 0x1);
	}

	/* for partial Y contour issue */
	if (wide_gamut_en == 0)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x40, 0x7F);
	else if (wide_gamut_en == 1)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LUMA_ADJ, 0x0, 0x7F);

	/* config parameter from customer color_index.h */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_1,
		(color_reg->BRIGHTNESS << 16) | color_reg->CONTRAST,
		0x07FF03FF);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_G_PIC_ADJ_MAIN_2,
		color_reg->GLOBAL_SAT, 0x000003FF);

	/* Partial Y Function */
	for (index = 0; index < 8; index++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_Y_SLOPE_1_0_MAIN + 4 * index,
			(color_reg->PARTIAL_Y[2 * index] |
			 color_reg->PARTIAL_Y[2 * index + 1] << 16),
			 0x00FF00FF);
	}

	if (color->data->support_color21 == false)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN,
			0 << 13, 0x00002000);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_C_BOOST_MAIN_2,
			0x40 << 24,	0xFF000000);

	/* Partial Saturation Function */

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_0,
		(color_reg->PURP_TONE_S[SG1][0] |
		color_reg->PURP_TONE_S[SG1][1] << 8 |
		color_reg->PURP_TONE_S[SG1][2] << 16 |
		color_reg->SKIN_TONE_S[SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_1,
		(color_reg->SKIN_TONE_S[SG1][1] |
		color_reg->SKIN_TONE_S[SG1][2] << 8 |
		color_reg->SKIN_TONE_S[SG1][3] << 16 |
		color_reg->SKIN_TONE_S[SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_2,
		(color_reg->SKIN_TONE_S[SG1][5] |
		color_reg->SKIN_TONE_S[SG1][6] << 8 |
		color_reg->SKIN_TONE_S[SG1][7] << 16 |
		color_reg->GRASS_TONE_S[SG1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_3,
		(color_reg->GRASS_TONE_S[SG1][1] |
		color_reg->GRASS_TONE_S[SG1][2] << 8 |
		color_reg->GRASS_TONE_S[SG1][3] << 16 |
		color_reg->GRASS_TONE_S[SG1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN1_4,
		(color_reg->GRASS_TONE_S[SG1][5] |
		color_reg->SKY_TONE_S[SG1][0] << 8 |
		color_reg->SKY_TONE_S[SG1][1] << 16 |
		color_reg->SKY_TONE_S[SG1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_0,
		(color_reg->PURP_TONE_S[SG2][0] |
		color_reg->PURP_TONE_S[SG2][1] << 8 |
		color_reg->PURP_TONE_S[SG2][2] << 16 |
		color_reg->SKIN_TONE_S[SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_1,
		(color_reg->SKIN_TONE_S[SG2][1] |
		color_reg->SKIN_TONE_S[SG2][2] << 8 |
		color_reg->SKIN_TONE_S[SG2][3] << 16 |
		color_reg->SKIN_TONE_S[SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_2,
		(color_reg->SKIN_TONE_S[SG2][5] |
		color_reg->SKIN_TONE_S[SG2][6] << 8 |
		color_reg->SKIN_TONE_S[SG2][7] << 16 |
		color_reg->GRASS_TONE_S[SG2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_3,
		(color_reg->GRASS_TONE_S[SG2][1] |
		color_reg->GRASS_TONE_S[SG2][2] << 8 |
		color_reg->GRASS_TONE_S[SG2][3] << 16 |
		color_reg->GRASS_TONE_S[SG2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN2_4,
		(color_reg->GRASS_TONE_S[SG2][5] |
		color_reg->SKY_TONE_S[SG2][0] << 8 |
		color_reg->SKY_TONE_S[SG2][1] << 16 |
		color_reg->SKY_TONE_S[SG2][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_0,
		(color_reg->PURP_TONE_S[SG3][0] |
		color_reg->PURP_TONE_S[SG3][1] << 8 |
		color_reg->PURP_TONE_S[SG3][2] << 16 |
		color_reg->SKIN_TONE_S[SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_1,
		(color_reg->SKIN_TONE_S[SG3][1] |
		color_reg->SKIN_TONE_S[SG3][2] << 8 |
		color_reg->SKIN_TONE_S[SG3][3] << 16 |
		color_reg->SKIN_TONE_S[SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_2,
		(color_reg->SKIN_TONE_S[SG3][5] |
		color_reg->SKIN_TONE_S[SG3][6] << 8 |
		color_reg->SKIN_TONE_S[SG3][7] << 16 |
		color_reg->GRASS_TONE_S[SG3][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_3,
		(color_reg->GRASS_TONE_S[SG3][1] |
		color_reg->GRASS_TONE_S[SG3][2] << 8 |
		color_reg->GRASS_TONE_S[SG3][3] << 16 |
		color_reg->GRASS_TONE_S[SG3][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_GAIN3_4,
		(color_reg->GRASS_TONE_S[SG3][5] |
		color_reg->SKY_TONE_S[SG3][0] << 8 |
		color_reg->SKY_TONE_S[SG3][1] << 16 |
		color_reg->SKY_TONE_S[SG3][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_0,
		(color_reg->PURP_TONE_S[SP1][0] |
		color_reg->PURP_TONE_S[SP1][1] << 8 |
		color_reg->PURP_TONE_S[SP1][2] << 16 |
		color_reg->SKIN_TONE_S[SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_1,
		(color_reg->SKIN_TONE_S[SP1][1] |
		color_reg->SKIN_TONE_S[SP1][2] << 8 |
		color_reg->SKIN_TONE_S[SP1][3] << 16 |
		color_reg->SKIN_TONE_S[SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_2,
		(color_reg->SKIN_TONE_S[SP1][5] |
		color_reg->SKIN_TONE_S[SP1][6] << 8 |
		color_reg->SKIN_TONE_S[SP1][7] << 16 |
		color_reg->GRASS_TONE_S[SP1][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_3,
		(color_reg->GRASS_TONE_S[SP1][1] |
		color_reg->GRASS_TONE_S[SP1][2] << 8 |
		color_reg->GRASS_TONE_S[SP1][3] << 16 |
		color_reg->GRASS_TONE_S[SP1][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT1_4,
		(color_reg->GRASS_TONE_S[SP1][5] |
		color_reg->SKY_TONE_S[SP1][0] << 8 |
		color_reg->SKY_TONE_S[SP1][1] << 16 |
		color_reg->SKY_TONE_S[SP1][2] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_0,
		(color_reg->PURP_TONE_S[SP2][0] |
		color_reg->PURP_TONE_S[SP2][1] << 8 |
		color_reg->PURP_TONE_S[SP2][2] << 16 |
		color_reg->SKIN_TONE_S[SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_1,
		(color_reg->SKIN_TONE_S[SP2][1] |
		color_reg->SKIN_TONE_S[SP2][2] << 8 |
		color_reg->SKIN_TONE_S[SP2][3] << 16 |
		color_reg->SKIN_TONE_S[SP2][4] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_2,
		(color_reg->SKIN_TONE_S[SP2][5] |
		color_reg->SKIN_TONE_S[SP2][6] << 8 |
		color_reg->SKIN_TONE_S[SP2][7] << 16 |
		color_reg->GRASS_TONE_S[SP2][0] << 24), ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_3,
		(color_reg->GRASS_TONE_S[SP2][1] |
		color_reg->GRASS_TONE_S[SP2][2] << 8 |
		color_reg->GRASS_TONE_S[SP2][3] << 16 |
		color_reg->GRASS_TONE_S[SP2][4] << 24), ~0);


	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_PART_SAT_POINT2_4,
		(color_reg->GRASS_TONE_S[SP2][5] |
		color_reg->SKY_TONE_S[SP2][0] << 8 |
		color_reg->SKY_TONE_S[SP2][1] << 16 |
		color_reg->SKY_TONE_S[SP2][2] << 24), ~0);

	for (index = 0; index < 3; index++) {
		h_series[index + PURP_TONE_START] =
			color_reg->PURP_TONE_H[index];
	}

	for (index = 0; index < 8; index++) {
		h_series[index + SKIN_TONE_START] =
		    color_reg->SKIN_TONE_H[index];
	}

	for (index = 0; index < 6; index++) {
		h_series[index + GRASS_TONE_START] =
			color_reg->GRASS_TONE_H[index];
	}

	for (index = 0; index < 3; index++) {
		h_series[index + SKY_TONE_START] =
		    color_reg->SKY_TONE_H[index];
	}

	for (index = 0; index < 5; index++) {
		u4Temp = (h_series[4 * index]) +
		    (h_series[4 * index + 1] << 8) +
		    (h_series[4 * index + 2] << 16) +
		    (h_series[4 * index + 3] << 24);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_LOCAL_HUE_CD_0 + 4 * index,
			u4Temp, ~0);
	}

	if (color->data->support_color21 == true) {
		/* S Gain By Y */
		u4Temp = 0;

		reg_index = 0;
		for (i = 0; i < S_GAIN_BY_Y_CONTROL_CNT && !drecolor_sel; i++) {
			for (j = 0; j < S_GAIN_BY_Y_HUE_PHASE_CNT; j += 4) {
				u4Temp = (color_reg->S_GAIN_BY_Y[i][j]) +
					(color_reg->S_GAIN_BY_Y[i][j + 1]
					<< 8) +
					(color_reg->S_GAIN_BY_Y[i][j + 2]
					<< 16) +
					(color_reg->S_GAIN_BY_Y[i][j + 3]
					<< 24);

				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa +
					DISP_COLOR_S_GAIN_BY_Y0_0 +
					reg_index,
					u4Temp, ~0);
				reg_index += 4;
			}
		}
		if (!drecolor_sel) {
			/* LSP */
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_LSP_1,
				(primary_data->color_index.LSP[3] << 0) |
				(primary_data->color_index.LSP[2] << 7) |
				(primary_data->color_index.LSP[1] << 14) |
				(primary_data->color_index.LSP[0] << 22), 0x1FFFFFFF);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_COLOR_LSP_2,
				(primary_data->color_index.LSP[7] << 0) |
				(primary_data->color_index.LSP[6] << 8) |
				(primary_data->color_index.LSP[5] << 16) |
				(primary_data->color_index.LSP[4] << 23), 0x3FFF7F7F);
		}
	}

	/* color window */
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_COLOR_TWO_D_WINDOW_1,
		color->data->color_window, ~0);

	if (color->data->support_color30 == true) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CM_CONTROL,
			0x0 |
			(0x3 << 1) |	/* enable window 1 */
			(0x3 << 4) |	/* enable window 2 */
			(0x3 << 7)		/* enable window 3 */
			, 0x1B7);

		for (i = 0; i < WIN_TOTAL; i++) {
			reg_index = i * 4 * (LUT_TOTAL * 5);
			for (j = 0; j < LUT_TOTAL; j++) {
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_0 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_L] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_U] << 10) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT0] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_1 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT1] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT2] << 10) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT3] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_2 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_POINT4] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE0] << 10) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE1] << 20),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_3 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE2] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE3] << 8) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE4] << 16) |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_SLOPE5] << 24),
					~0);
				cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_CM_W1_HUE_4 +
					reg_index,
					color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_WGT_LSLOPE] |
					(color_reg->COLOR_3D[i]
					[j*LUT_REG_TOTAL+REG_WGT_USLOPE] << 16),
					~0);

				reg_index += (4 * 5);
			}
		}
	}
}

static void mtk_disp_color_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	DDPINFO("line: %d\n", __LINE__);
	if (cfg->tile_overhead.is_support) {
		if (!color->is_right_pipe) {
			color->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				color->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width +=
				color->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			color->tile_overhead.in_width =
				cfg->tile_overhead.left_in_width;
			color->tile_overhead.overhead =
				cfg->tile_overhead.left_overhead;
		} else {
			/*set component overhead*/
			color->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				color->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				color->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			color->tile_overhead.in_width =
				cfg->tile_overhead.right_in_width;
			color->tile_overhead.overhead =
				cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_disp_color_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	color->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		color->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	color->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void mtk_color_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	unsigned int width;
	unsigned int overhead_v;

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support)
		width = color->tile_overhead.in_width;
	else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;
	}

	if (comp->mtk_crtc->is_dual_pipe) {
		primary_data->width = width;
	}

	if (comp->mtk_crtc->is_dual_pipe)
		color->color_dst_w = cfg->w / 2;
	else
		color->color_dst_w = cfg->w;
	color->color_dst_h = cfg->h;

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_COLOR_WIDTH(color), width, ~0);
	if (!color->set_partial_update)
		cmdq_pkt_write(handle, comp->cmdq_base,
					comp->regs_pa + DISP_COLOR_HEIGHT(color), cfg->h, ~0);
	else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : color->tile_overhead_v.overhead_v;
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_HEIGHT(color), color->roi_height + overhead_v * 2, ~0);
	}
	// set color_8bit_switch register
	if (cfg->source_bpc == 8)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN, (0x1 << 25), (0x1 << 25));
	else if (cfg->source_bpc == 10)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN, (0x0 << 25), (0x1 << 25));
	else
		DDPINFO("Disp COLOR's bit is : %u\n", cfg->bpc);
}


int mtk_drm_color_cfg_set_pqparam(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct DISP_PQ_PARAM *pq_param;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	pq_param = &primary_data->color_param;
	memcpy(pq_param, (struct DISP_PQ_PARAM *)data,
		sizeof(struct DISP_PQ_PARAM));

	if (primary_data->ncs_tuning_mode == 0) {
		/* normal mode */
		/* normal mode */
		DpEngine_COLORonInit(comp, handle);
		DpEngine_COLORonConfig(comp, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_color1 = color->companion;

			DpEngine_COLORonInit(comp_color1, handle);
			DpEngine_COLORonConfig(comp_color1, handle);
		}

		DDPINFO("SET_PQ_PARAM\n");
	} else {
		/* ncs_tuning_mode = 0; */
		ret = -EINVAL;
		DDPINFO("SET_PQ_PARAM, bypassed by ncs_tuning_mode = 1\n");
	}

	return ret;

}

int mtk_drm_ioctl_set_pqparam_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	struct DISP_PQ_PARAM *pq_param;

	pq_param = &primary_data->color_param;
	memcpy(pq_param, (struct DISP_PQ_PARAM *)data,
		sizeof(struct DISP_PQ_PARAM));

	if (primary_data->ncs_tuning_mode == 0) {
		/* normal mode */
		ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp, SET_PQPARAM, data);
		mtk_crtc_check_trigger(mtk_crtc, true, true);

		DDPINFO("SET_PQ_PARAM\n");
	} else {
		/* ncs_tuning_mode = 0; */
		DDPINFO
		 ("SET_PQ_PARAM, bypassed by ncs_tuning_mode = 1\n");
	}

	return ret;
}

int mtk_drm_ioctl_set_pqparam(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_set_pqparam_impl(comp, data);
}

int mtk_drm_color_cfg_set_pqindex(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct DISPLAY_PQ_T *pq_index;

	pq_index = get_Color_index(comp);
	memcpy(pq_index, (struct DISPLAY_PQ_T *)data,
		sizeof(struct DISPLAY_PQ_T));

	return ret;
}

int mtk_drm_ioctl_set_pqindex_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct DISPLAY_PQ_T *pq_index;

	DDPINFO("%s...", __func__);

	pq_index = get_Color_index(comp);
	memcpy(pq_index, (struct DISPLAY_PQ_T *)data,
		sizeof(struct DISPLAY_PQ_T));

	return ret;
}

int mtk_drm_ioctl_set_pqindex(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_set_pqindex_impl(comp, data);
}

int mtk_color_cfg_set_color_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	DDPINFO("%s,SET COLOR REG id(%d)\n", __func__, comp->id);

	if (data != NULL) {
		memcpy(&primary_data->color_reg, (struct DISPLAY_COLOR_REG *)data,
			sizeof(struct DISPLAY_COLOR_REG));

		color_write_hw_reg(comp, &primary_data->color_reg, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_color1 = color->companion;

			DDPINFO("%s,SET COLOR REG id(%d)\n", __func__, comp_color1->id);
			color_write_hw_reg(comp_color1, &primary_data->color_reg, handle);
		}
	} else {
		ret = -EINVAL;
		DDPINFO("%s: data is NULL", __func__);
	}
	primary_data->color_reg_valid = 1;

	return ret;
}

int mtk_drm_ioctl_set_color_reg_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	int ret;

	ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp, SET_COLOR_REG, data);
	mtk_crtc_check_trigger(mtk_crtc, true, true);

	return ret;
}

int mtk_drm_ioctl_set_color_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_set_color_reg_impl(comp, data);
}

int mtk_color_cfg_mutex_control(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;
	int ret = 0;
	unsigned int *value = data;

	DDPINFO("%s...value:%d", __func__, *value);

	if (*value == 1) {
		primary_data->ncs_tuning_mode = 1;
		DDPINFO("ncs_tuning_mode = 1\n");
	} else if (*value == 2) {
		primary_data->ncs_tuning_mode = 0;
		DDPINFO("ncs_tuning_mode = 0\n");
	} else {
		DDPPR_ERR("DISP_IOCTL_MUTEX_CONTROL invalid control\n");
		return -EFAULT;
	}

	return ret;

}

int mtk_drm_ioctl_mutex_control_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	unsigned int *value = data;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	DDPINFO("%s...value:%d", __func__, *value);

	if (*value == 1) {
		primary_data->ncs_tuning_mode = 1;
		DDPINFO("ncs_tuning_mode = 1\n");
	} else if (*value == 2) {
		primary_data->ncs_tuning_mode = 0;
		DDPINFO("ncs_tuning_mode = 0\n");
		mtk_crtc_check_trigger(mtk_crtc, true, true);
	} else {
		DDPPR_ERR("DISP_IOCTL_MUTEX_CONTROL invalid control\n");
		return -EFAULT;
	}

	return ret;
}

int mtk_drm_ioctl_mutex_control(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_mutex_control_impl(comp, data);
}

int mtk_drm_ioctl_read_sw_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_drm_ioctl_sw_read_impl(crtc, data);
}

int mtk_drm_ioctl_write_sw_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_drm_ioctl_sw_write_impl(crtc, data);
}

int mtk_drm_ioctl_read_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_drm_ioctl_hw_read_impl(crtc, data);
}

int mtk_drm_ioctl_write_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_drm_ioctl_hw_write_impl(crtc, data);
}

void mtk_color_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{

	struct mtk_disp_color *color;
	struct mtk_disp_color_primary *primary_data;

	if (comp == NULL) {
		DDPPR_ERR("%s, null pointer!", __func__);
		return;
	}
	color = comp_to_color(comp);
	primary_data = NULL;


	DDPINFO("%s: bypass: %d\n", __func__, bypass);

	primary_data = comp_to_color(comp)->primary_data;
	primary_data->color_bypass = bypass;

	if (bypass) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_COLOR_CFG_MAIN,
			       COLOR_BYPASS_ALL | COLOR_SEQ_SEL, ~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_CFG_MAIN,
			(0 << 7), 0xFF); /* resume all */
	}
}

int mtk_color_cfg_bypass(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	unsigned int *value = data;
	struct mtk_disp_color *color = comp_to_color(comp);

	mtk_color_bypass(comp, *value, handle);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_color1 = color->companion;

		mtk_color_bypass(comp_color1, *value, handle);
	}

	return ret;
}

int mtk_drm_ioctl_bypass_color_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;

	ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp, BYPASS_COLOR, data);
	mtk_crtc_check_trigger(mtk_crtc, true, true);

	return ret;
}

int mtk_drm_ioctl_bypass_color(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_bypass_color_impl(comp, data);
}

int mtk_drm_color_cfg_pq_set_window(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	struct DISP_PQ_WIN_PARAM *win_param = data;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	ddp_color_set_window(comp, win_param, handle);
	DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
		__func__, comp->id, primary_data->split_en,
		((primary_data->split_window_x_end << 16) |
		 primary_data->split_window_x_start),
		((primary_data->split_window_y_end << 16) |
		 primary_data->split_window_y_start));

	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_color1 = color->companion;

		ddp_color_set_window(comp_color1, win_param, handle);
		DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
			__func__, comp_color1->id, primary_data->split_en,
			((primary_data->split_window_x_end << 16) |
			 primary_data->split_window_x_start),
			((primary_data->split_window_y_end << 16) |
			 primary_data->split_window_y_start));
	}

	return ret;
}

int mtk_drm_ioctl_pq_set_window_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary =
		color->primary_data;
	struct DISP_PQ_WIN_PARAM *win_param = data;

	unsigned int split_window_x, split_window_y;

	/* save to global, can be applied on following PQ param updating. */
	if (win_param->split_en) {
		primary->split_en = 1;
		primary->split_window_x_start = win_param->start_x;
		primary->split_window_y_start = win_param->start_y;
		primary->split_window_x_end = win_param->end_x;
		primary->split_window_y_end = win_param->end_y;
	} else {
		primary->split_en = 0;
		primary->split_window_x_start = 0x0000;
		primary->split_window_y_start = 0x0000;
		primary->split_window_x_end = 0xFFFF;
		primary->split_window_y_end = 0xFFFF;
	}

	DDPINFO("%s: input: id[%d], en[%d], x[0x%x], y[0x%x]\n",
		__func__, comp->id, primary->split_en,
		((win_param->end_x << 16) | win_param->start_x),
		((win_param->end_y << 16) | win_param->start_y));

	ddp_color_cal_split_window(comp, &split_window_x, &split_window_y);

	DDPINFO("%s: output: x[0x%x], y[0x%x]", __func__,
		split_window_x, split_window_y);

	DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
		__func__, comp->id, primary->split_en,
		((primary->split_window_x_end << 16) | primary->split_window_x_start),
		((primary->split_window_y_end << 16) | primary->split_window_y_start));

	ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp, PQ_SET_WINDOW, data);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_color1 = color->companion;

		ddp_color_cal_split_window(comp_color1, &split_window_x, &split_window_y);
		ret = mtk_crtc_user_cmd(&mtk_crtc->base, comp_color1, PQ_SET_WINDOW, data);
		DDPINFO("%s: output: x[0x%x], y[0x%x]", __func__,
			split_window_x, split_window_y);

		DDPINFO("%s..., id=%d, en=%d, x=0x%x, y=0x%x\n",
			__func__, comp_color1->id, primary->split_en,
			((primary->split_window_x_end << 16) | primary->split_window_x_start),
			((primary->split_window_y_end << 16) | primary->split_window_y_start));
	}
	mtk_crtc_check_trigger(mtk_crtc, true, true);

	return ret;
}

int mtk_drm_ioctl_pq_set_window(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_COLOR, 0);

	return mtk_drm_ioctl_pq_set_window_impl(comp, data);
}

static void mtk_color_primary_data_init(struct mtk_ddp_comp *comp)
{
	int i;
	struct mtk_disp_color *color_data = comp_to_color(comp);
	struct mtk_disp_color *companion_data = comp_to_color(color_data->companion);
	struct mtk_disp_color_primary *primary_data = color_data->primary_data;
	struct DISP_PQ_DC_PARAM dc_param_init = {
param:
			{1, 1, 0, 0, 0, 0, 0, 0, 0, 0x0A,
			 0x30, 0x40, 0x06, 0x12, 40, 0x40, 0x80, 0x40, 0x40, 1,
			 0x80, 0x60, 0x80, 0x10, 0x34, 0x40, 0x40, 1, 0x80, 0xa,
			 0x19, 0x00, 0x20, 0, 0, 1, 2, 1, 80, 1}
		};
	struct DISP_PQ_DS_PARAM ds_param_init = {
param:
			{1, -4, 1024, -4, 1024, 1, 400, 200, 1600, 800, 128, 8,
			 4, 12, 16, 8, 24, -8, -4, -12, 0, 0, 0}
		};
	/* initialize index */
	/* (because system default is 0, need fill with 0x80) */
	struct DISPLAY_PQ_T color_index_init = {
GLOBAL_SAT:	/* 0~9 */
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
CONTRAST :	/* 0~9 */
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
BRIGHTNESS :	/* 0~9 */
			{0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400},
PARTIAL_Y :
			{
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
PURP_TONE_S :
		{			/* hue 0~10 */
			{			/* 0 disable */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 1 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 2 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 3 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 4 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 5 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 6 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 7 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 8 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 9 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 10 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 11 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 12 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 13 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 14 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 15 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 16 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 17 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 18 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			}
		},
SKIN_TONE_S :
		{
			{			/* 0 disable */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 1 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 2 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 3 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 4 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 5 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 6 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 7 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 8 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 9 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 10 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 11 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 12 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 13 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 14 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 15 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 16 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 17 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 18 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			}
		},
GRASS_TONE_S :
		{
			{			/* 0 disable */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 1 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 2 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 3 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 4 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 5 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 6 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 7 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 8 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 9 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 10 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 11 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 12 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 13 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 14 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 15 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 16 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 17 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			},
			{			/* 18 */
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
			}
		},
SKY_TONE_S :
		{			/* hue 0~10 */
			{			/* 0 disable */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 1 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 2 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 3 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 4 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 5 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 6 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 7 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 8 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 9 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 10 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 11 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 12 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 13 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 14 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 15 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 16 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 17 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			},
			{			/* 18 */
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80},
				{0x80, 0x80, 0x80}
			}
		},
PURP_TONE_H :
		{
			/* hue 0~2 */
			{0x80, 0x80, 0x80},	/* 3 */
			{0x80, 0x80, 0x80},	/* 4 */
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},	/* 3 */
			{0x80, 0x80, 0x80},	/* 4 */
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},	/* 4 */
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},	/* 3 */
			{0x80, 0x80, 0x80},	/* 4 */
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80}
		},
SKIN_TONE_H :
		{
			/* hue 3~16 */
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
		},
GRASS_TONE_H :
		{
		/* hue 17~24 */
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
		},
SKY_TONE_H :
		{
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80},
			{0x80, 0x80, 0x80}
		},
CCORR_COEF : /* ccorr feature */
		{
			{
				{0x400, 0x0, 0x0},
				{0x0, 0x400, 0x0},
				{0x0, 0x0, 0x400},
			},
			{
				{0x400, 0x0, 0x0},
				{0x0, 0x400, 0x0},
				{0x0, 0x0, 0x400},
			},
			{
				{0x400, 0x0, 0x0},
				{0x0, 0x400, 0x0},
				{0x0, 0x0, 0x400},
			},
			{
				{0x400, 0x0, 0x0},
				{0x0, 0x400, 0x0},
				{0x0, 0x0, 0x400}
			}
		},
S_GAIN_BY_Y :
		{
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			},
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			},
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			},
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			},
			{0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80,
			 0x80, 0x80, 0x80, 0x80
			}
		},
S_GAIN_BY_Y_EN:0,
LSP_EN:0,
LSP :
		{0x0, 0x0, 0x7F, 0x7F, 0x7F, 0x0, 0x7F, 0x7F},
COLOR_3D :
		{
			{			/* 0 */
				/* Windows  1 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  2 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  3 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
			},
			{			/* 1 */
				/* Windows  1 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  2 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  3 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
			},
			{			/* 2 */
				/* Windows  1 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  2 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  3 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
			},
			{			/* 3 */
				/* Windows  1 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  2 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
				/* Windows  3 */
				{ 0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF,
				  0x80,  0x80,  0x80,  0x80,  0x80,  0x80, 0x3FF, 0x3FF,
				 0x000, 0x050, 0x100, 0x200, 0x300, 0x350, 0x3FF},
			},
		}
	};


	if (color_data->is_right_pipe) {
		kfree(color_data->primary_data);
		color_data->primary_data = companion_data->primary_data;
		return;
	}

	primary_data->legacy_color_cust = false;
	primary_data->color_param.u4SHPGain = 2;
	primary_data->color_param.u4SatGain = 4;
	for (i = 0; i < PQ_HUE_ADJ_PHASE_CNT; i++)
		primary_data->color_param.u4HueAdj[i] = 9;
	primary_data->color_param.u4Contrast = 4;
	primary_data->color_param.u4Brightness = 4;
	primary_data->split_window_x_end = 0xFFFF;
	primary_data->split_window_y_end = 0xFFFF;
	memcpy(&primary_data->pq_dc_param, &dc_param_init,
			sizeof(struct DISP_PQ_DC_PARAM));
	memcpy(&primary_data->pq_ds_param, &ds_param_init,
			sizeof(struct DISP_PQ_DS_PARAM));
	memcpy(&primary_data->color_index, &color_index_init,
			sizeof(struct DISPLAY_PQ_T));
	mutex_init(&primary_data->reg_lock);
}

static int mtk_color_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{
	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_color *data = comp_to_color(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_color *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = mtk_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_color(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		mtk_color_primary_data_init(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion)
			mtk_color_primary_data_init(data->companion);
	}
		break;
	default:
		break;
	}
	return 0;
}

static void mtk_color_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	//struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;
	struct DISP_AAL_DRECOLOR_PARAM *drecolor = &primary_data->drecolor_param;

	DpEngine_COLORonInit(comp, handle);

	mutex_lock(&primary_data->reg_lock);
	if (primary_data->color_reg_valid) {
		color_write_hw_reg(comp, &primary_data->color_reg, handle);
		if (drecolor->drecolor_sel)
			disp_color_write_drecolor_hw_reg(comp, handle, &drecolor->drecolor_reg);
		mutex_unlock(&primary_data->reg_lock);
	} else {
		mutex_unlock(&primary_data->reg_lock);
		DpEngine_COLORonConfig(comp, handle);
	}
	/*
	 *cmdq_pkt_write(handle, comp->cmdq_base,
	 *	       comp->regs_pa + DISP_COLOR_CFG_MAIN,
	 *	       COLOR_BYPASS_ALL | COLOR_SEQ_SEL, ~0);
	 *cmdq_pkt_write(handle, comp->cmdq_base,
	 *	       comp->regs_pa + DISP_COLOR_START(color), 0x1, ~0);
	 */
}

static void mtk_color_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{

}

void disp_color_write_pos_main_for_dual_pipe(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_WRITE_REG *wParams,
	unsigned int pa, unsigned int pa1)
{
	unsigned int pos_x, pos_y, val, val1, mask;
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	val = wParams->val;
	mask = wParams->mask;
	pos_x = (wParams->val & 0xffff);
	pos_y = ((wParams->val & (0xffff0000)) >> 16);
	DDPINFO("write POS_MAIN: pos_x[%d] pos_y[%d]\n",
		pos_x, pos_y);
	if (pos_x < primary_data->width) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa, val, mask);
		DDPINFO("dual pipe write pa:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa, val, mask);
		val1 = ((pos_x + primary_data->width) | ((pos_y << 16)));
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa1, val1, mask);
		DDPINFO("dual pipe write pa1:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa1, val1, mask);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa, val, mask);
		DDPINFO("dual pipe write pa:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa, val, mask);
		val1 = ((pos_x - primary_data->width) | ((pos_y << 16)));
		cmdq_pkt_write(handle, comp->cmdq_base,
			pa1, val1, mask);
		DDPINFO("dual pipe write pa1:0x%x(va:0) = 0x%x (0x%x)\n"
			, pa1, val1, mask);
	}
}

static int mtk_color_cfg_drecolor_set_param(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_disp_color *priv_data = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = comp_to_color(comp)->primary_data;
	struct DISP_AAL_DRECOLOR_PARAM *param = data;
	struct DISP_AAL_DRECOLOR_PARAM *prev_param = &priv_data->primary_data->drecolor_param;

	if (sizeof(struct DISP_AAL_DRECOLOR_PARAM) < data_size) {
		DDPPR_ERR("%s param size error %lu, %u\n", __func__, sizeof(*param), data_size);
		return -EFAULT;
	}
	DDPINFO("%s sel %d,prev_sel %d\n", __func__, param->drecolor_sel, prev_param->drecolor_sel);
	mutex_lock(&primary_data->reg_lock);
	if (!param->drecolor_sel) {
		DDPINFO("%s set skip\n", __func__);
		prev_param->drecolor_sel = param->drecolor_sel;
		mutex_unlock(&primary_data->reg_lock);
		return 0;
	}
	memcpy(prev_param, param, sizeof(struct DISP_AAL_DRECOLOR_PARAM));
	disp_color_write_drecolor_hw_reg(comp, handle, &param->drecolor_reg);
	if (comp->mtk_crtc->is_dual_pipe)
		disp_color_write_drecolor_hw_reg(priv_data->companion, handle, &param->drecolor_reg);
	mutex_unlock(&primary_data->reg_lock);
	return 0;
}

static int mtk_color_pq_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	DDPINFO("%s,SET COLOR REG id(%d) cmd = %d\n", __func__, comp->id, cmd);
	/* will only call left path */
	switch (cmd) {
	/* TYPE1 no user cmd */
	case PQ_COLOR_MUTEX_CONTROL:
		/*set ncs mode*/
		ret = mtk_color_cfg_mutex_control(comp, handle, data, data_size);
		break;
	case PQ_COLOR_BYPASS:
		ret = mtk_color_cfg_bypass(comp, handle, data, data_size);
		break;
	case PQ_COLOR_SET_PQINDEX:
		/*just memcpy user data*/
		ret = mtk_drm_color_cfg_set_pqindex(comp, handle, data, data_size);
		break;
	case PQ_COLOR_SET_PQPARAM:
		ret = mtk_drm_color_cfg_set_pqparam(comp, handle, data, data_size);
		break;
	case PQ_COLOR_SET_COLOR_REG:
		ret = mtk_color_cfg_set_color_reg(comp, handle, data, data_size);
		break;
	case PQ_COLOR_SET_WINDOW:
		ret = mtk_drm_color_cfg_pq_set_window(comp, handle, data, data_size);
		break;
	case PQ_COLOR_DRECOLOR_SET_PARAM:
		ret = mtk_color_cfg_drecolor_set_param(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_color_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	struct mtk_disp_color_primary *primary_data = color->primary_data;

	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case SET_PQPARAM:
	{
		/* normal mode */
		DpEngine_COLORonInit(comp, handle);
		DpEngine_COLORonConfig(comp, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_color1 = color->companion;

			DpEngine_COLORonInit(comp_color1, handle);
			DpEngine_COLORonConfig(comp_color1, handle);
		}
	}
	break;
	case SET_COLOR_REG:
	{
		mutex_lock(&primary_data->reg_lock);

		if (data != NULL) {
			memcpy(&primary_data->color_reg, (struct DISPLAY_COLOR_REG *)data,
				sizeof(struct DISPLAY_COLOR_REG));

			color_write_hw_reg(comp, &primary_data->color_reg, handle);
			if (comp->mtk_crtc->is_dual_pipe) {
				struct mtk_ddp_comp *comp_color1 = color->companion;

				color_write_hw_reg(comp_color1, &primary_data->color_reg, handle);
			}
		} else {
			DDPINFO("%s: data is NULL", __func__);
		}

		primary_data->color_reg_valid = 1;
		mutex_unlock(&primary_data->reg_lock);
	}
	break;
	case BYPASS_COLOR:
	{
		unsigned int *value = data;

		mtk_color_bypass(comp, *value, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_color1 = color->companion;

			mtk_color_bypass(comp_color1, *value, handle);
		}
	}
	break;
	case PQ_SET_WINDOW:
	{
		struct DISP_PQ_WIN_PARAM *win_param = data;

		ddp_color_set_window(comp, win_param, handle);
	}
	break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static void ddp_color_backup(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	primary_data->color_backup.COLOR_CFG_MAIN =
		readl(comp->regs + DISP_COLOR_CFG_MAIN);
}

static void ddp_color_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color_primary *primary_data =
		comp_to_color(comp)->primary_data;

	writel(primary_data->color_backup.COLOR_CFG_MAIN, comp->regs + DISP_COLOR_CFG_MAIN);
}

static void mtk_color_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color = comp_to_color(comp);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&color->color_is_clock_on, 1);

	/* Bypass shadow register and read shadow register */
	if (color->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, COLOR_BYPASS_SHADOW,
			DISP_COLOR_SHADOW_CTRL, COLOR_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			DISP_COLOR_SHADOW_CTRL, COLOR_BYPASS_SHADOW);
	// restore DISP_COLOR_CFG_MAIN register
	ddp_color_restore(comp);
}

static void mtk_color_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color_data = comp_to_color(comp);
	unsigned long flags;

	DDPINFO("%s @ %d......... spin_lock_irqsave ++ ", __func__, __LINE__);
	spin_lock_irqsave(&color_data->clock_lock, flags);
	DDPINFO("%s @ %d......... spin_lock_irqsave -- ", __func__, __LINE__);
	atomic_set(&color_data->color_is_clock_on, 0);
	spin_unlock_irqrestore(&color_data->clock_lock, flags);
	DDPINFO("%s @ %d......... spin_unlock_irqrestore ", __func__, __LINE__);
	// backup DISP_COLOR_CFG_MAIN register
	ddp_color_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_color_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color_data = comp_to_color(comp);

	spin_lock_init(&color_data->clock_lock);
}

void mtk_color_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	mtk_color_config(comp, cfg, handle);
	ddp_color_backup(comp);
}

static int mtk_color_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	switch (cmd) {
	case PQ_COLOR_SET_PQPARAM:
		ret = mtk_drm_ioctl_set_pqparam_impl(comp, data);
		break;
	case PQ_COLOR_SET_PQINDEX:
		ret = mtk_drm_ioctl_set_pqindex_impl(comp, data);
		break;
	case PQ_COLOR_SET_COLOR_REG:
		ret = mtk_drm_ioctl_set_color_reg_impl(comp, data);
		break;
	case PQ_COLOR_MUTEX_CONTROL:
		ret = mtk_drm_ioctl_mutex_control_impl(comp, data);
		break;
	case PQ_COLOR_BYPASS:
		ret = mtk_drm_ioctl_bypass_color_impl(comp, data);
		break;
	case PQ_COLOR_SET_WINDOW:
		ret = mtk_drm_ioctl_pq_set_window_impl(comp, data);
		break;
	default:
		break;
	}
	return ret;
}


static int mtk_color_set_partial_update(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, struct mtk_rect partial_roi, bool enable)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	color->set_partial_update = enable;
	color->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : color->tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (color->set_partial_update) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_HEIGHT(color), color->roi_height + overhead_v * 2, ~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_COLOR_HEIGHT(color), full_height, ~0);
	}

	return 0;
}


static const struct mtk_ddp_comp_funcs mtk_disp_color_funcs = {
	.config = mtk_color_config,
	.first_cfg = mtk_color_first_cfg,
	.start = mtk_color_start,
	.stop = mtk_color_stop,
	.bypass = mtk_color_bypass,
	.user_cmd = mtk_color_user_cmd,
	.prepare = mtk_color_prepare,
	.unprepare = mtk_color_unprepare,
	.config_overhead = mtk_disp_color_config_overhead,
	.config_overhead_v = mtk_disp_color_config_overhead_v,
	.io_cmd = mtk_color_io_cmd,
	.pq_frame_config = mtk_color_pq_frame_config,
	.pq_ioctl_transact = mtk_color_ioctl_transact,
	.partial_update = mtk_color_set_partial_update,
};

void mtk_color_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_serial_dump_reg(baddr, 0x400, 3);
	mtk_serial_dump_reg(baddr, 0xC50, 2);
}

void mtk_color_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_color *color = comp_to_color(comp);
	void __iomem *baddr = comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp),
			comp->regs_pa);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0x400; k <= 0xd5c; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (comp->mtk_crtc->is_dual_pipe && color->companion) {
		baddr = color->companion->regs;
		DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(color->companion),
				color->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(color->companion));
		for (k = 0x400; k <= 0xd5c; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(color->companion));
	}
}

static int mtk_disp_color_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_color *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_color_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_color *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_color_component_ops = {
	.bind	= mtk_disp_color_bind,
	.unbind = mtk_disp_color_unbind,
};

static int mtk_disp_color_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_color *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret = -1;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		goto error_dev_init;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		dev_err(dev, "Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_COLOR);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		goto error_primary;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_color_funcs);
	if (ret != 0) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		goto error_primary;
	}
	mtk_color_data_init(&priv->ddp_comp);

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_color_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int mtk_disp_color_remove(struct platform_device *pdev)
{
	struct mtk_disp_color *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_color_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_color_data mt2701_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT2701,
	.support_color21 = false,
	.support_color30 = false,
	.color_window = 0x40106051,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6779_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6779,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x1400E000, 0x1400F000, 0x14001000,
			0x14011000, 0x14012000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt8173_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT8173,
	.support_color21 = false,
	.support_color30 = false,
	.color_window = 0x40106051,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6885_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6885,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14007000, 0x14008000, 0x14009000,
			0x1400A000, 0x1400B000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_color_data mt6873_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400B000,
			0x1400C000, 0x1400E000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6853_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = false,
	.reg_table = {0x14009000, 0x1400B000, 0x1400C000,
			0x1400D000, 0x1400F000, 0x1400A000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6833_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = false,
	.reg_table = {0x14009000, 0x1400A000, 0x1400B000,
			0x1400C000, 0x1400E000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6983_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000, 0x1400E000,
			0x14010000, 0x1400B000, 0x14007000, 0x14008000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6895_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000, 0x1400E000,
			0x14010000, 0x1400B000, 0x14007000, 0x14008000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6879_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = false,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000, 0x1400E000,
			0x14010000, 0x0, 0x14007000, 0x14008000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6855_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000,
			0x1400E000, 0x14010000, -1UL, 0x14007000, -1UL},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6985_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14008000, 0x14004000, 0x14002000, 0x1400E000,
			0x14009000, 0x14005000, 0x14018000, 0x14003000,
			0x1400F000, 0x14013000, 0x14014000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6897_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14008000, 0x14004000, 0x14002000, 0x1400E000,
			0x14009000, 0x14005000, 0x14018000, 0x14003000,
			0x1400F000, 0x14013000, 0x14014000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6886_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x14009000, 0x1400A000, 0x1400D000, 0x1400E000,
			0x14010000, 0x1400B000, -1UL, 0x14008000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6989_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.reg_table = {0x1400C000, 0x14006000, 0x14002000, 0x14011000,
			0x1400E000, 0x14007000, 0x1401C000, 0x14004000,
			0x14013000, 0x14015000, 0x14016000},
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_color_data mt6878_color_driver_data = {
	.color_offset = DISP_COLOR_START_MT6873,
	.support_color21 = true,
	.support_color30 = true,
	.color_window = 0x40185E57,
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_color_driver_dt_match[] = {
	{.compatible = "mediatek,mt2701-disp-color",
	 .data = &mt2701_color_driver_data},
	{.compatible = "mediatek,mt6779-disp-color",
	 .data = &mt6779_color_driver_data},
	{.compatible = "mediatek,mt6885-disp-color",
	 .data = &mt6885_color_driver_data},
	{.compatible = "mediatek,mt8173-disp-color",
	 .data = &mt8173_color_driver_data},
	{.compatible = "mediatek,mt6873-disp-color",
	 .data = &mt6873_color_driver_data},
	{.compatible = "mediatek,mt6853-disp-color",
	 .data = &mt6853_color_driver_data},
	{.compatible = "mediatek,mt6833-disp-color",
	 .data = &mt6833_color_driver_data},
	{.compatible = "mediatek,mt6983-disp-color",
	 .data = &mt6983_color_driver_data},
	{.compatible = "mediatek,mt6895-disp-color",
	 .data = &mt6895_color_driver_data},
	{.compatible = "mediatek,mt6879-disp-color",
	 .data = &mt6879_color_driver_data},
	{.compatible = "mediatek,mt6855-disp-color",
	 .data = &mt6855_color_driver_data},
	{.compatible = "mediatek,mt6985-disp-color",
	 .data = &mt6985_color_driver_data},
	{.compatible = "mediatek,mt6886-disp-color",
	 .data = &mt6886_color_driver_data},
	{.compatible = "mediatek,mt6835-disp-color",
	 .data = &mt6835_color_driver_data},
	{.compatible = "mediatek,mt6897-disp-color",
	 .data = &mt6897_color_driver_data},
	{.compatible = "mediatek,mt6989-disp-color",
	 .data = &mt6989_color_driver_data},
	{.compatible = "mediatek,mt6878-disp-color",
	 .data = &mt6878_color_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_color_driver_dt_match);

struct platform_driver mtk_disp_color_driver = {
	.probe = mtk_disp_color_probe,
	.remove = mtk_disp_color_remove,
	.driver = {
			.name = "mediatek-disp-color",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_color_driver_dt_match,
		},
};

void disp_color_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_COLOR, 0);

	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_COLOR, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}

unsigned int disp_color_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_COLOR, 0);
	struct mtk_disp_color *color_data = comp_to_color(comp);

	return color_data->primary_data->color_bypass;
}
