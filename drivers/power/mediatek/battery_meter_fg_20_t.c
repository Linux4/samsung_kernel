/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/slab.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <linux/uaccess.h>
#include <linux/netlink.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/genetlink.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>


#include <mt-plat/mt_boot.h>
#include <mt-plat/mtk_rtc.h>


#include <mt-plat/mt_boot_reason.h>

#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter_hal.h>
#include <mach/mt_battery_meter.h>
#include <mach/mt_battery_meter_table.h>
#include <mach/mt_pmic.h>

#include <linux/battery/sec_charging_common.h>

#include <mt-plat/upmu_common.h>

static enum power_supply_property mtk_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TEMP,
};

/* ============================================================ // */
/* define */
/* ============================================================ // */

static DEFINE_MUTEX(FGADC_mutex);

int Enable_FGADC_LOG = BMLOG_INFO_LEVEL;

#define NETLINK_FGD 26
#define CUST_SETTING_VERSION 0x100000
#define FGD_CHECK_VERSION 0x100001

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
BATTERY_METER_CONTROL battery_meter_ctrl = NULL;

kal_bool gFG_Is_Charging = KAL_FALSE;
kal_bool gFG_Is_Charging_init = KAL_FALSE;

unsigned int g_spm_timer = 600;

struct timespec g_sleep_total_time;

static struct sock *daemo_nl_sk;
static void nl_send_to_user(u32 pid, int seq, struct fgd_nl_msg_t *reply_msg);
static u_int g_fgd_pid;
static unsigned int g_fgd_version = -1;
static kal_bool init_flag;

/* PMIC AUXADC Related Variable */
int g_R_BAT_SENSE = R_BAT_SENSE;
int g_R_I_SENSE = R_I_SENSE;
int g_R_CHARGER_1 = R_CHARGER_1;
int g_R_CHARGER_2 = R_CHARGER_2;

int gFG_plugout_status = 0;

/* HW FG */
#ifndef DIFFERENCE_HWOCV_RTC
#define DIFFERENCE_HWOCV_RTC		30	/* 30% difference */
#endif

#ifndef DIFFERENCE_HWOCV_SWOCV
#define DIFFERENCE_HWOCV_SWOCV		15	/* 105% difference */
#endif

#ifndef DIFFERENCE_SWOCV_RTC
#define DIFFERENCE_SWOCV_RTC		10	/* 10% difference */
#endif

#ifndef MAX_SWOCV
#define MAX_SWOCV			5	/* 5% maximum */
#endif

/* SW Fuel Gauge */
#ifndef MAX_HWOCV
#define MAX_HWOCV				5
#endif

#ifndef MAX_VBAT
#define MAX_VBAT				90
#endif

#ifndef DIFFERENCE_HWOCV_VBAT
#define DIFFERENCE_HWOCV_VBAT			30
#endif

#ifndef Q_MAX_SYS_VOLTAGE
#define Q_MAX_SYS_VOLTAGE 3300
#endif

/* move from battery_common_fg_20.c */
PMU_ChargerStruct BMT_status;
kal_bool g_battery_soc_ready = KAL_FALSE;
struct timespec battery_duration_time[DURATION_NUM];	/* sec */
unsigned int wake_up_smooth_time = 0;	/* sec */
unsigned int battery_tracking_time;
CHARGING_CONTROL battery_charging_control;



/* smooth time tracking */
signed int gFG_coulomb_act_time = -1;
signed int gFG_coulomb_act_pre = 0;
signed int gFG_coulomb_act_diff = 0;
signed int gFG_coulomb_act_diff_time = 0;
signed int gFG_coulomb_is_charging = 0;

signed int gFG_DOD0_init = 0;
signed int gFG_DOD0 = 0;
signed int gFG_DOD1 = 0;
signed int gFG_DOD_B = 0;
signed int gFG_coulomb = 0;
signed int gFG_coulomb_act = 0;
signed int gFG_voltage = 0;
signed int gFG_current = 0;
signed int gFG_current_init = 0;
signed int gFG_capacity = 0;
signed int gFG_capacity_by_c = -1;
signed int gFG_capacity_by_c_init = 0;
signed int gFG_capacity_by_v = 0;
signed int gFG_capacity_by_v_init = 0;
signed int gFG_temp = 100;
signed int gFG_temp_avg = 100;
signed int gFG_temp_avg_init = 100;
signed int gFG_resistance_bat = 0;
signed int gFG_compensate_value = 0;
signed int gFG_ori_voltage = 0;
signed int gFG_BATT_CAPACITY = 0;
signed int gFG_voltage_init = 0;
signed int gFG_current_auto_detect_R_fg_total = 0;
signed int gFG_current_auto_detect_R_fg_count = 0;
signed int gFG_current_auto_detect_R_fg_result = 0;
signed int gFG_15_vlot = 3700;
signed int gFG_BATT_CAPACITY_high_current = 1200;
signed int gFG_BATT_CAPACITY_aging = 1200;
signed int gFG_vbat = 0;
signed int gFG_swocv = 0;
signed int gFG_hwocv = 0;
signed int gFG_vbat_soc = 0;
signed int gFG_hw_soc = 0;
signed int gFG_sw_soc = 0;

signed int g_tracking_point = CUST_TRACKING_POINT;
signed int g_rtc_fg_soc = 0;
signed int g_I_SENSE_offset = 0;
signed int g_charger_exist = 0;


/* SW FG */
signed int oam_v_ocv_init = 0;
signed int oam_v_ocv_1 = 0;
signed int oam_v_ocv_2 = 0;
signed int oam_r_1 = 0;
signed int oam_r_2 = 0;
signed int oam_d0 = 0;
signed int oam_i_ori = 0;
signed int oam_i_1 = 0;
signed int oam_i_2 = 0;
signed int oam_car_1 = 0;
signed int oam_car_2 = 0;
signed int oam_d_1 = 1;
signed int oam_d_2 = 1;
signed int oam_d_3 = 1;
signed int oam_d_3_pre = 0;
signed int oam_d_4 = 0;
signed int oam_d_4_pre = 0;
signed int oam_d_5 = 0;
signed int oam_init_i = 0;
signed int oam_run_i = 0;
signed int d5_count = 0;
signed int d5_count_time = 60;
signed int d5_count_time_rate = 1;
signed int g_d_hw_ocv = 0;
signed int g_vol_bat_hw_ocv = 0;

/* SW FG 2.0 */
signed int oam_v_ocv;
signed int oam_r;
signed int swfg_ap_suspend_time;
signed int ap_suspend_car;
struct timespec ap_suspend_time;
signed int total_suspend_times;
signed int this_suspend_times;
signed int last_hwocv;
signed int last_i;
signed int hwocv_token;
signed int is_hwocv_update;

signed int g_hw_ocv_before_sleep = 0;
struct timespec suspend_time, car_time;
signed int g_sw_vbat_temp = 0;
struct timespec last_oam_run_time;

static DECLARE_WAIT_QUEUE_HEAD(fg_wq);
static struct hrtimer battery_meter_kthread_timer;
unsigned char fg_ipoh_reset;
struct timespec batteryMeterThreadRunTime;
static bool bat_meter_thread_timeout;
static DEFINE_MUTEX(battery_meter_mutex);
struct wake_lock battery_meter_lock;

/*static signed int coulomb_before_sleep = 0x123456;*/
/*static signed int last_time = 1;*/
/* aging mechanism */
#ifdef MTK_ENABLE_AGING_ALGORITHM

#ifndef SUSPEND_CURRENT_CHECK_THRESHOLD
#define SUSPEND_CURRENT_CHECK_THRESHOLD 100	/* 10mA */
#endif


#ifndef DIFFERENCE_VOLTAGE_UPDATE
#define DIFFERENCE_VOLTAGE_UPDATE 20	/* 20mV */
#endif

#ifndef OCV_RECOVER_TIME
#define OCV_RECOVER_TIME 2100
#endif

#ifndef AGING1_UPDATE_SOC
#define AGING1_UPDATE_SOC 30
#endif

#ifndef AGING1_LOAD_SOC
#define AGING1_LOAD_SOC 70
#endif

#ifndef MIN_DOD_DIFF_THRESHOLD
#define MIN_DOD_DIFF_THRESHOLD 40
#endif

#ifndef MIN_DOD2_DIFF_THRESHOLD
#define MIN_DOD2_DIFF_THRESHOLD 70
#endif

#ifndef CHARGE_TRACKING_TIME
#define CHARGE_TRACKING_TIME		60
#endif

#ifndef DISCHARGE_TRACKING_TIME
#define DISCHARGE_TRACKING_TIME		10
#endif

static signed int aging1_load_soc = AGING1_LOAD_SOC;
static signed int aging1_update_soc = AGING1_UPDATE_SOC;
static signed int shutdown_system_voltage = SHUTDOWN_SYSTEM_VOLTAGE;
static signed int charge_tracking_time = CHARGE_TRACKING_TIME;
static signed int discharge_tracking_time = DISCHARGE_TRACKING_TIME;
static signed int shutdown_gauge1_mins = SHUTDOWN_GAUGE1_MINS;
static signed int difference_voltage_update = DIFFERENCE_VOLTAGE_UPDATE;


#endif				/* aging mechanism */

#ifndef RECHARGE_TOLERANCE
#define RECHARGE_TOLERANCE	10
#endif

/*static signed int recharge_tolerance = RECHARGE_TOLERANCE;*/

#ifdef SHUTDOWN_GAUGE0
static signed int shutdown_gauge0 = 1;
#else
static signed int shutdown_gauge0;
#endif

#ifdef SHUTDOWN_GAUGE1_MINS
static signed int shutdown_gauge1_xmins = 1;
#else
static signed int shutdown_gauge1_xmins;
#endif

#ifndef FG_CURRENT_INIT_VALUE
#define FG_CURRENT_INIT_VALUE 3500
#endif

#ifndef FG_MIN_CHARGING_SMOOTH_TIME
#define FG_MIN_CHARGING_SMOOTH_TIME 40
#endif

#ifndef APSLEEP_MDWAKEUP_CAR
#define APSLEEP_MDWAKEUP_CAR 5240
#endif

#ifndef AP_MDSLEEP_CAR
#define AP_MDSLEEP_CAR 30
#endif

#ifndef APSLEEP_BATTERY_VOLTAGE_COMPENSATE
#define APSLEEP_BATTERY_VOLTAGE_COMPENSATE 150
#endif


signed int gFG_battery_cycle = 0;
signed int gFG_aging_factor_1 = 100;
signed int gFG_aging_factor_2 = 100;
signed int gFG_loading_factor1 = 100;
signed int gFG_loading_factor2 = 100;
/* battery info */

signed int gFG_coulomb_cyc = 0;
signed int gFG_coulomb_aging = 0;
signed int gFG_pre_coulomb_count = 0x12345678;
#ifdef MTK_BATTERY_LIFETIME_DATA_SUPPORT
signed int gFG_max_voltage = 0;
signed int gFG_min_voltage = 10000;
signed int gFG_max_current = 0;
signed int gFG_min_current = 0;
signed int gFG_max_temperature = -20;
signed int gFG_min_temperature = 100;

#endif				/* battery info */

static signed int gFG_daemon_log_level = BM_DAEMON_DEFAULT_LOG_LEVEL;
static unsigned char gDisableFG;
static signed int ocv_check_time = OCV_RECOVER_TIME;
static signed int suspend_current_threshold = SUSPEND_CURRENT_CHECK_THRESHOLD;

unsigned int g_sw_fg_version = 150327;
signed int g_RBAT_PULL_UP_R = RBAT_PULL_UP_R;
signed int g_RBAT_PULL_UP_VOLT = RBAT_PULL_UP_VOLT;
signed int g_CUST_R_SENSE = CUST_R_SENSE;
signed int g_R_FG_VALUE = R_FG_VALUE;
signed int g_CUST_R_FG_OFFSET = CUST_R_FG_OFFSET;

kal_bool g_use_mtk_fg = KAL_FALSE;

/* ============================================================ // */
/* function prototype */
/* ============================================================ // */


struct battery_meter_table_custom_data {

	/* cust_battery_meter_table.h */
	int battery_profile_t0_size;
	BATTERY_PROFILE_STRUCT battery_profile_t0[100];
	int battery_profile_t1_size;
	BATTERY_PROFILE_STRUCT battery_profile_t1[100];
	int battery_profile_t2_size;
	BATTERY_PROFILE_STRUCT battery_profile_t2[100];
	int battery_profile_t3_size;
	BATTERY_PROFILE_STRUCT battery_profile_t3[100];
	int battery_profile_temperature_size;
	BATTERY_PROFILE_STRUCT battery_profile_temperature[100];

	int r_profile_t0_size;
	R_PROFILE_STRUCT r_profile_t0[100];
	int r_profile_t1_size;
	R_PROFILE_STRUCT r_profile_t1[100];
	int r_profile_t2_size;
	R_PROFILE_STRUCT r_profile_t2[100];
	int r_profile_t3_size;
	R_PROFILE_STRUCT r_profile_t3[100];
	int r_profile_temperature_size;
	R_PROFILE_STRUCT r_profile_temperature[100];
};

struct battery_meter_custom_data batt_meter_cust_data;
struct battery_meter_table_custom_data batt_meter_table_cust_data;
struct battery_custom_data batt_cust_data;


/* Temperature window size */
#define TEMP_AVERAGE_SIZE	30
kal_bool gFG_Is_offset_init = KAL_FALSE;

#ifdef MTK_MULTI_BAT_PROFILE_SUPPORT
unsigned int g_fg_battery_id = 0;

#ifdef MTK_GET_BATTERY_ID_BY_AUXADC
void fgauge_get_profile_id(void)
{
	int id_volt = 0;
	int id = 0;
	int ret = 0;

	ret = IMM_GetOneChannelValue_Cali(BATTERY_ID_CHANNEL_NUM, &id_volt);
	if (ret != 0)
		bm_info("[fgauge_get_profile_id]id_volt read fail\n");
	else
		bm_info("[fgauge_get_profile_id]id_volt = %d\n", id_volt);

	if ((sizeof(g_battery_id_voltage) / sizeof(signed int)) != TOTAL_BATTERY_NUMBER) {
		bm_info("[fgauge_get_profile_id]error! voltage range incorrect!\n");
		return;
	}

	for (id = 0; id < TOTAL_BATTERY_NUMBER; id++) {
		if (id_volt < g_battery_id_voltage[id]) {
			g_fg_battery_id = id;
			break;
		} else if (g_battery_id_voltage[id] == -1) {
			g_fg_battery_id = TOTAL_BATTERY_NUMBER - 1;
		}
	}

	bm_info("[fgauge_get_profile_id]Battery id (%d)\n", g_fg_battery_id);
}
#elif defined(MTK_GET_BATTERY_ID_BY_GPIO)
void fgauge_get_profile_id(void)
{
	g_fg_battery_id = 0;
}
#else
void fgauge_get_profile_id(void)
{
	g_fg_battery_id = 0;
}
#endif
#endif

/* ============================================================ // */
/* extern function */
/* ============================================================ // */
/* extern int get_rtc_spare_fg_value(void); */
/* extern unsigned long rtc_read_hw_time(void); */

/* ============================================================ // */

unsigned char bat_is_kpoc(void)
{
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
	    || get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
		return KAL_TRUE;
	}
#endif
	return KAL_FALSE;
}

unsigned int bat_get_ui_percentage(void)
{
	unsigned int SOC_val = 0;
	union power_supply_propval val;

	if (g_use_mtk_fg)
		SOC_val = BMT_status.UI_SOC2;
	else {
		psy_do_property("battery", get, POWER_SUPPLY_PROP_CAPACITY, val);
		bm_info("[psy_do_property] CAPACITY=%d\n", val.intval);
		SOC_val = val.intval;
	}

	return SOC_val;
}

kal_bool upmu_is_chr_det(void)
{
	union power_supply_propval val;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, val);
	bm_info("[psy_do_property] online=%d\n", val.intval);

	if (val.intval == POWER_SUPPLY_TYPE_BATTERY || val.intval == POWER_SUPPLY_TYPE_UNKNOWN)
		return KAL_FALSE;

	return KAL_TRUE;
}

kal_bool bat_is_charger_exist(void)
{
	if (upmu_is_chr_det() == KAL_TRUE)
		return KAL_TRUE;

	bm_debug("[bat_is_charger_exist] No charger\n");
	return KAL_FALSE;
}

kal_bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det() == KAL_TRUE)
		return KAL_TRUE;

	bm_debug("[pmic_chrdet_status] No charger\n");
	return KAL_FALSE;
}

void do_chrdet_int_task(void)
{
	if (g_use_mtk_fg) {
		wakeup_fg_algo(FG_CHARGER);
		wake_up_bat();
	}
}

