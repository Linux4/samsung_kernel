/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include <tzdev/tee_client_api.h>

#include "tz_tee_kernel_api_test.h"
#include "core/cdev.h"
#include "core/subsystem.h"
#include "core/wait.h"

#include "teec/misc.h"
#include "teec/tests.h"

#define CONTEXT_NAME_LEN_MAX	128
#define ARGV_NAME_LEN_MAX	128
#define INVALID_PTR		((void *) -1L)

static struct workqueue_struct *tz_tee_kernel_api_workq;

struct tz_tee_kernel_api_cache {
	struct idr idr_context;
	struct idr idr_memory;
	struct idr idr_session;
	struct mutex mtx_context;
	struct mutex mtx_memory;
	struct mutex mtx_session;
};

struct tz_tee_kernel_api_work {
	struct work_struct work;
	wait_queue_head_t wq;
	unsigned int done;
	unsigned int test_id;
	uint32_t result;
	uint32_t origin;
	int ret;
	struct test_data *argv;
};

static void *uint64_to_ptr(uint64_t val)
{
	return (void *)(unsigned long)val;
}

static uint64_t ptr_to_uint64(void *ptr)
{
	return (uint64_t)(unsigned long)ptr;
}

static void tz_tee_kernel_api_workq_func(struct work_struct *work)
{
	struct tz_tee_kernel_api_work *kapi_work =
			container_of(work, struct tz_tee_kernel_api_work, work);

	switch (kapi_work->test_id) {
	case TZ_TEE_KERNEL_API_TEE_TEST:
		kapi_work->result = tz_tee_test(&kapi_work->origin);
		break;
	case TZ_TEE_KERNEL_API_TEE_TEST_SEND_TA:
		kapi_work->result = tz_tee_test_send_ta(&kapi_work->origin, kapi_work->argv);
		break;
	case TZ_TEE_KERNEL_API_TEE_TEST_CANCELLATION:
		kapi_work->result = tz_tee_test_cancellation(&kapi_work->origin);
		break;
	case TZ_TEE_KERNEL_API_TEE_TEST_NULL_ARGS:
		kapi_work->result = tz_tee_test_null_args(&kapi_work->origin);
		break;
	case TZ_TEE_KERNEL_API_TEE_TEST_SHARED_REF:
		kapi_work->result = tz_tee_test_shared_ref(&kapi_work->origin);
		break;
	case TZ_TEE_KERNEL_API_TEE_TEST_SLEEP:
		kapi_work->result = tz_tee_test_sleep(&kapi_work->origin, kapi_work->argv);
		break;
	default:
		kapi_work->ret = -ENOENT;
		break;
	}

	kapi_work->done = 1;
	wake_up(&kapi_work->wq);
}

static int tz_tee_kernel_api_run_kernel_test(unsigned long arg)
{
	struct tz_tee_kernel_api_work kapi_work;
	struct tz_tee_kernel_api_cmd cmd;
	struct test_data data;
	void *pdata = NULL;

	if (copy_from_user(&cmd, (void *)arg, sizeof(struct tz_tee_kernel_api_cmd))) {
		return -EFAULT;
	}

	INIT_WORK_ONSTACK(&kapi_work.work, tz_tee_kernel_api_workq_func);
	init_waitqueue_head(&kapi_work.wq);
	kapi_work.done = 0;
	kapi_work.test_id = cmd.test_case.test_id;
	kapi_work.result = TEEC_ERROR_GENERIC;
	kapi_work.origin = TEEC_ORIGIN_API;
	kapi_work.ret = 0;

	if (cmd.test_case.address) {
		pdata = vmalloc(cmd.test_case.size);
		if (pdata == NULL) {
			return -ENOMEM;
		}

		if (copy_from_user(pdata, uint64_to_ptr(cmd.test_case.address), cmd.test_case.size)) {
			vfree(pdata);
			return -EFAULT;
		}

		data.size = cmd.test_case.size;
		data.address = ptr_to_uint64(pdata);

		kapi_work.argv = &data;
	}

	BUG_ON(!queue_work(tz_tee_kernel_api_workq, &kapi_work.work));

	wait_event_uninterruptible_freezable_nested(kapi_work.wq, kapi_work.done == 1);

	cmd.result = kapi_work.result;
	cmd.origin = kapi_work.origin;

	if (copy_to_user((void *)arg, &cmd, sizeof(struct tz_tee_kernel_api_cmd))) {
		vfree(pdata);
		return -EFAULT;
	}

	vfree(pdata);

	return kapi_work.ret;
}

