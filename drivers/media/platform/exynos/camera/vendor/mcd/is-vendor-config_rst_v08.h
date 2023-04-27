#ifndef IS_VENDOR_CONFIG_RST_V08_H
#define IS_VENDOR_CONFIG_RST_V08_H

#include "is-eeprom-rear-2ld_v019.h"

#define VENDER_PATH

#define CAMERA_REAR_DUAL_CAL
#define CAMERA_REAR2
#define CAMERA_REAR2_AF /* related to OIS */
#define CAMERA_REAR2_TILT
#define CAMERA_REAR2_MODULEID
#define CAMERA_REAR3
#define CAMERA_REAR3_AFCAL
#define CAMERA_REAR3_TILT
#define CAMERA_REAR3_MODULEID

#define CAMERA_USE_OIS_VDD_1_8V

#define CAMERA_OIS_DOM_UPDATE_VERSION 'O'
#define CAMERA_OIS_SEC_UPDATE_VERSION 'P'

#define CAMERA_MODULE_DUAL_CAL_AVAILABLE_VERSION 'A'
#define IS_MAX_FW_BUFFER_SIZE (4100 * 1024)

#define CAMERA_OIS_GYRO_OFFSET_SPEC 10000

#define CAMERA_REAR2_SENSOR_SHIFT_CROP

#define CAMERA_2ND_OIS

#define RTA_CODE_AREA_SIZE (0x00180000)

#define USE_CAMERA_EMBEDDED_HEADER

#define USE_CAMERA_MIPI_CLOCK_VARIATION
#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
/*#define USE_CAMERA_MIPI_CLOCK_VARIATION_RUNTIME*/
#endif

/* #define USE_CAMERA_CHECK_SENSOR_REV */

#define USE_CAMERA_HW_BIG_DATA

#ifdef USE_CAMERA_HW_BIG_DATA
/* #define CAMERA_HW_BIG_DATA_FILE_IO */
/* #define CSI_SCENARIO_COMP		(0) This value follows dtsi */
#define CSI_SCENARIO_SEN_FRONT	(1)
#define CSI_SCENARIO_TELE		(2)
#define CSI_SCENARIO_SECURE		(3)
#define CSI_SCENARIO_SEN_REAR	(0)
#endif

#define USE_AF_SLEEP_MODE

/* It should be align with DDK and RTA side */
#define USE_NEW_PER_FRAME_CONTROL

/* define supported aperture level */
// #define ROM_SUPPORT_APERTURE_F2	// Second step of aperture.

/* Tele sensor crop shift and OIS calibration will be applied instead of this feature in this project */
/* #define OIS_CENTERING_SHIFT_ENABLE */
#undef ENABLE_DYNAMIC_MEM

#if defined(CONFIG_USE_CAMERA_LDU) || defined(CONFIG_SEC_FACTORY)
#define USE_OIS_SHIFT_FOR_APERTURE
#endif

#ifdef USE_OIS_SHIFT_FOR_APERTURE
#if defined(CONFIG_SEC_FACTORY)
#define OIS_SHIFT_OFFSET_VALUE_NORMAL 0
#else
#define OIS_SHIFT_OFFSET_VALUE_NORMAL 1000
#endif
#endif

#ifdef CAMERA_REAR_TOF
#define REAR_TOF_ROM_ID ROM_ID_REAR3
#endif

#define USE_SENSOR_LONG_EXPOSURE_SHOT

#define OIS_DUAL_CAL_DEFAULT_VALUE_WIDE 0
#define OIS_DUAL_CAL_DEFAULT_VALUE_TELE 0

#define USE_MCU_SPI_PUD_SETTING

// #define USE_FAKE_RETENTION
#define USE_CAMFW_POLICY_ED38
#define CAMERA_FRONT_FIXED_FOCUS

/*
 * Issue : When 2ld camera open, I2C SCL signal rarely keep LOW especially on certain devices due to long falling time of VDDIO.
 * Solution : Add additional delay between rom_power_off and prepare_retention to make sure VDDIO is LOW during the boot.
 */
#define CAMERA_USE_COMMON_VDDIO

#define TELE_OIS_TILT_ROM_ID 4
#endif /* IS_VENDOR_CONFIG_CNT_V01_H */
