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

enum hw_param_id_type {
	HW_PARAM_REAR = 0,
	HW_PARAM_FRONT,
	HW_PARAM_REAR2,
	HW_PARAM_REAR3,
	HW_PARAM_REAR4,
	HW_PARAM_FRONT2,
	HW_PARAM_FRONT3,
	HW_PARAM_REAR_TOF,
	HW_PARAM_FRONT_TOF,
	MAX_HW_PARAM_ID,
};

char *hwparam_str[MAX_HW_PARAM_ID] = {
	"R",
	"F",
	"R2",
	"R3",
	"R4",
	"F2",
	"F3",
	"R4", /*REAR_TOF*/
	"F2", /*FRONT_TOF*/
};

char wifi_info[128] = "\0";

ssize_t camera_hw_param_show(struct cam_hw_param *ec_param, struct is_vendor_rom *rom_info,
		int hw_param_id, char *buf)
{
	int ret = 0;
	char msg1[15], msg2[20], msg3[50], msg4[40], msg5[40], msg6[170], msg7[50];
	char af_info[30] = { 0, };

	sprintf(msg1, "\"CAMI%s_ID\":", hwparam_str[hw_param_id]);

	if (is_sec_is_valid_moduleid(rom_info->rom_module_id)) {
		sprintf(msg2, "\"%c%c%c%c%cXX%02X%02X%02X\",",
			rom_info->rom_module_id[0], rom_info->rom_module_id[1],
			rom_info->rom_module_id[2], rom_info->rom_module_id[3],
			rom_info->rom_module_id[4], rom_info->rom_module_id[7],
			rom_info->rom_module_id[8], rom_info->rom_module_id[9]);
	} else {
		sprintf(msg2, "\"MIR_ERR\",");
	}

	sprintf(msg3, "\"I2C%s_AF\":\"%d\",\"I2C%s_OIS\":\"%d\",\"I2C%s_SEN\":\"%d\",",
		hwparam_str[hw_param_id], ec_param->i2c_af_err_cnt,
		hwparam_str[hw_param_id], ec_param->i2c_ois_err_cnt,
		hwparam_str[hw_param_id], ec_param->i2c_sensor_err_cnt);

	sprintf(msg4, "\"MIPI%s_SEN\":\"%d\",\"MIPI%s_INFO\":\"%d,%d,%d\",",
		hwparam_str[hw_param_id], ec_param->mipi_sensor_err_cnt,
		hwparam_str[hw_param_id], ec_param->mipi_info_on_err.rat,
		ec_param->mipi_info_on_err.band, ec_param->mipi_info_on_err.channel);

	sprintf(msg5, "\"I2C%s_EEP\":\"%d\",\"CRC%s_EEP\":\"%d\",",
		hwparam_str[hw_param_id], ec_param->eeprom_i2c_err_cnt,
		hwparam_str[hw_param_id], ec_param->eeprom_crc_err_cnt);

	sprintf(msg6, "\"CAM%s_CNT\":\"%d\",\"WIFI%s_INFO\":\"%s\",",
		hwparam_str[hw_param_id], ec_param->cam_entrance_cnt,
		hwparam_str[hw_param_id], wifi_info);

	sprintf(af_info, "%d, %d", ec_param->af_pos_info_on_fail.position,
		ec_param->af_pos_info_on_fail.hall_value);

	sprintf(msg7, "\"AF%s_FAIL\":\"%d\",\"AF%s_INFO\":\"%s\"\n",
		hwparam_str[hw_param_id], ec_param->af_fail_cnt,
		hwparam_str[hw_param_id], af_info);

	ret = sprintf(buf, "%s%s%s%s%s%s%s", msg1, msg2, msg3, msg4, msg5, msg6, msg7);

	return ret;
}

ssize_t rear_hwparam_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR);
	position = cam_info->position;

	is_sec_get_rom_info(&rom_info, is_vendor_get_rom_id_from_position(position));
	if (!rom_info) {
		err("wrong position (%d)", position);
		return 0;
	}

	is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("Fail to get ec_param (%d)", position);
		return 0;
	}

	return camera_hw_param_show(ec_param, rom_info, HW_PARAM_REAR, buf);
}
ssize_t rear_hwparam_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR);
	position = cam_info->position;

	if (!strncmp(buf, "c", 1)) {
		is_sec_get_hw_param(&ec_param, position);

		if (ec_param)
			is_sec_init_err_cnt(ec_param);
	}

	return count;
}
static DEVICE_ATTR_RW(rear_hwparam);

ssize_t rear2_hwparam_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR2);
	position = cam_info->position;

	is_sec_get_rom_info(&rom_info, is_vendor_get_rom_id_from_position(position));
	if (!rom_info) {
		err("wrong position (%d)", position);
		return 0;
	}

	is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("Fail to get ec_param (%d)", position);
		return 0;
	}

	return camera_hw_param_show(ec_param, rom_info, HW_PARAM_REAR2, buf);
}
ssize_t rear2_hwparam_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR2);
	position = cam_info->position;

	if (!strncmp(buf, "c", 1)) {
		is_sec_get_hw_param(&ec_param, position);

		if (ec_param)
			is_sec_init_err_cnt(ec_param);
	}

	return count;
}
static DEVICE_ATTR_RW(rear2_hwparam);

