/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __GXY_PSY_SYSFS_H__
#define __GXY_PSY_SYSFS_H__

#include <linux/power_supply.h>
#include <linux/sysfs.h>

#if defined(CONFIG_CUSTOM_FACTORY_BUILD)
#define CONFIG_GXY_FACTORY_BUILD
#endif //CONFIG_CUSTOM_FACTORY_BUILD

/* log debug */
#define GXY_PSY_TAG    "[GXY_PSY]"
/*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 start*/
#define GXY_PSY_DEBUG_LOG 1
#ifdef GXY_PSY_DEBUG_LOG
    #define GXY_PSY_INFO(fmt, args...)    pr_info(GXY_PSY_TAG fmt, ##args)
    #define GXY_PSY_DEBUG(fmt, args...)   pr_debug(GXY_PSY_TAG fmt, ##args)
#else
    #define GXY_PSY_INFO(fmt, args...)
#endif
/*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 end*/
#define GXY_PSY_ERR(fmt, args...)    pr_err(GXY_PSY_TAG fmt, ##args)

/* battery psy attrs */
ssize_t gxy_bat_show_attrs(struct device *dev,
                            struct device_attribute *attr, char *buf);

ssize_t gxy_bat_store_attrs(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count);

/* usb psy attrs */
ssize_t gxy_usb_show_attrs(struct device *dev,
                            struct device_attribute *attr, char *buf);

ssize_t gxy_usb_store_attrs(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count);

/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
/* ac psy attrs */
ssize_t gxy_ac_show_attrs(struct device *dev,
                            struct device_attribute *attr, char *buf);

ssize_t gxy_ac_store_attrs(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count);
/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/