int read_tbat_value(void)
{
	union power_supply_propval val;
	int ret = 0;

	if (g_use_mtk_fg)
		ret = battery_meter_get_battery_temperature();
	else {
		psy_do_property("battery", get, POWER_SUPPLY_PROP_TEMP, val);
		ret = val.intval / 10;
	}

	return ret;
}

unsigned int get_cv_voltage(void)
{
	return BATTERY_VOLT_04_200000_V;
}

signed int battery_meter_get_battery_current(void)
{
	int ret = 0;
	signed int val = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &val);

	return val;
}

kal_bool battery_meter_get_battery_current_sign(void)
{
	int ret = 0;
	kal_bool val = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &val);

	return val;
}

void BATTERY_SetUSBState(int usb_state_value)
{
}

int __batt_init_cust_data_from_cust_header(void)
{
	/* cust_charging.h */
	/* stop charging while in talking mode */
#if defined(STOP_CHARGING_IN_TAKLING)
	batt_cust_data.stop_charging_in_takling = 1;
#else				/* #if defined(STOP_CHARGING_IN_TAKLING) */
	batt_cust_data.stop_charging_in_takling = 0;
#endif				/* #if defined(STOP_CHARGING_IN_TAKLING) */

#if defined(TALKING_RECHARGE_VOLTAGE)
	batt_cust_data.talking_recharge_voltage = TALKING_RECHARGE_VOLTAGE;
#endif

#if defined(TALKING_SYNC_TIME)
	batt_cust_data.talking_sync_time = TALKING_SYNC_TIME;
#endif

	/* Battery Temperature Protection */
#if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT)
	batt_cust_data.mtk_temperature_recharge_support = 1;
#else				/* #if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT) */
	batt_cust_data.mtk_temperature_recharge_support = 0;
#endif				/* #if defined(MTK_TEMPERATURE_RECHARGE_SUPPORT) */

#if defined(MAX_CHARGE_TEMPERATURE)
	batt_cust_data.max_charge_temperature = MAX_CHARGE_TEMPERATURE;
#endif

#if defined(MAX_CHARGE_TEMPERATURE_MINUS_X_DEGREE)
	batt_cust_data.max_charge_temperature_minus_x_degree =
	    MAX_CHARGE_TEMPERATURE_MINUS_X_DEGREE;
#endif

#if defined(MIN_CHARGE_TEMPERATURE)
	batt_cust_data.min_charge_temperature = MIN_CHARGE_TEMPERATURE;
#endif

#if defined(MIN_CHARGE_TEMPERATURE_PLUS_X_DEGREE)
	batt_cust_data.min_charge_temperature_plus_x_degree = MIN_CHARGE_TEMPERATURE_PLUS_X_DEGREE;
#endif

#if defined(ERR_CHARGE_TEMPERATURE)
	batt_cust_data.err_charge_temperature = ERR_CHARGE_TEMPERATURE;
#endif

	/* Linear Charging Threshold */
#if defined(V_PRE2CC_THRES)
	batt_cust_data.v_pre2cc_thres = V_PRE2CC_THRES;
#endif
#if defined(V_CC2TOPOFF_THRES)
	batt_cust_data.v_cc2topoff_thres = V_CC2TOPOFF_THRES;
#endif
#if defined(RECHARGING_VOLTAGE)
	batt_cust_data.recharging_voltage = RECHARGING_VOLTAGE;
#endif
#if defined(CHARGING_FULL_CURRENT)
	batt_cust_data.charging_full_current = CHARGING_FULL_CURRENT;
#endif

	/* Charging Current Setting */
#if defined(CONFIG_USB_IF)
	batt_cust_data.config_usb_if = 1;
#else				/* #if defined(CONFIG_USB_IF) */
	batt_cust_data.config_usb_if = 0;
#endif				/* #if defined(CONFIG_USB_IF) */

#if defined(USB_CHARGER_CURRENT_SUSPEND)
	batt_cust_data.usb_charger_current_suspend = USB_CHARGER_CURRENT_SUSPEND;
#endif
#if defined(USB_CHARGER_CURRENT_UNCONFIGURED)
	batt_cust_data.usb_charger_current_unconfigured = USB_CHARGER_CURRENT_UNCONFIGURED;
#endif
#if defined(USB_CHARGER_CURRENT_CONFIGURED)
	batt_cust_data.usb_charger_current_configured = USB_CHARGER_CURRENT_CONFIGURED;
#endif
#if defined(USB_CHARGER_CURRENT)
	batt_cust_data.usb_charger_current = USB_CHARGER_CURRENT;
#endif
#if defined(AC_CHARGER_CURRENT)
	batt_cust_data.ac_charger_current = AC_CHARGER_CURRENT;
#endif
#if defined(NON_STD_AC_CHARGER_CURRENT)
	batt_cust_data.non_std_ac_charger_current = NON_STD_AC_CHARGER_CURRENT;
#endif
#if defined(CHARGING_HOST_CHARGER_CURRENT)
	batt_cust_data.charging_host_charger_current = CHARGING_HOST_CHARGER_CURRENT;
#endif
#if defined(APPLE_0_5A_CHARGER_CURRENT)
	batt_cust_data.apple_0_5a_charger_current = APPLE_0_5A_CHARGER_CURRENT;
#endif
#if defined(APPLE_1_0A_CHARGER_CURRENT)
	batt_cust_data.apple_1_0a_charger_current = APPLE_1_0A_CHARGER_CURRENT;
#endif
#if defined(APPLE_2_1A_CHARGER_CURRENT)
	batt_cust_data.apple_2_1a_charger_current = APPLE_2_1A_CHARGER_CURRENT;
#endif

	/* Precise Tunning
	   batt_cust_data.battery_average_data_number = BATTERY_AVERAGE_DATA_NUMBER;
	   batt_cust_data.battery_average_size = BATTERY_AVERAGE_SIZE;
	 */

	/* charger error check */
#if defined(BAT_LOW_TEMP_PROTECT_ENABLE)
	batt_cust_data.bat_low_temp_protect_enable = 1;
#else				/* #if defined(BAT_LOW_TEMP_PROTECT_ENABLE) */
	batt_cust_data.bat_low_temp_protect_enable = 0;
#endif				/* #if defined(BAT_LOW_TEMP_PROTECT_ENABLE) */

#if defined(V_CHARGER_ENABLE)
	batt_cust_data.v_charger_enable = V_CHARGER_ENABLE;
#endif
#if defined(V_CHARGER_MAX)
	batt_cust_data.v_charger_max = V_CHARGER_MAX;
#endif
#if defined(V_CHARGER_MIN)
	batt_cust_data.v_charger_min = V_CHARGER_MIN;
#endif

	/* Tracking TIME */
#if defined(ONEHUNDRED_PERCENT_TRACKING_TIME)
	batt_cust_data.onehundred_percent_tracking_time = ONEHUNDRED_PERCENT_TRACKING_TIME;
#endif
#if defined(NPERCENT_TRACKING_TIME)
	batt_cust_data.npercent_tracking_time = NPERCENT_TRACKING_TIME;
#endif
#if defined(SYNC_TO_REAL_TRACKING_TIME)
	batt_cust_data.sync_to_real_tracking_time = SYNC_TO_REAL_TRACKING_TIME;
#endif
#if defined(V_0PERCENT_TRACKING)
	batt_cust_data.v_0percent_tracking = V_0PERCENT_TRACKING;
#endif

	/* High battery support */
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	batt_cust_data.high_battery_voltage_support = 1;
#else				/* #if defined(HIGH_BATTERY_VOLTAGE_SUPPORT) */
	batt_cust_data.high_battery_voltage_support = 0;
#endif				/* #if defined(HIGH_BATTERY_VOLTAGE_SUPPORT) */

	return 0;
}


int __batt_meter_init_cust_data_from_cust_header(struct platform_device *dev)
{
	/* cust_battery_meter_table.h */

	batt_meter_table_cust_data.battery_profile_t0_size = sizeof(battery_profile_t0)
	    / sizeof(BATTERY_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.battery_profile_t0,
	       &battery_profile_t0, sizeof(battery_profile_t0));

	batt_meter_table_cust_data.battery_profile_t1_size = sizeof(battery_profile_t1)
	    / sizeof(BATTERY_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.battery_profile_t1,
	       &battery_profile_t1, sizeof(battery_profile_t1));

	batt_meter_table_cust_data.battery_profile_t2_size = sizeof(battery_profile_t2)
	    / sizeof(BATTERY_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.battery_profile_t2,
	       &battery_profile_t2, sizeof(battery_profile_t2));

	batt_meter_table_cust_data.battery_profile_t3_size = sizeof(battery_profile_t3)
	    / sizeof(BATTERY_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.battery_profile_t3,
	       &battery_profile_t3, sizeof(battery_profile_t3));

	batt_meter_table_cust_data.r_profile_t0_size =
	    sizeof(r_profile_t0) / sizeof(R_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.r_profile_t0, &r_profile_t0, sizeof(r_profile_t0));

	batt_meter_table_cust_data.r_profile_t1_size =
	    sizeof(r_profile_t1) / sizeof(R_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.r_profile_t1, &r_profile_t1, sizeof(r_profile_t1));

	batt_meter_table_cust_data.r_profile_t2_size =
	    sizeof(r_profile_t2) / sizeof(R_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.r_profile_t2, &r_profile_t2, sizeof(r_profile_t2));
	batt_meter_table_cust_data.r_profile_t3_size =
	    sizeof(r_profile_t3) / sizeof(R_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.r_profile_t3, &r_profile_t3, sizeof(r_profile_t3));

	/* cust_battery_meter.h */

#if defined(SOC_BY_HW_FG)
	batt_meter_cust_data.soc_flow = HW_FG;
#elif defined(SOC_BY_SW_FG)
	batt_meter_cust_data.soc_flow = SW_FG;
#elif defined(SOC_BY_AUXADC)
	batt_meter_cust_data.soc_flow = AUXADC;
#endif

#if defined(HW_FG_FORCE_USE_SW_OCV)
	batt_meter_cust_data.hw_fg_force_use_sw_ocv = 1;
#else				/* #if defined(HW_FG_FORCE_USE_SW_OCV) */
	batt_meter_cust_data.hw_fg_force_use_sw_ocv = 0;
#endif				/* #if defined(HW_FG_FORCE_USE_SW_OCV) */

	/* ADC resister */
	batt_meter_cust_data.r_bat_sense = R_BAT_SENSE;
	g_R_BAT_SENSE = R_BAT_SENSE;
	batt_meter_cust_data.r_i_sense = R_I_SENSE;
	g_R_I_SENSE = R_I_SENSE;
	batt_meter_cust_data.r_charger_1 = R_CHARGER_1;
	g_R_CHARGER_1 = R_CHARGER_1;
	batt_meter_cust_data.r_charger_2 = R_CHARGER_2;
	g_R_CHARGER_2 = R_CHARGER_2;

	batt_meter_cust_data.temperature_t0 = TEMPERATURE_T0;
	batt_meter_cust_data.temperature_t1 = TEMPERATURE_T1;
	batt_meter_cust_data.temperature_t2 = TEMPERATURE_T2;
	batt_meter_cust_data.temperature_t3 = TEMPERATURE_T3;
	batt_meter_cust_data.temperature_t = TEMPERATURE_T;

	batt_meter_cust_data.fg_meter_resistance = FG_METER_RESISTANCE;

	/* Qmax for battery  */
	batt_meter_cust_data.q_max_pos_50 = Q_MAX_POS_50;
	batt_meter_cust_data.q_max_pos_25 = Q_MAX_POS_25;
	batt_meter_cust_data.q_max_pos_0 = Q_MAX_POS_0;
	batt_meter_cust_data.q_max_neg_10 = Q_MAX_NEG_10;
	batt_meter_cust_data.q_max_pos_50_h_current = Q_MAX_POS_50_H_CURRENT;
	batt_meter_cust_data.q_max_pos_25_h_current = Q_MAX_POS_25_H_CURRENT;
	batt_meter_cust_data.q_max_pos_0_h_current = Q_MAX_POS_0_H_CURRENT;
	batt_meter_cust_data.q_max_neg_10_h_current = Q_MAX_NEG_10_H_CURRENT;
	batt_meter_cust_data.oam_d5 = OAM_D5;	/* 1 : D5,   0: D2 */

#if defined(CHANGE_TRACKING_POINT)
	batt_meter_cust_data.change_tracking_point = 1;
#else				/* #if defined(CHANGE_TRACKING_POINT) */
	batt_meter_cust_data.change_tracking_point = 0;
#endif				/* #if defined(CHANGE_TRACKING_POINT) */
	batt_meter_cust_data.cust_tracking_point = CUST_TRACKING_POINT;
	g_tracking_point = CUST_TRACKING_POINT;
	batt_meter_cust_data.cust_r_sense = CUST_R_SENSE;
	batt_meter_cust_data.cust_hw_cc = CUST_HW_CC;
	batt_meter_cust_data.aging_tuning_value = AGING_TUNING_VALUE;
	batt_meter_cust_data.cust_r_fg_offset = CUST_R_FG_OFFSET;
	batt_meter_cust_data.ocv_board_compesate = OCV_BOARD_COMPESATE;
	batt_meter_cust_data.r_fg_board_base = R_FG_BOARD_BASE;
	batt_meter_cust_data.r_fg_board_slope = R_FG_BOARD_SLOPE;
	batt_meter_cust_data.car_tune_value = CAR_TUNE_VALUE;

	/* HW Fuel gague  */
	batt_meter_cust_data.current_detect_r_fg = CURRENT_DETECT_R_FG;
	batt_meter_cust_data.minerroroffset = MinErrorOffset;
	batt_meter_cust_data.fg_vbat_average_size = FG_VBAT_AVERAGE_SIZE;
	batt_meter_cust_data.r_fg_value = R_FG_VALUE;

	batt_meter_cust_data.difference_hwocv_rtc = DIFFERENCE_HWOCV_RTC;
	batt_meter_cust_data.difference_hwocv_swocv = DIFFERENCE_HWOCV_SWOCV;
	batt_meter_cust_data.difference_swocv_rtc = DIFFERENCE_SWOCV_RTC;
	batt_meter_cust_data.max_swocv = MAX_SWOCV;

	batt_meter_cust_data.max_hwocv = MAX_HWOCV;
	batt_meter_cust_data.max_vbat = MAX_VBAT;
	batt_meter_cust_data.difference_hwocv_vbat = DIFFERENCE_HWOCV_VBAT;

	batt_meter_cust_data.suspend_current_threshold = SUSPEND_CURRENT_CHECK_THRESHOLD;
	batt_meter_cust_data.ocv_check_time = OCV_RECOVER_TIME;

	batt_meter_cust_data.shutdown_system_voltage = SHUTDOWN_SYSTEM_VOLTAGE;
	batt_meter_cust_data.recharge_tolerance = RECHARGE_TOLERANCE;

#if defined(FIXED_TBAT_25)
	batt_meter_cust_data.fixed_tbat_25 = 1;
#else				/* #if defined(FIXED_TBAT_25) */
	batt_meter_cust_data.fixed_tbat_25 = 0;
#endif				/* #if defined(FIXED_TBAT_25) */

	batt_meter_cust_data.batterypseudo100 = BATTERYPSEUDO100;
	batt_meter_cust_data.batterypseudo1 = BATTERYPSEUDO1;

	/* Dynamic change wake up period of battery thread when suspend */
	batt_meter_cust_data.vbat_normal_wakeup = VBAT_NORMAL_WAKEUP;
	batt_meter_cust_data.vbat_low_power_wakeup = VBAT_LOW_POWER_WAKEUP;
	batt_meter_cust_data.normal_wakeup_period = NORMAL_WAKEUP_PERIOD;
	/* _g_bat_sleep_total_time = NORMAL_WAKEUP_PERIOD; */
	batt_meter_cust_data.low_power_wakeup_period = LOW_POWER_WAKEUP_PERIOD;
	batt_meter_cust_data.close_poweroff_wakeup_period = CLOSE_POWEROFF_WAKEUP_PERIOD;

#if defined(INIT_SOC_BY_SW_SOC)
	batt_meter_cust_data.init_soc_by_sw_soc = 1;
#else				/* #if defined(INIT_SOC_BY_SW_SOC) */
	batt_meter_cust_data.init_soc_by_sw_soc = 0;
#endif				/* #if defined(INIT_SOC_BY_SW_SOC) */
#if defined(SYNC_UI_SOC_IMM)
	batt_meter_cust_data.sync_ui_soc_imm = 1;
#else				/* #if defined(SYNC_UI_SOC_IMM) */
	batt_meter_cust_data.sync_ui_soc_imm = 0;
#endif				/* #if defined(SYNC_UI_SOC_IMM) */
#if defined(MTK_ENABLE_AGING_ALGORITHM)
	batt_meter_cust_data.mtk_enable_aging_algorithm = 1;
#else				/* #if defined(MTK_ENABLE_AGING_ALGORITHM) */
	batt_meter_cust_data.mtk_enable_aging_algorithm = 0;
#endif				/* #if defined(MTK_ENABLE_AGING_ALGORITHM) */
#if defined(MD_SLEEP_CURRENT_CHECK)
	batt_meter_cust_data.md_sleep_current_check = 1;
#else				/* #if defined(MD_SLEEP_CURRENT_CHECK) */
	batt_meter_cust_data.md_sleep_current_check = 0;
#endif				/* #if defined(MD_SLEEP_CURRENT_CHECK) */
#if defined(Q_MAX_BY_CURRENT)
	batt_meter_cust_data.q_max_by_current = 1;
#elif defined(Q_MAX_BY_SYS)
	batt_meter_cust_data.q_max_by_current = 2;
#else				/* #if defined(Q_MAX_BY_CURRENT) */
	batt_meter_cust_data.q_max_by_current = 0;
#endif				/* #if defined(Q_MAX_BY_CURRENT) */
	batt_meter_cust_data.q_max_sys_voltage = Q_MAX_SYS_VOLTAGE;

#if defined(SHUTDOWN_GAUGE0)
	batt_meter_cust_data.shutdown_gauge0 = 1;
#else				/* #if defined(SHUTDOWN_GAUGE0) */
	batt_meter_cust_data.shutdown_gauge0 = 0;
#endif				/* #if defined(SHUTDOWN_GAUGE0) */
#if defined(SHUTDOWN_GAUGE1_XMINS)
	batt_meter_cust_data.shutdown_gauge1_xmins = 1;
#else				/* #if defined(SHUTDOWN_GAUGE1_XMINS) */
	batt_meter_cust_data.shutdown_gauge1_xmins = 0;
#endif				/* #if defined(SHUTDOWN_GAUGE1_XMINS) */
	batt_meter_cust_data.shutdown_gauge1_mins = SHUTDOWN_GAUGE1_MINS;

	batt_meter_cust_data.min_charging_smooth_time = FG_MIN_CHARGING_SMOOTH_TIME;

	batt_meter_cust_data.apsleep_battery_voltage_compensate =
	    APSLEEP_BATTERY_VOLTAGE_COMPENSATE;

	return 0;
}

