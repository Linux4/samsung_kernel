/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CHARGER_H
#define __MTK_CHARGER_H

#include <linux/alarmtimer.h>
#include "charger_class.h"
#include "adapter_class.h"
#include "mtk_charger_algorithm_class.h"
#if defined (CONFIG_N26_CHARGER_PRIVATE)
#include "wingtech_charger.h"
#endif

#define CHARGING_INTERVAL 10
#define CHARGING_FULL_INTERVAL 20

#define CHRLOG_ERROR_LEVEL	1
#define CHRLOG_INFO_LEVEL	2
#define CHRLOG_DEBUG_LEVEL	3

extern int chr_get_debug_level(void);

#define chr_err(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define chr_info(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_INFO_LEVEL) {	\
		pr_notice_ratelimited(fmt, ##args);		\
	}							\
} while (0)

#define chr_debug(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

struct mtk_charger;
#define BATTERY_CV 4350000
#define V_CHARGER_MAX 6500000 /* 6.5 V */
#define V_CHARGER_MIN 4600000 /* 4.6 V */
#if defined (CONFIG_N26_CHARGER_PRIVATE) 
#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2000000
#define AC_CHARGER_INPUT_CURRENT		2000000
#define FAST_CHARGER_CURRENT			3000000
#define FAST_CHARGER_INPUT_CURRENT		1670000
#define NON_STD_AC_CHARGER_CURRENT		500000
#define CHARGING_HOST_CHARGER_CURRENT		650000
#define LCMON_CHARGING_CURRENT		700000
#define LCMON_CHARGING_LIMIT_CURRENT		500000
#elif defined (CONFIG_N23_CHARGER_PRIVATE)
#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	100000 /* 100mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		3200000
#define NON_STD_AC_CHARGER_CURRENT		500000
#define CHARGING_HOST_CHARGER_CURRENT		650000
#else
#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		3200000
#define NON_STD_AC_CHARGER_CURRENT		500000
#define CHARGING_HOST_CHARGER_CURRENT		650000
#endif
/* dynamic mivr */
#define V_CHARGER_MIN_1 4400000 /* 4.4 V */
#define V_CHARGER_MIN_2 4200000 /* 4.2 V */
#define MAX_DMIVR_CHARGER_CURRENT 1800000 /* 1.8 A */

/* battery warning */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP

/* charging abnormal status */
#define CHG_VBUS_OV_STATUS	(1 << 0)
#define CHG_BAT_OT_STATUS	(1 << 1)
#define CHG_OC_STATUS		(1 << 2)
#define CHG_BAT_OV_STATUS	(1 << 3)
#define CHG_ST_TMO_STATUS	(1 << 4)
#define CHG_BAT_LT_STATUS	(1 << 5)
#define CHG_TYPEC_WD_STATUS	(1 << 6)

/* Battery Temperature Protection */
#define MIN_CHARGE_TEMP  0
#define MIN_CHARGE_TEMP_PLUS_X_DEGREE	6
#define MAX_CHARGE_TEMP  50
#define MAX_CHARGE_TEMP_MINUS_X_DEGREE	47

#define MAX_ALG_NO 10

enum bat_temp_state_enum {
	BAT_TEMP_LOW = 0,
	BAT_TEMP_NORMAL,
	BAT_TEMP_HIGH
};

enum chg_dev_notifier_events {
	EVENT_FULL,
	EVENT_RECHARGE,
	EVENT_DISCHARGE,
};

struct battery_thermal_protection_data {
	int sm;
	bool enable_min_charge_temp;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
};

/* sw jeita */
#define JEITA_TEMP_ABOVE_T4_CV	4240000
#define JEITA_TEMP_T3_TO_T4_CV	4240000
#define JEITA_TEMP_T2_TO_T3_CV	4340000
#define JEITA_TEMP_T1_TO_T2_CV	4240000
#define JEITA_TEMP_T0_TO_T1_CV	4040000
#define JEITA_TEMP_BELOW_T0_CV	4040000
#if defined (CONFIG_N26_CHARGER_PRIVATE) || defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
#define JEITA_TEMP_ABOVE_T4_CC	0
#define JEITA_TEMP_T3_TO_T4_CC	1400000
#define JEITA_TEMP_T2_TO_T3_CC	2000000
#define JEITA_TEMP_T1_TO_T2_CC	1200000
#define JEITA_TEMP_T0_TO_T1_CC	400000
#define JEITA_TEMP_BELOW_T0_CC	0
#define JEITA_TEMP_ABOVE_T4_FAST_CC	0
#define JEITA_TEMP_T3_TO_T4_FAST_CC	2800000
#define JEITA_TEMP_T2_TO_T3_FAST_CC	3000000
#define JEITA_TEMP_T1_TO_T2_FAST_CC	2400000
#define JEITA_TEMP_T0_TO_T1_FAST_CC	400000
#define JEITA_TEMP_BELOW_T0_FAST_CC	0
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
#if defined (CONFIG_N26_CHARGER_PRIVATE)
#define AP_TEMP_LCMON_ABOVE_T4 300000
#define AP_TEMP_LCMON_T3_TO_T4 600000
#define AP_TEMP_LCMON_T2_TO_T3 1000000
#define AP_TEMP_LCMON_T1_TO_T2 2000000
#define AP_TEMP_LCMON_T0_TO_T1 2500000
#define AP_TEMP_LCMON_BELOW_T0 3000000

