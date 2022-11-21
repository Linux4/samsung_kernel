#include "imgsensor_vendor_rom_config_mmv_v53x.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,    S5KHM6_SENSOR_ID,   &rear_s5khm6_cal_addr},
	{SENSOR_POSITION_FRONT,   IMX616_SENSOR_ID,   &front_imx616_cal_addr},
	{SENSOR_POSITION_REAR2,   IMX355_SENSOR_ID,   &rear2_imx355_cal_addr},
	{SENSOR_POSITION_REAR3,   GC02M1B_SENSOR_ID,  &rear3_gc02m1b_cal_addr},
	{SENSOR_POSITION_REAR4,   GC02M1_SENSOR_ID,   &rear4_gc02m1_cal_addr},
};
