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

#include "icpu/pablo-icpu-debug.h"
#include "pablo-icpu-log.h"

static struct debug_ctx {
	void *kva;
	size_t log_size;
	size_t margin;
	struct file_operations fops;
} ctx;

static unsigned long __debug_copy_buf(void *to, const void *from, unsigned long n)
{
	memcpy(to, from, n);

	return 0;
}

/* Define the test cases. */

static void pablo_icpu_debug_open_kunit_test(struct kunit *test)
{
	int ret;
	struct inode node;
	struct file file;

	node.i_private = NULL;
	file.private_data = NULL;
	ret = ctx.fops.open(&node, &file);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, node.i_private, NULL);
	KUNIT_EXPECT_PTR_EQ(test, file.private_data, NULL);

	node.i_private = test;
	ret = ctx.fops.open(&node, &file);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, node.i_private, (void *)test);
	KUNIT_EXPECT_PTR_EQ(test, file.private_data, (void *)test);
}

static void pablo_icpu_debug_log_basic_kunit_test(struct kunit *test)
{
	u32 *log_offset = ctx.kva;
	void *log_base_kva = ctx.kva + 4;
	char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
	size_t read_len;
	loff_t ppos = 0;
	char buf[100];
	u32 buf_len;
	int i;

	*log_offset = 25;
	memcpy(log_base_kva, data, 25);

	buf_len = 10;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)buf_len);
	for (i = 0; i < buf_len; i++)
		KUNIT_EXPECT_EQ(test, buf[i], data[i]);

	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)buf_len);
	for (i = 0; i < buf_len; i++)
		KUNIT_EXPECT_EQ(test, buf[i], data[i + 10]);

	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)5);
	for (i = 0; i < 5; i++)
		KUNIT_EXPECT_EQ(test, buf[i], data[i + 20]);
}

static void pablo_icpu_debug_log_no_new_log_kunit_test(struct kunit *test)
{
	u32 *log_offset = ctx.kva;
	void *log_base_kva = ctx.kva + 4;
	char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
	size_t read_len;
	loff_t ppos = 0;
	char buf[100];
	u32 buf_len;

	*log_offset = 0;
	buf_len = 20;
	memcpy(log_base_kva, data, 27);

	/* no new log: but return 1 to continue to read */
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)1);

	*log_offset = 11;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)11);
	KUNIT_EXPECT_EQ(test, buf[0], (char)'A');
	KUNIT_EXPECT_EQ(test, buf[1], (char)'B');
	KUNIT_EXPECT_EQ(test, buf[2], (char)'C');
	KUNIT_EXPECT_EQ(test, buf[3], (char)'D');
	KUNIT_EXPECT_EQ(test, buf[4], (char)'E');
	KUNIT_EXPECT_EQ(test, buf[5], (char)'F');
	KUNIT_EXPECT_EQ(test, buf[6], (char)'G');
	KUNIT_EXPECT_EQ(test, buf[7], (char)'H');
	KUNIT_EXPECT_EQ(test, buf[8], (char)'I');
	KUNIT_EXPECT_EQ(test, buf[9], (char)'J');
	KUNIT_EXPECT_EQ(test, buf[10], (char)'K');

	/* no new log: but return 1 to continue to read */
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)1);
}

