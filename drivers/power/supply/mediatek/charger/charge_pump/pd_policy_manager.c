#define pr_fmt(fmt)    "[UPM-USBPD-PM]: %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
//#include <linux/usb/usbpd.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "pd_policy_manager.h"

//config battery charge full voltage
/*A06 code for AL7160A-67 by jiashixian at 20240415 start*/
#define BATT_MAX_CHG_VOLT           4450
/*A06 code for AL7160A-67 by jiashixian at 20240415 end*/
//config fast charge current
#define BATT_FAST_CHG_CURR          6000

//config vbus max voltage
#define    BUS_OVP_THRESHOLD           11000

//config open CP vbus/vbat
/*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
#define BUS_VOLT_INIT_UP            208 / 100
#define BUS_VOLT_INIT_MIN           206 / 100
#define BUS_VOLT_INIT_MAX           215 / 100
/*A06 code for AL7160A-231 by jiashixian at 20240512 end*/
//config monitor time (ms)
#define PM_WORK_RUN_INTERVAL        300

#define BAT_VOLT_LOOP_LMT           BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT           BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT           BUS_OVP_THRESHOLD
/* A06 code for AL7160A-31 | AL7160A-684 by jiashixian at 20240507 begin */
#define CHECK_PPS_RETRY 15
/* A06 code for AL7160A-31 | AL7160A-684 by jiashixian at 20240507 end */
/*A06 code for AL7160A-830 by jiashixian at 20240525 start*/
#define BAT_CV_OFFSET 50 //cv-50mv
/*A06 code for AL7160A-830 by jiashixian at 20240525 end*/


enum {
    PM_ALGO_RET_OK,
    PM_ALGO_RET_CHG_DISABLED,
    PM_ALGO_RET_TAPER_DONE,
};

/* A06 code for AL7160A-67 by jiashixian at 20240415 start*/
struct cp_charging_param g_cp_charging_lmt = {
    .cp_chg_enable = 0,
    .cp_jeita_stop_chg = 0,
    .cp_thermal_stop_chg = 0,
    .cp_fac_stop_chg = 0,
    .cp_jeita_cc = 6000000,
    .cp_jeita_cv = 4450000,
    .cp_therm_cc = 6000000,
    /*A06 code for SR-AL7160A-01-258 by shixuanxuan at 20240417 start*/
    .cp_ss_stop_chg = 0,
    /*A06 code for SR-AL7160A-01-258 by shixuanxuan at 20240417 end*/
};
/* A06 code for AL7160A-67 by jiashixian at 20240415 end*/
static struct pdpm_config pm_config = {
    .bat_volt_lp_lmt            = BAT_VOLT_LOOP_LMT,
    .bat_curr_lp_lmt            = BAT_CURR_LOOP_LMT,
    .bus_volt_lp_lmt            = BUS_VOLT_LOOP_LMT,
    .bus_curr_lp_lmt            = (BAT_CURR_LOOP_LMT >> 1),

    //config CP to main charger current(ma)
    .fc2_taper_current          = 1500,
    //config adapter voltage step(PPS:1-->20mV)
    .fc2_steps                  = 1,
    //config adapter pps pdo min voltage
    .min_adapter_volt_required  = 11000,
    //config adapter pps pdo min curremt
    .min_adapter_curr_required  = 2000,
    //config CP charge min vbat voltage
    .min_vbat_for_cp            = 3600,
    //config standalone(false) or master+slave(true) CP
    .cp_sec_enable              = false,
    //config CP charging, main charger is disable
    .fc2_disable_sw                = true,
};

static struct usbpd_pm *__pdpm;

/****************Charge Pump API*****************/
static void usbpd_check_cp_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->cp_psy) {
        if (pm_config.cp_sec_enable){
                 pr_err("cp_psy master found\n");
            pdpm->cp_psy = power_supply_get_by_name(PSY_NAME_CP_MASTER);
        }
        else {
                pr_err("cp_psy STANDALONE found\n");
            pdpm->cp_psy = power_supply_get_by_name(PSY_NAME_CP_STANDALONE);
        }
        if (!pdpm->cp_psy)
            pr_err("cp_psy not found\n");
    }
}

static void usbpd_check_cp_sec_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->cp_sec_psy) {
        pdpm->cp_sec_psy = power_supply_get_by_name(PSY_NAME_CP_SLAVE);
        if (!pdpm->cp_sec_psy)
            pr_err("cp_sec_psy not found\n");
    }
}

static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_psy(pdpm);
    if (!pdpm->cp_psy)
        return;

    /* A06 code for AL7160A-42 by jiashixian at 2024/04/07 start */
    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CPM_VBUS_VOLT, &val);
    if (!ret)
        pdpm->cp.vbus_volt = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CPM_IBUS_CURRENT, &val);
    if (!ret)
        pdpm->cp.ibus_curr = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CPM_VBAT_VOLT, &val);
    if (!ret)
        pdpm->cp.vbat_volt = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CPM_IBAT_CURRENT, &val);
    if (!ret)
        pdpm->cp.ibat_curr = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CPM_TEMP, &val);
    if (!ret)
        pdpm->cp.die_temp = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
    if (!ret)
        pdpm->cp.charge_enabled = val.intval;
}

static void usbpd_pm_update_cp_sec_status(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    if (!pm_config.cp_sec_enable) {
        return;
    }

    usbpd_check_cp_sec_psy(pdpm);
    if (!pdpm->cp_sec_psy) {
        return;
    }

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_CPM_VBUS_VOLT, &val);

    if (!ret) {
        pdpm->cp_sec.vbus_volt = val.intval;
    }

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_CPM_IBUS_CURRENT, &val);

    if (!ret) {
        pdpm->cp_sec.ibus_curr = val.intval;
    }

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_CPM_VBAT_VOLT, &val);

    if (!ret) {
        pdpm->cp_sec.vbat_volt = val.intval;
    }

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_CPM_IBAT_CURRENT, &val);

    if (!ret) {
        pdpm->cp_sec.ibat_curr = val.intval;
    }

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_CPM_TEMP, &val);

    if (!ret) {
        pdpm->cp_sec.die_temp = val.intval;
    }

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);

    /* A06 code for AL7160A-67 by jiashixian at 20240415 start*/
    if (!ret) {
        pdpm->cp_sec.charge_enabled = false;//val.intval;
    }
}
    /* A06 code for AL7160A-67 by jiashixian at 20240415 end*/
    /* A06 code for AL7160A-42 by jiashixian at 2024/04/07 end */

