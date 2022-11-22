/* drivers/video/fbdev/exynos/dpu30/cal_9630/dpp_regs.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS9 SoC series Display Pre Processor driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#if defined(CONFIG_EXYNOS_HDR_TUNABLE_TONEMAPPING)
#include <video/exynos_hdr_tunables.h>
#endif

#include "../dpp.h"
#include "../dpp_coef.h"
#include "../hdr_lut.h"
#include "../format.h"

/****************** ODMA CAL functions ******************/
/* EMPTY */

#define DPP_SC_RATIO_MAX	((1 << 20) * 8 / 8)
#define DPP_SC_RATIO_7_8	((1 << 20) * 8 / 7)
#define DPP_SC_RATIO_6_8	((1 << 20) * 8 / 6)
#define DPP_SC_RATIO_5_8	((1 << 20) * 8 / 5)
#define DPP_SC_RATIO_4_8	((1 << 20) * 8 / 4)
#define DPP_SC_RATIO_3_8	((1 << 20) * 8 / 3)

/****************** IDMA CAL functions ******************/
static void idma_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, IDMA_IRQ, val, IDMA_ALL_IRQ_MASK);
}

static void idma_reg_set_irq_enable(u32 id)
{
	dma_write_mask(id, IDMA_IRQ, ~0, IDMA_IRQ_ENABLE);
}

static void idma_reg_set_in_qos_lut(u32 id, u32 lut_id, u32 qos_t)
{
	u32 reg_id;

	if (lut_id == 0)
		reg_id = IDMA_QOS_LUT07_00;
	else
		reg_id = IDMA_QOS_LUT15_08;
	dma_write(id, reg_id, qos_t);
}

static void idma_reg_set_sram_clk_gate_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, IDMA_DYNAMIC_GATING_EN, val, IDMA_SRAM_CG_EN);
}

static void idma_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, IDMA_DYNAMIC_GATING_EN, val, IDMA_DG_EN_ALL);
}

static void idma_reg_clear_irq(u32 id, u32 irq)
{
	dma_write_mask(id, IDMA_IRQ, ~0, irq);
}

static void idma_reg_set_sw_reset(u32 id)
{
	dma_write_mask(id, IDMA_ENABLE, ~0, IDMA_SRESET);
}

static void idma_reg_set_assigned_mo(u32 id, u32 mo)
{
	dma_write_mask(id, IDMA_ENABLE, IDMA_ASSIGNED_MO(mo),
			IDMA_ASSIGNED_MO_MASK);
}

static int idma_reg_wait_sw_reset_status(u32 id)
{
	u32 cfg = 0;
	unsigned long cnt = 100000;

	do {
		cfg = dma_read(id, IDMA_ENABLE);
		if (!(cfg & (IDMA_SRESET)))
			return 0;
		udelay(10);
	} while (--cnt);

	dpp_err("[idma%d] timeout sw-reset\n", id);

	return -1;
}

static void idma_reg_set_coordinates(u32 id, struct decon_frame *src)
{
	dma_write(id, IDMA_SRC_OFFSET,
			IDMA_SRC_OFFSET_Y(src->y) | IDMA_SRC_OFFSET_X(src->x));
	dma_write(id, IDMA_SRC_SIZE,
			IDMA_SRC_HEIGHT(src->f_h) | IDMA_SRC_WIDTH(src->f_w));
	dma_write(id, IDMA_IMG_SIZE,
			IDMA_IMG_HEIGHT(src->h) | IDMA_IMG_WIDTH(src->w));
}

static void idma_reg_set_pixel_alpha(u32 id, u32 alpha)
{
	dma_write_mask(id, IDMA_IN_CON, IDMA_PIXEL_ALPHA(alpha),
			IDMA_PIXEL_ALPHA_MASK);
}

static void idma_reg_set_in_ic_max(u32 id, u32 ic_max)
{
	dma_write_mask(id, IDMA_IN_CON, IDMA_IN_IC_MAX(ic_max),
			IDMA_IN_IC_MAX_MASK);
}

static void idma_reg_set_rotation(u32 id, u32 rot)
{
	dma_write_mask(id, IDMA_IN_CON, IDMA_ROTATION(rot), IDMA_ROTATION_MASK);
}

static void idma_reg_set_block_mode(u32 id, bool en, int x, int y, u32 w, u32 h)
{
	if (!en) {
		dma_write_mask(id, IDMA_IN_CON, 0, IDMA_BLOCK_EN);
		return;
	}

	dma_write(id, IDMA_BLOCK_OFFSET,
			IDMA_BLK_OFFSET_Y(y) | IDMA_BLK_OFFSET_X(x));
	dma_write(id, IDMA_BLOCK_SIZE, IDMA_BLK_HEIGHT(h) | IDMA_BLK_WIDTH(w));
	dma_write_mask(id, IDMA_IN_CON, ~0, IDMA_BLOCK_EN);

	dpp_dbg("dpp%d: block x(%d) y(%d) w(%d) h(%d)\n", id, x, y, w, h);
}

static void idma_reg_set_format(u32 id, u32 fmt)
{
	dma_write_mask(id, IDMA_IN_CON, IDMA_IMG_FORMAT(fmt),
			IDMA_IMG_FORMAT_MASK);
}

#if defined(DMA_BIST)
static void idma_reg_set_test_pattern(u32 id, u32 pat_id, u32 *pat_dat)
{
	/* 0=AXI, 3=PAT */
	dma_write_mask(id, IDMA_IN_REQ_DEST, ~0, IDMA_IN_REG_DEST_SEL_MASK);

	if (pat_id == 0) {
		dma_com_write(id, DPU_DMA_TEST_PATTERN0_0, pat_dat[0]);
		dma_com_write(id, DPU_DMA_TEST_PATTERN0_1, pat_dat[1]);
		dma_com_write(id, DPU_DMA_TEST_PATTERN0_2, pat_dat[2]);
		dma_com_write(id, DPU_DMA_TEST_PATTERN0_3, pat_dat[3]);
	} else {
		dma_com_write(id, DPU_DMA_TEST_PATTERN1_0, pat_dat[4]);
		dma_com_write(id, DPU_DMA_TEST_PATTERN1_1, pat_dat[5]);
		dma_com_write(id, DPU_DMA_TEST_PATTERN1_2, pat_dat[6]);
		dma_com_write(id, DPU_DMA_TEST_PATTERN1_3, pat_dat[7]);
	}
}
#endif

