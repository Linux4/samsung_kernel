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

#include "tzdd_internal.h"

#define INIT_MAGIC(_n)  \
	do {                \
		((uint8_t *)_n)[0] = 'T';   \
		((uint8_t *)_n)[1] = 'Z';   \
		((uint8_t *)_n)[2] = 'D';   \
		((uint8_t *)_n)[3] = 'D';   \
	} while (0)

#define IS_MAGIC_VALID(_n) \
	(('T' == ((uint8_t *)_n)[0]) && \
	('Z' == ((uint8_t *)_n)[1]) && \
	('D' == ((uint8_t *)_n)[2]) && \
	('D' == ((uint8_t *)_n)[3]))

#define CLEANUP_MAGIC(_n) \
	do {                            \
		((uint8_t *)_n)[0] = '\0';  \
		((uint8_t *)_n)[1] = '\0';  \
		((uint8_t *)_n)[2] = '\0';  \
		((uint8_t *)_n)[3] = '\0';  \
	} while (0)

#define INIT_MSG_MAGIC(_m)  \
	do {                \
		_m[0] = 'M';    \
		_m[1] = 's';    \
		_m[2] = 'G';    \
		_m[3] = 'm';    \
	} while (0)
#define IS_MSG_MAGIC_VALID(_m) \
	(('M' == _m[0]) &&  \
	 ('s' == _m[1]) &&  \
	 ('G' == _m[2]) &&  \
	 ('m' == _m[3]))
#define CLEANUP_MSG_MAGIC(_m)  \
	do {                \
		_m[0] = _m[1] = _m[2] = _m[3] = 0;\
	} while (0)

#define CA_AOLLC_BUF    (1)
#define DRV_AOLLC_BUF   (2)

#define NO_SET          (0)
#define INC_ATOMIC      (1)
#define DEC_ATOMIC      (2)

#define OP_PARAM_NUM    (4)

static TEEC_Result _set_sharedMem_atomic(
					tee_impl tee_ctx_ntw,
					TEEC_Operation *operation,
					uint32_t set_atomic_flag)
{
	uint32_t param_types;
	uint32_t i, tmp;
	_TEEC_MRVL_SharedMemory *_teec_mrvl_sharedMem;

	param_types = operation->paramTypes;

	for (i = 0; i < OP_PARAM_NUM; i++) {
		tmp = TEEC_PARAM_TYPE_GET(param_types, i);
		if (tmp == TEEC_NONE) {
			continue;
		} else {
			switch (tmp) {
			case TEEC_MEMREF_WHOLE:
			case TEEC_MEMREF_PARTIAL_INPUT:
			case TEEC_MEMREF_PARTIAL_OUTPUT:
			case TEEC_MEMREF_PARTIAL_INOUT:
				{
					TEEC_RegisteredMemoryReference memref;
					memref = operation->params[i].memref;
					if (!memref.parent)
						OSA_ASSERT(0);

					_teec_mrvl_sharedMem =
					    (_TEEC_MRVL_SharedMemory *)
					    (memref.parent->imp);
					if (!(IS_MAGIC_VALID(
						_teec_mrvl_sharedMem->magic)))
						OSA_ASSERT(0);
#ifdef SHAREDMEM_CONTEXT_CHECKING
					if (_teec_mrvl_sharedMem->tee_ctx_ntw !=
						tee_ctx_ntw)
						return TEEC_ERROR_BAD_PARAMETERS;
#endif
					if (set_atomic_flag == INC_ATOMIC)
						osa_atomic_inc(
						&(_teec_mrvl_sharedMem->count));
					else if (set_atomic_flag == DEC_ATOMIC)
						osa_atomic_dec(
						&(_teec_mrvl_sharedMem->count));
				}
				break;
			default:
				break;
			}
		}

	}

	return TEEC_SUCCESS;
}

static tee_msg_head_t *_init_msg_head(uint8_t *tee_msg_buf, uint32_t buf_size)
{
	tee_msg_head_t *tee_msg_head = (tee_msg_head_t *)tee_msg_buf;
	INIT_MSG_MAGIC(tee_msg_head->magic);
	tee_msg_head->msg_sz = buf_size;
	tee_msg_head->msg_prop.value = 0;
	tee_msg_head->msg_prop.bp.stat = TEE_MSG_STAT_REQ;
	tee_msg_head->tee_msg = (void *)tee_msg_buf;
	tee_msg_head->comp = osa_create_sem(0);
	tee_msg_head->reserved = NULL;

	return tee_msg_head;
}

