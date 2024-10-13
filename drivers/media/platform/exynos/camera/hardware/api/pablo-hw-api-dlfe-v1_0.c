// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * DLFE HW control APIs
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw.h"
#include "sfr/pablo-sfr-dlfe-v1_0.h"
#include "pmio.h"
#include "pablo-hw-api-dlfe.h"

/* PMIO MACRO */
#define SET_CR(base, R, val)		PMIO_SET_R(base, R, val)
#define SET_CR_F(base, R, F, val)	PMIO_SET_F(base, R, F, val)
#define SET_CR_V(base, reg_val, F, val)	PMIO_SET_V(base, reg_val, F, val)

#define GET_CR(base, R)			PMIO_GET_R(base, R)
#define GET_CR_F(base, R, F)		PMIO_GET_F(base, R, F)

/* LOG MACRO */
#define HW_NAME		"DLFE"
#define dlfe_info(fmt, args...)		info_hw("[%s]" fmt "\n", HW_NAME, ##args)
#define dlfe_warn(fmt, args...)		warn_hw("[%s]" fmt, HW_NAME, ##args)
#define dlfe_err(fmt, args...)		err_hw("[%s]" fmt, HW_NAME, ##args)

/* Tuning Parameters */
#define HBLANK_CYCLE	0x20 /* Reset value */

enum dlfe_cinfifo_new_frame_in {
	NEW_FRAME_ASAP = 0,
	NEW_FRAME_VVALID_RISE = 1,
};

struct dlfe_int_mask {
	ulong bitmask;
	bool partial;
};

static struct dlfe_int_mask int_mask[INT_TYPE_NUM] = {
	[INT_FRAME_START]	= {BIT_MASK(INTR0_DLFE_FRAME_START_INT),	false},
	[INT_FRAME_END]		= {BIT_MASK(INTR0_DLFE_FRAME_END_INT),		false},
	[INT_COREX_END]		= {BIT_MASK(INTR0_DLFE_COREX_END_INT_0),	false},
	[INT_SETTING_DONE]	= {BIT_MASK(INTR0_DLFE_SETTING_DONE_INT),	false},
	[INT_ERR0]		= {INT0_ERR_MASK, 				true},
	[INT_ERR1]		= {INT1_ERR_MASK,				true},
};

/* Debugging Methodologies */
#define USE_DLFE_IO_MOCK	0
#if IS_ENABLED(USE_DLFE_IO_MOCK)
static int dlfe_reg_read_mock(void *ctx, unsigned int reg, unsigned int *val)
{
	dlfe_info("reg_read: 0x%04x", reg);

	return 0;
}

static int dlfe_reg_write_mock(void *ctx, unsigned int reg, unsigned int val)
{
	dlfe_info("reg_write: 0x%04x 0x%08x", reg, val);

	return 0;
}
#else
#define dlfe_reg_read_mock	NULL
#define dlfe_reg_write_mock	NULL
#endif

/* Internal functions */
static void _dlfe_hw_cmdq_lock(struct pablo_mmio *pmio, bool lock)
{
	u32 val = 0;

	if (lock) {
		val = SET_CR_V(pmio, val, DLFE_F_CMDQ_POP_LOCK, 1);
		val = SET_CR_V(pmio, val, DLFE_F_CMDQ_RELOAD_LOCK, 1);
	}

	SET_CR(pmio, DLFE_R_CMDQ_LOCK, val);
}

static int _dlfe_hw_s_reset(struct pablo_mmio *pmio)
{
	u32 retry = 0;

	SET_CR(pmio, DLFE_R_SW_RESET, 1);

	/* Wait to finish reset processing. */
	while (GET_CR(pmio, DLFE_R_SW_RESET)) {
		if (retry++ > DLFE_TRY_COUNT) {
			dlfe_err("Failed to finish reset processing. %uus", retry);
			return -ETIME;
		}
		udelay(1);
	}

	return 0;
}

static inline void _dlfe_hw_s_clk(struct pablo_mmio *pmio, bool en)
{
	SET_CR(pmio, DLFE_R_IP_PROCESSING, (u32)en);
}

