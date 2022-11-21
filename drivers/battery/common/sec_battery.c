/*
 *  sec_battery.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"
#include "sec_battery_sysfs.h"
#include "sec_battery_dt.h"
#include "sec_battery_ttf.h"
#if defined(CONFIG_SEC_COMMON)
#include <linux/sec_common.h>
#endif

#if defined(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#if defined(CONFIG_ARCH_QCOM) && !defined(CONFIG_ARCH_EXYNOS)
#include <linux/samsung/sec_param.h>
#endif
#include <linux/sec_debug.h>

static const char *sec_voter_name[] = {
	FOREACH_VOTER(GENERATE_STRING)
};

static enum power_supply_property sec_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
#if IS_ENABLED(CONFIG_FUELGAUGE_MAX77705)
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
#endif
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
};

static enum power_supply_property sec_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property sec_wireless_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
};

static enum power_supply_property sec_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TEMP,
};

static char *supply_list[] = {
	"battery",
};

char *sec_cable_type[SEC_BATTERY_CABLE_MAX] = {
	"UNKNOWN",		/* 0 */
	"NONE",			/* 1 */
	"PREPARE_TA",		/* 2 */
	"TA",			/* 3 */
	"USB",			/* 4 */
	"USB_CDP",		/* 5 */
	"9V_TA",		/* 6 */
	"9V_ERR",		/* 7 */
	"9V_UNKNOWN",		/* 8 */
	"12V_TA",		/* 9 */
	"WC",			/* 10 */
	"HV_WC",		/* 11 */
	"PMA_WC",		/* 12 */
	"WC_PACK",		/* 13 */
	"WC_HV_PACK",		/* 14 */
	"WC_STAND",		/* 15 */
	"WC_HV_STAND",		/* 16 */
	"OC20",			/* 17 */
	"QC30",			/* 18 */
	"PDIC",			/* 19 */
	"UARTOFF",		/* 20 */
	"OTG",			/* 21 */
	"LAN_HUB",		/* 22 */
	"POWER_SHARGING",	/* 23 */
	"HMT_CONNECTED",	/* 24 */
	"HMT_CHARGE",		/* 25 */
	"HV_TA_CHG_LIMIT",	/* 26 */
	"WC_VEHICLE",		/* 27 */
	"WC_HV_VEHICLE",	/* 28 */
	"WC_HV_PREPARE",	/* 29 */
	"TIMEOUT",		/* 30 */
	"SMART_OTG",		/* 31 */
	"SMART_NOTG",		/* 32 */
	"WC_TX",		/* 33 */
	"HV_WC_20",		/* 34 */
	"HV_WC_20_LIMIT",	/* 35 */
	"WC_FAKE",		/* 36 */
	"HV_WC_20_PREPARE",	/* 37 */
	"PDIC_APDO",		/* 38 */
	"POGO",			/* 39 */
};
EXPORT_SYMBOL(sec_cable_type);

char *sec_bat_charging_mode_str[] = {
	"None",
	"Normal",
	"Additional",
	"Re-Charging",
	"ABS"
};

char *sec_bat_status_str[] = {
	"Unknown",
	"Charging",
	"Discharging",
	"Not-charging",
	"Full"
};

char *sec_bat_health_str[] = {
	"Unknown", "Good", "Overheat", "Dead", "Over voltage",
	"Unspecified failure", "Cold", "Watchdog timer expire",
	"Safety timer expire", "Over current", "Warm", "Cool", "Hot",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",	"RESERVED", "RESERVED", "RESERVED",
	/* POWER_SUPPLY_EXT_HEALTH_MIN = 20 */
	"UnderVoltage",
	"OverheatLimit",
	"VsysOVP",
	"VbatOVP",
	"DCErr",
};

char *sec_bat_charge_mode_str[] = {
	"Buck-Off",
	"Charging-Off",
	"Charging-On",
};

char *sec_bat_rx_type_str[] = {
	"No Dev",
	"Other DEV",
	"SS Gear",
	"SS Phone",
	"SS Buds",
};

char *vout_control_mode_str[] = {
	"Set VOUT Off",
	"Set VOUT NV",
	"Set Vout Rsv",
	"Set Vout HV",
	"Set Vout CV",
	"Set Vout Call",
	"Set Vout 5V",
	"Set Vout 9V",
	"Set Vout 10V",
	"Set Vout 11V",
	"Set Vout 12V",
	"Set Vout 12.5V",
	"Set Vout 5V Step",
	"Set Vout 5.5V Step",
	"Set Vout 9V Step",
	"Set Vout 10V Step",
};

#if !defined(CONFIG_SEC_FACTORY) && !defined(CONFIG_ARCH_EXYNOS)
#define SALE_CODE_STR_LEN		3
static char sales_code_from_cmdline[SALE_CODE_STR_LEN+1];
bool sales_code_is(char *str)
{
	pr_info("%s: %s\n", __func__, sales_code_from_cmdline);
	return !strncmp(sales_code_from_cmdline, str, SALE_CODE_STR_LEN + 1);
}

#if IS_BUILTIN(CONFIG_BATTERY_SAMSUNG)
static int __init sales_code_setup(char *str)
{
	strlcpy(sales_code_from_cmdline, str, ARRAY_SIZE(sales_code_from_cmdline));
	return 1;
}
__setup("androidboot.sales_code=", sales_code_setup);
#endif
#endif

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
extern int muic_afc_set_voltage(int vol);
extern int muic_hv_charger_disable(bool en);
#else
int muic_afc_set_voltage(int vol)
{
	pr_info("%s: MUIC is not supported.\n", __func__);
	return -1;
}

int muic_hv_charger_disable(bool en)
{
	pr_info("%s: MUIC is not supported.\n", __func__);
	return -1;
}
#endif

static int set_charging_current(void * data, int v)
{
	union power_supply_propval value = {0, };
	struct sec_battery_info *battery = data;

	battery->charging_current = v;
	value.intval = v;
	psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_AVG, value);
#if defined(CONFIG_DUAL_BATTERY)
	sec_bat_divide_charging_current(battery, charging_current);

	value.intval = battery->main_charging_current;
	psy_do_property(battery->pdata->main_limiter_name, set,
			POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT, value);

	value.intval = battery->sub_charging_current;
	psy_do_property(battery->pdata->sub_limiter_name, set,
			POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT, value);
#endif

#if defined(CONFIG_DUAL_BATTERY)
	pr_info("%s: power(%d), input(%d), charge(%d), charge_m(%d), charge_s(%d)\n", __func__,
			battery->charge_power, battery->input_current, battery->charging_current, battery->main_charging_current, battery->sub_charging_current);
#else
	pr_info("%s: power(%d), input(%d), charge(%d)\n", __func__,
			battery->charge_power, battery->input_current, battery->charging_current);
#endif
	return 0;
}

static int set_input_current(void * data, int v)
{
	union power_supply_propval value = {0, };
	struct sec_battery_info *battery = data;

	battery->input_current = v;
	battery->charge_power = mW_by_mVmA(battery->input_voltage, v);
	if (battery->charge_power > battery->max_charge_power)
		battery->max_charge_power = battery->charge_power;
	value.intval = v;
	psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_MAX, value);
	return 0;
}

static int set_float_voltage(void * data, int voltage)
{
	struct sec_battery_info *battery = data;
	union power_supply_propval value = {0, };

	value.intval = voltage;
	psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, value);
	return 0;
}

static int set_topoff_current(void * data, int v)
{
	struct sec_battery_info *battery = data;
	union power_supply_propval value = {0, };
	bool do_chgen_vote = false;

	if (battery->charging_mode == SEC_BATTERY_CHARGING_2ND ||
		battery->pdata->full_check_type == SEC_BATTERY_FULLCHARGED_CHGPSY ||
		battery->pdata->full_check_type == SEC_BATTERY_FULLCHARGED_CHGINT)
		do_chgen_vote = true;

	if (do_chgen_vote)
		sec_vote(battery->chgen_vote, VOTER_TOPOFF_CHANGE, true, SEC_BAT_CHG_MODE_CHARGING_OFF);

	value.intval = v;
	psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_EXT_PROP_CURRENT_FULL, value);

	if (do_chgen_vote)
		sec_vote(battery->chgen_vote, VOTER_TOPOFF_CHANGE, false, 0);
	battery->topoff_condition = v;

	return 0;
}

void sec_bat_set_misc_event(struct sec_battery_info *battery,
	unsigned int misc_event_val, unsigned int misc_event_mask) {

	unsigned int temp = battery->misc_event;

	mutex_lock(&battery->misclock);

	battery->misc_event &= ~misc_event_mask;
	battery->misc_event |= misc_event_val;

	pr_info("%s: misc event before(0x%x), after(0x%x)\n",
		__func__, temp, battery->misc_event);

	mutex_unlock(&battery->misclock);

	if (battery->prev_misc_event != battery->misc_event) {
		cancel_delayed_work(&battery->misc_event_work);
		__pm_stay_awake(battery->misc_event_ws);
		queue_delayed_work(battery->monitor_wqueue,
				&battery->misc_event_work, 0);
	}
}
EXPORT_SYMBOL(sec_bat_set_misc_event);

void sec_bat_set_tx_event(struct sec_battery_info *battery,
	unsigned int tx_event_val, unsigned int tx_event_mask) {

	unsigned int temp = battery->tx_event;

	mutex_lock(&battery->txeventlock);

	battery->tx_event &= ~tx_event_mask;
	battery->tx_event |= tx_event_val;

	pr_info("@Tx_Mode %s: tx event before(0x%x), after(0x%x)\n",
		__func__, temp, battery->tx_event);

	if (temp != battery->tx_event) {
		/* Assure receiving tx_event to App for sleep case */
		__pm_wakeup_event(battery->tx_event_ws, jiffies_to_msecs(HZ * 2));
		power_supply_changed(battery->psy_bat);
	}
	mutex_unlock(&battery->txeventlock);
}

void sec_bat_set_current_event(struct sec_battery_info *battery,
			      unsigned int current_event_val, unsigned int current_event_mask)
{
	unsigned int temp = battery->current_event;

	mutex_lock(&battery->current_eventlock);

	battery->current_event &= ~current_event_mask;
	battery->current_event |= current_event_val;

	pr_info("%s: current event before(0x%x), after(0x%x)\n",
		__func__, temp, battery->current_event);

	mutex_unlock(&battery->current_eventlock);
}
EXPORT_SYMBOL(sec_bat_set_current_event);

void sec_bat_set_temp_control_test(struct sec_battery_info *battery, bool temp_enable)
{
	if (temp_enable) {
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_TEMP_CTRL_TEST) {
			pr_info("%s : BATT_TEMP_CONTROL_TEST already ENABLED\n", __func__);
			return;
		}
		pr_info("%s : BATT_TEMP_CONTROL_TEST ENABLE\n", __func__);
		sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_TEMP_CTRL_TEST,
			SEC_BAT_CURRENT_EVENT_TEMP_CTRL_TEST);
		battery->pdata->usb_temp_check_type_backup = battery->pdata->usb_temp_check_type;
		battery->pdata->usb_temp_check_type = SEC_BATTERY_TEMP_CHECK_NONE;
		battery->overheatlimit_threshold_backup = battery->overheatlimit_threshold;
		battery->overheatlimit_threshold = 990;
		battery->overheatlimit_recovery_backup = battery->overheatlimit_recovery;
		battery->overheatlimit_recovery = 980;
	} else {
		if (!(battery->current_event & SEC_BAT_CURRENT_EVENT_TEMP_CTRL_TEST)) {
			pr_info("%s : BATT_TEMP_CONTROL_TEST already END\n", __func__);
			return;
		}
		pr_info("%s : BATT_TEMP_CONTROL_TEST END\n", __func__);
		sec_bat_set_current_event(battery, 0,
			SEC_BAT_CURRENT_EVENT_TEMP_CTRL_TEST);
		battery->pdata->usb_temp_check_type = battery->pdata->usb_temp_check_type_backup;
		battery->overheatlimit_threshold = battery->overheatlimit_threshold_backup;
		battery->overheatlimit_recovery = battery->overheatlimit_recovery_backup;
	}
}
EXPORT_SYMBOL(sec_bat_set_temp_control_test);

void sec_bat_change_default_current(struct sec_battery_info *battery,
					int cable_type, int input, int output)
{
	bool do_set_vote = (battery->cable_type == cable_type || battery->wire_status == cable_type);

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	if (!battery->test_max_current)
#endif
		battery->pdata->charging_current[cable_type].input_current_limit = input;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	if (!battery->test_charge_current)
#endif
		battery->pdata->charging_current[cable_type].fast_charging_current = output;
	pr_info("%s: cable_type: %d(%d,%d), input: %d, output: %d\n",
			__func__, cable_type, battery->cable_type, battery->wire_status,
			battery->pdata->charging_current[cable_type].input_current_limit,
			battery->pdata->charging_current[cable_type].fast_charging_current);
	if (do_set_vote) {
		sec_vote(battery->input_vote, VOTER_CABLE, true,
			battery->pdata->charging_current[cable_type].input_current_limit);
		sec_vote(battery->fcc_vote, VOTER_CABLE, true,
			battery->pdata->charging_current[cable_type].fast_charging_current);
	}
}

void sec_bat_change_pdo(struct sec_battery_info *battery, int vol)
{
	int target_pd_index = 0;

	if (is_pd_wire_type(battery->wire_status)) {
		if (vol == SEC_INPUT_VOLTAGE_9V) /* select PDO greater than 5V */
			target_pd_index = battery->pd_list.num_fpdo - 1;
		else /* select 5V PDO */
			target_pd_index = 0;

		if (target_pd_index < 0 || target_pd_index >= MAX_PDO_NUM) {
			pr_info("%s: target_pd_index is wrong: %d\n", __func__, target_pd_index);
			return;
		}
		pr_info("%s: target_pd_index: %d, now_pd_index: %d\n", __func__,
			target_pd_index, battery->pd_list.now_pd_index);

		if (target_pd_index != battery->pd_list.now_pd_index) {
			/* change input current before request new pdo if new pdo's input current is less than now */
			if (battery->pd_list.pd_info[target_pd_index].max_current < battery->input_current) {
				battery->input_current = battery->pd_list.pd_info[target_pd_index].max_current;
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SELECT_PDO,
					SEC_BAT_CURRENT_EVENT_SELECT_PDO);
				sec_vote(battery->input_vote, VOTER_SELECT_PDO, true, battery->input_current);
			}
			battery->pdic_ps_rdy = false;
			if (target_pd_index >= 0 && target_pd_index < MAX_PDO_NUM)
				sec_pd_select_pdo(battery->pd_list.pd_info[target_pd_index].pdo_index);
		}
	}
}

#if !defined(CONFIG_SEC_FACTORY)
static bool sec_bat_change_vbus(struct sec_battery_info *battery)
{
	if (battery->pdata->chg_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return false;

#if defined(CONFIG_SUPPORT_HV_CTRL)
	union power_supply_propval value;
	unsigned int target_vbus = SEC_INPUT_VOLTAGE_0V;

	if (battery->store_mode)
		return false;

	if (is_hv_wire_type(battery->cable_type) &&
		(battery->cable_type != SEC_BATTERY_CABLE_QC30)) {

		if (battery->current_event & SEC_BAT_CURRENT_EVENT_AFC) {
			pr_info("%s: skip during current_event(0x%x)\n",
				__func__, battery->current_event);
			return false;
		}

		/* check target vbus */
		if (battery->vbus_limit)
			target_vbus = SEC_INPUT_VOLTAGE_0V;
		else if (battery->vbus_chg_by_full)
			target_vbus = SEC_INPUT_VOLTAGE_5V;
		else if (battery->siop_level >= 100 && !battery->lcd_status) {
			if (is_hv_wire_12v_type(battery->cable_type))
				target_vbus = SEC_INPUT_VOLTAGE_12V;
			else
				target_vbus = SEC_INPUT_VOLTAGE_9V;

			if (battery->vbus_chg_by_siop == SEC_INPUT_VOLTAGE_NONE)
				battery->vbus_chg_by_siop = target_vbus;

		} else if (battery->status == POWER_SUPPLY_STATUS_CHARGING)
			target_vbus = SEC_INPUT_VOLTAGE_5V;

		if (target_vbus == SEC_INPUT_VOLTAGE_0V) {
			pr_info("%s: skip set vbus %dV, level(%d), Cable(%s, %s, %d, %d)\n",
				__func__, target_vbus, battery->siop_level,
				sec_cable_type[battery->cable_type], sec_cable_type[battery->wire_status],
				battery->muic_cable_type, battery->pd_usb_attached);

			return false;
		}

		if (battery->vbus_chg_by_siop != target_vbus) {
			/* set current event */
			cancel_delayed_work(&battery->afc_work);
			sec_bat_check_afc_input_current(battery);

			battery->chg_limit = false;
			battery->vbus_chg_by_siop = target_vbus;
			muic_afc_set_voltage(target_vbus/1000);

			pr_info("%s: vbus set %dV by level(%d), Cable(%s, %s, %d, %d)\n",
				__func__, target_vbus, battery->siop_level,
				sec_cable_type[battery->cable_type], sec_cable_type[battery->wire_status],
				battery->muic_cable_type, battery->pd_usb_attached);

			return true;
		}
	}
#endif
	return false;
}

static bool sec_bat_change_vbus_pd(struct sec_battery_info *battery)
{
#if defined(CONFIG_SUPPORT_HV_CTRL)
	int target_pd_index = 0;
	int input_current;

	if (battery->pdata->chg_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return false;

	if (battery->store_mode)
		return false;

	if (battery->cable_type == SEC_BATTERY_CABLE_PDIC) {
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_SELECT_PDO) {
			pr_info("%s: skip during current_event(0x%x)\n",
				__func__, battery->current_event);
			return false;
		}

		if (battery->siop_level >= 100) {
			/* select PDO greater than 5V */
			target_pd_index = battery->pd_list.max_pd_count - 1;
		} else {
			/* select 5V PDO */
			target_pd_index = 0;
		}

		if (target_pd_index < 0 || target_pd_index >= MAX_PDO_NUM) {
			pr_info("%s: target_pd_index is wrong: %d\n", __func__, target_pd_index);
			return false;
		}
		pr_info("%s: target_pd_index: %d, now_pd_index: %d\n", __func__,
			target_pd_index, battery->pd_list.now_pd_index);

		if (target_pd_index != battery->pd_list.now_pd_index) {
			/* change input current before request new pdo if new pdo's input current is less than now */
			input_current = battery->pd_list.pd_info[target_pd_index].max_current;
			battery->pdic_ps_rdy = false;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SELECT_PDO,
				SEC_BAT_CURRENT_EVENT_SELECT_PDO);
			sec_vote(battery->input_vote, VOTER_SELECT_PDO, true, input_current);
			if (target_pd_index >= 0 && target_pd_index < MAX_PDO_NUM)
				sec_pd_select_pdo(battery->pd_list.pd_info[target_pd_index].pdo_index);
			return true;
		}
	}
#endif
	return false;
}
#endif

static int sec_bat_check_afc_input_current(struct sec_battery_info *battery)
{
	int work_delay = 0;
	int input_current;

	sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_AFC,
			(SEC_BAT_CURRENT_EVENT_CHG_LIMIT | SEC_BAT_CURRENT_EVENT_AFC));
	if (!is_wireless_type(battery->cable_type)) {
		input_current = battery->pdata->pre_afc_input_current; // 1000mA
		work_delay = battery->pdata->pre_afc_work_delay;
	} else {
		input_current = battery->pdata->pre_wc_afc_input_current;
		/* do not reduce this time, this is for noble pad */
		work_delay = battery->pdata->pre_wc_afc_work_delay;
	}
	sec_vote(battery->input_vote, VOTER_VBUS_CHANGE, true, input_current);

	if (!delayed_work_pending(&battery->afc_work)) {
		__pm_stay_awake(battery->afc_ws);
		queue_delayed_work(battery->monitor_wqueue,
				&battery->afc_work, msecs_to_jiffies(work_delay));
	}

	pr_info("%s: change input_current(%d), cable_type(%d)\n", __func__, input_current, battery->cable_type);

	return input_current;
}

static void sec_bat_get_input_current_in_power_list(struct sec_battery_info *battery)
{
	int pdo_num = battery->sink_status.current_pdo_num;
	int max_input_current = 0;

	if (is_pd_apdo_wire_type(battery->wire_status) && battery->pd_list.now_isApdo)
		pdo_num = 1;

	max_input_current = battery->pdata->charging_current[SEC_BATTERY_CABLE_PDIC].input_current_limit =
		battery->sink_status.power_list[pdo_num].max_current;
	battery->pdata->charging_current[SEC_BATTERY_CABLE_PDIC_APDO].input_current_limit =
		battery->sink_status.power_list[pdo_num].max_current;

	pr_info("%s:max_input_current : %dmA\n", __func__, max_input_current);
	sec_vote(battery->input_vote, VOTER_CABLE, true, max_input_current);
}

static void sec_bat_get_charging_current_in_power_list(struct sec_battery_info *battery)
{
	int max_charging_current = 0, pd_power = 0;
	int pdo_num = battery->sink_status.current_pdo_num;

	if (is_pd_apdo_wire_type(battery->wire_status) && battery->pd_list.now_isApdo)
		pdo_num = 1;

	pd_power = mW_by_mVmA(battery->sink_status.power_list[pdo_num].max_voltage,
		battery->sink_status.power_list[pdo_num].max_current);

	/* We assume that output voltage to float voltage */
	max_charging_current = mA_by_mWmV(pd_power, (battery->pdata->chg_float_voltage / battery->pdata->chg_float_voltage_conv));
	max_charging_current = max_charging_current > battery->pdata->max_charging_current ?
		battery->pdata->max_charging_current : max_charging_current;
	battery->pdata->charging_current[SEC_BATTERY_CABLE_PDIC].fast_charging_current = max_charging_current;

#if defined(CONFIG_STEP_CHARGING)
	if (is_pd_apdo_wire_type(battery->wire_status) && !battery->pd_list.now_isApdo &&
		battery->step_chg_status < 0)
#else
	if (is_pd_apdo_wire_type(battery->wire_status) && !battery->pd_list.now_isApdo)
#endif
		battery->pdata->charging_current[SEC_BATTERY_CABLE_PDIC_APDO].fast_charging_current = max_charging_current;
	battery->charge_power = pd_power;

	pr_info("%s:pd_charge_power : %dmW, max_charging_current : %dmA\n", __func__,
		battery->charge_power, max_charging_current);
	sec_vote(battery->fcc_vote, VOTER_CABLE, true, battery->pdata->charging_current[battery->wire_status].fast_charging_current);
}

#if defined(CONFIG_DUAL_BATTERY)
void sec_bat_divide_charging_current(struct sec_battery_info *battery, int charging_current)
{
	unsigned int main_current = 0, sub_current = 0;

	main_current = (charging_current * battery->pdata->main_charging_rate) / 100;
	sub_current = (charging_current * battery->pdata->sub_charging_rate) / 100;

	/* calculate main battery current */
	if (main_current > battery->pdata->max_main_charging_current)
		main_current = battery->pdata->max_main_charging_current;
	else if (main_current < battery->pdata->min_main_charging_current)
		main_current = battery->pdata->min_main_charging_current;

	/* calculate sub battery current */
	if (sub_current > battery->pdata->max_sub_charging_current)
		sub_current = battery->pdata->max_sub_charging_current;
	else if (sub_current < battery->pdata->min_sub_charging_current)
		sub_current = battery->pdata->min_sub_charging_current;

	battery->main_charging_current = main_current;
	battery->sub_charging_current = sub_current;
}
#endif

int sec_bat_set_charging_current(struct sec_battery_info *battery)
{
#if defined(CONFIG_AFC_CHARGER_MODE)
	static int afc_init = false;
	union power_supply_propval value = {0, };
#endif

	mutex_lock(&battery->iolock);
	if (!is_nocharge_type(battery->cable_type)) {
#if !defined(CONFIG_SEC_FACTORY)
		/* VOTER_MIX_LIMIT */
		sec_bat_check_mix_temp(battery);

		/* VOTER_CHG_TEMP */
		if (is_wireless_type(battery->cable_type))
			sec_bat_check_wpc_temp(battery);
		else if (!is_pd_apdo_wire_type(battery->wire_status) && !sec_bat_change_vbus(battery))
			sec_bat_check_afc_temp(battery);
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		else if (is_pd_apdo_wire_type(battery->wire_status))
			sec_bat_check_direct_chg_temp(battery);
#endif

		/* VOTER_PDIC_TEMP */
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		if (battery->cable_type == SEC_BATTERY_CABLE_PDIC && !sec_bat_change_vbus_pd(battery))
#else
		if (is_pd_wire_type(battery->cable_type) && !sec_bat_change_vbus_pd(battery))
#endif
			sec_bat_check_pdic_temp(battery);
#endif

		/* VOTER_STORE_MODE */
		/* Set limited max power when store mode is set and LDU		*/
		/* Limited max power should be set with over 5% capacity	*/
		/* since target could be turned off during boot up		*/
		if (battery->store_mode && battery->capacity >= 5) {
			sec_vote(battery->input_vote, VOTER_STORE_MODE, true,
				mA_by_mWmV(battery->pdata->store_mode_max_input_power, battery->input_voltage));
		}

#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
		/* Calculate wireless input current under the specific conditions (wpc_sleep_mode, chg_limit)*/
		/* VOTER_WPC_CUR */
		if (battery->wc_status != SEC_WIRELESS_PAD_NONE) {
			sec_bat_get_wireless_current(battery);
		}
#endif

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		if (battery->dc_float_voltage_set) {
			int age_step = battery->pdata->age_step;
			int chg_step = battery->step_chg_status;

			pr_info("%s : step float voltage = %d\n", __func__,
					battery->pdata->dc_step_chg_val_vfloat[age_step][chg_step]);
			value.intval = battery->pdata->dc_step_chg_val_vfloat[age_step][chg_step];
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_EXT_PROP_DIRECT_VOLTAGE_MAX, value);
			battery->dc_float_voltage_set = false;
		}
#endif

		/* check topoff current */
		if (battery->charging_mode == SEC_BATTERY_CHARGING_2ND &&
				(battery->pdata->full_check_type_2nd == SEC_BATTERY_FULLCHARGED_CHGPSY ||
				 battery->pdata->full_check_type_2nd == SEC_BATTERY_FULLCHARGED_LIMITER)) {
			sec_vote(battery->topoff_vote, VOTER_FULL_CHARGE, true, battery->pdata->full_check_current_2nd);
		}

	}/* !is_nocharge_type(battery->cable_type) */

#if defined(CONFIG_AFC_CHARGER_MODE)
	if (!afc_init) {
		afc_init = true;
		value.intval = 1;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_EXT_PROP_AFC_CHARGER_MODE, value);
	}
#endif
	mutex_unlock(&battery->iolock);
	return 0;
}
EXPORT_SYMBOL(sec_bat_set_charging_current);

int sec_bat_set_charge(void * data, int chg_mode)
{
	struct sec_battery_info *battery = data;
	union power_supply_propval val = {0, };
	struct timespec ts = {0, };

	if ((battery->current_event & SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE) &&
		(chg_mode == SEC_BAT_CHG_MODE_CHARGING)) {
		dev_info(battery->dev, "%s: charge disable by HMT\n", __func__);
		chg_mode = SEC_BAT_CHG_MODE_CHARGING_OFF;
	}

	battery->charger_mode = chg_mode;
	pr_info("%s set %s mode\n", __func__, sec_bat_charge_mode_str[chg_mode]);

	val.intval = battery->status;
	psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_STATUS, val);
	ts = ktime_to_timespec(ktime_get_boottime());

	if (chg_mode == SEC_BAT_CHG_MODE_CHARGING) {
		/*Reset charging start time only in initial charging start */
		if (battery->charging_start_time == 0) {
			if (ts.tv_sec < 1)
				ts.tv_sec = 1;
			battery->charging_start_time = ts.tv_sec;
			battery->charging_next_time =
				battery->pdata->charging_reset_time;
		}
		battery->charging_block = false;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		if (is_pd_apdo_wire_type(battery->cable_type)) {
			sec_bat_reset_step_charging(battery);
			sec_bat_check_dc_step_charging(battery);
		}
#endif
	} else {
		battery->charging_start_time = 0;
		battery->charging_passed_time = 0;
		battery->charging_next_time = 0;
		battery->charging_fullcharged_time = 0;
		battery->full_check_cnt = 0;
		battery->charging_block = true;
#if defined(CONFIG_STEP_CHARGING)
		sec_bat_reset_step_charging(battery);
#endif
#if defined(CONFIG_BATTERY_CISD)
		battery->usb_overheat_check = false;
		battery->cisd.ab_vbat_check_count = 0;
#endif
	}

	val.intval = chg_mode;
	psy_do_property(battery->pdata->charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, val);
	val.intval = chg_mode;
	psy_do_property(battery->pdata->fuelgauge_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, val);

#if defined(CONFIG_DUAL_BATTERY)
	/* disable main/sub supplement mode */
	if (chg_mode != SEC_BAT_CHG_MODE_CHARGING) {
		val.intval = 0;
		psy_do_property(battery->pdata->dual_battery_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, val);
	} else {
		val.intval = 1;
		psy_do_property(battery->pdata->dual_battery_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, val);
	}

	if (chg_mode != SEC_BAT_CHG_MODE_CHARGING_OFF && battery->status != POWER_SUPPLY_PROP_CHARGE_FULL) {
		/* disable supplement mode execpt 2nd full charge such as charging, discharging, buck off */
		val.intval = 0;
		psy_do_property(battery->pdata->dual_battery_name, set,
		POWER_SUPPLY_PROP_CHARGE_FULL, val);
	}
#endif
	return 0;
}
EXPORT_SYMBOL(sec_bat_set_charge);

static bool sec_bat_check_by_psy(struct sec_battery_info *battery)
{
	char *psy_name = NULL;
	union power_supply_propval value = {0, };
	bool ret = true;

	switch (battery->pdata->battery_check_type) {
	case SEC_BATTERY_CHECK_PMIC:
		psy_name = battery->pdata->pmic_name;
		break;
	case SEC_BATTERY_CHECK_FUELGAUGE:
		psy_name = battery->pdata->fuelgauge_name;
		break;
	case SEC_BATTERY_CHECK_CHARGER:
		psy_name = battery->pdata->charger_name;
		break;
	default:
		dev_err(battery->dev,
			"%s: Invalid Battery Check Type\n", __func__);
		ret = false;
		goto battery_check_error;
		break;
	}

	psy_do_property(psy_name, get,
		POWER_SUPPLY_PROP_PRESENT, value);
	ret = (bool)value.intval;

battery_check_error:
	return ret;
}

#if defined(CONFIG_DUAL_BATTERY)
static bool sec_bat_check_by_gpio(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };
	bool ret = true;
	int main_det = -1, sub_det = -1;

	value.intval = SEC_DUAL_BATTERY_MAIN;
	psy_do_property(battery->pdata->dual_battery_name, get,
		POWER_SUPPLY_EXT_PROP_DUAL_BAT_DET, value);
	main_det = value.intval;

	value.intval = SEC_DUAL_BATTERY_SUB;
	psy_do_property(battery->pdata->dual_battery_name, get,
		POWER_SUPPLY_EXT_PROP_DUAL_BAT_DET, value);
	sub_det = value.intval;

	ret = (bool)(main_det & sub_det);
	if (!ret)
		pr_info("%s : main det = %d, sub det = %d\n", __func__, main_det, sub_det);

	return ret;
}
#endif

