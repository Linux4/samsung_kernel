// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "pablo-hw-helper.h"
#include "is-hw-common-dma.h"

static struct is_hw_ip *hw_ip;
static struct is_frame *frame;
static void *addr_base;
static void *addr_contents;

static void pablo_hw_helper_dummy_kunit_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, 0);
}

static void pablo_hw_helper_probe_kunit_test(struct kunit *test)
{
	int ret;

	ret = pablo_hw_helper_probe(hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_set_rta_regs_kunit_test(struct kunit *test)
{
	bool ret;
	bool skip;
	struct size_cr_set *scs = (struct size_cr_set *)addr_contents;

	skip = true;
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_TRUE(test, (ret == false));

	skip = false;
	hw_ip->id = DEV_HW_BYRP;
	frame->kva_byrp_rta_info[PLANE_INDEX_CR_SET] = (u64)0;
	frame->kva_byrp_rta_info[PLANE_INDEX_CONFIG] = (u64)0;
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_TRUE(test, (ret == true));

	frame->kva_byrp_rta_info[PLANE_INDEX_CR_SET] = (u64)addr_contents;
	frame->kva_byrp_rta_info[PLANE_INDEX_CONFIG] = (u64)0;
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_TRUE(test, (ret == true));

	scs->size = 1;
	hw_ip->regs[REG_SETA] = addr_base;
	frame->kva_byrp_rta_info[PLANE_INDEX_CR_SET] = (u64)addr_contents;
	frame->kva_byrp_rta_info[PLANE_INDEX_CONFIG] = 0;
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_TRUE(test, (ret == true));

	scs->size = 1;
	hw_ip->regs[REG_SETA] = addr_base;
	frame->kva_byrp_rta_info[PLANE_INDEX_CR_SET] = (u64)addr_contents;
	frame->kva_byrp_rta_info[PLANE_INDEX_CONFIG] = (u64)addr_contents;
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_TRUE(test, (ret == false));

	hw_ip->id = DEV_HW_3AA0;
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_TRUE(test, ret);
}

#define LME_COREX_DIRECT_OFFSET	0x8000
#define COREX_DIRECT_OFFSET	0x0000
static void pablo_hw_helper_set_cr_set_kunit_test(struct kunit *test)
{
	int ret;
	u32 val_ret;
	u32 val_0 = 0x12345678;
	u32 val_1 = 0x87654321;
	struct size_cr_set *scs = (struct size_cr_set *)addr_contents;

	scs->size = 2;
	scs->cr[0].reg_addr = sizeof(u32) * 0;
	scs->cr[0].reg_data = val_0;
	scs->cr[1].reg_addr = sizeof(u32) * 1;
	scs->cr[1].reg_data = val_1;

	/* LME offset test */
	ret = pablo_hw_helper_set_cr_set(addr_base, LME_COREX_DIRECT_OFFSET,
					 scs->size, scs->cr);
	KUNIT_EXPECT_EQ(test, ret, 0);

	val_ret = *(u32 *)(addr_base + LME_COREX_DIRECT_OFFSET + sizeof(u32) * 0);
	KUNIT_EXPECT_EQ(test, val_ret, val_0);

	val_ret = *(u32 *)(addr_base + LME_COREX_DIRECT_OFFSET + sizeof(u32) * 1);
	KUNIT_EXPECT_EQ(test, val_ret, val_1);

	/* ISP/BYRP offset test */
	ret = pablo_hw_helper_set_cr_set(addr_base, COREX_DIRECT_OFFSET,
					 scs->size, scs->cr);
	KUNIT_EXPECT_EQ(test, ret, 0);

	val_ret = *(u32 *)(addr_base + COREX_DIRECT_OFFSET + sizeof(u32) * 0);
	KUNIT_EXPECT_EQ(test, val_ret, val_0);

	val_ret = *(u32 *)(addr_base + COREX_DIRECT_OFFSET + sizeof(u32) * 1);
	KUNIT_EXPECT_EQ(test, val_ret, val_1);

}

static int pablo_hw_helper_kunit_test_init(struct kunit *test)
{
	int ret;

	hw_ip = kunit_kzalloc(test, sizeof(struct is_hw_ip), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hw_ip);

	frame = kunit_kzalloc(test, sizeof(struct is_frame), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);

	hw_ip->hardware = kunit_kzalloc(test, sizeof(struct is_hardware), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hw_ip->hardware);

	ret = pablo_hw_chain_info_probe(hw_ip->hardware);
	KUNIT_EXPECT_EQ(test, ret, 0);

	addr_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr_base);
	addr_contents = kunit_kzalloc(test, sizeof(struct size_cr_set), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr_contents);

	ret = pablo_hw_helper_probe(hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_helper_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, addr_contents);
	kunit_kfree(test, addr_base);
	kunit_kfree(test, hw_ip->hardware);
	kunit_kfree(test, frame);
	kunit_kfree(test, hw_ip);
}

static struct kunit_case pablo_hw_helper_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_helper_dummy_kunit_test),
	KUNIT_CASE(pablo_hw_helper_probe_kunit_test),
	KUNIT_CASE(pablo_hw_helper_set_rta_regs_kunit_test),
	KUNIT_CASE(pablo_hw_helper_set_cr_set_kunit_test),
	{},
};

struct kunit_suite pablo_hw_helper_kunit_test_suite = {
	.name = "pablo-hw-helper-v2-kunit-test",
	.init = pablo_hw_helper_kunit_test_init,
	.exit = pablo_hw_helper_kunit_test_exit,
	.test_cases = pablo_hw_helper_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_helper_kunit_test_suite);

MODULE_LICENSE("GPL v2");
