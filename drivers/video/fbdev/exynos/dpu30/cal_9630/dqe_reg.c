/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../decon.h"
#include "../dqe.h"
#include "regs-dqe.h"

/* DQE_CON register set */
void dqe_reg_set_dqecon_all_reset(void)
{
	dqe_write_mask(DQECON, 0, DQECON_ALL_MASK);
}

void dqe_reg_set_gamma_on(u32 on)
{
	dqe_write_mask(DQECON, on ? ~0 : 0, DQE_GAMMA_ON_MASK);
}

u32 dqe_reg_get_gamma_on(void)
{
	return dqe_read_mask(DQECON, DQE_GAMMA_ON_MASK);
}

void dqe_reg_set_cgc_on(u32 on)
{
	dqe_write_mask(DQECON, on ? ~0 : 0, DQE_CGC_ON_MASK);
}

u32 dqe_reg_get_cgc_on(void)
{
	return dqe_read_mask(DQECON, DQE_CGC_ON_MASK);
}

void dqe_reg_set_hsc_on(u32 on)
{
	dqe_write_mask(DQECON, on ? ~0 : 0, DQE_HSC_ON_MASK);
}

u32 dqe_reg_get_hsc_on(void)
{
	return dqe_read_mask(DQECON, DQE_HSC_ON_MASK);
}

void dqe_reg_hsc_sw_reset(struct decon_device *decon)
{
	u32 cnt = 5000; /* 3 frame */
	u32 state;
	struct decon_mode_info psr;

	dqe_write_mask(DQECON, ~0, DQE_HSC_SW_RESET_MASK);

	decon_to_psr_info(decon, &psr);
	decon_reg_update_req_dqe(decon->id);
	decon_reg_set_trigger(decon->id, &psr, DECON_TRIG_ENABLE);

	do {
		state = dqe_read_mask(DQECON, DQE_HSC_SW_RESET_MASK);
		cnt--;
		udelay(10);
	} while (state && cnt);

	if (!cnt)
		dqe_err("%s is timeout.\n", __func__);

	dqe_dbg("dqe hsc_sw_reset:%d cnt:%d\n",
		DQE_HSC_SW_RESET_GET(dqe_read(DQECON)), cnt);
}

void dqe_reg_set_hsc_full_pxl_num(struct exynos_panel_info *lcd_info)
{
	u32 val, mask;

	val = (u32)(lcd_info->xres * lcd_info->yres);
	mask = HSC_FULL_PXL_NUM_MASK;
	dqe_write_mask(DQEHSC_CONTROL1, val, mask);
}

void dqe_reg_set_hsc_partial_frame(struct exynos_panel_info *full_info, struct exynos_panel_info *lcd_info)
{
	u32 vsize, val, mask;

	val = 0;
	vsize = DQEIMG_VSIZE_GET(dqe_read(DQEIMG_SIZESET));

	if (full_info->yres > lcd_info->yres) {
		if (vsize > lcd_info->yres)
			val = HSC_ROI_SAME(1) | HSC_IMG_PARTIAL_FRAME(1);
		else
			val = HSC_ROI_SAME(0) | HSC_IMG_PARTIAL_FRAME(1);
	}

	mask = HSC_IMG_PARTIAL_FRAME_MASK | HSC_ROI_SAME_MASK;
	dqe_write_mask(DQEHSC_CONTROL0, val, mask);
	dqe_info("DQEHSC_CONTROL0 (%d, %d): %x\n", vsize, lcd_info->yres, dqe_read(DQEHSC_CONTROL0));
}

u32 dqe_reg_get_hsc_full_pxl_num(void)
{
	return dqe_read_mask(DQEHSC_CONTROL1, HSC_FULL_PXL_NUM_MASK);
}

void dqe_reg_set_lpd_mode_exit(u32 on)
{
	dqe_write_mask(DQECON, on ? ~0 : 0, DQE_LPD_MODE_EXIT_MASK);
}

void dqe_reg_hsc_lpd_read(struct dqe_device *dqe)
{

	u32 val, mask;

	val = DQE_LPD_WR_DIR(0) | DQE_LPD_ADDR(0);
	mask = DQE_LPD_WR_DIR_MASK | DQE_LPD_ADDR_MASK;
	dqe_write_mask(DQE_LPD_DATA_CONTROL, val, mask);

	mask = DQE_LPD_DATA_MASK;
	dqe->lpd_data[0] = dqe_read_mask(DQE_LPD_DATA_CONTROL, mask);


	val = DQE_LPD_WR_DIR(0) | DQE_LPD_ADDR(1);
	mask = DQE_LPD_WR_DIR_MASK | DQE_LPD_ADDR_MASK;
	dqe_write_mask(DQE_LPD_DATA_CONTROL, val, mask);

	mask = DQE_LPD_DATA_MASK;
	dqe->lpd_data[1] = dqe_read_mask(DQE_LPD_DATA_CONTROL, mask);
}

