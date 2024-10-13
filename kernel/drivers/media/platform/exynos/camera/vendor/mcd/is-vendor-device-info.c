/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/string.h>

#include "is-vendor-device-info.h"
#include "is-vendor-private.h"
#include "is-sec-define.h"
#include "is-device-sensor-peri.h"
#include "is-sysfs.h"
#include "is-cis.h"

int is_vendor_device_info_get_factory_supported_id(void __user *user_data)
{
	uint32_t i, size = 0;
	unsigned int supported_cam_ids[DEVICE_INFO_SUPPORTED_CAM_LIST_SIZE_MAX];
	struct device_info_data_ctrl supported_list_ctrl;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	if (copy_from_user((void *)&supported_list_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	size = vendor_priv->max_supported_camera;

	for (i = 0; i < size; i++)
		supported_cam_ids[i] = vendor_priv->supported_camera_ids[i];

	supported_list_ctrl.ptr_size = size;

	if (copy_to_user(user_data, &supported_list_ctrl, sizeof(struct  device_info_data_ctrl))) {
		err("%s : failed to copy data to user", __func__);
		return -EINVAL;
	}

	if (copy_to_user(supported_list_ctrl.uint32_ptr, supported_cam_ids, sizeof(uint32_t) * size)) {
		err("%s : failed to copy bpc buffer data to user", __func__);
		return -EINVAL;
	}

	return 0;
}

int is_vendor_device_info_get_rom_data_by_position(void __user *user_data)
{
	uint32_t position;
	int rom_id;
	char *cal_buf;
	struct device_info_data_ctrl rom_data_ctrl;
	struct is_vendor_rom *rom_info;

	if (copy_from_user((void *)&rom_data_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	position = rom_data_ctrl.param1;
	rom_id = is_vendor_get_rom_id_from_position(position);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s : invalid camera position (%d)", __func__, position);
		return -EINVAL;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	rom_data_ctrl.ptr_size = rom_info->rom_size;

	if (copy_to_user(user_data, &rom_data_ctrl, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data to user", __func__);
		return -EINVAL;
	}

	if (copy_to_user(rom_data_ctrl.uint8_ptr, cal_buf, sizeof(uint8_t) * rom_info->rom_size)) {
		err("%s : failed to copy data to user", __func__);
		return -EINVAL;
	}

	return 0;
}

int is_vendor_device_info_set_efs_data(void __user *user_data)
{
	struct is_core *core = NULL;
	struct is_vendor_private *vendor_priv;
	struct device_info_efs_data efs_data;
	struct device_info_data_ctrl efs_data_ctrl;

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;

	if (copy_from_user((void *)&efs_data_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	if (copy_from_user((void *)&efs_data, efs_data_ctrl.void_ptr, sizeof(struct device_info_efs_data))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	vendor_priv->tilt_cal_tele_efs_size = efs_data.tilt_cal_tele_efs_size;
	if (efs_data.tilt_cal_tele_efs_size <= DEVICE_INFO_TILT_CAL_TELE_EFS_SIZE_MAX) {
		if (copy_from_user(vendor_priv->tilt_cal_tele_efs_data, efs_data.tilt_cal_tele_efs_buf,
				sizeof(uint8_t) * efs_data.tilt_cal_tele_efs_size)) {
			err("%s : failed to copy data from user", __func__);
			return -EINVAL;
		}
	} else {
		err("%s : invalid size. Do not copy data from user", __func__);
		return -EINVAL;
	}

#ifdef CAMERA_3RD_OIS
	vendor_priv->tilt_cal_tele2_efs_size = efs_data.tilt_cal_tele2_efs_size;
	if (efs_data.tilt_cal_tele2_efs_size <= DEVICE_INFO_TILT_CAL_TELE_EFS_SIZE_MAX) {
		if (copy_from_user(vendor_priv->tilt_cal_tele2_efs_data, efs_data.tilt_cal_tele2_efs_buf,
				sizeof(uint8_t) * efs_data.tilt_cal_tele2_efs_size)) {
			err("%s : failed to copy data from user", __func__);
			return -EINVAL;
		}
	} else {
		err("%s : invalid size. Do not copy data from user", __func__);
		return -EINVAL;
	}
#endif

	vendor_priv->gyro_efs_size = efs_data.gyro_efs_size;
	if (efs_data.gyro_efs_size <= DEVICE_INFO_GYRO_EFS_SIZE_MAX) {
		if (copy_from_user(vendor_priv->gyro_efs_data, efs_data.gyro_efs_buf,
				sizeof(uint8_t) * efs_data.gyro_efs_size)) {
			err("%s : failed to copy data from user", __func__);
			return -EINVAL;
		}
	} else {
		err("%s : invalid size. Do not copy data from user", __func__);
		return -EINVAL;
	}

	return 0;
}

int is_vendor_device_info_get_sensorid_by_cameraid(void __user *user_data)
{
	uint32_t cameraId;
	struct is_core *core = NULL;
	struct is_vendor_private *vendor_priv;
	struct device_info_data_ctrl sensor_id_ctrl;

	if (copy_from_user((void *)&sensor_id_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	cameraId = sensor_id_ctrl.param1;

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;

	if (cameraId < SENSOR_POSITION_MAX)
		sensor_id_ctrl.int32 = vendor_priv->sensor_id[cameraId];
	else
		sensor_id_ctrl.int32 = -1;

	if (copy_to_user(user_data, &sensor_id_ctrl, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data to user", __func__);
		return -EINVAL;
	}

	return 0;
}

int is_vendor_device_info_get_awb_data_addr(void __user *user_data)
{
	int ret = 0;
	struct is_vendor_rom *rom_info;
	int position;
	int rom_id;
	int rom_cal_index;
	int32_t awb_data[4];
	struct device_info_data_ctrl awb_data_addr_ctrl;

	awb_data[0] = -1; // awb_master_addr
	awb_data[1] = IS_AWB_MASTER_DATA_SIZE;
	awb_data[2] = -1; // awb_module_addr
	awb_data[3] = IS_AWB_MODULE_DATA_SIZE;

	if (copy_from_user((void *)&awb_data_addr_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto exit;
	}

	position = awb_data_addr_ctrl.param1;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: invalid ROM ID [%d][%d]", __func__, position, rom_id);
		goto exit;
	}

	is_sec_get_rom_info(&rom_info, rom_id);

	if (rom_cal_index == 1)
		awb_data[0] = rom_info->rom_sensor2_awb_master_addr;
	else
		awb_data[0] = rom_info->rom_awb_master_addr;

	if (rom_cal_index == 1)
		awb_data[2] = rom_info->rom_sensor2_awb_module_addr;
	else
		awb_data[2] = rom_info->rom_awb_module_addr;

exit:
	if (copy_to_user(awb_data_addr_ctrl.int32_ptr, &awb_data, sizeof(int32_t) * 4)) {
		err("%s : failed to copy data to user", __func__);
		ret = -EINVAL;
	}

	return ret;
}

int is_vendor_device_info_get_sec2lsi_module_info(void __user *user_data)
{
	int ret = 0;
	struct device_info_romdata_sec2lsi romdata_sec2lsi;
	struct device_info_data_ctrl romdata_ctrl;
	struct is_vendor_rom *rom_info;
	char *cal_buf;
	int rom_id;

	if (copy_from_user((void *)&romdata_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s :failed to copy data from user", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (copy_from_user((void *)&romdata_sec2lsi, romdata_ctrl.void_ptr, sizeof(struct device_info_romdata_sec2lsi))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto exit;
	}

	rom_id = is_vendor_get_rom_id_from_position(romdata_sec2lsi.camID);

	if (rom_id == ROM_ID_NOTHING) {
		err("invalid camera position (%d)", romdata_sec2lsi.camID);
		ret = -EINVAL;
		goto exit;
	}

	info("%s : in for ROM[%d]", __func__, rom_id);

	is_sec_get_rom_info(&rom_info, rom_id);

	if (!rom_info) {
		err("There is no cal map (caminfo.camID : %d, rom_id : %d)\n",
			romdata_sec2lsi.camID, rom_id);
		ret = -EINVAL;
		goto exit;
	} else {
		info("rom_header_version_start_addr : %d", rom_info->rom_header_version_start_addr);
	}

	if (!rom_info->use_standard_cal) {
		info("%s : ROM[%d] does not use standard cal", __func__, rom_id);
		ret = -EINVAL;
		goto exit;
	}

	if (rom_info->sec2lsi_conv_done == false) {
		cal_buf = rom_info->buf;

		if (!cal_buf) {
			err("There is no cal buffer allocated (caminfo.camID : %d, rom ID : %d)\n",
				romdata_sec2lsi.camID, rom_id);
			ret = -EINVAL;
			goto exit;
		}

		if (!rom_info->read_done) {
			info("%s : ROM[%d] failed to read cal data, skip sec2lsi", __func__, rom_id);
			ret = -EINVAL;
			goto exit;
		}

		if (rom_info->rom_header_version_start_addr > 0) {
			ret = copy_to_user(romdata_sec2lsi.mdInfo, &cal_buf[rom_info->rom_header_version_start_addr],
				sizeof(uint8_t) * SEC2LSI_MODULE_INFO_SIZE);
		} else {
			info("%s : rom_header_version_start_addr is invalid for rom_id %d",
				__func__, rom_id);
		}

		if (copy_to_user(romdata_ctrl.void_ptr, (void *)&romdata_sec2lsi, sizeof(struct device_info_romdata_sec2lsi))) {
			err("failed to copy data to user");
			ret = -EINVAL;
			goto exit;
		}
		info("%s : mdinfo size : %u",__func__, SEC2LSI_MODULE_INFO_SIZE);
	} else {
		info("%s : already did for ROM_[%d]", __func__, rom_id);
	}

exit:
	return ret;
}

int is_vendor_device_info_get_sec2lsi_buff(void __user *user_data)
{
	int ret = 0;
	struct device_info_romdata_sec2lsi romdata_sec2lsi;
	struct device_info_data_ctrl romdata_ctrl;
	struct is_vendor_rom *rom_info;
	char *cal_buf;
	int rom_id;

	struct is_vendor_standard_cal *standard_cal_data;

	if (copy_from_user((void *)&romdata_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto exit;
	}
	
	if (copy_from_user((void *)&romdata_sec2lsi, romdata_ctrl.void_ptr, sizeof(struct device_info_romdata_sec2lsi))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto exit;
	}

	rom_id = is_vendor_get_rom_id_from_position(romdata_sec2lsi.camID);

	if (rom_id == ROM_ID_NOTHING) {
		err("invalid camera position (%d)", romdata_sec2lsi.camID);
		ret = -EINVAL;
		goto exit;
	}

	info("%s : in for ROM[%d]", __func__, rom_id);

	is_sec_get_rom_info(&rom_info, rom_id);

	if (!rom_info) {
		err("There is no cal map (caminfo.camID : %d, rom_id : %d)\n",
			romdata_sec2lsi.camID, rom_id);
		ret = -EINVAL;
		goto exit;
	} else {
		standard_cal_data = &(rom_info->standard_cal_data);
		info("rom_header_version_start_addr : %d", rom_info->rom_header_version_start_addr);
	}

	if (!rom_info->use_standard_cal) {
		info("%s : ROM[%d] does not use standard cal", __func__, rom_id);
		ret = -EINVAL;
		goto exit;
	}

	if (rom_info->sec2lsi_conv_done == false) {
		cal_buf = rom_info->buf;

		if (!cal_buf) {
			err("There is no cal buffer allocated (caminfo.camID : %d, rom_id : %d)\n",
				romdata_sec2lsi.camID, rom_id);
			ret = -EINVAL;
			goto exit;
		}

		if (!rom_info->read_done) {
			info("%s : ROM[%d] failed to read cal data, skip sec2lsi", __func__, rom_id);
			ret = -EINVAL;
			goto exit;
		}

		if (standard_cal_data->rom_awb_sec2lsi_start_addr >= 0) {
			romdata_sec2lsi.awb_size = standard_cal_data->rom_awb_end_addr
				- standard_cal_data->rom_awb_start_addr + 1;
			romdata_sec2lsi.lsc_size = standard_cal_data->rom_shading_end_addr
				- standard_cal_data->rom_shading_start_addr + 1;
		}

		if (standard_cal_data->rom_awb_start_addr > 0) {
			info("%s : rom[%d] awb_start_addr is 0x%08X size is %d", __func__,
				rom_id, standard_cal_data->rom_awb_start_addr, romdata_sec2lsi.awb_size);
			if (copy_to_user(romdata_sec2lsi.secBuf, &cal_buf[standard_cal_data->rom_awb_start_addr],
				sizeof(uint8_t) * romdata_sec2lsi.awb_size)) {
				err("failed to copy data to user");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			info("%s : sensor %d with rom_id %d does not have awb info",
				__func__, romdata_sec2lsi.camID, rom_id);
		}

		if (standard_cal_data->rom_shading_start_addr > 0) {
			info("%s : rom[%d] shading_start_addr is 0x%08X size is %d", __func__,
				rom_id, standard_cal_data->rom_shading_start_addr, romdata_sec2lsi.lsc_size);
			if (copy_to_user(romdata_sec2lsi.secBuf + romdata_sec2lsi.awb_size,
					&cal_buf[standard_cal_data->rom_shading_start_addr],
					sizeof(uint8_t) * romdata_sec2lsi.lsc_size)) {
				err("failed to copy data to user");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			info("%s : sensor %d with rom_id : %d does not have shading info",
				__func__, romdata_sec2lsi.camID, rom_id);
		}

		if (copy_to_user(romdata_ctrl.void_ptr, (void *)&romdata_sec2lsi, sizeof(struct device_info_romdata_sec2lsi))) {
			err("failed to copy data to user");
			ret = -EINVAL;
			goto exit;
		}
		info("%s : awb size : %u, lsc size : %u", __func__,
			romdata_sec2lsi.awb_size, romdata_sec2lsi.lsc_size);
	} else {
		info("%s : already did for ROM_[%d]", __func__, rom_id);
	}

exit:
	return ret;
}

int is_vendor_device_info_set_sec2lsi_buff(void __user *user_data)
{
	int ret = 0;
	struct device_info_romdata_sec2lsi romdata_sec2lsi;
	struct device_info_data_ctrl romdata_ctrl;
	struct is_vendor_rom *rom_info;
	char *cal_buf;
	char *cal_buf_rom_data;

	u32 factory_data_len;
	int rom_id;

	struct is_vendor_standard_cal *standard_cal_data;

	if (copy_from_user((void *)&romdata_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto exit;
	}
	
	if (copy_from_user((void *)&romdata_sec2lsi, romdata_ctrl.void_ptr, sizeof(struct device_info_romdata_sec2lsi))) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
		goto exit;
	}

	rom_id = is_vendor_get_rom_id_from_position(romdata_sec2lsi.camID);

	if (rom_id == ROM_ID_NOTHING) {
		err("invalid camera position (%d)", romdata_sec2lsi.camID);
		ret = -EINVAL;
		goto exit;
	}

	info("%s : in for ROM[%d]", __func__, rom_id);

	is_sec_get_rom_info(&rom_info, rom_id);

	if (!rom_info) {
		err("There is no cal map (caminfo.camID : %d, rom_id : %d)\n",
			romdata_sec2lsi.camID, rom_id);
		ret = -EINVAL;
		goto exit;
	} else {
		standard_cal_data = &(rom_info->standard_cal_data);
		info("rom_header_version_start_addr : %d", rom_info->rom_header_version_start_addr);
	}

	if (!rom_info->use_standard_cal) {
		info("%s : ROM[%d] does not use standard cal", __func__, rom_id);
		ret = -EINVAL;
		goto exit;
	}

	if (rom_info->sec2lsi_conv_done == false) {
		rom_info->sec2lsi_conv_done = true;
		cal_buf = rom_info->buf;

		if (!cal_buf) {
			err("There is no cal buffer allocated (caminfo.camID : %d, rom_id : %d)\n",
				romdata_sec2lsi.camID, rom_id);
			ret = -EINVAL;
			goto exit;
		}

		if (!rom_info->read_done) {
			info("%s : ROM[%d] failed to read cal data, skip sec2lsi", __func__, rom_id);
			ret = -EINVAL;
			goto exit;
		}

		if (standard_cal_data->rom_awb_sec2lsi_start_addr > 0) {
			info("%s : caminfo.awb_size is %d", __func__, romdata_sec2lsi.awb_size);
			if (copy_from_user(&cal_buf[standard_cal_data->rom_awb_sec2lsi_start_addr], romdata_sec2lsi.lsiBuf,
				sizeof(uint8_t) * (romdata_sec2lsi.awb_size))) {
				err("failed to copy data from user");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			info("%s : sensor %d with rom_id %d does not have awb info",
				__func__, romdata_sec2lsi.camID, rom_id);
		}

		if (standard_cal_data->rom_shading_sec2lsi_start_addr > 0) {
			info("%s : caminfo.lsc_size is %d", __func__, romdata_sec2lsi.lsc_size);
			if (copy_from_user(&cal_buf[standard_cal_data->rom_shading_sec2lsi_start_addr], romdata_sec2lsi.lsiBuf +
				romdata_sec2lsi.awb_size, sizeof(uint8_t) * (romdata_sec2lsi.lsc_size))) {
				err("failed to copy data to user");
				ret = -EINVAL;
				goto exit;
			}
		} else {
			info("%s : sensor %d with rom_id %d does not have shading info",
				__func__, romdata_sec2lsi.camID, rom_id);
		}

		if (standard_cal_data->rom_factory_start_addr > 0) {
			cal_buf_rom_data = rom_info->sec_buf;
			factory_data_len =
				standard_cal_data->rom_factory_end_addr -
				standard_cal_data->rom_factory_start_addr + 1;
			memcpy(&cal_buf[standard_cal_data->rom_factory_sec2lsi_start_addr],
				&cal_buf_rom_data[standard_cal_data->rom_factory_start_addr],
				factory_data_len);
		}

		if (!is_sec_check_awb_lsc_crc32_post_sec2lsi(cal_buf, romdata_sec2lsi.camID))
			err("CRC check post sec2lsi failed!");

	} else {
		info("%s : already did for ROM_[%d]", __func__, rom_id);
	}

exit:
	return ret;
}

int is_vendor_device_info_perform_cal_reload(void __user *user_data)
{
	return is_sec_cal_reload();
}

int is_vendor_device_info_get_ois_hall_data(void __user *user_data)
{
	struct is_core *core = NULL;
	struct is_ois_hall_data halldata;
	struct device_info_data_ctrl ois_hall_data_ctrl;

	core = is_get_is_core();

#if IS_ENABLED(CONFIG_OIS_USE)
	is_ois_get_hall_data(core, &halldata);
#endif

	if (copy_from_user((void *)&ois_hall_data_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	if (copy_to_user(ois_hall_data_ctrl.void_ptr, (void *)&halldata, sizeof(struct is_ois_hall_data))) {
		err("%s : failed to copy data to user", __func__);
		return -EINVAL;
	}

	return 0;
}

#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
int is_vendor_device_info_get_crop_shift_data(void __user *user_data)
{
	int ret = 0;
	int position = 0;
	struct crop_shift_info shift_info = {0, };
	struct device_info_data_ctrl crop_shift_data_ctrl;
	struct is_cis *cis;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;

	if (copy_from_user((void *)&crop_shift_data_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	position = crop_shift_data_ctrl.param1;

	ret = is_vendor_get_module_from_position(position, &module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	if (!sensor_peri->subdev_cis)
		return -EINVAL;

	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	CALL_CISOPS(cis, cis_get_crop_shift_data, sensor_peri->subdev_cis, &shift_info);

	if (copy_to_user(crop_shift_data_ctrl.void_ptr, (void *)&shift_info, sizeof(struct crop_shift_info))) {
		err("%s : failed to copy data to user", __func__);
		return -EINVAL;
	}

	return ret;
}
#endif

int is_vendor_device_info_read_bpc_otp(int position, char *bpc_data, u32 *bpc_size)
{
	int ret = 0;
	struct is_cis *cis;
	struct is_module_enum *module;
	struct is_device_sensor *device;
	struct is_device_sensor_peri *sensor_peri;

	ret = is_vendor_get_module_from_position(position, &module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	if (!sensor_peri->subdev_cis)
		return -EINVAL;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(sensor_peri->subdev_cis);
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	if (!device)
		return -EINVAL;

	if (!device->subdev_module)
		device->subdev_module = module->subdev;

	if (!cis->use_bpc_otp)
		return -EINVAL;

	ret |= is_sensor_gpio_on(device);

	ret |= CALL_CISOPS(cis, cis_read_bpc_otp, sensor_peri->subdev_cis, bpc_data, bpc_size);

	ret |= is_sensor_gpio_off(device);

	return ret;
}

int is_vendor_device_info_get_bpc_otp_data(void __user *user_data)
{
	int ret = 0;
	int sensor_position;
	char *bpc_data;
	u32 bpc_size = 0;
	struct device_info_data_ctrl bpc_otp_ctrl;

	if (copy_from_user((void *)&bpc_otp_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("failed to copy data from user");
		return -EINVAL;
	}

	sensor_position = bpc_otp_ctrl.param1;

	bpc_data = vzalloc(DEVICE_INFO_BPC_OTP_SIZE_MAX * sizeof(char));

	ret = is_vendor_device_info_read_bpc_otp(sensor_position, bpc_data, &bpc_size);
	if (ret < 0)
		err("failed to read bpc otp");

	bpc_otp_ctrl.ptr_size = bpc_size;

	if (copy_to_user(user_data, &bpc_otp_ctrl, sizeof(struct device_info_data_ctrl))) {
		err("failed to copy bpc data to user");
		ret = -EINVAL;
	}

	if (copy_to_user(bpc_otp_ctrl.uint8_ptr, bpc_data, sizeof(uint8_t) * bpc_size)) {
		err("failed to copy bpc buffer data to user");
		ret = -EINVAL;
	}

	vfree(bpc_data);

	return ret;
}

int is_vendor_device_info_get_mipi_phy(void __user *user_data)
{
	int ret = 0;

#ifdef USE_SENSOR_DEBUG
	int i, j;
	int sensor_position;
	int phy_num;
	char str_val[15];
	char *phy_setfile_string;
	struct phy_setfile *phy;
	struct is_cis *cis;
	struct is_device_csi *csi;
	struct is_module_enum *module;
	struct is_device_sensor *sensor;
	struct is_device_sensor_peri *sensor_peri;
	struct device_info_data_ctrl mipi_phy_ctrl;

	if (copy_from_user((void *)&mipi_phy_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	sensor_position = mipi_phy_ctrl.param1;

	if (is_vendor_get_module_from_position(sensor_position, &module) < 0)
		return -EINVAL;

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	if (!sensor_peri->subdev_cis)
		return -EINVAL;

	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(sensor_peri->subdev_cis);
	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	phy_num = csi->phy_sf_tbl->sz_comm + csi->phy_sf_tbl->sz_lane;

	phy_setfile_string = vzalloc(DEVICE_INFO_MIPI_PHY_MAX_SIZE * sizeof(char));

	strcpy(phy_setfile_string, cis->sensor_info->name);

	strcat(phy_setfile_string, " < addr, bit_start, bit_width, val, index, max_lane >\n");
	for (i = 0; i < phy_num; i++) {
		if (i < csi->phy_sf_tbl->sz_comm) {
			phy = &csi->phy_sf_tbl->sf_comm[i];
			sprintf(str_val, "comm %d", i);
		} else {
			phy = &csi->phy_sf_tbl->sf_lane[i - csi->phy_sf_tbl->sz_comm];
			sprintf(str_val, "lane %d", (int)(i - csi->phy_sf_tbl->sz_comm));
		}
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " < %#06x", phy->addr);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %d", phy->start);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %d", phy->width);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %#010x", phy->val);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %d", phy->index);
		strcat(phy_setfile_string, str_val);

		sprintf(str_val, " %d > ", phy->max_lane);
		strcat(phy_setfile_string, str_val);

		for (j = 0; j < MIPI_PHY_REG_MAX_SIZE; j++) {
			if (device_info_mipi_phy_reg_list[j].reg == phy->addr) {
				strcat(phy_setfile_string, device_info_mipi_phy_reg_list[j].reg_name);
				break;
			}
		}

		strcat(str_val, " \n");
		strcat(phy_setfile_string, str_val);
	}

	mipi_phy_ctrl.ptr_size = strlen(phy_setfile_string);

	if (copy_to_user(user_data, &mipi_phy_ctrl, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data to user", __func__);
		ret = -EINVAL;
	}

	if (copy_to_user(mipi_phy_ctrl.uint8_ptr, phy_setfile_string,
			sizeof(char) * strlen(phy_setfile_string))) {
		err("%s : failed to copy data to user", __func__);
		ret  = -EINVAL;
	}

	vfree(phy_setfile_string);
#endif

	return ret;
}

int is_vendor_device_info_set_mipi_phy(void __user *user_data)
{
	int ret = 0;

#ifdef USE_SENSOR_DEBUG
	int i;
	int sensor_position;
	int phy_num, phy_value, phy_index;
	char *token, *token_sub, *token_tmp;
	char *phy_setfile_string;
	struct phy_setfile *phy;
	struct is_cis *cis;
	struct is_device_csi *csi;
	struct is_module_enum *module;
	struct is_device_sensor *sensor;
	struct is_device_sensor_peri *sensor_peri;
	struct device_info_data_ctrl mipi_phy_ctrl;


	if (copy_from_user((void *)&mipi_phy_ctrl, user_data, sizeof(struct device_info_data_ctrl))) {
		err("%s : failed to copy data from user", __func__);
		return -EINVAL;
	}

	sensor_position = mipi_phy_ctrl.param1;

	if (is_vendor_get_module_from_position(sensor_position, &module) < 0)
		return -EINVAL;

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	if (!sensor_peri->subdev_cis)
		return -EINVAL;

	sensor = (struct is_device_sensor *)v4l2_get_subdev_hostdata(sensor_peri->subdev_cis);
	csi = v4l2_get_subdevdata(sensor->subdev_csi);
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	phy_setfile_string = vzalloc(DEVICE_INFO_MIPI_PHY_MAX_SIZE * sizeof(char));

	if (copy_from_user((void *)phy_setfile_string, mipi_phy_ctrl.uint8_ptr,
			sizeof(uint8_t) * mipi_phy_ctrl.ptr_size)) {
		err("%s : failed to copy data from user", __func__);
		ret = -EINVAL;
	}

	phy_num = csi->phy_sf_tbl->sz_comm + csi->phy_sf_tbl->sz_lane;

	info("[%s] set mipi phy tune : sensor(%d:%s)", __func__, sensor_position, cis->sensor_info->name);

	token = phy_setfile_string;

	for (i = 0; i < phy_num; i++) {
		if (strstr(token, "comm ") != 0) {
			token_tmp = strstr(token, "comm ");
			token = token_tmp + strlen("comm ");
			token_sub = strsep(&token, " ");
			ret = kstrtoint(token_sub, 10, &phy_index);
			phy = &csi->phy_sf_tbl->sf_comm[phy_index];
		} else if (strstr(token, "lane ") != 0) {
			token_tmp = strstr(token, "lane ");
			token = token_tmp + strlen("lane ");
			token_sub = strsep(&token, " ");
			ret = kstrtoint(token_sub, 10, &phy_index);
			phy = &csi->phy_sf_tbl->sf_lane[phy_index];
		} else {
			break;
		}

		token_sub = strsep(&token, " "); // ignore "<"

		token_sub = strsep(&token, " ");
		ret = sscanf(token_sub, "%x", &phy_value);
		phy->addr = phy_value;

		token_sub = strsep(&token, " ");
		ret = kstrtoint(token_sub, 10, &phy_value);
		phy->start = phy_value;

		token_sub = strsep(&token, " ");
		ret = kstrtoint(token_sub, 10, &phy_value);
		phy->width = phy_value;

		token_sub = strsep(&token, " ");
		ret = sscanf(token_sub, "%x", &phy_value);
		phy->val = phy_value;

		token_sub = strsep(&token, " ");
		ret = kstrtoint(token_sub, 10, &phy_value);
		phy->index = phy_value;

		token_sub = strsep(&token, " ");
		ret = kstrtoint(token_sub, 10, &phy_value);
		phy->max_lane = phy_value;

		token_sub = strsep(&token, " "); // ignore ">"

		token_sub = strsep(&token, " "); // reg name

		info("[%s] set mipi phy tune : %#06x %d %d %#010x %d %d, %s",
			__func__, phy->addr, phy->start, phy->width, phy->val,
			phy->index, phy->max_lane, token_sub);
	}

	vfree(phy_setfile_string);
#endif

	return ret;
}

