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
#include "cam_debug_util.h"
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
#include "cam_ois_mcu_stm32g.h"
#include "cam_sysfs_ois_mcu.h"
#endif
#if defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
#include "cam_ois_rumba_s4.h"
#include "cam_sysfs_ois_mcu.h"
#endif

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA) || defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
#include "cam_sensor_cmn_header.h"
#include "cam_hw_bigdata.h"
#include "cam_sysfs_hw_bigdata.h"
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

#if defined(CONFIG_SAMSUNG_ACTUATOR_READ_HALL_VALUE)
#include "cam_sec_actuator_core.h"
#endif

#if defined(CONFIG_CAMERA_CDR_TEST)
#include "cam_clock_data_recovery.h"
#endif
#ifdef CONFIG_SEC_KUNIT
#include "camera_kunit_main.h"
#endif

// Must match with enum sysfs_index.
const char sysfs_identifier[INDEX_MAX][16] = {
	"rear",
	"rear2",
	"rear3",
	"rear4",
	"front",
	"front2",
	"front3",
};

const char svc_sensor_identifier[INDEX_MAX][16] = {
	"rear_sensor",
	"rear_sensor2",
	"rear_sensor3",
	"rear_sensor4",
	"front_sensor",
	"front_sensor2",
	"front_sensor3",
};

const char svc_module_identifier[INDEX_MAX][16] = {
	"rear_module",
	"rear_module2",
	"rear_module3",
	"rear_module4",
	"front_module",
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	"front_module2",
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
	"upper_module",
#endif
};

int find_sysfs_index(struct device_attribute *attr)
{
	int i = 0;

	if (strstr(attr->attr.name, "SVC_") != NULL) {
		for (i = INDEX_MAX - 1; i >= 0; i--)
		{
			CAM_DBG(CAM_UTIL, "svc_sensor_identifier[%d] %s",
				i, svc_sensor_identifier[i]);
			if (strlen(svc_sensor_identifier[i]) &&
				strstr(attr->attr.name, svc_sensor_identifier[i])) {
				CAM_INFO(CAM_UTIL, "%s find index %d",
					attr->attr.name, i);
				return i;
			}

			CAM_DBG(CAM_UTIL, "svc_module_identifier[%d] %s",
				i, svc_module_identifier[i]);
			if (strlen(svc_module_identifier[i]) &&
				strstr(attr->attr.name, svc_module_identifier[i])) {
					CAM_INFO(CAM_UTIL, "%s find index %d",
						attr->attr.name, i);
				   return i;
			   }
		}
	} else {
		for (i = INDEX_MAX - 1; i >= 0; i--)
		{
			if (strlen(sysfs_identifier[i]) &&
				strstr(attr->attr.name, sysfs_identifier[i])) {
				CAM_INFO(CAM_UTIL, "%s find index %d",
					attr->attr.name, i);

				return i;
			}
		}
	}

	CAM_ERR(CAM_UTIL, "%s fail to find index", attr->attr.name);
	return -1;
}

int map_sysfs_index_to_sensor_id(int sysfs_index)
{
	int sensor_id = -1;

	switch (sysfs_index) {
		case INDEX_REAR:
			sensor_id = SEC_WIDE_SENSOR;
			break;
		case INDEX_REAR2:
			sensor_id = SEC_ULTRA_WIDE_SENSOR;
			break;
		case INDEX_REAR3:
			sensor_id = SEC_TELE_SENSOR;
			break;
		case INDEX_REAR4:
			sensor_id = SEC_TELE2_SENSOR;
			break;
		case INDEX_FRONT:
			sensor_id = SEC_FRONT_SENSOR;
			break;
		case INDEX_FRONT2:
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
			sensor_id = SEC_FRONT_AUX1_SENSOR;
#endif
#if defined(CONFIG_SAMSUNG_FRONT_TOP) && !defined(CONFIG_SAMSUNG_FRONT_DUAL)
			sensor_id = SEC_FRONT_TOP_SENSOR;
#endif
			break;
		case INDEX_FRONT3:
#if defined(CONFIG_SAMSUNG_FRONT_TOP) && defined(CONFIG_SAMSUNG_FRONT_DUAL)
			sensor_id = SEC_FRONT_SENSOR;
#endif
			break;
		default:
			break;
	}

	if (sensor_id < 0)
		CAM_ERR(CAM_UTIL, "mapping sysfs index %d to sensor id fail",
			sysfs_index);

	return sensor_id;
}

int map_sysfs_index_to_hw_param_id(int sysfs_index)
{
	int id = -1;

	switch (sysfs_index) {
		case INDEX_REAR:
			id = HW_PARAM_REAR;
			break;
		case INDEX_REAR2:
			id = HW_PARAM_REAR2;
			break;
		case INDEX_REAR3:
			id = HW_PARAM_REAR3;
			break;
		case INDEX_REAR4:
			id = HW_PARAM_REAR4;
			break;
		case INDEX_FRONT:
			id = HW_PARAM_FRONT;
			break;
		case INDEX_FRONT2:
			id = HW_PARAM_FRONT2;
			break;
		case INDEX_FRONT3:
			id = HW_PARAM_FRONT3;
			break;
		default:
			break;
	}

	if (id < 0)
		CAM_ERR(CAM_UTIL, "mapping sysfs index %d to hw param id fail",
			sysfs_index);

	return id;
}

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

#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
#define BPC_OTP_DATA_MAX_SIZE 0x9000
uint8_t *otp_data = NULL;
#endif

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

#if defined(CONFIG_CAMERA_CDR_TEST)
static ssize_t rear_cam_cdr_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_clock_data_recovery_get_value());
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_cam_cdr_value_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	cam_clock_data_recovery_set_value(buf);

	return size;
}

