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

#include "pablo-kunit-test.h"
#include "icpu/pablo-icpu-msg-queue.h"
#include "pablo-icpu-itf.h"

static struct icpu_msg_queue _single_queue;
static struct icpu_msg_queue *single_queue;
static struct icpu_msg_queue _priority_queue;
static struct icpu_msg_queue *priority_queue;

#define TEST_MAX_MSG 32
static struct icpu_itf_msg msgs[TEST_MAX_MSG];

static void pablo_icpu_msg_queue_init_kunit_test(struct kunit *test)
{
	int ret;

	ret = QUEUE_INIT(single_queue);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, single_queue->num_priority, (u32)1);
	KUNIT_EXPECT_EQ(test, single_queue->msg_cnt, (u32)0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, single_queue->msg_list);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, single_queue->get_msg);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, single_queue->get_msg_key);
	KUNIT_EXPECT_PTR_EQ(test, (void *)single_queue->get_msg_prio, NULL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, single_queue->len);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, single_queue->dump);

	ret = PRIORITY_QUEUE_INIT(priority_queue, ICPU_MSG_PRIO_MAX);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, priority_queue->num_priority, (u32)ICPU_MSG_PRIO_MAX);
	KUNIT_EXPECT_EQ(test, priority_queue->msg_cnt, (u32)0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, priority_queue->msg_list);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, priority_queue->get_msg);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, priority_queue->get_msg_key);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, priority_queue->get_msg_prio);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, priority_queue->len);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, priority_queue->dump);

	QUEUE_DEINIT(single_queue);
	KUNIT_EXPECT_EQ(test, single_queue->num_priority, (u32)0);
	KUNIT_EXPECT_EQ(test, single_queue->msg_cnt, (u32)0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)single_queue->msg_list, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)single_queue->get_msg, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)single_queue->get_msg_key, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)single_queue->get_msg_prio, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)single_queue->len, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)single_queue->dump, NULL);

	QUEUE_DEINIT(priority_queue);
	KUNIT_EXPECT_EQ(test, priority_queue->num_priority, (u32)0);
	KUNIT_EXPECT_EQ(test, priority_queue->msg_cnt, (u32)0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)priority_queue->msg_list, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)priority_queue->get_msg, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)priority_queue->get_msg_key, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)priority_queue->get_msg_prio, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)priority_queue->len, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)priority_queue->dump, NULL);
}

static void pablo_icpu_msg_queue_single_basic_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	int idx = 0;

	ret = QUEUE_INIT(single_queue);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)0);

	msg = QUEUE_GET_MSG(single_queue);
	KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);

	msgs[idx].data[0] = 0xBABEFACE;
	QUEUE_SET_MSG(single_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)1);

	msg = QUEUE_GET_MSG(single_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)0);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xBABEFACE);

	QUEUE_DEINIT(single_queue);
}

static void pablo_icpu_msg_queue_priority_basic_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	int idx = 0;

	ret = PRIORITY_QUEUE_INIT(priority_queue, ICPU_MSG_PRIO_MAX);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)0);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);

	msgs[idx].data[0] = 0xFF1;
	msgs[idx].prio = ICPU_MSG_PRIO_HIGH;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)1);

	idx++;
	msgs[idx].data[0] = 0xFF2;
	msgs[idx].prio = ICPU_MSG_PRIO_HIGH;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)2);

	idx++;
	msgs[idx].data[0] = 0xFF3;
	msgs[idx].prio = ICPU_MSG_PRIO_NORMAL;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)3);

	idx++;
	msgs[idx].data[0] = 0xFF4;
	msgs[idx].prio = ICPU_MSG_PRIO_LOW;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)4);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)3);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF1);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)2);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF2);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)1);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF3);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)0);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF4);

	QUEUE_DEINIT(priority_queue);
}

