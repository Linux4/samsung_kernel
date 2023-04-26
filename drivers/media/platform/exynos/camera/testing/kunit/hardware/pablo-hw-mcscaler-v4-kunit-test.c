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
#include "is-hw-mcscaler-v4.h"
#include "api/is-hw-api-mcscaler-v4.h"
#include "is-hw-control.h"

extern u32 is_hardware_dma_cfg(char *name, struct is_hw_ip *hw_ip,
			struct is_frame *frame, int cur_idx, u32 num_buffers,
			u32 *cmd, u32 plane,
			pdma_addr_t *dst_dva, dma_addr_t *src_dva);

struct pablo_hw_mcsc_kunit_test_ctx {
	int ret;
	struct is_hardware	hardware;
	struct is_interface_ischain	itfc;
	struct is_framemgr	framemgr;
	struct is_frame		frame;
	struct camera2_shot_ext	shot_ext;
	struct is_param_region	parameter;
	struct is_mem		mem;
	struct is_region	region;
	void			*test_addr;
	struct is_hardware_ops hw_ops;
	struct hw_mcsc_setfile mcsc_setfile;
	struct is_hw_ip_ops	ip_ops;
	struct is_hw_ip_ops     *org_ip_ops;
} test_ctx;

static struct param_mcs_input test_input = {
	.width = 320,
	.height = 240,
	.plane = DMA_INOUT_PLANE_4,
	.dma_cmd = DMA_INPUT_COMMAND_ENABLE,
	.dma_crop_width = 320,
	.dma_crop_height = 240,
	.dma_format = OTF_INPUT_FORMAT_YUV422,
	.dma_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	.otf_cmd = OTF_INPUT_COMMAND_ENABLE,
	.otf_format = OTF_INPUT_FORMAT_YUV422,
	.otf_bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
	.stripe_in_start_pos_x = 0,
	.stripe_in_end_pos_x = 320,
	.stripe_roi_start_pos_x = 0,
	.stripe_roi_end_pos_x = 320,
};

static struct param_mcs_output test_output = {
	.dma_cmd = DMA_OUTPUT_COMMAND_ENABLE,
	.cmd = 1,
	.width = 640,
	.height = 480,
	.dma_format = DMA_INOUT_FORMAT_YUV420,
	.dma_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	.plane = DMA_INOUT_PLANE_4,
	.dma_order = DMA_INOUT_ORDER_CbCr,
	.offset_x = 120,
	.full_input_width = 640,
	.full_output_width = 120,
	.crop_offset_x = 0,
	.crop_offset_y = 0,
	.crop_width = 320,
	.crop_height = 240,
};

/* Define the test cases. */

static void pablo_hw_mcsc_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
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

static void pablo_hw_mcsc_handle_interrupt_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
	u32 instance = 0;
	struct is_interface_ischain *itfc = &test_ctx.itfc;
	int hw_slot;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, DEV_HW_MCSC0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler);

	/* not opened */
	ret = itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* overflow recorvery */
	set_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag);
	ret = itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	clear_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag);

	/* not run */
	ret = itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip->state);
	*(u32 *)(test_ctx.test_addr + 0x0800) = 0xFFFFFFFF;
	*(u32 *)(test_ctx.test_addr + 0x0804) = 0xFFFFFFFF;
	ret = itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static bool __set_rta_regs(struct is_hw_ip *hw_ip, u32 instance, u32 set_id, bool skip,
		struct is_frame *frame, void *buf)
{
	return true;
}

