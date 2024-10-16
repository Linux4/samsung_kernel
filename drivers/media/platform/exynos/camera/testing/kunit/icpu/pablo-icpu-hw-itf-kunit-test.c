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

#include "pablo-kunit-test.h"

#include "icpu/hardware/pablo-icpu-hw-itf.h"
#include "icpu/hardware/pablo-icpu-hw.h"

static struct icpu_platform_data test_pdata;

/* Define the test cases. */

static void pablo_icpu_hw_set_icpu_base_address_kunit_test(struct kunit *test)
{
	int ret;
	u32 *base_addr;

	ret = pablo_icpu_hw_set_base_address(NULL, 0xFF);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	base_addr = test_pdata.mcuctl_reg_base;
	ret = pablo_icpu_hw_set_base_address(&test_pdata, 0xFF);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_icpu_hw_misc_prepare_kunit_test(struct kunit *test)
{
	pablo_icpu_hw_misc_prepare(&test_pdata);
}

static void pablo_icpu_hw_reset_kunit_test(struct kunit *test)
{
	int ret;

	ret = pablo_icpu_hw_reset(NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = pablo_icpu_hw_reset(&test_pdata, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pablo_icpu_hw_reset(NULL, 1);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = pablo_icpu_hw_reset(&test_pdata, 1);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_icpu_hw_num_tx_mbox_kunit_test(struct kunit *test)
{
	u32 num;

	num = pablo_icpu_hw_get_num_tx_mbox(NULL);
	KUNIT_EXPECT_EQ(test, num, (u32)0);

	num = pablo_icpu_hw_get_num_tx_mbox(&test_pdata);
	KUNIT_EXPECT_EQ(test, num, (u32)2);
}

static void pablo_icpu_hw_get_tx_info_kunit_test(struct kunit *test)
{
	void *info;
	int i;

	info = pablo_icpu_hw_get_tx_info(NULL, 0);
	KUNIT_EXPECT_PTR_EQ(test, info, NULL);

	for (i = 0; i < pablo_icpu_hw_get_num_tx_mbox(&test_pdata); i++) {
		info = pablo_icpu_hw_get_tx_info(&test_pdata, i);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, info);
	}
}

static void pablo_icpu_hw_num_rx_mbox_kunit_test(struct kunit *test)
{
	u32 num;

	num = pablo_icpu_hw_get_num_rx_mbox(NULL);
	KUNIT_EXPECT_EQ(test, num, (u32)0);

	num = pablo_icpu_hw_get_num_rx_mbox(&test_pdata);
	KUNIT_EXPECT_EQ(test, num, (u32)2);
}

static void pablo_icpu_hw_get_rx_info_kunit_test(struct kunit *test)
{
	void *info;
	int i;

	info = pablo_icpu_hw_get_tx_info(NULL, 0);
	KUNIT_EXPECT_PTR_EQ(test, info, NULL);

	for (i = 0; i < pablo_icpu_hw_get_num_tx_mbox(&test_pdata); i++) {
		info = pablo_icpu_hw_get_tx_info(&test_pdata, i);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, info);
	}
}

static void pablo_icpu_hw_num_channels_kunit_test(struct kunit *test)
{
	u32 num;

	num = pablo_icpu_hw_get_num_channels(NULL);
	KUNIT_EXPECT_EQ(test, num, (u32)0);

	num = pablo_icpu_hw_get_num_channels(&test_pdata);
	KUNIT_EXPECT_EQ(test, num, (u32)2);
}

static void pablo_icpu_hw_force_powerdown_kunit_test(struct kunit *test)
{
	pablo_icpu_hw_force_powerdown(NULL);
}

static void pablo_icpu_hw_panic_handler_kunit_test(struct kunit *test)
{
	pablo_icpu_hw_panic_handler(NULL);
}

static void pablo_icpu_hw_set_debug_reg_kunit_test(struct kunit *test)
{
	pablo_icpu_hw_set_debug_reg(&test_pdata, 0, 0xBABEFACE);
}

static struct kunit_case pablo_icpu_hw_itf_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_hw_set_icpu_base_address_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_misc_prepare_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_reset_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_num_tx_mbox_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_get_tx_info_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_num_rx_mbox_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_get_rx_info_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_num_channels_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_force_powerdown_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_panic_handler_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_set_debug_reg_kunit_test),
	{},
};

static int pablo_icpu_hw_itf_kunit_test_init(struct kunit *test)
{
	test_pdata.mcuctl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_pdata.mcuctl_reg_base);

	test_pdata.sysctrl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_pdata.sysctrl_reg_base);

	test_pdata.num_tx_mbox = 2;
	test_pdata.num_rx_mbox = 2;

	test_pdata.tx_infos = kunit_kzalloc(test, sizeof(struct icpu_mbox_tx_info) * test_pdata.num_tx_mbox, 0);
	test_pdata.rx_infos = kunit_kzalloc(test, sizeof(struct icpu_mbox_rx_info) * test_pdata.num_rx_mbox, 0);

	test_pdata.num_chans = 2;

	return 0;
}

static void pablo_icpu_hw_itf_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_pdata.tx_infos);
	kunit_kfree(test, test_pdata.rx_infos);

	kunit_kfree(test, test_pdata.mcuctl_reg_base);
	kunit_kfree(test, test_pdata.sysctrl_reg_base);
}

struct kunit_suite pablo_icpu_hw_itf_kunit_test_suite = {
	.name = "pablo-icpu-hw-itf-kunit-test",
	.init = pablo_icpu_hw_itf_kunit_test_init,
	.exit = pablo_icpu_hw_itf_kunit_test_exit,
	.test_cases = pablo_icpu_hw_itf_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_hw_itf_kunit_test_suite);

MODULE_LICENSE("GPL");
