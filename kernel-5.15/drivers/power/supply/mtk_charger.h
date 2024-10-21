/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CHARGER_H
#define __MTK_CHARGER_H

#include <linux/alarmtimer.h>
#include <linux/atomic.h>
#include "charger_class.h"
#include "adapter_class.h"
#include "mtk_charger_algorithm_class.h"
#include <linux/power_supply.h>
#include "mtk_smartcharging.h"
#include "afc_charger_intf.h"
#include "../../misc/mediatek/typec/tcpc/inc/tcpci_config.h"


#ifdef CONFIG_WATER_DETECTION
#undef CONFIG_WATER_DETECTION
#define CONFIG_WATER_DETECTION 0
#endif

#define CHARGING_INTERVAL 10
#define CHARGING_FULL_INTERVAL 20

#define CHRLOG_ERROR_LEVEL	1
#define CHRLOG_INFO_LEVEL	2
#define CHRLOG_DEBUG_LEVEL	3

#define SC_TAG "smartcharging"

//#define CHARGING_THERMAL_ENABLE
//liwei19.wt 20240727, adjust thermal current when adpater is 15w pd or afc
#define CHARGING_15W_THERMAL_ENABLE

#ifndef CONFIG_WT_COMPILE_FACTORY_VERSION
#define ONEUI_6P1_CHG_PROTECION_ENABLE
#endif

extern int chr_get_debug_level(void);
//+S98901AA1,zhangjian.wt,modify,2024/07/31,Optimize charging mode
extern struct blocking_notifier_head usb_tp_notifier_list;
//+S98901AA1,zhangjian.wt,modify,2024/07/31,Optimize charging mode

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
#define BATTERY_FOR_TESTING_CV 4100000
#define BATTERY_CV 4350000
#define V_CHARGER_MAX 6500000 /* 6.5 V */
#define V_CHARGER_MIN 4600000 /* 4.6 V */
#define VBUS_OVP_VOLTAGE 15000000 /* 15V */

#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		3200000
#define NON_STD_AC_CHARGER_CURRENT		500000
#define CHARGING_HOST_CHARGER_CURRENT		650000
#define FIX_PDO_CHARGER_CURRENT     2800000
#define FIX_PDO_INPUT_CURRENT       1600000
#define APDO_CHARGER_T2_TO_T3_CURRENT        5200000
#define APDO_CHARGER_T3_TO_T4_CURRENT        3800000
#define FIX_PDO_COMMON_INPUT_CURRENT       1100000

//S98901AA1-12622, liwei19@wt, add 20240822, charging_type
#define PROP_SIZE_LEN 20

/* dynamic mivr */
#define V_CHARGER_MIN_1 4400000 /* 4.4 V */
#define V_CHARGER_MIN_2 4200000 /* 4.2 V */
#define MAX_DMIVR_CHARGER_CURRENT 1800000 /* 1.8 A */

#define FAST_CHARGING_INDICATOR 16

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

//+S98901AA1-12622, liwei19@wt, add 20240822, charging_type
struct wt_charging_type {
	int charging_type;
	char *ta_type;
};
//-S98901AA1-12622, liwei19@wt, add 20240822, charging_type

/* sw jeita */
#define JEITA_TEMP_ABOVE_T4_CV	4250000
#define JEITA_TEMP_T3_TO_T4_CV	4250000
#define JEITA_TEMP_T2_TO_T3_CV	4450000
#define JEITA_TEMP_T1_TO_T2_CV	4400000
#define JEITA_TEMP_T0_TO_T1_CV	4450000
#define JEITA_TEMP_BELOW_T0_CV	4450000

#define JEITA_TEMP_ABOVE_T4_CC	0
#define JEITA_TEMP_T3_TO_T4_CC	1500000
#define JEITA_TEMP_T2_TO_T3_CC	1500000
#define JEITA_TEMP_T1_TO_T2_CC	1400000
#define JEITA_TEMP_T0_TO_T1_CC	486000
#define JEITA_TEMP_BELOW_T0_CC	0

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

