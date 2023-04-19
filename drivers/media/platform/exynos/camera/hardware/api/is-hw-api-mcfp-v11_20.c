// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * MCFP HW control APIs
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include "is-hw-api-mcfp-v11.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-mcfp-v11_20.h"

#define MCFP_SET_F(base, R, F, val)		PMIO_SET_F(base, R, F, val)
#define MCFP_SET_R(base, R, val)		PMIO_SET_R(base, R, val)
#define MCFP_SET_V(base, reg_val, F, val)	PMIO_SET_V(base, reg_val, F, val)
#define MCFP_GET_F(base, R, F)			PMIO_GET_F(base, R, F)
#define MCFP_GET_R(base, R)			PMIO_GET_R(base, R)

#define VBLANK_CYCLE			(0xA)
#define HBLANK_CYCLE			(0x2E)
#define PBLANK_CYCLE			(0)

static void __mcfp_hw_wait_corex_idle(struct pablo_mmio *base)
{
	u32 busy;
	u32 try_cnt = 0;

	do {
		udelay(1);

		try_cnt++;
		if (try_cnt >= MCFP_TRY_COUNT) {
			err_hw("[MCFP] fail to wait corex idle");
			break;
		}

		busy = MCFP_GET_F(base, MCFP_R_COREX_STATUS_0, MCFP_F_COREX_BUSY_0);
		dbg_hw(1, "[MCFP] %s busy(%d)\n", __func__, busy);

	} while (busy);
}

static void __mcfp_hw_s_int_mask(struct pablo_mmio *base)
{
	MCFP_SET_F(base, MCFP_R_INT_REQ_INT0_ENABLE, MCFP_F_INT_REQ_INT0_ENABLE, MCFP_INT0_EN_MASK);
}

/*
 * Context: O
 * CR type: Shadow/Dual
 */
static void __mcfp_hw_s_secure_id(struct pablo_mmio *base, u32 set_id)
{
	/*
	 * Set Paramer Value
	 *
	 * scenario
	 * 0: Non-secure,  1: Secure
	 */
	MCFP_SET_F(base,
			MCFP_R_SECU_CTRL_SEQID,
			MCFP_F_SECU_CTRL_SEQID, 0); /* TODO: get secure scenario */
}

