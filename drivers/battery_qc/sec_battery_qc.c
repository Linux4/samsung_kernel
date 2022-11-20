/*
 *  sec_battery_qc.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "include/sec_battery_qc.h"
#include "include/sec_battery_sysfs_qc.h"
#include "include/sec_battery_dt_qc.h"
#include "include/sec_battery_ttf.h"
#include <linux/sec_param.h>

#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/usb_typec_manager_notifier.h>
#endif

#if defined(CONFIG_QPNP_SMB5)
#if defined(CONFIG_SEC_A90Q_PROJECT) || defined(CONFIG_SEC_A70S_PROJECT) || defined(CONFIG_SEC_A70Q_PROJECT)
#include "../power/supply/qcom_r1/smb5-lib.h"
#include "../power/supply/qcom_r1/smb5-reg.h"
#else
#include "../power/supply/qcom/smb5-lib.h"
#include "../power/supply/qcom/smb5-reg.h"
#endif
#endif
#include <linux/pmic-voter.h>
#if defined(CONFIG_AFC)
#include <linux/afc/pm6150-afc.h>
#endif

unsigned int lpcharge;
static struct sec_battery_info *g_battery;
#if defined(CONFIG_SEC_FACTORY)
int factory_mode;
#endif

char *sec_bat_status_str[POWER_SUPPLY_STATUS_FULL + 1] = {
	"Unknown",
	"Charging",
	"Discharging",
	"Not-charging",
	"Full",
};

char *sec_bat_charge_type_str[POWER_SUPPLY_CHARGE_TYPE_SLOW + 1] = {
	"Unknown",
	"None",
	"Trickle",
	"Fast",
	"Taper",
	"Slow",
};

char *sec_bat_typec_mode_str[POWER_SUPPLY_TYPEC_NON_COMPLIANT + 1] = {
	"None",
	"Rd_only",
	"Rd/Ra",
	"Ra/Ra",
	"Ra/Rd",
	"Ra_only",
	"Rp_900",
	"Rp_1500",
	"Rp_3000",
	"Non_compliant",
};

char *sec_bat_health_str[POWER_SUPPLY_HEALTH_MAX] = {
	"Unknown",
	"Good",
	"Overheat",
	"Dead",
	"OverVoltage",
	"UnspecFailure",
	"Cold",
	"WatchdogTimerExpire",
	"SafetyTimerExpire",
	"Warm",
	"Cool",
	"Hot",
	"UnderVoltage",
	"OverheatLimit",
};

char *sec_cable_type[POWER_SUPPLY_TYPE_MAX] = {
	"UNKNOWN",                 /* 0 */
	"BATTERY",                 /* 1 */
	"UPS",                     /* 2 */
	"MAINS",                   /* 3 */
	"USB",                     /* 4 */
	"USB_DCP",                 /* 5 */
	"USB_CDP",                 /* 6 */
	"USB_ACA",                 /* 7 */
	"USB_TYPE_C",              /* 8 */
	"USB_PD",                  /* 9 */
	"USB_PD_DRP",              /* 10 */
	"APPLE_BRICK_ID",          /* 11 */
	"USB_HVDCP",               /* 12 */
	"USB_HVDCP_3",             /* 13 */
	"WIRELESS",                /* 14 */
	"USB_FLOAT",               /* 15 */
	"BMS",                     /* 16 */
	"PARALLEL",                /* 17 */
	"MAIN",                    /* 18 */
	"WIPOWER",                 /* 19 */
	"UFP",                     /* 20 */
	"DFP",                     /* 21 */
	"CHARGE_PUMP",             /* 22 */
	"POWER_SHARING",           /* 23 */
	"OTG",                     /* 24 */
#if defined(CONFIG_AFC)
	"AFC",                     /* 25 */
#endif
};

static int sec_bat_is_lpm_check(char *str)
{
	if (strncmp(str, "charger", 7) == 0)
		lpcharge = 1;
	else
		lpcharge = 0;
	pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);

	return lpcharge;
}
__setup("androidboot.mode=", sec_bat_is_lpm_check);

#if defined(CONFIG_SEC_FACTORY)
static int sec_bat_get_factory_mode(char *val)
{
	factory_mode = strncmp(val, "1", 1) ? 0 : 1;
	pr_info("%s, factory_mode:%d\n", __func__, factory_mode);
	return 1;
}
__setup("factory_mode=", sec_bat_get_factory_mode);
#endif

struct sec_battery_info * get_sec_battery(void)
{
	return g_battery;
}

static int get_cable_type(struct sec_battery_info *battery)
{
	int rc;
	union power_supply_propval value = {0, };

	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_REAL_TYPE, &value);
	if (rc < 0) {
		pr_err("%s: Fail to get usb real_type_prop (%d=>%d)\n", __func__, POWER_SUPPLY_PROP_REAL_TYPE, rc);
		return POWER_SUPPLY_TYPE_UNKNOWN;
	} else { 
		//pr_info("%s: cable_type(%d)\n", __func__, value.intval);
		ttf_work_start(battery);
		return value.intval;
	}
}

int get_usb_voltage_now(struct sec_battery_info *battery)
{
	int rc;
	union power_supply_propval value = {0, };

	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
	if (rc < 0) {
		pr_err("%s: Fail to get usb voltage_now (%d=>%d)\n", __func__, POWER_SUPPLY_PROP_VOLTAGE_NOW, rc);
		return 0;
	} else { 
		pr_info("%s: voltage_now (%d)\n", __func__, value.intval);
		return value.intval;
	}
}

int get_pd_active(struct sec_battery_info *battery)
{
	int rc;
	union power_supply_propval value = {0, };

	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_PD_ACTIVE, &value);
	if (rc < 0) {
		pr_err("%s: Fail to get pd active (%d=>%d)\n", __func__, POWER_SUPPLY_PROP_PD_ACTIVE, rc);
		return 0;
	} else { 
		pr_info("%s: pd active (%d)\n", __func__, value.intval);
		return value.intval;
	}
}

void disable_charge_pump(struct smb_charger *chg, bool disable, const char *client_str, int sleep_time)
{
	if (disable) {
		if (!chg->cp_disable_votable)
			chg->cp_disable_votable = find_votable("CP_DISABLE");
		if (chg->cp_disable_votable)
			vote(chg->cp_disable_votable, client_str, disable, 0);
		else
			pr_err("Couldn't find chg->cp_disable_votable\n");

		if (!chg->smb_override_votable)
			chg->smb_override_votable = find_votable("SMB_EN_OVERRIDE");
		if (chg->smb_override_votable)
			vote(chg->smb_override_votable, client_str, disable, 0); 
		else
			pr_err("Couldn't find chg->smb_override_votable\n");
	} else {
		if (!chg->smb_override_votable)
			chg->smb_override_votable = find_votable("SMB_EN_OVERRIDE");
		if (chg->smb_override_votable)
			vote(chg->smb_override_votable, client_str, disable, 0); 
		else
			pr_err("Couldn't find chg->smb_override_votable\n");

		/* Delay for 1000ms (1 second) after disableing smb_override_votable and
		   before voting to enable SMB1390 via cp_disable_votable */
		if (sleep_time)
			msleep(sleep_time);

		if (!chg->cp_disable_votable)
			chg->cp_disable_votable = find_votable("CP_DISABLE");
		if (chg->cp_disable_votable)
			vote(chg->cp_disable_votable, client_str, disable, 0);
		else
			pr_err("Couldn't find chg->cp_disable_votable\n");
	}
}

void force_vbus_voltage_QC30(struct smb_charger *chg, int val)
{
	union power_supply_propval value = {0, };

	if ((val == MICRO_5V) && !chg->forced_5v_qc30) {
		value.intval = 1;
		power_supply_set_property(chg->usb_main_psy, POWER_SUPPLY_PROP_FLASH_ACTIVE, &value);
		chg->forced_5v_qc30 = true;
	} else if ((val == MICRO_12V) && chg->forced_5v_qc30) {
		value.intval = 0;
		power_supply_set_property(chg->usb_main_psy, POWER_SUPPLY_PROP_FLASH_ACTIVE, &value);
		chg->forced_5v_qc30 = false;
	}
	return;
}

void sec_bat_set_cable_type_current(int real_cable_type, bool attached)
{
	struct sec_battery_info *battery = get_sec_battery();
	struct smb_charger *chg;
	u32 input_current = 0, charging_current = 0;
	int jig_type = 0;

	if (!battery) {
		pr_info("%s: battery is NULL\n", __func__);
		return;
	}

	chg = power_supply_get_drvdata(battery->psy_bat);

	if (attached) {
		jig_type = get_rid_type();
		/* jig cable */
		if((jig_type >= 2 /* RID_ADC_56K */) && (jig_type <= 6 /* RID_ADC_619K */)) {
#if defined(CONFIG_BATTERY_CISD)
			battery->skip_cisd = true;
#endif
			if (jig_type == 6 /* RID_ADC_619K */) {
				input_current = battery->charging_current[POWER_SUPPLY_TYPE_USB_DCP].input_current_limit;
				charging_current = battery->charging_current[POWER_SUPPLY_TYPE_USB_DCP].fast_charging_current;
				vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
				vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
				vote(chg->usb_icl_votable, SEC_BATTERY_CABLE_TYPE_VOTER, true, (input_current * 1000));
			}
		}
		/* normal cable */
		else {
			if (real_cable_type == POWER_SUPPLY_TYPE_USB_HVDCP_3)
				disable_charge_pump(chg, true, SEC_BATTERY_QC3P0_VOTER, 0);

			input_current = battery->charging_current[real_cable_type].input_current_limit;
			charging_current = battery->charging_current[real_cable_type].fast_charging_current ;

			if ((real_cable_type == POWER_SUPPLY_TYPE_USB_PD) && (chg->pd_active == POWER_SUPPLY_PD_ACTIVE)) {
#if defined(CONFIG_AFC)
				input_current = battery->charging_current[POWER_SUPPLY_TYPE_AFC].input_current_limit;
				charging_current = battery->charging_current[POWER_SUPPLY_TYPE_AFC].fast_charging_current ;
#else
				input_current = battery->charging_current[POWER_SUPPLY_TYPE_USB_HVDCP].input_current_limit;
				charging_current = battery->charging_current[POWER_SUPPLY_TYPE_USB_HVDCP].fast_charging_current;
#endif
			}
		}
		pr_info("%s: cable(%s) PD(%d) attached, icl(%d), fcc(%d)\n", __func__, 
			sec_cable_type[real_cable_type], chg->pd_active, input_current, charging_current);

		vote(chg->fcc_votable, SEC_BATTERY_CABLE_TYPE_VOTER, true, (charging_current * 1000));
	}
	else {
		pr_info("%s: cable(%s) PD(%d) detached\n", __func__, sec_cable_type[real_cable_type], chg->pd_active);

		vote(chg->usb_icl_votable, SEC_BATTERY_CABLE_TYPE_VOTER, false, 0);
		vote(chg->fcc_votable, SEC_BATTERY_CABLE_TYPE_VOTER, false, 0);
		disable_charge_pump(chg, false, SEC_BATTERY_QC3P0_VOTER, 0);
	}
}

