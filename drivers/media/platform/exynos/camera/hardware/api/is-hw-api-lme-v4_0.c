// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * lme HW control APIs
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include "is-hw-api-lme-v3_0.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"

#define	LME_USE_MMIO	0

#if IS_ENABLED(LME_USE_MMIO)
#include "sfr/is-sfr-lme-mmio-v4_0.h"
#else
#include "sfr/is-sfr-lme-v4_0.h"
#endif

#include "pablo-smc.h"

#if IS_ENABLED(LME_USE_MMIO)
#define LME_SET_F(base, R, F, val) \
	is_hw_set_field(base, &lme_regs[R], &lme_fields[F], val)
#define LME_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &lme_regs[R], &lme_fields[F], val)
#define LME_SET_R(base, R, val) \
	is_hw_set_reg(base, &lme_regs[R], val)
#define LME_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &lme_regs[R], val)
#define LME_SET_V(base, reg_val, F, val) \
	is_hw_set_field_value(reg_val, &lme_fields[F], val)

#define LME_GET_F(base, R, F) \
	is_hw_get_field(base, &lme_regs[R], &lme_fields[F])
#define LME_GET_R(base, R) \
	is_hw_get_reg(base, &lme_regs[R])
#define LME_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &lme_regs[R])
#define LME_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &lme_fields[F])

#else
#define LME_SET_F(base, R, F, val)		PMIO_SET_F(base, R, F, val)
#define LME_SET_F_COREX(base, R, F, val)	PMIO_SET_F_COREX(base, R, F, val)
#define LME_SET_R(base, R, val)			PMIO_SET_R(base, R, val)
#define LME_SET_V(base, reg_val, F, val)	PMIO_SET_V(base, reg_val, F, val)
#define LME_GET_F(base, R, F)			PMIO_GET_F(base, R, F)
#define LME_GET_R(base, R)			PMIO_GET_R(base, R)
#endif


unsigned int lme_hw_is_occurred(unsigned int status, enum lme_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR0_LME_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR0_LME_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR0_LME_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR0_LME_COREX_END_INT_1;
		break;
	case INTR_ERR:
		mask = LME_INT_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(lme_hw_is_occurred);

int lme_hw_s_reset(void __iomem *base)
{
	u32 reset_count = 0;
	u32 temp;

	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) + LME_R_SW_RESET, 0x1);

	while (1) {
		temp = LME_GET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
					LME_R_SW_RESET);
		if (temp == 0)
			break;
		if (reset_count > LME_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	info_hw("[LME] %s done.\n", __func__);

	return 0;
}

void lme_hw_s_init(struct pablo_mmio *base, u32 set_id)
{
	LME_SET_F(base, LME_R_CMDQ_VHD_CONTROL, LME_F_CMDQ_VHD_VBLANK_QRUN_ENABLE, 1);
	LME_SET_F(base, LME_R_CMDQ_VHD_CONTROL, LME_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE, 1);
	LME_SET_F(base, LME_R_DEBUG_CLOCK_ENABLE, LME_F_DEBUG_CLOCK_ENABLE, 0); /* for debugging */

	if (!IS_ENABLED(LME_USE_MMIO)) {
		LME_SET_R(base, LME_R_C_LOADER_ENABLE, 1);
		LME_SET_R(base, LME_R_STAT_RDMACL_EN, 1);
	}

	/* Interrupt group enable for one frame */
	LME_SET_F(base, LME_R_CMDQ_QUE_CMD_L, LME_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, LME_INT_GRP_EN_MASK);
	/* 1: DMA preloading, 2: COREX, 3: APB Direct */
	LME_SET_F(base, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	LME_SET_R(base, LME_R_CMDQ_ENABLE, 1);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_init);

void lme_hw_s_clock(void __iomem *base, bool on)
{
	dbg_hw(5, "[LME] clock %s", on ? "on" : "off");
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_IP_PROCESSING, LME_F_IP_PROCESSING, on);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_clock);