static void pablo_icpu_debug_log_overlap_kunit_test(struct kunit *test)
{
	u32 *log_offset = ctx.kva;
	void *log_base_kva = ctx.kva + 4;
	char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
	size_t read_len;
	loff_t ppos = 0;
	char buf[100];
	u32 buf_len;

	buf_len = 100;
	memcpy(log_base_kva, data, 27);

	*log_offset = 11;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)11);
	KUNIT_EXPECT_EQ(test, buf[0], (char)'A');
	KUNIT_EXPECT_EQ(test, buf[1], (char)'B');
	KUNIT_EXPECT_EQ(test, buf[2], (char)'C');
	KUNIT_EXPECT_EQ(test, buf[3], (char)'D');
	KUNIT_EXPECT_EQ(test, buf[4], (char)'E');
	KUNIT_EXPECT_EQ(test, buf[5], (char)'F');
	KUNIT_EXPECT_EQ(test, buf[6], (char)'G');
	KUNIT_EXPECT_EQ(test, buf[7], (char)'H');
	KUNIT_EXPECT_EQ(test, buf[8], (char)'I');
	KUNIT_EXPECT_EQ(test, buf[9], (char)'J');
	KUNIT_EXPECT_EQ(test, buf[10], (char)'K');

	*log_offset = (1 << 31) | 5;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)21);
	KUNIT_EXPECT_EQ(test, buf[0], (char)'L');
	KUNIT_EXPECT_EQ(test, buf[1], (char)'M');
	KUNIT_EXPECT_EQ(test, buf[2], (char)'N');
	KUNIT_EXPECT_EQ(test, buf[3], (char)'O');
	KUNIT_EXPECT_EQ(test, buf[4], (char)'P');
	KUNIT_EXPECT_EQ(test, buf[5], (char)'Q');
	KUNIT_EXPECT_EQ(test, buf[6], (char)'R');
	KUNIT_EXPECT_EQ(test, buf[7], (char)'S');
	KUNIT_EXPECT_EQ(test, buf[8], (char)'T');
	KUNIT_EXPECT_EQ(test, buf[9], (char)'U');
	KUNIT_EXPECT_EQ(test, buf[10], (char)'V');
	KUNIT_EXPECT_EQ(test, buf[11], (char)'W');
	KUNIT_EXPECT_EQ(test, buf[12], (char)'X');
	KUNIT_EXPECT_EQ(test, buf[13], (char)'Y');
	KUNIT_EXPECT_EQ(test, buf[14], (char)'Z');
	KUNIT_EXPECT_EQ(test, buf[15], (char)'\n');
	KUNIT_EXPECT_EQ(test, buf[16], (char)'A');
	KUNIT_EXPECT_EQ(test, buf[17], (char)'B');
	KUNIT_EXPECT_EQ(test, buf[18], (char)'C');
	KUNIT_EXPECT_EQ(test, buf[19], (char)'D');
	KUNIT_EXPECT_EQ(test, buf[20], (char)'E');
}

static void pablo_icpu_debug_log_overlap2_kunit_test(struct kunit *test)
{
	u32 *log_offset = ctx.kva;
	void *log_base_kva = ctx.kva + 4;
	char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
	size_t read_len;
	loff_t ppos = 0;
	char buf[100];
	u32 buf_len;

	buf_len = 100;
	memcpy(log_base_kva, data, 27);

	*log_offset = (1 << 31) | 11;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)22);

	/* start from offset + overlap margin(we asume that margin is 5) */
	KUNIT_EXPECT_EQ(test, buf[0], (char)'Q');
	KUNIT_EXPECT_EQ(test, buf[1], (char)'R');
	KUNIT_EXPECT_EQ(test, buf[2], (char)'S');
	KUNIT_EXPECT_EQ(test, buf[3], (char)'T');
	KUNIT_EXPECT_EQ(test, buf[4], (char)'U');
	KUNIT_EXPECT_EQ(test, buf[5], (char)'V');
	KUNIT_EXPECT_EQ(test, buf[6], (char)'W');
	KUNIT_EXPECT_EQ(test, buf[7], (char)'X');
	KUNIT_EXPECT_EQ(test, buf[8], (char)'Y');
	KUNIT_EXPECT_EQ(test, buf[9], (char)'Z');
	KUNIT_EXPECT_EQ(test, buf[10], (char)'\n');
	KUNIT_EXPECT_EQ(test, buf[11], (char)'A');
	KUNIT_EXPECT_EQ(test, buf[12], (char)'B');
	KUNIT_EXPECT_EQ(test, buf[13], (char)'C');
	KUNIT_EXPECT_EQ(test, buf[14], (char)'D');
	KUNIT_EXPECT_EQ(test, buf[15], (char)'E');
	KUNIT_EXPECT_EQ(test, buf[16], (char)'F');
	KUNIT_EXPECT_EQ(test, buf[17], (char)'G');
	KUNIT_EXPECT_EQ(test, buf[18], (char)'H');
	KUNIT_EXPECT_EQ(test, buf[19], (char)'I');
	KUNIT_EXPECT_EQ(test, buf[20], (char)'J');
	KUNIT_EXPECT_EQ(test, buf[21], (char)'K');
}

