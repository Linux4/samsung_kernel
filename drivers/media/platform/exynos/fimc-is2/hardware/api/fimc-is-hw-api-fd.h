/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_HW_API_FD_H
#define FIMC_IS_HW_API_FD_H

#include "fimc-is-hw-api-common.h"
#include "../../interface/fimc-is-interface-ischain.h"

enum hw_api_fd_format_mode {
	FD_FORMAT_PREVIEW_MODE, /* OTF mode */
	FD_FORMAT_PLAYBACK_MODE, /* M2M mode */
	FD_FORMAT_MODE_END
};

enum fimc_is_lib_fd_cbcr_format {
	FD_FORMAT_CBCR,
	FD_FORMAT_CRCB,
	FD_FORMAT_FIRST_CBCR,
	FD_FORMAT_SECOND_CBCR,
};

struct hw_api_fd_setfile_half {
	u32 setfile_version;
	u32 skip_frames;
	u32 flag;
	u32 min_face;
	u32 max_face;
	u32 central_lock_area_percent_w;
	u32 central_lock_area_percent_h;
	u32 central_max_face_num_limit;
	u32 frame_per_lock;
	u32 smooth;
	s32 boost_fd_vs_fd;
	u32 max_face_cnt;
	s32 boost_fd_vs_speed;
	u32 min_face_size_feature_detect;
	u32 max_face_size_feature_detect;
	s32 keep_faces_over_frame;
	s32 keep_limit_no_face;
	u32 frame_per_lock_no_face;
	s32 lock_angle[16];
};

struct hw_api_fd_setfile {
	struct hw_api_fd_setfile_half preveiw;
	struct hw_api_fd_setfile_half playback;
};

struct hw_api_fd_map {
	u32 dvaddr_0;
	u32 dvaddr_1;
	u32 dvaddr_2;
	u32 dvaddr_3;
	u32 dvaddr_4;
	u32 dvaddr_5;
	u32 dvaddr_6;
	u32 dvaddr_7;
};

/* fimc-is-fd get functions */
u32 fimc_is_fd_get_version(void __iomem *base_addr);
u32 fimc_is_fd_get_intr_status(void __iomem *base_addr);
u32 fimc_is_fd_get_intr_mask(void __iomem *base_addr);
u32 fimc_is_fd_get_buffer_status(void __iomem *base_addr);
u32 fimc_is_fd_get_input_width(void __iomem *base_addr);
u32 fimc_is_fd_get_input_height(void __iomem *base_addr);
u32 fimc_is_fd_get_input_start_x(void __iomem *base_addr);
u32 fimc_is_fd_get_input_start_y(void __iomem *base_addr);
u32 fimc_is_fd_get_down_sampling_x(void __iomem *base_addr);
u32 fimc_is_fd_get_down_sampling_y(void __iomem *base_addr);
u32 fimc_is_fd_get_output_width(void __iomem *base_addr);
u32 fimc_is_fd_get_output_height(void __iomem *base_addr);
u32 fimc_is_fd_get_output_size(void __iomem *base_addr);
u32 fimc_is_fd_get_ycc_format(void __iomem *base_addr);
u32 fimc_is_fd_get_playback_mode(void __iomem *base_addr);
u32 fimc_is_fd_get_sat(void __iomem *base_addr);

/* fimc-is-fd set functions */
void fimc_is_fd_clear_intr(void __iomem *base_addr, u32 intr);
void fimc_is_fd_enable_intr(void __iomem *base_addr, u32 intr);
void fimc_is_fd_enable(void __iomem *base_addr, bool enable);
void fimc_is_fd_input_mode(void __iomem *base_addr, enum hw_api_fd_format_mode);
void fimc_is_fd_set_input_point(void __iomem *base_addr, u32 start_x, u32 start_y);
void fimc_is_fd_set_input_size(void __iomem *base_addr, u32 width, u32 height, u32 size);
void fimc_is_fd_set_down_sampling(void __iomem *base_addr, u32 scale_x, u32 scale_y);
void fimc_is_fd_set_output_size(void __iomem *base_addr, u32 width, u32 height, u32 size);
void fimc_is_fd_ycc_format(void __iomem *base_addr, u32 format);
void fimc_is_fd_generate_end_intr(void __iomem *base_addr, bool generate);
void fimc_is_fd_shadow_sw(void __iomem *base_addr, bool generate);
void fimc_is_fd_skip_frame(void __iomem *base_addr, u32 value);
void fimc_is_fd_set_coefk(void __iomem *base_addr, u32 coef_k);
void fimc_is_fd_set_shift(void __iomem *base_addr, int shift);
void fimc_is_fd_set_map_addr(void __iomem *base_addr, struct hw_api_fd_map *out_map);
void fimc_is_fd_set_ymap_addr(void __iomem *base_addr, u8 *y_map);
void fimc_is_fd_config_by_setfile(void __iomem *base_addr, struct hw_api_fd_setfile_half *setfile);
void fimc_is_fd_set_cbcr_align(void __iomem *base_addr, enum fimc_is_lib_fd_cbcr_format cbcr);

int fimc_is_fd_sw_reset(void __iomem *base_addr);

#endif
