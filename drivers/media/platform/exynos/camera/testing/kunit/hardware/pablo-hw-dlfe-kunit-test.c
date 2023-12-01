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
#include "is-hw-ip.h"
#include "hardware/sfr/pablo-sfr-dlfe-v1_0.h"
#include "pablo-hw-dlfe.h"

#define STREAM_ID(id)	(id % IS_STREAM_COUNT)
#define SET_CR(R, value)				\
	do {						\
		u32 *addr = (u32 *)(test_ctx.base + R);	\
		*addr = value;				\
	} while (0)

static struct pablo_hw_dlfe_kunit_test_ctx {
	void				*base;
	struct is_hw_ip			hw_ip;
	struct is_interface_ischain	itfc;
	struct is_frame			frame;
	struct is_hardware		hardware;
	struct is_param_region		p_region;
} test_ctx;

/* This is dummy function */
static int hw_frame_done(struct is_hw_ip *hw_ip, struct is_frame *frame,
		int wq_id, u32 output_id, enum ShotErrorType done_type, bool get_meta)
{
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);

	return (int)done_type;
}

static const struct is_hardware_ops hw_ops = {
	.frame_done = hw_frame_done,
};

/* Define testcases */
static void pablo_hw_dlfe_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_OPEN, &hw_ip->state));

	KUNIT_ASSERT_PTR_NE(test, hw_ip->priv_info, NULL);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_INIT, &hw_ip->state));

	ret = CALL_HWIP_OPS(hw_ip, close, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, hw_ip->priv_info, NULL);
	KUNIT_EXPECT_FALSE(test, test_bit(HW_OPEN, &hw_ip->state));

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_dlfe_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);
	ulong hw_map = 0L;

	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(HW_RUN, &hw_ip->state));

	set_bit(hw_ip->id, &hw_map);
	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	set_bit(HW_INIT, &hw_ip->state);
	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_RUN, &hw_ip->state));

	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_dlfe_disable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);
	ulong hw_map = 0UL;

	set_bit(HW_RUN, &hw_ip->state);
	set_bit(HW_CONFIG, &hw_ip->state);

	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_TRUE(test, test_bit(HW_CONFIG, &hw_ip->state));

	set_bit(hw_ip->id, &hw_map);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	set_bit(HW_INIT, &hw_ip->state);
	atomic_set(&hw_ip->status.Vvalid, V_VALID);
	set_bit(instance, &hw_ip->run_rsc_state);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_TRUE(test, test_bit(HW_CONFIG, &hw_ip->state));

	hw_ip->run_rsc_state = 0UL;
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_FALSE(test, test_bit(HW_CONFIG, &hw_ip->state));
}

static void pablo_hw_dlfe_shot_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_frame *frame = &test_ctx.frame;
	struct is_param_region *p_region = &test_ctx.p_region;
	struct pablo_hw_dlfe *hw;
	struct pablo_dlfe_config *cfg;
	struct dlfe_param_set *p_set;
	u32 instance = STREAM_ID(__LINE__);
	ulong hw_map = 0;

	/* TC#1. There is no HW id in hw_map. */
	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC#2. HW is not initialized. */
	set_bit(hw_ip->id, &hw_map);
	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_ASSERT_EQ(test, ret, 0);

	frame->instance = instance;
	hw = (struct pablo_hw_dlfe *)hw_ip->priv_info;
	cfg = &hw->config[instance];
	p_set = &hw->param_set[instance];
	frame->parameter = p_region;

	/* TC#3. Run DLFE without Valid MCFP parameter. */
	cfg->bypass = 0;
	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, p_set->curr_in.cmd);
	KUNIT_EXPECT_FALSE(test, p_set->prev_in.cmd);
	KUNIT_EXPECT_FALSE(test, p_set->wgt_out.cmd);

	/* TC#4. Run DLFE with zero MCFP input size. */
	cfg->bypass = 0;
	p_region->mcfp.otf_input.width = 0;
	p_region->mcfp.otf_input.height = 0;
	set_bit(PARAM_MCFP_OTF_INPUT, frame->pmap);

	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, p_set->curr_in.cmd);
	KUNIT_EXPECT_FALSE(test, p_set->prev_in.cmd);
	KUNIT_EXPECT_FALSE(test, p_set->wgt_out.cmd);

	/* TC#5. Run DLFE with valid MCFP input size. */
	cfg->bypass = 0;
	p_region->mcfp.otf_input.width = __LINE__;
	p_region->mcfp.otf_input.height = __LINE__;

	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, p_set->curr_in.cmd);
	KUNIT_EXPECT_TRUE(test, p_set->prev_in.cmd);
	KUNIT_EXPECT_TRUE(test, p_set->wgt_out.cmd);
	KUNIT_EXPECT_EQ(test, p_set->curr_in.width, p_region->mcfp.otf_input.width);
	KUNIT_EXPECT_EQ(test, p_set->curr_in.height, p_region->mcfp.otf_input.height);

	/* TC#6. Bypass DLFE. */
	cfg->bypass = 1;
	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, p_set->curr_in.cmd);
	KUNIT_EXPECT_FALSE(test, p_set->prev_in.cmd);
	KUNIT_EXPECT_FALSE(test, p_set->wgt_out.cmd);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_hw_dlfe_frame_ndone_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_frame *frame = &test_ctx.frame;
	enum ShotErrorType type = __LINE__ % SHOT_ERR_PERFRAME;

	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &frame->core_flag);
	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, type);
}

