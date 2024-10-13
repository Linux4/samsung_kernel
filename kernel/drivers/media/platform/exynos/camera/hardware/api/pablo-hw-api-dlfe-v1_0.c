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
#include "is-hw-common-dma.h"
#include "pablo-hw-api-common-ctrl.h"
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

/* CMDQ Interrupt group mask */
#define DLFE_INT_GRP_EN_MASK                                                                       \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
	 BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                        \
	 BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                        \
	 BIT_MASK(PCC_INT_GRP_ENABLE_ALL))

/* Tuning Parameters */
#define HBLANK_CYCLE	0x20 /* Reset value */
#define VBLANK_CYCLE	0xA

/* HPL Core Buffer Base Mask */
#define HPLBUF_INSTR_BASE_MASK		0
#define HPLBUF_MODEL_BASE_MASK		(1 << 30)
#define HPLBUF_INPUT_BASE_MASK		(1 << 31)
#define HPLBUF_OUTPUT_BASE_MASK		0

enum dlfe_cotf_in_id {
	DLFE_COTF_IN_CUR,
	DLFE_COTF_IN_PREV,
};

enum dlfe_cotf_out_id {
	DLFE_COTF_OUT_WGT,
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
		SET_CR_F(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_INTERVAL_VBLANK,
			 DLFE_F_YUV_DLFECOUTFIFOWGT_INTERVAL_VBLANK, VBLANK_CYCLE);
	} else {
		SET_CR(pmio, DLFE_R_YUV_DLFECINFIFOCURR_ENABLE, 0);
		SET_CR(pmio, DLFE_R_YUV_DLFECINFIFOPREV_ENABLE, 0);
		SET_CR(pmio, DLFE_R_YUV_DLFECOUTFIFOWGT_ENABLE, 0);
	}
}

static void _dlfe_hw_s_model_buf(struct pablo_mmio *pmio)
{
	SET_CR(pmio, DLFE_R_BUFFER_INSTRUCTION_BASE_ADDR, HPLBUF_INSTR_BASE_MASK);
	SET_CR(pmio, DLFE_R_BUFFER_MODEL_BASE_ADDR, HPLBUF_MODEL_BASE_MASK);
	SET_CR(pmio, DLFE_R_BUFFER_INPUT_BASE_ADDR, HPLBUF_INPUT_BASE_MASK);
	SET_CR(pmio, DLFE_R_BUFFER_OUTPUT_BASE_ADDR, HPLBUF_OUTPUT_BASE_MASK); /* No effect */
}

static void _dlfe_hw_s_cloader(struct pablo_mmio *pmio)
{
	SET_CR_F(pmio, DLFE_R_STAT_RDMACL_EN, DLFE_F_STAT_RDMACL_EN, 1);
}

