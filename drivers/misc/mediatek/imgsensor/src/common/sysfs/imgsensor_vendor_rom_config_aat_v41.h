#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V41_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V41_H
#include "imgsensor_vendor_specific.h"
#include "kd_imgsensor.h"
#include "imgsensor_eeprom_rear_imx582_gc5035_v001.h"
#include "imgsensor_eeprom_front_imx576_v001.h"
#include "imgsensor_eeprom_front_s5k2x5sp13_v001.h"
#include "imgsensor_eeprom_rear2_4ha_v002.h"

#define FRONT_CAMERA
#define REAR_CAMERA
#define REAR_CAMERA2
#define REAR_CAMERA3

#define REAR_SUB_CAMERA
#define USE_SHARED_ROM_REAR3

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];
#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V41_H*/

