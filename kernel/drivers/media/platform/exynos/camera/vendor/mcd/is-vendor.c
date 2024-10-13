/*
* Samsung Exynos5 SoC series FIMC-IS driver
*
* exynos5 fimc-is vendor functions
*
* Copyright (c) 2011 Samsung Electronics Co., Ltd
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <exynos-is-module.h>
#include "is-vendor.h"
#include "is-vendor-private.h"
#include "is-core.h"
#include "is-sec-define.h"
#include "is-dt.h"
#include "is-sysfs.h"
#include "is-sysfs-rear.h"
#if defined(CONFIG_OIS_USE)
#include "is-sysfs-ois.h"
#include "is-device-ois.h"
#endif
#include "is-notifier.h"

#include "pablo-binary.h"

#include "is-interface-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-cis.h"
#include "is-interface-library.h"
#include "is-device-af.h"
#include "is-devicemgr.h"
#include "is-vendor-device-info.h"
#include "is-vendor-private.h"
#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
#include "is-hw-api-ois-mcu.h"
#include "is-vendor-ois-internal-mcu.h"
#elif defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
#include "is-vendor-ois-external-mcu.h"
#elif defined(CONFIG_CAMERA_USE_AOIS)
#include "is-vendor-aois.h"
#endif
#include "is-device-rom.h"

#ifdef CONFIG_OIS_USE
extern bool check_shaking_noise;
#endif

static bool is_hw_init_running;
static bool check_hw_init;
static struct mutex g_efs_mutex;
static struct mutex g_shaking_mutex;

#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
struct workqueue_struct *sensor_pwr_ctrl_wq;
#define CAMERA_WORKQUEUE_MAX_WAITING	1000
#endif

extern int sensor_cis_set_registers(struct v4l2_subdev *subdev, const u32 *regs, const u32 size);

#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
static struct cam_hw_param_collector cam_hwparam_collector;

void is_sec_init_err_cnt(struct cam_hw_param *hw_param)
{
	if (hw_param) {
		memset(hw_param, 0, sizeof(struct cam_hw_param));
	}
}

void is_sec_get_hw_param(struct cam_hw_param **hw_param, u32 position)
{
	switch (position) {
	case SENSOR_POSITION_REAR:
		*hw_param = &cam_hwparam_collector.rear_hwparam;
		break;
	case SENSOR_POSITION_REAR2:
		*hw_param = &cam_hwparam_collector.rear2_hwparam;
		break;
	case SENSOR_POSITION_REAR3:
		*hw_param = &cam_hwparam_collector.rear3_hwparam;
		break;
	case SENSOR_POSITION_REAR4:
		*hw_param = &cam_hwparam_collector.rear4_hwparam;
		break;
	case SENSOR_POSITION_FRONT:
		*hw_param = &cam_hwparam_collector.front_hwparam;
		break;
	case SENSOR_POSITION_FRONT2:
		*hw_param = &cam_hwparam_collector.front2_hwparam;
		break;
	case SENSOR_POSITION_REAR_TOF:
		*hw_param = &cam_hwparam_collector.rear_tof_hwparam;
		break;
	case SENSOR_POSITION_FRONT_TOF:
		*hw_param = &cam_hwparam_collector.front_tof_hwparam;
		break;
	default:
		*hw_param = NULL;
		break;
	}
}
EXPORT_SYMBOL_GPL(is_sec_get_hw_param);

void is_sec_update_hw_param_mipi_info_on_err(struct cam_hw_param *hw_param)
{
	struct cam_cp_noti_cell_infos cell_infos;
	struct cam_cp_cell_info *cell_info;
	int i;

	is_vendor_get_rf_cell_infos(&cell_infos);

	for (i = 0; i < cell_infos.num_cell; i++) {
		cell_info = &cell_infos.cell_list[i];
		if (cell_info->connection_status == CAM_CON_STATUS_PRIMARY_SERVING) {
			hw_param->mipi_info_on_err.rat = cell_info->rat;
			hw_param->mipi_info_on_err.band = cell_info->band;
			hw_param->mipi_info_on_err.channel = cell_info->channel;
			break;
		}
	}
}
EXPORT_SYMBOL_GPL(is_sec_update_hw_param_mipi_info_on_err);

void is_sec_update_hw_param_af_info(struct is_device_sensor *device, u32 position, int state)
{
	int i, id, temp_id = 2;
	u16 hall_value = 0;
	u32 posistion_hall_diff;
	u32 timestamp_interval;
	struct v4l2_subdev *subdev_module;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_actuator *actuator;
	struct cam_hw_param *hw_param;

	subdev_module = device->subdev_module;
	WARN_ON(!subdev_module);

	module = v4l2_get_subdevdata(subdev_module);
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	is_sec_get_hw_param(&hw_param, sensor_peri->module->position);
	WARN_ON(!hw_param);

	actuator = sensor_peri->actuator;

	if (!actuator || !actuator->vendor_use_hall)
		return;

	if (state == HW_PARAM_STREAM_ON) {
		for (i = 0; i < 3; i++) {
			hw_param->af_pos_info[i].timestamp = 0;
			hw_param->af_pos_info[i].position = -1;
			hw_param->af_pos_info[i].hall_value = -1;
		}
		return;
	}

	id = (hw_param->af_pos_info[0].hall_value < 0) ? 0 : 1;

	if (hw_param->af_pos_info[temp_id].position < 0) {
		hw_param->af_pos_info[temp_id].timestamp = ktime_get_ns();
		hw_param->af_pos_info[temp_id].position = position;
	} else {
		timestamp_interval =
			(u32)((ktime_get_ns() - hw_param->af_pos_info[temp_id].timestamp) / 1000000); //ns -> ms
		if (timestamp_interval > actuator->vendor_hall_min_interval) {
			if (actuator->state != ACTUATOR_STATE_ACTIVE ||
					CALL_ACTUATOROPS(actuator, get_hall_value, actuator->subdev, &hall_value)) {
				hw_param->af_pos_info[temp_id].position = -1;
				return;
			}

			hw_param->af_pos_info[id].timestamp = hw_param->af_pos_info[temp_id].timestamp;
			hw_param->af_pos_info[id].position = hw_param->af_pos_info[temp_id].position;
			hw_param->af_pos_info[id].hall_value = hall_value;
		}

		hw_param->af_pos_info[temp_id].timestamp = ktime_get_ns();
		hw_param->af_pos_info[temp_id].position = position;
	}

	if (state == HW_PARAM_STREAM_OFF) {
		for (i = 0; i < 2; i++) {
			if (hw_param->af_pos_info[i].position < 0)
				break;

			posistion_hall_diff =
				abs(hw_param->af_pos_info[i].position - hw_param->af_pos_info[i].hall_value);
			if (posistion_hall_diff > actuator->vendor_hall_hw_param_tolerance) {
				hw_param->af_fail_cnt++;
				hw_param->af_pos_info_on_fail.position = hw_param->af_pos_info[i].position;
				hw_param->af_pos_info_on_fail.hall_value = hw_param->af_pos_info[i].hall_value;

				err("AF Hall check fail. sensor(%d), set af position(%d), get hall_value(%d)",
					sensor_peri->module->position, hw_param->af_pos_info[i].position,
					hw_param->af_pos_info[i].hall_value);
			}
		}
	}
}
EXPORT_SYMBOL_GPL(is_sec_update_hw_param_af_info);
#endif

bool is_sec_is_valid_moduleid(char *moduleid)
{
	int i = 0;

	if (moduleid == NULL || strlen(moduleid) < 5) {
		goto err;
	}

	for (i = 0; i < 5; i++) {
		if (!((moduleid[i] >= '0' && moduleid[i] <= '9') ||
			(moduleid[i] >= 'A' && moduleid[i] <= 'Z'))) {
			goto err;
		}
	}

	return true;

err:
	warn("invalid moduleid\n");
	return false;
}

void is_vendor_csi_stream_on(struct is_device_csi *csi)
{
	struct is_device_sensor *device = NULL;
#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
	struct cam_hw_param *hw_param = NULL;
#endif

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);
	if (device == NULL) {
		warn("device is null");
		return;
	}

	is_vendor_update_mipi_info(device);

#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
	is_sec_get_hw_param(&hw_param, device->position);

	if (hw_param)
		hw_param->mipi_err_check = false;
#endif
}

void is_vendor_csi_stream_off(struct is_device_csi *csi)
{
	struct is_device_sensor *device;
	struct is_device_ischain *ischain;
	struct is_group *group;

	device = container_of(csi->subdev, struct is_device_sensor, subdev_csi);
	ischain = device->ischain;
	if (!ischain) {
		merr("ischain is NULL", device);
		return;
	}

	group = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, ischain->group[GROUP_SLOT_3AA]);

	if (!group) {
		mwarn("group is NULL", device);
		return;
	}

	group->remainRemosaicCropIntentCount = 0;
}

void is_vendor_csi_err_handler(u32 sensor_position)
{
#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
	struct cam_hw_param *hw_param = NULL;

	is_sec_get_hw_param(&hw_param, sensor_position);

	if (hw_param && !hw_param->mipi_err_check) {
		hw_param->mipi_sensor_err_cnt++;
		hw_param->mipi_err_check = true;
		is_sec_update_hw_param_mipi_info_on_err(hw_param);
	}
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
	is_vendor_sec_abc_send_event_mipi_error(sensor_position);
#endif
}

void is_vendor_csi_err_print_debug_log(struct is_device_sensor *device)
{
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_cis *cis = NULL;
	int ret = 0;

	if (device && device->pdev) {
		ret = is_sensor_g_module(device, &module);
		if (ret) {
			warn("%s sensor_g_module failed(%d)", __func__, ret);
			return;
		}

		sensor_peri = (struct is_device_sensor_peri *)module->private_data;
		if (sensor_peri->subdev_cis) {
			cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);
			CALL_CISOPS(cis, cis_log_status, sensor_peri->subdev_cis);
		}
	}
}

static int parse_sysfs_caminfo(struct device_node *np,
				struct is_cam_info *cam_infos, int i)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(np, "position", cam_infos[i].position);
	DT_READ_U32(np, "valid", cam_infos[i].valid);
	DT_READ_U32(np, "type", cam_infos[i].type);

	return 0;
}

static int is_sysfs_dt(struct device_node *np, struct is_vendor_private *vendor_priv)
{
	struct is_sysfs_config *sysfs_config = &vendor_priv->sysfs_config;

	sysfs_config->rear_afcal = of_property_read_bool(np, "rear_afcal");
	if (!sysfs_config->rear_afcal)
		probe_err("rear_afcal not use(%d)", sysfs_config->rear_afcal);

	sysfs_config->rear_dualcal = of_property_read_bool(np, "rear_dualcal");
	if (!sysfs_config->rear_dualcal)
		probe_err("rear_dualcal not use(%d)", sysfs_config->rear_dualcal);

	sysfs_config->rear2 = of_property_read_bool(np, "rear2");
	if (!sysfs_config->rear2)
		probe_err("rear2 not use(%d)", sysfs_config->rear2);

	sysfs_config->rear2_afcal = of_property_read_bool(np, "rear2_afcal");
	if (!sysfs_config->rear2_afcal)
		probe_err("rear2_afcal not use(%d)", sysfs_config->rear2_afcal);

	sysfs_config->rear2_moduleid = of_property_read_bool(np, "rear2_moduleid");
	if (!sysfs_config->rear2_moduleid)
		probe_err("rear2_moduleid not use(%d)", sysfs_config->rear2_moduleid);

	sysfs_config->rear2_tilt = of_property_read_bool(np, "rear2_tilt");
	if (!sysfs_config->rear2_tilt)
		probe_err("rear2_tilt not use(%d)", sysfs_config->rear2_tilt);

	sysfs_config->rear3 = of_property_read_bool(np, "rear3");
	if (!sysfs_config->rear3)
		probe_err("rear3 not use(%d)", sysfs_config->rear3);

	sysfs_config->rear3_afcal = of_property_read_bool(np, "rear3_afcal");
	if (!sysfs_config->rear3_afcal)
		probe_err("rear3_afcal not use(%d)", sysfs_config->rear3_afcal);

	sysfs_config->rear3_afhall = of_property_read_bool(np, "rear3_afhall");
	if (!sysfs_config->rear3_afhall)
		probe_err("rear3_afhall not use(%d)", sysfs_config->rear3_afhall);

	sysfs_config->rear3_moduleid = of_property_read_bool(np, "rear3_moduleid");
	if (!sysfs_config->rear3_moduleid)
		probe_err("rear3_moduleid not use(%d)", sysfs_config->rear3_moduleid);

	sysfs_config->rear3_tilt = of_property_read_bool(np, "rear3_tilt");
	if (!sysfs_config->rear3_tilt)
		probe_err("rear3_tilt not use(%d)", sysfs_config->rear3_tilt);

	sysfs_config->rear4 = of_property_read_bool(np, "rear4");
	if (!sysfs_config->rear4)
		probe_err("rear4 not use(%d)", sysfs_config->rear4);

	sysfs_config->rear4_afcal = of_property_read_bool(np, "rear4_afcal");
	if (!sysfs_config->rear4_afcal)
		probe_err("rear4_afcal not use(%d)", sysfs_config->rear4_afcal);

	sysfs_config->rear4_moduleid = of_property_read_bool(np, "rear4_moduleid");
	if (!sysfs_config->rear4_moduleid)
		probe_err("rear4_moduleid not use(%d)", sysfs_config->rear4_moduleid);

	sysfs_config->rear4_tilt = of_property_read_bool(np, "rear4_tilt");
	if (!sysfs_config->rear4_tilt)
		probe_err("rear4_tilt not use(%d)", sysfs_config->rear4_tilt);

	sysfs_config->rear_tof = of_property_read_bool(np, "rear_tof");
	if (!sysfs_config->rear_tof)
		probe_err("rear_tof not use(%d)", sysfs_config->rear_tof);

	sysfs_config->rear_tof_cal = of_property_read_bool(np, "rear_tof_cal");
	if (!sysfs_config->rear_tof_cal)
		probe_err("rear_tof_cal not use(%d)", sysfs_config->rear_tof_cal);

	sysfs_config->rear_tof_dualcal = of_property_read_bool(np, "rear_tof_dualcal");
	if (!sysfs_config->rear_tof_dualcal)
		probe_err("rear_tof_dualcal not use(%d)", sysfs_config->rear_tof_dualcal);

	sysfs_config->rear_tof_moduleid = of_property_read_bool(np, "rear_tof_moduleid");
	if (!sysfs_config->rear_tof_moduleid)
		probe_err("rear_tof_moduleid not use(%d)", sysfs_config->rear_tof_moduleid);

	sysfs_config->rear_tof_tilt = of_property_read_bool(np, "rear_tof_tilt");
	if (!sysfs_config->rear_tof_tilt)
		probe_err("rear_tof_tilt not use(%d)", sysfs_config->rear_tof_tilt);

	sysfs_config->rear2_tof_tilt = of_property_read_bool(np, "rear2_tof_tilt");
	if (!sysfs_config->rear2_tof_tilt)
		probe_err("rear2_tof_tilt not use(%d)", sysfs_config->rear2_tof_tilt);

	sysfs_config->front_afcal = of_property_read_bool(np, "front_afcal");
	if (!sysfs_config->front_afcal)
		probe_err("front_afcal not use(%d)", sysfs_config->front_afcal);

	sysfs_config->front_dualcal = of_property_read_bool(np, "front_dualcal");
	if (!sysfs_config->front_dualcal)
		probe_err("front_dualcal not use(%d)", sysfs_config->front_dualcal);

	sysfs_config->front_fixed_focus = of_property_read_bool(np, "front_fixed_focus");
	if (!sysfs_config->front_fixed_focus)
		probe_err("front_fixed_focus not use(%d)", sysfs_config->front_fixed_focus);

	sysfs_config->front2 = of_property_read_bool(np, "front2");
	if (!sysfs_config->front2)
		probe_err("front2 not use(%d)", sysfs_config->front2);

	sysfs_config->front2_moduleid = of_property_read_bool(np, "front2_moduleid");
	if (!sysfs_config->front2_moduleid)
		probe_err("front2_moduleid not use(%d)", sysfs_config->front2_moduleid);

	sysfs_config->front2_tilt = of_property_read_bool(np, "front2_tilt");
	if (!sysfs_config->front2_tilt)
		probe_err("front2_tilt not use(%d)", sysfs_config->front2_tilt);

	sysfs_config->front_tof = of_property_read_bool(np, "front_tof");
	if (!sysfs_config->front_tof)
		probe_err("front_tof not use(%d)", sysfs_config->front_tof);

	sysfs_config->front_tof_cal = of_property_read_bool(np, "front_tof_cal");
	if (!sysfs_config->front_tof_cal)
		probe_err("front_tof_cal not use(%d)", sysfs_config->front_tof_cal);

	sysfs_config->front_tof_moduleid = of_property_read_bool(np, "front_tof_moduleid");
	if (!sysfs_config->front_tof_moduleid)
		probe_err("front_tof_moduleid not use(%d)", sysfs_config->front_tof_moduleid);

	sysfs_config->front_tof_tilt = of_property_read_bool(np, "front_tof_tilt");
	if (!sysfs_config->front_tof_tilt)
		probe_err("front_tof_tilt not use(%d)", sysfs_config->front_tof_tilt);

	return 0;
}

int is_vendor_dt(struct device_node *np, struct is_vendor_private *vendor_priv)
{
	int ret = 0, i;
	struct device_node *si_np = NULL;
	struct device_node *sn_np = NULL;
	struct device_node *cam_info_np  = NULL;
	struct device_node *sysfs_np = NULL;
	char caminfo_index_str[15];
	char *dev_id;

	si_np = of_get_child_by_name(np, "sensor_id");
	if (si_np) {
		dev_id = __getname();
		for (i = 0; i < SENSOR_POSITION_MAX; i++) {
			snprintf(dev_id, PATH_MAX, "%d", i);
			if (of_property_read_u32(si_np, dev_id, &vendor_priv->sensor_id[i]))
				probe_warn("sensor_id[%d] read is skipped", i);
		}
		__putname(dev_id);
	}

	sn_np = of_get_child_by_name(np, "sensor_name");
	if (sn_np) {
		dev_id = __getname();
		for (i = 0; i < SENSOR_POSITION_MAX; i++) {
			snprintf(dev_id, PATH_MAX, "%d", i);
			if (of_property_read_string(sn_np, dev_id, &vendor_priv->sensor_name[i]))
				probe_warn("sensor_name[%d] read is skipped", i);
		}
		__putname(dev_id);
	}

	if (of_property_read_u32(np, "ois_sensor_index", &vendor_priv->ois_sensor_index))
		probe_info("no ois_sensor_index\n");

	if (of_property_read_u32(np, "mcu_sensor_index", &vendor_priv->mcu_sensor_index))
		probe_info("no mcu_sensor_index\n");

	if (of_property_read_u32(np, "aperture_sensor_index", &vendor_priv->aperture_sensor_index))
		probe_info("no aperture_sensor_index\n");

	vendor_priv->check_sensor_vendor = of_property_read_bool(np, "check_sensor_vendor");
	if (!vendor_priv->check_sensor_vendor)
		probe_info("check_sensor_vendor not use(%d)\n", vendor_priv->check_sensor_vendor);

	vendor_priv->use_module_check = of_property_read_bool(np, "use_module_check");
	if (!vendor_priv->use_module_check)
		probe_err("use_module_check not use(%d)", vendor_priv->use_module_check);

	if (of_property_read_u32(np, "eeprom_on_delay", &vendor_priv->eeprom_on_delay)) {
		vendor_priv->eeprom_on_delay = 20;
		probe_info("no eeprom_on_delay (eeprom_on_delay : %d)\n", vendor_priv->eeprom_on_delay);
	}

	ret = of_property_read_u32(np, "max_camera_num", &vendor_priv->max_camera_num);
	if (ret) {
		err("max_camera_num read is fail(%d)", ret);
		vendor_priv->max_camera_num = 0;
	}

	for (i = 0; i < vendor_priv->max_camera_num; i++) {
		sprintf(caminfo_index_str, "%s%d", "camera_info", i);

		cam_info_np = of_find_node_by_name(np, caminfo_index_str);
		if (!cam_info_np) {
			info("[%s] cannot find %s dt node\n", __func__, caminfo_index_str);
			continue;
		}
		parse_sysfs_caminfo(cam_info_np, vendor_priv->cam_infos, i);
	}

	ret = of_property_read_u32(np, "max_supported_camera", &vendor_priv->max_supported_camera);
	if (ret)
		probe_err("supported_cameraId read is fail(%d)", ret);

	ret = of_property_read_u32_array(np, "supported_cameraId",
		vendor_priv->supported_camera_ids, vendor_priv->max_supported_camera);
	if (ret)
		probe_err("supported_cameraId read is fail(%d)", ret);

	if (of_property_read_u32(np, "is_vendor_sensor_count", &vendor_priv->is_vendor_sensor_count)) {
		probe_info("no is_vendor_sensor_count\n");
		vendor_priv->is_vendor_sensor_count = IS_SENSOR_COUNT;
	}


	sysfs_np = of_get_child_by_name(np, "sysfs");
	if (sysfs_np) {
		ret = is_sysfs_dt(sysfs_np, vendor_priv);
		if (ret)
			probe_err("is_sysfs_dt is fail(%d)", ret);
	}

	return ret;
}

int is_vendor_probe(struct is_vendor *vendor)
{
	int i;
	int ret = 0;
	struct is_core *core;
	struct device_node *vendor_np = NULL;
	struct is_vendor_private *vendor_priv;

	BUG_ON(!vendor);

	core = container_of(vendor, struct is_core, vendor);
	snprintf(vendor->fw_path, sizeof(vendor->fw_path), "%s", IS_FW_SDCARD);
	snprintf(vendor->request_fw_path, sizeof(vendor->request_fw_path), "%s", IS_FW);

	vendor_priv = devm_kzalloc(&core->pdev->dev,
			sizeof(struct is_vendor_private), GFP_KERNEL);
	if (!vendor_priv) {
		probe_err("failed to allocate is_vendor_private");
		return -ENOMEM;
	}

	/* init mutex for rom read */
	mutex_init(&vendor_priv->rom_lock);