#define GXY_BATTERY_ATTR(_name)                     \
{                                                   \
    .attr = {.name = #_name, .mode = 0664},         \
    .show = gxy_bat_show_attrs,                     \
    .store = gxy_bat_store_attrs,                   \
}

#define GXY_USB_ATTR(_name)                         \
{                                                   \
    .attr = {.name = #_name, .mode =  0664},        \
    .show = gxy_usb_show_attrs,                     \
    .store = gxy_usb_store_attrs,                   \
}

/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
#define GXY_AC_ATTR(_name)                         \
{                                                   \
    .attr = {.name = #_name, .mode =  0664},        \
    .show = gxy_ac_show_attrs,                     \
    .store = gxy_ac_store_attrs,                   \
}
/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/
/*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 start*/
/* usb psy attrs */
ssize_t gxy_otg_show_attrs(struct device *dev,
                            struct device_attribute *attr, char *buf);

ssize_t gxy_otg_store_attrs(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t count);

#define GXY_OTG_ATTR(_name)                         \
{                                                   \
    .attr = {.name = #_name, .mode =  0664},        \
    .show = gxy_otg_show_attrs,                     \
    .store = gxy_otg_store_attrs,                   \
}

enum {
    OTG_ONLINE = 0,
    GXY_OTG_PROP_MAX,
};

typedef enum gxy_otg_state {
    OTG_STATE_OFFLINE = 0,
    OTG_STATE_ONLINE,
} gxy_otg_state_t;

/*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 end*/

/*M55 code for SR-QN6887A-01-548|SR-QN6887A-01-539 by xiongxiaoliang at 20230919 start*/
/* battery psy property */
enum {
    BATT_CAP_CONTROL,
    INPUT_SUSPEND,
    BATT_FULL_CAPACITY,
    STORE_MODE,
    BATT_SLATE_MODE,
    /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 start*/
    HV_DISABLE,
    /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 end*/
    /*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 start*/
    HV_CHARGER_STATUS,
    /*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 end*/
    /*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 start*/
    BATTERY_TYPE,
    /*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 end*/
    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 start */
    AFC_RESULT,
    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 end */
    /*M55 code for SR-QN6887A-01-512 by xiongxiaoliang at 20230908 start*/
    CP_DUMP_REG,
    /*M55 code for SR-QN6887A-01-512 by xiongxiaoliang at 20230908 end*/
    /*M55 code for SR-QN6887A-01-526 by liufurong at 20231023 start*/
    BATT_TEMP,
    BATT_TYPE,
    BATTERY_CYCLE,
    BATT_DISCHARGE_LEVEL,
    RESET_BATTERY_CYCLE,
    /*M55 code for SR-QN6887A-01-526 by liufurong at 20231023 end*/
    /*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 start*/
    BATT_CURRENT_EVENT,
    /*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 end*/
    /*M55 code for SR-QN6887A-01-533 by gaoxugang at 20230921 start*/
    BATT_MISC_EVENT,
    /*M55 code for SR-QN6887A-01-533 by gaoxugang at 20230921 end*/
    /*M55 code for P231122-06687 by lina at 20231206 start*/
    BATT_SOC_RECHG,
    /*M55 code for P231122-06687 by lina at 20231206 end*/
    /*M55 code for P231127-07871 by xiongxiaoliang at 20231211 start*/
    CHARGE_TYPE,
    /*M55 code for P231127-07871 by xiongxiaoliang at 20231211 end*/
    /*M55 code for QN6887A-4205 by xiongxiaoliang at 20240102 start*/
    DBG_BATTERY_CYCLE,
    /*M55 code for QN6887A-4205 by xiongxiaoliang at 20240102 end*/
    /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
    TIME_TO_FULL_NOW,
    /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/
    /*M55 code for P240109-02015 by shixuanxuan at 20240123 start*/
    POWER_RESET,
    /*M55 code for P240109-02015 by shixuanxuan at 20240123 end*/
    /*M55 code for P240202-03197 by xiongxiaoliang at 20240204 start*/
    BATT_CURRENT_UA_NOW,
    /*M55 code for P240202-03197 by xiongxiaoliang at 20240204 end*/
    GXY_BATT_PROP_MAX,
};
/*M55 code for SR-QN6887A-01-548|SR-QN6887A-01-539 by xiongxiaoliang at 20230919 end*/

/*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 start*/
/*M55 code for P231125-00024 by xiongxiaoliang at 20231211 start*/
enum gxy_charger_status {
    GXY_NORMAL_CHARGING = 0,
    GXY_FAST_CHARGING_9VOR15W = 1,
    GXY_FAST_CHARGING_12VOR20W = 2,
    GXY_SUPER_FAST_CHARGING_25W = 3,
    GXY_SUPER_FAST_CHARGING_2_0_45W = 4,
};

enum gxy_pd_status {
    GXY_PD_15W_CHARGING = 1,
    GXY_PD_20W_CHARGING = 2,
    GXY_PD_25W_CHARGING = 3,
    GXY_PD_45W_CHARGING = 4,
};
/*M55 code for P231125-00024 by xiongxiaoliang at 20231211 end*/
/*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 end*/
/*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 start*/
/*M55 code for QN6887A-5069 by shixuanxuan at 20240124 start*/
#define BAT_LOW_TEMP_SWELLING 50
/*M55 code for QN6887A-5069 by shixuanxuan at 20240124 end*/
#define BAT_HIGH_TEMP_SWELLING 450
enum batt_current_event {
	SEC_BAT_CURRENT_EVENT_NONE = 0x00000,               /* Default value, nothing will be happen */
	SEC_BAT_CURRENT_EVENT_AFC = 0x00001,                /* Do not use this*/
	SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE = 0x00002,     /* This event is for a head mount VR, It is not need */
	SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING = 0x00010,  /* battery temperature is lower than 18 celsius degree */
	SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING = 0x00020, /* battery temperature is higher than 41 celsius degree */
	SEC_BAT_CURRENT_EVENT_USB_100MA = 0x00040,          /* USB 2.0 device under test is set in unconfigured state, not enumerate */
	SEC_BAT_CURRENT_EVENT_SLATE = 0x00800,
};
/*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 end*/
/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
enum {
    AC_ONLINE = 0,
    GXY_AC_PROP_MAX,
};

enum {
    TYPEC_CC_ORIENT = 0,
    USB_ONLINE,
    /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 start*/
    ANALOG_EARPHONE_ONLINE,
    /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 end*/
    /*M55 code for QN6887A-66 by shixuanxuan at 20230917 start*/
    CHG_PUMP_STATE,
    /*M55 code for QN6887A-66 by shixuanxuan at 20230917 end*/
    /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 start*/
    PD_MAX_POWER,
    /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 end*/
    GXY_USB_PROP_MAX,
};
/*M55 code for QN6887A-66 by shixuanxuan at 20230917 start*/
enum chg_pump_test_result {
    GXY_CHG_PUMP_NONE = 0,
    GXY_CHG_PUMP_PPS_ONLY,
    GXY_CHG_PUMP_RUN,
};
/*M55 code for QN6887A-66 by shixuanxuan at 20230917 end*/
typedef enum gxy_online_state {
    OFFLINE = 0,
    ONLINE_AC,
    ONLINE_USB,
} gxy_online_state_t;
/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/

/* usb psy - orient property */
enum gxy_usb_orient {
    GXY_USB_ORIENT_UNKOWN = 0,
    GXY_USB_ORIENT_CC1,
    GXY_USB_ORIENT_CC2,
};

typedef enum {
  GXY_TYPEC_CC_EVENT,
  /*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 start*/
  GXY_OTG_EVENT,
  /*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 end*/
  /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 start */
  GXY_AFC_ATTACH_EVENT,
  /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 end */
  /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 start*/
  GXY_ANALOG_EARPHONE_ATTACH_EVENT,
  /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 end*/
  /*M55 code for QN6887A-66 by shixuanxuan at 20230917 start*/
  GXY_CP_RUN_EVENT,
  /*M55 code for QN6887A-66 by shixuanxuan at 20230917 end*/
  /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 start*/
  GXY_PD_MAX_POWER_EVENT,
  /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 end*/
  GXY_MAX_EVENT_TYPE
}gxy_notify_event_type;

struct gxy_battery {
    struct power_supply *psy;
    struct power_supply *usb_psy;
    /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
    struct power_supply *ac_psy;
    /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/
    /*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230817 start*/
    struct power_supply *otg_psy;
    /*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 end*/
};

struct gxy_usb {
    u8 cc_orient;
    /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 start*/
    u8 pd_maxpower_status;
    /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 end*/
};

/*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 start*/
enum gxy_battery_type {
    GXY_BATTERY_TYPE_ATL = 0,
    /*M55 code for QN6887A-383 by xiongxiaoliang at 20231027 start*/
    GXY_BATTERY_TYPE_SCUD = 1,
    /*M55 code for QN6887A-383 by xiongxiaoliang at 20231027 end*/
    GXY_BATTERY_TYPE_UNKNOWN,
};
/*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 end*/
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
extern struct gxy_usb g_gxy_usb;
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/
extern int gxy_usb_get_prop(u32 prop_id);
extern int gxy_usb_set_prop(u32 prop_id);
extern int gxy_battery_set_prop(u32 prop_id, u32 value);
extern int gxy_battery_get_prop(u32 prop_id);
/*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 start*/
extern int gxy_otg_get_prop(u32 prop_id);
/*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 end*/

#endif /* __GXY_PSY_SYSFS_H__ */