int lme_hw_wait_idle(void __iomem *base, u32 set_id)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	idle = LME_GET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_IDLENESS_STATUS, LME_F_IDLENESS_STATUS);
	int_all = LME_GET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_INT_REQ_INT0_STATUS);

	info_hw("[LME] idle status before disable (idle:%d, int1:0x%X)\n",
			idle, int_all);

	while (!idle) {
		idle = LME_GET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_IDLENESS_STATUS, LME_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= LME_TRY_COUNT) {
			err_hw("[LME] timeout waiting idle - disable fail");
			lme_hw_dump(base, COREX_DIRECT);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int_all = LME_GET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_INT_REQ_INT0_STATUS);

	info_hw("[LME] idle status after disable (idle:%d, int1:0x%X)\n", idle, int_all);

	return ret;
}

void lme_hw_dump(void __iomem *base, enum corex_set set_id)
{
	info_hw("[LME] SFR DUMP (v4.0)\n");
	if (IS_ENABLED(BYRP_USE_MMIO))
		is_hw_dump_regs(base, GET_COREX_OFFSET(COREX_DIRECT) + lme_regs, LME_REG_CNT);
	else
		is_hw_dump_regs(pmio_get_base(base), GET_COREX_OFFSET(COREX_DIRECT) + lme_regs, LME_REG_CNT);
}

static void lme_hw_s_common(void __iomem *base, u32 set_id)
{
	/* 0: start frame asap, 1; start frame upon cinfifo vvalid rise */
	LME_SET_F(base, GET_COREX_OFFSET(set_id) +
		LME_R_IP_USE_CINFIFO_NEW_FRAME_IN,
		LME_F_IP_USE_CINFIFO_NEW_FRAME_IN, 0x0);
}

static void lme_hw_s_int_mask(void __iomem *base, u32 set_id)
{
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_INT_REQ_INT0_ENABLE,
		LME_F_INT_REQ_INT0_ENABLE,
		LME_INT_EN_MASK);
}

static void lme_hw_s_secure_id(void __iomem *base, u32 set_id)
{
	/*
	 * Set Paramer Value
	 * scenario
	 * 0: Non-secure,  1: Secure
	 */
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		 LME_R_SECU_CTRL_SEQID, LME_F_SECU_CTRL_SEQID, 0x0);
}

static void lme_hw_s_block_crc(void __iomem *base, u32 set_id)
{
	/* Nothing to config except AXI CRC */
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_AXICRC_SIPULME4P0P0_CNTX0_SEED_0,
			LME_F_AXICRC_SIPULME4P0P0_CNTX0_SEED_0, 0x37);
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_AXICRC_SIPULME4P0P0_CNTX0_SEED_0,
			LME_F_AXICRC_SIPULME4P0P0_CNTX0_SEED_1, 0x37);
}

void lme_hw_s_core(void __iomem *base, u32 set_id)
{
	lme_hw_s_common(base, set_id);
	lme_hw_s_int_mask(base, set_id);
	lme_hw_s_secure_id(base, set_id);
	lme_hw_s_block_crc(base, set_id);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_core);

int lme_hw_s_rdma_init(void __iomem *base, struct lme_param_set *param_set,
	u32 enable, u32 id, u32 set_id)
{
	u32 mv_height, lwidth;
	u32 val = 0;

	switch (id) {
	case LME_RDMA_CACHE_IN_0:
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_CACHE_IN_CLIENT_ENABLE, enable);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_CACHE_IN_GEOM_BURST_LENGTH, 0xf);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_CACHE_IN_BURST_ALIGNMENT, 0x0);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_IN_RDMAYIN_EN, enable);
		break;
	case LME_RDMA_CACHE_IN_1:
		break;
	case LME_RDMA_MBMV_IN:
		/* MBMV IN */
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			 LME_R_DMACLIENT_CNTX0_MBMV_IN_CLIENT_ENABLE, enable);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			 LME_R_MBMV_IN_RDMAYIN_EN, enable);

		if (enable == 0)
			return 0;

		lwidth = 2 * DIV_ROUND_UP(param_set->dma.output_width, 16);
		mv_height = DIV_ROUND_UP(param_set->dma.output_height, 16);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BURST_LENGTH, 0x2);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LWIDTH, lwidth);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LINE_COUNT,
			mv_height);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_TOTAL_WIDTH, lwidth);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LINE_DIRECTION_HW,
			0x1);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_LWIDTH, lwidth);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_LINEGAP, 0x14);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PREGAP, 0x1);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_POSTGAP, 0x14);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PIXELGAP, 0x0);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_STALLGAP, 0x1);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PACKING, 0x8);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PACKING, 0x8);

		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_FRMT_BPAD_SET, 0x0);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_FRMT_BSHIFT_SET, 0x0);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_MNM, val);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_CH_MIX_0, 0x0);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_CH_MIX_1, 0x1);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_BURST_ALIGNMENT, 0x0);
		break;
	default:
		err_hw("[LME] invalid dma_id[%d]", id);
		break;
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_rdma_init);

