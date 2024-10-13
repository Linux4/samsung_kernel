/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm_qos.h>
#include <linux/device.h>

#include "is-sysfs.h"
#include "is-core.h"
#include "is-sec-define.h"
#include "is-device-sensor-peri.h"
#include "is-vendor-private.h"
#include "is-interface-library.h"
#include "is-sysfs-rear.h"
#include "is-sysfs-front.h"
#if defined(CONFIG_OIS_USE)
#include "is-sysfs-ois.h"
#endif
#include "is-sysfs-svc.h"
#include "pablo-phy-tune.h"

struct class *camera_class = NULL;
EXPORT_SYMBOL_GPL(camera_class);

struct is_sysfs_config *is_vendor_get_sysfs_config(void)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	return &vendor_priv->sysfs_config;
}

int is_get_cam_info_from_index(struct is_cam_info **caminfo, enum is_cam_info_index cam_index)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	*caminfo = &(vendor_priv->cam_infos[cam_index]);
	return 0;
}

int is_get_sensor_data(char *maker, char *name, int position)
{
	struct is_module_enum *module;
	int ret = 0;

	ret = is_vendor_get_module_from_position(position, &module);

	if (module && !ret) {
		if (maker != NULL)
			sprintf(maker, "%s", module->sensor_maker ?
					module->sensor_maker : "UNKNOWN");
		if (name != NULL)
			sprintf(name, "%s", module->sensor_name ?
					module->sensor_name : "UNKNOWN");
		return 0;
	}

	err("%s: there's no matched sensor id", __func__);

	return -ENODEV;
}

ssize_t camera_af_hall_show(char *buf, enum is_cam_info_index cam_index)
{
	int ret;
	int position;
	struct is_cam_info *cam_info;
	struct is_module_enum *module;
	struct is_device_sensor *device;
	struct is_actuator *actuator;
	int sensor_id;
	u16 hall_value = 0;

	is_get_cam_info_from_index(&cam_info, cam_index);
	position = cam_info->position;

	is_vendor_get_module_from_position(position, &module);

	WARN_ON(!module);
	sensor_id = module->pdata->id;

	WARN_ON(!module->subdev);
	device = v4l2_get_subdev_hostdata(module->subdev);

	if (!test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		err("%s: Camera is not started yet : %d", __func__, sensor_id);
		goto p_err;
	}

	actuator = device->actuator[sensor_id];
	WARN_ON(!actuator);

	ret = CALL_ACTUATOROPS(actuator, get_hall_value, actuator->subdev, &hall_value);
	if (ret) {
		err("[%d] actuator read af hall fail\n", sensor_id);
		goto p_err;
	}

	return sprintf(buf, "%d\n", hall_value);

p_err:
	return sprintf(buf, "NG\n");
}

ssize_t camera_afcal_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;

	int position;
	int rom_id;
	int rom_cal_index;
	char *cal_buf;
	bool is_front = false;
	int i;
	char tempbuf[15 * AF_CAL_MAX] = {0, };
	char tmpChar[15] = {0, };

	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err_afcal;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	if (position == SENSOR_POSITION_FRONT ||
		position == SENSOR_POSITION_FRONT2 ||
		position == SENSOR_POSITION_FRONT3 ||
		position == SENSOR_POSITION_FRONT4 ) {
		is_front = true;
	}

	strcpy(tempbuf, "20");

	for (i = 0; i < AF_CAL_MAX; i++) {
		if (rom_cal_index == 1) {
			if (i >= rom_info->rom_sensor2_af_cal_addr_len) {
				strncat(tempbuf, " N", strlen(" N"));
			} else {
				sprintf(tmpChar, " %d", *((s32 *)&cal_buf[rom_info->rom_sensor2_af_cal_addr[i]]));
				strncat(tempbuf, tmpChar, strlen(tmpChar));
			}
		} else {
			if (i >= rom_info->rom_af_cal_addr_len) {
				strncat(tempbuf, " N", strlen(" N"));
			} else {
				sprintf(tmpChar, " %d", *((s32 *)&cal_buf[rom_info->rom_af_cal_addr[i]]));
				strncat(tempbuf, tmpChar, strlen(tmpChar));
			}
		}
	}

	return sprintf(buf, "%s\n", tempbuf);

