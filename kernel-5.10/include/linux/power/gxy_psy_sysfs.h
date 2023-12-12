/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __GXY_PSY_SYSFS_H__
#define __GXY_PSY_SYSFS_H__

#include <linux/power_supply.h>
#include <linux/sysfs.h>


/* log debug */
#define GXY_PSY_TAG    "[GXY_PSY]"

#define GXY_PSY_DEBUG 1
#ifdef GXY_PSY_DEBUG
    #define GXY_PSY_INFO(fmt, args...)    pr_info(GXY_PSY_TAG fmt, ##args)
#else
    #define GXY_PSY_INFO(fmt, args...)
#endif

#define GXY_PSY_ERR(fmt, args...)    pr_err(GXY_PSY_TAG fmt, ##args)

/*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    /* DATA */
    #define DATA_MAXSIZE 32
#endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 end*/

/* battery psy attrs */
ssize_t gxy_bat_show_attrs(struct device *dev,
                            struct device_attribute *attr, char *buf);

ssize_t gxy_bat_store_attrs(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count);

/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
/* usb psy attrs */
ssize_t gxy_usb_show_attrs(struct device *dev,
                            struct device_attribute *attr, char *buf);

ssize_t gxy_usb_store_attrs(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count);
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/

#define GXY_BATTERY_ATTR(_name)                              \
{                                                      \
    .attr = {.name = #_name, .mode = 0664},        \
    .show = gxy_bat_show_attrs,                      \
    .store = gxy_bat_store_attrs,                      \
}

/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
#define GXY_USB_ATTR(_name)                              \
{                                                      \
    .attr = {.name = #_name, .mode =  0664},        \
    .show = gxy_usb_show_attrs,                      \
    .store = gxy_usb_store_attrs,                      \
}
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/

/* battery psy property */
enum {
    CHG_INFO = 0,
    /*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 start*/
    TCPC_INFO,
    /*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 end*/
    /*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 start*/
    BATTERY_TYPE,
    /*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 end*/
    /*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 start*/
    #if defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    BATT_CAP_CONTROL,
    #endif //CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 end*/
    /*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    BATT_FULL_CAPACITY,
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 end*/
    /*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    STORE_MODE,
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 end*/
    /*Tab A9 code for SR-AX6739A-01-456 | P230713-00914 by qiaodan at 20230713 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    BATT_SLATE_MODE,
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-456 | P230713-00914 by qiaodan at 20230713 end*/
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    INPUT_SUSPEND,
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
    /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 start*/
    AFC_RESULT,
    HV_DISABLE,
    /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 end*/
    /*Tab A9 code for SR-AX6739A-01-512 by wenyaqi at 20230523 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    HV_CHARGER_STATUS,
    #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-512 by wenyaqi at 20230523 end*/
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
    SHIPMODE,
    SHIPMODE_REG,
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/
    /*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 start*/
    DUMP_CHARGER_IC,
    /*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 end*/
    /*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 start*/
    #ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
    BATT_MISC_EVENT,
    #endif//!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 end*/
    /*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 start*/
    #ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
    BATT_CURRENT_EVENT,
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 end*/
    /*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    BATT_TEMP,
    BATT_DISCHARGE_LEVEL,
    BATT_TYPE,
    BATT_CURRENT_UA_NOW,
    BATTERY_CYCLE,
    RESET_BATTERY_CYCLE,
    #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 end*/
    /*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    CHARGE_TYPE,
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 end*/
};

/*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 start*/
/* AFC detected */
enum {
    AFC_INIT,
    NOT_AFC,
    AFC_FAIL,
    AFC_DISABLE,
    NON_AFC_MAX,
    AFC_5V = NON_AFC_MAX,
    AFC_9V,
    AFC_12V,
};
/*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 end*/

/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
enum {
    TYPEC_CC_ORIENT = 0,
};
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/

/* battey psy property - chg_info data */
enum gxy_bat_chg_info {
    GXY_BAT_CHG_INFO_UNKNOWN = 0,
    GXY_BAT_CHG_INFO_SC89601D,
    GXY_BAT_CHG_INFO_UPM6910DH,
    GXY_BAT_CHG_INFO_UPM6910DS,
    /* Tab A9 code for AX6739A-1334 by gaozhengwei at 20230620 start */
    GXY_BAT_CHG_INFO_SGM41515D,
    /* Tab A9 code for AX6739A-1334 by gaozhengwei at 20230620 end */
};

/*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 start*/
/* battey psy property - chg_info data */
enum gxy_bat_tcpc_info {
    GXY_BAT_TCPC_INFO_UNKNOWN = 0,
    GXY_BAT_TCPC_INFO_ET7304MQ,
    GXY_BAT_TCPC_INFO_AW35615,
    GXY_BAT_TCPC_INFO_CPS8851MRE,
};
/*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 end*/
/*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 start*/
enum gxy_battery_type {
    GXY_BATTERY_TYPE_ATL = 0,
    GXY_BATTERY_TYPE_BYD,
    GXY_BATTERY_TYPE_UNKNOWN,
};
/*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 end*/
/* data struct type */
struct gxy_bat_hwinfo {
    enum gxy_bat_chg_info cinfo;
    /*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 start*/
    enum gxy_bat_tcpc_info tinfo;
    /*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 end*/
    /*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 start*/
    enum gxy_battery_type binfo;
    /*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 end*/
    /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 start*/
    int afc_result;
    /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 end*/
};

/*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
struct gxy_battery_data {
    char cust_batt_type[DATA_MAXSIZE];
};
#endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 end*/


/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
/* usb psy - orient property */
enum gxy_usb_orient {
    GXY_USB_ORIENT_UNKOWN = 0,
    GXY_USB_ORIENT_CC1,
    GXY_USB_ORIENT_CC2,
};

struct gxy_usb_hwinfo {
    int usb_cc_flag;
    /*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 start*/
    bool pdhub_flag;
    /*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 end*/
};
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/

struct gxy_battery {
    struct power_supply *psy;
    /*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
    struct power_supply *usb_psy;
    /*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/
};

/*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 start*/
extern void gxy_usb_set_pdhub_flag(bool is_pdhub);
extern bool gxy_usb_get_pdhub_flag(void);
/*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 end*/

#endif /* __GXY_PSY_SYSFS_H__ */
