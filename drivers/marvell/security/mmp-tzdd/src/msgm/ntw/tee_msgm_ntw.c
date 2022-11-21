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
#include "tee_msgm_ntw.h"
#include "tee_msgm_op_internal.h"
#include "tee_memm.h"
#include "tzdd_internal.h"

extern const char cmd_magic[];
static uint8_t magic[] = { 'm', 'N', 'T', 'W', };

#define INIT_MAGIC(_m)	\
	do {                                \
		((uint8_t *)(_m))[0] = magic[0]; \
		((uint8_t *)(_m))[1] = magic[1]; \
		((uint8_t *)(_m))[2] = magic[2]; \
		((uint8_t *)(_m))[3] = magic[3]; \
	} while (0)
#define IS_MAGIC_VALID(_m) \
	((magic[0] == ((uint8_t *)(_m))[0]) && \
	(magic[1] == ((uint8_t *)(_m))[1]) &&  \
	(magic[2] == ((uint8_t *)(_m))[2]) &&  \
	(magic[3] == ((uint8_t *)(_m))[3]))
#define CLEANUP_MAGIC(_m) \
	do {                                \
		((uint8_t *)(_m))[0] = '\0';    \
		((uint8_t *)(_m))[1] = '\0';    \
		((uint8_t *)(_m))[2] = '\0';    \
		((uint8_t *)(_m))[3] = '\0';    \
	} while (0)
#define IS_PARAMTYPES_TYPE_VALID(_m) \
	(IS_TYPE_NONE(_m) || \
	 IS_TYPE_VALUE(_m) || \
	IS_TYPE_TMPREF(_m) || \
	IS_TYPE_MEMREF(_m))
#define IS_NTW_MEM_FLAG_VALID(_m) ((_m) > 0 && (_m) < 4)
static uint32_t _g_body_size_array[] = {
	0, MAP_SHM_SZ, UNMAP_SHM_SZ, OPEN_SS_SZ,
	CLOSE_SS_SZ, INV_OP_SZ, CAN_OP_SZ,
};

const tee_msgm_op_class *_g_op_array[] = {
	NULL, &map_shm_class, &unmap_shm_class,
	&open_ss_class, &close_ss_class,
	&inv_op_class, &can_op_class,
};

/* ********************
 * create instance
 * ********************/
tee_msgm_t tee_msgm_create_inst(void)
{
	tee_msgm_ntw_struct *handle = NULL;

	handle =
	    (tee_msgm_ntw_struct *) osa_kmem_alloc(sizeof(tee_msgm_ntw_struct));
	if (NULL == handle) {
		TZDD_DBG("In tee_msgm_create_inst, alloc err\n");
		goto err;
	}
	osa_memset(handle, 0, sizeof(tee_msgm_ntw_struct));
	INIT_MAGIC(handle->magic);
	handle->ntw_cmd = NULL;
	handle->body = NULL;
	handle->op = NULL;
	handle->cmd_record.cmd = TEE_CMD_INVALID;
	handle->cmd_record.cmd_sz = 0;
	handle->cmd_record.param_size = 0;
	handle->cmd_record.stat = TEE_NTW_CMD_STAT_CLEAR;
	return (tee_msgm_t) handle;
err:
	TZDD_DBG("In tee_msgm_create_inst err\n");
	return NULL;
}

/* ******************************
 * will not destroy the msg buf
 * *******************************/
void tee_msgm_destroy_inst(tee_msgm_t msgm)
{
	tee_msgm_ntw_struct *ntw_handle = NULL;

	OSA_ASSERT(msgm);
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	OSA_ASSERT(IS_MAGIC_VALID(ntw_handle->magic));
	CLEANUP_MAGIC(ntw_handle->magic);
	osa_kmem_free(ntw_handle);
	ntw_handle = NULL;
	return;
}

/* ************************************
 * calculate the sizo of tmp_memref
 * ************************************/
static inline tee_stat_t _calc_map_shm_len(TEEC_SharedMemory *obj,
					   uint32_t *num)
{
	tee_memm_ss_t memm_handle = NULL;
	tee_stat_t ntw_stat = TEEC_SUCCESS;

	memm_handle = tee_memm_create_ss();
	OSA_ASSERT(memm_handle);
	ntw_stat = tee_memm_calc_pages_num(memm_handle, obj->buffer,
					   obj->size, num);
	if (TEEC_SUCCESS != ntw_stat)
		return ntw_stat;

	tee_memm_destroy_ss(memm_handle);
	memm_handle = NULL;
	return TEEC_SUCCESS;
}

/* *************************************************************************
 * "buf = NULL" to get the buf size first */