static void idma_reg_set_afbc(u32 id, enum dpp_comp_type ct, u32 rcv_num)
{
	u32 afbc_en = 0;
	u32 rcv_en = 0;

	if (ct == COMP_TYPE_AFBC)
		afbc_en = IDMA_AFBC_EN;
	if (ct != COMP_TYPE_NONE)
		rcv_en = IDMA_RECOVERY_EN;

	dma_write_mask(id, IDMA_IN_CON, afbc_en, IDMA_AFBC_EN);
	dma_write_mask(id, IDMA_RECOVERY_CTRL, rcv_en, IDMA_RECOVERY_EN);
	dma_write_mask(id, IDMA_RECOVERY_CTRL, IDMA_RECOVERY_NUM(rcv_num),
				IDMA_RECOVERY_NUM_MASK);
}

static void idma_reg_set_sbwc(u32 id, enum dpp_comp_type ct, u32 rcv_num)
{
	u32 sbwc_en = 0;
	u32 rcv_en = 0;

	if (ct == COMP_TYPE_SBWC)
		sbwc_en = IDMA_SBWC_EN;
	if (ct != COMP_TYPE_NONE)
		rcv_en = IDMA_RECOVERY_EN;

	dma_write_mask(id, IDMA_IN_CON, sbwc_en, IDMA_SBWC_EN);
	dma_write_mask(id, IDMA_RECOVERY_CTRL, rcv_en, IDMA_RECOVERY_EN);
	dma_write_mask(id, IDMA_RECOVERY_CTRL, IDMA_RECOVERY_NUM(rcv_num),
				IDMA_RECOVERY_NUM_MASK);
}

static void idma_reg_set_sbwcl(u32 id, enum dpp_comp_type ct, u32 size_32x, u32 rcv_num)
{
	u32 sbwc_en = 0;
	u32 rcv_en = 0;

	if (ct == COMP_TYPE_SBWCL)
		sbwc_en = IDMA_SBWC_LOSSY_EN | IDMA_SBWC_EN;
	if (ct != COMP_TYPE_NONE)
		rcv_en = IDMA_RECOVERY_EN;

	dma_write_mask(id, IDMA_IN_CON, sbwc_en, IDMA_SBWC_LOSSY_EN | IDMA_SBWC_EN);
	dma_write_mask(id, IDMA_SBWC_PARAM,
			IDMA_CHM_LOSSY_BYTENUM(size_32x) | IDMA_LUM_LOSSY_BYTENUM(size_32x),
			IDMA_CHM_LOSSY_BYTENUM_MASK | IDMA_LUM_LOSSY_BYTENUM_MASK);
	dma_write_mask(id, IDMA_RECOVERY_CTRL, rcv_en, IDMA_RECOVERY_EN);
	dma_write_mask(id, IDMA_RECOVERY_CTRL, IDMA_RECOVERY_NUM(rcv_num),
				IDMA_RECOVERY_NUM_MASK);
}

static void idma_reg_set_deadlock(u32 id, u32 en, u32 dl_num)
{
	u32 val = en ? ~0 : 0;

	dma_write_mask(id, IDMA_DEADLOCK_EN, val, IDMA_DEADLOCK_TIMER_EN);
	dma_write_mask(id, IDMA_DEADLOCK_EN, IDMA_DEADLOCK_TIMER(dl_num),
				IDMA_DEADLOCK_TIMER_MASK);
}

/****************** ODMA CAL functions ******************/
/* EMPTY */

/****************** DPP CAL functions ******************/
static void dpp_reg_set_irq_mask_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dpp_write_mask(id, DPP_IRQ, val, DPP_ALL_IRQ_MASK);
}

static void dpp_reg_set_irq_enable(u32 id)
{
	dpp_write_mask(id, DPP_IRQ, ~0, DPP_IRQ_ENABLE);
}

static void dpp_reg_set_clock_gate_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dpp_write_mask(id, DPP_ENABLE, val, DPP_ALL_CLOCK_GATE_EN_MASK);
}

static void dpp_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dpp_write_mask(id, DPP_DYNAMIC_GATING_EN, val, DPP_DG_EN_ALL);
}

static void dpp_reg_set_linecnt(u32 id, u32 en)
{
	if (en)
		dpp_write_mask(id, DPP_LINECNT_CON,
				DPP_LC_MODE(0) | DPP_LC_ENABLE(1),
				DPP_LC_MODE_MASK | DPP_LC_ENABLE_MASK);
	else
		dpp_write_mask(id, DPP_LINECNT_CON, DPP_LC_ENABLE(0),
				DPP_LC_ENABLE_MASK);
}

static void dpp_reg_clear_irq(u32 id, u32 irq)
{
	dpp_write_mask(id, DPP_IRQ, ~0, irq);
}

static void dpp_reg_set_sw_reset(u32 id)
{
	dpp_write_mask(id, DPP_ENABLE, ~0, DPP_SRSET);
}

static int dpp_reg_wait_sw_reset_status(u32 id)
{
	u32 cfg = 0;
	unsigned long cnt = 100000;

	do {
		cfg = dpp_read(id, DPP_ENABLE);
		if (!(cfg & (DPP_SRSET)))
			return 0;
		udelay(10);
	} while (--cnt);

	dpp_err("[dpp] timeout sw reset\n");

	return -EBUSY;
}

static void dpp_reg_set_csc_coef(u32 id, u32 csc_std, u32 csc_rng)
{
	u32 val, mask;
	u32 csc_id = CSC_CUSTOMIZED_START; /* CSC_BT601/625/525 */
	u32 c00, c01, c02;
	u32 c10, c11, c12;
	u32 c20, c21, c22;

	if ((csc_std > CSC_DCI_P3) && (csc_std <= CSC_ADOBE_RGB))
		csc_id = (csc_std - CSC_CUSTOMIZED_START) * 2 + csc_rng;
	else
		dpp_warn("[DPP%d] Undefined CSC Type!(std=%d)\n", id, csc_std);

	c00 = csc_y2r_3x3_t[csc_id][0][0];
	c01 = csc_y2r_3x3_t[csc_id][0][1];
	c02 = csc_y2r_3x3_t[csc_id][0][2];

	c10 = csc_y2r_3x3_t[csc_id][1][0];
	c11 = csc_y2r_3x3_t[csc_id][1][1];
	c12 = csc_y2r_3x3_t[csc_id][1][2];

	c20 = csc_y2r_3x3_t[csc_id][2][0];
	c21 = csc_y2r_3x3_t[csc_id][2][1];
	c22 = csc_y2r_3x3_t[csc_id][2][2];

	mask = (DPP_CSC_COEF_H_MASK | DPP_CSC_COEF_L_MASK);
	val = (DPP_CSC_COEF_H(c01) | DPP_CSC_COEF_L(c00));
	dpp_write_mask(id, DPP_CSC_COEF0, val, mask);

	val = (DPP_CSC_COEF_H(c10) | DPP_CSC_COEF_L(c02));
	dpp_write_mask(id, DPP_CSC_COEF1, val, mask);

	val = (DPP_CSC_COEF_H(c12) | DPP_CSC_COEF_L(c11));
	dpp_write_mask(id, DPP_CSC_COEF2, val, mask);

	val = (DPP_CSC_COEF_H(c21) | DPP_CSC_COEF_L(c20));
	dpp_write_mask(id, DPP_CSC_COEF3, val, mask);

	mask = DPP_CSC_COEF_L_MASK;
	val = DPP_CSC_COEF_L(c22);
	dpp_write_mask(id, DPP_CSC_COEF4, val, mask);

	dpp_dbg("---[DPP%d Y2R CSC Type: std=%d, rng=%d]---\n",
		id, csc_std, csc_rng);
	dpp_dbg("0x%4x  0x%4x  0x%4x\n", c00, c01, c02);
	dpp_dbg("0x%4x  0x%4x  0x%4x\n", c10, c11, c12);
	dpp_dbg("0x%4x  0x%4x  0x%4x\n", c20, c21, c22);
}

