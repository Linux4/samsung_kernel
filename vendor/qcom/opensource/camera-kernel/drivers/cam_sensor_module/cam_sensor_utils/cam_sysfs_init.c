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
#include "cam_sysfs_init.h"
#include "cam_ois_core.h"
#include "cam_eeprom_dev.h"
#include "cam_actuator_core.h"
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
#include "cam_ois_mcu_stm32g.h"
#endif
#if defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
#include "cam_ois_rumba_s4.h"
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA) || defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
#include "cam_sensor_cmn_header.h"
#include "cam_debug_util.h"
#endif
#if IS_REACHABLE(CONFIG_LEDS_S2MPB02)
#include <linux/leds-s2mpb02.h>
#endif
#if defined(CONFIG_LEDS_KTD2692)
#include <linux/leds-ktd2692.h>
#endif
#if defined(CONFIG_SAMSUNG_PMIC_FLASH)
#include "cam_flash_core.h"
#endif

#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
extern void cam_sensor_ssm_i2c_read(uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);
extern void cam_sensor_ssm_i2c_write(uint32_t addr, uint32_t data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);
#endif

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
extern void cam_mipi_register_ril_notifier(void);
#endif
#if defined(CONFIG_SAMSUNG_PMIC_FLASH)
extern ssize_t flash_power_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size);
#endif
extern struct device *is_dev;

struct class *camera_class;

#define SYSFS_FW_VER_SIZE       40
#define SYSFS_MODULE_INFO_SIZE  96
/* #define FORCE_CAL_LOAD */
#define SYSFS_MAX_READ_SIZE     4096

#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
static ssize_t rear_ssm_frame_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	uint32_t read_data = -1;
	int rc = 0;

	cam_sensor_ssm_i2c_read(0x000A, &read_data, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

	rc = scnprintf(buf, PAGE_SIZE, "%x\n", read_data);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_ssm_frame_id_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value = -1;

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	return size;
}

static ssize_t rear_ssm_gmc_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	uint32_t read_data = -1;
	int rc = 0;

	cam_sensor_ssm_i2c_read(0x9C6A, &read_data, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

	rc = scnprintf(buf, PAGE_SIZE, "%x\n", read_data);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_ssm_gmc_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value = -1;

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	return size;
}

static ssize_t rear_ssm_flicker_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	uint32_t read_data = -1;
	int rc = 0;

	cam_sensor_ssm_i2c_read(0x9C6B, &read_data, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);

	rc = scnprintf(buf, PAGE_SIZE, "%x\n", read_data);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_ssm_flicker_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value = -1;

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;

	return size;
}
#endif

char rear_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	pr_info("[FW_DBG] rear_fw_ver : %s\n", rear_fw_ver);

	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear_fw_ver, sizeof(rear_fw_ver), "%s", buf);

	return size;
}

static ssize_t rear_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

#if defined(CONFIG_SEC_B0Q_PROJECT)
	char cam_type[] = "SLSI_S5KHM3\n";
#elif defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
	char cam_type[] = "SLSI_S5KGN3\n";
#elif defined(CONFIG_SEC_GTS8U_PROJECT) || defined(CONFIG_SEC_GTS8P_PROJECT)
	char cam_type[] = "HYNIX_HI1337\n";
#else
	char cam_type[] = "SONY_IMX555\n";
#endif

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);

	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
#if defined(CONFIG_SEC_B0Q_PROJECT)
	char cam_type[] = "SLSI_S5KGH1\n";
#elif defined(CONFIG_SEC_GTS8U_PROJECT) || defined(CONFIG_SEC_GTS8P_PROJECT)
	char cam_type[] = "HYNIX_HI1337\n";
#else
	char cam_type[] = "SONY_IMX374\n";
#endif
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static ssize_t front2_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
#if defined(CONFIG_SEC_GTS8U_PROJECT)
	char cam_type[] = "HYNIX_HI1337\n";
#else
	char cam_type[] = "S5K4HA\n";
#endif
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static ssize_t front3_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SONY_IMX374\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}
#else
static ssize_t front2_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SONY_IMX374\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}
#endif
#endif
char rear_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear_fw_user_ver : %s\n", rear_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear_fw_user_ver, sizeof(rear_fw_user_ver), "%s", buf);

	return size;
}

char rear_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear_fw_factory_ver : %s\n", rear_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear_fw_factory_ver, sizeof(rear_fw_factory_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SEC_B0Q_PROJECT) || defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
char rear3_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear3_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear3_fw_user_ver : %s\n", rear3_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear3_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear3_fw_user_ver, sizeof(rear3_fw_user_ver), "%s", buf);

	return size;
}

char rear3_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear3_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear3_fw_factory_ver : %s\n", rear3_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear3_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear3_fw_factory_ver, sizeof(rear3_fw_factory_ver), "%s", buf);

	return size;
}
#endif

char rear_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear_fw_full_ver : %s\n", rear_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear_fw_full_ver, sizeof(rear_fw_full_ver), "%s", buf);

	return size;
}

char rear_load_fw[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t rear_firmware_load_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear_load_fw : %s\n", rear_load_fw);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_load_fw);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_load_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear_load_fw, sizeof(rear_load_fw), "%s\n", buf);
	return size;
}

char cal_crc[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
static ssize_t rear_cal_data_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cal_crc : %s\n", cal_crc);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cal_crc);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_cal_data_check_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(cal_crc, sizeof(cal_crc), "%s", buf);

	return size;
}

char module_info[SYSFS_MODULE_INFO_SIZE] = "NULL\n";
static ssize_t rear_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] module_info : %s\n", module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(module_info, sizeof(module_info), "%s", buf);

	return size;
}

char front_module_info[SYSFS_MODULE_INFO_SIZE] = "NULL\n";
static ssize_t front_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front_module_info : %s\n", front_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_module_info, sizeof(front_module_info), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
char front2_module_info[SYSFS_MODULE_INFO_SIZE] = "NULL\n";
static ssize_t front2_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front2_module_info : %s\n", front2_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_module_info, sizeof(front2_module_info), "%s", buf);

	return size;
}
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
char front3_module_info[SYSFS_MODULE_INFO_SIZE] = "NULL\n";
static ssize_t front3_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front3_module_info : %s\n", front3_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_module_info, sizeof(front3_module_info), "%s", buf);

	return size;
}
#else
char front2_module_info[SYSFS_MODULE_INFO_SIZE] = "NULL\n";
static ssize_t front2_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front2_module_info : %s\n", front2_module_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_module_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_module_info, sizeof(front2_module_info), "%s", buf);

	return size;
}
#endif
#endif

char isp_core[10];
static ssize_t rear_isp_core_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if 0// Power binning is used
	char cam_isp_core[] = "0.8V\n";

	return scnprintf(buf, sizeof(cam_isp_core), "%s", cam_isp_core);
#else
	int rc = 0;

	pr_debug("[FW_DBG] isp_core : %s\n", isp_core);
	rc = scnprintf(buf, PAGE_SIZE, "%s\n", isp_core);
	if (rc)
		return rc;
	return 0;
#endif
}

static ssize_t rear_isp_core_check_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(isp_core, sizeof(isp_core), "%s", buf);

	return size;
}

char rear_af_cal_str[MAX_AF_CAL_STR_SIZE] = "";
static ssize_t rear_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear_af_cal_str : 20 %s\n", rear_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "20 %s", rear_af_cal_str);
	if (rc)
		return rc;

	return 0;
}

char rear_paf_cal_data_far[PAF_2PD_CAL_INFO_SIZE] = {0,};
char rear_paf_cal_data_mid[PAF_2PD_CAL_INFO_SIZE] = {0,};

static ssize_t rear_paf_offset_mid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear_paf_cal_data : %s\n", rear_paf_cal_data_mid);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_paf_cal_data_mid);
	if (rc) {
		pr_debug("data size %d\n", rc);
		return rc;
	}
	return 0;
}
static ssize_t rear_paf_offset_far_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear_paf_cal_data : %s\n", rear_paf_cal_data_far);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_paf_cal_data_far);
	if (rc) {
		pr_debug("data size %d\n", rc);
		return rc;
	}
	return 0;
}

uint32_t paf_err_data_result = 0xFFFFFFFF;
static ssize_t rear_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("paf_cal_check : %u\n", paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}

char rear_f2_paf_cal_data_far[PAF_2PD_CAL_INFO_SIZE] = {0,};
char rear_f2_paf_cal_data_mid[PAF_2PD_CAL_INFO_SIZE] = {0,};
static ssize_t rear_f2_paf_offset_mid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear_f2_paf_cal_data : %s\n", rear_f2_paf_cal_data_mid);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_f2_paf_cal_data_mid);
	if (rc) {
		pr_debug("data size %d\n", rc);
		return rc;
	}
	return 0;
}
static ssize_t rear_f2_paf_offset_far_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear_f2_paf_cal_data : %s\n", rear_f2_paf_cal_data_far);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_f2_paf_cal_data_far);
	if (rc) {
		pr_debug("data size %d\n", rc);
		return rc;
	}
	return 0;
}

uint32_t f2_paf_err_data_result = 0xFFFFFFFF;
static ssize_t rear_f2_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("f2_paf_cal_check : %u\n", f2_paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", f2_paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
uint32_t rear3_paf_err_data_result = 0xFFFFFFFF;
static ssize_t rear3_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear3_paf_err_data_result : %u\n", rear3_paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", rear3_paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}
#endif
uint32_t front_paf_err_data_result = 0xFFFFFFFF;
static ssize_t front_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("front_paf_err_data_result : %u\n", front_paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", front_paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}

#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
char front_af_cal_str[MAX_AF_CAL_STR_SIZE] = "";
static ssize_t front_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("front_af_cal_str : 20 %s\n", front_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "20 %s", front_af_cal_str);
	if (rc)
		return rc;

	return 0;
}
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
char front3_af_cal_str[MAX_AF_CAL_STR_SIZE] = "";
static ssize_t front3_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("front3_af_cal_str : 20 %s\n", front3_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "20 %s", front3_af_cal_str);
	if (rc)
		return rc;

	return 0;
}
#else
char front2_af_cal_str[MAX_AF_CAL_STR_SIZE] = "";
static ssize_t front2_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("front2_af_cal_str : 20 %s\n", front2_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "20 %s", front2_af_cal_str);
	if (rc)
		return rc;

	return 0;
}
#endif
#endif
#endif

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front_cam_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
#else
char front_cam_fw_ver[SYSFS_FW_VER_SIZE] = "IMX374 N\n";
#endif
static ssize_t front_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front_cam_fw_ver : %s\n", front_cam_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_cam_fw_ver, sizeof(front_cam_fw_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";
#else
char front_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "IMX374 N N\n";
#endif
static ssize_t front_camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front_cam_fw_full_ver : %s\n", front_cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_cam_fw_full_ver, sizeof(front_cam_fw_full_ver), "%s", buf);
	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
#else
char front_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "OK\n";
#endif
static ssize_t front_camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_fw_ver : %s\n", front_cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_cam_fw_user_ver, sizeof(front_cam_fw_user_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
#else
char front_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "OK\n";
#endif
static ssize_t front_camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_fw_ver : %s\n", front_cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front_cam_fw_factory_ver, sizeof(front_cam_fw_factory_ver), "%s", buf);

	return size;
}
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front2_cam_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
#else
char front2_cam_fw_ver[SYSFS_FW_VER_SIZE] = "S5K4HA N\n";
#endif
static ssize_t front2_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front2_cam_fw_ver : %s\n", front2_cam_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_ver, sizeof(front2_cam_fw_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front2_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";
#else
char front2_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "S5K4HA N N\n";
#endif
static ssize_t front2_camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front_cam_fw_full_ver : %s\n", front2_cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_full_ver, sizeof(front2_cam_fw_full_ver), "%s", buf);
	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front2_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
#else
char front2_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "OK\n";
#endif
static ssize_t front2_camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_fw_ver : %s\n", front2_cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_user_ver, sizeof(front2_cam_fw_user_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front2_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
#else
char front2_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "OK\n";
#endif
static ssize_t front2_camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_fw_ver : %s\n", front2_cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_factory_ver, sizeof(front2_cam_fw_factory_ver), "%s", buf);

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
char front3_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front3_cam_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
#else
char front3_cam_fw_ver[SYSFS_FW_VER_SIZE] = "IMX374 N\n";
#endif
static ssize_t front3_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front3_cam_fw_ver : %s\n", front3_cam_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_cam_fw_ver, sizeof(front3_cam_fw_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front3_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";
#else
char front3_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "IMX374 N N\n";
#endif
static ssize_t front3_camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front3_cam_fw_full_ver : %s\n", front3_cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_cam_fw_full_ver, sizeof(front3_cam_fw_full_ver), "%s", buf);
	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front3_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
#else
char front3_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "OK\n";
#endif
static ssize_t front3_camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_fw_ver : %s\n", front3_cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_cam_fw_user_ver, sizeof(front3_cam_fw_user_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front3_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
#else
char front3_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "OK\n";
#endif
static ssize_t front3_camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_fw_ver : %s\n", front3_cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front3_cam_fw_factory_ver, sizeof(front3_cam_fw_factory_ver), "%s", buf);

	return size;
}
#else
#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front2_cam_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
#else
char front2_cam_fw_ver[SYSFS_FW_VER_SIZE] = "IMX374 N\n";
#endif
static ssize_t front2_camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front2_cam_fw_ver : %s\n", front2_cam_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_ver, sizeof(front2_cam_fw_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front2_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";
#else
char front2_cam_fw_full_ver[SYSFS_FW_VER_SIZE] = "IMX374 N N\n";
#endif
static ssize_t front2_camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front2_cam_fw_full_ver : %s\n", front2_cam_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_full_ver, sizeof(front2_cam_fw_full_ver), "%s", buf);
	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front2_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
