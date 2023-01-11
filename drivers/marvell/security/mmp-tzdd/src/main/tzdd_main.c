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

#include <uapi/linux/resource.h>
#include <asm/pgtable.h>
#include "tzdd_internal.h"

MODULE_LICENSE("GPL");

/*
 * Parameters which can be set at load time.
 */

#ifdef TEE_RES_CFG_8M
#define TZDD_VERSION    "TEEC Drvier Version 1.1.4 8MB, kernel_3_10/3_14"
#elif defined (TEE_RES_CFG_16M)
#define TZDD_VERSION    "TEEC Drvier Version 1.1.4 16MB, kernel_3_10/3_14"
#elif defined(TEE_RES_CFG_24M)
#define TZDD_VERSION    "TEEC Drvier Version 1.1.4 24MB, kernel_3_10/3_14"
#else
#define TZDD_VERSION    "TEEC Drvier Version 1.1.4, kernel_3_10/3_14"
#endif
#define TZDD_DRV_NAME   "tzdd"

int tzdd_major;
int tzdd_minor;

module_param(tzdd_major, int, S_IRUGO);
module_param(tzdd_minor, int, S_IRUGO);

/* driver data struct */
tzdd_dev_t *tzdd_dev;

#define OP_PARAM_NUM	4

#define IS_MAGIC_VALID(_n) \
	(('T' == ((uint8_t *)_n)[0]) && \
	('Z' == ((uint8_t *)_n)[1]) && \
	('D' == ((uint8_t *)_n)[2]) && \
	('D' == ((uint8_t *)_n)[3]))
#define CLEANUP_MAGIC(_n) \
	do {	\
		((uint8_t *)_n)[0] = '\0';	\
		((uint8_t *)_n)[1] = '\0';	\
		((uint8_t *)_n)[2] = '\0';	\
		((uint8_t *)_n)[3] = '\0';	\
	} while (0)

static void _tzdd_free_session(tzdd_ctx_node_t *tzdd_ctx_node)
{
	TEEC_Session *teec_session;
	tzdd_ss_node_t *tzdd_ss_node;
	osa_list_t *entry, *next;

	osa_list_iterate_safe(&(tzdd_ctx_node->ss_list), entry, next) {
		OSA_LIST_ENTRY(entry, tzdd_ss_node_t, node, tzdd_ss_node);
		if (!(IS_MAGIC_VALID(tzdd_ss_node->magic)))
			OSA_ASSERT(0);
		CLEANUP_MAGIC(tzdd_ss_node->magic);

		osa_list_del(&(tzdd_ss_node->node));

		teec_session = (TEEC_Session *)(tzdd_ss_node->tee_ss_ntw);
		TEEC_CloseSession(teec_session);

		osa_kmem_free(tzdd_ss_node);
		osa_kmem_free(teec_session);
	}

	return;
}

static void _tzdd_free_shared_mem(tzdd_ctx_node_t *tzdd_ctx_node)
{
	TEEC_SharedMemory *teec_shared_mem;
	tzdd_shm_node_t *tzdd_shm_node;
	osa_list_t *entry, *next;

	osa_list_iterate_safe(&(tzdd_ctx_node->shm_list), entry, next) {
		OSA_LIST_ENTRY(entry, tzdd_shm_node_t, node, tzdd_shm_node);
		if (!(IS_MAGIC_VALID(tzdd_shm_node->magic)))
			OSA_ASSERT(0);
		CLEANUP_MAGIC(tzdd_shm_node->magic);

		osa_list_del(&(tzdd_shm_node->node));

		teec_shared_mem = (TEEC_SharedMemory *)(tzdd_shm_node->tee_shm_ntw);
		TEEC_ReleaseSharedMemory(teec_shared_mem);

		osa_kmem_free(tzdd_shm_node);
		osa_kmem_free(teec_shared_mem);
	}

	return;
}

static void _tzdd_free_context(tzdd_ctx_node_t *tzdd_ctx_node)
{
	TEEC_Context *teec_context;

	CLEANUP_MAGIC(tzdd_ctx_node->magic);

	osa_list_del(&(tzdd_ctx_node->node));

	teec_context = (TEEC_Context *)(tzdd_ctx_node->tee_ctx_ntw);
	TEEC_FinalizeContext(teec_context);

	osa_kmem_free(tzdd_ctx_node);
	osa_kmem_free(teec_context);

	return;
}

