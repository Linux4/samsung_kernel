/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SCP_MBRAIN_DBG__
#define __SCP_MBRAIN_DBG__

#include <linux/types.h>

struct scp_res_mbrain_dbg_ops {
	unsigned int (*get_length)(void);
	int (*get_data)(void *address, uint32_t size);
	// int (*get_last_suspend_res_data)(void *address, uint32_t size);
	// int (*get_over_threshold_num)(void *address, uint32_t size, uint32_t *threshold, uint32_t threshold_num);
	// int (*get_over_threshold_data)(void *address, uint32_t size);
};

struct scp_res_mbrain_dbg_ops *get_scp_mbrain_dbg_ops(void);
int register_scp_mbrain_dbg_ops(struct scp_res_mbrain_dbg_ops *ops);
void unregister_scp_mbrain_dbg_ops(void);

#endif