int __batt_meter_init_cust_data_from_dt(struct platform_device *dev)
{
	struct device_node *np = dev->dev.of_node;
	unsigned int val;
	unsigned int addr;
	int idx;
	int size = 0;

	bm_debug("__batt_meter_init_cust_data_from_dt\n");


	if (of_property_read_u32(np, "batt_temperature_table_size", &val) == 0)
		size = (int)val;
	else
		bm_err("Get batt_temperature_table_size failed\n");

	idx = 0;
	while (!of_property_read_u32_index(np, "batt_temperature_table", idx, &addr)) {
		if (!of_property_read_u32_index(np, "batt_temperature_table", idx+1, &val)) {
			Batt_Temperature_Table[idx/2].BatteryTemp = addr;
			Batt_Temperature_Table[idx/2].TemperatureR = val;
		} else
			bm_err("Get batt_temperature_table failed\n");

		idx = idx + 2;
		if (idx >= size * 2)
			break;
	}
	if (idx != size * 2)
		bm_err("Get batt_temperature_table failed\n");


	if (of_property_read_u32(np, "battery_profile_t0_size", &val) == 0)
		batt_meter_table_cust_data.battery_profile_t0_size = (int)val;
	else
		bm_err("Get battery_profile_t0_size failed\n");

	idx = 0;
	while (!of_property_read_u32_index(np, "battery_profile_t0", idx, &addr)) {
		if (!of_property_read_u32_index(np, "battery_profile_t0", idx+1, &val)) {
			batt_meter_table_cust_data.battery_profile_t0[idx/2].percentage = addr;
			batt_meter_table_cust_data.battery_profile_t0[idx/2].voltage = val;
		} else
			bm_err("Get battery_profile_t0 failed\n");

		idx = idx + 2;
		if (idx >= batt_meter_table_cust_data.battery_profile_t0_size * 2)
			break;
	}
	if (idx != batt_meter_table_cust_data.battery_profile_t0_size * 2)
		bm_err("Get battery_profile_t0 failed\n");


	if (of_property_read_u32(np, "battery_profile_t1_size", &val) == 0)
		batt_meter_table_cust_data.battery_profile_t1_size = (int)val;
	else
		bm_err("Get battery_profile_t1_size failed\n");

	idx = 0;
	while (!of_property_read_u32_index(np, "battery_profile_t1", idx, &addr)) {
		if (!of_property_read_u32_index(np, "battery_profile_t1", idx+1, &val)) {
			batt_meter_table_cust_data.battery_profile_t1[idx/2].percentage = addr;
			batt_meter_table_cust_data.battery_profile_t1[idx/2].voltage = val;
		} else
			bm_err("Get battery_profile_t1 failed\n");

		idx = idx + 2;
		if (idx >= batt_meter_table_cust_data.battery_profile_t1_size * 2)
			break;
	}
	if (idx != batt_meter_table_cust_data.battery_profile_t1_size * 2)
		bm_err("Get battery_profile_t1 failed\n");


	if (of_property_read_u32(np, "battery_profile_t2_size", &val) == 0)
		batt_meter_table_cust_data.battery_profile_t2_size = (int)val;
	else
		bm_err("Get battery_profile_t2_size failed\n");

	idx = 0;
	while (!of_property_read_u32_index(np, "battery_profile_t2", idx, &addr)) {
		if (!of_property_read_u32_index(np, "battery_profile_t2", idx+1, &val)) {
			batt_meter_table_cust_data.battery_profile_t2[idx/2].percentage = addr;
			batt_meter_table_cust_data.battery_profile_t2[idx/2].voltage = val;
		} else
			bm_err("Get battery_profile_t2 failed\n");

		idx = idx + 2;
		if (idx >= batt_meter_table_cust_data.battery_profile_t2_size * 2)
			break;
	}
	if (idx != batt_meter_table_cust_data.battery_profile_t2_size * 2)
		bm_err("Get battery_profile_t2 failed\n");


	if (of_property_read_u32(np, "battery_profile_t3_size", &val) == 0)
		batt_meter_table_cust_data.battery_profile_t3_size = (int)val;
	else
		bm_err("Get battery_profile_t3_size failed\n");

	idx = 0;
	while (!of_property_read_u32_index(np, "battery_profile_t3", idx, &addr)) {
		if (!of_property_read_u32_index(np, "battery_profile_t3", idx+1, &val)) {
			batt_meter_table_cust_data.battery_profile_t3[idx/2].percentage = addr;
			batt_meter_table_cust_data.battery_profile_t3[idx/2].voltage = val;
		} else
			bm_err("Get battery_profile_t3 failed\n");

		idx = idx + 2;
		if (idx >= batt_meter_table_cust_data.battery_profile_t3_size * 2)
			break;
	}
	if (idx != batt_meter_table_cust_data.battery_profile_t3_size * 2)
		bm_err("Get battery_profile_t3 failed\n");


	if (of_property_read_u32(np, "r_profile_t0_size", &val) == 0)
		batt_meter_table_cust_data.r_profile_t0_size = (int)val;
	else
		bm_err("Get r_profile_t0_size failed\n");

	idx = 0;
	while (!of_property_read_u32_index(np, "r_profile_t0", idx, &addr)) {
		if (!of_property_read_u32_index(np, "r_profile_t0", idx+1, &val)) {
			batt_meter_table_cust_data.r_profile_t0[idx/2].resistance = addr;
			batt_meter_table_cust_data.r_profile_t0[idx/2].voltage = val;
			} else
				bm_err("Get r_profile_t0 failed\n");

		idx = idx + 2;
		if (idx >= batt_meter_table_cust_data.r_profile_t0_size * 2)
			break;
	}
	if (idx != batt_meter_table_cust_data.r_profile_t0_size * 2)
		bm_err("Get r_profile_t0 failed\n");


	if (of_property_read_u32(np, "r_profile_t1_size", &val) == 0)
		batt_meter_table_cust_data.r_profile_t1_size = (int)val;
	else
		bm_err("Get r_profile_t1_size failed\n");

	idx = 0;
	while (!of_property_read_u32_index(np, "r_profile_t1", idx, &addr)) {
		if (!of_property_read_u32_index(np, "r_profile_t1", idx+1, &val)) {
			batt_meter_table_cust_data.r_profile_t1[idx/2].resistance = addr;
			batt_meter_table_cust_data.r_profile_t1[idx/2].voltage = val;
			} else
				bm_err("Get r_profile_t1 failed\n");

		idx = idx + 2;
		if (idx >= batt_meter_table_cust_data.r_profile_t1_size * 2)
			break;
	}
	if (idx != batt_meter_table_cust_data.r_profile_t1_size * 2)
		bm_err("Get r_profile_t1 failed\n");


	if (of_property_read_u32(np, "r_profile_t2_size", &val) == 0)
		batt_meter_table_cust_data.r_profile_t2_size = (int)val;
	else
		bm_err("Get r_profile_t2_size failed\n");

	idx = 0;
	while (!of_property_read_u32_index(np, "r_profile_t2", idx, &addr)) {
		if (!of_property_read_u32_index(np, "r_profile_t2", idx+1, &val)) {
			batt_meter_table_cust_data.r_profile_t2[idx/2].resistance = addr;
			batt_meter_table_cust_data.r_profile_t2[idx/2].voltage = val;
			} else
				bm_err("Get r_profile_t2 failed\n");

		idx = idx + 2;
		if (idx >= batt_meter_table_cust_data.r_profile_t2_size * 2)
			break;
	}
	if (idx != batt_meter_table_cust_data.r_profile_t2_size * 2)
		bm_err("Get r_profile_t2 failed\n");


	if (of_property_read_u32(np, "r_profile_t3_size", &val) == 0)
		batt_meter_table_cust_data.r_profile_t3_size = (int)val;
	else
		bm_err("Get r_profile_t3_size failed\n");

	idx = 0;
	while (!of_property_read_u32_index(np, "r_profile_t3", idx, &addr)) {
		if (!of_property_read_u32_index(np, "r_profile_t3", idx+1, &val)) {
			batt_meter_table_cust_data.r_profile_t3[idx/2].resistance = addr;
			batt_meter_table_cust_data.r_profile_t3[idx/2].voltage = val;
			} else
				bm_err("Get r_profile_t3 failed\n");

		idx = idx + 2;
		if (idx >= batt_meter_table_cust_data.r_profile_t3_size * 2)
			break;
	}
	if (idx != batt_meter_table_cust_data.r_profile_t3_size * 2)
		bm_err("Get r_profile_t3 failed\n");


	if (of_property_read_u32(np, "hw_fg_force_use_sw_ocv", &val) == 0) {
		batt_meter_cust_data.hw_fg_force_use_sw_ocv = (int)val;
		bm_debug("Get hw_fg_force_use_sw_ocv: %d\n",
			 batt_meter_cust_data.hw_fg_force_use_sw_ocv);
	} else {
		bm_err("Get hw_fg_force_use_sw_ocv failed\n");
	}

	if (of_property_read_u32(np, "r_bat_sense", &val) == 0) {
		batt_meter_cust_data.r_bat_sense = (int)val;
		g_R_BAT_SENSE = batt_meter_cust_data.r_bat_sense;
		bm_debug("Get r_bat_sense: %d\n", batt_meter_cust_data.r_bat_sense);
	} else {
		bm_err("Get r_bat_sense failed\n");
	}

	if (of_property_read_u32(np, "r_i_sense", &val) == 0) {
		batt_meter_cust_data.r_i_sense = (int)val;
		g_R_I_SENSE = batt_meter_cust_data.r_i_sense;
		bm_debug("Get r_i_sense: %d\n", batt_meter_cust_data.r_i_sense);
	} else {
		bm_err("Get r_i_sense failed\n");
	}

	if (of_property_read_u32(np, "r_charger_1", &val) == 0) {
		batt_meter_cust_data.r_charger_1 = (int)val;
		g_R_CHARGER_1 = batt_meter_cust_data.r_charger_1;
		bm_debug("Get r_charger_1: %d\n", batt_meter_cust_data.r_charger_1);
	} else {
		bm_err("Get r_charger_1 failed\n");
	}

	if (of_property_read_u32(np, "r_charger_2", &val) == 0) {
		batt_meter_cust_data.r_charger_2 = (int)val;
		g_R_CHARGER_2 = batt_meter_cust_data.r_charger_2;
		bm_debug("Get r_charger_2: %d\n", batt_meter_cust_data.r_charger_2);
	} else {
		bm_err("Get r_charger_2 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t0", &val) == 0) {
		batt_meter_cust_data.temperature_t0 = (int)val;
		bm_debug("Get temperature_t0: %d\n", batt_meter_cust_data.temperature_t0);
	} else {
		bm_err("Get temperature_t0 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t1", &val) == 0) {
		batt_meter_cust_data.temperature_t1 = (int)val;
		bm_debug("Get temperature_t1: %d\n", batt_meter_cust_data.temperature_t1);
	} else {
		bm_err("Get temperature_t1 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t2", &val) == 0) {
		batt_meter_cust_data.temperature_t2 = (int)val;
		bm_debug("Get temperature_t2: %d\n", batt_meter_cust_data.temperature_t2);
	} else {
		bm_err("Get temperature_t2 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t3", &val) == 0) {
		batt_meter_cust_data.temperature_t3 = (int)val;
		bm_debug("Get temperature_t3: %d\n", batt_meter_cust_data.temperature_t3);
	} else {
		bm_err("Get temperature_t3 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t", &val) == 0) {
		batt_meter_cust_data.temperature_t = (int)val;
		bm_debug("Get temperature_t: %d\n", batt_meter_cust_data.temperature_t);
	} else {
		bm_err("Get temperature_t failed\n");
	}

	if (of_property_read_u32(np, "fg_meter_resistance", &val) == 0) {
		batt_meter_cust_data.fg_meter_resistance = (int)val;
		bm_debug("Get fg_meter_resistance: %d\n", batt_meter_cust_data.fg_meter_resistance);
	} else {
		bm_err("Get fg_meter_resistance failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_50", &val) == 0) {
		batt_meter_cust_data.q_max_pos_50 = (int)val;
		bm_debug("Get q_max_pos_50: %d\n", batt_meter_cust_data.q_max_pos_50);
	} else {
		bm_err("Get q_max_pos_50 failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_25", &val) == 0) {
		batt_meter_cust_data.q_max_pos_25 = (int)val;
		bm_debug("Get q_max_pos_25: %d\n", batt_meter_cust_data.q_max_pos_25);
	} else {
		bm_err("Get q_max_pos_25 failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_0", &val) == 0) {
		batt_meter_cust_data.q_max_pos_0 = (int)val;
		bm_debug("Get q_max_pos_0: %d\n", batt_meter_cust_data.q_max_pos_0);
	} else {
		bm_err("Get q_max_pos_0 failed\n");
	}

	if (of_property_read_u32(np, "q_max_neg_10", &val) == 0) {
		batt_meter_cust_data.q_max_neg_10 = (int)val;
		bm_debug("Get q_max_neg_10: %d\n", batt_meter_cust_data.q_max_neg_10);
	} else {
		bm_err("Get q_max_neg_10 failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_50_h_current", &val) == 0) {
		batt_meter_cust_data.q_max_pos_50_h_current = (int)val;
		bm_debug("Get q_max_pos_50_h_current: %d\n",
			 batt_meter_cust_data.q_max_pos_50_h_current);
	} else {
		bm_err("Get q_max_pos_50_h_current failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_25_h_current", &val) == 0) {
		batt_meter_cust_data.q_max_pos_25_h_current = (int)val;
		bm_debug("Get q_max_pos_25_h_current: %d\n",
			 batt_meter_cust_data.q_max_pos_25_h_current);
	} else {
		bm_err("Get q_max_pos_25_h_current failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_0_h_current", &val) == 0) {
		batt_meter_cust_data.q_max_pos_0_h_current = (int)val;
		bm_debug("Get q_max_pos_0_h_current: %d\n",
			 batt_meter_cust_data.q_max_pos_0_h_current);
	} else {
		bm_err("Get q_max_pos_0_h_current failed\n");
	}

	if (of_property_read_u32(np, "q_max_neg_10_h_current", &val) == 0) {
		batt_meter_cust_data.q_max_neg_10_h_current = (int)val;
		bm_debug("Get q_max_neg_10_h_current: %d\n",
			 batt_meter_cust_data.q_max_neg_10_h_current);
	} else {
		bm_err("Get q_max_neg_10_h_current failed\n");
	}

	if (of_property_read_u32(np, "oam_d5", &val) == 0) {
		batt_meter_cust_data.oam_d5 = (int)val;
		bm_debug("Get oam_d5: %d\n", batt_meter_cust_data.oam_d5);
	} else {
		bm_err("Get oam_d5 failed\n");
	}

	if (of_property_read_u32(np, "change_tracking_point", &val) == 0) {
		batt_meter_cust_data.change_tracking_point = (int)val;
		bm_debug("Get change_tracking_point: %d\n",
			 batt_meter_cust_data.change_tracking_point);
	} else {
		bm_err("Get change_tracking_point failed\n");
	}

	if (of_property_read_u32(np, "cust_tracking_point", &val) == 0) {
		batt_meter_cust_data.cust_tracking_point = (int)val;
		g_tracking_point = batt_meter_cust_data.cust_tracking_point;
		bm_debug("Get cust_tracking_point: %d\n", batt_meter_cust_data.cust_tracking_point);
	} else {
		bm_err("Get cust_tracking_point failed\n");
	}

	if (of_property_read_u32(np, "cust_r_sense", &val) == 0) {
		batt_meter_cust_data.cust_r_sense = (int)val;
		g_CUST_R_SENSE = (int)val;
		bm_debug("Get cust_r_sense: %d\n", batt_meter_cust_data.cust_r_sense);
	} else {
		bm_err("Get cust_r_sense failed\n");
	}

	if (of_property_read_u32(np, "cust_hw_cc", &val) == 0) {
		batt_meter_cust_data.cust_hw_cc = (int)val;
		bm_debug("Get cust_hw_cc: %d\n", batt_meter_cust_data.cust_hw_cc);
	} else {
		bm_err("Get cust_hw_cc failed\n");
	}

	if (of_property_read_u32(np, "aging_tuning_value", &val) == 0) {
		batt_meter_cust_data.aging_tuning_value = (int)val;
		bm_debug("Get aging_tuning_value: %d\n", batt_meter_cust_data.aging_tuning_value);
	} else {
		bm_err("Get aging_tuning_value failed\n");
	}

	if (of_property_read_u32(np, "cust_r_fg_offset", &val) == 0) {
		batt_meter_cust_data.cust_r_fg_offset = (int)val;
		g_CUST_R_FG_OFFSET = (int)val;
		bm_debug("Get cust_r_fg_offset: %d\n", batt_meter_cust_data.cust_r_fg_offset);
	} else {
		bm_err("Get cust_r_fg_offset failed\n");
	}

	if (of_property_read_u32(np, "ocv_board_compesate", &val) == 0) {
		batt_meter_cust_data.ocv_board_compesate = (int)val;
		bm_debug("Get ocv_board_compesate: %d\n", batt_meter_cust_data.ocv_board_compesate);
	} else {
		bm_err("Get ocv_board_compesate failed\n");
	}

	if (of_property_read_u32(np, "r_fg_board_base", &val) == 0) {
		batt_meter_cust_data.r_fg_board_base = (int)val;
		bm_debug("Get r_fg_board_base: %d\n", batt_meter_cust_data.r_fg_board_base);
	} else {
		bm_err("Get r_fg_board_base failed\n");
	}

	if (of_property_read_u32(np, "r_fg_board_slope", &val) == 0) {
		batt_meter_cust_data.r_fg_board_slope = (int)val;
		bm_debug("Get r_fg_board_slope: %d\n", batt_meter_cust_data.r_fg_board_slope);
	} else {
		bm_err("Get r_fg_board_slope failed\n");
	}

	if (of_property_read_u32(np, "car_tune_value", &val) == 0) {
		batt_meter_cust_data.car_tune_value = (int)val;
		bm_debug("Get car_tune_value: %d\n", batt_meter_cust_data.car_tune_value);
	} else {
		bm_err("Get car_tune_value failed\n");
	}

	if (of_property_read_u32(np, "current_detect_r_fg", &val) == 0) {
		batt_meter_cust_data.current_detect_r_fg = (int)val;
		bm_debug("Get current_detect_r_fg: %d\n", batt_meter_cust_data.current_detect_r_fg);
	} else {
		bm_err("Get current_detect_r_fg failed\n");
	}

	if (of_property_read_u32(np, "minerroroffset", &val) == 0) {
		batt_meter_cust_data.minerroroffset = (int)val;
		bm_debug("Get minerroroffset: %d\n", batt_meter_cust_data.minerroroffset);
	} else {
		bm_err("Get minerroroffset failed\n");
	}

	if (of_property_read_u32(np, "fg_vbat_average_size", &val) == 0) {
		batt_meter_cust_data.fg_vbat_average_size = (int)val;
		bm_debug("Get fg_vbat_average_size: %d\n",
			 batt_meter_cust_data.fg_vbat_average_size);
	} else {
		bm_err("Get fg_vbat_average_size failed\n");
	}

	if (of_property_read_u32(np, "r_fg_value", &val) == 0) {
		batt_meter_cust_data.r_fg_value = (int)val;
		g_R_FG_VALUE = (int)val;
		bm_debug("Get r_fg_value: %d\n", batt_meter_cust_data.r_fg_value);
	} else {
		bm_err("Get r_fg_value failed\n");
	}

	/* TODO: update dt for new parameters */

	if (of_property_read_u32(np, "difference_hwocv_rtc", &val) == 0) {
		batt_meter_cust_data.difference_hwocv_rtc = (int)val;
		bm_debug("Get difference_hwocv_rtc: %d\n",
			 batt_meter_cust_data.difference_hwocv_rtc);
	} else {
		bm_err("Get difference_hwocv_rtc failed\n");
	}

	if (of_property_read_u32(np, "difference_hwocv_swocv", &val) == 0) {
		batt_meter_cust_data.difference_hwocv_swocv = (int)val;
		bm_debug("Get difference_hwocv_swocv: %d\n",
			 batt_meter_cust_data.difference_hwocv_swocv);
	} else {
		bm_err("Get difference_hwocv_swocv failed\n");
	}

	if (of_property_read_u32(np, "difference_swocv_rtc", &val) == 0) {
		batt_meter_cust_data.difference_swocv_rtc = (int)val;
		bm_debug("Get difference_swocv_rtc: %d\n",
			 batt_meter_cust_data.difference_swocv_rtc);
	} else {
		bm_err("Get difference_swocv_rtc failed\n");
	}


	if (of_property_read_u32(np, "difference_vbat_rtc", &val) == 0) {
		batt_meter_cust_data.difference_vbat_rtc = (int)val;
		bm_debug("Get difference_vbat_rtc: %d\n",
			 batt_meter_cust_data.difference_vbat_rtc);
	} else {
		bm_err("Get difference_vbat_rtc failed\n");
	}


	if (of_property_read_u32(np, "difference_swocv_rtc_pos", &val) == 0) {
		batt_meter_cust_data.difference_swocv_rtc_pos = (int)val;
		bm_debug("Get difference_swocv_rtc_pos: %d\n",
			 batt_meter_cust_data.difference_swocv_rtc_pos);
	} else {
		bm_err("Get difference_swocv_rtc_pos failed\n");
	}


	if (of_property_read_u32(np, "embedded_battery", &val) == 0) {
		batt_meter_cust_data.embedded_battery = (int)val;
		bm_debug("Get embedded_battery: %d\n",
			 batt_meter_cust_data.embedded_battery);
	} else {
		bm_err("Get embedded_battery failed\n");
	}


	if (of_property_read_u32(np, "max_swocv", &val) == 0) {
		batt_meter_cust_data.max_swocv = (int)val;
		bm_debug("Get max_swocv: %d\n", batt_meter_cust_data.max_swocv);
	} else {
		bm_err("Get max_swocv failed\n");
	}

	if (of_property_read_u32(np, "max_hwocv", &val) == 0) {
		batt_meter_cust_data.max_hwocv = (int)val;
		bm_debug("Get max_hwocv: %d\n", batt_meter_cust_data.max_hwocv);
	} else {
		bm_err("Get max_hwocv failed\n");
	}

	if (of_property_read_u32(np, "max_vbat", &val) == 0) {
		batt_meter_cust_data.max_vbat = (int)val;
		bm_debug("Get max_vbat: %d\n", batt_meter_cust_data.max_vbat);
	} else {
		bm_err("Get max_vbat failed\n");
	}

	if (of_property_read_u32(np, "difference_hwocv_vbat", &val) == 0) {
		batt_meter_cust_data.difference_hwocv_vbat = (int)val;
		bm_debug("Get difference_hwocv_vbat: %d\n",
			 batt_meter_cust_data.difference_hwocv_vbat);
	} else {
		bm_err("Get difference_hwocv_vbat failed\n");
	}


	if (of_property_read_u32(np, "fixed_tbat_25", &val) == 0) {
		batt_meter_cust_data.fixed_tbat_25 = (int)val;
		bm_debug("Get fixed_tbat_25: %d\n", batt_meter_cust_data.fixed_tbat_25);
	} else {
		bm_err("Get fixed_tbat_25 failed\n");
	}



	if (of_property_read_u32(np, "vbat_normal_wakeup", &val) == 0) {
		batt_meter_cust_data.vbat_normal_wakeup = (int)val;
		bm_debug("Get vbat_normal_wakeup: %d\n", batt_meter_cust_data.vbat_normal_wakeup);
	} else {
		bm_err("Get vbat_normal_wakeup failed\n");
	}

	if (of_property_read_u32(np, "vbat_low_power_wakeup", &val) == 0) {
		batt_meter_cust_data.vbat_low_power_wakeup = (int)val;
		bm_debug("Get vbat_low_power_wakeup: %d\n",
			 batt_meter_cust_data.vbat_low_power_wakeup);
	} else {
		bm_err("Get vbat_low_power_wakeup failed\n");
	}

	if (of_property_read_u32(np, "normal_wakeup_period", &val) == 0) {
		batt_meter_cust_data.normal_wakeup_period = (int)val;
		bm_debug("Get normal_wakeup_period: %d\n",
			 batt_meter_cust_data.normal_wakeup_period);
	} else {
		bm_err("Get normal_wakeup_period failed\n");
	}

	if (of_property_read_u32(np, "low_power_wakeup_period", &val) == 0) {
		batt_meter_cust_data.low_power_wakeup_period = (int)val;
		bm_debug("Get low_power_wakeup_period: %d\n",
			 batt_meter_cust_data.low_power_wakeup_period);
	} else {
		bm_err("Get low_power_wakeup_period failed\n");
	}

	if (of_property_read_u32(np, "close_poweroff_wakeup_period", &val) == 0) {
		batt_meter_cust_data.close_poweroff_wakeup_period = (int)val;
		bm_debug("Get close_poweroff_wakeup_period: %d\n",
			 batt_meter_cust_data.close_poweroff_wakeup_period);
	} else {
		bm_err("Get close_poweroff_wakeup_period failed\n");
	}


	if (of_property_read_u32(np, "init_soc_by_sw_soc", &val) == 0) {
		batt_meter_cust_data.init_soc_by_sw_soc = (int)val;
		bm_debug("Get init_soc_by_sw_soc: %d\n", batt_meter_cust_data.init_soc_by_sw_soc);
	} else {
		bm_err("Get init_soc_by_sw_soc failed\n");
	}

	if (of_property_read_u32(np, "sync_ui_soc_imm", &val) == 0) {
		batt_meter_cust_data.sync_ui_soc_imm = (int)val;
		bm_debug("Get sync_ui_soc_imm: %d\n", batt_meter_cust_data.sync_ui_soc_imm);
	} else {
		bm_err("Get sync_ui_soc_imm failed\n");
	}

	if (of_property_read_u32(np, "mtk_enable_aging_algorithm", &val) == 0) {
		batt_meter_cust_data.mtk_enable_aging_algorithm = (int)val;
		bm_debug("Get mtk_enable_aging_algorithm: %d\n",
			 batt_meter_cust_data.mtk_enable_aging_algorithm);
	} else {
		bm_err("Get mtk_enable_aging_algorithm failed\n");
	}

	if (of_property_read_u32(np, "md_sleep_current_check", &val) == 0) {
		batt_meter_cust_data.md_sleep_current_check = (int)val;
		bm_debug("Get md_sleep_current_check: %d\n",
			 batt_meter_cust_data.md_sleep_current_check);
	} else {
		bm_err("Get md_sleep_current_check failed\n");
	}

	if (of_property_read_u32(np, "q_max_by_current", &val) == 0) {
		if ((int)val == 1) {
			batt_meter_cust_data.q_max_by_current = 1;
			bm_debug("Get q_max_by_current: %d\n", batt_meter_cust_data.q_max_by_current);
		}
	} else {
		bm_err("Get q_max_by_current failed\n");
	}

	if (of_property_read_u32(np, "q_max_by_sys", &val) == 0) {
		if (batt_meter_cust_data.q_max_by_current != 1 && (int)val == 1) {
			batt_meter_cust_data.q_max_by_current = 2;
			bm_debug("Get q_max_by_current: %d\n", batt_meter_cust_data.q_max_by_current);
		}
	} else {
		bm_err("Get q_max_by_current failed\n");
	}

	if (of_property_read_u32(np, "q_max_sys_voltage", &val) == 0) {
		batt_meter_cust_data.q_max_sys_voltage = (int)val;
		bm_debug("Get q_max_sys_voltage: %d\n", batt_meter_cust_data.q_max_sys_voltage);
	} else {
		bm_err("Get q_max_sys_voltage failed\n");
	}

	if (of_property_read_u32(np, "shutdown_gauge0", &val) == 0) {
		batt_meter_cust_data.shutdown_gauge0 = (int)val;
		shutdown_gauge0 = (int)val;
		bm_debug("Get shutdown_gauge0: %d\n", batt_meter_cust_data.shutdown_gauge0);
	} else {
		bm_err("Get shutdown_gauge0 failed\n");
	}

	if (of_property_read_u32(np, "shutdown_gauge1_xmins", &val) == 0) {
		batt_meter_cust_data.shutdown_gauge1_xmins = (int)val;
		shutdown_gauge1_xmins = (int)val;
		bm_debug("Get shutdown_gauge1_xmins: %d\n",
			 batt_meter_cust_data.shutdown_gauge1_xmins);
	} else {
		bm_err("Get shutdown_gauge1_xmins failed\n");
	}

	if (of_property_read_u32(np, "shutdown_gauge1_mins", &val) == 0) {
		batt_meter_cust_data.shutdown_gauge1_mins = (int)val;
		shutdown_gauge1_mins = (int)val;
		bm_debug("Get shutdown_gauge1_mins: %d\n",
			 batt_meter_cust_data.shutdown_gauge1_mins);
	} else {
		bm_err("Get shutdown_gauge1_mins failed\n");
	}

	if (of_property_read_u32(np, "shutdown_system_voltage", &val) == 0) {
		batt_meter_cust_data.shutdown_system_voltage = (int)val;
		shutdown_system_voltage = (int)val;
		bm_debug("Get shutdown_system_voltage: %d\n",
			batt_meter_cust_data.shutdown_system_voltage);
	} else {
		bm_err("Get shutdown_system_voltage failed\n");
	}


	if (of_property_read_u32(np, "recharge_tolerance", &val) == 0) {
		batt_meter_cust_data.recharge_tolerance = (int)val;
		bm_debug("Get recharge_tolerance: %d\n",
			batt_meter_cust_data.recharge_tolerance);
	} else {
		bm_err("Get recharge_tolerance failed\n");
	}

	if (of_property_read_u32(np, "batterypseudo100", &val) == 0) {
		batt_meter_cust_data.batterypseudo100 = (int)val;
		bm_debug("Get batterypseudo100: %d\n",
			batt_meter_cust_data.batterypseudo100);
	} else {
		bm_err("Get batterypseudo100 failed\n");
	}

	if (of_property_read_u32(np, "batterypseudo1", &val) == 0) {
		batt_meter_cust_data.batterypseudo1 = (int)val;
		bm_debug("Get batterypseudo1: %d\n",
			batt_meter_cust_data.batterypseudo1);
	} else {
		bm_err("Get batterypseudo1 failed\n");
	}

#if defined(SOC_BY_HW_FG)
	batt_meter_cust_data.soc_flow = HW_FG;
#elif defined(SOC_BY_SW_FG)
	batt_meter_cust_data.soc_flow = SW_FG;
#elif defined(SOC_BY_AUXADC)
	batt_meter_cust_data.soc_flow = AUXADC;
#endif

	batt_meter_cust_data.suspend_current_threshold = SUSPEND_CURRENT_CHECK_THRESHOLD;
	batt_meter_cust_data.ocv_check_time = OCV_RECOVER_TIME;

	batt_meter_cust_data.min_charging_smooth_time = FG_MIN_CHARGING_SMOOTH_TIME;

	batt_meter_cust_data.apsleep_battery_voltage_compensate =
		APSLEEP_BATTERY_VOLTAGE_COMPENSATE;

	if (of_property_read_u32(np, "rbat_pull_up_r", &val) == 0) {
		g_RBAT_PULL_UP_R = (int)val;
		bm_debug("Get rbat_pull_up_r: %d\n",
			g_RBAT_PULL_UP_R);
	} else {
		bm_err("Get rbat_pull_up_r failed\n");
	}

	if (of_property_read_u32(np, "rbat_pull_up_volt", &val) == 0) {
		g_RBAT_PULL_UP_VOLT = (int)val;
		bm_debug("Get rbat_pull_up_volt: %d\n",
			g_RBAT_PULL_UP_VOLT);
	} else {
		bm_err("Get rbat_pull_up_volt failed\n");
	}


	if (of_property_read_u32(np, "difference_voltage_update", &val) == 0) {
		difference_voltage_update = (int)val;
		bm_debug("Get difference_voltage_update: %d\n",
			difference_voltage_update);
	} else {
		bm_err("Get difference_voltage_update failed\n");
	}


	if (of_property_read_u32(np, "aging1_load_soc", &val) == 0) {
		aging1_load_soc = (int)val;
		bm_debug("Get aging1_load_soc: %d\n",
			aging1_load_soc);
	} else {
		bm_err("Get aging1_load_soc failed\n");
	}

	if (of_property_read_u32(np, "aging1_update_soc", &val) == 0) {
		aging1_update_soc = (int)val;
		bm_debug("Get aging1_update_soc: %d\n",
			aging1_update_soc);
	} else {
		bm_err("Get aging1_update_soc failed\n");
	}


	if (of_property_read_u32(np, "charge_tracking_time", &val) == 0) {
		charge_tracking_time = (int)val;
		bm_debug("Get charge_tracking_time: %d\n",
			charge_tracking_time);
	} else {
		bm_err("Get charge_tracking_time failed\n");
	}

	if (of_property_read_u32(np, "discharge_tracking_time", &val) == 0) {
		discharge_tracking_time = (int)val;
		bm_debug("Get discharge_tracking_time: %d\n",
			discharge_tracking_time);
	} else {
		bm_err("Get discharge_tracking_time failed\n");
	}

	if (of_property_read_u32(np, "cust_min_uisoc_percentage", &val) == 0) {
		batt_meter_cust_data.cust_min_uisoc_percentage = (int)val;
		bm_debug("Get cust_min_uisoc_percentage: %d\n",
			batt_meter_cust_data.cust_min_uisoc_percentage);
	} else {
		bm_err("Get cust_min_uisoc_percentage failed\n");
	}

	return 0;
}

