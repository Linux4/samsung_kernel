// SPDX-License-Identifier: GPL-2.0

#include <linux/err.h>    /* IS_ERR, PTR_ERR */
#include <linux/init.h>        /* For init/exit macros */
#include <linux/kernel.h>
#include <linux/module.h>    /* For MODULE_ marcros  */
#include <linux/of_fdt.h>    /*of_dt API*/
#include <linux/of.h>
#include <linux/platform_device.h>    /* platform device */
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/power/gxy_psy_sysfs.h>
/*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 start*/
#include "gpio_afc.h"
/*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 end*/
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
#include "gxy_battery_ttf.h"
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/

/*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 start*/
static const char * const GXY_BATTERY_TYPE_TEXT[] = {
    [GXY_BATTERY_TYPE_ATL]    = "1:battery-ATL",
    /*M55 code for QN6887A-383 by xiongxiaoliang at 20231027 start*/
    [GXY_BATTERY_TYPE_SCUD]    = "2:battery-SCUD",
    /*M55 code for QN6887A-383 by xiongxiaoliang at 20231027 end*/
    [GXY_BATTERY_TYPE_UNKNOWN] = "UNKNOWN",
};
/*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 end*/
/*M55 code for SR-QN6887A-01-526 by liufurong at 20230913 start*/
#define BATTERY_TYPE_SIZE    32
static char s_gxy_battery_type[BATTERY_TYPE_SIZE] = "UNKNOWN";
/*M55 code for SR-QN6887A-01-526 by liufurong at 20230913 end*/

/* include attrs for battery psy */
static struct device_attribute gxy_battery_attrs[] = {
    GXY_BATTERY_ATTR(batt_cap_control),
    /*M55 code for SR-QN6887A-01-539 by xiongxiaoliang at 20230919 start*/
    GXY_BATTERY_ATTR(input_suspend),
    /*M55 code for SR-QN6887A-01-539 by xiongxiaoliang at 20230919 end*/
    GXY_BATTERY_ATTR(batt_full_capacity),
    GXY_BATTERY_ATTR(store_mode),
    GXY_BATTERY_ATTR(batt_slate_mode),
    /*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 start*/
    /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 start*/
    GXY_BATTERY_ATTR(hv_disable),
    /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 end*/
    GXY_BATTERY_ATTR(hv_charger_status),
    /*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 end*/
    /*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 start*/
    GXY_BATTERY_ATTR(battery_type),
    /*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 end*/
    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 start */
    GXY_BATTERY_ATTR(afc_result),
    /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 end */
    /*M55 code for SR-QN6887A-01-512 by xiongxiaoliang at 20230908 start*/
    GXY_BATTERY_ATTR(cp_dump_reg),
    /*M55 code for SR-QN6887A-01-512 by xiongxiaoliang at 20230908 end*/
    /*M55 code for SR-QN6887A-01-526 by liufurong at 20231023 start*/
    GXY_BATTERY_ATTR(batt_temp),
    GXY_BATTERY_ATTR(batt_type),
    GXY_BATTERY_ATTR(battery_cycle),
    GXY_BATTERY_ATTR(batt_discharge_level),
    GXY_BATTERY_ATTR(reset_battery_cycle),
    /*M55 code for SR-QN6887A-01-526 by liufurong at 20231023 end*/
    /*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 start*/
    GXY_BATTERY_ATTR(batt_current_event),
    /*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 end*/
    /*M55 code for SR-QN6887A-01-533 by gaoxugang at 20230921 start*/
    GXY_BATTERY_ATTR(batt_misc_event),
    /*M55 code for SR-QN6887A-01-533 by gaoxugang at 20230921 end*/
    /*M55 code for P231122-06687 by lina at 20231206 start*/
    GXY_BATTERY_ATTR(batt_soc_rechg),
    /*M55 code for P231122-06687 by lina at 20231206 end*/
    /*M55 code for P231127-07871 by xiongxiaoliang at 20231211 start*/
    GXY_BATTERY_ATTR(charge_type),
    /*M55 code for P231127-07871 by xiongxiaoliang at 20231211 end*/
    /*M55 code for QN6887A-4205 by xiongxiaoliang at 20240102 start*/
    GXY_BATTERY_ATTR(dbg_battery_cycle),
    /*M55 code for QN6887A-4205 by xiongxiaoliang at 20240102 end*/
    /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
    GXY_BATTERY_ATTR(time_to_full_now),
    /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/
    /*M55 code for P240109-02015 by shixuanxuan at 20240123 start*/
    GXY_BATTERY_ATTR(power_reset),
    /*M55 code for P240109-02015 by shixuanxuan at 20240123 end*/
    /*M55 code for P240202-03197 by xiongxiaoliang at 20240204 start*/
    GXY_BATTERY_ATTR(batt_current_ua_now),
    /*M55 code for P240202-03197 by xiongxiaoliang at 20240204 end*/
};

