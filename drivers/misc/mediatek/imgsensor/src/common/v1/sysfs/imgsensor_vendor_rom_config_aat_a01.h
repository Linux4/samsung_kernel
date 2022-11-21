#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAT_A01_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAT_A01_H

#include "imgsensor_eeprom_rear_4ha_v001.h"
#include "imgsensor_eeprom_rear_gc8034_v001.h"
#include "imgsensor_otprom_front_gc5035_v001.h"
#include "imgsensor_otprom_front_s5k5e9yx_v001.h"

#define FRONT_CAMERA
#define REAR_CAMERA

const struct imgsensor_vendor_rom_info vendor_rom_info[] = {
	{SENSOR_POSITION_REAR,    S5K4HAYX_SENSOR_ID, &rear_4ha_cal_addr},
	{SENSOR_POSITION_REAR,      GC8034_SENSOR_ID, &rear_gc8034_cal_addr},
	{SENSOR_POSITION_FRONT,     GC5035_SENSOR_ID, &front_gc5035_cal_addr},
	{SENSOR_POSITION_FRONT,   S5K5E9YX_SENSOR_ID, &front_s5k5e9yx_cal_addr},
};

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAT_A01_H*/

