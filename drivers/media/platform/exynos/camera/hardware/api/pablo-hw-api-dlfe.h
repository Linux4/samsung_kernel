/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * DLFE HW control APIs
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_API_DLFE_H
#define PABLO_HW_API_DLFE_H

struct pablo_mmio;
struct pmio_config;
struct dlfe_param_set;

enum dlfe_hw_dump_mode {
	DLFE_HW_DUMP_CR,
	DLFE_HW_DUMP_DBG_STATE,
	DLFE_HW_DUMP_MODE_NUM,
};

enum dlfe_int_type {
	INT_FRAME_START,
	INT_FRAME_END,
	INT_COREX_END,
	INT_SETTING_DONE,
	INT_ERR0,
	INT_ERR1,
	INT_TYPE_NUM,
};

struct dlfe_hw_ops {
	int (*reset)(struct pablo_mmio *pmio);
	void (*init)(struct pablo_mmio *pmio);
	void (*s_core)(struct pablo_mmio *pmio, struct dlfe_param_set *p_set);
	void (*s_path)(struct pablo_mmio *pmio, struct dlfe_param_set *p_set);
	void (*q_cmdq)(struct pablo_mmio *pmio);
	u32 (*g_int_state)(struct pablo_mmio *pmio, u32 int_id);
	int (*wait_idle)(struct pablo_mmio *pmio);
	void (*dump)(struct pablo_mmio *pmio, u32 mode);
	bool (*is_occurred)(u32 status, ulong type);
};

const struct dlfe_hw_ops *dlfe_hw_g_ops(void);
void dlfe_hw_g_pmio_cfg(struct pmio_config *pcfg);
#endif /* PABLO_HW_API_DLFE_H */
