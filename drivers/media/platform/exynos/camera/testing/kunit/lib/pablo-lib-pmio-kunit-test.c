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
#include "pablo-mmio.h"

#define KUNIT_R_RW		0x0000
#define KUNIT_R_RD		0x0008
#define KUNIT_R_WR		0x0010
#define KUNIT_R_VOLATILE	0x0018
#define KUNIT_R_SELECT		0x0040
#define KUNIT_R_OFFSET		0x0044
#define KUNIT_R_WR_NOINC	0x0048
#define KUNIT_R_FIELD		0x0068
#define KUNIT_R_RAW_START	0x0100
#define KUNIT_R_RAW_END		0x01FC
#define KUNIT_R_LAST		0x0FFC

#define MMIO_BASE		0x2000
#define MMIO_SIZE		0x8000
#define MMIO_LIMIT		(MMIO_BASE + MMIO_SIZE)

static const struct pmio_range kunit_wr_yes_ranges[] = {
	pmio_reg_range(KUNIT_R_RW, KUNIT_R_RW),
	pmio_reg_range(KUNIT_R_WR, KUNIT_R_FIELD),
	pmio_reg_range(KUNIT_R_RAW_START, KUNIT_R_RAW_END),
};

static const struct pmio_range kunit_wr_no_ranges[] = {
	pmio_reg_range(KUNIT_R_RD, KUNIT_R_RD),
};

static const struct pmio_access_table kunit_wr_table = {
	.yes_ranges	= kunit_wr_yes_ranges,
	.n_yes_ranges	= ARRAY_SIZE(kunit_wr_yes_ranges),
	.no_ranges	= kunit_wr_no_ranges,
	.n_no_ranges	= ARRAY_SIZE(kunit_wr_no_ranges),
};

static const struct pmio_range kunit_rd_ranges[] = {
	pmio_reg_range(KUNIT_R_RW, KUNIT_R_RD),
	pmio_reg_range(KUNIT_R_VOLATILE, KUNIT_R_FIELD),
};

static const struct pmio_access_table kunit_rd_table = {
	.yes_ranges	= kunit_rd_ranges,
	.n_yes_ranges	= ARRAY_SIZE(kunit_rd_ranges),
};

static const struct pmio_range kunit_volatile_ranges[] = {
	pmio_reg_range(KUNIT_R_VOLATILE, KUNIT_R_VOLATILE),
};

static const struct pmio_access_table kunit_volatile_table = {
	.yes_ranges	= kunit_volatile_ranges,
	.n_yes_ranges	= ARRAY_SIZE(kunit_volatile_ranges),
};

static const struct pmio_range kunit_wr_noinc_ranges[] = {
	pmio_reg_range(KUNIT_R_WR_NOINC, KUNIT_R_WR_NOINC),
};

static const struct pmio_access_table kunit_wr_noinc_table = {
	.yes_ranges      = kunit_wr_noinc_ranges,
	.n_yes_ranges    = ARRAY_SIZE(kunit_wr_noinc_ranges),
};

static const struct pmio_range_cfg kunit_range_cfgs[] = {
	{
		.name = "LUT",
		.min = KUNIT_R_WR_NOINC,
		.max = KUNIT_R_WR_NOINC,
		.select_reg = KUNIT_R_SELECT,
		.select_val = 1,
		.offset_reg = KUNIT_R_OFFSET,
		.stride = 2,
		.count = 4,
	},
};

enum kunit_fields {
	KUNIT_F_0_0,
	KUNIT_F_1_2,
	KUNIT_F_4_7,
	KUNIT_F_8_15,
	KUNIT_F_16_31,
};

struct pmio_field_desc kunit_field_descs[] = {
	[KUNIT_F_0_0] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 0, 0),
	[KUNIT_F_1_2] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 1, 2),
	[KUNIT_F_4_7] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 4, 7),
	[KUNIT_F_8_15] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 8, 15),
	[KUNIT_F_16_31] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 16, 31),
};

struct pmio_config kunit_pmio_config;
struct pablo_mmio *kunit_pmio;
struct pmio_field *kunit_pmio_fields;
void *kunit_memory;

static int kunit_reg_read(void *ctx, unsigned int reg, unsigned int *val)
{
	u32 *regs = ctx + MMIO_BASE;

	*val = regs[reg >> 2];

	return 0;
}

static int kunit_reg_write(void *ctx, unsigned int reg, unsigned int val)
{
	u32 *regs = ctx + MMIO_BASE;

	regs[reg >> 2] = val;

	return 0;
}

