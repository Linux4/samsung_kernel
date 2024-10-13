/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>

#include "is-sysfs.h"
#include "is-core.h"
#include "is-sec-define.h"
#include "is-device-sensor-peri.h"
#include "is-vendor-private.h"
#include "is-device-rom.h"
#include "is-sysfs-front.h"
#include "is-sysfs-hwparam.h"

struct device *camera_front_dev;

static ssize_t front_afcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_afcal_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(front_afcal);

static ssize_t front_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_FRONT, false);
}
static DEVICE_ATTR_RO(front_camfw);

static ssize_t front_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_FRONT, true);
}
static DEVICE_ATTR_RO(front_camfw_full);

static ssize_t front_caminfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_info_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(front_caminfo);

static ssize_t front_camtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char sensor_maker[50];
	char sensor_name[50];
	int ret;

	ret = is_get_sensor_data(sensor_maker, sensor_name, SENSOR_POSITION_FRONT);

	if (ret < 0)
		return sprintf(buf, "UNKNOWN_UNKNOWN_EXYNOS_IS\n");
	else
		return sprintf(buf, "%s_%s_EXYNOS_IS\n", sensor_maker, sensor_name);
}
static DEVICE_ATTR_RO(front_camtype);

static ssize_t front_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(front_checkfw_factory);

static ssize_t front_dualcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* TODO */
	return 0;
}
static DEVICE_ATTR_RO(front_dualcal);

static ssize_t front_eeprom_retry_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_eeprom_retry_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(front_eeprom_retry);

static ssize_t front_mipi_clock_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	char mipi_string[20] = {0, };
	int i;
	struct device *is_dev;

	is_dev = is_get_is_dev();
	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	core = is_get_is_core();
	if (!core) {
		dev_err(dev, "%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		is_search_sensor_module_with_position(&core->sensor[i],
				SENSOR_POSITION_FRONT, &module);
		if (module)
			break;
	}

	is_vendor_get_mipi_clock_string(device, mipi_string);

	return sprintf(buf, "%s\n", mipi_string);
}
static DEVICE_ATTR_RO(front_mipi_clock);

ssize_t front_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(front_moduleid);

static ssize_t front_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(front_mtf_exif);

static ssize_t front_paf_cal_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_paf_cal_check_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(front_paf_cal_check);

static ssize_t front_phy_tune_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_phy_tune_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(front_phy_tune);

static ssize_t front_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(front_sensorid_exif);

static ssize_t front2_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_FRONT2, false);
}
static DEVICE_ATTR_RO(front2_camfw);

static ssize_t front2_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_FRONT2, true);
}
static DEVICE_ATTR_RO(front2_camfw_full);

static ssize_t front2_caminfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_info_show(buf, CAM_INFO_FRONT2);
}
static DEVICE_ATTR_RO(front2_caminfo);

static ssize_t front2_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(buf, CAM_INFO_FRONT2);
}
static DEVICE_ATTR_RO(front2_checkfw_factory);

static ssize_t front2_dualcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* TODO */
	return 0;
}
static DEVICE_ATTR_RO(front2_dualcal);

ssize_t front2_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_FRONT2);
}
static DEVICE_ATTR_RO(front2_moduleid);

static ssize_t front2_mtf_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(buf, CAM_INFO_FRONT2);
}
static DEVICE_ATTR_RO(front2_mtf_exif);

static ssize_t front2_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_FRONT2);
}
static DEVICE_ATTR_RO(front2_sensorid_exif);

static ssize_t front2_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_tilt_show(buf, CAM_INFO_FRONT2);
}
static DEVICE_ATTR_RO(front2_tilt);

static ssize_t front_tof_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_FRONT_TOF, false);
}
static DEVICE_ATTR_RO(front_tof_camfw);

static ssize_t front_tof_camfw_full_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(buf, CAM_INFO_FRONT_TOF, true);
}
static DEVICE_ATTR_RO(front_tof_camfw_full);

static ssize_t front_tof_caminfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_info_show(buf, CAM_INFO_FRONT_TOF);
}
static DEVICE_ATTR_RO(front_tof_caminfo);