#ifdef CHARGING_THERMAL_ENABLE
#define AP_TEMP_T4_THRES  43
#define AP_TEMP_T4_THRES_MINUS_X_DEGREE 42
#define AP_TEMP_T3_THRES  41
#define AP_TEMP_T3_THRES_MINUS_X_DEGREE 40
#define AP_TEMP_T2_THRES  39
#define AP_TEMP_T2_THRES_MINUS_X_DEGREE 38
#define AP_TEMP_T1_THRES  37
#define AP_TEMP_T1_THRES_MINUS_X_DEGREE 36
#define AP_TEMP_T0_THRES  35
#define AP_TEMP_T0_THRES_MINUS_X_DEGREE 34

#define AP_TEMP_LCMON_T4 45
#define AP_TEMP_LCMON_T3 43
#define AP_TEMP_LCMON_T2 41
#define AP_TEMP_LCMON_T1 39
#define AP_TEMP_LCMON_T0 37

#define AP_TEMP_LCMON_T4_ANTI_SHAKE 44
#define AP_TEMP_LCMON_T3_ANTI_SHAKE 42
#define AP_TEMP_LCMON_T2_ANTI_SHAKE 40
#define AP_TEMP_LCMON_T1_ANTI_SHAKE 38
#define AP_TEMP_LCMON_T0_ANTI_SHAKE 36
#endif

//+liwei19.wt 20240727, adjust thermal current when adpater is 15w pd or afc
#ifdef CHARGING_15W_THERMAL_ENABLE
#define THERMAL_CURRENT_LCMOFF_T4     500000
#define THERMAL_CURRENT_LCMOFF_T3     1500000
#define THERMAL_CURRENT_LCMOFF_T2     2500000
#define THERMAL_CURRENT_LCMOFF_T1     3000000
#define THERMAL_CURRENT_LCMOFF_T0     4000000

#define THERMAL_15W_CURRENT_LCMOFF_T4 1000000
#define THERMAL_15W_CURRENT_LCMOFF_T3 1500000
#define THERMAL_15W_CURRENT_LCMOFF_T2 1500000
#define THERMAL_15W_CURRENT_LCMOFF_T1 1500000
#define THERMAL_15W_CURRENT_LCMOFF_T0 2000000
#endif
//-liwei19.wt 20240727, adjust thermal current when adpater is 15w pd or afc

/* yuanjian.wt add for AFC */
/* AFC */
#define AFC_ICHG_LEAVE_THRESHOLD  1000000 /* uA */
#define AFC_START_BATTERY_SOC	  0
#define AFC_STOP_BATTERY_SOC	  85
#define AFC_PRE_INPUT_CURRENT     500000 /* uA */
#define AFC_CHARGER_INPUT_CURRENT 1670000 /* uA */
#define AFC_CHARGER_CURRENT 2800000
#define AFC_MIN_CHARGER_VOLTAGE   4200000
#define AFC_MAX_CHARGER_VOLTAGE   9000000
#define AFC_COMMON_ICL_CURR_MAX 1800000
#define CHG_AFC_COMMON_CURR_MAX 2500000

