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

#include "pablo-hw-api-common.h"
#include "pablo-kunit-test.h"
#include "hardware/sfr/pablo-sfr-dlfe-v1_0.h"
#include "pmio.h"
#include "hardware/api/pablo-hw-api-dlfe.h"
#include "is-param.h"

#define CALL_HWAPI_OPS(op, args...)	test_ctx.ops->op(args)

#define SET_CR(R, value)				\
	do {						\
		u32 *addr = (u32 *)(test_ctx.base + R);	\
		*addr = value;				\
	} while (0)
#define GET_CR(R)			*(u32 *)(test_ctx.base + R)

static struct dlfe_test_ctx {
	void				*base;
	const struct dlfe_hw_ops	*ops;
	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field		*pmio_fields;
	struct dlfe_param_set		p_set;
} test_ctx;

/* Define the testcases. */
static void pablo_hw_api_dlfe_hw_reset_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	int ret;

	pmio = test_ctx.pmio;

	SET_CR(DLFE_R_YUV_DLFECINFIFOCURR_ENABLE, 1);
	SET_CR(DLFE_R_YUV_DLFECINFIFOPREV_ENABLE, 1);
	SET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_ENABLE, 1);

	ret = CALL_HWAPI_OPS(reset, pmio);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_SW_RESET), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_CMDQ_LOCK), 0);

	/* It always fail because it's not cleared by actual HW. */
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_api_dlfe_hw_init_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(init, pmio);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_IP_PROCESSING), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_CONFIG), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_CONFIG), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_CMDQ_VHD_CONTROL), (1 << 24));
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_INT_REQ_INT0_ENABLE), INT0_EN_MASK);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_INT_REQ_INT1_ENABLE), INT1_EN_MASK);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_CMDQ_QUE_CMD_L), DLFE_INT_GRP_EN_MASK);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_CMDQ_QUE_CMD_M), (3 << 12));
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_CMDQ_ENABLE), 1);
}

static void pablo_hw_api_dlfe_hw_s_core_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	struct dlfe_param_set *p_set;
	u32 w, h, val;

	pmio = test_ctx.pmio;
	p_set = &test_ctx.p_set;

	/* TC#1. DLFE Chain configuration. */
	w = __LINE__;
	h = __LINE__;
	p_set->curr_in.width = w;
	p_set->curr_in.height = h;

	CALL_HWAPI_OPS(s_core, pmio, p_set);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_IP_USE_CINFIFO_NEW_FRAME_IN), 0);
	val = (w << 16) | h;
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_SOURCE_IMAGE_DIMENSION), val);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_DESTINATION_IMAGE_DIMENSION), val);
	KUNIT_EXPECT_NE(test, GET_CR(DLFE_R_H_BLANK), 0);

	/* TC#2. Set CRC seed with 0. */
	p_set->crc_seed = 0;

	CALL_HWAPI_OPS(s_core, pmio, p_set);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_STREAM_CRC), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_STREAM_CRC), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_STREAM_CRC), 0);

	/* TC#3. Set CRC seed with non-zero. */

	p_set->crc_seed = val = __LINE__;

	CALL_HWAPI_OPS(s_core, pmio, p_set);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOCURR_STREAM_CRC), (u8)val);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECINFIFOPREV_STREAM_CRC), (u8)val);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_YUV_DLFECOUTFIFOWGT_STREAM_CRC), (u8)val);
}

static void pablo_hw_api_dlfe_hw_s_path_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	struct dlfe_param_set *p_set;
	u32 val;

	pmio = test_ctx.pmio;
	p_set = &test_ctx.p_set;

	/* TC#1. Enable every path. */
	p_set->curr_in.cmd = 1;
	p_set->prev_in.cmd = 1;
	p_set->wgt_out.cmd = 1;

	CALL_HWAPI_OPS(s_path, pmio, p_set);

	val = (1 << 16) | (1 << 8) | 1;
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_IP_USE_OTF_PATH_01), val);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_BYPASS), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_BYPASS_WEIGHT), 0);
	val = (1 << 8) | 1;
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_ENABLE_PATH), val);

	/* TC#2. Disable every path. */
	p_set->curr_in.cmd = 0;
	p_set->prev_in.cmd = 0;
	p_set->wgt_out.cmd = 0;

	CALL_HWAPI_OPS(s_path, pmio, p_set);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_IP_USE_OTF_PATH_01), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_ENABLE_PATH), 0);
}

static void pablo_hw_api_dlfe_hw_q_cmdq_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(q_cmdq, pmio);

	KUNIT_EXPECT_EQ(test, GET_CR(DLFE_R_CMDQ_ADD_TO_QUEUE_0), 1);
}

