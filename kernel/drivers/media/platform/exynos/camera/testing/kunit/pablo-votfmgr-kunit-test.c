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
#include "votf/pablo-votf.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "is-groupmgr.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is_groupmgr_config.h"
#include "is-hw.h"

/* Define the test cases. */
static struct pablo_votfmgr_test_ctx {
	struct is_device_ischain	device;
	struct is_group		group;
	struct is_group		group_tws;
	struct is_hw_ip		hw_ip;
	struct votf_ops 	vops;
	void *src_addr, *dst_addr;
	struct votf_dev *src_votf, *dst_votf;
	struct votf_info src_info, dst_info;
	int src_ip, dst_ip;
	struct is_subdev	leader;
	struct is_subdev	junction;
} test_ctx;

static void pablo_votfmgr_register_framemgr_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	int ret = 0;

	ret = is_votf_register_framemgr(&tctx->group, TRS, &tctx->hw_ip,
		&tctx->vops, tctx->group.leader.id);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = is_votf_register_framemgr(&tctx->group, TWS, &tctx->hw_ip,
		&tctx->vops, tctx->group.leader.id);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_votfmgr_get_framemgr_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	struct is_framemgr *votf_framemgr;
	struct is_framemgr *leader_framemgr = &tctx->group.leader.internal_framemgr;

	votf_framemgr = is_votf_get_framemgr(&tctx->group, TRS, tctx->group.leader.id);
	KUNIT_ASSERT_PTR_EQ(test, leader_framemgr, votf_framemgr);
}

static void pablo_votfmgr_get_frame_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	struct is_frame *votf_frame;

	votf_frame = is_votf_get_frame(&tctx->group, TRS, tctx->group.leader.id, 0);
	KUNIT_ASSERT_PTR_EQ(test, NULL, votf_frame);
}

static void pablo_votfmgr_create_link_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	int ret = 0;

	ret = is_votf_create_link(&tctx->group, 320, 240);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_votfmgr_destroy_link_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	int ret = 0;

	ret = is_votf_destroy_link(&tctx->group);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_votfmgr_check_link_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	int ret = 0;

	ret = is_votf_check_link(&tctx->group);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_votfmgr_force_flush_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	int ret = 0;

	ret = __is_votf_force_flush(&tctx->src_info, &tctx->dst_info);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_votfmgr_link_set_service_cfg_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	int ret = 0;

	ret = is_votf_link_set_service_cfg(&tctx->src_info, &tctx->dst_info,
		320, 240, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_votfmgr_check_wait_con_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	int ret = 0;

	ret = is_votf_check_wait_con(&tctx->group);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_votfmgr_check_invalid_state_kunit_test(struct kunit *test)
{
	struct pablo_votfmgr_test_ctx *tctx = &test_ctx;
	int ret = 0;

	ret = is_votf_check_invalid_state(&tctx->group);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static int pablo_test_votfitf_create_link(int src_ip, int dst_ip)
{
	return 0;
}

static int pablo_test_votfitf_set_flush(struct votf_info *vinfo)
{
	return 0;
}

static struct is_subdev *pablo_test_votf_get_slave_leader_subdev(
	struct is_group *group,
	enum votf_service type)
{
	return &test_ctx.group.leader;
}

static int pablo_test_votfitf_func(struct votf_info *s_vinfo, struct votf_info *d_vinfo)
{
	return 0;
}

static struct pablo_votfitf_ops pablo_test_votfitf_ops = {
	.check_wait_con = pablo_test_votfitf_func,
	.check_invalid_state = pablo_test_votfitf_func,
	.create_link = pablo_test_votfitf_create_link,
	.set_flush = pablo_test_votfitf_set_flush,
	.get_slave_leader_subdev = pablo_test_votf_get_slave_leader_subdev,
};

static struct pablo_votfitf_ops pablo_normal_votfitf_ops;
static void pablo_votfmgr_set_ops(struct pablo_votfitf_ops *dst_ops, struct pablo_votfitf_ops *src_ops)
{
	dst_ops->check_wait_con = src_ops->check_wait_con;
	dst_ops->check_invalid_state = src_ops->check_invalid_state;
	dst_ops->create_link = src_ops->create_link;
	dst_ops->set_flush = src_ops->set_flush;
	dst_ops->get_slave_leader_subdev = src_ops->get_slave_leader_subdev;
}

static int pablo_votfmgr_kunit_test_init(struct kunit *test)
{
	void *addr;

	memset(&test_ctx, 0x0, sizeof(struct pablo_votfmgr_test_ctx));

	test_ctx.group.id = KUNIT_VOTF_GROUP_ID;
	test_ctx.group.device_type = IS_DEVICE_ISCHAIN;
	test_ctx.device.group[GROUP_SLOT_RGBP] = &test_ctx.group_tws;
	test_ctx.device.group[GROUP_SLOT_RGBP]->id = GROUP_ID_RGBP;
	test_ctx.device.group[GROUP_SLOT_RGBP]->device_type = IS_DEVICE_ISCHAIN;
	test_ctx.group.device = &test_ctx.device;
	test_ctx.group.prev = &test_ctx.group;
	test_ctx.group.junction = &test_ctx.junction;

	test_ctx.src_info.service = TWS;
	test_ctx.src_info.ip = KUNIT_VOTF_SRC_IP;
	test_ctx.src_info.id = 0;
	test_ctx.src_info.mode = VOTF_NORMAL;
	test_ctx.dst_info.service = TRS;
	test_ctx.dst_info.ip = KUNIT_VOTF_DST_IP;
	test_ctx.dst_info.id = 0;
	test_ctx.dst_info.mode = VOTF_NORMAL;

	test_ctx.src_votf = get_votf_dev_by_ip_kunit_wrapper(&test_ctx.src_info);
	test_ctx.src_ip = search_ip_idx_kunit_wrapper(&test_ctx.src_info);
	addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr);
	test_ctx.src_addr = test_ctx.src_votf->votf_addr[test_ctx.src_ip];
	test_ctx.src_votf->votf_addr[test_ctx.src_ip] = addr;

	test_ctx.dst_votf = get_votf_dev_by_ip_kunit_wrapper(&test_ctx.dst_info);
	test_ctx.dst_ip = search_ip_idx_kunit_wrapper(&test_ctx.dst_info);
	addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr);
	test_ctx.dst_addr = test_ctx.dst_votf->votf_addr[test_ctx.dst_ip];
	test_ctx.dst_votf->votf_addr[test_ctx.dst_ip] = addr;

	pablo_votfmgr_set_ops(&pablo_normal_votfitf_ops, pablo_get_votfitf());
	pablo_votfmgr_set_ops(pablo_get_votfitf(), &pablo_test_votfitf_ops);

	return 0;
}