static void pablo_icpu_msg_queue_priority_basic2_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	int idx = 0;

	ret = PRIORITY_QUEUE_INIT(priority_queue, ICPU_MSG_PRIO_MAX);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)0);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);


	msgs[idx].data[0] = 0xFF1;
	msgs[idx].prio = ICPU_MSG_PRIO_LOW;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)1);

	idx++;
	msgs[idx].data[0] = 0xFF2;
	msgs[idx].prio = ICPU_MSG_PRIO_NORMAL;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)2);

	idx++;
	msgs[idx].data[0] = 0xFF3;
	msgs[idx].prio = ICPU_MSG_PRIO_HIGH;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)3);

	idx++;
	msgs[idx].data[0] = 0xFF4;
	msgs[idx].prio = ICPU_MSG_PRIO_HIGH;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)4);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)3);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF3);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)2);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF4);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)1);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF2);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)0);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF1);

	QUEUE_DEINIT(priority_queue);
}

static void pablo_icpu_msg_queue_priority_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;

	ret = PRIORITY_QUEUE_INIT(priority_queue, ICPU_MSG_PRIO_MAX);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)0);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);

	/* invalid priority case */
	msg = priority_queue->get_msg_prio(priority_queue, ICPU_MSG_PRIO_MAX);
	KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);

	QUEUE_DEINIT(priority_queue);
}

static void pablo_icpu_msg_queue_single_get_key_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	int idx = 0;

	ret = QUEUE_INIT(single_queue);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)0);

	msg = QUEUE_GET_MSG(single_queue);
	KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);

	msgs[idx].data[0] = 0xFF1;
	msgs[idx].cmd.key = msgs[idx].data[0];
	QUEUE_SET_MSG(single_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)1);

	idx++;
	msgs[idx].data[0] = 0xFF2;
	msgs[idx].cmd.key = msgs[idx].data[0];
	QUEUE_SET_MSG(single_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)2);

	idx++;
	msgs[idx].data[0] = 0xFF3;
	msgs[idx].cmd.key = msgs[idx].data[0];
	QUEUE_SET_MSG(single_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)3);

	/* matching key */
	msg = QUEUE_GET_MSG_KEY(single_queue, 0xFF2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)2);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF2);

	/* no msg with matching key */
	msg = QUEUE_GET_MSG_KEY(single_queue, 0xFF2);
	KUNIT_EXPECT_PTR_EQ(test, (void*)msg, NULL);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)2);

	QUEUE_DEINIT(single_queue);
}

static void pablo_icpu_msg_queue_priority_get_key_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	int idx = 0;

	ret = PRIORITY_QUEUE_INIT(priority_queue, ICPU_MSG_PRIO_MAX);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)0);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);


	msgs[idx].data[0] = 0xFF1;
	msgs[idx].cmd.key = msgs[idx].data[0];
	msgs[idx].prio = ICPU_MSG_PRIO_LOW;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)1);

	idx++;
	msgs[idx].data[0] = 0xFF2;
	msgs[idx].cmd.key = msgs[idx].data[0];
	msgs[idx].prio = ICPU_MSG_PRIO_NORMAL;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)2);

	idx++;
	msgs[idx].data[0] = 0xFF3;
	msgs[idx].cmd.key = msgs[idx].data[0];
	msgs[idx].prio = ICPU_MSG_PRIO_HIGH;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)3);

	idx++;
	msgs[idx].data[0] = 0xFF4;
	msgs[idx].cmd.key = msgs[idx].data[0];
	msgs[idx].prio = ICPU_MSG_PRIO_HIGH;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)4);

	/* matching key */
	msg = QUEUE_GET_MSG_KEY(priority_queue, 0xFF2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)3);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF2);

	/* no msg with matching key */
	msg = QUEUE_GET_MSG_KEY(priority_queue, 0xFF2);
	KUNIT_EXPECT_PTR_EQ(test, (void*)msg, NULL);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)3);

	QUEUE_DEINIT(priority_queue);
}

