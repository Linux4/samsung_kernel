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

#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <tzdev/tee_client_api.h>

#include "core/log.h"

#define MSECS_DELAY_BEFORE_CANCEL	2000
#define MSECS_DELAY_BETWEEN_TESTS	500

enum {
	CMD_GET_FLAG,
	CMD_CANCELLATION_UNMASK,
	CMD_CANCELLATION_MASK,
	CMD_CANCELLABLE_COMMAND,
};

/* test_TA_tee_cancellation */
static TEEC_UUID uuid = {0x7e91c11a,0xcaa7,0x11eb,{0x99, 0x46,0x73,0xf7,0x5f,0x93,0x03,0x58}};

static void cancellation_work_do(struct work_struct *work);
static DECLARE_DELAYED_WORK(cancellation_work, cancellation_work_do);
static TEEC_Operation operation;

static void cancellation_work_do(struct work_struct *work)
{
	(void)work;
	TEEC_RequestCancellation(&operation);
}

static TEEC_Result test_cancel_open_session(TEEC_Context *context)
{
	TEEC_Session session;
	TEEC_Result result;

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	operation.started = 0;

	schedule_delayed_work(&cancellation_work, msecs_to_jiffies(MSECS_DELAY_BEFORE_CANCEL));

	result = TEEC_OpenSession(context, &session, &uuid, TEEC_LOGIN_PUBLIC, NULL, &operation, NULL);
	if (result != TEEC_ERROR_CANCEL) {
		printk("TEST: operation failed to cancel during open session, result=%x\n", result);
		if (result == TEEC_SUCCESS)
			TEEC_CloseSession(&session);
		return TEEC_ERROR_GENERIC;
	}

	cancel_delayed_work_sync(&cancellation_work);

	return TEEC_SUCCESS;
}

static TEEC_Result test_cancel_open_session_before(TEEC_Context *context)
{
	TEEC_Session session;
	TEEC_Result result;

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	operation.started = 0;

	TEEC_RequestCancellation(&operation);

	result = TEEC_OpenSession(context, &session, &uuid, TEEC_LOGIN_PUBLIC, NULL, &operation, NULL);
	if (result != TEEC_ERROR_CANCEL) {
		printk("TEST: operation failed to cancel before open session, result=%x\n", result);
		if (result == TEEC_SUCCESS)
			TEEC_CloseSession(&session);
		return TEEC_ERROR_GENERIC;
	}

	return TEEC_SUCCESS;
}

