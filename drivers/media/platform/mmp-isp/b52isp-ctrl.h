/*
 * Copyright (C) 2013 Marvell International Ltd.
 *				 Jianle Wang <wanjl@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef __B52ISP_CTRL_H__
#define __B52ISP_CTRL_H__

#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <uapi/media/b52_api.h>

#define NR_METERING_MODE (0x4)

struct b52isp_ctrls {
	struct v4l2_ctrl_handler ctrl_handler;

	/* Auto white balance */
	struct v4l2_ctrl *auto_wb;
	/* Adjust - contrast */
	struct v4l2_ctrl *contrast;
	/* Adjust - saturation */
	struct v4l2_ctrl *saturation;
	/* Adjust - sharpness */
	struct v4l2_ctrl *sharpness;
	/* Adjust - brightness */
	struct v4l2_ctrl *brightness;
	/* Adjust - hue */
	struct v4l2_ctrl *hue;

	/* power line frequence and band filter*/
	struct v4l2_ctrl *pwr_line_freq;
	struct v4l2_ctrl *band_filter;

	struct {
		/* exposure/auto exposure cluster */
		struct v4l2_ctrl *auto_expo;
		struct v4l2_ctrl *exposure;
		struct v4l2_ctrl *expo_line;
		struct v4l2_ctrl *expo_bias;
	};
	struct {
		/* continuous auto focus/auto focus cluster */
		struct v4l2_ctrl *auto_focus;
		struct v4l2_ctrl *f_distance;
		struct v4l2_ctrl *af_start;
		struct v4l2_ctrl *af_stop;
		struct v4l2_ctrl *af_status;
		struct v4l2_ctrl *af_range;
		struct v4l2_ctrl *af_mode;
		struct v4l2_ctrl *af_5x5_win;
	};
	struct {
		/* Auto ISO control cluster */
		struct v4l2_ctrl *auto_iso;
		struct v4l2_ctrl *iso;
	};

	struct {
		/* Auto gain control cluster */
		struct v4l2_ctrl *auto_gain;
		struct v4l2_ctrl *gain;
	};
	struct v4l2_ctrl *aec_manual_mode;
	struct {
		/* Auto frame rate cluster */
		struct v4l2_ctrl *auto_frame_rate;
		struct v4l2_ctrl *afr_min_fps;
		struct v4l2_ctrl *afr_sr_min_fps;
		struct v4l2_ctrl *afr_max_gain;
	};

	/* AE/AWB/AF lock/unlock */
	struct v4l2_ctrl *aaa_lock;
	/* Exposure metering mode */
	struct v4l2_ctrl *exp_metering;
	/* preview zoom*/
	struct v4l2_ctrl *zoom;
	/* ISP image effect */
	struct v4l2_ctrl *colorfx;
	/* ISP target Y */
	struct v4l2_ctrl *target_y;
	/* ISP mean Y */
	struct v4l2_ctrl *mean_y;
	/* ISP AEC stable */
	struct v4l2_ctrl *aec_stable;
	/* ISP band step */
	struct v4l2_ctrl *band_step;
	/* ISP set fps */
	struct v4l2_ctrl *set_fps;
	/* ISP set max_expo */
	struct v4l2_ctrl *max_expo;
	/* user config data*/
	struct v4l2_rect af_win;
	struct b52isp_win metering_roi;
	struct b52isp_expo_metering metering_mode[NR_METERING_MODE];
};

int b52isp_init_ctrls(struct b52isp_ctrls *ctrls);
int b52isp_rw_awb_gain(struct b52isp_awb_gain *awb_gain, int id);
void b52isp_ctrl_reset_bp_val(void);
#endif
