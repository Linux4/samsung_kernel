// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * shrp HW control APIs
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/delay.h>
#include "is-hw-api-shrp-v3_0.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
#include "sfr/is-sfr-shrp-v3_1.h"
#else
#include "sfr/is-sfr-shrp-v3_0.h"
#endif
#include "pmio.h"

#define SHRP_LUT_REG_CNT	2560
#define SHRP_LUT_NUM		2	/* DRC, Face DRC */

unsigned int shrp_hw_is_occurred0(unsigned int status, enum shrp_event_type type)
{
	u32 mask;

	dbg_hw(4, "[API] %s\n", __func__);

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR1_SHRP_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR1_SHRP_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR1_SHRP_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR1_SHRP_COREX_END_INT_1;
		break;
	case INTR_SETTING_DONE:
		mask = 1 << INTR1_SHRP_SETTING_DONE_INT;
		break;
	case INTR_ERR:
		mask = INT1_SHRP_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_is_occurred0);

unsigned int shrp_hw_is_occurred1(unsigned int status, enum shrp_event_type type)
{
	u32 mask;

	dbg_hw(4, "[API] %s\n", __func__);

	switch (type) {
	case INTR_ERR:
		mask = INT2_SHRP_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_is_occurred1);

int shrp_hw_s_reset(void *base)
{
	u32 reset_count = 0;

	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_R(base, SHRP_R_SW_RESET, 0x1);

	while (SHRP_GET_R(base, SHRP_R_SW_RESET)) {
		if (reset_count > SHRP_TRY_COUNT)
			return -ENODEV;
		reset_count++;
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_reset);

void shrp_hw_s_init(void *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_M, SHRP_F_CMDQ_QUE_CMD_SETTING_MODE, 3);

	SHRP_SET_R(base, SHRP_R_AUTO_IGNORE_PREADY_ENABLE, 1);
	SHRP_SET_F(base, SHRP_R_CMDQ_VHD_CONTROL, SHRP_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE, 1);
	SHRP_SET_F(base, SHRP_R_CMDQ_VHD_CONTROL, SHRP_F_CMDQ_VHD_VBLANK_QRUN_ENABLE, 1);
	SHRP_SET_R(base, SHRP_R_CMDQ_ENABLE, 1);

	SHRP_SET_R(base, SHRP_R_DEBUG_CLOCK_ENABLE, 1);

	if (!IS_ENABLED(SHRP_USE_MMIO)) {
		SHRP_SET_R(base, SHRP_R_C_LOADER_ENABLE, 1);
		SHRP_SET_R(base, SHRP_R_STAT_RDMACL_EN, 1);
	}
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_init);

void shrp_hw_s_clock(void *base, bool on)
{
	dbg_hw(5, "[SHRP] clock %s\n", on ? "on" : "off");
	SHRP_SET_F(base, SHRP_R_IP_PROCESSING, SHRP_F_IP_PROCESSING, on);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_clock);

int shrp_hw_wait_idle(void *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	idle = SHRP_GET_F(base, SHRP_R_IDLENESS_STATUS, SHRP_F_IDLENESS_STATUS);
	int_all = SHRP_GET_R(base, SHRP_R_INT_REQ_INT0);

	info_hw("[SHRP] idle status before disable (idle:%d, int1:0x%X)\n",
			idle, int_all);

	while (!idle) {
		idle = SHRP_GET_F(base, SHRP_R_IDLENESS_STATUS, SHRP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= SHRP_TRY_COUNT) {
			err_hw("[SHRP] timeout waiting idle - disable fail");
			shrp_hw_dump(base, HW_DUMP_CR);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int_all = SHRP_GET_R(base, SHRP_R_INT_REQ_INT0);

	info_hw("[SHRP] idle status after disable (idle:%d, int1:0x%X)\n", idle, int_all);

	return ret;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_wait_idle);

static const struct is_reg shrp_dbg_cr[] = {
	/* The order of DBG_CR should match with the DBG_CR parser. */
	/* Interrupt History */
	{0x0900, "INT_HIST_CURINT0"},
	{0x090c, "INT_HIST_CURINT1"},
	/* Core Status */
	{0x0f08, "QCH_STATUS"},
	{0x0f0c, "IDLENESS_STATUS"},
	{0x0f40, "IP_BUSY_MONITOR_0"},
	{0x0f44, "IP_BUSY_MONITOR_1"},
	{0x0f48, "IP_BUSY_MONITOR_2"},
	{0x0f4c, "IP_BUSY_MONITOR_3"},
	{0x0f60, "IP_STALL_OUT_STATUS_0"},
	{0x0f64, "IP_STALL_OUT_STATUS_1"},
	{0x0f68, "IP_STALL_OUT_STATUS_2"},
	{0x0f6c, "IP_STALL_OUT_STATUS_3"},
	/* Chain Size */
	{0x0200, "CHAIN_SRC_IMG_SIZE"},
	{0x0204, "CHAIN_SHARPEN_IMG_SIZE"},
	{0x0208, "CHAIN_DST_IMG_SIZE"},
	/* Chain Path */
	{0x0080, "IP_USE_OTF_PATH_01"},
	{0x0210, "CHAIN_MUX_SELECT"},
	{0x0214, "CHAIN_DEMUX_ENABLE"},
	/* CMDQ Frame Counter */
	{0x04a8, "CMDQ_FRAME_COUNTER"},
	/* CINFIFO 0 Status */
	{0x1000, "YUV_CINFIFO0_ENABLE"},
	{0x1014, "YUV_CINFIFO0_STATUS"},
	{0x1018, "YUV_CINFIFO0_INPUT_CNT"},
	{0x101c, "YUV_CINFIFO0_STALL_CNT"},
	{0x1020, "YUV_CINFIFO0_FIFO_FULLNESS"},
	{0x1040, "YUV_CINFIFO0_INT"},
	/* CINFIFO 1 Status */
	{0x1100, "YUV_CINFIFODECOMP_ENABLE"},
	{0x1114, "YUV_CINFIFODECOMP_STATUS"},
	{0x1118, "YUV_CINFIFODECOMP_INPUT_CNT"},
	{0x111c, "YUV_CINFIFODECOMP_STALL_CNT"},
	{0x1120, "YUV_CINFIFODECOMP_FIFO_FULLNESS"},
	{0x1140, "YUV_CINFIFODECOMP_INT"},
	/* COUTFIFO 0 Status */
	{0x1200, "YUV_COUTFIFO0_ENABLE"},
	{0x1214, "YUV_COUTFIFO0_STATUS"},
	{0x1218, "YUV_COUTFIFO0_INPUT_CNT"},
	{0x121c, "YUV_COUTFIFO0_STALL_CNT"},
	{0x1220, "YUV_COUTFIFO0_FIFO_FULLNESS"},
	{0x1240, "YUV_COUTFIFO0_INT"},
	/* COUTFIFO 1 Status */
	{0x1300, "YUV_COUTFIFODECOMP_ENABLE"},
	{0x1314, "YUV_COUTFIFODECOMP_STATUS"},
	{0x1318, "YUV_COUTFIFODECOMP_INPUT_CNT"},
	{0x131c, "YUV_COUTFIFODECOMP_STALL_CNT"},
	{0x1320, "YUV_COUTFIFODECOMP_FIFO_FULLNESS"},
	{0x1340, "YUV_COUTFIFODECOMP_INT"},
};

static void shrp_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	void *ctx;
	const struct is_reg *cr;
	u32 i, val;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;
	pmio->reg_read(ctx, SHRP_R_IP_VERSION, &val);

	is_dbg("[HW:%s] v%02u.%02u.%02u ======================================\n", pmio->name,
		(val >> 24) & 0xff, (val >> 16) & 0xff, val & 0xffff);
	for (i = 0; i < ARRAY_SIZE(shrp_dbg_cr); i++) {
		cr = &shrp_dbg_cr[i];

		pmio->reg_read(ctx, cr->sfr_offset, &val);
		is_dbg("[HW:%s]%40s %08x\n", pmio->name, cr->reg_name, val);
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);

	for(i = 0; i < SHRP_CHAIN_DEBUG_COUNTER_MAX; i++) {
		SHRP_SET_R(pmio, SHRP_R_CHAIN_DEBUG_CNT_SEL, shrp_counter[i].counter);
		sfrinfo("[CHAIN_DEBUG] counter:[%s], row:[%d], col:[%d]\n",
			shrp_counter[i].name,
			SHRP_GET_F(pmio, SHRP_R_CHAIN_DEBUG_CNT_VAL, SHRP_F_CHAIN_DEBUG_ROW_CNT),
			SHRP_GET_F(pmio, SHRP_R_CHAIN_DEBUG_CNT_VAL, SHRP_F_CHAIN_DEBUG_COL_CNT));
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);
}

void shrp_hw_dump(struct pablo_mmio *pmio, u32 mode)
{
	switch (mode) {
	case HW_DUMP_CR:
		info_hw("[SHRP]%s:DUMP CR\n", __FILENAME__);
		is_hw_dump_regs(pmio_get_base(pmio), shrp_regs, SHRP_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		info_hw("[SHRP]%s:DUMP DBG_STATE\n", __FILENAME__);
		shrp_hw_dump_dbg_state(pmio);
		break;
	default:
		err_hw("[SHRP]%s:Not supported dump_mode %u", __FILENAME__, mode);
		break;
	}
}
KUNIT_EXPORT_SYMBOL(shrp_hw_dump);

void shrp_hw_s_coutfifo(void *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_IP_USE_OTF_PATH_01,
			SHRP_F_IP_USE_OTF_OUT_FOR_PATH_0, 1);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_COUTFIFO0_INTERVALS,
			SHRP_F_YUV_COUTFIFO0_INTERVAL_HBLANK, HBLANK_CYCLE);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_COUTFIFO0_INTERVAL_VBLANK,
			SHRP_F_YUV_COUTFIFO0_INTERVAL_VBLANK, VBLANK_CYCLE);
	SHRP_SET_F(base, SHRP_R_YUV_COUTFIFO0_AUTO_VBLANK,
			SHRP_F_YUV_COUTFIFO0_AUTO_VBLANK, 0);

	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_COUTFIFO0_ENABLE, 1);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_COUTFIFO0_CONFIG,
			SHRP_F_YUV_COUTFIFO0_DEBUG_EN, 1);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_COUTFIFO0_CONFIG,
			SHRP_F_YUV_COUTFIFO0_VVALID_RISE_AT_FIRST_DATA_EN, 1);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_coutfifo);

