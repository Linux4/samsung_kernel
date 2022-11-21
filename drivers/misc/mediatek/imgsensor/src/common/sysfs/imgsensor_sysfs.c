/*
 * Copyright (C) 2019.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include "imgsensor_cfg_table.h"
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/unistd.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>

#include <linux/syscalls.h>
#include <linux/file.h>

#undef CONFIG_MTK_SMI_EXT
#ifdef CONFIG_MTK_SMI_EXT
#include "mmdvfs_mgr.h"
#endif
#ifdef CONFIG_OF
/* device tree */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef CONFIG_MTK_CCU
#include "ccu_inc.h"
#endif

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"


#include "imgsensor_sensor_list.h"
#include "imgsensor_hw.h"
#include "imgsensor_i2c.h"
#include "imgsensor_proc.h"
#include "imgsensor_sysfs.h"
#include "imgsensor.h"

#define NOTYETDONE 0
#define ISP_VENDOR1 'K' // K means mediatek
#define ISP_VENDOR2 'E' // E means common

extern struct kset *devices_kset;

bool crc32_check_list[SENSOR_POSITION_MAX][CRC32_SCENARIO_MAX];
bool check_latest_cam_module[SENSOR_POSITION_MAX] = {false, };
bool check_final_cam_module[SENSOR_POSITION_MAX] = {false, };

static struct imgsensor_cam_info cam_infos[CAM_INFO_MAX];
static struct imgsensor_common_cam_info common_cam_infos;
static struct imgsensor_vendor_specific *specific;
struct imgsensor_rom_info *finfo[SENSOR_POSITION_MAX] = {NULL,};
struct ssrm_camera_data SsrmCameraInfo[IMGSENSOR_SENSOR_COUNT];

static int current_sensor_pos = 0;

struct class   *camera_class = NULL;
struct device  *camera_rear_dev;
struct device  *camera_front_dev;


static char *g_cal_buf[SENSOR_POSITION_MAX] = {NULL};

static int g_cal_buf_size[SENSOR_POSITION_MAX] = {0};

static const char * const g_cal_data_name_by_postion[SENSOR_POSITION_MAX] = {
	"CD",  //SENSOR_POSITION_REAR
	"CD3", //SENSOR_POSITION_FRONT
	"CD4", //SENSOR_POSITION_REAR2
	NULL,  //SENSOR_POSITION_FRONT2
	"CD6", //SENSOR_POSITION_REAR3
	NULL,  //SENSOR_POSITION_FRONT3
	"CD8", //SENSOR_POSITION_REAR4
	NULL,  //SENSOR_POSITION_FRONT4
	NULL,  //SENSOR_POSITION_REAR_TOF
	NULL,  //SENSOR_POSITION_FRONT_TOF
};

bool imgsensor_sec_check_rom_ver(int sub_deviceIdx)
{
	if (!specific) {
		pr_err("%s: specific not yet probed", __func__);
		return false;
	}

	if (specific->skip_cal_loading) {
		pr_err("skip_cal_loading implemented");
		return false;
	}

	if (finfo[sub_deviceIdx] == NULL) {
		pr_err("Failed to get finfo. Please check position %d", sub_deviceIdx);
		return false;
	}
	return true;
}

enum sensor_position map_position(enum IMGSENSOR_SENSOR_IDX img_position)
{
	switch(img_position) {
	case IMGSENSOR_SENSOR_IDX_MAIN:
		return SENSOR_POSITION_REAR;
	case IMGSENSOR_SENSOR_IDX_SUB:
		return SENSOR_POSITION_FRONT;
	case IMGSENSOR_SENSOR_IDX_MAIN2:
		return SENSOR_POSITION_REAR2;
	case IMGSENSOR_SENSOR_IDX_SUB2:
		return SENSOR_POSITION_REAR3;
	case IMGSENSOR_SENSOR_IDX_MAIN3:
		return SENSOR_POSITION_REAR4;
	case IMGSENSOR_SENSOR_IDX_MAX_NUM:
		return SENSOR_POSITION_MAX;
	case IMGSENSOR_SENSOR_IDX_NONE:
		return SENSOR_POSITION_NONE;
	default:
		return SENSOR_POSITION_NONE;
	};
}

void update_curr_sensor_pos(int sensorId) {
	int remapped_device_id = (int)map_position(sensorId);
	current_sensor_pos = remapped_device_id;
}

void update_mipi_sensor_err_cnt()
{
	struct cam_hw_param *ec_param = NULL;

	imgsensor_sec_get_hw_param(&ec_param, current_sensor_pos);
	if (ec_param) {
	ec_param->mipi_sensor_err_cnt++;
	}
}

void init_cam_hwparam(int position)
{
	struct cam_hw_param *ec_param = NULL;

	imgsensor_sec_get_hw_param(&ec_param, position);
	if (ec_param == NULL) {
		pr_err("Failed to get hwparam. Position - %d\n", position);
		return;
	}
	imgsensor_sec_init_err_cnt(ec_param);
}

int imgsensor_get_cal_buf(int position, char **buf)
{
	if (position >= SENSOR_POSITION_MAX) {
		pr_err("position %d exceeds available index(%d)", position, SENSOR_POSITION_MAX-1);
		return -EINVAL;
	}

	*buf = g_cal_buf[position];

	if (*buf == NULL) {
		pr_err("cal buf is null. position %d", position);
		return -EINVAL;
	}
	return 0;
}

int imgsensor_get_cal_size(int position)
{
	if (position >= SENSOR_POSITION_MAX)
		return 0;

	return g_cal_buf_size[position];
}

int imgsensor_get_cal_buf_by_sensor_idx(int sensor_idx, char **buf)
{
	int position = map_position(sensor_idx);
	return imgsensor_get_cal_buf(position, buf);
}

int imgsensor_get_cal_size_by_sensor_idx(int sensor_idx)
{
	int position = map_position(sensor_idx);
	return imgsensor_get_cal_size(position);
}

int get_cam_info(struct imgsensor_cam_info **caminfo)
{
	*caminfo = cam_infos;
	return 0;
}

void imgsensor_get_common_cam_info(struct imgsensor_common_cam_info **caminfo)
{
	*caminfo = &common_cam_infos;
}

int get_specific(struct imgsensor_vendor_specific **sys_specific)
{
	specific = *sys_specific;
	return 0;
}

int svc_cheating_prevent_device_file_create(struct kobject **obj)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct kobject *Camera;

	/* To find svc kobject */
	svc_sd = sysfs_get_dirent(devices_kset->kobj.sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", &devices_kset->kobj);
		if (IS_ERR_OR_NULL(data))
			pr_info("Failed to create sys/devices/svc already exist svc : 0x%pK\n", data);
		else
			pr_info("Success to create sys/devices/svc svc : 0x%pK\n", data);
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("Success to find svc_sd : 0x%pK svc : 0x%pK\n", svc_sd, data);
	}

	Camera = kobject_create_and_add("Camera", data);
	if (IS_ERR_OR_NULL(Camera))
		pr_info("Failed to create sys/devices/svc/Camera : 0x%pK\n", Camera);
	else
		pr_info("Success to create sys/devices/svc/Camera : 0x%pK\n", Camera);

	*obj = Camera;
	return 0;
}


#if defined(REAR_SUB_CAMERA)
static ssize_t camera_tilt_show(char *buf, int deviceIdx)
{
	char *cal_buf = NULL;
	char temp_buffer[50] = {0,};

	if (!imgsensor_sec_check_rom_ver(deviceIdx)) {
		pr_err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}
	if(imgsensor_get_cal_buf(deviceIdx, &cal_buf) == -EINVAL) {
		pr_err(" NG, Cal Buf error");
		return sprintf(buf, "%s\n", "NG");
	}
	if (vendor_rom_addr[deviceIdx] == NULL) {
		pr_err("[%s] fail \n", __func__);
		return sprintf(buf, "%s\n", "NG");
	}

	if (vendor_rom_addr[deviceIdx]->rom_dual_cal_data2_size > 0) {
		int32_t dual_tilt_x = vendor_rom_addr[deviceIdx]->rom_dual_tilt_x_addr;
		int32_t dual_tilt_y = vendor_rom_addr[deviceIdx]->rom_dual_tilt_y_addr;
		int32_t dual_tilt_z = vendor_rom_addr[deviceIdx]->rom_dual_tilt_z_addr;
		int32_t dual_tilt_sx = vendor_rom_addr[deviceIdx]->rom_dual_tilt_sx_addr;
		int32_t dual_tilt_sy = vendor_rom_addr[deviceIdx]->rom_dual_tilt_sy_addr;
		int32_t dual_tilt_range = vendor_rom_addr[deviceIdx]->rom_dual_tilt_range_addr;
		int32_t dual_tilt_max_err = vendor_rom_addr[deviceIdx]->rom_dual_tilt_max_err_addr;
		int32_t dual_tilt_avg_err = vendor_rom_addr[deviceIdx]->rom_dual_tilt_avg_err_addr;
		int32_t dual_tilt_dll_version = vendor_rom_addr[deviceIdx]->rom_dual_tilt_dll_version_addr;
		int32_t dual_tilt_dll_modelID = vendor_rom_addr[deviceIdx]->rom_dual_tilt_dll_modelID_addr;
		int32_t dual_tilt_dll_modelIDLength = vendor_rom_addr[deviceIdx]->rom_dual_tilt_dll_modelID_size;

		if (specific->use_dualcam_set_cal) {
			return sprintf(buf, "1 0 0 0 0 0 0 0 0 0\n");
		}

		if (crc32_check_list[deviceIdx][CRC32_CHECK_FW_VER] &&
			crc32_check_list[deviceIdx][CRC32_CHECK_FACTORY]) {
			s32 *x = NULL, *y = NULL, *z = NULL, *sx = NULL, *sy = NULL;
			s32 *range = NULL, *max_err = NULL, *avg_err = NULL, *dll_version = NULL;
			char *modelID = NULL;

			x = (s32 *)&cal_buf[dual_tilt_x];
			y = (s32 *)&cal_buf[dual_tilt_y];
			z = (s32 *)&cal_buf[dual_tilt_z];
			sx = (s32 *)&cal_buf[dual_tilt_sx];
			sy = (s32 *)&cal_buf[dual_tilt_sy];
			range = (s32 *)&cal_buf[dual_tilt_range];
			max_err = (s32 *)&cal_buf[dual_tilt_max_err];
			avg_err = (s32 *)&cal_buf[dual_tilt_avg_err];
			dll_version = (s32 *)&cal_buf[dual_tilt_dll_version];
			modelID = (char *)&cal_buf[dual_tilt_dll_modelID];

			strncpy(temp_buffer,modelID,dual_tilt_dll_modelIDLength);
			temp_buffer[dual_tilt_dll_modelIDLength] = '\0';
			pr_info("%s -- Model ID = %s\n", __func__, temp_buffer);

			return sprintf(buf, "1 %d %d %d %d %d %d %d %d %d %s\n",
								*x, *y, *z, *sx, *sy, *range, *max_err, *avg_err, *dll_version, temp_buffer);
		} else {
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		return sprintf(buf, "%s\n", "NG");
	}
}
#endif

static ssize_t camera_ssrm_camera_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ssrm_camera_data temp;
	int ret_count;
	int index = -1;
	int i = 0;

	ret_count = sscanf(buf, "%d%d%d%d%d%d%d", &temp.operation, &temp.cameraID, &temp.previewMinFPS,
				&temp.previewMaxFPS, &temp.previewSizeWidth,  &temp.previewSizeHeight, &temp.sensorOn);

	if (ret_count > sizeof(SsrmCameraInfo)/(sizeof(int)))
		return -EINVAL;

	switch (temp.operation) {
	case SSRM_CAMERA_INFO_CLEAR:
		for (i = 0; i < IMGSENSOR_SENSOR_COUNT; i++) { /* clear */
			if (SsrmCameraInfo[i].cameraID == temp.cameraID) {
				SsrmCameraInfo[i].previewMaxFPS = 0;
				SsrmCameraInfo[i].previewMinFPS = 0;
				SsrmCameraInfo[i].previewSizeHeight = 0;
				SsrmCameraInfo[i].previewSizeWidth = 0;
				SsrmCameraInfo[i].sensorOn = 0;
				SsrmCameraInfo[i].cameraID = -1;
			}
		}
		break;

	case SSRM_CAMERA_INFO_SET:
		for (i = 0; i < IMGSENSOR_SENSOR_COUNT; i++) { /* find empty space*/
			if (SsrmCameraInfo[i].cameraID == -1) {
				index = i;
				break;
			}
		}

		if (index == -1)
			return -EPERM;

		memcpy(&SsrmCameraInfo[i], &temp, sizeof(temp));
		break;

	case SSRM_CAMERA_INFO_UPDATE:
		for (i = 0; i < IMGSENSOR_SENSOR_COUNT; i++) {
			if (SsrmCameraInfo[i].cameraID == temp.cameraID) {
				SsrmCameraInfo[i].previewMaxFPS = temp.previewMaxFPS;
				SsrmCameraInfo[i].previewMinFPS = temp.previewMinFPS;
				SsrmCameraInfo[i].previewSizeHeight = temp.previewSizeHeight;
				SsrmCameraInfo[i].previewSizeWidth = temp.previewSizeWidth;
				SsrmCameraInfo[i].sensorOn = temp.sensorOn;
				break;
			}
		}
		break;
	default:
		break;
	}

	return count;
}