#else
char front2_cam_fw_user_ver[SYSFS_FW_VER_SIZE] = "OK\n";
#endif
static ssize_t front2_camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_fw_ver : %s\n", front2_cam_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_user_ver, sizeof(front2_cam_fw_user_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_EEPROM)
char front2_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";
#else
char front2_cam_fw_factory_ver[SYSFS_FW_VER_SIZE] = "OK\n";
#endif
static ssize_t front2_camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_fw_ver : %s\n", front2_cam_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(front2_cam_fw_factory_ver, sizeof(front2_cam_fw_factory_ver), "%s", buf);

	return size;
}
#endif
#endif

#if defined(CONFIG_CAMERA_SYSFS_V2)
char rear_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_info : %s\n", rear_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_cam_info, sizeof(rear_cam_info), "%s", buf);

	return size;
}

char front_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_info : %s\n", front_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front_cam_info, sizeof(front_cam_info), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
char front2_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front2_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_info : %s\n", front2_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front2_cam_info, sizeof(front2_cam_info), "%s", buf);

	return size;
}
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
char front3_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front3_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_info : %s\n", front3_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front3_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front3_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front3_cam_info, sizeof(front3_cam_info), "%s", buf);

	return size;
}
#else
char front2_cam_info[150] = "NULL\n";	//camera_info
static ssize_t front2_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_info : %s\n", front2_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", front2_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t front2_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front2_cam_info, sizeof(front2_cam_info), "%s", buf);

	return size;
}
#endif
#endif
#endif

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
uint8_t front2_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t front2_camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] front2_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3], front2_module_id[4],
	  front2_module_id[5], front2_module_id[6], front2_module_id[7], front2_module_id[8], front2_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3], front2_module_id[4],
	  front2_module_id[5], front2_module_id[6], front2_module_id[7], front2_module_id[8], front2_module_id[9]);
}
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
uint8_t front3_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t front3_camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] front3_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front3_module_id[0], front3_module_id[1], front3_module_id[2], front3_module_id[3], front3_module_id[4],
	  front3_module_id[5], front3_module_id[6], front3_module_id[7], front3_module_id[8], front3_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front3_module_id[0], front3_module_id[1], front3_module_id[2], front3_module_id[3], front3_module_id[4],
	  front3_module_id[5], front3_module_id[6], front3_module_id[7], front3_module_id[8], front3_module_id[9]);
}
#else
uint8_t front2_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t front2_camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] front2_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3], front2_module_id[4],
	  front2_module_id[5], front2_module_id[6], front2_module_id[7], front2_module_id[8], front2_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3], front2_module_id[4],
	  front2_module_id[5], front2_module_id[6], front2_module_id[7], front2_module_id[8], front2_module_id[9]);
}
#endif
#endif

char supported_camera_ids[128];
static ssize_t supported_camera_ids_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("supported_camera_ids : %s\n", supported_camera_ids);
	rc = scnprintf(buf, PAGE_SIZE, "%s", supported_camera_ids);
	if (rc)
		return rc;
	return 0;
}

static ssize_t supported_camera_ids_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(supported_camera_ids, sizeof(supported_camera_ids), "%s", buf);

	return size;
}

#define FROM_SENSOR_ID_SIZE 16
char rear_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear_sensor_id : %s\n", rear_sensor_id);
	memcpy(buf, rear_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_sensor_id, sizeof(rear_sensor_id), "%s", buf);

	return size;
}

char front_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] front_sensor_id : %s\n", front_sensor_id);
	memcpy(buf, front_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t front_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front_sensor_id, sizeof(front_sensor_id), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
char front2_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front2_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] front2_sensor_id : %s\n", front2_sensor_id);
	memcpy(buf, front2_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t front2_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front2_sensor_id, sizeof(front2_sensor_id), "%s", buf);

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
char front3_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front3_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] front3_sensor_id : %s\n", front3_sensor_id);
	memcpy(buf, front3_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t front3_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front3_sensor_id, sizeof(front3_sensor_id), "%s", buf);

	return size;
}
#else
char front2_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t front2_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] front2_sensor_id : %s\n", front2_sensor_id);
	memcpy(buf, front2_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t front2_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front2_sensor_id, sizeof(front2_sensor_id), "%s", buf);

	return size;
}
#endif
#endif

#define FROM_MTF_SIZE 54
char front_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t front_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] front_mtf_exif : %s\n", front_mtf_exif);
	memcpy(buf, front_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t front_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(front_mtf_exif, sizeof(front_mtf_exif), "%s", buf);

	return size;
}

char rear_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear_mtf_exif : %s\n", rear_mtf_exif);
	memcpy(buf, rear_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_mtf_exif, sizeof(rear_mtf_exif), "%s", buf);

	return size;
}

char rear_mtf2_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear_mtf2_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear_mtf2_exif : %s\n", rear_mtf2_exif);
	memcpy(buf, rear_mtf2_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear_mtf2_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear_mtf2_exif, sizeof(rear_mtf2_exif), "%s", buf);

	return size;
}

uint8_t rear_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t rear_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3], rear_module_id[4],
	  rear_module_id[5], rear_module_id[6], rear_module_id[7], rear_module_id[8], rear_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3], rear_module_id[4],
	  rear_module_id[5], rear_module_id[6], rear_module_id[7], rear_module_id[8], rear_module_id[9]);
}

uint8_t front_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t front_camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] front_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3], front_module_id[4],
	  front_module_id[5], front_module_id[6], front_module_id[7], front_module_id[8], front_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3], front_module_id[4],
	  front_module_id[5], front_module_id[6], front_module_id[7], front_module_id[8], front_module_id[9]);
}

#define SSRM_CAMERA_INFO_SIZE 256
char ssrm_camera_info[SSRM_CAMERA_INFO_SIZE + 1] = "\0";
static ssize_t ssrm_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_info("ssrm_camera_info : %s\n", ssrm_camera_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", ssrm_camera_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t ssrm_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("ssrm_camera_info buf : %s\n", buf);
	scnprintf(ssrm_camera_info, sizeof(ssrm_camera_info), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
char rear3_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear3_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear3_fw_ver : %s\n", rear3_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear3_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear3_fw_ver, sizeof(rear3_fw_ver), "%s", buf);

	return size;
}

char rear3_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear3_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear3_fw_full_ver : %s\n", rear3_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear3_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear3_fw_full_ver, sizeof(rear3_fw_full_ver), "%s", buf);

	return size;
}

char rear3_af_cal_str[MAX_AF_CAL_STR_SIZE] = "";
static ssize_t rear3_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear3_af_cal_str : 20 %s\n", rear3_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "20 %s", rear3_af_cal_str);
	if (rc)
		return rc;

	return 0;
}

char rear3_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear3_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_info : %s\n", rear3_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear3_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear3_cam_info, sizeof(rear3_cam_info), "%s", buf);

	return size;
}

static ssize_t rear3_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
#if defined(CONFIG_SEC_B0Q_PROJECT)
	char cam_type[] = "SONY_IMX754\n";
#elif defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
	char cam_type[] = "SLSI_S5K3K1\n";
#else
	char cam_type[] = "SLSI_S5KGW2\n";
#endif

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char rear3_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear3_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear3_mtf_exif : %s\n", rear3_mtf_exif);
	memcpy(buf, rear3_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear3_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear3_mtf_exif, sizeof(rear3_mtf_exif), "%s", buf);

	return size;
}

char rear3_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear3_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear3_sensor_id : %s\n", rear3_sensor_id);
	memcpy(buf, rear3_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear3_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear3_sensor_id, sizeof(rear3_sensor_id), "%s", buf);

	return size;
}

#define FROM_REAR_DUAL_CAL_SIZE 2060
uint8_t rear3_dual_cal[FROM_REAR_DUAL_CAL_SIZE + 1] = "\0";
static ssize_t rear3_dual_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	pr_debug("[FW_DBG] rear3_dual_cal : %s\n", rear3_dual_cal);

	if (FROM_REAR_DUAL_CAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = FROM_REAR_DUAL_CAL_SIZE;

	ret = memcpy(buf, rear3_dual_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;

}


uint32_t rear3_dual_cal_size = FROM_REAR_DUAL_CAL_SIZE;
static ssize_t rear3_dual_cal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear3_dual_cal_size : %d\n", rear3_dual_cal_size);
	rc = scnprintf(buf, PAGE_SIZE, "%d", rear3_dual_cal_size);
	if (rc)
		return rc;
	return 0;
}

DualTilt_t rear3_dual;
static ssize_t rear3_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear3 dual tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d, project_cal_type=%s\n",
		rear3_dual.x, rear3_dual.y, rear3_dual.z, rear3_dual.sx, rear3_dual.sy,
		rear3_dual.range, rear3_dual.max_err, rear3_dual.avg_err, rear3_dual.dll_ver, rear3_dual.project_cal_type);

	rc = scnprintf(buf, PAGE_SIZE, "1 %d %d %d %d %d %d %d %d %d %s\n", rear3_dual.x, rear3_dual.y,
			rear3_dual.z, rear3_dual.sx, rear3_dual.sy, rear3_dual.range,
			rear3_dual.max_err, rear3_dual.avg_err, rear3_dual.dll_ver, rear3_dual.project_cal_type);
	if (rc)
		return rc;
	return 0;
}

uint8_t rear3_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
#if defined(CONFIG_SEC_B0Q_PROJECT) || defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
static ssize_t rear3_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear3_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear3_module_id[0], rear3_module_id[1], rear3_module_id[2], rear3_module_id[3], rear3_module_id[4],
	  rear3_module_id[5], rear3_module_id[6], rear3_module_id[7], rear3_module_id[8], rear3_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear3_module_id[0], rear3_module_id[1], rear3_module_id[2], rear3_module_id[3], rear3_module_id[4],
	  rear3_module_id[5], rear3_module_id[6], rear3_module_id[7], rear3_module_id[8], rear3_module_id[9]);
}
#endif

char module3_info[SYSFS_MODULE_INFO_SIZE] = "NULL\n";
static ssize_t rear3_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] module3_info : %s\n", module3_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module3_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear3_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(module3_info, sizeof(module3_info), "%s", buf);

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_REAR_DUAL)
#if defined(CONFIG_SEC_B0Q_PROJECT)
char rear2_af_cal_str[MAX_AF_CAL_STR_SIZE] = "";
static ssize_t rear2_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear2_af_cal_str : 20 %s\n", rear2_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "20 %s", rear2_af_cal_str);
	if (rc)
		return rc;

	return 0;
}

uint32_t rear2_paf_err_data_result = 0xFFFFFFFF;
static ssize_t rear2_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear2_paf_err_data_result : %u\n", rear2_paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", rear2_paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}

#endif

char rear2_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear2_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] cam_info : %s\n", rear2_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear2_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear2_cam_info, sizeof(rear2_cam_info), "%s", buf);

	return size;
}

char rear2_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear2_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear2_mtf_exif : %s\n", rear2_mtf_exif);
	memcpy(buf, rear2_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear2_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear2_mtf_exif, sizeof(rear2_mtf_exif), "%s", buf);

	return size;
}

char rear2_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear2_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear2_sensor_id : %s\n", rear2_sensor_id);
	memcpy(buf, rear2_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear2_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear2_sensor_id, sizeof(rear2_sensor_id), "%s", buf);

	return size;
}

char module2_info[SYSFS_MODULE_INFO_SIZE] = "NULL\n";
static ssize_t rear2_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] module2_info : %s\n", module2_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module2_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(module2_info, sizeof(module2_info), "%s", buf);

	return size;
}

uint8_t rear2_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t rear2_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear2_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear2_module_id[0], rear2_module_id[1], rear2_module_id[2], rear2_module_id[3], rear2_module_id[4],
	  rear2_module_id[5], rear2_module_id[6], rear2_module_id[7], rear2_module_id[8], rear2_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear2_module_id[0], rear2_module_id[1], rear2_module_id[2], rear2_module_id[3], rear2_module_id[4],
	  rear2_module_id[5], rear2_module_id[6], rear2_module_id[7], rear2_module_id[8], rear2_module_id[9]);
}

char rear2_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear2_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	pr_debug("[FW_DBG] rear2_fw_ver : %s\n", rear2_fw_ver);

	rc = scnprintf(buf, PAGE_SIZE, "%s", rear2_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear2_fw_ver, sizeof(rear2_fw_ver), "%s", buf);

	return size;
}


char rear2_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear2_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear2_fw_user_ver : %s\n", rear2_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear2_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear2_fw_user_ver, sizeof(rear2_fw_user_ver), "%s", buf);

	return size;
}

char rear2_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear2_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear2_fw_factory_ver : %s\n", rear2_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear2_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear2_fw_factory_ver, sizeof(rear2_fw_factory_ver), "%s", buf);

	return size;
}

char rear2_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear2_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear2_fw_full_ver : %s\n", rear2_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear2_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear2_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear2_fw_full_ver, sizeof(rear2_fw_full_ver), "%s", buf);

	return size;
}

