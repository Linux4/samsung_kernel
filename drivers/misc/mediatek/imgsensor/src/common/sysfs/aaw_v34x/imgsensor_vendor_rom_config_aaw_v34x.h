// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Inc.
 */

#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAW_V34X_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAW_V34X_H

#include "imgsensor_eeprom_rear_imx582.h"
#include "imgsensor_eeprom_front_imx258.h"
#include "imgsensor_otp_rear2_4ha.h"
#include "imgsensor_otp_rear3_gc5035.h"
#include "imgsensor_vendor_specific.h"
#include "kd_imgsensor.h"

#define FRONT_CAMERA
#define REAR_CAMERA
#define REAR_CAMERA2
#define REAR_CAMERA3

#define REAR_SUB_CAMERA
//#define USE_SHARED_ROM_REAR3

#if IS_ENABLED(CONFIG_IMGSENSOR_SYSFS)
#define IMGSENSOR_HW_PARAM
#endif

#ifdef IMGSENSOR_HW_PARAM
#define IMX582_CAL_SENSOR_POSITION SENSOR_POSITION_REAR
#define IMX258_CAL_SENSOR_POSITION SENSOR_POSITION_FRONT
#define S5K4HA_CAL_SENSOR_POSITION SENSOR_POSITION_REAR2
#define GC5035_CAL_SENSOR_POSITION SENSOR_POSITION_REAR3
#endif

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAW_V34X_H*/
