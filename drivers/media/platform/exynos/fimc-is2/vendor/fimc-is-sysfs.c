/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm_qos.h>

#include "fimc-is-sysfs.h"
#include "fimc-is-core.h"
#include "fimc-is-err.h"
#include "fimc-is-sec-define.h"
#ifdef CONFIG_OIS_USE
#include "fimc-is-device-ois.h"
#endif
#ifdef CONFIG_AF_HOST_CONTROL
#include "fimc-is-device-af.h"
#endif
/* #define FORCE_CAL_LOAD */

extern struct device *fimc_is_dev;
struct class *camera_class = NULL;
struct device *camera_front_dev;
struct device *camera_rear_dev;
#ifdef CONFIG_OIS_USE
struct device *camera_ois_dev;
#endif
static struct fimc_is_core *sysfs_core;

extern bool crc32_fw_check;
extern bool crc32_check;
extern bool crc32_check_factory;
extern bool fw_version_crc_check;
extern bool is_latest_cam_module;
extern bool is_final_cam_module;

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_OIS)
extern bool crc32_ois_check;
extern bool crc32_ois_fw_check;
extern bool crc32_ois_cal_check;
#endif

#ifdef CONFIG_COMPANION_USE
extern bool crc32_c1_fw_check;
extern bool crc32_c1_check;
extern bool crc32_c1_check_factory;
#endif /* CONFIG_COMPANION_USE */
extern bool fw_version_crc_check;
static struct fimc_is_from_info *pinfo = NULL;
static struct fimc_is_from_info *finfo = NULL;
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
extern bool crc32_check_factory_front;
extern bool is_final_cam_module_front;
static struct fimc_is_from_info *front_finfo = NULL;
#endif

#ifdef CAMERA_SYSFS_V2
static struct fimc_is_cam_info cam_infos[2];
#endif

extern bool force_caldata_dump;

#ifdef CAMERA_SYSFS_V2
int fimc_is_get_cam_info(struct fimc_is_cam_info **caminfo)
{
	*caminfo = cam_infos;
	return 0;
}
#endif

static int read_from_firmware_version(int position)
{
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;
	int ret = 0;

	fimc_is_sec_get_sysfs_pinfo(&pinfo);
	fimc_is_sec_get_sysfs_finfo(&finfo);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	fimc_is_sec_get_sysfs_finfo_front(&front_finfo);
#endif

	if (((position == SENSOR_POSITION_REAR) && (!finfo->is_caldata_read))
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		|| ((position == SENSOR_POSITION_FRONT) && (!front_finfo->is_caldata_read))
#endif
	) {
		ret = fimc_is_sec_run_fw_sel(is_dev, position);
		if (ret) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
		}

		if ((position == SENSOR_POSITION_REAR)) {
#ifdef CONFIG_COMPANION_USE
			fimc_is_sec_concord_fw_sel(sysfs_core, is_dev);
#endif
		}
	}
	return 0;
}

#ifdef CONFIG_OIS_USE
static bool read_ois_version(void)
{
	bool ret = true;

	if (!sysfs_core->running_rear_camera) {
		if (!sysfs_core->ois_ver_read) {
			fimc_is_ois_gpio_on(sysfs_core);
			msleep(150);

			ret = fimc_is_ois_check_fw(sysfs_core);
			if (!sysfs_core->running_rear_camera) {
				fimc_is_ois_gpio_off(sysfs_core);
			}
		}
	}
	return ret;
}
#endif

static ssize_t camera_front_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return sprintf(buf, "%d\n", 0);
	}

	dev_info(dev, "%s: E", __func__);

	return sprintf(buf, "%d\n", core_pdata->front_sensor_id);
}

static ssize_t camera_rear_sensorid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_platform_fimc_is *core_pdata = NULL;
	int ret = 0;
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;

	if (force_caldata_dump == false) {
		ret = fimc_is_sec_run_fw_sel(is_dev, SENSOR_POSITION_REAR);
		if (ret) {
			err("fimc_is_sec_run_fw_sel is fail(%d)", ret);
		}
	}

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return sprintf(buf, "%d\n", 0);
	}

	dev_info(dev, "%s: E", __func__);

	return sprintf(buf, "%d\n", core_pdata->rear_sensor_id);
}

static DEVICE_ATTR(front_sensorid, S_IRUGO, camera_front_sensorid_show, NULL);
static DEVICE_ATTR(rear_sensorid, S_IRUGO, camera_rear_sensorid_show, NULL);