int lme_hw_s_wdma_init(void __iomem *base,
	struct lme_param_set *param_set, u32 enable, u32 id, u32 set_id,
	enum lme_sps_out_mode sps_mode)
{
	u32 mv_height, lwidth;
	u32 val = 0;

	switch (id) {
	case LME_WDMA_MV_OUT:
		/* MV OUT */
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_CLIENT_ENABLE, enable);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_SPS_MV_OUT_WDMAMV_EN, enable);

		if (enable == 0)
			return 0;

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BURST_LENGTH,
			0x3);

		switch (sps_mode) {
		case LME_OUTPUT_MODE_8_4:
			lwidth = 4 * DIV_ROUND_UP(param_set->dma.cur_input_width, 8);
			mv_height = DIV_ROUND_UP(param_set->dma.cur_input_height, 4);
			break;
		case LME_OUTPUT_MODE_2_2:
			lwidth = 4 * DIV_ROUND_UP(param_set->dma.cur_input_width, 2);
			mv_height = DIV_ROUND_UP(param_set->dma.cur_input_height, 2);
			break;
		default:
			err_hw("[LME] undefined sps_mode(%d) force set as LME_OUTPUT_MODE_8_4",
				sps_mode);
			lwidth = 4 * DIV_ROUND_UP(param_set->dma.cur_input_width, 8);
			mv_height = DIV_ROUND_UP(param_set->dma.cur_input_height, 4);
			break;
		};

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LWIDTH, lwidth);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_COUNT,
			mv_height);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_TOTAL_WIDTH,
			lwidth);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_DIRECTION_HW,
			0x1);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_PACKING, 16);

		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_BPAD_SET, 0x4);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_BSHIFT_SET, 0x4);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_MNM, val);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_CH_MIX_0, 0x0);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_CH_MIX_1, 0x1);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			 LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_BURST_ALIGNMENT, 0x0);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_SELF_HW_FLUSH_ENABLE,
			0x0);
		break;

	case LME_WDMA_SAD_OUT:
		/* SAD OUT */
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_CLIENT_ENABLE, enable);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_SAD_OUT_WDMA_EN, enable);

		if (enable == 0)
			return 0;

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_BURST_LENGTH, 0x3);

		switch (sps_mode) {
		case LME_OUTPUT_MODE_8_4:
			lwidth = 3 * DIV_ROUND_UP(param_set->dma.output_width, 8);
			mv_height = DIV_ROUND_UP(param_set->dma.output_height, 4);
			break;
		case LME_OUTPUT_MODE_2_2:
			lwidth = 3 * DIV_ROUND_UP(param_set->dma.output_width, 2);
			mv_height = DIV_ROUND_UP(param_set->dma.output_height, 2);
			break;
		default:
			err_hw("[LME] undefined sps_mode(%d) force set as LME_OUTPUT_MODE_8_4",
				sps_mode);
			lwidth = 3 * DIV_ROUND_UP(param_set->dma.output_width, 8);
			mv_height = DIV_ROUND_UP(param_set->dma.output_height, 4);
			break;
		};

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_LWIDTH, lwidth);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_LINE_COUNT,
			mv_height);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_TOTAL_WIDTH, lwidth);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_LINE_DIRECTION_HW,
			0x1);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_FRMT_PACKING, 24);

		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_SAD_OUT_FRMT_BPAD_SET, 0x8);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_SAD_OUT_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_SAD_OUT_FRMT_BSHIFT_SET, 0x8);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_FRMT_MNM, val);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_BURST_ALIGNMENT, 0x0);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_SELF_HW_FLUSH_ENABLE,
			0x0);
		break;
	case LME_WDMA_MBMV_OUT:
		/* MBMV OUT */
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_CLIENT_ENABLE, enable);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_MBMV_OUT_WDMAMV_EN, enable);

		if (enable == 0)
			return 0;

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BURST_LENGTH, 0x3);

		lwidth = 2 * DIV_ROUND_UP(param_set->dma.output_width, 16);
		mv_height = DIV_ROUND_UP(param_set->dma.output_height, 16);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_LWIDTH, lwidth);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_LINE_COUNT,
			mv_height);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_TOTAL_WIDTH,
			lwidth);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_LINE_DIRECTION_HW,
			0x1);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_PACKING, 0x8);

		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_OUT_FRMT_BPAD_SET, 0x0);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_OUT_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_OUT_FRMT_BSHIFT_SET, 0x0);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_MNM, val);

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_CH_MIX_0, 0x0);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_CH_MIX_1, 0x1);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_BURST_ALIGNMENT, 0x0);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_CLIENT_FLUSH, 0);
		break;
	default:
		break;
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_wdma_init);

