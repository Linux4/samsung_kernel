// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "is-hw-api-csi.h"
#include "is-device-csi.h"
#if IS_ENABLED(CONFIG_CSIS_V5_4_8)
#include "csi/is-hw-csi-v5_4_8.h"
#else
#include "csi/is-hw-csi-v8_0.h"
#endif

/* Define the test cases. */

static void pablo_hw_csi_bns_dump_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_bns_dump);

	func->csi_hw_bns_dump(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_clear_fro_count_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	void *test_addr2 = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr2);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_clear_fro_count);

	func->csi_hw_clear_fro_count(test_addr, test_addr2);

	kunit_kfree(test, test_addr);
	kunit_kfree(test, test_addr2);
}

static void pablo_hw_csi_fro_dump_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_fro_dump);

	func->csi_hw_fro_dump(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_g_bns_scale_factor_kunit_test(struct kunit *test)
{
	u32 factor;
	u32 in = 2000;
	u32 out = 2000;
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_bns_scale_factor);

	factor = func->csi_hw_g_bns_scale_factor(in, out);
	KUNIT_EXPECT_EQ(test, factor, (u32)2);
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

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_g_ebuf_irq_src_kunit_test(struct kunit *test)
{
	struct csis_irq_src irq_src = { 0 };
	int ebuf_ch = 0;
	unsigned int offset_fake_frame_done = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_ebuf_irq_src);

	func->csi_hw_g_ebuf_irq_src(test_addr, &irq_src, ebuf_ch, offset_fake_frame_done);
	KUNIT_EXPECT_EQ(test, irq_src.err_flag, (bool)false);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_g_irq_src_kunit_test(struct kunit *test)
{
	int ret;
	struct csis_irq_src irq_src = { 0 };
	bool clear = true;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_irq_src);

	ret = func->csi_hw_g_irq_src(test_addr, &irq_src, clear);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, irq_src.err_flag, (bool)false);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_g_frame_ptr_kunit_test(struct kunit *test)
{
	u32 frame_ptr;
	u32 vc = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_frameptr);

	frame_ptr = func->csi_hw_g_frameptr(test_addr, vc);
	KUNIT_EXPECT_EQ(test, frame_ptr, (u32)0);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_g_mapped_phy_port_kunit_test(struct kunit *test)
{
	u32 port;
	u32 csi_ch = 0;
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_g_mapped_phy_port);

	port = func->csi_hw_g_mapped_phy_port(csi_ch);
	KUNIT_EXPECT_EQ(test, port, (u32)0);
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

	enable = func->csi_hw_g_output_cur_dma_enable(test_addr, vc);
	KUNIT_EXPECT_EQ(test, enable, (bool)false);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_reset_bns_kunit_test(struct kunit *test)
{
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_reset_bns);

	func->csi_hw_reset_bns(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_s_bns_ch_kunit_test(struct kunit *test)
{
	u32 ch = 0;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_s_bns_ch);

	func->csi_hw_s_bns_ch(test_addr, ch);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_s_cfg_ebuf_kunit_test(struct kunit *test)
{
	u32 ebuf_ch = 0;
	u32 vc = 0;
	u32 width = 1920;
	u32 height = 1080;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_s_cfg_ebuf);

	func->csi_hw_s_cfg_ebuf(test_addr, ebuf_ch, vc, width, height);

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
	u32 width = 1920;
	u32 height = 1080;
	u32 fps = 30;
	u32 clk = 24000000;
	void *test_addr = kunit_kzalloc(test, 0x8000, 0);
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_addr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_s_dma_common_pattern_enable);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, func->csi_hw_s_dma_common_pattern_disable);

	ret = func->csi_hw_s_dma_common_pattern_enable(test_addr, width, height, fps, clk);
	KUNIT_EXPECT_EQ(test, ret, 0);

	func->csi_hw_s_dma_common_pattern_disable(test_addr);

	kunit_kfree(test, test_addr);
}

static void pablo_hw_csi_hw_s_dma_input_mux(struct kunit *test)
{
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	void *reg_mux = kunit_kzalloc(test, 0x8000, 0);
	u32 otf_ch = 2;
	u32 bns_dma_mux_val = 0x24;
	u32 csi_ch = 3;
	u32 wdma_ch = 0;
	u32 val;
	bool bns_en = true;

	/* bns ---> wdma_ch */
	func->csi_hw_s_dma_input_mux(reg_mux, wdma_ch, bns_en,
			bns_dma_mux_val, csi_ch, otf_ch, 0, 0, 0);
	val = readl(reg_mux);
	KUNIT_EXPECT_EQ(test, val, bns_dma_mux_val);

	/* csi->ch ---> wdma_ch */
	bns_en = false;
	func->csi_hw_s_dma_input_mux(reg_mux, wdma_ch, bns_en,
			bns_dma_mux_val, csi_ch, otf_ch, 0, 0, 0);
	val = readl(reg_mux);
	KUNIT_EXPECT_EQ(test, val, csi_ch);

	kunit_kfree(test, reg_mux);
}

static void pablo_hw_csi_hw_s_init_input_mux(struct kunit *test)
{
	struct pablo_kunit_hw_csi_func *func = pablo_kunit_get_hw_csi_test();
	void *reg_mux = kunit_kzalloc(test, 0x8000, 0);
	u32 val;

	func->csi_hw_s_init_input_mux(reg_mux, 0);

	val = readl(reg_mux);
	KUNIT_EXPECT_EQ(test, val, (u32)0xFFFFFFFF);

	kunit_kfree(test, reg_mux);
}

static struct kunit_case pablo_hw_csi_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_csi_bns_dump_kunit_test),
	KUNIT_CASE(pablo_hw_csi_clear_fro_count_kunit_test),
	KUNIT_CASE(pablo_hw_csi_fro_dump_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_bns_scale_factor_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_dam_common_frame_id_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_ebuf_irq_src_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_irq_src_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_frame_ptr_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_mapped_phy_port_kunit_test),
	KUNIT_CASE(pablo_hw_csi_g_output_cur_dma_enable_kunit_test),
	KUNIT_CASE(pablo_hw_csi_reset_bns_kunit_test),
	KUNIT_CASE(pablo_hw_csi_s_bns_ch_kunit_test),
	KUNIT_CASE(pablo_hw_csi_s_cfg_ebuf_kunit_test),
	KUNIT_CASE(pablo_hw_csi_s_dma_common_dynamic_kunit_test),
	KUNIT_CASE(pablo_hw_csi_s_dma_common_pattern_kunit_test),
	KUNIT_CASE(pablo_hw_csi_hw_s_dma_input_mux),
	KUNIT_CASE(pablo_hw_csi_hw_s_init_input_mux),
	{},
};

struct kunit_suite pablo_hw_csi_kunit_test_suite = {
	.name = "pablo-hw-csi-kunit-test",
	.test_cases = pablo_hw_csi_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_csi_kunit_test_suite);

MODULE_LICENSE("GPL");
