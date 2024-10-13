/* M55 code for SR-QN6887A-01-596 by shixuanuxan at 20230913 start */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __GPIO_AFC_INTF_H
#define __GPIO_AFC_INTF_H

#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
/*M55 code for P230918-03913 by suyurui at 20230923 start*/
#include <linux/sec_class.h>
/*M55 code for P230918-03913 by suyurui at 20230923 end*/

#define ECABLEOUT    1    /* cable out */
#define ECOMMU        2

/*AFC Parameter*/
#define AFC_RETRY_MAX 10
#define AFC_T_UI  160
#define AFC_T_DATA  (AFC_T_UI << 3)
#define AFC_T_MPING  (AFC_T_UI  <<  4)
#define AFC_T_SYNC_PULSE  (AFC_T_UI >> 2)
#define AFC_T_RESET  (AFC_T_UI * 100)
#define AFC_START_BATTERY_SOC    0
#define AFC_STOP_BATTERY_SOC    85
#define DISABLE_VBAT_THRESHOLD -1
#define UNIT_VOLTAGE 1000

/* log level */
#define AFC_ERROR_LEVEL    1
#define AFC_INFO_LEVEL     2
#define AFC_DEBUG_LEVEL    3

struct chg_limit_setting {
    int cv;
    int input_current_limit1;
    int input_current_limit2;
    int input_current_limit_dvchg1;
    int charging_current_limit1;
    int charging_current_limit2;
    bool vbat_mon_en;
};

enum notifier_events {
    EVT_ATTACH,
    EVT_DETACH,
    EVT_FULL,
    EVT_RECHARGE,
    EVT_HARDRESET,
    EVT_ALGO_STOP,
    EVT_MAX,
};

enum afc_alg_state {
    ALG_INIT_FAIL,
    ALG_TA_CHECKING,
    ALG_TA_NOT_SUPPORT,
    ALG_NOT_READY,
    ALG_READY,
    ALG_RUNNING,
    ALG_DONE,
};

enum {
    SPING_ERR_1 = 1,
    SPING_ERR_2,
    SPING_ERR_3,
    SPING_ERR_4,
};

enum {
    SET_5V = 5000,
    SET_9V = 9000,
};

enum {
    AFC_INIT = 0,
    AFC_FAIL,
    AFC_RUNING,
    AFC_5V,
    AFC_9V,
};

enum afc_state_enum {
    AFC_HW_UNINIT = 0,
    AFC_HW_FAIL,
    AFC_HW_READY,
    AFC_TA_NOT_SUPPORT,
    AFC_RUN,
    AFC_DONE
};

struct afc_charger {
    struct platform_device *pdev;
    struct mutex access_lock;
    struct wakeup_source *suspend_lock;
    struct power_supply *bat_psy;
    struct mutex cable_out_lock;
    int ta_vchr_org; /* mA */
    int state;
    bool is_cable_out_occur; /* Plug out happened while detecting AFC */

    int afc_start_battery_soc;
    int afc_stop_battery_soc;
    int afc_min_charger_voltage;
    int cv;
    int afc_switch_gpio;
    int afc_data_gpio;
    bool pin_state;
    #if IS_ENABLED(CONFIG_DRV_SAMSUNG)
    int afc_disable;
    struct device *switch_device;
    #endif // CONFIG_DRV_SAMSUNG
    spinlock_t afc_spin_lock;
};

extern void gxy_bat_set_hv_disable(int hv_disable);
extern int get_afc_result(void);
int gpio_afc_init(struct platform_device *pdev);
int afc_notifier_call(int event);
int afc_hal_check_first_attach(void);
#endif // __GPIO_AFC_INTF_H
/* M55 code for SR-QN6887A-01-596 by shixuanuxan at 20230913 end */
