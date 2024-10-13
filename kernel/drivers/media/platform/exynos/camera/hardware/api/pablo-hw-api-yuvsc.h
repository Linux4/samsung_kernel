/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * YUVSC HW control APIs
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_API_YUVSC_H
#define PABLO_HW_API_YUVSC_H

#include "is-hw-api-type.h"
#include "pablo-hw-api-common-ctrl.h"

enum yuvsc_int_type {
	INT_FRAME_START,
	INT_FRAME_END,
	INT_COREX_END,
	INT_SETTING_DONE,
	INT_ERR0,
	INT_ERR1,
	INT_TYPE_NUM,
};

struct yuvsc_hw_ops {
	void (*reset)(struct pablo_mmio *pmio);
	void (*init)(struct pablo_mmio *pmio);
	void (*s_core)(struct pablo_mmio *pmio, struct yuvsc_param_set *p_set);
	void (*s_path)(struct pablo_mmio *pmio, struct yuvsc_param_set *p_set,
		       struct pablo_common_ctrl_frame_cfg *frame_cfg);
	void (*g_int_en)(u32 *int_en);
	u32 (*g_int_grp_en)(void);
	int (*wait_idle)(struct pablo_mmio *pmio);
	void (*dump)(struct pablo_mmio *pmio, u32 mode);
	bool (*is_occurred)(u32 status, ulong type);
	void (*s_strgen)(struct pablo_mmio *pmio);
};

const struct yuvsc_hw_ops *yuvsc_hw_g_ops(void);
void yuvsc_hw_g_pmio_cfg(struct pmio_config *pcfg);
#endif /* PABLO_HW_API_YUVSC_H */