//+S98901AA1-12619, liwei19@wt, add 20240822, batt_charging_source
/*batt_charging_source*/
#define SEC_BATTERY_CABLE_UNKNOWN                0
#define SEC_BATTERY_CABLE_NONE                   1
#define SEC_BATTERY_CABLE_PREPARE_TA             2
#define SEC_BATTERY_CABLE_TA                     3
#define SEC_BATTERY_CABLE_USB                    4
#define SEC_BATTERY_CABLE_USB_CDP                5
#define SEC_BATTERY_CABLE_9V_TA                  6
#define SEC_BATTERY_CABLE_9V_ERR                 7
#define SEC_BATTERY_CABLE_9V_UNKNOWN             8
#define SEC_BATTERY_CABLE_12V_TA                 9
#define SEC_BATTERY_CABLE_WIRELESS               10
#define SEC_BATTERY_CABLE_HV_WIRELESS            11
#define SEC_BATTERY_CABLE_PMA_WIRELESS           12
#define SEC_BATTERY_CABLE_WIRELESS_PACK          13
#define SEC_BATTERY_CABLE_WIRELESS_HV_PACK       14
#define SEC_BATTERY_CABLE_WIRELESS_STAND         15
#define SEC_BATTERY_CABLE_WIRELESS_HV_STAND      16
#define SEC_BATTERY_CABLE_QC20                   17
#define SEC_BATTERY_CABLE_QC30                   18
#define SEC_BATTERY_CABLE_PDIC                   19
#define SEC_BATTERY_CABLE_UARTOFF                20
#define SEC_BATTERY_CABLE_OTG                    21
#define SEC_BATTERY_CABLE_LAN_HUB                22
#define SEC_BATTERY_CABLE_POWER_SHARING          23
#define SEC_BATTERY_CABLE_HMT_CONNECTED          24
#define SEC_BATTERY_CABLE_HMT_CHARGE             25
#define SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT        26
#define SEC_BATTERY_CABLE_WIRELESS_VEHICLE       27
#define SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE    28
#define SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV    29
#define SEC_BATTERY_CABLE_TIMEOUT                30
#define SEC_BATTERY_CABLE_SMART_OTG              31
#define SEC_BATTERY_CABLE_SMART_NOTG             32
#define SEC_BATTERY_CABLE_WIRELESS_TX            33
#define SEC_BATTERY_CABLE_HV_WIRELESS_20         34
#define SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT   35
#define SEC_BATTERY_CABLE_WIRELESS_FAKE          36
#define SEC_BATTERY_CABLE_PREPARE_WIRELESS_20    37
#define SEC_BATTERY_CABLE_PDIC_APDO              38
#define SEC_BATTERY_CABLE_POGO                   39
#define SEC_BATTERY_CABLE_POGO_9V                40
#define SEC_BATTERY_CABLE_FPDO_DC                41
#define SEC_BATTERY_CABLE_MAX                    42
//-S98901AA1-12619, liwei19@wt, add 20240822, batt_charging_source

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
	TEMP_ABOVE_T4,
	TEMP_MAX,
};

//+S98901AA1-4521, liwei19.wt, add, 20240509, add SDP/DCP/CDP/HVDCP in  88 mode
enum real_charge_type {
	REAL_TYPE_UNKNOWN = 0,
	REAL_TYPE_BATTERY,
	REAL_TYPE_UPS,
	REAL_TYPE_MAINS,
	REAL_TYPE_USB,			/* Standard Downstream Port */
	REAL_TYPE_USB_DCP,		/* Dedicated Charging Port */
	REAL_TYPE_USB_CDP,		/* Charging Downstream Port */
	REAL_TYPE_USB_ACA,		/* Accessory Charger Adapters */
	REAL_TYPE_USB_TYPE_C,		/* Type C Port */
	REAL_TYPE_USB_PD,		/* Power Delivery Port */
	REAL_TYPE_USB_PD_DRP,		/* PD Dual Role Port */
	REAL_TYPE_APPLE_BRICK_ID,	/* Apple Charging Method */
	REAL_TYPE_WIRELESS,		/* Wireless */
	REAL_TYPE_USB_HVDCP,      /* HVDCP */
	REAL_TYPE_USB_AFC,      /* AFC */
	REAL_TYPE_USB_FLOAT,      /* Float */
};
//-S98901AA1-4521, liwei19.wt, add, 20240509, add SDP/DCP/CDP/HVDCP in  88 mode

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
enum {
	POWER_SUPPLY_NO_ONEUI_CHG = 0,
	POWER_SUPPLY_CAPACITY_100,
	POWER_SUPPLY_CAPACITY_80_HIGHSOC,
	POWER_SUPPLY_CAPACITY_80_SLEEP,
	POWER_SUPPLY_CAPACITY_80_OPTION,
	POWER_SUPPLY_CAPACITY_80_OFFCHARGING,
};

static const char * const POWER_SUPPLY_BATT_FULL_CAPACITY_TEXT[] = {
	[POWER_SUPPLY_NO_ONEUI_CHG]		= "null",
	[POWER_SUPPLY_CAPACITY_100]		= "100",
	[POWER_SUPPLY_CAPACITY_80_HIGHSOC]	= "80 HIGHSOC",
	[POWER_SUPPLY_CAPACITY_80_SLEEP]	= "80 SLEEP",
	[POWER_SUPPLY_CAPACITY_80_OPTION]	= "80 OPTION",
	[POWER_SUPPLY_CAPACITY_80_OFFCHARGING]	= "80",
};
#endif

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
	int (*do_dvchg1_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_dvchg2_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_hvdvchg1_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_hvdvchg2_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*change_current_setting)(struct mtk_charger *info);
	void *algo_data;
};

