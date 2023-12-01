// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>

#include "pablo-icpu.h"
#include "pablo-icpu-sysfs.h"

#define DECLARE_LOGLEVEL_SHOW(x) \
	static ssize_t show_loglevel_##x(struct device *dev, struct device_attribute *attr, char *buf) \
	{ return scnprintf(buf, PAGE_SIZE, "%d\n", pablo_icpu_##x##_log->level); }

#define DECLARE_LOGLEVEL_STORE(x) \
	static ssize_t store_loglevel_##x(struct device *dev, \
					struct device_attribute *attr, \
					const char *buf, size_t count) \
	{ long _res; \
	int _ret = kstrtol(buf, 0, &_res); \
	if (_ret) return _ret;\
	if (_res > LOGLEVEL_CUSTOM1 || _res < 0) return -EINVAL; \
	pablo_icpu_##x##_log->level = _res; \
	return count; }

#define DECLARE_LOGLEVEL_ATTR(x) \
	struct icpu_logger *get_icpu_##x##_log(void); \
	static struct icpu_logger *pablo_icpu_##x##_log; \
	DECLARE_LOGLEVEL_SHOW(x); \
	DECLARE_LOGLEVEL_STORE(x); \
	static DEVICE_ATTR(x##_loglevel, 0644, show_loglevel_##x, store_loglevel_##x);

#define PABLO_ICPU_GET_LOG(x) \
	pablo_icpu_##x##_log = get_icpu_##x##_log();

DECLARE_LOGLEVEL_ATTR(core);
DECLARE_LOGLEVEL_ATTR(debug);
DECLARE_LOGLEVEL_ATTR(firmware);
DECLARE_LOGLEVEL_ATTR(itf);
DECLARE_LOGLEVEL_ATTR(msgqueue);
DECLARE_LOGLEVEL_ATTR(selftest);
DECLARE_LOGLEVEL_ATTR(hw_itf);
DECLARE_LOGLEVEL_ATTR(hw);
DECLARE_LOGLEVEL_ATTR(mbox);
DECLARE_LOGLEVEL_ATTR(mem);
DECLARE_LOGLEVEL_ATTR(cma);
DECLARE_LOGLEVEL_ATTR(pmem);
DECLARE_LOGLEVEL_ATTR(imgloader);

static struct attribute *icpu_loglevel_entries[] = {
	&dev_attr_core_loglevel.attr,
	&dev_attr_debug_loglevel.attr,
	&dev_attr_firmware_loglevel.attr,
	&dev_attr_itf_loglevel.attr,
	&dev_attr_msgqueue_loglevel.attr,
	&dev_attr_selftest_loglevel.attr,
	&dev_attr_hw_itf_loglevel.attr,
	&dev_attr_hw_loglevel.attr,
	&dev_attr_mbox_loglevel.attr,
	&dev_attr_mem_loglevel.attr,
	&dev_attr_cma_loglevel.attr,
	&dev_attr_pmem_loglevel.attr,
	&dev_attr_imgloader_loglevel.attr,
	NULL,
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct attribute **pablo_icpu_sysfs_get_loglevel_attr_tbl(void)
{
	return icpu_loglevel_entries;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_sysfs_get_loglevel_attr_tbl);
#endif

static struct attribute_group icpu_loglevel_attr_group = {
	.name	= "loglevel",
	.attrs	= icpu_loglevel_entries,
};

#ifdef ENABLE_ICPU_TRACE
#define DECLARE_TRACE_SHOW(x) \
	static ssize_t show_trace_##x(struct device *dev, struct device_attribute *attr, char *buf) \
	{ return scnprintf(buf, PAGE_SIZE, "%s\n", pablo_icpu_##x##_log->trace ? "true" : "false"); }

#define DECLARE_TRACE_STORE(x) \
	static ssize_t store_trace_##x(struct device *dev, \
					struct device_attribute *attr, \
					const char *buf, size_t count) \
	{ long _res; \
	int _ret = kstrtol(buf, 0, &_res); \
	if (_ret) return _ret;\
	pablo_icpu_##x##_log->trace = _res ? true : false; \
	return count; }

#define DECLARE_TRACE_ATTR(x) \
	extern struct icpu_logger *pablo_icpu_##x##_log; \
	DECLARE_TRACE_SHOW(x); \
	DECLARE_TRACE_STORE(x); \
	static DEVICE_ATTR(x##_trace, 0644, show_trace_##x, store_trace_##x);

DECLARE_TRACE_ATTR(itf);

static struct attribute *icpu_trace_entries[] = {
	&dev_attr_itf_trace.attr,
	NULL,
};

static struct attribute_group icpu_trace_attr_group = {
	.name	= "trace",
	.attrs	= icpu_trace_entries,
};
#endif

int pablo_icpu_sysfs_probe(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &icpu_loglevel_attr_group);
	if (ret)
		return ret;

#ifdef ENABLE_ICPU_TRACE
	ret = sysfs_create_group(&dev->kobj, &icpu_trace_attr_group);
	if (ret)
		return ret;
#endif

	PABLO_ICPU_GET_LOG(core);
	PABLO_ICPU_GET_LOG(debug);
	PABLO_ICPU_GET_LOG(firmware);
	PABLO_ICPU_GET_LOG(itf);
	PABLO_ICPU_GET_LOG(msgqueue);
	PABLO_ICPU_GET_LOG(selftest);
	PABLO_ICPU_GET_LOG(hw_itf);
	PABLO_ICPU_GET_LOG(hw);
	PABLO_ICPU_GET_LOG(mbox);
	PABLO_ICPU_GET_LOG(mem);
	PABLO_ICPU_GET_LOG(cma);
	PABLO_ICPU_GET_LOG(pmem);
	PABLO_ICPU_GET_LOG(imgloader);

	return ret;
}

void pablo_icpu_sysfs_remove(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &icpu_loglevel_attr_group);

#ifdef ENABLE_ICPU_TRACE
	sysfs_remove_group(&dev->kobj, &icpu_trace_attr_group);
#endif

}
