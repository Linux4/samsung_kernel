/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 * Author: Haibing.Yang <haibing.yang@spreadtrum.com>
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)		"sprdfb_sysfs: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <mach/board.h>

#include "sprdfb.h"
#include "sprdfb_panel.h"

static ssize_t sysfs_rd_current_pclk(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret, current_pclk;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct sprdfb_device *fb_dev = (struct sprdfb_device *)fbi->par;

	if (!fb_dev) {
		pr_err("sysfs: fb_dev can't be found\n");
		return -ENXIO;
	}
	ret = snprintf(buf, PAGE_SIZE, "current dpi_clk: %u\ndyna_dpi_clk:%u\n",
			fb_dev->dpi_clock, fb_dev->dyna_dpi_clk);

	return ret;
}

static ssize_t sysfs_write_pclk(struct device *dev,
			struct device_attribute *attr,
			const char *buf, ssize_t count)
{
	int ret;
	int divider;
	u32 dpi_clk_src, new_pclk;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct sprdfb_device *fb_dev = (struct sprdfb_device *)fbi->par;

	if (!fb_dev) {
		pr_err("sysfs: fb_dev can't be found\n");
		return -ENXIO;
	}

	sscanf(buf, "%u,%d\n", &dpi_clk_src, &divider);

	if (divider < 1 || divider > 0xff || dpi_clk_src < 1) {
		pr_warn("sysfs: para is invalid, nothing is done.\n", divider);
		return count;
	}
	if (dpi_clk_src < 1000 && dpi_clk_src > 0)
		dpi_clk_src *= 1000000;

	new_pclk = dpi_clk_src / divider;
	if (new_pclk == fb_dev->dpi_clock) {
		/* Do nothing */
		pr_warn("sysfs: new pclk is the same as current pclk\n");
		return count;
	}
	if (!fb_dev->enable) {
		pr_warn("After fb_dev is resumed, pclk will be changed\n");
		fb_dev->dyna_dpi_clk = new_pclk;
		return count;
	}
	if (fb_dev->ctrl->change_pclk) {
		ret = fb_dev->ctrl->change_pclk(fb_dev, new_pclk);
		if (ret) {
			pr_err("%s: failed to change pclk. ret=%d\n",
					__func__, ret);
			return ret;
		}
	}

	return count;
}

static ssize_t sysfs_rd_current_fps(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	/* FIXME: todo */
	return ret;
}

static ssize_t sysfs_write_fps(struct device *dev,
			struct device_attribute *attr,
			const char *buf, ssize_t count)
{
	int ret = 0;
	/* FIXME: todo */
	return ret;
}

static ssize_t sysfs_rd_current_mipi_clk(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	/* FIXME: todo */
	return ret;
}

static ssize_t sysfs_write_mipi_clk(struct device *dev,
			struct device_attribute *attr,
			const char *buf, ssize_t count)
{
	int ret = 0;
	/* FIXME: todo */
	return ret;
}

static ssize_t sysfs_write_fps_interval(struct device *dev,
			struct device_attribute *attr,
			const char *buf, ssize_t count)
{
	int ret = 0;
	u32 fps_interval, log_enable;

	struct fb_info *fbi = dev_get_drvdata(dev);
	struct sprdfb_device *fb_dev = (struct sprdfb_device *)fbi->par;

	if (!fb_dev) {
		pr_err("sysfs: fb_dev can't be found\n");
		return -ENXIO;
	}

	sscanf(buf, "%u,%d\n", &fps_interval, &log_enable);

	if (fps_interval <= 0)
		fps_interval = 1000/*1 sec*/;

	pr_info("fps_debug message interval=[%d], debug_message_enable=[%d]\n",
						fps_interval, log_enable);

	fb_dev->fps.interval_ms = fps_interval;
	fb_dev->fps.enable_dmesg = log_enable;

	return count;
}

static DEVICE_ATTR(dynamic_pclk, S_IRUGO | S_IWUSR, sysfs_rd_current_pclk,
		sysfs_write_pclk);
static DEVICE_ATTR(dynamic_fps, S_IRUGO | S_IWUSR, sysfs_rd_current_fps,
		sysfs_write_fps);
static DEVICE_ATTR(dynamic_mipi_clk, S_IRUGO | S_IWUSR,
		sysfs_rd_current_mipi_clk, sysfs_write_mipi_clk);
static DEVICE_ATTR(runtime_cur_fps, S_IRUGO | S_IWUSR,
		NULL, sysfs_write_fps_interval);

static struct attribute *sprdfb_fs_attrs[] = {
	&dev_attr_dynamic_pclk.attr,
	&dev_attr_dynamic_fps.attr,
	&dev_attr_dynamic_mipi_clk.attr,
	&dev_attr_runtime_cur_fps.attr,
	NULL,
};

static struct attribute_group sprdfb_attrs_group = {
	.attrs = sprdfb_fs_attrs,
};

int sprdfb_create_sysfs(struct sprdfb_device *fb_dev)
{
	int rc;

	rc = sysfs_create_group(&fb_dev->fb->dev->kobj, &sprdfb_attrs_group);
	if (rc)
		pr_err("sysfs group creation failed, rc=%d\n", rc);

	return rc;
}

void sprdfb_remove_sysfs(struct sprdfb_device *fb_dev)
{
	sysfs_remove_group(&fb_dev->fb->dev->kobj, &sprdfb_attrs_group);
}