void dqe_reg_hsc_lpd_write(struct dqe_device *dqe)
{

	u32 val[2] = {0,};
	u32 mask;

	val[0] = DQE_LPD_WR_DIR(1) | DQE_LPD_ADDR(0) | DQE_LPD_DATA(dqe->lpd_data[0]) ;
	mask = DQE_LPD_DATA_ALL_MASK;
	dqe_write_mask(DQE_LPD_DATA_CONTROL, val[0], mask);

	val[1] = DQE_LPD_WR_DIR(1) | DQE_LPD_ADDR(1) | DQE_LPD_DATA(dqe->lpd_data[1]) ;
	mask = DQE_LPD_DATA_ALL_MASK;
	dqe_write_mask(DQE_LPD_DATA_CONTROL, val[1], mask);

	dqe_dbg("dqe lpd data write lpd_data[0](%x) lpd_data[1](%x) DQECON (%x)\n", val[0], val[1], dqe_read(DQECON));
}

void dqe_reg_aps_lpd_read(struct dqe_device *dqe)
{
	u32 val, mask;
	u32 i;

	for (i = 2; i < LPD_DATA_SIZE; i++) {
		val = DQE_LPD_WR_DIR(0) | DQE_LPD_ADDR(i);
		mask = DQE_LPD_WR_DIR_MASK | DQE_LPD_ADDR_MASK;
		dqe_write_mask(DQE_LPD_DATA_CONTROL, val, mask);

		mask = DQE_LPD_DATA_MASK;
		dqe->lpd_data[i] = dqe_read_mask(DQE_LPD_DATA_CONTROL, mask);
	}
}

void dqe_reg_aps_lpd_write(struct dqe_device *dqe)
{
	u32 val, mask;
	u32 i;

	for (i = 2; i < LPD_DATA_SIZE; i++) {
		val = DQE_LPD_WR_DIR(1) | DQE_LPD_ADDR(i) | DQE_LPD_DATA(dqe->lpd_data[i]) ;
		mask = DQE_LPD_DATA_ALL_MASK;
		dqe_write_mask(DQE_LPD_DATA_CONTROL, val, mask);

		dqe_dbg("dqe lpd data write lpd_data[%d](%x) DQECON (%x)\n",
				i, val, dqe_read(DQECON));
	}
}

void dqe_reg_hsc_lpd_read_for_hdr(struct dqe_device *dqe)
{

	u32 val, mask;

	val = DQE_LPD_WR_DIR(0) | DQE_LPD_ADDR(0);
	mask = DQE_LPD_WR_DIR_MASK | DQE_LPD_ADDR_MASK;
	dqe_write_mask(DQE_LPD_DATA_CONTROL, val, mask);

	mask = DQE_LPD_DATA_MASK;
	dqe->lpd_data_for_hdr[0] = dqe_read_mask(DQE_LPD_DATA_CONTROL, mask);

	val = DQE_LPD_WR_DIR(0) | DQE_LPD_ADDR(1);
	mask = DQE_LPD_WR_DIR_MASK | DQE_LPD_ADDR_MASK;
	dqe_write_mask(DQE_LPD_DATA_CONTROL, val, mask);

	mask = DQE_LPD_DATA_MASK;
	dqe->lpd_data_for_hdr[1] = dqe_read_mask(DQE_LPD_DATA_CONTROL, mask);
}

void dqe_reg_hsc_lpd_write_for_hdr(struct dqe_device *dqe)
{

	u32 val[2] = {0,};
	u32 mask;

	val[0] = DQE_LPD_WR_DIR(1) | DQE_LPD_ADDR(0) | DQE_LPD_DATA(dqe->lpd_data_for_hdr[0]) ;
	mask = DQE_LPD_DATA_ALL_MASK;
	dqe_write_mask(DQE_LPD_DATA_CONTROL, val[0], mask);

	val[1] = DQE_LPD_WR_DIR(1) | DQE_LPD_ADDR(1) | DQE_LPD_DATA(dqe->lpd_data_for_hdr[1]) ;
	mask = DQE_LPD_DATA_ALL_MASK;
	dqe_write_mask(DQE_LPD_DATA_CONTROL, val[1], mask);

	dqe_dbg("dqe lpd data for hdr write (%x, %x) DQECON (%x)\n", val[0], val[1], dqe_read(DQECON));
}

void dqe_reg_aps_lpd_read_for_hdr(struct dqe_device *dqe)
{
	u32 val, mask;
	u32 i;

	for (i = 2; i < LPD_DATA_SIZE; i++) {
		val = DQE_LPD_WR_DIR(0) | DQE_LPD_ADDR(i);
		mask = DQE_LPD_WR_DIR_MASK | DQE_LPD_ADDR_MASK;
		dqe_write_mask(DQE_LPD_DATA_CONTROL, val, mask);

		mask = DQE_LPD_DATA_MASK;
		dqe->lpd_data_for_hdr[i] = dqe_read_mask(DQE_LPD_DATA_CONTROL, mask);
	}
}

