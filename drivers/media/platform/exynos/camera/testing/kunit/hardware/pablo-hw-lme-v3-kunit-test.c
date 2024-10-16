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
	struct is_hw_ip *hw_ip;
	u32 instance;
	struct is_core *core;
	struct is_framemgr framemgr;
	struct is_hardware *hardware;
	int hw_slot;
	struct is_region is_region;
	struct is_frame frame;
	struct camera2_shot shot;
	struct is_param_region parameter;
	void *addr;
} test_ctx;

/* Define the test cases. */

static void pablo_hw_lme_open_kunit_test(struct kunit *test)
{
	struct pablo_hw_lme_kunit_test_ctx *tctx = test->priv;
	int ret;
	struct is_hw_lme *hw_lme = NULL;

	/* [Abnormal] already open case */
	set_bit(HW_OPEN, &tctx->hw_ip->state);

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* [Normal] open case */
	clear_bit(HW_OPEN, &tctx->hw_ip->state);

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_lme = (struct is_hw_lme *)tctx->hw_ip->priv_info;
	KUNIT_EXPECT_EQ(test, hw_lme->instance, tctx->instance);

	KUNIT_EXPECT_EQ(test, atomic_read(&tctx->hw_ip->status.Vvalid), V_BLANK);

	KUNIT_EXPECT_TRUE(test, test_bit(HW_OPEN, &tctx->hw_ip->state));

	/* [Abnormal] already close case */
	clear_bit(HW_OPEN, &tctx->hw_ip->state);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* [Normal] close case */
	set_bit(HW_OPEN, &tctx->hw_ip->state);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_PTR_EQ(test, tctx->hw_ip->priv_info, NULL);
}