static void pablo_votfmgr_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.src_votf->votf_addr[test_ctx.src_ip]);
	test_ctx.src_votf->votf_addr[test_ctx.src_ip] = test_ctx.src_addr;

	kunit_kfree(test, test_ctx.dst_votf->votf_addr[test_ctx.dst_ip]);
	test_ctx.dst_votf->votf_addr[test_ctx.dst_ip] = test_ctx.dst_addr;

	pablo_votfmgr_set_ops(pablo_get_votfitf(), &pablo_normal_votfitf_ops);
}

static struct kunit_case pablo_votfmgr_kunit_test_cases[] = {
	KUNIT_CASE(pablo_votfmgr_register_framemgr_kunit_test),
	KUNIT_CASE(pablo_votfmgr_get_framemgr_kunit_test),
	KUNIT_CASE(pablo_votfmgr_get_frame_kunit_test),
	KUNIT_CASE(pablo_votfmgr_create_link_kunit_test),
	KUNIT_CASE(pablo_votfmgr_destroy_link_kunit_test),
	KUNIT_CASE(pablo_votfmgr_check_link_kunit_test),
	KUNIT_CASE(pablo_votfmgr_force_flush_kunit_test),
	KUNIT_CASE(pablo_votfmgr_link_set_service_cfg_kunit_test),
	KUNIT_CASE(pablo_votfmgr_check_wait_con_kunit_test),
	KUNIT_CASE(pablo_votfmgr_check_invalid_state_kunit_test),
	{},
};

struct kunit_suite pablo_votfmgr_kunit_test_suite = {
	.name = "pablo-votfmgr-kunit-test",
	.init = pablo_votfmgr_kunit_test_init,
	.exit = pablo_votfmgr_kunit_test_exit,
	.test_cases = pablo_votfmgr_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_votfmgr_kunit_test_suite);

MODULE_LICENSE("GPL");