static void sec_bat_siop_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, siop_work.work);

	int voltage_now;
	int current_val = 0;
	int siop_level = battery->siop_level;
	struct smb_charger *chg;
	union power_supply_propval value = {0, };
	int cable_type = 0;
	
	chg = power_supply_get_drvdata(battery->psy_bat);
	cable_type = get_cable_type(battery);

	if (siop_level < 100) {
		disable_charge_pump(chg, true, SEC_BATTERY_SIOP_VOTER, 0);

		voltage_now = get_usb_voltage_now(battery);
#if defined(CONFIG_DIRECT_CHARGING)
		if (get_pd_active(battery) == POWER_SUPPLY_PD_PPS_ACTIVE) {
			pr_info("%s : siop level: %d, cable_type: %d, votlage: %d\n",__func__, siop_level, cable_type, voltage_now);
			current_val = battery->siop_input_limit_current * 1000;

#if defined(CONFIG_SEC_A90Q_PROJECT)
			if (chg->pdp_limit_w > 0) {
				value.intval = chg->default_pdp_limit_w - 1;
				power_supply_set_property(battery->psy_usb,
					POWER_SUPPLY_PROP_PDP_LIMIT_W, &value);
			}
#endif
		} else if (voltage_now >= 7500000) { //High voltage charger
#else
		if (voltage_now >= 7500000) { //High voltage charger
#endif
			pr_info("%s : siop level: %d, cable_type: %d, votlage: %d\n",__func__, siop_level, cable_type, voltage_now);
			if (battery->minimum_hv_input_current_by_siop_10 > 0 && siop_level <=10)
				current_val = battery->minimum_hv_input_current_by_siop_10 * 1000;
			else
				current_val =  battery->siop_hv_input_limit_current * 1000;
		} else { //Normal charger
			current_val = battery->siop_input_limit_current * 1000;
		}

#if defined(CONFIG_AFC_SET_VOLTAGE_BY_SIOP)
		if(chg->afc_sts == AFC_9V)
			afc_set_voltage(SET_5V);
#endif
		value.intval = MICRO_5V;
		if (cable_type == POWER_SUPPLY_TYPE_USB_HVDCP_3)
			force_vbus_voltage_QC30(chg, value.intval);
		else
			power_supply_set_property(battery->psy_usb,
				POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT, &value);

		vote(chg->usb_icl_votable, SEC_BATTERY_SIOP_VOTER, true, current_val);
	} else {
		vote(chg->usb_icl_votable, SEC_BATTERY_SIOP_VOTER, false, 0);
		if(!chg->vbus_chg_by_full) {
#if defined(CONFIG_AFC_SET_VOLTAGE_BY_SIOP)
			if (chg->afc_sts == AFC_5V)
				afc_set_voltage(SET_9V);
#endif

#if !defined(CONFIG_SEC_FACTORY)
			if (battery->store_mode)
				value.intval = MICRO_5V;
			else
#endif
				value.intval = MICRO_12V;
			if (cable_type == POWER_SUPPLY_TYPE_USB_HVDCP_3)
				force_vbus_voltage_QC30(chg, value.intval);
			else
				power_supply_set_property(battery->psy_usb,
						POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT, &value);
		}
		disable_charge_pump(chg, false, SEC_BATTERY_SIOP_VOTER, 1000);

#if defined(CONFIG_DIRECT_CHARGING)
#if defined(CONFIG_SEC_A90Q_PROJECT)
		if ((get_pd_active(battery) == POWER_SUPPLY_PD_PPS_ACTIVE)
				&& (chg->pdp_limit_w > 0)) {
			value.intval = chg->default_pdp_limit_w;
			power_supply_set_property(battery->psy_usb,
				POWER_SUPPLY_PROP_PDP_LIMIT_W, &value);
		}
#endif
#endif
	}
	pr_info("%s : set current by siop level: %d, current: %d, afc_sts:%d\n",__func__, siop_level, current_val, chg->afc_sts);
	wake_unlock(&battery->siop_wake_lock);
}

void sec_bat_run_siop_work(void)
{
	struct sec_battery_info *battery = get_sec_battery();

	if (!battery) {
		pr_info("%s : null\n",__func__);
		return;
	}
	wake_lock(&battery->siop_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->siop_work, 0);
}

int get_battery_health(void)
{
	struct sec_battery_info *battery = get_sec_battery();

	if (battery)
		return battery->batt_health;

	return POWER_SUPPLY_HEALTH_UNKNOWN;
}

void sec_bat_get_ps_ready(struct sec_battery_info *battery)
{
	battery->pdic_ps_rdy = get_ps_ready_status();
	pr_info("%s : CHECK_PS_READY=%d\n",__func__, battery->pdic_ps_rdy);

	return;
}

bool sec_bat_get_wdt_control(void)
{
	struct sec_battery_info *battery = get_sec_battery();
	if (battery)
		return battery->wdt_kick_disable;
	return false;
}

void sec_bat_set_usb_configuration(int config)
{
	switch (config) {
	case USB_CURRENT_UNCONFIGURED:
		sec_bat_set_current_event(SEC_BAT_CURRENT_EVENT_USB_100MA,
				(SEC_BAT_CURRENT_EVENT_USB_100MA | SEC_BAT_CURRENT_EVENT_USB_SUPER));
		break;
	case USB_CURRENT_HIGH_SPEED:
		sec_bat_set_misc_event(0, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE);
		sec_bat_set_current_event(0, (SEC_BAT_CURRENT_EVENT_USB_100MA | SEC_BAT_CURRENT_EVENT_USB_SUPER));
		break;
	case USB_CURRENT_SUPER_SPEED:
		sec_bat_set_misc_event(0, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE);
		sec_bat_set_current_event(SEC_BAT_CURRENT_EVENT_USB_SUPER,
				(SEC_BAT_CURRENT_EVENT_USB_100MA | SEC_BAT_CURRENT_EVENT_USB_SUPER));
		break;
	default:
		break;
	}
	pr_info("%s: usb configured(%d)\n", __func__, config);
}

void sec_bat_set_current_event(unsigned int current_event_val, unsigned int current_event_mask)
{
	struct sec_battery_info *battery = get_sec_battery();
	unsigned int temp = 0;

	if (!battery) {
		pr_info("%s: battery is NULL\n", __func__);
		return;
	}

	temp = battery->current_event;

	mutex_lock(&battery->current_eventlock);
	battery->current_event &= ~current_event_mask;
	battery->current_event |= current_event_val;

	pr_info("%s: current event before(0x%x), after(0x%x)\n",
		__func__, temp, battery->current_event);
	mutex_unlock(&battery->current_eventlock);
}

void sec_bat_set_misc_event(unsigned int misc_event_val, unsigned int misc_event_mask)
{
	struct sec_battery_info *battery = get_sec_battery();
	unsigned int temp = 0;
	
	if (!battery) {
		pr_info("%s: battery is NULL\n", __func__);
		return;
	}

	temp = battery->misc_event;

	mutex_lock(&battery->misc_eventlock);
	battery->misc_event &= ~misc_event_mask;
	battery->misc_event |= misc_event_val;

	pr_info("%s: misc event before(0x%x), after(0x%x)\n",
		__func__, temp, battery->misc_event);

	mutex_unlock(&battery->misc_eventlock);
}

void sec_bat_check_mix_temp(void)
{
	struct sec_battery_info *battery = get_sec_battery();
	struct smb_charger *chg;
	int input_current = 0;
	int max_input_current = 0;
	chg = power_supply_get_drvdata(battery->psy_bat);

	if (is_nocharge_type(battery->cable_real_type) || chg->charging_test_mode) {
		if (battery->mix_limit) {
			pr_info("%s: mix temp recovery by CHG OFF.\n", __func__);
			vote(chg->usb_icl_votable, SEC_BATTERY_MIX_TEMP_VOTER, false, 0);
			disable_charge_pump(chg, false, SEC_BATTERY_MIX_TEMP_VOTER, 1000);
			battery->mix_limit = false;
		}
		return;
	}

	if (battery->siop_level >= 100) {
		if ((battery->mix_limit) && (battery->batt_temp <= battery->mix_temp_recovery)){
			pr_info("%s: mix temp recovery. batt temp(%d), die_temp(%d)\n", __func__,
					battery->batt_temp, battery->die_temp);
			vote(chg->usb_icl_votable, SEC_BATTERY_MIX_TEMP_VOTER, false, 0);
			disable_charge_pump(chg, false, SEC_BATTERY_MIX_TEMP_VOTER, 1000);
			battery->mix_limit = false;
		} else if ((!battery->mix_limit) &&
				(battery->batt_temp > battery->mix_temp_batt_threshold) &&
				(battery->die_temp > battery->mix_temp_die_threshold)) {
			disable_charge_pump(chg, true, SEC_BATTERY_MIX_TEMP_VOTER, 0);

			max_input_current = battery->full_check_current_1st + 50;	
			input_current = (battery->float_voltage * max_input_current) / ((battery->vbus_level * 9) / 10) ;
			if (input_current > max_input_current)
				input_current = max_input_current;
			input_current *= 1000; /* change uA for QC driver */

			pr_info("%s: mix temp trigger. batt temp(%d), die_temp(%d), icl-uA(%d)\n",
					__func__, battery->batt_temp, battery->die_temp, input_current);
			vote(chg->usb_icl_votable, SEC_BATTERY_MIX_TEMP_VOTER, true, input_current);
			battery->mix_limit = true;
		} else {
			pr_info("%s: mix limit(%d), batt temp(%d), die_temp(%d)\n",
					__func__, battery->mix_limit, battery->batt_temp, battery->die_temp);
		}
	} else { /* battery->siop_level < 100 */
		if (battery->mix_limit) {
			pr_info("%s: mix temp recovery by SIOP LV(%d). batt temp(%d), die_temp(%d)\n",
					__func__, battery->siop_level, battery->batt_temp, battery->die_temp);
			vote(chg->usb_icl_votable, SEC_BATTERY_MIX_TEMP_VOTER, false, 0);
			disable_charge_pump(chg, false, SEC_BATTERY_MIX_TEMP_VOTER, 1000);
			battery->mix_limit = false;
		}
	}
	return;
}

int is_dc_wire_type(struct sec_battery_info *battery)
{
	int ret = 0, is_dc = 0;
	union power_supply_propval value = {0, };

#if !defined(CONFIG_DIRECT_CHARGING)
	return 0;
#endif
	ret = power_supply_get_property(battery->psy_usb, POWER_SUPPLY_PROP_PD_ACTIVE, &value);
	if(ret < 0)
		pr_info("%s: Fail to get usb pd_active_prop (%d=>%d)\n", __func__, POWER_SUPPLY_PROP_PD_ACTIVE,ret);
	
	is_dc = value.intval;
	
	if (is_dc == POWER_SUPPLY_PD_PPS_ACTIVE)
		return 1;

	return 0;
}