u32 mcfp_hw_is_occurred(unsigned int status, enum mcfp_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR0_MCFP_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR0_MCFP_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR0_MCFP_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR0_MCFP_COREX_END_INT_1;
		break;
	case INTR_SETTING_DONE:
		mask = 1 << INTR0_MCFP_SETTING_DONE_INT;
		break;
	case INTR_ERR:
		mask = MCFP_INT0_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

int mcfp_hw_s_reset(struct pablo_mmio *base)
{
	u32 reset_count = 0;

	MCFP_SET_R(base, MCFP_R_YUV_CINFIFO_OTF_ENABLE, 0);
	MCFP_SET_R(base, MCFP_R_YUV_CINFIFO_NR_OTF_ENABLE, 0);
	MCFP_SET_R(base, MCFP_R_YUV_COUTFIFO_CLEAN_ENABLE, 0);

	MCFP_SET_F(base, MCFP_R_CMDQ_LOCK, MCFP_F_CMDQ_POP_LOCK, 1);
	MCFP_SET_F(base, MCFP_R_CMDQ_LOCK, MCFP_F_CMDQ_RELOAD_LOCK, 1);

	MCFP_SET_R(base, MCFP_R_SW_RESET, 0x1);

	while (MCFP_GET_R(base, MCFP_R_SW_RESET)) {
		if (reset_count > MCFP_TRY_COUNT)
			return -ENODEV;
		reset_count++;
	}

	MCFP_SET_F(base, MCFP_R_CMDQ_LOCK, MCFP_F_CMDQ_POP_LOCK, 0);
	MCFP_SET_F(base, MCFP_R_CMDQ_LOCK, MCFP_F_CMDQ_RELOAD_LOCK, 0);

	info_hw("[MCFP] %s done.\n", __func__);

	return 0;
}

void mcfp_add_to_queue(struct pablo_mmio *base, u32 queue_num)
{
	switch (queue_num) {
	case 0:
		MCFP_SET_F(base, MCFP_R_CMDQ_ADD_TO_QUEUE_0, MCFP_F_CMDQ_ADD_TO_QUEUE_0, 1);
		break;
	case 1:
		MCFP_SET_F(base, MCFP_R_CMDQ_ADD_TO_QUEUE_1, MCFP_F_CMDQ_ADD_TO_QUEUE_1, 1);
		break;
	case 2:
		MCFP_SET_F(base, MCFP_R_CMDQ_ADD_TO_QUEUE_URGENT, MCFP_F_CMDQ_ADD_TO_QUEUE_URGENT, 1);
		break;
	default:
		warn_hw("invalid queue num(%d) for MCFP api\n", queue_num);
		break;
	}
}

void mcfp_hw_s_init(struct pablo_mmio *base)
{
	MCFP_SET_F(base, MCFP_R_CMDQ_VHD_CONTROL, MCFP_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE, 1);
	MCFP_SET_F(base, MCFP_R_CMDQ_VHD_CONTROL, MCFP_F_CMDQ_VHD_VBLANK_QRUN_ENABLE, 1);
	MCFP_SET_F(base, MCFP_R_AUTO_IGNORE_INTERRUPT_ENABLE, MCFP_F_AUTO_IGNORE_INTERRUPT_ENABLE, 0);
	MCFP_SET_F(base, MCFP_R_AUTO_IGNORE_PREADY_ENABLE, MCFP_F_AUTO_IGNORE_PREADY_ENABLE, 0);
	MCFP_SET_F(base, MCFP_R_IP_USE_SW_FINISH_COND, MCFP_F_IP_USE_SW_FINISH_COND, 0);
	MCFP_SET_F(base, MCFP_R_SW_FINISH_COND_ENABLE, MCFP_F_SW_FINISH_COND_ENABLE, 0);
	MCFP_SET_F(base, MCFP_R_IP_CORRUPTED_COND_ENABLE, MCFP_F_IP_CORRUPTED_COND_ENABLE, 0);
	MCFP_SET_R(base, MCFP_R_C_LOADER_ENABLE, 1);
	MCFP_SET_R(base, MCFP_R_YUV_RDMACL_EN, 1);
	MCFP_SET_R(base, MCFP_R_DEBUG_CLOCK_ENABLE, 0); /* for debugging */
}

void mcfp_hw_s_clock(struct pablo_mmio *base, bool on)
{
	dbg_hw(5, "[MCFP] clock %s", on ? "on" : "off");
	MCFP_SET_F(base, MCFP_R_IP_PROCESSING, MCFP_F_IP_PROCESSING, on);
}
KUNIT_EXPORT_SYMBOL(mcfp_hw_s_clock);

int mcfp_hw_wait_idle(struct pablo_mmio *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int0_all, int1_all;
	u32 try_cnt = 0;

	idle = MCFP_GET_F(base, MCFP_R_IDLENESS_STATUS, MCFP_F_IDLENESS_STATUS);
	int0_all = MCFP_GET_R(base, MCFP_R_INT_REQ_INT0);
	int1_all = MCFP_GET_R(base, MCFP_R_INT_REQ_INT1);

	info_hw("[MCFP] idle status before disable (idle:%d, int:0x%X, 0x%X)\n",
			idle, int0_all, int1_all);

	while (!idle) {
		idle = MCFP_GET_F(base, MCFP_R_IDLENESS_STATUS, MCFP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= MCFP_TRY_COUNT) {
			err_hw("[MCFP] timeout waiting idle - disable fail");
			mcfp_hw_dump(base);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int0_all = MCFP_GET_R(base, MCFP_R_INT_REQ_INT0);

	info_hw("[MCFP] idle status after disable (idle:%d, int:0x%X, 0x%X)\n",
			idle, int0_all, int1_all);

	return ret;
}

void mcfp_hw_s_core(struct pablo_mmio *base, u32 set_id)
{
	__mcfp_hw_s_int_mask(base);
	__mcfp_hw_s_secure_id(base, set_id);
}

void mcfp_hw_dump(void __iomem *base)
{
	info_hw("[MCFP] SFR DUMP (v10.0)\n");
	if (IS_ENABLED(MCFP_USE_MMIO))
		is_hw_dump_regs(base, mcfp_regs, MCFP_REG_CNT);
	else
		is_hw_dump_regs(pmio_get_base(base), mcfp_regs, MCFP_REG_CNT);
}
KUNIT_EXPORT_SYMBOL(mcfp_hw_dump);

void mcfp_hw_dma_dump(struct is_common_dma *dma)
{
	CALL_DMA_OPS(dma, dma_print_info, 0);
}

int mcfp_hw_s_rdma_init(struct is_common_dma *dma, struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input,
	u32 frame_width, u32 frame_height,
	u32 *sbwc_en, u32 *payload_size, u32 *strip_offset, u32 *header_offset,
	struct is_mcfp_config *config)
{
	u32 comp_sbwc_en = 0, comp_64b_align = 1;
	u32 lossy_byte32num = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 full_frame_width, full_dma_width;
	u32 format;
	u32 strip_offset_in_pixel = 0, strip_offset_in_byte = 0, strip_header_offset_in_byte = 0;
	u32 strip_enable = (stripe_input->total_count < 2) ? 0 : 1;
	u32 strip_index = stripe_input->index;
	u32 strip_start_pos = (strip_index) ? (stripe_input->start_pos_x) : 0;
	u32 mv_width, mv_height;
	int ret = SET_SUCCESS;
	bool is_image = true;

	if (!dma) {
		err_hw("[MCFP] dma is null");
		return 0;
	}

	ret = CALL_DMA_OPS(dma, dma_enable, dma_input->cmd);

	if (dma_input->cmd == 0)
		return 0;

	full_frame_width = (strip_enable) ? stripe_input->full_width : frame_width;

	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;

	switch (dma->id) {
	case MCFP_RDMA_CUR_IN_Y:
	case MCFP_RDMA_CUR_IN_UV:
		strip_offset_in_pixel = dma_input->dma_crop_offset;
		width = dma_input->dma_crop_width;
		height = frame_height;
		full_dma_width = full_frame_width;
		break;
	case MCFP_RDMA_GEOMATCH_MV:
	case MCFP_RDMA_MIXER_MV:
		mv_width = 32 << config->motion_scale_x;
		mv_height = 16 << config->motion_scale_y;
		width = DIV_ROUND_UP(frame_width, mv_width) * 4;
		height = DIV_ROUND_UP(frame_height, mv_height);
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		strip_offset_in_byte = (strip_start_pos / mv_width) * mv_width / 8;
		full_dma_width = DIV_ROUND_UP(full_frame_width, mv_width) * 4;
		is_image = false;
		break;
	case MCFP_RDMA_CUR_DRC:
	case MCFP_RDMA_PREV_DRC:
		width = 4 * ((frame_width / 16) + 1);
		height = (frame_height / 16) + 1;
		full_dma_width = 4 * ((full_frame_width / 16) + 1);
		break;
	case MCFP_RDMA_SAT_FLAG:
		width = (frame_width + 7) / 8;
		height = frame_height;
		full_dma_width = (full_frame_width + 7) / 8;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		break;
	case MCFP_RDMA_PREV_IN0_Y:
	case MCFP_RDMA_PREV_IN0_UV:
	case MCFP_RDMA_PREV_IN1_Y:
	case MCFP_RDMA_PREV_IN1_UV:
		width = full_frame_width;
		height = frame_height;
		hwformat = DMA_INOUT_FORMAT_YUV422;
		memory_bitwidth = config->img_bit;
		pixelsize = config->img_bit;

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (dma->id == MCFP_RDMA_PREV_IN0_Y || dma->id == MCFP_RDMA_PREV_IN1_Y) {
			if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
				sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
			else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
				sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;
		} else {
			if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
				sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_32B;
			else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
				sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;
		}

		if (memory_bitwidth == 8)
			lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U8;
		else if (memory_bitwidth == 10)
			lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U10;
		else
			lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U12;

		full_dma_width = full_frame_width;
		break;
	case MCFP_RDMA_PREV_IN_WGT:
		if (config->wgt_bit == 4) {
			if ((full_frame_width / 2) % 4 == 1)
				width = ((full_frame_width / 4 + 2) / 2) * 2;
			else
				width = ((full_frame_width / 4 + 1) / 2) * 2;
		} else {
			width = (full_frame_width / 2 + 1) / 2 * 2;
		}
		height = frame_height / 2;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		full_dma_width = width;
		break;
	case MCFP_RDMA_CUR_IN_WGT:
		return 0;
	default:
		err_hw("[MCFP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (!is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en,
						true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 0) {
		stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
							full_dma_width, 16, is_image);
		if (strip_enable && strip_offset_in_pixel)
			strip_offset_in_byte = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
					hwformat, strip_offset_in_pixel, 16, true);
	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width,
							comp_64b_align, lossy_byte32num,
							MCFP_COMP_BLOCK_WIDTH,
							MCFP_COMP_BLOCK_HEIGHT);
		header_stride_1p = (comp_sbwc_en == 1) ?
			is_hw_dma_get_header_stride(width, MCFP_COMP_BLOCK_WIDTH, 16) : 0;
		if (strip_enable && strip_offset_in_pixel) {
			strip_offset_in_byte = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize,
								strip_offset_in_pixel,
								comp_64b_align, lossy_byte32num,
								MCFP_COMP_BLOCK_WIDTH,
								MCFP_COMP_BLOCK_HEIGHT);
			strip_header_offset_in_byte = (comp_sbwc_en == 1) ?
				is_hw_dma_get_header_stride(strip_offset_in_pixel,
								MCFP_COMP_BLOCK_WIDTH, 0) : 0;
		}
	} else {
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, 0, 0);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + MCFP_COMP_BLOCK_HEIGHT - 1) / MCFP_COMP_BLOCK_HEIGHT) * stride_1p;
		break;
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_rate, lossy_byte32num);
		break;
	default:
		break;
	}

	*strip_offset = strip_offset_in_byte;
	*header_offset = strip_header_offset_in_byte;

	dbg_hw(3, "%s : dma_id %d, st_ofs %d, width %d, height %d, img_ofs %d, header_ofs %d\n",
		__func__, dma->id, strip_start_pos, width, height, *strip_offset, *header_offset);

	return ret;
}
KUNIT_EXPORT_SYMBOL(mcfp_hw_s_rdma_init);

int mcfp_hw_s_wdma_init(struct is_common_dma *dma, struct param_dma_output *dma_output,
	struct param_stripe_input *stripe_input,
	u32 frame_width, u32 frame_height,
	u32 *sbwc_en, u32 *payload_size, u32 *strip_offset, u32 *header_offset,
	struct is_mcfp_config *config)
{
	u32 comp_sbwc_en = 0, comp_64b_align = 1;
	u32 lossy_byte32num = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 full_frame_width, full_dma_width;
	u32 strip_frame_width;
	u32 strip_margin;
	u32 format;
	u32 strip_offset_in_pixel, strip_offset_in_byte = 0, strip_header_offset_in_byte = 0;
	u32 strip_enable = (stripe_input->total_count < 2) ? 0 : 1;
	u32 strip_index = stripe_input->index;
	u32 strip_start_pos = (strip_index) ? stripe_input->start_pos_x : 0;