static int fimc_is_get_sensor_data(struct device *dev, char *maker, char *name, int position)
{
	struct fimc_is_core *core;
	struct exynos_platform_fimc_is *core_pdata = NULL;
	struct fimc_is_device_sensor *device;
	struct fimc_is_module_enum *module;
	int sensor_id = 0;
	int i = 0;

	if (!fimc_is_dev) {
		dev_err(dev, "%s: fimc_is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		dev_err(dev, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	core_pdata = dev_get_platdata(fimc_is_dev);
	if (!core_pdata) {
		err("core->pdata is null");
		return -EINVAL;
	}

	if (position == SENSOR_POSITION_REAR) {
		sensor_id = core_pdata->rear_sensor_id;
	} else if(position == SENSOR_POSITION_FRONT) {
		sensor_id = core_pdata->front_sensor_id;
	} else {
		dev_err(dev, "%s: position value is wrong", __func__);
		return -EINVAL;
	}

	for (i = 0; i < FIMC_IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];

		if (device->resourcemgr == NULL)
			continue;

		fimc_is_search_sensor_module(&core->sensor[i], sensor_id, &module);
		if (module)
			break;
	}

	if(module) {
		if (maker != NULL)
			sprintf(maker, "%s", module->sensor_maker ?
					module->sensor_maker : "UNKNOWN");
		if (name != NULL)
			sprintf(name, "%s", module->sensor_name ?
					module->sensor_name : "UNKNOWN");
		return 0;
	}

	dev_err(dev, "%s: there's no matched sensor id", __func__);

	return -ENODEV;
}

static ssize_t camera_front_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_maker[50];
	char sensor_name[50];
	int ret;

	ret = fimc_is_get_sensor_data(dev, sensor_maker, sensor_name, SENSOR_POSITION_FRONT);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_FIMC_IS\n");
	else
		return sprintf(buf, "%s_%s_FIMC_IS\n", sensor_maker, sensor_name);
}

static ssize_t camera_front_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
	char command_ack[20] = {0, };

	read_from_firmware_version(SENSOR_POSITION_FRONT);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_FRONT)) {
		err(" NG, invalid FROM version");
		strcpy(command_ack, "NG_CD3");
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

	if (crc32_check_factory_front) {
		return sprintf(buf, "%s %s\n", front_finfo->header_ver, front_finfo->header_ver);
	} else {
		err(" NG, crc check fail");
		strcpy(command_ack, "NG_");
		if (!crc32_check_factory_front)
			strcat(command_ack, "CD3");
		if (front_finfo->header_ver[3] != 'L')
			strcat(command_ack, "_Q");
		return sprintf(buf, "%s %s\n", front_finfo->header_ver, command_ack);
	}
#else
	char sensor_name[50];
	int ret;

	ret = fimc_is_get_sensor_data(dev, NULL, sensor_name, SENSOR_POSITION_FRONT);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN UNKNOWN\n");
	else
		return sprintf(buf, "%s N\n", sensor_name);
#endif
}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
static ssize_t camera_front_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };

	read_from_firmware_version(SENSOR_POSITION_FRONT);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_FRONT)) {
		err(" NG, invalid FROM version");
		strcpy(command_ack, "NG_CD3");
		return sprintf(buf, "%s %s %s\n", "NULL", "N", command_ack);
	}

	if (crc32_check_factory_front) {
		return sprintf(buf, "%s %s %s\n", front_finfo->header_ver, "N", front_finfo->header_ver);
	} else {
		err(" NG, crc check fail");
		strcpy(command_ack, "NG_");
		if (!crc32_check_factory_front)
			strcat(command_ack, "CD3");
		if (front_finfo->header_ver[3] != 'L')
			strcat(command_ack, "_Q");
		return sprintf(buf, "%s %s %s\n", front_finfo->header_ver, "N", command_ack);
	}
}

static ssize_t camera_front_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[10] = {0, };

	read_from_firmware_version(SENSOR_POSITION_FRONT);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_FRONT)) {
		err(" NG, invalid FROM version");
#ifdef CAMERA_SYSFS_V2
		return sprintf(buf, "%s\n", "NG_VER");
#else
		return sprintf(buf, "%s\n", "NG");
#endif
	}

	if (crc32_check_factory_front) {
		if (!is_final_cam_module_front) {
			err(" NG, not final cam module");
#ifdef CAMERA_SYSFS_V2
			strcpy(command_ack, "NG_VER\n");
#else
			strcpy(command_ack, "NG\n");
#endif
		} else {
			strcpy(command_ack, "OK\n");
		}
	} else {
		err(" NG, crc check fail");
#ifdef CAMERA_SYSFS_V2
		strcpy(command_ack, "NG_CRC\n");
#else
		strcpy(command_ack, "NG\n");
#endif
	}
	return sprintf(buf, "%s", command_ack);
}
#endif

