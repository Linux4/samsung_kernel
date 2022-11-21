#include "imgsensor_vendor_rom_config_aat_v32x.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX] = {
	{SENSOR_POSITION_REAR,        S5KGM2_SENSOR_ID, &rear_gm2_cal_addr},
	{SENSOR_POSITION_FRONT,       S5K3L6_SENSOR_ID, &front_3l6_cal_addr},
	{SENSOR_POSITION_REAR2,       SR846D_SENSOR_ID, &rear2_sr846d_cal_addr},
	{SENSOR_POSITION_REAR3,      GC02M1B_SENSOR_ID, &rear3_gc02m1b_cal_addr},
	{SENSOR_POSITION_REAR4,       GC5035_SENSOR_ID, &rear4_gc5035_cal_addr},
};