static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_psy(pdpm);
    if (!pdpm->cp_psy)
        return -ENODEV;

    val.intval = enable;
    ret = power_supply_set_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);

    return ret;
}

static int usbpd_pm_enable_cp_sec(struct usbpd_pm *pdpm, bool enable)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_sec_psy(pdpm);
    if (!pdpm->cp_sec_psy)
        return -ENODEV;

    val.intval = enable;
    ret = power_supply_set_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);

    return ret;
}

static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};
    usbpd_check_cp_psy(pdpm);

    if (!pdpm->cp_psy)
        return -ENODEV;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
    /* A06 code for AL7160A-67 by jiashixian at 20240415 start*/
    if (!ret) {
        pdpm->cp.charge_enabled = !!val.intval;
    }
    /* A06 code for AL7160A-67 by jiashixian at 20240415 end*/
    return ret;
}

static int usbpd_pm_check_cp_sec_enabled(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_sec_psy(pdpm);
    if (!pdpm->cp_sec_psy)
        return -ENODEV;

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
    /* A06 code for AL7160A-67 by jiashixian at 20240415 start*/
    if (!ret)
        pdpm->cp_sec.charge_enabled = false;//!!val.intval;
    /* A06 code for AL7160A-67 by jiashixian at 20240415 end*/

    return ret;
}

/*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
#if 0
static void usbpd_check_sw_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name(PSY_NAME_SW_CHARGER);
        if (!pdpm->sw_psy) {
            pr_err("sw psy not found!\n");
        }
    }
}

static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool enable)
{
    /*TODO: config main charger enable or disable
    If the main charger needs to take 100mA current when the CC charging, you
    can set the max current to 100mA when disable
    */
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_sw_psy(pdpm);
    if (!pdpm->sw_psy) {
        return -ENODEV;
    }

    if(enable)   val.intval = 1600000;//3000000
    else         val.intval = 100000;

    ret = power_supply_set_property(pdpm->sw_psy,
            POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);

    return ret;
}

static int usbpd_pm_check_sw_enabled(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_sw_psy(pdpm);
    if (!pdpm->sw_psy) {
        return -ENODEV;
    }

    ret = power_supply_get_property(pdpm->sw_psy,
        POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
    if (!ret)
    {
        if(val.intval <= 500000)  pdpm->sw.charge_enabled = false;
        else  pdpm->sw.charge_enabled = true;
    }

    return ret;
}

static void usbpd_pm_update_sw_status(struct usbpd_pm *pdpm)
{
    usbpd_pm_check_sw_enabled(pdpm);
}
#endif
/*A06 code for AL7160A-231 by jiashixian at 20240512 end*/
/***************PD API****************/
static inline int check_typec_attached_snk(struct tcpc_device *tcpc)
{
    u8 type = 0;

    if (!tcpc){
           pr_err("tcpc is null return\n");// fix no pd dump
        return -EINVAL;
    }
    type = tcpm_inquire_typec_attach_state(tcpc);
    if (type != TYPEC_ATTACHED_SNK) {
        pr_err("no typec_attached_snk, type:%d\n", type);
        return -EINVAL;
    }

    return 0;
}

static int usbpd_pps_enable_charging(struct usbpd_pm *pdpm, bool en,
                u32 mV, u32 mA)
{
    int ret, cnt = 0;

    if (check_typec_attached_snk(pdpm->tcpc) < 0)
        return -EINVAL;
    pr_err("en = %d, %dmV, %dmA\n", en, mV, mA);

    do {
        if (en)
            ret = tcpm_set_apdo_charging_policy(pdpm->tcpc,
                DPM_CHARGING_POLICY_PPS, mV, mA, NULL);
        else
            ret = tcpm_reset_pd_charging_policy(pdpm->tcpc, NULL);
        cnt++;
    } while (ret != TCP_DPM_RET_SUCCESS && cnt < 3);

    if (ret != TCP_DPM_RET_SUCCESS)
        pr_err("fail(%d)\n", ret);
    return ret > 0 ? -ret : ret;
}

static bool usbpd_get_pps_status(struct usbpd_pm *pdpm)
{
    int ret, apdo_idx = -1;
    struct tcpm_power_cap_val apdo_cap = {0};
    u8 cap_idx;

    pr_err("++\n");
    if (check_typec_attached_snk(pdpm->tcpc) < 0)
        return false;

    if (!pdpm->is_pps_en_unlock) {
        pr_err("pps en is locked\n");
        return false;
    }

    if (!tcpm_inquire_pd_pe_ready(pdpm->tcpc)) {
        pr_err("PD PE not ready\n");
        return false;
    }

    /* select TA boundary */
    cap_idx = 0;
    while (1) {
        ret = tcpm_inquire_pd_source_apdo(pdpm->tcpc,
                        TCPM_POWER_CAP_APDO_TYPE_PPS,
                        &cap_idx, &apdo_cap);
        if (ret != TCP_DPM_RET_SUCCESS) {
            pr_err("inquire pd apdo fail(%d)\n", ret);
            break;
        }

        pr_err("cap_idx[%d], %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
            apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma,
            apdo_cap.pwr_limit);

        /*
        * !(apdo_cap.min_mv <= data->vcap_min &&
        *   apdo_cap.max_mv >= data->vcap_max &&
        *   apdo_cap.ma >= data->icap_min)
        */
        if (apdo_cap.max_mv < pm_config.min_adapter_volt_required ||
            apdo_cap.ma < pm_config.min_adapter_curr_required)
            continue;
        if (apdo_idx == -1) {
            apdo_idx = cap_idx;
            pdpm->apdo_max_volt = apdo_cap.max_mv;
            pdpm->apdo_max_curr = apdo_cap.ma;
        } else {
            if (apdo_cap.ma > pdpm->apdo_max_curr) {
                apdo_idx = cap_idx;
                pdpm->apdo_max_volt = apdo_cap.max_mv;
                pdpm->apdo_max_curr = apdo_cap.ma;
            }
        }
    }
    if (apdo_idx != -1) {
        pr_err("select potential cap_idx[%d]\n", apdo_idx);
        ret = usbpd_pps_enable_charging(pdpm, true, 5000, 3000);
        if (ret != TCP_DPM_RET_SUCCESS)
            return false;
        return true;
    }
    return false;
}

static int usbpd_select_pdo(struct usbpd_pm *pdpm, u32 mV, u32 mA)
{
    int ret, cnt = 0;

    if (check_typec_attached_snk(pdpm->tcpc) < 0)
        return -EINVAL;
    pr_err("%dmV, %dmA\n", mV, mA);

    if (!tcpm_inquire_pd_connected(pdpm->tcpc)) {
        pr_err("pd not connected\n");
        return -EINVAL;
    }

    do {
        ret = tcpm_dpm_pd_request(pdpm->tcpc, mV, mA, NULL);
        cnt++;
    } while (ret != TCP_DPM_RET_SUCCESS && cnt < 3);

    if (ret != TCP_DPM_RET_SUCCESS)
        pr_err("fail(%d)\n", ret);
    return ret > 0 ? -ret : ret;
}

static int pca_pps_tcp_notifier_call(struct notifier_block *nb,
                    unsigned long event, void *data)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, tcp_nb);
    struct tcp_notify *noti = data;

    pr_err("event:%d, connected:%d\n", event, noti->pd_state.connected);

    switch (event) {
    case TCP_NOTIFY_PD_STATE:
        switch (noti->pd_state.connected) {
        case PD_CONNECT_NONE:
            pr_err("detached\n");
            pdpm->is_pps_en_unlock = false;
            pdpm->hrst_cnt = 0;

            if (pdpm->usb_psy) {
                    power_supply_changed(pdpm->usb_psy);
                }
            break;
        case PD_CONNECT_HARD_RESET:
            pdpm->hrst_cnt++;
            pr_err("pd hardreset, cnt = %d\n",
                pdpm->hrst_cnt);
            pdpm->is_pps_en_unlock = false;
            break;
        case PD_CONNECT_PE_READY_SNK_APDO:
            if (pdpm->hrst_cnt < 5) {
                pr_err("en unlock\n");
                pdpm->is_pps_en_unlock = true;

                if (pdpm->usb_psy) {
                    power_supply_changed(pdpm->usb_psy);
                }
            }
            break;
        default:
            break;
        }
    default:
        break;
    }
    return NOTIFY_OK;
}