#if defined(CONFIG_SAMSUNG_REAR_DUAL)
uint8_t rear2_dual_cal[FROM_REAR_DUAL_CAL_SIZE + 1] = "\0";
static ssize_t rear2_dual_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	pr_debug("[FW_DBG] rear2_dual_cal : %s\n", rear2_dual_cal);

	if (FROM_REAR_DUAL_CAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = FROM_REAR_DUAL_CAL_SIZE;

	ret = memcpy(buf, rear2_dual_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;

}

uint32_t rear2_dual_cal_size = FROM_REAR_DUAL_CAL_SIZE;
static ssize_t rear2_dual_cal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_info("[FW_DBG] rear2_dual_cal_size : %d\n", rear2_dual_cal_size);
	rc = scnprintf(buf, PAGE_SIZE, "%d", rear2_dual_cal_size);
	if (rc)
		return rc;
	return 0;
}

DualTilt_t rear2_dual;
static ssize_t rear2_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear2 dual tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d, project_cal_type=%s\n",
		rear2_dual.x, rear2_dual.y, rear2_dual.z, rear2_dual.sx, rear2_dual.sy,
		rear2_dual.range, rear2_dual.max_err, rear2_dual.avg_err, rear2_dual.dll_ver, rear2_dual.project_cal_type);

	rc = scnprintf(buf, PAGE_SIZE, "1 %d %d %d %d %d %d %d %d %d %s\n", rear2_dual.x, rear2_dual.y,
			rear2_dual.z, rear2_dual.sx, rear2_dual.sy, rear2_dual.range,
			rear2_dual.max_err, rear2_dual.avg_err, rear2_dual.dll_ver, rear2_dual.project_cal_type);
	if (rc)
		return rc;
	return 0;
}
#endif

static ssize_t rear2_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
#if defined(CONFIG_SEC_GTS8U_PROJECT) || defined(CONFIG_SEC_GTS8P_PROJECT)
	char cam_type[] = "HYNIX_HI847\n";
#else
	char cam_type[] = "SONY_IMX563\n";
#endif
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}
#endif

#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
char rear4_fw_ver[SYSFS_FW_VER_SIZE] = "NULL NULL\n";//multi module
static ssize_t rear4_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear4_fw_ver : %s\n", rear4_fw_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear4_fw_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear4_fw_ver, sizeof(rear4_fw_ver), "%s", buf);

	return size;
}

char rear4_fw_full_ver[SYSFS_FW_VER_SIZE] = "NULL NULL NULL\n";//multi module
static ssize_t rear4_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear4_fw_full_ver : %s\n", rear4_fw_full_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear4_fw_full_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear4_fw_full_ver, sizeof(rear4_fw_full_ver), "%s", buf);

	return size;
}

char rear4_fw_user_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear4_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear4_fw_user_ver : %s\n", rear4_fw_user_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear4_fw_user_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear4_fw_user_ver, sizeof(rear4_fw_user_ver), "%s", buf);

	return size;
}

char rear4_fw_factory_ver[SYSFS_FW_VER_SIZE] = "NULL\n";//multi module
static ssize_t rear4_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear4_fw_factory_ver : %s\n", rear4_fw_factory_ver);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear4_fw_factory_ver);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(rear4_fw_factory_ver, sizeof(rear4_fw_factory_ver), "%s", buf);

	return size;
}

char rear4_af_cal_str[MAX_AF_CAL_STR_SIZE] = "";
static ssize_t rear4_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear4_af_cal_str : 20 %s\n", rear4_af_cal_str);
	rc = scnprintf(buf, PAGE_SIZE, "20 %s", rear4_af_cal_str);
	if (rc)
		return rc;

	return 0;
}

char rear4_cam_info[150] = "NULL\n";	//camera_info
static ssize_t rear4_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear4_info : %s\n", rear4_cam_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear4_cam_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear4_cam_info, sizeof(rear4_cam_info), "%s", buf);

	return size;
}

static ssize_t rear4_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char cam_type[] = "SONY_IMX754\n";

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_type);
	if (rc)
		return rc;
	return 0;
}

char rear4_mtf_exif[FROM_MTF_SIZE + 1] = "\0";
static ssize_t rear4_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear4_mtf_exif : %s\n", rear4_mtf_exif);
	memcpy(buf, rear4_mtf_exif, FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t rear4_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear4_mtf_exif, sizeof(rear4_mtf_exif), "%s", buf);

	return size;
}

char rear4_sensor_id[FROM_SENSOR_ID_SIZE + 1] = "\0";
static ssize_t rear4_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear4_sensor_id : %s\n", rear4_sensor_id);
	memcpy(buf, rear4_sensor_id, FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t rear4_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
//	scnprintf(rear4_sensor_id, sizeof(rear4_sensor_id), "%s", buf);

	return size;
}

#define FROM_REAR_DUAL_CAL_SIZE 2060
uint8_t rear4_dual_cal[FROM_REAR_DUAL_CAL_SIZE + 1] = "\0";
static ssize_t rear4_dual_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	pr_debug("[FW_DBG] rear4_dual_cal : %s\n", rear3_dual_cal);

	if (FROM_REAR_DUAL_CAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = FROM_REAR_DUAL_CAL_SIZE;

	ret = memcpy(buf, rear4_dual_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;

}

uint32_t rear4_dual_cal_size = FROM_REAR_DUAL_CAL_SIZE;
static ssize_t rear4_dual_cal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear3_dual_cal_size : %d\n", rear4_dual_cal_size);
	rc = scnprintf(buf, PAGE_SIZE, "%d", rear4_dual_cal_size);
	if (rc)
		return rc;
	return 0;
}

DualTilt_t rear4_dual;
static ssize_t rear4_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] rear4 dual tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d, project_cal_type=%s\n",
		rear4_dual.x, rear4_dual.y, rear4_dual.z, rear4_dual.sx, rear4_dual.sy,
		rear4_dual.range, rear4_dual.max_err, rear4_dual.avg_err, rear4_dual.dll_ver, rear4_dual.project_cal_type);

	rc = scnprintf(buf, PAGE_SIZE, "1 %d %d %d %d %d %d %d %d %d %s\n", rear4_dual.x, rear4_dual.y,
			rear4_dual.z, rear4_dual.sx, rear4_dual.sy, rear4_dual.range,
			rear4_dual.max_err, rear4_dual.avg_err, rear4_dual.dll_ver, rear4_dual.project_cal_type);
	if (rc)
		return rc;
	return 0;
}

uint8_t rear4_module_id[FROM_MODULE_ID_SIZE + 1] = "\0";
static ssize_t rear4_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_debug("[FW_DBG] rear4_module_id : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear4_module_id[0], rear4_module_id[1], rear4_module_id[2], rear4_module_id[3], rear4_module_id[4],
	  rear4_module_id[5], rear4_module_id[6], rear4_module_id[7], rear4_module_id[8], rear4_module_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  rear4_module_id[0], rear4_module_id[1], rear4_module_id[2], rear4_module_id[3], rear4_module_id[4],
	  rear4_module_id[5], rear4_module_id[6], rear4_module_id[7], rear4_module_id[8], rear4_module_id[9]);
}

char module4_info[SYSFS_MODULE_INFO_SIZE] = "NULL\n";
static ssize_t rear4_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] module4_info : %s\n", module4_info);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module4_info);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear4_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(module4_info, sizeof(module4_info), "%s", buf);

	return size;
}

uint32_t rear4_paf_err_data_result = 0xFFFFFFFF;
static ssize_t rear4_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("rear4_paf_err_data_result : %u\n", rear4_paf_err_data_result);
	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", rear4_paf_err_data_result);
	if (rc)
		return rc;
	return 0;
}
#endif

#if defined(CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING)
static int actuator_power = 0;
#endif
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
static int ois_power = 0;
extern struct cam_ois_ctrl_t *g_o_ctrl;
extern struct cam_actuator_ctrl_t *g_a_ctrls[SEC_SENSOR_ID_MAX];
uint32_t ois_autotest_threshold = 150;
static ssize_t ois_autotest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	uint32_t i = 0, module_mask = 0;

	pr_info("%s: E\n", __func__);

	for (i = 0; i < CUR_MODULE_NUM; i++)
		module_mask |= (1 << i);
	cam_ois_sine_wavecheck(g_o_ctrl, ois_autotest_threshold, buf, module_mask);

	pr_info("%s: X\n", __func__);

	if (strlen(buf))
		return strlen(buf);
	return 0;
}

static ssize_t ois_autotest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	uint32_t value = 0;

	if (buf == NULL || kstrtouint(buf, 10, &value))
		return -1;
	ois_autotest_threshold = value;
	return size;
}

static ssize_t ois_power_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (g_o_ctrl == NULL ||
		((g_o_ctrl->io_master_info.master_type == I2C_MASTER) &&
		(g_o_ctrl->io_master_info.client == NULL)) ||
		((g_o_ctrl->io_master_info.master_type == CCI_MASTER) &&
		(g_o_ctrl->io_master_info.cci_client == NULL)))
		return size;

	mutex_lock(&(g_o_ctrl->ois_mutex));
	if (g_o_ctrl->cam_ois_state != CAM_OIS_INIT) {
		pr_err("%s: Not in right state to control OIS power %d",
			__func__, g_o_ctrl->cam_ois_state);
		goto error;
	}

	switch (buf[0]) {
	case '0':
		if (ois_power == 0) {
			pr_info("%s: [WARNING] ois is off, skip power down", __func__);
			goto error;
		}
		cam_ois_power_down(g_o_ctrl);
		pr_info("%s: power down", __func__);
		ois_power = 0;
		break;
	case '1':
#if defined(CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING)
		if (actuator_power > 0) {
			pr_info("%s: [WARNING] actuator is used", __func__);
			goto error;
		}
#endif

		ois_power = 1;

		cam_ois_power_up(g_o_ctrl);
		msleep(200);
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
		cam_ois_mcu_init(g_o_ctrl);
#endif
		g_o_ctrl->ois_mode = 0;
		pr_info("%s: power up", __func__);
		break;

	default:
		break;
	}

error:
	mutex_unlock(&(g_o_ctrl->ois_mutex));
	return size;
}

