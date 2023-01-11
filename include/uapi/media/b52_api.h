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
 */

#ifndef _B52_API_H
#define _B52_API_H

#include <linux/videodev2.h>

#define NR_METERING_WIN_WEIGHT 13
#define B52_NR_PIPELINE_MAX 2

struct b52_regval {
	__u32	reg;
	__u32	val;
};

struct b52_data_node {
	__u32	size;
	void	*buffer;
};

struct b52isp_profile {
	unsigned int profile_id;
	void	*arg;
};

struct b52isp_win {
	__s32 left;
	__s32 top;
	__s32 right;
	__s32 bottom;
};

struct b52isp_expo_metering {
	unsigned int mode;
	struct b52isp_win stat_win;
	struct v4l2_rect center_win;
	unsigned int win_weight[NR_METERING_WIN_WEIGHT];
};

struct b52isp_awb_gain {
	int write;
	unsigned int b;
	unsigned int gb;
	unsigned int gr;
	unsigned int r;
};

struct memory_sensor {
	char	name[32];
};

enum adv_dns_type {
	ADV_DNS_NONE = 0,
	ADV_DNS_Y,
	ADV_DNS_UV,
	ADV_DNS_YUV,
	ADV_DNS_MAX,
};

struct b52isp_adv_dns {
	enum adv_dns_type type;
	unsigned int times;
};
enum OTP_TYPE {
	SENSOR_TO_SENSOR = 1,
	SENSOR_TO_ISP = 2,
	ISP_TO_SENSOR = 3,
};
struct sensor_otp {
	enum OTP_TYPE	otp_type;
	__u16			lsc_otp_len;
	__u16			wb_otp_len;
	__u16			vcm_otp_len;
	__u16			module_data_len;
	void				*otp_data;
	void				*module_data;
	void				*full_otp;
	__u16			full_otp_offset;
	__u16			full_otp_len;
	__u16			*read_otp_len;
};

enum type_aeag {
	TYPE_3A_UNLOCK,
	TYPE_3A_LOCKED,
	TYPE_3A_CNT,
};

enum type_combo {
	TYPE_3D_COMBO,
	TYPE_HS_COMBO,
	TYPE_HDR_COMBO,
	TYPE_COMBO_CNT,
};

struct b52isp_path_arg {
	enum type_aeag	aeag;
	enum type_combo	combo;
	__u16		nr_frame;
	__u16		ratio_1_2;
	__u16		ratio_1_3;
	__u16		linear_yuv;
};

struct b52isp_anti_shake_arg {
	__u16		block_size;
	int			enable;
};

enum type_brightness_ops {
	TYPE_READ = 0,
	TYPE_WRITE,
};

struct b52isp_brightness {
	enum type_brightness_ops op;
	__u8    ygain_h;
	__u8    ygain_l;
	__u8    yoffset_h;
	__u8    yoffset_l;
	__u8    uv_matrix_00_h;
	__u8    uv_matrix_00_l;
	__u8    uv_matrix_01_h;
	__u8    uv_matrix_01_l;
	__u8    uv_matrix_10_h;
	__u8    uv_matrix_10_l;
	__u8    uv_matrix_11_h;
	__u8    uv_matrix_11_l;
	__u8    hgain;
};

enum SENSOR_ESD_STATUS {
	SENSOR_ESD_ST_DAMAGED = 0,
	SENSOR_ESD_ST_WORKING = 1,
	SENSOR_ESD_ST_UNKNOWN = 2,
};

struct b52isp_saturation {
	__u8 sat_arr[6];
};

struct b52isp_sharpness {
	__u8 sha_arr[6];
};

enum v4l2_priv_colorfx {
	V4L2_PRIV_COLORFX_NONE          = 0,
	V4L2_PRIV_COLORFX_MONO_CHROME   = 1,
	V4L2_PRIV_COLORFX_NEGATIVE      = 2,
	V4L2_PRIV_COLORFX_SEPIA         = 3,
	V4L2_PRIV_COLORFX_SKETCH        = 4,
	V4L2_PRIV_COLORFX_WATER_COLOR   = 5,
	V4L2_PRIV_COLORFX_INK           = 6,
	V4L2_PRIV_COLORFX_CARTOON       = 7,
	V4L2_PRIV_COLORFX_COLOR_INK     = 8,
	V4L2_PRIV_COLORFX_AQUA          = 9,
	V4L2_PRIV_COLORFX_BLACK_BOARD   = 10,
	V4L2_PRIV_COLORFX_WHITE_BOARD   = 11,
	V4L2_PRIV_COLORFX_POSTER        = 12,
	V4L2_PRIV_COLORFX_SOLARIZATION  = 13,
	V4L2_PRIV_COLORFX_MAX,
};

