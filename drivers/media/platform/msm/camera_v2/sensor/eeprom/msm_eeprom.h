/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MSM_EEPROM_H
#define MSM_EEPROM_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_camera_spi.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"

struct msm_eeprom_ctrl_t;

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define PROPERTY_MAXSIZE 32
#ifdef CONFIG_MSM_REAR_ROM_TYPE_EEPROM
#define REAR_CAM_MAP_VERSION_ADDR   0x0040
#define FROM_AF_CAL_PAN_ADDR        0x0110
#define FROM_AF_CAL_MACRO_ADDR      0x010C
/* Module ID : 0x00A8~0x00B7(16Byte) for EEPROM (Don't support OTP)*/
#define FROM_MODULE_ID_ADDR         0x00A8
#else
#define REAR_CAM_MAP_VERSION_ADDR   0x0063
#define FROM_AF_CAL_PAN_ADDR        0x3C30
#define FROM_AF_CAL_MACRO_ADDR      0x3C28
/* Module ID : 0x00A8~0x00B7(16Byte) for FROM, EEPROM (Don't support OTP)*/
#define FROM_MODULE_ID_ADDR         0x00AE
#endif

#define FRONT_CAM_MAP_VERSION_ADDR  0x0043
#define FROM_FRONT_SENSOR_ID_ADDR   0x0054
#define FROM_REAR_SENSOR_ID_ADDR    0x00B8
#define FROM_SENSOR_ID_SIZE         16
#define FROM_MODULE_ID_SIZE         10

struct msm_eeprom_ctrl_t {
	struct platform_device *pdev;
	struct mutex *eeprom_mutex;

	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *eeprom_v4l2_subdev_ops;
	enum msm_camera_device_type_t eeprom_device_type;
	struct msm_sd_subdev msm_sd;
	enum cci_i2c_master_t cci_master;
	enum i2c_freq_mode_t i2c_freq_mode;

	struct msm_camera_i2c_client i2c_client;
	struct msm_eeprom_memory_block_t cal_data;
	uint16_t is_supported;
	struct msm_eeprom_board_info *eboard_info;
	uint32_t subdev_id;
	int32_t userspace_probe;
};

#endif
