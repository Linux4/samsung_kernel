// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EXYNOS CAMERA PostProcessing GDC driver
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-api-gdc.h"
#if IS_ENABLED(GDC_USE_MMIO)
#include "pablo-hw-reg-gdc-mmio-v1200.h"
#else
#include "pablo-hw-reg-gdc-v1200.h"
#endif
#include "votf/pablo-votf-common-enum.h"
#include "is-hw-chain.h"
#include "is-common-enum.h"
#include "is-type.h"
#include "is-config.h"
#include "pmio.h"

#if IS_ENABLED(GDC_USE_MMIO)
#define GDC_SET_F(base, R, F, val) \
	is_hw_set_field(((struct pablo_mmio *)base)->mmio_base, &gdc_regs[R], &gdc_fields[F], val)
#define GDC_SET_R(base, R, val) \
	is_hw_set_reg(((struct pablo_mmio *)base)->mmio_base, &gdc_regs[R], val)
#define GDC_SET_V(base, reg_val, F, val) \
	is_hw_set_field_value(reg_val, &gdc_fields[F], val)
#define GDC_GET_F(base, R, F) \
	is_hw_get_field(((struct pablo_mmio *)base)->mmio_base, &gdc_regs[R], &gdc_fields[F])
#define GDC_GET_R(base, R) \
	is_hw_get_reg(((struct pablo_mmio *)base)->mmio_base, &gdc_regs[R])
#else
#define GDC_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define GDC_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define GDC_SET_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)
#define GDC_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define GDC_GET_R(base, R) PMIO_GET_R(base, R)
#endif

static const struct gdc_variant gdc_variant[] = {
	{
		.limit_input = {
			.min_w		= 96,
			.min_h		= 64,
			.max_w		= 16384,
			.max_h		= 12288,
		},
		.limit_output = {
			.min_w		= 96,
			.min_h		= 64,
			.max_w		= 16384,
			.max_h		= 12288,
		},
		.version		= 0x12000000,
	},
};

const struct gdc_variant *camerapp_hw_gdc_get_size_constraints(struct pablo_mmio *pmio)
{
	return gdc_variant;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_get_size_constraints);

u32 camerapp_hw_gdc_get_ver(struct pablo_mmio *pmio)
{
	return GDC_GET_R(pmio, GDC_R_IP_VERSION);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_get_ver);

static void gdc_hw_s_cmdq(struct pablo_mmio *pmio, dma_addr_t clh, u32 noh)
{
	if (clh && noh) {
		GDC_SET_F(pmio, GDC_R_CMDQ_QUE_CMD_H,
			GDC_F_CMDQ_QUE_CMD_BASE_ADDR, DVA_GET_MSB(clh));
		GDC_SET_F(pmio, GDC_R_CMDQ_QUE_CMD_M, GDC_F_CMDQ_QUE_CMD_HEADER_NUM, noh);
		GDC_SET_F(pmio, GDC_R_CMDQ_QUE_CMD_M, GDC_F_CMDQ_QUE_CMD_SETTING_MODE, 1);
	}

	GDC_SET_R(pmio, GDC_R_CMDQ_ADD_TO_QUEUE_0, 1);
	GDC_SET_F(pmio, GDC_R_IP_PROCESSING, GDC_F_IP_PROCESSING, 0);
}

void camerapp_hw_gdc_start(struct pablo_mmio *pmio, struct c_loader_buffer *clb)
{
	gdc_hw_s_cmdq(pmio, clb->header_dva, clb->num_of_headers);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_start);

void camerapp_hw_gdc_stop(struct pablo_mmio *pmio)
{
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_stop);

u32 camerapp_hw_gdc_sw_reset(struct pablo_mmio *pmio)
{
	u32 reset_count = 0;

	/* request to gdc hw */
	GDC_SET_F(pmio, GDC_R_SW_RESET, GDC_F_SW_RESET, 1);

	/* wait reset complete */
	do {
		reset_count++;
		if (reset_count > 10000)
			return reset_count;

	} while (GDC_GET_F(pmio, GDC_R_SW_RESET, GDC_F_SW_RESET) != 0);

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_sw_reset);

void camerapp_hw_gdc_clear_intr_all(struct pablo_mmio *pmio)
{
	GDC_SET_F(pmio, GDC_R_INT_REQ_INT0_CLEAR, GDC_F_INT_REQ_INT0_CLEAR, GDC_INT0_EN);
	GDC_SET_F(pmio, GDC_R_INT_REQ_INT1_CLEAR, GDC_F_INT_REQ_INT1_CLEAR, GDC_INT1_EN);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_clear_intr_all);

u32 camerapp_hw_gdc_get_intr_status_and_clear(struct pablo_mmio *pmio)
{
	u32 int0_status, int1_status;

	int0_status = GDC_GET_R(pmio, GDC_R_INT_REQ_INT0);
	GDC_SET_R(pmio, GDC_R_INT_REQ_INT0_CLEAR, int0_status);

	int1_status = GDC_GET_R(pmio, GDC_R_INT_REQ_INT1);
	GDC_SET_R(pmio, GDC_R_INT_REQ_INT1_CLEAR, int1_status);

	gdc_dbg("int0(0x%x), int1(0x%x)\n", int0_status, int1_status);

	return int0_status;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_get_intr_status_and_clear);

u32 camerapp_hw_gdc_get_int_frame_start(void)
{
	return GDC_INT_FRAME_START;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_get_int_frame_start);

u32 camerapp_hw_gdc_get_int_frame_end(void)
{
	return GDC_INT_FRAME_END;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_get_int_frame_end);

/* DMA Configuration */
static void camerapp_hw_gdc_set_rdma_enable(struct pablo_mmio *pmio,
	u32 en_y, u32 en_uv)
{
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_EN, GDC_F_YUV_RDMAY_EN, en_y);
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_EN, GDC_F_YUV_RDMAUV_EN, en_uv);
}

static void camerapp_hw_gdc_set_wdma_enable(struct pablo_mmio *pmio, u32 en)
{
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_EN, GDC_F_YUV_WDMA_EN, en);
}

static void camerapp_hw_gdc_set_rdma_sbwc_enable(struct pablo_mmio *pmio, u32 en)
{
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_COMP_CONTROL, GDC_F_YUV_RDMAY_SBWC_EN, en);
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_COMP_CONTROL, GDC_F_YUV_RDMAUV_SBWC_EN, en);
}

static void camerapp_hw_gdc_set_wdma_sbwc_enable(struct pablo_mmio *pmio, u32 en)
{
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_COMP_CONTROL, GDC_F_YUV_WDMA_SBWC_EN, en);
}

static void camerapp_hw_gdc_set_rdma_sbwc_64b_align(struct pablo_mmio *pmio, u32 align_64b)
{
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_COMP_CONTROL,
		GDC_F_YUV_RDMAY_SBWC_64B_ALIGN, align_64b);
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_COMP_CONTROL,
		GDC_F_YUV_RDMAUV_SBWC_64B_ALIGN, align_64b);
}