static void kunit_pmio_pre(struct kunit *test)
{
	kunit_memory = kunit_kmalloc(test, MMIO_LIMIT, GFP_KERNEL);

	kunit_pmio_config.name = "kunit";

	kunit_pmio_config.phys_base = 0x00BEEF00;
	kunit_pmio_config.mmio_base = (void *)(kunit_memory + MMIO_BASE);
	kunit_pmio_config.reg_read = kunit_reg_read;
	kunit_pmio_config.reg_write = kunit_reg_write;

	kunit_pmio_config.num_corexs = 2;
	kunit_pmio_config.corex_stride = (MMIO_SIZE / kunit_pmio_config.num_corexs);

	kunit_pmio_config.wr_table = &kunit_wr_table;
	kunit_pmio_config.rd_table = &kunit_rd_table;
	kunit_pmio_config.volatile_table = &kunit_volatile_table;
	kunit_pmio_config.wr_noinc_table = &kunit_wr_noinc_table;

	kunit_pmio_config.cache_type = PMIO_CACHE_NONE;
	kunit_pmio_config.max_register = KUNIT_R_LAST;
	kunit_pmio_config.num_reg_defaults_raw = (KUNIT_R_LAST >> 2) + 1;
	kunit_pmio_config.phys_base = 0xBEEEEEEF;

	kunit_pmio_config.ranges = kunit_range_cfgs;
	kunit_pmio_config.num_ranges = ARRAY_SIZE(kunit_range_cfgs);

	kunit_pmio_config.fields = kunit_field_descs;
	kunit_pmio_config.num_fields = ARRAY_SIZE(kunit_field_descs);

	kunit_pmio = pmio_init(NULL, kunit_memory, &kunit_pmio_config);
}

static void kunit_pmio_post(struct kunit *test)
{
	pmio_exit(kunit_pmio);
	kunit_kfree(test, kunit_memory);
}

