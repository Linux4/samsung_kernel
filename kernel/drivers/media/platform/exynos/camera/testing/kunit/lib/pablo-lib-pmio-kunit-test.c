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
#include "pmio.h"
#include "is-type.h"

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
#define KUNIT_R_WR_BULK		0x0200
#define KUNIT_R_MULTI		0x0300
#define KUNIT_R_CACHED		0x0400
#define KUNIT_R_LAST		0x0FFC
#define KUNIT_R_INVALID		KUNIT_R_LAST
#define KUNIT_R_INVALID_STRIDE	(KUNIT_R_RW + 1)

#define MMIO_BASE		0x2000
#define MMIO_SIZE		0x8000
#define MMIO_LIMIT		(MMIO_BASE + MMIO_SIZE)

#define TEST_VAL		0xCAFEBABE
#define TEST_DEFAULT_CACHE_TYPE	PMIO_CACHE_FLAT

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

static const enum pmio_cache_type test_cache_type[] = {
	PMIO_CACHE_FLAT,
	PMIO_CACHE_FLAT_THIN,
	PMIO_CACHE_FLAT_EAGER,
	PMIO_CACHE_FLAT_COREX,
};

enum kunit_fields {
	KUNIT_F_0_0,
	KUNIT_F_1_2,
	KUNIT_F_4_7,
	KUNIT_F_8_15,
	KUNIT_F_16_31,
};

static struct pmio_field_desc kunit_field_descs[] = {
	[KUNIT_F_0_0] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 0, 0),
	[KUNIT_F_1_2] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 1, 2),
	[KUNIT_F_4_7] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 4, 7),
	[KUNIT_F_8_15] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 8, 15),
	[KUNIT_F_16_31] = PMIO_FIELD_DESC(KUNIT_R_FIELD, 16, 31),
};

static struct pmio_reg_def kunit_reg_def[] = {
	{ KUNIT_R_RW, 1 },
	{ KUNIT_R_WR, 2 },
};

struct pmio_config kunit_pmio_config;
static struct pablo_mmio *kunit_pmio;
static struct pmio_field *kunit_pmio_fields;
static void *kunit_memory;

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

static void kunit_pmio_config_preset(enum pmio_cache_type cache_type)
{
	memset(&kunit_pmio_config, 0, sizeof(struct pmio_config));

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

	kunit_pmio_config.cache_type = cache_type;
	kunit_pmio_config.max_register = KUNIT_R_LAST;
	kunit_pmio_config.num_reg_defaults_raw = (KUNIT_R_LAST >> 2) + 1;
	kunit_pmio_config.phys_base = 0xBEEEEEEF;

	kunit_pmio_config.ranges = kunit_range_cfgs;
	kunit_pmio_config.num_ranges = ARRAY_SIZE(kunit_range_cfgs);

	kunit_pmio_config.fields = kunit_field_descs;
	kunit_pmio_config.num_fields = ARRAY_SIZE(kunit_field_descs);

	kunit_pmio_config.reg_defaults = kunit_reg_def;
	kunit_pmio_config.num_reg_defaults = 2;
}

static void kunit_pmio_cache_pre(struct kunit *test, enum pmio_cache_type cache_type)
{
	kunit_memory = kunit_kmalloc(test, MMIO_LIMIT, GFP_KERNEL);

	kunit_pmio_config_preset(cache_type);

	kunit_pmio = pmio_init(NULL, kunit_memory, &kunit_pmio_config);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, kunit_pmio);

	if (cache_type != PMIO_CACHE_NONE)
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio->cache_ops);
}

static void kunit_pmio_pre(struct kunit *test)
{
	kunit_pmio_cache_pre(test, PMIO_CACHE_NONE);
}

static void kunit_pmio_post(struct kunit *test)
{
	pmio_exit(kunit_pmio);
	kunit_kfree(test, kunit_memory);
}