	int ret = SET_SUCCESS;

	ret = CALL_DMA_OPS(dma, dma_enable, dma_output->cmd);
	if (dma_output->cmd == 0) {
		ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, DMA_OUTPUT_SBWC_DISABLE);
		return ret;
	}

	full_frame_width = (strip_enable) ? stripe_input->full_width : frame_width;
	hwformat = dma_output->format;
	sbwc_type = dma_output->sbwc_type;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;

	switch (dma->id) {
	case MCFP_WDMA_PREV_OUT_Y:
	case MCFP_WDMA_PREV_OUT_UV:
		strip_offset_in_pixel = dma_output->dma_crop_offset_x;
		width = dma_output->dma_crop_width;
		height = frame_height;
		hwformat = DMA_INOUT_FORMAT_YUV422;
		memory_bitwidth = config->img_bit;
		pixelsize = config->img_bit;

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (dma->id == MCFP_WDMA_PREV_OUT_Y) {
			if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
				sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
			else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
				sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;
		} else {
			if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
				sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_32B;
			else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
				sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;
		}

		if (memory_bitwidth == 8)
			lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U8;
		else if (memory_bitwidth == 10)
			lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U10;
		else
			lossy_byte32num = COMP_LOSSYBYTE32NUM_32X4_U12;

		full_dma_width = full_frame_width;
		break;
	case MCFP_WDMA_PREV_OUT_WGT:
		strip_margin = (strip_enable) ?
			stripe_input->left_margin + stripe_input->right_margin : 0;
		strip_frame_width = frame_width - strip_margin;
		width = strip_frame_width;
		if (config->wgt_bit == 4) {
			if ((width / 2) % 4 == 1) {
				width = ((width / 4 + 2) / 2) * 2;
				full_dma_width = ((full_frame_width / 4 + 2) / 2) * 2;
			} else {
				width = ((width / 4 + 1) / 2) * 2;
				full_dma_width = ((full_frame_width / 4 + 1) / 2) * 2;
			}
			strip_offset_in_pixel = (strip_start_pos + stripe_input->left_margin) / 4;
		} else {
			width = (width / 2 + 1) / 2 * 2;
			full_dma_width = (full_frame_width / 2 + 1) / 2 * 2;
			strip_offset_in_pixel = (strip_start_pos + stripe_input->left_margin) / 2;
		}
		height = frame_height / 2;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		break;
	default:
		err_hw("[MCFP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (!is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, comp_sbwc_en,
						true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 0) {
		stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat,
							full_dma_width, 16, true);
		if (strip_enable)
			strip_offset_in_byte = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
					hwformat, strip_offset_in_pixel, 16, true);

	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize,
						full_dma_width,
						comp_64b_align, lossy_byte32num,
						MCFP_COMP_BLOCK_WIDTH,
						MCFP_COMP_BLOCK_HEIGHT);
		header_stride_1p = (comp_sbwc_en == 1) ?
			is_hw_dma_get_header_stride(full_dma_width, MCFP_COMP_BLOCK_WIDTH, 16)
			: 0;

		if (strip_enable) {
			strip_offset_in_byte = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize,
								strip_offset_in_pixel,
								comp_64b_align, lossy_byte32num,
								MCFP_COMP_BLOCK_WIDTH,
								MCFP_COMP_BLOCK_HEIGHT);
			strip_header_offset_in_byte = (comp_sbwc_en == 1) ?
				is_hw_dma_get_header_stride(strip_offset_in_pixel,
								MCFP_COMP_BLOCK_WIDTH, 0) : 0;
		}
	} else {
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, 0, 0);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + MCFP_COMP_BLOCK_HEIGHT - 1) / MCFP_COMP_BLOCK_HEIGHT) * stride_1p;
		break;
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_rate, lossy_byte32num);
		break;
	default:
		break;
	}

	*strip_offset = strip_offset_in_byte;
	*header_offset = strip_header_offset_in_byte;

	dbg_hw(3, "%s : dma_id %d, st_ofs %d, width %d, height %d, img_ofs %d, header_ofs %d\n",
		__func__, dma->id, strip_start_pos, width, height, *strip_offset, *header_offset);

	return ret;
}

