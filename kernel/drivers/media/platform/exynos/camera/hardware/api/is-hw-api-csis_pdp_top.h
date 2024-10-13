/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * CSIS_PDP TOP HW control APIs
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_CSIS_PDP_TOP_H
#define IS_HW_API_CSIS_PDP_TOP_H
#include "exynos-is-sensor.h"

struct is_device_csi;
struct pablo_camif_otf_info;

#if IS_ENABLED(CONFIG_CSIS_PDP_TOP_API)
void csis_pdp_top_frame_id_en(struct is_device_csi *csi, struct is_fid_loc *fid_loc);
u32 csis_pdp_top_get_frame_id_en(void __iomem *base_addr, struct is_device_csi *csi);

void csis_pdp_top_qch_cfg(void __iomem *base_reg, bool on);
void csis_pdp_top_s_link_vc_list(int *link_vc_list, u32 *mux_val_base,
				 u32 max_vc_num, u32 otf_out_id);
void csis_pdp_top_set_ibuf(void __iomem *base_reg, struct pablo_camif_otf_info *otf_info,
	u32 otf_out_id, struct is_sensor_cfg *sensor_cfg, bool csi_potf);
void csis_pdp_top_enable_ibuf_ptrn_gen(void __iomem *base_reg, struct is_sensor_cfg *sensor_cfg,
	struct pablo_camif_otf_info *otf_info, u32 clk, bool on);
void csis_pdp_top_irq_msk(void __iomem *base_reg, bool on);
ulong csis_pdp_top_irq_src(void __iomem *base_reg);
void csis_pdp_top_s_otf_out_mux(
	void __iomem *regs, u32 csi_ch, u32 otf_ch, u32 img_vc, u32 mux_val_base, bool en);
void csis_pdp_top_s_otf_lc(void __iomem *regs, u32 otf_ch, u32 *lc);
void csi_pdp_top_dump(void __iomem *base_reg);
#else
#define csis_pdp_top_frame_id_en(csi, fid_loc) do_nothing
#define csis_pdp_top_get_frame_id_en(base_addr, csi) 0
#define csis_pdp_top_qch_cfg(base_reg, on) do_nothing
#define csis_pdp_top_s_link_vc_list(link_vc_list, mux_val_base, max_vc_num, otf_out_id) do_nothing
#define csis_pdp_top_set_ibuf(base_reg, otf_info, otf_out_id, sensor_cfg, csi_potf) do_nothing
#define csis_pdp_top_enable_ibuf_ptrn_gen(base_reg, sensor_cfg, otf_info, clk, on) do_nothing
#define csis_pdp_top_irq_msk(base_reg, on) do_nothing
#define csis_pdp_top_irq_src(base_reg) 0
#define csis_pdp_top_s_otf_out_mux(regs, csi_ch, otf_ch, img_vc, mux_val_base, en) do_nothing
#define csis_pdp_top_s_otf_lc(regs, otf_ch, lc) do_nothing
#define csi_pdp_top_dump(base_reg) do_nothing
#endif

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_hw_csis_pdp_top_func {
	void (*frame_id_en)(struct is_device_csi *csi, struct is_fid_loc *fid_loc);
	u32 (*get_frame_id_en)(void __iomem *base_addr, struct is_device_csi *csi);
	void (*qch_cfg)(void __iomem *base_reg, bool on);
	void (*s_link_vc_list)(int *link_vc_list, u32 *mux_val_base, u32 max_vc_num,
			       u32 otf_out_id);
	void (*s_otf_out_mux)(void __iomem *regs,
		u32 csi_ch, u32 otf_ch, u32 img_vc, u32 mux_val_base, bool en);
	void (*s_otf_lc)(void __iomem *regs, u32 otf_ch, u32 *lc);
	void (*irq_msk)(void __iomem *base_reg, bool on);
	ulong (*irq_src)(void __iomem *regs);
};
struct pablo_kunit_hw_csis_pdp_top_func *pablo_kunit_get_hw_csis_pdp_top_test(void);
#endif
#endif
