/* linux/include/soc/samsung/exynos-dm-esca-ipc.h
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS5 - Header file to support exynos ESCA DVFS Manager IPC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __EXYNOS_DM_ESCA_IPC_H
#define __EXYNOS_DM_ESCA_IPC_H

#define DM_DATA_INIT		(0)
#define DM_REG_CONST_START	(1)
#define DM_REG_CONST_FREQ	(2)
#define DM_REG_CONST_END	(3)
#define DM_POLICY_UPDATE	(4)
#define DM_FREQ_REQ	 	(5)
#define DM_FREQ_SYNC	 	(6)


#define DM_START_GET_DOMAIN_POLICY	(7)
#define DM_CONTINUE_GET_DOMAIN_POLICY	(8)
#define DM_GET_DOMAIN_CONST_POLICY	(9)

#define DM_GET_DM_CONST_INFO		(10)
#define DM_GET_DM_CONST_TABLE_INFO	(11)
#define DM_GET_DM_CONST_TABLE_ENTRY	(12)

#define DM_DYNAMIC_DISABLE		(13)
#define DM_CHANGE_FREQ_TABLE		(14)

/*
 * 16-byte DM IPC message format (REQ)
 *  (MSB)    3          2          1          0
 * ---------------------------------------------
 * |        fw_use       | dm_type  |  msg     |
 * ----------------------- 1----------------------
 * |          |          |          |          |
 * ---------------------------------------------
 * |          |          |          |          |
 * ---------------------------------------------
 * |          |          |          |          |
 * ---------------------------------------------
 */
struct dm_ipc_request {
	u8 msg;		/* LSB */
	u8 dm_type;
	u16 fw_use;	/* MSB */
	u32 req_rsvd0;
	u32 req_rsvd1;
	u32 req_rsvd2;
};

struct dm_ipc_response {
	u8 msg;		/* LSB */
	u8 dm_type;
	u16 fw_use;	/* MSB */
	u32 resp_rsvd0;
	u32 resp_rsvd1;
	u32 resp_rsvd2;
};

struct dm_ipc_freq_request {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u32 target_freq;
	u32 start_time;
	u32 req_rsvd2;
};

struct dm_ipc_policy_request {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u32 min_freq;
	u32 max_freq;
	u32 req_rsvd2;
};

struct dm_ipc_freq_sync_response {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u32 target_freq;
	u32 start_time;
	u32 end_time;
};

struct dm_ipc_const_request {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u8 slave_dm_type;
	u8 constraint_type : 1;
	u8 guidance : 1;
	u8 support_dynamic_disable : 1;
	u8 support_variable_freq_table : 1;
	u8 num_table_index : 4;
	u8 current_table_idx : 4;
	u8 table_idx : 4;
	u8 table_length;
	u32 master_freq;
	u32 slave_freq;
};

struct dm_ipc_init_request {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u32 min_freq;
	u32 max_freq;
	u32 cur_freq;
};

struct dm_ipc_get_policy_request {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u32 index;
	u32 req_rsvd1;
	u32 req_rsvd2;
};

struct dm_ipc_get_policy_response {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u16 values[6];
};

/* get overall constraint info for given domain */
struct dm_ipc_domain_constraint_info {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u32 num_constraints;
	u32 rsvd1;
	u32 rsvd2;
};
/* get each constraint info */
struct dm_ipc_const_table_info {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u8 slave_dm_type;
	u8 constraint_type : 1;
	u8 guidance : 1;
	u8 support_dynamic_disable : 1;
	u8 support_variable_freq_table : 1;
	u8 constraint_idx : 4;
	u8 num_table_index : 4;
	u8 current_table_idx : 4;
	u8 table_length;
	u32 const_freq;
	u32 gov_freq;
};

/* get constraint table */
struct dm_ipc_const_table_entry {
	u8 msg;
	u8 dm_type;
	u16 fw_use;
	u8 const_idx; // index of constraint
	u8 table_idx; // index of variable table
	u8 entry_idx; // index of table entry
	u8 rsvd;
	u32 master_freq;
	u32 slave_freq;
};

union dm_ipc_message {
	u32 data[4];
	struct dm_ipc_request req;
	struct dm_ipc_response resp;
	struct dm_ipc_policy_request policy_req;
	struct dm_ipc_freq_request freq_req;
	struct dm_ipc_const_request const_req;
	struct dm_ipc_init_request init_req;
	struct dm_ipc_freq_sync_response sync_resp;
	struct dm_ipc_get_policy_request get_policy_req;
	struct dm_ipc_get_policy_response get_policy_resp;
	struct dm_ipc_const_table_info const_table_info;
	struct dm_ipc_domain_constraint_info domain_const_info;
	struct dm_ipc_const_table_entry const_table_entry;
};

#endif