int batt_meter_init_cust_data(struct platform_device *dev)
{
	/* #ifdef CONFIG_OF */
	/* return __batt_meter_init_cust_data_from_dt(dev); */
	/* #else */
	__batt_init_cust_data_from_cust_header();
	/*return __batt_meter_init_cust_data_from_cust_header(dev);*/
	return __batt_meter_init_cust_data_from_dt(dev);
	/* #endif */
}

kal_bool mt_battery_check_battery_full(void)
{
	union power_supply_propval val;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, val);
	bm_info("[mt_battery_check_battery_full] =%d\n", val.intval);

	if (val.intval == POWER_SUPPLY_STATUS_FULL)
		return KAL_TRUE;

	return KAL_FALSE;
}

void battery_meter_set_init_flag(kal_bool flag)
{
	init_flag = flag;
}

void battery_meter_reset_sleep_time(void)
{
	g_sleep_total_time.tv_sec = 0;
	g_sleep_total_time.tv_nsec = 0;
}

void mt_battery_update_time(struct timespec *pre_time, BATTERY_TIME_ENUM duration_type)
{
	struct timespec time;

	time.tv_sec = 0;
	time.tv_nsec = 0;
	get_monotonic_boottime(&time);

	battery_duration_time[duration_type] = timespec_sub(time, *pre_time);

	bm_err("[Battery] mt_battery_update_duration_time , last_time=%d current_time=%d duration=%d\n",
		    (int)pre_time->tv_sec, (int)time.tv_sec,
		    (int)battery_duration_time[duration_type].tv_sec);

	pre_time->tv_sec = time.tv_sec;
	pre_time->tv_nsec = time.tv_nsec;

}