static ssize_t camera_ssrm_camera_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp_buffer[50] = {0,};
	int i = 0;

	for (i = 0; i < IMGSENSOR_SENSOR_COUNT; i++) {
		if (SsrmCameraInfo[i].cameraID != -1) {
			strncat(buf, "ID=", strlen("ID="));
			sprintf(temp_buffer, "%d;", SsrmCameraInfo[i].cameraID);
			strncat(buf, temp_buffer, strlen(temp_buffer));

			strncat(buf, "ON=", strlen("ON="));
			sprintf(temp_buffer, "%d;", SsrmCameraInfo[i].sensorOn);
			strncat(buf, temp_buffer, strlen(temp_buffer));

			if (SsrmCameraInfo[i].previewMinFPS && SsrmCameraInfo[i].previewMaxFPS) {
				strncat(buf, "FPS=", strlen("FPS="));
				sprintf(temp_buffer, "%d,%d;",
						SsrmCameraInfo[i].previewMinFPS, SsrmCameraInfo[i].previewMaxFPS);
				strncat(buf, temp_buffer, strlen(temp_buffer));
			}
			if (SsrmCameraInfo[i].previewSizeWidth && SsrmCameraInfo[i].previewSizeHeight) {
				strncat(buf, "SIZE=", strlen("SIZE="));
				sprintf(temp_buffer, "%d,%d;",
						SsrmCameraInfo[i].previewSizeWidth, SsrmCameraInfo[i].previewSizeHeight);
				strncat(buf, temp_buffer, strlen(temp_buffer));
			}
			strncat(buf, "\n", strlen("\n"));
		}
	}
	return strlen(buf);
}

static ssize_t camera_sensorid_show(int position, char *buf)
{
	if(!imgsensor_sec_check_rom_ver(position)) {
		return -ENODEV;
	}
	return sprintf(buf, "%d\n", specific->sensor_id[position]);
}

static ssize_t camera_sensorid_exif_show(int position, char *buf)
{
	if(!imgsensor_sec_check_rom_ver(position)) {
		return -ENODEV;
	}
	return sprintf(buf, "%s\n", finfo[position]->rom_sensor_id);
}

static ssize_t camera_mtf_exif_show(int position, char *buf)
{
	char *cal_buf;

	if (!imgsensor_sec_check_rom_ver(position))
		return -ENODEV;

	imgsensor_get_cal_buf(position, &cal_buf);
	memcpy(buf, &cal_buf[finfo[position]->mtf_data_addr], IMGSENSOR_RESOLUTION_DATA_SIZE);
	return IMGSENSOR_RESOLUTION_DATA_SIZE;
}

static ssize_t camera_moduleid_show(int position, char *buf)
{
	if (!imgsensor_sec_check_rom_ver(position))
		return sprintf(buf, "0000000000\n");

	return sprintf(buf, "%c%c%c%c%c%02X%02X%02X%02X%02X\n",
			finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
			finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
			finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[5],
			finfo[position]->rom_module_id[6], finfo[position]->rom_module_id[7],
			finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9]);
}

static ssize_t camera_camtype_show(int position, char *buf)
{
	if (!imgsensor_sec_check_rom_ver(position))
		return -ENODEV;

	return sprintf(buf, "%s_%s\n", finfo[position]->sensor_maker, finfo[position]->sensor_name);
}

static ssize_t camera_camfw_show(int position, char *buf)
{
	char command_ack[20] = {0, };

	if (!imgsensor_sec_check_rom_ver(position)) {
		pr_err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FW");
		strcat(command_ack, g_cal_data_name_by_postion[position]);
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, finfo[position]->header_ver);
		} else {
			pr_err(" NG, crc check fail");
			strcpy(command_ack, "NG_");
			strcat(command_ack, g_cal_data_name_by_postion[position]);

			if (finfo[position]->header_ver[3] != ISP_VENDOR1
				&& finfo[position]->header_ver[3] != ISP_VENDOR2)
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
		}
	} else {
		pr_err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_");
		strcat(command_ack, g_cal_data_name_by_postion[position]);
		return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
	}
}

static ssize_t camera_camfw_full_show(int position, char *buf)
{
	char command_ack[20] = {0, };

	if (!imgsensor_sec_check_rom_ver(position)) {
		pr_err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FW");
		strcat(command_ack, g_cal_data_name_by_postion[position]);
		return sprintf(buf, "%s %s %s\n", "NULL", "NULL", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			return sprintf(buf, "%s %s %s\n",
				finfo[position]->header_ver, "N", finfo[position]->header_ver);
		} else {
			pr_err(" NG, crc check fail");
			strcpy(command_ack, "NG_");
			strcat(command_ack, g_cal_data_name_by_postion[position]);

			if (finfo[position]->header_ver[3] != ISP_VENDOR1
				&& finfo[position]->header_ver[3] != ISP_VENDOR2)
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, "N", command_ack);
		}
	} else {
		strcpy(command_ack, g_cal_data_name_by_postion[position]);
		return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, "N", command_ack);
	}
}

static ssize_t camera_checkfw_user_show(int position, char *buf)
{
	if (!imgsensor_sec_check_rom_ver(position)) {

		pr_err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			if (!check_latest_cam_module[position]) {
				pr_err(" NG, not latest cam module");
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			pr_err(" NG, crc check fail");
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		pr_err(" NG, fw ver crc check fail");
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_checkfw_factory_show(int position, char *buf)
{
	char command_ack[10] = {0, };

	if (!imgsensor_sec_check_rom_ver(position)) {
		pr_err(" NG, invalid FROM version");
		return sprintf(buf, "%s\n", "NG_VER");
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[position][CRC32_CHECK_FACTORY]) {
			if (!check_final_cam_module[position]) {
				pr_err(" NG, not final cam module");
				strcpy(command_ack, "NG_VER\n");
			} else {
				strcpy(command_ack, "OK\n");
			}
		} else {
			pr_err(" NG, crc check fail");
			strcpy(command_ack, "NG_CRC\n");
		}
	} else {
		pr_err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_VER\n");
	}

	return sprintf(buf, "%s", command_ack);
}

static ssize_t camera_info_show(int position, char *buf)
{
	char camera_info[130] = {0, };

#ifdef CONFIG_OF
	struct imgsensor_cam_info *rear_cam_info = &(cam_infos[position]);

	if(!rear_cam_info->valid) {
		strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N");

		return sprintf(buf, "%s\n", camera_info);
	} else {
		strcpy(camera_info, "ISP=");
		switch(rear_cam_info->isp) {
		case CAM_INFO_ISP_TYPE_INTERNAL:
			strncat(camera_info, "INT;", strlen("INT;"));
			break;
		case CAM_INFO_ISP_TYPE_EXTERNAL:
			strncat(camera_info, "EXT;", strlen("EXT;"));
			break;
		case CAM_INFO_ISP_TYPE_SOC:
			strncat(camera_info, "SOC;", strlen("SOC;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "CALMEM=", strlen("CALMEM="));
		switch(rear_cam_info->cal_memory) {
		case CAM_INFO_CAL_MEM_TYPE_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_CAL_MEM_TYPE_FROM:
		case CAM_INFO_CAL_MEM_TYPE_EEPROM:
		case CAM_INFO_CAL_MEM_TYPE_OTP:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "READVER=", strlen("READVER="));
		switch(rear_cam_info->read_version) {
		case CAM_INFO_READ_VER_SYSFS:
			strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
			break;
		case CAM_INFO_READ_VER_CAMON:
			strncat(camera_info, "CAMON;", strlen("CAMON;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "COREVOLT=", strlen("COREVOLT="));
		switch(rear_cam_info->core_voltage) {
		case CAM_INFO_CORE_VOLT_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_CORE_VOLT_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "UPGRADE=", strlen("UPGRADE="));
		switch(rear_cam_info->upgrade) {
		case CAM_INFO_FW_UPGRADE_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_FW_UPGRADE_SYSFS:
			strncat(camera_info, "SYSFS;", strlen("SYSFS;"));
			break;
		case CAM_INFO_FW_UPGRADE_CAMON:
			strncat(camera_info, "CAMON;", strlen("CAMON;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "FWWRITE=", strlen("FWWRITE="));
		switch(rear_cam_info->fw_write) {
		case CAM_INFO_FW_WRITE_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_FW_WRITE_OS:
			strncat(camera_info, "OS;", strlen("OS;"));
			break;
		case CAM_INFO_FW_WRITE_SD:
			strncat(camera_info, "SD;", strlen("SD;"));
			break;
		case CAM_INFO_FW_WRITE_ALL:
			strncat(camera_info, "ALL;", strlen("ALL;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "FWDUMP=", strlen("FWDUMP="));
		switch(rear_cam_info->fw_dump) {
		case CAM_INFO_FW_DUMP_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_FW_DUMP_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "CC=", strlen("CC="));
		switch(rear_cam_info->companion) {
		case CAM_INFO_COMPANION_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_COMPANION_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "OIS=", strlen("OIS="));
		switch(rear_cam_info->ois) {
		case CAM_INFO_OIS_NONE:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_OIS_USE:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		strncat(camera_info, "VALID=", strlen("VALID="));
		switch(rear_cam_info->valid) {
		case CAM_INFO_INVALID:
			strncat(camera_info, "N;", strlen("N;"));
			break;
		case CAM_INFO_VALID:
			strncat(camera_info, "Y;", strlen("Y;"));
			break;
		default:
			strncat(camera_info, "NULL;", strlen("NULL;"));
			break;
		}

		return sprintf(buf, "%s\n", camera_info);
	}
#endif

	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;"
		"FWWRITE=NULL;FWDUMP=NULL;CC=NULL;OIS=NULL;VALID=N");

	return sprintf(buf, "%s\n", camera_info);
}

static ssize_t camera_rear_sensorid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(SENSOR_POSITION_REAR, buf);
}

static ssize_t camera_rear_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_exif_show(SENSOR_POSITION_REAR, buf);
}

static ssize_t camera_rear_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(SENSOR_POSITION_REAR, buf);
}

static ssize_t camera_rear_camtype_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camtype_show(SENSOR_POSITION_REAR, buf);
}

static ssize_t camera_rear_camfw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	char command_ack[20] = {0, };

	if (!imgsensor_sec_check_rom_ver(position)) {
		pr_err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FW");
		strcat(command_ack, g_cal_data_name_by_postion[position]);
#if defined(REAR_CAMERA2)
		strcat(command_ack, g_cal_data_name_by_postion[SENSOR_POSITION_REAR2]);
#endif
		return sprintf(buf, "%s %s\n", "NULL", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER] == true) {
		bool crc32_check_fw = crc32_check_list[position][CRC32_CHECK_FW];
		bool crc32_check_factory = crc32_check_list[position][CRC32_CHECK_FACTORY];
		bool crc32_check_setfile = crc32_check_list[position][CRC32_CHECK_SETFILE];

		if (crc32_check_fw && crc32_check_factory && crc32_check_setfile
			&& crc32_check_list[SENSOR_POSITION_REAR2][CRC32_CHECK_FACTORY] == true) {
			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, finfo[position]->header_ver);
		} else {
			pr_err(" NG, crc check fail");

			strcpy(command_ack, "NG_");

			if (!crc32_check_fw)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, g_cal_data_name_by_postion[position]);
			if (!crc32_check_setfile)
				strcat(command_ack, "SET");
#if defined(REAR_CAMERA2)
			if (!crc32_check_list[SENSOR_POSITION_REAR2][CRC32_CHECK_FACTORY])
				strcat(command_ack, g_cal_data_name_by_postion[SENSOR_POSITION_REAR2]);
#endif
			if (finfo[position]->header_ver[3] != ISP_VENDOR1
				&& finfo[position]->header_ver[3] != ISP_VENDOR2)
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
		}
	} else {
		pr_err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_FW");
		strcat(command_ack, g_cal_data_name_by_postion[position]);
#if defined(REAR_CAMERA2)
		strcat(command_ack, g_cal_data_name_by_postion[SENSOR_POSITION_REAR2]);
#endif

		return sprintf(buf, "%s %s\n", finfo[position]->header_ver, command_ack);
	}
}

static ssize_t camera_rear_camfw_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int position = SENSOR_POSITION_REAR;

	if (!imgsensor_sec_check_rom_ver(position))
		return -ENODEV;

	pr_info("[FW_DBG] buf : %s\n", buf);
	scnprintf(finfo[position]->header_ver, sizeof(finfo[position]->header_ver), "%s", buf);

	return size;
}

static ssize_t camera_rear_camfw_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	char command_ack[20] = {0, };

	if (!imgsensor_sec_check_rom_ver(position)) {
		pr_err(" NG, invalid ROM version");
		strcpy(command_ack, "NG_FW");
		strcat(command_ack, g_cal_data_name_by_postion[position]);
#if defined(REAR_CAMERA2)
		strcat(command_ack, g_cal_data_name_by_postion[SENSOR_POSITION_REAR2]);
#endif
		return sprintf(buf, "%s %s %s\n", "NULL", "NULL", command_ack);
	}

	if (crc32_check_list[position][CRC32_CHECK_FW_VER] == true) {
		bool crc32_check_fw = crc32_check_list[position][CRC32_CHECK_FW];
		bool crc32_check_factory = crc32_check_list[position][CRC32_CHECK_FACTORY];
		bool crc32_check_setfile = crc32_check_list[position][CRC32_CHECK_SETFILE];

		if (crc32_check_fw && crc32_check_factory && crc32_check_setfile
			&& crc32_check_list[SENSOR_POSITION_REAR2][CRC32_CHECK_FACTORY]
		) {
			return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, finfo[position]->header_ver, finfo[position]->header_ver);
		} else {
			pr_err(" NG, crc check fail");
			strcpy(command_ack, "NG_");

			if (!crc32_check_fw)
				strcat(command_ack, "FW");
			if (!crc32_check_factory)
				strcat(command_ack, g_cal_data_name_by_postion[position]);
			if (!crc32_check_setfile)
				strcat(command_ack, "SET");
#if defined(REAR_CAMERA2)
			if (!crc32_check_list[SENSOR_POSITION_REAR2][CRC32_CHECK_FACTORY])
				strcat(command_ack, g_cal_data_name_by_postion[SENSOR_POSITION_REAR2]);
#endif
			if (finfo[position]->header_ver[3] != ISP_VENDOR1
				&& finfo[position]->header_ver[3] != ISP_VENDOR2)
				strcat(command_ack, "_Q");

			return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, finfo[position]->header_ver, command_ack);
		}
	} else {
		pr_err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_FW");
		strcat(command_ack, g_cal_data_name_by_postion[position]);
#if defined(REAR_CAMERA2)
		strcat(command_ack, g_cal_data_name_by_postion[SENSOR_POSITION_REAR2]);
#endif
		return sprintf(buf, "%s %s %s\n", finfo[position]->header_ver, finfo[position]->header_ver, command_ack);
	}
}

static ssize_t camera_rear_checkfw_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	if (!imgsensor_sec_check_rom_ver(SENSOR_POSITION_REAR)) {
		pr_err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG");
	}

	if (crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FW]
			&& crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FACTORY]
#if defined(REAR_CAMERA2)
			&& crc32_check_list[SENSOR_POSITION_REAR2][CRC32_CHECK_FACTORY]
#endif
		) {
			if (!check_latest_cam_module[SENSOR_POSITION_REAR]
#if defined(REAR_CAMERA2)
			|| !check_latest_cam_module[SENSOR_POSITION_REAR2]
#endif
			) {
				pr_err(" NG, not latest cam module");
				return sprintf(buf, "%s\n", "NG");
			} else {
				return sprintf(buf, "%s\n", "OK");
			}
		} else {
			pr_err(" NG, crc check fail");
			return sprintf(buf, "%s\n", "NG");
		}
	} else {
		pr_err(" NG, fw ver crc check fail");
		return sprintf(buf, "%s\n", "NG");
	}
}

