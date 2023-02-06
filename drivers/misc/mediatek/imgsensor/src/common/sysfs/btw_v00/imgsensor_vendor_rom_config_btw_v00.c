#include "imgsensor_vendor_rom_config_btw_v00.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,    IMX355_SENSOR_ID,   &rear_imx355_cal_addr},
	{SENSOR_POSITION_FRONT,   GC5035_SENSOR_ID,   &front_gc5035_cal_addr},
};