static ssize_t rear_cam_cdr_result_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_clock_data_recovery_get_result());
	if (rc)
		return rc;
	return 0;
}

char cdr_fastaec[5] = "";
static ssize_t rear_cam_cdr_fastaec_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "%s", cdr_fastaec);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_cam_cdr_fastaec_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	scnprintf(cdr_fastaec, sizeof(cdr_fastaec), "%s", buf);

	return size;
}
#endif

#if defined(CONFIG_CAMERA_HW_ERROR_DETECT)
char rear_i2c_rfinfo[30] = "\n";
static ssize_t rear_i2c_rfinfo_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_i2c_rfinfo);
	if (rc)
		return rc;
	return 0;
}

char retry_cnt[INDEX_MAX][5] = { [0 ... INDEX_MAX - 1] = "\n" };
static ssize_t camera_eeprom_retry_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	rc = scnprintf(buf, PAGE_SIZE, "%s", retry_cnt[index]);
	if (rc)
		return rc;
	return 0;
}
#endif

#ifdef CONFIG_SEC_KUNIT
static ssize_t cam_kunit_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if ((strlen(buf) <= 2) || (strlen(buf) > 10))
	{
		pr_err("[KUNIT_DBG] invalid argument\n");
		return size;
	}

	if (!strncmp(buf, "hwb", 3)) {
		cam_kunit_hw_bigdata_test();
	}

	if (!strncmp(buf, "eeprom", 6)) {
		cam_kunit_eeprom_test();
	}

	if (!strncmp(buf, "cdr", 3)) {
		cam_kunit_clock_data_recovery_test();
	}

	if (!strncmp(buf, "mipi", 4)) {
		cam_kunit_adaptive_mipi_test();
	}

	if (!strncmp(buf, "sysfs_hwb", 9)) {
		cam_kunit_sysfs_hw_bigdata_test();
	}

	if (!strncmp(buf, "sen_util", sizeof("sen_util") - 1)) {
		cam_kunit_sensor_util_test();
	}

	return size;
}
#endif

static ssize_t camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	switch (index)
	{
		case INDEX_REAR:
#if defined(CONFIG_SEC_DM3Q_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SLSI_S5KHP2\n");
#elif defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SLSI_S5KGN3\n");
#elif defined(CONFIG_SEC_B5Q_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SLSI_S5K2LD\n");
#elif defined(CONFIG_SEC_GTS9_PROJECT) || defined(CONFIG_SEC_GTS9P_PROJECT) || defined(CONFIG_SEC_GTS9U_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "HYNIX_HI1337\n");
#else
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SONY_IMX555\n");
#endif
			break;
		case INDEX_REAR2:
#if defined(CONFIG_SEC_GTS9P_PROJECT) || defined(CONFIG_SEC_GTS9U_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "HYNIX_HI847\n");
#elif defined(CONFIG_SEC_Q5Q_PROJECT) || defined(CONFIG_SEC_B5Q_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SONY_IMX258\n");
#else
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SONY_IMX564\n");
#endif
			break;
		case INDEX_REAR3:
#if defined(CONFIG_SEC_DM3Q_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SONY_IMX754\n");
#elif defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SLSI_S5K3K1\n");
#else
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SLSI_S5KGW2\n");
#endif
			break;
		case INDEX_REAR4:
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SONY_IMX754\n");
			break;
		case INDEX_FRONT:
#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SLSI_S5K3LU\n");
#elif defined(CONFIG_SEC_GTS9_PROJECT) || defined(CONFIG_SEC_GTS9P_PROJECT) || defined(CONFIG_SEC_GTS9U_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "HYNIX_HI1337\n");
#elif defined(CONFIG_SEC_Q5Q_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SONY_IMX471\n");
#elif defined(CONFIG_SEC_B5Q_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SLSI_S5K3J1\n");
#else
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SONY_IMX374\n";
#endif
			break;
		case INDEX_FRONT2:
#if defined(CONFIG_SEC_GTS9U_PROJECT)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "HYNIX_HI1337\n");
#else
#if defined(CONFIG_SAMSUNG_FRONT_TOP)
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SONY_IMX374\n");
#else
			rc = scnprintf(buf, PAGE_SIZE, "%s", "S5K4HA\n");
#endif
#endif
			break;
		case INDEX_FRONT3:
			rc = scnprintf(buf, PAGE_SIZE, "%s", "SONY_IMX374\n");
			break;
		default:
			break;
	}

	if (rc)
		return rc;
	return 0;
}

char fw_ver[INDEX_MAX][SYSFS_FW_VER_SIZE] = { [0 ... INDEX_MAX - 1] = "NULL NULL\n" };
static ssize_t camera_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

#if !defined(CONFIG_SAMSUNG_FRONT_EEPROM)
	if (index == INDEX_FRONT)
		scnprintf(fw_ver[index], SYSFS_FW_VER_SIZE, "%s", "IMX374 N\n");
	if (index == INDEX_FRONT2)
		scnprintf(fw_ver[index], SYSFS_FW_VER_SIZE, "%s", "S5K4HA N\n");
	if (index == INDEX_FRONT3)
		scnprintf(fw_ver[index], SYSFS_FW_VER_SIZE, "%s", "IMX374 N\n");
#endif

	CAM_INFO(CAM_UTIL, "[FW_DBG] %s fw_ver : %s\n",
		attr->attr.name, fw_ver[index]);

	rc = scnprintf(buf, PAGE_SIZE, "%s", fw_ver[index]);
	if (rc)
		return rc;
	return 0;
}

