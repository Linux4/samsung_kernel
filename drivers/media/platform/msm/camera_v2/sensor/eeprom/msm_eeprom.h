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

#if defined(CONFIG_GET_REAR_MODULE_ID) || defined(CONFIG_GET_FRONT_MODULE_ID)
/* Module ID : 0x00A8~0x00B7(16Byte) for FROM, EEPROM (Don't support OTP)*/
#define FROM_REAR_MODULE_ID_ADDR            0x00AE
#if defined(CONFIG_SEC_ASTARQLTE_PROJECT) || defined(CONFIG_SEC_DREAMLITEQLTE_PROJECT)
#define FROM_FRONT_MODULE_ID_ADDR           0x00AE
#else
#define FROM_FRONT_MODULE_ID_ADDR           0x0056
#endif
#define FROM_MODULE_ID_SIZE                 16
#endif
#if defined(CONFIG_SEC_DREAMLITEQLTE_PROJECT)
#define FROM_REAR_AF_CAL_PAN_ADDR           0x0110
#define FROM_REAR_AF_CAL_MACRO_ADDR         0x010C
#define FROM_FRONT_AF_CAL_PAN_ADDR          0x0104
#define FROM_FRONT_AF_CAL_MACRO_ADDR        0x0110
#else
#define FROM_REAR_AF_CAL_PAN_ADDR           0x081C
#define FROM_REAR_AF_CAL_MACRO_ADDR         0x0818
#endif
/*#define FROM_REAR_AF_CAL_D10_ADDR*/
/*#define FROM_REAR_AF_CAL_D20_ADDR*/
/*#define FROM_REAR_AF_CAL_D30_ADDR*/
/*#define FROM_REAR_AF_CAL_D40_ADDR*/
/*#define FROM_REAR_AF_CAL_D50_ADDR*/
/*#define FROM_REAR_AF_CAL_D60_ADDR*/
/*#define FROM_REAR_AF_CAL_D70_ADDR*/
/*#define FROM_REAR_AF_CAL_D80_ADDR*/

#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
#if defined(CONFIG_CAMERA_DUAL_REAR)
#if defined(CONFIG_SEC_ASTARQLTE_PROJECT)
#define REAR2_HAVE_AF_CAL_DATA
#define FROM_REAR2_DUAL_TILT_X            0x11D0
#define FROM_REAR2_DUAL_TILT_Y            0x11D4
#define FROM_REAR2_DUAL_TILT_Z            0x11D8
#define FROM_REAR2_DUAL_TILT_SX           0x1230
#define FROM_REAR2_DUAL_TILT_SY           0x1234
#define FROM_REAR2_DUAL_TILT_RANGE        0x1354
#define FROM_REAR2_DUAL_TILT_MAX_ERR      0x1358
#define FROM_REAR2_DUAL_TILT_AVG_ERR      0x135C
#define FROM_REAR2_DUAL_TILT_DLL_VERSION  0x1350
#define FROM_REAR2_DUAL_CAL_ADDR          0x1174
#else
#define FROM_REAR2_DUAL_TILT_X            0x0946
#define FROM_REAR2_DUAL_TILT_Y            0x094A
#define FROM_REAR2_DUAL_TILT_Z            0x094E
#define FROM_REAR2_DUAL_TILT_SX           0x09A6
#define FROM_REAR2_DUAL_TILT_SY           0x09AA
#define FROM_REAR2_DUAL_TILT_RANGE        0x0BCA
#define FROM_REAR2_DUAL_TILT_MAX_ERR      0x0BCE
#define FROM_REAR2_DUAL_TILT_AVG_ERR      0x0BD2
#define FROM_REAR2_DUAL_TILT_DLL_VERSION  0x08E6
#define FROM_REAR2_DUAL_CAL_ADDR          0x08EA
#endif
#define FROM_REAR2_DUAL_CAL_SIZE          512