int lme_hw_s_rdma_addr(void __iomem *base, dma_addr_t *addr, u32 id, u32 set_id)
{
	int ret = SET_SUCCESS;
	int val = 0;

	switch (id) {
	dbg_hw(4, "[API] [%s] id:%d addr:%pad/%pad offset:0x%x/0x%x/0x%x/0x%x",
		__func__, id, &addr[0], &addr[1], GET_COREX_OFFSET(0),
		GET_COREX_OFFSET(1), GET_COREX_OFFSET(2),
		GET_COREX_OFFSET(3));
	case LME_RDMA_CACHE_IN_0:
		/* TOOD: Need to fix support corex */
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_BASE_ADDR_1P_0_LSB,
			DVA_36BIT_LOW(addr[0]));

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_BASE_ADDR_1P_0_MSB,
			DVA_36BIT_HIGH(addr[0]));
		break;
	case LME_RDMA_CACHE_IN_1:
		/* TOOD: Need to fix support corex */
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_BASE_ADDR_1P_1_LSB,
			DVA_36BIT_LOW(addr[0]));

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_BASE_ADDR_1P_1_MSB,
			DVA_36BIT_HIGH(addr[0]));
		break;
	case LME_RDMA_MBMV_IN:
		/* TOOD: Need to fix support corex */
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_EN_0, 1);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_EN_1, 1);
		val = LME_SET_V(base, val,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_SIZE,
			1);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_CONF, val);

		LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB_0,
			DVA_36BIT_LOW(addr[0]));

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_0,
			DVA_36BIT_HIGH(addr[0]));

		LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB_1,
			DVA_36BIT_LOW(addr[1]));

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_1,
			DVA_36BIT_HIGH(addr[1]));
		break;
	default:
		err_hw("[LME] invalid dma_id[%d]", id);
		break;
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_rdma_addr);

int lme_hw_s_wdma_addr(void __iomem *base, dma_addr_t *addr, u32 id, u32 set_id)
{
	int ret = SET_SUCCESS;
	int val = 0;

	switch (id) {
	dbg_hw(4, "[API] [%s] id:%d addr:%pad/%pad offset:0x%x/0x%x/0x%x/0x%x",
		__func__, id, &addr[0], &addr[1], GET_COREX_OFFSET(0),
		GET_COREX_OFFSET(1), GET_COREX_OFFSET(2),
		GET_COREX_OFFSET(3));
	case LME_WDMA_MV_OUT:
		/* TOOD: Need to fix support corex */
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_LSB,
			DVA_36BIT_LOW(addr[0]));

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_0,
			DVA_36BIT_HIGH(addr[0]));
		break;
	case LME_WDMA_MBMV_OUT:
		/* TOOD: Need to fix support corex */
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_EN_0, 1);
		val = LME_SET_V(base, val,
			LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_EN_1, 1);
		val = LME_SET_V(base, val,
		LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_ROTATION_SIZE,
			1);
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_CONF,
			val);

		LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB,
			LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB_0,
			DVA_36BIT_LOW(addr[0]));

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_0,
			(DVA_36BIT_HIGH(addr[0])));

		LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB,
			LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB_1,
			DVA_36BIT_LOW(addr[1]));

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_1,
			DVA_36BIT_HIGH(addr[1]));


		break;
	case LME_WDMA_SAD_OUT:
		/* TOOD: Need to fix support corex */
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_BASE_ADDR_LSB,
			DVA_36BIT_LOW(addr[0]));

		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_BASE_ADDR_0,
			DVA_36BIT_HIGH(addr[0]));
		break;
	default:
		err_hw("[LME] invalid dma_id[%d]", id);
		ret = SET_ERROR;
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_wdma_addr);

