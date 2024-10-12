/*
 * linux/drivers/video/fbdev/exynos/dpu_3830/decon_reg.c
 *
 * Copyright 2013-2017 Samsung Electronics
 *	  SeungBeom Park <sb1.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../decon.h"
#include "../format.h"

#if defined(CONFIG_SAMSUNG_TUI)
#include <linux/smc.h>
#endif

/******************* DECON CAL functions *************************/
static int decon_reg_reset(u32 id)
{
	int tries;

	decon_write_mask(id, GLOBAL_CONTROL, ~0, GLOBAL_CONTROL_SRESET);
	for (tries = 2000; tries; --tries) {
		if (~decon_read(id, GLOBAL_CONTROL) & GLOBAL_CONTROL_SRESET)
			break;
		udelay(10);
	}

	if (!tries) {
		decon_err("failed to reset Decon\n");
		return -EBUSY;
	}

	return 0;
}

/* select op mode */
static void decon_reg_set_operation_mode(u32 id, enum decon_psr_mode mode)
{
	u32 val, mask;

	mask = GLOBAL_CONTROL_OPERATION_MODE_F;
	if (mode == DECON_MIPI_COMMAND_MODE)
		val = GLOBAL_CONTROL_OPERATION_MODE_CMD_F;
	else
		val = GLOBAL_CONTROL_OPERATION_MODE_VIDEO_F;
	decon_write_mask(id, GLOBAL_CONTROL, val, mask);
}

static void decon_reg_direct_on_off(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = (GLOBAL_CONTROL_DECON_EN | GLOBAL_CONTROL_DECON_EN_F);
	decon_write_mask(id, GLOBAL_CONTROL, val, mask);
}

static void decon_reg_per_frame_off(u32 id)
{
	decon_write_mask(id, GLOBAL_CONTROL, 0, GLOBAL_CONTROL_DECON_EN_F);
}

static u32 decon_reg_get_idle_status(u32 id)
{
	u32 val;

	val = decon_read(id, GLOBAL_CONTROL);
	if (val & GLOBAL_CONTROL_IDLE_STATUS)
		return 1;

	return 0;
}

u32 decon_reg_get_run_status(u32 id)
{
	u32 val;

	val = decon_read(id, GLOBAL_CONTROL);
	if (val & GLOBAL_CONTROL_RUN_STATUS)
		return 1;

	return 0;
}

static int decon_reg_wait_run_status_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 status;

	do {
		status = decon_reg_get_run_status(id);
		cnt--;
		udelay(delay_time);
	} while (!status && cnt);

	if (!cnt) {
		decon_err("decon%d wait timeout decon run status(%u)\n",
								id, status);
		return -EBUSY;
	}

	return 0;
}

/* Determine that DECON is perfectly shuttled off through
 * checking this function
 */
static int decon_reg_wait_run_is_off_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 status;

	do {
		status = decon_reg_get_run_status(id);
		cnt--;
		udelay(delay_time);
	} while (status && cnt);

	if (!cnt) {
		decon_err("decon%d wait timeout decon run is shut-off(%u)\n",
								id, status);
		return -EBUSY;
	}

	return 0;
}

/* In bring-up, all bits are disabled */
static void decon_reg_set_clkgate_mode(u32 id, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	/* all unmask */
	mask = CLOCK_CONTROL_0_CG_MASK | CLOCK_CONTROL_0_QACTIVE_MASK;
	decon_write_mask(id, CLOCK_CONTROL_0, val, mask);
}

/*
 * API is considering real possible Display Scenario
 */
static void decon_reg_set_sram_share(u32 id)
{
	u32 val = FF0_SRAM_SHARE_ENABLE_F;

	decon_write(id, SRAM_SHARE_ENABLE_MAIN, val);
}

static void decon_reg_set_frame_fifo_size(u32 id, u32 width, u32 height)
{
	u32 val, cnt;
	u32 th;

	val = FRAME_FIFO_HEIGHT_F(height) | FRAME_FIFO_WIDTH_F(width);
	cnt = height * width;

	decon_write(id, FRAME_FIFO_0_SIZE_CONTROL_0, val);
	decon_write(id, FRAME_FIFO_0_SIZE_CONTROL_1, cnt);

	th = FRAME_FIFO_0_TH_F(width);
	decon_write(id, FRAME_FIFO_TH_CONTROL_0, th);
}

