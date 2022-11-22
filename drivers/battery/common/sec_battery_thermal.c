/*
 *  sec_battery_thermal.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "include/sec_battery.h"

#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
extern int muic_set_hiccup_mode(int on_off);
#endif

char *sec_bat_thermal_zone[] = {
	"COLD",
	"COOL3",
	"COOL2",
	"COOL1",
	"NORMAL",
	"WARM",
	"OVERHEAT",
	"OVERHEATLIMIT",
};

void sec_bat_set_threshold(struct sec_battery_info *battery)
{
	if (is_wireless_type(battery->cable_type) || battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE) {
		battery->cold_cool3_thresh = battery->pdata->wireless_cold_cool3_thresh;
		battery->cool3_cool2_thresh = battery->pdata->wireless_cool3_cool2_thresh;
		battery->cool2_cool1_thresh = battery->pdata->wireless_cool2_cool1_thresh;
		battery->cool1_normal_thresh = battery->pdata->wireless_cool1_normal_thresh;
		battery->normal_warm_thresh = battery->pdata->wireless_normal_warm_thresh;
		battery->warm_overheat_thresh = battery->pdata->wireless_warm_overheat_thresh;
	} else {
		battery->cold_cool3_thresh = battery->pdata->wire_cold_cool3_thresh;
		battery->cool3_cool2_thresh = battery->pdata->wire_cool3_cool2_thresh;
		battery->cool2_cool1_thresh = battery->pdata->wire_cool2_cool1_thresh;
		battery->cool1_normal_thresh = battery->pdata->wire_cool1_normal_thresh;
		battery->normal_warm_thresh = battery->pdata->wire_normal_warm_thresh;
		battery->warm_overheat_thresh = battery->pdata->wire_warm_overheat_thresh;

	}
}

bool sec_usb_thm_overheatlimit(struct sec_battery_info *battery)
{
#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
	int gap = 0;
	int bat_thm = battery->temperature;
#endif

	if (battery->pdata->usb_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE) {
		pr_err("%s: USB_THM, Invalid Temp Check Type, usb_thm <- bat_thm\n", __func__);
		battery->usb_temp = battery->temperature;
	}

	if (battery->usb_thm_status == USB_THM_NORMAL) {
#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
#if defined(CONFIG_DUAL_BATTERY)
		/* select low temp thermistor */
		if (battery->temperature > battery->sub_bat_temp)
			bat_thm = battery->sub_bat_temp;
#endif
		if (battery->usb_temp > bat_thm)
			gap = battery->usb_temp - bat_thm;
#endif

		if (battery->usb_temp >= battery->overheatlimit_threshold) {
			pr_info("%s: Usb Temp over than %d (usb_thm : %d)\n", __func__,
					battery->overheatlimit_threshold, battery->usb_temp);
			battery->usb_thm_status = USB_THM_OVERHEATLIMIT;
			return true;
#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
		} else if ((battery->usb_temp >= battery->usb_protection_temp) &&
				(gap >= battery->temp_gap_bat_usb)) {
			pr_info("%s: Temp gap between Usb temp and Bat temp : %d\n", __func__, gap);
#if defined(CONFIG_BATTERY_CISD)
			if (gap > battery->cisd.data[CISD_DATA_USB_OVERHEAT_ALONE_PER_DAY])
				battery->cisd.data[CISD_DATA_USB_OVERHEAT_ALONE_PER_DAY] = gap;
#endif
			battery->usb_thm_status = USB_THM_GAP_OVER;
			return true;
#endif
		} else {
			battery->usb_thm_status = USB_THM_NORMAL;
			return false;
		}
	} else if (battery->usb_thm_status == USB_THM_OVERHEATLIMIT) {
		if (battery->usb_temp <= battery->overheatlimit_recovery) {
			battery->usb_thm_status = USB_THM_NORMAL;
			return false;
		} else {
			return true;
		}
#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
	} else if (battery->usb_thm_status == USB_THM_GAP_OVER) {
		if (battery->usb_temp < battery->usb_protection_temp) {
			battery->usb_thm_status = USB_THM_NORMAL;
			return false;
		} else {
			return true;
		}
#endif
	} else {
		battery->usb_thm_status = USB_THM_NORMAL;
		return false;
	}
	return false;

}

