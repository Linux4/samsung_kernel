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

#include "cam_sysfs_ois_mcu.h"
#include "cam_actuator_core.h"
#include "cam_ois_core.h"
#include "cam_ois_mcu_stm32g.h"
#include "cam_sensor_cmn_header.h"
#include "cam_debug_util.h"
#include "cam_sysfs_init.h"

static int ois_power = 0;

#if defined(CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING)
static int actuator_power = 0;

static int rear_actuator_power_off (struct cam_ois_ctrl_t *o_ctrl, uint32_t *target, int cnt)
{
	int i = 0, rc = 0, index = 0;

	if (o_ctrl == NULL) {
		CAM_WARN(CAM_SENSOR_UTIL, "[WARNING] cam ois is not probed yet, skip power down");
		return -EFAULT;
	}

	if (actuator_power == 0) {
		CAM_WARN(CAM_SENSOR_UTIL, "[WARNING] actuator is off, skip power down");
		return -EFAULT;
	}

	for (i = 0; i < cnt; i++) {
		index = target[i];
		if (g_a_ctrls[index] != NULL) {
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
			if (g_a_ctrls[index]->use_mcu) {
				mutex_lock(&(o_ctrl->ois_mutex));
				rc |= cam_ois_power_down(o_ctrl);
				mutex_unlock(&(o_ctrl->ois_mutex));
			}
			else
#endif
			{
				mutex_lock(&(g_a_ctrls[index]->actuator_mutex));
				rc |= cam_actuator_power_down(g_a_ctrls[index]);
				mutex_unlock(&(g_a_ctrls[index]->actuator_mutex));
			}
			CAM_INFO(CAM_SENSOR_UTIL, "actuator %u power down", index);
		}
	}

	if (rc < 0) {
		CAM_INFO(CAM_SENSOR_UTIL, "actuator power down fail");
		actuator_power = 0;
		return 0;
	}

	actuator_power = 0;
	return 0;
}

static int rear_actuator_power_on (struct cam_ois_ctrl_t *o_ctrl, uint32_t *target, int cnt)
{
	int i = 0, index = 0;

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
	if (ois_power > 0) {
		CAM_WARN(CAM_SENSOR_UTIL, "[WARNING] ois is used");
		return -EFAULT;
	}
#endif

	actuator_power = 1;

	for (i = 0; i < cnt; i++) {
		index = target[i];
		if (g_a_ctrls[index] != NULL) {
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
			if (g_a_ctrls[index]->use_mcu) {
				mutex_lock(&(o_ctrl->ois_mutex));
				cam_ois_power_up(o_ctrl);
				msleep(20);
				cam_ois_mcu_init(o_ctrl);
				mutex_unlock(&(o_ctrl->ois_mutex));
			}
			else
#endif
			{
				mutex_lock(&(g_a_ctrls[index]->actuator_mutex));
				cam_actuator_power_up(g_a_ctrls[index]);
				mutex_unlock(&(g_a_ctrls[index]->actuator_mutex));
			}
			cam_actuator_default_init_setting(g_a_ctrls[index]);
			CAM_INFO(CAM_SENSOR_UTIL, "actuator %u power up", index);
		}
	}
	return 0;
}
#endif

static ssize_t rear_actuator_power_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
#if defined(CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING)
	struct cam_ois_ctrl_t *o_ctrl = g_o_ctrl;
	int cnt = 0, rc = 0;
	uint32_t target[] = { SEC_WIDE_SENSOR };

	cnt = ARRAY_SIZE(target);

	switch (buf[0]) {
	case ACTUATOR_POWER_OFF:
		rc = rear_actuator_power_off(o_ctrl, target, cnt);
		if (rc < 0)
			goto error;

		break;

	case ACTUATOR_POWER_ON:
		rc = rear_actuator_power_on(o_ctrl, target, cnt);
		if (rc < 0)
			goto error;

		break;

	default:
		break;
	}

	error:
