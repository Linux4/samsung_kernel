// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/platform_device.h>

#include <linux/samsung/debug/sec_upload_cause.h>

#include <kunit/test.h>

extern int kunit_upldc_mock_notifier_call(unsigned long type, void *data);
extern int kunit_upldc_mock_probe(struct platform_device *pdev);
extern int kunit_upldc_mock_remove(struct platform_device *pdev);

static unsigned long kunit_upldc_data;

struct kunit_upload_cause {
	struct sec_upload_cause uc;
	unsigned long bit;
};

static int kunit_upldc_simple(const struct sec_upload_cause *uc,
		const char *cause)
{
	const struct kunit_upload_cause *kuc;
	size_t len = strlen(uc->cause);

	if (len != strlen(cause))
		return SEC_UPLOAD_CAUSE_HANDLE_DONE;

	if (strncmp(cause, uc->cause, len))
		return SEC_UPLOAD_CAUSE_HANDLE_DONE;

	kuc = container_of(uc, struct kunit_upload_cause, uc);

	kunit_upldc_data |= BIT(kuc->bit);

	return SEC_UPLOAD_CAUSE_HANDLE_OK;
}

#define KUNIT_UPLOAD_CAUSE_CALL(__cause, __bit, __func) \
{ \
	.uc.cause = __cause, \
	.uc.func = __func, \
	.bit = __bit, \
}

#define KUNIT_UPLOAD_CAUSE_CALL_SIMPLE(__cause, __bit) \
	KUNIT_UPLOAD_CAUSE_CALL(__cause, __bit, kunit_upldc_simple)

/* bit 0 ~ 7 */
static struct kunit_upload_cause kunit_upldc_custom_list[] = {
	KUNIT_UPLOAD_CAUSE_CALL_SIMPLE("simple_0", 0),
	KUNIT_UPLOAD_CAUSE_CALL_SIMPLE("simple_1", 1),
	KUNIT_UPLOAD_CAUSE_CALL_SIMPLE("simple_2", 2),
	KUNIT_UPLOAD_CAUSE_CALL_SIMPLE("simple_3", 3),
};

static void __kunit_upldc_add_cause(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(kunit_upldc_custom_list); i++)
		sec_upldc_add_cause(&kunit_upldc_custom_list[i].uc);
}

static void __kunit_upldc_del_cause(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(kunit_upldc_custom_list); i++)
		sec_upldc_del_cause(&kunit_upldc_custom_list[i].uc);
}

static struct platform_device *mock_upldc_pdev;

static void __upldc_create_mock_upldc_pdev(void)
{
	mock_upldc_pdev = kzalloc(sizeof(struct platform_device),
			GFP_KERNEL);
	device_initialize(&mock_upldc_pdev->dev);
}

static void __upldc_remove_mock_upldc_pdev(void)
{
	kfree(mock_upldc_pdev);
}

static void test_case_0_simple_cause(struct kunit *test)
{
	kunit_upldc_data = 0;
	kunit_upldc_mock_notifier_call(0, "simple_0");
	KUNIT_EXPECT_EQ(test, kunit_upldc_data, BIT(0));

	kunit_upldc_data = 0;
	kunit_upldc_mock_notifier_call(0, "simple_1");
	KUNIT_EXPECT_EQ(test, kunit_upldc_data, BIT(1));

	kunit_upldc_data = 0;
	kunit_upldc_mock_notifier_call(0, "simple_2");
	KUNIT_EXPECT_EQ(test, kunit_upldc_data, BIT(2));

	kunit_upldc_data = 0;
	kunit_upldc_mock_notifier_call(0, "simple_3");
	KUNIT_EXPECT_EQ(test, kunit_upldc_data, BIT(3));

	kunit_upldc_data = 0;
	kunit_upldc_mock_notifier_call(0, "simple_4");
	KUNIT_EXPECT_EQ(test, kunit_upldc_data, 0UL);
}

static int sec_upldc_test_init(struct kunit *test)
{
	__upldc_create_mock_upldc_pdev();
	kunit_upldc_mock_probe(mock_upldc_pdev);
	__kunit_upldc_add_cause();

	return 0;
}

static void sec_upldc_test_exit(struct kunit *test)
{
	__kunit_upldc_del_cause();
	kunit_upldc_mock_remove(mock_upldc_pdev);
	__upldc_remove_mock_upldc_pdev();
}

static struct kunit_case sec_upldc_test_cases[] = {
	KUNIT_CASE(test_case_0_simple_cause),
	{}
};

static struct kunit_suite sec_upldc_test_suite = {
	.name = "SEC Panic Notifier Updating Upload Cause",
	.init = sec_upldc_test_init,
	.exit = sec_upldc_test_exit,
	.test_cases = sec_upldc_test_cases,
};

kunit_test_suites(&sec_upldc_test_suite);
