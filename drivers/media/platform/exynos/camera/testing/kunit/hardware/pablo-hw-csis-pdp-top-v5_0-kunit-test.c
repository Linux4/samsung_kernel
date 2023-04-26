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
#include "sfr/is-sfr-csi_pdp_top-v5_0.h"
#include "api/is-hw-api-csis_pdp_top.h"
#include "pablo-smc.h"
#include "is-device-csi.h"

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

	/* if there is no CSIS_PDP_TOP */
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

	ret = is_hw_get_field(test_addr, &csis_top_regs[CSIS_TOP_R_CSIS_CTRL],
					&csis_top_fields[CSIS_TOP_F_QACTIVE_ON]);
	KUNIT_EXPECT_EQ(test, 1, ret);

	ret = is_hw_get_field(test_addr, &csis_top_regs[CSIS_TOP_R_CSIS_CTRL],
					&csis_top_fields[CSIS_TOP_F_FORCE_BUS_ACT_ON]);
	KUNIT_EXPECT_EQ(test, 1, ret);

	/* Power on qch on */
	qch_enable = false;
	func->qch_cfg(test_addr, qch_enable);

	ret = is_hw_get_field(test_addr, &csis_top_regs[CSIS_TOP_R_CSIS_CTRL],
					&csis_top_fields[CSIS_TOP_F_QACTIVE_ON]);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = is_hw_get_field(test_addr, &csis_top_regs[CSIS_TOP_R_CSIS_CTRL],
					&csis_top_fields[CSIS_TOP_F_FORCE_BUS_ACT_ON]);
	KUNIT_EXPECT_EQ(test, 0, ret);

	kunit_kfree(test, test_addr);
}

typedef unsigned long (*psmc_call_t)(unsigned long cmd, unsigned long arg0,
		unsigned long arg1, unsigned long arg2);

#define MAX_MUX_LIST_CNT 4
static const u32 mux_list[MAX_MUX_LIST_CNT] = {
		0x17250254, /* IBUF_MUX0 */
		0x17250258, /* IBUF_MUX1 */
		0x1725025c, /* IBUF_MUX2 */
		0x17250260, /* IBUF_MUX3 */
};
static u32 mux_val[MAX_MUX_LIST_CNT];

static unsigned long pablo_smc_call_mock(unsigned long cmd, unsigned long arg0,
		unsigned long arg1, unsigned long arg2)
{
	u32 i;

	/* Only SMC_CMD_REG (==-101) cmd is acceptable. */
	if (cmd != (unsigned long)-101)
		return (unsigned long)-1;

	for (i = 0; i < MAX_MUX_LIST_CNT; i++) {
		if (arg0 == SMC_REG_ID_SFR_W(mux_list[i])) {
				mux_val[i] = arg1;
				break;
		}
	}

	return 0;
}

static void pablo_hw_csi_pdp_top_s_otf_out_mux_kunit_test(struct kunit *test)
{
	struct pablo_kunit_hw_csis_pdp_top_func *func = pablo_kunit_get_hw_csis_pdp_top_test();
	struct pablo_smc_ops *psmc_ops = pablo_get_smc_ops();
	void *reg_mux = NULL; /* Not actually used */
	psmc_call_t psmc_backup;
	u32 otf_ch;
	u32 csi_ch = 1;
	u32 img_vc = 0;

	/* Change pablo_smc ops into mock ops. */
	psmc_backup = psmc_ops->call;
	psmc_ops->call = pablo_smc_call_mock;

	memset(mux_val, 0x00, sizeof(mux_val));

	/* Case 1. Set MUX */
	for (otf_ch = 0; otf_ch < MAX_NUM_CSIS_OTF_CH; otf_ch++) {
		func->s_otf_out_mux(reg_mux, csi_ch, otf_ch, img_vc, 0, true);
		KUNIT_EXPECT_EQ(test, csi_ch, mux_val[otf_ch]);
	}

	/* Case 2. Reset MUX */
	for (otf_ch = 0; otf_ch < MAX_NUM_CSIS_OTF_CH; otf_ch++) {
		func->s_otf_out_mux(reg_mux, csi_ch, otf_ch, img_vc, 0, false);
		KUNIT_EXPECT_EQ(test, 0x3f, mux_val[otf_ch]);
	}

	/* Restore pablo_smc ops. */
	psmc_ops->call = psmc_backup;
}

static void pablo_hw_csi_pdp_top_s_otf_lc_kunit_test(struct kunit *test)
{
	struct pablo_kunit_hw_csis_pdp_top_func *func = pablo_kunit_get_hw_csis_pdp_top_test();
	void *base = kunit_kzalloc(test, 0x1000, 0);
	u32 *reg;
	u32 otf_ch, lc_num, val;
	u32 lc[] = {0, 2, 3};

	for (otf_ch = 0; otf_ch < MAX_NUM_CSIS_OTF_CH; otf_ch++) {
		reg = (u32 *)(base + csis_top_regs[CSIS_TOP_R_PDP_VC_CON0 + otf_ch].sfr_offset);
		memset(reg, 0, 0x4);

		for (lc_num = 1; lc_num < ARRAY_SIZE(lc); lc_num++) {
			func->s_otf_lc(base, otf_ch, lc, lc_num);

			/* Check IMG_LC */
			val = is_hw_get_field_value(*reg, &csis_top_fields[CSIS_TOP_F_MUX_IMG_VC_PDP0]);
			KUNIT_EXPECT_EQ(test, (lc_num > 0) ? lc[0] : 0, val);

			/* Check HPD LC */
			val = is_hw_get_field_value(*reg, &csis_top_fields[CSIS_TOP_F_MUX_AF0_VC_PDP0]);
			KUNIT_EXPECT_EQ(test, (lc_num > 1) ? lc[1] : 0, val);

			/* Check VPD LC */
			val = is_hw_get_field_value(*reg, &csis_top_fields[CSIS_TOP_F_MUX_AF1_VC_PDP0]);
			KUNIT_EXPECT_EQ(test, (lc_num > 2) ? lc[2] : 0, val);
		}
	}

	kunit_kfree(test, base);
}

static struct kunit_case pablo_hw_csi_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_csi_pdp_top_frame_id_en_kunit_test),
	KUNIT_CASE(pablo_hw_csi_pdp_top_qch_kunit_test),
	KUNIT_CASE(pablo_hw_csi_pdp_top_s_otf_out_mux_kunit_test),
	KUNIT_CASE(pablo_hw_csi_pdp_top_s_otf_lc_kunit_test),
	{},
};

struct kunit_suite pablo_hw_csis_pdp_top_kunit_test_suite = {
	.name = "pablo-hw-csis-pdp-top-v5_0-kunit-test",
	.test_cases = pablo_hw_csi_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_csis_pdp_top_kunit_test_suite);

MODULE_LICENSE("GPL");