/* Define the test cases. */
static void pablo_lib_pmio_init_kunit_test(struct kunit *test)
{
	kunit_pmio_pre(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, kunit_pmio);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_reg_rw_kunit_test(struct kunit *test)
{
	int ret;
	unsigned int val;

	kunit_pmio_pre(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, kunit_pmio);

	if (IS_ENABLED(PMIO_RANGE_CHECK_FULL)) {
		ret = pmio_write(kunit_pmio, KUNIT_R_RD, 0xBAAAAAAD);
		KUNIT_EXPECT_EQ(test, ret,  -EIO);
		ret = pmio_read(kunit_pmio, KUNIT_R_WR, &val);
		KUNIT_EXPECT_EQ(test, ret,  -EIO);
	}

	ret = pmio_write(kunit_pmio, KUNIT_R_RW, 0xCAFEBABE);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = pmio_read(kunit_pmio, KUNIT_R_RW, &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, 0xCAFEBABE);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_field_rw_kunit_test(struct kunit *test)
{
	int ret;
	unsigned int val;
	bool changed;

	kunit_pmio_pre(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, kunit_pmio);

	ret = pmio_write(kunit_pmio, KUNIT_R_FIELD, 0xFFFF00F7);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = pmio_read(kunit_pmio, KUNIT_R_FIELD, &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, 0xFFFF00F7);

	ret = pmio_field_bulk_alloc(kunit_pmio, &kunit_pmio_fields,
				    kunit_pmio_config.fields,
				    kunit_pmio_config.num_fields);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = pmio_field_read(kunit_pmio, KUNIT_R_FIELD,
			      &kunit_pmio_fields[KUNIT_F_0_0], &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0x1);
	ret = pmio_field_read(kunit_pmio, KUNIT_R_FIELD,
			      &kunit_pmio_fields[KUNIT_F_1_2], &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0x3);
	ret = pmio_field_read(kunit_pmio, KUNIT_R_FIELD,
			      &kunit_pmio_fields[KUNIT_F_4_7], &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0xF);
	ret = pmio_field_read(kunit_pmio, KUNIT_R_FIELD,
			      &kunit_pmio_fields[KUNIT_F_8_15], &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0x0);
	ret = pmio_field_read(kunit_pmio, KUNIT_R_FIELD,
			      &kunit_pmio_fields[KUNIT_F_16_31], &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0xFFFF);

	ret = pmio_field_write(kunit_pmio, KUNIT_R_FIELD,
			       &kunit_pmio_fields[KUNIT_F_0_0], 0x0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = pmio_field_read(kunit_pmio, KUNIT_R_FIELD,
			      &kunit_pmio_fields[KUNIT_F_0_0], &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0x0);

	ret = pmio_field_write(kunit_pmio, KUNIT_R_FIELD,
			       &kunit_pmio_fields[KUNIT_F_1_2], 0x1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = pmio_field_read(kunit_pmio, KUNIT_R_FIELD,
			      &kunit_pmio_fields[KUNIT_F_1_2], &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0x1);

	ret = pmio_field_update_bits_base(kunit_pmio, KUNIT_R_FIELD,
					  &kunit_pmio_fields[KUNIT_F_8_15],
					  0xFF, 0x0, false, &changed);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, changed);
	ret = pmio_field_read(kunit_pmio, KUNIT_R_FIELD,
			      &kunit_pmio_fields[KUNIT_F_8_15], &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0x0);

	ret = pmio_field_update_bits(kunit_pmio, KUNIT_R_FIELD,
				     &kunit_pmio_fields[KUNIT_F_16_31],
				     0xFF, 0x00);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = pmio_field_read(kunit_pmio, KUNIT_R_FIELD,
			      &kunit_pmio_fields[KUNIT_F_16_31], &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0xFF00);

	pmio_field_bulk_free(kunit_pmio, kunit_pmio_fields);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_raw_rw_kunit_test(struct kunit *test)
{
	int ret;
	size_t size = KUNIT_R_RAW_END - KUNIT_R_RAW_START + 4;
	void *val = kunit_kzalloc(test, size, GFP_KERNEL);
	size_t count = size >> 2;
	u32 *regs;
	int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, val);

	kunit_pmio_pre(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, kunit_pmio);

	regs = val;
	for (i = 0; i < count; i++)
		regs[i] = 0xCAFEBABE + i;

	ret = pmio_raw_write(kunit_pmio, KUNIT_R_RAW_START, val, size);
	KUNIT_EXPECT_EQ(test, ret, 0);

	memset(val, 0, size);
	ret = pmio_raw_read(kunit_pmio, KUNIT_R_RAW_START, val, size);
	KUNIT_EXPECT_EQ(test, ret, 0);
	for (i = 0; i < count; i++)
		KUNIT_EXPECT_EQ(test, regs[i], 0xCAFEBABE + i);

	kunit_pmio_post(test);

	kunit_kfree(test, val);
}

static void pablo_lib_pmio_noinc_write_kunit_test(struct kunit *test)
{
	int ret;
	size_t size = 0x20; /* stride * count * 4 */
	void *val = kunit_kzalloc(test, size, GFP_KERNEL);
	size_t count = size >> 3; /* stride == 2 */
	u32 *regs;
	unsigned int v;
	int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, val);

	kunit_pmio_pre(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, kunit_pmio);

	regs = val;
	for (i = 0; i < count; i++)
		regs[i] = 0xCAFEBABE + i;

	/* FIFO start			     FIFO end			*
	 * |				     |				*
	 * [[x][x]] [[x][x]] [[0][1]] [[2][3]] [[4][5]] [[6][7]]	*
	 * [[x][x]] [[x][x]] [[0][1]] [[2][3]] [[4][5]] [[6][7]]	*
	 *                   |               |	               |	*
	 *		     buffer start    |		     buffer end	*
	 *		     write start     write end			*/
	ret = pmio_noinc_write(kunit_pmio, KUNIT_R_WR_NOINC, 2, val, 2, false);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pmio_read(kunit_pmio, KUNIT_R_WR_NOINC, &v);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, v, 0xCAFEBAC1);

	ret = pmio_read(kunit_pmio, KUNIT_R_SELECT, &v);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, v, (unsigned int)1);

	ret = pmio_read(kunit_pmio, KUNIT_R_OFFSET, &v);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, v, (unsigned int)2);

	ret = pmio_noinc_write(kunit_pmio, KUNIT_R_WR_NOINC, 2, val, count, false);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	kunit_pmio_post(test);

	kunit_kfree(test, val);
}

static struct kunit_case pablo_lib_pmio_kunit_test_cases[] = {
	KUNIT_CASE(pablo_lib_pmio_init_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_reg_rw_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_field_rw_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_raw_rw_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_noinc_write_kunit_test),
	{},
};

struct kunit_suite pablo_lib_pmio_kunit_test_suite = {
	.name = "pablo-lib-pmio-kunit-test",
	.test_cases = pablo_lib_pmio_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_lib_pmio_kunit_test_suite);

MODULE_LICENSE("GPL");
