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

void csis_pdp_top_frame_id_en(struct is_device_csi *csi, struct is_fid_loc *fid_loc);
u32 csis_pdp_top_get_frame_id_en(void __iomem *base_addr, struct is_device_csi *csi);

void csis_pdp_top_qch_cfg(u32 __iomem *base_reg, bool on);

#if IS_ENABLED(CONFIG_CSIS_PDP_TOP_V6_0)
void  csis_pdp_top_set_ibuf(u32 __iomem *base_reg, u32 otf_ch, u32 otf_out_id,
		u32* link_vc_list, struct is_sensor_cfg *sensor_cfg);
void csis_pdp_top_enable_ibuf_ptrn_gen(u32 __iomem *base_reg, u32 otf_ch, bool sensor_sync);
#else
#define csis_pdp_top_set_ibuf(base_reg, otf_ch, otf_out_id, link_vc_list, sensor_cfg)
#define csis_pdp_top_enable_ibuf_ptrn_gen(base_reg, otf_ch, sensor_sync)
#endif

#if IS_ENABLED(CONFIG_CSIS_PDP_TOP_API)
void csis_pdp_top_s_otf_out_mux(void __iomem *regs,
		u32 csi_ch, u32 otf_ch, u32 img_vc,
		u32* link_vc_list, bool en);
void csis_pdp_top_s_otf_lc(void __iomem *regs, u32 otf_ch, u32 *lc, u32 lc_num);
void csi_pdp_top_dump(u32 __iomem *base_reg);
#else
#define csis_pdp_top_s_otf_out_mux(regs, csi_ch, otf_ch, img_vc, link_vc_list, en) do {} while(0)
#define csis_pdp_top_s_otf_lc(regs, otf_ch, lc, lc_num) do {} while(0)
#define csi_pdp_top_dump(base_reg)
#endif

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_hw_csis_pdp_top_func {
	void (*frame_id_en)(struct is_device_csi *csi, struct is_fid_loc *fid_loc);
	u32 (*get_frame_id_en)(void __iomem *base_addr, struct is_device_csi *csi);
	void (*qch_cfg)(u32 __iomem *base_reg, bool on);
	void (*s_otf_out_mux)(void __iomem *regs, u32 csi_ch, u32 otf_ch, u32 img_vc,
			u32* link_vc_list, bool en);
	void (*s_otf_lc)(void __iomem *regs, u32 otf_ch, u32 *lc, u32 lc_num);
};
struct pablo_kunit_hw_csis_pdp_top_func *pablo_kunit_get_hw_csis_pdp_top_test(void);
#endif
#endif