static ssize_t gyro_calibration_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int result = 0;
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;

	result = cam_ois_gyro_sensor_calibration(g_o_ctrl, &raw_data_x, &raw_data_y, &raw_data_z);

	if (raw_data_x < 0 && raw_data_y < 0) {
		if (raw_data_z < 0) {
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
		else {
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
	} else if (raw_data_x < 0) {
		if (raw_data_z < 0) {
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld,-%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
		else {
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
	} else if (raw_data_y < 0) {
		if (raw_data_z < 0) {
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
		else {
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
	} else {
		if (raw_data_z < 0) {
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld,-%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
		else {
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result, abs(raw_data_x / 1000),
			abs(raw_data_x % 1000), abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
	}
}

static ssize_t gyro_noise_stdev_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int result = 0;
	long stdev_data_x = 0, stdev_data_y = 0;

	result = cam_ois_gyro_sensor_noise_check(g_o_ctrl, &stdev_data_x, &stdev_data_y);

	if (stdev_data_x < 0 && stdev_data_y < 0) {
		return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld\n", result, abs(stdev_data_x / 1000),
			abs(stdev_data_x % 1000), abs(stdev_data_y / 1000), abs(stdev_data_y % 1000));
	} else if (stdev_data_x < 0) {
		return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld\n", result, abs(stdev_data_x / 1000),
			abs(stdev_data_x % 1000), stdev_data_y / 1000, stdev_data_y % 1000);
	} else if (stdev_data_y < 0) {
		return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld\n", result, stdev_data_x / 1000,
			stdev_data_x % 1000, abs(stdev_data_y / 1000), abs(stdev_data_y % 1000));
	} else {
		return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld\n", result, stdev_data_x / 1000,
			stdev_data_x % 1000, stdev_data_y / 1000, stdev_data_y % 1000);
	}
}

long raw_init_x = 0, raw_init_y = 0, raw_init_z = 0;

static ssize_t gyro_selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int result_total = 0, result = 0;
	bool result_offset = 0, result_selftest = 0;
	uint32_t selftest_ret = 0;
	long raw_data_x = 0, raw_data_y = 0;
	int OIS_GYRO_OFFSET_SPEC = 10000;

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
	long raw_data_z = 0;

	result = cam_ois_offset_test(g_o_ctrl, &raw_data_x, &raw_data_y, &raw_data_z, 1);
#else
	cam_ois_offset_test(g_o_ctrl, &raw_data_x, &raw_data_y, 1);
#endif
	msleep(50);
	selftest_ret = cam_ois_self_test(g_o_ctrl);

	if (selftest_ret == 0x0)
		result_selftest = true;
	else
		result_selftest = false;

	if ((result < 0) ||
		abs(raw_init_x - raw_data_x) > OIS_GYRO_OFFSET_SPEC ||
		abs(raw_init_y - raw_data_y) > OIS_GYRO_OFFSET_SPEC ||
		abs(raw_init_z - raw_data_z) > OIS_GYRO_OFFSET_SPEC)
		result_offset = false;
	else
		result_offset = true;

	if (result_offset && result_selftest)
		result_total = 0;
	else if (!result_offset && !result_selftest)
		result_total = 3;
	else if (!result_offset)
		result_total = 1;
	else if (!result_selftest)
		result_total = 2;

	pr_info("%s: Result : 0 (success), 1 (offset fail), 2 (selftest fail) , 3 (both fail)\n", __func__);

	sprintf(buf, "Result : %d, result x = %ld.%03ld, result y = %ld.%03ld, result z = %ld.%03ld\n",
		result_total, raw_data_x / 1000, (long int)abs(raw_data_x % 1000),
		raw_data_y / 1000, (long int)abs(raw_data_y % 1000),
		raw_data_z / 1000, (long int)abs(raw_data_z % 1000));

	pr_info("%s", buf);

	if (raw_data_x < 0 && raw_data_y < 0) {
		if (raw_data_z < 0) {
			rc = scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result_total,
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
		else {
			rc = scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result_total,
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
	} else if (raw_data_x < 0) {
		if (raw_data_z < 0) {
			rc = scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld,-%ld.%03ld\n", result_total,
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
		else {
			rc = scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result_total,
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
	} else if (raw_data_y < 0) {
		if (raw_data_z < 0) {
			rc = scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result_total,
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
		else {
			rc = scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result_total,
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
	} else {
		if (raw_data_z < 0) {
			rc = scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld,-%ld.%03ld\n", result_total,
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
		else {
			rc = scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result_total,
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
	}

	if (rc)
		return rc;
	return 0;
}

static ssize_t gyro_rawdata_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	uint8_t raw_data[MAX_EFS_DATA_LENGTH] = {0, };
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;
	long efs_size = 0;

	if (ois_power) {
		if (size > MAX_EFS_DATA_LENGTH || size == 0) {
			pr_err("%s count is abnormal, count = %d", __func__, size);
			return 0;
		}

		scnprintf(raw_data, sizeof(raw_data), "%s", buf);
		efs_size = strlen(raw_data);
		cam_ois_parsing_raw_data(g_o_ctrl, raw_data, efs_size, &raw_data_x, &raw_data_y, &raw_data_z);

		raw_init_x = raw_data_x;
		raw_init_y = raw_data_y;
		raw_init_z = raw_data_z;

		pr_info("%s efs data = %s, size = %ld, raw x = %ld, raw y = %ld, raw z = %ld",
			__func__, buf, efs_size, raw_data_x, raw_data_y, raw_data_z);
	} else {
		pr_err("%s OIS power is not enabled.", __func__);
	}
	return size;
}

static ssize_t gyro_rawdata_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	long raw_data_x = 0, raw_data_y = 0, raw_data_z = 0;

	raw_data_x = raw_init_x;
	raw_data_y = raw_init_y;
	raw_data_z = raw_init_z;

	pr_info("%s: raw data x = %ld.%03ld, raw data y = %ld.%03ld, raw data z = %ld.%03ld\n", __func__,
		raw_data_x / 1000, raw_data_x % 1000,
		raw_data_y / 1000, raw_data_y % 1000,
		raw_data_z / 1000, raw_data_z % 1000);

	if (raw_data_x < 0 && raw_data_y < 0) {
		if (raw_data_z < 0) {
			rc = scnprintf(buf, PAGE_SIZE, "-%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n",
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
		else {
			rc = scnprintf(buf, PAGE_SIZE, "-%ld.%03ld,-%ld.%03ld,%ld.%03ld\n",
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
	} else if (raw_data_x < 0) {
		if (raw_data_z < 0) {
			rc = scnprintf(buf, PAGE_SIZE, "-%ld.%03ld,%ld.%03ld,-%ld.%03ld\n",
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
		else {
			rc = scnprintf(buf, PAGE_SIZE, "-%ld.%03ld,%ld.%03ld,%ld.%03ld\n",
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
	} else if (raw_data_y < 0) {
		if (raw_data_z < 0) {
			rc = scnprintf(buf, PAGE_SIZE, "%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n",
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
		else {
			rc = scnprintf(buf, PAGE_SIZE, "%ld.%03ld,-%ld.%03ld,%ld.%03ld\n",
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
	} else {
		if (raw_data_z < 0) {
			rc = scnprintf(buf, PAGE_SIZE, "%ld.%03ld,%ld.%03ld,-%ld.%03ld\n",
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
		else {
			rc = scnprintf(buf, PAGE_SIZE, "%ld.%03ld,%ld.%03ld,%ld.%03ld\n",
				(long int)abs(raw_data_x / 1000), (long int)abs(raw_data_x % 1000),
				(long int)abs(raw_data_y / 1000), (long int)abs(raw_data_y % 1000),
				(long int)abs(raw_data_z / 1000), (long int)abs(raw_data_z % 1000));
		}
	}

	if (rc)
		return rc;
	return 0;
}

char ois_fw_full[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
static ssize_t ois_fw_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_info("[FW_DBG] OIS_fw_ver : %s\n", ois_fw_full);
	rc = scnprintf(buf, PAGE_SIZE, "%s", ois_fw_full);
	if (rc)
		return rc;
	return 0;
}

static ssize_t ois_fw_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("[FW_DBG] buf : %s\n", buf);
	scnprintf(ois_fw_full, sizeof(ois_fw_full), "%s", buf);

	return size;
}

char ois_debug[40] = "NULL NULL NULL\n";
static ssize_t ois_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_info("[FW_DBG] ois_debug : %s\n", ois_debug);
	rc = scnprintf(buf, PAGE_SIZE, "%s", ois_debug);
	if (rc)
		return rc;
	return 0;
}

static ssize_t ois_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("[FW_DBG] buf: %s\n", buf);
	scnprintf(ois_debug, sizeof(ois_debug), "%s", buf);

	return size;
}

static ssize_t ois_reset_check(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	if (g_o_ctrl == NULL)
		return 0;

	pr_debug("ois reset_check : %d\n", g_o_ctrl->ois_mode);
	rc = scnprintf(buf, PAGE_SIZE, "%d", g_o_ctrl->ois_mode);
	return rc;
}

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
static ssize_t ois_hall_position_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, i = 0;
    uint32_t cnt = 0;
	uint32_t targetPosition[MAX_MODULE_NUM * 2] = { 0, 0, 0, 0, 0, 0 };
	uint32_t hallPosition[MAX_MODULE_NUM * 2] = { 0, 0, 0, 0, 0, 0};

	rc = cam_ois_read_hall_position(g_o_ctrl, targetPosition, hallPosition);

	for (i = 0; i < CUR_MODULE_NUM; i++) {
		cnt += scnprintf(buf + cnt, PAGE_SIZE, "%u,%u,",
			targetPosition[(2 * i)], targetPosition[(2 * i) + 1]);
	}

	for (i = 0; i < CUR_MODULE_NUM; i++) {
		cnt += scnprintf(buf + cnt, PAGE_SIZE, "%u,%u,",
			hallPosition[(2 * i)], hallPosition[(2 * i) + 1]);
	}
	buf[cnt--] = '\0';

	if (cnt)
		return cnt;
	return 0;
}
#endif

static ssize_t ois_set_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc = 0;
	uint32_t mode = 0;

	if (g_o_ctrl == NULL || g_o_ctrl->io_master_info.client == NULL)
		return size;

	if (buf == NULL || kstrtouint(buf, 10, &mode))
		return -1;

	if (g_o_ctrl->is_power_up == false) {
		pr_err("%s: Fail, power down state",
			__func__);
		return -1;
	}

	mutex_lock(&(g_o_ctrl->ois_mutex));
	if (g_o_ctrl->cam_ois_state != CAM_OIS_INIT) {
		pr_err("%s: Not in right state to control OIS power %d",
			__func__, g_o_ctrl->cam_ois_state);
		goto error;
	}

	rc |= cam_ois_i2c_write(g_o_ctrl, 0x00BE, 0x03,
		CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); /* select module */
	rc |= cam_ois_set_ois_mode(g_o_ctrl, mode); // Centering mode

error:
	mutex_unlock(&(g_o_ctrl->ois_mutex));
	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
extern uint8_t ois_m1_xygg[OIS_XYGG_SIZE];
extern uint8_t ois_m1_cal_mark;
int ois_gain_rear_result = 2; //0:normal, 1: No cal, 2: rear cal fail
static ssize_t ois_gain_rear_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t xgg = 0, ygg = 0;

	pr_info("[FW_DBG] ois_gain_rear_result : %d\n",
		ois_gain_rear_result);
	if (ois_gain_rear_result == 0) {
		memcpy(&xgg, &ois_m1_xygg[0], 4);
		memcpy(&ygg, &ois_m1_xygg[4], 4);
		rc = scnprintf(buf, PAGE_SIZE, "%d,0x%x,0x%x",
			ois_gain_rear_result, xgg, ygg);
	} else {
		rc = scnprintf(buf, PAGE_SIZE, "%d",
			ois_gain_rear_result);
	}
	if (rc)
		return rc;
	return 0;
}
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
extern uint8_t ois_m2_xygg[OIS_XYGG_SIZE];
extern uint8_t ois_m2_cal_mark;
int ois_gain_rear3_result = 2; //0:normal, 1: No cal, 2: rear cal fail
static ssize_t ois_gain_rear3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t xgg = 0, ygg = 0;

	pr_info("[FW_DBG] ois_gain_rear3_result : %d\n",
		ois_gain_rear3_result);
	if (ois_gain_rear3_result == 0) {
		memcpy(&xgg, &ois_m2_xygg[0], 4);
		memcpy(&ygg, &ois_m2_xygg[4], 4);
		rc = scnprintf(buf, PAGE_SIZE, "%d,0x%x,0x%x",
			ois_gain_rear3_result, xgg, ygg);
	} else {
		rc = scnprintf(buf, PAGE_SIZE, "%d",
			ois_gain_rear3_result);
	}
	if (rc)
		return rc;
	return 0;
}
#endif

#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
extern uint8_t ois_m3_xygg[OIS_XYGG_SIZE];
extern uint8_t ois_m3_cal_mark;
int ois_gain_rear4_result = 2; //0:normal, 1: No cal, 2: rear cal fail
static ssize_t ois_gain_rear4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t xgg = 0, ygg = 0;

	pr_info("[FW_DBG] ois_gain_rear4_result : %d\n",
		ois_gain_rear4_result);
	if (ois_gain_rear4_result == 0) {
		memcpy(&xgg, &ois_m3_xygg[0], 4);
		memcpy(&ygg, &ois_m3_xygg[4], 4);
		rc = scnprintf(buf, PAGE_SIZE, "%d,0x%x,0x%x",
			ois_gain_rear4_result, xgg, ygg);
	} else {
		rc = scnprintf(buf, PAGE_SIZE, "%d",
			ois_gain_rear4_result);
	}
	if (rc)
		return rc;
	return 0;
}
#endif

extern uint8_t ois_m1_xysr[OIS_XYSR_SIZE];
int ois_sr_rear_result = 2; //0:normal, 1: No cal, 2: rear cal fail
static ssize_t ois_supperssion_ratio_rear_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t xsr = 0, ysr = 0;

	pr_info("[FW_DBG] ois_sr_rear_result : %d\n",
		ois_sr_rear_result);
	if (ois_sr_rear_result == 0) {
		memcpy(&xsr, &ois_m1_xysr[0], 2);
		memcpy(&ysr, &ois_m1_xysr[2], 2);
		rc = scnprintf(buf, PAGE_SIZE, "%d,%u.%02u,%u.%02u",
			ois_sr_rear_result, (xsr / 100), (xsr % 100), (ysr / 100), (ysr % 100));
	} else {
		rc = scnprintf(buf, PAGE_SIZE, "%d",
			ois_sr_rear_result);
	}

	if (rc)
		return rc;
	return 0;
}

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
extern uint8_t ois_m2_xysr[OIS_XYSR_SIZE];
int ois_sr_rear3_result = 2; //0:normal, 1: No cal, 2: rear cal fail
static ssize_t ois_supperssion_ratio_rear3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t xsr = 0, ysr = 0;

	pr_info("[FW_DBG] ois_sr_rear3_result : %d\n",
		ois_sr_rear3_result);
	if (ois_sr_rear3_result == 0) {
		memcpy(&xsr, &ois_m2_xysr[0], 2);
		memcpy(&ysr, &ois_m2_xysr[2], 2);
		rc = scnprintf(buf, PAGE_SIZE, "%d,%u.%02u,%u.%02u",
			ois_sr_rear3_result, (xsr / 100), (xsr % 100), (ysr / 100), (ysr % 100));
	} else {
		rc = scnprintf(buf, PAGE_SIZE, "%d",
			ois_sr_rear3_result);
	}
	if (rc)
		return rc;
	return 0;
}

extern uint8_t ois_m2_cross_talk[OIS_CROSSTALK_SIZE];
int ois_m2_cross_talk_result = 2; //0:normal, 1: No cal, 2: rear cal fail
static ssize_t ois_rear3_read_cross_talk_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t xcrosstalk = 0, ycrosstalk = 0;

	pr_info("[FW_DBG] ois_tele_crosstalk_result : %d\n",
		ois_m2_cross_talk_result);
	memcpy(&xcrosstalk, &ois_m2_cross_talk[0], 2);
	memcpy(&ycrosstalk, &ois_m2_cross_talk[2], 2);
	if (ois_m2_cross_talk_result == 0) { // normal
		rc = scnprintf(buf, PAGE_SIZE, "%u.%02u,%u.%02u",
			(xcrosstalk/ 100), (xcrosstalk % 100),
			(ycrosstalk / 100), (ycrosstalk % 100));
	} else if (ois_m2_cross_talk_result == 1) { // No cal
		rc = scnprintf(buf, PAGE_SIZE, "NONE");
	} else { // read cal fail
		rc = scnprintf(buf, PAGE_SIZE, "NG");
	}

	if (rc)
		return rc;
	return 0;
}
#endif

#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
extern uint8_t ois_m3_xysr[OIS_XYSR_SIZE];
int ois_sr_rear4_result = 2; //0:normal, 1: No cal, 2: rear cal fail
static ssize_t ois_supperssion_ratio_rear4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t xsr = 0, ysr = 0;

	pr_info("[FW_DBG] ois_sr_rear4_result : %d\n",
		ois_sr_rear4_result);
	if (ois_sr_rear4_result == 0) {
		memcpy(&xsr, &ois_m3_xysr[0], 2);
		memcpy(&ysr, &ois_m3_xysr[2], 2);
		rc = scnprintf(buf, PAGE_SIZE, "%d,%u.%02u,%u.%02u",
			ois_sr_rear4_result, (xsr / 100), (xsr % 100), (ysr / 100), (ysr % 100));
	} else {
		rc = scnprintf(buf, PAGE_SIZE, "%d",
			ois_sr_rear4_result);
	}
	if (rc)
		return rc;
	return 0;
}

extern uint8_t ois_m3_cross_talk[OIS_CROSSTALK_SIZE];
int ois_m3_cross_talk_result = 2; //0:normal, 1: No cal, 2: rear cal fail
static ssize_t ois_rear4_read_cross_talk_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t xcrosstalk = 0, ycrosstalk = 0;

	pr_info("[FW_DBG] ois_m3_crosstalk_result : %d\n",
		ois_m3_cross_talk_result);
	memcpy(&xcrosstalk, &ois_m3_cross_talk[0], 2);
	memcpy(&ycrosstalk, &ois_m3_cross_talk[2], 2);
	if (ois_m3_cross_talk_result == 0) { // normal
		rc = scnprintf(buf, PAGE_SIZE, "%u.%02u,%u.%02u",
			(xcrosstalk/ 100), (xcrosstalk % 100),
			(ycrosstalk / 100), (ycrosstalk % 100));
	} else if (ois_m3_cross_talk_result == 1) { // No cal
		rc = scnprintf(buf, PAGE_SIZE, "NONE");
	} else { // read cal fail
		rc = scnprintf(buf, PAGE_SIZE, "NG");
	}

	if (rc)
		return rc;
	return 0;
}
#endif

static ssize_t ois_check_cross_talk_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint16_t result[STEP_COUNT] = { 0, };

	rc = cam_ois_check_tele_cross_talk(g_o_ctrl, result);
	if (rc < 0)
		pr_err("ois check tele cross talk fail\n");

	rc = scnprintf(buf, PAGE_SIZE, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
		(rc < 0 ? 0 : 1), result[0], result[1], result[2], result[3], result[4],
		result[5], result[6], result[7], result[8], result[9]);

	if (rc)
		return rc;
	return 0;
}

static ssize_t check_ois_valid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint16_t result[MAX_MODULE_NUM] = { 1, 1, 1 };

	rc = cam_ois_check_ois_valid_show(g_o_ctrl, result);
	if (rc < 0)
		pr_err("ois check ois valid fail\n");

	rc = scnprintf(buf, PAGE_SIZE, "%u,%u,%u\n", result[0], result[1], result[2]);

	if (rc)
		return rc;
	return 0;
}

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
static ssize_t ois_check_hall_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	uint16_t subdev_id = SEC_TELE_SENSOR;

	uint16_t result[HALL_CAL_COUNT] = { 0, };

	rc = cam_ois_read_hall_cal(g_o_ctrl, subdev_id, result);
	if (rc < 0)
		pr_err("ois check hall cal fail\n");

	rc = scnprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d",
		(rc < 0 ? 0 : 1), result[0], result[1], result[2], result[3], result[4],
		result[5], result[6], result[7]);

	if (rc)
		return rc;
	return 0;
}
#endif

static ssize_t ois_ext_clk_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t clk = 0;

	clk = cam_ois_check_ext_clk(g_o_ctrl);
	if (clk == 0)
		pr_err("ois check ext clk fail\n");

	rc = scnprintf(buf, PAGE_SIZE, "%u", clk);

	if (rc)
		return rc;
	return 0;
}

static ssize_t ois_ext_clk_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc = 0;
	uint32_t clk = 0;

	if (buf == NULL || kstrtouint(buf, 10, &clk))
		return -1;
	pr_info("new ois ext clk %u\n", clk);

	rc = cam_ois_set_ext_clk(g_o_ctrl, clk);
	if (rc < 0) {
		pr_err("ois check ext clk fail\n");
		return -1;
	}

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING)
extern struct cam_actuator_ctrl_t *g_a_ctrls[SEC_SENSOR_ID_MAX];
#endif

static ssize_t rear_actuator_power_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
#if defined(CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING)
	int i = 0, cnt = 0, rc = 0, index = 0;
	uint32_t target[] = { SEC_WIDE_SENSOR };
	cnt = ARRAY_SIZE(target);

	switch (buf[0]) {
	case '0':
		if (actuator_power == 0) {
			pr_info("%s: [WARNING] actuator is off, skip power down", __func__);
			goto error;
		}
		for (i = 0; i < cnt; i++) {
			index = target[i];
			if (g_a_ctrls[index] != NULL) {
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
				if (g_a_ctrls[index]->use_mcu)
				{
					mutex_lock(&(g_o_ctrl->ois_mutex));
					rc |= cam_ois_power_down(g_o_ctrl);
					mutex_unlock(&(g_o_ctrl->ois_mutex));
				}
				else
#endif
				{
					mutex_lock(&(g_a_ctrls[index]->actuator_mutex));
					rc |= cam_actuator_power_down(g_a_ctrls[index]);
					mutex_unlock(&(g_a_ctrls[index]->actuator_mutex));
				}
				pr_info("%s: actuator %u power down", __func__, index);
			}
		}
		if (rc < 0) {
			pr_info("%s: actuator power down fail", __func__);
			actuator_power = 0;
			return 0;
		}
		actuator_power = 0;
		break;
	case '1':
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
		if (ois_power > 0) {
			pr_info("%s: [WARNING] ois is used", __func__);
			goto error;
		}
#endif
		actuator_power = 1;

		for (i = 0; i < cnt; i++) {
			index = target[i];
			if (g_a_ctrls[index] != NULL) {
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
				if (g_a_ctrls[index]->use_mcu)
				{
					mutex_lock(&(g_o_ctrl->ois_mutex));
					cam_ois_power_up(g_o_ctrl);
					msleep(20);
					cam_ois_mcu_init(g_o_ctrl);
					mutex_unlock(&(g_o_ctrl->ois_mutex));
				}
				else
#endif
				{
					mutex_lock(&(g_a_ctrls[index]->actuator_mutex));
					cam_actuator_power_up(g_a_ctrls[index]);
					mutex_unlock(&(g_a_ctrls[index]->actuator_mutex));
				}
				cam_actuator_default_init_setting(g_a_ctrls[index]);
				pr_info("%s: actuator %u power up", __func__, index);
			}
		}
		break;

	default:
		break;
	}

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
	error:
#endif
#endif
	return size;
}

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
char mipi_string[20] = {0, };
static ssize_t front_camera_mipi_clock_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_info("front_camera_mipi_clock_show : %s\n", mipi_string);
	rc = scnprintf(buf, PAGE_SIZE, "%s\n", mipi_string);
	if (rc)
		return rc;
	return 0;
}
#endif

#if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
extern long rear_frs_test_mode;

static ssize_t rear_camera_frs_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_info("[FRS_DBG] rear_frs_test_mode : %ld\n", rear_frs_test_mode);
	rc = scnprintf(buf, PAGE_SIZE, "%ld\n", rear_frs_test_mode);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_camera_frs_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	long data = simple_strtol(buf, NULL, 10);

	pr_info("[FRS_DBG] rear_frs_test_mode : %c(%ld)\n", buf[0], data);
	rear_frs_test_mode = data;

	return size;
}
#endif