static bool sec_bat_check(struct sec_battery_info *battery)
{
	bool ret = true;

	if (battery->factory_mode || battery->is_jig_on) {
		dev_dbg(battery->dev, "%s: No need to check in factory mode\n",
			__func__);
		return ret;
	}

	if (battery->health != POWER_SUPPLY_HEALTH_GOOD &&
		battery->health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
		dev_dbg(battery->dev, "%s: No need to check\n", __func__);
		return ret;
	}

	switch (battery->pdata->battery_check_type) {
	case SEC_BATTERY_CHECK_ADC:
		if (is_nocharge_type(battery->cable_type))
			ret = battery->present;
		else
			ret = sec_bat_check_vf_adc(battery);
		break;
	case SEC_BATTERY_CHECK_INT:
	case SEC_BATTERY_CHECK_CALLBACK:
		if (is_nocharge_type(battery->cable_type)) {
			ret = battery->present;
		} else {
			if (battery->pdata->check_battery_callback)
				ret = battery->pdata->check_battery_callback();
		}
		break;
	case SEC_BATTERY_CHECK_PMIC:
	case SEC_BATTERY_CHECK_FUELGAUGE:
	case SEC_BATTERY_CHECK_CHARGER:
		ret = sec_bat_check_by_psy(battery);
		break;
#if defined(CONFIG_DUAL_BATTERY)
	case SEC_BATTERY_CHECK_DUAL_BAT_GPIO:
		ret = sec_bat_check_by_gpio(battery);
		break;
#endif
	case SEC_BATTERY_CHECK_NONE:
		dev_dbg(battery->dev, "%s: No Check\n", __func__);
	default:
		break;
	}

	return ret;
}

static bool sec_bat_get_cable_type(
			struct sec_battery_info *battery,
			int cable_source_type)
{
	bool ret = false;
	int cable_type = battery->cable_type;

	if (cable_source_type & SEC_BATTERY_CABLE_SOURCE_CALLBACK) {
		if (battery->pdata->check_cable_callback)
			cable_type =
				battery->pdata->check_cable_callback();
	}

	if (cable_source_type & SEC_BATTERY_CABLE_SOURCE_ADC) {
		if (gpio_get_value_cansleep(
			battery->pdata->bat_gpio_ta_nconnected) ^
			battery->pdata->bat_polarity_ta_nconnected)
			cable_type = SEC_BATTERY_CABLE_NONE;
		else
			cable_type =
				sec_bat_get_charger_type_adc(battery);
	}

	if (battery->cable_type == cable_type) {
		dev_dbg(battery->dev,
			"%s: No need to change cable status\n", __func__);
	} else {
		if (cable_type < SEC_BATTERY_CABLE_NONE ||
			cable_type >= SEC_BATTERY_CABLE_MAX) {
			dev_err(battery->dev,
				"%s: Invalid cable type\n", __func__);
		} else {
			battery->cable_type = cable_type;
			if (battery->pdata->check_cable_result_callback)
				battery->pdata->check_cable_result_callback(
						battery->cable_type);

			ret = true;

			dev_dbg(battery->dev, "%s: Cable Changed (%d)\n",
				__func__, battery->cable_type);
		}
	}

	return ret;
}

void sec_bat_set_charging_status(struct sec_battery_info *battery,
		int status) {
	union power_supply_propval value = {0, };

	switch (status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (battery->siop_level < 100 || battery->lcd_status || battery->wc_tx_enable)
			battery->stop_timer = true;
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
		if ((battery->status == POWER_SUPPLY_STATUS_FULL ||
			(battery->capacity == 100 && !is_slate_mode(battery))) &&
			!battery->store_mode) {

			pr_info("%s : Update fg scale to 101%%\n", __func__);
			value.intval = 100;
			psy_do_property(battery->pdata->fuelgauge_name, set,
					POWER_SUPPLY_PROP_CHARGE_FULL, value);

			/* To get SOC value (NOT raw SOC), need to reset value */
			value.intval = 0;
			psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_CAPACITY, value);
			battery->capacity = value.intval;
		}
		battery->expired_time = battery->pdata->expired_time;
		battery->prev_safety_time = 0;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		if (is_wireless_type(battery->cable_type)) {
			bool send_cs100_cmd = true;

#ifdef CONFIG_CS100_JPNCONCEPT
			psy_do_property(battery->pdata->wireless_charger_name, get,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ID, value);

			/* In case of the JPN PAD, this pad blocks the charge after give the cs100 command. */
			send_cs100_cmd = (battery->charging_mode == SEC_BATTERY_CHARGING_2ND ||	value.intval);
#endif
			if (send_cs100_cmd) {
				value.intval = POWER_SUPPLY_STATUS_FULL;
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_STATUS, value);
			}
		}
		break;
	default:
		break;
	}
	battery->status = status;
}

void sec_bat_set_health(struct sec_battery_info *battery, int health)
{
	if (battery->health != health) {
		if (health == POWER_SUPPLY_EXT_HEALTH_OVERHEATLIMIT) {
			sec_bat_set_misc_event(battery,
				BATT_MISC_EVENT_HEALTH_OVERHEATLIMIT, BATT_MISC_EVENT_HEALTH_OVERHEATLIMIT);
		} else {
			sec_bat_set_misc_event(battery,
				0, BATT_MISC_EVENT_HEALTH_OVERHEATLIMIT);
		}

		if ((health == POWER_SUPPLY_HEALTH_OVERHEAT) ||
			(health == POWER_SUPPLY_HEALTH_COLD) ||
			(health == POWER_SUPPLY_EXT_HEALTH_OVERHEATLIMIT)) {
			if (is_wireless_fake_type(battery->cable_type)) {
				union power_supply_propval val = {0, };

				val.intval = health;
				psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_PROP_HEALTH, val);
			}
		}
	}

	battery->health = health;
}

static bool sec_bat_battery_cable_check(struct sec_battery_info *battery)
{
	if (!sec_bat_check(battery)) {
		if (battery->check_count < battery->pdata->check_count)
			battery->check_count++;
		else {
			dev_err(battery->dev,
				"%s: Battery Disconnected\n", __func__);
			battery->present = false;
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);

			if (battery->status !=
				POWER_SUPPLY_STATUS_DISCHARGING) {
				sec_bat_set_charging_status(battery,
						POWER_SUPPLY_STATUS_NOT_CHARGING);
				sec_vote(battery->chgen_vote, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			}

			if (battery->pdata->check_battery_result_callback)
				battery->pdata->
					check_battery_result_callback();
			return false;
		}
	} else
		battery->check_count = 0;

	battery->present = true;

	if (battery->health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
		sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);

		if (battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_CHARGING);
			sec_vote(battery->chgen_vote, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE, false, 0);
		}
	}

	dev_dbg(battery->dev, "%s: Battery Connected\n", __func__);

	if (battery->pdata->cable_check_type &
		SEC_BATTERY_CABLE_CHECK_POLLING) {
		if (sec_bat_get_cable_type(battery,
			battery->pdata->cable_source_type)) {
			__pm_stay_awake(battery->cable_ws);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->cable_work, 0);
		}
	}
	return true;
}

static int sec_bat_ovp_uvlo_by_psy(struct sec_battery_info *battery)
{
	char *psy_name = NULL;
	union power_supply_propval value = {0, };

	value.intval = POWER_SUPPLY_HEALTH_GOOD;

	switch (battery->pdata->ovp_uvlo_check_type) {
	case SEC_BATTERY_OVP_UVLO_PMICPOLLING:
		psy_name = battery->pdata->pmic_name;
		break;
	case SEC_BATTERY_OVP_UVLO_CHGPOLLING:
		psy_name = battery->pdata->charger_name;
		break;
	default:
		dev_err(battery->dev,
			"%s: Invalid OVP/UVLO Check Type\n", __func__);
		goto ovp_uvlo_check_error;
		break;
	}

	psy_do_property(psy_name, get,
		POWER_SUPPLY_PROP_HEALTH, value);

ovp_uvlo_check_error:
	return value.intval;
}

static bool sec_bat_ovp_uvlo_result(
		struct sec_battery_info *battery, int health)
{
	if (health == POWER_SUPPLY_EXT_HEALTH_DC_ERR) {
		dev_info(battery->dev,
			"%s: DC err (%d)\n",
			__func__, health);
		battery->is_recharging = false;
		battery->health_check_count = DEFAULT_HEALTH_CHECK_COUNT;
		__pm_wakeup_event(battery->vbus_ws, jiffies_to_msecs(HZ * 10));
		/* Enable charging anyway to check actual DC's health */
		sec_vote(battery->chgen_vote, VOTER_DC_ERR, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
		sec_vote(battery->chgen_vote, VOTER_DC_ERR, false, 0);
	}

	if (battery->health != health) {
		sec_bat_set_health(battery, health);
		switch (health) {
		case POWER_SUPPLY_HEALTH_GOOD:
			dev_info(battery->dev, "%s: Safe voltage\n", __func__);
			dev_info(battery->dev, "%s: is_recharging : %d\n", __func__, battery->is_recharging);
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_CHARGING);
			battery->charging_mode = SEC_BATTERY_CHARGING_1ST;
			sec_vote(battery->chgen_vote, VOTER_VBUS_OVP, false, 0);
			battery->health_check_count = 0;
			break;
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE:
			dev_info(battery->dev,
				"%s: Unsafe voltage (%d)\n",
				__func__, health);
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_NOT_CHARGING);
			sec_vote(battery->chgen_vote, VOTER_VBUS_OVP, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			battery->is_recharging = false;
			battery->health_check_count = DEFAULT_HEALTH_CHECK_COUNT;
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_UNSAFETY_VOLTAGE]++;
			battery->cisd.data[CISD_DATA_UNSAFE_VOLTAGE_PER_DAY]++;
#endif
			/*
			 * Take the wakelock during 10 seconds
			 * when over-voltage status is detected
			 */
			__pm_wakeup_event(battery->vbus_ws, jiffies_to_msecs(HZ * 10));
			break;
		}
		power_supply_changed(battery->psy_bat);
		return true;
	}

	return false;
}

static bool sec_bat_ovp_uvlo(struct sec_battery_info *battery)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;

	if (battery->wdt_kick_disable) {
		dev_dbg(battery->dev,
			"%s: No need to check in wdt test\n",
			__func__);
		return false;
	} else if ((battery->status == POWER_SUPPLY_STATUS_FULL) &&
		   (battery->charging_mode == SEC_BATTERY_CHARGING_NONE)) {
		dev_dbg(battery->dev, "%s: No need to check in Full status", __func__);
		return false;
	}

	if (battery->health != POWER_SUPPLY_HEALTH_GOOD &&
		battery->health != POWER_SUPPLY_HEALTH_OVERVOLTAGE &&
		battery->health != POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE &&
		battery->health != POWER_SUPPLY_EXT_HEALTH_DC_ERR) {
		dev_dbg(battery->dev, "%s: No need to check\n", __func__);
		return false;
	}
	health = battery->health;

	switch (battery->pdata->ovp_uvlo_check_type) {
	case SEC_BATTERY_OVP_UVLO_CALLBACK:
		if (battery->pdata->ovp_uvlo_callback)
			health = battery->pdata->ovp_uvlo_callback();
		break;
	case SEC_BATTERY_OVP_UVLO_PMICPOLLING:
	case SEC_BATTERY_OVP_UVLO_CHGPOLLING:
		health = sec_bat_ovp_uvlo_by_psy(battery);
		break;
	case SEC_BATTERY_OVP_UVLO_PMICINT:
	case SEC_BATTERY_OVP_UVLO_CHGINT:
		/* nothing for interrupt check */
	default:
		break;
	}

	/*
	 * Move the location for calling the get_health
	 * in case of attaching the jig
	 */
	if (battery->factory_mode || battery->is_jig_on) {
		dev_dbg(battery->dev,
			"%s: No need to check in factory mode\n",
			__func__);
		return false;
	}

	return sec_bat_ovp_uvlo_result(battery, health);
}

static bool sec_bat_check_recharge(struct sec_battery_info *battery)
{
#if defined(CONFIG_DUAL_BATTERY)
	int voltage = 0;
#endif

	if (battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING)
		return false;

	if ((battery->status == POWER_SUPPLY_STATUS_CHARGING) &&
			(battery->pdata->full_condition_type &
			 SEC_BATTERY_FULL_CONDITION_NOTIMEFULL) &&
			(battery->charging_mode == SEC_BATTERY_CHARGING_NONE)) {
		dev_info(battery->dev,
				"%s: Re-charging by NOTIMEFULL (%d)\n",
				__func__, battery->capacity);
		goto check_recharge_check_count;
	}

	if (battery->status == POWER_SUPPLY_STATUS_FULL &&
			battery->charging_mode == SEC_BATTERY_CHARGING_NONE) {
		int recharging_voltage = battery->pdata->recharge_condition_vcell;

		if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP_MODE) {
			recharging_voltage = battery->pdata->swelling_low_rechg_voltage;
			dev_info(battery->dev, "%s: recharging voltage changed by low temp(%d)\n",
					__func__, recharging_voltage);
		}
		dev_info(battery->dev, "%s: recharging voltage (%d)\n",
				__func__, recharging_voltage);

		if ((battery->pdata->recharge_condition_type &
					SEC_BATTERY_RECHARGE_CONDITION_SOC) &&
				(battery->capacity <=
				 battery->pdata->recharge_condition_soc)) {
			battery->expired_time = battery->pdata->recharging_expired_time;
			battery->prev_safety_time = 0;
			dev_info(battery->dev,
					"%s: Re-charging by SOC (%d)\n",
					__func__, battery->capacity);
			goto check_recharge_check_count;
		}

		if ((battery->pdata->recharge_condition_type &
		     SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL) &&
		    (battery->voltage_avg <= recharging_voltage)) {
			battery->expired_time = battery->pdata->recharging_expired_time;
			battery->prev_safety_time = 0;
			dev_info(battery->dev,
					"%s: Re-charging by average VCELL (%d)\n",
					__func__, battery->voltage_avg);
			goto check_recharge_check_count;
		}

		if ((battery->pdata->recharge_condition_type &
		     SEC_BATTERY_RECHARGE_CONDITION_VCELL) &&
		    (battery->voltage_now <= recharging_voltage)) {
			battery->expired_time = battery->pdata->recharging_expired_time;
			battery->prev_safety_time = 0;
			dev_info(battery->dev,
					"%s: Re-charging by VCELL (%d)\n",
					__func__, battery->voltage_now);
			goto check_recharge_check_count;
		}

#if defined(CONFIG_DUAL_BATTERY)
		if (battery->pdata->recharge_condition_type &
					SEC_BATTERY_RECHARGE_CONDITION_LIMITER) {
			if (battery->voltage_avg_main > battery->voltage_avg_sub)
				voltage = battery->voltage_avg_main;
			else
				voltage = battery->voltage_avg_sub;

			if (voltage <= recharging_voltage) {
				battery->expired_time = battery->pdata->recharging_expired_time;
				battery->prev_safety_time = 0;
				dev_info(battery->dev,
						"%s: Re-charging by VPACK (%d)mV\n",
						__func__, voltage);
				goto check_recharge_check_count;
			} else if (abs(battery->voltage_avg_main - battery->voltage_avg_sub) >
						battery->pdata->force_recharge_margin) {
				battery->expired_time = battery->pdata->recharging_expired_time;
				battery->prev_safety_time = 0;
				dev_info(battery->dev,
						"%s: Force Re-charging by Vavg_m(%d)mV - Vavg_s(%d)mV,\n",
						__func__, battery->voltage_avg_main, battery->voltage_avg_sub);
				goto check_recharge_check_count;
			}
		}
#endif
	}

	battery->recharge_check_cnt = 0;
	return false;

check_recharge_check_count:
	if (battery->recharge_check_cnt <
		battery->pdata->recharge_check_count)
		battery->recharge_check_cnt++;
	dev_dbg(battery->dev,
		"%s: recharge count = %d\n",
		__func__, battery->recharge_check_cnt);

	if (battery->recharge_check_cnt >=
		battery->pdata->recharge_check_count)
		return true;
	else
		return false;
}

static bool sec_bat_voltage_check(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING ||
		is_nocharge_type(battery->cable_type) ||
		battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE) {
		dev_dbg(battery->dev,
			"%s: Charging Disabled\n", __func__);
		return true;
	}

	/* OVP/UVLO check */
	if (sec_bat_ovp_uvlo(battery)) {
		if (battery->pdata->ovp_uvlo_result_callback)
			battery->pdata->
				ovp_uvlo_result_callback(battery->health);
		return false;
	}

	if ((battery->status == POWER_SUPPLY_STATUS_FULL) &&
			((battery->charging_mode != SEC_BATTERY_CHARGING_NONE &&
			!battery->charging_block) ||
			(battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING))) {
		int voltage_ref = battery->pdata->recharge_condition_vcell - 50;
#if defined(CONFIG_ENABLE_FULL_BY_SOC)
		int soc_ref = 98;
#else
		int soc_ref = battery->pdata->full_condition_soc;
#endif
		pr_info("%s: chg mode (%d), voltage_ref(%d), voltage_now(%d)\n",
			__func__, battery->charging_mode, voltage_ref, battery->voltage_now);

		if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP_MODE)
			voltage_ref = battery->pdata->swelling_low_rechg_voltage - 50;

		value.intval = 0;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
		if (value.intval < soc_ref && battery->voltage_now < voltage_ref) {
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
			battery->is_recharging = false;
			battery->charging_mode = SEC_BATTERY_CHARGING_1ST;
			sec_vote(battery->topoff_vote, VOTER_FULL_CHARGE, false, 0);
			sec_vote(battery->chgen_vote, VOTER_FULL_CHARGE, false, 0);
			pr_info("%s: battery status full -> charging, RepSOC(%d)\n", __func__, value.intval);
			return false;
		}
	}

	/* Re-Charging check */
	if (sec_bat_check_recharge(battery)) {
		if (battery->pdata->full_check_type !=
			SEC_BATTERY_FULLCHARGED_NONE)
			battery->charging_mode = SEC_BATTERY_CHARGING_1ST;
		else
			battery->charging_mode = SEC_BATTERY_CHARGING_2ND;
		battery->is_recharging = true;
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.data[CISD_DATA_RECHARGING_COUNT]++;
		battery->cisd.data[CISD_DATA_RECHARGING_COUNT_PER_DAY]++;
#endif
		sec_vote(battery->chgen_vote, VOTER_CABLE, true, SEC_BAT_CHG_MODE_CHARGING);
		sec_vote(battery->topoff_vote, VOTER_FULL_CHARGE, false, 0);
		sec_vote(battery->chgen_vote, VOTER_FULL_CHARGE, false, 0);
		return false;
	}

	return true;
}

#if defined(CONFIG_BATTERY_AGE_FORECAST)
static bool sec_bat_set_aging_step(struct sec_battery_info *battery, int step)
{
	union power_supply_propval value = {0, };

	if (battery->pdata->num_age_step <= 0 || step < 0 || step >= battery->pdata->num_age_step) {
		pr_info("%s: [AGE] abnormal age step : %d/%d\n",
			__func__, step, battery->pdata->num_age_step-1);
		return false;
	}

	battery->pdata->age_step = step;

	/* float voltage */
	battery->pdata->chg_float_voltage =
		battery->pdata->age_data[battery->pdata->age_step].float_voltage;
	sec_vote(battery->fv_vote, VOTER_AGING_STEP, true, battery->pdata->chg_float_voltage);

	/* full/recharge condition */
	battery->pdata->recharge_condition_vcell =
		battery->pdata->age_data[battery->pdata->age_step].recharge_condition_vcell;
	battery->pdata->full_condition_soc =
		battery->pdata->age_data[battery->pdata->age_step].full_condition_soc;
	battery->pdata->full_condition_vcell =
		battery->pdata->age_data[battery->pdata->age_step].full_condition_vcell;

#if defined(CONFIG_LSI_IFPMIC)
	value.intval = battery->pdata->age_step;
	psy_do_property(battery->pdata->fuelgauge_name, set,
		POWER_SUPPLY_EXT_PROP_UPDATE_BATTERY_DATA, value);
#else
	value.intval = battery->pdata->full_condition_soc;
	psy_do_property(battery->pdata->fuelgauge_name, set,
		POWER_SUPPLY_PROP_CAPACITY_LEVEL, value);
#endif
#if defined(CONFIG_STEP_CHARGING)
	sec_bat_set_aging_info_step_charging(battery);
#endif

	dev_info(battery->dev,
		 "%s: Step(%d/%d), Cycle(%d), float_v(%d), r_v(%d), f_s(%d), f_vl(%d)\n",
		 __func__,
		 battery->pdata->age_step, battery->pdata->num_age_step-1, battery->batt_cycle,
		 battery->pdata->chg_float_voltage,
		 battery->pdata->recharge_condition_vcell,
		 battery->pdata->full_condition_soc,
		 battery->pdata->full_condition_vcell);

	return true;
}

void sec_bat_aging_check(struct sec_battery_info *battery)
{
	int prev_step = battery->pdata->age_step;
	int calc_step;
	bool ret = 0;
	static bool init; /* false */

	if ((battery->pdata->num_age_step <= 0) || (battery->batt_cycle < 0))
		return;

	if (battery->temperature < 50) {
		pr_info("%s: [AGE] skip (temperature:%d)\n", __func__, battery->temperature);
		return;
	}

	for (calc_step = battery->pdata->num_age_step - 1; calc_step >= 0; calc_step--) {
		if (battery->pdata->age_data[calc_step].cycle <= battery->batt_cycle)
			break;
	}

	if ((calc_step == prev_step) && init)
		return;

	init = true;
	ret = sec_bat_set_aging_step(battery, calc_step);
	dev_info(battery->dev,
		 "%s: %s change step (%d->%d), Cycle(%d)\n",
		 __func__, ret ? "Succeed in" : "Fail to",
		 prev_step, battery->pdata->age_step, battery->batt_cycle);
}
EXPORT_SYMBOL(sec_bat_aging_check);

void sec_bat_check_battery_health(struct sec_battery_info *battery)
{
	static battery_health_condition default_table[3] =
		{{.cycle = 900, .asoc = 75}, {.cycle = 1200, .asoc = 65}, {.cycle = 1500, .asoc = 55}};

	battery_health_condition *ptable = default_table;
	battery_health_condition state;
	int i, battery_health, size = BATTERY_HEALTH_MAX;

	if (battery->pdata->health_condition == NULL) {
		/*
		 * If a new type is added to misc_battery_health, default table cannot verify the actual state except "bad".
		 * If you want to modify to return the correct values for all states,
		 * add a table that matches the state added to the dt file.
		*/
		pr_info("%s: does not set health_condition_table, use default table\n", __func__);
		size = 3;
	} else {
		ptable = battery->pdata->health_condition;
	}

	/* Checking Cycle and ASoC */
	state.cycle = state.asoc = BATTERY_HEALTH_BAD;
	for (i = size - 1; i >= 0; i--) {
		if (ptable[i].cycle >= (battery->batt_cycle % 10000))
			state.cycle = i + BATTERY_HEALTH_GOOD;
		if (ptable[i].asoc <= battery->batt_asoc)
			state.asoc = i + BATTERY_HEALTH_GOOD;
	}
	battery_health = max(state.cycle, state.asoc);
	pr_info("%s: update battery_health(%d), (%d - %d)\n",
		__func__, battery_health, state.cycle, state.asoc);
	/* Update battery health */
	sec_bat_set_misc_event(battery,
		(battery_health << BATTERY_HEALTH_SHIFT), BATT_MISC_EVENT_BATTERY_HEALTH);
}
EXPORT_SYMBOL(sec_bat_check_battery_health);
#endif

static bool sec_bat_check_fullcharged_condition(
					struct sec_battery_info *battery)
{
	int full_check_type = SEC_BATTERY_FULLCHARGED_NONE;

	if (battery->charging_mode == SEC_BATTERY_CHARGING_1ST)
		full_check_type = battery->pdata->full_check_type;
	else
		full_check_type = battery->pdata->full_check_type_2nd;

	switch (full_check_type) {
	case SEC_BATTERY_FULLCHARGED_ADC:
	case SEC_BATTERY_FULLCHARGED_FG_CURRENT:
	case SEC_BATTERY_FULLCHARGED_SOC:
	case SEC_BATTERY_FULLCHARGED_CHGGPIO:
	case SEC_BATTERY_FULLCHARGED_CHGPSY:
#if defined(CONFIG_DUAL_BATTERY)
	case SEC_BATTERY_FULLCHARGED_LIMITER:
#endif
		break;

	/* If these is NOT full check type or NONE full check type,
	 * it is full-charged
	 */
	case SEC_BATTERY_FULLCHARGED_CHGINT:
	case SEC_BATTERY_FULLCHARGED_TIME:
	case SEC_BATTERY_FULLCHARGED_NONE:
	default:
		return true;
		break;
	}

#if defined(CONFIG_ENABLE_FULL_BY_SOC)
	if (battery->capacity >= 100 && !battery->is_recharging) {
		dev_info(battery->dev, "%s: enough SOC (%d%%), skip!\n", __func__, battery->capacity);
		return true;
	}
#endif

	if (battery->pdata->full_condition_type & SEC_BATTERY_FULL_CONDITION_SOC) {
		if (battery->capacity < battery->pdata->full_condition_soc) {
			dev_dbg(battery->dev, "%s: Not enough SOC (%d%%)\n", __func__, battery->capacity);
			return false;
		}
	}

	if (battery->pdata->full_condition_type & SEC_BATTERY_FULL_CONDITION_VCELL) {
		int full_condition_vcell = battery->pdata->full_condition_vcell;

		if (battery->thermal_zone == BAT_THERMAL_WARM)	/* high temp swelling full */
			full_condition_vcell = battery->pdata->high_temp_float - 100;

		if (battery->voltage_now < full_condition_vcell) {
			dev_dbg(battery->dev, "%s: Not enough VCELL (%dmV)\n", __func__, battery->voltage_now);
			return false;
		}
	}

	if (battery->pdata->full_condition_type & SEC_BATTERY_FULL_CONDITION_AVGVCELL) {
		if (battery->voltage_avg < battery->pdata->full_condition_avgvcell) {
			dev_dbg(battery->dev, "%s: Not enough AVGVCELL (%dmV)\n", __func__, battery->voltage_avg);
			return false;
		}
	}

	if (battery->pdata->full_condition_type & SEC_BATTERY_FULL_CONDITION_OCV) {
		if (battery->voltage_ocv < battery->pdata->full_condition_ocv) {
			dev_dbg(battery->dev, "%s: Not enough OCV (%dmV)\n", __func__, battery->voltage_ocv);
			return false;
		}
	}

	return true;
}

static void sec_bat_do_test_function(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	pr_info("%s: Test Mode\n", __func__);
	switch (battery->test_mode) {
		case 1:
			if (battery->status == POWER_SUPPLY_STATUS_CHARGING) {
				sec_vote(battery->chgen_vote, VOTER_TEST_MODE, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
				sec_bat_set_charging_status(battery,
						POWER_SUPPLY_STATUS_DISCHARGING);
			}
			break;
		case 2:
			if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
				sec_vote(battery->chgen_vote, VOTER_TEST_MODE, true, SEC_BAT_CHG_MODE_CHARGING);
				psy_do_property(battery->pdata->charger_name, get,
						POWER_SUPPLY_PROP_STATUS, value);
				sec_bat_set_charging_status(battery, value.intval);
			}
			battery->test_mode = 0;
			break;
		case 3: // clear temp block
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_DISCHARGING);
			break;
		case 4:
			if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
				sec_vote(battery->chgen_vote, VOTER_TEST_MODE, true, SEC_BAT_CHG_MODE_CHARGING);
				psy_do_property(battery->pdata->charger_name, get,
						POWER_SUPPLY_PROP_STATUS, value);
				sec_bat_set_charging_status(battery, value.intval);
			}
			break;
		default:
			pr_info("%s: error test: unknown state\n", __func__);
			break;
	}
}

static bool sec_bat_time_management(struct sec_battery_info *battery)
{
	struct timespec ts = {0, };
	unsigned long charging_time;

	if (battery->charging_start_time == 0 || !battery->safety_timer_set) {
		dev_dbg(battery->dev,
			"%s: Charging Disabled\n", __func__);
		return true;
	}

	ts = ktime_to_timespec(ktime_get_boottime());

	if (ts.tv_sec >= battery->charging_start_time) {
		charging_time = ts.tv_sec - battery->charging_start_time;
	} else {
		charging_time = 0xFFFFFFFF - battery->charging_start_time
			+ ts.tv_sec;
	}

	battery->charging_passed_time = charging_time;

	switch (battery->status) {
	case POWER_SUPPLY_STATUS_FULL:
		if (battery->expired_time == 0) {
			dev_info(battery->dev,
				"%s: Recharging Timer Expired\n", __func__);
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE);
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
			battery->is_recharging = false;
			sec_vote(battery->chgen_vote, VOTER_TIME_EXPIRED, true, SEC_BAT_CHG_MODE_CHARGING_OFF);

			return false;
		}
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		if ((battery->pdata->full_condition_type &
		     SEC_BATTERY_FULL_CONDITION_NOTIMEFULL) &&
		    (battery->is_recharging && (battery->expired_time == 0))) {
			dev_info(battery->dev,
			"%s: Recharging Timer Expired\n", __func__);
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE);
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
			battery->is_recharging = false;
			sec_vote(battery->chgen_vote, VOTER_TIME_EXPIRED, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			return false;
		} else if (!battery->is_recharging &&
			   (battery->expired_time == 0)) {
			dev_info(battery->dev,
				"%s: Charging Timer Expired\n", __func__);
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE);
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_SAFETY_TIMER]++;
			battery->cisd.data[CISD_DATA_SAFETY_TIMER_PER_DAY]++;
#endif
#if defined(CONFIG_SEC_ABC)
			sec_abc_send_event("MODULE=battery@ERROR=safety_timer");
#endif
			sec_vote(battery->chgen_vote, VOTER_TIME_EXPIRED, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			return false;
		}
		break;
	default:
		dev_err(battery->dev,
			"%s: Undefine Battery Status\n", __func__);
		return true;
	}

	return true;
}