static ssize_t camera_rear_checkfw_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char command_ack[10] = {0, };

	if (!imgsensor_sec_check_rom_ver(SENSOR_POSITION_REAR)) {
		pr_err(" NG, invalid ROM version");
		return sprintf(buf, "%s\n", "NG_VER");
	}
	if (crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FW_VER]) {
		if (crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FW]
			&& crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FACTORY]
			&& crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_SETFILE]
#if defined(REAR_CAMERA2)
			&& crc32_check_list[SENSOR_POSITION_REAR2][CRC32_CHECK_FACTORY]
#endif
		) {
			if (!check_final_cam_module[SENSOR_POSITION_REAR]
				|| !check_final_cam_module[SENSOR_POSITION_REAR2]
			) {
				pr_err(" NG, not final cam module");
				strcpy(command_ack, "NG_VER\n");
			} else {
				strcpy(command_ack, "OK\n");
			}
		} else {
			pr_err(" NG, crc check fail");
			strcpy(command_ack, "NG_CRC\n");
		}
	} else {
		pr_err(" NG, fw ver crc check fail");
		strcpy(command_ack, "NG_VER\n");
	}

	return sprintf(buf, "%s", command_ack);
}


static ssize_t camera_rear_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_info_show(SENSOR_POSITION_REAR, buf);
}

static ssize_t camera_rear_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(SENSOR_POSITION_REAR, buf);
}

static ssize_t camera_rear_afcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	if (!imgsensor_sec_check_rom_ver(position)) {
		return -ENODEV;
	}
	return sprintf(buf, "1 %d %d\n",
		finfo[position]->af_cal_macro, finfo[position]->af_cal_pan);
}

#if defined(REAR_SUB_CAMERA)
static ssize_t camera_rear_dualcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	int32_t dual_cal_data2_addr, dual_cal_data2_size;
	char *cal_buf;
	if (!imgsensor_sec_check_rom_ver(position)) {
		return -ENODEV;
	}
	imgsensor_get_cal_buf(position, &cal_buf);

	if (vendor_rom_addr[position] == NULL) {
		pr_err("[%s] fail \n", __func__);
		return -ENODEV;
	}
	dual_cal_data2_addr = vendor_rom_addr[position]->rom_dual_cal_data2_start_addr;
	dual_cal_data2_size = vendor_rom_addr[position]->rom_dual_cal_data2_size;

	if (dual_cal_data2_addr > 0 && dual_cal_data2_size > 0) {
		memcpy(buf, &cal_buf[dual_cal_data2_addr], dual_cal_data2_size);
		return dual_cal_data2_size;
	}

	return 0;
}

static ssize_t camera_rear_dualcal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	int32_t dual_cal_data2_size;

	if (!imgsensor_sec_check_rom_ver(position)) {
		return -ENODEV;
	}

	if (vendor_rom_addr[position] == NULL) {
		pr_err("[%s] fail \n", __func__);
		return -ENODEV;
	}
	dual_cal_data2_size = vendor_rom_addr[position]->rom_dual_cal_data2_size;

	if (dual_cal_data2_size > 0)
		return sprintf(buf, "%d\n", dual_cal_data2_size);
	else
		return 0;
}
static ssize_t camera_rear3_tilt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_tilt_show(buf, SENSOR_POSITION_REAR);
}
#endif

static ssize_t camera_rear_calcheck_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char rear_sensor[10] = {0, };
	char front_sensor[10] = {0, };

	if (imgsensor_sec_check_rom_ver(SENSOR_POSITION_REAR)
		&& crc32_check_list[SENSOR_POSITION_REAR][CRC32_CHECK_FACTORY])
		strcpy(rear_sensor, "Normal");
	else
		strcpy(rear_sensor, "Abnormal");

	if (imgsensor_sec_check_rom_ver(SENSOR_POSITION_FRONT)
		&& crc32_check_list[SENSOR_POSITION_FRONT][CRC32_CHECK_FACTORY])
		strcpy(front_sensor, "Normal");
	else
		strcpy(front_sensor, "Abnormal");

	return sprintf(buf, "%s %s\n", rear_sensor, front_sensor);
}

static ssize_t camera_supported_cameraIds_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp_buf[IMGSENSOR_SENSOR_COUNT];
	char *end = "\n";
	int i;

	for (i = 0; i < common_cam_infos.max_supported_camera; i++)
	{
		sprintf(temp_buf, "%d ", common_cam_infos.supported_camera_ids[i]);
		strncat(buf, temp_buf, strlen(temp_buf));
	}
	strncat(buf, end, strlen(end));

	return strlen(buf);
}


static DEVICE_ATTR(rear_sensorid, S_IRUGO, camera_rear_sensorid_show, NULL);
static DEVICE_ATTR(rear_moduleid, S_IRUGO, camera_rear_moduleid_show, NULL);
static DEVICE_ATTR(rear_caminfo, S_IRUGO, camera_rear_info_show, NULL);
static DEVICE_ATTR(rear_sensorid_exif, S_IRUGO, camera_rear_sensorid_exif_show, NULL);
static DEVICE_ATTR(rear_afcal, S_IRUGO, camera_rear_afcal_show, NULL);
#if defined(REAR_SUB_CAMERA)
static DEVICE_ATTR(rear_dualcal, S_IRUGO, camera_rear_dualcal_show, NULL);
static DEVICE_ATTR(rear_dualcal_size, S_IRUGO, camera_rear_dualcal_size_show, NULL);
#endif
static DEVICE_ATTR(rear_mtf_exif, S_IRUGO, camera_rear_mtf_exif_show, NULL);
static DEVICE_ATTR(supported_cameraIds, S_IRUGO, camera_supported_cameraIds_show, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO, camera_rear_camfw_show, camera_rear_camfw_write);
static DEVICE_ATTR(rear_camfw_full, S_IRUGO, camera_rear_camfw_full_show, NULL);
static DEVICE_ATTR(rear_checkfw_user, S_IRUGO, camera_rear_checkfw_user_show, NULL);
static DEVICE_ATTR(rear_checkfw_factory, S_IRUGO, camera_rear_checkfw_factory_show, NULL);
static DEVICE_ATTR(rear_camtype, S_IRUGO, camera_rear_camtype_show, NULL);
static DEVICE_ATTR(rear_calcheck, S_IRUGO, camera_rear_calcheck_show, NULL);
static DEVICE_ATTR(SVC_rear_module, S_IRUGO, camera_rear_moduleid_show, NULL);
static DEVICE_ATTR(ssrm_camera_info, 0644, camera_ssrm_camera_info_show, camera_ssrm_camera_info_store);


static ssize_t camera_front_sensorid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(SENSOR_POSITION_FRONT, buf);
}

static ssize_t camera_front_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_exif_show(SENSOR_POSITION_FRONT, buf);
}

static ssize_t camera_front_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(SENSOR_POSITION_FRONT, buf);
}

static ssize_t camera_front_camtype_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camtype_show(SENSOR_POSITION_FRONT, buf);
}

static ssize_t camera_front_camfw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(SENSOR_POSITION_FRONT, buf);
}

static ssize_t camera_front_camfw_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camfw_full_show(SENSOR_POSITION_FRONT, buf);
}

static ssize_t camera_front_checkfw_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_checkfw_user_show(SENSOR_POSITION_FRONT, buf);
}

static ssize_t camera_front_checkfw_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(SENSOR_POSITION_FRONT, buf);
}

static ssize_t camera_front_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_info_show(SENSOR_POSITION_FRONT, buf);
}

static ssize_t camera_front_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(SENSOR_POSITION_FRONT, buf);
}


static DEVICE_ATTR(front_sensorid, S_IRUGO, camera_front_sensorid_show, NULL);
static DEVICE_ATTR(front_moduleid, S_IRUGO, camera_front_moduleid_show, NULL);
static DEVICE_ATTR(front_caminfo, S_IRUGO, camera_front_info_show, NULL);
static DEVICE_ATTR(front_sensorid_exif, S_IRUGO, camera_front_sensorid_exif_show, NULL);
static DEVICE_ATTR(front_mtf_exif, S_IRUGO, camera_front_mtf_exif_show, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO, camera_front_camfw_show, NULL);
static DEVICE_ATTR(front_camfw_full, S_IRUGO, camera_front_camfw_full_show, NULL);
static DEVICE_ATTR(front_camtype, S_IRUGO, camera_front_camtype_show, NULL);
static DEVICE_ATTR(front_checkfw_user, S_IRUGO, camera_front_checkfw_user_show, NULL);
static DEVICE_ATTR(front_checkfw_factory, S_IRUGO, camera_front_checkfw_factory_show, NULL);
static DEVICE_ATTR(SVC_front_module, S_IRUGO, camera_front_moduleid_show, NULL);

static ssize_t camera_rear2_sensorid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(SENSOR_POSITION_REAR2, buf);
}

static ssize_t camera_rear2_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_exif_show(SENSOR_POSITION_REAR2, buf);
}

static ssize_t camera_rear2_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(SENSOR_POSITION_REAR2, buf);
}


static ssize_t camera_rear2_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(SENSOR_POSITION_REAR2, buf);
}

static ssize_t camera_rear2_camtype_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camtype_show(SENSOR_POSITION_REAR2, buf);
}

static ssize_t camera_rear2_camfw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(SENSOR_POSITION_REAR2, buf);
}

static ssize_t camera_rear2_camfw_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camfw_full_show(SENSOR_POSITION_REAR2, buf);
}

static ssize_t camera_rear2_checkfw_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_checkfw_user_show(SENSOR_POSITION_REAR2, buf);
}

static ssize_t camera_rear2_checkfw_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(SENSOR_POSITION_REAR2, buf);
}

static ssize_t camera_rear2_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_info_show(SENSOR_POSITION_REAR2, buf);
}

