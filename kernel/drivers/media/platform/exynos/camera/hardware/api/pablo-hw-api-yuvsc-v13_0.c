// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * YUVSC HW control APIs
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw.h"
#include "pmio.h"
#include "is-hw-common-dma.h"
#include "sfr/pablo-sfr-yuvsc-v13_0.h"
#include "pablo-hw-api-yuvsc.h"

/* PMIO MACRO */
#define SET_CR(base, R, val)		PMIO_SET_R(base, R, val)
#define SET_CR_F(base, R, F, val)	PMIO_SET_F(base, R, F, val)
#define SET_CR_V(base, reg_val, F, val)	PMIO_SET_V(base, reg_val, F, val)

#define GET_CR(base, R)			PMIO_GET_R(base, R)
#define GET_CR_F(base, R, F)		PMIO_GET_F(base, R, F)

/* LOG MACRO */
#define HW_NAME		"YUVSC"
#define yuvsc_info(fmt, args...)	info_hw("[%s]" fmt "\n", HW_NAME, ##args)
#define yuvsc_dbg(level, fmt, args...)	dbg_hw(level, "[%s]" fmt "\n", HW_NAME, ##args)
#define yuvsc_warn(fmt, args...)	warn_hw("[%s]" fmt, HW_NAME, ##args)
#define yuvsc_err(fmt, args...)		err_hw("[%s]" fmt, HW_NAME, ##args)

/* CMDQ Interrupt group mask */
#define YUVSC_INT_GRP_EN_MASK                                                       \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |    \
	 BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |             \
	 BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |             \
	 BIT_MASK(PCC_INT_GRP_ENABLE_ALL))

/* Tuning Parameters */
#define VBLANK_CYCLE	0xA

enum yuvsc_cotf_in_id {
	YUVSC_COTF_IN,
};

enum yuvsc_cotf_out_id {
	YUVSC_COTF_OUT,
};

struct yuvsc_int_mask {
	ulong bitmask;
	bool partial;
};

/* Internal functions */
static void _yuvsc_hw_s_otf(struct pablo_mmio *pmio, bool en)
{
	if (en) {
		SET_CR(pmio, YUVSC_R_RGB_CINFIFO_ENABLE, 1);
		SET_CR(pmio, YUVSC_R_YUV_COUTFIFO_ENABLE, 1);
		SET_CR_F(pmio, YUVSC_R_RGB_CINFIFO_CONFIG,
				YUVSC_F_RGB_CINFIFO_STALL_BEFORE_FRAME_START_EN, 1);
		SET_CR_F(pmio, YUVSC_R_YUV_COUTFIFO_INTERVAL_VBLANK,
			 YUVSC_F_YUV_COUTFIFO_INTERVAL_VBLANK, VBLANK_CYCLE);
	} else {
		SET_CR(pmio, YUVSC_R_RGB_CINFIFO_ENABLE, 0);
		SET_CR(pmio, YUVSC_R_YUV_COUTFIFO_ENABLE, 0);
	}
}

static void yuvsc_hw_reset(struct pablo_mmio *pmio)
{
	_yuvsc_hw_s_otf(pmio, false);
}

static void _yuvsc_hw_s_cloader(struct pablo_mmio *pmio)
{
	SET_CR_F(pmio, YUVSC_R_STAT_RDMACL_EN, YUVSC_F_STAT_RDMACL_EN, 1);
}

static void yuvsc_hw_init(struct pablo_mmio *pmio)
{
	_yuvsc_hw_s_otf(pmio, true);
	_yuvsc_hw_s_cloader(pmio);
}

