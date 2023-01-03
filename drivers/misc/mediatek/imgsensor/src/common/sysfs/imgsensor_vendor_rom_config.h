// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Inc.
 */

#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_H

#include "imgsensor_vendor_specific.h"

#if defined(CONFIG_CAMERA_AAT_V12)
#include "aat_v12/imgsensor_vendor_rom_config_aat_v12.h"
#elif defined(CONFIG_CAMERA_AAV_V13VE)
#include "aav_v13ve/imgsensor_vendor_rom_config_aav_v13ve.h"
#elif defined(CONFIG_CAMERA_AAV_V23EX)
#include "aav_v23ex/imgsensor_vendor_rom_config_aav_v23ex.h"
#elif  defined(CONFIG_CAMERA_MMV_V53X)
#include "mmv_v53x/imgsensor_vendor_rom_config_mmv_v53x.h"
#elif  defined(CONFIG_CAMERA_BTW_V00)
#include "btw_v00/imgsensor_vendor_rom_config_btw_v00.h"
#elif  defined(CONFIG_CAMERA_AAW_V34X)
#include "aaw_v34x/imgsensor_vendor_rom_config_aaw_v34x.h"
#else
//default
#include "imgsensor_vendor_rom_config_default.h"
#endif

extern const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX];

#endif