static void decon_reg_set_rgb_order(u32 id, enum decon_rgb_order order)
{
	u32 val, mask;

	val = FORMATTER_OUT_RGB_ORDER_F(order);
	mask = FORMATTER_OUT_RGB_ORDER_MASK;
	decon_write_mask(id, FORMATTER_CONTROL, val, mask);
}

static void decon_reg_set_blender_bg_image_size(u32 id, struct exynos_panel_info *lcd_info)
{
	u32 val;

	val = BLENDER_BG_HEIGHT_F(lcd_info->yres) | BLENDER_BG_WIDTH_F(lcd_info->xres);
	decon_write(id, BLENDER_BG_IMAGE_SIZE_0, val);

    val = (lcd_info->yres) * (lcd_info->xres);
    decon_write(id, BLENDER_BG_IMAGE_SIZE_1, val);

}

static void decon_reg_set_data_path(u32 id, enum decon_data_path d_path,
		enum decon_enhance_path e_path)
{
	u32 val;

	val = ENHANCE_LOGIC_PATH_F(e_path) | COMP_LINKIF_WB_PATH_F(d_path);
	decon_write(id, DATA_PATH_CONTROL_2, val);
}

static void decon_reg_config_win_channel(u32 id, u32 win_idx, int ch)
{
	u32 val, mask;

	val = WIN_CHMAP_F(win_idx, ch);
	mask = WIN_CHMAP_MASK(win_idx);
	decon_write_mask(id, DATA_PATH_CONTROL_1, val, mask);
}

static void decon_reg_clear_int_all(u32 id)
{
	u32 mask;

	mask = (DPU_FRAME_DONE_INT_EN
			| DPU_FRAME_START_INT_EN);
	decon_write_mask(id, INTERRUPT_PENDING, ~0, mask);

	mask = (DPU_RESOURCE_CONFLICT_INT_EN
		| DPU_TIME_OUT_INT_EN);
	decon_write_mask(id, EXTRA_INTERRUPT_PENDING, ~0, mask);
}

static void decon_reg_configure_lcd(u32 id, struct decon_param *p)
{
	enum decon_data_path d_path = DPATH_NOCOMP_FF0_FORMATTER0_DSIMIF0;
	enum decon_enhance_path e_path = ENHANCEPATH_ENHANCE_ALL_OFF;

	struct exynos_panel_info *lcd_info = p->lcd_info;

	decon_reg_set_rgb_order(id, DECON_BGR);

	decon_reg_set_data_path(id, d_path, e_path);

	decon_reg_set_frame_fifo_size(id, lcd_info->xres, lcd_info->yres);

	decon_reg_per_frame_off(id);
}

static void decon_reg_init_probe(u32 id, u32 dsi_idx, struct decon_param *p)
{
	struct exynos_panel_info *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;
	enum decon_data_path d_path = DPATH_NOCOMP_FF0_FORMATTER0_DSIMIF0;
	enum decon_enhance_path e_path = ENHANCEPATH_ENHANCE_ALL_OFF;

	decon_reg_set_clkgate_mode(id, 0);

	decon_reg_set_sram_share(id);

	decon_reg_set_operation_mode(id, psr->psr_mode);

	decon_reg_set_blender_bg_image_size(id, lcd_info);

	/*
	 * same as decon_reg_configure_lcd(...) function
	 * except using decon_reg_update_req_global(id)
	 * instead of decon_reg_direct_on_off(id, 0)
	 */
	decon_reg_set_rgb_order(id, DECON_BGR);

	decon_reg_set_data_path(id, d_path, e_path);

	decon_reg_set_frame_fifo_size(id, lcd_info->xres, lcd_info->yres);
}


static void decon_reg_set_blender_bg_size(u32 id, u32 bg_w, u32 bg_h)
{
	u32 val;

	val = BLENDER_BG_HEIGHT_F(bg_h) | BLENDER_BG_WIDTH_F(bg_w);
	decon_write(id, BLENDER_BG_IMAGE_SIZE_0, val);

	val = bg_w * bg_h;
	decon_write(id, BLENDER_BG_IMAGE_SIZE_1, val);
}

