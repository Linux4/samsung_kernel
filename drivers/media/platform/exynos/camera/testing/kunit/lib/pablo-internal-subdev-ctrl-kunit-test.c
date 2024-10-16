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
#include "pablo-internal-subdev-ctrl.h"
#include "is-core.h"

static void pablo_internal_subdev_ctrl_probe_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_internal_subdev *sd;
	struct is_core *core = is_get_is_core();
	struct is_mem *mem = CALL_HW_CHAIN_INFO_OPS(&core->hardware, get_iommu_mem, GROUP_ID_SS0);

	sd = kunit_kzalloc(test, sizeof(struct pablo_internal_subdev), GFP_KERNEL);
	KUNIT_ASSERT_TRUE(test, sd);

	ret = pablo_internal_subdev_probe(sd, 0, ENTRY_INTERNAL, mem, "KUNIT");
	KUNIT_EXPECT_TRUE(test, ret == 0);
	KUNIT_ASSERT_TRUE(test, sd->ops);
	KUNIT_EXPECT_TRUE(test, sd->id == ENTRY_INTERNAL);
	KUNIT_EXPECT_TRUE(test, sd->instance == 0);
	KUNIT_EXPECT_TRUE(test, sd->mem == mem);
	KUNIT_EXPECT_TRUE(test, !strcmp(sd->name, "KUNIT"));
	KUNIT_EXPECT_TRUE(test, sd->state == 0);
	KUNIT_EXPECT_TRUE(test, sd->secure == 0);

	kunit_kfree(test, sd);
}

static void pablo_internal_subdev_ctrl_free_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_internal_subdev *sd;
	struct is_core *core = is_get_is_core();
	struct is_mem *mem = CALL_HW_CHAIN_INFO_OPS(&core->hardware, get_iommu_mem, GROUP_ID_SS0);

	sd = kunit_kzalloc(test, sizeof(struct pablo_internal_subdev), GFP_KERNEL);
	KUNIT_ASSERT_TRUE(test, sd);

	pablo_internal_subdev_probe(sd, 0, ENTRY_INTERNAL, mem, "KUNIT");

	ret = CALL_I_SUBDEV_OPS(sd, free, sd);
	KUNIT_EXPECT_TRUE(test, ret == -EINVAL);

	kunit_kfree(test, sd);
}

static void pablo_internal_subdev_ctrl_alloc_kunit_test(struct kunit *test)
{
	int ret, i, j;
	struct pablo_internal_subdev *sd;
	struct is_core *core = is_get_is_core();
	struct is_mem *mem = CALL_HW_CHAIN_INFO_OPS(&core->hardware, get_iommu_mem, GROUP_ID_SS0);
	struct is_frame *frame;

	sd = kunit_kzalloc(test, sizeof(struct pablo_internal_subdev), GFP_KERNEL);
	KUNIT_ASSERT_TRUE(test, sd);

	pablo_internal_subdev_probe(sd, 0, ENTRY_INTERNAL, mem, "KUNIT");

	/* TC#1. invalid input case */
	sd->num_buffers = 9;
	ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
	KUNIT_EXPECT_TRUE(test, ret == -EINVAL);

	/* TC#2. system heap alloc */
	sd->width = 4032;
	sd->height = 3024;
	sd->num_planes = 2;
	sd->num_batch = 2;
	sd->num_buffers = 2;
	sd->bits_per_pixel = 16;
	sd->memory_bitwidth = 16;
	sd->size[0] = sd->height * ALIGN(DIV_ROUND_UP(sd->width * sd->memory_bitwidth, BITS_PER_BYTE), 32);
	sd->size[1] = sd->height / 2 * ALIGN(DIV_ROUND_UP(sd->width / 2 * sd->memory_bitwidth, BITS_PER_BYTE), 32);

	ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
	KUNIT_EXPECT_TRUE(test, ret == 0);
	KUNIT_EXPECT_TRUE(test, test_bit(PABLO_SUBDEV_ALLOC, &sd->state));

	for (i = 0; i < sd->num_batch; i++) {
		KUNIT_EXPECT_TRUE(test, sd->size[sd->num_planes * i + 0] == sd->size[0]);
		KUNIT_EXPECT_TRUE(test, sd->size[sd->num_planes * i + 1] == sd->size[1]);
	}

	for (i = 0; i < sd->num_buffers; i++) {
		frame = &sd->internal_framemgr.frames[i];

		for (j = 0; j < IS_MAX_PLANES && sd->size[j]; j++) {
			KUNIT_EXPECT_TRUE(test, frame->dvaddr_buffer[j]);
			KUNIT_EXPECT_TRUE(test, frame->kvaddr_buffer[j]);
		}
	}

	ret = CALL_I_SUBDEV_OPS(sd, free, sd);
	KUNIT_EXPECT_TRUE(test, ret == 0);
	KUNIT_EXPECT_TRUE(test, !test_bit(PABLO_SUBDEV_ALLOC, &sd->state));

	/* TC#3. alloc --> alloc */
	ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
	ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
	KUNIT_EXPECT_TRUE(test, ret == -EINVAL);

	/* TC#4. secure heap alloc is not available in kunit test */

	kunit_kfree(test, sd);
}

static struct kunit_case pablo_internal_subdev_ctrl_kunit_test_cases[] = {
	KUNIT_CASE(pablo_internal_subdev_ctrl_probe_kunit_test),
	KUNIT_CASE(pablo_internal_subdev_ctrl_free_kunit_test),
	KUNIT_CASE(pablo_internal_subdev_ctrl_alloc_kunit_test),
	{},
};

struct kunit_suite pablo_internal_subdev_ctrl_kunit_test_suite = {
	.name = "pablo-internal-subdev-ctrl-kunit-test",
	.test_cases = pablo_internal_subdev_ctrl_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_internal_subdev_ctrl_kunit_test_suite);

MODULE_LICENSE("GPL v2");
