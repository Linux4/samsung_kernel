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
#include "is-hw-api-csi.h"
#include "is-device-csi.h"
#include "is-device-sensor.h"
#include "csi/is-hw-csi-v6_0.h"
#include "is-hw.h"

#define DMA_INPUT_MUX_NUM	5

/*
 * [00]: OTF0 VC0~3
 * ...
 * [08]: OTF0 VC2~5
 * ...
 */
#define DMA_MUX_VAL_BASE_LC_0_3		0x0
#define DMA_MUX_VAL_BASE_LC_2_5		0x8

/* Define the test cases. */
static void pablo_hw_csi_bns_dump_kunit_test(struct kunit *test)
{
	int ret;
	void *test_addr = NULL;
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_bns_dump);

	ret = func->csi_hw_bns_dump(test_addr);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);

	test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	ret = func->csi_hw_bns_dump(test_addr);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_clear_fro_count_kunit_test(struct kunit *test)
{
	int ret;
	void *test_addr = NULL;
	void *test_addr2 = NULL;
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_clear_fro_count);

	ret = func->csi_hw_clear_fro_count(test_addr, test_addr2);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);

	test_addr = kunit_kzalloc(test, 0x8000, 0);
	test_addr2 = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr2);
	ret = func->csi_hw_clear_fro_count(test_addr, test_addr2);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, test_addr);
	kunit_kfree(test, test_addr2);
}

static void pablo_hw_csi_fro_dump_kunit_test(struct kunit *test)
{
	int ret;
	void *test_addr = NULL;
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_fro_dump);

	ret = func->csi_hw_fro_dump(test_addr);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);

	test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	ret = func->csi_hw_fro_dump(test_addr);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_g_bns_scale_factor_kunit_test(struct kunit *test)
{
	u32 factor, in, out;
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_bns_scale_factor);

	for (in = 2000; in >= 100; in-=100) {
		for (out = in; out >= 100; out-=100) {
			factor = func->csi_hw_g_bns_scale_factor(in, out);
			if (factor)
				KUNIT_EXPECT_EQ(test, factor, (u32)((in * 2) / out));
			else
				KUNIT_EXPECT_EQ(test, factor, (u32)0);
		}
	}
}

