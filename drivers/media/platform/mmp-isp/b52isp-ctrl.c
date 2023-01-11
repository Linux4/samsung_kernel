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

#include <linux/slab.h>
#include <media/b52socisp/b52socisp-mdev.h>
#include <media/b52-sensor.h>

#include "b52isp-ctrl.h"
#include "plat_cam.h"
#include "b52-reg.h"
#include "b52isp.h"

/*
 * save the blue print value
 */
static u8 low_def[B52_NR_PIPELINE_MAX];
static u8 high_def[B52_NR_PIPELINE_MAX];

#define USER_GAIN_BIT 16
#define SENSOR_GAIN_BIT 4
#define GAIN_CONVERT (USER_GAIN_BIT - SENSOR_GAIN_BIT)

#define AWB_IDX_OFFSET (AWB_CUSTOMIZED_BASE - 2)

const struct v4l2_ctrl_ops b52isp_ctrl_ops;

static struct v4l2_ctrl_config b52isp_ctrl_af_mode_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_AF_MODE,
	.name = "AF mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = CID_AF_SNAPSHOT,
	.max = CID_AF_CONTINUOUS,
	.step = 1,
	.def = CID_AF_SNAPSHOT,
};

static struct v4l2_ctrl_config b52isp_ctrl_colorfx_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_COLORFX,
	.name = "color effect",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = V4L2_PRIV_COLORFX_NONE,
	.max = V4L2_PRIV_COLORFX_MAX - 1,
	.step = 1,
	.def = V4L2_PRIV_COLORFX_NONE,
};

static struct v4l2_ctrl_config b52isp_ctrl_auto_frame_rate_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_AUTO_FRAME_RATE,
	.name = "auto frame rate",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = CID_AUTO_FRAME_RATE_DISABLE,
	.max = CID_AUTO_FRAME_RATE_ENABLE,
	.step = 1,
	.def = CID_AUTO_FRAME_RATE_ENABLE,
};

static struct v4l2_ctrl_config b52isp_ctrl_afr_min_fps_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_AFR_MIN_FPS,
	.name = "MIN FPS for AFR",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = CID_AFR_MIN_FPS_MIN,
	.max = CID_AFR_MIN_FPS_MAX,
	.step = 1,
	.def = CID_AFR_MIN_FPS_MAX,
};

static struct v4l2_ctrl_config b52isp_ctrl_afr_sr_min_fps_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_AFR_SR_MIN_FPS,
	.name = "save and restore MIN FPS for AFR",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = CID_AFR_SAVE_MIN_FPS,
	.max = CID_AFR_RESTORE_MIN_FPS,
	.step = 1,
	.def = CID_AFR_SAVE_MIN_FPS,
};

static struct v4l2_ctrl_config b52isp_ctrl_af_5x5_win_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_AF_5X5_WIN,
	.name = "AF 5x5 windown",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = CID_AF_5X5_WIN_DISABLE,
	.max = CID_AF_5X5_WIN_ENABLE,
	.step = 1,
	.def = CID_AF_5X5_WIN_ENABLE,
};
static struct v4l2_ctrl_config b52isp_ctrl_aec_manual_mode_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_AEC_MANUAL_MODE,
	.name = "gain expoure work mode",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = CID_AEC_AUTO_THRESHOLD,
	.max = CID_AEC_MANUAL,
	.step = 1,
	.def = CID_AEC_AUTO_THRESHOLD,
};
static struct v4l2_ctrl_config b52isp_ctrl_target_y_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_TARGET_Y,
	.name = "target y low and y high",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xffff,
	.step = 1,
	.def = 0,
};
static struct v4l2_ctrl_config b52isp_ctrl_mean_y_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_MEAN_Y,
	.name = "mean y",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xff,
	.step = 1,
	.def = 0,
};
static struct v4l2_ctrl_config b52isp_ctrl_aec_stable_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_AEC_STABLE,
	.name = "aec stable",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min =  CID_AEC_NO_STABLE,
	.max = CID_AEC_STABLE,
	.step = 1,
	.def = CID_AEC_NO_STABLE,
};
static struct v4l2_ctrl_config b52isp_ctrl_afr_max_gain_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_AFR_MAX_GAIN,
	.name = "max gain for AFR",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = CID_AFR_MAX_GAIN_MIN,
	.max = CID_AFR_MAX_GAIN_MAX,
	.step = 1,
	.def = CID_AFR_MAX_GAIN_MAX,
};
static struct v4l2_ctrl_config b52isp_ctrl_band_step_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_BAND_STEP,
	.name = "band step",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xff,
	.step = 1,
	.def = 0,
};
static struct v4l2_ctrl_config b52isp_ctrl_set_fps_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_SET_FPS,
	.name = "set fps",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xff,
	.step = 1,
	.def = 0,
};
static struct v4l2_ctrl_config b52isp_ctrl_max_expo_cfg = {
	.ops = &b52isp_ctrl_ops,
	.id = V4L2_CID_PRIVATE_MAX_EXPO,
	.name = "max_expo",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xffff,
	.step = 1,
	.def = 0,
};
struct b52isp_ctrl_colorfx_reg {
	u32 reg;
	u32 val;
	u8 len;
	u32 mask;
};

struct b52isp_ctrl_colorfx {
	int id;
	struct b52isp_ctrl_colorfx_reg *reg;
	int reg_len;
};