static struct device_attribute gxy_usb_attrs[] = {
    GXY_USB_ATTR(typec_cc_orientation),
    /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
    GXY_USB_ATTR(online),
    /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/
    /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 start*/
    GXY_USB_ATTR(analog_earphone),
    /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 end*/
    /*M55 code for QN6887A-66 by shixuanxuan at 20230917 start*/
    GXY_USB_ATTR(chg_pump),
    /*M55 code for QN6887A-66 by shixuanxuan at 20230917 end*/
    /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 start*/
    GXY_USB_ATTR(pd_max_power),
    /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 end*/
};

/*M55 code for P240202-03197 by xiongxiaoliang at 20240204 start*/
static int gxy_get_batt_current_ua_now(void)
{
    static struct power_supply *batt_psy = NULL;
    union power_supply_propval val;
    int ret = 0;

    batt_psy = power_supply_get_by_name("battery");
    if (IS_ERR_OR_NULL(batt_psy)) {
        GXY_PSY_ERR("%s:get batt psy failed\n", __func__);
        return -EINVAL;
    }

    ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    if (ret < 0) {
        GXY_PSY_ERR("%s:get battery current property failed\n", __func__);
        return -ENODATA;
    }

    GXY_PSY_ERR("%s:batt_ua_now is %d\n", __func__, val.intval);
    return val.intval;
}
/*M55 code for P240202-03197 by xiongxiaoliang at 20240204 end*/

/*M55 code for SR-QN6887A-01-526 by liufurong at 20230913 start*/
static int gxy_get_battery_temp(void)
{
    static struct power_supply *batt_psy = NULL;
    union power_supply_propval val;

    if (batt_psy == NULL) {
        batt_psy = power_supply_get_by_name("battery");
    }

    if (IS_ERR_OR_NULL(batt_psy)) {
        GXY_PSY_ERR("%s: get batt psy failed\n", __func__);
        return -EINVAL;
    }
    power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
    GXY_PSY_ERR("%s: battery temp is %d\n", __func__, val.intval);

    return val.intval;
}

static int gxy_get_battery_cycle(void)
{
    static struct power_supply *batt_psy = NULL;
    union power_supply_propval val;

    if (batt_psy == NULL) {
        batt_psy = power_supply_get_by_name("battery");
    }

    if (IS_ERR_OR_NULL(batt_psy)) {
        GXY_PSY_ERR("%s: get batt psy failed\n", __func__);
        return -EINVAL;
    }
    power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
    GXY_PSY_ERR("%s: battery cycle is %d\n", __func__, val.intval);

    return val.intval;
}
/*M55 code for SR-QN6887A-01-526 by liufurong at 20230913 end*/

/*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 start*/
extern bool get_analog_online(void);
/*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 end*/
/*M55 code for QN6887A-66 by shixuanxuan at 20230917 start*/
extern bool get_chg_pump_run_state(void);
/*M55 code for QN6887A-66 by shixuanxuan at 20230917 end*/

/*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 start*/
static struct device_attribute gxy_otg_attrs[] = {
    GXY_OTG_ATTR(online),
};

ssize_t gxy_otg_show_attrs(struct device *dev, struct device_attribute *attr, char * buf)
{
    const ptrdiff_t offset = attr - gxy_otg_attrs;
    int count = 0;
    int value = 0;

    GXY_PSY_ERR("%s: gxy_otg_show_attrs enter\n", __func__);

    switch (offset) {
        case OTG_ONLINE:
            value = gxy_otg_get_prop(offset);
            count = sprintf(buf, "%d\n", value);
            break;
        default:
            count = -EINVAL;
            break;
    }
    return count;
}

ssize_t gxy_otg_store_attrs(struct device *dev, struct device_attribute *attr,
    const char *buf, size_t count)
{
    const ptrdiff_t offset = attr - gxy_otg_attrs;
    int ret = -EINVAL;
    GXY_PSY_ERR("%s: gxy_otg_store_attrs enter\n", __func__);

    switch (offset) {
        default:
            ret = -EINVAL;
            break;
    }

    return ret;
}

static int gxy_otg_create_attrs(struct device *dev)
{
    unsigned long i = 0;
    int ret = 0;

    for (i = 0; i < ARRAY_SIZE(gxy_otg_attrs); i++) {
        ret = device_create_file(dev, &gxy_otg_attrs[i]);
        if (ret) {
            goto create_attrs_failed;
        }
    }
    goto create_attrs_succeed;

create_attrs_failed:
    GXY_PSY_ERR("%s: failed (%d)\n", __func__, ret);
    while (i--) {
        device_remove_file(dev, &gxy_otg_attrs[i]);
    }

create_attrs_succeed:
    return ret;
}
/*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 end*/

/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
static struct device_attribute gxy_ac_attrs[] = {
    GXY_AC_ATTR(online),
};

