/*
 *
 * Copyright (C) 2017-2019 Samsung Electronics
 *
 * Author:Wookwang Lee. <wookwang.lee@samsung.com>,
 * Author:Guneet Singh Khurana  <gs.khurana@samsung.com>,
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/usb/typec/common/pdic_core.h>
#include <linux/usb/typec/common/pdic_sysfs.h>

static ssize_t pdic_sysfs_show_property(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t pdic_sysfs_store_property(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define PDIC_SYSFS_ATTR(_name)				\
{							\
	.attr = { .name = #_name },			\
	.show = pdic_sysfs_show_property,		\
	.store = pdic_sysfs_store_property,		\
}

static struct device_attribute pdic_attributes[] = {
	PDIC_SYSFS_ATTR(chip_name),
	PDIC_SYSFS_ATTR(cur_version),
	PDIC_SYSFS_ATTR(src_version),
	PDIC_SYSFS_ATTR(lpm_mode),
	PDIC_SYSFS_ATTR(state),
	PDIC_SYSFS_ATTR(rid),
	PDIC_SYSFS_ATTR(ccic_control_option),
	PDIC_SYSFS_ATTR(booting_dry),
	PDIC_SYSFS_ATTR(fw_update),
	PDIC_SYSFS_ATTR(fw_update_status),
	PDIC_SYSFS_ATTR(water),
	PDIC_SYSFS_ATTR(dex_fan_uvdm),
	PDIC_SYSFS_ATTR(acc_device_version),
	PDIC_SYSFS_ATTR(debug_opcode),
	PDIC_SYSFS_ATTR(control_gpio),
	PDIC_SYSFS_ATTR(usbpd_ids),
	PDIC_SYSFS_ATTR(usbpd_type),
	PDIC_SYSFS_ATTR(cc_pin_status),
	PDIC_SYSFS_ATTR(ram_test),
	PDIC_SYSFS_ATTR(sbu_adc),
	PDIC_SYSFS_ATTR(vsafe0v_status),
	PDIC_SYSFS_ATTR(ovp_ic_shutdown),
	PDIC_SYSFS_ATTR(hmd_power),
	PDIC_SYSFS_ATTR(water_threshold),
	PDIC_SYSFS_ATTR(water_check),
	PDIC_SYSFS_ATTR(15mode_watertest_type),
	PDIC_SYSFS_ATTR(vbus_adc),
};

static ssize_t pdic_sysfs_show_property(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	ppdic_data_t ppdic_data = dev_get_drvdata(dev);
	ppdic_sysfs_property_t ppdic_sysfs =
			(ppdic_sysfs_property_t)ppdic_data->pdic_sysfs_prop;
	const ptrdiff_t off = attr - pdic_attributes;

	if (off == PDIC_SYSFS_PROP_CHIP_NAME) {
		return snprintf(buf, PAGE_SIZE, "%s\n",
					ppdic_data->name);
	} else {
		ret = ppdic_sysfs->get_property(ppdic_data, off, buf);
		if (ret < 0) {
			if (ret == -ENODATA)
				dev_info(dev,
					"driver has no data for `%s' property\n",
					attr->attr.name);
			else if (ret != -ENODEV)
				dev_err(dev,
					"driver failed to report `%s' property: %zd\n",
					attr->attr.name, ret);
			return ret;
		}
		return ret;
	}
}

static ssize_t pdic_sysfs_store_property(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	ssize_t ret = 0;
	ppdic_data_t ppdic_data = dev_get_drvdata(dev);
	ppdic_sysfs_property_t ppdic_sysfs =
			(ppdic_sysfs_property_t)ppdic_data->pdic_sysfs_prop;
	const ptrdiff_t off = attr - pdic_attributes;

	ret = ppdic_sysfs->set_property(ppdic_data, off, buf, count);
	if (ret < 0) {
		if (ret == -ENODATA)
			dev_info(dev,
				"driver cannot set  data for `%s' property\n",
				attr->attr.name);
		else if (ret != -ENODEV)
			dev_err(dev,
				"driver failed to set `%s' property: %zd\n",
				attr->attr.name, ret);
		return ret;
	}
	return ret;
}

static umode_t pdic_sysfs_attr_is_visible(struct kobject *kobj,
					struct attribute *attr, int attrno)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	ppdic_data_t ppdic_data = dev_get_drvdata(dev);
	ppdic_sysfs_property_t ppdic_sysfs =
			(ppdic_sysfs_property_t)ppdic_data->pdic_sysfs_prop;
	umode_t mode = 0444;
	int i;

	for (i = 0; i < (int)ppdic_sysfs->num_properties; i++) {
		int property = ppdic_sysfs->properties[i];

		if (property == attrno) {
			if (ppdic_sysfs->property_is_writeable &&
					ppdic_sysfs->property_is_writeable(
					ppdic_data, property) > 0)
				mode |= 0200;
			if (ppdic_sysfs->property_is_writeonly &&
			    ppdic_sysfs->property_is_writeonly(ppdic_data, property)
			    > 0)
				mode = 0200;
			return mode;
		}
	}

	return 0;
}

static struct attribute *__pdic_sysfs_attrs[ARRAY_SIZE(pdic_attributes) + 1];

const struct attribute_group pdic_sysfs_group = {
	.attrs = __pdic_sysfs_attrs,
	.is_visible = pdic_sysfs_attr_is_visible,
};

void pdic_sysfs_init_attrs(void)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(pdic_attributes); i++)
		__pdic_sysfs_attrs[i] = &pdic_attributes[i].attr;
}
