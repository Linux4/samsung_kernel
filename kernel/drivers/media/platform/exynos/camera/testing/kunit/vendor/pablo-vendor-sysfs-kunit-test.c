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

#include "is-core.h"
#include "is-device-sensor-peri.h"
#include "is-vendor-private.h"
#include "is-sysfs-front.h"
#include "is-sysfs-rear.h"

struct attribute **pablo_vendor_sysfs_get_rear_attr_tbl(void);
struct attribute **pablo_vendor_sysfs_get_front_attr_tbl(void);
/* Define the test cases. */

static char *sysfs_output_buf;

static bool pablo_vendor_sysfs_need_varification(struct kunit *test, const char *attr_name)
{
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr_name);

	/* some sysfs node can return 0 */
	if (strstr(attr_name, "ssrm") == NULL
		&& strstr(attr_name, "dualcal") == NULL) {
		return true;
	}

	return false;
}

static bool pablo_vendor_sysfs_check_camfw(struct kunit *test, const char *attr_name)
{
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr_name);

	/* some sysfs node can return 0 */
	if (strstr(attr_name, "camfw") != NULL)
		return true;

	return false;
}

static void pablo_vendor_sysfs_all_kunit_test(struct kunit *test, struct attribute **attr_tbl)
{
	ssize_t ret;
	struct device_attribute *dev_attr;
	int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr_tbl);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, attr_tbl[0]);

	for (i = 0; attr_tbl[i] != NULL; i++) {
		dev_attr = container_of(attr_tbl[i], struct device_attribute, attr);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev_attr->show);
		ret = dev_attr->show(NULL, dev_attr, sysfs_output_buf);

		if (pablo_vendor_sysfs_need_varification(test, dev_attr->attr.name)) {
			/* camera version length should be greater than 10 */
			if (pablo_vendor_sysfs_check_camfw(test, dev_attr->attr.name))
				KUNIT_EXPECT_GT_MSG(test, ret, 10, "attr=%s", dev_attr->attr.name);
			else
				KUNIT_EXPECT_GT_MSG(test, ret, 0, "attr=%s", dev_attr->attr.name);
		}
	}
}

static void pablo_vendor_sysfs_rear_all_kunit_test(struct kunit *test)
{
	struct attribute **attr_tbl = pablo_vendor_sysfs_get_rear_attr_tbl();

	pablo_vendor_sysfs_all_kunit_test(test, attr_tbl);
}

static void pablo_vendor_sysfs_front_all_kunit_test(struct kunit *test)
{
	struct attribute **attr_tbl = pablo_vendor_sysfs_get_front_attr_tbl();

	pablo_vendor_sysfs_all_kunit_test(test, attr_tbl);
}

static int pablo_vendor_sysfs_kunit_test_init(struct kunit *test)
{
	int ret = 0;

	sysfs_output_buf = vmalloc(PAGE_SIZE);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sysfs_output_buf);

	return ret;
}

static void pablo_vendor_sysfs_kunit_test_exit(struct kunit *test)
{
	if (sysfs_output_buf) {
		vfree(sysfs_output_buf);
		sysfs_output_buf = NULL;
	}
}

static struct kunit_case pablo_vendor_sysfs_kunit_test_cases[] = {
	KUNIT_CASE(pablo_vendor_sysfs_rear_all_kunit_test),
	KUNIT_CASE(pablo_vendor_sysfs_front_all_kunit_test),
	KUNIT_CASE(pablo_vendor_sysfs_reload_kunit_test),
	{},
};

struct kunit_suite pablo_vendor_sysfs_kunit_test_suite = {
	.name = "pablo-vendor-sysfs-kunit-test",
	.init = pablo_vendor_sysfs_kunit_test_init,
	.exit = pablo_vendor_sysfs_kunit_test_exit,
	.test_cases = pablo_vendor_sysfs_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_vendor_sysfs_kunit_test_suite);

MODULE_LICENSE("GPL");