#if defined(CONFIG_CAMERA_FAC_LN_TEST) // Factory Low Noise Test
extern uint8_t factory_ln_test;
static ssize_t cam_factory_ln_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_info("[LN_TEST] factory_ln_test : %d\n", factory_ln_test);
	rc = scnprintf(buf, PAGE_SIZE, "%d\n", factory_ln_test);
	if (rc)
		return rc;
	return 0;
}
static ssize_t cam_factory_ln_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("[LN_TEST] factory_ln_test : %c\n", buf[0]);
	if (buf[0] == '1')
		factory_ln_test = 1;
	else
		factory_ln_test = 0;

	return size;
}
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
static int16_t is_hw_param_valid_module_id(char *moduleid)
{
	int i = 0;
	int32_t moduleid_cnt = 0;
	int16_t rc = HW_PARAMS_MI_VALID;

	if (moduleid == NULL) {
		CAM_ERR(CAM_UTIL, "MI_INVALID\n");
		return HW_PARAMS_MI_INVALID;
	}

	for (i = 0; i < FROM_MODULE_ID_SIZE; i++) {
		if (moduleid[i] == '\0') {
			moduleid_cnt = moduleid_cnt + 1;
		} else if ((i < 5)
				&& (!((moduleid[i] > 47 && moduleid[i] < 58)	// 0 to 9
						|| (moduleid[i] > 64 && moduleid[i] < 91)))) { // A to Z
			CAM_ERR(CAM_UTIL, "MIR_ERR_1\n");
			rc = HW_PARAMS_MIR_ERR_1;
			break;
		}
	}

	if (moduleid_cnt == FROM_MODULE_ID_SIZE) {
		CAM_ERR(CAM_UTIL, "MIR_ERR_0\n");
		rc = HW_PARAMS_MIR_ERR_0;
	}

	return rc;
}

static ssize_t rear_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;

	msm_is_sec_get_rear_hw_param(&ec_param);

	if (ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(rear_module_id);
		switch (moduelid_chk) {
		case HW_PARAMS_MI_VALID:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIR_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR_AF\":\"%d\",\"I2CR_COM\":\"%d\",\"I2CR_OIS\":\"%d\","
					"\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
					rear_module_id[0], rear_module_id[1], rear_module_id[2], rear_module_id[3],
					rear_module_id[4], rear_module_id[7], rear_module_id[8], rear_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		case HW_PARAMS_MIR_ERR_1:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIR_ID\":\"MIR_ERR\",\"I2CR_AF\":\"%d\",\"I2CR_COM\":\"%d\",\"I2CR_OIS\":\"%d\","
					"\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		default:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIR_ID\":\"MI_NO\",\"I2CR_AF\":\"%d\",\"I2CR_COM\":\"%d\",\"I2CR_OIS\":\"%d\","
					"\"I2CR_SEN\":\"%d\",\"MIPIR_COM\":\"%d\",\"MIPIR_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;
		}
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
		msm_is_sec_get_rear_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)

//#define FROM_FRONT2_DUAL_CAL_SIZE 1024

uint8_t front2_dual_cal[FROM_FRONT_DUAL_CAL_SIZE + 1] = "\0";
static ssize_t front2_dual_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int copy_size = 0;

	pr_debug("[FW_DBG] front2_dual_cal : %s\n", front2_dual_cal);

	if (FROM_FRONT_DUAL_CAL_SIZE > SYSFS_MAX_READ_SIZE)
		copy_size = SYSFS_MAX_READ_SIZE;
	else
		copy_size = FROM_FRONT_DUAL_CAL_SIZE;

	ret = memcpy(buf, front2_dual_cal, copy_size);

	if (ret)
		return copy_size;

	return 0;

}


uint32_t front2_dual_cal_size = FROM_FRONT_DUAL_CAL_SIZE;
static ssize_t front2_dual_cal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front2_dual_cal_size : %d\n", front2_dual_cal_size);
	rc = scnprintf(buf, PAGE_SIZE, "%d", front2_dual_cal_size);
	if (rc)
		return rc;
	return 0;
}

DualTilt_t front2_dual;
static ssize_t front2_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[FW_DBG] front2 dual tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d\n",
		front2_dual.x, front2_dual.y, front2_dual.z, front2_dual.sx, front2_dual.sy,
		front2_dual.range, front2_dual.max_err, front2_dual.avg_err, front2_dual.dll_ver);

	rc = scnprintf(buf, PAGE_SIZE, "1 %d %d %d %d %d %d %d %d %d\n", front2_dual.x, front2_dual.y,
			front2_dual.z, front2_dual.sx, front2_dual.sy, front2_dual.range,
			front2_dual.max_err, front2_dual.avg_err, front2_dual.dll_ver);
	if (rc)
		return rc;
	return 0;
}
#endif
static ssize_t front_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;

	msm_is_sec_get_front_hw_param(&ec_param);

	if (ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(front_module_id);

		switch (moduelid_chk) {
		case HW_PARAMS_MI_VALID:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CF_AF\":\"%d\",\"I2CF_COM\":\"%d\",\"I2CF_OIS\":\"%d\","
					"\"I2CF_SEN\":\"%d\",\"MIPIF_COM\":\"%d\",\"MIPIF_SEN\":\"%d\"\n",
					front_module_id[0], front_module_id[1], front_module_id[2], front_module_id[3],
					front_module_id[4], front_module_id[7], front_module_id[8], front_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		case HW_PARAMS_MIR_ERR_1:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF_ID\":\"MIR_ERR\",\"I2CF_AF\":\"%d\",\"I2CF_COM\":\"%d\",\"I2CF_OIS\":\"%d\","
					"\"I2CF_SEN\":\"%d\",\"MIPIF_COM\":\"%d\",\"MIPIF_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		default:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF_ID\":\"MI_NO\",\"I2CF_AF\":\"%d\",\"I2CF_COM\":\"%d\",\"I2CF_OIS\":\"%d\","
					"\"I2CF_SEN\":\"%d\",\"MIPIF_COM\":\"%d\",\"MIPIF_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;
		}
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
		msm_is_sec_get_front_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
		}
	}

	return size;
}

