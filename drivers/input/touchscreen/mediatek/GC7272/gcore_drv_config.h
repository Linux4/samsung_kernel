/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2023 GalaxyCore Incorporated
 *
 * Copyright (C) 2023 Newgate tian <newgate_tian@gcoreinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


/*----------------------------------------------------------*/
/* GALAXYCORE TOUCH DEVICE DRIVER RELEASE VERSION           */
/* Driver version: x.x.x (platform.ic_type.version)         */
/* platform  MTK:1  SPRD:2   QCOM:3                         */
/* ic type  7371:1  7271:2  7202:3  7302:4  7372:5  7202H:6 */
/*----------------------------------------------------------*/
#define TOUCH_DRIVER_RELEASE_VERISON   ("1.6.1")

/*----------------------------------------------------------*/
/* COMPILE OPTION DEFINITION                                */
/*----------------------------------------------------------*/

/*
 * Note.
 * The below compile option is used to enable the specific platform this driver running on.
 */
 #define CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
/* #define CONFIG_TOUCH_DRIVER_RUN_ON_RK_PLATFORM */
/* #define CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM */
/* #define CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM */

/*
 * Note.
 * The below compile option is used to enable the chip type used
 */
/* #define CONFIG_ENABLE_CHIP_TYPE_GC7371 */
/* #define CONFIG_ENABLE_CHIP_TYPE_GC7271 */
/* #define CONFIG_ENABLE_CHIP_TYPE_GC7202 */
/* #define CONFIG_ENABLE_CHIP_TYPE_GC7372 */
/* #define CONFIG_ENABLE_CHIP_TYPE_GC7302 */
#define CONFIG_ENABLE_CHIP_TYPE_GC7202H_GC7272

/*
 * Note.
 * The below compile option is used to enable the IC interface used
 */
/* #define CONFIG_TOUCH_DRIVER_INTERFACE_I2C */
#define CONFIG_TOUCH_DRIVER_INTERFACE_SPI

/*
 * Note.
 * The below compile option is used to enable the fw download type
 * Hostdownload for 0 flash,flashdownload for 1 flash
 */
#define CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
/* #define CONFIG_GCORE_AUTO_UPDATE_FW_FLASHDOWNLOAD */

/*
 * Note.
 * The below compile option is used to enable function transfer fw raw data to App or Tool
 * By default,this compile option is disable
 * Connect to App or Tool function need another interface module, if needed, contact to our colleague
 */
#define CONFIG_ENABLE_FW_RAWDATA

/*
 * Note.
 * The below compile option is used to enable enable/disable multi-touch procotol for reporting touch point to input sub-system
 * If this compile option is defined, Type B procotol is enabled
 * Else, Type A procotol is enabled
 * By default, this compile option is enable
 */
#define CONFIG_ENABLE_TYPE_B_PROCOTOL

/*
 * Note.
 * The below compile option is used to enable/disable log output for touch driver
 * If is set as 1, the log output is enabled
 * By default, the debug log is set as 0
 */
#define CONFIG_ENABLE_REPORT_LOG  (1)

/*
 * Note.
 * The below compile option is used to enable GESTURE WAKEUP function
 */
#define CONFIG_ENABLE_GESTURE_WAKEUP

/*
 * Note.
 * The following compile option is used to enable the proximity tp turn off display function when on the phone
 */
//#define CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
//#define CONFIG_MTK_PROXIMITY_TP_SCREEN_ON_ALSPS


/*
 * Note.
 * The below compile option is used to enable/disable ITO test
 * If is set as 1, mp test is enabled
 * By default, the mp test is set as 0
 */
#define GCORE_MP_TEST_ON 1

/*
 *Note.
 * The below compile option is used to enable/disable ESD check
 * If GCORE_WDT_RECOVERY_ENABLE set 1, firmware must open ESD check.
 * By default, The ESD check is enabled.
 */
#define GCORE_WDT_RECOVERY_ENABLE  1
#define GCORE_WDT_TIMEOUT_PERIOD   2500
#define MAX_FW_RETRY_NUM          2


/*
 * LCD Resolution
 * The below options are screen resolution Settings
 */

/* #define TOUCH_SCREEN_X_MAX          (720)     //(1080) */
/* #define TOUCH_SCREEN_Y_MAX          (1560)    //(2340) */
#define TOUCH_SCREEN_X_MAX            (720)
#define TOUCH_SCREEN_Y_MAX            (1600)


/*
 * Note.
 * The below compile option is used to enable update firmware by bin file
 * Else, the firmware is from array in driver
 */
/* +S96818AA1-1936,daijun1.wt,modify,2023/08/23,Change firmware download path */
//#define CONFIG_UPDATE_FIRMWARE_BY_BIN_FILE
/* -S96818AA1-1936,daijun1.wt,modify,2023/08/23,Change firmware download path */
/*
 * Note.
 * This compile option is used for MTK platform only
 * This compile option is used for MTK legacy platform different
 */
/* #define CONFIG_MTK_LEGACY_PLATFORM */

#define CONFIG_GCORE_HOSTDOWNLOAD_ESD_PROTECT

#define CONFIG_MTK_SPI_MULTUPLE_1024
//+S96818AA1-1936,wangtao14.wt,modify,2023/07/06,gc7272 save CB size
#define CONFIG_SAVE_CB_CHECK_VALUE
//-S96818AA1-1936,wangtao14.wt,modify,2023/07/06,gc7272 save CB size

//+S96818AA1-1936,wangtao14.wt,add,2023/07/10,gc7272 power system notify
#define CONFIG_GCORE_PSY_NOTIFY
//-S96818AA1-1936,wangtao14.wt,add,2023/07/10,gc7272 power system notify