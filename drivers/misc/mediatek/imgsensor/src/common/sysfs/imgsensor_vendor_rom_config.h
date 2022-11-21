/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_H

#include "imgsensor_vendor_specific.h"

#if defined(CONFIG_CAMERA_AAT_V32X)
#include "imgsensor_vendor_rom_config_aat_v32x.h"
#elif defined(CONFIG_CAMERA_AAU_V32)
#include "imgsensor_vendor_rom_config_aau_v32.h"
#else
//default
#include "imgsensor_vendor_rom_config_aat_v32x.h"
#endif

extern const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX];

#endif
