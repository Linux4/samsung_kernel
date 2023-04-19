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

#include "is-device-sensor.h"
#include "pablo-hw-api-common.h"
#include "sfr/is-sfr-csi_pdp_top-v6_0.h"
#include "api/is-hw-api-csis_pdp_top.h"
#include "pablo-smc.h"
#include "is-device-csi.h"

struct testcase
{
	int format;
	int bit_mode;
};
#define TESTCASE_MAX 4

#define IBUF_MUX_VAL_BASE_LINK_VC_0_5	0x0
#define IBUF_MUX_VAL_BASE_LINK_VC_6_9	0x8

/* Define the test cases. */
static struct is_device_csi _csi;
static void pablo_hw_csi_pdp_top_frame_id_en_kunit_test(struct kunit *test)
{
	struct is_fid_loc fid_loc;
	struct pablo_camif_csis_pdp_top top;
	struct pablo_kunit_hw_csis_pdp_top_func *func = pablo_kunit_get_hw_csis_pdp_top_test();
	u32 val;
	int flags = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->frame_id_en);

	top.regs = kunit_kmalloc(test, 0xF000, flags);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, top.regs);

	/* normal case */
	_csi.top = &top;
	_csi.otf_info.csi_ch = 0;
	_csi.f_id_dec = true;
	fid_loc.valid = true;
	fid_loc.byte = 0xDEAD;
	fid_loc.line = 0x3;

	func->frame_id_en(&_csi, &fid_loc);
	val = func->get_frame_id_en(top.regs, &_csi);
	KUNIT_EXPECT_EQ(test, val, (u32)0xDEAD0301);

	fid_loc.valid = false;
	func->frame_id_en(&_csi, &fid_loc);
	val = func->get_frame_id_en(top.regs, &_csi);
	KUNIT_EXPECT_EQ(test, val, (u32)0x001B0001);

	/* if there is no f_id_dec */
	_csi.f_id_dec = false;
	func->frame_id_en(&_csi, &fid_loc);
	val = func->get_frame_id_en(top.regs, &_csi);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	kunit_kfree(test, top.regs);

	/* if there is no CSIS_PDP */
	_csi.top = NULL;
	func->frame_id_en(&_csi, &fid_loc);
}