static void sec_bat_check_hv_temp(void)
{
	struct sec_battery_info *battery = get_sec_battery();
	struct smb_charger *chg;
	chg = power_supply_get_drvdata(battery->psy_bat);

	if (is_nocharge_type(battery->cable_real_type) || is_dc_wire_type(battery) || chg->charging_test_mode) {
		if (battery->chg_limit) {
			pr_info("%s: hv temp recovery by CHG OFF.\n", __func__);
			vote(chg->fcc_votable, SEC_BATTERY_HV_VOTER, false, 0);
			vote(chg->usb_icl_votable, SEC_BATTERY_HV_VOTER, false, 0);
			battery->chg_limit = false;
		}
		return;
	}

	if (battery->siop_level >= 100) {
		if ((!battery->chg_limit && (battery->vbus_level >= 7500) && (battery->die_temp >= battery->chg_high_temp)) ||
			(battery->chg_limit && (battery->vbus_level >= 7500) && (battery->die_temp > battery->chg_high_temp_recovery))) {
			pr_info("%s: hv temp trigger. die_temp(%d)\n", __func__, battery->die_temp);
			vote(chg->fcc_votable, SEC_BATTERY_HV_VOTER, true, (battery->chg_charging_limit_current * 1000));
			vote(chg->usb_icl_votable, SEC_BATTERY_HV_VOTER, true, (battery->chg_input_limit_current * 1000));
			battery->chg_limit = true;
		} else if (battery->chg_limit && (battery->die_temp <= battery->chg_high_temp_recovery)) {
			pr_info("%s: hv temp recovery. die_temp(%d)\n", __func__, battery->die_temp);
			vote(chg->fcc_votable, SEC_BATTERY_HV_VOTER, false, 0);
			vote(chg->usb_icl_votable, SEC_BATTERY_HV_VOTER, false, 0);
			battery->chg_limit = false;
		} else {
			pr_info("%s: chg limit(%d), die_temp(%d)\n",
					__func__, battery->chg_limit, battery->die_temp);
		}
	} else {
		if (battery->chg_limit) {
			pr_info("%s: hv temp recovery by SIOP LV(%d). die_temp(%d)\n",
					__func__, battery->siop_level, battery->die_temp);
			vote(chg->usb_icl_votable, SEC_BATTERY_HV_VOTER, false, 0);
			vote(chg->fcc_votable, SEC_BATTERY_HV_VOTER, false, 0);
			battery->chg_limit = false;
		}
	}
	return;
}

#if defined(CONFIG_DIRECT_CHARGING)
static void sec_bat_check_dc_temp(void)
{
	struct sec_battery_info *battery = get_sec_battery();
	struct smb_charger *chg;
	chg = power_supply_get_drvdata(battery->psy_bat);

	if (!is_dc_wire_type(battery) || chg->charging_test_mode) {
		if (battery->dc_chg_limit) {
			pr_info("%s: dc temp recovery by CHG OFF.\n", __func__);
			vote(chg->usb_icl_votable, SEC_BATTERY_DC_TEMP_VOTER, false, 0);
			battery->dc_chg_limit = false;
			if (chg->cp_disable_votable)
				vote(chg->cp_disable_votable, SEC_BATTERY_DC_TEMP_VOTER, false, 0);
		}
		return;
	}

	if (battery->siop_level >= 100) {
		if ((!battery->dc_chg_limit && (battery->die_temp >= battery->dc_chg_high_temp)) ||
			(battery->dc_chg_limit && (battery->die_temp > battery->dc_chg_high_temp_recovery))) {
			pr_info("%s: dc temp trigger. die_temp(%d)\n", __func__, battery->die_temp);
			if (chg->cp_disable_votable)
				vote(chg->cp_disable_votable, SEC_BATTERY_DC_TEMP_VOTER, true, 0);
			vote(chg->usb_icl_votable, SEC_BATTERY_DC_TEMP_VOTER, true, (battery->dc_chg_input_limit_current * 1000));
			battery->dc_chg_limit = true;
		} else if (battery->dc_chg_limit && (battery->die_temp <= battery->dc_chg_high_temp_recovery)) {
			pr_info("%s: dc temp recovery. die_temp(%d)\n", __func__, battery->die_temp);
			vote(chg->usb_icl_votable, SEC_BATTERY_DC_TEMP_VOTER, false, 0);
			battery->dc_chg_limit = false;
			if (chg->cp_disable_votable)
				vote(chg->cp_disable_votable, SEC_BATTERY_DC_TEMP_VOTER, false, 0);
		} else {
			pr_info("%s: dc_chg limit(%d), die_temp(%d)\n",
					__func__, battery->dc_chg_limit, battery->die_temp);
		}
	} else {
		if (battery->dc_chg_limit) {
			pr_info("%s: dc temp recovery by SIOP LV(%d). die_temp(%d)\n",
					__func__, battery->siop_level, battery->die_temp);
			vote(chg->usb_icl_votable, SEC_BATTERY_DC_TEMP_VOTER, false, 0);
			battery->dc_chg_limit = false;
			if (chg->cp_disable_votable)
				vote(chg->cp_disable_votable, SEC_BATTERY_DC_TEMP_VOTER, false, 0);		
		}
	}
	return;
}

#if defined(CONFIG_SEC_A90Q_PROJECT)
static void sec_bat_check_pdp_limit_w(void)
{
	struct sec_battery_info *battery = get_sec_battery();
	struct smb_charger *chg;
	union power_supply_propval val = {0, };
	int cp_switcher_en_status = 0; 
	s64 time_now, time_diff;

	chg = power_supply_get_drvdata(battery->psy_bat);

	power_supply_get_property(battery->psy_slave,
		POWER_SUPPLY_PROP_CP_SWITCHER_EN, &val);
	cp_switcher_en_status = val.intval;

	// Only do PDP power limit step ups if CP_SWITCHER_EN is '1' 
	if (cp_switcher_en_status) { 
		if (chg->pdp_limit_w >= chg->final_pdp_limit_w)
			return;

		time_now = ktime_to_ms(ktime_get_boottime());
		time_diff = time_now - chg->pdp_limit_w_set_time;

		pr_info("[%s] t_now : %lld , t_set_limit_w : %lld , diff_ms : %lld\n", __func__,
			time_now, chg->pdp_limit_w_set_time, time_diff);

		if (time_diff > (battery->interval_pdp_limit_w * 1000)) {
			val.intval = chg->pdp_limit_w + 1;
			power_supply_set_property(battery->psy_usb,
				POWER_SUPPLY_PROP_PDP_LIMIT_W, &val);
		}		
	}
}
#endif
#endif

static void sec_bat_monitor_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, monitor_work.work);	
	struct smb_charger *chg;
	union power_supply_propval val = {0, };
	int rc = 0, full_condi_soc = 0;
	const char *usb_icl_voter, *fcc_voter = NULL;
	static int term_cur_cnt = 0;
	int recharge_vbat = 0, recharge_vbat_fixed = 0;
	
	chg = power_supply_get_drvdata(battery->psy_bat);

	battery->cable_real_type = get_cable_type(battery);

	rc = power_supply_get_property(battery->psy_bms,
			POWER_SUPPLY_PROP_TEMP, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get temp prop. rc=%d\n", __func__, rc);
		battery->batt_temp = 300;
	} else {
		battery->batt_temp = val.intval;
	}

	rc = power_supply_get_property(battery->psy_bat,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BATT_CHG_TEMP, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get temp prop. rc=%d\n", __func__, rc);
		battery->die_temp = 300;
	} else {
		battery->die_temp = val.intval;
	}

	if (battery->psy_slave) {
		rc = power_supply_get_property(battery->psy_slave,
				POWER_SUPPLY_PROP_CP_DIE_TEMP, &val);
		if (rc < 0) {
			battery->cp_die_temp = 300;
		} else {
			battery->cp_die_temp = val.intval;
		}
	}

	battery->vbus_level = get_usb_voltage_now(battery) / 1000; /* change mV for simple calculation */

	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_CONNECTOR_TYPE, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get connector type prop. rc=%d\n", __func__, rc);
		battery->conn_type = POWER_SUPPLY_CONNECTOR_MICRO_USB;
	} else {
		battery->conn_type = val.intval;
	}

	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_TYPEC_MODE, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get typec mode prop. rc=%d\n", __func__, rc);
		battery->typec_mode = POWER_SUPPLY_TYPEC_NONE;
	} else {
		battery->typec_mode = val.intval;
	}

	rc = power_supply_get_property(battery->psy_usb_main,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get float voltage prop. rc=%d\n", __func__, rc);
		battery->float_voltage = 4350;
	} else {
		battery->float_voltage = val.intval / 1000; /* change mV for simple calculation */
	}

	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_STATUS, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get status prop. rc=%d\n", __func__, rc);
		battery->status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else {
		battery->status = val.intval;
	}

	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_HEALTH, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get health prop. rc=%d\n", __func__, rc);
		battery->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	} else {
		battery->batt_health = val.intval;
	}

	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_DIE_HEALTH, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get die health prop. rc=%d\n", __func__, rc);
		battery->die_health = POWER_SUPPLY_HEALTH_GOOD;
	} else {
		battery->die_health = val.intval;
	}

	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get charge type prop. rc=%d\n", __func__, rc);
		battery->charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	} else {
		battery->charge_type = val.intval;
	}

	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_CAPACITY, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get capacity prop. rc=%d\n", __func__, rc);
		battery->soc = 50;
	} else {
		battery->soc = val.intval;
	}

	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get voltage now prop. rc=%d\n", __func__, rc);
		battery->v_now = 0;
	} else {
		battery->v_now = val.intval / 1000; /* uV -> mV */
	}

	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get input current now prop. rc=%d\n", __func__, rc);
		battery->i_in = 0;
	} else {
		battery->i_in = val.intval / 1000; /* uA -> mA */
	}

	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get current now prop. rc=%d\n", __func__, rc);
		battery->i_now = 0;
	} else {
		battery->i_now = val.intval / 1000; /* uA -> mA */
	}

	rc = power_supply_get_property(battery->psy_usb_main,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get constant charge current max prop. rc=%d\n", __func__, rc);
		battery->i_chg = 0;
	} else {
		battery->i_chg = val.intval / 1000; /* uA -> mA */
	}

	rc = power_supply_get_property(battery->psy_usb,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_SW_CURRENT_MAX, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get sw current max prop. rc=%d\n", __func__, rc);
		battery->i_max = 500;
	} else {
		battery->i_max = val.intval / 1000; /* uA -> mA */
	}

	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get hw current max prop. rc=%d\n", __func__, rc);
		battery->hw_max = 500;
	} else {
		battery->hw_max = val.intval / 1000; /* uA -> mA */
	}

	rc = power_supply_get_property(battery->psy_bms,
			POWER_SUPPLY_PROP_VOLTAGE_OCV, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get voltage ocv prop. rc=%d\n", __func__, rc);
		battery->ocv = 0;
	} else {
		battery->ocv = val.intval / 1000; /* uV -> mV */
	}

	if (battery->batt_temp >= TEMP_HIGHLIMIT_THRESHOLD && !battery->batt_temp_overheat_check) {
		vote(chg->usb_icl_votable, SEC_BATTERY_OVERHEATLIMIT_VOTER, true, 0);
		battery->batt_temp_overheat_check = true;
		sec_bat_set_current_event(SEC_BAT_CURRENT_EVENT_HIGH_TEMP_LIMIT,
				SEC_BAT_CURRENT_EVENT_HIGH_TEMP_LIMIT);
		pr_info("%s: high temp limit trigger. batt_temp(%d)\n", __func__, battery->batt_temp);
	#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.data[CISD_DATA_USB_OVERHEAT_CHARGING]++;
		battery->cisd.data[CISD_DATA_USB_OVERHEAT_CHARGING_PER_DAY]++;
		battery->cisd.data[CISD_DATA_BUCK_OFF]++;
		battery->cisd.data[CISD_DATA_BUCK_OFF_PER_DAY]++;
	#endif	
	} else if (battery->batt_temp <= TEMP_HIGHLINMIT_RECOVERY && battery->batt_temp_overheat_check) {
		vote(chg->usb_icl_votable, SEC_BATTERY_OVERHEATLIMIT_VOTER, false, 0);
		battery->batt_temp_overheat_check = false;
		sec_bat_set_current_event(0, 
				SEC_BAT_CURRENT_EVENT_HIGH_TEMP_LIMIT);
		pr_info("%s: high temp limit recovery. batt_temp(%d)\n", __func__, battery->batt_temp);
	}

	/* time to full check */
	sec_bat_calc_time_to_full(battery);

