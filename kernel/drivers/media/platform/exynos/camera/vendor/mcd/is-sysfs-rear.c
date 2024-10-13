/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>

#include "is-sysfs.h"
#include "is-core.h"
#include "is-sec-define.h"
#include "is-device-sensor-peri.h"
#include "is-vendor-private.h"
#include "is-device-rom.h"
#include "is-sysfs-rear.h"
#include "is-sysfs-hwparam.h"

struct device *camera_rear_dev;

#define SSRM_INFO_PRINT(container, member)						\
	do {										\
		sprintf(temp_buffer, #member"=%d;", (container.member));		\
		strncat(buf, temp_buffer, strlen(temp_buffer));				\
	} while (0)

struct ssrm_camera_data {
	int ID;
	int ON;
	int ISPWidth;
	int ISPHeight;
};

struct ssrm_camera_data_common {
	int previewWidth;
	int previewHeight;
	int minFPS;
	int maxFPS;
	int FPSHint;
	int capture;
	int HDRCapture;
	int MODE;
	int HDR10;
	int zoom;
	int flash;
	int ssteady;
	int mem;
} SsrmCameraInfoExt;

struct ssrm_send_format {
	int operation;
	// per Camera Info
	struct ssrm_camera_data per_camera_info;
	// Common Info
	struct ssrm_camera_data_common common_info;
};

enum ssrm_camerainfo_operation {
	SSRM_CAMERA_INFO_CLEAR,
	SSRM_CAMERA_INFO_SET,
	SSRM_CAMERA_INFO_UPDATE,
};

int ssrmCameraInfoCnt;
struct ssrm_camera_data SsrmCameraInfo[IS_SENSOR_COUNT];

void is_sysfs_hw_init(void)
{
	int i;

	for (i = 0; i < IS_SENSOR_COUNT; i++)
		SsrmCameraInfo[i].ID = -1;
}

static ssize_t rear_af_hall_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_af_hall_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_af_hall);

static ssize_t rear_afcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_afcal_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_afcal);

static ssize_t rear_calcheck_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	char rear_sensor[10] = {0, };
	char front_sensor[10] = {0, };
	int position;
	int rom_id;
	int rom_cal_index;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		info("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		strcpy(rear_sensor, "Null");
	} else {
		is_sec_get_rom_info(&rom_info, rom_id);

		if (is_sec_check_rom_ver(core, position)
			&& !rom_info->other_vendor_module
			&& !rom_info->crc_error)
			strcpy(rear_sensor, "Normal");
		else
			strcpy(rear_sensor, "Abnormal");
	}

	/* front check */
	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		strcpy(front_sensor, "Null");
	} else {
		is_sec_get_rom_info(&rom_info, rom_id);

		if (is_sec_check_rom_ver(core, position)
			&& !rom_info->other_vendor_module
			&& !rom_info->crc_error)
			strcpy(front_sensor, "Normal");
		else
			strcpy(front_sensor, "Abnormal");
	}

	return sprintf(buf, "%s %s\n", rear_sensor, front_sensor);
}
static DEVICE_ATTR_RO(rear_calcheck);

static ssize_t rear_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR, false);
}
static DEVICE_ATTR_RO(rear_camfw);

static ssize_t rear_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR, true);
}
static DEVICE_ATTR_RO(rear_camfw_full);

static ssize_t rear_caminfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_info_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_caminfo);

static ssize_t rear_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_maker[50];
	char sensor_name[50];
	int ret;

	ret = is_get_sensor_data(sensor_maker, sensor_name, SENSOR_POSITION_REAR);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_EXYNOS_IS\n");
	else
		return sprintf(buf, "%s_%s_EXYNOS_IS\n", sensor_maker, sensor_name);
}
static DEVICE_ATTR_RO(rear_camtype);

static ssize_t rear_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_checkfw_factory);

static ssize_t rear_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_user_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_checkfw_user);

static ssize_t rear_dualcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int copy_size = 0;

	copy_size = is_sysfs_get_dualcal_param(dev, &buf, SENSOR_POSITION_REAR);

	return copy_size;
}
static DEVICE_ATTR_RO(rear_dualcal);

static ssize_t rear_eeprom_retry_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_eeprom_retry_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_eeprom_retry);

#if defined(USE_CAMERA_ADAPTIVE_MIPI) && IS_ENABLED(CONFIG_SEC_ABC)
static ssize_t rear_i2c_rfinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_cp_noti_cell_infos cell_infos;
	char print_buf[300] = {0, };
	int print_buf_size = (int)sizeof(print_buf);
	int print_buf_cnt = 0;
	int i;
	struct cam_cp_cell_info *cell_info;

	is_vendor_get_rf_cell_infos_on_error(&cell_infos);

	info("[%s]", __func__);

	for (i = 0; i < cell_infos.num_cell; i++) {
		cell_info = &cell_infos.cell_list[i];
		print_buf_cnt += snprintf(print_buf + print_buf_cnt, print_buf_size - print_buf_cnt,
			 "%d : [%d, %d, %d] \n", i, cell_info->rat, cell_info->band, cell_info->channel);
		info("[%s] %d : rat(%d), band(%d), channel(%d)", __func__,
			i, cell_info->rat, cell_info->band, cell_info->channel);
	}

	sprintf(buf, "%s", print_buf);

	return strlen(buf);
}
static DEVICE_ATTR_RO(rear_i2c_rfinfo);
#endif