int mcfp_hw_rdma_create(struct is_common_dma *dma, void *base, u32 input_id)
{
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char name[64];

	switch (input_id) {
	case MCFP_RDMA_CUR_IN_Y:
		dma->reg_ofs = MCFP_R_YUV_RDMACURRINLUMA_EN;
		dma->field_ofs = MCFP_F_YUV_RDMACURRINLUMA_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_CUR_IN_Y", sizeof(name) - 1);
		break;
	case MCFP_RDMA_CUR_IN_UV:
		dma->reg_ofs = MCFP_R_YUV_RDMACURRINCHROMA_EN;
		dma->field_ofs = MCFP_F_YUV_RDMACURRINCHROMA_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_CUR_IN_UV", sizeof(name) - 1);
		break;
	case MCFP_RDMA_GEOMATCH_MV:
		dma->reg_ofs = MCFP_R_STAT_RDMAMVGEOMATCH_EN;
		dma->field_ofs = MCFP_F_STAT_RDMAMVGEOMATCH_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_GEOMATCH_MV", sizeof(name) - 1);
		break;
	case MCFP_RDMA_SAT_FLAG:
		dma->reg_ofs = MCFP_R_STAT_RDMASATFLGMVMIXER_EN;
		dma->field_ofs = MCFP_F_STAT_RDMASATFLGMVMIXER_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_SAT_FLAG", sizeof(name) - 1);
		break;
	case MCFP_RDMA_CUR_DRC:
		dma->reg_ofs = MCFP_R_STAT_RDMADRCCURR_EN;
		dma->field_ofs = MCFP_F_STAT_RDMADRCCURR_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_CUR_DRC", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_DRC:
		dma->reg_ofs = MCFP_R_STAT_RDMADRCPREV_EN;
		dma->field_ofs = MCFP_F_STAT_RDMADRCPREV_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_DRC", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_IN0_Y:
		dma->reg_ofs = MCFP_R_YUV_RDMAPREVINLUMA_EN;
		dma->field_ofs = MCFP_F_YUV_RDMAPREVINLUMA_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_IN0_Y", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_IN0_UV:
		dma->reg_ofs = MCFP_R_YUV_RDMAPREVINCHROMA_EN;
		dma->field_ofs = MCFP_F_YUV_RDMAPREVINCHROMA_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_IN0_UV", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_IN1_Y:
		dma->reg_ofs = MCFP_R_YUV_RDMAPREVINLUMA1_EN;
		dma->field_ofs = MCFP_F_YUV_RDMAPREVINLUMA1_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_IN1_Y", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_IN1_UV:
		dma->reg_ofs = MCFP_R_YUV_RDMAPREVINCHROMA1_EN;
		dma->field_ofs = MCFP_F_YUV_RDMAPREVINCHROMA1_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_IN1_UV", sizeof(name) - 1);
		break;
	case MCFP_RDMA_PREV_IN_WGT:
		dma->reg_ofs = MCFP_R_STAT_RDMAPREVWGTIN_EN;
		dma->field_ofs = MCFP_F_STAT_RDMAPREVWGTIN_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_RDMA_PREV_IN_WGT", sizeof(name) - 1);
		break;
	/* EVT1 no DMA */
	case MCFP_RDMA_PRE_MV:
	case MCFP_RDMA_MIXER_MV:
	case MCFP_RDMA_CUR_IN_WGT:
		return 0;
	default:
		err_hw("[MCFP] invalid input_id[%d]", input_id);
		return SET_ERROR;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

	return ret;
}
KUNIT_EXPORT_SYMBOL(mcfp_hw_rdma_create);

int mcfp_hw_wdma_create(struct is_common_dma *dma, void *base, u32 input_id)
{
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char name[64];

	switch (input_id) {
	case MCFP_WDMA_PREV_OUT_Y:
		dma->reg_ofs = MCFP_R_YUV_WDMAPREVOUTLUMA_EN;
		dma->field_ofs = MCFP_F_YUV_WDMAPREVOUTLUMA_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_WDMA_PREV_OUT_Y", sizeof(name) - 1);
		break;
	case MCFP_WDMA_PREV_OUT_UV:
		dma->reg_ofs = MCFP_R_YUV_WDMAPREVOUTCHROMA_EN;
		dma->field_ofs = MCFP_F_YUV_WDMAPREVOUTCHROMA_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_WDMA_PREV_OUT_UV", sizeof(name) - 1);
		break;
	case MCFP_WDMA_PREV_OUT_WGT:
		dma->reg_ofs = MCFP_R_STAT_WDMAPREVWGTOUT_EN;
		dma->field_ofs = MCFP_F_STAT_WDMAPREVWGTOUT_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		strncpy(name, "MCFP_WDMA_PREV_OUT_WGT", sizeof(name) - 1);
		break;
	default:
		err_hw("[MCFP] invalid input_id[%d]", input_id);
		return SET_ERROR;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

	return ret;
}

void mcfp_hw_s_dma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int mcfp_hw_s_rdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane,
	u32 num_buffers, int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset,
	u32 header_offset)
{
	int ret = SET_SUCCESS, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case MCFP_RDMA_CUR_IN_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i)) + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MCFP_RDMA_PREV_IN0_Y:
	case MCFP_RDMA_PREV_IN1_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MCFP_RDMA_CUR_IN_UV:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i + 1)) + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MCFP_RDMA_PREV_IN0_UV:
	case MCFP_RDMA_PREV_IN1_UV:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i + 1));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MCFP_RDMA_GEOMATCH_MV:
	case MCFP_RDMA_MIXER_MV:
	case MCFP_RDMA_CUR_DRC:
	case MCFP_RDMA_PREV_DRC:
	case MCFP_RDMA_SAT_FLAG:
	case MCFP_RDMA_PREV_IN_WGT:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + i) + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr,
			(dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	case MCFP_RDMA_CUR_IN_WGT:
		return 0;
	default:
		err_hw("[MCFP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en == 1) {
		/* Lossless, need to set header base address */
		switch (dma->id) {
		case MCFP_RDMA_CUR_IN_Y:
		case MCFP_RDMA_CUR_IN_UV:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = address[i] + payload_size + header_offset;
			break;
		case MCFP_RDMA_PREV_IN0_Y:
		case MCFP_RDMA_PREV_IN0_UV:
		case MCFP_RDMA_PREV_IN1_Y:
		case MCFP_RDMA_PREV_IN1_UV:
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
KUNIT_EXPORT_SYMBOL(mcfp_hw_s_rdma_addr);

int mcfp_hw_s_wdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane,
	u32 num_buffers, int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset,
	u32 header_offset)
{
	int ret = SET_SUCCESS, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case MCFP_WDMA_PREV_OUT_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i)) + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MCFP_WDMA_PREV_OUT_UV:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (2 * i + 1)) + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MCFP_WDMA_PREV_OUT_WGT:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + i) + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr,
			(dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[MCFP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en == 1) {
		/* Lossless, need to set header base address */
		switch (dma->id) {
		case MCFP_WDMA_PREV_OUT_Y:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)*(addr + (2 * i)) +
						 payload_size + header_offset;
			break;
		case MCFP_WDMA_PREV_OUT_UV:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)*(addr + (2 * i + 1)) +
						 payload_size + header_offset;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr,
			plane, buf_idx, num_buffers);
	}

	return ret;
}

void mcfp_hw_s_cmdq_queue(struct pablo_mmio *base, u32 num_buffers, dma_addr_t clh, u32 noh)
{
	int i;
	u32 fro_en = num_buffers > 1 ? 1 : 0;

	if (fro_en)
		MCFP_SET_R(base, MCFP_R_CMDQ_ENABLE, 0);

	for (i = 0; i < num_buffers; ++i) {
		if (i == 0)
			MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_L, MCFP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE,
				fro_en ? MCFP_INT_GRP_EN_MASK_FRO_FIRST : MCFP_INT_GRP_EN_MASK);
		else if (i < num_buffers - 1)
			MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_L, MCFP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE,
				MCFP_INT_GRP_EN_MASK_FRO_MIDDLE);
		else
			MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_L, MCFP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE,
				MCFP_INT_GRP_EN_MASK_FRO_LAST);

		if (clh && noh) {
			MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_H, MCFP_F_CMDQ_QUE_CMD_BASE_ADDR, clh);
			MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_M, MCFP_F_CMDQ_QUE_CMD_HEADER_NUM, noh);
			MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_M, MCFP_F_CMDQ_QUE_CMD_SETTING_MODE, 1);
		} else {
			MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_M, MCFP_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
		}

		MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_L, MCFP_F_CMDQ_QUE_CMD_FRO_INDEX, i);
		mcfp_add_to_queue(base, 0);
	}
	MCFP_SET_R(base, MCFP_R_CMDQ_ENABLE, 1);
}

void mcfp_hw_s_cmdq_init(struct pablo_mmio *base)
{
	/*
	 * Check COREX idleness
	 */
	__mcfp_hw_wait_corex_idle(base);

	MCFP_SET_F(base, MCFP_R_COREX_ENABLE, MCFP_F_COREX_ENABLE, 0);
	MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_H, MCFP_F_CMDQ_QUE_CMD_BASE_ADDR, 0);
	MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_M, MCFP_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_L, MCFP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, MCFP_INT_GRP_EN_MASK);
	MCFP_SET_F(base, MCFP_R_CMDQ_QUE_CMD_L, MCFP_F_CMDQ_QUE_CMD_FRO_INDEX, 0);

	info_hw("[MCFP] %s done.\n", __func__);
}

void mcfp_hw_s_cmdq_enable(struct pablo_mmio *base, u32 enable)
{
	MCFP_SET_F(base, MCFP_R_CMDQ_ENABLE, MCFP_F_CMDQ_ENABLE, enable);

	info_hw("[MCFP] %s done. enable(%d)\n", __func__, enable);
}

unsigned int mcfp_hw_g_int_state(struct pablo_mmio *base, bool clear, u32 num_buffers, u32 irq_index, u32 *irq_state)
{
	u32 src, int_req, int_req_clear;

	switch (irq_index) {
	case MCFP_INTR_0:
		int_req = MCFP_R_INT_REQ_INT0;
		int_req_clear = MCFP_R_INT_REQ_INT0_CLEAR;
		break;
	case MCFP_INTR_1:
		int_req = MCFP_R_INT_REQ_INT1;
		int_req_clear = MCFP_R_INT_REQ_INT1_CLEAR;
		break;
	default:
		err_hw("[MCFP] invalid irq_index[%d]", irq_index);
		return 0;
	}

	src = MCFP_GET_R(base, int_req);

	if (clear)
		MCFP_SET_R(base, int_req_clear, src);

	*irq_state = src;

	return src;
}

unsigned int mcfp_hw_g_int_mask(struct pablo_mmio *base, u32 irq_index)
{
	u32 int_req_enable;

	switch (irq_index) {
	case MCFP_INTR_0:
		int_req_enable = MCFP_R_INT_REQ_INT0_ENABLE;
		break;
	case MCFP_INTR_1:
		int_req_enable = MCFP_R_INT_REQ_INT1_ENABLE;
		break;
	default:
		err_hw("[MCFP] invalid irq_index[%d]", irq_index);
		return 0;
	}

	return MCFP_GET_R(base, int_req_enable);
}

