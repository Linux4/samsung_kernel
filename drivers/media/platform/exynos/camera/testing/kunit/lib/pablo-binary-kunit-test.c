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

#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/firmware.h>

#include "pablo-kunit-test.h"
#include "pablo-binary.h"

#define PKT_BIN_NAME "pablo_icpufw.bin"

/* Define the test cases. */
static void pablo_binary_put_filesystem_binary_kunit_test(struct kunit *test)
{
	int ret;
	char *fname;
	struct is_binary bin;
	u32 flags = O_TRUNC | O_CREAT | O_EXCL | O_WRONLY | O_APPEND;

	fname = __getname();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fname);

	/* TC #1. It does always fail w/o 'USE_KERNEL_VFS_READ_WRITE' config. */
	ret = put_filesystem_binary(fname, &bin, flags);

	KUNIT_EXPECT_LT(test, ret, (int)0);

	__putname(fname);
}

static void pablo_binary_setup_binary_loader_kunit_test(struct kunit *test)
{
	struct is_binary bin;
	unsigned int retry_cnt = 3;
	int retry_err = -EAGAIN;

	/* TC #1. Don't set private alloc/free functions. */
	setup_binary_loader(&bin, retry_cnt, retry_err, NULL, NULL);

	KUNIT_EXPECT_EQ(test, bin.retry_cnt, retry_cnt);
	KUNIT_EXPECT_EQ(test, bin.retry_err, retry_err);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)bin.alloc);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)bin.free);
	KUNIT_EXPECT_EQ(test, bin.customized, (unsigned long)&bin);

	/* TC #2. Set private alloc/free functions. */
	retry_cnt = 5;
	retry_err = -ENOMEM;
	setup_binary_loader(&bin, retry_cnt, retry_err, &vzalloc, &vfree);

	KUNIT_EXPECT_EQ(test, bin.retry_cnt, retry_cnt);
	KUNIT_EXPECT_EQ(test, bin.retry_err, retry_err);
	KUNIT_EXPECT_PTR_EQ(test, bin.alloc, &vzalloc);
	KUNIT_EXPECT_PTR_EQ(test, bin.free, &vfree);
	KUNIT_EXPECT_EQ(test, bin.customized, (unsigned long)&bin);
}

static void pablo_binary_request_binary_kunit_test(struct kunit *test)
{
	int ret;
	struct is_binary bin;
	struct device dev;

	memset(&bin, 0, sizeof(bin));

	/**
	 * TC #1.
	 * - Empty binary loader
	 * - Specify the path
	 * - NULL device pointer
	 */
	ret = request_binary(&bin, "/vendor/firmware/", PKT_BIN_NAME, NULL);

	KUNIT_EXPECT_LT(test, ret, (int)0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)bin.alloc);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)bin.free);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/**
	 * TC #2.
	 * - Initialized binary loader
	 * - Specify the path
	 * - Valid device pointer
	 */
	ret = request_binary(&bin, "/vendor/firmware/", PKT_BIN_NAME, &dev);

	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, bin.fw);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, bin.data);
	KUNIT_EXPECT_GT(test, bin.size, (size_t)0);

	release_binary(&bin);

	/**
	 * TC #3.
	 * - Initialized binary loader
	 * - No path
	 * - Valid device pointer
	 */
	ret = request_binary(&bin, NULL, PKT_BIN_NAME, &dev);

	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, bin.fw);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, bin.data);
	KUNIT_EXPECT_GT(test, bin.size, (size_t)0);

	release_binary(&bin);

	/**
	 * TC #4.
	 * - Initialized binary loader
	 * - No path
	 * - Valid device pointer
	 * - Invalid binary name
	 */
	bin.size = 0;
	ret = request_binary(&bin, NULL, "invalid.bin", &dev);
	KUNIT_EXPECT_LT(test, ret, (int)0);
	KUNIT_EXPECT_EQ(test, bin.size, (size_t)0);
}

