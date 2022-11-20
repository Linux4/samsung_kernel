/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/dirent.h>
#include "adsp.h"
#define VENDOR "STK"
#define CHIP_ID "STK33910"

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/
static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

int get_light_sidx(struct adsp_data *data)
{
	int ret = MSG_LIGHT;
#ifdef CONFIG_SUPPORT_DUAL_OPTIC
	switch (data->fac_fstate) {
	case FSTATE_INACTIVE:
	case FSTATE_FAC_INACTIVE:
		ret = MSG_LIGHT;
		break;
	case FSTATE_ACTIVE:
	case FSTATE_FAC_ACTIVE:
		ret = MSG_LIGHT_SUB;
		break;
	default:
		break;
	}
#endif
//	pr_info("[FACTORY] %s: idx:%d\n", __func__, ret);
	return ret;
}

static ssize_t light_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(NULL, 0, light_idx, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0\n");
	}

	mutex_unlock(&data->light_factory_mutex);
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d\n",
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2], data->msg_buf[light_idx][3],
		data->msg_buf[light_idx][4], data->msg_buf[light_idx][5]);
}

static ssize_t light_get_dhr_sensor_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	pr_info("[FACTORY] %s: start\n", __func__);
	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(NULL, 0, light_idx, 0, MSG_TYPE_GET_DHR_INFO);
	while (!(data->ready_flag[MSG_TYPE_GET_DHR_INFO] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_DHR_INFO] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	mutex_unlock(&data->light_factory_mutex);
	return data->msg_buf[light_idx][0];
}

#if defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
static ssize_t light_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int brightness = 0;

	if (sscanf(buf, "%3d", &brightness) != 1) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}
	pr_info("[FACTORY] %s: %d\n", __func__, brightness);

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&brightness, sizeof(brightness), light_idx, 0, MSG_TYPE_SET_CAL_DATA);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}
#endif

#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
static ssize_t light_set_aod_rect_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int32_t msg_buf[5] = {OPTION_TYPE_SSC_AOD_RECT, 0, 0, 0, 0};

	if (sscanf(buf, "%3d,%3d,%3d,%3d",
		&msg_buf[1], &msg_buf[2], &msg_buf[3], &msg_buf[4]) != 4) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	pr_info("[FACTORY] %s: rect:%d,%d,%d,%d \n", __func__,
		msg_buf[1], msg_buf[2], msg_buf[3], msg_buf[4]);
	adsp_unicast(msg_buf, sizeof(msg_buf),
			MSG_SSC_CORE, 0, MSG_TYPE_OPTION_DEFINE);
	return size;
}
#endif

#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
static ssize_t light_hallic_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int32_t msg_buf[2];
	int new_value;

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else if (sysfs_streq(buf, "1"))
		new_value = 1;
	else
		return size;

	pr_info("[FACTORY] %s: new_value %d\n", __func__, new_value);
	msg_buf[0] = OPTION_TYPE_SET_HALLIC_INFO;
	msg_buf[1] = new_value;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

static ssize_t light_circle_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SEC_A82XQ_PROJECT)
	return snprintf(buf, PAGE_SIZE, "42.6 8.0 2.5\n");
#elif defined(CONFIG_SEC_M52XQ_PROJECT)
	return snprintf(buf, PAGE_SIZE, "46.3 6.2 2.5\n");
#else
	return snprintf(buf, PAGE_SIZE, "0 0 0\n");
#endif
}
#endif

static ssize_t light_register_read_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int cnt = 0;
	int32_t msg_buf[1];

	msg_buf[0] = data->msg_buf[light_idx][MSG_LIGHT_MAX - 1];

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_GET_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_GET_REGISTER] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_REGISTER] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	pr_info("[FACTORY] %s: [0x%x]: 0x%x\n",
		__func__, data->msg_buf[light_idx][MSG_LIGHT_MAX - 1],
		data->msg_buf[light_idx][0]);

	mutex_unlock(&data->light_factory_mutex);

	return snprintf(buf, PAGE_SIZE, "[0x%x]: 0x%x\n",
		data->msg_buf[light_idx][MSG_LIGHT_MAX - 1],
		data->msg_buf[light_idx][0]);
}

