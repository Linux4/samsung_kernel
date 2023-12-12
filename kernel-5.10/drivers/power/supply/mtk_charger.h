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
#include <linux/power_supply.h>
#include "mtk_smartcharging.h"
/*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 start*/
#include <linux/power/gxy_psy_sysfs.h>
/*Tab A9 code for SR-AX6739A-01-482 by qiaodan at 20230516 end*/

#define CHARGING_INTERVAL 10
#define CHARGING_FULL_INTERVAL 20

#define CHRLOG_ERROR_LEVEL	1
#define CHRLOG_INFO_LEVEL	2
#define CHRLOG_DEBUG_LEVEL	3

#define SC_TAG "smartcharging"

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
struct charger_data;
#define BATTERY_CV 4350000
#define V_CHARGER_MAX 6500000 /* 6.5 V */
#define V_CHARGER_MIN 4600000 /* 4.6 V */
/*Tab A9 code for SR-AX6739A-01-514 by qiaodan at 20230601 start*/
#if defined(CONFIG_CUSTOM_PROJECT_OT11)
	#define V_FAST_CHARGER_MAX 10900000 /* 10.9 V */
#endif //CONFIG_CUSTOM_PROJECT_OT11
/*Tab A9 code for SR-AX6739A-01-514 by qiaodan at 20230601 end*/

#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		3200000
#define NON_STD_AC_CHARGER_CURRENT		500000
#define CHARGING_HOST_CHARGER_CURRENT		650000

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

/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
#define MAX_CV_ENTRIES	8
#define MAX_CYCLE_COUNT	0xFFFF
#define is_between(left, right, value) \
	(((left) >= (right) && (left) >= (value) && \
	(value) >= (right)) || \
	((left) <= (right) && (left) <= (value) && \
	(value) <= (right)))

struct range_data {
	u32 low_threshold;
	u32 high_threshold;
	u32 value;
};
#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 start*/

/*Tab A9 code for SR-AX6739A-01-505 by lina at 20230510 start*/
/*D85 setting */
#ifdef CONFIG_ODM_CUSTOM_D85_BUILD
#define D85_BATTERY_CV 4000000
#define D85_JEITA_TEMP_CV 4000000
#endif
/*Tab A9 code for SR-AX6739A-01-505 by lina at 20230510 end*/

/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 start*/
#if defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	#define BATT_CAP_CONTROL_CAPACITY 80
#endif //CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 end*/

/*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 start*/
/*definition for batt_full_capacity*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	#define BATT_FULL_CAPACITY_RECHG_VALUE 2
	#define BATT_FULL_CAPACITY_NORMAL_VALUE 100
#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 end*/


/*Tab A9 code for SR-AX6739A-01-510 by qiaodan at 20230608 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	#define GXY_SAFETY_TIMER_THRESHOD 259200000// 3days=259200s
	#define GXY_SAFETY_CAPACTIY_HIGH  75
	#define GXY_SAFETY_CAPACTIY_LOW  55
	#define GXY_SAFETY_TIMER_FLAG_NORMAL 0
	#define GXY_SAFETY_TIMER_FLAG_IBAT 1
	#define GXY_SAFETY_TIMER_FLAG_IBUS 2
#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-510 by qiaodan at 20230608 end*/

/*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 start*/
#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	#define STORE_MODE_CAPACITY_MAX 70
	#define STORE_MODE_CAPACITY_MIN 60
	#define STORE_MODE_FCC_MAX 500000 /*uA*/
#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 end*/

/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
/* Discharge controller Info*/
#define GXY_NORMAL_CHARGING 0
#define GXY_DISCHARGE_INPUT_SUSPEND (1 << 0)
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 start*/
#define GXY_DISCHARGE_BATT_CAP_CONTROL (1 << 1)
/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 end*/
/*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 start*/
#define GXY_DISCHARGE_BATT_FULL_CAPACITY (1 << 2)
/*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 end*/
/*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 start*/
#define GXY_DISCHARGE_STORE_MODE (1 << 3)
/*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 end*/
/*Tab A9 code for SR-AX6739A-01-510 by qiaodan at 20230608 start*/
#define GXY_DISCHARGE_SAFETY_TIMER (1 << 4)
/*Tab A9 code for SR-AX6739A-01-510 by qiaodan at 20230608 end*/
/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
#define GXY_SHIP_CYCLE 3
/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/