#ifdef CAMERA_SYSFS_V2
static ssize_t camera_front_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char camera_info[100] = {0, };
#ifdef CONFIG_OF
	struct fimc_is_cam_info *front_cam_info = &(cam_infos[1]);
	strcpy(camera_info, "ISP=");
	switch(front_cam_info->isp) {
		case CAM_INFO_ISP_TYPE_INTERNAL :
			strcat(camera_info, "INT;");
			break;
		case CAM_INFO_ISP_TYPE_EXTERNAL :
			strcat(camera_info, "EXT;");
			break;
		case CAM_INFO_ISP_TYPE_SOC :
			strcat(camera_info, "SOC;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CALMEM=");
	switch(front_cam_info->cal_memory) {
		case CAM_INFO_CAL_MEM_TYPE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CAL_MEM_TYPE_FROM :
		case CAM_INFO_CAL_MEM_TYPE_EEPROM :
		case CAM_INFO_CAL_MEM_TYPE_OTP :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "READVER=");
	switch(front_cam_info->read_version) {
		case CAM_INFO_READ_VER_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_READ_VER_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "COREVOLT=");
	switch(front_cam_info->core_voltage) {
		case CAM_INFO_CORE_VOLT_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CORE_VOLT_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "UPGRADE=");
	switch(front_cam_info->upgrade) {
		case CAM_INFO_FW_UPGRADE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_UPGRADE_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_FW_UPGRADE_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CC=");
	switch(front_cam_info->companion) {
		case CAM_INFO_COMPANION_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_COMPANION_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "OIS=");
	switch(front_cam_info->ois) {
		case CAM_INFO_OIS_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_OIS_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	return sprintf(buf, "%s\n", camera_info);
#endif
	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;CC=NULL;OIS=NULL");

	return sprintf(buf, "%s\n", camera_info);
}
#endif

static DEVICE_ATTR(front_camtype, S_IRUGO,
		camera_front_camtype_show, NULL);
#ifdef CAMERA_SYSFS_V2
static DEVICE_ATTR(front_caminfo, S_IRUGO,
		camera_front_info_show, NULL);
#endif
static DEVICE_ATTR(front_camfw, S_IRUGO, camera_front_camfw_show, NULL);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
static DEVICE_ATTR(front_camfw_full, S_IRUGO, camera_front_camfw_full_show, NULL);
static DEVICE_ATTR(front_checkfw_factory, S_IRUGO, camera_front_checkfw_factory_show, NULL);
#endif

#ifdef CAMERA_MODULE_DUALIZE
static ssize_t camera_rear_writefw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct device *is_dev = &sysfs_core->ischain[0].pdev->dev;
	int ret = 0;

	ret = fimc_is_sec_write_fw(sysfs_core, is_dev);

	if (ret)
		return sprintf(buf, "NG\n");
	else
		return sprintf(buf, "OK\n");
}
#endif

static ssize_t camera_rear_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_maker[50];
	char sensor_name[50];
	int ret;

	ret = fimc_is_get_sensor_data(dev, sensor_maker, sensor_name, SENSOR_POSITION_REAR);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_FIMC_IS\n");
	else
		return sprintf(buf, "%s_%s_FIMC_IS\n", sensor_maker, sensor_name);
}

static ssize_t camera_rear_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };
	char *loaded_fw;
	struct exynos_platform_fimc_is *core_pdata = NULL;

	core_pdata = dev_get_platdata(fimc_is_dev);

#if defined(FRONT_CAMERA_ONLY_SUPPORT)
	err(" N,N There is no REAR CAMERA : FRONT_CAMERA_ONLY_SUPPORT");
	return sprintf(buf, "%s %s\n", "N", "N");
#endif

	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid FROM version");
		strcpy(command_ack, "NG_FWCDFW1CD1");
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	loaded_fw = pinfo->header_ver;
#else
	fimc_is_sec_get_loaded_fw(&loaded_fw);
#endif

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
#ifdef CONFIG_COMPANION_USE
			&& crc32_c1_fw_check && crc32_c1_check_factory
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_OIS
			&& crc32_ois_check
#endif
		) {
			return sprintf(buf, "%s %s\n", finfo->header_ver, loaded_fw);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_");
			if (!crc32_fw_check)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
#ifdef CONFIG_COMPANION_USE
			if (!crc32_c1_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_c1_check_factory)
				strcat(command_ack, "CD1");
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_OIS
			if (!crc32_ois_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_ois_check)
				strcat(command_ack, "CD1");
#endif
			if (finfo->header_ver[3] != 'L')
				strcat(command_ack, "_Q");
			return sprintf(buf, "%s %s\n", finfo->header_ver, command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		strcat(command_ack, "CD");
#else
		strcat(command_ack, "FWCD");
#endif
#ifdef CONFIG_COMPANION_USE
		strcat(command_ack, "FW1CD1");
#endif
		return sprintf(buf, "%s %s\n", finfo->header_ver, command_ack);
	}
}

static ssize_t camera_rear_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[20] = {0, };
	char *loaded_fw;

#if defined(FRONT_CAMERA_ONLY_SUPPORT)
	err(" N,N There is no REAR CAMERA : FRONT_CAMERA_ONLY_SUPPORT");
	return sprintf(buf, "%s %s %s\n", "N", "N", "N");
#endif

	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid FROM version");
		strcpy(command_ack, "NG_FWCDFW1CD1");
		return sprintf(buf, "%s %s %s\n", "NULL", "NULL", command_ack);
	}

#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
	loaded_fw = pinfo->header_ver;
#else
	fimc_is_sec_get_loaded_fw(&loaded_fw);
#endif

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
#ifdef CONFIG_COMPANION_USE
			&& crc32_c1_fw_check && crc32_c1_check_factory
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_OIS
			&& crc32_ois_check
#endif
		) {
			return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, loaded_fw);
		} else {
			err(" NG, crc check fail");
			strcpy(command_ack, "NG_");
			if (!crc32_fw_check)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, "CD");
