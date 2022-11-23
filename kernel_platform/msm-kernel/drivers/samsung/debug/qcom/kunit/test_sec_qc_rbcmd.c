// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>
#include <linux/input/qpnp-power-on.h>
#include <linux/platform_device.h>

#include <linux/samsung/debug/sec_reboot_cmd.h>
#include <linux/samsung/debug/qcom/sec_qc_rbcmd.h>

#include <kunit/test.h>

extern int kunit_rbcmd_mock_notifier_call(enum sec_rbcmd_stage s, unsigned long type, void *data);
extern int kunit_rbcmd_mock_probe(struct platform_device *pdev);
extern int kunit_rbcmd_mock_remove(struct platform_device *pdev);

extern int kunit_qc_rbcmd_mock_probe(struct platform_device *pdev);
extern int kunit_qc_rbcmd_mock_remove(struct platform_device *pdev);

static u32 mock_pon_rr;

static int kunit_rbcmd_mock_write_pon_rr(struct notifier_block *this,
		unsigned long pon_rr, void *d)
{
	mock_pon_rr = pon_rr;

	return NOTIFY_DONE;
}

static struct notifier_block mock_write_pon_rr = {
	.notifier_call = kunit_rbcmd_mock_write_pon_rr
};

static u32 mock_sec_rr;

static int kunit_rbcmd_mock_write_sec_rr(struct notifier_block *this,
		unsigned long sec_rr, void *d)
{
	mock_sec_rr = sec_rr;

	return NOTIFY_DONE;
}

static struct notifier_block mock_write_sec_rr = {
	.notifier_call = kunit_rbcmd_mock_write_sec_rr
};

static struct platform_device *mock_rbcmd_pdev;

static void __qc_rbcmd_create_mock_rbcmd_pdev(void)
{
	mock_rbcmd_pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	device_initialize(&mock_rbcmd_pdev->dev);
}

static void __qc_rbcmd_remove_mock_rbcmd_pdev(void)
{
	kfree(mock_rbcmd_pdev);
	mock_rbcmd_pdev = NULL;
}

static struct platform_device *mock_qc_rbcmd_pdev;

static void __qc_rbcmd_create_mock_qc_rbcmd_pdev(void)
{
	mock_qc_rbcmd_pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	device_initialize(&mock_qc_rbcmd_pdev->dev);
}

static void __qc_rbcmd_remove_mock_qc_rbcmd_pdev(void)
{
	kfree(mock_qc_rbcmd_pdev);
	mock_qc_rbcmd_pdev = NULL;
}

static int sec_qc_rbcmd_test_init(struct kunit *test)
{
	__qc_rbcmd_create_mock_rbcmd_pdev();
	kunit_rbcmd_mock_probe(mock_rbcmd_pdev);

	__qc_rbcmd_create_mock_qc_rbcmd_pdev();
	kunit_qc_rbcmd_mock_probe(mock_qc_rbcmd_pdev);

	sec_qc_rbcmd_register_pon_rr_writer(&mock_write_pon_rr);
	sec_qc_rbcmd_register_sec_rr_writer(&mock_write_sec_rr);

	return 0;
}

static void sec_qc_rbcmd_test_exit(struct kunit *test)
{
	sec_qc_rbcmd_unregister_pon_rr_writer(&mock_write_pon_rr);
	sec_qc_rbcmd_unregister_sec_rr_writer(&mock_write_sec_rr);

	kunit_qc_rbcmd_mock_remove(mock_qc_rbcmd_pdev);
	__qc_rbcmd_remove_mock_qc_rbcmd_pdev();

	kunit_rbcmd_mock_remove(mock_rbcmd_pdev);
	__qc_rbcmd_remove_mock_rbcmd_pdev();
}

static void test_case_0_default_pon_reason(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, mock_pon_rr,
			(u32)RESTART_REASON_SEC_DEBUG_MODE);
}