static ssize_t front_tof_check_pd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u16 value;
	struct device *is_dev;

	is_dev = is_get_is_dev();
	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	if (camera_tof_get_laser_photo_diode(SENSOR_POSITION_FRONT_TOF, &value) < 0)
		return sprintf(buf, "NG\n");

	return sprintf(buf, "%d\n", value);
}
static ssize_t front_tof_check_pd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret_count;
	u32 value;
	struct device *is_dev;

	is_dev = is_get_is_dev();
	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	ret_count = kstrtou32(buf, 0, &value);

	if (ret_count != 1)
		return -EINVAL;

	camera_tof_set_laser_current(SENSOR_POSITION_FRONT_TOF, value);

	return count;
}
static DEVICE_ATTR_RW(front_tof_check_pd);

static ssize_t front_tof_checkfw_factory_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(buf, CAM_INFO_FRONT_TOF);
}
static DEVICE_ATTR_RO(front_tof_checkfw_factory);

static ssize_t front_tof_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 value = 0;
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_cis *cis = NULL;
	int i;
	struct device *is_dev;

	is_dev = is_get_is_dev();
	if (!is_dev) {
		dev_err(dev, "%s: is_dev is not yet probed", __func__);
		return -ENODEV;
	}

	core = is_get_is_core();
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		is_search_sensor_module_with_position(&core->sensor[i],
				SENSOR_POSITION_FRONT_TOF, &module);
		if (module)
			break;
	}

	if (!module) {
		err("Could not find sensor id.");
		return -EINVAL;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	WARN_ON(!sensor_peri);

	if (sensor_peri->subdev_cis) {
		cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);
		CALL_CISOPS(cis, cis_get_tof_tx_freq, sensor_peri->subdev_cis, &value);
	}

	return sprintf(buf, "%d\n", value);
}
static DEVICE_ATTR_RO(front_tof_freq);

ssize_t front_tof_moduleid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_FRONT_TOF);
}
static DEVICE_ATTR_RO(front_tof_moduleid);

static ssize_t front_tof_sensorid_exif_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_FRONT_TOF);
}
static DEVICE_ATTR_RO(front_tof_sensorid_exif);

static ssize_t front_tof_tilt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_tilt_show(buf, CAM_INFO_FRONT_TOF);
}
static DEVICE_ATTR_RO(front_tof_tilt);

static ssize_t front_tofcal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	s32 cal_size;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT_TOF);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	cal_size = *((s32 *)&cal_buf[rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len-1]]) +
		(rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len - 1]
		- rom_info->rom_tof_cal_start_addr + 4);

	if (cal_size > IS_TOF_CAL_SIZE_ONCE)
		cal_size = IS_TOF_CAL_SIZE_ONCE;

	memcpy(buf, &cal_buf[rom_info->rom_tof_cal_start_addr], cal_size);
	return cal_size;

err:
	return 0;
}
static DEVICE_ATTR_RO(front_tofcal);

static ssize_t front_tofcal_extra_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	s32 cal_size;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT_TOF);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	cal_size = *((s32 *)&cal_buf[rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len-1]]) +
		(rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len - 1]
		- rom_info->rom_tof_cal_start_addr + 4);

	cal_size -= IS_TOF_CAL_SIZE_ONCE;

	if (cal_size > IS_TOF_CAL_SIZE_ONCE)
		cal_size = IS_TOF_CAL_SIZE_ONCE;

	memcpy(buf, &cal_buf[rom_info->rom_tof_cal_start_addr + IS_TOF_CAL_SIZE_ONCE], cal_size);
	return cal_size;

err:
	return 0;
}
static DEVICE_ATTR_RO(front_tofcal_extra);

static ssize_t front_tofcal_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	s32 cal_size;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT_TOF);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	cal_size = *((s32 *)&cal_buf[rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len-1]]) +
		(rom_info->rom_tof_cal_size_addr[rom_info->rom_tof_cal_size_addr_len - 1]
		- rom_info->rom_tof_cal_start_addr + 4);

	return sprintf(buf, "%d\n", cal_size);

err:
	return sprintf(buf, "0");
}
static DEVICE_ATTR_RO(front_tofcal_size);