#define THERMAL_HYSTERESIS_2	19
#define THERMAL_HYSTERESIS_5	49
void sec_bat_thermal_check(struct sec_battery_info *battery)
{
	int bat_thm = battery->temperature;
	int pre_thermal_zone = battery->thermal_zone;
	int voter_swelling_status = SEC_BAT_CHG_MODE_CHARGING;

#if defined(CONFIG_DUAL_BATTERY)
	bat_thm = sec_bat_get_high_priority_temp(battery);
#endif

	pr_err("%s: co_c3: %d, c3_c2: %d, c2_c1: %d, c1_no: %d, no_wa: %d, wa_ov: %d, tz(%d)\n", __func__,
			battery->cold_cool3_thresh, battery->cool3_cool2_thresh, battery->cool2_cool1_thresh,
			battery->cool1_normal_thresh, battery-> normal_warm_thresh, battery->warm_overheat_thresh,
			battery->thermal_zone);
	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING ||
		battery->skip_swelling) {
		battery->health_change = false;
		pr_debug("%s: DISCHARGING or 15 test mode. stop thermal check\n", __func__);
		sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->fcc_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->fv_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->chgen_vote, VOTER_SWELLING, false, 0);
		return;
	}

	if (battery->pdata->temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE) {
		pr_err("%s: BAT_THM, Invalid Temp Check Type\n", __func__);
		return;
	} else {
		/* COLD - COOL3 - COOL2 - COOL1 - NORMAL - WARM - OVERHEAT - OVERHEATLIMIT*/
		if (sec_usb_thm_overheatlimit(battery)) {
			battery->thermal_zone = BAT_THERMAL_OVERHEATLIMIT;
		} else if (bat_thm >= battery->normal_warm_thresh) {
			if (bat_thm >= battery->warm_overheat_thresh) {
				battery->thermal_zone = BAT_THERMAL_OVERHEAT;
			} else {
				battery->thermal_zone = BAT_THERMAL_WARM;
			}
		} else if (bat_thm  <= battery->cool1_normal_thresh) {
			if (bat_thm <= battery->cold_cool3_thresh) {
				battery->thermal_zone = BAT_THERMAL_COLD;
			} else if (bat_thm <= battery->cool3_cool2_thresh) {
				battery->thermal_zone = BAT_THERMAL_COOL3;
			} else if (bat_thm <= battery->cool2_cool1_thresh) {
				battery->thermal_zone = BAT_THERMAL_COOL2;
			} else {
				battery->thermal_zone = BAT_THERMAL_COOL1;
			}
		} else {
			battery->thermal_zone = BAT_THERMAL_NORMAL;
		}
	}

	if (pre_thermal_zone != battery->thermal_zone) {
		battery->bat_thm_count++;

		if (battery->bat_thm_count < battery->pdata->temp_check_count) {
			pr_info("%s : bat_thm_count %d/%d\n", __func__,
					battery->bat_thm_count, battery->pdata->temp_check_count);
			battery->thermal_zone = pre_thermal_zone;
			return;
		}

		pr_info("%s: thermal zone update (%s -> %s), bat_thm(%d), usb_thm(%d)\n", __func__,
				sec_bat_thermal_zone[pre_thermal_zone],
				sec_bat_thermal_zone[battery->thermal_zone], bat_thm, battery->usb_temp);
		battery->health_change = true;
		battery->bat_thm_count = 0;

		pr_info("%s : SAFETY TIME RESET!\n", __func__);
		battery->expired_time = battery->pdata->expired_time;
		battery->prev_safety_time = 0;

		sec_bat_set_threshold(battery);
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SWELLING_MODE);

		switch (battery->thermal_zone) {
		case BAT_THERMAL_OVERHEATLIMIT:
			battery->warm_overheat_thresh -= THERMAL_HYSTERESIS_2;
			battery->normal_warm_thresh -= THERMAL_HYSTERESIS_2;
			if (battery->usb_thm_status == USB_THM_OVERHEATLIMIT) {
				pr_info("%s: USB_THM_OVERHEATLIMIT\n", __func__);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_USB_OVERHEAT_CHARGING]++;
				battery->cisd.data[CISD_DATA_USB_OVERHEAT_CHARGING_PER_DAY]++;
#endif
#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
			} else if (battery->usb_thm_status == USB_THM_GAP_OVER) {
				pr_info("%s: USB_THM_GAP_OVER : %d\n", __func__, gap);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_USB_OVERHEAT_RAPID_CHANGE]++;
				battery->cisd.data[CISD_DATA_USB_OVERHEAT_RAPID_CHANGE_PER_DAY]++;