static int decon_reg_stop_perframe(u32 id, struct decon_mode_info *psr, u32 fps)
{
	int ret = 0;
	int timeout_value = 0;

	decon_dbg("%s +\n", __func__);

	if ((psr->psr_mode == DECON_MIPI_COMMAND_MODE) &&
			(psr->trig_mode == DECON_HW_TRIG)) {
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
	}

	/* perframe stop */
	decon_reg_per_frame_off(id);

	decon_reg_update_req_global(id);

	/* timeout : 1 / fps + 20% margin */
	timeout_value = 1000 / fps * 12 / 10 + 5;
	ret = decon_reg_wait_run_is_off_timeout(id, timeout_value * MSEC);

	decon_dbg("%s -\n", __func__);
	return ret;
}

int decon_reg_stop_inst(u32 id, u32 dsi_idx, struct decon_mode_info *psr,
		u32 fps)
{
	int ret = 0;
	int timeout_value = 0;

	decon_dbg("%s +\n", __func__);

	if ((psr->psr_mode == DECON_MIPI_COMMAND_MODE) &&
			(psr->trig_mode == DECON_HW_TRIG)) {
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
	}

	/* instant stop */
	decon_reg_direct_on_off(id, 0);

	decon_reg_update_req_global(id);

	/* timeout : 1 / fps + 20% margin */
	timeout_value = 1000 / fps * 12 / 10 + 5;
	ret = decon_reg_wait_run_is_off_timeout(id, timeout_value * MSEC);

	decon_dbg("%s -\n", __func__);
	return ret;
}

void decon_reg_set_win_enable(u32 id, u32 win_idx, u32 en)
{
	u32 val, mask;

	val = en ? ~0 : 0;
	mask = WIN_EN_F(win_idx);
	decon_write_mask(id, DATA_PATH_CONTROL_0, val, mask);
	decon_dbg("%s: 0x%x\n", __func__, decon_read(id, DATA_PATH_CONTROL_0));
}

/*
 * argb_color : 32-bit
 * A[31:24] - R[23:16] - G[15:8] - B[7:0]
 */
static void decon_reg_set_win_mapcolor(u32 id, u32 win_idx, u32 argb_color)
{
	u32 val, mask;
	u32 mc_alpha = 0, mc_red = 0;
	u32 mc_green = 0, mc_blue = 0;

	mc_alpha = (argb_color >> 24) & 0xFF;
	mc_red = (argb_color >> 16) & 0xFF;
	mc_green = (argb_color >> 8) & 0xFF;
	mc_blue = (argb_color >> 0) & 0xFF;

	val = WIN_MAPCOLOR_A_F(mc_alpha) | WIN_MAPCOLOR_R_F(mc_red);
	mask = WIN_MAPCOLOR_A_MASK | WIN_MAPCOLOR_R_MASK;
	decon_write_mask(id, WIN_COLORMAP_0(win_idx), val, mask);

	val = WIN_MAPCOLOR_G_F(mc_green) | WIN_MAPCOLOR_B_F(mc_blue);
	mask = WIN_MAPCOLOR_G_MASK | WIN_MAPCOLOR_B_MASK;
	decon_write_mask(id, WIN_COLORMAP_1(win_idx), val, mask);
}

static void decon_reg_set_win_plane_alpha(u32 id, u32 win_idx, u32 a0, u32 a1)
{
	u32 val, mask;

	val = WIN_ALPHA1_F(a1) | WIN_ALPHA0_F(a0);
	mask = WIN_ALPHA1_MASK | WIN_ALPHA0_MASK;
	decon_write_mask(id, WIN_CONTROL_0(win_idx), val, mask);
}

static void decon_reg_set_winmap(u32 id, u32 win_idx, u32 color, u32 en)
{
	u32 val, mask;

	/* Enable */
	val = en ? ~0 : 0;
	mask = WIN_MAPCOLOR_EN_F(win_idx);
	decon_write_mask(id, DATA_PATH_CONTROL_0, val, mask);
	decon_dbg("%s: 0x%x\n", __func__, decon_read(id, DATA_PATH_CONTROL_0));

	/* Color Set */
	decon_reg_set_win_mapcolor(0, win_idx, color);
}

/* ALPHA_MULT selection used in (a',b',c',d') coefficient */
static void decon_reg_set_win_alpha_mult(u32 id, u32 win_idx, u32 a_sel)
{
	u32 val, mask;

	val = WIN_ALPHA_MULT_SRC_SEL_F(a_sel);
	mask = WIN_ALPHA_MULT_SRC_SEL_MASK;
	decon_write_mask(id, WIN_CONTROL_0(win_idx), val, mask);
}