#if defined(CONFIG_BATTERY_CISD)
	sec_bat_cisd_check(battery);
#endif
#if defined(CONFIG_SEC_A60Q_PROJECT) || defined(CONFIG_SEC_M40_PROJECT)
	if (((get_pd_active(battery) == POWER_SUPPLY_PD_PPS_ACTIVE) 
				|| (battery->cable_real_type == POWER_SUPPLY_TYPE_USB_HVDCP_3)) && battery->siop_level < 100)
		sec_bat_run_siop_work();
#endif
	rc = power_supply_get_property(battery->psy_bms,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_FULL_CONDITION_SOC, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get full condition soc. rc=%d\n", __func__, rc);
		full_condi_soc = 93;
	} else {
		full_condi_soc = val.intval;
	}

	rc = power_supply_get_property(battery->psy_usb,
			(enum power_supply_property)POWER_SUPPLY_EXT_FIXED_RECHARGE_VBAT, &val);
	recharge_vbat_fixed = val.intval;

	rc = power_supply_get_property(battery->psy_bat,
				POWER_SUPPLY_PROP_RECHARGE_VBAT, &val);
	recharge_vbat = val.intval;

	/* check 1st termination current */
	if ((battery->soc >= full_condi_soc) && (battery->status == POWER_SUPPLY_STATUS_CHARGING)
		&& (battery->i_now <= battery->full_check_current_1st) && (battery->i_now > 0) && (battery->v_now >=  recharge_vbat_fixed)) {
		pr_info("%s: (soc >= %d) && (0 < i_now <= %d) : count(%d)\n", __func__, full_condi_soc, battery->full_check_current_1st, term_cur_cnt++);

		if (term_cur_cnt == 3) {
			pr_info("%s: reach to 1st_term_curr! change soc to 100\n", __func__);
			val.intval = 100;
			power_supply_set_property(battery->psy_bms,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_SOC_RESCALE, &val);
		}
	}
	else {
		term_cur_cnt = 0;
	}

	/* keep 100% when low swelling mode before re-charging */
	if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP_MODE)
		recharge_vbat_fixed = recharge_vbat;

	if ((battery->status == POWER_SUPPLY_STATUS_FULL) && (battery->v_now < (recharge_vbat_fixed - 50))
		&& ((battery->i_now < 0) || (battery->current_event & SEC_BAT_CURRENT_EVENT_SWELLING_MODE))) {
		pr_info("%s: change recharging UI\n", __func__);

		val.intval = 0;
		power_supply_set_property(battery->psy_bms,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_SOC_RESCALE, &val);

		val.intval = 0;
		power_supply_set_property(battery->psy_bms,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHARGE_FULL, &val);
	}

	usb_icl_voter = get_effective_client(chg->usb_icl_votable);
	fcc_voter = get_effective_client(chg->fcc_votable);
	
	pr_info("%s: Status(%s, %s), Health(%s), Cable(%s, %s, %s, %d), Afc Status(%d)\n", __func__,
			sec_bat_status_str[battery->status], sec_bat_charge_type_str[battery->charge_type],
			sec_bat_health_str[battery->batt_health], sec_cable_type[battery->cable_real_type],
			battery->conn_type ? "MICRO USB" : "TYPE C", sec_bat_typec_mode_str[battery->typec_mode],
			get_rid_type(), chg->afc_sts);

	pr_info("%s: Soc(%d), Vnow(%d), Ocv(%d), Inow(%d, %d), Imax(sw(%d), hw(%d)), Ichg(sw(%d)), Fv(%d), RechgV(%d)\n", __func__,
			battery->soc, battery->v_now, battery->ocv, battery->i_in, battery->i_now,
			battery->i_max, battery->hw_max, battery->i_chg, battery->float_voltage, recharge_vbat);
	pr_info("%s: Tbat(%d), Tdie(%d), Tcpdie(%d), Vbus(%d), Cycle(%d), Siop(%d), Store(%d), Slate(%d), Misc(0x%x), Current(0x%x)\n", __func__,
			battery->batt_temp, battery->die_temp, battery->cp_die_temp, battery->vbus_level, battery->batt_cycle,
			battery->siop_level, battery->store_mode, battery->slate_mode,
			battery->misc_event, battery->current_event);
	pr_info("%s: usb_icl_votable(%s): %d, fcc_votable(%s): %d\n", __func__,
			usb_icl_voter ? usb_icl_voter : "None", get_effective_result(chg->usb_icl_votable),
			fcc_voter ? fcc_voter : "None", get_effective_result(chg->fcc_votable));
	get_setanytype_effective_client(chg->chg_disable_votable);
	if(!!chg->cp_disable_votable)
		get_setanytype_effective_client(chg->cp_disable_votable);

	battery->prev_charge_type = battery->charge_type;
	battery->prev_status = battery->status;
	battery->prev_current_event = battery->current_event;
	battery->prev_batt_health = battery->batt_health;
	sec_bat_check_mix_temp();
	sec_bat_check_hv_temp();
#if defined(CONFIG_DIRECT_CHARGING)
	if (battery->check_dc_chg_lmit)
		sec_bat_check_dc_temp();

#if defined(CONFIG_SEC_A90Q_PROJECT)
	if (chg->pdp_limit_w > 0)
		sec_bat_check_pdp_limit_w();
#endif
#endif

	wake_unlock(&battery->monitor_wake_lock);

	return;
}

