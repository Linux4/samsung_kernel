// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Exynos Pablo IS CSIS_PDP TOP HW control functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-api-common.h"
#include "is-common-config.h"
#include "exynos-is-sensor.h"
#include "pablo-smc.h"
#if IS_ENABLED(CONFIG_CSIS_PDP_TOP_V6_0)
#include "sfr/is-sfr-csi_pdp_top-v6_0.h"
#endif
#if IS_ENABLED(CONFIG_CSIS_PDP_TOP_V6_1)
#include "sfr/is-sfr-csi_pdp_top-v6_1.h"
#endif
#include "api/is-hw-api-csis_pdp_top.h"
#include "is-device-csi.h"

/*
 * [00]: CSIS LInk0 (VC0/1/2/3/4/5)
 * ...
 * [08]: CSIS LInk0 (VC6/7/8/9)
 * ...
 */
#define IBUF_MUX_VAL_BASE_LINK_VC_0_5	0x0
#define IBUF_MUX_VAL_BASE_LINK_VC_6_9	0x8

#define IBUF_ERR_INTR_AUTO_RESET	0
#define CONFIG_IBUF_ERR_CHECK_ON 0

void csis_pdp_top_frame_id_en(struct is_device_csi *csi, struct is_fid_loc *fid_loc)
{
	struct pablo_camif_csis_pdp_top *top = csi->top;
	struct pablo_camif_otf_info *otf_info = &csi->otf_info;
	u32 val = 0;

	if (!top) {
		err("CSIS%d doesn't have top regs.\n", otf_info->csi_ch);
		return;
	}

	if (csi->f_id_dec) {
		if (!fid_loc->valid) {
			fid_loc->byte = 27;
			fid_loc->line = 0;
			warn("fid_loc is NOT properly set.\n");
		}

		val = is_hw_get_reg(top->regs,
				&csis_top_regs[CSIS_R_CSIS0_FRAME_ID_EN + otf_info->csi_ch]);
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_F_FID_LOC_BYTE],
				fid_loc->byte);
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_F_FID_LOC_LINE],
				fid_loc->line);
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_F_FRAME_ID_EN_CSIS], 1);

		info("CSIS%d_FRAME_ID_EN:byte %d line %d\n", otf_info->csi_ch, fid_loc->byte, fid_loc->line);
	} else {
		val = 0;
	}

	is_hw_set_reg(top->regs,
		&csis_top_regs[CSIS_R_CSIS0_FRAME_ID_EN + otf_info->csi_ch], val);

}

u32 csis_pdp_top_get_frame_id_en(void __iomem *base_addr, struct is_device_csi *csi)
{
	return is_hw_get_reg(base_addr,
		&csis_top_regs[CSIS_R_CSIS0_FRAME_ID_EN + csi->otf_info.csi_ch]);
}

void csis_pdp_top_qch_cfg(void __iomem *base_reg, bool on)
{
	u32 val = 0;
	u32 qactive_on, force_bus_act_on;

	if (on) {
		qactive_on = 1;
		force_bus_act_on = 1;
	} else {
		qactive_on = 0;
		force_bus_act_on = 0;
	}

	val = is_hw_set_field_value(val,
			&csis_top_fields[CSIS_F_QACTIVE_ON], qactive_on);
	val = is_hw_set_field_value(val,
			&csis_top_fields[CSIS_F_IP_PROCESSING], 1);
	is_hw_set_reg(base_reg,
		&csis_top_regs[CSIS_R_CSIS_CTRL], val);
}

void csis_pdp_top_irq_msk(void __iomem *base_reg, bool on)
{
	u32 val = 0, msk;

	if (on)
		msk = CSIS_TOP_IBUF_INTR_EN_MASK;
	else
		msk = 0;

	is_hw_set_reg(base_reg,
		&csis_top_regs[CSIS_R_CSIS_TOP_INT_ENABLE], msk);

	if (!IS_ENABLED(IBUF_ERR_INTR_AUTO_RESET)) {
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_F_IBUF_ERR_INTR_CLR_SW_CTRL_EN], 1);
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_F_IBUF_ERR_MODULE_SWRESET_AUTO_DISABLE], 0);
		is_hw_set_reg(base_reg,
			&csis_top_regs[CSIS_R_MISC], val); /* bring up guide */
	}
}

