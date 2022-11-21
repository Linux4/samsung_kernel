#ifndef FIMC_IS_VENDOR_CONFIG_A5_A7_Y17_H
#define FIMC_IS_VENDOR_CONFIG_A5_A7_Y17_H

#include "fimc-is-eeprom-rear-3p8_v002.h"
#include "fimc-is-eeprom-front-3p8_v001.h"

#define VENDER_PATH

#define CAMERA_MODULE_ES_VERSION_REAR 'A'
#define CAMERA_MODULE_ES_VERSION_FRONT 'A'
#define CAL_MAP_ES_VERSION_REAR '2'
#define CAL_MAP_ES_VERSION_FRONT '1'

#define CAMERA_SYSFS_V2

/* Support PDAF SW Solution */
#define USE_AP_PDAF /* S5K3P8 with PDAF */

//#define USE_SSRM_CAMERA_INFO /* Match with SAMSUNG_SSRM define of Camera Hal side */

#define USE_COMMON_CAM_IO_PWR

//#define USE_SMART_BINNING_FRONT /* smart binning 2 */
//#define USE_SMART_BINNING_REAR /* smart binning 2 */

/* Temp : Remove Me */
//#define S5K3P8_PDAF_DISABLE
//#define S5K3P8_TAIL_DISABLE
//#define S5K3P8_BPC_DISABLE
//#define SKIP_CHECK_CRC

/* When read rear eeprom, if it is need to input AF power, add actuator feature */
#if defined(CONFIG_CAMERA_ACT_DW9807_OBJ) || defined(CONFIG_CAMERA_ACT_ZC533_OBJ)
#define USE_AF_PWR_READ_EEPROM
#endif

#if defined(CONFIG_OIS_USE)
#define CAMERA_OIS_GYRO_OFFSET_SPEC 30000

/* Force to Set as Centering mode in OIS(BU24219) */
//#define FORCE_OPTICAL_STABILIZATION_MODE_CENTERING
#endif

#define USE_CAMERA_HW_BIG_DATA

#ifdef USE_CAMERA_HW_BIG_DATA
#define CSI_SCENARIO_SEN_REAR	(0) /* This value follows dtsi */ 
#define CSI_SCENARIO_SEN_FRONT	(1)
#endif

#endif /* FIMC_IS_VENDOR_CONFIG_A5_A7_Y17_H */
