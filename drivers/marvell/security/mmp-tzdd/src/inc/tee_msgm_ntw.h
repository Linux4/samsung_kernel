/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */
#ifndef _TEE_MSGM_NTW_H_
#define _TEE_MSGM_NTW_H_

#include "tee_mrvl_imp.h"
#include "tee_client_api.h"

typedef TEEC_Result tee_stat_t;
typedef void *tee_msgm_t;

typedef enum _tee_msg_business_stat_t {
	TEE_MSG_STAT_INVALID,
	TEE_MSG_STAT_REQ,
	TEE_MSG_STAT_RSP,
	TEE_MSG_STAT_CAN,
} tee_msg_business_stat_t;

typedef enum _tee_msg_control_cmd_t {
	TEE_MSG_CTL_INVALID,
	TEE_MSG_CTL_RETURN_TO_NTW,
	TEE_MSG_CTL_TRAPPED,
} tee_msg_control_cmd_t;

typedef enum _tee_msg_type_t {
	TEE_MSG_TYPE_BUSINESS,
	TEE_MSG_TYPE_CONTROL,
} tee_msg_type_t;

/*
 * this struct is like this
 * ,----------------------------------------------------------,
 * | type   | tw | tp |      reserve      |       stat        | bp
 * | 31~28  | 27 | 26 |      25~16        |       15~0        |
 * |        |-----------------------------+-------------------|
 * |        |            reserve          |       cmd         | ctl
 * |        |             27~16           |       15~0        |
 * |----------------------------------------------------------|
 * |                            value                         |
 * |                            32~0                          |
 * '----------------------------------------------------------'
 */
typedef union _tee_msg_prop_t {
	struct {
		tee_msg_business_stat_t stat:16;
		uint32_t:10;
		bool has_trapped:1;
		bool is_from_tw:1;
	} bp;
	struct {
		tee_msg_control_cmd_t cmd:16;
		uint32_t:12;
	} ctl;
	struct {
		uint32_t:28;
		tee_msg_type_t type:4;
	};
	uint32_t value;
} tee_msg_prop_t;

/* open msg-head for updating msg-ntw according to msg-in-rb by pt-ntw */
typedef struct _tee_msg_head_t {
	uint8_t magic[4];
	uint32_t msg_sz;
	osa_list_t node;
	tee_msg_prop_t msg_prop;
	tee_msg_handle_t tee_msg;
	osa_sem_t comp;
	void *reserved;
} tee_msg_head_t;

typedef TEEC_SharedMemory tee_msg_map_shm_info_t;
typedef TEEC_Operation tee_msg_op_info_t;

typedef enum _tee_cmd_t {
	TEE_CMD_INVALID,
	TEE_CMD_MAP_SHM,
	TEE_CMD_UNMAP_SHM,
	TEE_CMD_OPEN_SS,
	TEE_CMD_CLOSE_SS,
	TEE_CMD_INV_OP,
	TEE_CMD_CAN_OP,
} tee_cmd_t;

typedef struct _tee_msgm_msg_info_t {
	tee_cmd_t cmd;
	/* valid when map_shm */
	tee_msg_map_shm_info_t *msg_map_shm_info;
	/* valid when op available */
	tee_msg_op_info_t *msg_op_info;
} tee_msgm_msg_info_t;

/* vvv set_cmd_arg vvv */
typedef TEEC_SharedMemory tee_set_cmd_map_shm_arg_t;
typedef TEEC_SharedMemory tee_set_cmd_unmap_shm_arg_t;
typedef struct _tee_set_cmd_open_ss_arg_t {
	TEEC_UUID *uuid;
	uint32_t meth;
	TEEC_UUID data;
} tee_set_cmd_open_ss_arg_t;
typedef void *tee_set_cmd_close_ss_arg_t;
typedef struct _tee_set_cmd_inv_op_arg_t {
	void *ss;
	uint32_t srv_cmd;
} tee_set_cmd_inv_op_arg_t;
typedef TEEC_Operation tee_set_cmd_can_op_arg_t;
/* ^^^ set_cmd_arg ^^^ */