int lme_hw_s_corex_update_type(void __iomem *base, u32 set_id, u32 type)
{
	int ret = 0;

	switch (set_id) {
	case COREX_SET_A:
	case COREX_SET_B:
		LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_COREX_UPDATE_TYPE_0,
			LME_F_COREX_UPDATE_TYPE_0, type);
		break;
	case COREX_DIRECT:
		LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_COREX_UPDATE_TYPE_0,
			LME_F_COREX_UPDATE_TYPE_0, COREX_IGNORE);
		break;
	default:
		err_hw("[LME] invalid corex set_id");
		ret = -EINVAL;
		break;
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_corex_update_type);

static void lme_hw_wait_corex_idle(void __iomem *base)
{
	u32 busy;
	u32 try_cnt = 0;

	do {
		udelay(1);

		try_cnt++;
		if (try_cnt >= LME_TRY_COUNT) {
			err_hw("[LME] fail to wait corex idle");
			break;
		}

		busy = LME_GET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_COREX_STATUS_0, LME_F_COREX_BUSY_0);
		dbg_hw(1, "[LME] %s busy(%d)\n", __func__, busy);

	} while (busy);
}

void lme_hw_s_cmdq(void __iomem *base, u32 set_id, dma_addr_t clh, u32 noh)
{
	if (clh && noh) {
		LME_SET_F(base, LME_R_CMDQ_QUE_CMD_H, LME_F_CMDQ_QUE_CMD_BASE_ADDR, DVA_36BIT_HIGH(clh));
		LME_SET_F(base, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_HEADER_NUM, noh);
		LME_SET_F(base, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE, 1);
	} else {
		LME_SET_F(base, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	}

	LME_SET_R(base, LME_R_CMDQ_ADD_TO_QUEUE_0, 1);
	LME_SET_R(base, LME_R_CMDQ_ENABLE, 1);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_cmdq);

void lme_hw_s_corex_init(void __iomem *base, bool enable)
{
	/*
	 * Check COREX idleness
	 */
	if (!enable) {
		LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_COREX_UPDATE_MODE_0,
			LME_F_COREX_UPDATE_MODE_0, SW_TRIGGER);

		lme_hw_wait_corex_idle(base);

		LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_COREX_ENABLE, LME_F_COREX_ENABLE, 0x0);

		info_hw("[LME] %s disable done\n", __func__);
		return;
	}

	/*
	 * config type0
	 * @TAA_R_COREX_UPDATE_TYPE_0: 0: ignore, 1: copy from SRAM, 2: swap
	 * @TAA_R_COREX_UPDATE_MODE_0: 0: HW trigger, 1: SW trigger
	 */
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_UPDATE_TYPE_0, LME_F_COREX_UPDATE_TYPE_0,
		COREX_COPY);
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_UPDATE_TYPE_1, LME_F_COREX_UPDATE_TYPE_1,
		COREX_IGNORE);

	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_UPDATE_MODE_0, LME_F_COREX_UPDATE_MODE_0,
			HW_TRIGGER);
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_UPDATE_MODE_1, LME_F_COREX_UPDATE_MODE_1,
			HW_TRIGGER);

	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_START_0, LME_F_COREX_START_0, 0);
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_START_1, LME_F_COREX_START_1, 0);

	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) + LME_R_COREX_ENABLE,
			LME_F_COREX_ENABLE, 0x1);

	/* initialize type0 */
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_COPY_FROM_IP_0, LME_F_COREX_COPY_FROM_IP_0, 0x1);

	/*
	 * Check COREX idleness, again.
	 */
	lme_hw_wait_corex_idle(base);

	info_hw("[LME] %s done\n", __func__);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_corex_init);