err_afcal:
	return sprintf(buf, "1 N N\n");
}

ssize_t camera_camfw_show(char *buf, enum is_cam_info_index cam_index, bool camfw_full)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	char command_ack[20] = {0, };
	char sensor_name[20] = {0, };
	char *cam_fw;
	char *loaded_fw;
	char *phone_fw;
	int position;
	int rom_id;
	int rom_cal_index;
	int ret;

	is_vendor_check_hw_init_running();
	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		ret = is_get_sensor_data(NULL, sensor_name, position);

		if (ret < 0)
			return sprintf(buf, "UNKNOWN UNKNOWN\n");
		else
			return sprintf(buf, "%s N\n", sensor_name);
	}

	info("%s: [%d][%d][%d]\n", __func__, cam_index, position, rom_id);

	is_sec_get_rom_info(&rom_info, rom_id);

	if (!is_sec_check_rom_ver(core, rom_id)) {
		err("%s: invalid ROM version [%d][%d][%d]", __func__, cam_index, position, rom_id);
		goto err_camfw;
	}

	if (rom_cal_index == 1)
		cam_fw = rom_info->header2_ver;
	else
		cam_fw = rom_info->header_ver;

	loaded_fw = cam_fw;
	phone_fw = "N";

	if (!rom_info->crc_error && !rom_info->other_vendor_module) {
		if (camfw_full)
			return sprintf(buf, "%s %s %s\n", cam_fw, phone_fw, loaded_fw);
		else
			return sprintf(buf, "%s %s\n", cam_fw, loaded_fw);
	} else {
		err("%s: NG CRC Check Fail [%d][%d][%d]", __func__, cam_index, position, rom_id);
		if (position == SENSOR_POSITION_REAR_TOF ||
			position == SENSOR_POSITION_FRONT_TOF) {
			strcpy(command_ack, "N");
		} else {
			strcpy(command_ack, "NG");
		}

		if (camfw_full)
			return sprintf(buf, "%s %s %s\n", cam_fw, phone_fw, command_ack);
		else
			return sprintf(buf, "%s %s\n", cam_fw, command_ack);
	}

err_camfw:
	if (position == SENSOR_POSITION_REAR_TOF ||
		position == SENSOR_POSITION_FRONT_TOF) {
		strcpy(command_ack, "N");
	} else {
		strcpy(command_ack, "NG");
	}

	cam_fw = "NULL";

	if (position == SENSOR_POSITION_REAR)
		phone_fw = "NULL";
	else
		phone_fw = "N";

	if (camfw_full)
		return sprintf(buf, "%s %s %s\n", cam_fw, phone_fw, command_ack);
	else
		return sprintf(buf, "%s %s\n", cam_fw, command_ack);
}

ssize_t camera_info_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_module_enum *module;
	struct is_cam_info *cam_info;
	int ret = 0;
	bool use_rom, use_ois;
	char *param_msg1 = "ISP=INT;CALMEM=";
	char *param_msg2 = ";READVER=SYSFS;COREVOLT=N;UPGRADE=N;FWWRITE=N;FWDUMP=N;CC=N;OIS=";
	char *param_msg3 = ";VALID=Y;";

	is_get_cam_info_from_index(&cam_info, cam_index);

	if (cam_info->valid) {
		ret = is_vendor_get_module_from_position(cam_info->position, &module);
		if (module && !ret) {
			use_rom = (module->pdata->rom_id != ROM_ID_NOTHING);
			use_ois = (module->pdata->ois_product_name != OIS_NAME_NOTHING
					|| module->pdata->mcu_product_name != MCU_NAME_NOTHING);
			return sprintf(buf, "%s%s%s%s%s\n",
				param_msg1, (use_rom ? "Y" : "N"),
				param_msg2, (use_ois ? "Y" : "N"),
				param_msg3);
		}
	}

	return sprintf(buf, "%s%s\n",
		"ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;",
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N;");
}

