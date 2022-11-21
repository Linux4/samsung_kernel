/*
 * sec_battery.h
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

#include <linux/battery/sec_charging_common.h>
#include <linux/of_gpio.h>
#if defined(ANDROID_ALARM_ACTIVATED)
#include <linux/android_alarm.h>
#else
#include <linux/alarmtimer.h>
#endif
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */
#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif
#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
#include <linux/usb_notify.h>
#endif
#if defined(CONFIG_BATTERY_CISD)
#include <linux/battery/sec_cisd.h>
#endif
#include <linux/sec_batt.h>

#if defined(CONFIG_CHARGING_VZWCONCEPT)
#define STORE_MODE_CHARGING_MAX 35
#define STORE_MODE_CHARGING_MIN 30
#else
#define STORE_MODE_CHARGING_MAX 70
#define STORE_MODE_CHARGING_MIN 60
#endif

#define ADC_CH_COUNT		10
#define ADC_SAMPLE_COUNT	10
#define BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE	0x00000001

#define SEC_INPUT_VOLTAGE_5V	5
#define SEC_INPUT_VOLTAGE_9V	9

struct adc_sample_info {
	unsigned int cnt;
	int total_adc;
	int average_adc;
	int adc_arr[ADC_SAMPLE_COUNT];
	int index;
};

struct sec_battery_info {
	struct device *dev;
	sec_battery_platform_data_t *pdata;
	/* power supply used in Android */
	struct power_supply psy_bat;
	struct power_supply psy_usb;
	struct power_supply psy_ac;
	struct power_supply psy_wireless;
	struct power_supply psy_ps;
	unsigned int irq;

	struct notifier_block batt_nb;
#if defined(CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
#endif
#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
	struct notifier_block usb_nb;
#endif

	int status;
	int health;
	bool present;

	int voltage_now;		/* cell voltage (mV) */
	int voltage_avg;		/* average voltage (mV) */
	int voltage_ocv;		/* open circuit voltage (mV) */
	int current_now;		/* current (mA) */
	int inbat_adc;                  /* inbat adc */
	int current_avg;		/* average current (mA) */
	int current_max;		/* input current limit (mA) */
	int current_adc;
	int current_isys_now;	/* isys current (mA) */
	int current_isys_avg;	/* isys avg current (mA) */

	unsigned int capacity;			/* SOC (%) */



	struct mutex adclock;
	struct adc_sample_info	adc_sample[ADC_CH_COUNT];

	/* keep awake until monitor is done */
	struct wake_lock monitor_wake_lock;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;
#ifdef CONFIG_SAMSUNG_BATTERY_FACTORY
	struct wake_lock lpm_wake_lock;
#endif
	unsigned int polling_count;
	unsigned int polling_time;
	bool polling_in_sleep;
	bool polling_short;

	struct delayed_work polling_work;
	struct alarm polling_alarm;
	ktime_t last_poll_time;

	/* event set */
	unsigned int event;
	unsigned int event_wait;

	struct alarm event_termination_alarm;

	ktime_t	last_event_time;

#if defined(CONFIG_BATTERY_CISD)
	struct cisd cisd;
#endif

	/* battery check */
	unsigned int check_count;
	/* ADC check */
	unsigned int check_adc_count;
	unsigned int check_adc_value;

	/* health change check */
	bool health_change;

	/* time check */
	unsigned long charging_start_time;
	unsigned long charging_passed_time;
	unsigned long charging_next_time;
	unsigned long charging_fullcharged_time;

	/* chg temperature check */
	bool chg_limit;

	/* wpc temperature and pad status check */
	bool pad_limit;

	/* bat temperature check */
	bool mix_limit;

	/* temperature check */
	int temperature;	/* battery temperature */
	int temper_amb;		/* target temperature */
	int chg_temp;
	int pre_chg_temp;
	int wpc_temp;
	int camera_temp;

	int temp_adc;
	int temp_ambient_adc;
	int chg_temp_adc;
	int wpc_temp_adc;
	int camera_temp_adc;

	int camera_temp_limit;

	bool camera_limit;
	bool prev_camera_limit;

	int temp_highlimit_threshold;
	int temp_highlimit_recovery;
	int temp_high_threshold;
	int temp_high_recovery;
	int temp_low_threshold;
	int temp_low_recovery;

	unsigned int temp_highlimit_cnt;
	unsigned int temp_high_cnt;
	unsigned int temp_low_cnt;
	unsigned int temp_recover_cnt;

