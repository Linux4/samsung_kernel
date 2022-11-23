// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/samsung/bsp/sec_key_notifier.h>
#include <linux/samsung/debug/sec_crashkey_long.h>

#include <kunit/test.h>

extern int kunit_crashkey_long_mock_key_notifier_call(unsigned long type, struct sec_key_notifier_param *data);
extern void kunit_crashkey_long_mock_set_used_key(const unsigned int *used_key, const size_t nr_used_key);
extern int kunit_crashkey_long_mock_probe(struct platform_device *pdev);
extern int kunit_crashkey_long_mock_remove(struct platform_device *pdev);

static struct sec_key_notifier_param __crashkey_long_used_key_event[] = {
	SEC_KEY_EVENT_KEY_PRESS(KEY_F13),
	SEC_KEY_EVENT_KEY_PRESS(KEY_F14),
};

static unsigned int *__crashkey_long_used_key;
static const size_t __crashkey_long_nr_used_key =
		ARRAY_SIZE(__crashkey_long_used_key_event);

static void __crashkey_long_alloc_used_key(void)
{
	__crashkey_long_used_key = kmalloc_array(__crashkey_long_nr_used_key,
			sizeof(*__crashkey_long_used_key), GFP_KERNEL);
}

static void __crashkey_long_free_used_key(void)
{
	kfree(__crashkey_long_used_key);
}

static void __crashkey_long_set_used_key(void)
{
	size_t i;

	for (i = 0; i < __crashkey_long_nr_used_key; i++)
		__crashkey_long_used_key[i] =
				__crashkey_long_used_key_event[i].keycode;

	kunit_crashkey_long_mock_set_used_key(__crashkey_long_used_key,
			__crashkey_long_nr_used_key);
}

static bool __crashkey_long_mock_panic;

static int __crashkey_long_call_mock_panic(struct notifier_block *this,
		unsigned long type, void *data)
{
	if (type != SEC_CRASHKEY_LONG_NOTIFY_TYPE_EXPIRED)
		return NOTIFY_DONE;

	__crashkey_long_mock_panic = true;

	pr_info("Mock Kernel Panic!\n");

	return NOTIFY_OK;
}

static struct notifier_block __crashkey_long_mock_panic_nb = {
	.notifier_call = __crashkey_long_call_mock_panic,
};

#define KUNIT_CRASHKEY_LONG_TYPE_MATCHED	0U
#define KUNIT_CRASHKEY_LONG_TYPE_UNMATCHED	1U
#define KUNIT_CRASHKEY_LONG_TYPE_UNKNOWN	2U

static unsigned int __crashkey_long_mock_matched;