void shrp_hw_s_cinfifo(void *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_CINFIFO0_INTERVALS,
			SHRP_F_YUV_CINFIFO0_INTERVAL_HBLANK, HBLANK_CYCLE);

	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_CINFIFO0_ENABLE, 1);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_CINFIFO0_CONFIG,
			SHRP_F_YUV_CINFIFO0_STALL_BEFORE_FRAME_START_EN, 1);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_CINFIFO0_CONFIG,
			SHRP_F_YUV_CINFIFO0_DEBUG_EN, 1);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_cinfifo);

#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
void shrp_hw_s_hf_cinfifo(void *base, u32 set_id, u32 en)
{
	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_STAT_CINFIFODECOMP_INTERVALS,
			SHRP_F_STAT_CINFIFODECOMP_INTERVAL_HBLANK, HBLANK_CYCLE);

	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_STAT_CINFIFODECOMP_ENABLE, en);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_STAT_CINFIFODECOMP_CONFIG,
			SHRP_F_STAT_CINFIFODECOMP_STALL_BEFORE_FRAME_START_EN, 1);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_STAT_CINFIFODECOMP_CONFIG,
			SHRP_F_STAT_CINFIFODECOMP_DEBUG_EN, 1);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_hf_cinfifo);

void shrp_hw_s_hf_coutfifo(void *base, u32 set_id, u32 en)
{
	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_STAT_COUTFIFODECOMP_INTERVALS,
			SHRP_F_STAT_COUTFIFODECOMP_INTERVAL_HBLANK, HBLANK_CYCLE);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_STAT_COUTFIFODECOMP_INTERVAL_VBLANK,
			SHRP_F_STAT_COUTFIFODECOMP_INTERVAL_VBLANK, VBLANK_CYCLE);
	SHRP_SET_F(base, SHRP_R_STAT_COUTFIFODECOMP_AUTO_VBLANK,
			SHRP_F_STAT_COUTFIFODECOMP_AUTO_VBLANK, 0);

	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_STAT_COUTFIFODECOMP_ENABLE, en);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_STAT_COUTFIFODECOMP_CONFIG,
			SHRP_F_STAT_COUTFIFODECOMP_DEBUG_EN, 1);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_STAT_COUTFIFODECOMP_CONFIG,
			SHRP_F_STAT_COUTFIFODECOMP_VVALID_RISE_AT_FIRST_DATA_EN, 1);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_hf_coutfifo);