static void _dlfe_hw_s_otf(struct pablo_mmio *pmio, bool en)
{
	if (en) {
		SET_CR(pmio, DLFE_R_YUV_DLFECINFIFOCURR_ENABLE, 1);
		SET_CR(pmio, DLFE_R_YUV_DLFECINFIFOPREV_ENABLE, 1);
		SET_CR(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_ENABLE, 1);
		SET_CR_F(pmio, DLFE_R_YUV_DLFECINFIFOCURR_CONFIG,
				DLFE_F_YUV_DLFECINFIFOCURR_STALL_BEFORE_FRAME_START_EN, 1);
		SET_CR_F(pmio, DLFE_R_YUV_DLFECINFIFOPREV_CONFIG,
				DLFE_F_YUV_DLFECINFIFOPREV_STALL_BEFORE_FRAME_START_EN, 1);
		SET_CR_F(pmio, DLFE_R_CMDQ_VHD_CONTROL,
			DLFE_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE, 1);
	} else {
		SET_CR(pmio, DLFE_R_YUV_DLFECINFIFOCURR_ENABLE, 0);
		SET_CR(pmio, DLFE_R_YUV_DLFECINFIFOPREV_ENABLE, 0);
		SET_CR(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_ENABLE, 0);
	}
}

static void _dlfe_hw_s_int(struct pablo_mmio *pmio)
{
	SET_CR(pmio, DLFE_R_INT_REQ_INT0_ENABLE, INT0_EN_MASK);
	SET_CR(pmio, DLFE_R_INT_REQ_INT1_ENABLE, INT1_EN_MASK);

	/* TODO: Need to figure out the usage of below interrupts. */
#if 0
	SET_CR(pmio, DLFE_R_CMDQ_INT_ENABLE, 0);
	SET_CR(pmio, DLFE_R_COREX_INT_ENABLE, 0);
#endif
}

static void _dlfe_hw_s_cmdq(struct pablo_mmio *pmio)
{
	u32 mode = CTRL_MODE_APB_DIRECT;

	SET_CR_F(pmio, DLFE_R_CMDQ_QUE_CMD_L, DLFE_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, DLFE_INT_GRP_EN_MASK);
	SET_CR_F(pmio, DLFE_R_CMDQ_QUE_CMD_M, DLFE_F_CMDQ_QUE_CMD_SETTING_MODE, mode);
	SET_CR(pmio, DLFE_R_CMDQ_ENABLE, 1);
}

static void _dlfe_hw_s_chain(struct pablo_mmio *pmio, struct dlfe_param_set *p_set)
{
	u32 in_w, in_h, ot_w, ot_h;
	u32 val;

	ot_w = in_w = p_set->curr_in.width;
	ot_h = in_h = p_set->curr_in.height;

	SET_CR(pmio, DLFE_R_IP_USE_CINFIFO_NEW_FRAME_IN, NEW_FRAME_ASAP);

	val = 0;
	val = SET_CR_V(pmio, val, DLFE_F_SOURCE_IMAGE_WIDTH, in_w);
	val = SET_CR_V(pmio, val, DLFE_F_SOURCE_IMAGE_HEIGHT, in_h);
	SET_CR(pmio, DLFE_R_SOURCE_IMAGE_DIMENSION, val);

	val = 0;
	val = SET_CR_V(pmio, val, DLFE_F_DESTINATION_IMAGE_WIDTH, ot_w);
	val = SET_CR_V(pmio, val, DLFE_F_DESTINATION_IMAGE_HEIGHT, ot_h);
	SET_CR(pmio, DLFE_R_DESTINATION_IMAGE_DIMENSION, val);

	SET_CR(pmio, DLFE_R_H_BLANK, HBLANK_CYCLE);
}

static void _dlfe_hw_s_crc(struct pablo_mmio *pmio, u8 seed)
{
	SET_CR_F(pmio, DLFE_R_YUV_DLFECINFIFOCURR_STREAM_CRC,
			DLFE_F_YUV_DLFECINFIFOCURR_CRC_SEED, seed);
	SET_CR_F(pmio, DLFE_R_YUV_DLFECINFIFOPREV_STREAM_CRC,
			DLFE_F_YUV_DLFECINFIFOPREV_CRC_SEED, seed);
	SET_CR_F(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_STREAM_CRC,
			DLFE_F_YUV_DLFECOUTFIFOWGT_CRC_SEED, seed);
}

