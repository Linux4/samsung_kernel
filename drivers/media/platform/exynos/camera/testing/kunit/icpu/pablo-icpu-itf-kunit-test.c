// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/completion.h>

#include "pablo-kunit-test.h"
#include "icpu/pablo-icpu-core.h"
#include "icpu/hardware/pablo-icpu-hw.h"
#include "icpu/mbox/pablo-icpu-mbox.h"
#include "pablo-icpu-itf.h"

static struct icpu_platform_data test_pdata;

/* Define the test cases. */
static struct test_ctx {
	struct icpu_mbox_controller tx_mbox;
	struct icpu_mbox_controller rx_mbox;
	struct pablo_icpu_mbox_chan tx_chan;
	struct pablo_icpu_mbox_chan rx_chan;
	struct icpu_mbox_client cl;

	struct timer_list timer;

	struct kunit *test;
} ctx;

static struct pablo_icpu_itf_api *itf;

static struct completion timer_done;

#define SET_ICPU_CPU_WFI_STATUS(_pd) \
	do { *(u32 *)(_pd.mcuctl_reg_base + 0xC4) = 1; } while(0)

static void pablo_icpu_itf_open_kunit_test(struct kunit *test)
{
	int ret;
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_itf_shutdown_firmware_kunit_test(struct kunit *test)
{
	int ret;
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	pablo_icpu_itf_shutdown_firmware();

	pablo_icpu_test_itf_close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_itf_fsm_open_kunit_test(struct kunit *test)
{
	int ret;
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_itf_fsm_send_message_kunit_test(struct kunit *test)
{
	int ret;
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);
	u32 send_buf[1] = { 4, };

	/* invalid state */
	ret = itf->send_message(test, NULL, NULL, ICPU_MSG_PRIO_NORMAL, 1, send_buf);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* wait fw ready state */
	ret = itf->send_message(test, NULL, NULL, ICPU_MSG_PRIO_NORMAL, 1, send_buf);
	KUNIT_EXPECT_EQ(test, ret, -EAGAIN);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = itf->send_message(test, NULL, NULL, ICPU_MSG_PRIO_NORMAL, 1, send_buf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_icpu_test_itf_close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_itf_get_free_message_kunit_test(struct kunit *test)
{
	int ret;
	int i;
	struct icpu_itf_msg *msgs[100];
	u32 free_msg_cnt;
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	for (i = 0; i < 100; i++) {
		free_msg_cnt = get_free_msg_cnt_wrap();
		KUNIT_EXPECT_EQ(test, free_msg_cnt, (u32)(MESSAGE_MAX_COUNT - i));

		msgs[i] = get_free_msg_wrap();
		if (!msgs[i])
			break;
		msgs[i]->len = 1;
	}
	KUNIT_EXPECT_EQ(test, i, MESSAGE_MAX_COUNT);

	for (i = 0; i < MESSAGE_MAX_COUNT; i++) {
		free_msg_cnt = get_free_msg_cnt_wrap();
		KUNIT_EXPECT_EQ(test, free_msg_cnt, (u32)i);

		ret = set_free_msg_wrap(msgs[i]);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* i = MESSAGE_MAX_COUNT, it mean null pointer */
	ret = set_free_msg_wrap(msgs[i]);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_itf_get_pending_message_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	u32 pending_msg_cnt;
	int i;
	struct test_vec {
		union icpu_itf_msg_cmd cmd;
		enum icpu_msg_priority prio;
	} vec[10] = {
		{ { .key = 0 }, ICPU_MSG_PRIO_HIGH, },
		{ { .key = 1 }, ICPU_MSG_PRIO_NORMAL, },
		{ { .key = 2 }, ICPU_MSG_PRIO_LOW, },
		{ { .key = 3 }, ICPU_MSG_PRIO_LOW, },
		{ { .key = 4 }, ICPU_MSG_PRIO_LOW, },
		{ { .key = 5 }, ICPU_MSG_PRIO_NORMAL, },
		{ { .key = 6 }, ICPU_MSG_PRIO_LOW, },
		{ { .key = 7 }, ICPU_MSG_PRIO_NORMAL, },
		{ { .key = 8 }, ICPU_MSG_PRIO_NORMAL, },
		{ { .key = 9 }, ICPU_MSG_PRIO_HIGH, },
	};
	u32 sol[10] = { 0, 9, 1, 5, 7, 8, 2, 3, 4, 6, };
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, (u32)0);

	msg = get_pending_msg_prio_wrap();
	KUNIT_EXPECT_PTR_EQ(test, (void*)msg, NULL);

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, (u32)0);

	for (i = 0; i < 10; i++) {
		msg = get_free_msg_wrap();
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

		msg->cmd.key = vec[i].cmd.key;
		msg->prio = vec[i].prio;

		ret = set_pending_msg_prio_wrap(msg);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, (u32)10);

	for (i = 0; i < 10; i++) {
		msg = get_pending_msg_prio_wrap();
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

		KUNIT_EXPECT_EQ(test, msg->cmd.key, vec[sol[i]].cmd.key);
		KUNIT_EXPECT_EQ(test, msg->prio, vec[sol[i]].prio);
	}

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, (u32)0);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_itf_get_pending_message_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	u32 pending_msg_cnt;
	int i;
	struct test_vec {
		union icpu_itf_msg_cmd cmd;
		enum icpu_msg_priority prio;
	} vec = { { .key = 0xFE }, ICPU_MSG_PRIO_HIGH, };
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, (u32)0);

	msg = get_pending_msg_prio_wrap();
	KUNIT_EXPECT_PTR_EQ(test, (void*)msg, NULL);

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, (u32)0);

	msg = get_free_msg_wrap();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

	msg->cmd.key = vec.cmd.key;
	msg->prio = vec.prio;

	ret = set_pending_msg_prio_wrap(msg);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, (u32)1);

	/* get valid pending msg */
	msg = get_pending_msg_prio_wrap();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, msg->cmd.key, vec.cmd.key);
	KUNIT_EXPECT_EQ(test, msg->prio, vec.prio);

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, (u32)0);

	/* try to get pending msg from empty queue */
	for (i = 0; i < 10; i++) {
		msg = get_pending_msg_prio_wrap();
		KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);
	}

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, (u32)0);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_itf_get_response_message_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	u32 response_msg_cnt;
	int i;
	struct test_vec {
		union icpu_itf_msg_cmd cmd;
		enum icpu_msg_priority prio;
		u32 data[1];
	} vec[10] = {
		{ { .key = 0 }, ICPU_MSG_PRIO_HIGH, { 100, }, },
		{ { .key = 1 }, ICPU_MSG_PRIO_NORMAL, { 101, }, },
		{ { .key = 2 }, ICPU_MSG_PRIO_LOW, { 102, }, },
		{ { .key = 3 }, ICPU_MSG_PRIO_LOW, { 103, }, },
		{ { .key = 4 }, ICPU_MSG_PRIO_LOW, { 104, }, },
		{ { .key = 5 }, ICPU_MSG_PRIO_NORMAL, { 105, }, },
		{ { .key = 6 }, ICPU_MSG_PRIO_LOW, { 106, }, },
		{ { .key = 7 }, ICPU_MSG_PRIO_NORMAL, { 107, }, },
		{ { .key = 8 }, ICPU_MSG_PRIO_NORMAL, { 108, }, },
		{ { .key = 9 }, ICPU_MSG_PRIO_HIGH, { 109, }, },
	};
	union icpu_itf_msg_cmd sol[10] = {
		{ .key = 0 },
		{ .key = 9 },
		{ .key = 1 },
		{ .key = 5 },
		{ .key = 7 },
		{ .key = 8 },
		{ .key = 2 },
		{ .key = 3 },
		{ .key = 4 },
		{ .key = 6 },
	};
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	response_msg_cnt = get_response_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, response_msg_cnt, (u32)0);

	msg = get_response_msg_match_wrap(vec[0].cmd);
	KUNIT_EXPECT_PTR_EQ(test, (void*)msg, NULL);

	response_msg_cnt = get_response_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, response_msg_cnt, (u32)0);

	for (i = 0; i < 10; i++) {
		msg = get_free_msg_wrap();
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

		msg->cmd.key = vec[i].cmd.key;
		msg->prio = vec[i].prio;
		msg->data[0] = vec[i].data[0];

		ret = set_response_msg_wrap(msg);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	response_msg_cnt = get_response_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, response_msg_cnt, (u32)10);

	for (i = 0; i < 10; i++) {
		msg = get_response_msg_match_wrap(sol[i]);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

		KUNIT_EXPECT_EQ(test, msg->cmd.key, vec[sol[i].key].cmd.key);
		KUNIT_EXPECT_EQ(test, msg->prio, vec[sol[i].key].prio);
		KUNIT_EXPECT_EQ(test, msg->data[0], vec[sol[i].key].data[0]);
	}

	response_msg_cnt = get_response_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, response_msg_cnt, (u32)0);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_itf_get_response_message_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	u32 response_msg_cnt;
	int i;
	struct test_vec {
		union icpu_itf_msg_cmd cmd;
		enum icpu_msg_priority prio;
		u32 data[1];
	} vec = { { .key = 0x123 }, ICPU_MSG_PRIO_HIGH, { 100, } };
	union icpu_itf_msg_cmd sol = { .key = 0x123 };
	union icpu_itf_msg_cmd invalid_cmd = { .key = 0xFF };
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	response_msg_cnt = get_response_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, response_msg_cnt, (u32)0);

	msg = get_response_msg_match_wrap(vec.cmd);
	KUNIT_EXPECT_PTR_EQ(test, (void*)msg, NULL);

	response_msg_cnt = get_response_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, response_msg_cnt, (u32)0);

	/* free -> response msg */
	msg = get_free_msg_wrap();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

	msg->cmd.key = vec.cmd.key;
	msg->prio = vec.prio;
	msg->data[0] = vec.data[0];

	ret = set_response_msg_wrap(msg);
	KUNIT_EXPECT_EQ(test, ret, 0);

	response_msg_cnt = get_response_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, response_msg_cnt, (u32)1);

	/* try to get msg with invalid cmd */
	for (i = 0; i < 10; i++) {
		msg = get_response_msg_match_wrap(invalid_cmd);
		KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);
	}

	/* get valid msg */
	msg = get_response_msg_match_wrap(sol);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

	KUNIT_EXPECT_EQ(test, msg->cmd.key, vec.cmd.key);
	KUNIT_EXPECT_EQ(test, msg->prio, vec.prio);
	KUNIT_EXPECT_EQ(test, msg->data[0], vec.data[0]);

	response_msg_cnt = get_response_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, response_msg_cnt, (u32)0);

	/* try to get rsp msg in case empty queue */
	for (i = 0; i < 10; i++) {
		msg = get_response_msg_match_wrap(sol);
		KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);
	}

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();
	pablo_icpu_test_set_platform_data(pdata);
}

