// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_color.h"
#include "mtk_log.h"
#include "mtk_dump.h"
#include "mtk_drm_helper.h"
#include "platform/mtk_drm_platform.h"
#include "mtk_disp_pq_helper.h"

#ifdef CONFIG_LEDS_MTK_MODULE
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <linux/leds-mtk.h>
#else
#define mtk_leds_brightness_set(x, y) do { } while (0)
#endif

#define DISP_REG_CCORR_EN (0x000)
#define DISP_REG_CCORR_INTEN                     (0x008)
#define DISP_REG_CCORR_INTSTA                    (0x00C)
#define DISP_REG_CCORR_CFG (0x020)
#define DISP_REG_CCORR_SIZE (0x030)
#define DISP_REG_CCORR_COLOR_OFFSET_0	(0x100)
#define DISP_REG_CCORR_COLOR_OFFSET_1	(0x104)
#define DISP_REG_CCORR_COLOR_OFFSET_2	(0x108)
#define CCORR_COLOR_OFFSET_MASK	(0x3FFFFFF)

#define DISP_REG_CCORR_SHADOW (0x0A0)
#define CCORR_READ_WORKING		BIT(0)
#define CCORR_BYPASS_SHADOW		BIT(2)

#define CCORR_12BIT_MASK				0x0fff
#define CCORR_13BIT_MASK				0x1fff

#define CCORR_INVERSE_GAMMA   (0)
#define CCORR_BYASS_GAMMA      (1)

#define CCORR_REG(idx) (idx * 4 + 0x80)
#define CCORR_CLIP(val, min, max) ((val >= max) ? \
	max : ((val <= min) ? min : val))

// For color conversion bug fix
//static bool need_offset;
#define OFFSET_VALUE (1024)

static struct drm_device *g_drm_dev;

static int disp_ccorr_write_coef_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock);
/* static void ccorr_dump_reg(void); */

enum CCORR_IOCTL_CMD {
	SET_CCORR = 0,
	SET_INTERRUPT,
	BYPASS_CCORR
};

inline struct mtk_disp_ccorr *comp_to_ccorr(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_ccorr, ddp_comp);
}

static void disp_ccorr_multiply_3x3(struct mtk_ddp_comp *comp,
		unsigned int ccorrCoef[3][3], int color_matrix[3][3],
		unsigned int resultCoef[3][3])
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int temp_Result;
	int signedCcorrCoef[3][3];
	int i, j;

	/* convert unsigned 12 bit ccorr coefficient to signed 12 bit format */
	for (i = 0; i < 3; i += 1) {
		DDPINFO("inputCcorrCoef[%d][0-2] = {%d, %d, %d}\n", i,
			ccorrCoef[i][0],
			ccorrCoef[i][1],
			ccorrCoef[i][2]);
	}
	for (i = 0; i < 3; i += 1) {
		DDPINFO("inputcolor_matrix[%d][0-2] = {%d, %d, %d}\n", i,
			color_matrix[i][0],
			color_matrix[i][1],
			color_matrix[i][2]);
	}

	/* convert unsigned 12 bit ccorr coefficient to signed 12 bit format */
	for (i = 0; i < 3; i += 1) {
		for (j = 0; j < 3; j += 1) {
			if (ccorrCoef[i][j] > primary_data->ccorr_max_positive) {
				signedCcorrCoef[i][j] =
					(int)ccorrCoef[i][j] - (primary_data->ccorr_offset_base<<2);
			} else {
				signedCcorrCoef[i][j] =
					(int)ccorrCoef[i][j];
			}
		}
	}

	for (i = 0; i < 3; i += 1) {
		DDPINFO("signedCcorrCoef[%d][0-2] = {%d, %d, %d}\n", i,
			signedCcorrCoef[i][0],
			signedCcorrCoef[i][1],
			signedCcorrCoef[i][2]);
	}

	temp_Result = (int)((signedCcorrCoef[0][0]*color_matrix[0][0] +
		signedCcorrCoef[0][1]*color_matrix[1][0] +
		signedCcorrCoef[0][2]*color_matrix[2][0]) / primary_data->ccorr_offset_base);
	resultCoef[0][0] = CCORR_CLIP(temp_Result, primary_data->ccorr_max_negative,
			primary_data->ccorr_max_positive) & primary_data->ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[0][0]*color_matrix[0][1] +
		signedCcorrCoef[0][1]*color_matrix[1][1] +
		signedCcorrCoef[0][2]*color_matrix[2][1]) / primary_data->ccorr_offset_base);
	resultCoef[0][1] = CCORR_CLIP(temp_Result, primary_data->ccorr_max_negative,
			primary_data->ccorr_max_positive) & primary_data->ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[0][0]*color_matrix[0][2] +
		signedCcorrCoef[0][1]*color_matrix[1][2] +
		signedCcorrCoef[0][2]*color_matrix[2][2]) / primary_data->ccorr_offset_base);
	resultCoef[0][2] = CCORR_CLIP(temp_Result, primary_data->ccorr_max_negative,
			primary_data->ccorr_max_positive) & primary_data->ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[1][0]*color_matrix[0][0] +
		signedCcorrCoef[1][1]*color_matrix[1][0] +
		signedCcorrCoef[1][2]*color_matrix[2][0]) / primary_data->ccorr_offset_base);
	resultCoef[1][0] = CCORR_CLIP(temp_Result, primary_data->ccorr_max_negative,
			primary_data->ccorr_max_positive) & primary_data->ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[1][0]*color_matrix[0][1] +
		signedCcorrCoef[1][1]*color_matrix[1][1] +
		signedCcorrCoef[1][2]*color_matrix[2][1]) / primary_data->ccorr_offset_base);
	resultCoef[1][1] = CCORR_CLIP(temp_Result, primary_data->ccorr_max_negative,
			primary_data->ccorr_max_positive) & primary_data->ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[1][0]*color_matrix[0][2] +
		signedCcorrCoef[1][1]*color_matrix[1][2] +
		signedCcorrCoef[1][2]*color_matrix[2][2]) / primary_data->ccorr_offset_base);
	resultCoef[1][2] = CCORR_CLIP(temp_Result, primary_data->ccorr_max_negative,
			primary_data->ccorr_max_positive) & primary_data->ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[2][0]*color_matrix[0][0] +
		signedCcorrCoef[2][1]*color_matrix[1][0] +
		signedCcorrCoef[2][2]*color_matrix[2][0]) / primary_data->ccorr_offset_base);
	resultCoef[2][0] = CCORR_CLIP(temp_Result, primary_data->ccorr_max_negative,
			primary_data->ccorr_max_positive) & primary_data->ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[2][0]*color_matrix[0][1] +
		signedCcorrCoef[2][1]*color_matrix[1][1] +
		signedCcorrCoef[2][2]*color_matrix[2][1]) / primary_data->ccorr_offset_base);
	resultCoef[2][1] = CCORR_CLIP(temp_Result, primary_data->ccorr_max_negative,
			primary_data->ccorr_max_positive) & primary_data->ccorr_fullbit_mask;

	temp_Result = (int)((signedCcorrCoef[2][0]*color_matrix[0][2] +
		signedCcorrCoef[2][1]*color_matrix[1][2] +
		signedCcorrCoef[2][2]*color_matrix[2][2]) / primary_data->ccorr_offset_base);
	resultCoef[2][2] = CCORR_CLIP(temp_Result, primary_data->ccorr_max_negative,
			primary_data->ccorr_max_positive) & primary_data->ccorr_fullbit_mask;

	for (i = 0; i < 3; i += 1) {
		DDPINFO("resultCoef[%d][0-2] = {0x%x, 0x%x, 0x%x}\n", i,
			resultCoef[i][0],
			resultCoef[i][1],
			resultCoef[i][2]);
	}
}