static void _yuvsc_hw_s_scaler(struct pablo_mmio *pmio, struct is_crop *in, struct is_crop *out)
{
	struct { u32 h; u32 v; } zoom_ratio = { GET_ZOOM_RATIO(10, 10), GET_ZOOM_RATIO(10, 10) };
	enum scaler_state {
		SCALER_ENABLE,
		SCALER_BYPASS,
	};
	u32 target_ratio, val;

	if ((out->w > in->w) || (out->h > in->h)) {
		u32 tem_w = out->w, tem_h = out->h;

		if (tem_w > in->w)
			tem_w = in->w;

		if (tem_h > in->h)
			tem_h = in->h;

		yuvsc_err(
			"%s: YUVSC do not support scale up. input(%dx%d), output(%dx%d -> %dx%d)\n",
			__func__, in->w, in->h, out->w, out->h, tem_w, tem_h);

		out->w = tem_w;
		out->h = tem_h;
	}

	if (in->w == out->w || in->h == out->h) {
		SET_CR(pmio, YUVSC_R_YUV_SCALER_CTRL0, SCALER_BYPASS);
		return;
	}

	SET_CR(pmio, YUVSC_R_YUV_SCALER_CTRL0, SCALER_ENABLE);

	if (out->w < in->w)
		zoom_ratio.h = GET_ZOOM_RATIO(in->w, out->w);

	if (out->h < in->h)
		zoom_ratio.v = GET_ZOOM_RATIO(in->h, out->h);

	target_ratio = min(zoom_ratio.h, zoom_ratio.v);

	yuvsc_dbg(4, "%s: h_ratio(0x%x), v_ratio(0x%x)", __func__, zoom_ratio.h, zoom_ratio.v);

	val = 0;
	val = SET_CR_V(pmio, val, YUVSC_F_YUV_SCALER_DST_HSIZE, out->w);
	val = SET_CR_V(pmio, val, YUVSC_F_YUV_SCALER_DST_VSIZE, out->h);
	SET_CR(pmio, YUVSC_R_YUV_SCALER_DST_SIZE, val);

	SET_CR(pmio, YUVSC_R_YUV_SCALER_H_RATIO, target_ratio);
	SET_CR(pmio, YUVSC_R_YUV_SCALER_V_RATIO, target_ratio);

	yuvsc_dbg(4, "%s: input(%dx%d) -> output(%dx%d)", __func__,
			in->w, in->h, out->w, out->h);
}

static void _yuvsc_hw_s_chain(struct pablo_mmio *pmio, struct yuvsc_param_set *param_set)
{
	struct is_crop in = { 0, 0, param_set->otf_input.width, param_set->otf_input.height };
	struct is_crop out = { 0, 0, param_set->otf_output.width, param_set->otf_output.height };
	u32 val;

	val = 0;
	val = SET_CR_V(pmio, val, YUVSC_F_CHAIN_SRC_IMG_WIDTH, in.w);
	val = SET_CR_V(pmio, val, YUVSC_F_CHAIN_SRC_IMG_HEIGHT, in.h);
	SET_CR(pmio, YUVSC_R_CHAIN_SRC_IMG_SIZE, val);

	_yuvsc_hw_s_scaler(pmio, &in, &out);

	val = 0;
	val = SET_CR_V(pmio, val, YUVSC_F_CHAIN_SCALED_IMG_WIDTH, out.w);
	val = SET_CR_V(pmio, val, YUVSC_F_CHAIN_SCALED_IMG_HEIGHT, out.h);
	SET_CR(pmio, YUVSC_R_CHAIN_SCALED_IMG_SIZE, val);

	val = 0;
	val = SET_CR_V(pmio, val, YUVSC_F_CHAIN_DST_IMG_WIDTH, out.w);
	val = SET_CR_V(pmio, val, YUVSC_F_CHAIN_DST_IMG_HEIGHT, out.h);
	SET_CR(pmio, YUVSC_R_CHAIN_DST_IMG_SIZE, val);
}