static void dpp_reg_set_csc_params(u32 id, u32 csc_eq)
{
	u32 type = (csc_eq >> CSC_STANDARD_SHIFT) & 0x3F;
	u32 range = (csc_eq >> CSC_RANGE_SHIFT) & 0x7;
	u32 mode = (type <= CSC_DCI_P3) ? CSC_COEF_HARDWIRED : CSC_COEF_CUSTOMIZED;
	u32 val, mask;

	if (type == CSC_STANDARD_UNSPECIFIED) {
		dpp_dbg("unspecified CSC type! -> BT_601\n");
		type = CSC_BT_601;
		mode = CSC_COEF_HARDWIRED;
	}
	if (range == CSC_RANGE_UNSPECIFIED) {
		dpp_dbg("unspecified CSC range! -> LIMITED\n");
		range = CSC_RANGE_LIMITED;
	}

	val = (DPP_CSC_TYPE(type) | DPP_CSC_RANGE(range) | DPP_CSC_MODE(mode));
	mask = (DPP_CSC_TYPE_MASK | DPP_CSC_RANGE_MASK | DPP_CSC_MODE_MASK);
	dpp_write_mask(id, DPP_IN_CON, val, mask);

	if (mode == CSC_COEF_CUSTOMIZED)
		dpp_reg_set_csc_coef(id, type, range);

	dpp_dbg("DPP%d CSC info: type=%d, range=%d, mode=%d\n",
		id, type, range, mode);
}

static void dpp_reg_set_h_coef(u32 id, u32 h_ratio)
{
	int i, j, k, sc_ratio;

	if (h_ratio <= DPP_SC_RATIO_MAX)
		sc_ratio = 0;
	else if (h_ratio <= DPP_SC_RATIO_7_8)
		sc_ratio = 1;
	else if (h_ratio <= DPP_SC_RATIO_6_8)
		sc_ratio = 2;
	else if (h_ratio <= DPP_SC_RATIO_5_8)
		sc_ratio = 3;
	else if (h_ratio <= DPP_SC_RATIO_4_8)
		sc_ratio = 4;
	else if (h_ratio <= DPP_SC_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++)
		for (j = 0; j < 8; j++)
			for (k = 0; k < 2; k++)
				dpp_write(id, DPP_H_COEF(i, j, k),
						h_coef_8t[sc_ratio][i][j]);
}

static void dpp_reg_set_v_coef(u32 id, u32 v_ratio)
{
	int i, j, k, sc_ratio;

	if (v_ratio <= DPP_SC_RATIO_MAX)
		sc_ratio = 0;
	else if (v_ratio <= DPP_SC_RATIO_7_8)
		sc_ratio = 1;
	else if (v_ratio <= DPP_SC_RATIO_6_8)
		sc_ratio = 2;
	else if (v_ratio <= DPP_SC_RATIO_5_8)
		sc_ratio = 3;
	else if (v_ratio <= DPP_SC_RATIO_4_8)
		sc_ratio = 4;
	else if (v_ratio <= DPP_SC_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < 9; i++)
		for (j = 0; j < 4; j++)
			for (k = 0; k < 2; k++)
				dpp_write(id, DPP_V_COEF(i, j, k),
						v_coef_4t[sc_ratio][i][j]);
}

static void dpp_reg_set_scale_ratio(u32 id, struct dpp_params_info *p)
{
	dpp_write_mask(id, DPP_MAIN_H_RATIO, DPP_H_RATIO(p->h_ratio),
			DPP_H_RATIO_MASK);
	dpp_write_mask(id, DPP_MAIN_V_RATIO, DPP_V_RATIO(p->v_ratio),
			DPP_V_RATIO_MASK);

	dpp_reg_set_h_coef(id, p->h_ratio);
	dpp_reg_set_v_coef(id, p->v_ratio);

	dpp_dbg("h_ratio : %#x, v_ratio : %#x\n", p->h_ratio, p->v_ratio);
}

static void dpp_reg_set_img_size(u32 id, u32 w, u32 h)
{
	dpp_write(id, DPP_IMG_SIZE, DPP_IMG_HEIGHT(h) | DPP_IMG_WIDTH(w));
}

static void dpp_reg_set_scaled_img_size(u32 id, u32 w, u32 h)
{
	dpp_write(id, DPP_SCALED_IMG_SIZE,
			DPP_SCALED_IMG_HEIGHT(h) | DPP_SCALED_IMG_WIDTH(w));
}

static void dpp_reg_set_alpha_type(u32 id, u32 type)
{
	/* [type] 0=per-frame, 1=per-pixel */
	dpp_write_mask(id, DPP_IN_CON, DPP_ALPHA_SEL(type), DPP_ALPHA_SEL_MASK);
}

static void dpp_reg_set_format(u32 id, u32 fmt)
{
	dpp_write_mask(id, DPP_IN_CON, DPP_IMG_FORMAT(fmt), DPP_IMG_FORMAT_MASK);
}

static void dpp_reg_set_oetf_lut(u32 id, struct dpp_params_info *p)
{
	u32 i = 0;
	u32 *lut_x = NULL;
	u32 *lut_y = NULL;

	lut_x = p->hdr_params.oetf_x;
	lut_y = p->hdr_params.oetf_y;

	for (i = 0; i < MAX_OETF; i++) {
		dpp_hdr_write_mask(id,
			HDR_LSI_LN_OETF_POSX(i),
			HDR_LSI_LN_OETF_POSX_VAL(i, lut_x[i]),
			HDR_LSI_LN_OETF_POSX_MASK(i));
		dpp_dbg("OE_X offset 0x%x : 0x%x, mask(0x%x)\n",
			HDR_LSI_LN_OETF_POSX(i),
			HDR_LSI_LN_OETF_POSX_VAL(i, lut_x[i]),
			HDR_LSI_LN_OETF_POSX_MASK(i));
		dpp_hdr_write_mask(id,
			HDR_LSI_LN_OETF_POSY(i),
			HDR_LSI_LN_OETF_POSY_VAL(i, lut_y[i]),
			HDR_LSI_LN_OETF_POSY_MASK(i));
		dpp_dbg("OE_Y offset 0x%x : 0x%x, mask(0x%x)\n",
			HDR_LSI_LN_OETF_POSY(i),
			HDR_LSI_LN_OETF_POSY_VAL(i, lut_y[i]),
			HDR_LSI_LN_OETF_POSY_MASK(i));
	}
}