static int disp_ccorr_color_matrix_to_dispsys(struct mtk_ddp_comp *comp,
		struct drm_device *dev)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int ret = 0;
	struct mtk_drm_private *private = dev->dev_private;

	// All Support 3*4 matrix on drm architecture
	if ((primary_data->disp_ccorr_number == 1) && (primary_data->disp_ccorr_linear&0x01)
		&& (!primary_data->prim_ccorr_force_linear))
		ret = mtk_drm_helper_set_opt_by_name(private->helper_opt,
			"MTK_DRM_OPT_PQ_34_COLOR_MATRIX", 0);
	else
		ret = mtk_drm_helper_set_opt_by_name(private->helper_opt,
			"MTK_DRM_OPT_PQ_34_COLOR_MATRIX", 1);

	return ret;
}

static int disp_ccorr_write_coef_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	struct DRM_DISP_CCORR_COEF_T *ccorr, *multiply_matrix;
	int ret = 0;
	unsigned int temp_matrix[3][3];
	unsigned int cfg_val;
	int i, j;

	if (lock)
		mutex_lock(&primary_data->ccorr_global_lock);

	DDPINFO("%s: %s, aosp ccorr:%d,nonlinear:%d\n", __func__, mtk_dump_comp_str(comp),
		primary_data->disp_aosp_ccorr, primary_data->disp_ccorr_without_gamma);
	/* to avoid different show of dual pipe, pipe1 use pipe0's config data */
	ccorr = primary_data->disp_ccorr_coef;
	if (ccorr == NULL) {
		DDPINFO("%s: %s is not initialized\n", __func__, mtk_dump_comp_str(comp));
		ret = -EFAULT;
		goto ccorr_write_coef_unlock;
	}

	multiply_matrix = &primary_data->multiply_matrix_coef;
	if (((primary_data->prim_ccorr_force_linear && (primary_data->disp_ccorr_linear&0x01)) ||
		(primary_data->prim_ccorr_pq_nonlinear && primary_data->disp_ccorr_linear == 0)) &&
		(primary_data->disp_ccorr_number == 1)) {
		DDPINFO("%s: %s forcelinear\n", __func__, mtk_dump_comp_str(comp));
		disp_ccorr_multiply_3x3(comp, ccorr->coef, primary_data->ccorr_color_matrix,
			temp_matrix);
		disp_ccorr_multiply_3x3(comp, temp_matrix, primary_data->rgb_matrix,
			multiply_matrix->coef);
	} else {
		if (primary_data->disp_aosp_ccorr) {
			DDPINFO("%s: %s aospccorr\n", __func__, mtk_dump_comp_str(comp));
			disp_ccorr_multiply_3x3(comp, ccorr->coef, primary_data->ccorr_color_matrix,
				multiply_matrix->coef);//AOSP multiply
		} else {
			DDPINFO("%s: %s noaospccorr\n", __func__, mtk_dump_comp_str(comp));
			disp_ccorr_multiply_3x3(comp, ccorr->coef, primary_data->rgb_matrix,
				multiply_matrix->coef);//PQ service multiply
		}

		if (primary_data->disp_aosp_ccorr)
			primary_data->disp_aosp_ccorr = false; // restore to default setting
	}
	ccorr = multiply_matrix;

	ccorr->offset[0] = primary_data->disp_ccorr_coef->offset[0];
	ccorr->offset[1] = primary_data->disp_ccorr_coef->offset[1];
	ccorr->offset[2] = primary_data->disp_ccorr_coef->offset[2];

	// For 6885 need to left shift one bit
	if (primary_data->disp_ccorr_caps.ccorr_bit == 12) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				ccorr->coef[i][j] = ccorr->coef[i][j]<<1;
	}

	if (handle == NULL) {
		/* use CPU to write */
		writel(1, comp->regs + DISP_REG_CCORR_EN);
		cfg_val = 0x2 | primary_data->ccorr_relay_value |
			     (primary_data->disp_ccorr_without_gamma << 2) |
				(primary_data->ccorr_8bit_switch << 10);
		writel(cfg_val, comp->regs + DISP_REG_CCORR_CFG);
		writel(((ccorr->coef[0][0] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[0][1] & CCORR_13BIT_MASK),
			comp->regs + CCORR_REG(0));
		writel(((ccorr->coef[0][2] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[1][0] & CCORR_13BIT_MASK),
			comp->regs + CCORR_REG(1));
		writel(((ccorr->coef[1][1] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[1][2] & CCORR_13BIT_MASK),
			comp->regs + CCORR_REG(2));
		writel(((ccorr->coef[2][0] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[2][1] & CCORR_13BIT_MASK),
			comp->regs + CCORR_REG(3));
		writel(((ccorr->coef[2][2] & CCORR_13BIT_MASK) << 16),
			comp->regs + CCORR_REG(4));
		/* Ccorr Offset */
		writel(((ccorr->offset[0] & CCORR_COLOR_OFFSET_MASK) |
			(0x1 << 31)),
			comp->regs + DISP_REG_CCORR_COLOR_OFFSET_0);
		writel(((ccorr->offset[1] & CCORR_COLOR_OFFSET_MASK)),
			comp->regs + DISP_REG_CCORR_COLOR_OFFSET_1);
		writel(((ccorr->offset[2] & CCORR_COLOR_OFFSET_MASK)),
			comp->regs + DISP_REG_CCORR_COLOR_OFFSET_2);
	} else {
		/* use CMDQ to write */

		cfg_val = 0x2 | primary_data->ccorr_relay_value |
				(primary_data->disp_ccorr_without_gamma << 2 |
				(primary_data->ccorr_8bit_switch << 10));

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_CCORR_CFG, cfg_val, ~0);

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(0),
			((ccorr->coef[0][0] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[0][1] & CCORR_13BIT_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(1),
			((ccorr->coef[0][2] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[1][0] & CCORR_13BIT_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(2),
			((ccorr->coef[1][1] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[1][2] & CCORR_13BIT_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(3),
			((ccorr->coef[2][0] & CCORR_13BIT_MASK) << 16) |
			(ccorr->coef[2][1] & CCORR_13BIT_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + CCORR_REG(4),
			((ccorr->coef[2][2] & CCORR_13BIT_MASK) << 16), ~0);
		/* Ccorr Offset */
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_CCORR_COLOR_OFFSET_0,
			(ccorr->offset[0] & CCORR_COLOR_OFFSET_MASK) |
			(0x1 << 31), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_CCORR_COLOR_OFFSET_1,
			(ccorr->offset[1] & CCORR_COLOR_OFFSET_MASK), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_CCORR_COLOR_OFFSET_2,
			(ccorr->offset[2] & CCORR_COLOR_OFFSET_MASK), ~0);
	}

	DDPINFO("%s: finish\n", __func__);
ccorr_write_coef_unlock:
	if (lock)
		mutex_unlock(&primary_data->ccorr_global_lock);

	return ret;
}

static void disp_ccorr_on_start_of_frame(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;

	if (ccorr_data->path_order != 0)
		return;
	if (atomic_read(&primary_data->ccorr_irq_en) == 1 ||
			primary_data->sbd_on) {
		atomic_set(&primary_data->ccorr_get_irq, 1);
		wake_up_interruptible(&primary_data->ccorr_get_irq_wq);
	}
}

static irqreturn_t mtk_disp_ccorr_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_ccorr *ccorr = dev_id;
	struct mtk_ddp_comp *comp = &ccorr->ddp_comp;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned int intsta;
	irqreturn_t ret = IRQ_NONE;

	if (IS_ERR_OR_NULL(ccorr))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get("ccorr_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	mtk_crtc = ccorr->ddp_comp.mtk_crtc;
	if (!mtk_crtc) {
		DDPPR_ERR("%s mtk_crtc is NULL\n", __func__);
		ret = IRQ_NONE;
		goto out;
	}

	intsta = readl(comp->regs + DISP_REG_CCORR_INTSTA);
	writel(intsta & ~0x3, comp->regs + DISP_REG_CCORR_INTSTA);
	ret = IRQ_HANDLED;
out:
	mtk_drm_top_clk_isr_put("ccorr_irq");
	return ret;
}

static int disp_ccorr_wait_irq(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int ret = 0;

	if (atomic_read(&primary_data->ccorr_get_irq) == 0) {
		DDPDBG("%s: wait_event_interruptible ++ ", __func__);
		ret = wait_event_interruptible(primary_data->ccorr_get_irq_wq,
			atomic_read(&primary_data->ccorr_get_irq) == 1);
		DDPDBG("%s: wait_event_interruptible -- ", __func__);
		DDPINFO("%s: get_irq = 1, waken up", __func__);
		DDPINFO("%s: get_irq = 1, ret = %d", __func__, ret);
	} else {
		/* If primary_data->ccorr_get_irq is already set, */
		/* means PQService was delayed */
		DDPINFO("%s: get_irq = 0", __func__);
	}

	atomic_set(&primary_data->ccorr_get_irq, 0);

	return ret;
}

static int disp_pq_copy_backlight_to_user(struct mtk_ddp_comp *comp, int *backlight)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	unsigned long flags;
	int ret = 0;

	/* We assume only one thread will call this function */
	spin_lock_irqsave(&primary_data->pq_bl_change_lock, flags);
	primary_data->pq_backlight_db = primary_data->pq_backlight;

	if (primary_data->old_pq_backlight != primary_data->pq_backlight)
		primary_data->old_pq_backlight = primary_data->pq_backlight;

	spin_unlock_irqrestore(&primary_data->pq_bl_change_lock, flags);

	memcpy(backlight, &primary_data->pq_backlight_db, sizeof(int));

	DDPINFO("%s: %d\n", __func__, ret);

	return ret;
}

void disp_pq_notify_backlight_changed(struct mtk_ddp_comp *comp, int bl_1024)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	unsigned long flags;

	spin_lock_irqsave(&primary_data->pq_bl_change_lock, flags);
	primary_data->old_pq_backlight = primary_data->pq_backlight;
	primary_data->pq_backlight = bl_1024;
	spin_unlock_irqrestore(&primary_data->pq_bl_change_lock, flags);

	if (atomic_read(&primary_data->ccorr_is_init_valid) != 1)
		return;

	DDPINFO("%s: %d\n", __func__, bl_1024);

	if (pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {
		if (primary_data->ccorr_relay_value != 1) {

			mtk_crtc_check_trigger(mtk_crtc, true, true);
			DDPINFO("%s: trigger refresh when backlight changed", __func__);
		}
	} else {
		if (primary_data->old_pq_backlight == 0 || bl_1024 == 0) {

			mtk_crtc_check_trigger(mtk_crtc, true, true);
			DDPINFO("%s: trigger refresh when backlight ON/Off", __func__);
		}
	}
}
EXPORT_SYMBOL(disp_pq_notify_backlight_changed);

static int disp_ccorr_set_coef(
	const struct DRM_DISP_CCORR_COEF_T *user_color_corr,
	struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int ret = 0;
	struct DRM_DISP_CCORR_COEF_T *ccorr, *old_ccorr;

	ccorr = kmalloc(sizeof(struct DRM_DISP_CCORR_COEF_T), GFP_KERNEL);
	if (ccorr == NULL) {
		DDPPR_ERR("%s: no memory\n", __func__);
		return -EFAULT;
	}

	if (user_color_corr == NULL) {
		ret = -EFAULT;
		kfree(ccorr);
	} else {
		memcpy(ccorr, user_color_corr,
			sizeof(struct DRM_DISP_CCORR_COEF_T));

		mutex_lock(&primary_data->ccorr_global_lock);

		old_ccorr = primary_data->disp_ccorr_coef;
		primary_data->disp_ccorr_coef = ccorr;
		DDPINFO("%s: Set module(%s) coef\n", __func__, mtk_dump_comp_str(comp));
		if (primary_data->disp_aosp_ccorr)
			primary_data->disp_aosp_ccorr = false;

		ret = disp_ccorr_write_coef_reg(comp, handle, 0);

		mutex_unlock(&primary_data->ccorr_global_lock);

		if (old_ccorr != NULL)
			kfree(old_ccorr);

		//mtk_crtc_check_trigger(comp->mtk_crtc, false, false);
	}

	return ret;
}

static int mtk_disp_ccorr_set_interrupt(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int enabled = *((int *)data);
	int ret = 0;

	if (enabled) {
		if (ccorr_data->path_order == 0)
			atomic_set(&primary_data->ccorr_irq_en, 1);
		DDPINFO("%s: Interrupt enabled\n", __func__);
	} else {
		/* Disable output frame end interrupt */
		if (ccorr_data->path_order == 0)
			atomic_set(&primary_data->ccorr_irq_en, 0);
		DDPINFO("%s: Interrupt disabled\n", __func__);
	}
	return ret;
}

/*
 * ret
 *     0 success
 *     1 skip for special case(disp_ccorr_number = 1)
 *     2 skip for linear setting
 *     -EFAULT error
 */
int disp_ccorr_set_color_matrix(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
	int32_t matrix[16], int32_t hint, bool fte_flag, bool linear)
{
	int i, j;
	int ccorr_without_gamma = 0;
	bool need_refresh = false;
	bool identity_matrix = true;
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	struct DRM_DISP_CCORR_COEF_T *ccorr;

	if (handle == NULL) {
		DDPPR_ERR("%s: cmdq can not be NULL\n", __func__);
		return -EFAULT;
	}

	if (identity_matrix && (primary_data->disp_ccorr_number == 1) &&
			(!primary_data->prim_ccorr_force_linear) &&
			(primary_data->disp_ccorr_linear & 0x01)) {
		DDPINFO("%s, skip aosp ccorr setting for single ccorr\n", __func__);
		return 1;
	}

	if (ccorr_data->is_linear != linear)
		return 2;

	if (primary_data->disp_ccorr_coef == NULL) {
		ccorr = kmalloc(sizeof(struct DRM_DISP_CCORR_COEF_T), GFP_KERNEL);
		if (ccorr == NULL) {
			DDPPR_ERR("%s: no memory\n", __func__);
			return -EFAULT;
		}
		primary_data->disp_ccorr_coef = ccorr;
	}

	mutex_lock(&primary_data->ccorr_global_lock);


	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			/* Copy Color Matrix */
			primary_data->ccorr_color_matrix[i][j] = matrix[j*4 + i];

			/* early jump out */
			if (ccorr_without_gamma == 1)
				continue;

			if (i == j && primary_data->ccorr_color_matrix[i][j] !=
					primary_data->ccorr_offset_base) {
				ccorr_without_gamma = 1;
				identity_matrix = false;
			} else if (i != j && primary_data->ccorr_color_matrix[i][j] != 0) {
				ccorr_without_gamma = 1;
				identity_matrix = false;
			}
		}
	}

	DDPINFO("%s, ccorr linear:%d\n", __func__, linear);
	if (linear)
		ccorr_without_gamma = 0;
	else
		ccorr_without_gamma = 1;

	// hint: 0: identity matrix; 1: arbitraty matrix
	// fte_flag: true: gpu overlay && hwc not identity matrix
	// arbitraty matrix maybe identity matrix or color transform matrix;
	// only when set identity matrix and not gpu overlay, open display color
	DDPINFO("hint: %d, identity: %d, fte_flag: %d, %s bypass color:%d\n",
		hint, identity_matrix, fte_flag, mtk_dump_comp_str(comp), ccorr_data->bypass_color);
	if (((hint == 0) || ((hint == 1) && identity_matrix)) && (!fte_flag)) {
		if (ccorr_data->bypass_color == true) {
			mtk_color_bypass(ccorr_data->color_comp, false, handle);
			ccorr_data->bypass_color = false;
		}
	} else {
		if ((ccorr_data->bypass_color == false) &&
				(primary_data->disp_ccorr_number == 1) &&
				(!(primary_data->disp_ccorr_linear & 0x01))) {
			mtk_color_bypass(ccorr_data->color_comp, true, handle);
			ccorr_data->bypass_color = true;
		}
	}

	// offset part
/*	if ((matrix[12] != 0) || (matrix[13] != 0) || (matrix[14] != 0))
		need_offset = true;
	else
		need_offset = false;
*/

	primary_data->disp_ccorr_coef->offset[0] =
		(matrix[12] << 1) << primary_data->ccorr_offset_mask;
	primary_data->disp_ccorr_coef->offset[1] =
		(matrix[13] << 1) << primary_data->ccorr_offset_mask;
	primary_data->disp_ccorr_coef->offset[2] =
		(matrix[14] << 1) << primary_data->ccorr_offset_mask;

	//if only ccorr0 hw exist and aosp forece linear or
	//pq force nonlinear,id should be 0, primary_data->disp_ccorr_coef
	//should be PQ ioctl data, so no need to set value here

	if (!(((primary_data->prim_ccorr_force_linear && (primary_data->disp_ccorr_linear&0x01)) ||
		(primary_data->prim_ccorr_pq_nonlinear && primary_data->disp_ccorr_linear == 0)) &&
		(primary_data->disp_ccorr_number == 1))) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++) {
				primary_data->disp_ccorr_coef->coef[i][j] = 0;
				if (i == j)
					primary_data->disp_ccorr_coef->coef[i][j] =
						primary_data->ccorr_offset_base;
		}
	}

	for (i = 0; i < 3; i += 1) {
		DDPDBG("ccorr_color_matrix[%d][0-2] = {%d, %d, %d}\n",
				i,
				primary_data->ccorr_color_matrix[i][0],
				primary_data->ccorr_color_matrix[i][1],
				primary_data->ccorr_color_matrix[i][2]);
	}

	DDPDBG("ccorr_color_matrix offset {%d, %d, %d}, hint: %d\n",
		primary_data->disp_ccorr_coef->offset[0],
		primary_data->disp_ccorr_coef->offset[1],
		primary_data->disp_ccorr_coef->offset[2], hint);

	primary_data->disp_ccorr_without_gamma = ccorr_without_gamma;
	primary_data->disp_ccorr_temp_linear = primary_data->disp_ccorr_without_gamma;

	primary_data->disp_aosp_ccorr = true;
	disp_ccorr_write_coef_reg(comp, handle, 0);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			if (primary_data->ccorr_prev_matrix[i][j]
				!= primary_data->ccorr_color_matrix[i][j]) {
				/* refresh when matrix changed */
				need_refresh = true;
			}
			/* Copy Color Matrix */
			primary_data->ccorr_prev_matrix[i][j] =
				primary_data->ccorr_color_matrix[i][j];
		}
	}

	for (i = 0; i < 3; i += 1) {
		DDPINFO("primary_data->ccorr_color_matrix[%d][0-2] = {%d, %d, %d}\n",
				i,
				primary_data->ccorr_color_matrix[i][0],
				primary_data->ccorr_color_matrix[i][1],
				primary_data->ccorr_color_matrix[i][2]);
	}

	DDPINFO("primary_data->disp_ccorr_without_gamma: [%d], need_refresh: [%d]\n",
		primary_data->disp_ccorr_without_gamma, need_refresh);

	mutex_unlock(&primary_data->ccorr_global_lock);

	if (need_refresh == true && comp->mtk_crtc != NULL)
		mtk_crtc_check_trigger(comp->mtk_crtc, true, false);

	return 0;
}

