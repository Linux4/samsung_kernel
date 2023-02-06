#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAV_V13VE_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAV_V13VE_H

#include "imgsensor_eeprom_rear_s5kjn1.h"
#include "imgsensor_otp_front_4ha.h"
#include "imgsensor_otp_rear2_gc5035.h"
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

#if IS_ENABLED(CONFIG_IMGSENSOR_SYSFS)
#define IMGSENSOR_HW_PARAM
#endif

#ifdef IMGSENSOR_HW_PARAM
#define S5KJN1_CAL_SENSOR_POSITION SENSOR_POSITION_REAR
#define S5K4HA_CAL_SENSOR_POSITION SENSOR_POSITION_FRONT
#define GC5035_CAL_SENSOR_POSITION SENSOR_POSITION_REAR2
#define GC02M1B_CAL_SENSOR_POSITION SENSOR_POSITION_REAR3
#define GC02M1_CAL_SENSOR_POSITION SENSOR_POSITION_REAR4
#endif

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAV_V13VE_H*/