/*
 * Context: O
 * CR type: No Corex
 */
void lme_hw_s_corex_start(void __iomem *base, bool enable)
{
	if (!enable)
		return;

	/*
	 * Set Fixed Value
	 *
	 * Type0 only:
	 *
	 * @TAA_R_COREX_START_0 - 1: puls generation
	 * @TAA_R_COREX_UPDATE_MODE_0 - 0: HW trigger, 1: SW tirgger
	 *
	 * SW trigger should be needed before stream on
	 * because there is no HW trigger before stream on.
	 */
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_UPDATE_MODE_0,
		LME_F_COREX_UPDATE_MODE_0, SW_TRIGGER);
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_START_0, LME_F_COREX_START_0, 0x1);

	lme_hw_wait_corex_idle(base);

	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_COREX_UPDATE_MODE_0,
		LME_F_COREX_UPDATE_MODE_0, HW_TRIGGER);

	info_hw("[LME] %s done\n", __func__);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_corex_start);

unsigned int lme_hw_g_int_state(void __iomem *base, bool clear, u32 num_buffers,
	u32 *irq_state, u32 set_id)
{
	u32 src, src_all, src_err;

	/*
	 * src_all: per-frame based lme IRQ status
	 *
	 * final normal status: src_fro (start, line, end)
	 * final error status(src_err): src_all & ERR_MASK
	 */
	src_all = LME_GET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_INT_REQ_INT0);

	dbg_hw(4, "[API][%s] src_all: 0x%x\n", __func__, src_all);

	if (clear)
		LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_INT_REQ_INT0_CLEAR, src_all);

	src_err = src_all & LME_INT_ERR_MASK;

	*irq_state = src_all;

	src = src_all;

	return src | src_err;
}
KUNIT_EXPORT_SYMBOL(lme_hw_g_int_state);

unsigned int lme_hw_g_int_mask(void __iomem *base, u32 set_id)
{
	return LME_GET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_INT_REQ_INT0_ENABLE);
}

void lme_hw_s_cache(void __iomem *base, u32 set_id,
		bool enable, u32 prev_width, u32 cur_width)
{
	u32 val = 0;
	u32 mode;
	u32 ignore_prefetch;
	u32 prev_pix_gain, cur_pix_gain;
	u32 prev_pix_offset, cur_pix_offset;

	/* Common */
	enable = 1;
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_CACHE_8BIT_LME_BYPASS,
		LME_F_Y_LME_BYPASS, !enable);

	mode = 1; /* 0: Fusion, 1: TNR */
	ignore_prefetch = 0; /* use prefetch for performance */

	val = LME_SET_V(base, val, LME_F_CACHE_8BIT_IGNORE_PREFETCH, ignore_prefetch);
	val = LME_SET_V(base, val, LME_F_CACHE_8BIT_DATA_REQ_CNT_EN, 1);
	val = LME_SET_V(base, val, LME_F_CACHE_8BIT_PRE_REQ_CNT_EN, 1);
	val = LME_SET_V(base, val, LME_F_CACHE_8BIT_CACHE_CADDR_OFFSET, 0x8);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_LME_BYPASS, val);

	prev_pix_gain = 0x40;
	prev_pix_offset = 0;

	val = 0;
	val = LME_SET_V(base, val, LME_F_Y_LME_PRVCMGAIN, prev_pix_gain);
	val = LME_SET_V(base, val, LME_F_Y_LME_PRVIMGHEIGHT, prev_pix_offset);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_PIX_CONFIG_0, val);

	cur_pix_gain = 0x40;
	cur_pix_offset = 0;

	val = 0;
	val = LME_SET_V(base, val, LME_F_Y_LME_CURCMGAIN, cur_pix_gain);
	val = LME_SET_V(base, val, LME_F_Y_LME_CURCMOFFSET, cur_pix_offset);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_PIX_CONFIG_1, val);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_cache);