static int tzdd_open(struct inode *inode, struct file *filp)
{
	tzdd_pid_list_t *tzdd_pid;
	tzdd_pid_list_t *tmp_pid;
	osa_list_t *entry;
	uint32_t flag = 0;
	pid_t pid;

	pid = current->tgid;

	osa_wait_for_sem(tzdd_dev->pid_mutex, INFINITE);
	osa_list_iterate(&(tzdd_dev->pid_list), entry) {
		OSA_LIST_ENTRY(entry, tzdd_pid_list_t, node, tmp_pid);
		if (pid == tmp_pid->pid) {
			flag = FIND_PID_IN_PID_LIST;
			tmp_pid->count++;
			break;
		}
	}

	if (flag != FIND_PID_IN_PID_LIST) {
		struct rlimit rlim_inf = { RLIM_INFINITY, RLIM_INFINITY };
		ulong_t flags;
		/* tzdd_pid will be released in tzdd_close */
		tzdd_pid = osa_kmem_alloc(sizeof(tzdd_pid_list_t));
		OSA_ASSERT(tzdd_pid != NULL);

		osa_list_init_head(&(tzdd_pid->ctx_list));
		tzdd_pid->pid = pid;
		tzdd_pid->count = 1;
		/*
		 * to make sure current->signal is valid,
		 * tasklist_lock should be held.
		 * since tasklist_lock is NOT exported to kernel modules,
		 * we have to save/restore irq to make sure current->signal is valid.
		 */
		/* no tasklist_lock exported: read_lock(&tasklist_lock); */
		task_lock(current->group_leader);
		local_irq_save(flags);
		if (current->signal) {
			/* save the previous rlim[RLIMIT_MEMLOCK] */
			tzdd_pid->mlock_rlim_orig = current->signal->rlim[RLIMIT_MEMLOCK];
			/* make the current task's rlim[RLIMIT_MEMLOCK] infinite */
			current->signal->rlim[RLIMIT_MEMLOCK] = rlim_inf;
		}
		local_irq_restore(flags);
		task_unlock(current->group_leader);
		/* no tasklist_lock exported: read_unlock(&tasklist_lock); */
		osa_list_add(&(tzdd_pid->node), &(tzdd_dev->pid_list));
	}

	osa_release_sem(tzdd_dev->pid_mutex);

	return 0;
}

static int tzdd_close(struct inode *inode, struct file *filp)
{
	tzdd_pid_list_t *tzdd_pid = NULL;
	osa_list_t *entry, *next;
	uint32_t flag;
	pid_t pid;

	pid = current->tgid;

	osa_wait_for_sem(tzdd_dev->pid_mutex, INFINITE);
	osa_list_iterate(&(tzdd_dev->pid_list), entry) {
		OSA_LIST_ENTRY(entry, tzdd_pid_list_t, node, tzdd_pid);
		if (pid == tzdd_pid->pid) {
			flag = FIND_PID_IN_PID_LIST;
			if (tzdd_pid->count > 0)
				tzdd_pid->count--;
			break;
		}
	}

	if ((flag == FIND_PID_IN_PID_LIST) &&
		(tzdd_pid->count == 0)) {
		tzdd_ctx_node_t *tzdd_ctx_node;
		ulong_t flags;
		osa_list_iterate_safe(&(tzdd_pid->ctx_list), entry, next) {
			OSA_LIST_ENTRY(entry, tzdd_ctx_node_t, node, tzdd_ctx_node);
			if (!(IS_MAGIC_VALID(tzdd_ctx_node->magic)))
				OSA_ASSERT(0);
			_tzdd_free_session(tzdd_ctx_node);
			_tzdd_free_shared_mem(tzdd_ctx_node);
			_tzdd_free_context(tzdd_ctx_node);
		}
		/* no tasklist_lock exported: read_lock(&tasklist_lock); */
		task_lock(current->group_leader);
		local_irq_save(flags);
		if (current->signal) {
			/* restore the current task's rlim[RLIMIT_MEMLOCK] */
			current->signal->rlim[RLIMIT_MEMLOCK] = tzdd_pid->mlock_rlim_orig;
		}
		local_irq_restore(flags);
		task_unlock(current->group_leader);
		/* no tasklist_lock exported: read_unlock(&tasklist_lock); */
		osa_list_del(&(tzdd_pid->node));
		osa_kmem_free(tzdd_pid);
	}

	osa_release_sem(tzdd_dev->pid_mutex);

	return 0;
}

