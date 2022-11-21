/****************************************************************************
*
* gc_hal_kernel_sysfs.h
*
* Author: Watson Wang <zswang@marvell.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version. *
*
*****************************************************************************/


#ifndef __gc_hal_kernel_sysfs_h_
#define __gc_hal_kernel_sysfs_h_

#include "gc_hal_options.h"

#include <linux/device.h>
#include <linux/platform_device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define gc_sysfs_attr_ro(_name) \
static struct device_attribute gc_attr_##_name = \
__ATTR(_name, 0444, show_##_name, NULL)

#define gc_sysfs_attr_rw(_name) \
static struct device_attribute gc_attr_##_name = \
__ATTR(_name, 0644, show_##_name, store_##_name)

#define SYSFS_VERIFY_INPUT(_func, _val) \
{ \
    int ret = _func; \
    if(ret != _val) { \
        printk("input error, return %d!\n", ret); \
        return -EINVAL; \
    } \
}

#define SYSFS_VERIFY_INPUT_RANGE(_value, _min, _max) \
{ \
    if((_value < _min) || (_value > _max)) \
    { \
        printk("input value %d is out of range [%d, %d]\n", _value, _min, _max); \
        return -EDOM; \
    } \
}

#if MRVL_CONFIG_SYSFS
extern void create_gc_sysfs_file(struct platform_device *pdev);
extern void remove_gc_sysfs_file(struct platform_device *pdev);
#else
static inline void create_gc_sysfs_file(struct platform_device *pdev)
{ return; }
static inline void remove_gc_sysfs_file(struct platform_device *pdev)
{ return; }
#endif

#if MRVL_CONFIG_POWER_VALIDATION
extern int create_sysfs_file_pm_test(struct platform_device *pdev, struct kobject *kobj);
extern void remove_sysfs_file_pm_test(struct platform_device *pdev);
#else
static inline int create_sysfs_file_pm_test(struct platform_device *pdev, struct kobject *kobj)
{ return 0; }
static inline void remove_sysfs_file_pm_test(struct platform_device *pdev)
{ return; }
#endif


#ifdef __cplusplus
}
#endif

#endif /* __gc_hal_kernel_sysfs_h_ */