#define NUM_TEST_BUF 8
static u32 test_buf[NUM_TEST_BUF];
static u32 history_cnt;
static bool wait_idle = false;
static bool mbox_timeout = false;
static int __pablo_icpu_itf_send_message_kunit_test(struct pablo_icpu_mbox_chan *chan, u32 *tx_data, u32 len)
{
	struct kunit *test = ctx.test;
	KUNIT_EXPECT_GT(test, len, (u32)0);

	/* Ignore timestamp sync msg */
	if (tx_data[0] == 2112)
		return 0;

	if (mbox_timeout) {
		pr_info("send_message timeout enabled");
		return -ETIMEDOUT;
	}

	while (wait_idle) {
		pr_err("wait_idle ... ");
		msleep(10);
	}

	test_buf[history_cnt] = tx_data[0];
	history_cnt++;

	if (chan->cl->tx_done)
		chan->cl->tx_done(chan->cl, tx_data, len);

	return 0;
}

static void pablo_icpu_itf_send_message_async_basic_kunit_test(struct kunit *test)
{
	int ret;
	u32 send_buf[NUM_TEST_BUF] = { 4, 2, 6, 34, 3, 3, 5, 6, };
	int i;

	memset(test_buf, 0, sizeof(u32) * NUM_TEST_BUF);
	history_cnt = 0;

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	for (i = 0; i < NUM_TEST_BUF; i++) {
		ret = itf->send_message(test, NULL, NULL, ICPU_MSG_PRIO_NORMAL, 1, &send_buf[i]);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	pablo_icpu_test_itf_close();

	for (i = 0; i < NUM_TEST_BUF; i++)
		KUNIT_EXPECT_EQ(test, test_buf[i], send_buf[i]);
}

static void pablo_icpu_itf_send_message_negative_kunit_test(struct kunit *test)
{
	int ret;
	u32 send_buf[1] = { 4, };
	int i;
	u32 free_msg_cnt;
	struct icpu_itf_msg *msgs[100];

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* no data */
	ret = itf->send_message(test, NULL, NULL, ICPU_MSG_PRIO_NORMAL, 0, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* consume all free message */
	for (i = 0; i < MESSAGE_MAX_COUNT; i++) {
		msgs[i] = get_free_msg_wrap();
		if (!msgs[i])
			break;
		msgs[i]->len = 1;
	}
	free_msg_cnt = get_free_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, free_msg_cnt, (u32)0);

	ret = itf->send_message(test, NULL, NULL, ICPU_MSG_PRIO_NORMAL, 1, send_buf);
	KUNIT_EXPECT_EQ(test, ret, -EBUSY);

	/* restore free messages */
	for (i = 0; i < MESSAGE_MAX_COUNT; i++) {
		if (!msgs[i])
			break;

		set_free_msg_wrap(msgs[i]);
	}
	free_msg_cnt = get_free_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, free_msg_cnt, (u32)MESSAGE_MAX_COUNT);

	pablo_icpu_test_itf_close();
}

