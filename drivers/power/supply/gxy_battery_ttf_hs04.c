/*
 * gxy_battery_ttf.c
 * Samsung Mobile Full Charging Time
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/slab.h>
//#include <linux/power/gxy_psy_sysfs.h>
#include "gxy_battery_ttf_hs04.h"

#define FULL_CAPACITY 80

static struct gxy_battery_ttf *gxy_battery = NULL;
#ifdef CONFIG_AFC_CHARGER
extern struct charger_manager *ssinfo;
#endif
static bool gxy_ttf_init_psy(struct gxy_battery_ttf *battery) {
    bool value = true;
    // struct power_supply *psy = NULL;
    if (IS_ERR_OR_NULL(battery->psy_bat)) {
        battery->psy_bat = power_supply_get_by_name("battery");
        if (IS_ERR_OR_NULL(battery->psy_bat)) {
            pr_err("TTF Failed to get battery psy\n");
            value = false;
        }
    }
    if (IS_ERR_OR_NULL(battery->psy_usb)) {
        battery->psy_usb = power_supply_get_by_name("usb");
        if (IS_ERR_OR_NULL(battery->psy_usb)) {
            pr_err("TTF Failed to get usb psy\n");
            value = false;
        }
    }
    if (IS_ERR_OR_NULL(battery->psy_chg)) {
        battery->psy_chg = power_supply_get_by_name("mtk_charger_type");
        if (IS_ERR_OR_NULL(battery->psy_chg)) {
            pr_err("TTF Failed to get charger psy\n");
            value = false;
        }
    }
    #ifdef CONFIG_AFC_CHARGER
    if (IS_ERR_OR_NULL(battery->chg_info)) {
        battery->chg_info = ssinfo;
        if (battery->chg_info == NULL) {
            pr_err("%s: get mtk_charger failed\n", __func__);
            value = false;
        }
    }
    #endif
    return value;
}
static bool check_ttf_state(unsigned int capacity, int bat_sts)
{
    return ((bat_sts == POWER_SUPPLY_STATUS_CHARGING) ||
        (bat_sts == POWER_SUPPLY_STATUS_FULL && capacity != 100));
}
static int gxy_calc_ttf(struct gxy_battery_ttf *battery, unsigned int ttf_curr)
{
    struct gxy_cv_slope *cv_data = battery->ttf_d->cv_data;
    int i, cc_time = 0, cv_time = 0;
    int soc = battery->capacity;
    int charge_current = ttf_curr;
    int design_cap = battery->ttf_d->ttf_capacity;
    if (!cv_data || (ttf_curr <= 0)) {
        pr_err("%s: TTF no cv_data or val: %d\n", __func__, ttf_curr);
        return -1;
    }
    for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
        if (charge_current >= cv_data[i].fg_current)
            break;
    }
    i = i >= battery->ttf_d->cv_data_length ? battery->ttf_d->cv_data_length - 1 : i;
    if (cv_data[i].soc < soc) {
        for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
            if (soc <= cv_data[i].soc)
                break;
        }
        cv_time =
            ((cv_data[i - 1].time - cv_data[i].time) * (cv_data[i].soc - soc)
             / (cv_data[i].soc - cv_data[i - 1].soc)) + cv_data[i].time;
        pr_err("%s: TTF cv_time: %d\n", __func__, cv_time);
    } else {        /* CC mode || NONE */
        cv_time = cv_data[i].time;
        cc_time =
            3600 * design_cap * (cv_data[i].soc - soc) / 100 / ttf_curr;
        pr_err("%s: TTF cc_time: %d\n", __func__, cc_time);
        if (cc_time < 0)
            cc_time = 0;
    }
    pr_err("%s: TTF cap:%d, soc:%d, T:%d, cv soc:%d, i:%d, ttf_curr:%d\n",
        __func__, design_cap, soc, cv_time + cc_time, cv_data[i].soc, i, ttf_curr);
    if (cv_time + cc_time >= 0)
        return cv_time + cc_time + 60;
    else
        return 60;    /* minimum 1minutes */
}

