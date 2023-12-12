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
#include "mtk_battery.h"
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
#include "mtk_charger.h"
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/


static struct gxy_bat_hwinfo s_hwinfo_data;
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
static struct gxy_usb_hwinfo s_hwinfo_usb_data;
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/
/*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
static struct gxy_battery_data data_t;
#endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 end*/

/* For chg_info requirement*/
/*Tab A9 code for SR-AX6739A-01-491 by qiaodan at 20230508 start*/
static const char * const GXY_BAT_CHG_INFO_TEXT[] = {
    [GXY_BAT_CHG_INFO_UNKNOWN]  =  "charger-Unknown",
    [GXY_BAT_CHG_INFO_SC89601D]  =  "1:charger-SC89601D",
    [GXY_BAT_CHG_INFO_UPM6910DS]  =  "2:charger-UPM6910DS",
    [GXY_BAT_CHG_INFO_UPM6910DH]  =  "3:charger-UPM6910DH",
    /* Tab A9 code for AX6739A-1334 by gaozhengwei at 20230620 start */
    [GXY_BAT_CHG_INFO_SGM41515D]  =  "4:charger-SGM41515D",
    /* Tab A9 code for AX6739A-1334 by gaozhengwei at 20230620 end */
};
/*Tab A9 code for SR-AX6739A-01-491 by qiaodan at 20230508 end*/

void gxy_bat_set_chginfo(enum gxy_bat_chg_info cinfo_data)
{
    s_hwinfo_data.cinfo = cinfo_data;
}
EXPORT_SYMBOL(gxy_bat_set_chginfo);

/*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 start*/
void gxy_usb_set_pdhub_flag(bool is_pdhub)
{
    /*Tab A9 code for SR-AX6739A-01-483 by wenyaqi at 20230608 start*/
    static struct mtk_charger *s_info = NULL;
    struct power_supply *psy = NULL;

    s_hwinfo_usb_data.pdhub_flag = is_pdhub;
    GXY_PSY_INFO("%s flag is %d\n", __func__, is_pdhub);
    if (s_info == NULL) {
        psy = power_supply_get_by_name("mtk-master-charger");
        if (psy == NULL) {
            GXY_PSY_ERR("%s: get mtk-master-charger psy failed\n", __func__);
            return;
        } else {
            s_info = (struct mtk_charger *)power_supply_get_drvdata(psy);
            if (s_info == NULL) {
                GXY_PSY_ERR("%s: get mtk_charger failed\n", __func__);
                return;
            }
        }
    }
    s_info->gxy_cint.wake_up_charger(s_info);
    /*Tab A9 code for SR-AX6739A-01-483 by wenyaqi at 20230608 end*/
}
EXPORT_SYMBOL(gxy_usb_set_pdhub_flag);

bool gxy_usb_get_pdhub_flag(void)
{
    /*Tab A9 code for AX6739A-723 by qiaodan at 20230606 start*/
    GXY_PSY_INFO("%s flag is %d\n", __func__, s_hwinfo_usb_data.pdhub_flag);
    /*Tab A9 code for AX6739A-723 by qiaodan at 20230606 end*/
    return s_hwinfo_usb_data.pdhub_flag;
}
EXPORT_SYMBOL(gxy_usb_get_pdhub_flag);
/*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 end*/

/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
void gxy_usb_set_usbccflag(enum gxy_usb_orient cc_flag)
{
    s_hwinfo_usb_data.usb_cc_flag = cc_flag;
    GXY_PSY_INFO("%s flag is %d\n", __func__, cc_flag);
}
EXPORT_SYMBOL(gxy_usb_set_usbccflag);
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/

/*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 start*/
/* For tcpc_info requirement*/
static const char * const GXY_BAT_TCPC_INFO_TEXT[] = {
    [GXY_BAT_TCPC_INFO_UNKNOWN]  =  "tcpc-Unknown",
    [GXY_BAT_TCPC_INFO_ET7304MQ]  =  "1:tcpc-ET7304MQ",
    [GXY_BAT_TCPC_INFO_AW35615]  =  "2:tcpc-AW35615",
    [GXY_BAT_TCPC_INFO_CPS8851MRE]  = "3:tcpc-CPS8851MRE",
};