static ssize_t camera_firmware_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s buf : %s\n",
		attr->attr.name, buf);
	scnprintf(fw_ver[index], sizeof(fw_ver[index]), "%s", buf);

	return size;
}

char fw_user_ver[INDEX_MAX][SYSFS_FW_VER_SIZE] = { [0 ... INDEX_MAX - 1] = "NULL\n" };
static ssize_t camera_firmware_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

#if !defined(CONFIG_SAMSUNG_FRONT_EEPROM)
	if (index == INDEX_FRONT ||
		index == INDEX_FRONT2 ||
		index == INDEX_FRONT3)
		scnprintf(fw_ver[index], SYSFS_FW_VER_SIZE, "%s", "OK\n");
#endif

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s fw_user_ver : %s\n",
		attr->attr.name, fw_user_ver[index]);
	rc = scnprintf(buf, PAGE_SIZE, "%s", fw_user_ver[index]);
	if (rc)
		return rc;
	return 0;
}

static ssize_t camera_firmware_user_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s buf : %s\n",
		attr->attr.name, buf);
	scnprintf(fw_user_ver[index], sizeof(fw_user_ver[index]), "%s", buf);

	return size;
}

char fw_factory_ver[INDEX_MAX][SYSFS_FW_VER_SIZE] = { [0 ... INDEX_MAX - 1] = "NULL\n" };
static ssize_t camera_firmware_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

#if !defined(CONFIG_SAMSUNG_FRONT_EEPROM)
	if (index == INDEX_FRONT ||
		index == INDEX_FRONT2 ||
		index == INDEX_FRONT3)
		scnprintf(fw_factory_ver[index], SYSFS_FW_VER_SIZE, "%s", "OK\n");
#endif

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s fw_factory_ver : %s\n",
		attr->attr.name, fw_factory_ver[index]);
	rc = scnprintf(buf, PAGE_SIZE, "%s", fw_factory_ver[index]);
	if (rc)
		return rc;
	return 0;
}

static ssize_t camera_firmware_factory_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s buf : %s\n",
		attr->attr.name, buf);
	scnprintf(fw_factory_ver[index], sizeof(fw_factory_ver[index]), "%s", buf);

	return size;
}

char fw_full_ver[INDEX_MAX][SYSFS_FW_VER_SIZE] = { [0 ... INDEX_MAX - 1] = "NULL NULL NULL\n" };
static ssize_t camera_firmware_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

#if !defined(CONFIG_SAMSUNG_FRONT_EEPROM)
	if (index == INDEX_FRONT)
		scnprintf(fw_full_ver[index], SYSFS_FW_VER_SIZE, "%s", "IMX374 N N\n");
	if (index == INDEX_FRONT2)
		scnprintf(fw_full_ver[index], SYSFS_FW_VER_SIZE, "%s", "S5K4HA N N\n");
	if (index == INDEX_FRONT3)
		scnprintf(fw_full_ver[index], SYSFS_FW_VER_SIZE, "%s", "IMX374 N N\n");
#endif

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s fw_full_ver : %s\n",
		attr->attr.name, fw_full_ver[index]);
	rc = scnprintf(buf, PAGE_SIZE, "%s", fw_full_ver[index]);
	if (rc)
		return rc;
	return 0;
}

static ssize_t camera_firmware_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s buf : %s\n",
		attr->attr.name, buf);
	scnprintf(fw_full_ver[index], sizeof(fw_full_ver[index]), "%s", buf);

	return size;
}

char rear_load_fw[SYSFS_FW_VER_SIZE] = "NULL\n";
static ssize_t rear_firmware_load_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] rear_load_fw : %s\n", rear_load_fw);
	rc = scnprintf(buf, PAGE_SIZE, "%s", rear_load_fw);
	if (rc)
		return rc;
	return 0;
}

static ssize_t rear_firmware_load_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	CAM_DBG(CAM_UTIL, "[FW_DBG] buf : %s\n", buf);
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

char module_info[INDEX_MAX][SYSFS_MODULE_INFO_SIZE] = { [0 ... INDEX_MAX - 1] = "NULL\n" };
static ssize_t camera_module_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s module_info : %s\n",
		attr->attr.name, module_info[index]);
	rc = scnprintf(buf, PAGE_SIZE, "%s", module_info[index]);
	if (rc)
		return rc;
	return 0;
}

static ssize_t camera_module_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s buf : %s\n",
		attr->attr.name, buf);
	scnprintf(module_info[index], sizeof(module_info[index]), "%s", buf);

	return size;
}

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

char af_cal_str[INDEX_MAX][MAX_AF_CAL_STR_SIZE] = { [0 ... INDEX_MAX - 1] = "" };
static ssize_t camera_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "%s af_cal_str : 20 %s\n",
		attr->attr.name, af_cal_str[index]);
	rc = scnprintf(buf, PAGE_SIZE, "20 %s", af_cal_str[index]);
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

char cam_info[INDEX_MAX][150] = { [0 ... INDEX_MAX - 1] = "NULL\n" }; // camera_info
static ssize_t camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s cam_info : %s\n",
		attr->attr.name, cam_info[index]);
	rc = scnprintf(buf, PAGE_SIZE, "%s", cam_info[index]);
	if (rc)
		return rc;
	return 0;
}

static ssize_t camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	CAM_DBG(CAM_UTIL, "[FW_DBG] %s buf : %s\n",
		attr->attr.name, buf);
