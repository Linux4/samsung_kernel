/*
 * sec_battery_thermal.c
 * Samsung Mobile Battery Driver
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
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

#define THERMAL_HYSTERESIS_2	19

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
int sec_bat_get_high_priority_temp(struct sec_battery_info *battery)
{
	int priority_temp = battery->temperature;
	int standard_temp = 250;

	if (battery->pdata->sub_bat_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return battery->temperature;

	if ((battery->temperature > standard_temp) && (battery->sub_bat_temp > standard_temp)) {
		if (battery->temperature < battery->sub_bat_temp)
			priority_temp = battery->sub_bat_temp;
	} else {
		if (battery->temperature > battery->sub_bat_temp)
			priority_temp = battery->sub_bat_temp;
	}

	pr_info("%s priority_temp = %d\n", __func__, priority_temp);
	return priority_temp;
}
#endif

void sec_bat_check_mix_temp(struct sec_battery_info *battery)
{
	int temperature = battery->temperature;
	int chg_temp;
	int input_current = 0;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	bool apdo_status = (is_pd_apdo_wire_type(battery->wire_status) && battery->pd_list.now_isApdo) ? 1 : 0;
#endif

	if (battery->pdata->temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE ||
			battery->pdata->chg_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return;

	if (battery->pdata->blkt_temp_check_type)
		temperature = battery->blkt_temp;


#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	if (apdo_status)
		chg_temp = battery->dchg_temp;
	else
		chg_temp = battery->chg_temp;
#else
	chg_temp = battery->chg_temp;
#endif

	if (battery->siop_level >= 100 && !battery->lcd_status && is_not_wireless_type(battery->cable_type)) {
		if ((!battery->mix_limit && (temperature >= battery->pdata->mix_high_temp) &&
					(chg_temp >= battery->pdata->mix_high_chg_temp)) ||
			(battery->mix_limit && (temperature > battery->pdata->mix_high_temp_recovery))) {
			int max_input_current = battery->pdata->full_check_current_1st + 50;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
			/* for checking charging source (DC -> SC) */
			if (battery->pdata->blkt_temp_check_type && apdo_status && !battery->mix_limit) {
				battery->mix_limit = true;
				sec_vote_refresh(battery->fcc_vote);
				return;
			}
#endif
			/* input current = float voltage * (topoff_current_1st + 50mA(margin)) / (vbus_level * 0.9) */
			input_current = ((battery->pdata->chg_float_voltage / battery->pdata->chg_float_voltage_conv) *
					max_input_current) / ((battery->input_voltage * 9) / 10);

			if (input_current > max_input_current)
				input_current = max_input_current;

			battery->mix_limit = true;
			/* skip other heating control */
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL,
					SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL);
			sec_vote(battery->input_vote, VOTER_MIX_LIMIT, true, input_current);

#if IS_ENABLED(CONFIG_WIRELESS_TX_MODE)
			if (battery->wc_tx_enable) {
				pr_info("%s @Tx_Mode enter mix_temp_limit, TX mode should turn off\n", __func__);
				sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP,
						BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP);
				battery->tx_retry_case |= SEC_BAT_TX_RETRY_MIX_TEMP;
				sec_wireless_set_tx_enable(battery, false);
			}