static void decon_reg_set_win_sub_coeff(u32 id, u32 win_idx,
		u32 fgd, u32 bgd, u32 fga, u32 bga)
{
	u32 val, mask;

	/*
	 * [ Blending Equation ]
	 * Color : Cr = (a x Cf) + (b x Cb)  <Cf=FG pxl_C, Cb=BG pxl_C>
	 * Alpha : Ar = (c x Af) + (d x Ab)  <Af=FG pxl_A, Ab=BG pxl_A>
	 *
	 * [ User-defined ]
	 * a' = WINx_FG_ALPHA_D_SEL : Af' that is multiplied by FG Pixel Color
	 * b' = WINx_BG_ALPHA_D_SEL : Ab' that is multiplied by BG Pixel Color
	 * c' = WINx_FG_ALPHA_A_SEL : Af' that is multiplied by FG Pixel Alpha
	 * d' = WINx_BG_ALPHA_A_SEL : Ab' that is multiplied by BG Pixel Alpha
	 */

	val = (WIN_FG_ALPHA_D_SEL_F(fgd)
		| WIN_BG_ALPHA_D_SEL_F(bgd)
		| WIN_FG_ALPHA_A_SEL_F(fga)
		| WIN_BG_ALPHA_A_SEL_F(bga));
	mask = (WIN_FG_ALPHA_D_SEL_MASK
		| WIN_BG_ALPHA_D_SEL_MASK
		| WIN_FG_ALPHA_A_SEL_MASK
		| WIN_BG_ALPHA_A_SEL_MASK);
	decon_write_mask(id, WIN_CONTROL_1(win_idx), val, mask);
}

static void decon_reg_set_win_func(u32 id, u32 win_idx, enum decon_win_func pd_func)
{
	u32 val, mask;

	val = WIN_FUNC_F(pd_func);
	mask = WIN_FUNC_MASK;
	decon_write_mask(id, WIN_CONTROL_0(win_idx), val, mask);
}

static void decon_reg_set_win_bnd_function(u32 id, u32 win_idx,
		struct decon_window_regs *regs)
{
	int plane_a = regs->plane_alpha;
	enum decon_blending blend = regs->blend;
	enum decon_win_func pd_func = PD_FUNC_USER_DEFINED;
	u8 alpha0 = 0xff;
	u8 alpha1 = 0xff;
	bool is_plane_a = false;
	u32 af_d = BND_COEF_ONE, ab_d = BND_COEF_ZERO,
		af_a = BND_COEF_ONE, ab_a = BND_COEF_ZERO;

	if (blend == DECON_BLENDING_NONE)
		pd_func = PD_FUNC_COPY;

	if ((plane_a >= 0) && (plane_a <= 0xff)) {
		alpha0 = plane_a;
		alpha1 = 0;
		is_plane_a = true;
	}

	if ((blend == DECON_BLENDING_COVERAGE) && !is_plane_a) {
		af_d = BND_COEF_AF;
		ab_d = BND_COEF_1_M_AF;
		af_a = BND_COEF_AF;
		ab_a = BND_COEF_1_M_AF;
	} else if ((blend == DECON_BLENDING_COVERAGE) && is_plane_a) {
		af_d = BND_COEF_ALPHA_MULT;
		ab_d = BND_COEF_1_M_ALPHA_MULT;
		af_a = BND_COEF_ALPHA_MULT;
		ab_a = BND_COEF_1_M_ALPHA_MULT;
	} else if ((blend == DECON_BLENDING_PREMULT) && !is_plane_a) {
		af_d = BND_COEF_ONE;
		ab_d = BND_COEF_1_M_AF;
		af_a = BND_COEF_ONE;
		ab_a = BND_COEF_1_M_AF;
	} else if ((blend == DECON_BLENDING_PREMULT) && is_plane_a) {
		af_d = BND_COEF_PLNAE_ALPHA0;
		ab_d = BND_COEF_1_M_ALPHA_MULT;
		af_a = BND_COEF_PLNAE_ALPHA0;
		ab_a = BND_COEF_1_M_ALPHA_MULT;
	} else if (blend == DECON_BLENDING_NONE) {
		decon_dbg("%s:%d none blending mode\n", __func__, __LINE__);
	} else {
		decon_warn("%s:%d undefined blending mode\n",
				__func__, __LINE__);
	}

