/* tui/stui_inf.c
 *
 * Samsung TUI HW Handler driver.
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#ifdef USE_TEE_CLIENT_API
#include <tee_client_api.h>
#endif /* USE_TEE_CLIENT_API */
#include "stui_inf.h"

#define TUI_REE_EXTERNAL_EVENT      42
#define SESSION_CANCEL_DELAY        150
#define MAX_WAIT_CNT                10

static int tui_mode = STUI_MODE_OFF;
static int tui_blank_cnt;
static DEFINE_SPINLOCK(tui_lock);

#ifdef USE_TEE_CLIENT_API
static TEEC_UUID uuid = {
	.timeLow = 0x0,
	.timeMid = 0x0,
	.timeHiAndVersion = 0x0,
	.clockSeqAndNode = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x81},
};
#endif

int stui_inc_blank_ref(void)
{
	unsigned long fls;
	int ret_cnt;

	spin_lock_irqsave(&tui_lock, fls);
	ret_cnt = ++tui_blank_cnt;
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_cnt;
}
EXPORT_SYMBOL(stui_inc_blank_ref);

int stui_dec_blank_ref(void)
{
	unsigned long fls;
	int ret_cnt;

	spin_lock_irqsave(&tui_lock, fls);
	ret_cnt = --tui_blank_cnt;
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_cnt;
}
EXPORT_SYMBOL(stui_dec_blank_ref);

int stui_get_blank_ref(void)
{
	unsigned long fls;
	int ret_cnt;

	spin_lock_irqsave(&tui_lock, fls);
	ret_cnt = tui_blank_cnt;
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_cnt;
}
EXPORT_SYMBOL(stui_get_blank_ref);

void stui_set_blank_ref(int cnt)
{
	unsigned long fls;

	spin_lock_irqsave(&tui_lock, fls);
	tui_blank_cnt = cnt;
	spin_unlock_irqrestore(&tui_lock, fls);
}
EXPORT_SYMBOL(stui_set_blank_ref);

int stui_get_mode(void)
{
	unsigned long fls;
	int ret_mode;

	spin_lock_irqsave(&tui_lock, fls);
	ret_mode = tui_mode;
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_mode;
}
EXPORT_SYMBOL(stui_get_mode);

void stui_set_mode(int mode)
{
	unsigned long fls;

	spin_lock_irqsave(&tui_lock, fls);
	tui_mode = mode;
	spin_unlock_irqrestore(&tui_lock, fls);
}
EXPORT_SYMBOL(stui_set_mode);

int stui_set_mask(int mask)
{
	unsigned long fls;
	int ret_mode;

	spin_lock_irqsave(&tui_lock, fls);
	ret_mode = (tui_mode |= mask);
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_mode;
}
EXPORT_SYMBOL(stui_set_mask);

int stui_clear_mask(int mask)
{
	unsigned long fls;
	int ret_mode;

	spin_lock_irqsave(&tui_lock, fls);
	ret_mode = (tui_mode &= ~mask);
	spin_unlock_irqrestore(&tui_lock, fls);
	return ret_mode;
}
EXPORT_SYMBOL(stui_clear_mask);

#ifdef USE_TEE_CLIENT_API
static atomic_t canceling = ATOMIC_INIT(0);
int stui_cancel_session(void)
{
	TEEC_Context context;
	TEEC_Session session;
	TEEC_Result result;
	TEEC_Operation operation;
	int ret = -1;
	int count = 0;

	pr_info("[STUI] %s begin\n", __func__);

	if (!(STUI_MODE_ALL & stui_get_mode())) {
		pr_info("[STUI] session cancel is not needed\n");
		return 0;
	}

	if (atomic_cmpxchg(&canceling, 0, 1) != 0) {
		pr_info("[STUI] already canceling.\n");
		return 0;
	}

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS) {
		pr_err("[STUI] TEEC_InitializeContext returned: 0x%x\n", result);
		goto out;
	}

	result = TEEC_OpenSession(&context, &session, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, NULL);
	if (result != TEEC_SUCCESS) {
		pr_err("[STUI] TEEC_OpenSession returned: 0x%x\n", result);
		goto finalize_context;
	}

	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	result = TEEC_InvokeCommand(&session, TUI_REE_EXTERNAL_EVENT, &operation, NULL);
	if (result != TEEC_SUCCESS) {
		pr_err("[STUI] TEEC_InvokeCommand returned: 0x%x\n", result);
		goto close_session;
	} else {
		pr_info("[STUI] invoked cancel cmd\n");
	}

	TEEC_CloseSession(&session);
	TEEC_FinalizeContext(&context);

	while ((STUI_MODE_ALL & stui_get_mode()) && (count < MAX_WAIT_CNT)) {
		msleep(SESSION_CANCEL_DELAY);
		count++;
	}

	if (STUI_MODE_ALL & stui_get_mode()) {
		pr_err("[STUI] session was not cancelled yet\n");
	} else {
		pr_info("[STUI] session was cancelled successfully\n");
		ret = 0;
	}

	atomic_set(&canceling, 0);
	return ret;

close_session:
	TEEC_CloseSession(&session);
finalize_context:
	TEEC_FinalizeContext(&context);
out:
	atomic_set(&canceling, 0);
	pr_err("[STUI] %s end, ret=%d, result=0x%x\n", __func__, ret, result);

	return ret;
}
EXPORT_SYMBOL(stui_cancel_session);
#else /* USE_TEE_CLIENT_API */
int stui_cancel_session(void)
{
	pr_err("[STUI] %s not supported\n", __func__);
	return -1;
}
EXPORT_SYMBOL(stui_cancel_session);
#endif /* USE_TEE_CLIENT_API */

static int __init teegris_tui_inf_init(void)
{
	pr_info("=============== Running TEEgris TUI Inf ===============");
	return 0;
}

static void __exit teegris_tui_inf_exit(void)
{
	pr_info("Unloading teegris tui inf module.");
}

module_init(teegris_tui_inf_init);
module_exit(teegris_tui_inf_exit);

MODULE_AUTHOR("TUI Teegris");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TEEGRIS TUI");
