/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_COMMON_CTRL_H
#define IS_HW_API_COMMON_CTRL_H

#include "pablo-internal-subdev-ctrl.h"

#define PCC_NAME_LEN 10

#define CALL_PCC_OPS(pcc, op, args...) (((pcc)->ops && (pcc)->ops->op) ? ((pcc)->ops->op(args)) : 0)

enum pablo_common_ctrl_mode { PCC_OTF, PCC_M2M, PCC_MODE_NUM };

enum pablo_common_ctrl_fs_mode {
	PCC_ASAP = 0,
	PCC_VVALID_RISE = 1,
	PCC_FS_MODE_NUM,
};

enum pablo_common_ctrl_setting_mode {
	PCC_DMA_PRELOADING = 0,
	PCC_DMA_DIRECT = 1,
	PCC_COREX = 2,
	PCC_APB_DIRECT = 3,
	PCC_SETTING_MODE_NUM,
};

enum pablo_common_ctrl_int_grp_id {
	PCC_INT_GRP_FRAME_START = 0,
	PCC_INT_GRP_FRAME_END = 1,
	PCC_INT_GRP_ERR_CRPT = 2,
	PCC_INT_GRP_CMDQ_HOLD = 3,
	PCC_INT_GRP_SETTING_DONE = 4,
	PCC_INT_GRP_DEBUG = 5,
	/* reserved = 6 */
	PCC_INT_GRP_ENABLE_ALL = 7,
	PCC_INT_GRP_MAX,
};

enum pablo_common_ctrl_int_id {
	PCC_INT_0,
	PCC_INT_1,
	PCC_CMDQ_INT,
	PCC_COREX_INT,
	PCC_INT_ID_NUM,
};

struct pablo_common_ctrl_cfg {
	enum pablo_common_ctrl_fs_mode fs_mode;
	u32 int_en[PCC_INT_ID_NUM];
};

struct pablo_common_ctrl_cmd {
	dma_addr_t base_addr;
	u32 header_num;
	u32 set_mode;
	u32 cmd_id;
	u32 fcount;
	u32 int_grp_en;
	u32 fro_id;
};

struct pablo_common_ctrl_frame_cfg {
	u32 cotf_in_en;
	u32 cotf_out_en;
	u32 delay;
	struct pablo_common_ctrl_cmd cmd;
};

/**
 * struct pablo_common_ctrl_dbg_info - Debugging information of the specific PCC object.
 *
 * @last_cmd: The last command that CMDQ has popped.
 *            Updated on SETTING_DONE ISR.
 * @last_rptr: The position of last_cmd.
 *             Updated on SETTING_DONE ISR.
 * @last_fcount: The last fcount of command that passed the FRAME state.
 *               Updated on FRAME_START ISR.
 */
struct pablo_common_ctrl_dbg_info {
	struct pablo_common_ctrl_cmd last_cmd;
	u32 last_rptr;
	u32 last_fcount;
};

struct pablo_common_ctrl {
	char name[PCC_NAME_LEN];
	enum pablo_common_ctrl_mode mode;

	struct pablo_mmio *pmio;
	struct pmio_field *fields;
	struct pablo_internal_subdev subdev_cloader;

	spinlock_t cmdq_lock;
	unsigned long irq_flags;

	const struct pablo_common_ctrl_ops *ops;

	struct pablo_common_ctrl_dbg_info dbg;
};

struct pablo_common_ctrl_ops {
	int (*enable)(struct pablo_common_ctrl *pcc, const struct pablo_common_ctrl_cfg *cfg);
	void (*disable)(struct pablo_common_ctrl *pcc);
	int (*reset)(struct pablo_common_ctrl *pcc);
	int (*shot)(struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg);
	u32 (*get_int_status)(struct pablo_common_ctrl *pcc, u32 int_id, bool clear);
	int (*cmp_fcount)(struct pablo_common_ctrl *pcc, const u32 fcount);
	void (*dump)(struct pablo_common_ctrl *pcc);
};

#if IS_ENABLED(CONFIG_PABLO_CMN_CTRL_API)
int pablo_common_ctrl_init(struct pablo_common_ctrl *pcc, struct pablo_mmio *pmio, const char *name,
	enum pablo_common_ctrl_mode mode, struct is_mem *mem);
void pablo_common_ctrl_deinit(struct pablo_common_ctrl *pcc);
#else
#define pablo_common_ctrl_init(pcc, pmio, name, mode, mem) 0
#define pablo_common_ctrl_deinit(pcc)                                                              \
	do {                                                                                       \
	} while (0)
#endif

#endif /* IS_HW_API_COMMON_CTRL_H */