static int __send_async_msg_func(void *data)
{
	int ret;
	struct test_vec {
		u32 val;
		enum icpu_msg_priority prio;
	} *send_buf = data;
	struct kunit *test = ctx.test;

	ret = itf->send_message(test, NULL, NULL, send_buf->prio, 1, &send_buf->val);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* wait until thread stop */
	while (!kthread_should_stop())
		msleep(5);

	return 0;
}

static void pablo_icpu_itf_send_message_async_pending_prio_kunit_test(struct kunit *test)
{
	int i;
	int ret;
	struct test_vec {
		u32 val;
		enum icpu_msg_priority prio;
	} send_buf[NUM_TEST_BUF] = {
		{ 7, ICPU_MSG_PRIO_NORMAL, }, /* wait idle in mbox send */
		{ 6, ICPU_MSG_PRIO_LOW, },
		{ 2, ICPU_MSG_PRIO_NORMAL, },
		{ 5, ICPU_MSG_PRIO_HIGH, },
		{ 0, ICPU_MSG_PRIO_NORMAL, },
		{ 4, ICPU_MSG_PRIO_NORMAL, },
		{ 4, ICPU_MSG_PRIO_LOW, },
		{ 7, ICPU_MSG_PRIO_HIGH, }, };
	u32 sol[NUM_TEST_BUF] = { 7, 6, 5, 7, 2, 0, 4, 4, };
	struct task_struct *task_async;
	u32 pending_msg_cnt;

	memset(test_buf, 0, sizeof(u32) * NUM_TEST_BUF);
	history_cnt = 0;

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	wait_idle = true;

	task_async = kthread_run(__send_async_msg_func, (void*)&send_buf[0], "thread_send_async_msg");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, task_async);

	msleep(1);

	for (i = 1; i < NUM_TEST_BUF; i++) {
		ret = itf->send_message(test, NULL, NULL, send_buf[i].prio, 1, &send_buf[i].val);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	pending_msg_cnt = get_pending_msg_cnt_wrap();
	/* pending queue has 6 messages at the moment */
	KUNIT_EXPECT_EQ(test, pending_msg_cnt, 6);

	wait_idle = false;

	kthread_stop(task_async);

	// TODO: confirm flush all pending message

	pablo_icpu_test_itf_close();

	for (i = 0; i < NUM_TEST_BUF; i++)
		KUNIT_EXPECT_EQ(test, test_buf[i], sol[i]);
}

