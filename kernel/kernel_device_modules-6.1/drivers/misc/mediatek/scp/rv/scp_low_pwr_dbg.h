/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SCP_LOW_PWR_DBG__
#define __SCP_LOW_PWR_DBG__

#include <linux/types.h>

#define SCP_RES_DATA_MODULE_ID      5
#define SCP_RES_DATA_VERSION        0
#define configMAX_LOCK_NAME_LEN     7
#define HEADER_SIZE					8

struct scp_res_mbrain_header {
	uint8_t mbrain_module;
	uint8_t version;
	uint16_t data_offset;
	uint32_t index_data_length;
};

struct scp_res_info {
	uint64_t user;          /* record the 1st user */
	uint64_t duration;
	uint64_t total_duration;
	uint64_t return_address;
};

struct sys_wlock_info {
	char pcLockName[configMAX_LOCK_NAME_LEN];
	uint64_t duration;
	uint64_t total_duration;
	uint64_t return_address;
};

int scp_sys_res_mbrain_plat_init (void);

#endif
