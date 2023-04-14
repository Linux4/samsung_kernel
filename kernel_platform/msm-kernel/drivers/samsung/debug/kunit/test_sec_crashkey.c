// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#include <linux/samsung/bsp/sec_key_notifier.h>
#include <linux/samsung/debug/sec_crashkey.h>

#include <kunit/test.h>

#define CRASHKEY_MOCK_NAME	"Mock Crash Key"

extern int kunit_crashkey_mock_key_notifier_call(const char *name, unsigned long type, struct sec_key_notifier_param *data);
extern void kunit_crashkey_mock_set_desired_pattern(const struct sec_key_notifier_param *kn_param, const size_t nr_kn_param);
extern int kunit_crashkey_mock_probe(struct platform_device *pdev);
extern int kunit_crashkey_mock_remove(struct platform_device *pdev);

static struct sec_key_notifier_param __crashkey_desried_pattern[] = {
	SEC_KEY_EVENT_KEY_PRESS(KEY_F13),
	SEC_KEY_EVENT_KEY_PRESS_AND_RELEASE(KEY_F14),
	SEC_KEY_EVENT_KEY_PRESS_AND_RELEASE(KEY_F15),
	SEC_KEY_EVENT_KEY_RELEASE(KEY_F13),
};

static const size_t __crashkey_nr_desired_pattern =
		ARRAY_SIZE(__crashkey_desried_pattern);

static void __crashkey_set_desired_pattern(void)
{
	kunit_crashkey_mock_set_desired_pattern(__crashkey_desried_pattern,
			__crashkey_nr_desired_pattern);
}

static bool __crashkey_mock_panic;

static int __crashkey_call_mock_panic(struct notifier_block *this,
		unsigned long type, void *data)
{
	__crashkey_mock_panic = true;

	pr_info("Mock Kernel Pnaic!\n");

	return NOTIFY_DONE;
}

static struct notifier_block __crashkey_mock_panic_nb = {
	.notifier_call = __crashkey_call_mock_panic,
};

static void __crashkey_add_preparing_mock_panic(void)
{
	__crashkey_mock_panic = false;

	sec_crashkey_add_preparing_panic(&__crashkey_mock_panic_nb,
			CRASHKEY_MOCK_NAME);
}

static void __crashkey_del_preparing_mock_panic(void)
{
	sec_crashkey_del_preparing_panic(&__crashkey_mock_panic_nb,
			CRASHKEY_MOCK_NAME);
}

static struct platform_device *mock_crashkey_pdev;

static void __crashkey_create_mock_crashkey_pdev(void)
{
	mock_crashkey_pdev = kzalloc(sizeof(struct platform_device),
			GFP_KERNEL);
	device_initialize(&mock_crashkey_pdev->dev);
}

static void __crashkey_remove_mock_crashkey_pdev(void)
{
	kfree(mock_crashkey_pdev);
}

static void test_case_0_triggered(struct kunit *test)
{
	size_t i;
	unsigned int msecs = (1000 - 100) /
			(unsigned int)__crashkey_nr_desired_pattern;

	for (i = 0; i < __crashkey_nr_desired_pattern; i++) {
		kunit_crashkey_mock_key_notifier_call(CRASHKEY_MOCK_NAME,
				0, &__crashkey_desried_pattern[i]);
		msleep(msecs);
	}

	KUNIT_EXPECT_TRUE(test, __crashkey_mock_panic);
}

static void test_case_0_not_triggered_by_press_slowly(struct kunit *test)
{
	size_t i;
	unsigned int msecs = 1500 / (unsigned int)__crashkey_nr_desired_pattern;

	for (i = 0; i < __crashkey_nr_desired_pattern - 1; i++) {
		kunit_crashkey_mock_key_notifier_call(CRASHKEY_MOCK_NAME,
				0, &__crashkey_desried_pattern[i]);
		msleep(msecs);
	}

	KUNIT_EXPECT_FALSE(test, __crashkey_mock_panic);
}

static void test_case_1_not_triggered_by_wrong_keys(struct kunit *test)
{
	struct sec_key_notifier_param wrong_pattern[] = {
		SEC_KEY_EVENT_KEY_PRESS(KEY_F16),
		SEC_KEY_EVENT_KEY_PRESS_AND_RELEASE(KEY_F15),
		SEC_KEY_EVENT_KEY_PRESS_AND_RELEASE(KEY_F14),
	};
	size_t nr_wrong_pattern = ARRAY_SIZE(wrong_pattern);
	unsigned int msecs = (1000 - 100) / (unsigned int)nr_wrong_pattern;
	size_t i;

	for (i = 0; i < nr_wrong_pattern; i++) {
		kunit_crashkey_mock_key_notifier_call(CRASHKEY_MOCK_NAME,
				0, &wrong_pattern[i]);
		msleep(msecs);
	}

	KUNIT_EXPECT_FALSE(test, __crashkey_mock_panic);
}

static int sec_crashkey_test_init(struct kunit *test)
{
	__crashkey_create_mock_crashkey_pdev();
	__crashkey_set_desired_pattern();
	kunit_crashkey_mock_probe(mock_crashkey_pdev);
	__crashkey_add_preparing_mock_panic();

	return 0;
}

static void sec_crashkey_test_exit(struct kunit *test)
{
	__crashkey_del_preparing_mock_panic();
	kunit_crashkey_mock_remove(mock_crashkey_pdev);
	__crashkey_remove_mock_crashkey_pdev();
}

static struct kunit_case sec_crashkey_test_cases[] = {
	KUNIT_CASE(test_case_0_triggered),
	KUNIT_CASE(test_case_0_not_triggered_by_press_slowly),
	KUNIT_CASE(test_case_1_not_triggered_by_wrong_keys),
	{}
};

static struct kunit_suite sec_crashkey_test_suite = {
	.name = "SEC Force key crash driver",
	.init = sec_crashkey_test_init,
	.exit = sec_crashkey_test_exit,
	.test_cases = sec_crashkey_test_cases,
};

kunit_test_suites(&sec_crashkey_test_suite);
