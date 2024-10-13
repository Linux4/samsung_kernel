/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/of_irq.h>
#include <linux/printk.h>

#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"
#include "npu-afm.h"
#include "npu-afm-debug.h"
#include "npu-hw-device.h"
#include "npu-scheduler.h"

extern struct npu_afm npu_afm;

struct npu_afm_debug_irp afm_irp = {
	.m = 0x0,
	.x = 0x0,
	.r = 0x0,
	.v = 0x0,
	.trw = 0x0,
};

static ssize_t afm_onoff_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "afm_onoff : %s\n", npu_afm.afm_local_onoff ? "on" : "off");

	return ret;
}

static ssize_t afm_onoff_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t len)
{
	int ret = 0;
	unsigned int x = 0;

	if (sscanf(buf, "%u", &x) > 0) {
		if (x > 2) {
			npu_err("Invalid afm onff : %u, please input 0(off) or 1(on)\n", x);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	npu_afm.afm_local_onoff = x;
	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(afm_onoff);

static ssize_t afm_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "afm_mode : %u\n0 - normal, 1 - tdc\n", npu_afm.npu_afm_system->afm_mode);

	return ret;
}

static ssize_t afm_mode_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t len)
{
	int ret = 0;
	unsigned int x = 0;

	if (sscanf(buf, "%u", &x) > 0) {
		if (x > 2) {
			npu_err("Invalid afm mode : %u, please input 0(normal) or 1(tdc)\n", x);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	npu_afm.npu_afm_system->afm_mode = x;
	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(afm_mode);

static ssize_t afm_tdc_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "afm_threshold : %u\n", npu_afm.npu_afm_tdc_threshold);

	return ret;
}

static ssize_t afm_tdc_threshold_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t len)
{
	int ret = 0;
	unsigned int x = 0;

	if (sscanf(buf, "%u", &x) > 0) {
		if (x > 1000000) {
			npu_err("Invalid afm threshold : %u, please input 0 < threshold <= 1000000)\n", x);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	npu_afm.npu_afm_tdc_threshold = x;
	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(afm_tdc_threshold);

static ssize_t afm_irp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "m - Monitor Unit : 1 ~ 64(I_CLK cycles)\n"
			"x - Monitor Interval : (X + 1) * Monitor Unit (x = 0 ~ 15)\n"
			"r - IRP : (R + 2) * Monitor Interval (R =  0 ~ 63)\n"
			"v - Integration Interval : (V + 2) * IRP (V = 0 ~ 6)\n"
			"trw - TRW : (TRW + 1) * Integration Interval (TRW = 0 ~ 31)\n\n"
			"m(%u), x(%u), r(%u), v(%u), trw(%u)\n",
			afm_irp.m, afm_irp.x, afm_irp.r, afm_irp.v, afm_irp.trw);

	return ret;
}

static int check_valid_irp_param(const char *buf, unsigned int m, unsigned int x,
			unsigned int r,	unsigned int v, unsigned int trw)
{
	if (m > 64)
		goto err_exit;
	else if (x > 15)
		goto err_exit;
	else if (r > 63)
		goto err_exit;
	else if (v > 6)
		goto err_exit;
	else if (trw > 31)
		goto err_exit;

	return 0;
err_exit:
	return -1;
}

static ssize_t afm_irp_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t len)
{
	int ret = 0;
	unsigned int m = 0;
	unsigned int x = 0;
	unsigned int r = 0;
	unsigned int v = 0;
	unsigned int trw = 0;

	if (sscanf(buf, "%u %u %u %u %u", &m, &x, &r, &v, &trw) > 0) {
		if (check_valid_irp_param(buf, m, x, r, v, trw) < 0) {
			ret = -EINVAL;
			goto err_exit;
		}
	}

	afm_irp.m = m;
	afm_irp.x = x;
	afm_irp.r = r;
	afm_irp.v = v;
	afm_irp.trw = trw;
	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(afm_irp);

static ssize_t afm_irp_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "afm_irp_debug : %s\n"
					"Please input 0(off) or 1(on)\n",
						npu_afm.npu_afm_irp_debug ? "on" : "off");

	return ret;
}