int disp_ccorr_set_RGB_Gain(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle,
	int r, int g, int b)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int ret;

	mutex_lock(&primary_data->ccorr_global_lock);
	primary_data->rgb_matrix[0][0] = r;
	primary_data->rgb_matrix[1][1] = g;
	primary_data->rgb_matrix[2][2] = b;

	DDPINFO("%s: r[%d], g[%d], b[%d]", __func__, r, g, b);
	ret = disp_ccorr_write_coef_reg(comp, NULL, 0);
	mutex_unlock(&primary_data->ccorr_global_lock);

	return ret;
}

int mtk_ccorr_cfg_set_ccorr(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct mtk_disp_ccorr *ccorr = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr->primary_data;
	struct DRM_DISP_CCORR_COEF_T *ccorr_config;
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int connector_id = 0;
	int ret = 0;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp == NULL) {
		DDPPR_ERR("%s: failed to get output_comp!\n", __func__);
		return -1;
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &connector_id);

	ccorr_config = data;
	DDPINFO("%s pqhal_linear = %d, comp_linear =%d, comp:%s\n", __func__,
			ccorr_config->linear, ccorr->is_linear, mtk_dump_comp_str(comp));

	if (ccorr_config->linear != ccorr->is_linear)
		return 0;

	if (ccorr_config->linear == true)
		primary_data->disp_ccorr_without_gamma = CCORR_INVERSE_GAMMA;
	else
		primary_data->disp_ccorr_without_gamma = CCORR_BYASS_GAMMA;

	if (disp_ccorr_set_coef(ccorr_config,
		comp, handle) < 0) {
		DDPPR_ERR("DISP_IOCTL_SET_CCORR: failed\n");
		return -EFAULT;
	}

	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *comp_ccorr1 = ccorr->companion;

		if (disp_ccorr_set_coef(ccorr_config, comp_ccorr1, handle) < 0) {
			DDPPR_ERR("DISP_IOCTL_SET_CCORR: failed\n");
			return -EFAULT;
		}
	}

	if (pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {

		if ((ccorr_config->silky_bright_flag) == 1 &&
			ccorr_config->FinalBacklight != 0) {
			DDPINFO("connector_id:%d, brightness:%d, silky_bright_flag:%d",
				connector_id, ccorr_config->FinalBacklight,
				ccorr_config->silky_bright_flag);
			mtk_leds_brightness_set(connector_id,
				ccorr_config->FinalBacklight, 0, (0X01<<SET_BACKLIGHT_LEVEL));
		}
	}

	return ret;
}

