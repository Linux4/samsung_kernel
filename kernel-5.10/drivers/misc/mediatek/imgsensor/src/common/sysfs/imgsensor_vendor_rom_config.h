// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Inc.
 */

#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_H

#include "imgsensor_vendor_specific.h"

#if defined(CONFIG_CAMERA_AAT_V12)
#include "./aat_v12/imgsensor_vendor_rom_config_aat_v12.h"
#elif defined(CONFIG_CAMERA_AAV_V23EX)
#include "./aav_v23ex/imgsensor_vendor_rom_config_aav_v23ex.h"
#elif defined(CONFIG_CAMERA_AAV_V23)
#include "./aav_v23/imgsensor_vendor_rom_config_aav_v23.h"
#elif defined(CONFIG_CAMERA_AAW_V24)
#include "./aaw_v24/imgsensor_vendor_rom_config_aaw_v24.h"
#elif defined(CONFIG_CAMERA_AAX_V15)
#include "./aax_v15/imgsensor_vendor_rom_config_aax_v15.h"
#else
//default
#include "imgsensor_vendor_rom_config_default.h"
#endif

extern const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX];

#endif
