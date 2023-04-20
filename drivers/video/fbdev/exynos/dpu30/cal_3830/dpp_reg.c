/* drivers/video/fbdev/exynos/dpu30/cal_3830/dpp_regs.c
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
#include "../format.h"

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

static void idma_reg_set_dynamic_gating_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	val &= IDMA_DG_EN_MASK;
	dma_write(id, IDMA_DYNAMIC_GATING_EN, val);
}

static void idma_reg_clear_irq(u32 id, u32 irq)
{
	dma_write_mask(id, IDMA_IRQ, ~0, irq);
}

static void idma_reg_set_sw_reset(u32 id)
{
	dma_write_mask(id, IDMA_ENABLE, ~0, IDMA_SRESET);
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
	dma_write_mask(id, IDMA_OUT_CON, IDMA_OUT_FRAME_ALPHA(alpha),
			IDMA_OUT_FRAME_ALPHA_MASK);
}

static void idma_reg_set_flip(u32 id, u32 flip)
{
	dma_write_mask(id, IDMA_IN_CON, IDMA_IN_FLIP(flip), IDMA_IN_FLIP_MASK);
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

static void dpp_reg_set_dynamic_gating_en_all(u32 id, u32 en)
{
	u32 val = en ? DPP_DG_EN_ALL : 0;

	dpp_write(id, DPP_DYNAMIC_GATING_EN, val);
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

static const u16 dpp_reg_csc_y2r_3x3[4][3][3] = {
	/* 00 : BT2020 Limited */
	{
		{0x0254, 0x0000, 0x035B},
		{0x0254, 0xFFA0, 0xFEB3},
		{0x0254, 0x0449, 0x0000},
	},
	/* 01 : BT2020 Full */
	{
		{0x0200, 0x0000, 0x02E2},
		{0x0200, 0xFFAE, 0xFEE2},
		{0x0200, 0x03AE, 0x0000},
	},
	/* 02 : DCI-P3 Limited */
	{
		{0x0254, 0x0000, 0x0399},
		{0x0254, 0xFF98, 0xFEF4},
		{0x0254, 0x043D, 0x0000},
	},
	/* 03 : DCI-P3 Full */
	{
		{0x0200, 0x0000, 0x0317},
		{0x0200, 0xFFA7, 0xFF1A},
		{0x0200, 0x03A4, 0x0000},
	},
};