ssize_t camera_checkfw_factory_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;

	is_vendor_check_hw_init_running();
	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d][%d]", __func__, cam_index, position, rom_id);
		goto err_factory_checkfw;
	}

	is_sec_get_rom_info(&rom_info, rom_id);

	if (!is_sec_check_rom_ver(core, rom_id)) {
		err(" NG, invalid ROM version");
		goto err_factory_checkfw;
	}

	if (!rom_info->crc_error) {
		if (!rom_info->final_module) {
			err(" NG, not final cam module");
			return sprintf(buf, "%s\n", "NG_VER");
		} else {
			return sprintf(buf, "%s\n", "OK");
		}
	} else {
		err(" NG, crc check fail");
		return sprintf(buf, "%s\n", "NG_CRC");
	}

err_factory_checkfw:
	return sprintf(buf, "%s\n", "NG_VER");
}

ssize_t camera_checkfw_user_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;

	is_vendor_check_hw_init_running();
	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d][%d]", __func__, cam_index, position, rom_id);
		goto err_user_checkfw;
	}

	is_sec_get_rom_info(&rom_info, rom_id);

	if (!is_sec_check_rom_ver(core, rom_id)) {
		err(" NG, invalid ROM version");
		goto err_user_checkfw;
	}

	if (rom_info->crc_error)
		err(" NG, crc check fail");
	else if (!rom_info->latest_module)
		err(" NG, not latest cam module");
	else
		return sprintf(buf, "%s\n", "OK");

err_user_checkfw:
	return sprintf(buf, "%s\n", "NG");
}

#ifdef USE_KERNEL_VFS_READ_WRITE
static int is_sysfs_get_dualcal_param_from_file(int offset, char **buf, int position, int *size)
{
	int ret = 0;
	long fsize, nread;
	struct file *fp;
	char *temp_buf = NULL;
	char fw_path[100];
	struct is_core *core;
	struct is_vendor *vendor;
	char *dual_cal_file_name;

	info("%s: E", __func__);

	core = is_get_is_core();
	vendor = core->vendor;

	is_vendor_get_dualcal_file_name_from_position(position, &dual_cal_file_name);
	snprintf(fw_path, sizeof(fw_path), "%s%s", IS_FW_PATH, dual_cal_file_name);

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		ret = PTR_ERR(fp);
		err("filp_open(%s) fail(%d)!!\n", fw_path, ret);
		goto p_err;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	temp_buf = vmalloc(fsize);
	if (!temp_buf) {
		err("failed to allocate memory");
		ret = -ENOMEM;
		goto p_err;
	}

	fp->f_pos = offset;
	nread = kernel_read(fp,  (void *)temp_buf, fsize, &fp->f_pos);

	/* Cal data save */
	memcpy(*buf, temp_buf, nread);
	*size = nread;
	info("%s dual cal file path %s, size %lu Bytes", __func__, fw_path, nread);

p_err:
	if (temp_buf)
		vfree(temp_buf);

	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

	info("%s: X", __func__);
	return ret;
}
#endif

static int is_sysfs_get_dualcal_param_from_rom(int slave_position, char **buf, int *size)
{
	char *cal_buf;
	int rom_dual_cal_start_addr;
	int rom_dual_cal_size;
	u32 rom_dual_flag_dummy_addr = 0;
	int rom_dualcal_id;
	int rom_dualcal_index;
	struct is_core *core = is_get_is_core();
	struct is_vendor_rom *rom_info = NULL;

	*buf = NULL;
	*size = 0;

	is_vendor_get_rom_dualcal_info_from_position(slave_position, &rom_dualcal_id, &rom_dualcal_index);
	if (rom_dualcal_id == ROM_ID_NOTHING) {
		err("[rom_dualcal_id:%d pos:%d] invalid ROM ID", rom_dualcal_id, slave_position);
		return -EINVAL;
	}

	is_sec_get_rom_info(&rom_info, rom_dualcal_id);
	cal_buf = rom_info->buf;
	if (is_sec_check_rom_ver(core, rom_dualcal_id) == false) {
		err("[rom_dualcal_id:%d pos:%d] ROM version is low. Cannot load dual cal.",
			rom_dualcal_id, slave_position);
		return -EINVAL;
	}

	if (rom_info->crc_error) {
		err("[rom_dualcal_id:%d pos:%d] ROM Cal CRC is wrong. Cannot load dual cal.",
			rom_dualcal_id, slave_position);
		return -EINVAL;
	}

	if (rom_dualcal_index == ROM_DUALCAL_SLAVE0) {
		rom_dual_cal_start_addr = rom_info->rom_dualcal_slave0_start_addr;
		rom_dual_cal_size = rom_info->rom_dualcal_slave0_size;
	} else if (rom_dualcal_index == ROM_DUALCAL_SLAVE1) {
		rom_dual_cal_start_addr = rom_info->rom_dualcal_slave1_start_addr;
		rom_dual_cal_size = rom_info->rom_dualcal_slave1_size;
		rom_dual_flag_dummy_addr = rom_info->rom_dualcal_slave1_dummy_flag_addr;
	} else {
		err("[index:%d] not supported index.", rom_dualcal_index);
		return -EINVAL;
	}

	if (rom_dual_flag_dummy_addr != 0 && cal_buf[rom_dual_flag_dummy_addr] != 7) {
		err("[rom_dualcal_id:%d pos:%d addr:%x] invalid dummy_flag [%d]. Cannot load dual cal.",
			rom_dualcal_id, slave_position, rom_dual_flag_dummy_addr, cal_buf[rom_dual_flag_dummy_addr]);
		return -EINVAL;
	}

	if (rom_dual_cal_start_addr <= 0) {
		info("[%s] not available dual_cal\n", __func__);
		return -EINVAL;
	}

	*buf = &cal_buf[rom_dual_cal_start_addr];
	*size = rom_dual_cal_size;

	return 0;
}