//	scnprintf(rear_cam_info, sizeof(rear_cam_info), "%s", buf);

	return size;
}

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
char sensor_id[INDEX_MAX][FROM_SENSOR_ID_SIZE + 1] = { [0 ... INDEX_MAX - 1] = "\0" };
static ssize_t camera_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s sensor_id : %s\n",
		attr->attr.name, sensor_id[index]);
	memcpy(buf, sensor_id[index], FROM_SENSOR_ID_SIZE);
	return FROM_SENSOR_ID_SIZE;
}

static ssize_t camera_sensorid_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	CAM_DBG(CAM_UTIL, "[FW_DBG] %s buf : %s\n",
		attr->attr.name, buf);
//	scnprintf(rear_sensor_id, sizeof(rear_sensor_id), "%s", buf);

	return size;
}

#define FROM_MTF_SIZE 54
char mtf_exif[INDEX_MAX][FROM_MTF_SIZE + 1] = { [0 ... INDEX_MAX - 1] = "\0" };
static ssize_t camera_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s mtf_exif : %s\n",
		attr->attr.name, mtf_exif[index]);
	memcpy(buf, mtf_exif[index], FROM_MTF_SIZE);
	return FROM_MTF_SIZE;
}

static ssize_t camera_mtf_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	CAM_DBG(CAM_UTIL, "[FW_DBG] %s buf : %s\n",
		attr->attr.name, buf);
//	scnprintf(front_mtf_exif, sizeof(front_mtf_exif), "%s", buf);

	return size;
}

uint8_t module_id[INDEX_MAX][FROM_MODULE_ID_SIZE + 1] = { [0 ... INDEX_MAX - 1] = "\0" };
static ssize_t camera_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int index = -1;
	char* m_id = NULL;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	m_id = module_id[index];

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s : %c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  attr->attr.name, m_id[0], m_id[1], m_id[2], m_id[3], m_id[4],
	  m_id[5], m_id[6], m_id[7], m_id[8], m_id[9]);
	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
	  m_id[0], m_id[1], m_id[2], m_id[3], m_id[4],
	  m_id[5], m_id[6], m_id[7], m_id[8], m_id[9]);
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

uint32_t paf_err_data_result[INDEX_MAX] = { [0 ... INDEX_MAX - 1] = 0xFFFFFFFF };
static ssize_t camera_paf_cal_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "%s paf_err_data_result : %u\n",
		attr->attr.name, paf_err_data_result[index]);

	rc = scnprintf(buf, PAGE_SIZE, "%08X\n", paf_err_data_result[index]);
	if (rc)
		return rc;
	return 0;
}

#if defined(CONFIG_SAMSUNG_REAR_DUAL) || defined(CONFIG_SAMSUNG_REAR_TRIPLE) \
	|| defined(CONFIG_SAMSUNG_REAR_QUADRA) || defined(CONFIG_SAMSUNG_FRONT_DUAL)
uint8_t dual_cal[INDEX_MAX][FROM_MAX_DUAL_CAL_SIZE + 1] = { [0 ... INDEX_MAX - 1] = "\0" };
uint32_t dual_cal_size[INDEX_MAX] = { [0 ... INDEX_MAX - 1] = FROM_REAR_DUAL_CAL_SIZE };
static ssize_t camera_dual_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int index = -1;
	int copy_size = 0;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s dual_cal : %s\n",
		attr->attr.name, dual_cal[index]);

	switch (index)
	{
		case INDEX_REAR:
		case INDEX_REAR2:
		case INDEX_REAR3:
		case INDEX_REAR4:
			copy_size = FROM_REAR_DUAL_CAL_SIZE;
			break;
		case INDEX_FRONT:
		case INDEX_FRONT2:
		case INDEX_FRONT3:
			copy_size = FROM_FRONT_DUAL_CAL_SIZE;
			break;
		default:
			copy_size = FROM_REAR_DUAL_CAL_SIZE;
			break;
	}
	if (copy_size > dual_cal_size[index])
		copy_size = dual_cal_size[index];

	memcpy(buf, dual_cal[index], copy_size);
	return copy_size;
}

static ssize_t camera_dual_cal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_INFO(CAM_UTIL, "[FW_DBG] %s dual_cal_size : %d\n",
		attr->attr.name, dual_cal_size[index]);
	rc = scnprintf(buf, PAGE_SIZE, "%d", dual_cal_size[index]);
	if (rc)
		return rc;
	return 0;
}

DualTilt_t dual_tilt[INDEX_MAX];
static ssize_t camera_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_DBG(CAM_UTIL, "[FW_DBG] %s dual tilt x = %d, y = %d, z = %d, sx = %d, sy = %d, range = %d, max_err = %d, avg_err = %d, dll_ver = %d, project_cal_type=%s\n",
		attr->attr.name, dual_tilt[index].x, dual_tilt[index].y, dual_tilt[index].z,
		dual_tilt[index].sx, dual_tilt[index].sy, dual_tilt[index].range,
		dual_tilt[index].max_err, dual_tilt[index].avg_err,
		dual_tilt[index].dll_ver, dual_tilt[index].project_cal_type);

	rc = scnprintf(buf, PAGE_SIZE, "1 %d %d %d %d %d %d %d %d %d %s\n",
			dual_tilt[index].x, dual_tilt[index].y, dual_tilt[index].z,
			dual_tilt[index].sx, dual_tilt[index].sy, dual_tilt[index].range,
			dual_tilt[index].max_err, dual_tilt[index].avg_err,
			dual_tilt[index].dll_ver, dual_tilt[index].project_cal_type);
	if (rc)
		return rc;
	return 0;
}
#endif