/* Define the test cases. */
static void pablo_lib_pmio_init_kunit_test(struct kunit *test)
{
	const struct pmio_range_cfg kunit_invalid_range_cfgs[] = {
		{
			.name = "LUT",
			.min = KUNIT_R_WR_NOINC + 1,
			.max = KUNIT_R_WR_NOINC,
			.select_reg = KUNIT_R_SELECT,
			.select_val = 1,
			.offset_reg = KUNIT_R_OFFSET,
			.stride = 2,
			.count = 4,
		},
	};

	/* SUB TC: normal case test */

	kunit_pmio_pre(test);

	KUNIT_EXPECT_STREQ(test, pmio_name(kunit_pmio), kunit_pmio_config.name);

	kunit_pmio_post(test);

	kunit_memory = kunit_kmalloc(test, MMIO_LIMIT, GFP_KERNEL);

	/* SUB TC: config is NULL */
	kunit_pmio = pmio_init(NULL, kunit_memory, NULL);
	KUNIT_EXPECT_PTR_EQ(test, kunit_pmio, (struct pablo_mmio *)ERR_PTR(-EINVAL));

	/* SUB TC: invalid num_corexs */
	kunit_pmio_config_preset(PMIO_CACHE_NONE);
	kunit_pmio_config.num_corexs = PMIO_MAX_NUM_COREX + 1;
	kunit_pmio = pmio_init(NULL, kunit_memory, &kunit_pmio_config);
	KUNIT_EXPECT_PTR_EQ(test, kunit_pmio, (struct pablo_mmio *)ERR_PTR(-EINVAL));

	/* SUB TC: invalid range configs */
	kunit_pmio_config_preset(PMIO_CACHE_NONE);
	kunit_pmio_config.ranges = kunit_invalid_range_cfgs;
	kunit_pmio = pmio_init(NULL, kunit_memory, &kunit_pmio_config);
	KUNIT_EXPECT_PTR_EQ(test, kunit_pmio, (struct pablo_mmio *)ERR_PTR(-EINVAL));

	/* SUB TC: invalid default register config */
	kunit_pmio_config_preset(PMIO_CACHE_NONE);
	kunit_pmio_config.cache_type = PMIO_CACHE_FLAT;
	kunit_pmio_config.reg_defaults = NULL;
	kunit_pmio_config.num_reg_defaults = 1;
	kunit_pmio = pmio_init(NULL, kunit_memory, &kunit_pmio_config);
	KUNIT_EXPECT_PTR_EQ(test, kunit_pmio, (struct pablo_mmio *)ERR_PTR(-EINVAL));

	kunit_kfree(test, kunit_memory);
}