static void pablo_hw_csi_g_dam_common_frame_id_kunit_test(struct kunit *test)
{
	int ret;
	u32 batch_num = 1;
	u32 frame_id[2] = { 0 };
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_dma_common_frame_id);

	ret = func->csi_hw_g_dma_common_frame_id(test_addr, batch_num, frame_id);
	KUNIT_EXPECT_EQ(test, ret, -ENOEXEC);

	batch_num = 8;
	*(u32 *)(test_addr +
		csi_dma_cmn_regs[CSIS_DMA_CMN_R_CSIS_CMN_FRO_PREV_FRAME_ID0].sfr_offset) = 0x23333333;

	ret = func->csi_hw_g_dma_common_frame_id(test_addr, batch_num, frame_id);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_EQ(test, frame_id[0], (u32)0x33333332);
	KUNIT_EXPECT_EQ(test, frame_id[1], (u32)0x0);

	batch_num = 16;
	*(u32 *)(test_addr +
		csi_dma_cmn_regs[CSIS_DMA_CMN_R_CSIS_CMN_FRO_PREV_FRAME_ID0].sfr_offset) = 0x23333333;
	*(u32 *)(test_addr +
		csi_dma_cmn_regs[CSIS_DMA_CMN_R_CSIS_CMN_FRO_PREV_FRAME_ID1].sfr_offset) = 0x45555555;
	ret = func->csi_hw_g_dma_common_frame_id(test_addr, batch_num, frame_id);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_EQ(test, frame_id[0], (u32)0x55555554);
	KUNIT_EXPECT_EQ(test, frame_id[1], (u32)0x55555555);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_g_irq_src_kunit_test(struct kunit *test)
{
	int ret;
	struct csis_irq_src irq_src = { 0 };
	bool clear = true;
	u32 fs_int_src = 0xAA;
	u32 fe_int_src = 0xBB;
	u32 line_end = 0xCC;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_irq_src);

	*(u32 *)(test_addr + csi_regs[CSIS_R_FS_INT_SRC].sfr_offset) = fs_int_src;
	*(u32 *)(test_addr + csi_regs[CSIS_R_FE_INT_SRC].sfr_offset) = fe_int_src;
	*(u32 *)(test_addr + csi_regs[CSIS_R_LINE_END].sfr_offset) = line_end;
	*(u32 *)(test_addr + csi_regs[CSIS_R_ERR_LOST_FS].sfr_offset) = 0xF;
	*(u32 *)(test_addr + csi_regs[CSIS_R_ERR_LOST_FE].sfr_offset) = 0xF;
	*(u32 *)(test_addr + csi_regs[CSIS_R_ERR_VRESOL].sfr_offset) = 0xF;
	*(u32 *)(test_addr + csi_regs[CSIS_R_ERR_HRESOL].sfr_offset) = 0xF;
	*(u32 *)(test_addr + csi_regs[CSIS_R_CSIS_INT_SRC0].sfr_offset) = CSIS_IRQ_MASK0;
	*(u32 *)(test_addr + csi_regs[CSIS_R_CSIS_INT_SRC1].sfr_offset) = CSIS_IRQ_MASK1;

	ret = func->csi_hw_g_irq_src(test_addr, &irq_src, clear);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, irq_src.err_flag, (bool)true);
	KUNIT_EXPECT_EQ(test, irq_src.otf_start, fs_int_src);
	KUNIT_EXPECT_EQ(test, irq_src.otf_end, fe_int_src);
	KUNIT_EXPECT_EQ(test, irq_src.line_end, line_end);
	KUNIT_EXPECT_EQ(test, irq_src.err_id[0], (ulong)0xFFFF);

	*(u32 *)(test_addr + csi_regs[CSIS_R_CSIS_INT_SRC0].sfr_offset) = 0x0;
	*(u32 *)(test_addr + csi_regs[CSIS_R_CSIS_INT_SRC1].sfr_offset) = 0x0;
	ret = func->csi_hw_g_irq_src(test_addr, &irq_src, clear);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, irq_src.err_flag, (bool)false);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_g_frame_ptr_kunit_test(struct kunit *test)
{
	u32 frame_ptr;
	u32 vc = 0;
	u32 f_no;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_frameptr);

	for(f_no = 0; f_no < 0x1F; f_no++) {
		*(u32 *)(test_addr + csi_dmax_chx_regs[CSIS_DMAX_CHX_R_ACT_CTRL].sfr_offset) = f_no << 2;
		frame_ptr = func->csi_hw_g_frameptr(test_addr, vc);
		KUNIT_EXPECT_EQ(test, frame_ptr, f_no);
	}

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_g_mapped_phy_port_kunit_test(struct kunit *test)
{
	u32 port, csi_ch;
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_mapped_phy_port);

	for (csi_ch = 0; csi_ch < 4; csi_ch++) {
		port = func->csi_hw_g_mapped_phy_port(csi_ch);
		KUNIT_EXPECT_EQ(test, port, csi_ch);
	}
}

