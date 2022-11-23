// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/platform_device.h>

#include <linux/samsung/debug/sec_reboot_cmd.h>

#include <kunit/test.h>

extern int kunit_rbcmd_mock_notifier_call(enum sec_rbcmd_stage s, unsigned long type, void *data);
extern int kunit_rbcmd_mock_probe(struct platform_device *pdev);
extern int kunit_rbcmd_mock_remove(struct platform_device *pdev);

static unsigned long kunit_rnc_data;

struct kunit_rnc {
	struct sec_reboot_cmd rc;
	unsigned long bit;
};

static int kunit_rnc_simple(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const struct kunit_rnc *krc = container_of(rc, struct kunit_rnc, rc);

	kunit_rnc_data |= BIT(krc->bit);

	return SEC_RBCMD_HANDLE_OK;
}

static int kunit_rnc_simple_strict(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	size_t len = strlen(rc->cmd);

	if (len != strlen(cmd))
		return SEC_RBCMD_HANDLE_DONE;

	return kunit_rnc_simple(rc, param, one_of_multi);
}

#define KUNIT_REBOOT_NOTIFY_CALL(__cmd, __bit, __func) \
{ \
	.rc.cmd = __cmd, \
	.rc.func = __func, \
	.bit = __bit, \
}

#define KUNIT_REBOOT_NOTIFY_CALL_SIMPLE(__cmd, __bit) \
	KUNIT_REBOOT_NOTIFY_CALL(__cmd, __bit, kunit_rnc_simple)

#define KUNIT_REBOOT_NOTIFY_CALL_SIMPLE_STRICT(__cmd, __bit) \
	KUNIT_REBOOT_NOTIFY_CALL(__cmd, __bit, kunit_rnc_simple_strict)

static int kunit_rnc_multicmd_oneshot(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	const struct kunit_rnc *krc = container_of(rc, struct kunit_rnc, rc);
	size_t len = strlen(rc->cmd);

	if (len != strlen(cmd))
		return SEC_RBCMD_HANDLE_DONE;

	kunit_rnc_data |= BIT(krc->bit);

	return SEC_RBCMD_HANDLE_OK | SEC_RBCMD_HANDLE_ONESHOT_MASK;
}

static int kunit_rnc_multicmd_continue(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	const char *cmd = param->cmd;
	const struct kunit_rnc *krc = container_of(rc, struct kunit_rnc, rc);
	size_t len = strlen(rc->cmd);

	if (len != strlen(cmd))
		return SEC_RBCMD_HANDLE_DONE;

	kunit_rnc_data |= BIT(krc->bit);

	return SEC_RBCMD_HANDLE_DONE;
}

/* bit 0 ~ 7 */
static struct kunit_rnc kunit_rnc_custom_list[] = {
	KUNIT_REBOOT_NOTIFY_CALL("multicmd_oneshot_0", 0,
				 kunit_rnc_multicmd_oneshot),
	KUNIT_REBOOT_NOTIFY_CALL("multicmd_continue_1", 1,
				 kunit_rnc_multicmd_continue),
};

/* bit 8 ~ 15 */
static struct kunit_rnc kunit_rnc_simple_list[] = {
	KUNIT_REBOOT_NOTIFY_CALL_SIMPLE("simple_8-", 8),
};

/* bit 16 ~ 23 */
static struct kunit_rnc kunit_rnc_simple_strict_list[] = {
	KUNIT_REBOOT_NOTIFY_CALL_SIMPLE_STRICT("strict_16", 16),
	KUNIT_REBOOT_NOTIFY_CALL_SIMPLE_STRICT("strict_17", 17),
	KUNIT_REBOOT_NOTIFY_CALL_SIMPLE_STRICT("strict_18", 18),
	KUNIT_REBOOT_NOTIFY_CALL_SIMPLE_STRICT("strict_19", 19),
};

/* bit 31 */
static int kunit_rnc_default(const struct sec_reboot_cmd *rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	kunit_rnc_data |= BIT(31);

	return SEC_RBCMD_HANDLE_OK;
}

struct kunit_rnc_group {
	struct kunit_rnc *kunit_rnc;
	size_t nr_size;
};

static struct sec_reboot_cmd kunit_rnc_default_handle = {
	.func = kunit_rnc_default,
};

static struct kunit_rnc_group reboot_cmd_group[] = {
	{
		.kunit_rnc = kunit_rnc_custom_list,
		.nr_size = ARRAY_SIZE(kunit_rnc_custom_list),
	},
	{
		.kunit_rnc = kunit_rnc_simple_list,
		.nr_size = ARRAY_SIZE(kunit_rnc_simple_list),
	},
	{
		.kunit_rnc = kunit_rnc_simple_strict_list,
		.nr_size = ARRAY_SIZE(kunit_rnc_simple_strict_list),
	},
};

static void __kunit_rbcmd_add_reason_for_each(struct kunit_rnc *kunit_rnc,
		size_t nr_size)
{
	size_t i;

	for (i = 0; i < nr_size; i++)
		sec_rbcmd_add_cmd(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
				&kunit_rnc[i].rc);
}

