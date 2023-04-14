// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/platform_device.h>

#include <linux/samsung/debug/qcom/sec_qc_upload_cause.h>

#include <kunit/test.h>

extern int kunit_upldc_mock_notifier_call(unsigned long type, const void *data);
extern int kunit_upldc_mock_probe(struct platform_device *pdev);
extern int kunit_upldc_mock_remove(struct platform_device *pdev);

extern int kunit_qc_upldc_mock_probe(struct platform_device *pdev);
extern int kunit_qc_upldc_mock_remove(struct platform_device *pdev);

static struct platform_device *mock_upldc_pdev;

static void __qc_upldc_create_mock_upldc_pdev(void)
{
	mock_upldc_pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	device_initialize(&mock_upldc_pdev->dev);
}

static void __qc_upldc_remove_mock_upldc_pdev(void)
{
	kfree(mock_upldc_pdev);
}

static struct platform_device *mock_qc_upldc_pdev;

static void __qc_upldc_create_mock_qc_upldc_pdev(void)
{
	mock_qc_upldc_pdev = kzalloc(sizeof(struct platform_device),
				     GFP_KERNEL);
	device_initialize(&mock_qc_upldc_pdev->dev);
}

static void __qc_upldc_remove_mock_qc_upldc_pdev(void)
{
	kfree(mock_qc_upldc_pdev);
}

static int sec_qc_upldc_test_init(struct kunit *test)
{
	__qc_upldc_create_mock_upldc_pdev();
	kunit_upldc_mock_probe(mock_upldc_pdev);

	__qc_upldc_create_mock_qc_upldc_pdev();
	kunit_qc_upldc_mock_probe(mock_qc_upldc_pdev);

	return 0;
}

static void sec_qc_upldc_test_exit(struct kunit *test)
{
	kunit_upldc_mock_remove(mock_upldc_pdev);
	__qc_upldc_remove_mock_upldc_pdev();

	kunit_qc_upldc_mock_remove(mock_qc_upldc_pdev);
	__qc_upldc_remove_mock_qc_upldc_pdev();
}

static void __qc_upldc_test_strncmp(struct kunit *test,
		const char *cause, unsigned long desired)
{
	unsigned long written;

	sec_qc_upldc_write_cause(UPLOAD_CAUSE_INIT);
	kunit_upldc_mock_notifier_call(0, cause);
	written = sec_qc_upldc_read_cause();

	KUNIT_EXPECT_EQ(test, written, desired);
}

static void test_case_0_causes_using_strncmp(struct kunit *test)
{
	__qc_upldc_test_strncmp(test,
			"User Fault",
			UPLOAD_CAUSE_USER_FAULT);
	__qc_upldc_test_strncmp(test,
			"Crash Key",
			UPLOAD_CAUSE_FORCED_UPLOAD);
	__qc_upldc_test_strncmp(test,
			"User Crash Key",
			UPLOAD_CAUSE_USER_FORCED_UPLOAD);
	__qc_upldc_test_strncmp(test,
			"Long Key Press",
			UPLOAD_CAUSE_POWER_LONG_PRESS);
	__qc_upldc_test_strncmp(test,
			"Platform Watchdog couldnot be initialized",
			UPLOAD_CAUSE_PF_WD_INIT_FAIL);
	__qc_upldc_test_strncmp(test,
			"Platform Watchdog couldnot be restarted",
			UPLOAD_CAUSE_PF_WD_RESTART_FAIL);
	__qc_upldc_test_strncmp(test,
			"Platform Watchdog can't update sync_cnt",
			UPLOAD_CAUSE_PF_WD_KICK_FAIL);
	__qc_upldc_test_strncmp(test,
			"CP Crash",
			UPLOAD_CAUSE_CP_ERROR_FATAL);
	__qc_upldc_test_strncmp(test,
			"MDM Crash",
			UPLOAD_CAUSE_MDM_ERROR_FATAL);
}