static TEEC_Result _teec_map_memory(TEEC_SharedMemory *sharedMem,
				    tee_get_ret_map_shm_arg_t *tee_get_ret)
{
	tee_msgm_t tee_msgm_handle;
	tee_cmd_t tee_msg_cmd;
	tee_set_cmd_map_shm_arg_t *tee_msg_set_cmd_map_shm_arg;
	tee_stat_t tee_msg_stat;
	uint32_t buf_size;
	tee_msgm_msg_info_t tee_msg_info;

	uint8_t *tee_msg_buf;
	tee_msg_head_t *tee_msg_head;

	/* prepare the request, create the msg and add it to the msgQ */
	tee_msgm_handle = tee_msgm_create_inst();
	if (!tee_msgm_handle)
		OSA_ASSERT(0);

	tee_msg_info.cmd = TEE_CMD_MAP_SHM;
	tee_msg_info.msg_map_shm_info = sharedMem;
	tee_msg_info.msg_op_info = NULL;

	/* Get buffer size */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, &tee_msg_info, NULL,
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	tee_msg_buf = osa_kmem_alloc(sizeof(tee_msg_head_t) + buf_size);
	if (!tee_msg_buf) {
		tee_msgm_destroy_inst(tee_msgm_handle);
		return TEEC_ERROR_OUT_OF_MEMORY;
	}

	/* give the buffer to Msgm */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, NULL,
				 tee_msg_buf + sizeof(tee_msg_head_t),
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	/* init head_t */
	tee_msg_head = _init_msg_head(tee_msg_buf, buf_size);

	/* set cmd body */
	tee_msg_cmd = TEE_CMD_MAP_SHM;
	tee_msg_set_cmd_map_shm_arg = sharedMem;

	tee_msg_stat =
	    tee_msgm_set_cmd(tee_msgm_handle, tee_msg_cmd,
			     tee_msg_set_cmd_map_shm_arg);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	tzdd_add_node_to_req_list(&(tee_msg_head->node));

	/* Wake up PT */
	osa_release_sem(tzdd_dev->pt_sem);
	/* osa_wakeup_process(tzdd_dev->proxy_thd); */

	/* Wait for the mutex, PT will wake it up */
	osa_wait_for_sem(tee_msg_head->comp, INFINITE);

	osa_destroy_sem(tee_msg_head->comp);

	/* deal with the response */
	if (!(IS_MSG_MAGIC_VALID(tee_msg_head->magic)))
		OSA_ASSERT(0);

	/* Get return */
	tee_msg_stat = tee_msgm_get_ret(tee_msgm_handle, (void *)tee_get_ret);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	osa_kmem_free(tee_msg_buf);
	tee_msgm_destroy_inst(tee_msgm_handle);

	return tee_msg_stat;
}

static void _teec_generate_connection_data(uint32_t connectionMethod,
		tee_set_cmd_open_ss_arg_t *tee_msg_set_cmd_open_ss_arg)
{
	tee_msg_set_cmd_open_ss_arg->meth = connectionMethod;

	switch (connectionMethod) {
	case TEEC_LOGIN_PUBLIC:
		{
			memset(&(tee_msg_set_cmd_open_ss_arg->data), 0,
			       sizeof(TEEC_UUID));
		}
		break;
	case TEEC_LOGIN_USER:
		{
			memset(&(tee_msg_set_cmd_open_ss_arg->data), 0,
			       sizeof(TEEC_UUID));
			tee_msg_set_cmd_open_ss_arg->data.timeLow =
			    current_uid();
		}
		break;
	case TEEC_LOGIN_GROUP:
		{
			memset(&(tee_msg_set_cmd_open_ss_arg->data), 0,
			       sizeof(TEEC_UUID));
			tee_msg_set_cmd_open_ss_arg->data.timeLow =
			    current_gid();
		}
		break;
	case TEEC_LOGIN_APPLICATION:
		{
			memset(&(tee_msg_set_cmd_open_ss_arg->data), 0,
			       sizeof(TEEC_UUID));
			tee_msg_set_cmd_open_ss_arg->data.timeLow =
			    current_euid();
		}
		break;
	case TEEC_LOGIN_USER_APPLICATION:
		{
			memset(&(tee_msg_set_cmd_open_ss_arg->data), 0,
			       sizeof(TEEC_UUID));
			tee_msg_set_cmd_open_ss_arg->data.timeLow =
			    (current_uid() << 16 | current_euid());
		}
		break;
	case TEEC_LOGIN_GROUP_APPLICATION:
		{
			memset(&(tee_msg_set_cmd_open_ss_arg->data), 0,
			       sizeof(TEEC_UUID));
			tee_msg_set_cmd_open_ss_arg->data.timeLow =
			    (current_gid() << 16 | current_euid());
		}
		break;
	case MRVL_LOGIN_APPLICATION_DAEMON:
		{
			memset(&(tee_msg_set_cmd_open_ss_arg->data), 0,
			       sizeof(TEEC_UUID));
			/* add more in the future */
		}
		break;
	default:
		/* never be here */
		OSA_ASSERT(0);
		break;
	}

	return;
}

TEEC_Result TEEC_InitializeContext(const char *name, TEEC_Context *context)
{
	_TEEC_MRVL_Context *_teec_mrvl_context;

#ifdef TEE_PERF_MEASURE
	/* Since performance measure is base on core level PMU registers.
	 * So we put all processors on core 0, the same core as PT.
	 */
	set_cpus_allowed_ptr(current, cpumask_of(0));
#endif

	if (name && (osa_memcmp(name, DEVICE_NAME, DEVICE_NAME_SIZE)))
		return TEEC_ERROR_BAD_PARAMETERS;

	if (!context)
		return TEEC_ERROR_BAD_PARAMETERS;

	context->imp = osa_kmem_alloc(sizeof(_TEEC_MRVL_Context));
	if (!(context->imp))
		return TEEC_ERROR_OUT_OF_MEMORY;

	_teec_mrvl_context = (_TEEC_MRVL_Context *) (context->imp);
	INIT_MAGIC(_teec_mrvl_context->magic);

	osa_atomic_set(&(_teec_mrvl_context->count), 0);

	tee_prepare_record_time();

	return TEEC_SUCCESS;
}

