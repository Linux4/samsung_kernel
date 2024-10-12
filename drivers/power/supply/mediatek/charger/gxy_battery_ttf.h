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

#include "mtk_charger_intf.h"


#define USB_HV_VLOTAGE_MIN  8000
#define USB_TYPE_SDP_DEFAULT_CURR  495
#define USB_TYPE_CDP_DEFAULT_CURR  978
#define USB_TYPE_DCP_DEFAULT_CURR 1585
#define HV_CHARGE_DEFAULT_CURR 2655
#define PD25_CHARGE_DEFAULT_CURR 2655
#define PD45_CHARGE_DEFAULT_CURR 2655

#define BATTERY_CAPACITY_SPEC 4980

typedef struct gxy_cv_slope {
    int fg_current;
    int soc;
    int time;
}gxy_cv_slope_t;

typedef struct gxy_ttf_data {
    int timetofull;

    unsigned int ttf_dcp_charge_current;
    unsigned int ttf_hv_charge_current;
    unsigned int ttf_pd25_charge_current;
    unsigned int ttf_pd45_charge_current;

    gxy_cv_slope_t *cv_data;
    int cv_data_length;
    unsigned int ttf_capacity;

    struct delayed_work timetofull_work;
}gxy_ttf_data_t;

/* hs14_u code for AL6528AU-252 by liufurong at 2024/02/19 start */
struct gxy_battery_ttf {
    struct power_supply *psy_bat;
    struct power_supply *psy_usb;
    struct power_supply *psy_chg;
    struct charger_manager *chg_info;
    gxy_ttf_data_t  *ttf_d;
    struct workqueue_struct *monitor_wqueue;

    unsigned int pd_max_charge_power_status;        /* max charge power for pd (1-4) */
    unsigned int max_charge_power;        /* max charge power (mW) */
    int cable_type;

    unsigned int capacity;     /* SOC (%) */
    int bat_sts;       /*charging status */
    bool ttf_time_update;
    int saved_h_min_time;

    int usb_vol_now;
    int batt_curr_now;
    int usb_type;
    int afc_result;
    u64 charging_start_time;
    u64 charging_passed_time;
    int saved_charging_passed_time;
    bool charging_reset_sts;
    bool charging_passed_flag;
    int next_cv_time;
    int timetofull_temp;
};

extern int gxy_ttf_init(void);
extern void gxy_ttf_work_start(void);
extern void gxy_ttf_work_cancel(void);
extern int gxy_ttf_display(void);
/* hs14_u code for AL6528AU-252 by liufurong at 2024/02/19 end */
#endif /* __GXY_BATTERY_H */