static void dpp_reg_set_eotf_lut(u32 id, struct dpp_params_info *p)
{
	u32 i = 0;
	u32 *lut_x = NULL;
	u32 *lut_y = NULL;

	lut_x = p->hdr_params.eotf_x;
	lut_y = p->hdr_params.eotf_y;

	for (i = 0; i < MAX_EOTF; i++) {
		dpp_hdr_write_mask(id,
			HDR_LSI_LN_EOTF_POSX(i),
			HDR_LSI_LN_EOTF_POSX_VAL(i, lut_x[i]),
			HDR_LSI_LN_EOTF_POSX_MASK(i));
		dpp_dbg("EO_X offset 0x%x : 0x%x, mask(0x%x)\n", HDR_LSI_LN_EOTF_POSX(i),
			HDR_LSI_LN_EOTF_POSX_VAL(i, lut_x[i]),
			HDR_LSI_LN_EOTF_POSX_MASK(i));
		dpp_hdr_write_mask(id,
			HDR_LSI_LN_EOTF_POSY(i),
			HDR_LSI_LN_EOTF_POSY_VAL(lut_y[i]),
			HDR_LSI_LN_EOTF_POSY_MASK(i));
		dpp_dbg("EO_Y offset 0x%x : 0x%x, mask(0x%x)\n", HDR_LSI_LN_EOTF_POSY(i),
			HDR_LSI_LN_EOTF_POSY_VAL(lut_y[i]),
			HDR_LSI_LN_EOTF_POSY_MASK(i));
	}
}

static void dpp_reg_set_gm_lut(u32 id, struct dpp_params_info *p)
{
	u32 i = 0;
	u32 *lut_gm = NULL;

	lut_gm = p->hdr_params.gm_coef;

	for (i = 0; i < MAX_GM; i++) {
		dpp_hdr_write(id,
			HDR_LSI_LN_GM_COEF(i),
			HDR_LSI_LN_GM_COEF_VAL(lut_gm[i]));
		dpp_dbg("GM offset 0x%x : 0x%x\n", HDR_LSI_LN_GM_COEF(i),
			HDR_LSI_LN_GM_COEF_VAL(lut_gm[i]));
	}
}

void dpp_reg_set_tm_lut(u32 id, struct dpp_params_info *p)
{
	u32 i = 0;
	u32 val = 0;
	u32 *lut_x = NULL;
	u32 *lut_y = NULL;

	lut_x = p->hdr_params.tm_x;
	lut_y = p->hdr_params.tm_y;

	for (i = 0; i < MAX_TM; i++) {
		dpp_hdr_write_mask(id,
			HDR_LSI_LN_TM_POSX(i),
			HDR_LSI_LN_TM_POSX_VAL(i, lut_x[i]),
			HDR_LSI_LN_TM_POSX_MASK(i));
		dpp_dbg("TM_X offset 0x%x : 0x%x, mask(0x%x)\n",
			HDR_LSI_LN_TM_POSX(i),
			HDR_LSI_LN_TM_POSX_VAL(i, lut_x[i]),
			HDR_LSI_LN_TM_POSX_MASK(i));
		dpp_hdr_write_mask(id,
			HDR_LSI_LN_TM_POSY(i),
			HDR_LSI_LN_TM_POSY_VAL(lut_y[i]),
			HDR_LSI_LN_TM_POSY_MASK(i));
		dpp_dbg("TM_X offset 0x%x : 0x%x, mask(0x%x)\n",
			HDR_LSI_LN_TM_POSY(i),
			HDR_LSI_LN_TM_POSY_VAL(lut_y[i]),
			HDR_LSI_LN_TM_POSY_MASK(i));
	}

	/* Tone mapping range is not used */
	val = HDR_LSI_LN_TM_COEFB_VAL(p->hdr_params.tm_coef[2])
		| HDR_LSI_LN_TM_COEFG_VAL(p->hdr_params.tm_coef[1])
		| HDR_LSI_LN_TM_COEFR_VAL(p->hdr_params.tm_coef[0]);

	dpp_hdr_write(id, HDR_LSI_LN_TM_COEF, val);

	val = HDR_LSI_LN_TM_RNGX_MAXY_VAL(p->hdr_params.tm_rngx[1])
		| HDR_LSI_LN_TM_RNGX_MINY_VAL(p->hdr_params.tm_rngx[0]);
	dpp_hdr_write(id, HDR_LSI_LN_TM_RNGX, val);

	val = HDR_LSI_LN_TM_RNGY_MAXY_VAL(p->hdr_params.tm_rngy[1])
		| HDR_LSI_LN_TM_RNGY_MINY_VAL(p->hdr_params.tm_rngy[0]);
	dpp_hdr_write(id, HDR_LSI_LN_TM_RNGY, val);

}

/* wcg & static */
void dpp_reg_set_hdr_params(u32 id, struct dpp_params_info *p)
{
	u32 hdr_en_val = 0;
	u32 hdr_en_mask = 0;
	u32 premulti_val = 0;
	u32 premulti_mask = 0;
	u32 val = 0;
	u32 mask = 0;

	/* SLSI HDR enable, HDR processing Enable */
	val = (p->hdr_params.hdr_en == true) ? HDR_LSI_LN_COM_CTRL_EN : 0;
	hdr_en_val = HDR_LSI_LN_COM_CTRL_MODE(0) | val;
	hdr_en_mask = HDR_LSI_LN_COM_CTRL_MODE_MASK | HDR_LSI_LN_COM_CTRL_EN_MASK;
	dpp_hdr_write_mask(id, HDR_LSI_LN_COM_CTRL, hdr_en_val, hdr_en_mask);

	/* Compensate Premultiplied Alpha is belong to DPP not HDR */
	premulti_val = (p->blend == DECON_BLENDING_PREMULT) ? ~0 : 0;
	premulti_mask = DPP_DEPREMULTIPLY_EN;
	dpp_write_mask(id, DPP_IN_CON, premulti_val, premulti_mask);

	val = (p->hdr_params.hdr_en == true) ? ~0 : 0;
	if (p->hdr_params.eotf_en == true)
		mask |= HDR_LSI_LN_MOD_CTRL_EEN;
	if (p->hdr_params.gm_en == true)
		mask |= HDR_LSI_LN_MOD_CTRL_GEN;
	if (p->hdr_params.tm_en == true)
		mask |= HDR_LSI_LN_MOD_CTRL_TEN;
	if (p->hdr_params.oetf_en == true)
		mask |= HDR_LSI_LN_MOD_CTRL_OEN;
	dpp_hdr_write_mask(id, HDR_LSI_LN_MOD_CTRL, val, mask);

	if (p->hdr_params.oetf_en == true)
		dpp_reg_set_oetf_lut(id, p);
	if (p->hdr_params.eotf_en == true)
		dpp_reg_set_eotf_lut(id, p);
	if (p->hdr_params.gm_en == true)
		dpp_reg_set_gm_lut(id, p);
	if (p->hdr_params.tm_en == true)
		dpp_reg_set_tm_lut(id, p);
}

