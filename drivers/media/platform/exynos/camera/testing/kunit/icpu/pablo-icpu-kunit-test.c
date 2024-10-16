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

#include <linux/platform_device.h>

#include "pablo-kunit-test.h"

#include "icpu/pablo-icpu-core.h"
#include "icpu/hardware/pablo-icpu-hw.h"
#include "icpu/mbox/pablo-icpu-mbox.h"

struct iommu_fault;

static struct icpu_platform_data test_pdata;

#define SET_ICPU_CPU_WFI_STATUS(_pd) \
	do { *(u32 *)(_pd.mcuctl_reg_base + 0xC4) = 1; } while(0)

/* Define the test cases. */

static void pablo_icpu_request_mbox_chan_kunit_test(struct kunit *test)
{
	struct pablo_icpu_mbox_chan *chan;
	struct icpu_mbox_client cl;
	struct icpu_mbox_controller *mbox;
	struct icpu_mbox_tx_info *tx_info;
	struct icpu_mbox_rx_info *rx_info;

	chan = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_TX);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, chan);

	mbox = chan->mbox;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mbox);

	tx_info = mbox->hw_info;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tx_info);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tx_info->int_enable_reg);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tx_info->int_gen_reg);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tx_info->int_status_reg);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tx_info->data_reg);
	KUNIT_EXPECT_NE(test, tx_info->data_max_len, (u32)0);

	pablo_icpu_free_mbox_chan(chan);

	chan = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_RX);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, chan);

	mbox = chan->mbox;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mbox);

	rx_info = mbox->hw_info;
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, rx_info);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, rx_info->int_status_reg);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, rx_info->data_reg);
	KUNIT_EXPECT_NE(test, rx_info->data_max_len, (u32)0);
	KUNIT_EXPECT_NE(test, rx_info->irq, 0);

	pablo_icpu_free_mbox_chan(chan);
}

static void pablo_icpu_request_mbox_chan_limit_kunit_test(struct kunit *test)
{
	struct pablo_icpu_mbox_chan *chan[4];
	struct icpu_mbox_client cl;

	chan[0] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_TX);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, chan[0]);

	chan[1] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_RX);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, chan[1]);

	chan[2] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_TX);
	KUNIT_EXPECT_PTR_EQ(test, (void*)chan[2], NULL);

	chan[3] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_RX);
	KUNIT_EXPECT_PTR_EQ(test, (void*)chan[3], NULL);

	pablo_icpu_free_mbox_chan(chan[0]);
	pablo_icpu_free_mbox_chan(chan[1]);
}

static void pablo_icpu_request_mbox_chan_reopen_kunit_test(struct kunit *test)
{
	struct pablo_icpu_mbox_chan *chan[4];
	struct icpu_mbox_client cl;

	chan[0] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_TX);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, chan[0]);

	pablo_icpu_free_mbox_chan(chan[0]);
	chan[0] = NULL;

	chan[1] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_TX);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, chan[1]);

	chan[2] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_RX);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, chan[2]);

	pablo_icpu_free_mbox_chan(chan[2]);
	chan[2] = NULL;

	chan[3] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_RX);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, chan[3]);

	pablo_icpu_free_mbox_chan(chan[1]);
	chan[1] = NULL;
	pablo_icpu_free_mbox_chan(chan[3]);
	chan[3] = NULL;
}

static void pablo_icpu_request_mbox_chan_reopen_negative_kunit_test(struct kunit *test)
{
	struct pablo_icpu_mbox_chan *chan[4];
	struct icpu_mbox_client cl;

	chan[0] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_TX);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, chan[0]);

	chan[1] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_TX);
	KUNIT_EXPECT_PTR_EQ(test, (void*)chan[1], NULL);

	chan[2] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_RX);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, chan[2]);

	chan[3] = pablo_icpu_request_mbox_chan(&cl, ICPU_MBOX_CHAN_RX);
	KUNIT_EXPECT_PTR_EQ(test, (void*)chan[3], NULL);

	pablo_icpu_free_mbox_chan(chan[0]);
	pablo_icpu_free_mbox_chan(chan[2]);
}

static void pablo_icpu_boot_kunit_test(struct kunit *test)
{
	int ret;
	void *pdata = pablo_icpu_test_set_platform_data(&test_pdata);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdata);

	ret = pablo_icpu_boot();
	KUNIT_EXPECT_EQ(test, ret, 0);

	SET_ICPU_CPU_WFI_STATUS(test_pdata);

	pablo_icpu_power_down();

	pablo_icpu_test_set_platform_data(pdata);
}

static void pablo_icpu_pm_ops_kunit_test(struct kunit *test)
{
	int ret;
	const struct dev_pm_ops *ops = pablo_icpu_test_get_pm_ops();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->suspend);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->resume);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->runtime_suspend);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->runtime_resume);

	ret = ops->suspend(NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = ops->resume(NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = ops->runtime_suspend(NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = ops->runtime_resume(NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_icpu_panic_handler_kunit_test(struct kunit *test)
{
	int ret;
	struct notifier_block *blk = pablo_icpu_test_get_panic_handler();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, blk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, blk->notifier_call);

	ret = blk->notifier_call(NULL, 0, NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_icpu_reboot_handler_kunit_test(struct kunit *test)
{
	int ret;
	struct notifier_block *blk = pablo_icpu_test_get_reboot_handler();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, blk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, blk->notifier_call);

	ret = blk->notifier_call(NULL, 0, NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	platform_driver_unregister(pablo_icpu_get_platform_driver());

	ret = platform_driver_register(pablo_icpu_get_platform_driver());
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_icpu_iommu_fault_handler_kunit_test(struct kunit *test)
{
	int ret;
	int (*ops)(struct iommu_fault *, void *) = pablo_icpu_test_get_iommu_fault_handler();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	ret = ops(NULL, NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_icpu_handle_msg_kunit_test(struct kunit *test)
{
	pablo_icpu_handle_msg(NULL, 0);
}

static struct kunit_case pablo_icpu_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_request_mbox_chan_kunit_test),
	KUNIT_CASE(pablo_icpu_request_mbox_chan_limit_kunit_test),
	KUNIT_CASE(pablo_icpu_request_mbox_chan_reopen_kunit_test),
	KUNIT_CASE(pablo_icpu_request_mbox_chan_reopen_negative_kunit_test),
	KUNIT_CASE(pablo_icpu_boot_kunit_test),
	KUNIT_CASE(pablo_icpu_pm_ops_kunit_test),
	KUNIT_CASE(pablo_icpu_panic_handler_kunit_test),
	KUNIT_CASE(pablo_icpu_reboot_handler_kunit_test),
	KUNIT_CASE(pablo_icpu_iommu_fault_handler_kunit_test),
	KUNIT_CASE(pablo_icpu_handle_msg_kunit_test),
	{},
};

static int pablo_icpu_kunit_test_init(struct kunit *test)
{
	test_pdata.mcuctl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_pdata.mcuctl_reg_base);

	test_pdata.sysctrl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_pdata.sysctrl_reg_base);

	return 0;
}

static void pablo_icpu_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_pdata.mcuctl_reg_base);
	kunit_kfree(test, test_pdata.sysctrl_reg_base);
}

struct kunit_suite pablo_icpu_kunit_test_suite = {
	.name = "pablo-icpu-kunit-test",
	.init = pablo_icpu_kunit_test_init,
	.exit = pablo_icpu_kunit_test_exit,
	.test_cases = pablo_icpu_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_kunit_test_suite);

MODULE_LICENSE("GPL");
