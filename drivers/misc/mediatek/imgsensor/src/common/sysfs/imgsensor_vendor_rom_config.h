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
#elif defined(CONFIG_CAMERA_AAT_V31)
#include "imgsensor_vendor_rom_config_aat_v31.h"
#elif defined(CONFIG_CAMERA_AAT_V41)
#include "imgsensor_vendor_rom_config_aat_v41.h"
#elif defined(CONFIG_CAMERA_AAU_V22)
#include "imgsensor_vendor_rom_config_aau_v22.h"
#elif defined(CONFIG_CAMERA_AAT_V01)
#include "imgsensor_vendor_rom_config_aat_v01.h"
#elif defined(CONFIG_CAMERA_AAU_V02)
#include "imgsensor_vendor_rom_config_aau_v02.h"
#elif defined(CONFIG_CAMERA_MMU_V32)
#include "imgsensor_vendor_rom_config_mmu_v32.h"
#elif defined(CONFIG_CAMERA_MMU_V22)
#include "imgsensor_vendor_rom_config_mmu_v22.h"
#else
//default
#include "imgsensor_vendor_rom_config_default.h"
#endif

extern const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX];

#endif