static void usbpd_check_tcpc(struct usbpd_pm *pdpm)
{
    int ret;

    if (!pdpm->tcpc) {
        pdpm->tcpc = tcpc_dev_get_by_name(TCPC_DEV_NAME);
        if (!pdpm->tcpc) {
            pr_err("get tcpc dev fail\n");
            return;
        }
        pdpm->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
        ret = register_tcp_dev_notifier(pdpm->tcpc, &pdpm->tcp_nb,
                        TCP_NOTIFY_TYPE_USB);
        if (ret < 0) {
            pr_err("register tcpc notifier fail\n");
        }
    }
}

/*******************main charger API********************/
static void usbpd_check_usb_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->usb_psy) {
        pdpm->usb_psy = power_supply_get_by_name(PSY_NAME_USB);
        if (!pdpm->usb_psy)
            pr_err("usb psy not found!\n");
    }
}

static int usbpd_update_ibat_curr(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->bms_psy) {
        pdpm->bms_psy = power_supply_get_by_name(PSY_NAME_BMS);
        if (!pdpm->bms_psy) {
            return -ENODEV;
        }
    }

    /**
     * The charging current must be a positive number.
     * If the current is negative during charging,
     * please modify it to a positive number by yourself!
     * eg: pdpm->sw.ibat_curr= -(int)(val.intval/1000);
     */
    ret = power_supply_get_property(pdpm->bms_psy,
            POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    if (!ret)
        pdpm->sw.ibat_curr= (int)(val.intval/1000);

    ret = power_supply_get_property(pdpm->bms_psy,
            POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
    if (!ret)
        pdpm->sw.vbat_volt = (int)(val.intval/1000);

    /* A06 code for AL7160A-67 by jiashixian at 20240415 start*/
    ret = power_supply_get_property(pdpm->bms_psy,
            POWER_SUPPLY_PROP_TEMP, &val);
    if (!ret) {
        pdpm->sw.bat_temp = (int)(val.intval/10);
    }
    /* A06 code for AL7160A-67 by jiashixian at 20240415 end*/
    /*A06 code for P240514-06911 by shixuanxuan at 20240515 start*/
    ret = power_supply_get_property(pdpm->bms_psy,
            POWER_SUPPLY_PROP_HV_DISABLE, &val);
    if (!ret) {
        pdpm->sw.hv_disable_state = val.intval;
    }
    /*A06 code for P240514-06911 by shixuanxuan at 20240515 end*/
    return ret;
}
/*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
#if 0
static int usbpd_update_sw_ibus_curr(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_sw_psy(pdpm);
    if (!pdpm->sw_psy) {
        return -ENODEV;
    }

    ret = power_supply_get_property(pdpm->sw_psy,
            POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
    if (!ret)
        pdpm->sw.ibus_curr = (int)(val.intval/1000);

    return ret;
}
#endif
/*A06 code for AL7160A-231 by jiashixian at 20240512 end*/
static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
    bool retValue;
    /* o8 code for AL7160A-31 by jiashixian at 20240401 begin */
    int i = 0;
    /* o8 code for AL7160A-31 by jiashixian at 20240401 end */

    retValue = usbpd_get_pps_status(pdpm);
    if (retValue)
        pdpm->pps_supported = true;
    /* o8 code for AL7160A-31 by jiashixian at 20240401 begin */
    for (i = 0; i < CHECK_PPS_RETRY; ++i) {
        if (retValue) {
            break;
        }
        msleep(100);
        retValue = usbpd_get_pps_status(pdpm);
        if (retValue) {
        pdpm->pps_supported = true;
        }
        pr_info("usbpd_get_pps_status i====:%d\n",i);
        if (pdpm->pps_supported) {
            break;
        }
    }
    /* o8 code for AL7160A-31 by jiashixian at 20240401 end */

    if (pdpm->pps_supported)
        pr_info("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
                pdpm->apdo_selected_pdo,
                pdpm->apdo_max_volt,
                pdpm->apdo_max_curr);
    else
        pr_info("Not qualified PPS adapter\n");
}

