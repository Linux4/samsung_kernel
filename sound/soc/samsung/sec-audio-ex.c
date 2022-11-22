/*
 * sec-audio-ex.c - Sub driver of Samsung devices
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/gpio.h>

static struct class *subarray;
static struct device *amp_on;

static ssize_t sec_audio_subarray_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int value = 0;

	pr_debug("%s()\n", __func__);

	if (gpio_get_value(106)) {
		pr_info("pcb board is sub array!\n");
		value = 1;
	}

	return snprintf(buf, 4, "%d\n", value);
}
static DEVICE_ATTR(amp_state, S_IRUGO, sec_audio_subarray_show, NULL);

static int __init sec_audio_ex_init(void)
{
	int ret = 0;

	/* create subarray class */
	subarray = class_create(THIS_MODULE, "subarray");

	/* create amp device */
	amp_on = device_create(subarray, NULL, 0, NULL, "amp_on");
	if (IS_ERR(amp_on))
		pr_err("Failed to create device (amp_on)!\n");

	ret = device_create_file(amp_on, &dev_attr_amp_state);
	if (ret) {
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_amp_state.attr.name);
	}

	return ret;
}
module_init(sec_audio_ex_init);

static void __exit sec_audio_ex_exit(void)
{
	/* remove device files */
	device_remove_file(amp_on, &dev_attr_amp_state);
	/* destroy devices */
	device_destroy(subarray, 0);
	/* destroy class */
	class_destroy(subarray);
}
module_exit(sec_audio_ex_exit);

MODULE_DESCRIPTION("Samsung Audio Subsystem Device Control Extension");
MODULE_AUTHOR("Sean Kang <sh76.kang@samsung.com>");
MODULE_LICENSE("GPL");
