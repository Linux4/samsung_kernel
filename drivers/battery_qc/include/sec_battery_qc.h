/*
 * sec_battery_qc.h
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

#ifndef __SEC_BATTERY_H
#define __SEC_BATTERY_H __FILE__

#include "sec_charging_common_qc.h"
#include <linux/of_gpio.h>
#include <linux/alarmtimer.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#if defined(CONFIG_BATTERY_CISD)
#include "sec_cisd_qc.h"
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif

#if defined(CONFIG_SEC_FACTORY)
extern int factory_mode;
#endif
extern char *sec_cable_type[]; 

#define SEC_BAT_CURRENT_EVENT_NONE					0x00000
#define SEC_BAT_CURRENT_EVENT_AFC					0x00001
#define SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE		0x00002
#define SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL	0x00004

#if defined(CONFIG_SEC_A60Q_PROJECT)
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING		0x00080
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_2ND			0x00010
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD			0x00008
#else
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING		0x00008
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_2ND			0x00080
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD			0x00010
#endif

#define SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING	0x00020
#if defined(CONFIG_ENABLE_100MA_CHARGING_BEFORE_USB_CONFIGURED)
#define SEC_BAT_CURRENT_EVENT_USB_100MA			0x00040
#else
#define SEC_BAT_CURRENT_EVENT_USB_100MA			0x00000
#endif

#define SEC_BAT_CURRENT_EVENT_SWELLING_MODE		(SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_2ND | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD | SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING)
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_MODE		(SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_2ND | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD)
#define SEC_BAT_CURRENT_EVENT_USB_SUPER			0x00100
#define SEC_BAT_CURRENT_EVENT_CHG_LIMIT			0x00200
#define SEC_BAT_CURRENT_EVENT_CALL			0x00400
#define SEC_BAT_CURRENT_EVENT_SLATE			0x00800
#define SEC_BAT_CURRENT_EVENT_VBAT_OVP			0x01000
#define SEC_BAT_CURRENT_EVENT_VSYS_OVP			0x02000
#define SEC_BAT_CURRENT_EVENT_WPC_VOUT_LOCK		0x04000
#define SEC_BAT_CURRENT_EVENT_AICL			0x08000
#define SEC_BAT_CURRENT_EVENT_HV_DISABLE		0x10000
#define SEC_BAT_CURRENT_EVENT_SELECT_PDO		0x20000
#define SEC_BAT_CURRENT_EVENT_FG_RESET			0x40000
#define SEC_BAT_CURRENT_EVENT_HIGH_TEMP_LIMIT		0x80000
#define SEC_BAT_CURRENT_EVENT_25W_OCP			0x100000

#if defined(CONFIG_SEC_FACTORY)             // SEC_FACTORY
#define STORE_MODE_CHARGING_MAX 80
#define STORE_MODE_CHARGING_MIN 70
#else                                       // !SEC_FACTORY, STORE MODE
#define STORE_MODE_CHARGING_MAX 70
#define STORE_MODE_CHARGING_MIN 60
#define STORE_MODE_CHARGING_MAX_VZW 35
#define STORE_MODE_CHARGING_MIN_VZW 30
#endif //(CONFIG_SEC_FACTORY)

#define BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE	0x00000001
#define BATT_MISC_EVENT_WIRELESS_BACKPACK_TYPE	0x00000002
#define BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE		0x00000004
#define BATT_MISC_EVENT_BATT_RESET_SOC			0x00000008
#define BATT_MISC_EVENT_HICCUP_TYPE				0x00000020
#define BATT_MISC_EVENT_WIRELESS_TX_STATUS		0x00000040
#define BATT_MISC_EVENT_WIRELESS_RX_CONNECT		0x00000080
#define BATT_MISC_EVENT_WIRELESS_FOD			0x00000100

#define MILLI_5V	5000
#define MILLI_9V	9000
#define MILLI_12V	12000

#define HV_CHARGER_STATUS_STANDARD1	12000 /* mW */
#define HV_CHARGER_STATUS_STANDARD2 20000 /* mW */
#define HV_CHARGER_STATUS_STANDARD3 25000 /* mW */
#define HV_CHARGER_STATUS_STANDARD4 40000 /* mW */
enum {
	NORMAL_TA,
	AFC_9V_OR_15W,
	AFC_12V_OR_20W,
	SFC_25W,
};

