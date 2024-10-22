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

uint32_t tz_tee_test_null_args(uint32_t *origin)
{
	TEEC_Context context;
	TEEC_Session session;
	TEEC_SharedMemory sharedMem;
	TEEC_UUID uuid;
	TEEC_Result result;

	*origin = 0x0;

	result = TEEC_InitializeContext(NULL, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_InitializeContext("Samsung TEE", NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_RegisterSharedMemory(NULL, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_RegisterSharedMemory(&context, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_RegisterSharedMemory(NULL, &sharedMem);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_AllocateSharedMemory(NULL, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_AllocateSharedMemory(&context, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_AllocateSharedMemory(NULL, &sharedMem);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_OpenSession(NULL, NULL, NULL, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_OpenSession(&context, NULL, NULL, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_OpenSession(NULL, &session, NULL, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_OpenSession(NULL, NULL, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_OpenSession(&context, &session, NULL, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	result = TEEC_InvokeCommand(NULL, 0x0, NULL, NULL);
	if (result != TEEC_ERROR_BAD_PARAMETERS)
		return TEEC_ERROR_GENERIC;

	TEEC_CloseSession(NULL);

	TEEC_ReleaseSharedMemory(NULL);

	TEEC_FinalizeContext(NULL);

	return TEEC_SUCCESS;
}
