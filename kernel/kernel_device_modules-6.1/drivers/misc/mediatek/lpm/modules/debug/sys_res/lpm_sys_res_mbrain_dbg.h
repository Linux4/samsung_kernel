/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_LPM_SYS_RES_MBRAIN_DBG__
#define __MTK_LPM_SYS_RES_MBRAIN_DBG__

#include <linux/types.h>

enum _release_scene {
	SYS_RES_RELEASE_SCENE_COMMON = 0,
	SYS_RES_RELEASE_SCENE_SUSPEND,
	SYS_RES_RELEASE_SCENE_LAST_SUSPEND,
	SCENE_RELEASE_NUM,
};

enum _release_bool {
	NOT_RELEASE = 0,
	RELEASE_GROUP,
};

struct sys_res_mbrain_header {
	uint8_t module;
	uint8_t version;
	uint16_t data_offset;
	uint32_t index_data_length;
};

struct sys_res_scene_info {
	uint64_t duration_time;
	uint64_t suspend_time;
	uint32_t res_sig_num;
};

struct sys_res_sig_info {
	uint64_t active_time;
	uint32_t sig_id;
	uint32_t grp_id;
};

struct lpm_sys_res_mbrain_dbg_ops {
	unsigned int (*get_length)(void);
	int (*get_data)(void *address, uint32_t size);
	int (*get_last_suspend_res_data)(void *address, uint32_t size);
	int (*get_over_threshold_num)(void *address, uint32_t size, uint32_t *threshold, uint32_t threshold_num);
	int (*get_over_threshold_data)(void *address, uint32_t size);
};

struct lpm_sys_res_mbrain_dbg_ops *get_lpm_mbrain_dbg_ops(void);
int register_lpm_mbrain_dbg_ops(struct lpm_sys_res_mbrain_dbg_ops *ops);
void unregister_lpm_mbrain_dbg_ops(void);

#endif
