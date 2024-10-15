/* linux/arch/arm/mach-exynos/include/mach/fimc-is-sysfs.h
 *
 * Copyright (C) 2011 Samsung Electronics, Co. Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IS_SYSFS_H_
#define _IS_SYSFS_H_

#include "is-core.h"

enum is_cam_info_index {
	CAM_INFO_REAR = 0,
	CAM_INFO_FRONT,
	CAM_INFO_REAR2,
	CAM_INFO_FRONT2,
	CAM_INFO_REAR3,
	CAM_INFO_FRONT3,
	CAM_INFO_REAR4,
	CAM_INFO_FRONT4,
	CAM_INFO_REAR_TOF,
	CAM_INFO_FRONT_TOF,
	CAM_INFO_IRIS,
	CAM_INFO_MAX
};

enum is_cam_info_type {
	CAM_INFO_TYPE_RW1 = 0,
	CAM_INFO_TYPE_RS1,
	CAM_INFO_TYPE_RT1,
	CAM_INFO_TYPE_FW1,
	CAM_INFO_TYPE_RT2,
	CAM_INFO_TYPE_UW1,
	CAM_INFO_TYPE_RM1,
	CAM_INFO_TYPE_RB1,
	CAM_INFO_TYPE_FS1,
	CAM_INFO_TYPE_MAX,
};

struct is_cam_info {
	unsigned int position;
	unsigned int valid;
	unsigned int type;
};

struct is_sysfs_config {
	bool rear_afcal;
	bool rear_dualcal;

	bool rear2;
	bool rear2_afcal;
	bool rear2_moduleid;
	bool rear2_tilt;

	bool rear3;
	bool rear3_afcal;
	bool rear3_afhall;
	bool rear3_moduleid;
	bool rear3_tilt;

	bool rear4;
	bool rear4_afcal;
	bool rear4_moduleid;
	bool rear4_tilt;

	bool rear_tof;
	bool rear_tof_cal;
	bool rear_tof_dualcal;
	bool rear_tof_moduleid;
	bool rear_tof_tilt;

	bool rear2_tof_tilt;

	bool front_afcal;
	bool front_dualcal;
	bool front_fixed_focus;

	bool front2;
	bool front2_moduleid;
	bool front2_tilt;

	bool front_tof;
	bool front_tof_cal;
	bool front_tof_moduleid;
	bool front_tof_tilt;
	bool front_tof_tx_freq_variation;
};

int is_create_sysfs(void);
struct class *is_get_camera_class(void);
struct is_sysfs_config *is_vendor_get_sysfs_config(void);
int is_get_cam_info_from_index(struct is_cam_info **caminfo, enum is_cam_info_index cam_index);
int is_sysfs_get_dualcal_param(struct device *dev, char **buf, int position);
int is_get_sensor_data(char *maker, char *name, int position);
int camera_tof_get_laser_photo_diode(int position, u16 *value);
int camera_tof_set_laser_current(int position, u32 value);

ssize_t camera_af_hall_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_afcal_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_camfw_show(char *buf, enum is_cam_info_index cam_index, bool camfw_full);
ssize_t camera_info_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_checkfw_factory_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_checkfw_user_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_eeprom_retry_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_moduleid_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_sensor_type_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_mtf_exif_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_paf_cal_check_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_paf_offset_show(char *buf, bool is_mid);
ssize_t camera_phy_tune_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_sensorid_show(char *buf, enum is_cam_info_index cam_index);
ssize_t camera_tilt_show(char *buf, enum is_cam_info_index cam_index);
#endif /* _IS_SYSFS_H_ */
