#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V31_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V31_H
#include "imgsensor_vendor_specific.h"
#include "kd_imgsensor.h"
// To Enable compilation of CAL Files
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

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V31_H*/