static void pablo_hw_api_dlfe_hw_g_int_state_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	u32 int_state;

	pmio = test_ctx.pmio;

	SET_CR(DLFE_R_INT_REQ_INT0, __LINE__);
	SET_CR(DLFE_R_INT_REQ_INT1, __LINE__);

	/* TC#1. Get interrupt state with invalid interrupt id. */
	int_state = CALL_HWAPI_OPS(g_int_state, pmio, 5);

	KUNIT_EXPECT_EQ(test, int_state, 0);
	KUNIT_EXPECT_NE(test, GET_CR(DLFE_R_INT_REQ_INT0_CLEAR), GET_CR(DLFE_R_INT_REQ_INT0));
	KUNIT_EXPECT_NE(test, GET_CR(DLFE_R_INT_REQ_INT1_CLEAR), GET_CR(DLFE_R_INT_REQ_INT1));

	/* TC#2. Get INT0 state. */
	int_state = CALL_HWAPI_OPS(g_int_state, pmio, 0);
	KUNIT_EXPECT_EQ(test, int_state, GET_CR(DLFE_R_INT_REQ_INT0));
	KUNIT_EXPECT_EQ(test, int_state, GET_CR(DLFE_R_INT_REQ_INT0_CLEAR));

	/* TC#3. Get INT1 state. */
	int_state = CALL_HWAPI_OPS(g_int_state, pmio, 1);
	KUNIT_EXPECT_EQ(test, int_state, GET_CR(DLFE_R_INT_REQ_INT1));
	KUNIT_EXPECT_EQ(test, int_state, GET_CR(DLFE_R_INT_REQ_INT1_CLEAR));
}

static void pablo_hw_api_dlfe_hw_wait_idle_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	int ret;

	pmio = test_ctx.pmio;

	/* TC#1. Timeout to wait idleness. */
	ret = CALL_HWAPI_OPS(wait_idle, pmio);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	/* TC#2. Succeed to wait idleness. */
	SET_CR(DLFE_R_IDLENESS_STATUS, 1);

	ret = CALL_HWAPI_OPS(wait_idle, pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_dlfe_hw_dump_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(dump, pmio, DLFE_HW_DUMP_CR);
	CALL_HWAPI_OPS(dump, pmio, DLFE_HW_DUMP_DBG_STATE);
	CALL_HWAPI_OPS(dump, pmio, DLFE_HW_DUMP_MODE_NUM);
}

static void pablo_hw_api_dlfe_hw_is_occurred_kunit_test(struct kunit *test)
{
	bool occur;
	u32 status;
	ulong type;

	/* TC#1. No interrupt. */
	status = 0;
	type = BIT_MASK(INT_FRAME_START);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	/* TC#2. Test each interrupt. */
	status = BIT_MASK(INTR0_DLFE_FRAME_START_INT);
	type = BIT_MASK(INT_FRAME_START);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_DLFE_FRAME_END_INT);
	type = BIT_MASK(INT_FRAME_END);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_DLFE_COREX_END_INT_0);
	type = BIT_MASK(INT_COREX_END);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_DLFE_SETTING_DONE_INT);
	type = BIT_MASK(INT_SETTING_DONE);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_DLFE_CINFIFO_0_OVERFLOW_ERROR_INT);
	type = BIT_MASK(INT_ERR0);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR1_DLFE_VOTF_LOST_FLUSH_INT);
	type = BIT_MASK(INT_ERR1);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	/* TC#3. Test interrupt ovarlapping. */
	status = BIT_MASK(INTR0_DLFE_FRAME_START_INT);
	type = BIT_MASK(INTR0_DLFE_FRAME_START_INT) | BIT_MASK(INTR0_DLFE_FRAME_END_INT);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	status = BIT_MASK(INTR0_DLFE_FRAME_START_INT) | BIT_MASK(INTR0_DLFE_FRAME_END_INT);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);
}

static struct kunit_case pablo_hw_api_dlfe_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_dlfe_hw_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_s_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_q_cmdq_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_g_int_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_dlfe_hw_is_occurred_kunit_test),
	{},
};

static struct pablo_mmio *pablo_hw_api_dlfe_pmio_init(void)
{
	struct pmio_config *pcfg;
	struct pablo_mmio *pmio;

	pcfg = &test_ctx.pmio_config;

	pcfg->name = "DLFE";
	pcfg->mmio_base = test_ctx.base;
	pcfg->cache_type = PMIO_CACHE_NONE;

	dlfe_hw_g_pmio_cfg(pcfg);

	pmio = pmio_init(NULL, NULL, pcfg);
	if (IS_ERR(pmio))
		goto err_init;

	if (pmio_field_bulk_alloc(pmio, &test_ctx.pmio_fields,
				pcfg->fields, pcfg->num_fields))
		goto err_field_bulk_alloc;

	return pmio;

err_field_bulk_alloc:
	pmio_exit(pmio);
err_init:
	return NULL;
}

static void pablo_hw_api_dlfe_pmio_deinit(void)
{
	if (test_ctx.pmio_fields) {
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		test_ctx.pmio_fields = NULL;
	}

	if (test_ctx.pmio) {
		pmio_exit(test_ctx.pmio);
		test_ctx.pmio = NULL;
	}
}

static int pablo_hw_api_dlfe_kunit_test_init(struct kunit *test)
{
	memset(&test_ctx, 0, sizeof(struct dlfe_test_ctx));

	test_ctx.base = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.base);

	test_ctx.ops = dlfe_hw_g_ops();

	test_ctx.pmio = pablo_hw_api_dlfe_pmio_init();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.pmio);

	return 0;
}

static void pablo_hw_api_dlfe_kunit_test_exit(struct kunit *test)
{
	pablo_hw_api_dlfe_pmio_deinit();
	kunit_kfree(test, test_ctx.base);
}

struct kunit_suite pablo_hw_api_dlfe_kunit_test_suite = {
	.name = "pablo-hw-api-dlfe-kunit-test",
	.init = pablo_hw_api_dlfe_kunit_test_init,
	.exit = pablo_hw_api_dlfe_kunit_test_exit,
	.test_cases = pablo_hw_api_dlfe_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_dlfe_kunit_test_suite);

MODULE_LICENSE("GPL");