static DEVICE_ATTR(rear2_sensorid, S_IRUGO, camera_rear2_sensorid_show, NULL);
static DEVICE_ATTR(rear2_moduleid, S_IRUGO, camera_rear2_moduleid_show, NULL);
static DEVICE_ATTR(rear2_caminfo, S_IRUGO, camera_rear2_info_show, NULL);
static DEVICE_ATTR(rear2_sensorid_exif, S_IRUGO, camera_rear2_sensorid_exif_show, NULL);
static DEVICE_ATTR(rear2_mtf_exif, S_IRUGO, camera_rear2_mtf_exif_show, NULL);
static DEVICE_ATTR(rear2_camfw, S_IRUGO, camera_rear2_camfw_show, NULL);
static DEVICE_ATTR(rear2_camfw_full, S_IRUGO, camera_rear2_camfw_full_show, NULL);
static DEVICE_ATTR(rear2_checkfw_user, S_IRUGO, camera_rear2_checkfw_user_show, NULL);
static DEVICE_ATTR(rear2_checkfw_factory, S_IRUGO, camera_rear2_checkfw_factory_show, NULL);
static DEVICE_ATTR(rear2_camtype, S_IRUGO, camera_rear2_camtype_show, NULL);
static DEVICE_ATTR(SVC_rear_module2, S_IRUGO, camera_rear2_moduleid_show, NULL);

static ssize_t camera_rear3_sensorid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(SENSOR_POSITION_REAR3, buf);
}

static ssize_t camera_rear3_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_exif_show(SENSOR_POSITION_REAR3, buf);
}

#if NOTYETDONE
static ssize_t camera_rear3_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(SENSOR_POSITION_REAR3, buf);
}
#endif

#if defined(REAR_SUB_CAMERA)
static ssize_t camera_rear3_dualcal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	int32_t dual_cal_data2_addr, dual_cal_data2_size;
	char *cal_buf;

	if (!imgsensor_sec_check_rom_ver(position)) {
		return -ENODEV;
	}
	imgsensor_get_cal_buf(position, &cal_buf);

	if (vendor_rom_addr[position] == NULL) {
		pr_err("[%s] fail \n", __func__);
		return -ENODEV;
	}
	dual_cal_data2_addr = vendor_rom_addr[position]->rom_dual_cal_data2_start_addr;
	dual_cal_data2_size = vendor_rom_addr[position]->rom_dual_cal_data2_size;

	if (dual_cal_data2_addr > 0 && dual_cal_data2_size > 0) {
		memcpy(buf, &cal_buf[dual_cal_data2_addr], dual_cal_data2_size);
		return dual_cal_data2_size;
	}

	return 0;
}

static ssize_t camera_rear3_dualcal_size_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	int32_t dual_cal_data2_size;

	if (!imgsensor_sec_check_rom_ver(position)) {
		return -ENODEV;
	}

	if (vendor_rom_addr[position] == NULL) {
		pr_err("[%s] fail \n", __func__);
		return -ENODEV;
	}
	dual_cal_data2_size = vendor_rom_addr[position]->rom_dual_cal_data2_size;

	if (dual_cal_data2_size > 0)
		return sprintf(buf, "%d\n", dual_cal_data2_size);
	else
		return 0;
}
#endif

#ifndef USE_SHARED_ROM_REAR3
static ssize_t camera_rear3_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(SENSOR_POSITION_REAR3, buf);
}
#endif

static ssize_t camera_rear3_camtype_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camtype_show(SENSOR_POSITION_REAR3, buf);
}

static ssize_t camera_rear3_camfw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(SENSOR_POSITION_REAR3, buf);
}

static ssize_t camera_rear3_camfw_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camfw_full_show(SENSOR_POSITION_REAR3, buf);
}

static ssize_t camera_rear3_checkfw_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_checkfw_user_show(SENSOR_POSITION_REAR3, buf);
}

static ssize_t camera_rear3_checkfw_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(SENSOR_POSITION_REAR3, buf);
}

static ssize_t camera_rear3_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_info_show(SENSOR_POSITION_REAR3, buf);
}

static DEVICE_ATTR(rear3_sensorid, S_IRUGO, camera_rear3_sensorid_show, NULL);
#ifndef USE_SHARED_ROM_REAR3
static DEVICE_ATTR(rear3_moduleid, S_IRUGO, camera_rear3_moduleid_show, NULL);
static DEVICE_ATTR(SVC_rear_module3, S_IRUGO, camera_rear3_moduleid_show, NULL);
#endif
static DEVICE_ATTR(rear3_caminfo, S_IRUGO, camera_rear3_info_show, NULL);
static DEVICE_ATTR(rear3_sensorid_exif, S_IRUGO, camera_rear3_sensorid_exif_show, NULL);
#if defined(REAR_SUB_CAMERA)
static DEVICE_ATTR(rear3_dualcal, S_IRUGO, camera_rear3_dualcal_show, NULL);
static DEVICE_ATTR(rear3_dualcal_size, S_IRUGO, camera_rear3_dualcal_size_show, NULL);
#endif
static DEVICE_ATTR(rear3_camfw, S_IRUGO, camera_rear3_camfw_show, NULL);
static DEVICE_ATTR(rear3_camfw_full, S_IRUGO, camera_rear3_camfw_full_show, NULL);
static DEVICE_ATTR(rear3_checkfw_user, S_IRUGO, camera_rear3_checkfw_user_show, NULL);
static DEVICE_ATTR(rear3_checkfw_factory, S_IRUGO, camera_rear3_checkfw_factory_show, NULL);
static DEVICE_ATTR(rear3_camtype, S_IRUGO, camera_rear3_camtype_show, NULL);
static DEVICE_ATTR(rear3_tilt, S_IRUGO, camera_rear3_tilt_show, NULL);
#if NOTYETDONE
static DEVICE_ATTR(rear3_mtf_exif, S_IRUGO, camera_rear3_mtf_exif_show, NULL);
#endif

#if defined(REAR_CAMERA4)
static ssize_t camera_rear4_sensorid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(SENSOR_POSITION_REAR4, buf);
}

static ssize_t camera_rear4_sensorid_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_sensorid_exif_show(SENSOR_POSITION_REAR4, buf);
}

static ssize_t camera_rear4_mtf_exif_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_mtf_exif_show(SENSOR_POSITION_REAR4, buf);
}

static ssize_t camera_rear4_moduleid_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(SENSOR_POSITION_REAR4, buf);
}

static ssize_t camera_rear4_camtype_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camtype_show(SENSOR_POSITION_REAR4, buf);
}

static ssize_t camera_rear4_camfw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camfw_show(SENSOR_POSITION_REAR4, buf);
}

static ssize_t camera_rear4_camfw_full_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_camfw_full_show(SENSOR_POSITION_REAR4, buf);
}

static ssize_t camera_rear4_checkfw_user_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_checkfw_user_show(SENSOR_POSITION_REAR4, buf);
}

static ssize_t camera_rear4_checkfw_factory_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_checkfw_factory_show(SENSOR_POSITION_REAR4, buf);
}

static ssize_t camera_rear4_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return camera_info_show(SENSOR_POSITION_REAR4, buf);
}

static DEVICE_ATTR(rear4_sensorid, S_IRUGO, camera_rear4_sensorid_show, NULL);
static DEVICE_ATTR(rear4_moduleid, S_IRUGO, camera_rear4_moduleid_show, NULL);
static DEVICE_ATTR(rear4_caminfo, S_IRUGO, camera_rear4_info_show, NULL);
static DEVICE_ATTR(rear4_sensorid_exif, S_IRUGO, camera_rear4_sensorid_exif_show, NULL);
static DEVICE_ATTR(rear4_mtf_exif, S_IRUGO, camera_rear4_mtf_exif_show, NULL);
static DEVICE_ATTR(rear4_camfw, S_IRUGO, camera_rear4_camfw_show, NULL);
static DEVICE_ATTR(rear4_camfw_full, S_IRUGO, camera_rear4_camfw_full_show, NULL);
static DEVICE_ATTR(rear4_checkfw_user, S_IRUGO, camera_rear4_checkfw_user_show, NULL);
static DEVICE_ATTR(rear4_checkfw_factory, S_IRUGO, camera_rear4_checkfw_factory_show, NULL);
static DEVICE_ATTR(rear4_camtype, S_IRUGO, camera_rear4_camtype_show, NULL);
static DEVICE_ATTR(SVC_rear_module4, S_IRUGO, camera_rear4_moduleid_show, NULL);
#endif//REAR_CAMERA4