void csis_pdp_top_s_link_vc_list(int *link_vc_list, u32 *mux_val_base,
				 u32 max_vc_num, u32 otf_out_id)
{
	u32 lc, start_vc, end_vc;
	lc = start_vc = end_vc = 0;

	if (max_vc_num <= CSIS_OTF_CH_LC_NUM ||
	    otf_out_id == CSI_OTF_OUT_SINGLE) {
		*mux_val_base = IBUF_MUX_VAL_BASE_LINK_VC_0_5;
		start_vc = 0;
		end_vc = 5;
	} else if (otf_out_id == CSI_OTF_OUT_SHORT) {
		*mux_val_base = IBUF_MUX_VAL_BASE_LINK_VC_6_9;
		start_vc = 6;
		end_vc = 9;
	}

	while (start_vc <= end_vc)
		link_vc_list[lc++] = start_vc++;
}

void csis_pdp_top_s_otf_out_mux(void __iomem *regs,
		u32 csi_ch, u32 otf_ch, u32 img_vc,
		u32 mux_val_base, bool en)
{
	u32 mux_val;

	if (en)
		mux_val = mux_val_base + csi_ch;
	else
		mux_val = 0x3f; /* reset value */

	info("CSI%d -> OTF%d [%s] %s\n", csi_ch, otf_ch,
		(mux_val_base == IBUF_MUX_VAL_BASE_LINK_VC_0_5) ? "VC0-5" : "VC6-9",
		en ? "ON" : "OFF");

	is_hw_set_field(regs, &csis_top_regs[CSIS_R_IBUF_MUX0 + otf_ch],
		&csis_top_fields[CSIS_F_GLUEMUX_IBUF_MUX_SEL0], mux_val);
}

void csis_pdp_top_s_otf_lc(void __iomem *regs, u32 otf_ch, u32 *lc)
{
	u32 val = 0;

	info("OTF%d -> ", otf_ch);

	/* IMG LC */
	val = is_hw_set_field_value(val,
		&csis_top_fields[CSIS_F_MUX_IMG_VC_PDP0_A], lc[CAMIF_VC_IMG]);
	if (lc[CAMIF_VC_IMG] < CSIS_OTF_CH_LC_NUM)
		printk(KERN_CONT "IMG_LC%d", lc[CAMIF_VC_IMG]);
	/* HPD LC */
	val = is_hw_set_field_value(val,
		&csis_top_fields[CSIS_F_MUX_AF0_VC_PDP0_A], lc[CAMIF_VC_HPD]);
	if (lc[CAMIF_VC_HPD] < CSIS_OTF_CH_LC_NUM)
		printk(KERN_CONT ", HPD_LC%d", lc[CAMIF_VC_HPD]);

	/* VPD LC */
	val = is_hw_set_field_value(val,
		&csis_top_fields[CSIS_F_MUX_AF1_VC_PDP0_A], lc[CAMIF_VC_VPD]);
	if (lc[CAMIF_VC_VPD] < CSIS_OTF_CH_LC_NUM)
		printk(KERN_CONT ", VPD_LC%d", lc[CAMIF_VC_VPD]);

	printk(KERN_CONT "\n");

	is_hw_set_reg(regs, &csis_top_regs[CSIS_R_PDP_VC_CON0 + otf_ch], val);
}

void csi_pdp_top_dump(void __iomem *base_reg)
{
	info("TOP REG DUMP\n");
	is_hw_dump_regs(base_reg, csis_top_regs, CSIS_REG_CNT);
}

static bool _get_ibuf_lc_en(u32 otf_lc[CAMIF_VC_ID_NUM], u32 lc)
{
	u32 i;

	for (i = 0; i < CAMIF_VC_ID_NUM; i++) {
		if (otf_lc[i] == lc)
			return true;
	}

	return false;
}