#endif
#endif
			}

			battery->health = POWER_SUPPLY_HEALTH_OVERHEATLIMIT;
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_BUCK_OFF);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_UNSAFETY_TEMPERATURE]++;
			battery->cisd.data[CISD_DATA_UNSAFE_TEMPERATURE_PER_DAY]++;
#endif

#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
			if (lpcharge) {
				if (is_hv_afc_wire_type(battery->cable_type) && !battery->vbus_limit) {
#if defined(CONFIG_MUIC_HV) || defined(CONFIG_SUPPORT_HV_CTRL)
					battery->vbus_chg_by_siop = SEC_INPUT_VOLTAGE_0V;
					muic_afc_set_voltage(SEC_INPUT_VOLTAGE_0V);
#endif
					battery->vbus_limit = true;
					pr_info("%s: Set AFC TA to 0V\n", __func__);
				} else if (is_pd_wire_type(battery->cable_type)) {
					select_pdo(1);
					pr_info("%s: Set PD TA to PDO 0\n", __func__);
				}
			} else if (is_pd_wire_type(battery->cable_type)) {
				select_pdo(1);
				muic_set_hiccup_mode(1);
			} else {
				muic_set_hiccup_mode(1);
			}
#endif
			break;
		case BAT_THERMAL_OVERHEAT:
			battery->warm_overheat_thresh -= THERMAL_HYSTERESIS_2;
			battery->normal_warm_thresh -= THERMAL_HYSTERESIS_2;
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			battery->health = POWER_SUPPLY_HEALTH_OVERHEAT;
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_UNSAFETY_TEMPERATURE]++;
			battery->cisd.data[CISD_DATA_UNSAFE_TEMPERATURE_PER_DAY]++;
#endif
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			battery->swelling_mode = true;
			battery->usb_thm_status = USB_THM_NORMAL;
			break;
		case BAT_THERMAL_WARM:
			battery->normal_warm_thresh -= THERMAL_HYSTERESIS_2;
			if (is_wireless_type(battery->cable_type) ||
					battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE) {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wireless_warm_current);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_WC_HIGH_TEMP_SWELLING]++;
				battery->cisd.data[CISD_DATA_WC_HIGH_TEMP_SWELLING_PER_DAY]++;
#endif
			} else {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wire_warm_current);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_HIGH_TEMP_SWELLING]++;
				battery->cisd.data[CISD_DATA_HIGH_TEMP_SWELLING_PER_DAY]++;
