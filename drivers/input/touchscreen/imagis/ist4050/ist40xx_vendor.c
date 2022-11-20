/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
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

#include "ist40xx.h"

typedef struct {
	u32 addr;
	u32 byte;
} READ_REG_INFO;

static READ_REG_INFO read_reg_info;

static ssize_t read_reg_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	static READ_REG_INFO *read_info = (READ_REG_INFO *)&read_reg_info;
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	char *buffer = NULL;
	char *buf_value;

	if (buf != NULL && count != 0) {
		buffer = (char *)buf;
		buf_value = strsep(&buffer, ",");
		if (buf_value == NULL) {
			input_err(true, &data->client->dev, "%s buf_value is NULL error\n", __func__);
			return count;
		} else if (kstrtou32(buf_value, 16, &read_info->addr) < 0) {
			input_err(true, &data->client->dev, "%s kstrto32 fail\n", __func__);
			return count;
		}

		buf_value = strsep(&buffer, ",");
		if (buf_value == NULL) {
			input_err(true, &data->client->dev, "%s buf_value is NULL error\n", __func__);
			return count;
		} else if (kstrtou32(buf_value, 10, &read_info->byte) < 0) {
			input_err(true, &data->client->dev, "%s kstrto32 fail\n", __func__);
			return count;
		}
	} else {
		input_err(true, &data->client->dev, "%s input buf data is NULL\n", __func__);
		return count;
	}

	return count;
}

static ssize_t read_reg_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	static READ_REG_INFO *read_info = (READ_REG_INFO *)&read_reg_info;
	int ret = 0;
	int i = 0;
	int idx = 0;
	int count = 0;
	const int msg_len = 256;
	char msg[msg_len];
	u8 *read_reg_data = NULL;
	int read_len = (read_info->byte / IST40XX_DATA_LEN) + 1;
	u32 read_addr = (read_info->addr / IST40XX_ADDR_LEN) * IST40XX_ADDR_LEN;

	if (read_info->addr % 4)
		read_len += 1;

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &data->client->dev,
				"%s: now sys_mode status is not STATE_POWER_ON!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "NG");
	}

	read_reg_data = kzalloc(read_info->byte, GFP_KERNEL);
	if (!read_reg_data) {
		input_err(true, &data->client->dev, "%s() couldn't allocate memory\n", __func__);
		return snprintf(buf, PAGE_SIZE, "NG");
	}

	ist40xx_cmd_hold(data, IST40XX_ENABLE);
	ret = ist40xx_burst_read(data->client, IST40XX_DA_ADDR(read_addr),
			(u32 *)read_reg_data, read_len, true);
	if (ret) {
		kfree(read_reg_data);
		input_err(true, &data->client->dev, "%s() I2C Burst read fail\n", __func__);
		return snprintf(buf, PAGE_SIZE, "NG");
	}
	ist40xx_cmd_hold(data, IST40XX_DISABLE);

	idx = read_info->addr % 4;
	for (i = 0; i < read_info->byte; i++) {
		count += snprintf(msg, msg_len, "%02X ", read_reg_data[idx + i]);
		strncat(buf, msg, msg_len);
	}

	kfree(read_reg_data);

	return count;
}

