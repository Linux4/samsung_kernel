// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Samsung Electronics Inc.
 */

#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAX_V15_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAX_V15_H

#include "imgsensor_eeprom_rear_hi5022q.h"
#include "imgsensor_eeprom_rear_s5kjn1.h"
#include "imgsensor_eeprom_front_gc13a0.h"
#include "imgsensor_otp_rear2_sc501cs.h"
#include "imgsensor_eeprom_rear3_gc02m2.h"
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

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAX_V15_H*/