static ssize_t rear_camera_hw_param_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	struct cam_hw_param *ec_param = NULL;

	imgsensor_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
			pr_err("failed to get hw param");
			return 0;
	}

	if (imgsensor_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
			return sprintf(buf, "\"CAMIR_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR_AF\":\"%d\",\"I2CR_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
					finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
					finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
					finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
					ec_param->i2c_af_err_cnt, ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
			return sprintf(buf, "\"CAMIR_ID\":\"MIR_ERR\",\"I2CR_AF\":\"%d\",\"I2CR_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					ec_param->i2c_af_err_cnt, ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t rear_camera_hw_param_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;

	if (!strncmp(buf, "c", 1)) {
			imgsensor_sec_get_hw_param(&ec_param, SENSOR_POSITION_REAR);

			if (ec_param)
					imgsensor_sec_init_err_cnt(ec_param);
	}

	return count;
}

static ssize_t front_camera_hw_param_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_FRONT;
	struct cam_hw_param *ec_param = NULL;

	imgsensor_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
			pr_err("failed to get hw param");
			return 0;
	}

	if (imgsensor_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
			return sprintf(buf, "\"CAMIF_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CF_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
					finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
					finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
					finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
			return sprintf(buf, "\"CAMIF_ID\":\"MIR_ERR\",\"I2CF_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t front_camera_hw_param_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct cam_hw_param *ec_param = NULL;

	if (!strncmp(buf, "c", 1)) {
			imgsensor_sec_get_hw_param(&ec_param, SENSOR_POSITION_FRONT);

			if (ec_param)
					imgsensor_sec_init_err_cnt(ec_param);
	}

	return count;
}

static ssize_t rear2_camera_hw_param_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR2;
	struct cam_hw_param *ec_param = NULL;

	imgsensor_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
			pr_err("failed to get hw param");
			return 0;
	}

	if (imgsensor_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
			return sprintf(buf, "\"CAMIR2_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR2_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
					finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
					finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
					finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
			return sprintf(buf, "\"CAMIR2_ID\":\"MIR_ERR\",\"I2CR2_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t rear2_camera_hw_param_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int position = SENSOR_POSITION_REAR2;
	struct cam_hw_param *ec_param = NULL;
	if (!strncmp(buf, "c", 1)) {
			imgsensor_sec_get_hw_param(&ec_param, position);

			if (ec_param)
					imgsensor_sec_init_err_cnt(ec_param);
	}

	return count;
}

static ssize_t rear3_camera_hw_param_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR;
	struct cam_hw_param *ec_param = NULL;

	imgsensor_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
			pr_err("failed to get hw param");
			return 0;
	}

	if (imgsensor_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
			return sprintf(buf, "\"CAMIR3_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR3_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
					finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
					finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
					finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
			return sprintf(buf, "\"CAMIR3_ID\":\"MIR_ERR\",\"I2CR3_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t rear3_camera_hw_param_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int position = SENSOR_POSITION_REAR3;
	struct cam_hw_param *ec_param = NULL;
	if (!strncmp(buf, "c", 1)) {
			imgsensor_sec_get_hw_param(&ec_param, position);

			if (ec_param)
					imgsensor_sec_init_err_cnt(ec_param);
	}

	return count;
}

#ifdef REAR_CAMERA4
static ssize_t rear4_camera_hw_param_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int position = SENSOR_POSITION_REAR4;
	struct cam_hw_param *ec_param = NULL;

	imgsensor_sec_get_hw_param(&ec_param, position);
	if (!ec_param) {
			pr_err("failed to get hw param");
			return 0;
	}

	if (imgsensor_sec_is_valid_moduleid(finfo[position]->rom_module_id)) {
			return sprintf(buf, "\"CAMIR4_ID\":\"%c%c%c%c%cXX%02X%02X%02X\",\"I2CR4_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					finfo[position]->rom_module_id[0], finfo[position]->rom_module_id[1],
					finfo[position]->rom_module_id[2], finfo[position]->rom_module_id[3],
					finfo[position]->rom_module_id[4], finfo[position]->rom_module_id[7],
					finfo[position]->rom_module_id[8], finfo[position]->rom_module_id[9],
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	} else {
			return sprintf(buf, "\"CAMIR4_ID\":\"MIR_ERR\",\"I2CR4_SEN\":\"%d\","
		"\"MIPIR_SEN\":\"%d\"\n",
					ec_param->i2c_sensor_err_cnt, ec_param->mipi_sensor_err_cnt);
	}
}

static ssize_t rear4_camera_hw_param_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	int position = SENSOR_POSITION_REAR4;
	struct cam_hw_param *ec_param = NULL;
	if (!strncmp(buf, "c", 1)) {
			imgsensor_sec_get_hw_param(&ec_param, position);

			if (ec_param)
					imgsensor_sec_init_err_cnt(ec_param);
	}

	return count;
}
#endif//REAR_CAMERA4

static DEVICE_ATTR(rear_hwparam,   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
					rear_camera_hw_param_show,   rear_camera_hw_param_store);

static DEVICE_ATTR(front_hwparam,  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
					front_camera_hw_param_show,  front_camera_hw_param_store);

static DEVICE_ATTR(rear2_hwparam,  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
					rear2_camera_hw_param_show,  rear2_camera_hw_param_store);
static DEVICE_ATTR(rear3_hwparam,  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
					rear3_camera_hw_param_show, rear3_camera_hw_param_store);
#ifdef REAR_CAMERA4
static DEVICE_ATTR(rear4_hwparam,  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
					rear4_camera_hw_param_show, rear4_camera_hw_param_store);
#endif//REAR_CAMERA4

#if ENABLE_CAM_CAL_DUMP
ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos)
{
	struct file *fp;
	mm_segment_t old_fs;
	ssize_t tx = -ENOENT;
	int fd, old_mask;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	old_mask = sys_umask(0);

	sys_rmdir(name);
	pr_err("delete done: %s", name);
	fd = sys_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);

	if (fd < 0) {
		pr_err("open file error: %s", name);
		sys_umask(old_mask);
		set_fs(old_fs);
		return -EINVAL;
	} else {
		pr_err("open file success: %s", name);
	}

	fp = fget(fd);
	if (fp) {
		pr_err("get file success %s", name);
		tx = vfs_write(fp, buf, count, pos);
		if (tx != count) {
			pr_err("fail to write %s. ret %zd", name, tx);
			tx = -ENOENT;
		} else {
			pr_err("write success %s", name);
		}

		vfs_fsync(fp, 0);
		fput(fp);
	} else {
		pr_err("fail to get file *: %s", name);
	}

	sys_close(fd);
	sys_umask(old_mask);
	set_fs(old_fs);

	return tx;
}

bool imgsensor_sec_readcal_dump(char *buf, int size, int position)
{
	int ret = false;
	struct file *dump_fp = NULL;
	mm_segment_t old_fs;
	loff_t pos = 0;

	char path[50] = IMGSENSOR_CAL_SDCARD_PATH;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	dump_fp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(dump_fp)) {
		pr_err("dump folder does not exist.\n");
		dump_fp = NULL;
		goto dump_err;
	}


	pr_info("dump folder exist, Dump EEPROM cal data.\n");

	strcat(path, eeprom_cal_dump_path[position]);
	if (write_data_to_file(path, buf, size, &pos) < 0) {
		pr_err("Failed to rear dump cal data.\n");
		goto dump_err;
	}
	ret = true;

dump_err:
	if (dump_fp)
		filp_close(dump_fp, current->files);
	set_fs(old_fs);
	return ret;
}
#endif

static void *imgsensor_sec_search_rom_extend_data(const struct rom_extend_cal_addr *extend_data, char *name)
{
	void *ret = NULL;

	const struct rom_extend_cal_addr *cur;
	cur = extend_data;

	while (cur != NULL) {
		if (!strcmp(cur->name, name)) {
			if (cur->data != NULL) {
				ret = (void *)cur->data;
			} else {
				pr_warn("[%s] : Found -> %s, but no data \n", __func__, cur->name);
				ret = NULL;
			}
			break;
		}
		cur = cur->next;
	}

	return ret;
}

bool compare_crc_checksum(char *buf, int device_idx, int address_boundary, struct cal_crc_info crc_info)
{
	u32 checksumFromRom, checksum;

	if (crc_info.cal_len < 0 || crc_info.checksum_addr < 0 || crc_info.cal_start_addr < 0) {
		pr_warn("Camera[%d] Skip to check %s crc32", device_idx, crc_info.cal_name);
		return true;
	}

	if (crc_info.checksum_addr >= address_boundary || crc_info.cal_start_addr >= address_boundary) {
		pr_err("Camera[%d] %s CRC address has error: 0x%08X, 0x%08X",
				device_idx, crc_info.cal_name, crc_info.checksum_addr, crc_info.cal_start_addr);
		return false;
	}

#ifdef ROM_CRC32_DEBUG
	pr_info("[CRC32_DEBUG] %s CRC Check. check_length = %d, cal_start_addr = 0x%08X, checksum_addr = 0x%08X\n",
			crc_info.cal_name, crc_info.cal_len, crc_info.cal_start_addr, crc_info.checksum_addr);
#endif

	checksumFromRom = buf[crc_info.checksum_addr] + (buf[crc_info.checksum_addr + 1] << 8)
					+ (buf[crc_info.checksum_addr + 2] << 16) + (buf[crc_info.checksum_addr + 3] << 24);

	if (checksumFromRom == 0) {
		pr_err("Camera[%d]: %s CRC32 error (checksum: 0)", device_idx, crc_info.cal_name);
		return false;
	}
	checksum = (u32)getCRC((u16 *)&buf[crc_info.cal_start_addr], crc_info.cal_len, NULL, NULL);
	if (checksum != checksumFromRom) {
		pr_err("Camera[%d]: %s CRC32 error (0x%08X != 0x%08X)", device_idx, crc_info.cal_name, checksum, checksumFromRom);
		return false;
	}
	pr_debug("Camera[%d]: End checking %s CRC32 (0x%08X = 0x%08X)", device_idx, crc_info.cal_name, checksum, checksumFromRom);

	return true;
}

static bool imgsensor_check_eeprom_crc32(char *buf, int deviceIdx, int sub_deviceIdx, bool rom_common)
{
	u32 address_boundary;
	int i;
	bool crc32_check_temp, crc32_header_temp;
	struct cal_crc_info crc_info;

	const struct imgsensor_vendor_rom_addr *rom_addr;
	struct imgsensor_rom_info *finfo_local = NULL;

	if (vendor_rom_addr[deviceIdx] == NULL) {
		pr_err("[%s] fail \n", __func__);
		return false;
	}
	rom_addr = vendor_rom_addr[deviceIdx];

	pr_info("%s E\n", __func__);
	/***** Initial Value *****/
	for (i = CRC32_CHECK_HEADER; i < CRC32_SCENARIO_MAX; i++ ) {
		crc32_check_list[sub_deviceIdx][i] = true;
	}
	crc32_check_temp = true;
	crc32_header_temp = true;

	/***** SKIP CHECK CRC *****/
#ifdef SKIP_CHECK_CRC
	pr_warn("Camera[%d]: Skip check crc32\n", sub_deviceIdx);

	crc32_check_temp = true;
	crc32_check_list[sub_deviceIdx][CRC32_CHECK] = crc32_check_temp;
	return crc32_check_temp;
#endif

	/***** START CHECK CRC *****/
	finfo_local = finfo[sub_deviceIdx];
	address_boundary = rom_addr->rom_max_cal_size;

	/* HEADER DATA CRC CHECK */
	crc_info.cal_name = "Header";
	crc_info.checksum_addr = finfo_local->header_section_crc_addr;
	crc_info.cal_start_addr = 0;
	crc_info.cal_len = rom_addr->rom_header_checksum_len;

	crc32_header_temp = compare_crc_checksum(buf, deviceIdx, address_boundary, crc_info);
	if (!crc32_header_temp)
		goto out;

	/* OEM Cal Data CRC CHECK */

	crc_info.cal_name = "OEM";
	crc_info.checksum_addr = finfo_local->oem_section_crc_addr;
	crc_info.cal_start_addr = finfo_local->oem_start_addr;
	crc_info.cal_len = (rom_common) ? rom_addr->rom_sub_oem_checksum_len : rom_addr->rom_oem_checksum_len;

	if (rom_addr->extend_cal_addr) {
		int32_t *addr = (int32_t *)imgsensor_sec_search_rom_extend_data(rom_addr->extend_cal_addr, EXTEND_OEM_CHECKSUM);

		if (addr != NULL && finfo_local->oem_start_addr != *addr)
			crc_info.cal_start_addr = *addr;
	}

	crc32_check_temp = compare_crc_checksum(buf, deviceIdx, address_boundary, crc_info);
	if (!crc32_check_temp)
		goto out;

	/* AWB Cal Data CRC CHECK */
	crc_info.cal_name = "AWB";
	crc_info.checksum_addr = finfo_local->awb_section_crc_addr;
	crc_info.cal_start_addr = finfo_local->awb_start_addr;
	crc_info.cal_len = (rom_common) ? rom_addr->rom_sub_awb_checksum_len : rom_addr->rom_awb_checksum_len;

	crc32_check_temp = compare_crc_checksum(buf, deviceIdx, address_boundary, crc_info);
	if (!crc32_check_temp)
		goto out;

	/* Shading Cal Data CRC CHECK*/
	crc_info.cal_name = "Shading";
	crc_info.checksum_addr = finfo_local->shading_section_crc_addr;
	crc_info.cal_start_addr = finfo_local->shading_start_addr;
	crc_info.cal_len = (rom_common) ? rom_addr->rom_sub_shading_checksum_len : rom_addr->rom_shading_checksum_len;

	crc32_check_temp = compare_crc_checksum(buf, deviceIdx, address_boundary, crc_info);
	if (!crc32_check_temp)
		goto out;

	/* DUAL Cal Data CRC CHECK */
	crc_info.cal_name = "DUAL";
	crc_info.checksum_addr = finfo_local->dual_data_section_crc_addr;
	crc_info.cal_start_addr = finfo_local->dual_data_start_addr;
	crc_info.cal_len = rom_addr->rom_dual_checksum_len;

	crc32_check_temp = compare_crc_checksum(buf, deviceIdx, address_boundary, crc_info);
	if (!crc32_check_temp)
		goto out;

	/* SENSOR Cal Data CRC CHECK */
	crc_info.cal_name = "Cal Data";
	crc_info.checksum_addr = finfo_local->sensor_cal_data_section_crc_addr;
	crc_info.cal_start_addr = finfo_local->sensor_cal_data_start_addr;
	crc_info.cal_len = rom_addr->rom_sensor_cal_checksum_len;

	crc32_check_temp = compare_crc_checksum(buf, deviceIdx, address_boundary, crc_info);
	if (!crc32_check_temp)
		goto out;

	/* PDAF Cal Data CRC CHECK */
	crc_info.cal_name = "PDAF";
	crc_info.checksum_addr = finfo_local->ap_pdaf_section_crc_addr;
	crc_info.cal_start_addr = finfo_local->ap_pdaf_start_addr;
	crc_info.cal_len = rom_addr->rom_pdaf_checksum_len;

	crc32_check_temp = compare_crc_checksum(buf, deviceIdx, address_boundary, crc_info);
	if (!crc32_check_temp)
		goto out;

out:
	crc32_check_list[sub_deviceIdx][CRC32_CHECK] = crc32_check_temp;
	crc32_check_list[sub_deviceIdx][CRC32_CHECK_HEADER] = crc32_header_temp;
	pr_info("Camera[%d]: CRC32 Check Result - crc32_header_check=%d, crc32_check=%d\n",
		sub_deviceIdx, crc32_header_temp, crc32_check_temp);

	return crc32_check_temp && crc32_header_temp;
}

bool imgsensor_check_otp_crc32(char *buf, int deviceIdx)
{
	int i;
	bool crc32_temp = true, crc32_header_temp = true;

	struct cal_crc_info crc_info;
	int otp_header_cal_start_addr = 0;
	int otp_header_checksum_addr = 0;
	int otp_header_checksum_len = 0;
	int otp_module_cal_start_addr = 0;
	int otp_module_checksum_addr = 0;
	int otp_module_checksum_len = 0;

	u32 address_boundary = 0;

	const struct imgsensor_vendor_rom_addr *rom_addr;

	if (vendor_rom_addr[deviceIdx] == NULL) {
		pr_err("[%s] fail, Rom addr is NULL \n", __func__);
		return false;
	}
	rom_addr = vendor_rom_addr[deviceIdx];

	otp_header_cal_start_addr = rom_addr->rom_header_cal_data_start_addr;
	otp_header_checksum_addr  = rom_addr->rom_header_checksum_addr;
	otp_header_checksum_len   = rom_addr->rom_header_checksum_len;
	otp_module_cal_start_addr    = rom_addr->rom_module_cal_data_start_addr;
	otp_module_checksum_addr     = rom_addr->rom_module_checksum_addr;
	otp_module_checksum_len      = rom_addr->rom_module_checksum_len;
	address_boundary = rom_addr->rom_max_cal_size;

	/***** Initial Value *****/
	for (i = CRC32_CHECK_HEADER; i < CRC32_SCENARIO_MAX; i++ ) {
		crc32_check_list[deviceIdx][i] = true;
	}

	/***** SKIP CHECK CRC *****/
#ifdef SKIP_CHECK_CRC
	pr_warn("Camera[%d]: Skip check crc32\n", deviceIdx);
	return true;
#endif

	/*************************** HEADER checksum ***************************/
	crc_info.cal_name = "Header";
	crc_info.checksum_addr = otp_header_checksum_addr;
	crc_info.cal_start_addr = otp_header_cal_start_addr;
	crc_info.cal_len = otp_header_checksum_len;

	crc32_header_temp = compare_crc_checksum(buf, deviceIdx, address_boundary, crc_info);

	/*************************** Module data checksum ***************************/
	crc_info.cal_name = "Module data";
	crc_info.checksum_addr = otp_module_checksum_addr;
	crc_info.cal_start_addr = otp_module_cal_start_addr;
	crc_info.cal_len = otp_module_checksum_len;

	crc32_temp = compare_crc_checksum(buf, deviceIdx, address_boundary, crc_info);

	crc32_check_list[deviceIdx][CRC32_CHECK] = crc32_temp;
	crc32_check_list[deviceIdx][CRC32_CHECK_HEADER] = crc32_header_temp;
	pr_info("[%s] Camera[%d]: OTP CRC32 Check Result - crc32_header_check=%d, crc32_check=%d\n",
			__func__, deviceIdx, crc32_header_temp, crc32_temp);

	return crc32_header_temp && crc32_temp;
}

static int imgsensor_rom_read_common(unsigned char *pRomData, struct imgsensor_eeprom_read_info *info)
{
	const struct imgsensor_vendor_rom_addr *rom_addr = NULL;
	int32_t temp_start_addr = 0, temp_end_addr = 0;
	int retry = IMGSENSOR_CAL_RETRY_CNT;
	int max_num_of_rom_info = 0, i = 0;
	struct imgsensor_rom_info *finfo_local;
	int ret = 0;

	if (info == NULL) {
		pr_err("[%s] fail, info is NULL\n", __func__);
		return -1;
	}

	pr_info("[%s] sensor position-> %d, sub sensor position -> %d, rom_common -> %s", __func__,
		info->sensor_position, info->sub_sensor_position, info->use_common_eeprom?"true":"false");

	if (!finfo[info->sub_sensor_position]) {
		finfo[info->sub_sensor_position] = (struct imgsensor_rom_info *)kmalloc(sizeof(struct imgsensor_rom_info), GFP_KERNEL);
	}
	finfo_local = finfo[info->sub_sensor_position];

	max_num_of_rom_info = ARRAY_SIZE(vendor_rom_info);
	pr_info("[%s]max_num_of_rom_info: %d \n", __func__, max_num_of_rom_info);

	for (i = 0; i <= max_num_of_rom_info; i++) {
		if (i == max_num_of_rom_info) {
			pr_err("[%s] fail, no searched rom_info \n", __func__);
			return -1;
		}

		if (vendor_rom_info[i].rom_addr != NULL &&
			vendor_rom_info[i].sensor_position == info->sensor_position &&
			vendor_rom_info[i].sensor_id_with_rom == info->sensor_id) {

			rom_addr = vendor_rom_info[i].rom_addr;
			vendor_rom_addr[info->sensor_position] = rom_addr;
			pr_info("[%s] sensor position: %d, sensor_id: %#06x \n", __func__,
					info->sensor_position, info->sensor_id);
			break;
		}
	}

crc_retry:
/////////////////////////////////////////////////////////////////////////////////////
/* Header Data */
/////////////////////////////////////////////////////////////////////////////////////
	/* Header Data: OEM */
	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_header_sub_oem_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_oem_start_addr;
			temp_end_addr = rom_addr->rom_header_sub_oem_end_addr;
		}
	} else {
		if (rom_addr->rom_header_main_oem_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_oem_start_addr;
			temp_end_addr = rom_addr->rom_header_main_oem_end_addr;
		}
	}

	if (temp_start_addr >= 0) {
		finfo_local->oem_start_addr = *((u32 *)&pRomData[temp_start_addr]);
		finfo_local->oem_end_addr = *((u32 *)&pRomData[temp_end_addr]);
		pr_debug("OEM start = 0x%08x, end = 0x%08x\n", finfo_local->oem_start_addr, finfo_local->oem_end_addr);
	}

	/* Header Data: AWB */
	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_header_sub_awb_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_awb_start_addr;
			temp_end_addr = rom_addr->rom_header_sub_awb_end_addr;
		}
	} else {
		if (rom_addr->rom_header_main_awb_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_awb_start_addr;
			temp_end_addr = rom_addr->rom_header_main_awb_end_addr;
		}
	}

	if (temp_start_addr >= 0) {
		finfo_local->awb_start_addr = *((u32 *)&pRomData[temp_start_addr]);
		finfo_local->awb_end_addr = *((u32 *)&pRomData[temp_end_addr]);
		pr_debug("AWB start = 0x%08x, end = 0x%08x\n", finfo_local->awb_start_addr, finfo_local->awb_end_addr);
	}

	/* Header Data: Shading */
	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_header_sub_shading_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_shading_start_addr;
			temp_end_addr = rom_addr->rom_header_sub_shading_end_addr;
		}
	} else {
		if (rom_addr->rom_header_main_shading_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_shading_start_addr;
			temp_end_addr = rom_addr->rom_header_main_shading_end_addr;
		}
	}

	if (temp_start_addr >= 0) {
		finfo_local->shading_start_addr = *((u32 *)&pRomData[temp_start_addr]);
		finfo_local->shading_end_addr = *((u32 *)&pRomData[temp_end_addr]);
		pr_debug("Shading start = 0x%08x, end = 0x%08x\n", finfo_local->shading_start_addr, finfo_local->shading_end_addr);
		//if (finfo_local->shading_end_addr > 0x3AFF) {
		//	pr_err("Shading end_addr has error!! 0x%08x", finfo_local->shading_end_addr);
		//	finfo_local->shading_end_addr = 0x3AFF;
		//}
	}

	/* Header Data: Sensor CAL (CrossTalk & LSC) */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_main_sensor_cal_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_main_sensor_cal_start_addr;
		temp_end_addr = rom_addr->rom_header_main_sensor_cal_end_addr;
	}

	if (temp_start_addr >= 0) {
		finfo_local->sensor_cal_data_start_addr = *((u32 *)&pRomData[temp_start_addr]);
		finfo_local->sensor_cal_data_end_addr = *((u32 *)&pRomData[temp_end_addr]);
		pr_debug("Sensor Cal Data start = 0x%08x, end = 0x%08x\n",
			finfo_local->sensor_cal_data_start_addr, finfo_local->sensor_cal_data_end_addr);
	}

	/* Header Data: Dual CAL */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_dual_cal_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_dual_cal_start_addr;
		temp_end_addr = rom_addr->rom_header_dual_cal_end_addr;
	}

	if (temp_start_addr >= 0) {
		finfo_local->dual_data_start_addr = *((u32 *)&pRomData[temp_start_addr]);
		finfo_local->dual_data_end_addr = *((u32 *)&pRomData[temp_end_addr]);
		pr_debug("Dual Cal Data start = 0x%08x, end = 0x%08x\n", finfo_local->dual_data_start_addr, finfo_local->dual_data_end_addr);
	}

	/* Header Data: Dual CAL */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_pdaf_cal_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_pdaf_cal_start_addr;
		temp_end_addr = rom_addr->rom_header_pdaf_cal_end_addr;
	}

	if (temp_start_addr >= 0) {
		finfo_local->ap_pdaf_start_addr = *((u32 *)&pRomData[temp_start_addr]);
		finfo_local->ap_pdaf_end_addr = *((u32 *)&pRomData[temp_end_addr]);
		pr_debug("PDAF Cal Data start = 0x%08x, end = 0x%08x\n", finfo_local->ap_pdaf_start_addr, finfo_local->ap_pdaf_end_addr);
	}

	/* Header Data: Header Module Info */
	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_header_sub_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_module_info_start_addr;
		}
	} else {
		if (rom_addr->rom_header_main_module_info_start_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_module_info_start_addr;
		}
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo_local->header_ver, &pRomData[temp_start_addr], IMGSENSOR_HEADER_VER_SIZE);
		finfo_local->header_ver[IMGSENSOR_HEADER_VER_SIZE] = '\0';
		pr_info("Header version: %s \n", finfo_local->header_ver);
	}

	/* Header Data: CAL MAP Version */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_cal_map_ver_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_cal_map_ver_start_addr;

		memcpy(finfo_local->cal_map_ver, &pRomData[temp_start_addr], IMGSENSOR_CAL_MAP_VER_SIZE);
		finfo_local->cal_map_ver[IMGSENSOR_CAL_MAP_VER_SIZE] = '\0';
		pr_info("Cal map version: %s \n", finfo_local->cal_map_ver);
	}

	/* Header Data: PROJECT NAME */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_project_name_start_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_project_name_start_addr;

		memcpy(finfo_local->project_name, &pRomData[temp_start_addr], IMGSENSOR_PROJECT_NAME_SIZE);
		finfo_local->project_name[IMGSENSOR_PROJECT_NAME_SIZE] = '\0';
	}

	/* Header Data: MODULE ID */
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_header_module_id_addr >= 0) {
		temp_start_addr = rom_addr->rom_header_module_id_addr;

		memcpy(finfo_local->rom_module_id, &pRomData[temp_start_addr], IMGSENSOR_MODULE_ID_SIZE);
	} else {
		memset(finfo_local->rom_module_id, 0x0, IMGSENSOR_MODULE_ID_SIZE);
	}

	/* Header Data: SENSOR ID */
	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_header_sub_sensor_id_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_sensor_id_addr;
		}
	} else {
		if (rom_addr->rom_header_main_sensor_id_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_sensor_id_addr;
		}
	}

	if (temp_start_addr >= 0) {
		memcpy(finfo_local->rom_sensor_id, &pRomData[temp_start_addr], IMGSENSOR_SENSOR_ID_SIZE);
		finfo_local->rom_sensor_id[IMGSENSOR_SENSOR_ID_SIZE] = '\0';
	}

	/* Header Data: MTF Data (Resolution) */
	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_header_sub_mtf_data_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_sub_mtf_data_addr;
		}
	} else {
		if (rom_addr->rom_header_main_mtf_data_addr >= 0) {
			temp_start_addr = rom_addr->rom_header_main_mtf_data_addr;
		}
	}

	if (temp_start_addr >= 0)
			finfo_local->mtf_data_addr = temp_start_addr;

	/* Header Data: HEADER CAL CHECKSUM */
	if (rom_addr->rom_header_checksum_addr >= 0)
		finfo_local->header_section_crc_addr = rom_addr->rom_header_checksum_addr;