bool sec_bat_check_full(struct sec_battery_info *battery, int full_check_type)
{
	union power_supply_propval value = {0, };
	int current_adc = 0;
	bool ret = false;
	int err = 0;

	switch (full_check_type) {
	case SEC_BATTERY_FULLCHARGED_ADC:
		current_adc =
			sec_bat_get_adc_data(battery,
			SEC_BAT_ADC_CHANNEL_FULL_CHECK,
			battery->pdata->adc_check_count);

		dev_dbg(battery->dev,
			"%s: Current ADC (%d)\n",
			__func__, current_adc);

		if (current_adc < 0)
			break;
		battery->current_adc = current_adc;

		if (battery->current_adc <
			battery->topoff_condition) {
			battery->full_check_cnt++;
			dev_dbg(battery->dev,
				"%s: Full Check ADC (%d)\n",
				__func__,
				battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_FG_CURRENT:
		if ((battery->current_now > 0 && battery->current_now <
			((battery->thermal_zone == BAT_THERMAL_NORMAL) ?
				battery->pdata->full_check_current_1st : battery->topoff_condition)) &&
			(battery->current_avg > 0 && battery->current_avg <
			battery->topoff_condition)) {
				battery->full_check_cnt++;
				dev_dbg(battery->dev,
				"%s: Full Check Current (%d)\n",
				__func__,
				battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_TIME:
		if ((battery->charging_mode ==
			SEC_BATTERY_CHARGING_2ND ?
			(battery->charging_passed_time -
			battery->charging_fullcharged_time) :
			battery->charging_passed_time) >
			battery->topoff_condition) {
			battery->full_check_cnt++;
			dev_dbg(battery->dev,
				"%s: Full Check Time (%d)\n",
				__func__,
				battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_SOC:
		if (battery->capacity <=
			battery->topoff_condition) {
			battery->full_check_cnt++;
			dev_dbg(battery->dev,
				"%s: Full Check SOC (%d)\n",
				__func__,
				battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	case SEC_BATTERY_FULLCHARGED_CHGGPIO:
		err = gpio_request(
			battery->pdata->chg_gpio_full_check,
			"GPIO_CHG_FULL");
		if (err) {
			dev_err(battery->dev,
				"%s: Error in Request of GPIO\n", __func__);
			break;
		}
		if (!(gpio_get_value_cansleep(
			battery->pdata->chg_gpio_full_check) ^
			!battery->pdata->chg_polarity_full_check)) {
			battery->full_check_cnt++;
			dev_dbg(battery->dev,
				"%s: Full Check GPIO (%d)\n",
				__func__, battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		gpio_free(battery->pdata->chg_gpio_full_check);
		break;

	case SEC_BATTERY_FULLCHARGED_CHGINT:
	case SEC_BATTERY_FULLCHARGED_CHGPSY:
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_PROP_STATUS, value);

		if (value.intval == POWER_SUPPLY_STATUS_FULL) {
			battery->full_check_cnt++;
			dev_info(battery->dev,
				"%s: Full Check Charger (%d)\n",
				__func__, battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;

	/* If these is NOT full check type or NONE full check type,
	 * it is full-charged
	 */
	case SEC_BATTERY_FULLCHARGED_NONE:
		battery->full_check_cnt = 0;
		ret = true;
		break;
#if defined(CONFIG_DUAL_BATTERY)
	case SEC_BATTERY_FULLCHARGED_LIMITER:
		value.intval = 1;
		psy_do_property(battery->pdata->dual_battery_name, get,
			POWER_SUPPLY_PROP_STATUS, value);

		if (value.intval == POWER_SUPPLY_STATUS_FULL) {
			battery->full_check_cnt++;
			dev_info(battery->dev,
				"%s: Full Check Limiter (%d)\n",
				__func__, battery->full_check_cnt);
		} else
			battery->full_check_cnt = 0;
		break;
#endif
	default:
		dev_err(battery->dev,
			"%s: Invalid Full Check\n", __func__);
		break;
	}

#if defined(CONFIG_ENABLE_FULL_BY_SOC)
	if (battery->capacity >= 100 &&
		battery->charging_mode == SEC_BATTERY_CHARGING_1ST &&
		!battery->is_recharging) {
		battery->full_check_cnt = battery->pdata->full_check_count;
		dev_info(battery->dev,
			"%s: enough SOC to make FULL(%d%%)\n",
			__func__, battery->capacity);
	}
#endif

	if (battery->full_check_cnt >=
		battery->pdata->full_check_count) {
		battery->full_check_cnt = 0;
		ret = true;
	}

#if defined(CONFIG_BATTERY_CISD)
	if (ret && (battery->current_event & SEC_BAT_CURRENT_EVENT_SWELLING_MODE)) {
		battery->cisd.data[CISD_DATA_SWELLING_FULL_CNT]++;
		battery->cisd.data[CISD_DATA_SWELLING_FULL_CNT_PER_DAY]++;
	}
#endif
	return ret;
}

bool sec_bat_check_fullcharged(struct sec_battery_info *battery)
{
	int full_check_type = SEC_BATTERY_FULLCHARGED_NONE;

	if (!sec_bat_check_fullcharged_condition(battery))
		return false;

	if (battery->charging_mode == SEC_BATTERY_CHARGING_1ST)
		full_check_type = battery->pdata->full_check_type;
	else
		full_check_type = battery->pdata->full_check_type_2nd;

	return sec_bat_check_full(battery, full_check_type);
}

static void sec_bat_do_fullcharged(struct sec_battery_info *battery, bool force_fullcharged)
{
	union power_supply_propval value = {0, };

	/* To let charger/fuel gauge know the full status,
	 * set status before calling sec_bat_set_charge()
	 */
#if defined(CONFIG_BATTERY_CISD)
	if (battery->status != POWER_SUPPLY_STATUS_FULL) {
		battery->cisd.data[CISD_DATA_FULL_COUNT]++;
		battery->cisd.data[CISD_DATA_FULL_COUNT_PER_DAY]++;
	}
#endif
	sec_bat_set_charging_status(battery,
			POWER_SUPPLY_STATUS_FULL);

	if (battery->charging_mode == SEC_BATTERY_CHARGING_1ST &&
		battery->pdata->full_check_type_2nd != SEC_BATTERY_FULLCHARGED_NONE && !force_fullcharged) {
		battery->charging_mode = SEC_BATTERY_CHARGING_2ND;
		battery->charging_fullcharged_time = battery->charging_passed_time;
		sec_vote(battery->topoff_vote, VOTER_FULL_CHARGE, true, battery->pdata->full_check_current_2nd);
		sec_vote(battery->chgen_vote, VOTER_FULL_CHARGE, true, SEC_BAT_CHG_MODE_CHARGING);
		pr_info("%s: 1st charging is done\n", __func__);
	} else {
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
		battery->is_recharging = false;

		if (!battery->wdt_kick_disable) {
			pr_info("%s: wdt kick enable -> Charger Off, %d\n",
					__func__, battery->wdt_kick_disable);
			sec_vote(battery->chgen_vote, VOTER_FULL_CHARGE, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			pr_info("%s: 2nd charging is done\n", __func__);
			if (is_wireless_type(battery->cable_type)) {
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_EXT_PROP_WIRELESS_2ND_DONE, value);
			}
		} else {
			pr_info("%s: wdt kick disabled -> skip charger off, %d\n",
					__func__, battery->wdt_kick_disable);
		}

#if defined(CONFIG_BATTERY_AGE_FORECAST)
		sec_bat_aging_check(battery);
#endif

		/* this concept is only for power-off charging mode*/
		if (is_hv_wire_type(battery->cable_type) && is_hv_wire_type(battery->wire_status) &&
			!battery->store_mode && (battery->cable_type != SEC_BATTERY_CABLE_QC30) &&
			lpcharge && !battery->vbus_chg_by_full) {
			/* vbus level : 9V --> 5V */
			battery->vbus_chg_by_full = true;
			battery->vbus_chg_by_siop = SEC_INPUT_VOLTAGE_5V;
			muic_afc_set_voltage(SEC_INPUT_VOLTAGE_5V/1000);
			pr_info("%s: vbus is set 5V by 2nd full\n", __func__);
		}

		value.intval = POWER_SUPPLY_STATUS_FULL;
		psy_do_property(battery->pdata->fuelgauge_name, set,
			POWER_SUPPLY_PROP_STATUS, value);
	}

	/* platform can NOT get information of battery
	 * because wakeup time is too short to check uevent
	 * To make sure that target is wakeup if full-charged,
	 * activated wake lock in a few seconds
	 */
	if (battery->pdata->polling_type == SEC_BATTERY_MONITOR_ALARM)
		__pm_wakeup_event(battery->vbus_ws, jiffies_to_msecs(HZ * 10));
}

static bool sec_bat_fullcharged_check(struct sec_battery_info *battery)
{
	if ((battery->charging_mode == SEC_BATTERY_CHARGING_NONE) ||
		(battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING)) {
		dev_dbg(battery->dev,
			"%s: No Need to Check Full-Charged\n", __func__);
		return true;
	}

	if (sec_bat_check_fullcharged(battery)) {
		union power_supply_propval value = {0, };
		if (battery->capacity < 100) {
			/* update capacity max */
			value.intval = battery->capacity;
			psy_do_property(battery->pdata->fuelgauge_name, set,
					POWER_SUPPLY_PROP_CHARGE_FULL, value);
			pr_info("%s : forced full-charged sequence for the capacity(%d)\n",
					__func__, battery->capacity);
			battery->full_check_cnt = battery->pdata->full_check_count;
		} else {
			sec_bat_do_fullcharged(battery, false);
		}
	}

	dev_info(battery->dev,
		"%s: Charging Mode : %s\n", __func__,
		battery->is_recharging ?
		sec_bat_charging_mode_str[SEC_BATTERY_CHARGING_RECHARGING] :
		sec_bat_charging_mode_str[battery->charging_mode]);

	return true;
}

#if !defined(CONFIG_SEC_FACTORY)
static int sec_bat_calc_unknown_wpc_temp(
	struct sec_battery_info *battery, int batt_temp, int usb_temp)
{
	if ((battery->pdata->wpc_thermal_source != SEC_BATTERY_THERMAL_SOURCE_NONE) &&
		!is_wireless_fake_type(battery->cable_type)) {
		if (batt_temp <= (-200)) {
			if (usb_temp >= 270) {
				batt_temp = usb_temp + 60;
				pr_info("%s :usb_temp >= 27\n", __func__);
			} else if (usb_temp <= 210) {
				batt_temp = usb_temp - 50;
				pr_info("%s : usb_temp < 21\n", __func__);
			} else {
				batt_temp = (170 * usb_temp - 26100) / 60;
				pr_info("%s : 21 <= usb_temp <= 27\n", __func__);
			}
		}
	}
	return batt_temp;
}
#endif

static void sec_bat_get_temperature_info(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };
	static bool shipmode_en = false;
	int batt_temp = battery->temperature;
	int usb_temp = battery->usb_temp;
	int chg_temp = battery->chg_temp;
	int dchg_temp = battery->dchg_temp;
	int wpc_temp = battery->wpc_temp;
	int sub_bat_temp = battery->sub_bat_temp;
	int sub_chg_temp = battery->sub_chg_temp;
	int blkt_temp = battery->blkt_temp;

	/* get battery thm info */
	switch (battery->pdata->thermal_source) {
	case SEC_BATTERY_THERMAL_SOURCE_FG:
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_TEMP, value);
		batt_temp = value.intval;

		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_TEMP_AMBIENT, value);
		battery->temper_amb = value.intval;
		break;
	case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		if (battery->pdata->get_temperature_callback) {
			battery->pdata->get_temperature_callback(
				POWER_SUPPLY_PROP_TEMP, &value);
			batt_temp = value.intval;
			psy_do_property(battery->pdata->fuelgauge_name, set,
				POWER_SUPPLY_PROP_TEMP, value);

			battery->pdata->get_temperature_callback(
				POWER_SUPPLY_PROP_TEMP_AMBIENT, &value);
			battery->temper_amb = value.intval;
			psy_do_property(battery->pdata->fuelgauge_name, set,
				POWER_SUPPLY_PROP_TEMP_AMBIENT, value);
		}
		break;
	case SEC_BATTERY_THERMAL_SOURCE_ADC:
		if (sec_bat_get_value_by_adc(battery,
				SEC_BAT_ADC_CHANNEL_TEMP, &value, battery->pdata->temp_check_type)) {
			batt_temp = value.intval;
			battery->temper_amb = value.intval;
		} else {
			batt_temp = 0;
			battery->temper_amb = 0;
		}
		break;
	default:
		break;
	}

	/* get usb thm info */
	switch (battery->pdata->usb_thermal_source) {
	case SEC_BATTERY_THERMAL_SOURCE_FG:
	case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		break;
	case SEC_BATTERY_THERMAL_SOURCE_ADC:
		if (sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_USB_TEMP, &value, battery->pdata->usb_temp_check_type)) {
			usb_temp = value.intval;

			/* this should be moved */
			if (battery->vbus_limit && usb_temp <= battery->overheatlimit_recovery)
				battery->vbus_limit = false;
		} else
			usb_temp = 0;
		break;
	default:
		break;
	}

	/* get chg thm info */
	switch (battery->pdata->chg_thermal_source) {
	case SEC_BATTERY_THERMAL_SOURCE_FG:
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_TEMP, value);
		chg_temp = value.intval;
		break;
	case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		break;
	case SEC_BATTERY_THERMAL_SOURCE_ADC:
		if (sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_CHG_TEMP, &value, battery->pdata->chg_temp_check_type)) {
			chg_temp = value.intval;
		} else
			chg_temp = 0;
		break;
	default:
		break;
	}

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	if (is_pd_apdo_wire_type(battery->wire_status)) {
		switch (battery->pdata->dchg_thermal_source) {
		case SEC_BATTERY_THERMAL_SOURCE_CHG_ADC:
			psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_TEMP, value);

			dchg_temp = sec_bat_get_direct_chg_temp_adc(battery,
								value.intval, battery->pdata->adc_check_count);
			break;
		case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		case SEC_BATTERY_THERMAL_SOURCE_ADC:
			break;
		case SEC_BATTERY_THERMAL_SOURCE_FG:
			psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_TEMP, value);
			dchg_temp = value.intval;
			break;
		default:
			break;
		}
	}
#else
	dchg_temp = 0;
#endif

	/* get wpc thm info */
	switch (battery->pdata->wpc_thermal_source) {
	case SEC_BATTERY_THERMAL_SOURCE_FG:
	case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		break;
	case SEC_BATTERY_THERMAL_SOURCE_ADC:
		if (sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_WPC_TEMP, &value, battery->pdata->wpc_temp_check_type)) {
			wpc_temp = value.intval;
		} else
			wpc_temp = 0;
		break;
	default:
		break;
	}

	/* get sub bat thm info */
	switch (battery->pdata->sub_bat_thermal_source) {
	case SEC_BATTERY_THERMAL_SOURCE_FG:
	case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		break;
	case SEC_BATTERY_THERMAL_SOURCE_ADC:
		if (sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_SUB_BAT_TEMP, &value, battery->pdata->sub_bat_temp_check_type)) {
			sub_bat_temp = value.intval;
		} else
			sub_bat_temp = 0;
		break;
	default:
		break;
	}

	/* get sub chg thm info */
	switch (battery->pdata->sub_chg_thermal_source) {
	case SEC_BATTERY_THERMAL_SOURCE_FG:
	case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		break;
	case SEC_BATTERY_THERMAL_SOURCE_ADC:
		if (sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_SUB_CHG_TEMP, &value, battery->pdata->sub_chg_temp_check_type)) {
			sub_chg_temp = value.intval;

			/* set temperature */
			value.intval = (sub_chg_temp << 16) | chg_temp;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_TEMP, value);
		} else
			sub_chg_temp = 0;
		break;
	default:
		break;
	}

	/* get blkt thm info */
	switch (battery->pdata->blkt_thermal_source) {
	case SEC_BATTERY_THERMAL_SOURCE_FG:
	case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
		break;
	case SEC_BATTERY_THERMAL_SOURCE_ADC:
		if (sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_BLKT_TEMP, &value, battery->pdata->blkt_temp_check_type)) {
			blkt_temp = value.intval;
		} else
			blkt_temp = 0;
		break;
	default:
		break;
	}

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	if (battery->temperature_test_battery > -300 && battery->temperature_test_battery < 3000) {
		pr_info("%s : battery temperature test %d\n", __func__, battery->temperature_test_battery);
		batt_temp = battery->temperature_test_battery;
	}
	if (battery->temperature_test_usb > -300 && battery->temperature_test_usb < 3000) {
		pr_info("%s : usb temperature test %d\n", __func__, battery->temperature_test_usb);
		usb_temp = battery->temperature_test_usb;
	}
	if (battery->temperature_test_wpc > -300 && battery->temperature_test_wpc < 3000) {
		pr_info("%s : wpc temperature test %d\n", __func__, battery->temperature_test_wpc);
		wpc_temp = battery->temperature_test_wpc;
	}
	if (battery->temperature_test_chg > -300 && battery->temperature_test_chg < 3000) {
		pr_info("%s : chg temperature test %d\n", __func__, battery->temperature_test_chg);
		chg_temp = battery->temperature_test_chg;
	}
	if (battery->temperature_test_blkt > -300 && battery->temperature_test_blkt < 3000) {
		pr_info("%s : blkt temperature test %d\n", __func__, battery->temperature_test_blkt);
		blkt_temp = battery->temperature_test_blkt;
	}
#if defined(CONFIG_DUAL_BATTERY)
	if (battery->temperature_test_sub > -300 && battery->temperature_test_sub < 3000) {
		pr_info("%s : sub temperature test %d\n", __func__, battery->temperature_test_sub);
		sub_bat_temp = battery->temperature_test_sub;
	}
#endif
	if (battery->temperature_test_dchg > -300 && battery->temperature_test_dchg < 3000) {
		pr_info("%s : direct chg temperature test %d\n", __func__, battery->temperature_test_dchg);
		dchg_temp = battery->temperature_test_dchg;
	}
#endif

#if !defined(CONFIG_SEC_FACTORY)
	batt_temp = sec_bat_calc_unknown_wpc_temp(battery, batt_temp, usb_temp);
#endif

	battery->temperature = batt_temp;
	battery->usb_temp = usb_temp;
	battery->chg_temp = chg_temp;
	battery->dchg_temp = dchg_temp;
	battery->wpc_temp = wpc_temp;
	battery->sub_bat_temp = sub_bat_temp;
	battery->sub_chg_temp = sub_chg_temp;
	battery->blkt_temp = blkt_temp;

#if defined(CONFIG_SEC_FACTORY)
	if (battery->pdata->usb_temp_check_type) {
		if (battery->temperature <= (-200))
			value.intval = (battery->usb_temp <= (-200) ? battery->chg_temp : battery->usb_temp);
		else
			value.intval = battery->temperature;
	}
#else
	value.intval = battery->temperature;
#endif
	psy_do_property(battery->pdata->fuelgauge_name, set,
		POWER_SUPPLY_PROP_TEMP, value);

	psy_do_property(battery->pdata->fuelgauge_name, set,
		POWER_SUPPLY_PROP_TEMP_AMBIENT, value);

	if (!battery->pdata->dis_auto_shipmode_temp_ctrl) {
		if (battery->temperature < 0 && !shipmode_en) {
			value.intval = 0;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_EXT_PROP_AUTO_SHIPMODE_CONTROL, value);
			shipmode_en = true;
		} else if (battery->temperature >= 50 && shipmode_en) {
			value.intval = 1;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_EXT_PROP_AUTO_SHIPMODE_CONTROL, value);
			shipmode_en = false;
		}
	}
}

void sec_bat_get_battery_info(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };

	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	battery->voltage_now = value.intval;

	value.intval = SEC_BATTERY_VOLTAGE_AVERAGE;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	battery->voltage_avg = value.intval;

	/* Do not call it to reduce time after cable_work, this function call FG full log*/
	if (!(battery->current_event & SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL)) {
		value.intval = SEC_BATTERY_VOLTAGE_OCV;
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
		battery->voltage_ocv = value.intval;
	}

#if defined(CONFIG_DUAL_BATTERY)
	value.intval = SEC_DUAL_BATTERY_MAIN;
	psy_do_property(battery->pdata->dual_battery_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	battery->voltage_avg_main = value.intval;

	value.intval = SEC_DUAL_BATTERY_SUB;
	psy_do_property(battery->pdata->dual_battery_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	battery->voltage_avg_sub = value.intval;

	value.intval = SEC_DUAL_BATTERY_MAIN;
	psy_do_property(battery->pdata->dual_battery_name, get,
		POWER_SUPPLY_PROP_CURRENT_AVG, value);
	battery->current_now_main = value.intval;

	value.intval = SEC_DUAL_BATTERY_SUB;
	psy_do_property(battery->pdata->dual_battery_name, get,
		POWER_SUPPLY_PROP_CURRENT_AVG, value);
	battery->current_now_sub = value.intval;
#endif

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CURRENT_NOW, value);
	battery->current_now = value.intval;

	value.intval = SEC_BATTERY_CURRENT_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CURRENT_AVG, value);
	battery->current_avg = value.intval;

	value.intval = SEC_BATTERY_ISYS_AVG_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_EXT_PROP_MEASURE_SYS, value);
	battery->current_sys_avg = value.intval;

	value.intval = SEC_BATTERY_ISYS_MA;
	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_EXT_PROP_MEASURE_SYS, value);
	battery->current_sys = value.intval;

	/* input current limit in charger */
	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_PROP_CURRENT_MAX, value);
	battery->current_max = value.intval;

	psy_do_property(battery->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CHARGE_COUNTER, value);
	battery->charge_counter = value.intval;

	/* check abnormal status for wireless charging */
	if (!(battery->current_event & SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL) &&
		(is_wireless_type(battery->cable_type) || battery->wc_tx_enable)) {
		value.intval = (battery->status == POWER_SUPPLY_STATUS_FULL) ?
			100 : battery->capacity;
		psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);
	}
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	value.intval = (battery->status == POWER_SUPPLY_STATUS_FULL) ?
		100 : battery->capacity;
	psy_do_property(battery->pdata->wireless_charger_name, set,
		POWER_SUPPLY_PROP_CAPACITY, value);
#endif

	sec_bat_get_temperature_info(battery);

	/* To get SOC value (NOT raw SOC), need to reset value */
	value.intval = 0;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
	/* if the battery status was full, and SOC wasn't 100% yet,
		then ignore FG SOC, and report (previous SOC +1)% */
	battery->capacity = value.intval;

#if defined(CONFIG_DUAL_BATTERY)
	pr_info("%s:Vnow(%dmV),Vavg(%dmV),Vmain(%dmV),Vsub(%dmv),Inow(%dmA),Iavg(%dmA),Isysavg(%dmA),"
		"Inow_m(%dmA),Inow_s(%dmA),Imax(%dmA),Ichg(%dmA),Ichg_m(%dmA),Ichg_s(%dmA),SOC(%d%%),"
		"Tbat(%d),Tsub(%d),Tusb(%d),Tchg(%d),Twpc(%d)\n", __func__,
		battery->voltage_now, battery->voltage_avg,
		battery->voltage_avg_main, battery->voltage_avg_sub,
		battery->current_now, battery->current_avg,
		battery->current_sys_avg,
		battery->current_now_main, battery->current_now_sub,
		battery->current_max, battery->charging_current,
		battery->main_charging_current, battery->sub_charging_current,
		battery->capacity, battery->temperature,
		battery->sub_bat_temp, battery->usb_temp,
		battery->chg_temp, battery->wpc_temp
	);
	dev_dbg(battery->dev,
		"%s,Vavg(%dmV),Vocv(%dmV),Tamb(%d),"
		"Iavg(%dmA),Iadc(%d)\n",
		battery->present ? "Connected" : "Disconnected",
		battery->voltage_avg, battery->voltage_ocv,
		battery->temper_amb,
		battery->current_avg, battery->current_adc);
#else
	pr_info("%s:Vnow(%dmV),Vavg(%dmV),Inow(%dmA),Iavg(%dmA),Isysavg(%dmA),Imax(%dmA),Ichg(%dmA),SOC(%d%%),"
		"Tbat(%d),Tusb(%d),Tchg(%d),Twpc(%d),Tdchg(%d)\n", __func__,
		battery->voltage_now, battery->voltage_avg, battery->current_now,
		battery->current_avg, battery->current_sys_avg,
		battery->current_max, battery->charging_current,
		battery->capacity, battery->temperature,
		battery->usb_temp, battery->chg_temp, battery->wpc_temp, battery->dchg_temp
	);
#endif

#if defined(CONFIG_ARCH_QCOM) && !defined(CONFIG_ARCH_EXYNOS)
	if (!strcmp(battery->pdata->chip_vendor, "QCOM"))
		battery_last_dcvs(battery->capacity, battery->voltage_avg,
				battery->temperature, battery->current_avg);
#endif
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	if (!strcmp(battery->pdata->chip_vendor, "LSI"))
		secdbg_exin_set_batt(battery->capacity, battery->voltage_avg,
				battery->temperature, battery->current_avg);
#endif
}
EXPORT_SYMBOL(sec_bat_get_battery_info);

static void sec_bat_polling_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(
		work, struct sec_battery_info, polling_work.work);

	__pm_stay_awake(battery->monitor_ws);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	dev_dbg(battery->dev, "%s: Activated\n", __func__);
}

static void sec_bat_program_alarm(
				struct sec_battery_info *battery, int seconds)
{
	alarm_start(&battery->polling_alarm,
		    ktime_add(battery->last_poll_time, ktime_set(seconds, 0)));
}

static unsigned int sec_bat_get_polling_time(
	struct sec_battery_info *battery)
{
	if (battery->status == POWER_SUPPLY_STATUS_FULL)
		battery->polling_time = battery->pdata->polling_time[SEC_BATTERY_POLLING_TIME_CHARGING];
	else
		battery->polling_time =	battery->pdata->polling_time[battery->status];

	battery->polling_short = true;

	switch (battery->status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (battery->polling_in_sleep)
			battery->polling_short = false;
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		if (battery->polling_in_sleep)
			battery->polling_time =	battery->pdata->polling_time[SEC_BATTERY_POLLING_TIME_SLEEP];
		else
			battery->polling_time =	battery->pdata->polling_time[battery->status];

		if (!battery->wc_enable) {
			battery->polling_time = battery->pdata->polling_time[SEC_BATTERY_POLLING_TIME_CHARGING];
			pr_info("%s: wc_enable is false, polling time is 30sec\n", __func__);
		}

		battery->polling_short = false;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		if (battery->polling_in_sleep) {
			if (!(battery->pdata->full_condition_type & SEC_BATTERY_FULL_CONDITION_NOSLEEPINFULL) &&
				battery->charging_mode == SEC_BATTERY_CHARGING_NONE)
				battery->polling_time =	battery->pdata->polling_time[SEC_BATTERY_POLLING_TIME_SLEEP];
			battery->polling_short = false;
		} else {
			if (battery->charging_mode == SEC_BATTERY_CHARGING_NONE)
				battery->polling_short = false;
		}
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if ((battery->health == POWER_SUPPLY_HEALTH_OVERVOLTAGE ||
			(battery->health == POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE)) &&
			(battery->health_check_count > 0)) {
			battery->health_check_count--;
			battery->polling_time = 1;
			battery->polling_short = false;
		}
		break;
	}

#if defined(CONFIG_WIRELESS_TX_MODE)
	if (battery->wc_tx_enable) {
		battery->polling_time = 10;
		battery->polling_short = false;
		pr_info("%s: Tx mode enable polling time is 10sec\n", __func__);
	}
#endif

	if (is_pd_apdo_wire_type(battery->cable_type) &&
		(battery->pd_list.now_isApdo || battery->ta_alert_mode != OCP_NONE)) {
		battery->polling_time = 10;
		battery->polling_short = false;
		pr_info("%s: DC mode enable polling time is 10sec\n", __func__);
	}

	if (battery->polling_short)
		return battery->pdata->polling_time[SEC_BATTERY_POLLING_TIME_BASIC];
	/* set polling time to 46s to reduce current noise on wc */
	else if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS &&
			battery->status == POWER_SUPPLY_STATUS_CHARGING)
		battery->polling_time = 46;

	return battery->polling_time;
}

static bool sec_bat_is_short_polling(struct sec_battery_info *battery)
{
	/* Change the full and short monitoring sequence
	 * Originally, full monitoring was the last time of polling_count
	 * But change full monitoring to first time
	 * because temperature check is too late
	 */
	if (!battery->polling_short || battery->polling_count == 1)
		return false;
	else
		return true;
}

static void sec_bat_update_polling_count(struct sec_battery_info *battery)
{
	/* do NOT change polling count in sleep
	 * even though it is short polling
	 * to keep polling count along sleep/wakeup
	 */
	if (battery->polling_short && battery->polling_in_sleep)
		return;

	if (battery->polling_short &&
			((battery->polling_time / battery->pdata->polling_time[SEC_BATTERY_POLLING_TIME_BASIC])
			 > battery->polling_count))
		battery->polling_count++;
	else
		battery->polling_count = 1;	/* initial value = 1 */
}

static void sec_bat_set_polling(struct sec_battery_info *battery)
{
	unsigned int polling_time_temp = 0;

	dev_dbg(battery->dev, "%s: Start\n", __func__);

	polling_time_temp = sec_bat_get_polling_time(battery);

	dev_dbg(battery->dev, "%s: Status:%s, Sleep:%s, Charging:%s, Short Poll:%s\n",
		__func__, sec_bat_status_str[battery->status],
		battery->polling_in_sleep ? "Yes" : "No",
		(battery->charging_mode ==
		SEC_BATTERY_CHARGING_NONE) ? "No" : "Yes",
		battery->polling_short ? "Yes" : "No");
	dev_info(battery->dev, "%s: Polling time %d/%d sec.\n", __func__,
		battery->polling_short ?
		(polling_time_temp * battery->polling_count) :
		polling_time_temp, battery->polling_time);

	/* To sync with log above,
	 * change polling count after log is displayed
	 * Do NOT update polling count in initial monitor
	 */
	if (!battery->pdata->monitor_initial_count)
		sec_bat_update_polling_count(battery);
	else
		dev_dbg(battery->dev,
			"%s: Initial monitor %d times left.\n", __func__,
			battery->pdata->monitor_initial_count);

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		if (battery->pdata->monitor_initial_count) {
			battery->pdata->monitor_initial_count--;
			schedule_delayed_work(&battery->polling_work, HZ);
		} else
			schedule_delayed_work(&battery->polling_work,
				polling_time_temp * HZ);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		battery->last_poll_time = ktime_get_boottime();

		if (battery->pdata->monitor_initial_count) {
			battery->pdata->monitor_initial_count--;
			sec_bat_program_alarm(battery, 1);
		} else
			sec_bat_program_alarm(battery, polling_time_temp);
		break;
	case SEC_BATTERY_MONITOR_TIMER:
		break;
	default:
		break;
	}
	dev_dbg(battery->dev, "%s: End\n", __func__);
}

extern bool get_usb_enumeration_state(void);
/* To display slow charging when usb charging 100MA*/
static void sec_bat_check_slowcharging_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, slowcharging_work.work);

	if (battery->sink_status.rp_currentlvl == RP_CURRENT_LEVEL_DEFAULT &&
		battery->cable_type == SEC_BATTERY_CABLE_USB) {
		if (!get_usb_enumeration_state() &&
			(battery->current_event & SEC_BAT_CURRENT_EVENT_USB_100MA)) {
			sec_bat_set_misc_event(battery,
					BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE);
			battery->max_charge_power = mW_by_mVmA(battery->input_voltage, battery->current_max);
		}
	}

	pr_info("%s:\n", __func__);
}