/*Tab A9 code for SR-AX6739A-01-263 by zhawei at 20230522 start*/
#define TP_USB_PLUGOUT_STATE      0
#define TP_USB_PLUGIN_STATE       1
/*Tab A9 code for SR-AX6739A-01-263 by zhawei at 20230522 end*/
/*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 start*/
#ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
enum batt_misc_event_ss {
	BATT_MISC_EVENT_NONE = 0x000000,			/* Default value, nothing will be happen */
	BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE = 0x00000001,	/* water detection - not support */
	BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE = 0x00000004,		/* DCD timeout */
	BATT_MISC_EVENT_HICCUP_TYPE = 0x00000020,		/* not use, it happens when water is detected in a interface port */
	BATT_MISC_EVENT_FULL_CAPACITY = 0x01000000,
};
#endif//!CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 end*/
/*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 start*/
#ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
enum batt_current_event_ss {
	SEC_BAT_CURRENT_EVENT_NONE = 0x00000,               /* Default value, nothing will be happen */
	SEC_BAT_CURRENT_EVENT_AFC = 0x00001,                /* Do not use this*/
	SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE = 0x00002,     /* This event is for a head mount VR, It is not need */
	SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING = 0x00010,  /* battery temperature is lower than 18 celsius degree */
	SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING = 0x00020, /* battery temperature is higher than 41 celsius degree */
	SEC_BAT_CURRENT_EVENT_USB_100MA = 0x00040,          /* USB 2.0 device under test is set in unconfigured state, not enumerate */
	SEC_BAT_CURRENT_EVENT_SLATE = 0x00800,
};
#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
/*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 end*/
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
/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230508 start*/
#if defined(CONFIG_CUSTOM_PROJECT_OT11)
#define JEITA_TEMP_ABOVE_T4_CUR  0
#define JEITA_TEMP_T3_TO_T4_CUR  1750000
#define JEITA_TEMP_T2_TO_T3_CUR  2700000
#define JEITA_TEMP_T1_TO_T2_CUR  1500000
#define JEITA_TEMP_T0_TO_T1_CUR  500000
#define JEITA_TEMP_BELOW_T0_CUR  0
#endif
/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230508 end*/
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

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
	bool charging;
	bool error_recovery_flag;
};

struct mtk_charger_algorithm {

	int (*do_algorithm)(struct mtk_charger *info);
	int (*enable_charging)(struct mtk_charger *info, bool en);
	/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
	int (*enable_port_charging)(struct mtk_charger *info, bool en);
	/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
	int (*do_event)(struct notifier_block *nb, unsigned long ev, void *v);
	int (*do_dvchg1_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_dvchg2_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*change_current_setting)(struct mtk_charger *info);
	void *algo_data;
};

struct charger_custom_data {
	int battery_cv;	/* uv */
	int max_charger_voltage;
	/*Tab A9 code for SR-AX6739A-01-514 by qiaodan at 20230601 start*/
	#if defined(CONFIG_CUSTOM_PROJECT_OT11)
	int max_fast_charger_voltage;
	#endif //CONFIG_CUSTOM_PROJECT_OT11
	/*Tab A9 code for SR-AX6739A-01-514 by qiaodan at 20230601 end*/
	int max_charger_voltage_setting;
	int min_charger_voltage;

	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int charging_host_charger_current;

	/* sw jeita */
	int jeita_temp_above_t4_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1_to_t2_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_below_t0_cv;
	/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230508 start*/
	#if defined(CONFIG_CUSTOM_PROJECT_OT11)
	int jeita_temp_above_t4_cur;
	int jeita_temp_t3_to_t4_cur;
	int jeita_temp_t2_to_t3_cur;
	int jeita_temp_t1_to_t2_cur;
	int jeita_temp_t0_to_t1_cur;
	int jeita_temp_below_t0_cur;
	#endif
	/*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230508 end*/
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