static ssize_t rear_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_moduleid);

static ssize_t rear_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_mtf_exif);

static ssize_t rear_paf_cal_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_paf_cal_check_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_paf_cal_check);

/* PAF PAN offset read */
static ssize_t rear_paf_offset_far_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_paf_offset_show(buf, false);
}
static DEVICE_ATTR_RO(rear_paf_offset_far);

/* PAF MID offset read */
static ssize_t rear_paf_offset_mid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_paf_offset_show(buf, true);
}
static DEVICE_ATTR_RO(rear_paf_offset_mid);

static ssize_t rear_phy_tune_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_phy_tune_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_phy_tune);

static ssize_t rear_sensor_standby_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Rear sensor standby\n");
}
static ssize_t rear_sensor_standby_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	switch (buf[0]) {
	case '0':
		break;
	case '1':
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}
static DEVICE_ATTR_RW(rear_sensor_standby);

static ssize_t rear_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(rear_sensorid_exif);

static ssize_t rear2_afcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_afcal_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_afcal);

static ssize_t rear2_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR2, false);
}
static DEVICE_ATTR_RO(rear2_camfw);

static ssize_t rear2_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR2, true);
}
static DEVICE_ATTR_RO(rear2_camfw_full);

static ssize_t rear2_caminfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_info_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_caminfo);

static ssize_t rear2_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_checkfw_factory);

static ssize_t rear2_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_user_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_checkfw_user);

static ssize_t rear2_dualcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int copy_size = 0;

	copy_size = is_sysfs_get_dualcal_param(dev, &buf, SENSOR_POSITION_REAR3);

	return copy_size;
}
static DEVICE_ATTR_RO(rear2_dualcal);

static ssize_t rear2_eeprom_retry_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_eeprom_retry_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_eeprom_retry);

static ssize_t rear2_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_moduleid);

static ssize_t rear2_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_mtf_exif);

static ssize_t rear2_paf_cal_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_paf_cal_check_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_paf_cal_check);

static ssize_t rear2_phy_tune_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_phy_tune_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_phy_tune);

static ssize_t rear2_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_sensorid_exif);

static ssize_t rear2_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_tilt_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(rear2_tilt);

static ssize_t rear3_af_hall_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_af_hall_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_af_hall);

static ssize_t rear3_afcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_afcal_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_afcal);

static ssize_t rear3_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR3, false);
}
static DEVICE_ATTR_RO(rear3_camfw);

static ssize_t rear3_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR3, true);
}
static DEVICE_ATTR_RO(rear3_camfw_full);

static ssize_t rear3_caminfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_info_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_caminfo);

static ssize_t rear3_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_checkfw_factory);

static ssize_t rear3_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_user_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_checkfw_user);

#ifdef CAMERA_REAR3_AFCAL
static ssize_t rear3_dualcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int copy_size = 0;

	copy_size = is_sysfs_get_dualcal_param(dev, &buf, SENSOR_POSITION_REAR2);

	return copy_size;
}
static DEVICE_ATTR_RO(rear3_dualcal);

static ssize_t rear3_dualcal_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int copy_size = 0;

	copy_size = is_sysfs_get_dualcal_param(dev, &buf, SENSOR_POSITION_REAR2);

	return sprintf(buf, "%d", copy_size);
}
static DEVICE_ATTR_RO(rear3_dualcal_size);

static ssize_t rear3_fullsize_cal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int fullsize_cal = 1;

	return sprintf(buf, "%d", fullsize_cal);
}
static DEVICE_ATTR_RO(rear3_fullsize_cal);
#endif

static ssize_t rear3_eeprom_retry_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_eeprom_retry_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_eeprom_retry);

static ssize_t rear3_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_moduleid);

static ssize_t rear3_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_mtf_exif);

static ssize_t rear3_paf_cal_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_paf_cal_check_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_paf_cal_check);

static ssize_t rear3_phy_tune_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_phy_tune_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_phy_tune);

static ssize_t rear3_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_sensorid_exif);

static ssize_t rear3_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_tilt_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(rear3_tilt);

static ssize_t rear4_afcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_afcal_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_afcal);

static ssize_t rear4_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR4, false);
}
static DEVICE_ATTR_RO(rear4_camfw);

static ssize_t rear4_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR4, true);
}
static DEVICE_ATTR_RO(rear4_camfw_full);

static ssize_t rear4_caminfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_info_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_caminfo);


static ssize_t rear4_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_checkfw_factory);


static ssize_t rear4_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_user_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_checkfw_user);

static ssize_t rear4_eeprom_retry_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_eeprom_retry_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_eeprom_retry);

static ssize_t rear4_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_moduleid);

static ssize_t rear4_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_mtf_exif);

static ssize_t rear4_paf_cal_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_paf_cal_check_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_paf_cal_check);

static ssize_t rear4_phy_tune_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_phy_tune_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_phy_tune);

static ssize_t rear4_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_sensorid_exif);

