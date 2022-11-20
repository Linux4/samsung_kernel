/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/uaccess.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "include/defex_caches.h"
#include "include/defex_catch_list.h"
#include "include/defex_config.h"
#include "include/defex_internal.h"
#include "include/defex_rules.h"
#include "include/defex_test.h"

#define ROOT_PATH "/"

static int kunit_mock_thread_function()
{
	while (!kthread_should_stop());
	return 42;
}

/* General test functions created by Generate_KUnit.sh */

static void release_defex_context_test(struct test *test)
{
	struct defex_context dc;

	init_defex_context(&dc, __DEFEX_execve, current, NULL);
	release_defex_context(&dc);
}


static void local_fread_test(struct test *test)
{
	struct file *f;
	loff_t offset = 0;

	f = local_fopen(ROOT_PATH, O_RDONLY, 0);
	ASSERT_FALSE(test, IS_ERR_OR_NULL(f));
	EXPECT_EQ(test, local_fread(f, offset, NULL, 0), -EISDIR);
	filp_close(f, NULL);

	/* Missing test case in which a file is opened for read without error */
}


static void local_fopen_test(struct test *test)
{
	struct file *f;

	f = local_fopen(ROOT_PATH, O_RDONLY, 0);
	EXPECT_FALSE(test, IS_ERR_OR_NULL(f));
	if (!IS_ERR_OR_NULL(f))
		filp_close(f, NULL);
}


static void init_defex_context_test(struct test *test)
{
	struct defex_context dc;
	struct task_struct *mock_task;

	mock_task = kthread_run(kunit_mock_thread_function, NULL, "defex_common_test_thread");
	get_task_struct(mock_task);
	ssleep(1);

	init_defex_context(&dc, __DEFEX_execve, mock_task, NULL);
	EXPECT_EQ(test, dc.syscall_no, __DEFEX_execve);
	EXPECT_EQ(test, dc.task, mock_task);
	EXPECT_EQ(test, dc.process_file, NULL);
	EXPECT_EQ(test, dc.process_dpath, NULL);
	EXPECT_EQ(test, dc.process_name, NULL);
	EXPECT_EQ(test, dc.target_file, NULL);
	EXPECT_EQ(test, dc.target_dpath, NULL);
	EXPECT_EQ(test, dc.target_name, NULL);
	EXPECT_EQ(test, dc.process_name_buff, NULL);
	EXPECT_EQ(test, dc.target_name_buff, NULL);
	EXPECT_EQ(test, dc.cred.uid.val, mock_task->cred->uid.val);
	EXPECT_EQ(test, dc.cred.gid.val, mock_task->cred->gid.val);
	EXPECT_EQ(test, dc.cred.suid.val, mock_task->cred->suid.val);
	EXPECT_EQ(test, dc.cred.sgid.val, mock_task->cred->sgid.val);
	EXPECT_EQ(test, dc.cred.euid.val, mock_task->cred->euid.val);
	EXPECT_EQ(test, dc.cred.egid.val, mock_task->cred->egid.val);
	EXPECT_EQ(test, dc.cred.fsuid.val, mock_task->cred->fsuid.val);
	EXPECT_EQ(test, dc.cred.fsgid.val, mock_task->cred->fsgid.val);

	/* Finalize */
	release_defex_context(&dc);
	kthread_stop(mock_task);
	put_task_struct(mock_task);
}


static void get_dc_target_name_test(struct test *test)
{
	struct defex_context dc;
	struct file *test_file;
	char * process_name;

	test_file = local_fopen(ROOT_PATH, O_RDONLY, 0);
	ASSERT_FALSE(test, IS_ERR_OR_NULL(test_file));

	/* Target name != "<unknown filename>" */
	init_defex_context(&dc, __DEFEX_execve, current, test_file);
	process_name = get_dc_target_name(&dc);
	EXPECT_NE(test, strncmp(process_name, "<unknown filename>", 18), 0);
	release_defex_context(&dc);
	filp_close(test_file, NULL);

	/* Target name == "<unknown filename>" */
	init_defex_context(&dc, __DEFEX_execve, current, NULL);
	process_name = get_dc_target_name(&dc);
	EXPECT_EQ(test, strncmp(process_name, "<unknown filename>", 18), 0);
	release_defex_context(&dc);
}