#endif
		} else if (battery->mix_limit) {
			battery->mix_limit = false;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
			/* for checking charging source (SC -> DC) */
			if (battery->pdata->blkt_temp_check_type)
				sec_vote_refresh(battery->fcc_vote);
#endif
			sec_vote(battery->input_vote, VOTER_MIX_LIMIT, false, 0);

			if (battery->tx_retry_case & SEC_BAT_TX_RETRY_MIX_TEMP) {
				pr_info("%s @Tx_Mode recovery mix_temp_limit, TX mode should be retried\n", __func__);
				if ((battery->tx_retry_case & ~SEC_BAT_TX_RETRY_MIX_TEMP) == 0)
					sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_RETRY,
							BATT_TX_EVENT_WIRELESS_TX_RETRY);
				battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_MIX_TEMP;
			}
		}

		pr_info("%s: mix_limit(%d), temp(%d), chg_temp(%d), input_current(%d)\n",
			__func__, battery->mix_limit, temperature, chg_temp, get_sec_vote_result(battery->input_vote));
	} else {
		if (battery->mix_limit) {
			battery->mix_limit = false;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
			/* for checking charging source (SC -> DC) */
			if (battery->pdata->blkt_temp_check_type)
				sec_vote_refresh(battery->fcc_vote);
#endif
			sec_vote(battery->input_vote, VOTER_MIX_LIMIT, false, 0);
		}
	}
}

int sec_bat_get_temp_by_temp_control_source(struct sec_battery_info *battery, int tcs)
{
	switch (tcs) {
	case TEMP_CONTROL_SOURCE_CHG_THM:
		return battery->chg_temp;
	case TEMP_CONTROL_SOURCE_USB_THM:
		return battery->usb_temp;
	case TEMP_CONTROL_SOURCE_WPC_THM:
		return battery->wpc_temp;
	case TEMP_CONTROL_SOURCE_NONE:
	case TEMP_CONTROL_SOURCE_BAT_THM:
	default:
		return battery->temperature;
	}
}

