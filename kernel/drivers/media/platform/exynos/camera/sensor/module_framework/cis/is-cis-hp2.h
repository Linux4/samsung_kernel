/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_HP2_H
#define IS_CIS_HP2_H

#include "is-cis.h"

#define AEB_HP2_LUT0	0x0E10
#define AEB_HP2_LUT1	0x0E2A

#define AEB_HP2_OFFSET_CIT		0x0
#define AEB_HP2_OFFSET_AGAIN	0x2
#define AEB_HP2_OFFSET_DGAIN	0x4
#define AEB_HP2_OFFSET_FLL		0x6

#ifdef CONFIG_CAMERA_VENDOR_MCD
#define CIS_CALIBRATION	1
#else
#define CIS_CALIBRATION	0
#endif

#if IS_ENABLED(CIS_CALIBRATION)
enum hp2_endian {
	HP2_LITTLE_ENDIAN = 0,
	HP2_BIG_ENDIAN = 1,
	HP2_CHECK_ENDIAN = 2,
	HP2_CONTINUE_ENDIAN = 3,
};
#define HP2_ENDIAN(a, b, endian)  ((endian == HP2_BIG_ENDIAN) ? ((a << 8)|(b)) : ((a)|(b << 8)))

#define HP2_IXC_NEXT  3
#define HP2_IXC_ARGV2  2
#define HP2_IXC_ARGV1  1
#define HP2_IXC_ARGV0  0
#endif

#define HP2_IXC_MODE_EEPROM	(10 + IXC_MODE_BASE_16BIT)

#define HP2_XTC_MAX  7

struct sensor_hp2_xtc_cal_info {
	u32 start;
	u32 end;
	u32 endian;
};

enum sensor_hp2_mode_enum {
	SENSOR_HP2_4080x3060_60FPS_VPD = 0,
	SENSOR_HP2_4000x3000_60FPS_VPD = 1,
	SENSOR_HP2_4000x3000_60FPS = 2,
	SENSOR_HP2_4000x3000_60FPS_12BIT = 3,
	SENSOR_HP2_4000x2252_60FPS_12BIT = 4,
	SENSOR_HP2_4000x2252_60FPS = 5,
	SENSOR_HP2_3328x1872_120FPS = 6,
	SENSOR_HP2_2800x2100_30FPS = 7,
	SENSOR_HP2_4080x3060_60FPS_12BIT_VPD = 8,
	SENSOR_HP2_4000x2252_120FPS = 9,
	SENSOR_HP2_8160x6120_30FPS_4SUM = 10,
	SENSOR_HP2_8160x6120_15FPS_4SUM_12BIT = 11,
	SENSOR_HP2_8000x4500_30FPS_4SUM = 12,
	SENSOR_HP2_8000x4500_30FPS_4SUM_MPC = 13,
	SENSOR_HP2_4000x3000_30FPS_4SUM_BDS_VPD = 14,
	SENSOR_HP2_4000x3000_30FPS_4SUM_BDS_12BIT = 15,
	SENSOR_HP2_4000x2252_60FPS_4SUM_BDS_12BIT = 16,
	SENSOR_HP2_8160x6120_30FPS_4SUM_12BIT = 17,
	SENSOR_HP2_4000x3000_60FPS_4SUM_VPD = 18,
	SENSOR_HP2_4000x3000_60FPS_4SUM_12BIT = 19,
	SENSOR_HP2_4000x2252_60FPS_4SUM_12BIT = 20,
	SENSOR_HP2_16320x12240_11FPS = 21,
	SENSOR_HP2_2000x1124_240FPS = 22,
	SENSOR_HP2_2000x1124_480FPS = 23,
	SENSOR_HP2_2040x1532_120FPS = 24,

	SENSOR_HP2_4080x3060_15FPS_VPD_LN4 = 25,
	SENSOR_HP2_4000x3000_15FPS_VPD_LN4 = 26,
	SENSOR_HP2_4080x3060_15FPS_12BIT_VPD_LN4 = 27,
	SENSOR_HP2_4000x3000_30FPS_12BIT_LN4 = 28,
	SENSOR_HP2_4000x2252_60FPS_12BIT_LN2 = 29,
	SENSOR_HP2_4000x2252_30FPS_12BIT_LN4 = 30,
	SENSOR_HP2_4000x3000_60FPS_12BIT_LN2 = 31,

	SENSOR_HP2_4000x3000_60FPS_12BIT_IDCG = 32,
	SENSOR_HP2_4000x2252_60FPS_12BIT_IDCG = 33,

	SENSOR_HP2_MODE_MAX,
};

#define MODE_GROUP_BASE (100)
#define MODE_GROUP_NONE	(MODE_GROUP_BASE + 1)
#define MODE_GROUP_RMS	(MODE_GROUP_BASE + 2)
enum sensor_hp2_mode_group_enum {
	SENSOR_HP2_MODE_DEFAULT,
	SENSOR_HP2_MODE_IDCG,
	SENSOR_HP2_MODE_LN2,
	SENSOR_HP2_MODE_LN4,
	SENSOR_HP2_MODE_RMS_CROP,
	SENSOR_HP2_MODE_MODE_GROUP_MAX
};
static u8 sensor_hp2_mode_groups[SENSOR_HP2_MODE_MODE_GROUP_MAX];

enum sensor_hp2_rms_zoom_mode_enum {
	SENSOR_HP2_RMS_ZOOM_MODE_NONE = 0,
	SENSOR_HP2_RMS_ZOOM_MODE_X_17 = 17,
	SENSOR_HP2_RMS_ZOOM_MODE_X_20 = 20,
	SENSOR_HP2_RMS_ZOOM_MODE_MAX
};
static u8 sensor_hp2_rms_zoom_mode[SENSOR_HP2_RMS_ZOOM_MODE_MAX];

