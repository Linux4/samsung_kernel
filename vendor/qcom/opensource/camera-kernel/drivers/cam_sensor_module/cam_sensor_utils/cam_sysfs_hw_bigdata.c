/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_sensor_cmn_header.h"
#include "cam_hw_bigdata.h"

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
extern char hwparam_str[MAX_HW_PARAM_ID][MAX_HW_PARAM_INFO][MAX_HW_PARAM_STR_LEN];
char wifi_info[128] = "\0";

int16_t is_hw_param_valid_module_id(char *moduleid)
{
	int i = 0;
	int32_t moduleid_cnt = 0;
	int16_t rc = MODULE_ID_VALID;

	if (moduleid == NULL) {
		CAM_ERR(CAM_UTIL, "MI_INVALID\n");
		return MODULE_ID_INVALID;
	}

	for (i = 0; i < FROM_MODULE_ID_SIZE; i++) {
		if (moduleid[i] == '\0') {
			moduleid_cnt = moduleid_cnt + 1;
		} else if ((i < 5)
				&& (!((moduleid[i] >= '0' && moduleid[i] <= '9')
						|| (moduleid[i] >= 'A' && moduleid[i] <= 'Z')))) {
			CAM_ERR(CAM_UTIL, "MIR_ERR_1\n");
			rc = MODULE_ID_ERR_CHAR;
			break;
		}
	}

	if (moduleid_cnt == FROM_MODULE_ID_SIZE) {
		CAM_ERR(CAM_UTIL, "MIR_ERR_0\n");
		rc = MODULE_ID_ERR_CNT_MAX;
	}

	return rc;
}

void camera_hw_param_check_avail_cam(void)
{
	struct cam_hw_param *hw_param = NULL;

	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR);
	hw_param->cam_available = 1;
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_FRONT);
	hw_param->cam_available = 1;

#if defined(CONFIG_SAMSUNG_REAR_DUAL)
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR2);
	hw_param->cam_available = 1;
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR3);
	hw_param->cam_available = 1;
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_REAR4);
	hw_param->cam_available = 1;
#endif
#endif
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	hw_bigdata_get_hw_param_static(&hw_param, HW_PARAM_FRONT2);
	hw_param->cam_available = 1;
#endif
}

ssize_t fill_hw_bigdata_sysfs_node(char *buf, struct cam_hw_param *ec_param, char *moduleid, uint32_t hw_param_id)
{
	if (is_hw_param_valid_module_id(moduleid) > 0) {
		return scnprintf(buf, PAGE_SIZE, "\"%s\":\"%c%c%c%c%cXX%02X%02X%02X\",\"%s\":\"%d\",\"%s\":\"%d\","
			"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d,%d,%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%s\"\n",
			hwparam_str[hw_param_id][CAMI_ID], moduleid[0], moduleid[1], moduleid[2], moduleid[3],
			moduleid[4], moduleid[7], moduleid[8], moduleid[9],
			hwparam_str[hw_param_id][I2C_AF], ec_param->err_cnt[I2C_AF_ERROR],
			hwparam_str[hw_param_id][I2C_OIS], ec_param->err_cnt[I2C_OIS_ERROR],
			hwparam_str[hw_param_id][I2C_SEN], ec_param->err_cnt[I2C_SENSOR_ERROR],
			hwparam_str[hw_param_id][MIPI_SEN], ec_param->err_cnt[MIPI_SENSOR_ERROR],
			hwparam_str[hw_param_id][MIPI_INFO], ec_param->rf_rat, ec_param->rf_band, ec_param->rf_channel,
			hwparam_str[hw_param_id][I2C_EEPROM], ec_param->err_cnt[I2C_EEPROM_ERROR],
			hwparam_str[hw_param_id][CRC_EEPROM], ec_param->err_cnt[CRC_EEPROM_ERROR],
			hwparam_str[hw_param_id][CAM_USE_CNT], ec_param->cam_entrance_cnt,
			hwparam_str[hw_param_id][WIFI_INFO], wifi_info);
	} else {
		return scnprintf(buf, PAGE_SIZE, "\"%s\":\"%s\",\"%s\":\"%d\",\"%s\":\"%d\","
			"\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d,%d,%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%d\",\"%s\":\"%s\"\n",
			hwparam_str[hw_param_id][CAMI_ID], ((is_hw_param_valid_module_id(moduleid) == MODULE_ID_ERR_CHAR) ? "MIR_ERR" : "MI_NO"),
			hwparam_str[hw_param_id][I2C_AF], ec_param->err_cnt[I2C_AF_ERROR],
			hwparam_str[hw_param_id][I2C_OIS], ec_param->err_cnt[I2C_OIS_ERROR],
			hwparam_str[hw_param_id][I2C_SEN], ec_param->err_cnt[I2C_SENSOR_ERROR],
			hwparam_str[hw_param_id][MIPI_SEN], ec_param->err_cnt[MIPI_SENSOR_ERROR],
			hwparam_str[hw_param_id][MIPI_INFO], ec_param->rf_rat, ec_param->rf_band, ec_param->rf_channel,
			hwparam_str[hw_param_id][I2C_EEPROM], ec_param->err_cnt[I2C_EEPROM_ERROR],
			hwparam_str[hw_param_id][CRC_EEPROM], ec_param->err_cnt[CRC_EEPROM_ERROR],
			hwparam_str[hw_param_id][CAM_USE_CNT], ec_param->cam_entrance_cnt,
			hwparam_str[hw_param_id][WIFI_INFO], wifi_info);
	}
}