static ssize_t front_tofcal_uid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_core *core = NULL;
	struct is_vendor_private *vendor_priv;

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;
	dev_info(dev, "%s: E", __func__);

	return sprintf(buf, "%d\n", vendor_priv->front_tof_mode_id);
}
static DEVICE_ATTR_RO(front_tofcal_uid);

static ssize_t front_tof_dual_cal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	s32 cal_size;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	cal_size = rom_info->rom_dualcal_slave0_size;

	memcpy(buf, &cal_buf[rom_info->rom_dualcal_slave0_start_addr], cal_size);
	return cal_size;

err:
	return 0;
}
static DEVICE_ATTR_RO(front_tof_dual_cal);

static ssize_t front_tof_cal_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;

	is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT_TOF);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	if (cal_buf[rom_info->rom_tof_cal_result_addr] == IS_TOF_CAL_CAL_RESULT_OK         /* CAT TREE */
		&& cal_buf[rom_info->rom_tof_cal_result_addr + 2] == IS_TOF_CAL_CAL_RESULT_OK  /* TRUN TABLE */
		&& cal_buf[rom_info->rom_tof_cal_result_addr + 4] == IS_TOF_CAL_CAL_RESULT_OK) /* VALIDATION */ {
		return sprintf(buf, "OK\n");
	} else {
		return sprintf(buf, "NG\n");
	}

err:
	return sprintf(buf, "NG\n");
}
static DEVICE_ATTR_RO(front_tof_cal_result);

static struct attribute *is_sysfs_attr_entries_front[] = {
	&dev_attr_front_camfw.attr,
	&dev_attr_front_camfw_full.attr,
	&dev_attr_front_caminfo.attr,
	&dev_attr_front_camtype.attr,
	&dev_attr_front_checkfw_factory.attr,
	&dev_attr_front_mipi_clock.attr,
	&dev_attr_front_moduleid.attr,
	&dev_attr_front_mtf_exif.attr,
	&dev_attr_front_phy_tune.attr,
	&dev_attr_front_sensorid_exif.attr,
	NULL,
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct attribute **pablo_vendor_sysfs_get_front_attr_tbl(void)
{
	return is_sysfs_attr_entries_front;
}
KUNIT_EXPORT_SYMBOL(pablo_vendor_sysfs_get_front_attr_tbl);
#endif

static const struct attribute_group is_sysfs_attr_group_front = {
	.attrs	= is_sysfs_attr_entries_front,
};

static struct attribute *camera_front2_attrs[] = {
	&dev_attr_front2_camfw.attr,
	&dev_attr_front2_camfw_full.attr,
	&dev_attr_front2_caminfo.attr,
	&dev_attr_front2_checkfw_factory.attr,
	&dev_attr_front2_mtf_exif.attr,
	&dev_attr_front2_sensorid_exif.attr,
	NULL,
};

static const struct attribute_group is_sysfs_attr_group_front2 = {
	.attrs = camera_front2_attrs,
};

static struct attribute *camera_front_tof_attrs[] = {
	&dev_attr_front_tof_camfw.attr,
	&dev_attr_front_tof_camfw_full.attr,
	&dev_attr_front_tof_caminfo.attr,
	&dev_attr_front_tof_check_pd.attr,
	&dev_attr_front_tof_checkfw_factory.attr,
	&dev_attr_front_tof_sensorid_exif.attr,
	NULL,
};

static const struct attribute_group is_sysfs_attr_group_front_tof = {
	.attrs	= camera_front_tof_attrs,
};

int is_create_front_sysfs(struct class *camera_class)
{
	int ret = 0;
	struct is_sysfs_config *sysfs_config = is_vendor_get_sysfs_config();

	camera_front_dev = device_create(camera_class, NULL, 1, NULL, "front");
	if (IS_ERR(camera_front_dev)) {
		pr_err("failed to create front device!\n");
		return ret;
	}

	ret = devm_device_add_group(camera_front_dev, &is_sysfs_attr_group_front);
	if (ret < 0) {
		pr_err("failed to add device group %s\n", is_sysfs_attr_group_front.name);
		return ret;
	}

	ret = device_create_file(camera_front_dev, &dev_attr_front_eeprom_retry);
	if (ret)
		pr_err("failed to create device file %s\n", dev_attr_front_eeprom_retry.attr.name);

	if (sysfs_config->front_dualcal) {
		ret = device_create_file(camera_front_dev, &dev_attr_front_dualcal);
		if (ret)
			pr_err("failed to create device file %s\n", dev_attr_front_dualcal.attr.name);
	}

	if (sysfs_config->front_fixed_focus == false) {
		ret = device_create_file(camera_front_dev, &dev_attr_front_afcal);
		if (ret)
			pr_err("failed to create device file %s\n", dev_attr_front_afcal.attr.name);
	}

	if (sysfs_config->front_fixed_focus == false && sysfs_config->front_afcal) {
		ret = device_create_file(camera_front_dev, &dev_attr_front_paf_cal_check);
		if (ret)
			pr_err("failed to create device file %s\n", dev_attr_front_paf_cal_check.attr.name);
	}

	if (sysfs_config->front2) {
		ret = devm_device_add_group(camera_front_dev, &is_sysfs_attr_group_front2);
		if (ret < 0) {
			pr_err("failed to add device group %s\n", is_sysfs_attr_group_front2.name);
			return ret;
		}

		if (sysfs_config->front_dualcal) {
			ret = device_create_file(camera_front_dev, &dev_attr_front2_dualcal);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front2_dualcal.attr.name);
		}

		if (sysfs_config->front2_moduleid) {
			ret = device_create_file(camera_front_dev, &dev_attr_front2_moduleid);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front2_moduleid.attr.name);
		}