/*
 * the buffer does NOT include the command header.
 * the header is maintained by callers.
 * the typical behavior of callers is to alloc
 * sizeof(tee_msg_head_t) + buf-size.
 * then set "mem + sizeof(tee_msg_head_t)" as the arg, buf.
 *
 * send Msg: tee_msgm_set_msg_buf(msgm, mi, NULL, &size); STAT_SET
 *           tee_msgm_set_msg_buf(msgm, NULL, buf, NULL); STAT_CLEAR
 * receive Msg: tee_msgm_set_msg_buf(msgm, NULL, buf, NULL);
 * **************************************************************************
 */
tee_stat_t tee_msgm_set_msg_buf(tee_msgm_t msgm, tee_msgm_msg_info_t *mi,
				uint8_t *buf, uint32_t *size)
{
	tee_msgm_ntw_struct *ntw_handle = NULL;
	uint32_t buf_size = 0, pages = 0, types = 0, idx = 0, value = 0;
	tee_stat_t shm_stat = TEEC_SUCCESS;
	TEEC_Operation *ntw_op = NULL;

	/* either mi && size or buf non-NULL; */
	OSA_ASSERT(((mi && size) || buf));
	OSA_ASSERT(msgm);
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	OSA_ASSERT(IS_MAGIC_VALID(ntw_handle->magic));

	if (NULL != buf) {
		/* set handle-item according to buf-ptr */
		ntw_handle->ntw_cmd = (cmd_struct *) buf;
		ntw_handle->body = buf + sizeof(cmd_struct);
		if (TEE_NTW_CMD_STAT_SET == ntw_handle->cmd_record.stat) {
			/* send msg: empty buffer for set cmd and params */
			osa_memset(buf, 0, ntw_handle->cmd_record.msg_sz);
			INIT_CMD_MAGIC(ntw_handle->ntw_cmd->magic);
			ntw_handle->ntw_params =
			    (params_struct *) (ntw_handle->body +
					       ntw_handle->cmd_record.cmd_sz);
			ntw_handle->ntw_cmd->cmd_sz =
			    ntw_handle->cmd_record.cmd_sz;
			ntw_handle->op =
			    _g_op_array[ntw_handle->cmd_record.cmd];
			ntw_handle->cmd_record.stat = TEE_NTW_CMD_STAT_CLEAR;
		} else {
			/* receive msg: msg for get cmd or params */
			OSA_ASSERT(IS_CMD_MAGIC_VALID
				   (ntw_handle->ntw_cmd->magic));
			ntw_handle->ntw_params =
			    (params_struct *) (ntw_handle->body +
					       ntw_handle->ntw_cmd->cmd_sz);
			ntw_handle->op = _g_op_array[ntw_handle->ntw_cmd->cmd];
		}
	} else if (mi && size) {
		/* send msg: calculate the size of msg-buf
		 * according info provided by mi
		*/
		OSA_ASSERT(TEE_NTW_CMD_STAT_CLEAR ==
			   ntw_handle->cmd_record.stat);
		/* handle tmp_mem_ref */
		if (mi->msg_op_info) {
			ntw_op = mi->msg_op_info;
			types = ntw_op->paramTypes;
			for (idx = 0; idx < 4; idx++) {
				value = TEEC_PARAM_TYPE_GET(types, idx);
				if (IS_TYPE_TMPREF(value)) {
					buf_size +=
					    ntw_op->params[idx].tmpref.size;
				}
			}
		}
		/* calculate the size of cmd_body */
		if (TEE_CMD_INVALID <= mi->cmd && TEE_CMD_CAN_OP >= mi->cmd) {
			if (TEE_CMD_MAP_SHM == mi->cmd
			    && NULL != mi->msg_map_shm_info) {
				shm_stat =
				    _calc_map_shm_len(mi->msg_map_shm_info,
						      &pages);
				if (TEEC_SUCCESS != shm_stat)
					return shm_stat;

				ntw_handle->cmd_record.cmd_sz =
				    _g_body_size_array[mi->cmd] +
				    pages * sizeof(tee_mem_page_t);
			} else
				ntw_handle->cmd_record.cmd_sz =
				    _g_body_size_array[mi->cmd];
		} else {
			TZDD_DBG("ERROR: cmd not exist\n");
			OSA_ASSERT(0);
		}
		/* saved rec to cmd_record */
		ntw_handle->cmd_record.param_size = buf_size;
		ntw_handle->cmd_record.cmd = mi->cmd;
		ntw_handle->cmd_record.stat = TEE_NTW_CMD_STAT_SET;
		*size = sizeof(cmd_struct) + ntw_handle->cmd_record.cmd_sz +
		    sizeof(params_struct) + ntw_handle->cmd_record.param_size;
		ntw_handle->cmd_record.msg_sz = *size;
	} else {
		TZDD_DBG("ERROR: both mi&&size and buf are NULL pointer\n");
		OSA_ASSERT(0);
	}
	return TEEC_SUCCESS;
}