#define SIOP_INPUT_LIMIT_CURRENT                1200
#define SIOP_CHARGING_LIMIT_CURRENT             1800
#define SIOP_HV_INPUT_LIMIT_CURRENT		700
#define SIOP_HV_CHARGING_LIMIT_CURRENT		1800
#define SIOP_HV_12V_INPUT_LIMIT_CURRENT		535
#define SIOP_HV_12V_CHARGING_LIMIT_CURRENT	1800
#define SIOP_DC_INPUT_LIMIT_CURRENT		1000
#define SIOP_DC_CHARGING_LIMIT_CURRENT		2000

#define TEMP_HIGHLIMIT_THRESHOLD 		700
#define TEMP_HIGHLINMIT_RECOVERY 		680

struct sec_ttf_data;


struct sec_battery_info {
	struct device *dev;
	//sec_battery_platform_data_t *pdata;
	struct sec_ttf_data *ttf_d;

	/* power supply used in Android */
	struct power_supply *psy_bat;
	struct power_supply *psy_usb;
	struct power_supply *psy_bms;
	struct power_supply *psy_usb_main;
	struct power_supply *psy_slave;

#if defined(CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
#endif

	unsigned int current_event;
	unsigned int prev_current_event;
	struct mutex current_eventlock;
	unsigned int misc_event;
	struct mutex misc_eventlock;
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct mutex typec_notylock;
#endif
	unsigned int siop_level;
	unsigned int mix_temp_recovery;
	unsigned int mix_temp_batt_threshold;
	unsigned int mix_temp_die_threshold;
	bool mix_limit;

	unsigned int chg_high_temp;
	unsigned int chg_high_temp_recovery;
	unsigned int chg_charging_limit_current;
	unsigned int chg_input_limit_current;
	bool chg_limit;

#if defined(CONFIG_DIRECT_CHARGING)
	unsigned int dc_chg_high_temp;
	unsigned int dc_chg_high_temp_recovery;
	unsigned int dc_chg_input_limit_current;
	bool dc_chg_limit;
	bool check_dc_chg_lmit;
#endif

	bool pdic_ps_rdy;
	int is_fet_off; // 0: bat to sys fet on, 1: fet off
	int is_ship_mode;
	unsigned int battery_full_capacity;
	int hiccup_status;

	struct notifier_block   nb;
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct notifier_block usb_typec_nb;
#endif
	struct workqueue_struct *monitor_wqueue;
	struct wake_lock monitor_wake_lock;
	struct delayed_work monitor_work;
	struct delayed_work bat_changed_work;
	struct delayed_work usb_changed_work;
	struct wake_lock bat_changed_wake_lock;
	struct wake_lock usb_changed_wake_lock;
	struct wake_lock safety_timer_wake_lock;
	struct delayed_work store_mode_work;
	struct delayed_work safety_timer_work;
	struct wake_lock siop_wake_lock;
	struct delayed_work siop_work;
	struct wake_lock slate_wake_lock;
	struct delayed_work slate_work;

	/* test mode */
	struct alarm polling_alarm;
	ktime_t last_poll_time;
	bool store_mode;
	bool slate_mode;
	unsigned int store_mode_polling_time;
	unsigned int store_mode_charging_max;
	unsigned int store_mode_charging_min;

	unsigned int pdata_expired_time;
	unsigned int recharging_expired_time;
	unsigned int standard_curr;
	unsigned int safety_timer_polling_time;

	bool lcd_status;
	bool stop_timer;
	bool safety_timer_set;
	bool safety_timer_scheduled;
	unsigned long prev_safety_time;
	unsigned long expired_time;
	unsigned long cal_safety_time;

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	int temp_test_batt_temp;
	int temp_test_chg_temp;
	int step_chg_test_value[9];
#endif
	bool wdt_kick_disable;

	int batt_cycle;
	bool isApdo;

	/* battery monitor */
	int cable_real_type;
	int status;
	int prev_status;
	int batt_temp;
	int die_temp;
	int cp_die_temp;
	int vbus_level;
	int float_voltage;
	int batt_health;
	int prev_batt_health;
	int die_health;
	int charge_type;
	int prev_charge_type;
	int soc;
	int v_now;
	int i_in;
	int i_now;
	int i_chg;
	int i_max;
	int hw_max;
	int ocv;
	int conn_type;
	int typec_mode;