struct qc_rbcmd_test_cmd {
	void *cmd;
	u32 pon_rr;
	u32 sec_rr;
};

static void __kunit_qc_rbcmd_test(struct kunit *test,
		struct qc_rbcmd_test_cmd *test_set, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		kunit_rbcmd_mock_notifier_call(SEC_RBCMD_STAGE_RESTART_HANDLER,
				0, test_set[i].cmd);

		if (test_set[i].sec_rr != RESTART_REASON_NOT_HANDLE)
			KUNIT_EXPECT_EQ(test, mock_sec_rr, test_set[i].sec_rr);

		if (test_set[i].pon_rr != PON_RESTART_REASON_NOT_HANDLE)
			KUNIT_EXPECT_EQ(test, mock_pon_rr, test_set[i].pon_rr);
	}
}

static void test_case_0_debug(struct kunit *test)
{
	struct qc_rbcmd_test_cmd cmd_list[] = {
		{ "debug0x4F4C", PON_RESTART_REASON_DBG_LOW,
		  RESTART_REASON_NORMAL },
		{ "debug0x494D", PON_RESTART_REASON_DBG_MID,
		  RESTART_REASON_NORMAL },
		{ "debug0x4948", PON_RESTART_REASON_DBG_HIGH,
		  RESTART_REASON_NORMAL },
		{ "debug0xBEEF", PON_RESTART_REASON_UNKNOWN,
		  RESTART_REASON_NORMAL },
	};

	__kunit_qc_rbcmd_test(test, cmd_list, ARRAY_SIZE(cmd_list));
}

static void test_case_0_sud(struct kunit *test)
{
	struct qc_rbcmd_test_cmd cmd_list[] = {
		{ "sud0", PON_RESTART_REASON_RORY_START | 0,
		  RESTART_REASON_NORMAL },
		{ "sud1", PON_RESTART_REASON_RORY_START | 1,
		  RESTART_REASON_NORMAL },
		{ "sud2", PON_RESTART_REASON_RORY_START | 2,
		  RESTART_REASON_NORMAL },
	};

	__kunit_qc_rbcmd_test(test, cmd_list, ARRAY_SIZE(cmd_list));
}

static void test_case_0_forceupload(struct kunit *test)
{
	struct qc_rbcmd_test_cmd cmd_list[] = {
		{ "forceupload0", PON_RESTART_REASON_FORCE_UPLOAD_OFF,
		  RESTART_REASON_NORMAL },
		{ "forceupload1", PON_RESTART_REASON_FORCE_UPLOAD_ON,
		  RESTART_REASON_NORMAL },
		{ "forceupload2", PON_RESTART_REASON_FORCE_UPLOAD_ON,
		  RESTART_REASON_NORMAL },
	};

	__kunit_qc_rbcmd_test(test, cmd_list, ARRAY_SIZE(cmd_list));
}

static void test_case_0_multi(struct kunit *test)
{
	struct qc_rbcmd_test_cmd cmd_list[] = {
		{ "multicmd:debug0x494D", PON_RESTART_REASON_MULTICMD,
		  RESTART_REASON_NORMAL },
		{ "debug0x494D:sud2", PON_RESTART_REASON_DBG_MID,
		  RESTART_REASON_NORMAL },
		{ "sud2:debug0x494D", PON_RESTART_REASON_RORY_START | 2,
		  RESTART_REASON_NORMAL },
	};

	__kunit_qc_rbcmd_test(test, cmd_list, ARRAY_SIZE(cmd_list));
}

static void test_case_0_oem(struct kunit *test)
{
	struct qc_rbcmd_test_cmd cmd_list[] = {
		{ "oem-", PON_RESTART_REASON_UNKNOWN, RESTART_REASON_NORMAL },
		{ "oem-0", PON_RESTART_REASON_UNKNOWN, RESTART_REASON_NORMAL },
	};

	__kunit_qc_rbcmd_test(test, cmd_list, ARRAY_SIZE(cmd_list));
}