#endif
	return size;
}

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
#if !defined(CONFIG_SEC_E1Q_PROJECT) && !defined(CONFIG_SEC_E2Q_PROJECT) && !defined(CONFIG_SEC_E3Q_PROJECT)
static ssize_t ois_mgless_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int offset = 0, i = 0;
	uint32_t mgless = 0;
	int mglessY = 0, mglessX = 0;
	mgless = cam_ois_get_mgless(g_o_ctrl);

	for (i = 0; i < CUR_MODULE_NUM; i++) {
		if (offset > 0)
			offset += sprintf(buf + offset, ", ");
		mglessX = ((mgless >> (2 * i)) & 0x01) ? 1 : 0;
		mglessY = ((mgless >> (2 * i)) & 0x02) ? 1 : 0;

		offset += sprintf(buf + offset, "%d, %d", mglessX, mglessY);
	}
	buf[offset] = '\0';

	return offset;
}
#endif

long raw_init_x = 0, raw_init_y = 0, raw_init_z = 0;
uint32_t ois_autotest_threshold = 150;
uint32_t ois_autotest_frequency = 0x05;
uint32_t ois_autotest_amplitude = 0x2A;
static ssize_t ois_autotest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint32_t i = 0, module_mask = 0;

	CAM_INFO(CAM_SENSOR_UTIL, "E");

	for (i = 0; i < CUR_MODULE_NUM; i++)
		module_mask |= (1 << i);
	cam_ois_sine_wavecheck(g_o_ctrl, ois_autotest_threshold,
		ois_autotest_frequency, ois_autotest_amplitude,
		buf, module_mask);

	CAM_INFO(CAM_SENSOR_UTIL, "X");

	if (strlen(buf))
		return strlen(buf);
	return 0;
}

static ssize_t ois_autotest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	char* token	  = NULL;
	char* pContext	  = NULL;
	uint32_t token_cnt = 0;
	uint32_t value	  = 0;
	uint32_t args[3] = { 150, 0x05, 0x2A };

	if (buf == NULL)
		return -1;

	pContext = (char*)buf;
	while ((token = strsep(&pContext, ","))) {
		if (kstrtoint(token, 10, &value))
			return -1;
		args[token_cnt++] = value;
		if (token_cnt >= 3)
			break;
	}

	ois_autotest_threshold = args[0];
	ois_autotest_frequency =
		(args[1] >= 1 && args[1] <= 255) ? args[1] : 0x05 ;
	ois_autotest_amplitude	=
		(args[2] >= 1 && args[2] <= 100) ? args[2] : 0x2A ;

	CAM_INFO(CAM_SENSOR_UTIL, "threshold %u, frequency %d, aplitude %d",
		ois_autotest_threshold, ois_autotest_frequency, ois_autotest_amplitude);

	return size;
}

static int ois_power_store_off (struct cam_ois_ctrl_t *o_ctrl)
{
	if (ois_power == 0) {
		CAM_WARN(CAM_SENSOR_UTIL, "[WARNING] ois is off, skip power down");
		return -EFAULT;
	}
	cam_ois_power_down(o_ctrl);
	CAM_INFO(CAM_SENSOR_UTIL, "power down");
	ois_power = 0;

	return 0;
}

static int ois_power_store_on (struct cam_ois_ctrl_t *o_ctrl)
{
#if defined(CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING)
	if (actuator_power > 0) {
		CAM_WARN(CAM_SENSOR_UTIL, "[WARNING] actuator is used");
		return -EFAULT;
	}
#endif

	ois_power = 1;
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) &&\
	defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
	o_ctrl->sysfs_ois_init = 0;
#endif
	cam_ois_power_up(o_ctrl);
	msleep(200);
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
	cam_ois_mcu_init(o_ctrl);
#endif
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) &&\
	defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
	o_ctrl->sysfs_ois_init = 1;
#endif
	o_ctrl->ois_mode = 0;
	CAM_INFO(CAM_SENSOR_UTIL, "power up");

	return 0;
}

