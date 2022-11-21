#include "imgsensor_vendor_rom_config_aau_v32.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX] = {
	{SENSOR_POSITION_REAR,        S5KGW3_SENSOR_ID, &rear_gw3_cal_addr},
	{SENSOR_POSITION_FRONT,      HI2021Q_SENSOR_ID, &front_hi2021q_cal_addr},
	{SENSOR_POSITION_REAR2,       SR846D_SENSOR_ID, &rear2_sr846d_cal_addr},
	{SENSOR_POSITION_REAR3,      GC5035B_SENSOR_ID, &rear3_gc5035b_cal_addr},
	{SENSOR_POSITION_REAR4,       GC5035_SENSOR_ID, &rear4_gc5035_cal_addr},
};