#define AP_TEMP_LCMON_T4 45
#define AP_TEMP_LCMON_T3 43
#define AP_TEMP_LCMON_T2 42
#define AP_TEMP_LCMON_T1 41
#define AP_TEMP_LCMON_T0 39

#define AP_TEMP_LCMON_T4_ANTI_SHAKE 43
#define AP_TEMP_LCMON_T3_ANTI_SHAKE 41
#define AP_TEMP_LCMON_T2_ANTI_SHAKE 40
#define AP_TEMP_LCMON_T1_ANTI_SHAKE 39
#define AP_TEMP_LCMON_T0_ANTI_SHAKE 37
#endif
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
#if defined (CONFIG_N23_CHARGER_PRIVATE)
#define AP_TEMP_ABOVE_T2_CC	500000
#define AP_TEMP_T1_TO_T2_CC	2000000
#define AP_TEMP_T0_TO_T1_CC	2500000
#define AP_TEMP_BELOW_T0_CC	3000000
#define AP_TEMP_HIGH_CC_LCMON 500000
#define AP_TEMP_LOW_CC_LCMON 700000

#define AP_TEMP_T2_THRES  45
#define AP_TEMP_T2_THRES_MINUS_X_DEGREE 43
#define AP_TEMP_T1_THRES  32
#define AP_TEMP_T1_THRES_MINUS_X_DEGREE 30
#define AP_TEMP_T0_THRES  25
#define AP_TEMP_T0_THRES_MINUS_X_DEGREE 23
#define AP_TEMP_THRES_LCMON 45
#define AP_TEMP_THRES_MINUS_X_DEGREE_LCMON 44
#elif defined (CONFIG_N21_CHARGER_PRIVATE)
#define AP_TEMP_ABOVE_T2_CC	500000
#define AP_TEMP_T1_TO_T2_CC	1500000
#define AP_TEMP_T0_TO_T1_CC	2350000
#define AP_TEMP_BELOW_T0_CC	2800000
#define AP_TEMP_HIGH_CC_LCMON 500000
#define AP_TEMP_LOW_CC_LCMON 800000

#define AP_TEMP_T2_THRES  48
#define AP_TEMP_T2_THRES_MINUS_X_DEGREE 47
#define AP_TEMP_T1_THRES  45
#define AP_TEMP_T1_THRES_MINUS_X_DEGREE 44
#define AP_TEMP_T0_THRES  40
#define AP_TEMP_T0_THRES_MINUS_X_DEGREE 39
#define AP_TEMP_THRES_LCMON 44
#define AP_TEMP_THRES_MINUS_X_DEGREE_LCMON 43
#else
#define AP_TEMP_THRES_LCMON 45
#define AP_TEMP_THRES_MINUS_X_DEGREE_LCMON 44
#endif
#endif
#define TEMP_T4_THRES  50
#define TEMP_T4_THRES_MINUS_X_DEGREE 47
#define TEMP_T3_THRES  45
#define TEMP_T3_THRES_MINUS_X_DEGREE 39
#define TEMP_T2_THRES  10
#define TEMP_T2_THRES_PLUS_X_DEGREE 16
#define TEMP_T1_THRES  0
#define TEMP_T1_THRES_PLUS_X_DEGREE 6
#define TEMP_T0_THRES  0
#define TEMP_T0_THRES_PLUS_X_DEGREE  0
#define TEMP_NEG_10_THRES 0

/*
 * Software JEITA
 * T0: -10 degree Celsius
 * T1: 0 degree Celsius
 * T2: 10 degree Celsius
 * T3: 45 degree Celsius
 * T4: 50 degree Celsius
 */
enum sw_jeita_state_enum {
	TEMP_BELOW_T0 = 0,
	TEMP_T0_TO_T1,
	TEMP_T1_TO_T2,
	TEMP_T2_TO_T3,
	TEMP_T3_TO_T4,
	TEMP_ABOVE_T4
};
enum sw_ap_state_enum {
	AP_BELOW_T0 = 0,
	AP_T0_TO_T1,
	AP_T1_TO_T2,
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
	AP_T2_TO_T3,
	AP_T3_TO_T4,
	AP_ABOVE_T4
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
};

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
	int cc;
	bool charging;
	bool error_recovery_flag;
};

