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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <tzdev/tee_client_api.h>

#include "core/log.h"

#define BUFFER_SIZE_ALLOCATED		50
#define BUFFER_SIZE_TMPREF		100
#define BUFFER_SIZE			32

/* test_TA */
static TEEC_UUID uuid = {
	0x10000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

uint32_t tz_tee_test(uint32_t *origin)
{
	TEEC_Context context;
	TEEC_SharedMemory sharedMem;
	TEEC_SharedMemory sharedMemAlc;
	TEEC_Session session;
	TEEC_Operation operation;
	TEEC_Result result;
	unsigned int i;
	bool val_is_ok = true;
	bool shm_is_ok = true;
	void *tmp_buffer;
	char *p;
	char *buffer;

	printk("TEST: Begin kernel tee_test\n");

	*origin = 0x0;

	buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
	if (!buffer) {
		result = TEEC_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS)
		goto release_buffer;

	sharedMem.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	sharedMem.size = BUFFER_SIZE;
	sharedMem.buffer = buffer;

	result = TEEC_RegisterSharedMemory(&context, &sharedMem);
	if (result != TEEC_SUCCESS)
		goto finalize_context;

	sharedMemAlc.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;
	sharedMemAlc.size = BUFFER_SIZE_ALLOCATED;

	result = TEEC_AllocateSharedMemory(&context, &sharedMemAlc);
	if (result != TEEC_SUCCESS)
		goto release_shared_memory;

	result = TEEC_OpenSession(&context, &session, &uuid, TEEC_LOGIN_PUBLIC,
			NULL, NULL, origin);
	if (result != TEEC_SUCCESS)
		goto release_shared_memory_alc;

	tmp_buffer = kmalloc(BUFFER_SIZE_TMPREF, GFP_KERNEL);
	if (!tmp_buffer) {
		result = TEEC_ERROR_OUT_OF_MEMORY;
		goto close_session;
	}

	operation.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INOUT,
		TEEC_MEMREF_TEMP_INOUT,
		TEEC_MEMREF_PARTIAL_INOUT,
		TEEC_MEMREF_PARTIAL_INOUT);

	operation.params[0].value.a = 0x123;
	operation.params[0].value.b = 0x321;

	operation.params[1].tmpref.buffer = tmp_buffer;
	operation.params[1].tmpref.size = BUFFER_SIZE_TMPREF;

	operation.params[2].memref.parent = &sharedMem;
	operation.params[2].memref.offset = 0x0;
	operation.params[2].memref.size = BUFFER_SIZE;

	operation.params[3].memref.parent = &sharedMemAlc;
	operation.params[3].memref.offset = 0x0;
	operation.params[3].memref.size = BUFFER_SIZE_ALLOCATED;

	p = sharedMemAlc.buffer;
	for (i = 0; i < BUFFER_SIZE_ALLOCATED; ++i)
		p[i] = (i & 0xff);

	result = TEEC_InvokeCommand(&session, 0x0, &operation, origin);
	if (result != TEEC_SUCCESS)
		goto free_tmp_buffer;

	if (operation.params[0].value.a != 0x321 || operation.params[0].value.b != 0x123)
		val_is_ok = false;

	for (i = 0; i < BUFFER_SIZE_ALLOCATED; ++i) {
		if (p[i] != ((BUFFER_SIZE_ALLOCATED - i - 1) & 0xff)) {
			shm_is_ok = false;
			break;
		}
	}

	printk("TEST: tee_test value check : %s\n", val_is_ok ? "OK" : "FAIL");
	printk("TEST: tee_test shmem check : %s\n", shm_is_ok ? "OK" : "FAIL");

	if (!val_is_ok || !shm_is_ok) {
		result = TEEC_ERROR_GENERIC;
		*origin = 0x0;
	}

free_tmp_buffer:
	kfree(tmp_buffer);
close_session:
	TEEC_CloseSession(&session);
release_shared_memory_alc:
	TEEC_ReleaseSharedMemory(&sharedMemAlc);
release_shared_memory:
	TEEC_ReleaseSharedMemory(&sharedMem);
finalize_context:
	TEEC_FinalizeContext(&context);
release_buffer:
	kfree(buffer);
out:
	printk("TEST: End kernel tee_test, result=0x%x origin=0x%x\n", result, *origin);

	return result;
}