#ifdef USE_TOF_AF
	/* TOF AF data mutex */
	mutex_init(&vendor_priv->tof_af_lock);
#endif
	mutex_init(&g_efs_mutex);
	mutex_init(&g_shaking_mutex);

	vendor_np = of_get_child_by_name(core->pdev->dev.of_node, "vendor");
	if (vendor_np) {
		ret = is_vendor_dt(vendor_np, vendor_priv);
		if (ret)
			probe_err("is_vendor_dt is fail(%d)", ret);
	}

	vendor_priv->suspend_resume_disable = false;
	vendor_priv->need_cold_reset = false;
	vendor_priv->tof_info.TOFExposure = 0;
	vendor_priv->tof_info.TOFFps = 0;

	for (i = 0; i < ROM_ID_MAX; i++) {
		vendor_priv->rom[i].client = NULL;
		vendor_priv->rom[i].valid = false;
	}

	vendor->private_data = vendor_priv;

	if (is_create_sysfs()) {
		probe_err("is_create_sysfs is failed");
		ret = -EINVAL;
		goto p_err;
	}

	is_load_ctrl_init();
#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
	if (!sensor_pwr_ctrl_wq) {
		sensor_pwr_ctrl_wq = create_singlethread_workqueue("sensor_pwr_ctrl");
	}
#endif

p_err:
	return ret;
}

#ifdef MODULE
int is_vendor_driver_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(is_get_rom_driver());
	if (ret) {
		err("sensor_rom_driver register failed(%d)", ret);
		return ret;
	}

#ifdef CONFIG_CAMERA_USE_INTERNAL_MCU
	ret = platform_driver_register(get_internal_ois_platform_driver());
	if (ret) {
		err("sensor_ois_mcu_platform_driver register failed(%d)", ret);
		return ret;
	}
#elif defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
	ret = i2c_add_driver(get_external_ois_i2c_driver());
	if (ret) {
		err("sensor_mcu_driver register failed(%d)", ret);
		return ret;
	}
#elif defined(CONFIG_CAMERA_USE_AOIS)
	ret = platform_driver_register(get_aois_platform_driver());
	if (ret) {
		err("sensor_aois_platform_driver register failed(%d)", ret);
		return ret;
	}
#endif

	return ret;
}

int is_vendor_driver_exit(void)
{
	int ret = 0;

	i2c_del_driver(is_get_rom_driver());
#ifdef CONFIG_CAMERA_USE_INTERNAL_MCU
	platform_driver_unregister(get_internal_ois_platform_driver());
#elif defined(CONFIG_CAMERA_USE_EXTERNAL_MCU)
	i2c_del_driver(get_external_ois_i2c_driver());
#elif defined(CONFIG_CAMERA_USE_AOIS)
	platform_driver_unregister(get_aois_platform_driver());
#endif

	return ret;
}
#endif

