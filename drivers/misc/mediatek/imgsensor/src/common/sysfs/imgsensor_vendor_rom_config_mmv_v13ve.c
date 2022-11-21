#include "imgsensor_vendor_rom_config_mmv_v13ve.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX] = {
	{SENSOR_POSITION_REAR,   S5KJN1_SENSOR_ID,    &rear_s5kjn1_cal_addr},
	{SENSOR_POSITION_FRONT,  S5K4HAYXF_SENSOR_ID, &front_4ha_cal_addr},
	{SENSOR_POSITION_REAR2,  GC5035U_SENSOR_ID,   &rear2_gc5035_cal_addr},
	{SENSOR_POSITION_REAR3,  GC02M1B_SENSOR_ID,   &rear3_gc02m1b_cal_addr},
};
