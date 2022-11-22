/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Motto Support Driver
 * Author: Minwoo Kim <minwoo7945.kim@samsung.com>
 *         Kimyung Lee <kernel.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/device.h>

#include "dsim.h"
#include "dsim_motto.h"
#include "panels/exynos_panel_drv.h"

static int dsim_set_swing_level(struct dsim_device *dsim, int level)
{
	int ret = 0;

	if (level < 0 || level > DSIM_SUPPORT_SWING_LEVEL) {
		dsim_err("[DSIM:ERR]:%s invalid level : %d\n", __func__, level);
		goto err_set;
	}

	dsim->motto_info.tune_swing = DSIM_TUNE_SWING_EN | SET_DSIM_SWING_LEVEL(level);
	if (IS_DSIM_ON_STATE(dsim)) {
		ret = dsim_reg_set_phy_swing_level(dsim->id);
	}

err_set:
	return ret;
}

static int dsim_set_impedance_level(struct dsim_device *dsim, int level)
{
	int ret = 0;

	if (level < 0 || level > DSIM_SUPPORT_IMPEDANCE_LEVEL) {
		dsim_err("[DSIM:ERR]:%s invalid level : %d\n", __func__, level);
		goto err_set;
	}

	dsim->motto_info.tune_impedance = DSIM_TUNE_IMPEDANCE_EN | SET_DSIM_IMPEDANCE_LEVEL(level);
	dsim_info("[DSIM:TUNE]:%s:%x\n", __func__, dsim->motto_info.tune_impedance);
	if (IS_DSIM_ON_STATE(dsim)) {
		ret = dsim_reg_set_phy_impedance_level(dsim->id);
	}

err_set:
	return ret;

}

static int dsim_set_emphasis_level(struct dsim_device *dsim, int level)
{
	int ret = 0;

	if (level < 0 || level > DSIM_SUPPORT_EMPHASIS_LEVEL) {
		dsim_err("[DSIM:ERR]:%s invalid level : %d\n", __func__, level);
		goto err_set;
	}

	dsim->motto_info.tune_emphasis = DSIM_TUNE_EMPHASIS_EN | SET_DSIM_EMPHASIS_LEVEL(level);
	if (IS_DSIM_ON_STATE(dsim)) {
		ret = dsim_reg_set_phy_emphasis_value(dsim->id);
	}

err_set:
	return ret;
}

static ssize_t swing_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct dsim_motto_info *motto = dev_get_drvdata(dev);
	struct dsim_device *dsim = container_of(motto, struct dsim_device, motto_info);

	dsim_info("[DSIM:INFO]:%s swing : %d\n", __func__, dsim->motto_info.tune_swing);

	len = sprintf(buf, "%d %d ",
		DSIM_SUPPORT_SWING_LEVEL + 1, dsim->motto_info.init_swing);

	if (dsim->motto_info.tune_swing & DSIM_TUNE_SWING_EN)
		len += sprintf(buf + len, "%d\n", GET_DSIM_SWING_LEVEL(dsim->motto_info.tune_swing));
	else
		len += sprintf(buf + len, "%d\n", dsim->motto_info.init_swing);

	return len;
}


static ssize_t swing_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, value;
	struct dsim_motto_info *motto = dev_get_drvdata(dev);
	struct dsim_device *dsim = container_of(motto, struct dsim_device, motto_info);

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0) {
		dsim_err("[DSIM:ERR]:%s: invalid param : %s\n", __func__, buf);
		goto err_store;
	}

	dsim_info("[DSIM:TUNE]:%s : value : %d\n", __func__, value);

	ret = dsim_set_swing_level(dsim, value);
	if (ret) {
		dsim_err("[DSIM:ERR]:%s: failed to set dsim's swing level\n", __func__);
	}

err_store:
	return size;
}

static ssize_t impedance_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct dsim_motto_info *motto = dev_get_drvdata(dev);
	struct dsim_device *dsim = container_of(motto, struct dsim_device, motto_info);

	dsim_info("[DSIM:TUNE]:%s strength : %d\n", __func__, dsim->motto_info.tune_impedance);

	len = sprintf(buf, "%d %d ",
		DSIM_SUPPORT_IMPEDANCE_LEVEL + 1, dsim->motto_info.init_impedance);

	if (dsim->motto_info.tune_impedance & DSIM_TUNE_IMPEDANCE_EN)
		len += sprintf(buf + len, "%d\n", SET_DSIM_IMPEDANCE_LEVEL(dsim->motto_info.tune_impedance));
	else
		len += sprintf(buf + len, "%d\n", dsim->motto_info.init_impedance);

	return len;
}

static ssize_t impedance_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, value;
	struct dsim_motto_info *motto = dev_get_drvdata(dev);
	struct dsim_device *dsim = container_of(motto, struct dsim_device, motto_info);

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0) {
		dsim_err("[DSIM:ERR]:%s: invalid param : %s\n", __func__, buf);
		goto err_store;
	}

	dsim_info("[DSIM:TUNE]:%s : value : %d\n", __func__, value);

	ret = dsim_set_impedance_level(dsim, value);
	if (ret) {
		dsim_err("[DSIM:ERR]:%s: failed to set dsim's swing level\n", __func__);
	}

