#include "imgsensor_vendor_rom_config_aau_v22ex.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,    IMX258_SENSOR_ID,   &rear_imx258_cal_addr},
	{SENSOR_POSITION_FRONT,   GC5035F_SENSOR_ID,  &front_gc5035_cal_addr},
};
