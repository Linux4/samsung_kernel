// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Exynos Pablo IS CSIS_PDP TOP HW control functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-api-common.h"
#include "is-common-config.h"
#include "exynos-is-sensor.h"
#include "pablo-smc.h"
#include "sfr/is-sfr-csi_pdp_top-v5_20.h"
#include "api/is-hw-api-csis_pdp_top.h"
#include "is-device-csi.h"

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
				&csis_top_regs[CSIS_TOP_R_CSIS0_FRAME_ID_EN + otf_info->csi_ch]);
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_TOP_F_FID_LOC_BYTE_CSIS0],
				fid_loc->byte);
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_TOP_F_FID_LOC_LINE_CSIS0],
				fid_loc->line);
		val = is_hw_set_field_value(val,
				&csis_top_fields[CSIS_TOP_F_FRAME_ID_EN_CSIS0], 1);

		info("CSIS%d_FRAME_ID_EN:byte %d line %d\n", otf_info->csi_ch, fid_loc->byte, fid_loc->line);
	} else {
		val = 0;
	}

	is_hw_set_reg(top->regs,
		&csis_top_regs[CSIS_TOP_R_CSIS0_FRAME_ID_EN + otf_info->csi_ch], val);

}

u32 csis_pdp_top_get_frame_id_en(void __iomem *base_addr, struct is_device_csi *csi)
{
	return is_hw_get_reg(base_addr,
		&csis_top_regs[CSIS_TOP_R_CSIS0_FRAME_ID_EN + csi->otf_info.csi_ch]);
}

void csis_pdp_top_qch_cfg(void __iomem *base_reg, bool on)
{
	u32 val = 0;
	u32 qactive_on, ip_processing;

	if (on) {
		qactive_on = 1;
		ip_processing = 1;
	} else {
		qactive_on = 0;
		ip_processing = 0;
	}

	val = is_hw_set_field_value(val,
			&csis_top_fields[CSIS_TOP_F_QACTIVE_ON], qactive_on);
	val = is_hw_set_field_value(val,
			&csis_top_fields[CSIS_TOP_F_IP_PROCESSING], ip_processing);
	is_hw_set_reg(base_reg,
		&csis_top_regs[CSIS_TOP_R_CSIS_CTRL], val);
}
KUNIT_EXPORT_SYMBOL(csis_pdp_top_qch_cfg);

#define NUM_CSI_LINK 5
void csis_pdp_top_s_otf_out_mux(void __iomem *regs,
		u32 csi_ch, u32 otf_ch, u32 img_vc,
		u32 mux_val_base, bool en)
{
	u32 mux_val;

	if (en)
		mux_val = csi_ch + (NUM_CSI_LINK * (img_vc / DMA_VIRTUAL_CH_MAX));
	else
		mux_val = 0x3f; /* reset value */

	info("CSI%d -> OTF%d [VC%d-%d] %s\n", csi_ch, otf_ch,
			img_vc, (img_vc + DMA_VIRTUAL_CH_MAX - 1),
			en ? "ON" : "OFF");


	is_hw_set_reg(regs,
		&csis_top_regs[CSIS_TOP_R_IBUF_MUX0 + otf_ch], mux_val);
}

void csis_pdp_top_s_otf_lc(void __iomem *regs, u32 otf_ch, u32 *lc)
{
	u32 val = 0;

	info("OTF%d -> ", otf_ch);

	/* IMG LC */
	val = is_hw_set_field_value(val,
		&csis_top_fields[CSIS_TOP_F_MUX_IMG_VC_PDP0], lc[CAMIF_VC_IMG]);
	if (lc[CAMIF_VC_IMG] < CSIS_OTF_CH_LC_NUM)
		printk(KERN_CONT "IMG_LC%d", lc[CAMIF_VC_IMG]);

	/* HPD LC */
	val = is_hw_set_field_value(val,
		&csis_top_fields[CSIS_TOP_F_MUX_AF0_VC_PDP0], lc[CAMIF_VC_HPD]);
	if (lc[CAMIF_VC_HPD] < CSIS_OTF_CH_LC_NUM)
		printk(KERN_CONT ", HPD_LC%d", lc[CAMIF_VC_HPD]);

	/* VPD LC */
	val = is_hw_set_field_value(val,
		&csis_top_fields[CSIS_TOP_F_MUX_AF1_VC_PDP0], 0xf); /* pdp input vpd disable*/

	printk(KERN_CONT "\n");

	is_hw_set_reg(regs, &csis_top_regs[CSIS_TOP_R_PDP_VC_CON0 + otf_ch], val);
}

void csi_pdp_top_dump(void __iomem *base_reg)
{
	info("TOP REG DUMP\n");
	is_hw_dump_regs(base_reg, csis_top_regs, CSIS_TOP_REG_CNT);
}

void csis_pdp_top_s_link_vc_list(
	int *link_vc_list, u32 *mux_val_base, u32 max_vc_num, u32 otf_out_id)
{
	/* Not supported API */
}

void csis_pdp_top_set_ibuf(void __iomem *base_reg, struct pablo_camif_otf_info *otf_info,
	u32 otf_out_id, struct is_sensor_cfg *sensor_cfg, bool csi_potf)
{
	/* Not supported API */
}

void csis_pdp_top_enable_ibuf_ptrn_gen(void __iomem *base_reg, struct is_sensor_cfg *sensor_cfg,
	struct pablo_camif_otf_info *otf_info, u32 clk, bool on)
{
	/* Not supported API */
}

void csis_pdp_top_irq_msk(void __iomem *base_reg, bool on)
{
	/* Not supported API */
}

ulong csis_pdp_top_irq_src(void __iomem *base_reg)
{
	/* Not supported API */
	return 0;
}

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