	/* charging */
	unsigned int charging_mode;
	bool is_recharging;
	bool is_jig_on;
	int cable_type;
	int muic_cable_type;
	bool usb_3s_nodevice;
#if defined(CONFIG_VBUS_NOTIFIER)
	int muic_vbus_status;
#endif
	int extended_cable_type;
	struct wake_lock cable_wake_lock;
	struct delayed_work cable_work;
	struct wake_lock vbus_wake_lock;
	struct delayed_work siop_work;
	struct wake_lock siop_wake_lock;
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	struct delayed_work update_work;
	struct delayed_work fw_init_work;
#endif

	unsigned int full_check_cnt;
	unsigned int recharge_check_cnt;

	/* wireless charging enable*/
	int wc_enable;
	int wc_status;

	int wire_status;

	/* wearable charging */
	int ps_enable;
	int ps_status;
	int ps_changed;

	/* test mode */
	int test_mode;
	bool factory_mode;
	bool store_mode;
	bool ignore_store_mode;
	bool slate_mode;

	/* MTBF test for CMCC */
	bool is_hc_usb;

	bool ignore_siop;
	int r_siop_level;
	int siop_level;
	int siop_event;
	int stability_test;
	int eng_not_full_status;

#if defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	bool factory_self_discharging_mode_on;
	bool force_discharging;
	bool self_discharging;
	bool discharging_ntc;
	int discharging_ntc_adc;
	int self_discharging_adc;
#endif

	bool charging_block;
#if defined(CONFIG_BATTERY_SWELLING)
	bool swelling_mode;
	unsigned long swelling_block_start;
	unsigned long swelling_block_passed;
	int swelling_full_check_cnt;
#endif
#if defined(CONFIG_AFC_CHARGER_MODE)
	char *hv_chg_name;
#endif
#if defined(CONFIG_WIRELESS_CHARGER_INBATTERY) || defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
	int wc_current;
	int cc_cv_mode;
	bool full_mode;
	bool cs100_status;
#endif
#if defined(CONFIG_CALC_TIME_TO_FULL)
	int timetofull;
	bool complete_timetofull;
	struct delayed_work timetofull_work;
#endif
	int batt_cycle;

	struct mutex misclock;
	unsigned int misc_event;
	unsigned int prev_misc_event;
	struct delayed_work misc_event_work;
	struct wake_lock misc_event_wake_lock;

	unsigned long lcd_on_total_time;
	unsigned long lcd_on_time;
};

ssize_t sec_bat_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sec_bat_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SEC_BATTERY_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_bat_show_attrs,					\
	.store = sec_bat_store_attrs,					\
}

/* event check */
#define EVENT_NONE				(0)
#define EVENT_2G_CALL			(0x1 << 0)
#define EVENT_3G_CALL			(0x1 << 1)
#define EVENT_MUSIC				(0x1 << 2)
#define EVENT_VIDEO				(0x1 << 3)
#define EVENT_BROWSER			(0x1 << 4)
#define EVENT_HOTSPOT			(0x1 << 5)
#define EVENT_CAMERA			(0x1 << 6)
#define EVENT_CAMCORDER			(0x1 << 7)
#define EVENT_DATA_CALL			(0x1 << 8)
#define EVENT_WIFI				(0x1 << 9)
#define EVENT_WIBRO				(0x1 << 10)
#define EVENT_LTE				(0x1 << 11)
#define EVENT_LCD			(0x1 << 12)
#define EVENT_GPS			(0x1 << 13)

enum {
	BATT_RESET_SOC = 0,
	BATT_READ_RAW_SOC,
	BATT_READ_ADJ_SOC,
	BATT_TYPE,
	BATT_VFOCV,
	BATT_VOL_ADC,
	BATT_VOL_ADC_CAL,
	BATT_VOL_AVER,
	BATT_VOL_ADC_AVER,

	BATT_CURRENT_UA_NOW,
	BATT_CURRENT_UA_AVG,
	BATT_FILTER_CFG,

	BATT_TEMP,
	BATT_TEMP_ADC,
	BATT_TEMP_AVER,
	BATT_TEMP_ADC_AVER,
	CHG_TEMP,
	CHG_TEMP_ADC,
	BATT_VF_ADC,
	BATT_SLATE_MODE,

	BATT_LP_CHARGING,
	SIOP_ACTIVATED,
	SIOP_LEVEL,
	SIOP_EVENT,
	BATT_CHARGING_SOURCE,
	FG_REG_DUMP,
	FG_RESET_CAP,
	FG_CAPACITY,
	FG_ASOC,
	AUTH,
	CHG_CURRENT_ADC,
	WC_ADC,
	WC_STATUS,
	WC_ENABLE,
	HV_CHARGER_STATUS,
	HV_CHARGER_SET,
	HV_CHARGER_SUPPORT,
	HV_WC_CHARGER_SUPPORT,
	FACTORY_MODE,
	STORE_MODE,
	UPDATE,
	TEST_MODE,

