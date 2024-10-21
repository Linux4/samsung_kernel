/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd All Rights Reserved
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
#include <linux/types.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include <tzdev/tee_client_api.h>

#include "core/log.h"
#include "core/sysdep.h"
#include "tests.h"

#define GET_VALUE 0x200
#define SEND_VALUE 0x100

static const TEEC_UUID ta_uuid = {
	0x6cec21f6,
	0xe601,
	0x49c7,
	{ 0x83, 0x1c, 0x5d, 0x8b, 0xab, 0xe0, 0x36, 0x2d },
};

static void *uint64_to_ptr(uint64_t val)
{
	return (void *)(unsigned long)val;
}

uint32_t tz_tee_test_send_ta(uint32_t *origin, struct test_data *data)
{
	TEEC_Result res = TEEC_ERROR_GENERIC;
	TEEC_Operation operation;
	TEEC_Context context;
	TEEC_Session session;

	*origin = 0x0;

	res = TEEC_InitializeContext(NULL, &context);
	if (res != TEEC_SUCCESS) {
		printk("TEST: TEEC_InitializeContext failed. res = %x\n", res);
		goto exit;
	}

	res = TEECS_OpenSession(&context, &session, &ta_uuid, uint64_to_ptr(data->address),
			data->size, 0, NULL, NULL, origin);
	if (res != TEEC_SUCCESS) {
		printk("TEST: TEECS_OpenSession returned %x from %x\n", res, *origin);
		goto context_cleanup;
	}

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	operation.params[0].value.a = SEND_VALUE;

	res = TEEC_InvokeCommand(&session, 0, &operation, origin);
	if (res != TEEC_SUCCESS) {
		printk("TEST: TEEC_InvokeCommand returned %x from %x\n", res, *origin);
		goto session_cleanup;
	}

	if (operation.params[0].value.a != GET_VALUE) {
		printk("TEST: TEEC_InvokeCommand returned %d, but expected %d\n",
			operation.params[0].value.a,
			GET_VALUE);
		goto session_cleanup;
	}

	printk("TEST: tee_test_send_ta value check : OK\n");

session_cleanup:
	TEEC_CloseSession(&session);

context_cleanup:
	TEEC_FinalizeContext(&context);

exit:
	printk("TEST: End kernel tee_test_send_ta, result = 0x%x, origin=0x%x\n", res, *origin);
	return res;
}