void gxy_bat_set_tcpcinfo(enum gxy_bat_tcpc_info tinfo_data)
{
    s_hwinfo_data.tinfo = tinfo_data;
}
EXPORT_SYMBOL(gxy_bat_set_tcpcinfo);
/*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 end*/
/*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 start*/
static const char * const GXY_BATTERY_TYPE_TEXT[] = {
    [GXY_BATTERY_TYPE_ATL]    = "1:battery-ATL",
    [GXY_BATTERY_TYPE_BYD] = "2:battery-BYD",
    [GXY_BATTERY_TYPE_UNKNOWN] = "UNKNOWN",
};
/*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
static const char * const GXY_CHARGE_TYPE_TEXT[] = {
    [POWER_SUPPLY_CHARGE_TYPE_UNKNOWN]     = "Unknown",
    [POWER_SUPPLY_CHARGE_TYPE_NONE]        = "N/A",
    [POWER_SUPPLY_CHARGE_TYPE_TRICKLE]     = "Trickle",
    [POWER_SUPPLY_CHARGE_TYPE_FAST]        = "Fast",
    [POWER_SUPPLY_CHARGE_TYPE_TAPER]       = "Taper",
};
#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 end*/
void gxy_bat_set_batterytype(enum gxy_battery_type binfo_data)
{
    s_hwinfo_data.binfo = binfo_data;
}
EXPORT_SYMBOL(gxy_bat_set_batterytype);
/*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 end*/

/*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 start*/
void gxy_bat_set_afc_result(int afc_result)
{
    if (afc_result >= AFC_5V) {
        s_hwinfo_data.afc_result = 1;
    } else {
        s_hwinfo_data.afc_result = 0;
    }
}
EXPORT_SYMBOL(gxy_bat_set_afc_result);

void gxy_bat_set_hv_disable(int hv_disable)
{
    static struct mtk_charger *s_info = NULL;
    struct power_supply *psy = NULL;

    if (s_info == NULL) {
        psy = power_supply_get_by_name("mtk-master-charger");
        if (psy == NULL) {
            GXY_PSY_ERR("%s: get mtk-master-charger psy failed\n", __func__);
            return;
        } else {
            s_info = (struct mtk_charger *)power_supply_get_drvdata(psy);
            if (s_info == NULL) {
                GXY_PSY_ERR("%s: get mtk_charger failed\n", __func__);
                return;
            }
        }
    }

    s_info->enable_hv_charging = (hv_disable ? false: true);
    GXY_PSY_INFO("%s: set hv_disable = (%d)\n", __func__, hv_disable);
}
EXPORT_SYMBOL(gxy_bat_set_hv_disable);
/*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 end*/

/*Tab A9 code for SR-AX6739A-01-512 by wenyaqi at 20230523 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
static int ss_fast_charger_status(struct mtk_charger *info)
{
    int ret = 0;

    if (info == NULL) {
        return 0;
    }
    if (info->enable_hv_charging == false) {
        return 0;
    }
    if (s_hwinfo_data.afc_result == 1 ||
        info->pd_type == MTK_PD_CONNECT_PE_READY_SNK ||
        info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 ||
        info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
        ret = 1;
    }

    return ret;
}
#endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-512 by wenyaqi at 20230523 end*/

