/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd All Rights Reserved
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

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include <tzdev/tee_client_api.h>

#include "core/log.h"
#include "tests.h"

/* test_TA_sleep */
static TEEC_UUID uuid = {
	0x09c63f81, 0xc034, 0x4c38, {0xa8, 0x13, 0x48, 0x6e, 0xe4, 0x23, 0xb8, 0x06}
};

static void *uint64_to_ptr(uint64_t val)
{
	return (void *)(unsigned long)val;
}

uint32_t tz_tee_test_sleep(uint32_t *origin, struct test_data *data)
{
	TEEC_Context context;
	TEEC_Session session;
	TEEC_Operation operation;
	TEEC_Result result;
	char *place;

	place = vmalloc(data->size + 1);
	if (place == NULL) {
		return -ENOMEM;
	}
	strncpy(place, uint64_to_ptr(data->address), data->size);
	place[data->size] = '\0';

	printk("TEST: Begin kernel tee_test_sleep for %s\n", place);

	*origin = 0x0;

	operation.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INOUT,
		TEEC_NONE,
		TEEC_NONE,
		TEEC_NONE);

	operation.params[0].value.a = 0;
	operation.params[0].value.b = 0;

	if (strcmp(place, "open_session") == 0)
		operation.params[0].value.a = 1;
	else if (strcmp(place, "invoke_cmd") == 0)
		operation.params[0].value.a = 2;

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS)
		goto out;

	result = TEEC_OpenSession(&context, &session, &uuid, TEEC_LOGIN_PUBLIC,
			NULL, &operation, origin);
	if (result != TEEC_SUCCESS)
		goto finalize_context;

	result = TEEC_InvokeCommand(&session, 0x0, &operation, origin);
	if (result != TEEC_SUCCESS)
		goto close_session;

close_session:
	TEEC_CloseSession(&session);
finalize_context:
	TEEC_FinalizeContext(&context);
out:
	vfree(place);
	printk("TEST: End kernel tee_test_sleep, result=0x%x origin=0x%x\n", result, *origin);

	return result;
}

