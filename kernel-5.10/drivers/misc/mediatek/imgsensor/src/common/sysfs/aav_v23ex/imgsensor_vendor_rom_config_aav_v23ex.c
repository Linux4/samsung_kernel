// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Inc.
 */

#include "imgsensor_vendor_rom_config_aav_v23ex.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,    S5KJN1_SENSOR_ID,   &rear_s5kjn1_cal_addr},
	{SENSOR_POSITION_FRONT,   GC5035F_SENSOR_ID,  &front_gc5035_cal_addr},
};