static void pablo_icpu_debug_log_edge_case_kunit_test(struct kunit *test)
{
	u32 *log_offset = ctx.kva;
	void *log_base_kva = ctx.kva + 4;
	char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
	size_t read_len;
	loff_t ppos = 0;
	char buf[100];
	u32 buf_len;

	*log_offset = 27;
	memcpy(log_base_kva, data, 27);

	buf_len = 30;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)27);
	KUNIT_EXPECT_EQ(test, buf[0], (char)'A');
	KUNIT_EXPECT_EQ(test, buf[1], (char)'B');
	KUNIT_EXPECT_EQ(test, buf[2], (char)'C');
	KUNIT_EXPECT_EQ(test, buf[3], (char)'D');
	KUNIT_EXPECT_EQ(test, buf[4], (char)'E');
	KUNIT_EXPECT_EQ(test, buf[5], (char)'F');
	KUNIT_EXPECT_EQ(test, buf[6], (char)'G');
	KUNIT_EXPECT_EQ(test, buf[7], (char)'H');
	KUNIT_EXPECT_EQ(test, buf[8], (char)'I');
	KUNIT_EXPECT_EQ(test, buf[9], (char)'J');
	KUNIT_EXPECT_EQ(test, buf[10], (char)'K');
	KUNIT_EXPECT_EQ(test, buf[11], (char)'L');
	KUNIT_EXPECT_EQ(test, buf[12], (char)'M');
	KUNIT_EXPECT_EQ(test, buf[13], (char)'N');
	KUNIT_EXPECT_EQ(test, buf[14], (char)'O');
	KUNIT_EXPECT_EQ(test, buf[15], (char)'P');
	KUNIT_EXPECT_EQ(test, buf[16], (char)'Q');
	KUNIT_EXPECT_EQ(test, buf[17], (char)'R');
	KUNIT_EXPECT_EQ(test, buf[18], (char)'S');
	KUNIT_EXPECT_EQ(test, buf[19], (char)'T');
	KUNIT_EXPECT_EQ(test, buf[20], (char)'U');
	KUNIT_EXPECT_EQ(test, buf[21], (char)'V');
	KUNIT_EXPECT_EQ(test, buf[22], (char)'W');
	KUNIT_EXPECT_EQ(test, buf[23], (char)'X');
	KUNIT_EXPECT_EQ(test, buf[24], (char)'Y');
	KUNIT_EXPECT_EQ(test, buf[25], (char)'Z');
	KUNIT_EXPECT_EQ(test, buf[26], (char)'\n');

	/* we assume that the debug log_size is set to 27 */
	*log_offset = (1 << 31) | 5;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)5);
	KUNIT_EXPECT_EQ(test, buf[0], (char)'A');
	KUNIT_EXPECT_EQ(test, buf[1], (char)'B');
	KUNIT_EXPECT_EQ(test, buf[2], (char)'C');
	KUNIT_EXPECT_EQ(test, buf[3], (char)'D');
	KUNIT_EXPECT_EQ(test, buf[4], (char)'E');

	*log_offset = (1 << 31) | 6;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)1);
	KUNIT_EXPECT_EQ(test, buf[0], (char)'F');
}