#endif

void shrp_hw_s_strgen(void *base, u32 set_id)
{
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_CINFIFO0_CONFIG, SHRP_F_YUV_CINFIFO0_STRGEN_MODE_EN, 1);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_CINFIFO0_CONFIG, SHRP_F_YUV_CINFIFO0_STRGEN_MODE_DATA_TYPE, 0);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id),
			SHRP_R_YUV_CINFIFO0_CONFIG, SHRP_F_YUV_CINFIFO0_STRGEN_MODE_DATA, 128);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_IP_USE_CINFIFO_NEW_FRAME_IN, 0);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_strgen);

static void _shrp_hw_s_common(void *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_R(base, SHRP_R_AUTO_IGNORE_INTERRUPT_ENABLE, 1);
}

static void _shrp_hw_s_int_mask(void *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_R(base, SHRP_R_INT_REQ_INT0_ENABLE, INT1_SHRP_EN_MASK);
	SHRP_SET_R(base, SHRP_R_INT_REQ_INT1_ENABLE, INT2_SHRP_EN_MASK);

	SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_L, SHRP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, INT_GRP_SHRP_EN_MASK);
}

static void _shrp_hw_s_secure_id(void *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	/*
	 * Set Paramer Value
	 *
	 * scenario
	 * 0: Non-secure,  1: Secure
	 * TODO: get secure scenario
	 */
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_SECU_CTRL_SEQID, 0);
}

static void _shrp_hw_s_fro(void *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);
}

void shrp_hw_s_core(void *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	_shrp_hw_s_common(base);
	_shrp_hw_s_int_mask(base);
	_shrp_hw_s_secure_id(base, set_id);
	_shrp_hw_s_fro(base, num_buffers, set_id);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_core);

