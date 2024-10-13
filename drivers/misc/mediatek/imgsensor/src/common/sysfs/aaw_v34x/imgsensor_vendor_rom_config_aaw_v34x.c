// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 Samsung Electronics Inc.
 */

#include "imgsensor_vendor_rom_config_aaw_v34x.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,   IMX582_SENSOR_ID,    &rear_imx582_cal_addr},
	{SENSOR_POSITION_FRONT,  IMX258_SENSOR_ID,    &front_imx258_cal_addr},
	{SENSOR_POSITION_FRONT,  HI1339_SENSOR_ID,    &front_hi1339_cal_addr},
	{SENSOR_POSITION_REAR2,  S5K4HAYX_SENSOR_ID,  &rear2_4ha_cal_addr},
	{SENSOR_POSITION_REAR2,  SR846D_SENSOR_ID,    &rear2_sr846d_cal_addr},
	{SENSOR_POSITION_REAR3,  GC5035_SENSOR_ID,    &rear3_gc5035_cal_addr},
};