static ssize_t rear4_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_tilt_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(rear4_tilt);

static ssize_t rear_tof_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR_TOF, false);
}
static DEVICE_ATTR_RO(rear_tof_camfw);

static ssize_t rear_tof_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_REAR_TOF, true);
}
static DEVICE_ATTR_RO(rear_tof_camfw_full);

static ssize_t rear_tof_caminfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_info_show(buf, CAM_INFO_REAR_TOF);
}
static DEVICE_ATTR_RO(rear_tof_caminfo);

static ssize_t rear_tof_check_pd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u16 value;
	struct device *is_dev;

	is_dev = is_get_is_dev();

	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	if (camera_tof_get_laser_photo_diode(SENSOR_POSITION_REAR_TOF, &value) < 0)
		return sprintf(buf, "NG\n");

	return sprintf(buf, "%d\n", value);
}
static ssize_t rear_tof_check_pd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret_count;
	u32 value;
	struct device *is_dev;

	is_dev = is_get_is_dev();

	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	ret_count = kstrtouint(buf, 0, &value);

	if (ret_count != 1)
		return -EINVAL;

	camera_tof_set_laser_current(SENSOR_POSITION_REAR_TOF, value);

	return count;
}
static DEVICE_ATTR_RW(rear_tof_check_pd);

static ssize_t rear_tof_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(buf, CAM_INFO_REAR_TOF);
}
static DEVICE_ATTR_RO(rear_tof_checkfw_factory);

static ssize_t rear_tof_dual_cal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	s32 cal_size = 0;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	if (rom_info->rom_dualcal_slave2_size != -1) {
		cal_size = rom_info->rom_dualcal_slave2_size;
		memcpy(buf, &cal_buf[rom_info->rom_dualcal_slave2_start_addr], cal_size);
	}
	return cal_size;

err:
	return 0;
}
static DEVICE_ATTR_RO(rear_tof_dual_cal);

#if defined(USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION) || defined(USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION_SYSFS_ENABLE)
static ssize_t rear_tof_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int value = -1;
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_cis *cis = NULL;
	int i;
	struct device *is_dev;

	is_dev = is_get_is_dev();

	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	core = is_get_is_core();
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];

		is_search_sensor_module_with_position(&core->sensor[i],
				SENSOR_POSITION_REAR_TOF, &module);
		if (module)
			break;
	}

	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	WARN_ON(!sensor_peri);

	if (sensor_peri->subdev_cis) {
		cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);
		CALL_CISOPS(cis, cis_get_tof_tx_freq, sensor_peri->subdev_cis, &value);
	}

	return sprintf(buf, "%d\n", value);
}
static ssize_t rear_tof_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_cis *cis = NULL;
	int i, value, ret_count;
	struct device *is_dev;

	is_dev = is_get_is_dev();

	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	core = is_get_is_core();
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];

		is_search_sensor_module_with_position(&core->sensor[i],
				SENSOR_POSITION_REAR_TOF, &module);
		if (module)
			break;
	}

	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	WARN_ON(!sensor_peri);

	ret_count = kstrtoint(buf, 10, &value);

	if (ret_count != 1)
		return -EINVAL;

	if (sensor_peri->subdev_cis) {
		cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);
		CALL_CISOPS(cis, cis_set_tof_tx_freq, sensor_peri->subdev_cis, value);
	}

	return count;
}
static DEVICE_ATTR_RW(rear_tof_freq);
#endif

static int camera_tof_laser_error_flag(int position, u32 mode, int *value)
{
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_cis *cis = NULL;
	int i;

	core = is_get_is_core();
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		is_search_sensor_module_with_position(&core->sensor[i],
				position, &module);
		if (module)
			break;
	}

	if (!module) {
		err("Could not find sensor id.");
		return -EINVAL;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	WARN_ON(!sensor_peri);

	if (sensor_peri->subdev_cis) {
		cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);
		CALL_CISOPS(cis, cis_get_tof_laser_error_flag, sensor_peri->subdev_cis, mode, value);
	}
	return *value;
}

static ssize_t rear_tof_laser_error_flag_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int value;
	struct device *is_dev;

	is_dev = is_get_is_dev();

	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	if (camera_tof_laser_error_flag(SENSOR_POSITION_REAR_TOF, 0, &value) < 0) {
		return sprintf(buf, "NG\n");
	}

	return sprintf(buf, "%x\n", value);
}
static ssize_t rear_tof_laser_error_flag_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u16 mode;
	int value;
	int ret_count;
	struct device *is_dev;

	is_dev = is_get_is_dev();

	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	ret_count = kstrtou16(buf, 0, &mode);

	if (ret_count != 1) {
		return -EINVAL;
	}

	camera_tof_laser_error_flag(SENSOR_POSITION_REAR_TOF, mode, &value);

	return count;
}
static DEVICE_ATTR_RW(rear_tof_laser_error_flag);

static ssize_t rear_tof_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_REAR_TOF);
}
static DEVICE_ATTR_RO(rear_tof_moduleid);

static ssize_t rear_tof_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_REAR_TOF);
}
static DEVICE_ATTR_RO(rear_tof_sensorid_exif);