static void pablo_lib_pmio_reg_rw_kunit_test(struct kunit *test)
{
	int ret;
	unsigned int val;

	kunit_pmio_pre(test);

	if (IS_ENABLED(PMIO_CFG_RANGE_CHECK_FULL)) {
		ret = pmio_write(kunit_pmio, KUNIT_R_RD, 0xBAAAAAAD);
		KUNIT_EXPECT_EQ(test, ret, -EIO);
		ret = pmio_read(kunit_pmio, KUNIT_R_WR, &val);
		KUNIT_EXPECT_EQ(test, ret, -EIO);
	}

	ret = pmio_write(kunit_pmio, KUNIT_R_RW, 0xCAFEBABE);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = pmio_read(kunit_pmio, KUNIT_R_RW, &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, 0xCAFEBABE);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_reg_rw_relaxed_kunit_test(struct kunit *test)
{
	int ret;
	unsigned int val;

	kunit_pmio_pre(test);

	/* SUB TC: success pmio_write_relaxed() */
	ret = pmio_write_relaxed(kunit_pmio, KUNIT_R_RW, 0xCAFEBABE);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* SUB TC: success pmio_read_relaxed() */
	ret = pmio_read_relaxed(kunit_pmio, KUNIT_R_RW, &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, 0xCAFEBABE);

	/* TODO: test fail cases with cache config */

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_field_rw_kunit_test(struct kunit *test)
{
	int ret;
	unsigned int val;
	bool changed;

	kunit_pmio_pre(test);

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

	/* SUB TC: invalid register count */
	count = 0;
	ret = pmio_noinc_write(kunit_pmio, KUNIT_R_WR_NOINC, 2, val, count, false);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	kunit_pmio_post(test);

	kunit_kfree(test, val);
}

static void pablo_lib_pmio_bulk_rw_kunit_test(struct kunit *test)
{
	int ret;
	int i;
	size_t count = 2;
	const unsigned int w_values[2] = { 0xCAFEBABE, 0xCAFEBABF };
	unsigned int r_values[2];

	kunit_pmio_pre(test);

	/* SUB TC: success pmio_bulk_write() */
	ret = pmio_bulk_write(kunit_pmio, KUNIT_R_WR_BULK, w_values, count);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* SUB TC: success pmio_bulk_read() */
	ret = pmio_bulk_read(kunit_pmio, KUNIT_R_WR_BULK, r_values, count);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < count; i++)
		KUNIT_EXPECT_EQ(test, w_values[i], r_values[i]);

	/* SUB TC: success pmio_bulk_read() when cache_bypass is true */
	kunit_pmio->cache_bypass = true;
	ret = pmio_bulk_write(kunit_pmio, KUNIT_R_WR_BULK, w_values, count);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* SUB TC: invalid register count */
	count = 0;
	ret = pmio_bulk_write(kunit_pmio, KUNIT_R_WR_BULK, w_values, count);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_multi_write_kunit_test(struct kunit *test)
{
	int ret;
	int i;
	unsigned int val;
	size_t count = 2;
	const struct pmio_reg_seq kunit_test_reg_seq[2] = {
		{
			.reg = KUNIT_R_MULTI,
			.val = 0xCAFEBABE,
			.delay_us = 100,
		},
		{
			.reg = KUNIT_R_MULTI + 4,
			.val = 0xCAFEBABF,
			.delay_us = 100,
		},
	};

	kunit_pmio_pre(test);

	/* SUB TC: success pmio_multi_write() */
	ret = pmio_multi_write(kunit_pmio, kunit_test_reg_seq, count);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < count; ++i) {
		ret = pmio_read(kunit_pmio, kunit_test_reg_seq[i].reg, &val);
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, val, (unsigned int)kunit_test_reg_seq[i].val);
	}

	/* SUB TC: success pmio_multi_write_bypassed() */
	ret = pmio_multi_write_bypassed(kunit_pmio, kunit_test_reg_seq, count);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TODO: test fail cases with cache config */

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_test_bits_kunit_test(struct kunit *test)
{
	int ret;
	unsigned int val = 0xCAFEBABE;
	unsigned int test_bit;

	kunit_pmio_pre(test);

	ret = pmio_write(kunit_pmio, KUNIT_R_RW, val);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* SUB TC: test 0xE bits from val */
	test_bit = 0xE;
	ret = pmio_test_bits(kunit_pmio, KUNIT_R_RW, test_bit);
	KUNIT_EXPECT_EQ(test, ret, 1);

	/* SUB TC: test 0xF bits from val */
	test_bit = 0xF;
	ret = pmio_test_bits(kunit_pmio, KUNIT_R_RW, test_bit);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* SUB TC: test 0xCAFEBABE bits from val */
	test_bit = 0xCAFEBABE;
	ret = pmio_test_bits(kunit_pmio, KUNIT_R_RW, test_bit);
	KUNIT_EXPECT_EQ(test, ret, 1);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_field_alloc_kunit_test(struct kunit *test)
{
	struct pmio_field *test_pmio_field;

	kunit_pmio_pre(test);

	/* SUB TC: test first field */
	test_pmio_field = pmio_field_alloc(kunit_pmio, kunit_field_descs[KUNIT_F_0_0]);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, test_pmio_field);
	KUNIT_EXPECT_EQ(test, test_pmio_field->reg, (unsigned int)KUNIT_R_FIELD);
	KUNIT_EXPECT_EQ(test, test_pmio_field->shift, (unsigned int)0);
	KUNIT_EXPECT_EQ(test, test_pmio_field->mask, (unsigned int)GENMASK(0, 0));
	pmio_field_free(test_pmio_field);

	/* SUB TC: test middle field */
	test_pmio_field = pmio_field_alloc(kunit_pmio, kunit_field_descs[KUNIT_F_4_7]);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, test_pmio_field);
	KUNIT_EXPECT_EQ(test, test_pmio_field->reg, (unsigned int)KUNIT_R_FIELD);
	KUNIT_EXPECT_EQ(test, test_pmio_field->shift, (unsigned int)4);
	KUNIT_EXPECT_EQ(test, test_pmio_field->mask, (unsigned int)GENMASK(7, 4));
	pmio_field_free(test_pmio_field);

	/* SUB TC: test last field */
	test_pmio_field = pmio_field_alloc(kunit_pmio, kunit_field_descs[KUNIT_F_16_31]);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, test_pmio_field);
	KUNIT_EXPECT_EQ(test, test_pmio_field->reg, (unsigned int)KUNIT_R_FIELD);
	KUNIT_EXPECT_EQ(test, test_pmio_field->shift, (unsigned int)16);
	KUNIT_EXPECT_EQ(test, test_pmio_field->mask, (unsigned int)GENMASK(31, 16));
	pmio_field_free(test_pmio_field);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_set_f_corex_kunit_test(struct kunit *test)
{
	int ret;
	void __iomem *mmio_base;
	unsigned int val;
	unsigned int corex_reg;

	kunit_pmio_pre(test);

	kunit_pmio_fields = NULL;
	ret = pmio_field_bulk_alloc(kunit_pmio, &kunit_pmio_fields, kunit_pmio_config.fields,
				    kunit_pmio_config.num_fields);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio_fields);

	/* SUB TC: write to corex register */
	corex_reg = kunit_pmio->corex_mask | KUNIT_R_FIELD;
	mmio_base = pmio_get_base(kunit_pmio);
	PMIO_SET_F_COREX(kunit_pmio, corex_reg, KUNIT_F_0_0, 0x1);
	PMIO_SET_F_COREX(kunit_pmio, corex_reg, KUNIT_F_1_2, 0x3);
	PMIO_SET_F_COREX(kunit_pmio, corex_reg, KUNIT_F_4_7, 0xF);
	PMIO_SET_F_COREX(kunit_pmio, corex_reg, KUNIT_F_8_15, 0xFF);
	PMIO_SET_F_COREX(kunit_pmio, corex_reg, KUNIT_F_16_31, 0xFFFF);
	KUNIT_EXPECT_PTR_EQ(test, (void __iomem *)mmio_base, (void __iomem *)kunit_pmio->mmio_base);
	ret = pmio_read(kunit_pmio, corex_reg, &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, 0xFFFFFFF7);

	/* SUB TC: write to non corex register */
	mmio_base = pmio_get_base(kunit_pmio);
	PMIO_SET_F_COREX(kunit_pmio, KUNIT_R_FIELD, KUNIT_F_0_0, 0x1);
	PMIO_SET_F_COREX(kunit_pmio, KUNIT_R_FIELD, KUNIT_F_1_2, 0x0);
	PMIO_SET_F_COREX(kunit_pmio, KUNIT_R_FIELD, KUNIT_F_4_7, 0xF);
	PMIO_SET_F_COREX(kunit_pmio, KUNIT_R_FIELD, KUNIT_F_8_15, 0x0);
	PMIO_SET_F_COREX(kunit_pmio, KUNIT_R_FIELD, KUNIT_F_16_31, 0xFFFF);
	KUNIT_EXPECT_PTR_EQ(test, (void __iomem *)mmio_base, (void __iomem *)kunit_pmio->mmio_base);
	ret = pmio_read(kunit_pmio, KUNIT_R_FIELD, &val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, val, 0xFFFF00F1);

	pmio_field_bulk_free(kunit_pmio, kunit_pmio_fields);
	kunit_pmio_post(test);
}

