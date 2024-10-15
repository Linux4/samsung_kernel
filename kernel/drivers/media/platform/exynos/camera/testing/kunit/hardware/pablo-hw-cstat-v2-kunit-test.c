// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "is-hw-ip.h"
#include "is-core.h"
#include "is-hw-cstat.h"
#include "pablo-icpu-adapter.h"

static struct pablo_hw_cstat_kunit_test_ctx {
	struct is_hw_ip hw_ip;
	struct is_hardware hardware;
	struct is_interface_ischain itfc;
	struct is_framemgr framemgr;
	struct is_frame frame;
	struct camera2_shot_ext shot_ext;
	struct camera2_shot shot;
	struct is_param_region parameter;
	struct is_mem mem;
	struct is_mem_ops memops;
	struct cstat_param cstat_param;
	struct is_region region;
	void *test_addr;
	void *test_addr_ext;
	struct is_hw_ip_ops hw_ops;
	struct is_hw_ip_ops *org_hw_ops;

	struct pablo_icpu_adt_msg_ops icpu_msg_ops;
	const struct pablo_icpu_adt_msg_ops *org_icpu_msg_ops;
} test_ctx;

static int __reset_stub(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static int __register_response_msg_cb_stub(struct pablo_icpu_adt *icpu_adt, u32 instance,
					   enum pablo_hic_cmd_id msg, pablo_response_msg_cb cb)
{
	return 0;
}

/* Define the test cases. */

static void pablo_hw_cstat_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	struct is_hw_cstat *hw;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw = (struct is_hw_cstat *)hw_ip->priv_info;

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_cstat_handle_interrupt_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	struct is_interface_ischain *itfc = &test_ctx.itfc;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itfc->itf_ip[4].handler[INTR_HWIP1].handler);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* overflow recorvery */
	set_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag);
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	clear_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag);

	/* not run */
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip->state);

	/* not config */
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(HW_CONFIG, &hw_ip->state);

	/* start -> config lock -> end */
	is_cstat_hw_s_int1_status(hw_ip, CSTAT_FS);
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	is_cstat_hw_c_int1(hw_ip);

	is_cstat_hw_s_int1_status(hw_ip, CSTAT_LINE);
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	is_cstat_hw_c_int1(hw_ip);

	is_cstat_hw_s_int1_status(hw_ip, CSTAT_COREX_END);
	is_cstat_hw_s_int1_status(hw_ip, CSTAT_FE);
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	is_cstat_hw_c_int1(hw_ip);

	/* start -> config lock -> end (config lock delay) */
	is_cstat_hw_s_int1_status(hw_ip, CSTAT_FS);
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	is_cstat_hw_c_int1(hw_ip);

	is_cstat_hw_s_int1_status(hw_ip, CSTAT_LINE);
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	is_cstat_hw_c_int1(hw_ip);

	is_cstat_hw_s_int1_status(hw_ip, CSTAT_FE);
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	is_cstat_hw_c_int1(hw_ip);

	/* start -> config lock -> end (isr overlap) */
	is_cstat_hw_s_int1_status(hw_ip, CSTAT_FS);
	is_cstat_hw_s_int1_status(hw_ip, CSTAT_LINE);
	is_cstat_hw_s_int1_status(hw_ip, CSTAT_FE);
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	is_cstat_hw_c_int1(hw_ip);

	/* cdaf */
	is_cstat_hw_s_int2_status(hw_ip, CSTAT_CDAF);
	ret = itfc->itf_ip[4].handler[INTR_HWIP2].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	is_cstat_hw_c_int2(hw_ip);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_cstat_shot_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	ulong hw_map = 0;
	struct is_param_region *param_region;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &hw_map);
	hw_ip->region[instance] = &test_ctx.region;

	set_bit(instance, &hw_ip->run_rsc_state);
	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	test_ctx.frame.shot = &test_ctx.shot;
	test_ctx.frame.shot_ext = &test_ctx.shot_ext;
	test_ctx.frame.parameter = &test_ctx.parameter;

	set_bit(PARAM_CSTAT_OTF_INPUT, test_ctx.frame.pmap);
	set_bit(PARAM_CSTAT_LME_DS0, test_ctx.frame.pmap);
	set_bit(PARAM_CSTAT_LME_DS1, test_ctx.frame.pmap);
	set_bit(PARAM_CSTAT_FDPIG, test_ctx.frame.pmap);
	set_bit(PARAM_CSTAT_DRC, test_ctx.frame.pmap);

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -22);

	param_region = &test_ctx.parameter;
	param_region->cstat[0].otf_input.cmd = 1;
	param_region->cstat[0].otf_input.width = 4032;
	param_region->cstat[0].otf_input.height = 3024;
	param_region->cstat[0].otf_input.offset_x = 0;
	param_region->cstat[0].otf_input.offset_y = 0;
	param_region->cstat[0].otf_input.physical_width = 4032;
	param_region->cstat[0].otf_input.physical_height = 3024;
	param_region->cstat[0].otf_input.format = OTF_INPUT_FORMAT_BAYER;
	param_region->cstat[0].otf_input.order = OTF_INPUT_FORMAT_BAYER;
	param_region->cstat[0].otf_input.bayer_crop_offset_x = 0;
	param_region->cstat[0].otf_input.bayer_crop_offset_y = 0;
	param_region->cstat[0].otf_input.bayer_crop_width = 4032;
	param_region->cstat[0].otf_input.bayer_crop_height = 3024;
	param_region->cstat[0].otf_input.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	clear_bit(instance, &hw_ip->run_rsc_state);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_cstat_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	ulong hw_map = 0;
	struct cstat_param *param;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->region[instance] = &test_ctx.region;
	param = &hw_ip->region[instance]->parameter.cstat[0];
	param->dma_input.cmd = 0;
	param->dma_input.v_otf_enable = 0;
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