ssize_t rear3_hwparam_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR3);
	position = cam_info->position;

	is_sec_get_rom_info(&rom_info, is_vendor_get_rom_id_from_position(position));
	if (!rom_info) {
		err("wrong position (%d)", position);
		return 0;
	}

	is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("Fail to get ec_param (%d)", position);
		return 0;
	}

	return camera_hw_param_show(ec_param, rom_info, HW_PARAM_REAR3, buf);
}
ssize_t rear3_hwparam_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR3);
	position = cam_info->position;

	if (!strncmp(buf, "c", 1)) {
		is_sec_get_hw_param(&ec_param, position);

		if (ec_param)
			is_sec_init_err_cnt(ec_param);
	}

	return count;
}
static DEVICE_ATTR_RW(rear3_hwparam);

ssize_t rear4_hwparam_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR4);
	position = cam_info->position;

	is_sec_get_rom_info(&rom_info, is_vendor_get_rom_id_from_position(position));
	if (!rom_info) {
		err("wrong position (%d)", position);
		return 0;
	}

	is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("Fail to get ec_param (%d)", position);
		return 0;
	}

	return camera_hw_param_show(ec_param, rom_info, HW_PARAM_REAR4, buf);
}
ssize_t rear4_hwparam_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR4);
	position = cam_info->position;

	if (!strncmp(buf, "c", 1)) {
		is_sec_get_hw_param(&ec_param, position);

		if (ec_param)
			is_sec_init_err_cnt(ec_param);
	}

	return count;
}
static DEVICE_ATTR_RW(rear4_hwparam);

ssize_t cam_wifi_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = 0;

	info("[%s] wifi_info : %s\n", __func__, wifi_info);
	rc = scnprintf(buf, sizeof(wifi_info), "%s", wifi_info);
	if (rc)
		return rc;
	return 0;
}
ssize_t cam_wifi_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	info("[%s] buf : %s\n", __func__, buf);
	scnprintf(wifi_info, sizeof(wifi_info), "%s", buf);

	return size;
}
static DEVICE_ATTR_RW(cam_wifi_info);

int is_add_rear_hwparam_sysfs(struct device *dev)
{
	int ret = 0;
	struct is_sysfs_config *sysfs_config = is_vendor_get_sysfs_config();

	ret = device_create_file(dev, &dev_attr_rear_hwparam);
	if (ret)
		pr_err("failed to create device file %s\n", dev_attr_rear_hwparam.attr.name);
	if (sysfs_config->rear2) {
		ret = device_create_file(dev, &dev_attr_rear2_hwparam);
		if (ret)
			pr_err("failed to create device file %s\n", dev_attr_rear2_hwparam.attr.name);
	}
	if (sysfs_config->rear3) {
		ret = device_create_file(dev, &dev_attr_rear3_hwparam);
		if (ret)
			pr_err("failed to create device file %s\n", dev_attr_rear3_hwparam.attr.name);
	}
	if (sysfs_config->rear4) {
		ret = device_create_file(dev, &dev_attr_rear4_hwparam);
		if (ret)
			pr_err("failed to create device file %s\n", dev_attr_rear4_hwparam.attr.name);
	}
	ret = device_create_file(dev, &dev_attr_cam_wifi_info);
	if (ret)
		pr_err("failed to create device file %s\n", dev_attr_cam_wifi_info.attr.name);

	return ret;
}

ssize_t front_hwparam_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT);
	position = cam_info->position;

	is_sec_get_rom_info(&rom_info, is_vendor_get_rom_id_from_position(position));
	if (!rom_info) {
		err("wrong position (%d)", position);
		return 0;
	}

	is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("Fail to get ec_param (%d)", position);
		return 0;
	}

	return camera_hw_param_show(ec_param, rom_info, HW_PARAM_FRONT, buf);
}
ssize_t front_hwparam_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT);
	position = cam_info->position;

	if (!strncmp(buf, "c", 1)) {
		is_sec_get_hw_param(&ec_param, position);

		if (ec_param)
			is_sec_init_err_cnt(ec_param);
	}

	return count;
}
static DEVICE_ATTR_RW(front_hwparam);

ssize_t front2_hwparam_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT2);
	position = cam_info->position;

	is_sec_get_rom_info(&rom_info, is_vendor_get_rom_id_from_position(position));
	if (!rom_info) {
		err("wrong position (%d)", position);
		return 0;
	}

	is_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
		err("Fail to get ec_param (%d)", position);
		return 0;
	}

	return camera_hw_param_show(ec_param, rom_info, HW_PARAM_FRONT2, buf);
}
ssize_t front2_hwparam_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;
	struct is_cam_info *cam_info;
	int position;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT2);
	position = cam_info->position;

	if (!strncmp(buf, "c", 1)) {
		is_sec_get_hw_param(&ec_param, position);

		if (ec_param)
			is_sec_init_err_cnt(ec_param);
	}

	return count;
}
static DEVICE_ATTR_RW(front2_hwparam);

int is_add_front_hwparam_sysfs(struct device *dev)
{
	int ret = 0;
	struct is_sysfs_config *sysfs_config = is_vendor_get_sysfs_config();

	ret = device_create_file(dev, &dev_attr_front_hwparam);
	if (ret)
		pr_err("failed to create device file %s\n", dev_attr_front_hwparam.attr.name);

	if (sysfs_config->front2) {
		ret = device_create_file(dev, &dev_attr_front2_hwparam);
		if (ret)
			pr_err("failed to create device file %s\n", dev_attr_front2_hwparam.attr.name);
	}

	return ret;
}