#if defined(CONFIG_SAMSUNG_ACTUATOR_READ_HALL_VALUE)
static ssize_t af_hall_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1, sensor_id = -1;
	uint16_t af_hall = 0;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	sensor_id = map_sysfs_index_to_sensor_id(index);
	if (sensor_id < 0)
		return 0;

	if (g_a_ctrls[sensor_id]->cam_act_state == CAM_ACTUATOR_START) {
		rc = cam_sec_actuator_read_hall_value(g_a_ctrls[sensor_id], &af_hall);
	} else {
		CAM_ERR(CAM_ACTUATOR,"[AF] Actuator is not starting\n");
		return 0;
	}

	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,"[AF] Hall read failed\n");
		return 0;
	}

	CAM_INFO(CAM_ACTUATOR,"[AF] af_hall : %u\n", af_hall);

	rc = scnprintf(buf, PAGE_SIZE, "%u\n", af_hall);

	if (rc)
		return rc;
	return 0;
}
#endif

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

#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
static ssize_t rear_otp_bpc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	void *ret = NULL;
	int index = 0;
	char* tok = NULL;

	tok = strstr(attr->attr.name, "bpc");
	if (tok == NULL)
		return 0;

	tok += strlen("bpc");
	if (0 != kstrtouint(tok, 10, &index)
)
		return 0;

	CAM_INFO(CAM_UTIL, "%s read otp bpc %d",
		attr->attr.name, index);
	ret = memcpy(buf, otp_data + (index * PAGE_SIZE), PAGE_SIZE);

	if (ret)
		return PAGE_SIZE;

	return 0;
}

static ssize_t rear_otp_bpc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char read_opt_reset[5] = "BEEF";
	int index = 0;
	char* tok = NULL;

	tok = strstr(attr->attr.name, "bpc");
	if (tok == NULL)
		return 0;

	tok += strlen("bpc");
	if (0 != kstrtouint(tok, 10, &index)
)
		return 0;

	CAM_INFO(CAM_UTIL, "%s store otp bpc %d", attr->attr.name, index);

	if (index == 0 && memcmp(buf, read_opt_reset, sizeof(read_opt_reset)) == 0) {
		CAM_INFO(CAM_SENSOR, "[BPC] Sensor is not same");
		memcpy(otp_data, buf, sizeof(read_opt_reset));
	} else {
		memcpy(otp_data + (index * PAGE_SIZE), buf, PAGE_SIZE);
	}
	return size;
}
#endif

ssize_t rear_flash_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
#if defined(CONFIG_SAMSUNG_PMIC_FLASH)
	flash_power_store(dev, attr, buf, count);
#elif IS_REACHABLE(CONFIG_LEDS_S2MPB02)
	s2mpb02_store(buf);
#elif defined(CONFIG_LEDS_KTD2692)
	ktd2692_store(buf);
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

#if defined(CONFIG_CAMERA_CDR_TEST)
static DEVICE_ATTR(cam_cdr_value, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_cam_cdr_value_show, rear_cam_cdr_value_store);
static DEVICE_ATTR(cam_cdr_result, S_IRUGO, rear_cam_cdr_result_show, NULL);
static DEVICE_ATTR(cam_cdr_fastaec, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_cam_cdr_fastaec_show, rear_cam_cdr_fastaec_store);
#endif

#if defined(CONFIG_CAMERA_HW_ERROR_DETECT)
static DEVICE_ATTR(rear_i2c_rfinfo, S_IRUGO, rear_i2c_rfinfo_show, NULL);
static DEVICE_ATTR(rear_eeprom_retry, S_IRUGO, camera_eeprom_retry_show, NULL);
static DEVICE_ATTR(rear2_eeprom_retry, S_IRUGO, camera_eeprom_retry_show, NULL);
static DEVICE_ATTR(rear3_eeprom_retry, S_IRUGO, camera_eeprom_retry_show, NULL);
static DEVICE_ATTR(rear4_eeprom_retry, S_IRUGO, camera_eeprom_retry_show, NULL);
static DEVICE_ATTR(front_eeprom_retry, S_IRUGO, camera_eeprom_retry_show, NULL);
#endif

#ifdef CONFIG_SEC_KUNIT
static DEVICE_ATTR(cam_kunit_test, S_IWUSR|S_IWGRP,
	NULL, cam_kunit_test_store);
#endif

static DEVICE_ATTR(rear_camtype, S_IRUGO, camera_type_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_show, camera_firmware_store);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_user_show, camera_firmware_user_store);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_factory_show, camera_firmware_factory_store);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_full_show, camera_firmware_full_store);
static DEVICE_ATTR(rear_camfw_load, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_firmware_load_show, rear_firmware_load_store);
static DEVICE_ATTR(rear_calcheck, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_cal_data_check_show, rear_cal_data_check_store);
static DEVICE_ATTR(rear_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_module_info_show, camera_module_info_store);
static DEVICE_ATTR(isp_core, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_isp_core_check_show, rear_isp_core_check_store);
static DEVICE_ATTR(rear_afcal, S_IRUGO, camera_afcal_show, NULL);
static DEVICE_ATTR(rear_paf_offset_far, S_IRUGO,
	rear_paf_offset_far_show, NULL);
static DEVICE_ATTR(rear_paf_offset_mid, S_IRUGO,
	rear_paf_offset_mid_show, NULL);
static DEVICE_ATTR(rear_paf_cal_check, S_IRUGO,
	camera_paf_cal_check_show, NULL);
static DEVICE_ATTR(rear_f2_paf_offset_far, S_IRUGO,
	rear_f2_paf_offset_far_show, NULL);
static DEVICE_ATTR(rear_f2_paf_offset_mid, S_IRUGO,
	rear_f2_paf_offset_mid_show, NULL);