void sec_bat_run_monitor_work(void) 
{
	struct sec_battery_info *battery = get_sec_battery();

	if (battery != NULL){
		wake_lock(&battery->monitor_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	}
	else
		pr_info("%s: SKIP: SEC Battery driver is not loaded.\n\n", __func__);

	return;
}

static void sec_bat_slate_work(struct work_struct *work)
{
	struct sec_battery_info *battery = container_of(work,
				struct sec_battery_info, slate_work.work);
	struct smb_charger *chg;
	int mode;
	chg = power_supply_get_drvdata(battery->psy_bat);

	mode = is_slate_mode(battery);
	pr_info("%s : %d\n",__func__, mode);
	if (!mode)
		disable_charge_pump(chg, false, SEC_BATTERY_SLATE_MODE_VOTER, 1000);

	wake_unlock(&battery->slate_wake_lock);
}
#define CP_VOTER		"CP_VOTER"
#define SRC_VOTER		"SRC_VOTER"
#define SOC_LEVEL_VOTER		"SOC_LEVEL_VOTER"
extern void set_pd_hard_reset(int val);
void sec_bat_set_slate_mode(struct sec_battery_info *battery)
{
	struct smb_charger *chg;
	union power_supply_propval val = {0, };

	chg = power_supply_get_drvdata(battery->psy_bat);

	if(is_slate_mode(battery)) {
		cancel_delayed_work_sync(&battery->slate_work);
		wake_unlock(&battery->slate_wake_lock);
		disable_charge_pump(chg, true, SEC_BATTERY_SLATE_MODE_VOTER, 0);

		val.intval = MICRO_5V;
		if (get_cable_type(battery) == POWER_SUPPLY_TYPE_USB_HVDCP_3)
			force_vbus_voltage_QC30(chg, val.intval);
		else
			power_supply_set_property(battery->psy_usb,
				POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT, &val);

		vote(chg->chg_disable_votable, SEC_BATTERY_SLATE_MODE_VOTER, true, 0);
		vote(chg->usb_icl_votable, SEC_BATTERY_SLATE_MODE_VOTER, true, 0);

		if (chg->cp_disable_votable) {
			vote(chg->cp_disable_votable, SRC_VOTER, false, 0);
			vote(chg->cp_disable_votable, TAPER_END_VOTER, false, 0);
			vote(chg->cp_disable_votable, SOC_LEVEL_VOTER, false, 0);
		}
		if (chg->fcc_votable) {
			vote(chg->fcc_votable, CP_VOTER, false, 0);
#if defined(CONFIG_SEC_A90Q_PROJECT)
			vote(chg->fcc_votable, SOC_LEVEL_VOTER, false, 0);
#endif
		}

		val.intval = 0;
		power_supply_set_property(battery->psy_bms,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_SOC_RESCALE, &val);

		val.intval = 0;
		power_supply_set_property(battery->psy_bms,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHARGE_FULL, &val);
	} else {
		vote(chg->chg_disable_votable, SEC_BATTERY_SLATE_MODE_VOTER, false, 0);
		if (battery->store_mode)
			val.intval = MICRO_5V;
		else
			val.intval = MICRO_12V;
		if (get_cable_type(battery) == POWER_SUPPLY_TYPE_USB_HVDCP_3)
			force_vbus_voltage_QC30(chg, val.intval);
		else
			power_supply_set_property(battery->psy_usb,
					POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT, &val);

		if (get_cable_type(battery) == POWER_SUPPLY_TYPE_USB_PD) {
			cancel_delayed_work(&battery->slate_work);
			wake_lock(&battery->slate_wake_lock);
			queue_delayed_work(battery->monitor_wqueue, &battery->slate_work, msecs_to_jiffies(3000));
			set_pd_hard_reset(1);
		} else {
			vote(chg->usb_icl_votable, SEC_BATTERY_SLATE_MODE_VOTER, false, 0);
			disable_charge_pump(chg, false, SEC_BATTERY_SLATE_MODE_VOTER, 0);
		}
	}
}

int sec_bat_get_hv_charger_status(struct sec_battery_info *battery)
{
	union power_supply_propval value = {0, };
	int ret, cable, pd_enabled, src_rp, input_vol_mV;
	int input_curr_mA = 0;
	int check_val=NORMAL_TA;
	struct smb_charger *chg;
	
	chg = power_supply_get_drvdata(battery->psy_bat);

	if(!battery->psy_bat)
	{
		pr_err("%s: Fail to get bat psy \n",__func__);
	} else {
		if(battery->psy_bat->desc->get_property != NULL)
		{
			ret = power_supply_get_property(battery->psy_bat, (enum power_supply_property)POWER_SUPPLY_EXT_PROP_AFC_RESULT, &value);
			if(ret < 0)
			{
				pr_err("%s: Fail to get usb POWER_SUPPLY_EXT_PROP_AFC_RESULT (%d=>%d)\n",
					__func__,(enum power_supply_property)POWER_SUPPLY_EXT_PROP_AFC_RESULT,ret);
			} else if (value.intval) { 
				pr_info("%s: Hv_charger_status(AFC) \n", __func__);
				return 1;			
			}			
		}	
	}
	if(!battery->psy_usb)
	{
		pr_err("%s: Fail to get usb psy \n",__func__);
		return -1;
	}
	else 
	{
		if(battery->psy_usb->desc->get_property != NULL)
		{
			cable = get_cable_type(battery);
			
			ret = power_supply_get_property(battery->psy_usb, POWER_SUPPLY_PROP_PD_ACTIVE, &value);
			if(ret < 0)
			{
				pr_err("%s: Fail to get usb pd_active_prop (%d=>%d)\n",__func__,POWER_SUPPLY_PROP_PD_ACTIVE,ret);
				pd_enabled=0;
			}
			else 
			{ 
				pd_enabled = value.intval;	
				//pr_info("%s: PD Active(%d) \n", __func__, pd_enabled);				
			}
			
			ret = power_supply_get_property(battery->psy_usb, POWER_SUPPLY_PROP_TYPEC_MODE, &value);
			if(ret < 0)
			{
				pr_err("%s: Fail to get usb typec_mode (%d=>%d)\n",__func__, POWER_SUPPLY_PROP_TYPEC_MODE,ret);
				src_rp =0;
			}
			else 
			{ 
				src_rp = value.intval;	
				//pr_info("%s: TYPE C Mode(%d) \n", __func__, src_rp);				
			}
			
			if(cable == POWER_SUPPLY_TYPE_USB_HVDCP)
			{
				//Input Voltage read
				ret = power_supply_get_property(battery->psy_usb, POWER_SUPPLY_PROP_VOLTAGE_MAX, &value);
				if(ret < 0)
				{
					pr_err("%s: Fail to get usb Input Voltage (%d=>%d)\n",__func__,POWER_SUPPLY_PROP_VOLTAGE_MAX,ret);
					input_vol_mV =0;
				}
				else 
				{	 
					input_vol_mV = value.intval/1000;	// Input Voltage in mV
					pr_info("%s: Input Voltage(%d) \n", __func__, input_vol_mV);				
				}
			}
	
			input_curr_mA = chg->now_icl/1000;	// Input Current in mA
			
			if ((pd_enabled == POWER_SUPPLY_PD_PPS_ACTIVE && 
				get_pd_max_power() >= HV_CHARGER_STATUS_STANDARD3) ||
				chg->ta_alert_mode) {
#if defined(CONFIG_DIRECT_CHARGING)
                                check_val = SFC_25W;
#else
                                check_val = AFC_12V_OR_20W;
#endif
			} else if((cable == POWER_SUPPLY_TYPE_USB_HVDCP_3) ||
			   (pd_enabled && (get_pd_max_power() >= HV_CHARGER_STATUS_STANDARD2))||
			   (cable == POWER_SUPPLY_TYPE_USB_HVDCP && input_vol_mV >= MILLI_12V)){
				check_val = AFC_12V_OR_20W;
			} 
			else if((pd_enabled && (get_pd_max_power() >= HV_CHARGER_STATUS_STANDARD1)) || 
				    (cable == POWER_SUPPLY_TYPE_USB_HVDCP && input_vol_mV >= MILLI_9V) || 
					(src_rp == POWER_SUPPLY_TYPEC_SOURCE_HIGH && (5*input_curr_mA) >= HV_CHARGER_STATUS_STANDARD1)){
				check_val = AFC_9V_OR_15W;
			}	// For POWER_SUPPLY_TYPEC_SOURCE_HIGH , if AICL occurs, return should be 0.
		}		
	}
	
	//pr_info("%s: Hv_charger_status(%d) input_curr_mA:%d\n", __func__, check_val, input_curr_mA);
	return check_val;
}

void sec_bat_set_ocp_mode(void)
{
	struct sec_battery_info *battery = get_sec_battery();

#if defined(CONFIG_BATTERY_CISD)
	if (battery != NULL) {
		struct smb_charger *chg =
			power_supply_get_drvdata(battery->psy_bat);

		battery->cisd.event_data[EVENT_TA_OCP_DET]++;
		if (!chg->ta_alert_mode)
			battery->cisd.event_data[EVENT_TA_OCP_ON]++;
	}
#endif
	sec_bat_set_current_event(SEC_BAT_CURRENT_EVENT_25W_OCP,
			SEC_BAT_CURRENT_EVENT_25W_OCP);
}

void sec_bat_set_ship_mode(int en)
{
	struct sec_battery_info *battery = get_sec_battery();

	if (battery != NULL) {
		struct smb_charger *chg;
		union power_supply_propval value = {0, };
		chg = power_supply_get_drvdata(battery->psy_bat);

		if (en == battery->is_ship_mode) {
			pr_info("%s: skip ship mode(%d)\n", __func__, en);
		} else {
			value.intval = !!en;
			power_supply_set_property(battery->psy_bat,
				POWER_SUPPLY_PROP_SET_SHIP_MODE, &value);
			battery->is_ship_mode = value.intval;
			pr_info("%s: change ship mode(%d)\n", __func__, battery->is_ship_mode);
		}
	}	
	return;
}
int sec_bat_get_ship_mode(void)
{
	struct sec_battery_info *battery = get_sec_battery();

	if (battery != NULL)
		return battery->is_ship_mode;
	else
		return 0;
}

void sec_bat_set_fet_control(int off)
{
	struct sec_battery_info *battery = get_sec_battery();

	if (battery != NULL) {
		struct smb_charger *chg;
		union power_supply_propval value = {0, };
		chg = power_supply_get_drvdata(battery->psy_bat);

		if (off == battery->is_fet_off) {
			pr_info("%s: skip batt fet control(%d)\n", __func__, off);
		} else {
			value.intval = !!off;
			power_supply_set_property(battery->psy_bat,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_BATT_FET_CONTROL, &value);
			battery->is_fet_off = value.intval;
			pr_info("%s: change batt fet off(%d)\n", __func__, battery->is_fet_off);
		}
	}	
	return;
}

int sec_bat_get_fet_control(void)
{
	struct sec_battery_info *battery = get_sec_battery();
	int off = 0;

	if(battery != NULL) {
		struct smb_charger *chg;
		chg = power_supply_get_drvdata(battery->psy_bat);
	
		off = smblib_read_batfet(chg);
		pr_info("%s: off(%d)\n", __func__, off);
	}

	return off;
}

int sec_bat_get_fg_asoc(struct sec_battery_info *battery)
{
	int rc = 0;
	unsigned int calc_asoc, charge_full;
	union power_supply_propval value = {0, };
		
	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_CHARGE_FULL, &value);

	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get fg charge full prop. rc=%d\n", __func__, rc);
		return -1;
	}

	charge_full = value.intval;
	calc_asoc = (charge_full * 100) / (battery->battery_full_capacity * 1000);

	pr_info("%s: charge_full(%d), asoc(%d)\n", __func__, charge_full, calc_asoc);

	return calc_asoc;
}

void sec_batt_reset_soc(void)
{
	struct sec_battery_info *battery = get_sec_battery();
	int cable_type;
	union power_supply_propval value = {0, };

	dev_info(battery->dev, "%s: Start\n", __func__);

	cable_type = get_cable_type(battery);

	/* Do NOT reset fuel gauge in charging mode */
	if (is_nocharge_type(cable_type)) {
		sec_bat_set_misc_event(BATT_MISC_EVENT_BATT_RESET_SOC,
			BATT_MISC_EVENT_BATT_RESET_SOC);

		value.intval = 1;
		power_supply_set_property(battery->psy_bms,
			POWER_SUPPLY_PROP_FG_RESET, &value);
	}
}

static void sec_bat_program_alarm(
				struct sec_battery_info *battery, int seconds)
{
	alarm_start(&battery->polling_alarm,
		    ktime_add(battery->last_poll_time, ktime_set(seconds, 0)));
}

static enum alarmtimer_restart sec_bat_alarm(
	struct alarm *alarm, ktime_t now)
{
	struct sec_battery_info *battery = container_of(alarm,
				struct sec_battery_info, polling_alarm);

	dev_dbg(battery->dev, "%s\n", __func__);

	if(battery->store_mode)
		queue_delayed_work(battery->monitor_wqueue, &battery->store_mode_work, 0);

	if(battery->safety_timer_set && battery->safety_timer_scheduled) {
		dev_dbg(battery->dev, "%s: Queue safety timer work\n", __func__);
		wake_lock(&battery->safety_timer_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->safety_timer_work, 0);
	}

	return ALARMTIMER_NORESTART;
}

bool is_store_mode(void)
{
	struct sec_battery_info *battery = get_sec_battery();

	return battery->store_mode;
}

void sec_start_store_mode(void)
{
	struct sec_battery_info *battery = get_sec_battery();
#if !defined(CONFIG_SEC_FACTORY)
	union power_supply_propval value = {0, };
	struct smb_charger *chg;

	chg = power_supply_get_drvdata(battery->psy_bat);

	disable_charge_pump(chg, true, SEC_BATTERY_STORE_MODE_VOTER, 0);

	value.intval = MICRO_5V;
	if (get_cable_type(battery) == POWER_SUPPLY_TYPE_USB_HVDCP_3)
		force_vbus_voltage_QC30(chg, value.intval);
	else
		power_supply_set_property(battery->psy_usb,
				POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT, &value);
#endif
	battery->store_mode = true;
	battery->last_poll_time = ktime_get_boottime();
	sec_bat_program_alarm(battery, 1);
}

void sec_stop_store_mode(void)
{
	struct sec_battery_info *battery = get_sec_battery();
#if !defined(CONFIG_SEC_FACTORY)
	union power_supply_propval value = {0, };
	struct smb_charger *chg;

	chg = power_supply_get_drvdata(battery->psy_bat);

	value.intval = MICRO_12V;
	if (get_cable_type(battery) == POWER_SUPPLY_TYPE_USB_HVDCP_3)
		force_vbus_voltage_QC30(chg, value.intval);
	else
		power_supply_set_property(battery->psy_usb,
				POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT, &value);

	disable_charge_pump(chg, false, SEC_BATTERY_STORE_MODE_VOTER, 1000);
#endif

	battery->store_mode = false;
	alarm_cancel(&battery->polling_alarm);
}

static void sec_store_mode_work(
				struct work_struct *work)
{
	struct sec_battery_info *battery =
		container_of(work, struct sec_battery_info,
		store_mode_work.work);
	int cable_type;
	union power_supply_propval value = {0, };
	struct smb_charger *chg;
	chg = power_supply_get_drvdata(battery->psy_bat);

	dev_dbg(battery->dev, "%s: Start\n", __func__);

	cable_type = get_cable_type(battery);

	if (!is_nocharge_type(cable_type) && battery->store_mode) {
		power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_CAPACITY, &value);
		battery->soc = value.intval;

		power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_STATUS, &value);
		battery->status = value.intval;

		pr_info("%s, Soc : %d , status : %s\n", __func__, battery->soc, sec_bat_status_str[battery->status]); 

		if (battery->soc >= battery->store_mode_charging_max) {
			/* to discharge the battery, off buck */
			vote(chg->chg_disable_votable, SEC_BATTERY_STORE_MODE_VOTER, true, 0);
			if (battery->soc > battery->store_mode_charging_max)
				vote(chg->usb_icl_votable, SEC_BATTERY_STORE_MODE_VOTER, true, 0);
		}

		if ((battery->soc <= battery->store_mode_charging_min) &&
			((battery->status == POWER_SUPPLY_STATUS_DISCHARGING) ||
			(battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING))) {
			vote(chg->chg_disable_votable, SEC_BATTERY_STORE_MODE_VOTER, false, 0);
			vote(chg->usb_icl_votable, SEC_BATTERY_STORE_MODE_VOTER, false, 0);
		}
	}
