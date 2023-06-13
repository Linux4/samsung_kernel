/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CRTA_INTERFACE_H
#define IS_CRTA_INTERFACE_H

#include "is-common-enum.h"
#include "pablo-crta-cmd-interface.h"

/* CONTROL_SENSOR CMD */
enum pablo_sensor_control_id {
	/* cis */
	SS_CTRL_CIS_REQUEST_EXPOSURE, /* legacy fn: request_exposure */
	SS_CTRL_CIS_REQUEST_GAIN, /* request_gain */
	SS_CTRL_CIS_SET_ALG_RESET_FLAG, /* set_alg_reset_flag */
	SS_CTRL_CIS_SET_NUM_OF_FRAME_PER_ONE_RTA, /* set_num_of_frame_per_one_3aa */
	SS_CTRL_CIS_APPLY_SENSOR_SETTING, /* apply_sensor_setting */
	SS_CTRL_CIS_REQUEST_RESET_EXPO_GAIN, /* request_reset_expo_gain */
	SS_CTRL_CIS_SET_STREAM_OFF_ON_MODE,  /* set_long_term_expo_mode */
	SS_CTRL_CIS_SET_SENSOR_INFO_MODE_CHANGE, /* set_sensor_info_mode_change */
	SS_CTRL_CIS_REQUEST_SENSOR_INFO_MFHDR_MODE_CHANGE, /* set_sensor_info_mfhdr_mode_change */
	SS_CTRL_CIS_ADJUST_SYNC, /* set_adjust_sync */
	SS_CTRL_CIS_REQUEST_FRAME_LENGTH_LINE, /* request_frame_length_line */
	SS_CTRL_CIS_REQUEST_SENSITIVITY, /* request_sensitivity */
	SS_CTRL_CIS_SET_12BIT_STATE, /* set_sensor_12bit_state */
	SS_CTRL_CIS_SET_NOISE_MODE, /* set_low_noise_mode */
	SS_CTRL_CIS_REQUEST_WB_GAIN, /* request_wb_gain */
	SS_CTRL_CIS_SET_MAINFLASH_DURATION, /* set_mainflash_duration */
	SS_CTRL_CIS_REQUEST_DIRECT_FLASH, /* request_direct_flash */
	SS_CTRL_CIS_SET_HDR_MODE, /* set_hdr_mode */
	SS_CTRL_CIS_SET_REMOSAIC_ZOOM_RATIO, /* set_remosaic_zoom_ratio */
	/* actuator */
	SS_CTRL_ACTUATOR_SET_POSITION, /* set_position */
	SS_CTRL_CIS_SET_SOFT_LANDING_CONFIG, /* set_soft_landing_config */
	/* flash */
	SS_CTRL_FLASH_REQUEST_FLASH, /* request_flash */
	SS_CTRL_FLASH_REQUEST_EXPO_GAIN, /* request_flash_expo_gain */
	/* aperture */
	SS_CTRL_APERTURE_SET_APERTURE_VALUE, /* set_aperture_value */

	SS_CTRL_END,
};

struct pablo_sensor_control_info {
	u32 sensor_control_size;
	u32 sensor_control[SENSOR_CONTROL_MAX];
};

/* INFO */
struct pablo_point {
	s32 x;
	s32 y;
};

struct pablo_size {
	u32 width;
	u32 height;
};

struct pablo_area {
	struct pablo_point offset;
	struct pablo_size size;
};

struct pablo_cr_set {
	u32 reg_addr;
	u32 reg_data;
};

struct is_cstat_config {
	u32 sensor_center_x;
	u32 sensor_center_y;
	u32 bitmask_bypass;
	u32 afident_dtp_bypass;
	u32 dtp_bypass;
	u32 afident_sbpc_bypass;
	u32 sbpc_bypass;
	u32 gamma_sensor_bypass;
	u32 blc_bypass;
	u32 thstatpre_bypass;
	u32 pre_grid_w;
	u32 pre_grid_h;
	u32 cgras_bypass;
	u32 thstatawb_bypass;
	u32 awb_grid_w;
	u32 awb_grid_h;
	u32 thstatae_bypass;
	u32 ae_grid_w;
	u32 ae_grid_h;
	u32 rgbyhist_bypass;
	u32 rgby_bin_num;
	u32 rgby_hist_num;
	u32 cdaf_mw_bypass;
	u32 cdaf_mw_x;
	u32 cdaf_mw_y;
	u32 wbg_bypass;
	u32 cdaf_bypass;
	u32 makergb_bypass;
	u32 luma_shading_bypass;
	u32 rgb2y_bypass;
	u32 menr_bypass;
	u32 gamma_y_bypass;
	u32 meds_bypass;
	u32 lmeds_bypass;
	u32 lmeds_w;
	u32 lmeds_h;
	u32 lmeds1_w;
	u32 lmeds1_h;
	u32 rgb_ccmFdpig_bypass;
	u32 gamma_rgb_fdpig_bypass;
	u32 drcclct_bypass;
	u32 drc_grid_w;
	u32 drc_grid_h;
	u32 lsc_step_x;
	u32 lsc_step_y;