static void pablo_lib_pmio_set_relaxed_io_kunit_test(struct kunit *test)
{
	kunit_pmio_pre(test);

	/* SUB TC: disable relaxed io */
	pmio_set_relaxed_io(kunit_pmio, false);
	KUNIT_EXPECT_FALSE(test, kunit_pmio->relaxed_io);

	/* SUB TC: enable relaxed io */
	pmio_set_relaxed_io(kunit_pmio, true);
	KUNIT_EXPECT_TRUE(test, kunit_pmio->relaxed_io);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_field_bulk_alloc_kunit_test(struct kunit *test)
{
	int ret;
	int num_fields;

	kunit_pmio_pre(test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, kunit_pmio);

	/* SUB TC : normal case */
	kunit_pmio_fields = NULL;
	ret = pmio_field_bulk_alloc(kunit_pmio, &kunit_pmio_fields, kunit_pmio_config.fields,
				    kunit_pmio_config.num_fields);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio_fields);

	pmio_field_bulk_free(kunit_pmio, kunit_pmio_fields);

	/* SUB TC : invalid num_fields */
	num_fields = 0;
	ret = pmio_field_bulk_alloc(kunit_pmio, &kunit_pmio_fields, kunit_pmio_config.fields,
				    num_fields);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_reset_cache_tester(struct kunit *test, enum pmio_cache_type cache_type)
{
	int ret;

	kunit_pmio_cache_pre(test, cache_type);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio->cache_ops->reset);

	ret = pmio_reset_cache(kunit_pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_reset_cache_kunit_test(struct kunit *test)
{
	int ret, i;

	kunit_pmio_pre(test);

	/* SUB TC : test non cache type */
	kunit_pmio->cache_type = PMIO_CACHE_NONE;
	ret = pmio_reset_cache(kunit_pmio);
	KUNIT_EXPECT_EQ(test, ret, -EPERM);

	/* SUB TC : cache type & invalid cache_ops */
	kunit_pmio->cache_type = PMIO_CACHE_FLAT;
	ret = pmio_reset_cache(kunit_pmio);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	kunit_pmio_post(test);

	for (i = 0; i < ARRAY_SIZE(test_cache_type); ++i)
		pablo_lib_pmio_reset_cache_tester(test, test_cache_type[i]);
}

static void pablo_lib_pmio_cache_init_kunit_test(struct kunit *test)
{
	int ret;

	kunit_pmio_pre(test);

	/* SUB TC : test non cache type */
	kunit_pmio->cache_type = PMIO_CACHE_NONE;
	ret = pmio_cache_init(kunit_pmio, &kunit_pmio_config);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, kunit_pmio->cache_bypass);

	/* test cache type */
	kunit_pmio->cache_type = PMIO_CACHE_FLAT;

	/* SUB TC: invalid num_reg_defaults */
	kunit_pmio_config.reg_defaults = kunit_reg_def;
	kunit_pmio_config.num_reg_defaults = 0;
	ret = pmio_cache_init(kunit_pmio, &kunit_pmio_config);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* SUB TC: invalid default register config */
	kunit_pmio_config.reg_defaults = NULL;
	kunit_pmio_config.num_reg_defaults = 2;
	ret = pmio_cache_init(kunit_pmio, &kunit_pmio_config);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* SUB TC: invalid stride register */
	kunit_reg_def[0].reg = KUNIT_R_INVALID_STRIDE;
	kunit_pmio_config.reg_defaults = kunit_reg_def;
	kunit_pmio_config.num_reg_defaults = 2;
	ret = pmio_cache_init(kunit_pmio, &kunit_pmio_config);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* SUB TC : invalid cache type */
	kunit_pmio->cache_type = PMIO_CACHE_INVALID;
	kunit_reg_def[0].reg = KUNIT_R_RW;
	kunit_pmio_config.reg_defaults = kunit_reg_def;
	kunit_pmio_config.num_reg_defaults = 2;
	ret = pmio_cache_init(kunit_pmio, &kunit_pmio_config);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* SUB TC : valid cache type */
	kunit_pmio->cache_type = PMIO_CACHE_FLAT;
	kunit_pmio_config.reg_defaults = kunit_reg_def;
	kunit_pmio_config.num_reg_defaults = 2;
	ret = pmio_cache_init(kunit_pmio, &kunit_pmio_config);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_raw_write_tester(struct kunit *test,
						  enum pmio_cache_type cache_type)
{
	int ret;
	size_t size = KUNIT_R_RAW_END - KUNIT_R_RAW_START + sizeof(u32);
	void *val = kunit_kzalloc(test, size, GFP_KERNEL);
	size_t count = size / sizeof(u32);
	u32 *regs;
	int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, val);

	kunit_pmio_cache_pre(test, cache_type);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio->cache_ops->raw_write);

	regs = val;
	for (i = 0; i < count; i++)
		regs[i] = 0xCAFEBABE + i;

	/* SUB TC: test pmio_raw_write()/pmio_raw_read() */
	ret = pmio_raw_write(kunit_pmio, KUNIT_R_RAW_START, val, size);
	KUNIT_EXPECT_EQ(test, ret, 0);

	memset(val, 0, size);
	ret = pmio_raw_read(kunit_pmio, KUNIT_R_RAW_START, val, size);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < count; i++)
		KUNIT_EXPECT_EQ(test, regs[i], 0xCAFEBABE + i);

	/* SUB TC: test pmio_cache_raw_write()/pmio_cache_raw_read() */
	ret = pmio_cache_raw_write(kunit_pmio, KUNIT_R_RAW_START, val, size);
	KUNIT_EXPECT_EQ(test, ret, 0);

	memset(val, 0, size);
	ret = pmio_raw_read(kunit_pmio, KUNIT_R_RAW_START, val, size);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < count; i++)
		KUNIT_EXPECT_EQ(test, regs[i], 0xCAFEBABE + i);

	kunit_pmio_post(test);

	kunit_kfree(test, val);
}