#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
void sec_bat_check_wpc_temp(struct sec_battery_info *battery)
{
	int wpc_high_temp = 0, wpc_high_temp_recovery = 0;
	int input_current = INT_MAX, charging_current = INT_MAX;
	bool do_wpc_step_limit = false;

	if (battery->pdata->wpc_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return;

	if (is_wireless_type(battery->cable_type)) {
		union power_supply_propval value = {0, };
		int wpc_vout_level = 0;

		if (battery->cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20) {
			wpc_vout_level = battery->wpc_max_vout_level;
			wpc_high_temp = battery->pdata->wpc_high_temp;
			wpc_high_temp_recovery = battery->pdata->wpc_high_temp_recovery;
		} else {
			wpc_vout_level = WIRELESS_VOUT_10V;
			wpc_high_temp = battery->pdata->non_wc20_wpc_high_temp;
			wpc_high_temp_recovery = battery->pdata->non_wc20_wpc_high_temp_recovery;
		}

		mutex_lock(&battery->voutlock);

		/* get vout level */
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_RX_VOUT, value);

		if (is_hv_wireless_type(battery->cable_type) &&
			value.intval == WIRELESS_VOUT_5_5V_STEP &&
			battery->wpc_vout_level != WIRELESS_VOUT_5_5V_STEP) {
			pr_info("%s: real vout was not 10V\n", __func__);
			battery->wpc_vout_level = WIRELESS_VOUT_5_5V_STEP;
		}

		if (battery->siop_level >= 100 && !battery->lcd_status) {
			int temp_val = sec_bat_get_temp_by_temp_control_source(battery,
				battery->pdata->wpc_temp_control_source);

			if ((!battery->chg_limit && temp_val >= wpc_high_temp) ||
				(battery->chg_limit && temp_val > wpc_high_temp_recovery)) {
				battery->chg_limit = true;
				if (input_current > battery->pdata->wpc_input_limit_current) {
					if (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_TX &&
						battery->pdata->wpc_input_limit_by_tx_check)
						input_current = battery->pdata->wpc_input_limit_current_by_tx;
					else
						input_current = battery->pdata->wpc_input_limit_current;
				}
				if (charging_current > battery->pdata->wpc_charging_limit_current)
					charging_current = battery->pdata->wpc_charging_limit_current;
				wpc_vout_level = WIRELESS_VOUT_5_5V_STEP;
			} else if (battery->chg_limit) {
				battery->chg_limit = false;
			} else if (temp_val >= wpc_high_temp_recovery) {
				int i;

				for (i = 0; i < battery->pdata->wpc_step_limit_size; ++i) {
					if (temp_val > battery->pdata->wpc_step_limit_temp[i]) {
						if (battery->wc20_rx_power == SEC_WIRELESS_RX_POWER_12W)
							charging_current = battery->pdata->wpc_step_limit_fcc_12w[i];
						else if (battery->wc20_rx_power >= SEC_WIRELESS_RX_POWER_15W)
							charging_current = battery->pdata->wpc_step_limit_fcc_15w[i];
						else
							charging_current = battery->pdata->wpc_step_limit_fcc[i];
						pr_info("%s: wc20_rx_power(%d), wpc_step_limit[%d] temp_val:%d, fcc:%d\n",
								__func__, battery->wc20_rx_power, i, temp_val, charging_current);
						do_wpc_step_limit = true;
						break;
					}
				}
			}
		} else {
			if ((is_hv_wireless_type(battery->cable_type) &&
				battery->cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE) ||
				battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV ||
				battery->cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20) {
				int temp_val = sec_bat_get_temp_by_temp_control_source(battery,
					battery->pdata->wpc_temp_lcd_on_control_source);

				if ((!battery->chg_limit &&
						temp_val >= battery->pdata->wpc_lcd_on_high_temp) ||
					(battery->chg_limit &&
						temp_val > battery->pdata->wpc_lcd_on_high_temp_rec)) {
					if (input_current > battery->pdata->wpc_lcd_on_input_limit_current)
						input_current = battery->pdata->wpc_lcd_on_input_limit_current;
					if (charging_current > battery->pdata->wpc_charging_limit_current)
						charging_current = battery->pdata->wpc_charging_limit_current;
					battery->chg_limit = true;
					wpc_vout_level = WIRELESS_VOUT_5_5V_STEP;
				} else if (battery->chg_limit) {
					battery->chg_limit = false;
				}
			} else if (battery->chg_limit) {
				battery->chg_limit = false;
			}
		}

		if (is_hv_wireless_type(battery->cable_type)) {
#if defined(CONFIG_ISDB_CHARGING_CONTROL)
			if ((battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING) ||
				(battery->current_event & SEC_BAT_CURRENT_EVENT_ISDB) ||
				battery->sleep_mode)
#else
			if (battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING ||
				battery->sleep_mode)
#endif
				wpc_vout_level = WIRELESS_VOUT_5_5V_STEP;

			if (wpc_vout_level != battery->wpc_vout_level) {
				battery->wpc_vout_level = wpc_vout_level;
				if (battery->current_event & SEC_BAT_CURRENT_EVENT_WPC_VOUT_LOCK) {
					pr_info("%s: block to set wpc vout level(%d) because otg on\n",
						__func__, wpc_vout_level);
				} else {
					value.intval = wpc_vout_level;
					psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION, value);
					pr_info("%s: change vout level(%d)",
						__func__, battery->wpc_vout_level);
					sec_vote(battery->input_vote, VOTER_AICL, false, 0);
				}
			} else if ((battery->wpc_vout_level == WIRELESS_VOUT_10V ||
						battery->wpc_vout_level == battery->wpc_max_vout_level)
						&& !battery->chg_limit)
				/* reset aicl current to recover current for unexpected aicl during */
				/* before vout boosting completion */
				sec_vote(battery->input_vote, VOTER_AICL, false, 0);
		}
		mutex_unlock(&battery->voutlock);
		pr_info("%s: vout_level(%d), chg_limit(%d)\n",
			__func__, battery->wpc_vout_level, battery->chg_limit);
		if (battery->chg_limit) {
			if ((battery->siop_level >= 100 && !battery->lcd_status) &&
					(input_current > battery->pdata->wpc_input_limit_current))
				input_current = battery->pdata->wpc_input_limit_current;
			else if ((battery->siop_level < 100 || battery->lcd_status) &&
					(input_current > battery->pdata->wpc_lcd_on_input_limit_current))
				input_current = battery->pdata->wpc_lcd_on_input_limit_current;
		}
		if (input_current != INT_MAX) {
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
		} else if (do_wpc_step_limit) {
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
		} else {
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
		}
	}
}
#endif