static void sec_bat_siop_level_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
			struct sec_battery_info, siop_level_work.work);
	sec_battery_platform_data_t *pdata = battery->pdata;

	int icl = INT_MAX;	/* Input Current Limit */
	int fcc = INT_MAX;	/* Fast Charging Current */
	enum {
		SIOP_STEP1,	/* siop level 70 */
		SIOP_STEP2,	/* siop level 10 */
		SIOP_STEP3,	/* siop level  0 */
	};
	int siop_step = (battery->siop_level == 0) ?
		SIOP_STEP3 : ((battery->siop_level == 10) ? SIOP_STEP2 : SIOP_STEP1);

	pr_info("%s : set current by siop level(%d), siop_step(%d)\n", __func__, battery->siop_level, siop_step);

	if (battery->siop_level >= 100) {
		sec_vote(battery->fcc_vote, VOTER_SIOP, false, 0);
		sec_vote(battery->input_vote, VOTER_SIOP, false, 0);

		__pm_relax(battery->siop_level_ws);
		return;
	}

	if (is_hv_wireless_type(battery->cable_type)) {
		icl = pdata->siop_hv_wpc_icl;
		fcc = pdata->siop_hv_wpc_fcc[siop_step];
	} else if (is_nv_wireless_type(battery->cable_type)) {
		icl = pdata->siop_wpc_icl;
		fcc = pdata->siop_wpc_fcc[siop_step];
	} else if (is_hv_wire_12v_type(battery->cable_type) && is_hv_wire_type(battery->wire_status)) {
		icl = pdata->siop_hv_12v_icl;
		fcc = pdata->siop_hv_12v_fcc;
	} else if (is_hv_wire_type(battery->cable_type) && is_hv_wire_type(battery->wire_status)) {
		icl = (siop_step == SIOP_STEP3) ? pdata->siop_hv_icl_2nd : pdata->siop_hv_icl;
		fcc = pdata->siop_hv_fcc;
	} else if (is_pd_apdo_wire_type(battery->cable_type)) {
		icl = pdata->siop_apdo_icl;
		fcc = pdata->siop_apdo_fcc;
	} else if (is_pd_wire_type(battery->cable_type)) {
		icl = mA_by_mWmV(6000, battery->input_voltage);
		fcc = pdata->siop_fcc;
	} else {
		icl = pdata->siop_icl;
		fcc = pdata->siop_fcc;
	}

	pr_info("%s: icl(%d), fcc(%d)\n", __func__, icl, fcc);
	sec_vote(battery->input_vote, VOTER_SIOP, true, icl);
	sec_vote(battery->fcc_vote, VOTER_SIOP, true, fcc);

	__pm_relax(battery->siop_level_ws);
}

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
static void sec_bat_fw_init_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, fw_init_work.work);

	union power_supply_propval value = {0, };
	int ret = 0;

#if defined(CONFIG_WIRELESS_IC_PARAM)
	psy_do_property(battery->pdata->wireless_charger_name, get,
		POWER_SUPPLY_EXT_PROP_WIRELESS_CHECK_FW_VER, value);
	if (value.intval) {
		pr_info("%s: wireless firmware is already updated.\n", __func__);
		return;
	}
#endif
	if (sec_bat_check_boost_mfc_condition(battery, SEC_WIRELESS_RX_INIT) &&
		battery->capacity > 30 && !lpcharge) {
		battery->mfc_fw_update = true;
		value.intval = SEC_WIRELESS_RX_INIT;
		ret = psy_do_property(battery->pdata->wireless_charger_name, set,
			POWER_SUPPLY_EXT_PROP_CHARGE_POWERED_OTG_CONTROL, value);
		if (ret < 0)
			battery->mfc_fw_update = false;
	}
}
#endif

#if defined(CONFIG_UPDATE_BATTERY_DATA)
static void sec_bat_update_data_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, batt_data_work.work);

	sec_battery_update_data(battery->data_path);
	__pm_relax(battery->batt_data_ws);
}
#endif

static void sec_bat_misc_event_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, misc_event_work.work);
	int xor_misc_event = battery->prev_misc_event ^ battery->misc_event;

	if (xor_misc_event & BATT_MISC_EVENT_MUIC_ABNORMAL) {
		if (battery->misc_event & BATT_MISC_EVENT_MUIC_ABNORMAL)
			sec_vote(battery->chgen_vote, VOTER_MUIC_ABNORMAL, true, SEC_BAT_CHG_MODE_BUCK_OFF);
		else if (battery->prev_misc_event & BATT_MISC_EVENT_MUIC_ABNORMAL)
			sec_vote(battery->chgen_vote, VOTER_MUIC_ABNORMAL, false, 0);
	}

	pr_info("%s: change misc event(0x%x --> 0x%x)\n",
		__func__, battery->prev_misc_event, battery->misc_event);
	battery->prev_misc_event = battery->misc_event;
	__pm_relax(battery->misc_event_ws);

	__pm_stay_awake(battery->monitor_ws);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
}

static void sec_bat_calculate_safety_time(struct sec_battery_info *battery)
{
	unsigned long long expired_time = battery->expired_time;
	struct timespec ts = {0, };
	int curr = 0;
	int input_power = 0;
	int charging_power = mW_by_mVmA(battery->charging_current,
		(battery->pdata->chg_float_voltage / battery->pdata->chg_float_voltage_conv));
	static int discharging_cnt = 0;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	int direct_chg_done = 0;
	union power_supply_propval value = {0,};
#endif

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	psy_do_property(battery->pdata->charger_name, get,
					POWER_SUPPLY_EXT_PROP_DIRECT_DONE, value);
	direct_chg_done = value.intval;

	if (is_pd_apdo_wire_type(battery->cable_type) && !battery->chg_limit &&
		(battery->pd_list.now_isApdo || direct_chg_done))
		input_power = battery->pd_max_charge_power;
	else
		input_power = mW_by_mVmA(battery->input_voltage, battery->current_max);
#else
	input_power = mW_by_mVmA(battery->input_voltage, battery->current_max);
#endif

	if (battery->current_avg < 0) {
		discharging_cnt++;
	} else {
		discharging_cnt = 0;
	}

	if (discharging_cnt >= 5) {
		battery->expired_time = battery->pdata->expired_time;
		battery->prev_safety_time = 0;
		pr_info("%s : SAFETY TIME RESET! DISCHARGING CNT(%d)\n",
			__func__, discharging_cnt);
		discharging_cnt = 0;
		return;
	} else if ((battery->lcd_status || battery->wc_tx_enable) && battery->stop_timer) {
		battery->prev_safety_time = 0;
		return;
	}

	ts = ktime_to_timespec(ktime_get_boottime());

	if (battery->prev_safety_time == 0) {
		battery->prev_safety_time = ts.tv_sec;
	}

	if (input_power > charging_power) {
		curr = battery->charging_current;
	} else {
		curr = mA_by_mWmV(input_power, (battery->pdata->chg_float_voltage / battery->pdata->chg_float_voltage_conv));
		curr = (curr * 9) / 10;
	}

	if ((battery->lcd_status || battery->wc_tx_enable) && !battery->stop_timer) {
		battery->stop_timer = true;
	} else if (!(battery->lcd_status || battery->wc_tx_enable) && battery->stop_timer) {
		battery->stop_timer = false;
	}

	pr_info("%s : EXPIRED_TIME(%llu), IP(%d), CP(%d), CURR(%d), STANDARD(%d)\n",
		__func__, expired_time, input_power, charging_power, curr, battery->pdata->standard_curr);

	if (curr == 0)
		return;
	else if (curr > battery->pdata->standard_curr)
		curr = battery->pdata->standard_curr;

	expired_time = (expired_time * battery->pdata->standard_curr) / curr;

	pr_info("%s : CAL_EXPIRED_TIME(%llu) TIME NOW(%ld) TIME PREV(%ld)\n", __func__, expired_time, ts.tv_sec, battery->prev_safety_time);

	if (expired_time <= ((ts.tv_sec - battery->prev_safety_time) * 1000))
		expired_time = 0;
	else
		expired_time -= ((ts.tv_sec - battery->prev_safety_time) * 1000);

	battery->cal_safety_time = expired_time;
	expired_time = (expired_time * curr) / battery->pdata->standard_curr;

	battery->expired_time = expired_time;
	battery->prev_safety_time = ts.tv_sec;
	pr_info("%s : REMAIN_TIME(%ld) CAL_REMAIN_TIME(%ld)\n", __func__, battery->expired_time, battery->cal_safety_time);
}

static int sec_bat_check_skip_monitor(struct sec_battery_info *battery)
{
	static struct timespec old_ts = {0, };
	struct timespec c_ts = {0, };
	union power_supply_propval val = {0, };

	c_ts = ktime_to_timespec(ktime_get_boottime());

	/* monitor once after wakeup */
	if (battery->polling_in_sleep) {
		battery->polling_in_sleep = false;
		if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING && !battery->wc_tx_enable) {
			if ((unsigned long)(c_ts.tv_sec - old_ts.tv_sec) < 10 * 60) {
				psy_do_property(battery->pdata->fuelgauge_name, get,
						POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
				battery->voltage_now = val.intval;

				val.intval = 0;
				psy_do_property(battery->pdata->fuelgauge_name, get,
						POWER_SUPPLY_PROP_CAPACITY, val);
				battery->capacity = val.intval;

				sec_bat_get_temperature_info(battery);
#if defined(CONFIG_BATTERY_CISD)
				sec_bat_cisd_check(battery);
#endif
				power_supply_changed(battery->psy_bat);
				pr_info("Skip monitor work(%ld, Vnow:%d(mV), SoC:%d(%%), Tbat:%d(0.1'C))\n",
						c_ts.tv_sec - old_ts.tv_sec, battery->voltage_now, battery->capacity, battery->temperature);

				return 0;
			}
		}
	}
	/* update last monitor time */
	old_ts = c_ts;

	return 1;

}

static void sec_bat_check_store_mode(struct sec_battery_info *battery)
{
#if defined(CONFIG_SEC_FACTORY)
	if (!is_nocharge_type(battery->cable_type)) {
#else
	if (!is_nocharge_type(battery->cable_type) && battery->store_mode) {
#endif
		pr_info("%s: capacity(%d), status(%d), store_mode(%d)\n",
			 __func__, battery->capacity, battery->status, battery->store_mode);

		if (battery->capacity >= battery->pdata->store_mode_charging_max) {
			int chg_mode = (battery->misc_event & BATT_MISC_EVENT_MUIC_ABNORMAL) ?
					SEC_BAT_CHG_MODE_BUCK_OFF : SEC_BAT_CHG_MODE_CHARGING_OFF;
			/* to discharge the battery, off buck */
			if (battery->capacity > battery->pdata->store_mode_charging_max
					|| battery->pdata->store_mode_buckoff)
				chg_mode = SEC_BAT_CHG_MODE_BUCK_OFF;
			if (factory_mode && chg_mode == SEC_BAT_CHG_MODE_BUCK_OFF)
				chg_mode = SEC_BAT_CHG_MODE_CHARGING_OFF;

			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_DISCHARGING);
			sec_vote(battery->chgen_vote, VOTER_STORE_MODE, true, chg_mode);
		}

		if (battery->capacity <= battery->pdata->store_mode_charging_min &&
				battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
			sec_vote(battery->chgen_vote, VOTER_STORE_MODE, false, 0);
		}
	}
}

static void sec_bat_monitor_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work, struct sec_battery_info,
		monitor_work.work);
	union power_supply_propval val = {0, };

	dev_dbg(battery->dev, "%s: Start\n", __func__);

#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	if (sec_bat_check_wc_available(battery))
		goto skip_monitor;

	if (is_hv_wireless_type(battery->cable_type)
		&& !battery->wc_auth_retried && !lpcharge)
		sec_bat_check_wc_re_auth(battery);
#endif

	if (!sec_bat_check_skip_monitor(battery))
		goto skip_monitor;

	sec_bat_get_battery_info(battery);

#if defined(CONFIG_BATTERY_CISD)
	sec_bat_cisd_check(battery);
#endif

#if defined(CONFIG_STEP_CHARGING)
	sec_bat_check_step_charging(battery);
#endif
	/* time to full check */
	sec_bat_calc_time_to_full(battery);

#if defined(CONFIG_WIRELESS_TX_MODE)
	/* tx mode check */
	sec_bat_check_tx_mode(battery);
#endif

	/* 0. test mode */
	if (battery->test_mode) {
		sec_bat_do_test_function(battery);
		if (battery->test_mode != 0)
			goto continue_monitor;
	}

	/* 1. battery check */
	if (!sec_bat_battery_cable_check(battery))
		goto continue_monitor;

	/* 2. voltage check */
	if (!sec_bat_voltage_check(battery))
		goto continue_monitor;

	/* monitor short routine in initial monitor */
	if (battery->pdata->monitor_initial_count || sec_bat_is_short_polling(battery))
		goto skip_current_monitor;

	/* 3. time management */
	if (!sec_bat_time_management(battery))
		goto continue_monitor;

	/* 4. bat thm check */
	sec_bat_thermal_check(battery);

	/* 5. full charging check */
	if (!(battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING))
		sec_bat_fullcharged_check(battery);

	/* 7. additional check */
	if (battery->pdata->monitor_additional_check)
		battery->pdata->monitor_additional_check();

#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	if (is_wireless_type(battery->cable_type) && !battery->wc_cv_mode && battery->charging_passed_time > 10)
		sec_bat_wc_cv_mode_check(battery);
#endif

#if defined(CONFIG_STEP_CHARGING)
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	if (is_pd_apdo_wire_type(battery->cable_type))
		sec_bat_check_dc_step_charging(battery);
#endif
#endif

continue_monitor:
	/* clear HEATING_CONTROL*/
	sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL);

	/* calculate safety time */
	if (!battery->charging_block)
		sec_bat_calculate_safety_time(battery);

	/* set charging current */
	sec_bat_set_charging_current(battery);

skip_current_monitor:
	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_EXT_PROP_MONITOR_WORK, val);
	if (battery->pdata->wireless_charger_name)
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_MONITOR_WORK, val);

	pr_info("%s: Status(%s), mode(%s), Health(%s), Cable(%s, %s, %d, %d), rp(%d), HV(%s, %d)\n",
			__func__,
			sec_bat_status_str[battery->status],
			sec_bat_charging_mode_str[battery->charging_mode],
			sec_bat_health_str[battery->health],
			sec_cable_type[battery->cable_type],
			sec_cable_type[battery->wire_status],
			battery->muic_cable_type,
			battery->pd_usb_attached,
			battery->sink_status.rp_currentlvl,
			battery->hv_chg_name, battery->vbus_chg_by_siop);
	pr_info("%s: lcd(%d), slate(%d), store(%d), siop_level(%d), sleep_mode(%d), Cycle(%d)\n",
			__func__,
			battery->lcd_status,
			is_slate_mode(battery),
			battery->store_mode,
			battery->siop_level,
			battery->sleep_mode
#if defined(CONFIG_BATTERY_AGE_FORECAST)
			, battery->batt_cycle
#else
			, -1
#endif
			);

#if defined(CONFIG_WIRELESS_TX_MODE)
	if (battery->wc_tx_enable) {
		pr_info("@Tx_Mode %s: Rx(%s), WC_TX_VOUT(%dmV), UNO_IOUT(%d), MFC_IOUT(%d) AFC_DISABLE(%d)\n",
			__func__, sec_bat_rx_type_str[battery->wc_rx_type],
			(battery->wc_tx_vout == WC_TX_VOUT_5_0V ? 5000 : (5000 + (battery->wc_tx_vout * 500))),
			battery->tx_uno_iout, battery->tx_mfc_iout, battery->afc_disable);

#if defined(CONFIG_TX_GEAR_AOV)
		if ((battery->wc_rx_type == SS_GEAR) && (battery->tx_switch_mode != TX_SWITCH_CHG_ONLY)) {
			psy_do_property(battery->pdata->wireless_charger_name, get,
				POWER_SUPPLY_EXT_PROP_WIRELESS_OP_FREQ, val);

			if ((((val.intval < battery->pdata->tx_aov_freq_low) && (battery->wc_tx_vout < WC_TX_VOUT_7_5V))
				|| ((val.intval > battery->pdata->tx_aov_freq_high)
				&& (battery->wc_tx_vout > WC_TX_VOUT_5_0V)))
				&& battery->wc_found_gear_freq) {
				pr_info("@Tx_Mode %s: new_freq(%d), wc_tx_vout(%d), wc_tx_freq(%d)\n",
					__func__, val.intval, battery->wc_tx_vout, battery->wc_tx_freq);
				battery->wc_found_gear_freq = false;
				sec_bat_run_wpc_tx_work(battery, 0);
			}
		}
#endif
	}
#endif

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	pr_info("%s: battery->stability_test(%d), battery->eng_not_full_status(%d)\n",
			__func__, battery->stability_test, battery->eng_not_full_status);
#endif

	/* store mode & fac bin */
	sec_bat_check_store_mode(battery);

	power_supply_changed(battery->psy_bat);

skip_monitor:
	sec_bat_set_polling(battery);

#if defined(CONFIG_WIRELESS_TX_MODE)
	if (battery->tx_switch_mode_change)
		sec_bat_run_wpc_tx_work(battery, 0);
#endif

	if (battery->capacity <= 0 || battery->health_change)
		__pm_wakeup_event(battery->monitor_ws, jiffies_to_msecs(HZ * 5));
	else
		__pm_relax(battery->monitor_ws);

	dev_dbg(battery->dev, "%s: End\n", __func__);

	return;
}

static enum alarmtimer_restart sec_bat_alarm(struct alarm *alarm, ktime_t now)
{
	struct sec_battery_info *battery = container_of(alarm,
				struct sec_battery_info, polling_alarm);

	dev_dbg(battery->dev, "%s\n", __func__);

	/* In wake up, monitor work will be queued in complete function
	 * To avoid duplicated queuing of monitor work,
	 * do NOT queue monitor work in wake up by polling alarm
	 */
	if (!battery->polling_in_sleep) {
		__pm_stay_awake(battery->monitor_ws);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		dev_dbg(battery->dev, "%s: Activated\n", __func__);
	}

	return ALARMTIMER_NORESTART;
}

static void sec_bat_check_input_voltage(struct sec_battery_info *battery)
{
	unsigned int voltage = 0;
	int input_current = battery->pdata->charging_current[battery->cable_type].input_current_limit;

	if (is_pd_wire_type(battery->cable_type)) {
		battery->max_charge_power = battery->pd_max_charge_power;
		return;
	} else if (is_hv_wire_12v_type(battery->cable_type))
		voltage = SEC_INPUT_VOLTAGE_12V;
	else if (is_hv_wire_9v_type(battery->cable_type))
		voltage = SEC_INPUT_VOLTAGE_9V;
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	else if (battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20 ||
			battery->cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20)
		voltage = battery->wc20_vout;
	else if (is_hv_wireless_type(battery->cable_type) ||
			battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV)
		voltage = SEC_INPUT_VOLTAGE_10V;
	else if (is_nv_wireless_type(battery->cable_type))
		voltage = SEC_INPUT_VOLTAGE_5_5V;
#endif
	else
		voltage = SEC_INPUT_VOLTAGE_5V;

	battery->input_voltage = voltage;
	battery->charge_power = mW_by_mVmA(voltage, input_current);
#if !defined(CONFIG_SEC_FACTORY)
	if (battery->charge_power > battery->max_charge_power)
#endif
		battery->max_charge_power = battery->charge_power;

	pr_info("%s: input_voltage:%dmV, charge_power:%dmW, max_charge_power:%dmW)\n", __func__,
		battery->input_voltage, battery->charge_power, battery->max_charge_power);
}

static void sec_bat_set_usb_configure(struct sec_battery_info *battery, int usb_status)
{
	int cable_work_delay = 0;

	pr_info("%s: usb configured %d -> %d\n", __func__, battery->prev_usb_conf, usb_status);

	if (usb_status == USB_CURRENT_UNCONFIGURED) {
		sec_bat_set_current_event(battery,
				SEC_BAT_CURRENT_EVENT_USB_100MA, SEC_BAT_CURRENT_EVENT_USB_STATE);
		if (battery->cable_type == SEC_BATTERY_CABLE_USB && !lpcharge) {
			sec_vote(battery->fcc_vote, VOTER_USB_100MA, true, 100);
			sec_vote(battery->input_vote, VOTER_USB_100MA, true, 100);
		}
	} else if (usb_status == USB_CURRENT_HIGH_SPEED || usb_status == USB_CURRENT_SUPER_SPEED) {
		sec_bat_set_misc_event(battery, 0, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE);
		sec_vote(battery->fcc_vote, VOTER_USB_100MA, false, 0);
		sec_vote(battery->input_vote, VOTER_USB_100MA, false, 0);
		if (usb_status == USB_CURRENT_HIGH_SPEED) {
			sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_USB_STATE);
			if (battery->cable_type == SEC_BATTERY_CABLE_USB) {
				sec_vote(battery->fcc_vote, VOTER_CABLE, true,
						battery->pdata->default_usb_charging_current);
				sec_vote(battery->input_vote, VOTER_CABLE, true,
						battery->pdata->default_usb_input_current);
			}
		} else {
			sec_bat_set_current_event(battery,
					SEC_BAT_CURRENT_EVENT_USB_SUPER, SEC_BAT_CURRENT_EVENT_USB_STATE);
			if (battery->cable_type == SEC_BATTERY_CABLE_USB) {
				sec_vote(battery->fcc_vote, VOTER_CABLE, true, USB_CURRENT_SUPER_SPEED);
				sec_vote(battery->input_vote, VOTER_CABLE, true, USB_CURRENT_SUPER_SPEED);
			}
		}
		if (battery->sink_status.rp_currentlvl == RP_CURRENT_LEVEL3) {
			if (battery->current_event & SEC_BAT_CURRENT_EVENT_HV_DISABLE) {
				sec_vote(battery->fcc_vote, VOTER_CABLE, true,
						battery->pdata->default_charging_current);
				sec_vote(battery->input_vote, VOTER_CABLE, true,
						battery->pdata->default_input_current);
			} else {
				if (battery->store_mode) {
					sec_vote(battery->fcc_vote, VOTER_CABLE, true,
							battery->pdata->max_charging_current);
					sec_vote(battery->input_vote, VOTER_CABLE, true,
							battery->pdata->rp_current_rdu_rp3);
				} else {
					sec_vote(battery->fcc_vote, VOTER_CABLE, true,
							battery->pdata->max_charging_current);
					sec_vote(battery->input_vote, VOTER_CABLE, true,
							battery->pdata->rp_current_rp3);
				}
			}
		} else if (battery->sink_status.rp_currentlvl == RP_CURRENT_LEVEL2) {
			sec_vote(battery->fcc_vote, VOTER_CABLE, true, battery->pdata->rp_current_rp2);
			sec_vote(battery->input_vote, VOTER_CABLE, true, battery->pdata->rp_current_rp2);
		}
	} else if (usb_status == USB_CURRENT_SUSPENDED) {
		sec_bat_set_misc_event(battery, 0, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE);
		sec_bat_set_current_event(battery,
				SEC_BAT_CURRENT_EVENT_USB_SUSPENDED, SEC_BAT_CURRENT_EVENT_USB_STATE);
		sec_vote(battery->chgen_vote, VOTER_SUSPEND, true, SEC_BAT_CHG_MODE_BUCK_OFF);
		sec_vote(battery->fcc_vote, VOTER_USB_100MA, true, 100);
		sec_vote(battery->input_vote, VOTER_USB_100MA, true, 100);
		cable_work_delay = 500;
	} else if (usb_status == USB_CURRENT_CLEAR) {
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_USB_STATE);
		sec_vote(battery->chgen_vote, VOTER_SUSPEND, false, 0);
		sec_vote(battery->fcc_vote, VOTER_USB_100MA, false, 0);
		sec_vote(battery->input_vote, VOTER_USB_100MA, false, 0);
	}

	if (usb_status != USB_CURRENT_SUSPENDED)
		sec_vote(battery->chgen_vote, VOTER_SUSPEND, false, 0);

	battery->prev_usb_conf = usb_status;

	cancel_delayed_work(&battery->cable_work);
	__pm_stay_awake(battery->cable_ws);
	queue_delayed_work(battery->monitor_wqueue, &battery->cable_work,
			msecs_to_jiffies(cable_work_delay));
}

#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
static void sec_bat_wc_headroom_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
			struct sec_battery_info, wc_headroom_work.work);

	sec_bat_wc_headroom_work_content(battery);
}

static void sec_bat_ext_event_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
			struct sec_battery_info, ext_event_work.work);

	sec_bat_ext_event_work_content(battery);
}

static void sec_bat_wpc_tx_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, wpc_tx_work.work);

	sec_bat_wpc_tx_work_content(battery);
}

static void sec_bat_txpower_calc_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, wpc_txpower_calc_work.work);

	sec_bat_txpower_calc(battery);
}

static void sec_bat_otg_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, otg_work.work);
	set_wireless_otg_input_current(battery);
}

static void sec_bat_wc20_current_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, wc20_current_work.work);

	sec_bat_set_wireless20_current(battery, battery->wc20_rx_power);
	sec_bat_predict_wireless20_time_to_full_current(battery, battery->wc20_rx_power);
}

#endif

static void sec_bat_cable_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, cable_work.work);
	union power_supply_propval val = {0, };
	int current_cable_type = SEC_BATTERY_CABLE_NONE;
	unsigned int input_current;
	unsigned int charging_current;
	int clear_afc_evt = 0;
	int prev_cable_type = battery->cable_type;
	int monitor_work_delay = 0;

	dev_info(battery->dev, "%s: Start\n", __func__);
	sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL,
			SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL);

	if (is_pd_wire_type(battery->wire_status)) {
		sec_bat_get_input_current_in_power_list(battery);
		sec_bat_get_charging_current_in_power_list(battery);
		if (battery->sink_status.selected_pdo_num ==
			battery->sink_status.current_pdo_num) {
			sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SELECT_PDO);
			sec_vote(battery->input_vote, VOTER_SELECT_PDO, false, 0);
		}
#if defined(CONFIG_STEP_CHARGING)
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		if (!is_pd_apdo_wire_type(battery->cable_type)) {
			sec_bat_reset_step_charging(battery);
		} else if (is_pd_apdo_wire_type(battery->cable_type) && (battery->ta_alert_mode != OCP_NONE)) {
			battery->ta_alert_mode = OCP_WA_ACTIVE;
			sec_bat_reset_step_charging(battery);
		}
#else
		sec_bat_reset_step_charging(battery);
#endif
#endif
		if (!battery->pd_list.pd_info[battery->pd_list.now_pd_index].comm_capable
			|| !battery->pd_list.pd_info[battery->pd_list.now_pd_index].suspend) {
			pr_info("%s : clear suspend event now_pd_index:%d, comm:%d, suspend:%d\n", __func__,
				battery->pd_list.now_pd_index,
				battery->pd_list.pd_info[battery->pd_list.now_pd_index].comm_capable,
				battery->pd_list.pd_info[battery->pd_list.now_pd_index].suspend);
			sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_USB_SUSPENDED);
			sec_vote(battery->chgen_vote, VOTER_SUSPEND, false, 0);
			sec_vote(battery->fcc_vote, VOTER_USB_100MA, false, 0);
			sec_vote(battery->input_vote, VOTER_USB_100MA, false, 0);
		}
	}

#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	current_cable_type = sec_bat_choose_cable_type(battery);
#else
	current_cable_type = battery->wire_status;
#endif

	if ((current_cable_type == battery->cable_type)
			&& !is_slate_mode(battery)
			&& !(battery->current_event & SEC_BAT_CURRENT_EVENT_USB_SUSPENDED)) {
		if (is_pd_wire_type(current_cable_type) && is_pd_wire_type(battery->cable_type)) {
			cancel_delayed_work(&battery->afc_work);
			__pm_relax(battery->afc_ws);
			sec_bat_set_current_event(battery, 0,
				SEC_BAT_CURRENT_EVENT_AFC | SEC_BAT_CURRENT_EVENT_AICL);
			sec_vote(battery->input_vote, VOTER_AICL, false, 0);
			sec_vote(battery->input_vote, VOTER_VBUS_CHANGE, false, 0);
			power_supply_changed(battery->psy_bat);
		} else if (battery->prev_usb_conf != USB_CURRENT_NONE) {
			dev_info(battery->dev, "%s: set usb charging current to %d mA\n",
				__func__, battery->prev_usb_conf);
			sec_bat_set_charging_current(battery);
			battery->prev_usb_conf = USB_CURRENT_NONE;
		}
		dev_info(battery->dev, "%s: Cable is NOT Changed(%d)\n",
			__func__, battery->cable_type);
		/* Do NOT activate cable work for NOT changed */
		goto end_of_cable_work;
	} else {
		sec_bat_set_threshold(battery);
	}

	/* to clear this value when cable type switched without dettach */
	if ((is_wired_type(battery->cable_type) && is_wireless_type(current_cable_type))
		|| (is_wireless_type(battery->cable_type) && is_wired_type(current_cable_type))
		|| (battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC))
		battery->max_charge_power = 0;

	if (current_cable_type == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT)
		current_cable_type = SEC_BATTERY_CABLE_9V_TA;

	if (!can_usb_suspend_type(current_cable_type) &&
			battery->current_event & SEC_BAT_CURRENT_EVENT_USB_SUSPENDED) {
		pr_info("%s: clear suspend event prev_cable_type:%s -> %s\n", __func__,
				sec_cable_type[battery->cable_type], sec_cable_type[current_cable_type]);
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_USB_SUSPENDED);
		sec_vote(battery->chgen_vote, VOTER_SUSPEND, false, 0);
		sec_vote(battery->fcc_vote, VOTER_USB_100MA, false, 0);
		sec_vote(battery->input_vote, VOTER_USB_100MA, false, 0);
	}

	prev_cable_type = battery->cable_type;
	battery->cable_type = current_cable_type;

	if (battery->cable_type != SEC_BATTERY_CABLE_WIRELESS_FAKE) {
		/* set online(cable type) */
		val.intval = battery->cable_type;
		if (is_slate_mode(battery))
			val.intval = SEC_BATTERY_CABLE_NONE;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_ONLINE, val);
		val.intval = battery->cable_type;
		psy_do_property(battery->pdata->fuelgauge_name, set,
			POWER_SUPPLY_PROP_ONLINE, val);
		sec_vote_refresh(battery->chgen_vote);
	}

	/* need to move to wireless set property */
	battery->wpc_vout_level = WIRELESS_VOUT_10V;
	if (is_wireless_type(battery->cable_type)) {
		power_supply_changed(battery->psy_bat);
		/* After 10sec wireless charging, Vrect headroom has to be reduced */
		__pm_stay_awake(battery->wc_headroom_ws);
		queue_delayed_work(battery->monitor_wqueue, &battery->wc_headroom_work,
			msecs_to_jiffies(10000));
	} else if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE) {
		power_supply_changed(battery->psy_bat);
	}

	/* For wire + wireless case */
	if (!is_wireless_type(prev_cable_type) && is_wireless_type(battery->cable_type)) {
		pr_info("%s: non-wl -> wl: prev_cable(%s) , current_cable(%s)\n",
			__func__, sec_cable_type[prev_cable_type], sec_cable_type[battery->cable_type]);
		battery->wc_cv_mode = false;
		battery->charging_passed_time = 0;
	}

	if (battery->pdata->check_cable_result_callback)
		battery->pdata->check_cable_result_callback(battery->cable_type);
	/* platform can NOT get information of cable connection
	 * because wakeup time is too short to check uevent
	 * To make sure that target is wakeup
	 * if cable is connected and disconnected,
	 * activated wake lock in a few seconds
	 */
	__pm_wakeup_event(battery->vbus_ws, jiffies_to_msecs(HZ * 10));

	if (is_nocharge_type(battery->cable_type) ||
		((battery->pdata->cable_check_type &
		SEC_BATTERY_CABLE_CHECK_NOINCOMPATIBLECHARGE) &&
		battery->cable_type == SEC_BATTERY_CABLE_UNKNOWN)) {
		int i;

		pr_info("%s: prev_cable_type(%d)\n", __func__, prev_cable_type);

		/* initialize all status */
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
		battery->vbus_chg_by_siop = SEC_INPUT_VOLTAGE_NONE;
		battery->vbus_chg_by_full = false;
		battery->is_recharging = false;
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.ab_vbat_check_count = 0;
		battery->cisd.state &= ~CISD_STATE_OVER_VOLTAGE;
#endif
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
		battery->wc20_power_class = 0;
		sec_bat_predict_wireless20_time_to_full_current(battery, -1);
		battery->wc20_rx_power = 0;
		battery->wc20_vout = 0;
#endif

		battery->input_voltage = 0;
		battery->charge_power = 0;
		battery->max_charge_power = 0;
		battery->pd_max_charge_power = 0;
		sec_bat_set_charging_status(battery,
				POWER_SUPPLY_STATUS_DISCHARGING);
		battery->thermal_zone = BAT_THERMAL_NORMAL;
		battery->chg_limit = false;
		battery->mix_limit = false;
		battery->chg_limit_recovery_cable = SEC_BATTERY_CABLE_NONE;
		battery->wc_heating_start_time = 0;
		sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
		battery->prev_usb_conf = USB_CURRENT_NONE;
		battery->ta_alert_mode = OCP_NONE;
		cancel_delayed_work(&battery->afc_work);
		__pm_relax(battery->afc_ws);
		sec_bat_change_default_current(battery, SEC_BATTERY_CABLE_USB,
			battery->pdata->default_usb_input_current,
			battery->pdata->default_usb_charging_current);
		sec_bat_change_default_current(battery, SEC_BATTERY_CABLE_TA,
			battery->pdata->default_input_current,
			battery->pdata->default_charging_current);
		sec_bat_change_default_current(battery, SEC_BATTERY_CABLE_HV_WIRELESS_20,
			battery->pdata->default_wc20_input_current,
			battery->pdata->default_wc20_charging_current);
		/* usb default current is 100mA before configured*/
		sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_USB_100MA,
					(SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE |
					 SEC_BAT_CURRENT_EVENT_AFC |
					 SEC_BAT_CURRENT_EVENT_VBAT_OVP |
					 SEC_BAT_CURRENT_EVENT_VSYS_OVP |
					 SEC_BAT_CURRENT_EVENT_CHG_LIMIT |
					 SEC_BAT_CURRENT_EVENT_AICL |
					 SEC_BAT_CURRENT_EVENT_SELECT_PDO |
					 SEC_BAT_CURRENT_EVENT_WDT_EXPIRED |
					 SEC_BAT_CURRENT_EVENT_SAFETY_TMR |
					 SEC_BAT_CURRENT_EVENT_25W_OCP |
					 SEC_BAT_CURRENT_EVENT_DC_ERR |
					 SEC_BAT_CURRENT_EVENT_USB_STATE |
					 SEC_BAT_CURRENT_EVENT_SEND_UVDM));

		battery->wc_cv_mode = false;
		battery->is_sysovlo = false;
		battery->is_vbatovlo = false;
		battery->is_abnormal_temp = false;
		battery->auto_mode = false;