static TEEC_Result test_cancel_open_session_after(TEEC_Context *context)
{
	TEEC_Session session;
	TEEC_Result result;

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	operation.started = 0;

	result = TEEC_OpenSession(context, &session, &uuid, TEEC_LOGIN_PUBLIC, NULL, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: open session failed with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	TEEC_RequestCancellation(&operation);
	TEEC_CloseSession(&session);

	return TEEC_SUCCESS;
}

static TEEC_Result test_cancel_invoke_command(TEEC_Session *session)
{
	TEEC_Result result;

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	operation.started = 0;

	schedule_delayed_work(&cancellation_work, msecs_to_jiffies(MSECS_DELAY_BEFORE_CANCEL));

	result = TEEC_InvokeCommand(session, CMD_CANCELLABLE_COMMAND, &operation, NULL);
	if (result != TEEC_ERROR_CANCEL) {
		printk("TEST: operation failed to cancel during invoke command, result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	cancel_delayed_work_sync(&cancellation_work);

	return TEEC_SUCCESS;
}

static TEEC_Result test_cancel_invoke_command_masked(TEEC_Session *session)
{
	TEEC_Result result;

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	result = TEEC_InvokeCommand(session, CMD_CANCELLATION_MASK, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: failed to mask cancellation with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	operation.started = 0;

	schedule_delayed_work(&cancellation_work, msecs_to_jiffies(MSECS_DELAY_BEFORE_CANCEL));

	result = TEEC_InvokeCommand(session, CMD_CANCELLABLE_COMMAND, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: invoke command failed with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	cancel_delayed_work_sync(&cancellation_work);

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	result = TEEC_InvokeCommand(session, CMD_GET_FLAG, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: failed to get cancellation flag with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	if (operation.params[0].value.a) {
		printk("TEST: cancellation should not be set in TA\n");
		return TEEC_ERROR_GENERIC;
	}

	result = TEEC_InvokeCommand(session, CMD_CANCELLATION_UNMASK, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: failed to unmask cancellation with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	return TEEC_SUCCESS;
}

static TEEC_Result test_cancel_invoke_command_before(TEEC_Session *session)
{
	TEEC_Result result;

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	operation.started = 0;

	TEEC_RequestCancellation(&operation);

	result = TEEC_InvokeCommand(session, CMD_CANCELLABLE_COMMAND, &operation, NULL);
	if (result != TEEC_ERROR_CANCEL) {
		printk("TEST: operation failed to cancel before invoke command, result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	return TEEC_SUCCESS;
}

static TEEC_Result test_cancel_invoke_command_after(TEEC_Session *session)
{
	TEEC_Result result;

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
	operation.started = 0;

	result = TEEC_InvokeCommand(session, CMD_CANCELLABLE_COMMAND, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: invoke command failed with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	TEEC_RequestCancellation(&operation);

	return TEEC_SUCCESS;
}

static TEEC_Result test_cancel_mask_unmask(TEEC_Session *session)
{
	TEEC_Result result;

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	/* mask cancellation - no check on return as we don't know current state */
	result = TEEC_InvokeCommand(session, CMD_CANCELLATION_MASK, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: failed to mask cancellation with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	/* mask cancellation again - return should state cancellations were previously masked */
	result = TEEC_InvokeCommand(session, CMD_CANCELLATION_MASK, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: failed to mask cancellation with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	if (!operation.params[0].value.b) {
		printk("TEST: cancellation should be masked in TA\n");
		return TEEC_ERROR_GENERIC;
	}

	/* unmask cancellation - return should state cancellations were previously masked */
	result = TEEC_InvokeCommand(session, CMD_CANCELLATION_UNMASK, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: failed to mask cancellation with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	if (!operation.params[0].value.b) {
		printk("TEST: cancellation should be masked in TA\n");
		return TEEC_ERROR_GENERIC;
	}

	/* unmask cancellation again - return should state cancellations were previously unmasked */
	result = TEEC_InvokeCommand(session, CMD_CANCELLATION_UNMASK, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: failed to mask cancellation with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	if (operation.params[0].value.b) {
		printk("TEST: cancellation should be unmasked in TA\n");
		return TEEC_ERROR_GENERIC;
	}

	/* mask cancellation - return should state cancellations were previously unmasked */
	result = TEEC_InvokeCommand(session, CMD_CANCELLATION_MASK, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		printk("TEST: failed to mask cancellation with result=%x\n", result);
		return TEEC_ERROR_GENERIC;
	}

	if (operation.params[0].value.b) {
		printk("TEST: cancellation should be unmasked in TA\n");
		return TEEC_ERROR_GENERIC;
	}

	return TEEC_SUCCESS;
}

uint32_t tz_tee_test_cancellation(uint32_t *origin)
{
	TEEC_Context context;
	TEEC_Session session;
	TEEC_Result result;

	printk("TEST: Begin kernel tee_test_cancellation\n");

	*origin = 0x0;

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS)
		goto out;

	result = test_cancel_open_session(&context);
	if (result != TEEC_SUCCESS)
		goto finalize_context;

	msleep(MSECS_DELAY_BETWEEN_TESTS);

	result = test_cancel_open_session_before(&context);
	if (result != TEEC_SUCCESS)
		goto finalize_context;

	msleep(MSECS_DELAY_BETWEEN_TESTS);

	result = test_cancel_open_session_after(&context);
	if (result != TEEC_SUCCESS)
		goto finalize_context;

	msleep(MSECS_DELAY_BETWEEN_TESTS);

	result = TEEC_OpenSession(&context, &session, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
	if (result != TEEC_SUCCESS)
		goto finalize_context;

	result = test_cancel_invoke_command(&session);
	if (result != TEEC_SUCCESS)
		goto close_session;

	result = test_cancel_invoke_command_masked(&session);
	if (result != TEEC_SUCCESS)
		goto close_session;

	result = test_cancel_invoke_command_before(&session);
	if (result != TEEC_SUCCESS)
		goto close_session;

	result = test_cancel_invoke_command_after(&session);
	if (result != TEEC_SUCCESS)
		goto close_session;

	result = test_cancel_mask_unmask(&session);
	if (result != TEEC_SUCCESS)
		goto close_session;

	printk("TEST: All tee_test_cancellation cases passed successfully\n");

close_session:
	TEEC_CloseSession(&session);
finalize_context:
	TEEC_FinalizeContext(&context);
out:
	printk("TEST: End kernel tee_test_cancellation, result=0x%x origin=%u\n", result, *origin);

	return result;
}
