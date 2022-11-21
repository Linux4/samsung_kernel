#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAU_V02EX_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAU_V02EX_H

#include "imgsensor_eeprom_rear_imx258_v008.h"
#include "imgsensor_otp_front_gc5035_v008.h"
#include "imgsensor_vendor_specific.h"
#include "kd_imgsensor.h"

#define FRONT_CAMERA
#define REAR_CAMERA
//#define REAR2_CAMERA

#define REAR_SUB_CAMERA
//#define USE_SHARED_ROM_REAR3

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAU_V02EX_H*/