static int __crashkey_long_call_mock_matched(struct notifier_block *this,
		unsigned long type, void *data)
{
	switch (type) {
	case SEC_CRASHKEY_LONG_NOTIFY_TYPE_MATCHED:
		__crashkey_long_mock_matched = KUNIT_CRASHKEY_LONG_TYPE_MATCHED;
		break;
	case SEC_CRASHKEY_LONG_NOTIFY_TYPE_UNMATCHED:
		__crashkey_long_mock_matched = KUNIT_CRASHKEY_LONG_TYPE_UNMATCHED;
		break;
	case SEC_CRASHKEY_LONG_NOTIFY_TYPE_EXPIRED:
		break;
	default:
		__crashkey_long_mock_matched = KUNIT_CRASHKEY_LONG_TYPE_UNKNOWN;
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block __crashkey_long_mock_matched_nb = {
	.notifier_call = __crashkey_long_call_mock_matched,
};

static void __crashkey_long_add_preparing_mock_panic(void)
{
	__crashkey_long_mock_panic = false;

	sec_crashkey_long_add_preparing_panic(&__crashkey_long_mock_panic_nb);
	sec_crashkey_long_add_preparing_panic(&__crashkey_long_mock_matched_nb);
}

static void __crashkey_long_del_preparing_mock_panic(void)
{
	sec_crashkey_long_del_preparing_panic(&__crashkey_long_mock_panic_nb);
	sec_crashkey_long_del_preparing_panic(&__crashkey_long_mock_matched_nb);
}

static struct platform_device *mock_crashkey_long_pdev;

static void __crashkey_long_create_mock_crashkey_long_pdev(void)
{
	mock_crashkey_long_pdev = kzalloc(sizeof(struct platform_device),
			GFP_KERNEL);
	device_initialize(&mock_crashkey_long_pdev->dev);
}

static void __crashkey_long_remove_mock_crashkey_long_pdev(void)
{
	kfree(mock_crashkey_long_pdev);
}

static int sec_crashkey_long_test_init(struct kunit *test)
{
	__crashkey_long_create_mock_crashkey_long_pdev();
	__crashkey_long_alloc_used_key();
	__crashkey_long_set_used_key();
	kunit_crashkey_long_mock_probe(mock_crashkey_long_pdev);
	__crashkey_long_add_preparing_mock_panic();

	return 0;
}

static void sec_crashkey_long_test_exit(struct kunit *test)
{
	__crashkey_long_del_preparing_mock_panic();
	__crashkey_long_free_used_key();
	kunit_crashkey_long_mock_remove(mock_crashkey_long_pdev);
	__crashkey_long_remove_mock_crashkey_long_pdev();
}

static void test_case_0_triggered_press_forward_order(struct kunit *test)
{
	size_t i;
	unsigned int msec = 1000 + 100;

	for (i = 0; i < __crashkey_long_nr_used_key; i++)
		kunit_crashkey_long_mock_key_notifier_call(0,
				&__crashkey_long_used_key_event[i]);

	msleep(msec);

	KUNIT_EXPECT_TRUE(test, __crashkey_long_mock_panic);
	KUNIT_EXPECT_EQ(test, __crashkey_long_mock_matched,
			 KUNIT_CRASHKEY_LONG_TYPE_MATCHED);
}

static void test_case_1_triggered_press_reverse_order(struct kunit *test)
{
	ssize_t i;
	unsigned int msec = 1000 + 100;

	for (i = __crashkey_long_nr_used_key - 1; i >= 0; i--)
		kunit_crashkey_long_mock_key_notifier_call(0,
				&__crashkey_long_used_key_event[i]);

	msleep(msec);

	KUNIT_EXPECT_TRUE(test, __crashkey_long_mock_panic);
	KUNIT_EXPECT_EQ(test, __crashkey_long_mock_matched,
			KUNIT_CRASHKEY_LONG_TYPE_MATCHED);
}

static void test_case_0_triggered_release_1_key_before_panic(struct kunit *test)
{
	size_t i;
	unsigned int msec = (1000 + 100) / 2;
	static struct sec_key_notifier_param release;

	release.keycode = __crashkey_long_used_key_event[0].keycode;
	release.down = !__crashkey_long_used_key_event[0].down;

	for (i = 0; i < __crashkey_long_nr_used_key; i++)
		kunit_crashkey_long_mock_key_notifier_call(0,
				&__crashkey_long_used_key_event[i]);

	msleep(msec);

	kunit_crashkey_long_mock_key_notifier_call(0, &release);

	msleep(msec);

	KUNIT_EXPECT_FALSE(test, __crashkey_long_mock_panic);
	KUNIT_EXPECT_EQ(test, __crashkey_long_mock_matched,
			KUNIT_CRASHKEY_LONG_TYPE_UNMATCHED);
}

static void test_case_0_triggered_press_other_key_before_panic(
		struct kunit *test)
{
	size_t i;
	unsigned int msec = (1000 + 100) / 2;
	static struct sec_key_notifier_param other_key = {
		.keycode = KEY_F15,
		.down = 1,
	};

	for (i = 0; i < __crashkey_long_nr_used_key; i++)
		kunit_crashkey_long_mock_key_notifier_call(0,
				&__crashkey_long_used_key_event[i]);

	msleep(msec);

	kunit_crashkey_long_mock_key_notifier_call(0, &other_key);

	msleep(msec);

	KUNIT_EXPECT_TRUE(test, __crashkey_long_mock_panic);
	KUNIT_EXPECT_EQ(test, __crashkey_long_mock_matched,
			KUNIT_CRASHKEY_LONG_TYPE_MATCHED);
}

static struct kunit_case sec_crashkey_long_test_cases[] = {
	KUNIT_CASE(test_case_0_triggered_press_forward_order),
	KUNIT_CASE(test_case_1_triggered_press_reverse_order),
	KUNIT_CASE(test_case_0_triggered_release_1_key_before_panic),
	KUNIT_CASE(test_case_0_triggered_press_other_key_before_panic),
	{}
};

static struct kunit_suite sec_crashkey_long_test_suite = {
	.name = "SEC Long key reset driver",
	.init = sec_crashkey_long_test_init,
	.exit = sec_crashkey_long_test_exit,
	.test_cases = sec_crashkey_long_test_cases,
};

kunit_test_suites(&sec_crashkey_long_test_suite);