static void pablo_icpu_itf_print_all_message_queue_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	u32 response_msg_cnt;
	int i;
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	for (i = 0; i < 10; i++) {
		msg = get_free_msg_wrap();
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

		msg->data[0] = i;
		msg->data[1] = i;
		msg->cmd.data[0] = msg->data[0];
		msg->cmd.data[1] = msg->data[1];
		msg->len = 2;
		msg->prio = i % ICPU_MSG_PRIO_MAX;

		ret = set_pending_msg_prio_wrap(msg);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	for (i = 0; i < 10; i++) {
		msg = get_free_msg_wrap();
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);

		msg->data[0] = 10 + i;
		msg->data[1] = 10 + i;
		msg->cmd.data[0] = msg->data[0];
		msg->cmd.data[1] = msg->data[1];
		msg->len = 2;
		msg->prio = i % ICPU_MSG_PRIO_MAX;

		ret = set_response_msg_wrap(msg);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* TODO: cmpstr with golden */
	print_all_message_queue();

	response_msg_cnt = get_response_msg_cnt_wrap();
	KUNIT_EXPECT_EQ(test, response_msg_cnt, (u32)10);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void __pablo_icpu_itf_isr_handler(void *sender, void *cookie, u32 *data)
{
	/* To nothing yet */
	struct test_ctx *rx_ctx = sender;
	struct kunit *test = ctx.test;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, rx_ctx);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, data);
	KUNIT_EXPECT_PTR_EQ(test, (void *)test, cookie);

	KUNIT_EXPECT_EQ(test, data[0], (u32)SET_RSP(TO_CMD(4)));
	KUNIT_EXPECT_EQ(test, data[1], (u32)0);
	KUNIT_EXPECT_EQ(test, data[2], (u32)127);
	KUNIT_EXPECT_EQ(test, data[3], (u32)128);
	KUNIT_EXPECT_EQ(test, data[4], (u32)129);
	KUNIT_EXPECT_EQ(test, data[5], (u32)130);
	KUNIT_EXPECT_EQ(test, data[6], (u32)131);
}