static void pablo_lib_pmio_cache_raw_write_kunit_test(struct kunit *test)
{
	int i, ret;
	const struct pmio_cache_ops pmio_cache_empty_ops;
	const unsigned int w_values[2] = { 0xCAFEBABE, 0xCAFEBABF };

	/* SUB TC: test non cache type */
	kunit_pmio_pre(test);

	ret = pmio_cache_raw_write(kunit_pmio, KUNIT_R_RAW_START, w_values, /* size */ 2);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_pmio_post(test);

	/* SUB TC: test empty pmio_cache_ops without .raw_write function pointer */
	kunit_pmio_cache_pre(test, TEST_DEFAULT_CACHE_TYPE);

	kunit_pmio->cache_ops = &pmio_cache_empty_ops;
	ret = pmio_cache_raw_write(kunit_pmio, KUNIT_R_RAW_START, w_values, /* size */ 2);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	kunit_pmio_post(test);

	/* SUB TC: test each cache type */
	for (i = 0; i < ARRAY_SIZE(test_cache_type) - 1; ++i)
		pablo_lib_pmio_cache_raw_write_tester(test, test_cache_type[i]);
}

static void pablo_lib_pmio_cache_rw_tester(struct kunit *test, enum pmio_cache_type cache_type)
{
	int ret;
	unsigned int wr_val;
	unsigned int rd_val;

	kunit_pmio_cache_pre(test, cache_type);

	wr_val = TEST_VAL;
	ret = pmio_cache_write(kunit_pmio, KUNIT_R_CACHED, wr_val);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pmio_cache_read(kunit_pmio, KUNIT_R_CACHED, &rd_val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, wr_val, rd_val);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_rw_kunit_test(struct kunit *test)
{
	int ret, i;
	unsigned int wr_val;
	unsigned int rd_val;

	kunit_pmio_cache_pre(test, PMIO_CACHE_NONE);

	/* SUB TC : write with none cache type */
	wr_val = TEST_VAL;
	ret = pmio_cache_write(kunit_pmio, KUNIT_R_CACHED, wr_val);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* SUB TC : read with none cache type */
	ret = pmio_cache_read(kunit_pmio, KUNIT_R_CACHED, &rd_val);
	KUNIT_EXPECT_EQ(test, ret, -EPERM);

	kunit_pmio_post(test);

	/* SUB TC : test each cache type */
	for (i = 0; i < ARRAY_SIZE(test_cache_type); ++i)
		pablo_lib_pmio_cache_rw_tester(test, test_cache_type[i]);
}

static void pablo_lib_pmio_cached_kunit_test(struct kunit *test)
{
	int ret;
	bool result;
	unsigned int wr_val;

	kunit_pmio_pre(test);

	/* SUB TC : invalid register */
	result = pmio_cached(kunit_pmio, KUNIT_R_INVALID);
	KUNIT_EXPECT_FALSE(test, result);

	/* SUB TC : test non cache type */
	result = pmio_cached(kunit_pmio, KUNIT_R_CACHED);
	KUNIT_EXPECT_FALSE(test, result);

	kunit_pmio_post(test);

	kunit_pmio_cache_pre(test, PMIO_CACHE_FLAT);

	wr_val = TEST_VAL;
	ret = pmio_cache_write(kunit_pmio, KUNIT_R_CACHED, wr_val);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* SUB TC : test cache type */
	result = pmio_cached(kunit_pmio, KUNIT_R_CACHED);
	KUNIT_EXPECT_TRUE(test, result);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_sync_tester(struct kunit *test, enum pmio_cache_type cache_type)
{
	int ret;
	unsigned int wr_val;

	kunit_pmio_cache_pre(test, cache_type);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio->cache_ops->write);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio->cache_ops->sync);

	/* SUB TC: test pmio_cache_sync_block_raw() when use_single_write is disabled */
	wr_val = TEST_VAL;
	pmio_cache_set_only(kunit_pmio, true);
	ret = pmio_write(kunit_pmio, KUNIT_R_FIELD, wr_val);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, kunit_pmio->cache_dirty);

	kunit_pmio->use_single_write = false;
	ret = pmio_cache_sync(kunit_pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, kunit_pmio->cache_dirty);

	/* SUB TC: test pmio_cache_sync_block_single() when use_single_write is enabled */
	wr_val = TEST_VAL + 1;
	ret = pmio_write(kunit_pmio, KUNIT_R_FIELD, wr_val);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_pmio->use_single_write = true;
	ret = pmio_cache_sync(kunit_pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, kunit_pmio->cache_bypass);
	KUNIT_EXPECT_FALSE(test, kunit_pmio->cache_dirty);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_sync_kunit_test(struct kunit *test)
{
	int ret, i;
	const struct pmio_cache_ops *pmio_cache_ops_backup;
	struct pmio_cache_ops pmio_cache_test_ops = {
		.type = PMIO_CACHE_FLAT,
		.name = "test",
	};

	kunit_pmio_cache_pre(test, PMIO_CACHE_FLAT);
	pmio_cache_ops_backup = kunit_pmio->cache_ops;

	/* SUB TC : cache_ops is NULL */
	kunit_pmio->cache_ops = NULL;
	ret = pmio_cache_sync(kunit_pmio);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* SUB TC : cache_dirty is false */
	kunit_pmio->cache_ops = pmio_cache_ops_backup;
	kunit_pmio->cache_dirty = false;
	ret = pmio_cache_sync(kunit_pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* SUB TC : test pmio_cache_default_sync() when only cache_ops.sync is NULL */
	pmio_cache_test_ops.read = pmio_cache_ops_backup->read;
	pmio_cache_test_ops.write = pmio_cache_ops_backup->write;
	pmio_cache_test_ops.sync = NULL;
	kunit_pmio->cache_ops = &pmio_cache_test_ops;

	pmio_cache_set_only(kunit_pmio, true);
	KUNIT_EXPECT_TRUE(test, kunit_pmio->cache_only);

	ret = pmio_write(kunit_pmio, KUNIT_R_FIELD, TEST_VAL);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, kunit_pmio->cache_dirty);

	ret = pmio_cache_sync(kunit_pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, kunit_pmio->cache_dirty);

	kunit_pmio_post(test);

	/* SUB TC : test each cache type */
	for (i = 0; i < ARRAY_SIZE(test_cache_type); ++i)
		pablo_lib_pmio_cache_sync_tester(test, test_cache_type[i]);
}