static void dpp_reg_set_csc_coef(u32 id, u32 csc_std, u32 csc_rng)
{
	u32 val;
	u32 csc_id = CSC_CUSTOMIZED_START; /* CSC_BT601/625/525 */
	u32 c00, c01, c02;
	u32 c10, c11, c12;
	u32 c20, c21, c22;

	if ((csc_std > CSC_DCI_P3) && (csc_std <= CSC_ADOBE_RGB)){
		csc_id = (csc_std - CSC_CUSTOMIZED_START) * 2 + csc_rng;

		c00 = csc_y2r_3x3_t[csc_id][0][0];
		c01 = csc_y2r_3x3_t[csc_id][0][1];
		c02 = csc_y2r_3x3_t[csc_id][0][2];

		c10 = csc_y2r_3x3_t[csc_id][1][0];
		c11 = csc_y2r_3x3_t[csc_id][1][1];
		c12 = csc_y2r_3x3_t[csc_id][1][2];

		c20 = csc_y2r_3x3_t[csc_id][2][0];
		c21 = csc_y2r_3x3_t[csc_id][2][1];
		c22 = csc_y2r_3x3_t[csc_id][2][2];
	} else if ((csc_std >= CSC_BT_2020) && (csc_std <= CSC_DCI_P3)) {
		csc_id = (csc_std % 2) * 2  + csc_rng;

		c00 = dpp_reg_csc_y2r_3x3[csc_id][0][0];
		c01 = dpp_reg_csc_y2r_3x3[csc_id][0][1];
		c02 = dpp_reg_csc_y2r_3x3[csc_id][0][2];

		c10 = dpp_reg_csc_y2r_3x3[csc_id][1][0];
		c11 = dpp_reg_csc_y2r_3x3[csc_id][1][1];
		c12 = dpp_reg_csc_y2r_3x3[csc_id][1][2];


		c20 = dpp_reg_csc_y2r_3x3[csc_id][2][0];
		c21 = dpp_reg_csc_y2r_3x3[csc_id][2][1];
		c22 = dpp_reg_csc_y2r_3x3[csc_id][2][2];
	} else {
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
	}

	val = (DPP_CSC_COEF_H(c01) | DPP_CSC_COEF_L(c00));
	dpp_write(id, DPP_CSC_COEF0, val);

	val = (DPP_CSC_COEF_H(c10) | DPP_CSC_COEF_L(c02));
	dpp_write(id, DPP_CSC_COEF1, val);

	val = (DPP_CSC_COEF_H(c12) | DPP_CSC_COEF_L(c11));
	dpp_write(id, DPP_CSC_COEF2, val);

	val = (DPP_CSC_COEF_H(c21) | DPP_CSC_COEF_L(c20));
	dpp_write(id, DPP_CSC_COEF3, val);

	val = DPP_CSC_COEF_L(c22);
	dpp_write(id, DPP_CSC_COEF4, val);

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
	u32 mode = (type <= CSC_BT_709) ? CSC_COEF_HARDWIRED : CSC_COEF_CUSTOMIZED;
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

static void dpp_reg_set_img_size(u32 id, u32 w, u32 h)
{
	dpp_write(id, DPP_IMG_SIZE, DPP_IMG_HEIGHT(h) | DPP_IMG_WIDTH(w));
}

static void dpp_reg_set_format(u32 id, u32 fmt)
{
	dpp_write_mask(id, DPP_IN_CON, DPP_IMG_FORMAT(fmt), DPP_IMG_FORMAT_MASK);
}

/********** IDMA and ODMA combination CAL functions **********/
/*
 * Y : Y or RGB base
 * C : C base
 *
 */
static void dma_reg_set_base_addr(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);

	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		dma_write(id, IDMA_IN_BASE_ADDR_Y, p->addr[0]);

		if (fmt_info->num_planes == 2) {
			dma_write(id, IDMA_IN_BASE_ADDR_C, p->addr[1]);
		}
	}

	dpp_dbg("dpp%d: base addr 1p(0x%lx) 2p(0x%lx) 3p(0x%lx) 4p(0x%lx)\n", id,
			(unsigned long)p->addr[0], (unsigned long)p->addr[1],
			(unsigned long)p->addr[2], (unsigned long)p->addr[3]);
}

/********** IDMA and DPP combination CAL functions **********/
static void dma_dpp_reg_set_coordinates(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		idma_reg_set_coordinates(id, &p->src);

		if (test_bit(DPP_ATTR_DPP, &attr)) {
			dpp_reg_set_img_size(id, p->src.w, p->src.h);
		}
	}
}