static void pablo_icpu_itf_async_msg_response_callback_basic_kunit_test(struct kunit *test)
{
	int ret;
	u32 send_buf[NUM_TEST_BUF] = { TO_CMD(4), };
	u32 rx_buf[NUM_TEST_BUF] = { 4, 127, 128, 129, 130, 131, };

	memset(test_buf, 0, sizeof(u32) * NUM_TEST_BUF);
	history_cnt = 0;

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = itf->send_message(&ctx, test, __pablo_icpu_itf_isr_handler,
			ICPU_MSG_PRIO_NORMAL, 1, send_buf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ctx.cl.rx_callback(&ctx.cl, rx_buf, 6);

	pablo_icpu_test_itf_close();
}

static void __pablo_icpu_itf_isr_err_handler(void *sender, void *cookie, u32 *data)
{
	struct test_ctx *rx_ctx = sender;
	struct kunit *test = ctx.test;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, rx_ctx);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, data);
	KUNIT_EXPECT_PTR_EQ(test, (void *)test, cookie);

	KUNIT_EXPECT_EQ(test, data[0], (u32)TO_CMD(4));
	KUNIT_EXPECT_EQ(test, data[1], (u32)0);
}

/* TODO: pending msg send timeout verify */

static void pablo_icpu_itf_async_msg_send_timeout_kunit_test(struct kunit *test)
{
	int ret;
	u32 send_buf[NUM_TEST_BUF] = { TO_CMD(4), };

	memset(test_buf, 0, sizeof(u32) * NUM_TEST_BUF);
	history_cnt = 0;

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	mbox_timeout = true;

	ret = itf->send_message(&ctx, test, __pablo_icpu_itf_isr_err_handler,
			ICPU_MSG_PRIO_NORMAL, 1, send_buf);
	KUNIT_EXPECT_EQ(test, ret, -ETIMEDOUT);

	mbox_timeout = false;

	pablo_icpu_test_itf_close();
}

static void __ihc_msg_handler(void *cookie, u32 *data)
{
}

static void pablo_icpu_itf_register_msg_handler_kunit_test(struct kunit *test)
{
	int ret;
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	__ihc_msg_handler(NULL, NULL);

	ret = itf->register_msg_handler(NULL, __ihc_msg_handler);
	KUNIT_EXPECT_EQ(test, ret, 0);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();
	pablo_icpu_test_set_platform_data(pdata);
}