static void pablo_hw_csi_pdp_top_qch_kunit_test(struct kunit *test)
{
	int ret;
	void *test_addr = kunit_kzalloc(test, 0xF000, 0);
	bool qch_enable;
	struct pablo_kunit_hw_csis_pdp_top_func *func = pablo_kunit_get_hw_csis_pdp_top_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->qch_cfg);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	/* Power on qch on */
	qch_enable = true;
	func->qch_cfg(test_addr, qch_enable);

	ret = is_hw_get_field(test_addr, &csis_top_regs[CSIS_R_CSIS_CTRL],
					&csis_top_fields[CSIS_F_QACTIVE_ON]);
	KUNIT_EXPECT_EQ(test, 1, ret);

	/* Power on qch on */
	qch_enable = false;
	func->qch_cfg(test_addr, qch_enable);

	ret = is_hw_get_field(test_addr, &csis_top_regs[CSIS_R_CSIS_CTRL],
					&csis_top_fields[CSIS_F_QACTIVE_ON]);
	KUNIT_EXPECT_EQ(test, 0, ret);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_pdp_top_set_ibuf_kunit_test(struct kunit *test)
{
	int ret;
	void *test_addr = kunit_kzalloc(test, 0xF000, 0);
	u32 ibuf_reg_offset, ibuf_vc_reg_offset;
	u32 i, otf_ch, lc, link_vc;
	struct pablo_kunit_hw_csis_pdp_top_func *func = pablo_kunit_get_hw_csis_pdp_top_test();
	struct is_sensor_cfg sensor_cfg;
	struct testcase tc[TESTCASE_MAX] = {
		{HW_FORMAT_RAW8, 0},
		{HW_FORMAT_RAW10, 1},
		{HW_FORMAT_RAW12, 2},
		{HW_FORMAT_RAW14, 3},
	};
	u32 link_vc_list[CAMIF_OTF_OUT_ID_NUM][CSIS_OTF_CH_LC_NUM] = {
		{0, 1, 2, 3, 4, 5},
		{6, 7, 8, 9, 10, 10},
		{0, 1, 2, 3, 4, 5},
	};


	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	/* size set test */
	for (i = 0; i < CSI_VIRTUAL_CH_MAX; i++) {
		sensor_cfg.input[i].width = i;
		sensor_cfg.input[i].height = i;
		sensor_cfg.input[i].hwformat = HW_FORMAT_RAW8;
	}

	for (otf_ch = 0; otf_ch < CSI_OTF_OUT_CH_NUM; otf_ch++) {
		csis_pdp_top_set_ibuf(test_addr, otf_ch, otf_ch, link_vc_list[otf_ch], &sensor_cfg);

		ibuf_reg_offset = otf_ch * (CSIS_R_IBUF1_INPUT_CONFIG_CNTL_0 - CSIS_R_IBUF0_INPUT_CONFIG_CNTL_0);

		for (lc = 0; lc < CSIS_OTF_CH_LC_NUM; lc++) {
			link_vc = link_vc_list[otf_ch][lc];

			ibuf_vc_reg_offset = lc *
				(CSIS_R_IBUF0_INPUT_CONFIG_VC1_0 - CSIS_R_IBUF0_INPUT_CONFIG_VC0_0);

			if (link_vc >= CSI_VIRTUAL_CH_MAX)
				continue;

			ret = is_hw_get_field(test_addr,
				&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_0  + ibuf_reg_offset + ibuf_vc_reg_offset],
				&csis_top_fields[CSIS_F_IBUF_WIDTH_VC0]);
			KUNIT_EXPECT_EQ(test, sensor_cfg.input[link_vc].width, ret);

			ret = is_hw_get_field(test_addr,
				&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_0  + ibuf_reg_offset + ibuf_vc_reg_offset],
				&csis_top_fields[CSIS_F_IBUF_HEIGHT_VC0]);
			KUNIT_EXPECT_EQ(test, sensor_cfg.input[link_vc].height, ret);
		}
	}

	/* bit_mode set test */
	for (i = 0; i < TESTCASE_MAX; i++) {
		sensor_cfg.input[0].hwformat = tc[i].format;
		csis_pdp_top_set_ibuf(test_addr, 0, 0, link_vc_list[0], &sensor_cfg);

		ret = is_hw_get_field(test_addr,
			&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_1],
			&csis_top_fields[CSIS_F_IBUF_BITMODE_VC0]);
		KUNIT_EXPECT_EQ(test, tc[i].bit_mode, ret);
	}

	/* potf and user_emb set test */
	sensor_cfg.input[0].type = VC_EMBEDDED;
	sensor_cfg.input[0].extformat = HW_FORMAT_RAW10_POTF;
	csis_pdp_top_set_ibuf(test_addr, 0, 0, link_vc_list[0], &sensor_cfg);

	ret = is_hw_get_field(test_addr,
		&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_1],
		&csis_top_fields[CSIS_F_IBUF_USER_EMB_VC0]);
	KUNIT_EXPECT_EQ(test, 1, ret);

	ret = is_hw_get_field(test_addr,
		&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_VC0_1],
		&csis_top_fields[CSIS_F_IBUF_OTF_EN_VC0]);
	KUNIT_EXPECT_EQ(test, 0, ret);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_pdp_top_s_otf_out_mux_kunit_test(struct kunit *test)
{
	struct pablo_kunit_hw_csis_pdp_top_func *func = pablo_kunit_get_hw_csis_pdp_top_test();
	void *base = kunit_kzalloc(test, 0x1000, 0);
	u32 otf_ch, val, img_vc, lc, start_vc, end_vc, csi_ch = 1, otf_out_id = 0;
	u32 link_vc_list[CAMIF_OTF_OUT_ID_NUM][CSIS_OTF_CH_LC_NUM];

	/* Case 1. Set MUX */
	img_vc = 0;
	start_vc = 0;
	end_vc = 5;
	for (otf_ch = 0; otf_ch < MAX_NUM_CSIS_OTF_CH; otf_ch++) {
		func->s_otf_out_mux(base, csi_ch, otf_ch, img_vc, link_vc_list[otf_out_id], true);

		val = is_hw_get_field(base,
			&csis_top_regs[CSIS_R_IBUF_MUX0 + otf_ch],
			&csis_top_fields[CSIS_F_GLUEMUX_IBUF_MUX_SEL0]);
		KUNIT_EXPECT_EQ(test, IBUF_MUX_VAL_BASE_LINK_VC_0_5 + csi_ch, val);

		lc = 0;
		while (start_vc <= end_vc)
			KUNIT_EXPECT_EQ(test, start_vc++, link_vc_list[otf_out_id][lc++]);
	}

	img_vc = 6;
	start_vc = 6;
	end_vc = 9;
	for (otf_ch = 0; otf_ch < MAX_NUM_CSIS_OTF_CH; otf_ch++) {
		func->s_otf_out_mux(base, csi_ch, otf_ch, img_vc, link_vc_list[otf_out_id], true);

		val = is_hw_get_field(base,
			&csis_top_regs[CSIS_R_IBUF_MUX0 + otf_ch],
			&csis_top_fields[CSIS_F_GLUEMUX_IBUF_MUX_SEL0]);
		KUNIT_EXPECT_EQ(test, IBUF_MUX_VAL_BASE_LINK_VC_6_9 + csi_ch, val);

		lc = 0;
		while (start_vc <= end_vc)
			KUNIT_EXPECT_EQ(test, start_vc++, link_vc_list[otf_out_id][lc++]);
	}

	/* Case 2. Reset MUX */
	img_vc = 0;
	for (otf_ch = 0; otf_ch < MAX_NUM_CSIS_OTF_CH; otf_ch++) {
		func->s_otf_out_mux(base, csi_ch, otf_ch, img_vc, link_vc_list[otf_out_id], false);

		val = is_hw_get_field(base,
			&csis_top_regs[CSIS_R_IBUF_MUX0 + otf_ch],
			&csis_top_fields[CSIS_F_GLUEMUX_IBUF_MUX_SEL0]);
		KUNIT_EXPECT_EQ(test, 0x3f, val);
	}

	kunit_kfree(test, base);
}