static void pablo_icpu_debug_log_overlap_edge_case_kunit_test(struct kunit *test)
{
	u32 *log_offset = ctx.kva;
	void *log_base_kva = ctx.kva + 4;
	char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
	size_t read_len;
	loff_t ppos;
	char buf[100];
	u32 buf_len;

	buf_len = 100;
	memcpy(log_base_kva, data, 27);

	*log_offset = (1 << 31) | 25;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)22);

	/* Start from offset + overlap margin
	   We asume that margin is 5 and debug log_size is 27
	   In this case the start offset is exceed log_size.
	   So expectation is starting with 'D'-the 4th of data
	 */
	KUNIT_EXPECT_EQ(test, buf[0], (char)'D');
	KUNIT_EXPECT_EQ(test, buf[1], (char)'E');
	KUNIT_EXPECT_EQ(test, buf[2], (char)'F');
	KUNIT_EXPECT_EQ(test, buf[3], (char)'G');
	KUNIT_EXPECT_EQ(test, buf[4], (char)'H');
	KUNIT_EXPECT_EQ(test, buf[5], (char)'I');
	KUNIT_EXPECT_EQ(test, buf[6], (char)'J');
	KUNIT_EXPECT_EQ(test, buf[7], (char)'K');
	KUNIT_EXPECT_EQ(test, buf[8], (char)'L');
	KUNIT_EXPECT_EQ(test, buf[9], (char)'M');
	KUNIT_EXPECT_EQ(test, buf[10], (char)'N');
	KUNIT_EXPECT_EQ(test, buf[11], (char)'O');
	KUNIT_EXPECT_EQ(test, buf[12], (char)'P');
	KUNIT_EXPECT_EQ(test, buf[13], (char)'Q');
	KUNIT_EXPECT_EQ(test, buf[14], (char)'R');
	KUNIT_EXPECT_EQ(test, buf[15], (char)'S');
	KUNIT_EXPECT_EQ(test, buf[16], (char)'T');
	KUNIT_EXPECT_EQ(test, buf[17], (char)'U');
	KUNIT_EXPECT_EQ(test, buf[18], (char)'V');
	KUNIT_EXPECT_EQ(test, buf[19], (char)'W');
	KUNIT_EXPECT_EQ(test, buf[20], (char)'X');
	KUNIT_EXPECT_EQ(test, buf[21], (char)'Y');
}

static void pablo_icpu_debug_log_overlap_edge_case2_kunit_test(struct kunit *test)
{
	u32 *log_offset = ctx.kva;
	void *log_base_kva = ctx.kva + 4;
	char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
	size_t read_len;
	loff_t ppos = 0;
	char buf[100];
	u32 buf_len;

	buf_len = 100;
	memcpy(log_base_kva, data, 27);

	*log_offset = (1 << 31) | 22;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)22);

	/* Start from offset + overlap margin
	   We asume that margin is 5 and debug log_size is 27
	   In this case the start offset is same as log_size.
	   So expectation is starting with the 1st of data
	 */
	KUNIT_EXPECT_EQ(test, buf[0], (char)'A');
	KUNIT_EXPECT_EQ(test, buf[1], (char)'B');
	KUNIT_EXPECT_EQ(test, buf[2], (char)'C');
	KUNIT_EXPECT_EQ(test, buf[3], (char)'D');
	KUNIT_EXPECT_EQ(test, buf[4], (char)'E');
	KUNIT_EXPECT_EQ(test, buf[5], (char)'F');
	KUNIT_EXPECT_EQ(test, buf[6], (char)'G');
	KUNIT_EXPECT_EQ(test, buf[7], (char)'H');
	KUNIT_EXPECT_EQ(test, buf[8], (char)'I');
	KUNIT_EXPECT_EQ(test, buf[9], (char)'J');
	KUNIT_EXPECT_EQ(test, buf[10], (char)'K');
	KUNIT_EXPECT_EQ(test, buf[11], (char)'L');
	KUNIT_EXPECT_EQ(test, buf[12], (char)'M');
	KUNIT_EXPECT_EQ(test, buf[13], (char)'N');
	KUNIT_EXPECT_EQ(test, buf[14], (char)'O');
	KUNIT_EXPECT_EQ(test, buf[15], (char)'P');
	KUNIT_EXPECT_EQ(test, buf[16], (char)'Q');
	KUNIT_EXPECT_EQ(test, buf[17], (char)'R');
	KUNIT_EXPECT_EQ(test, buf[18], (char)'S');
	KUNIT_EXPECT_EQ(test, buf[19], (char)'T');
	KUNIT_EXPECT_EQ(test, buf[20], (char)'U');
	KUNIT_EXPECT_EQ(test, buf[21], (char)'V');
}