static DEVICE_ATTR(rear_f2_paf_cal_check, S_IRUGO,
	rear_f2_paf_cal_check_show, NULL);
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
static DEVICE_ATTR(front_afcal, S_IRUGO, camera_afcal_show, NULL);
#endif
static DEVICE_ATTR(front_camtype, S_IRUGO, camera_type_show, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_show, camera_firmware_store);
static DEVICE_ATTR(front_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	camera_firmware_full_show, camera_firmware_full_store);
static DEVICE_ATTR(front_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_user_show, camera_firmware_user_store);
static DEVICE_ATTR(front_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_factory_show, camera_firmware_factory_store);
#if defined (CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(rear_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_info_show, camera_info_store);
static DEVICE_ATTR(front_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_info_show, camera_info_store);
#endif
static DEVICE_ATTR(front_paf_cal_check, S_IRUGO,
	camera_paf_cal_check_show, NULL);
static DEVICE_ATTR(front_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_module_info_show, camera_module_info_store);
static DEVICE_ATTR(rear_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_sensorid_exif_show, camera_sensorid_exif_store);
static DEVICE_ATTR(front_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_sensorid_exif_show, camera_sensorid_exif_store);
static DEVICE_ATTR(rear_moduleid, S_IRUGO, camera_moduleid_show, NULL);
static DEVICE_ATTR(front_moduleid, S_IRUGO, camera_moduleid_show, NULL);
static DEVICE_ATTR(front_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_mtf_exif_show, camera_mtf_exif_store);
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
static DEVICE_ATTR(front_mipi_clock, S_IRUGO, front_camera_mipi_clock_show, NULL);
#endif
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front2_camtype, S_IRUGO, camera_type_show, NULL);
static DEVICE_ATTR(front2_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_show, camera_firmware_store);
static DEVICE_ATTR(front2_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	camera_firmware_full_show, camera_firmware_full_store);
static DEVICE_ATTR(front2_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_user_show, camera_firmware_user_store);
static DEVICE_ATTR(front2_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_factory_show, camera_firmware_factory_store);
static DEVICE_ATTR(front2_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_module_info_show, camera_module_info_store);
#if defined (CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(front2_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_info_show, camera_info_store);
#endif
static DEVICE_ATTR(front2_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_sensorid_exif_show, camera_sensorid_exif_store);
#endif
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front2_moduleid, S_IRUGO, camera_moduleid_show, NULL);
#endif

#if defined(CONFIG_SAMSUNG_FRONT_TOP)
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front3_camtype, S_IRUGO, camera_type_show, NULL);
static DEVICE_ATTR(front3_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_show, camera_firmware_store);
static DEVICE_ATTR(front3_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	camera_firmware_full_show, camera_firmware_full_store);
static DEVICE_ATTR(front3_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_user_show, camera_firmware_user_store);
static DEVICE_ATTR(front3_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_factory_show, camera_firmware_factory_store);
static DEVICE_ATTR(front3_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_module_info_show, camera_module_info_store);
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
static DEVICE_ATTR(front3_afcal, S_IRUGO, camera_afcal_show, NULL);
#endif
static DEVICE_ATTR(front3_moduleid, S_IRUGO, camera_moduleid_show, NULL);
static DEVICE_ATTR(SVC_upper_module, S_IRUGO, camera_moduleid_show, NULL);
#if defined (CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(front3_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_info_show, camera_info_store);
#endif
static DEVICE_ATTR(front3_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_sensorid_exif_show, camera_sensorid_exif_store);
#else
static DEVICE_ATTR(front2_camtype, S_IRUGO, camera_type_show, NULL);
static DEVICE_ATTR(front2_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_show, camera_firmware_store);
static DEVICE_ATTR(front2_camfw_full, S_IRUGO | S_IWUSR | S_IWGRP,
	camera_firmware_full_show, camera_firmware_full_store);
static DEVICE_ATTR(front2_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_user_show, camera_firmware_user_store);
static DEVICE_ATTR(front2_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_factory_show, camera_firmware_factory_store);
static DEVICE_ATTR(front2_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_module_info_show, camera_module_info_store);
#if !defined(CONFIG_SAMSUNG_FRONT_TOP_EEPROM)
static DEVICE_ATTR(front2_afcal, S_IRUGO, camera_afcal_show, NULL);
#endif
static DEVICE_ATTR(front2_moduleid, S_IRUGO, camera_moduleid_show, NULL);
static DEVICE_ATTR(SVC_upper_module, S_IRUGO, camera_moduleid_show, NULL);
#if defined (CONFIG_CAMERA_SYSFS_V2)
static DEVICE_ATTR(front2_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_info_show, camera_info_store);
#endif
static DEVICE_ATTR(front2_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_sensorid_exif_show, camera_sensorid_exif_store);
#endif
#endif

static DEVICE_ATTR(supported_cameraIds, S_IRUGO|S_IWUSR|S_IWGRP,
	supported_camera_ids_show, supported_camera_ids_store);