/* vvv get_cmd_arg vvv */
typedef struct _tee_msgm_phys_memblock_t {
	void *phys;
	size_t size;
} tee_msgm_phys_memblock_t;
typedef struct _tee_get_cmd_map_shm_arg_t {
	tee_msgm_phys_memblock_t(*arr)[];
	uint32_t arr_sz;
} tee_get_cmd_map_shm_arg_t;
typedef struct _tee_get_cmd_unmap_shm_arg_t {
	void *vaddr_tw;
} tee_get_cmd_unmap_shm_arg_t;
typedef struct _tee_get_cmd_open_ss_arg_t {
	TEEC_UUID *uuid;
	uint32_t meth;
	TEEC_UUID data;
	uint32_t data_sz;
} tee_get_cmd_open_ss_arg_t;
typedef struct _tee_get_cmd_close_ss_arg_t {
	void *ss;
} tee_get_cmd_close_ss_arg_t;
typedef struct _tee_get_cmd_inv_op_arg_t {
	void *ss;
	uint32_t srv_cmd;
} tee_get_cmd_inv_op_arg_t;
typedef struct _tee_get_cmd_can_op_arg_t {
	void *ss;		/* NULL for tee-ss */
	void *msg;
} tee_get_cmd_can_op_arg_t;
/* ^^^ get_cmd_arg ^^^ */

/* vvv get_ret_arg vvv */
typedef struct _tee_get_ret_map_shm_arg_t {
	void *vaddr_tw;
	tee_stat_t ret;
} tee_get_ret_map_shm_arg_t;
typedef struct _tee_get_ret_open_ss_arg_t {
	void *ss_tw;
	uint32_t ret_orig;
	tee_stat_t ret;
} tee_get_ret_open_ss_arg_t;
typedef struct _tee_get_ret_inv_op_arg_t {
	uint32_t ret_orig;
	tee_stat_t ret;
} tee_get_ret_inv_op_arg_t;
/* ^^^ get_ret_arg ^^^ */

/* vvv set_ret_arg vvv */
typedef struct _tee_set_ret_map_shm_arg_t {
	void *vaddr_tw;
	tee_stat_t ret;
} tee_set_ret_map_shm_arg_t;
typedef struct _tee_set_ret_open_ss_arg_t {
	void *ss_tw;
	uint32_t ret_orig;
	tee_stat_t ret;
} tee_set_ret_open_ss_arg_t;
typedef struct _tee_set_ret_inv_op_arg_t {
	uint32_t ret_orig;
	tee_stat_t ret;
} tee_set_ret_inv_op_arg_t;
/* ^^^ set_ret_arg ^^^ */

tee_msgm_t tee_msgm_create_inst(void);
/* will not destroy the msg buf */
void tee_msgm_destroy_inst(tee_msgm_t msgm);

/* "buf = NULL" to get the buf size first */
/*
 * the buffer does NOT include the command header.
 * the header is maintained by callers.
 * the typical behavior of callers is to alloc
 * sizeof(tee_msg_head_t) + buf-size.
 * then set "mem + sizeof(tee_msg_head_t)" as the arg, buf.
 */
tee_stat_t tee_msgm_set_msg_buf(tee_msgm_t msgm,
				tee_msgm_msg_info_t *mi, uint8_t *buf,
				uint32_t *size);

tee_stat_t tee_msgm_get_cmd(tee_msgm_t msgm, tee_cmd_t *cmd, void *arg);
tee_stat_t tee_msgm_set_cmd(tee_msgm_t msgm, tee_cmd_t cmd, void *arg);

tee_stat_t tee_msgm_set_params(tee_msgm_t msgm, tee_msg_op_info_t *params);
tee_stat_t tee_msgm_update_params(tee_msgm_t msgm, tee_msg_op_info_t *params);

tee_stat_t tee_msgm_get_ret(tee_msgm_t msgm, void *arg);
tee_stat_t tee_msgm_set_ret(tee_msgm_t msgm, void *arg);
#endif /* _TEE_MSGM_NTW_H_ */