/////////////////////////////////////////////////////////////////////////////////////
/* OEM Data: OEM Module Info */
/////////////////////////////////////////////////////////////////////////////////////
	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_sub_oem_checksum_addr >= 0) {
			//temp_start_addr = rom_addr->rom_sub_oem_module_info_start_addr;
			temp_end_addr = rom_addr->rom_sub_oem_checksum_addr;
		}
	} else {
		if (rom_addr->rom_oem_checksum_addr >= 0) {
			//temp_start_addr = rom_addr->rom_oem_module_info_start_addr;
			temp_end_addr = rom_addr->rom_oem_checksum_addr;
		}
	}

	if (temp_end_addr >= 0) {
		//memcpy(finfo_local->oem_ver, &pRomData[temp_start_addr], IMGSENSOR_OEM_VER_SIZE);
		//finfo_local->oem_ver[IMGSENSOR_OEM_VER_SIZE] = '\0';
		finfo_local->oem_section_crc_addr = temp_end_addr;
	}

	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_sub_oem_af_inf_position_addr >= 0) {
			temp_start_addr = rom_addr->rom_sub_oem_af_inf_position_addr;
			temp_end_addr = rom_addr->rom_sub_oem_af_macro_position_addr;
		}
	} else {
		if (rom_addr->rom_oem_af_inf_position_addr >= 0) {
			temp_start_addr = rom_addr->rom_oem_af_inf_position_addr;
			temp_end_addr = rom_addr->rom_oem_af_macro_position_addr;
		}
	}

	if (temp_start_addr >= 0)
		finfo_local->af_cal_pan = *((u32 *)&pRomData[temp_start_addr]);
	if (temp_end_addr >= 0)
		finfo_local->af_cal_macro = *((u32 *)&pRomData[temp_end_addr]);

/////////////////////////////////////////////////////////////////////////////////////
/* AWB Data: AWB Module Info */
/////////////////////////////////////////////////////////////////////////////////////
	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_sub_awb_checksum_addr >= 0) {
			//temp_start_addr = rom_addr->rom_sub_awb_module_info_start_addr;
			temp_end_addr = rom_addr->rom_sub_awb_checksum_addr;
		}
	} else {
		if (rom_addr->rom_awb_checksum_addr >= 0) {
			//temp_start_addr = rom_addr->rom_awb_module_info_start_addr;
			temp_end_addr = rom_addr->rom_awb_checksum_addr;
		}
	}

	if (temp_end_addr >= 0) {
		//memcpy(finfo_local->awb_ver, &pRomData[temp_start_addr], IMGSENSOR_AWB_VER_SIZE);
		//finfo_local->awb_ver[IMGSENSOR_AWB_VER_SIZE] = '\0';
		finfo_local->awb_section_crc_addr = temp_end_addr;
	}

/////////////////////////////////////////////////////////////////////////////////////
/* SHADING Data: Shading Module Info */
/////////////////////////////////////////////////////////////////////////////////////
	temp_start_addr = temp_end_addr = -1;
	if (info->use_common_eeprom == true) {
		if (rom_addr->rom_shading_checksum_addr >= 0) {
			//temp_start_addr = rom_addr->rom_sub_shading_module_info_start_addr;
			temp_end_addr = rom_addr->rom_sub_shading_checksum_addr;
		}
	} else {
		if (rom_addr->rom_shading_checksum_addr >= 0) {
			//temp_start_addr = rom_addr->rom_shading_module_info_start_addr;
			temp_end_addr = rom_addr->rom_shading_checksum_addr;
		}
	}

	if (temp_end_addr >= 0) {
		//memcpy(finfo_local->shading_ver, &pRomData[temp_start_addr], IMGSENSOR_SHADING_VER_SIZE);
		//finfo_local->shading_ver[IMGSENSOR_SHADING_VER_SIZE] = '\0';
		finfo_local->shading_section_crc_addr = temp_end_addr;
	}