#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
		if (lpcharge)
			battery->usb_thm_status = USB_THM_NORMAL;
#endif
		for (i = 0; i < VOTER_MAX; i++) {
			if (i == VOTER_SIOP ||
				i == VOTER_SLATE ||
				i == VOTER_AGING_STEP ||
				i == VOTER_WC_TX ||
				i == VOTER_MUIC_ABNORMAL)
			continue;
			sec_vote(battery->topoff_vote, i, false, 0);
			sec_vote(battery->chgen_vote, i, false, 0);
			sec_vote(battery->input_vote, i, false, 0);
			sec_vote(battery->fcc_vote, i, false, 0);
			sec_vote(battery->fv_vote, i, false, 0);
		}
		cancel_delayed_work(&battery->slowcharging_work);
		/* Discharging has 100mA current unlike non LEGO model */
		sec_vote(battery->fcc_vote, VOTER_USB_100MA, true, 100);
		sec_vote(battery->input_vote, VOTER_USB_100MA, true, 100);
	} else if (is_slate_mode(battery)) {
		int j = 0;

		dev_info(battery->dev, "%s: slate mode on\n", __func__);
		/* Some charger ic's buck is enabled after vbus off, So disable buck again*/
		sec_vote_refresh(battery->chgen_vote);
		battery->is_recharging = false;
		battery->cable_type = SEC_BATTERY_CABLE_NONE;
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
		battery->health = POWER_SUPPLY_HEALTH_GOOD;
		battery->is_sysovlo = false;
		battery->is_vbatovlo = false;
		battery->is_abnormal_temp = false;

		for (j = 0; j < VOTER_MAX; j++) {
			if (j == VOTER_SIOP ||
					j == VOTER_SLATE ||
					j == VOTER_AGING_STEP ||
					j == VOTER_WC_TX)
				continue;
			sec_vote(battery->topoff_vote, j, false, 0);
			sec_vote(battery->chgen_vote, j, false, 0);
			sec_vote(battery->input_vote, j, false, 0);
			sec_vote(battery->fcc_vote, j, false, 0);
			sec_vote(battery->fv_vote, j, false, 0);
		}

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_DC_INITIALIZE, val);
#endif
		battery->thermal_zone = BAT_THERMAL_NORMAL;
		sec_bat_set_charging_status(battery,
			POWER_SUPPLY_STATUS_DISCHARGING);
	} else if (battery->current_event & SEC_BAT_CURRENT_EVENT_USB_SUSPENDED) {
		dev_info(battery->dev, "%s: usb suspend\n", __func__);
		/* Some charger ic's buck is enabled after vbus off, So disable buck again*/
		sec_vote_refresh(battery->chgen_vote);
		battery->is_recharging = false;
		battery->cable_type = SEC_BATTERY_CABLE_NONE;
		battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
		battery->health = POWER_SUPPLY_HEALTH_GOOD;
		battery->is_sysovlo = false;
		battery->is_vbatovlo = false;
		battery->is_abnormal_temp = false;
		battery->thermal_zone = BAT_THERMAL_NORMAL;
		sec_bat_set_charging_status(battery,
			POWER_SUPPLY_STATUS_DISCHARGING);
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_USB_SUSPENDED) {
			battery->prev_usb_conf = USB_CURRENT_NONE;
			monitor_work_delay = 3000;
			goto run_monitor_work;
		}
	} else if (is_nocharge_type(prev_cable_type)) {
#if defined(CONFIG_EN_OOPS)
		val.intval = battery->cable_type;
		psy_do_property(battery->pdata->fuelgauge_name, set,
				POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, val);
#endif
		dev_info(battery->dev,
			"%s: c: %d, ov: %d, at: %d, cb: %d, tz: %d\n", __func__,
			battery->cable_type, battery->is_vbatovlo, battery->is_abnormal_temp,
			battery->charging_block, battery->thermal_zone);
		if (battery->pdata->full_check_type !=
				SEC_BATTERY_FULLCHARGED_NONE)
			battery->charging_mode =
				SEC_BATTERY_CHARGING_1ST;
		else
			battery->charging_mode =
				SEC_BATTERY_CHARGING_2ND;
		sec_bat_set_charging_status(battery,
				POWER_SUPPLY_STATUS_CHARGING);
		sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);

		if (battery->cable_type == SEC_BATTERY_CABLE_TA ||
			battery->cable_type == SEC_BATTERY_CABLE_WIRELESS ||
			battery->cable_type == SEC_BATTERY_CABLE_PMA_WIRELESS ||
			(is_hv_wire_type(battery->cable_type) &&
			(battery->wc_status == SEC_WIRELESS_PAD_WPC_PREPARE_HV_20 ||
			battery->wc_status == SEC_WIRELESS_PAD_WPC_HV_20))) {
			sec_bat_check_afc_input_current(battery);
		} else {
			clear_afc_evt = true;
		}

#if defined(CONFIG_ENABLE_FULL_BY_SOC)
		if (battery->capacity >= 100) {
			sec_bat_do_fullcharged(battery, true);
			dev_info(battery->dev,
					"%s: charging start at full, do not turn on charging\n", __func__);
		}
		sec_vote(battery->chgen_vote, VOTER_CABLE, true, SEC_BAT_CHG_MODE_CHARGING);
#else
		sec_vote(battery->chgen_vote, VOTER_CABLE, true, SEC_BAT_CHG_MODE_CHARGING);
#endif
		if (battery->cable_type == SEC_BATTERY_CABLE_USB && !lpcharge)
			queue_delayed_work(battery->monitor_wqueue, &battery->slowcharging_work,
						msecs_to_jiffies(3000));
		if (is_hv_wireless_type(battery->cable_type) && battery->sleep_mode) {
			sec_vote(battery->input_vote, VOTER_SLEEP_MODE, true, battery->pdata->sleep_mode_limit_current);
		}
		ttf_work_start(battery);

	}

#if defined(CONFIG_STEP_CHARGING)
	if (!is_hv_wire_type(battery->cable_type)
		&& !is_pd_wire_type(battery->cable_type)
		&& !(battery->sink_status.rp_currentlvl == RP_CURRENT_LEVEL3))
		sec_bat_reset_step_charging(battery);
#endif

	/* Check VOTER_SIOP to set up current based on cable_type */
	__pm_stay_awake(battery->siop_level_ws);
	queue_delayed_work(battery->monitor_wqueue, &battery->siop_level_work, 0);

	if (battery->cable_type != SEC_BATTERY_CABLE_WIRELESS_FAKE) {
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_AICL);
		sec_vote(battery->input_vote, VOTER_AICL, false, 0);
		/* to init battery type current when wireless charging -> battery case */
		sec_vote_refresh(battery->input_vote);
		if (battery->status != POWER_SUPPLY_STATUS_DISCHARGING)
			sec_bat_check_input_voltage(battery);
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
		set_wireless_otg_input_current(battery);
#endif
		input_current =  battery->pdata->charging_current[current_cable_type].input_current_limit;
		charging_current = battery->pdata->charging_current[current_cable_type].fast_charging_current;
		sec_vote(battery->fcc_vote, VOTER_CABLE, true, charging_current);
		sec_vote(battery->input_vote, VOTER_CABLE, true, input_current);
	}

	if ((!is_nocharge_type(battery->cable_type) && battery->cable_type != SEC_BATTERY_CABLE_USB) ||
		lpcharge) {
		sec_vote(battery->fcc_vote, VOTER_USB_100MA, false, 0);
		sec_vote(battery->input_vote, VOTER_USB_100MA, false, 0);
	}

	if (clear_afc_evt) {
		cancel_delayed_work(&battery->afc_work);
		__pm_relax(battery->afc_ws);
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_AFC);
		sec_vote(battery->input_vote, VOTER_VBUS_CHANGE, false, 0);
	}

	/* polling time should be reset when cable is changed
	 * polling_in_sleep should be reset also
	 * before polling time is re-calculated
	 * to prevent from counting 1 for events
	 * right after cable is connected
	 */
	battery->polling_in_sleep = false;
	sec_bat_get_polling_time(battery);

	dev_info(battery->dev,
		"%s: Status:%s, Sleep:%s, Charging:%s, Short Poll:%s\n",
		__func__, sec_bat_status_str[battery->status],
		battery->polling_in_sleep ? "Yes" : "No",
		(battery->charging_mode ==
		SEC_BATTERY_CHARGING_NONE) ? "No" : "Yes",
		battery->polling_short ? "Yes" : "No");
	dev_info(battery->dev,
		"%s: Polling time is reset to %d sec.\n", __func__,
		battery->polling_time);

	battery->polling_count = 1;	/* initial value = 1 */

run_monitor_work:
	__pm_stay_awake(battery->monitor_ws);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, msecs_to_jiffies(monitor_work_delay));
end_of_cable_work:
	__pm_relax(battery->cable_ws);
	dev_info(battery->dev, "%s: End\n", __func__);
}

static void sec_bat_afc_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, afc_work.work);
	union power_supply_propval value = {0, };

	dev_info(battery->dev, "%s: start\n", __func__);
	psy_do_property(battery->pdata->charger_name, get,
		POWER_SUPPLY_PROP_CURRENT_MAX, value);
	battery->current_max = value.intval;

	if (battery->current_event & SEC_BAT_CURRENT_EVENT_AFC) {
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_AFC);
		sec_vote(battery->input_vote, VOTER_VBUS_CHANGE, false, 0);
		if ((battery->wc_status != SEC_WIRELESS_PAD_NONE &&
			battery->current_max >= battery->pdata->pre_wc_afc_input_current) ||
			(is_hv_wire_type(battery->cable_type) || battery->cable_type == SEC_BATTERY_CABLE_TA)) {
#if defined(CONFIG_BATTERY_CISD)
			if (battery->cable_type == SEC_BATTERY_CABLE_TA)
				battery->cisd.cable_data[CISD_CABLE_TA]++;
#endif
		}
#if defined(CONFIG_WIRELESS_TX_MODE)
		if (battery->wc_tx_enable)
			sec_bat_run_wpc_tx_work(battery, 0);
#endif
	}
	dev_info(battery->dev, "%s: End\n", __func__);
	__pm_relax(battery->afc_ws);
}

static int sec_bat_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	int current_cable_type = SEC_BATTERY_CABLE_NONE;
	int full_check_type = SEC_BATTERY_FULLCHARGED_NONE;
	union power_supply_propval value = {0, };
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	dev_dbg(battery->dev,
		"%s: (%d,%d)\n", __func__, psp, val->intval);

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (battery->charging_mode == SEC_BATTERY_CHARGING_1ST)
			full_check_type = battery->pdata->full_check_type;
		else
			full_check_type = battery->pdata->full_check_type_2nd;
		if ((full_check_type == SEC_BATTERY_FULLCHARGED_CHGINT) &&
			(val->intval == POWER_SUPPLY_STATUS_FULL))
			sec_bat_do_fullcharged(battery, false);
		sec_bat_set_charging_status(battery, val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (battery->cable_type != SEC_BATTERY_CABLE_WIRELESS_FAKE) {
			__pm_stay_awake(battery->monitor_ws);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->monitor_work, msecs_to_jiffies(100));
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		current_cable_type = val->intval;
#if !IS_ENABLED(CONFIG_PDIC_NOTIFIER) && !IS_ENABLED(CONFIG_CCIC_NOTIFIER)
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
		if ((battery->muic_cable_type != ATTACHED_DEV_SMARTDOCK_TA_MUIC)
		    && ((current_cable_type == SEC_BATTERY_CABLE_SMART_OTG) ||
			(current_cable_type == SEC_BATTERY_CABLE_SMART_NOTG)))
			break;
#endif
#endif

		if (current_cable_type < 0) {
			dev_info(battery->dev,
					"%s: ignore event(%d)\n",
					__func__, current_cable_type);
		} else if (current_cable_type == SEC_BATTERY_CABLE_OTG) {
			battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
			battery->is_recharging = false;
			sec_bat_set_charging_status(battery,
					POWER_SUPPLY_STATUS_DISCHARGING);
			battery->cable_type = current_cable_type;
			__pm_stay_awake(battery->monitor_ws);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->monitor_work, 0);
			break;
		} else {
			battery->wire_status = current_cable_type;
			if (is_nocharge_type(battery->wire_status) &&
				(battery->wc_status != SEC_WIRELESS_PAD_NONE) )
				current_cable_type = SEC_BATTERY_CABLE_WIRELESS;
		}
		dev_info(battery->dev,
				"%s: current_cable(%d), wc_status(%d), wire_status(%d)\n",
				__func__, current_cable_type, battery->wc_status,
				battery->wire_status);

		/* cable is attached or detached
		 * if current_cable_type is minus value,
		 * check cable by sec_bat_get_cable_type()
		 * although SEC_BATTERY_CABLE_SOURCE_EXTERNAL is set
		 * (0 is SEC_BATTERY_CABLE_UNKNOWN)
		 */
		if ((current_cable_type >= 0) &&
			(current_cable_type < SEC_BATTERY_CABLE_MAX) &&
			(battery->pdata->cable_source_type &
			SEC_BATTERY_CABLE_SOURCE_EXTERNAL)) {

			__pm_stay_awake(battery->cable_ws);
				queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work,0);
		} else {
			if (sec_bat_get_cable_type(battery,
						battery->pdata->cable_source_type)) {
				__pm_stay_awake(battery->cable_ws);
					queue_delayed_work(battery->monitor_wqueue,
						&battery->cable_work,0);
			}
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		battery->capacity = val->intval;
		power_supply_changed(battery->psy_bat);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		__pm_stay_awake(battery->monitor_ws);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		battery->present = val->intval;

		__pm_stay_awake(battery->monitor_ws);
		queue_delayed_work(battery->monitor_wqueue,
				   &battery->monitor_work, 0);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		break;
#if defined(CONFIG_BATTERY_CISD)
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		pr_info("%s: Valert was occurred! run monitor work for updating cisd data!\n", __func__);
		battery->cisd.data[CISD_DATA_VALERT_COUNT]++;
		battery->cisd.data[CISD_DATA_VALERT_COUNT_PER_DAY]++;
		__pm_stay_awake(battery->monitor_ws);
		queue_delayed_work_on(0, battery->monitor_wqueue,
			&battery->monitor_work, 0);
		break;
#endif
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_AICL_CURRENT:
			battery->max_charge_power = battery->charge_power =
				mW_by_mVmA(battery->input_voltage, val->intval);
			sec_vote(battery->input_vote, VOTER_AICL, true, val->intval);
			pr_info("%s: aicl : %dmA, %dmW)\n", __func__,
				val->intval, battery->charge_power);
			if (is_wired_type(battery->cable_type))
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_AICL,
					SEC_BAT_CURRENT_EVENT_AICL);

#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_AICL_COUNT]++;
				battery->cisd.data[CISD_DATA_AICL_COUNT_PER_DAY]++;
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_SYSOVLO:
			if (battery->status != POWER_SUPPLY_STATUS_DISCHARGING) {
				pr_info("%s: Vsys is ovlo !!\n", __func__);
				battery->is_sysovlo = true;
				battery->is_recharging = false;
				battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
				sec_bat_set_health(battery, POWER_SUPPLY_EXT_HEALTH_VSYS_OVP);
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_VSYS_OVP, SEC_BAT_CURRENT_EVENT_VSYS_OVP);
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_VSYS_OVP]++;
				battery->cisd.data[CISD_DATA_VSYS_OVP_PER_DAY]++;
#endif
#if defined(CONFIG_SEC_ABC)
				sec_abc_send_event("MODULE=battery@ERROR=vsys_ovp");
#endif
				sec_vote(battery->chgen_vote, VOTER_SYSOVLO, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
				__pm_stay_awake(battery->monitor_ws);
				queue_delayed_work(battery->monitor_wqueue,
						   &battery->monitor_work, 0);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_VBAT_OVP:
			if (battery->status != POWER_SUPPLY_STATUS_DISCHARGING) {
				pr_info("%s: Vbat is ovlo !!\n", __func__);
				battery->is_vbatovlo = true;
				battery->is_recharging = false;
				battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
				sec_bat_set_health(battery, POWER_SUPPLY_EXT_HEALTH_VBAT_OVP);
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_VBAT_OVP, SEC_BAT_CURRENT_EVENT_VBAT_OVP);
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);

				sec_vote(battery->chgen_vote, VOTER_VBAT_OVP, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
				__pm_stay_awake(battery->monitor_ws);
				queue_delayed_work(battery->monitor_wqueue,
						   &battery->monitor_work, 0);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_USB_CONFIGURE:
			if (val->intval == USB_CURRENT_CLEAR || val->intval != battery->prev_usb_conf)
				sec_bat_set_usb_configure(battery, val->intval);

			break;
		case POWER_SUPPLY_EXT_PROP_OVERHEAT_NOTIFY:
			pr_info("%s: POWER_SUPPLY_EXT_PROP_OVERHEAT_NOTIFY!\n", __func__);
			__pm_stay_awake(battery->monitor_ws);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->monitor_work, 0);
			break;
		case POWER_SUPPLY_EXT_PROP_HV_DISABLE:
#if !defined(CONFIG_PD_CHARGER_HV_DISABLE)
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
			pr_info("HV wired charging mode is %s\n", (val->intval == CH_MODE_AFC_DISABLE_VAL ? "Disabled" : "Enabled"));
			if (val->intval == CH_MODE_AFC_DISABLE_VAL) {
				sec_bat_set_current_event(battery,
					SEC_BAT_CURRENT_EVENT_HV_DISABLE, SEC_BAT_CURRENT_EVENT_HV_DISABLE);

				if (is_pd_wire_type(battery->cable_type)) {
					battery->update_pd_list = true;
					pr_info("%s: update pd list\n", __func__);
					sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SELECT_PDO,
						SEC_BAT_CURRENT_EVENT_SELECT_PDO);
					sec_vote(battery->input_vote, VOTER_SELECT_PDO, true,
						battery->pdata->select_pd_input_current);
					sec_pd_select_pdo(1);
				}
			} else {
				sec_bat_set_current_event(battery,
					0, SEC_BAT_CURRENT_EVENT_HV_DISABLE);

				if (is_pd_wire_type(battery->cable_type)) {
					unsigned int target_pd_index = battery->pd_list.num_fpdo - 1;

					battery->update_pd_list = true;
					pr_info("%s: update pd list\n", __func__);
					sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SELECT_PDO,
						SEC_BAT_CURRENT_EVENT_SELECT_PDO);
					sec_vote(battery->input_vote, VOTER_SELECT_PDO, true,
						battery->pdata->select_pd_input_current);
					if (target_pd_index >= 0 && target_pd_index < MAX_PDO_NUM)
						sec_pd_select_pdo(battery->pd_list.pd_info[target_pd_index].pdo_index);
				}
			}
#endif
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_WC_CONTROL:
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
			pr_info("%s: Recover MFC IC (wc_enable: %d)\n",
				__func__, battery->wc_enable);

			mutex_lock(&battery->wclock);
			if (battery->wc_enable) {
				sec_bat_set_mfc_off(battery, false);
				msleep(500);

				sec_bat_set_mfc_on(battery);
			}
			mutex_unlock(&battery->wclock);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_WDT_STATUS:
			if (val->intval)
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_WDT_EXPIRED,
					SEC_BAT_CURRENT_EVENT_WDT_EXPIRED);
			break;
		case POWER_SUPPLY_EXT_PROP_CURRENT_EVENT:
			if (!(battery->current_event & val->intval)) {
				pr_info("%s: set new current_event %d\n", __func__, val->intval);
#if defined(CONFIG_BATTERY_CISD)
				if (val->intval == SEC_BAT_CURRENT_EVENT_DC_ERR)
					battery->cisd.event_data[EVENT_DC_ERR]++;
#endif
				sec_bat_set_current_event(battery, val->intval, val->intval);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CURRENT_EVENT_CLEAR:
			pr_info("%s: new current_event clear %d\n", __func__, val->intval);
			sec_bat_set_current_event(battery, 0, val->intval);
			break;
#if defined(CONFIG_WIRELESS_TX_MODE)
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_AVG_CURR:
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE:
			sec_wireless_set_tx_enable(battery, val->intval);
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_SRCCAP:
			pr_info("%s: set init_src_cap(%d->%d)",
				__func__, battery->init_src_cap, val->intval);
			battery->init_src_cap = true;
			break;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		case POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT:
			if (battery->ta_alert_wa) {
				pr_info("@TA_ALERT: %s: TA OCP DETECT\n", __func__);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.event_data[EVENT_TA_OCP_DET]++;
				if (battery->ta_alert_mode == OCP_NONE)
					battery->cisd.event_data[EVENT_TA_OCP_ON]++;
#endif
				battery->ta_alert_mode = OCP_DETECT;
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_25W_OCP,
							SEC_BAT_CURRENT_EVENT_25W_OCP);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_SEND_UVDM:
			if (is_pd_apdo_wire_type(battery->cable_type)) {
				char direct_charging_source_status[2] = {0, };

				pr_info("@SEND_UVDM: Request Change Charging Source : %s\n",
					val->intval == 0 ? "Switch Charger" : "Direct Charger" );

				direct_charging_source_status[0] = SEC_SEND_UVDM;
				direct_charging_source_status[1] = val->intval;

				sec_bat_set_current_event(battery, val->intval == 0 ?
					SEC_BAT_CURRENT_EVENT_SEND_UVDM : 0, SEC_BAT_CURRENT_EVENT_SEND_UVDM);
				value.strval = direct_charging_source_status;
				psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE, value);
			}
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_WPC_EN:
			sec_bat_set_current_event(battery,
				val->intval ? 0 : SEC_BAT_CURRENT_EVENT_WPC_EN, SEC_BAT_CURRENT_EVENT_WPC_EN);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL:
			value.intval = val->intval;
			pr_info("%s: WCIN-UNO %s\n", __func__, value.intval > 0 ? "on" : "off");
			psy_do_property("otg", set,
					POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL, value);
			break;
		case POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION:
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_COUNTER_SHADOW:
			break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
		case POWER_SUPPLY_EXT_PROP_POWER_DESIGN:
			sec_bat_parse_dt(battery->dev, battery);
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_MFC_FW_UPDATE:
			battery->mfc_fw_update = val->intval;
			break;
		case POWER_SUPPLY_EXT_PROP_THERMAL_ZONE:
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_bat_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	union power_supply_propval value = {0, };
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if ((battery->health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) ||
			(battery->health == POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE)) {
				val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			if ((battery->pdata->cable_check_type &
				SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE) &&
				!lpcharge) {
				switch (battery->cable_type) {
				case SEC_BATTERY_CABLE_USB:
				case SEC_BATTERY_CABLE_USB_CDP:
					val->intval =
						POWER_SUPPLY_STATUS_DISCHARGING;
					return 0;
				}
			}
#if defined(CONFIG_STORE_MODE)
			if (battery->store_mode && !lpcharge &&
			    !is_nocharge_type(battery->cable_type) &&
			    battery->status == POWER_SUPPLY_STATUS_DISCHARGING) {
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			} else
#endif
				val->intval = battery->status;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (is_nocharge_type(battery->cable_type)) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		} else if (is_hv_wire_type(battery->cable_type) || is_pd_wire_type(battery->cable_type)) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		} else {
			psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_CHARGE_TYPE, value);
			if (value.intval == SEC_BATTERY_CABLE_UNKNOWN)
				/* if error in CHARGE_TYPE of charger
				 * set CHARGE_TYPE as NONE
				 */
				val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			else
				val->intval = value.intval;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (lpcharge &&
			(battery->health == POWER_SUPPLY_HEALTH_OVERVOLTAGE ||
			battery->health == POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE ||
			battery->health == POWER_SUPPLY_EXT_HEALTH_DC_ERR)) {
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		} else if (battery->health >= POWER_SUPPLY_EXT_HEALTH_MIN) {
			if (battery->health == POWER_SUPPLY_EXT_HEALTH_OVERHEATLIMIT)
				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			else
				val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		} else {
			val->intval = battery->health;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = battery->present;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* SEC_BATTERY_CABLE_SILENT_TYPE, defines with PMS team for avoid charger connection sound */
		if (battery->muic_cable_type == ATTACHED_DEV_RETRY_TIMEOUT_OPEN_MUIC ||
				battery->muic_cable_type == ATTACHED_DEV_RETRY_AFC_CHARGER_5V_MUIC ||
				battery->muic_cable_type == ATTACHED_DEV_RETRY_AFC_CHARGER_9V_MUIC)
			val->intval = SEC_BATTERY_CABLE_SILENT_TYPE;
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
		else if (is_hv_wireless_type(battery->cable_type) ||
			(battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV) ||
			(battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20)) {
			if (sec_bat_hv_wc_normal_mode_check(battery))
				val->intval = SEC_BATTERY_CABLE_WIRELESS;
			else
				val->intval = SEC_BATTERY_CABLE_HV_WIRELESS_ETX;
		}
		else if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK ||
			battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND ||
			battery->cable_type == SEC_BATTERY_CABLE_PMA_WIRELESS ||
			battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_VEHICLE ||
			battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_TX ||
			battery->cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20 ||
			battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE)
			val->intval = SEC_BATTERY_CABLE_WIRELESS;
#endif
		else
			val->intval = battery->cable_type;
		pr_info("%s cable type = %d sleep_mode = %d\n", __func__, val->intval, battery->sleep_mode);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = battery->pdata->technology;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
#ifdef CONFIG_SEC_FACTORY
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
		battery->voltage_now = value.intval;
		dev_err(battery->dev,
			"%s: voltage now(%d)\n", __func__, battery->voltage_now);
#endif
		/* voltage value should be in uV */
		val->intval = battery->voltage_now * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
#ifdef CONFIG_SEC_FACTORY
		value.intval = SEC_BATTERY_VOLTAGE_AVERAGE;
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
		battery->voltage_avg = value.intval;
		dev_err(battery->dev,
			"%s: voltage avg(%d)\n", __func__, battery->voltage_avg);
#endif
		/* voltage value should be in uV */
		val->intval = battery->voltage_avg * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		value.intval = SEC_BATTERY_CURRENT_MA;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CURRENT_NOW, value);
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		value.intval = SEC_BATTERY_CURRENT_MA;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CURRENT_AVG, value);
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = battery->pdata->battery_full_capacity * 1000;
		break;
	/* charging mode (differ from power supply) */
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = battery->charging_mode;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (battery->pdata->fake_capacity) {
			val->intval = 90;
			pr_info("%s : capacity(%d)\n", __func__, val->intval);
		} else {
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
			if (battery->status == POWER_SUPPLY_STATUS_FULL) {
				if (battery->eng_not_full_status)
					val->intval = battery->capacity;
				else
					val->intval = 100;
			} else {
				val->intval = battery->capacity;
			}
#else
			if (battery->status == POWER_SUPPLY_STATUS_FULL)
				val->intval = 100;
			else
				val->intval = battery->capacity;
#endif
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery->temperature;
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = battery->temper_amb;
		break;
#if IS_ENABLED(CONFIG_FUELGAUGE_MAX77705)
	case POWER_SUPPLY_PROP_POWER_NOW:
		value.intval = SEC_BATTERY_ISYS_MA;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_EXT_PROP_MEASURE_SYS, value);
		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_POWER_AVG:
		value.intval = SEC_BATTERY_ISYS_AVG_MA;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_EXT_PROP_MEASURE_SYS, value);
		val->intval = value.intval;
		break;
#endif
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = ttf_display(battery);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_SWELLING_MODE)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = battery->charge_counter;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_SUB_PBA_TEMP_REC:
			val->intval = !battery->vbus_limit;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_POWER:
			val->intval = battery->charge_power;
			break;
		case POWER_SUPPLY_EXT_PROP_CURRENT_EVENT:
			val->intval = battery->current_event;
			break;
#if defined(CONFIG_WIRELESS_TX_MODE)
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_AVG_CURR:
			val->intval = battery->tx_avg_curr;
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE:
			val->intval = battery->wc_tx_enable;
			break;