static void pablo_hw_csi_pdp_top_s_otf_lc_kunit_test(struct kunit *test)
{
	struct pablo_kunit_hw_csis_pdp_top_func *func = pablo_kunit_get_hw_csis_pdp_top_test();
	void *base = kunit_kzalloc(test, 0x1000, 0);
	u32 *reg;
	u32 otf_ch, lc_num, val;
	u32 lc[] = {0, 2, 3};

	for (otf_ch = 0; otf_ch < MAX_NUM_CSIS_OTF_CH; otf_ch++) {
		reg = (u32 *)(base + csis_top_regs[CSIS_R_PDP_VC_CON0 + otf_ch].sfr_offset);
		memset(reg, 0, 0x4);

		for (lc_num = 1; lc_num < ARRAY_SIZE(lc); lc_num++) {
			func->s_otf_lc(base, otf_ch, lc, lc_num);

			/* Check IMG_LC */
			val = is_hw_get_field_value(*reg, &csis_top_fields[CSIS_F_MUX_IMG_VC_PDP0_A]);
			KUNIT_EXPECT_EQ(test, (lc_num > 0) ? lc[0] : 0, val);

			/* Check HPD LC */
			val = is_hw_get_field_value(*reg, &csis_top_fields[CSIS_F_MUX_AF0_VC_PDP0_A]);
			KUNIT_EXPECT_EQ(test, (lc_num > 1) ? lc[1] : 0, val);

			/* Check VPD LC */
			val = is_hw_get_field_value(*reg, &csis_top_fields[CSIS_F_MUX_AF1_VC_PDP0_A]);
			KUNIT_EXPECT_EQ(test, (lc_num > 2) ? lc[2] : 0, val);
		}
	}

	kunit_kfree(test, base);
}

static void pablo_hw_csi_pdp_top_enable_ibuf_ptrn_gen_kunit_test(struct kunit *test)
{
	int ret;
	bool sensor_sync;
	void *test_addr = kunit_kzalloc(test, 0xF000, 0);
	struct pablo_kunit_hw_csis_pdp_top_func *func = pablo_kunit_get_hw_csis_pdp_top_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);

	sensor_sync = 0;
	csis_pdp_top_enable_ibuf_ptrn_gen(test_addr, 0, sensor_sync);

	ret = is_hw_get_field(test_addr,
		&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_PTRN_0],
		&csis_top_fields[CSIS_F_IBUF_PTRN_SENSOR_SYNC]);
	KUNIT_EXPECT_EQ(test, sensor_sync, ret);

	sensor_sync = 1;
	csis_pdp_top_enable_ibuf_ptrn_gen(test_addr, 0, sensor_sync);

	ret = is_hw_get_field(test_addr,
		&csis_top_regs[CSIS_R_IBUF0_INPUT_CONFIG_PTRN_0],
		&csis_top_fields[CSIS_F_IBUF_PTRN_SENSOR_SYNC]);
	KUNIT_EXPECT_EQ(test, sensor_sync, ret);

	kunit_kfree(test, test_addr);
}

static struct kunit_case pablo_hw_csi_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_csi_pdp_top_frame_id_en_kunit_test),
	KUNIT_CASE(pablo_hw_csi_pdp_top_qch_kunit_test),
	KUNIT_CASE(pablo_hw_csi_pdp_top_s_otf_out_mux_kunit_test),
	KUNIT_CASE(pablo_hw_csi_pdp_top_s_otf_lc_kunit_test),
	KUNIT_CASE(pablo_hw_csi_pdp_top_set_ibuf_kunit_test),
	KUNIT_CASE(pablo_hw_csi_pdp_top_enable_ibuf_ptrn_gen_kunit_test),
	{},
};

struct kunit_suite pablo_hw_csis_pdp_top_kunit_test_suite = {
	.name = "pablo-hw-csis-pdp-top-v6_0-kunit-test",
	.test_cases = pablo_hw_csi_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_csis_pdp_top_kunit_test_suite);

MODULE_LICENSE("GPL");