static ssize_t rear_tof_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	int i;

	core = is_get_is_core();
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		is_search_sensor_module_with_position(&core->sensor[i],
				SENSOR_POSITION_REAR_TOF, &module);
		if (module)
			break;
	}

	WARN_ON(!module);

	return sprintf(buf, "%d", test_bit(IS_SENSOR_OPEN, &device->state));
}
static DEVICE_ATTR_RO(rear_tof_state);

static ssize_t rear_tof_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_tilt_show(buf, CAM_INFO_REAR_TOF);
}
static DEVICE_ATTR_RO(rear_tof_tilt);

static ssize_t rear_tof_cal_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR_TOF);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	if (cal_buf[rom_info->rom_tof_cal_result_addr] == IS_TOF_CAL_CAL_RESULT_OK         /* CAT TREE */
		&& cal_buf[rom_info->rom_tof_cal_result_addr + 2] == IS_TOF_CAL_CAL_RESULT_OK  /* TRUN TABLE */
		&& cal_buf[rom_info->rom_tof_cal_result_addr + 4] == IS_TOF_CAL_CAL_RESULT_OK) /* VALIDATION */ {
		return sprintf(buf, "OK\n");
	} else {
		return sprintf(buf, "NG\n");
	}

err:
	return sprintf(buf, "NG\n");
}
static DEVICE_ATTR_RO(rear_tof_cal_result);

static ssize_t rear_tof_get_validation_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	u16 val_data[2];

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR_TOF);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	/* 2Byte from 11EE (300mm validation) */
	val_data[0] = *(u16 *)&cal_buf[rom_info->rom_tof_cal_validation_addr[1]];
	/* 2Byte from 11D0 (500mm validation) */
	val_data[1] = *(u16 *)&cal_buf[rom_info->rom_tof_cal_validation_addr[0]];

	return sprintf(buf, "%d,%d", val_data[0]/100, val_data[1]/100);

err:
	return 0;
}
static DEVICE_ATTR_RO(rear_tof_get_validation);

static ssize_t rear_tofcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	s32 cal_size;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR_TOF);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	cal_size = *((s32 *)&cal_buf[rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len-1]]) +
		(rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len - 1]
		- rom_info->rom_tof_cal_start_addr + 4);

	if (cal_size > IS_TOF_CAL_SIZE_ONCE)
		cal_size = IS_TOF_CAL_SIZE_ONCE;

	memcpy(buf, &cal_buf[rom_info->rom_tof_cal_start_addr], cal_size);
	memcpy(buf + cal_size, &cal_buf[rom_info->rom_tof_cal_validation_addr[0]], 2); /* 2byte validation data */
	memcpy(buf + cal_size + 2, &cal_buf[rom_info->rom_tof_cal_validation_addr[1]], 2); /* 2byte validation data */

	cal_size += 4;

	return cal_size;

err:
	return 0;
}
static DEVICE_ATTR_RO(rear_tofcal);

static ssize_t rear_tofcal_extra_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	s32 cal_size;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR_TOF);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	cal_size = *((s32 *)&cal_buf[rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len-1]]) +
		(rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len - 1]
		- rom_info->rom_tof_cal_start_addr + 4);

	cal_size -= IS_TOF_CAL_SIZE_ONCE;

	if (cal_size > IS_TOF_CAL_SIZE_ONCE)
		cal_size = IS_TOF_CAL_SIZE_ONCE;

	memcpy(buf, &cal_buf[rom_info->rom_tof_cal_start_addr + IS_TOF_CAL_SIZE_ONCE], cal_size);
	return cal_size;

err:
	return 0;
}
static DEVICE_ATTR_RO(rear_tofcal_extra);

static ssize_t rear_tofcal_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	s32 cal_size;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR_TOF);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	cal_size = *((s32 *)&cal_buf[rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len-1]]) +
		(rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len - 1]
		- rom_info->rom_tof_cal_start_addr + 4);

	cal_size += 4;	/* 4 bytes for validation data <rom_tof_cal_validation_addr> */

	return sprintf(buf, "%d\n", cal_size);

err:
	return sprintf(buf, "0");
}
static DEVICE_ATTR_RO(rear_tofcal_size);

static ssize_t rear_tofcal_uid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = NULL;
	struct is_vendor_private *vendor_priv;

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;
	dev_info(dev, "%s: E", __func__);

	return sprintf(buf, "%d\n", vendor_priv->rear_tof_mode_id);
}
static DEVICE_ATTR_RO(rear_tofcal_uid);