/////////////////////////////////////////////////////////////////////////////////////
/* Dual CAL Data: Dual cal Module Info */
/////////////////////////////////////////////////////////////////////////////////////
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_dual_checksum_addr >= 0) {
		//temp_start_addr = rom_addr->rom_dual_module_info_start_addr;
		temp_end_addr = rom_addr->rom_dual_checksum_addr;
	}

	if (temp_end_addr >= 0) {
		//memcpy(finfo_local->dual_data_ver, &pRomData[temp_start_addr], IMGSENSOR_DUAL_CAL_VER_SIZE);
		//finfo_local->dual_data_ver[IMGSENSOR_DUAL_CAL_VER_SIZE] = '\0';
		finfo_local->dual_data_section_crc_addr = temp_end_addr;
	}

/////////////////////////////////////////////////////////////////////////////////////
/* Sensor CAL Data: Sensor CAL Module Info */
/////////////////////////////////////////////////////////////////////////////////////
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_sensor_cal_checksum_addr >= 0) {
		//temp_start_addr = rom_addr->rom_sensor_cal_module_info_start_addr;
		temp_end_addr = rom_addr->rom_sensor_cal_checksum_addr;
	}

	if (temp_end_addr >= 0) {
		//memcpy(finfo_local->sensor_cal_data_ver, &pRomData[temp_start_addr], IMGSENSOR_SENSOR_CAL_DATA_VER_SIZE);
		//finfo_local->sensor_cal_data_ver[IMGSENSOR_SENSOR_CAL_DATA_VER_SIZE] = '\0';
		finfo_local->sensor_cal_data_section_crc_addr = temp_end_addr;
	}

/////////////////////////////////////////////////////////////////////////////////////
/* PDAF CAL Data: PDAF Module Info */
/////////////////////////////////////////////////////////////////////////////////////
	temp_start_addr = temp_end_addr = -1;
	if (rom_addr->rom_pdaf_checksum_addr >= 0) {
		//temp_start_addr = rom_addr->rom_pdaf_module_info_start_addr;
		temp_end_addr = rom_addr->rom_pdaf_checksum_addr;
	}

	if (temp_end_addr >= 0) {
		//memcpy(finfo_local->ap_pdaf_ver, &pRomData[temp_start_addr], IMGSENSOR_AP_PDAF_VER_SIZE);
		//finfo_local->ap_pdaf_ver[IMGSENSOR_AP_PDAF_VER_SIZE] = '\0';
		finfo_local->ap_pdaf_section_crc_addr = temp_end_addr;
	}

/////////////////////////////////////////////////////////////////////////////////////
/* Sensor Maker and Sensor Name */
/////////////////////////////////////////////////////////////////////////////////////
	if (info->use_common_eeprom == true) {
		finfo_local->sensor_maker = rom_addr->sub_sensor_maker;
		finfo_local->sensor_name  = rom_addr->sub_sensor_name;
	} else {
		finfo_local->sensor_maker = rom_addr->sensor_maker;
		finfo_local->sensor_name  = rom_addr->sensor_name;
	}

/////////////////////////////////////////////////////////////////////////////////////
/* CRC */
/////////////////////////////////////////////////////////////////////////////////////
	if (cam_infos[info->sub_sensor_position].cal_memory == CAM_INFO_CAL_MEM_TYPE_OTP) {
		if (!imgsensor_check_otp_crc32(pRomData, info->sensor_position)) {
			if (retry > 0) {
				retry--;
				pr_info("OTP Retry Remaining - %d", retry);
				goto crc_retry;
			} else
				ret = -1;
		}
	} else {
		if (!imgsensor_check_eeprom_crc32(pRomData, info->sensor_position,
			info->sub_sensor_position, info->use_common_eeprom)) {
			if (retry > 0) {
				retry--;
				pr_info("Retry Remaining - %d", retry);
				goto crc_retry;
			} else
				ret = -1;
		}
	}

	if (finfo_local->header_ver[3] == ISP_VENDOR1 || finfo_local->header_ver[3] == ISP_VENDOR2)
		crc32_check_list[info->sub_sensor_position][CRC32_CHECK_FACTORY] = crc32_check_list[info->sub_sensor_position][CRC32_CHECK];
	else
		crc32_check_list[info->sub_sensor_position][CRC32_CHECK_FACTORY] = false;

	if (specific != NULL && specific->use_module_check) {
		/* Check this module is latest */
		if (finfo[info->sub_sensor_position]->header_ver[10] >= rom_addr->camera_module_es_version) {
			check_latest_cam_module[info->sub_sensor_position] = true;
		} else {
			check_latest_cam_module[info->sub_sensor_position] = false;
		}
		/* Check this module is final for manufacture */
		if (finfo[info->sub_sensor_position]->header_ver[10] == IMGSENSOR_LATEST_ROM_VERSION_M) {
			check_final_cam_module[info->sub_sensor_position] = true;
		} else {
			check_final_cam_module[info->sub_sensor_position] = false;
		}
	} else {
		check_latest_cam_module[info->sub_sensor_position] = true;
		check_final_cam_module[info->sub_sensor_position] = true;
	}

	if (g_cal_buf[info->sub_sensor_position] == NULL)
		pr_err("%s: g_cal_buf[%d] is NULL", __func__, info->sub_sensor_position);
	else {
		memcpy(g_cal_buf[info->sub_sensor_position], pRomData, g_cal_buf_size[info->sub_sensor_position]);
	}
	return ret;
}

int imgsensor_sys_get_cal_size(unsigned int remapped_device_id, unsigned int sensorId)
{
	int max_num_of_rom_info = 0, i = 0, cal_size = 0;

	max_num_of_rom_info = ARRAY_SIZE(vendor_rom_info);
	pr_info("[%s]max_num_of_rom_info: %d \n", __func__, max_num_of_rom_info);

	if (remapped_device_id >= SENSOR_POSITION_MAX)
		return 0;

	for (i = 0; i < max_num_of_rom_info; i++) {
		if (vendor_rom_info[i].rom_addr != NULL &&
			vendor_rom_info[i].sensor_position == remapped_device_id) {
			if (sensorId == (unsigned int)-1) {
				int tmp = vendor_rom_info[i].rom_addr->rom_max_cal_size;

				cal_size = (tmp > cal_size) ? tmp : cal_size;
				pr_info("[%s] sensor position: %d, sensor_id: %#06x, Cal Size: %#06x, Max cal size: %d\n",
						__func__, remapped_device_id, vendor_rom_info[i].sensor_id_with_rom,
						tmp, cal_size);
			} else if (vendor_rom_info[i].sensor_id_with_rom == sensorId) {
				cal_size = vendor_rom_info[i].rom_addr->rom_max_cal_size;
				pr_info("[%s] sensor position: %d, sensor_id: %#06x, Cal Size: %#06x\n",
						__func__, remapped_device_id, sensorId, cal_size);
				break;
			}
		}
	}
	if (i == max_num_of_rom_info && cal_size == 0) {
		pr_err("[%s] fail, no searched rom_info, device id: %#06x, sensor id: %#06x\n",
				__func__, remapped_device_id, sensorId);
	}
	return cal_size;
}

int imgsensor_sys_get_cal_size_by_device_id(unsigned int deviceIdx, unsigned int sensorId)
{
	unsigned int remapped_device_id = (int)map_position(deviceIdx);

	return imgsensor_sys_get_cal_size(remapped_device_id, sensorId);
}

int imgsensor_sys_get_cal_size_by_dual_device_id(unsigned int dualDeviceId, unsigned int sensorId)
{
	enum IMGSENSOR_SENSOR_IDX deviceIdx = IMGSENSOR_SENSOR_IDX_MAP(dualDeviceId);

	return imgsensor_sys_get_cal_size_by_device_id(deviceIdx, sensorId);
}

const struct imgsensor_vendor_rom_addr *imgsensor_sys_get_rom_addr_by_id(
	unsigned int dualDeviceId, unsigned int sensorId)
{
	enum IMGSENSOR_SENSOR_IDX deviceIdx = IMGSENSOR_SENSOR_IDX_MAP(dualDeviceId);
	unsigned int remapped_device_id  = (int)map_position(deviceIdx);
	int max_num_of_rom_info = 0, i = 0;

	if (remapped_device_id >= SENSOR_POSITION_MAX) {
		pr_err("[%s] remapped_device_id has exceeded max value: %d", __func__, remapped_device_id);
		return NULL;
	}
	max_num_of_rom_info = ARRAY_SIZE(vendor_rom_info);
	pr_info("[%s]max_num_of_rom_info: %d\n", __func__, max_num_of_rom_info);

	for (i = 0; i <= max_num_of_rom_info; i++) {
		if (i == max_num_of_rom_info) {
			pr_err("[%s] fail, no searched rom_info, device id: %#06x, sensor id: %#06x\n",
					__func__, remapped_device_id, sensorId);
			return NULL;
		}

		if (vendor_rom_info[i].rom_addr != NULL &&
			vendor_rom_info[i].sensor_position == remapped_device_id &&
			vendor_rom_info[i].sensor_id_with_rom == sensorId) {
			break;
		}
	}
	return vendor_rom_info[i].rom_addr;
}


int imgsensor_sysfs_update(unsigned char* pRomData, unsigned int dualDeviceId, unsigned int sensorId, unsigned int offset, unsigned int length, int i4RetValue)
{
	struct imgsensor_eeprom_read_info eeprom_read_info;
	enum IMGSENSOR_SENSOR_IDX deviceIdx = IMGSENSOR_SENSOR_IDX_MAP(dualDeviceId);
	unsigned int sub_deviceIdx = 0;
	unsigned int remapped_device_id = 0;
	int cal_size = 0;
	int ret = 0;

	pr_info("%s : deviceID=%d, sensorID=0x%X, offset=%d, length=%d, i4RetValue=%d", __func__, deviceIdx, sensorId, offset, length, i4RetValue);
	if (deviceIdx == IMGSENSOR_SENSOR_IDX_NONE)
		return -1;

	cal_size = imgsensor_sys_get_cal_size_by_device_id(deviceIdx, sensorId);
	if (offset != 0 || i4RetValue != cal_size) {
		pr_err("%s : read size NG, read size: %#06x, cal_size: %#06x \n", __func__, i4RetValue, cal_size);
		return -1;
	}

	remapped_device_id = (int)map_position(deviceIdx);

	eeprom_read_info.sensor_position     = remapped_device_id;
	eeprom_read_info.sensor_id           = sensorId;
	eeprom_read_info.sub_sensor_position = remapped_device_id;
	eeprom_read_info.use_common_eeprom   = false;
	ret |= imgsensor_rom_read_common(pRomData, &eeprom_read_info);

	init_cam_hwparam(remapped_device_id);
	if (cam_infos[deviceIdx].includes_sub) {
		sub_deviceIdx = cam_infos[deviceIdx].sub_sensor_id;
		pr_info("%s : sub_deviceID=%d", __func__, sub_deviceIdx);

		eeprom_read_info.sensor_position     = remapped_device_id;
		eeprom_read_info.sensor_id           = sensorId;
		eeprom_read_info.sub_sensor_position = sub_deviceIdx;
		eeprom_read_info.use_common_eeprom   = true;
		ret |= imgsensor_rom_read_common(pRomData, &eeprom_read_info);

		init_cam_hwparam(sub_deviceIdx);
	}
#if ENABLE_CAM_CAL_DUMP
	imgsensor_sec_readcal_dump(pRomData, length, remapped_device_id);
#endif
	return ret;
}

static int create_rear_sysfs(struct kobject *svc)
{
	int ret = 0;
	if (camera_rear_dev == NULL) {
		camera_rear_dev = device_create(camera_class, NULL, 1, NULL, "rear");
	}
	if (IS_ERR(camera_rear_dev)) {
		pr_err("imgsensor_sysfs_init: failed to create device(rear)\n");
		return -ENODEV;
	}

	if (device_create_file(camera_rear_dev, &dev_attr_rear_sensorid) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_sensorid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_moduleid) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_moduleid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_caminfo) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_caminfo.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_sensorid_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_mtf_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_afcal) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_afcal.attr.name);
	}
#if defined(REAR_SUB_CAMERA)
	if (device_create_file(camera_rear_dev, &dev_attr_rear_dualcal) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_dualcal.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_dualcal_size) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_dualcal_size.attr.name);
	}
#endif
	if (device_create_file(camera_rear_dev, &dev_attr_supported_cameraIds) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_supported_cameraIds.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_camfw) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_camfw.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_camfw_full) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_camfw_full.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_checkfw_user.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_checkfw_factory.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_camtype) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_camtype.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_calcheck) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear_calcheck.attr.name);
	}
	if (sysfs_create_file(svc, &dev_attr_SVC_rear_module.attr) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_SVC_rear_module.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_ssrm_camera_info) < 0) {
			printk(KERN_ERR "failed to create front device file, %s\n",
					dev_attr_ssrm_camera_info.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear_hwparam) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear_hwparam.attr.name);
	}
	return ret;
}