#ifdef CONFIG_COMPANION_USE
			if (!crc32_c1_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_c1_check_factory)
				strcat(command_ack, "CD1");
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_OIS
			if (!crc32_ois_fw_check)
				strcat(command_ack, "FW1");
			if (!crc32_ois_check)
				strcat(command_ack, "CD1");
#endif
			if (finfo->header_ver[3] != 'L')
				strcat(command_ack, "_Q");
			return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, command_ack);
		}
	} else {
		err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_");
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_REAR)
		strcat(command_ack, "CD");
#else
		strcat(command_ack, "FWCD");
#endif
#ifdef CONFIG_COMPANION_USE
		strcat(command_ack, "FW1CD1");
#endif
		return sprintf(buf, "%s %s %s\n", finfo->header_ver, pinfo->header_ver, command_ack);
	}
}

#if !defined(FRONT_CAMERA_ONLY_SUPPORT)
static ssize_t camera_rear_checkfw_user_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *cal_buf;

	read_from_firmware_version(SENSOR_POSITION_REAR);

	/* 2T2 Sensor Only. Can be removed after development */
	fimc_is_sec_get_cal_buf(&cal_buf);
	if (((finfo->header_ver[0] == 'A') && (finfo->header_ver[1] == '2') && (finfo->header_ver[2] == '0'))
			&& (cal_buf[0xC0] < 0xC0)) {
		err(" NG, old sensor revision");
		return sprintf(buf, "%s\n", "NG");
	}

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid FROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
#ifdef CONFIG_COMPANION_USE
			&& crc32_c1_fw_check && crc32_c1_check_factory
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_OIS
			&& crc32_ois_check && crc32_ois_fw_check
#endif
		) {
			if (!is_latest_cam_module) {
				err(" NG, not latest cam module");
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			err(" NG, crc check fail");
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		err(" NG, fw ver crc check fail");
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_rear_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char command_ack[10] = {0, };
#ifdef CONFIG_OIS_USE
	struct fimc_is_ois_info *ois_minfo = NULL;
	bool ois_ret = false;

	ois_ret = read_ois_version();
	fimc_is_ois_get_module_version(&ois_minfo);
#endif

	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (!fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR)) {
		err(" NG, invalid FROM version");
#ifdef CAMERA_SYSFS_V2
		return sprintf(buf, "%s\n", "NG_VER");
#else
		return sprintf(buf, "%s\n", "NG");
#endif
	}

	if(fw_version_crc_check) {
		if (crc32_fw_check && crc32_check_factory
#ifdef CONFIG_COMPANION_USE
			&& crc32_c1_fw_check && crc32_c1_check_factory
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_OIS
			&& crc32_ois_check && crc32_ois_fw_check
#endif
		) {
			if (!is_final_cam_module) {
				err(" NG, not final cam module");
#ifdef CAMERA_SYSFS_V2
				strcpy(command_ack, "NG_VER\n");
#else
				strcpy(command_ack, "NG\n");
#endif
			} else {
#ifdef CONFIG_OIS_USE
				if (ois_minfo->checksum != 0x00 || ois_minfo->caldata != 0x00 || !ois_ret) {
					err(" NG, OIS crc check fail");
#ifdef CAMERA_SYSFS_V2
					strcpy(command_ack, "NG_CRC\n");
#else
					strcpy(command_ack, "NG\n");
#endif
				} else {
					strcpy(command_ack, "OK\n");
				}
#else
				strcpy(command_ack, "OK\n");
#endif
			}
		} else {
			err(" NG, crc check fail");
#ifdef CAMERA_SYSFS_V2
			strcpy(command_ack, "NG_CRC\n");
#else
			strcpy(command_ack, "NG\n");
#endif
		}
	} else {
		err(" NG, fw ver crc check fail");
#ifdef CAMERA_SYSFS_V2
		strcpy(command_ack, "NG_VER\n");
#else
		strcpy(command_ack, "NG\n");
#endif
	}

	return sprintf(buf, "%s", command_ack);
}
#endif

