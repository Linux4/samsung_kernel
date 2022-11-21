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

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>

#include "proximity_factory.h"
#include "../../sensor/proximity.h"
#include "../../sensorhub/shub_device.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_dev_core.h"
#include "../../utility/shub_utility.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

static struct device *proximity_sysfs_device;
static struct device_attribute **chipset_attrs;

static u32 position[6];

static ssize_t prox_probe_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	bool probe_pass_fail;

	probe_pass_fail = get_sensor_probe_state(SENSOR_TYPE_PROXIMITY);
	return snprintf(buf, PAGE_SIZE, "%d\n", probe_pass_fail);
}

static ssize_t thresh_high_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	shub_dbgf("ProxThresh = hi - %u", data->prox_threshold[PROX_THRESH_HIGH]);

	return sprintf(buf, "%u\n", data->prox_threshold[PROX_THRESH_HIGH]);
}

static ssize_t thresh_high_store(struct device *dev, struct device_attribute *attr, const char *buf,
					   size_t size)
{
	u16 uNewThresh;
	int ret, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	ret = kstrtou16(buf, 10, &uNewThresh);
	if (ret < 0) {
		shub_errf("kstrto16 failed.(%d)", ret);
	} else {
		if (uNewThresh & prox_non_bits_mask) {
			shub_errf("allow %ubits.(%d)", PROX_ADC_BITS_NUM, uNewThresh);
		} else {
			uNewThresh &= prox_bits_mask;
			data->prox_threshold[PROX_THRESH_HIGH] = uNewThresh;
		}
	}

	shub_infof("new prox threshold : hi - %u", data->prox_threshold[PROX_THRESH_HIGH]);

	return size;
}

static ssize_t thresh_low_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	shub_infof("ProxThresh = lo - %u", data->prox_threshold[PROX_THRESH_LOW]);

	return sprintf(buf, "%u\n", data->prox_threshold[PROX_THRESH_LOW]);
}

static ssize_t thresh_low_store(struct device *dev, struct device_attribute *attr, const char *buf,
					  size_t size)
{
	u16 uNewThresh;
	int ret, i = 0;
	u16 prox_bits_mask = 0, prox_non_bits_mask = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	while (i < PROX_ADC_BITS_NUM)
		prox_bits_mask += (1 << i++);

	while (i < 16)
		prox_non_bits_mask += (1 << i++);

	ret = kstrtou16(buf, 10, &uNewThresh);
	if (ret < 0) {
		shub_errf("kstrto16 failed.(%d)", ret);
	} else {
		if (uNewThresh & prox_non_bits_mask) {
			shub_errf("allow %ubits.(%d)", PROX_ADC_BITS_NUM, uNewThresh);
		} else {
			uNewThresh &= prox_bits_mask;
			data->prox_threshold[PROX_THRESH_LOW] = uNewThresh;
		}
	}

	shub_infof("new prox threshold : lo - %u", data->prox_threshold[PROX_THRESH_LOW]);

	return size;
}

static ssize_t raw_data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 raw_data = 0;
	s32 ms_delay = 20;
	char tmpe_buf[8] = { 0, };
	struct prox_raw_event *sensor_value =
	    (struct prox_raw_event *)(get_sensor_event(SENSOR_TYPE_PROXIMITY_RAW)->value);

	memcpy(&tmpe_buf[0], &ms_delay, 4);

	if (!get_sensor_enabled(SENSOR_TYPE_PROXIMITY_RAW)) {
		batch_sensor(SENSOR_TYPE_PROXIMITY_RAW, 20, 0);
		enable_sensor(SENSOR_TYPE_PROXIMITY_RAW, NULL, 0);

		msleep(200);
		raw_data = sensor_value->prox_raw;
		disable_sensor(SENSOR_TYPE_PROXIMITY_RAW, NULL, 0);
	} else {
		raw_data = sensor_value->prox_raw;
	}

	return sprintf(buf, "%u\n", raw_data);
}

static ssize_t prox_avg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct proximity_raw_data *data = get_sensor(SENSOR_TYPE_PROXIMITY_RAW)->data;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", data->avg_data[PROX_RAW_MIN], data->avg_data[PROX_RAW_AVG],
			data->avg_data[PROX_RAW_MAX]);
}