void mcfp_hw_s_block_bypass(struct pablo_mmio *base, u32 set_id)
{
	MCFP_SET_F(base,
		MCFP_R_YUV_GEOMATCH_EN, MCFP_F_YUV_GEOMATCH_EN, 0);
	MCFP_SET_F(base,
		MCFP_R_YUV_GEOMATCH_BYPASS, MCFP_F_YUV_GEOMATCH_BYPASS, 1);
	MCFP_SET_F(base,
		MCFP_R_YUV_GEOMATCH_MATCH_ENABLE, MCFP_F_YUV_GEOMATCH_MATCH_ENABLE, 0);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_ENABLE, MCFP_F_YUV_MIXER_ENABLE, 0);
	MCFP_SET_F(base,
		MCFP_R_YUV_YUV422TO444_BYPASS, MCFP_F_YUV_YUV422TO444_BYPASS, 1);
	MCFP_SET_F(base,
		MCFP_R_YUV_YUVTORGB_BYPASS, MCFP_F_YUV_YUVTORGB_BYPASS, 1);
	MCFP_SET_F(base,
		MCFP_R_YUV_GAMMARGB_BYPASS, MCFP_F_YUV_GAMMARGB_BYPASS, 1);
	MCFP_SET_F(base,
		MCFP_R_YUV_RGBTOYUV_BYPASS, MCFP_F_YUV_RGBTOYUV_BYPASS, 1);
	MCFP_SET_F(base,
		MCFP_R_YUV_YUV444TO422_BYPASS, MCFP_F_YUV_YUV444TO422_BYPASS, 1);
}

void mcfp_hw_s_otf_input(struct pablo_mmio *base, u32 set_id, u32 enable)
{
	MCFP_SET_R(base, MCFP_R_YUV_CINFIFO_OTF_ENABLE, enable);
	MCFP_SET_F(base, MCFP_R_IP_USE_OTF_PATH, MCFP_F_IP_USE_OTF_IN_FOR_PATH_0, enable);
	MCFP_SET_F(base, MCFP_R_IP_USE_CINFIFO_NEW_FRAME_IN, MCFP_F_IP_USE_CINFIFO_NEW_FRAME_IN, enable);
	MCFP_SET_F(base, MCFP_R_YUV_MAIN_CTRL_IN_CURR_ENABLE, MCFP_F_YUV_IN_CURR_IMAGE_TYPE, enable);
	MCFP_SET_F(base, MCFP_R_YUV_MAIN_CTRL_IN_CURR_ENABLE, MCFP_F_YUV_IN_SAT_FLAG_TYPE, enable);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_CONFIG,
		MCFP_F_YUV_CINFIFO_OTF_STALL_BEFORE_FRAME_START_EN, enable);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_CONFIG,
		MCFP_F_YUV_CINFIFO_OTF_AUTO_RECOVERY_EN, 0);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_CONFIG, MCFP_F_YUV_CINFIFO_OTF_DEBUG_EN, 1);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_INTERVAL_VBLANK,
		MCFP_F_YUV_CINFIFO_OTF_INTERVAL_VBLANK, VBLANK_CYCLE);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_INTERVALS,
		MCFP_F_YUV_CINFIFO_OTF_INTERVAL_HBLANK, HBLANK_CYCLE);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_INTERVALS,
		MCFP_F_YUV_CINFIFO_OTF_INTERVAL_PIXEL, PBLANK_CYCLE);
}

void mcfp_hw_s_nr_otf_input(struct pablo_mmio *base, u32 set_id, u32 enable)
{
	MCFP_SET_F(base, MCFP_R_IP_USE_OTF_PATH, MCFP_F_IP_USE_OTF_IN_FOR_PATH_1, enable);
	MCFP_SET_F(base, MCFP_R_YUV_MAIN_CTRL_YUVP_NR_IMG_WDMA_EN, MCFP_F_YUV_YUVP_NR_IMG_WDMA_EN, enable);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_NR_OTF_CONFIG,
		MCFP_F_YUV_CINFIFO_NR_OTF_STALL_BEFORE_FRAME_START_EN, enable);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_NR_OTF_INTERVAL_VBLANK,
		MCFP_F_YUV_CINFIFO_NR_OTF_INTERVAL_VBLANK, VBLANK_CYCLE);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_NR_OTF_INTERVALS,
		MCFP_F_YUV_CINFIFO_NR_OTF_INTERVAL_HBLANK, HBLANK_CYCLE);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_NR_OTF_INTERVALS,
		MCFP_F_YUV_CINFIFO_NR_OTF_INTERVAL_PIXEL, PBLANK_CYCLE);
}

void mcfp_hw_s_otf_output(struct pablo_mmio *base, u32 set_id, u32 enable)
{
	MCFP_SET_R(base, MCFP_R_YUV_COUTFIFO_CLEAN_ENABLE, enable);
	MCFP_SET_F(base, MCFP_R_YUV_COUTFIFO_CLEAN_CONFIG, MCFP_F_YUV_COUTFIFO_CLEAN_VVALID_RISE_AT_FIRST_DATA_EN, 1);
	MCFP_SET_F(base, MCFP_R_YUV_COUTFIFO_CLEAN_CONFIG, MCFP_F_YUV_COUTFIFO_CLEAN_DEBUG_EN, 1);
	MCFP_SET_F(base, MCFP_R_IP_USE_OTF_PATH, MCFP_F_IP_USE_OTF_OUT_FOR_PATH_0, enable);
	MCFP_SET_F(base, MCFP_R_YUV_COUTFIFO_CLEAN_INTERVAL_VBLANK,
		MCFP_F_YUV_COUTFIFO_CLEAN_INTERVAL_VBLANK, VBLANK_CYCLE);
	MCFP_SET_F(base, MCFP_R_YUV_COUTFIFO_CLEAN_INTERVALS,
		MCFP_F_YUV_COUTFIFO_CLEAN_INTERVAL_HBLANK, HBLANK_CYCLE);
	MCFP_SET_F(base, MCFP_R_YUV_COUTFIFO_CLEAN_INTERVALS,
		MCFP_F_YUV_COUTFIFO_CLEAN_INTERVAL_PIXEL, PBLANK_CYCLE);

	/* HW automatically guarantees vblank if set 0 */
	MCFP_SET_F(base, MCFP_R_YUV_COUTFIFO_CLEAN_AUTO_VBLANK, MCFP_F_YUV_COUTFIFO_CLEAN_AUTO_VBLANK, 0);
}

void mcfp_hw_s_input_size(struct pablo_mmio *base, u32 set_id, u32 width, u32 height)
{
	MCFP_SET_F(base, MCFP_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH, MCFP_F_YUV_IN_IMG_SZ_WIDTH, width);
	MCFP_SET_F(base, MCFP_R_YUV_MAIN_CTRL_IN_IMG_SZ_HEIGHT, MCFP_F_YUV_IN_IMG_SZ_HEIGHT, height);
}