static ssize_t ois_power_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cam_ois_ctrl_t *o_ctrl = g_o_ctrl;
	int rc = 0;

	if (o_ctrl == NULL ||
		((o_ctrl->io_master_info.master_type == I2C_MASTER) &&
		(o_ctrl->io_master_info.client == NULL)) ||
		((o_ctrl->io_master_info.master_type == CCI_MASTER) &&
		(o_ctrl->io_master_info.cci_client == NULL)))
		return size;

	mutex_lock(&(o_ctrl->ois_mutex));
	if (o_ctrl->cam_ois_state != CAM_OIS_INIT) {
		CAM_ERR(CAM_SENSOR_UTIL, "Not in right state to control OIS power %d",
			o_ctrl->cam_ois_state);
		goto error;
	}

	switch (buf[0]) {
	case OIS_POWER_OFF:
		rc = ois_power_store_off(o_ctrl);
		if (rc < 0)
			goto error;
		break;

	case OIS_POWER_ON:
		rc = ois_power_store_on(o_ctrl);
		if (rc < 0)
			goto error;
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

	return scnprintf(buf, PAGE_SIZE, "%d,%s%ld.%03ld,%s%ld.%03ld,%s%ld.%03ld\n", result,
		(raw_data_x >= 0 ? "" : "-"), abs(raw_data_x) / 1000, abs(raw_data_x) % 1000,
		(raw_data_y >= 0 ? "" : "-"), abs(raw_data_y) / 1000, abs(raw_data_y) % 1000,
		(raw_data_z >= 0 ? "" : "-"), abs(raw_data_z) / 1000, abs(raw_data_z) % 1000);
}

static ssize_t gyro_noise_stdev_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int result = 0;
	long stdev_data_x = 0, stdev_data_y = 0;

	result = cam_ois_gyro_sensor_noise_check(g_o_ctrl, &stdev_data_x, &stdev_data_y);

	return scnprintf(buf, PAGE_SIZE, "%d,%s%ld.%03ld,%s%ld.%03ld\n", result,
		(stdev_data_x >= 0 ? "" : "-"), abs(stdev_data_x) / 1000, abs(stdev_data_x) % 1000,
		(stdev_data_y >= 0 ? "" : "-"), abs(stdev_data_y) / 1000, abs(stdev_data_y) % 1000);
}

static ssize_t gyro_selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int result_total = 0, result = 0;
	bool result_offset = 0, result_selftest = 0;
	uint32_t selftest_ret = 0;
	long raw_data_x = 0, raw_data_y = 0;
	int OIS_GYRO_OFFSET_SPEC = 15000;

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

	CAM_INFO(CAM_SENSOR_UTIL, "Result : 0 (success), 1 (offset fail), 2 (selftest fail) , 3 (both fail)");
	CAM_INFO(CAM_SENSOR_UTIL, "Result : %d, result x = %ld, result y = %ld, result z = %ld",
		result_total, raw_data_x, raw_data_y, raw_data_z);

	rc = scnprintf(buf, PAGE_SIZE, "%d,%s%ld.%03ld,%s%ld.%03ld,%s%ld.%03ld\n", result_total,
		(raw_data_x >= 0 ? "" : "-"), abs(raw_data_x) / 1000, abs(raw_data_x) % 1000,
		(raw_data_y >= 0 ? "" : "-"), abs(raw_data_y) / 1000, abs(raw_data_y) % 1000,
		(raw_data_z >= 0 ? "" : "-"), abs(raw_data_z) / 1000, abs(raw_data_z) % 1000);

	CAM_INFO(CAM_SENSOR_UTIL, "%s", buf);

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
			CAM_ERR(CAM_SENSOR_UTIL, "count is abnormal, count = %d", size);
			return 0;
		}

		scnprintf(raw_data, sizeof(raw_data), "%s", buf);
		efs_size = strlen(raw_data);
		cam_ois_parsing_raw_data(g_o_ctrl, raw_data, efs_size, &raw_data_x, &raw_data_y, &raw_data_z);

		raw_init_x = raw_data_x;
		raw_init_y = raw_data_y;
		raw_init_z = raw_data_z;

		CAM_INFO(CAM_SENSOR_UTIL, "%s efs data = %s, size = %ld, raw x = %ld, raw y = %ld, raw z = %ld",
			buf, efs_size, raw_data_x, raw_data_y, raw_data_z);
	} else {
		CAM_ERR(CAM_SENSOR_UTIL, "%s OIS power is not enabled.");
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

	CAM_INFO(CAM_SENSOR_UTIL, "raw data x = %ld, raw data y = %ld, raw data z = %ld",
		raw_data_x, raw_data_y, raw_data_z);

	rc = scnprintf(buf, PAGE_SIZE, "%s%ld.%03ld,%s%ld.%03ld,%s%ld.%03ld\n",
		(raw_data_x >= 0 ? "" : "-"), abs(raw_data_x) / 1000, abs(raw_data_x) % 1000,
		(raw_data_y >= 0 ? "" : "-"), abs(raw_data_y) / 1000, abs(raw_data_y) % 1000,
		(raw_data_z >= 0 ? "" : "-"), abs(raw_data_z) / 1000, abs(raw_data_z) % 1000);

	CAM_INFO(CAM_SENSOR_UTIL, "%s", buf);

	if (rc)
		return rc;
	return 0;
}