static void camerapp_hw_gdc_set_wdma_sbwc_64b_align(struct pablo_mmio *pmio, u32 align_64b)
{
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_COMP_CONTROL,
		GDC_F_YUV_WDMA_SBWC_64B_ALIGN, align_64b);
}

static void camerapp_hw_gdc_set_rdma_data_format(struct pablo_mmio *pmio, u32 yuv_format)
{
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_DATA_FORMAT,
		GDC_F_YUV_RDMAY_DATA_FORMAT_YUV, yuv_format);
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_DATA_FORMAT,
		GDC_F_YUV_RDMAUV_DATA_FORMAT_YUV, yuv_format);
}

static void camerapp_hw_gdc_set_wdma_data_format(struct pablo_mmio *pmio, u32 yuv_format)
{
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_DATA_FORMAT,
		GDC_F_YUV_WDMA_DATA_FORMAT_YUV, yuv_format);
}

static void camerapp_hw_gdc_set_mono_mode(struct pablo_mmio *pmio, u32 mode)
{
	GDC_SET_F(pmio, GDC_R_YUV_GDC_YUV_FORMAT, GDC_F_YUV_GDC_MONO_MODE, mode);
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_MONO_MODE, GDC_F_YUV_WDMA_MONO_MODE, mode);
}

static void camerapp_hw_gdc_set_rdma_comp_control(struct pablo_mmio *pmio, u32 mode)
{
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_COMP_LOSSY_QUALITY_CONTROL,
		GDC_F_YUV_RDMAY_COMP_LOSSY_QUALITY_CONTROL, mode);
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_COMP_LOSSY_QUALITY_CONTROL,
		GDC_F_YUV_RDMAUV_COMP_LOSSY_QUALITY_CONTROL, mode);
}

static void camerapp_hw_gdc_set_wdma_comp_control(struct pablo_mmio *pmio, u32 mode)
{
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_COMP_LOSSY_QUALITY_CONTROL,
		GDC_F_YUV_WDMA_COMP_LOSSY_QUALITY_CONTROL, mode);
}

static void camerapp_hw_gdc_set_rdma_img_size(struct pablo_mmio *pmio,
	u32 width, u32 height)
{
	u32 yuv_format;

	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_WIDTH, GDC_F_YUV_RDMAY_WIDTH, width);
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_HEIGHT, GDC_F_YUV_RDMAY_HEIGHT, height);

	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_WIDTH, GDC_F_YUV_RDMAUV_WIDTH, width);

	yuv_format = GDC_GET_F(pmio, GDC_R_YUV_GDC_YUV_FORMAT, GDC_F_YUV_GDC_YUV_FORMAT);
	if (yuv_format == GDC_YUV420)
		height >>= 1;

	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_HEIGHT, GDC_F_YUV_RDMAUV_HEIGHT, height);
}

static void camerapp_hw_gdc_set_wdma_img_size(struct pablo_mmio *pmio,
	u32 width, u32 height)
{
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_WIDTH, GDC_F_YUV_WDMA_WIDTH, width);
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_HEIGHT, GDC_F_YUV_WDMA_HEIGHT, height);
}

static void camerapp_hw_gdc_set_rdma_stride(struct pablo_mmio *pmio,
	u32 y_stride, u32 uv_stride)
{
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_IMG_STRIDE_1P,
		GDC_F_YUV_RDMAY_IMG_STRIDE_1P, y_stride);
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_IMG_STRIDE_1P,
		GDC_F_YUV_RDMAUV_IMG_STRIDE_1P, uv_stride);
}

static void camerapp_hw_gdc_set_rdma_header_stride(struct pablo_mmio *pmio,
	u32 y_header_stride, u32 uv_header_stride)
{
		GDC_SET_F(pmio, GDC_R_YUV_RDMAY_HEADER_STRIDE_1P,
			GDC_F_YUV_RDMAY_HEADER_STRIDE_1P, y_header_stride);
		GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_HEADER_STRIDE_1P,
			GDC_F_YUV_RDMAUV_HEADER_STRIDE_1P, uv_header_stride);
}

static void camerapp_hw_gdc_set_wdma_stride(struct pablo_mmio *pmio,
	u32 y_stride, u32 uv_stride)
{
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_IMG_STRIDE_1P,
		GDC_F_YUV_WDMA_IMG_STRIDE_1P, y_stride);
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_IMG_STRIDE_2P,
		GDC_F_YUV_WDMA_IMG_STRIDE_2P, uv_stride);
}

static void camerapp_hw_gdc_set_wdma_header_stride(struct pablo_mmio *pmio,
	u32 y_header_stride, u32 uv_header_stride)
{
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_HEADER_STRIDE_1P,
		GDC_F_YUV_WDMA_HEADER_STRIDE_1P, y_header_stride);
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_HEADER_STRIDE_2P,
		GDC_F_YUV_WDMA_HEADER_STRIDE_2P, uv_header_stride);
}

static void camerapp_hw_gdc_set_rdma_addr(struct pablo_mmio *pmio,
	dma_addr_t y_addr, dma_addr_t uv_addr)
{
	/* MSB */
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_0_0,
		GDC_F_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_0_0, DVA_GET_MSB(y_addr));
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_0_0,
		GDC_F_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_0_0, DVA_GET_MSB(uv_addr));

	/* LSB */
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0,
		GDC_F_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0, DVA_GET_LSB(y_addr));
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0,
		GDC_F_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0, DVA_GET_LSB(uv_addr));
}

static void camerapp_hw_gdc_set_wdma_addr(struct pablo_mmio *pmio,
	dma_addr_t y_addr, dma_addr_t uv_addr)
{
	/* MSB */
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_0_0,
		GDC_F_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_0_0, DVA_GET_MSB(y_addr));
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_0_0,
		GDC_F_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_0_0, DVA_GET_MSB(uv_addr));

	/* LSB */
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0,
		GDC_F_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0, DVA_GET_LSB(y_addr));
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_LSB_4B_0_0,
		GDC_F_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_LSB_4B_0_0, DVA_GET_LSB(uv_addr));
}

static void camerapp_hw_gdc_set_rdma_header_addr(struct pablo_mmio *pmio,
	dma_addr_t y_addr, dma_addr_t uv_addr)
{
	/* MSB */
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_0_0,
		GDC_F_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_0_0, DVA_GET_MSB(y_addr));
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_0_0,
		GDC_F_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_0_0, DVA_GET_MSB(uv_addr));

	/* LSB */
	GDC_SET_F(pmio, GDC_R_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0,
		GDC_F_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0, DVA_GET_LSB(y_addr));
	GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0,
		GDC_F_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0, DVA_GET_LSB(uv_addr));
}