/*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
static int gxy_bat_get_temp(void)
{
    static struct power_supply *s_batt_psy = NULL;
    int ret = 0;
    union power_supply_propval prop = {0};
    const int normal_temp = 250; // 25 degree

    if (s_batt_psy == NULL) {
        s_batt_psy = power_supply_get_by_name("battery");
        if (s_batt_psy == NULL) {
            GXY_PSY_ERR("%s: get battery psy failed\n", __func__);
            return normal_temp;
        }
    }
    ret = power_supply_get_property(s_batt_psy, POWER_SUPPLY_PROP_TEMP, &prop);
    if (ret < 0) {
        GXY_PSY_ERR("%s:get temp property fail\n", __func__);
        return normal_temp;
    }
    return prop.intval;
}

static int gxy_bat_get_current_now(void)
{
    static struct power_supply *s_batt_psy = NULL;
    int ret = 0;
    union power_supply_propval prop = {0};
    const int current_now = 50000; // 500ma

    if (s_batt_psy == NULL) {
        s_batt_psy = power_supply_get_by_name("battery");
        if (s_batt_psy == NULL) {
            GXY_PSY_ERR("%s: get battery psy failed\n", __func__);
            return current_now;
        }
    }
    ret = power_supply_get_property(s_batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
    if (ret < 0) {
        GXY_PSY_ERR("%s:get temp property fail\n", __func__);
        return current_now;
    }
    return prop.intval;
}

static int gxy_get_bat_discharge_level(void)
{
    static struct mtk_battery *s_info = NULL;
    struct power_supply *psy = NULL;
    int bat_discharge_level = 0;

    if (s_info == NULL) {
        psy = power_supply_get_by_name("battery");
        if (psy == NULL) {
            GXY_PSY_ERR("%s: get battery psy failed\n", __func__);
            return bat_discharge_level;
        } else {
            s_info = (struct mtk_battery *)power_supply_get_drvdata(psy);
            if (s_info == NULL) {
                GXY_PSY_ERR("%s: get mtk_battery failed\n", __func__);
                return bat_discharge_level;
            }
        }
    }

    bat_discharge_level = (s_info->bat_cycle * 100) +
            (abs(s_info->gauge->fg_hw_info.ncar) * 100 / s_info->bat_cycle_thr);

    pr_err("%s: get b_c = %d, get ncar = %d, get b_c_thr = %d, get b_d_l = %d\n",
            __func__, s_info->bat_cycle, s_info->gauge->fg_hw_info.ncar,
            s_info->bat_cycle_thr, bat_discharge_level);

    return bat_discharge_level;
}

static int gxy_get_bat_cycle(void)
{
    static struct mtk_battery *s_info = NULL;
    struct power_supply *psy = NULL;
    int batt_cycle = 0;

    if (s_info == NULL) {
        psy = power_supply_get_by_name("battery");
        if (psy == NULL) {
            GXY_PSY_ERR("%s: get battery psy failed\n", __func__);
            return batt_cycle;
        } else {
            s_info = (struct mtk_battery *)power_supply_get_drvdata(psy);
            if (s_info == NULL) {
                GXY_PSY_ERR("%s: get mtk_battery failed\n", __func__);
                return batt_cycle;
            }
        }
    }

    batt_cycle = s_info->bat_cycle;

    pr_err("%s: get bat_cycle = %d\n", __func__, s_info->bat_cycle);

    return batt_cycle;
}

static void gxy_batt_set_reset_cycle(bool val)
{
    static struct mtk_battery *s_info = NULL;
    struct power_supply *psy = NULL;

    if (s_info == NULL) {
        psy = power_supply_get_by_name("battery");
        if (psy == NULL) {
            GXY_PSY_ERR("%s: get battery psy failed\n", __func__);
            return;
        } else {
            s_info = (struct mtk_battery *)power_supply_get_drvdata(psy);
            if (s_info == NULL) {
                GXY_PSY_ERR("%s: get mtk_battery failed\n", __func__);
                return;
            }
        }
    }
    s_info->reset_cycle(s_info, val);
}

static int gxy_batt_get_reset_cycle(void)
{
    static struct mtk_battery *s_info = NULL;
    struct power_supply *psy = NULL;
    int val = 0;

    if (s_info == NULL) {
        psy = power_supply_get_by_name("battery");
        if (psy == NULL) {
            GXY_PSY_ERR("%s: get battery psy failed\n", __func__);
            return val;
        } else {
            s_info = (struct mtk_battery *)power_supply_get_drvdata(psy);
            if (s_info == NULL) {
                GXY_PSY_ERR("%s: get mtk_battery failed\n", __func__);
                return val;
            }
        }
    }
    pr_err("%s: get reset_cycle = %d\n", __func__, s_info->is_reset_battery_cycle);
    return s_info->is_reset_battery_cycle;
}
#endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 end*/
/*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
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
#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 end*/
/* include attrs for battery psy */
static struct device_attribute gxy_battery_attrs[] = {
    GXY_BATTERY_ATTR(chg_info),
    /*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 start*/
    GXY_BATTERY_ATTR(tcpc_info),
    /*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 end*/
    /*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 start*/
    GXY_BATTERY_ATTR(battery_type),
    /*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 end*/
    /*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 start*/
    #if defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    GXY_BATTERY_ATTR(batt_cap_control),
    #endif //CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-4867 by qiaodan at 20230515 end*/
    /*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    GXY_BATTERY_ATTR(batt_full_capacity),
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 end*/
    /*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    GXY_BATTERY_ATTR(store_mode),
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 end*/
    /*Tab A9 code for SR-AX6739A-01-456 | P230713-00914 by qiaodan at 20230713 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    GXY_BATTERY_ATTR(batt_slate_mode),
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-456 | P230713-00914 by qiaodan at 20230713 end*/
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    GXY_BATTERY_ATTR(input_suspend),
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
    /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 start*/
    GXY_BATTERY_ATTR(afc_result),
    GXY_BATTERY_ATTR(hv_disable),
    /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 end*/
    /*Tab A9 code for SR-AX6739A-01-512 by wenyaqi at 20230523 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    GXY_BATTERY_ATTR(hv_charger_status),
    #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-512 by wenyaqi at 20230523 end*/
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
    GXY_BATTERY_ATTR(shipmode),
    GXY_BATTERY_ATTR(shipmode_reg),
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/
    /*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 start*/
    GXY_BATTERY_ATTR(dump_charger_ic),
    /*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 end*/
    /*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 start*/
    #ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
    GXY_BATTERY_ATTR(batt_misc_event),
    #endif//!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 end*/
    /*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 start*/
    #ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
    GXY_BATTERY_ATTR(batt_current_event),
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 end*/
    /*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    GXY_BATTERY_ATTR(batt_temp),
    GXY_BATTERY_ATTR(batt_discharge_level),
    GXY_BATTERY_ATTR(batt_type),
    GXY_BATTERY_ATTR(batt_current_ua_now),
    GXY_BATTERY_ATTR(battery_cycle),
    GXY_BATTERY_ATTR(reset_battery_cycle),
    #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 end*/
    /*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    GXY_BATTERY_ATTR(charge_type),
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 end*/
};

