// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Inc.
 */

#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_H

#include "imgsensor_vendor_specific.h"

#if defined(CONFIG_CAMERA_STX_V10P)
#include "./stx_v10p/imgsensor_vendor_rom_config_stx_v10p.h"
#elif defined(CONFIG_CAMERA_STX_V10U)
#include "./stx_v10u/imgsensor_vendor_rom_config_stx_v10u.h"
#else
//default
#include "imgsensor_vendor_rom_config_default.h"
#endif

extern const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX];

#endif