unsigned int mt_battery_get_duration_time(BATTERY_TIME_ENUM duration_type)
{
	return battery_duration_time[duration_type].tv_sec;
}

struct timespec mt_battery_get_duration_time_act(BATTERY_TIME_ENUM duration_type)
{
	return battery_duration_time[duration_type];
}

int get_r_fg_value(void)
{
	return (g_R_FG_VALUE + g_CUST_R_FG_OFFSET);
}

#ifdef MTK_MULTI_BAT_PROFILE_SUPPORT
int BattThermistorConverTemp(int Res)
{
	int i = 0;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -200, TMP1 = 0, TMP2 = 0;
	BATT_TEMPERATURE *batt_temperature_table = &Batt_Temperature_Table[g_fg_battery_id];

	if (Res >= batt_temperature_table[0].TemperatureR) {
		TBatt_Value = -20;
	} else if (Res <= batt_temperature_table[16].TemperatureR) {
		TBatt_Value = 60;
	} else {
		RES1 = batt_temperature_table[0].TemperatureR;
		TMP1 = batt_temperature_table[0].BatteryTemp;

		for (i = 0; i <= 16; i++) {
			if (Res >= batt_temperature_table[i].TemperatureR) {
				RES2 = batt_temperature_table[i].TemperatureR;
				TMP2 = batt_temperature_table[i].BatteryTemp;
				break;
			}

			RES1 = batt_temperature_table[i].TemperatureR;
			TMP1 = batt_temperature_table[i].BatteryTemp;

		}

		TBatt_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2)) / (RES1 - RES2);
	}

	return TBatt_Value;
}
#else
int BattThermistorConverTemp(int Res)
{
	int i = 0;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -200, TMP1 = 0, TMP2 = 0;

	if (Res >= Batt_Temperature_Table[0].TemperatureR) {
		TBatt_Value = -20;
	} else if (Res <= Batt_Temperature_Table[16].TemperatureR) {
		TBatt_Value = 60;
	} else {
		RES1 = Batt_Temperature_Table[0].TemperatureR;
		TMP1 = Batt_Temperature_Table[0].BatteryTemp;

		for (i = 0; i <= 16; i++) {
			if (Res >= Batt_Temperature_Table[i].TemperatureR) {
				RES2 = Batt_Temperature_Table[i].TemperatureR;
				TMP2 = Batt_Temperature_Table[i].BatteryTemp;
				break;
			}
			RES1 = Batt_Temperature_Table[i].TemperatureR;
			TMP1 = Batt_Temperature_Table[i].BatteryTemp;

		}

		TBatt_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2)) / (RES1 - RES2);
	}

	return TBatt_Value;
}

#endif

int BattVoltToTemp(int dwVolt)
{
	long long TRes_temp;
	long long TRes;
	int sBaTTMP = -100;

	/* TRes_temp = ((long long)g_RBAT_PULL_UP_R*(long long)dwVolt) / (g_RBAT_PULL_UP_VOLT-dwVolt); */
	/* TRes = (TRes_temp * (long long)RBAT_PULL_DOWN_R)/((long long)RBAT_PULL_DOWN_R - TRes_temp); */

	TRes_temp = (g_RBAT_PULL_UP_R * (long long)dwVolt);
#ifdef RBAT_PULL_UP_VOLT_BY_BIF
	do_div(TRes_temp, (pmic_get_vbif28_volt() - dwVolt));
	/* bm_debug("[RBAT_PULL_UP_VOLT_BY_BIF] vbif28:%d\n",pmic_get_vbif28_volt()); */
#else
	do_div(TRes_temp, (g_RBAT_PULL_UP_VOLT - dwVolt));
	bm_err("[BattVoltToTemp] TRes_temp %lld\n", TRes_temp);
#endif

#ifdef RBAT_PULL_DOWN_R
	TRes = (TRes_temp * RBAT_PULL_DOWN_R);
	do_div(TRes, abs(RBAT_PULL_DOWN_R - TRes_temp));
#else
	TRes = TRes_temp;
	bm_err("[BattVoltToTemp] TRes %lld\n", TRes);
#endif

	/* convert register to temperature */
	sBaTTMP = BattThermistorConverTemp((int)TRes);

#ifdef RBAT_PULL_UP_VOLT_BY_BIF
	bm_debug("[BattVoltToTemp] %d %d\n", g_RBAT_PULL_UP_R, pmic_get_vbif28_volt());
#else
	bm_err("[BattVoltToTemp] %d\n", g_RBAT_PULL_UP_R);
#endif

	return sBaTTMP;
}

int force_get_tbat(kal_bool update)
{
#if defined(CONFIG_POWER_EXT) || defined(FIXED_TBAT_25)
	bm_debug("[force_get_tbat] fixed TBAT=25 t\n");
	return 25;
#else
	int bat_temperature_volt = 0;
	int bat_temperature_val = 0;
	static int pre_bat_temperature_val = -1;
	int fg_r_value = 0;
	signed int fg_current_temp = 0;
	kal_bool fg_current_state = KAL_FALSE;
	int bat_temperature_volt_temp = 0;
	int ret = 0;

	if (update == KAL_TRUE || pre_bat_temperature_val == -1) {
		/* Get V_BAT_Temperature */
		bat_temperature_volt = 2;
		ret =
		    battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &bat_temperature_volt);

		if (bat_temperature_volt != 0) {
#if defined(SOC_BY_HW_FG)
			fg_r_value = get_r_fg_value();

			ret =
			    battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT,
					       &fg_current_temp);
			ret =
			    battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,
					       &fg_current_state);
			fg_current_temp = fg_current_temp / 10;

			if (fg_current_state == KAL_TRUE) {
				bat_temperature_volt_temp = bat_temperature_volt;
				bat_temperature_volt =
				    bat_temperature_volt - ((fg_current_temp * fg_r_value) / 1000);
			} else {
				bat_temperature_volt_temp = bat_temperature_volt;
				bat_temperature_volt =
				    bat_temperature_volt + ((fg_current_temp * fg_r_value) / 1000);
			}
#endif

			bat_temperature_val = BattVoltToTemp(bat_temperature_volt);
		}

#ifdef CONFIG_MTK_BIF_SUPPORT
		battery_charging_control(CHARGING_CMD_GET_BIF_TBAT, &bat_temperature_val);
#endif
	bm_err("[force_get_tbat] %d,%d,%d,%d,%d,%d\n",
		 bat_temperature_volt_temp, bat_temperature_volt, fg_current_state, fg_current_temp,
		 fg_r_value, bat_temperature_val);
		pre_bat_temperature_val = bat_temperature_val;
	} else {
		bat_temperature_val = pre_bat_temperature_val;
	}
	return bat_temperature_val;
#endif
}
EXPORT_SYMBOL(force_get_tbat);

#if defined(SOC_BY_HW_FG)
void mt_battery_set_init_vol(int init_voltage)
{
	BMT_status.bat_vol = init_voltage;
}


void fgauge_algo_run_get_init_data(void)
{

	int ret = 0;
	/*kal_bool charging_enable = KAL_FALSE;*/

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING) && !defined(SWCHR_POWER_PATH)
	if (LOW_POWER_OFF_CHARGING_BOOT != get_boot_mode())
#endif
		/*stop charging for vbat measurement */
		/*battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);*/

	msleep(50);
/* 1. Get Raw Data */
	gFG_voltage_init = battery_meter_get_battery_voltage(KAL_TRUE);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &gFG_current_init);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &gFG_Is_Charging_init);
	/*charging_enable = KAL_TRUE;*/
	/*battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);*/
	bm_info
	    ("1.[fgauge_algo_run_get_init_data](gFG_voltage_init %d, gFG_current_init %d, gFG_Is_Charging_init %d)\n",
	     gFG_voltage_init, gFG_current_init, gFG_Is_Charging_init);
	mt_battery_set_init_vol(gFG_voltage_init);
}
#endif


#if defined(SOC_BY_SW_FG)
void update_fg_dbg_tool_value(void)
{
}

void fgauge_algo_run_get_init_data(void)
{
	kal_bool charging_enable = KAL_FALSE;

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING) && !defined(SWCHR_POWER_PATH)
	if (LOW_POWER_OFF_CHARGING_BOOT != get_boot_mode())
#endif
		/*stop charging for vbat measurement */
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	msleep(50);
/* 1. Get Raw Data */
	gFG_voltage_init = battery_meter_get_battery_voltage(KAL_TRUE);
	gFG_current_init = FG_CURRENT_INIT_VALUE;
	gFG_Is_Charging_init = KAL_FALSE;
	charging_enable = KAL_TRUE;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
	bm_info
	    ("1.[fgauge_algo_run_get_init_data](gFG_voltage_init %d, gFG_current_init %d, gFG_Is_Charging_init %d)\n",
	     gFG_voltage_init, gFG_current_init, gFG_Is_Charging_init);
}
#endif


signed int get_dynamic_period(int first_use, int first_wakeup_time, int battery_capacity_level)
{
	if (wake_up_smooth_time == 0)
		g_spm_timer = NORMAL_WAKEUP_PERIOD;
	else
		g_spm_timer = wake_up_smooth_time;

	bm_info("get_dynamic_period: time:%d\n", g_spm_timer);

	return g_spm_timer;
}

/* ============================================================ // */
signed int battery_meter_get_battery_voltage(kal_bool update)
{
	int ret = 0;
	int val = 5;
	static int pre_val = -1;

	if (update == KAL_TRUE || pre_val == -1) {
		val = 5;	/* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &val);
		pre_val = val;
	} else {
		val = pre_val;
	}
	g_sw_vbat_temp = val;

#ifdef MTK_BATTERY_LIFETIME_DATA_SUPPORT
	if (g_sw_vbat_temp > gFG_max_voltage)
		gFG_max_voltage = g_sw_vbat_temp;

	if (g_sw_vbat_temp < gFG_min_voltage)
		gFG_min_voltage = g_sw_vbat_temp;
#endif

	return val;
}

int battery_meter_get_low_battery_interrupt_status(void)
{
	int is_lbat_int_trigger;
	int ret;

	ret =
	    battery_meter_ctrl(BATTERY_METER_CMD_GET_LOW_BAT_INTERRUPT_STATUS,
			       &is_lbat_int_trigger);

	if (ret != 0)
		return KAL_FALSE;

	return is_lbat_int_trigger;
}


signed int battery_meter_get_charging_current_imm(void)
{
#ifdef AUXADC_SUPPORT_IMM_CURRENT_MODE
	return PMIC_IMM_GetCurrent();
#else
	int ret;
	signed int ADC_I_SENSE = 1;	/* 1 measure time */
	signed int ADC_BAT_SENSE = 1;	/* 1 measure time */
	int ICharging = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &ADC_BAT_SENSE);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &ADC_I_SENSE);

	ICharging = (ADC_I_SENSE - ADC_BAT_SENSE + g_I_SENSE_offset) * 1000 / g_CUST_R_SENSE;
	return ICharging;
#endif

}