	decon_reg_set_win_plane_alpha(id, win_idx, alpha0, alpha1);
	decon_reg_set_win_alpha_mult(id, win_idx, ALPHA_MULT_SRC_SEL_AF);
	decon_reg_set_win_func(id, win_idx, pd_func);
	if (pd_func == PD_FUNC_USER_DEFINED)
		decon_reg_set_win_sub_coeff(id,
				win_idx, af_d, ab_d, af_a, ab_a);
}

/******************** EXPORTED DECON CAL APIs ********************/
void decon_reg_update_req_global(u32 id)
{
	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, ~0,
			SHADOW_REG_UPDATE_REQ_GLOBAL);
}

int decon_reg_init(u32 id, u32 dsi_idx, struct decon_param *p)
{
	struct exynos_panel_info *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;

	/*
	 * DECON does not need to start, if DECON is already
	 * running(enabled in LCD_ON_UBOOT)
	 */
	if (decon_reg_get_run_status(id)) {
		decon_info("decon_reg_init already called by BOOTLOADER\n");
		decon_reg_init_probe(id, dsi_idx, p);
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
			decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
		/* to prevent irq storm that may occur in the OFF STATE */
		decon_reg_clear_int_all(id);
		return -EBUSY;
	}

	decon_reg_set_clkgate_mode(id, 0);

	decon_reg_set_sram_share(id);

	decon_reg_set_operation_mode(id, psr->psr_mode);

	decon_reg_set_blender_bg_image_size(id, lcd_info);

	decon_reg_configure_lcd(id, p);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE) {
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
	}

	/* asserted interrupt should be cleared before initializing decon hw */
	decon_reg_clear_int_all(id);

	return 0;
}

int decon_reg_start(u32 id, struct decon_mode_info *psr)
{
	int ret = 0;

	decon_reg_direct_on_off(id, 1);
	decon_reg_update_req_global(id);

	/*
	 * DECON goes to run-status as soon as
	 * request shadow update without HW_TE
	 */
	ret = decon_reg_wait_run_status_timeout(id, 20 * 1000);

	/* wait until run-status, then trigger */
	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
	return ret;
}

/*
 * stop sequence should be carefully for stability
 * try sequecne
 *	1. perframe off
 *	2. instant off
 */
int decon_reg_stop(u32 id, u32 dsi_idx, struct decon_mode_info *psr, bool rst,
		u32 fps)
{
	int ret = 0;

	/* call perframe stop */
	ret = decon_reg_stop_perframe(id, psr, fps);
	if (ret < 0) {
		decon_err("%s, failed to perframe_stop\n", __func__);
		/* if fails, call decon instant off */
		ret = decon_reg_stop_inst(id, 0, psr, fps);
		if (ret < 0)
			decon_err("%s, failed to instant_stop\n", __func__);
	}

	/* assert reset when stopped normally or requested */
	if (!ret && rst)
		decon_reg_reset(id);

	decon_reg_clear_int_all(id);

	return ret;
}

void decon_reg_win_enable_and_update(u32 id, u32 win_idx, u32 en)
{
	decon_reg_set_win_enable(id, win_idx, en);
	decon_reg_update_req_window(id, win_idx);
}

#if defined(CONFIG_SAMSUNG_TUI)
/*
 * Only for Video mode TUI for EXYNOS3830
 *
 * This function is set SHADOW_UPDATE_REQ of secure window in SECURE SFR.
 */
#define SMC_DPU_SEC_SHADOW_UPDATE_REQ	((unsigned int)0x82002122)

void decon_reg_sec_win_shadow_update_req(void)
{
	int ret;
	struct decon_device *decon;

	decon = get_decon_drvdata(0);

	if (decon->dt.psr_mode == DECON_VIDEO_MODE && decon->tui_buf_protected) {
		ret = exynos_smc(SMC_DPU_SEC_SHADOW_UPDATE_REQ, 0, 0, 0);
		if (ret) {
			decon_err("%s, %d smc_call error\n", __func__, __LINE__);
		}
	}
}
#endif
void decon_reg_all_win_shadow_update_req(u32 id)
{
	u32 mask;

	mask = SHADOW_REG_UPDATE_REQ_FOR_DECON;
#if defined(CONFIG_SAMSUNG_TUI)
	decon_reg_sec_win_shadow_update_req();
#endif
	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, ~0, mask);
}