static DEVICE_ATTR(rear_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_mtf_exif_show, camera_mtf_exif_store);
static DEVICE_ATTR(rear_mtf2_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_mtf2_exif_show, rear_mtf2_exif_store);
static DEVICE_ATTR(SVC_rear_module, S_IRUGO, camera_moduleid_show, NULL);
static DEVICE_ATTR(SVC_front_module, S_IRUGO, camera_moduleid_show, NULL);
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(SVC_front_module2, S_IRUGO, camera_moduleid_show, NULL);
#endif
static DEVICE_ATTR(ssrm_camera_info, S_IRUGO|S_IWUSR|S_IWGRP,
	ssrm_camera_info_show, ssrm_camera_info_store);

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
static DEVICE_ATTR(rear3_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_show, camera_firmware_store);
static DEVICE_ATTR(rear3_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_full_show, camera_firmware_full_store);
#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
static DEVICE_ATTR(rear3_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_user_show, camera_firmware_user_store);
static DEVICE_ATTR(rear3_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_factory_show, camera_firmware_factory_store);
#endif
static DEVICE_ATTR(rear3_afcal, S_IRUGO, camera_afcal_show, NULL);
static DEVICE_ATTR(rear3_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_info_show, camera_info_store);
static DEVICE_ATTR(rear3_camtype, S_IRUGO, camera_type_show, NULL);
static DEVICE_ATTR(rear3_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_module_info_show, camera_module_info_store);
static DEVICE_ATTR(rear3_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_mtf_exif_show, camera_mtf_exif_store);
static DEVICE_ATTR(rear3_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_sensorid_exif_show, camera_sensorid_exif_store);
static DEVICE_ATTR(rear3_dualcal, S_IRUGO, camera_dual_cal_show, NULL);
static DEVICE_ATTR(rear3_dualcal_size, S_IRUGO, camera_dual_cal_size_show, NULL);

static DEVICE_ATTR(rear3_tilt, S_IRUGO, camera_tilt_show, NULL);
static DEVICE_ATTR(rear3_paf_cal_check, S_IRUGO,
	camera_paf_cal_check_show, NULL);
#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
static DEVICE_ATTR(rear3_moduleid, S_IRUGO, camera_moduleid_show, NULL);
static DEVICE_ATTR(SVC_rear_module3, S_IRUGO, camera_moduleid_show, NULL);
#endif
#endif
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
#if defined(CONFIG_SEC_DM3Q_PROJECT)
static DEVICE_ATTR(rear2_afcal, S_IRUGO, camera_afcal_show, NULL);
static DEVICE_ATTR(rear2_paf_cal_check, S_IRUGO,
	camera_paf_cal_check_show, NULL);
#endif
static DEVICE_ATTR(rear2_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_info_show, camera_info_store);
static DEVICE_ATTR(rear2_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_module_info_show, camera_module_info_store);
static DEVICE_ATTR(rear2_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_mtf_exif_show, camera_mtf_exif_store);
static DEVICE_ATTR(rear2_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_sensorid_exif_show, camera_sensorid_exif_store);
static DEVICE_ATTR(rear2_moduleid, S_IRUGO,
	camera_moduleid_show, NULL);
static DEVICE_ATTR(rear2_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_show, camera_firmware_store);
static DEVICE_ATTR(rear2_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_user_show, camera_firmware_user_store);
static DEVICE_ATTR(rear2_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_factory_show, camera_firmware_factory_store);
static DEVICE_ATTR(rear2_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_full_show, camera_firmware_full_store);
static DEVICE_ATTR(SVC_rear_module2, S_IRUGO, camera_moduleid_show, NULL);
static DEVICE_ATTR(rear2_camtype, S_IRUGO, camera_type_show, NULL);
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
static DEVICE_ATTR(rear2_dualcal, S_IRUGO, camera_dual_cal_show, NULL);
static DEVICE_ATTR(rear2_dualcal_size, S_IRUGO, camera_dual_cal_size_show, NULL);
static DEVICE_ATTR(rear2_tilt, S_IRUGO, camera_tilt_show, NULL);
#endif
#endif

#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
static DEVICE_ATTR(rear4_camfw, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_show, camera_firmware_store);
static DEVICE_ATTR(rear4_camfw_full, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_full_show, camera_firmware_full_store);
static DEVICE_ATTR(rear4_checkfw_user, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_user_show, camera_firmware_user_store);
static DEVICE_ATTR(rear4_checkfw_factory, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_firmware_factory_show, camera_firmware_factory_store);
static DEVICE_ATTR(rear4_afcal, S_IRUGO, camera_afcal_show, NULL);
static DEVICE_ATTR(rear4_caminfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_info_show, camera_info_store);
static DEVICE_ATTR(rear4_camtype, S_IRUGO, camera_type_show, NULL);
static DEVICE_ATTR(rear4_moduleinfo, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_module_info_show, camera_module_info_store);
static DEVICE_ATTR(rear4_mtf_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_mtf_exif_show, camera_mtf_exif_store);
static DEVICE_ATTR(rear4_sensorid_exif, S_IRUGO|S_IWUSR|S_IWGRP,
	camera_sensorid_exif_show, camera_sensorid_exif_store);
static DEVICE_ATTR(rear4_dualcal, S_IRUGO, camera_dual_cal_show, NULL);
static DEVICE_ATTR(rear4_dualcal_size, S_IRUGO, camera_dual_cal_size_show, NULL);

static DEVICE_ATTR(rear4_tilt, S_IRUGO, camera_tilt_show, NULL);
static DEVICE_ATTR(rear4_paf_cal_check, S_IRUGO,
	camera_paf_cal_check_show, NULL);
static DEVICE_ATTR(rear4_moduleid, S_IRUGO, camera_moduleid_show, NULL);
static DEVICE_ATTR(SVC_rear_module4, S_IRUGO, camera_moduleid_show, NULL);
#endif

#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
static DEVICE_ATTR(front2_dualcal, S_IRUGO, camera_dual_cal_show, NULL);
static DEVICE_ATTR(front2_dualcal_size, S_IRUGO, camera_dual_cal_size_show, NULL);
static DEVICE_ATTR(front2_tilt, S_IRUGO, camera_tilt_show, NULL);

#endif