#define SDE_REG_NUMS	28
static struct b52isp_ctrl_colorfx_reg __colorfx_none[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x40,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x40,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x00,  1, 0},
	{REG_SDE_HGAIN,       0x04,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_mono_chrome[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x00,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x00,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x00,  1, 0},
	{REG_SDE_HGAIN,       0x04,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_negative[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x07,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x01,  1, 0},
	{REG_SDE_UV_M00_1,    0xc0,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x01,  1, 0},
	{REG_SDE_UV_M11_1,    0xc0,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0xff,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x00,  1, 0},
	{REG_SDE_HGAIN,       0x02,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_sepia[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x00,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x00,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x01,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0xd7,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x18,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x00,  1, 0},
	{REG_SDE_HGAIN,       0x04,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_sketch[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x00,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x00,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x00,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0xff,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x08,  1, 0},
	{REG_SDE_HGAIN,       0x06,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_water_color[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x48,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,	  0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x48,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x08,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x10,  1, 0},
	{REG_SDE_HGAIN,       0x05,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_ink[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x48,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x48,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x08,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x04,  1, 0},
	{REG_SDE_HGAIN,       0x05,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_cartoon[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x40,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x40,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x01,  1, 0x7},
	{REG_SDE_CTRL17,      0x08,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0xa0,  1, 0},
	{REG_SDE_HGAIN,       0x04,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_color_ink[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x00,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x40,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x40,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0xff,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x08,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x00,  1, 0},
	{REG_SDE_HGAIN,       0x07,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_aqua[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x00,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x00,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x16,  1, 0},
	{REG_SDE_UVOFFSET1,   0x01,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0xcd,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x00,  1, 0},
	{REG_SDE_HGAIN,       0x04,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_black_board[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x04,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x00,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x01,  1, 0},
	{REG_SDE_UV_M11_1,    0x40,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x00,  1, 0},
	{REG_SDE_HGAIN,       0x06,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_white_board[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x00,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x00,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0xff,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x07,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x10,  1, 0},
	{REG_SDE_HGAIN,       0x08,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_poster[] = {
	{REG_SDE_YUVTHRE00,   0x00,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x00,  1, 0x3f},
	{REG_SDE_YGAIN,       0x00,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x00,  1, 0},
	{REG_SDE_UV_M00_1,    0x40,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x00,  1, 0},
	{REG_SDE_UV_M11_1,    0x40,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x00,  1, 0},
	{REG_SDE_HGAIN,       0x04,  1, 0},
};

static struct b52isp_ctrl_colorfx_reg __colorfx_solariztion[] = {
	{REG_SDE_YUVTHRE00,   0x80,  1, 0},
	{REG_SDE_YUVTHRE01,   0xff,  1, 0},
	{REG_SDE_YUVTHRE10,   0x80,  1, 0},
	{REG_SDE_YUVTHRE11,   0x7f,  1, 0},
	{REG_SDE_YUVTHRE20,   0x80,  1, 0},
	{REG_SDE_YUVTHRE21,   0x7f,  1, 0},
	{REG_SDE_CTRL06,      0x20,  1, 0x3f},
	{REG_SDE_YGAIN,       0x04,  1, 0},
	{REG_SDE_YGAIN_1,     0x80,  1, 0},
	{REG_SDE_UV_M00,      0x01,  1, 0},
	{REG_SDE_UV_M00_1,    0xc0,  1, 0},
	{REG_SDE_UV_M01,      0x00,  1, 0},
	{REG_SDE_UV_M01_1,    0x00,  1, 0},
	{REG_SDE_UV_M10,      0x00,  1, 0},
	{REG_SDE_UV_M10_1,    0x00,  1, 0},
	{REG_SDE_UV_M11,      0x01,  1, 0},
	{REG_SDE_UV_M11_1,    0xc0,  1, 0},
	{REG_SDE_YOFFSET,     0x00,  1, 0},
	{REG_SDE_YOFFSET_1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0,   0x00,  1, 0},
	{REG_SDE_UVOFFSET0_1, 0x00,  1, 0},
	{REG_SDE_UVOFFSET1,   0x00,  1, 0},
	{REG_SDE_UVOFFSET1_1, 0x00,  1, 0},
	{REG_SDE_CTRL17,      0x00,  1, 0x7},
	{REG_SDE_CTRL17,      0x00,  1, 0x8},
	{REG_SDE_CTRL17,      0x00,  1, 0x10},
	{REG_SDE_HTHRE,       0x00,  1, 0},
	{REG_SDE_HGAIN,       0x04,  1, 0},
};

#define B52ISP_CTRL_COLORFX(NAME, name) \
	{	.id = V4L2_PRIV_COLORFX_##NAME, \
		.reg = __colorfx_##name, \
		.reg_len = ARRAY_SIZE(__colorfx_##name), \
	}
static struct b52isp_ctrl_colorfx b52isp_ctrl_effects[] = {
	B52ISP_CTRL_COLORFX(NONE, none),
	B52ISP_CTRL_COLORFX(MONO_CHROME, mono_chrome),
	B52ISP_CTRL_COLORFX(NEGATIVE, negative),
	B52ISP_CTRL_COLORFX(SEPIA, sepia),
	B52ISP_CTRL_COLORFX(SKETCH, sketch),
	B52ISP_CTRL_COLORFX(WATER_COLOR, water_color),
	B52ISP_CTRL_COLORFX(INK, ink),
	B52ISP_CTRL_COLORFX(CARTOON, cartoon),
	B52ISP_CTRL_COLORFX(COLOR_INK, color_ink),
	B52ISP_CTRL_COLORFX(AQUA, aqua),
	B52ISP_CTRL_COLORFX(BLACK_BOARD, black_board),
	B52ISP_CTRL_COLORFX(WHITE_BOARD, white_board),
	B52ISP_CTRL_COLORFX(POSTER, poster),
	B52ISP_CTRL_COLORFX(SOLARIZATION, solariztion),
};

static struct v4l2_rect b52isp_ctrl_af_win = {
	.left = 0,
	.top  = 0,
	.width = 640,
	.height = 480,
};

static struct b52isp_expo_metering b52isp_ctrl_metering_mode[] = {
	[V4L2_EXPOSURE_METERING_AVERAGE] = {
		.mode = V4L2_EXPOSURE_METERING_AVERAGE,
		.stat_win = {
			.left = 16,
			.top  = 16,
			.right = 16,
			.bottom = 16,
		},
		.center_win = {
			.left = 160,
			.top  = 120,
			.width = 160,
			.height = 120,
		},
		.win_weight = {
			[0 ... 12] = 1,
		},
	},
	[V4L2_EXPOSURE_METERING_CENTER_WEIGHTED] = {
		.mode = V4L2_EXPOSURE_METERING_CENTER_WEIGHTED,
		.stat_win = {
			.left = 16,
			.top  = 16,
			.right = 16,
			.bottom = 16,
		},
		.center_win = {
			.left = 160,
			.top  = 120,
			.width = 160,
			.height = 120,
		},
		.win_weight = {
			[0 ... 3]  = 1,
			[5 ... 7]  = 2,
			[8]        = 3,
			[9 ... 12] = 2,
		},
	},
	[V4L2_EXPOSURE_METERING_SPOT] = {
		.mode = V4L2_EXPOSURE_METERING_SPOT,
		.stat_win = {
			.left = 16,
			.top  = 16,
			.right = 16,
			.bottom = 16,
		},
		.center_win = {
			.left = 160,
			.top  = 120,
			.width = 160,
			.height = 120,
		},
		.win_weight = {
			[0 ... 12] = 1,
		},
	},
	[V4L2_EXPOSURE_METERING_MATRIX] = {
		.mode = V4L2_EXPOSURE_METERING_MATRIX,
		.stat_win = {
			.left = 16,
			.top  = 16,
			.right = 16,
			.bottom = 16,
		},
		.center_win = {
			.left = 160,
			.top  = 120,
			.width = 160,
			.height = 120,
		},
		.win_weight = {
			[0 ... 3]  = 1,
			[5 ... 7]  = 2,
			[8]        = 3,
			[9 ... 12] = 2,
		},
	},
};

static struct b52isp_win b52isp_ctrl_metering_roi = {
	.left = 160,
	.top  = 120,
	.right = 160,
	.bottom = 120,
};

static struct b52_sensor *b52isp_ctrl_to_sensor(struct v4l2_ctrl *ctrl)
{
	struct b52isp_ctrls *ctrls = container_of(ctrl->handler,
			struct b52isp_ctrls, ctrl_handler);
	struct isp_subdev *ispsd = &(container_of(ctrls,
			struct b52isp_lpipe, ctrls)->isd);
	struct b52_sensor *sensor = b52_get_sensor(&ispsd->subdev.entity);
	if (!sensor)
		return NULL;

	return sensor;
}

void b52isp_ctrl_reset_bp_val(void)
{
	int i;

	for (i = 0; i < B52_NR_PIPELINE_MAX; i++) {
		low_def[i] = 0;
		high_def[i] = 0;
	}
}
EXPORT_SYMBOL(b52isp_ctrl_reset_bp_val);

static int b52isp_ctrl_set_contrast(struct v4l2_ctrl *ctrl, int id)
{
	u32 base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;

	b52_writeb(base + REG_SDE_YGAIN, (ctrl->val >> 8) & 0x0F);
	b52_writeb(base + REG_SDE_YGAIN_1, ctrl->val & 0xFF);
	b52_writeb(base + REG_SDE_YOFFSET, (ctrl->val >> 20) & 0xFF);
	b52_writeb(base + REG_SDE_YOFFSET_1, (ctrl->val >> 12) & 0xFF);

	return 0;
}

static int __maybe_unused b52isp_ctrl_set_saturation(struct v4l2_ctrl *ctrl, int id)
{
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;

	b52_writeb(base + REG_FW_UV_MAX_SATURATON, ctrl->val & 0xFF);
	b52_writeb(base + REG_FW_UV_MIN_SATURATON, (ctrl->val >> 8) & 0xFF);

	return 0;
}

static int __maybe_unused b52isp_ctrl_set_sharpness(struct v4l2_ctrl *ctrl, int id)
{
	u32 base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;

	b52_writeb(base + REG_CIP_MIN_SHARPEN, (ctrl->val >> 0) & 0xFF);
	b52_writeb(base + REG_CIP_MAX_SHARPEN, (ctrl->val >> 8) & 0xFF);

	return 0;
}

static int b52isp_ctrl_set_brightness(struct v4l2_ctrl *ctrl, int id)
{
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;

	b52_writeb(base + REG_FW_CMX_PREGAIN0, (ctrl->val >>  0) & 0xFF);
	b52_writeb(base + REG_FW_CMX_PREGAIN1, (ctrl->val >>  8) & 0xFF);
	b52_writeb(base + REG_FW_CMX_PREGAIN2, (ctrl->val >> 16) & 0xFF);

	return 0;
}

static int b52isp_ctrl_set_white_balance(struct b52isp_ctrls *ctrls,
		int id)
{
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	int value = ctrls->auto_wb->val;
	bool awb_lock = ctrls->aaa_lock->cur.val & V4L2_LOCK_WHITE_BALANCE;
	if (awb_lock) {
		pr_err("%s: error: the white balance is locked\n", __func__);
		return -EBUSY;
	}

	switch (value) {
	case V4L2_WHITE_BALANCE_MANUAL:
		base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;
		b52_writeb(base + REG_AWB_GAIN_MAN_EN, AWB_MAN_EN);
		break;
	case V4L2_WHITE_BALANCE_AUTO:
		b52_writeb(base + REG_FW_AWB_TYPE, AWB_AUTO);
		base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;
		b52_writeb(base + REG_AWB_GAIN_MAN_EN, AWB_MAN_DIS);
		break;
	case V4L2_WHITE_BALANCE_INCANDESCENT:
	case V4L2_WHITE_BALANCE_FLUORESCENT:
	case V4L2_WHITE_BALANCE_FLUORESCENT_H:
	case V4L2_WHITE_BALANCE_HORIZON:
	case V4L2_WHITE_BALANCE_DAYLIGHT:
	case V4L2_WHITE_BALANCE_FLASH:
	case V4L2_WHITE_BALANCE_CLOUDY:
	case V4L2_WHITE_BALANCE_SHADE:
		b52_writeb(base + REG_FW_AWB_TYPE, value + AWB_IDX_OFFSET);
		base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;
		b52_writeb(base + REG_AWB_GAIN_MAN_EN, AWB_MAN_DIS);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* Supported manual ISO values */
static const s64 iso_qmenu[] = {
	100, 200, 400, 800, 1600, 3200
};

/*
 * Supported Exposure Bias values, -3.0EV...+3.0EV
 * ev = ev_bias_qmenu[] / 2
 */

static const s64 ev_bias_qmenu[] = {
	-6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6
};

static const int ev_bias_offset[] = {
	-0x3C, -0x33, -0x2B,-0x22, -0x1A, -0x0D, 0,
	0x16, 0x2C, 0x53, 0x7A, 0x4B, 0x5A
};

static const int ev_ext_offset[] = {
	0x03, 0x05, 0x0B, 0x0F, 0x14, 0x1A, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};

static int b52isp_ctrl_set_expo_bias(int idx, int id, struct b52_sensor *sensor)
{
	int low_target;
	int high_target;
	int offset;
	char ev_ext;
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;

	/* get the blue print value */
	if (!low_def[id]) {
		low_def[id] = b52_readb(base + REG_FW_AEC_TARGET_LOW);
		high_def[id] = b52_readb(base + REG_FW_AEC_TARGET_HIGH);
	}

	if (idx >= ARRAY_SIZE(ev_bias_qmenu)) {
			pr_err("exposure bias value %d is wrong\n", idx);
			return -EINVAL;
	}
	if (sensor->drvdata->ev_bias_offset != NULL)
		offset = sensor->drvdata->ev_bias_offset[idx];
	else
	offset = ev_bias_offset[idx];

	/* (low_def[id]+ high_def[id])/2+(EV offset) */
	low_target = ((low_def[id] + high_def[id]) >> 1) + offset;
	low_target = clamp(low_target, 0, 0xFF);
	high_target = low_target;
	b52_writeb(base + REG_FW_AEC_TARGET_LOW, low_target);
	b52_writeb(base + REG_FW_AEC_TARGET_HIGH, high_target);
	ev_ext = ev_ext_offset[idx];
	b52_writeb(base + REG_FW_AEC_TARGET_EV, ev_ext);
	if (idx == 2 || idx == 3)
		b52_writeb(base + REG_FW_AEC_STABLE_RANGE1, 0x02);
	else
		b52_writeb(base + REG_FW_AEC_STABLE_RANGE1, 0x04);
	return 0;
}

static int b52isp_ctrl_set_expo(struct b52isp_ctrls *ctrls, int id)
{
	int ret = 0;
	u32 lines = 0;
	u32 expo = 0;
	u16 max_expo = 0;
	u16 min_expo = 0;
	int value = ctrls->auto_expo->val;
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	bool ae_lock  = ctrls->aaa_lock->cur.val & V4L2_LOCK_EXPOSURE;
	struct b52_sensor *sensor = b52isp_ctrl_to_sensor(ctrls->auto_expo);
	if (ae_lock) {
		pr_err("%s: error: the expo is locked\n", __func__);
		return -EINVAL;
	}
	switch (value) {
	case V4L2_EXPOSURE_AUTO:
		if (ctrls->auto_expo->is_new) {
			if (!sensor)
				return -EINVAL;
			ret = b52_sensor_call(sensor, g_param_range,
				B52_SENSOR_EXPO, &min_expo, &max_expo);
			if (ret < 0)
				return ret;

			b52_writel(base + REG_FW_MAX_CAM_EXP, max_expo);
			b52_writel(base + REG_FW_MIN_CAM_EXP, min_expo);

			b52_writeb(base + REG_FW_AEC_MANUAL_EN, AEC_AUTO);
		}

		if (ctrls->expo_bias->is_new)
			b52isp_ctrl_set_expo_bias(ctrls->expo_bias->val, id, sensor);
		break;

	case V4L2_EXPOSURE_MANUAL:
		if (ctrls->auto_expo->is_new) {
			if (ctrls->aec_manual_mode->val ==
					CID_AEC_AUTO_THRESHOLD)
				b52_writeb(base + REG_FW_AEC_MANUAL_EN, AEC_AUTO);
			else
				b52_writeb(base + REG_FW_AEC_MANUAL_EN, AEC_MANUAL);
		}
		if (ctrls->exposure->is_new) {
			if (!sensor)
				return -EINVAL;
			expo = ctrls->exposure->val;
			ret = b52_sensor_call(sensor, to_expo_line, expo, &lines);
			if (ret < 0)
				return ret;
			if (ctrls->aec_manual_mode->val ==
						CID_AEC_AUTO_THRESHOLD) {
				b52_writel(base + REG_FW_MAX_CAM_EXP, lines);
				b52_writel(base + REG_FW_MIN_CAM_EXP, lines);
			} else
				b52_writel(base + REG_FW_AEC_MAN_EXP, lines);
		} else if (ctrls->expo_line->is_new) {
			expo = (ctrls->expo_line->val >> 4);
			if (ctrls->aec_manual_mode->val ==
						CID_AEC_AUTO_THRESHOLD) {
				b52_writel(base + REG_FW_MAX_CAM_EXP, expo);
				b52_writel(base + REG_FW_MIN_CAM_EXP, expo);
			} else
				b52_writel(base + REG_FW_AEC_MAN_EXP, expo);
		}
		break;

	default:
		return -EINVAL;
	}

	b52_writeb(base + REG_FW_NIGHT_MODE, NIGHT_MODE_DISABLE);

	return ret;
}

static int b52isp_ctrl_get_expo(struct b52isp_ctrls *ctrls, int id)
{
	u32 time;
	u32 lines;
	int ret = 0;
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	struct b52_sensor *sensor = b52isp_ctrl_to_sensor(ctrls->auto_expo);
	lines = b52_readl(base + REG_FW_AEC_MAN_EXP);
	ctrls->expo_line->val = lines;
	if (!sensor)
		ctrls->exposure->val = lines;
	else {
		ret = b52_sensor_call(sensor, to_expo_time, &time, lines >> 4);
		if (ret < 0)
			return ret;

		ctrls->exposure->val = time;
	}
	return ret;
}
static int b52isp_ctrl_set_fps(struct b52isp_ctrls *ctrls, int id)
{
	u32 vts = 0;
	int value = ctrls->set_fps->val;
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	struct b52_sensor *sensor = b52isp_ctrl_to_sensor(ctrls->auto_expo);
	if (value > 30 || value <= 0)
		return -EINVAL;
	vts = sensor->drvdata->vts_range.min * 30 / value;
	b52_writew(base + REG_FW_VTS, vts);
	b52_writel(base + REG_FW_MAX_CAM_EXP, vts - 16);
	return 0;
}
static int b52isp_ctrl_set_max_expo(struct b52isp_ctrls *ctrls, int id)
{
	u32 base;
	int ret = 0;
	u32 lines = 0;
	u16 max_expo = ctrls->max_expo->val;
	struct b52_sensor *sensor = b52isp_ctrl_to_sensor(ctrls->max_expo);
	if (!sensor)
		return -EINVAL;
	base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	ret = b52_sensor_call(sensor, to_expo_line, max_expo, &lines);
	if (ret < 0)
		return ret;
	b52_writel(base + REG_FW_MAX_CAM_EXP, lines);
	return 0;
}
static int b52isp_ctrl_set_3a_lock(struct v4l2_ctrl *ctrl, int id)
{
	u32 base;
	bool awb_lock = ctrl->val & V4L2_LOCK_WHITE_BALANCE;
	bool ae_lock  = ctrl->val & V4L2_LOCK_EXPOSURE;
	bool af_lock  = ctrl->val & V4L2_LOCK_FOCUS;
	int changed = ctrl->val ^ ctrl->cur.val;
	base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	if (changed & V4L2_LOCK_EXPOSURE)
		b52_writeb(base + REG_FW_AEC_MANUAL_EN,
			ae_lock ? AEC_MANUAL : AEC_AUTO);

	base = FW_P1_REG_AF_BASE + id * FW_P1_P2_AF_OFFSET;
	if (changed & V4L2_LOCK_FOCUS)
		b52_writeb(base + REG_FW_AF_ACIVE,
			af_lock ? AF_STOP : AF_START);

	base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;
	if (changed & V4L2_LOCK_WHITE_BALANCE)
		b52_writeb(base + REG_AWB_GAIN_MAN_EN,
			awb_lock ? AWB_MAN_EN : AWB_MAN_DIS);

	return 0;
}

static int b52_set_metering_win(
		struct b52isp_expo_metering *metering, int id)
{
	int i;

	u32 base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;

	b52_writew(base + REG_AEC_STATWIN_LEFT, metering->stat_win.left);
	b52_writew(base + REG_AEC_STATWIN_TOP, metering->stat_win.top);
	b52_writew(base + REG_AEC_STATWIN_RIGHT, metering->stat_win.right);
	b52_writew(base + REG_AEC_STATWIN_BOTTOM, metering->stat_win.bottom);

	b52_writew(base + REG_AEC_WINLEFT, metering->center_win.left);
	b52_writew(base + REG_AEC_WINTOP, metering->center_win.top);
	b52_writew(base + REG_AEC_WINWIDTH, metering->center_win.width);
	b52_writew(base + REG_AEC_WINHEIGHT, metering->center_win.height);
	for (i = 0; i < NR_METERING_WIN_WEIGHT; i++)
		b52_writeb(base + REG_AEC_WIN_WEIGHT0 + i,
				metering->win_weight[i]);

	return 0;
}

static int b52_set_metering_roi(struct b52isp_win *r, int id)
{
	u32 base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;

	b52_writew(base + REG_AEC_ROI_LEFT, r->left);
	b52_writew(base + REG_AEC_ROI_TOP, r->top);
	b52_writew(base + REG_AEC_ROI_RIGHT, r->right);
	b52_writew(base + REG_AEC_ROI_BOTTOM, r->bottom);

	return 0;
}

static int b52isp_ctrl_set_metering_mode(struct b52isp_ctrls *ctrls,
		int id)
{
	int ret;

	switch (ctrls->exp_metering->val) {
	case V4L2_EXPOSURE_METERING_AVERAGE:
		ret = b52_set_metering_win(&ctrls->metering_mode[0], id);
		break;
	case V4L2_EXPOSURE_METERING_CENTER_WEIGHTED:
		ret = b52_set_metering_win(&ctrls->metering_mode[1], id);
		break;
	case V4L2_EXPOSURE_METERING_SPOT:
		ret = b52_set_metering_win(&ctrls->metering_mode[2], id);
		break;
	case V4L2_EXPOSURE_METERING_MATRIX:
		ret = b52_set_metering_win(&ctrls->metering_mode[3], id);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	ret |= b52_set_metering_roi(&ctrls->metering_roi, id);

	return ret;
}

static int b52isp_ctrl_set_iso(struct b52isp_ctrls *ctrls, int id)
{
	u32 iso;
	u32 gain;
	int ret;
	u16 min_gain = 0;
	u16 max_gain = 0;
	int idx = ctrls->iso->val;
	int auto_iso = ctrls->auto_iso->val;
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	struct b52_sensor *sensor = b52isp_ctrl_to_sensor(ctrls->auto_iso);
	if (!sensor)
		return -EINVAL;

	/* FIXME:expo and gain auto register are same, how to handle*/
	switch (auto_iso) {
	case V4L2_ISO_SENSITIVITY_AUTO:
		ret = b52_sensor_call(sensor, g_param_range,
			B52_SENSOR_GAIN, &min_gain, &max_gain);
		if (ret < 0)
			return ret;

		b52_writew(base + REG_FW_MIN_CAM_GAIN, min_gain);
		b52_writew(base + REG_FW_MAX_CAM_GAIN, max_gain);

		b52_writeb(base + REG_FW_AEC_MANUAL_EN, AEC_AUTO);
		break;

	case V4L2_ISO_SENSITIVITY_MANUAL:
		if (ctrls->auto_iso->is_new)
			b52_writeb(base + REG_FW_AEC_MANUAL_EN, AEC_AUTO);

		if (!ctrls->iso->is_new)
			break;

		if (idx >= ARRAY_SIZE(iso_qmenu))
			return -EINVAL;
		iso = iso_qmenu[idx];

		ret = b52_sensor_call(sensor, iso_to_gain, iso, &gain);
		if (ret < 0)
			return ret;

		b52_writew(base + REG_FW_MIN_CAM_GAIN, gain);
		b52_writew(base + REG_FW_MAX_CAM_GAIN, gain);

		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int b52isp_ctrl_get_iso(struct b52isp_ctrls *ctrls, int id)
{
	int i;
	int ret = 0;
	u32 gain = 0;
	u32 iso = 0;
	u32 reg = id ? REG_FW_AEC_RD_GAIN2 : REG_FW_AEC_RD_GAIN1;
	struct b52_sensor *sensor = b52isp_ctrl_to_sensor(ctrls->auto_iso);
	if (!sensor)
		return -EINVAL;

	gain = b52_readw(reg);
	ret = b52_sensor_call(sensor, gain_to_iso, gain, &iso);
	if (ret < 0)
			return ret;

	for (i = 0; i < ARRAY_SIZE(iso_qmenu); i++)
		if (iso <= iso_qmenu[i])
			break;

	ctrls->iso->val = i;
	return ret;
}

static int b52isp_ctrl_set_gain(struct b52isp_ctrls *ctrls, int id)
{
	int ret = 0;
	u16 min_gain = 0;
	u16 max_gain = 0;
	int gain = ctrls->gain->val;
	int auto_gain = ctrls->auto_gain->val;
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	struct b52_sensor *sensor = b52isp_ctrl_to_sensor(ctrls->auto_iso);
	/*FIXME: convert the unit*/
	gain >>= GAIN_CONVERT;

	/* FIXME:expo and gain auto register are same, how to handle*/
	switch (auto_gain) {
	case 1:
		ret = b52_sensor_call(sensor, g_param_range,
			B52_SENSOR_GAIN, &min_gain, &max_gain);
		if (ret < 0)
			return ret;

		b52_writew(base + REG_FW_MAX_CAM_GAIN, max_gain);
		b52_writew(base + REG_FW_MIN_CAM_GAIN, min_gain);

		b52_writeb(base + REG_FW_AEC_MANUAL_EN, AEC_AUTO);
		break;
	case 0:
		if (ctrls->auto_gain->is_new) {
			if (ctrls->aec_manual_mode->val ==
					CID_AEC_AUTO_THRESHOLD)
				b52_writeb(base + REG_FW_AEC_MANUAL_EN,
								AEC_AUTO);
			else
				b52_writeb(base + REG_FW_AEC_MANUAL_EN,
								AEC_MANUAL);
		}
		if (ctrls->gain->is_new) {
			if (ctrls->aec_manual_mode->val ==
						CID_AEC_AUTO_THRESHOLD) {
				b52_writew(base + REG_FW_MAX_CAM_GAIN, gain);
				b52_writew(base + REG_FW_MIN_CAM_GAIN, gain);
			} else
				b52_writew(base + REG_FW_AEC_MAN_GAIN, gain);
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int b52isp_ctrl_get_gain(struct b52isp_ctrls *ctrls, int id)
{
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	ctrls->gain->val = b52_readw(base + REG_FW_AEC_MAN_GAIN);
	ctrls->gain->val <<= GAIN_CONVERT;
	return 0;
}

static int b52isp_ctrl_get_band_step(struct b52isp_ctrls *ctrls, int id)
{
	/* unit is line Q4*/
	switch (ctrls->pwr_line_freq->val) {
	case V4L2_CID_POWER_LINE_FREQUENCY_50HZ:
		ctrls->band_step->val = b52_readw(REG_BAND_DET_50HZ) << 4;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_60HZ:
		ctrls->band_step->val = b52_readw(REG_BAND_DET_60HZ) << 4;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int b52isp_ctrl_set_band(struct v4l2_ctrl *ctrl, int id)
{
	int ret;
	u16 band_50hz = 0;
	u16 band_60hz = 0;
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	struct b52_sensor *sensor = b52isp_ctrl_to_sensor(ctrl);
	if (!sensor)
		return -EINVAL;

	switch (ctrl->val) {
	case V4L2_CID_POWER_LINE_FREQUENCY_DISABLED:
		b52_writeb(base + REG_FW_AUTO_BAND_DETECTION,
				AUTO_BAND_DETECTION_OFF);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_50HZ:
		ret = b52_sensor_call(sensor, g_band_step,
				&band_50hz, &band_60hz);
		if (ret < 0)
			return ret;
		b52_writew(REG_BAND_DET_50HZ, band_50hz);

		b52_writeb(base + REG_FW_AUTO_BAND_DETECTION,
				AUTO_BAND_DETECTION_OFF);
		b52_writeb(base + REG_FW_BAND_FILTER_MODE, BAND_FILTER_50HZ);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_60HZ:
		ret = b52_sensor_call(sensor, g_band_step,
				&band_50hz, &band_60hz);
		if (ret < 0)
			return ret;
		b52_writew(REG_BAND_DET_60HZ, band_60hz);

		b52_writeb(base + REG_FW_AUTO_BAND_DETECTION,
				AUTO_BAND_DETECTION_OFF);
		b52_writeb(base + REG_FW_BAND_FILTER_MODE, BAND_FILTER_60HZ);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_AUTO:
		b52_writeb(base + REG_FW_AUTO_BAND_DETECTION,
				AUTO_BAND_DETECTION_ON);
		break;
	default:
		return -EINVAL;
	}

	b52_writeb(base + REG_FW_BAND_FILTER_LESS1BAND,
		   LESS_THAN_1_BAND_ENABLE);

	return 0;
}

static int b52isp_ctrl_set_band_filter(struct v4l2_ctrl *ctrl, int id)
{
	int ret;
	u16 band_50hz = 0;
	u16 band_60hz = 0;
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	struct b52_sensor *sensor;

	switch (ctrl->val) {
	case 1:
		sensor = b52isp_ctrl_to_sensor(ctrl);
		if (!sensor)
			return -EINVAL;

		ret = b52_sensor_call(sensor, g_band_step,
				&band_50hz, &band_60hz);
		if (ret < 0)
			return ret;

		b52_writew(base + REG_FW_BAND_50HZ,
				band_50hz << BAND_VALUE_SHIFT);
		b52_writew(base + REG_FW_BAND_60HZ,
				band_60hz << BAND_VALUE_SHIFT);

		b52_writeb(base + REG_FW_BAND_FILTER_EN,
				BAND_FILTER_ENABLE);
		break;
	case 0:
		b52_writeb(base + REG_FW_BAND_FILTER_EN,
				BAND_FILTER_DISABLE);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int b52isp_ctrl_set_image_effect(int value, int id)
{
	int i;
	int j;
	int n;
	int ret;
	u32 base = REG_SDE_BUF_BASE;
	u32 base1 = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;
	struct b52isp_ctrl_colorfx_reg *reg;

	for (i = 0; i < ARRAY_SIZE(b52isp_ctrl_effects); i++)
		if (b52isp_ctrl_effects[i].id == value)
			goto found;
	return -EINVAL;

found:
	reg = b52isp_ctrl_effects[i].reg;

	for (j = 0, n = 0; j < b52isp_ctrl_effects[i].reg_len; j++) {
		if (reg[j].len == 1) {
			if (!reg[j].mask) {
				b52_writeb(base + n, ((base1 + reg[j].reg) >> 8) & 0xff);
				b52_writeb(base + n + 1, (base1 + reg[j].reg) & 0xff);
				b52_writeb(base + n + 2, reg[j].val);
				b52_writeb(base + n + 3, 0xff);
			} else {
				b52_writeb(base + n, ((base1 + reg[j].reg) >> 8) & 0xff);
				b52_writeb(base + n + 1, (base1 + reg[j].reg) & 0xff);
				b52_writeb(base + n + 2, reg[j].val);
				b52_writeb(base + n + 3, reg[j].mask);
			}
			n += 4;
		}
	}

	ret = b52_cmd_effect(SDE_REG_NUMS);
	if (ret < 0)
		return ret;
	return 0;
}

static int b52isp_af_run(struct v4l2_rect *r,
	int af_mode, int enable, int id)
{
	u32 base = FW_P1_REG_AF_BASE + id * FW_P1_P2_AF_OFFSET;

	if (!enable) {
		b52_writeb(base + REG_FW_AF_ACIVE, AF_STOP);
		return 0;
	}

	b52_writeb(base + REG_FW_AF_ACIVE, AF_STOP);

	/* set AF window */
	base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;
	b52_clearb_bit(base + REG_AFC_CTRL0, AFC_5X5_WIN);
	b52_set_focus_win(r, id);

	/* set af mode */
	base = FW_P1_REG_AF_BASE + id * FW_P1_P2_AF_OFFSET;
	if (af_mode == CID_AF_SNAPSHOT)
		b52_writeb(base + REG_FW_AF_MODE, AF_SNAPSHOT);
	else if (af_mode == CID_AF_CONTINUOUS)
		b52_writeb(base + REG_FW_AF_MODE, AF_CONTINUOUS);

	if (af_mode == CID_AF_CONTINUOUS)
		b52_writeb(base + REG_FW_AF_ACIVE, AF_START);

	return 0;
}

#define TRY_TIMES_MAX 100
/* only used in manual mode*/
int b52isp_set_focus_distance(u32 distance, int id)
{
	u32 base = FW_P1_REG_AF_BASE + id * FW_P1_P2_AF_OFFSET;
	int try_times = 0;

	if (b52_readb(base + REG_FW_AF_ACIVE) == AF_START) {
		pr_err("%s: warning still in focus\n", __func__);
		b52_writeb(base + REG_FW_AF_ACIVE, AF_STOP);
	}

	b52_writew(base + REG_FW_FOCUS_POS, distance);
/*	b52_writeb(base + REG_FW_FOCUS_MAN_TRIGGER, FOCUS_MAN_TRIGGER);*/
	do {
		b52_cmd_vcm();
		msleep(20);
		try_times++;
	} while ((!b52_readb(base + REG_FW_FOCUS_MAN_STATUS)) && (try_times < TRY_TIMES_MAX));

	pr_info("%s: manual focus success, postion:0x%x\n",
			__func__, b52_readw(base + REG_FW_AF_CURR_POS));

	return 0;
}
EXPORT_SYMBOL_GPL(b52isp_set_focus_distance);

static int b52isp_force_start(int start, int af_mode, int id)
{
	u32 base = FW_P1_REG_AF_BASE + id * FW_P1_P2_AF_OFFSET;
	u32 reg;
	u8 val;

	if (af_mode == CID_AF_SNAPSHOT) {
		reg = REG_FW_AF_ACIVE;
		val = start ? AF_START : AF_STOP;
		if (val == AF_START && b52_readb(base + reg) == AF_START) {
			pr_err("In focusing, not need to start\n");
			return 0;
		}
	} else {
		reg = REG_FW_AF_FORCE_START;
		val = start ? AF_FORCE_START :  AF_FORCE_STOP;
	}

	b52_writeb(base + reg, val);

	return 0;
}

static int b52isp_af_5x5_win_ctrl(int enable, int id)
{
	u32 base = FW_P1_REG_AF_BASE + id * FW_P1_P2_AF_OFFSET;
	u8 val = enable ? AF_5X5_WIN_ENABLE : AF_5X5_WIN_DISABLE;

	if (id == 1) {
		pr_err("pipeline2 does not have 5x5 win ctrl register\n");
		return -EINVAL;
	}

	b52_writeb(base + REG_FW_AF_5X5_WIN_MODE, val);

	return 0;
}

static int b52isp_set_focus_range(int id, u32 reg)
{
	u16 distance;
	u32 base = FW_P1_REG_AF_BASE + id * FW_P1_P2_AF_OFFSET;

	b52_writeb(base + REG_FW_AF_ACIVE, AF_STOP);
	distance = b52_readw(base + reg);

	return b52isp_set_focus_distance(distance, id);
}

static int b52isp_ctrl_set_focus(struct b52isp_ctrls *ctrls, int id)
{
	bool af_lock  = ctrls->aaa_lock->cur.val & V4L2_LOCK_FOCUS;

	if (af_lock) {
		pr_err("%s: error: the focus is locked\n", __func__);
		return -EINVAL;
	}

	if (ctrls->af_range->is_new) {
		switch (ctrls->af_range->val) {
		case V4L2_AUTO_FOCUS_RANGE_AUTO:
			break;
		case V4L2_AUTO_FOCUS_RANGE_NORMAL:
			break;
		case V4L2_AUTO_FOCUS_RANGE_MACRO:
			b52isp_set_focus_range(id, REG_FW_AF_MACRO_POS);
			break;
		case V4L2_AUTO_FOCUS_RANGE_INFINITY:
			b52isp_set_focus_range(id, REG_FW_AF_INFINITY_POS);
			break;
		default:
			return -EINVAL;
		}
	}

	if (ctrls->auto_focus->is_new && ctrls->auto_focus->val)
		b52isp_af_run(&ctrls->af_win, ctrls->af_mode->val, 1, id);
	else if (ctrls->auto_focus->is_new && !ctrls->auto_focus->val)
		b52isp_af_run(&ctrls->af_win, ctrls->af_mode->val, 0, id);

	if (ctrls->auto_focus->val && ctrls->af_start->is_new)
		b52isp_force_start(1, ctrls->af_mode->val, id);
	else if (ctrls->auto_focus->val && ctrls->af_stop->is_new)
		b52isp_force_start(0, ctrls->af_mode->val, id);

	if (!ctrls->auto_focus->val)
		if (ctrls->f_distance->is_new)
			b52isp_set_focus_distance(ctrls->f_distance->val, id);

	if (ctrls->af_5x5_win->is_new)
		b52isp_af_5x5_win_ctrl(ctrls->af_5x5_win->val, id);

	return 0;
}

static int b52isp_ctrl_get_focus(struct b52isp_ctrls *ctrls, int id)
{
	u32 base = FW_P1_REG_AF_BASE + id * FW_P1_P2_AF_OFFSET;
	u8 status = b52_readb(base + REG_FW_AF_STATUS);

	switch (status) {
	case AF_ST_FOCUSING:
		ctrls->af_status->val = V4L2_AUTO_FOCUS_STATUS_BUSY;
		break;
	case AF_ST_SUCCESS:
		ctrls->af_status->val = V4L2_AUTO_FOCUS_STATUS_REACHED;
		break;
	case AF_ST_FAILED:
		ctrls->af_status->val = V4L2_AUTO_FOCUS_STATUS_FAILED;
		break;
	case AF_ST_IDLE:
		ctrls->af_status->val = V4L2_AUTO_FOCUS_STATUS_IDLE;
		break;
	default:
		pr_info("Unknown AF status %#x\n", status);
		break;
	}

	ctrls->f_distance->val = b52_readw(base + REG_FW_AF_CURR_POS);

	return 0;
}
static int b52isp_ctrl_get_target_y(struct b52isp_ctrls *ctrls, int id)
{
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;

	ctrls->target_y->val = b52_readw(base + REG_FW_AEC_TARGET_LOW);
	return 0;
}

static int b52isp_ctrl_get_mean_y(struct b52isp_ctrls *ctrls, int id)
{
	ctrls->mean_y->val = b52_readb(REG_FW_MEAN_Y);
	return 0;
}

static int b52isp_ctrl_get_aec_stable(struct b52isp_ctrls *ctrls, int id)
{
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;
	#ifdef CONFIG_MACH_XCOVER3LTE
		u8 status = b52_readb(REG_FW_AEC_RD_STATE1);
	#else
	u8 status = b52_readb(REG_FW_AEC_RD_STATE3);
	#endif
	if (status == AEC_STATE_STABLE)
		ctrls->aec_stable->val = AEC_STATE_STABLE;
	#ifdef CONFIG_MACH_XCOVER3LTE
		else if (b52_readw(base + REG_FW_AEC_MAN_GAIN) == b52_readw(base + REG_FW_MAX_CAM_GAIN))
		ctrls->aec_stable->val = AEC_STATE_STABLE;
		else if ((b52_readl(base + REG_FW_AEC_MAN_EXP) >> 4) == b52_readl(base + REG_FW_MIN_CAM_EXP))
		ctrls->aec_stable->val = AEC_STATE_STABLE;
	#endif
	else
		ctrls->aec_stable->val = AEC_STATE_UNSTABLE;
	return 0;
}

static int b52isp_ctrl_set_zoom(int zoom, int id)
{
	int path;

	path = id ? B52ISP_ISD_PIPE2 : B52ISP_ISD_PIPE1;
	return b52_cmd_zoom_in(path, zoom);
}

static int b52isp_ctrl_afr_sr_min_fps(int sr, int id)
{
	static u8 min_fps[B52_NR_PIPELINE_MAX][3];
	static u16 max_gain[B52_NR_PIPELINE_MAX];
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;

	if (sr == CID_AFR_SAVE_MIN_FPS) {
		min_fps[id][0] = b52_readb(base + REG_FW_AFR_MIN_FPS1);
		min_fps[id][1] = b52_readb(base + REG_FW_AFR_MIN_FPS2);
		min_fps[id][2] = b52_readb(base + REG_FW_AFR_MIN_FPS3);
		max_gain[id] = b52_readw(base + REG_FW_MAX_CAM_GAIN);
	} else if (sr == CID_AFR_RESTORE_MIN_FPS) {
		if (!min_fps[id][0]) {
			pr_err("%s: min fps val is 0\n", __func__);
			return -EINVAL;
		}

		b52_writeb(base + REG_FW_AFR_MIN_FPS1, min_fps[id][0]);
		b52_writeb(base + REG_FW_AFR_MIN_FPS2, min_fps[id][1]);
		b52_writeb(base + REG_FW_AFR_MIN_FPS3, min_fps[id][2]);
		b52_writew(base + REG_FW_MAX_CAM_GAIN, max_gain[id]);
	}

	return 0;
}

static int b52isp_ctrl_set_afr(struct b52isp_ctrls *ctrls, int id)
{
	int ret = 0;
	u8 min_fps;
	u16 max_gain;
	u16 max_sensor_gain, min_sensor_gain;
	struct b52_sensor *sensor;
	u32 base = FW_P1_REG_BASE + id * FW_P1_P2_OFFSET;

	switch (ctrls->auto_frame_rate->val) {
	case CID_AUTO_FRAME_RATE_ENABLE:
		if (ctrls->auto_frame_rate->is_new)
			b52_writeb(base + REG_FW_AUTO_FRAME_RATE, AFR_ENABLE);

		if (ctrls->afr_min_fps->is_new) {
			min_fps = AFR_DEF_VAL_30FPS / ctrls->afr_min_fps->val;
			min_fps = min_fps + 1;
			b52_writeb(base + REG_FW_AFR_MIN_FPS1, min_fps);
			b52_writeb(base + REG_FW_AFR_MIN_FPS2, min_fps);
			b52_writeb(base + REG_FW_AFR_MIN_FPS3, min_fps);
		}
		if (ctrls->afr_max_gain->is_new) {
			sensor = b52isp_ctrl_to_sensor(ctrls->afr_max_gain);
			if (!sensor)
				return -EINVAL;
			ret = b52_sensor_call(sensor, g_param_range,
				B52_SENSOR_GAIN, &min_sensor_gain, &max_sensor_gain);
			if (ret < 0)
				return ret;

			max_gain = (ctrls->afr_max_gain->val) *
							0x20 / max_sensor_gain;
			b52_writew(base + REG_FW_AFR_MAX_GAIN1,
						max_gain);
			b52_writew(base + REG_FW_AFR_MAX_GAIN2,
						max_gain);
			b52_writew(base + REG_FW_AFR_MAX_GAIN3,
						max_gain);
		}
		break;

	case CID_AUTO_FRAME_RATE_DISABLE:
		if (ctrls->auto_frame_rate->is_new)
			b52_writeb(base + REG_FW_AUTO_FRAME_RATE, AFR_DISABLE);

		if (ctrls->afr_min_fps->is_new) {
			ret = -EPERM;
			pr_err("%s: unable to set min fps for afr\n", __func__);
		}
		break;

	default:
		return -EINVAL;
	};

	if (ctrls->afr_sr_min_fps->is_new)
		ret = b52isp_ctrl_afr_sr_min_fps(ctrls->afr_sr_min_fps->val, id);

	return ret;
}


int b52isp_rw_awb_gain(struct b52isp_awb_gain *awb_gain, int id)
{
	u32 base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;

	if (awb_gain->write) {
		if (b52_readb(base + REG_AWB_GAIN_MAN_EN) != AWB_MAN_EN) {
			pr_err("awb gain not in manual mode\n");
			return -EINVAL;
		}

		b52_writew(base + REG_AWB_GAIN_B, awb_gain->b & 0x3FF);
		b52_writew(base + REG_AWB_GAIN_GB, awb_gain->gb & 0x3FF);
		b52_writew(base + REG_AWB_GAIN_GB, awb_gain->gr & 0x3FF);
		b52_writew(base + REG_AWB_GAIN_R, awb_gain->r & 0x3FF);
		b52_writew(DATA_BGAIN, awb_gain->b & 0x3FF);
		b52_writew(DATA_GGAIN, ((awb_gain->gb +
					awb_gain->gr) >> 1) & 0x3FF);
		b52_writew(DATA_RGAIN, awb_gain->r & 0x3FF);
	} else {
		awb_gain->b = b52_readw(base + REG_AWB_GAIN_B);
		awb_gain->gb = b52_readw(base + REG_AWB_GAIN_GB);
		awb_gain->gr = b52_readw(base + REG_AWB_GAIN_GR);
		awb_gain->r = b52_readw(base + REG_AWB_GAIN_R);
	}

	return 0;
}

static int b52isp_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int id;
	int ret = 0;
	struct b52isp_ctrls *ctrls = container_of(ctrl->handler,
			struct b52isp_ctrls, ctrl_handler);
	struct isp_subdev *ispsd = &container_of(ctrls,
			struct b52isp_lpipe, ctrls)->isd;
	if (ispsd->sd_code == SDCODE_B52ISP_PIPE1 ||
		ispsd->sd_code == SDCODE_B52ISP_MS1)
		id = 0;
	else if (ispsd->sd_code == SDCODE_B52ISP_PIPE2 ||
		ispsd->sd_code == SDCODE_B52ISP_MS2)
		id = 1;
	else
		return -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_FOCUS_AUTO:
		ret = b52isp_ctrl_get_focus(ctrls, id);
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		ret = b52isp_ctrl_get_expo(ctrls, id);
		break;
	case V4L2_CID_ISO_SENSITIVITY:
		ret = b52isp_ctrl_get_iso(ctrls, id);
		break;
	case V4L2_CID_ISO_SENSITIVITY_AUTO:
		break;
	case V4L2_CID_AUTOGAIN:
		ret = b52isp_ctrl_get_gain(ctrls, id);
		break;
	case V4L2_CID_PRIVATE_BAND_STEP:
		ret = b52isp_ctrl_get_band_step(ctrls, id);
		break;
	case V4L2_CID_PRIVATE_TARGET_Y:
		ret = b52isp_ctrl_get_target_y(ctrls, id);
		break;

	case V4L2_CID_PRIVATE_MEAN_Y:
		ret = b52isp_ctrl_get_mean_y(ctrls, id);
		break;

	case V4L2_CID_PRIVATE_AEC_STABLE:
		ret = b52isp_ctrl_get_aec_stable(ctrls, id);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int b52isp_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	int id;

	struct b52isp_ctrls *ctrls = container_of(ctrl->handler,
			struct b52isp_ctrls, ctrl_handler);
	struct isp_subdev *ispsd = &container_of(ctrls,
			struct b52isp_lpipe, ctrls)->isd;
	if (ispsd->sd_code == SDCODE_B52ISP_PIPE1 ||
		ispsd->sd_code == SDCODE_B52ISP_MS1)
		id = 0;
	else if (ispsd->sd_code == SDCODE_B52ISP_PIPE2 ||
		ispsd->sd_code == SDCODE_B52ISP_MS2)
		id = 1;
	else
		return -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_CONTRAST:
		ret = b52isp_ctrl_set_contrast(ctrl, id);
		break;

	case V4L2_CID_SATURATION:
		ret = b52isp_ctrl_set_saturation(ctrl, id);
		break;

	case V4L2_CID_SHARPNESS:
		ret = b52isp_ctrl_set_sharpness(ctrl, id);
		break;

	case V4L2_CID_HUE:
		break;

	case V4L2_CID_BRIGHTNESS:
		ret = b52isp_ctrl_set_brightness(ctrl, id);
		break;

	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
		ret = b52isp_ctrl_set_white_balance(ctrls, id);
		break;

	case V4L2_CID_EXPOSURE_AUTO:
		ret = b52isp_ctrl_set_expo(ctrls, id);
		break;

	case V4L2_CID_EXPOSURE_METERING:
		ret = b52isp_ctrl_set_metering_mode(ctrls, id);
		break;

	case V4L2_CID_ISO_SENSITIVITY:
		ret = b52isp_ctrl_set_iso(ctrls, id);
		break;

	case V4L2_CID_ISO_SENSITIVITY_AUTO:
		break;

	case V4L2_CID_AUTOGAIN:
		ret = b52isp_ctrl_set_gain(ctrls, id);
		break;

	case V4L2_CID_FOCUS_AUTO:
		ret = b52isp_ctrl_set_focus(ctrls, id);
		break;

	case V4L2_CID_3A_LOCK:
		ret = b52isp_ctrl_set_3a_lock(ctrl, id);
		break;

	case V4L2_CID_POWER_LINE_FREQUENCY:
		ret = b52isp_ctrl_set_band(ctrl, id);
		break;

	case V4L2_CID_BAND_STOP_FILTER:
		ret = b52isp_ctrl_set_band_filter(ctrl, id);
		break;

	case V4L2_CID_ZOOM_ABSOLUTE:
		ret = b52isp_ctrl_set_zoom(ctrl->val, id);
		break;

	case V4L2_CID_PRIVATE_COLORFX:
		ret = b52isp_ctrl_set_image_effect(ctrl->val, id);
		break;

	case V4L2_CID_PRIVATE_AUTO_FRAME_RATE:
		ret = b52isp_ctrl_set_afr(ctrls, id);
		break;

	case V4L2_CID_PRIVATE_AEC_MANUAL_MODE:
		break;

	case V4L2_CID_PRIVATE_SET_FPS:
		ret = b52isp_ctrl_set_fps(ctrls, id);
		break;
	case V4L2_CID_PRIVATE_MAX_EXPO:
		ret = b52isp_ctrl_set_max_expo(ctrls, id);
		break;
	default:
		pr_err("no to s_ctrl: %s (%d)\n", ctrl->name, ctrl->val);
		ret = -EINVAL;
		break;
	}

	if (ret < 0)
		pr_err("Failed to s_ctrl: %s (%d)\n", ctrl->name, ctrl->val);

	return ret;
}

const struct v4l2_ctrl_ops b52isp_ctrl_ops = {
	.s_ctrl	= b52isp_s_ctrl,
	.g_volatile_ctrl = b52isp_g_volatile_ctrl,
};

int b52isp_init_ctrls(struct b52isp_ctrls *ctrls)
{
	int i;
	struct v4l2_ctrl_handler *handler = &ctrls->ctrl_handler;
	const struct v4l2_ctrl_ops *ops = &b52isp_ctrl_ops;

	v4l2_ctrl_handler_init(handler, 38);

	ctrls->saturation = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_SATURATION, 0, 0xFFFF, 1, 0);
	ctrls->brightness = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_BRIGHTNESS, 0, 0xFFFFFF, 1, 0);
	ctrls->contrast = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_CONTRAST, 0, 0xFFFFFF, 1, 0);
	ctrls->sharpness = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_SHARPNESS, 0, 0xFFFF, 1, 0);
	ctrls->hue = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_HUE, -2, 2, 1, 0);

	/* white balance */
	ctrls->auto_wb = v4l2_ctrl_new_std_menu(handler, ops,
			V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE,
			9, 0, V4L2_WHITE_BALANCE_AUTO);
	/* Auto exposure */
	ctrls->auto_expo = v4l2_ctrl_new_std_menu(handler, ops,
			V4L2_CID_EXPOSURE_AUTO, 1, 0, V4L2_EXPOSURE_AUTO);
	ctrls->expo_line = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_EXPOSURE, 0, 0xFFFFFF, 1, 0);
	ctrls->expo_line->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->exposure = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_EXPOSURE_ABSOLUTE, 0, 0xFFFFFF, 1, 0);
	ctrls->exposure->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->expo_bias = v4l2_ctrl_new_int_menu(handler, ops,
			V4L2_CID_AUTO_EXPOSURE_BIAS,
			ARRAY_SIZE(ev_bias_qmenu) - 1,
			ARRAY_SIZE(ev_bias_qmenu)/2,
			ev_bias_qmenu);

	ctrls->exp_metering = v4l2_ctrl_new_std_menu(handler, ops,
			V4L2_CID_EXPOSURE_METERING, 3, ~0xf,
			V4L2_EXPOSURE_METERING_AVERAGE);

	ctrls->pwr_line_freq = v4l2_ctrl_new_std_menu(handler, ops,
			V4L2_CID_POWER_LINE_FREQUENCY, 3, ~0xf,
			V4L2_CID_POWER_LINE_FREQUENCY_AUTO);
	ctrls->band_filter = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_BAND_STOP_FILTER, 0, 1, 1, 0);

	/* Auto focus */
	ctrls->auto_focus = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_FOCUS_AUTO, 0, 1, 1, 1);
	/*FIXME: how to get the distance min/max value*/
	ctrls->f_distance = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_FOCUS_ABSOLUTE, 0, 0xFFFF, 1, 0);
	ctrls->f_distance->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->af_start = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_AUTO_FOCUS_START, 0, 1, 1, 0);
	ctrls->af_stop = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_AUTO_FOCUS_STOP, 0, 1, 1, 0);
	ctrls->af_status = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_AUTO_FOCUS_STATUS, 0,
			(V4L2_AUTO_FOCUS_STATUS_BUSY |
			 V4L2_AUTO_FOCUS_STATUS_REACHED |
			 V4L2_AUTO_FOCUS_STATUS_FAILED),
			0, V4L2_AUTO_FOCUS_STATUS_IDLE);
	ctrls->af_status->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->af_range = v4l2_ctrl_new_std_menu(handler, ops,
			V4L2_CID_AUTO_FOCUS_RANGE, 3, ~0xf,
			V4L2_AUTO_FOCUS_RANGE_NORMAL);
	ctrls->af_mode = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_af_mode_cfg, NULL);
	ctrls->af_5x5_win = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_af_5x5_win_cfg, NULL);

	/* ISO sensitivity */
	ctrls->auto_iso = v4l2_ctrl_new_std_menu(handler, ops,
			V4L2_CID_ISO_SENSITIVITY_AUTO, 1, 0,
			V4L2_ISO_SENSITIVITY_AUTO);
	ctrls->iso = v4l2_ctrl_new_int_menu(handler, ops,
			V4L2_CID_ISO_SENSITIVITY, ARRAY_SIZE(iso_qmenu) - 1,
			ARRAY_SIZE(iso_qmenu)/2 - 1, iso_qmenu);

	/* GAIN: 0x100 for 1x gain */
	ctrls->auto_gain = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_AUTOGAIN, 0, 1, 1, 1);
	ctrls->auto_gain->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->gain = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_GAIN, 0x100, 0xFFFFFFF, 1, 0x100);

	/* auto frame rate */
	ctrls->auto_frame_rate = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_auto_frame_rate_cfg, NULL);
	ctrls->afr_min_fps = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_afr_min_fps_cfg, NULL);
	ctrls->afr_sr_min_fps = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_afr_sr_min_fps_cfg, NULL);
	ctrls->afr_max_gain = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_afr_max_gain_cfg, NULL);

	ctrls->aaa_lock = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_3A_LOCK, 0, 0x7, 0, 0);

	ctrls->zoom = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_ZOOM_ABSOLUTE, 0x100, 0x400, 1, 0x100);

	ctrls->colorfx = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_colorfx_cfg, NULL);
	ctrls->aec_manual_mode = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_aec_manual_mode_cfg, NULL);
	ctrls->target_y = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_target_y_cfg, NULL);
	ctrls->target_y->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->mean_y = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_mean_y_cfg, NULL);
	ctrls->mean_y->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->aec_stable = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_aec_stable_cfg, NULL);
	ctrls->aec_stable->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->band_step = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_band_step_cfg, NULL);
	ctrls->band_step->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->set_fps = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_set_fps_cfg, NULL);
	ctrls->max_expo = v4l2_ctrl_new_custom(handler,
			&b52isp_ctrl_max_expo_cfg, NULL);
	if (handler->error) {
		pr_err("%s: failed to init all ctrls", __func__);
		return handler->error;
	}

	v4l2_ctrl_cluster(4, &ctrls->auto_expo);
	v4l2_ctrl_auto_cluster(2, &ctrls->auto_iso,
			V4L2_ISO_SENSITIVITY_MANUAL, true);
	v4l2_ctrl_auto_cluster(2, &ctrls->auto_gain, 0, true);
	v4l2_ctrl_cluster(8, &ctrls->auto_focus);
	v4l2_ctrl_cluster(3, &ctrls->auto_frame_rate);

	ctrls->af_win = b52isp_ctrl_af_win;
	ctrls->metering_roi = b52isp_ctrl_metering_roi;

	for (i = 0; i < NR_METERING_MODE; i++)
		ctrls->metering_mode[i] = b52isp_ctrl_metering_mode[i];

	return 0;
}
