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
#ifndef _TEE_MSGM_OP_INTERNAL_H_
#define _TEE_MSGM_OP_INTERNAL_H_
#include "tee_msgm_ntw.h"

extern const char cmd_magic[];
#define BYTES_OF_MAGIC (4)
#define INIT_CMD_MAGIC(_m) \
	do {                         \
		_m[0] = cmd_magic[0];    \
		_m[1] = cmd_magic[1];    \
		_m[2] = cmd_magic[2];    \
		_m[3] = cmd_magic[3];    \
	} while (0)
#define CLEANUP_CMD_MAGIC(_m) \
	do {                                    \
		_m[0] = _m[1] = _m[2] = _m[3] = 0;  \
	} while (0)
#define IS_CMD_MAGIC_VALID(_m) \
	((cmd_magic[0] == _m[0]) &&   \
	(cmd_magic[1] == _m[1]) &&   \
	(cmd_magic[2] == _m[2]) &&   \
	(cmd_magic[3] == _m[3]))
/* for translate cmd idx */
#define TEE_TW_SUB_CMD_SIZE       (8)
#define TEE_TW_PREM_CMD_MASK      ((1 << (32 - TEE_TW_SUB_CMD_SIZE)) - 1)
/* for parameters */
#define  PARAM_NUMBERS      (4)
#define IS_TYPE_NONE(_m)    (0x0 == (_m))
#define IS_TYPE_VALUE(_m)   ((_m) >= 0x1 && (_m) <= 0x3)
#define IS_TYPE_TMPREF(_m)  ((_m) >= 0x5 && (_m) <= 0x7)
#define IS_TYPE_MEMREF(_m)  ((_m) >= 0xc && (_m) <= 0xf)
/* common macro */
#define IS_PARAM_TAGED_INPUT(_m)   (((_m) & 0x1) == 0x1)
#define IS_PARAM_TAGED_OUTPUT(_m)  (((_m) & 0x2) == 0x2)
#define OFFSETOF(TYPE, MEMBER)     __builtin_offsetof(TYPE, MEMBER)

/* vvv cmd_body struct vvv */
typedef struct _map_shm_body {
	ulong_t cntx;
	void *vaddr_tw;
	uint32_t flags;
	tee_stat_t ret;

	uint32_t arr_sz;
	uint32_t rsvd;
	tee_msgm_phys_memblock_t arr[];
} map_shm_body;
typedef struct _unmap_shm_body {
	void *vaddr_tw;
	size_t sz;
} unmap_shm_body;
typedef struct _open_ss_body {
	ulong_t cntx;
	TEEC_UUID uuid;
	uint32_t meth;
	uint32_t data_sz;
	TEEC_UUID data;
	void *ss_tw;
	uint32_t ret_orig;
	tee_stat_t ret;
} open_ss_body;
typedef struct _close_ss_body {
	void *ss_tw;
} close_ss_body;
typedef struct _inv_op_body {
	uint32_t srv_cmd;
	void *ss_tw;

	uint32_t ret_orig;
	tee_stat_t ret;
} inv_op_body;
typedef struct _can_op_body {
	void *ss_tw;
	void *tee_msg_ntw;
} can_op_body;
/* ^^^^^^ cmd_body struct ^^^^^^^ */

/* vvv size of cmd_body vvv */
#define MAP_SHM_SZ     (sizeof(map_shm_body))
#define UNMAP_SHM_SZ   (sizeof(unmap_shm_body))
#define OPEN_SS_SZ     (sizeof(open_ss_body))
#define CLOSE_SS_SZ    (sizeof(close_ss_body))
#define INV_OP_SZ      (sizeof(inv_op_body))
#define CAN_OP_SZ      (sizeof(can_op_body))
/* ^^^ size of cmd_body ^^^ */

/* vvv cmd operation class vvv */
typedef struct _tee_msgm_op_class {
	tee_stat_t(*set_cmd) (tee_msgm_t msgm, void *arg);
	tee_stat_t(*get_cmd) (tee_msgm_t msgm, void *arg);
	tee_stat_t(*set_ret) (tee_msgm_t msgm, void *arg);
	tee_stat_t(*get_ret) (tee_msgm_t msgm, void *arg);
	bool(*cmd_class) (tee_cmd_t cmd);
} tee_msgm_op_class;
extern const tee_msgm_op_class map_shm_class;
extern const tee_msgm_op_class unmap_shm_class;
extern const tee_msgm_op_class open_ss_class;
extern const tee_msgm_op_class close_ss_class;
extern const tee_msgm_op_class inv_op_class;
extern const tee_msgm_op_class can_op_class;
/* ^^^^^ cmd operation class ^^^^ */

/* vvv  params handling vvv */
tee_stat_t get_params(tee_msgm_t msgm, tee_msg_op_info_t *arg);
tee_stat_t set_params(tee_msgm_t msgm, tee_msg_op_info_t *arg);
tee_stat_t update_params(tee_msgm_t msgm, tee_msg_op_info_t *arg);
/* ^^^^^^^^^^ params handling ^^^^^^ */

/* vvv  msg params format vvv */
typedef struct _cmd_struct {
	uint8_t magic[4];
	tee_cmd_t cmd;
	uint32_t cmd_sz;	/* size of cmd */
	uint32_t rsvd;		/* keep 64b-aligned */
	uint8_t cmd_body[0];
} cmd_struct;
typedef union {
	struct {
		void *buffer;
		size_t size;
	} tmpref;
	struct {
		void *buffer;
		size_t size;
	} memref;
	struct {
		uint32_t a, b;
	} value;
} Union_Parameter;
typedef struct _params_struct {
	uint32_t paramTypes;
	Union_Parameter param[4];
} params_struct;
/* ^^^  msg params format ^^^ */

/* vvv  internal handler vvv */
typedef enum _tee_ntw_cmd_stat_t {
	TEE_NTW_CMD_STAT_CLEAR,
	TEE_NTW_CMD_STAT_SET,
} tee_ntw_cmd_stat_t;
typedef struct _tee_ntw_cmd_record_t {
	tee_cmd_t cmd;
	uint32_t cmd_sz;
	uint32_t param_size;
	uint32_t msg_sz;	/* the summary size of msg */
	tee_ntw_cmd_stat_t stat;
} tee_ntw_cmd_record_t;
typedef struct _tee_msgm_ntw_struct {
	uint8_t magic[BYTES_OF_MAGIC];

	cmd_struct *ntw_cmd;
	uint8_t *body;
	params_struct *ntw_params;
	const tee_msgm_op_class *op;
	tee_ntw_cmd_record_t cmd_record;
} tee_msgm_ntw_struct;

#endif
