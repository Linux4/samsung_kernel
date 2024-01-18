/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos9630 DPP CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DPP_CAL_H__
#define __SAMSUNG_DPP_CAL_H__

#include "../decon.h"
#include "../format.h"

/* dpp_hdr_lut structure is belong to decon.h
 * struct dpp_hdr_lut has below value
 * hdr_en; 	Disable the all hdr/wcg engine
 * oetf_en;	It is depeded on input transfer function
 * oetf_x[33];
 * oetf_y[33];
 * eotf_en;	It is depended on output transfer function
 * eotf_x[129];
 * eotf_y[129];
 * gm_en;	It is related with colorspace of input/output
 * gm_coef[9];
 * tm_en;	It is used for HDR10P only
 * tm_rngx[2];
 * tm_rngy[2];
 * tm_x[33];
 * tm_y[33];
 */

struct dpp_hdr_params {
	unsigned int hdr_en;
	unsigned int eotf_en;
	unsigned int gm_en;
	unsigned int tm_en;
	unsigned int oetf_en;

	unsigned int *oetf_x;
	unsigned int *oetf_y;
	unsigned int *eotf_x;
	unsigned int *eotf_y;
	unsigned int *gm_coef;
	unsigned int *tm_coef;
	unsigned int *tm_rngx;
	unsigned int *tm_rngy;
	unsigned int *tm_x;
	unsigned int *tm_y;
};

struct dpp_params_info {
	struct decon_frame src;
	struct decon_frame dst;
	struct decon_win_rect block;
	u32 rot;

	enum dpp_hdr_standard hdr;
	u32 min_luminance;
	u32 max_luminance;
	u32 yhd_y2_strd;
	u32 ypl_c2_strd;
	u32 chd_strd;
	u32 cpl_strd;
	enum decon_blending blend;

	bool is_comp;
	bool is_scale;
	bool is_block;
	enum decon_pixel_format format;
	dma_addr_t addr[MAX_PLANE_ADDR_CNT];
	enum dpp_csc_eq eq_mode;
	int h_ratio;
	int v_ratio;

	unsigned long rcv_num;
	enum dpp_comp_type comp_type;
	bool is_hdr_tune;
	struct dpp_hdr_params hdr_params;
};

struct dpp_size_constraints {
	u32 src_mul_w;
	u32 src_mul_h;
	u32 src_w_min;
	u32 src_w_max;
	u32 src_h_min;
	u32 src_h_max;
	u32 img_mul_w;
	u32 img_mul_h;
	u32 img_w_min;
	u32 img_w_max;
	u32 img_h_min;
	u32 img_h_max;
	u32 blk_w_min;
	u32 blk_w_max;
	u32 blk_h_min;
	u32 blk_h_max;
	u32 blk_mul_w;
	u32 blk_mul_h;
	u32 src_mul_x;
	u32 src_mul_y;
	u32 sca_w_min;
	u32 sca_w_max;
	u32 sca_h_min;
	u32 sca_h_max;
	u32 sca_mul_w;
	u32 sca_mul_h;
	u32 dst_mul_w;
	u32 dst_mul_h;
	u32 dst_w_min;
	u32 dst_w_max;
	u32 dst_h_min;
	u32 dst_h_max;
	u32 dst_mul_x;
	u32 dst_mul_y;
};

struct dpp_img_format {
	u32 vgr;
	u32 normal;
	u32 rot;
	u32 scale;
	u32 format;
	u32 afbc_en;
	u32 wb;
};

struct dpp_restriction;

/* DPP CAL APIs exposed to DPP driver */
void dpp_reg_init(u32 id, const unsigned long attr);
int dpp_reg_deinit(u32 id, bool reset, const unsigned long attr);
void dpp_reg_configure_params(u32 id, struct dpp_params_info *p,
		const unsigned long attr);
void dpp_constraints_params(struct dpp_size_constraints *vc,
		struct dpp_img_format *vi, struct dpp_restriction *res);

/* DPU_DMA, DPP DEBUG */
void __dpp_dump(u32 id, void __iomem *regs, void __iomem *dma_regs,
		unsigned long attr);

/* DPU_DMA and DPP interrupt handler */
u32 dpp_reg_get_irq_and_clear(u32 id);
u32 idma_reg_get_irq_and_clear(u32 id);
u32 odma_reg_get_irq_and_clear(u32 id);

void dma_reg_get_shd_addr(u32 id, u32 shd_addr[], const unsigned long attr);
const struct dpu_fmt *dpu_find_cal_fmt_info(enum decon_pixel_format fmt);

#endif /* __SAMSUNG_DPP_CAL_H__ */
