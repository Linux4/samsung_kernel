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

static int pid;
/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/

static char operation_mode_flag[11];

static ssize_t dumpstate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct msg_data message;

	message.sensor_type = ADSP_FACTORY_SSC_CORE;
	if (pid != 0) {
		pr_info("[FACTORY] to take the logs\n");
		message.param1 = 0;
		adsp_unicast(&message, sizeof(message),
			NETLINK_MESSAGE_DUMPSTATE, 0, 0);
	} else {
		message.param1 = 2;
		adsp_unicast(&message, sizeof(message),
			NETLINK_MESSAGE_DUMPSTATE, 0, 0);
		pr_info("[FACTORY] logging service was stopped %d\n", pid);
	}

	return snprintf(buf, PAGE_SIZE, "SSC_CORE\n");
}

static ssize_t operation_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s", operation_mode_flag);
}

static ssize_t operation_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int i;

	for (i = 0; i < 10 && buf[i] != '\0'; i++)
		operation_mode_flag[i] = buf[i];
	operation_mode_flag[i] = '\0';

	pr_info("[FACTORY] %s: operation_mode_flag = %s\n", __func__,
		operation_mode_flag);
	return size;
}

static DEVICE_ATTR(dumpstate, S_IRUSR | S_IRGRP, dumpstate_show, NULL);
static DEVICE_ATTR(operation_mode, S_IRUGO | S_IWUSR | S_IWGRP,
	operation_mode_show, operation_mode_store);

static ssize_t mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct msg_data message;
	unsigned long timeout;

	if (pid != 0) {
		pr_info("[FACTORY] To stop logging %d\n", pid);
		timeout = jiffies + (2 * HZ);
		message.sensor_type = ADSP_FACTORY_SSC_CORE;
		message.param1 = 1;
		adsp_unicast(&message, sizeof(message),
			NETLINK_MESSAGE_DUMPSTATE, 0, 0);

		while (pid != 0) {
			msleep(20);
			if (time_after(jiffies, timeout))
				pr_info("[FACTORY] %s: Timeout!!!\n", __func__);
		}
	}
	pr_info("[FACTORY] PID %d\n", pid);

	if (pid != 0)
		return snprintf(buf, PAGE_SIZE, "1\n");

	return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int data = 0;
	struct msg_data message;

	if (kstrtoint(buf, 10, &data)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	if (data != 1) {
		pr_err("[FACTORY] %s: data was wrong\n", __func__);
		return -EINVAL;
	}

	if (pid != 0) {
		message.sensor_type = ADSP_FACTORY_SSC_CORE;
		message.param1 = 1;
		adsp_unicast(&message, sizeof(message),
			NETLINK_MESSAGE_DUMPSTATE, 0, 0);
	}
	return size;
}
static DEVICE_ATTR(mode, S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP,
	mode_show, mode_store);

static ssize_t pid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", pid);
}

static ssize_t pid_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int data = 0;

	if (kstrtoint(buf, 10, &data)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	pid = data;
	pr_info("[FACTORY] %s: pid %d\n", __func__, pid);

	return size;
}
static DEVICE_ATTR(ssc_pid, S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP,
	pid_show, pid_store);

static ssize_t remove_sensor_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int type = ADSP_FACTORY_SENSOR_MAX;
	struct adsp_data *data = dev_get_drvdata(dev);

	if (kstrtouint(buf, 10, &type)) {
		pr_err("[FACTORY] %s: kstrtouint fail\n", __func__);
		return -EINVAL;
	}

	if (type > ADSP_FACTORY_PROX && type != ADSP_FACTORY_HH_HOLE) {
		pr_err("[FACTORY] %s: Invalid type %u\n", __func__, type);
		return size;
	}

	pr_info("[FACTORY] %s: type = %u\n", __func__, type);

	mutex_lock(&data->remove_sysfs_mutex);
	adsp_factory_unregister(type);
	mutex_unlock(&data->remove_sysfs_mutex);

	return size;
}
static DEVICE_ATTR(remove_sysfs, S_IWUSR | S_IWGRP,
	NULL, remove_sensor_sysfs_store);

static struct device_attribute *core_attrs[] = {
	&dev_attr_dumpstate,
	&dev_attr_operation_mode,
	&dev_attr_mode,
	&dev_attr_ssc_pid,
	&dev_attr_remove_sysfs,
	NULL,
};

static int __init core_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_SSC_CORE, core_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit core_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_SSC_CORE);

	pr_info("[FACTORY] %s\n", __func__);
}
module_init(core_factory_init);
module_exit(core_factory_exit);