#if defined(CONFIG_SAMSUNG_REAR_DUAL)
static ssize_t rear2_camera_hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;

	msm_is_sec_get_rear2_hw_param(&ec_param);

	if (ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(rear2_module_id);

		switch (moduelid_chk) {
		case HW_PARAMS_MI_VALID:
			rc = sprintf(buf, "\"CAMIR2_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR2_AF\":\"%d\",\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\","
					"\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
					rear2_module_id[0], rear2_module_id[1], rear2_module_id[2], rear2_module_id[3],
					rear2_module_id[4], rear2_module_id[7], rear2_module_id[8], rear2_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		case HW_PARAMS_MIR_ERR_1:
			rc = sprintf(buf, "\"CAMIR2_ID\":\"MIR_ERR\",\"I2CR2_AF\":\"%d\",\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\","
					"\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		default:
			rc = sprintf(buf, "\"CAMIR2_ID\":\"MI_NO\",\"I2CR2_AF\":\"%d\",\"I2CR2_COM\":\"%d\",\"I2CR2_OIS\":\"%d\","
					"\"I2CR2_SEN\":\"%d\",\"MIPIR2_COM\":\"%d\",\"MIPIR2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;
		}
	}

	return rc;
}

static ssize_t rear2_camera_hw_param_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_hw_param *ec_param = NULL;

	CAM_DBG(CAM_UTIL, "[R2] buf : %s\n", buf);

	if (!strncmp(buf, "c", 1)) {
		msm_is_sec_get_rear2_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
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
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;

	msm_is_sec_get_rear3_hw_param(&ec_param);

	if (ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(rear3_module_id);

		switch (moduelid_chk) {
		case HW_PARAMS_MI_VALID:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIR3_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR3_AF\":\"%d\",\"I2CR3_COM\":\"%d\",\"I2CR3_OIS\":\"%d\","
					"\"I2CR3_SEN\":\"%d\",\"MIPIR3_COM\":\"%d\",\"MIPIR3_SEN\":\"%d\"\n",
					rear3_module_id[0], rear3_module_id[1], rear3_module_id[2], rear3_module_id[3],
					rear3_module_id[4], rear3_module_id[7], rear3_module_id[8], rear3_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		case HW_PARAMS_MIR_ERR_1:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIR3_ID\":\"MIR_ERR\",\"I2CR3_AF\":\"%d\",\"I2CR3_COM\":\"%d\",\"I2CR3_OIS\":\"%d\","
					"\"I2CR3_SEN\":\"%d\",\"MIPIR3_COM\":\"%d\",\"MIPIR3_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		default:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIR3_ID\":\"MI_NO\",\"I2CR3_AF\":\"%d\",\"I2CR3_COM\":\"%d\",\"I2CR3_OIS\":\"%d\","
					"\"I2CR3_SEN\":\"%d\",\"MIPIR3_COM\":\"%d\",\"MIPIR3_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;
		}
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
		msm_is_sec_get_rear3_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static ssize_t front2_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;

	msm_is_sec_get_front2_hw_param(&ec_param);

	if (ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(front2_module_id);

		switch (moduelid_chk) {
		case HW_PARAMS_MI_VALID:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF2_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CF2_AF\":\"%d\",\"I2CF2_COM\":\"%d\",\"I2CF2_OIS\":\"%d\","
					"\"I2CF2_SEN\":\"%d\",\"MIPIF2_COM\":\"%d\",\"MIPIF2_SEN\":\"%d\"\n",
					front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3],
					front2_module_id[4], front2_module_id[7], front2_module_id[8], front2_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		case HW_PARAMS_MIR_ERR_1:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF2_ID\":\"MIR_ERR\",\"I2CF2_AF\":\"%d\",\"I2CF2_COM\":\"%d\",\"I2CF2_OIS\":\"%d\","
					"\"I2CF2_SEN\":\"%d\",\"MIPIF2_COM\":\"%d\",\"MIPIF2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		default:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF2_ID\":\"MI_NO\",\"I2CF2_AF\":\"%d\",\"I2CF2_COM\":\"%d\",\"I2CF2_OIS\":\"%d\","
					"\"I2CF2_SEN\":\"%d\",\"MIPIF2_COM\":\"%d\",\"MIPIF2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;
		}
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
		msm_is_sec_get_front2_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
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
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;

	msm_is_sec_get_front3_hw_param(&ec_param);

	if (ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(front3_module_id);

		switch (moduelid_chk) {
		case HW_PARAMS_MI_VALID:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF2_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CF2_AF\":\"%d\",\"I2CF2_COM\":\"%d\",\"I2CF2_OIS\":\"%d\","
					"\"I2CF2_SEN\":\"%d\",\"MIPIF2_COM\":\"%d\",\"MIPIF2_SEN\":\"%d\"\n",
					front3_module_id[0], front3_module_id[1], front3_module_id[2], front3_module_id[3],
					front3_module_id[4], front3_module_id[7], front3_module_id[8], front3_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		case HW_PARAMS_MIR_ERR_1:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF2_ID\":\"MIR_ERR\",\"I2CF2_AF\":\"%d\",\"I2CF2_COM\":\"%d\",\"I2CF2_OIS\":\"%d\","
					"\"I2CF2_SEN\":\"%d\",\"MIPIF2_COM\":\"%d\",\"MIPIF2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		default:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF2_ID\":\"MI_NO\",\"I2CF2_AF\":\"%d\",\"I2CF2_COM\":\"%d\",\"I2CF2_OIS\":\"%d\","
					"\"I2CF2_SEN\":\"%d\",\"MIPIF2_COM\":\"%d\",\"MIPIF2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;
		}
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
		msm_is_sec_get_front3_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#else
static ssize_t front2_camera_hw_param_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;
	int16_t moduelid_chk = 0;
	struct cam_hw_param *ec_param = NULL;

	msm_is_sec_get_front2_hw_param(&ec_param);

	if (ec_param != NULL) {
		moduelid_chk = is_hw_param_valid_module_id(front2_module_id);

		switch (moduelid_chk) {
		case HW_PARAMS_MI_VALID:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF2_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CF2_AF\":\"%d\",\"I2CF2_COM\":\"%d\",\"I2CF2_OIS\":\"%d\","
					"\"I2CF2_SEN\":\"%d\",\"MIPIF2_COM\":\"%d\",\"MIPIF2_SEN\":\"%d\"\n",
					front2_module_id[0], front2_module_id[1], front2_module_id[2], front2_module_id[3],
					front2_module_id[4], front2_module_id[7], front2_module_id[8], front2_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		case HW_PARAMS_MIR_ERR_1:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF2_ID\":\"MIR_ERR\",\"I2CF2_AF\":\"%d\",\"I2CF2_COM\":\"%d\",\"I2CF2_OIS\":\"%d\","
					"\"I2CF2_SEN\":\"%d\",\"MIPIF2_COM\":\"%d\",\"MIPIF2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;

		default:
			rc = scnprintf(buf, PAGE_SIZE, "\"CAMIF2_ID\":\"MI_NO\",\"I2CF2_AF\":\"%d\",\"I2CF2_COM\":\"%d\",\"I2CF2_OIS\":\"%d\","
					"\"I2CF2_SEN\":\"%d\",\"MIPIF2_COM\":\"%d\",\"MIPIF2_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_comp_err_cnt, ec_param->i2c_ois_err_cnt,
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_comp_err_cnt,
					ec_param->mipi_sensor_err_cnt);
			break;
		}
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
		msm_is_sec_get_front2_hw_param(&ec_param);
		if (ec_param != NULL) {
			msm_is_sec_init_err_cnt_file(ec_param);
		}
	}

	return size;
}
#endif
#endif
#endif

ssize_t rear_flash_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
#if IS_REACHABLE(CONFIG_LEDS_S2MPB02)
	s2mpb02_store(buf);
#elif defined(CONFIG_LEDS_KTD2692)
	ktd2692_store(buf);
#elif defined(CONFIG_SAMSUNG_PMIC_FLASH)
        flash_power_store(dev, attr, buf, count);
#endif
	return count;
}

ssize_t rear_flash_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if IS_REACHABLE(CONFIG_LEDS_S2MPB02)
	return s2mpb02_show(buf);
#elif defined(CONFIG_LEDS_KTD2692)
	return ktd2692_show(buf);
#else
	return 0;
#endif
}

static DEVICE_ATTR(rear_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	rear_flash_show, rear_flash_store);

#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
static DEVICE_ATTR(ssm_frame_id, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_ssm_frame_id_show, rear_ssm_frame_id_store);
static DEVICE_ATTR(ssm_gmc_state, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_ssm_gmc_state_show, rear_ssm_gmc_state_store);
static DEVICE_ATTR(ssm_flicker_state, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_ssm_flicker_state_show, rear_ssm_flicker_state_store);
#endif

static DEVICE_ATTR(rear_camtype, S_IRUGO, rear_type_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_show, rear_firmware_store);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_user_show, rear_firmware_user_store);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_factory_show, rear_firmware_factory_store);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_full_show, rear_firmware_full_store);
static DEVICE_ATTR(rear_camfw_load, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_load_show, rear_firmware_load_store);
static DEVICE_ATTR(rear_calcheck, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_cal_data_check_show, rear_cal_data_check_store);
static DEVICE_ATTR(rear_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_module_info_show, rear_module_info_store);
static DEVICE_ATTR(isp_core, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_isp_core_check_show, rear_isp_core_check_store);
static DEVICE_ATTR(rear_afcal, S_IRUGO, rear_afcal_show, NULL);
static DEVICE_ATTR(rear_paf_offset_far, S_IRUGO,
	rear_paf_offset_far_show, NULL);
static DEVICE_ATTR(rear_paf_offset_mid, S_IRUGO,
	rear_paf_offset_mid_show, NULL);
static DEVICE_ATTR(rear_paf_cal_check, S_IRUGO,
	rear_paf_cal_check_show, NULL);
static DEVICE_ATTR(rear_f2_paf_offset_far, S_IRUGO,
	rear_f2_paf_offset_far_show, NULL);
static DEVICE_ATTR(rear_f2_paf_offset_mid, S_IRUGO,
	rear_f2_paf_offset_mid_show, NULL);
static DEVICE_ATTR(rear_f2_paf_cal_check, S_IRUGO,
	rear_f2_paf_cal_check_show, NULL);
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
static DEVICE_ATTR(front_afcal, S_IRUGO, front_afcal_show, NULL);
#endif
static DEVICE_ATTR(front_camtype, S_IRUGO, front_camera_type_show, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_firmware_show, front_camera_firmware_store);
static DEVICE_ATTR(front_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	front_camera_firmware_full_show, front_camera_firmware_full_store);
static DEVICE_ATTR(front_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_firmware_user_show, front_camera_firmware_user_store);
static DEVICE_ATTR(front_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_firmware_factory_show, front_camera_firmware_factory_store);
#if defined (CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(rear_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_camera_info_show, rear_camera_info_store);
static DEVICE_ATTR(front_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front_camera_info_show, front_camera_info_store);
#endif
static DEVICE_ATTR(front_paf_cal_check, S_IRUGO,
	front_paf_cal_check_show, NULL);
static DEVICE_ATTR(front_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front_module_info_show, front_module_info_store);
static DEVICE_ATTR(rear_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_sensorid_exif_show, rear_sensorid_exif_store);
static DEVICE_ATTR(front_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front_sensorid_exif_show, front_sensorid_exif_store);
static DEVICE_ATTR(rear_moduleid, S_IRUGO, rear_moduleid_show, NULL);
static DEVICE_ATTR(front_moduleid, S_IRUGO, front_camera_moduleid_show, NULL);
static DEVICE_ATTR(front_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front_mtf_exif_show, front_mtf_exif_store);
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
static DEVICE_ATTR(front_mipi_clock, S_IRUGO, front_camera_mipi_clock_show, NULL);
#endif
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front2_camtype, S_IRUGO, front2_camera_type_show, NULL);
static DEVICE_ATTR(front2_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_firmware_show, front2_camera_firmware_store);
static DEVICE_ATTR(front2_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	front2_camera_firmware_full_show, front2_camera_firmware_full_store);
static DEVICE_ATTR(front2_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_firmware_user_show, front2_camera_firmware_user_store);
static DEVICE_ATTR(front2_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_firmware_factory_show, front2_camera_firmware_factory_store);
static DEVICE_ATTR(front2_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_module_info_show, front2_module_info_store);
#if defined (CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(front2_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_info_show, front2_camera_info_store);
#endif
static DEVICE_ATTR(front2_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_sensorid_exif_show, front2_sensorid_exif_store);
#endif
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front2_moduleid, S_IRUGO, front2_camera_moduleid_show, NULL);
#endif

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front3_camtype, S_IRUGO, front3_camera_type_show, NULL);
static DEVICE_ATTR(front3_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_firmware_show, front3_camera_firmware_store);
static DEVICE_ATTR(front3_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	front3_camera_firmware_full_show, front3_camera_firmware_full_store);
static DEVICE_ATTR(front3_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_firmware_user_show, front3_camera_firmware_user_store);
static DEVICE_ATTR(front3_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_firmware_factory_show, front3_camera_firmware_factory_store);
static DEVICE_ATTR(front3_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_module_info_show, front3_module_info_store);
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
static DEVICE_ATTR(front3_afcal, S_IRUGO, front3_afcal_show, NULL);
#endif
static DEVICE_ATTR(front3_moduleid, S_IRUGO, front3_camera_moduleid_show, NULL);
static DEVICE_ATTR(SVC_upper_module, S_IRUGO, front3_camera_moduleid_show, NULL);
#if defined (CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(front3_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_camera_info_show, front3_camera_info_store);
#endif
static DEVICE_ATTR(front3_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front3_sensorid_exif_show, front3_sensorid_exif_store);
#else
static DEVICE_ATTR(front2_camtype, S_IRUGO, front2_camera_type_show, NULL);
static DEVICE_ATTR(front2_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_firmware_show, front2_camera_firmware_store);
static DEVICE_ATTR(front2_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	front2_camera_firmware_full_show, front2_camera_firmware_full_store);
static DEVICE_ATTR(front2_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_firmware_user_show, front2_camera_firmware_user_store);
static DEVICE_ATTR(front2_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_firmware_factory_show, front2_camera_firmware_factory_store);
static DEVICE_ATTR(front2_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_module_info_show, front2_module_info_store);
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
static DEVICE_ATTR(front2_afcal, S_IRUGO, front2_afcal_show, NULL);
#endif
static DEVICE_ATTR(front2_moduleid, S_IRUGO, front2_camera_moduleid_show, NULL);
static DEVICE_ATTR(SVC_upper_module, S_IRUGO, front2_camera_moduleid_show, NULL);
#if defined (CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(front2_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_info_show, front2_camera_info_store);
#endif
static DEVICE_ATTR(front2_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_sensorid_exif_show, front2_sensorid_exif_store);
#endif
#endif

