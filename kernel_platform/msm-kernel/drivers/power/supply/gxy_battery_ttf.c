/*
 * gxy_battery_ttf.c
 * Samsung Mobile Battery Driver
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
#include <linux/power/gxy_psy_sysfs.h>
#include "gxy_battery_ttf.h"

struct gxy_battery_ttf *gxy_battery = NULL;
EXPORT_SYMBOL(gxy_battery);

static bool check_ttf_state(unsigned int capacity, int bat_sts)
{
    return ((bat_sts == POWER_SUPPLY_STATUS_CHARGING) ||
        (bat_sts == POWER_SUPPLY_STATUS_FULL && capacity != 100));
}

static int gxy_calc_ttf(struct gxy_battery_ttf * battery, unsigned int ttf_curr)
{
    struct gxy_cv_slope *cv_data = battery->ttf_d->cv_data;
    int i, cc_time = 0, cv_time = 0;
    int soc = battery->capacity;
    int charge_current = ttf_curr;
    int design_cap = battery->ttf_d->ttf_capacity;

    if (!cv_data || (ttf_curr <= 0)) {
        GXY_PSY_ERR("%s: no cv_data or val: %d\n", __func__, ttf_curr);
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
        /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 start*/
        if (i < (battery->ttf_d->cv_data_length - 1)) {
            battery->next_cv_time = cv_data[i + 1].time;
        }
        GXY_PSY_ERR("%s: cv_time: %d, next_cv_time:%d\n", __func__, cv_time, battery->next_cv_time);
        /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 end*/
    } else {        /* CC mode || NONE */
        cv_time = cv_data[i].time;
        cc_time =
            design_cap * (cv_data[i].soc - soc) / ttf_curr * 3600 / 100;
        GXY_PSY_ERR("%s: cc_time: %d\n", __func__, cc_time);
        if (cc_time < 0)
            cc_time = 0;
    }

    GXY_PSY_ERR("%s: cap:%d, soc:%d, T:%d, cv soc:%d, i:%d, ttf_curr:%d\n",
        __func__, design_cap, soc, cv_time + cc_time, cv_data[i].soc, i, ttf_curr);

    if (cv_time + cc_time >= 0)
        return cv_time + cc_time + 60;
    else
        return 60;    /* minimum 1minutes */
}

#define FULL_CAPACITY 80
static int gxy_calc_ttf_to_full_capacity(struct gxy_battery_ttf *battery, unsigned int ttf_curr)
{
    struct gxy_cv_slope *cv_data = battery->ttf_d->cv_data;
    int i, cc_time = 0, cv_time = 0;
    int soc = FULL_CAPACITY;
    int charge_current = ttf_curr;
    int design_cap = battery->ttf_d->ttf_capacity;

    if (!cv_data || (ttf_curr <= 0)) {
        GXY_PSY_ERR("%s: no cv_data or val: %d\n", __func__, ttf_curr);
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
            design_cap * (cv_data[i].soc - soc) / ttf_curr * 3600 / 100;
        GXY_PSY_ERR("%s: cc_time: %d\n", __func__, cc_time);
        if (cc_time < 0)
            cc_time = 0;
    }

    GXY_PSY_ERR("%s: cap:%d, soc:%d, T:%d, cv soc:%d, i:%d, ttf_curr:%d\n",
        __func__, design_cap, soc, cv_time + cc_time, cv_data[i].soc, i, ttf_curr);

    if (cv_time + cc_time >= 0)
        return cv_time + cc_time;
    else
        return 0;
}

