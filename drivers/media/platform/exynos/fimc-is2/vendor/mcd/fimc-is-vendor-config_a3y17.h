#ifndef FIMC_IS_VENDOR_CONFIG_A3Y17_H
#define FIMC_IS_VENDOR_CONFIG_A3Y17_H

#include "fimc-is-eeprom-rear-3p8_v002.h"
#include "fimc-is-eeprom-front-4h5yc_v002.h"

#define VENDER_PATH

#define CAMERA_MODULE_ES_VERSION_REAR 'A'
#define CAMERA_MODULE_ES_VERSION_FRONT 'A'
#define CAL_MAP_ES_VERSION_REAR '2'
#define CAL_MAP_ES_VERSION_FRONT '2'

#define CAMERA_SYSFS_V2
#define VDD_CAM_CORE_GPIO_CONTROL
#define VDD_CAM_AF_GPIO_CONTROL
#define VDD_CAM_IO_GPIO_CONTROL

#define VDD_VTCAM_CORE_GPIO_CONTROL
#define VDD_VTCAM_IO_GPIO_CONTROL

#define CAMERA_SHARED_IO_POWER	// if used front and rear shared IO power

#endif /* FIMC_IS_VENDOR_CONFIG_A3Y17_H */