/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
static struct device_attribute gxy_usb_attrs[] = {
    GXY_USB_ATTR(typec_cc_orientation),
};
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/

/* sysfs read for "battery" psd */
ssize_t gxy_bat_show_attrs(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    const ptrdiff_t offset = attr - gxy_battery_attrs;
    int count = 0;
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
    int ship_flag = 0;
    /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/
    /*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 start*/
    int dump_flag = 0;
    /*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 end*/

    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    static struct mtk_charger *s_info = NULL;
    struct power_supply *psy = NULL;
    /*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    int charge_status = 0;
    #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
    /*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 end*/

    if (s_info == NULL) {
        psy = power_supply_get_by_name("mtk-master-charger");
        if (psy == NULL) {
            return -ENODEV;
        } else {
            s_info = (struct mtk_charger *)power_supply_get_drvdata(psy);
            if (s_info == NULL) {
                return -ENODEV;
            }
        }
    }
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

    switch (offset) {
        case CHG_INFO:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n",
                    GXY_BAT_CHG_INFO_TEXT[s_hwinfo_data.cinfo]);
        break;
        /*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 start*/
        case TCPC_INFO:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n",
                    GXY_BAT_TCPC_INFO_TEXT[s_hwinfo_data.tinfo]);
        break;
        /*Tab A9 code for SR-AX6739A-01-470 by wenyaqi at 20230504 end*/
        /*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 start*/
        case BATTERY_TYPE:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n",
                    GXY_BATTERY_TYPE_TEXT[s_hwinfo_data.binfo]);
        break;
        /*Tab A9 code for SR-AX6739A-01-515 by lina at 20230505 end*/
        /*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 start*/
        #if defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case BATT_CAP_CONTROL:
                count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    s_info->gxy_discharge_batt_cap_control);
            break;
        #endif //CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 end*/
        /*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case BATT_FULL_CAPACITY:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    s_info->gxy_discharge_batt_full_capacity);
            break;
        #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 end*/
        /*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case STORE_MODE:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    s_info->gxy_discharge_store_mode);
            break;
        #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 end*/
        /*Tab A9 code for SR-AX6739A-01-456 | P230713-00914 by qiaodan at 20230713 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case BATT_SLATE_MODE:
        #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-456 | P230713-00914 by qiaodan at 20230713 end*/
        /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
        case INPUT_SUSPEND:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    s_info->gxy_discharge_input_suspend);
            break;
        /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
        /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 start*/
        case AFC_RESULT:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    s_hwinfo_data.afc_result);
            break;
        case HV_DISABLE:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    (s_info->enable_hv_charging ? 0 : 1));
            break;
        /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 end*/
        /*Tab A9 code for SR-AX6739A-01-512 by wenyaqi at 20230523 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case HV_CHARGER_STATUS:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    (ss_fast_charger_status(s_info)));
            break;
        #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-512 by wenyaqi at 20230523 end*/
        /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
        case SHIPMODE:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    s_info->gxy_shipmode_flag);
            break;
        case SHIPMODE_REG:
            ship_flag = s_info->gxy_cint.get_ship_mode(s_info);
            GXY_PSY_INFO("get ship mode reg = %d\n",ship_flag);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    ship_flag);
            break;
        /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/
        /*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 start*/
        case DUMP_CHARGER_IC:
            dump_flag = s_info->gxy_cint.dump_charger_ic(s_info);
            GXY_PSY_INFO("dump_charger_ic = %d\n", dump_flag);
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    dump_flag);
            break;
        /*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 end*/
        /*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 start*/
        #ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
        case BATT_MISC_EVENT:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                s_info->gxy_cint.batt_misc_event(s_info));
            break;
        #endif//!CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 end*/
        /*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 start*/
        #ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
        case BATT_CURRENT_EVENT:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                s_info->gxy_cint.batt_current_event(s_info));
            break;
        #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 end*/
        /*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case BATT_TEMP:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    (gxy_bat_get_temp()));
            break;
        case BATT_DISCHARGE_LEVEL:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    (gxy_get_bat_discharge_level()));
            break;
        case BATT_TYPE:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n",
                    data_t.cust_batt_type);
            break;
        case BATT_CURRENT_UA_NOW:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    (gxy_bat_get_current_now()));
            break;
        case BATTERY_CYCLE:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    (gxy_get_bat_cycle()));
            break;
        case RESET_BATTERY_CYCLE:
            count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
                    (gxy_batt_get_reset_cycle()));
            break;
        #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 end*/
        /*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case CHARGE_TYPE:
            charge_status = gxy_get_chr_type();
            count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n",
                    (GXY_CHARGE_TYPE_TEXT[charge_status]));
            break;
        #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-454 by lina at 20230607 end*/
    default:
        count = -EINVAL;
        break;
    }
    return count;
}