char ois_fw_full[SYSFS_FW_VER_SIZE] = "NULL NULL\n";
static ssize_t ois_fw_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	CAM_INFO(CAM_SENSOR_UTIL, "[FW_DBG] OIS_fw_ver : %s", ois_fw_full);
	rc = scnprintf(buf, PAGE_SIZE, "%s", ois_fw_full);
	if (rc)
		return rc;
	return 0;
}

static ssize_t ois_fw_full_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	CAM_INFO(CAM_SENSOR_UTIL, "[FW_DBG] buf : %s", buf);
	scnprintf(ois_fw_full, sizeof(ois_fw_full), "%s", buf);

	return size;
}

char ois_debug[40] = "NULL NULL NULL\n";
static ssize_t ois_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	CAM_INFO(CAM_SENSOR_UTIL, "[FW_DBG] ois_debug : %s", ois_debug);
	rc = scnprintf(buf, PAGE_SIZE, "%s", ois_debug);
	if (rc)
		return rc;
	return 0;
}

static ssize_t ois_exif_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	CAM_INFO(CAM_SENSOR_UTIL, "[FW_DBG] buf: %s", buf);
	scnprintf(ois_debug, sizeof(ois_debug), "%s", buf);

	return size;
}

static ssize_t ois_reset_check(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	if (g_o_ctrl == NULL)
		return 0;

	CAM_DBG(CAM_SENSOR_UTIL, "ois reset_check : %d", g_o_ctrl->ois_mode);
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
		CAM_ERR(CAM_SENSOR_UTIL, "Fail, power down state");
		return -1;
	}

	mutex_lock(&(g_o_ctrl->ois_mutex));
	if (g_o_ctrl->cam_ois_state != CAM_OIS_START) {
		CAM_ERR(CAM_SENSOR_UTIL, "Not in right state to set ois mode %d",
			g_o_ctrl->cam_ois_state);
		goto error;
	}

	CAM_INFO(CAM_SENSOR_UTIL, "Configure OIS driver output 0x%x",
		g_o_ctrl->driver_output_mask);
	rc |= cam_ois_i2c_write(g_o_ctrl, OISSEL, g_o_ctrl->driver_output_mask,
		CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE); /* select module */
	rc |= cam_ois_set_ois_mode(g_o_ctrl, mode); // Centering mode