static DEVICE_ATTR(supported_cameraIds, S_IRUGO|S_IWUSR|S_IWGRP,
	supported_camera_ids_show, supported_camera_ids_store);

static DEVICE_ATTR(rear_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_mtf_exif_show, rear_mtf_exif_store);
static DEVICE_ATTR(rear_mtf2_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_mtf2_exif_show, rear_mtf2_exif_store);
static DEVICE_ATTR(SVC_rear_module, S_IRUGO, rear_moduleid_show, NULL);
static DEVICE_ATTR(SVC_front_module, S_IRUGO, front_camera_moduleid_show, NULL);
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(SVC_front_module2, S_IRUGO, front2_camera_moduleid_show, NULL);
#endif
static DEVICE_ATTR(ssrm_camera_info, S_IRUGO|S_IWUSR|S_IWGRP,
	ssrm_camera_info_show, ssrm_camera_info_store);

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
static DEVICE_ATTR(rear3_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_firmware_show, rear3_firmware_store);
static DEVICE_ATTR(rear3_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_firmware_full_show, rear3_firmware_full_store);
#if defined(CONFIG_SEC_B0Q_PROJECT) || defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
static DEVICE_ATTR(rear3_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_firmware_user_show, rear3_firmware_user_store);
static DEVICE_ATTR(rear3_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_firmware_factory_show, rear3_firmware_factory_store);
#endif
static DEVICE_ATTR(rear3_afcal, S_IRUGO, rear3_afcal_show, NULL);
static DEVICE_ATTR(rear3_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_camera_info_show, rear3_camera_info_store);
static DEVICE_ATTR(rear3_camtype, S_IRUGO, rear3_type_show, NULL);
static DEVICE_ATTR(rear3_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_module_info_show, rear3_module_info_store);
static DEVICE_ATTR(rear3_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_mtf_exif_show, rear3_mtf_exif_store);
static DEVICE_ATTR(rear3_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear3_sensorid_exif_show, rear3_sensorid_exif_store);
static DEVICE_ATTR(rear3_dualcal, S_IRUGO, rear3_dual_cal_show, NULL);
static DEVICE_ATTR(rear3_dualcal_size, S_IRUGO, rear3_dual_cal_size_show, NULL);

static DEVICE_ATTR(rear3_tilt, S_IRUGO, rear3_tilt_show, NULL);
static DEVICE_ATTR(rear3_paf_cal_check, S_IRUGO,
	rear3_paf_cal_check_show, NULL);
#if defined(CONFIG_SEC_B0Q_PROJECT) || defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
static DEVICE_ATTR(rear3_moduleid, S_IRUGO, rear3_moduleid_show, NULL);
static DEVICE_ATTR(SVC_rear_module3, S_IRUGO, rear3_moduleid_show, NULL);
#endif
#endif
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
#if defined(CONFIG_SEC_B0Q_PROJECT)
static DEVICE_ATTR(rear2_afcal, S_IRUGO, rear2_afcal_show, NULL);
static DEVICE_ATTR(rear2_paf_cal_check, S_IRUGO,
	rear2_paf_cal_check_show, NULL);
#endif
static DEVICE_ATTR(rear2_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_camera_info_show, rear2_camera_info_store);
static DEVICE_ATTR(rear2_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_module_info_show, rear2_module_info_store);
static DEVICE_ATTR(rear2_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_mtf_exif_show, rear2_mtf_exif_store);
static DEVICE_ATTR(rear2_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_sensorid_exif_show, rear2_sensorid_exif_store);
static DEVICE_ATTR(rear2_moduleid, S_IRUGO,
	rear2_moduleid_show, NULL);
static DEVICE_ATTR(rear2_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_firmware_show, rear2_firmware_store);
static DEVICE_ATTR(rear2_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_firmware_user_show, rear2_firmware_user_store);
static DEVICE_ATTR(rear2_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_firmware_factory_show, rear2_firmware_factory_store);
static DEVICE_ATTR(rear2_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear2_firmware_full_show, rear2_firmware_full_store);
static DEVICE_ATTR(SVC_rear_module2, S_IRUGO, rear2_moduleid_show, NULL);
static DEVICE_ATTR(rear2_camtype, S_IRUGO, rear2_type_show, NULL);
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
static DEVICE_ATTR(rear2_dualcal, S_IRUGO, rear2_dual_cal_show, NULL);
static DEVICE_ATTR(rear2_dualcal_size, S_IRUGO, rear2_dual_cal_size_show, NULL);
static DEVICE_ATTR(rear2_tilt, S_IRUGO, rear2_tilt_show, NULL);
#endif
#endif

#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
static DEVICE_ATTR(rear4_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_firmware_show, rear4_firmware_store);
static DEVICE_ATTR(rear4_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_firmware_full_show, rear4_firmware_full_store);
static DEVICE_ATTR(rear4_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_firmware_user_show, rear4_firmware_user_store);
static DEVICE_ATTR(rear4_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_firmware_factory_show, rear4_firmware_factory_store);
static DEVICE_ATTR(rear4_afcal, S_IRUGO, rear4_afcal_show, NULL);
static DEVICE_ATTR(rear4_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_camera_info_show, rear4_camera_info_store);
static DEVICE_ATTR(rear4_camtype, S_IRUGO, rear4_type_show, NULL);
static DEVICE_ATTR(rear4_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_module_info_show, rear4_module_info_store);
static DEVICE_ATTR(rear4_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_mtf_exif_show, rear4_mtf_exif_store);
static DEVICE_ATTR(rear4_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear4_sensorid_exif_show, rear4_sensorid_exif_store);
static DEVICE_ATTR(rear4_dualcal, S_IRUGO, rear4_dual_cal_show, NULL);
static DEVICE_ATTR(rear4_dualcal_size, S_IRUGO, rear4_dual_cal_size_show, NULL);

static DEVICE_ATTR(rear4_tilt, S_IRUGO, rear4_tilt_show, NULL);
static DEVICE_ATTR(rear4_paf_cal_check, S_IRUGO,
	rear4_paf_cal_check_show, NULL);
static DEVICE_ATTR(rear4_moduleid, S_IRUGO, rear4_moduleid_show, NULL);
static DEVICE_ATTR(SVC_rear_module4, S_IRUGO, rear4_moduleid_show, NULL);
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
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front2_hwparam, S_IRUGO|S_IWUSR|S_IWGRP,
	front2_camera_hw_param_show, front2_camera_hw_param_store);

static DEVICE_ATTR(front2_dualcal, S_IRUGO, front2_dual_cal_show, NULL);
static DEVICE_ATTR(front2_dualcal_size, S_IRUGO, front2_dual_cal_size_show, NULL);
static DEVICE_ATTR(front2_tilt, S_IRUGO, front2_tilt_show, NULL);

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

static DEVICE_ATTR(rear_actuator_power, S_IWUSR|S_IWGRP, NULL, rear_actuator_power_store);

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
static DEVICE_ATTR(ois_power, S_IWUSR, NULL, ois_power_store);
static DEVICE_ATTR(autotest, S_IRUGO|S_IWUSR|S_IWGRP, ois_autotest_show, ois_autotest_store);
static DEVICE_ATTR(calibrationtest, S_IRUGO, gyro_calibration_show, NULL);
static DEVICE_ATTR(ois_noise_stdev, S_IRUGO, gyro_noise_stdev_show, NULL);
static DEVICE_ATTR(selftest, S_IRUGO, gyro_selftest_show, NULL);
static DEVICE_ATTR(ois_rawdata, S_IRUGO|S_IWUSR|S_IWGRP, gyro_rawdata_test_show, gyro_rawdata_test_store);
static DEVICE_ATTR(oisfw, S_IRUGO|S_IWUSR|S_IWGRP, ois_fw_full_show, ois_fw_full_store);
static DEVICE_ATTR(ois_exif, S_IRUGO|S_IWUSR|S_IWGRP, ois_exif_show, ois_exif_store);
static DEVICE_ATTR(reset_check, S_IRUGO, ois_reset_check, NULL);
static DEVICE_ATTR(ois_set_mode, S_IWUSR, NULL, ois_set_mode_store);
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
static DEVICE_ATTR(ois_hall_position, S_IRUGO, ois_hall_position_show, NULL);
#endif
#endif
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
static DEVICE_ATTR(ois_gain_rear, S_IRUGO, ois_gain_rear_show, NULL);
static DEVICE_ATTR(ois_supperssion_ratio_rear, S_IRUGO, ois_supperssion_ratio_rear_show, NULL);
static DEVICE_ATTR(check_hall_cal, S_IRUGO, ois_check_hall_cal_show, NULL);
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
static DEVICE_ATTR(ois_gain_rear3, S_IRUGO, ois_gain_rear3_show, NULL);
static DEVICE_ATTR(ois_supperssion_ratio_rear3, S_IRUGO, ois_supperssion_ratio_rear3_show, NULL);
static DEVICE_ATTR(rear3_read_cross_talk, S_IRUGO, ois_rear3_read_cross_talk_show, NULL);
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
static DEVICE_ATTR(ois_gain_rear4, S_IRUGO, ois_gain_rear4_show, NULL);
static DEVICE_ATTR(ois_supperssion_ratio_rear4, S_IRUGO, ois_supperssion_ratio_rear4_show, NULL);
static DEVICE_ATTR(rear4_read_cross_talk, S_IRUGO, ois_rear4_read_cross_talk_show, NULL);
#endif

static DEVICE_ATTR(check_cross_talk, S_IRUGO, ois_check_cross_talk_show, NULL);
static DEVICE_ATTR(check_ois_valid, S_IRUGO, check_ois_valid_show, NULL);
static DEVICE_ATTR(ois_ext_clk, S_IRUGO|S_IWUSR|S_IWGRP, ois_ext_clk_show, ois_ext_clk_store);
#endif

#if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
static DEVICE_ATTR(rear_frs_test, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_camera_frs_test_show, rear_camera_frs_test_store);
#endif
#if defined(CONFIG_CAMERA_FAC_LN_TEST)
static DEVICE_ATTR(cam_ln_test, S_IRUGO|S_IWUSR|S_IWGRP,
	cam_factory_ln_test_show, cam_factory_ln_test_store);
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
char af_position_value[128] = "0\n";
static ssize_t af_position_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[syscamera] af_position_show : %s\n", af_position_value);
	rc = snprintf(buf, PAGE_SIZE, "%s", af_position_value);
	if (rc)
		return rc;
	return 0;
}

static ssize_t af_position_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(af_position_value, sizeof(af_position_value), "%s", buf);
	return size;
}
static DEVICE_ATTR(af_position, S_IRUGO|S_IWUSR|S_IWGRP,
	af_position_show, af_position_store);

char dual_fallback_value[SYSFS_FW_VER_SIZE] = "0\n";
static ssize_t dual_fallback_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	pr_debug("[syscamera] dual_fallback_show : %s\n", dual_fallback_value);
	rc = scnprintf(buf, PAGE_SIZE, "%s", dual_fallback_value);
	if (rc)
		return rc;
	return 0;
}

static ssize_t dual_fallback_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("[FW_DBG] buf : %s\n", buf);
	scnprintf(dual_fallback_value, sizeof(dual_fallback_value), "%s", buf);
	return size;
}
static DEVICE_ATTR(fallback, S_IRUGO|S_IWUSR|S_IWGRP,
	dual_fallback_show, dual_fallback_store);
#endif

struct device		*cam_dev_flash;
struct device		*cam_dev_rear;
struct device		*cam_dev_front;
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
struct device		*cam_dev_af;
struct device		*cam_dev_dual;
#endif
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
struct device		*cam_dev_ois;
#endif
#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
struct device		*cam_dev_ssm;
#endif

const struct device_attribute *flash_attrs[] = {
    &dev_attr_rear_flash,
	NULL, // DO NOT REMOVE
};