int is_sysfs_get_dualcal_param(struct device *dev, char **buf, int position)
{
	int ret;
	int copy_size = 0;
	char *temp_buf;
	bool use_dualcal_from_file;

	is_vendor_get_use_dualcal_file_from_position(position, &use_dualcal_from_file);

	if (use_dualcal_from_file) {
#ifdef USE_KERNEL_VFS_READ_WRITE
		ret = is_sysfs_get_dualcal_param_from_file(0, buf, position, &copy_size);
#else
		ret = is_vendor_get_dualcal_param_from_file(dev, buf, position, &copy_size);
#endif
		if (ret < 0) {
			err("%s: Fail to read dualcal bin of %d", __func__, position);
			copy_size = 0;
		} else {
			info("%s: success to get dualcal from firmware of %d, size = %d",
				__func__, position, copy_size);
		}
	} else {
		ret = is_sysfs_get_dualcal_param_from_rom(position, &temp_buf, &copy_size);
		if (ret < 0) {
			err("%s: Fail to get dualcal of %d", __func__, position);
		} else {
			info("%s: success to get dualcal of %d", __func__, position);
			memcpy(*buf, temp_buf, copy_size);
		}
	}

	return copy_size;
}

ssize_t camera_eeprom_retry_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_cam_info *cam_info;
	struct is_vendor_rom *rom_info;
	int position;
	int rom_id;
	int rom_cal_index;

	is_vendor_check_hw_init_running();
	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("ROM_ID_NOTHING [%d][%d][%d]", cam_index, position, rom_id);
		goto err_eeprom_retry_show;
	}

	info("[%s] [%d][%d][%d]\n", __func__, cam_index, position, rom_id);

	is_sec_get_rom_info(&rom_info, rom_id);

	sprintf(buf, "%d", rom_info->crc_retry_cnt);

err_eeprom_retry_show:
	strncat(buf, "\n", strlen("\n"));

	return strlen(buf);
}

