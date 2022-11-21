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
#include "adsp.h"
#define VENDOR "AMS"
#define CHIP_ID "TMD3725"

#define START_TEST	'0'
#define COUNT_TEST	'1'
#define REGISTER_TEST	'2'
#define DATA_TEST	'3'
#define STOP_TEST	'4'

#define MAX_COUNT 15

static u8 reg_id_table[MAX_COUNT][2] = {
	{0x81, 0}, {0x88, 1}, {0x8F, 2}, {0x96, 3}, {0x9D, 4},
	{0xA4, 5}, {0xAB, 6}, {0xB2, 7}, {0xB9, 8}, {0xC0, 9},
	{0xC7, 10}, {0xCE, 11}, {0xD5, 12}, {0xDC, 13}, {0xE3, 14}
};

static u8 is_beaming;

static ssize_t mobeam_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t mobeam_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t barcode_emul_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct msg_big_data message;
	u8 cmd, i;

	if (buf[0] == 0xFF && buf[1] != 0) {
		if (is_beaming)
			return size;
			is_beaming = 1;
			cmd = NETLINK_MESSAGE_MOBEAM_START;
			message.msg_size = 1;
			memcpy(message.msg, &buf[1], message.msg_size);
	} else if (buf[0] == 0xFF && buf[1] == 0) {
		if (is_beaming == 0)
			return size;
			is_beaming = 0;
			cmd = NETLINK_MESSAGE_MOBEAM_STOP;
			message.msg_size = 1;
			memcpy(message.msg, &buf[1], message.msg_size);
	} else if (buf[0] == 0x00) {
		cmd = NETLINK_MESSAGE_MOBEAM_SEND_DATA;
		message.msg_size = 128;
		memcpy(message.msg, &buf[2], message.msg_size);
	} else if (buf[0] == 0x80) {
		cmd = NETLINK_MESSAGE_MOBEAM_SEND_COUNT;
		message.msg_size = 1;
		memcpy(message.msg, &buf[1], message.msg_size);
	} else {
		u8 send_buf[6];
		for (i = 0; i < MAX_COUNT; i++) {
			if (reg_id_table[i][0] == buf[0])
				send_buf[0] = reg_id_table[i][1];
		}
		send_buf[1] = buf[1];
		send_buf[2] = buf[2];
		send_buf[3] = buf[4];
		send_buf[4] = buf[5];
		send_buf[5] = buf[7];
		cmd = NETLINK_MESSAGE_MOBEAM_SEND_REG;
		message.msg_size = 6;
		memcpy(message.msg, send_buf, message.msg_size);
	}

	message.sensor_type = ADSP_FACTORY_MOBEAM;
	adsp_unicast(&message, sizeof(message), cmd, 0, 0);
	return size;
}

static ssize_t barcode_led_status_show(struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", is_beaming);
}

static ssize_t barcode_ver_check_show(struct device *dev,
    struct device_attribute *attr,
    char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", 15);
}

static ssize_t barcode_emul_test_store(struct device *dev,
    struct device_attribute *attr,
    const char *buf, size_t size)
{
	if (buf[0] == START_TEST) {
		u8 send_buf[2] = {0xFF, 0x01};
		barcode_emul_store(dev, attr, send_buf, sizeof(send_buf));
	} else if (buf[0] == STOP_TEST) {
		u8 send_buf[2] = {0xFF, 0};
		barcode_emul_store(dev, attr, send_buf, sizeof(send_buf));
	} else if (buf[0] == COUNT_TEST) {
		u8 send_buf[2] = {0x80, 15};
		barcode_emul_store(dev, attr, send_buf, sizeof(send_buf));
	} else if (buf[0] == REGISTER_TEST) {
		u8 send_buf[8] = {0x81, 0xAC, 0xDB, 0x36, 0x42, 0x85, 0x0A, 0xA8};
		barcode_emul_store(dev, attr, send_buf, sizeof(send_buf));
	} else {
		u8 send_buf[130] = {0x00, 0xAC, 0xDB, 0x36, 0x42, 0x85, 0x0A, 0xA8, };
		barcode_emul_store(dev, attr, send_buf, sizeof(send_buf));
	}

	return size;
}

static DEVICE_ATTR(vendor, S_IRUGO, mobeam_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, mobeam_name_show, NULL);
static DEVICE_ATTR(barcode_send, S_IWUSR | S_IWGRP, NULL, barcode_emul_store);
static DEVICE_ATTR(barcode_led_status, S_IRUGO,	barcode_led_status_show, NULL);
static DEVICE_ATTR(barcode_ver_check, S_IRUGO, barcode_ver_check_show, NULL);
static DEVICE_ATTR(barcode_test_send, S_IWUSR | S_IWGRP,
    NULL, barcode_emul_test_store);

static struct device_attribute *mobeam_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_barcode_send,
	&dev_attr_barcode_led_status,
	&dev_attr_barcode_ver_check,
	&dev_attr_barcode_test_send,	
	NULL,
};

static int __init initialize_mobeam(void)
{
	adsp_mobeam_register(mobeam_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit remove_mobeam(void)
{
    adsp_mobeam_unregister(mobeam_attrs);
    pr_info("[FACTORY] %s\n", __func__);
}

module_init(initialize_mobeam);
module_exit(remove_mobeam);