static void pablo_hw_cstat_set_config_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 chain_id = 0;
	u32 instance = 0;
	u32 fcount = 0;
	struct is_cstat_config conf;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, set_config, chain_id, instance, fcount, &conf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_cstat_dump_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	u32 fcount = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, IS_REG_DUMP_TO_LOG);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, IS_REG_DUMP_DMA);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, IS_REG_DUMP_TO_ARRAY);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_cstat_set_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 chain_id = 0;
	u32 instance = 0;
	u32 fcount = 0;
	struct cr_set cstat_cr;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, set_regs, chain_id, instance, fcount, &cstat_cr, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_cstat_notify_timeout_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_cstat_restore_kunit_test(struct kunit *test)
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

static void pablo_hw_cstat_setfile_kunit_test(struct kunit *test)
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

static void pablo_hw_cstat_get_meta_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	ulong hw_map = 0;

	ret = CALL_HWIP_OPS(hw_ip, get_meta, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_cstat_set_param_kunit_test(struct kunit *test)
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

static void pablo_hw_cstat_frame_ndone_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	enum ShotErrorType type = IS_SHOT_UNKNOWN;

	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static struct kunit_case pablo_hw_cstat_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_cstat_open_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_handle_interrupt_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_shot_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_enable_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_set_config_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_dump_regs_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_set_regs_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_notify_timeout_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_restore_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_setfile_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_get_meta_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_set_param_kunit_test),
	KUNIT_CASE(pablo_hw_cstat_frame_ndone_kunit_test),
	{},
};

static void __setup_hw_ip(struct kunit *test)
{
	int ret;
	enum is_hardware_id hw_id = DEV_HW_3AA0;
	struct is_interface *itf = NULL;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_interface_ischain *itfc = &test_ctx.itfc;

	hw_ip->hardware = &test_ctx.hardware;

	ret = is_hw_cstat_probe(hw_ip, itf, itfc, hw_id, "CSTAT");
	KUNIT_ASSERT_EQ(test, ret, 0);

	hw_ip->id = hw_id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "CSTAT");
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
	hw_ip->ops = &test_ctx.hw_ops;
}

static int pablo_hw_cstat_kunit_test_init(struct kunit *test)
{
	int ret;
	struct pablo_icpu_adt *icpu_adt;

	test_ctx.hardware = is_get_is_core()->hardware;

	test_ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);
	test_ctx.test_addr_ext = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr_ext);

	test_ctx.hw_ip.regs[REG_SETA] = test_ctx.test_addr;
	test_ctx.hw_ip.regs[REG_EXT1] = test_ctx.test_addr_ext;

	icpu_adt = pablo_get_icpu_adt();
	test_ctx.org_icpu_msg_ops = icpu_adt->msg_ops;
	test_ctx.icpu_msg_ops = *(icpu_adt->msg_ops);
	test_ctx.icpu_msg_ops.register_response_msg_cb = __register_response_msg_cb_stub;
	icpu_adt->msg_ops = &test_ctx.icpu_msg_ops;

	ret = is_mem_init(&test_ctx.mem, is_get_is_core()->pdev);
	KUNIT_ASSERT_EQ(test, ret, 0);

	__setup_hw_ip(test);

	return 0;
}

static void pablo_hw_cstat_kunit_test_exit(struct kunit *test)
{
	struct pablo_icpu_adt *icpu_adt;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;

	hw_ip->ops = test_ctx.org_hw_ops;

	icpu_adt = pablo_get_icpu_adt();
	icpu_adt->msg_ops = test_ctx.org_icpu_msg_ops;

	kunit_kfree(test, test_ctx.test_addr);
	kunit_kfree(test, test_ctx.test_addr_ext);
	memset(&test_ctx, 0, sizeof(test_ctx));
}

struct kunit_suite pablo_hw_cstat_kunit_test_suite = {
	.name = "pablo-hw-cstat-v2-kunit-test",
	.init = pablo_hw_cstat_kunit_test_init,
	.exit = pablo_hw_cstat_kunit_test_exit,
	.test_cases = pablo_hw_cstat_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_cstat_kunit_test_suite);

MODULE_LICENSE("GPL");
