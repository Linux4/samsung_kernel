#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_MMV_V53X_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_MMV_V53X_H

#include "imgsensor_vendor_specific.h"
#include "kd_imgsensor.h"
#include "imgsensor_eeprom_rear_s5khm6.h"
#include "imgsensor_eeprom_front_imx616.h"
#include "imgsensor_eeprom_rear2_imx355.h"
#include "imgsensor_eeprom_rear4_gc02m1.h"
#include "imgsensor_otp_rear3_gc02m1b.h"

#define FRONT_CAMERA
#define REAR_CAMERA
#define REAR_CAMERA2
#define REAR_CAMERA3
#define REAR_CAMERA4

#define REAR_SUB_CAMERA
//#define USE_SHARED_ROM_REAR3

#if IS_ENABLED(CONFIG_IMGSENSOR_SYSFS)
#define IMGSENSOR_HW_PARAM
#endif

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_MMV_V53X_H*/
