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

void camerapp_hw_gdc_start(struct pablo_mmio *pmio, struct c_loader_buffer *clb);
void camerapp_hw_gdc_stop(struct pablo_mmio *pmio);
u32 camerapp_hw_gdc_sw_reset(struct pablo_mmio *pmio);
void camerapp_hw_gdc_set_initialization(struct pablo_mmio *pmio);
int camerapp_hw_gdc_update_param(struct pablo_mmio *pmio, struct gdc_dev *gdc);
void camerapp_hw_gdc_status_read(struct pablo_mmio *pmio);
void camerapp_gdc_sfr_dump(void __iomem *base_addr);
u32 camerapp_hw_gdc_get_intr_status_and_clear(struct pablo_mmio *pmio);
u32 camerapp_hw_gdc_get_int_frame_start(void);
u32 camerapp_hw_gdc_get_int_frame_end(void);
u32 camerapp_hw_gdc_get_ver(struct pablo_mmio *pmio);
int camerapp_hw_get_sbwc_constraint(u32 pixelformat, u32 width, u32 height, u32 type);
int camerapp_hw_check_sbwc_fmt(u32 pixelformat);
bool camerapp_hw_gdc_has_comp_header(u32 comp_extra);
int camerapp_hw_get_comp_buf_size(struct gdc_dev *gdc, struct gdc_frame *frame,
	u32 width, u32 height, u32 pixfmt, enum gdc_buf_plane plane, enum gdc_sbwc_size_type type);
const struct gdc_variant *camerapp_hw_gdc_get_size_constraints(struct pablo_mmio *pmio);
void camerapp_hw_gdc_init_pmio_config(struct gdc_dev *gdc);
u32 camerapp_hw_gdc_g_reg_cnt(void);

struct pablo_camerapp_hw_gdc {
	u32 (*sw_reset)(struct pablo_mmio *pmio);
	void (*set_initialization)(struct pablo_mmio *pmio);
	int (*update_param)(struct pablo_mmio *pmio, struct gdc_dev *gdc);
	void (*start)(struct pablo_mmio *pmio, struct c_loader_buffer *clb);
	void (*sfr_dump)(void __iomem *base_addr);
};
struct pablo_camerapp_hw_gdc *pablo_get_hw_gdc_ops(void);
#endif