#if !defined(CONFIG_SEC_FACTORY)
		/* Set limited max current with hv cable when store mode is set and LDU
			limited max current should be set with over 5% capacity since target could be turned off during boot up */
		if(battery->store_mode){
				int voltage_now;
				voltage_now = get_usb_voltage_now(battery);
			if (voltage_now >= 7500000 && battery->soc > 5) { //7.5V
#if defined(CONFIG_SEC_A60Q_PROJECT) || defined(CONFIG_SEC_M40_PROJECT)
				if(chg->afc_sts == AFC_9V) {
					afc_set_voltage(SET_5V);
					value.intval = MICRO_5V;
					power_supply_set_property(battery->psy_usb,
							POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT, &value);
					vote(chg->usb_icl_votable, SEC_BATTERY_STORE_MODE_VOTER, true, 720000);
				} else if (chg->afc_sts < AFC_5V)
#endif
				vote(chg->usb_icl_votable, SEC_BATTERY_STORE_MODE_VOTER, true, 440000);
			} else {
#if defined(CONFIG_SEC_A60Q_PROJECT) || defined(CONFIG_SEC_M40_PROJECT)
				if (chg->afc_sts < AFC_5V || battery->soc <= 5)
#endif
				vote(chg->usb_icl_votable, SEC_BATTERY_STORE_MODE_VOTER, false, 0);
			}
		}
#endif

	if (battery->store_mode && !is_nocharge_type(cable_type)) {
		battery->last_poll_time = ktime_get_boottime();
		sec_bat_program_alarm(battery, battery->store_mode_polling_time);
	}
}

static void sec_usb_changed_work(
				struct work_struct *work)
{
	struct sec_battery_info *battery =
		container_of(work, struct sec_battery_info,
		usb_changed_work.work);
	unsigned int set_val;
	int cable_type;

	dev_dbg(battery->dev, "%s: Start\n", __func__);
	
	cable_type = get_cable_type(battery);
	if (cable_type == POWER_SUPPLY_TYPE_USB_FLOAT
		&& (get_rid_type() != 6 /* RID_ADC_619K */)) {
		union power_supply_propval pval = {0};
		pval.intval = -ETIMEDOUT;
		power_supply_set_property(battery->psy_usb,
				POWER_SUPPLY_PROP_SDP_CURRENT_MAX, &pval);
		set_val = BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE;
	} else {
		set_val = 0;
	}
	sec_bat_set_misc_event(set_val, BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE);

	wake_unlock(&battery->usb_changed_wake_lock);
}

static void sec_bat_changed_work(
				struct work_struct *work)
{
	struct sec_battery_info *battery =
		container_of(work, struct sec_battery_info,
		bat_changed_work.work);
	int rc = 0;
	int charge_type;
	union power_supply_propval value = {0, };
	struct smb_charger *chg;
	chg = power_supply_get_drvdata(battery->psy_bat);

	dev_dbg(battery->dev, "%s: Start\n", __func__);

	rc = power_supply_get_property(battery->psy_bat,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &value);
	if (rc < 0){
		dev_err(battery->dev, "%s: Fail to get charge type (%d)\n", __func__, rc);
	} else {
		charge_type = value.intval;
		if ((charge_type != POWER_SUPPLY_CHARGE_TYPE_NONE) && battery->safety_timer_set) {
			if (!battery->safety_timer_scheduled) {
				pr_info("%s: Start safety timer work\n", __func__);

				power_supply_get_property(battery->psy_bat,
					POWER_SUPPLY_PROP_STATUS, &value);
				battery->status = value.intval;
				if (battery->status == POWER_SUPPLY_STATUS_FULL) {
					battery->expired_time = battery->recharging_expired_time;
					battery->prev_safety_time = 0;
				} else {
					battery->expired_time = battery->pdata_expired_time;
					battery->prev_safety_time = 0;
				}
				battery->safety_timer_scheduled = true;
				wake_lock(&battery->safety_timer_wake_lock);
				queue_delayed_work(battery->monitor_wqueue, &battery->safety_timer_work, 0);
			}
		} else if (battery->safety_timer_scheduled) {
			battery->safety_timer_scheduled = false;
			pr_info("%s: Cancel safety timer work\n", __func__);
			cancel_delayed_work(&battery->safety_timer_work);
			wake_unlock(&battery->safety_timer_wake_lock);
			battery->batt_health = POWER_SUPPLY_HEALTH_GOOD;
		}
	}

	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_HEALTH, &value);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get batt health prop. rc=%d\n", __func__, rc);
	} else {
		if (value.intval == POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
			vote(chg->chg_disable_votable, SEC_BATTERY_VBAT_OVP_VOTER, true, 0);
			sec_bat_set_current_event(SEC_BAT_CURRENT_EVENT_VBAT_OVP,
				  SEC_BAT_CURRENT_EVENT_VBAT_OVP);
		}
	}

	wake_unlock(&battery->bat_changed_wake_lock);
}

static void sec_bat_safety_timer_work(struct work_struct *work)
{
	struct sec_battery_info *battery =
		container_of(work, struct sec_battery_info,
		safety_timer_work.work);
	unsigned long long expired_time = battery->expired_time;
	struct timespec ts = {0, };
	unsigned int curr = 0;
	unsigned int current_max, current_avg;
	unsigned int charging_current;
	unsigned int chg_float_voltage;
	unsigned int input_voltage;
	unsigned long int input_power, charging_power;
	int rc = 0;
	static int discharging_cnt = 0;
	union power_supply_propval value = {0, };
	struct smb_charger *chg =
		power_supply_get_drvdata(battery->psy_bat);

	dev_dbg(battery->dev, "%s: Start\n", __func__);

	if (battery->siop_level != 100) {
		battery->stop_timer = true;
	}

	rc = power_supply_get_property(battery->psy_bat,
		POWER_SUPPLY_PROP_CURRENT_NOW, &value);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get current now. rc=%d\n", __func__, rc);
		current_avg = 0;
	} else {
		current_avg = value.intval;
	}

	if (current_avg < 0) {
		discharging_cnt++;
	} else {
		discharging_cnt = 0;
	}

	if (discharging_cnt >= 5) {
		battery->expired_time = battery->pdata_expired_time;
		battery->prev_safety_time = 0;
		pr_info("%s : SAFETY TIME RESET! DISCHARGING CNT(%d)\n",
			__func__, discharging_cnt);
		discharging_cnt = 0;
		goto exit_safety_timer;
	} else if (battery->lcd_status && battery->stop_timer) {
		battery->prev_safety_time = 0;
		pr_info("%s : LCD is ON pause safety timer work.\n",
			__func__);
		goto exit_safety_timer;
	}

	get_monotonic_boottime(&ts);

	if (battery->prev_safety_time == 0) {
		battery->prev_safety_time = ts.tv_sec;
	}

#if defined(CONFIG_SEC_A60Q_PROJECT)
	rc = power_supply_get_property(battery->psy_usb,
		POWER_SUPPLY_PROP_VOLTAGE_MAX_LIMIT, &value);
#else
	rc = power_supply_get_property(battery->psy_usb,
		POWER_SUPPLY_PROP_VOLTAGE_MAX, &value);
#endif
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get input voltage. rc=%d\n", __func__, rc);
		input_voltage = 5000;    /* in mV */
	} else {
		input_voltage = value.intval;
		input_voltage = input_voltage / 1000;    /* uV -> mV */
	}

	rc = power_supply_get_property(battery->psy_usb,
		POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED, &value);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get input current. rc=%d\n", __func__, rc);
		current_max = 500000;    /* in uA */
	} else {
		current_max = value.intval;
	}

	rc = power_supply_get_property(battery->psy_bat,
		POWER_SUPPLY_PROP_VOLTAGE_MAX, &value);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get float voltage. rc=%d\n", __func__, rc);
		chg_float_voltage = 4350;    /* in mV */
	} else {
		chg_float_voltage = value.intval;
		chg_float_voltage = chg_float_voltage / 1000;    /* uV -> mV */
	}

	rc = power_supply_get_property(battery->psy_bat,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_CHARGE_CURRENT, &value);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get charge current. rc=%d\n", __func__, rc);
		charging_current = 0;
	} else {
		charging_current = value.intval;
	}

	pr_info("%s : INPUT_VOLTAGE(%u), FLOAT_VOLTAGE(%u), CURR_MAX(%u), CHARGE_CURRENT(%u), CURRENT_AVG(%u)\n",
		__func__, input_voltage, chg_float_voltage, current_max, charging_current, current_avg);

	input_power = (unsigned long int) current_max * input_voltage;
	charging_power = (unsigned long int) charging_current * chg_float_voltage;

	if (input_power > charging_power) {
		curr = charging_current;
	} else {
		curr = input_power / chg_float_voltage;
		curr = (curr * 9) / 10;
	}

	if (battery->lcd_status && !battery->stop_timer) {
		battery->stop_timer = true;
	} else if (!battery->lcd_status && battery->stop_timer) {
		battery->stop_timer = false;
	}

	if (curr == 0) {
		goto exit_safety_timer;
	} else if (curr > battery->standard_curr)
		curr = battery->standard_curr;

	pr_info("%s : EXPIRED_TIME(%llu), IP(%lu), CP(%lu), CURR(%u), STANDARD(%u)\n",
		__func__, expired_time, input_power, charging_power, curr, battery->standard_curr);

	expired_time *= battery->standard_curr;
	do_div(expired_time, curr);

	pr_info("%s : CAL_EXPIRED_TIME(%llu) TIME NOW(%ld) TIME PREV(%ld)\n", __func__, expired_time, ts.tv_sec, battery->prev_safety_time);

	if (expired_time <= ((ts.tv_sec - battery->prev_safety_time) * 1000))
		expired_time = 0;
	else
		expired_time -= ((ts.tv_sec - battery->prev_safety_time) * 1000);

	battery->cal_safety_time = expired_time;
	expired_time *= curr;
	do_div(expired_time, battery->standard_curr);

	battery->expired_time = expired_time;
	battery->prev_safety_time = ts.tv_sec;
	pr_info("%s : REMAIN_TIME(%ld) CAL_REMAIN_TIME(%ld)\n", __func__, battery->expired_time, battery->cal_safety_time);

	if (battery->expired_time == 0) {
		/* disable charging off buck */
		pr_info("%s: Safety timer expired, stop charging\n", __func__);
		vote(chg->chg_disable_votable, SEC_BATTERY_SAFETY_TIMER_VOTER, true, 0);
		battery->expired_time = battery->pdata_expired_time;
		battery->prev_safety_time = 0;
		battery->batt_health = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
		battery->safety_timer_scheduled = false;
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.data[CISD_DATA_SAFETY_TIMER]++;
		battery->cisd.data[CISD_DATA_SAFETY_TIMER_PER_DAY]++;
#endif
		wake_unlock(&battery->safety_timer_wake_lock);
		return;
	}