static void camerapp_hw_gdc_set_wdma_header_addr(struct pablo_mmio *pmio,
	dma_addr_t y_addr, dma_addr_t uv_addr)
{
	/* MSB */
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_0_0,
		GDC_F_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_0_0, DVA_GET_MSB(y_addr));
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_0_0,
		GDC_F_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_0_0, DVA_GET_MSB(uv_addr));

	/* LSB */
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0,
		GDC_F_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0, DVA_GET_LSB(y_addr));
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_LSB_4B_0_0,
		GDC_F_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_LSB_4B_0_0, DVA_GET_LSB(uv_addr));
}

static void camerapp_hw_gdc_write_grid_pmio(struct pablo_mmio *pmio,
	struct gdc_crop_param *crop_param)
{
	u32 size = GRID_X_SIZE * GRID_Y_SIZE * 4;

	pmio_raw_write(pmio, GDC_R_YUV_GDC_GRID_DX_0_0, crop_param->calculated_grid_x, size);
	pmio_raw_write(pmio, GDC_R_YUV_GDC_GRID_DY_0_0, crop_param->calculated_grid_y, size);
}

static void camerapp_hw_gdc_write_grid_mmio(struct pablo_mmio *pmio,
	struct gdc_crop_param *crop_param)
{
	int i, j;
	u32 sfr_offset = 0x0004;
	u32 sfr_start_x = GDC_R_YUV_GDC_GRID_DX_0_0;
	u32 sfr_start_y = GDC_R_YUV_GDC_GRID_DY_0_0;
	u32 cal_sfr_offset;

	for (i = 0; i < GRID_Y_SIZE; i++) {
		for (j = 0; j < GRID_X_SIZE; j++) {
			cal_sfr_offset = (sfr_offset * i * GRID_X_SIZE) + (sfr_offset * j);
			writel((u32)crop_param->calculated_grid_x[i][j],
				pmio->mmio_base + sfr_start_x + cal_sfr_offset);
			writel((u32)crop_param->calculated_grid_y[i][j],
				pmio->mmio_base + sfr_start_y + cal_sfr_offset);
		}
	}
}

static void camerapp_hw_gdc_update_grid_param(struct pablo_mmio *pmio,
	struct gdc_crop_param *crop_param)
{
	if (!crop_param->use_calculated_grid) {
		memset(crop_param->calculated_grid_x, 0, sizeof(crop_param->calculated_grid_x));
		memset(crop_param->calculated_grid_y, 0, sizeof(crop_param->calculated_grid_y));
	}

	if (IS_ENABLED(GDC_USE_MMIO))
		camerapp_hw_gdc_write_grid_mmio(pmio, crop_param);
	else
		camerapp_hw_gdc_write_grid_pmio(pmio, crop_param);
}

