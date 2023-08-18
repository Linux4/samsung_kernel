// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <linux/samsung/debug/sec_debug_region.h>
#include <linux/samsung/debug/qcom/sec_qc_sched_log.h>
#include <linux/samsung/debug/qcom/sec_qc_irq_log.h>
#include <linux/samsung/debug/qcom/sec_qc_msg_log.h>

#include <kunit/test.h>

#include "../sec_qc_logger.h"

#define MOCK_DBG_REGION_RESERVED_SIZE	(2 * 1024 * 1024)

extern void kunit_dbg_region_set_reserved_size(size_t size);
extern unsigned long kunit_dbg_region_get_pool_virt(void);
extern int kunit_dbg_region_mock_probe(struct platform_device *pdev);
extern int kunit_dbg_region_mock_remove(struct platform_device *pdev);

extern int kunit_qc_logger_set_mock_drvdata(const struct qc_logger_drvdata *drvdata);
extern int kunit_qc_logger_mock_probe(struct platform_device *pdev);
extern int kunit_qc_logger_mock_remove(struct platform_device *pdev);

/* [[BEGIG>> NOTE: imported from 'test_sec_debugregion.c' */
static struct platform_device *mock_dbg_region_pdev;

static void __dbg_region_create_mock_dbg_region_pdev(void)
{
	mock_dbg_region_pdev =
			kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	device_initialize(&mock_dbg_region_pdev->dev);
}

static void __dbg_region_remove_mock_dbg_region_pdev(void)
{
	kfree(mock_dbg_region_pdev);
}

static int sec_dbg_region_test_init(struct kunit *test)
{
	__dbg_region_create_mock_dbg_region_pdev();
	kunit_dbg_region_set_reserved_size(MOCK_DBG_REGION_RESERVED_SIZE);
	kunit_dbg_region_mock_probe(mock_dbg_region_pdev);

	return 0;
}

static void sec_dbg_region_test_exit(struct kunit *test)
{
	kunit_dbg_region_mock_remove(mock_dbg_region_pdev);
	__dbg_region_remove_mock_dbg_region_pdev();
}
/* <<END]] */

static struct qc_logger_drvdata mock_sched_log = {
	.name = "sec,qcom-sched_log",
	.unique_id = SEC_QC_SCHED_LOG_MAGIC,
};

static struct qc_logger_drvdata mock_irq_log = {
	.name = "sec,qcom-irq_log",
	.unique_id = SEC_QC_IRQ_LOG_MAGIC,
};

static struct qc_logger_drvdata mock_msg_log = {
	.name = "sec,qcom-msg_log",
	.unique_id = SEC_QC_MSG_LOG_MAGIC,
};

static struct platform_device *mock_qc_logger_pdev;

static void __qc_logger_create_mock_qc_logger_pdev(void)
{
	mock_qc_logger_pdev =
			kzalloc(sizeof(struct platform_device),  GFP_KERNEL);
	device_initialize(&mock_qc_logger_pdev->dev);
}

static void __qc_logger_remove_mock_qc_logger_pdev(void)
{
	kfree(mock_qc_logger_pdev);
}

static int sec_qc_logger_test_init(struct kunit *test,
		 const struct qc_logger_drvdata *mock_drvdata)
{
	__qc_logger_create_mock_qc_logger_pdev();
	kunit_qc_logger_set_mock_drvdata(mock_drvdata);
	kunit_qc_logger_mock_probe(mock_qc_logger_pdev);

	return 0;
}

static void sec_qc_logger_test_exit(struct kunit *test)
{
	kunit_qc_logger_mock_remove(mock_qc_logger_pdev);
	__qc_logger_remove_mock_qc_logger_pdev();
}

static struct sec_qc_sched_buf *__get_sched_buf_ptr(struct kunit *test)
{
	struct qc_logger_drvdata *drvdata;
	struct sec_dbg_region_client *client;
	struct qc_logger *logger;
	struct sec_qc_sched_log_data *data;
	struct sec_qc_sched_buf *buf;

	drvdata = platform_get_drvdata(mock_qc_logger_pdev);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, drvdata);

	client = drvdata->client;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, client);

	KUNIT_EXPECT_NE(test, client->virt, 0UL);
	data = (void *)client->virt;
	buf = (void *)&data->buf[0];

	logger = drvdata->logger;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, logger);

	return buf;
}