#if defined(CONFIG_SAMSUNG_ACTUATOR_READ_HALL_VALUE)
static DEVICE_ATTR(rear_af_hall, S_IRUGO, af_hall_show, NULL);
static DEVICE_ATTR(rear3_af_hall, S_IRUGO, af_hall_show, NULL);
#endif

#if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
static DEVICE_ATTR(rear_frs_test, S_IRUGO|S_IWUSR|S_IWGRP,
	rear_camera_frs_test_show, rear_camera_frs_test_store);
#endif
#if defined(CONFIG_CAMERA_FAC_LN_TEST)
static DEVICE_ATTR(cam_ln_test, S_IRUGO|S_IWUSR|S_IWGRP,
	cam_factory_ln_test_show, cam_factory_ln_test_store);
#endif

#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
static DEVICE_ATTR(rear_otp_bpc0, S_IRUGO, rear_otp_bpc_show, rear_otp_bpc_store);
static DEVICE_ATTR(rear_otp_bpc1, S_IRUGO, rear_otp_bpc_show, rear_otp_bpc_store);
static DEVICE_ATTR(rear_otp_bpc2, S_IRUGO, rear_otp_bpc_show, rear_otp_bpc_store);
static DEVICE_ATTR(rear_otp_bpc3, S_IRUGO, rear_otp_bpc_show, rear_otp_bpc_store);
static DEVICE_ATTR(rear_otp_bpc4, S_IRUGO, rear_otp_bpc_show, rear_otp_bpc_store);
static DEVICE_ATTR(rear_otp_bpc5, S_IRUGO, rear_otp_bpc_show, rear_otp_bpc_store);
static DEVICE_ATTR(rear_otp_bpc6, S_IRUGO, rear_otp_bpc_show, rear_otp_bpc_store);
static DEVICE_ATTR(rear_otp_bpc7, S_IRUGO, rear_otp_bpc_show, rear_otp_bpc_store);
static DEVICE_ATTR(rear_otp_bpc8, S_IRUGO, rear_otp_bpc_show, rear_otp_bpc_store);
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
#ifdef CONFIG_SEC_KUNIT
struct device		*cam_dev_kunit;
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

#ifdef CONFIG_SEC_KUNIT
const struct device_attribute *kunit_attrs[] = {
	&dev_attr_cam_kunit_test,
	NULL, // DO NOT REMOVE
};
#endif

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
#if defined(CONFIG_SAMSUNG_ACTUATOR_READ_HALL_VALUE)
	&dev_attr_rear_af_hall,
	&dev_attr_rear3_af_hall,
#endif
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
#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
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
#if defined(CONFIG_SEC_DM3Q_PROJECT)
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
#if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
	&dev_attr_rear_frs_test,
#endif
#if defined(CONFIG_CAMERA_FAC_LN_TEST)
	&dev_attr_cam_ln_test,
#endif
#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
	&dev_attr_rear_otp_bpc0,
	&dev_attr_rear_otp_bpc1,
	&dev_attr_rear_otp_bpc2,
	&dev_attr_rear_otp_bpc3,
	&dev_attr_rear_otp_bpc4,
	&dev_attr_rear_otp_bpc5,
	&dev_attr_rear_otp_bpc6,
	&dev_attr_rear_otp_bpc7,
	&dev_attr_rear_otp_bpc8,
#endif
#if defined(CONFIG_CAMERA_CDR_TEST)
	&dev_attr_cam_cdr_value,
	&dev_attr_cam_cdr_result,
	&dev_attr_cam_cdr_fastaec,
#endif
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT)
	&dev_attr_rear_i2c_rfinfo,
	&dev_attr_rear_eeprom_retry,
	&dev_attr_rear2_eeprom_retry,
	&dev_attr_rear3_eeprom_retry,
	&dev_attr_rear4_eeprom_retry,
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
#if defined(CONFIG_SAMSUNG_FRONT_DUAL)
	&dev_attr_front2_dualcal,
	&dev_attr_front2_dualcal_size,
	&dev_attr_front2_tilt,
#endif
#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
	&dev_attr_front_mipi_clock,
#endif
#if defined(CONFIG_CAMERA_HW_ERROR_DETECT)
	&dev_attr_front_eeprom_retry,
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

static struct attribute *svc_cam_attrs[] = {
	&dev_attr_SVC_rear_module.attr,
#if defined(CONFIG_SAMSUNG_REAR_DUAL)
	&dev_attr_SVC_rear_module2.attr,
#endif
#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_DM3Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
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

#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	ret |= cam_device_create_files(cam_dev_rear, hwb_rear_attrs);
	ret |= cam_device_create_files(cam_dev_front, hwb_front_attrs);
#endif

#ifdef CONFIG_SEC_KUNIT
	cam_dev_kunit = device_create(camera_class, NULL,
		0, NULL, "kunit");
	ret |= cam_device_create_files(cam_dev_kunit, kunit_attrs);
#endif

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
	cam_mipi_register_ril_notifier();
#endif

#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
	otp_data = kmalloc(BPC_OTP_DATA_MAX_SIZE, GFP_KERNEL);
	if (otp_data == NULL) {
		CAM_ERR(CAM_SENSOR, "out of memory");
		return -1;
	}
#endif
#if defined(CONFIG_USE_CAMERA_HW_BIG_DATA)
	camera_hw_param_check_avail_cam();
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

#if defined(CONFIG_SAMSUNG_READ_BPC_FROM_OTP)
	if (otp_data != NULL) {
		kfree(otp_data);
		otp_data = NULL;
	}
#endif
}

MODULE_DESCRIPTION("CAM_SYSFS");
MODULE_LICENSE("GPL v2");
