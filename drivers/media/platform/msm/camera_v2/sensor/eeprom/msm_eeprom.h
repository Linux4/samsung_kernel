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

#if defined(CONFIG_SEC_GTS4LLTE_PROJECT) || defined(CONFIG_SEC_GTS4LWIFI_PROJECT)

#define FRONT_CAM_MAP_VERSION_ADDR          0x0020
#define FROM_FRONT_SENSOR_ID_ADDR           0x004E

/*Address 0x0010 is reserverd address - no data*/
#define FROM_FRONT_AF_CAL_PAN_ADDR          0x0010
#define FROM_FRONT_AF_CAL_MACRO_ADDR        0x0010
#define FROM_FRONT_MTF_ADDR                 0x0010

#define REAR_CAM_MAP_VERSION_ADDR           0x0030

#define FROM_REAR_SENSOR_ID_ADDR            0x00B8
#define FROM_SENSOR_ID_SIZE                 16
#define SHOW_REAR_SENSOR_ID_AS_HEX_VALUE

//#define FROM_MODULE_ID_ADDR                 0x00A8
//#define FROM_MODULE_ID_SIZE                 16
#define FROM_MODULE_ID_ADDR                 0x00AE
#define FROM_MODULE_ID_SIZE                 10

/*
#define FROM_REAR_AF_CAL_D10_ADDR           0x3C30
#define FROM_REAR_AF_CAL_D40_ADDR           0x3C2C
#define FROM_REAR_AF_CAL_D50_ADDR           0x3C28
#define FROM_REAR_AF_CAL_D80_ADDR           0x3C20
*/

#define FROM_REAR_AF_CAL_MACRO_ADDR         0x0110
#define FROM_REAR_AF_CAL_PAN_ADDR           0x0104

#define FROM_REAR_DUAL_CAL_ADDR             0x5900
#define FROM_REAR_DUAL_CAL_SIZE             512

#define FROM_REAR_AF_CAL_SIZE               10

/* EEPROM address 0x0020 is reserverd - no data */
#define FROM_REAR_MTF_ADDR                  0x0020
#define FROM_REAR2_AF_CAL_D40_ADDR          0x0020
#define FROM_REAR2_AF_CAL_D50_ADDR          0x0020
#define FROM_REAR2_AF_CAL_D80_ADDR          0x0020
#define FROM_REAR2_AF_CAL_PAN_ADDR          0x0020
#define FROM_REAR2_SENSOR_ID_ADDR           0x0020
#define FROM_REAR2_DUAL_TILT_X              0x0020
#define FROM_REAR2_DUAL_TILT_Y              0x0020
#define FROM_REAR2_DUAL_TILT_Z              0x0020
#define FROM_REAR2_DUAL_TILT_SX             0x0020
#define FROM_REAR2_DUAL_TILT_SY             0x0020
#define FROM_REAR2_DUAL_TILT_RANGE          0x0020
#define FROM_REAR2_DUAL_TILT_MAX_ERR        0x0020
#define FROM_REAR2_DUAL_TILT_AVG_ERR        0x0020
#define FROM_REAR2_DUAL_TILT_DLL_VERSION    0x0020
#define FROM_REAR2_MTF_ADDR                 0x0020



#define FROM_MTF_SIZE                       54


#else


#define FRONT_CAM_MAP_VERSION_ADDR          0x0043
#define REAR_CAM_MAP_VERSION_ADDR           0x0063

#define FROM_REAR_AF_CAL_D10_ADDR           0x3C30
/*#define FROM_REAR_AF_CAL_D20_ADDR*/
/*#define FROM_REAR_AF_CAL_D30_ADDR*/
#define FROM_REAR_AF_CAL_D40_ADDR           0x3C2C
#define FROM_REAR_AF_CAL_D50_ADDR           0x3C28
/*#define FROM_REAR_AF_CAL_D60_ADDR*/
/*#define FROM_REAR_AF_CAL_D70_ADDR*/
#define FROM_REAR_AF_CAL_D80_ADDR           0x3C20
#define FROM_REAR_AF_CAL_PAN_ADDR           0x3C24
#define FROM_REAR_AF_CAL_MACRO_ADDR         0x3C30
#define FROM_FRONT_AF_CAL_PAN_ADDR          0x0104
#define FROM_FRONT_AF_CAL_MACRO_ADDR        0x0110

#if defined(CONFIG_SEC_KELLYLTE_PROJECT)
#define FROM_FRONT_SENSOR_ID_ADDR           0x004E
#else
#define FROM_FRONT_SENSOR_ID_ADDR           0x0054
#endif

#define FROM_REAR_SENSOR_ID_ADDR            0x00B8


#if 1//defined(CONFIG_SAMSUNG_MULTI_CAMERA)
/*#define FROM_REAR2_AF_CAL_D10_ADDR*/
/*#define FROM_REAR2_AF_CAL_D20_ADDR*/
/*#define FROM_REAR2_AF_CAL_D30_ADDR*/
#define FROM_REAR2_AF_CAL_D40_ADDR          0x555C
#define FROM_REAR2_AF_CAL_D50_ADDR          0x5558
/*#define FROM_REAR2_AF_CAL_D60_ADDR*/
/*#define FROM_REAR2_AF_CAL_D70_ADDR*/
#define FROM_REAR2_AF_CAL_D80_ADDR          0x5550
/*#define FROM_REAR2_AF_CAL_MACRO_ADDR*/
#define FROM_REAR2_AF_CAL_PAN_ADDR          0x5554
#define FROM_REAR2_SENSOR_ID_ADDR           0x00C8
#define FROM_REAR2_DUAL_TILT_X              0x595c
#define FROM_REAR2_DUAL_TILT_Y              0x5960
#define FROM_REAR2_DUAL_TILT_Z              0x5964
#define FROM_REAR2_DUAL_TILT_SX             0x59BC
#define FROM_REAR2_DUAL_TILT_SY             0x59C0
#define FROM_REAR2_DUAL_TILT_RANGE          0x5AE0
#define FROM_REAR2_DUAL_TILT_MAX_ERR        0x5AE4
#define FROM_REAR2_DUAL_TILT_AVG_ERR        0x5AE8
#define FROM_REAR2_DUAL_TILT_DLL_VERSION    0x5ADC
#define FROM_REAR_DUAL_CAL_ADDR             0x5900
#define FROM_REAR_DUAL_CAL_SIZE             512
#endif


#define FROM_REAR_AF_CAL_SIZE           10
#define FROM_SENSOR_ID_SIZE             16

/* Module ID : 0x00A8~0x00B7(16Byte) for FROM, EEPROM (Don't support OTP)*/
#define FROM_MODULE_ID_ADDR                 0x00AE
#define FROM_MODULE_ID_SIZE                 10

/* MTF exif : 0x0064~0x0099(54Byte) for FROM, EEPROM */
#define FROM_REAR_MTF_ADDR            0x0170
#define FROM_FRONT_MTF_ADDR           0x0064
#if 1//defined(CONFIG_SAMSUNG_MULTI_CAMERA)
#define FROM_REAR2_MTF_ADDR           0x01A6
#endif
#define FROM_MTF_SIZE                 54



#endif

#define I2C_BAM_MAX_SIZE              (0xEC0)
#define SPI_TRANSFER_MAX_SIZE         (0x20000) //  128K
#if 1//defined(CONFIG_USE_ACTUATOR_FPGA)
#define FPGA_I2C_MAX_SIZE             (0X7F8) //2040byte
#endif

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
};

#endif