static void gxy_bat_calc_time_to_full(struct gxy_battery_ttf * battery)
{
    union power_supply_propval val;
    int h_time = 0;
    int min_time = 0;
    static int saved_h_min_time = 0;
    /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 start*/
    struct timespec64 ts = {0, };
    /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 end*/

    if (delayed_work_pending(&battery->ttf_d->timetofull_work)) {
        GXY_PSY_ERR("%s: keep time_to_full(%5d sec)\n", __func__, battery->ttf_d->timetofull);
    } else if (check_ttf_state(battery->capacity, battery->bat_sts)) {
        int charge = 0;
        if (battery->psy_usb != NULL) {
            power_supply_get_property(battery->psy_usb, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
            battery->usb_vol_max = val.intval / 1000;
            power_supply_get_property(battery->psy_usb, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
            battery->usb_curr_max = val.intval / 1000;
            power_supply_get_property(battery->psy_usb, POWER_SUPPLY_PROP_USB_TYPE, &val);
            battery->usb_type = val.intval;
        } else {
            GXY_PSY_ERR("%s: battery->psy_usb == NULL\n", __func__);
        }

        battery->max_charge_power = (battery->usb_vol_max * battery->usb_curr_max) / 1000;
        GXY_PSY_ERR("%s: usb_vol_max[%d], usb_curr_max[%d], usb_type[%d], max_charge_power[%d]\n", __func__,
            battery->usb_vol_max, battery->usb_curr_max, battery->usb_type, battery->max_charge_power);

        // STEP1: USB TYPE judge
        if (battery->usb_type == POWER_SUPPLY_USB_TYPE_DCP ||
            battery->usb_type == POWER_SUPPLY_USB_TYPE_PD ||
            battery->usb_type == POWER_SUPPLY_USB_TYPE_PD_PPS) {
            battery->pd_max_charge_power_status = g_gxy_usb.pd_maxpower_status;
            if (battery->pd_max_charge_power_status == GXY_SUPER_FAST_CHARGING_2_0_45W) {
                charge = battery->ttf_d->ttf_pd45_charge_current;
            } else if (battery->pd_max_charge_power_status == GXY_SUPER_FAST_CHARGING_25W) {
                charge = battery->ttf_d->ttf_pd25_charge_current;
            } else if (battery->pd_max_charge_power_status == GXY_FAST_CHARGING_9VOR15W) {
                charge = battery->ttf_d->ttf_hv_charge_current;
            } else {
                if (battery->usb_vol_max == USB_HV_VLOTAGE_MAX) {
                    charge = battery->usb_curr_max * 2;
                } else {
                    charge = battery->usb_curr_max;
                }
            }
            GXY_PSY_ERR("%s: pd_max_charge_power_status [%d]\n", __func__, battery->pd_max_charge_power_status);
        } else if (battery->usb_type == POWER_SUPPLY_USB_TYPE_CDP) {
            charge = USB_TYPE_CDP_DEFAULT_CURR;
        } else if (battery->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
            charge = USB_TYPE_SDP_DEFAULT_CURR;
        } else {
            if (battery->usb_vol_max == USB_HV_VLOTAGE_MAX) {
                charge = battery->usb_curr_max * 2;
            } else {
                charge = battery->usb_curr_max;
            }
        }

        // STEP2: afc judge
        if (battery->ttf_afc_result == true) {
            charge = battery->ttf_d->ttf_hv_charge_current;
            GXY_PSY_ERR("%s: detected afc\n", __func__);
        }

        // STEP3: hv_disable judge
        if ((battery->ttf_hv_disable == true) && (charge >= battery->ttf_d->ttf_hv_disable_charge_current)) {
            charge = battery->ttf_d->ttf_hv_disable_charge_current;
            GXY_PSY_ERR("%s: hv disable\n", __func__);
        }

        /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 start*/
        // STEP4: battery protect judge
        if (battery->batt_full_capacity > 0 && battery->batt_full_capacity < 100) {
            GXY_PSY_ERR("%s: time to 80 percent\n", __func__);
            battery->timetofull_temp =
                gxy_calc_ttf(battery, charge) - gxy_calc_ttf_to_full_capacity(battery, charge);
        } else {
            battery->timetofull_temp = gxy_calc_ttf(battery, charge);
        }

        // time sec convert
        h_time = battery->timetofull_temp / 3600;
        min_time = (battery->timetofull_temp - h_time * 3600) / 60;

        if (saved_h_min_time != (h_time * 60 + min_time)) {
            saved_h_min_time = h_time * 60 + min_time;  //min
            battery->ttf_time_update = true;
            /*reset status & data*/
            battery->charging_reset_sts = true;
            battery->charging_passed_flag = false;
            battery->saved_charging_passed_time = 0;
        } else {
            if ((battery->capacity >= 98) && (battery->capacity < 100) && charge >= 889) {
                // STEP5：charging_pass_time
                ts = ktime_to_timespec64(ktime_get_boottime());
                if (battery->charging_reset_sts == true) {
                    battery->charging_reset_sts = false;
                    battery->charging_start_time = ts.tv_sec;
                } else {
                    if (ts.tv_sec >= battery->charging_start_time) {
                        battery->charging_passed_time = ts.tv_sec - battery->charging_start_time;
                    } else {
                        battery->charging_passed_time = 0xFFFFFFFF - battery->charging_start_time
                            + ts.tv_sec;
                    }
                }

                if ((battery->charging_passed_time >= 60) && (battery->charging_passed_flag == false)) {
                    battery->saved_charging_passed_time += battery->charging_passed_time;
                    battery->charging_start_time = ts.tv_sec;
                    battery->ttf_time_update = true;

                    if (battery->ttf_d->timetofull < battery->next_cv_time + 149) {
                        battery->charging_passed_flag = true;
                    }
                }

                battery->timetofull_temp -= battery->saved_charging_passed_time;
                GXY_PSY_ERR("%s: ts.tv_sec:%lld, charging_start_time=%lld, charging_passed_time:%lld, saved_charging_passed_time:%lld\n", __func__,
                    ts.tv_sec, battery->charging_start_time, battery->charging_passed_time, battery->saved_charging_passed_time);
            }
        }
        battery->ttf_d->timetofull = battery->timetofull_temp;
        /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 end*/
        GXY_PSY_ERR("%s: T:%d sec = [%dh%dmin], current:%d\n", __func__, battery->ttf_d->timetofull,
            h_time, min_time, charge);
    } else {
        battery->ttf_d->timetofull = -1;
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
        GXY_PSY_ERR("%s: np NULL\n", __func__);
        return 1;
    }

    ret = of_property_read_u32(np, "battery,ttf_hv_disable_charge_current",
                    &pttfdata->ttf_hv_disable_charge_current);
    if (ret) {
        pttfdata->ttf_hv_disable_charge_current = HV_DISABLE_CHARGE_DEFAULT_CURR;
        GXY_PSY_ERR("%s: ttf_hv_disable_charge_current is Empty, Default value %d\n",
            __func__, pttfdata->ttf_hv_disable_charge_current);
    }

    ret = of_property_read_u32(np, "battery,ttf_hv_charge_current",
                    &pttfdata->ttf_hv_charge_current);
    if (ret) {
        pttfdata->ttf_hv_charge_current = HV_CHARGE_DEFAULT_CURR;
        GXY_PSY_ERR("%s: ttf_hv_charge_current is Empty, Default value %d\n",
            __func__, pttfdata->ttf_hv_charge_current);
    }

    ret = of_property_read_u32(np, "battery,ttf_pd25_charge_current",
                    &pttfdata->ttf_pd25_charge_current);
    if (ret) {
        pttfdata->ttf_pd25_charge_current = PD25_CHARGE_DEFAULT_CURR;
        GXY_PSY_ERR("%s: ttf_pd25_charge_current is Empty, Default value %d\n",
            __func__, pttfdata->ttf_pd25_charge_current);
    }

    ret = of_property_read_u32(np, "battery,ttf_pd45_charge_current",
                    &pttfdata->ttf_pd45_charge_current);
    if (ret) {
        pttfdata->ttf_pd45_charge_current = PD45_CHARGE_DEFAULT_CURR;
        GXY_PSY_ERR("%s: ttf_pd45_charge_current is Empty, Default value %d \n",
            __func__, pttfdata->ttf_pd45_charge_current);
    }

    ret = of_property_read_u32(np, "battery,ttf_capacity", &pttfdata->ttf_capacity);
    if (ret < 0) {
        GXY_PSY_ERR("%s error reading capacity_calculation_type %d\n", __func__, ret);
        pttfdata->ttf_capacity = BATTERY_CAPACITY_SPEC;
    }

    p = of_get_property(np, "battery,cv_data", &len);
    if (p) {
        pttfdata->cv_data = kzalloc(len, GFP_KERNEL);
        pttfdata->cv_data_length = len / sizeof(struct gxy_cv_slope);
        GXY_PSY_ERR("%s: len= %ld, length= %d, %d\n", __func__,
            sizeof(int) * len, len, pttfdata->cv_data_length);
        ret = of_property_read_u32_array(np, "battery,cv_data",
            (u32 *)pttfdata->cv_data, len / sizeof(u32));
        if (ret) {
            GXY_PSY_ERR("%s: failed to read battery->cv_data: %d\n", __func__, ret);
            kfree(pttfdata->cv_data);
            pttfdata->cv_data = NULL;
        }
    } else {
        GXY_PSY_ERR("%s: there is not cv_data\n", __func__);
    }
    return 0;
}

static void gxy_bat_time_to_full_work(struct work_struct *work)
{
    union power_supply_propval val;

    if (gxy_battery->psy_bat != NULL) {
        power_supply_get_property(gxy_battery->psy_bat, POWER_SUPPLY_PROP_CAPACITY, &val);
        gxy_battery->capacity = val.intval;
        power_supply_get_property(gxy_battery->psy_bat, POWER_SUPPLY_PROP_STATUS, &val);
        gxy_battery->bat_sts = val.intval;

        gxy_bat_calc_time_to_full(gxy_battery);
        if (check_ttf_state(gxy_battery->capacity, gxy_battery->bat_sts)) {
            if (gxy_battery->ttf_time_update == true) {
                power_supply_changed(gxy_battery->psy_bat);
                gxy_battery->ttf_time_update = false;
            }
        }
    } else {
        GXY_PSY_ERR("%s: battery->psy_bat == NULL\n", __func__);
    }

    if (gxy_battery->monitor_wqueue != NULL) {
        queue_delayed_work(gxy_battery->monitor_wqueue,
            &gxy_battery->ttf_d->timetofull_work, msecs_to_jiffies(2000));
    } else {
        GXY_PSY_ERR("%s: battery->monitor_wqueue == NULL\n", __func__);
    }
}

void gxy_ttf_work_start(void)
{
    if ((gxy_battery != NULL) && (gxy_battery->ttf_d != NULL) && (gxy_battery->monitor_wqueue != NULL)) {
        cancel_delayed_work(&gxy_battery->ttf_d->timetofull_work);

        queue_delayed_work(gxy_battery->monitor_wqueue,
            &gxy_battery->ttf_d->timetofull_work, msecs_to_jiffies(1000));
        /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 start*/
        gxy_battery->charging_reset_sts = true;
        gxy_battery->charging_passed_flag = false;
        gxy_battery->charging_passed_time = 0;
        /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 end*/
        GXY_PSY_ERR("%s\n", __func__);
    } else {
        GXY_PSY_ERR("%s: point == NULL\n", __func__);
    }
}
EXPORT_SYMBOL(gxy_ttf_work_start);

void gxy_ttf_work_cancel(void)
{
    if ((gxy_battery != NULL) && (gxy_battery->ttf_d != NULL)) {
        cancel_delayed_work(&gxy_battery->ttf_d->timetofull_work);
        gxy_battery->pd_max_charge_power_status = 0;
        gxy_battery->ttf_afc_result = false;
        /*M55 code for QN6887A-5387 by xiongxiaoliang at 20240204 start*/
        gxy_battery->ttf_d->timetofull = -1;
        /*M55 code for QN6887A-5387 by xiongxiaoliang at 20240204 end*/
        GXY_PSY_ERR("%s\n", __func__);
    }
}
EXPORT_SYMBOL(gxy_ttf_work_cancel);

int gxy_ttf_display(void)
{
    union power_supply_propval val;

    if ((gxy_battery != NULL) && (gxy_battery->psy_bat != NULL) && (gxy_battery->ttf_d != NULL)) {
        power_supply_get_property(gxy_battery->psy_bat, POWER_SUPPLY_PROP_CAPACITY, &val);
        gxy_battery->capacity = val.intval;
        power_supply_get_property(gxy_battery->psy_bat, POWER_SUPPLY_PROP_STATUS, &val);
        gxy_battery->bat_sts = val.intval;

        GXY_PSY_ERR("gxy_ttf_display: [%d, %d]\n", gxy_battery->capacity, gxy_battery->bat_sts);
        if (gxy_battery->capacity == 100)
            return 0;

        if (check_ttf_state(gxy_battery->capacity, gxy_battery->bat_sts))
            return gxy_battery->ttf_d->timetofull;
    } else {
        GXY_PSY_ERR("%s: point == NULL\n", __func__);
    }

    return 0;
}
EXPORT_SYMBOL(gxy_ttf_display);

int gxy_ttf_init(void)
{
    gxy_battery = kzalloc(sizeof(*gxy_battery), GFP_KERNEL);
    if (!gxy_battery) {
        GXY_PSY_ERR("Failed to allocate battery memory\n");
        goto ttf_init_fail;
    }

    gxy_battery->ttf_d = kzalloc(sizeof(struct gxy_ttf_data), GFP_KERNEL);
    if (!gxy_battery->ttf_d) {
        GXY_PSY_ERR("Failed to allocate ttf_d memory\n");
        goto ttf_init_fail;
    }

    gxy_battery->psy_bat = power_supply_get_by_name("battery");
    if (IS_ERR_OR_NULL(gxy_battery->psy_bat)) {
        GXY_PSY_ERR("Failed to get battery psy\n");
    }

    gxy_battery->psy_usb = power_supply_get_by_name("usb");
    if (IS_ERR_OR_NULL(gxy_battery->psy_usb)) {
        GXY_PSY_ERR("Failed to get usb psy\n");
    }

    gxy_ttf_parse_dt(gxy_battery);
    gxy_battery->ttf_d->timetofull = -1;

    gxy_battery->monitor_wqueue = create_singlethread_workqueue("gxy_battery_ttf");
    if (!gxy_battery->monitor_wqueue) {
        GXY_PSY_ERR("%s: Fail to Create Workqueue\n", __func__);
        goto ttf_init_fail;
    }

    INIT_DELAYED_WORK(&gxy_battery->ttf_d->timetofull_work, gxy_bat_time_to_full_work);
    return 0;

ttf_init_fail:
    GXY_PSY_ERR("%s: Fail \n", __func__);
    return -1;
}
EXPORT_SYMBOL(gxy_ttf_init);
