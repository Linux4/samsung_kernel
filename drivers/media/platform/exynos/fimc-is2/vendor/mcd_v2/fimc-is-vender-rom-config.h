/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_VENDER_ROM_CONFIG_H
#define FIMC_IS_VENDER_ROM_CONFIG_H

#include "fimc-is-vender-specific.h"

#if defined(CONFIG_CAMERA_A7Y18)
#include "fimc-is-vender-rom-config_a7y18.h"
#elif defined(CONFIG_CAMERA_WISDOM)
#include "fimc-is-vender-rom-config_wisdom.h"
#elif defined(CONFIG_CAMERA_GTA3XL)
#include "fimc-is-vender-rom-config_gta3xl.h"
#elif defined(CONFIG_CAMERA_A30)
#include "fimc-is-vender-rom-config_a30.h"
#elif defined(CONFIG_CAMERA_A30JPN)
#include "fimc-is-vender-rom-config_a30jpn.h"
#elif defined(CONFIG_CAMERA_A40)
#include "fimc-is-vender-rom-config_a40.h"
#elif defined(CONFIG_CAMERA_A20)
#include "fimc-is-vender-rom-config_a20.h"
#elif defined(CONFIG_CAMERA_A30CHN)
#include "fimc-is-vender-rom-config_a30chn.h"
#elif defined(CONFIG_CAMERA_A10)
#include "fimc-is-vender-rom-config_a10.h"
#elif defined(CONFIG_CAMERA_A20E)
#include "fimc-is-vender-rom-config_a20e.h"
#elif defined(CONFIG_CAMERA_A10E)
#include "fimc-is-vender-rom-config_a10e.h"
#else

const struct fimc_is_vender_rom_addr *vender_rom_addr[SENSOR_POSITION_MAX] = {
	NULL,		//[0] SENSOR_POSITION_REAR
	NULL,		//[1] SENSOR_POSITION_FRONT
	NULL,		//[2] SENSOR_POSITION_REAR2
	NULL,		//[3] SENSOR_POSITION_FRONT2
	NULL,		//[4] SENSOR_POSITION_REAR3
	NULL,		//[5] SENSOR_POSITION_FRONT3
};

#endif
#endif
