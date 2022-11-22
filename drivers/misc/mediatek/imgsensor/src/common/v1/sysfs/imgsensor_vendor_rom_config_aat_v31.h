#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V31_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V31_H

#include "imgsensor_eeprom_rear_gm2_gc5035_v001.h"
#include "imgsensor_eeprom_front_hi2021_v001.h"
#include "imgsensor_eeprom_rear2_4ha_v002.h"
#include "imgsensor_eeprom_rear4_gc5035_v001.h"

#define FRONT_CAMERA
#define REAR_CAMERA
#define REAR_CAMERA2
#define REAR_CAMERA3
#define REAR_CAMERA4

#define REAR_SUB_CAMERA
#define USE_SHARED_ROM_REAR3

const struct imgsensor_vendor_rom_addr *vendor_rom_addr[SENSOR_POSITION_MAX] = {
	&rear_gm2_gc5035_cal_addr,	//[0] SENSOR_POSITION_REAR
	&front_hi2021_cal_addr,		//[1] SENSOR_POSITION_FRONT
	&rear2_4ha_cal_addr, 		//[2] SENSOR_POSITION_REAR2
	NULL,						//[3] SENSOR_POSITION_FRONT2
	NULL,                		//[4] SENSOR_POSITION_REAR3
	NULL,						//[5] SENSOR_POSITION_FRONT3
	&rear4_gc5035_cal_addr,		//[6] SENSOR_POSITION_REAR4
	NULL,						//[7] SENSOR_POSITION_FRONT4
	NULL,						//[8] SP_REAR_TOF
	NULL,						//[9] SP_FRONT_TOF
};

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_A31_H*/

