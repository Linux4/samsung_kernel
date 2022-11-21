/*
 * ccic_sysfs.c
 *
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ccic/ccic_sysfs.h>
#ifdef CONFIG_CCIC_S2MU004
#include <linux/ccic/usbpd.h>
#include <linux/ccic/usbpd-s2mu004.h>
#endif
static ssize_t ccic_cur_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_err("Need implementation\n");
	return 0;
}
static DEVICE_ATTR(cur_version, 0444, ccic_cur_ver_show, NULL);

static ssize_t ccic_src_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_err("Need implementation\n");
	return 0;
}
static DEVICE_ATTR(src_version, 0444, ccic_src_ver_show, NULL);

static ssize_t ccic_show_manual_lpm_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mu004_usbpd_data *usbpd_data = dev_get_drvdata(dev);

	if (!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}

	return sprintf(buf, "%d\n", usbpd_data->lpm_mode);
}
static ssize_t ccic_store_manual_lpm_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mu004_usbpd_data *usbpd_data = dev_get_drvdata(dev);
	int mode;

	if (!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}

	sscanf(buf, "%d", &mode);
	pr_info("usb: %s mode=%d\n", __func__, mode);

	mutex_lock(&usbpd_data->lpm_mutex);
	if (mode == 1 || mode == 2)
		s2mu004_set_lpm_mode(usbpd_data);
	else
		s2mu004_set_normal_mode(usbpd_data);
	mutex_unlock(&usbpd_data->lpm_mutex);

	return size;
}
static DEVICE_ATTR(lpm_mode, 0664,
		ccic_show_manual_lpm_mode, ccic_store_manual_lpm_mode);

static ssize_t ccic_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mu004_usbpd_data *usbpd_data = dev_get_drvdata(dev);

	if (!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	} else {
		struct usbpd_data *pd_data = dev_get_drvdata(usbpd_data->dev);

		if (!pd_data) {
			pr_err("%s pd_data is null!!\n", __func__);
			return -ENODEV;
		}

		return sprintf(buf, "%d\n", pd_data->policy.plug_valid);
	}
}
static DEVICE_ATTR(state, 0444, ccic_state_show, NULL);

#if defined(CONFIG_SEC_FACTORY)
static ssize_t ccic_rid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mu004_usbpd_data *usbpd_data = dev_get_drvdata(dev);

	if (!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}
	return sprintf(buf, "%d\n", usbpd_data->rid == REG_RID_MAX ? REG_RID_OPEN : usbpd_data->rid);
}
static DEVICE_ATTR(rid, 0444, ccic_rid_show, NULL);

static ssize_t ccic_store_control_option_command(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct s2mu004_usbpd_data *usbpd_data = dev_get_drvdata(dev);
	int cmd;

	if (!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}

	sscanf(buf, "%d", &cmd);
	pr_info("usb: %s mode=%d\n", __func__, cmd);

	s2mu004_control_option_command(usbpd_data, cmd);

	return size;
}
static DEVICE_ATTR(ccic_control_option, 0220, NULL, ccic_store_control_option_command);

static ssize_t ccic_booting_dry_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_info("%s booting_run_dry is not supported \n", __func__);
	return 0;
}
static DEVICE_ATTR(booting_dry, 0444, ccic_booting_dry_show, NULL);
#endif

static ssize_t ccic_water_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mu004_usbpd_data *usbpd_data = dev_get_drvdata(dev);

	if(!usbpd_data) {
		pr_err("%s usbpd_data is null!!\n", __func__);
		return -ENODEV;
	}
	pr_info("%s is_water_detect=%d\n", __func__,
		(int)usbpd_data->is_water_detect);

	return sprintf(buf, "%d\n", usbpd_data->is_water_detect);
}
static DEVICE_ATTR(water, 0444, ccic_water_show, NULL);

static ssize_t ccic_acc_device_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mu004_usbpd_data *pdic_data = dev_get_drvdata(dev);
	struct usbpd_data *pd_data;
	struct usbpd_manager_data *manager;

	if (!pdic_data) {
		pr_info("pdic_data is null\n");
		return -ENXIO;
	}
	pd_data = dev_get_drvdata(pdic_data->dev);
	if (!pd_data) {
		pr_info("pd_data is null\n");
		return -ENXIO;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_info("manager is null\n");
		return -ENXIO;
	}

	pr_info("%s - 0x%04x\n", __func__, manager->Device_Version);

	return sprintf(buf, "%04x\n", manager->Device_Version);
}
static DEVICE_ATTR(acc_device_version, 0444, ccic_acc_device_version_show, NULL);

static ssize_t ccic_usbpd_ids_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mu004_usbpd_data *pdic_data = dev_get_drvdata(dev);
	struct usbpd_data *pd_data;
	struct usbpd_manager_data *manager;

	if (!pdic_data) {
		pr_info("pdic_data is null\n");
		return -ENXIO;
	}
	pd_data = dev_get_drvdata(pdic_data->dev);
	if (!pd_data) {
		pr_info("pd_data is null\n");
		return -ENXIO;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_info("manager is null\n");
		return -ENXIO;
	}

	pr_info("%s - %04x:%04x\n", __func__, manager->Vendor_ID, manager->Product_ID);

	return sprintf(buf, "%04x:%04x\n",
				le16_to_cpu(manager->Vendor_ID),
				le16_to_cpu(manager->Product_ID));
}
static DEVICE_ATTR(usbpd_ids, 0444, ccic_usbpd_ids_show, NULL);

static ssize_t ccic_usbpd_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mu004_usbpd_data *pdic_data = dev_get_drvdata(dev);
	struct usbpd_data *pd_data;
	struct usbpd_manager_data *manager;

	if (!pdic_data) {
		pr_info("pdic_data is null\n");
		return -ENXIO;
	}
	pd_data = dev_get_drvdata(pdic_data->dev);
	if (!pd_data) {
		pr_info("pd_data is null\n");
		return -ENXIO;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_info("manager is null\n");
		return -ENXIO;
	}

	pr_info("%s - %d\n", __func__, manager->acc_type);

	return sprintf(buf, "%d\n", manager->acc_type);
}
static DEVICE_ATTR(usbpd_type, 0444, ccic_usbpd_type_show, NULL);

static struct attribute *ccic_attributes[] = {
	&dev_attr_cur_version.attr,
	&dev_attr_src_version.attr,
	&dev_attr_lpm_mode.attr,
	&dev_attr_state.attr,
#if defined(CONFIG_SEC_FACTORY)
	&dev_attr_rid.attr,
	&dev_attr_ccic_control_option.attr,
	&dev_attr_booting_dry.attr,
#endif
	&dev_attr_water.attr,
	&dev_attr_acc_device_version.attr,
	&dev_attr_usbpd_ids.attr,
	&dev_attr_usbpd_type.attr,
	NULL
};

const struct attribute_group ccic_sysfs_group = {
	.attrs = ccic_attributes,
};