void shrp_hw_dma_dump(struct is_common_dma *dma)
{
	dbg_hw(4, "[API] %s\n", __func__);

	CALL_DMA_OPS(dma, dma_print_info, 0);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_dma_dump);

int shrp_hw_s_rdma_init(struct is_common_dma *dma, struct shrp_param_set *param_set,
	u32 enable, u32 cache_hint, u32 *sbwc_en, u32 *payload_size)
{
	int ret = SET_SUCCESS;
	struct param_dma_input *dma_input = NULL;
	u32 comp_sbwc_en, comp_64b_align, quality_control = 0;
	u32 stride_1p, header_stride_1p;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 format, en_votf, bus_info;
	u32 strip_enable;
	bool img_flag = true;
	struct comp_block_size {
		u32 width;
		u32 height;
	} comp_block = { 0, };

	dbg_hw(4, "[API] %s\n", __func__);

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	switch (dma->id) {
	case SHRP_RDMA_HF:
		dma_input = &param_set->dma_input_hf;
		width = dma_input->width;
		height = dma_input->height;
		comp_block.width = SHRP_HF_COMP_BLOCK_WIDTH;
		comp_block.height = SHRP_HF_COMP_BLOCK_HEIGHT;
		img_flag = false;
		break;
	case SHRP_RDMA_SEG:
		dma_input = &param_set->dma_input_seg;
		width = dma_input->width;
		height = dma_input->height;
		comp_block.width = SHRP_COMP_BLOCK_WIDTH;
		comp_block.height = SHRP_COMP_BLOCK_HEIGHT;
		img_flag = false;
		break;
	default:
		err_hw("[SHRP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	en_votf = dma_input->v_otf_enable;
	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;
	bus_info = en_votf ? (cache_hint << 4) : 0x00000000;  /* cache hint [6:4] */

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (!is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en,
						true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 0)
		stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
				width, 16, img_flag);
	else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width,
							comp_64b_align, quality_control,
							comp_block.width,
							comp_block.height);
		header_stride_1p = is_hw_dma_get_header_stride(width, comp_block.width, 16);
	} else {
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + comp_block.height - 1) / comp_block.height) * stride_1p;
		break;
	default:
		break;
	}

	return ret;
}

int shrp_hw_rdma_create(struct is_common_dma *dma, void *base, u32 input_id)
{
	int ret = SET_SUCCESS;
#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
	ulong available_bayer_format_map;
	char *name;

	name = __getname();
	if (!name) {
		err_hw("[SHRP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (input_id) {
	case SHRP_RDMA_HF:
		dma->reg_ofs = SHRP_R_STAT_RDMAHFRECOMP_ENABLE;
		dma->field_ofs = SHRP_F_STAT_RDMAHFRECOMP_EN;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "SHRP_RDMA_HF");
		break;
	case SHRP_RDMA_SEG:
		dma->reg_ofs = SHRP_R_STAT_RDMASEG_EN;
		dma->field_ofs = SHRP_F_STAT_RDMASEG_EN;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "SHRP_RDMA_SEG");
		break;
	default:
		err_hw("[SHRP] invalid input_id[%d]", input_id);
		ret = SET_ERROR;
		goto func_exit;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

func_exit:
	__putname(name);
#endif

	return ret;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_rdma_create);

int shrp_hw_get_input_dva(u32 id, u32 *cmd, pdma_addr_t **input_dva,
	struct shrp_param_set *param_set)
{
	switch (id) {
	case SHRP_RDMA_HF:
		*input_dva = param_set->input_dva_hf;
		*cmd = param_set->dma_input_hf.cmd;
		break;
	case SHRP_RDMA_SEG:
		*input_dva = param_set->input_dva_seg;
		*cmd = param_set->dma_input_seg.cmd;
		break;
	default:
		return -1;
	}

	return 0;
}

int shrp_hw_get_rdma_cache_hint(u32 id, u32 *cache_hint)
{
	switch (id) {
	case SHRP_RDMA_HF:
	case SHRP_RDMA_SEG:
		*cache_hint = 0x7;	/* 111: last-access-read */
		return 0;
	default:
		return -1;
	}
}

void shrp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_rdma_corex_id);

