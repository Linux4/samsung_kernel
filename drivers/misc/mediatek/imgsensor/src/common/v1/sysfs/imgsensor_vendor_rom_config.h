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

#if defined(CONFIG_CAMERA_AAT_V31)
#include "imgsensor_vendor_rom_config_aat_v31.h"
#elif defined(CONFIG_CAMERA_AAT_V41)
#include "imgsensor_vendor_rom_config_aat_v41.h"
#else

const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX] = {
	NULL,		//[0] SENSOR_POSITION_REAR
	NULL,		//[1] SENSOR_POSITION_FRONT
	NULL,		//[2] SENSOR_POSITION_REAR2
	NULL,		//[3] SENSOR_POSITION_FRONT2
	NULL,		//[4] SENSOR_POSITION_REAR3
	NULL,		//[5] SENSOR_POSITION_FRONT3
	NULL,		//[6] SENSOR_POSITION_REAR4
	NULL,		//[7] SENSOR_POSITION_FRONT4
	NULL,		//[8] SENSOR_POSITION_REAR_TOF
	NULL,		//[9] SENSOR_POSITION_FRONT_TOF
};
#endif
#endif