/****************** WB MUX CAL functions ******************/
/* EMPTY */

/********** IDMA and ODMA combination CAL functions **********/
/*
 * Y8 : Y8 or RGB base, AFBC or SBWC-Y header
 * C8 : C8 base,        AFBC or SBWC-Y payload
 * Y2 : (SBWC disable - Y2 base), (SBWC enable - C header)
 * C2 : (SBWC disable - C2 base), (SBWC enable - C payload)
 *
 * PLANE_0_STRIDE : Y-HD (or Y-2B) stride -> yhd_y2_strd
 * PLANE_1_STRIDE : Y-PL (or C-2B) stride -> ypl_c2_strd
 * PLANE_2_STRIDE : C-HD stride           -> chd_strd
 * PLANE_3_STRIDE : C-PL stride           -> cpl_strd
 *
 * [ MFC encoder: buffer for SBWC - similar to 8+2 ]
 * plane[0] fd : Y payload(base addr) + Y header => Y header calc.
 * plane[1] fd : C payload(base addr) + C header => C header calc.
 */
static void dma_reg_set_base_addr(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);

	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		dma_write(id, IDMA_IN_BASE_ADDR_Y8, p->addr[0]);
		if (p->comp_type == COMP_TYPE_AFBC)
			dma_write(id, IDMA_IN_BASE_ADDR_C8, p->addr[0]);
		else
			dma_write(id, IDMA_IN_BASE_ADDR_C8, p->addr[1]);

		if (fmt_info->num_planes == 4) { /* use 4 base addresses */
			dma_write(id, IDMA_IN_BASE_ADDR_Y2, p->addr[2]);
			dma_write(id, IDMA_IN_BASE_ADDR_C2, p->addr[3]);
			dma_write_mask(id, IDMA_SRC_STRIDE_1,
					IDMA_PLANE_0_STRIDE(p->yhd_y2_strd),
					IDMA_PLANE_0_STRIDE_MASK);
			dma_write_mask(id, IDMA_SRC_STRIDE_1,
					IDMA_PLANE_1_STRIDE(p->ypl_c2_strd),
					IDMA_PLANE_1_STRIDE_MASK);

			/* C-stride of SBWC: valid if STRIDE_SEL is enabled */
			dma_write_mask(id, IDMA_SRC_STRIDE_2,
					IDMA_PLANE_2_STRIDE(p->chd_strd),
					IDMA_PLANE_2_STRIDE_MASK);
			dma_write_mask(id, IDMA_SRC_STRIDE_2,
					IDMA_PLANE_3_STRIDE(p->cpl_strd),
					IDMA_PLANE_3_STRIDE_MASK);
		}
	}

	dpp_dbg("dpp%d: base addr 1p(0x%lx) 2p(0x%lx) 3p(0x%lx) 4p(0x%lx)\n", id,
			(unsigned long)p->addr[0], (unsigned long)p->addr[1],
			(unsigned long)p->addr[2], (unsigned long)p->addr[3]);
	if ((p->comp_type == COMP_TYPE_SBWC) || (p->comp_type == COMP_TYPE_SBWCL))
		dpp_dbg("dpp%d: [stride] yh(0x%lx) yp(0x%lx) ch(0x%lx) cp(0x%lx)\n", id,
			(unsigned long)p->yhd_y2_strd, (unsigned long)p->ypl_c2_strd,
			(unsigned long)p->chd_strd, (unsigned long)p->cpl_strd);
}

/********** IDMA, ODMA, DPP and WB MUX combination CAL functions **********/
static void dma_dpp_reg_set_coordinates(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_set_coordinates(id, &p->src);

		if (test_bit(DPP_ATTR_DPP, &attr)) {
			if (p->rot > DPP_ROT_180)
				dpp_reg_set_img_size(id, p->src.h, p->src.w);
			else
				dpp_reg_set_img_size(id, p->src.w, p->src.h);
		}

		if (test_bit(DPP_ATTR_SCALE, &attr))
			dpp_reg_set_scaled_img_size(id, p->dst.w, p->dst.h);
	}
}

static int dma_dpp_reg_set_format(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	u32 dma_fmt;
	u32 alpha_type = 0; /* 0: per-frame, 1: per-pixel */
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);

	if (fmt_info->fmt == DECON_PIXEL_FORMAT_RGB_565 && p->is_comp)
		dma_fmt = IDMA_IMG_FORMAT_BGR565;
	else if (fmt_info->fmt == DECON_PIXEL_FORMAT_BGR_565 && p->is_comp)
		dma_fmt = IDMA_IMG_FORMAT_RGB565;
	else
		dma_fmt = fmt_info->dma_fmt;

	alpha_type = (fmt_info->len_alpha > 0) ? 1 : 0;

	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_set_format(id, dma_fmt);
		if (test_bit(DPP_ATTR_DPP, &attr)) {
			dpp_reg_set_alpha_type(id, alpha_type);
			dpp_reg_set_format(id, fmt_info->dpp_fmt);
		}
	}

	return 0;
}

/******************** EXPORTED DPP CAL APIs ********************/
void dpp_constraints_params(struct dpp_size_constraints *vc,
		struct dpp_img_format *vi, struct dpp_restriction *res)
{
	u32 sz_align = 1;
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(vi->format);

	if (IS_YUV(fmt_info))
		sz_align = 2;

	vc->src_mul_w = res->src_f_w.align * sz_align;
	vc->src_mul_h = res->src_f_h.align * sz_align;
	vc->src_w_min = res->src_f_w.min * sz_align;
	vc->src_w_max = res->src_f_w.max;
	vc->src_h_min = res->src_f_h.min;
	vc->src_h_max = res->src_f_h.max;
	vc->img_mul_w = res->src_w.align * sz_align;
	vc->img_mul_h = res->src_h.align * sz_align;
	vc->img_w_min = res->src_w.min * sz_align;
	vc->img_w_max = res->src_w.max;
	vc->img_h_min = res->src_h.min * sz_align;
	if (vi->rot > DPP_ROT_180)
		vc->img_h_max = res->src_h_rot_max;
	else
		vc->img_h_max = res->src_h.max;
	vc->src_mul_x = res->src_x_align * sz_align;
	vc->src_mul_y = res->src_y_align * sz_align;