int mtk_drm_ioctl_set_ccorr_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct pq_common_data *pq_data = mtk_crtc->pq_data;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	struct DRM_DISP_CCORR_COEF_T *ccorr_config = data;
	struct mtk_ddp_comp *output_comp = NULL;
	unsigned int connector_id = 0;
	int ret;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp == NULL) {
		DDPPR_ERR("%s: failed to get output_comp!\n", __func__);
		return -1;
	}
	mtk_ddp_comp_io_cmd(output_comp, NULL, GET_CONNECTOR_ID, &connector_id);

	DDPINFO("%s pqhal_linear = %d, comp_linear =%d, comp:%s\n", __func__,
			ccorr_config->linear, ccorr_data->is_linear, mtk_dump_comp_str(comp));

	if (ccorr_config->linear != ccorr_data->is_linear)
		return 0;

	if (ccorr_config->linear == true)
		primary_data->disp_ccorr_without_gamma = CCORR_INVERSE_GAMMA;
	else
		primary_data->disp_ccorr_without_gamma = CCORR_BYASS_GAMMA;

	if (pq_data->new_persist_property[DISP_PQ_CCORR_SILKY_BRIGHTNESS]) {

		ret = mtk_crtc_user_cmd(crtc, comp, SET_CCORR, data);

		if ((ccorr_config->silky_bright_flag) == 1 &&
			ccorr_config->FinalBacklight != 0) {
			DDPINFO("connector_id:%d, brightness:%d, silky_bright_flag:%d\n",
				connector_id, ccorr_config->FinalBacklight,
				ccorr_config->silky_bright_flag);
			mtk_leds_brightness_set(connector_id,
				ccorr_config->FinalBacklight, 0, (0X01<<SET_BACKLIGHT_LEVEL));
		}

		mtk_crtc_check_trigger(comp->mtk_crtc, false, true);

		return ret;
	} else {
		ret = mtk_crtc_user_cmd(crtc, comp, SET_CCORR, data);

		mtk_crtc_check_trigger(comp->mtk_crtc, false, true);

		return ret;
	}
}

