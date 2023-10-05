// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/hardirq.h>

#include "pablo-kunit-test.h"
#include "pablo-work.h"

static struct pablo_kunit_test_ctx {
	struct is_work_list *work_list;
} pkt_ctx;

/* Define the test cases. */
static void pablo_work_kunit_test(struct kunit *test)
{
	struct is_work_list *work_list = pkt_ctx.work_list;
	struct is_work *work1, *work2;
	int ret, id;

	/* TC #1. Get FREE work with NULL pointer */
	ret = get_free_work(work_list, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)1);

	/* TC #2. Get FREE work */
	ret = get_free_work(work_list, &work1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, work1);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)0);

	/* TC #3. Get FREE work, again */
	ret = get_free_work(work_list, &work2);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)work2, NULL);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)0);

	/* TC #4. Set REQ work with NULL pointer */
	ret = set_req_work(work_list, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);
	KUNIT_EXPECT_EQ(test, work_list->work_request_cnt, (u32)0);

	/* TC #5. Set REQ work */
	ret = set_req_work(work_list, work1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, work_list->work_request_cnt, (u32)1);

	/* TC #6. Get REQ work with NULL pointer */
	ret = get_req_work(work_list, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);
	KUNIT_EXPECT_EQ(test, work_list->work_request_cnt, (u32)1);

	/* TC #7. Print REQ work list */
	ret = print_req_work_list(work_list);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, work_list->work_request_cnt, (u32)1);

	/* TC #8. Print REQ work list with invalid id */
	id = work_list->id;
	work_list->id = WORK_MAX_MAP;
	ret = print_req_work_list(work_list);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, work_list->work_request_cnt, (u32)1);

	/* TC #9. Get REQ work */
	work_list->id = id;
	work1 = NULL;
	ret = get_req_work(work_list, &work1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, work1);
	KUNIT_EXPECT_EQ(test, work_list->work_request_cnt, (u32)0);

	/* TC #10. Get REQ work, again */
	ret = get_req_work(work_list, &work2);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)work2, NULL);
	KUNIT_EXPECT_EQ(test, work_list->work_request_cnt, (u32)0);

	/* TC #11. Set FREE work with NULL pointer */
	ret = set_free_work(work_list, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)0);

	/* TC #12. Set FREE work */
	ret = set_free_work(work_list, work1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)1);

	/* TC #13. Print FREE work list */
	ret = print_fre_work_list(work_list);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)1);

	/* TC #14. Print FREE work list with invalid id */
	id = work_list->id;
	work_list->id = WORK_MAX_MAP;
	ret = print_fre_work_list(work_list);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)1);
}

static void pablo_work_irq_kunit_test(struct kunit *test)
{
	struct is_work_list *work_list = pkt_ctx.work_list;
	struct is_work *work1, *work2;
	int ret;

	__irq_enter_raw();

	/* TC #1. Get FREE work with NULL pointer */
	ret = get_free_work(work_list, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)1);

	/* TC #2. Get FREE work */
	ret = get_free_work(work_list, &work1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, work1);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)0);

	/* TC #3. Get FREE work, again */
	ret = get_free_work(work_list, &work2);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, (void *)work2, NULL);
	KUNIT_EXPECT_EQ(test, work_list->work_free_cnt, (u32)0);

	/* TC #4. Set REQ work with NULL pointer */
	ret = set_req_work(work_list, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);
	KUNIT_EXPECT_EQ(test, work_list->work_request_cnt, (u32)0);

	/* TC #5. Set REQ work */
	ret = set_req_work(work_list, work1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, work_list->work_request_cnt, (u32)1);

	__irq_exit_raw();
}
/* End of test cases definition. */

static int pablo_work_kunit_test_init(struct kunit *test)
{
	struct is_work_list *work_list;
	u32 id;

	work_list = (struct is_work_list *)kunit_kmalloc(test, sizeof(struct is_work_list), 0);
	id = __LINE__ % WORK_MAX_MAP;

	init_work_list(work_list, id, 1);
	KUNIT_ASSERT_EQ(test, work_list->id, id);
	KUNIT_ASSERT_EQ(test, work_list->work_free_cnt, (u32)1);

	pkt_ctx.work_list = work_list;

	return 0;
}

static void pablo_work_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, (void *)pkt_ctx.work_list);
}

static struct kunit_case pablo_work_kunit_test_cases[] = {
	KUNIT_CASE(pablo_work_kunit_test),
	KUNIT_CASE(pablo_work_irq_kunit_test),
	{},
};

struct kunit_suite pablo_work_kunit_test_suite = {
	.name = "pablo-work-kunit-test",
	.init = pablo_work_kunit_test_init,
	.exit = pablo_work_kunit_test_exit,
	.test_cases = pablo_work_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_work_kunit_test_suite);

MODULE_LICENSE("GPL");