enum b52_vcm_type {
	VCM_BUILD_IN = 0,
	VCM_AD5820 = 1,
	VCM_DW9714 = VCM_AD5820,
	VCM_AD5823 = 2,
	VCM_DW9804 = 4,
	VCM_DW9806 = VCM_DW9804,
	VCM_DW9807 = VCM_DW9804,
	VCM_DW9761 = VCM_DW9804,
	VCM_DW9718 = 5,
	VCM_NONE,
};

enum b52_flash_type {
	FLASH_GPIO = 0,
	FLASH_BOARD,
	FLASH_NONE,
};


#define CID_AF_SNAPSHOT	1
#define CID_AF_CONTINUOUS	2

/*
 * the auto frame rate control.
 * if set it to enable, the frame rate will drop to increase exposure.
 */
#define CID_AUTO_FRAME_RATE_DISABLE   0
#define CID_AUTO_FRAME_RATE_ENABLE    1

/*
 * the range for auto frame rate
 */
#define CID_AFR_MIN_FPS_MIN   5
#define CID_AFR_MIN_FPS_MAX   30

/*
 * save and restore min fps for auto frame rate
 */
#define CID_AFR_SAVE_MIN_FPS      0
#define CID_AFR_RESTORE_MIN_FPS   1

/*
 * If enable AF 5x5 windown, the focus is based on central part.
 */
#define CID_AF_5X5_WIN_DISABLE    0
#define CID_AF_5X5_WIN_ENABLE     1

/*
 * If 3A work on threshold, the gain or exposure value will be
 * decided by max and min value, otherwise the value is setted value.
 */
#define CID_AEC_AUTO_THRESHOLD	0
#define CID_AEC_MANUAL		1

#define CID_AEC_NO_STABLE		0
#define CID_AEC_STABLE		1

#define CID_AFR_MAX_GAIN_MIN   0
#define CID_AFR_MAX_GAIN_MAX   0xffff
/*
 * The OTP control variant
 */
#define V4L2_CID_SENSOR_OTP_CONTROL_NONE	0x00
#define V4L2_CID_SENSOR_OTP_CONTROL_WB	0x01
#define V4L2_CID_SENSOR_OTP_CONTROL_LENC	0x02

/* B52 component names */
#define B52_IDI1_NAME		"b52isd-IDI1"
#define B52_IDI2_NAME		"b52isd-IDI2"
#define B52_PATH_YUV_1_NAME	"b52isd-Pipeline#1"
#define B52_PATH_YUV_2_NAME	"b52isd-Pipeline#2"
#define B52_PATH_RAW_1_NAME	"b52isd-DataDump#1"
#define B52_PATH_RAW_2_NAME	"b52isd-DataDump#2"
#define B52_PATH_M2M_1_NAME	"b52isd-MemorySensor#1"
#define B52_PATH_M2M_2_NAME	"b52isd-MemorySensor#2"
#define B52_PATH_COMBINE_NAME	"b52isd-combine"
#define B52_OUTPUT_A_NAME	"b52isd-Output-A"
#define B52_OUTPUT_B_NAME	"b52isd-Output-B"
#define B52_OUTPUT_C_NAME	"b52isd-Output-C"
#define B52_OUTPUT_D_NAME	"b52isd-Output-D"
#define B52_OUTPUT_E_NAME	"b52isd-Output-E"
#define B52_OUTPUT_F_NAME	"b52isd-Output-F"
#define B52_INPUT_A_NAME	"b52isd-Input-A"
#define B52_INPUT_B_NAME	"b52isd-Input-B"
#define B52_INPUT_C_NAME	"b52isd-Input-C"
#define VDEV_INPUT_A_NAME	"vin<"B52_INPUT_A_NAME">"
#define VDEV_INPUT_B_NAME	"vin<"B52_INPUT_B_NAME">"
#define VDEV_INPUT_C_NAME	"vin<"B52_INPUT_C_NAME">"

