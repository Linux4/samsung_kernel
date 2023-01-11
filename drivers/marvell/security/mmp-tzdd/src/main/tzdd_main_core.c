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

#include <linux/time.h>
#include "tzdd_internal.h"

#define INIT_MAGIC(_n)  \
	do {	\
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
	do {	\
		((uint8_t *)_n)[0] = '\0';  \
		((uint8_t *)_n)[1] = '\0';  \
		((uint8_t *)_n)[2] = '\0';  \
		((uint8_t *)_n)[3] = '\0';  \
	} while (0)

static void _tzdd_ctx_list_add(TEEC_Context *teec_context,
				tzdd_ctx_node_t *tzdd_ctx_node)
{
	tzdd_pid_list_t *tzdd_pid = NULL;
	osa_list_t *entry;
	uint32_t flag = 0;
	pid_t pid;

	pid = current->tgid;

	osa_list_iterate(&(tzdd_dev->pid_list), entry) {
		OSA_LIST_ENTRY(entry, tzdd_pid_list_t, node, tzdd_pid);
		if (pid == tzdd_pid->pid) {
			flag = FIND_PID_IN_PID_LIST;
			break;
		}
	}

	if (flag == FIND_PID_IN_PID_LIST) {
		osa_list_init_head(&(tzdd_ctx_node->shm_list));
		osa_list_init_head(&(tzdd_ctx_node->ss_list));

		osa_list_add(&(tzdd_ctx_node->node),
				&(tzdd_pid->ctx_list));

		INIT_MAGIC(tzdd_ctx_node->magic);
		tzdd_ctx_node->tee_ctx_ntw = teec_context;
	}

	return;
}

static void _tzdd_ctx_list_del(tzdd_ctx_node_t *tzdd_ctx_node)
{
	tzdd_pid_list_t *tzdd_pid = NULL;
	osa_list_t *entry;
	uint32_t flag = 0;
	pid_t pid;

	pid = current->tgid;

	osa_list_iterate(&(tzdd_dev->pid_list), entry) {
		OSA_LIST_ENTRY(entry, tzdd_pid_list_t, node, tzdd_pid);
		if (pid == tzdd_pid->pid) {
			flag = FIND_PID_IN_PID_LIST;
			break;
		}
	}

	if (flag == FIND_PID_IN_PID_LIST) {
		if (!(IS_MAGIC_VALID(tzdd_ctx_node->magic)))
			OSA_ASSERT(0);
		CLEANUP_MAGIC(tzdd_ctx_node->magic);

		osa_list_del(&(tzdd_ctx_node->node));
	}

	return;
}

static void _tzdd_shm_list_add(TEEC_Context *teec_context,
				TEEC_SharedMemory *teec_shared_memory,
				tzdd_shm_node_t *tzdd_shm_node)
{
	tzdd_ctx_node_t *tzdd_ctx_node;
	_TEEC_MRVL_Context *_teec_mrvl_context;

	_teec_mrvl_context = (_TEEC_MRVL_Context *)(teec_context->imp);
	tzdd_ctx_node = (tzdd_ctx_node_t *)(_teec_mrvl_context->private_data);

	osa_list_add(&(tzdd_shm_node->node), &(tzdd_ctx_node->shm_list));
	INIT_MAGIC(tzdd_shm_node->magic);
	tzdd_shm_node->tee_ctx_ntw = teec_context;
	tzdd_shm_node->tee_shm_ntw = teec_shared_memory;

	return;
}

static void _tzdd_shm_list_del(tzdd_shm_node_t *tzdd_shm_node)
{
	TEEC_Context *teec_context;
	_TEEC_MRVL_Context *_teec_mrvl_context;
	tzdd_ctx_node_t *tzdd_ctx_node;

	if (!(IS_MAGIC_VALID(tzdd_shm_node->magic)))
		OSA_ASSERT(0);

	teec_context = tzdd_shm_node->tee_ctx_ntw;
	_teec_mrvl_context = (_TEEC_MRVL_Context *)(teec_context->imp);
	tzdd_ctx_node = (tzdd_ctx_node_t *)(_teec_mrvl_context->private_data);

	if (!(IS_MAGIC_VALID(tzdd_ctx_node->magic)))
		OSA_ASSERT(0);

	CLEANUP_MAGIC(tzdd_shm_node->magic);

	osa_list_del(&(tzdd_shm_node->node));

	return;
}

static void _tzdd_ss_list_add(TEEC_Context *teec_context,
				TEEC_Session *teec_session,
				tzdd_ss_node_t *tzdd_ss_node)
{
	tzdd_ctx_node_t *tzdd_ctx_node;
	_TEEC_MRVL_Context *_teec_mrvl_context;

	_teec_mrvl_context = (_TEEC_MRVL_Context *)(teec_context->imp);
	tzdd_ctx_node = (tzdd_ctx_node_t *)(_teec_mrvl_context->private_data);

	osa_list_add(&(tzdd_ss_node->node), &(tzdd_ctx_node->ss_list));
	INIT_MAGIC(tzdd_ss_node->magic);
	tzdd_ss_node->tee_ctx_ntw = teec_context;
	tzdd_ss_node->tee_ss_ntw = teec_session;

	return;
}

static void _tzdd_ss_list_del(tzdd_ss_node_t *tzdd_ss_node)
{
	TEEC_Context *teec_context;
	_TEEC_MRVL_Context *_teec_mrvl_context;
	tzdd_ctx_node_t *tzdd_ctx_node;

	if (!(IS_MAGIC_VALID(tzdd_ss_node->magic)))
		OSA_ASSERT(0);

	teec_context = tzdd_ss_node->tee_ctx_ntw;
	_teec_mrvl_context = (_TEEC_MRVL_Context *)(teec_context->imp);
	tzdd_ctx_node = (tzdd_ctx_node_t *)(_teec_mrvl_context->private_data);

	if (!(IS_MAGIC_VALID(tzdd_ctx_node->magic)))
		OSA_ASSERT(0);

	CLEANUP_MAGIC(tzdd_ss_node->magic);

	osa_list_del(&(tzdd_ss_node->node));

	return;
}

TEEC_Result _teec_initialize_context(const char *name, tee_impl *tee_ctx_ntw)
{
	TEEC_Context *teec_context;
	TEEC_Result result;
	tzdd_ctx_node_t *tzdd_ctx_node;
	_TEEC_MRVL_Context *_teec_mrvl_context;

	if (!name || (!osa_memcmp(name, DEVICE_NAME, DEVICE_NAME_SIZE))) {
		teec_context =
			(TEEC_Context *) osa_kmem_alloc(sizeof(TEEC_Context));
		if (!teec_context) {
			TZDD_DBG("Fail to malloc TEEC_Context\n");
			return TEEC_ERROR_OUT_OF_MEMORY;
		}

		result = TEEC_InitializeContext(name, teec_context);
		if (result != TEEC_SUCCESS) {
			TZDD_DBG
				("%s(%d), Error in TEEC_InitializeContext,"
				" result = 0x%x\n",
				__func__, __LINE__, result);
			*tee_ctx_ntw = NULL;
			osa_kmem_free(teec_context);

			return result;
		}

		*tee_ctx_ntw = (tee_impl) (&(teec_context->imp));

		_teec_mrvl_context = (_TEEC_MRVL_Context *)(teec_context->imp);
		tzdd_ctx_node = osa_kmem_alloc(sizeof(tzdd_ctx_node_t));
		OSA_ASSERT(tzdd_ctx_node != NULL);

		osa_wait_for_sem(tzdd_dev->pid_mutex, INFINITE);
		_tzdd_ctx_list_add(teec_context, tzdd_ctx_node);
		osa_release_sem(tzdd_dev->pid_mutex);

		_teec_mrvl_context->private_data = tzdd_ctx_node;
	} else {
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	return TEEC_SUCCESS;
}

TEEC_Result _teec_final_context(tee_impl tee_ctx_ntw)
{
	TEEC_Context *teec_context;
	TEEC_MRVL_Context *teec_mrvl_context;
	tzdd_ctx_node_t *tzdd_ctx_node;
	_TEEC_MRVL_Context *_teec_mrvl_context;

	/* Find the struct according to the member */
	teec_mrvl_context = (TEEC_MRVL_Context *) tee_ctx_ntw;
	teec_context = container_of(teec_mrvl_context, TEEC_Context, imp);

	_teec_mrvl_context = (_TEEC_MRVL_Context *)(teec_context->imp);
	tzdd_ctx_node = (tzdd_ctx_node_t *)(_teec_mrvl_context->private_data);

	osa_wait_for_sem(tzdd_dev->pid_mutex, INFINITE);
	_tzdd_ctx_list_del(tzdd_ctx_node);
	osa_release_sem(tzdd_dev->pid_mutex);

	osa_kmem_free(tzdd_ctx_node);

	TEEC_FinalizeContext(teec_context);

	osa_kmem_free(teec_context);

	return TEEC_SUCCESS;
}

TEEC_Result _teec_map_shared_mem(teec_map_shared_mem_args teec_map_shared_mem,
				tee_impl *tee_shm_ntw)
{
	/* only call TEEC_RegisterSharedMemory */
	TEEC_Context *teec_context;
	TEEC_MRVL_Context *teec_mrvl_context;
	TEEC_SharedMemory *teec_shared_memory;
	TEEC_Result result;
	tzdd_shm_node_t *tzdd_shm_node;
	_TEEC_MRVL_SharedMemory *_teec_mrvl_sharedMem;

	teec_mrvl_context =
		(TEEC_MRVL_Context *) (teec_map_shared_mem.tee_ctx_ntw);
	teec_context = container_of(teec_mrvl_context, TEEC_Context, imp);

	teec_shared_memory =
		(TEEC_SharedMemory *) osa_kmem_alloc(sizeof(TEEC_SharedMemory));
	if (!teec_shared_memory) {
		TZDD_DBG("Fail to malloc TEEC_SharedMemory\n");
		return TEEC_ERROR_OUT_OF_MEMORY;
	}

	teec_shared_memory->buffer = teec_map_shared_mem.buffer;
	teec_shared_memory->size = teec_map_shared_mem.size;
	teec_shared_memory->flags = teec_map_shared_mem.flags;

	result = TEEC_RegisterSharedMemory(teec_context, teec_shared_memory);
	if (result != TEEC_SUCCESS) {
		TZDD_DBG
			("%s(%d), Error in TEEC_RegisterSharedMemory, result = 0x%x\n",
			__func__, __LINE__, result);
		*tee_shm_ntw = NULL;
		osa_kmem_free(teec_shared_memory);

		return result;
	}

	*tee_shm_ntw = (tee_impl) (&(teec_shared_memory->imp));

	_teec_mrvl_sharedMem =
		(_TEEC_MRVL_SharedMemory *)(teec_shared_memory->imp);
	tzdd_shm_node = osa_kmem_alloc(sizeof(tzdd_shm_node_t));
	OSA_ASSERT(tzdd_shm_node != NULL);

	osa_wait_for_sem(tzdd_dev->pid_mutex, INFINITE);
	_tzdd_shm_list_add(teec_context, teec_shared_memory, tzdd_shm_node);
	osa_release_sem(tzdd_dev->pid_mutex);

	_teec_mrvl_sharedMem->private_data = tzdd_shm_node;

	return TEEC_SUCCESS;
}

TEEC_Result _teec_unmap_shared_mem(teec_unmap_shared_mem_args
				teec_unmap_shared_mem, tee_impl tee_shm_ntw)
{
	TEEC_SharedMemory *teec_shared_memory;
	TEEC_MRVL_SharedMemory *teec_mrvl_shared_mem;
	_TEEC_MRVL_SharedMemory *_teec_mrvl_sharedMem;
	tzdd_shm_node_t *tzdd_shm_node;

	teec_mrvl_shared_mem = (TEEC_MRVL_SharedMemory *) tee_shm_ntw;
	teec_shared_memory =
		container_of(teec_mrvl_shared_mem, TEEC_SharedMemory, imp);

	_teec_mrvl_sharedMem = (_TEEC_MRVL_SharedMemory *)(teec_shared_memory->imp);
	tzdd_shm_node = (tzdd_shm_node_t *)(_teec_mrvl_sharedMem->private_data);

	osa_wait_for_sem(tzdd_dev->pid_mutex, INFINITE);
	_tzdd_shm_list_del(tzdd_shm_node);
	osa_release_sem(tzdd_dev->pid_mutex);

	osa_kmem_free(tzdd_shm_node);

	TEEC_ReleaseSharedMemory(teec_shared_memory);

	osa_kmem_free(teec_shared_memory);

	return TEEC_SUCCESS;
}

TEEC_Result _teec_open_session(tee_impl tee_ctx_ntw,
				tee_impl *tee_ss_ntw,
				const TEEC_UUID *destination,
				uint32_t connectionMethod,
				const void *connectionData,
				uint32_t flags,
				uint32_t paramTypes,
				TEEC_Parameter(*params)[4],
				tee_impl *tee_op_ntw,
				tee_impl tee_op_ntw_for_cancel,
				uint32_t *returnOrigin,
				unsigned long arg,
				teec_open_ss_args open_ss_args)
{
	TEEC_Context *teec_context;
	TEEC_MRVL_Context *teec_mrvl_context;
	TEEC_Session *teec_session;
	TEEC_Operation *teec_operation = NULL;
	TEEC_Result result;
	uint32_t i;
	ulong_t tmp_for_cancel;
	tzdd_ss_node_t *tzdd_ss_node;
	_TEEC_MRVL_Session *_teec_mrvl_session;

	teec_mrvl_context = (TEEC_MRVL_Context *) (tee_ctx_ntw);
	teec_context = container_of(teec_mrvl_context, TEEC_Context, imp);

	teec_session = (TEEC_Session *) osa_kmem_alloc(sizeof(TEEC_Session));
	if (!teec_session) {
		TZDD_DBG("Fail to malloc TEEC_Session\n");
		return TEEC_ERROR_OUT_OF_MEMORY;
	}

	*tee_ss_ntw = (tee_impl) (&(teec_session->imp));

	if (flags == OPERATION_EXIST) {
		if (copy_from_user
			(&tmp_for_cancel, (void __user *)(tee_op_ntw_for_cancel),
			4)) {
			osa_kmem_free(teec_session);
			return -EFAULT;
		}

		teec_operation =
			(TEEC_Operation *) osa_kmem_alloc(sizeof(TEEC_Operation));
		if (!teec_operation) {
			TZDD_DBG("Fail to malloc TEEC_Operation\n");
			osa_kmem_free(teec_session);
			return TEEC_ERROR_OUT_OF_MEMORY;
		}
		teec_operation->started = 0;
		teec_operation->paramTypes = paramTypes;
		for (i = 0; i < 4; i++)
			teec_operation->params[i] = (*params)[i];

		*tee_op_ntw = (tee_impl) (&(teec_operation->imp));
		tmp_for_cancel = (ulong_t)(&(teec_operation->imp));

		/* Add copy_to_user here, for cancellation */
		if (copy_to_user
			((void __user *)(tee_op_ntw_for_cancel), &tmp_for_cancel,
			sizeof(tmp_for_cancel))) {
			osa_kmem_free(teec_operation);
			osa_kmem_free(teec_session);
			return -EFAULT;
		}
	} else {
		*tee_op_ntw = NULL;
	}

	result = TEEC_OpenSession(teec_context,
				teec_session,
				destination,
				connectionMethod,
				connectionData, teec_operation, returnOrigin);

	if (result != TEEC_SUCCESS) {
		TZDD_DBG("%s(%d), Error in TEEC_OpenSession, result = 0x%x\n",
			__func__, __LINE__, result);
		*tee_ss_ntw = NULL;
		osa_kmem_free(teec_session);

		if (flags == OPERATION_EXIST) {
			for (i = 0; i < 4; i++)
				(*params)[i] = teec_operation->params[i];

			*tee_op_ntw = NULL;
			osa_kmem_free(teec_operation);
		}
		return result;
	}

	/* Copy param in order to return to CA */
	if (flags == OPERATION_EXIST) {
		for (i = 0; i < 4; i++)
			(*params)[i] = teec_operation->params[i];

		osa_kmem_free(teec_operation);
	}

	_teec_mrvl_session =
		(_TEEC_MRVL_Session*)(teec_session->imp);
	tzdd_ss_node = osa_kmem_alloc(sizeof(tzdd_ss_node_t));
	OSA_ASSERT(tzdd_ss_node != NULL);

	osa_wait_for_sem(tzdd_dev->pid_mutex, INFINITE);
	_tzdd_ss_list_add(teec_context, teec_session, tzdd_ss_node);
	osa_release_sem(tzdd_dev->pid_mutex);

	_teec_mrvl_session->private_data = tzdd_ss_node;

	return TEEC_SUCCESS;
}

TEEC_Result _teec_close_session(tee_impl tee_ss_ntw)
{
	TEEC_Session *teec_session;
	TEEC_MRVL_Session *teec_mrvl_session;
	tzdd_ss_node_t *tzdd_ss_node;
	_TEEC_MRVL_Session *_teec_mrvl_session;

	teec_mrvl_session = (TEEC_MRVL_Session *) (tee_ss_ntw);
	teec_session = container_of(teec_mrvl_session, TEEC_Session, imp);

	_teec_mrvl_session = (_TEEC_MRVL_Session *)(teec_session->imp);
	tzdd_ss_node = (tzdd_ss_node_t *)(_teec_mrvl_session->private_data);

	osa_wait_for_sem(tzdd_dev->pid_mutex, INFINITE);
	_tzdd_ss_list_del(tzdd_ss_node);
	osa_release_sem(tzdd_dev->pid_mutex);

	osa_kmem_free(tzdd_ss_node);

	TEEC_CloseSession(teec_session);

	osa_kmem_free(teec_session);

	return TEEC_SUCCESS;
}

TEEC_Result _teec_invoke_command(tee_impl tee_ss_ntw,
				uint32_t cmd_ID,
				uint32_t started,
				uint32_t flags,
				uint32_t paramTypes,
				TEEC_Parameter(*params)[4],
				tee_impl *tee_op_ntw,
				tee_impl tee_op_ntw_for_cancel,
				uint32_t *returnOrigin,
				unsigned long arg,
				teec_invoke_cmd_args invoke_cmd_args)
{
	TEEC_Session *teec_session;
	TEEC_MRVL_Session *teec_mrvl_session;
	TEEC_Operation *teec_operation = NULL;
	TEEC_Result result;
	uint32_t i;
	ulong_t tmp_for_cancel;

	teec_mrvl_session = (TEEC_MRVL_Session *) (tee_ss_ntw);
	teec_session = container_of(teec_mrvl_session, TEEC_Session, imp);

	if (flags == OPERATION_EXIST) {
		teec_operation =
			(TEEC_Operation *) osa_kmem_alloc(sizeof(TEEC_Operation));
		if (!teec_operation) {
			TZDD_DBG("Fail to malloc TEEC_Operation\n");
			return TEEC_ERROR_OUT_OF_MEMORY;
		}
		teec_operation->started = 0;
		teec_operation->paramTypes = paramTypes;
		for (i = 0; i < 4; i++)
			teec_operation->params[i] = (*params)[i];

		*tee_op_ntw = (tee_impl) (&(teec_operation->imp));
		tmp_for_cancel = (ulong_t) (&(teec_operation->imp));

		/* Add copy_to_user here, for cancellation */
		if (copy_to_user
			((void __user *)(tee_op_ntw_for_cancel), &tmp_for_cancel,
			sizeof(tmp_for_cancel))) {
			osa_kmem_free(teec_operation);
			return -EFAULT;
		}
	} else {
		*tee_op_ntw = NULL;
	}

	result = TEEC_InvokeCommand(teec_session,
					cmd_ID, teec_operation, returnOrigin);

	if (result != TEEC_SUCCESS) {
		TZDD_DBG("%s(%d), Error in TEEC_InvokeCommand, result = 0x%x\n",
			__func__, __LINE__, result);
		if (flags == OPERATION_EXIST) {
			for (i = 0; i < 4; i++)
				(*params)[i] = teec_operation->params[i];

			*tee_op_ntw = NULL;
			osa_kmem_free(teec_operation);
		}
		return result;
	}

	/* Copy param in order to return to CA */
	if (flags == OPERATION_EXIST) {
		for (i = 0; i < 4; i++)
			(*params)[i] = teec_operation->params[i];

		osa_kmem_free(teec_operation);
	}

	return TEEC_SUCCESS;
}

TEEC_Result _teec_request_cancellation(tee_impl tee_op_ntw)
{
	TEEC_Operation *teec_operation;
	TEEC_MRVL_Operation *teec_mrvl_operation;

	teec_mrvl_operation = (TEEC_MRVL_Operation *) tee_op_ntw;
	teec_operation = container_of(teec_mrvl_operation, TEEC_Operation, imp);

	TEEC_RequestCancellation(teec_operation);

	return TEEC_SUCCESS;
}

/*
 * Name:        tzdd_get_first_req
 *
 * Description: get first node from request list
 *
 * Params:
 *   node       - node pointer
 *
 * Returns:
 *   true       - have request
 *   false      - have no request
 *
 * Notes:       none
 *
 */
bool tzdd_get_first_req(tee_msg_head_t **tee_msg_head)
{
	struct osa_list *first;

	*tee_msg_head = NULL;

	osa_wait_for_mutex(tzdd_dev->req_mutex, INFINITE);
	if (osa_list_empty(&tzdd_dev->req_list)) {
		osa_release_mutex(tzdd_dev->req_mutex);
		return false;
	} else {
		first = (tzdd_dev->req_list).next;
		OSA_LIST_ENTRY(first, tee_msg_head_t, node, *tee_msg_head);
	}
	osa_release_mutex(tzdd_dev->req_mutex);

	return true;
}

/*
 * Name:        tzdd_add_node_to_req_list
 *
 * Description: add node to request list
 *
 * Params:
 *   list       - list pointer
 *
 * Returns:     none
 *
 * Notes:       none
 *
 */
void tzdd_add_node_to_req_list(osa_list_t *list)
{
	osa_wait_for_mutex(tzdd_dev->req_mutex, INFINITE);
	osa_list_add_tail(list, &tzdd_dev->req_list);
	osa_release_mutex(tzdd_dev->req_mutex);

	return;
}

/*
 * Name:        tzdd_chk_node_on_req_list
 *
 * Description: check node to request list
 *
 * Params:
 *   list       - list pointer
 *
 * Returns:
 *   true       - node is on the list
 *   false      - node is not exist
 *
 * Notes:       none
 *
 */
bool tzdd_chk_node_on_req_list(osa_list_t *list)
{
	struct osa_list *entry;

	osa_wait_for_mutex(tzdd_dev->req_mutex, INFINITE);
	osa_list_iterate(&tzdd_dev->req_list, entry) {
		if (entry == list) {
			osa_release_mutex(tzdd_dev->req_mutex);
			return true;
		}
	}
	osa_release_mutex(tzdd_dev->req_mutex);

	return false;
}

/*
 * Name:        tzdd_del_node_from_req_list
 *
 * Description: delete node from request list
 *
 * Params:
 *   list       - list pointer
 *
 * Returns:
 *   true       - node has deleted from the list
 *   false      - node is not exist
 *
 * Notes:       none
 *
 */
bool tzdd_del_node_from_req_list(osa_list_t *list)
{
	struct osa_list *entry, *next;

	osa_wait_for_mutex(tzdd_dev->req_mutex, INFINITE);
	osa_list_iterate_safe(&tzdd_dev->req_list, entry, next) {
		if (entry == list) {
			osa_list_del(entry);
			osa_release_mutex(tzdd_dev->req_mutex);
			return true;
		}
	}
	osa_release_mutex(tzdd_dev->req_mutex);

	return false;
}

/*
 * Name:        tzdd_have_req
 *
 * Description: check there have some request in the run list
 *              and the request list or not
 *
 * Params:      none
 *
 * Returns:
 *   true       - have request
 *   false      - have no request
 *
 * Notes:       none
 *
 */
bool tzdd_have_req(void)
{
	osa_wait_for_mutex(tzdd_dev->req_mutex, INFINITE);
	if (!osa_list_empty(&tzdd_dev->req_list)) {
		osa_release_mutex(tzdd_dev->req_mutex);
		return true;
	}
	osa_release_mutex(tzdd_dev->req_mutex);

	return false;
}

uint32_t tzdd_get_req_num_in_list(void)
{
	struct osa_list *entry;
	uint32_t n = 0;

	osa_wait_for_mutex(tzdd_dev->req_mutex, INFINITE);
	osa_list_iterate(&tzdd_dev->req_list, entry) {
		n++;
	}
	osa_release_mutex(tzdd_dev->req_mutex);

	return n;
}

extern int32_t teec_shd_init(void);
extern void teec_shd_cleanup(void);
#ifdef TEE_DEBUG_ENALBE_PROC_FS_LOG
extern void tee_dbg_proc_fs_init(void);
extern void tee_dbg_proc_fs_cleanup(void);
#endif

int32_t tzdd_init_core(tzdd_dev_t *dev)
{
	int32_t ret;

	osa_list_init_head(&dev->req_list);
	dev->req_mutex = osa_create_mutex();

	ret = tee_cm_init();
	if (ret) {
		TZDD_DBG("%s: failed to init cm\n", __func__);

		goto _failure_in_cm_init;
	}
#ifdef TEE_DEBUG_ENALBE_PROC_FS_LOG
	tee_dbg_proc_fs_init();
#endif

	ret = tzdd_proxy_thread_init(dev);
	if (ret) {
		TZDD_DBG("%s: failed to init proxy threads\n", __func__);
		goto _failure_in_proxy_thread_init;
	}

	ret = (int32_t)teec_cb_module_init();
	if (ret) {
		TZDD_DBG("%s: failed to init callback module\n", __FUNCTION__);
		goto _failure_in_cb_init;
	}
	ret = (int32_t)teec_reg_time_cb();
	if (ret) {
		TZDD_DBG("%s: failed to reg time cb\n", __FUNCTION__);
		goto _failure_in_reg_time_cb;
	}

	ret = teec_shd_init();
	if (ret) {
		TZDD_DBG("%s: failed to init shared device\n", __FUNCTION__);
		goto _failure_in_shd_init;
	}

	return 0;

_failure_in_shd_init:
	teec_unreg_time_cb();
_failure_in_reg_time_cb:
	teec_cb_module_cleanup();
_failure_in_cb_init:
	tzdd_proxy_thread_cleanup(dev);
_failure_in_proxy_thread_init:
_failure_in_cm_init:

	tee_cm_cleanup();
	return ret;
}

void tzdd_cleanup_core(tzdd_dev_t *dev)
{
	teec_shd_cleanup();
	teec_unreg_time_cb();
	teec_cb_module_cleanup();
	tzdd_proxy_thread_cleanup(dev);
#ifdef TEE_DEBUG_ENALBE_PROC_FS_LOG
	tee_dbg_proc_fs_cleanup();
#endif
	tee_cm_cleanup();

	osa_destroy_mutex(dev->req_mutex);
	return;
}