static ssize_t prox_avg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	char temp_buf[8] = { 0, };
	int ret;
	int64_t enable;
	s32 ms_delay = 20;

	memcpy(&temp_buf[0], &ms_delay, 4);

	ret = kstrtoll(buf, 10, &enable);
	if (ret < 0)
		shub_errf("- failed = %d", ret);

	if (enable) {
		if (!get_sensor_enabled(SENSOR_TYPE_PROXIMITY_RAW)) {
			batch_sensor(SENSOR_TYPE_PROXIMITY_RAW, 20, 0);
			enable_sensor(SENSOR_TYPE_PROXIMITY_RAW, NULL, 0);
		}
	} else {
		if (get_sensor_enabled(SENSOR_TYPE_PROXIMITY_RAW))
			disable_sensor(SENSOR_TYPE_PROXIMITY_RAW, NULL, 0);
	}

	return size;
}

static ssize_t prox_offset_pass_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

static ssize_t prox_position_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u.%u %u.%u %u.%u\n", position[0], position[1], position[2], position[3], position[4],
		       position[5]);
}

static DEVICE_ATTR_RO(prox_position);
static DEVICE_ATTR_RO(prox_probe);
static DEVICE_ATTR(thresh_high, 0664, thresh_high_show, thresh_high_store);
static DEVICE_ATTR(thresh_low, 0664, thresh_low_show, thresh_low_store);
static DEVICE_ATTR_RO(raw_data);
static DEVICE_ATTR(prox_avg, 0664, prox_avg_show, prox_avg_store);
static DEVICE_ATTR_RO(prox_offset_pass);

static struct device_attribute *proximity_attrs[] = {
	&dev_attr_prox_probe,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_prox_offset_pass,
	NULL,
	NULL,
};

typedef struct device_attribute** (*get_chipset_dev_attrs)(char *);
get_chipset_dev_attrs get_proximity_chipset_dev_attrs[] = {
	get_proximity_gp2ap110s_dev_attrs,
	get_proximity_stk3x6x_dev_attrs,
	get_proximity_stk3328_dev_attrs,
	get_proximity_tmd4912_dev_attrs,
};

static void check_proximity_dev_attr(void)
{
	struct device_node *np = get_shub_device()->of_node;
	int index = 0;

	while (proximity_attrs[index] != NULL)
		index++;

	if (!of_property_read_u32_array(np, "prox-position", position, ARRAY_LEN(position))) {
		if (index < ARRAY_SIZE(proximity_attrs))
			proximity_attrs[index++] = &dev_attr_prox_position;
		shub_info("prox-position - %u.%u %u.%u %u.%u", position[0], position[1], position[2], position[3],
			   position[4], position[5]);
	}
}

void initialize_proximity_sysfs(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	int ret, i;

	ret = sensor_device_create(&proximity_sysfs_device, NULL, "proximity_sensor");
	if (ret < 0) {
		shub_errf("fail to creat %s sysfs device", sensor->name);
		return;
	}

	check_proximity_dev_attr();

	ret = add_sensor_device_attr(proximity_sysfs_device, proximity_attrs);
	if (ret < 0) {
		shub_errf("fail to add %s sysfs device attr", sensor->name);
		return;
	}

	for (i = 0; i < ARRAY_LEN(get_proximity_chipset_dev_attrs); i++) {
		chipset_attrs = get_proximity_chipset_dev_attrs[i](sensor->chipset_name);
		if (chipset_attrs) {
			ret = add_sensor_device_attr(proximity_sysfs_device, chipset_attrs);
			if (ret < 0) {
				shub_errf("fail to add sysfs chipset device attr(%d)", i);
				return;
			}
			break;
		}
	}
}

void remove_proximity_sysfs(void)
{
	if (chipset_attrs)
		remove_sensor_device_attr(proximity_sysfs_device, chipset_attrs);
	remove_sensor_device_attr(proximity_sysfs_device, proximity_attrs);
	sensor_device_destroy(proximity_sysfs_device);
	proximity_sysfs_device = NULL;
}

void initialize_proximity_factory(bool en)
{
	if (!get_sensor_probe_state(SENSOR_TYPE_PROXIMITY))
		return;

	if (en)
		initialize_proximity_sysfs();
	else
		remove_proximity_sysfs();
}