#endif
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		case POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO:
			val->intval = battery->pd_list.num_apdo > 0 ? battery->pd_list.num_fpdo : 1;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE:
			val->intval = battery->pd_list.now_isApdo;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_HV_PDO:
			val->intval = battery->hv_pdo;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_HAS_APDO:
			val->intval = battery->sink_status.has_apdo;
			break;
		case POWER_SUPPLY_EXT_PROP_PAD_VOLT_CTRL:
			if (battery->wpc_vout_ctrl_lcd_on)
				val->intval = battery->lcd_status;
			else
				val->intval = false;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_TA_ALERT:
			if (battery->ta_alert_wa) {
				val->intval = battery->ta_alert_mode;
			} else
				val->intval = OCP_NONE;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_SEND_UVDM:
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL:
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_COUNTER_SHADOW:
			val->intval = battery->wire_status;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			break;
		case POWER_SUPPLY_EXT_PROP_HEALTH:
			val->intval = battery->health;
			break;
		case POWER_SUPPLY_EXT_PROP_MFC_FW_UPDATE:
			val->intval = battery->mfc_fw_update;
			break;
		case POWER_SUPPLY_EXT_PROP_THERMAL_ZONE:
			val->intval = battery->thermal_zone;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sec_usb_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	if ((battery->health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) ||
		(battery->health == POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE)) {
		val->intval = 0;
		return 0;
	}
	/* Set enable=1 only if the USB charger is connected */
	switch (battery->wire_status) {
	case SEC_BATTERY_CABLE_USB:
	case SEC_BATTERY_CABLE_USB_CDP:
		val->intval = 1;
		break;
	case SEC_BATTERY_CABLE_PDIC:
	case SEC_BATTERY_CABLE_NONE:
	        val->intval = (battery->pd_usb_attached) ? 1:0;
	        break;
	default:
		val->intval = 0;
		break;
	}

	if (is_slate_mode(battery))
		val->intval = 0;
	return 0;
}

static int sec_ac_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if ((battery->health == POWER_SUPPLY_HEALTH_OVERVOLTAGE) ||
				(battery->health == POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE)) {
			val->intval = 0;
			return 0;
		}

		/* Set enable=1 only if the AC charger is connected */
		switch (battery->cable_type) {
		case SEC_BATTERY_CABLE_TA:
		case SEC_BATTERY_CABLE_UARTOFF:
		case SEC_BATTERY_CABLE_LAN_HUB:
		case SEC_BATTERY_CABLE_UNKNOWN:
		case SEC_BATTERY_CABLE_PREPARE_TA:
		case SEC_BATTERY_CABLE_9V_ERR:
		case SEC_BATTERY_CABLE_9V_UNKNOWN:
		case SEC_BATTERY_CABLE_9V_TA:
		case SEC_BATTERY_CABLE_12V_TA:
		case SEC_BATTERY_CABLE_HMT_CONNECTED:
		case SEC_BATTERY_CABLE_HMT_CHARGE:
		case SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT:
		case SEC_BATTERY_CABLE_QC20:
		case SEC_BATTERY_CABLE_QC30:
		case SEC_BATTERY_CABLE_TIMEOUT:
		case SEC_BATTERY_CABLE_SMART_OTG:
		case SEC_BATTERY_CABLE_SMART_NOTG:
		case SEC_BATTERY_CABLE_POGO:
		case SEC_BATTERY_CABLE_PDIC_APDO:
			val->intval = 1;
			break;
		case SEC_BATTERY_CABLE_PDIC:
			val->intval = (battery->pd_usb_attached) ? 0:1;
			break;
		default:
			val->intval = 0;
			break;
		}
		if (lpcharge && (battery->misc_event & BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE))
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery->chg_temp;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
			case POWER_SUPPLY_EXT_PROP_WATER_DETECT:
				if (battery->misc_event & (BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE |
					BATT_MISC_EVENT_WATER_HICCUP_TYPE)) {
					val->intval = 1;
					pr_info("%s: Water Detect\n", __func__);
				} else {
					val->intval = 0;
				}
				break;
			default:
				return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_wireless_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = is_wireless_fake_type(battery->cable_type) ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = (battery->pdata->wireless_charger_name) ?
			1 : 0;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
#if defined(CONFIG_TX_GEAR_AOV)
		case POWER_SUPPLY_EXT_PROP_TX_SWITCH_MODE:
			val->intval = battery->tx_switch_mode;
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_POWER_DESIGN:
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
			if (battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20)
				val->intval = battery->wc20_power_class;
			else
#endif
				val->intval = 0;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_wireless_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	int work_delay;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#if defined(CONFIG_BATTERY_CISD)
		if (val->intval != SEC_WIRELESS_PAD_NONE && battery->wc_status == SEC_WIRELESS_PAD_NONE) {
			battery->cisd.data[CISD_DATA_WIRELESS_COUNT]++;
			battery->cisd.data[CISD_DATA_WIRELESS_COUNT_PER_DAY]++;
		}
#endif
		pr_info("%s : wireless_type(0x%x)\n", __func__, val->intval);

		/* Clear the FOD , AUTH State */
		sec_bat_set_misc_event(battery, 0, BATT_MISC_EVENT_WIRELESS_FOD);

		battery->wc_status = val->intval;

		if ((battery->ext_event & BATT_EXT_EVENT_CALL) &&
			(battery->wc_status == SEC_WIRELESS_PAD_WPC_PACK ||
			battery->wc_status == SEC_WIRELESS_PAD_WPC_PACK_HV ||
			battery->wc_status == SEC_WIRELESS_PAD_TX)) {
				battery->wc_rx_phm_mode = true;
		}

		if (battery->wc_status == SEC_WIRELESS_PAD_NONE) {
			battery->wpc_vout_level = WIRELESS_VOUT_10V;
			battery->wpc_max_vout_level = WIRELESS_VOUT_12_5V;
			battery->auto_mode = false;

			if (delayed_work_pending(&battery->wc_headroom_work)) {
				__pm_relax(battery->wc_headroom_ws);
				cancel_delayed_work(&battery->wc_headroom_work);
			}

			sec_bat_set_misc_event(battery, 0,
			(BATT_MISC_EVENT_WIRELESS_DET_LEVEL | /* clear wpc_det level status */
			BATT_MISC_EVENT_WIRELESS_AUTH_START |
			BATT_MISC_EVENT_WIRELESS_AUTH_RECVED |
			BATT_MISC_EVENT_WIRELESS_AUTH_FAIL |
			BATT_MISC_EVENT_WIRELESS_AUTH_PASS));
		} else if (battery->wc_status != SEC_WIRELESS_PAD_FAKE) {
			sec_bat_set_misc_event(battery, BATT_MISC_EVENT_WIRELESS_DET_LEVEL, /* set wpc_det level status */
			BATT_MISC_EVENT_WIRELESS_DET_LEVEL);

			if (battery->wc_status == SEC_WIRELESS_PAD_WPC_HV_20) {
				sec_bat_set_misc_event(battery, BATT_MISC_EVENT_WIRELESS_AUTH_PASS,
					BATT_MISC_EVENT_WIRELESS_AUTH_PASS);
#if defined(CONFIG_BATTERY_CISD)
				if (battery->wc_status == SEC_WIRELESS_PAD_WPC_HV_20)
					battery->cisd.cable_data[CISD_CABLE_HV_WC_20]++;
#endif
			}
		}

		__pm_stay_awake(battery->cable_ws);
		queue_delayed_work(battery->monitor_wqueue,
			&battery->cable_work, 0);

		if (battery->wc_status == SEC_WIRELESS_PAD_NONE ||
			battery->wc_status == SEC_WIRELESS_PAD_WPC_PACK ||
			battery->wc_status == SEC_WIRELESS_PAD_WPC_PACK_HV ||
			battery->wc_status == SEC_WIRELESS_PAD_VEHICLE) {
			sec_bat_set_misc_event(battery,
				(battery->wc_status == SEC_WIRELESS_PAD_NONE ?
				0 : BATT_MISC_EVENT_WIRELESS_BACKPACK_TYPE),
				BATT_MISC_EVENT_WIRELESS_BACKPACK_TYPE);
		}
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
#if defined(CONFIG_BATTERY_CISD)
		pr_info("%s : tx_type(0x%x)\n", __func__, val->intval);
		count_cisd_pad_data(&battery->cisd, val->intval);
#endif
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		sec_vote(battery->input_vote, VOTER_AICL, false, 0);
		pr_info("%s: reset aicl\n", __func__);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
		case POWER_SUPPLY_EXT_PROP_WIRELESS_ERR:
			if (is_wireless_type(battery->cable_type))
				sec_bat_set_misc_event(battery, val->intval ? BATT_MISC_EVENT_WIRELESS_FOD : 0,
					BATT_MISC_EVENT_WIRELESS_FOD);
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR:
			if (val->intval & BATT_TX_EVENT_WIRELESS_TX_MISALIGN) {
				sec_bat_handle_tx_misalign(battery, true);
			} else if (val->intval & BATT_TX_EVENT_WIRELESS_TX_OCP) {
				sec_bat_handle_tx_ocp(battery, true);
			} else if (val->intval & BATT_TX_EVENT_WIRELESS_TX_AC_MISSING) {
				if (battery->wc_tx_enable)
					sec_wireless_set_tx_enable(battery, false);
				battery->tx_retry_case |= BATT_TX_EVENT_WIRELESS_TX_AC_MISSING;
				/* clear tx all event */
				sec_bat_set_tx_event(battery, 0, BATT_TX_EVENT_WIRELESS_ALL_MASK);
				sec_bat_set_tx_event(battery,
						BATT_TX_EVENT_WIRELESS_TX_RETRY, BATT_TX_EVENT_WIRELESS_TX_RETRY);
			} else {
				sec_bat_set_tx_event(battery, val->intval, val->intval);
				sec_wireless_set_tx_enable(battery, false);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONNECTED:
			sec_bat_set_tx_event(battery, val->intval ? BATT_TX_EVENT_WIRELESS_RX_CONNECT : 0,
				BATT_TX_EVENT_WIRELESS_RX_CONNECT);
			battery->wc_rx_connected = val->intval;
			if (!val->intval) {
				battery->wc_rx_type = NO_DEV;

				battery->tx_switch_mode = TX_SWITCH_MODE_OFF;
				battery->tx_switch_mode_change = false;
				battery->tx_switch_start_soc = 0;
#if defined(CONFIG_TX_GEAR_AOV)
				battery->wc_tx_freq = 0;
				battery->wc_tx_adaptive_vout = false;
				battery->wc_found_gear_freq = false;
#endif

				if (battery->afc_disable) {
					battery->afc_disable = false;
					muic_hv_charger_disable(battery->afc_disable);
				}
				if (battery->wc_tx_enable) {
					pr_info("@Tx_Mode %s: Device detached.\n", __func__);

					if (is_hv_wire_type(battery->wire_status)) {
						pr_info("@Tx_Mode %s : charging voltage change(9V -> 5V).\n", __func__);
						muic_afc_set_voltage(SEC_INPUT_VOLTAGE_5V/1000);
						break; /* do not set buck off/uno off until vbus level get real 5V */
					} else if (is_pd_wire_type(battery->wire_status) && battery->hv_pdo) {
						pr_info("@Tx_Mode %s: PD charnge pdo (9V -> 5V). Because Tx Start.\n", __func__);
						sec_bat_change_pdo(battery, SEC_INPUT_VOLTAGE_5V);
						break; /* do not set buck off/uno off until vbus level get real 5V */
					}

					if (!battery->buck_cntl_by_tx) {
						battery->buck_cntl_by_tx = true;
						sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
					}

					sec_bat_wireless_iout_cntl(battery, battery->pdata->tx_uno_iout, battery->pdata->tx_mfc_iout_gear);
					sec_bat_wireless_vout_cntl(battery, WC_TX_VOUT_5_0V);
					sec_bat_wireless_minduty_cntl(battery, battery->pdata->tx_minduty_default);
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_RX_TYPE:
			work_delay = 0;
			battery->wc_rx_type = val->intval;
#if defined(CONFIG_BATTERY_CISD)
			if (battery->wc_rx_type) {
				if (battery->wc_rx_type == SS_BUDS) {
					battery->cisd.tx_data[SS_PHONE]--;
				}
				battery->cisd.tx_data[battery->wc_rx_type]++;
			}
#endif
#if defined(CONFIG_TX_GEAR_AOV)
			if (battery->wc_rx_type == SS_GEAR)
				work_delay = battery->pdata->tx_aov_delay_phm_escape;
#endif
			pr_info("%s : RX_TYPE=%d, delay(%d)\n", __func__, battery->wc_rx_type, work_delay);

			sec_bat_run_wpc_tx_work(battery, work_delay);
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_STATUS:
			if (val->intval == WIRELESS_AUTH_START)
				sec_bat_set_misc_event(battery, BATT_MISC_EVENT_WIRELESS_AUTH_START, BATT_MISC_EVENT_WIRELESS_AUTH_START);
			else if (val->intval == WIRELESS_AUTH_RECEIVED)
				sec_bat_set_misc_event(battery, BATT_MISC_EVENT_WIRELESS_AUTH_RECVED, BATT_MISC_EVENT_WIRELESS_AUTH_RECVED);
			else if (val->intval == WIRELESS_AUTH_SENT) /* need to be clear this value when data is sent */
				sec_bat_set_misc_event(battery, 0, BATT_MISC_EVENT_WIRELESS_AUTH_START | BATT_MISC_EVENT_WIRELESS_AUTH_RECVED);
			else if (val->intval == WIRELESS_AUTH_FAIL)
				sec_bat_set_misc_event(battery, BATT_MISC_EVENT_WIRELESS_AUTH_FAIL, BATT_MISC_EVENT_WIRELESS_AUTH_FAIL);
			break;
		case POWER_SUPPLY_EXT_PROP_CALL_EVENT:
			if (val->intval == 1) {
				pr_info("%s : PHM enabled\n",__func__);
				battery->wc_rx_phm_mode = true;
			}
			break;
#if defined(CONFIG_TX_GEAR_PHM_VOUT_CTRL) || defined(CONFIG_TX_GEAR_AOV)
		case POWER_SUPPLY_EXT_PROP_GEAR_PHM_EVENT:
			work_delay = 0;
#if defined(CONFIG_TX_GEAR_AOV)
			if (!val->intval)
				work_delay = battery->pdata->tx_aov_delay_phm_escape;
#endif
			pr_info("@Tx_Mode %s : gear phm event(%d), work_delay(%d)\n",
				__func__, val->intval, work_delay);
#if !defined(CONFIG_TX_GEAR_AOV)
			if (battery->pdata->tx_gear_vout != WC_TX_VOUT_5_0V)
#endif
				sec_bat_run_wpc_tx_work(battery, work_delay);
			break;
#endif
#if defined(CONFIG_TX_GEAR_AOV)
		case POWER_SUPPLY_EXT_PROP_TX_SWITCH_MODE:
			pr_info("@Tx_Mode %s : set tx switch mode(%d)\n",
				__func__, val->intval);
			sec_bat_set_forced_tx_switch_mode(battery, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_TX_AOV_BUCK_CTRL:
			pr_info("@Tx_Mode %s : buck ctrl for aov. buck(%s). buck_cntl_by_tx(%d)\n",
					__func__, val->intval ? "on" : "off", battery->buck_cntl_by_tx);
			if (val->intval)
				sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			else
				sec_vote(battery->chgen_vote, VOTER_WC_TX, false, 0);
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_WIRELESS_RX_POWER:
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
			pr_info("%s : rx power %d\n", __func__, val->intval);
			battery->wc20_rx_power = val->intval;
			queue_delayed_work(battery->monitor_wqueue, &battery->wc20_current_work,
				msecs_to_jiffies(0));
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_MAX_VOUT:
			pr_info("%s : max vout %s\n", __func__, vout_control_mode_str[val->intval]);
			battery->wpc_max_vout_level = val->intval;
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			sec_wireless_otg_control(battery, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_ABNORMAL_PAD:
			sec_bat_set_misc_event(battery, val->intval ? BATT_MISC_EVENT_ABNORMAL_PAD : 0,
					BATT_MISC_EVENT_ABNORMAL_PAD);
			break;
#endif
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_pogo_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

#if defined(CONFIG_USE_POGO)
	val->intval = battery->pogo_status;
	pr_info("%s: POGO online : %d\n", __func__, val->intval);
#else
	val->intval = 0;
	battery->pogo_status = 0;
	pr_info("%s: POGO not supported\n", __func__);
#endif

	return 0;
}

static int sec_pogo_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
#if defined(CONFIG_USE_POGO)
		battery->pogo_status = val->intval;

		__pm_stay_awake(battery->cable_ws);
		queue_delayed_work(battery->monitor_wqueue,
			&battery->cable_work, 0);
		pr_info("%s: pogo_status : %d\n", __func__, battery->pogo_status);
#else
		battery->pogo_status = 0;
		pr_info("%s: POGO not supported\n", __func__);
#endif
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) || IS_ENABLED(CONFIG_MUIC_NOTIFIER)
static int sec_bat_cable_check(struct sec_battery_info *battery,
				muic_attached_dev_t attached_dev)
{
	int current_cable_type = -1;
	union power_supply_propval val = {0, };

	pr_info("[%s]ATTACHED(%d)\n", __func__, attached_dev);

	switch (attached_dev)
	{
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		battery->is_jig_on = true;
#if defined(CONFIG_BATTERY_CISD)
		battery->skip_cisd = true;
#endif
		current_cable_type = SEC_BATTERY_CABLE_NONE;
		break;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_NONE;
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNDEFINED_RANGE_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_NONE;
		break;
	case ATTACHED_DEV_HICCUP_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_NONE;
		break;
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_HMT_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_OTG;
		break;
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
	case ATTACHED_DEV_RETRY_TIMEOUT_OPEN_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_TIMEOUT;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_USB;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
#if defined(CONFIG_LSI_IFPMIC)
		current_cable_type = SEC_BATTERY_CABLE_NONE;
		break;
#endif
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_UARTOFF;
		if (factory_mode)
			current_cable_type = SEC_BATTERY_CABLE_NONE;
		break;
	case ATTACHED_DEV_RDU_TA_MUIC:
		battery->store_mode = true;
		__pm_stay_awake(battery->parse_mode_dt_ws);
		queue_delayed_work(battery->monitor_wqueue, &battery->parse_mode_dt_work, 0);
		current_cable_type = SEC_BATTERY_CABLE_TA;
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC:
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_TA;
		break;
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_RETRY_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT;
		break;
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_USB_CDP;
		break;
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_LAN_HUB;
		break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_POWER_SHARING;
		break;
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_PREPARE_TA;
		break;
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_9V_TA;
#if defined(CONFIG_BATTERY_CISD)
		if ((battery->cable_type == SEC_BATTERY_CABLE_TA) ||
				(battery->cable_type == SEC_BATTERY_CABLE_NONE))
			battery->cisd.cable_data[CISD_CABLE_QC]++;
#endif
		break;
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_RETRY_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_9V_TA;
#if defined(CONFIG_BATTERY_CISD)
		if ((battery->cable_type == SEC_BATTERY_CABLE_TA) ||
				(battery->cable_type == SEC_BATTERY_CABLE_NONE))
			battery->cisd.cable_data[CISD_CABLE_AFC]++;
#endif
		break;
#if defined(CONFIG_MUIC_HV_12V)
	case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_12V_TA;
		break;
#endif
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.cable_data[CISD_CABLE_AFC_FAIL]++;
#endif
		break;
	case ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC:
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.cable_data[CISD_CABLE_QC_FAIL]++;
#endif
		break;
	case ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_9V_UNKNOWN;
		break;
	case ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC:
		current_cable_type = SEC_BATTERY_CABLE_UNKNOWN;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, attached_dev);
		break;
	}

#if defined(CONFIG_DUAL_BATTERY) && defined(CONFIG_SEC_FACTORY)
	val.intval = SEC_DUAL_BATTERY_MAIN;
	psy_do_property(battery->pdata->dual_battery_name, get,
	POWER_SUPPLY_EXT_PROP_DUAL_BAT_DET, val);

	if (!val.intval &&
		(attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC ||
		attached_dev == ATTACHED_DEV_JIG_USB_ON_MUIC)) {
		pr_info("%s No main Battery, 301k and 523k with FACTORY\n", __func__);
		gpio_direction_output(battery->pdata->sub_bat_enb_gpio, 1);
	}
#endif

	if (battery->is_jig_on && !battery->pdata->support_fgsrc_change)
		psy_do_property(battery->pdata->fuelgauge_name, set,
			POWER_SUPPLY_PROP_ENERGY_NOW, val);

#if defined(CONFIG_LSI_IFPMIC)
	switch (attached_dev) {
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
#if defined(CONFIG_SIDO_OVP)
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
#endif
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		val.intval = 1;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_ENERGY_NOW, val);
		pr_err("%s : FACTORY MODE TEST! (%d)\n", __func__, val.intval);
		break;
#if defined(CONFIG_SIDO_OVP)
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
#endif
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		val.intval = 0;
		psy_do_property(battery->pdata->charger_name, set,
			POWER_SUPPLY_PROP_ENERGY_NOW, val);
		pr_err("%s : FACTORY MODE TEST! (%d)\n", __func__, val.intval);
		break;
	default:
		break;
	}
#endif

	return current_cable_type;
}
#endif

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int sec_bat_get_pd_list_index(int current_pdo_num, struct sec_bat_pdic_list *pd_list)
{
	int i = 0;

	for (i = 0; i < pd_list->max_pd_count; i++) {
		if (pd_list->pd_info[i].pdo_index == current_pdo_num)
			return i;
	}

	return 0;
}

static void sec_bat_set_rp_current(struct sec_battery_info *battery, int cable_type)
{
	if (battery->sink_status.rp_currentlvl == RP_CURRENT_ABNORMAL) {
		sec_bat_change_default_current(battery, cable_type,
			battery->pdata->rp_current_abnormal_rp3, battery->pdata->rp_current_abnormal_rp3);
	} else if (battery->sink_status.rp_currentlvl == RP_CURRENT_LEVEL3) {
		if (battery->current_event & SEC_BAT_CURRENT_EVENT_HV_DISABLE)
			sec_bat_change_default_current(battery, cable_type,
				battery->pdata->default_input_current, battery->pdata->default_charging_current);
		else {
			if (battery->store_mode)
				sec_bat_change_default_current(battery, cable_type,
					battery->pdata->rp_current_rdu_rp3, battery->pdata->max_charging_current);
			else
				sec_bat_change_default_current(battery, cable_type,
					battery->pdata->rp_current_rp3, battery->pdata->max_charging_current);
		}
	} else if (battery->sink_status.rp_currentlvl == RP_CURRENT_LEVEL2) {
		sec_bat_change_default_current(battery, cable_type,
			battery->pdata->rp_current_rp2, battery->pdata->rp_current_rp2);
	} else if (battery->sink_status.rp_currentlvl == RP_CURRENT_LEVEL_DEFAULT) {
		if (cable_type == SEC_BATTERY_CABLE_USB) {
			if (battery->current_event & SEC_BAT_CURRENT_EVENT_USB_SUPER)
				sec_bat_change_default_current(battery, SEC_BATTERY_CABLE_USB,
					USB_CURRENT_SUPER_SPEED, USB_CURRENT_SUPER_SPEED);
			else
				sec_bat_change_default_current(battery, cable_type,
					battery->pdata->default_usb_input_current,
					battery->pdata->default_usb_charging_current);
		} else if (cable_type == SEC_BATTERY_CABLE_TA) {
			sec_bat_change_default_current(battery, cable_type,
				battery->pdata->default_input_current,
				battery->pdata->default_charging_current);
		}
	}

	pr_info("%s:(%d)\n", __func__, battery->sink_status.rp_currentlvl);
	battery->max_charge_power = 0;
	if (battery->status != POWER_SUPPLY_STATUS_DISCHARGING)
		sec_bat_check_input_voltage(battery);

	sec_vote(battery->input_vote, VOTER_AICL, false, 0);
}

static int make_pd_list(struct sec_battery_info *battery)
{
	int i = 0;
	int base_charge_power = 0, selected_pdo_voltage = 0, selected_pdo_power = 0, selected_pdo_num = 0;
	int pd_list_index = 0, temp_power = 0, num_pd_list = 0, pd_list_select = 0;
	int pd_charging_charge_power = battery->current_event & SEC_BAT_CURRENT_EVENT_HV_DISABLE ?
		battery->pdata->nv_charge_power : battery->pdata->pd_charging_charge_power;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	union power_supply_propval value = {0, };
#endif
	POWER_LIST* pPower_list;

	/* If PD charger is attached first, current_pdo_num should be 1 supports 5V */
	battery->pd_list.pd_info[0].max_voltage =
		battery->sink_status.power_list[1].max_voltage;
	battery->pd_list.pd_info[0].max_current =
		battery->sink_status.power_list[1].max_current;
	battery->pd_list.pd_info[0].comm_capable =
		battery->sink_status.power_list[1].comm_capable;
	battery->pd_list.pd_info[0].suspend =
		battery->sink_status.power_list[1].suspend;
	battery->pd_list.pd_info[0].pdo_index = 1;
	pd_list_index++;

	base_charge_power = mW_by_mVmA(battery->sink_status.power_list[1].max_voltage,
		battery->sink_status.power_list[1].max_current);

	selected_pdo_voltage = SEC_INPUT_VOLTAGE_5V;
	selected_pdo_power = 0;
	selected_pdo_num = 0;

	for (i = 1; i <= battery->sink_status.available_pdo_num; i++)
	{
		pPower_list = &battery->sink_status.power_list[i];
		if (!pPower_list->accept || pPower_list->apdo) /* skip not accept of apdo list */
			continue;

		temp_power = mW_by_mVmA(pPower_list->max_voltage, pPower_list->max_current);

		if ((temp_power >= base_charge_power - 1000) && (temp_power <= pd_charging_charge_power))
		{
			if (temp_power >= selected_pdo_power &&
				pPower_list->max_voltage > selected_pdo_voltage && pPower_list->max_voltage <= battery->pdata->max_input_voltage)
			{
				selected_pdo_voltage = pPower_list->max_voltage;
				selected_pdo_power = temp_power;
				selected_pdo_num = i;
			}
		}
	}
	if (selected_pdo_num)
	{
		POWER_LIST* pSelected_power_list =
			&battery->sink_status.power_list[selected_pdo_num];

		battery->pd_list.pd_info[pd_list_index].pdo_index = selected_pdo_num;
		battery->pd_list.pd_info[pd_list_index].apdo = false;
		battery->pd_list.pd_info[pd_list_index].max_voltage = pSelected_power_list->max_voltage;
		battery->pd_list.pd_info[pd_list_index].max_current = pSelected_power_list->max_current;
		battery->pd_list.pd_info[pd_list_index].min_voltage = 0;
		battery->pd_list.pd_info[pd_list_index].comm_capable = pSelected_power_list->comm_capable;
		battery->pd_list.pd_info[pd_list_index].suspend = pSelected_power_list->suspend;
		pd_list_index++;
	}

	battery->pd_list.num_fpdo = pd_list_index;

	if (battery->sink_status.has_apdo) {
		/* unconditionally add APDO list */
		for (i = 1; i <= battery->sink_status.available_pdo_num; i++)
		{
			pPower_list = &battery->sink_status.power_list[i];

			if (pPower_list->apdo && pd_list_index >= 0 && pd_list_index < MAX_PDO_NUM) {
				battery->pd_list.pd_info[pd_list_index].pdo_index = i;
				battery->pd_list.pd_info[pd_list_index].apdo = true;
				battery->pd_list.pd_info[pd_list_index].max_voltage = pPower_list->max_voltage;
				battery->pd_list.pd_info[pd_list_index].min_voltage = pPower_list->min_voltage;
				battery->pd_list.pd_info[pd_list_index].max_current = pPower_list->max_current;

				pd_list_index++;
			}
		}
		battery->pd_list.num_apdo = pd_list_index - battery->pd_list.num_fpdo;
	} else {
		/* battery->sink_status has no apdo */
		battery->pd_list.num_apdo = 0;
	}
	num_pd_list = pd_list_index;

	if (num_pd_list <= 0 || num_pd_list > MAX_PDO_NUM) {
		pr_info("%s : PDO list is wrong: %d!!\n", __func__, num_pd_list);
		return 0;
	} else {
		pr_info("%s: total num_pd_list: %d, num_fpdo: %d, num_apdo: %d\n",
			__func__, num_pd_list, battery->pd_list.num_fpdo, battery->pd_list.num_apdo);
	}

	/* select  high voltage pdo */
	pd_list_select = num_pd_list - battery->pd_list.num_apdo - 1;
	if (pd_list_select < 0 || pd_list_select >= MAX_PDO_NUM) {
		pr_info("%s: pd_list_select is wrong: %d\n", __func__, pd_list_select);
		return 0;
	}

	for (i = 0; i < num_pd_list; i++) {
		pr_info("%s: Made pd_list[%d] %s[%d,%s] maxVol:%d, minVol:%d, maxCur:%d, comm:%d, suspend:%d\n",
			__func__, i, i == pd_list_select ? "**" : " ",
			battery->pd_list.pd_info[i].pdo_index,
			battery->pd_list.pd_info[i].apdo ? "APDO" : "FIXED",
			battery->pd_list.pd_info[i].max_voltage,
			battery->pd_list.pd_info[i].min_voltage,
			battery->pd_list.pd_info[i].max_current,
			battery->pd_list.pd_info[i].comm_capable,
			battery->pd_list.pd_info[i].suspend);
	}

	battery->pd_list.max_pd_count = num_pd_list;

	if (!battery->sink_status.has_apdo ||
		battery->current_event & SEC_BAT_CURRENT_EVENT_HV_DISABLE) {
		battery->max_charge_power = mW_by_mVmA(battery->sink_status.power_list[battery->pd_list.pd_info[pd_list_select].pdo_index].max_voltage,
			battery->sink_status.power_list[battery->pd_list.pd_info[pd_list_select].pdo_index].max_current);
		battery->pd_max_charge_power = battery->max_charge_power;
	}

#if defined(CONFIG_BATTERY_CISD)
	if (battery->cable_type == SEC_BATTERY_CABLE_NONE) {
		if (battery->pd_max_charge_power > 12000)
			battery->cisd.cable_data[CISD_CABLE_PD_HIGH]++;
		else
			battery->cisd.cable_data[CISD_CABLE_PD]++;
	}
#endif

	if (battery->sink_status.selected_pdo_num == battery->pd_list.pd_info[pd_list_select].pdo_index) {
		battery->pdic_ps_rdy = true;
		dev_info(battery->dev, "%s: battery->pdic_ps_rdy(%d)\n", __func__, battery->pdic_ps_rdy);
	} else if (battery->wc_rx_type != SS_GEAR) {
		/* change input current before request new pdo if new pdo's input current is less than now */
		int input_current = battery->pd_list.pd_info[pd_list_select].max_current;
		battery->pdic_ps_rdy = false;
		sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SELECT_PDO,
			SEC_BAT_CURRENT_EVENT_SELECT_PDO);
		sec_vote(battery->input_vote, VOTER_SELECT_PDO, true, input_current);
		if (pd_list_select >= 0 && pd_list_select < MAX_PDO_NUM)
			sec_pd_select_pdo(battery->pd_list.pd_info[pd_list_select].pdo_index);
	}

	battery->pd_list.now_pd_index = sec_bat_get_pd_list_index(battery->sink_status.current_pdo_num,
		&battery->pd_list);
	pr_info("%s : now_pd_index : %d\n", __func__, battery->pd_list.now_pd_index);

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	value.intval = battery->pd_list.num_apdo > 0 ? battery->pd_list.num_fpdo : 1;
	psy_do_property(battery->pdata->charger_name, set,
		POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO, value);
#endif

	return battery->pd_list.max_pd_count;
}