void mcfp_hw_s_geomatch_size(struct pablo_mmio *base, u32 set_id,
				u32 frame_width, u32 dma_width, u32 height,
				bool strip_enable, u32 strip_start_pos,
				struct is_mcfp_config *mcfp_config)
{
	u32 mv_width, mv_height;

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_REF_IMG_SIZE,
		MCFP_F_YUV_GEOMATCH_REF_IMG_WIDTH, dma_width);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_REF_IMG_SIZE,
		MCFP_F_YUV_GEOMATCH_REF_IMG_HEIGHT, height);

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_REF_ROI_START,
		MCFP_F_YUV_GEOMATCH_REF_ROI_START_X, 0);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_REF_ROI_START,
		MCFP_F_YUV_GEOMATCH_REF_ROI_START_Y, 0);

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_ROI_SIZE,
		MCFP_F_YUV_GEOMATCH_ROI_SIZE_X, frame_width);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_ROI_SIZE,
		MCFP_F_YUV_GEOMATCH_ROI_SIZE_Y, height);

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_SCH_IMG_SIZE,
		MCFP_F_YUV_GEOMATCH_SCH_IMG_WIDTH, frame_width);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_SCH_IMG_SIZE,
		MCFP_F_YUV_GEOMATCH_SCH_IMG_HEIGHT, height);

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_SCH_ACTIVE_START,
			MCFP_F_YUV_GEOMATCH_SCH_ACTIVE_START_X, strip_start_pos);

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_SCH_ACTIVE_START,
		MCFP_F_YUV_GEOMATCH_SCH_ACTIVE_START_Y, 0);

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_SCH_ACTIVE_SIZE,
		MCFP_F_YUV_GEOMATCH_SCH_ACTIVE_SIZE_X, dma_width);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_SCH_ACTIVE_SIZE,
		MCFP_F_YUV_GEOMATCH_SCH_ACTIVE_SIZE_Y, height);

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_SCH_ROI_START, MCFP_F_YUV_GEOMATCH_SCH_ROI_START_X, 0);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_SCH_ROI_START, MCFP_F_YUV_GEOMATCH_SCH_ROI_START_Y, 0);

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MV_SCALE_SHIFT,
		MCFP_F_YUV_GEOMATCH_MV_SCALE_SHIFT_WIDTH, mcfp_config->motion_scale_x);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MV_SCALE_SHIFT,
		MCFP_F_YUV_GEOMATCH_MV_SCALE_SHIFT_HEIGHT, mcfp_config->motion_scale_y);

	mv_width = 32 << mcfp_config->motion_scale_x;
	mv_height = 16 << mcfp_config->motion_scale_y;

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MV_INPUT_SIZE,
		MCFP_F_YUV_GEOMATCH_MV_INPUT_WIDTH, DIV_ROUND_UP(dma_width, mv_width));
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MV_INPUT_SIZE,
		MCFP_F_YUV_GEOMATCH_MV_INPUT_HEIGHT, DIV_ROUND_UP(height, mv_height));

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MV_SIZE,
		MCFP_F_YUV_GEOMATCH_MV_WIDTH, (dma_width + 31) / 32);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MV_SIZE,
		MCFP_F_YUV_GEOMATCH_MV_HEIGHT, (height + 15) / 16);

	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MV_FRACTION,
		MCFP_F_YUV_GEOMATCH_MV_FRACTION, 1);
}

void mcfp_hw_s_geomatch_bypass(struct pablo_mmio *base, u32 set_id, bool geomatch_bypass)
{
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_BYPASS,
		MCFP_F_YUV_GEOMATCH_BYPASS, geomatch_bypass);
	if (geomatch_bypass)
		MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MATCH_ENABLE,
			MCFP_F_YUV_GEOMATCH_MATCH_ENABLE, 0);
}

void mcfp_hw_s_mixer_size(struct pablo_mmio *base, u32 set_id,
		u32 frame_width, u32 dma_width, u32 height, bool strip_enable, u32 strip_start_pos,
		struct mcfp_radial_cfg *radial_cfg, struct is_mcfp_config *mcfp_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;
	u32 biquad_scale_shift_adder;

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x * 1024ULL *
			radial_cfg->taa_crop_width / frame_width / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y * 1024ULL *
			radial_cfg->taa_crop_height / height / 1000 / 1000;

	sensor_center_x = (radial_cfg->sensor_full_width >> 1) & (~0x1);
	sensor_center_y = (radial_cfg->sensor_full_height >> 1) & (~0x1);

	offset_x = radial_cfg->sensor_crop_x +
			(radial_cfg->bns_binning_x * (radial_cfg->taa_crop_x + strip_start_pos) / 1000);
	offset_y = radial_cfg->sensor_crop_y +
			(radial_cfg->bns_binning_y * radial_cfg->taa_crop_y / 1000);

	radial_center_x = -sensor_center_x + radial_cfg->sensor_binning_x * offset_x / 1000;
	radial_center_y = -sensor_center_y + radial_cfg->sensor_binning_y * offset_y / 1000;

	biquad_scale_shift_adder = mcfp_config->biquad_scale_shift_adder;

	if (biquad_scale_shift_adder >= 8) {
		if (biquad_scale_shift_adder < 10) {
			binning_x = shift_right_round(binning_x, 1);
			binning_y = shift_right_round(binning_y, 1);
			radial_center_x = shift_right_round(radial_center_x, 1);
			radial_center_y = shift_right_round(radial_center_y, 1);
			biquad_scale_shift_adder = biquad_scale_shift_adder - 2;
		} else {
			binning_x = shift_right_round(binning_x, 2);
			binning_y = shift_right_round(binning_y, 2);
			radial_center_x = shift_right_round(radial_center_x, 2);
			radial_center_y = shift_right_round(radial_center_y, 2);
			biquad_scale_shift_adder = biquad_scale_shift_adder - 4;
		}
	}

	MCFP_SET_F(base, MCFP_R_YUV_MAIN_CTRL_RDMA_SHARE_MIXMV_CURSATFLAG,
		MCFP_F_YUV_MAIN_CTRL_RDMA_SHARE_MIXMV_CURSATFLAG, 1);

	MCFP_SET_F(base, MCFP_R_YUV_MIXER_SENSOR_WIDTH, MCFP_F_YUV_MIXER_SENSOR_WIDTH, frame_width);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_SENSOR_HEIGHT, MCFP_F_YUV_MIXER_SENSOR_HEIGHT, height);

	MCFP_SET_F(base, MCFP_R_YUV_MIXER_ROIFUSIONSTARTX, MCFP_F_YUV_MIXER_ROIFUSIONSTARTX, 0);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_ROIFUSIONSTARTX, MCFP_F_YUV_MIXER_ROIFUSIONSTARTY, 0);

	MCFP_SET_F(base, MCFP_R_YUV_MIXER_ROIFUSIONSIZEX,
		MCFP_F_YUV_MIXER_ROIFUSIONSIZEX, dma_width);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_ROIFUSIONSIZEX,
		MCFP_F_YUV_MIXER_ROIFUSIONSIZEY, height);

	MCFP_SET_F(base, MCFP_R_YUV_MIXER_ROISTITCHSTARTX, MCFP_F_YUV_MIXER_ROISTITCHSTARTX, 0);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_ROISTITCHSTARTX, MCFP_F_YUV_MIXER_ROISTITCHSTARTY, 0);

	MCFP_SET_F(base, MCFP_R_YUV_MIXER_ROISTITCHSIZEX,
			MCFP_F_YUV_MIXER_ROISTITCHSIZEX, dma_width);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_ROISTITCHSIZEX,
			MCFP_F_YUV_MIXER_ROISTITCHSIZEY, height);

	MCFP_SET_F(base, MCFP_R_YUV_MIXER_BIQUAD_SCALE_SHIFT_ADDER,
			MCFP_F_YUV_MIXER_BIQUAD_SCALE_SHIFT_ADDER, biquad_scale_shift_adder);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_BINNING_X, MCFP_F_YUV_MIXER_BINNING_X, binning_x);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_BINNING_X, MCFP_F_YUV_MIXER_BINNING_Y, binning_y);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_RADIAL_CENTER_X,
			MCFP_F_YUV_MIXER_RADIAL_CENTER_X, radial_center_x);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_RADIAL_CENTER_X,
			MCFP_F_YUV_MIXER_RADIAL_CENTER_Y, radial_center_y);

	MCFP_SET_R(base,
		MCFP_R_YUV_MIXER_MV_SCALE_SHIFT_WIDTH, mcfp_config->motion_scale_x);
	MCFP_SET_R(base,
		MCFP_R_YUV_MIXER_MV_SCALE_SHIFT_HEIGHT, mcfp_config->motion_scale_y);
}