static gxy_online_state_t gxy_get_online_state(void)
{
    gxy_online_state_t ret = OFFLINE;
    struct power_supply *usb_psy = NULL;
    union power_supply_propval val;
    union power_supply_propval val_vol;
    usb_psy = power_supply_get_by_name("usb");
    if (IS_ERR_OR_NULL(usb_psy)) {
        GXY_PSY_ERR("%s: get usb psy failed\n", __func__);
        return -EPROBE_DEFER;
    }
    power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
    power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val_vol);
    /*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 start*/
    GXY_PSY_DEBUG("%s: get usb type val = %d, voltage now = %d\n", __func__, val.intval, val_vol.intval);
    /*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 end*/
    switch (val.intval) {
        case POWER_SUPPLY_USB_TYPE_DCP:
        case POWER_SUPPLY_USB_TYPE_PD_DRP:
        case POWER_SUPPLY_USB_TYPE_PD_PPS:
        case POWER_SUPPLY_USB_TYPE_C:
        case POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID:
        case POWER_SUPPLY_USB_TYPE_ACA:
            ret = ONLINE_AC;
            break;
        case POWER_SUPPLY_USB_TYPE_SDP:
        case POWER_SUPPLY_USB_TYPE_CDP:
            ret = ONLINE_USB;
            break;
        case POWER_SUPPLY_USB_TYPE_PD:
            /*M55 code for SR-QN6887A-01-707 by wenyaqi at 20230921 start*/
            if (val_vol.intval <= 6500000) {
                ret = ONLINE_USB;
            } else {
                ret = ONLINE_AC;
            }
            /*M55 code for SR-QN6887A-01-707 by wenyaqi at 20230921 end*/
            break;
        case POWER_SUPPLY_USB_TYPE_UNKNOWN:
            ret = OFFLINE;
            break;
        default:
            break;
    }
    /*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 start*/
    GXY_PSY_DEBUG("%s: final ret = %d\n", __func__, ret);
    /*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 end*/
    return ret;
}
/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/

/*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 start*/
/*M55 code for P231125-00024 by xiongxiaoliang at 20231211 start*/
/*M55 code for QN6887A-5060 by xiongxiaoliang at 20240122 start*/
#define HV_CHARGE_VOLTAGE_MV 6500000
/*M55 code for QN6887A-5060 by xiongxiaoliang at 20240122 end*/
static int gxy_get_charger_status(void)
{
    struct power_supply *usb_psy = NULL;
    union power_supply_propval val;
    union power_supply_propval val_vol;
    int hv_charger = GXY_NORMAL_CHARGING;
    u8 pd_charger_status = 0;

    /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 start*/
    /* M55 code for SR-QN6887A-01-515 by liufurong at 20231110 start */
    if (gxy_battery_get_prop(HV_DISABLE)) {
        return hv_charger;
    }
    /* M55 code for SR-QN6887A-01-515 by liufurong at 20231110 end */
    /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 end*/
    usb_psy = power_supply_get_by_name("usb");
    if (IS_ERR_OR_NULL(usb_psy)) {
        GXY_PSY_ERR("%s: get usb psy failed\n", __func__);
        return -EPROBE_DEFER;
    }
    power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
    power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val_vol);
    pd_charger_status = gxy_usb_get_prop(PD_MAX_POWER);
    GXY_PSY_ERR("%s: get usb type val = %d, voltage now = %d, pd_charger_statue = %d\n", __func__, val.intval, val_vol.intval, pd_charger_status);
    switch (val.intval) {
        /*M55 code for QN6887A-5060 by xiongxiaoliang at 20240122 start*/
        case POWER_SUPPLY_USB_TYPE_PD:
        case POWER_SUPPLY_USB_TYPE_PD_DRP:
        case POWER_SUPPLY_USB_TYPE_PD_PPS:
            switch (pd_charger_status) {
                case GXY_PD_15W_CHARGING:
                    hv_charger = GXY_FAST_CHARGING_9VOR15W;
                    break;
                case GXY_PD_20W_CHARGING:
                    hv_charger = GXY_FAST_CHARGING_12VOR20W;
                    break;
                case GXY_PD_25W_CHARGING:
                    hv_charger = GXY_SUPER_FAST_CHARGING_25W;
                    break;
                case GXY_PD_45W_CHARGING:
                    hv_charger = GXY_SUPER_FAST_CHARGING_2_0_45W;
                    break;
                default:
                    break;
            }
            break;
        case POWER_SUPPLY_USB_TYPE_C:
        case POWER_SUPPLY_USB_TYPE_DCP:
            if (val_vol.intval >= HV_CHARGE_VOLTAGE_MV) {
                hv_charger = GXY_FAST_CHARGING_9VOR15W;
            }
            break;
        /*M55 code for QN6887A-5060 by xiongxiaoliang at 20240122 end*/
        default:
            break;
    }

    return hv_charger;
}
/*M55 code for P231125-00024 by xiongxiaoliang at 20231211 end*/
/*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 end*/
/*M55 code for QN6887A-66 by shixuanxuan at 20230826 start*/
static int gxy_charg_pump_test_result(void)
{
    struct power_supply *usb_psy = NULL;
    union power_supply_propval val;
    bool chg_pump_run = 0;
    int ret = 0;

    usb_psy = power_supply_get_by_name("usb");
    if (IS_ERR_OR_NULL(usb_psy)) {
        GXY_PSY_ERR("%s: get usb psy failed\n", __func__);
        return -EPROBE_DEFER;
    }
    power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
    chg_pump_run = get_chg_pump_run_state();

    if ((val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS) && !chg_pump_run) {
        ret = GXY_CHG_PUMP_PPS_ONLY;
    } else if ((val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS) && chg_pump_run) {
        ret = GXY_CHG_PUMP_RUN;
    }
    return ret;
}
/*M55 code for QN6887A-66 by shixuanxuan at 20230826 end*/
/*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 start*/
bool g_usb_connected_unconfigured = false;
EXPORT_SYMBOL(g_usb_connected_unconfigured);