void TEEC_FinalizeContext(TEEC_Context *context)
{
	int32_t count;
	_TEEC_MRVL_Context *_teec_mrvl_context;

	if (!context)
		return;

	tee_finish_record_time();

	_teec_mrvl_context = (_TEEC_MRVL_Context *) (context->imp);
	TZDD_DBG("_teec_final_context name = %c%c%c%c\n",
		 _teec_mrvl_context->magic[0],
		 _teec_mrvl_context->magic[1],
		 _teec_mrvl_context->magic[2], _teec_mrvl_context->magic[3]);

	if (!(IS_MAGIC_VALID(_teec_mrvl_context->magic)))
		OSA_ASSERT(0);

	CLEANUP_MAGIC(_teec_mrvl_context->magic);

	count = osa_atomic_read(&(_teec_mrvl_context->count));
	if (count != 0)
		OSA_ASSERT(0);

	osa_kmem_free(context->imp);
	return;
}

TEEC_Result TEEC_RegisterSharedMemory(TEEC_Context *context,
				      TEEC_SharedMemory *sharedMem)
{
	TEEC_Result tee_result = TEEC_SUCCESS;
	TEEC_Result tee_msg_stat;
	_TEEC_MRVL_Context *_teec_mrvl_context;
	_TEEC_MRVL_SharedMemory *_teec_mrvl_sharedMem;

	tee_get_ret_map_shm_arg_t tee_get_ret;

	if (!context || !sharedMem || !(sharedMem->buffer))
		return TEEC_ERROR_BAD_PARAMETERS;

	if ((sharedMem->flags != TEEC_MEM_INPUT) &&
	    (sharedMem->flags != TEEC_MEM_OUTPUT) &&
	    (sharedMem->flags != (TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)))
		return TEEC_ERROR_BAD_PARAMETERS;

	_teec_mrvl_context = (_TEEC_MRVL_Context *) (context->imp);

	if (!(IS_MAGIC_VALID(_teec_mrvl_context->magic)))
		return TEEC_ERROR_BAD_PARAMETERS;

	if (sharedMem->size >= TEEC_CONFIG_SHAREDMEM_MAX_SIZE)
		return TEEC_ERROR_OUT_OF_MEMORY;

	if (sharedMem->size == 0)
		return TEEC_SUCCESS;

	osa_atomic_inc(&(_teec_mrvl_context->count));

	sharedMem->imp = osa_kmem_alloc(sizeof(_TEEC_MRVL_SharedMemory));
	if (!(sharedMem->imp))
		return TEEC_ERROR_OUT_OF_MEMORY;

	_teec_mrvl_sharedMem = (_TEEC_MRVL_SharedMemory *) (sharedMem->imp);
	_teec_mrvl_sharedMem->tee_ctx_ntw = &(context->imp);
	_teec_mrvl_sharedMem->flag = CA_AOLLC_BUF;
	osa_atomic_set(&(_teec_mrvl_sharedMem->count), 0);

	tee_msg_stat = _teec_map_memory(sharedMem, &tee_get_ret);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	_teec_mrvl_sharedMem->vaddr_tw = tee_get_ret.vaddr_tw;
	tee_result = tee_get_ret.ret;

	INIT_MAGIC(_teec_mrvl_sharedMem->magic);

	if (tee_result != TEEC_SUCCESS)
		osa_atomic_dec(&(_teec_mrvl_context->count));

	return tee_result;
}