int is_vendor_rom_parse_dt_specific(struct device_node *dnode, int rom_id, struct is_vendor_rom *rom_info)
{

	const u32 *rom_af_cal_addr_spec;
	const u32 *rom_sensor2_af_cal_addr_spec;
	const u32 *crc_check_list_spec;
	const u32 *dual_crc_check_list_spec;
	const u32 *rom_dualcal_slave0_tilt_list_spec;
	const u32 *rom_dualcal_slave1_tilt_list_spec;
	const u32 *rom_dualcal_slave2_tilt_list_spec; /* wide(rear) - tof */
	const u32 *rom_dualcal_slave3_tilt_list_spec; /* ultra wide(rear2) - tof */
	const u32 *rom_ois_list_spec;
	const u32 *tof_cal_size_list_spec;
	const u32 *tof_cal_valid_list_spec;

	int ret = 0;
#ifdef IS_DEVICE_ROM_DEBUG
	int i;
#endif
	u32 temp;
	char *pprop;
	const u32 *rom_pdxtc_cal_data_addr_list_spec;
	const u32 *rom_gcc_cal_data_addr_list_spec;
	const u32 *rom_xtc_cal_data_addr_list_spec;

	struct is_vendor_standard_cal *standard_cal_data;


	crc_check_list_spec = of_get_property(dnode, "crc_check_list", &rom_info->crc_check_list_len);
	if (crc_check_list_spec) {
		rom_info->crc_check_list_len /= (unsigned int)sizeof(*crc_check_list_spec);

		BUG_ON(rom_info->crc_check_list_len > IS_ROM_CRC_MAX_LIST);

		ret = of_property_read_u32_array(dnode, "crc_check_list",
			rom_info->crc_check_list, rom_info->crc_check_list_len);
		if (ret)
			err("Not exist crc_check_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("crc_check_list :");
			for (i = 0; i < rom_info->crc_check_list_len; i++)
				info(" %d ", rom_info->crc_check_list[i]);
			info("\n");
		}
#endif
	}

	dual_crc_check_list_spec = of_get_property(dnode, "dual_crc_check_list", &rom_info->dual_crc_check_list_len);
	if (dual_crc_check_list_spec) {
		rom_info->dual_crc_check_list_len /= (unsigned int)sizeof(*dual_crc_check_list_spec);

		ret = of_property_read_u32_array(dnode, "dual_crc_check_list",
			rom_info->dual_crc_check_list, rom_info->dual_crc_check_list_len);
		if (ret)
			info("Not exist dual_crc_check_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("dual_crc_check_list :");
			for (i = 0; i < rom_info->dual_crc_check_list_len; i++)
				info(" %d ", rom_info->dual_crc_check_list[i]);
			info("\n");
		}
#endif
	}

	rom_af_cal_addr_spec = of_get_property(dnode, "rom_af_cal_addr", &rom_info->rom_af_cal_addr_len);
	if (rom_af_cal_addr_spec) {
		rom_info->rom_af_cal_addr_len /= (unsigned int)sizeof(*rom_af_cal_addr_spec);

		BUG_ON(rom_info->rom_af_cal_addr_len > AF_CAL_MAX);

		ret = of_property_read_u32_array(dnode, "rom_af_cal_addr",
			rom_info->rom_af_cal_addr, rom_info->rom_af_cal_addr_len);
	}

	rom_sensor2_af_cal_addr_spec = of_get_property(dnode, "rom_sensor2_af_cal_addr", &rom_info->rom_sensor2_af_cal_addr_len);
	if (rom_sensor2_af_cal_addr_spec) {
		rom_info->rom_sensor2_af_cal_addr_len /= (unsigned int)sizeof(*rom_sensor2_af_cal_addr_spec);

		BUG_ON(rom_info->rom_sensor2_af_cal_addr_len > AF_CAL_MAX);

		ret = of_property_read_u32_array(dnode, "rom_sensor2_af_cal_addr",
			rom_info->rom_sensor2_af_cal_addr, rom_info->rom_sensor2_af_cal_addr_len);
		if (ret)
			info("Not exist rom_sensor2_af_cal_addr in DT(%d)", ret);
	}

	rom_dualcal_slave0_tilt_list_spec
		= of_get_property(dnode, "rom_dualcal_slave0_tilt_list", &rom_info->rom_dualcal_slave0_tilt_list_len);
	if (rom_dualcal_slave0_tilt_list_spec) {
		rom_info->rom_dualcal_slave0_tilt_list_len /= (unsigned int)sizeof(*rom_dualcal_slave0_tilt_list_spec);

		ret = of_property_read_u32_array(dnode, "rom_dualcal_slave0_tilt_list",
			rom_info->rom_dualcal_slave0_tilt_list, rom_info->rom_dualcal_slave0_tilt_list_len);
		if (ret)
			info("Not exist rom_dualcal_slave0_tilt_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_dualcal_slave0_tilt_list :");
			for (i = 0; i < rom_info->rom_dualcal_slave0_tilt_list_len; i++)
				info(" %d ", rom_info->rom_dualcal_slave0_tilt_list[i]);
			info("\n");
		}
#endif
	}

	rom_dualcal_slave1_tilt_list_spec
		= of_get_property(dnode, "rom_dualcal_slave1_tilt_list", &rom_info->rom_dualcal_slave1_tilt_list_len);
	if (rom_dualcal_slave1_tilt_list_spec) {
		rom_info->rom_dualcal_slave1_tilt_list_len /= (unsigned int)sizeof(*rom_dualcal_slave1_tilt_list_spec);

		ret = of_property_read_u32_array(dnode, "rom_dualcal_slave1_tilt_list",
			rom_info->rom_dualcal_slave1_tilt_list, rom_info->rom_dualcal_slave1_tilt_list_len);
		if (ret)
			info("Not exist rom_dualcal_slave1_tilt_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_dualcal_slave1_tilt_list :");
			for (i = 0; i < rom_info->rom_dualcal_slave1_tilt_list_len; i++)
				info(" %d ", rom_info->rom_dualcal_slave1_tilt_list[i]);
			info("\n");
		}
#endif
	}

	rom_dualcal_slave2_tilt_list_spec
		= of_get_property(dnode, "rom_dualcal_slave2_tilt_list", &rom_info->rom_dualcal_slave2_tilt_list_len);
	if (rom_dualcal_slave2_tilt_list_spec) {
		rom_info->rom_dualcal_slave2_tilt_list_len /= (unsigned int)sizeof(*rom_dualcal_slave2_tilt_list_spec);

		ret = of_property_read_u32_array(dnode, "rom_dualcal_slave2_tilt_list",
			rom_info->rom_dualcal_slave2_tilt_list, rom_info->rom_dualcal_slave2_tilt_list_len);
		if (ret)
			info("Not exist rom_dualcal_slave2_tilt_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_dualcal_slave2_tilt_list :");
			for (i = 0; i < rom_info->rom_dualcal_slave2_tilt_list_len; i++)
				info(" %d ", rom_info->rom_dualcal_slave2_tilt_list[i]);
			info("\n");
		}
#endif
	}

	rom_dualcal_slave3_tilt_list_spec
		= of_get_property(dnode, "rom_dualcal_slave3_tilt_list", &rom_info->rom_dualcal_slave3_tilt_list_len);
	if (rom_dualcal_slave3_tilt_list_spec) {
		rom_info->rom_dualcal_slave3_tilt_list_len /= (unsigned int)sizeof(*rom_dualcal_slave3_tilt_list_spec);

		ret = of_property_read_u32_array(dnode, "rom_dualcal_slave3_tilt_list",
			rom_info->rom_dualcal_slave3_tilt_list, rom_info->rom_dualcal_slave3_tilt_list_len);
		if (ret)
			info("Not exist rom_dualcal_slave3_tilt_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_dualcal_slave3_tilt_list :");
			for (i = 0; i < rom_info->rom_dualcal_slave3_tilt_list_len; i++)
				info(" %d ", rom_info->rom_dualcal_slave3_tilt_list[i]);
			info("\n");
		}
#endif
	}

	rom_ois_list_spec = of_get_property(dnode, "rom_ois_list", &rom_info->rom_ois_list_len);
	if (rom_ois_list_spec) {
		rom_info->rom_ois_list_len /= (unsigned int)sizeof(*rom_ois_list_spec);

		ret = of_property_read_u32_array(dnode, "rom_ois_list",
			rom_info->rom_ois_list, rom_info->rom_ois_list_len);
		if (ret)
			info("Not exist rom_ois_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_ois_list :");
			for (i = 0; i < rom_info->rom_ois_list_len; i++)
				info(" %d ", rom_info->rom_ois_list[i]);
			info("\n");
		}
#endif
	}


	DT_READ_U32_DEFAULT(dnode, "rom_awb_master_addr", rom_info->rom_awb_master_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_awb_module_addr", rom_info->rom_awb_module_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_sensor2_awb_master_addr", rom_info->rom_sensor2_awb_master_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_sensor2_awb_module_addr", rom_info->rom_sensor2_awb_module_addr, -1);

	DT_READ_U32_DEFAULT(dnode, "rom_header_mtf_data_addr", rom_info->rom_header_mtf_data_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_header_sensor2_mtf_data_addr", rom_info->rom_header_sensor2_mtf_data_addr, -1);

	DT_READ_U32_DEFAULT(dnode, "rom_paf_cal_start_addr", rom_info->rom_paf_cal_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_sensor2_paf_cal_start_addr", rom_info->rom_sensor2_paf_cal_start_addr, -1);

	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave0_start_addr", rom_info->rom_dualcal_slave0_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave0_size", rom_info->rom_dualcal_slave0_size, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave1_start_addr", rom_info->rom_dualcal_slave1_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave1_size", rom_info->rom_dualcal_slave1_size, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave2_start_addr", rom_info->rom_dualcal_slave2_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave2_size", rom_info->rom_dualcal_slave2_size, -1);

	DT_READ_U32_DEFAULT(dnode, "rom_pdxtc_cal_data_start_addr", rom_info->rom_pdxtc_cal_data_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_pdxtc_cal_data_0_size", rom_info->rom_pdxtc_cal_data_0_size, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_pdxtc_cal_data_1_size", rom_info->rom_pdxtc_cal_data_1_size, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_spdc_cal_data_start_addr", rom_info->rom_spdc_cal_data_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_spdc_cal_data_size", rom_info->rom_spdc_cal_data_size, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_xtc_cal_data_start_addr", rom_info->rom_xtc_cal_data_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_xtc_cal_data_size", rom_info->rom_xtc_cal_data_size, -1);


	rom_info->sec2lsi_conv_done = false;
	if (rom_info->use_standard_cal) {
		standard_cal_data = &(rom_info->standard_cal_data);
		DT_READ_U32_DEFAULT(dnode, "rom_standard_cal_start_addr",
							standard_cal_data->rom_standard_cal_start_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_standard_cal_end_addr",
							standard_cal_data->rom_standard_cal_end_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_standard_cal_module_crc_addr",
							standard_cal_data->rom_standard_cal_module_crc_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_standard_cal_module_checksum_len",
							standard_cal_data->rom_standard_cal_module_checksum_len, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_header_standard_cal_end_addr",
							standard_cal_data->rom_header_standard_cal_end_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_standard_cal_sec2lsi_end_addr",
							standard_cal_data->rom_standard_cal_sec2lsi_end_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_awb_start_addr", standard_cal_data->rom_awb_start_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_awb_end_addr", standard_cal_data->rom_awb_end_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_shading_start_addr", standard_cal_data->rom_shading_start_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_shading_end_addr", standard_cal_data->rom_shading_end_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_factory_start_addr", standard_cal_data->rom_factory_start_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_factory_end_addr", standard_cal_data->rom_factory_end_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_header_main_shading_end_addr",
							standard_cal_data->rom_header_main_shading_end_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_awb_sec2lsi_start_addr",
							standard_cal_data->rom_awb_sec2lsi_start_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_awb_sec2lsi_end_addr", standard_cal_data->rom_awb_sec2lsi_end_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_awb_sec2lsi_checksum_addr",
							standard_cal_data->rom_awb_sec2lsi_checksum_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_awb_sec2lsi_checksum_len",
							standard_cal_data->rom_awb_sec2lsi_checksum_len, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_shading_sec2lsi_start_addr",
							standard_cal_data->rom_shading_sec2lsi_start_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_shading_sec2lsi_end_addr",
							standard_cal_data->rom_shading_sec2lsi_end_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_shading_sec2lsi_checksum_addr",
							standard_cal_data->rom_shading_sec2lsi_checksum_addr, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_shading_sec2lsi_checksum_len",
							standard_cal_data->rom_shading_sec2lsi_checksum_len, -1);
		DT_READ_U32_DEFAULT(dnode, "rom_factory_sec2lsi_start_addr",
							standard_cal_data->rom_factory_sec2lsi_start_addr, -1);
	}

	rom_pdxtc_cal_data_addr_list_spec = of_get_property(dnode, "rom_pdxtc_cal_data_addr_list", &rom_info->rom_pdxtc_cal_data_addr_list_len);
	if (rom_pdxtc_cal_data_addr_list_spec) {
		rom_info->rom_pdxtc_cal_data_addr_list_len /= (unsigned int)sizeof(*rom_pdxtc_cal_data_addr_list_spec);

		BUG_ON(rom_info->rom_pdxtc_cal_data_addr_list_len > CROSSTALK_CAL_MAX);

		ret = of_property_read_u32_array(dnode, "rom_pdxtc_cal_data_addr_list", rom_info->rom_pdxtc_cal_data_addr_list, rom_info->rom_pdxtc_cal_data_addr_list_len);
		if (ret)
			err("Not exist rom_pdxtc_cal_data_addr_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_pdxtc_cal_data_addr_list :");
			for (i = 0; i < rom_info->rom_pdxtc_cal_data_addr_list_len; i++)
				info(" %d ", rom_info->rom_pdxtc_cal_data_addr_list[i]);
			info("\n");
		}
#endif
	}
	rom_info->rom_pdxtc_cal_endian_check = of_property_read_bool(dnode, "rom_pdxtc_cal_endian_check");

	rom_gcc_cal_data_addr_list_spec = of_get_property(dnode, "rom_gcc_cal_data_addr_list", &rom_info->rom_gcc_cal_data_addr_list_len);
	if (rom_gcc_cal_data_addr_list_spec) {
		rom_info->rom_gcc_cal_data_addr_list_len /= (unsigned int)sizeof(*rom_gcc_cal_data_addr_list_spec);

		BUG_ON(rom_info->rom_gcc_cal_data_addr_list_len > CROSSTALK_CAL_MAX);

		ret = of_property_read_u32_array(dnode, "rom_gcc_cal_data_addr_list", rom_info->rom_gcc_cal_data_addr_list, rom_info->rom_gcc_cal_data_addr_list_len);
		if (ret)
			err("Not exist rom_gcc_cal_data_addr_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_gcc_cal_data_addr_list :");
			for (i = 0; i < rom_info->rom_gcc_cal_data_addr_list_len; i++)
				info(" %d ", rom_info->rom_gcc_cal_data_addr_list[i]);
			info("\n");
		}
#endif
	}
	rom_info->rom_gcc_cal_endian_check = of_property_read_bool(dnode, "rom_gcc_cal_endian_check");

	rom_xtc_cal_data_addr_list_spec = of_get_property(dnode, "rom_xtc_cal_data_addr_list", &rom_info->rom_xtc_cal_data_addr_list_len);
	if (rom_xtc_cal_data_addr_list_spec) {
		rom_info->rom_xtc_cal_data_addr_list_len /= (unsigned int)sizeof(*rom_xtc_cal_data_addr_list_spec);

		BUG_ON(rom_info->rom_xtc_cal_data_addr_list_len > CROSSTALK_CAL_MAX);

		ret = of_property_read_u32_array(dnode, "rom_xtc_cal_data_addr_list", rom_info->rom_xtc_cal_data_addr_list, rom_info->rom_xtc_cal_data_addr_list_len);
		if (ret)
			err("Not exist rom_xtc_cal_data_addr_list in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_xtc_cal_data_addr_list :");
			for (i = 0; i < rom_info->rom_xtc_cal_data_addr_list_len; i++)
				info(" %d ", rom_info->rom_xtc_cal_data_addr_list[i]);
			info("\n");
		}
