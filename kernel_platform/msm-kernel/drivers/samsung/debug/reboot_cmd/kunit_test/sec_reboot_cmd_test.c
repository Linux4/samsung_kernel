// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/samsung/sec_kunit.h>
#include <linux/samsung/sec_of_kunit.h>

#include "sec_reboot_cmd_test.h"

struct reboot_cmd_kunit_context {
	struct sec_of_kunit_data testdata;
	struct sec_reboot_cmd dflt_rbnf;	// reboot notifier
	bool dflt_rbnf_called;
	struct sec_reboot_cmd dflt_rshd;	// restart handler
	bool dflt_rshd_called;
	struct sec_reboot_cmd cmd_00;
	int cmd_00_result;
	struct sec_reboot_cmd cmd_01;
	int cmd_01_result;
};

static void test__rbcmd_parse_dt_reboot_notifier_priority(struct kunit *test)
{
	struct reboot_cmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct reboot_cmd_drvdata *drvdata;
	struct reboot_cmd_stage *stage;
	int err;

	err = __rbcmd_parse_dt_reboot_notifier_priority(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct reboot_cmd_drvdata, bd);
	stage = &drvdata->stage[SEC_RBCMD_STAGE_REBOOT_NOTIFIER];

	KUNIT_EXPECT_EQ(test, 250, stage->nb.priority);
}

static void test__rbcmd_parse_dt_restart_handler_priority(struct kunit *test)
{
	struct reboot_cmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct reboot_cmd_drvdata *drvdata;
	struct reboot_cmd_stage *stage;
	int err;

	err = __rbcmd_parse_dt_restart_handler_priority(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct reboot_cmd_drvdata, bd);
	stage = &drvdata->stage[SEC_RBCMD_STAGE_RESTART_HANDLER];

	KUNIT_EXPECT_EQ(test, 220, stage->nb.priority);
}

static void test__rbcmd_probe_prolog(struct kunit *test)
{
	struct reboot_cmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	int err;

	err = __rbcmd_probe_prolog(testdata->bd);
	KUNIT_EXPECT_EQ(test, 0, err);
}

static void test__rbcmd_set_default_cmd(struct kunit *test)
{
	struct reboot_cmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct reboot_cmd_drvdata *drvdata = container_of(testdata->bd,
			struct reboot_cmd_drvdata, bd);
	int err;

	err = __rbcmd_set_default_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->dflt_rbnf);
	KUNIT_EXPECT_EQ(test, 0, err);

	// one more time.. error should be returned...
	err = __rbcmd_set_default_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->dflt_rbnf);
	KUNIT_EXPECT_EQ(test, -EINVAL, err);

	err = __rbcmd_set_default_cmd(drvdata, SEC_RBCMD_STAGE_RESTART_HANDLER,
			&testctx->dflt_rshd);
	KUNIT_EXPECT_EQ(test, 0, err);

	// one more time.. error should be returned...
	err = __rbcmd_set_default_cmd(drvdata, SEC_RBCMD_STAGE_RESTART_HANDLER,
			&testctx->dflt_rshd);
	KUNIT_EXPECT_EQ(test, -EINVAL, err);
}

static void test__rbcmd_unset_default_cmd(struct kunit *test)
{
	struct reboot_cmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct reboot_cmd_drvdata *drvdata = container_of(testdata->bd,
			struct reboot_cmd_drvdata, bd);
	int err;

	err = __rbcmd_unset_default_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->dflt_rshd);
	KUNIT_EXPECT_EQ(test, -EINVAL, err);

	err = __rbcmd_unset_default_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->dflt_rbnf);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __rbcmd_unset_default_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->dflt_rbnf);
	KUNIT_EXPECT_EQ(test, -ENOENT, err);

	err = __rbcmd_unset_default_cmd(drvdata, SEC_RBCMD_STAGE_RESTART_HANDLER,
			&testctx->dflt_rbnf);
	KUNIT_EXPECT_EQ(test, -EINVAL, err);

	err = __rbcmd_unset_default_cmd(drvdata, SEC_RBCMD_STAGE_RESTART_HANDLER,
			&testctx->dflt_rshd);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __rbcmd_unset_default_cmd(drvdata, SEC_RBCMD_STAGE_RESTART_HANDLER,
			&testctx->dflt_rshd);
	KUNIT_EXPECT_EQ(test, -ENOENT, err);
}