#define TAPER_TIMEOUT    (5000 / PM_WORK_RUN_INTERVAL)
#define IBUS_CHANGE_TIMEOUT  (500 / PM_WORK_RUN_INTERVAL)
static int usbpd_pm_fc2_charge_algo(struct usbpd_pm *pdpm)
{
    int steps;
    int step_vbat = 0;
    int step_ibus = 0;
    int step_ibat = 0;
    int ibus_total = 0;
    int ibat_limit = 0;
    int vbat_now = 0;
    int ibat_now = 0;
    /* A06 code for AL7160A-67 | AL7160A-78 by jiashixian at 20240417 start*/
    int bat_temp_now = 0;
    int vbat_sw_now = 0;

    static int ibus_limit = 0;
    static int jeita_ibat_current = 0;

    /*if vbat_volt update by main charger
    * vbat_limit = pdpm->sw.vbat_volt;
    */
    vbat_now = pdpm->cp.vbat_volt;
    bat_temp_now = pdpm->sw.bat_temp;
    vbat_sw_now = pdpm->sw.vbat_volt;

    if (vbat_now < JEITA_TEMP_T2_TO_T3_CP_CV1) {
        jeita_ibat_current = JEITA_TEMP_T2_TO_T3_CP_CC1;
    } else if (vbat_now < JEITA_TEMP_T2_TO_T3_CP_CV2) {
        jeita_ibat_current = JEITA_TEMP_T2_TO_T3_CP_CC2;
    } else {
        jeita_ibat_current = JEITA_TEMP_T2_TO_T3_CP_CC3;
    }

    pm_config.bat_curr_lp_lmt = min(jeita_ibat_current, g_cp_charging_lmt.cp_jeita_cc / 1000);
    pm_config.bat_volt_lp_lmt = g_cp_charging_lmt.cp_jeita_cv / 1000;
    /* A06 code for AL7160A-2776 by xiongxiaoliang at 20240606 start*/
    if (g_cp_charging_lmt.cp_battery_cv != 0) {
       pm_config.bat_volt_lp_lmt = min(pm_config.bat_volt_lp_lmt, g_cp_charging_lmt.cp_battery_cv / 1000);
    }
    pm_config.bat_curr_lp_lmt = min(pm_config.bat_curr_lp_lmt, g_cp_charging_lmt.cp_therm_cc / 1000);
    pr_err("jeita_cc(%d) thermal_cc(%d) jeita_cv(%d) battery_cv(%d)\n",
        g_cp_charging_lmt.cp_jeita_cc, g_cp_charging_lmt.cp_therm_cc,
        g_cp_charging_lmt.cp_jeita_cv, g_cp_charging_lmt.cp_battery_cv);
    pr_err("vbat_now(%d) bat_temp_now(%d) vbat_sw_now(%d)\n", vbat_now, bat_temp_now, vbat_sw_now);
    pr_err("bat_curr_lmt(%d), bat_volt_lmt(%d)\n", pm_config.bat_curr_lp_lmt, pm_config.bat_volt_lp_lmt);
    /* A06 code for AL7160A-2776 by xiongxiaoliang at 20240606 end*/
    /* A06 code for AL7160A-67 | AL7160A-78 by jiashixian at 20240417 end*/

    if (ibus_limit == 0)
        ibus_limit = min(pm_config.bus_curr_lp_lmt, pdpm->apdo_max_curr);

    /* reduce bus current in cv loop */
    if (vbat_now > pm_config.bat_volt_lp_lmt - 50) {
        if (pdpm->ibus_lmt_change_timer++ > IBUS_CHANGE_TIMEOUT) {
            pdpm->ibus_lmt_change_timer = 0;
            ibus_limit = min(pm_config.bus_curr_lp_lmt, pdpm->apdo_max_curr);
        }
    } else if (vbat_now < pm_config.bat_volt_lp_lmt - 250) {
        ibus_limit = min(pm_config.bus_curr_lp_lmt, pdpm->apdo_max_curr);
        pdpm->ibus_lmt_change_timer = 0;
    } else {
        pdpm->ibus_lmt_change_timer = 0;
    }

    /* battery voltage loop*/
    /*A06 code for AL7160A-830 by jiashixian at 20240525 start*/
    if (vbat_now > pm_config.bat_volt_lp_lmt - BAT_CV_OFFSET) {
        step_vbat = -pm_config.fc2_steps;
    } else if (vbat_now < (pm_config.bat_volt_lp_lmt - (BAT_CV_OFFSET + 7))) {
        step_vbat = pm_config.fc2_steps;
    }
    /*A06 code for AL7160A-830 by jiashixian at 20240525 end*/
    /* battery charge current loop*/
    /*TODO: thermal contrl
    * bat_limit = min(pm_config.bat_curr_lp_lmt, fcc_curr);
    */
    ibat_limit = pm_config.bat_curr_lp_lmt;
    ibat_now = pdpm->sw.ibat_curr;
    if (ibat_now < ibat_limit)
        step_ibat = pm_config.fc2_steps;
    else if (pdpm->sw.ibat_curr > ibat_limit + 100)
        step_ibat = -pm_config.fc2_steps;

    /* bus current loop*/
    ibus_total = pdpm->cp.ibus_curr + pdpm->sw.ibus_curr;
   // if (pm_config.cp_sec_enable)
   //     ibus_total += pdpm->cp_sec.ibus_curr;

    if (ibus_total < ibus_limit - 100)
        step_ibus = pm_config.fc2_steps;
    else if (ibus_total > ibus_limit - 80)
        step_ibus = -pm_config.fc2_steps;

    steps = min(min(step_vbat, step_ibus), step_ibat);

    /* check if cp disabled due to other reason*/
    usbpd_pm_check_cp_enabled(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_check_cp_sec_enabled(pdpm);
    }
    if (!pdpm->cp.charge_enabled || (pm_config.cp_sec_enable &&
        !pdpm->cp_sec_stopped && !pdpm->cp_sec.charge_enabled)) {
        pr_notice("cp.charge_enabled:%d  %d  %d\n",
                pdpm->cp.charge_enabled, pdpm->cp.vbus_err_low, pdpm->cp.vbus_err_high);
        return PM_ALGO_RET_CHG_DISABLED;
    }

    /* charge pump taper charge */
    /*A06 code for AL7160A-830 by jiashixian at 20240525 start*/
    if (vbat_sw_now > pm_config.bat_volt_lp_lmt - 50
            && ibat_now < pm_config.fc2_taper_current) {
        if (pdpm->fc2_taper_timer++ > TAPER_TIMEOUT) {
            pr_notice("charge pump taper charging done\n");
            pdpm->fc2_taper_timer = 0;
            return PM_ALGO_RET_TAPER_DONE;
        }
    } else {
        pdpm->fc2_taper_timer = 0;
    }
    /*A06 code for AL7160A-830 by jiashixian at 20240525 end*/

    /*TODO: customer can add hook here to check system level
        * thermal mitigation*/

    pr_err("%s step: vbat(%d) ibat(%d) ibus(%d) all(%d)\n", __func__,
            step_vbat, step_ibat, step_ibus, steps);

    pdpm->request_voltage += steps * 20;

    if (pdpm->request_voltage > pdpm->apdo_max_volt - 1000)
        pdpm->request_voltage = pdpm->apdo_max_volt - 1000;

    return PM_ALGO_RET_OK;
}