static ssize_t rear2_tof_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	char *cal_buf;
	int position;
	int rom_dualcal_id;
	int rom_dualcal_index;
	s32 *x = NULL, *y = NULL, *z = NULL, *sx = NULL, *sy = NULL;
	s32 *range = NULL, *max_err = NULL, *avg_err = NULL, *dll_version = NULL;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR_TOF);

	position = cam_info->position;
	is_vendor_get_rom_dualcal_info_from_position(position, &rom_dualcal_id, &rom_dualcal_index);

	if (rom_dualcal_id == ROM_ID_NOTHING) {
		err("%s: invalid ROM ID [%d][%d]", __func__, position, rom_dualcal_id);
		goto err_tilt;
	}

	is_sec_get_rom_info(&rom_info, rom_dualcal_id);
	cal_buf = rom_info->buf;

	if (!is_sec_check_rom_ver(core, rom_dualcal_id)) {
		err(" NG, invalid ROM version");
		goto err_tilt;
	}

	if (rom_info->other_vendor_module || rom_info->crc_error) {
		err(" NG, invalid ROM version");
		goto err_tilt;
	}

	if (rom_info->rom_dualcal_slave3_tilt_list_len > IS_ROM_DUAL_TILT_MAX_LIST) {
		err(" NG, invalid ROM dual tilt value, dualcal_index[%d]", rom_dualcal_index);
		goto err_tilt;
	}

	x = (s32 *)&cal_buf[rom_info->rom_dualcal_slave3_tilt_list[0]];
	y = (s32 *)&cal_buf[rom_info->rom_dualcal_slave3_tilt_list[1]];
	z = (s32 *)&cal_buf[rom_info->rom_dualcal_slave3_tilt_list[2]];
	sx = (s32 *)&cal_buf[rom_info->rom_dualcal_slave3_tilt_list[3]];
	sy = (s32 *)&cal_buf[rom_info->rom_dualcal_slave3_tilt_list[4]];
	range = (s32 *)&cal_buf[rom_info->rom_dualcal_slave3_tilt_list[5]];
	max_err = (s32 *)&cal_buf[rom_info->rom_dualcal_slave3_tilt_list[6]];
	avg_err = (s32 *)&cal_buf[rom_info->rom_dualcal_slave3_tilt_list[7]];
	dll_version = (s32 *)&cal_buf[rom_info->rom_dualcal_slave3_tilt_list[8]];

	return sprintf(buf, "1 %d %d %d %d %d %d %d %d %d\n",
							*x, *y, *z, *sx, *sy, *range, *max_err, *avg_err, *dll_version);
err_tilt:
	return sprintf(buf, "%s\n", "NG");
}
static DEVICE_ATTR_RO(rear2_tof_tilt);

static ssize_t ssrm_camera_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp_buffer[65] = {0, };
	int i = 0, zoomtmp = 0;
	int runningCameraCnt = 0;

	if (ssrmCameraInfoCnt <= 0) {
		SsrmCameraInfoExt.capture = 0;
		SsrmCameraInfoExt.HDRCapture = 0;
		return 0;
	}

	SSRM_INFO_PRINT(SsrmCameraInfoExt, MODE);

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		if (SsrmCameraInfo[i].ID != -1) {
			sprintf(temp_buffer, "ID=%d,%d;", SsrmCameraInfo[i].ID, SsrmCameraInfo[i].ON);
			runningCameraCnt = (SsrmCameraInfo[i].ON) ? (runningCameraCnt+1) : runningCameraCnt;
			strncat(buf, temp_buffer, strlen(temp_buffer));
			if (SsrmCameraInfo[i].ISPWidth && SsrmCameraInfo[i].ISPHeight) {
				sprintf(temp_buffer, "ISP=%d,%d;",
						SsrmCameraInfo[i].ISPWidth, SsrmCameraInfo[i].ISPHeight);
				strncat(buf, temp_buffer, strlen(temp_buffer));
			}
		}
	}

	sprintf(temp_buffer, "[%d];", runningCameraCnt);
	strncat(buf, temp_buffer, strlen(temp_buffer));

	if (SsrmCameraInfoExt.minFPS && SsrmCameraInfoExt.maxFPS) {
		sprintf(temp_buffer, "FPS=%d,%d,%d;",
				SsrmCameraInfoExt.minFPS, SsrmCameraInfoExt.maxFPS, SsrmCameraInfoExt.FPSHint);
		strncat(buf, temp_buffer, strlen(temp_buffer));
	}
	if (SsrmCameraInfoExt.previewWidth && SsrmCameraInfoExt.previewHeight) {
		sprintf(temp_buffer, "SIZE=%d,%d;",
				SsrmCameraInfoExt.previewWidth, SsrmCameraInfoExt.previewHeight);
		strncat(buf, temp_buffer, strlen(temp_buffer));
	}
	sprintf(temp_buffer, "cap=%d,%d;", SsrmCameraInfoExt.capture, SsrmCameraInfoExt.HDRCapture);
	strncat(buf, temp_buffer, strlen(temp_buffer));
	SsrmCameraInfoExt.capture = SsrmCameraInfoExt.HDRCapture = 0;

	if (SsrmCameraInfoExt.zoom >= 1000) {
		zoomtmp = SsrmCameraInfoExt.zoom / 1000;
		sprintf(temp_buffer, "zoom=%dx;", zoomtmp);
	} else {
		zoomtmp = SsrmCameraInfoExt.zoom / 100;
		sprintf(temp_buffer, "zoom=0.%dx;", zoomtmp);
	}
	strncat(buf, temp_buffer, strlen(temp_buffer));

	SSRM_INFO_PRINT(SsrmCameraInfoExt, ssteady);
	SSRM_INFO_PRINT(SsrmCameraInfoExt, HDR10);
	SSRM_INFO_PRINT(SsrmCameraInfoExt, flash);
	SSRM_INFO_PRINT(SsrmCameraInfoExt, mem);
	strncat(buf, "\n", strlen("\n"));

	return strlen(buf);
}
static ssize_t ssrm_camera_info_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ssrm_send_format recv_data;
	struct ssrm_camera_data *per_camera_info;
	struct ssrm_camera_data_common *common_info;
	int index = -1;
	int i = 0, capturetmp[2] = {SsrmCameraInfoExt.capture, SsrmCameraInfoExt.HDRCapture};

	if (count != sizeof(struct ssrm_send_format)) {
		err("err not matched format");
		return -EINVAL;
	}
	memcpy(&recv_data, buf, sizeof(struct ssrm_send_format));

	per_camera_info = &recv_data.per_camera_info;
	common_info = &recv_data.common_info;
	common_info->capture += capturetmp[0];
	common_info->HDRCapture += capturetmp[1];

	switch (recv_data.operation) {
	case SSRM_CAMERA_INFO_CLEAR:
		for (i = 0; i < IS_SENSOR_COUNT; i++) { /* clear */
			if (SsrmCameraInfo[i].ID == per_camera_info->ID) {
				SsrmCameraInfo[i].ID = -1;
				ssrmCameraInfoCnt--;
			}
		}
		break;

	case SSRM_CAMERA_INFO_SET:
		for (i = 0; i < IS_SENSOR_COUNT; i++) { /* find empty space*/
			if (SsrmCameraInfo[i].ID == -1) {
				index = i;
				break;
			}
		}

		if (index == -1)
			return -EPERM;

		memcpy(&SsrmCameraInfo[i], per_camera_info, sizeof(struct ssrm_camera_data));
		memcpy(&SsrmCameraInfoExt, common_info, sizeof(struct ssrm_camera_data_common));
		ssrmCameraInfoCnt++;
		break;

	case SSRM_CAMERA_INFO_UPDATE:
		for (i = 0; i < IS_SENSOR_COUNT; i++) {
			if (SsrmCameraInfo[i].ID == per_camera_info->ID) {
				index = i;
				break;
			}
		}
		if (index == -1)
			return -EPERM;

		memcpy(&SsrmCameraInfo[i], per_camera_info, sizeof(struct ssrm_camera_data));
		memcpy(&SsrmCameraInfoExt, common_info, sizeof(struct ssrm_camera_data_common));
		break;
	default:
		break;
	}

	return count;
}
static DEVICE_ATTR_RW(ssrm_camera_info);

