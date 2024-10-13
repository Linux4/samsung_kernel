/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_AFM_DEBUG_H_
#define _NPU_AFM_DEBUG_H_

struct npu_afm_debug_irp {
	u32 m; // Monitor Unit : (m + 1) (1 ~ 64(I_CLK cycles)) (m = 0 ~ 63)
	u32 x; // Monitor Interval : (X + 1) * Monitor Unit (x = 0 ~ 15)
	u32 r; // IRP : (R + 2) * Monitor Interval (R =  0 ~ 63)
	u32 v; // Integration Interval : (V + 2) * IRP (V = 0 ~ 6)
	u32 trw; // TRW : (TRW + 1) * Integration Interval (TRW = 0 ~ 31)
};

void npu_afm_debug_set_irp(struct npu_system *system);
void npu_afm_debug_set_tdt(struct npu_system *system);
int npu_afm_register_debug_sysfs(struct npu_system *system);
#endif /* _NPU_AFM_DEBUG_H */