signed int battery_meter_get_charging_current(void)
{
#ifdef DISABLE_CHARGING_CURRENT_MEASURE
	return 0;
#elif defined(AUXADC_SUPPORT_IMM_CURRENT_MODE)
	return PMIC_IMM_GetCurrent();
#elif !defined(EXTERNAL_SWCHR_SUPPORT)
	signed int ADC_BAT_SENSE_tmp[20] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	signed int ADC_BAT_SENSE_sum = 0;
	signed int ADC_BAT_SENSE = 0;
	signed int ADC_I_SENSE_tmp[20] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	signed int ADC_I_SENSE_sum = 0;
	signed int ADC_I_SENSE = 0;
	int repeat = 20;
	int i = 0;
	int j = 0;
	signed int temp = 0;
	int ICharging = 0;
	int ret = 0;
	int val = 1;

	for (i = 0; i < repeat; i++) {
		val = 1;	/* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE, &val);
		ADC_BAT_SENSE_tmp[i] = val;

		val = 1;	/* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &val);
		ADC_I_SENSE_tmp[i] = val;

		ADC_BAT_SENSE_sum += ADC_BAT_SENSE_tmp[i];
		ADC_I_SENSE_sum += ADC_I_SENSE_tmp[i];
	}

	/* sorting    BAT_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_BAT_SENSE_tmp[j] < ADC_BAT_SENSE_tmp[i]) {
				temp = ADC_BAT_SENSE_tmp[j];
				ADC_BAT_SENSE_tmp[j] = ADC_BAT_SENSE_tmp[i];
				ADC_BAT_SENSE_tmp[i] = temp;
			}
		}
	}

	bm_trace("[g_Get_I_Charging:BAT_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		bm_trace("%d,", ADC_BAT_SENSE_tmp[i]);
	bm_trace("\r\n");

	/* sorting    I_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_I_SENSE_tmp[j] < ADC_I_SENSE_tmp[i]) {
				temp = ADC_I_SENSE_tmp[j];
				ADC_I_SENSE_tmp[j] = ADC_I_SENSE_tmp[i];
				ADC_I_SENSE_tmp[i] = temp;
			}
		}
	}

	bm_trace("[g_Get_I_Charging:I_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		bm_trace("%d,", ADC_I_SENSE_tmp[i]);
	bm_trace("\r\n");

	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[0];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[1];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[18];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[19];
	ADC_BAT_SENSE = ADC_BAT_SENSE_sum / (repeat - 4);

	bm_trace("[g_Get_I_Charging] ADC_BAT_SENSE=%d\r\n", ADC_BAT_SENSE);

	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[0];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[1];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[18];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[19];
	ADC_I_SENSE = ADC_I_SENSE_sum / (repeat - 4);

	bm_trace("[g_Get_I_Charging] ADC_I_SENSE(Before)=%d\r\n", ADC_I_SENSE);


	bm_trace("[g_Get_I_Charging] ADC_I_SENSE(After)=%d\r\n", ADC_I_SENSE);

	if (ADC_I_SENSE > ADC_BAT_SENSE)
		ICharging = (ADC_I_SENSE - ADC_BAT_SENSE + g_I_SENSE_offset) * 1000 / g_CUST_R_SENSE;
	else
		ICharging = 0;

	return ICharging;
#else
	return 0;
#endif
}

signed int battery_meter_get_car(void)
{
	int ret = 0;
	signed int val = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT, &val);
	if (val < 0)
		val = (val - 5) / 10;
	else
		val = (val + 5) / 10;

	return val;
}

signed int battery_meter_get_battery_temperature(void)
{
#ifdef MTK_BATTERY_LIFETIME_DATA_SUPPORT
	signed int batt_temp = force_get_tbat(KAL_TRUE);

	if (batt_temp > gFG_max_temperature)
		gFG_max_temperature = batt_temp;
	if (batt_temp < gFG_min_temperature)
		gFG_min_temperature = batt_temp;

	return batt_temp;
#else
	return force_get_tbat(KAL_TRUE);
#endif
}

signed int battery_meter_get_charger_voltage(void)
{
	int ret = 0;
	int val = 0;

	val = 5;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_CHARGER, &val);

	return val;
}

signed int battery_meter_get_battery_percentage(void)
{
#if defined(CONFIG_POWER_EXT)
	return 50;
#else

#if defined(SOC_BY_HW_FG)
	return gFG_capacity_by_c;
#endif
#endif
	return gFG_capacity_by_c;
}


signed int battery_meter_initial(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	static kal_bool meter_initilized = KAL_FALSE;

	mutex_lock(&FGADC_mutex);

	if (meter_initilized == KAL_FALSE) {
#if defined(SOC_BY_HW_FG)
		/* 1. HW initialization */
		battery_meter_ctrl(BATTERY_METER_CMD_HW_FG_INIT, NULL);

		if (wakeup_fg_algo(FG_MAIN) == -1) {
			/* fgauge_initialization(); */
			fgauge_algo_run_get_init_data();
			bm_err("[battery_meter_initial] SOC_BY_HW_FG not done\n");
		}
#endif

		meter_initilized = KAL_TRUE;
	}

	mutex_unlock(&FGADC_mutex);
	return 0;
#endif
}

signed int battery_meter_sync(signed int bat_i_sense_offset)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	g_I_SENSE_offset = bat_i_sense_offset;
	return 0;
#endif
}

signed int battery_meter_get_battery_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3987;
#else
	return gFG_voltage;
#endif
}

signed int battery_meter_get_battery_nPercent_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3700;
#else
	return gFG_15_vlot;	/* 15% zcv,  15% can be customized by 100-g_tracking_point */
#endif
}

signed int battery_meter_get_battery_nPercent_UI_SOC(void)
{
#if defined(CONFIG_POWER_EXT)
	return 15;
#else
	return g_tracking_point;	/* tracking point */
#endif
}

signed int battery_meter_get_tempR(signed int dwVolt)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int TRes;

	TRes = (g_RBAT_PULL_UP_R * dwVolt) / (g_RBAT_PULL_UP_VOLT - dwVolt);

	return TRes;
#endif
}

signed int battery_meter_get_tempV(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret = 0;
	int val = 0;

	val = 1;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &val);
	return val;
#endif
}

signed int battery_meter_get_VSense(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret = 0;
	int val = 0;

	val = 1;		/* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &val);
	return val;
#endif
}

/* ============================================================ // */
static ssize_t fgadc_log_write(struct file *filp, const char __user *buff,
			       size_t len, loff_t *data)
{
	char proc_fgadc_data;

	if ((len <= 0) || copy_from_user(&proc_fgadc_data, buff, 1)) {
		bm_debug("fgadc_log_write error.\n");
		return -EFAULT;
	}

	if (proc_fgadc_data == '1') {
		bm_debug("enable FGADC driver log system\n");
		Enable_FGADC_LOG = 1;
	} else if (proc_fgadc_data == '2') {
		bm_debug("enable FGADC driver log system:2\n");
		Enable_FGADC_LOG = 2;
	} else if (proc_fgadc_data == '3') {
		bm_debug("enable FGADC driver log system:3\n");
		Enable_FGADC_LOG = 3;
	} else if (proc_fgadc_data == '4') {
		bm_debug("enable FGADC driver log system:4\n");
		Enable_FGADC_LOG = 4;
	} else if (proc_fgadc_data == '5') {
		bm_debug("enable FGADC driver log system:5\n");
		Enable_FGADC_LOG = 5;
	} else if (proc_fgadc_data == '6') {
		bm_debug("enable FGADC driver log system:6\n");
		Enable_FGADC_LOG = 6;
	} else if (proc_fgadc_data == '7') {
		bm_debug("enable FGADC driver log system:7\n");
		Enable_FGADC_LOG = 7;
	} else if (proc_fgadc_data == '8') {
		bm_debug("enable FGADC driver log system:8\n");
		Enable_FGADC_LOG = 8;
	} else {
		bm_debug("Disable FGADC driver log system\n");
		Enable_FGADC_LOG = 0;
	}

	return len;
}

static const struct file_operations fgadc_proc_fops = {
	.write = fgadc_log_write,
};

int init_proc_log_fg(void)
{
	int ret = 0;

#if 1
	proc_create("fgadc_log", 0644, NULL, &fgadc_proc_fops);
	bm_debug("proc_create fgadc_proc_fops\n");
#else
	proc_entry_fgadc = create_proc_entry("fgadc_log", 0644, NULL);

	if (proc_entry_fgadc == NULL) {
		ret = -ENOMEM;
		bm_debug("init_proc_log_fg: Couldn't create proc entry\n");
	} else {
		proc_entry_fgadc->write_proc = fgadc_log_write;
		bm_debug("init_proc_log_fg loaded.\n");
	}
#endif

	return ret;
}

/* ------------------------------------------------------------------------------------------- */

static ssize_t show_FG_daemon_log_level(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	bm_trace("[FG] show FG_daemon_log_level : %d\n", gFG_daemon_log_level);
	return sprintf(buf, "%d\n", gFG_daemon_log_level);
}

static ssize_t store_FG_daemon_log_level(struct device *dev, struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_debug("[FG_daemon_log_level]\n");

	if (buf != NULL && size != 0) {
		bm_debug("[FG_daemon_log_level] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_debug("[FG_daemon_log_level] val is %d ??\n", (int)val);
			val = 0;
		}
		gFG_daemon_log_level = val;
		bm_debug("[FG_daemon_log_level] gFG_daemon_log_level=%d\n", gFG_daemon_log_level);
	}
	return size;
}

static DEVICE_ATTR(FG_daemon_log_level, 0664, show_FG_daemon_log_level, store_FG_daemon_log_level);
/* ------------------------------------------------------------------------------------------- */

#ifdef FG_BAT_INT
unsigned char reset_fg_bat_int = KAL_TRUE;

signed int fg_bat_int_coulomb_pre;
signed int fg_bat_int_coulomb;


void fg_bat_int_handler(void)
{
	battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT, &fg_bat_int_coulomb);
	bm_err("fg_bat_int_handler %d %d\n", fg_bat_int_coulomb_pre,
		    fg_bat_int_coulomb);

	reset_fg_bat_int = KAL_TRUE;
	bat_meter_thread_timeout = true;
	wake_up(&fg_wq);
}

signed int battery_meter_set_columb_interrupt(unsigned int val)
{
	bm_err("battery_meter_set_columb_interrupt=%d\n", val);
	battery_meter_ctrl(BATTERY_METER_CMD_SET_COLUMB_INTERRUPT, &val);
	return 0;
}

void fg_bat_int_check(void)
{
#if defined(FG_BAT_INT)
#if defined(CONFIG_POWER_EXT)
#elif defined(SOC_BY_HW_FG)
		if (reset_fg_bat_int == KAL_TRUE) {
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT, &fg_bat_int_coulomb_pre);
			bm_err("[fg_bat_int_check]enable battery_meter_set_columb_interrupt bat size=%d car=%d\n",
				batt_meter_cust_data.q_max_pos_25, fg_bat_int_coulomb_pre);
			battery_meter_set_columb_interrupt(batt_meter_cust_data.q_max_pos_25 / 100);
			/*battery_meter_set_columb_interrupt(1); */
			reset_fg_bat_int = KAL_FALSE;
		} else {
			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT, &fg_bat_int_coulomb);
			bm_err("fg_bat_int_check %d %d %d\n", fg_bat_int_coulomb_pre, fg_bat_int_coulomb,
				batt_meter_cust_data.q_max_pos_25 / 1000);
		}
#endif
#else
#endif				/* #if defined(FG_BAT_INT) */
}
#endif

int battery_meter_routine_thread(void *x)
{
	ktime_t ktime = ktime_set(3, 0);	/* 10s, 10* 1000 ms */
#if 0
	union power_supply_propval val;
#endif

	static kal_bool battery_meter_initilized = KAL_FALSE;

	if (battery_meter_initilized == KAL_FALSE) {
		battery_meter_initial();	/* move from battery_probe() to decrease booting time */
		battery_meter_initilized = KAL_TRUE;
	}

	/* Run on a process content */
	while (1) {
		wake_lock(&battery_meter_lock);
		mutex_lock(&battery_meter_mutex);

		mt_battery_update_time(&batteryMeterThreadRunTime, BATTERY_THREAD_TIME);
		if (fg_ipoh_reset) {
			bm_err("[FG BAT_thread]FG_MAIN because IPOH  .\n");
			battery_meter_set_init_flag(false);
			fgauge_algo_run_get_init_data();
			wakeup_fg_algo((FG_MAIN));
			fg_ipoh_reset = 0;
		} else {

#if 0
			val.intval = 0;
			psy_do_property("mtk-fuelgauge", get, POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
			bm_info("[psy_do_property] POWER_SUPPLY_PROP_VOLTAGE_NOW %d", val.intval);

			val.intval = SEC_BATTERY_CURRENT_UA;
			psy_do_property("mtk-fuelgauge", get, POWER_SUPPLY_PROP_CURRENT_NOW, val);
			bm_info("[psy_do_property] POWER_SUPPLY_PROP_VOLTAGE_NOW(UA) %d", val.intval);

			val.intval = SEC_BATTERY_CURRENT_MA;
			psy_do_property("mtk-fuelgauge", get, POWER_SUPPLY_PROP_CURRENT_NOW, val);
			bm_info("[psy_do_property] POWER_SUPPLY_PROP_VOLTAGE_NOW(MA) %d", val.intval);

			val.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
			psy_do_property("mtk-fuelgauge", get, POWER_SUPPLY_PROP_CAPACITY, val);
			bm_info("[psy_do_property] POWER_SUPPLY_PROP_CAPACITY %d", val.intval);

			psy_do_property("mtk-fuelgauge", get, POWER_SUPPLY_PROP_TEMP, val);
			bm_info("[psy_do_property] POWER_SUPPLY_PROP_TEMP %d", val.intval);

			val.intval = 1;
			psy_do_property("mtk-fuelgauge", set, POWER_SUPPLY_PROP_CHARGE_FULL, val);

			val.intval = 2;
			psy_do_property("mtk-fuelgauge", set, POWER_SUPPLY_PROP_ONLINE, val);
#endif

			wakeup_fg_algo(FG_MAIN);
		};

		bm_err("vbat:%d soc:%d uisoc2:%d\n",
			battery_meter_get_battery_voltage(KAL_TRUE),
			BMT_status.SOC,
			BMT_status.UI_SOC2);

		mutex_unlock(&battery_meter_mutex);
		wake_unlock(&battery_meter_lock);


		bat_meter_thread_timeout = KAL_FALSE;
		wait_event(fg_wq, (bat_meter_thread_timeout == true));

		hrtimer_start(&battery_meter_kthread_timer, ktime, HRTIMER_MODE_REL);
		ktime = ktime_set(BAT_TASK_PERIOD, 0);	/* 10s, 10* 1000 ms */
	}

	return 0;
}


void battery_meter_thread_wakeup(void)
{
	bm_debug("******** battery_meter : battery_meter_thread_wakeup  ********\n");
	bat_meter_thread_timeout = true;
	wake_up(&fg_wq);
}

void wake_up_bat(void)
{
	if (g_use_mtk_fg) {
		bm_debug("[BATTERY] wake_up_bat\n");
		battery_meter_thread_wakeup();
	}
}

enum hrtimer_restart battery_meter_kthread_hrtimer_func(struct hrtimer *timer)
{
	battery_meter_thread_wakeup();
	return HRTIMER_NORESTART;
}


void battery_meter_kthread_hrtimer_init(void)
{
	ktime_t ktime;

	ktime = ktime_set(10, 0);
	hrtimer_init(&battery_meter_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	battery_meter_kthread_timer.function = battery_meter_kthread_hrtimer_func;
	hrtimer_start(&battery_meter_kthread_timer, ktime, HRTIMER_MODE_REL);

	kthread_run(battery_meter_routine_thread, NULL, "battery_meter_thread");

	bm_err("battery_meter_kthread_hrtimer_init : done\n");
}


static int mtk_fg_get_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery_meter_get_battery_voltage(KAL_TRUE);
		break;
		/* Current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CURRENT_UA:
			val->intval = battery_meter_get_battery_current();
			if (battery_meter_get_battery_current_sign() == KAL_FALSE)
				val->intval = 0 - val->intval; /* discharging */

			val->intval  = val->intval * 1000;
			break;
		case SEC_BATTERY_CURRENT_MA:
		default:
			val->intval = battery_meter_get_battery_current();
			bm_info("%s 1. SEC_BATTERY_CURRENT_MA = %dmA\n", __func__, val->intval);
			val->intval = val->intval / 10;
			if (battery_meter_get_battery_current_sign() == KAL_FALSE) {
				val->intval = 0 - val->intval; /* discharging */
				bm_info("%s 2. SEC_BATTERY_CURRENT_MA = %dmA\n", __func__, val->intval);
			} else
				bm_info("%s 3. SEC_BATTERY_CURRENT_MA = %dmA\n", __func__, val->intval);
			break;
		}
		break;
		/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = BMT_status.UI_SOC2;
		break;
		/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery_meter_get_battery_temperature() * 10;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int mtk_fg_set_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     const union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		BMT_status.bat_full = KAL_TRUE;
		bm_info("[mtk_fg_set_property] BMT_status.bat_full=%d\n", BMT_status.bat_full);
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY)
			g_charger_exist = 0;
		else
			g_charger_exist = 1;

		bm_info("[mtk_fg_set_property] g_charger_exist=%d, val->intval=%d\n", g_charger_exist, val->intval);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}


struct fuelgauge_data {
	struct power_supply psy;
};

static struct fuelgauge_data mtk_fuelgauge = {
	.psy = {
		.name = "mtk-fuelgauge",
		.type = POWER_SUPPLY_TYPE_UNKNOWN,
		.properties = mtk_fuelgauge_props,
		.num_properties = ARRAY_SIZE(mtk_fuelgauge_props),
		.get_property = mtk_fg_get_property,
		.set_property = mtk_fg_set_property,
		},
};

static int battery_meter_probe(struct platform_device *dev)
{

	int ret_device_file = 0;
	int ret;

	bm_err("[battery_meter_probe] probe\n");
	/* select battery meter control method */

	if (g_use_mtk_fg) {
		ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_daemon_log_level);

		wake_lock_init(&battery_meter_lock, WAKE_LOCK_SUSPEND, "battery meter wakelock");
	}

	battery_meter_ctrl = bm_ctrl_cmd;

	batt_meter_init_cust_data(dev);

	if (g_use_mtk_fg) {

#if defined(FG_BAT_INT)
		pmic_register_interrupt_callback(FG_BAT_INT_L_NO, fg_bat_int_handler);
		pmic_register_interrupt_callback(FG_BAT_INT_H_NO, fg_bat_int_handler);
#endif

#if defined(FG_BAT_ZCV_INT)
		bm_err("[FG_BAT_ZCV_INT]register\n");
		pmic_register_interrupt_callback(FG_BAT_ZCV_INT_NO, fg_bat_ZCV_int_handler);
#endif

		battery_meter_kthread_hrtimer_init();


		bm_err("[BAT_probe] power_supply_register\n");
		ret = power_supply_register(&(dev->dev), &mtk_fuelgauge.psy);
		if (ret) {
			bm_err("[BAT_probe] power_supply_register FuelGauge Fail !!\n");
			return ret;
		}
	}

	return 0;
}