static ssize_t supported_cameraIds_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char temp_buf[IS_SENSOR_COUNT];
	char *end = "\n";
	int i;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	for (i = 0; i < vendor_priv->max_supported_camera; i++)	{
		sprintf(temp_buf, "%d ", vendor_priv->supported_camera_ids[i]);
		strncat(buf, temp_buf, strlen(temp_buf));
	}
	strncat(buf, end, strlen(end));

	return strlen(buf);
}
static DEVICE_ATTR_RO(supported_cameraIds);

static struct attribute *is_sysfs_attr_entries_rear[] = {
	&dev_attr_rear_af_hall.attr,
	&dev_attr_rear_afcal.attr,
	&dev_attr_rear_calcheck.attr,
	&dev_attr_rear_camfw.attr,
	&dev_attr_rear_camfw_full.attr,
	&dev_attr_rear_caminfo.attr,
	&dev_attr_rear_camtype.attr,
	&dev_attr_rear_checkfw_factory.attr,
	&dev_attr_rear_checkfw_user.attr,
	&dev_attr_rear_moduleid.attr,
	&dev_attr_rear_mtf_exif.attr,
	&dev_attr_rear_paf_cal_check.attr,
	&dev_attr_rear_paf_offset_far.attr,
	&dev_attr_rear_paf_offset_mid.attr,
	&dev_attr_rear_phy_tune.attr,
	&dev_attr_rear_sensor_standby.attr,
	&dev_attr_rear_sensorid_exif.attr,
	&dev_attr_ssrm_camera_info.attr,
	&dev_attr_supported_cameraIds.attr,
	NULL,
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct attribute **pablo_vendor_sysfs_get_rear_attr_tbl(void)
{
	return is_sysfs_attr_entries_rear;
}
KUNIT_EXPORT_SYMBOL(pablo_vendor_sysfs_get_rear_attr_tbl);
#endif

static const struct attribute_group is_sysfs_attr_group_rear = {
	.attrs	= is_sysfs_attr_entries_rear,
};

static struct attribute *camera_rear2_attrs[] = {
	&dev_attr_rear2_camfw.attr,
	&dev_attr_rear2_camfw_full.attr,
	&dev_attr_rear2_caminfo.attr,
	&dev_attr_rear2_checkfw_factory.attr,
	&dev_attr_rear2_checkfw_user.attr,
	&dev_attr_rear2_mtf_exif.attr,
	&dev_attr_rear2_phy_tune.attr,
	&dev_attr_rear2_sensorid_exif.attr,
	NULL,
};

static const struct attribute_group is_sysfs_attr_group_rear2 = {
	.attrs	= camera_rear2_attrs,
};

static struct attribute *camera_rear3_attrs[] = {
	&dev_attr_rear3_camfw.attr,
	&dev_attr_rear3_camfw_full.attr,
	&dev_attr_rear3_caminfo.attr,
	&dev_attr_rear3_checkfw_factory.attr,
	&dev_attr_rear3_checkfw_user.attr,
	&dev_attr_rear3_mtf_exif.attr,
	&dev_attr_rear3_phy_tune.attr,
	&dev_attr_rear3_sensorid_exif.attr,
	NULL,
};

static const struct attribute_group is_sysfs_attr_group_rear3 = {
	.attrs	= camera_rear3_attrs,
};

static struct attribute *camera_rear4_attrs[] = {

