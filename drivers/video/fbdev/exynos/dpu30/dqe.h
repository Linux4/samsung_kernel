/* linux/drivers/video/fbdev/exynos/dpu/dqe_common.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DQE_H__
#define __SAMSUNG_DQE_H__

#include "decon.h"
#if defined(CONFIG_SOC_EXYNOS9830)
#include "./cal_9830/regs-dqe.h"
#endif

#define dqe_err(fmt, ...)							\
	do {									\
		if (dqe_log_level >= 3) {					\
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dqe_warn(fmt, ...)							\
	do {									\
		if (dqe_log_level >= 4) {					\
			pr_warn(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dqe_info(fmt, ...)							\
	do {									\
		if (dqe_log_level >= 6)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define dqe_dbg(fmt, ...)							\
	do {									\
		if (dqe_log_level >= 7)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

static inline u32 dqe_read(u32 reg_id)
{
	struct decon_device *decon = get_decon_drvdata(0);

	return readl(decon->res.regs + DQE_BASE + reg_id);
}

static inline u32 dqe_read_mask(u32 reg_id, u32 mask)
{
	u32 val = dqe_read(reg_id);

	val &= (mask);
	return val;
}

static inline void dqe_write(u32 reg_id, u32 val)
{
	struct decon_device *decon = get_decon_drvdata(0);

	writel(val, decon->res.regs + DQE_BASE + reg_id);
}

static inline void dqe_write_mask(u32 reg_id, u32 val, u32 mask)
{
	struct decon_device *decon = get_decon_drvdata(0);
	u32 old = dqe_read(reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, decon->res.regs + DQE_BASE + reg_id);
}

static inline bool IS_DQE_OFF_STATE(struct decon_device *decon)
{
	return decon == NULL ||
		decon->state == DECON_STATE_OFF ||
		decon->state == DECON_STATE_INIT;
}

struct dqe_reg_dump {
	u32 addr;
	u32 val;
};

struct dqe_ctx {
	struct dqe_reg_dump cgc[DQECGCLUT_MAX];
	struct dqe_reg_dump gamma[DQEGAMMALUT_MAX];
	struct dqe_reg_dump hsc[DQEHSCLUT_MAX];
	struct dqe_reg_dump aps[DQEAPSLUT_MAX];
	u32 cgc_on;
	u32 gamma_on;
	u32 hsc_on;
	u32 aps_on;
	u32 aps_lux;
	bool need_udpate;
	u32 color_mode;
	u32 night_light_on;
	u32 boosted_on;
};

struct dqe_device {
	struct device *dev;
	struct decon_device *decon;
	struct mutex lock;
	struct mutex restore_lock;
	struct dqe_ctx ctx;
};

extern int dqe_log_level;

/* CAL APIs list */
void dqe_reg_start(u32 id, struct exynos_panel_info *lcd_info);
void dqe_reg_stop(u32 id);

void dqe_reg_set_dqecon_all_reset(void);
void dqe_reg_set_cgc_on(u32 on);
u32 dqe_reg_get_cgc_on(void);
void dqe_reg_set_gamma_on(u32 on);
u32 dqe_reg_get_gamma_on(void);
void dqe_reg_set_hsc_on(u32 on);
u32 dqe_reg_get_hsc_on(void);
void dqe_reg_hsc_sw_reset(struct decon_device *decon);
void dqe_reg_set_hsc_full_pxl_num(struct exynos_panel_info *lcd_info);
u32 dqe_reg_get_hsc_full_pxl_num(void);
void dqe_reg_set_aps_on(u32 on);
u32 dqe_reg_get_aps_on(void);
void dqe_reg_set_aps_full_pxl_num(struct exynos_panel_info *lcd_info);
u32 dqe_reg_get_aps_full_pxl_num(void);
void dqe_reg_set_aps_img_size(struct exynos_panel_info *lcd_info);

int dqe_save_context(void);
int dqe_restore_context(void);
void decon_dqe_sw_reset(struct decon_device *decon);
void decon_dqe_enable(struct decon_device *decon);
void decon_dqe_disable(struct decon_device *decon);
int decon_dqe_create_interface(struct decon_device *decon);

int decon_dqe_set_color_mode(struct decon_color_mode_with_render_intent_info *color_mode);
int decon_dqe_set_color_transform(struct decon_color_transform_info *transform);

/* APS GAMMA */
#define APS_GAMMA_BIT_LENGTH			(8)
#define APS_GAMMA_BIT				(8)
#define APS_GAMMA_PHASE_SHIFT			(APS_GAMMA_BIT - 6)
#define APS_GAMMA_INPUT_MASK			(0x3f << APS_GAMMA_PHASE_SHIFT)
#define APS_GAMMA_PHASE_MASK			((1 << APS_GAMMA_PHASE_SHIFT)-1)
#define APS_GAMMA_PIXEL_MAX			((1 << APS_GAMMA_BIT)-1)
#define APS_GAMMA_INTP_LIMIT			(0x7 << APS_GAMMA_BIT)
#define APS_GAMMA_DEFAULT_VALUE_ALIGN		(APS_GAMMA_BIT-8)

#endif