/* ********************
 * set cmd in ntw
 * ********************/
tee_stat_t tee_msgm_set_cmd(tee_msgm_t msgm, tee_cmd_t cmd, void *arg)
{
	tee_msgm_ntw_struct *ntw_handle = NULL;
	tee_stat_t set_cmd_stat = TEEC_SUCCESS;

	OSA_ASSERT(msgm);
	if (TEE_CMD_INVALID == cmd) {
		TZDD_DBG("cmd TEE_CMD_INVALID\n");
		return set_cmd_stat;
	}
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	OSA_ASSERT(IS_MAGIC_VALID(ntw_handle->magic));
	INIT_CMD_MAGIC(ntw_handle->ntw_cmd->magic);

	ntw_handle->ntw_cmd->cmd = cmd;
	if (NULL != arg)
		set_cmd_stat = ntw_handle->op->set_cmd(msgm, arg);

	return set_cmd_stat;
}

/* ******************
 * get cmd in ntw
 * *******************/
tee_stat_t tee_msgm_get_cmd(tee_msgm_t msgm, tee_cmd_t *cmd, void *arg)
{
	tee_msgm_ntw_struct *ntw_handle = NULL;
	tee_stat_t get_cmd_stat = TEEC_SUCCESS;

	OSA_ASSERT(msgm);
	OSA_ASSERT((cmd || arg));
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	OSA_ASSERT(IS_MAGIC_VALID(ntw_handle->magic));

	if (NULL != cmd)
		*cmd = ntw_handle->ntw_cmd->cmd;

	if (NULL != arg)
		get_cmd_stat = ntw_handle->op->get_cmd(msgm, arg);

	return get_cmd_stat;
}

/* *****************
 * get ret in ntw
 * *****************/
tee_stat_t tee_msgm_get_ret(tee_msgm_t msgm, void *arg)
{
	tee_msgm_ntw_struct *ntw_handle = NULL;
	tee_stat_t get_ret_stat = TEEC_SUCCESS;

	OSA_ASSERT(msgm);
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	OSA_ASSERT(IS_MAGIC_VALID(ntw_handle->magic));

	if (NULL != arg)
		get_ret_stat = ntw_handle->op->get_ret(msgm, arg);

	return get_ret_stat;
}

/* ****************
 * set ret in ntw
 * ****************/
tee_stat_t tee_msgm_set_ret(tee_msgm_t msgm, void *arg)
{
	tee_msgm_ntw_struct *ntw_handle = NULL;
	tee_stat_t set_ret_stat = TEEC_SUCCESS;

	OSA_ASSERT((arg && msgm));
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	OSA_ASSERT(IS_MAGIC_VALID(ntw_handle->magic));

	set_ret_stat = ntw_handle->op->set_ret(msgm, arg);
	return set_ret_stat;
}

/* *********************
 * set params in ntw
 * *********************/
tee_stat_t tee_msgm_set_params(tee_msgm_t msgm, tee_msg_op_info_t *params)
{
	tee_msgm_ntw_struct *ntw_handle = NULL;
	tee_stat_t set_params_stat = TEEC_SUCCESS;
	uint32_t times = PARAM_NUMBERS, idx = 0, type = TEEC_NONE;

	OSA_ASSERT((msgm && params));
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	OSA_ASSERT(IS_MAGIC_VALID(ntw_handle->magic));
	while (times--) {
		type = TEEC_PARAM_TYPE_GET(params->paramTypes, idx);
		if (!IS_PARAMTYPES_TYPE_VALID(type)) {
			TZDD_DBG("tee_msgm_set_params: bad paramsTypes\n");
			return TEEC_ERROR_BAD_PARAMETERS;
		}
		if (IS_TYPE_MEMREF(type)) {
			if (!IS_NTW_MEM_FLAG_VALID
			    (params->params[idx].memref.parent->flags)) {
				TZDD_DBG
				    ("tee_msgm_set_params: \
				     un-compatible arg\n");
				return TEEC_ERROR_BAD_PARAMETERS;
			}
		}
		idx++;
	}
	set_params_stat = set_params(msgm, params);
	return set_params_stat;
}

/* **********************
 * update_params in ntw
 * **********************/
tee_stat_t tee_msgm_update_params(tee_msgm_t msgm, tee_msg_op_info_t *params)
{
	tee_msgm_ntw_struct *ntw_handle = NULL;
	tee_stat_t update_params_stat = TEEC_SUCCESS;

	OSA_ASSERT((msgm && params));
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	OSA_ASSERT(IS_MAGIC_VALID(ntw_handle->magic));

	update_params_stat = update_params(msgm, params);
	return update_params_stat;
}