static int __reset_stub(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static void pablo_hw_mcsc_shot_kunit_test(struct kunit *test)
{
	int ret, k;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
	struct pablo_hw_helper_ops ops;
	u32 instance = 0;
	ulong hw_map = 0;
	struct is_hw_mcsc *hw_mcsc;
	u32 scenario = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &hw_map);
	test_ctx.frame.shot = &test_ctx.shot_ext.shot;
	test_ctx.frame.shot_ext = &test_ctx.shot_ext;
	test_ctx.frame.parameter = &test_ctx.parameter;
	ops.set_rta_regs = __set_rta_regs;
	hw_ip->help_ops = &ops;
	test_ctx.parameter.mcs.control.cmd = CONTROL_COMMAND_TEST;
	test_ctx.hardware.hw_ip[0].region[0] = &test_ctx.region;

	test_ctx.parameter.mcs.input = test_input;

	/* basic test */
	hw_mcsc = (struct is_hw_mcsc *)hw_ip->priv_info;
	hw_mcsc->rdma_max_cnt = 1;

	test_ctx.frame.num_buffers = 1;
	test_ctx.frame.dvaddr_buffer[0] = (dma_addr_t)test_ctx.test_addr;
	test_ctx.frame.dvaddr_buffer[1] = (dma_addr_t)test_ctx.test_addr;

	set_bit(0, &hw_mcsc->out_en);
	test_ctx.hw_ops.dma_cfg = is_hardware_dma_cfg;
	hw_ip->hw_ops = &test_ctx.hw_ops;

	hw_mcsc->cur_setfile[0][0] = &hw_mcsc->setfile[0][0];
	hw_mcsc->cur_ni[SUBBLK_HF] = 2;

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* output dma test */
	for (k = 0; k < MCSC_OUTPUT_MAX; k++)
		test_ctx.parameter.mcs.output[k] = test_output;

	test_ctx.frame.sc0TargetAddress[0] = (dma_addr_t)test_ctx.test_addr;
	test_ctx.frame.sc1TargetAddress[0] = (dma_addr_t)test_ctx.test_addr;
	test_ctx.frame.sc2TargetAddress[0] = (dma_addr_t)test_ctx.test_addr;
	test_ctx.frame.sc3TargetAddress[0] = (dma_addr_t)test_ctx.test_addr;
	test_ctx.frame.sc4TargetAddress[0] = (dma_addr_t)test_ctx.test_addr;
	test_ctx.frame.sc5TargetAddress[0] = (dma_addr_t)test_ctx.test_addr;

	test_ctx.parameter.mcs.stripe_input.full_width = 320;

	hw_ip->setfile[0].version = SETFILE_V3;
	hw_ip->setfile[0].index[scenario] = 0;
	hw_ip->setfile[0].using_count = 1;
	hw_ip->setfile[0].table[0].addr = (ulong )&test_ctx.mcsc_setfile;
	hw_ip->setfile[0].table[0].size = sizeof(test_ctx.mcsc_setfile);
	test_ctx.mcsc_setfile.setfile_version = MCSC_SETFILE_VERSION;

	ret = CALL_HWIP_OPS(hw_ip, load_setfile, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, apply_setfile, scenario, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_mcsc->cur_setfile[0][0]->djag.djag_en = 1;
	hw_mcsc->cur_setfile[0][0]->hf.ni_max = HF_MAX_NI_DEPENDED_CFG;

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* input dma test */
	hw_mcsc->tune_set[instance] = 0;
	hw_mcsc->cap.in_dma = MCSC_CAP_SUPPORT;

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, delete_setfile, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_mcsc_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
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

static void pablo_hw_mcsc_dump_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
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

static void pablo_hw_mcsc_notify_timeout_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
	u32 instance = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_mcsc_restore_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
	u32 instance = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, restore, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_mcsc_setfile_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
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

static void pablo_hw_mcsc_set_param_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
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
	KUNIT_EXPECT_PTR_EQ(test, hw_ip->region[instance], &test_ctx.region);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_mcsc_frame_ndone_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
	enum ShotErrorType type = IS_SHOT_UNKNOWN;

	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_mcsc_set_ni_kunit_test(struct kunit *test)
{
	test_ctx.frame.shot = &test_ctx.shot_ext.shot;
	is_hw_mcsc_set_ni(&test_ctx.hardware, &test_ctx.frame, 0);
	KUNIT_EXPECT_EQ(test, test_ctx.hardware.ni_udm[0][0].currentFrameNoiseIndex,
		test_ctx.frame.shot->udm.ni.currentFrameNoiseIndex);
}

struct pablo_hw_mcsc_test_vector {
	int ret;
	u32 bitwidth, format, plane, order, img_format;
};

static struct pablo_hw_mcsc_test_vector adj_input[] = {
	/* 420 FORMAT */
	{0, 0, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_CbCr, MCSC_YUV420_2P_UFIRST},
	{0, 0, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_CrCb, MCSC_YUV420_2P_VFIRST},
	{-EINVAL, 0, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_NO, 0},
	{0, 0, DMA_INOUT_FORMAT_YUV420, 3, DMA_INOUT_ORDER_NO, MCSC_YUV420_3P},
	{-EINVAL, 0, DMA_INOUT_FORMAT_YUV420, 1, DMA_INOUT_ORDER_NO, 0},
	/* 422 FORMAT */
	{0, 0, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_CrYCbY, MCSC_YUV422_1P_VYUY},
	{0, 0, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_CbYCrY, MCSC_YUV422_1P_UYVY},
	{0, 0, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_YCrYCb, MCSC_YUV422_1P_YVYU},
	{0, 0, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_YCbYCr, MCSC_YUV422_1P_YUYV},
	{-EINVAL, 0, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_NO, 0},
	{0, 0, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_CbCr, MCSC_YUV422_2P_UFIRST},
	{0, 0, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_CrCb, MCSC_YUV422_2P_VFIRST},
	{-EINVAL, 0, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_NO, 0},
	{0, 0, DMA_INOUT_FORMAT_YUV422, 3, DMA_INOUT_ORDER_NO, MCSC_YUV422_3P},
	{-EINVAL, 0, DMA_INOUT_FORMAT_YUV422, 0, DMA_INOUT_ORDER_NO, 0},
	/* Y FORMAT */
	{0, 0, DMA_INOUT_FORMAT_Y, 1, DMA_INOUT_ORDER_NO, MCSC_MONO_Y8},
	{-EINVAL, 0, DMA_INOUT_FORMAT_BAYER, 1, DMA_INOUT_ORDER_NO, 0},
};

static void pablo_hw_mcsc_adjust_input_img_fmt_kunit_test(struct kunit *test)
{
	int ret, k;
	u32 img_format;

	for (k = 0; k < sizeof(adj_input) / sizeof(struct pablo_hw_mcsc_test_vector); k++) {
		img_format = 0;
		ret = is_hw_mcsc_adjust_input_img_fmt(adj_input[k].format, adj_input[k].plane,
			adj_input[k].order, &img_format);
		KUNIT_EXPECT_EQ(test, ret, adj_input[k].ret);
		KUNIT_EXPECT_EQ(test, img_format, adj_input[k].img_format);
	}
}

static struct pablo_hw_mcsc_test_vector adj_output[] = {
	/* DMA_INOUT_BIT_WIDTH_8BIT */
	/* 420 FORMAT */
	{0, 8, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_CbCr, MCSC_YUV420_2P_UFIRST},
	{0, 8, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_CrCb, MCSC_YUV420_2P_VFIRST},
	{-EINVAL, 8, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_NO, 0},
	{0, 8, DMA_INOUT_FORMAT_YUV420, 3, DMA_INOUT_ORDER_NO, MCSC_YUV420_3P},
	/* 422 FORMAT */
	{0, 8, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_CrYCbY, MCSC_YUV422_1P_VYUY},
	{0, 8, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_CbYCrY, MCSC_YUV422_1P_UYVY},
	{0, 8, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_YCrYCb, MCSC_YUV422_1P_YVYU},
	{0, 8, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_YCbYCr, MCSC_YUV422_1P_YUYV},
	{-EINVAL, 8, DMA_INOUT_FORMAT_YUV422, 1, DMA_INOUT_ORDER_NO, 0},
	{0, 8, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_CbCr, MCSC_YUV422_2P_UFIRST},
	{0, 8, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_CrCb, MCSC_YUV422_2P_VFIRST},
	{-EINVAL, 8, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_NO, 0},
	{0, 8, DMA_INOUT_FORMAT_YUV422, 3, DMA_INOUT_ORDER_NO, MCSC_YUV422_3P},
	{-EINVAL, 8, DMA_INOUT_FORMAT_YUV422, 4, DMA_INOUT_ORDER_NO, 0},
	/* RGB FORMAT */
	{0, 8, DMA_INOUT_FORMAT_RGB, 1, DMA_INOUT_ORDER_ARGB, MCSC_RGB_ARGB8888},
	{0, 8, DMA_INOUT_FORMAT_RGB, 1, DMA_INOUT_ORDER_BGRA, MCSC_RGB_BGRA8888},
	{0, 8, DMA_INOUT_FORMAT_RGB, 1, DMA_INOUT_ORDER_RGBA, MCSC_RGB_RGBA8888},
	{0, 8, DMA_INOUT_FORMAT_RGB, 1, DMA_INOUT_ORDER_ABGR, MCSC_RGB_ABGR8888},
	{0, 8, DMA_INOUT_FORMAT_RGB, 1, DMA_INOUT_ORDER_NO, MCSC_RGB_RGBA8888},
	/* 444 FORMAT */
	{0, 8, DMA_INOUT_FORMAT_YUV444, 1, DMA_INOUT_ORDER_NO, MCSC_YUV444_1P},
	{0, 8, DMA_INOUT_FORMAT_YUV444, 3, DMA_INOUT_ORDER_NO, MCSC_YUV444_3P},
	{-EINVAL, 8, DMA_INOUT_FORMAT_YUV444, 4, DMA_INOUT_ORDER_NO, 0},
	/* Y FORMAT */
	{0, 8, DMA_INOUT_FORMAT_Y, 1, DMA_INOUT_ORDER_NO, MCSC_MONO_Y8},
	{-EINVAL, 8, DMA_INOUT_FORMAT_BAYER, 1, DMA_INOUT_ORDER_NO, 0},

	/* DMA_INOUT_BIT_WIDTH_10BIT */
	/* 420 FORMAT */
	{0, 10, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_CbCr, MCSC_YUV420_2P_UFIRST_PACKED10},
	{0, 10, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_CrCb, MCSC_YUV420_2P_VFIRST_PACKED10},
	{-EINVAL, 10, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_NO, 0},
	{0, 10, DMA_INOUT_FORMAT_YUV420, 4, DMA_INOUT_ORDER_CbCr, MCSC_YUV420_2P_UFIRST_8P2},
	{0, 10, DMA_INOUT_FORMAT_YUV420, 4, DMA_INOUT_ORDER_CrCb, MCSC_YUV420_2P_VFIRST_8P2},
	{-EINVAL, 10, DMA_INOUT_FORMAT_YUV420, 4, DMA_INOUT_ORDER_NO, 0},
	{-EINVAL, 10, DMA_INOUT_FORMAT_YUV420, 0, DMA_INOUT_ORDER_NO, 0},
	/* 422 FORMAT */
	{0, 10, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_CbCr, MCSC_YUV422_2P_UFIRST_PACKED10},
	{0, 10, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_CrCb, MCSC_YUV422_2P_VFIRST_PACKED10},
	{-EINVAL, 10, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_NO, 0},
	{0, 10, DMA_INOUT_FORMAT_YUV422, 4, DMA_INOUT_ORDER_CbCr, MCSC_YUV422_2P_UFIRST_8P2},
	{0, 10, DMA_INOUT_FORMAT_YUV422, 4, DMA_INOUT_ORDER_CrCb, MCSC_YUV422_2P_VFIRST_8P2},
	{-EINVAL, 10, DMA_INOUT_FORMAT_YUV422, 4, DMA_INOUT_ORDER_NO, 0},
	{-EINVAL, 10, DMA_INOUT_FORMAT_YUV422, 0, DMA_INOUT_ORDER_NO, 0},
	/* RGB FORMAT */
	{0, 10, DMA_INOUT_FORMAT_RGB, 1, DMA_INOUT_ORDER_RGBA, MCSC_RGB_RGBA1010102},
	{0, 10, DMA_INOUT_FORMAT_RGB, 1, DMA_INOUT_ORDER_ABGR, MCSC_RGB_ABGR1010102},
	{-EINVAL, 10, DMA_INOUT_FORMAT_RGB, 1, DMA_INOUT_ORDER_NO, 0},
	/* 444 FORMAT */
	{0, 10, DMA_INOUT_FORMAT_YUV444, 1, DMA_INOUT_ORDER_NO, MCSC_YUV444_1P_PACKED10},
	{0, 10, DMA_INOUT_FORMAT_YUV444, 3, DMA_INOUT_ORDER_NO, MCSC_YUV444_3P_PACKED10},
	{-EINVAL, 10, DMA_INOUT_FORMAT_YUV444, 4, DMA_INOUT_ORDER_NO, 0},
	{-EINVAL, 10, DMA_INOUT_FORMAT_BAYER, 1, DMA_INOUT_ORDER_NO, 0},

	/* DMA_INOUT_BIT_WIDTH_16BIT */
	/* 420 FORMAT */
	{0, 16, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_CbCr, MCSC_YUV420_2P_UFIRST_P010},
	{0, 16, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_CrCb, MCSC_YUV420_2P_VFIRST_P010},
	{-EINVAL, 16, DMA_INOUT_FORMAT_YUV420, 2, DMA_INOUT_ORDER_NO, 0},
	/* 422 FORMAT */
	{0, 16, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_CbCr, MCSC_YUV422_2P_UFIRST_P210},
	{0, 16, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_CrCb, MCSC_YUV422_2P_VFIRST_P210},
	{-EINVAL, 16, DMA_INOUT_FORMAT_YUV422, 2, DMA_INOUT_ORDER_NO, 0},
	/* 444 FORMAT */
	{0, 16, DMA_INOUT_FORMAT_YUV444, 1, DMA_INOUT_ORDER_NO, MCSC_YUV444_1P_UNPACKED},
	{0, 16, DMA_INOUT_FORMAT_YUV444, 3, DMA_INOUT_ORDER_NO, MCSC_YUV444_3P_UNPACKED},
	{-EINVAL, 16, DMA_INOUT_FORMAT_YUV444, 4, DMA_INOUT_ORDER_NO, 0},
	{-EINVAL, 16, DMA_INOUT_FORMAT_BAYER, 1, DMA_INOUT_ORDER_NO, 0},
	{-EINVAL, 32, DMA_INOUT_FORMAT_BAYER, 1, DMA_INOUT_ORDER_NO, 0},
};

static void pablo_hw_mcsc_adjust_output_img_fmt_kunit_test(struct kunit *test)
{
	int ret, k;
	u32 img_format;
	bool conv420_flag;

	for (k = 0; k < sizeof(adj_output) / sizeof(struct pablo_hw_mcsc_test_vector); k++) {
		img_format = 0;
		ret = is_hw_mcsc_adjust_output_img_fmt(adj_output[k].bitwidth,
			adj_output[k].format, adj_output[k].plane,
			adj_output[k].order, &img_format, &conv420_flag);
		KUNIT_EXPECT_EQ(test, ret, adj_output[k].ret);
		KUNIT_EXPECT_EQ(test, img_format, adj_output[k].img_format);
	}
}

struct pablo_hw_mcsc_chk_test_vector {
	int ret;
	enum mcsc_io_type type;
	u32 format, bit_width, width, height;
};

static struct pablo_hw_mcsc_chk_test_vector chk_fmt[] = {
	{0, HW_MCSC_OTF_INPUT, OTF_INPUT_FORMAT_YUV422, 14, 320, 240},
	{0, HW_MCSC_OTF_INPUT, OTF_INPUT_FORMAT_BAYER, 14, 320, 240},
	{0, HW_MCSC_OTF_INPUT, OTF_INPUT_FORMAT_YUV422, 14, 320, 0},
	{0, HW_MCSC_OTF_INPUT, OTF_INPUT_FORMAT_YUV422, 14, 0, 240},
	{0, HW_MCSC_OTF_OUTPUT, OTF_INPUT_FORMAT_YUV422, 14, 320, 240},
	{0, HW_MCSC_OTF_OUTPUT, OTF_INPUT_FORMAT_BAYER, 14, 320, 240},
	{0, HW_MCSC_OTF_OUTPUT, OTF_INPUT_FORMAT_YUV422, 14, 320, 0},
	{0, HW_MCSC_OTF_OUTPUT, OTF_INPUT_FORMAT_YUV422, 14, 0, 240},
	{0, HW_MCSC_DMA_INPUT, OTF_INPUT_FORMAT_YUV422, 14, 320, 240},
	{0, HW_MCSC_DMA_INPUT, OTF_INPUT_FORMAT_BAYER, 14, 320, 240},
	{0, HW_MCSC_DMA_INPUT, OTF_INPUT_FORMAT_YUV422, 14, 320, 0},
	{0, HW_MCSC_DMA_INPUT, OTF_INPUT_FORMAT_YUV422, 14, 0, 240},
	{0, HW_MCSC_DMA_OUTPUT, OTF_INPUT_FORMAT_YUV422, 14, 320, 240},
	{0, HW_MCSC_DMA_OUTPUT, OTF_INPUT_FORMAT_BAYER, 14, 320, 240},
	{0, HW_MCSC_DMA_OUTPUT, OTF_INPUT_FORMAT_YUV422, 14, 320, 0},
	{0, HW_MCSC_DMA_OUTPUT, OTF_INPUT_FORMAT_YUV422, 14, 0, 240},
};

static void pablo_hw_mcsc_check_format_kunit_test(struct kunit *test)
{
	int ret, k;

	for (k = 0; k < sizeof(chk_fmt) / sizeof(struct pablo_hw_mcsc_chk_test_vector); k++) {
		ret = is_hw_mcsc_check_format(chk_fmt[k].type, chk_fmt[k].format,
			chk_fmt[k].bit_width, chk_fmt[k].width, chk_fmt[k].height);
		KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	}
}

static void pablo_hw_mcsc_param_get_debug_mcsc_kunit_test(struct kunit *test)
{
	const struct kernel_param_ops *param_ops_debug_mcsc;
	char buffer[200];
	struct kernel_param kp;

	is_hw_mcsc_s_debug_type(MCSC_DBG_DUMP_REG);

	param_ops_debug_mcsc = is_hw_mcsc_get_param_ops_debug_mcsc_kunit_wrapper();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, param_ops_debug_mcsc);
	param_ops_debug_mcsc->get(buffer, &kp);

	is_hw_mcsc_c_debug_type(MCSC_DBG_DUMP_REG);
}

static void pablo_hw_mcsc_reset_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
	u32 instance = 0;

	hw_ip->ops = test_ctx.org_ip_ops;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	KUNIT_EXPECT_EQ(test, ret, -EBUSY);
}