static void _dlfe_hw_s_cin(struct pablo_mmio *pmio, struct dlfe_param_set *p_set)
{
	u32 cin0_en, cin1_en, val;

	cin0_en = p_set->curr_in.cmd;
	cin1_en = p_set->prev_in.cmd;

	val = GET_CR(pmio, DLFE_R_IP_USE_OTF_PATH_01);
	val = SET_CR_V(pmio, val, DLFE_F_IP_USE_OTF_IN_FOR_PATH_0, cin0_en);
	val = SET_CR_V(pmio, val, DLFE_F_IP_USE_OTF_IN_FOR_PATH_1, cin1_en);
	SET_CR(pmio, DLFE_R_IP_USE_OTF_PATH_01, val);
}

static void _dlfe_hw_s_cout(struct pablo_mmio *pmio, struct dlfe_param_set *p_set)
{
	u32 cout0_en;

	cout0_en = p_set->wgt_out.cmd;

	SET_CR_F(pmio, DLFE_R_IP_USE_OTF_PATH_01,
			DLFE_F_IP_USE_OTF_OUT_FOR_PATH_0, cout0_en);
}

static void _dlfe_hw_s_bypass(struct pablo_mmio *pmio, struct dlfe_param_set *p_set)
{
	/* There is no actual bypass control scenario by user. */
	SET_CR(pmio, DLFE_R_BYPASS, 0);
	SET_CR(pmio, DLFE_R_BYPASS_WEIGHT, 0);
}

static void _dlfe_hw_s_uv_path(struct pablo_mmio *pmio, struct dlfe_param_set *p_set)
{
	u32 uv0_en, uv1_en, val = 0;

	uv0_en = p_set->curr_in.cmd;
	uv1_en = p_set->prev_in.cmd;

	val = SET_CR_V(pmio, val, DLFE_F_ENABLE_PATH_0, uv0_en);
	val = SET_CR_V(pmio, val, DLFE_F_ENABLE_PATH_1, uv1_en);
	SET_CR(pmio, DLFE_R_ENABLE_PATH, val);
}

static inline void _dlfe_hw_q_cmdq(struct pablo_mmio *pmio)
{
	SET_CR(pmio, DLFE_R_CMDQ_ADD_TO_QUEUE_0, 1);
}

static inline u32 _dlfe_hw_g_int_state(struct pablo_mmio *pmio,
		u32 int_src, u32 int_clr, bool clear)
{
	u32 int_state;

	int_state = GET_CR(pmio, int_src);

	if (clear)
		SET_CR(pmio, int_clr, int_state);

	return int_state;
}

static inline u32 _dlfe_hw_g_idleness_state(struct pablo_mmio *pmio)
{
	return GET_CR_F(pmio, DLFE_R_IDLENESS_STATUS, DLFE_F_IDLENESS_STATUS);
}

static int _dlfe_hw_wait_idleness(struct pablo_mmio *pmio)
{
	u32 retry = 0;

	while (!_dlfe_hw_g_idleness_state(pmio)) {
		if (retry++ > DLFE_TRY_COUNT) {
			dlfe_err("Failed to wait IDLENESS. retry %u", retry);
			return -ETIME;
		}

		usleep_range(3, 4);
	}

	return 0;
}