err_store:
	return size;
}

static ssize_t emphasis_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct dsim_motto_info *motto = dev_get_drvdata(dev);
	struct dsim_device *dsim = container_of(motto, struct dsim_device, motto_info);

	dsim_info("[DSIM:INFO]:%s Deemphasis : %d\n", __func__, dsim->motto_info.tune_emphasis);

	len = sprintf(buf, "%d %d ",
		DSIM_SUPPORT_EMPHASIS_LEVEL + 1, dsim->motto_info.init_emphasis);

	if (dsim->motto_info.tune_emphasis & DSIM_TUNE_EMPHASIS_EN)
		len += sprintf(buf + len, "%d\n", SET_DSIM_EMPHASIS_LEVEL(dsim->motto_info.tune_emphasis));
	else
		len += sprintf(buf + len, "%d\n", dsim->motto_info.init_emphasis);

	return len;
}

static ssize_t emphasis_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, value;
	struct dsim_motto_info *motto = dev_get_drvdata(dev);
	struct dsim_device *dsim = container_of(motto, struct dsim_device, motto_info);

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0) {
		dsim_err("[DSIM:ERR]:%s: invalid param : %s\n", __func__, buf);
		goto err_store;
	}

	dsim_info("[DSIM:TUNE]:%s : value : %d\n", __func__, value);

	ret = dsim_set_emphasis_level(dsim, value);
	if (ret) {
		dsim_err("[DSIM:ERR]:%s: failed to set dsim's swing level\n", __func__);
	}

err_store:
	return size;
}


static DEVICE_ATTR(swing, 0664, swing_show, swing_store);
static DEVICE_ATTR(impedance, 0664, impedance_show, impedance_store);
static DEVICE_ATTR(emphasis, 0664, emphasis_show, emphasis_store);


static struct attribute *motto_tune_attrs[] = {
	&dev_attr_swing.attr,
	&dev_attr_impedance.attr,
	&dev_attr_emphasis.attr,
	NULL,
};

static const struct attribute_group motto_tune_group = {
	.attrs = motto_tune_attrs
};

int create_motto_tune_sysfs(struct dsim_motto_info *motto)
{
	int ret = 0;
	struct dsim_device *dsim;
	unsigned int swing;
	unsigned int impedance;
	unsigned int emphasis;

	dsim = container_of(motto, struct dsim_device, motto_info);

	swing = dsim_phy_extra_read(dsim->id, DSIM_PHY_BIAS_CON2);
	motto->init_swing = DSIM_PHY_BIAS0_REG400_GET(swing);
	dsim_info("[DSIM:TUNE]:%s init swing value : %x\n", __func__, motto->init_swing);

	impedance = dsim_phy_read(dsim->id, DSIM_PHY_MC_ANA_CON0);
	motto->init_impedance = DSIM_PHY_RES_DN_GET(impedance);
	dsim_info("[DSIM:TUNE]:%s init impedance value : %x\n", __func__, motto->init_impedance);

	emphasis = dsim_phy_read(dsim->id, DSIM_PHY_MC_ANA_CON1);
	motto->init_emphasis = DSIM_PHY_EMPHASIS_GET(emphasis);
	dsim_info("[DSIM:TUNE]:%s init emphasis value : %x\n", __func__, motto->init_emphasis);

	ret = sysfs_create_group(&motto->dev->kobj, &motto_tune_group);
	if (ret) {
		dsim_err("[DSIM:ERR]:%s failed to create motto's nodes\n", __func__);
		goto err_create;
	}
err_create:
	return 0;
}

int dsim_motto_probe(struct dsim_device *dsim)
{
	struct dsim_motto_info *motto = &dsim->motto_info;
	struct panel_device *panel;
	int ret = 0;

	motto->class = class_create(THIS_MODULE, "motto");
	if (IS_ERR_OR_NULL(motto->class)) {
		pr_err("failed to create motto class\n");
		ret = -EINVAL;
		goto err;
	}

	motto->dev = device_create(motto->class,
			dsim->dev, 0, &motto, "motto");
	if (IS_ERR_OR_NULL(motto->dev)) {
		pr_err("failed to create motto device\n");
		ret = -EINVAL;
		goto err;
	}

	dev_set_drvdata(motto->dev, motto);

	create_motto_tune_sysfs(motto);

/* create symlink into panel */
	panel = container_of(dsim->panel->panel_drv_sd, struct panel_device, sd);
	if (IS_ERR_OR_NULL(panel->lcd)) {
		pr_err("%s: failed to link motto device into panel. panel_lcd is null\n", __func__);
		goto err;
	}
	ret = sysfs_create_link(&panel->lcd->dev.kobj, &motto->dev->kobj, "motto");
	if (ret)
		pr_err("%s: failed to create symlink\n", __func__);
	else
		pr_info("%s: success to create symlink\n", __func__);

err:
	return 0;
}