void lme_hw_s_cache_size(void __iomem *base, u32 set_id, u32 prev_width,
		 u32 prev_height, u32 cur_width, u32 cur_height)
{
	u32 val = 0;
	u32 prev_crop_start_x, prev_crop_start_y;
	u32 cur_crop_start_x, cur_crop_start_y;
	u32 prev_width_jump, cur_width_jump;

	dbg_hw(4, "[API][%s] prev(%dx%d), cur(%dx%d)\n", __func__,
		prev_width, prev_height, cur_width, cur_height);

	/* previous frame */
	val = LME_SET_V(base, val, LME_F_Y_LME_PRVIMGWIDTH, prev_width);
	val = LME_SET_V(base, val, LME_F_Y_LME_PRVIMGHEIGHT, prev_height);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_IMAGE0_CONFIG, val);

	prev_crop_start_x = 0; /* Not use crop */
	prev_crop_start_y = 0;

	val = 0;
	val = LME_SET_V(base, val, LME_F_Y_LME_PRVROISTARTX,
			prev_crop_start_x);
	val = LME_SET_V(base, val, LME_F_Y_LME_PRVROISTARTY,
			prev_crop_start_y);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_CROP_CONFIG_START_0, val);

	prev_width_jump = ALIGN(prev_width, DMA_CLIENT_LME_BYTE_ALIGNMENT);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_0, prev_width_jump);

	/* current frame */
	val = 0;
	val = LME_SET_V(base, val, LME_F_Y_LME_CURIMGWIDTH, cur_width);
	val = LME_SET_V(base, val, LME_F_Y_LME_CURIMGHEIGHT, cur_height);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_IMAGE1_CONFIG, val);

	cur_crop_start_x = 0; /* Not use crop */
	cur_crop_start_y = 0;

	val = 0;
	val = LME_SET_V(base, val, LME_F_Y_LME_CURROISTARTX, cur_crop_start_x);
	val = LME_SET_V(base, val, LME_F_Y_LME_CURROISTARTY, cur_crop_start_y);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_CROP_CONFIG_START_1, val);

	cur_width_jump = ALIGN(cur_width, DMA_CLIENT_LME_BYTE_ALIGNMENT);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_1, cur_width_jump);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_cache_size);

void lme_hw_s_mvct_size(void __iomem *base, u32 set_id, u32 width, u32 height)
{
	u32 val = 0;
	u32 prefetch_gap;
	u32 prefetch_en;
	const u32 cell_width = 16;

	prefetch_gap = DIV_ROUND_UP(width * 15 / 100, cell_width);
	prefetch_en = 1;
	dbg_hw(4, "[API][%s] width: %d, cell_width: %d, prefetch_gap : %d\n",
		__func__, width, cell_width, prefetch_gap);

	val = LME_SET_V(base, val, LME_F_MVCT_8BIT_LME_PREFETCH_GAP, prefetch_gap);
	val = LME_SET_V(base, val, LME_F_MVCT_8BIT_LME_PREFETCH_EN, prefetch_en);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_MVCT_8BIT_LME_PREFETCH, val);

	val = 0;
	val = LME_SET_V(base, val, LME_F_Y_LME_ROISIZEX, width);
	val = LME_SET_V(base, val, LME_F_Y_LME_ROISIZEY, height);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_MVCT_8BIT_IMAGE_DIMENTIONS, val);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_mvct_size);

void lme_hw_s_mvct(void __iomem *base, u32 set_id)
{
	u32 val = 0;
	const u32 sr_x = 128; /* search range: 16 ~ 128 */
	const u32 sr_y = 128;

	/* 0: fusion, 1: TNR */
	val = LME_SET_V(base, val, LME_F_Y_LME_MODE, 0x1);
	val = LME_SET_V(base, val, LME_F_Y_LME_FIRSTFRAME, 0x0);
	val = LME_SET_V(base, val, LME_F_MVCT_8BIT_LME_FW_FRAME_ONLY, 0x0);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_MVCT_8BIT_LME_CONFIG, val);

	val = 0;
	val = LME_SET_V(base, val, LME_F_Y_LME_USEAD, 0x1);
	val = LME_SET_V(base, val, LME_F_Y_LME_USESAD, 0x0);
	val = LME_SET_V(base, val, LME_F_Y_LME_USECT, 0x1);
	val = LME_SET_V(base, val, LME_F_Y_LME_USEZSAD, 0x1);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_MVCT_8BIT_MVE_CONFIG, val);

	val = 0;
	val = LME_SET_V(base, val, LME_F_Y_LME_WEIGHTCT, 1);
	val = LME_SET_V(base, val, LME_F_Y_LME_WEIGHTAD, 5);
	val = LME_SET_V(base, val, LME_F_Y_LME_WEIGHTSAD, 1);
	val = LME_SET_V(base, val, LME_F_Y_LME_WEIGHTZSAD, 1);
	val = LME_SET_V(base, val, LME_F_Y_LME_NOISELEVEL, 3);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_MVCT_8BIT_MVE_WEIGHT, val);

	val = 0;
	val = LME_SET_V(base, val, LME_F_Y_LME_MPSSRCHRANGEX, sr_x);
	val = LME_SET_V(base, val, LME_F_Y_LME_MPSSRCHRANGEY, sr_y);
	LME_SET_R(base, GET_COREX_OFFSET(COREX_DIRECT) +
			LME_R_MVCT_8BIT_MV_SR, val);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_mvct);

