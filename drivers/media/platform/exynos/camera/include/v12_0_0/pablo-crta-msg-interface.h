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

#ifndef IS_CRTA_MSG_INTERFACE_H
#define IS_CRTA_MSG_INTERFACE_H

enum pablo_hic_open_param {
	OPEN_CMD,
	OPEN_HW_ID,
	OPEN_REP,
	OPEN_POS,
	OPEN_F_TYPE,
	OPEN_MAX,
};

enum pablo_hic_put_buf {
	PUT_BUF_CMD,
	PUT_BUF_TYPE,
	PUT_BUF_DVA_HIGH,
	PUT_BUF_DVA_LOW,
	PUT_BUF_SIZE,
	PUT_BUF_MAX,
};

enum pablo_hic_set_buf_idx {
	SET_BUF_CMD,
	SET_BUF_TYPE,
	SET_BUF_FCOUNT,
	SET_BUF_MAX,
};

enum pablo_hic_start {
	START_CMD,
	START_PCSI_HIGH,
	START_PCSI_LOW,
	START_MAX,
};

enum pablo_hic_shot {
	SHOT_CMD,
	SHOT_FCOUNT,
	SHOT_META_DVA_HIGH,
	SHOT_META_DVA_LOW,
	SHOT_PCFI_DVA_HIGH,
	SHOT_PCFI_DVA_LOW,
	SHOT_MAX,
};

enum pablo_hic_cstat_frame_start {
	FRAME_START_CMD,
	FRAME_START_FCOUNT,
	FRAME_START_PCSI_DVA_HIGH,
	FRAME_START_PCSI_DVA_LOW,
	FRAME_START_MAX,
};

enum pablo_hic_cstat_cdaf_end {
	CDAF_CMD,
	CDAF_FCOUNT,
	CDAF_CDAF_RAW_DVA_HIGH,
	CDAF_CDAF_RAW_DVA_LOW,
	CDAF_PDAF_TAIL_DVA_HIGH,
	CDAF_PDAF_TAIL_DVA_LOW,
	CDAF_LASER_AF_DVA_HIGH,
	CDAF_LASER_AF_DVA_LOW,
	CDAF_MAX,
};

enum pablo_hic_pdp_stat0_end {
	PDP_STAT0_CMD,
	PDP_STAT0_FCOUNT,
	PDP_STAT0_DVA_HIGH,
	PDP_STAT0_DVA_LOW,
	PDP_STAT0_MAX,
};

enum pablo_hic_pdp_stat1_end {
	PDP_STAT1_CMD,
	PDP_STAT1_FCOUNT,
	PDP_STAT1_DVA_HIGH,
	PDP_STAT1_DVA_LOW,
	PDP_STAT1_MAX,
};

enum pablo_hic_cstat_frame_end {
	FRAME_END_CMD,
	FRAME_END_FCOUNT,
	FRAME_END_SHOT_DVA_HIGH,
	FRAME_END_SHOT_DVA_LOW,
	FRAME_END_EDGE_SCORE,
	FRAME_END_PRE_THUMB_DVA_HIGH,
	FRAME_END_AE_THUMB_DVA_HIGH,
	FRAME_END_AWB_THUMB_DVA_HIGH,
	FRAME_END_RGBY_HIST_DVA_HIGH,
	FRAME_END_CDAF_MW_DVA_HIGH,
	FRAME_END_VPDAF_DVA_HIGH,
	FRAME_END_MAX,
};

enum pablo_hic_stop {
	STOP_CMD,
	STOP_SUSPEND,
	STOP_MAX,
};

enum pablo_hic_close {
	CLOSE_CMD,
	CLOSE_MAX,
};

enum pablo_hic_control_sensor {
	CONTROL_SENSOR_CMD,
	CONTROL_SENSOR_BUF_TYPE,
	CONTROL_SENSOR_BUF_DVA_HIGH,
	CONTROL_SENSOR_BUF_DVA_LOW,
	CONTROL_SENSOR_MAX,
};

#endif /* IS_CRTA_MSG_INTERFACE_H */
