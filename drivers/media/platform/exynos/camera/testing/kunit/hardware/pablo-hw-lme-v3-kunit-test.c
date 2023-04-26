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

#include "is-core.h"
#include "is-hw.h"
#include "is-config.h"
#include "hardware/include/is-hw-lme-v3.h"
#include "hardware/api/is-hw-api-lme-v3_0.h"

#define PLANE_INDEX_CR_SET 0
#define PLANE_INDEX_CONFIG 1
static struct pablo_hw_lme_kunit_test_ctx {
	struct is_hw_ip		hw_ip;
	struct is_hardware	hardware;
	struct is_interface_ischain	itfc;
	struct is_framemgr	framemgr;
	struct is_frame		frame;
	struct camera2_shot_ext	shot_ext;
	struct camera2_shot	shot;
	struct is_param_region	parameter;
	struct is_mem		mem;
	struct is_mem_ops	memops;
	struct lme_param	lme_param;
	struct is_region	region;
	void			*test_addr;
	struct is_hw_ip_ops	hw_ops;
	struct is_hw_ip_ops     *org_hw_ops;

	struct is_core *core;

} test_ctx;

/* Define the test cases. */

static void pablo_hw_lme_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	ulong hw_map = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &hw_map);

	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_set_param_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	ulong hw_map = 0;
	IS_DECLARE_PMAP(pmap);

	set_bit(hw_ip->id, &hw_map);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, set_param, &test_ctx.region, pmap, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_frame_ndone_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	enum ShotErrorType type = IS_SHOT_UNKNOWN;

	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_reset_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;

	hw_ip->ops = test_ctx.org_hw_ops;

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);
}

static bool __set_rta_regs(struct is_hw_ip *hw_ip, u32 instance, u32 set_id, bool skip,
		struct is_frame *frame, void *buf)
{
	return false;
}

static int __reset_stub(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static int __wait_idle_stub(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static void pablo_hw_lme_shot_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct pablo_hw_helper_ops ops;
	u32 instance = 0;
	ulong hw_map = 0;
	struct size_cr_set *scs = (struct size_cr_set *)test_ctx.frame.kva_lme_rta_info[PLANE_INDEX_CR_SET];
	struct is_lme_config *config;
	struct is_hw_lme *hw_lme;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &hw_map);

	ret = CALL_HWIP_OPS(hw_ip, set_param, &test_ctx.region, test_ctx.frame.pmap, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	test_ctx.frame.shot = &test_ctx.shot;
	test_ctx.frame.shot_ext = &test_ctx.shot_ext;
	test_ctx.frame.parameter = &test_ctx.parameter;
	ops.set_rta_regs = __set_rta_regs;
	hw_ip->help_ops = &ops;
	hw_ip->region[instance] = &test_ctx.region;

	hw_lme = (struct is_hw_lme *)hw_ip->priv_info;
	config = &hw_lme->config[instance];

	hw_ip->hardware = &test_ctx.hardware;

	/* [ERR] call shot without init case */
	clear_bit(HW_INIT, &hw_ip->state);

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	set_bit(HW_INIT, &hw_ip->state);

	/* [ERR] failed to get test_ctx.hardware case */
	hw_ip->hardware = 0;

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	hw_ip->hardware = &test_ctx.hardware;

	set_bit(PARAM_LME_CONTROL, test_ctx.frame.pmap);
	set_bit(PARAM_LME_DMA, test_ctx.frame.pmap);

	/* [ERR] set size fail case */
	config->lme_in_w = 0;
	config->lme_in_h = 0;

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	config->lme_in_w = 1008;
	config->lme_in_h = 756;
	scs->size = 1;
	scs->cr->reg_addr = 0;
	scs->cr->reg_data = 0;

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, hw_ip->num_buffers, test_ctx.frame.num_buffers);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_CONFIG, &hw_ip->state));

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_set_config_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 chain_id = 0;
	u32 instance = 0;
	u32 fcount = 0;
	struct is_lme_config conf;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, set_config, chain_id, instance, fcount, &conf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_dump_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	u32 fcount = 0;

	ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, IS_REG_DUMP_TO_LOG);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, IS_REG_DUMP_DMA);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_set_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 chain_id = 0;
	u32 instance = 0;
	u32 fcount = 0;

	ret = CALL_HWIP_OPS(hw_ip, set_regs, chain_id, instance, fcount, NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_notify_timeout_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;

	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_restore_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, restore, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_setfile_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 scenario = 0;
	u32 instance = 0;
	ulong hw_map = 0;

	ret = CALL_HWIP_OPS(hw_ip, load_setfile, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, apply_setfile, scenario, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, delete_setfile, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_get_meta_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	ulong hw_map = 0;

	ret = CALL_HWIP_OPS(hw_ip, get_meta, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void __setup_hw_ip(struct kunit *test)
{
	int ret;
	enum is_hardware_id hw_id = DEV_HW_LME0;
	struct is_interface *itf = NULL;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_interface_ischain *itfc = &test_ctx.itfc;

	hw_ip->hardware = &test_ctx.hardware;

	ret = is_hw_lme_probe(hw_ip, itf, itfc, hw_id, "LME");
	KUNIT_ASSERT_EQ(test, ret, 0);

	hw_ip->id = hw_id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "LME");
	hw_ip->itf = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);
	hw_ip->state = 0;

	hw_ip->framemgr = &test_ctx.framemgr;

	test_ctx.org_hw_ops = (struct is_hw_ip_ops *)hw_ip->ops;
	test_ctx.hw_ops = *(hw_ip->ops);
	test_ctx.hw_ops.reset = __reset_stub;
	test_ctx.hw_ops.wait_idle = __wait_idle_stub;
	hw_ip->ops = &test_ctx.hw_ops;
}

static int pablo_hw_lme_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.core = is_get_is_core();
	test_ctx.hardware = test_ctx.core->hardware;
	test_ctx.hw_ip.framemgr = &test_ctx.framemgr;
	test_ctx.hw_ip.hardware = &test_ctx.hardware;

	test_ctx.frame.kva_lme_rta_info[PLANE_INDEX_CR_SET] = (u64)kunit_kzalloc(test, sizeof(struct size_cr_set), 0);
	test_ctx.frame.kva_lme_rta_info[PLANE_INDEX_CONFIG] = (u64)kunit_kzalloc(test, sizeof(struct is_hw_lme) + sizeof(u32), 0);

	test_ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	test_ctx.hw_ip.regs[REG_SETA] = test_ctx.test_addr;

	ret = is_mem_init(&test_ctx.mem, is_get_is_core()->pdev);
	KUNIT_ASSERT_EQ(test, ret, 0);

	__setup_hw_ip(test);

	return 0;
}

