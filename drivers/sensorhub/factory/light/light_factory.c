/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include "../../sensor/light.h"
#include "../../comm/shub_comm.h"
#include "../../sensorhub/shub_device.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_dev_core.h"
#include "../../utility/shub_utility.h"

#include <linux/device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h>

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/
static struct device *light_sysfs_device;
static u32 light_position[6];

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);

	return sprintf(buf, "%s\n", sensor->chipset_name);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);

	return sprintf(buf, "%s\n", sensor->vendor);
}

static ssize_t lux_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct light_event *sensor_value = (struct light_event *)(get_sensor_event(SENSOR_TYPE_LIGHT)->value);

	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n", sensor_value->r, sensor_value->g, sensor_value->b, sensor_value->w,
		       sensor_value->a_time, sensor_value->a_gain);
}

static ssize_t raw_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct light_event *sensor_value = (struct light_event *)(get_sensor_event(SENSOR_TYPE_LIGHT)->value);

	return sprintf(buf, "%u,%u,%u,%u,%u,%u\n", sensor_value->r, sensor_value->g, sensor_value->b, sensor_value->w,
		       sensor_value->a_time, sensor_value->a_gain);
}

static ssize_t light_circle_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u.%u %u.%u %u.%u\n", light_position[0], light_position[1],
		       light_position[2], light_position[3], light_position[4],
		       light_position[5]);
}

static ssize_t hall_ic_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	u8 hall_ic = 0;

	if (!get_sensor_probe_state(SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS))
		return -ENOENT;
	if (!buf)
		return -EINVAL;

	if (kstrtou8(buf, 10, &hall_ic) < 0)
		return -EINVAL;

	shub_infof("%d", hall_ic);

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT_AUTOBRIGHTNESS, HALL_IC_STATUS, (char *)&hall_ic,
				sizeof(hall_ic));
	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return size;
	}

	return size;
}

static ssize_t coef_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	char *coef_buf = NULL;
	int coef_buf_length = 0;
	int temp_coef[7] = {0, };
	struct light_data *data = get_sensor(SENSOR_TYPE_LIGHT)->data;

	if (data->light_coef) {
		ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_LIGHT, LIGHT_COEF, 1000, NULL, 0, &coef_buf,
					&coef_buf_length, true);

		if (ret < 0) {
			shub_errf("shub_send_command_wait Fail %d", ret);
			return ret;
		}

		if (coef_buf_length != 28) {
			shub_errf("buffer length error %d", coef_buf_length);
			kfree(coef_buf);
			return -EINVAL;
		}

		memcpy(temp_coef, coef_buf, sizeof(temp_coef));

		shub_infof("%d %d %d %d %d %d %d\n", temp_coef[0], temp_coef[1], temp_coef[2], temp_coef[3],
			   temp_coef[4], temp_coef[5], temp_coef[6]);

		ret = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d\n", temp_coef[0], temp_coef[1], temp_coef[2],
			       temp_coef[3], temp_coef[4], temp_coef[5], temp_coef[6]);

		kfree(coef_buf);
	} else {
		ret = snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0,0\n");
	}

	return ret;
}

static ssize_t sensorhub_ddi_spi_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int retries = 0;
	char *buffer = NULL;
	int buffer_len = 0;
	short copr = 0;

retry:
	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_LIGHT, DDI_COPR, 1000, NULL, 0, &buffer, &buffer_len,
				     true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait fail %d", ret);

		if (retries++ < 2) {
			shub_errf("fail, retry");
			mdelay(5);
			goto retry;
		}
		return ret;
	}

	if (buffer_len != sizeof(copr)) {
		shub_errf("buffer length error %d", buffer_len);
		kfree(buffer);
		return -EINVAL;
	}
	memcpy(&copr, buffer, sizeof(copr));

	shub_infof("%d", copr);

	return snprintf(buf, PAGE_SIZE, "%d\n", copr);
}

static ssize_t test_copr_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int retries = 0;
	short copr[4];
	char *buffer = NULL;
	int buffer_len = 0;

	memset(copr, 0, sizeof(copr));
retry:
	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_LIGHT, TEST_COPR, 1000, NULL, 0, &buffer, &buffer_len,
				     true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait fail %d", ret);

		if (retries++ < 2) {
			shub_errf("fail, retry");
			mdelay(5);
			goto retry;
		}
		return ret;
	}

	if (buffer_len != sizeof(copr)) {
		shub_errf("buffer length error %d", buffer_len);
		kfree(buffer);
		return -EINVAL;
	}
	memcpy(&copr, buffer, sizeof(copr));

	shub_infof("%d, %d, %d, %d", copr[0], copr[1], copr[2], copr[3]);

	return snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d\n", copr[0], copr[1], copr[2], copr[3]);
}