ssize_t gxy_get_prop_batt_current_event(void)
{
    struct power_supply *battery_psy = NULL;
    struct power_supply *usb_psy = NULL;
    int battery_temperature = 0;
    int slate_mode = 0;
    int usb_onlie = 0;
    int ret = 0;
    union power_supply_propval val;
    int gxy_batt_current_event = SEC_BAT_CURRENT_EVENT_NONE;

    battery_psy = power_supply_get_by_name("battery");
    usb_psy = power_supply_get_by_name("usb");
    ret |= power_supply_get_property(battery_psy, POWER_SUPPLY_PROP_TEMP, &val);
    battery_temperature = val.intval;
    ret |= power_supply_get_property(usb_psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
    usb_onlie = val.intval;
    if (ret < 0) {
        GXY_PSY_ERR("%s: get usb and battery psy property fail\n", __func__);
    }
    slate_mode = gxy_battery_get_prop(BATT_SLATE_MODE);
    if (usb_onlie) {
        if (battery_temperature <= BAT_LOW_TEMP_SWELLING) {
            gxy_batt_current_event |= SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING;
        }
        if (battery_temperature >= BAT_HIGH_TEMP_SWELLING) {
            gxy_batt_current_event |= SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING;
        }
        if (g_usb_connected_unconfigured) {
            gxy_batt_current_event |= SEC_BAT_CURRENT_EVENT_USB_100MA;
        }
        if (slate_mode) {
            gxy_batt_current_event |= SEC_BAT_CURRENT_EVENT_SLATE;
        }
    }

    GXY_PSY_DEBUG("%s: get battery temp: %d, slate_mode: %d, usb_onlie: %d, event_current: %d\n",
            __func__, battery_temperature, slate_mode, usb_onlie, gxy_batt_current_event);

    return gxy_batt_current_event;
}
/*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 end*/
/*M55 code for P231127-07871 by xiongxiaoliang at 20231211 start*/
static const char * const GXY_POWER_SUPPLY_CHARGE_TYPE_TEXT[] = {
    [POWER_SUPPLY_CHARGE_TYPE_UNKNOWN]  = "Unknown",
    [POWER_SUPPLY_CHARGE_TYPE_NONE]     = "N/A",
    [POWER_SUPPLY_CHARGE_TYPE_TRICKLE]  = "Trickle",
    [POWER_SUPPLY_CHARGE_TYPE_FAST]     = "Fast",
    [POWER_SUPPLY_CHARGE_TYPE_TAPER]    = "Taper",
};

static int gxy_get_chr_type(void)
{
    static struct power_supply *psy = NULL;
    union power_supply_propval prop = {0};
    int ret = 0;
    int chr_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

    if (psy == NULL) {
        psy = power_supply_get_by_name("battery");
        if (psy == NULL) {
            GXY_PSY_ERR("%s: get battery psy failed\n", __func__);
            return chr_type;
        }
    }

    ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &prop);
    if (ret < 0) {
        GXY_PSY_ERR("%s:get battery status property fail\n", __func__);
        return chr_type;
    }

    switch (prop.intval) {
        case POWER_SUPPLY_STATUS_DISCHARGING:
            chr_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
            break;
        case POWER_SUPPLY_STATUS_CHARGING:
        case POWER_SUPPLY_STATUS_FULL:
            chr_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
            break;
        default:
            chr_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
            break;
    }

    return chr_type;
}
/*M55 code for P231127-07871 by xiongxiaoliang at 20231211 end*/