void csis_pdp_top_set_ibuf(void __iomem *base_reg, struct pablo_camif_otf_info *otf_info,
	u32 otf_out_id, struct is_sensor_cfg *sensor_cfg, bool csi_potf)
{
	u32 val = 0;
	u32 otf_ch = otf_info->otf_out_ch[otf_out_id];
	u32 ibuf_reg_offset, ibuf_vc_reg_offset;
	u32 width, height, bit_mode, lc;
	int link_vc;
	bool potf, user_emb, lc_en;
	u32 ibuf_enable = 0;

	ibuf_reg_offset = otf_ch * (CSIS_R_IBUF1_INPUT_CONFIG_CNTL_0 - CSIS_R_IBUF0_INPUT_CONFIG_CNTL_0);
	is_hw_set_field(base_reg,
		&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_CNTL_0 + ibuf_reg_offset],
		&csis_top_fields[CSIS_F_IBUF_STUCK_DETECT], 0); /* bring up guide */

	for (lc = 0; lc < CSIS_OTF_CH_LC_NUM; lc++) {
		potf = false;
		user_emb = false; /* 0:IMG, PD, 1:User / Embedded at POTF */
		lc_en = true;
		ibuf_vc_reg_offset =
			lc * (CSIS_R_IBUF0_INPUT_CONFIG_VC1_0 - CSIS_R_IBUF0_INPUT_CONFIG_VC0_0);

		/**
		 * Disable IBUF err_check function by default.
		 * The reset value is 1(== enable).
		 */
		is_hw_set_field(base_reg,
			&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_1 + ibuf_reg_offset +
				       ibuf_vc_reg_offset],
			&csis_top_fields[CSIS_F_IBUF_ERR_CHECK_ON_VC0], 0);

		link_vc = otf_info->link_vc_list[otf_out_id][lc];
		if (link_vc < 0)
			continue;

		width = sensor_cfg->input[link_vc].width;
		height = sensor_cfg->input[link_vc].height;

		val = 0;
		val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_WIDTH_VC0], width);
		val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_HEIGHT_VC0], height);

		is_hw_set_reg(base_reg,
			&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_0 + ibuf_reg_offset + ibuf_vc_reg_offset], val);

		if (csi_potf || CHECK_POTF_EN(sensor_cfg->input[link_vc].extformat))
			potf = true;

		switch (sensor_cfg->input[link_vc].hwformat) {
		case HW_FORMAT_UNKNOWN:
			lc_en = false;
			fallthrough;
		case HW_FORMAT_RAW8:
			bit_mode = 0;
			break;
		case HW_FORMAT_RAW10:
			bit_mode = 1;
			break;
		case HW_FORMAT_RAW12:
			bit_mode = 2;
			break;
		case HW_FORMAT_RAW14:
			bit_mode = 3;
			break;
		default:
			warn("[@][IBUF][VC%d] Invalid data format 0x%x", link_vc,
					sensor_cfg->input[link_vc].hwformat);
			bit_mode = is_hw_get_field(base_reg,
				&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_1 + ibuf_reg_offset + ibuf_vc_reg_offset],
				&csis_top_fields[CSIS_F_IBUF_BITMODE_VC0]);
			lc_en = false;
			break;
		}

		if (potf && (sensor_cfg->input[link_vc].type == VC_EMBEDDED))
			user_emb = true;

		if (lc_en)
			ibuf_enable |= (1 << lc);
		else
			ibuf_enable &= ~(1 << lc);

		lc_en = lc_en && _get_ibuf_lc_en(otf_info->otf_lc[otf_out_id], lc);
		info("[CSI%d][IBUF%d][LC%d] %s\n", otf_info->csi_ch,
			otf_info->otf_out_ch[otf_out_id], lc, lc_en ? "ON" : "OFF");

		val = 0;
		val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_OTF_EN_VC0], !potf);
		val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_BITMODE_VC0], bit_mode);
		val = is_hw_set_field_value(
			val, &csis_top_fields[CSIS_F_IBUF_USER_EMB_VC0], user_emb);

		if (IS_ENABLED(CONFIG_IBUF_ERR_CHECK_ON))
			val = is_hw_set_field_value(
				val, &csis_top_fields[CSIS_F_IBUF_ERR_CHECK_ON_VC0], lc_en);

		is_hw_set_reg(base_reg,
			&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_1 + ibuf_reg_offset +
				       ibuf_vc_reg_offset],
			val);
	}

#if IS_ENABLED(CONFIG_CSIS_PDP_TOP_V6_1)
	is_hw_set_field(base_reg, &csis_top_regs[CSIS_R_VC_CTRL_0], &csis_top_fields[CSIS_F_IBUF0 + otf_ch], ibuf_enable);