#endif
	}
	rom_info->rom_xtc_cal_endian_check = of_property_read_bool(dnode, "rom_xtc_cal_endian_check");

	tof_cal_size_list_spec = of_get_property(dnode, "rom_tof_cal_size_addr", &rom_info->rom_tof_cal_size_addr_len);
	if (tof_cal_size_list_spec) {
		rom_info->rom_tof_cal_size_addr_len /= (unsigned int)sizeof(*tof_cal_size_list_spec);

		BUG_ON(rom_info->rom_tof_cal_size_addr_len > TOF_CAL_SIZE_MAX);

		ret = of_property_read_u32_array(dnode, "rom_tof_cal_size_addr", rom_info->rom_tof_cal_size_addr, rom_info->rom_tof_cal_size_addr_len);
		if (ret)
			err("Not exist rom_tof_cal_size_addr in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_tof_cal_size_addr :");
			for (i = 0; i < rom_info->rom_tof_cal_size_addr_len; i++)
				info(" %d ", rom_info->rom_tof_cal_size_addr[i]);
			info("\n");
		}
#endif
	}

	DT_READ_U32_DEFAULT(dnode, "rom_tof_cal_mode_id_addr", rom_info->rom_tof_cal_mode_id_addr, -1);

	tof_cal_valid_list_spec = of_get_property(dnode, "rom_tof_cal_validation_addr", &rom_info->rom_tof_cal_validation_addr_len);
	if (tof_cal_valid_list_spec) {
		rom_info->rom_tof_cal_validation_addr_len /= (unsigned int)sizeof(*tof_cal_valid_list_spec);
		ret = of_property_read_u32_array(dnode, "rom_tof_cal_validation_addr", rom_info->rom_tof_cal_validation_addr, rom_info->rom_tof_cal_validation_addr_len);
		if (ret)
			err("Not exist rom_tof_cal_validation_addr in DT(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("rom_tof_cal_validation_addr :");
			for (i = 0; i < rom_info->rom_tof_cal_validation_addr_len; i++)
				info(" %d ", rom_info->rom_tof_cal_validation_addr[i]);
			info("\n");
		}
#endif
	}

	DT_READ_U32_DEFAULT(dnode, "rom_tof_cal_start_addr", rom_info->rom_tof_cal_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_tof_cal_result_addr", rom_info->rom_tof_cal_result_addr, -1);

	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave1_cropshift_x_addr", rom_info->rom_dualcal_slave1_cropshift_x_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave1_cropshift_y_addr", rom_info->rom_dualcal_slave1_cropshift_y_addr, -1);

	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave1_oisshift_x_addr", rom_info->rom_dualcal_slave1_oisshift_x_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave1_oisshift_y_addr", rom_info->rom_dualcal_slave1_oisshift_y_addr, -1);

	DT_READ_U32_DEFAULT(dnode, "rom_dualcal_slave1_dummy_flag_addr", rom_info->rom_dualcal_slave1_dummy_flag_addr, -1);

	return 0;
}

int is_vendor_dualized_rom_parse_dt(struct device_node *dnode, int rom_id)
{
	struct is_vendor_rom *rom_info;
	struct device_node *rom_node = NULL;
	is_sec_get_rom_info(&rom_info, rom_id);

	rom_node = of_find_node_by_name(dnode, rom_info->sensor_name);

	if (!rom_node) {
		err("%s cannot find dualized node by sensor name %s", __func__, rom_info->sensor_name);
		return -EINVAL;
	}

	info("%s sensor node %s selected for EEPROM read", __func__, rom_info->sensor_name);
	return is_vendor_rom_parse_dt_specific(rom_node, rom_id, rom_info);
}
EXPORT_SYMBOL_GPL(is_vendor_dualized_rom_parse_dt);

int is_vendor_rom_parse_dt(struct device_node *dnode, int rom_id)
{
	const u32 *header_crc_check_list_spec;
	const char *node_string;
	struct is_vendor_rom *rom_info;
	int ret = 0;
	u32 temp;
	char *pprop;

	is_sec_get_rom_info(&rom_info, rom_id);

	memset(rom_info, 0, sizeof(struct is_vendor_rom));

	ret = of_property_read_u32(dnode, "rom_type", &rom_info->rom_type);
	BUG_ON(ret);

	ret = of_property_read_u32(dnode, "rom_power_position", &rom_info->rom_power_position);
	BUG_ON(ret);

	ret = of_property_read_u32(dnode, "rom_size", &rom_info->rom_size);
	BUG_ON(ret);

	ret = of_property_read_u32(dnode, "cal_map_es_version", &rom_info->cal_map_es_version);
	if (ret)
		rom_info->cal_map_es_version = 0;

	ret = of_property_read_string(dnode, "camera_module_es_version", &node_string);
	if (ret)
		rom_info->camera_module_es_version = 'A';
	else
		rom_info->camera_module_es_version = node_string[0];

	rom_info->need_i2c_config = of_property_read_bool(dnode, "need_i2c_config");

	rom_info->skip_cal_loading = of_property_read_bool(dnode, "skip_cal_loading");
	rom_info->skip_crc_check = of_property_read_bool(dnode, "skip_crc_check");
	rom_info->skip_header_loading = of_property_read_bool(dnode, "skip_header_loading");
	rom_info->ignore_cal_crc_error = of_property_read_bool(dnode, "ignore_cal_crc_error");

	info("[rom%d] power_position:%d, rom_size:0x%X, skip_cal_loading:%d, calmap_es:%d, module_es:%c\n",
		rom_id, rom_info->rom_power_position, rom_info->rom_size, rom_info->skip_cal_loading,
		rom_info->cal_map_es_version, rom_info->camera_module_es_version);

	header_crc_check_list_spec = of_get_property(dnode, "header_crc_check_list",
		&rom_info->header_crc_check_list_len);
	if (header_crc_check_list_spec) {
		rom_info->header_crc_check_list_len /= (unsigned int)sizeof(*header_crc_check_list_spec);

		BUG_ON(rom_info->header_crc_check_list_len > IS_ROM_CRC_MAX_LIST);

		ret = of_property_read_u32_array(dnode, "header_crc_check_list",
			rom_info->header_crc_check_list, rom_info->header_crc_check_list_len);
		if (ret)
			err("header_crc_check_list read is fail(%d)", ret);
#ifdef IS_DEVICE_ROM_DEBUG
		else {
			info("header_crc_check_list :");
			for (i = 0; i < rom_info->header_crc_check_list_len; i++)
				info(" %d ", rom_info->header_crc_check_list[i]);
			info("\n");
		}
#endif
	}

	DT_READ_U32_DEFAULT(dnode, "rom_header_cal_data_start_addr", rom_info->rom_header_cal_data_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_header_version_start_addr", rom_info->rom_header_version_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_header_sensor2_version_start_addr", rom_info->rom_header_sensor2_version_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_header_cal_map_ver_start_addr", rom_info->rom_header_cal_map_ver_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_header_isp_setfile_ver_start_addr", rom_info->rom_header_isp_setfile_ver_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_header_project_name_start_addr", rom_info->rom_header_project_name_start_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_header_module_id_addr", rom_info->rom_header_module_id_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_header_sensor_id_addr", rom_info->rom_header_sensor_id_addr, -1);
	DT_READ_U32_DEFAULT(dnode, "rom_header_sensor2_id_addr", rom_info->rom_header_sensor2_id_addr, -1);

	rom_info->use_standard_cal = of_property_read_bool(dnode, "use_standard_cal");
	rom_info->check_dualize = of_property_read_bool(dnode, "check_dualize");
	if (rom_info->check_dualize){
		ret = of_property_read_u32(dnode, "sensor_name_addr", &rom_info->sensor_name_addr);
		BUG_ON(ret);
	} else
		is_vendor_rom_parse_dt_specific(dnode, rom_id, rom_info);

	return 0;
}
EXPORT_SYMBOL_GPL(is_vendor_rom_parse_dt);

bool is_vendor_wait_sensor_probe_done(struct is_core *core)
{
	int i = 0;
	bool ret = false;
	int retry_count = 20;
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	do {
		ret = false;
		for (i = 0; i < vendor_priv->is_vendor_sensor_count; i++) {
			if (!test_bit(IS_SENSOR_PROBE, &core->sensor[i].state)) {
				ret = true;
				break;
			}
		}

		if (i == vendor_priv->is_vendor_sensor_count && ret == false) {
			info("Retry count = %d\n", retry_count);
			break;
		}

		mdelay(100);
		if (retry_count > 0) {
			--retry_count;
		} else {
			err("Could not get sensor before start ois fw update routine.\n");
			break;
		}
	} while (ret);

	return ret;
}

void is_vendor_check_hw_init_running(void)
{
	int retry = 50;

	do {
		if (!is_hw_init_running) {
			break;
		}
		--retry;
		msleep(100);
	} while (retry > 0);

	if (retry <= 0) {
		err("HW init is not completed.");
	}

	return;
}

#if defined(USE_CAMERA_SENSOR_RETENTION)
void is_vendor_prepare_retention(struct is_core *core, int sensor_id, int position)
{
	struct is_device_sensor *device;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct is_cis *cis;
	int ret = 0;
	u32 scenario = SENSOR_SCENARIO_NORMAL;
	u32 i2c_channel;

#ifdef CONFIG_SEC_FACTORY
	struct is_vendor_rom *rom_info;
	int rom_id = 0;

	rom_id = is_vendor_get_rom_id_from_position(position);
	is_sec_get_rom_info(&rom_info, rom_id);
	if (rom_id == ROM_ID_NOTHING || !rom_info->read_done) {
		info("%s: skip prepare retention - sensor_id[%d] position[%d] rom_id[%d] read_done[%d]\n",
			__func__, sensor_id, position, rom_id, rom_info->read_done);
		return;
	}
#endif

	device = &core->sensor[sensor_id];

	info("%s: start %d %d\n", __func__, sensor_id, position);

	WARN_ON(!device);

	device->pdata->scenario = scenario;

	ret = is_search_sensor_module_with_position(device, position, &module);
	if (ret) {
		warn("%s is_search_sensor_module_with_position failed(%d)", __func__, ret);
		goto p_exit;
	}

	if (module->ext.use_retention_mode == SENSOR_RETENTION_UNSUPPORTED) {
		warn("%s module doesn't support retention", __func__);
		goto p_err;
	}

	/* Sensor power on */
	ret = module->pdata->gpio_cfg(module, scenario, GPIO_SCENARIO_ON);
	if (ret) {
		warn("gpio on is fail(%d)", ret);
		goto p_err;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	if (sensor_peri->subdev_cis) {
		i2c_channel = module->pdata->sensor_i2c_ch;
		if (i2c_channel < SENSOR_CONTROL_I2C_MAX) {
			sensor_peri->cis.ixc_lock = &core->ixc_lock[i2c_channel];
			info("%s[%d]enable cis i2c client. position = %d\n",
					__func__, __LINE__, position);
		} else {
			warn("%s: wrong cis i2c_channel(%d)", __func__, i2c_channel);
			goto p_err;
		}

		cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);
		ret = CALL_CISOPS(cis, cis_check_rev_on_init, sensor_peri->subdev_cis);
		if (ret) {
			warn("v4l2_subdev_call(cis_check_rev_on_init) is fail(%d)", ret);
			goto p_err;
		}

		ret = CALL_CISOPS(cis, cis_init, sensor_peri->subdev_cis);
		if (ret) {
			warn("v4l2_subdev_call(init) is fail(%d)", ret);
			goto p_err;
		}

		ret = CALL_CISOPS(cis, cis_set_global_setting, sensor_peri->subdev_cis);
		if (ret) {
			warn("v4l2_subdev_call(cis_set_global_setting) is fail(%d)", ret);
			goto p_err;
		}
	}

	ret = module->pdata->gpio_cfg(module, scenario, GPIO_SCENARIO_SENSOR_RETENTION_ON);
	if (ret)
		warn("gpio off (retention) is fail(%d)", ret);

	info("%s: end %d (retention)\n", __func__, ret);
	return;
p_err:
	ret = module->pdata->gpio_cfg(module, scenario, GPIO_SCENARIO_OFF);
	if (ret)
		err("gpio off is fail(%d)", ret);
p_exit:

	warn("%s: end %d\n", __func__, ret);
}
#endif

#ifdef USE_SHARE_I2C_CLIENT
int is_vendor_share_i2c_client(struct is_core *core, u32 source, u32 target)
{
	int ret = 0;
	struct is_device_sensor *device = NULL;
	struct is_device_sensor_peri *sensor_peri_source = NULL;
	struct is_device_sensor_peri *sensor_peri_target = NULL;
	struct is_cis *cis_source = NULL;
	struct is_cis *cis_target = NULL;
	int i;

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		sensor_peri_source = find_peri_by_cis_id(device, source);
		if (!sensor_peri_source) {
			info("Not device for sensor_peri_source");
			continue;
		}

		cis_source = &sensor_peri_source->cis;
		if (!cis_source) {
			info("cis_source is NULL");
			continue;
		}

		sensor_peri_target = find_peri_by_cis_id(device, target);
		if (!sensor_peri_target) {
			info("Not device for sensor_peri_target");
			continue;
		}

		cis_target = &sensor_peri_target->cis;
		if (!cis_target) {
			info("cis_target is NULL");
			continue;
		}

		cis_target->client = cis_source->client;
		sensor_peri_target->module->client = cis_target->client;
		info("i2c client copy done(source: %d target: %d)\n", source, target);
		break;
	}

	return ret;
}
#endif

