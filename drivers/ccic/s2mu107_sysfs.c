/*
 * s2mu107_sysfs.c
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
#include <linux/ccic/s2mu107_sysfs.h>

#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
#include <linux/ccic/usbpd.h>
#endif

static ssize_t ccic_show_manual_lpm_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);

	if (!pd_data) {
		pr_err("%s pd_data is null!!\n", __func__);
		return -ENODEV;
	}

	return sprintf(buf, "%d\n", pd_data->phy_ops.get_lpm_mode(pd_data));
#endif
	return -ENODEV;
}
static ssize_t ccic_store_manual_lpm_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	int mode;

	if (!pd_data) {
		pr_err("%s pd_data is null!!\n", __func__);
		return -ENODEV;
	}

	sscanf(buf, "%d", &mode);
	pr_info("usb: %s mode=%d\n", __func__, mode);

#ifdef CONFIG_SEC_FACTORY
	if (mode != 1 && mode != 2)
		pd_data->phy_ops.set_normal_mode(pd_data);
#else
	if (mode == 1 || mode == 2)
		pd_data->phy_ops.set_lpm_mode(pd_data);
	else
		pd_data->phy_ops.set_normal_mode(pd_data);
#endif

	return size;
#endif
	return -ENODEV;
}

static DEVICE_ATTR(lpm_mode, 0664,
		ccic_show_manual_lpm_mode, ccic_store_manual_lpm_mode);

static ssize_t ccic_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);

		if (!pd_data) {
			pr_err("%s pd_data is null!!\n", __func__);
			return -ENODEV;
		}

		return sprintf(buf, "%d\n", pd_data->policy.plug_valid);
#endif
	return -ENODEV;
}
static DEVICE_ATTR(state, 0444, ccic_state_show, NULL);

#if defined(CONFIG_SEC_FACTORY)
static ssize_t ccic_rid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	int rid;

	if (!pd_data) {
		pr_err("%s pd_data is null!!\n", __func__);
		return -ENODEV;
	}

	rid = pd_data->phy_ops.get_rid(pd_data);

	return sprintf(buf, "%d\n", rid == REG_RID_MAX ? REG_RID_OPEN : rid);
#endif
	return -ENODEV;
}
static DEVICE_ATTR(rid, 0444, ccic_rid_show, NULL);

static ssize_t ccic_store_control_option_command(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	int cmd;

	if (!pd_data) {
		pr_err("%s pd_data is null!!\n", __func__);
		return -ENODEV;
	}

	sscanf(buf, "%d", &cmd);
	pr_info("usb: %s mode=%d\n", __func__, cmd);

	pd_data->phy_ops.control_option_command(pd_data, cmd);

	return size;
#endif
	return -ENODEV;
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

static ssize_t ccic_acc_device_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	if (!pd_data) {
		pr_err("%s pd_data is null!!\n", __func__);
		return -ENODEV;
	}

	if (!manager) {
		pr_err("%s manager_data is null!!\n", __func__);
		return -ENODEV;
	}
	pr_info("%s 0x%04x\n", __func__, manager->Device_Version);

	return sprintf(buf, "%04x\n", manager->Device_Version);
#endif
	return -ENODEV;
}
static DEVICE_ATTR(acc_device_version, 0444, ccic_acc_device_version_show,NULL);


#if defined(CONFIG_SEC_FACTORY)
#if defined(CONFIG_CCIC_S2MU107)
static ssize_t ccic_power_off_water_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	int ret = 0;

	if (!pd_data) {
		pr_err("%s pd_data is null!!\n", __func__);
		return -ENODEV;
	}

	ret = pd_data->phy_ops.power_off_water_check(pd_data);

	return sprintf(buf, "%d\n", ret);
}

static DEVICE_ATTR(water_check, 0444, ccic_power_off_water_check_show, NULL);
#endif
#endif /* CONFIG_SEC_FACTORY */

static ssize_t ccic_usbpd_ids_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;
	int retval = 0;

	if (!pd_data) {
		pr_err("%s pd_data is null!!\n", __func__);
		return -ENODEV;
	}

	if (!manager) {
		pr_err("%s manager_data is null!!\n", __func__);
		return -ENODEV;
	}

	retval = sprintf(buf, "%04x:%04x\n",
			le16_to_cpu(manager->Vendor_ID),
			le16_to_cpu(manager->Product_ID));
	pr_info("usb: %s : %s",
			__func__, buf);

	return retval;
#endif
	return -ENODEV;
}
static DEVICE_ATTR(usbpd_ids, 0444, ccic_usbpd_ids_show, NULL);

static ssize_t ccic_usbpd_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;
	int retval = 0;

	if (!pd_data) {
		pr_err("%s pd_data is null!!\n", __func__);
		return -ENODEV;
	}

	if (!manager) {
		pr_err("%s manager_data is null!!\n", __func__);
		return -ENODEV;
	}
	retval = sprintf(buf, "%d\n", manager->acc_type);
	pr_info("usb: %s : %d",
			__func__, manager->acc_type);
	return retval;
#endif
	return -ENODEV;
}
static DEVICE_ATTR(usbpd_type, 0444, ccic_usbpd_type_show, NULL);

static ssize_t ccic_water_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_CCIC_S2MU106) || defined(CONFIG_CCIC_S2MU107)
	struct usbpd_data *pd_data = dev_get_drvdata(dev);

	if (!pd_data) {
		pr_err("%s pd_data is null!!\n", __func__);
		return -ENODEV;
	}
	pr_info("%s is_water_detect=%d\n", __func__,
		pd_data->phy_ops.get_water_detect(pd_data));

	return sprintf(buf, "%d\n", pd_data->phy_ops.get_water_detect(pd_data));
#endif
	return -ENODEV;
}
static DEVICE_ATTR(water, 0444, ccic_water_show, NULL);

static struct attribute *ccic_attributes[] = {
	&dev_attr_lpm_mode.attr,
	&dev_attr_state.attr,
#if defined(CONFIG_SEC_FACTORY)
	&dev_attr_rid.attr,
	&dev_attr_ccic_control_option.attr,
	&dev_attr_booting_dry.attr,
#endif
#if (defined(CONFIG_CCIC_S2MU107) || defined(CONFIG_CCIC_S2MU205)) && defined(CONFIG_SEC_FACTORY)
	&dev_attr_water_check.attr,
#endif
	&dev_attr_acc_device_version.attr,
	&dev_attr_usbpd_ids.attr,
	&dev_attr_usbpd_type.attr,
	&dev_attr_water.attr,
#ifdef CONFIG_CCIC_S2MU205
	&dev_attr_chip_name.attr,
#endif
	NULL
};

const struct attribute_group s2mu107_ccic_sysfs_group = {
	.attrs = ccic_attributes,
};