void decon_reg_set_window_control(u32 id, int win_idx,
		struct decon_window_regs *regs, u32 winmap_en)
{
	u32 win_en = regs->wincon & WIN_EN_F(win_idx) ? 1 : 0;

	if (win_en) {
		decon_dbg("%s: win id = %d\n", __func__, win_idx);
		decon_reg_set_win_bnd_function(0, win_idx, regs);
		decon_write(0, WIN_START_POSITION(win_idx), regs->start_pos);
		decon_write(0, WIN_END_POSITION(win_idx), regs->end_pos);
		decon_write(0, WIN_START_TIME_CONTROL(win_idx),
							regs->start_time);
		decon_reg_set_winmap(id, win_idx, regs->colormap, winmap_en);
	}

	decon_reg_config_win_channel(id, win_idx, regs->ch);
	decon_reg_set_win_enable(id, win_idx, win_en);

	decon_dbg("%s: regs->ch(%d)\n", __func__, regs->ch);
}

void decon_reg_update_req_window_mask(u32 id, u32 win_idx)
{
	u32 mask;

	mask = SHADOW_REG_UPDATE_REQ_FOR_DECON;
	mask &= ~(SHADOW_REG_UPDATE_REQ_WIN(win_idx));
	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, ~0, mask);
}

void decon_reg_update_req_window(u32 id, u32 win_idx)
{
	u32 mask;

	mask = SHADOW_REG_UPDATE_REQ_WIN(win_idx);
	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, ~0, mask);
}

void decon_reg_set_trigger(u32 id, struct decon_mode_info *psr,
		enum decon_set_trig en)
{
	u32 val, mask;

	if (psr->psr_mode == DECON_VIDEO_MODE)
		return;

	if (psr->trig_mode == DECON_SW_TRIG) {
		val = (en == DECON_TRIG_ENABLE) ? SW_TRIG_EN : 0;
		mask = HW_TRIG_EN | SW_TRIG_EN;
	} else { /* DECON_HW_TRIG */
		val = HW_TRIG_EN;
		if (en == DECON_TRIG_DISABLE)
				val |= HW_TRIG_MASK_DECON;
		mask = HW_TRIG_EN | HW_TRIG_MASK_DECON;
	}

	decon_write_mask(id, HW_SW_TRIG_CONTROL, val, mask);
}

void decon_reg_update_req_and_unmask(u32 id, struct decon_mode_info *psr)
{
	decon_reg_update_req_global(id);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
}

int decon_reg_wait_update_done_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 100;
	unsigned long cnt = timeout / delay_time;

	while (decon_read(id, SHADOW_REG_UPDATE_REQ) && --cnt)
		udelay(delay_time);

	if (!cnt) {
		decon_err("decon%d timeout of updating decon registers\n", id);
		return -EBUSY;
	}

	return 0;
}

int decon_reg_wait_update_done_and_mask(u32 id,
		struct decon_mode_info *psr, u32 timeout)
{
	int result;

	result = decon_reg_wait_update_done_timeout(id, timeout);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);

	return result;
}

int decon_reg_wait_idle_status_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 status;

	do {
		status = decon_reg_get_idle_status(id);
		cnt--;
		udelay(delay_time);
	} while (!status && cnt);

	if (!cnt) {
		decon_err("decon%d wait timeout decon idle status(%u)\n",
								id, status);
		return -EBUSY;
	}

	return 0;
}

void decon_reg_set_partial_update(u32 id, enum decon_dsi_mode dsi_mode,
		struct exynos_panel_info *lcd_info, bool in_slice[],
		u32 partial_w, u32 partial_h)
{
	/* Here, lcd_info contains the size to be updated */
	decon_reg_set_blender_bg_size(id, partial_w, partial_h);
	decon_reg_set_frame_fifo_size(id, partial_w, partial_h);
}

void decon_reg_set_mres(u32 id, struct decon_param *p)
{
	struct exynos_panel_info *lcd_info = p->lcd_info;

	if (lcd_info->mode != DECON_MIPI_COMMAND_MODE) {
		dsim_info("%s: mode[%d] doesn't support multi resolution\n",
				__func__, lcd_info->mode);
		return;
	}

	decon_reg_set_blender_bg_image_size(id, lcd_info);

	decon_reg_set_frame_fifo_size(id, lcd_info->xres, lcd_info->yres);
}