static int test_phy_margin_check(char *buffer, int cam_index)
{
	int ret;
	u32 position = cam_index + SP_REAR;
	char const *phy_tune_default_mode;
	char sensor_run_args[22] = {0, };
	char phy_tune_args[10] = {0, };
	char tmpChar[5] = {0, };
	struct device_node *dnode;
	struct device *dev;
	struct is_module_enum *module;
	struct is_device_sensor *device;

	is_vendor_get_module_from_position(cam_index, &module);

	WARN_ON(!module);
	WARN_ON(!module->subdev);
	device = v4l2_get_subdev_hostdata(module->subdev);

	dev = &device->pdev->dev;
	dnode = dev->of_node;

	ret = of_property_read_string(dnode, "phy_tune_default_mode", &phy_tune_default_mode);
	if (ret) {
		err("phy_tune_default_mode read fail!!");
		return ret;
	}

	if (device->pdata->use_cphy)
		is_set_debug_param(IS_DEBUG_PARAM_PHY_TUNE, 1);
	else
		is_set_debug_param(IS_DEBUG_PARAM_PHY_TUNE, 2);

	strcpy(sensor_run_args, "1"); // action : start=1, stop=0
	sprintf(tmpChar, " %d ", position);
	strncat(sensor_run_args, tmpChar, strlen(tmpChar)); // camera position
	strncat(sensor_run_args, phy_tune_default_mode, strlen(phy_tune_default_mode)); // camera default mode
	pablo_test_set_sensor_run(sensor_run_args, NULL);

	if (device->pdata->use_cphy)
		strcpy(phy_tune_args, "32 200 0"); // default phy tune command
	else
		strcpy(phy_tune_args, "12 100 0"); // dphy tune command

	pablo_test_set_phy_tune(phy_tune_args, NULL);

	sprintf(buffer, "%d,%d,%d", pablo_test_get_open_window(),
            pablo_test_get_total_err(), pablo_test_get_first_open());

	strcpy(sensor_run_args, "0");
	pablo_test_set_sensor_run(sensor_run_args, NULL);

	is_set_debug_param(IS_DEBUG_PARAM_PHY_TUNE, 0);

	return 0;
}

ssize_t camera_phy_tune_show(char *buf, enum is_cam_info_index cam_index)
{
	char tempBuf[30];
	int position;
	struct is_cam_info *cam_info;

	is_get_cam_info_from_index(&cam_info, cam_index);
	position = cam_info->position;

	memset(tempBuf, 0, sizeof(tempBuf));
	test_phy_margin_check(tempBuf, position);
	sprintf(buf, "%s\n", tempBuf);

	return strlen(buf);
}

ssize_t camera_moduleid_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;

	int position;
	int rom_id;
	int rom_cal_index;

	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err_moduleid;
	}

	is_sec_get_rom_info(&rom_info, rom_id);

	if (is_sec_is_valid_moduleid(rom_info->rom_module_id)) {
		return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
			rom_info->rom_module_id[0], rom_info->rom_module_id[1], rom_info->rom_module_id[2],
			rom_info->rom_module_id[3], rom_info->rom_module_id[4], rom_info->rom_module_id[5],
			rom_info->rom_module_id[6], rom_info->rom_module_id[7], rom_info->rom_module_id[8],
			rom_info->rom_module_id[9]);
	}

err_moduleid:
	return sprintf(buf, "%s\n", "0000000000");
}

static const char *is_cam_info_type_list[CAM_INFO_TYPE_MAX] = {
	[CAM_INFO_TYPE_RW1] = "rear_wide",
	[CAM_INFO_TYPE_RS1] = "rear_ultra_wide",
	[CAM_INFO_TYPE_RT1] = "rear_tele",
	[CAM_INFO_TYPE_FW1] = "front_wide",
	[CAM_INFO_TYPE_RT2] = "rear_super_tele",
	[CAM_INFO_TYPE_UW1] = "inner",
	[CAM_INFO_TYPE_RM1] = "rear_macro",
	[CAM_INFO_TYPE_RB1] = "rear_depth",
	[CAM_INFO_TYPE_FS1] = "front_ultra_wide",
};

ssize_t camera_sensor_type_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_cam_info *cam_info = NULL;

	is_get_cam_info_from_index(&cam_info, cam_index);

	if (cam_info->type >= CAM_INFO_TYPE_MAX) {
		info("%s : is_cam_info_type index overflow", __func__);
		goto err_module_type;
	} else {
		return sprintf(buf, "%s\n", is_cam_info_type_list[cam_info->type]);
	}

err_module_type:
	return sprintf(buf, "%s\n", "is_cam_info_type index overflow");
}

ssize_t camera_mtf_exif_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	int mtf_offset;
	char *cal_buf;

	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err_mtf_exif;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;
	if (cal_buf == NULL) {
		err("%s: cal_buf is NULL", __func__);
		goto err_mtf_exif;
	}

	if (rom_cal_index == 1) {
		mtf_offset = rom_info->rom_header_sensor2_mtf_data_addr;
	} else {
		mtf_offset = rom_info->rom_header_mtf_data_addr;
	}

	if (mtf_offset != -1)
		memcpy(buf, &cal_buf[mtf_offset], IS_RESOLUTION_DATA_SIZE);
	else
		memset(buf, 0, IS_RESOLUTION_DATA_SIZE);

	return IS_RESOLUTION_DATA_SIZE;