bool is_vendor_is_possible_retention(int sensor_id, int position)
{
#ifdef CONFIG_SEC_FACTORY
	struct is_vendor_rom *rom_info;
	int rom_id = 0;

	rom_id = is_vendor_get_rom_id_from_position(position);
	is_sec_get_rom_info(&rom_info, rom_id);
	if (rom_info) {
		if (rom_id == ROM_ID_NOTHING || !rom_info->read_done) {
			info("%s: skip prepare retention - sensor_id[%d] position[%d] rom_id[%d] read_done[%d]\n",
				__func__, sensor_id, position, rom_id, rom_info->read_done);
			return false;
		}
	} else {
		info("%s: skip prepare retention - rom_info is NULL rom_id[%d]\n",
				__func__, rom_id);
		return false;
	}
#endif

	return true;
}

int is_vendor_hw_init(void)
{
	bool ret = false;
	struct device *dev  = NULL;
	struct is_core *core;
	struct is_vendor_private *vendor_priv = NULL;
	struct is_vendor_rom *rom_info;
	int rom_id;

	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;
	dev = &core->ischain[0].pdev->dev;

	info("[%s] hw init start\n", __func__);

	is_hw_init_running = true;

	ret = is_vendor_wait_sensor_probe_done(core);
	if (ret) {
		err("Do not init hw routine. Check sensor failed!\n");
		is_hw_init_running = false;
		return -EINVAL;
	} else {
		info("Start hw init. Check sensor success!\n");
	}

#ifdef USE_SHARE_I2C_CLIENT
	ret = is_vendor_share_i2c_client(core, SOURSE_SENSOR_NAME, TARGET_SENSOR_NAME);
	if (ret) {
		err("i2c client copy failed!\n");
		return -EINVAL;
	}
#endif

	for (rom_id = 0; rom_id < ROM_ID_MAX; rom_id++) {
		info("[%s] rom_id[%d] valid[%d]\n", __func__, rom_id, vendor_priv->rom[rom_id].valid);

		if (vendor_priv->rom[rom_id].valid == true) {
			is_sec_get_rom_info(&rom_info, rom_id);
			is_sec_rom_power_on(core, rom_info->rom_power_position);

			ret = is_sec_run_fw_sel(rom_id);
			if (ret) {
				err("is_sec_run_fw_sel for ROM_ID(%d) is fail(%d)", rom_id, ret);
			}

			is_sec_rom_power_off(core, rom_info->rom_power_position);
		}
	}
#ifdef USE_CAMERA_NOTIFY_WACOM
	is_eeprom_wacom_update_notifier();
#endif

#ifdef USE_CAMERA_SENSOR_RETENTION
	if (is_vendor_is_possible_retention(0, SENSOR_POSITION_REAR))
		is_sensor_prepare_retention(&core->sensor[0], SENSOR_POSITION_REAR);
#endif

#if IS_ENABLED(CONFIG_PABLO_HW_HELPER_V1) // quadra_bringup
	ret = is_load_bin_on_boot();
	if (ret) {
		err("is_load_bin_on_boot is fail(%d)", ret);
	}
#endif

#if defined(USE_CAMERA_ADAPTIVE_MIPI) && IS_ENABLED(CONFIG_DEV_RIL_BRIDGE)
	is_vendor_register_ril_notifier();
#endif
	is_hw_init_running = false;
	info("hw init done\n");
	return 0;
}

