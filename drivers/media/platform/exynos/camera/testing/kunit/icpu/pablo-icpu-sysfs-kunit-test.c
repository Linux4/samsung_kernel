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

#include <linux/device.h>

#include "pablo-kunit-test.h"

struct attribute **pablo_icpu_sysfs_get_loglevel_attr_tbl(void);
/* Define the test cases. */

static void pablo_icpu_sysfs_core_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_debug_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 1;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_firmware_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 2;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_itf_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 3;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_msgqueue_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 4;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_selftest_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 5;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_hw_itf_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 6;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_hw_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 7;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_mbox_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 8;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_mem_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 9;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_cma_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 10;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_pmem_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 11;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_imgloader_kunit_test(struct kunit *test)
{
	ssize_t ret;
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	struct device_attribute *dev_attr;
	char input[2] = { 0, };
	char output[2];
	char backup[2];
	int index = 12;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr[index]);

	dev_attr = container_of(attr[index], struct device_attribute, attr);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->store);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);

	ret = dev_attr->show(NULL, dev_attr, backup);
	KUNIT_EXPECT_EQ(test, ret, 2);

	input[0] = '3';
	ret = dev_attr->store(NULL, dev_attr, input, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], '3');

	ret = dev_attr->store(NULL, dev_attr, backup, 1);
	KUNIT_EXPECT_EQ(test, ret, 1);
	ret = dev_attr->show(NULL, dev_attr, output);
	KUNIT_EXPECT_EQ(test, ret, 2);
	KUNIT_EXPECT_EQ(test, output[0], backup[0]);
}

static void pablo_icpu_sysfs_max_kunit_test(struct kunit *test)
{
	struct attribute **attr = pablo_icpu_sysfs_get_loglevel_attr_tbl();
	int index = 13;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr);

	KUNIT_ASSERT_PTR_EQ(test, (void*)attr[index], NULL);
}

static struct kunit_case pablo_icpu_sysfs_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_sysfs_core_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_debug_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_firmware_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_itf_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_msgqueue_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_selftest_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_hw_itf_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_hw_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_mbox_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_mem_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_cma_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_pmem_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_imgloader_kunit_test),
	KUNIT_CASE(pablo_icpu_sysfs_max_kunit_test),
	{},
};

static int pablo_icpu_sysfs_kunit_test_init(struct kunit *test)
{
	return 0;
}

static void pablo_icpu_sysfs_kunit_test_exit(struct kunit *test)
{
}

struct kunit_suite pablo_icpu_sysfs_kunit_test_suite = {
	.name = "pablo-icpu-sysfs-kunit-test",
	.init = pablo_icpu_sysfs_kunit_test_init,
	.exit = pablo_icpu_sysfs_kunit_test_exit,
	.test_cases = pablo_icpu_sysfs_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_sysfs_kunit_test_suite);

MODULE_LICENSE("GPL");
