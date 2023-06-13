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
#include "pablo-kunit-test-utils.h"
#include "votf/pablo-votf.h"
#include "is-core.h"
#include "is-hw-chain.h"
#include "pablo-lib.h"

extern struct pablo_kunit_subdev_mcs_func *pablo_kunit_get_subdev_mcs_func(void);
static struct pablo_kunit_subdev_mcs_func *fn;
static struct is_device_ischain *idi;
static struct is_frame *frame;

static struct pablo_kunit_test_ctx {
	struct votf_dev *votf_dev_mock[VOTF_DEV_NUM];
	struct votf_dev *votf_dev[VOTF_DEV_NUM];
} pkt_ctx;

static void pablo_subdev_mcs_tag_hf_kunit_test(struct kunit *test)
{
	int ret;
	struct param_mcs_output *mcs_output;
	struct is_framemgr *votf_framemgr;
	dma_addr_t dva = 0x12345678;
	struct pablo_lib_platform_data *plpd = pablo_lib_get_platform_data();
	struct pablo_internal_subdev *subdev = &plpd->sd_votf[PABLO_LIB_I_SUBDEV_VOTF_HF];

	/* VOTF OFF */
	ret = fn->mcs_tag_hf(idi, frame);
	KUNIT_EXPECT_TRUE(test, ret == 0);

	/* VOTF ON */
	mcs_output = pablo_kunit_get_param_from_device(idi, PARAM_MCS_OUTPUT5);
	mcs_output->dma_cmd = DMA_OUTPUT_VOTF_ENABLE;
	mcs_output->plane = 1;
	set_bit(PABLO_SUBDEV_ALLOC, &subdev->state);

	ret = fn->mcs_tag_hf(idi, frame);
	KUNIT_EXPECT_TRUE(test, ret == -EINVAL);
	ret = pablo_kunit_compare_param(frame, PARAM_MCS_OUTPUT5, mcs_output);
	KUNIT_EXPECT_TRUE(test, ret == 0);

	/* VOTF ON & set VOTF FRAME */
	votf_framemgr = GET_SUBDEV_I_FRAMEMGR(subdev);
	votf_framemgr->frames = kunit_kzalloc(test, sizeof(struct is_frame), 0);
	votf_framemgr->frames[0].dvaddr_buffer[0] = dva;
	votf_framemgr->frames[0].planes = 1;

	ret = fn->mcs_tag_hf(idi, frame);
	KUNIT_EXPECT_TRUE(test, ret == 0);
	KUNIT_EXPECT_TRUE(test, frame->sc5TargetAddress[0] == dva);
	KUNIT_EXPECT_TRUE(test, frame->dva_rgbp_hf[0] == dva);

	kunit_kfree(test, votf_framemgr->frames);
	clear_bit(PABLO_SUBDEV_ALLOC, &subdev->state);
}

static void pablo_subdev_mcs_kunit_test_init_votfdev(struct kunit *test)
{
	struct votf_dev *dev, *org_dev;
	u32 dev_id, ip;
	void *votf_addr;

	for (dev_id = 0; dev_id < VOTF_DEV_NUM; dev_id++) {
		org_dev = get_votf_dev(dev_id);
		if (!org_dev)
			continue;

		/* Copy original dev infos */
		dev = kunit_kzalloc(test, sizeof(struct votf_dev), 0);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);
		memcpy(dev, org_dev, sizeof(struct votf_dev));

		/* Set dummy VOTF addr region */
		for (ip = 0; ip < IP_MAX; ip++) {
			votf_addr = kunit_kzalloc(test, 0x10000, 0);
			KUNIT_ASSERT_NOT_ERR_OR_NULL(test, votf_addr);
			dev->votf_addr[ip] = votf_addr;
		}


		pkt_ctx.votf_dev[dev_id] = org_dev;
		pkt_ctx.votf_dev_mock[dev_id] = dev;

		set_votf_dev(dev_id, dev);
	}
}

static void pablo_subdev_mcs_kunit_test_deinit_votfdev(struct kunit *test)
{
	struct votf_dev *dev;
	u32 dev_id, ip;
	void *votf_addr;

	for (dev_id = 0; dev_id < VOTF_DEV_NUM; dev_id++) {
		dev = pkt_ctx.votf_dev_mock[dev_id];
		if (!dev)
			continue;

		for (ip = 0; ip < IP_MAX; ip++) {
			votf_addr = dev->votf_addr[ip];
			if (!votf_addr)
				continue;

			kunit_kfree(test, votf_addr);
			dev->votf_addr[ip] = NULL;
		}

		set_votf_dev(dev_id, pkt_ctx.votf_dev[dev_id]);
		kunit_kfree(test, pkt_ctx.votf_dev_mock[dev_id]);

		pkt_ctx.votf_dev[dev_id] = NULL;
		pkt_ctx.votf_dev_mock[dev_id] = NULL;
	}
}

static int pablo_subdev_mcs_kunit_test_init(struct kunit *test)
{
	fn = pablo_kunit_get_subdev_mcs_func();
	idi = kunit_kzalloc(test, sizeof(struct is_device_ischain), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, idi);

	idi->is_region = kunit_kzalloc(test, sizeof(struct is_region), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, idi->is_region);

	frame = kunit_kzalloc(test, sizeof(struct is_frame), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);

	frame->parameter = kunit_kzalloc(test, sizeof(struct is_param_region), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame->parameter);

	pablo_subdev_mcs_kunit_test_init_votfdev(test);

	return 0;
}

static void pablo_subdev_mcs_kunit_test_exit(struct kunit *test)
{
	pablo_subdev_mcs_kunit_test_deinit_votfdev(test);
	kunit_kfree(test, frame->parameter);
	kunit_kfree(test, frame);
	kunit_kfree(test, idi->is_region);
	kunit_kfree(test, idi);
}

static struct kunit_case pablo_subdev_mcs_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_mcs_tag_hf_kunit_test),
	{},
};

struct kunit_suite pablo_subdev_mcs_kunit_test_suite = {
	.name = "pablo-subdev-mcs-kunit-test",
	.init = pablo_subdev_mcs_kunit_test_init,
	.exit = pablo_subdev_mcs_kunit_test_exit,
	.test_cases = pablo_subdev_mcs_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_mcs_kunit_test_suite);

MODULE_LICENSE("GPL v2");
