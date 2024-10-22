/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MML_PQ_H__
#define __MTK_MML_PQ_H__

#include <linux/kernel.h>
#include <linux/types.h>

#include "mtk-mml.h"

struct mml_pq_rsz_tile_init_param {
	u32 coeff_step_x;
	u32 coeff_step_y;
	u32 precision_x;
	u32 precision_y;
	u32 crop_offset_x;
	u32 crop_subpix_x;
	u32 crop_offset_y;
	u32 crop_subpix_y;
	u32 hor_dir_scale;
	u32 hor_algorithm;
	u32 ver_dir_scale;
	u32 ver_algorithm;
	u32 vertical_first;
	u32 ver_cubic_trunc;
};

struct mml_pq_reg {
	u16 offset;
	u32 value;
	u32 mask;
};

struct mml_pq_tile_init_result {
	u8 rsz_param_cnt;
	struct mml_pq_rsz_tile_init_param rsz_param[MML_MAX_OUTPUTS];
	uint32_t rsz_reg_cnt[MML_MAX_OUTPUTS];
	struct mml_pq_reg *rsz_regs[MML_MAX_OUTPUTS];
	bool is_set_test;
};

struct mml_pq_frame_info {
	struct mml_crop frame_in_crop_s[MML_MAX_OUTPUTS]; /*rsz out size*/
	struct mml_frame_size crop_size_s; /*video crop in size*/
	struct mml_frame_size frame_size_s; /*video frame in size*/
	u16 out_rotate[MML_MAX_OUTPUTS];
};

struct mml_pq_tile_init_job {
	/* input from user-space */
	u32 result_job_id;
	struct mml_pq_tile_init_result *result;

	/* output to user-space */
	u32 new_job_id;
	struct mml_frame_info info;
	struct mml_frame_size dst[MML_MAX_OUTPUTS];
	struct mml_pq_frame_info size_info;
	struct mml_pq_param param[MML_MAX_OUTPUTS];
};

struct mml_pq_aal_config_param {
	u32 dre_blk_width;
	u32 dre_blk_height;
};

struct mml_pq_comp_config_result {
	u8 param_cnt;
	u32 hdr_reg_cnt;
	struct mml_pq_reg *hdr_regs;
	u32 *hdr_curve;
	bool is_hdr_need_readback;
	struct mml_pq_aal_config_param *aal_param;
	u32 aal_reg_cnt;
	struct mml_pq_reg *aal_regs;
	u32 *aal_curve;
	bool is_aal_need_readback;
	bool is_clarity_need_readback;
	bool is_dc_need_readback;
	u32 ds_reg_cnt;
	struct mml_pq_reg *ds_regs;
	u32 color_reg_cnt;
	struct mml_pq_reg *color_regs;
	u32 c3d_reg_cnt;
	struct mml_pq_reg *c3d_regs;
	u32 *c3d_lut; // 9*9*9*3
	bool is_set_test;
};

struct mml_pq_comp_config_job {
	/* input from user-space */
	u32 result_job_id;
	struct mml_pq_comp_config_result *result;

	/* output to user-space */
	u32 new_job_id;
	struct mml_frame_info info;
	struct mml_frame_size dst[MML_MAX_OUTPUTS];
	struct mml_pq_frame_info size_info;
	struct mml_pq_param param[MML_MAX_OUTPUTS];
};

struct mml_pq_aal_readback_result {
	u8 param_cnt;
	u32 *aal_pipe0_hist;
	u32 *aal_pipe1_hist;
	u32 cut_pos_x;
	bool is_dual;
};

struct mml_pq_aal_readback_job {
	/* input from user-space */
	u32 result_job_id;

	/* output to user-space */
	struct mml_pq_aal_readback_result *result;
	u32 new_job_id;
	struct mml_frame_info info;
	struct mml_pq_frame_info size_info;
	struct mml_pq_param param[MML_MAX_OUTPUTS];
};

struct mml_pq_hdr_readback_result {
	u8 param_cnt;
	u32 *hdr_pipe0_hist;
	u32 *hdr_pipe1_hist;
	u32 cut_pos_x;
	bool is_dual;
};

struct mml_pq_hdr_readback_job {
	/* input from user-space */
	u32 result_job_id;

	/* output to user-space */
	struct mml_pq_hdr_readback_result *result;
	u32 new_job_id;
	struct mml_frame_info info;
	struct mml_pq_frame_info size_info;
	struct mml_pq_param param[MML_MAX_OUTPUTS];
};

struct mml_pq_wrot_callback_job {
	/* input from user-space */
	u32 result_job_id;

	/* output to user-space */
	u32 new_job_id;
	struct mml_frame_info info;
	struct mml_pq_frame_info size_info;
	struct mml_pq_param param[MML_MAX_OUTPUTS];
};

struct mml_pq_clarity_readback_result {
	u8 param_cnt;
	u32 *clarity_pipe0_hist;
	u32 *clarity_pipe1_hist;
	u32 cut_pos_x;
	bool is_dual;
};

struct mml_pq_clarity_readback_job {
	/* input from user-space */
	u32 result_job_id;

	/* output to user-space */
	struct mml_pq_clarity_readback_result *result;
	u32 new_job_id;
	struct mml_frame_info info;
	struct mml_pq_frame_info size_info;
	struct mml_pq_param param[MML_MAX_OUTPUTS];
};

struct mml_pq_dc_readback_result {
	u8 param_cnt;
	u32 *dc_pipe0_hist;
	u32 *dc_pipe1_hist;
	u32 cut_pos_x;
	bool is_dual;
};

struct mml_pq_dc_readback_job {
	/* input from user-space */
	u32 result_job_id;

	/* output to user-space */
	struct mml_pq_dc_readback_result *result;
	u32 new_job_id;
	struct mml_frame_info info;
	struct mml_pq_frame_info size_info;
	struct mml_pq_param param[MML_MAX_OUTPUTS];
};

#define MML_PQ_IOC_MAGIC 'W'
#define MML_PQ_IOC_TILE_INIT _IOWR(MML_PQ_IOC_MAGIC, 0,\
		struct mml_pq_tile_init_job)
#define MML_PQ_IOC_COMP_CONFIG _IOWR(MML_PQ_IOC_MAGIC, 1,\
		struct mml_pq_comp_config_job)
#define MML_PQ_IOC_AAL_READBACK _IOWR(MML_PQ_IOC_MAGIC, 2,\
		struct mml_pq_aal_readback_job)
#define MML_PQ_IOC_HDR_READBACK _IOWR(MML_PQ_IOC_MAGIC, 3,\
		struct mml_pq_hdr_readback_job)
#define MML_PQ_IOC_WROT_CALLBACK _IOWR(MML_PQ_IOC_MAGIC, 4,\
		struct mml_pq_wrot_callback_job)
#define MML_PQ_IOC_CLARITY_READBACK _IOWR(MML_PQ_IOC_MAGIC, 5,\
		struct mml_pq_clarity_readback_job)
#define MML_PQ_IOC_DC_READBACK _IOWR(MML_PQ_IOC_MAGIC, 6,\
		struct mml_pq_dc_readback_job)
#endif	/* __MTK_MML_PQ_H__ */