#if defined(CONFIG_WIRELESS_TX_MODE)
void sec_bat_check_tx_temperature(struct sec_battery_info *battery)
{
	if (battery->wc_tx_enable) {
		if (battery->temperature >= battery->pdata->tx_high_threshold) {
			pr_info("@Tx_Mode : %s: Battery temperature is too high. Tx mode should turn off\n", __func__);
			/* set tx event */
			sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP, BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP);
			battery->tx_retry_case |= SEC_BAT_TX_RETRY_HIGH_TEMP;
			sec_wireless_set_tx_enable(battery, false);
		} else if (battery->temperature <= battery->pdata->tx_low_threshold) {
			pr_info("@Tx_Mode : %s: Battery temperature is too low. Tx mode should turn off\n", __func__);
			/* set tx event */
			sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_LOW_TEMP, BATT_TX_EVENT_WIRELESS_TX_LOW_TEMP);
			battery->tx_retry_case |= SEC_BAT_TX_RETRY_LOW_TEMP;
			sec_wireless_set_tx_enable(battery, false);
		}
	} else if (battery->tx_retry_case & SEC_BAT_TX_RETRY_HIGH_TEMP) {
		if (battery->temperature <= battery->pdata->tx_high_recovery) {
			pr_info("@Tx_Mode : %s: Battery temperature goes to normal(High). Retry TX mode\n", __func__);
			battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_HIGH_TEMP;
			if (!battery->tx_retry_case)
				sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_RETRY, BATT_TX_EVENT_WIRELESS_TX_RETRY);
		}
	} else if (battery->tx_retry_case & SEC_BAT_TX_RETRY_LOW_TEMP) {
		if (battery->temperature >= battery->pdata->tx_low_recovery) {
			pr_info("@Tx_Mode : %s: Battery temperature goes to normal(Low). Retry TX mode\n", __func__);
			battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_LOW_TEMP;
			if (!battery->tx_retry_case)
				sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_RETRY, BATT_TX_EVENT_WIRELESS_TX_RETRY);
		}
	}
}
#endif

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
void sec_bat_check_direct_chg_temp(struct sec_battery_info *battery)
{
	int input_current = 0, charging_current = 0;

	if (battery->pdata->dchg_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return;

	if (battery->siop_level >= 100 && !battery->lcd_status) {
		if (!battery->chg_limit && battery->pd_list.now_isApdo &&
			((battery->dchg_temp >= battery->pdata->dchg_high_temp) ||
			(battery->temperature >= battery->pdata->dchg_high_batt_temp))) {
			input_current = battery->pdata->dchg_input_limit_current;
			charging_current = battery->pdata->dchg_charging_limit_current;
			battery->chg_limit = true;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
		} else if (!battery->chg_limit && (!battery->pd_list.now_isApdo) &&
			(battery->chg_temp >= battery->pdata->chg_high_temp)) {
			if (battery->input_voltage == SEC_INPUT_VOLTAGE_5V) {
				input_current = battery->pdata->default_input_current;
				charging_current = battery->pdata->default_charging_current;
			} else {
				input_current = battery->pdata->chg_input_limit_current;
				charging_current = battery->pdata->chg_charging_limit_current;
			}
			battery->chg_limit = true;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
		} else if (battery->chg_limit) {
			if (((battery->dchg_temp <= battery->pdata->dchg_high_temp_recovery) &&
				(battery->temperature <= battery->pdata->dchg_high_batt_temp_recovery) &&
				battery->pd_list.now_isApdo) || ((battery->chg_temp <= battery->pdata->chg_high_temp_recovery) &&
				(!battery->pd_list.now_isApdo))) {
				battery->chg_limit = false;
				sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
				sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
			} else {
				if (battery->pd_list.now_isApdo) {
					input_current = battery->pdata->dchg_input_limit_current;
					charging_current = battery->pdata->dchg_charging_limit_current;
				} else {
					if (battery->input_voltage == SEC_INPUT_VOLTAGE_5V) {
						input_current = battery->pdata->default_input_current;
						charging_current = battery->pdata->default_charging_current;
					} else {
						input_current = battery->pdata->chg_input_limit_current;
						charging_current = battery->pdata->chg_charging_limit_current;
					}
				}
				battery->chg_limit = true;
				sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
				sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
			}
		}
		pr_info("%s: cable_type(%d), chg_limit(%d)\n", __func__,
			battery->cable_type, battery->chg_limit);
	}
}
#endif

void sec_bat_check_pdic_temp(struct sec_battery_info *battery)
{
	int input_current, charging_current;

	if (battery->pdata->chg_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return;

	if (battery->pdic_ps_rdy && battery->siop_level >= 100 && !battery->lcd_status) {
		if ((!battery->chg_limit && (battery->chg_temp >= battery->pdata->chg_high_temp)) ||
			(battery->chg_limit && (battery->chg_temp >= battery->pdata->chg_high_temp_recovery))) {
			input_current =
				(battery->pdata->chg_input_limit_current * SEC_INPUT_VOLTAGE_9V) / battery->input_voltage;
			charging_current = battery->pdata->chg_charging_limit_current;
			battery->chg_limit = true;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
		} else if (battery->chg_limit && battery->chg_temp <= battery->pdata->chg_high_temp_recovery) {
			battery->chg_limit = false;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
		}
		pr_info("%s: cable_type(%d), chg_limit(%d)\n", __func__,
			battery->cable_type, battery->chg_limit);
	} else if (battery->chg_limit) {
		battery->chg_limit = false;
		sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
		sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
	}
}

void sec_bat_check_afc_temp(struct sec_battery_info *battery)
{
	int input_current = 0, charging_current = 0;

	if (battery->pdata->chg_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return;

	if (battery->siop_level >= 100 && !battery->lcd_status) {
#if defined(CONFIG_SUPPORT_HV_CTRL)
		if (!battery->chg_limit && is_hv_wire_type(battery->cable_type) &&
				(battery->chg_temp >= battery->pdata->chg_high_temp)) {
			input_current = battery->pdata->chg_input_limit_current;
			charging_current = battery->pdata->chg_charging_limit_current;
			battery->chg_limit = true;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
		} else if (!battery->chg_limit && battery->max_charge_power >= (battery->pdata->pd_charging_charge_power - 500) &&
				(battery->chg_temp >= battery->pdata->chg_high_temp)) {
			input_current = battery->pdata->default_input_current;
			charging_current = battery->pdata->default_charging_current;
			battery->chg_limit = true;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
		} else if (battery->chg_limit && is_hv_wire_type(battery->cable_type)) {
			if (battery->chg_temp <= battery->pdata->chg_high_temp_recovery) {
				battery->chg_limit = false;
				sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
				sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
			}
		} else if (battery->chg_limit && battery->max_charge_power >= (battery->pdata->pd_charging_charge_power - 500)) {
			if (battery->chg_temp <= battery->pdata->chg_high_temp_recovery) {
				battery->chg_limit = false;
				sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
				sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
			}
		}
		pr_info("%s: cable_type(%d), chg_limit(%d)\n", __func__,
			battery->cable_type, battery->chg_limit);
#else
		if ((!battery->chg_limit && is_hv_wire_type(battery->cable_type) &&
					(battery->chg_temp >= battery->pdata->chg_high_temp)) ||
			(battery->chg_limit && is_hv_wire_type(battery->cable_type) &&
			 (battery->chg_temp > battery->pdata->chg_high_temp_recovery))) {
			if (!battery->chg_limit) {
				input_current = battery->pdata->chg_input_limit_current;
				charging_current = battery->pdata->chg_charging_limit_current;
				battery->chg_limit = true;
				sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
				sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
			}
		} else if (battery->chg_limit && is_hv_wire_type(battery->cable_type) &&
				(battery->chg_temp <= battery->pdata->chg_high_temp_recovery)) {
			battery->chg_limit = false;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
		}
#endif
	} else if (battery->chg_limit) {
		battery->chg_limit = false;
		sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
		sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
	}
}

void sec_bat_set_threshold(struct sec_battery_info *battery, int cable_type)
{
	if (is_wireless_fake_type(cable_type)) {
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

	switch (battery->thermal_zone) {
	case BAT_THERMAL_OVERHEATLIMIT:
		battery->warm_overheat_thresh -= THERMAL_HYSTERESIS_2;
		battery->normal_warm_thresh -= THERMAL_HYSTERESIS_2;
		break;
	case BAT_THERMAL_OVERHEAT:
		battery->warm_overheat_thresh -= THERMAL_HYSTERESIS_2;
		battery->normal_warm_thresh -= THERMAL_HYSTERESIS_2;
		break;
	case BAT_THERMAL_WARM:
		battery->normal_warm_thresh -= THERMAL_HYSTERESIS_2;
		break;
	case BAT_THERMAL_COOL1:
		battery->cool1_normal_thresh += THERMAL_HYSTERESIS_2;
		break;
	case BAT_THERMAL_COOL2:
		battery->cool2_cool1_thresh += THERMAL_HYSTERESIS_2;
		battery->cool1_normal_thresh += THERMAL_HYSTERESIS_2;
		break;
	case BAT_THERMAL_COOL3:
		battery->cool3_cool2_thresh += THERMAL_HYSTERESIS_2;
		battery->cool2_cool1_thresh += THERMAL_HYSTERESIS_2;
		battery->cool1_normal_thresh += THERMAL_HYSTERESIS_2;
		break;
	case BAT_THERMAL_COLD:
		battery->cold_cool3_thresh += THERMAL_HYSTERESIS_2;
		battery->cool3_cool2_thresh += THERMAL_HYSTERESIS_2;
		battery->cool2_cool1_thresh += THERMAL_HYSTERESIS_2;
		battery->cool1_normal_thresh += THERMAL_HYSTERESIS_2;
		break;
	case BAT_THERMAL_NORMAL:
	default:
		break;
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
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
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

void sec_bat_thermal_check(struct sec_battery_info *battery)
{
	int bat_thm = battery->temperature;
	int pre_thermal_zone = battery->thermal_zone;
	int voter_status = SEC_BAT_CHG_MODE_CHARGING;
#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
	int gap = 0;
#endif

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	union power_supply_propval val = {0, };

	bat_thm = sec_bat_get_high_priority_temp(battery);
#endif

#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
	if (battery->usb_temp > bat_thm)
		gap = battery->usb_temp - bat_thm;
#endif

	pr_err("%s: co_c3: %d, c3_c2: %d, c2_c1: %d, c1_no: %d, no_wa: %d, wa_ov: %d, tz(%s)\n", __func__,
			battery->cold_cool3_thresh, battery->cool3_cool2_thresh, battery->cool2_cool1_thresh,
			battery->cool1_normal_thresh, battery->normal_warm_thresh, battery->warm_overheat_thresh,
			sec_bat_thermal_zone[battery->thermal_zone]);

	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING ||
		battery->skip_swelling) {
		battery->health_change = false;
		pr_debug("%s: DISCHARGING or 15 test mode. stop thermal check\n", __func__);
		battery->thermal_zone = BAT_THERMAL_NORMAL;
		sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->fcc_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->fv_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->chgen_vote, VOTER_SWELLING, false, 0);
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
		sec_bat_set_threshold(battery, battery->cable_type);
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
		} else if (bat_thm <= battery->cool1_normal_thresh) {
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

		sec_bat_set_threshold(battery, battery->cable_type);
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SWELLING_MODE);

		switch (battery->thermal_zone) {
		case BAT_THERMAL_OVERHEATLIMIT:
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

			sec_bat_set_health(battery, POWER_SUPPLY_EXT_HEALTH_OVERHEATLIMIT);
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_BUCK_OFF);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_UNSAFETY_TEMPERATURE]++;
			battery->cisd.data[CISD_DATA_UNSAFE_TEMPERATURE_PER_DAY]++;
#endif

			sec_vote(battery->iv_vote, VOTER_MUIC_ABNORMAL, true, SEC_INPUT_VOLTAGE_5V);
			if (!sec_bat_get_lpmode()) {
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
				muic_set_hiccup_mode(1);
#endif
				if (is_pd_wire_type(battery->cable_type))
					sec_pd_manual_ccopen_req(1);
			}
			break;
		case BAT_THERMAL_OVERHEAT:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			if (battery->voltage_now > battery->pdata->high_temp_float)
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			else
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_OVERHEAT);
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_UNSAFETY_TEMPERATURE]++;
			battery->cisd.data[CISD_DATA_UNSAFE_TEMPERATURE_PER_DAY]++;
#endif
			break;
		case BAT_THERMAL_WARM:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			if (battery->voltage_now > battery->pdata->high_temp_float) {
				if (battery->wc_tx_enable &&
					(is_hv_wire_type(battery->cable_type) ||
					is_pd_wire_type(battery->cable_type))) {
					sec_vote(battery->chgen_vote, VOTER_SWELLING, true,
							SEC_BAT_CHG_MODE_CHARGING_OFF);
				} else {
					sec_vote(battery->chgen_vote, VOTER_SWELLING,
						true, SEC_BAT_CHG_MODE_BUCK_OFF);
				}
			} else if (battery->voltage_now > battery->pdata->swelling_high_rechg_voltage) {
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			} else {
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			}

			if (is_wireless_fake_type(battery->cable_type)) {
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
			sec_vote(battery->fv_vote, VOTER_SWELLING, true, battery->pdata->high_temp_float);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, true, battery->pdata->full_check_current_2nd);

			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			break;
		case BAT_THERMAL_COOL1:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL1,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			if (is_wireless_fake_type(battery->cable_type)) {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wireless_cool1_current);
			} else {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wire_cool1_current);
			}
			sec_vote(battery->fv_vote, VOTER_SWELLING, true, battery->pdata->low_temp_float);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			break;
		case BAT_THERMAL_COOL2:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL2,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			if (is_wireless_fake_type(battery->cable_type)) {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wireless_cool2_current);
			} else {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wire_cool2_current);
			}
			sec_vote(battery->fv_vote, VOTER_SWELLING, true, battery->pdata->low_temp_float);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			break;
		case BAT_THERMAL_COOL3:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL3,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			if (is_wireless_fake_type(battery->cable_type)) {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wireless_cool3_current);
			} else {
				sec_vote(battery->fcc_vote, VOTER_SWELLING, true, battery->pdata->wire_cool3_current);
			}
			sec_vote(battery->fv_vote, VOTER_SWELLING, true, battery->pdata->low_temp_float);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, true, battery->pdata->full_check_current_2nd);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			break;
		case BAT_THERMAL_COLD:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL3,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_COLD);
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_UNSAFETY_TEMPERATURE]++;
			battery->cisd.data[CISD_DATA_UNSAFE_TEMPERATURE_PER_DAY]++;