void camerapp_hw_gdc_set_initialization(struct pablo_mmio *pmio)
{
	GDC_SET_F(pmio, GDC_R_IP_PROCESSING, GDC_F_IP_PROCESSING, 0x1);
	if (!IS_ENABLED(GDC_USE_MMIO)) {
		GDC_SET_R(pmio, GDC_R_C_LOADER_ENABLE, 1);
		GDC_SET_R(pmio, GDC_R_STAT_RDMACLOADER_EN, 1);
	}
	/* Interrupt group enable for one frame */
	GDC_SET_F(pmio, GDC_R_CMDQ_QUE_CMD_L, GDC_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, 0xFF);
	/* 1: DMA preloading, 2: COREX, 3: APB Direct */
	GDC_SET_F(pmio, GDC_R_CMDQ_QUE_CMD_M, GDC_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	GDC_SET_R(pmio, GDC_R_CMDQ_ENABLE, 1);

	GDC_SET_F(pmio, GDC_R_INT_REQ_INT0_ENABLE, GDC_F_INT_REQ_INT0_ENABLE, GDC_INT0_EN);
	GDC_SET_F(pmio, GDC_R_INT_REQ_INT1_ENABLE, GDC_F_INT_REQ_INT1_ENABLE, GDC_INT1_EN);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_set_initialization);

static void camerapp_hw_gdc_update_scale_parameters(struct pablo_mmio *pmio,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame,
	struct gdc_crop_param *crop_param)
{
	u32 gdc_input_width;
	u32 gdc_input_height;
	u32 gdc_crop_offset_x;
	u32 gdc_crop_offset_y;
	u32 gdc_crop_width;
	u32 gdc_crop_height;
	u32 gdc_scale_shifter_x;
	u32 gdc_scale_shifter_y;
	u32 gdc_scale_x;
	u32 gdc_scale_y;
	u32 gdc_scale_width;
	u32 gdc_scale_height;

	u32 gdc_inv_scale_x;
	u32 gdc_inv_scale_y;
	u32 gdc_output_offset_x;
	u32 gdc_output_offset_y;
	u32 gdc_output_width;
	u32 gdc_output_height;

	u32 scaleShifterX;
	u32 scaleShifterY;
	u32 imagewidth;
	u32 imageheight;
	u32 out_scaled_width;
	u32 out_scaled_height;

	/* GDC original input size */
	gdc_input_width = s_frame->width;
	gdc_input_height = s_frame->height;
	gdc_output_width = d_frame->width;
	gdc_output_height = d_frame->height;

	/* Meaningful only when out_crop_bypass = 0, x <= (gdc_crop_width - gdc_image_active_width) */
	if (!IS_ALIGNED(gdc_output_width, GDC_WIDTH_ALIGN) ||
		!IS_ALIGNED(gdc_output_height, GDC_HEIGHT_ALIGN)) {
		gdc_dbg("gdc output width(%d) is not (%d)align or height(%d) is not (%d)align.\n",
				gdc_output_width, GDC_WIDTH_ALIGN,
				gdc_output_height, GDC_HEIGHT_ALIGN);
		gdc_output_width = ALIGN_DOWN(gdc_output_width, GDC_WIDTH_ALIGN);
		gdc_output_height = ALIGN_DOWN(gdc_output_height, GDC_HEIGHT_ALIGN);
	}

	if (crop_param->is_grid_mode) {
		/* output */
		gdc_scale_width = gdc_output_width;
		gdc_scale_height = gdc_output_height;
	} else {
		/* input */
		gdc_scale_width = gdc_input_width;
		gdc_scale_height = gdc_input_height;
	}

	/* GDC input crop size from original input size */
	if (crop_param->is_bypass_mode) {
		gdc_crop_width = gdc_input_width;
		gdc_crop_height = gdc_input_height;
		gdc_crop_offset_x = 0;
		gdc_crop_offset_y = 0;

	} else {
		gdc_crop_width = crop_param->crop_width;
		gdc_crop_height = crop_param->crop_height;
		gdc_crop_offset_x = crop_param->crop_start_x;
		gdc_crop_offset_y = crop_param->crop_start_y;
	}

	/* Real to virtual grid scaling factor shifters */
	scaleShifterX = DS_SHIFTER_MAX;
	imagewidth = gdc_scale_width << 1;
	while ((imagewidth <= MAX_VIRTUAL_GRID_X) && (scaleShifterX > 0)) {
		imagewidth <<= 1;
		scaleShifterX--;
	}
	gdc_scale_shifter_x = scaleShifterX;

	scaleShifterY  = DS_SHIFTER_MAX;
	imageheight = gdc_scale_height << 1;
	while ((imageheight <= MAX_VIRTUAL_GRID_Y) && (scaleShifterY > 0)) {
		imageheight <<= 1;
		scaleShifterY--;
	}
	gdc_scale_shifter_y = scaleShifterY;

	/* Real to virtual grid scaling factors */
	gdc_scale_x = MIN(65535,
		((MAX_VIRTUAL_GRID_X << (DS_FRACTION_BITS + gdc_scale_shifter_x)) + gdc_scale_width / 2)
		/ gdc_scale_width);
	gdc_scale_y = MIN(65535,
		((MAX_VIRTUAL_GRID_Y << (DS_FRACTION_BITS + gdc_scale_shifter_y)) + gdc_scale_height / 2)
		/ gdc_scale_height);

	/* Virtual to real grid scaling factors */
	gdc_inv_scale_x = gdc_input_width;
	gdc_inv_scale_y = ((gdc_input_height << 3) + 3) / 6;

	if ((gdc_crop_width < gdc_output_width) || crop_param->is_bypass_mode)
		gdc_output_offset_x = 0;
	else
		gdc_output_offset_x =
			ALIGN_DOWN((gdc_crop_width - gdc_output_width) >> 1, GDC_OFFSET_ALIGN);

	if ((gdc_crop_height < gdc_output_height) || crop_param->is_bypass_mode)
		gdc_output_offset_y = 0;
	else
		gdc_output_offset_y =
			ALIGN_DOWN((gdc_crop_height - gdc_output_height) >> 1, GDC_OFFSET_ALIGN);

	/* if GDC is scaled up : 8192(default) = no scaling, 4096 = 2 times scaling */
	/* now is selected no scaling. => calculation (8192 * in / out) */
	out_scaled_width = 8192;
	out_scaled_height = 8192;
	if (!crop_param->is_bypass_mode) {
		/* Only for scale up */
		if (gdc_crop_width * MAX_OUTPUT_SCALER_RATIO < gdc_output_width) {
			gdc_dbg("gdc_out_scale_x exceeds max ratio(%d). in_crop(%dx%d) out(%dx%d)\n",
					MAX_OUTPUT_SCALER_RATIO,
					gdc_crop_width, gdc_crop_height,
					gdc_output_width, gdc_output_height);
			out_scaled_width = 8192 / MAX_OUTPUT_SCALER_RATIO;
		} else if (gdc_crop_width < gdc_output_width) {
			out_scaled_width = 8192 * gdc_crop_width / gdc_output_width;
		}

		/* Only for scale up */
		if (gdc_crop_height * MAX_OUTPUT_SCALER_RATIO < gdc_output_height) {
			gdc_dbg("gdc_out_scale_y exceeds max ratio(%d). in_crop(%dx%d) out(%dx%d)\n",
					MAX_OUTPUT_SCALER_RATIO,
					gdc_crop_width, gdc_crop_height,
					gdc_output_width, gdc_output_height);
			out_scaled_height = 8192 / MAX_OUTPUT_SCALER_RATIO;
		} else if (gdc_crop_height < gdc_output_height) {
			out_scaled_height = 8192 * gdc_crop_height / gdc_output_height;
		}
	}

	GDC_SET_F(pmio, GDC_R_YUV_GDC_CONFIG, GDC_F_YUV_GDC_MIRROR_X, 0);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_CONFIG, GDC_F_YUV_GDC_MIRROR_Y, 0);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INPUT_ORG_SIZE,
		GDC_F_YUV_GDC_ORG_HEIGHT, gdc_input_height);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INPUT_ORG_SIZE,
		GDC_F_YUV_GDC_ORG_WIDTH, gdc_input_width);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INPUT_CROP_START,
		GDC_F_YUV_GDC_CROP_START_X, gdc_crop_offset_x);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INPUT_CROP_START,
		GDC_F_YUV_GDC_CROP_START_Y, gdc_crop_offset_y);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INPUT_CROP_SIZE,
		GDC_F_YUV_GDC_CROP_WIDTH, gdc_crop_width);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INPUT_CROP_SIZE,
		GDC_F_YUV_GDC_CROP_HEIGHT, gdc_crop_height);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_SCALE, GDC_F_YUV_GDC_SCALE_X, gdc_scale_x);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_SCALE, GDC_F_YUV_GDC_SCALE_Y, gdc_scale_y);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_SCALE_SHIFTER,
		GDC_F_YUV_GDC_SCALE_SHIFTER_X, gdc_scale_shifter_x);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_SCALE_SHIFTER,
		GDC_F_YUV_GDC_SCALE_SHIFTER_Y, gdc_scale_shifter_y);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INV_SCALE,
		GDC_F_YUV_GDC_INV_SCALE_X, gdc_inv_scale_x);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INV_SCALE,
		GDC_F_YUV_GDC_INV_SCALE_Y, gdc_inv_scale_y);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_OUT_CROP_START,
		GDC_F_YUV_GDC_IMAGE_CROP_PRE_X, gdc_output_offset_x);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_OUT_CROP_START,
		GDC_F_YUV_GDC_IMAGE_CROP_PRE_Y, gdc_output_offset_y);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_OUT_CROP_SIZE,
		GDC_F_YUV_GDC_IMAGE_ACTIVE_WIDTH, gdc_output_width);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_OUT_CROP_SIZE,
		GDC_F_YUV_GDC_IMAGE_ACTIVE_HEIGHT, gdc_output_height);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_OUT_SCALE,
		GDC_F_YUV_GDC_OUT_SCALE_Y, out_scaled_height);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_OUT_SCALE,
		GDC_F_YUV_GDC_OUT_SCALE_X, out_scaled_width);

	gdc_dbg("input %dx%d in_crop %d,%d %dx%d -> out_crop %d,%d %dx%d / scale %dx%d\n",
			gdc_input_width, gdc_input_height,
			gdc_crop_offset_x, gdc_crop_offset_y, gdc_crop_width, gdc_crop_height,
			gdc_output_offset_x, gdc_output_offset_y, gdc_output_width, gdc_output_height,
			out_scaled_height, out_scaled_width);
}

bool camerapp_hw_gdc_has_comp_header(u32 comp_extra)
{
	/* Both Lossless & Lossy have header*/
	return true;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_has_comp_header);

static int camerapp_hw_gdc_get_comp_type_size(int payload, int header,
	enum gdc_sbwc_size_type type)
{
	int ret;

	switch (type) {
	case GDC_SBWC_SIZE_PAYLOAD:
		ret = payload;
		break;
	case GDC_SBWC_SIZE_HEADER:
		ret = header;
		break;
	default: /* GDC_SBWC_SIZE_ALL */
		ret = payload + header;
		break;
	}

	return ret;
}