exit_safety_timer:
	if (battery->safety_timer_set && battery->safety_timer_scheduled) {
		battery->last_poll_time = ktime_get_boottime();
		sec_bat_program_alarm(battery, battery->safety_timer_polling_time);
	}

	wake_unlock(&battery->safety_timer_wake_lock);

}

void sec_bat_safety_timer_reset(void)
{
	struct sec_battery_info *battery = get_sec_battery();
	if (battery) {
		battery->expired_time = battery->pdata_expired_time;
		battery->prev_safety_time = 0;
	}
}

static int sec_battery_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct sec_battery_info *battery = container_of(nb, struct sec_battery_info, nb);

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (strcmp(psy->desc->name, "usb") == 0) {
		wake_lock(&battery->usb_changed_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->usb_changed_work, 0);
	}
	if (strcmp(psy->desc->name, "battery") == 0) {
		wake_lock(&battery->bat_changed_wake_lock);
		queue_delayed_work(battery->monitor_wqueue, &battery->bat_changed_work, 0);
	}

	return NOTIFY_OK;
}

#if defined(CONFIG_VBUS_NOTIFIER)
static int vbus_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	vbus_status_t vbus_status = *(vbus_status_t *)data;
	struct sec_battery_info *battery =
		container_of(nb, struct sec_battery_info, vbus_nb);
	struct smb_charger *chg;
	static int vbus_pre_attach;
	union power_supply_propval val = {0, };

	chg = power_supply_get_drvdata(battery->psy_bat);

	if (vbus_pre_attach == vbus_status)
		return 0;

	switch (vbus_status) {
	case STATUS_VBUS_HIGH:
		pr_info("%s: cable is inserted\n", __func__);
#if defined(CONFIG_DIRECT_CHARGING)
#if defined(CONFIG_SEC_A90Q_PROJECT)
		if ((get_pd_active(battery) == POWER_SUPPLY_PD_PPS_ACTIVE)
				&& (chg->pdp_limit_w > 0)) {
			val.intval = chg->default_pdp_limit_w;
			power_supply_set_property(battery->psy_usb,
				POWER_SUPPLY_PROP_PDP_LIMIT_W, &val);
		}
#endif
#endif
		break;
	case STATUS_VBUS_LOW:
		pr_info("%s: cable is removed\n", __func__);
		if(!is_slate_mode(battery) &&
				get_client_vote(chg->usb_icl_votable, SEC_BATTERY_SLATE_MODE_VOTER) == 0) {
			vote(chg->usb_icl_votable, SEC_BATTERY_SLATE_MODE_VOTER, false, 0);
		}
		sec_bat_set_cable_type_current(chg->real_charger_type, false);

		val.intval = 0;
		power_supply_set_property(battery->psy_bms,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_SOC_RESCALE, &val);

#if defined(CONFIG_DIRECT_CHARGING)
#if defined(CONFIG_SEC_A90Q_PROJECT)
		if (chg->pdp_limit_w > 0) {
			val.intval = chg->default_pdp_limit_w - 1;
			power_supply_set_property(battery->psy_usb,
				POWER_SUPPLY_PROP_PDP_LIMIT_W, &val);
		}
#endif
#endif

		chg->float_type_recheck = false;
#if defined(CONFIG_AFC)
		chg->afc_sts = AFC_INIT;
		chg->vbus_chg_by_full = false;
		vote(chg->usb_icl_votable, SEC_BATTERY_AFC_VOTER, false, 0);
#endif
		vote(chg->usb_icl_votable, SEC_BATTERY_OVERHEATLIMIT_VOTER, false, 0);
		battery->batt_temp_overheat_check = false;
#if defined(CONFIG_BATTERY_CISD)
		battery->cisd.ab_vbat_check_count = 0;
		battery->cisd.state &= ~CISD_STATE_OVER_VOLTAGE;
#endif
		
		if (battery->store_mode) {
			pr_info("%s: [store_mode] release STORE_MODE_VOTER\n", __func__);
			vote(chg->chg_disable_votable, SEC_BATTERY_STORE_MODE_VOTER, false, 0);
			vote(chg->usb_icl_votable, SEC_BATTERY_STORE_MODE_VOTER, false, 0);
		}

		vote(chg->chg_disable_votable, SEC_BATTERY_VBAT_OVP_VOTER, false, 0);
		vote(chg->chg_disable_votable, SEC_BATTERY_SAFETY_TIMER_VOTER, false, 0);
		battery->batt_health = POWER_SUPPLY_HEALTH_GOOD;

		sec_bat_set_current_event(SEC_BAT_CURRENT_EVENT_USB_100MA,
			  (SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE |
			   SEC_BAT_CURRENT_EVENT_AFC |
			   SEC_BAT_CURRENT_EVENT_USB_SUPER |
			   SEC_BAT_CURRENT_EVENT_USB_100MA |
			   SEC_BAT_CURRENT_EVENT_VBAT_OVP |
			   SEC_BAT_CURRENT_EVENT_VSYS_OVP |
			   SEC_BAT_CURRENT_EVENT_CHG_LIMIT |
			   SEC_BAT_CURRENT_EVENT_AICL |
			   SEC_BAT_CURRENT_EVENT_SELECT_PDO |
			   SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING |
			   SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_2ND |
			   SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD |
			   SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING | 
			   SEC_BAT_CURRENT_EVENT_HIGH_TEMP_LIMIT |
			   SEC_BAT_CURRENT_EVENT_25W_OCP));
		sec_bat_set_misc_event(0, BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE);
		sec_bat_set_misc_event(0, BATT_MISC_EVENT_HICCUP_TYPE);
		vote(chg->usb_icl_votable, MOISTURE_VOTER, false, 0);
		break;
	default:
		pr_info("%s: vbus skip attach = %d\n", __func__, vbus_status);
		break;
	}

	vbus_pre_attach = vbus_status;

	sec_bat_run_monitor_work();

	return 0;
}
#endif

#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int sec_battery_handle_ccic_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct smb_charger *chg;
	struct sec_battery_info *battery =
			container_of(nb, struct sec_battery_info, usb_typec_nb);
	CC_NOTI_ATTACH_TYPEDEF usb_typec_info = *(CC_NOTI_ATTACH_TYPEDEF *)data;
	chg = power_supply_get_drvdata(battery->psy_bat);
	
	if (usb_typec_info.dest != CCIC_NOTIFY_DEV_BATTERY) {
		dev_info(battery->dev, "%s: skip handler dest(%d)\n",
			__func__, usb_typec_info.dest);
		return 0;
	}

	mutex_lock(&battery->typec_notylock);
	if(usb_typec_info.id == CCIC_NOTIFY_ID_WATER ) {
		if(usb_typec_info.attach)
		{
			pr_info("%s: Water Detected\n", __func__);	
			battery->hiccup_status = 1;
			// Disable Charging.
			vote(chg->usb_icl_votable, MOISTURE_VOTER, true, 0);
			if(lpcharge)
				sec_bat_set_misc_event(BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE, BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE);
			else
				sec_bat_set_misc_event(BATT_MISC_EVENT_HICCUP_TYPE, BATT_MISC_EVENT_HICCUP_TYPE);
		}
		else
		{	
			battery->hiccup_status = 0;
			vote(chg->usb_icl_votable, MOISTURE_VOTER, false, 0);
			sec_bat_set_misc_event(0, BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE);
			sec_bat_set_misc_event(0, BATT_MISC_EVENT_HICCUP_TYPE);
			pr_info("%s: Dry Event detected\n", __func__);
		}
	
	}
	mutex_unlock(&battery->typec_notylock);
	return 0;
}
#endif

static int sec_battery_register_notifier(struct sec_battery_info *battery)
{
	int rc;

	battery->nb.notifier_call = sec_battery_notifier_call;
	rc = power_supply_reg_notifier(&battery->nb);
	if (rc < 0) {
		dev_info(battery->dev,
			"%s: Couldn't register psy notifier rc = %d\n", __func__, rc);
		return rc;
	}
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	rc = manager_notifier_register(&battery->usb_typec_nb, sec_battery_handle_ccic_notification, MANAGER_NOTIFY_CCIC_BATTERY);
	if (rc < 0) {
		dev_info(battery->dev,
			"%s: Couldn't register psy notifier rc = %d\n", __func__, rc);
		return rc;
	}
#endif
	return 0;
}

void sec_bat_set_gpio_vbat_sense(int signal)
{
	struct sec_battery_info *battery = get_sec_battery();

	if (battery != NULL) {
		if (battery->gpio_vbat_sense) {
			pr_info("%s : %s\n", __func__, ((signal == 0) ? "LOW" : "HIGH"));
			gpio_direction_output(battery->gpio_vbat_sense, signal);
		}
		else {
			pr_info("%s : There is no gpio_vbat_sense\n", __func__);
		}
	}
}
EXPORT_SYMBOL(sec_bat_set_gpio_vbat_sense);

bool sec_bat_use_temp_adc_table(void)
{
	struct sec_battery_info *battery = get_sec_battery();

	if (battery == NULL)
		return false;
	else
		return battery->use_temp_adc_table;
}

bool sec_bat_use_chg_temp_adc_table(void)
{
	struct sec_battery_info *battery = get_sec_battery();

	if (battery == NULL)
		return false;
	else
		return battery->use_chg_temp_adc_table;
}

int sec_bat_convert_adc_to_temp(enum sec_battery_adc_channel channel, int temp_adc)
{
	struct sec_battery_info *battery = get_sec_battery();
	int temp = 0, low = 0, high = 0, mid = 0;
	const sec_bat_adc_table_data_t *temp_adc_table = {0 , };
	unsigned int temp_adc_table_size = 0;

	if (temp_adc < 0) {
		dev_info(battery->dev,
			"%s:  Invalid temp_adc(%d). channel(%d)\n", __func__, temp_adc, channel);
		return (-ENODATA * 10);
	}

	switch (channel) {
		case SEC_BAT_ADC_CHANNEL_TEMP:
			temp_adc_table = battery->temp_adc_table;
			temp_adc_table_size = battery->temp_adc_table_size;
			break;
		case SEC_BAT_ADC_CHANNEL_CHG_TEMP:
			temp_adc_table = battery->chg_temp_adc_table;
			temp_adc_table_size = battery->chg_temp_adc_table_size;
			break;
		default:
			dev_err(battery->dev,
				"%s: Channel(%d). Invalid Property\n", __func__, channel);
			return (-ENODATA * 10);
	}

	if (temp_adc_table[0].adc >= temp_adc) {
		temp = temp_adc_table[0].data;
		goto temp_by_adc_goto;
	} else if (temp_adc_table[temp_adc_table_size-1].adc <= temp_adc) {
		temp = temp_adc_table[temp_adc_table_size-1].data;
		goto temp_by_adc_goto;
	}

	high = temp_adc_table_size - 1;

	while (low <= high) {
		mid = (low + high) / 2;
		if (temp_adc_table[mid].adc > temp_adc)
			high = mid - 1;
		else if (temp_adc_table[mid].adc < temp_adc)
			low = mid + 1;
		else {
			temp = temp_adc_table[mid].data;
			goto temp_by_adc_goto;
		}
	}

	temp = temp_adc_table[high].data;
	temp += ((temp_adc_table[low].data - temp_adc_table[high].data) *
		 (temp_adc - temp_adc_table[high].adc)) /
		(temp_adc_table[low].adc - temp_adc_table[high].adc);

temp_by_adc_goto:
	dev_info(battery->dev,
		"%s: Channel(%d), Temp(%d), Temp-ADC(%d)\n",
		__func__, channel, temp, temp_adc);

	return temp;
}