error:
	mutex_unlock(&(g_o_ctrl->ois_mutex));
	return size;
}
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
int ois_gain_result[INDEX_MAX] = {[0 ... INDEX_MAX - 1] = 2}; //0:normal, 1: No cal, 2: rear cal fail
int ois_sr_result[INDEX_MAX] = {[0 ... INDEX_MAX - 1] = 2}; //0:normal, 1: No cal, 2: rear cal fail
int ois_cross_talk_result[INDEX_MAX] = {[0 ... INDEX_MAX - 1] = 2}; //0:normal, 1: No cal, 2: rear cal fail

static ssize_t ois_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;
	uint32_t xgg = 0, ygg = 0;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_INFO(CAM_SENSOR_UTIL, "[FW_DBG] %s ois gain result : %d",
		attr->attr.name, ois_gain_result[index]);
	if (ois_gain_result[index] == 0) {
		memcpy(&xgg, &ois_xygg[index][0], 4);
		memcpy(&ygg, &ois_xygg[index][4], 4);
		rc = scnprintf(buf, PAGE_SIZE, "%d,0x%x,0x%x",
			ois_gain_result[index], xgg, ygg);
	} else {
		rc = scnprintf(buf, PAGE_SIZE, "%d",
			ois_gain_result[index]);
	}
	if (rc)
		return rc;
	return 0;
}

static ssize_t ois_supperssion_ratio_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;
	uint32_t xsr = 0, ysr = 0;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_INFO(CAM_SENSOR_UTIL, "[FW_DBG] %s ois sr result : %d",
		attr->attr.name, ois_sr_result[index]);
	if (ois_sr_result[index] == 0) {
		memcpy(&xsr, &ois_xysr[index][0], 2);
		memcpy(&ysr, &ois_xysr[index][2], 2);
		rc = scnprintf(buf, PAGE_SIZE, "%d,%u.%02u,%u.%02u",
			ois_sr_result[index], (xsr / 100), (xsr % 100), (ysr / 100), (ysr % 100));
	} else {
		rc = scnprintf(buf, PAGE_SIZE, "%d",
			ois_sr_result[index]);
	}

	if (rc)
		return rc;
	return 0;
}

#if defined(CONFIG_SAMSUNG_REAR_TRIPLE) || defined(CONFIG_SAMSUNG_REAR_QUADRA)
static ssize_t ois_read_cross_talk_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0, index = -1;
	uint32_t xcrosstalk = 0, ycrosstalk = 0;

	index = find_sysfs_index(attr);
	if (index < 0)
		return 0;

	CAM_INFO(CAM_SENSOR_UTIL, "[FW_DBG] %s read crosstalk result : %d",
		attr->attr.name, ois_cross_talk_result[index]);
	memcpy(&xcrosstalk, &ois_cross_talk[index][0], 2);
	memcpy(&ycrosstalk, &ois_cross_talk[index][2], 2);
	if (ois_cross_talk_result[index] == 0) { // normal
		rc = scnprintf(buf, PAGE_SIZE, "%u.%02u,%u.%02u",
			(xcrosstalk/ 100), (xcrosstalk % 100),
			(ycrosstalk / 100), (ycrosstalk % 100));
	} else if (ois_cross_talk_result[index] == 1) { // No cal
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
		CAM_ERR(CAM_SENSOR_UTIL, "ois check tele cross talk fail");

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
		CAM_ERR(CAM_SENSOR_UTIL, "ois check ois valid fail");

	rc = scnprintf(buf, PAGE_SIZE, "%u,%u,%u\n", result[0], result[1], result[2]);

	if (rc)
		return rc;
	return 0;
}

static ssize_t ois_check_hall_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	uint16_t subdev_id = SEC_TELE_SENSOR;

	uint16_t result[HALL_CAL_COUNT] = { 0, };

	rc = cam_ois_read_hall_cal(g_o_ctrl, subdev_id, result);
	if (rc < 0)
		CAM_ERR(CAM_SENSOR_UTIL, "ois check hall cal fail");

	rc = scnprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d",
		(rc < 0 ? 0 : 1), result[0], result[1], result[2], result[3], result[4],
		result[5], result[6], result[7]);

	if (rc)
		return rc;
	return 0;
}