static ssize_t light_register_read_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int reg = 0;
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);

	if (sscanf(buf, "%3x", &reg) != 1) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	data->msg_buf[light_idx][MSG_LIGHT_MAX - 1] = reg;
	pr_info("[FACTORY] %s: [0x%x]\n", __func__,
		data->msg_buf[light_idx][MSG_LIGHT_MAX - 1]);

	return size;
}

static ssize_t light_register_write_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int cnt = 0;
	int32_t msg_buf[2];

	if (sscanf(buf, "%3x,%3x", &msg_buf[0], &msg_buf[1]) != 2) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_SET_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_SET_REGISTER] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_SET_REGISTER] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	data->msg_buf[light_idx][MSG_LIGHT_MAX - 1] = msg_buf[0];
	pr_info("[FACTORY] %s: 0x%x - 0x%x\n",
		__func__, msg_buf[0], data->msg_buf[light_idx][0]);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

void light_factory_init_work(struct adsp_data *data)
{
	pr_info("[FACTORY] %s : not supported light cal\n", __func__);
}

static ssize_t light_debug_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_LIGHT_DEBUG_INFO;
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0\n");
	}

	pr_info("[FACTORY] %s: %d,%d,%d,%d,%d,%d\n", __func__,
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2] >> 8, data->msg_buf[light_idx][2] & 0xff,
		data->msg_buf[light_idx][3], data->msg_buf[light_idx][4]);

	mutex_unlock(&data->light_factory_mutex);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d\n",
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2] >> 8, data->msg_buf[light_idx][2] & 0xff,
		data->msg_buf[light_idx][3], data->msg_buf[light_idx][4]);
}

static DEVICE_ATTR(register_write, 0220, NULL, light_register_write_store);
static DEVICE_ATTR(register_read, 0664,
		light_register_read_show, light_register_read_store);
static DEVICE_ATTR(vendor, 0444, light_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, light_name_show, NULL);
static DEVICE_ATTR(lux, 0444, light_raw_data_show, NULL);
static DEVICE_ATTR(raw_data, 0444, light_raw_data_show, NULL);
static DEVICE_ATTR(dhr_sensor_info, 0444, light_get_dhr_sensor_info_show, NULL);
#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
static DEVICE_ATTR(hallic_info, 0220, NULL, light_hallic_info_store);
static DEVICE_ATTR(light_circle, 0444, light_circle_show, NULL);
static DEVICE_ATTR(debug_info, 0444, light_debug_info_show, NULL);
#endif
#if defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
static DEVICE_ATTR(brightness, 0220, NULL, light_brightness_store);
#endif
#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
static DEVICE_ATTR(set_aod_rect, 0220, NULL, light_set_aod_rect_store);
#endif

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	&dev_attr_dhr_sensor_info,
#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
	&dev_attr_hallic_info,
	&dev_attr_light_circle,
	&dev_attr_debug_info,
#endif
#ifdef CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR
	&dev_attr_register_write,
	&dev_attr_register_read,
#endif
#if defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
	&dev_attr_brightness,
#endif
#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
	&dev_attr_set_aod_rect,
#endif
	NULL,
};

static int __init stk3a9x_light_factory_init(void)
{
	adsp_factory_register(MSG_LIGHT, light_attrs);
	pr_info("[FACTORY] %s\n", __func__);
	return 0;
}

static void __exit stk3a9x_light_factory_exit(void)
{
	adsp_factory_unregister(MSG_LIGHT);
	pr_info("[FACTORY] %s\n", __func__);
	return;
}
module_init(stk3a9x_light_factory_init);
module_exit(stk3a9x_light_factory_exit);