struct mtk_charger_algorithm {

	int (*do_algorithm)(struct mtk_charger *info);
	int (*enable_charging)(struct mtk_charger *info, bool en);
	int (*do_event)(struct notifier_block *nb, unsigned long ev, void *v);
	int (*change_current_setting)(struct mtk_charger *info);
	void *algo_data;
};

struct charger_custom_data {
	int battery_cv;	/* uv */
	int max_charger_voltage;
	int max_charger_voltage_setting;
	int min_charger_voltage;

	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int charging_host_charger_current;
	int fast_charger_current;
	int fast_charger_input_current;

	/* sw jeita */
	int jeita_temp_above_t4_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1_to_t2_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_below_t0_cv;
	int jeita_temp_above_t4_cc;
	int jeita_temp_t3_to_t4_cc;
	int jeita_temp_t2_to_t3_cc;
	int jeita_temp_t1_to_t2_cc;
	int jeita_temp_t0_to_t1_cc;
	int jeita_temp_below_t0_cc;
	int jeita_temp_above_t4_fast_cc;
	int jeita_temp_t3_to_t4_fast_cc;
	int jeita_temp_t2_to_t3_fast_cc;
	int jeita_temp_t1_to_t2_fast_cc;
	int jeita_temp_t0_to_t1_fast_cc;
	int jeita_temp_below_t0_fast_cc;
	int ap_temp_lcmoff_above_t2_cc;
	int ap_temp_lcmoff_t1_to_t2_cc;
	int ap_temp_lcmoff_t0_to_t1_cc;
	int ap_temp_lcmoff_below_t0_cc;
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	int ap_temp_lcmon_above_t4;
	int ap_temp_lcmon_t3_to_t4;
	int ap_temp_lcmon_t2_to_t3;
	int ap_temp_lcmon_t1_to_t2;
	int ap_temp_lcmon_t0_to_t1;
	int ap_temp_lcmon_below_t0;
#else
	int ap_temp_lcmon_above_t0_cc;
	int ap_temp_lcmon_below_t0_cc;
#endif
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
	int ap_lcmoff_t2_thres;
	int ap_lcmoff_t2_thres_plus_x_degree;
	int ap_lcmoff_t1_thres;
	int ap_lcmoff_t1_thres_plus_x_degree;
	int ap_lcmoff_t0_thres;
	int ap_lcmoff_t0_thres_plus_x_degree;
//+P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	int ap_temp_lcmon_t4;
	int ap_temp_lcmon_t3;
	int ap_temp_lcmon_t2;
	int ap_temp_lcmon_t1;
	int ap_temp_lcmon_t0;

	int ap_temp_lcmon_t4_anti_shake;
	int ap_temp_lcmon_t3_anti_shake;
	int ap_temp_lcmon_t2_anti_shake;
	int ap_temp_lcmon_t1_anti_shake;
	int ap_temp_lcmon_t0_anti_shake;
#else
	int ap_lcmon_t0_thres;
	int ap_lcmon_t0_thres_plus_x_degree;
#endif
//-P240111-05098   guhan01.wt 2024031820,Modify the maximum current limit for bright screen charging
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
	int ap_temp_above_t2_cc;
	int ap_temp_t1_to_t2_cc;
	int ap_temp_t0_to_t1_cc;
	int ap_temp_below_t0_cc;
	int ap_temp_high_lcmon_cc;
	int ap_temp_low_lcmon_cc;

	int ap_temp_t2_thres;
	int ap_temp_t2_thres_minus_x_degree;
	int ap_temp_t1_thres;
	int ap_temp_t1_thres_minus_x_degree;
	int ap_temp_t0_thres;
	int ap_temp_t0_thres_minus_x_degree;
	int ap_temp_thres_lcmon;
	int ap_temp_thres_minus_x_degree_lcmon;
#endif

	int temp_t4_thres;
	int temp_t4_thres_minus_x_degree;
	int temp_t3_thres;
	int temp_t3_thres_minus_x_degree;
	int temp_t2_thres;
	int temp_t2_thres_plus_x_degree;
	int temp_t1_thres;
	int temp_t1_thres_plus_x_degree;
	int temp_t0_thres;
	int temp_t0_thres_plus_x_degree;
	int temp_neg_10_thres;

	/* battery temperature protection */
	int mtk_temperature_recharge_support;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;