static const unsigned char *pm_str[] = {
    "PD_PM_STATE_ENTRY",
    "PD_PM_STATE_FC2_ENTRY",
    "PD_PM_STATE_FC2_ENTRY_1",
    "PD_PM_STATE_FC2_ENTRY_2",
    "PD_PM_STATE_FC2_ENTRY_3",
    "PD_PM_STATE_FC2_TUNE",
    "PD_PM_STATE_FC2_EXIT",
};

static void usbpd_pm_move_state(struct usbpd_pm *pdpm, enum pm_state state)
{
    pr_err("state change:%s -> %s\n",
        pm_str[pdpm->state], pm_str[state]);
    pdpm->state = state;
}

static int usbpd_pm_sm(struct usbpd_pm *pdpm)
{
    int ret;
    int rc = 0;
    static int tune_vbus_retry;
    static bool stop_sw;
    static bool recover;

    pr_err("state phase :%d\n", pdpm->state);
    pr_err("cp vbus_vol %d  cp vbat_vol %d\n", pdpm->cp.vbus_volt, pdpm->cp.vbat_volt);
    /*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
    pr_err("cp m ibus_curr:%d,cp m ibat:%d, sw ibus_curr:%d, ibat_curr:%d，sw vbat_now:%d\n",
            pdpm->cp.ibus_curr,pdpm->cp.ibat_curr,
            pdpm->sw.ibus_curr,pdpm->sw.ibat_curr,pdpm->sw.vbat_volt);
    /*A06 code for AL7160A-231 by jiashixian at 20240512 end*/
    /* A06 code for AL7160A-62 | AL7160A-67 by jiashixian at 20240415 start*/
    /* A06 code for AL7160A-77 by jiashixian at 20240417 start*/
    /*A06 code for SR-AL7160A-01-258 by shixuanxuan at 20240417 start*/
    if ((g_cp_charging_lmt.cp_jeita_stop_chg) ||
      (g_cp_charging_lmt.cp_thermal_stop_chg) ||
      (g_cp_charging_lmt.cp_fac_stop_chg) ||
      (g_cp_charging_lmt.cp_ss_stop_chg)  || pdpm->sw.hv_disable_state) {
        /*A06 code for AL7160A-1449 by jiashixian at 20240522 start*/
        pr_err("cp stop charge reason:jeita_stop=%d,thermal_stop=%d,fac_stop=%d,ss_stop=%d,hv_disable=%d\n",g_cp_charging_lmt.cp_jeita_stop_chg,g_cp_charging_lmt.cp_thermal_stop_chg,g_cp_charging_lmt.cp_fac_stop_chg,g_cp_charging_lmt.cp_ss_stop_chg,pdpm->sw.hv_disable_state);
        /*A06 code AL7160A-1449 by jiashixian at 20240522 end*/
        recover = true;
        /*A06 code for AL7160A-1532 by jiashixian at 20240525 start*/
        usbpd_pm_enable_cp(pdpm, false);
        /*A06 code for AL7160A-1532 by jiashixian at 20240525 end*/
        usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
    }
    /*A06 code for SR-AL7160A-01-258 by shixuanxuan at 20240417 end*/
    switch (pdpm->state) {
    case PD_PM_STATE_ENTRY:
        stop_sw = false;
        recover = false;
        /*A06 code for AL7160A-830 by jiashixian at 20240525 start*/
        if (pdpm->sw.vbat_volt < pm_config.min_vbat_for_cp) {
            pr_notice("batt_volt-%d, waiting...\n", pdpm->sw.vbat_volt);
        } else if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 100) {
            pr_notice("CP batt_volt-%d is too high for cp,\
                    charging with switch charger\n",
                    pdpm->sw.vbat_volt);
            /*A06 code for AL7160A-297 by jiashixian at 20240506 start*/
            if(pm_config.bat_volt_lp_lmt == JEITA_TEMP_T3_TO_T4_CP_CV1) {
                recover = true;
            }
            /*A06 code for AL7160A-297 by jiashixian at 20240506 end*/
            /*A06 code for AL7160A-830 by jiashixian at 20240525 end*/
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 start*/
            usbpd_pm_enable_cp(pdpm, false);
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 end*/
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
        } else if ((pdpm->sw.bat_temp > JEITA_HIGH_TEMP_DISABLE_CP) ||
                     (pdpm->sw.bat_temp < JEITA_LOW_TEMP_DISABLE_CP) ||
                     (g_cp_charging_lmt.cp_jeita_stop_chg) ||
                     (g_cp_charging_lmt.cp_thermal_stop_chg) ||
                     (g_cp_charging_lmt.cp_fac_stop_chg)) {
            recover = true;
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 start*/
            usbpd_pm_enable_cp(pdpm, false);
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 end*/
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            pr_notice("jeita or thermal is out of range, exit cp ...\n");
    /* A06 code for AL7160A-77 by jiashixian at 20240417 end*/
    /* A06 code for AL7160A-62 | AL7160A-67 by jiashixian at 20240415 end*/
        } else {
            pr_notice("batt_volt-%d is ok, start flash charging\n",
                    pdpm->sw.vbat_volt);
            /*A06 code for AL7160A-830 by jiashixian at 20240525 start*/
            g_cp_charging_lmt.cp_chg_enable = true;
            /*A06 code for AL7160A-830 by jiashixian at 20240525 end*/
            /*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
            /*A06 code for AL7160A-231 by jiashixian at 20240512 end*/
        }
        break;

    /*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
    case PD_PM_STATE_FC2_ENTRY:
        /*if (pm_config.fc2_disable_sw) {
            if (pdpm->sw.charge_enabled) {
                usbpd_pm_enable_sw(pdpm, false);
                usbpd_pm_check_sw_enabled(pdpm);
            }
            if (!pdpm->sw.charge_enabled)
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
        } else {
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
        }*/
        break;
    /*A06 code for AL7160A-231 by jiashixian at 20240512 end*/

    case PD_PM_STATE_FC2_ENTRY_1:
        if (pm_config.cp_sec_enable) {
            pdpm->request_voltage = pdpm->sw.vbat_volt * BUS_VOLT_INIT_UP;
        } else {
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 start*/
            pdpm->request_voltage = pdpm->sw.vbat_volt * BUS_VOLT_INIT_UP + 100;
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 end*/
        }


        pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);
        usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
        pr_err("request_voltage:%d, request_current:%d\n",
                pdpm->request_voltage, pdpm->request_current);

        usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);
        tune_vbus_retry = 0;
        break;

    case PD_PM_STATE_FC2_ENTRY_2:
        pr_err("tune_vbus_retry %d\n", tune_vbus_retry);
        if (pdpm->cp.vbus_err_low || (pdpm->cp.vbus_volt <
                pdpm->sw.vbat_volt * BUS_VOLT_INIT_MIN)) {
            tune_vbus_retry++;
            pdpm->request_voltage += 20;
            usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
            pr_err("request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
        } else if (pdpm->cp.vbus_err_high || (pdpm->cp.vbus_volt >
                pdpm->sw.vbat_volt * BUS_VOLT_INIT_MAX)) {
            tune_vbus_retry++;
            pdpm->request_voltage -= 20;
            usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
            pr_err("request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
        } else {
            pr_notice("adapter volt tune ok, retry %d times\n", tune_vbus_retry);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
            break;
        }

        if (tune_vbus_retry > 150) {
            pr_notice("Failed to tune adapter volt into valid range, \
                    charge with switching charger\n");
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 start*/
            usbpd_pm_enable_cp(pdpm, false);
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 end*/
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
        }
        break;

    case PD_PM_STATE_FC2_ENTRY_3:
        usbpd_pm_check_cp_enabled(pdpm);
        if (!pdpm->cp.charge_enabled) {
            usbpd_pm_enable_cp(pdpm, true);
            msleep(100);
            usbpd_pm_check_cp_enabled(pdpm);
        }

        if (pm_config.cp_sec_enable) {
            usbpd_pm_check_cp_sec_enabled(pdpm);
            if(!pdpm->cp_sec.charge_enabled) {
                usbpd_pm_enable_cp_sec(pdpm, true);
                msleep(100);
                usbpd_pm_check_cp_sec_enabled(pdpm);
            }
        }

        if (pdpm->cp.charge_enabled) {
            if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled)
                    || !pm_config.cp_sec_enable) {
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
                pdpm->ibus_lmt_change_timer = 0;
                pdpm->fc2_taper_timer = 0;
            }
        }
        break;

    case PD_PM_STATE_FC2_TUNE:
        ret = usbpd_pm_fc2_charge_algo(pdpm);
        if (ret == PM_ALGO_RET_TAPER_DONE) {
            pr_notice("Move to switch charging:%d\n", ret);
            stop_sw = false;
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 start*/
            usbpd_pm_enable_cp(pdpm, false);
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 end*/
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            break;
        } else if (ret == PM_ALGO_RET_CHG_DISABLED) {
            pr_notice("Move to switch charging, will try to recover \
                    flash charging:%d\n", ret);
            recover = true;
            stop_sw = false;
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 start*/
            usbpd_pm_enable_cp(pdpm, false);
            /*A06 code for AL7160A-1532 by jiashixian at 20240525 end*/
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            break;
        } else {
            usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
            pr_err("request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
        }
        /*A06 code for AL7160A-67 by jiashixian at 20240415 start*/
        /*A06 code for AL7160A-830 by jiashixian at 20240525 start*/
        /*stop second charge pump if either of ibus is lower than 750ma during CV*/
            //pr_err("false666 cp_sec_enable ===:%d\n,cp_sec.charge_enabled====%d\n",pm_config.cp_sec_enable,pdpm->cp_sec.charge_enabled);

        /*if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled)
                && (pdpm->sw.vbat_volt > (pm_config.bat_volt_lp_lmt - 50))
                && (pdpm->cp.ibus_curr < 750 || pdpm->cp_sec.ibus_curr < 750)) {
            pr_notice("second cp is disabled due to ibus < 750mA\n");
            usbpd_pm_enable_cp_sec(pdpm, false);
            usbpd_pm_check_cp_sec_enabled(pdpm);
            pdpm->cp_sec_stopped = true;
        }*/
        break;
        /*A06 code for AL7160A-830 by jiashixian at 20240525 end*/
        /*A06 code for AL7160A-67 by jiashixian at 20240415 end*/
    case PD_PM_STATE_FC2_EXIT:
        /* select default 5V*/
        /*A06 code for AL7160A-830 by jiashixian at 20240525 start*/
        g_cp_charging_lmt.cp_chg_enable = false;
        /*A06 code for AL7160A-830 by jiashixian at 20240525 end*/
        /*A06 code for AL7160A-3795 by jiashixian at 20240704 start*/
        usbpd_select_pdo(pdpm, 5000, 2000); //usbpd_select_pdo(pdpm, 9000, 2000)
        /*A06 code for AL7160A-3795 by jiashixian at 20240704 start*/

        if (pdpm->cp.charge_enabled) {
            usbpd_pm_enable_cp(pdpm, false);
            usbpd_pm_check_cp_enabled(pdpm);
        }

        if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) {
            usbpd_pm_enable_cp_sec(pdpm, false);
            usbpd_pm_check_cp_sec_enabled(pdpm);
        }
        /*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
        /*pr_err(">>>sw state %d   %d\n", stop_sw, pdpm->sw.charge_enabled);
        if (stop_sw && pdpm->sw.charge_enabled)
            usbpd_pm_enable_sw(pdpm, false);
        else if (!stop_sw && !pdpm->sw.charge_enabled)
            usbpd_pm_enable_sw(pdpm, true);
        usbpd_pm_check_sw_enabled(pdpm);*/
        /*A06 code for AL7160A-231 by jiashixian at 20240512 end*/
        if (recover)
            usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
        else
            rc = 1;

        break;
    }

    return rc;
}

