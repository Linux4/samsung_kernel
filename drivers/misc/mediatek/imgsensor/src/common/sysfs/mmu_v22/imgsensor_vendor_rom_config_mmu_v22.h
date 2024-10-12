#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_MMU_V22_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_MMU_V22_H

#include "imgsensor_eeprom_rear_gm2.h"
#include "imgsensor_eeprom_front_imx258.h"
#include "imgsensor_eeprom_rear2_imx355.h"
#include "imgsensor_otp_rear3_gc02m1b.h"
#include "imgsensor_eeprom_rear4_gc02m1.h"
#include "imgsensor_vendor_specific.h"
#include "kd_imgsensor.h"

#define FRONT_CAMERA
#define REAR_CAMERA
#define REAR_CAMERA2
#define REAR_CAMERA3
#define REAR_CAMERA4

#define REAR_SUB_CAMERA
//#define USE_SHARED_ROM_REAR3

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_MMU_V22_H*/
