// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Inc.
 */

#include "imgsensor_vendor_rom_config_stx_v10p.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,  HI1337_SENSOR_ID,   &rear_hi1337_cal_addr},
	{SENSOR_POSITION_REAR2, HI847_SENSOR_ID,    &rear2_hi847_cal_addr},
	{SENSOR_POSITION_FRONT, HI1337FU_SENSOR_ID, &front_hi1337fu_cal_addr},
};