#ifdef CAMERA_SYSFS_V2
static ssize_t camera_rear_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char camera_info[100] = {0, };
#ifdef CONFIG_OF
	struct fimc_is_cam_info *rear_cam_info = &(cam_infos[0]);

	strcpy(camera_info, "ISP=");
	switch(rear_cam_info->isp) {
		case CAM_INFO_ISP_TYPE_INTERNAL :
			strcat(camera_info, "INT;");
			break;
		case CAM_INFO_ISP_TYPE_EXTERNAL :
			strcat(camera_info, "EXT;");
			break;
		case CAM_INFO_ISP_TYPE_SOC :
			strcat(camera_info, "SOC;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CALMEM=");
	switch(rear_cam_info->cal_memory) {
		case CAM_INFO_CAL_MEM_TYPE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CAL_MEM_TYPE_FROM :
		case CAM_INFO_CAL_MEM_TYPE_EEPROM :
		case CAM_INFO_CAL_MEM_TYPE_OTP :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "READVER=");
	switch(rear_cam_info->read_version) {
		case CAM_INFO_READ_VER_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_READ_VER_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "COREVOLT=");
	switch(rear_cam_info->core_voltage) {
		case CAM_INFO_CORE_VOLT_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CORE_VOLT_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "UPGRADE=");
	switch(rear_cam_info->upgrade) {
		case CAM_INFO_FW_UPGRADE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_UPGRADE_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_FW_UPGRADE_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "CC=");
	switch(rear_cam_info->companion) {
		case CAM_INFO_COMPANION_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_COMPANION_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	strcat(camera_info, "OIS=");
	switch(rear_cam_info->ois) {
		case CAM_INFO_OIS_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_OIS_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	return sprintf(buf, "%s\n", camera_info);
#endif
	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;CC=NULL;OIS=NULL");

	return sprintf(buf, "%s\n", camera_info);
}
#endif

static ssize_t camera_rear_sensor_standby(struct device *dev,
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

static ssize_t camera_rear_sensor_standby_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Rear sensor standby \n");
}

#ifdef CONFIG_COMPANION_USE
static ssize_t camera_rear_companionfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *loaded_c1_fw;

	read_from_firmware_version(SENSOR_POSITION_REAR);
	fimc_is_sec_get_loaded_c1_fw(&loaded_c1_fw);

	return sprintf(buf, "%s %s\n",
		finfo->concord_header_ver, loaded_c1_fw);
}

static ssize_t camera_rear_companionfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char *loaded_c1_fw;

	read_from_firmware_version(SENSOR_POSITION_REAR);
	fimc_is_sec_get_loaded_c1_fw(&loaded_c1_fw);

	return sprintf(buf, "%s %s %s\n",
		finfo->concord_header_ver, pinfo->concord_header_ver, loaded_c1_fw);
}
#endif

static ssize_t camera_rear_camfw_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = -EINVAL;

	if ((size == 1 || size == 2) && (buf[0] == 'F' || buf[0] == 'f')) {
		fimc_is_sec_set_force_caldata_dump(true);
		ret = size;
	} else {
		fimc_is_sec_set_force_caldata_dump(false);
	}
	return ret;
}

static ssize_t camera_rear_calcheck_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char rear_sensor[10] = {0, };
#ifdef CONFIG_COMPANION_USE
	char rear_companion[10] = {0, };
#endif
#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_FRONT
	char front_sensor[10] = {0, };
#endif

	read_from_firmware_version(SENSOR_POSITION_REAR);

	if (fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR) && crc32_check_factory)
		strcpy(rear_sensor, "Normal");
	else
		strcpy(rear_sensor, "Abnormal");

#ifdef CONFIG_CAMERA_EEPROM_SUPPORT_FRONT
	read_from_firmware_version(SENSOR_POSITION_FRONT);
#ifdef CONFIG_COMPANION_USE
	if (fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR) && crc32_c1_check_factory)
		strcpy(rear_companion, "Normal");
	else
		strcpy(rear_companion, "Abnormal");

	if (fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_FRONT) && crc32_check_factory_front)
		strcpy(front_sensor, "Normal");
	else
		strcpy(front_sensor, "Abnormal");

	return sprintf(buf, "%s %s %s\n", rear_sensor, rear_companion, front_sensor);
#else
	return sprintf(buf, "%s %s\n", rear_sensor, front_sensor);
#endif
#else
#ifdef CONFIG_COMPANION_USE
	if (fimc_is_sec_check_from_ver(sysfs_core, SENSOR_POSITION_REAR) && crc32_c1_check_factory)
		strcpy(rear_companion, "Normal");
	else
		strcpy(rear_companion, "Abnormal");

	return sprintf(buf, "%s %s %s\n", rear_sensor, rear_companion, "Null");
#else
	return sprintf(buf, "%s %s\n", rear_sensor, "Null");
#endif
#endif
}

#ifdef CONFIG_COMPANION_USE
static ssize_t camera_isp_core_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_COMPANION_DCDC_USE
	int sel;

	if (DCDC_VENDOR_NONE == sysfs_core->companion_dcdc.type)
		return sprintf(buf, "none\n");

	sel = fimc_is_power_binning(sysfs_core);
	return sprintf(buf, "%s\n", sysfs_core->companion_dcdc.get_vout_str(sel));
#else
	return sprintf(buf, "none\n");
#endif
}
#endif

#ifdef CONFIG_OIS_USE
static ssize_t camera_ois_power_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)

{
	switch (buf[0]) {
	case '0':
		fimc_is_ois_gpio_off(sysfs_core);
		break;
	case '1':
		fimc_is_ois_gpio_on(sysfs_core);
		msleep(150);
		break;
	default:
		pr_debug("%s: %c\n", __func__, buf[0]);
		break;
	}

	return count;
}

