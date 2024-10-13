/*
 * gxy__battery_ttf.h
 * Samsung Mobile Battery Header
 *
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __GXY_BATTERY_TTF_H__
#define __GXY_BATTERY_TTF_H__


#define USB_HV_VLOTAGE_MAX  9000
#define USB_TYPE_SDP_DEFAULT_CURR  500
#define USB_TYPE_CDP_DEFAULT_CURR  1100
#define HV_DISABLE_CHARGE_DEFAULT_CURR 1500
#define HV_CHARGE_DEFAULT_CURR 2931
#define PD25_CHARGE_DEFAULT_CURR 4507
#define PD45_CHARGE_DEFAULT_CURR 5732

#define BATTERY_CAPACITY_SPEC 4855

typedef struct gxy_cv_slope {
    int fg_current;
    int soc;
    int time;
}gxy_cv_slope_t;

typedef struct gxy_ttf_data {
    int timetofull;

    unsigned int ttf_hv_disable_charge_current;
    unsigned int ttf_hv_charge_current;
    unsigned int ttf_pd25_charge_current;
    unsigned int ttf_pd45_charge_current;

    gxy_cv_slope_t *cv_data;
    int cv_data_length;
    unsigned int ttf_capacity;

    struct delayed_work timetofull_work;
}gxy_ttf_data_t;

struct gxy_battery_ttf {
    struct power_supply *psy_bat;
    struct power_supply *psy_usb;
    gxy_ttf_data_t  *ttf_d;
    struct workqueue_struct *monitor_wqueue;

    unsigned int pd_max_charge_power_status;        /* max charge power for pd (1-4) */
    unsigned int max_charge_power;        /* max charge power (mW) */
    int cable_type;

    unsigned int capacity;     /* SOC (%) */
    int bat_sts;       /*charging status */
    int batt_full_capacity;  /*battery protect */
    unsigned int battery_full_capacity; /*battery capacity */
    bool ttf_hv_disable;
    bool ttf_afc_result;
    bool ttf_time_update;

    int usb_vol_max;
    int usb_curr_max;
    int usb_type;

    /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 start*/
    u64 charging_start_time;
    u64 charging_passed_time;
    int saved_charging_passed_time;
    bool charging_reset_sts;
    bool charging_passed_flag;
    int next_cv_time;
    int timetofull_temp;
    /*M55 code for P240131-05683 by xiongxiaoliang at 20240131 end*/
};

extern struct gxy_battery_ttf *gxy_battery;

extern int gxy_ttf_init(void);
extern void gxy_ttf_work_start(void);
extern void gxy_ttf_work_cancel(void);
extern int gxy_ttf_display(void);

#endif /* __GXY_BATTERY_H */