/* sysfs read for "battery" psd */
ssize_t gxy_bat_show_attrs(struct device *dev, struct device_attribute *attr, char *buf)
{
    const ptrdiff_t offset = attr - gxy_battery_attrs;
    int count = 0;
    int value = 0;

    switch (offset) {
        #if defined(CONFIG_GXY_FACTORY_BUILD)
        case BATT_CAP_CONTROL:
        /*M55 code for SR-QN6887A-01-539 by xiongxiaoliang at 20230919 start*/
        case INPUT_SUSPEND:
            value = gxy_battery_get_prop(offset);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /*M55 code for SR-QN6887A-01-539 by xiongxiaoliang at 20230919 end*/
        #endif //CONFIG_GXY_FACTORY_BUILD
        case BATT_FULL_CAPACITY:
        case STORE_MODE:
        case BATT_SLATE_MODE:
        /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 start*/
        case HV_DISABLE:
        /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 end*/
            /*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 start*/
            value = gxy_battery_get_prop(offset);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
            /*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 end*/
        /*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 start*/
        case HV_CHARGER_STATUS:
            value = gxy_get_charger_status();
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /*M55 code for SR-QN6887A-01-516 by shixuanxuan at 20230826 end*/
        /*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 start*/
        case BATTERY_TYPE:
            value = gxy_battery_get_prop(offset);
            if ((value < GXY_BATTERY_TYPE_ATL) || (value > GXY_BATTERY_TYPE_UNKNOWN)) {
                value = GXY_BATTERY_TYPE_UNKNOWN;
            }
            count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n", GXY_BATTERY_TYPE_TEXT[value]);
            break;
        /*M55 code for SR-QN6887A-01-525 by liufurong at 20230816 end*/
        /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 start */
        case AFC_RESULT:
            value = get_afc_result();
            if (value == AFC_9V) {
                value = 1;
            } else {
                value = 0;
            }
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /* M55 code for SR-QN6887A-01-596 by shixuanxuan at 20230902 end */
        /*M55 code for SR-QN6887A-01-512 by xiongxiaoliang at 20230908 start*/
        case CP_DUMP_REG:
            value = gxy_battery_get_prop(offset);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /*M55 code for SR-QN6887A-01-512 by xiongxiaoliang at 20230908 end*/
        /*M55 code for SR-QN6887A-01-526 by liufurong at 20231023 start*/
        case BATT_TEMP:
            value = gxy_get_battery_temp();
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        case BATT_TYPE:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n", s_gxy_battery_type);
            break;
        case BATTERY_CYCLE:
            value = gxy_get_battery_cycle();
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        case BATT_DISCHARGE_LEVEL:
            value = gxy_battery_get_prop(offset);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        case RESET_BATTERY_CYCLE:
            value = gxy_battery_get_prop(offset);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /*M55 code for SR-QN6887A-01-526 by liufurong at 20231023 end*/
        /*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 start*/
        case BATT_CURRENT_EVENT:
            value = gxy_get_prop_batt_current_event();
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /*M55 code for SR-QN6887A-01-536 by shixuanxuan at 20230919 end*/
        /*M55 code for SR-QN6887A-01-533 by gaoxugang at 20230921 start*/
        case BATT_MISC_EVENT:
            value = gxy_battery_get_prop(offset);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /*M55 code for SR-QN6887A-01-533 by gaoxugang at 20230921 end*/
        /*M55 code for P231122-06687 by lina at 20231206 start*/
        case BATT_SOC_RECHG:
            value = gxy_battery_get_prop(offset);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            pr_err("get batt_soc_rechg %d\n",value);
            break;
        /*M55 code for P231122-06687 by lina at 20231206 end*/
        /*M55 code for P231127-07871 by xiongxiaoliang at 20231211 start*/
        case CHARGE_TYPE:
            value = gxy_get_chr_type();
            count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n", GXY_POWER_SUPPLY_CHARGE_TYPE_TEXT[value]);
            break;
        /*M55 code for P231127-07871 by xiongxiaoliang at 20231211 end*/
        /*M55 code for QN6887A-4205 by xiongxiaoliang at 20240102 start*/
        case DBG_BATTERY_CYCLE:
            value = gxy_battery_get_prop(offset);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /*M55 code for QN6887A-4205 by xiongxiaoliang at 20240102 end*/
        /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
        case TIME_TO_FULL_NOW:
            value = gxy_ttf_display();
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/
        /*M55 code for P240202-03197 by xiongxiaoliang at 20240204 start*/
        case BATT_CURRENT_UA_NOW:
            value = gxy_get_batt_current_ua_now();
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n", value);
            break;
        /*M55 code for P240202-03197 by xiongxiaoliang at 20240204 end*/
        default:
            count = -EINVAL;
            break;
    }
    return count;
}
/*M55 code for P231122-06914|QN6887A-4129 by lina at 20231124 start*/
const char *g_basic_protection  = "100";
const char *g_maximum_protection  = "80 OPTION";
const char *g_sleep_protection  = "80 SLEEP";
const char *g_highsoc_protection  = "80 HIGHSOC";
#define MAX_PROTECTION_FLAG    1   //buckon (system on / chg off)
#define SLEEP_PROTECTION_FLAG    2  //buckon (system on / chg off)
#define HIGHSOC_PROTECTION_FLAG    3  //buckoff (system off / chg off)
/*M55 code for P231122-06914|QN6887A-4129 by lina at 20231124 end*/
/*M55 code for P231204-00897 by xiongxiaoliang at 20231208 start*/
bool g_slatemode_status = false;
/*M55 code for P231204-00897 by xiongxiaoliang at 20231208 end*/
/* sysfs write for "battery" psd */
ssize_t gxy_bat_store_attrs(struct device *dev, struct device_attribute *attr,
    const char *buf, size_t count)
{
    const ptrdiff_t offset = attr - gxy_battery_attrs;
    int ret = -EINVAL;
    int value = 0;
    /*M55 code for P231122-06914|QN6887A-4129 by lina at 20231124 start*/
    int num = 100;
    /*M55 code for P231122-06914|QN6887A-4129 by lina at 20231124 end*/

    switch (offset) {
        #if defined(CONFIG_GXY_FACTORY_BUILD)
        case BATT_CAP_CONTROL:
        /*M55 code for SR-QN6887A-01-539 by xiongxiaoliang at 20230919 start*/
        case INPUT_SUSPEND:
        /*M55 code for SR-QN6887A-01-539 by xiongxiaoliang at 20230919 end*/
        #endif //CONFIG_GXY_FACTORY_BUILD
        case STORE_MODE:
        case BATT_SLATE_MODE:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                ret = gxy_battery_set_prop(offset, value);
                if (ret < 0) {
                    GXY_PSY_ERR("%s: gxy_battery_set_prop error, ret=%d\n", __func__, ret);
                }
                /*M55 code for P231204-00897 by xiongxiaoliang at 20231219 start*/
                if (offset == BATT_SLATE_MODE) {
                    g_slatemode_status = !!value;
                }
                /*M55 code for P231204-00897 by xiongxiaoliang at 20231219 end*/
                ret = count;
            }
            break;
        /*M55 code for SR-QN6887A-01-526 by liufurong at 20231023 start*/
        /*M55 code for P231122-06914|QN6887A-4129 by lina at 20231124 start*/
         case BATT_FULL_CAPACITY:
            if (!strncmp(buf, g_basic_protection,(count-1))) {
                num = 100;
            } else if (!strncmp(buf, g_maximum_protection,(count-1))) {
                num = MAX_PROTECTION_FLAG;
            } else if (!strncmp(buf, g_sleep_protection,(count-1))) {
                num = SLEEP_PROTECTION_FLAG;
            } else if (!strncmp(buf, g_highsoc_protection,(count-1))) {
                num = HIGHSOC_PROTECTION_FLAG;
            } else {
                GXY_PSY_ERR("%s: Setting Value incorrect, cancel battery protection, %s %d\n",__func__, buf, num);
            }
            /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
            if (num != 100) {
                gxy_battery->batt_full_capacity = 80;
            } else {
                gxy_battery->batt_full_capacity = 100;
            }
            /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/
            GXY_PSY_ERR("%s:set battery protection, type = %s value = %d\n",__func__, buf, num);
            ret = gxy_battery_set_prop(offset, num);
            if (ret < 0) {
                GXY_PSY_ERR("%s: gxy_battery_set_prop error, ret=%d\n", __func__, ret);
            }
            ret = count;
            break;
        /*M55 code for P231122-06914|QN6887A-4129 by lina at 20231124 end*/
        /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 start*/
        case HV_DISABLE:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                ret = gxy_battery_set_prop(offset, value);
                if (ret < 0) {
                    GXY_PSY_ERR("%s: gxy_battery_set_prop error, ret=%d\n", __func__, ret);
                }
                ret = count;
            }
            break;
        /*M55 code for SR-QN6887A-01-515 by shixuanxuan at 20230923 end*/
        case BATT_TYPE:
            memset(s_gxy_battery_type, 0, BATTERY_TYPE_SIZE);
            strncpy(s_gxy_battery_type, buf, count);
            s_gxy_battery_type[count] = '\0';
            ret = count;
            break;
        case RESET_BATTERY_CYCLE:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                ret = gxy_battery_set_prop(offset, !!value);
                if (ret < 0) {
                    GXY_PSY_ERR("%s: gxy_battery_set_prop reset battery cycle error, ret=%d\n", __func__, ret);
                }
                ret = count;
            }
            break;
        /*M55 code for SR-QN6887A-01-526 by liufurong at 20231023 end*/
        /*M55 code for P231122-06687 by lina at 20231206 start*/
        case BATT_SOC_RECHG:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                ret = gxy_battery_set_prop(offset, value);
                if (ret < 0) {
                    GXY_PSY_ERR("%s: gxy_battery_set_prop error, ret=%d\n", __func__, ret);
                }
                ret = count;
            }
            pr_err("set batt_soc_rechg %d\n",value);
            break;
        /*M55 code for P231122-06687 by lina at 20231206 end*/
        /*M55 code for QN6887A-4205 by xiongxiaoliang at 20240102 start*/
        case DBG_BATTERY_CYCLE:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                ret = gxy_battery_set_prop(offset, value);
                if (ret < 0) {
                    GXY_PSY_ERR("%s: gxy_battery_set_prop error, ret=%d\n", __func__, ret);
                }
                ret = count;
            }
            break;
        /*M55 code for QN6887A-4205 by xiongxiaoliang at 20240102 end*/
        /*M55 code for P240109-02015 by shixuanxuan at 20240123 start*/
        case POWER_RESET:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                ret = gxy_battery_set_prop(offset, value);
                if (ret < 0) {
                    GXY_PSY_ERR("%s: gxy_battery_set_prop POWER_RESET error, ret=%d\n", __func__, ret);
                }
                ret = count;
            }
            break;

        /*M55 code for P240109-02015 by shixuanxuan at 20240123 end*/
        default:
            ret = -EINVAL;
            break;
    }

    return ret;
}

