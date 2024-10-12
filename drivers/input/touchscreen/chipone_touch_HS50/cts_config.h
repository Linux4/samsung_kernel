#ifndef CTS_CONFIG_H
#define CTS_CONFIG_H

/** Driver version */
#define CFG_CTS_DRIVER_MAJOR_VERSION        1
#define CFG_CTS_DRIVER_MINOR_VERSION        3
#define CFG_CTS_DRIVER_PATCH_VERSION        7

#define CFG_CTS_DRIVER_VERSION              "v1.3.7"

/** Whether reset pin is used */
#define CFG_CTS_HAS_RESET_PIN

//#define CONFIG_CTS_I2C_HOST
#ifndef CONFIG_CTS_I2C_HOST

#ifndef CFG_CTS_HAS_RESET_PIN
#define CFG_CTS_HAS_RESET_PIN
#endif

#define CFG_CTS_SPI_SPEED_KHZ               4000

#endif

/* UP event missing by some reason will cause touch remaining when
 * all fingers are lifted or flying line when touch in the next time.
 * Enable following option will prevent this.
 * FIXME:
 *   Currently, one finger UP event missing in multi-touch, it will
 *   report UP to system when all fingers are lifted.
 */
#define CFG_CTS_MAKEUP_EVENT_UP

//#define CFG_CTS_FW_LOG_REDIRECT

/** Whether force download firmware to chip */
//#define CFG_CTS_FIRMWARE_FORCE_UPDATE

/** Use build in firmware or firmware file in fs*/
#define CFG_CTS_DRIVER_BUILTIN_FIRMWARE
#define CFG_CTS_FIRMWARE_IN_FS
#ifdef CFG_CTS_FIRMWARE_IN_FS
    #define CFG_CTS_FIRMWARE_FILENAME       "chipone-tddi.bin"
    #define CFG_CTS_FIRMWARE_FILEPATH       "/vendor/firmware/chipone-tddi.bin"
#endif /* CFG_CTS_FIRMWARE_IN_FS */

#ifdef CONFIG_PROC_FS
    /* Proc FS for backward compatibility for APK tool com.ICN85xx */
    #define CONFIG_CTS_LEGACY_TOOL
#endif /* CONFIG_PROC_FS */

#ifdef CONFIG_SYSFS
    /* Sys FS for gesture report, debug feature etc. */
    #define CONFIG_CTS_SYSFS
#endif /* CONFIG_SYSFS */

#define CFG_CTS_MAX_TOUCH_NUM               (10)

/* Virtual key support */
//#define CONFIG_CTS_VIRTUALKEY
#ifdef CONFIG_CTS_VIRTUALKEY
    #define CFG_CTS_MAX_VKEY_NUM            (4)
    #define CFG_CTS_NUM_VKEY                (3)
    #define CFG_CTS_VKEY_KEYCODES           {KEY_BACK, KEY_HOME, KEY_MENU}
#endif /* CONFIG_CTS_VIRTUALKEY */

/* Gesture wakeup */
#define CFG_CTS_GESTURE
#ifdef CFG_CTS_GESTURE
#define GESTURE_UP                          0x11
#define GESTURE_C                           0x12
#define GESTURE_O                           0x13
#define GESTURE_M                           0x14
#define GESTURE_W                           0x15
#define GESTURE_E                           0x16
#define GESTURE_S                           0x17
#define GESTURE_Z                           0x1d
#define GESTURE_V                           0x1e
#define GESTURE_D_TAP                       0x50
#define GESTURE_DOWN                        0x22
#define GESTURE_LEFT                        0x23
#define GESTURE_RIGHT                       0x24

    #define CFG_CTS_NUM_GESTURE             (1u)
    #define CFG_CTS_GESTURE_REPORT_KEY
/* HS70 HS50 add for P210915-03922 all screen issue by zhangkexin at 2021/9/17 start */
    #define CFG_CTS_GESTURE_KEYMAP      \
        {{GESTURE_D_TAP, KEY_WAKEUP,},   \
        }
/* HS70 HS50 add for P210915-03922 all screen issue by zhangkexin at 2021/9/17 end */
    #define CFG_CTS_GESTURE_REPORT_TRACE    0
#endif /* CFG_CTS_GESTURE */

#ifdef CFG_CTS_GESTURE
#define LCM_LAB_MIN_UV                      5500000
#define LCM_LAB_MAX_UV                      6000000
#define LCM_IBB_MIN_UV                      5500000
#define LCM_IBB_MAX_UV                      6000000
#endif

//#define CONFIG_CTS_GLOVE

#define CONFIG_CTS_CHARGER_DETECT

#define CONFIG_CTS_EARJACK_DETECT

//tp selftest for HUAQIN only
//use: cat /sys/device/platform/tp_wake_switch/factory_check
#define CONFIG_CTS_TEST_FOR_HQ

/* ESD protection */
//#define CONFIG_CTS_ESD_PROTECTION
#ifdef CONFIG_CTS_ESD_PROTECTION
    #define CFG_CTS_ESD_PROTECTION_CHECK_PERIOD         (2 * HZ)
    #define CFG_CTS_ESD_FAILED_CONFIRM_CNT              3
#endif /* CONFIG_CTS_ESD_PROTECTION */

/* Use slot protocol (protocol B), comment it if use protocol A. */
#define CONFIG_CTS_SLOTPROTOCOL

#ifdef CONFIG_CTS_LEGACY_TOOL
    #define CFG_CTS_TOOL_PROC_FILENAME      "icn85xx_tool"
#endif /* CONFIG_CTS_LEGACY_TOOL */

#define CFG_CTS_UPDATE_CRCCHECK
/****************************************************************************
 * Platform configurations
 ****************************************************************************/

#include "cts_plat_qcom_config.h"

#endif /* CTS_CONFIG_H */