static int tz_tee_kernel_api_initialize_context(struct tz_tee_kernel_api_cache *cache,
		unsigned long arg)
{
	TEEC_Context *context;
	struct tz_tee_kernel_api_cmd cmd;
	char name_buf[CONTEXT_NAME_LEN_MAX];
	char *name = NULL;
	int ret = 0;

	if (copy_from_user(&cmd, (void *)arg, sizeof(struct tz_tee_kernel_api_cmd)))
		return -EFAULT;

	if (cmd.initialize_context.name_address) {
		if (strncpy_from_user(name_buf,
				uint64_to_ptr(cmd.initialize_context.name_address),
				CONTEXT_NAME_LEN_MAX) < 0)
			return -EFAULT;

		name = name_buf;
	}

	context = kzalloc(sizeof(TEEC_Context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;

	memset(&cmd, 0x0, sizeof(struct tz_tee_kernel_api_cmd));

	cmd.result = TEEC_InitializeContext(name, context);

	if (copy_to_user((void *)arg, &cmd, sizeof(struct tz_tee_kernel_api_cmd))) {
		ret = -EFAULT;
		if (cmd.result == TEEC_SUCCESS)
			goto finalize_context;
		else
			goto free_context;
	}

	if (cmd.result != TEEC_SUCCESS)
		goto free_context;

	mutex_lock(&cache->mtx_context);
	ret = idr_alloc(&cache->idr_context, context, 1, 0, GFP_KERNEL);
	mutex_unlock(&cache->mtx_context);

	if (ret < 0)
		goto finalize_context;

	return ret;

finalize_context:
	TEEC_FinalizeContext(context);
free_context:
	kfree(context);

	return ret;
}

static int tz_tee_kernel_api_finalize_context(struct tz_tee_kernel_api_cache *cache,
		unsigned long arg)
{
	TEEC_Context *context;

	mutex_lock(&cache->mtx_context);
	context = idr_find(&cache->idr_context, arg);
	if (!context) {
		mutex_unlock(&cache->mtx_context);
		return -ENOENT;
	}

	idr_remove(&cache->idr_context, arg);
	mutex_unlock(&cache->mtx_context);

	TEEC_FinalizeContext(context);

	kfree(context);

	return 0;
}

static int tz_tee_kernel_api_allocate_shared_memory(struct tz_tee_kernel_api_cache *cache,
		unsigned long arg)
{
	TEEC_Context *context;
	TEEC_SharedMemory *shmem;
	struct tz_tee_kernel_api_cmd cmd;
	int ret = 0;

	if (copy_from_user(&cmd, (void *)arg, sizeof(struct tz_tee_kernel_api_cmd)))
		return -EFAULT;

	mutex_lock(&cache->mtx_context);
	context = idr_find(&cache->idr_context, cmd.create_memory.context_id);
	mutex_unlock(&cache->mtx_context);

	if (!context)
		return -ENOENT;

	shmem = kzalloc(sizeof(TEEC_SharedMemory), GFP_KERNEL);
	if (!shmem)
		return -ENOMEM;

	shmem->size = cmd.create_memory.size;
	shmem->flags = cmd.create_memory.flags;

	memset(&cmd, 0x0, sizeof(struct tz_tee_kernel_api_cmd));

	cmd.result = TEEC_AllocateSharedMemory(context, shmem);
	cmd.create_memory.address = ptr_to_uint64(shmem->buffer);
	cmd.create_memory.size = shmem->size;

	if (copy_to_user((void *)arg, &cmd, sizeof(struct tz_tee_kernel_api_cmd))) {
		ret = -EFAULT;
		if (cmd.result == TEEC_SUCCESS)
			goto release_shared_memory;
		else
			goto free_shared_memory;
	}

	if (cmd.result != TEEC_SUCCESS)
		goto free_shared_memory;

	mutex_lock(&cache->mtx_memory);
	ret = idr_alloc(&cache->idr_memory, shmem, 1, 0, GFP_KERNEL);
	mutex_unlock(&cache->mtx_memory);

	if (ret < 0)
		goto release_shared_memory;

	return ret;

release_shared_memory:
	TEEC_ReleaseSharedMemory(shmem);
free_shared_memory:
	kfree(shmem);

	return ret;
}

static int tz_tee_kernel_api_register_shared_memory(struct tz_tee_kernel_api_cache *cache,
		unsigned long arg)
{
	TEEC_Context *context;
	TEEC_SharedMemory *shmem;
	struct tz_tee_kernel_api_cmd cmd;
	int ret = 0;

	if (copy_from_user(&cmd, (void *)arg, sizeof(struct tz_tee_kernel_api_cmd)))
		return -EFAULT;

	mutex_lock(&cache->mtx_context);
	context = idr_find(&cache->idr_context, cmd.create_memory.context_id);
	mutex_unlock(&cache->mtx_context);

	if (!context)
		return -ENOENT;

	shmem = kzalloc(sizeof(TEEC_SharedMemory), GFP_KERNEL);
	if (!shmem)
		return -ENOMEM;

	shmem->buffer = NULL;
	shmem->size = cmd.create_memory.size;
	shmem->flags = cmd.create_memory.flags;

	if (shmem->size > 0 && shmem->size <= TEEC_CONFIG_SHAREDMEM_MAX_SIZE) {
		shmem->buffer = vmalloc(shmem->size);
		if (!shmem->buffer) {
			ret = -ENOMEM;
			goto free_shared_memory;
		}
	}

	memset(&cmd, 0x0, sizeof(struct tz_tee_kernel_api_cmd));

	cmd.result = TEEC_RegisterSharedMemory(context, shmem);

	if (copy_to_user((void *)arg, &cmd, sizeof(struct tz_tee_kernel_api_cmd))) {
		ret = -EFAULT;
		if (cmd.result == TEEC_SUCCESS)
			goto release_shared_memory;
		else
			goto free_buffer;
	}

	if (cmd.result != TEEC_SUCCESS)
		goto free_buffer;

	mutex_lock(&cache->mtx_memory);
	ret = idr_alloc(&cache->idr_memory, shmem, 1,0, GFP_KERNEL);
	mutex_unlock(&cache->mtx_memory);

	if (ret < 0)
		goto release_shared_memory;

	return ret;

release_shared_memory:
	TEEC_ReleaseSharedMemory(shmem);
free_buffer:
	if (shmem->buffer)
		vfree(shmem->buffer);
free_shared_memory:
	kfree(shmem);

	return ret;
}

static int tz_tee_kernel_api_release_shared_memory(struct tz_tee_kernel_api_cache *cache,
		unsigned long arg)
{
	struct tz_tee_kernel_api_cmd cmd;
	TEEC_SharedMemory *shmem;

	if (copy_from_user(&cmd, (void *)arg, sizeof(struct tz_tee_kernel_api_cmd)))
		return -EFAULT;

	mutex_lock(&cache->mtx_memory);
	shmem = idr_find(&cache->idr_memory, cmd.release_memory.shmem_id);
	if (!shmem) {
		mutex_unlock(&cache->mtx_memory);
		return -ENOENT;
	}

	idr_remove(&cache->idr_memory, cmd.release_memory.shmem_id);
	mutex_unlock(&cache->mtx_memory);

	TEEC_ReleaseSharedMemory(shmem);

	memset(&cmd, 0x0, sizeof(struct tz_tee_kernel_api_cmd));

	cmd.release_memory.ret_address = ptr_to_uint64(shmem->buffer);
	cmd.release_memory.ret_size = shmem->size;

	vfree(shmem->buffer);
	kfree(shmem);

	if (copy_to_user((void *)arg, &cmd, sizeof(struct tz_tee_kernel_api_cmd)))
		return -EFAULT;

	return 0;
}

static int tz_tee_kernel_api_init_operation(struct tz_tee_kernel_api_cache *cache,
		struct tz_tee_kernel_api_operation *op, TEEC_Operation *operation,
		void *tmp_buf[4])
{
	TEEC_SharedMemory *shmem;
	unsigned int i;
	unsigned int type;
	void *to;
	void *from;
	uint64_t offset;
	uint64_t size;
	int ret;

	memset(operation, 0x0, sizeof(TEEC_Operation));
	memset(tmp_buf, 0x0, 4 * sizeof(void *));

	operation->paramTypes = op->param_types;

	for (i = 0; i < 4; ++i) {
		type = PARAM_TYPE_GET(operation->paramTypes, i);

		switch (type) {
		case TEEC_NONE:
			break;
		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			operation->params[i].value.a = op->params[i].value.a;
			operation->params[i].value.b = op->params[i].value.b;
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			from = uint64_to_ptr(op->params[i].tmpref.address);
			size = op->params[i].tmpref.size;

			operation->params[i].tmpref.size = size;

			if (!from) {
				operation->params[i].tmpref.buffer = NULL;
				break;
			}

			if (!size || size > TEEC_CONFIG_SHAREDMEM_MAX_SIZE) {
				operation->params[i].tmpref.buffer = INVALID_PTR;
				break;
			}

			tmp_buf[i] = vmalloc(size);
			if (!tmp_buf[i]) {
				ret = -ENOMEM;
				goto cleanup;
			}

			if (copy_from_user(tmp_buf[i], from, size)) {
				ret = -EFAULT;
				goto cleanup;
			}

			operation->params[i].tmpref.buffer = tmp_buf[i];
			break;
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
		case TEEC_MEMREF_WHOLE:
			mutex_lock(&cache->mtx_memory);
			shmem = idr_find(&cache->idr_memory, op->params[i].memref.shmem_id);
			mutex_unlock(&cache->mtx_memory);

			if (!shmem) {
				ret = -ENOENT;
				goto cleanup;
			}

			/* Parameters passed directly to kernel TEE implementation
			 * to be sure sanity checks works as expected. */
			operation->params[i].memref.parent = shmem;
			operation->params[i].memref.offset = op->params[i].memref.offset;
			operation->params[i].memref.size = op->params[i].memref.size;

			/* But in case of whole memory reference whole buffer should be
			 * copied from user-space, so offset and size calculated here
			 * may differ from operation->params[i].memref.* fields. */
			if (type == TEEC_MEMREF_WHOLE) {
				offset = 0;
				size = shmem->size;
			} else {
				offset = op->params[i].memref.offset;
				size = op->params[i].memref.size;
			}

			to = shmem->buffer;
			from = uint64_to_ptr(op->params[i].memref.address);

			if (!from || !size || size > TEEC_CONFIG_SHAREDMEM_MAX_SIZE)
				break;

			if (copy_from_user(to + offset, from + offset, size)) {
				ret = -EFAULT;
				goto cleanup;
			}

			break;
		default:
			break;
		}
	}

	return 0;

cleanup:
	for (i = 0; i < 4; ++i)
		if (tmp_buf[i])
			vfree(tmp_buf[i]);

	return ret;
}

static int tz_tee_kernel_api_fini_operation(struct tz_tee_kernel_api_cache *cache,
		struct tz_tee_kernel_api_operation *op, TEEC_Operation *operation,
		void *tmp_buf[4])
{
	TEEC_SharedMemory *shmem;
	unsigned int type;
	unsigned int i;
	void *to;
	void *from;
	uint64_t offset;
	uint64_t size;
	int ret = 0;

	op->param_types = operation->paramTypes;

	for (i = 0; i < 4; ++i) {
		type = PARAM_TYPE_GET(operation->paramTypes, i);

		switch (type) {
		case TEEC_NONE:
		case TEEC_VALUE_INPUT:
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_PARTIAL_INPUT:
			break;
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			op->params[i].value.a = operation->params[i].value.a;
			op->params[i].value.b = operation->params[i].value.b;
			break;
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			to = uint64_to_ptr(op->params[i].tmpref.address);
			from = tmp_buf[i];
			size = op->params[i].tmpref.size;

			if (to && from && size && size <= TEEC_CONFIG_SHAREDMEM_MAX_SIZE)
				if (copy_to_user(to, from, size)) {
					ret = -EFAULT;
					goto cleanup;
				}

			op->params[i].tmpref.size = operation->params[i].tmpref.size;
			break;
		case TEEC_MEMREF_WHOLE:
			if (!(operation->params[i].memref.parent->flags & TEEC_MEM_OUTPUT))
				break;
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			mutex_lock(&cache->mtx_memory);
			shmem = idr_find(&cache->idr_memory, op->params[i].memref.shmem_id);
			mutex_unlock(&cache->mtx_memory);

			if (!shmem) {
				ret = -ENOENT;
				goto cleanup;
			}

			if (type == TEEC_MEMREF_WHOLE) {
				offset = 0;
				size = shmem->size;
			} else {
				offset = op->params[i].memref.offset;
				size = op->params[i].memref.size;
			}

			to = uint64_to_ptr(op->params[i].memref.address);
			from = shmem->buffer;

			if (to && size && size <= TEEC_CONFIG_SHAREDMEM_MAX_SIZE)
				if (copy_to_user(to + offset, from + offset, size)) {
					ret = -EFAULT;
					goto cleanup;
				}

			op->params[i].memref.size = operation->params[i].memref.size;
			break;
		default:
			break;
		}
	}

cleanup:
	for (i = 0; i < 4; ++i)
		if (tmp_buf[i])
			vfree(tmp_buf[i]);

	return ret;
}

static unsigned long tz_tee_kernel_api_conn_data_len(unsigned int conn_method)
{
	switch(conn_method) {
	case TEEC_LOGIN_PUBLIC:
	case TEEC_LOGIN_USER:
	case TEEC_LOGIN_APPLICATION:
	case TEEC_LOGIN_USER_APPLICATION:
		return 0;
	case TEEC_LOGIN_GROUP:
	case TEEC_LOGIN_GROUP_APPLICATION:
		return sizeof(uint32_t);
	default:
		return 0;
	}
}

static int tz_tee_kernel_api_open_session(struct tz_tee_kernel_api_cache *cache,
		unsigned long arg)
{
	TEEC_Context *context;
	TEEC_Session *session;
	TEEC_UUID uuid;
	TEEC_Operation operation;
	TEEC_Operation *operation_ptr = NULL;
	struct tz_tee_kernel_api_cmd cmd;
	char conn_data_buf[PROFILE_STR_LEN];
	unsigned long conn_data_len;
	char *conn_data = NULL;
	void *tmp_buf[4];
	unsigned int i;
	int ret = 0;

	if (copy_from_user(&cmd, (void *)arg, sizeof(struct tz_tee_kernel_api_cmd)))
		return -EFAULT;

	if (cmd.open_session.conn_data_address) {
		memset(conn_data_buf, 0x0, PROFILE_STR_LEN);

		conn_data_len = tz_tee_kernel_api_conn_data_len(cmd.open_session.conn_method);

		if (conn_data_len && copy_from_user(conn_data_buf,
				uint64_to_ptr(cmd.open_session.conn_data_address),
				conn_data_len))
			return -EFAULT;

		conn_data = conn_data_buf;
	}

	if (cmd.open_session.op.exists)
		operation_ptr = &operation;

	mutex_lock(&cache->mtx_context);
	context = idr_find(&cache->idr_context, cmd.open_session.context_id);
	mutex_unlock(&cache->mtx_context);

	if (!context)
		return -ENOENT;

	session = kzalloc(sizeof(TEEC_Session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;

	uuid.timeLow = cmd.open_session.uuid.time_low;
	uuid.timeMid = cmd.open_session.uuid.time_mid;
	uuid.timeHiAndVersion = cmd.open_session.uuid.time_hi_and_version;
	for (i = 0; i < 8; ++i)
		uuid.clockSeqAndNode[i] = cmd.open_session.uuid.clock_seq_and_node[i];

	if (operation_ptr) {
		ret = tz_tee_kernel_api_init_operation(cache, &cmd.open_session.op,
				operation_ptr, tmp_buf);
		if (ret)
			goto free_session;
	}

	cmd.result = TEEC_OpenSession(context, session, &uuid,
			cmd.open_session.conn_method, conn_data,
			operation_ptr, &cmd.origin);

	if (operation_ptr) {
		ret = tz_tee_kernel_api_fini_operation(cache, &cmd.open_session.op,
				operation_ptr, tmp_buf);
		if (ret) {
			if (cmd.result == TEEC_SUCCESS)
				goto close_session;
			else
				goto free_session;
		}
	}

	if (copy_to_user((void *)arg, &cmd, sizeof(struct tz_tee_kernel_api_cmd))) {
		ret = -EFAULT;
		if (cmd.result == TEEC_SUCCESS)
			goto close_session;
		else
			goto free_session;
	}

	if (cmd.result != TEEC_SUCCESS)
		goto free_session;

	mutex_lock(&cache->mtx_session);
	ret = idr_alloc(&cache->idr_session, session, 1, 0, GFP_KERNEL);
	mutex_unlock(&cache->mtx_session);

	if (ret < 0)
		goto close_session;

	return ret;

close_session:
	TEEC_CloseSession(session);
free_session:
	kfree(session);

	return ret;
}

static int tz_tee_kernel_api_close_session(struct tz_tee_kernel_api_cache *cache,
		unsigned long arg)
{
	TEEC_Session *session;

	mutex_lock(&cache->mtx_session);
	session = idr_find(&cache->idr_session, arg);
	if (!session) {
		mutex_unlock(&cache->mtx_session);
		return -ENOENT;
	}

	idr_remove(&cache->idr_session, arg);
	mutex_unlock(&cache->mtx_session);

	TEEC_CloseSession(session);

	kfree(session);

	return 0;
}

static int tz_tee_kernel_api_invoke_command(struct tz_tee_kernel_api_cache *cache,
		unsigned long arg)
{
	TEEC_Session *session;
	TEEC_Operation operation;
	TEEC_Operation *operation_ptr = NULL;
	struct tz_tee_kernel_api_cmd cmd;
	void *tmp_buf[4];
	int ret;

	if (copy_from_user(&cmd, (void *)arg, sizeof(struct tz_tee_kernel_api_cmd)))
		return -EFAULT;

	if (cmd.invoke_command.op.exists)
		operation_ptr = &operation;

	mutex_lock(&cache->mtx_session);
	session = idr_find(&cache->idr_session, cmd.invoke_command.session_id);
	mutex_unlock(&cache->mtx_session);

	if (!session)
		return -ENOENT;

	if (operation_ptr) {
		ret = tz_tee_kernel_api_init_operation(cache, &cmd.invoke_command.op,
				&operation, tmp_buf);
		if (ret)
			return ret;
	}

	cmd.result = TEEC_InvokeCommand(session, cmd.invoke_command.command_id,
			operation_ptr, &cmd.origin);

	if (operation_ptr) {
		ret = tz_tee_kernel_api_fini_operation(cache, &cmd.invoke_command.op,
				&operation, tmp_buf);
		if (ret)
			return ret;
	}

	if (copy_to_user((void *)arg, &cmd, sizeof(struct tz_tee_kernel_api_cmd)))
		return -EFAULT;

	return 0;
}

static int tz_tee_kernel_api_open(struct inode *inode, struct file *file)
{
	struct tz_tee_kernel_api_cache *cache;

	(void)inode;

	cache = kmalloc(sizeof(struct tz_tee_kernel_api_cache), GFP_KERNEL);
	if (!cache)
		return -ENOMEM;

	idr_init(&cache->idr_context);
	idr_init(&cache->idr_memory);
	idr_init(&cache->idr_session);

	mutex_init(&cache->mtx_context);
	mutex_init(&cache->mtx_memory);
	mutex_init(&cache->mtx_session);

	file->private_data = cache;

	return 0;
}

static int tz_tee_kernel_api_release(struct inode *inode, struct file *file)
{
	struct tz_tee_kernel_api_cache *cache = file->private_data;
	TEEC_Context *context;
	TEEC_Session *session;
	TEEC_SharedMemory *shmem;
	unsigned int id;

	(void)inode;

	idr_for_each_entry(&cache->idr_session, session, id) {
		TEEC_CloseSession(session);
		kfree(session);
	}

	idr_for_each_entry(&cache->idr_memory, shmem, id) {
		TEEC_ReleaseSharedMemory(shmem);
		vfree(shmem->buffer);
		kfree(shmem);
	}

	idr_for_each_entry(&cache->idr_context, context, id) {
		TEEC_FinalizeContext(context);
		kfree(context);
	}

	idr_destroy(&cache->idr_context);
	idr_destroy(&cache->idr_memory);
	idr_destroy(&cache->idr_session);

	mutex_destroy(&cache->mtx_context);
	mutex_destroy(&cache->mtx_memory);
	mutex_destroy(&cache->mtx_session);

	kfree(cache);

	return 0;
}

static long tz_tee_kernel_api_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tz_tee_kernel_api_cache *cache = file->private_data;

	switch (cmd) {
	case TZ_TEE_KERNEL_API_RUN_KERNEL_TEST:
		return tz_tee_kernel_api_run_kernel_test(arg);
	case TZ_TEE_KERNEL_API_INITIALIZE_CONTEXT:
		return tz_tee_kernel_api_initialize_context(cache, arg);
	case TZ_TEE_KERNEL_API_FINALIZE_CONTEXT:
		return tz_tee_kernel_api_finalize_context(cache, arg);
	case TZ_TEE_KERNEL_API_ALLOCATE_SHARED_MEMORY:
		return tz_tee_kernel_api_allocate_shared_memory(cache, arg);
	case TZ_TEE_KERNEL_API_REGISTER_SHARED_MEMORY:
		return tz_tee_kernel_api_register_shared_memory(cache, arg);
	case TZ_TEE_KERNEL_API_RELEASE_SHARED_MEMORY:
		return tz_tee_kernel_api_release_shared_memory(cache, arg);
	case TZ_TEE_KERNEL_API_OPEN_SESSION:
		return tz_tee_kernel_api_open_session(cache, arg);
	case TZ_TEE_KERNEL_API_CLOSE_SESSION:
		return tz_tee_kernel_api_close_session(cache, arg);
	case TZ_TEE_KERNEL_API_INVOKE_COMMAND:
		return tz_tee_kernel_api_invoke_command(cache, arg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations tz_tee_kernel_api_fops = {
	.owner = THIS_MODULE,
	.open = tz_tee_kernel_api_open,
	.release = tz_tee_kernel_api_release,
	.unlocked_ioctl = tz_tee_kernel_api_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_tee_kernel_api_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct tz_cdev tz_tee_kernel_api_cdev = {
	.name = "tz_tee_kernel_api_test",
	.fops = &tz_tee_kernel_api_fops,
	.owner = THIS_MODULE,
};

int tz_tee_kernel_api_init(void)
{
	int ret;

	tz_tee_kernel_api_workq = alloc_workqueue("tz_tee_kernel_api",
			WQ_UNBOUND | WQ_MEM_RECLAIM, WQ_DFL_ACTIVE);
	if (!tz_tee_kernel_api_workq) {
		pr_err("failed to alloc workqueue\n");
		return -ENOMEM;
	}

	ret = tz_cdev_register(&tz_tee_kernel_api_cdev);
	if (ret) {
		pr_err("failed to register device, ret=%d\n", ret);
		goto destroy_workq;
	}

	return 0;

destroy_workq:
	destroy_workqueue(tz_tee_kernel_api_workq);

	return ret;
}

void tz_tee_kernel_api_exit(void)
{
	tz_cdev_unregister(&tz_tee_kernel_api_cdev);

	flush_workqueue(tz_tee_kernel_api_workq);
	destroy_workqueue(tz_tee_kernel_api_workq);
}

tzdev_initcall(tz_tee_kernel_api_init);
tzdev_exitcall(tz_tee_kernel_api_exit);
