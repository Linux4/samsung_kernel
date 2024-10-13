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

#ifndef IS_VENDOR_H
#define IS_VENDOR_H

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/device.h>
#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#include "exynos-is-sensor.h"
#include "is-device-csi.h"
#include "is-device-sensor.h"
#include "is-vendor-mipi.h"
#include "pablo-binary.h"

#define IS_PATH_LEN 100
#define VENDOR_S_CTRL 0
#define VENDOR_G_CTRL 0

#define CAL_OFFSET0		(0x01FD0000)
#define CAL_OFFSET1		(0x01FE0000)

#ifndef EV_LIST_SIZE
#define EV_LIST_SIZE  24
#endif

#ifndef MANUAL_LIST_SIZE
#define MANUAL_LIST_SIZE  5
#endif

#define ZERO_IF_NEG(val) ((val) > 0 ? (val) : 0)
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

struct is_cis;

struct is_vendor {
	char fw_path[IS_PATH_LEN];
	char request_fw_path[IS_PATH_LEN];
	char setfile_path[SENSOR_POSITION_MAX][IS_PATH_LEN];
	char request_setfile_path[SENSOR_POSITION_MAX][IS_PATH_LEN];
	void *private_data;
	int opening_hint;
	int closing_hint;
	int isp_max_width;
	int isp_max_height;
};

enum {
	FW_SKIP,
	FW_SUCCESS,
	FW_FAIL,
};

enum is_rom_id {
	ROM_ID_REAR		= 0,
	ROM_ID_FRONT	= 1,
	ROM_ID_REAR2	= 2,
	ROM_ID_FRONT2	= 3,
	ROM_ID_REAR3	= 4,
	ROM_ID_FRONT3	= 5,
	ROM_ID_REAR4	= 6,
	ROM_ID_FRONT4	= 7,
	ROM_ID_MAX,
	ROM_ID_NOTHING	= 100
};

enum is_rom_type {
	ROM_TYPE_NONE	= 0,
	ROM_TYPE_FROM	= 1,
	ROM_TYPE_EEPROM	= 2,
	ROM_TYPE_OTPROM	= 3,
	ROM_TYPE_MAX,
};

enum is_rom_cal_index {
	ROM_CAL_MASTER	= 0,
	ROM_CAL_SLAVE0	= 1,
	ROM_CAL_SLAVE1	= 2,
	ROM_CAL_MAX,
	ROM_CAL_NOTHING	= 100
};

enum is_rom_dualcal_index {
	ROM_DUALCAL_SLAVE0	= 0,
	ROM_DUALCAL_SLAVE1	= 1,
	ROM_DUALCAL_SLAVE2	= 2,
	ROM_DUALCAL_SLAVE3	= 3,
	ROM_DUALCAL_MAX,
	ROM_DUALCAL_NOTHING	= 100
};

enum hw_init_mode {
	HW_INIT_MODE_NORMAL,
	HW_INIT_MODE_RELOAD,
	HW_INIT_MODE_MAX,
};

#define TOF_AF_SIZE 1200

struct tof_data_t {
	u64 timestamp;
	u32 width;
	u32 height;
	u16 data[TOF_AF_SIZE];
	int AIFCaptureNum;
};

struct tof_info_t {
	u16 TOFExposure;
	u16 TOFFps;
};

struct capture_intent_info_t {
	u16 captureIntent;
	u16 captureCount;
	s16 captureEV;
	u32 captureExtraInfo;
	u32 captureIso;
	u16 captureAeExtraMode;
	char captureMultiEVList[EV_LIST_SIZE];
	uint32_t captureMultiIsoList[MANUAL_LIST_SIZE];
	uint32_t CaptureMultiExposureList[MANUAL_LIST_SIZE];
	u16 captureAeMode;
};

#if IS_ENABLED(CONFIG_CAMERA_HW_BIG_DATA)
enum hw_param_state {
	HW_PARAM_STREAM_ON,
	HW_PARAM_STREAM_OFF,
	HW_PARAM_STREAMING,
};