static void pablo_icpu_debug_log_kmsg_kunit_test(struct kunit *test)
{
	u32 loglevel = pablo_icpu_debug_test_loglevel(LOGLEVEL_CUSTOM1);
	u32 *log_offset = ctx.kva;
	void *log_base_kva = ctx.kva + 4;
	char *data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
	size_t read_len;
	loff_t ppos = 0;
	char buf[100];
	u32 buf_len;

	buf_len = 100;
	memcpy(log_base_kva, data, 27);

	*log_offset = 5;
	read_len = ctx.fops.read(NULL, buf, buf_len, &ppos);
	KUNIT_EXPECT_EQ(test, read_len, (size_t)1);

	pablo_icpu_debug_test_loglevel(loglevel);
}

static void pablo_icpu_debug_fw_log_dump_kunit_test(struct kunit *test)
{
	u32 loglevel = pablo_icpu_debug_test_loglevel(LOGLEVEL_CUSTOM1);

	pablo_icpu_debug_test_loglevel(loglevel);

	pablo_icpu_debug_fw_log_dump();

	KUNIT_EXPECT_EQ(test, pablo_icpu_debug_test_loglevel(loglevel), (int)loglevel);
}

static struct kunit_case pablo_icpu_debug_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_debug_open_kunit_test),
	KUNIT_CASE(pablo_icpu_debug_log_basic_kunit_test),
	KUNIT_CASE(pablo_icpu_debug_log_no_new_log_kunit_test),
	KUNIT_CASE(pablo_icpu_debug_log_overlap_kunit_test),
	KUNIT_CASE(pablo_icpu_debug_log_overlap2_kunit_test),
	KUNIT_CASE(pablo_icpu_debug_log_edge_case_kunit_test),
	KUNIT_CASE(pablo_icpu_debug_log_overlap_edge_case_kunit_test),
	KUNIT_CASE(pablo_icpu_debug_log_overlap_edge_case2_kunit_test),
	KUNIT_CASE(pablo_icpu_debug_log_kmsg_kunit_test),
	KUNIT_CASE(pablo_icpu_debug_fw_log_dump_kunit_test),
	{},
};

static int pablo_icpu_debug_kunit_test_init(struct kunit *test)
{
	int  ret;
	ctx.kva = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx.kva);

	ctx.log_size = 27;
	ctx.margin = 5;

	ret = pablo_icpu_debug_test_config(ctx.kva, ctx.log_size, ctx.margin, __debug_copy_buf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pablo_icpu_test_get_file_ops(&ctx.fops);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx.fops.open);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx.fops.read);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx.fops.llseek);

	return 0;
}

static void pablo_icpu_debug_kunit_test_exit(struct kunit *test)
{
	pablo_icpu_debug_reset();
	kunit_kfree(test, ctx.kva);
}

struct kunit_suite pablo_icpu_debug_kunit_test_suite = {
	.name = "pablo-icpu-debug-kunit-test",
	.init = pablo_icpu_debug_kunit_test_init,
	.exit = pablo_icpu_debug_kunit_test_exit,
	.test_cases = pablo_icpu_debug_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_debug_kunit_test_suite);

MODULE_LICENSE("GPL");