/* the specific controls */
#define V4L2_CID_PRIVATE_AF_MODE \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1000)
#define V4L2_CID_PRIVATE_COLORFX \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1001)
#define V4L2_CID_PRIVATE_AUTO_FRAME_RATE \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1002)
#define V4L2_CID_PRIVATE_AFR_MIN_FPS \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1003)
#define V4L2_CID_PRIVATE_AFR_SR_MIN_FPS \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1004)
#define V4L2_CID_PRIVATE_AF_5X5_WIN \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1005)
#define V4L2_CID_PRIVATE_SENSOR_OTP_CONTROL	\
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1006)
#define V4L2_CID_PRIVATE_AEC_MANUAL_MODE\
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1007)
#define V4L2_CID_PRIVATE_TARGET_Y\
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1008)
#define V4L2_CID_PRIVATE_MEAN_Y\
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1009)
#define V4L2_CID_PRIVATE_AEC_STABLE\
	(V4L2_CID_CAMERA_CLASS_BASE + 0x100a)
#define V4L2_CID_PRIVATE_AFR_MAX_GAIN \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x100B)
#define V4L2_CID_PRIVATE_BAND_STEP \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x100C)
#define V4L2_CID_PRIVATE_SET_FPS \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x100D)
#define V4L2_CID_PRIVATE_FLASH_DURATION \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x100E)
#define V4L2_CID_PRIVATE_FLASH_CURRENT \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x100F)
#define V4L2_CID_PRIVATE_MAX_EXPO \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1010)
#define V4L2_PLANE_SIGNATURE_PIPELINE_META	\
	v4l2_fourcc('M', 'E', 'T', 'A')
#define V4L2_PLANE_SIGNATURE_PIPELINE_INFO	\
	v4l2_fourcc('P', 'P', 'I', 'F')



#define VIDIOC_PRIVATE_B52ISP_TOPOLOGY_SNAPSHOT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 0, struct b52isp_profile)
#define VIDIOC_PRIVATE_B52ISP_CONFIG_AF_WINDONW \
	_IOW('V', BASE_VIDIOC_PRIVATE + 1, struct v4l2_rect)
#define VIDIOC_PRIVATE_B52ISP_CONFIG_EXPO_METERING_MODE \
	_IOW('V', BASE_VIDIOC_PRIVATE + 2, struct b52isp_expo_metering)
#define VIDIOC_PRIVATE_B52ISP_CONFIG_EXPO_METERING_ROI \
	_IOW('V', BASE_VIDIOC_PRIVATE + 3, struct b52isp_win)
#define VIDIOC_PRIVATE_B52ISP_DOWNLOAD_CTDATA \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct b52_data_node)
#define VIDIOC_PRIVATE_B52ISP_UPLOAD_CTDATA \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct b52_data_node)
#define VIDIOC_PRIVATE_B52ISP_CONFIG_AWB_GAIN \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct b52isp_awb_gain)
#define VIDIOC_PRIVATE_B52ISP_CONFIG_MEMORY_SENSOR \
	_IOW('V', BASE_VIDIOC_PRIVATE + 7, struct memory_sensor)
#define VIDIOC_PRIVATE_B52ISP_CONFIG_ADV_DNS \
	_IOW('V', BASE_VIDIOC_PRIVATE + 8, struct b52isp_adv_dns)
#define VIDIOC_PRIVATE_B52ISP_SET_PATH_ARG \
	_IOW('V', BASE_VIDIOC_PRIVATE + 9, struct b52isp_path_arg)
#define VIDIOC_PRIVATE_B52ISP_ANTI_SHAKE\
	_IOW('V', BASE_VIDIOC_PRIVATE + 10, struct b52isp_anti_shake_arg)
#define VIDIOC_PRIVATE_B52ISP_SENSOR_OTP\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct sensor_otp)
#define VIDIOC_PRIVATE_B52ISP_SENSOR_REINIT\
	_IO('V', BASE_VIDIOC_PRIVATE + 12)
#define VIDIOC_PRIVATE_B52ISP_BRIGHTNESS\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct b52isp_brightness)
#define VIDIOC_PRIVATE_B52ISP_SENSOR_ESD_STATUS\
	_IOR('V', BASE_VIDIOC_PRIVATE + 14, int)
#define VIDIOC_PRIVATE_B52ISP_SATURATION\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct b52isp_saturation)
#define VIDIOC_PRIVATE_B52ISP_SHARPNESS\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 16, struct b52isp_sharpness)
#endif /* _B52_API_H */