static int create_rear2_sysfs(struct kobject *svc)
{
	int ret = 0;
	if (camera_rear_dev == NULL) {
		camera_rear_dev = device_create(camera_class, NULL, 1, NULL, "rear");
	}
	if (IS_ERR(camera_rear_dev)) {
		pr_err("imgsensor_sysfs_init: failed to create device(rear)\n");
		return -ENODEV;
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_sensorid) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_sensorid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_moduleid) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_moduleid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_caminfo) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_caminfo.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_sensorid_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_mtf_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_camfw) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_camfw.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_camfw_full) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_camfw_full.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_camtype) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear2_camtype.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_checkfw_user.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create rear2 device file, %s\n",
			dev_attr_rear2_checkfw_factory.attr.name);
	}
	if (sysfs_create_file(svc, &dev_attr_SVC_rear_module2.attr) < 0) {
			printk(KERN_ERR "failed to create rear2 device file, %s\n",
					dev_attr_SVC_rear_module2.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear2_hwparam) < 0) {
			printk(KERN_ERR "failed to create rear device file, %s\n",
					dev_attr_rear2_hwparam.attr.name);
	}
	return ret;
}

static int create_rear3_sysfs(struct kobject *svc)
{
	int ret = 0;
	if (camera_rear_dev == NULL) {
		camera_rear_dev = device_create(camera_class, NULL, 1, NULL, "rear");
	}
	if (IS_ERR(camera_rear_dev)) {
		pr_err("imgsensor_sysfs_init: failed to create device(rear)\n");
		return -ENODEV;
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_sensorid) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_sensorid.attr.name);
	}
#ifndef USE_SHARED_ROM_REAR3
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_moduleid) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_moduleid.attr.name);
	}
	if (sysfs_create_file(svc, &dev_attr_SVC_rear_module3.attr) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
				dev_attr_SVC_rear_module3.attr.name);
	}
#endif
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_caminfo) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_caminfo.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear3_sensorid_exif.attr.name);
	}
#if defined(REAR_SUB_CAMERA)
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_dualcal) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_dualcal.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_dualcal_size) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_dualcal_size.attr.name);
	}
#endif
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_camfw) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_camfw.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_camfw_full) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_camfw_full.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_checkfw_user.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_checkfw_factory.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_camtype) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_camtype.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_tilt) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_tilt.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_hwparam) < 0) {
			printk(KERN_ERR "failed to create rear3 device file, %s\n",
					dev_attr_rear3_hwparam.attr.name);
	}
#if NOTYETDONE
	if (device_create_file(camera_rear_dev, &dev_attr_rear3_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create rear3 device file, %s\n",
			dev_attr_rear3_mtf_exif.attr.name);
	}
#endif
	return ret;
}

#if defined(REAR_CAMERA4)
static int create_rear4_sysfs(struct kobject *svc)
{
	int ret = 0;
	if (camera_rear_dev == NULL) {
		camera_rear_dev = device_create(camera_class, NULL, 1, NULL, "rear");
	}
	if (IS_ERR(camera_rear_dev)) {
		pr_err("imgsensor_sysfs_init: failed to create device(rear)\n");
		return -ENODEV;
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_sensorid) < 0) {
		printk(KERN_ERR "failed to create rear4 device file, %s\n",
			dev_attr_rear4_sensorid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_moduleid) < 0) {
		printk(KERN_ERR "failed to create rear4 device file, %s\n",
			dev_attr_rear4_moduleid.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_caminfo) < 0) {
		printk(KERN_ERR "failed to create rear4 device file, %s\n",
			dev_attr_rear4_caminfo.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create rear4 device file, %s\n",
			dev_attr_rear4_sensorid_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create rear4 device file, %s\n",
			dev_attr_rear4_mtf_exif.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_camtype) < 0) {
		printk(KERN_ERR "failed to create rear device file, %s\n",
			dev_attr_rear4_camtype.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_camfw) < 0) {
		printk(KERN_ERR "failed to create rear4 device file, %s\n",
			dev_attr_rear4_camfw.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_camfw_full) < 0) {
		printk(KERN_ERR "failed to create rear4 device file, %s\n",
			dev_attr_rear4_camfw_full.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create rear4 device file, %s\n",
			dev_attr_rear4_checkfw_user.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create rear4 device file, %s\n",
			dev_attr_rear4_checkfw_factory.attr.name);
	}
	if (sysfs_create_file(svc, &dev_attr_SVC_rear_module4.attr) < 0) {
			printk(KERN_ERR "failed to create rear4 device file, %s\n",
					dev_attr_SVC_rear_module4.attr.name);
	}
	if (device_create_file(camera_rear_dev, &dev_attr_rear4_hwparam) < 0) {
			printk(KERN_ERR "failed to create rear4 device file, %s\n",
					dev_attr_rear4_hwparam.attr.name);
	}
	return ret;
}
#endif//REAR_CAMERA4

static int create_front_sysfs(struct kobject *svc)
{
	int ret = 0;
	if (camera_front_dev == NULL) {
		camera_front_dev = device_create(camera_class, NULL, 1, NULL, "front");
	}
	if (IS_ERR(camera_front_dev)) {
		pr_err("imgsensor_sysfs_init: failed to create device(front)\n");
		return -ENODEV;
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_sensorid) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_sensorid.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_moduleid) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_moduleid.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_caminfo) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_caminfo.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_sensorid_exif) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_sensorid_exif.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_mtf_exif) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_mtf_exif.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_camfw) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_camfw.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_camfw_full) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_camfw_full.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_camtype) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_camtype.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_checkfw_user) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_checkfw_user.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_checkfw_factory) < 0) {
		printk(KERN_ERR "failed to create front device file, %s\n",
			dev_attr_front_checkfw_factory.attr.name);
	}
	if (sysfs_create_file(svc, &dev_attr_SVC_front_module.attr) < 0) {
			printk(KERN_ERR "failed to create front device file, %s\n",
					dev_attr_SVC_front_module.attr.name);
	}
	if (device_create_file(camera_front_dev, &dev_attr_front_hwparam) < 0) {
			printk(KERN_ERR "failed to create front device file, %s\n",
					dev_attr_front_hwparam.attr.name);
	}
	return ret;
}

void imgsensor_destroy_rear_sysfs(void)
{
	device_remove_file(camera_rear_dev, &dev_attr_rear_sensorid);
	device_remove_file(camera_rear_dev, &dev_attr_rear_moduleid);
	device_remove_file(camera_rear_dev, &dev_attr_rear_camtype);
	device_remove_file(camera_rear_dev, &dev_attr_rear_camfw);
	device_remove_file(camera_rear_dev, &dev_attr_rear_camfw_full);
	device_remove_file(camera_rear_dev, &dev_attr_rear_caminfo);
	device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_user);
	device_remove_file(camera_rear_dev, &dev_attr_rear_checkfw_factory);
	device_remove_file(camera_rear_dev, &dev_attr_supported_cameraIds);
	device_remove_file(camera_rear_dev, &dev_attr_rear_sensorid_exif);
	device_remove_file(camera_rear_dev, &dev_attr_rear_mtf_exif);
	device_remove_file(camera_rear_dev, &dev_attr_rear_afcal);
#if defined(REAR_SUB_CAMERA)
	device_remove_file(camera_rear_dev, &dev_attr_rear_dualcal);
	device_remove_file(camera_rear_dev, &dev_attr_rear_dualcal_size);
#endif
	device_remove_file(camera_rear_dev, &dev_attr_rear_calcheck);
}

void imgsensor_destroy_front_sysfs(void)
{
	device_remove_file(camera_front_dev, &dev_attr_front_sensorid);
	device_remove_file(camera_front_dev, &dev_attr_front_moduleid);
	device_remove_file(camera_front_dev, &dev_attr_front_camtype);
	device_remove_file(camera_front_dev, &dev_attr_front_camfw);
	device_remove_file(camera_front_dev, &dev_attr_front_camfw_full);
	device_remove_file(camera_front_dev, &dev_attr_front_caminfo);
	device_remove_file(camera_front_dev, &dev_attr_front_checkfw_user);
	device_remove_file(camera_front_dev, &dev_attr_front_checkfw_factory);
	device_remove_file(camera_front_dev, &dev_attr_front_sensorid_exif);
	device_remove_file(camera_front_dev, &dev_attr_front_mtf_exif);
}

void imgsensor_destroy_rear2_sysfs(void)
{
	device_remove_file(camera_rear_dev, &dev_attr_rear2_sensorid);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_moduleid);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_camfw);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_camfw_full);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_caminfo);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_checkfw_user);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_checkfw_factory);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_sensorid_exif);
	device_remove_file(camera_rear_dev, &dev_attr_rear2_mtf_exif);
}

void imgsensor_destroy_rear3_sysfs(void)
{
	device_remove_file(camera_rear_dev, &dev_attr_rear3_sensorid);
#ifndef USE_SHARED_ROM_REAR3
	device_remove_file(camera_rear_dev, &dev_attr_rear3_moduleid);
#endif
	device_remove_file(camera_rear_dev, &dev_attr_rear3_camfw);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_camfw_full);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_caminfo);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_checkfw_user);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_checkfw_factory);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_sensorid_exif);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_dualcal);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_dualcal_size);
	device_remove_file(camera_rear_dev, &dev_attr_rear3_tilt);
#if NOTYETDONE
	device_remove_file(camera_rear_dev, &dev_attr_rear3_mtf_exif);
#endif
}

#if defined(REAR_CAMERA4)
void imgsensor_destroy_rear4_sysfs(void)
{
	device_remove_file(camera_rear_dev, &dev_attr_rear4_sensorid);
	device_remove_file(camera_rear_dev, &dev_attr_rear4_moduleid);
	device_remove_file(camera_rear_dev, &dev_attr_rear4_camfw);
	device_remove_file(camera_rear_dev, &dev_attr_rear4_camfw_full);
	device_remove_file(camera_rear_dev, &dev_attr_rear4_caminfo);
	device_remove_file(camera_rear_dev, &dev_attr_rear4_checkfw_user);
	device_remove_file(camera_rear_dev, &dev_attr_rear4_checkfw_factory);
	device_remove_file(camera_rear_dev, &dev_attr_rear4_sensorid_exif);
	device_remove_file(camera_rear_dev, &dev_attr_rear4_mtf_exif);
}
#endif

static void __exit imgsensor_destroy_sysfs(void)
{
	int i = 0;

	if (camera_rear_dev) {
		imgsensor_destroy_rear_sysfs();
		imgsensor_destroy_rear2_sysfs();
		imgsensor_destroy_rear3_sysfs();
#if defined(REAR_CAMERA4)
		imgsensor_destroy_rear4_sysfs();
#endif
	}

	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		g_cal_buf_size[i] = 0;
		if (g_cal_buf[i]) {
			kvfree(g_cal_buf[i]);
			g_cal_buf[i] = NULL;
		}
	}

	if (camera_front_dev) {
		imgsensor_destroy_front_sysfs();
	}

	if (camera_class) {
		if (camera_front_dev)
			device_destroy(camera_class, camera_front_dev->devt);

		if (camera_rear_dev)
			device_destroy(camera_class, camera_rear_dev->devt);
	}

	class_destroy(camera_class);
}


// init function
static int __init imgsensor_sysfs_init(void)
{
	int ret = 0, i = 0;
	struct kobject *svc = 0;

	pr_info("%s start", __func__);

	if (camera_class == NULL) {
		camera_class = class_create(THIS_MODULE, "camera");
	}

	if (IS_ERR_OR_NULL(camera_class)) {
		pr_err("imgsensor_sysfs_init: error, camera class not exist");
		return -ENODEV;
	}

	for (i = 0; i < SENSOR_POSITION_MAX; i++) {
		int size  = imgsensor_sys_get_cal_size(i, (unsigned int)-1);

		if (size > 0) {
			g_cal_buf_size[i] = size;
			g_cal_buf[i] = (char *)vmalloc(sizeof(char) * size);
			if (g_cal_buf[i] == NULL) {
				g_cal_buf_size[i] = 0;
				pr_err("%s: error, cal buffer malloc failed for remapped_device_id %d", __func__, i);
			} else {
				pr_info("%s: remapped_device_id %d cal buffer malloc size %d", __func__, i, size);
			}
		}
	}

	svc_cheating_prevent_device_file_create(&svc);

	create_rear_sysfs(svc);
	create_rear2_sysfs(svc);
	create_rear3_sysfs(svc);
#if defined(REAR_CAMERA4)
	create_rear4_sysfs(svc);
#endif
	create_front_sysfs(svc);
	return ret;
}

module_init(imgsensor_sysfs_init);
module_exit(imgsensor_destroy_sysfs);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Civashritt A B <ab.civa@samsung.com>, Heechul Kim <heechul7.kim@samsung.com>");
MODULE_DESCRIPTION("CAM SYSFS");