static void test_case_0_sched_log_init_exit(struct kunit *test)
{
	struct sec_qc_sched_buf *buf;

	sec_qc_logger_test_init(test, &mock_sched_log);

	buf = __get_sched_buf_ptr(test);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, buf);

	sec_qc_logger_test_exit(test);
}

static void __compare_sched_buf_expect_eq(struct kunit *test,
		const struct sec_qc_sched_buf *a,
		const struct sec_qc_sched_buf *b)
{
	/* test some logs are written */
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, a->task);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, b->task);

	/* test only must be same */
	KUNIT_EXPECT_STREQ(test, &a->comm[0], &b->comm[0]);
	KUNIT_EXPECT_EQ(test, a->pid, b->pid);
	KUNIT_EXPECT_STREQ(test, &a->prev_comm[0], &b->prev_comm[0]);
	KUNIT_EXPECT_EQ(test, a->prio, b->prio);
	KUNIT_EXPECT_EQ(test, a->prev_pid, b->prev_pid);
	KUNIT_EXPECT_EQ(test, a->prev_prio, b->prev_prio);
	KUNIT_EXPECT_EQ(test, a->prev_prio, b->prev_prio);
}

static void test_case_0_sched_log_write_same(struct kunit *test)
{
	struct sec_qc_sched_buf *buf;

	sec_qc_logger_test_init(test, &mock_sched_log);

	buf = __get_sched_buf_ptr(test);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, buf);

	sec_debug_task_sched_log(0, false, current, current);
	sec_debug_task_sched_log(0, false, current, current);
	sec_debug_task_sched_log(0, false, current, current);

	__compare_sched_buf_expect_eq(test, &buf[0], &buf[1]);
	__compare_sched_buf_expect_eq(test, &buf[0], &buf[2]);

	sec_qc_logger_test_exit(test);
}

static struct sec_qc_irq_buf *__get_irq_buf_ptr(struct kunit *test)
{
	struct qc_logger_drvdata *drvdata;
	struct sec_dbg_region_client *client;
	struct qc_logger *logger;
	struct sec_qc_irq_log_data *data;
	struct sec_qc_irq_buf *buf;

	drvdata = platform_get_drvdata(mock_qc_logger_pdev);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, drvdata);

	client = drvdata->client;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, client);

	KUNIT_EXPECT_NE(test, client->virt, 0UL);
	data = (void *)client->virt;
	buf = (void *)&data->buf[0];

	logger = drvdata->logger;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, logger);

	return buf;
}

static void test_case_0_irq_log_init_exit(struct kunit *test)
{
	struct sec_qc_irq_buf *buf;

	sec_qc_logger_test_init(test, &mock_irq_log);

	buf = __get_irq_buf_ptr(test);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, buf);

	sec_qc_logger_test_exit(test);
}

static void __compare_irq_buf_expect_eq(struct kunit *test,
		const struct sec_qc_irq_buf *a,
		const struct sec_qc_irq_buf *b)
{
	/* test some logs are written */
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, a->fn);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, b->fn);

	/* test only must be same */
	KUNIT_EXPECT_EQ(test, a->irq, b->irq);
	KUNIT_EXPECT_PTR_EQ(test, a->fn, b->fn);
	KUNIT_EXPECT_STREQ(test, a->name, b->name);
	KUNIT_EXPECT_EQ(test, a->entry_exit, b->entry_exit);
}

static void test_case_0_irq_log_write_same(struct kunit *test)
{
	struct sec_qc_irq_buf *buf;

	sec_qc_logger_test_init(test, &mock_irq_log);

	buf = __get_irq_buf_ptr(test);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, buf);

	sec_debug_irq_sched_log(4349, __builtin_return_address(0),
			"raccoon_exit", IRQ_EXIT);
	sec_debug_irq_sched_log(4349, __builtin_return_address(0),
			"raccoon_exit", IRQ_EXIT);

	__compare_irq_buf_expect_eq(test, &buf[0], &buf[1]);

	sec_qc_logger_test_exit(test);
}

static void __compare_irq_buf_expect_ne(struct kunit *test,
		const struct sec_qc_irq_buf *a,
		const struct sec_qc_irq_buf *b)
{
	/* test some logs are written */
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, a->fn);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, b->fn);

	/* test only must be different */
	KUNIT_EXPECT_NE(test, a->irq, b->irq);
	KUNIT_EXPECT_PTR_NE(test, a->fn, b->fn);
	KUNIT_EXPECT_STRNEQ(test, a->name, b->name);
	KUNIT_EXPECT_NE(test, a->entry_exit, b->entry_exit);
}

