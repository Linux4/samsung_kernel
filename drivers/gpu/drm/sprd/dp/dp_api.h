// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __DP_API_H__
#define __DP_API_H__

#include "dw_dptx.h"

u8 dptx_get_video_format(struct dptx *dptx);
u8 dptx_get_link_lane_count(struct dptx *dptx);
u8 dptx_get_link_rate(struct dptx *dptx);
u8 dptx_get_pixel_enc(struct dptx *dptx);
u8 dptx_get_bpc(struct dptx *dptx);
u8 dptx_get_video_colorimetry(struct dptx *dptx);
u8 dptx_get_video_dynamic_range(struct dptx *dptx);

int dptx_set_pixel_enc(struct dptx *dptx, u8 pix_enc);
int dptx_set_bpc(struct dptx *dptx, u8 bpc);
int dptx_set_video_colorimetry(struct dptx *dptx, u8 video_col);
int dptx_set_video_dynamic_range(struct dptx *dptx, u8 dynamic_range);
int dptx_set_video_format(struct dptx *dptx, u8 video_format);

int dptx_link_retrain(struct dptx *dptx, u8 rate, u8 lanes);
#endif /* __DP_API_H__ */