int camerapp_hw_get_comp_buf_size(struct gdc_dev *gdc, struct gdc_frame *frame,
	u32 width, u32 height, u32 pixfmt, enum gdc_buf_plane plane, enum gdc_sbwc_size_type type)
{
	u32 payload_size, header_size;

	switch (pixfmt) {
	case V4L2_PIX_FMT_NV12M_SBWCL_64_8B:
	case V4L2_PIX_FMT_NV12M_SBWCL_64_10B:
		if (plane) {
			payload_size = SBWCL_64_CBCR_SIZE(width, height);
			header_size = SBWCL_CBCR_HEADER_SIZE(width, height);
		} else {
			payload_size = SBWCL_64_Y_SIZE(width, height);
			header_size = SBWCL_Y_HEADER_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_NV12M_SBWCL_32_8B:
	case V4L2_PIX_FMT_NV12M_SBWCL_32_10B:
		if (plane) {
			payload_size = SBWCL_32_CBCR_SIZE(width, height);
			header_size = SBWCL_CBCR_HEADER_SIZE(width, height);
		} else {
			payload_size = SBWCL_32_Y_SIZE(width, height);
			header_size = SBWCL_Y_HEADER_SIZE(width, height);
		}
		break;
	default:
		gdc_info("not supported format values\n");
		return -EINVAL;
	}

	return camerapp_hw_gdc_get_comp_type_size(payload_size, header_size, type);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_get_comp_buf_size);

static void camerapp_hw_gdc_calculate_stride(struct gdc_frame *frame, u32 dma_width,
	u32 *lum_w_header, u32 *chroma_w_header,
	u32 *lum_w, u32 *chroma_w, u32 bytesperline[])
{
	u32 pixfmt = frame->gdc_fmt->pixelformat;

	/* calculate DMA stride size */
	switch (pixfmt) {
	case V4L2_PIX_FMT_NV12M_SBWCL_64_8B:
	case V4L2_PIX_FMT_NV12M_SBWCL_64_10B:
		*lum_w = (u32)(SBWCL_64_STRIDE(dma_width));
		*chroma_w = (u32)(SBWCL_64_STRIDE(dma_width));
		*lum_w_header = (u32)(SBWCL_HEADER_STRIDE(dma_width));
		*chroma_w_header = (u32)(SBWCL_HEADER_STRIDE(dma_width));
		break;
	case V4L2_PIX_FMT_NV12M_SBWCL_32_8B:
	case V4L2_PIX_FMT_NV12M_SBWCL_32_10B:
		*lum_w = (u32)(SBWCL_32_STRIDE(dma_width));
		*chroma_w = (u32)(SBWCL_32_STRIDE(dma_width));
		*lum_w_header = (u32)(SBWCL_HEADER_STRIDE(dma_width));
		*chroma_w_header = (u32)(SBWCL_HEADER_STRIDE(dma_width));
		break;
	case V4L2_PIX_FMT_NV12M_P010:
	case V4L2_PIX_FMT_NV21M_P010:
	case V4L2_PIX_FMT_NV16M_P210:
	case V4L2_PIX_FMT_NV61M_P210:
		*lum_w = max(bytesperline[0], ALIGN(dma_width * 2, 16));
		*chroma_w = max(bytesperline[1], ALIGN(dma_width * 2, 16));
		break;
	default:
		*lum_w = max(bytesperline[0], ALIGN(dma_width, 16));
		*chroma_w = max(bytesperline[1], ALIGN(dma_width, 16));
		break;
	}
}

static void camerapp_hw_gdc_update_dma_size(struct pablo_mmio *pmio,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame,
	struct gdc_crop_param *crop_param)
{
	u32 input_stride_lum_w = 0, input_stride_chroma_w = 0;
	u32 output_stride_lum_w = 0, output_stride_chroma_w = 0;
	u32 output_stride_lum_w_header = 0, output_stride_chroma_w_header = 0;
	u32 input_stride_lum_w_header = 0, input_stride_chroma_w_header = 0;
	u32 in_dma_width, out_dma_width;
	u32 gdc_input_width, gdc_input_height;
	u32 gdc_output_width, gdc_output_height;
/*
 * supported format
 * - input 8bit : YUV422_2P_8bit, YUV420_2P_8bit
 * - input 10bit : YUV422_2P_10bit, YUV420_2P_10bit, P010(P210), 8+2bit
 * - output 8bit : YUV422_2P_8bit, YUV420_2P_8bit
 * - output 10bit : YUV422_2P_10bit, YUV420_2P_10bit, P010(P210), 8+2bit
 * - output SBWC : YUV_CbCr_422_2P_SBWC_8bit/10bit, YUV_CbCr_420_2P_SBWC_8bit/10bit
 */

	/* GDC input / output image size */
	gdc_input_width = s_frame->width;
	gdc_input_height = s_frame->height;
	gdc_output_width = d_frame->width;
	gdc_output_height = d_frame->height;

	out_dma_width = GDC_GET_F(pmio, GDC_R_YUV_GDC_OUT_CROP_SIZE,
		GDC_F_YUV_GDC_IMAGE_ACTIVE_WIDTH);
	in_dma_width = GDC_GET_F(pmio, GDC_R_YUV_GDC_INPUT_ORG_SIZE,
		GDC_F_YUV_GDC_ORG_WIDTH);

	camerapp_hw_gdc_calculate_stride(s_frame, in_dma_width,
		&input_stride_lum_w_header, &input_stride_chroma_w_header,
		&input_stride_lum_w, &input_stride_chroma_w,
		crop_param->src_bytesperline);

	camerapp_hw_gdc_calculate_stride(d_frame, out_dma_width,
		&output_stride_lum_w_header, &output_stride_chroma_w_header,
		&output_stride_lum_w, &output_stride_chroma_w,
		crop_param->dst_bytesperline);

/*
 * src_10bit_format == packed10bit (10bit + 10bit + 10bit... no padding)
 * input_stride_lum_w = (u32)(((in_dma_width * 10 + 7) / 8 + 15) / 16 * 16);
 * input_stride_chroma_w = (u32)(((in_dma_width * 10 + 7) / 8 + 15) / 16 * 16);
 */
	gdc_dbg("s_w = %d, lum stride_w = %d, stride_header_w = %d\n",
		s_frame->width, input_stride_lum_w, input_stride_lum_w_header);
	gdc_dbg("s_w = %d, chroma stride_w = %d, stride_header_w = %d\n",
		s_frame->width, input_stride_chroma_w, input_stride_chroma_w_header);
	gdc_dbg("d_w = %d, lum stride_w = %d, stride_header_w = %d\n",
		d_frame->width, output_stride_lum_w, output_stride_lum_w_header);
	gdc_dbg("d_w = %d, chroma stride_w = %d, stride_header_w = %d\n",
		d_frame->width, output_stride_chroma_w, output_stride_chroma_w_header);

	camerapp_hw_gdc_set_wdma_img_size(pmio, gdc_output_width, gdc_output_height);
	camerapp_hw_gdc_set_rdma_img_size(pmio, gdc_input_width, gdc_input_height);

	/* about write DMA stride size : It should be aligned with 16 bytes */
	camerapp_hw_gdc_set_wdma_stride(pmio,
		output_stride_lum_w, output_stride_chroma_w);
	camerapp_hw_gdc_set_wdma_header_stride(pmio,
		output_stride_lum_w_header, output_stride_chroma_w_header);

	/* about read DMA stride size */
	camerapp_hw_gdc_set_rdma_stride(pmio,
		input_stride_lum_w, input_stride_chroma_w);
	camerapp_hw_gdc_set_rdma_header_stride(pmio,
		input_stride_lum_w_header, input_stride_chroma_w_header);
}

static void camerapp_hw_gdc_set_dma_address(struct pablo_mmio *pmio,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame)
{
	u32 mono_format = (s_frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_GREY)
		&& (d_frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_GREY);
	u32 wdma_enable;

	if (d_frame->io_mode == GDC_OUT_OTF) {
		wdma_enable = 0;
	} else {
		wdma_enable = 1;
		camerapp_hw_gdc_set_wdma_addr(pmio,
			d_frame->addr.y, d_frame->addr.cb);
		camerapp_hw_gdc_set_wdma_header_addr(pmio,
			d_frame->addr.y_2bit, d_frame->addr.cbcr_2bit);
	}
	camerapp_hw_gdc_set_wdma_enable(pmio, wdma_enable);

	/* RDMA address */
	camerapp_hw_gdc_set_rdma_addr(pmio,
		s_frame->addr.y, s_frame->addr.cb);
	camerapp_hw_gdc_set_rdma_header_addr(pmio,
		s_frame->addr.y_2bit, s_frame->addr.cbcr_2bit);
	camerapp_hw_gdc_set_rdma_enable(pmio, 1, !mono_format);
}


static int camerapp_hw_gdc_adjust_fmt(u32 pixelformat,
	u32 *yuv_format, u32 *dma_format, u32 *bit_depth_format)
{
	int ret = 0;

	switch (pixelformat) {
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV12M_SBWCL_8B:
	case V4L2_PIX_FMT_NV12M_SBWCL_32_8B:
	case V4L2_PIX_FMT_NV12M_SBWCL_64_8B:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_UFIRST;
		*bit_depth_format = CAMERA_PIXEL_SIZE_8BIT;
		break;
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV21:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_VFIRST;
		*bit_depth_format = CAMERA_PIXEL_SIZE_8BIT;
		break;
	case V4L2_PIX_FMT_NV12M_P010:
	case V4L2_PIX_FMT_NV12M_SBWCL_10B:
	case V4L2_PIX_FMT_NV12M_SBWCL_32_10B:
	case V4L2_PIX_FMT_NV12M_SBWCL_64_10B:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_UFIRST_P010;
		*bit_depth_format = CAMERA_PIXEL_SIZE_10BIT;
		break;
	case V4L2_PIX_FMT_NV21M_P010:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_VFIRST_P010;
		*bit_depth_format = CAMERA_PIXEL_SIZE_10BIT;
		break;
	case V4L2_PIX_FMT_NV12M_S10B:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_UFIRST_PACKED10;
		*bit_depth_format = CAMERA_PIXEL_SIZE_PACKED_10BIT;
		break;
	case V4L2_PIX_FMT_NV21M_S10B:
		*yuv_format = GDC_YUV420;
		*dma_format = GDC_YUV420_2P_VFIRST_PACKED10;
		*bit_depth_format = CAMERA_PIXEL_SIZE_PACKED_10BIT;
		break;
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV16:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_UFIRST;
		*bit_depth_format = CAMERA_PIXEL_SIZE_8BIT;
		break;
	case V4L2_PIX_FMT_NV61M:
	case V4L2_PIX_FMT_NV61:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_VFIRST;
		*bit_depth_format = CAMERA_PIXEL_SIZE_8BIT;
		break;
	case V4L2_PIX_FMT_NV16M_P210:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_UFIRST_P210;
		*bit_depth_format = CAMERA_PIXEL_SIZE_10BIT;
		break;
	case V4L2_PIX_FMT_NV61M_P210:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_VFIRST_P210;
		*bit_depth_format = CAMERA_PIXEL_SIZE_10BIT;
		break;
	case V4L2_PIX_FMT_NV16M_S10B:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_UFIRST_PACKED10;
		*bit_depth_format = CAMERA_PIXEL_SIZE_PACKED_10BIT;
		break;
	case V4L2_PIX_FMT_NV61M_S10B:
		*yuv_format = GDC_YUV422;
		*dma_format = GDC_YUV422_2P_VFIRST_PACKED10;
		*bit_depth_format = CAMERA_PIXEL_SIZE_PACKED_10BIT;
		break;
	default:
		yuv_format = 0;
		dma_format = 0;
		bit_depth_format = 0;
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void camerapp_hw_gdc_set_format(struct pablo_mmio *pmio,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame)
{
	u32 input_10bit_format, output_10bit_format;
	u32 input_yuv_format, output_yuv_format;
	u32 input_yuv_dma_format, output_yuv_dma_format;
	u32 mono_format;

	/* PIXEL_FORMAT:	0: NV12 (2plane Y/UV order), 1: NV21 (2plane Y/VU order) */
	/* YUV bit depth:	0: 8bit, 1: P010 , 2: reserved, 3: packed 10bit */
	/* YUV format:		0: YUV422, 1: YUV420 */
	/* MONO format:		0: YCbCr, 1: Y only(ignore UV related SFRs) */

	/* Check MONO format */
	mono_format = (s_frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_GREY)
			&& (d_frame->gdc_fmt->pixelformat == V4L2_PIX_FMT_GREY);
	if (mono_format) {
		gdc_dbg("gdc mono_mode enabled\n");
		camerapp_hw_gdc_set_mono_mode(pmio, mono_format);
		return;
	}

	/* input */
	if (camerapp_hw_gdc_adjust_fmt(s_frame->gdc_fmt->pixelformat,
		&input_yuv_format, &input_yuv_dma_format, &input_10bit_format)) {
		gdc_info("not supported src pixelformat(%c%c%c%c)\n",
			(char)((s_frame->gdc_fmt->pixelformat >> 0) & 0xFF),
			(char)((s_frame->gdc_fmt->pixelformat >> 8) & 0xFF),
			(char)((s_frame->gdc_fmt->pixelformat >> 16) & 0xFF),
			(char)((s_frame->gdc_fmt->pixelformat >> 24) & 0xFF));
	}

	/* output */
	if (camerapp_hw_gdc_adjust_fmt(d_frame->gdc_fmt->pixelformat,
		&output_yuv_format, &output_yuv_dma_format, &output_10bit_format)) {
		gdc_info("not supported dst pixelformat(%c%c%c%c)\n",
			(char)((d_frame->gdc_fmt->pixelformat >> 0) & 0xFF),
			(char)((d_frame->gdc_fmt->pixelformat >> 8) & 0xFF),
			(char)((d_frame->gdc_fmt->pixelformat >> 16) & 0xFF),
			(char)((d_frame->gdc_fmt->pixelformat >> 24) & 0xFF));
	}

	gdc_dbg("gdc format (10bit, pix, yuv) : in(%d, %d, %d), out(%d, %d, %d), mono(%d)\n",
		input_10bit_format, input_yuv_dma_format, input_yuv_format,
		output_10bit_format, output_yuv_dma_format, output_yuv_format,
		mono_format);

	/* IN/OUT Format */
	GDC_SET_F(pmio, GDC_R_YUV_GDC_YUV_FORMAT,
		GDC_F_YUV_GDC_DST_10BIT_FORMAT, output_10bit_format);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_YUV_FORMAT,
		GDC_F_YUV_GDC_SRC_10BIT_FORMAT, input_10bit_format);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_YUV_FORMAT,
		GDC_F_YUV_GDC_YUV_FORMAT, input_yuv_format);

	camerapp_hw_gdc_set_wdma_data_format(pmio, output_yuv_dma_format);
	camerapp_hw_gdc_set_rdma_data_format(pmio, input_yuv_dma_format);
}

static void camerapp_hw_gdc_set_core_param(struct pablo_mmio *pmio,
	struct gdc_crop_param *crop_param)
{
	u32 luma_min, chroma_min;
	u32 luma_max, chroma_max;
	u32 input_bit_depth;

	luma_min = chroma_min = 0;

	/* interpolation type
	 * 0 -all closest / 1 - Y bilinear, UV closest
	 * 2 - all bilinear / 3 - Y bi-cubic, UV bilinear */
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INTERPOLATION,
		GDC_F_YUV_GDC_INTERP_TYPE, 0x3);

	/* Clamping type: 0 - none / 1 - min/max / 2 - near pixels min/max */
	GDC_SET_F(pmio, GDC_R_YUV_GDC_INTERPOLATION,
		GDC_F_YUV_GDC_CLAMP_TYPE, 0x2);

	/* gdc boundary option setting : 0 - constant / 1 - mirroring */
	GDC_SET_F(pmio, GDC_R_YUV_GDC_BOUNDARY_OPTION,
		GDC_F_YUV_GDC_BOUNDARY_OPTION, 0x1);

	/* gdc grid mode : 0 - input / 1 - output */
	GDC_SET_F(pmio, GDC_R_YUV_GDC_GRID_MODE,
		GDC_F_YUV_GDC_GRID_MODE, crop_param->is_grid_mode);

	/* gdc bypass mode: : 0 - off / 1 - on */
	GDC_SET_F(pmio, GDC_R_YUV_GDC_YUV_FORMAT,
		GDC_F_YUV_GDC_BYPASS_MODE, crop_param->is_bypass_mode);

	/* output pixcel min/max value clipping */

	input_bit_depth = GDC_GET_F(pmio, GDC_R_YUV_GDC_YUV_FORMAT,
		GDC_F_YUV_GDC_SRC_10BIT_FORMAT);
	if (input_bit_depth == CAMERA_PIXEL_SIZE_8BIT)
		luma_max = chroma_max = 0xFF;
	else
		luma_max = chroma_max = 0x3FF;

	GDC_SET_F(pmio, GDC_R_YUV_GDC_LUMA_MINMAX,
		GDC_F_YUV_GDC_LUMA_MIN, luma_min);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_LUMA_MINMAX,
		GDC_F_YUV_GDC_LUMA_MAX, luma_max);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_CHROMA_MINMAX,
		GDC_F_YUV_GDC_CHROMA_MIN, chroma_min);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_CHROMA_MINMAX,
		GDC_F_YUV_GDC_CHROMA_MAX, chroma_max);
}