static ssize_t afm_irp_debug_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t len)
{
	int ret = 0;
	unsigned int x = 0;

	if (sscanf(buf, "%u", &x) > 0) {
		if (x > 2) {
			npu_err("Invalid afm irp debug : %u, please input 0(off) or 1(on)\n", x);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	npu_afm.npu_afm_irp_debug = x;
	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(afm_irp_debug);

static ssize_t afm_restore_msec_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "afm_restore_msec : %u\n"
					"Please input under 100000 msec\n", npu_afm.npu_afm_restore_msec);

	return ret;
}

static ssize_t afm_restore_msec_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t len)
{
	int ret = 0;
	unsigned int x = 0;

	if (sscanf(buf, "%u", &x) > 0) {
		if (x > 100000) {
			npu_err("Invalid afm restore msec : %u, please input under 100000 msec\n", x);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	npu_afm.npu_afm_restore_msec = x;
	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(afm_restore_msec);

static ssize_t afm_tdt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "afm_tdt_debug : %s, afm_tdt : %u\n"
					"Please input 0(off) or 1(on) for tdt_debug\n"
					"Please input under 2048 for tdt\n",
					npu_afm.npu_afm_tdt_debug ? "on" : "off", npu_afm.npu_afm_tdt);

	return ret;
}

static ssize_t afm_tdt_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t len)
{
	int ret = 0;
	unsigned int debug = 0;
	unsigned int tdt = 0;

	if (sscanf(buf, "%u %u", &debug, &tdt) > 0) {
		if (debug > 2) {
			npu_err("Invalid afm tdt debug : %u, please input 0(off) or 1(on)\n", debug);
			ret = -EINVAL;
			goto err_exit;
		}

		if (tdt > 2048) {
			npu_err("Invalid afm restore msec : %u, please input under 2048\n", tdt);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	npu_afm.npu_afm_tdt_debug = debug;
	npu_afm.npu_afm_tdt = tdt;
	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(afm_tdt);

void npu_afm_debug_set_irp(struct npu_system *system)
{
	npu_info("Start set irp debug value.\n");
	npu_info("m(%u), x(%u), r(%u), v(%u), trw(%u)\n",
		afm_irp.m, afm_irp.x, afm_irp.r, afm_irp.v, afm_irp.trw);

	npu_write_hw_reg(npu_get_io_area(system, "htudnc"), 0x48, (afm_irp.m << 0), 0x000000FF, 0);
	npu_write_hw_reg(npu_get_io_area(system, "htudnc"), 0x48, (afm_irp.x << 20), 0x00F00000, 0);
	npu_write_hw_reg(npu_get_io_area(system, "htudnc"), 0x48, (afm_irp.r << 24), 0x0F000000, 0);
	npu_write_hw_reg(npu_get_io_area(system, "htudnc"), 0x20, (afm_irp.v << 11), 0x00003800, 0);
	npu_write_hw_reg(npu_get_io_area(system, "htudnc"), 0x6c, (afm_irp.trw << 3), 0x000000F8, 0);

#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	npu_write_hw_reg(npu_get_io_area(system, "htugnpu1"), 0x48, (afm_irp.m << 0), 0x000000FF, 0);
	npu_write_hw_reg(npu_get_io_area(system, "htugnpu1"), 0x48, (afm_irp.x << 20), 0x00F00000, 0);
	npu_write_hw_reg(npu_get_io_area(system, "htugnpu1"), 0x48, (afm_irp.r << 24), 0x0F000000, 0);
	npu_write_hw_reg(npu_get_io_area(system, "htugnpu1"), 0x20, (afm_irp.v << 11), 0x00003800, 0);
	npu_write_hw_reg(npu_get_io_area(system, "htugnpu1"), 0x6c, (afm_irp.trw << 3), 0x000000F8, 0);
#endif
}

void npu_afm_debug_set_tdt(struct npu_system *system)
{
	npu_info("Start set tdt debug value.\n");
	npu_info("tdt(%u)\n", npu_afm.npu_afm_tdt);

	npu_write_hw_reg(npu_get_io_area(system, "htudnc"), 0x80, (npu_afm.npu_afm_tdt << 20), 0xFFF00000, 0);
#if !IS_ENABLED(CONFIG_SOC_S5E8845)
	npu_write_hw_reg(npu_get_io_area(system, "htugnpu1"), 0x80, (npu_afm.npu_afm_tdt << 20), 0xFFF00000, 0);
#endif
}

int npu_afm_register_debug_sysfs(struct npu_system *system)
{
	int ret = 0;
	struct npu_device *device = container_of(system, struct npu_device, system);

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_afm_onoff.attr);
	if (ret) {
		probe_err("sysfs_create_file afm_onoff error : ret = %d\n", ret);
		goto err_exit;
	}

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_afm_mode.attr);
	if (ret) {
		probe_err("sysfs_create_file afm_mode error : ret = %d\n", ret);
		goto err_exit;
	}

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_afm_tdc_threshold.attr);
	if (ret) {
		probe_err("sysfs_create_file afm_tdc_threshold error : ret = %d\n", ret);
		goto err_exit;
	}

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_afm_irp.attr);
	if (ret) {
		probe_err("sysfs_create_file afm_irp error : ret = %d\n", ret);
		goto err_exit;
	}

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_afm_irp_debug.attr);
	if (ret) {
		probe_err("sysfs_create_file afm_irp_debug error : ret = %d\n", ret);
		goto err_exit;
	}

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_afm_restore_msec.attr);
	if (ret) {
		probe_err("sysfs_create_file afm_restore_msec error : ret = %d\n", ret);
		goto err_exit;
	}

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_afm_tdt.attr);
	if (ret) {
		probe_err("sysfs_create_file afm_tdt error : ret = %d\n", ret);
		goto err_exit;
	}

err_exit:
	return ret;
}