#endif
			}
			sec_vote(battery->topoff_vote, VOTER_SWELLING, true, battery->pdata->high_temp_topoff);
			if (battery->voltage_now > battery->pdata->high_temp_float) {
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			} else {
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
				sec_vote(battery->fv_vote, VOTER_SWELLING, true, battery->pdata->high_temp_float);
			}
			battery->health = POWER_SUPPLY_HEALTH_GOOD;
			if (battery->capacity >= 100)
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_FULL);
			else
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			battery->swelling_mode = true;
			battery->usb_thm_status = USB_THM_NORMAL;
			break;
		case BAT_THERMAL_COOL1:
			battery->cool1_normal_thresh += THERMAL_HYSTERESIS_2;
			if (is_wireless_type(battery->cable_type) ||
					battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE) {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wireless_cool1_current);
			} else {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wire_cool1_current);
			}
			sec_vote(battery->fv_vote, VOTER_SWELLING, true, battery->pdata->low_temp_float);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, true, battery->pdata->low_temp_topoff);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			battery->health = POWER_SUPPLY_HEALTH_GOOD;
			if (battery->capacity >= 100)
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_FULL);
			else
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			battery->swelling_mode = true;
			battery->usb_thm_status = USB_THM_NORMAL;
			break;
		case BAT_THERMAL_COOL2:
			battery->cool2_cool1_thresh += THERMAL_HYSTERESIS_2;
			battery->cool1_normal_thresh += THERMAL_HYSTERESIS_2;
			if (is_wireless_type(battery->cable_type) ||
					battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE) {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wireless_cool2_current);
			} else {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wire_cool2_current);
			}
			sec_vote(battery->fv_vote, VOTER_SWELLING, true, battery->pdata->low_temp_float);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, true, battery->pdata->low_temp_topoff);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			battery->health = POWER_SUPPLY_HEALTH_GOOD;
			if (battery->capacity >= 100)
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_FULL);
			else
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_2ND,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			battery->swelling_mode = true;
			battery->usb_thm_status = USB_THM_NORMAL;
			break;
		case BAT_THERMAL_COOL3:
			battery->cool3_cool2_thresh += THERMAL_HYSTERESIS_2;
			battery->cool2_cool1_thresh += THERMAL_HYSTERESIS_2;
			battery->cool1_normal_thresh += THERMAL_HYSTERESIS_2;
			if (is_wireless_type(battery->cable_type) ||
					battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE) {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wireless_cool3_current);
			} else {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wire_cool3_current);
			}
			sec_vote(battery->fv_vote, VOTER_SWELLING, true, battery->pdata->low_temp_float);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, true, battery->pdata->low_temp_topoff);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			battery->health = POWER_SUPPLY_HEALTH_GOOD;
			if (battery->capacity >= 100)
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_FULL);
			else
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			battery->swelling_mode = true;
			battery->usb_thm_status = USB_THM_NORMAL;
			break;
		case BAT_THERMAL_COLD:
			battery->cold_cool3_thresh += THERMAL_HYSTERESIS_2;
			battery->cool3_cool2_thresh += THERMAL_HYSTERESIS_2;
			battery->cool2_cool1_thresh += THERMAL_HYSTERESIS_2;
			battery->cool1_normal_thresh += THERMAL_HYSTERESIS_2;
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			battery->health = POWER_SUPPLY_HEALTH_COLD;
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_UNSAFETY_TEMPERATURE]++;
			battery->cisd.data[CISD_DATA_UNSAFE_TEMPERATURE_PER_DAY]++;
#endif
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			battery->swelling_mode = true;
			battery->usb_thm_status = USB_THM_NORMAL;
			break;
		case BAT_THERMAL_NORMAL:
		default:
			sec_vote(battery->fcc_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->fv_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, false, 0);
			battery->health = POWER_SUPPLY_HEALTH_GOOD;
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
			battery->swelling_mode = false;
			battery->usb_thm_status = USB_THM_NORMAL;
			break;
		}
	} else { /* pre_thermal_zone == battery->thermal_zone */
		battery->health_change = false;

		switch (battery->thermal_zone) {
		case BAT_THERMAL_WARM:
			if (battery->health == POWER_SUPPLY_HEALTH_GOOD) {
				if (get_sec_voter_status(battery->chgen_vote, VOTER_SWELLING, &voter_swelling_status) < 0){
					pr_info("%s: There is no enabled voter.\n", __func__);
					return;
				}

				if (voter_swelling_status == SEC_BAT_CHG_MODE_CHARGING) {
					if (sec_bat_check_full(battery, battery->pdata->full_check_type)) {
						pr_info("%s: battery thermal zone WARM. Full charged.\n", __func__);
						sec_vote(battery->chgen_vote, VOTER_SWELLING, true,
								SEC_BAT_CHG_MODE_CHARGING_OFF);
					}
				} else if (voter_swelling_status == SEC_BAT_CHG_MODE_CHARGING_OFF) {
					if (battery->voltage_now <= battery->pdata->swelling_high_rechg_voltage) {
						pr_info("%s: thermal zone WARM. charging recovery. Vnow: %d\n",
								__func__, battery->voltage_now);
						sec_vote(battery->fv_vote, VOTER_SWELLING, true,
								battery->pdata->high_temp_float);
						sec_vote(battery->chgen_vote, VOTER_SWELLING, true,
								SEC_BAT_CHG_MODE_CHARGING);
					}
				}
			}

			break;
		default:
			break;
		}
	}

	return;
}