static void test__rbcmd_add_cmd(struct kunit *test)
{
	struct reboot_cmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct reboot_cmd_drvdata *drvdata = container_of(testdata->bd,
			struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;
	struct sec_reboot_param param;
	int err;

	testctx->dflt_rbnf_called = false;
	testctx->cmd_00_result = 0;
	testctx->cmd_01_result = 0;

	err = __rbcmd_add_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->cmd_00);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __rbcmd_add_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->cmd_01);
	KUNIT_EXPECT_EQ(test, 0, err);

	stage = __rbcmd_get_stage(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER);

	param.cmd = "cmd_unknow";
	err = __rbcmd_handle(stage, &param);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_DONE, err);

	err = __rbcmd_set_default_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->dflt_rbnf);
	KUNIT_EXPECT_EQ(test, 0, err);

	param.cmd = "cmd_unknow";
	err = __rbcmd_handle(stage, &param);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, err);
	KUNIT_EXPECT_TRUE(test, testctx->dflt_rbnf_called);

	param.cmd = "cmd_00";
	err = __rbcmd_handle(stage, &param);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, err);
	KUNIT_EXPECT_EQ(test, 1, testctx->cmd_00_result);

	param.cmd = "cmd_01";
	err = __rbcmd_handle(stage, &param);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, err);
	KUNIT_EXPECT_EQ(test, 1, testctx->cmd_01_result);

	param.cmd = "cmd_00:cmd_01";
	err = __rbcmd_handle(stage, &param);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_OK, err);
	KUNIT_EXPECT_EQ(test, 2, testctx->cmd_00_result);
	KUNIT_EXPECT_NE(test, 2, testctx->cmd_01_result);
}

static void test__rbcmd_del_cmd(struct kunit *test)
{
	struct reboot_cmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct reboot_cmd_drvdata *drvdata = container_of(testdata->bd,
			struct reboot_cmd_drvdata, bd);
	struct reboot_cmd_stage *stage;
	struct sec_reboot_param param;
	int err;

	testctx->dflt_rbnf_called = false;
	testctx->cmd_00_result = 0;
	testctx->cmd_01_result = 0;

	err = __rbcmd_del_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->cmd_00);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __rbcmd_del_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->cmd_01);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __rbcmd_unset_default_cmd(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER,
			&testctx->dflt_rbnf);
	KUNIT_EXPECT_EQ(test, 0, err);

	stage = __rbcmd_get_stage(drvdata, SEC_RBCMD_STAGE_REBOOT_NOTIFIER);

	param.cmd = "cmd_unknow";
	err = __rbcmd_handle(stage, &param);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_DONE, err);

	param.cmd = "cmd_00";
	err = __rbcmd_handle(stage, &param);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_DONE, err);
	KUNIT_EXPECT_EQ(test, 0, testctx->cmd_00_result);

	param.cmd = "cmd_01";
	err = __rbcmd_handle(stage, &param);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_DONE, err);
	KUNIT_EXPECT_EQ(test, 0, testctx->cmd_01_result);

	param.cmd = "cmd_00:cmd_01";
	err = __rbcmd_handle(stage, &param);
	KUNIT_EXPECT_EQ(test, SEC_RBCMD_HANDLE_DONE, err);
	KUNIT_EXPECT_EQ(test, 0, testctx->cmd_00_result);
	KUNIT_EXPECT_EQ(test, 0, testctx->cmd_01_result);
}

static struct reboot_cmd_kunit_context *sec_reboot_cmd_testctx;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_reboot_cmd_test);
static SEC_OF_KUNIT_DTB_INFO(sec_reboot_cmd_test);

static int __reboot_cmd_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct reboot_cmd_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,reboot_cmd_test", &drvdata->bd,
			"samsung,reboot_cmd", &sec_reboot_cmd_test_info);
}