/*#define FROM_REAR2_AF_CAL_D10_ADDR*/
/*#define FROM_REAR2_AF_CAL_D20_ADDR*/
/*#define FROM_REAR2_AF_CAL_D30_ADDR*/
/*#define FROM_REAR2_AF_CAL_D40_ADDR*/
/*#define FROM_REAR2_AF_CAL_D50_ADDR*/
/*#define FROM_REAR2_AF_CAL_D60_ADDR*/
/*#define FROM_REAR2_AF_CAL_D70_ADDR*/
/*#define FROM_REAR2_AF_CAL_D80_ADDR*/
#if defined(CONFIG_SEC_ASTARQLTE_PROJECT)
#define FROM_REAR2_AF_CAL_MACRO_ADDR      0x1018
#define FROM_REAR2_AF_CAL_PAN_ADDR        0x101C
#else
#define FROM_REAR2_AF_CAL_MACRO_ADDR      0x0818
#define FROM_REAR2_AF_CAL_PAN_ADDR        0x081C
#endif
#endif
#if defined(CONFIG_CAMERA_QUAD_REAR)
#define FROM_REAR3_DUAL_TILT_X            0x0F46
#define FROM_REAR3_DUAL_TILT_Y            0x0F4A
#define FROM_REAR3_DUAL_TILT_Z            0x0F4E
#define FROM_REAR3_DUAL_TILT_SX           0x0FA6
#define FROM_REAR3_DUAL_TILT_SY           0x0FAA
#define FROM_REAR3_DUAL_TILT_RANGE        0x1354
#define FROM_REAR3_DUAL_TILT_MAX_ERR      0x1358
#define FROM_REAR3_DUAL_TILT_AVG_ERR      0x135C
#define FROM_REAR3_DUAL_TILT_DLL_VERSION  0x0EE6
#define FROM_REAR3_DUAL_CAL_ADDR          0x0EEA
#define FROM_REAR3_DUAL_CAL_SIZE          512

#define FROM_REAR3_AF_CAL_MACRO_ADDR      0x1018
#define FROM_REAR3_AF_CAL_PAN_ADDR        0x101C

#define REAR4_HAVE_AF_CAL_DATA
#define FROM_REAR4_DUAL_TILT_X            0x0C46
#define FROM_REAR4_DUAL_TILT_Y            0x0C4A
#define FROM_REAR4_DUAL_TILT_Z            0x0C4E
#define FROM_REAR4_DUAL_TILT_SX           0x0CA6
#define FROM_REAR4_DUAL_TILT_SY           0x0CAA
#define FROM_REAR4_DUAL_TILT_RANGE        0x0CCA
#define FROM_REAR4_DUAL_TILT_MAX_ERR      0x0CCE
#define FROM_REAR4_DUAL_TILT_AVG_ERR      0x0CD2
#define FROM_REAR4_DUAL_TILT_DLL_VERSION  0x0BE6
#define FROM_REAR4_DUAL_CAL_ADDR          0x0BEA
#define FROM_REAR4_DUAL_CAL_SIZE          512

//#define FROM_REAR4_AF_CAL_MACRO_ADDR    0x0818
#define FROM_REAR4_AF_CAL_D50_ADDR        0x0818
#define FROM_REAR4_AF_CAL_PAN_ADDR        0x081C
#endif
#if defined(CONFIG_CAMERA_DUAL_FRONT)
#define FROM_FRONT2_DUAL_TILT_X            0x1210
#define FROM_FRONT2_DUAL_TILT_Y            0x1214
#define FROM_FRONT2_DUAL_TILT_Z            0x1218
#define FROM_FRONT2_DUAL_TILT_SX           0x1270
#define FROM_FRONT2_DUAL_TILT_SY           0x1274
#define FROM_FRONT2_DUAL_TILT_RANGE        0x1394
#define FROM_FRONT2_DUAL_TILT_MAX_ERR      0x1398
#define FROM_FRONT2_DUAL_TILT_AVG_ERR      0x139C
#define FROM_FRONT2_DUAL_TILT_DLL_VERSION  0x1390

#define FROM_FRONT_DUAL_CAL_ADDR           0x11B4
#define FROM_FRONT_DUAL_CAL_SIZE           512
#endif
#endif
#define FROM_REAR_AF_CAL_SIZE              10

/* MTF exif : 0x0050~0x0085(54Byte) for FROM, EEPROM */
#if defined(CONFIG_SAMSUNG_MULTI_CAMERA)
#define FROM_REAR_MTF_ADDR                 0x084E
#define FROM_FRONT_MTF_ADDR                0x0080
#if defined(CONFIG_CAMERA_DUAL_FRONT)
#define FROM_FRONT2_MTF_ADDR               0x1068
#endif
#if defined(CONFIG_CAMERA_DUAL_REAR)
#define FROM_REAR2_MTF_ADDR                0x084A
#endif
#if defined(CONFIG_CAMERA_QUAD_REAR)
#define FROM_REAR3_MTF_ADDR                0x0A82
#define FROM_REAR4_MTF_ADDR                0x084A
#endif
#else
#define FROM_REAR_MTF_ADDR                 0x084E
#define FROM_FRONT_MTF_ADDR                0x0080
#endif
#define FROM_MTF_SIZE                      54

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
	struct msm_eeprom_board_info *eboard_info;
	uint32_t subdev_id;
	int32_t userspace_probe;
	struct msm_eeprom_memory_block_t cal_data;
	uint16_t is_supported;
	
	int pvdd_en;
	int pvdd_is_en;
};
#endif
