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
#include "pablo-hw-api-common.h"

static struct is_reg reg = {0x0, "pkt_reg"};
static const struct is_field fld = {"pkt_field", 3, 8, RW, 0x0};

static struct pablo_kunit_test_ctx {
	void *base;
} pkt_ctx;

static void pablo_hw_api_common_reg_kunit_test(struct kunit *test)
{
	u32 val;
	u32 *cr;

	/* TC #1. Write single CR */
	reg.sfr_offset = 0x4;
	cr = (u32 *)(pkt_ctx.base + reg.sfr_offset);
	val = __LINE__;

	is_hw_set_reg(pkt_ctx.base, &reg, val);
	KUNIT_EXPECT_EQ(test, *cr, val);

	/* TC #2. Read single CR */
	reg.sfr_offset = 0x8;
	cr = (u32 *)(pkt_ctx.base + reg.sfr_offset);
	*cr = __LINE__;

	val = is_hw_get_reg(pkt_ctx.base, &reg);
	KUNIT_EXPECT_EQ(test, val, *cr);
}

static void pablo_hw_api_common_reg_u8_kunit_test(struct kunit *test)
{
	u8 val;
	u8 *cr;

	/* TC #1. Write single CR */
	reg.sfr_offset = 0x1;
	cr = (u8 *)(pkt_ctx.base + reg.sfr_offset);
	val = (u8)(__LINE__ & 0xFF);

	is_hw_set_reg_u8(pkt_ctx.base, &reg, val);
	KUNIT_EXPECT_EQ(test, *cr, val);

	/* TC #2. Read single CR */
	reg.sfr_offset = 0x5;
	cr = (u8 *)(pkt_ctx.base + reg.sfr_offset);
	*cr = (u8)(__LINE__ & 0xFF);

	val = is_hw_get_reg_u8(pkt_ctx.base, &reg);
	KUNIT_EXPECT_EQ(test, val, *cr);
}

static void pablo_hw_api_common_field_kunit_test(struct kunit *test)
{
	u32 src_val, dst_val;
	u32 msk = GENMASK(fld.bit_width - 1, 0);
	u32 *cr;

	/* TC #1. Write single field */
	reg.sfr_offset = 0x4;
	cr = (u32 *)(pkt_ctx.base + reg.sfr_offset);
	src_val = __LINE__ & msk;

	is_hw_set_field(pkt_ctx.base, &reg, &fld, src_val);
	dst_val = ((*cr) >> fld.bit_start) & msk;
	KUNIT_EXPECT_EQ(test, dst_val, src_val);

	/* TC #2. Read single field */
	reg.sfr_offset = 0x8;
	cr = (u32 *)(pkt_ctx.base + reg.sfr_offset);
	src_val = __LINE__ & msk;
	(*cr) = src_val << fld.bit_start;

	dst_val = is_hw_get_field(pkt_ctx.base, &reg, &fld);
	KUNIT_EXPECT_EQ(test, dst_val, src_val);
}

static void pablo_hw_api_common_dump_regs_kunit_test(struct kunit *test)
{
	reg.sfr_offset = 0x4;
	is_hw_dump_regs(pkt_ctx.base, &reg, 1);

	is_hw_dump_regs_hex(pkt_ctx.base, __func__, 1);

	is_hw_cdump_regs(pkt_ctx.base, &reg, 1);
}

static int pablo_hw_api_common_kunit_test_init(struct kunit *test)
{
	pkt_ctx.base = kunit_kzalloc(test, 0x10, 0);

	return 0;
}

static void pablo_hw_api_common_kunit_test_exit(struct kunit *test)
{
	if (pkt_ctx.base)
		kunit_kfree(test, pkt_ctx.base);
}

static struct kunit_case pablo_hw_api_common_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_common_reg_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_reg_u8_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_field_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dump_regs_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_common_kunit_test_suite = {
	.name = "pablo-hw-api-common-kunit-test",
	.init = pablo_hw_api_common_kunit_test_init,
	.exit = pablo_hw_api_common_kunit_test_exit,
	.test_cases = pablo_hw_api_common_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_common_kunit_test_suite);

MODULE_LICENSE("GPL");