err_mtf_exif:
	memset(buf, '\0', IS_RESOLUTION_DATA_SIZE);

	return IS_RESOLUTION_DATA_SIZE;
}

ssize_t camera_paf_cal_check_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;
	int paf_cal_start_addr;
	char *cal_buf;
	u8 data[5] = {0, };
	u32 paf_err_data_result = 0;

	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err_paf_cal_check;
	}

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;
	if (cal_buf == NULL)
		goto err_paf_cal_check;

	if (rom_cal_index == ROM_CAL_SLAVE0) {
		paf_cal_start_addr = rom_info->rom_sensor2_paf_cal_start_addr;
	} else {
		paf_cal_start_addr = rom_info->rom_paf_cal_start_addr;
	}

	if (paf_cal_start_addr != -1)
		memcpy(data, &cal_buf[paf_cal_start_addr + IS_PAF_CAL_ERR_CHECK_OFFSET], 4);
	else
		memset(data, 0, 4);

	paf_err_data_result = *data | ( *(data + 1) << 8) | ( *(data + 2) << 16) | (*(data + 3) << 24);

	return sprintf(buf, "%08X\n", paf_err_data_result);

err_paf_cal_check:
	memset(data, '\0', 4);

	return sprintf(buf, "%08X\n", paf_err_data_result);
}

/* PAF offset read */
ssize_t camera_paf_offset_show(char *buf, bool is_mid)
{
	char *cal_buf;
	char tempbuf[10];
	int i;
	int divider;
	int paf_offset_count;
	u32 paf_offset_base = 0;
	struct is_sysfs_config *sysfs_config = is_vendor_get_sysfs_config();
	struct is_vendor_rom *rom_info;

	is_sec_get_rom_info(&rom_info, ROM_ID_REAR);
	cal_buf = rom_info->buf;

	if (sysfs_config->rear_afcal) {
		if (rom_info->rom_paf_cal_start_addr == -1) {
			err("%s fail, check DT", __func__);
			strncat(buf, "\n", strlen("\n"));
			return strlen(buf);
		}

		if (is_mid)
			paf_offset_base = rom_info->rom_paf_cal_start_addr + IS_PAF_OFFSET_MID_OFFSET;
		else
			paf_offset_base = rom_info->rom_paf_cal_start_addr + IS_PAF_OFFSET_PAN_OFFSET;
	}

	if (paf_offset_base == 0) {
		strncat(buf, "\n", strlen("\n"));
		err("%s: no definition for paf offset mid/pan[%d]", __func__, is_mid);
		return strlen(buf);
	}

	if (is_mid) {
		divider = 8;
		paf_offset_count = IS_PAF_OFFSET_MID_SIZE / divider;
	} else {
		divider = 2;
		paf_offset_count = IS_PAF_OFFSET_PAN_SIZE / divider;
	}

	memset(tempbuf, 0, sizeof(tempbuf));

	for (i = 0; i < paf_offset_count - 1; i++) {
		sprintf(tempbuf, "%d,", *((s16 *)&cal_buf[paf_offset_base + divider * i]));
		strncat(buf, tempbuf, strlen(tempbuf));
		memset(tempbuf, 0, sizeof(tempbuf));
	}
	sprintf(tempbuf, "%d", *((s16 *)&cal_buf[paf_offset_base + divider * i]));
	strncat(buf, tempbuf, strlen(tempbuf));

	strncat(buf, "\n", strlen("\n"));
	return strlen(buf);
}

ssize_t camera_sensorid_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	int position;
	int rom_id;
	int rom_cal_index;

	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_info_from_position(position, &rom_id, &rom_cal_index);

	if (rom_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_id);
		goto err_sensorid_exif;
	}

	is_sec_get_rom_info(&rom_info, rom_id);

	if (rom_cal_index == 1)
		memcpy(buf, rom_info->rom_sensor2_id, IS_SENSOR_ID_SIZE);
	else
		memcpy(buf, rom_info->rom_sensor_id, IS_SENSOR_ID_SIZE);

	return IS_SENSOR_ID_SIZE;

