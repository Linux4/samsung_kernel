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

#include "icpu/hardware/pablo-icpu-hw.h"

static struct icpu_platform_data test_pdata;
struct icpu_hw hw;

/* Define the test cases. */

static void pablo_icpu_hw_set_base_addr_kunit_test(struct kunit *test)
{
	int ret;
	u32 *base_addr;

	base_addr = test_pdata.mcuctl_reg_base;
	ret = HW_OPS(set_base_addr, test_pdata.mcuctl_reg_base, 0xFF);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, *(base_addr), (u32)1);
	KUNIT_EXPECT_EQ(test, *(base_addr + 1), (u32)1);
	KUNIT_EXPECT_EQ(test, *(base_addr + 2), (u32)0xFF);
	KUNIT_EXPECT_EQ(test, *(base_addr + 3), (u32)0xFF);

}

static void pablo_icpu_hw_misc_prepare_kunit_test(struct kunit *test)
{
	int ret;

	ret = HW_OPS(hw_misc_prepare);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_icpu_hw_reset_kunit_test(struct kunit *test)
{
	int ret;

	ret = HW_OPS(reset, test_pdata.sysreg_reset, test_pdata.sysreg_reset_bit, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = HW_OPS(reset, test_pdata.sysreg_reset, test_pdata.sysreg_reset_bit, 1);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static u32 __io_mem;
static void __io_write(u32 addr, u32 val)
{
	__io_mem = val;
}

static u32 __io_read(u32 addr)
{
	return __io_mem;
}

void icpu_hw_config_io_ops(void *write, void *read);
void icpu_hw_reset_io_ops(void);
static void pablo_icpu_hw_force_powerdown_kunit_test(struct kunit *test)
{
	struct icpu_io_sequence test_seq[3] = { 0, };

	test_pdata.num_force_powerdown_step = 3;
	test_pdata.force_powerdown_seq = test_seq;

	icpu_hw_config_io_ops(__io_write, __io_read);

	test_pdata.force_powerdown_seq[0].type = 0;
	test_pdata.force_powerdown_seq[0].addr = 0x100;
	test_pdata.force_powerdown_seq[0].mask = 0xFFFFFFFF;
	test_pdata.force_powerdown_seq[0].val = 0x123;
	test_pdata.force_powerdown_seq[0].timeout = 0;

	test_pdata.force_powerdown_seq[1].type = 1;
	test_pdata.force_powerdown_seq[1].addr = 0x100;
	test_pdata.force_powerdown_seq[1].mask = 0x123;
	test_pdata.force_powerdown_seq[1].val = 0x123;
	test_pdata.force_powerdown_seq[1].timeout = 1000;

	test_pdata.force_powerdown_seq[2].type = 0xBABEFACE;

	HW_OPS(force_powerdown, test_pdata.num_force_powerdown_step, test_pdata.force_powerdown_seq);

	icpu_hw_reset_io_ops();
}

static void pablo_icpu_hw_panic_handler_kunit_test(struct kunit *test)
{
	HW_OPS(panic_handler, test_pdata.sysctrl_reg_base);
}

static struct kunit_case pablo_icpu_hw_v1_0_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_hw_set_base_addr_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_misc_prepare_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_reset_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_force_powerdown_kunit_test),
	KUNIT_CASE(pablo_icpu_hw_panic_handler_kunit_test),
	{},
};

static int pablo_icpu_hw_v1_0_kunit_test_init(struct kunit *test)
{
	icpu_hw_init(&hw);

	test_pdata.mcuctl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_pdata.mcuctl_reg_base);

	test_pdata.sysctrl_reg_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_pdata.sysctrl_reg_base);

	return 0;
}

static void pablo_icpu_hw_v1_0_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_pdata.mcuctl_reg_base);
	kunit_kfree(test, test_pdata.sysctrl_reg_base);
}

struct kunit_suite pablo_icpu_hw_v1_0_kunit_test_suite = {
	.name = "pablo-icpu-hw-v1_0-kunit-test",
	.init = pablo_icpu_hw_v1_0_kunit_test_init,
	.exit = pablo_icpu_hw_v1_0_kunit_test_exit,
	.test_cases = pablo_icpu_hw_v1_0_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_hw_v1_0_kunit_test_suite);

MODULE_LICENSE("GPL");