#endif
}
KUNIT_EXPORT_SYMBOL(csis_pdp_top_set_ibuf);

static void get_ibuf_ptrn_gen_vhd_blank(
	u32 width, u32 height, u32 fps, u32 clk, u32 *vblank, u32 *hblank, u32 *dblank)
{
	const struct is_field *h2h_field = &csis_top_fields[CSIS_F_IBUF_OUT_H2H_BLANK];
	const struct is_field *d2d_field = &csis_top_fields[CSIS_F_IBUF_PTRN_D2D_BLANK];
	const u32 dvalid = 8; /* 8 cycle fixed */
	u32 hvalid, vvalid, line_length, frame_length; /* unit: cycle */
	u32 tmp_dblank, tmp_hblank; /* unit: cycle */
	u32 vvalid_mul;

	if (fps <= 30)
		vvalid_mul = 2;
	else if (fps <= 60)
		vvalid_mul = 3;
	else
		vvalid_mul = 5;

	frame_length = (clk * 1000 * 1000) / fps;
	hvalid = ALIGN(width, 8) / 8; /* 8ppc */
	vvalid = frame_length * vvalid_mul / 6;

	line_length = vvalid / height;

	if (hvalid >= line_length) {
		tmp_dblank = 0;
		tmp_hblank = 16; /* minimum value */
	} else if ((line_length - hvalid) >> h2h_field->bit_width) {
		tmp_hblank = GENMASK(h2h_field->bit_width - 1, 0); /* maximum value */
		tmp_dblank = (line_length - hvalid - tmp_hblank) / (width / 8); /* 8ppc */
		if (tmp_dblank >> d2d_field->bit_width)
			tmp_dblank = GENMASK(d2d_field->bit_width - 1, 0); /* maximum value */
	} else {
		tmp_dblank = 0;
		tmp_hblank = line_length - hvalid;
	}

	*vblank = frame_length - vvalid;
	*hblank = tmp_hblank;
	*dblank = tmp_dblank;

	info("[IBUF-PTR_GEN] V(%d/%d) H(%d/%d) D(%d/%d)\n", vvalid, *vblank, hvalid, *hblank,
		dvalid, *dblank);
}

void csis_pdp_top_enable_ibuf_ptrn_gen(void __iomem *base_reg, struct is_sensor_cfg *sensor_cfg,
	struct pablo_camif_otf_info *otf_info, u32 clk, bool on)
{
	u32 val = 0;
	u32 width = sensor_cfg->input[CSI_VIRTUAL_CH_0].width;
	u32 height = sensor_cfg->input[CSI_VIRTUAL_CH_0].height;
	u32 fps = sensor_cfg->framerate;
	u32 otf_ch = otf_info->otf_out_ch[CSI_OTF_OUT_SINGLE];
	u32 reg_base_offset = otf_ch * (CSIS_R_IBUF1_INPUT_CONFIG_CNTL_0 - CSIS_R_IBUF0_INPUT_CONFIG_CNTL_0);
	u32 dblank = 0, hblank = 16, vblank = 16;

	/* VHD blank configuration */
	if (on)
		get_ibuf_ptrn_gen_vhd_blank(width, height, fps, clk, &vblank, &hblank, &dblank);

	is_hw_set_field(base_reg,
		&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_PTRN_0 + reg_base_offset],
		&csis_top_fields[CSIS_F_IBUF_PTRN_D2D_BLANK], dblank);
	is_hw_set_field(base_reg,
		&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_CNTL_0 + reg_base_offset],
		&csis_top_fields[CSIS_F_IBUF_OUT_H2H_BLANK], hblank);
	is_hw_set_field(base_reg, &csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_2 + reg_base_offset],
		&csis_top_fields[CSIS_F_IBUF_OUT_V2V_BLANK_VC0], vblank);

	/* Pattern_Gen configuration */
	val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_PTRN_GEN_ON], on);
	val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_PTRN_SENSOR_SYNC], 0);
	val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_PTRN_DATA_TYPE], 3); /* 3 : color bar */
	is_hw_set_reg(base_reg, &csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_PTRN_0 + reg_base_offset], val);

	info("[IBUF%d] %s Pattern Generator (%dx%d@%d, %dMHz)\n", otf_ch, on ? "Enable" : "Disable",
		width, height, fps, clk);
}
KUNIT_EXPORT_SYMBOL(csis_pdp_top_enable_ibuf_ptrn_gen);