static void timer_func(struct timer_list *data)
{
	int ret;
	u32 send_buf[NUM_TEST_BUF] = { 4, };
	struct kunit *test = ctx.test;

	ret = itf->send_message(test, NULL, NULL, ICPU_MSG_PRIO_NORMAL, 1, send_buf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	complete(&timer_done);
}

static void pablo_icpu_itf_send_message_in_interrupt_kunit_test(struct kunit *test)
{
	int ret;

	memset(test_buf, 0, sizeof(u32) * NUM_TEST_BUF);
	history_cnt = 0;

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	init_completion(&timer_done);

	timer_setup(&ctx.timer, (void (*)(struct timer_list *))timer_func, 0);
	mod_timer(&ctx.timer, jiffies + msecs_to_jiffies(5));

	wait_for_completion(&timer_done);

	del_timer(&ctx.timer);

	pablo_icpu_test_itf_close();

	KUNIT_EXPECT_EQ(test, test_buf[0], (u32)4);
}

static void pablo_icpu_itf_wait_boot_complete_kunit_test(struct kunit *test)
{
	int ret;
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	/* expect invalid state */
	ret = itf->wait_boot_complete(10);
	KUNIT_EXPECT_EQ(test, ret, -1);

	ret = itf->open();
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* timeout */
	ret = itf->wait_boot_complete(10);
	KUNIT_EXPECT_EQ(test, ret, -ETIMEDOUT);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);
	itf->close();

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	/* running state */
	ret = itf->wait_boot_complete(10);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_icpu_test_itf_close();

	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_itf_rx_callback_kunit_test(struct kunit *test)
{
	int ret;
	u32 rx_data[8] = { 0, };
	u32 len = 1;

	ret = pablo_icpu_test_itf_open(ctx.cl, &ctx.tx_chan, &ctx.rx_chan);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rx_data[0] = TO_CMD(PABLO_IHC_READY);
	ctx.cl.rx_callback(&ctx.cl, rx_data, len);

	rx_data[0] = TO_CMD(PABLO_IHC_ERROR);
	ctx.cl.rx_callback(&ctx.cl, rx_data, len);

	pablo_icpu_test_itf_close();
}

static void pablo_icpu_itf_preload_firmware_kunit_test(struct kunit *test)
{
	int ret;
	unsigned long flag = 0;

	set_bit(PABLO_ICPU_PRELOAD_FLAG_LOAD, &flag);
	ret = pablo_icpu_itf_preload_firmware(flag);
	KUNIT_ASSERT_EQ(test, ret, 0);

	set_bit(PABLO_ICPU_PRELOAD_FLAG_FORCE_RELEASE, &flag);
	ret = pablo_icpu_itf_preload_firmware(flag);
	KUNIT_ASSERT_EQ(test, ret, 0);
}
/* TODO: cmd overlap testing */
/* TODO: thread safe test */

static struct kunit_case pablo_icpu_itf_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_itf_open_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_shutdown_firmware_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_fsm_open_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_fsm_send_message_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_get_free_message_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_get_pending_message_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_get_pending_message_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_get_response_message_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_get_response_message_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_send_message_async_basic_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_send_message_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_send_message_async_pending_prio_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_print_all_message_queue_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_async_msg_response_callback_basic_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_async_msg_send_timeout_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_register_msg_handler_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_send_message_in_interrupt_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_wait_boot_complete_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_rx_callback_kunit_test),
	KUNIT_CASE(pablo_icpu_itf_preload_firmware_kunit_test),
	{},
};

static struct icpu_mbox_chan_ops __test_tx_ops = {
	.send_data = __pablo_icpu_itf_send_message_kunit_test,
	.startup = NULL,
	.shutdown = NULL,
};

static struct icpu_mbox_chan_ops __test_rx_ops = {
	.send_data = NULL,
	.startup = NULL,
	.shutdown = NULL,
};

static int pablo_icpu_itf_kunit_test_init(struct kunit *test)
{
	void *pdata = pablo_icpu_test_set_platform_data(NULL);

	itf = pablo_icpu_itf_api_get();

	ctx.cl = pablo_icpu_test_itf_get_client();

	ctx.tx_mbox.ops = &__test_tx_ops;
	ctx.tx_mbox.debug_timeout = 500;
	ctx.rx_mbox.ops = &__test_rx_ops;
	ctx.rx_mbox.debug_timeout = 500;

	ctx.tx_chan.cl = &ctx.cl;
	ctx.tx_chan.mbox = &ctx.tx_mbox;

	ctx.rx_chan.cl = &ctx.cl;
	ctx.rx_chan.mbox = &ctx.rx_mbox;

	test->priv = &ctx;
	ctx.test = test;

	memcpy(&test_pdata, pdata, sizeof(struct icpu_platform_data));

	test_pdata.mcuctl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_pdata.mcuctl_reg_base);

	test_pdata.sysctrl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_pdata.sysctrl_reg_base);

	test_pdata.sysreg_reset = NULL;

	pablo_icpu_test_set_platform_data(pdata);

	return 0;
}

static void pablo_icpu_itf_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_pdata.mcuctl_reg_base);
	kunit_kfree(test, test_pdata.sysctrl_reg_base);
}

struct kunit_suite pablo_icpu_itf_kunit_test_suite = {
	.name = "pablo-icpu-itf-kunit-test",
	.init = pablo_icpu_itf_kunit_test_init,
	.exit = pablo_icpu_itf_kunit_test_exit,
	.test_cases = pablo_icpu_itf_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_itf_kunit_test_suite);

MODULE_LICENSE("GPL");