	u32 magic;
};

struct pablo_rta_frame_info {
	/* Common */
	struct pablo_size sensor_calibration_size;
	struct pablo_point sensor_binning;
	struct pablo_area sensor_crop;
	u32 sensor_remosaic_crop_zoom_ratio;

	struct pablo_point csis_bns_binning;	/* 1000 = 1.0 */
	struct pablo_point csis_mcb_binning;	/* 1000 = 1.0 */

	u32 batch_num;

	/* CSTAT */
	u32 cstat_input_bayer_bit;
	struct pablo_size cstat_input_size;
	struct pablo_area cstat_crop_in;
	struct pablo_area cstat_crop_dzoom;
	struct pablo_size cstat_bns_size;
	struct pablo_area cstat_crop_bns;
	struct pablo_size cstat_fdpig_ds_size;
	struct pablo_area cstat_fdpig_crop;

	/* Buffer available */
	u32 cstat_out_drcclct_buffer;
	u32 cstat_out_meds_buffer;

	u32 magic;
};

struct pablo_crta_frame_info {
	/* driver -> CRTA */
	struct pablo_rta_frame_info	prfi;

	/* CRTA -> driver : config & reg*/
	struct is_cstat_config	cstat_cfg;

	u32 pdp_cr_size;
	struct pablo_cr_set pdp_cr[PDP_CR_NUM];
	u32 cstat_cr_size;
	struct pablo_cr_set cstat_cr[CSTAT_CR_NUM];

	u32 magic;
};

struct pablo_pdaf_info {
	u32			sensor_mode;	/* enum itf_vc_sensor_mode */

	u32			hpd_width;
	u32			hpd_height;

	u32			vpd_width;
	u32			vpd_height;
};

struct pablo_laser_af_info {
	u32			type;		/* enum itf_laser_af_type */
	u32			dataSize;	/* size of itf_laser_af_data_<type> structure */
};

struct pablo_sensor_mode_info {
	u32	mode;
	u32	min_expo;
	u32	max_expo;
	u32	min_again;
	u32	max_again;
	u32	min_dgain;
	u32	max_dgain;
	u32	vvalid_time;
	u32	vblank_time;
	u32	max_fps;
};

struct pablo_crta_sensor_info {
	/* sensor properties */
	u32				position;		/* enum exynos_sensor_position */
	u32				type;
	u32				pixel_order;		/* enum otf_input_order */
	u32				min_expo;
	u32				max_expo;
	u32				min_again;
	u32				max_again;
	u32				min_dgain;
	u32				max_dgain;
	u32				vvalid_time;
	u32				vblank_time;
	u32				max_fps;
	u32				mono_en;
	u32				bayer_pattern;
	u32				hdr_type;
	u32				reserved0[10];

	/* additional sensor properties */
	u32				hdr_mode;		/* enum is_sensor_hdr_mode */
	u32				sensor_12bit_mode;	/* enum is_sensor_12bit_mode */
	u32				expo_count;
	u32				sensor_12bit_state;	/* enum is_sensor_12bit_state */
	u32				initial_aperture;
	u32				reserved1[10];

	/* peri */
	u32				actuator_available;
	u32				flash_available;
	u32				laser_af_available;
	u32				tof_af_available;
	u32				reserved2[10];

	/* ae */
	u32				expo[EXPOSURE_NUM_MAX];
	u32				frame_period[EXPOSURE_NUM_MAX];
	u64				line_period[EXPOSURE_NUM_MAX];
	u32				again[EXPOSURE_NUM_MAX];
	u32				dgain[EXPOSURE_NUM_MAX];
	u32				next_exposure[EXPOSURE_NUM_MAX];
	u32				next_frame_period[EXPOSURE_NUM_MAX];
	u64				next_line_period[EXPOSURE_NUM_MAX];
	u32				next_again[EXPOSURE_NUM_MAX];
	u32				next_dgain[EXPOSURE_NUM_MAX];
	u32				current_fps;
	u32				reserved3[10];

	/* af */
	u32				actuator_position;
	u32				temperature_valid;
	int				temperature;
	u32				voltage_valid;
	int				voltage;
	u32				laser_af_distance;
	u32				laser_af_confiden;

	/* pdaf */
	struct pablo_pdaf_info		pdaf_info;
	struct pablo_laser_af_info	laser_af_info;
	u32				reserved5[8];

	/* seamless mode */
	struct pablo_sensor_mode_info seamless_mode_info[SEAMLESS_MODE_MAX];
	u32				seamless_mode_cnt;

	u32 magic;
};

#endif /* IS_CRTA_INTERFACE_H */
