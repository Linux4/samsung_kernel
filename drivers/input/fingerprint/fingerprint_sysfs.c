/*
 * Copyright (C) 2017 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

/*
 *  fingerprint sysfs class
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>

int adm_flag[] = {0};

int finger_sysfs = 0;
EXPORT_SYMBOL_GPL(finger_sysfs);

struct class *fingerprint_class;
struct device *adm_dev;

/*
 * Create sysfs interface
 */

static ssize_t fingerprint_adm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    pr_info("%s: finger_sysfs = %d\n", __func__, finger_sysfs);

    if (finger_sysfs)
        return snprintf(buf, PAGE_SIZE, "%d\n", 1);
    else
        return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static DEVICE_ATTR(adm, 0444, fingerprint_adm_show, NULL);

static struct device_attribute *fps_attrs[] = {
    &dev_attr_adm,
    NULL,
};

static int __init fingerprint_class_init(void)
{
    int i = 0;

    pr_info("%s\n", __func__);

    fingerprint_class = class_create(THIS_MODULE, "fingerprint");

    if (IS_ERR(fingerprint_class)) {
        pr_err("%s: create fingerprint_class is failed.\n", __func__);
        return PTR_ERR(fingerprint_class);
    }

    fingerprint_class->dev_uevent = NULL;

    adm_dev = device_create(fingerprint_class, NULL, 0, NULL, "fingerprint");

    if (IS_ERR(adm_dev)) {
        pr_err("%s: device_create failed!\n", __func__);
        class_destroy(fingerprint_class);
        return PTR_ERR(adm_dev);
    }

    for (i = 0; fps_attrs[i] != NULL; i++)
    {
        adm_flag[i] = device_create_file(adm_dev, fps_attrs[i]);

        if (adm_flag[i]) {
            pr_err("%s: fail device_create_file (adm_dev, fps_attrs[%d])\n", __func__, i);
            device_destroy(fingerprint_class, 0);
            class_destroy(fingerprint_class);
            return adm_flag[i];
        }
    }

    return 0;
}

static void __exit fingerprint_class_exit(void)
{
    int i = 0;

    for (i = 0; fps_attrs[i] != NULL; i++)
    {
        if (!adm_flag[i]) {
            device_remove_file(adm_dev, &dev_attr_adm);
            device_destroy(fingerprint_class, 0);
            class_destroy(fingerprint_class);
        }
    }
    fingerprint_class = NULL;
}

subsys_initcall(fingerprint_class_init);
module_exit(fingerprint_class_exit);

MODULE_DESCRIPTION("fingerprint sysfs class");
MODULE_LICENSE("GPL");