static void camerapp_hw_gdc_get_sbwc_type(struct gdc_frame *frame,
	u32 *comp_type, u32 *align_64b)
{
	switch (frame->extra) {
	case COMP:
		*comp_type = COMP;
		*align_64b = 0;
		break;
	case COMP_LOSS:
		*comp_type = COMP_LOSS;
		if (frame->pixelformat == V4L2_PIX_FMT_NV12M_SBWCL_64_8B ||
			frame->pixelformat == V4L2_PIX_FMT_NV12M_SBWCL_64_10B)
			*align_64b = 1;
		else
			*align_64b = 0;
		break;
	default:
		*comp_type = NONE;
		*align_64b = 0;
		break;
	}
}

static void camerapp_hw_gdc_set_compressor(struct pablo_mmio *pmio,
	struct gdc_frame *s_frame, struct gdc_frame *d_frame)
{
	u32 s_comp_type, d_comp_type;
	u32 s_comp_64b_align, d_comp_64b_align;

	camerapp_hw_gdc_get_sbwc_type(s_frame, &s_comp_type, &s_comp_64b_align);
	camerapp_hw_gdc_get_sbwc_type(d_frame, &d_comp_type, &d_comp_64b_align);

	gdc_dbg("src_comp_type(%d) dst_comp_type(%d)\n", s_comp_type, d_comp_type);

