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

#if IS_ENABLED(CONFIG_CSIS_PDP_TOP_V5_0) || IS_ENABLED(CONFIG_CSIS_PDP_TOP_V6_0)
void csi_pdp_top_dump(u32 __iomem *base_reg);
#else
#define csi_pdp_top_dump(base_reg)
#endif

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_hw_csis_pdp_top_func {
	void (*frame_id_en)(struct is_device_csi *csi, struct is_fid_loc *fid_loc);
	u32 (*get_frame_id_en)(void __iomem *base_addr, struct is_device_csi *csi);
	void (*qch_cfg)(u32 __iomem *base_reg, bool on);
};
struct pablo_kunit_hw_csis_pdp_top_func *pablo_kunit_get_hw_csis_pdp_top_test(void);
#endif
#endif
