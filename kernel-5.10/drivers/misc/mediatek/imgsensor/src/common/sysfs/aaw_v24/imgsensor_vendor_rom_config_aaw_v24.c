// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Inc.
 */

#include "imgsensor_vendor_rom_config_aaw_v24.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,   S5KJN1_SENSOR_ID,    &rear_s5kjn1_cal_addr},
	{SENSOR_POSITION_FRONT,  IMX258_SENSOR_ID,   &front_imx258_cal_addr},
	{SENSOR_POSITION_FRONT,  HI1339_SENSOR_ID,    &front_hi1339_cal_addr},
	{SENSOR_POSITION_REAR2,  GC5035_SENSOR_ID,   &rear2_gc5035_cal_addr},
	{SENSOR_POSITION_REAR3,  GC02M1_SENSOR_ID,    &rear3_gc02m1_cal_addr},
};