static void pablo_hw_dlfe_notify_timeout_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_ASSERT_EQ(test, ret, 0);

	atomic_set(&hw_ip->status.Vvalid, V_VALID);
	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_hw_dlfe_reset_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	/* It always fail because it's not cleared by actual HW. */
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_hw_dlfe_restore_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);

	ret = CALL_HWIP_OPS(hw_ip, restore, instance);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, restore, instance);
	/* It always fail because the internal reset be always failed. */
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static const int reg_dump_type_result[] = {
	[IS_REG_DUMP_TO_ARRAY] = 0,
	[IS_REG_DUMP_TO_LOG] = 0,
	[IS_REG_DUMP_DMA] = 0,
};

static void pablo_hw_dlfe_dump_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);
	u32 fcount = __LINE__;
	u32 dump_type;

	for (dump_type = 0; dump_type < ARRAY_SIZE(reg_dump_type_result); dump_type++) {
		ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, dump_type);
		KUNIT_EXPECT_EQ(test, ret, reg_dump_type_result[dump_type]);
	}
}

static void pablo_hw_dlfe_set_config_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct pablo_hw_dlfe *hw;
	struct pablo_dlfe_config *org_cfg, new_cfg;
	u32 instance = STREAM_ID(__LINE__);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	hw = (struct pablo_hw_dlfe *)hw_ip->priv_info;
	org_cfg = &hw->config[instance];
	memset(&new_cfg, 0, sizeof(new_cfg));

	new_cfg.bypass = 1;
	new_cfg.magic = __LINE__;

	CALL_HWIP_OPS(hw_ip, set_config, 0, instance, 0, (void *)&new_cfg);
	KUNIT_EXPECT_EQ(test, memcmp(org_cfg, &new_cfg, sizeof(new_cfg)), 0);
}

static void pablo_hw_dlfe_handler_int0_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_interface_ischain *itfc = &test_ctx.itfc;
	u32 int_id = INTR_HWIP1, instance = STREAM_ID(__LINE__), val;
	hwip_handler handler;

	handler = itfc->itf_ip[0].handler[int_id].handler;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	/* TC#1. HW is not in OPEN state. */
	ret = handler(int_id, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC#2. HW is not in RUN state. */
	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = handler(int_id, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC#2. Empty interrupt. */
	set_bit(HW_RUN, &hw_ip->state);

	ret = handler(int_id, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC#3. FS/FE overlapping. */
	val = BIT_MASK(INTR0_DLFE_FRAME_START_INT) | BIT_MASK(INTR0_DLFE_FRAME_END_INT);
	SET_CR(DLFE_R_INT_REQ_INT0, val);

	ret = handler(int_id, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&hw_ip->status.Vvalid), V_BLANK);

	/* TC#4. FS interrupt. */
	val = BIT_MASK(INTR0_DLFE_FRAME_START_INT);
	SET_CR(DLFE_R_INT_REQ_INT0, val);

	ret = handler(int_id, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&hw_ip->status.Vvalid), V_VALID);

	/* TC#5. FE interrupt. */
	val = BIT_MASK(INTR0_DLFE_FRAME_END_INT);
	SET_CR(DLFE_R_INT_REQ_INT0, val);

	ret = handler(int_id, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&hw_ip->status.Vvalid), V_BLANK);

	/* TC#6. ERR interrupt. */
	val = BIT_MASK(INTR0_DLFE_CINFIFO_0_OVERFLOW_ERROR_INT);
	SET_CR(DLFE_R_INT_REQ_INT0, val);

	ret = handler(int_id, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_dlfe_handler_int1_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_interface_ischain *itfc = &test_ctx.itfc;
	u32 int_id = INTR_HWIP2;
	hwip_handler handler;

	handler = itfc->itf_ip[0].handler[int_id].handler;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, handler);

	ret = CALL_HWIP_OPS(hw_ip, open, atomic_read(&hw_ip->instance));
	KUNIT_ASSERT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip->state);

	/* TC#1. Empty interrupt. */
	ret = handler(int_id, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC#2. ERR interrupt. */
	SET_CR(DLFE_R_INT_REQ_INT1, BIT_MASK(INTR1_DLFE_VOTF_LOST_FLUSH_INT));

	ret = handler(int_id, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static struct kunit_case pablo_hw_dlfe_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_dlfe_open_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_enable_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_disable_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_shot_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_frame_ndone_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_notify_timeout_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_reset_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_restore_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_dump_regs_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_set_config_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_handler_int0_kunit_test),
	KUNIT_CASE(pablo_hw_dlfe_handler_int1_kunit_test),
	{},
};

static void setup_hw_ip(struct is_hw_ip *hw_ip)
{
	hw_ip->id = DEV_HW_DLFE;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	init_waitqueue_head(&hw_ip->status.wait_queue);
	hw_ip->hw_ops = &hw_ops;
	hw_ip->regs[REG_SETA] = test_ctx.base;
	hw_ip->hardware = &test_ctx.hardware;
}

static int pablo_hw_dlfe_kunit_test_init(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_interface *itf = NULL;
	struct is_interface_ischain *itfc = &test_ctx.itfc;

	memset(&test_ctx, 0, sizeof(test_ctx));
	test_ctx.base = kunit_kzalloc(test, 0x8000, 0);

	setup_hw_ip(hw_ip);

	ret = pablo_hw_dlfe_probe(hw_ip, itf, itfc, DEV_HW_DLFE, "DLFE");
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_dlfe_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.base);
}

struct kunit_suite pablo_hw_dlfe_kunit_test_suite = {
	.name = "pablo-hw-dlfe-kunit-test",
	.init = pablo_hw_dlfe_kunit_test_init,
	.exit = pablo_hw_dlfe_kunit_test_exit,
	.test_cases = pablo_hw_dlfe_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_dlfe_kunit_test_suite);

MODULE_LICENSE("GPL");