void decon_reg_release_resource(u32 id, struct decon_mode_info *psr)
{
	decon_reg_per_frame_off(id);
	decon_reg_update_req_global(id);
	decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
}

void decon_reg_config_wb_size(u32 id, struct exynos_panel_info *lcd_info,
		struct decon_param *param)
{
	/*
	 * Exynos3830 can not support WB feature
	 * This function is called from common decon drvier
	 * so It can not be removed.
	 */
}

void decon_reg_set_int(u32 id, struct decon_mode_info *psr, u32 en)
{
	u32 val, mask;

	decon_reg_clear_int_all(id);

	if (en) {
		val = (DPU_FRAME_DONE_INT_EN
			| DPU_FRAME_START_INT_EN
			| DPU_EXTRA_INT_EN
			| DPU_INT_EN);

		decon_write_mask(id, INTERRUPT_ENABLE,
				val, INTERRUPT_ENABLE_MASK);
		decon_dbg("decon %d, interrupt val = %x\n", id, val);

		val = DPU_TIME_OUT_INT_EN;
		decon_write(id, EXTRA_INTERRUPT_ENABLE, val);
	} else {
		mask = (DPU_EXTRA_INT_EN | DPU_INT_EN);
		decon_write_mask(id, INTERRUPT_ENABLE, 0, mask);
	}
}

int decon_reg_get_interrupt_and_clear(u32 id, u32 *ext_irq)
{
	u32 val, val1;
	u32 reg_id;

	reg_id = INTERRUPT_PENDING;
	val = decon_read(id, reg_id);

	if (val & DPU_FRAME_START_INT_PEND)
		decon_write(id, reg_id, DPU_FRAME_START_INT_PEND);

	if (val & DPU_FRAME_DONE_INT_PEND)
		decon_write(id, reg_id, DPU_FRAME_DONE_INT_PEND);

	if (val & DPU_EXTRA_INT_PEND) {
		decon_write(id, reg_id, DPU_EXTRA_INT_PEND);

		reg_id = EXTRA_INTERRUPT_PENDING;
		val1 = decon_read(id, reg_id);
		*ext_irq = val1;

		if (val1 & DPU_TIME_OUT_INT_PEND)
			decon_write(id, reg_id, DPU_TIME_OUT_INT_PEND);
	}

	return val;
}

void decon_reg_set_start_crc(u32 id, u32 en)
{
	decon_write_mask(id, CRC_CONTROL, en ? ~0 : 0, CRC_START);
}

/* bit_sel : 0=B, 1=G, 2=R */
void decon_reg_set_select_crc_bits(u32 id, u32 bit_sel)
{
	u32 val;

	val = CRC_COLOR_SEL(bit_sel);
	decon_write_mask(id, CRC_CONTROL, val, CRC_COLOR_SEL_MASK);
}

void decon_reg_get_crc_data(u32 id, u32 *w0_data, u32 *w1_data)
{
	u32 val;

	val = decon_read(id, CRC_DATA_0);
	*w0_data = CRC_DATA_DSIMIF0_GET(val);
	*w1_data = CRC_DATA_DSIMIF1_GET(val);
}

u32 DPU_DMA2CH(u32 dma)
{
	u32 ch_id;

	switch (dma) {
	case IDMA_VG0:
		ch_id = 0;
		break;
	case IDMA_G1:
		ch_id = 1;
		break;
	case IDMA_G2:
		ch_id = 2;
		break;
	case IDMA_G0:
		ch_id = 3;
		break;
	default:
		decon_dbg("channel(0x%x) is not valid\n", dma);
		return -1;
	}

	return ch_id;

}

u32 DPU_CH2DMA(u32 ch)
{
	u32 dma;

	switch (ch) {
	case 0:
		dma = IDMA_VG0;
		break;
	case 1:
		dma = IDMA_G1;
		break;
	case 2:
		dma = IDMA_G2;
		break;
	case 3:
		dma = IDMA_G0;
		break;
	default:
		decon_warn("channal(%d) is invalid\n", ch);
		return -1;
	}

	return dma;

}