static ssize_t copr_roix_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	int ret = 0;
	int retries = 0;
	char *buffer = NULL;
	int buffer_len = 0;
	short copr[12];

	memset(copr, 0, sizeof(copr));
retry:
	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_LIGHT, COPR_ROIX, 1000, NULL, 0, &buffer, &buffer_len,
				     true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait fail %d", ret);
		if (retries++ < 2) {
			shub_errf("fail, retry");
			mdelay(5);
			goto retry;
		}
		return ret;
	}

	if (buffer_len != sizeof(copr)) {
		shub_errf("buffer length error %d", buffer_len);
		kfree(buffer);
		return -EINVAL;
	}
	memcpy(&copr, buffer, sizeof(copr));

	shub_infof("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", __func__, copr[0], copr[1], copr[2],
		copr[3], copr[4], copr[5], copr[6], copr[7], copr[8], copr[9], copr[10],
		copr[11]);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", copr[0], copr[1], copr[2],
			copr[3], copr[4], copr[5], copr[6], copr[7], copr[8], copr[9],
			copr[10], copr[11]);
}

struct light_debug_info {
	uint32_t stdev;
	uint32_t moving_stdev;
	uint32_t mode;
	uint32_t brightness;
	uint32_t min_div_max;
	uint32_t lux;
} __attribute__((__packed__));

static ssize_t debug_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_len = 0;
	struct light_debug_info debug_info;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_LIGHT, DEBUG_INFO, 1000, NULL, 0, &buffer, &buffer_len,
				     false);

	if (ret < 0) {
		shub_errf("shub_send_command_wait fail %d", ret);
		return ret;
	}

	if (buffer == NULL) {
		shub_errf("buffer is null");
		return -EINVAL;
	}

	if (buffer_len != sizeof(debug_info)) {
		shub_errf("buffer length error %d", buffer_len);
		kfree(buffer);
		return -EINVAL;
	}

	memcpy(&debug_info, buffer, sizeof(debug_info));

	return snprintf(buf, PAGE_SIZE, "%u, %u, %u, %u, %u, %u\n", debug_info.stdev, debug_info.moving_stdev,
			debug_info.mode, debug_info.brightness, debug_info.min_div_max, debug_info.lux);
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(lux);
static DEVICE_ATTR_RO(raw_data);
static DEVICE_ATTR_RO(light_circle);
static DEVICE_ATTR_RO(coef);
static DEVICE_ATTR(hall_ic, 0220, NULL, hall_ic_store);
static DEVICE_ATTR_RO(sensorhub_ddi_spi_check);
static DEVICE_ATTR_RO(test_copr);
static DEVICE_ATTR_RO(copr_roix);
static DEVICE_ATTR_RO(debug_info);

static struct device_attribute *light_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_lux,
	&dev_attr_raw_data,
	&dev_attr_hall_ic,
	&dev_attr_debug_info,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static void check_light_dev_attr(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);
	struct light_data *data = sensor->data;
	struct device_node *np = get_shub_device()->of_node;
	int index = 0;

	while (light_attrs[index] != NULL)
		index++;

	if (!of_property_read_u32_array(np, "light-position", light_position, ARRAY_LEN(light_position))) {
		if (index < ARRAY_SIZE(light_attrs))
			light_attrs[index++] = &dev_attr_light_circle;
		shub_info("light-position - %u.%u %u.%u %u.%u", light_position[0], light_position[1],
			   light_position[2], light_position[3], light_position[4], light_position[5]);
	}

	if (data->light_coef && index < ARRAY_SIZE(light_attrs))
		light_attrs[index++] = &dev_attr_coef;

	if (data->ddi_support && index + 3 <= ARRAY_SIZE(light_attrs)) {
		light_attrs[index++] = &dev_attr_sensorhub_ddi_spi_check;
		light_attrs[index++] = &dev_attr_test_copr;
		light_attrs[index++] = &dev_attr_copr_roix;
	}
}

void initialize_light_sysfs(void)
{
	int ret;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT);

	ret = sensor_device_create(&light_sysfs_device, NULL, "light_sensor");
	if (ret < 0) {
		shub_errf("fail to creat %s sysfs device", sensor->name);
		return;
	}

	check_light_dev_attr();

	ret = add_sensor_device_attr(light_sysfs_device, light_attrs);
	if (ret < 0) {
		shub_errf("fail to add %s sysfs device attr", sensor->name);
		return;
	}
}

void remove_light_sysfs(void)
{
	remove_sensor_device_attr(light_sysfs_device, light_attrs);
	sensor_device_destroy(light_sysfs_device);
	light_sysfs_device = NULL;
}

void initialize_light_factory(bool en)
{
	if (!get_sensor_probe_state(SENSOR_TYPE_LIGHT))
		return;
	if (en)
		initialize_light_sysfs();
	else
		remove_light_sysfs();
}