static void pablo_lib_pmio_cache_fsync_tester(struct kunit *test, enum pmio_cache_type cache_type)
{
	int ret;
	struct c_loader_buffer pair_clb;
	struct c_loader_buffer inc_clb;
	const u32 clb_cnt = 2;
	const u32 count = 2;
	const unsigned int w_values[2] = { 0xCAFEBABE, 0xCAFEBABF };

	kunit_pmio_cache_pre(test, cache_type);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio->cache_ops->write);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio->cache_ops->fsync);

	pmio_cache_set_only(kunit_pmio, true);
	kunit_pmio->no_sync_defaults = true;

	/* SUB TC : test PMIO_FORMATTER_PAIR fsync */
	ret = pmio_write(kunit_pmio, KUNIT_R_FIELD, TEST_VAL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pmio_raw_write(kunit_pmio, KUNIT_R_RAW_START, w_values, count * PMIO_REG_STRIDE);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pair_clb.clh = kunit_kzalloc(test, sizeof(struct c_loader_header) * clb_cnt, GFP_KERNEL);
	pair_clb.clp = kunit_kzalloc(test, sizeof(struct c_loader_payload) * clb_cnt, GFP_KERNEL);

	ret = pmio_cache_fsync(kunit_pmio, &pair_clb, PMIO_FORMATTER_PAIR);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, pair_clb.clh);
	kunit_kfree(test, pair_clb.clp);

	/* SUB TC : test PMIO_FORMATTER_INC fsync */
	ret = pmio_write(kunit_pmio, KUNIT_R_FIELD, TEST_VAL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pmio_raw_write(kunit_pmio, KUNIT_R_RAW_START, w_values, count * PMIO_REG_STRIDE);
	KUNIT_EXPECT_EQ(test, ret, 0);

	inc_clb.clh = kunit_kzalloc(test, sizeof(struct c_loader_header) * clb_cnt, GFP_KERNEL);
	inc_clb.clp = kunit_kzalloc(test, sizeof(struct c_loader_payload) * clb_cnt, GFP_KERNEL);

	ret = pmio_cache_fsync(kunit_pmio, &inc_clb, PMIO_FORMATTER_INC);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, inc_clb.clh);
	kunit_kfree(test, inc_clb.clp);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_fsync_kunit_test(struct kunit *test)
{
	int i;

	/* SUB TC : test each cache type */
	for (i = 0; i < ARRAY_SIZE(test_cache_type); ++i)
		pablo_lib_pmio_cache_fsync_tester(test, test_cache_type[i]);
}