/*Tab A9 code for P230713-00914 by qiaodan at 20230724 start*/
/* sysfs write for "battery" psd */
ssize_t gxy_bat_store_attrs(
                    struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    const ptrdiff_t offset = attr - gxy_battery_attrs;
    int ret = -EINVAL;
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
    static struct mtk_charger *s_info = NULL;
    struct power_supply *psy = NULL;
    int value = 0;

    if (s_info == NULL) {
        psy = power_supply_get_by_name("mtk-master-charger");
        if (psy == NULL) {
            return -ENODEV;
        } else {
            s_info = (struct mtk_charger *)power_supply_get_drvdata(psy);
            if (s_info == NULL) {
                return -ENODEV;
            }
        }
    }
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/

    switch (offset) {
        /*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 start*/
        #if defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case BATT_CAP_CONTROL:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                s_info->gxy_discharge_batt_cap_control = !!value;
                GXY_PSY_INFO("%s: set batt_cap_control = (%d)\n", __func__, value);
                ret = count;
            }
            break;
        #endif //CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 end*/
        /*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case BATT_FULL_CAPACITY:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                s_info->gxy_discharge_batt_full_capacity = value;
                GXY_PSY_INFO("%s: set batt_full_capacity = (%d)\n", __func__, value);
                ret = count;
            }
            break;
        #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 end*/
        /*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case STORE_MODE:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                s_info->gxy_discharge_store_mode = !!value;
                GXY_PSY_INFO("%s: set store_mode = (%d)\n", __func__, value);
                ret = count;
            }
            break;
        #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 end*/
        /*Tab A9 code for SR-AX6739A-01-456 | P230713-00914 by qiaodan at 20230713 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case BATT_SLATE_MODE:
        #endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-456 | P230713-00914 by qiaodan at 20230713 end*/
        /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
        case INPUT_SUSPEND:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                s_info->gxy_discharge_input_suspend = value;
                GXY_PSY_INFO("%s: set input_suspend = (%d)\n", __func__, value);
                ret = count;
            }
            break;
        /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
        /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 start*/
        case HV_DISABLE:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                gxy_bat_set_hv_disable(value);
                GXY_PSY_INFO("%s: set hv_disable = (%d)\n", __func__, value);
                ret = count;
            }
            break;
        /*Tab A9 code for SR-AX6739A-01-499 by wenyaqi at 20230515 end*/
        /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
        case SHIPMODE:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                s_info->gxy_shipmode_flag = !!value;
                s_info->gxy_cint.set_ship_mode(s_info, s_info->gxy_shipmode_flag);
                GXY_PSY_INFO("%s: enable shipmode val = %d\n",__func__, value);
                ret = count;
            }
            break;
        /*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/
        /*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 start*/
        #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
        case BATT_TYPE:
            strncpy(data_t.cust_batt_type, buf, count);
            data_t.cust_batt_type[count] = '\0';
            ret = count;
            break;
        case RESET_BATTERY_CYCLE:
            if (sscanf(buf, "%10d\n", &value) == 1) {
                gxy_batt_set_reset_cycle(!!value);
                GXY_PSY_INFO("%s: set reset_cycle = (%d)\n", __func__, value);
                ret = count;
            }
            break;
        #endif // !CONFIG_ODM_CUSTOM_FACTORY_BUILD
        /*Tab A9 code for SR-AX6739A-01-521 by shanxinkai at 20230601 end*/
        default:
            ret = -EINVAL;
            break;
    }

    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230514 start*/
    s_info->gxy_cint.wake_up_charger(s_info);
    /*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230514 end*/

    return ret;
}
/*Tab A9 code for P230713-00914 by qiaodan at 20230724 end*/