static void __reboot_cmd_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct reboot_cmd_drvdata *drvdata = container_of(testdata->bd,
			struct reboot_cmd_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int test_dflt_reboot_nofifier(const struct sec_reboot_cmd * rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	struct reboot_cmd_kunit_context *testctx = container_of(rc,
			struct reboot_cmd_kunit_context, dflt_rbnf);

	testctx->dflt_rbnf_called = true;

	return SEC_RBCMD_HANDLE_OK;
}

static int test_dflt_restart_handler(const struct sec_reboot_cmd * rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	struct reboot_cmd_kunit_context *testctx = container_of(rc,
			struct reboot_cmd_kunit_context, dflt_rshd);

	testctx->dflt_rshd_called = true;

	return SEC_RBCMD_HANDLE_OK;
}

static int test_cmd_00_handler(const struct sec_reboot_cmd * rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	struct reboot_cmd_kunit_context *testctx = container_of(rc,
			struct reboot_cmd_kunit_context, cmd_00);

	testctx->cmd_00_result = one_of_multi ? 2 : 1;

	return SEC_RBCMD_HANDLE_OK |
			(one_of_multi ? SEC_RBCMD_HANDLE_ONESHOT_MASK : 0);
}

static int test_cmd_01_handler(const struct sec_reboot_cmd * rc,
		struct sec_reboot_param *param, bool one_of_multi)
{
	struct reboot_cmd_kunit_context *testctx = container_of(rc,
			struct reboot_cmd_kunit_context, cmd_01);

	testctx->cmd_01_result = one_of_multi ? 2 : 1;

	return SEC_RBCMD_HANDLE_OK;
}

static void __reboot_cmd_test_init_command(struct reboot_cmd_kunit_context *testctx)
{
	testctx->dflt_rbnf.func = test_dflt_reboot_nofifier;
	testctx->dflt_rshd.func = test_dflt_restart_handler;

	testctx->cmd_00.func = test_cmd_00_handler;
	testctx->cmd_00.cmd = "cmd_00";

	testctx->cmd_01.func = test_cmd_01_handler;
	testctx->cmd_01.cmd = "cmd_01";
}

static int sec_reboot_cmd_test_case_init(struct kunit *test)
{
	struct reboot_cmd_kunit_context *testctx = sec_reboot_cmd_testctx;
	struct sec_of_kunit_data *testdata;

	if (!testctx) {
		int err;

		testctx = kzalloc(sizeof(*testctx), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testctx);

		testdata = &testctx->testdata;
		err = __reboot_cmd_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		__reboot_cmd_test_init_command(testctx);

		sec_reboot_cmd_testctx = testctx;
	}

	test->priv = testctx;

	return 0;
}

static void tear_down_sec_reboot_cmd_test(struct kunit *test)
{
	struct reboot_cmd_kunit_context *testctx = sec_reboot_cmd_testctx;
	struct sec_of_kunit_data *testdata = &testctx->testdata;

	__reboot_cmd_test_suite_tear_down(testdata);
	kfree_sensitive(testctx);

	sec_reboot_cmd_testctx = NULL;
}

static struct kunit_case sec_reboot_cmd_test_cases[] = {
	KUNIT_CASE(test__rbcmd_parse_dt_reboot_notifier_priority),
	KUNIT_CASE(test__rbcmd_parse_dt_restart_handler_priority),
	KUNIT_CASE(test__rbcmd_probe_prolog),
	KUNIT_CASE(test__rbcmd_set_default_cmd),
	KUNIT_CASE(test__rbcmd_unset_default_cmd),
	KUNIT_CASE(test__rbcmd_add_cmd),
	KUNIT_CASE(test__rbcmd_del_cmd),
	KUNIT_CASE(tear_down_sec_reboot_cmd_test),
	{},
};

struct kunit_suite sec_reboot_cmd_test_suite = {
	.name = "sec_reboot_cmd_test",
	.init = sec_reboot_cmd_test_case_init,
	.test_cases = sec_reboot_cmd_test_cases,
};

kunit_test_suites(&sec_reboot_cmd_test_suite);