static void usbpd_pm_workfunc(struct work_struct *work)
{
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pm_work.work);
    int internal = PM_WORK_RUN_INTERVAL;
    /*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
    //usbpd_pm_update_sw_status(pdpm);
    //usbpd_update_sw_ibus_curr(pdpm);
    /*A06 code for AL7160A-231 by jiashixian at 20240512 end*/
    usbpd_pm_update_cp_status(pdpm);
    usbpd_pm_update_cp_sec_status(pdpm);
    usbpd_update_ibat_curr(pdpm);

    if (!usbpd_pm_sm(pdpm) && pdpm->pd_active)
        schedule_delayed_work(&pdpm->pm_work,
            msecs_to_jiffies(internal));
}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
    usbpd_pm_enable_cp(pdpm, false);
    usbpd_pm_check_cp_enabled(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_enable_cp_sec(pdpm, false);
        usbpd_pm_check_cp_sec_enabled(pdpm);
    }
    cancel_delayed_work_sync(&pdpm->pm_work);

    /*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
    /*if (!pdpm->sw.charge_enabled) {
        usbpd_pm_enable_sw(pdpm, true);
        usbpd_pm_check_sw_enabled(pdpm);
    }*/
    /*A06 code for AL7160A-231 by jiashixian at 20240512 end*/

    pdpm->pps_supported = false;
    pdpm->apdo_selected_pdo = 0;
    /*A06 code for AL7160A-830 by jiashixian at 20240525 start*/
    g_cp_charging_lmt.cp_chg_enable = false;
    /*A06 code for AL7160A-830 by jiashixian at 20240525 end*/

    usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
}