static int gxy_calc_ttf_to_full_capacity(struct gxy_battery_ttf *battery, unsigned int ttf_curr)
{
    struct gxy_cv_slope *cv_data = battery->ttf_d->cv_data;
    int i, cc_time = 0, cv_time = 0;
    int soc = FULL_CAPACITY;
    int charge_current = ttf_curr;
    int design_cap = battery->ttf_d->ttf_capacity;
    if (!cv_data || (ttf_curr <= 0)) {
        pr_err("%s: TTF no cv_data or val: %d\n", __func__, ttf_curr);
        return -1;
    }
    for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
        if (charge_current >= cv_data[i].fg_current)
            break;
    }
    i = i >= battery->ttf_d->cv_data_length ? battery->ttf_d->cv_data_length - 1 : i;
    if (cv_data[i].soc < soc) {
        for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
            if (soc <= cv_data[i].soc)
                break;
        }
        cv_time =
            ((cv_data[i - 1].time - cv_data[i].time) * (cv_data[i].soc - soc)
             / (cv_data[i].soc - cv_data[i - 1].soc)) + cv_data[i].time;
    } else {        /* CC mode || NONE */
        cv_time = cv_data[i].time;
        cc_time =
            3600 * design_cap * (cv_data[i].soc - soc) / 100 / ttf_curr;
        pr_err("%s: TTF cc_time: %d\n", __func__, cc_time);
        if (cc_time < 0)
            cc_time = 0;
    }
    pr_err("%s:TTF cap:%d, soc:%d, T:%d, cv soc:%d, i:%d, ttf_curr:%d\n",
        __func__, design_cap, soc, cv_time + cc_time, cv_data[i].soc, i, ttf_curr);
    if (cv_time + cc_time >= 0)
        return cv_time + cc_time;
    else
        return 0;
}
static void gxy_bat_calc_time_to_full(struct gxy_battery_ttf *battery)
{
    union power_supply_propval val;
    //struct charger_manager *info = NULL;
    int charge = 0;
    int h_time = 0;
    int min_time = 0;
    if (delayed_work_pending(&battery->ttf_d->timetofull_work)) {
        pr_err("%s: TTF keep time_to_full(%5d sec)\n", __func__, battery->ttf_d->timetofull);
    } else if (check_ttf_state(battery->capacity, battery->bat_sts)) {
        #ifdef CONFIG_AFC_CHARGER
        info = battery->chg_info;
        if(info == NULL) {
            pr_err("%s: TTF mtk_charger init fail, wait for next loop\n", __func__);
            battery->ttf_d->timetofull = -1;
            return;
        }
        #endif
        power_supply_get_property(battery->psy_usb, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
        battery->usb_vol_now = val.intval;
        power_supply_get_property(battery->psy_bat, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
        battery->batt_curr_now = val.intval;
        #ifdef CONFIG_AFC_CHARGER
        power_supply_get_property(battery->psy_bat, POWER_SUPPLY_PROP_AFC_RESULT, &val);
        battery->afc_result = val.intval;
        #else
        battery->afc_result = 0;
        #endif
        power_supply_get_property(battery->psy_chg, POWER_SUPPLY_PROP_USB_TYPE, &val);
        battery->usb_type = val.intval;
        battery->max_charge_power = (battery->usb_vol_now) * (battery->batt_curr_now / 1000) / 1000;
        pr_err("%s: TTF usb_vol_now[%d], batt_curr_now[%d], usb_type[%d], max_charge_power[%d]\n", __func__,
            battery->usb_vol_now, battery->batt_curr_now, battery->usb_type, battery->max_charge_power);
        // USB TYPE judge
       if (battery->usb_type == POWER_SUPPLY_USB_TYPE_DCP){ /* other AC charging */
            if (battery->usb_vol_now >= USB_HV_VLOTAGE_MIN) {
                charge = battery->ttf_d->ttf_hv_charge_current;
            } else {
                charge = battery->ttf_d->ttf_dcp_charge_current;
            }
        } else if (battery->usb_type == POWER_SUPPLY_USB_TYPE_CDP) {
            charge = USB_TYPE_CDP_DEFAULT_CURR;
        } else if (battery->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
            charge = USB_TYPE_SDP_DEFAULT_CURR;
        } else {
            if (battery->usb_vol_now >= USB_HV_VLOTAGE_MIN) {
                charge = battery->ttf_d->ttf_hv_charge_current;
            } else {
                charge = battery->batt_curr_now/1000;
            }
        }
        // battery protect judge
        if (battery->batt_full_capacity > 0 && battery->batt_full_capacity != 100) {
            pr_err("%s: TTF time to 80 percent\n", __func__);
            battery->ttf_d->timetofull =
                gxy_calc_ttf(battery, charge) - gxy_calc_ttf_to_full_capacity(battery, charge);
        } else {
            battery->ttf_d->timetofull = gxy_calc_ttf(battery, charge);
        }
        // time sec convert
        h_time = battery->ttf_d->timetofull / 3600;
        min_time = (battery->ttf_d->timetofull - h_time * 3600) / 60;
        pr_err("%s: TTF T:%d sec = [%dh%dmin], current:%d, batt_full_soc:%d\n", __func__, battery->ttf_d->timetofull,
            h_time, min_time, charge, battery->batt_full_capacity);
    } else {
        battery->ttf_d->timetofull = 0;
    }
}
static int gxy_ttf_parse_dt(struct gxy_battery_ttf *battery)
{
    struct device_node *np;
    struct gxy_ttf_data *pttfdata = battery->ttf_d;
    int ret = 0, len = 0;
    const u32 *p;
    np = of_find_node_by_name(NULL, "gxy_battery_ttf");
    if (!np) {
        pr_err("%s:TTF np NULL\n", __func__);
        return 1;
    }
    ret = of_property_read_u32(np, "battery,ttf_dcp_charge_current",
                    &pttfdata->ttf_dcp_charge_current);
    if (ret) {
        pttfdata->ttf_dcp_charge_current = USB_TYPE_DCP_DEFAULT_CURR;
        pr_err("%s: TTF ttf_dcp_charge_current is Empty, Default value %d\n",
            __func__, pttfdata->ttf_dcp_charge_current);
    }
    #ifdef CONFIG_AFC_CHARGER
    ret = of_property_read_u32(np, "battery,ttf_hv_charge_current",
                    &pttfdata->ttf_hv_charge_current);
    if (ret) {
        pttfdata->ttf_hv_charge_current = HV_CHARGE_DEFAULT_CURR;
        pr_err("%s: ttf_hv_charge_current is Empty, Default value %d\n",
            __func__, pttfdata->ttf_hv_charge_current);
    }
    ret = of_property_read_u32(np, "battery,ttf_pd25_charge_current",
                    &pttfdata->ttf_pd25_charge_current);
    if (ret) {
        pttfdata->ttf_pd25_charge_current = PD25_CHARGE_DEFAULT_CURR;
        pr_err("%s: ttf_pd25_charge_current is Empty, Default value %d\n",
            __func__, pttfdata->ttf_pd25_charge_current);
    }
    ret = of_property_read_u32(np, "battery,ttf_pd45_charge_current",
                    &pttfdata->ttf_pd45_charge_current);
    if (ret) {
        pttfdata->ttf_pd45_charge_current = PD45_CHARGE_DEFAULT_CURR;
        pr_err("%s: ttf_pd45_charge_current is Empty, Default value %d \n",
            __func__, pttfdata->ttf_pd45_charge_current);
    }
    #endif
    ret = of_property_read_u32(np, "battery,ttf_capacity", &pttfdata->ttf_capacity);
    if (ret < 0) {
        pr_err("%sTTF error reading capacity_calculation_type %d\n", __func__, ret);
        pttfdata->ttf_capacity = BATTERY_CAPACITY_SPEC;
    }
    p = of_get_property(np, "battery,cv_data", &len);
    if (p) {
        pttfdata->cv_data = kzalloc(len, GFP_KERNEL);
        pttfdata->cv_data_length = len / sizeof(struct gxy_cv_slope);
        pr_err("%s:TTF len= %ld, length= %d, %d\n", __func__,
            sizeof(int) * len, len, pttfdata->cv_data_length);
        ret = of_property_read_u32_array(np, "battery,cv_data",
            (u32 *)pttfdata->cv_data, len / sizeof(u32));
        if (ret) {
            pr_err("%s: TTF failed to read battery->cv_data: %d\n", __func__, ret);
            kfree(pttfdata->cv_data);
            pttfdata->cv_data = NULL;
        }
    } else {
        pr_err("%s:TTF there is not cv_data\n", __func__);
    }
    return 0;
}
static void gxy_bat_time_to_full_work(struct work_struct *work)
{
    union power_supply_propval val;
    if (gxy_ttf_init_psy(gxy_battery) == false) {
        pr_err("%s: TTF init psy fail, wait next loop\n", __func__);
        cancel_delayed_work(&gxy_battery->ttf_d->timetofull_work);
        queue_delayed_work(gxy_battery->monitor_wqueue,
            &gxy_battery->ttf_d->timetofull_work, msecs_to_jiffies(2000));
        return;
    } else {
        power_supply_get_property(gxy_battery->psy_bat, POWER_SUPPLY_PROP_CAPACITY, &val);
        gxy_battery->capacity = val.intval;
        power_supply_get_property(gxy_battery->psy_bat, POWER_SUPPLY_PROP_STATUS, &val);
        gxy_battery->bat_sts = val.intval;
        power_supply_get_property(gxy_battery->psy_bat, POWER_SUPPLY_PROP_BATT_FULL_CAPACITY, &val);
        gxy_battery->batt_full_capacity = val.intval;
        gxy_bat_calc_time_to_full(gxy_battery);
        if (check_ttf_state(gxy_battery->capacity, gxy_battery->bat_sts)) {
            power_supply_changed(gxy_battery->psy_bat);
        } else {
            cancel_delayed_work(&gxy_battery->ttf_d->timetofull_work);
        }
    }
}
void gxy_ttf_work_start(void)
{
    if ((gxy_battery != NULL) && (gxy_battery->psy_bat != NULL) && (gxy_battery->ttf_d != NULL)) {
        cancel_delayed_work(&gxy_battery->ttf_d->timetofull_work);
        queue_delayed_work(gxy_battery->monitor_wqueue,
            &gxy_battery->ttf_d->timetofull_work, msecs_to_jiffies(1000));
        pr_err("%sTTF \n", __func__);
    } else {
        pr_err("%s: TTF point == NULL\n", __func__);
    }
}
EXPORT_SYMBOL(gxy_ttf_work_start);
int gxy_ttf_display(void)
{
    union power_supply_propval val;
    if (gxy_battery == NULL ) {
        pr_err("%s: TTF gxy_battery == NULL init psy fail, wait next loop\n", __func__);
        return -1;
    }
    if ( gxy_battery->ttf_d == NULL ) {
        pr_err("%s: TTF gxy_battery->ttf_d == NULL init psy fail, wait next loop\n", __func__);
        return -1;
    }
    if (gxy_battery->monitor_wqueue == NULL) {
        pr_err("%s: TTF gxy_battery->monitor_wqueue == NULL init psy fail, wait next loop\n", __func__);
        return -1;
    }
    if (gxy_ttf_init_psy(gxy_battery) == false) {
        pr_err("%s: TTF gxy_ttf_init_psy(gxy_battery) == false init psy fail, wait next loop\n", __func__);
        return -1;
    }
    power_supply_get_property(gxy_battery->psy_bat, POWER_SUPPLY_PROP_CAPACITY, &val);
    gxy_battery->capacity = val.intval;
    power_supply_get_property(gxy_battery->psy_bat, POWER_SUPPLY_PROP_STATUS, &val);
    gxy_battery->bat_sts = val.intval;
    pr_err("TTF gxy_ttf_display: [%d, %d]\n", gxy_battery->capacity, gxy_battery->bat_sts);
    if (gxy_battery->capacity == 100)
        return 0;
    if (check_ttf_state(gxy_battery->capacity, gxy_battery->bat_sts))
        return gxy_battery->ttf_d->timetofull;
    return 0;
}
EXPORT_SYMBOL(gxy_ttf_display);
void gxy_ttf_init(void)
{
    pr_err("TTF %s start\n", __func__);
    gxy_battery = kzalloc(sizeof(*gxy_battery), GFP_KERNEL);
    if (!gxy_battery) {
        pr_err("TTF Failed to allocate battery memory\n");
    }
    gxy_battery->ttf_d = kzalloc(sizeof(struct gxy_ttf_data), GFP_KERNEL);
    if (!gxy_battery->ttf_d) {
        pr_err("TTF Failed to allocate ttf_d memory\n");
    }
    gxy_battery->psy_bat = NULL;
    gxy_battery->psy_usb = NULL;
    gxy_battery->psy_chg = NULL;
    gxy_battery->chg_info = NULL;
    gxy_ttf_init_psy(gxy_battery);
    gxy_ttf_parse_dt(gxy_battery);
    gxy_battery->ttf_d->timetofull = -1;
    gxy_battery->monitor_wqueue = create_singlethread_workqueue("gxy_battery_ttf");
    if (!gxy_battery->monitor_wqueue) {
        pr_err("%s: TTF Fail to Create Workqueue\n", __func__);
    }
    INIT_DELAYED_WORK(&gxy_battery->ttf_d->timetofull_work, gxy_bat_time_to_full_work);
    pr_err("TTF %s end\n", __func__);
}
EXPORT_SYMBOL(gxy_ttf_init);