void mcfp_hw_s_crop_clean_img_otf(struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width)
{
	MCFP_SET_F(base,
		MCFP_R_YUV_CROPCLEANOTF_BYPASS, MCFP_F_YUV_CROPCLEANOTF_BYPASS, 1);
}

void mcfp_hw_s_crop_wgt_otf(struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width)
{
	MCFP_SET_F(base,
		MCFP_R_YUV_CROPWEIGHTOTF_BYPASS, MCFP_F_YUV_CROPWEIGHTOTF_BYPASS, 1);
}

void mcfp_hw_s_crop_clean_img_dma(struct pablo_mmio *base, u32 set_id,
					u32 start_x, u32 width, u32 height,
					bool bypass)
{
	MCFP_SET_F(base,
		MCFP_R_YUV_CROPCLEANDMA_BYPASS, MCFP_F_YUV_CROPCLEANDMA_BYPASS, bypass);

	if (!bypass) {
		MCFP_SET_F(base,
			MCFP_R_YUV_CROPCLEANDMA_START, MCFP_F_YUV_CROPCLEANDMA_START_X, start_x);
		MCFP_SET_F(base,
			MCFP_R_YUV_CROPCLEANDMA_START, MCFP_F_YUV_CROPCLEANDMA_START_Y, 0);
		MCFP_SET_F(base,
			MCFP_R_YUV_CROPCLEANDMA_SIZE, MCFP_F_YUV_CROPCLEANDMA_SIZE_X, width);
		MCFP_SET_F(base,
			MCFP_R_YUV_CROPCLEANDMA_SIZE, MCFP_F_YUV_CROPCLEANDMA_SIZE_Y, height);
	}
}

void mcfp_hw_s_crop_wgt_dma(struct pablo_mmio *base, u32 set_id,
				u32 start_x, u32 width, u32 height,
				bool bypass)
{
	MCFP_SET_F(base,
		MCFP_R_YUV_CROPWEIGHTDMA_BYPASS, MCFP_F_YUV_CROPWEIGHTDMA_BYPASS, bypass);

	if (!bypass) {
		MCFP_SET_F(base,
				MCFP_R_YUV_CROPWEIGHTDMA_START,
				MCFP_F_YUV_CROPWEIGHTDMA_START_X, start_x);
		MCFP_SET_F(base,
				MCFP_R_YUV_CROPWEIGHTDMA_START,
				MCFP_F_YUV_CROPWEIGHTDMA_START_Y, 0);
		MCFP_SET_F(base,
			MCFP_R_YUV_CROPWEIGHTDMA_SIZE, MCFP_F_YUV_CROPWEIGHTDMA_SIZE_X, width);
		MCFP_SET_F(base,
			MCFP_R_YUV_CROPWEIGHTDMA_SIZE, MCFP_F_YUV_CROPWEIGHTDMA_SIZE_Y, height);
	}
}

void mcfp_hw_s_img_bitshift(struct pablo_mmio *base, u32 set_id, u32 img_shift_bit)
{
	if (img_shift_bit) {
		MCFP_SET_R(base,
			MCFP_R_YUV_MAIN_CTRL_DATASHIFTERRDMA_BYPASS, 0);
		MCFP_SET_R(base,
			MCFP_R_YUV_MAIN_CTRL_DATASHIFTERWDMA_BYPASS, 0);
	} else {
		MCFP_SET_R(base,
			MCFP_R_YUV_MAIN_CTRL_DATASHIFTERRDMA_BYPASS, 1);
		MCFP_SET_R(base,
			MCFP_R_YUV_MAIN_CTRL_DATASHIFTERWDMA_BYPASS, 1);
	}
	MCFP_SET_R(base,
		MCFP_R_YUV_MAIN_CTRL_DATASHIFTERRDMA_LSHIFT, img_shift_bit);
	MCFP_SET_R(base,
		MCFP_R_YUV_MAIN_CTRL_DATASHIFTERWDMA_RSHIFT, img_shift_bit);
}

void mcfp_hw_s_wgt_bitshift(struct pablo_mmio *base, u32 set_id, u32 wgt_shift_bit)
{
	if (wgt_shift_bit) {
		MCFP_SET_R(base,
			MCFP_R_YUV_MAIN_CTRL_WGTSHIFTERRDMA_BYPASS, 0);
		MCFP_SET_R(base,
			MCFP_R_YUV_MAIN_CTRL_WGTSHIFTERWDMA_BYPASS, 0);
	} else {
		MCFP_SET_R(base,
			MCFP_R_YUV_MAIN_CTRL_WGTSHIFTERRDMA_BYPASS, 1);
		MCFP_SET_R(base,
			MCFP_R_YUV_MAIN_CTRL_WGTSHIFTERWDMA_BYPASS, 1);
	}
	MCFP_SET_R(base,
		MCFP_R_YUV_MAIN_CTRL_WGTSHIFTERRDMA_WGT_LSHIFT, wgt_shift_bit);
	MCFP_SET_R(base,
		MCFP_R_YUV_MAIN_CTRL_WGTSHIFTERWDMA_WGT_RSHIFT, wgt_shift_bit);
}

void mcfp_hw_s_mono_mode(struct pablo_mmio *base, u32 set_id, bool enable)
{
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MONO_EN, MCFP_F_YUV_GEOMATCH_PREV_MONO_EN, enable);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_MONO_EN, MCFP_F_YUV_GEOMATCH_CURR_MONO_EN, enable);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_TNR_MONO_EN, MCFP_F_YUV_MIXER_TNR_MONO_EN, enable);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_TNR_MONO_EN, MCFP_F_YUV_MIXER_FUSION_CURR_MONO_EN, enable);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_TNR_MONO_EN, MCFP_F_YUV_MIXER_FUSION_PREV_MONO_EN, enable);
}

void mcfp_hw_s_crc(struct pablo_mmio *base, u32 seed)
{
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_STREAM_CRC,
			MCFP_F_YUV_CINFIFO_OTF_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_NR_OTF_STREAM_CRC,
			MCFP_F_YUV_CINFIFO_NR_OTF_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_CURR_YUV_IN_Y_CRC,
			MCFP_F_YUV_DATASHIFTERRDMA_CURR_Y_IN_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_CURR_YUV_IN_C_CRC,
			MCFP_F_YUV_DATASHIFTERRDMA_CURR_UV_IN_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUVP_NR_IMG_WDMA_CRC,
			MCFP_F_YUV_CROPCLEANDMA_YUVP_NR_IMG_WDMA_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_PRV_IMG_Y_CRC,
			MCFP_F_YUV_GEOMATCH_PRV_IMG_Y_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_PRV_IMG_C_CRC,
			MCFP_F_YUV_GEOMATCH_PRV_IMG_C_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_PRV_IMG_OOB_CRC,
			MCFP_F_YUV_GEOMATCH_PRV_IMG_OOB_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_GEOMATCH_PRV_WEIGHT_CRC,
			MCFP_F_YUV_GEOMATCH_PRV_WEIGHT_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_CLEAN_IMG_OTF_Y_CRC,
			MCFP_F_YUV_MIXER_CLEAN_IMG_OTF_Y_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_CLEAN_IMG_OTF_C_CRC,
			MCFP_F_YUV_MIXER_CLEAN_IMG_OTF_C_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_CLEAN_IMG_DMA_Y_CRC,
			MCFP_F_YUV_MIXER_CLEAN_IMG_DMA_Y_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_CLEAN_IMG_DMA_C_CRC,
			MCFP_F_YUV_MIXER_CLEAN_IMG_DMA_C_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_CLEAN_IMG_GUIDE_Y_CRC,
			MCFP_F_YUV_MIXER_CLEAN_IMG_GUIDE_Y_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_CLEAN_WEIGHT_OTF_CRC,
			MCFP_F_YUV_MIXER_CLEAN_WEIGHT_OTF_CRC_SEED, seed);
	MCFP_SET_F(base, MCFP_R_YUV_MIXER_CLEAN_WEIGHT_DMA_CRC,
			MCFP_F_YUV_MIXER_CLEAN_WEIGHT_DMA_CRC_SEED, seed);
}

