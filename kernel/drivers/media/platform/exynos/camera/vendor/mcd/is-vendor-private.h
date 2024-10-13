/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vendor functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDOR_PRIVATE_H
#define IS_VENDOR_PRIVATE_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>

#include "is-sysfs.h"
#include "is-device-rom.h"
#include "is-sec-define.h"

#define IS_TILT_CAL_TELE_EFS_MAX_SIZE 800
#define IS_GYRO_EFS_MAX_SIZE 30
#define SUPPORTED_CAMERA_IDS_MAX 11

struct is_vendor_private {
	struct is_vendor_rom rom[ROM_ID_MAX];
	struct mutex rom_lock;

	u32 sensor_id[SENSOR_POSITION_MAX];
	const char *sensor_name[SENSOR_POSITION_MAX];

	u32 max_camera_num;
	struct is_cam_info cam_infos[CAM_INFO_MAX];

	u32 supported_camera_ids[SUPPORTED_CAMERA_IDS_MAX];
	u32 max_supported_camera;

	u32 is_vendor_sensor_count;
	u32 ois_sensor_index;
	u32 mcu_sensor_index;
	u32 aperture_sensor_index;
	bool check_sensor_vendor;
	bool use_module_check;
	u32 eeprom_on_delay;

	struct is_sysfs_config sysfs_config;

#ifdef CONFIG_OIS_USE
	bool ois_ver_read;
#endif /* CONFIG_OIS_USE */
	bool suspend_resume_disable;
	bool need_cold_reset;
	int32_t rear_tof_mode_id;
	int32_t front_tof_mode_id;
#ifdef USE_TOF_AF
	struct tof_data_t tof_af_data;
	struct mutex tof_af_lock;
#endif
	struct tof_info_t tof_info;

	int tilt_cal_tele_efs_size;
	uint8_t tilt_cal_tele_efs_data[IS_TILT_CAL_TELE_EFS_MAX_SIZE];
	int tilt_cal_tele2_efs_size;
	uint8_t tilt_cal_tele2_efs_data[IS_TILT_CAL_TELE_EFS_MAX_SIZE];
	int gyro_efs_size;
	uint8_t gyro_efs_data[IS_GYRO_EFS_MAX_SIZE];
	int cal_sensor_pos;	/* sensor position for loading cal data*/
	int is_dualized[SENSOR_POSITION_MAX];
};

#endif