	if (s_comp_type) {
		GDC_SET_F(pmio, GDC_R_YUV_GDC_COMPRESSOR,
			GDC_F_YUV_GDC_SRC_COMPRESSOR, s_comp_type);
		camerapp_hw_gdc_set_rdma_sbwc_enable(pmio, s_comp_type);
		camerapp_hw_gdc_set_rdma_sbwc_64b_align(pmio, s_comp_64b_align);
		if (s_comp_type == COMP_LOSS)
			camerapp_hw_gdc_set_rdma_comp_control(pmio, 0);

	}

	if (d_comp_type && d_frame->io_mode != GDC_OUT_OTF) {
		/* M2M or vOTF */
		GDC_SET_F(pmio, GDC_R_YUV_GDC_COMPRESSOR,
			GDC_F_YUV_GDC_DST_COMPRESSOR, d_comp_type);
		camerapp_hw_gdc_set_wdma_sbwc_enable(pmio, d_comp_type);
		camerapp_hw_gdc_set_wdma_sbwc_64b_align(pmio, d_comp_64b_align);
		if (d_comp_type == COMP_LOSS)
			camerapp_hw_gdc_set_wdma_comp_control(pmio, 0);
	}
}

int camerapp_hw_check_sbwc_fmt(u32 pixelformat)
{
	if ((pixelformat != V4L2_PIX_FMT_NV12M_SBWCL_32_8B) &&
		(pixelformat != V4L2_PIX_FMT_NV12M_SBWCL_32_10B) &&
		(pixelformat != V4L2_PIX_FMT_NV12M_SBWCL_64_8B) &&
		(pixelformat != V4L2_PIX_FMT_NV12M_SBWCL_64_10B))
		return -EINVAL;

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_check_sbwc_fmt);

