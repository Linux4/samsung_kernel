// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Inc.
 */

#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_STX_V10U_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_STX_V10U_H

#include "imgsensor_vendor_specific.h"
#include "imgsensor_eeprom_rear_hi1337.h"
#include "imgsensor_otp_rear2_hi847.h"
#include "imgsensor_eeprom_front_hi1337f.h"
#include "imgsensor_eeprom_front_hi1337fu.h"

#include "kd_imgsensor.h"

#define REAR_CAMERA
#define REAR_CAMERA2
#define FRONT_CAMERA
#define FRONT_CAMERA2

#define REAR_FLASH_SYSFS

#if IS_ENABLED(CONFIG_IMGSENSOR_SYSFS)
#define IMGSENSOR_HW_PARAM
#endif

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_STX_V10U_H*/