void dqe_reg_aps_lpd_write_for_hdr(struct dqe_device *dqe)
{
	u32 val, mask;
	u32 i;

	for (i = 2; i < LPD_DATA_SIZE; i++) {
		val = DQE_LPD_WR_DIR(1) | DQE_LPD_ADDR(i) | DQE_LPD_DATA(dqe->lpd_data_for_hdr[i]) ;
		mask = DQE_LPD_DATA_ALL_MASK;
		dqe_write_mask(DQE_LPD_DATA_CONTROL, val, mask);

		dqe_dbg("dqe lpd data write lpd_data[%d](%x) DQECON (%x)\n",
				i, val, dqe_read(DQECON));
	}
}

void dqe_reg_set_aps_on(u32 on)
{
	dqe_write_mask(DQECON, on ? ~0 : 0, DQE_APS_ON_MASK);
}

u32 dqe_reg_get_aps_on(void)
{
	return dqe_read_mask(DQECON, DQE_APS_ON_MASK);
}

void dqe_reg_set_aps_ibsi(struct exynos_panel_info *lcd_info)
{
	u32 val, val1, val2;
	u32 width = lcd_info->xres;
	u32 height = lcd_info->yres;
	u32 small_n = width / 8;
	u32 large_N = small_n + 1;
	u32 grid = height / 16;

	val1 = (width % 8 == 0) ? ((1 << 16) / (small_n * 4)) : ((1 << 16) / (large_N * 4));
	val2 = (height % 16 == 0) ? ((1 << 16) / (grid * 4)) : ((1 << 16) / (grid * 4) + 4);
	val = DQEAPSLUT_IBSI_00(val1) | DQEAPSLUT_IBSI_01(val2);
	dqe_write(DQEAPS_PARTIAL_IBSI_01_00, val);

	val1 = (width % 8 == 0) ? ((1 << 16) / (small_n * 2)) : ((1 << 16) / (large_N * 2));
	val2 = (height % 16 == 0) ? ((1 << 16) / (grid * 2)) : ((1 << 16) / (grid * 2) + 2);
	val = DQEAPSLUT_IBSI_10(val1) | DQEAPSLUT_IBSI_11(val2);
	dqe_write(DQEAPS_PARTIAL_IBSI_11_10, val);
}

void dqe_reg_set_aps_full_pxl_num(struct exynos_panel_info *lcd_info)
{
	u32 val, mask;

	val = (u32)(lcd_info->xres * lcd_info->yres);
	mask = DQEAPS_FULL_PXL_NUM_MASK;
	dqe_write_mask(DQEAPS_FULL_PXL_NUM, val, mask);
}

u32 dqe_reg_get_aps_full_pxl_num(void)
{
	return dqe_read_mask(DQEAPS_FULL_PXL_NUM, DQEAPS_FULL_PXL_NUM_MASK);
}

void dqe_reg_set_aps_img_size(struct exynos_panel_info *lcd_info)
{
	u32 val, mask;

	val = DQEAPS_FULL_IMG_VSIZE_F(lcd_info->yres) | DQEAPS_FULL_IMG_HSIZE_F(lcd_info->xres);
	mask = DQEAPS_FULL_IMG_VSIZE_MASK | DQEAPS_FULL_IMG_HSIZE_MASK;
	dqe_write_mask(DQEAPS_FULL_IMG_SIZESET, val, mask);
}

void dqe_reg_set_img_size(u32 id, u32 xres, u32 yres)
{
	u32 val, mask;

	if (id != 0)
		return;

	val = DQEIMG_VSIZE_F(yres) | DQEIMG_HSIZE_F(xres);
	mask = DQEIMG_VSIZE_MASK | DQEIMG_HSIZE_MASK;
	dqe_write_mask(DQEIMG_SIZESET, val, mask);
}

void dqe_reg_set_data_path_enable(u32 id, u32 en)
{
	u32 val;
	enum decon_data_path d_path = DPATH_DSCENC0_OUTFIFO0_DSIMIF0;
	enum decon_scaler_path s_path = SCALERPATH_OFF;
	enum decon_enhance_path e_path = ENHANCEPATH_ENHANCE_ALL_OFF;

	if (id != 0)
		return;

	val = en ? ENHANCEPATH_DQE_ON : ENHANCEPATH_ENHANCE_ALL_OFF;
	decon_reg_get_data_path(id, &d_path, &s_path, &e_path);
	decon_reg_set_data_path(id, d_path, s_path, val);
}

void dqe_reg_start(u32 id, struct exynos_panel_info *lcd_info)
{
	dqe_reg_set_data_path_enable(id, 1);
	dqe_reg_set_img_size(id, lcd_info->xres, lcd_info->yres);
}

void dqe_reg_stop(u32 id)
{
	dqe_reg_set_data_path_enable(id, 0);
}

