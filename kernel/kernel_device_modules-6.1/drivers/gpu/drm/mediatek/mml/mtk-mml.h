/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#ifndef __MTK_MML_H__
#define __MTK_MML_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ktime.h>

#define MML_MAX_INPUTS		2
#define MML_MAX_OUTPUTS		2
#define MML_MAX_PLANES		3
#define MML_PIPE_CNT		2

struct mml_job {
	uint32_t jobid;
	int32_t fence;
};

enum mml_mode {
	MML_MODE_NOT_SUPPORT = -1,
	MML_MODE_UNKNOWN = 0,
	MML_MODE_DIRECT_LINK,
	MML_MODE_RACING,
	MML_MODE_MML_DECOUPLE,
	MML_MODE_MDP_DECOUPLE,
	MML_MODE_APUDC,		/* APU Direct Couple MML_RDMA */

	/* belows are modes from driver internally */
	MML_MODE_DDP_ADDON,
	MML_MODE_SRAM_READ,
};

enum mml_orientation {
	MML_ROT_0 = 0,
	MML_ROT_90,
	MML_ROT_180,
	MML_ROT_270
};

enum mml_pq_scenario {
	MML_PQ_MEDIA_UNKNOWN = 0x0,
	MML_PQ_MEDIA_VIDEO = 0x101,
	MML_PQ_MEDIA_GAME_NORMAL = 0x1001,
	MML_PQ_MEDIA_GAME_HDR = 0x1002,
	MML_PQ_MEDIA_ISP_PREVIEW = 0x10001
};

/* only used for crtc dual pipe */
enum mml_dl_pos {
	MML_DL_POS_UNKNOWN,
	MML_DL_POS_DUAL,
	MML_DL_POS_LEFT,
	MML_DL_POS_RIGHT,
};

struct mml_pq_film_grain_params {
	unsigned char apply_grain;
	unsigned int grain_seed;
	unsigned char update_grain;
	unsigned char film_grain_params_ref_idx;
	unsigned char num_y_points;
	unsigned char point_y_value[16];
	unsigned char point_y_scaling[16];
	unsigned char chroma_scaling_from_luma;
	unsigned char num_cb_points;
	unsigned char point_cb_value[16];
	unsigned char point_cb_scaling[16];
	unsigned char num_cr_points;
	unsigned char point_cr_value[16];
	unsigned char point_cr_scaling[16];
	unsigned char grain_scaling;
	unsigned char ar_coeff_lag;
	int ar_coeffs_y[25];
	int ar_coeffs_cb[25];
	int ar_coeffs_cr[25];
	unsigned char ar_coeff_shift;
	unsigned char grain_scale_shift;
	unsigned short cb_mult;
	unsigned short cb_luma_mult;
	unsigned short cb_offset;
	unsigned short cr_mult;
	unsigned short cr_luma_mult;
	unsigned short cr_offset;
	unsigned char overlap_flag;
	unsigned char clip_to_restricted_range;
};

struct mml_pq_video_param {
	uint32_t video_id;
	uint32_t  time_stamp;
	bool ishdr2sdr:1;
	uint32_t param_table;
	int32_t xml_mode_id;
	int32_t buffer_fd;
	int32_t qpsum;
	struct mml_pq_film_grain_params fg_meta;
	uint16_t rotate;
};

struct mml_pq_config {
	bool en:1;
	bool en_sharp:1;
	bool en_ur:1;
	bool en_dc:1;
	bool en_color:1;
	bool en_hdr:1;
	bool en_ccorr:1;
	bool en_dre:1;
	bool en_region_pq:1;
	bool en_fg:1;
	bool en_c3d:1;
	bool en_clarity:1;
};

enum mml_pq_enable_flag {
	/* If MML_PQ_DEFAULT_EN, use default enable set by PQ,
	 * and the other flags will be ignored.
	 */
	MML_PQ_DEFAULT_EN = 1 << 0,
	MML_PQ_COLOR_EN = 1 << 1,
	MML_PQ_SHP_EN = 1 << 2,
	MML_PQ_ULTRARES_EN = 1 << 3,
	MML_PQ_DYN_CONTRAST_EN = 1 << 4,
	MML_PQ_DRE_EN = 1 << 5,
	MML_PQ_CCORR_EN = 1 << 6,
	MML_PQ_AI_SCENE_PQ_EN = 1 << 7,
	MML_PQ_AI_SDR_TO_HDR_EN = 1 << 8,
	MML_PQ_VIDEO_HDR_EN = 1 << 9,
	MML_PQ_AI_REGION_EN = 1 << 10,
	MML_PQ_C3D_EN = 1 << 11,
	MML_PQ_CLARITY_EN = 1 << 12,
};

enum mml_pq_user_info {
	MML_PQ_USER_UNKNOWN = 0,
	MML_PQ_USER_HWC = 1,
	MML_PQ_USER_GPU = 2,
};