int is_vendor_fw_prepare(struct is_vendor *vendor, u32 position)
{
	int ret;
	int rom_id;

	rom_id = is_vendor_get_rom_id_from_position(position);
	ret = is_sec_run_fw_sel(rom_id);
	if (ret < 0) {
		err("is_sec_run_fw_sel is fail1(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_vendor_cal_load(struct is_vendor *vendor, void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vendor_cal_verify(struct is_vendor *vendor, void *module_data)
{
	int ret = 0;
	struct is_core *core;
	struct is_module_enum *module = module_data;
	char *cal_buf = NULL;
	int rom_id = ROM_ID_NOTHING;
	struct is_vendor_rom *rom_info;
	int position = module->position;

	core = container_of(vendor, struct is_core, vendor);

	rom_id = is_vendor_get_rom_id_from_position(position);

	is_sec_get_rom_info(&rom_info, rom_id);
	cal_buf = rom_info->buf;

	pr_info("%s rom_id : %d, position : %d\n", __func__, rom_id, position);

	if (!is_sec_check_rom_ver(core, rom_id)) {
		err("Camera : Did not load cal data.");
		return 0;
	}

	/* CRC check */
	if (!rom_info->crc_error) {
		info("CAL DATA : MAP ver : %c%c%c%c, Sensor id : 0x%x, Module Ver : %s",
			cal_buf[rom_info->rom_header_cal_map_ver_start_addr],
			cal_buf[rom_info->rom_header_cal_map_ver_start_addr + 1],
			cal_buf[rom_info->rom_header_cal_map_ver_start_addr + 2],
			cal_buf[rom_info->rom_header_cal_map_ver_start_addr + 3],
			cal_buf[rom_info->rom_header_sensor_id_addr],
			rom_info->header_ver);
	} else {
		err("position[%d] rom_id[%d]: CRC error", position, rom_id);

		if (!rom_info->ignore_cal_crc_error)
			ret = -EIO;
	}

	return ret;
}

int is_vendor_module_sel(struct is_vendor *vendor, void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vendor_module_del(struct is_vendor *vendor, void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vendor_setfile_sel(struct is_vendor *vendor,
	char *setfile_name,
	int position)
{
	int ret = 0;
	struct is_core *core;

	WARN_ON(!vendor);
	WARN_ON(!setfile_name);

	core = container_of(vendor, struct is_core, vendor);

	snprintf(vendor->setfile_path[position], sizeof(vendor->setfile_path[position]),
		"%s%s", IS_SETFILE_SDCARD_PATH, setfile_name);
	snprintf(vendor->request_setfile_path[position],
		sizeof(vendor->request_setfile_path[position]),
		"%s", setfile_name);

	return ret;
}

int is_vendor_get_position_from_rom_id(int rom_id, int *position)
{
	struct is_core *core;
	struct is_device_sensor *device;
	struct is_module_enum *module;
	int sindex = 0, mindex = 0, mmax = 0;

	core = is_get_is_core();
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	for (sindex = 0; sindex < IS_SENSOR_COUNT; sindex++) {
		device = &core->sensor[sindex];
		mmax = atomic_read(&device->module_count);
		for (mindex = 0; mindex < mmax; mindex++) {
			module = &device->module_enum[mindex];
			if (module->pdata->rom_id == rom_id) {
				*position = module->pdata->position;
				return 0;
			}
		}
	}

	return -EINVAL;
}

int is_vendor_get_module_from_position(int position, struct is_module_enum **module)
{
	struct is_core *core;
	int i = 0;

	*module = NULL;

	core = is_get_is_core();
	if (!core) {
		err("%s: core is NULL", __func__);
		return -EINVAL;
	}

	if (position >= SENSOR_POSITION_MAX) {
		err("%s: invalid position value.", __func__);
		return -EINVAL;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		is_search_sensor_module_with_position(&core->sensor[i], position, module);
		if (*module)
			break;
	}

	if (*module == NULL) {
		err("%s: Could not find sensor id.", __func__);
		return -EINVAL;
	}

	return 0;
}

bool is_vendor_check_camera_running(int position)
{
	struct is_module_enum *module;
	int ret = 0;

	ret = is_vendor_get_module_from_position(position, &module);

	if (module && !ret) {
		return test_bit(IS_MODULE_GPIO_ON, &module->state);
	}

	return false;
}

int is_vendor_get_rom_id_from_position(int position)
{
	struct is_module_enum *module;
	int ret = 0;

	ret = is_vendor_get_module_from_position(position, &module);

	if (module && !ret) {
		return module->pdata->rom_id;
	}

	return ROM_ID_NOTHING;
}
EXPORT_SYMBOL_GPL(is_vendor_get_rom_id_from_position);

void is_vendor_get_rom_info_from_position(int position, int *rom_id, int *rom_cal_index)
{
	struct is_module_enum *module;
	int ret = 0;

	ret = is_vendor_get_module_from_position(position, &module);

	if (module && !ret) {
		*rom_id = module->pdata->rom_id;
		*rom_cal_index = module->pdata->rom_cal_index;
	} else {
		*rom_id = ROM_ID_NOTHING;
		*rom_cal_index = 0;
	}
}

void is_vendor_get_rom_dualcal_info_from_position(int position,
	int *rom_dualcal_id, int *rom_dualcal_index)
{
	struct is_module_enum *module;
	int ret = 0;

	ret = is_vendor_get_module_from_position(position, &module);

	if (module && !ret) {
		*rom_dualcal_id = module->pdata->rom_dualcal_id;
		*rom_dualcal_index = module->pdata->rom_dualcal_index;
	} else {
		*rom_dualcal_id = ROM_ID_NOTHING;
		*rom_dualcal_index = 0;
	}
}

void is_vendor_get_use_dualcal_file_from_position(int position,
	bool *use_dualcal_from_file)
{
	struct is_module_enum *module;
	int ret = 0;

	ret = is_vendor_get_module_from_position(position, &module);

	if (module && !ret)
		*use_dualcal_from_file = module->pdata->use_dualcal_from_file;
	else
		*use_dualcal_from_file = false;
}

void is_vendor_get_dualcal_file_name_from_position(int position,
	char **dual_cal_file_name)
{
	struct is_module_enum *module;
	int ret = 0;

	ret = is_vendor_get_module_from_position(position, &module);

	if (module && !ret)
		*dual_cal_file_name = module->pdata->dual_cal_file_name;
	else
		*dual_cal_file_name = NULL;
}

int is_vendor_get_dualcal_param_from_file(struct device *dev, char **buf, int position, int *size)
{
	int ret = 0;
	struct is_binary bin;
	struct is_core *core;
	struct is_vendor *vendor;
	char *dual_cal_file_name;

	info("%s: E", __func__);

	core = is_get_is_core();
	vendor = &core->vendor;

	is_vendor_get_dualcal_file_name_from_position(position, &dual_cal_file_name);

	setup_binary_loader(&bin, 3, -EAGAIN, NULL, NULL);
	ret = request_binary(&bin, IS_FW_PATH, dual_cal_file_name, dev);

	if (ret) {
		err("filp_open(%s%s) fail(%d)!!\n", IS_FW_PATH, dual_cal_file_name, ret);
		goto p_err;
	}

	/* Cal data save */
	memcpy(*buf, (char *)bin.data, bin.size);
	*size = bin.size;
	info("%s dual cal data copy size:%ld bytes", __func__, bin.size);

p_err:
	release_binary(&bin);
	info("%s: X", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(is_vendor_get_dualcal_param_from_file);

#ifdef CAMERA_PARALLEL_RETENTION_SEQUENCE
void sensor_pwr_ctrl(struct work_struct *work)
{
	int ret = 0;
	struct exynos_platform_is_module *pdata;
	struct is_module_enum *g_module = NULL;
	struct is_core *core;

	core = is_get_is_core();
	if (!core) {
		err("core is NULL");
		return;
	}

	ret = is_preproc_g_module(&core->preproc, &g_module);
	if (ret) {
		err("is_sensor_g_module is fail(%d)", ret);
		return;
	}

	pdata = g_module->pdata;
	ret = pdata->gpio_cfg(g_module, SENSOR_SCENARIO_NORMAL,
		GPIO_SCENARIO_STANDBY_OFF_SENSOR);
	if (ret) {
		err("gpio_cfg(sensor) is fail(%d)", ret);
	}
}

static DECLARE_DELAYED_WORK(sensor_pwr_ctrl_work, sensor_pwr_ctrl);
#endif

int is_vendor_sensor_gpio_on_sel(struct is_vendor *vendor, u32 scenario, u32 *gpio_scenario,
	void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vendor_sensor_gpio_on(struct is_vendor *vendor, u32 scenario, u32 gpio_scenario
		, void *module_data)
{
	int ret = 0;

	return ret;
}

int is_vendor_sensor_gpio_off_sel(struct is_vendor *vendor, u32 scenario, u32 *gpio_scenario,
		void *module_data)
{
	int ret = 0;
	struct is_module_enum *module = module_data;
	struct sensor_open_extended *ext_info;
	struct is_core *core;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	FIMC_BUG(!sensor_peri);

	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	core = container_of(vendor, struct is_core, vendor);
	ext_info = &(((struct is_module_enum *)module)->ext);

#ifdef USE_CAMERA_SENSOR_RETENTION
	if ((ext_info->use_retention_mode == SENSOR_RETENTION_ACTIVATED)
		&& (scenario == SENSOR_SCENARIO_NORMAL)) {
		*gpio_scenario = GPIO_SCENARIO_SENSOR_RETENTION_ON;

		info("%s: use_retention_mode\n", __func__);
	}
#endif

	return ret;
}
PST_EXPORT_SYMBOL(is_vendor_sensor_gpio_off_sel);

int is_vendor_sensor_gpio_off(struct is_vendor *vendor, u32 scenario, u32 gpio_scenario
		, void *module_data)
{
	int ret = 0;

	return ret;
}

void is_vendor_sensor_suspend(struct platform_device *pdev)
{
#ifdef CONFIG_OIS_USE
	ois_factory_resource_clean();
#endif
}

#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
void is_vendor_print_hw_debug_info(struct is_core *core)
{
	struct cam_hw_param *ec_param;
	struct is_cam_info *cam_info;
	int position;
	int i = 0;
	char msg_buf[SZ_128];
	char log_buf[SZ_512];

	strcpy(log_buf, "hw_bigdata_debug_info:");

	/* rear */
	for (i = 0; i < 4; i++) {
		/* REAR = 0, REAR2 = 2, REAR3 = 4, REAR4 = 6*/
		is_get_cam_info_from_index(&cam_info, CAM_INFO_REAR + (2 * i));
		position = cam_info->position;

		is_sec_get_hw_param(&ec_param, position);
		if (!ec_param) {
			err("Fail to get ec_param (%d)", position);
			return;
		}

		sprintf(msg_buf, " [R%d:%c,%x,%x,%x,%x,%x,%x]",
			(i + 1), cam_info->valid ? 'Y':'N',
			ec_param->i2c_ois_err_cnt, ec_param->i2c_af_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt,
			ec_param->eeprom_i2c_err_cnt, ec_param->eeprom_crc_err_cnt);
		strcat(log_buf, msg_buf);
	}

	/* front */
	for (i = 0; i < 2; i++) {
		/* FRONT = 1, FRONT2 = 3 */
		is_get_cam_info_from_index(&cam_info, CAM_INFO_FRONT + (2 * i));
		position = cam_info->position;

		is_sec_get_hw_param(&ec_param, position);
		if (!ec_param) {
			err("Fail to get ec_param (%d)", position);
			return;
		}

		sprintf(msg_buf, " [F%d:%c,%x,%x,%x,%x,%x,%x]",
			(i + 1), cam_info->valid ? 'Y':'N',
			ec_param->i2c_ois_err_cnt, ec_param->i2c_af_err_cnt,
			ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt,
			ec_param->eeprom_i2c_err_cnt, ec_param->eeprom_crc_err_cnt);
		strcat(log_buf, msg_buf);
	}

	info("%s\n", log_buf);
}
#endif

void is_vendor_resource_clean(struct is_core *core)
{
#ifdef CONFIG_OIS_USE
	ois_factory_resource_clean();
#endif
#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
	is_vendor_print_hw_debug_info(core);
#endif
}

void is_vendor_sensor_s_input(struct is_vendor *vendor, u32 position)
{
	is_vendor_fw_prepare(vendor, position);

	return;
}

void is_vendor_update_meta(struct is_vendor *vendor, struct camera2_shot *shot)
{
    struct is_vendor_private *vendor_priv;
    vendor_priv = vendor->private_data;

    shot->ctl.aa.vendor_TOFInfo.exposureTime = vendor_priv->tof_info.TOFExposure;
    shot->ctl.aa.vendor_TOFInfo.fps = vendor_priv->tof_info.TOFFps;
}

bool is_vendor_check_remosaic_mode_change(struct is_frame *frame)
{
	u32 captureExtraInfo;
	bool processed_bayer;
	bool seamless_remosaic;

	captureExtraInfo = frame->shot->ctl.aa.vendor_captureExtraInfo;

	processed_bayer = captureExtraInfo & AA_CAPTURE_EXTRA_INFO_REMOSAIC_PROCESSED_BAYER;
	seamless_remosaic = captureExtraInfo & AA_CAPTURE_EXTRA_INFO_CROPPED_REMOSAIC_SEAMLESS;

	return (!processed_bayer && !seamless_remosaic
		&& CHK_MODECHANGE_SCN(frame->shot->ctl.aa.captureIntent));
}

int is_vendor_vidioc_s_ctrl(struct is_video_ctx *vctx, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video *video;
	struct is_device_ischain *device;
	struct is_core *core;
	struct is_vendor_private *vendor_priv;
	unsigned int value = 0;
	unsigned int captureIntent = 0;
	unsigned int captureCount = 0;
	struct is_group *head;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_DEVICE_ISCHAIN(vctx));
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!ctrl);

	device = GET_DEVICE_ISCHAIN(vctx);
	video = GET_VIDEO(vctx);

	FIMC_BUG(!device);
	FIMC_BUG(!device->pdev);

	core = (struct is_core *)platform_get_drvdata(device->pdev);
	vendor_priv = core->vendor.private_data;

	switch (ctrl->id) {
	case V4L2_CID_IS_INTENT:
		value = (unsigned int)ctrl->value;
		captureIntent = (value >> 16) & 0x0000FFFF;
		switch (captureIntent) {
		case AA_CAPTURE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_MFHDR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_LLHDR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_HANDHELD:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_LLHDR_VEHDR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_VENR_DYNAMIC_SHOT:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_LLS_FLASH:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_HANDHELD_FAST:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD_FAST:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SUPER_NIGHT_SHOT_TRIPOD_LE_FAST:
		case AA_CAPTURE_INTENT_STILL_CAPTURE_SHORT_REF_LLHDR_DYNAMIC_SHOT:
			captureCount = value & 0x0000FFFF;
			break;
		default:
			captureIntent = ctrl->value;
			captureCount = 0;
			break;
		}

		head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, device->group[GROUP_SLOT_3AA]);
		if (!head) {
			err("is_group is null. ctrl id(%d), captureIntent(%d)", ctrl->id, captureIntent);
			return -EINVAL;
		}

		head->intent_ctl.captureIntent = captureIntent;
		head->intent_ctl.vendor_captureCount = captureCount;

		ctrl->id = VENDOR_S_CTRL;

		if (captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI)
			head->remainCaptureIntentCount = 3 + CAPTURE_INTENT_RETRY_CNT;
		else
			head->remainCaptureIntentCount = 1 + CAPTURE_INTENT_RETRY_CNT;

		mvinfo("[VENDOR] s_ctrl intent(%d) count(%d) remainCaptureIntentCount(%d)\n",
			device, video, captureIntent, captureCount, head->remainCaptureIntentCount);
		break;
	case V4L2_CID_IS_CAPTURE_EXPOSURETIME:
		head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, device->group[GROUP_SLOT_3AA]);
		if (!head) {
			err("is_group is null. ctrl id(%d)", ctrl->id);
			return -EINVAL;
		}
		head->intent_ctl.vendor_captureExposureTime = ctrl->value;
		ctrl->id = VENDOR_S_CTRL;
		mvinfo("[VENDOR] s_ctrl vendor_captureExposureTime(%d)\n", device, video, ctrl->value);
		break;
	case V4L2_CID_IS_TRANSIENT_ACTION:
		head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, device->group[GROUP_SLOT_3AA]);
		if (!head) {
			err("is_group is null. ctrl id(%d)", ctrl->id);
			return -EINVAL;
		}
		head->intent_ctl.vendor_transientAction = ctrl->value;
		head->remainIntentCount = INTENT_RETRY_CNT;
		ctrl->id = VENDOR_S_CTRL;
		mvinfo("[VENDOR] s_ctrl transient action(%d)\n", device, video, ctrl->value);
		break;
	case V4L2_CID_IS_REMOSAIC_CROP_ZOOM_RATIO:
		head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, device->group[GROUP_SLOT_3AA]);
		if (!head) {
			err("is_group is null. ctrl id(%d)", ctrl->id);
			return -EINVAL;
		}
		if (device->sensor) {
			head->intent_ctl.vendor_remosaicCropZoomRatio = ctrl->value;
			head->remainRemosaicCropIntentCount = (INTENT_RETRY_CNT * 3) + 1;

			mvinfo("[VENDOR] s_ctrl remosaic crop zoom ratio(%d) fps(%d) remainRemosaicCropIntentCount(%d)\n",
				device, video, ctrl->value, device->sensor->image.framerate,
				head->remainRemosaicCropIntentCount);
		}
		ctrl->id = VENDOR_S_CTRL;
		break;
	case V4L2_CID_IS_FORCE_FLASH_MODE:
		if (device->sensor != NULL) {
			struct v4l2_subdev *subdev_flash;

			subdev_flash = device->sensor->subdev_flash;

			if (subdev_flash != NULL) {
				struct is_flash *flash = NULL;

				flash = (struct is_flash *)v4l2_get_subdevdata(subdev_flash);
				FIMC_BUG(!flash);

				mvinfo("[VENDOR] force flash mode\n", device, video);

				ctrl->id = V4L2_CID_FLASH_SET_FIRE;
				if (ctrl->value == CAM2_FLASH_MODE_OFF) {
					ctrl->value = 0; /* intensity */
					flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
					flash->flash_data.flash_fired = false;
					ret = v4l2_subdev_call(subdev_flash, core, ioctl, SENSOR_IOCTL_FLS_S_CTRL, ctrl);
				}
			}
		}
		break;
	case V4L2_CID_SENSOR_SET_CAL_POS:
		vendor_priv->cal_sensor_pos = (int)ctrl->value;
		mvinfo("[VENDOR] s_ctrl sensor position(%d) for loading cal data\n", device, video,
			(int)ctrl->value);
		break;
#ifdef USE_CAMERA_SENSOR_RETENTION
	case V4L2_CID_IS_PREVIEW_STATE:
		ctrl->id = VENDOR_S_CTRL;
		break;
#endif
	}
	return ret;
}

int is_vendor_vidioc_g_ctrl(struct is_video_ctx *vctx,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	int rom_id;
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;
	struct is_vendor_rom *rom_info = NULL;

	switch (ctrl->id) {
	case V4L2_CID_SENSOR_GET_CAL_SIZE:
		rom_id = is_vendor_get_rom_id_from_position(vendor_priv->cal_sensor_pos);
		is_sec_get_rom_info(&rom_info, rom_id);
		if (!rom_info) {
			err("fail to get rom_info for rom_id(%d)", rom_id);
			ctrl->value = 0;
			goto p_err;
		}
		ctrl->value = rom_info->rom_size;
		break;
	default:
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

int is_vendor_vidioc_g_ext_ctrl(struct is_video_ctx *vctx,
	struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	int cal_size = 0;
	int rom_id;
	char *cal_buf = NULL;
	struct is_core *core;
	struct is_device_ischain *idi;
	struct v4l2_ext_control *ext_ctrl;
	struct is_vendor_private *vendor_priv;
	struct is_vendor_rom *rom_info = NULL;

	FIMC_BUG(!ctrls);

	idi = GET_DEVICE(vctx);
	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;
	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		switch (ext_ctrl->id) {
		case V4L2_CID_SENSOR_GET_CAL_DATA:
			rom_id = is_vendor_get_rom_id_from_position(vendor_priv->cal_sensor_pos);
			is_sec_get_rom_info(&rom_info, rom_id);
			if (!rom_info) {
				err("fail to get rom_info for rom_id(%d)", rom_id);
				goto p_err;
			}

			if (!rom_info->read_done) {
				ret = -EIO;
				err("fail to check rom read_done(%d)", rom_id);
				goto p_err;
			}

			cal_buf = rom_info->buf;
			cal_size = rom_info->rom_size;

			minfo("%s: cal size(%#x) sensor position(%d)\n", idi,
				__func__, cal_size, vendor_priv->cal_sensor_pos);

			ret = copy_to_user(ext_ctrl->ptr, (void *)cal_buf, cal_size);
			if (ret) {
				err("fail to copy_to_user, ret(%d)\n", ret);
				goto p_err;
			}
			break;
		case V4L2_CID_DEV_INFO_GET_FACTORY_SUPPORTED_ID:
			ret = is_vendor_device_info_get_factory_supported_id(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_GET_ROM_DATA_BY_POSITION:
			ret = is_vendor_device_info_get_rom_data_by_position(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_GET_SENSOR_ID:
			ret = is_vendor_device_info_get_sensorid_by_cameraid(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_GET_AWB_DATA_ADDR:
			ret = is_vendor_device_info_get_awb_data_addr(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_GET_OIS_HALL_DATA:
			ret = is_vendor_device_info_get_ois_hall_data(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_GET_BPC_OTP:
			ret = is_vendor_device_info_get_bpc_otp_data(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_GET_MIPI_PHY:
			ret = is_vendor_device_info_get_mipi_phy(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_GET_MODULE_INFO:
			ret = is_vendor_device_info_get_sec2lsi_module_info(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_GET_SEC2LSI_BUFF:
			ret = is_vendor_device_info_get_sec2lsi_buff(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_GET_CROP_SHIFT_DATA:
#ifdef CAMERA_REAR2_SENSOR_SHIFT_CROP
			ret = is_vendor_device_info_get_crop_shift_data(ext_ctrl->ptr);
#endif
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
		}
	}

p_err:
	return ret;
}

int is_vendor_vidioc_s_ext_ctrl(struct is_video_ctx *vctx,
	struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	struct is_core *core;
	struct is_device_ischain *idi;
	struct v4l2_ext_control *ext_ctrl;
	struct is_vendor_private *vendor_priv;

	FIMC_BUG(!ctrls);
	idi = GET_DEVICE(vctx);
	core = is_get_is_core();
	vendor_priv = core->vendor.private_data;
	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		switch (ext_ctrl->id) {
		case V4L2_CID_DEV_INFO_SET_EFS_DATA:
			ret = is_vendor_device_info_set_efs_data(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_PERFORM_CAL_RELOAD:
			ret = is_vendor_device_info_perform_cal_reload(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_SET_MIPI_PHY:
			ret = is_vendor_device_info_set_mipi_phy(ext_ctrl->ptr);
			break;
		case V4L2_CID_DEV_INFO_SET_SEC2LSI_BUFF:
			ret = is_vendor_device_info_set_sec2lsi_buff(ext_ctrl->ptr);
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

int is_vendor_ssx_video_s_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	return 0;
}

int is_vendor_ssx_video_s_ext_ctrl(struct v4l2_ext_controls *ctrls, void *device_data)
{
	int ret = 0;
	int i;
	struct is_device_sensor *device = (struct is_device_sensor *)device_data;
	struct is_core *core;
	struct is_vendor_private *vendor_priv;
	struct is_module_enum *module;
	struct is_device_sensor_peri *sensor_peri;
	struct v4l2_ext_control *ext_ctrl;

	WARN_ON(!device);
	WARN_ON(!ctrls);

	core = (struct is_core *)platform_get_drvdata(device->pdev);
	vendor_priv = core->vendor.private_data;

	module = (struct is_module_enum *)v4l2_get_subdevdata(device->subdev_module);
	if (unlikely(!module)) {
		err("%s, module in is NULL", __func__);
		module = NULL;
		ret = -1;
		goto p_err;
	}
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;

	WARN_ON(!sensor_peri);

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);
		switch (ext_ctrl->id) {
		default:
			break;
		}
	}

p_err:
	return ret;
}

int is_vendor_ssx_video_g_ctrl(struct v4l2_control *ctrl,
	void *device_data)
{
	int ret = 0;
	int rom_id = ROM_ID_NOTHING;
	struct is_device_sensor *device;
	struct is_vendor_rom *rom_info = NULL;

	FIMC_BUG(!device_data);
	FIMC_BUG(!ctrl);

	device = (struct is_device_sensor *)device_data;
	switch (ctrl->id) {
	case V4L2_CID_SENSOR_GET_CAL_SIZE:
		rom_id = is_vendor_get_rom_id_from_position(device->position);
		is_sec_get_rom_info(&rom_info, rom_id);
		if (!rom_info) {
			err("fail to get rom_info for rom_id(%d)", rom_id);
			ctrl->value = 0;
			goto p_err;
		}
		ctrl->value = rom_info->rom_size;
		break;
	default:
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

int is_vendor_ssx_video_g_ext_ctrl(struct v4l2_ext_controls *ctrls,
	void *device_data)
{
	int ret = 0;
	int i;
	int cal_size = 0;
	int rom_id = ROM_ID_NOTHING;
	char *cal_buf = NULL;
	struct is_device_sensor *device;
	struct v4l2_ext_control *ext_ctrl;
	struct is_vendor_rom *rom_info = NULL;

	FIMC_BUG(!device_data);
	FIMC_BUG(!ctrls);

	device = (struct is_device_sensor *)device_data;
	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		switch (ext_ctrl->id) {
		case V4L2_CID_SENSOR_GET_CAL_DATA:
			rom_id = is_vendor_get_rom_id_from_position(device->position);
			is_sec_get_rom_info(&rom_info, rom_id);
			if (!rom_info) {
				err("fail to get rom_info for rom_id(%d)", rom_id);
				goto p_err;
			}

			if (!rom_info->read_done) {
				err("fail to check rom read_done(%d)", rom_id);
				goto p_err;
			}

			cal_buf = rom_info->buf;
			cal_size = rom_info->rom_size;

			minfo("%s: SS%d cal size(%#x) sensor position(%d)\n", device,
				__func__, device->device_id, cal_size, device->position);

			ret = copy_to_user(ext_ctrl->ptr, (void *)cal_buf, cal_size);
			if (ret) {
				err("fail to copy_to_user, ret(%d)\n", ret);
				goto p_err;
			}
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

p_err:
	return ret;
}

bool is_vendor_check_resource_type(u32 rsc_type)
{
	if (rsc_type == RESOURCE_TYPE_SENSOR0
#ifdef CAMERA_2ND_OIS
		|| rsc_type == RESOURCE_TYPE_SENSOR2
#endif
#ifdef CAMERA_3RD_OIS
		|| rsc_type == RESOURCE_TYPE_SENSOR3
#endif
	)
		return true;
	else
		return false;
}

#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
int acquire_shared_rsc(struct ois_mcu_dev *mcu)
{
	return atomic_inc_return(&mcu->shared_rsc_count);
}

int release_shared_rsc(struct ois_mcu_dev *mcu)
{
	return atomic_dec_return(&mcu->shared_rsc_count);
}

void is_vendor_mcu_power_on(bool use_shared_rsc)
{
	struct is_core *core = NULL;
	struct ois_mcu_dev *mcu = NULL;
	int active_count = 0;

	core = is_get_is_core();
	if (!core->mcu)
		return;

	mcu = core->mcu;

	if (use_shared_rsc) {
		active_count = acquire_shared_rsc(mcu);
		mcu->current_rsc_count = active_count;
		if (active_count != MCU_SHARED_SRC_ON_COUNT) {
			info("%s: mcu is already on. active count = %d\n", __func__, active_count);
			return;
		}
	}

	is_vendor_ois_power_ctrl(mcu, 0x1);
	is_vendor_ois_load_binary(mcu);
	is_vendor_ois_core_ctrl(mcu, 0x1);
	if (!use_shared_rsc)
		is_vendor_ois_device_ctrl(mcu, 0x01);

	info("%s: mcu on.\n", __func__);
}

void is_vendor_mcu_power_off(bool use_shared_rsc)
{
	struct is_core *core = NULL;
	struct ois_mcu_dev *mcu = NULL;
	int active_count = 0;

	core = is_get_is_core();
	if (!core->mcu)
		return;

	mcu = core->mcu;

	if (use_shared_rsc) {
		active_count = release_shared_rsc(mcu);
		mcu->current_rsc_count = active_count;
		if (active_count != MCU_SHARED_SRC_OFF_COUNT) {
			info("%s: mcu is still on use. active count = %d\n", __func__, active_count);
			return;
		}
	}

	is_vendor_ois_power_ctrl(mcu, 0x0);
	info("%s: mcu off.\n", __func__);
}
#endif /* CONFIG_CAMERA_USE_INTERNAL_MCU */

#ifdef CONFIG_OIS_USE
int is_vendor_shaking_gpio_on(struct is_vendor *vendor)
{
	int ret = 0;
	struct is_core *core = NULL;
#if defined(USE_TELE2_OIS_AF_COMMON_INTERFACE) && defined(CAMERA_3RD_OIS) && defined(SHAKING_DISABLE_TELE10X)
	struct is_device_sensor *device = NULL;
	struct is_module_enum *module;
	int sensor_id = 0;
#endif

	info("%s E\n", __func__);

	mutex_lock(&g_shaking_mutex);

	if (check_shaking_noise) {
		mutex_unlock(&g_shaking_mutex);
		return ret;
	}

	check_shaking_noise = true;

	core = container_of(vendor, struct is_core, vendor);

	is_ois_control_gpio(core, SENSOR_POSITION_REAR, GPIO_SCENARIO_ON);
#ifdef CONFIG_AF_HOST_CONTROL
	is_af_move_lens(core, SENSOR_POSITION_REAR);
#endif
#if defined(USE_TELE2_OIS_AF_COMMON_INTERFACE) && defined(CAMERA_3RD_OIS) && defined(SHAKING_DISABLE_TELE10X)
	ret = is_vendor_get_module_from_position(SENSOR_POSITION_REAR4, &module);
	if (!module || ret) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		mutex_unlock(&g_shaking_mutex);
		return ret;
	}

	sensor_id = module->pdata->id;
	device = &core->sensor[sensor_id];

	is_ois_control_gpio(core, SENSOR_POSITION_REAR4, GPIO_SCENARIO_ON);

	if (device->mcu->mcu_ctrl_actuator) {
		is_vendor_mcu_power_on(false);
		is_ois_init_rear2(core);
		ret = CALL_OISOPS(device->mcu->ois, ois_set_af_active, device->subdev_mcu, 1);
		if (ret < 0)
			err("ois set af active fail");
	}
#endif

	mutex_unlock(&g_shaking_mutex);
	info("%s X\n", __func__);

	return ret;
}

int is_vendor_shaking_gpio_off(struct is_vendor *vendor)
{
	int ret = 0;
	struct is_core *core = NULL;
#if defined(USE_TELE2_OIS_AF_COMMON_INTERFACE) && defined(CAMERA_3RD_OIS) && defined(SHAKING_DISABLE_TELE10X)
	struct is_device_sensor *device = NULL;
	struct is_module_enum *module;
	int sensor_id = 0;
#endif

	info("%s E\n", __func__);

	mutex_lock(&g_shaking_mutex);

	if (!check_shaking_noise) {
		mutex_unlock(&g_shaking_mutex);
		return ret;
	}

	core = container_of(vendor, struct is_core, vendor);

#if defined(USE_TELE2_OIS_AF_COMMON_INTERFACE) && defined(CAMERA_3RD_OIS) && defined(SHAKING_DISABLE_TELE10X)
	ret = is_vendor_get_module_from_position(SENSOR_POSITION_REAR4, &module);
	if (!module || ret) {
		err("%s, module is NULL", __func__);
		ret = -EINVAL;
		mutex_unlock(&g_shaking_mutex);
		return ret;
	}

	sensor_id = module->pdata->id;
	device = &core->sensor[sensor_id];

	if (device->mcu->mcu_ctrl_actuator) {
		ret = CALL_OISOPS(device->mcu->ois, ois_set_af_active, device->subdev_mcu, 0);
		if (ret < 0)
			err("ois set af active fail");
	}
	is_vendor_mcu_power_off(false);
#endif

	is_ois_control_gpio(core, SENSOR_POSITION_REAR, GPIO_SCENARIO_OFF);
#if defined(USE_TELE2_OIS_AF_COMMON_INTERFACE) && defined(CAMERA_3RD_OIS) && defined(SHAKING_DISABLE_TELE10X)
	is_ois_control_gpio(core, SENSOR_POSITION_REAR4, GPIO_SCENARIO_OFF);
#endif

	check_shaking_noise = false;
	mutex_unlock(&g_shaking_mutex);
	info("%s X\n", __func__);

	return ret;
}
#endif

void is_vendor_resource_get(struct is_vendor *vendor, u32 rsc_type)
{
#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	if (is_vendor_check_resource_type(rsc_type))
		is_vendor_mcu_power_on(true);
#endif
}

void is_vendor_resource_put(struct is_vendor *vendor, u32 rsc_type)
{
#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
	if (is_vendor_check_resource_type(rsc_type))
		is_vendor_mcu_power_off(true);
#endif
}

// TEMP_OLYMPUS
long is_vendor_read_efs(char *efs_path, u8 *buf, int buflen)
{
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct file *fp = NULL;
	char filename[100];
	long ret = 0, fsize = 0, nread = 0;

	info("%s  start", __func__);

	mutex_lock(&g_efs_mutex);

	snprintf(filename, sizeof(filename), "%s", efs_path);

	fp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(fp)) {
		mutex_unlock(&g_efs_mutex);
		err("file open  fail(%ld)\n", fsize);
		return 0;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	if (fsize <= 0 || fsize > buflen) {
		err(" __get_file_size fail(%ld)\n", fsize);
		ret = 0;
		goto p_err;
	}

	nread = kernel_read(fp, buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		err("kernel_read was failed(%ld != %ld)n",
			nread, fsize);
		ret = 0;
		goto p_err;
	}

	ret = fsize;

p_err:
	filp_close(fp, current->files);

	return ret;
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(is_vendor_read_efs);

bool is_vendor_wdr_mode_on(void *cis_data)
{
#ifdef USE_CAMERA_SENSOR_WDR
	return (((cis_shared_data *)cis_data)->is_data.wdr_mode != CAMERA_WDR_OFF ? true : false);
#else
	return false;
#endif
}
EXPORT_SYMBOL_GPL(is_vendor_wdr_mode_on);

bool is_vendor_enable_wdr(void *cis_data)
{
	return false;
}

#ifdef USE_TOF_AF
void is_vendor_store_af(struct is_vendor *vendor, struct tof_data_t *data)
{
	struct is_vendor_private *vendor_priv;
	vendor_priv = vendor->private_data;

	mutex_lock(&vendor_priv->tof_af_lock);
	copy_from_user(&vendor_priv->tof_af_data, data, sizeof(struct tof_data_t));
	mutex_unlock(&vendor_priv->tof_af_lock);
	return;
}
#endif

void is_vendor_store_tof_info(struct is_vendor *vendor, struct tof_info_t *info)
{
	int ret;
	struct is_vendor_private *vendor_priv;
	vendor_priv = vendor->private_data;

	ret = copy_from_user(&vendor_priv->tof_info, info, sizeof(struct tof_info_t));
	return;
}

int is_vendor_is_dualized(struct is_device_sensor *sensor, int pos)
{
	struct is_core *core;
	struct is_vendor_private *vendor_priv = NULL;

	core = sensor->private_data;
	vendor_priv = core->vendor.private_data;

	return vendor_priv->is_dualized[pos];
}

void is_vendor_update_otf_data(struct is_group *group, struct is_frame *frame)
{
	struct is_device_sensor *sensor = NULL;
	enum aa_capture_intent captureIntent;
	unsigned long flags;

	spin_lock_irqsave(&group->slock_s_ctrl, flags);

	captureIntent = group->intent_ctl.captureIntent;

	/* Capture scenario */
	if (captureIntent != AA_CAPTURE_INTENT_CUSTOM) {
		if (group->remainCaptureIntentCount > 0) {
			frame->shot->ctl.aa.captureIntent = captureIntent;
			frame->shot->ctl.aa.vendor_captureCount = group->intent_ctl.vendor_captureCount;
			frame->shot->ctl.aa.vendor_captureExposureTime = group->intent_ctl.vendor_captureExposureTime;
			frame->shot->ctl.aa.vendor_captureEV = group->intent_ctl.vendor_captureEV;
			frame->shot->ctl.aa.vendor_captureExtraInfo = group->intent_ctl.vendor_captureExtraInfo;
			if (group->intent_ctl.vendor_isoValue) {
				frame->shot->ctl.aa.vendor_isoMode = AA_ISOMODE_MANUAL;
				frame->shot->ctl.aa.vendor_isoValue = group->intent_ctl.vendor_isoValue;
				frame->shot->ctl.sensor.sensitivity = frame->shot->ctl.aa.vendor_isoValue;
			}
			if (group->intent_ctl.vendor_aeExtraMode) {
				frame->shot->ctl.aa.vendor_aeExtraMode = group->intent_ctl.vendor_aeExtraMode;
			}
			frame->shot->ctl.aa.aeMode = group->intent_ctl.aeMode;

			if (group->slot == GROUP_SLOT_PAF || group->slot == GROUP_SLOT_3AA) {
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameEvList),
					&(group->intent_ctl.vendor_multiFrameEvList),
					sizeof(group->intent_ctl.vendor_multiFrameEvList));
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameIsoList),
					&(group->intent_ctl.vendor_multiFrameIsoList),
					sizeof(group->intent_ctl.vendor_multiFrameIsoList));
				memcpy(&(frame->shot->ctl.aa.vendor_multiFrameExposureList),
					&(group->intent_ctl.vendor_multiFrameExposureList),
					sizeof(group->intent_ctl.vendor_multiFrameExposureList));
			}
			group->remainCaptureIntentCount--;
		} else {
			group->intent_ctl.captureIntent = AA_CAPTURE_INTENT_CUSTOM;
			group->intent_ctl.vendor_captureCount = 0;
			group->intent_ctl.vendor_captureExposureTime = 0;
			group->intent_ctl.vendor_captureEV = 0;
			group->intent_ctl.vendor_captureExtraInfo = 0;
			memset(&(group->intent_ctl.vendor_multiFrameEvList), 0,
				sizeof(group->intent_ctl.vendor_multiFrameEvList));
			memset(&(group->intent_ctl.vendor_multiFrameIsoList), 0,
				sizeof(group->intent_ctl.vendor_multiFrameIsoList));
			memset(&(group->intent_ctl.vendor_multiFrameExposureList), 0,
				sizeof(group->intent_ctl.vendor_multiFrameExposureList));
			group->intent_ctl.vendor_isoValue = 0;
			group->intent_ctl.vendor_aeExtraMode = AA_AE_EXTRA_MODE_AUTO;
			group->intent_ctl.aeMode = 0;
		}

		info("frame count(%d), intent(%d), count(%d) captureExposureTime(%d) remainCaptureIntentCount(%d)\n",
			frame->fcount,
			frame->shot->ctl.aa.captureIntent, frame->shot->ctl.aa.vendor_captureCount,
			frame->shot->ctl.aa.vendor_captureExposureTime, group->remainCaptureIntentCount);
	}

	/* General scenario except for capture */
	if (group->remainIntentCount > 0) {
		if (group->intent_ctl.vendor_transientAction != AA_TRANSIENT_ACTION_INVALID) {
			frame->shot->ctl.aa.vendor_transientAction = group->intent_ctl.vendor_transientAction;
			info("frame count(%d), vendor_transientAction(%d), remainIntentCount(%d)\n",
				frame->fcount, frame->shot->ctl.aa.vendor_transientAction,  group->remainIntentCount);
		}

		group->remainIntentCount--;
	} else {
		group->intent_ctl.vendor_transientAction = AA_TRANSIENT_ACTION_INVALID;
	}

	if (group->remainRemosaicCropIntentCount > 0) {
		if (group->intent_ctl.vendor_remosaicCropZoomRatio != AA_REMOSAIC_CROP_ZOOM_RATIO_INVALID) {
			frame->shot->ctl.aa.vendor_remosaicCropZoomRatio = group->intent_ctl.vendor_remosaicCropZoomRatio;
			info("frame count(%d), vendor_remosaicCropZoomRatio(%d), remainRemosaicCropIntentCount(%d)\n",
				frame->fcount, frame->shot->ctl.aa.vendor_remosaicCropZoomRatio,
				group->remainRemosaicCropIntentCount);
		}

		group->remainRemosaicCropIntentCount--;
	} else {
		group->intent_ctl.vendor_remosaicCropZoomRatio = AA_REMOSAIC_CROP_ZOOM_RATIO_INVALID;
	}

	/* STAT ROI */
	sensor = group->device->sensor;
	if (sensor != NULL) {
		int remosaicCropZoomRatio = 0;
		int sensor_ratio = (sensor->sensor_width * 100) / sensor->sensor_height;
		int sensorWidth = sensor->sensor_width;
		int sensorHeight = sensor->sensor_height;
		int bcrop[4] = {frame->shot_ext->node_group.leader.input.cropRegion[0],
				frame->shot_ext->node_group.leader.input.cropRegion[1],
				frame->shot_ext->node_group.leader.input.cropRegion[2],
				frame->shot_ext->node_group.leader.input.cropRegion[3]};
		int bcrop_ratio = (bcrop[2] * 100) / bcrop[3];

		if (sensor_ratio > bcrop_ratio)
			sensorWidth = (sensorHeight * bcrop_ratio) / 100;
		else if (sensor_ratio < bcrop_ratio)
			sensorHeight = (sensorWidth * 100 / bcrop_ratio);

		if (sensor->pdev) {
			struct is_module_enum *module = NULL;
			struct is_device_sensor_peri *sensor_peri = NULL;
			struct is_cis *cis = NULL;

			if (!is_sensor_g_module(sensor, &module)) {
				sensor_peri = (struct is_device_sensor_peri *)module->private_data;
				if (sensor_peri->subdev_cis) {
					cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

					if (cis && cis->cis_data)
						remosaicCropZoomRatio = cis->cis_data->cur_remosaic_zoom_ratio / 10;
				}
			}
		} else {
			info("frame count(%d) sensor->pdev is null(%pK)\n", frame->fcount, sensor->pdev);
		}

		if (test_bit(IS_SENSOR_RMS_CROP_ON, &sensor->rms_crop_state) && remosaicCropZoomRatio > 1) {
			int stat_roi[4] = {frame->shot->uctl.statsRoi[0], frame->shot->uctl.statsRoi[1],
					frame->shot->uctl.statsRoi[2], frame->shot->uctl.statsRoi[3]};
			int ss_crop[4] = {(sensor->sensor_width * remosaicCropZoomRatio - sensorWidth) >> 1,
					(sensor->sensor_height * remosaicCropZoomRatio - sensorHeight) >> 1,
					sensorWidth, sensorHeight};
			int i;

			dbg_sensor(1, "[%u][F%d]rms_crop_state(%lu)bcrop(%d %d %d %d)(%d)ss_crop(%d %d %d %d)",
				group->instance, frame->fcount, sensor->rms_crop_state,
				bcrop[0], bcrop[1], bcrop[2], bcrop[3],
				((bcrop[2] * 100) / bcrop[3]),
				ss_crop[0], ss_crop[1], ss_crop[2], ss_crop[3]);

			/**
			* Step 1.
			*
			* Transform the user bcrop coordinate
			* from 'sensor crop coordinate' to 'sensor binning coordinate'.
			*/
			for (i = 0; i < 4; i++) {
				stat_roi[i] = stat_roi[i] * remosaicCropZoomRatio;
				bcrop[i] = bcrop[i] * remosaicCropZoomRatio;
			}
			dbg_sensor(1, "[%u][F%d]Step1.bcrop(%d %d %d %d)(%d)ss_crop(%d %d %d %d)stat_roi(%d %d %d %d)",
				group->instance, frame->fcount,
				bcrop[0], bcrop[1], bcrop[2], bcrop[3],
				((bcrop[2] * 100) / bcrop[3]),
				ss_crop[0], ss_crop[1], ss_crop[2], ss_crop[3],
				stat_roi[0], stat_roi[1], stat_roi[2], stat_roi[3]);

			/**
			* Step 2.
			*
			* Remove the overlapped crop offset from user bcrop region.
			* In case of Bcrop, does't remove the overlapped crop offset
			* because of changed the domain of statRoi to BCrop.
			*/
			for (i = 0; i < 2; i++) {
				int diff = 0;

				if (bcrop[i+2] > ss_crop[i+2])
					diff = (bcrop[i+2] - ss_crop[i+2]) / 2;

				if (stat_roi[i] > diff)
					stat_roi[i] = (stat_roi[i] - diff);
				else
					stat_roi[i] = 0;
			}
			dbg_sensor(1, "[%u][F%d]Step2.bcrop(%d %d %d %d)(%d)ss_crop(%d %d %d %d)stat_roi(%d %d %d %d)",
				group->instance, frame->fcount,
				bcrop[0], bcrop[1], bcrop[2], bcrop[3],
				((bcrop[2] * 100) / bcrop[3]),
				ss_crop[0], ss_crop[1], ss_crop[2], ss_crop[3],
				stat_roi[0], stat_roi[1], stat_roi[2], stat_roi[3]);

			/**
			* Step 3.
			*
			* Check user crop boundary.
			*/
			for (i = 2; i < 4; i++)
				stat_roi[i] = (stat_roi[i] > ss_crop[i]) ? ss_crop[i] : stat_roi[i];

			/**
			* Step 4.
			*
			* Check alignment. CSTAT requires 2px alignment for width.
			*/
			stat_roi[2] = ALIGN_DOWN(stat_roi[2], 2);

			dbg_sensor(1, "[%u][F%d]rms_crop_state(%lu)stat_roi(%d %d %d %d)->(%d %d %d %d)",
				group->instance, frame->fcount, sensor->rms_crop_state,
				frame->shot->uctl.statsRoi[0], frame->shot->uctl.statsRoi[1],
				frame->shot->uctl.statsRoi[2], frame->shot->uctl.statsRoi[3],
				stat_roi[0], stat_roi[1], stat_roi[2], stat_roi[3]);

			frame->shot->uctl.statsRoi[0] = stat_roi[0];
			frame->shot->uctl.statsRoi[1] = stat_roi[1];
			frame->shot->uctl.statsRoi[2] = stat_roi[2];
			frame->shot->uctl.statsRoi[3] = stat_roi[3];
		}
	}
	spin_unlock_irqrestore(&group->slock_s_ctrl, flags);
}

void is_vendor_s_ext_ctrl_capture_intent_info(struct is_group *head, struct capture_intent_info_t info)
{
	unsigned long flags;

	spin_lock_irqsave(&head->slock_s_ctrl, flags);

	head->intent_ctl.captureIntent = info.captureIntent;
	head->intent_ctl.vendor_captureCount = info.captureCount;
	head->intent_ctl.vendor_captureEV = info.captureEV;
	head->intent_ctl.vendor_captureExtraInfo = info.captureExtraInfo;
	if (info.captureIso) {
		head->intent_ctl.vendor_isoValue = info.captureIso;
	}
	if (info.captureAeExtraMode) {
		head->intent_ctl.vendor_aeExtraMode = info.captureAeExtraMode;
	}
	if (info.captureAeMode) {
		head->intent_ctl.aeMode = info.captureAeMode;
	}
	memcpy(&(head->intent_ctl.vendor_multiFrameEvList), &(info.captureMultiEVList),
		sizeof(info.captureMultiEVList));
	memcpy(&(head->intent_ctl.vendor_multiFrameIsoList), &(info.captureMultiIsoList),
		sizeof(info.captureMultiIsoList));
	memcpy(&(head->intent_ctl.vendor_multiFrameExposureList), &(info.CaptureMultiExposureList),
		sizeof(info.CaptureMultiExposureList));

	switch (info.captureIntent) {
	case AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI:
	case AA_CAPTURE_INTENT_STILL_CAPTURE_GALAXY_RAW_DYNAMIC_SHOT:
	case AA_CAPTURE_INTENT_STILL_CAPTURE_EXECUTOR_NIGHT_SHOT:
	case AA_CAPTURE_INTENT_STILL_CAPTURE_REPEATED_MULTI_FRAME_LIST:
		head->remainCaptureIntentCount = 3 + CAPTURE_INTENT_RETRY_CNT;
		break;
	default:
		head->remainCaptureIntentCount = 1 + CAPTURE_INTENT_RETRY_CNT;
		break;
	}
	spin_unlock_irqrestore(&head->slock_s_ctrl, flags);
}

int is_vendor_notify_hal_init(int mode, struct is_device_sensor *sensor)
{
	int ret = 0;

	if ((!check_hw_init && mode == HW_INIT_MODE_NORMAL)
		|| mode == HW_INIT_MODE_RELOAD) {
		ret = is_vendor_hw_init();
		is_sysfs_hw_init();
		check_hw_init = true;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_SEC_ABC)
static const char *is_mipi_error_list[CAM_INFO_TYPE_MAX] = {
	[CAM_INFO_TYPE_RW1] = "rw1",
	[CAM_INFO_TYPE_RS1] = "rs1",
	[CAM_INFO_TYPE_RT1] = "rt1",
	[CAM_INFO_TYPE_FW1] = "fw1",
	[CAM_INFO_TYPE_RT2] = "rt2",
	[CAM_INFO_TYPE_UW1] = "uw1",
	[CAM_INFO_TYPE_RM1] = "rm1",
	[CAM_INFO_TYPE_RB1] = "rb1",
	[CAM_INFO_TYPE_FS1] = "fs1",
};

void is_vendor_sec_abc_send_event_mipi_error(u32 position)
{
	char msg1[32], event[40];
	struct is_cam_info *cam_info = NULL;
	int i;

	for (i = 0; i < CAM_INFO_MAX; i++) {
		is_get_cam_info_from_index(&cam_info, i);
		if (cam_info->position == position)
			break;
	}

	if (i == CAM_INFO_MAX) {
		info("[%s] failed to get cam_info which corresponds to position(%d)\n", __func__, position);
		return;
	}

	if (cam_info->type >= CAM_INFO_TYPE_MAX) {
		info("[%s] cam_info_type overflow! (type: %d)\n", __func__, cam_info->type);
		return;
	}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	sprintf(msg1, "%s", "MODULE=camera@INFO=mipi_error_");
#else
	sprintf(msg1, "%s", "MODULE=camera@WARN=mipi_error_");
#endif

	sprintf(event, "%s%s", msg1, is_mipi_error_list[cam_info->type]);

	sec_abc_send_event(event);
}
#endif