static void get_dc_target_dpath_test(struct test *test)
{
	struct defex_context dc;
	struct file *test_file;

	test_file = local_fopen(ROOT_PATH, O_RDONLY, 0);
	ASSERT_FALSE(test, IS_ERR_OR_NULL(test_file));

	/* With target file, get_dc_target_dpath != NULL */
	init_defex_context(&dc, __DEFEX_execve, current, test_file);
	EXPECT_NE(test, get_dc_target_dpath(&dc), NULL);
	release_defex_context(&dc);
	filp_close(test_file, NULL);

	/* Without target file, get_dc_target_dpath == NULL */
	init_defex_context(&dc, __DEFEX_execve, current, NULL);
	EXPECT_EQ(test, get_dc_target_dpath(&dc), NULL);
	release_defex_context(&dc);
}


static void get_dc_process_name_test(struct test *test)
{
	struct defex_context dc;
	char *process_name;

	init_defex_context(&dc, __DEFEX_execve, current, NULL);
	process_name = get_dc_process_name(&dc);
	EXPECT_EQ(test, strncmp(process_name, "<unknown filename>", 18), 0);
	release_defex_context(&dc);
}


static void get_dc_process_file_test(struct test *test)
{
	struct defex_context dc;

	init_defex_context(&dc, __DEFEX_execve, current, NULL);
	EXPECT_EQ(test, get_dc_process_file(&dc), NULL);
	release_defex_context(&dc);
}


static void get_dc_process_dpath_test(struct test *test)
{
	struct defex_context dc;

	init_defex_context(&dc, __DEFEX_execve, current, NULL);
	EXPECT_EQ(test, get_dc_process_dpath(&dc), NULL);
	release_defex_context(&dc);

	/* Need a situation in which get_dc_process_dpath != NULL to increase coverage */
}


static void defex_resolve_filename_test(struct test *test)
{
	char *filename = NULL;
	char *buff = NULL;

	filename = defex_resolve_filename("/test.txt", &buff);
	EXPECT_EQ(test, filename, NULL);

	/* Missing case where dpath in defex_resolve_filename returns != NULL */
}


static void defex_get_source_file_test(struct test *test)
{
	EXPECT_EQ(test, defex_get_source_file(current), NULL);
	/* Not able to test a source file != NULL */
}


static void defex_get_filename_test(struct test *test)
{
	char *filename;

	filename = defex_get_filename(current);
	EXPECT_EQ(test, strncmp(filename, "<unknown filename>", 18), 0);

	/* Could not set up a situation where defex_get_source_file is != NULL */
}


static void defex_files_identical_test(struct test *test)
{
	/* EXPECT_EQ(test, 1, defex_files_identical()); */
}


static void __vfs_read_test(struct test *test)
{
	/* EXPECT_EQ(test, 1, __vfs_read()); */
}


static int defex_common_test_init(struct test *test)
{
	/*
	 * test->priv = a_struct_pointer;
	 * if (!test->priv)
	 *    return -ENOMEM;
	 */

	return 0;
}

static void defex_common_test_exit(struct test *test)
{
}

static struct test_case defex_common_test_cases[] = {
	/* TEST FUNC DEFINES */
	TEST_CASE(release_defex_context_test),
	TEST_CASE(local_fread_test),
	TEST_CASE(local_fopen_test),
	TEST_CASE(init_defex_context_test),
	TEST_CASE(get_dc_target_name_test),
	TEST_CASE(get_dc_target_dpath_test),
	TEST_CASE(get_dc_process_name_test),
	TEST_CASE(get_dc_process_file_test),
	TEST_CASE(get_dc_process_dpath_test),
	TEST_CASE(defex_resolve_filename_test),
	TEST_CASE(defex_get_source_file_test),
	TEST_CASE(defex_get_filename_test),
	TEST_CASE(defex_files_identical_test),
	TEST_CASE(__vfs_read_test),
	{},
};

static struct test_module defex_common_test_module = {
	.name = "defex_common_test",
	.init = defex_common_test_init,
	.exit = defex_common_test_exit,
	.test_cases = defex_common_test_cases,
};
module_test(defex_common_test_module);