static ssize_t write_reg_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	int ret = 0;
	int i = 0;
	int idx, idx2 = 0;
	char *buffer = NULL;
	char *buf_value;
	u32 addr;
	u32 byte;
	u8 *write_reg_data = NULL;
	int write_len = 0;
	u32 read_temp = 0;

	if (buf != NULL && count != 0) {
		buffer = (char *)buf;
		buf_value = strsep(&buffer, ",");
		if (buf_value == NULL) {
			input_err(true, &data->client->dev, "%s buf_value is NULL error\n", __func__);
			return count;
		} else if (kstrtou32(buf_value, 16, &addr) < 0) {
			input_err(true, &data->client->dev, "%s kstrto32 fail\n", __func__);
			return count;
		}

		buf_value = strsep(&buffer, ",");
		if (buf_value == NULL) {
			input_err(true, &data->client->dev, "%s buf_value is NULL error\n", __func__);
			return count;
		} else if (kstrtou32(buf_value, 10, &byte) < 0) {
			input_err(true, &data->client->dev, "%s kstrto32 fail\n", __func__);
			return count;
		}

		idx = addr % IST40XX_ADDR_LEN;
		idx2 = IST40XX_ADDR_LEN - (addr + byte) % IST40XX_ADDR_LEN;

		write_len = byte / IST40XX_DATA_LEN;
		write_len += (idx + idx2) / IST40XX_DATA_LEN;
		if ((idx + idx2) % IST40XX_DATA_LEN)
			write_len += 1;

		write_reg_data = kzalloc(write_len, GFP_KERNEL);
		if (!write_reg_data) {
			input_err(true, &data->client->dev, "%s() couldn't allocate memory\n", __func__);
			return count;
		}

		for (i = 0; i < byte; i++) {
			buf_value = strsep(&buffer, ",");
			if (buf_value == NULL) {
				kfree(write_reg_data);
				input_err(true, &data->client->dev, "%s buf_value is NULL error\n", __func__);
				return count;
			} else if (kstrtou8(buf_value, 16, &write_reg_data[idx + i]) < 0) {
				kfree(write_reg_data);
				input_err(true, &data->client->dev, "%s kstrto32 fail\n", __func__);
				return count;
			}
		}
	} else {
		input_err(true, &data->client->dev, "%s input buf data is NULL\n", __func__);
		return count;
	}

	ist40xx_cmd_hold(data, IST40XX_ENABLE);

	if (idx) {
		ret = ist40xx_burst_read(data->client,
				IST40XX_DA_ADDR((addr / IST40XX_ADDR_LEN) * IST40XX_ADDR_LEN),
				&read_temp, 1, true);
		if (ret) {
			kfree(write_reg_data);
			input_err(true, &data->client->dev, "%s() I2C Burst read fail\n", __func__);
			return count;
		}

		for (i = 0; i < idx; i++)
			write_reg_data[i] = (read_temp >> (i * 8)) & 0xFF;
	}

	if (idx2) {
		ret = ist40xx_burst_read(data->client,
				IST40XX_DA_ADDR(((addr + byte) / IST40XX_ADDR_LEN) * IST40XX_ADDR_LEN),
				&read_temp, 1, true);
		if (ret) {
			kfree(write_reg_data);
			input_err(true, &data->client->dev, "%s() I2C Burst read fail\n", __func__);
			return count;
		}

		for (i = 0; i < idx2; i++)
			write_reg_data[idx + byte + i] =
				(read_temp >> (((IST40XX_DATA_LEN - idx2) + i) * 8)) & 0xFF;
	}

	ret = ist40xx_burst_write(data->client,
			IST40XX_DA_ADDR((addr / IST40XX_ADDR_LEN) * IST40XX_ADDR_LEN),
			(u32 *)write_reg_data, write_len);
	if (ret) {
		kfree(write_reg_data);
		input_err(true, &data->client->dev, "%s() I2C Burst write fail\n", __func__);
		return count;
	}

	ist40xx_cmd_hold(data, IST40XX_DISABLE);

	kfree(write_reg_data);

	return count;
}

static DEVICE_ATTR(read_reg_data, S_IRUGO | S_IWUSR | S_IWGRP,
				read_reg_data_show, read_reg_data_store);
static DEVICE_ATTR(write_reg_data, S_IWUSR | S_IWGRP, NULL,
				write_reg_data_store);

static struct attribute *vendor_attributes[] = {
	&dev_attr_read_reg_data.attr,
	&dev_attr_write_reg_data.attr,
	NULL,
};

static struct attribute_group vendor_attr_group = {
	.attrs = vendor_attributes,
};

int ist40xx_create_vendor_sysfs(struct ist40xx_data *data)
{
	return sysfs_create_group(&data->sec.fac_dev->kobj, &vendor_attr_group);
}

void ist40xx_remove_vendor_sysfs(struct ist40xx_data *data)
{
	sysfs_remove_group(&data->sec.fac_dev->kobj, &vendor_attr_group);
}
