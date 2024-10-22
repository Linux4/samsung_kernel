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
#include <linux/types.h>
#include <linux/workqueue.h>

#include <tzdev/tee_client_api.h>

#include "core/log.h"

/* test_TA_shared_ref */
static TEEC_UUID uuid = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 93}};

uint32_t tz_tee_test_shared_ref(uint32_t *origin)
{
	TEEC_Context context;
	TEEC_Session session;
	TEEC_SharedMemory sharedMem;
	TEEC_Operation operation;
	TEEC_Result result;
	unsigned long long phys_addr_ca;
	unsigned long long phys_addr_ta;
	int *dummy_buffer;

	dummy_buffer = kmalloc(sizeof(int), GFP_KERNEL);
	if (!dummy_buffer) {
		result = TEEC_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS)
		goto release_buffer;

	sharedMem.buffer = dummy_buffer;
	sharedMem.size = sizeof(int);
	sharedMem.flags = TEEC_MEM_INPUT;

	result = TEEC_RegisterSharedMemory(&context, &sharedMem);
	if (result != TEEC_SUCCESS)
		goto finalize_context;

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT, TEEC_MEMREF_PARTIAL_INPUT,
			TEEC_NONE, TEEC_NONE);
	operation.params[1].memref.parent = &sharedMem;
	operation.params[1].memref.size = sharedMem.size;
	operation.params[1].memref.offset = 0x0;

	result = TEEC_OpenSession(&context, &session, &uuid, TEEC_LOGIN_PUBLIC,
			NULL, &operation, origin);
	if (result != TEEC_SUCCESS)
		goto release_memory;

	phys_addr_ca = virt_to_phys(dummy_buffer);

	phys_addr_ta = operation.params[0].value.b;
	phys_addr_ta <<= 32;
	phys_addr_ta |= operation.params[0].value.a;

	printk("TEST: phys addr from CA: 0x%llx\n", phys_addr_ca);
	printk("TEST: phys addr from TA: 0x%llx\n", phys_addr_ta);

	if (phys_addr_ca != phys_addr_ta)
		result = TEEC_ERROR_GENERIC;

	TEEC_CloseSession(&session);
release_memory:
	TEEC_ReleaseSharedMemory(&sharedMem);
finalize_context:
	TEEC_FinalizeContext(&context);
release_buffer:
	kfree(dummy_buffer);
out:
	return result;
}