static void pablo_hw_lme_init_kunit_test(struct kunit *test)
{
	struct pablo_hw_lme_kunit_test_ctx *tctx = test->priv;
	int ret;

	/* [Normal] init case */
	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_TRUE(test, test_bit(HW_INIT, &tctx->hw_ip->state));

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_enable_kunit_test(struct kunit *test)
{
	struct pablo_hw_lme_kunit_test_ctx *tctx = test->priv;
	int ret;

	/* [Abnormal] hw_ip->id bit clear in hw_map case */
	clear_bit(tctx->hw_ip->id, &tctx->hardware->hw_map[0]);

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_FALSE(test, test_bit(HW_RUN, &tctx->hw_ip->state));

	set_bit(tctx->hw_ip->id, &tctx->hardware->hw_map[0]);

	/* [Abnormal] already init case */
	set_bit(HW_RUN, &tctx->hw_ip->state);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* [Normal] enable case */
	clear_bit(HW_RUN, &tctx->hw_ip->state);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, test_bit(HW_RUN, &tctx->hw_ip->state), 1);

	ret = tctx->hw_ip->ops->disable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_set_param_kunit_test(struct kunit *test)
{
	struct pablo_hw_lme_kunit_test_ctx *tctx = test->priv;
	IS_DECLARE_PMAP(pmap);
	int ret;
	struct is_hw_lme *hw_lme;

	/* [ERR] call set_param without init case */
	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	clear_bit(HW_INIT, &tctx->hw_ip->state);

	IS_INIT_PMAP(pmap);
	ret = tctx->hw_ip->ops->set_param(tctx->hw_ip, &tctx->is_region,
		pmap, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* [Normal] set_param case */
	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	IS_INIT_PMAP(pmap);
	ret = tctx->hw_ip->ops->set_param(tctx->hw_ip, &tctx->is_region,
		pmap, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_lme = (struct is_hw_lme *)tctx->hw_ip->priv_info;
	KUNIT_EXPECT_EQ(test, hw_lme->instance, (u32)IS_STREAM_COUNT);

	ret = tctx->hw_ip->ops->disable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_lme_shot_kunit_test(struct kunit *test)
{
	struct pablo_hw_lme_kunit_test_ctx *tctx = test->priv;
	IS_DECLARE_PMAP(pmap);
	int ret;
	struct size_cr_set *scs = (struct size_cr_set *)tctx->frame.kva_lme_rta_info[PLANE_INDEX_CR_SET];
	struct is_lme_config *config;
	config = (struct is_lme_config *)(tctx->frame.kva_lme_rta_info[PLANE_INDEX_CONFIG] + sizeof(u32));

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	IS_INIT_PMAP(pmap);
	ret = tctx->hw_ip->ops->set_param(tctx->hw_ip, &tctx->is_region,
		pmap, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* [Abnormal] hw_ip->id bit clear in hw_map case */
	clear_bit(tctx->hw_ip->id, &tctx->hardware->hw_map[0]);

	ret = tctx->hw_ip->ops->shot(tctx->hw_ip, &tctx->frame, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(tctx->hw_ip->id, &tctx->hardware->hw_map[0]);

	/* [ERR] call shot without init case */
	clear_bit(HW_INIT, &tctx->hw_ip->state);

	ret = tctx->hw_ip->ops->shot(tctx->hw_ip, &tctx->frame, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	set_bit(HW_INIT, &tctx->hw_ip->state);

	/* [ERR] failed to get hardware case */
	tctx->hw_ip->hardware = 0;

	ret = tctx->hw_ip->ops->shot(tctx->hw_ip, &tctx->frame, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	tctx->hw_ip->hardware = tctx->hardware;

	/* [ERR] set size fail case */
	config->lme_in_w = 0;
	config->lme_in_h = 0;

	ret = tctx->hw_ip->ops->shot(tctx->hw_ip, &tctx->frame, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* [Normal] shot case */
	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	IS_INIT_PMAP(pmap);
	ret = tctx->hw_ip->ops->set_param(tctx->hw_ip, &tctx->is_region,
		pmap, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	config->lme_in_w = 1008;
	config->lme_in_h = 756;
	scs->size = 1;
	scs->cr->reg_addr = 0;
	scs->cr->reg_data = 0;

	ret = tctx->hw_ip->ops->shot(tctx->hw_ip, &tctx->frame, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, tctx->hw_ip->num_buffers, tctx->frame.num_buffers);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_CONFIG, &tctx->hw_ip->state));

	ret = tctx->hw_ip->ops->disable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int pablo_hw_lme_kunit_test_init(struct kunit *test)
{
	void *addr;

	test_ctx.core = is_get_is_core();
	test_ctx.hardware = &test_ctx.core->hardware;
	test_ctx.hw_slot = CALL_HW_CHAIN_INFO_OPS(test_ctx.hardware, get_hw_slot_id, DEV_HW_LME0);
	test_ctx.hw_ip = &test_ctx.hardware->hw_ip[test_ctx.hw_slot];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.hw_ip);
	test_ctx.hw_ip->framemgr = &test_ctx.framemgr;
	test_ctx.hw_ip->hardware = test_ctx.hardware;
	test_ctx.frame.shot = &test_ctx.shot;
	test_ctx.frame.parameter = &test_ctx.parameter;
	test_ctx.frame.kva_lme_rta_info[PLANE_INDEX_CR_SET] = (u64)kunit_kzalloc(test, sizeof(struct size_cr_set), 0);
	test_ctx.frame.kva_lme_rta_info[PLANE_INDEX_CONFIG] = (u64)kunit_kzalloc(test, sizeof(struct is_hw_lme) + sizeof(u32), 0);

	addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr);

	test_ctx.addr = test_ctx.hw_ip->regs[REG_SETA];
	test_ctx.hw_ip->regs[REG_SETA] = addr;
	test->priv = (struct pablo_hw_lme_kunit_test_ctx *)&test_ctx;

	return 0;
}

static void pablo_hw_lme_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_lme_kunit_test_ctx *test_ctx = test->priv;

	kunit_kfree(test, test_ctx->hw_ip->regs[REG_SETA]);
	kunit_kfree(test, (void *)test_ctx->frame.kva_lme_rta_info[PLANE_INDEX_CONFIG]);
	kunit_kfree(test, (void *)test_ctx->frame.kva_lme_rta_info[PLANE_INDEX_CR_SET]);
	test_ctx->hw_ip->regs[REG_SETA] = test_ctx->addr;
	test_ctx->hw_ip->state = 0;
}

static struct kunit_case pablo_hw_lme_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_lme_open_kunit_test),
	KUNIT_CASE(pablo_hw_lme_init_kunit_test),
	KUNIT_CASE(pablo_hw_lme_enable_kunit_test),
	KUNIT_CASE(pablo_hw_lme_set_param_kunit_test),
	KUNIT_CASE(pablo_hw_lme_shot_kunit_test),
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