static void test_case_0_simple_strict(struct kunit *test)
{
	struct qc_rbcmd_test_cmd cmd_list[] = {
		{ "sec_debug_hw_reset", PON_RESTART_REASON_NOT_HANDLE,
		  RESTART_REASON_SEC_DEBUG_MODE },
		{ "download", PON_RESTART_REASON_DOWNLOAD,
		  RESTART_REASON_NOT_HANDLE },
		{ "nvbackup", PON_RESTART_REASON_NVBACKUP,
		  RESTART_REASON_NOT_HANDLE },
		{ "nvrestore", PON_RESTART_REASON_NVRESTORE,
		  RESTART_REASON_NOT_HANDLE },
		{ "nverase", PON_RESTART_REASON_NVERASE,
		  RESTART_REASON_NOT_HANDLE },
		{ "nvrecovery", PON_RESTART_REASON_NVRECOVERY,
		  RESTART_REASON_NOT_HANDLE },
		{ "cpmem_on", PON_RESTART_REASON_CP_MEM_RESERVE_ON,
		  RESTART_REASON_NOT_HANDLE },
		{ "cpmem_off", PON_RESTART_REASON_CP_MEM_RESERVE_OFF,
		  RESTART_REASON_NOT_HANDLE },
		{ "mbsmem_on", PON_RESTART_REASON_MBS_MEM_RESERVE_ON,
		  RESTART_REASON_NOT_HANDLE },
		{ "mbsmem_off", PON_RESTART_REASON_MBS_MEM_RESERVE_OFF,
		  RESTART_REASON_NOT_HANDLE },
		{ "GlobalActions restart", PON_RESTART_REASON_NORMALBOOT,
		  RESTART_REASON_NOT_HANDLE },
		{ "userrequested", PON_RESTART_REASON_NORMALBOOT,
		  RESTART_REASON_NOT_HANDLE },
		{ "silent.sec", PON_RESTART_REASON_NORMALBOOT,
		  RESTART_REASON_NOT_HANDLE },
		{ "peripheral_hw_reset", PON_RESTART_REASON_SECURE_CHECK_FAIL,
		  RESTART_REASON_NORMAL },
		{ "cross_fail", PON_RESTART_REASON_CROSS_FAIL,
		  RESTART_REASON_NORMAL },
		{ "user_quefi_quest", PON_RESTART_REASON_QUEST_QUEFI_USER_START,
		  RESTART_REASON_NORMAL },
		{ "user_suefi_quest", PON_RESTART_REASON_QUEST_SUEFI_USER_START,
		  RESTART_REASON_NORMAL },
		{ "user_dram_test", PON_RESTART_REASON_USER_DRAM_TEST,
		  RESTART_REASON_NORMAL },
		{ "erase_param_quest", PON_RESTART_REASON_QUEST_ERASE_PARAM,
		  RESTART_REASON_NORMAL },		  
	};

	__kunit_qc_rbcmd_test(test, cmd_list, ARRAY_SIZE(cmd_list));
}

static struct kunit_case sec_qc_rbcmd_test_cases[] = {
	KUNIT_CASE(test_case_0_default_pon_reason),
	KUNIT_CASE(test_case_0_debug),
	KUNIT_CASE(test_case_0_sud),
	KUNIT_CASE(test_case_0_forceupload),
	KUNIT_CASE(test_case_0_multi),
	KUNIT_CASE(test_case_0_oem),
	KUNIT_CASE(test_case_0_simple_strict),
	{}
};

static struct kunit_suite sec_qc_rbcmd_test_suite = {
	.name = "SEC - QC Reboot command iterator - Restart Handler",
	.init = sec_qc_rbcmd_test_init,
	.exit = sec_qc_rbcmd_test_exit,
	.test_cases = sec_qc_rbcmd_test_cases,
};

kunit_test_suites(&sec_qc_rbcmd_test_suite);
