#ifndef IS_VENDOR_CONFIG_AAV_V14_H
#define IS_VENDOR_CONFIG_AAV_V14_H

#define VENDER_PATH

/***** CAL ROM DEFINE *****/
#define ROM_DEBUG
#define ROM_CRC32_DEBUG
//#define SKIP_CHECK_CRC                                      /* Skip the CRC CHECK of cal data */

//#define USE_AE_CAL                                          /* USE Ae cal data in rear camera, Only VENDER_CAL_STRUCT_VER2 */

#define SUPPORT_SENSOR_DUALIZATION                            /* Support dualization */
// #define BOKEH_NO_ROM_SUPPORT                                  /* Support dualization for no rom bokeh sensor */
#define APPLY_MIRROR_VERTICAL_FLIP                            /* Need To Apply Mirror and Vertical Flip */

#define USE_CAMERA_IOVM_BEST_FIT

/***** SUPPORT CAMERA DEFINE *****/
#define IS_VENDOR_SENSOR_COUNT 4                              /* REAR_0, FRONT_0, REAR_2, REAR_3 */

//#define REAR_SUB_CAMERA            (SENSOR_POSITION_REAR2)  /* Supported Camera Type for rear bokeh */
#define REAR_ULTRA_WIDE_CAMERA     (SENSOR_POSITION_REAR3)    /* Supported Camera Type for rear ultra wide */
#define REAR_MACRO_CAMERA          (SENSOR_POSITION_REAR4)    /* Supported Camera Type for rear ultra wide */
//#define FRONT_SUB_CAMERA         (SENSOR_POSITION_FRONT2)   /* Supported Camera Type for front bokeh */

#define CAMERA_REAR2             (REAR_ULTRA_WIDE_CAMERA)     /* For Rear2 of SYSFS */
#define CAMERA_REAR3               (REAR_MACRO_CAMERA)        /* For Rear3 of SYSFS */

#define CAMERA_REAR2_MODULEID
#define CAMERA_REAR3_MODULEID
#define CAMERA_REAR_DUAL_CAL

#define SENSOR_OTP_HI1336B
#define SENSOR_OTP_HI556
#define SENSOR_OTP_HI556_STANDARD_CAL

#define USES_STANDARD_CAL_RELOAD
#define USE_PERSISTENT_DEVICE_PROPERTIES_FOR_CAL /* For cal reload */

#define CONFIG_SEC_CAL_ENABLE
#define IS_REAR_MAX_CAL_SIZE (0x531A)
#define IS_FRONT_MAX_CAL_SIZE (0x1C84)
#define IS_REAR2_MAX_CAL_SIZE (0x1B74)
#define IS_REAR4_MAX_CAL_SIZE (0x1DC0)

#define CAMERA_EEPROM_SUPPORT_FRONT
#define CONFIG_CAMERA_EEPROM_DUALIZED

#define READ_DUAL_CAL_FIRMWARE_DATA
#define DUAL_CAL_DATA_SIZE_DEFAULT (0x080C)

#ifdef USE_KERNEL_VFS_READ_WRITE
#define DUAL_CAL_DATA_PATH "/vendor/firmware/SetMultiCalInfo.bin"
#else
#define DUAL_CAL_DATA_PATH "/vendor/firmware/"
#define DUAL_CAL_DATA_BIN_NAME "SetMultiCalInfo.bin"
#endif

#define USE_CAMERA_ADAPTIVE_MIPI
/***** SUPPORT FUCNTION DEFINE *****/
#define SAMSUNG_LIVE_OUTFOCUS                                 /* Allocate memory For Dual Camera */
#define ENABLE_REMOSAIC_CAPTURE                               /* Base Remosaic */
#define ENABLE_REMOSAIC_CAPTURE_WITH_ROTATION                 /* M2M and Rotation is used during Remosaic */
#define CHAIN_USE_STRIPE_PROCESSING  1                        /* Support STRIPE_PROCESSING during Remosaic */
#define USE_AP_PDAF                                           /* Support sensor PDAF SW Solution */
//#define USE_SENSOR_WDR                                      /* Support sensor WDR */
//#define SUPPORT_SENSOR_3DHDR                                /* whether capable of 3DHDR or not */
//#define SUPPORT_REMOSAIC_CROP_ZOOM
//#undef OVERFLOW_PANIC_ENABLE_CSIS                           /* Not Support Kernel Panic when CSIS OVERFLOW */
#define DISABLE_DUAL_SYNC
#define USE_MONO_SENSOR
#define CAMERA_FRONT_FIXED_FOCUS
#define CAMERA_STANDARD_CAL_ISP_VERSION 'E'

/***** DDK - DRIVER INTERFACE *****/
/* This feature since A50s(ramen) and A30s(lassen).
   In previous projects, only A30 / A20 / A20p is applied for sensor duplication */
#define USE_DEBUG_LIBRARY_VER

//#define USE_FACE_UNLOCK_AE_AWB_INIT                         /* for Face Unlock */


/***** HW DEFENDANT DEFINE *****/
#define USE_CAMERA_ACT_DRIVER_SOFT_LANDING                    /* Using NRC based softlanding for FP5529 */
#define USE_COMMON_CAM_IO_PWR                                 /* CAM_VDDIO_1P8 Power is used commonly for all camera and EEPROM */


/***** SUPPORT EXTERNEL FUNCTION DEFINE *****/
#define USE_SSRM_CAMERA_INFO                                  /* Match with SAMSUNG_SSRM define of Camera Hal side */

#define USE_CAMERA_HW_BIG_DATA
#ifdef USE_CAMERA_HW_BIG_DATA
//#define USE_CAMERA_HW_BIG_DATA_FOR_PANIC
#define CSI_SCENARIO_SEN_REAR	(0)                           /* This value follows dtsi */
#define CSI_SCENARIO_SEN_FRONT	(1)
#endif

//#define SM5714_DEBUG_PRINT_REGMAP

/* For the demands to modify DVFS CAM Level only in 16:9 ratio of rear recording.
 * This modification is only limited to A21s.
 */
//#define USE_SPECIFIC_MIPISPEED

#define JN1_MODIFY_REMOSAIC_CAL_ORDER
#define MODIFY_CAL_MAP_FOR_SWREMOSAIC_LIB
#define PUT_30MS_BETWEEN_EACH_CAL_LOADING

#endif /* IS_VENDOR_CONFIG_AAV_V14_H */

#define CONFIG_LEDS_S2MU106_FLASH /* Flash config */