err_sensorid_exif:
	memset(buf, '\0', IS_SENSOR_ID_SIZE);

	return IS_SENSOR_ID_SIZE;
}

ssize_t camera_tilt_show(char *buf, enum is_cam_info_index cam_index)
{
	struct is_core *core = is_get_is_core();
	struct is_vendor_rom *rom_info;
	struct is_cam_info *cam_info;
	char *cal_buf;
	int position;
	int rom_dualcal_id;
	int rom_dualcal_index;
	char tempbuf[25] = {0, };
	int i = 0;

	is_get_cam_info_from_index(&cam_info, cam_index);

	position = cam_info->position;
	is_vendor_get_rom_dualcal_info_from_position(position, &rom_dualcal_id, &rom_dualcal_index);

	if (rom_dualcal_id == ROM_ID_NOTHING) {
		err("%s: ROM_ID_NOTHING [%d][%d]", __func__, position, rom_dualcal_id);
		goto err_tilt;
	}

	is_sec_get_rom_info(&rom_info, rom_dualcal_id);
	cal_buf = rom_info->buf;

	if (!is_sec_check_rom_ver(core, rom_dualcal_id)){
		err(" NG, invalid ROM version");
		goto err_tilt;
	}

	if (rom_info->other_vendor_module || rom_info->crc_error) {
		err(" NG, invalid ROM version");
		goto err_tilt;
	}

	strcat(buf, "1");

	switch (rom_dualcal_index)
	{
	case ROM_DUALCAL_SLAVE0:
		if (rom_info->rom_dualcal_slave0_tilt_list_len > IS_ROM_DUAL_TILT_MAX_LIST) {
			err(" NG, invalid ROM dual tilt value, dualcal_index[%d]", rom_dualcal_index);
			goto err_tilt;
		}

		for (i = 0; i < IS_ROM_DUAL_TILT_MAX_LIST - 1; i++) {
			sprintf(tempbuf, " %d", *((s32 *)&cal_buf[rom_info->rom_dualcal_slave0_tilt_list[i]]));
			strncat(buf, tempbuf, strlen(tempbuf));
			memset(tempbuf, 0, sizeof(tempbuf));
		}

		if (rom_info->rom_dualcal_slave0_tilt_list_len == IS_ROM_DUAL_TILT_MAX_LIST) {
			strcat(buf, " ");
			memcpy(tempbuf, &cal_buf[rom_info->rom_dualcal_slave0_tilt_list[i]],
				IS_DUAL_TILT_PROJECT_NAME_SIZE);
			tempbuf[IS_DUAL_TILT_PROJECT_NAME_SIZE] = '\0';
			for (i = 0; i < IS_DUAL_TILT_PROJECT_NAME_SIZE; i++) {
				if (tempbuf[i] == 0xFF && i == 0) {
					sprintf(tempbuf, "NONE");
					break;
				}
				if (tempbuf[i] == 0) {
					tempbuf[i] = '\0';
					break;
				}
			}
			strncat(buf, tempbuf, strlen(tempbuf));
		}
		strncat(buf, "\n", strlen("\n"));

		return strlen(buf);
	case ROM_DUALCAL_SLAVE1:
		if (rom_info->rom_dualcal_slave1_tilt_list_len > IS_ROM_DUAL_TILT_MAX_LIST) {
			err(" NG, invalid ROM dual tilt value, dualcal_index[%d]", rom_dualcal_index);
			goto err_tilt;
		}

		for (i = 0; i < IS_ROM_DUAL_TILT_MAX_LIST - 1; i++) {
			sprintf(tempbuf, " %d", *((s32 *)&cal_buf[rom_info->rom_dualcal_slave1_tilt_list[i]]));
			strncat(buf, tempbuf, strlen(tempbuf));
			memset(tempbuf, 0, sizeof(tempbuf));
		}

		if (rom_info->rom_dualcal_slave1_tilt_list_len == IS_ROM_DUAL_TILT_MAX_LIST) {
			strcat(buf, " ");
			memcpy(tempbuf, &cal_buf[rom_info->rom_dualcal_slave1_tilt_list[i]],
				IS_DUAL_TILT_PROJECT_NAME_SIZE);
			tempbuf[IS_DUAL_TILT_PROJECT_NAME_SIZE] = '\0';
			for (i = 0; i < IS_DUAL_TILT_PROJECT_NAME_SIZE; i++) {
				if (tempbuf[i] == 0xFF && i == 0) {
					sprintf(tempbuf, "NONE");
					break;
				}
				if (tempbuf[i] == 0) {
					tempbuf[i] = '\0';
					break;
				}
			}
			strncat(buf, tempbuf, strlen(tempbuf));
		}
		strncat(buf, "\n", strlen("\n"));

		return strlen(buf);
	case ROM_DUALCAL_SLAVE2:
		if (rom_info->rom_dualcal_slave2_tilt_list_len > IS_ROM_DUAL_TILT_MAX_LIST) {
			err(" NG, invalid ROM dual tilt value, dualcal_index[%d]", rom_dualcal_index);
			goto err_tilt;
		}

		for (i = 0; i < IS_ROM_DUAL_TILT_MAX_LIST - 1; i++) {
			sprintf(tempbuf, " %d", *((s32 *)&cal_buf[rom_info->rom_dualcal_slave2_tilt_list[i]]));
			strncat(buf, tempbuf, strlen(tempbuf));
			memset(tempbuf, 0, sizeof(tempbuf));
		}

		if (rom_info->rom_dualcal_slave2_tilt_list_len == IS_ROM_DUAL_TILT_MAX_LIST) {
			strcat(buf, " ");
			memcpy(tempbuf, &cal_buf[rom_info->rom_dualcal_slave2_tilt_list[i]],
				IS_DUAL_TILT_PROJECT_NAME_SIZE);
			tempbuf[IS_DUAL_TILT_PROJECT_NAME_SIZE] = '\0';
			for (i = 0; i < IS_DUAL_TILT_PROJECT_NAME_SIZE; i++) {
				if (tempbuf[i] == 0xFF && i == 0) {
					sprintf(tempbuf, "NONE");
					break;
				}
				if (tempbuf[i] == 0) {
					tempbuf[i] = '\0';
					break;
				}
			}
			strncat(buf, tempbuf, strlen(tempbuf));
		}
		strncat(buf, "\n", strlen("\n"));

		return strlen(buf);
	default:
		err("not defined tilt cal values");
		break;
	}

err_tilt:
	return sprintf(buf, "%s\n", "NG");
}