static void pablo_hw_lme_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.test_addr);
	kunit_kfree(test, (void *)test_ctx.frame.kva_lme_rta_info[PLANE_INDEX_CONFIG]);
	kunit_kfree(test, (void *)test_ctx.frame.kva_lme_rta_info[PLANE_INDEX_CR_SET]);

	memset(&test_ctx, 0, sizeof(test_ctx));
}

static struct kunit_case pablo_hw_lme_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_lme_open_kunit_test),
	KUNIT_CASE(pablo_hw_lme_shot_kunit_test),
	KUNIT_CASE(pablo_hw_lme_enable_kunit_test),
	KUNIT_CASE(pablo_hw_lme_set_config_kunit_test),
	KUNIT_CASE(pablo_hw_lme_dump_regs_kunit_test),
	KUNIT_CASE(pablo_hw_lme_set_regs_kunit_test),
	KUNIT_CASE(pablo_hw_lme_notify_timeout_kunit_test),
	KUNIT_CASE(pablo_hw_lme_restore_kunit_test),
	KUNIT_CASE(pablo_hw_lme_setfile_kunit_test),
	KUNIT_CASE(pablo_hw_lme_get_meta_kunit_test),
	KUNIT_CASE(pablo_hw_lme_set_param_kunit_test),
	KUNIT_CASE(pablo_hw_lme_frame_ndone_kunit_test),
	KUNIT_CASE(pablo_hw_lme_reset_kunit_test),
	{},
};

struct kunit_suite pablo_hw_lme_kunit_test_suite = {
	.name = "pablo-hw-lme-v3-kunit-test",
	.init = pablo_hw_lme_kunit_test_init,
	.exit = pablo_hw_lme_kunit_test_exit,
	.test_cases = pablo_hw_lme_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_lme_kunit_test_suite);

MODULE_LICENSE("GPL");