	/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 start*/
	#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	bool gxy_batt_aging_enable;
	struct range_data batt_cv_data[MAX_CV_ENTRIES];
	#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
	/*Tab A9 code for SR-AX6739A-01-522 by gaozhengwei at 20230607 end*/
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
	DVCHG1_SETTING,
	DVCHG2_SETTING,
	CHGS_SETTING_MAX,
};
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230514 start*/
struct gxy_charger_interface {
	void (*wake_up_charger)(struct mtk_charger *info);
	/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
	int (*set_ship_mode)(struct mtk_charger *info, bool en);
	int (*get_ship_mode)(struct mtk_charger *info);
	/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/
	/*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 start*/
	int (*dump_charger_ic)(struct mtk_charger *info);
	/*Tab A9 code for SR-AX6739A-01-506 by hualei at 20230519 end*/
	/*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 start*/
	#ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
	int (*batt_misc_event)(struct mtk_charger *info);
	#endif//!CONFIG_ODM_CUSTOM_FACTORY_BUILD
	/*Tab A9 code for SR-AX6739A-01-530 by lina at 20230530 end*/
	/*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 start*/
	#ifndef CONFIG_ODM_CUSTOM_FACTORY_BUILD
	int (*batt_current_event)(struct mtk_charger *info);
	#endif//!CONFIG_ODM_CUSTOM_FACTORY_BUILD
	/*Tab A9 code for SR-AX6739A-01-453 by lina at 20230601 end*/
};
/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230514 end*/

struct mtk_charger {
	struct platform_device *pdev;
	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_device *chg2_dev;
	struct charger_device *dvchg1_dev;
	struct notifier_block dvchg1_nb;
	struct charger_device *dvchg2_dev;
	struct notifier_block dvchg2_nb;

	struct charger_data chg_data[CHGS_SETTING_MAX];
	struct chg_limit_setting setting;
	enum charger_configuration config;

	struct power_supply_desc psy_desc1;
	struct power_supply_config psy_cfg1;
	struct power_supply *psy1;

	struct power_supply_desc psy_desc2;
	struct power_supply_config psy_cfg2;
	struct power_supply *psy2;

	struct power_supply_desc psy_dvchg_desc1;
	struct power_supply_config psy_dvchg_cfg1;
	struct power_supply *psy_dvchg1;

	struct power_supply_desc psy_dvchg_desc2;
	struct power_supply_config psy_dvchg_cfg2;
	struct power_supply *psy_dvchg2;

	/*Tab A9 code for AX6739A-12 by wenyaqi at 20230421 start*/
	struct power_supply_desc psy_pmic_desc;
	struct power_supply_config psy_pmic_cfg;
	struct power_supply *psy_pmic;
	struct iio_channel *chan_vbus;
	/*Tab A9 code for AX6739A-12 by wenyaqi at 20230421 end*/

	/*Tab A9 code for SR-AX6739A-01-488 by qiaodan at 20230503 start*/
	struct power_supply_desc psy_main1_desc;
	struct power_supply_config psy_main1_cfg;
	struct power_supply *psy_main1;

	struct power_supply_desc psy_main2_desc;
	struct power_supply_config psy_main2_cfg;
	struct power_supply *psy_main2;
	/*Tab A9 code for SR-AX6739A-01-488 by qiaodan at 20230503 end*/

	/*Tab A9 code for SR-AX6739A-01-473 by hualei at 20230525 start*/
	struct power_supply_desc psy_main3_desc;
	struct power_supply_config psy_main3_cfg;
	struct power_supply *psy_main3;
	/*Tab A9 code for SR-AX6739A-01-473 by hualei at 20230525 end*/

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
	int usb_type;
	int usb_state;

	/*Tab A9 code for SR-AX6739A-01-488 by qiaodan at 20230503 start*/
	int ac_online;
	int usb_online;
	/*Tab A9 code for SR-AX6739A-01-488 by qiaodan at 20230503 end*/

	/*Tab A9 code for SR-AX6739A-01-473 by hualei at 20230525 start*/
	int otg_online;
	/*Tab A9 code for SR-AX6739A-01-473 by hualei at 20230525 end*/

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
	struct timespec64 endtime;
	bool is_suspend;
	struct notifier_block pm_notifier;
	ktime_t timer_cb_duration[8];

	/* notify charger user */
	struct srcu_notifier_head evt_nh;

	/* common info */
	int log_level;
	bool usb_unlimited;
	bool charger_unlimited;
	bool disable_charger;
	bool disable_aicl;
	int battery_temp;
	bool can_charging;
	bool cmd_discharging;
	bool safety_timeout;
	int safety_timer_cmd;
	bool vbusov_stat;
	bool is_chg_done;
	/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 start*/
	bool port_can_charging;
	int gxy_discharge_info;
	int gxy_discharge_input_suspend;
	/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230512 end*/
	/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 start*/
	int gxy_shipmode_flag;
	/*Tab A9 code for SR-AX6739A-01-523 by hualei at 20230516 end*/
	/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 start*/
	#if defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	bool gxy_discharge_batt_cap_control;
	#endif //CONFIG_ODM_CUSTOM_FACTORY_BUILD
	/*Tab A9 code for SR-AX6739A-01-487 by qiaodan at 20230515 end*/
	/*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 start*/
	#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	int gxy_discharge_batt_full_capacity;
	int gxy_discharge_batt_full_flag;
	#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
	/*Tab A9 code for SR-AX6739A-01-457 by qiaodan at 20230522 end*/
	/*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 start*/
	#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	bool gxy_discharge_store_mode;
	bool gxy_discharge_store_mode_flag;
	int store_mode_charging_max;
	int store_mode_charging_min;
	#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
	/*Tab A9 code for SR-AX6739A-01-455 by qiaodan at 20230524 end*/