const struct device_attribute *ssm_attrs[] = {
#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
	&dev_attr_ssm_frame_id,
	&dev_attr_ssm_gmc_state,
	&dev_attr_ssm_flicker_state,
#endif
	NULL, // DO NOT REMOVE
};

const struct device_attribute *rear_attrs[] = {
	&dev_attr_rear_camtype,
	&dev_attr_rear_camfw,
	&dev_attr_rear_checkfw_user,
	&dev_attr_rear_checkfw_factory,
	&dev_attr_rear_camfw_full,
	&dev_attr_rear_camfw_load,
	&dev_attr_rear_calcheck,
	&dev_attr_rear_moduleinfo,
	&dev_attr_isp_core,
#if defined(CONFIG_CAMERA_SYSFS_V2)
	&dev_attr_rear_caminfo,
#endif
	&dev_attr_rear_afcal,
	&dev_attr_rear_paf_offset_far,
	&dev_attr_rear_paf_offset_mid,
	&dev_attr_rear_paf_cal_check,
	&dev_attr_rear_f2_paf_offset_far,
	&dev_attr_rear_f2_paf_offset_mid,
	&dev_attr_rear_f2_paf_cal_check,
	&dev_attr_rear_sensorid_exif,
	&dev_attr_rear_moduleid,
	&dev_attr_rear_mtf_exif,
	&dev_attr_rear_mtf2_exif,
	&dev_attr_ssrm_camera_info,
	&dev_attr_rear_actuator_power,
	&dev_attr_supported_cameraIds,
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	&dev_attr_rear3_camfw,
	&dev_attr_rear3_camfw_full,
	&dev_attr_rear3_afcal,
	&dev_attr_rear3_tilt,
	&dev_attr_rear3_caminfo,
	&dev_attr_rear3_camtype,
	&dev_attr_rear3_moduleinfo,
	&dev_attr_rear3_mtf_exif,
	&dev_attr_rear3_sensorid_exif,
	&dev_attr_rear3_dualcal,
	&dev_attr_rear3_dualcal_size,
	&dev_attr_rear3_paf_cal_check,
#if defined(CONFIG_SEC_B0Q_PROJECT) || defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
	&dev_attr_rear3_moduleid,
	&dev_attr_rear3_checkfw_user,
	&dev_attr_rear3_checkfw_factory,
#endif
#endif
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
	&dev_attr_rear2_caminfo,
	&dev_attr_rear2_moduleinfo,
	&dev_attr_rear2_sensorid_exif,
	&dev_attr_rear2_mtf_exif,
	&dev_attr_rear2_moduleid,
	&dev_attr_rear2_camfw,
	&dev_attr_rear2_checkfw_user,
	&dev_attr_rear2_checkfw_factory,
#if defined(CONFIG_SEC_B0Q_PROJECT)
	&dev_attr_rear2_afcal,
	&dev_attr_rear2_paf_cal_check,
#endif
	&dev_attr_rear2_camfw_full,
	&dev_attr_rear2_dualcal,
	&dev_attr_rear2_dualcal_size,
	&dev_attr_rear2_tilt,
	&dev_attr_rear2_camtype,
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
	&dev_attr_rear4_camfw,
	&dev_attr_rear4_camfw_full,
	&dev_attr_rear4_moduleid,
	&dev_attr_rear4_checkfw_user,
	&dev_attr_rear4_checkfw_factory,
	&dev_attr_rear4_afcal,
	&dev_attr_rear4_tilt,
	&dev_attr_rear4_caminfo,
	&dev_attr_rear4_camtype,
	&dev_attr_rear4_moduleinfo,
	&dev_attr_rear4_mtf_exif,
	&dev_attr_rear4_sensorid_exif,
	&dev_attr_rear4_dualcal,
	&dev_attr_rear4_dualcal_size,
	&dev_attr_rear4_paf_cal_check,
#endif
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	&dev_attr_rear_hwparam,
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
	&dev_attr_rear2_hwparam,
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	&dev_attr_rear3_hwparam,
#endif
#endif
#endif
#if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
	&dev_attr_rear_frs_test,
#endif
#if defined(CONFIG_CAMERA_FAC_LN_TEST)
	&dev_attr_cam_ln_test,
#endif
	NULL, // DO NOT REMOVE
};

const struct device_attribute *front_attrs[] = {
	&dev_attr_front_camtype,
	&dev_attr_front_camfw,
	&dev_attr_front_camfw_full,
	&dev_attr_front_checkfw_user,
	&dev_attr_front_checkfw_factory,
	&dev_attr_front_moduleinfo,
	&dev_attr_front_paf_cal_check,
#if defined(CONFIG_CAMERA_SYSFS_V2)
	&dev_attr_front_caminfo,
#endif
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
	&dev_attr_front_afcal,
#endif
	&dev_attr_front_sensorid_exif,
	&dev_attr_front_moduleid,
	&dev_attr_front_mtf_exif,
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	&dev_attr_front2_camtype,
	&dev_attr_front2_camfw,
	&dev_attr_front2_camfw_full,
	&dev_attr_front2_checkfw_user,
	&dev_attr_front2_checkfw_factory,
	&dev_attr_front2_moduleinfo,
#if defined(CONFIG_CAMERA_SYSFS_V2)
	&dev_attr_front2_caminfo,
#endif
	&dev_attr_front2_sensorid_exif,
#endif
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	&dev_attr_front2_moduleid,
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	&dev_attr_front3_camtype,
	&dev_attr_front3_camfw,
	&dev_attr_front3_camfw_full,
	&dev_attr_front3_checkfw_user,
	&dev_attr_front3_checkfw_factory,
	&dev_attr_front3_moduleinfo,
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
	&dev_attr_front3_afcal,
#endif
#if defined(CONFIG_CAMERA_SYSFS_V2)
	&dev_attr_front3_caminfo,
#endif
	&dev_attr_front3_moduleid,
	&dev_attr_front3_sensorid_exif,
#else
	&dev_attr_front2_camtype,
	&dev_attr_front2_camfw,
	&dev_attr_front2_camfw_full,
	&dev_attr_front2_checkfw_user,
	&dev_attr_front2_checkfw_factory,
	&dev_attr_front2_moduleinfo,
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
	&dev_attr_front2_afcal,
#endif
#if defined(CONFIG_CAMERA_SYSFS_V2)
	&dev_attr_front2_caminfo,
#endif
	&dev_attr_front2_moduleid,
	&dev_attr_front2_sensorid_exif,
#endif
#endif
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	&dev_attr_front_hwparam,
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	&dev_attr_front2_hwparam,
	&dev_attr_front2_dualcal,
	&dev_attr_front2_dualcal_size,
	&dev_attr_front2_tilt,
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	&dev_attr_front3_hwparam,
#else
	&dev_attr_front2_hwparam,
#endif
#endif
#endif
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
	&dev_attr_front_mipi_clock,
#endif
	NULL, // DO NOT REMOVE
};

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
const struct device_attribute *af_attrs[] = {
	&dev_attr_af_position,
	NULL, // DO NOT REMOVE
};
#endif

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
const struct device_attribute *dual_attrs[] = {
	&dev_attr_fallback,
	NULL, // DO NOT REMOVE
};
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
const struct device_attribute *ois_attrs[] = {
	&dev_attr_ois_power,
	&dev_attr_autotest,
	&dev_attr_selftest,
	&dev_attr_ois_rawdata,
	&dev_attr_oisfw,
	&dev_attr_ois_exif,
	&dev_attr_calibrationtest,
	&dev_attr_ois_noise_stdev,
	&dev_attr_reset_check,
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	&dev_attr_ois_hall_position,
#endif
	&dev_attr_ois_set_mode,
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
	&dev_attr_ois_gain_rear,
	&dev_attr_ois_supperssion_ratio_rear,
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	&dev_attr_ois_gain_rear3,
	&dev_attr_ois_supperssion_ratio_rear3,
	&dev_attr_rear3_read_cross_talk,
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
	&dev_attr_ois_gain_rear4,
	&dev_attr_ois_supperssion_ratio_rear4,
	&dev_attr_rear4_read_cross_talk,
#endif
	&dev_attr_check_cross_talk,
	&dev_attr_check_ois_valid,
	&dev_attr_ois_ext_clk,
	&dev_attr_check_hall_cal,
#endif
	NULL, // DO NOT REMOVE
};
#endif

static struct attribute *svc_cam_attrs[] = {
	&dev_attr_SVC_rear_module.attr,
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
	&dev_attr_SVC_rear_module2.attr,
#endif
#if defined(CONFIG_SEC_B0Q_PROJECT) || defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
	&dev_attr_SVC_rear_module3.attr,
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
	&dev_attr_SVC_rear_module4.attr,
#endif
	&dev_attr_SVC_front_module.attr,
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	&dev_attr_SVC_front_module2.attr,
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
	&dev_attr_SVC_upper_module.attr,
#endif
	NULL, // DO NOT REMOVE
};

static struct attribute_group svc_cam_group = {
	.attrs = svc_cam_attrs,
};

static const struct attribute_group *svc_cam_groups[] = {
	&svc_cam_group,
	NULL, // DO NOT REMOVE
};

static void svc_cam_release(struct device *dev)
{
	kfree(dev);
}

int svc_cheating_prevent_device_file_create(void)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct device *dev;
	int err;

	/* To find SVC kobject */
	struct kobject *top_kobj = NULL;

	if(is_dev == NULL) {
		pr_err("[SVC] Error! cam-cci-driver module does not exist\n");
		return -ENODEV;
	}

	top_kobj = &is_dev->kobj.kset->kobj;

	svc_sd = sysfs_get_dirent(top_kobj->sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", top_kobj);
		if (IS_ERR_OR_NULL(data)) {
			pr_info("[SVC] Failed to create sys/devices/svc already exist svc : 0x%pK\n", data);
		} else {
			pr_info("[SVC] Success to create sys/devices/svc svc : 0x%pK\n", data);
		}
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("[SVC] Success to find svc_sd : 0x%pK SVC : 0x%pK\n", svc_sd, data);
	}

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!dev) {
		pr_err("[SVC] Error allocating svc_ap device\n");
		return -ENOMEM;
	}

	err = dev_set_name(dev, "Camera");
	if (err < 0) {
		pr_err("[SVC] Error dev_set_name\n");
		goto err_name;
	}

	dev->kobj.parent = data;
	dev->groups = svc_cam_groups;
	dev->release = svc_cam_release;

	err = device_register(dev);
	if (err < 0) {
		pr_err("[SVC] Error device_register\n");
		goto err_dev_reg;
	}

	return 0;

err_dev_reg:
	put_device(dev);
err_name:
	kfree(dev);
	dev = NULL;
	return err;
}

int cam_device_create_files(struct device *device,
	const struct device_attribute **attrs)
{
	int ret = 0, i = 0;

	if (device == NULL) {
		pr_err("device is null!\n");
		return ret;
	}

	for (i = 0; attrs[i]; i++) {
		if (device_create_file(device, attrs[i]) < 0) {
			pr_err("Failed to create device file!(%s)!\n",
				attrs[i]->attr.name);
			ret = -ENODEV;
		}
	}
	return ret;
}

int cam_device_remove_file(struct device *device,
	const struct device_attribute **attrs)
{
	int ret = 0;

	if (device == NULL) {
		pr_err("device is null!\n");
		return ret;
	}

	for (; *attrs; attrs++)
		device_remove_file(device, *attrs);
	return ret;
}

int cam_sysfs_init_module(void)
{
	int ret = 0;

	svc_cheating_prevent_device_file_create();

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class))
			pr_err("failed to create device cam_dev_rear!\n");
	}

	cam_dev_flash = device_create(camera_class, NULL,
		0, NULL, "flash");
	ret |= cam_device_create_files(cam_dev_flash, flash_attrs);
#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
	cam_dev_ssm = device_create(camera_class, NULL,
		0, NULL, "ssm");
	ret |= cam_device_create_files(cam_dev_ssm, ssm_attrs);
#endif
	cam_dev_rear = device_create(camera_class, NULL,
		1, NULL, "rear");
	ret |= cam_device_create_files(cam_dev_rear, rear_attrs);

	cam_dev_front = device_create(camera_class, NULL,
		2, NULL, "front");
	ret |= cam_device_create_files(cam_dev_front, front_attrs);

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	cam_dev_af = device_create(camera_class, NULL,
		1, NULL, "af");
	ret |= cam_device_create_files(cam_dev_af, af_attrs);

	cam_dev_dual = device_create(camera_class, NULL,
		1, NULL, "dual");
	ret |= cam_device_create_files(cam_dev_dual, dual_attrs);
 #endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
	cam_dev_ois = device_create(camera_class, NULL,
		0, NULL, "ois");
	ret |= cam_device_create_files(cam_dev_ois, ois_attrs);
#endif

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
	cam_mipi_register_ril_notifier();
#endif

	return ret;
}

void cam_sysfs_exit_module(void)
{
	cam_device_remove_file(cam_dev_flash, flash_attrs);
	cam_device_remove_file(cam_dev_rear, rear_attrs);
	cam_device_remove_file(cam_dev_front, front_attrs);
#if defined(CONFIG_CAMERA_SSM_I2C_ENV)
	cam_device_remove_file(cam_dev_ssm, ssm_attrs);
#endif
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
	cam_device_remove_file(cam_dev_af, af_attrs);
	cam_device_remove_file(cam_dev_dual, dual_attrs);
 #endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
	cam_device_remove_file(cam_dev_ois, ois_attrs);
#endif
}

MODULE_DESCRIPTION("CAM_SYSFS");
MODULE_LICENSE("GPL v2");