static int usb_typec_handle_id_attach(struct sec_battery_info *battery, PD_NOTI_ATTACH_TYPEDEF *pdata, int *cable_type, const char **cmd)
{
	struct pdic_notifier_struct *pd_noti = pdata->pd;
	SEC_PD_SINK_STATUS *psink_status = NULL;

	if (pd_noti)
		psink_status = &pd_noti->sink_status;

	switch (pdata->attach) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		*cmd = "DETACH";
		battery->is_jig_on = false;
		battery->pd_usb_attached = false;
		*cable_type = SEC_BATTERY_CABLE_NONE;
		battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
		battery->sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		/* Skip notify from MUIC if PDIC is attached already */
		if (is_pd_wire_type(battery->wire_status) || battery->init_src_cap) {
			if (lpcharge ||
				(battery->usb_thm_status == USB_THM_NORMAL &&
				!(battery->misc_event & BATT_MISC_EVENT_TEMP_HICCUP_TYPE))) {
				return -1; /* skip usb_typec_handle_after_id() */
			}
		}
		*cmd = "ATTACH";
		battery->muic_cable_type = pdata->cable_type;
		*cable_type = sec_bat_cable_check(battery, battery->muic_cable_type);
		if (battery->cable_type != *cable_type &&
			battery->sink_status.rp_currentlvl >= RP_CURRENT_LEVEL_DEFAULT &&
			(*cable_type == SEC_BATTERY_CABLE_USB || *cable_type == SEC_BATTERY_CABLE_TA)) {
			sec_bat_set_rp_current(battery, *cable_type);
		} else if (psink_status &&
			pd_noti->event == PDIC_NOTIFY_EVENT_PDIC_ATTACH &&
			psink_status->rp_currentlvl >= RP_CURRENT_LEVEL_DEFAULT &&
			(*cable_type == SEC_BATTERY_CABLE_USB || *cable_type == SEC_BATTERY_CABLE_TA)) {
			battery->sink_status.rp_currentlvl = psink_status->rp_currentlvl;
			sec_bat_set_rp_current(battery, *cable_type);
		}
		break;
	default:
		*cmd = "ERROR";
		*cable_type = -1;
		battery->muic_cable_type = pdata->cable_type;
		break;
	}
	battery->pdic_attach = false;
	battery->pdic_ps_rdy = false;
	battery->init_src_cap = false;
	if (battery->muic_cable_type == ATTACHED_DEV_QC_CHARGER_9V_MUIC ||
		battery->muic_cable_type == ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC)
		battery->hv_chg_name = "QC";
	else if (battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_9V_MUIC ||
		battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC ||
		battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC ||
		battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC)
		battery->hv_chg_name = "AFC";
#if defined(CONFIG_MUIC_HV_12V)
	else if (battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_12V_MUIC ||
		battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC)
		battery->hv_chg_name = "12V";
#endif
	else
		battery->hv_chg_name = "NONE";

	dev_info(battery->dev, "%s: cable_type:%d, muic_cable_type:%d\n",
		__func__, *cable_type, battery->muic_cable_type);

	return 0;
}

static int usb_typec_handle_id_power_status(struct sec_battery_info *battery,
		PD_NOTI_POWER_STATUS_TYPEDEF *pdata, int *cable_type, const char **cmd)
{
	int pd_charging_charge_power = battery->current_event & SEC_BAT_CURRENT_EVENT_HV_DISABLE ?
		battery->pdata->nv_charge_power : battery->pdata->pd_charging_charge_power;
	int i = 0, current_pdo = 0;
	int max_power = 0, fpdo_power = 0;
	bool bPdIndexChanged = false, bPrintPDlog = true;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	union power_supply_propval val = {0, };
#endif
	struct pdic_notifier_struct *pd_noti = pdata->pd;
	SEC_PD_SINK_STATUS *psink_status = NULL;

	if (pd_noti)
		psink_status = &pd_noti->sink_status;

	if (!psink_status) {
		dev_err(battery->dev, "%s: sink_status is NULL\n", __func__);
		return -1; /* skip usb_typec_handle_after_id() */
	}

#ifdef CONFIG_SEC_FACTORY
	dev_info(battery->dev, "%s: pd_event(%d)\n", __func__, pd_noti->event);
#endif

	if (pd_noti->event != PDIC_NOTIFY_EVENT_DETACH &&
		pd_noti->event != PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC) {
		if (!lpcharge && (battery->usb_thm_status ||
					(battery->misc_event & BATT_MISC_EVENT_TEMP_HICCUP_TYPE))) {
			return 0;
		}
	}

	switch (pd_noti->event) {
	case PDIC_NOTIFY_EVENT_DETACH:
		dev_info(battery->dev, "%s: skip pd operation - attach(%d)\n", __func__, pdata->attach);
		battery->pdic_attach = false;
		battery->pdic_ps_rdy = false;
		battery->init_src_cap = false;
		battery->hv_pdo = false;
		battery->pd_list.now_pd_index = 0;
		battery->pd_list.now_isApdo = false;
		battery->pd_list.num_apdo = 0;
		battery->pd_list.num_fpdo = 0;
		return -1; /* usb_typec_handle_after_id() */
		break;
	case PDIC_NOTIFY_EVENT_PDIC_ATTACH:
		battery->sink_status.rp_currentlvl = psink_status->rp_currentlvl;
		dev_info(battery->dev, "%s: battery->rp_currentlvl(%d)\n",
				__func__, battery->sink_status.rp_currentlvl);
		if (battery->wire_status == SEC_BATTERY_CABLE_USB || battery->wire_status == SEC_BATTERY_CABLE_TA) {
			*cable_type = battery->wire_status;
			battery->chg_limit = false;
			sec_bat_set_rp_current(battery, *cable_type);
			return 0;
		}
		return -1; /* skip usb_typec_handle_after_id() */
		break;
	case PDIC_NOTIFY_EVENT_PD_SOURCE:
	case PDIC_NOTIFY_EVENT_PD_SINK:
		break;
	case PDIC_NOTIFY_EVENT_PD_SINK_CAP:
		battery->update_pd_list = true;
		break;
	case PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC:
		*cmd = "PD_PRWAP";
		dev_info(battery->dev, "%s: PRSWAP_SNKTOSRC(%d)\n", __func__, pdata->attach);
		*cable_type = SEC_BATTERY_CABLE_NONE;

		battery->pdic_attach = false;
		battery->pdic_ps_rdy = false;
		battery->init_src_cap = false;
		battery->hv_pdo = false;
		battery->pd_list.now_pd_index = 0;
		return 0;
		break;
	default:
		break;
	}

	*cmd = "PD_ATTACH";
	battery->init_src_cap = false;

	if (battery->update_pd_list) {
		pr_info("%s : update_pd_list(%d)\n", __func__, battery->update_pd_list);
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
#if defined(CONFIG_STEP_CHARGING)
		sec_bat_reset_step_charging(battery);
#endif
		psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_DIRECT_CLEAR_ERR, val);
#endif
		battery->pdic_attach = false;
		battery->update_pd_list = false;
	}
	if (!battery->pdic_attach) {
		battery->sink_status = *psink_status;
		battery->pd_list.now_pd_index = 0;
		battery->hv_pdo = false;
		bPdIndexChanged = true;
	} else {
		unsigned int prev_pd_index = battery->pd_list.now_pd_index;

		battery->sink_status.selected_pdo_num = psink_status->selected_pdo_num;
		battery->sink_status.current_pdo_num = psink_status->current_pdo_num;
		battery->pd_list.now_pd_index = sec_bat_get_pd_list_index(battery->sink_status.current_pdo_num,
			&battery->pd_list);
		dev_info(battery->dev, "%s: battery->pd_list.now_pd_index(%d), prev_pd_index(%d)\n",
			__func__, battery->pd_list.now_pd_index, prev_pd_index);
		if (battery->pd_list.now_pd_index != prev_pd_index) {
			bPdIndexChanged = true;
		}

		if (battery->pd_list.now_pd_index > 0)
			battery->hv_pdo = true;
		else
			battery->hv_pdo = false;

		battery->pdic_ps_rdy = true;
		dev_info(battery->dev, "%s: battery->pdic_ps_rdy(%d), hv_pdo(%d)\n",
			__func__, battery->pdic_ps_rdy, battery->hv_pdo);
	}
	current_pdo = battery->sink_status.current_pdo_num;

	if (battery->sink_status.has_apdo) {
		*cable_type = SEC_BATTERY_CABLE_PDIC_APDO;
		if (battery->sink_status.power_list[current_pdo].apdo) {
			battery->hv_chg_name = "PDIC_APDO";
			battery->pd_list.now_isApdo = true;
		} else {
			battery->hv_chg_name = "PDIC_FIXED";
			battery->pd_list.now_isApdo = false;
		}

		if (battery->pdic_attach)
			bPrintPDlog = false;
	} else {
		*cable_type = SEC_BATTERY_CABLE_PDIC;
		battery->hv_chg_name = "PDIC";
		battery->pd_list.now_isApdo = false;
	}
	battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
	battery->input_voltage = battery->sink_status.power_list[current_pdo].max_voltage;
	dev_info(battery->dev, "%s: available pdo : %d, current pdo : %d\n", __func__,
		battery->sink_status.available_pdo_num, current_pdo);

	for (i = 1; i <= battery->sink_status.available_pdo_num; i++) {
		bool isUpdated = false;
		bool isApdo = battery->sink_status.power_list[i].apdo;
		bool isAccpet = battery->sink_status.power_list[i].accept;
		int power = 0;

		if (!battery->pdic_attach &&
			(battery->sink_status.power_list[i].max_voltage *
			battery->sink_status.power_list[i].max_current) > max_power) {
			max_power = mW_by_mVmA(battery->sink_status.power_list[i].max_voltage,
				battery->sink_status.power_list[i].max_current);
			pr_info("%s: max_power = %dmW\n", __func__, max_power);
		}

		if (bPrintPDlog)
			pr_info("%s:%spower_list[%d,%s,%s], maxVol:%d, minVol:%d, maxCur:%d, power:%d\n",
				__func__, i == current_pdo ? "**" : "  ",
				i, isApdo ? "APDO" : "FIXED", isAccpet ? "O" : "X",
				battery->sink_status.power_list[i].max_voltage,
				isApdo ? battery->sink_status.power_list[i].min_voltage : 0,
				battery->sink_status.power_list[i].max_current,
				battery->sink_status.power_list[i].max_voltage *
				battery->sink_status.power_list[i].max_current);

		/* no change apdo */
		if (!isAccpet || isApdo)
			continue;

		if (!battery->pdic_attach && battery->sink_status.has_apdo && !isApdo &&
			(battery->sink_status.power_list[i].max_voltage *
			battery->sink_status.power_list[i].max_current) > fpdo_power) {
			fpdo_power = mW_by_mVmA(battery->sink_status.power_list[i].max_voltage,
				battery->sink_status.power_list[i].max_current);
			pr_info("%s: fpdo_power = %dmW\n", __func__, fpdo_power);
		}

		power = mW_by_mVmA(battery->sink_status.power_list[i].max_voltage,
			battery->sink_status.power_list[i].max_current);
		if (power > pd_charging_charge_power) {
			battery->sink_status.power_list[i].max_current =
				mA_by_mWmV(pd_charging_charge_power, battery->sink_status.power_list[i].max_voltage);
			isUpdated = true;
		}

		if (battery->sink_status.power_list[i].max_current >
		    battery->pdata->max_input_current) {
			isUpdated = true;
			battery->sink_status.power_list[i].max_current =
				battery->pdata->max_input_current;
		}

		if (isUpdated) {
			if (bPrintPDlog)
				pr_info("%s: ->updated [%d,%s,%s], maxVol:%d, minVol:%d, maxCur:%d, power:%d\n",
					__func__, i, isApdo ? "APDO" : "FIXED", isAccpet ? "O" : "X",
					battery->sink_status.power_list[i].max_voltage,
					isApdo ? battery->sink_status.power_list[i].min_voltage : 0,
					battery->sink_status.power_list[i].max_current,
					battery->sink_status.power_list[i].max_voltage *
					battery->sink_status.power_list[i].max_current);
		}
	}

	if (!battery->pdic_attach) {
		if (battery->sink_status.has_apdo &&
			!(battery->current_event & SEC_BAT_CURRENT_EVENT_HV_DISABLE)) {
			fpdo_power = fpdo_power > battery->pdata->max_charging_charge_power ?
				battery->pdata->max_charging_charge_power : fpdo_power;
			battery->max_charge_power = fpdo_power;
			battery->pd_max_charge_power = battery->max_charge_power;
			pr_info("%s: pd_max_charge_power = %dmW\n", __func__, battery->pd_max_charge_power);
		}
#if defined(CONFIG_BATTERY_CISD)
		count_cisd_power_data(&battery->cisd, max_power);
#endif
		if (make_pd_list(battery) <= 0)
			return -1; /* skip usb_typec_handle_after_id() */
	}
	battery->pdic_attach = true;
	if (is_pd_apdo_wire_type(battery->wire_status) && !bPdIndexChanged &&
		battery->sink_status.power_list[current_pdo].apdo) {
		battery->wire_status = *cable_type;
		return -1; /* skip usb_typec_handle_after_id() */
	}

	return 0;
}

static int usb_typec_handle_id_usb(struct sec_battery_info *battery, PD_NOTI_ATTACH_TYPEDEF *pdata)
{
	if (pdata->cable_type == PD_USB_TYPE)
		battery->pd_usb_attached = true;
	dev_info(battery->dev, "%s: PDIC_NOTIFY_ID_USB: %d\n",__func__, battery->pd_usb_attached);
	__pm_stay_awake(battery->monitor_ws);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);

	return -1; /* skip usb_typec_handle_after_id() */
}

static int usb_typec_handle_after_id(struct sec_battery_info *battery, int cable_type, const char *cmd)
{
#if defined(CONFIG_PD_CHARGER_HV_DISABLE) && !defined(CONFIG_SEC_FACTORY)
	if (battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC) {
		pr_info("%s set SEC_BAT_CURRENT_EVENT_AFC_DISABLE\n", __func__);
		sec_bat_set_current_event(battery,
			SEC_BAT_CURRENT_EVENT_AFC_DISABLE, SEC_BAT_CURRENT_EVENT_AFC_DISABLE);
		__pm_stay_awake(battery->monitor_ws);
		queue_delayed_work(battery->monitor_wqueue,
				   &battery->monitor_work, 0);
	} else {
		sec_bat_set_current_event(battery,
			0, SEC_BAT_CURRENT_EVENT_AFC_DISABLE);
	}
#endif
	sec_bat_set_misc_event(battery,
		(battery->muic_cable_type == ATTACHED_DEV_UNDEFINED_CHARGING_MUIC ?
		BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE : 0) |
		(battery->muic_cable_type == ATTACHED_DEV_UNDEFINED_RANGE_MUIC ?
		BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE : 0),
		BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE);

	if (battery->muic_cable_type == ATTACHED_DEV_HICCUP_MUIC) {
		if (battery->usb_thm_status || (battery->misc_event & BATT_MISC_EVENT_TEMP_HICCUP_TYPE)) {
			pr_info("%s: Hiccup Set because of USB Temp\n", __func__);
			sec_bat_set_misc_event(battery,
					BATT_MISC_EVENT_TEMP_HICCUP_TYPE, BATT_MISC_EVENT_TEMP_HICCUP_TYPE);
			battery->usb_thm_status = USB_THM_NORMAL;
		} else {
			pr_info("%s: Hiccup Set because of Water detect\n", __func__);
			sec_bat_set_misc_event(battery,
					BATT_MISC_EVENT_WATER_HICCUP_TYPE, BATT_MISC_EVENT_WATER_HICCUP_TYPE);
		}
		battery->hiccup_status = 1;
	} else {
		battery->hiccup_status = 0;
		if (battery->hiccup_clear) {
			sec_bat_set_misc_event(battery, 0,
				(BATT_MISC_EVENT_WATER_HICCUP_TYPE | BATT_MISC_EVENT_TEMP_HICCUP_TYPE));
			battery->hiccup_clear = false;
			pr_info("%s : Hiccup event clear! hiccup clear bit set (%d)\n",
					__func__, battery->hiccup_clear);
		} else if (battery->misc_event &
				(BATT_MISC_EVENT_WATER_HICCUP_TYPE | BATT_MISC_EVENT_TEMP_HICCUP_TYPE)) {
			__pm_stay_awake(battery->monitor_ws);
			queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		}
	}

	/* showing charging icon and noti(no sound, vi, haptic) only
	   if slow insertion is detected by MUIC */
	sec_bat_set_misc_event(battery,
		(battery->muic_cable_type == ATTACHED_DEV_TIMEOUT_OPEN_MUIC ? BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE : 0),
		 BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE);

	if (cable_type < 0 || cable_type > SEC_BATTERY_CABLE_MAX) {
		dev_info(battery->dev, "%s: ignore event(%d)\n",
			__func__, battery->muic_cable_type);
		return -1; /* skip usb_typec_handle_after_id() */
	} else if ((cable_type == SEC_BATTERY_CABLE_UNKNOWN) &&
		   (battery->status != POWER_SUPPLY_STATUS_DISCHARGING)) {
		battery->cable_type = cable_type;
		__pm_stay_awake(battery->monitor_ws);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		dev_info(battery->dev, "%s: UNKNOWN cable plugin\n", __func__);
		return -1; /* skip usb_typec_handle_after_id() */
	}
	battery->wire_status = cable_type;

#if defined(CONFIG_WIRELESS_TX_MODE)
	if (battery->wc_tx_enable) {
		int work_delay = 0;

		if (battery->wire_status == SEC_BATTERY_CABLE_NONE) {
			battery->buck_cntl_by_tx = true;
			sec_vote(battery->chgen_vote, VOTER_WC_TX, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			battery->tx_switch_mode = TX_SWITCH_MODE_OFF;
			battery->tx_switch_mode_change = false;
			battery->tx_switch_start_soc = 0;
		}

		if (battery->pdata->tx_5v_disable && battery->wire_status == SEC_BATTERY_CABLE_TA)
			work_delay = battery->pdata->pre_afc_work_delay + 500;	//add delay more afc check

		sec_bat_run_wpc_tx_work(battery, work_delay);
	}
#endif

	cancel_delayed_work(&battery->cable_work);
	__pm_relax(battery->cable_ws);

	if (cable_type == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT) {
		/* set current event */
		cancel_delayed_work(&battery->afc_work);
		__pm_relax(battery->afc_ws);
		sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_CHG_LIMIT,
					  (SEC_BAT_CURRENT_EVENT_CHG_LIMIT | SEC_BAT_CURRENT_EVENT_AFC));
		__pm_stay_awake(battery->monitor_ws);
		battery->polling_count = 1;	/* initial value = 1 */
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	} else if ((battery->wire_status == battery->cable_type) &&
		(((battery->wire_status == SEC_BATTERY_CABLE_USB || battery->wire_status == SEC_BATTERY_CABLE_TA) &&
		battery->sink_status.rp_currentlvl > RP_CURRENT_LEVEL_DEFAULT &&
		!(battery->current_event & SEC_BAT_CURRENT_EVENT_AFC)) ||
		is_hv_wire_type(battery->wire_status))) {
		cancel_delayed_work(&battery->afc_work);
		__pm_relax(battery->afc_ws);
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_AFC);

		__pm_stay_awake(battery->monitor_ws);
		battery->polling_count = 1;	/* initial value = 1 */
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	} else if (cable_type == SEC_BATTERY_CABLE_PREPARE_TA) {
		sec_bat_check_afc_input_current(battery);
	} else {
		__pm_stay_awake(battery->cable_ws);
		if (battery->ta_alert_wa && battery->ta_alert_mode != OCP_NONE) {
			if (!strcmp(cmd, "DETACH")) {
				queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work, msecs_to_jiffies(3000));
			} else {
				queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work, 0);
			}
		} else {
			queue_delayed_work(battery->monitor_wqueue,
				&battery->cable_work, 0);
		}
	}

	return 0;
}

static int usb_typec_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	const char *cmd = "NONE";
	struct sec_battery_info *battery =
			container_of(nb, struct sec_battery_info, usb_typec_nb);
	int cable_type = SEC_BATTERY_CABLE_NONE;
	PD_NOTI_TYPEDEF *pdata = (PD_NOTI_TYPEDEF *)data;
	struct pdic_notifier_struct *pd_noti = pdata->pd;
	SEC_PD_SINK_STATUS *psink_status = NULL;
	int ret_handle = 0;

	dev_info(battery->dev, "%s: action:%ld src:%d, dest:%d, id:%d, sub1:%d, sub2:%d, sub3:%d\n",
		__func__, action, pdata->src, pdata->dest, pdata->id, pdata->sub1, pdata->sub2, pdata->sub3);

	if (pdata->dest != PDIC_NOTIFY_DEV_BATT) {
		dev_info(battery->dev, "%s: skip handler dest(%d)\n",
			__func__, pdata->dest);
		return 0;
	}

	if (!pd_noti) {
		dev_info(battery->dev, "%s: pd_noti(pdata->pd) is NULL\n", __func__);
	} else {
		psink_status = &pd_noti->sink_status;

		if (!battery->psink_status) {
			battery->psink_status = psink_status;
			sec_pd_init_data(battery->psink_status);
#if defined(CONFIG_BATTERY_CISD)
			sec_pd_register_chg_info_cb(count_cisd_pd_data);
#endif
		}
	}

	mutex_lock(&battery->typec_notylock);
	switch (pdata->id) {
	case PDIC_NOTIFY_ID_WATER:
	case PDIC_NOTIFY_ID_ATTACH:
		ret_handle = usb_typec_handle_id_attach(battery, (PD_NOTI_ATTACH_TYPEDEF *)pdata, &cable_type, &cmd);
		break;
	case PDIC_NOTIFY_ID_POWER_STATUS:
		ret_handle = usb_typec_handle_id_power_status(battery, (PD_NOTI_POWER_STATUS_TYPEDEF *)pdata, &cable_type, &cmd);
		break;
	case PDIC_NOTIFY_ID_USB:
		ret_handle = usb_typec_handle_id_usb(battery, (PD_NOTI_ATTACH_TYPEDEF *)pdata);
		break;
	default:
		cmd = "ERROR";
		cable_type = -1;
		battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
		battery->hv_chg_name = "NONE";
		break;
	}

	if (ret_handle < 0)
		goto skip_handle_after_id;

	usb_typec_handle_after_id(battery, cable_type, cmd);

skip_handle_after_id:
	dev_info(battery->dev, "%s: CMD[%s], CABLE_TYPE[%d]\n", __func__, cmd, cable_type);
	mutex_unlock(&battery->typec_notylock);
	return 0;
}
#else
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
static int batt_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	const char *cmd;
	int cable_type = SEC_BATTERY_CABLE_NONE;
	struct sec_battery_info *battery =
		container_of(nb, struct sec_battery_info,
			     batt_nb);
	union power_supply_propval value = {0, };

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) || IS_ENABLED(CONFIG_CCIC_NOTIFIER)
	PD_NOTI_ATTACH_TYPEDEF *p_noti = (PD_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = p_noti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif

	mutex_lock(&battery->batt_handlelock);
	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		cmd = "DETACH";
		battery->is_jig_on = false;
		cable_type = SEC_BATTERY_CABLE_NONE;
		battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		cmd = "ATTACH";
		cable_type = sec_bat_cable_check(battery, attached_dev);
		battery->muic_cable_type = attached_dev;
		break;
	default:
		cmd = "ERROR";
		cable_type = -1;
		battery->muic_cable_type = ATTACHED_DEV_NONE_MUIC;
		break;
	}

	sec_bat_set_misc_event(battery,
#if !defined(CONFIG_ENG_BATTERY_CONCEPT) && !defined(CONFIG_SEC_FACTORY)
		(battery->muic_cable_type == ATTACHED_DEV_JIG_UART_ON_MUIC ? BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE : 0) |
		(battery->muic_cable_type == ATTACHED_DEV_JIG_USB_ON_MUIC ? BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE : 0) |
#endif
		(battery->muic_cable_type == ATTACHED_DEV_UNDEFINED_RANGE_MUIC ? BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE : 0),
		BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE);

	if (battery->muic_cable_type == ATTACHED_DEV_HICCUP_MUIC) {
		if (battery->usb_thm_status || (battery->misc_event & BATT_MISC_EVENT_TEMP_HICCUP_TYPE)) {
			pr_info("%s: Hiccup Set because of USB Temp\n", __func__);
			sec_bat_set_misc_event(battery,
					BATT_MISC_EVENT_TEMP_HICCUP_TYPE, BATT_MISC_EVENT_TEMP_HICCUP_TYPE);
			battery->usb_thm_status = USB_THM_NORMAL;
		} else {
			pr_info("%s: Hiccup Set because of Water detect\n", __func__);
			sec_bat_set_misc_event(battery,
					BATT_MISC_EVENT_WATER_HICCUP_TYPE, BATT_MISC_EVENT_WATER_HICCUP_TYPE);
		}
		battery->hiccup_status = 1;
	} else {
		battery->hiccup_status = 0;
		if (battery->misc_event &
				(BATT_MISC_EVENT_WATER_HICCUP_TYPE | BATT_MISC_EVENT_TEMP_HICCUP_TYPE)) {
			__pm_stay_awake(battery->monitor_ws);
			queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		}
	}

	/* If PD cable is already attached, return this function */
	if (battery->pdic_attach) {
		dev_info(battery->dev, "%s: ignore event pdic attached(%d)\n",
			__func__, battery->pdic_attach);
		mutex_unlock(&battery->batt_handlelock);
		return 0;
	}

	if (attached_dev == ATTACHED_DEV_MHL_MUIC) {
		mutex_unlock(&battery->batt_handlelock);
		return 0;
	}

	if (cable_type < 0) {
		dev_info(battery->dev, "%s: ignore event(%d)\n",
			__func__, cable_type);
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_PACK;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_PACK) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_PACK_HV;
	} else if (cable_type == SEC_BATTERY_CABLE_HV_WIRELESS) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_HV;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_STAND;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_STAND) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_STAND_HV;
	} else if (cable_type == SEC_BATTERY_CABLE_PMA_WIRELESS) {
		battery->wc_status = SEC_WIRELESS_PAD_PMA;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_VEHICLE) {
		battery->wc_status = SEC_WIRELESS_PAD_VEHICLE;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE) {
		battery->wc_status = SEC_WIRELESS_PAD_VEHICLE_HV;
	} else if (cable_type == SEC_BATTERY_CABLE_WIRELESS_TX) {
		battery->wc_status = SEC_WIRELESS_PAD_TX;
	} else if (cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_PREPARE_HV_20;
	} else if (cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20) {
		battery->wc_status = SEC_WIRELESS_PAD_WPC_HV_20;
	} else if ((cable_type == SEC_BATTERY_CABLE_UNKNOWN) &&
		   (battery->status != POWER_SUPPLY_STATUS_DISCHARGING)) {
		battery->cable_type = cable_type;
		__pm_stay_awake(battery->monitor_ws);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		dev_info(battery->dev,
			"%s: UNKNOWN cable plugin\n", __func__);
		mutex_unlock(&battery->batt_handlelock);
		return 0;
	} else {
		battery->wire_status = cable_type;
		if (is_nocharge_type(battery->wire_status) && (battery->wc_status))
			cable_type = SEC_BATTERY_CABLE_WIRELESS;
	}
	dev_info(battery->dev,
			"%s: current_cable(%d), wc_status(%d), wire_status(%d)\n",
			__func__, cable_type, battery->wc_status,
			battery->wire_status);

	mutex_unlock(&battery->batt_handlelock);
	if (attached_dev == ATTACHED_DEV_USB_LANHUB_MUIC) {
		if (!strcmp(cmd, "ATTACH")) {
			value.intval = true;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_EXT_PROP_CHARGE_POWERED_OTG_CONTROL,
					value);
			dev_info(battery->dev,
				"%s: Powered OTG cable attached\n", __func__);
		} else {
			value.intval = false;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_EXT_PROP_CHARGE_POWERED_OTG_CONTROL,
					value);
			dev_info(battery->dev,
				"%s: Powered OTG cable detached\n", __func__);
		}
	}

	if (!strcmp(cmd, "ATTACH")) {
		if ((battery->muic_cable_type >= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC) &&
		    (battery->muic_cable_type <= ATTACHED_DEV_QC_CHARGER_9V_MUIC)) {
			battery->hv_chg_name = "QC";
		} else if ((battery->muic_cable_type >= ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC) &&
			 (battery->muic_cable_type <= ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC)) {
			battery->hv_chg_name = "AFC";
#if defined(CONFIG_MUIC_HV_12V)
		} else if (battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_12V_MUIC ||
			battery->muic_cable_type == ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC) {
			battery->hv_chg_name = "12V";
#endif
		} else
			battery->hv_chg_name = "NONE";
	} else {
			battery->hv_chg_name = "NONE";
	}

	pr_info("%s : HV_CHARGER_NAME(%s)\n",
		__func__, battery->hv_chg_name);

	if ((cable_type >= 0) &&
	    cable_type <= SEC_BATTERY_CABLE_MAX) {
		if (cable_type == SEC_BATTERY_CABLE_NONE) {
			__pm_stay_awake(battery->monitor_ws);
			queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
		} else if (cable_type != battery->cable_type) {
			__pm_stay_awake(battery->cable_ws);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->cable_work, 0);
		} else {
			dev_info(battery->dev,
				"%s: Cable is Not Changed(%d)\n",
				__func__, battery->cable_type);
		}
	}

	pr_info("%s: CMD=%s, attached_dev=%d\n", __func__, cmd, attached_dev);

	return 0;
}
#endif /* CONFIG_MUIC_NOTIFIER */
#endif

static void sec_bat_init_chg_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, init_chg_work.work);

	if (battery->cable_type == SEC_BATTERY_CABLE_NONE &&
		!(battery->misc_event & (BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE |
			BATT_MISC_EVENT_WATER_HICCUP_TYPE | BATT_MISC_EVENT_TEMP_HICCUP_TYPE))) {
		union power_supply_propval value = {0, };

		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_INITIAL_WC_CHECK, value);

		if (value.intval) {
			pr_info("%s: enable charging (wpc_det)\n", __func__);
			sec_vote(battery->chgen_vote, VOTER_CABLE, true, SEC_BAT_CHG_MODE_CHARGING);
		} else {
			pr_info("%s: disable charging\n", __func__);
			sec_vote(battery->chgen_vote, VOTER_CABLE, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
		}
	}
}