void mcfp_hw_s_dtp(struct pablo_mmio *base, u32 enable, enum mcfp_dtp_type type,
	u32 y, u32 u, u32 v, enum mcfp_dtp_color_bar cb)
{
	/* Not support */
}

void mcfp_hw_s_geomatch_mode(struct pablo_mmio *base, u32 set_id, u32 tnr_mode)
{
	if (tnr_mode == MCFP_TNR_MODE_PREPARE)
		MCFP_SET_F(base,
			MCFP_R_YUV_GEOMATCH_EN, MCFP_F_YUV_GEOMATCH_EN, 0);
	else
		MCFP_SET_F(base,
			MCFP_R_YUV_GEOMATCH_EN, MCFP_F_YUV_GEOMATCH_EN, 1);

	MCFP_SET_F(base,
		MCFP_R_YUV_GEOMATCH_MC_LMC_TNR_MODE, MCFP_F_YUV_GEOMATCH_MC_LMC_TNR_MODE, tnr_mode);
	MCFP_SET_F(base,
		MCFP_R_YUV_GEOMATCH_MATCH_ENABLE, MCFP_F_YUV_GEOMATCH_MATCH_ENABLE, 0);
}

void mcfp_hw_s_mixer_mode(struct pablo_mmio *base, u32 set_id, u32 tnr_mode)
{
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_ENABLE, MCFP_F_YUV_MIXER_ENABLE, 1);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_MODE, MCFP_F_YUV_MIXER_MODE, tnr_mode);

	/* avoid rule checker assertion*/
	MCFP_SET_F(base,
		MCFP_R_YUV_CINFIFO_OTF_CONFIG, MCFP_F_YUV_CINFIFO_OTF_ROL_RESET_ON_FRAME_START, 0);

	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_SLOPE_0_0, MCFP_F_YUV_MIXER_THRESH_SLOPE_0_0, 127);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_SLOPE_0_1, MCFP_F_YUV_MIXER_THRESH_SLOPE_0_1, 84);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_SLOPE_0_2, MCFP_F_YUV_MIXER_THRESH_SLOPE_0_2, 101);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_SLOPE_0_3, MCFP_F_YUV_MIXER_THRESH_SLOPE_0_3, 138);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_SLOPE_0_4, MCFP_F_YUV_MIXER_THRESH_SLOPE_0_4, 309);

	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_0_0, MCFP_F_YUV_MIXER_THRESH_0_0, 86);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_0_1, MCFP_F_YUV_MIXER_THRESH_0_1, 96);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_0_2, MCFP_F_YUV_MIXER_THRESH_0_2, 86);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_0_3, MCFP_F_YUV_MIXER_THRESH_0_3, 43);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_THRESH_0_4, MCFP_F_YUV_MIXER_THRESH_0_4, 21);

	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_0_0, MCFP_F_YUV_MIXER_SUB_THRESH_0_0, 86);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_0_1, MCFP_F_YUV_MIXER_SUB_THRESH_0_1, 96);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_0_2, MCFP_F_YUV_MIXER_SUB_THRESH_0_2, 86);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_0_3, MCFP_F_YUV_MIXER_SUB_THRESH_0_3, 43);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_0_4, MCFP_F_YUV_MIXER_SUB_THRESH_0_4, 21);

	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_WIDTH_0_0, MCFP_F_YUV_MIXER_SUB_THRESH_WIDTH_0_0, 0);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_WIDTH_0_1, MCFP_F_YUV_MIXER_SUB_THRESH_WIDTH_0_1, 0);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_WIDTH_0_2, MCFP_F_YUV_MIXER_SUB_THRESH_WIDTH_0_2, 0);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_WIDTH_0_3, MCFP_F_YUV_MIXER_SUB_THRESH_WIDTH_0_3, 0);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_WIDTH_0_4, MCFP_F_YUV_MIXER_SUB_THRESH_WIDTH_0_4, 0);

	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_SLOPE_0_0, MCFP_F_YUV_MIXER_SUB_THRESH_SLOPE_0_0, 16383);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_SLOPE_0_1, MCFP_F_YUV_MIXER_SUB_THRESH_SLOPE_0_1, 16383);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_SLOPE_0_2, MCFP_F_YUV_MIXER_SUB_THRESH_SLOPE_0_2, 16383);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_SLOPE_0_3, MCFP_F_YUV_MIXER_SUB_THRESH_SLOPE_0_3, 16383);
	MCFP_SET_F(base,
		MCFP_R_YUV_MIXER_SUB_THRESH_SLOPE_0_4, MCFP_F_YUV_MIXER_SUB_THRESH_SLOPE_0_4, 16383);

	if ((tnr_mode == MCFP_TNR_MODE_PREPARE) || (tnr_mode == MCFP_TNR_MODE_FUSION)) {
		MCFP_SET_F(base,
			MCFP_R_YUV_MIXER_WGT_UPDATE_EN, MCFP_F_YUV_MIXER_WGT_UPDATE_EN, 0);
	} else {
		MCFP_SET_F(base,
			MCFP_R_YUV_MIXER_WGT_UPDATE_EN, MCFP_F_YUV_MIXER_WGT_UPDATE_EN, 1);
	}
}

void mcfp_hw_s_strgen(struct pablo_mmio *base, u32 set_id)
{
	/* STRGEN setting */
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_CONFIG, MCFP_F_YUV_CINFIFO_OTF_STRGEN_MODE_EN, 1);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_CONFIG, MCFP_F_YUV_CINFIFO_OTF_STRGEN_MODE_DATA_TYPE, 1);
	MCFP_SET_F(base, MCFP_R_YUV_CINFIFO_OTF_CONFIG, MCFP_F_YUV_CINFIFO_OTF_STRGEN_MODE_DATA, 255);
}

u32 mcfp_hw_g_reg_cnt(void)
{
	return MCFP_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(mcfp_hw_g_reg_cnt);

void mcfp_hw_init_pmio_config(struct pmio_config *cfg)
{
	cfg->num_corexs = 2;
	cfg->corex_stride = 0x8000;

	cfg->volatile_table = &mcfp_volatile_ranges_table;
	cfg->wr_noinc_table = &mcfp_wr_noinc_ranges_table;

	cfg->max_register = MCFP_R_DBG_44;
	cfg->num_reg_defaults_raw = (MCFP_R_DBG_44 >> 2) + 1;
	cfg->phys_base = 0x15780000;
	cfg->dma_addr_shift = 0;

	cfg->ranges = mcfp_range_cfgs;
	cfg->num_ranges = ARRAY_SIZE(mcfp_range_cfgs);

	cfg->fields = mcfp_field_descs;
	cfg->num_fields = ARRAY_SIZE(mcfp_field_descs);
}
KUNIT_EXPORT_SYMBOL(mcfp_hw_init_pmio_config);