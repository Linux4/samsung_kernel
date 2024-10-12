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

#if defined(CONFIG_CAMERA_MMV_V53X)
#include "mmv_v53x/imgsensor_vendor_rom_config_mmv_v53x.h"
#elif defined(CONFIG_CAMERA_AAU_V13X)
#include "aau_v13x/imgsensor_vendor_rom_config_aau_v13x.h"
#elif defined(CONFIG_CAMERA_MMV_V13X)
#include "mmv_v13x/imgsensor_vendor_rom_config_mmv_v13x.h"
#elif defined(CONFIG_CAMERA_AAV_V13VE)
#include "aav_v13ve/imgsensor_vendor_rom_config_aav_v13ve.h"
#elif defined(CONFIG_CAMERA_MMV_V13VE)
#include "mmv_v13ve/imgsensor_vendor_rom_config_mmv_v13ve.h"
#elif defined(CONFIG_CAMERA_AAT_V32X)
#include "aat_v32x/imgsensor_vendor_rom_config_aat_v32x.h"
#elif defined(CONFIG_CAMERA_AAU_V32)
#include "aau_v32/imgsensor_vendor_rom_config_aau_v32.h"
#elif defined(CONFIG_CAMERA_MMU_V32)
#include "mmu_v32/imgsensor_vendor_rom_config_mmu_v32.h"
#elif defined(CONFIG_CAMERA_AAT_V31)
#include "aat_v31/imgsensor_vendor_rom_config_aat_v31.h"
#elif defined(CONFIG_CAMERA_AAT_V41)
#include "aat_v41/imgsensor_vendor_rom_config_aat_v41.h"
#elif defined(CONFIG_CAMERA_AAU_V22)
#include "aau_v22/imgsensor_vendor_rom_config_aau_v22.h"
#elif defined(CONFIG_CAMERA_MMU_V22)
#include "mmu_v22/imgsensor_vendor_rom_config_mmu_v22.h"
#elif defined(CONFIG_CAMERA_AAU_V22EX)
#include "aau_v22ex/imgsensor_vendor_rom_config_aau_v22ex.h"
#elif defined(CONFIG_CAMERA_AAU_V02)
#include "aau_v02/imgsensor_vendor_rom_config_aau_v02.h"
#else
//default
#include "imgsensor_vendor_rom_config_default.h"
#endif

extern const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX];

#endif