static int dma_dpp_reg_set_format(u32 id, struct dpp_params_info *p,
		const unsigned long attr)
{
	u32 dma_fmt, dpp_fmt;

	const struct dpu_fmt *fmt_info = dpu_find_fmt_info(p->format);

	dma_fmt = fmt_info->dma_fmt;

	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		if (dma_fmt >= IDMA_IMG_FORMAT_MAX) {
			dpp_err(" %s : not supported IDMA image format\n",
				__func__);
			BUG();
		}

		idma_reg_set_format(id, dma_fmt);
		if (test_bit(DPP_ATTR_DPP, &attr)) {
			dpp_fmt = fmt_info->dpp_fmt;

			if(dpp_fmt >= DPP_IMG_FORMAT_MAX) {

				dpp_err(" %s : not supported DPP image format\n",
				__func__);
				BUG();
			}
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
		idma_reg_set_dynamic_gating_en(id, 0);
		idma_reg_set_pixel_alpha(id, 0xFF);
		/* to prevent irq storm that may occur in the OFF STATE */
		idma_reg_clear_irq(id, IDMA_ALL_IRQ_CLEAR);
	}

	if (test_bit(DPP_ATTR_DPP, &attr)) {
		dpp_reg_set_irq_mask_all(id, 0);
		dpp_reg_set_irq_enable(id);
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
	if (test_bit(DPP_ATTR_CSC, &attr) && test_bit(DPP_ATTR_DPP, &attr))
		dpp_reg_set_csc_params(id, p->eq_mode);

	if (test_bit(DPP_ATTR_SCALE, &attr)) {
		dpp_err(" %s : scaling is not supported\n", __func__);
		BUG();
	}

	/* configure coordinates and size of IDMA and DPP */
	dma_dpp_reg_set_coordinates(id, p, attr);

	if (test_bit(DPP_ATTR_ROT, &attr)) {
		dpp_err(" %s : Rotation is not supported\n", __func__);
		BUG();
	}

	if (test_bit(DPP_ATTR_FLIP, &attr))
		idma_reg_set_flip(id, p->rot);

	/* configure base address of IDMA */
	dma_reg_set_base_addr(id, p, attr);

	if (test_bit(DPP_ATTR_BLOCK, &attr))
		idma_reg_set_block_mode(id, p->is_block, p->block.x, p->block.y,
				p->block.w, p->block.h);

	/* configure image format of IDMA and DPP */
	dma_dpp_reg_set_format(id, p, attr);

	if (test_bit(DPP_ATTR_HDR, &attr)) {
		dpp_err(" %s : HDR is not supported\n", __func__);
		BUG();
	}

	if (test_bit(DPP_ATTR_AFBC, &attr)) {
		dpp_err(" %s : AFBC is not supported\n", __func__);
		BUG();
	}

	if (test_bit(DPP_ATTR_SBWC, &attr)) {
		dpp_err(" %s : SBWC is not supported\n", __func__);
		BUG();
	}

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

u32 odma_reg_get_irq_and_clear(u32 id)
{
      return 0;
}

void dma_reg_get_shd_addr(u32 id, u32 shd_addr[], const unsigned long attr)
{
	if (test_bit(DPP_ATTR_IDMA, &attr)) {
		shd_addr[0] = dma_read(id, IDMA_IN_BASE_ADDR_Y + DMA_SHD_OFFSET);
		shd_addr[1] = dma_read(id, IDMA_IN_BASE_ADDR_C + DMA_SHD_OFFSET);
	}
	dpp_dbg("dpp%d: shadow addr 1p(0x%x) 2p(0x%x) 3p(0x%x) 4p(0x%x)\n",
			id, shd_addr[0], shd_addr[1], shd_addr[2], shd_addr[3]);
}

static bool checked;

static void dpp_check_data_glb_dma(void)
{
	u32 data[20] = {0,};
	u32 sel[20] = {
		0x00000001, 0x00010001, 0x00020001, 0x00030001, 0x00040001,
		0x00050001, 0x00060001, 0x00070001, 0x80000001, 0x80010001,
		0x80040001, 0x80050001, 0x80060001, 0x80080001, 0x80090001,
		0x800a0001, 0x800b0001, 0x800c0001, 0x800d0001, 0x800e0001};
	int i;

	if (checked)
		return;

	dpp_info("-< DPU_DMA_DATA >-\n");
	for (i = 0; i < 20; i++) {
		dma_com_write(0, DPU_DMA_DEBUG_CONTROL, sel[i]);
		/* dummy dpp data read */
		/* temp code */
		dpp_read(0, DPP_CFG_ERR_STATE);
		data[i] = dma_com_read(0, DPU_DMA_DEBUG_DATA);

		dpp_info("[0x%08x: %08x]\n", sel[i], data[i]);
	}
	checked = 1;
}

static void dpp_check_data_g_ch(int id)
{
	u32 data[6] = {0,};
	u32 sel[6] = {
		0x00000001, 0x00010001, 0x00040001, 0x10000001, 0x10010001,
		0x10020001};
	int i;

	dpp_info("-< IDMA%d_G_CH_DATA >-\n", id);
	for (i = 0; i < 6; i++) {
		dma_write(id, IDMA_DEBUG_CONTROL, sel[i]);
		/* dummy dpp data read */
		/* temp code */
		data[i] = dma_read(id, IDMA_DEBUG_DATA);

		dpp_info("[0x%08x: %08x]\n", sel[i], data[i]);
	}
}

static void dpp_check_data_vg_ch(int id)
{
	u32 data[12] = {0, };
	u32 sel[12] = {
		0x00000001, 0x00010001, 0x00040001, 0x10000001, 0x10010001,
		0x10020001, 0x80000001, 0x80010001, 0x80040001, 0x90000001,
		0x90010001, 0x90020001};
	int i;

	dpp_info("-< IDMA%d_VG_CH_DATA >-\n", id);
	for (i = 0; i < 12; i++) {
		dma_write(id, IDMA_DEBUG_CONTROL, sel[i]);

		/* dummy dpp data read */
		/* temp code */
		dpp_read(id, DPP_CFG_ERR_STATE);
		data[i] = dma_read(id, IDMA_DEBUG_DATA);

		dpp_info("[0x%08x: %08x]\n", sel[i], data[i]);
	}
}

static void dma_reg_dump_debug_regs(int id)
{
	dpp_check_data_glb_dma();

	switch (id) {
		case IDMA_G1:
		case IDMA_G2:
		case IDMA_G0:
			dpp_check_data_g_ch(id);
			break;
		case IDMA_VG0:
			dpp_check_data_vg_ch(id);
			break;
		default:
			break;
	}
}

static void dpp_reg_dump_debug_regs(int id)
{
	u32 data[17] = {0,};
	u32 sel[17] = {
		0x00000000, 0x00000100, 0x00000200, 0x00000300, 0x00000400,
		0x00000500,
		0x00000101, 0x00000102, 0x00000103, 0x00000104, 0x00000105,
		0x00000106, 0x00000107, 0x00000108, 0x00000109, 0x0000010A,
		0x0000010B};
	int i;

	dpp_info("-< DPP%d_CH_DATA >-\n", id);
	for (i = 0; i < 17; i++) {
		dpp_write(id, DPP_DEBUG_SEL, sel[i]);
		data[i] = dpp_read(id, DPP_DEBUG_DATA);

		dpp_info("[0x%08x: %08x]\n", sel[i], data[i]);
	}
}

#define PREFIX_LEN      40
#define ROW_LEN         32
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

static void dma_reg_dump_com_debug_regs(int id)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	struct dpp_device *dpp0 = get_dpp_drvdata(0);

	dma_write(dpp->id, 0x0060, 0x1);
	dpp_info("=== DPU_DMA GLB SFR DUMP ===\n");
	dpp_print_hex_dump(dpp0->res.dma_com_regs, dpp0->res.dma_com_regs + 0x0000,
			0x108);
}

static void dma_dump_regs(u32 id, void __iomem *dma_regs)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);

	dpp_info("=== DPU_DMA%d SFR DUMP ===\n", dpp->id);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0000, 0x68);

	dpp_info("=== DPU_DMA%d SHADOW SFR DUMP ===\n", dpp->id);
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0800, 0x68);

	/* config_err_status */
	dpp_print_hex_dump(dma_regs, dma_regs + 0x0870, 0x4);
}