static void test_case_0_irq_log_write_different(struct kunit *test)
{
	struct sec_qc_irq_buf *buf;

	sec_qc_logger_test_init(test, &mock_irq_log);

	buf = __get_irq_buf_ptr(test);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, buf);

	sec_debug_irq_sched_log(4349, __builtin_return_address(0),
			"raccoon_exit", IRQ_EXIT);
	sec_debug_irq_sched_log(9390, __builtin_return_address(1),
			"raccoon_entry", IRQ_ENTRY);

	__compare_irq_buf_expect_ne(test, &buf[0], &buf[1]);

	sec_qc_logger_test_exit(test);
}


static struct sec_qc_msg_buf *__get_msg_buf_ptr(struct kunit *test)
{
	struct qc_logger_drvdata *drvdata;
	struct sec_dbg_region_client *client;
	struct qc_logger *logger;
	struct sec_qc_msg_log_data *data;
	struct sec_qc_msg_buf *buf;

	drvdata = platform_get_drvdata(mock_qc_logger_pdev);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, drvdata);

	client = drvdata->client;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, client);

	KUNIT_EXPECT_NE(test, client->virt, 0UL);
	data = (void *)client->virt;
	buf = (void *)&data->buf[0];

	logger = drvdata->logger;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, logger);

	return buf;
}

static void test_case_0_msg_log_init_exit(struct kunit *test)
{
	struct sec_qc_msg_buf *buf;

	sec_qc_logger_test_init(test, &mock_msg_log);

	buf = __get_msg_buf_ptr(test);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, buf);

	sec_qc_logger_test_exit(test);
}

static void __compare_msg_buf_expect_eq(struct kunit *test,
		const struct sec_qc_msg_buf *a,
		const struct sec_qc_msg_buf *b)
{
	/* test some logs are written */
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, a->caller0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, a->task);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, b->caller0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, b->task);

	/* test only must be same */
	KUNIT_EXPECT_STREQ(test, &a->msg[0], &b->msg[0]);
}

static void test_case_0_msg_log_write_same(struct kunit *test)
{
	struct sec_qc_msg_buf *buf;

	sec_qc_logger_test_init(test, &mock_msg_log);

	buf = __get_msg_buf_ptr(test);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, buf);

	sec_debug_msg_log("%s-%s", __func__, "raccoon");
	sec_debug_msg_log("%s-%s", __func__, "raccoon");

	__compare_msg_buf_expect_eq(test, &buf[0], &buf[1]);

	sec_qc_logger_test_exit(test);
}

static void __compare_msg_buf_expect_ne(struct kunit *test,
		const struct sec_qc_msg_buf *a,
		const struct sec_qc_msg_buf *b)
{
	/* test some logs are written */
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, a->caller0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, a->task);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, b->caller0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, b->task);

	/* test only must be different */
	KUNIT_EXPECT_STRNEQ(test, &a->msg[0], &b->msg[0]);
}

static void test_case_0_msg_log_write_different(struct kunit *test)
{
	struct sec_qc_msg_buf *buf;

	sec_qc_logger_test_init(test, &mock_msg_log);

	buf = __get_msg_buf_ptr(test);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, buf);

	sec_debug_msg_log("%s-%s", __func__, "raccoon");
	sec_debug_msg_log("%s-%s", "raccoon", __func__);

	__compare_msg_buf_expect_ne(test, &buf[0], &buf[1]);

	sec_qc_logger_test_exit(test);
}

static struct kunit_case sec_qc_logger_test_cases[] = {
	KUNIT_CASE(test_case_0_sched_log_init_exit),
	KUNIT_CASE(test_case_0_sched_log_write_same),
	KUNIT_CASE(test_case_0_irq_log_init_exit),
	KUNIT_CASE(test_case_0_irq_log_write_same),
	KUNIT_CASE(test_case_0_irq_log_write_different),
	KUNIT_CASE(test_case_0_msg_log_init_exit),
	KUNIT_CASE(test_case_0_msg_log_write_same),
	KUNIT_CASE(test_case_0_msg_log_write_different),
	{}
};

static struct kunit_suite sec_qc_logger_test_suite = {
	.name = "SEC Logger for Qualcomm based devices",
	.init = sec_dbg_region_test_init,
	.exit = sec_dbg_region_test_exit,
	.test_cases = sec_qc_logger_test_cases,
};

kunit_test_suites(&sec_qc_logger_test_suite);