static void usbpd_pd_contact(struct usbpd_pm *pdpm, bool connected)
{
    pdpm->pd_active = connected;
    pr_err("pd_active %d\n", pdpm->pd_active);
    if (connected) {
        /* A06 code for AL7160A-684 by jiashixian at 20240507 begin */
        msleep(50);//default 30ms
        /* A06 code for AL7160A-684 by jiashixian at 20240507 begin */
        usbpd_pm_evaluate_src_caps(pdpm);
        pr_err("start cp charging pps support %d\n",
            pdpm->pps_supported);
        if (pdpm->pps_supported)
            schedule_delayed_work(&pdpm->pm_work, 0);
    } else {
        usbpd_pm_disconnect(pdpm);
    }
}

static void cp_psy_change_work(struct work_struct *work)
{
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
                    cp_psy_change_work);

    pr_err("cp change work\n");
    pdpm->psy_change_running = false;
}

static void usb_psy_change_work(struct work_struct *work)
{
    int ret;
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
                    usb_psy_change_work);
    union power_supply_propval val = {0,};

    pr_err("usb change work\n");

    if (check_typec_attached_snk(pdpm->tcpc) < 0) {
        if (pdpm->pd_active) {
            usbpd_pd_contact(pdpm, false);
        }
        goto out;
    }

    ret = power_supply_get_property(pdpm->usb_psy,
            POWER_SUPPLY_PROP_ONLINE, &val);
    if (ret) {
        pr_err("Failed to get usb pd active state\n");
        goto out;
    }

    if (!val.intval) {
            msleep(300);
            ret = power_supply_get_property(pdpm->usb_psy,
                POWER_SUPPLY_PROP_ONLINE, &val);
        if (ret) {
            pr_err("Failed to get usb pd active state\n");
            goto out;
        }
    }
    pr_err("pd_active %d,  val.intval %d\n",
            pdpm->pd_active, val.intval);

    if (!pdpm->pd_active && val.intval) {
        usbpd_pd_contact(pdpm, true);
    } else if (pdpm->pd_active && !val.intval) {
        usbpd_pd_contact(pdpm, false);
    }
out:
    pdpm->psy_change_running = false;
}

