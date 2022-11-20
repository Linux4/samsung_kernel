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

#if defined(CONFIG_CAMERA_AAU_V02)
#include "imgsensor_vendor_rom_config_aau_v02.h"
#else
//default config
#include "imgsensor_vendor_rom_config_aau_v02.h"
#endif

extern const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX];

#endif