#endif
			break;
		case BAT_THERMAL_NORMAL:
		default:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_vote(battery->fcc_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->fv_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, false, 0);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			break;
		}
		if ((battery->thermal_zone >= BAT_THERMAL_COOL3) && (battery->thermal_zone <= BAT_THERMAL_WARM)) {
#if defined(CONFIG_ENABLE_FULL_BY_SOC)
			if ((battery->capacity >= 100) || (battery->status == POWER_SUPPLY_STATUS_FULL))
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_FULL);
			else
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
#else
			if (battery->status != POWER_SUPPLY_STATUS_FULL)
				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
#endif
		}
	} else { /* pre_thermal_zone == battery->thermal_zone */
		battery->health_change = false;

		switch (battery->thermal_zone) {
		case BAT_THERMAL_OVERHEAT:
			/* does not need buck control at low temperatures */
			if (battery->health == POWER_SUPPLY_HEALTH_OVERHEAT) {
				int v_ref = battery->pdata->high_temp_float - battery->pdata->buck_recovery_margin;

				if (get_sec_voter_status(battery->chgen_vote, VOTER_SWELLING, &voter_status) < 0)
					pr_err("%s: INVALID VOTER ID\n", __func__);
				pr_info("%s: voter_status: %d\n", __func__, voter_status);
				if ((voter_status == SEC_BAT_CHG_MODE_BUCK_OFF) &&
					(battery->voltage_now < v_ref)) {
					pr_info("%s: Vnow(%dmV) < %dmV, buck on\n", __func__,
						battery->voltage_now, v_ref);
					sec_vote(battery->chgen_vote, VOTER_SWELLING, true,
							SEC_BAT_CHG_MODE_CHARGING_OFF);
				}
			}
			break;
		case BAT_THERMAL_WARM:
			if (battery->health == POWER_SUPPLY_HEALTH_GOOD) {
				int v_ref = battery->pdata->high_temp_float - battery->pdata->buck_recovery_margin;

				if (get_sec_voter_status(battery->chgen_vote, VOTER_SWELLING, &voter_status) < 0)
					pr_err("%s: INVALID VOTER ID\n", __func__);
				pr_info("%s: voter_status: %d\n", __func__, voter_status);
				if (voter_status == SEC_BAT_CHG_MODE_CHARGING) {
					if (sec_bat_check_fullcharged(battery)) {
						pr_info("%s: battery thermal zone WARM. Full charged.\n", __func__);
						sec_vote(battery->chgen_vote, VOTER_SWELLING, true,
								SEC_BAT_CHG_MODE_CHARGING_OFF);
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
						/* Enable supplement mode for swelling full charging done, should cut off charger then limiter sequence */
						val.intval = 1;
						psy_do_property(battery->pdata->dual_battery_name, set,
							POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, val);
#endif
					}
				} else if ((voter_status == SEC_BAT_CHG_MODE_CHARGING_OFF ||
						voter_status == SEC_BAT_CHG_MODE_BUCK_OFF) &&
						(battery->voltage_now <= battery->pdata->swelling_high_rechg_voltage)) {
					pr_info("%s: thermal zone WARM. charging recovery. Vnow: %d\n",
							__func__, battery->voltage_now);
					battery->expired_time = battery->pdata->expired_time;
					battery->prev_safety_time = 0;
					sec_vote(battery->fv_vote, VOTER_SWELLING, true,
							battery->pdata->high_temp_float);
					sec_vote(battery->chgen_vote, VOTER_FULL_CHARGE, false, 0);
					sec_vote(battery->chgen_vote, VOTER_SWELLING, true,
							SEC_BAT_CHG_MODE_CHARGING);
				} else if (voter_status == SEC_BAT_CHG_MODE_BUCK_OFF &&
						battery->voltage_now < v_ref) {
					pr_info("%s: Vnow(%dmV) < %dmV, buck on\n", __func__,
						battery->voltage_now, v_ref);
					sec_vote(battery->chgen_vote, VOTER_SWELLING, true,
							SEC_BAT_CHG_MODE_CHARGING_OFF);
				}
			}
			break;
		default:
			break;
		}
	}

	return;
}