static ssize_t camera_ois_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result_total = 0;
	bool result_offset = 0, result_selftest = 0;
	int selftest_ret = 0;
	long raw_data_x = 0, raw_data_y = 0;

	fimc_is_ois_offset_test(sysfs_core, &raw_data_x, &raw_data_y);
	msleep(50);
	selftest_ret = fimc_is_ois_self_test(sysfs_core);

	if (selftest_ret == 0x0) {
		result_selftest = true;
	} else {
		result_selftest = false;
	}

	if (abs(raw_data_x) > 30000 || abs(raw_data_y) > 30000)  {
		result_offset = false;
	} else {
		result_offset = true;
	}

	if (result_offset && result_selftest) {
		result_total = 0;
	} else if (!result_offset && !result_selftest) {
		result_total = 3;
	} else if (!result_offset) {
		result_total = 1;
	} else if (!result_selftest) {
		result_total = 2;
	}

	if (raw_data_x < 0 && raw_data_y < 0) {
		return sprintf(buf, "%d,-%ld.%03ld,-%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else if (raw_data_x < 0) {
		return sprintf(buf, "%d,-%ld.%03ld,%ld.%03ld\n", result_total, abs(raw_data_x /1000), abs(raw_data_x % 1000),
			raw_data_y /1000, raw_data_y % 1000);
	} else if (raw_data_y < 0) {
		return sprintf(buf, "%d,%ld.%03ld,-%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else {
		return sprintf(buf, "%d,%ld.%03ld,%ld.%03ld\n", result_total, raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

static ssize_t camera_ois_rawdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	long raw_data_x = 0, raw_data_y = 0;

	fimc_is_ois_get_offset_data(sysfs_core, &raw_data_x, &raw_data_y);

	if (raw_data_x < 0 && raw_data_y < 0) {
		return sprintf(buf, "-%ld.%03ld,-%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else if (raw_data_x < 0) {
		return sprintf(buf, "-%ld.%03ld,%ld.%03ld\n", abs(raw_data_x /1000), abs(raw_data_x % 1000),
			raw_data_y /1000, raw_data_y % 1000);
	} else if (raw_data_y < 0) {
		return sprintf(buf, "%ld.%03ld,-%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
			abs(raw_data_y /1000), abs(raw_data_y % 1000));
	} else {
		return sprintf(buf, "%ld.%03ld,%ld.%03ld\n", raw_data_x /1000, raw_data_x % 1000,
			raw_data_y /1000, raw_data_y % 1000);
	}
}

static ssize_t camera_ois_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;
	bool ret = false;

	ret = read_ois_version();
	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);

	if (ois_minfo->checksum != 0x00 || !ret) {
		return sprintf(buf, "%s %s\n", "NG_FW2", "NULL");
	} else if (ois_minfo->caldata != 0x00) {
		return sprintf(buf, "%s %s\n", "NG_CD2", ois_pinfo->header_ver);
	} else {
		return sprintf(buf, "%s %s\n", ois_minfo->header_ver, ois_pinfo->header_ver);
	}
}

static ssize_t camera_ois_diff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result = 0;
	int x_diff = 0, y_diff = 0;

	result = fimc_is_ois_diff_test(sysfs_core, &x_diff, &y_diff);

	return sprintf(buf, "%d,%d,%d\n", result == true ? 0 : 1, x_diff, y_diff);
}

static ssize_t camera_ois_fw_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_OIS_FW_UPDATE_THREAD_USE
	fimc_is_ois_init_thread(sysfs_core);
#else
	bool ret = false;
	ret = fimc_is_ois_check_sensor(sysfs_core);
	if (ret) {
		err("Do not update ois fw update. Check sensor failed!\n");
		return sprintf(buf, "%s\n", "Ois update failed.");
	}

	fimc_is_ois_gpio_on(sysfs_core);
	msleep(150);

	fimc_is_ois_fw_update(sysfs_core);
	fimc_is_ois_gpio_off(sysfs_core);
#endif

	return sprintf(buf, "%s\n", "Ois update done.");
}

static ssize_t camera_ois_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_is_ois_info *ois_minfo = NULL;
	struct fimc_is_ois_info *ois_pinfo = NULL;
	struct fimc_is_ois_info *ois_uinfo = NULL;
	struct fimc_is_ois_exif *ois_exif = NULL;

	fimc_is_ois_get_module_version(&ois_minfo);
	fimc_is_ois_get_phone_version(&ois_pinfo);
	fimc_is_ois_get_user_version(&ois_uinfo);
	fimc_is_ois_get_exif_data(&ois_exif);

	return sprintf(buf, "%s %s %s %d %d", ois_minfo->header_ver, ois_pinfo->header_ver,
		ois_uinfo->header_ver, ois_exif->error_data, ois_exif->status_data);
}
#endif

#ifdef FORCE_CAL_LOAD
static ssize_t camera_rear_force_cal_load_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	fimc_is_sec_set_force_caldata_dump(true);
	return sprintf(buf, "FORCE CALDATA LOAD\n");
}
#endif

static ssize_t camera_rear_afcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	read_from_firmware_version(SENSOR_POSITION_REAR);

	return sprintf(buf, "1 %d %d\n", finfo->af_cal_macro, finfo->af_cal_pan);
}

#ifdef CAMERA_MODULE_DUALIZE
static DEVICE_ATTR(from_write, S_IRUGO,
		camera_rear_writefw_show, NULL);
#endif
static DEVICE_ATTR(rear_camtype, S_IRUGO,
		camera_rear_camtype_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO,
		camera_rear_camfw_show, camera_rear_camfw_write);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO,
		camera_rear_camfw_full_show, NULL);