	&dev_attr_rear4_camfw.attr,
	&dev_attr_rear4_camfw_full.attr,
	&dev_attr_rear4_caminfo.attr,
	&dev_attr_rear4_checkfw_factory.attr,
	&dev_attr_rear4_checkfw_user.attr,

	&dev_attr_rear4_mtf_exif.attr,

	&dev_attr_rear4_phy_tune.attr,
	&dev_attr_rear4_sensorid_exif.attr,
	NULL,
};

static const struct attribute_group is_sysfs_attr_group_rear4 = {
	.attrs	= camera_rear4_attrs,
};

static struct attribute *camera_rear_tof_attrs[] = {
	&dev_attr_rear_tof_camfw.attr,
	&dev_attr_rear_tof_camfw_full.attr,
	&dev_attr_rear_tof_caminfo.attr,
	&dev_attr_rear_tof_check_pd.attr,
	&dev_attr_rear_tof_checkfw_factory.attr,
	&dev_attr_rear_tof_laser_error_flag.attr,
	&dev_attr_rear_tof_sensorid_exif.attr,
	&dev_attr_rear_tof_state.attr,
	NULL,
};

static const struct attribute_group is_sysfs_attr_group_rear_tof = {
	.attrs	= camera_rear_tof_attrs,
};

int is_create_rear_sysfs(struct class *camera_class)
{
	int ret = 0;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	struct is_sysfs_config *sysfs_config = &vendor_priv->sysfs_config;

	camera_rear_dev = device_create(camera_class, NULL, 2, NULL, "rear");
	if (IS_ERR(camera_rear_dev))
		pr_err("failed to create rear device!\n");

	ret = devm_device_add_group(camera_rear_dev, &is_sysfs_attr_group_rear);
	if (ret < 0) {
		pr_err("failed to add device group %s\n", is_sysfs_attr_group_rear.name);
		return ret;
	}

	if (sysfs_config->rear_dualcal) {
		ret = device_create_file(camera_rear_dev, &dev_attr_rear_dualcal);
		if (ret != 0)
			pr_err("failed to create device file %s\n", dev_attr_rear_dualcal.attr.name);
	}

	ret = device_create_file(camera_rear_dev, &dev_attr_rear_eeprom_retry);
	if (ret != 0)
		pr_err("failed to create device file %s\n", dev_attr_rear_eeprom_retry.attr.name);

#if defined(USE_CAMERA_ADAPTIVE_MIPI) && IS_ENABLED(CONFIG_SEC_ABC)
	ret = device_create_file(camera_rear_dev, &dev_attr_rear_i2c_rfinfo);
	if (ret != 0)
		pr_err("failed to create device file %s\n", dev_attr_rear_i2c_rfinfo.attr.name);
#endif

	if (sysfs_config->rear2) {
		ret = devm_device_add_group(camera_rear_dev, &is_sysfs_attr_group_rear2);
		if (ret < 0) {
			pr_err("failed to add device group %s\n", is_sysfs_attr_group_rear2.name);
			return ret;
		}
		if (sysfs_config->rear2_afcal) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear2_afcal);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear2_afcal.attr.name);
		}
		if (sysfs_config->rear_dualcal) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear2_dualcal);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear2_dualcal.attr.name);
		}

		ret = device_create_file(camera_rear_dev, &dev_attr_rear2_eeprom_retry);
		if (ret != 0)
			pr_err("failed to create device file %s\n", dev_attr_rear2_eeprom_retry.attr.name);

		if (sysfs_config->rear2_moduleid) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear2_moduleid);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear2_moduleid.attr.name);
		}

		if (sysfs_config->rear2_afcal) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear2_paf_cal_check);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear2_paf_cal_check.attr.name);
		}

		if (sysfs_config->rear2_tilt) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear2_tilt);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear2_tilt.attr.name);
		}
	}

	if (sysfs_config->rear3) {
		ret = devm_device_add_group(camera_rear_dev, &is_sysfs_attr_group_rear3);
		if (ret < 0) {
			pr_err("failed to add device group %s\n", is_sysfs_attr_group_rear3.name);
			return ret;
		}

		if (sysfs_config->rear3_afhall) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear3_af_hall);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear3_af_hall.attr.name);
		}

		if (sysfs_config->rear3_afcal) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear3_afcal);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear3_afcal.attr.name);
		}

		if (sysfs_config->rear_dualcal) {
#ifdef CAMERA_REAR3_AFCAL
			ret = device_create_file(camera_rear_dev, &dev_attr_rear3_dualcal);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear3_dualcal.attr.name);
			ret = device_create_file(camera_rear_dev, &dev_attr_rear3_dualcal_size);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear3_dualcal_size.attr.name);
			ret = device_create_file(camera_rear_dev, &dev_attr_rear3_fullsize_cal);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear3_fullsize_cal.attr.name);
