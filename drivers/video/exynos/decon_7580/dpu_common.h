/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_DPU_H__
#define __SAMSUNG_DPU_H__


#define DPU_VFP		4000
#define DPU_VSA		0
#define DPU_VBP		0
#define DPU_HFP		0
#define DPU_HSA		0
#define DPU_HBP		0

#include "decon.h"
#include "regs-dpu.h"
#define DPU_BASE 0x4000
#define DPU_GAMMA_OFFSET	132

extern const u32 gamma_table1[][65];
extern const u32 gamma_table2[][65];
extern const u32 gamma_table3[][65];

struct dpu_reg_dump {
	u32	addr;
	u32	val;
};

static inline u32 dpu_read(u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(0);
	return readl(decon->regs + DPU_BASE + reg_id);
}

static inline u32 dpu_read_mask(u32 reg_id, u32 mask)
{
	u32 val = dpu_read(reg_id);
	val &= (~mask);
	return val;
}

static inline void dpu_write(u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(0);
	writel(val, decon->regs + DPU_BASE + reg_id);
}

static inline void dpu_write_mask(u32 reg_id, u32 val, u32 mask)
{
	struct decon_device *decon = get_decon_drvdata(0);
	u32 old = dpu_read(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, decon->regs + DPU_BASE + reg_id);
}

void dpu_reg_start(u32 w, u32 h);
void dpu_reg_stop(void);

#if defined(CONFIG_EXYNOS_DECON_DPU)
void dpu_reg_set_scr_onoff(u32 val);
void dpu_reg_set_scr_r(u32 mask, u32 val);
void dpu_reg_set_scr_g(u32 mask, u32 val);
void dpu_reg_set_scr_b(u32 mask, u32 val);
void dpu_reg_set_scr_c(u32 mask, u32 val);
void dpu_reg_set_scr_m(u32 mask, u32 val);
void dpu_reg_set_scr_y(u32 mask, u32 val);
void dpu_reg_set_scr_w(u32 mask, u32 val);
void dpu_reg_set_scr_k(u32 mask, u32 val);
void dpu_reg_set_saturation_onoff(u32 val);
void dpu_reg_set_saturation_tscgain(u32 val);
void dpu_reg_set_saturation_rgb(u32 mask, u32 val);
void dpu_reg_set_saturation_cmy(u32 mask, u32 val);
void dpu_reg_set_saturation_shift(u32 mask, u32 val);
void dpu_reg_set_gamma_onoff(u32 val);
void dpu_reg_set_gamma(u32 offset, u32 mask, u32 val);
void dpu_reg_set_hue_onoff(u32 val);
void dpu_reg_set_hue(u32 val);
void dpu_reg_set_hue_rgb(u32 mask, u32 val);
void dpu_reg_set_hue_cmy(u32 mask, u32 val);
void dpu_reg_set_bright(u32 val);
void dpu_reg_save(void);
void dpu_reg_restore(void);
void dpu_reg_gamma_init(void);
#endif

#endif