void lme_hw_s_first_frame(void __iomem *base, u32 set_id, bool first_frame)
{
	/* FIX ME : fist mode sync with ddk */
	first_frame = true;

	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_MVCT_8BIT_LME_CONFIG,
		LME_F_Y_LME_FIRSTFRAME, first_frame);

	if (first_frame)
		LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET,
		0);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_first_frame);

void lme_hw_s_sps_out_mode(void __iomem *base, u32 set_id, enum lme_sps_out_mode outmode)
{
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_MVCT_8BIT_LME_CONFIG,
		LME_F_Y_LME_LME_8X8SEARCH, outmode);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_sps_out_mode);

void lme_hw_s_crc(void __iomem *base, u32 seed)
{
	LME_SET_F(base, LME_R_AXICRC_SIPULME4P0P0_CNTX0_SEED_0,
			LME_F_AXICRC_SIPULME4P0P0_CNTX0_SEED_0, seed);
	LME_SET_F(base, LME_R_AXICRC_SIPULME4P0P0_CNTX0_SEED_1,
			LME_F_AXICRC_SIPULME4P0P0_CNTX0_SEED_1, seed);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_crc);

struct is_reg *lme_hw_get_reg_struct(void)
{
	return (struct is_reg *)lme_regs;
}
KUNIT_EXPORT_SYMBOL(lme_hw_get_reg_struct);

unsigned int lme_hw_get_reg_cnt(void)
{
	return LME_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(lme_hw_get_reg_cnt);

void lme_hw_s_block_bypass(void __iomem *base, u32 set_id)
{
	LME_SET_F(base, GET_COREX_OFFSET(COREX_DIRECT) +
		LME_R_CACHE_8BIT_LME_BYPASS, LME_F_Y_LME_BYPASS, 0x0);
}
KUNIT_EXPORT_SYMBOL(lme_hw_s_block_bypass);

unsigned int lme_hw_g_reg_cnt(void)
{
	return LME_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(lme_hw_g_reg_cnt);

bool lme_hw_use_corex_set(void)
{
	return false;
}
KUNIT_EXPORT_SYMBOL(lme_hw_use_corex_set);

bool lme_hw_use_mmio(void)
{
	return LME_USE_MMIO;
}
KUNIT_EXPORT_SYMBOL(lme_hw_use_mmio);

void lme_hw_init_pmio_config(struct pmio_config *cfg)
{
	cfg->num_corexs = 2;
	cfg->corex_stride = 0x8000;

	cfg->volatile_table = &lme_volatile_table;
	cfg->wr_noinc_table = &lme_wr_noinc_table;

	cfg->max_register = LME_R_IP_END_INTERRUPT_FRO;
	cfg->num_reg_defaults_raw = (LME_R_IP_END_INTERRUPT_FRO >> 2) + 1;
	cfg->phys_base = 0x28830000;
	cfg->dma_addr_shift = 4;

	cfg->ranges = lme_range_cfgs;
	cfg->num_ranges = ARRAY_SIZE(lme_range_cfgs);

	cfg->fields = lme_field_descs;
	cfg->num_fields = ARRAY_SIZE(lme_field_descs);
}
KUNIT_EXPORT_SYMBOL(lme_hw_init_pmio_config);