static void _dlfe_hw_s_chain(struct pablo_mmio *pmio, struct dlfe_param_set *p_set)
{
	u32 in_w, in_h, ot_w, ot_h;
	u32 val;

	ot_w = in_w = p_set->curr_in.width;
	ot_h = in_h = p_set->curr_in.height;

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

static void _dlfe_hw_s_cin(struct pablo_mmio *pmio, struct dlfe_param_set *p_set, u32 *cotf_en)
{

	if (p_set->curr_in.cmd)
		*cotf_en |= BIT_MASK(DLFE_COTF_IN_CUR);

	if (p_set->prev_in.cmd)
		*cotf_en |= BIT_MASK(DLFE_COTF_IN_PREV);
}

static void _dlfe_hw_s_cout(struct pablo_mmio *pmio, struct dlfe_param_set *p_set, u32 *cotf_en)
{
	if (p_set->wgt_out.cmd)
		*cotf_en |= BIT_MASK(DLFE_COTF_OUT_WGT);
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

static const struct is_reg dlfe_dbg_cr[] = {
	/* The order of DBG_CR should match with the DBG_CR parser. */
	/* Chain Size */
	{ 0x0200, "SOURCE_IMAGE_DIMENSION" },
	{ 0x0204, "DESTINATION_IMAGE_DIMENSION" },
	/* Chain Path */
	{ 0x0208, "BYPASS" },
	{ 0x020c, "BYPASS_WEIGHT" },
	{ 0x0214, "ENABLE_PATH" },
	/* CINFIFO 0 Status */
	{ 0x1000, "YUV_DLFECINFIFOCURR_ENABLE" },
	{ 0x1014, "YUV_DLFECINFIFOCURR_STATUS" },
	{ 0x1018, "YUV_DLFECINFIFOCURR_INPUT_CNT" },
	{ 0x101c, "YUV_DLFECINFIFOCURR_STALL_CNT" },
	{ 0x1020, "YUV_DLFECINFIFOCURR_FIFO_FULLNESS" },
	{ 0x1040, "YUV_DLFECINFIFOCURR_INT" },
	/* CINFIFO 1 Status */
	{ 0x1100, "YUV_DLFECINFIFOPREV_ENABLE" },
	{ 0x1114, "YUV_DLFECINFIFOPREV_STATUS" },
	{ 0x1118, "YUV_DLFECINFIFOPREV_INPUT_CNT" },
	{ 0x111c, "YUV_DLFECINFIFOPREV_STALL_CNT" },
	{ 0x1120, "YUV_DLFECINFIFOPREV_FIFO_FULLNESS" },
	{ 0x1140, "YUV_DLFECINFIFOPREV_INT" },
	/* COUTFIFO 0 Status */
	{ 0x1200, "YUV_DLFECOUTFIFOWGT_ENABLE" },
	{ 0x1214, "YUV_DLFECOUTFIFOWGT_STATUS" },
	{ 0x1218, "YUV_DLFECOUTFIFOWGT_INPUT_CNT" },
	{ 0x121c, "YUV_DLFECOUTFIFOWGT_STALL_CNT" },
	{ 0x1220, "YUV_DLFECOUTFIFOWGT_FIFO_FULLNESS" },
	{ 0x1240, "YUV_DLFECOUTFIFOWGT_INT" },
};

static void _dlfe_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	void *ctx;
	const struct is_reg *cr;
	u32 i, val;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;
	pmio->reg_read(ctx, DLFE_R_IP_VERSION, &val);

	is_dbg("[HW:%s] v%02u.%02u.%02u ======================================\n", pmio->name,
		(val >> 24) & 0xff, (val >> 16) & 0xff, val & 0xffff);
	for (i = 0; i < ARRAY_SIZE(dlfe_dbg_cr); i++) {
		cr = &dlfe_dbg_cr[i];

		pmio->reg_read(ctx, cr->sfr_offset, &val);
		is_dbg("[HW:%s]%40s %08x\n", pmio->name, cr->reg_name, val);
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);
}

static void dlfe_hw_reset(struct pablo_mmio *pmio)
{
	_dlfe_hw_s_otf(pmio, false);
}

static void dlfe_hw_init(struct pablo_mmio *pmio)
{
	_dlfe_hw_s_otf(pmio, true);
	_dlfe_hw_s_model_buf(pmio);
	_dlfe_hw_s_cloader(pmio);
}

static void dlfe_hw_s_core(struct pablo_mmio *pmio, struct dlfe_param_set *p_set)
{
	_dlfe_hw_s_chain(pmio, p_set);

	if (unlikely(p_set->crc_seed))
		_dlfe_hw_s_crc(pmio, (u8)p_set->crc_seed);
}

static void dlfe_hw_s_path(struct pablo_mmio *pmio, struct dlfe_param_set *p_set,
			   struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	_dlfe_hw_s_cin(pmio, p_set, &frame_cfg->cotf_in_en);
	_dlfe_hw_s_cout(pmio, p_set, &frame_cfg->cotf_out_en);
	_dlfe_hw_s_bypass(pmio, p_set);
	_dlfe_hw_s_uv_path(pmio, p_set);
}

static void dlfe_hw_g_int_en(u32 *int_en)
{
	int_en[PCC_INT_0] = INT0_EN_MASK;
	int_en[PCC_INT_1] = INT1_EN_MASK;
	/* Not used */
	int_en[PCC_CMDQ_INT] = 0;
	int_en[PCC_COREX_INT] = 0;
}

static u32 dlfe_hw_g_int_grp_en(void)
{
	return DLFE_INT_GRP_EN_MASK;
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
	case HW_DUMP_CR:
		dlfe_info("%s:DUMP CR", __FILENAME__);
		is_hw_dump_regs(pmio_get_base(pmio), dlfe_regs, DLFE_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		dlfe_info("%s:DUMP DBG_STATE", __FILENAME__);
		_dlfe_hw_dump_dbg_state(pmio);
		break;
	default:
		dlfe_err("%s:Not supported dump_mode %d", __FILENAME__, mode);
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

static void dlfe_hw_s_strgen(struct pablo_mmio *pmio)
{
	SET_CR_F(pmio, DLFE_R_YUV_DLFECINFIFOCURR_CONFIG,
			DLFE_F_YUV_DLFECINFIFOCURR_STRGEN_MODE_EN, 1);
	SET_CR_F(pmio, DLFE_R_YUV_DLFECINFIFOPREV_CONFIG,
			DLFE_F_YUV_DLFECINFIFOPREV_STRGEN_MODE_EN, 1);
}

static const struct dlfe_hw_ops hw_ops = {
	.reset = dlfe_hw_reset,
	.init = dlfe_hw_init,
	.s_core = dlfe_hw_s_core,
	.s_path = dlfe_hw_s_path,
	.g_int_en = dlfe_hw_g_int_en,
	.g_int_grp_en = dlfe_hw_g_int_grp_en,
	.wait_idle = dlfe_hw_wait_idle,
	.dump = dlfe_hw_dump,
	.is_occurred = dlfe_hw_is_occurred,
	.s_strgen = dlfe_hw_s_strgen,
};

const struct dlfe_hw_ops *dlfe_hw_g_ops(void)
{
	return &hw_ops;
}
KUNIT_EXPORT_SYMBOL(dlfe_hw_g_ops);

void dlfe_hw_g_pmio_cfg(struct pmio_config *pcfg)
{
	pcfg->volatile_table = &dlfe_volatile_ranges_table;

	pcfg->max_register = DLFE_R_WR_OUTSTANDING;
	pcfg->num_reg_defaults_raw = (pcfg->max_register / PMIO_REG_STRIDE) + 1;
	pcfg->dma_addr_shift = LSB_BIT;

	pcfg->fields = dlfe_field_descs;
	pcfg->num_fields = ARRAY_SIZE(dlfe_field_descs);

	if (IS_ENABLED(USE_DLFE_IO_MOCK)) {
		pcfg->reg_read = dlfe_reg_read_mock;
		pcfg->reg_write = dlfe_reg_write_mock;
	}
}
KUNIT_EXPORT_SYMBOL(dlfe_hw_g_pmio_cfg);