static void _yuvsc_hw_s_crc(struct pablo_mmio *pmio, u8 seed)
{
	SET_CR_F(pmio, YUVSC_R_RGB_CINFIFO_STREAM_CRC,
			YUVSC_F_RGB_CINFIFO_CRC_SEED, seed);
	SET_CR_F(pmio, YUVSC_R_YUV_COUTFIFO_STREAM_CRC,
			YUVSC_F_YUV_COUTFIFO_CRC_SEED, seed);
	SET_CR_F(pmio, YUVSC_R_RGB_RGBTOYUV_STREAM_CRC,
			YUVSC_F_RGB_RGBTOYUV_CRC_SEED, seed);
	SET_CR_F(pmio, YUVSC_R_YUV_YUV444TO422_STREAM_CRC,
			YUVSC_F_YUV_YUV444TO422_CRC_SEED, seed);
	SET_CR_F(pmio, YUVSC_R_YUV_SCALER_STREAM_CRC,
			YUVSC_F_YUV_SCALER_CRC_SEED, seed);
	SET_CR_F(pmio, YUVSC_R_RGB_GAMMA_STREAM_CRC,
			YUVSC_F_RGB_GAMMA_CRC_SEED, seed);
	SET_CR_F(pmio, YUVSC_R_YUV_CROP_STREAM_CRC,
			YUVSC_F_YUV_CROP_CRC_SEED, seed);
}

static void yuvsc_hw_s_core(struct pablo_mmio *pmio, struct yuvsc_param_set *param_set)
{
	u32 seed;

	_yuvsc_hw_s_chain(pmio, param_set);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		_yuvsc_hw_s_crc(pmio, seed);
}

static void _yuvsc_hw_s_cin(struct pablo_mmio *pmio, struct yuvsc_param_set *param_set,
		u32 *cotf_en)
{
	if (param_set->otf_input.cmd)
		*cotf_en |= BIT_MASK(YUVSC_COTF_IN);
}

static void _yuvsc_hw_s_cout(struct pablo_mmio *pmio, struct yuvsc_param_set *param_set,
		u32 *cotf_en)
{
	if (param_set->otf_output.cmd)
		*cotf_en |= BIT_MASK(YUVSC_COTF_OUT);
}

static void _yuvsc_hw_s_bypass(struct pablo_mmio *pmio, struct yuvsc_param_set *param_set)
{
	SET_CR(pmio, YUVSC_R_RGB_CCM33_BYPASS, 1);
	SET_CR(pmio, YUVSC_R_RGB_RGBTOYUV_BYPASS, 1);
	SET_CR(pmio, YUVSC_R_YUV_YUV444TO422_ISP_BYPASS, 1);
	SET_CR(pmio, YUVSC_R_RGB_GAMMA_BYPASS, 1);
	SET_CR(pmio, YUVSC_R_YUV_CROP_BYPASS, 1);
}

static void yuvsc_hw_s_path(struct pablo_mmio *pmio, struct yuvsc_param_set *param_set,
			   struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	_yuvsc_hw_s_cin(pmio, param_set, &frame_cfg->cotf_in_en);
	_yuvsc_hw_s_cout(pmio, param_set, &frame_cfg->cotf_out_en);
	if (!IS_ENABLED(IRTA_CALL))
		_yuvsc_hw_s_bypass(pmio, param_set);
}

static void yuvsc_hw_g_int_en(u32 *int_en)
{
	int_en[PCC_INT_0] = INT0_EN_MASK;
	int_en[PCC_INT_1] = INT1_EN_MASK;
	/* Not used */
	int_en[PCC_CMDQ_INT] = 0;
	int_en[PCC_COREX_INT] = 0;
}

static u32 yuvsc_hw_g_int_grp_en(void)
{
	return YUVSC_INT_GRP_EN_MASK;
}

static inline u32 _yuvsc_hw_g_idleness_state(struct pablo_mmio *pmio)
{
	return GET_CR_F(pmio, YUVSC_R_IDLENESS_STATUS, YUVSC_F_IDLENESS_STATUS);
}

