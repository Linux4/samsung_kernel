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
#include "sfr/is-sfr-csi_pdp_top-v6_0.h"
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

void csis_pdp_top_qch_cfg(u32 __iomem *base_reg, bool on)
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

	is_hw_set_reg(base_reg,
		&csis_top_regs[CSIS_R_CSIS_CTRL], val);
}

void csis_pdp_top_s_otf_out_mux(void __iomem *regs,
		u32 csi_ch, u32 otf_ch, u32 img_vc,
		u32* link_vc_list, bool en)
{
	u32 mux_val, start_vc, end_vc, lc;

	memset(link_vc_list, CSI_VIRTUAL_CH_MAX, sizeof(u32) * CSIS_OTF_CH_LC_NUM);

	if (img_vc < CSIS_OTF_CH_LC_NUM) {
		mux_val = IBUF_MUX_VAL_BASE_LINK_VC_0_5 + csi_ch;
		start_vc = 0;
		end_vc = 5;
	} else {
		mux_val = IBUF_MUX_VAL_BASE_LINK_VC_6_9 + csi_ch;
		start_vc = 6;
		end_vc = 9;
	}

	info("CSI%d -> OTF%d [VC%d-%d] %s\n", csi_ch, otf_ch, start_vc, end_vc,
			en ? "ON" : "OFF");

	if (!en) {
		mux_val = 0x3f; /* reset value */
	} else {
		lc = 0;
		while (start_vc <= end_vc)
			link_vc_list[lc++] = start_vc++;
	}

	is_hw_set_field(regs, &csis_top_regs[CSIS_R_IBUF_MUX0 + otf_ch],
		&csis_top_fields[CSIS_F_GLUEMUX_IBUF_MUX_SEL0], mux_val);
}

void csis_pdp_top_s_otf_lc(void __iomem *regs, u32 otf_ch, u32 *lc, u32 lc_num)
{
	u32 val = 0;

	info("OTF%d -> ", otf_ch);

	/* IMG LC */
	if (lc_num > 0) {
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_F_MUX_IMG_VC_PDP0_A], lc[0]);
		printk(KERN_CONT " IMG_LC%d", lc[0]);
	}

	/* HPD LC */
	if (lc_num > 1) {
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_F_MUX_AF1_VC_PDP0_A], lc[1]);
		printk(KERN_CONT ", HPD_LC%d", lc[1]);
	}

	/* VPD LC */
	if (lc_num > 2) {
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_F_MUX_AF1_VC_PDP0_A], lc[2]);
		printk(KERN_CONT ", VPD_LC%d", lc[2]);
	}

	printk(KERN_CONT "\n");

	is_hw_set_reg(regs, &csis_top_regs[CSIS_R_PDP_VC_CON0 + otf_ch], val);
}

void csi_pdp_top_dump(u32 __iomem *base_reg)
{
	info("TOP REG DUMP\n");
	is_hw_dump_regs(base_reg, csis_top_regs, CSIS_REG_CNT);
}

void csis_pdp_top_set_ibuf(u32 __iomem *base_reg, u32 otf_ch, u32 otf_out_id,
		u32* link_vc_list, struct is_sensor_cfg *sensor_cfg)
{
	u32 val = 0;
	u32 hblank, vblank, ibuf_reg_offset, ibuf_vc_reg_offset;
	u32 width, height, bit_mode, user_emb, lc, link_vc;
	bool potf;

	hblank = vblank = 16;
	user_emb = 0; /* 0:IMG, PD, 1:User / Embedded at POTF */

	ibuf_reg_offset = otf_ch * (CSIS_R_IBUF1_INPUT_CONFIG_CNTL_0 - CSIS_R_IBUF0_INPUT_CONFIG_CNTL_0);

	is_hw_set_field(base_reg, &csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_CNTL_0 + ibuf_reg_offset],
			&csis_top_fields[CSIS_F_IBUF_OUT_H2H_BLANK], hblank);

	for (lc = 0; lc < CSIS_OTF_CH_LC_NUM; lc++) {
		link_vc = link_vc_list[lc];

		if (link_vc >= CSI_VIRTUAL_CH_MAX)
			continue;

		ibuf_vc_reg_offset = lc * (CSIS_R_IBUF0_INPUT_CONFIG_VC1_0 - CSIS_R_IBUF0_INPUT_CONFIG_VC0_0);

		width = sensor_cfg->input[link_vc].width;
		height = sensor_cfg->input[link_vc].height;

		val = 0;
		val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_WIDTH_VC0], width);
		val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_HEIGHT_VC0], height);

		is_hw_set_reg(base_reg,
			&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_0 + ibuf_reg_offset + ibuf_vc_reg_offset], val);

		potf = CHECK_POTF_EN(sensor_cfg->input[link_vc].extformat);

		switch (sensor_cfg->input[link_vc].hwformat) {
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
			break;
		}

		if (potf && (sensor_cfg->input[link_vc].type == VC_EMBEDDED))
			user_emb = 1;

		val = 0;
		val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_OTF_EN_VC0], !potf);
		val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_BITMODE_VC0], bit_mode);
		val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_USER_EMB_VC0], user_emb);
		is_hw_set_reg(base_reg,
			&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_1  + ibuf_reg_offset + ibuf_vc_reg_offset], val);

		is_hw_set_reg(base_reg,
			&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_2 + ibuf_reg_offset + ibuf_vc_reg_offset], vblank);
	}
}
KUNIT_EXPORT_SYMBOL(csis_pdp_top_set_ibuf);

void csis_pdp_top_enable_ibuf_ptrn_gen(u32 __iomem *base_reg, u32 otf_ch, bool sensor_sync)
{
	u32 val = 0;
	u32 reg_base_offset = otf_ch * (CSIS_R_IBUF1_INPUT_CONFIG_CNTL_0 - CSIS_R_IBUF0_INPUT_CONFIG_CNTL_0);

	val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_PTRN_GEN_ON], 1);
	val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_PTRN_SENSOR_SYNC], sensor_sync);
	val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_PTRN_D2D_BLANK], 0);
	val = is_hw_set_field_value(val, &csis_top_fields[CSIS_F_IBUF_PTRN_DATA_TYPE], 3); /* 3 : color bar */
	is_hw_set_reg(base_reg, &csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_PTRN_0 + reg_base_offset], val);
}
KUNIT_EXPORT_SYMBOL(csis_pdp_top_enable_ibuf_ptrn_gen);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_hw_csis_pdp_top_func pablo_kunit_hw_csis_pdp_top = {
	.frame_id_en = csis_pdp_top_frame_id_en,
	.get_frame_id_en = csis_pdp_top_get_frame_id_en,
	.qch_cfg = csis_pdp_top_qch_cfg,
	.s_otf_out_mux = csis_pdp_top_s_otf_out_mux,
	.s_otf_lc = csis_pdp_top_s_otf_lc,
};

struct pablo_kunit_hw_csis_pdp_top_func *pablo_kunit_get_hw_csis_pdp_top_test(void)
{
	return &pablo_kunit_hw_csis_pdp_top;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_hw_csis_pdp_top_test);
#endif