static void sec_bat_parse_param_value(struct sec_battery_info *battery)
{
	int pd_hv_disable = 0;
	union power_supply_propval value = {0, };

	battery->charging_night_mode = charging_mode & 0x000000FF;
	pr_info("%s: charging_mode: 0x%x (charging_night_mode:0x%x)\n",
		__func__, charging_mode, battery->charging_night_mode);

	/* Check High Voltage charging option for wireless charging */
	/* '1' means disabling High Voltage charging */
	if (battery->charging_night_mode == '1') /* 0x31 */
		battery->sleep_mode = true;
	else
		battery->sleep_mode = false;
	value.intval = battery->sleep_mode;
	psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_SLEEP_MODE, value);

	pd_hv_disable = (pd_disable & 0x000000FF);
	pr_info("%s: pd_disable: 0x%x (pd_hv_disable:0x%x)\n",
		__func__, pd_disable, pd_hv_disable);
#if defined(CONFIG_PD_CHARGER_HV_DISABLE)
	if (pd_hv_disable == '1') { /* 0x31 */
		battery->pd_disable = true;
		pr_info("PD wired charging mode is disabled\n");
		sec_bat_set_current_event(battery,
			SEC_BAT_CURRENT_EVENT_HV_DISABLE, SEC_BAT_CURRENT_EVENT_HV_DISABLE);
	}
#else
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	/* Check High Voltage charging option for wired charging */
	if (get_afc_mode() == CH_MODE_AFC_DISABLE_VAL) {
		pr_info("HV wired charging mode is disabled\n");
		sec_bat_set_current_event(battery,
			SEC_BAT_CURRENT_EVENT_HV_DISABLE, SEC_BAT_CURRENT_EVENT_HV_DISABLE);
	}
#endif
#endif
}

static void sec_battery_check_devs_registered(struct device *dev)
{
	struct device_node *np = dev->of_node;
	const char *dev_name;
	struct power_supply *psy_dev = NULL;

	if (!np) {
		pr_err("%s: np NULL\n", __func__);
		return;
	}

	if (!of_property_read_string(np, "battery,charger_name", &dev_name)) {
		psy_dev = power_supply_get_by_name(dev_name);
		if (psy_dev) {
			dev_info(dev, "%s: %s is registered\n", __func__, dev_name);
			power_supply_put(psy_dev);
		} else {
			dev_err(dev, "%s: %s is not registered yet\n", __func__, dev_name);
		}
	}

	if (!of_property_read_string(np, "battery,fuelgauge_name", &dev_name)) {
		psy_dev = power_supply_get_by_name(dev_name);
		if (psy_dev) {
			dev_info(dev, "%s: %s is registered\n", __func__, dev_name);
			power_supply_put(psy_dev);
		} else {
			dev_err(dev, "%s: %s is not registered yet\n", __func__, dev_name);
		}
	}

	if (!of_property_read_string(np, "battery,wireless_charger_name", &dev_name)) {
		psy_dev = power_supply_get_by_name(dev_name);
		if (psy_dev) {
			dev_info(dev, "%s: %s is registered\n", __func__, dev_name);
			power_supply_put(psy_dev);
		} else {
			dev_err(dev, "%s: %s is not registered yet\n", __func__, dev_name);
		}
	}
}

static const struct power_supply_desc battery_power_supply_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = sec_battery_props,
	.num_properties = ARRAY_SIZE(sec_battery_props),
	.get_property = sec_bat_get_property,
	.set_property = sec_bat_set_property,
};

static const struct power_supply_desc usb_power_supply_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = sec_power_props,
	.num_properties = ARRAY_SIZE(sec_power_props),
	.get_property = sec_usb_get_property,
};

static const struct power_supply_desc ac_power_supply_desc = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = sec_ac_props,
	.num_properties = ARRAY_SIZE(sec_ac_props),
	.get_property = sec_ac_get_property,
};

static const struct power_supply_desc wireless_power_supply_desc = {
	.name = "wireless",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = sec_wireless_props,
	.num_properties = ARRAY_SIZE(sec_wireless_props),
	.get_property = sec_wireless_get_property,
	.set_property = sec_wireless_set_property,
};

static const struct power_supply_desc pogo_power_supply_desc = {
	.name = "pogo",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sec_power_props,
	.num_properties = ARRAY_SIZE(sec_power_props),
	.get_property = sec_pogo_get_property,
	.set_property = sec_pogo_set_property,
};

static int sec_battery_probe(struct platform_device *pdev)
{
	sec_battery_platform_data_t *pdata = NULL;
	struct sec_battery_info *battery;
	struct power_supply_config battery_cfg = {};

	int ret = 0;
#ifndef CONFIG_OF
	int i = 0;
#endif
#if defined(CONFIG_STORE_MODE) && !defined(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_DIRECT_CHARGING)
	char direct_charging_source_status[2] = {0, };
#endif

	union power_supply_propval value = {0, };

	dev_info(&pdev->dev,
		"%s: SEC Battery Driver Loading\n", __func__);

	if (pdev->dev.of_node)
		sec_battery_check_devs_registered(&pdev->dev);

	battery = kzalloc(sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(sec_battery_platform_data_t),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_bat_free;
		}

		battery->pdata = pdata;

		if (sec_bat_parse_dt(&pdev->dev, battery)) {
			dev_err(&pdev->dev,
				"%s: Failed to get battery dt\n", __func__);
			ret = -EINVAL;
			goto err_pdata_free;
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		battery->pdata = pdata;
	}

	platform_set_drvdata(pdev, battery);

	battery->dev = &pdev->dev;

	mutex_init(&battery->adclock);
	mutex_init(&battery->iolock);
	mutex_init(&battery->misclock);
	mutex_init(&battery->txeventlock);
	mutex_init(&battery->batt_handlelock);
	mutex_init(&battery->current_eventlock);
	mutex_init(&battery->typec_notylock);
	mutex_init(&battery->wclock);
	mutex_init(&battery->voutlock);

	dev_dbg(battery->dev, "%s: ADC init\n", __func__);

#ifdef CONFIG_OF
	adc_init(pdev, battery);
#else
	for (i = 0; i < SEC_BAT_ADC_CHANNEL_NUM; i++)
		adc_init(pdev, pdata, i);
#endif
	battery->monitor_ws = wakeup_source_register(&pdev->dev, "sec-battery-monitor");
	battery->cable_ws = wakeup_source_register(&pdev->dev, "sec-battery-cable");
	battery->vbus_ws = wakeup_source_register(&pdev->dev, "sec-battery-vbus");
	battery->afc_ws = wakeup_source_register(&pdev->dev, "sec-battery-afc");
	battery->siop_level_ws = wakeup_source_register(&pdev->dev, "sec-battery-siop_level");
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	battery->ext_event_ws = wakeup_source_register(&pdev->dev, "sec-battery-ext_event");
	battery->wc_headroom_ws = wakeup_source_register(&pdev->dev, "sec-battery-wc_headroom");
	battery->wpc_tx_ws = wakeup_source_register(&pdev->dev, "sec-battery-wcp-tx");
	battery->tx_event_ws = wakeup_source_register(&pdev->dev, "sec-battery-tx-event");
#endif
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	battery->batt_data_ws = wakeup_source_register(&pdev->dev, "sec-battery-update-data");
#endif
	battery->misc_event_ws = wakeup_source_register(&pdev->dev, "sec-battery-misc-event");
#ifdef CONFIG_OF
	battery->parse_mode_dt_ws = wakeup_source_register(&pdev->dev, "sec-battery-parse_mode_dt");
#endif
	battery->fcc_vote = sec_vote_init("FCC", SEC_VOTE_MIN, VOTER_MAX,
			500, sec_voter_name, set_charging_current, battery);
	battery->input_vote = sec_vote_init("ICL", SEC_VOTE_MIN, VOTER_MAX,
			500, sec_voter_name, set_input_current, battery);
	battery->fv_vote = sec_vote_init("FV", SEC_VOTE_MIN, VOTER_MAX,
			battery->pdata->chg_float_voltage, sec_voter_name, set_float_voltage, battery);
	battery->chgen_vote = sec_vote_init("CHGEN", SEC_VOTE_MIN, VOTER_MAX,
			SEC_BAT_CHG_MODE_CHARGING_OFF, sec_voter_name, sec_bat_set_charge, battery);
	battery->topoff_vote = sec_vote_init("TOPOFF", SEC_VOTE_MIN, VOTER_MAX,
			battery->pdata->full_check_current_1st, sec_voter_name, set_topoff_current, battery);
	/* initialization of battery info */
	sec_bat_set_charging_status(battery,
			POWER_SUPPLY_STATUS_DISCHARGING);
	sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
	battery->ta_alert_mode = OCP_NONE;
	battery->present = true;
	battery->is_jig_on = false;
	battery->wdt_kick_disable = 0;

	battery->polling_count = 1;	/* initial value = 1 */
	battery->polling_time = pdata->polling_time[
		SEC_BATTERY_POLLING_TIME_DISCHARGING];
	battery->polling_in_sleep = false;
	battery->polling_short = false;

	battery->check_count = 0;
	battery->check_adc_count = 0;
	battery->check_adc_value = 0;

	battery->input_current = 0;
	battery->charging_current = 0;
#if defined(CONFIG_DUAL_BATTERY)
	battery->main_charging_current = 0;
	battery->sub_charging_current = 0;
#endif
	battery->topoff_condition = 0;
	battery->wpc_vout_level = WIRELESS_VOUT_10V;
	battery->wpc_max_vout_level = WIRELESS_VOUT_12_5V;
	battery->charging_start_time = 0;
	battery->charging_passed_time = 0;
	battery->wc_heating_start_time = 0;
	battery->wc_heating_passed_time = 0;
	battery->charging_next_time = 0;
	battery->charging_fullcharged_time = 0;
	battery->siop_level = 100;
	battery->wc_enable = true;
	battery->wc_enable_cnt = 0;
	battery->wc_enable_cnt_value = 3;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	battery->stability_test = 0;
	battery->eng_not_full_status = 0;
	battery->temperature_test_battery = 0x7FFF;
	battery->temperature_test_usb = 0x7FFF;
	battery->temperature_test_wpc = 0x7FFF;
	battery->temperature_test_chg = 0x7FFF;
#if defined(CONFIG_DUAL_BATTERY)
	battery->temperature_test_sub = 0x7FFF;
#endif
	battery->temperature_test_dchg = 0x7FFF;
	battery->temperature_test_blkt = 0x7FFF;
#if defined(CONFIG_STEP_CHARGING)
	battery->test_step_condition = 0x7FFF;
#endif
	battery->test_max_current = false;
	battery->test_charge_current = false;
#endif
	battery->wc_status = SEC_WIRELESS_PAD_NONE;
	battery->wc_cv_mode = false;
	battery->wire_status = SEC_BATTERY_CABLE_NONE;

	battery->wc_rx_phm_mode = false;
#if defined(CONFIG_TX_GEAR_AOV)
	battery->wc_tx_freq = 0;
	battery->wc_tx_adaptive_vout = false;
	battery->wc_found_gear_freq = false;
#endif
	battery->wc_tx_enable = false;
	battery->uno_en = false;
	battery->afc_disable = false;
	battery->pd_disable = false;
	battery->buck_cntl_by_tx = false;
	battery->wc_tx_vout = WC_TX_VOUT_5_0V;
	battery->wc_rx_type = NO_DEV;
	battery->tx_mfc_iout = 0;
	battery->tx_uno_iout = 0;
	battery->wc_need_ldo_on = false;

	battery->tx_minduty = battery->pdata->tx_minduty_default;

#if defined(CONFIG_WIRELESS_TX_MODE)
	battery->tx_clear = true;
	battery->tx_clear_cisd = true;
#endif
	battery->charging_block = false;
	battery->chg_limit = false;
	battery->mix_limit = false;
	battery->vbus_limit = false;
	battery->vbus_chg_by_siop = SEC_INPUT_VOLTAGE_0V;
	battery->vbus_chg_by_full = false;
	battery->usb_temp = 0;
	battery->dchg_temp = 0;
	battery->blkt_temp = 0;
	battery->skip_swelling = false;
	battery->bat_thm_count = 0;
	battery->adc_init_count = 0;
	battery->led_cover = 0;
	battery->hiccup_status = 0;
	battery->hiccup_clear = false;
	battery->ext_event = BATT_EXT_EVENT_NONE;
	battery->tx_retry_case = SEC_BAT_TX_RETRY_NONE;
	battery->tx_misalign_cnt = 0;
	battery->tx_ocp_cnt = 0;
	battery->auto_mode = false;
	battery->update_pd_list = false;

	psy_do_property(battery->pdata->wireless_charger_name, get,
		POWER_SUPPLY_EXT_PROP_WPC_EN, value);
	sec_bat_set_current_event(battery,
		value.intval ? SEC_BAT_CURRENT_EVENT_WPC_EN : 0, SEC_BAT_CURRENT_EVENT_WPC_EN);
	sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_USB_100MA, SEC_BAT_CURRENT_EVENT_USB_100MA);

	battery->charging_mode = SEC_BATTERY_CHARGING_NONE;
	battery->is_recharging = false;
	battery->cable_type = SEC_BATTERY_CABLE_NONE;
	battery->test_mode = 0;
	battery->factory_mode = false;
	battery->store_mode = false;
	battery->prev_usb_conf = USB_CURRENT_NONE;
	battery->is_hc_usb = false;
	battery->is_sysovlo = false;
	battery->is_vbatovlo = false;
	battery->is_abnormal_temp = false;
	battery->hv_pdo = false;

	battery->safety_timer_set = true;
	battery->stop_timer = false;
	battery->prev_safety_time = 0;
	battery->lcd_status = false;
	battery->wc_auth_retried = false;

#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	battery->wc20_power_class = 0;
	battery->wc20_vout = 0;
	battery->wc20_rx_power = 0;
#endif

	battery->thermal_zone = BAT_THERMAL_NORMAL;
	sec_bat_set_threshold(battery);

#if defined(CONFIG_BATTERY_CISD)
	battery->usb_overheat_check = false;
	battery->skip_cisd = false;
#endif

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	battery->batt_cycle = -1;
	battery->pdata->age_step = 0;
#endif
	battery->batt_asoc = 100;
	battery->health_change = false;
	battery->usb_thm_status = USB_THM_NORMAL;

	sec_bat_parse_param_value(battery);

	if (fg_reset)
		sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_FG_RESET,
			SEC_BAT_CURRENT_EVENT_FG_RESET);

	battery->pdata->store_mode_charging_max = STORE_MODE_CHARGING_MAX;
	battery->pdata->store_mode_charging_min = STORE_MODE_CHARGING_MIN;
#if !defined(CONFIG_SEC_FACTORY) && !defined(CONFIG_ARCH_EXYNOS)
	if (sales_code_is("VZW")) {
		dev_err(battery->dev, "%s: Sales is VZW\n", __func__);
		battery->pdata->store_mode_charging_max = STORE_MODE_CHARGING_MAX_VZW;
		battery->pdata->store_mode_charging_min = STORE_MODE_CHARGING_MIN_VZW;
	}
#endif
	if (battery->pdata->charger_name == NULL)
		battery->pdata->charger_name = "sec-charger";
	if (battery->pdata->fuelgauge_name == NULL)
		battery->pdata->fuelgauge_name = "sec-fuelgauge";

	/* create work queue */
	battery->monitor_wqueue =
		create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!battery->monitor_wqueue) {
		dev_err(battery->dev,
			"%s: Fail to Create Workqueue\n", __func__);
		goto err_irq;
	}

	ttf_init(battery);

	INIT_DELAYED_WORK(&battery->monitor_work, sec_bat_monitor_work);
	INIT_DELAYED_WORK(&battery->cable_work, sec_bat_cable_work);
	INIT_DELAYED_WORK(&battery->slowcharging_work, sec_bat_check_slowcharging_work);
	INIT_DELAYED_WORK(&battery->afc_work, sec_bat_afc_work);
	INIT_DELAYED_WORK(&battery->siop_level_work, sec_bat_siop_level_work);
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	INIT_DELAYED_WORK(&battery->fw_init_work, sec_bat_fw_init_work);
#endif
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	INIT_DELAYED_WORK(&battery->wpc_tx_work, sec_bat_wpc_tx_work);
	INIT_DELAYED_WORK(&battery->wpc_txpower_calc_work, sec_bat_txpower_calc_work);
	INIT_DELAYED_WORK(&battery->ext_event_work, sec_bat_ext_event_work);
	INIT_DELAYED_WORK(&battery->wc_headroom_work, sec_bat_wc_headroom_work);
	INIT_DELAYED_WORK(&battery->otg_work, sec_bat_otg_work);
	INIT_DELAYED_WORK(&battery->wc20_current_work, sec_bat_wc20_current_work);
#endif
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	INIT_DELAYED_WORK(&battery->batt_data_work, sec_bat_update_data_work);
#endif
	INIT_DELAYED_WORK(&battery->misc_event_work, sec_bat_misc_event_work);
#ifdef CONFIG_OF
	INIT_DELAYED_WORK(&battery->parse_mode_dt_work, sec_bat_parse_mode_dt_work);
#endif
	INIT_DELAYED_WORK(&battery->init_chg_work, sec_bat_init_chg_work);

	switch (pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		INIT_DELAYED_WORK(&battery->polling_work,
			sec_bat_polling_work);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		battery->last_poll_time = ktime_get_boottime();
		alarm_init(&battery->polling_alarm, ALARM_BOOTTIME,
			sec_bat_alarm);
		break;
	default:
		break;
	}

#if defined(CONFIG_BATTERY_CISD)
	sec_battery_cisd_init(battery);
#endif
	/* updates temperatures on boot */
	sec_bat_get_temperature_info(battery);

	battery_cfg.drv_data = battery;

	/* init power supplier framework */
	battery->psy_usb = power_supply_register(&pdev->dev, &usb_power_supply_desc, &battery_cfg);
	if (IS_ERR(battery->psy_usb)) {
		ret = PTR_ERR(battery->psy_usb);
		dev_err(battery->dev,
			"%s: Failed to Register psy_usb(%d)\n", __func__, ret);
		goto err_workqueue;
	}
	battery->psy_usb->supplied_to = supply_list;
	battery->psy_usb->num_supplicants = ARRAY_SIZE(supply_list);

	battery->psy_ac = power_supply_register(&pdev->dev, &ac_power_supply_desc, &battery_cfg);
	if (IS_ERR(battery->psy_ac)) {
		ret = PTR_ERR(battery->psy_ac);
		dev_err(battery->dev,
			"%s: Failed to Register psy_ac(%d)\n", __func__, ret);
		goto err_supply_unreg_usb;
	}
	battery->psy_ac->supplied_to = supply_list;
	battery->psy_ac->num_supplicants = ARRAY_SIZE(supply_list);

	battery->psy_bat = power_supply_register(&pdev->dev, &battery_power_supply_desc, &battery_cfg);
	if (IS_ERR(battery->psy_bat)) {
		ret = PTR_ERR(battery->psy_bat);
		dev_err(battery->dev,
			"%s: Failed to Register psy_bat(%d)\n", __func__, ret);
		goto err_supply_unreg_ac;
	}

	battery->psy_pogo = power_supply_register(&pdev->dev, &pogo_power_supply_desc, &battery_cfg);
	if (IS_ERR(battery->psy_pogo)) {
		ret = PTR_ERR(battery->psy_pogo);
		dev_err(battery->dev,
			"%s: Failed to Register psy_pogo(%d)\n", __func__, ret);
		goto err_supply_unreg_bat;
	}

	battery->psy_wireless = power_supply_register(&pdev->dev, &wireless_power_supply_desc, &battery_cfg);
	if (IS_ERR(battery->psy_wireless)) {
		ret = PTR_ERR(battery->psy_wireless);
		dev_err(battery->dev,
			"%s: Failed to Register psy_wireless(%d)\n", __func__, ret);
		goto err_supply_unreg_pogo;
	}
	battery->psy_wireless->supplied_to = supply_list;
	battery->psy_wireless->num_supplicants = ARRAY_SIZE(supply_list);

	ret = sec_bat_create_attrs(&battery->psy_bat->dev);
	if (ret) {
		dev_err(battery->dev,
			"%s : Failed to sec_bat_create_attrs\n", __func__);
		goto err_req_irq;
	}

	ret = sec_pogo_create_attrs(&battery->psy_pogo->dev);
	if (ret) {
		dev_err(battery->dev,
			"%s : Failed to sec_pogo_create_attrs\n", __func__);
		goto err_req_irq;
	}

	/* initialize battery level*/
	value.intval = 0;
	psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
	battery->capacity = value.intval;

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	queue_delayed_work(battery->monitor_wqueue, &battery->fw_init_work, msecs_to_jiffies(2000));
#endif

	/* notify wireless charger driver when sec_battery probe is done,
		if wireless charging is possible, POWER_SUPPLY_PROP_ONLINE of wireless property will be called. */
	value.intval = 0;
	psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_TYPE, value);

#if defined(CONFIG_STORE_MODE) && !defined(CONFIG_SEC_FACTORY)
	battery->store_mode = true;
	sec_bat_parse_mode_dt(battery);
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	direct_charging_source_status[0] = SEC_STORE_MODE;
	direct_charging_source_status[1] = SEC_CHARGING_SOURCE_SWITCHING;
	value.strval = direct_charging_source_status;
	psy_do_property(battery->pdata->charger_name, set,
		POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE, value);
#endif
#endif

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	battery->sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	manager_notifier_register(&battery->usb_typec_nb,
		usb_typec_handle_notification, MANAGER_NOTIFY_PDIC_BATTERY);
#else
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&battery->batt_nb,
		batt_handle_notification, MUIC_NOTIFY_DEV_CHARGER);
#endif
#endif

#if defined(CONFIG_WIRELESS_AUTH)
	sec_bat_misc_init(battery);
#endif

	value.intval = true;
	psy_do_property(battery->pdata->charger_name, set,
		POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);

	/* make fg_reset true again for actual normal booting after recovery kernel is done */
#if defined(CONFIG_SEC_COMMON)
	if (fg_reset && seccmn_recv_is_boot_recovery()) {
		pr_info("%s: fg_reset(%d) boot_recov(%d)\n",
			__func__, fg_reset, seccmn_recv_is_boot_recovery());
		psy_do_property(battery->pdata->fuelgauge_name, set,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);
		pr_info("%s: make fg_reset true again for actual normal booting\n", __func__);
	}
#endif
	if ((battery->cable_type == SEC_BATTERY_CABLE_NONE) ||
		(battery->cable_type == SEC_BATTERY_CABLE_PREPARE_TA)) {
		queue_delayed_work(battery->monitor_wqueue, &battery->init_chg_work, 0);

		pr_info("%s: SEC Battery Driver Monitor work\n", __func__);
		__pm_stay_awake(battery->monitor_ws);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	}

	if (battery->pdata->check_battery_callback)
		battery->present = battery->pdata->check_battery_callback();

	sec_vote(battery->topoff_vote, VOTER_FULL_CHARGE, true, battery->pdata->full_check_current_1st);
	sec_vote(battery->fv_vote, VOTER_FULL_CHARGE, true, battery->pdata->chg_float_voltage);
	pr_info("%s: SEC Battery Driver Loaded\n", __func__);
	return 0;

err_req_irq:
	power_supply_unregister(battery->psy_wireless);
err_supply_unreg_pogo:
	power_supply_unregister(battery->psy_pogo);
err_supply_unreg_bat:
	power_supply_unregister(battery->psy_bat);
err_supply_unreg_ac:
	power_supply_unregister(battery->psy_ac);
err_supply_unreg_usb:
	power_supply_unregister(battery->psy_usb);
err_workqueue:
	destroy_workqueue(battery->monitor_wqueue);
err_irq:
	wakeup_source_unregister(battery->monitor_ws);
	wakeup_source_unregister(battery->cable_ws);
	wakeup_source_unregister(battery->vbus_ws);
	wakeup_source_unregister(battery->afc_ws);
	wakeup_source_unregister(battery->siop_level_ws);
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	wakeup_source_unregister(battery->ext_event_ws);
	wakeup_source_unregister(battery->wc_headroom_ws);
	wakeup_source_unregister(battery->wpc_tx_ws);
	wakeup_source_unregister(battery->tx_event_ws);
#endif
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	wakeup_source_unregister(battery->batt_data_ws);
#endif
	wakeup_source_unregister(battery->misc_event_ws);
#ifdef CONFIG_OF
	wakeup_source_unregister(battery->parse_mode_dt_ws);
#endif
	mutex_destroy(&battery->adclock);
	mutex_destroy(&battery->iolock);
	mutex_destroy(&battery->misclock);
	mutex_destroy(&battery->txeventlock);
	mutex_destroy(&battery->batt_handlelock);
	mutex_destroy(&battery->current_eventlock);
	mutex_destroy(&battery->typec_notylock);
	mutex_destroy(&battery->wclock);
	mutex_destroy(&battery->voutlock);
err_pdata_free:
	kfree(pdata);
err_bat_free:
	kfree(battery);

	return ret;
}

static int sec_battery_remove(struct platform_device *pdev)
{
	struct sec_battery_info *battery = platform_get_drvdata(pdev);
#ifndef CONFIG_OF
	int i;
#endif

	pr_info("%s: ++\n", __func__);

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		cancel_delayed_work(&battery->polling_work);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		alarm_cancel(&battery->polling_alarm);
		break;
	default:
		break;
	}

	flush_workqueue(battery->monitor_wqueue);
	destroy_workqueue(battery->monitor_wqueue);
	wakeup_source_unregister(battery->monitor_ws);
	wakeup_source_unregister(battery->cable_ws);
	wakeup_source_unregister(battery->vbus_ws);
	wakeup_source_unregister(battery->afc_ws);
	wakeup_source_unregister(battery->siop_level_ws);
	wakeup_source_unregister(battery->misc_event_ws);
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	wakeup_source_unregister(battery->ext_event_ws);
	wakeup_source_unregister(battery->wc_headroom_ws);
	wakeup_source_unregister(battery->wpc_tx_ws);
	wakeup_source_unregister(battery->tx_event_ws);
#endif
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	wakeup_source_unregister(battery->batt_data_ws);
#endif
#ifdef CONFIG_OF
	wakeup_source_unregister(battery->parse_mode_dt_ws);
#endif
	mutex_destroy(&battery->adclock);
	mutex_destroy(&battery->iolock);
	mutex_destroy(&battery->misclock);
	mutex_destroy(&battery->txeventlock);
	mutex_destroy(&battery->batt_handlelock);
	mutex_destroy(&battery->current_eventlock);
	mutex_destroy(&battery->typec_notylock);
	mutex_destroy(&battery->wclock);
	mutex_destroy(&battery->voutlock);

#ifdef CONFIG_OF
	adc_exit(battery);
#else
	for (i = 0; i < SEC_BAT_ADC_CHANNEL_NUM; i++)
		adc_exit(battery->pdata, i);
#endif
	power_supply_unregister(battery->psy_wireless);
	power_supply_unregister(battery->psy_pogo);
	power_supply_unregister(battery->psy_ac);
	power_supply_unregister(battery->psy_usb);
	power_supply_unregister(battery->psy_bat);

	kfree(battery);

	pr_info("%s: --\n", __func__);

	return 0;
}

static int sec_battery_prepare(struct device *dev)
{
	struct sec_battery_info *battery
		= dev_get_drvdata(dev);

	dev_info(battery->dev, "%s: Start\n", __func__);

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		cancel_delayed_work(&battery->polling_work);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		alarm_cancel(&battery->polling_alarm);
		break;
	default:
		break;
	}

	/* monitor_ws should be unlocked before cancel monitor_work */
	__pm_relax(battery->monitor_ws);
	cancel_delayed_work_sync(&battery->monitor_work);

	battery->polling_in_sleep = true;

	sec_bat_set_polling(battery);

	/* cancel work for polling
	 * that is set in sec_bat_set_polling()
	 * no need for polling in sleep
	 */
	if (battery->pdata->polling_type ==
		SEC_BATTERY_MONITOR_WORKQUEUE)
		cancel_delayed_work(&battery->polling_work);

	dev_info(battery->dev, "%s: End\n", __func__);

	return 0;
}

static int sec_battery_suspend(struct device *dev)
{
	return 0;
}

static int sec_battery_resume(struct device *dev)
{
	return 0;
}

static void sec_battery_complete(struct device *dev)
{
	struct sec_battery_info *battery
		= dev_get_drvdata(dev);

	dev_info(battery->dev, "%s: Start\n", __func__);

	/* cancel current alarm and reset after monitor work */
	if (battery->pdata->polling_type == SEC_BATTERY_MONITOR_ALARM)
		alarm_cancel(&battery->polling_alarm);

	__pm_stay_awake(battery->monitor_ws);
	queue_delayed_work(battery->monitor_wqueue,
		&battery->monitor_work, 0);

	dev_info(battery->dev, "%s: End\n", __func__);

	return;
}

static void sec_battery_shutdown(struct platform_device *pdev)
{
	struct sec_battery_info *battery = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	switch (battery->pdata->polling_type) {
	case SEC_BATTERY_MONITOR_WORKQUEUE:
		cancel_delayed_work(&battery->polling_work);
		break;
	case SEC_BATTERY_MONITOR_ALARM:
		alarm_cancel(&battery->polling_alarm);
		break;
	default:
		break;
	}

	cancel_delayed_work(&battery->monitor_work);
	cancel_delayed_work(&battery->cable_work);
	cancel_delayed_work(&battery->slowcharging_work);
	cancel_delayed_work(&battery->afc_work);
	cancel_delayed_work(&battery->siop_level_work);
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	cancel_delayed_work(&battery->fw_init_work);
#endif
#if IS_ENABLED(CONFIG_WIRELESS_CHARGER_MFC)
	cancel_delayed_work(&battery->wpc_tx_work);
	cancel_delayed_work(&battery->wpc_txpower_calc_work);
	cancel_delayed_work(&battery->ext_event_work);
	cancel_delayed_work(&battery->wc_headroom_work);
	cancel_delayed_work(&battery->otg_work);
	cancel_delayed_work(&battery->wc20_current_work);
#endif
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	cancel_delayed_work(&battery->batt_data_work);
#endif
	cancel_delayed_work(&battery->misc_event_work);
#ifdef CONFIG_OF
	cancel_delayed_work(&battery->parse_mode_dt_work);
#endif
	cancel_delayed_work(&battery->init_chg_work);

	pr_info("%s: --\n", __func__);
}

#ifdef CONFIG_OF
static struct of_device_id sec_battery_dt_ids[] = {
	{ .compatible = "samsung,sec-battery" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_battery_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops sec_battery_pm_ops = {
	.prepare = sec_battery_prepare,
	.suspend = sec_battery_suspend,
	.resume = sec_battery_resume,
	.complete = sec_battery_complete,
};

static struct platform_driver sec_battery_driver = {
	.driver = {
		   .name = "sec-battery",
		   .owner = THIS_MODULE,
		   .pm = &sec_battery_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = sec_battery_dt_ids,
#endif
	},
	.probe = sec_battery_probe,
	.remove = sec_battery_remove,
	.shutdown = sec_battery_shutdown,
};

static int __init sec_battery_init(void)
{
	return platform_driver_register(&sec_battery_driver);
}

static void __exit sec_battery_exit(void)
{
	platform_driver_unregister(&sec_battery_driver);
}

late_initcall(sec_battery_init);
module_exit(sec_battery_exit);

MODULE_DESCRIPTION("Samsung Battery Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