int mtk_drm_ioctl_set_ccorr(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp;
	int i, j, ret = -1;

	for_each_comp_in_cur_crtc_path(comp, to_mtk_crtc(crtc), i, j) {
		if (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_CCORR)
			ret = mtk_drm_ioctl_set_ccorr_impl(comp, data);
		if (ret < 0) {
			DDPPR_ERR("%s:%d, failed, comp:%d\n", __func__, __LINE__, comp->id);
			break;
		}
	}

	return ret;
}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
int led_brightness_changed_event_to_pq(struct notifier_block *nb, unsigned long event,
	void *v)
{
	int trans_level;
	struct led_conf_info *led_conf;
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_ddp_comp *comp = NULL;

	led_conf = (struct led_conf_info *)v;
	if (!led_conf) {
		DDPPR_ERR("%s: led_conf is NULL!\n", __func__);
		return -1;
	}
	crtc = get_crtc_from_connector(led_conf->connector_id, g_drm_dev);
	if (crtc == NULL) {
		led_conf->aal_enable = 0;
		DDPPR_ERR("%s: failed to get crtc!\n", __func__);
		return -1;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (!(mtk_crtc->crtc_caps.crtc_ability & ABILITY_PQ) ||
			atomic_read(&mtk_crtc->pq_data->pipe_info_filled) != 1) {
		DDPINFO("%s, bl %d no need pq, connector_id:%d, crtc_id:%d\n", __func__,
				led_conf->cdev.brightness, led_conf->connector_id, drm_crtc_index(crtc));
		led_conf->aal_enable = 0;
		return 0;
	}
	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_CCORR, 0);
	if (!comp) {
		led_conf->aal_enable = 0;
		DDPPR_ERR("%s, comp is null!\n", __func__);
		return -1;
	}

	switch (event) {
	case LED_BRIGHTNESS_CHANGED:
		trans_level = led_conf->cdev.brightness;

		if (led_conf->led_type == LED_TYPE_ATOMIC)
			break;

		disp_pq_notify_backlight_changed(comp, trans_level);
		DDPINFO("%s: brightness changed: %d(%d)\n",
			__func__, trans_level, led_conf->cdev.brightness);
		break;
	case LED_STATUS_SHUTDOWN:
		if (led_conf->led_type == LED_TYPE_ATOMIC)
			break;

		disp_pq_notify_backlight_changed(comp, 0);
		break;
	case LED_TYPE_CHANGED:
		pr_info("[leds -> ccorr] led type changed: %d", led_conf->led_type);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block leds_init_notifier = {
	.notifier_call = led_brightness_changed_event_to_pq,
};
#endif

int mtk_ccorr_cfg_eventctl(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, void *data, unsigned int data_size)
{
	int ret = 0;
	/* TODO: dual pipe */
	int *enabled = data;

	if (*enabled)
		mtk_crtc_check_trigger(comp->mtk_crtc, true, true);

	DDPINFO("ccorr_eventctl, enabled = %d\n", *enabled);

	mtk_disp_ccorr_set_interrupt(comp, data);

	return ret;
}

int mtk_drm_ioctl_ccorr_eventctl_impl(struct mtk_ddp_comp *comp, void *data)
{
	int ret = 0;
	/* TODO: dual pipe */
	int *enabled = data;

	if (enabled)
		mtk_crtc_check_trigger(comp->mtk_crtc, true, true);

	if (enabled == NULL) {
		DDPPR_ERR("%s, null pointer!\n", __func__);
		return -1;
	}
	//mtk_crtc_user_cmd(crtc, comp, EVENTCTL, data);
	DDPINFO("ccorr_eventctl, enabled = %d\n", *enabled);
	mtk_disp_ccorr_set_interrupt(comp, enabled);

	return ret;
}

int mtk_drm_ioctl_ccorr_eventctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_CCORR, 0);

	return mtk_drm_ioctl_ccorr_eventctl_impl(comp, data);
}

int mtk_drm_ioctl_ccorr_get_irq_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int ret = 0;

	atomic_set(&primary_data->ccorr_is_init_valid, 1);

	disp_ccorr_wait_irq(comp);

	if (disp_pq_copy_backlight_to_user(comp, (int *) data) < 0) {
		DDPPR_ERR("%s: failed", __func__);
		ret = -EFAULT;
	}

	return ret;
}

int mtk_drm_ioctl_ccorr_get_irq(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_CCORR, 0);

	return mtk_drm_ioctl_ccorr_get_irq_impl(comp, data);
}

int mtk_drm_ioctl_aibld_cv_mode_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;

	primary_data->sbd_on = *(bool *)data;
	return 0;
}

int mtk_drm_ioctl_aibld_cv_mode(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_CCORR, 0);

	return mtk_drm_ioctl_aibld_cv_mode_impl(comp, data);
}

int mtk_drm_ioctl_support_color_matrix_impl(struct mtk_ddp_comp *comp, void *data)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int ret = 0;
	struct DISP_COLOR_TRANSFORM *color_transform;
	bool support_matrix = true;
	bool identity_matrix = true;
	int i, j;

	if (data == NULL) {
		support_matrix = false;
		ret = -EFAULT;
		DDPINFO("unsupported matrix");
		return ret;
	}

	color_transform = data;

	// Support matrix:
	// AOSP is 4x3 matrix. Offset is located at 4th row (not zero)

	for (i = 0 ; i < 3; i++) {
		if (color_transform->matrix[i][3] != 0) {
			for (i = 0 ; i < 4; i++) {
				DDPINFO("unsupported:[%d][0-3]:[%d %d %d %d]",
					i,
					color_transform->matrix[i][0],
					color_transform->matrix[i][1],
					color_transform->matrix[i][2],
					color_transform->matrix[i][3]);
			}
			support_matrix = false;
			ret = -EFAULT;
			return ret;
		}
	}
	if (support_matrix) {
		ret = 0; //Zero: support color matrix.
		for (i = 0 ; i < 3; i++)
			for (j = 0 ; j < 3; j++)
				if ((i == j) &&
					(color_transform->matrix[i][j] !=
					 primary_data->ccorr_offset_base))
					identity_matrix = false;
	}

	//if only one ccorr and ccorr0 is linear, AOSP matrix unsupport
	if ((primary_data->disp_ccorr_number == 1) && (primary_data->disp_ccorr_linear&0x01)
		&& (!identity_matrix) && (!primary_data->prim_ccorr_force_linear))
		ret = -EFAULT;
	else
		ret = 0;

	return ret;
}

int mtk_drm_ioctl_support_color_matrix(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_CCORR, 0);

	if (IS_ERR_OR_NULL(comp))
		return -EFAULT;

	return mtk_drm_ioctl_support_color_matrix_impl(comp, data);
}

int mtk_get_ccorr_caps(struct mtk_ddp_comp *comp, struct drm_mtk_ccorr_caps *ccorr_caps)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;

	memcpy(ccorr_caps, &primary_data->disp_ccorr_caps, sizeof(primary_data->disp_ccorr_caps));
	return 0;
}