int camerapp_hw_get_sbwc_constraint(u32 pixelformat, u32 width, u32 height, u32 type)
{
	/* Support NV12 format for v2.7 */
	if (camerapp_hw_check_sbwc_fmt(pixelformat)) {
		gdc_info("No sbwc format (%c%c%c%c)\n",
			(char)((pixelformat >> 0) & 0xFF),
			(char)((pixelformat >> 8) & 0xFF),
			(char)((pixelformat >> 16) & 0xFF),
			(char)((pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_get_sbwc_constraint);

static void camerapp_hw_gdc_set_output_select(struct pablo_mmio *pmio,
	struct gdc_crop_param *crop_param)
{
	u32 select, size;

	if (crop_param->out_mode == GDC_OUT_OTF) {
		select = GDC_OUTPUT_OTF;
		size = GDC_TILE_SIZE_OTF;
		gdc_dbg("out mode is OTF\n");
	} else {
		select = GDC_OUTPUT_WDMA;
		size = GDC_TILE_SIZE_WDMA;
		gdc_dbg("out mode is WDMA\n");
	}

	GDC_SET_F(pmio, GDC_R_YUV_GDC_OUTPUT_SELECT, GDC_F_YUV_GDC_OUTPUT_SELECT, select);
	GDC_SET_F(pmio, GDC_R_YUV_GDC_OUTPUT_SELECT, GDC_F_YUV_GDC_OUTPUT_TILE, size);
}

static void camerapp_hw_gdc_votf_enable(struct pablo_mmio *pmio, u8 rw)
{
	u32 reg_value = 0;

	if (rw == TRS) {
		GDC_SET_F(pmio, GDC_R_YUV_RDMAY_VOTF_EN, GDC_F_YUV_RDMAY_VOTF_EN, 0x1);
		GDC_SET_F(pmio, GDC_R_YUV_RDMAUV_VOTF_EN, GDC_F_YUV_RDMAUV_VOTF_EN, 0x1);
	} else {
		reg_value = GDC_SET_V(pmio, reg_value, GDC_F_YUV_WDMA_VOTF_EN, 0x1);
		reg_value = GDC_SET_V(pmio, reg_value, GDC_F_YUV_WDMA_VOTF_STALL, 0x1);
		GDC_SET_R(pmio, GDC_R_YUV_WDMA_VOTF_EN, reg_value);
	}
}

static void camerapp_hw_gdc_set_llc(struct pablo_mmio *pmio, u32 comp)
{
	u32 en_32b_pa = 0;
	u32 businfo = IS_LLC_CACHE_HINT_VOTF_TYPE << IS_LLC_CACHE_HINT_SHIFT;

	if (comp) {
		businfo |= 1 << IS_32B_WRITE_ALLOC_SHIFT;
		en_32b_pa = 1;
	}

	GDC_SET_F(pmio, GDC_R_YUV_WDMA_BUSINFO, GDC_F_YUV_WDMA_BUSINFO, businfo);
	GDC_SET_F(pmio, GDC_R_YUV_WDMA_CACHE_CONTROL,
		GDC_F_YUV_WDMA_32B_WRITE_ALLOC_CONTROL, en_32b_pa);
}

void camerapp_hw_gdc_update_param(struct pablo_mmio *pmio, struct gdc_dev *gdc)
{
	struct gdc_frame *d_frame, *s_frame;
	struct gdc_crop_param *crop_param;

	s_frame = &gdc->current_ctx->s_frame;
	d_frame = &gdc->current_ctx->d_frame;
	crop_param = &gdc->current_ctx->crop_param[gdc->current_ctx->cur_index];

	/* gdc grid scale setting */
	camerapp_hw_gdc_update_scale_parameters(pmio, s_frame, d_frame, crop_param);
	/* gdc grid setting */
	camerapp_hw_gdc_update_grid_param(pmio, crop_param);
	/* gdc SBWC */
	camerapp_hw_gdc_set_compressor(pmio, s_frame, d_frame);
	/* in/out data Format */
	camerapp_hw_gdc_set_format(pmio, s_frame, d_frame);
	/* dma buffer size & address setting */
	camerapp_hw_gdc_set_dma_address(pmio, s_frame, d_frame);
	camerapp_hw_gdc_update_dma_size(pmio, s_frame, d_frame, crop_param);
	if (crop_param->out_mode == GDC_OUT_VOTF) {
		camerapp_hw_gdc_votf_enable(pmio, TWS);
		camerapp_hw_gdc_set_llc(pmio, d_frame->extra);
	}
	/* gdc core */
	camerapp_hw_gdc_set_core_param(pmio, crop_param);
	camerapp_hw_gdc_set_output_select(pmio, crop_param);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_update_param);

void camerapp_hw_gdc_status_read(struct pablo_mmio *pmio)
{
	u32 rdmaStatus;
	u32 wdmaStatus;

	wdmaStatus = GDC_GET_F(pmio, GDC_R_YUV_GDC_BUSY, GDC_F_YUV_GDC_WDMA_BUSY);
	rdmaStatus = GDC_GET_F(pmio, GDC_R_YUV_GDC_BUSY, GDC_F_YUV_GDC_RDMA_BUSY);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_status_read);

void camerapp_gdc_sfr_dump(void __iomem *base_addr)
{
	u32 i;
	u32 reg_value;

	gdc_info("gdc v12.0");

	for (i = 0; i < GDC_CTRL_REG_CNT; i++) {
		reg_value = readl(base_addr + gdc_regs[i].sfr_offset);
		pr_info("[@][SFR][DUMP] reg:[%s][0x%04X], value:[0x%08X]\n",
			gdc_regs[i].reg_name, gdc_regs[i].sfr_offset, reg_value);
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_gdc_sfr_dump);

u32 camerapp_hw_gdc_g_reg_cnt(void)
{
	return GDC_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_g_reg_cnt);

void camerapp_hw_gdc_init_pmio_config(struct gdc_dev *gdc)
{
	struct pmio_config *cfg = &gdc->pmio_config;

	cfg->name = "gdc";
	cfg->mmio_base = gdc->regs_base;

	if (!IS_ENABLED(GDC_USE_MMIO)) {
		cfg->volatile_table = &gdc_volatile_table;
		cfg->wr_table = &gdc_wr_table;
		cfg->max_register = GDC_R_YUV_GDC_GRID_DY_0_1088;
		cfg->reg_defaults_raw = gdc_sfr_reset_value;
		cfg->num_reg_defaults_raw = GDC_REG_TOTAL_CNT;
		cfg->phys_base = gdc->regs_rsc->start;
		cfg->dma_addr_shift = 4;
		cfg->fields = gdc_field_descs;
		cfg->num_fields = ARRAY_SIZE(gdc_field_descs);
		cfg->cache_type = PMIO_CACHE_FLAT;
		gdc->pmio_en = true;
	} else {
		cfg->fields = NULL;
		cfg->num_fields = 0;
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_gdc_init_pmio_config);