/* sysfs read for "usb" psd */
ssize_t gxy_usb_show_attrs(struct device *dev, struct device_attribute *attr, char * buf)
{
    const ptrdiff_t offset = attr - gxy_usb_attrs;
    int count = 0;
    int value = 0;
    /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
    gxy_online_state_t online_state = OFFLINE;
    /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/

    switch (offset) {
        case TYPEC_CC_ORIENT:
            value = gxy_usb_get_prop(offset);
            count = sprintf(buf, "%d\n", value);
            break;
        /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
        case USB_ONLINE:
            online_state = gxy_get_online_state();
            if (online_state == ONLINE_USB) {
                /*M55 code for P231204-00897 by xiongxiaoliang at 20231208 start*/
                if (g_slatemode_status == false) {
                    value = 1;
                } else {
                    value = 0;
                }
                /*M55 code for P231204-00897 by xiongxiaoliang at 20231208 end*/
            } else {
                value = 0;
            }
            count = sprintf(buf, "%d\n", value);
            break;
        /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/
        /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 start*/
        case ANALOG_EARPHONE_ONLINE:
            value = get_analog_online();
            count = sprintf(buf, "%d\n", value);
            break;
        /*M55 code for SR-QN6887A-01-555 by shixuanxuan at 20230907 end*/
        /*M55 code for QN6887A-66 by shixuanxuan at 20230917 start*/
        case CHG_PUMP_STATE:
            value = gxy_charg_pump_test_result();
            count = sprintf(buf, "%d\n", value);
            break;
        /*M55 code for QN6887A-66 by shixuanxuan at 20230917 end*/
        /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 start*/
        case PD_MAX_POWER:
            value = gxy_usb_get_prop(offset);
            count = sprintf(buf, "%d\n", value);
            break;
        /*M55 code for P231125-00024 by xiongxiaoliang at 20231211 end*/
        default:
            count = -EINVAL;
            break;
    }
    return count;
}

