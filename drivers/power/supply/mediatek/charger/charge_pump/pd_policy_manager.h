#ifndef __PD_POLICY_MANAGER_H__
#define __PD_POLICY_MANAGER_H__
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <../../../../../misc/mediatek/typec/tcpc/inc/tcpm.h>

/* A06 code for AL7160A-67 by jiashixian at 20240327 start*/
#define PSY_NAME_CP_MASTER      "cpm-master"
#define PSY_NAME_CP_SLAVE       "cpm-slave"
#define PSY_NAME_CP_STANDALONE         "cpm-standalone"
#define PSY_NAME_USB            "ac"
#define PSY_NAME_SW_CHARGER         "charger"
#define PSY_NAME_BMS            "battery"
#define TCPC_DEV_NAME           "type_c_port0"

#define JEITA_TEMP_T2_TO_T3_CP_CC1 6000
#define JEITA_TEMP_T2_TO_T3_CP_CC2 4700
#define JEITA_TEMP_T2_TO_T3_CP_CC3 3800

#define JEITA_TEMP_T2_TO_T3_CP_CV1 4160
#define JEITA_TEMP_T2_TO_T3_CP_CV2 4280
#define JEITA_TEMP_T3_TO_T4_CP_CV1 4250

#define JEITA_HIGH_TEMP_DISABLE_CP 52
#define JEITA_LOW_TEMP_DISABLE_CP -2


enum pm_state {
    PD_PM_STATE_ENTRY,
    PD_PM_STATE_FC2_ENTRY,
    PD_PM_STATE_FC2_ENTRY_1,
    PD_PM_STATE_FC2_ENTRY_2,
    PD_PM_STATE_FC2_ENTRY_3,
    PD_PM_STATE_FC2_TUNE,
    PD_PM_STATE_FC2_EXIT,
};

struct sw_device {
    bool charge_enabled;

    int vbat_volt;
    int ibat_curr;
    int ibus_curr;
    int bat_temp;
    /*A06 code for P240514-06911 by shixuanxuan at 20240515 start*/
    int hv_disable_state;
    /*A06 code for P240514-06911 by shixuanxuan at 20240515 end*/
};
/* A06 code for AL7160A-67 by jiashixian at 20240327 end*/

struct cp_device {
    bool charge_enabled;
    bool vbus_err_low;
    bool vbus_err_high;

    int vbus_volt;
    int ibus_curr;
    int vbat_volt;
    int ibat_curr;

    int die_temp;
};

#if 0
enum {
    CP_STOP = BIT(0),    /* bit0: CP stop */
    CP_REENTER = BIT(1), /* bit1：CP reenter */
    CP_EXIT = BIT(2),    /* bit2：CP exit */
    CP_DONE = BIT(3),    /* bit3：CP done */
};

struct cp_charging {
    int cp_chg_status;
    int cp_jeita_cc;
    int cp_jeita_cv;
    int cp_therm_cc;
};
#endif
struct usbpd_pm {
    struct device *dev;

    enum pm_state state;
    struct cp_device cp;
    struct cp_device cp_sec;
    struct sw_device sw;

    struct tcpc_device *tcpc;
    struct notifier_block tcp_nb;
    bool is_pps_en_unlock;
    int hrst_cnt;

    bool cp_sec_stopped;

    bool pd_active;
    bool pps_supported;

    int    request_voltage;
    int    request_current;

    int fc2_taper_timer;
    int ibus_lmt_change_timer;

    int    apdo_max_volt;
    int    apdo_max_curr;
    int    apdo_selected_pdo;

    struct delayed_work pm_work;

    struct notifier_block nb;

    bool psy_change_running;
    struct work_struct cp_psy_change_work;
    struct work_struct usb_psy_change_work;
    spinlock_t psy_change_lock;

    struct power_supply *cp_psy;
    struct power_supply *cp_sec_psy;
    struct power_supply *sw_psy;
    struct power_supply *usb_psy;
    struct power_supply *bms_psy;
};

struct pdpm_config {
    int    bat_volt_lp_lmt; /*bat volt loop limit*/
    int    bat_curr_lp_lmt;
    int    bus_volt_lp_lmt;
    int    bus_curr_lp_lmt;

    int    fc2_taper_current;
    int    fc2_steps;

    int    min_adapter_volt_required;
    int min_adapter_curr_required;

    int    min_vbat_for_cp;

    bool cp_sec_enable;
    bool fc2_disable_sw;        /* disable switching charger during flash charge*/

};

/* A06 code for AL7160A-62 | AL7160A-67 by jiashixian at 20240327 start*/
struct cp_charging_param {
    int cp_chg_enable;
    int cp_jeita_stop_chg;
    int cp_thermal_stop_chg;
    int cp_fac_stop_chg;
    int cp_jeita_cc;
    int cp_jeita_cv;
    /* A06 code for AL7160A-2776 by xiongxiaoliang at 20240606 start*/
    int cp_battery_cv;
    /* A06 code for AL7160A-2776 by xiongxiaoliang at 20240606 end*/
    int cp_therm_cc;
    /*A06 code for SR-AL7160A-01-258 by shixuanxuan at 20240417 start*/
    bool cp_ss_stop_chg;
    /*A06 code for SR-AL7160A-01-258 by shixuanxuan at 20240417 end*/
};

extern struct cp_charging_param g_cp_charging_lmt;
/* A06 code for AL7160A-62 | AL7160A-67 by jiashixian at 20240327 end*/
#endif /* SRC_PDLIB_USB_PD_POLICY_MANAGER_H_ */