struct charger_custom_data {
	int battery_cv;	/* uv */
	int max_charger_voltage;
	int max_charger_voltage_setting;
	int min_charger_voltage;
	int vbus_sw_ovp_voltage;

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
	int jeita_temp_above_t4_cc;
	int jeita_temp_t3_to_t4_cc;
	int jeita_temp_t2_to_t3_cc;
	int jeita_temp_t1_to_t2_cc;
	int jeita_temp_t0_to_t1_cc;
	int jeita_temp_below_t0_cc;
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

#ifdef CHARGING_THERMAL_ENABLE
	int ap_temp_t4_thres;
	int ap_temp_t4_thres_minus_x_degree;
	int ap_temp_t3_thres;
	int ap_temp_t3_thres_minus_x_degree;
	int ap_temp_t2_thres;
	int ap_temp_t2_thres_minus_x_degree;
	int ap_temp_t1_thres;
	int ap_temp_t1_thres_minus_x_degree;
	int ap_temp_t0_thres;
	int ap_temp_t0_thres_minus_x_degree;

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
	int lcd_on_ibat[TEMP_MAX];
	int lcd_off_ibat[TEMP_MAX];
#endif

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

	/*yuanjian.wt, start 20191123, add for AFC charge*/
	/*AFC*/
	int afc_start_battery_soc;
	int afc_stop_battery_soc;
	int afc_ichg_level_threshold;
	int afc_pre_input_current;
	int afc_charger_input_current;
	int afc_charger_current;
	int afc_common_charger_input_curr;
	int afc_common_charger_curr;
	int afc_min_charger_voltage;
	int afc_max_charger_voltage;
	/*yuanjian.wt, End 20191123, add for AFC charge*/

};

struct charger_data {
	int input_current_limit;
	int charging_current_limit;

	int force_charging_current;
	int thermal_input_current_limit;
	int thermal_charging_current_limit;
	bool thermal_throttle_record;
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
	HVDVCHG1_SETTING,
	HVDVCHG2_SETTING,
	CHGS_SETTING_MAX,
};

struct mtk_charger {
	struct platform_device *pdev;
	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_device *chg2_dev;
	struct charger_device *dvchg1_dev;
	struct notifier_block dvchg1_nb;
	struct charger_device *dvchg2_dev;
	struct notifier_block dvchg2_nb;
	struct charger_device *hvdvchg1_dev;
	struct notifier_block hvdvchg1_nb;
	struct charger_device *hvdvchg2_dev;
	struct notifier_block hvdvchg2_nb;
	struct charger_device *bkbstchg_dev;
	struct notifier_block bkbstchg_nb;

	struct charger_data chg_data[CHGS_SETTING_MAX];
	struct chg_limit_setting setting;
	enum charger_configuration config;

	struct power_supply_desc psy_desc1;
	struct power_supply_config psy_cfg1;
	struct power_supply *psy1;

	struct power_supply_desc psy_desc2;
	struct power_supply_config psy_cfg2;
	struct power_supply *psy2;

	struct power_supply_desc psy_desc3;
	struct power_supply_config psy_cfg3;
	struct power_supply *psy3;

	struct power_supply_desc psy_dvchg_desc1;
	struct power_supply_config psy_dvchg_cfg1;
	struct power_supply *psy_dvchg1;

	struct power_supply_desc psy_dvchg_desc2;
	struct power_supply_config psy_dvchg_cfg2;
	struct power_supply *psy_dvchg2;

	struct power_supply_desc psy_hvdvchg_desc1;
	struct power_supply_config psy_hvdvchg_cfg1;
	struct power_supply *psy_hvdvchg1;

	struct power_supply_desc psy_hvdvchg_desc2;
	struct power_supply_config psy_hvdvchg_cfg2;
	struct power_supply *psy_hvdvchg2;