/* sysfs write for "usb" psd */
ssize_t gxy_usb_store_attrs(struct device *dev, struct device_attribute *attr,
    const char *buf, size_t count)
{
    const ptrdiff_t offset = attr - gxy_usb_attrs;
    int ret = -EINVAL;

    switch (offset) {
        default:
            ret = -EINVAL;
            break;
    }

    return ret;
}

/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
/* sysfs read for "ac" psd */
ssize_t gxy_ac_show_attrs(struct device *dev, struct device_attribute *attr, char * buf)
{
    const ptrdiff_t offset = attr - gxy_ac_attrs;
    int count = 0;
    int value = 0;
    gxy_online_state_t online_state = OFFLINE;
    /*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 start*/
    GXY_PSY_DEBUG("%s: gxy_ac_show_attrs enter\n", __func__);
    /*M55 code for SR-QN6887A-01-516 by xiongxiaoliang at 20230904 end*/
    switch (offset) {
        case AC_ONLINE:
            online_state = gxy_get_online_state();
            if (online_state == ONLINE_AC) {
                /*M55 code for P231204-00897 by xiongxiaoliang at 20231208 start*/
                if (g_slatemode_status == false) {
                    value = 1;
                } else {
                    value = 0;
                }
                /*M55 code for P231204-00897 by xiongxiaoliang at 20231208 end*/
            } else {
                value = 0;
            }
            count = sprintf(buf, "%d\n", value);
            break;
        default:
            count = -EINVAL;
            break;
    }
    return count;
}

/* sysfs write for "ac" psd */
ssize_t gxy_ac_store_attrs(struct device *dev, struct device_attribute *attr,
    const char *buf, size_t count)
{
    const ptrdiff_t offset = attr - gxy_ac_attrs;
    int ret = -EINVAL;
    GXY_PSY_ERR("%s: gxy_ac_store_attrs enter\n", __func__);

    switch (offset) {
        default:
            ret = -EINVAL;
            break;
    }

    return ret;
}
/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/

static int gxy_battery_create_attrs(struct device *dev)
{
    unsigned long i = 0;
    int ret = 0;

    for (i = 0; i < ARRAY_SIZE(gxy_battery_attrs); i++) {
        ret = device_create_file(dev, &gxy_battery_attrs[i]);
        if (ret) {
            goto create_attrs_failed;
        }
    }
    goto create_attrs_succeed;

create_attrs_failed:
    GXY_PSY_ERR("%s: failed (%d)\n", __func__, ret);
    while (i--) {
        device_remove_file(dev, &gxy_battery_attrs[i]);
    }

create_attrs_succeed:
    return ret;
}