static int battery_meter_remove(struct platform_device *dev)
{
	bm_debug("[battery_meter_remove]\n");
	return 0;
}

static void battery_meter_shutdown(struct platform_device *dev)
{
	bm_debug("[battery_meter_shutdown]\n");
}

static int battery_meter_suspend(struct platform_device *dev, pm_message_t state)
{
	bm_debug("[battery_meter_suspend]\n");
	if (g_use_mtk_fg)
		fg_bat_int_check();

	return 0;
}

static int battery_meter_resume(struct platform_device *dev)
{
	bm_debug("[battery_meter_resume]\n");
	return 0;
}


/* ----------------------------------------------------- */

#ifdef CONFIG_OF
static const struct of_device_id mt_bat_meter_of_match[] = {
	{.compatible = "mediatek,bat_meter",},
	{},
};

MODULE_DEVICE_TABLE(of, mt_bat_meter_of_match);
#endif
struct platform_device battery_meter_device = {
	.name = "battery_meter",
	.id = -1,
};


static struct platform_driver battery_meter_driver = {
	.probe = battery_meter_probe,
	.remove = battery_meter_remove,
	.shutdown = battery_meter_shutdown,
	.suspend = battery_meter_suspend,
	.resume = battery_meter_resume,
	.driver = {
		   .name = "battery_meter",
		   },
};

static int battery_meter_dts_probe(struct platform_device *dev)
{
	int ret = 0;

	bm_err("******** battery_meter_dts_probe!! ********\n");

	battery_meter_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&battery_meter_device);
	if (ret) {
		bm_err("****[battery_meter_dts_probe] Unable to register device (%d)\n",
				    ret);
		return ret;
	}
	return 0;

}

static struct platform_driver battery_meter_dts_driver = {
	.probe = battery_meter_dts_probe,
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		   .name = "battery_meter_dts",
#ifdef CONFIG_OF
		   .of_match_table = mt_bat_meter_of_match,
#endif
		   },
};

/* ============================================================ */
void bmd_ctrl_cmd_from_user(void *nl_data, struct fgd_nl_msg_t *ret_msg)
{
	struct fgd_nl_msg_t *msg;

	msg = nl_data;

	ret_msg->fgd_cmd = msg->fgd_cmd;

	switch (msg->fgd_cmd) {
	case FG_DAEMON_CMD_GET_INIT_FLAG:
		{
			ret_msg->fgd_data_len += sizeof(init_flag);
			memcpy(ret_msg->fgd_data, &init_flag, sizeof(init_flag));
			bm_debug("[fg_res] init_flag = %d\n", init_flag);
		}
		break;

	case FG_DAEMON_CMD_GET_SOC:
		{
			ret_msg->fgd_data_len += sizeof(gFG_capacity_by_c);
			memcpy(ret_msg->fgd_data, &gFG_capacity_by_c, sizeof(gFG_capacity_by_c));
			bm_debug("[fg_res] gFG_capacity_by_c = %d\n", gFG_capacity_by_c);
		}
		break;

	case FG_DAEMON_CMD_GET_DOD0:
		{
			ret_msg->fgd_data_len += sizeof(gFG_DOD0);
			memcpy(ret_msg->fgd_data, &gFG_DOD0, sizeof(gFG_DOD0));
			bm_debug("[fg_res] gFG_DOD0 = %d\n", gFG_DOD0);
		}
		break;

	case FG_DAEMON_CMD_GET_DOD1:
		{
			ret_msg->fgd_data_len += sizeof(gFG_DOD1);
			memcpy(ret_msg->fgd_data, &gFG_DOD1, sizeof(gFG_DOD1));
			bm_debug("[fg_res] gFG_DOD1 = %d\n", gFG_DOD1);
		}
		break;

	case FG_DAEMON_CMD_GET_HW_OCV:
		{
			signed int voltage = 0;

			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &voltage);

			ret_msg->fgd_data_len += sizeof(voltage);
			memcpy(ret_msg->fgd_data, &voltage, sizeof(voltage));

			bm_debug("[fg_res] hwocv voltage = %d\n", voltage);
			gFG_hwocv = voltage;
		}
		break;

	case FG_DAEMON_CMD_GET_HW_FG_INIT_CURRENT:
		{
			ret_msg->fgd_data_len += sizeof(gFG_current_init);
			memcpy(ret_msg->fgd_data, &gFG_current_init, sizeof(gFG_current_init));

			bm_debug("[fg_res] init fg_current = %d\n", gFG_current_init);
			gFG_current = gFG_current_init;
		}
		break;

	case FG_DAEMON_CMD_GET_HW_FG_CURRENT:
		{
			signed int fg_current = 0;

			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &fg_current);

			ret_msg->fgd_data_len += sizeof(fg_current);
			memcpy(ret_msg->fgd_data, &fg_current, sizeof(fg_current));

			bm_debug("[fg_res] fg_current = %d\n", fg_current);
			gFG_current = fg_current;
		}
		break;

	case FG_DAEMON_CMD_GET_HW_FG_INIT_CURRENT_SIGN:
		{
			ret_msg->fgd_data_len += sizeof(gFG_Is_Charging_init);
			memcpy(ret_msg->fgd_data, &gFG_Is_Charging_init,
			       sizeof(gFG_Is_Charging_init));

			bm_debug("[fg_res] current_state = %d\n", gFG_Is_Charging_init);
			gFG_Is_Charging = gFG_Is_Charging_init;
		}
		break;

	case FG_DAEMON_CMD_GET_HW_FG_CURRENT_SIGN:
		{
			kal_bool current_state = KAL_FALSE;

			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,
					   &current_state);

			ret_msg->fgd_data_len += sizeof(current_state);
			memcpy(ret_msg->fgd_data, &current_state, sizeof(current_state));

			bm_debug("[fg_res] current_state = %d\n", current_state);
			gFG_Is_Charging = current_state;
		}
		break;

	case FG_DAEMON_CMD_GET_HW_FG_CAR_ACT:
		{
			signed int fg_coulomb = 0;

			battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT, &fg_coulomb);
			ret_msg->fgd_data_len += sizeof(fg_coulomb);
			memcpy(ret_msg->fgd_data, &fg_coulomb, sizeof(fg_coulomb));

			bm_debug("[fg_res] fg_coulomb = %d\n", fg_coulomb);
			gFG_coulomb_act = fg_coulomb;
			break;
		}

	case FG_DAEMON_CMD_GET_TEMPERTURE:
		{
			kal_bool update;
			int temperture = 0;

			memcpy(&update, &msg->fgd_data[0], sizeof(update));
			bm_debug("[fg_res] update = %d\n", update);
			temperture = force_get_tbat(update);
			bm_debug("[fg_res] temperture = %d\n", temperture);
			ret_msg->fgd_data_len += sizeof(temperture);
			memcpy(ret_msg->fgd_data, &temperture, sizeof(temperture));
			gFG_temp = temperture;

		}
		break;

	case FG_DAEMON_CMD_DUMP_REGISTER:
		battery_meter_ctrl(BATTERY_METER_CMD_DUMP_REGISTER, NULL);
		break;

	case FG_DAEMON_CMD_CHARGING_ENABLE:
		{
			kal_bool charging_enable = KAL_FALSE;

			battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
			ret_msg->fgd_data_len += sizeof(charging_enable);
			memcpy(ret_msg->fgd_data, &charging_enable, sizeof(charging_enable));

			bm_debug("[fg_res] charging_enable = %d\n", charging_enable);
		}
		break;

	case FG_DAEMON_CMD_GET_BATTERY_INIT_VOLTAGE:
		{
			ret_msg->fgd_data_len += sizeof(gFG_voltage_init);
			memcpy(ret_msg->fgd_data, &gFG_voltage_init, sizeof(gFG_voltage_init));

			bm_debug("[fg_res] battery init voltage = %d\n", gFG_voltage_init);
		}
		break;

	case FG_DAEMON_CMD_GET_BATTERY_VOLTAGE:
		{
			signed int update;
			int voltage = 0;

			memcpy(&update, &msg->fgd_data[0], sizeof(update));
			bm_debug("[fg_res] update = %d\n", update);
			if (update == 1)
				voltage = battery_meter_get_battery_voltage(KAL_TRUE);
			else
				voltage = BMT_status.bat_vol;

			ret_msg->fgd_data_len += sizeof(voltage);
			memcpy(ret_msg->fgd_data, &voltage, sizeof(voltage));

			bm_debug("[fg_res] battery voltage = %d\n", voltage);
		}
		break;

	case FG_DAEMON_CMD_FGADC_RESET:
		bm_debug("[fg_res] fgadc_reset\n");
		battery_meter_ctrl(BATTERY_METER_CMD_HW_RESET, NULL);
#ifdef FG_BAT_INT
					reset_fg_bat_int = KAL_TRUE;
