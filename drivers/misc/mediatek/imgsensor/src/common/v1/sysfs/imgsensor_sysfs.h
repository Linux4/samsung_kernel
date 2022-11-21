/* 
 * Copyright (C) 2011 Samsung Electronics, Co. Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IMGSENSOR_SYSFS_H_
#define _IMGSENSOR_SYSFS_H_

#include "imgsensor_vendor_specific.h"
#include "imgsensor_vendor.h"
#include "crc32.h"
#include "kd_camera_feature.h"

//#define SKIP_CHECK_CRC
//#define ROM_CRC32_DEBUG

#define IMGSENSOR_LATEST_ROM_VERSION_M 'M'
#define IMGSENSOR_CAL_RETRY_CNT (2)
#define ENABLE_CAM_CAL_DUMP 1
#define IMGSENSOR_CAL_SDCARD_PATH "/data/vendor/camera/dump/"

#define IMGSENSOR_HEADER_VER_SIZE 11
#define IMGSENSOR_OEM_VER_SIZE 11
#define IMGSENSOR_AWB_VER_SIZE 11
#define IMGSENSOR_SHADING_VER_SIZE 11
#define IMGSENSOR_CAL_MAP_VER_SIZE 4
#define IMGSENSOR_PROJECT_NAME_SIZE 8
#define IMGSENSOR_ISP_SETFILE_VER_SIZE 6
#define IMGSENSOR_SENSOR_ID_SIZE 16
#define IMGSENSOR_CAL_DLL_VERSION_SIZE 4
#define IMGSENSOR_MODULE_ID_SIZE 10
#define IMGSENSOR_DUAL_CAL_VER_SIZE 11
#define IMGSENSOR_SENSOR_CAL_DATA_VER_SIZE 11
#define IMGSENSOR_AP_PDAF_VER_SIZE 11
#define IMGSENSOR_RESOLUTION_DATA_SIZE 54
#define IMGSENSOR_SENSOR_COUNT 6


enum imgsensor_cam_info_isp {
	CAM_INFO_ISP_TYPE_INTERNAL = 0,
	CAM_INFO_ISP_TYPE_EXTERNAL,
	CAM_INFO_ISP_TYPE_SOC,
};

enum imgsensor_cam_info_cal_mem {
	CAM_INFO_CAL_MEM_TYPE_NONE = 0,
	CAM_INFO_CAL_MEM_TYPE_FROM,
	CAM_INFO_CAL_MEM_TYPE_EEPROM,
	CAM_INFO_CAL_MEM_TYPE_OTP,
};

enum imgsensor_cam_info_read_ver {
	CAM_INFO_READ_VER_SYSFS = 0,
	CAM_INFO_READ_VER_CAMON,
};

enum imgsensor_cam_info_core_voltage {
	CAM_INFO_CORE_VOLT_NONE = 0,
	CAM_INFO_CORE_VOLT_USE,
};

enum imgsensor_cam_info_upgrade {
	CAM_INFO_FW_UPGRADE_NONE = 0,
	CAM_INFO_FW_UPGRADE_SYSFS,
	CAM_INFO_FW_UPGRADE_CAMON,
};

enum imgsensor_cam_info_fw_write {
	CAM_INFO_FW_WRITE_NONE = 0,
	CAM_INFO_FW_WRITE_OS,
	CAM_INFO_FW_WRITE_SD,
	CAM_INFO_FW_WRITE_ALL,
};

enum imgsensor_cam_info_fw_dump {
	CAM_INFO_FW_DUMP_NONE = 0,
	CAM_INFO_FW_DUMP_USE,
};

enum imgsensor_cam_info_companion {
	CAM_INFO_COMPANION_NONE = 0,
	CAM_INFO_COMPANION_USE,
};

enum imgsensor_cam_info_ois {
	CAM_INFO_OIS_NONE = 0,
	CAM_INFO_OIS_USE,
};

enum imgsensor_cam_info_valid {
	CAM_INFO_INVALID = 0,
	CAM_INFO_VALID,
};

enum imgsensor_cam_info_dual_open {
	CAM_INFO_SINGLE_OPEN = 0,
	CAM_INFO_DUAL_OPEN,
};

enum imgsensor_cam_info_index {
	CAM_INFO_REAR = 0,
	CAM_INFO_FRONT,
	CAM_INFO_REAR2,
	CAM_INFO_FRONT2,
	CAM_INFO_REAR3,
	CAM_INFO_FRONT3,
	CAM_INFO_REAR4,
	CAM_INFO_FRONT4,
	CAM_INFO_MAX
};
struct imgsensor_common_cam_info {
	unsigned int supported_camera_ids[IMGSENSOR_SENSOR_COUNT];
	unsigned int max_supported_camera;
};
struct imgsensor_cam_info {
	unsigned int isp;
	unsigned int cal_memory;
	unsigned int read_version;
	unsigned int core_voltage;
	unsigned int upgrade;
	unsigned int fw_write;
	unsigned int fw_dump;
	unsigned int companion;
	unsigned int ois;
	unsigned int valid;
	unsigned int dual_open;
	bool includes_sub;
	unsigned int sub_sensor_id;
};

struct imgsensor_rom_info {
	u32 oem_start_addr;
	u32 oem_end_addr;
	u32 awb_start_addr;
	u32 awb_end_addr;
	u32 shading_start_addr;
	u32 shading_end_addr;

	u32 header_section_crc_addr;
	u32 oem_section_crc_addr;
	u32 awb_section_crc_addr;
	u32 shading_section_crc_addr;

	u32 mtf_data_addr;
	u32 af_cal_pan;
	u32 af_cal_macro;

	char header_ver[IMGSENSOR_HEADER_VER_SIZE + 1];
	char oem_ver[IMGSENSOR_OEM_VER_SIZE + 1];
	char awb_ver[IMGSENSOR_AWB_VER_SIZE + 1];
	char shading_ver[IMGSENSOR_SHADING_VER_SIZE + 1];

	char cal_map_ver[IMGSENSOR_CAL_MAP_VER_SIZE + 1];
	char project_name[IMGSENSOR_PROJECT_NAME_SIZE + 1];
	u8 rom_module_id[IMGSENSOR_MODULE_ID_SIZE + 1];
	char rom_sensor_id[IMGSENSOR_SENSOR_ID_SIZE + 1];

	bool is_caldata_read;
	bool is_check_cal_reload;

	u32 dual_data_section_crc_addr;
	u32 dual_data_start_addr;
	u32 dual_data_end_addr;
	char dual_data_ver[IMGSENSOR_DUAL_CAL_VER_SIZE + 1];

	u32 sensor_cal_data_section_crc_addr;
	u32 sensor_cal_data_start_addr;
	u32 sensor_cal_data_end_addr;
	char sensor_cal_data_ver[IMGSENSOR_SENSOR_CAL_DATA_VER_SIZE + 1];

	u32 ap_pdaf_section_crc_addr;
	u32 ap_pdaf_start_addr;
	u32 ap_pdaf_end_addr;
	char ap_pdaf_ver[IMGSENSOR_AP_PDAF_VER_SIZE + 1];

	unsigned long fw_size;
	unsigned long setfile_size;
	u8 sensor_version;

	char *sensor_maker;
	char *sensor_name;
};

struct ssrm_camera_data {
        int operation;
        int cameraID;
        int previewSizeWidth;
        int previewSizeHeight;
        int previewMinFPS;
        int previewMaxFPS;
        int sensorOn;
};

struct imgsensor_eeprom_read_info {
	unsigned int sensor_position;
	unsigned int sensor_id; //sensor id of sensor with ROM
	unsigned int sub_sensor_position;
	bool use_common_eeprom; //use same eeprom for sensor and sub-sensor
};

enum ssrm_camerainfo_operation {
        SSRM_CAMERA_INFO_CLEAR,
        SSRM_CAMERA_INFO_SET,
        SSRM_CAMERA_INFO_UPDATE,
};

int get_cam_info(struct imgsensor_cam_info **caminfo);
int get_specific(struct imgsensor_vendor_specific **sys_specific);
void imgsensor_get_common_cam_info(struct imgsensor_common_cam_info **caminfo);
enum sensor_position map_position(enum IMGSENSOR_SENSOR_IDX img_position);
void update_curr_sensor_pos(int sensorId);
void update_mipi_sensor_err_cnt(void);
#endif /* _IMGSENSOR_SYSFS_H_ */