int camera_tof_get_laser_photo_diode(int position, u16 *value)
{
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_cis *cis = NULL;
	int i;

	core = is_get_is_core();
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		is_search_sensor_module_with_position(&core->sensor[i],
				position, &module);
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
		return CALL_CISOPS(cis, cis_get_laser_photo_diode, sensor_peri->subdev_cis, value);
	}

	return 0;
}

int camera_tof_set_laser_current(int position, u32 value)
{
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_cis *cis = NULL;
	int i;

	core = is_get_is_core();
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		is_search_sensor_module_with_position(&core->sensor[i],
				position, &module);
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
		CALL_CISOPS(cis, cis_set_laser_current, sensor_peri->subdev_cis, value);
	}

	return 0;
}

int is_create_sysfs(void)
{
	int ret = 0;

	if (!is_get_is_core()) {
		err("is_core is null");
		return -EINVAL;
	}

	ret = svc_cheating_prevent_device_file_create();
	if (ret < 0)
		return ret;

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class)) {
			pr_err("Failed to create class(camera)!\n");
			return PTR_ERR(camera_class);
		}
	}

	ret = is_create_rear_sysfs(camera_class);
	if (ret != 0)
		return ret;

	ret = is_create_front_sysfs(camera_class);
	if (ret != 0)
		return ret;

#if IS_ENABLED(CONFIG_OIS_USE)
	is_create_ois_sysfs(camera_class);
#endif

	return ret;
}

int is_destroy_sysfs(void)
{
	is_destroy_rear_sysfs(camera_class);
	is_destroy_front_sysfs(camera_class);
#if IS_ENABLED(CONFIG_OIS_USE)
	is_destroy_ois_sysfs(camera_class);
#endif

	class_destroy(camera_class);

	return 0;
}

struct class *is_get_camera_class(void)
{
	return camera_class;
}
EXPORT_SYMBOL_GPL(is_get_camera_class);
