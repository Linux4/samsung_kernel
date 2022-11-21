#include "imgsensor_vendor_rom_config_aat_v41.h"

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,      IMX582_SENSOR_ID, &rear_imx582_gc5035_cal_addr},
	{SENSOR_POSITION_FRONT,     IMX576_SENSOR_ID, &front_imx576_cal_addr},
	{SENSOR_POSITION_FRONT, S5K2X5SP13_SENSOR_ID, &front_s5k2x5sp13_cal_addr},
	{SENSOR_POSITION_REAR2,   S5K4HAYX_SENSOR_ID, &rear2_4ha_cal_addr},
};