#ifdef CONFIG_COMPANION_USE
static DEVICE_ATTR(rear_companionfw, S_IRUGO,
		camera_rear_companionfw_show, NULL);
static DEVICE_ATTR(rear_companionfw_full, S_IRUGO,
		camera_rear_companionfw_full_show, NULL);
#endif
static DEVICE_ATTR(rear_calcheck, S_IRUGO,
		camera_rear_calcheck_show, NULL);
#if !defined(FRONT_CAMERA_ONLY_SUPPORT)
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO,
		camera_rear_checkfw_user_show, NULL);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO,
		camera_rear_checkfw_factory_show, NULL);
#endif
static DEVICE_ATTR(rear_sensor_standby, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
		camera_rear_sensor_standby_show, camera_rear_sensor_standby);
#ifdef CAMERA_SYSFS_V2
static DEVICE_ATTR(rear_caminfo, S_IRUGO,
		camera_rear_info_show, NULL);
#endif
#ifdef CONFIG_COMPANION_USE
static DEVICE_ATTR(isp_core, S_IRUGO,
		camera_isp_core_show, NULL);
#endif
#ifdef CONFIG_OIS_USE
static DEVICE_ATTR(selftest, S_IRUGO,
		camera_ois_selftest_show, NULL);
static DEVICE_ATTR(ois_power, S_IWUSR,
		NULL, camera_ois_power_store);
static DEVICE_ATTR(ois_rawdata, S_IRUGO,
		camera_ois_rawdata_show, NULL);
static DEVICE_ATTR(oisfw, S_IRUGO,
		camera_ois_version_show, NULL);
static DEVICE_ATTR(ois_diff, S_IRUGO,
		camera_ois_diff_show, NULL);
static DEVICE_ATTR(fw_update, S_IRUGO,
		camera_ois_fw_update_show, NULL);
static DEVICE_ATTR(ois_exif, S_IRUGO,
		camera_ois_exif_show, NULL);
#endif
#ifdef FORCE_CAL_LOAD
static DEVICE_ATTR(rear_force_cal_load, S_IRUGO, camera_rear_force_cal_load_show, NULL);
#endif
static DEVICE_ATTR(rear_afcal, S_IRUGO, camera_rear_afcal_show, NULL);

int fimc_is_create_sysfs(struct fimc_is_core *core)
{
	if (!core) {
		err("fimc_is_core is null");
		return -EINVAL;
	}

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class)) {
			pr_err("Failed to create class(camera)!\n");
			return PTR_ERR(camera_class);
		}
	}

	camera_front_dev = device_create(camera_class, NULL, 0, NULL, "front");
	if (IS_ERR(camera_front_dev)) {
		printk(KERN_ERR "failed to create front device!\n");
	} else {
		if (device_create_file(camera_front_dev,
				&dev_attr_front_sensorid) < 0) {
			printk(KERN_ERR "failed to create front device file, %s\n",
					dev_attr_front_sensorid.attr.name);
		}

		if (device_create_file(camera_front_dev,
					&dev_attr_front_camtype) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camtype.attr.name);
		}
		if (device_create_file(camera_front_dev,
					&dev_attr_front_camfw) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camfw.attr.name);
		}
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		if (device_create_file(camera_front_dev,
					&dev_attr_front_camfw_full) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_camfw_full.attr.name);
		}
		if (device_create_file(camera_front_dev,
				&dev_attr_front_checkfw_factory) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_checkfw_factory.attr.name);
		}
#endif
	}
#ifdef CAMERA_SYSFS_V2
		if (device_create_file(camera_front_dev,
					&dev_attr_front_caminfo) < 0) {
			printk(KERN_ERR
				"failed to create front device file, %s\n",
				dev_attr_front_caminfo.attr.name);
		}
#endif
	camera_rear_dev = device_create(camera_class, NULL, 1, NULL, "rear");
	if (IS_ERR(camera_rear_dev)) {
		printk(KERN_ERR "failed to create rear device!\n");
	} else {
		if (device_create_file(camera_rear_dev, &dev_attr_rear_sensorid) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_sensorid.attr.name);
		}

#ifdef CAMERA_MODULE_DUALIZE
		if (device_create_file(camera_rear_dev, &dev_attr_from_write) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_from_write.attr.name);
		}
#endif
		if (device_create_file(camera_rear_dev, &dev_attr_rear_camtype) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_camtype.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_camfw) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_camfw.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_camfw_full) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_camfw_full.attr.name);
		}
#if !defined(FRONT_CAMERA_ONLY_SUPPORT)
		if (device_create_file(camera_rear_dev, &dev_attr_rear_checkfw_user) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_checkfw_user.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_checkfw_factory) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_checkfw_factory.attr.name);
		}
#endif
		if (device_create_file(camera_rear_dev, &dev_attr_rear_sensor_standby) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_sensor_standby.attr.name);
		}
#ifdef CAMERA_SYSFS_V2
		if (device_create_file(camera_rear_dev,
					&dev_attr_rear_caminfo) < 0) {
			printk(KERN_ERR
				"failed to create rear device file, %s\n",
				dev_attr_rear_caminfo.attr.name);
		}