static void pablo_lib_pmio_cache_set_bypass_kunit_test(struct kunit *test)
{
	bool bypass_enable;

	kunit_pmio_cache_pre(test, TEST_DEFAULT_CACHE_TYPE);

	/* SUB TC: disable bypass */
	bypass_enable = false;
	pmio_cache_set_bypass(kunit_pmio, bypass_enable);
	KUNIT_EXPECT_FALSE(test, kunit_pmio->cache_bypass);

	/* SUB TC: can't set bypass if cache_only */
	kunit_pmio->cache_only = true;
	bypass_enable = true;
	pmio_cache_set_bypass(kunit_pmio, bypass_enable);
	KUNIT_EXPECT_FALSE(test, kunit_pmio->cache_bypass);

	/* SUB TC: enable bypass */
	kunit_pmio->cache_only = false;
	bypass_enable = true;
	pmio_cache_set_bypass(kunit_pmio, bypass_enable);
	KUNIT_EXPECT_TRUE(test, kunit_pmio->cache_bypass);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_mark_dirty_kunit_test(struct kunit *test)
{
	kunit_pmio_cache_pre(test, TEST_DEFAULT_CACHE_TYPE);

	kunit_pmio->cache_dirty = false;
	kunit_pmio->no_sync_defaults = false;
	pmio_cache_mark_dirty(kunit_pmio);
	KUNIT_EXPECT_TRUE(test, kunit_pmio->cache_dirty);
	KUNIT_EXPECT_TRUE(test, kunit_pmio->no_sync_defaults);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_lookup_reg_kunit_test(struct kunit *test)
{
	int ret;

	kunit_pmio_cache_pre(test, TEST_DEFAULT_CACHE_TYPE);

	/* SUB TC: success lookup */
	ret = pmio_cache_lookup_reg(kunit_pmio, KUNIT_R_RW);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* SUB TC: fail lookup */
	ret = pmio_cache_lookup_reg(kunit_pmio, KUNIT_R_INVALID);
	KUNIT_EXPECT_EQ(test, ret, -ENOENT);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_drop_tester(struct kunit *test, enum pmio_cache_type cache_type)
{
	int ret;

	kunit_pmio_cache_pre(test, cache_type);

	ret = pmio_write(kunit_pmio, KUNIT_R_RW, TEST_VAL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pmio_cache_drop_region(kunit_pmio, 0, kunit_pmio_config.num_reg_defaults);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_drop_kunit_test(struct kunit *test)
{
	int i, ret;
	const struct pmio_cache_ops pmio_cache_empty_ops;

	/* SUB TC: test empty pmio_cache_ops without .drop function pointer */
	kunit_pmio_cache_pre(test, TEST_DEFAULT_CACHE_TYPE);

	kunit_pmio->cache_ops = &pmio_cache_empty_ops;
	ret = pmio_cache_drop_region(kunit_pmio, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	kunit_pmio_post(test);

	/* SUB TC : test each cache type */
	for (i = 0; i < ARRAY_SIZE(test_cache_type); ++i)
		pablo_lib_pmio_cache_drop_tester(test, test_cache_type[i]);
}

static void pablo_lib_pmio_cache_ext_fsync_kunit_test(struct kunit *test)
{
	int i, ret;
	struct c_loader_buffer clb;
	struct size_cr_set *scs;
	const u32 clb_set_cnt = 6;
	const unsigned int w_values[2] = { 0xCAFEBABE, 0xCAFEBABF };

	kunit_pmio_cache_pre(test, TEST_DEFAULT_CACHE_TYPE);

	scs = kunit_kzalloc(test, sizeof(struct size_cr_set), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, scs);

	clb.clh = kunit_kzalloc(test, sizeof(struct c_loader_header) * clb_set_cnt, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clb.clh);
	clb.clp = kunit_kzalloc(test, sizeof(struct c_loader_payload) * clb_set_cnt, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clb.clp);

	/* SUB TC : test pmio_cache_ext_fsync */
	ret = pmio_raw_write(kunit_pmio, KUNIT_R_RAW_START, w_values, 2 * PMIO_REG_STRIDE);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* add one cloader header + payload buffer */
	ret = pmio_cache_fsync(kunit_pmio, &clb, PMIO_FORMATTER_PAIR);
	KUNIT_EXPECT_EQ(test, ret, 0);

	scs->size = 10;
	for (i = 0; i < scs->size; ++i) {
		scs->cr[i].reg_addr = KUNIT_R_WR + (PMIO_REG_STRIDE * i);
		scs->cr[i].reg_data = TEST_VAL + i;
	}

	/* add two cloader header + payload buffer */
	ret = pmio_cache_ext_fsync(kunit_pmio, &clb, (const void *)scs, scs->size);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clb.num_of_headers, (u32)2);

	/* SUB TC : test pmio_cache_ext_fsync if clb->num_of_pairs is not zero */
	clb.num_of_pairs = 2;
	ret = pmio_cache_ext_fsync(kunit_pmio, &clb, (const void *)scs, scs->size);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, clb.num_of_headers, clb_set_cnt - 1);
	KUNIT_EXPECT_EQ(test, clb.num_of_pairs, (u32)0);

	kunit_kfree(test, clb.clh);
	kunit_kfree(test, clb.clp);
	kunit_kfree(test, scs);
	kunit_pmio_post(test);
}

static void pablo_lib_pmio_cache_fsync_noinc_kunit_test(struct kunit *test)
{
	int ret;
	struct c_loader_buffer clb;
	struct c_loader_header clh;
	struct c_loader_payload clp;
	const unsigned int w_values[2] = { 0xCAFEBABE, 0xCAFEBABF };

	kunit_pmio_cache_pre(test, TEST_DEFAULT_CACHE_TYPE);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio->cache_ops->write);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kunit_pmio->cache_ops->fsync);

	pmio_cache_set_only(kunit_pmio, true);
	ret = pmio_noinc_write(kunit_pmio, KUNIT_R_WR_NOINC, /* offset */ 0, w_values,
			       /* count */ 2, false);
	KUNIT_EXPECT_EQ(test, ret, 0);

	clb.clh = &clh;
	clb.clp = &clp;

	ret = pmio_cache_fsync(kunit_pmio, &clb, PMIO_FORMATTER_RPT);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_pmio_post(test);
}

static struct kunit_case pablo_lib_pmio_kunit_test_cases[] = {
	KUNIT_CASE(pablo_lib_pmio_init_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_reg_rw_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_reg_rw_relaxed_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_field_rw_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_raw_rw_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_noinc_write_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_bulk_rw_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_multi_write_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_test_bits_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_field_alloc_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_set_f_corex_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_set_relaxed_io_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_field_bulk_alloc_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_reset_cache_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_init_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_raw_write_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_rw_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cached_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_sync_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_fsync_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_set_bypass_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_mark_dirty_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_lookup_reg_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_drop_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_ext_fsync_kunit_test),
	KUNIT_CASE(pablo_lib_pmio_cache_fsync_noinc_kunit_test),
	{},
};

struct kunit_suite pablo_lib_pmio_kunit_test_suite = {
	.name = "pablo-lib-pmio-kunit-test",
	.test_cases = pablo_lib_pmio_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_lib_pmio_kunit_test_suite);

MODULE_LICENSE("GPL");