static ssize_t ois_ext_clk_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t clk = 0;

	clk = cam_ois_check_ext_clk(g_o_ctrl);
	if (clk == 0)
		CAM_ERR(CAM_SENSOR_UTIL, "ois check ext clk fail");

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
	CAM_INFO(CAM_SENSOR_UTIL, "new ois ext clk %u", clk);

	rc = cam_ois_set_ext_clk(g_o_ctrl, clk);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR_UTIL, "ois check ext clk fail");
		return -1;
	}

	return size;
}

static ssize_t ois_center_shift_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc = 0, i = 0;
	char* token = NULL;
	char* str_shift = NULL;
	int val = 0, token_cnt = 0;
	int16_t shift[CUR_MODULE_NUM * 2] = { 0, };

	if (buf == NULL)
		return -1;

	if (!g_o_ctrl || !g_o_ctrl->is_power_up) {
		CAM_ERR(CAM_SENSOR_UTIL, "camera is not running");
		return -1;
	}

	str_shift = (char*)buf;
	while (((token = strsep(&str_shift, ",")) != NULL) && (token_cnt < (CUR_MODULE_NUM * 2))) {
		rc = kstrtoint(token, 10, &val);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR_UTIL, "invalid shift value %s", token);
			return -1;
		}
		shift[token_cnt++] = (int16_t)val;
	}

	for (i = 0; i < CUR_MODULE_NUM; i++)
		CAM_INFO(CAM_SENSOR_UTIL, "ois center shift M%d = (%d, %d)",
			(i+1), shift[2 * i], shift[2 * i + 1]);

	rc = cam_ois_center_shift(g_o_ctrl, shift);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR_UTIL, "ois center shift fail");
		return -1;
	}

	return size;
}

#if defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
static ssize_t ois_adc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	uint32_t result = 0;

	rc = get_ois_adc_value(g_o_ctrl, &result);
	if (rc < 0)
		CAM_ERR(CAM_OIS, "get ois adc fail");

	CAM_INFO(CAM_OIS, "ois_adc = %d", result);

	rc = scnprintf(buf, PAGE_SIZE, "%u\n", result);

	if (rc)
		return rc;
	return 0;
}

static int convert_adc_to_temperature(struct cam_ois_ctrl_t *o_ctrl, uint32_t adc)
{
	int low = 0;
	int high = 0;
	int temp = 0;
	int temp2 = 0;

	if (!o_ctrl->adc_temperature_table || !o_ctrl->adc_arr_size) {
		/* using fake temp */
		return 0;
	}

	high = o_ctrl->adc_arr_size - 1;

	if (o_ctrl->adc_temperature_table[low].adc >= adc)
		return o_ctrl->adc_temperature_table[low].temperature;
	else if (o_ctrl->adc_temperature_table[high].adc <= adc)
		return o_ctrl->adc_temperature_table[high].temperature;

	while (low <= high) {
		int mid = 0;

		mid = (low + high) / 2;
		if (o_ctrl->adc_temperature_table[mid].adc > adc)
			high = mid - 1;
		else if (o_ctrl->adc_temperature_table[mid].adc < adc)
			low = mid + 1;
		else
			return o_ctrl->adc_temperature_table[mid].temperature;
	}

	temp = o_ctrl->adc_temperature_table[high].temperature;

	temp2 = (o_ctrl->adc_temperature_table[high].temperature -
		o_ctrl->adc_temperature_table[low].temperature) *
		(adc - o_ctrl->adc_temperature_table[high].adc);

	temp -= temp2 /
		(o_ctrl->adc_temperature_table[low].adc -
		o_ctrl->adc_temperature_table[high].adc);

	return temp;
}

