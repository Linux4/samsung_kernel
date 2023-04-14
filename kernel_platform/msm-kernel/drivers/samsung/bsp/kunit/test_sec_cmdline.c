// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/platform_device.h>

#include <linux/samsung/bsp/sec_cmdline.h>

#include <kunit/test.h>

extern void kunit_cmdline_set_mock_bootargs(const char *bootargs);
extern int kunit_cmdline_mock_probe(struct platform_device *pdev);
extern int kunit_cmdline_mock_remove(struct platform_device *pdev);

static struct platform_device *mock_cmdline_pdev;

static void __cmdline_create_mock_cmdline_pdev(void)
{
	mock_cmdline_pdev = kzalloc(sizeof(struct platform_device),
			GFP_KERNEL);
	device_initialize(&mock_cmdline_pdev->dev);
}

static void __cmdline_remove_mock_cmdline_pdev(void)
{
	kfree(mock_cmdline_pdev);
}

const char *mock_bootargs = "raccoon_0=0 raccoon_1=is_not_a_dog raccoon_2=\"a cute animal\" raccoon_3=No raccoon_3=Yes";

static int sec_cmdline_test_init(struct kunit *test)
{
	__cmdline_create_mock_cmdline_pdev();
	kunit_cmdline_set_mock_bootargs(mock_bootargs);
	kunit_cmdline_mock_probe(mock_cmdline_pdev);

	return 0;
}

static void sec_cmdline_test_exit(struct kunit *test)
{
	kunit_cmdline_mock_remove(mock_cmdline_pdev);
	__cmdline_remove_mock_cmdline_pdev();
}

static void test_case_0_racoon_0(struct kunit *test)
{
	const char *val = sec_cmdline_get_val("raccoon_0");

	KUNIT_EXPECT_STREQ(test, val, "0");
}

static void test_case_0_racoon_1(struct kunit *test)
{
	const char *val = sec_cmdline_get_val("raccoon_1");

	KUNIT_EXPECT_STREQ(test, val, "is_not_a_dog");
}

static void test_case_0_racoon_2(struct kunit *test)
{
	const char *val = sec_cmdline_get_val("raccoon_2");

	KUNIT_EXPECT_STREQ(test, val, "a cute animal");
}

static void test_case_0_racoon_3(struct kunit *test)
{
	const char *val = sec_cmdline_get_val("raccoon_3");

	KUNIT_EXPECT_STREQ(test, val, "Yes");
}

static struct kunit_case sec_cmdline_test_cases[] = {
	KUNIT_CASE(test_case_0_racoon_0),
	KUNIT_CASE(test_case_0_racoon_1),
	KUNIT_CASE(test_case_0_racoon_2),
	KUNIT_CASE(test_case_0_racoon_3),
	{}
};

static struct kunit_suite sec_cmdline_test_suite = {
	.name = "SEC Kernel command-line helper for modules",
	.init = sec_cmdline_test_init,
	.exit = sec_cmdline_test_exit,
	.test_cases = sec_cmdline_test_cases,
};

kunit_test_suites(&sec_cmdline_test_suite);