static int usbpd_psy_notifier_cb(struct notifier_block *nb,
            unsigned long event, void *data)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
    struct power_supply *psy = data;
    unsigned long flags;

    if (event != PSY_EVENT_PROP_CHANGED)
        return NOTIFY_OK;

    usbpd_check_cp_psy(pdpm);
    usbpd_check_usb_psy(pdpm);
    usbpd_check_tcpc(pdpm);

    if (!pdpm->cp_psy || !pdpm->usb_psy)
        return NOTIFY_OK;

    if (psy == pdpm->cp_psy || psy == pdpm->usb_psy) {
        spin_lock_irqsave(&pdpm->psy_change_lock, flags);
        pr_err("pdpm->psy_change_running : %d\n", pdpm->psy_change_running);
        if (!pdpm->psy_change_running) {
            pdpm->psy_change_running = true;
            if (psy == pdpm->cp_psy)
                schedule_work(&pdpm->cp_psy_change_work);
            else
                schedule_work(&pdpm->usb_psy_change_work);
        }
        spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);
    }

    return NOTIFY_OK;
}

static int pd_policy_parse_dt(struct usbpd_pm *pdpm)
{
    struct device_node *node = pdpm->dev->of_node;
    int rc = 0;
    u32 val = 0;

    if (!node) {
        pr_err("device tree node missing\n");
        return -ENOENT;
    }

    rc = of_property_read_u32(node, "pd-bat-volt-max", &val);
    if (rc < 0) {
        pr_err("pd-bat-volt-max property missing, use default val\n");
    } else {
        pm_config.bat_volt_lp_lmt = val;
    }

    rc = of_property_read_u32(node, "pd-bat-curr-max", &val);
    if (rc < 0) {
        pr_err("pd-bat-curr-max property missing, use default val\n");
    } else {
        pm_config.bat_curr_lp_lmt = val;
    }

    rc = of_property_read_u32(node, "pd-bus-volt-max", &val);
    if (rc < 0) {
        pr_err("pd-bus-volt-max property missing, use default val\n");
    } else {
        pm_config.bus_volt_lp_lmt = val;
    }

    rc = of_property_read_u32(node, "pd-bus-curr-max", &val);
    if (rc < 0) {
        pr_err("pd-bus-curr-max property missing, use default val\n");
    } else {
        pm_config.bus_curr_lp_lmt = val;
    }

    rc = of_property_read_u32(node, "pd-fc2-taper-curr", &val);
    if (rc < 0) {
        pr_err("pd-fc2-taper-curr property missing, use default val\n");
    } else {
        pm_config.fc2_taper_current = val;
    }

    rc = of_property_read_u32(node, "pd-fc2-one-step", &val);
    if (rc < 0) {
        pr_err("pd-fc2-one-step property missing, use default val\n");
    } else {
        pm_config.fc2_steps = val;
    }

    rc = of_property_read_u32(node, "pd-adapter-min-vol-required", &val);
    if (rc < 0) {
        pr_err("pd-adapter-min-vol-required property missing, use default val\n");
    } else {
        pm_config.min_adapter_volt_required = val;
    }

    rc = of_property_read_u32(node, "pd-adapter-min-curr-required", &val);
    if (rc < 0) {
        pr_err("pd-adapter-min-curr-required property missing, use default val\n");
    } else {
        pm_config.min_adapter_curr_required = val;
    }

    rc = of_property_read_u32(node, "pd-fc2-min-vbat-for-cp", &val);
    if (rc < 0) {
        pr_err("pd-fc2-min-vbat-for-cp property missing, use default val\n");
    } else {
        pm_config.min_vbat_for_cp = val;
    }
    /* A06 code for AL7160A-67 by jiashixian at 20240415 start*/
    pm_config.cp_sec_enable = false;
    //of_property_read_bool(node,
    //"pd-fc2-second-cp-enable");
    /* A06 code for AL7160A-67 by jiashixian at 20240415 end*/

    pm_config.fc2_disable_sw = of_property_read_bool(node,
            "pd-fc2-disable-sw");

    return 0;
}

static int usbpd_pm_probe(struct platform_device *pdev)
{
    struct usbpd_pm *pdpm;
    int ret = 0;

    pr_err("enter\n");

    pdpm = kzalloc(sizeof(*pdpm), GFP_KERNEL);
    if (!pdpm) {
        pr_err("kzalloc pdpm failed\n");
        return -ENOMEM;
    }

    __pdpm = pdpm;
    pdpm->dev = &pdev->dev;
    platform_set_drvdata(pdev, pdpm);

    ret = pd_policy_parse_dt(pdpm);
    if (ret < 0) {
        pr_err("Couldn't parse device tree rc=%d\n", ret);
        return ret;
    }

    spin_lock_init(&pdpm->psy_change_lock);

    usbpd_check_cp_psy(pdpm);
    usbpd_check_cp_sec_psy(pdpm);
    usbpd_check_usb_psy(pdpm);
    usbpd_check_tcpc(pdpm);
    /*A06 code for AL7160A-231 by jiashixian at 20240512 start*/
    //usbpd_check_sw_psy(pdpm);
    /*A06 code for AL7160A-231 by jiashixian at 20240512 end*/

    INIT_WORK(&pdpm->cp_psy_change_work, cp_psy_change_work);
    INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);
    INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);

    pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
    power_supply_reg_notifier(&pdpm->nb);

//    pr_err("end\n");
    return 0;
}

static int usbpd_pm_remove(struct platform_device *pdev)
{
    power_supply_unreg_notifier(&__pdpm->nb);
    cancel_delayed_work(&__pdpm->pm_work);
    cancel_work_sync(&__pdpm->cp_psy_change_work);
    cancel_work_sync(&__pdpm->usb_psy_change_work);

    return 0;
}

static const struct of_device_id usbpd_pm_of_match[] = {
    { .compatible = "unisemipower,usbpd-pm", },
    {},
};

static struct platform_driver usbpd_pm_driver = {
    .driver = {
        .name = "usbpd-pm",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(usbpd_pm_of_match),
    },
    .probe = usbpd_pm_probe,
    .remove = usbpd_pm_remove,
};

static int __init usbpd_pm_init(void)
{
    return platform_driver_register(&usbpd_pm_driver);;
}

static void __exit usbpd_pm_exit(void)
{
    return platform_driver_unregister(&usbpd_pm_driver);
}

module_init(usbpd_pm_init);
module_exit(usbpd_pm_exit);

MODULE_LICENSE("GPL v2");