/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
/* sysfs read for "usb" psd */
ssize_t gxy_usb_show_attrs(struct device *dev,
                            struct device_attribute *attr, char * buf)
{
    const ptrdiff_t offset = attr - gxy_usb_attrs;
    int count = 0;

    switch (offset) {
        case TYPEC_CC_ORIENT:
            count = sprintf(buf, "%d\n", s_hwinfo_usb_data .usb_cc_flag);
            break;
    default:
        count = -EINVAL;
        break;
    }
    return count;
}

/* sysfs write for "usb" psd */
ssize_t gxy_usb_store_attrs(
                    struct device *dev,
                    struct device_attribute *attr,
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
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/

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

/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
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
/*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506  end*/

static int gxy_psy_probe(struct platform_device *pdev)
{
    int ret = 0;

    struct gxy_battery *g_batt = devm_kzalloc(&pdev->dev, sizeof(*g_batt), GFP_KERNEL);

    if (!g_batt) {
        GXY_PSY_ERR("dont not kzmalloc\n", __func__);
        return -ENOMEM;
    }

    GXY_PSY_INFO("enter\n");

    g_batt->psy = power_supply_get_by_name("battery");
    if (IS_ERR_OR_NULL(g_batt->psy)) {
        return -EPROBE_DEFER;
    }

    ret = gxy_battery_create_attrs(&g_batt->psy->dev);
    if (ret) {
        GXY_PSY_ERR(" battery fail to create attrs!!\n", __func__);
        return ret;
    }

    GXY_PSY_INFO("Battery Install Success !!\n", __func__);

    /*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 start*/
    g_batt->usb_psy = power_supply_get_by_name("usb");
    if (IS_ERR_OR_NULL(g_batt->usb_psy)) {
        return -EPROBE_DEFER;
    }
    ret = gxy_usb_create_attrs(&g_batt->usb_psy->dev);
    if (ret) {
        GXY_PSY_ERR(" usb fail to create attrs!!\n", __func__);
    }
    GXY_PSY_INFO("USB Install Success !!\n", __func__);
    /*Tab A9 code for SR-AX6739A-01-467 by hualei at 20230506 end*/

    return 0;
}

static int gxy_psy_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id gxy_psy_of_match[] = {
    {.compatible = "gxy, gxy_psy",},
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
    platform_driver_unregister(&gxy_psy_driver);
}
module_exit(gxy_psy_exit);

MODULE_AUTHOR("Lucas-Gao");
MODULE_DESCRIPTION("gxy psy driver");
MODULE_LICENSE("GPL");