	int gpio_vbat_sense;
	int gpio_sub_pcb_detect;
	unsigned int full_check_current_1st;
	bool use_temp_adc_table;
	bool use_chg_temp_adc_table;
	unsigned int temp_adc_table_size;
	sec_bat_adc_table_data_t *temp_adc_table;
	unsigned int chg_temp_adc_table_size;
	sec_bat_adc_table_data_t *chg_temp_adc_table;

	int default_input_current;
	int default_charging_current;
	sec_charging_current_t *charging_current;
	
#if defined(CONFIG_BATTERY_CISD)
	struct cisd cisd;
	bool skip_cisd;
	int prev_volt;
	int prev_temp;
	int prev_jig_on;
	int enable_update_data;
	int prev_chg_on;

	unsigned int cisd_cap_high_thr;
	unsigned int cisd_cap_low_thr;
	unsigned int cisd_cap_limit;
	unsigned int max_voltage_thr;
	unsigned int cisd_alg_index;

	bool is_jig_on;
	bool factory_mode;
	int voltage_now;		/* cell voltage (mV) */
	/* temperature check */
	int temperature;	/* battery temperature */
	int chg_temp;		/* charger temperature */
	int wpc_temp;
	int usb_temp;
	int fg_reset;
	bool charging_block;
	char batt_type[48];
#endif
	int siop_input_limit_current;
	int siop_charging_limit_current;
	int siop_hv_input_limit_current;
	int siop_hv_charging_limit_current;
	int siop_hv_12v_input_limit_current;
	int siop_hv_12v_charging_limit_current;
	int siop_dc_input_limit_current;
	int siop_dc_charging_limit_current;
	int minimum_hv_input_current_by_siop_10;

	int batt_pon_ocv;
	bool batt_temp_overheat_check;
	int interval_pdp_limit_w;
};

#define DEFAULT_SWELLING_CNT	4
#define SWELLING_TYPE_CNT	2

#define JEITA_COOL3	0
#define JEITA_COOL2	1
#define JEITA_COOL1	2
#define JEITA_NORMAL	3
#define JEITA_WARM	4
#define JEITA_MAX	5

extern int swelling_index[SWELLING_TYPE_CNT][JEITA_MAX];

extern int swelling_type;

extern unsigned int lpcharge;
extern int get_pd_active(struct sec_battery_info *battery);
extern void sec_bat_set_current_event(unsigned int current_event_val, unsigned int current_event_mask);
extern void sec_bat_set_misc_event(unsigned int misc_event_val, unsigned int misc_event_mask);
extern void sec_bat_set_slate_mode(struct sec_battery_info *battery);
extern void sec_bat_set_ship_mode(int en);
extern int sec_bat_get_ship_mode(void);
extern void sec_bat_set_fet_control(int off);
extern int sec_bat_get_fet_control(void);
extern int sec_bat_get_hv_charger_status(struct sec_battery_info *battery);
extern void sec_bat_get_ps_ready(struct sec_battery_info *battery);
extern int sec_bat_get_fg_asoc(struct sec_battery_info *battery);
extern void sec_batt_reset_soc(void);
extern bool is_store_mode(void);
extern void sec_start_store_mode(void);
extern void sec_stop_store_mode(void);
extern void sec_bat_set_usb_configuration(int config);
extern struct sec_battery_info * get_sec_battery(void);
extern int get_battery_health(void);
extern bool sec_bat_get_wdt_control(void);
extern void sec_bat_set_gpio_vbat_sense(int signal);
extern void qg_set_sdam_prifile_version(u8 ver);
extern bool sec_bat_use_temp_adc_table(void);
extern bool sec_bat_use_chg_temp_adc_table(void);
extern int sec_bat_convert_adc_to_temp(enum sec_battery_adc_channel channel, int temp_adc);
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
extern void update_step_chg_data(int t[]);
#endif
#if defined(CONFIG_BATTERY_CISD)
extern bool sec_bat_cisd_check(struct sec_battery_info *battery);
extern void sec_battery_cisd_init(struct sec_battery_info *battery);
extern void set_cisd_pad_data(struct sec_battery_info *battery, const char* buf);
#endif
extern void sec_bat_run_monitor_work(void);
extern void sec_bat_run_siop_work(void);
extern void sec_bat_safety_timer_reset(void);
extern int get_usb_voltage_now(struct sec_battery_info *battery);
extern void sec_bat_set_cable_type_current(int real_cable_type, bool attached);
extern int get_rid_type(void);
extern void sec_bat_set_ocp_mode(void);
#endif /* __SEC_BATTERY_H */