int decon_check_supported_formats(enum decon_pixel_format format)
{
	switch (format) {
	case DECON_PIXEL_FORMAT_ARGB_8888:
	case DECON_PIXEL_FORMAT_ABGR_8888:
	case DECON_PIXEL_FORMAT_RGBA_8888:
	case DECON_PIXEL_FORMAT_BGRA_8888:
	case DECON_PIXEL_FORMAT_XRGB_8888:
	case DECON_PIXEL_FORMAT_XBGR_8888:
	case DECON_PIXEL_FORMAT_RGBX_8888:
	case DECON_PIXEL_FORMAT_BGRX_8888:
	case DECON_PIXEL_FORMAT_RGB_565:
	case DECON_PIXEL_FORMAT_BGR_565:
	case DECON_PIXEL_FORMAT_NV12:
	case DECON_PIXEL_FORMAT_NV12M:
	case DECON_PIXEL_FORMAT_NV21:
	case DECON_PIXEL_FORMAT_NV21M:
	case DECON_PIXEL_FORMAT_NV12N:
#if 0 /*	This is not supported in exynos3830 */
	case DECON_PIXEL_FORMAT_NV12N_10B:

	case DECON_PIXEL_FORMAT_ARGB_2101010:
	case DECON_PIXEL_FORMAT_ABGR_2101010:
	case DECON_PIXEL_FORMAT_RGBA_1010102:
	case DECON_PIXEL_FORMAT_BGRA_1010102:

	case DECON_PIXEL_FORMAT_NV12M_P010:
	case DECON_PIXEL_FORMAT_NV21M_P010:
	case DECON_PIXEL_FORMAT_NV12M_S10B:
	case DECON_PIXEL_FORMAT_NV21M_S10B:
	case DECON_PIXEL_FORMAT_NV12_P010:

	case DECON_PIXEL_FORMAT_NV16:
	case DECON_PIXEL_FORMAT_NV61:
	case DECON_PIXEL_FORMAT_NV16M_P210:
	case DECON_PIXEL_FORMAT_NV61M_P210:
	case DECON_PIXEL_FORMAT_NV16M_S10B:
	case DECON_PIXEL_FORMAT_NV61M_S10B:

	case DECON_PIXEL_FORMAT_NV12M_SBWC_8B:
	case DECON_PIXEL_FORMAT_NV12M_SBWC_10B:
	case DECON_PIXEL_FORMAT_NV21M_SBWC_8B:
	case DECON_PIXEL_FORMAT_NV21M_SBWC_10B:
	case DECON_PIXEL_FORMAT_NV12N_SBWC_8B:
	case DECON_PIXEL_FORMAT_NV12N_SBWC_10B:
#endif
		return 0;
	default:
		break;
	}

	return -EINVAL;
}

#define PREFIX_LEN	40
#define ROW_LEN		32
static void decon_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	size_t i, row;

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

/* base_regs means DECON0's SFR base address */
void __decon_dump(u32 id, void __iomem *regs, void __iomem *base_regs, bool dsc_en)
{
	/* decon debug enable */
	decon_write_mask(id, 0x400, (1 << 31), (1 << 31));

	decon_info("\n=== DECON%d SFR DUMP ===\n", id);
	decon_print_hex_dump(regs, regs + 0x0000, 0x480);

	decon_info("\n=== DECON%d SHADOW SFR DUMP ===\n", id);
	decon_print_hex_dump(regs, regs + SHADOW_OFFSET + 0x0000, 0x304);

	decon_info("\n=== DECON0 WINDOW SFR DUMP ===\n");
	decon_print_hex_dump(base_regs, base_regs + 0x1000, 0x310);

	decon_info("\n=== DECON0 WINDOW SHADOW SFR DUMP ===\n");
	decon_print_hex_dump(base_regs, base_regs + SHADOW_OFFSET + 0x1000, 0x220);
}

/* 3830 chip dependent HW limitation
 *	: returns 0 if no error
 *	: otherwise returns -EPERM for HW-wise not permitted
 */
int decon_check_global_limitation(struct decon_device *decon,
		struct decon_win_config *config)
{
	int ret = 0;
	int i;

	for (i = 0; i < MAX_DECON_WIN; i++) {
		if (config[i].state != DECON_WIN_STATE_BUFFER)
			continue;

		if (config[i].channel >= decon->dt.dpp_cnt) {
			ret = -EINVAL;
			decon_err("invalid dpp channel(%d)\n",
					config[i].channel);
			goto err;
		}
	}

err:
	return ret;
}