static void __kunit_rbcmd_add_reason(void)
{
	size_t i;

	sec_rbcmd_set_default_cmd(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&kunit_rnc_default_handle);

	for (i = 0; i < ARRAY_SIZE(reboot_cmd_group); i++)
		__kunit_rbcmd_add_reason_for_each(reboot_cmd_group[i].kunit_rnc,
				reboot_cmd_group[i].nr_size);
}

static void __kunit_rbcmd_del_reason_for_each(struct kunit_rnc *kunit_rnc,
		size_t nr_size)
{
	size_t i;

	for (i = 0; i < nr_size; i++)
		sec_rbcmd_del_cmd(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
				&kunit_rnc[i].rc);
}

static void __kunit_rbcmd_del_reason(void)
{
	size_t i;

	sec_rbcmd_unset_default_cmd(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&kunit_rnc_default_handle);

	for (i = 0; i < ARRAY_SIZE(reboot_cmd_group); i++)
		__kunit_rbcmd_del_reason_for_each(reboot_cmd_group[i].kunit_rnc,
				reboot_cmd_group[i].nr_size);
}

static struct platform_device *mock_rbcmd_pdev;

static void __rbcmd_create_mock_rbcmd_pdev(void)
{
	mock_rbcmd_pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	device_initialize(&mock_rbcmd_pdev->dev);
}

static void __rbcmd_remove_mock_rbcmd_pdev(void)
{
	kfree(mock_rbcmd_pdev);
}

static void test_case_0_default_cmd(struct kunit *test)
{
	unsigned long desired = BIT(31);

	kunit_rnc_data = 0;

	/* empty command */
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER, 0, "");
	KUNIT_EXPECT_EQ(test, kunit_rnc_data, desired);

	/* unknown command */
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, "unknown");
	KUNIT_EXPECT_EQ(test, kunit_rnc_data, desired);
}

static void test_case_0_simple_cmd(struct kunit *test)
{
	unsigned long desired = BIT(8);

	kunit_rnc_data = 0;
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, "simple_8-1");
	KUNIT_EXPECT_EQ(test, kunit_rnc_data, desired);

	kunit_rnc_data = 0;
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, "simple_8");
	KUNIT_EXPECT_NE(test, kunit_rnc_data, desired);

	kunit_rnc_data = 0;
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, "simple_8-");
	KUNIT_EXPECT_EQ(test, kunit_rnc_data, desired);
}

static void test_case_0_strict_cmd(struct kunit *test)
{
	unsigned long desired = 0;

	kunit_rnc_data = 0;

	desired |= BIT(17);
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, "strict_17");
	KUNIT_EXPECT_EQ(test, kunit_rnc_data, desired);

	desired |= BIT(18);
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, "strict_18");
	KUNIT_EXPECT_EQ(test, kunit_rnc_data, desired);

	desired |= BIT(19);
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, "strict_19");
	KUNIT_EXPECT_EQ(test, kunit_rnc_data, desired);

	/* maybe default handler called */
	desired |= BIT(20);
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, "strict_20");
	KUNIT_EXPECT_NE(test, kunit_rnc_data, desired);
}

static void test_case_0_multi_cmd(struct kunit *test)
{
	const char *testing_cmd =
			"multicmd_oneshot_0:simple_8-0:strict_17";
	unsigned long desired_oneshot = BIT(0);
	char *multicmd_oneshot = kstrdup(testing_cmd, GFP_KERNEL);

	kunit_rnc_data = 0;
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, multicmd_oneshot);
	KUNIT_EXPECT_EQ(test, kunit_rnc_data, desired_oneshot);

	kfree(multicmd_oneshot);
}

static void test_case_1_multi_cmd(struct kunit *test)
{
	const char *testing_cmd =
			"multicmd_continue_1:simple_8-1:strict_18";
	unsigned long desired_continue = BIT(1) | BIT(8) | BIT(18);
	char *multicmd_continue = kstrdup(testing_cmd, GFP_KERNEL);

	kunit_rnc_data = 0;
	kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			0, multicmd_continue);
	KUNIT_EXPECT_EQ(test, kunit_rnc_data, desired_continue);

	kfree(multicmd_continue);
}

static int sec_rbcmd_test_init(struct kunit *test)
{
	__rbcmd_create_mock_rbcmd_pdev();
	kunit_rbcmd_mock_probe(mock_rbcmd_pdev);
	__kunit_rbcmd_add_reason();

	return 0;
}

static void sec_rbcmd_test_exit(struct kunit *test)
{
	__kunit_rbcmd_del_reason();
	kunit_rbcmd_mock_remove(mock_rbcmd_pdev);
	__rbcmd_remove_mock_rbcmd_pdev();
}

static struct kunit_case sec_rbcmd_test_cases[] = {
	KUNIT_CASE(test_case_0_default_cmd),
	KUNIT_CASE(test_case_0_simple_cmd),
	KUNIT_CASE(test_case_0_strict_cmd),
	KUNIT_CASE(test_case_0_multi_cmd),
	KUNIT_CASE(test_case_1_multi_cmd),
	{}
};

static struct kunit_suite sec_rbcmd_test_suite = {
	.name = "SEC Reboot command iteratorr",
	.init = sec_rbcmd_test_init,
	.exit = sec_rbcmd_test_exit,
	.test_cases = sec_rbcmd_test_cases,
};

kunit_test_suites(&sec_rbcmd_test_suite);
