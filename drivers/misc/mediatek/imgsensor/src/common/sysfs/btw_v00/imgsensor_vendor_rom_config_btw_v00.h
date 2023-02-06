#ifndef IMGESENSOR_VENDOR_ROM_CONFIG_BTW_V00_H
#define IMGESENSOR_VENDOR_ROM_CONFIG_BTW_V00_H

#include "imgsensor_vendor_specific.h"
#include "kd_imgsensor.h"
#include "imgsensor_otp_front_gc5035.h"
#include "imgsensor_eeprom_rear_imx355.h"


#define FRONT_CAMERA
#define REAR_CAMERA

#if IS_ENABLED(CONFIG_IMGSENSOR_SYSFS)
#define IMGSENSOR_HW_PARAM
#endif

#ifdef IMGSENSOR_HW_PARAM
#define IMX355_CAL_SENSOR_POSITION SENSOR_POSITION_REAR
#define GC5035_CAL_SENSOR_POSITION SENSOR_POSITION_FRONT
#endif

extern const struct imgsensor_vendor_rom_info vendor_rom_info[SENSOR_POSITION_MAX];

#endif /*IMGESENSOR_VENDOR_ROM_CONFIG_BTW_V00_H*/