static int mtk_set_ccorr_caps(struct mtk_ddp_comp *comp, struct drm_mtk_ccorr_caps *ccorr_caps)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;

	if (ccorr_caps->ccorr_linear != primary_data->disp_ccorr_caps.ccorr_linear) {
		primary_data->disp_ccorr_caps.ccorr_linear = ccorr_caps->ccorr_linear;
		primary_data->disp_ccorr_linear = primary_data->disp_ccorr_caps.ccorr_linear;
		DDPINFO("%s:update ccorr 0 linear by DORA API\n", __func__);
	}
	return 0;
}

int mtk_drm_ioctl_get_pq_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			to_mtk_crtc(crtc), MTK_DISP_CCORR, 0);
	struct mtk_drm_pq_caps_info *pq_info = data;

	if (IS_ERR_OR_NULL(comp))
		return -EFAULT;

	return mtk_get_ccorr_caps(comp, &pq_info->ccorr_caps);
}

int mtk_drm_ioctl_set_pq_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_crtc *crtc = private->crtc[0];
	struct mtk_ddp_comp *comp;
	struct mtk_drm_pq_caps_info *pq_info = data;
	int i, j;

	for_each_comp_in_cur_crtc_path(comp, to_mtk_crtc(crtc), i, j) {
		if (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_CCORR)
			mtk_set_ccorr_caps(comp, &pq_info->ccorr_caps);
	}
	return 0;
}

static void mtk_disp_ccorr_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);

	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (!ccorr_data->is_right_pipe) {
			ccorr_data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				ccorr_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.left_in_width +=
				ccorr_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			ccorr_data->tile_overhead.in_width =
				cfg->tile_overhead.left_in_width;
			ccorr_data->tile_overhead.overhead =
				cfg->tile_overhead.left_overhead;
		} else {
			ccorr_data->tile_overhead.comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				ccorr_data->tile_overhead.comp_overhead;
			cfg->tile_overhead.right_in_width +=
				ccorr_data->tile_overhead.comp_overhead;
			/*copy from total overhead info*/
			ccorr_data->tile_overhead.in_width =
				cfg->tile_overhead.right_in_width;
			ccorr_data->tile_overhead.overhead =
				cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_disp_ccorr_config_overhead_v(struct mtk_ddp_comp *comp,
	struct total_tile_overhead_v  *tile_overhead_v)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);

	DDPDBG("line: %d\n", __LINE__);

	/*set component overhead*/
	ccorr_data->tile_overhead_v.comp_overhead_v = 0;
	/*add component overhead on total overhead*/
	tile_overhead_v->overhead_v +=
		ccorr_data->tile_overhead_v.comp_overhead_v;
	/*copy from total overhead info*/
	ccorr_data->tile_overhead_v.overhead_v = tile_overhead_v->overhead_v;
}

static void mtk_ccorr_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	unsigned int width;
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	unsigned int overhead_v;

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		width = ccorr_data->tile_overhead.in_width;
	} else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;
	}

	DDPINFO("%s\n", __func__);

	if (cfg->source_bpc == 8)
		primary_data->ccorr_8bit_switch = 1;
	else if (cfg->source_bpc == 10)
		primary_data->ccorr_8bit_switch = 0;
	else
		DDPINFO("Disp CCORR's bit is : %u\n", cfg->bpc);

	if (!ccorr_data->set_partial_update)
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_REG_CCORR_SIZE,
				   (width << 16) | cfg->h, ~0);
	else {
		overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
					? 0 : ccorr_data->tile_overhead_v.overhead_v;
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_CCORR_SIZE,
			       (width << 16) | (ccorr_data->roi_height + overhead_v * 2), ~0);
	}
}

static void mtk_ccorr_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;

	primary_data->disp_aosp_ccorr = false;
	primary_data->disp_ccorr_without_gamma = CCORR_INVERSE_GAMMA;

	if (primary_data->disp_ccorr_number == 2) {
		if (!ccorr_data->is_linear) {
			primary_data->disp_aosp_ccorr = true;
			primary_data->disp_ccorr_without_gamma =
				primary_data->disp_ccorr_temp_linear;
		}
	} else if (!(primary_data->disp_ccorr_linear & 0x01)) {
		primary_data->disp_aosp_ccorr = true;
		primary_data->disp_ccorr_without_gamma =
			primary_data->disp_ccorr_temp_linear;
	}

	disp_ccorr_write_coef_reg(comp, handle, 1);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_CCORR_EN, 0x1, 0x1);
}

static void mtk_ccorr_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;

	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_CCORR_CFG, bypass, 0x1);
	primary_data->ccorr_relay_value = bypass;
}

static int mtk_ccorr_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);

	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case SET_CCORR:
	{
		struct DRM_DISP_CCORR_COEF_T *config = data;

		if (disp_ccorr_set_coef(config,
			comp, handle) < 0) {
			DDPPR_ERR("DISP_IOCTL_SET_CCORR: failed\n");
			return -EFAULT;
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_ddp_comp *comp_ccorr1 = ccorr_data->companion;

			if (disp_ccorr_set_coef(config, comp_ccorr1, handle) < 0) {
				DDPPR_ERR("DISP_IOCTL_SET_CCORR: failed\n");
				return -EFAULT;
			}
		}

	}
	break;

	case SET_INTERRUPT:
	{
		mtk_disp_ccorr_set_interrupt(comp, data);
	}
	break;

	case BYPASS_CCORR:
	{
		struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
		struct mtk_ddp_comp *cur_comp;
		int *value = data;
		int i, j;

		for_each_comp_in_cur_crtc_path(cur_comp, mtk_crtc, i, j) {
			if (mtk_ddp_comp_get_type(cur_comp->id) == MTK_DISP_CCORR)
				mtk_ccorr_bypass(cur_comp, *value, handle);
		}
		if (mtk_crtc->is_dual_pipe) {
			for_each_comp_in_dual_pipe(cur_comp, mtk_crtc, i, j) {
				if (mtk_ddp_comp_get_type(cur_comp->id) == MTK_DISP_CCORR)
					mtk_ccorr_bypass(cur_comp, *value, handle);
			}
		}
	}
	break;

	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static void ddp_ccorr_backup(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;

	primary_data->backup.REG_CCORR_CFG =
			readl(comp->regs + DISP_REG_CCORR_CFG);
}

static void ddp_ccorr_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;

	writel(primary_data->backup.REG_CCORR_CFG,
			comp->regs + DISP_REG_CCORR_CFG);
}

static void mtk_ccorr_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr = comp_to_ccorr(comp);
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);

	DDPINFO("%s\n", __func__);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&ccorr_data->is_clock_on, 1);

	/* Bypass shadow register and read shadow register */
	if (ccorr->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, CCORR_BYPASS_SHADOW,
			DISP_REG_CCORR_SHADOW, CCORR_BYPASS_SHADOW);
	else
		mtk_ddp_write_mask_cpu(comp, 0,
			DISP_REG_CCORR_SHADOW, CCORR_BYPASS_SHADOW);
	ddp_ccorr_restore(comp);
}

static void mtk_ccorr_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;

	ddp_ccorr_backup(comp);

	atomic_set(&ccorr_data->is_clock_on, 0);
	wake_up_interruptible(&primary_data->ccorr_get_irq_wq); // wake up who's waiting isr
	mtk_ddp_comp_clk_unprepare(comp);

	DDPINFO("%s\n", __func__);

}

static void mtk_ccorr_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_ddp_comp *color_comp;

	if (!ccorr_data->is_right_pipe)
		color_comp = mtk_ddp_comp_sel_in_cur_crtc_path(
				comp->mtk_crtc, MTK_DISP_COLOR, 0);
	else
		color_comp = mtk_ddp_comp_sel_in_dual_pipe(
				comp->mtk_crtc, MTK_DISP_COLOR, 0);
	ccorr_data->color_comp = color_comp;
}

