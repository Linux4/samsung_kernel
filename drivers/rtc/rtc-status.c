/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sec_class.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/rtc.h>
#include <linux/time64.h>

static struct device *rtc_status_dev;
static int rtc_status = 0;

extern struct class *rtc_class;
static int rtc_status_read_status(time64_t stdtime)
{
	int ret = 0, err = 0;
	struct class_dev_iter iter;
	struct device *dev;
	struct rtc_device *rtc = NULL;
	struct rtc_time tm;
	time64_t curtime;

	class_dev_iter_init(&iter, rtc_class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		get_device(dev);

		rtc = to_rtc_device(dev);
		if (rtc) {
			if (!try_module_get(rtc->owner)) {
				put_device(dev);
				err = -ENODEV;
				goto error;
			}
		}

		err = rtc_read_time(rtc, &tm);
		if (err)
			goto error;

		curtime = rtc_tm_to_time64(&tm);

		pr_info("%s: rtc time = %d", __func__, curtime);

		if (curtime <= stdtime) {
			ret = 1;
			break;
		}
	}
	class_dev_iter_exit(&iter);

	return ret;

error:
	class_dev_iter_exit(&iter);
	return err;
}

static ssize_t rtc_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int status = rtc_status;
	pr_info("complete power off status(%d)\n", status);
	rtc_status = 0;
	return sprintf(buf, "%d\n", status);
}

static DEVICE_ATTR(rtc_status, 0444, rtc_status_show, NULL);

static struct attribute *rtc_status_attrs[] = {
	&dev_attr_rtc_status.attr,
	NULL,
};

static struct attribute_group rtc_status_attr_group = {
	.attrs = rtc_status_attrs,
};

static int __init rtc_status_init(void)
{
	int err;

	rtc_status_dev = sec_device_create(NULL, "rtc");
	if(IS_ERR(rtc_status_dev)) {
		pr_err("%s: Failed to create rtc device\n", __func__);
		err = PTR_ERR(rtc_status_dev);
		goto error;
	}

	err = sysfs_create_group(&rtc_status_dev->kobj, &rtc_status_attr_group);
	if (err < 0) {
		pr_err("%s: Failed to create sysfs group\n", __func__);
		goto error;
	}

	rtc_status = rtc_status_read_status(300);
	if (rtc_status < 0) {
		pr_err("%s: Failed to read rtc reset status\n", __func__);
		err = rtc_status;
		rtc_status = 0;
		goto error;
	}

	pr_info("%s: rtc status successfully inited.\n", __func__);

	return 0;

error:
	return err;
}

module_init(rtc_status_init);
MODULE_LICENSE("GPL");