#endif
		break;

	case FG_DAEMON_CMD_GET_BATTERY_PLUG_STATUS:
		{
			int plugout_status = 0;

			battery_meter_ctrl(BATTERY_METER_CMD_GET_BATTERY_PLUG_STATUS,
					   &plugout_status);
			ret_msg->fgd_data_len += sizeof(plugout_status);
			memcpy(ret_msg->fgd_data, &plugout_status, sizeof(plugout_status));

			bm_debug("[fg_res] plugout_status = %d\n", plugout_status);

			gFG_plugout_status = plugout_status;
		}
		break;

	case FG_DAEMON_CMD_GET_RTC_SPARE_FG_VALUE:
		{
			signed int rtc_fg_soc = 0;

			rtc_fg_soc = get_rtc_spare_fg_value();
			ret_msg->fgd_data_len += sizeof(rtc_fg_soc);
			memcpy(ret_msg->fgd_data, &rtc_fg_soc, sizeof(rtc_fg_soc));

			bm_debug("[fg_res] rtc_fg_soc = %d\n", rtc_fg_soc);
		}
		break;

	case FG_DAEMON_CMD_IS_CHARGER_EXIST:
		{
			kal_bool charger_exist = KAL_FALSE;

			charger_exist = bat_is_charger_exist();
			ret_msg->fgd_data_len += sizeof(charger_exist);
			memcpy(ret_msg->fgd_data, &charger_exist, sizeof(charger_exist));

			bm_debug("[fg_res] charger_exist = %d\n", charger_exist);
		}
		break;

	case FG_DAEMON_CMD_IS_BATTERY_FULL:
		{
			kal_bool battery_full = KAL_FALSE;

			BMT_status.bat_full = mt_battery_check_battery_full();
			battery_full = BMT_status.bat_full;
			ret_msg->fgd_data_len += sizeof(battery_full);
			memcpy(ret_msg->fgd_data, &battery_full, sizeof(battery_full));

			bm_debug("[fg_res] battery_full = %d\n", battery_full);
		}
		break;

	case FG_DAEMON_CMD_GET_BOOT_REASON:
		{
			signed int boot_reason = get_boot_reason();

			ret_msg->fgd_data_len += sizeof(boot_reason);
			memcpy(ret_msg->fgd_data, &boot_reason, sizeof(boot_reason));
			bm_debug(" ret_msg->fgd_data_len %d\n", ret_msg->fgd_data_len);
			bm_debug("[fg_res] g_boot_reason = %d\n", boot_reason);
		}
		break;

	case FG_DAEMON_CMD_GET_CHARGING_CURRENT:
		{
			signed int ICharging = battery_meter_get_charging_current();

			ret_msg->fgd_data_len += sizeof(ICharging);
			memcpy(ret_msg->fgd_data, &ICharging, sizeof(ICharging));

			bm_debug("[fg_res] ICharging = %d\n", ICharging);
		}
		break;

	case FG_DAEMON_CMD_GET_CHARGER_VOLTAGE:
		{
			signed int charger_vol = battery_meter_get_charger_voltage();

			ret_msg->fgd_data_len += sizeof(charger_vol);
			memcpy(ret_msg->fgd_data, &charger_vol, sizeof(charger_vol));

			bm_debug("[fg_res] charger_vol = %d\n", charger_vol);
		}
		break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_COND:
		{
			unsigned int shutdown_cond = 0;	/* mt_battery_shutdown_check(); move to user space */

			ret_msg->fgd_data_len += sizeof(shutdown_cond);
			memcpy(ret_msg->fgd_data, &shutdown_cond, sizeof(shutdown_cond));

			bm_debug("[fg_res] shutdown_cond = %d\n", shutdown_cond);
		}
		break;

	case FG_DAEMON_CMD_GET_CUSTOM_SETTING:
		{
			kal_bool version;

			memcpy(&version, &msg->fgd_data[0], sizeof(version));
			bm_debug("[fg_res] version = %d\n", version);

			if (version != CUST_SETTING_VERSION) {
				bm_debug("ERROR version 0x%x, expect 0x%x\n", version,
					 CUST_SETTING_VERSION);
				break;
			}

			memcpy(ret_msg->fgd_data, &batt_meter_cust_data,
			       sizeof(batt_meter_cust_data));
			ret_msg->fgd_data_len += sizeof(batt_meter_cust_data);

			memcpy(&ret_msg->fgd_data[ret_msg->fgd_data_len],
			       &batt_meter_table_cust_data, sizeof(batt_meter_table_cust_data));
			ret_msg->fgd_data_len += sizeof(batt_meter_table_cust_data);

			bm_debug("k fgauge_construct_profile_init1 %d:%d %d:%d %d:%d %d:%d %d:%d\n",
				 batt_meter_table_cust_data.battery_profile_t0[0].percentage,
				 batt_meter_table_cust_data.battery_profile_t0[0].voltage,
				 batt_meter_table_cust_data.battery_profile_t0[10].percentage,
				 batt_meter_table_cust_data.battery_profile_t0[10].voltage,
				 batt_meter_table_cust_data.battery_profile_t0[20].percentage,
				 batt_meter_table_cust_data.battery_profile_t0[20].voltage,
				 batt_meter_table_cust_data.battery_profile_t0[30].percentage,
				 batt_meter_table_cust_data.battery_profile_t0[30].voltage,
				 batt_meter_table_cust_data.battery_profile_t0[40].percentage,
				 batt_meter_table_cust_data.battery_profile_t0[40].voltage);
		}
		break;

	case FG_DAEMON_CMD_GET_UI_SOC:
		{
			ret_msg->fgd_data_len += sizeof(BMT_status.UI_SOC);
			memcpy(ret_msg->fgd_data, &(BMT_status.UI_SOC), sizeof(BMT_status.UI_SOC));

			bm_debug("[fg_res] ui soc = %d\n", BMT_status.UI_SOC);
		}
		break;

	case FG_DAEMON_CMD_GET_CV_VALUE:
		{
			unsigned int cv_voltage;

			cv_voltage = get_cv_voltage();
			ret_msg->fgd_data_len += sizeof(cv_voltage);
			memcpy(ret_msg->fgd_data, &cv_voltage, sizeof(cv_voltage));

			bm_debug("[fg_res] cv value = %d\n", cv_voltage);
		}
		break;

	case FG_DAEMON_CMD_GET_DURATION_TIME:
		{
			int duration_time = 0;
			BATTERY_TIME_ENUM duration_type;

			memcpy(&duration_type, &msg->fgd_data[0], sizeof(duration_type));
			bm_debug("[fg_res] duration_type = %d\n", duration_type);

			duration_time = mt_battery_get_duration_time(duration_type);
			ret_msg->fgd_data_len += sizeof(duration_time);
			memcpy(ret_msg->fgd_data, &duration_time, sizeof(duration_time));

			bm_debug("[fg_res] duration time = %d\n", duration_time);
		}
		break;

	case FG_DAEMON_CMD_GET_TRACKING_TIME:
		{
			ret_msg->fgd_data_len += sizeof(battery_tracking_time);
			memcpy(ret_msg->fgd_data, &battery_tracking_time,
			       sizeof(battery_tracking_time));

			bm_debug("[fg_res] tracking time = %d\n", battery_tracking_time);
		}
		break;

	case FG_DAEMON_CMD_GET_CURRENT_TH:
		{
			ret_msg->fgd_data_len += sizeof(suspend_current_threshold);
			memcpy(ret_msg->fgd_data, &suspend_current_threshold,
			       sizeof(suspend_current_threshold));

			bm_debug("[fg_res] suspend_current_threshold = %d\n",
				 suspend_current_threshold);
		}
		break;

	case FG_DAEMON_CMD_GET_CHECK_TIME:
		{
			ret_msg->fgd_data_len += sizeof(ocv_check_time);
			memcpy(ret_msg->fgd_data, &ocv_check_time, sizeof(ocv_check_time));

			bm_debug("[fg_res] check time = %d\n", ocv_check_time);
		}
		break;

	case FG_DAEMON_CMD_GET_DIFFERENCE_VOLTAGE_UPDATE:
		{
			ret_msg->fgd_data_len += sizeof(difference_voltage_update);
			memcpy(ret_msg->fgd_data, &difference_voltage_update,
			       sizeof(difference_voltage_update));

			bm_debug("[fg_res] difference_voltage_update = %d\n",
				 difference_voltage_update);
		}
		break;

	case FG_DAEMON_CMD_GET_AGING1_LOAD_SOC:
		{
			ret_msg->fgd_data_len += sizeof(aging1_load_soc);
			memcpy(ret_msg->fgd_data, &aging1_load_soc, sizeof(aging1_load_soc));

			bm_debug("[fg_res] aging1_load_soc = %d\n", aging1_load_soc);
		}
		break;

	case FG_DAEMON_CMD_GET_AGING1_UPDATE_SOC:
		{
			ret_msg->fgd_data_len += sizeof(aging1_update_soc);
			memcpy(ret_msg->fgd_data, &aging1_update_soc, sizeof(aging1_update_soc));

			bm_debug("[fg_res] aging1_update_soc = %d\n", aging1_update_soc);
		}
		break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_SYSTEM_VOLTAGE:
		{
			ret_msg->fgd_data_len += sizeof(shutdown_system_voltage);
			memcpy(ret_msg->fgd_data, &shutdown_system_voltage,
			       sizeof(shutdown_system_voltage));

			bm_debug("[fg_res] shutdown_system_voltage = %d\n",
				 shutdown_system_voltage);
		}
		break;

	case FG_DAEMON_CMD_GET_CHARGE_TRACKING_TIME:
		{
			ret_msg->fgd_data_len += sizeof(charge_tracking_time);
			memcpy(ret_msg->fgd_data, &charge_tracking_time,
			       sizeof(charge_tracking_time));

			bm_debug("[fg_res] charge_tracking_time = %d\n", charge_tracking_time);
		}
		break;

	case FG_DAEMON_CMD_GET_DISCHARGE_TRACKING_TIME:
		{
			ret_msg->fgd_data_len += sizeof(discharge_tracking_time);
			memcpy(ret_msg->fgd_data, &discharge_tracking_time,
			       sizeof(discharge_tracking_time));

			bm_debug("[fg_res] discharge_tracking_time = %d\n",
				 discharge_tracking_time);
		}
		break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_GAUGE0:
		{
			ret_msg->fgd_data_len += sizeof(shutdown_gauge0);
			memcpy(ret_msg->fgd_data, &shutdown_gauge0, sizeof(shutdown_gauge0));

			bm_debug("[fg_res] shutdown_gauge0 = %d\n", shutdown_gauge0);
		}
		break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_GAUGE1_XMINS:
		{
			ret_msg->fgd_data_len += sizeof(shutdown_gauge1_xmins);
			memcpy(ret_msg->fgd_data, &shutdown_gauge1_xmins,
			       sizeof(shutdown_gauge1_xmins));

			bm_debug("[fg_res] shutdown_gauge1_xmins = %d\n", shutdown_gauge1_xmins);
		}
		break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_GAUGE1_MINS:
		{
			ret_msg->fgd_data_len += sizeof(shutdown_gauge1_mins);
			memcpy(ret_msg->fgd_data, &shutdown_gauge1_mins,
			       sizeof(shutdown_gauge1_mins));

			bm_debug("[fg_res] shutdown_gauge1_mins = %d\n", shutdown_gauge1_mins);
		}
		break;

	case FG_DAEMON_CMD_SET_SUSPEND_TIME:
		bm_debug("[fg_res] set suspend time\n");
		get_monotonic_boottime(&suspend_time);
		break;

	case FG_DAEMON_CMD_SET_WAKEUP_SMOOTH_TIME:
		{
			memcpy(&wake_up_smooth_time, &msg->fgd_data[0],
			       sizeof(wake_up_smooth_time));
			bm_debug("[fg_res] wake_up_smooth_time = %d\n", wake_up_smooth_time);
		}
		break;

	case FG_DAEMON_CMD_SET_IS_CHARGING:
		{
			memcpy(&gFG_coulomb_is_charging, &msg->fgd_data[0],
			       sizeof(gFG_coulomb_is_charging));
			bm_debug("[fg_res] is_charging = %d\n", gFG_coulomb_is_charging);
		}
		break;

	case FG_DAEMON_CMD_SET_RBAT:
		{
			memcpy(&gFG_resistance_bat, &msg->fgd_data[0], sizeof(gFG_resistance_bat));
			bm_debug("[fg_res] gFG_resistance_bat = %d\n", gFG_resistance_bat);
		}
		break;

	case FG_DAEMON_CMD_SET_SWOCV:
		{
			memcpy(&gFG_voltage, &msg->fgd_data[0], sizeof(gFG_voltage));
			bm_debug("[fg_res] gFG_voltage = %d\n", gFG_voltage);
		}
		break;

	case FG_DAEMON_CMD_SET_DOD0:
		{
			memcpy(&gFG_DOD0, &msg->fgd_data[0], sizeof(gFG_DOD0));
			bm_debug("[fg_res] gFG_DOD0 = %d\n", gFG_DOD0);
		}
		break;

	case FG_DAEMON_CMD_SET_DOD1:
		{
			memcpy(&gFG_DOD1, &msg->fgd_data[0], sizeof(gFG_DOD1));
			bm_debug("[fg_res] gFG_DOD1 = %d\n", gFG_DOD1);
		}
		break;

	case FG_DAEMON_CMD_SET_QMAX:
		{
			memcpy(&gFG_BATT_CAPACITY_aging, &msg->fgd_data[0],
			       sizeof(gFG_BATT_CAPACITY_aging));
			bm_debug("[fg_res] QMAX = %d\n", gFG_BATT_CAPACITY_aging);
		}
		break;

	case FG_DAEMON_CMD_SET_BATTERY_FULL:
		{
			signed int battery_full;

			memcpy(&battery_full, &msg->fgd_data[0], sizeof(battery_full));
			BMT_status.bat_full = (kal_bool) battery_full;
			bm_debug("[fg_res] set bat_full = %d\n", BMT_status.bat_full);
		}
		break;

	case FG_DAEMON_CMD_SET_RTC:
		{
			signed int rtcvalue;

			memcpy(&rtcvalue, &msg->fgd_data[0], sizeof(rtcvalue));
			set_rtc_spare_fg_value(rtcvalue);
			bm_notice("[fg_res] set rtc = %d\n", rtcvalue);
		}
		break;

	case FG_DAEMON_CMD_SET_POWEROFF:
		{

			bm_debug("[fg_res] FG_DAEMON_CMD_SET_POWEROFF\n");
			/* kernel_power_off(); */
		}
		break;

	case FG_DAEMON_CMD_SET_INIT_FLAG:
		{
			memcpy(&init_flag, &msg->fgd_data[0], sizeof(init_flag));
			bm_notice("[fg_res] init_flag = %d\n", init_flag);
		}
		break;

	case FG_DAEMON_CMD_IS_KPOC:
		{
			signed int kpoc = bat_is_kpoc();

			ret_msg->fgd_data_len += sizeof(kpoc);
			memcpy(ret_msg->fgd_data, &kpoc, sizeof(kpoc));
			bm_debug("[fg_res] query kpoc = %d\n", kpoc);
		}
		break;

	case FG_DAEMON_CMD_SET_SOC:
		{
			memcpy(&gFG_capacity_by_c, &msg->fgd_data[0], sizeof(gFG_capacity_by_c));
			bm_debug("[fg_res] SOC = %d\n", gFG_capacity_by_c);
			BMT_status.SOC = gFG_capacity_by_c;
		}
		break;

	case FG_DAEMON_CMD_SET_UI_SOC:
		{
			signed int UI_SOC;

			memcpy(&UI_SOC, &msg->fgd_data[0], sizeof(UI_SOC));
			bm_debug("[fg_res] UI_SOC = %d\n", UI_SOC);
			BMT_status.UI_SOC = UI_SOC;
		}
		break;

	case FG_DAEMON_CMD_SET_UI_SOC2:
		{
			signed int UI_SOC;

			memcpy(&UI_SOC, &msg->fgd_data[0], sizeof(UI_SOC));
			bm_debug("[fg_res] UI_SOC2 = %d\n", UI_SOC);
			BMT_status.UI_SOC2 = UI_SOC;
			if (!g_battery_soc_ready)
				g_battery_soc_ready = KAL_TRUE;

		}
		break;

	case FG_DAEMON_CMD_CHECK_FG_DAEMON_VERSION:
		{
			memcpy(&g_fgd_version, &msg->fgd_data[0], sizeof(g_fgd_version));
			bm_debug("[fg_res] g_fgd_pid = %d\n", g_fgd_version);
			if (FGD_CHECK_VERSION != g_fgd_version) {
				bm_err("bad FG_DAEMON_VERSION 0x%x, 0x%x\n",
				       FGD_CHECK_VERSION, g_fgd_version);
			} else {
				bm_debug("FG_DAEMON_VERSION OK\n");
			}
		}
		break;

	case FG_DAEMON_CMD_SET_DAEMON_PID:
		{
			memcpy(&g_fgd_pid, &msg->fgd_data[0], sizeof(g_fgd_pid));
			bm_debug("[fg_res] g_fgd_pid = %d\n", g_fgd_pid);
		}
		break;

	case FG_DAEMON_CMD_SET_OAM_V_OCV:
		{
			signed int tmp;

			memcpy(&tmp, &msg->fgd_data[0], sizeof(tmp));
			bm_print(BM_LOG_CRTI, "[fg_res] OAM_V_OCV = %d\n", tmp);
			oam_v_ocv = tmp;
		}
		break;

	case FG_DAEMON_CMD_SET_OAM_R:
		{
			signed int tmp;

			memcpy(&tmp, &msg->fgd_data[0], sizeof(tmp));
			bm_print(BM_LOG_CRTI, "[fg_res] OAM_R = %d\n", tmp);
			oam_r = tmp;
		}
		break;

	case FG_DAEMON_CMD_GET_SUSPEND_TIME:
		{
			ret_msg->fgd_data_len += sizeof(swfg_ap_suspend_time);
			memcpy(ret_msg->fgd_data, &swfg_ap_suspend_time,
			       sizeof(swfg_ap_suspend_time));
			bm_print(BM_LOG_CRTI, "[fg_res] suspend_time = %d\n", swfg_ap_suspend_time);
			swfg_ap_suspend_time = 0;
		}
		break;

	case FG_DAEMON_CMD_GET_SUSPEND_CAR:
		{
			signed int car = ap_suspend_car / 3600;

			ret_msg->fgd_data_len += sizeof(car);
			memcpy(ret_msg->fgd_data, &car, sizeof(car));
			bm_print(BM_LOG_CRTI,
				 "[fg_res] ap_suspend_car:(%d:%d) t:%d hwocv:%d ocv:%d i:%d stime:%d:%d\n",
				 ap_suspend_car, car, swfg_ap_suspend_time, last_hwocv, oam_v_ocv,
				 last_i, total_suspend_times, this_suspend_times);
			ap_suspend_car = ap_suspend_car % 3600;
			this_suspend_times = 0;
		}
		break;

	case FG_DAEMON_CMD_IS_HW_OCV_UPDATE:
		{
			ret_msg->fgd_data_len += sizeof(is_hwocv_update);
			memcpy(ret_msg->fgd_data, &is_hwocv_update, sizeof(is_hwocv_update));
			bm_print(BM_LOG_CRTI, "[fg_res] is_hwocv_update = %d\n", is_hwocv_update);
			is_hwocv_update = KAL_FALSE;
		}
		break;

	default:
		bm_debug("bad FG_DAEMON_CTRL_CMD_FROM_USER 0x%x\n", msg->fgd_cmd);
		break;
	}			/* switch() */

}

static void nl_send_to_user(u32 pid, int seq, struct fgd_nl_msg_t *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	/* int size=sizeof(struct fgd_nl_msg_t); */
	int size = reply_msg->fgd_data_len + FGD_NL_MSG_T_HDR_LEN;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0;	/* from kernel */
	NETLINK_CB(skb).dst_group = 0;	/* unicast */

	/* bm_debug("[Netlink] nl_reply_user: netlink_unicast size=%d fgd_cmd=%d pid=%d\n",
	   /size, reply_msg->fgd_cmd, pid); */
	ret = netlink_unicast(daemo_nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		bm_err("[Netlink] send failed %d\n", ret);
		return;
	}
	/*bm_debug("[Netlink] reply_user: netlink_unicast- ret=%d\n", ret); */


}


static void nl_data_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct fgd_nl_msg_t *fgd_msg, *fgd_ret_msg;
	int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	/*bm_debug("[Netlink] recv skb from user space uid:%d pid:%d seq:%d\n",uid,pid,seq); */
	data = NLMSG_DATA(nlh);

	fgd_msg = (struct fgd_nl_msg_t *)data;

	size = fgd_msg->fgd_ret_data_len + FGD_NL_MSG_T_HDR_LEN;

	fgd_ret_msg = vmalloc(size);
	memset(fgd_ret_msg, 0, size);

	bmd_ctrl_cmd_from_user(data, fgd_ret_msg);
	nl_send_to_user(pid, seq, fgd_ret_msg);
	/*bm_print(BM_LOG_CRTI,"[Netlink] send to user space process done\n"); */

	vfree(fgd_ret_msg);
}

int wakeup_fg_algo(int flow_state)
{

	if (gDisableFG) {
		bm_notice("FG daemon is disabled\n");
		return -1;
	}

	bm_err("[wakeup_fg_algo] g_fgd_pid=%d\n", g_fgd_pid);

	if (g_fgd_pid != 0) {
		struct fgd_nl_msg_t *fgd_msg;
		int size = FGD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		fgd_msg = vmalloc(size);
		bm_debug("[battery_meter_driver] malloc size=%d\n", size);
		memset(fgd_msg, 0, size);
		fgd_msg->fgd_cmd = FG_DAEMON_CMD_NOTIFY_DAEMON;
		memcpy(fgd_msg->fgd_data, &flow_state, sizeof(flow_state));
		fgd_msg->fgd_data_len += sizeof(flow_state);
		nl_send_to_user(g_fgd_pid, 0, fgd_msg);
		vfree(fgd_msg);
		return 0;
	} else {
		return -1;
	}
}

static int __init battery_meter_init(void)
{

	int ret;
#ifdef CONFIG_OF
	struct device_node *node;
	char *fuelgauge_name;
#endif

	/* add by willcai for the userspace  to kernelspace */
	struct netlink_kernel_cfg cfg = {
		.input = nl_data_handler,
	};
	/* end */

#ifdef CONFIG_OF
		node = of_find_compatible_node(NULL, NULL, "samsung,sec-battery");
		if (node) {
			ret = of_property_read_string(node,
				"battery,fuelgauge_name", (char const **)&fuelgauge_name);

			if (!strcmp(fuelgauge_name, "mtk-fuelgauge"))
				g_use_mtk_fg = KAL_TRUE;

			bm_notice("[battery_meter_driver] fuelgauge_name=%s use_mtk_fg=%d\n",
				fuelgauge_name, g_use_mtk_fg);
		}
#endif


#ifdef CONFIG_OF
	/* */
#else
	if (g_use_mtk_fg) {
		ret = platform_device_register(&battery_meter_device);
		if (ret) {
			bm_err("[battery_meter_driver] Unable to device register(%d)\n", ret);
			return ret;
		}
	}
#endif

	ret = platform_driver_register(&battery_meter_driver);
	if (ret) {
		bm_err("[battery_meter_driver] Unable to register driver (%d)\n", ret);
		return ret;
	}



#ifdef CONFIG_OF
	ret = platform_driver_register(&battery_meter_dts_driver);
#endif

/* add by willcai for the userspace to kernelspace */

	if (g_use_mtk_fg) {
		daemo_nl_sk = netlink_kernel_create(&init_net, NETLINK_FGD, &cfg);
		bm_debug("netlink_kernel_create protol= %d\n", NETLINK_FGD);

		if (daemo_nl_sk == NULL) {
			bm_err("netlink_kernel_create error\n");
			return -1;
		}
		bm_debug("netlink_kernel_create ok\n");
	}

	bm_debug("[battery_meter_driver] Initialization : DONE\n");

	return 0;
}

#ifdef BATTERY_MODULE_INIT
device_initcall(battery_meter_init);
#else
static void __exit battery_meter_exit(void)
{
}
module_init(battery_meter_init);
/* module_exit(battery_meter_exit); */
#endif


MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("Battery Meter Device Driver");
MODULE_LICENSE("GPL");