	/* dynamic mivr */
	int min_charger_voltage_1;
	int min_charger_voltage_2;
	int max_dmivr_charger_current;

};

struct charger_data {
	int input_current_limit;
	int charging_current_limit;

	int force_charging_current;
	int thermal_input_current_limit;
	int thermal_charging_current_limit;
	int disable_charging_count;
	int input_current_limit_by_aicl;
	int junction_temp_min;
	int junction_temp_max;
};

enum chg_data_idx_enum {
	CHG1_SETTING,
	CHG2_SETTING,
	CHGS_SETTING_MAX,
};

struct mtk_charger {
	struct platform_device *pdev;
	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_device *chg2_dev;

	struct charger_data chg_data[CHGS_SETTING_MAX];
	struct chg_limit_setting setting;
	enum charger_configuration config;

	struct power_supply_desc psy_desc1;
	struct power_supply_config psy_cfg1;
	struct power_supply *psy1;

	struct power_supply_desc psy_desc2;
	struct power_supply_config psy_cfg2;
	struct power_supply *psy2;

	struct power_supply  *chg_psy;
	struct power_supply  *bat_psy;

	struct adapter_device *pd_adapter;
	struct notifier_block pd_nb;
	struct mutex pd_lock;
	int pd_type;
	bool pd_reset;

	u32 bootmode;
	u32 boottype;

	int chr_type;
	int usb_state;

	struct mutex cable_out_lock;
	int cable_out_cnt;

	/* system lock */
	spinlock_t slock;
	struct wakeup_source *charger_wakelock;
	struct mutex charger_lock;

	/* thread related */
	wait_queue_head_t  wait_que;
	bool charger_thread_timeout;
	unsigned int polling_interval;
	bool charger_thread_polling;

	/* alarm timer */
	struct alarm charger_timer;
	struct timespec endtime;
	bool is_suspend;
	struct notifier_block pm_notifier;

	/* notify charger user */
	struct srcu_notifier_head evt_nh;

	/* common info */
	int log_level;
	bool usb_unlimited;
	bool disable_charger;
	int battery_temp;
	bool can_charging;
	bool cmd_discharging;
	bool safety_timeout;
	bool vbusov_stat;
	bool is_chg_done;
	/* ATM */
	bool atm_enabled;

	const char *algorithm_name;
	struct mtk_charger_algorithm algo;

	/* dtsi custom data */
	struct charger_custom_data data;

	/* battery warning */
	unsigned int notify_code;
	unsigned int notify_test_mode;

	/* sw safety timer */
	bool enable_sw_safety_timer;
	bool sw_safety_timer_setting;
	struct timespec charging_begin_time;
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
	int ap_temp;
	bool lcmoff;
	struct sw_jeita_data ap_thermal_lcmoff;
	struct sw_jeita_data ap_thermal_lcmon;
#endif
	/* sw jeita */
	bool enable_sw_jeita;
	struct sw_jeita_data sw_jeita;

	/* battery thermal protection */
	struct battery_thermal_protection_data thermal;

	struct chg_alg_device *alg[MAX_ALG_NO];
	struct notifier_block chg_alg_nb;
	bool enable_hv_charging;

	/* water detection */
	bool water_detected;

	bool enable_dynamic_mivr;
	struct wtchg_info *wtchg_info;
};

/* functions which framework needs*/
extern int mtk_basic_charger_init(struct mtk_charger *info);
extern int mtk_pulse_charger_init(struct mtk_charger *info);
extern int get_uisoc(struct mtk_charger *info);
extern int get_battery_voltage(struct mtk_charger *info);
extern int get_battery_temperature(struct mtk_charger *info);
extern int get_battery_current(struct mtk_charger *info);
extern int get_vbus(struct mtk_charger *info);
extern int get_ibus(struct mtk_charger *info);
extern bool is_battery_exist(struct mtk_charger *info);
extern int get_charger_type(struct mtk_charger *info);
extern int disable_hw_ovp(struct mtk_charger *info, int en);
extern bool is_charger_exist(struct mtk_charger *info);
extern int get_charger_temperature(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_charging_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_input_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_zcv(struct mtk_charger *info,
	struct charger_device *chg);
extern void _wake_up_charger(struct mtk_charger *info);

/* functions for other */
extern int mtk_chg_enable_vbus_ovp(bool enable);
#if defined (CONFIG_N23_CHARGER_PRIVATE) || defined (CONFIG_N21_CHARGER_PRIVATE)
extern int charger_manager_disable_charging_new(
	struct mtk_charger *info, bool en);
#endif

extern bool fast_charger_connect(struct mtk_charger *info);//Bug 682591,wangmingyuan.wt,ADD,20210814,hv charger status

#endif /* __MTK_CHARGER_H */
