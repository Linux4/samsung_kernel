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

#if defined(CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING)
static int actuator_power = 0;

static int rear_actuator_power_off (struct cam_ois_ctrl_t *o_ctrl, uint32_t *target, int cnt)
{
	int i = 0, rc = 0, index = 0;

	if (o_ctrl == NULL) {
		pr_info("%s: [WARNING] cam ois is not probed yet, skip power down", __func__);
		return -EFAULT;
	}

	if (actuator_power == 0) {
		pr_info("%s: [WARNING] actuator is off, skip power down", __func__);
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
			pr_info("%s: actuator %u power down", __func__, index);
		}
	}

	if (rc < 0) {
		pr_info("%s: actuator power down fail", __func__);
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
	if (o_ctrl->sysfs_ois_power > 0) {
		pr_info("%s: [WARNING] ois is used", __func__);
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
			pr_info("%s: actuator %u power up", __func__, index);
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
long raw_init_x = 0, raw_init_y = 0, raw_init_z = 0;
uint32_t ois_autotest_threshold = 150;

static ssize_t ois_autotest_show(struct device *dev, struct device_attribute *attr, char *buf)
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

static ois_power_store_off (struct cam_ois_ctrl_t *o_ctrl)
{
	if (o_ctrl->sysfs_ois_power == 0) {
		pr_info("%s: [WARNING] ois is off, skip power down", __func__);
		return -EFAULT;
	}
	cam_ois_power_down(o_ctrl);
	pr_info("%s: power down", __func__);
	o_ctrl->sysfs_ois_power = 0;

	return 0;
}

static ois_power_store_on (struct cam_ois_ctrl_t *o_ctrl)
{
#if defined(CONFIG_SAMSUNG_ACTUATOR_PREVENT_SHAKING)
	if (actuator_power > 0) {
		pr_info("%s: [WARNING] actuator is used", __func__);
		return -EFAULT;
	}
#endif

	o_ctrl->sysfs_ois_power = 1;
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
	pr_info("%s: power up", __func__);

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
		pr_err("%s: Not in right state to control OIS power %d",
			__func__, o_ctrl->cam_ois_state);
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

	if (raw_data_x < 0 && raw_data_y < 0) {
		if (raw_data_z < 0) {
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result,
			abs(raw_data_x / 1000), abs(raw_data_x % 1000),
			abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
		else {
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result,
			abs(raw_data_x / 1000), abs(raw_data_x % 1000),
			abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
	} else if (raw_data_x < 0) {
		if (raw_data_z < 0) {
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld,-%ld.%03ld\n",result,
			abs(raw_data_x / 1000),	abs(raw_data_x % 1000),
			abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
		else {
			return scnprintf(buf, PAGE_SIZE, "%d,-%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result,
			abs(raw_data_x / 1000),	abs(raw_data_x % 1000),
			abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
	} else if (raw_data_y < 0) {
		if (raw_data_z < 0) {
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld,-%ld.%03ld\n", result,
			abs(raw_data_x / 1000), abs(raw_data_x % 1000),
			abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
		else {
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,-%ld.%03ld,%ld.%03ld\n", result,
			abs(raw_data_x / 1000),	abs(raw_data_x % 1000),
			abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
	} else {
		if (raw_data_z < 0) {
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld,-%ld.%03ld\n", result,
			abs(raw_data_x / 1000),	abs(raw_data_x % 1000),
			abs(raw_data_y / 1000), abs(raw_data_y % 1000),
			abs(raw_data_z / 1000), abs(raw_data_z % 1000));
		}
		else {
			return scnprintf(buf, PAGE_SIZE, "%d,%ld.%03ld,%ld.%03ld,%ld.%03ld\n", result,
			abs(raw_data_x / 1000),	abs(raw_data_x % 1000),
			abs(raw_data_y / 1000), abs(raw_data_y % 1000),
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

	if (g_o_ctrl->sysfs_ois_power) {
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
#if defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
static DEVICE_ATTR(adc, S_IRUGO, ois_adc_show, NULL);
static DEVICE_ATTR(temperature, S_IRUGO, ois_temperature_show, NULL);
#endif
#endif

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32) || defined(CONFIG_SAMSUNG_OIS_RUMBA_S4)
const struct device_attribute *ois_attrs[] = {
	&dev_attr_rear_actuator_power,
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