TEEC_Result TEEC_AllocateSharedMemory(TEEC_Context *context,
				      TEEC_SharedMemory *sharedMem)
{
	TEEC_Result tee_result = TEEC_SUCCESS;
	TEEC_Result tee_msg_stat;
	_TEEC_MRVL_Context *_teec_mrvl_context;
	_TEEC_MRVL_SharedMemory *_teec_mrvl_sharedMem;

	tee_get_ret_map_shm_arg_t tee_get_ret;

	if (!sharedMem)
		return TEEC_ERROR_BAD_PARAMETERS;

	if (!context) {
		sharedMem->buffer = NULL;
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if ((sharedMem->flags != TEEC_MEM_INPUT) &&
	    (sharedMem->flags != TEEC_MEM_OUTPUT) &&
	    (sharedMem->flags != (TEEC_MEM_INPUT | TEEC_MEM_OUTPUT))) {
		sharedMem->buffer = NULL;
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	_teec_mrvl_context = (_TEEC_MRVL_Context *) (context->imp);

	if (!(IS_MAGIC_VALID(_teec_mrvl_context->magic))) {
		sharedMem->buffer = NULL;
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (sharedMem->size >= TEEC_CONFIG_SHAREDMEM_MAX_SIZE) {
		sharedMem->buffer = NULL;
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (sharedMem->size == 0)
		return TEEC_SUCCESS;

	sharedMem->buffer = osa_kmem_alloc(sharedMem->size);
	if (!sharedMem->buffer)
		return TEEC_ERROR_OUT_OF_MEMORY;

	osa_atomic_inc(&(_teec_mrvl_context->count));

	sharedMem->imp = osa_kmem_alloc(sizeof(_TEEC_MRVL_SharedMemory));
	if (!(sharedMem->imp))
		return TEEC_ERROR_OUT_OF_MEMORY;

	_teec_mrvl_sharedMem = (_TEEC_MRVL_SharedMemory *) (sharedMem->imp);
	_teec_mrvl_sharedMem->tee_ctx_ntw = &(context->imp);
	_teec_mrvl_sharedMem->flag = DRV_AOLLC_BUF;
	osa_atomic_set(&(_teec_mrvl_sharedMem->count), 0);

	tee_msg_stat = _teec_map_memory(sharedMem, &tee_get_ret);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	_teec_mrvl_sharedMem->vaddr_tw = tee_get_ret.vaddr_tw;
	tee_result = tee_get_ret.ret;

	INIT_MAGIC(_teec_mrvl_sharedMem->magic);

	if (tee_result != TEEC_SUCCESS)
		sharedMem->buffer = NULL;

	if (tee_result != TEEC_SUCCESS)
		osa_atomic_dec(&(_teec_mrvl_context->count));

	return tee_result;
}

void TEEC_ReleaseSharedMemory(TEEC_SharedMemory *sharedMem)
{
	TEEC_Context *teec_context;
	int32_t count;

	tee_msgm_t tee_msgm_handle;
	tee_cmd_t tee_msg_cmd;
	tee_set_cmd_unmap_shm_arg_t *tee_msg_set_cmd_ummap_shm_arg;
	tee_stat_t tee_msg_stat;
	uint32_t buf_size;
	tee_msgm_msg_info_t tee_msg_info;

	uint8_t *tee_msg_buf;
	tee_msg_head_t *tee_msg_head;
	_TEEC_MRVL_Context *_teec_mrvl_context;
	_TEEC_MRVL_SharedMemory *_teec_mrvl_sharedMem;

	if (!sharedMem)
		return;

	if (sharedMem->size == 0)
		return;

	_teec_mrvl_sharedMem = (_TEEC_MRVL_SharedMemory *) (sharedMem->imp);

	if (!(IS_MAGIC_VALID(_teec_mrvl_sharedMem->magic)))
		OSA_ASSERT(0);

	CLEANUP_MAGIC(_teec_mrvl_sharedMem->magic);

	if (_teec_mrvl_sharedMem->flag == DRV_AOLLC_BUF) {
		osa_kmem_free(sharedMem->buffer);
		sharedMem->buffer = NULL;
		sharedMem->size = 0;
		sharedMem->flags = 0;
	} else if (_teec_mrvl_sharedMem->flag == CA_AOLLC_BUF) {
		/* Do nothing */
		/* Do not need to free the buffer, it's owned by CA */
	}

	count = osa_atomic_read(&(_teec_mrvl_sharedMem->count));
	if (count != 0)
		OSA_ASSERT(0);

	/* prepare the request, create the msg and add it to the msgQ */
	tee_msgm_handle = tee_msgm_create_inst();
	if (!tee_msgm_handle)
		OSA_ASSERT(0);

	tee_msg_info.cmd = TEE_CMD_UNMAP_SHM;
	tee_msg_info.msg_map_shm_info = NULL;
	tee_msg_info.msg_op_info = NULL;

	/* Get buffer size */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, &tee_msg_info, NULL,
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	tee_msg_buf = osa_kmem_alloc(sizeof(tee_msg_head_t) + buf_size);
	if (!tee_msg_buf) {
		tee_msgm_destroy_inst(tee_msgm_handle);
		OSA_ASSERT(0);
	}

	/* give the buffer to Msgm */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, NULL,
				 tee_msg_buf + sizeof(tee_msg_head_t),
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	/* init head_t */
	tee_msg_head = _init_msg_head(tee_msg_buf, buf_size);

	/* set cmd body */
	tee_msg_cmd = TEE_CMD_UNMAP_SHM;
	tee_msg_set_cmd_ummap_shm_arg = sharedMem;
	tee_msg_stat =
	    tee_msgm_set_cmd(tee_msgm_handle, tee_msg_cmd,
			     tee_msg_set_cmd_ummap_shm_arg);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	tzdd_add_node_to_req_list(&(tee_msg_head->node));

	/* Wake up PT */
	osa_release_sem(tzdd_dev->pt_sem);
	/* osa_wakeup_process(tzdd_dev->proxy_thd); */

	/* Wait for the mutex, PT will wake it up */
	osa_wait_for_sem(tee_msg_head->comp, INFINITE);

	osa_destroy_sem(tee_msg_head->comp);

	/* deal with the response */
	osa_kmem_free(tee_msg_buf);
	tee_msgm_destroy_inst(tee_msgm_handle);

	teec_context =
	    container_of(_teec_mrvl_sharedMem->tee_ctx_ntw, TEEC_Context, imp);
	_teec_mrvl_context = (_TEEC_MRVL_Context *) (teec_context->imp);
	osa_atomic_dec(&(_teec_mrvl_context->count));

	_teec_mrvl_sharedMem->tee_ctx_ntw = NULL;
	_teec_mrvl_sharedMem->vaddr_tw = NULL;
	_teec_mrvl_sharedMem->size = 0;
	_teec_mrvl_sharedMem->flag = 0;

	osa_kmem_free(sharedMem->imp);
	return;
}

TEEC_Result TEEC_OpenSession(TEEC_Context *context,
			     TEEC_Session *session,
			     const TEEC_UUID *destination,
			     uint32_t connectionMethod,
			     const void *connectionData,
			     TEEC_Operation *operation,
			     uint32_t *returnOrigin)
{
	TEEC_Result tee_result = TEEC_SUCCESS;

	tee_msgm_t tee_msgm_handle;
	tee_cmd_t tee_msg_cmd;
	tee_set_cmd_open_ss_arg_t tee_msg_set_cmd_open_ss_arg;
	tee_stat_t tee_msg_stat;
	uint32_t buf_size;
	tee_msgm_msg_info_t tee_msg_info;
	tee_get_ret_open_ss_arg_t tee_get_ret;

	uint8_t *tee_msg_buf;
	tee_msg_head_t *tee_msg_head;
	_TEEC_MRVL_Context *_teec_mrvl_context;
	_TEEC_MRVL_Session *_teec_mrvl_session;
	_TEEC_MRVL_Operation *_teec_mrvl_operation;

	if (!context || !session || !destination) {
		if (returnOrigin)
			*returnOrigin = TEEC_ORIGIN_API;

		return TEEC_ERROR_BAD_PARAMETERS;
	}

	tee_add_time_record_point("naoe");
	switch (connectionMethod) {
	case TEEC_LOGIN_PUBLIC:
	case TEEC_LOGIN_USER:
	case TEEC_LOGIN_APPLICATION:
	case TEEC_LOGIN_USER_APPLICATION:
		{
			if (connectionData) {
				if (returnOrigin)
					*returnOrigin = TEEC_ORIGIN_API;

				return TEEC_ERROR_BAD_PARAMETERS;
			}
		}
		break;
	case TEEC_LOGIN_GROUP:
	case TEEC_LOGIN_GROUP_APPLICATION:
		{
			if (!connectionData) {
				if (returnOrigin)
					*returnOrigin = TEEC_ORIGIN_API;

				return TEEC_ERROR_BAD_PARAMETERS;
			}
		}
		break;
	case MRVL_LOGIN_APPLICATION_DAEMON:
		{
			/* Mrvl internal method */
			/* The thread should NOT be frozen
			* even the system is suspending
			*/
			unsigned long flags;
			local_irq_save(flags);
			current->flags |= PF_NOFREEZE;
			local_irq_restore(flags);
		}
		break;
	default:
		{
			if (returnOrigin)
				*returnOrigin = TEEC_ORIGIN_API;

			return TEEC_ERROR_BAD_PARAMETERS;
		}
		break;
	}

	_teec_mrvl_context = (_TEEC_MRVL_Context *) (context->imp);

	if (!(IS_MAGIC_VALID(_teec_mrvl_context->magic))) {
		if (returnOrigin)
			*returnOrigin = TEEC_ORIGIN_API;

		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (operation) {
		tee_result = _set_sharedMem_atomic((tee_impl)&(context->imp),
										operation, INC_ATOMIC);
		if (tee_result != TEEC_SUCCESS) {
			if (returnOrigin)
				*returnOrigin = TEEC_ORIGIN_API;
			return TEEC_ERROR_BAD_PARAMETERS;
		}
	}

	osa_atomic_inc(&(_teec_mrvl_context->count));

	session->imp = osa_kmem_alloc(sizeof(_TEEC_MRVL_Session));
	if (!(session->imp)) {
		if (returnOrigin)
			*returnOrigin = TEEC_ORIGIN_API;
		return TEEC_ERROR_OUT_OF_MEMORY;
	}
	_teec_mrvl_session = (_TEEC_MRVL_Session *) (session->imp);
	INIT_MAGIC(_teec_mrvl_session->magic);
	_teec_mrvl_session->tee_ctx_ntw = &(context->imp);
	osa_atomic_set(&(_teec_mrvl_session->count), 0);

	if (operation)
		osa_atomic_inc(&(_teec_mrvl_session->count));

	/* prepare the request, create the msg and add it to the msgQ */
	tee_msgm_handle = tee_msgm_create_inst();
	if (!tee_msgm_handle)
		OSA_ASSERT(0);

	tee_msg_info.cmd = TEE_CMD_OPEN_SS;
	tee_msg_info.msg_map_shm_info = NULL;
	tee_msg_info.msg_op_info = operation;

	/* Get buffer size */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, &tee_msg_info, NULL,
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	tee_msg_buf = osa_kmem_alloc(sizeof(tee_msg_head_t) + buf_size);
	if (!tee_msg_buf) {
		tee_msgm_destroy_inst(tee_msgm_handle);
		if (returnOrigin)
			*returnOrigin = TEEC_ORIGIN_API;

		return TEEC_ERROR_GENERIC;
	}

	/* give the buffer to Msgm */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, NULL,
				 tee_msg_buf + sizeof(tee_msg_head_t),
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	/* init head_t */
	tee_msg_head = _init_msg_head(tee_msg_buf, buf_size);

	if (operation) {
		operation->imp = osa_kmem_alloc(sizeof(_TEEC_MRVL_Operation));
		if (!(operation->imp)) {
			if (returnOrigin)
				*returnOrigin = TEEC_ORIGIN_API;
			return TEEC_ERROR_OUT_OF_MEMORY;
		}
		_teec_mrvl_operation =
		    (_TEEC_MRVL_Operation *) (operation->imp);

		_teec_mrvl_operation->tee_msg_ntw = (void *)tee_msg_buf;
	}

	/* set cmd body */
	tee_msg_cmd = TEE_CMD_OPEN_SS;
	tee_msg_set_cmd_open_ss_arg.uuid = (TEEC_UUID *) destination;
	_teec_generate_connection_data(connectionMethod,
				       &tee_msg_set_cmd_open_ss_arg);
	tee_msg_stat =
	    tee_msgm_set_cmd(tee_msgm_handle, tee_msg_cmd,
			     &tee_msg_set_cmd_open_ss_arg);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	/* set params body */
	if (operation) {
		tee_msg_stat = tee_msgm_set_params(tee_msgm_handle, operation);
		if (tee_msg_stat != TEEC_SUCCESS) {
			osa_atomic_dec(&(_teec_mrvl_context->count));

			if (operation) {
				osa_atomic_dec(&(_teec_mrvl_session->count));
				_set_sharedMem_atomic((tee_impl)&(context->imp),
										operation, DEC_ATOMIC);
			}

			osa_kmem_free(tee_msg_buf);
			tee_msgm_destroy_inst(tee_msgm_handle);
			if (returnOrigin)
				*returnOrigin = TEEC_ORIGIN_API;

			return tee_msg_stat;
		}
	}

	tzdd_add_node_to_req_list(&(tee_msg_head->node));

	tee_add_time_record_point("naow");
	/* Wake up PT */
	osa_release_sem(tzdd_dev->pt_sem);
	/* osa_wakeup_process(tzdd_dev->proxy_thd); */

	/* Wait for the mutex, PT will wake it up */
	osa_wait_for_sem(tee_msg_head->comp, INFINITE);
	tee_add_time_record_point("naob");
	osa_destroy_sem(tee_msg_head->comp);

	/* deal with the response */
	if (!(IS_MSG_MAGIC_VALID(tee_msg_head->magic)))
		OSA_ASSERT(0);

	/* Get return */
	if (operation) {
		tee_msg_stat =
		    tee_msgm_update_params(tee_msgm_handle, operation);
		if (tee_msg_stat != TEEC_SUCCESS)
			OSA_ASSERT(0);
	}

	tee_msg_stat = tee_msgm_get_ret(tee_msgm_handle, (void *)&tee_get_ret);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	_teec_mrvl_session->tee_ss_tw = tee_get_ret.ss_tw;
	if (returnOrigin)
		*returnOrigin = tee_get_ret.ret_orig;

	tee_result = tee_get_ret.ret;

	if (tee_result != TEEC_SUCCESS)
		osa_atomic_dec(&(_teec_mrvl_context->count));

	if (operation) {
		osa_atomic_dec(&(_teec_mrvl_session->count));
		_set_sharedMem_atomic((tee_impl)&(context->imp),
								operation, DEC_ATOMIC);
	}

	osa_kmem_free(tee_msg_buf);
	tee_msgm_destroy_inst(tee_msgm_handle);

	if (operation)
		osa_kmem_free(operation->imp);

	tee_add_time_record_point("naox");
	return tee_result;
}

void TEEC_CloseSession(TEEC_Session *session)
{
	TEEC_Context *teec_context;
	int32_t count;

	tee_msgm_t tee_msgm_handle;
	tee_cmd_t tee_msg_cmd;
	tee_set_cmd_close_ss_arg_t tee_msg_set_cmd_close_ss_arg;
	tee_stat_t tee_msg_stat;
	uint32_t buf_size;
	tee_msgm_msg_info_t tee_msg_info;

	uint8_t *tee_msg_buf;
	tee_msg_head_t *tee_msg_head;
	_TEEC_MRVL_Context *_teec_mrvl_context;
	_TEEC_MRVL_Session *_teec_mrvl_session;

	if (!session)
		return;

	tee_add_time_record_point("nace");
	_teec_mrvl_session = (_TEEC_MRVL_Session *) (session->imp);

	if (!(IS_MAGIC_VALID(_teec_mrvl_session->magic)))
		OSA_ASSERT(0);

	CLEANUP_MAGIC(_teec_mrvl_session->magic);

	count = osa_atomic_read(&(_teec_mrvl_session->count));
	if (count != 0)
		OSA_ASSERT(0);

	/* prepare the request, create the msg and add it to the msgQ */
	tee_msgm_handle = tee_msgm_create_inst();
	if (!tee_msgm_handle)
		OSA_ASSERT(0);

	tee_msg_info.cmd = TEE_CMD_CLOSE_SS;
	tee_msg_info.msg_map_shm_info = NULL;
	tee_msg_info.msg_op_info = NULL;

	/* Get buffer size */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, &tee_msg_info, NULL,
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	tee_msg_buf = osa_kmem_alloc(sizeof(tee_msg_head_t) + buf_size);
	if (!tee_msg_buf) {
		tee_msgm_destroy_inst(tee_msgm_handle);
		OSA_ASSERT(0);
	}

	/* give the buffer to Msgm */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, NULL,
				 tee_msg_buf + sizeof(tee_msg_head_t),
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	/* init head_t */
	tee_msg_head = _init_msg_head(tee_msg_buf, buf_size);

	/* set cmd body */
	tee_msg_cmd = TEE_CMD_CLOSE_SS;
	tee_msg_set_cmd_close_ss_arg = _teec_mrvl_session->tee_ss_tw;
	tee_msg_stat =
	    tee_msgm_set_cmd(tee_msgm_handle, tee_msg_cmd,
			     tee_msg_set_cmd_close_ss_arg);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	tzdd_add_node_to_req_list(&(tee_msg_head->node));

	tee_add_time_record_point("nacw");
	/* Wake up PT */
	osa_release_sem(tzdd_dev->pt_sem);
	/* osa_wakeup_process(tzdd_dev->proxy_thd); */

	/* Wait for the mutex, PT will wake it up */
	osa_wait_for_sem(tee_msg_head->comp, INFINITE);
	tee_add_time_record_point("nacb");

	osa_destroy_sem(tee_msg_head->comp);

	/* deal with the response */
	osa_kmem_free(tee_msg_buf);
	tee_msgm_destroy_inst(tee_msgm_handle);

	teec_context =
	    container_of(_teec_mrvl_session->tee_ctx_ntw, TEEC_Context, imp);
	_teec_mrvl_context = (_TEEC_MRVL_Context *) (teec_context->imp);
	osa_atomic_dec(&(_teec_mrvl_context->count));

	_teec_mrvl_session->tee_ctx_ntw = NULL;
	_teec_mrvl_session->tee_ss_tw = NULL;

	osa_kmem_free(session->imp);
	tee_add_time_record_point("nacx");

	return;
}

TEEC_Result TEEC_InvokeCommand(TEEC_Session *session,
			       uint32_t commandID,
			       TEEC_Operation *operation,
			       uint32_t *returnOrigin)
{
	TEEC_Result tee_result = TEEC_SUCCESS;

	tee_msgm_t tee_msgm_handle;
	tee_cmd_t tee_msg_cmd;
	tee_set_cmd_inv_op_arg_t tee_msg_set_cmd_inv_op_arg;
	tee_stat_t tee_msg_stat;
	uint32_t buf_size;
	tee_msgm_msg_info_t tee_msg_info;
	tee_get_ret_inv_op_arg_t tee_get_ret;

	uint8_t *tee_msg_buf;
	tee_msg_head_t *tee_msg_head;
	_TEEC_MRVL_Session *_teec_mrvl_session;
	_TEEC_MRVL_Operation *_teec_mrvl_operation;

	tee_add_time_record_point("naie");
	if (!session) {
		if (returnOrigin)
			*returnOrigin = TEEC_ORIGIN_API;

		return TEEC_ERROR_BAD_PARAMETERS;
	}

	_teec_mrvl_session = (_TEEC_MRVL_Session *) (session->imp);

	if (!(IS_MAGIC_VALID(_teec_mrvl_session->magic))) {
		if (returnOrigin)
			*returnOrigin = TEEC_ORIGIN_API;
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (operation) {
		tee_result = _set_sharedMem_atomic(
						_teec_mrvl_session->tee_ctx_ntw,
						operation, INC_ATOMIC);
		if (tee_result != TEEC_SUCCESS) {
			if (returnOrigin)
				*returnOrigin = TEEC_ORIGIN_API;
			return TEEC_ERROR_BAD_PARAMETERS;
		}
	}

	if (operation)
		osa_atomic_inc(&(_teec_mrvl_session->count));

	/* prepare the request, create the msg and add it to the msgQ */
	tee_msgm_handle = tee_msgm_create_inst();
	if (!tee_msgm_handle)
		OSA_ASSERT(0);

	tee_msg_info.cmd = TEE_CMD_INV_OP;
	tee_msg_info.msg_map_shm_info = NULL;
	if (operation)
		tee_msg_info.msg_op_info = operation;
	else
		tee_msg_info.msg_op_info = NULL;

	/* Get buffer size */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, &tee_msg_info, NULL,
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	tee_msg_buf = osa_kmem_alloc(sizeof(tee_msg_head_t) + buf_size);
	if (!tee_msg_buf) {
		tee_msgm_destroy_inst(tee_msgm_handle);
		if (returnOrigin)
			*returnOrigin = TEEC_ORIGIN_API;

		return TEEC_ERROR_GENERIC;
	}

	/* give the buffer to Msgm */
	tee_msg_stat =
	    tee_msgm_set_msg_buf(tee_msgm_handle, NULL,
				 tee_msg_buf + sizeof(tee_msg_head_t),
				 &buf_size);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	/* init head_t */
	tee_msg_head = _init_msg_head(tee_msg_buf, buf_size);

	if (operation) {
		operation->imp = osa_kmem_alloc(sizeof(_TEEC_MRVL_Operation));
		if (!(operation->imp)) {
			if (returnOrigin)
				*returnOrigin = TEEC_ORIGIN_API;
			return TEEC_ERROR_OUT_OF_MEMORY;
		}

		_teec_mrvl_operation =
		    (_TEEC_MRVL_Operation *) (operation->imp);

		_teec_mrvl_operation->tee_msg_ntw = (void *)tee_msg_buf;
	}

	/* set cmd body */
	tee_msg_cmd = TEE_CMD_INV_OP;
	tee_msg_set_cmd_inv_op_arg.ss = _teec_mrvl_session->tee_ss_tw;
	tee_msg_set_cmd_inv_op_arg.srv_cmd = commandID;
	tee_msg_stat =
	    tee_msgm_set_cmd(tee_msgm_handle, tee_msg_cmd,
			     &tee_msg_set_cmd_inv_op_arg);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	/* set params body */
	if (operation) {
		tee_msg_stat = tee_msgm_set_params(tee_msgm_handle, operation);
		if (tee_msg_stat != TEEC_SUCCESS) {
			if (operation) {
				osa_atomic_dec(&(_teec_mrvl_session->count));
				_set_sharedMem_atomic(_teec_mrvl_session->tee_ctx_ntw,
									  operation, DEC_ATOMIC);
			}

			osa_kmem_free(tee_msg_buf);
			tee_msgm_destroy_inst(tee_msgm_handle);
			if (returnOrigin)
				*returnOrigin = TEEC_ORIGIN_API;
			return tee_msg_stat;
		}
	}

	tzdd_add_node_to_req_list(&(tee_msg_head->node));
	tee_add_time_record_point("naiw");
	/* Wake up PT */
	osa_release_sem(tzdd_dev->pt_sem);
	/* osa_wakeup_process(tzdd_dev->proxy_thd); */

	/* Wait for the mutex, PT will wake it up */
	osa_wait_for_sem(tee_msg_head->comp, INFINITE);
	tee_add_time_record_point("naib");
	osa_destroy_sem(tee_msg_head->comp);

	/* deal with the response */
	if (!(IS_MSG_MAGIC_VALID(tee_msg_head->magic)))
		OSA_ASSERT(0);

	/* Get return */
	if (operation) {
		tee_msg_stat =
		    tee_msgm_update_params(tee_msgm_handle, operation);
		if (tee_msg_stat != TEEC_SUCCESS)
			OSA_ASSERT(0);
	}

	tee_msg_stat = tee_msgm_get_ret(tee_msgm_handle, (void *)&tee_get_ret);
	if (tee_msg_stat != TEEC_SUCCESS)
		OSA_ASSERT(0);

	if (returnOrigin)
		*returnOrigin = tee_get_ret.ret_orig;

	tee_result = tee_get_ret.ret;

	if (operation) {
		osa_atomic_dec(&(_teec_mrvl_session->count));
		_set_sharedMem_atomic(_teec_mrvl_session->tee_ctx_ntw,
							  operation, DEC_ATOMIC);
	}

	osa_kmem_free(tee_msg_buf);
	tee_msgm_destroy_inst(tee_msgm_handle);

	if (operation)
		osa_kmem_free(operation->imp);

	tee_add_time_record_point("naix");
	return tee_result;
}

void TEEC_RequestCancellation(TEEC_Operation *operation)
{
	tee_msg_head_t *tee_msg_head;

	tee_msgm_t tee_msgm_handle;
	tee_cmd_t tee_msg_cmd;
	tee_set_cmd_can_op_arg_t *tee_msg_set_cmd_can_op_arg;
	tee_stat_t tee_msg_stat;
	uint32_t buf_size;
	tee_msgm_msg_info_t tee_msg_info;

	uint8_t *tee_msg_buf;
	_TEEC_MRVL_Operation *_teec_mrvl_operation;

	if (!operation)
		OSA_ASSERT(0);

	_teec_mrvl_operation = (_TEEC_MRVL_Operation *)(operation->imp);

	tee_msg_head = _teec_mrvl_operation->tee_msg_ntw;

	if (tzdd_chk_node_on_req_list(&(tee_msg_head->node))) {
		/* The msg is in the msgQ in NTW */
		tee_msg_head->msg_prop.bp.stat = TEE_MSG_STAT_CAN;
		return;
	} else {
		/* The msg has been sent to the TW */

		/* prepare the request, create the msg and add it to the msgQ */
		tee_msgm_handle = tee_msgm_create_inst();
		if (!tee_msgm_handle)
			OSA_ASSERT(0);

		tee_msg_info.cmd = TEE_CMD_CAN_OP;
		tee_msg_info.msg_map_shm_info = NULL;
		tee_msg_info.msg_op_info = operation;

		/* Get buffer size */
		tee_msg_stat =
		    tee_msgm_set_msg_buf(tee_msgm_handle, &tee_msg_info, NULL,
					 &buf_size);
		if (tee_msg_stat != TEEC_SUCCESS)
			OSA_ASSERT(0);

		tee_msg_buf = osa_kmem_alloc(sizeof(tee_msg_head_t) + buf_size);
		if (!tee_msg_buf) {
			tee_msgm_destroy_inst(tee_msgm_handle);
			OSA_ASSERT(0);
		}

		/* give the buffer to Msgm */
		tee_msg_stat =
		    tee_msgm_set_msg_buf(tee_msgm_handle, NULL,
					 tee_msg_buf + sizeof(tee_msg_head_t),
					 &buf_size);
		if (tee_msg_stat != TEEC_SUCCESS)
			OSA_ASSERT(0);

		/* init head_t */
		tee_msg_head = _init_msg_head(tee_msg_buf, buf_size);

		_teec_mrvl_operation->tee_msg_ntw = (void *)tee_msg_buf;

		/* set cmd body */
		tee_msg_cmd = TEE_CMD_CAN_OP;
		tee_msg_set_cmd_can_op_arg = operation;
		tee_msg_stat =
		    tee_msgm_set_cmd(tee_msgm_handle, tee_msg_cmd,
				     tee_msg_set_cmd_can_op_arg);
		if (tee_msg_stat != TEEC_SUCCESS)
			OSA_ASSERT(0);

		tzdd_add_node_to_req_list(&(tee_msg_head->node));

		/* Wake up PT */
		osa_release_sem(tzdd_dev->pt_sem);
		/* osa_wakeup_process(tzdd_dev->proxy_thd); */

		tee_msgm_destroy_inst(tee_msgm_handle);
	}
	return;
}