static void pablo_hw_csi_g_output_cur_dma_enable_kunit_test(struct kunit *test)
{
	bool enable;
	u32 vc = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_output_cur_dma_enable);

	*(u32 *)(test_addr + csi_dmax_chx_regs[CSIS_DMAX_CHX_R_ACT_CTRL].sfr_offset) = 0x0;
	enable = func->csi_hw_g_output_cur_dma_enable(test_addr, vc);
	KUNIT_EXPECT_EQ(test, enable, (bool)false);

	*(u32 *)(test_addr + csi_dmax_chx_regs[CSIS_DMAX_CHX_R_ACT_CTRL].sfr_offset) = 0x1;
	enable = func->csi_hw_g_output_cur_dma_enable(test_addr, vc);
	KUNIT_EXPECT_EQ(test, enable, (bool)true);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_reset_bns_kunit_test(struct kunit *test)
{
	u32 val;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_reset_bns);

	func->csi_hw_reset_bns(test_addr);

	val = *(u32 *)(test_addr + csi_bns_regs[CSIS_BNS_R_BYR_BNS_BYPASS].sfr_offset);
	KUNIT_EXPECT_EQ(test, val, (u32)0x1);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_s_bns_ch_kunit_test(struct kunit *test)
{
	u32 ch, val, otf_ch_max = 4;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_s_bns_ch);

	for (ch = 0; ch < otf_ch_max; ch++) {
		func->csi_hw_s_bns_ch(test_addr, ch);
		val = *(u32 *)(test_addr);
		KUNIT_EXPECT_EQ(test, ch, val);
	}

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_s_dma_common_dynamic_kunit_test(struct kunit *test)
{
	int ret;
	size_t size = 0;
	unsigned int dma_ch = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_s_dma_common_dynamic);

	ret = func->csi_hw_s_dma_common_dynamic(test_addr, size, dma_ch);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_s_dma_common_pattern_kunit_test(struct kunit *test)
{
	int ret;
	int clk_mhz;
	int vvalid;
	int vblank;
	int vblank_size;
	u32 width = 1920;
	u32 height = 1080;
	u32 fps = 30;
	u32 clk = 24000000;
	u32 hblank = 0xFF;
	u32 v_to_hblank = 0x14;
	u32 h_to_vblank = 0x28;
	u32 val;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_s_dma_common_pattern_enable);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_s_dma_common_pattern_disable);

	ret = func->csi_hw_s_dma_common_pattern_enable(test_addr, width, height, fps, clk);
	KUNIT_EXPECT_EQ(test, ret, 0);

	val = *(u32 *)(test_addr +
		csi_dma_cmn_regs[CSIS_DMA_CMN_R_CSIS_CMN_TEST_PATTERN_CTRL].sfr_offset);
	KUNIT_EXPECT_EQ(test, hblank, (u32)((val >> 8) & 0xFF));
	KUNIT_EXPECT_EQ(test, v_to_hblank, (u32)(val >> 16) & 0xFF);
	KUNIT_EXPECT_EQ(test, h_to_vblank, (u32)(val & 0xFF));

	val = *(u32 *)(test_addr +
		csi_dma_cmn_regs[CSIS_DMA_CMN_R_CSIS_CMN_TEST_PATTERN_SIZE].sfr_offset);
	KUNIT_EXPECT_EQ(test, width / 2, (u32)(val & 0x3FFF));
	KUNIT_EXPECT_EQ(test, height, (u32)((val >> 16) & 0x3FFF));

	clk_mhz = clk / 1000000;
	vvalid = (width * height) / (clk_mhz * 2);
	vblank = ((1000000 / fps) - vvalid);
	if (vblank < 0)
		vblank = 1000; /* 1000 us */
	vblank_size = vblank * clk_mhz;

	val = *(u32 *)(test_addr +
		csi_dma_cmn_regs[CSIS_DMA_CMN_R_CSIS_CMN_TEST_PATTERN_ON].sfr_offset);
	KUNIT_EXPECT_EQ(test, (u32)(val >> 31), (u32)1);
	KUNIT_EXPECT_EQ(test, (u32)(val >> 30) & 0x1, (u32)1);
	KUNIT_EXPECT_EQ(test, (u32)(val & 0x3FFFFFFF), (u32)vblank_size);

	/* error return check */
	width = 1;
	ret = func->csi_hw_s_dma_common_pattern_enable(test_addr, width, height, fps, clk);
	KUNIT_EXPECT_EQ(test, ret, (int)-EINVAL);

	func->csi_hw_s_dma_common_pattern_disable(test_addr);
	val = *(u32 *)(test_addr +
		csi_dma_cmn_regs[CSIS_DMA_CMN_R_CSIS_CMN_TEST_PATTERN_ON].sfr_offset);
	KUNIT_EXPECT_EQ(test, (u32)(val >> 31), (u32)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_hw_s_dma_input_mux(struct kunit *test)
{
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	void *reg_mux = kunit_kzalloc(test, sizeof(u32), 0);
	bool bns_en;
	u32 otf_ch = 2;
	u32 bns_dma_mux_val = 0x24;
	u32 csi_ch = 0;
	u32 otf_out_id, val;
	struct is_wdma_info wdma_info;
	u32 link_vc, wdma_vc, lc, lc_start, lc_end, mux_val_base, wdma_ch = 0;
	u32 link_vc_list[CAMIF_OTF_OUT_ID_NUM][CSIS_OTF_CH_LC_NUM] = {
		{0, 1, 2, 3, 4, 5},
		{6, 7, 8, 9, 10, 10},
		{0, 1, 2, 3, 4, 5},
	};

	/* Case 1. CSIS OTF out -> WDMA */
	bns_en = false;
	for (otf_out_id = CSI_OTF_OUT_SINGLE; otf_out_id < CSI_OTF_OUT_CH_NUM; otf_out_id++) {
		if (otf_out_id == CSI_OTF_OUT_SINGLE || otf_out_id == CSI_OTF_OUT_SHORT) {
			mux_val_base = DMA_MUX_VAL_BASE_LC_0_3;
			lc_start = 0;
			lc_end = 3;
		} else { /* CSI_OTF_OUT_MID */
			mux_val_base = DMA_MUX_VAL_BASE_LC_2_5;
			lc_start = 2;
			lc_end = 5;
		}

		func->csi_hw_s_dma_input_mux(reg_mux, wdma_ch, bns_en,
				bns_dma_mux_val, csi_ch, otf_ch, otf_out_id,
				link_vc_list[otf_out_id], &wdma_info);
		val = *(u32 *)(reg_mux);
		KUNIT_EXPECT_EQ(test, mux_val_base + otf_ch, val);

		wdma_vc = 0;
		for (lc = lc_start; lc <= lc_end; lc++) {
			link_vc = link_vc_list[otf_out_id][lc];
			KUNIT_EXPECT_EQ(test, otf_out_id, wdma_info.wdma_idx[link_vc]);
			KUNIT_EXPECT_EQ(test, wdma_vc, wdma_info.wdma_vc[link_vc]);
			wdma_vc++;
		}
	}

	/* Case 2. CSIS OTF out -> BNS -> WDMA */
	bns_en = true;
	for (otf_out_id = 0; otf_out_id < CSI_OTF_OUT_CH_NUM; otf_out_id++) {
		func->csi_hw_s_dma_input_mux(reg_mux, wdma_ch, bns_en,
				bns_dma_mux_val, csi_ch, otf_ch, otf_out_id,
				link_vc_list[otf_out_id], &wdma_info);
		val = *(u32 *)(reg_mux);
		KUNIT_EXPECT_EQ(test, bns_dma_mux_val, val);
	}

	kunit_kfree(test, reg_mux);
}

static void pablo_hw_csi_hw_s_init_input_mux(struct kunit *test)
{
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	void *reg_mux = kunit_kzalloc(test, sizeof(u32), 0);
	u32 wdma_ch, val;

	for (wdma_ch = 0; wdma_ch < DMA_INPUT_MUX_NUM; wdma_ch++) {
		func->csi_hw_s_init_input_mux(reg_mux, wdma_ch);
		val = *(u32 *)(reg_mux);
		KUNIT_EXPECT_EQ(test, 0xFFFFFFFF, val);
	}

	kunit_kfree(test, reg_mux);
}

static void pablo_hw_csi_hw_s_sbwc_ctrl(struct kunit *test)
{
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	void *reg_mux = kunit_kzalloc(test, 0x8000, 0);
	u32 sbwc_en, sbwc_type, comp_64b_align, val;

	sbwc_en = 1;
	sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_32B;
	comp_64b_align = 0;

	func->csi_hw_s_sbwc_ctrl(reg_mux, sbwc_en, sbwc_type, comp_64b_align);

	val = *(u32 *)(reg_mux + csi_dmax_regs[CSIS_DMAX_R_SBWC_CTRL].sfr_offset);
	KUNIT_EXPECT_EQ(test, val, (u32)0x1);


	sbwc_en = 1;
	sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;
	comp_64b_align = 1;

	func->csi_hw_s_sbwc_ctrl(reg_mux, sbwc_en, sbwc_type, comp_64b_align);

	val = *(u32 *)(reg_mux + csi_dmax_regs[CSIS_DMAX_R_SBWC_CTRL].sfr_offset);
	KUNIT_EXPECT_EQ(test, val, (u32)0xd);

	kunit_kfree(test, reg_mux);
}


static struct kunit_case pablo_hw_csi_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_csi_bns_dump_kunit_test),
	KUNIT_CASE(pablo_hw_csi_clear_fro_count_kunit_test),
	KUNIT_CASE(pablo_hw_csi_fro_dump_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_bns_scale_factor_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_dam_common_frame_id_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_irq_src_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_frame_ptr_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_mapped_phy_port_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_output_cur_dma_enable_kunit_test),
	KUNIT_CASE(pablo_hw_csi_reset_bns_kunit_test),
	KUNIT_CASE(pablo_hw_csi_s_bns_ch_kunit_test),
	KUNIT_CASE(pablo_hw_csi_s_dma_common_dynamic_kunit_test),
	KUNIT_CASE(pablo_hw_csi_s_dma_common_pattern_kunit_test),
	KUNIT_CASE(pablo_hw_csi_hw_s_dma_input_mux),
	KUNIT_CASE(pablo_hw_csi_hw_s_init_input_mux),
	KUNIT_CASE(pablo_hw_csi_hw_s_sbwc_ctrl),
	{},
};

struct kunit_suite pablo_hw_csi_kunit_test_suite = {
	.name = "pablo-hw-csi-v6_0-kunit-test",
	.test_cases = pablo_hw_csi_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_csi_kunit_test_suite);

MODULE_LICENSE("GPL");