static void pablo_binary_release_binary_kunit_test(struct kunit *test)
{
	int ret;
	struct is_binary bin;
	struct device dev;

	memset(&bin, 0, sizeof(bin));

	/* TC #1. With firmware object */
	ret = request_binary(&bin, NULL, PKT_BIN_NAME, &dev);
	KUNIT_ASSERT_EQ(test, ret, (int)0);

	release_binary(&bin);

	/* TC #2. With FS binary */
	bin.fw = NULL;
	bin.data = vmalloc(4);

	release_binary(&bin);
}

static void pablo_binary_was_loaded_by_kunit_test(struct kunit *test)
{
	int ret;
	struct is_binary bin;
	struct firmware fw;

	memset(&bin, 0, sizeof(bin));

	/* TC #1. With empty binary */
	ret = was_loaded_by(&bin);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* TC #2. With firmware object */
	bin.fw = &fw;
	ret = was_loaded_by(&bin);
	KUNIT_EXPECT_EQ(test, ret, (int)1);

	/* TC #3. With FS binary */
	bin.fw = NULL;
	bin.data = (void *)&fw;

	ret = was_loaded_by(&bin);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
}

static void pablo_binary_carve_binary_version_kunit_test(struct kunit *test)
{
	char *str, *bin_ver;
	char data[NAME_MAX];
	u32 ofs;

	/* TC #1. Invalid binary type */
	carve_binary_version(0xff, 0, (void *)data, 0);

	/* TC #2. Invalid hint */
	carve_binary_version(IS_BIN_LIBRARY, 0xffffffff, (void *)data, 0);
	carve_binary_version(IS_BIN_SETFILE, 0xffffffff, (void *)data, 0);

	/* TC #3. Not enough data size */
	carve_binary_version(IS_BIN_LIBRARY, 0, (void *)data, 1);
	carve_binary_version(IS_BIN_SETFILE, 0, (void *)data, 1);

	/* TC #4. Carve the valid data */
	str = "pablo_lib_carve_version";
	ofs = sizeof(data) - LIBRARY_VER_OFS;
	snprintf(data + ofs, LIBRARY_VER_LEN, "%s", str);

	carve_binary_version(IS_BIN_LIBRARY, 0, (void *)data, sizeof(data));

	bin_ver = get_binary_version(IS_BIN_LIBRARY, 0);
	KUNIT_EXPECT_STREQ(test, bin_ver, str);

	str = "pablo_set_carve_version";
	ofs = sizeof(data) - SETFILE_VER_OFS;
	snprintf(data + ofs, SETFILE_VER_LEN, "%s", str);

	carve_binary_version(IS_BIN_SETFILE, 0, (void *)data, sizeof(data));

	bin_ver = get_binary_version(IS_BIN_SETFILE, 0);
	KUNIT_EXPECT_STREQ(test, bin_ver, str);
}

static void pablo_binary_get_binary_version_kunit_test(struct kunit *test)
{
	char *bin_ver;

	/* TC #1. Invalid binary type */
	bin_ver = get_binary_version(0xff, 0);
	KUNIT_EXPECT_PTR_EQ(test, bin_ver, (char *)NULL);

	/* TC #2. Invalid hint */
	bin_ver = get_binary_version(IS_BIN_LIBRARY, 0xffffffff);

	KUNIT_EXPECT_PTR_EQ(test, bin_ver, (char *)NULL);
}

static struct kunit_case pablo_binary_kunit_test_cases[] = {
	KUNIT_CASE(pablo_binary_put_filesystem_binary_kunit_test),
	KUNIT_CASE(pablo_binary_setup_binary_loader_kunit_test),
	KUNIT_CASE(pablo_binary_request_binary_kunit_test),
	KUNIT_CASE(pablo_binary_release_binary_kunit_test),
	KUNIT_CASE(pablo_binary_was_loaded_by_kunit_test),
	KUNIT_CASE(pablo_binary_carve_binary_version_kunit_test),
	KUNIT_CASE(pablo_binary_get_binary_version_kunit_test),
	{},
};

struct kunit_suite pablo_binary_kunit_test_suite = {
	.name = "pablo-binary-kunit-test",
	.test_cases = pablo_binary_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_binary_kunit_test_suite);

MODULE_LICENSE("GPL");