int shrp_hw_s_rdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane,
	u32 num_buffers, int buf_idx, u32 comp_sbwc_en, u32 payload_size)
{
	int ret, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	ret = SET_SUCCESS;

	dbg_hw(4, "[API] %s (%d 0x%8llx 0x%8llx)\n", __func__, dma->id,
			(unsigned long long)addr[0], (unsigned long long)addr[1]);

	switch (dma->id) {
	case SHRP_RDMA_HF:
	case SHRP_RDMA_SEG:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + i);
		ret = CALL_DMA_OPS(dma, dma_set_img_addr,
			(dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[SHRP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en) {
		/* Lossless, Lossy need to set header base address */
		switch (dma->id) {
		case SHRP_RDMA_HF:
		case SHRP_RDMA_SEG:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = address[i] + payload_size;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr,
			plane, buf_idx, num_buffers);
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_rdma_addr);

int shrp_hw_s_corex_update_type(void *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return 0;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_corex_update_type);

void shrp_hw_s_cmdq(void *base, u32 set_id, u32 num_buffers, dma_addr_t clh, u32 noh)
{
	int i;
	u32 fro_en = num_buffers > 1 ? 1 : 0;

	dbg_hw(4, "[API] %s\n", __func__);

	if (fro_en)
		SHRP_SET_R(base, SHRP_R_CMDQ_ENABLE, 0);

	for (i = 0; i < num_buffers; ++i) {
		if (i == 0)
			SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_L, SHRP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE,
				fro_en ? INT_GRP_SHRP_EN_MASK_FRO_FIRST : INT_GRP_SHRP_EN_MASK);
		else if (i < num_buffers - 1)
			SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_L, SHRP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE,
				INT_GRP_SHRP_EN_MASK_FRO_MIDDLE);
		else
			SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_L, SHRP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE,
				INT_GRP_SHRP_EN_MASK_FRO_LAST);

		if (clh && noh) {
			SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_H, SHRP_F_CMDQ_QUE_CMD_BASE_ADDR, DVA_36BIT_HIGH(clh));
			SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_M, SHRP_F_CMDQ_QUE_CMD_HEADER_NUM, noh);
			SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_M, SHRP_F_CMDQ_QUE_CMD_SETTING_MODE, 1);
		} else {
			SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_M, SHRP_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
		}

		SHRP_SET_F(base, SHRP_R_CMDQ_QUE_CMD_L, SHRP_F_CMDQ_QUE_CMD_FRO_INDEX, i);
		SHRP_SET_R(base, SHRP_R_CMDQ_ADD_TO_QUEUE_0, 1);
	}
	SHRP_SET_R(base, SHRP_R_CMDQ_ENABLE, 1);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_cmdq);

unsigned int shrp_hw_g_int_state0(void *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src_all;

	src_all = SHRP_GET_R(base, SHRP_R_INT_REQ_INT0);

	dbg_hw(4, "[API] %s, src_all(%08x)\n", __func__, src_all);

	if (clear)
		SHRP_SET_R(base, SHRP_R_INT_REQ_INT0_CLEAR, src_all);

	*irq_state = src_all;

	return src_all;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_g_int_state0);

unsigned int shrp_hw_g_int_state1(void *base, bool clear, u32 num_buffers, u32 *irq_state)
{
	u32 src_all;

	src_all = SHRP_GET_R(base, SHRP_R_INT_REQ_INT1);

	dbg_hw(4, "[API] %s, src_all(%08x)\n", __func__, src_all);

	if (clear)
		SHRP_SET_R(base, SHRP_R_INT_REQ_INT1_CLEAR, src_all);

	*irq_state = src_all;

	return src_all;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_g_int_state1);

unsigned int shrp_hw_g_int_mask0(void *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return SHRP_GET_R(base, SHRP_R_INT_REQ_INT0_ENABLE);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_g_int_mask0);

void shrp_hw_s_int_mask0(void *base, u32 mask)
{
	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_R(base, SHRP_R_INT_REQ_INT0_ENABLE, mask);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_int_mask0);

unsigned int shrp_hw_g_int_mask1(void *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	return SHRP_GET_R(base, SHRP_R_INT_REQ_INT1_ENABLE);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_g_int_mask1);

void shrp_hw_s_block_bypass(void *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_DITHER_BYPASS, 1);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_GAMMAOETF_BYPASS, 1);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_BYPASS, 1);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_block_bypass);

