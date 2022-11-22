/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/fs.h>
#include <linux/defex.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>

static void defex_syscall_enter_test(struct test *test)
{
	struct pt_regs *regs = task_pt_regs(current);

	EXPECT_EQ(test, defex_syscall_enter(__NR_syscalls + 1, regs), 0);
#ifdef __NR_write
	EXPECT_EQ(test, defex_syscall_enter(__NR_write, regs), 0);
#endif
}


static void defex_syscall_catch_enter_test(struct test *test)
{
	/* EXPECT_EQ(test, 1, defex_syscall_catch_enter()); */
}


static void defex_lsm_init_test(struct test *test)
{
	/* __init function */
	SUCCEED(test);
}

static void defex_lsm_load_test(struct test *test)
{
	/* __init function */
	SUCCEED(test);
}

static int defex_lsm_test_init(struct test *test)
{
	return 0;
}

static void defex_lsm_test_exit(struct test *test)
{
}

static struct test_case defex_lsm_test_cases[] = {
	/* TEST FUNC DEFINES */
	TEST_CASE(defex_syscall_enter_test),
	TEST_CASE(defex_syscall_catch_enter_test),
	TEST_CASE(defex_lsm_init_test),
	TEST_CASE(defex_lsm_load_test),
	{},
};

static struct test_module defex_lsm_test_module = {
	.name = "defex_lsm_test",
	.init = defex_lsm_test_init,
	.exit = defex_lsm_test_exit,
	.test_cases = defex_lsm_test_cases,
};
module_test(defex_lsm_test_module);