static ssize_t ois_temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	uint32_t result = 0;
	int temperature = 0;
	static int prev_temperature = 250;
	uint32_t retry = 10;

	do {
		rc = get_ois_adc_value(g_o_ctrl, &result);
		if ((rc < 0) || result)
			break;
		CAM_INFO(CAM_OIS, "ois_adc = %d, retry = %d", result, retry);
		usleep_range(2000, 2100);
	} while ((--retry > 0) && (rc >= 0) && (result == 0));

	if ((rc < 0) || (result == 0)) {
		CAM_ERR(CAM_OIS, "get ois adc fail");
		temperature = prev_temperature;
	} else
		temperature = convert_adc_to_temperature(g_o_ctrl, result);

	prev_temperature = temperature;

	CAM_INFO(CAM_OIS, "ois_adc = %d ois_temperature = %d", result, temperature);

	rc = scnprintf(buf, PAGE_SIZE, "%d\n", temperature);

	if (rc)
		return rc;
	return 0;
}
#endif
#endif

static DEVICE_ATTR(rear_actuator_power, S_IWUSR|S_IWGRP, NULL, rear_actuator_power_store);

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
static DEVICE_ATTR(ois_power, S_IWUSR, NULL, ois_power_store);
#if !defined(CONFIG_SEC_E1Q_PROJECT) && !defined(CONFIG_SEC_E2Q_PROJECT) && !defined(CONFIG_SEC_E3Q_PROJECT)
static DEVICE_ATTR(ois_mgless, S_IRUGO, ois_mgless_show, NULL);
#endif
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
static DEVICE_ATTR(ois_gain_rear, S_IRUGO, ois_gain_show, NULL);
static DEVICE_ATTR(ois_supperssion_ratio_rear, S_IRUGO, ois_supperssion_ratio_show, NULL);
static DEVICE_ATTR(check_hall_cal, S_IRUGO, ois_check_hall_cal_show, NULL);
#if defined(CONFIG_SAMSUNG_REAR_TRIPLE)
static DEVICE_ATTR(ois_gain_rear3, S_IRUGO, ois_gain_show, NULL);
static DEVICE_ATTR(ois_supperssion_ratio_rear3, S_IRUGO, ois_supperssion_ratio_show, NULL);
static DEVICE_ATTR(rear3_read_cross_talk, S_IRUGO, ois_read_cross_talk_show, NULL);
#endif
#if defined(CONFIG_SAMSUNG_REAR_QUADRA)
static DEVICE_ATTR(ois_gain_rear4, S_IRUGO, ois_gain_show, NULL);
static DEVICE_ATTR(ois_supperssion_ratio_rear4, S_IRUGO, ois_supperssion_ratio_show, NULL);
static DEVICE_ATTR(rear4_read_cross_talk, S_IRUGO, ois_read_cross_talk_show, NULL);
#endif

static DEVICE_ATTR(check_cross_talk, S_IRUGO, ois_check_cross_talk_show, NULL);
static DEVICE_ATTR(check_ois_valid, S_IRUGO, check_ois_valid_show, NULL);
static DEVICE_ATTR(ois_ext_clk, S_IRUGO|S_IWUSR|S_IWGRP, ois_ext_clk_show, ois_ext_clk_store);
static DEVICE_ATTR(ois_center_shift, S_IWUSR|S_IWGRP, NULL, ois_center_shift_store);
#if defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
static DEVICE_ATTR(adc, S_IRUGO, ois_adc_show, NULL);
static DEVICE_ATTR(temperature, S_IRUGO, ois_temperature_show, NULL);
#endif
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
const struct device_attribute *ois_attrs[] = {
	&dev_attr_rear_actuator_power,
	&dev_attr_ois_power,
#if !defined(CONFIG_SEC_E1Q_PROJECT) && !defined(CONFIG_SEC_E2Q_PROJECT) && !defined(CONFIG_SEC_E3Q_PROJECT)
	&dev_attr_ois_mgless,
#endif
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
	&dev_attr_ois_center_shift,
#if defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
	&dev_attr_adc,
	&dev_attr_temperature,
#endif
#endif
	NULL, // DO NOT REMOVE
};
#endif

MODULE_DESCRIPTION("CAM_SYSFS_OIS_MCU");
MODULE_LICENSE("GPL v2");