static void dpp_dump_regs(u32 id, void __iomem *regs, unsigned long attr)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);

	dpp_write(id, 0x0B00, 0x1);
	dpp_write(id, 0x0C00, 0x1);
	dpp_info("=== DPP%d SFR DUMP ===\n", dpp->id);

	/* Normal */
	dpp_print_hex_dump(regs, regs + 0x0000, 0x4C);
	/* Dynamic gating */
	dpp_print_hex_dump(regs, regs + 0x0A54, 0x4);
	/* Shadow */
	dpp_print_hex_dump(regs, regs + 0x0B00, 0x44);
	/* Line cnt, Err state */
	dpp_print_hex_dump(regs, regs + 0x0D00, 0xC);
}

void __dpp_dump(u32 id, void __iomem *regs, void __iomem *dma_regs,
		unsigned long attr)
{
	dma_reg_dump_com_debug_regs(id);

	dma_dump_regs(id, dma_regs);
	dma_reg_dump_debug_regs(id);

	if (test_bit(DPP_ATTR_DPP, &attr)) {
		dpp_dump_regs(id, regs, attr);
		dpp_reg_dump_debug_regs(id);
	}
}

const struct dpu_fmt *dpu_find_cal_fmt_info(enum decon_pixel_format fmt)
{
/*
 * This product doesn't support any other format except basic fotmat.
 * If this function is removed, should modify dpp_drv.c file.
 * dpp_drv.c file is common code for exynos. so we leave this function
 * to preserve dpp_drv.c file.
 */
	return NULL;
}
