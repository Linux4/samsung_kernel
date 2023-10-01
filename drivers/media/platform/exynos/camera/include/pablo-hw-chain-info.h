// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_CHAIN_INFO_H
#define PABLO_HW_CHAIN_INFO_H

#include "is-hw.h"

#define PLANE_INDEX_CR_SET 0
#define PLANE_INDEX_CONFIG 1

#define valid_hw_slot_id(slot_id) (slot_id >= 0)

#define CALL_HW_CHAIN_INFO_OPS(hw, op, args...)	\
	(((hw)->chain_info_ops && (hw)->chain_info_ops->op) ? ((hw)->chain_info_ops->op(hw, ##args)) : 0)

enum hw_get_ctrl_id {
	HW_G_CTRL_HAS_VRA_CH1_ONLY,
};

struct pablo_hw_chain_info_ops {
	bool (*check_valid_lib)(struct is_hardware *hw, u32 hw_id);
	struct size_cr_set *(*get_cr_set)(struct is_hardware *hw, u32 hw_id,
					struct is_frame *frame);
	void *(*get_config)(struct is_hardware *hw, u32 hw_id, struct is_frame *frame);
	int (*get_hw_slot_id)(struct is_hardware *hw, u32 hw_id);
	struct is_hw_ip *(*get_hw_ip)(struct is_hardware *hw, u32 hw_id);
	void (*query_cap)(struct is_hardware *hw, u32 hw_id, void *cap_data);
	void (*get_ctrl)(struct is_hardware *hw, int hw_id, enum hw_get_ctrl_id id, void *val);
	int (*set_votf_hwfc)(struct is_hardware *hw, u32 output_id);
	int (*set_votf_reset)(struct is_hardware *hw);
	struct is_mem *(*get_iommu_mem)(struct is_hardware *hw, u32 gid);
	bool (*check_crta_group)(struct is_hardware *hw, int group_id);
	int (*get_pdaf_buf_num)(struct is_hardware *hw, int max_target_fps);
	bool (*skip_setfile_load)(struct is_hardware *hw, int hw_id);
};

int pablo_hw_chain_info_probe(struct is_hardware *hw);


/* common */
void pablo_hw_cmn_get_ctrl(struct is_hardware *hw, int hw_id, enum hw_get_ctrl_id id, void *val);

#endif