	vc->sca_w_min = res->dst_w.min;
	vc->sca_w_max = res->dst_w.max;
	vc->sca_h_min = res->dst_h.min;
	vc->sca_h_max = res->dst_h.max;
	vc->sca_mul_w = res->dst_w.align;
	vc->sca_mul_h = res->dst_h.align;

	vc->blk_w_min = res->blk_w.min;
	vc->blk_w_max = res->blk_w.max;
	vc->blk_h_min = res->blk_h.min;
	vc->blk_h_max = res->blk_h.max;
	vc->blk_mul_w = res->blk_w.align;
	vc->blk_mul_h = res->blk_h.align;

	if (vi->wb) {
		vc->src_mul_w = res->dst_f_w.align * sz_align;
		vc->src_mul_h = res->dst_f_h.align * sz_align;
		vc->src_w_min = res->dst_f_w.min;
		vc->src_w_max = res->dst_f_w.max;
		vc->src_h_min = res->dst_f_h.min;
		vc->src_h_max = res->dst_f_h.max;
		vc->img_mul_w = res->dst_w.align * sz_align;
		vc->img_mul_h = res->dst_h.align * sz_align;
		vc->img_w_min = res->dst_w.min;
		vc->img_w_max = res->dst_w.max;
		vc->img_h_min = res->dst_h.min;
		vc->img_h_max = res->dst_h.max;
		vc->src_mul_x = res->dst_x_align * sz_align;
		vc->src_mul_y = res->dst_y_align * sz_align;
	}
}

void dpp_reg_init(u32 id, const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_set_irq_mask_all(id, 0);
		idma_reg_set_irq_enable(id);
		idma_reg_set_in_qos_lut(id, 0, 0x44444444);
		idma_reg_set_in_qos_lut(id, 1, 0x44444444);
		/* TODO: clock gating will be enabled */
		idma_reg_set_sram_clk_gate_en(id, 0);
		idma_reg_set_dynamic_gating_en_all(id, 0);
		idma_reg_set_pixel_alpha(id, 0xFF);
		idma_reg_set_in_ic_max(id, 0x20);
		idma_reg_set_assigned_mo(id, 0x20);
		/* to prevent irq storm that may occur in the OFF STATE */
		idma_reg_clear_irq(id, IDMA_ALL_IRQ_CLEAR);
	}

	if (test_bit(DPP_ATTR_DPP, &attr)) {
		dpp_reg_set_irq_mask_all(id, 0);
		dpp_reg_set_irq_enable(id);
		dpp_reg_set_clock_gate_en_all(id, 0);
		dpp_reg_set_dynamic_gating_en_all(id, 0);
		dpp_reg_set_linecnt(id, 1);
		/* to prevent irq storm that may occur in the OFF STATE */
		dpp_reg_clear_irq(id, DPP_ALL_IRQ_CLEAR);
	}
}

int dpp_reg_deinit(u32 id, bool reset, const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_clear_irq(id, IDMA_ALL_IRQ_CLEAR);
		idma_reg_set_irq_mask_all(id, 1);
	}

	if (test_bit(DPP_ATTR_DPP, &attr)) {
		dpp_reg_clear_irq(id, DPP_ALL_IRQ_CLEAR);
		dpp_reg_set_irq_mask_all(id, 1);
	}

	if (reset) {
		if (test_bit(DPP_ATTR_IDMA, &attr) &&
				!test_bit(DPP_ATTR_DPP, &attr)) { /* IDMA only */
			idma_reg_set_sw_reset(id);
			if (idma_reg_wait_sw_reset_status(id))
				return -1;
		} else if (test_bit(DPP_ATTR_IDMA, &attr) &&
				test_bit(DPP_ATTR_DPP, &attr)) { /* IDMA/DPP */
			idma_reg_set_sw_reset(id);
			dpp_reg_set_sw_reset(id);
			if (idma_reg_wait_sw_reset_status(id) ||
					dpp_reg_wait_sw_reset_status(id))
				return -1;
		} else {
			dpp_err("%s: not support attribute case(0x%lx)\n",
					__func__, attr);
		}
	}

	return 0;
}

#if defined(DMA_BIST)
u32 pattern_data[] = {
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0xffffffff,
	0x000000ff,
	0x000000ff,
	0x000000ff,
	0x000000ff,
};
#endif

void dpp_reg_configure_params(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	int deadlock_cnt = 0;
	u32 size_32x = 2;

	if (test_bit(DPP_ATTR_CSC, &attr) && test_bit(DPP_ATTR_DPP, &attr))
		dpp_reg_set_csc_params(id, p->eq_mode);

	if (test_bit(DPP_ATTR_SCALE, &attr))
		dpp_reg_set_scale_ratio(id, p);

	/* configure coordinates and size of IDMA, DPP, ODMA and WB MUX */
	dma_dpp_reg_set_coordinates(id, p, attr);

	if (test_bit(DPP_ATTR_ROT, &attr) || test_bit(DPP_ATTR_FLIP, &attr))
		idma_reg_set_rotation(id, p->rot);

	/* configure base address of IDMA and ODMA */
	dma_reg_set_base_addr(id, p, attr);

	if (test_bit(DPP_ATTR_BLOCK, &attr))
		idma_reg_set_block_mode(id, p->is_block, p->block.x, p->block.y,
				p->block.w, p->block.h);

	/* configure image format of IDMA, DPP, ODMA and WB MUX */
	dma_dpp_reg_set_format(id, p, attr);

	if (test_bit(DPP_ATTR_HDR, &attr)
		|| test_bit(DPP_ATTR_C_WCG, &attr)
		|| test_bit(DPP_ATTR_HDR10P, &attr)) {
		dpp_reg_set_hdr_params(id, p);
	}

	if (test_bit(DPP_ATTR_AFBC, &attr))
		idma_reg_set_afbc(id, p->comp_type, p->rcv_num);

	if (test_bit(DPP_ATTR_SBWC, &attr)) {
		if (p->comp_type == COMP_TYPE_SBWCL) {
			switch (p->format) {
			case DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L50:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L50:
			case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L40:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L40:
				size_32x = 2;
				break;
			case DECON_PIXEL_FORMAT_NV12M_SBWC_8B_L75:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_8B_L75:
			case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L60:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L60:
				size_32x = 3;
				break;
			case DECON_PIXEL_FORMAT_NV12M_SBWC_10B_L80:
			case DECON_PIXEL_FORMAT_NV12N_SBWC_10B_L80:
				size_32x = 4;
				break;
			default:
				dpp_err("dpp%d idma lossy sbwc config error occur\n", id);
				break;
			}
			idma_reg_set_sbwcl(id, p->comp_type, size_32x, p->rcv_num);
		} else
			idma_reg_set_sbwc(id, p->comp_type, p->rcv_num);
	}

	/*
	 * To check HW stuck
	 * dead_lock min: 17ms (17ms: 1-frame time, rcv_time: 1ms)
	 * but, considered DVFS 3x level switch (ex: 200 <-> 600 Mhz)
	 */

	deadlock_cnt = p->rcv_num * 51;
	dpp_dbg("idma deadlock_cnt(%d)\n", deadlock_cnt);
	/* TODO : deadlock is not set for bring up */
	idma_reg_set_deadlock(id, 1, deadlock_cnt);

#if defined(DMA_BIST)
	idma_reg_set_test_pattern(id, 0, pattern_data);
#endif
}