	/*Tab A9 code for SR-AX6739A-01-514 by qiaodan at 20230601 start*/
	#if defined(CONFIG_CUSTOM_PROJECT_OT11)
	bool hv_voltage_switch1;
	bool ovp_status;
	#endif //CONFIG_CUSTOM_PROJECT_OT11
	/*Tab A9 code for SR-AX6739A-01-514 by qiaodan at 20230601 end*/

	/*Tab A9 code for SR-AX6739A-01-510 by qiaodan at 20230608 start*/
	#if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
	int gxy_safety_timer_flag;
	#endif //!CONFIG_ODM_CUSTOM_FACTORY_BUILD
	/*Tab A9 code for SR-AX6739A-01-510 by qiaodan at 20230608 end*/

	/* ATM */
	bool atm_enabled;

	const char *algorithm_name;
	struct mtk_charger_algorithm algo;

	/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230514 start*/
	struct gxy_charger_interface gxy_cint;
	/*Tab A9 code for SR-AX6739A-01-486 by qiaodan at 20230514 end*/

	/* dtsi custom data */
	struct charger_custom_data data;

	/* battery warning */
	unsigned int notify_code;
	unsigned int notify_test_mode;

	/* sw safety timer */
	bool enable_sw_safety_timer;
	bool sw_safety_timer_setting;
	struct timespec64 charging_begin_time;

	/* vbat monitor, 6pin bat */
	bool batpro_done;
	bool enable_vbat_mon;
	bool enable_vbat_mon_bak;
	int old_cv;
	bool stop_6pin_re_en;
	int vbat0_flag;

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

	/* fast charging algo support indicator */
	bool enable_fast_charging_indicator;
	unsigned int fast_charging_indicator;

	/* diasable meta current limit for testing */
	unsigned int enable_meta_current_limit;

	struct smartcharging sc;

	/*daemon related*/
	struct sock *daemo_nl_sk;
	u_int g_scd_pid;
	struct scd_cmd_param_t_1 sc_data;

	/*charger IC charging status*/
	bool is_charging;

	ktime_t uevent_time_check;

	bool force_disable_pp[CHG2_SETTING + 1];
	bool enable_pp[CHG2_SETTING + 1];
	struct mutex pp_lock[CHG2_SETTING + 1];
};

static inline int mtk_chg_alg_notify_call(struct mtk_charger *info,
					  enum chg_alg_notifier_events evt,
					  int value)
{
	int i;
	struct chg_alg_notify notify = {
		.evt = evt,
		.value = value,
	};

	for (i = 0; i < MAX_ALG_NO; i++) {
		if (info->alg[i])
			chg_alg_notifier_call(info->alg[i], &notify);
	}
	return 0;
}

/* functions which framework needs*/
extern int mtk_basic_charger_init(struct mtk_charger *info);
extern int mtk_pulse_charger_init(struct mtk_charger *info);
extern int get_uisoc(struct mtk_charger *info);
extern int get_battery_voltage(struct mtk_charger *info);
extern int get_battery_temperature(struct mtk_charger *info);
extern int get_battery_current(struct mtk_charger *info);
extern int get_vbus(struct mtk_charger *info);
extern int get_ibat(struct mtk_charger *info);
extern int get_ibus(struct mtk_charger *info);
extern bool is_battery_exist(struct mtk_charger *info);
extern int get_charger_type(struct mtk_charger *info);
extern int get_usb_type(struct mtk_charger *info);
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
/*Tab A9 code for SR-AX6739A-01-263 by zhawei at 20230522 start*/
extern int g_tp_detect_usb_flag;
extern int tp_usb_notifier_register(struct notifier_block *nb);
extern int tp_usb_notifier_unregister(struct notifier_block *nb);
/*Tab A9 code for SR-AX6739A-01-263 by zhawei at 20230522 end*/


#endif /* __MTK_CHARGER_H */