static ssize_t rear_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct cam_hw_param *ec_param = NULL;

	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, module_id[EEP_REAR], HW_PARAM_REAR);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CAM_DBG(CAM_UTIL, "[R] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

static ssize_t front_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	struct cam_hw_param *ec_param = NULL;

	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, module_id[EEP_FRONT], HW_PARAM_FRONT);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CAM_DBG(CAM_UTIL, "[F] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

#if defined(CONFIG_SAMSUNG_REAR_DUAL)
static ssize_t rear2_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	struct cam_hw_param *ec_param = NULL;

	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR2);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, module_id[EEP_REAR2], HW_PARAM_REAR2);
	}

	return rc;
}

static ssize_t rear2_camera_hw_param_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CAM_DBG(CAM_UTIL, "[R2] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR2);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
static ssize_t rear3_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	struct cam_hw_param *ec_param = NULL;

	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR3);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, module_id[EEP_REAR3], HW_PARAM_REAR3);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CAM_DBG(CAM_UTIL, "[R3] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR3);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
static ssize_t rear4_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	struct cam_hw_param *ec_param = NULL;

	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR4);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, module_id[EEP_REAR4], HW_PARAM_REAR4);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CAM_DBG(CAM_UTIL, "[R4] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_REAR4);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#endif

static ssize_t rear_camera_wifi_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("wifi_info : %s\n", wifi_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", wifi_info);
	if (rc)
		return rc;
	return 0;
}
static ssize_t rear_camera_wifi_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[HWB] buf : %s\n", buf);
	scnprintf(wifi_info, sizeof(wifi_info), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static ssize_t front2_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	struct cam_hw_param *ec_param = NULL;

	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT2);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, module_id[EEP_FRONT2], HW_PARAM_FRONT2);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CAM_DBG(CAM_UTIL, "[F2] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT2);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static ssize_t front3_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	struct cam_hw_param *ec_param = NULL;

	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT3);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, module_id[EEP_FRONT3], HW_PARAM_FRONT3);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CAM_DBG(CAM_UTIL, "[F3] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT3);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#else
static ssize_t front2_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	struct cam_hw_param *ec_param = NULL;

	hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT2);

	if (ec_param != NULL) {
		rc = fill_hw_bigdata_sysfs_node(buf, ec_param, module_id[EEP_FRONT2], HW_PARAM_FRONT2);
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_hw_param_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CAM_DBG(CAM_UTIL, "[F2] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		hw_bigdata_get_hw_param(&ec_param, HW_PARAM_FRONT2);
		if (ec_param != NULL) {
			hw_bigdata_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#endif
#endif
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
static DEVICE_ATTR(rear_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_camera_hw_param_show, rear_camera_hw_param_store);
static DEVICE_ATTR(front_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_hw_param_show, front_camera_hw_param_store);
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
static DEVICE_ATTR(rear2_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_camera_hw_param_show, rear2_camera_hw_param_store);
#endif
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
static DEVICE_ATTR(rear3_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_camera_hw_param_show, rear3_camera_hw_param_store);
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
static DEVICE_ATTR(rear4_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_camera_hw_param_show, rear4_camera_hw_param_store);
#endif
static DEVICE_ATTR(cam_wifi_info, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_camera_wifi_info_show, rear_camera_wifi_info_store);
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front2_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_hw_param_show, front2_camera_hw_param_store);
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front3_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_hw_param_show, front3_camera_hw_param_store);
#else
static DEVICE_ATTR(front2_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_hw_param_show, front2_camera_hw_param_store);
#endif
#endif
#endif

const struct device_attribute *hwb_rear_attrs[] = {
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	&dev_attr_rear_hwparam,
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
	&dev_attr_rear2_hwparam,
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	&dev_attr_rear3_hwparam,
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
	&dev_attr_rear4_hwparam,
#endif
#endif
	&dev_attr_cam_wifi_info,
#endif
	NULL, // DO NOT REMOVE
};

const struct device_attribute *hwb_front_attrs[] = {
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
		&dev_attr_front_hwparam,
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
		&dev_attr_front2_hwparam,
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
		&dev_attr_front3_hwparam,
#else
		&dev_attr_front2_hwparam,
#endif
#endif
#endif
	NULL, // DO NOT REMOVE
};

MODULE_DESCRIPTION("CAM_SYSFS_HW_BIGDATA");
MODULE_LICENSE("GPL v2");
