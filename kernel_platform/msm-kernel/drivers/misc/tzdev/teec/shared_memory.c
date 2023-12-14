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

#include <linux/export.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/page.h>

#include <tzdev/tee_client_api.h>

#include "iw_messages.h"
#include "misc.h"
#include "types.h"
#include "core/log.h"
#include "core/wait.h"
#include "core/iw_shmem.h"

#define PTR_ALIGN_PGDN(p)	((typeof(p))(((uintptr_t)(p)) & PAGE_MASK))
#define OFFSET_IN_PAGE(x)	((x) & (~PAGE_MASK))

static uint32_t tzdev_teec_shared_memory_init(TEEC_Context *context,
		TEEC_SharedMemory *sharedMem)
{
	struct tzdev_teec_shared_memory *shm;
	uint32_t result;
	void *addr;
	size_t size;
	unsigned int offset;
	unsigned int flags = TZDEV_IW_SHMEM_FLAG_SYNC
			| (sharedMem->flags & TEEC_MEM_OUTPUT ? TZDEV_IW_SHMEM_FLAG_WRITE : 0);
	int ret;

	log_debug(tzdev_teec, "Enter, context = %pK sharedMem = %pK buffer = %pK size = %zu flags = %x\n",
			context, sharedMem, sharedMem->buffer, sharedMem->size, sharedMem->flags);

	shm = kmalloc(sizeof(struct tzdev_teec_shared_memory), GFP_KERNEL);
	if (!shm) {
		log_error(tzdev_teec, "Failed to allocate shared memory struct\n");
		result = TEEC_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	addr = PTR_ALIGN_PGDN(sharedMem->buffer);
	offset = OFFSET_IN_PAGE((uintptr_t)sharedMem->buffer);
	size = PAGE_ALIGN(sharedMem->size + offset);

	shm->is_allocated = !sharedMem->buffer;

	if (shm->is_allocated)
		/* According to the TEE Client API Specification the size is allowed to be zero. In this case
		 * sharedMem->buffer MUST not be NULL. So we register 1 byte shared memory and write its
		 * pointer to the buffer. */
		ret = tzdev_iw_shmem_register(&sharedMem->buffer, size ? size : 1, flags);
	else
		ret = tzdev_iw_shmem_register_exist(addr, size, flags);
	if (ret < 0) {
		log_error(tzdev_teec, "Failed to register shared memory, ret = %d\n", ret);
		result = tzdev_teec_error_to_tee_error(ret);
		kfree(shm);
		goto out;
	}

	shm->id = ret;
	shm->context = context;
	shm->offset = offset;

	sharedMem->imp = shm;

	result = TEEC_SUCCESS;

out:
	log_debug(tzdev_teec, "Exit, context = %pK sharedMem = %pK buffer = %pK size = %zu flags = %x\n",
			context, sharedMem, sharedMem->buffer, sharedMem->size, sharedMem->flags);

	return result;
}

static void tzdev_teec_shared_memory_fini(TEEC_SharedMemory *sharedMem)
{
	struct tzdev_teec_shared_memory *shm = sharedMem->imp;

	log_debug(tzdev_teec, "Enter, sharedMem = %pK id = %u\n", sharedMem, shm->id);

	sharedMem->imp = NULL;

	if (shm->id)
		BUG_ON(tzdev_iw_shmem_release(shm->id));

	kfree(shm);

	log_debug(tzdev_teec, "Exit, sharedMem = %pK\n", sharedMem);
}

static uint32_t tzdev_teec_grant_shared_memory_rights(TEEC_SharedMemory *sharedMem, uint32_t *origin)
{
	struct tzdev_teec_shared_memory *shm = sharedMem->imp;
	struct tzdev_teec_context *ctx = shm->context->imp;
	struct cmd_set_shared_memory_rights cmd;
	struct cmd_reply_set_shared_memory_rights ack;
	uint32_t result;
	int ret;

	log_debug(tzdev_teec, "Enter, sharedMem = %pK\n", sharedMem);

	*origin = TEEC_ORIGIN_API;

	cmd.base.cmd = CMD_SET_SHMEM_RIGHTS;
	cmd.base.serial = ctx->serial;
	cmd.buf_desc.offset = shm->offset;
	cmd.buf_desc.size = sharedMem->size;
	cmd.buf_desc.id = shm->id;

	ret = tzdev_teec_send_then_recv(ctx->socket,
			&cmd, sizeof(cmd), 0x0,
			&ack, sizeof(ack), 0x0,
			&result, origin);
	if (ret < 0) {
		log_error(tzdev_teec, "Failed to xmit set shmem rights, ret = %d\n", ret);
		goto out;
	}

	ret = tzdev_teec_check_reply(&ack.base, CMD_REPLY_SET_SHMEM_RIGHTS,
			ctx->serial, &result, origin);
	if (ret) {
		log_error(tzdev_teec, "Failed to check set shmem rights reply, ret = %d\n", ret);
		goto out;
	}

	result = ack.base.result;
	*origin = ack.base.origin;

out:
	ctx->serial++;

	tzdev_teec_fixup_origin(result, origin);

	log_debug(tzdev_teec, "Exit, sharedMem = %pK\n", sharedMem);

	return result;
}

static uint32_t tzdev_teec_shared_memory_check_args(TEEC_Context *context,
		TEEC_SharedMemory *sharedMem, bool null_buffer)
{
	if (!sharedMem) {
		log_error(tzdev_teec, "Null shared memory passed\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (null_buffer)
		sharedMem->buffer = NULL;

	if (!context) {
		log_error(tzdev_teec, "Null context passed\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (sharedMem->size > TEEC_CONFIG_SHAREDMEM_MAX_SIZE) {
		log_error(tzdev_teec, "Too big shared memory requested, size = %zu max = %u\n",
				sharedMem->size, TEEC_CONFIG_SHAREDMEM_MAX_SIZE);
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (sharedMem->flags & ~(TEEC_MEM_INPUT | TEEC_MEM_OUTPUT)) {
		log_error(tzdev_teec, "Invalid flags passed, flags = %x\n", sharedMem->flags);
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (!sharedMem->buffer && !null_buffer) {
		log_error(tzdev_teec, "No buffer provided for shared memory\n");
		return TEEC_ERROR_NO_DATA;
	}

	return TEEC_SUCCESS;
}

static uint32_t tzdev_teec_register_shared_memory(TEEC_Context *context, TEEC_SharedMemory *sharedMem,
		bool null_buffer)
{
	struct tzdev_teec_context *ctx;
	struct tzdev_teec_shared_memory *shm;
	uint32_t origin;
	uint32_t result;

	log_debug(tzdev_teec, "Enter, context = %pK sharedMem = %pK\n", context, sharedMem);

	origin = TEEC_ORIGIN_API;

	result = tzdev_teec_shared_memory_check_args(context, sharedMem, null_buffer);
	if (result != TEEC_SUCCESS)
		goto out;

	ctx = context->imp;

	result = tzdev_teec_shared_memory_init(context, sharedMem);
	if (result != TEEC_SUCCESS) {
		log_error(tzdev_teec, "Failed to create shared memory, context = %pK sharedMem = %pK\n",
				context, sharedMem);
		goto out;
	}

	mutex_lock(&ctx->mutex);
	result = tzdev_teec_grant_shared_memory_rights(sharedMem, &origin);
	mutex_unlock(&ctx->mutex);

	if (result != TEEC_SUCCESS) {
		log_error(tzdev_teec, "Failed to grant shared memory rights, context = %pK sharedMem = %pK\n",
				context, sharedMem);
		tzdev_teec_shared_memory_fini(sharedMem);
		goto out;
	}

	shm = sharedMem->imp;

	log_debug(tzdev_teec, "Success, context = %pK sharedMem = %pK id = %u\n",
		  context, sharedMem, shm->id);

out:
	tzdev_teec_fixup_origin(result, &origin);

	log_debug(tzdev_teec, "Exit, context = %pK sharedMem = %pK\n", context, sharedMem);

	return result;
}

TEEC_Result TEEC_RegisterSharedMemory(TEEC_Context *context, TEEC_SharedMemory *sharedMem)
{
	return tzdev_teec_register_shared_memory(context, sharedMem, false);
}
EXPORT_SYMBOL(TEEC_RegisterSharedMemory);

TEEC_Result TEEC_AllocateSharedMemory(TEEC_Context *context, TEEC_SharedMemory *sharedMem)
{
	return tzdev_teec_register_shared_memory(context, sharedMem, true);
}
EXPORT_SYMBOL(TEEC_AllocateSharedMemory);

void TEEC_ReleaseSharedMemory(TEEC_SharedMemory *sharedMem)
{
	struct tzdev_teec_shared_memory *shm;
	uint32_t result = TEEC_SUCCESS;
	uint32_t origin;

	log_debug(tzdev_teec, "Enter, sharedMem = %pK\n", sharedMem);

	origin = TEEC_ORIGIN_API;

	if (!sharedMem || !sharedMem->imp) {
		log_error(tzdev_teec, "Null shared memory passed\n");
		result = TEEC_ERROR_BAD_PARAMETERS;
		goto out;
	}

	shm = sharedMem->imp;
	if (shm->is_allocated) {
		sharedMem->buffer = NULL;
		sharedMem->size = 0;
	}

	tzdev_teec_shared_memory_fini(sharedMem);

out:
	log_debug(tzdev_teec, "Exit, sharedMem = %pK result = %x origin = %u\n",
			sharedMem, result, origin);
}
EXPORT_SYMBOL(TEEC_ReleaseSharedMemory);
