// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Samsung Electronics Inc.
 */

#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_XXX_V07_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_XXX_V07_H

#include "imgsensor_eeprom_rear_s5kjn1.h"
#include "imgsensor_otp_front_gc5035.h"
#include "kd_imgsensor.h"

#define FRONT_CAMERA
#define REAR_CAMERA

#if IS_ENABLED(CONFIG_IMGSENSOR_SYSFS)
#define IMGSENSOR_HW_PARAM
#endif

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_XXX_V07_H*/