struct cam_hw_param_mipi_info {
	u32 rat;
	u32 band;
	u32 channel;
};

struct cam_hw_param_af_info {
	u64 timestamp;
	int position;
	int hall_value;
};

struct cam_hw_param {
	u32 i2c_sensor_err_cnt;
	u32 i2c_ois_err_cnt;
	u32 i2c_af_err_cnt;
	u32 i2c_aperture_err_cnt;
	u32 mipi_sensor_err_cnt;
	bool mipi_err_check;
	u32 eeprom_i2c_err_cnt;
	u32 eeprom_crc_err_cnt;
	u32 cam_entrance_cnt;
	u32 af_fail_cnt;
	struct cam_hw_param_af_info af_pos_info[3];
	struct cam_hw_param_af_info af_pos_info_on_fail;
	struct cam_hw_param_mipi_info mipi_info_on_err;
};

struct cam_hw_param_collector {
	struct cam_hw_param rear_hwparam;
	struct cam_hw_param rear2_hwparam;
	struct cam_hw_param rear3_hwparam;
	struct cam_hw_param rear4_hwparam;
	struct cam_hw_param front_hwparam;
	struct cam_hw_param front2_hwparam;
	struct cam_hw_param rear_tof_hwparam;
	struct cam_hw_param front_tof_hwparam;
	struct cam_hw_param iris_hwparam;
};

void is_sec_init_err_cnt(struct cam_hw_param *hw_param);
void is_sec_get_hw_param(struct cam_hw_param **hw_param, u32 position);
void is_sec_update_hw_param_mipi_info_on_err(struct cam_hw_param *hw_param);
void is_sec_update_hw_param_af_info(struct is_device_sensor *device, u32 position, int state);
#endif /* CONFIG_CAMERA_HW_BIG_DATA */
bool is_sec_is_valid_moduleid(char *moduleid);

void is_vendor_csi_stream_on(struct is_device_csi *csi);
void is_vendor_csi_stream_off(struct is_device_csi *csi);
void is_vendor_csi_err_handler(u32 sensor_position);
void is_vendor_csi_err_print_debug_log(struct is_device_sensor *device);

int is_vendor_probe(struct is_vendor *vendor);
#ifdef MODULE
int is_vendor_driver_init(void);
int is_vendor_driver_exit(void);
#endif
int is_vendor_rom_parse_dt(struct device_node *dnode, int rom_id);
int is_vendor_dualized_rom_parse_dt(struct device_node *dnode, int rom_id);
int is_vendor_fw_prepare(struct is_vendor *vendor, u32 position);
int is_vendor_fw_filp_open(struct is_vendor *vendor, struct file **fp, int bin_type);
int is_vendor_preproc_fw_load(struct is_vendor *vendor);
int is_vendor_s_ctrl(struct is_vendor *vendor);
int is_vendor_cal_load(struct is_vendor *vendor, void *module_data);
int is_vendor_cal_verify(struct is_vendor *vendor, void *module_data);
int is_vendor_module_sel(struct is_vendor *vendor, void *module_data);
int is_vendor_module_del(struct is_vendor *vendor, void *module_data);
int is_vendor_fw_sel(struct is_vendor *vendor);
int is_vendor_setfile_sel(struct is_vendor *vendor, char *setfile_name, int position);
int is_vendor_sensor_gpio_on_sel(struct is_vendor *vendor, u32 scenario, u32 *gpio_scenario, void *module_data);
int is_vendor_sensor_gpio_on(struct is_vendor *vendor, u32 scenario, u32 gpio_scenario, void *module_data);
int is_vendor_preprocessor_gpio_off_sel(struct is_vendor *vendor, u32 scenario, u32 *gpio_scenario,
	void *module_data);
int is_vendor_preprocessor_gpio_off(struct is_vendor *vendor, u32 scenario, u32 gpio_scenario);
int is_vendor_sensor_gpio_off_sel(struct is_vendor *vendor, u32 scenario, u32 *gpio_scenario,
	void *module_data);