#endif
		}

		ret = device_create_file(camera_rear_dev, &dev_attr_rear3_eeprom_retry);
		if (ret != 0)
			pr_err("failed to create device file %s\n", dev_attr_rear3_eeprom_retry.attr.name);

		if (sysfs_config->rear3_moduleid) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear3_moduleid);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear3_moduleid.attr.name);

		}

		if (sysfs_config->rear3_afcal) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear3_paf_cal_check);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear3_paf_cal_check.attr.name);
		}

		if (sysfs_config->rear3_tilt) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear3_tilt);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear3_tilt.attr.name);
		}
	}

	if (sysfs_config->rear4) {
		ret = devm_device_add_group(camera_rear_dev, &is_sysfs_attr_group_rear4);
		if (ret < 0) {
			pr_err("failed to add device group %s\n", is_sysfs_attr_group_rear4.name);
			return ret;
		}

		if (sysfs_config->rear4_afcal) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear4_afcal);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear4_afcal.attr.name);
		}

		ret = device_create_file(camera_rear_dev, &dev_attr_rear4_eeprom_retry);
		if (ret)
			pr_err("failed to create device file %s\n", dev_attr_rear4_eeprom_retry.attr.name);

		if (sysfs_config->rear4_moduleid) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear4_moduleid);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear4_moduleid.attr.name);
		}

		if (sysfs_config->rear4_afcal) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear4_paf_cal_check);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear4_paf_cal_check.attr.name);
		}

		if (sysfs_config->rear4_tilt) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear4_tilt);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear4_tilt.attr.name);
		}
	}

	if (sysfs_config->rear_tof) {
		ret = devm_device_add_group(camera_rear_dev, &is_sysfs_attr_group_rear_tof);
		if (ret < 0) {
			pr_err("failed to add device group %s\n", is_sysfs_attr_group_rear_tof.name);
			return ret;
		}

		if (sysfs_config->rear_tof_dualcal) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear_tof_dual_cal);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear_tof_dual_cal.attr.name);
		}

#if defined(USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION) || defined(USE_CAMERA_REAR_TOF_TX_FREQ_VARIATION_SYSFS_ENABLE)
		ret = device_create_file(camera_rear_dev, &dev_attr_rear_tof_freq);
		if (ret != 0)
			pr_err("failed to create device file %s\n", dev_attr_rear_tof_freq.attr.name);
#endif

		if (sysfs_config->rear_tof_moduleid) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear_tof_moduleid);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear_tof_moduleid.attr.name);
		}

		if (sysfs_config->rear_tof_tilt) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear_tof_tilt);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear_tof_tilt.attr.name);
		}

		if (sysfs_config->rear_tof_cal) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear_tof_cal_result);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear_tof_cal_result.attr.name);

			ret = device_create_file(camera_rear_dev, &dev_attr_rear_tof_get_validation);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear_tof_get_validation.attr.name);

			ret = device_create_file(camera_rear_dev, &dev_attr_rear_tofcal);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear_tofcal.attr.name);

			ret = device_create_file(camera_rear_dev, &dev_attr_rear_tofcal_extra);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear_tofcal_extra.attr.name);

			ret = device_create_file(camera_rear_dev, &dev_attr_rear_tofcal_size);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear_tofcal_size.attr.name);

			ret = device_create_file(camera_rear_dev, &dev_attr_rear_tofcal_uid);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear_tofcal_uid.attr.name);
		}

		if (sysfs_config->rear2_tof_tilt) {
			ret = device_create_file(camera_rear_dev, &dev_attr_rear2_tof_tilt);
			if (ret != 0)
				pr_err("failed to create device file %s\n", dev_attr_rear2_tof_tilt.attr.name);
		}
	}

#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
	ret = is_add_rear_hwparam_sysfs(camera_rear_dev);
	if (ret != 0)
		return ret;
#endif

	return ret;
}

void is_destroy_rear_sysfs(struct class *camera_class)
{
	struct is_sysfs_config *sysfs_config = is_vendor_get_sysfs_config();

	devm_device_remove_group(camera_rear_dev, &is_sysfs_attr_group_rear);
	if (sysfs_config->rear2)
		devm_device_remove_group(camera_rear_dev, &is_sysfs_attr_group_rear2);
	if (sysfs_config->rear3)
		devm_device_remove_group(camera_rear_dev, &is_sysfs_attr_group_rear3);
	if (sysfs_config->rear4)
		devm_device_remove_group(camera_rear_dev, &is_sysfs_attr_group_rear4);
	if (sysfs_config->rear_tof)
		devm_device_remove_group(camera_rear_dev, &is_sysfs_attr_group_rear_tof);

	if (camera_class && camera_rear_dev)
		device_destroy(camera_class, camera_rear_dev->devt);
}
