/*
 * Samsung EXYNOS CAMERA PostProcessing driver
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_HW_API_GDC_H
#define CAMERAPP_HW_API_GDC_H

#include "pablo-gdc.h"
#include "pablo-hw-api-common.h"

#define GDC_SETFILE_VERSION	0x12345678

#define DS_FRACTION_BITS		8
#define DS_SHIFTER_MAX			7
#define MAX_OUTPUT_SCALER_RATIO	2

#define GDC_USE_MMIO 0

#define GDC_CFG_FMT_YCBCR420_2P	(0 << 0)
#define GDC_CFG_FMT_YUYV		(0xa << 0)
#define GDC_CFG_FMT_UYVY		(0xb << 0)
#define GDC_CFG_FMT_YVYU		(9 << 0)
#define GDC_CFG_FMT_YCBCR422_2P	(2 << 0)
#define GDC_CFG_FMT_YCBCR444_2P	(3 << 0)
#define GDC_CFG_FMT_RGB565		(4 << 0)
#define GDC_CFG_FMT_ARGB1555		(5 << 0)
#define GDC_CFG_FMT_ARGB4444		(0xc << 0)
#define GDC_CFG_FMT_ARGB8888		(6 << 0)
#define GDC_CFG_FMT_RGBA8888		(0xe << 0)
#define GDC_CFG_FMT_P_ARGB8888	(7 << 0)
#define GDC_CFG_FMT_L8A8		(0xd << 0)
#define GDC_CFG_FMT_L8		(0xf << 0)
#define GDC_CFG_FMT_YCRCB420_2P	(0x10 << 0)
#define GDC_CFG_FMT_YCRCB422_2P	(0x12 << 0)
#define GDC_CFG_FMT_YCRCB444_2P	(0x13 << 0)
#define GDC_CFG_FMT_YCBCR420_3P	(0x14 << 0)
#define GDC_CFG_FMT_YCBCR422_3P	(0x16 << 0)
#define GDC_CFG_FMT_YCBCR444_3P	(0x17 << 0)
#define GDC_CFG_FMT_P010_16B_2P	(0x18 << 0)
#define GDC_CFG_FMT_P210_16B_2P	(0x19 << 0)
#define GDC_CFG_FMT_YCRCB422_8P2_2P	(0x1a << 0)
#define GDC_CFG_FMT_YCRCB420_8P2_2P	(0x1b << 0)
#define GDC_CFG_FMT_NV12M_SBWC_LOSSY_8B	(0x1c << 0)
#define GDC_CFG_FMT_NV12M_SBWC_LOSSY_10B	(0x1d << 0)

void camerapp_hw_gdc_start(struct pablo_mmio *pmio, struct c_loader_buffer *clb);
void camerapp_hw_gdc_stop(struct pablo_mmio *pmio);
u32 camerapp_hw_gdc_sw_reset(struct pablo_mmio *pmio);
void camerapp_hw_gdc_set_initialization(struct pablo_mmio *pmio);
void camerapp_hw_gdc_update_param(struct pablo_mmio *pmio, struct gdc_dev *gdc);
void camerapp_hw_gdc_status_read(struct pablo_mmio *pmio);
void camerapp_gdc_sfr_dump(void __iomem *base_addr);
u32 camerapp_hw_gdc_get_intr_status_and_clear(struct pablo_mmio *pmio);
u32 camerapp_hw_gdc_get_int_frame_start(void);
u32 camerapp_hw_gdc_get_int_frame_end(void);
void camerapp_hw_gdc_votf_enable(struct pablo_mmio *pmio, u8 rw);
u32 camerapp_hw_gdc_get_ver(struct pablo_mmio *pmio);
int camerapp_hw_get_sbwc_constraint(u32 pixelformat, u32 width, u32 height, u32 type);
int camerapp_hw_check_sbwc_fmt(u32 pixelformat);
bool camerapp_hw_gdc_has_comp_header(u32 comp_extra);
int camerapp_hw_get_comp_buf_size(struct gdc_dev *gdc, struct gdc_frame *frame,
	u32 width, u32 height, u32 pixfmt, enum gdc_buf_plane plane, enum gdc_sbwc_size_type type);
const struct gdc_variant *camerapp_hw_gdc_get_size_constraints(struct pablo_mmio *pmio);
void camerapp_hw_gdc_init_pmio_config(struct gdc_dev *gdc);
u32 camerapp_hw_gdc_g_reg_cnt(void);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct camerapp_kunit_hw_gdc_func {
	u32 (*camerapp_hw_gdc_get_ver)(struct pablo_mmio *pmio);
	void (*camerapp_gdc_sfr_dump)(void __iomem *base_addr);
	const struct gdc_variant *(*camerapp_hw_gdc_get_size_constraints)(struct pablo_mmio *pmio);
	void (*camerapp_hw_gdc_start)(struct pablo_mmio *pmio, struct c_loader_buffer *clb);
	void (*camerapp_hw_gdc_stop)(struct pablo_mmio *pmio);
	u32 (*camerapp_hw_gdc_sw_reset)(struct pablo_mmio *pmio);
	u32 (*camerapp_hw_gdc_get_intr_status_and_clear)(struct pablo_mmio *pmio);
	u32 (*camerapp_hw_gdc_get_int_frame_start)(void);
	u32 (*camerapp_hw_gdc_get_int_frame_end)(void);
	void (*camerapp_hw_gdc_votf_enable)(struct pablo_mmio *pmio, u8 rw);
	int (*camerapp_hw_get_sbwc_constraint)(u32 pixelformat, u32 width, u32 height, u32 type);
	bool (*camerapp_hw_gdc_has_comp_header)(u32 comp_extra);
	int (*camerapp_hw_get_comp_buf_size)(struct gdc_dev *gdc, struct gdc_frame *frame,
		u32 width, u32 height, u32 pixfmt, enum gdc_buf_plane plane, enum gdc_sbwc_size_type type);
	void (*camerapp_hw_gdc_set_initialization)(struct pablo_mmio *pmio);
	void (*camerapp_hw_gdc_update_scale_parameters)(struct pablo_mmio *pmio,
		struct gdc_frame *s_frame, struct gdc_frame *d_frame, struct gdc_crop_param *crop_param);
	void (*camerapp_hw_gdc_update_grid_param)(struct pablo_mmio *pmio,
		struct gdc_crop_param *crop_param);
	void (*camerapp_hw_gdc_set_compressor)(struct pablo_mmio *pmio,
		struct gdc_frame *s_frame, struct gdc_frame *d_frame);
	void (*camerapp_hw_gdc_set_format)(struct pablo_mmio *pmio,
		struct gdc_frame *s_frame, struct gdc_frame *d_frame);
	void (*camerapp_hw_gdc_set_dma_address)(struct pablo_mmio *pmio,
		struct gdc_frame *s_frame, struct gdc_frame *d_frame);
	void (*camerapp_hw_gdc_update_dma_size)(struct pablo_mmio *pmio,
		struct gdc_frame *s_frame, struct gdc_frame *d_frame, struct gdc_crop_param *crop_param);
	void (*camerapp_hw_gdc_set_core_param)(struct pablo_mmio *pmio,
		struct gdc_crop_param *crop_param);
	void (*camerapp_hw_gdc_set_output_select)(struct pablo_mmio *pmio,
		struct gdc_crop_param *crop_param);
	void (*camerapp_hw_gdc_update_param)(struct pablo_mmio *pmio, struct gdc_dev *gdc);
	u32 (*camerapp_hw_gdc_g_reg_cnt)(void);
	void (*camerapp_hw_gdc_init_pmio_config)(struct gdc_dev *gdc);
};
struct camerapp_kunit_hw_gdc_func *camerapp_kunit_get_hw_gdc_test(void);
#endif
#endif