static void _dlfe_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	/* TOP */
	dlfe_info("= TOP ================================");
	dlfe_info("IDLENESS:\t0x%08x", GET_CR(pmio, DLFE_R_IDLENESS_STATUS));
	dlfe_info("BUSY:\t0x%08x", GET_CR(pmio, DLFE_R_IP_BUSY_MONITOR_0));
	dlfe_info("STALL_OUT:\t0x%08x", GET_CR(pmio, DLFE_R_IP_STALL_OUT_STATUS_0));

	dlfe_info("INT0:\t0x%08x/ HIST: 0x%08x", GET_CR(pmio, DLFE_R_INT_REQ_INT0),
			GET_CR(pmio, DLFE_R_INT_HIST_CURINT0));
	dlfe_info("INT1:\t0x%08x/ HIST: 0x%08x", GET_CR(pmio, DLFE_R_INT_REQ_INT1),
			GET_CR(pmio, DLFE_R_INT_HIST_CURINT1));

	/* CTRL */
	dlfe_info("= CTRL ===============================");
	dlfe_info("CMDQ_DBG:\t0x%08x", GET_CR(pmio, DLFE_R_CMDQ_DEBUG_STATUS));
	dlfe_info("CMDQ_INT:\t0x%08x", GET_CR(pmio, DLFE_R_CMDQ_INT));
	dlfe_info("COREX_INT:\t0x%08x", GET_CR(pmio, DLFE_R_COREX_INT));

	/* OTF_PATH */
	dlfe_info("= OTF ================================");
	dlfe_info("OTF_PATH:\t0x%08x", GET_CR(pmio, DLFE_R_IP_USE_OTF_PATH_01));

	/* CINFIFO0 */
	dlfe_info("= CIN0 ===============================");
	dlfe_info("CIN0_CFG:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOCURR_CONFIG));
	dlfe_info("CIN0_INT:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOCURR_INT));
	dlfe_info("CIN0_CNT:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOCURR_INPUT_CNT));
	dlfe_info("CIN0_STL:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOCURR_STALL_CNT));
	dlfe_info("CIN0_FUL:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOCURR_FIFO_FULLNESS));

	/* CINFIFO1 */
	dlfe_info("= CIN1 ===============================");
	dlfe_info("CIN1_CFG:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOPREV_CONFIG));
	dlfe_info("CIN1_INT:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOPREV_INT));
	dlfe_info("CIN1_CNT:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOPREV_INPUT_CNT));
	dlfe_info("CIN1_STL:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOPREV_INPUT_CNT));
	dlfe_info("CIN1_FUL:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECINFIFOPREV_FIFO_FULLNESS));

	/* COUTFIFO */
	dlfe_info("= COUT ===============================");
	dlfe_info("COUT_CFG:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_CONFIG));
	dlfe_info("COUT_INT:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_INT));
	dlfe_info("COUT_CNT:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_INPUT_CNT));
	dlfe_info("COUT_STL:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_STALL_CNT));
	dlfe_info("COUT_FUL:\t0x%08x", GET_CR(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_FIFO_FULLNESS));

	dlfe_info("======================================");
}

static int dlfe_hw_reset(struct pablo_mmio *pmio)
{
	u32 ret;

	_dlfe_hw_s_otf(pmio, false);

	_dlfe_hw_cmdq_lock(pmio, true);
	ret = _dlfe_hw_s_reset(pmio);
	_dlfe_hw_cmdq_lock(pmio, false);

	return ret;
}

static void dlfe_hw_init(struct pablo_mmio *pmio)
{
	_dlfe_hw_s_clk(pmio, true);
	_dlfe_hw_s_otf(pmio, true);
	_dlfe_hw_s_int(pmio);
	_dlfe_hw_s_cmdq(pmio);
}

static void dlfe_hw_s_core(struct pablo_mmio *pmio, struct dlfe_param_set *p_set)
{
	_dlfe_hw_s_chain(pmio, p_set);

	if (unlikely(p_set->crc_seed))
		_dlfe_hw_s_crc(pmio, (u8)p_set->crc_seed);
}

static void dlfe_hw_s_path(struct pablo_mmio *pmio, struct dlfe_param_set *p_set)
{
	_dlfe_hw_s_cin(pmio, p_set);
	_dlfe_hw_s_cout(pmio, p_set);
	_dlfe_hw_s_bypass(pmio, p_set);
	_dlfe_hw_s_uv_path(pmio, p_set);
}

static void dlfe_hw_q_cmdq(struct pablo_mmio *pmio)
{
	_dlfe_hw_q_cmdq(pmio);
}

static u32 dlfe_hw_g_int_state(struct pablo_mmio *pmio, u32 int_id)
{
	u32 int_src, int_clr;

	switch (int_id) {
	case 0:
		int_src = DLFE_R_INT_REQ_INT0;
		int_clr = DLFE_R_INT_REQ_INT0_CLEAR;
		break;
	case 1:
		int_src = DLFE_R_INT_REQ_INT1;
		int_clr = DLFE_R_INT_REQ_INT1_CLEAR;
		break;
	default:
		dlfe_err("Invalid interrupt id %d", int_id);
		return 0;
	}

	return _dlfe_hw_g_int_state(pmio, int_src, int_clr, true);
}

static int dlfe_hw_wait_idle(struct pablo_mmio *pmio)
{
	u32 idle, int0_state, int1_state;
	int ret;

	idle = _dlfe_hw_g_idleness_state(pmio);
	int0_state = _dlfe_hw_g_int_state(pmio, DLFE_R_INT_REQ_INT0, 0, false);
	int1_state = _dlfe_hw_g_int_state(pmio, DLFE_R_INT_REQ_INT1, 0, false);

	dlfe_info("Wait IDLENESS start. idleness 0x%x int0 0x%x int1 0x%x",
			idle, int0_state, int1_state);

	ret = _dlfe_hw_wait_idleness(pmio);

	idle = _dlfe_hw_g_idleness_state(pmio);
	int0_state = _dlfe_hw_g_int_state(pmio, DLFE_R_INT_REQ_INT0, 0, false);
	int1_state = _dlfe_hw_g_int_state(pmio, DLFE_R_INT_REQ_INT1, 0, false);

	dlfe_info("Wait IDLENESS done. idleness 0x%x int0 0x%x int1 0x%x",
			idle, int0_state, int1_state);

	return ret;
}

static void dlfe_hw_dump(struct pablo_mmio *pmio, u32 mode)
{

	switch (mode) {
	case DLFE_HW_DUMP_CR:
		dlfe_info("DUMP CR");
		is_hw_dump_regs(pmio_get_base(pmio), dlfe_regs, DLFE_REG_CNT);
		break;
	case DLFE_HW_DUMP_DBG_STATE:
		dlfe_info("DUMP DBG_STATE");
		_dlfe_hw_dump_dbg_state(pmio);
		break;
	default:
		dlfe_err("Not supported dump_mode %d", mode);
		break;
	}
}

static bool dlfe_hw_is_occurred(u32 status, ulong type)
{
	ulong i;
	u32 bitmask = 0, partial_mask = 0;
	bool occur = true;

	for_each_set_bit(i, &type, INT_TYPE_NUM) {
		if (int_mask[i].partial)
			partial_mask |= int_mask[i].bitmask;
		else
			bitmask |= int_mask[i].bitmask;
	}

	if (bitmask)
		occur = occur && ((status & bitmask) == bitmask);

	if (partial_mask)
		occur = occur && (status & partial_mask);

	return occur;
}

static const struct dlfe_hw_ops hw_ops = {
	.reset		= dlfe_hw_reset,
	.init		= dlfe_hw_init,
	.s_core		= dlfe_hw_s_core,
	.s_path		= dlfe_hw_s_path,
	.q_cmdq		= dlfe_hw_q_cmdq,
	.g_int_state	= dlfe_hw_g_int_state,
	.wait_idle	= dlfe_hw_wait_idle,
	.dump		= dlfe_hw_dump,
	.is_occurred	= dlfe_hw_is_occurred,
};

const struct dlfe_hw_ops *dlfe_hw_g_ops(void)
{
	return &hw_ops;
}
KUNIT_EXPORT_SYMBOL(dlfe_hw_g_ops);

void dlfe_hw_g_pmio_cfg(struct pmio_config *pcfg)
{
	pcfg->max_register = DLFE_R_WR_OUTSTANDING;
	pcfg->num_reg_defaults_raw = (pcfg->max_register / PMIO_REG_STRIDE) + 1;
	pcfg->phys_base = 0x27040000;

	pcfg->fields = dlfe_field_descs;
	pcfg->num_fields = ARRAY_SIZE(dlfe_field_descs);

	if (IS_ENABLED(USE_DLFE_IO_MOCK)) {
		pcfg->reg_read = dlfe_reg_read_mock;
		pcfg->reg_write = dlfe_reg_write_mock;
	}
}
KUNIT_EXPORT_SYMBOL(dlfe_hw_g_pmio_cfg);