static long tzdd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	TEEC_Result result;

	OSA_ASSERT(arg);

	switch (cmd) {
	case TEEC_INIT_CONTEXT:
	{
		teec_init_context_args teec_init_context;

		if (copy_from_user
			(&teec_init_context, (void __user *)arg,
			sizeof(teec_init_context_args))) {
			return -EFAULT;
		}

		result =
			_teec_initialize_context(teec_init_context.name,
						&(teec_init_context.
						tee_ctx_ntw));
		teec_init_context.ret = result;

		if (copy_to_user
			((void __user *)arg, &teec_init_context,
			sizeof(teec_init_context_args))) {
			return -EFAULT;
		}
	}
	break;
	case TEEC_FINAL_CONTEXT:
	{
		teec_final_context_args teec_final_context;
		if (copy_from_user
			(&teec_final_context, (void __user *)arg,
			sizeof(teec_final_context_args))) {
			return -EFAULT;
		}

		OSA_ASSERT(teec_final_context.tee_ctx_ntw);

		result =
			_teec_final_context(teec_final_context.tee_ctx_ntw);
		teec_final_context.ret = result;

		if (copy_to_user
			((void __user *)arg, &teec_final_context,
			sizeof(teec_final_context_args))) {
			return -EFAULT;
		}
	}
	break;
	case TEEC_REGIST_SHARED_MEM:
	case TEEC_ALLOC_SHARED_MEM:
	{
		teec_map_shared_mem_args teec_map_shared_mem;
		void *buf_local_virt;
		tee_memm_ss_t memm_handle;
		if (copy_from_user
			(&teec_map_shared_mem, (void __user *)arg,
			sizeof(teec_map_shared_mem_args))) {
			return -EFAULT;
		}

		if ((((unsigned long long) teec_map_shared_mem.buffer) < VMALLOC_START) &&
			(((unsigned long long) teec_map_shared_mem.buffer) < MODULES_VADDR) &&
			((0 != (unsigned long long) teec_map_shared_mem.buffer) &&
			 (0 != teec_map_shared_mem.size))) {
			memm_handle = tee_memm_create_ss();
			tee_memm_get_user_mem(memm_handle,
				teec_map_shared_mem.buffer,
				teec_map_shared_mem.size,
				&buf_local_virt);
			if (!buf_local_virt) {
				teec_map_shared_mem.ret = TEEC_ERROR_GENERIC;
				if (copy_to_user((void __user *)arg, &teec_map_shared_mem,
					sizeof(teec_map_shared_mem_args))) {
					return -EFAULT;
				}
				TZDD_DBG("Error in tee_memm_get_user_mem\n");
				return -EFAULT;
			}
			tee_memm_destroy_ss(memm_handle);
			teec_map_shared_mem.buffer = buf_local_virt;
		}

		OSA_ASSERT(teec_map_shared_mem.tee_ctx_ntw);
		result =
			_teec_map_shared_mem(teec_map_shared_mem,
					&(teec_map_shared_mem.
					tee_shm_ntw));
		teec_map_shared_mem.ret = result;

		if (copy_to_user
			((void __user *)arg, &teec_map_shared_mem,
			sizeof(teec_map_shared_mem_args))) {
			return -EFAULT;
		}
	}
	break;
	case TEEC_RELEASE_SHARED_MEM:
	{
		teec_unmap_shared_mem_args teec_unmap_shared_mem;
		if (copy_from_user
			(&teec_unmap_shared_mem, (void __user *)arg,
			sizeof(teec_unmap_shared_mem_args))) {
			return -EFAULT;
		}

		OSA_ASSERT(teec_unmap_shared_mem.tee_shm_ntw);
		result =
			_teec_unmap_shared_mem(teec_unmap_shared_mem,
					teec_unmap_shared_mem.
					tee_shm_ntw);
		teec_unmap_shared_mem.ret = result;

		if (copy_to_user
			((void __user *)arg, &teec_unmap_shared_mem,
			sizeof(teec_unmap_shared_mem_args))) {
			return -EFAULT;
		}
	}
	break;
	case TEEC_OPEN_SS:
	{
		uint32_t param_types, tmp_types;
		uint32_t i;
		uint8_t *tmpref_buf[OP_PARAM_NUM];
		void *save_buf_addr[OP_PARAM_NUM];

		teec_open_ss_args teec_open_ss;
		TEEC_UUID destination;
		uint32_t _connectionData;
		void *connectionData;

		for (i = 0; i < OP_PARAM_NUM; i++) {
			tmpref_buf[i] = NULL;
			save_buf_addr[i] = NULL;
		}

		if (copy_from_user
			(&teec_open_ss, (void __user *)arg,
			sizeof(teec_open_ss_args))) {
			return -EFAULT;
		}

		if (teec_open_ss.destination) {
			if (copy_from_user
				(&destination,
				(void __user *)teec_open_ss.destination,
				sizeof(TEEC_UUID))) {
				return -EFAULT;
			}
		}

		if (teec_open_ss.connectionData) {
			if (copy_from_user
				(&_connectionData,
				(void __user *)teec_open_ss.connectionData,
				sizeof(uint32_t))) {
				return -EFAULT;
			}
			connectionData = (void *)&_connectionData;
		} else {
			connectionData = NULL;
		}

		if (teec_open_ss.flags == OPERATION_EXIST) {
			param_types = teec_open_ss.paramTypes;
			for (i = 0; i < OP_PARAM_NUM; i++) {
				tmp_types =
					TEEC_PARAM_TYPE_GET(param_types, i);
				switch (tmp_types) {
				case TEEC_MEMREF_TEMP_INPUT:
				case TEEC_MEMREF_TEMP_OUTPUT:
				case TEEC_MEMREF_TEMP_INOUT:
				{
					if (NULL !=
						teec_open_ss.params[i]
						.tmpref.buffer) {
						tmpref_buf[i] = memdup_user((void __user *)
							(teec_open_ss.params[i].tmpref.buffer),
							teec_open_ss.params[i].tmpref.size);
						if (NULL == tmpref_buf[i]) {
							TZDD_DBG
							("%s(%d): Failed to \
							allocate buffer\n",
							__func__,
							__LINE__);
							return -ENOMEM;
						}
						save_buf_addr[i] =
							teec_open_ss.params[i].tmpref.buffer;
						teec_open_ss.params[i].tmpref.buffer =
							tmpref_buf[i];
					}
				}
				break;
				case TEEC_MEMREF_WHOLE:
				case TEEC_MEMREF_PARTIAL_INPUT:
				case TEEC_MEMREF_PARTIAL_OUTPUT:
				case TEEC_MEMREF_PARTIAL_INOUT:
				{
					TEEC_MRVL_SharedMemory *teec_mrvl_shared_mem;
					tee_impl impl;
					if (copy_from_user(&impl,
						(void __user *)teec_open_ss.params[i].memref.parent,
						sizeof(tee_impl))) {
						return -EFAULT;
					}
					save_buf_addr[i] = impl;
					teec_mrvl_shared_mem =
						(TEEC_MRVL_SharedMemory *)(save_buf_addr[i]);
					teec_open_ss.params[i].memref.parent =
						container_of(teec_mrvl_shared_mem,
						TEEC_SharedMemory,
						imp);
				}
				break;
				default:
					break;
				}
			}
		}

		OSA_ASSERT(teec_open_ss.tee_ctx_ntw);

		result = _teec_open_session(
						teec_open_ss.tee_ctx_ntw,
						&(teec_open_ss.tee_ss_ntw),
						(const TEEC_UUID
						*)(&destination),
						teec_open_ss.connectionMethod,
						connectionData,
						teec_open_ss.flags,
						teec_open_ss.paramTypes,
						&(teec_open_ss.params),
						&(teec_open_ss.tee_op_ntw),
						teec_open_ss.tee_op_ntw_for_cancel,
						&(teec_open_ss.returnOrigin), arg,
						teec_open_ss);
		teec_open_ss.ret = result;

		if (teec_open_ss.flags == OPERATION_EXIST) {
			if (TEEC_SUCCESS == teec_open_ss.ret) {
				param_types = teec_open_ss.paramTypes;
				for (i = 0; i < OP_PARAM_NUM; i++) {
					tmp_types =
						TEEC_PARAM_TYPE_GET
						(param_types, i);
					switch (tmp_types) {
					case TEEC_MEMREF_TEMP_INPUT:
					case TEEC_MEMREF_TEMP_OUTPUT:
					case TEEC_MEMREF_TEMP_INOUT:
					{
						if (NULL !=
							teec_open_ss.params[i].tmpref.buffer) {
							teec_open_ss.params[i].tmpref.buffer =
								save_buf_addr[i];
							if (copy_to_user((void __user *)
								(teec_open_ss.params[i].tmpref.buffer),
								tmpref_buf[i],
								teec_open_ss.params[i].tmpref.size)) {
								for (i = 0; i < OP_PARAM_NUM; i++)
									if (NULL != tmpref_buf[i])
										kfree(tmpref_buf[i]);
								return -EFAULT;
							}
						}
					}
					break;
					default:
						break;
					}
				}
			}
		}
		if (copy_to_user
			((void __user *)arg, &teec_open_ss,
			sizeof(teec_open_ss_args))) {
			for (i = 0; i < OP_PARAM_NUM; i++)
				if (NULL != tmpref_buf[i])
					kfree(tmpref_buf[i]);

			return -EFAULT;
		}
		for (i = 0; i < OP_PARAM_NUM; i++)
			if (NULL != tmpref_buf[i])
				kfree(tmpref_buf[i]);
	}
		break;
	case TEEC_CLOSE_SS:
	{
		teec_close_ss_args teec_close_ss;
		if (copy_from_user
			(&teec_close_ss, (void __user *)arg,
			sizeof(teec_close_ss_args)))
			return -EFAULT;

		OSA_ASSERT(teec_close_ss.tee_ss_ntw);

		result = _teec_close_session(teec_close_ss.tee_ss_ntw);
		teec_close_ss.ret = result;

		if (copy_to_user
			((void __user *)arg, &teec_close_ss,
			sizeof(teec_close_ss_args)))
			return -EFAULT;

	}
	break;
	case TEEC_INVOKE_CMD:
	{
		uint32_t param_types, tmp_types;
		uint32_t i;
		uint8_t *tmpref_buf[OP_PARAM_NUM];
		void *save_buf_addr[OP_PARAM_NUM];

		teec_invoke_cmd_args teec_invoke_cmd;

		if (copy_from_user(&teec_invoke_cmd,
			(void __user *)arg,
			sizeof(teec_invoke_cmd_args)))
			return -EFAULT;

		for (i = 0; i < OP_PARAM_NUM; i++) {
			tmpref_buf[i] = NULL;
			save_buf_addr[i] = NULL;
		}

		if (teec_invoke_cmd.flags == OPERATION_EXIST) {
			param_types = teec_invoke_cmd.paramTypes;
			for (i = 0; i < OP_PARAM_NUM; i++) {
				tmp_types =
					TEEC_PARAM_TYPE_GET(param_types, i);
				switch (tmp_types) {
				case TEEC_MEMREF_TEMP_INPUT:
				case TEEC_MEMREF_TEMP_OUTPUT:
				case TEEC_MEMREF_TEMP_INOUT:
				{
					if (NULL !=
						teec_invoke_cmd.params[i].tmpref.buffer) {
						tmpref_buf[i] = memdup_user((void __user *)
							(teec_invoke_cmd.params[i].tmpref.buffer),
							teec_invoke_cmd.params[i].tmpref.size);
						if (NULL == tmpref_buf[i]) {
							TZDD_DBG
								("%s(%d): Failed to allocate buffer\n",
								__func__,
								__LINE__);
							return -ENOMEM;
						}
						save_buf_addr[i] =
							teec_invoke_cmd.params[i].tmpref.buffer;
						teec_invoke_cmd.params[i].tmpref.buffer =
							tmpref_buf[i];
					}
				}
				break;
				case TEEC_MEMREF_WHOLE:
				case TEEC_MEMREF_PARTIAL_INPUT:
				case TEEC_MEMREF_PARTIAL_OUTPUT:
				case TEEC_MEMREF_PARTIAL_INOUT:
				{
					TEEC_MRVL_SharedMemory *teec_mrvl_shared_mem;
					tee_impl impl;
					if (copy_from_user(&impl,
						(void __user *)teec_invoke_cmd.params[i].memref.parent,
						sizeof(tee_impl))) {
						return -EFAULT;
					}
					save_buf_addr[i] = impl;
					teec_mrvl_shared_mem =
						(TEEC_MRVL_SharedMemory *) (save_buf_addr[i]);
					teec_invoke_cmd.params[i].memref.parent =
						container_of(teec_mrvl_shared_mem,
							TEEC_SharedMemory,
							imp);
				}
				break;
				default:
					break;
				}
			}
		}

		OSA_ASSERT(teec_invoke_cmd.tee_ss_ntw);

		result = _teec_invoke_command(teec_invoke_cmd.tee_ss_ntw,
						teec_invoke_cmd.cmd_ID,
						teec_invoke_cmd.started,
						teec_invoke_cmd.flags,
						teec_invoke_cmd.paramTypes,
						&(teec_invoke_cmd.params),
						&(teec_invoke_cmd.tee_op_ntw),
						teec_invoke_cmd.tee_op_ntw_for_cancel,
						&(teec_invoke_cmd.returnOrigin),
						arg,
						teec_invoke_cmd);
		teec_invoke_cmd.ret = result;

		if (teec_invoke_cmd.flags == OPERATION_EXIST) {
			if (TEEC_SUCCESS == teec_invoke_cmd.ret) {
				param_types =
					teec_invoke_cmd.paramTypes;
				for (i = 0; i < OP_PARAM_NUM; i++) {
					tmp_types =
						TEEC_PARAM_TYPE_GET(param_types, i);
					switch (tmp_types) {
					case TEEC_MEMREF_TEMP_INPUT:
					case TEEC_MEMREF_TEMP_OUTPUT:
					case TEEC_MEMREF_TEMP_INOUT:
					{
						if (NULL !=
							teec_invoke_cmd.params[i].tmpref.buffer) {
							teec_invoke_cmd.params[i].tmpref.buffer =
								save_buf_addr[i];
							if (copy_to_user(
								(void __user *)
								(teec_invoke_cmd.params[i].tmpref.buffer),
								tmpref_buf[i],
								teec_invoke_cmd.params[i].tmpref.size)) {
								for (i = 0; i < OP_PARAM_NUM; i++)
									if (NULL != tmpref_buf[i])
										kfree(tmpref_buf[i]);

								return -EFAULT;
							}
						}
					}
					break;
					default:
						break;
					}
				}
			}
		}

		if (copy_to_user
			((void __user *)arg, &teec_invoke_cmd,
			sizeof(teec_invoke_cmd_args))) {
			for (i = 0; i < OP_PARAM_NUM; i++)
				if (NULL != tmpref_buf[i])
					kfree(tmpref_buf[i]);

			return -EFAULT;
		}

		for (i = 0; i < OP_PARAM_NUM; i++)
			if (NULL != tmpref_buf[i])
				kfree(tmpref_buf[i]);
	}
	break;
	case TEEC_REQUEST_CANCEL:
	{
		teec_request_cancel_args teec_request_cancel;
		if (copy_from_user
			(&teec_request_cancel, (void __user *)arg,
			sizeof(teec_request_cancel_args)))
			return -EFAULT;

		OSA_ASSERT(teec_request_cancel.tee_op_ntw);

		result =
			_teec_request_cancellation(teec_request_cancel.tee_op_ntw);
		teec_request_cancel.ret = result;

		if (copy_to_user
			((void __user *)arg, &teec_request_cancel,
			sizeof(teec_request_cancel_args))) {
			return -EFAULT;
		}
	}
	break;
	default:
		TZDD_DBG("TZDD IOCTL invald command %x\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

extern osa_sem_t _g_pm_lock;
static int32_t tzdd_suspend(struct platform_device *dev, pm_message_t state)
{
	int32_t ret;
	ret = (osa_try_to_wait_for_sem(tzdd_dev->pm_lock) ? -1 : 0);
	return ret;
}

static int32_t tzdd_resume(struct platform_device *dev)
{
	osa_release_sem(tzdd_dev->pm_lock);
	return 0;
}

/* driver file ops */
static struct file_operations _tzdd_fops = {
	.open = tzdd_open,
	.release = tzdd_close,
	.unlocked_ioctl = tzdd_ioctl,
#ifdef CONFIG_64BIT
	.compat_ioctl = tzdd_compat_ioctl,
#endif
	.owner = THIS_MODULE,
};

static int32_t tzdd_cdev_init(tzdd_dev_t *dev)
{
	int32_t err, dev_nr;

	OSA_ASSERT(dev);

	if (tzdd_major) {
		dev_nr = MKDEV(tzdd_major, tzdd_minor);
		err = register_chrdev_region(dev_nr, 1, "tzdd");
	} else {
		err = alloc_chrdev_region(&dev_nr, 0, 1, "tzdd");
		tzdd_major = MAJOR(dev_nr);
		tzdd_minor = MINOR(dev_nr);
	}

	if (err < 0)
		goto _err0;

	cdev_init(&dev->cdev, &_tzdd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &_tzdd_fops;

	err = cdev_add(&dev->cdev, dev_nr, 1);
	/* Fail gracefully if need be */
	if (err)
		goto _err1;

	dev->_tzdd_class = class_create(THIS_MODULE, "tzdd");
	if (IS_ERR(dev->_tzdd_class))
		goto _err2;
	device_create(dev->_tzdd_class, NULL,
			MKDEV(tzdd_major, tzdd_minor), NULL, "tzdd");

	return 0;

_err2:
	cdev_del(&dev->cdev);
_err1:
	unregister_chrdev_region(MKDEV(tzdd_major, tzdd_minor), 1);
_err0:
	return -1;
}

static void tzdd_cdev_cleanup(tzdd_dev_t *dev)
{
	OSA_ASSERT(dev);

	device_destroy(dev->_tzdd_class, MKDEV(tzdd_major, tzdd_minor));
	class_destroy(dev->_tzdd_class);
	cdev_del(&dev->cdev);
	unregister_chrdev_region(MKDEV(tzdd_major, tzdd_minor), 1);

	return;
}

static int tzdd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	const void *qos;
	unsigned int ret = 0;

	dev_info(dev, "%s\n", TZDD_VERSION);

	/* allocate driver struct */
	tzdd_dev = devm_kzalloc(dev, sizeof(tzdd_dev_t), GFP_KERNEL);
	if (NULL == tzdd_dev) {
		dev_err(dev, "Failed to allocate buffer\n");
		return -ENOMEM;
	}
	memset(tzdd_dev, 0, sizeof(tzdd_dev_t));

	ret = tzdd_cdev_init(tzdd_dev);
	if (ret < 0) {
		dev_err(dev, "Failed to init cdev\n");
		goto _fail_in_adding_cdev;
	}

	ret = tzdd_init_core(tzdd_dev);
	if (ret) {
		dev_err(dev, "failed to init core\n");
		goto _fail_in_init_core;
	}

	osa_list_init_head(&(tzdd_dev->pid_list));
	tzdd_dev->pid_mutex = osa_create_sem(1);

	qos = of_get_property(np, "lpm-qos", NULL);
	if (!qos) {
		dev_err(dev, "failed to get property qos\n");
		goto _fail_in_init_core;
	}
	tzdd_dev->tzdd_lpm = be32_to_cpup(qos);

	return 0;

_fail_in_init_core:
	tzdd_cdev_cleanup(tzdd_dev);
_fail_in_adding_cdev:

	return -EBUSY;
}

static int tzdd_remove(struct platform_device *pdev)
{
	TZDD_DBG("tzdd cleanup\n");

	osa_destroy_sem(tzdd_dev->pid_mutex);
	tzdd_cleanup_core(tzdd_dev);
	tzdd_cdev_cleanup(tzdd_dev);

	return 0;
}

static const struct of_device_id _tzdd_dt_match[] = {
	{ .compatible = "pxa-tzdd", .data = NULL },
	{},
};

static struct platform_driver _tzdd_drv = {
	.driver = {
		.name = TZDD_DRV_NAME,
		.of_match_table = of_match_ptr(_tzdd_dt_match),
	},
	.probe = tzdd_probe,
	.remove = tzdd_remove,
	.suspend = tzdd_suspend,
	.resume = tzdd_resume,
};

module_platform_driver(_tzdd_drv);