	struct power_supply_desc ac_desc;
	struct power_supply_config ac_cfg;
	struct power_supply *ac_psy;

	struct power_supply_desc usb_desc;
	struct power_supply_config usb_cfg;
	struct power_supply *usb_psy;

	struct power_supply_desc otg_desc;
	struct power_supply_config otg_cfg;
	struct power_supply *otg_psy;

	struct power_supply  *chg_psy;
	struct power_supply  *bc12_psy;
	struct power_supply  *bat_psy;
	struct adapter_device *pd_adapter;
	struct notifier_block pd_nb;
	struct mutex pd_lock;
	int pd_type;
	bool pd_reset;
#ifdef CONFIG_AFC_CHARGER
	/*AFC*/
	bool enable_afc;
	struct afc_dev afc;
#endif

	u32 bootmode;
	u32 boottype;

	int chr_type;
	int usb_type;
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
	struct timespec64 endtime;
	bool is_suspend;
	struct notifier_block pm_notifier;

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
	struct timespec64 charging_begin_time;

	/* vbat monitor, 6pin bat */
	bool batpro_done;
	bool enable_vbat_mon;
	bool enable_vbat_mon_bak;
	int old_cv;
	bool stop_6pin_re_en;
	int vbat0_flag;

	/*yuanjian.wt, start 20191123, add for AFC charge*/
	/*AFC*/
	int afc_start_battery_soc;
	int afc_stop_battery_soc;
	int afc_ichg_level_threshold;
	int afc_pre_input_current;
	int afc_charger_input_current;
	int afc_charger_current;
	int afc_min_charger_voltage;
	int afc_max_charger_voltage;
	/*yuanjian.wt, End 20191123, add for AFC charge*/

	struct delayed_work psy_update_dwork;
	struct delayed_work late_init_work;

	/* sw jeita */
	bool enable_sw_jeita;
	struct sw_jeita_data sw_jeita;

	/* battery thermal protection */
	struct battery_thermal_protection_data thermal;

	struct chg_alg_device *alg[MAX_ALG_NO];
	int lst_rnd_alg_idx;
	bool alg_new_arbitration;
	bool alg_unchangeable;
	struct notifier_block chg_alg_nb;
	bool enable_hv_charging;

	/* water detection */
	bool water_detected;
	bool record_water_detected;

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

	bool afc_flag;
	bool pd_flag;
	int typec_cc_orientation;
	//S98901AA1-4521, liwei19.wt, add, 20240509, add SDP/DCP/CDP/HVDCP in 88 mode
	int real_charge_type;
	int shipmode;
//+liwei19.wt 20240727, adjust thermal current when adpater is 15w pd or afc
#if defined (CHARGING_THERMAL_ENABLE) || defined (CHARGING_15W_THERMAL_ENABLE)
	struct notifier_block fb_notif;
	bool lcmoff;
#endif
//+andy.wt,20240910,battery Current event and slate mode
struct notifier_block charger_nb;
//-andy.wt,20240910,battery Current event and slate mode
#if defined (CHARGING_THERMAL_ENABLE)
	int ap_temp;
	struct sw_jeita_data ap_thermal_data;
#endif
#if defined (CHARGING_15W_THERMAL_ENABLE)
	int old_thermal_charging_current_limit;
	atomic_t thermal_current_update;
#endif
//-liwei19.wt 20240727, adjust thermal current when adpater is 15w pd or afc
	bool wt_disable_thermal_debug;
	bool disable_quick_charge;
	ktime_t uevent_time_check;
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	int batt_full_capacity;
	int batt_soc_rechg;
#endif
	//P240803-01757, liwei19@wt, modify 20240807, Update notification after charging status change
	atomic_t batt_full_discharge;

//+S98901AA1-12182, liwei19@wt, add 20240820, water detect
#if CONFIG_WATER_DETECTION
	int detect_water_state;
#endif
//-S98901AA1-12182, liwei19@wt, add 20240820, water detect
	//S98901AA1-12619, liwei19@wt, add 20240822, batt_charging_source
	int batt_charging_source;

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
extern int adapter_is_support_pd_pps(struct mtk_charger *info);
extern bool batt_store_mode;


#endif /* __MTK_CHARGER_H */