int shrp_hw_s_sharpen_size(void *base, u32 set_id, u32 shrp_strip_start_pos, u32 frame_width,
	u32 width, u32 height, u32 strip_enable, struct shrp_radial_cfg *radial_cfg)
{
	u32 val;
	u32 offset_x, offset_y, step_x, step_y, start_crop_x, start_crop_y;
	int binning_x, binning_y, radial_center_x, radial_center_y;
	u32 sensor_center_x, sensor_center_y;

	step_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x * radial_cfg->bns_binning_x *
			256ULL * radial_cfg->taa_crop_width / frame_width / 1000 / 1000 / 1000;
	step_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y * radial_cfg->bns_binning_y *
			256ULL * radial_cfg->taa_crop_height / height / 1000 / 1000 / 1000;

	offset_x = radial_cfg->sensor_crop_x + (radial_cfg->bns_binning_x *
			(radial_cfg->taa_crop_x + shrp_strip_start_pos) / 1000);
	offset_y = radial_cfg->sensor_crop_y +
			(radial_cfg->bns_binning_y * radial_cfg->taa_crop_y / 1000);

	start_crop_x = radial_cfg->sensor_binning_x * offset_x / 1000;
	start_crop_y = radial_cfg->sensor_binning_y * offset_y / 1000;

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x *
			radial_cfg->sw_binning_x * 1024ULL *
			radial_cfg->taa_crop_width / frame_width / 1000 / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y *
			radial_cfg->sw_binning_y * 1024ULL *
			radial_cfg->taa_crop_height / height / 1000 / 1000 / 1000;

	sensor_center_x = (radial_cfg->sensor_full_width >> 1) & (~0x1);
	sensor_center_y = (radial_cfg->sensor_full_height >> 1) & (~0x1);

	radial_center_x = -sensor_center_x + radial_cfg->sensor_binning_x * offset_x / 1000;
	radial_center_y = -sensor_center_y + radial_cfg->sensor_binning_y * offset_y / 1000;

	dbg_hw(4, "[API]%s: sensor (%dx%d) img(%dx%d)\n", __func__,
			radial_cfg->sensor_full_width,
			radial_cfg->sensor_full_height,
			width, height);
	dbg_hw(4, "[API]%s: start_crop_x %d, start_crop_y %d\n", __func__,
			start_crop_x, start_crop_y);
	dbg_hw(4, "[API]%s: step_x %d, step_y %d\n", __func__, step_x, step_y);
	dbg_hw(4, "[API]%s: binning_x %d, binning_y %d\n", __func__, binning_x, binning_y);
	dbg_hw(4, "[API]%s: radial_center_x %d, radial_center_y %d\n", __func__,
		radial_center_x, radial_center_y);

	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHARPENHANCER_SENSOR_WIDTH,
		radial_cfg->sensor_full_width);
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHARPENHANCER_SENSOR_HEIGHT,
		radial_cfg->sensor_full_height);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_SENSOR, val);
	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHARPENHANCER_START_CROP_X, start_crop_x);
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHARPENHANCER_START_CROP_Y, start_crop_y);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_START_CROP, val);
	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHARPENHANCER_STEP_X, step_x);
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHARPENHANCER_STEP_Y, step_y);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_STEP, val);

#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHRPRECOMP_RADIAL_CENTER_X, radial_center_x);
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHRPRECOMP_RADIAL_CENTER_Y, radial_center_y);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHRPRECOMP_RADIAL_CENTER, val);

	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHRPRECOMP_RADIAL_BINNING_X, binning_x);
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHRPRECOMP_RADIAL_BINNING_Y, binning_y);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHRPRECOMP_RADIAL_BINNING, val);

	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_SHARPEN_IMG_WIDTH, width);
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_SHARPEN_IMG_HEIGHT, height);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_SHARPEN_IMG_SIZE, val);
#endif

	return 0;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_sharpen_size);

int shrp_hw_s_strip(void *base, u32 set_id, u32 strip_enable, u32 strip_start_pos,
	enum shrp_strip_type type, u32 left, u32 right, u32 full_width)
{
	if (!strip_enable)
		strip_start_pos = 0x0;

	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_STRIP_TYPE, type);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_STRIP_START_POSITION,
		SHRP_F_CHAIN_STRIP_START_POSITION, strip_start_pos);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_STRIP_START_POSITION,
		SHRP_F_CHAIN_FULL_FRAME_WIDTH, full_width);

	dbg_hw(4, "[API] %s: type(%d), start_pos(%d), full_width(%d)\n", __func__,
			type, strip_start_pos, full_width);

#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
#else
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_STRIP_OVERLAP_SIZE,
		SHRP_F_CHAIN_STRIP_OVERLAP_LEFT, left);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_STRIP_OVERLAP_SIZE,
		SHRP_F_CHAIN_STRIP_OVERLAP_RIGHT, right);
#endif

	return 0;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_strip);

void shrp_hw_s_otf_input_size(void *base, u32 set_id, u32 width, u32 height)
{
	u32 val;

	dbg_hw(4, "[API] %s (%d x %d)\n", __func__, width, height);

	val = 0;
#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_SRC_IMG_WIDTH, width);
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_SRC_IMG_HEIGHT, height);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_SRC_IMG_SIZE, val);
#else
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_IMG_WIDTH, width);
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_IMG_HEIGHT, height);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_IMG_SIZE, val);
#endif
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_otf_input_size);

#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
void shrp_hw_s_otf_output_size(void *base, u32 set_id, u32 width, u32 height)
{
	u32 val;

	dbg_hw(4, "[API] %s (%d x %d)\n", __func__, width, height);

	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_DST_IMG_WIDTH, width);
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_DST_IMG_HEIGHT, height);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_DST_IMG_SIZE, val);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_otf_output_size);

static void __shrp_hw_s_disable_content_aware(void *base, u32 set_id)
{
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_CONTENTS_AWARE_EN,
			SHRP_F_YUV_SHARPENHANCER_CONTENTS_AWARE_ISP_EN, 0);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_CONTROL_CONFIG,
			SHRP_F_YUV_SHARPENHANCER_CONTD_OUT_SELECT_0, 0);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_CONTROL_CONFIG,
			SHRP_F_YUV_SHARPENHANCER_CONTD_OUT_SELECT_1, 0);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_CONTROL_CONFIG,
			SHRP_F_YUV_SHARPENHANCER_CONTD_OUT_SELECT_2, 0);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_CONTROL_CONFIG,
			SHRP_F_YUV_SHARPENHANCER_CONTD_OUT_SELECT_3, 0);
	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHARPENHANCER_CONTROL_CONFIG,
			SHRP_F_YUV_SHARPENHANCER_CONTD_OUT_SELECT_4, 0);
}