const u32 sensor_hp2_rms_binning_ratio[SENSOR_HP2_MODE_MAX] = {
	[SENSOR_HP2_4000x3000_30FPS_4SUM_BDS_VPD] = 2400,
	[SENSOR_HP2_4000x3000_30FPS_4SUM_BDS_12BIT] = 2400,
	[SENSOR_HP2_4000x2252_60FPS_4SUM_BDS_12BIT] = 2400,
	[SENSOR_HP2_4000x3000_60FPS_4SUM_VPD] = 2000,
	[SENSOR_HP2_4000x3000_60FPS_4SUM_12BIT] = 2000,
	[SENSOR_HP2_4000x2252_60FPS_4SUM_12BIT] = 2000,
};

#define HP2_FCI_NONE (0x00FF)

struct sensor_hp2_check_reg {
	const u16 page;
	const u16 addr;
	const u16 check_data;
	const u32 max_cnt;
	const u32 interval;
};

struct sensor_hp2_default_mipi {
	const u32 mipi_rate;
	const struct sensor_regs mipi_setting;
};

struct sensor_hp2_private_data {
	const struct sensor_regs init_tnp;
	const struct sensor_regs global;
	const struct sensor_regs sram_fcm;
	const struct sensor_regs init_fcm;
	const struct sensor_regs retention_exit;
	const struct sensor_regs retention_hw_init;
	const struct sensor_regs retention_init_checksum;
	const struct sensor_regs fcm_off;
	const struct sensor_regs xtc_cal;
	const struct sensor_regs xtc_cal_off;
	const struct sensor_regs dcg_cal_off;
	const struct sensor_regs cal_decomp_off;
	const struct sensor_regs hw_rms_on;
	const struct sensor_regs hw_rms_off;
	const struct sensor_regs stream_on;
	const struct sensor_regs stream_off;
	const struct sensor_regs aeb_on;
	const struct sensor_regs aeb_off;
	const struct sensor_regs aeb_10bit_image_dt;
	const struct sensor_regs aeb_12bit_image_dt;
	const struct sensor_regs seamless_update_n_plus_1;
	const struct sensor_regs seamless_update_n_plus_2;
	const struct sensor_regs prepare_bpc_otp_read_1st;
	const struct sensor_regs prepare_bpc_otp_read_2nd;
	const struct sensor_hp2_check_reg *verify_bpc_otp_read_dma;
	const struct sensor_hp2_check_reg *verify_retention_checksum;
	const struct sensor_hp2_check_reg *verify_retention_ready;
	const struct sensor_hp2_default_mipi *default_mipi_list;
	const u32 default_mipi_list_count;
};

struct sensor_hp2_private_runtime {
	unsigned int seamless_update_fe_cnt;
	bool eeprom_cal_available;
	bool load_retention;
	bool need_stream_on_retention;
};

static const struct sensor_reg_addr sensor_hp2_reg_addr = {
	.fll = 0x0340,
	.fll_aeb_long = AEB_HP2_LUT0 + AEB_HP2_OFFSET_FLL,
	.fll_aeb_short = AEB_HP2_LUT1 + AEB_HP2_OFFSET_FLL,
	.fll_shifter = 0x0702,
	.cit = 0x0202,
	.cit_aeb_long = AEB_HP2_LUT0 + AEB_HP2_OFFSET_CIT,
	.cit_aeb_short = AEB_HP2_LUT1 + AEB_HP2_OFFSET_CIT,
	.cit_shifter = 0x0704,
	.again = 0x0204,
	.again_aeb_long = AEB_HP2_LUT0 + AEB_HP2_OFFSET_AGAIN,
	.again_aeb_short = AEB_HP2_LUT1 + AEB_HP2_OFFSET_AGAIN,
	.dgain = 0x020E,
	.dgain_aeb_long = AEB_HP2_LUT0 + AEB_HP2_OFFSET_DGAIN,
	.dgain_aeb_short = AEB_HP2_LUT1 + AEB_HP2_OFFSET_DGAIN,
	.group_param_hold = 0x0104,
};

/**
 * Register address & data
 */
#define REG_HP2_SENSOR_FEATURE_ID_1		(0x000D)
#define REG_HP2_SENSOR_FEATURE_ID_2		(0x000E)

#define DATA_HP2_GPH_HOLD				(0x0101)
#define DATA_HP2_GPH_RELEASE			(0x0001)

#define REG_HP2_ABS_GAIN_R				(0x0D82)
#define REG_HP2_ABS_GAIN_G				(0x0D84)
#define REG_HP2_ABS_GAIN_B				(0x0D86)

#define REG_HP2_FCM_INDEX				(0x0B30)

#define REG_HP2_IMB_SETTING				(0x0C50)
#define DATA_HP2_IMB_SETTING_OFF		(0x0000)

#define REG_HP2_BPC_MIN_REV				(0xb100)
#define REG_HP2_BPC_OTP_PAGE			(0x2006)
#define REG_HP2_BPC_OTP_S_ADDR			(0x0000)
#define REG_HP2_BPC_OTP_E_ADDR			(0x8FFF)
#define REG_HP2_BPC_OTP_CRC_S_ADDR		(0x8FFC)
#define DATA_HP2_BPC_OTP_TERMINATE_CODE	(0xFFFFFFFF)

int sensor_hp2_cis_check_register_poll(struct v4l2_subdev *subdev, const struct sensor_hp2_check_reg *chrck_reg);
int sensor_hp2_sensor_hp2_cis_get_bpc_otp(struct v4l2_subdev *subdev, char *bpc_data, u32 *bpc_size);
#endif