u32 dpp_reg_get_irq_and_clear(u32 id)
{
	u32 val, cfg_err;

	val = dpp_read(id, DPP_IRQ);
	if (val & DPP_CONFIG_ERROR) {
		cfg_err = dpp_read(id, DPP_CFG_ERR_STATE);
		dpp_err("dpp%d config error occur(0x%x)\n", id, cfg_err);
	}
	dpp_reg_clear_irq(id, val);

	return val;
}

/*
 * CFG_ERR is cleared when clearing pending bits
 * So, get cfg_err first, then clear pending bits
 */
u32 idma_reg_get_irq_and_clear(u32 id)
{
	u32 val, cfg_err;

	val = dma_read(id, IDMA_IRQ);
	if (val & IDMA_CONFIG_ERROR) {
		cfg_err = dma_read(id, IDMA_CFG_ERR_STATE);
		dpp_err("dpp%d idma config error occur(0x%x)\n", id, cfg_err);
	}
	idma_reg_clear_irq(id, val);

	return val;
}

void dma_reg_get_shd_addr(u32 id, u32 shd_addr[], const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		shd_addr[0] = dma_read(id, IDMA_IN_BASE_ADDR_Y8 + DMA_SHD_OFFSET);
		shd_addr[1] = dma_read(id, IDMA_IN_BASE_ADDR_C8 + DMA_SHD_OFFSET);
		shd_addr[2] = dma_read(id, IDMA_IN_BASE_ADDR_Y2 + DMA_SHD_OFFSET);
		shd_addr[3] = dma_read(id, IDMA_IN_BASE_ADDR_C2 + DMA_SHD_OFFSET);
	}
	dpp_dbg("dpp%d: shadow addr 1p(0x%x) 2p(0x%x) 3p(0x%x) 4p(0x%x)\n",
			id, shd_addr[0], shd_addr[1], shd_addr[2], shd_addr[3]);
}

static void dpp_reg_dump_ch_data(int id, enum dpp_reg_area reg_area,
		u32 sel[], u32 cnt)
{
	unsigned char linebuf[128] = {0, };
	int i, ret;
	int len = 0;
	u32 data;

	for (i = 0; i < cnt; i++) {
		if (!(i % 4) && i != 0) {
			linebuf[len] = '\0';
			len = 0;
			dpp_info("%s\n", linebuf);
		}

		if (reg_area == REG_AREA_DPP) {
			dpp_write(id, 0xC04, sel[i]);
			data = dpp_read(id, 0xC10);
		} else if (reg_area == REG_AREA_DMA) {
			dma_write(id, IDMA_DEBUG_CONTROL,
					IDMA_DEBUG_CONTROL_SEL(sel[i]) |
					IDMA_DEBUG_CONTROL_EN);
			data = dma_read(id, IDMA_DEBUG_DATA);
		} else { /* REG_AREA_DMA_COM */
			dma_com_write(0, DPU_DMA_DEBUG_CONTROL,
					DPU_DMA_DEBUG_CONTROL_SEL(sel[i]) |
					DPU_DMA_DEBUG_CONTROL_EN);
			data = dma_com_read(0, DPU_DMA_DEBUG_DATA);
		}

		ret = snprintf(linebuf + len, sizeof(linebuf) - len,
				"[0x%08x: %08x] ", sel[i], data);
		if (ret >= sizeof(linebuf) - len) {
			dpp_err("overflow: %d %ld %d\n",
					ret, sizeof(linebuf), len);
			return;
		}
		len += ret;
	}
	dpp_info("%s\n", linebuf);
}
static bool checked;

static void dma_reg_dump_com_debug_regs(int id)
{
	u32 sel_glb[99] = {
		0x0000, 0x0001, 0x0005, 0x0009, 0x000D, 0x000E, 0x0020, 0x0021,
		0x0025, 0x0029, 0x002D, 0x002E, 0x0040, 0x0041, 0x0045, 0x0049,
		0x004D, 0x004E, 0x0060, 0x0061, 0x0065, 0x0069, 0x006D, 0x006E,
		0x0080, 0x0081, 0x0082, 0x0083, 0x00C0, 0x00C1, 0x00C2, 0x00C3,
		0x0100, 0x0101, 0x0200, 0x0201, 0x0202, 0x0300, 0x0301, 0x0302,
		0x0303, 0x0304, 0x0400, 0x4000, 0x4001, 0x4005, 0x4009, 0x400D,
		0x400E, 0x4020, 0x4021, 0x4025, 0x4029, 0x402D, 0x402E, 0x4040,
		0x4041, 0x4045, 0x4049, 0x404D, 0x404E, 0x4060, 0x4061, 0x4065,
		0x4069, 0x406D, 0x406E, 0x4100, 0x4101, 0x4200, 0x4201, 0x4300,
		0x4301, 0x4302, 0x4303, 0x4304, 0x4400, 0x8080, 0x8081, 0x8082,
		0x8083, 0x80C0, 0x80C1, 0x80C2, 0x80C3, 0x8100, 0x8101, 0x8201,
		0x8202, 0x8300, 0x8301, 0x8302, 0x8303, 0x8304, 0x8400, 0xC000,
		0xC001, 0xC002, 0xC005
	};

	dpp_info("%s: checked = %d\n", __func__, checked);
	if (checked)
		return;

	dpp_info("-< DMA COMMON DEBUG SFR >-\n");
	dpp_reg_dump_ch_data(id, REG_AREA_DMA_COM, sel_glb, 99);

	checked = true;
}