void shrp_hw_s_yuvsc_size(void *base, u32 set_id, u32 in_width, u32 in_height,
		u32 out_width, u32 out_height)
{
	u32 val;
	u32 h_ratio;
	u32 v_ratio;

	dbg_hw(4, "[API] %s in(%d x %d) out(%d x %d)\n", __func__,
			in_width, in_height, out_width, out_height);

	if (out_width == 0 || out_height == 0) {
		err_hw("Invalid output size (%dx%d)", out_width, out_height);
		return;
	}

	h_ratio = ((unsigned long long)in_width * 0x100000) / out_width;
	v_ratio = ((unsigned long long)in_height * 0x100000) / out_height;

	dbg_hw(4, "[API] %s h_ratio(0x%x), v_ratio(0x%x)",
			__func__, h_ratio, v_ratio);

	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SCRECOMP_BYPASS, 0);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SCRECOMP_CTRL0, val);

	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SCRECOMP_DST_HSIZE, out_width);
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SCRECOMP_DST_VSIZE, out_height);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SCRECOMP_DST_SIZE, val);

	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SCRECOMP_H_RATIO, h_ratio);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SCRECOMP_V_RATIO, v_ratio);

	if (h_ratio < 0x100000 || v_ratio < 0x100000) {
		dbg_hw(4, "[API] %s mux select SHRP_MUX_RECOMP_ENABLE_IN", __func__);
		val = SHRP_GET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_MUX_SELECT);
		val |= SHRP_MUX_RECOMP_ENABLE_IN;
		SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_MUX_SELECT, val);

		__shrp_hw_s_disable_content_aware(base, set_id);
	}
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_yuvsc_size);

void shrp_hw_s_hf_size(void *base, u32 set_id, u32 width, u32 height)
{
	u32 val;

	dbg_hw(4, "[API] %s (%d x %d)\n", __func__, width, height);

	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_HF_WIDTH, width);
	val = SHRP_SET_V(base, val, SHRP_F_CHAIN_HF_HEIGHT, height);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_HF_SIZE, val);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_hf_size);

void shrp_hw_s_recomp_size(void *base, u32 set_id, u32 width, u32 height)
{
	u32 val;

	dbg_hw(4, "[API] %s (%d x %d)\n", __func__, width, height);

	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHRPRECOMP_INPUT_IMG_VSIZE, height);
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHRPRECOMP_INPUT_IMG_HSIZE, width);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHRPRECOMP_IMG_SIZE, val);

	val = 0;
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHRPRECOMP_IN_CROP_SIZE_V, height);
	val = SHRP_SET_V(base, val, SHRP_F_YUV_SHRPRECOMP_IN_CROP_SIZE_H, width);
	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHRPRECOMP_IN_CROP_SIZE, val);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_recomp_size);

void shrp_hw_s_shrprecomp_bypass(void *base, u32 set_id, u32 bypass)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, bypass);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_YUV_SHRPRECOMP_CTRL, SHRP_F_YUV_SHRPRECOMP_BYPASS, bypass);

}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_shrprecomp_bypass);
#endif

void shrp_hw_s_otf_input_path(void *base, u32 set_id, u32 otf_en)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, otf_en);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_IP_USE_OTF_PATH_01, SHRP_F_IP_USE_OTF_IN_FOR_PATH_0, otf_en);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_otf_input_path);

void shrp_hw_s_otf_output_path(void *base, u32 set_id, u32 otf_en)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, otf_en);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_IP_USE_OTF_PATH_01, SHRP_F_IP_USE_OTF_OUT_FOR_PATH_0, otf_en);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_otf_output_path);

#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
void shrp_hw_s_hf_otf_input_path(void *base, u32 set_id, u32 otf_en)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, otf_en);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_IP_USE_OTF_PATH_01, SHRP_F_IP_USE_OTF_IN_FOR_PATH_1, otf_en);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_hf_otf_input_path);

void shrp_hw_s_hf_otf_output_path(void *base, u32 set_id, u32 otf_en)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, otf_en);

	SHRP_SET_F(base + GET_COREX_OFFSET(set_id), SHRP_R_IP_USE_OTF_PATH_01, SHRP_F_IP_USE_OTF_OUT_FOR_PATH_1, otf_en);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_hf_otf_output_path);
#endif

void shrp_hw_s_mux_select(void *base, u32 set_id, enum shrp_mux_select type)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, type);

	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_MUX_SELECT, type);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_mux_select);

