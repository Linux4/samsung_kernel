/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __GPIO_AFC_INTF_H
#define __GPIO_AFC_INTF_H

#include "mtk_charger_algorithm_class.h"

/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
#define ECABLEOUT	1	/* cable out */
/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
#define EHAL        2    /* hal operation error */

/*AFC Parameter*/
#define AFC_RETRY_MAX 10
#define AFC_T_UI  160
#define AFC_T_DATA  (AFC_T_UI << 3)
#define AFC_T_MPING  (AFC_T_UI  <<  4)
#define AFC_T_SYNC_PULSE  (AFC_T_UI >> 2)
#define AFC_T_RESET  (AFC_T_UI * 100)

#define AFC_CHARGER_INPUT_CURRENT    1700000
#define AFC_CHARGER_CURRENT 2700000
#define AFC_MIN_CHARGER_VOLTAGE 4600000
#define AFC_START_BATTERY_SOC    0
#define AFC_STOP_BATTERY_SOC    85
#define AFC_V_CHARGER_MIN 4600000 /* 4.6 V */
#define DISABLE_VBAT_THRESHOLD -1
#define UNIT_VOLTAGE 1000000

/* log level */
#define AFC_ERROR_LEVEL    1
#define AFC_INFO_LEVEL     2
#define AFC_DEBUG_LEVEL    3

extern int afc_get_debug_level(void);
#define afc_err(fmt, args...)                    \
do {                                \
    if (afc_get_debug_level() >= AFC_ERROR_LEVEL) {    \
        pr_notice(fmt, ##args);                \
    }                            \
} while (0)

#define afc_info(fmt, args...)                    \
do {                                \
    if (afc_get_debug_level() >= AFC_INFO_LEVEL) { \
        pr_notice(fmt, ##args);                \
    }                            \
} while (0)

#define afc_dbg(fmt, args...)                    \
do {                                \
    if (afc_get_debug_level() >= AFC_DEBUG_LEVEL) {    \
        pr_notice(fmt, ##args);                \
    }                            \
} while (0)

enum {
    SPING_ERR_1 = 1,
    SPING_ERR_2,
    SPING_ERR_3,
    SPING_ERR_4,
};

enum {
    SET_5V = 5000000,
    SET_9V = 9000000,
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
    struct chg_alg_device *alg;
    struct mutex access_lock;
    struct wakeup_source *suspend_lock;
    int state;
    struct power_supply *bat_psy;

    int ta_vchr_org; /* uA */
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    struct mutex cable_out_lock;
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    bool is_cable_out_occur; /* Plug out happened while detecting AFC */

    int afc_start_battery_soc;
    int afc_stop_battery_soc;
    int afc_min_charger_voltage;

    int afc_charger_input_current;
    int afc_charger_current;

    int vbat_threshold; /* For checking Ready */
    int ref_vbat; /* Vbat with cable in */
    int cv;
    int input_current_limit;
    int charging_current_limit;
    int input_current;
    int charging_current;

    int afc_switch_gpio;
    int afc_data_gpio;
    bool pin_state; /* true: active, false: suspend */
    /*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 start*/
    #if IS_ENABLED(CONFIG_DRV_SAMSUNG)
    int afc_disable;
    struct device *switch_device;
    #endif // CONFIG_DRV_SAMSUNG
    /*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 end*/
    spinlock_t afc_spin_lock;
};

extern int afc_hal_init_hardware(struct chg_alg_device *alg);
extern int afc_hal_get_vbus(struct chg_alg_device *alg);
extern int afc_hal_get_ibat(struct chg_alg_device *alg);
extern int afc_hal_get_charging_current(struct chg_alg_device *alg,
    enum chg_idx chgidx, u32 *ua);
/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
extern int afc_hal_get_hvdcp_status(struct chg_alg_device *alg);
/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
extern int afc_hal_enable_vbus_ovp(struct chg_alg_device *alg,
    bool enable);
extern int afc_hal_enable_charging(struct chg_alg_device *alg,
    bool enable);
extern int afc_hal_set_mivr(struct chg_alg_device *alg,
    enum chg_idx chgidx, int uV);
extern int afc_hal_get_uisoc(struct chg_alg_device *alg);
extern int afc_hal_get_charger_type(struct chg_alg_device *alg);
extern int afc_hal_set_cv(struct chg_alg_device *alg,
    enum chg_idx chgidx, u32 uv);
extern int afc_hal_set_charging_current(struct chg_alg_device *alg,
    enum chg_idx chgidx, u32 ua);
extern int afc_hal_set_input_current(struct chg_alg_device *alg,
    enum chg_idx chgidx, u32 ua);
extern int afc_hal_get_log_level(struct chg_alg_device *alg);
#endif // __GPIO_AFC_INTF_H