		if (sysfs_config->front2_tilt) {
			ret = device_create_file(camera_front_dev, &dev_attr_front2_tilt);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front2_tilt.attr.name);
		}
	}

	if (sysfs_config->front_tof) {
		ret = devm_device_add_group(camera_front_dev, &is_sysfs_attr_group_front_tof);
		if (ret < 0) {
			pr_err("failed to add device group %s\n", is_sysfs_attr_group_front_tof.name);
			return ret;
		}

		if (sysfs_config->front_tof_moduleid) {
			ret = device_create_file(camera_front_dev, &dev_attr_front_tof_moduleid);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front_tof_moduleid.attr.name);
		}

		if (sysfs_config->front_tof_tilt) {
			ret = device_create_file(camera_front_dev, &dev_attr_front_tof_tilt);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front_tof_tilt.attr.name);
		}

		if (sysfs_config->front_tof_tx_freq_variation) {
			ret = device_create_file(camera_front_dev, &dev_attr_front_tof_freq);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front_tof_freq.attr.name);
		}

		if (sysfs_config->front_tof_cal) {
			ret = device_create_file(camera_front_dev, &dev_attr_front_tofcal);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front_tofcal.attr.name);

			ret = device_create_file(camera_front_dev, &dev_attr_front_tofcal_extra);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front_tofcal_extra.attr.name);

			ret = device_create_file(camera_front_dev, &dev_attr_front_tofcal_size);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front_tofcal_size.attr.name);

			ret = device_create_file(camera_front_dev, &dev_attr_front_tofcal_uid);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front_tofcal_uid.attr.name);

			ret = device_create_file(camera_front_dev, &dev_attr_front_tof_dual_cal);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front_tof_dual_cal.attr.name);

			ret = device_create_file(camera_front_dev, &dev_attr_front_tof_cal_result);
			if (ret)
				pr_err("failed to create device file %s\n", dev_attr_front_tof_cal_result.attr.name);
		}
	}

#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
	ret = is_add_front_hwparam_sysfs(camera_front_dev);
#endif

	return ret;
}

void is_destroy_front_sysfs(struct class *camera_class)
{
	struct is_sysfs_config *sysfs_config = is_vendor_get_sysfs_config();

	devm_device_remove_group(camera_front_dev, &is_sysfs_attr_group_front);
	if (sysfs_config->front2)
		devm_device_remove_group(camera_front_dev, &is_sysfs_attr_group_front2);
	if (sysfs_config->front_tof)
		devm_device_remove_group(camera_front_dev, &is_sysfs_attr_group_front_tof);

	if (camera_class && camera_front_dev)
		device_destroy(camera_class, camera_front_dev->devt);
}