static void __qc_upldc_test_strnstr(struct kunit *test,
		const char *cause, unsigned long desired)
{
	char *cause_modified;
	unsigned long written;

	cause_modified = kasprintf(GFP_KERNEL,
			"Raccoon's Cave - %s - Test Suite", cause);

	sec_qc_upldc_write_cause(UPLOAD_CAUSE_INIT);
	kunit_upldc_mock_notifier_call(0, cause_modified);
	written = sec_qc_upldc_read_cause();

	kfree(cause_modified);

	KUNIT_EXPECT_EQ(test, written, desired);
}

static void test_case_0_causes_using_strnstr(struct kunit *test)
{
	__qc_upldc_test_strnstr(test,
			"unrecoverable external_modem",
			UPLOAD_CAUSE_MDM_CRITICAL_FATAL);
	__qc_upldc_test_strnstr(test,
			"external_modem",
			UPLOAD_CAUSE_MDM_ERROR_FATAL);
	__qc_upldc_test_strnstr(test,
			"esoc0 crashed",
			UPLOAD_CAUSE_MDM_ERROR_FATAL);
	__qc_upldc_test_strnstr(test,
			"modem",
			UPLOAD_CAUSE_MODEM_RST_ERR);
	__qc_upldc_test_strnstr(test,
			"riva",
			UPLOAD_CAUSE_RIVA_RST_ERR);
	__qc_upldc_test_strnstr(test,
			"lpass",
			UPLOAD_CAUSE_LPASS_RST_ERR);
	__qc_upldc_test_strnstr(test,
			"dsps",
			UPLOAD_CAUSE_DSPS_RST_ERR);
	__qc_upldc_test_strnstr(test,
			"SMPL",
			UPLOAD_CAUSE_SMPL);
	__qc_upldc_test_strnstr(test,
			"adsp",
			UPLOAD_CAUSE_ADSP_ERROR_FATAL);
	__qc_upldc_test_strnstr(test,
			"slpi",
			UPLOAD_CAUSE_SLPI_ERROR_FATAL);
	__qc_upldc_test_strnstr(test,
			"spss",
			UPLOAD_CAUSE_SPSS_ERROR_FATAL);
	__qc_upldc_test_strnstr(test,
			"npu",
			UPLOAD_CAUSE_NPU_ERROR_FATAL);
	__qc_upldc_test_strnstr(test,
			"cdsp",
			UPLOAD_CAUSE_CDSP_ERROR_FATAL);
	__qc_upldc_test_strnstr(test,
			"taking too long",
			UPLOAD_CAUSE_SUBSYS_IF_TIMEOUT);
	__qc_upldc_test_strnstr(test,
			"Software Watchdog Timer expired",
			UPLOAD_CAUSE_PF_WD_BITE);
}

static void __qc_upldc_test_strncasecmp(struct kunit *test,
		const char *cause, const char *cause_modified,
		unsigned long desired)
{
	unsigned long written;

	sec_qc_upldc_write_cause(UPLOAD_CAUSE_INIT);
	kunit_upldc_mock_notifier_call(0, cause_modified);
	written = sec_qc_upldc_read_cause();

	KUNIT_EXPECT_EQ(test, written, desired);
}

static void test_case_0_causes_using_strncasecmp(struct kunit *test)
{
	__qc_upldc_test_strncasecmp(test,
			"subsys",
			"SuBsYs",
			UPLOAD_CAUSE_PERIPHERAL_ERR);
}

static struct kunit_case sec_qc_upldc_test_cases[] = {
	KUNIT_CASE(test_case_0_causes_using_strncmp),
	KUNIT_CASE(test_case_0_causes_using_strnstr),
	KUNIT_CASE(test_case_0_causes_using_strncasecmp),
	{}
};

static struct kunit_suite sec_qc_upldc_test_suite = {
	.name = "SEC - QC Panic Notifier Updating Upload Cause",
	.init = sec_qc_upldc_test_init,
	.exit = sec_qc_upldc_test_exit,
	.test_cases = sec_qc_upldc_test_cases,
};

kunit_test_suites(&sec_qc_upldc_test_suite);