static int _yuvsc_hw_wait_idleness(struct pablo_mmio *pmio)
{
	u32 retry = 0;

	while (!_yuvsc_hw_g_idleness_state(pmio)) {
		if (retry++ > YUVSC_TRY_COUNT) {
			yuvsc_err("Failed to wait IDLENESS. retry %u", retry);
			return -ETIME;
		}

		usleep_range(3, 4);
	}

	return 0;
}

static inline u32 _yuvsc_hw_g_int_state(struct pablo_mmio *pmio,
		u32 int_src, u32 int_clr, bool clear)
{
	u32 int_state;

	int_state = GET_CR(pmio, int_src);

	if (clear)
		SET_CR(pmio, int_clr, int_state);

	return int_state;
}

static int yuvsc_hw_wait_idle(struct pablo_mmio *pmio)
{
	u32 idle, int0_state, int1_state;
	int ret;

	idle = _yuvsc_hw_g_idleness_state(pmio);
	int0_state = _yuvsc_hw_g_int_state(pmio, YUVSC_R_INT_REQ_INT0, 0, false);
	int1_state = _yuvsc_hw_g_int_state(pmio, YUVSC_R_INT_REQ_INT1, 0, false);

	yuvsc_info("Wait IDLENESS start. idleness 0x%x int0 0x%x int1 0x%x",
			idle, int0_state, int1_state);

	ret = _yuvsc_hw_wait_idleness(pmio);

	idle = _yuvsc_hw_g_idleness_state(pmio);
	int0_state = _yuvsc_hw_g_int_state(pmio, YUVSC_R_INT_REQ_INT0, 0, false);
	int1_state = _yuvsc_hw_g_int_state(pmio, YUVSC_R_INT_REQ_INT1, 0, false);

	yuvsc_info("Wait IDLENESS done. idleness 0x%x int0 0x%x int1 0x%x",
			idle, int0_state, int1_state);

	return ret;
}

static const struct is_reg yuvsc_dbg_cr[] = {
	/* The order of DBG_CR should match with the DBG_CR parser. */
	/* Chain Size */
	{ 0x0200, "CHAIN_SRC_IMG_SIZE" },
	{ 0x0204, "CHAIN_SCALED_IMG_SIZE" },
	{ 0x0208, "CHAIN_DST_IMG_SIZE" },
	/* CINFIFO Status */
	{ 0x1000, "RGB_CINFIFO_ENABLE" },
	{ 0x1014, "RGB_CINFIFO_STATUS" },
	{ 0x1018, "RGB_CINFIFO_INPUT_CNT" },
	{ 0x101c, "RGB_CINFIFO_STALL_CNT" },
	{ 0x1020, "RGB_CINFIFO_FIFO_FULLNESS" },
	{ 0x1040, "RGB_CINFIFO_INT" },
	/* COUTFIFO Status */
	{ 0x1200, "YUV_COUTFIFO_ENABLE" },
	{ 0x1214, "YUV_COUTFIFO_STATUS" },
	{ 0x1218, "YUV_COUTFIFO_INPUT_CNT" },
	{ 0x121c, "YUV_COUTFIFO_STALL_CNT" },
	{ 0x1220, "YUV_COUTFIFO_FIFO_FULLNESS" },
	{ 0x1240, "YUV_COUTFIFO_INT" },
};

static void _yuvsc_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	void *ctx;
	const struct is_reg *cr;
	u32 i, val;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;
	pmio->reg_read(ctx, YUVSC_R_IP_VERSION, &val);

	is_dbg("[HW:%s] v%02u.%02u.%02u ======================================\n", pmio->name,
		(val >> 24) & 0xff, (val >> 16) & 0xff, val & 0xffff);
	for (i = 0; i < ARRAY_SIZE(yuvsc_dbg_cr); i++) {
		cr = &yuvsc_dbg_cr[i];

		pmio->reg_read(ctx, cr->sfr_offset, &val);
		is_dbg("[HW:%s]%40s %08x\n", pmio->name, cr->reg_name, val);
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);
}