enum mml_pq_video_mode {
	MML_PQ_NORMAL = 0,
	MML_PQ_HDR10,
	MML_PQ_HDR10P,
	MML_PQ_HDRVIVID,
	MML_PQ_AISPQ,
	MML_PQ_AISDR2HDR,
	MML_PQ_AIREGION,
	MML_PQ_CCORR,
	MML_PQ_HLG,
};

struct mml_pq_param {
	uint32_t enable;
	uint32_t time_stamp;
	uint32_t scenario;
	uint32_t layer_id;
	uint32_t disp_id;
	uint32_t src_gamut;	/* legacy */
	uint32_t dst_gamut;	/* legacy */
	uint32_t src_hdr_video_mode;
	uint32_t metadata_mem_id;
	struct mml_pq_video_param video_param;
	uint32_t user_info;
	uint32_t app_hint;
	uint32_t connector_id;
	uint32_t current_panel_lum;
	bool svp;
};

struct mml_color_desc {
	uint8_t gamut;		/* enum mml_gamut, color primaries */
	uint8_t ycbcr_enc;	/* enum mml_ycbcr_encoding, matrix coeffs */
	uint8_t color_range;	/* enum mml_color_range, quantization */
	uint8_t gamma;		/* enum mml_gamma, transfer function */
};

struct mml_frame_data {
	uint32_t width;
	uint32_t height;
	uint32_t y_stride;
	uint32_t uv_stride;
	uint32_t vert_stride;
	uint32_t format;
	uint64_t modifier;
	uint16_t profile;	/* enum mml_ycbcr_profile, legacy */
	struct mml_color_desc color;
	uint32_t plane_offset[MML_MAX_PLANES];
	uint8_t plane_cnt;
	bool secure;
};

struct mml_rect {
	uint32_t left;
	uint32_t top;
	uint32_t width;
	uint32_t height;
};

#define MML_SUBPIXEL_BITS	20

struct mml_crop {
	struct mml_rect r;
	uint32_t x_sub_px;
	uint32_t y_sub_px;
	uint32_t w_sub_px;
	uint32_t h_sub_px;
};

struct mml_frame_dest {
	struct mml_frame_data data;
	struct mml_crop crop;
	struct mml_rect compose;
	uint16_t rotate;
	bool flip:1;
	struct mml_pq_config pq_config;
};

struct mml_frame_info {
	struct mml_frame_data src;
	struct mml_frame_data seg_map;
	struct mml_frame_dest dest[MML_MAX_OUTPUTS];
	uint32_t act_time;	/* ns time for mml frame */
	uint8_t dest_cnt;	/* should be < MML_MAX_OUTPUTS */
	int8_t mode;	/* one of mml_mode */
	int8_t dl_pos;
	uint16_t disp_done_event;
	uint8_t ovlsys_id;
	bool alpha;	/* alpha channel preserve */
};

struct mml_frame_size {
	uint32_t width;
	uint32_t height;
};

struct mml_buffer {
	int32_t fd[MML_MAX_PLANES];
	void *dmabuf[MML_MAX_PLANES];
	uint32_t size[MML_MAX_PLANES];
	uint8_t cnt;
	int32_t fence;
	bool use_dma:1;
	bool flush:1;
	bool invalid:1;
};

#define mml_set_apu_handle(b, h)	(*((u64 *)((b)->dmabuf[0])) = h)
#define mml_get_apu_handle(b)		(*((u64 *)((b)->dmabuf[0])))

struct mml_frame_buffer {
	struct mml_buffer src;
	struct mml_buffer seg_map;
	struct mml_buffer dest[MML_MAX_OUTPUTS];
	uint8_t dest_cnt;
};

enum mml_layer_id {
	MML_DLO_OVLSYS0 = 0,
	MML_DLO_OVLSYS1,
};

/**
 * struct mml_submit - submit mml task from user
 * @job:	[in/out] The mml task serial number and fence for user to wait.
 *		job->jobid must provide if update flag set to true and
 *		mml adaptor will try to match previous mml_frame_config to
 *		reuse commands.
 * @info:	Frame configs which not change between frame-to-frame.
 *		MML try to match same info in cache and reuse same commands.
 * @buffer:	Buffer fd and related parameters.
 * @layer:	Rect on display screen for mml detect layout (left/right pipe).
 * @dl_out:	Direct Link output ROI.
 * @sec:	End-Time for time value second
 * @usec:	End-Time for time value usecond
 * @pq_param:	PQ parameters pointer. Leave empty also disable PQ.
 * @update:	Flag to enable partial update.
 *		Turn on to force mml driver find same job->jobid in cache to
 *		reuse previous configs and commands.
 */
struct mml_submit {
	struct mml_job *job;
	struct mml_frame_info info;
	struct mml_frame_buffer buffer;
	struct mml_rect layer;
	struct mml_rect dl_out[MML_PIPE_CNT];
	struct timeval_t {
		uint64_t sec;
		uint64_t nsec;
	} end;
	struct mml_pq_param *pq_param[MML_MAX_OUTPUTS];
	bool update;
};

#endif	/* __MTK_MML_H__ */