static struct kunit_case pablo_hw_mcsc_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_mcsc_open_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_handle_interrupt_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_shot_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_enable_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_dump_regs_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_notify_timeout_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_restore_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_setfile_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_set_param_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_frame_ndone_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_set_ni_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_adjust_input_img_fmt_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_adjust_output_img_fmt_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_check_format_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_param_get_debug_mcsc_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_reset_kunit_test),
	{},
};

static void __setup_hw_ip(struct kunit *test)
{
	int ret;
	enum is_hardware_id hw_id = DEV_HW_MCSC0;
	struct is_interface *itf = NULL;
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
	struct is_interface_ischain *itfc = &test_ctx.itfc;

	pablo_hw_chain_info_probe(&test_ctx.hardware);
	hw_ip->hardware = &test_ctx.hardware;

	ret = is_hw_mcsc_probe(hw_ip, itf, itfc, hw_id, "MCSC");
	KUNIT_ASSERT_EQ(test, ret, 0);

	hw_ip->id = hw_id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "MCSC");
	hw_ip->itf = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);
	hw_ip->state = 0;

	hw_ip->framemgr = &test_ctx.framemgr;

	test_ctx.org_ip_ops = (struct is_hw_ip_ops *)hw_ip->ops;
	test_ctx.ip_ops = *(hw_ip->ops);
	test_ctx.ip_ops.reset = __reset_stub;
	hw_ip->ops = &test_ctx.ip_ops;
}

static int pablo_hw_mcsc_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	test_ctx.hardware.hw_ip[0].regs[REG_SETA] = test_ctx.test_addr;

	ret = is_mem_init(&test_ctx.mem, is_get_is_core()->pdev);
	KUNIT_ASSERT_EQ(test, ret, 0);

	__setup_hw_ip(test);

	return 0;
}

static void pablo_hw_mcsc_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.test_addr);
	memset(&test_ctx, 0, sizeof(test_ctx));
}

struct kunit_suite pablo_hw_mcsc_kunit_test_suite = {
	.name = "pablo-hw-mcsc-v4-kunit-test",
	.init = pablo_hw_mcsc_kunit_test_init,
	.exit = pablo_hw_mcsc_kunit_test_exit,
	.test_cases = pablo_hw_mcsc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_mcsc_kunit_test_suite);

MODULE_LICENSE("GPL");