int is_vendor_sensor_gpio_off(struct is_vendor *vendor, u32 scenario, u32 gpio_scenario, void *module_data);
void is_vendor_sensor_suspend(struct platform_device *pdev);
void is_vendor_update_meta(struct is_vendor *vendor, struct camera2_shot *shot);
bool is_vendor_check_remosaic_mode_change(struct is_frame *frame);
int is_vendor_vidioc_s_ctrl(struct is_video_ctx *vctx, struct v4l2_control *ctrl);
int is_vendor_vidioc_g_ctrl(struct is_video_ctx *vctx, struct v4l2_control *ctrl);
int is_vendor_vidioc_s_ext_ctrl(struct is_video_ctx *vctx, struct v4l2_ext_controls *ctrls);
int is_vendor_vidioc_g_ext_ctrl(struct is_video_ctx *vctx, struct v4l2_ext_controls *ctrls);
int is_vendor_ssx_video_s_ctrl(struct v4l2_control *ctrl, void *device_data);
int is_vendor_ssx_video_g_ctrl(struct v4l2_control *ctrl, void *device_data);
int is_vendor_ssx_video_s_ext_ctrl(struct v4l2_ext_controls *ctrls, void *device_data);
int is_vendor_ssx_video_g_ext_ctrl(struct v4l2_ext_controls *ctrls, void *device_data);
int is_vendor_hw_init(void);
void is_vendor_check_hw_init_running(void);
void is_vendor_sensor_s_input(struct is_vendor *vendor, u32 position);
bool is_vendor_wdr_mode_on(void *cis_data);
bool is_vendor_enable_wdr(void *cis_data);
void is_vendor_resource_get(struct is_vendor *vendor, u32 rsc_type);
void is_vendor_resource_put(struct is_vendor *vendor, u32 rsc_type);
#if defined(CONFIG_CAMERA_USE_INTERNAL_MCU)
void is_vendor_mcu_power_on(bool use_shared_rsc);
void is_vendor_mcu_power_off(bool use_shared_rsc);
#endif
long is_vendor_read_efs(char *efs_path, u8 *buf, int buflen);
int is_vendor_get_module_from_position(int position, struct is_module_enum **module);
int is_vendor_get_rom_id_from_position(int position);
int is_vendor_get_position_from_rom_id(int rom_id, int *position);
void is_vendor_get_rom_info_from_position(int position, int *rom_id, int *rom_cal_index);
void is_vendor_get_rom_dualcal_info_from_position(int position, int *rom_dualcal_id, int *rom_dualcal_index);
void is_vendor_get_use_dualcal_file_from_position(int position, bool *use_dualcal_from_file);
void is_vendor_get_dualcal_file_name_from_position(int position, char **dual_cal_file_name);
int is_vendor_get_dualcal_param_from_file(struct device *dev, char **buf, int position, int *size);
bool is_vendor_check_camera_running(int position);
#ifdef USE_TOF_AF
void is_vendor_store_af(struct is_vendor *vendor, struct tof_data_t *data);
#endif

int is_vendor_shaking_gpio_on(struct is_vendor *vendor);
int is_vendor_shaking_gpio_off(struct is_vendor *vendor);
int is_vendor_replace_sensorid_with_second_sensorid(struct is_vendor *vendor, int position);
int is_vendor_get_dualized_sensorid(int position);
int is_vendor_is_dualized(struct is_device_sensor *sensor, int pos);
void is_vendor_update_otf_data(struct is_group *group, struct is_frame *frame);
void is_vendor_s_ext_ctrl_capture_intent_info(struct is_group *head, struct capture_intent_info_t info);
int is_vendor_notify_hal_init(int mode, struct is_device_sensor *sensor);

int is_vendor_set_mipi_clock(struct is_device_sensor *device);
int is_vendor_set_mipi_mode(struct is_cis *cis);

#if IS_ENABLED(CONFIG_SEC_ABC)
void is_vendor_sec_abc_send_event_mipi_error(u32 sensor_position);
#endif
#endif