#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
void shrp_hw_s_demux_select(void *base, u32 set_id, enum shrp_demux_select type)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, type);

	SHRP_SET_R(base + GET_COREX_OFFSET(set_id), SHRP_R_CHAIN_DEMUX_SELECT, type);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_demux_select);
#endif

u32 shrp_hw_g_rdma_max_cnt(void)
{
	return SHRP_RDMA_MAX;
}

u32 shrp_hw_g_wdma_max_cnt(void)
{
	return SHRP_WDMA_MAX;
}

u32 shrp_hw_g_reg_cnt(void)
{
	return SHRP_REG_CNT + SHRP_LUT_REG_CNT * SHRP_LUT_NUM;
}
KUNIT_EXPORT_SYMBOL(shrp_hw_g_reg_cnt);

void shrp_hw_s_crc(void *base, u32 seed)
{
	SHRP_SET_F(base, SHRP_R_YUV_CINFIFO0_STREAM_CRC,
			SHRP_F_YUV_CINFIFO0_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_RGB_RGBTOYUV_STREAM_CRC,
			SHRP_F_RGB_RGBTOYUV_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_YUV444TO422_STREAM_CRC,
			SHRP_F_YUV_YUV444TO422_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_DITHER_STREAM_CRC,
			SHRP_F_YUV_DITHER_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_GAMMAOETF_STREAM_CRC,
			SHRP_F_YUV_GAMMAOETF_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_SHARPENHANCER_SHARPEN_STREAM_CRC,
			SHRP_F_YUV_SHARPENHANCER_SHARPEN_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_SHARPENHANCER_HFMIXER_STREAM_CRC,
			SHRP_F_YUV_SHARPENHANCER_HFMIXER_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_SHARPENHANCER_NOISEGEN_STREAM_CRC,
			SHRP_F_YUV_SHARPENHANCER_NOISEGEN_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_SHARPENHANCER_NOISEMIXER_STREAM_CRC,
			SHRP_F_YUV_SHARPENHANCER_NOISEMIXER_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_SHARPENHANCER_CONTDET_STREAM_CRC,
			SHRP_F_YUV_SHARPENHANCER_CONTDET_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_SHARPENHANCER_STREAM_CRC,
			SHRP_F_YUV_SHARPENHANCER_CRC_SEED, seed);

#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
	SHRP_SET_F(base, SHRP_R_YUV_SHARPENHANCER_STREAM_CRC,
			SHRP_F_YUV_SHARPENHANCER_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_STAT_CINFIFODECOMP_STREAM_CRC,
			SHRP_F_STAT_CINFIFODECOMP_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_COUTFIFO0_STREAM_CRC,
			SHRP_F_YUV_COUTFIFO0_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_STAT_COUTFIFODECOMP_STREAM_CRC,
			SHRP_F_STAT_COUTFIFODECOMP_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_YUV444TO422_STREAM_CRC,
			SHRP_F_YUV_YUV444TO422_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_SCRECOMP_STREAM_CRC,
			SHRP_F_YUV_SCRECOMP_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_SHRPRECOMP_STREAM_CRC,
			SHRP_F_YUV_SHRPRECOMP_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_YUV422TO444RECOMP_STREAM_CRC,
			SHRP_F_YUV_YUV422TO444RECOMP_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_YUVTORGBSHARPEN_STREAM_CRC,
			SHRP_F_YUV_YUVTORGBSHARPEN_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_RGB_GAMMARGBSHARPEN_STREAM_CRC,
			SHRP_F_RGB_GAMMARGBSHARPEN_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_RGB_RGBTOYUVSHARPEN_STREAM_CRC,
			SHRP_F_RGB_RGBTOYUVSHARPEN_CRC_SEED, seed);
	SHRP_SET_F(base, SHRP_R_YUV_YUV444TO422RECOMP_STREAM_CRC,
			SHRP_F_YUV_YUV444TO422RECOMP_CRC_SEED, seed);
#endif
}
KUNIT_EXPORT_SYMBOL(shrp_hw_s_crc);

void shrp_hw_init_pmio_config(struct pmio_config *cfg)
{
	cfg->num_corexs = 2;
	cfg->corex_stride = 0x8000;

	cfg->volatile_table = &shrp_volatile_ranges_table;
	cfg->wr_noinc_table = &shrp_wr_noinc_ranges_table;

	cfg->max_register = SHRP_R_YUV_SHARPENHANCER_STREAM_CRC;
	cfg->num_reg_defaults_raw = (SHRP_R_YUV_SHARPENHANCER_STREAM_CRC >> 2) + 1;
	cfg->dma_addr_shift = 4;

	cfg->ranges = shrp_range_cfgs;
	cfg->num_ranges = ARRAY_SIZE(shrp_range_cfgs);

	cfg->fields = shrp_field_descs;
	cfg->num_fields = ARRAY_SIZE(shrp_field_descs);
}
KUNIT_EXPORT_SYMBOL(shrp_hw_init_pmio_config);