static void dma_reg_dump_debug_regs(int id)
{
	u32 sel_layer_012[14] = {
		0x0000, 0x0001, 0x0002, 0x0003, 0x0007, 0x0008, 0x0100, 0x0101,
		0x0102, 0x0103, 0x7000, 0x7001, 0x7002, 0x7003
	};

	u32 sel_layer_3[52] = {
		0x0000, 0x0001, 0x0002, 0x0003, 0x0007, 0x0008, 0x0100, 0x0101,
		0x0102, 0x0103, 0x1000, 0x1001, 0x1002, 0x1003, 0x1007, 0x1008,
		0x1100, 0x1101, 0x1102, 0x1103, 0x2000, 0x2001, 0x2002, 0x2003,
		0x2007, 0x2008, 0x2100, 0x2101, 0x2102, 0x2103, 0x3000, 0x3001,
		0x3002, 0x3003, 0x3007, 0x3008, 0x3100, 0x3101, 0x3102, 0x3103,
		0x4000, 0x4001, 0x4002, 0x4003, 0x4004, 0x4005, 0x4006, 0x4007,
		0x7000, 0x7001, 0x7002, 0x7003
	};

	dpp_info("-< DPU_DMA%d DEBUG SFR >-\n", id);
	if (id == 0 || id == 1 || id == 2)
		dpp_reg_dump_ch_data(id, REG_AREA_DMA, sel_layer_012, 14);
	else /* (id == 3) */
		dpp_reg_dump_ch_data(id, REG_AREA_DMA, sel_layer_3, 52);
}

static void dpp_reg_dump_debug_regs(int id)
{
	u32 sel_gf[3] = {0x0000, 0x0100, 0x0101};
	u32 sel_vgs_vgrfs[37] = {0x0000, 0x0100, 0x0101, 0x0200, 0x0201, 0x0210,
		0x0211, 0x0220, 0x0221, 0x0230, 0x0231, 0x0240, 0x0241, 0x0250,
		0x0251, 0x0300, 0x0301, 0x0302, 0x0303, 0x0304, 0x0305, 0x0306,
		0x0307, 0x0308, 0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0500,
		0x0501, 0x0502, 0x0503, 0x0504, 0x0505, 0x0600, 0x0601};
	u32 cnt;
	u32 *sel = NULL;

	if (id == 0 || id == 1 || id == 2) { /* GF0, GF1 */
		sel =  sel_gf;
		cnt = 3;
	} else if (id == 3) { /* VGS, VGRFS */
		sel = sel_vgs_vgrfs;
		cnt = 37;
	} else {
		dpp_err("DPP%d is wrong ID\n", id);
		return;
	}

	dpp_write(id, 0x0C00, 0x1);
	dpp_info("-< DPP%d DEBUG SFR >-\n", id);
	dpp_reg_dump_ch_data(id, REG_AREA_DPP, sel, cnt);
}

#define PREFIX_LEN	40
#define ROW_LEN		32
static void dpp_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	int i, row;

	for (i = 0; i < len; i += ROW_LEN) {
		p = buf - regs + i;

		if (len - i < ROW_LEN)
			row = len - i;
		else
			row = ROW_LEN;

		snprintf(prefix_buf, sizeof(prefix_buf), "[%08lX] ", p);
		print_hex_dump(KERN_NOTICE, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

static void dma_dump_regs(u32 id, void __iomem *dma_regs)
{
	dpp_info("\n=== DPU_DMA%d SFR DUMP ===\n", id);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0000, 0x74);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0100, 0x44);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0200, 0x8);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0300, 0x24);

	dpp_info("=== DPU_DMA%d SHADOW SFR DUMP ===\n", id);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0800, 0x74);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0900, 0x44);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0A00, 0x8);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0B00, 0x24);
	/* config_err_status */
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0B30, 0x4);
}

static void dpp_dump_regs(u32 id, void __iomem *regs, unsigned long attr)
{
	dpp_info("=== DPP%d SFR DUMP ===\n", id);
	dpp_print_hex_dump(regs, regs + 0x0000, 0x4C);
	dpp_print_hex_dump(regs, regs + 0x0A54, 0x4);
	/* shadow */
	dpp_print_hex_dump(regs, regs + 0x0B00, 0x4C);
	/* debug */
	dpp_print_hex_dump(regs, regs + 0x0D00, 0xC);
}

void __dpp_dump(u32 id, void __iomem *regs, void __iomem *dma_regs,
		unsigned long attr)
{
	dma_reg_dump_com_debug_regs(id);

	dma_dump_regs(id, dma_regs);
	dma_reg_dump_debug_regs(id);

	dpp_dump_regs(id, regs, attr);
	dpp_reg_dump_debug_regs(id);

	/* TODO: dump for hdr -> dpp_dump_hdr_regs(id, regs, attr); */
}

static const struct dpu_fmt dpu_cal_formats_list[] = {
	{
		.name = "NV16",
		.fmt = DECON_PIXEL_FORMAT_NV16,
		.dma_fmt = IDMA_IMG_FORMAT_YUV422_2P,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_8P,
		.bpp = 16,
		.padding = 0,
		.bpc = 8,
		.num_planes = 2,
		.num_buffers = 2,
		.num_meta_planes = 0,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV61",
		.fmt = DECON_PIXEL_FORMAT_NV61,
		.dma_fmt = IDMA_IMG_FORMAT_YVU422_2P,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_8P,
		.bpp = 16,
		.padding = 0,
		.bpc = 8,
		.num_planes = 2,
		.num_buffers = 2,
		.num_meta_planes = 0,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV16M_P210",
		.fmt = DECON_PIXEL_FORMAT_NV16M_P210,
		.dma_fmt = IDMA_IMG_FORMAT_YUV422_P210,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_P210,
		.bpp = 20,
		.padding = 12,
		.bpc = 10,
		.num_planes = 2,
		.num_buffers = 2,
		.num_meta_planes = 1,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV61M_P210",
		.fmt = DECON_PIXEL_FORMAT_NV61M_P210,
		.dma_fmt = IDMA_IMG_FORMAT_YVU422_P210,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_P210,
		.bpp = 20,
		.padding = 12,
		.bpc = 10,
		.num_planes = 2,
		.num_buffers = 2,
		.num_meta_planes = 1,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV16M_S10B",
		.fmt = DECON_PIXEL_FORMAT_NV16M_S10B,
		.dma_fmt = IDMA_IMG_FORMAT_YUV422_8P2,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_8P2,
		.bpp = 20,
		.padding = 0,
		.bpc = 10,
		.num_planes = 4,
		.num_buffers = 2,
		.num_meta_planes = 1,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	}, {
		.name = "NV61M_S10B",
		.fmt = DECON_PIXEL_FORMAT_NV61M_S10B,
		.dma_fmt = IDMA_IMG_FORMAT_YVU422_8P2,
		.dpp_fmt = DPP_IMG_FORMAT_YUV422_8P2,
		.bpp = 20,
		.padding = 0,
		.bpc = 10,
		.num_planes = 4,
		.num_buffers = 2,
		.num_meta_planes = 1,
		.len_alpha = 0,
		.cs = DPU_COLORSPACE_YUV422,
		.ct = COMP_TYPE_NONE,
	},
};

const struct dpu_fmt *dpu_find_cal_fmt_info(enum decon_pixel_format fmt)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dpu_cal_formats_list); i++)
		if (dpu_cal_formats_list[i].fmt == fmt)
			return &dpu_cal_formats_list[i];

	return NULL;
}