static int gxy_usb_create_attrs(struct device *dev)
{
    unsigned long i = 0;
    int ret = 0;

    for (i = 0; i < ARRAY_SIZE(gxy_usb_attrs); i++) {
        ret = device_create_file(dev, &gxy_usb_attrs[i]);
        if (ret) {
            goto create_attrs_failed;
        }
    }
    goto create_attrs_succeed;

create_attrs_failed:
    GXY_PSY_ERR("%s: failed (%d)\n", __func__, ret);
    while (i--) {
        device_remove_file(dev, &gxy_usb_attrs[i]);
    }

create_attrs_succeed:
    return ret;
}
/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
static int gxy_ac_create_attrs(struct device *dev)
{
    unsigned long i = 0;
    int ret = 0;

    for (i = 0; i < ARRAY_SIZE(gxy_ac_attrs); i++) {
        ret = device_create_file(dev, &gxy_ac_attrs[i]);
        if (ret) {
            goto create_attrs_failed;
        }
    }
    goto create_attrs_succeed;

create_attrs_failed:
    GXY_PSY_ERR("%s: failed (%d)\n", __func__, ret);
    while (i--) {
        device_remove_file(dev, &gxy_ac_attrs[i]);
    }

create_attrs_succeed:
    return ret;
}
/*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/
static int gxy_psy_probe(struct platform_device *pdev)
{
    int ret = 0;

    struct gxy_battery *gxy_batt = devm_kzalloc(&pdev->dev, sizeof(*gxy_batt), GFP_KERNEL);

    if (!gxy_batt) {
        GXY_PSY_ERR("dont not kzmalloc\n", __func__);
        return -ENOMEM;
    }

    GXY_PSY_INFO("enter\n");

    gxy_batt->psy = power_supply_get_by_name("battery");
    if (IS_ERR_OR_NULL(gxy_batt->psy)) {
        return -EPROBE_DEFER;
    }

    ret = gxy_battery_create_attrs(&gxy_batt->psy->dev);
    if (ret) {
        GXY_PSY_ERR(" battery fail to create attrs!!\n", __func__);
        return ret;
    }

    GXY_PSY_INFO("Battery Install Success !!\n", __func__);

    gxy_batt->usb_psy = power_supply_get_by_name("usb");
    if (IS_ERR_OR_NULL(gxy_batt->usb_psy)) {
        return -EPROBE_DEFER;
    }
    ret = gxy_usb_create_attrs(&gxy_batt->usb_psy->dev);
    if (ret) {
        GXY_PSY_ERR(" usb fail to create attrs!!\n", __func__);
    }
    GXY_PSY_INFO("USB Install Success !!\n", __func__);

    /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 start*/
    gxy_batt->ac_psy = power_supply_get_by_name("ac");
    if (IS_ERR_OR_NULL(gxy_batt->ac_psy)) {
        return -EPROBE_DEFER;
    }
    ret = gxy_ac_create_attrs(&gxy_batt->ac_psy->dev);
    if (ret) {
        GXY_PSY_ERR(" ac fail to create attrs!!\n", __func__);
    }
    GXY_PSY_INFO("AC Install Success !!\n", __func__);
    /*M55 code for SR-QN6887A-01-590 by liufurong at 20230817 end*/
    /*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 start*/
    gxy_batt->otg_psy = power_supply_get_by_name("otg");
    if (IS_ERR_OR_NULL(gxy_batt->otg_psy)) {
        return -EPROBE_DEFER;
    }
    ret = gxy_otg_create_attrs(&gxy_batt->otg_psy->dev);
    if (ret) {
        GXY_PSY_ERR("%s: otg fail to create attrs!!\n", __func__);
    }
    GXY_PSY_INFO("%s: Otg Install Success !!\n", __func__);
    /*M55 code for SR-QN6887A-01-549 by shixuanxuan at 20230831 end*/

    return 0;
}

static int gxy_psy_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id gxy_psy_of_match[] = {
    {.compatible = "gxy,gxy_psy",},
    {},
};

MODULE_DEVICE_TABLE(of, gxy_psy_of_match);

static struct platform_driver gxy_psy_driver = {
    .probe = gxy_psy_probe,
    .remove = gxy_psy_remove,
    .driver = {
        .name = "gxy_psy",
        .of_match_table = of_match_ptr(gxy_psy_of_match),
    },
};

static int __init gxy_psy_init(void)
{
    GXY_PSY_INFO("init\n");

    return platform_driver_register(&gxy_psy_driver);
}
late_initcall(gxy_psy_init);

static void __exit gxy_psy_exit(void)
{
    GXY_PSY_INFO("exit\n");
    platform_driver_unregister(&gxy_psy_driver);
}
module_exit(gxy_psy_exit);

MODULE_AUTHOR("gxy_oem");
MODULE_DESCRIPTION("gxy psy module driver");
MODULE_LICENSE("GPL");