#endif
#ifdef CONFIG_COMPANION_USE
		if (device_create_file(camera_rear_dev, &dev_attr_rear_companionfw) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_companionfw.attr.name);
		}
		if (device_create_file(camera_rear_dev, &dev_attr_rear_companionfw_full) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_companionfw_full.attr.name);
		}
#endif
		if (device_create_file(camera_rear_dev, &dev_attr_rear_calcheck) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_rear_calcheck.attr.name);
		}
#ifdef CONFIG_COMPANION_USE
		if (device_create_file(camera_rear_dev, &dev_attr_isp_core) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_isp_core.attr.name);
		}
#endif
#ifdef CONFIG_OIS_USE
		if (device_create_file(camera_rear_dev, &dev_attr_fw_update) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
				dev_attr_fw_update.attr.name);
		}
#endif
#ifdef FORCE_CAL_LOAD
		if (device_create_file(camera_rear_dev, &dev_attr_rear_force_cal_load) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_force_cal_load.attr.name);
		}
#endif
		if (device_create_file(camera_rear_dev, &dev_attr_rear_afcal) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_afcal.attr.name);
		}
	}

#ifdef CONFIG_OIS_USE
	camera_ois_dev = device_create(camera_class, NULL, 2, NULL, "ois");
	if (IS_ERR(camera_ois_dev)) {
		printk(KERN_ERR "failed to create ois device!\n");
	} else {
		if (device_create_file(camera_ois_dev, &dev_attr_selftest) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_selftest.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_ois_power) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_ois_power.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_ois_rawdata) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_ois_rawdata.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_oisfw) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_oisfw.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_ois_diff) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_ois_diff.attr.name);
		}
		if (device_create_file(camera_ois_dev, &dev_attr_ois_exif) < 0) {
			printk(KERN_ERR "failed to create ois device file, %s\n",
				dev_attr_ois_exif.attr.name);
		}
	}
#endif

	sysfs_core = core;

	return 0;
}

int fimc_is_destroy_sysfs(struct fimc_is_core *core)
{
	if (camera_front_dev) {
		device_remove_file(camera_front_dev, &dev_attr_front_sensorid);
		device_remove_file(camera_front_dev, &dev_attr_front_camtype);
		device_remove_file(camera_front_dev, &dev_attr_front_camfw);
#if defined(CONFIG_CAMERA_EEPROM_SUPPORT_FRONT)
		device_remove_file(camera_front_dev, &dev_attr_front_camfw_full);
		device_remove_file(camera_front_dev, &dev_attr_front_checkfw_factory);
#endif
#ifdef CAMERA_SYSFS_V2
		device_remove_file(camera_front_dev, &dev_attr_front_caminfo);
#endif
	}

	if (camera_rear_dev) {
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensorid);
#ifdef CAMERA_MODULE_DUALIZE
		device_remove_file(camera_rear_dev, &dev_attr_from_write);
#endif
		device_remove_file(camera_rear_dev, &dev_attr_rear_camtype);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw);
		device_remove_file(camera_rear_dev, &dev_attr_rear_camfw_full);
#if !defined(FRONT_CAMERA_ONLY_SUPPORT)
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_user);
		device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_factory);
#endif
		device_remove_file(camera_rear_dev, &dev_attr_rear_sensor_standby);
#ifdef CONFIG_COMPANION_USE
		device_remove_file(camera_rear_dev, &dev_attr_rear_companionfw);
		device_remove_file(camera_rear_dev, &dev_attr_rear_companionfw_full);
#endif
		device_remove_file(camera_rear_dev, &dev_attr_rear_calcheck);
#ifdef CAMERA_SYSFS_V2
		device_remove_file(camera_rear_dev, &dev_attr_rear_caminfo);
#endif
#ifdef CONFIG_COMPANION_USE
		device_remove_file(camera_rear_dev, &dev_attr_isp_core);
#endif
#ifdef FORCE_CAL_LOAD
		device_remove_file(camera_rear_dev, &dev_attr_rear_force_cal_load);
#endif
		device_remove_file(camera_rear_dev, &dev_attr_rear_afcal);
	}

#ifdef CONFIG_OIS_USE
	if (camera_ois_dev) {
		device_remove_file(camera_ois_dev, &dev_attr_selftest);
		device_remove_file(camera_ois_dev, &dev_attr_ois_power);
		device_remove_file(camera_ois_dev, &dev_attr_ois_rawdata);
		device_remove_file(camera_ois_dev, &dev_attr_oisfw);
		device_remove_file(camera_ois_dev, &dev_attr_ois_diff);
		device_remove_file(camera_ois_dev, &dev_attr_ois_exif);
		device_remove_file(camera_ois_dev, &dev_attr_fw_update);
	}
#endif

	if (camera_class) {
		if (camera_front_dev)
			device_destroy(camera_class, camera_front_dev->devt);

		if (camera_rear_dev)
			device_destroy(camera_class, camera_rear_dev->devt);

#ifdef CONFIG_OIS_USE
		if (camera_ois_dev)
			device_destroy(camera_class, camera_ois_dev->devt);
#endif
	}

	class_destroy(camera_class);

	return 0;
}
