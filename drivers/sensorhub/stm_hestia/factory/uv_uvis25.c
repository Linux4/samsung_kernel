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
#include "../ssp.h"

#define	VENDOR		"ST"
#define	CHIP_ID		"UVIS25"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/
static ssize_t uv_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t uv_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t uv_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->buf[UV_SENSOR].uv);
}

static ssize_t uv_part_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned char part_value = 0;
	int ret = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	msg->cmd = MSG2SSP_AP_GET_UV_DEVICE_ID;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = &part_value;
	msg->free_buffer = 0;

	ret = ssp_spi_sync(data, msg, 3000);

	if (ret != SUCCESS) {
		pr_err("[SSP] %s - UV GET PART TYPE Timeout!!\n", __func__);
		goto exit;
	}

	ssp_dbg("[SSP] %s - %d (0x%02X)\n", __func__, part_value, part_value);

	exit:
	return snprintf(buf, PAGE_SIZE, "0x%02X", part_value);
}

static DEVICE_ATTR(vendor, S_IRUGO, uv_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, uv_name_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, uv_data_show, NULL);
static DEVICE_ATTR(part_type, S_IRUGO, uv_part_type_show, NULL);

static struct device_attribute *uv_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_raw_data,
	&dev_attr_part_type,
	NULL,
};

void initialize_uv_factorytest(struct ssp_data *data)
{
	sensors_register(data->uv_device, data, uv_attrs, "uv_sensor");
}

void remove_uv_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->uv_device, uv_attrs);
}