static void pablo_icpu_msg_queue_single_not_empty_set_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	int idx = 0;

	ret = QUEUE_INIT(single_queue);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)0);

	msg = QUEUE_GET_MSG(single_queue);
	KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);

	/* msg is not set due to queue is empty */
	msgs[idx].data[0] = 0xBABEFACE;
	ret = QUEUE_SET_MSG_IF_NOT_EMPTY(single_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)0);

	/* set msg */
	QUEUE_SET_MSG(single_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)1);

	/* msg can set due to queue is not empty */
	idx++;
	msgs[idx].data[0] = 0xBABE2;
	ret = QUEUE_SET_MSG_IF_NOT_EMPTY(single_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)2);

	msg = QUEUE_GET_MSG(single_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)1);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xBABEFACE);

	msg = QUEUE_GET_MSG(single_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(single_queue), (u32)0);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xBABE2);

	QUEUE_DEINIT(single_queue);
}

static void pablo_icpu_msg_queue_priority_not_empty_set_kunit_test(struct kunit *test)
{
	int ret;
	struct icpu_itf_msg *msg;
	int idx = 0;

	ret = PRIORITY_QUEUE_INIT(priority_queue, ICPU_MSG_PRIO_MAX);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)0);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_EXPECT_PTR_EQ(test, (void *)msg, NULL);

	/* msg is not set due to queue is empty */
	msgs[idx].data[0] = 0xFF1;
	msgs[idx].prio = ICPU_MSG_PRIO_LOW;
	ret = QUEUE_SET_MSG_IF_NOT_EMPTY(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)0);

	/* set msg */
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)1);

	/* msg can set due to queue is not empty */
	idx++;
	msgs[idx].data[0] = 0xFF2;
	msgs[idx].prio = ICPU_MSG_PRIO_NORMAL;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)2);

	idx++;
	msgs[idx].data[0] = 0xFF3;
	msgs[idx].prio = ICPU_MSG_PRIO_HIGH;
	ret = QUEUE_SET_MSG_IF_NOT_EMPTY(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, ret, 1);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)3);

	idx++;
	msgs[idx].data[0] = 0xFF4;
	msgs[idx].prio = ICPU_MSG_PRIO_HIGH;
	QUEUE_SET_MSG(priority_queue, &msgs[idx]);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)4);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)3);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF3);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)2);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF4);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)1);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF2);

	msg = QUEUE_GET_MSG(priority_queue);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, msg);
	KUNIT_EXPECT_EQ(test, QUEUE_LEN(priority_queue), (u32)0);
	KUNIT_EXPECT_EQ(test, msg->data[0], (u32)0xFF1);

	QUEUE_DEINIT(priority_queue);
}

static struct kunit_case pablo_icpu_msg_queue_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_msg_queue_init_kunit_test),
	KUNIT_CASE(pablo_icpu_msg_queue_single_basic_kunit_test),
	KUNIT_CASE(pablo_icpu_msg_queue_priority_basic_kunit_test),
	KUNIT_CASE(pablo_icpu_msg_queue_priority_basic2_kunit_test),
	KUNIT_CASE(pablo_icpu_msg_queue_priority_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_msg_queue_single_get_key_kunit_test),
	KUNIT_CASE(pablo_icpu_msg_queue_priority_get_key_kunit_test),
	KUNIT_CASE(pablo_icpu_msg_queue_single_not_empty_set_kunit_test),
	KUNIT_CASE(pablo_icpu_msg_queue_priority_not_empty_set_kunit_test),
	{},
};

static int pablo_icpu_msg_queue_kunit_test_init(struct kunit *test)
{
	single_queue = &_single_queue;
	priority_queue = &_priority_queue;

	return 0;
}

static void pablo_icpu_msg_queue_kunit_test_exit(struct kunit *test)
{
	/* Check TC release all resources */
	KUNIT_EXPECT_PTR_EQ(test, (void *)single_queue->msg_list, NULL);
	KUNIT_EXPECT_PTR_EQ(test, (void *)priority_queue->msg_list, NULL);
}

struct kunit_suite pablo_icpu_msg_queue_kunit_test_suite = {
	.name = "pablo-icpu-msg-queue-kunit-test",
	.init = pablo_icpu_msg_queue_kunit_test_init,
	.exit = pablo_icpu_msg_queue_kunit_test_exit,
	.test_cases = pablo_icpu_msg_queue_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_msg_queue_kunit_test_suite);

MODULE_LICENSE("GPL");