#if !defined(CONFIG_SEC_FACTORY)
static char* salescode_from_cmdline;

bool sales_code_is(char* str)
{
	bool status = 0;
	char* salescode;

	salescode = kmalloc(4, GFP_KERNEL);
	if (!salescode) {
		goto out;
	}
	memset(salescode, 0x00,4);

	salescode = salescode_from_cmdline;

	pr_info("%s: %s\n", __func__,salescode);

	if(!strncmp((char *)salescode, str, 3))
		status = 1;

out:	return status;
}

static int __init sales_code_setup(char *str)
{
	salescode_from_cmdline = str;
	return 1;
}
__setup("androidboot.sales_code=", sales_code_setup);
#endif

static int sec_battery_probe(struct platform_device *pdev)
{
//	sec_battery_platform_data_t *pdata = NULL;
	struct sec_battery_info *battery;
	int ret = 0;
	union power_supply_propval value = {0, };
	int init_cable_type;

	dev_info(&pdev->dev,
		"%s: SEC Battery Driver Loading\n", __func__);

	battery = kzalloc(sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -ENOMEM;
#if 0
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
			goto err_bat_free;
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		battery->pdata = pdata;
	}
#endif

	if (sec_bat_parse_dt(&pdev->dev, battery)) {
		dev_err(&pdev->dev,
			"%s: Failed to get battery dt\n", __func__);
		ret = -EINVAL;
		goto err_bat_free;
	}

	platform_set_drvdata(pdev, battery);
	battery->dev = &pdev->dev;

	mutex_init(&battery->current_eventlock);
	mutex_init(&battery->misc_eventlock);
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	mutex_init(&battery->typec_notylock);
#endif
	wake_lock_init(&battery->usb_changed_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec_usb_changed_work");
	wake_lock_init(&battery->bat_changed_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec_bat_changed_work");
	wake_lock_init(&battery->safety_timer_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec_bat_safety_timer_work");
	wake_lock_init(&battery->siop_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-siop");
	wake_lock_init(&battery->monitor_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-monitor");
	wake_lock_init(&battery->slate_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-slate");

	battery->store_mode = false;
	battery->store_mode_charging_max = STORE_MODE_CHARGING_MAX;
	battery->store_mode_charging_min = STORE_MODE_CHARGING_MIN;
#if !defined(CONFIG_SEC_FACTORY)
	if (sales_code_is("VZW")) {
		dev_err(battery->dev, "%s: Sales is VZW\n", __func__);
		battery->store_mode_charging_max = STORE_MODE_CHARGING_MAX_VZW;
		battery->store_mode_charging_min = STORE_MODE_CHARGING_MIN_VZW;
	}
#endif
	battery->siop_level = 100;
	battery->mix_limit = false;
	battery->chg_limit = false;
	battery->slate_mode = false;

	battery->lcd_status = false;
	battery->safety_timer_set = true;
	battery->safety_timer_scheduled = false;
	battery->stop_timer = false;
	battery->prev_safety_time = 0;

	battery->cable_real_type = POWER_SUPPLY_TYPE_BATTERY;
	battery->batt_temp = 300;
	battery->die_temp = 300;
	battery->cp_die_temp = 300;
	battery->vbus_level = 5000;
	battery->float_voltage = 4350;
	battery->status = POWER_SUPPLY_STATUS_DISCHARGING;
	battery->prev_status = POWER_SUPPLY_STATUS_DISCHARGING;
	battery->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	battery->die_health = POWER_SUPPLY_HEALTH_GOOD;
	battery->charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	battery->soc = 0;
	battery->v_now = 0;
	battery->i_in = 0;
	battery->i_now = 0;
	battery->i_chg = 0;
	battery->i_max = 0;
	battery->hw_max = 0;
	battery->ocv = 0;
	battery->conn_type = POWER_SUPPLY_CONNECTOR_MICRO_USB;
	battery->typec_mode = POWER_SUPPLY_TYPEC_NONE;
	battery->hiccup_status = 0;

	battery->batt_cycle = -1;
	battery->isApdo = false;
	battery->batt_pon_ocv = 0;
	battery->is_fet_off = 0;
	battery->is_ship_mode = 0;
#if defined(CONFIG_BATTERY_CISD)
	battery->skip_cisd = false;
	battery->cisd.ab_vbat_check_count = 0;
	battery->cisd.state &= ~CISD_STATE_OVER_VOLTAGE;
#endif
	battery->batt_temp_overheat_check = false;
	battery->interval_pdp_limit_w = 16;

	/* create work queue */
	battery->monitor_wqueue =
		create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!battery->monitor_wqueue) {
		dev_err(battery->dev,
			"%s: Fail to Create Workqueue\n", __func__);
		goto err_workqueue;
	}

	ttf_init(battery);

	INIT_DELAYED_WORK(&battery->usb_changed_work, sec_usb_changed_work);
	INIT_DELAYED_WORK(&battery->bat_changed_work, sec_bat_changed_work);
	INIT_DELAYED_WORK(&battery->store_mode_work, sec_store_mode_work);
	INIT_DELAYED_WORK(&battery->safety_timer_work, sec_bat_safety_timer_work);
	INIT_DELAYED_WORK(&battery->siop_work, sec_bat_siop_work);
	INIT_DELAYED_WORK(&battery->monitor_work, sec_bat_monitor_work);
	INIT_DELAYED_WORK(&battery->slate_work, sec_bat_slate_work);

	alarm_init(&battery->polling_alarm, ALARM_BOOTTIME, sec_bat_alarm);

#if defined(CONFIG_BATTERY_CISD)
	sec_battery_cisd_init(battery);
#endif

	/* init power supplier framework */
	battery->psy_bat = power_supply_get_by_name("battery");
	if (!battery->psy_bat) {
		dev_err(battery->dev,
			"%s: Failed to Register psy_bat\n", __func__);
		ret = -EPROBE_DEFER;
		goto err_supply_unreg_bat;
	}

	battery->psy_usb = power_supply_get_by_name("usb");
	if (!battery->psy_usb) {
		dev_err(battery->dev,
			"%s: Failed to Register psy_usb\n", __func__);
		goto err_supply_unreg_usb;
	}else{
                /*Check charging option for wired charging*/
		if(get_afc_mode())
		    value.intval = HV_DISABLE;
		else
		    value.intval = HV_ENABLE;
		ret = power_supply_set_property(battery->psy_usb,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_HV_DISABLE, &value);
		if(ret < 0) {
			pr_err("%s: Fail to set voltage max limit%d\n", __func__, ret);
		}
	}

	battery->psy_bms = power_supply_get_by_name("bms");
	if (!battery->psy_bms) {
		dev_err(battery->dev,
			"%s: Failed to Register psy_bms\n", __func__);
		goto err_supply_unreg_bms;
	}

	battery->psy_usb_main = power_supply_get_by_name("main");
	if (!battery->psy_usb_main) {
		dev_err(battery->dev,
			"%s: Failed to Register psy_usb_main\n", __func__);
		goto err_supply_unreg_usb_main;
	}

	battery->psy_slave = power_supply_get_by_name("charge_pump_master");
	if (!battery->psy_slave) {
		dev_err(battery->dev,
			"%s: Failed to Register charge_pump_master\n", __func__);
	}

	ret = sec_bat_create_attrs(&battery->psy_bat->dev);
	if (ret) {
		dev_err(battery->dev,
			"%s : Failed to create_attrs\n", __func__);
		goto err_supply_unreg_bat;
	}
	g_battery = battery;

#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&battery->vbus_nb,
		vbus_handle_notification, VBUS_NOTIFY_DEV_CHARGER);
#endif
	if (get_rid_type() == 5 /* RID_ADC_523K */) {
		power_supply_get_property(battery->psy_bms,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_PON_OCV, &value);
		battery->batt_pon_ocv = value.intval / 1000;

		qg_set_sdam_prifile_version(0xfa);	// fg_reset in next booting
		sec_bat_set_ship_mode(1);
#if defined(CONFIG_SEC_FACTORY)
		battery->is_fet_off = 1;
		sec_bat_set_fet_control(0);
#endif
	}

	// temp code for a90 vbat_sense_select oscillation issue
	sec_bat_set_gpio_vbat_sense(0);

#if defined(CONFIG_STORE_MODE) || defined(CONFIG_SEC_FACTORY)
	sec_start_store_mode();
#endif

	init_cable_type = get_cable_type(battery);

	if (!is_nocharge_type(init_cable_type)) {
		sec_bat_set_cable_type_current(init_cable_type, true);
	}

	sec_battery_register_notifier(battery);

	dev_info(battery->dev,
		"%s: SEC Battery Driver Loaded\n", __func__);
	return 0;

err_supply_unreg_usb_main:
power_supply_unregister(battery->psy_bms);
err_supply_unreg_bms:
power_supply_unregister(battery->psy_usb);
err_supply_unreg_usb:
power_supply_unregister(battery->psy_bat);
err_supply_unreg_bat:
	destroy_workqueue(battery->monitor_wqueue);
err_workqueue:
	wake_lock_destroy(&battery->usb_changed_wake_lock);
	wake_lock_destroy(&battery->bat_changed_wake_lock);
	wake_lock_destroy(&battery->safety_timer_wake_lock);
	wake_lock_destroy(&battery->siop_wake_lock);
	wake_lock_destroy(&battery->monitor_wake_lock);
	wake_lock_destroy(&battery->slate_wake_lock);
	mutex_destroy(&battery->current_eventlock);
	mutex_destroy(&battery->misc_eventlock);
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	mutex_destroy(&battery->typec_notylock);
#endif
err_bat_free:
	kfree(battery);

	return ret;
}

static int sec_battery_remove(struct platform_device *pdev)
{
	struct sec_battery_info *battery = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);
	alarm_cancel(&battery->polling_alarm);
	destroy_workqueue(battery->monitor_wqueue);
	wake_lock_destroy(&battery->usb_changed_wake_lock);
	wake_lock_destroy(&battery->bat_changed_wake_lock);
	mutex_destroy(&battery->current_eventlock);
	mutex_destroy(&battery->misc_eventlock);
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	mutex_destroy(&battery->typec_notylock);
#endif
	kfree(battery);
	pr_info("%s: --\n", __func__);

	return 0;
}

static int sec_battery_prepare(struct device *dev)
{
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
	return;
}

static void sec_battery_shutdown(struct platform_device *pdev)
{
	pr_info("%s: ++\n", __func__);
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

static int sec_battery_init(void)
{
	return platform_driver_register(&sec_battery_driver);
}

static void sec_battery_exit(void)
{
	platform_driver_unregister(&sec_battery_driver);
}

late_initcall(sec_battery_init);
module_exit(sec_battery_exit);

MODULE_DESCRIPTION("Samsung Battery Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