static void mtk_ccorr_primary_data_init(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	struct mtk_disp_ccorr *companion_data = comp_to_ccorr(ccorr_data->companion);

	if (ccorr_data->is_right_pipe) {
		kfree(ccorr_data->primary_data);
		ccorr_data->primary_data = companion_data->primary_data;
		return;
	}

	init_waitqueue_head(&primary_data->ccorr_get_irq_wq);
	spin_lock_init(&primary_data->ccorr_clock_lock);
	spin_lock_init(&primary_data->pq_bl_change_lock);
	mutex_init(&primary_data->ccorr_global_lock);
}

static int mtk_ccorr_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
							enum mtk_ddp_io_cmd cmd, void *params)
{
	switch (cmd) {
	case PQ_FILL_COMP_PIPE_INFO:
	{
		struct mtk_disp_ccorr *data = comp_to_ccorr(comp);
		bool *is_right_pipe = &data->is_right_pipe;
		int ret, *path_order = &data->path_order;
		struct mtk_ddp_comp **companion = &data->companion;
		struct mtk_disp_ccorr *companion_data;

		if (atomic_read(&comp->mtk_crtc->pq_data->pipe_info_filled) == 1)
			break;
		ret = mtk_pq_helper_fill_comp_pipe_info(comp, path_order, is_right_pipe, companion);
		if (!ret && comp->mtk_crtc->is_dual_pipe && data->companion) {
			companion_data = comp_to_ccorr(data->companion);
			companion_data->path_order = data->path_order;
			companion_data->is_right_pipe = !data->is_right_pipe;
			companion_data->companion = comp;
		}
		mtk_ccorr_data_init(comp);
		mtk_ccorr_primary_data_init(comp);
		if (comp->mtk_crtc->is_dual_pipe && data->companion) {
			mtk_ccorr_data_init(data->companion);
			mtk_ccorr_primary_data_init(data->companion);
		}
	}
		break;
	default:
		break;
	}
	return 0;
}

void mtk_ccorr_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	mtk_ccorr_config(comp, cfg, handle);
}

static int mtk_ccorr_pq_frame_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data, unsigned int data_size)
{
	int ret = -1;

	DDPINFO("%s CMD = %d\n", __func__, cmd);
	switch (cmd) {

	case PQ_CCORR_EVENTCTL:
		ret = mtk_ccorr_cfg_eventctl(comp, handle, data, data_size);
		break;
	case PQ_CCORR_SET_CCORR:
		ret = mtk_ccorr_cfg_set_ccorr(comp, handle, data, data_size);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_ccorr_ioctl_transact(struct mtk_ddp_comp *comp,
		unsigned int cmd, void *data, unsigned int data_size)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_drm_pq_caps_info *pq_info = NULL;
	int ret = -1;

	if (ccorr_data->path_order != 0 &&
		cmd != PQ_CCORR_SET_PQ_CAPS &&
		cmd != PQ_CCORR_SET_CCORR)
		return 0;

	switch (cmd) {
	case PQ_CCORR_SET_CCORR:
		ret = mtk_drm_ioctl_set_ccorr_impl(comp, data);
		break;
	case PQ_CCORR_EVENTCTL:
		ret = mtk_drm_ioctl_ccorr_eventctl_impl(comp, data);
		break;
	case PQ_CCORR_GET_IRQ:
		ret = mtk_drm_ioctl_ccorr_get_irq_impl(comp, data);
		break;
	case PQ_CCORR_SUPPORT_COLOR_MATRIX:
		ret = mtk_drm_ioctl_support_color_matrix_impl(comp, data);
		break;
	case PQ_CCORR_AIBLD_CV_MODE:
		ret = mtk_drm_ioctl_aibld_cv_mode_impl(comp, data);
		break;
	case PQ_CCORR_GET_PQ_CAPS:
		pq_info = (struct mtk_drm_pq_caps_info *)data;
		ret = mtk_get_ccorr_caps(comp, &pq_info->ccorr_caps);
		break;
	case PQ_CCORR_SET_PQ_CAPS:
		pq_info = (struct mtk_drm_pq_caps_info *)data;
		ret = mtk_set_ccorr_caps(comp, &pq_info->ccorr_caps);
		break;
	default:
		break;
	}
	return ret;
}

static int mtk_ccorr_set_partial_update(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle, struct mtk_rect partial_roi, bool enable)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	unsigned int full_height = mtk_crtc_get_height_by_comp(__func__,
						&comp->mtk_crtc->base, comp, true);
	unsigned int overhead_v;

	DDPDBG("%s, %s set partial update, height:%d, enable:%d\n",
			__func__, mtk_dump_comp_str(comp), partial_roi.height, enable);

	ccorr_data->set_partial_update = enable;
	ccorr_data->roi_height = partial_roi.height;
	overhead_v = (!comp->mtk_crtc->tile_overhead_v.overhead_v)
				? 0 : ccorr_data->tile_overhead_v.overhead_v;

	DDPDBG("%s, %s overhead_v:%d\n",
			__func__, mtk_dump_comp_str(comp), overhead_v);

	if (ccorr_data->set_partial_update) {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_REG_CCORR_SIZE,
				   ccorr_data->roi_height + overhead_v * 2, 0x1fff);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_REG_CCORR_SIZE,
				   full_height, 0x1fff);
	}

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_ccorr_funcs = {
	.config = mtk_ccorr_config,
	.first_cfg = mtk_ccorr_first_cfg,
	.start = mtk_ccorr_start,
	.bypass = mtk_ccorr_bypass,
	.user_cmd = mtk_ccorr_user_cmd,
	.io_cmd = mtk_ccorr_io_cmd,
	.prepare = mtk_ccorr_prepare,
	.unprepare = mtk_ccorr_unprepare,
	.config_overhead = mtk_disp_ccorr_config_overhead,
	.config_overhead_v = mtk_disp_ccorr_config_overhead_v,
	.pq_frame_config = mtk_ccorr_pq_frame_config,
	.mutex_sof_irq = disp_ccorr_on_start_of_frame,
	.pq_ioctl_transact = mtk_ccorr_ioctl_transact,
	.partial_update = mtk_ccorr_set_partial_update,
};

static int mtk_disp_ccorr_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_ccorr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);
	if (!g_drm_dev)
		g_drm_dev = drm_dev;

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	disp_ccorr_color_matrix_to_dispsys(&priv->ddp_comp, drm_dev);
	return 0;
}

static void mtk_disp_ccorr_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_ccorr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_ccorr_component_ops = {
	.bind	= mtk_disp_ccorr_bind,
	.unbind = mtk_disp_ccorr_unbind,
};

void mtk_ccorr_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, -1);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, -1, -1);
}

static int mtk_update_ccorr_base(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int i, j;

	if (primary_data->disp_ccorr_caps.ccorr_bit == 12)
		return 0;

	primary_data->ccorr_offset_base = 2048;
	primary_data->ccorr_max_negative = primary_data->ccorr_offset_base*(-2);
	primary_data->ccorr_max_positive = (primary_data->ccorr_offset_base*2)-1;
	primary_data->ccorr_fullbit_mask = 0x1fff;
	primary_data->ccorr_offset_mask = 13;

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++) {
			if (i == j) {
				primary_data->ccorr_color_matrix[i][j] =
					primary_data->ccorr_offset_base;
				primary_data->ccorr_prev_matrix[i][j] =
					primary_data->ccorr_offset_base;
				primary_data->rgb_matrix[i][j] =
					primary_data->ccorr_offset_base;
			}
		}
	return 0;
}

