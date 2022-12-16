#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V32_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V32_H

#include "imgsensor_eeprom_rear_gm2_v008_2.h"
#include "imgsensor_otp_front_3l6_v007_2.h"
#include "imgsensor_otp_rear2_sr846d_v008.h"
#include "imgsensor_otp_rear3_gc02m1b_v007.h"
#include "imgsensor_otp_rear4_gc5035_v008.h"
#include "imgsensor_vendor_specific.h"
#include "kd_imgsensor.h"

#define FRONT_CAMERA
#define REAR_CAMERA
#define REAR_CAMERA2
#define REAR_CAMERA3
#define REAR_CAMERA4

#define REAR_SUB_CAMERA
#define USE_SHARED_ROM_REAR3

#ifdef CONFIG_IMGSENSOR_SYSFS
#define IMGSENSOR_HW_PARAM
#endif

#ifdef IMGSENSOR_HW_PARAM
#define S5KGM2_CAL_SENSOR_POSITION SENSOR_POSITION_REAR
#define S5K3L6_CAL_SENSOR_POSITION SENSOR_POSITION_FRONT
#define SR846D_CAL_SENSOR_POSITION SENSOR_POSITION_REAR2
#define GC02M1B_CAL_SENSOR_POSITION SENSOR_POSITION_REAR3
#define GC5035_CAL_SENSOR_POSITION SENSOR_POSITION_REAR4
#endif

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAT_V32_H*/
