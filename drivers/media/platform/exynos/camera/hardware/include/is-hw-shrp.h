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

#ifndef IS_HW_SHRP_H
#define IS_HW_SHRP_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"

#define dbg_shrp(level, fmt, args...) \
	dbg_common((is_get_debug_param(IS_DEBUG_PARAM_SHRP)) >= (level), \
		"[SHRP]", fmt, ##args)

enum is_hw_shrp_irq_src {
	SHRP0_INTR,
	SHRP1_INTR,
	SHRP_INTR_MAX,
};

enum is_hw_shrp_event_id {
     SHRP_EVENT_FRAME_START = 1,
     SHRP_EVENT_FRAME_END
};

struct is_hw_shrp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct shrp_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		*rdma;
	struct is_common_dma		*wdma;
	struct is_shrp_config		config;
	u32				irq_state[SHRP_INTR_MAX];
	u32				instance;
	unsigned long 			state;
	atomic_t			strip_index;

	struct is_priv_buf		*pb_c_loader_payload;
	unsigned long			kva_c_loader_payload;
	dma_addr_t			dva_c_loader_payload;
	struct is_priv_buf		*pb_c_loader_header;
	unsigned long			kva_c_loader_header;
	dma_addr_t			dva_c_loader_header;
};

int is_hw_shrp_test(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
#endif