static void mtk_get_ccorr_property(struct mtk_ddp_comp *comp, struct device_node *node)
{
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int ret;
	int ccorr0_force_linear = 0;

	ret = of_property_read_u32(node, "ccorr-bit", &primary_data->disp_ccorr_caps.ccorr_bit);
	if (ret)
		DDPPR_ERR("read ccorr_bit failed\n");

	ret = of_property_read_u32(node, "ccorr-num-per-pipe",
			&primary_data->disp_ccorr_caps.ccorr_number);
	if (ret)
		DDPPR_ERR("read ccorr_number failed\n");

	ret = of_property_read_u32(node, "ccorr-linear-per-pipe",
			&primary_data->disp_ccorr_caps.ccorr_linear);
	if (ret)
		DDPPR_ERR("read ccorr_linear failed\n");

	ret = of_property_read_u32(node, "ccorr-prim-force-linear", &ccorr0_force_linear);
	if (ret)
		DDPPR_ERR("read ccorr_prim_force_linear failed\n");

	ret = of_property_read_u32(node, "ccorr-linear", &(ccorr_data->is_linear));
	if (ret)
		DDPPR_ERR("read ccorr-linear failed\n");
	DDPINFO("%s is_linear %d\n", mtk_dump_comp_str(comp), ccorr_data->is_linear);
	DDPINFO("%s:ccorr_bit:%d,ccorr_number:%d,ccorr_linear:%d,ccorr0 force linear:%d\n",
		__func__, primary_data->disp_ccorr_caps.ccorr_bit,
		primary_data->disp_ccorr_caps.ccorr_number,
		primary_data->disp_ccorr_caps.ccorr_linear, ccorr0_force_linear);

	primary_data->disp_ccorr_number = primary_data->disp_ccorr_caps.ccorr_number;
	primary_data->disp_ccorr_linear = primary_data->disp_ccorr_caps.ccorr_linear;

	if (ccorr0_force_linear == 0x1)
		primary_data->prim_ccorr_force_linear = true;
	else
		primary_data->prim_ccorr_force_linear = false;

	mtk_update_ccorr_base(comp);
}

static int mtk_disp_ccorr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_ccorr *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq;
	int ret = -1;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		goto error_dev_init;

	priv->primary_data = kzalloc(sizeof(*priv->primary_data), GFP_KERNEL);
	if (priv->primary_data == NULL) {
		ret = -ENOMEM;
		DDPPR_ERR("Failed to alloc primary_data %d\n", ret);
		goto error_dev_init;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		goto error_primary;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_CCORR);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		ret = (int)comp_id;
		goto error_primary;
	}

	priv->primary_data->disp_ccorr_caps.ccorr_bit = 12;
	priv->primary_data->disp_ccorr_caps.ccorr_number = 1;
	priv->primary_data->disp_ccorr_caps.ccorr_linear = 0x01;
	priv->primary_data->prim_ccorr_force_linear = false;
	priv->primary_data->prim_ccorr_pq_nonlinear = false;
	mtk_get_ccorr_property(&priv->ddp_comp, dev->of_node);

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_ccorr_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		goto error_primary;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	ret = devm_request_irq(dev, irq, mtk_disp_ccorr_irq_handler,
			       IRQF_TRIGGER_NONE | IRQF_SHARED,
			       dev_name(dev), priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_ccorr_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (comp_id == DDP_COMPONENT_CCORR0)
		mtk_leds_register_notifier(&leds_init_notifier);
#endif
	DDPINFO("%s-\n", __func__);

error_primary:
	if (ret < 0)
		kfree(priv->primary_data);
error_dev_init:
	if (ret < 0)
		devm_kfree(dev, priv);

	return ret;
}

static int mtk_disp_ccorr_remove(struct platform_device *pdev)
{
	struct mtk_disp_ccorr *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_ccorr_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

#ifdef CONFIG_LEDS_BRIGHTNESS_CHANGED
	if (priv->ddp_comp.id == DDP_COMPONENT_CCORR0)
		mtk_leds_unregister_notifier(&leds_init_notifier);
#endif
	return 0;
}

static const struct mtk_disp_ccorr_data mt6779_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_ccorr_data mt6885_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_ccorr_data mt6873_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6853_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6833_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6983_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6895_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6879_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6855_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6985_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6897_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6886_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6989_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_ccorr_data mt6878_ccorr_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};


static const struct of_device_id mtk_disp_ccorr_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6779-disp-ccorr",
	  .data = &mt6779_ccorr_driver_data},
	{ .compatible = "mediatek,mt6885-disp-ccorr",
	  .data = &mt6885_ccorr_driver_data},
	{ .compatible = "mediatek,mt6873-disp-ccorr",
	  .data = &mt6873_ccorr_driver_data},
	{ .compatible = "mediatek,mt6853-disp-ccorr",
	  .data = &mt6853_ccorr_driver_data},
	{ .compatible = "mediatek,mt6833-disp-ccorr",
	  .data = &mt6833_ccorr_driver_data},
	{ .compatible = "mediatek,mt6983-disp-ccorr",
	  .data = &mt6983_ccorr_driver_data},
	{ .compatible = "mediatek,mt6895-disp-ccorr",
	  .data = &mt6895_ccorr_driver_data},
	{ .compatible = "mediatek,mt6879-disp-ccorr",
	  .data = &mt6879_ccorr_driver_data},
	{ .compatible = "mediatek,mt6855-disp-ccorr",
	  .data = &mt6855_ccorr_driver_data},
	{ .compatible = "mediatek,mt6985-disp-ccorr",
	  .data = &mt6985_ccorr_driver_data},
	{ .compatible = "mediatek,mt6886-disp-ccorr",
	  .data = &mt6886_ccorr_driver_data},
	{ .compatible = "mediatek,mt6835-disp-ccorr",
	  .data = &mt6835_ccorr_driver_data},
	{ .compatible = "mediatek,mt6897-disp-ccorr",
	  .data = &mt6897_ccorr_driver_data},
	{ .compatible = "mediatek,mt6989-disp-ccorr",
	  .data = &mt6989_ccorr_driver_data},
	{ .compatible = "mediatek,mt6878-disp-ccorr",
	  .data = &mt6878_ccorr_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_ccorr_driver_dt_match);

struct platform_driver mtk_disp_ccorr_driver = {
	.probe = mtk_disp_ccorr_probe,
	.remove = mtk_disp_ccorr_remove,
	.driver = {

			.name = "mediatek-disp-ccorr",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_ccorr_driver_dt_match,
		},
};

void disp_ccorr_set_bypass(struct drm_crtc *crtc, int bypass)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *comp = mtk_ddp_comp_sel_in_cur_crtc_path(
			mtk_crtc, MTK_DISP_CCORR, 0);
	struct mtk_disp_ccorr *ccorr_data = comp_to_ccorr(comp);
	struct mtk_disp_ccorr_primary *primary_data = ccorr_data->primary_data;
	int ret;

	if (primary_data->ccorr_relay_value == bypass)
		return;
	ret = mtk_crtc_user_cmd(crtc, comp, BYPASS_CCORR, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}

void mtk_ccorr_regdump(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_ccorr *ccorr = comp_to_ccorr(comp);
	void __iomem *baddr = comp->regs;
	bool isDualPQ = comp->mtk_crtc->is_dual_pipe;
	int k;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp),
			comp->regs_pa);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(comp));
	for (k = 0; k <= 0x110; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(comp));
	if (isDualPQ && ccorr->companion) {
		baddr = ccorr->companion->regs;
		DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(ccorr->companion),
				ccorr->companion->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(ccorr->companion));
		for (k = 0; k < 0x110; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(ccorr->companion));
	}
}

unsigned int disp_ccorr_bypass_info(struct mtk_drm_crtc *mtk_crtc)
{
	struct mtk_ddp_comp *comp;
	struct mtk_disp_ccorr *ccorr_data;

	comp = mtk_ddp_comp_sel_in_cur_crtc_path(mtk_crtc, MTK_DISP_CCORR, 0);
	ccorr_data = comp_to_ccorr(comp);

	return ccorr_data->primary_data->ccorr_relay_value;
}
