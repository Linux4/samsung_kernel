#include "imgsensor_vendor_rom_config_aat_v31.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX] = {
	{SENSOR_POSITION_REAR,        S5KGM2_SENSOR_ID, &rear_gm2_gc5035_cal_addr},
	{SENSOR_POSITION_FRONT,      HI2021Q_SENSOR_ID, &front_hi2021_cal_addr},
	{SENSOR_POSITION_REAR2,     S5K4HAYX_SENSOR_ID, &rear2_4ha_cal_addr},
	{SENSOR_POSITION_REAR4,       GC5035_SENSOR_ID, &rear4_gc5035_cal_addr},
};
