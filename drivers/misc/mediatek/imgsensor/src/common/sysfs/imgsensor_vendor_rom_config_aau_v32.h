#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_AAU_V32_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_AAU_V32_H

#include "imgsensor_eeprom_rear_gw3_v008.h"
#include "imgsensor_eeprom_front_hi2021q_v008.h"
#include "imgsensor_otp_rear2_sr846d_v008.h"
#include "imgsensor_otp_rear3_gc5035b_v007.h"
#include "imgsensor_otp_rear4_gc5035_v008.h"
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

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_AAU_V32_H*/