	BATT_EVENT_CALL,
	BATT_EVENT_2G_CALL,
	BATT_EVENT_TALK_GSM,
	BATT_EVENT_3G_CALL,
	BATT_EVENT_TALK_WCDMA,
	BATT_EVENT_MUSIC,
	BATT_EVENT_VIDEO,
	BATT_EVENT_BROWSER,
	BATT_EVENT_HOTSPOT,
	BATT_EVENT_CAMERA,
	BATT_EVENT_CAMCORDER,
	BATT_EVENT_DATA_CALL,
	BATT_EVENT_WIFI,
	BATT_EVENT_WIBRO,
	BATT_EVENT_LTE,
	BATT_EVENT_LCD,
	BATT_EVENT_GPS,
	BATT_EVENT,
	BATT_TEMP_TABLE,
	BATT_HIGH_CURRENT_USB,
#if defined(CONFIG_SAMSUNG_BATTERY_ENG_TEST)
	BATT_TEST_CHARGE_CURRENT,
#endif
	BATT_STABILITY_TEST,
	BATT_CAPACITY_MAX,
	BATT_INBAT_VOLTAGE,
#if defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	BATT_DISCHARGING_CHECK,
	BATT_DISCHARGING_CHECK_ADC,
	BATT_DISCHARGING_NTC,
	BATT_DISCHARGING_NTC_ADC,
	BATT_SELF_DISCHARGING_CONTROL,
#endif
#if defined(CONFIG_WIRELESS_CHARGER_INBATTERY) || defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
	BATT_INBAT_WIRELESS_CS100,
#endif
	HMT_TA_CONNECTED,
	HMT_TA_CHARGE,
	FG_CYCLE,
	FG_FULL_VOLTAGE,
	FG_FULLCAPNOM,
	BATTERY_CYCLE,
#if defined(CONFIG_WIRELESS_CHARGER_THM)
	BATT_WPC_TEMP,
	BATT_WPC_TEMP_ADC,
#endif
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	BATT_WIRELESS_FIRMWARE_UPDATE,
	BATT_WIRELESS_OTP_FIRMWARE_RESULT,
	BATT_WIRELESS_IC_GRADE,
	BATT_WIRELESS_FIRMWARE_VER_BIN,
	BATT_WIRELESS_FIRMWARE_VER,
	BATT_WIRELESS_TX_FIRMWARE_RESULT,
	BATT_WIRELESS_TX_FIRMWARE_VER,
	BATT_TX_STATUS,
#endif
#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
	BATT_WIRELESS_VOUT,
	BATT_WIRELESS_VRCT,
	BATT_HV_WIRELESS_STATUS,
	BATT_HV_WIRELESS_PAD_CTRL,
#endif
	BATT_TUNE_FLOAT_VOLTAGE,
	BATT_TUNE_INPUT_CHARGE_CURRENT,
	BATT_TUNE_FAST_CHARGE_CURRENT,
	BATT_TUNE_UI_TERM_CURRENT_1ST,
	BATT_TUNE_UI_TERM_CURRENT_2ND,
	BATT_TUNE_TEMP_HIGH_EVENT,
	BATT_TUNE_TEMP_HIGH_REC_EVENT,
	BATT_TUNE_TEMP_LOW_EVENT,
	BATT_TUNE_TEMP_LOW_REC_EVENT,
	BATT_TUNE_TEMP_HIGH_NORMAL,
	BATT_TUNE_TEMP_HIGH_REC_NORMAL,
	BATT_TUNE_TEMP_LOW_NORMAL,
	BATT_TUNE_TEMP_LOW_REC_NORMAL,
	BATT_TUNE_CHG_TEMP_HIGH,
	BATT_TUNE_CHG_TEMP_REC,
	BATT_TUNE_CHG_LIMMIT_CURRENT,
	BATT_TUNE_COIL_TEMP_HIGH,
	BATT_TUNE_COIL_TEMP_REC,
	BATT_TUNE_COIL_LIMMIT_CURRENT,
	CAMERA_TEMP_ADC,
	CAMERA_TEMP,
	CAMERA_LIMIT,
	BATT_MISC_EVENT,
	CISD_FULLCAPREP_MAX,
#if defined(CONFIG_BATTERY_CISD)
	CISD_DATA,
#endif
};

#ifdef CONFIG_OF
extern int adc_read(struct sec_battery_info *battery, int channel);
extern void adc_init(struct platform_device *pdev, struct sec_battery_info *battery);
extern void adc_exit(struct sec_battery_info *battery);
#endif

#if defined(CONFIG_BATTERY_CISD)
extern bool sec_bat_cisd_check(struct sec_battery_info *battery);
extern void sec_battery_cisd_init(struct sec_battery_info *battery);
#endif
#endif /* __SEC_BATTERY_H */