static void yuvsc_hw_dump(struct pablo_mmio *pmio, u32 mode)
{
	switch (mode) {
	case HW_DUMP_CR:
		yuvsc_info("%s:DUMP CR", __FILENAME__);
		is_hw_dump_regs(pmio_get_base(pmio), yuvsc_regs, YUVSC_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		yuvsc_info("%s:DUMP DBG_STATE", __FILENAME__);
		_yuvsc_hw_dump_dbg_state(pmio);
		break;
	default:
		yuvsc_err("%s:Not supported dump_mode %d", __FILENAME__, mode);
		break;
	}
}

static struct yuvsc_int_mask int_mask[INT_TYPE_NUM] = {
	[INT_FRAME_START]	= {BIT_MASK(INTR0_YUVSC_FRAME_START_INT),	false},
	[INT_FRAME_END]		= {BIT_MASK(INTR0_YUVSC_FRAME_END_INT),		false},
	[INT_COREX_END]		= {BIT_MASK(INTR0_YUVSC_COREX_END_INT_0),	false},
	[INT_SETTING_DONE]	= {BIT_MASK(INTR0_YUVSC_SETTING_DONE_INT),	false},
	[INT_ERR0]		= {INT0_ERR_MASK, 				true},
	[INT_ERR1]		= {INT1_ERR_MASK,				true},
};

static bool yuvsc_hw_is_occurred(u32 status, ulong type)
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

static void yuvsc_hw_s_strgen(struct pablo_mmio *pmio)
{
	SET_CR_F(pmio, YUVSC_R_RGB_CINFIFO_CONFIG, YUVSC_F_RGB_CINFIFO_STRGEN_MODE_EN, 1);
	SET_CR_F(pmio, YUVSC_R_RGB_CINFIFO_CONFIG, YUVSC_F_RGB_CINFIFO_STRGEN_MODE_DATA_TYPE, 1);
	SET_CR_F(pmio, YUVSC_R_RGB_CINFIFO_CONFIG, YUVSC_F_RGB_CINFIFO_STRGEN_MODE_DATA, 255);
	SET_CR(pmio, YUVSC_R_IP_USE_CINFIFO_NEW_FRAME_IN, PCC_ASAP);
}

static const struct yuvsc_hw_ops hw_ops = {
	.reset = yuvsc_hw_reset,
	.init = yuvsc_hw_init,
	.s_core = yuvsc_hw_s_core,
	.s_path = yuvsc_hw_s_path,
	.g_int_en = yuvsc_hw_g_int_en,
	.g_int_grp_en = yuvsc_hw_g_int_grp_en,
	.wait_idle = yuvsc_hw_wait_idle,
	.dump = yuvsc_hw_dump,
	.is_occurred = yuvsc_hw_is_occurred,
	.s_strgen = yuvsc_hw_s_strgen,
};

const struct yuvsc_hw_ops *yuvsc_hw_g_ops(void)
{
	return &hw_ops;
}
KUNIT_EXPORT_SYMBOL(yuvsc_hw_g_ops);

void yuvsc_hw_g_pmio_cfg(struct pmio_config *pcfg)
{
	pcfg->volatile_table = &yuvsc_volatile_ranges_table;

	pcfg->max_register = YUVSC_R_YUV_CROP_STREAM_CRC;
	pcfg->num_reg_defaults_raw = (pcfg->max_register / PMIO_REG_STRIDE) + 1;
	pcfg->dma_addr_shift = LSB_BIT;

	pcfg->fields = yuvsc_field_descs;
	pcfg->num_fields = ARRAY_SIZE(yuvsc_field_descs);
}
KUNIT_EXPORT_SYMBOL(yuvsc_hw_g_pmio_cfg);