ulong csis_pdp_top_irq_src(void __iomem *base_reg)
{
	ulong ret = 0;
	unsigned long src;
	u32 ch, ibuf_err_config_reg_offset, err_ovf, err_stuck, err_wrongsize, err_protocol, hcnt,
		vcnt;
	ulong config0, config1;

	err_ovf = err_stuck = err_wrongsize = err_protocol = 0;

	src = is_hw_get_reg(base_reg, &csis_top_regs[CSIS_R_CSIS_TOP_INTR_SOURCE]);

	for (ch = 0; ch < MAX_NUM_CSIS_OTF_CH; ch++) {
		if (test_bit(INTR_CSIS_TOP_IBUF_CH0_ERR_INT + ch, &src)) {
			ibuf_err_config_reg_offset =
				(CSIS_R_IBUF1_ERR_CONFIG_0 - CSIS_R_IBUF0_ERR_CONFIG_0) * ch;

			config0 = is_hw_get_reg(base_reg,
				&csis_top_regs[CSIS_R_IBUF0_ERR_CONFIG_0 + ibuf_err_config_reg_offset]);
			config1 = is_hw_get_reg(base_reg,
				&csis_top_regs[CSIS_R_IBUF0_ERR_CONFIG_1 + ibuf_err_config_reg_offset]);

			err_ovf = is_hw_get_field_value(config0,
							&csis_top_fields[CSIS_F_IBUF_ERR_OVERFLOW]);
			err_stuck = is_hw_get_field_value(config0,
							  &csis_top_fields[CSIS_F_IBUF_ERR_STUCK]);
			err_wrongsize = is_hw_get_field_value(
				config0, &csis_top_fields[CSIS_F_IBUF_ERR_WRONGSIZE]);
			err_protocol = is_hw_get_field_value(
				config0, &csis_top_fields[CSIS_F_IBUF_ERR_PROTOCOL]);

			hcnt = is_hw_get_field_value(config1,
						     &csis_top_fields[CSIS_F_IBUF_ERR_HCNT]);
			vcnt = is_hw_get_field_value(config1,
						     &csis_top_fields[CSIS_F_IBUF_ERR_VCNT]);

			if (err_wrongsize)
				set_bit(ch + IBUF0_ERR_WRONGSIZE, &ret);

			err("IBUF ERR OTF_CH%d : OVF(%d) STUCK(%d) WRONGSIZE(%d) PROTOCOL(%d) CNT(%dx%d)",
			    ch, err_ovf, err_stuck, err_wrongsize, err_protocol, hcnt, vcnt);
		}
	}

	if (err_ovf) {
		set_bit(IBUF_ERR_OVERFLOW, &ret);
#if IS_ENABLED(CONFIG_SEC_ABC)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=camera@INFO=mipi_overflow");
#else
		sec_abc_send_event("MODULE=camera@WARN=mipi_overflow");
#endif
#endif
	}

	/* clear */
	if (!IS_ENABLED(IBUF_ERR_INTR_AUTO_RESET))
		is_hw_set_reg(base_reg, &csis_top_regs[CSIS_R_CSIS_TOP_INTR_SOURCE], src);

	return ret;
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_hw_csis_pdp_top_func pablo_kunit_hw_csis_pdp_top = {
	.frame_id_en = csis_pdp_top_frame_id_en,
	.get_frame_id_en = csis_pdp_top_get_frame_id_en,
	.qch_cfg = csis_pdp_top_qch_cfg,
	.s_link_vc_list = csis_pdp_top_s_link_vc_list,
	.s_otf_out_mux = csis_pdp_top_s_otf_out_mux,
	.s_otf_lc = csis_pdp_top_s_otf_lc,
	.irq_msk = csis_pdp_top_irq_msk,
	.irq_src = csis_pdp_top_irq_src,
};

struct pablo_kunit_hw_csis_pdp_top_func *pablo_kunit_get_hw_csis_pdp_top_test(void)
{
	return &pablo_kunit_hw_csis_pdp_top;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_hw_csis_pdp_top_test);
#endif

