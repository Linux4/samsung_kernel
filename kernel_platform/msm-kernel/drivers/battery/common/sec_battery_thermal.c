/*
 * sec_battery_thermal.c
 * Samsung Mobile Battery Driver
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"
#include "battery_logger.h"

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER) && !defined(CONFIG_SEC_FACTORY)
extern int muic_set_hiccup_mode(int on_off);
#endif
#if defined(CONFIG_SEC_KUNIT)
#include <kunit/mock.h>
#else
#define __visible_for_testing static
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

const char *sec_usb_thm_str(int usb_thm_sts)
{
	switch (usb_thm_sts) {
	case USB_THM_NORMAL:
		return "NORMAL";
	case USB_THM_OVERHEATLIMIT:
		return "OVERHEATLIMIT";
	case USB_THM_GAP_OVER1:
		return "GAP_OVER1";
	case USB_THM_GAP_OVER2:
		return "GAP_OVER2";
	default:
		return "UNKNOWN";
	}
}

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
int sec_bat_get_high_priority_temp(struct sec_battery_info *battery)
{
	int priority_temp = battery->temperature;
	int standard_temp = 250;

	if (battery->pdata->sub_bat_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE)
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

void sec_bat_check_mix_temp(struct sec_battery_info *battery, int ct, int siop_level, bool is_apdo)
{
	int temperature = battery->temperature;
	int chg_temp;
	int input_current = 0;

	if (battery->pdata->bat_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE ||
			battery->pdata->chg_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return;

	if (battery->pdata->blk_thm_info.check_type)
		temperature = battery->blkt_temp;

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	if (is_apdo)
		chg_temp = battery->dchg_temp;
	else
		chg_temp = battery->chg_temp;
#else
	chg_temp = battery->chg_temp;
#endif

	if (siop_level >= 100 && !battery->lcd_status && !is_wireless_fake_type(ct)) {
		if ((!battery->mix_limit && (temperature >= battery->pdata->mix_high_temp) &&
					(chg_temp >= battery->pdata->mix_high_chg_temp)) ||
			(battery->mix_limit && (temperature > battery->pdata->mix_high_temp_recovery))) {
			int max_input_current = battery->pdata->full_check_current_1st + 50;
			/* for checking charging source (DC -> SC) */
			if (battery->pdata->blk_thm_info.check_type && is_apdo && !battery->mix_limit) {
				battery->mix_limit = true;
				sec_vote_refresh(battery->fcc_vote);
				return;
			}
			/* input current = float voltage * (topoff_current_1st + 50mA(margin)) / (vbus_level * 0.9) */
			input_current = ((battery->pdata->chg_float_voltage / battery->pdata->chg_float_voltage_conv) *
					max_input_current) / ((battery->input_voltage * 9) / 10);

			if (input_current > max_input_current)
				input_current = max_input_current;

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
			if (!battery->mix_limit)
				store_battery_log(
					"Mix:%d%%,%dmV,mix_lim(%d),tbat(%d),tchg(%d),icurr(%d),ct(%s)",
					battery->capacity, battery->voltage_now, true,
					temperature, chg_temp, input_current, sb_get_ct_str(battery->cable_type));

			battery->mix_limit = true;
		} else if (battery->mix_limit) {
			battery->mix_limit = false;
			/* for checking charging source (SC -> DC) */
			if (battery->pdata->blk_thm_info.check_type)
				sec_vote_refresh(battery->fcc_vote);
			sec_vote(battery->input_vote, VOTER_MIX_LIMIT, false, 0);

			if (battery->tx_retry_case & SEC_BAT_TX_RETRY_MIX_TEMP) {
				pr_info("%s @Tx_Mode recovery mix_temp_limit, TX mode should be retried\n", __func__);
				if ((battery->tx_retry_case & ~SEC_BAT_TX_RETRY_MIX_TEMP) == 0)
					sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_RETRY,
							BATT_TX_EVENT_WIRELESS_TX_RETRY);
				battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_MIX_TEMP;
			}
			store_battery_log(
				"Mix:%d%%,%dmV,mix_lim(%d),tbat(%d),tchg(%d),icurr(%d),ct(%s)",
				battery->capacity, battery->voltage_now, battery->mix_limit,
				temperature, chg_temp, get_sec_vote_result(battery->input_vote),
				sb_get_ct_str(battery->cable_type));
		}

		pr_info("%s: mix_limit(%d), temp(%d), chg_temp(%d), input_current(%d)\n",
			__func__, battery->mix_limit, temperature, chg_temp, get_sec_vote_result(battery->input_vote));
	} else {
		if (battery->mix_limit) {
			battery->mix_limit = false;
			/* for checking charging source (SC -> DC) */
			if (battery->pdata->blk_thm_info.check_type)
				sec_vote_refresh(battery->fcc_vote);
			sec_vote(battery->input_vote, VOTER_MIX_LIMIT, false, 0);
		}
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_check_mix_temp);

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
EXPORT_SYMBOL_KUNIT(sec_bat_get_temp_by_temp_control_source);

#if IS_ENABLED(CONFIG_WIRELESS_CHARGING)
__visible_for_testing int sec_bat_check_wpc_vout(struct sec_battery_info *battery, int ct, unsigned int chg_limit,
		int pre_vout, unsigned int evt)
{
	union power_supply_propval value = {0, };
	int vout = 0;
	bool check_flicker_wa = false;

	if (!is_hv_wireless_type(ct))
		return 0;

	if (ct == SEC_BATTERY_CABLE_HV_WIRELESS_20)
		vout = battery->wpc_max_vout_level;
	else
		vout = WIRELESS_VOUT_10V;

	mutex_lock(&battery->voutlock);
	if (battery->pdata->wpc_vout_ctrl_lcd_on) {
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ID, value);
		if ((value.intval != WC_PAD_ID_UNKNOWN) &&
			(value.intval != WC_PAD_ID_SNGL_DREAM) &&
			(value.intval != WC_PAD_ID_STAND_DREAM)) {
			if (battery->wpc_vout_ctrl_mode && battery->lcd_status) {
				pr_info("%s: trigger flicker wa\n", __func__);
				check_flicker_wa = true;
			} else {
				value.intval = 0;
				psy_do_property(battery->pdata->wireless_charger_name, get,
					POWER_SUPPLY_EXT_PROP_PAD_VOLT_CTRL, value);
				if (!value.intval) {
					pr_info("%s: recover flicker wa\n", __func__);
					value.intval = battery->lcd_status;
					psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_EXT_PROP_PAD_VOLT_CTRL, value);
				}
			}
		}
	}

	/* get vout level */
	psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_RX_VOUT, value);

	if (value.intval == WIRELESS_VOUT_5_5V_STEP)
		pre_vout = WIRELESS_VOUT_5_5V_STEP;

	if ((evt & (SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING | SEC_BAT_CURRENT_EVENT_ISDB)) ||
			battery->sleep_mode || chg_limit || check_flicker_wa)
		vout = WIRELESS_VOUT_5_5V_STEP;

	if (vout != pre_vout) {
		if (evt & SEC_BAT_CURRENT_EVENT_WPC_VOUT_LOCK) {
			vout = pre_vout;
			pr_info("%s: block to set wpc vout level(%d) because otg on\n",
					__func__, vout);
		} else {
			value.intval = vout;
			psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION, value);
			pr_info("%s: change vout level(%d)", __func__, vout);
			sec_vote(battery->input_vote, VOTER_AICL, false, 0);
		}
	} else if ((vout == WIRELESS_VOUT_10V ||
				vout == battery->wpc_max_vout_level)) {
		/* reset aicl current to recover current for unexpected aicl during */
		/* before vout boosting completion */
		sec_vote(battery->input_vote, VOTER_AICL, false, 0);
	}
	mutex_unlock(&battery->voutlock);
	return vout;
}
EXPORT_SYMBOL_KUNIT(sec_bat_check_wpc_vout);

__visible_for_testing int sec_bat_check_wpc_step_limit(struct sec_battery_info *battery, unsigned int step_sz,
		unsigned int *step_limit_temp, unsigned int rx_power, int temp)
{
	int i;
	int fcc = 0;

	for (i = 0; i < step_sz; ++i) {
		if (temp > step_limit_temp[i]) {
			if (rx_power == SEC_WIRELESS_RX_POWER_12W)
				fcc = battery->pdata->wpc_step_limit_fcc_12w[i];
			else if (rx_power >= SEC_WIRELESS_RX_POWER_15W)
				fcc = battery->pdata->wpc_step_limit_fcc_15w[i];
			else
				fcc = battery->pdata->wpc_step_limit_fcc[i];
			pr_info("%s: wc20_rx_power(%d), wpc_step_limit[%d] temp:%d, fcc:%d\n",
					__func__, rx_power, i, temp, fcc);
			break;
		}
	}
	return fcc;
}
EXPORT_SYMBOL_KUNIT(sec_bat_check_wpc_step_limit);

__visible_for_testing void sec_bat_check_wpc_condition(struct sec_battery_info *battery, bool lcd_off, int ct,
		unsigned int rx_power, int *wpc_high_temp, int *wpc_high_temp_recovery)
{
	if (lcd_off) {
		if (ct == SEC_BATTERY_CABLE_HV_WIRELESS_20) {
			if (rx_power == SEC_WIRELESS_RX_POWER_12W) {
				*wpc_high_temp = battery->pdata->wpc_high_temp_12w;
				*wpc_high_temp_recovery = battery->pdata->wpc_high_temp_recovery_12w;
			} else {
				*wpc_high_temp = battery->pdata->wpc_high_temp_15w;
				*wpc_high_temp_recovery = battery->pdata->wpc_high_temp_recovery_15w;
			}
		} else {
			*wpc_high_temp = battery->pdata->wpc_high_temp;
			*wpc_high_temp_recovery = battery->pdata->wpc_high_temp_recovery;
		}
	} else {
		if (ct == SEC_BATTERY_CABLE_HV_WIRELESS_20) {
			if (rx_power == SEC_WIRELESS_RX_POWER_12W) {
				*wpc_high_temp = battery->pdata->wpc_lcd_on_high_temp_12w;
				*wpc_high_temp_recovery = battery->pdata->wpc_lcd_on_high_temp_rec_12w;
			} else {
				*wpc_high_temp = battery->pdata->wpc_lcd_on_high_temp_15w;
				*wpc_high_temp_recovery = battery->pdata->wpc_lcd_on_high_temp_rec_15w;
			}
		} else {
			*wpc_high_temp = battery->pdata->wpc_lcd_on_high_temp;
			*wpc_high_temp_recovery = battery->pdata->wpc_lcd_on_high_temp_rec;
		}
	}
}

__visible_for_testing int sec_bat_check_wpc_chg_limit(struct sec_battery_info *battery, bool lcd_off, int ct,
		int chg_limit, int *step_limit_fcc)
{
	int wpc_high_temp = battery->pdata->wpc_high_temp;
	int wpc_high_temp_recovery = battery->pdata->wpc_high_temp_recovery;
	int thermal_source = battery->pdata->wpc_temp_control_source;
	int temp;

	if (!lcd_off)
		thermal_source = battery->pdata->wpc_temp_lcd_on_control_source;

	temp = sec_bat_get_temp_by_temp_control_source(battery, thermal_source);
	sec_bat_check_wpc_condition(battery,
		lcd_off, ct, battery->wc20_rx_power, &wpc_high_temp, &wpc_high_temp_recovery);

	if (temp >= wpc_high_temp)
		chg_limit = true;
	else if (temp <= wpc_high_temp_recovery)
		chg_limit = false;

	if (!chg_limit && (temp >= wpc_high_temp_recovery)) {
		*step_limit_fcc = sec_bat_check_wpc_step_limit(battery, battery->pdata->wpc_step_limit_size,
				battery->pdata->wpc_step_limit_temp, battery->wc20_rx_power, temp);
	}
	return chg_limit;
}

void sec_bat_check_wpc_temp(struct sec_battery_info *battery, int ct, int siop_level)
{
	int step_limit_fcc = 0;
	int chg_limit = battery->chg_limit;
	bool lcd_off = !battery->lcd_status;
	int icl = battery->pdata->wpc_input_limit_current;
	int fcc = battery->pdata->wpc_charging_limit_current;

	/* nv wc temp control is not necessary when nv wc icl has same value with hv limited icl.
	   That is why nv_wc_temp_ctrl_skip has true when nv wc icl has same value with hv limited icl.
	   Otherwise wc temp control is necessary with all kinds of wireless charger types when when nv wc icl is bigger than hv limited icl.
	   For example, 5.5V/800mA(NV) and 5.5V/800mA(HV) same power so that nv wc type can skip nv wc temp control,
	   but in case of 5.5V/800mA(NV) and 5.5V/700mA(HV) need wc temp control with nv wc type. */
	if (battery->pdata->wpc_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE ||
		(battery->nv_wc_temp_ctrl_skip && !is_hv_wireless_type(ct) && (ct != SEC_BATTERY_CABLE_WIRELESS_TX)) ||
		(!battery->nv_wc_temp_ctrl_skip && !is_wireless_type(ct)))
		return;

	chg_limit = sec_bat_check_wpc_chg_limit(battery, lcd_off, ct, chg_limit, &step_limit_fcc);
	battery->wpc_vout_level = sec_bat_check_wpc_vout(battery, ct, chg_limit,
			battery->wpc_vout_level, battery->current_event);

	pr_info("%s: vout_level: %d, chg_limit: %d, step_limit: %d\n",
			__func__, battery->wpc_vout_level, chg_limit, step_limit_fcc);
	battery->chg_limit = chg_limit;
	if (chg_limit) {
		if (!lcd_off)
			icl = battery->pdata->wpc_lcd_on_input_limit_current;
		if (ct == SEC_BATTERY_CABLE_WIRELESS_TX &&
				battery->pdata->wpc_input_limit_by_tx_check)
			icl = battery->pdata->wpc_input_limit_current_by_tx;
		sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, icl);
		sec_vote(battery->input_vote, VOTER_CABLE, false, 0); /* 10(V)/ICL(mA) -> 5.5(V)/wpc_input_limit_current(mA) */
		sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, fcc);
	} else {
		if (step_limit_fcc)
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, step_limit_fcc);
		else
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, fcc);
		sec_vote(battery->input_vote, VOTER_CABLE, true,
			battery->pdata->charging_current[ct].input_current_limit); /* 5.5(V)/wpc_input_limit_current(mA) -> 10(V)/ICL(mA) */
		sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, icl);
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_check_wpc_temp);

void sec_bat_thermal_warm_wc_fod(struct sec_battery_info *battery, bool is_charging)
{
	union power_supply_propval value = {0, };

	if (!battery->pdata->wpc_warm_fod)
		return;

	if (!is_wireless_fake_type(battery->cable_type))
		return;

	value.intval = is_charging;
	if (!is_charging)
		sec_vote(battery->input_vote, VOTER_SWELLING, true, battery->pdata->wpc_warm_fod_icc);

	psy_do_property(battery->pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_WARM_FOD, value);
}
#else
void sec_bat_check_wpc_temp(struct sec_battery_info *battery, int ct, int siop_level) {}
void sec_bat_thermal_warm_wc_fod(struct sec_battery_info *battery, bool is_charging) {}
#endif

#if defined(CONFIG_WIRELESS_TX_MODE)
void sec_bat_check_tx_temperature(struct sec_battery_info *battery)
{
	int bat_temp = battery->temperature;

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	bat_temp = sec_bat_get_high_priority_temp(battery);
#endif
	if (battery->wc_tx_enable) {
		if (bat_temp >= battery->pdata->tx_high_threshold) {
			pr_info("@Tx_Mode : %s: Battery temperature is too high. Tx mode should turn off\n", __func__);
			/* set tx event */
			sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP, BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP);
			battery->tx_retry_case |= SEC_BAT_TX_RETRY_HIGH_TEMP;
			sec_wireless_set_tx_enable(battery, false);
		} else if (bat_temp <= battery->pdata->tx_low_threshold) {
			pr_info("@Tx_Mode : %s: Battery temperature is too low. Tx mode should turn off\n", __func__);
			/* set tx event */
			sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_LOW_TEMP, BATT_TX_EVENT_WIRELESS_TX_LOW_TEMP);
			battery->tx_retry_case |= SEC_BAT_TX_RETRY_LOW_TEMP;
			sec_wireless_set_tx_enable(battery, false);
		}
	} else if (battery->tx_retry_case & SEC_BAT_TX_RETRY_HIGH_TEMP) {
		if (bat_temp <= battery->pdata->tx_high_recovery) {
			pr_info("@Tx_Mode : %s: Battery temperature goes to normal(High). Retry TX mode\n", __func__);
			battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_HIGH_TEMP;
			if (!battery->tx_retry_case)
				sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_RETRY, BATT_TX_EVENT_WIRELESS_TX_RETRY);
		}
	} else if (battery->tx_retry_case & SEC_BAT_TX_RETRY_LOW_TEMP) {
		if (bat_temp >= battery->pdata->tx_low_recovery) {
			pr_info("@Tx_Mode : %s: Battery temperature goes to normal(Low). Retry TX mode\n", __func__);
			battery->tx_retry_case &= ~SEC_BAT_TX_RETRY_LOW_TEMP;
			if (!battery->tx_retry_case)
				sec_bat_set_tx_event(battery, BATT_TX_EVENT_WIRELESS_TX_RETRY, BATT_TX_EVENT_WIRELESS_TX_RETRY);
		}
	}
}
#endif

int sec_bat_check_power_type(
	int max_chg_pwr, int pd_max_chg_pwr, int ct, int ws, int is_apdo)
{
	if (is_pd_wire_type(ct) && is_apdo) {
		if (get_chg_power_type(ct, ws, pd_max_chg_pwr, max_chg_pwr) == SFC_45W)
			return SFC_45W;
		else
			return SFC_25W;
	} else {
		return NORMAL_TA;
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_check_power_type);

int sec_bat_check_lrp_temp_cond(int prev_step,
	int temp, int trig, int recov)
{
	if (trig <= temp)
		prev_step++;
	else if (recov >= temp)
		prev_step--;

	if (prev_step < LRP_NONE)
		prev_step = LRP_NONE;
	else if (prev_step > LRP_STEP2)
		prev_step = LRP_STEP2;

	return prev_step;
}

int sec_bat_check_lrp_step(
	struct sec_battery_info *battery, int temp, int pt, bool lcd_sts)
{
	int step = LRP_NONE;
	int lcd_st = LCD_OFF;
	int lrp_pt = LRP_NORMAL;
	int lrp_high_temp_st1 = battery->pdata->lrp_temp[LRP_NORMAL].trig[ST1][LCD_OFF];
	int lrp_high_temp_st2 = battery->pdata->lrp_temp[LRP_NORMAL].trig[ST2][LCD_OFF];
	int lrp_high_temp_recov_st1 = battery->pdata->lrp_temp[LRP_NORMAL].recov[ST1][LCD_OFF];
	int lrp_high_temp_recov_st2 = battery->pdata->lrp_temp[LRP_NORMAL].recov[ST2][LCD_OFF];

	if (lcd_sts)
		lcd_st = LCD_ON;

	if (pt == SFC_45W)
		lrp_pt = LRP_45W;
	else if (pt == SFC_25W)
		lrp_pt = LRP_25W;

	lrp_high_temp_st1 = battery->pdata->lrp_temp[lrp_pt].trig[ST1][lcd_st];
	lrp_high_temp_st2 = battery->pdata->lrp_temp[lrp_pt].trig[ST2][lcd_st];
	lrp_high_temp_recov_st1 = battery->pdata->lrp_temp[lrp_pt].recov[ST1][lcd_st];
	lrp_high_temp_recov_st2 = battery->pdata->lrp_temp[lrp_pt].recov[ST2][lcd_st];

	pr_info("%s: st1(%d), st2(%d), recv_st1(%d), recv_st2(%d), lrp(%d)\n", __func__,
		lrp_high_temp_st1, lrp_high_temp_st2,
		lrp_high_temp_recov_st1, lrp_high_temp_recov_st2, temp);

	switch (battery->lrp_step) {
	case LRP_STEP2:
		step = sec_bat_check_lrp_temp_cond(battery->lrp_step,
				temp, 900, lrp_high_temp_recov_st2);
		break;
	case LRP_STEP1:
		step = sec_bat_check_lrp_temp_cond(battery->lrp_step,
				temp, lrp_high_temp_st2, lrp_high_temp_recov_st1);
		break;
	case LRP_NONE:
		step = sec_bat_check_lrp_temp_cond(battery->lrp_step,
				temp, lrp_high_temp_st1, -200);
		break;
	default:
		break;
	}

	if ((battery->lrp_step != LRP_STEP1) && (step == LRP_STEP1))
		step = sec_bat_check_lrp_temp_cond(step,
				temp, lrp_high_temp_st2, lrp_high_temp_recov_st1);

	return step;
}

#if defined(CONFIG_SUPPORT_HV_CTRL) && !defined(CONFIG_SEC_FACTORY)
bool sec_bat_temp_vbus_condition(int ct, unsigned int evt)
{
	if (!(is_hv_wire_type(ct) || is_pd_wire_type(ct)) ||
			(ct == SEC_BATTERY_CABLE_QC30))
		return false;

	if ((evt & SEC_BAT_CURRENT_EVENT_AFC) ||
			(evt & SEC_BAT_CURRENT_EVENT_SELECT_PDO))
		return false;
	return true;
}

bool sec_bat_change_temp_vbus(struct sec_battery_info *battery,
		int ct, unsigned int evt, bool lcd_sts, int vote_evt)
{
	if (battery->pdata->chg_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE ||
			battery->store_mode)
		return false;
	if (!sec_bat_temp_vbus_condition(ct, evt))
		return false;

	if (!lcd_sts)
		sec_vote(battery->iv_vote, vote_evt, false, 0);
	else {
		sec_vote(battery->iv_vote, vote_evt, true, SEC_INPUT_VOLTAGE_5V);
		pr_info("%s: vbus set 5V by lrp, Cable(%d, %d, %d)\n",
					__func__, ct, battery->muic_cable_type, battery->pd_usb_attached);
		return true;
	}
	return false;
}
#endif

void sec_bat_check_lrp_temp(
	struct sec_battery_info *battery, int ct, int ws, int siop_level, bool lcd_sts)
{
	int input_current = 0, charging_current = 0;
	int lrp_step = LRP_NONE;
	bool is_apdo = false;
	int power_type = NORMAL_TA;
	bool force_check = false;
	int ret = 0;
	bool hv_ctrl = false;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	unsigned int cur_lrp_chg_src = battery->lrp_chg_src;
#endif
	if (battery->pdata->lrp_temp_check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return;

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	is_apdo = (is_pd_apdo_wire_type(ct) && battery->pd_list.now_isApdo) ? 1 : 0;
#endif
	power_type = sec_bat_check_power_type(battery->max_charge_power,
				battery->pd_max_charge_power, ct, ws, is_apdo);

	lrp_step = sec_bat_check_lrp_step(battery, battery->lrp, power_type, lcd_sts);

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
	/* trigger from DC to SC for lcd on at LRP_STEP2 */
	if (battery->pdata->sc_LRP_25W) {
		if (power_type == SFC_25W || battery->lrp_chg_src == SEC_CHARGING_SOURCE_SWITCHING) {
			if (lcd_sts && lrp_step >= LRP_STEP2)
				cur_lrp_chg_src = SEC_CHARGING_SOURCE_SWITCHING;
			else
				cur_lrp_chg_src = SEC_CHARGING_SOURCE_DIRECT;
		}
	}
#endif
	/* 15w afc or pd ta */
	if (power_type == NORMAL_TA) {
		ret = get_sec_vote_result(battery->iv_vote);
		if (((ret == SEC_INPUT_VOLTAGE_5V) && !lcd_sts) ||
			((ret != SEC_INPUT_VOLTAGE_5V) && lcd_sts))
			force_check = true;
		pr_info("%s: force_check(%d), ret(%d), lcd(%d)\n", __func__,
			(force_check ? 1 : 0), ret, (lcd_sts ? 1 : 0));
	}

	if ((lrp_step == LRP_STEP2) || (lrp_step == LRP_STEP1)) {
		if (is_pd_wire_type(ct)) {
			if (power_type == SFC_45W) {
				input_current = battery->pdata->lrp_curr[LRP_45W].st_icl[lrp_step - 1];
				charging_current = battery->pdata->lrp_curr[LRP_45W].st_fcc[lrp_step - 1];
			} else if (power_type == SFC_25W) {
				input_current = battery->pdata->lrp_curr[LRP_25W].st_icl[lrp_step - 1];
				charging_current = battery->pdata->lrp_curr[LRP_25W].st_fcc[lrp_step - 1];
			} else {
#if defined(CONFIG_SUPPORT_HV_CTRL) && !defined(CONFIG_SEC_FACTORY)
				hv_ctrl = sec_bat_change_temp_vbus(battery, ct,
					battery->current_event, lcd_sts, VOTER_LRP_TEMP);
#endif
				if (lcd_sts) {
					input_current = hv_ctrl ?
						battery->pdata->siop_icl :
						mA_by_mWmV(battery->pdata->power_value, battery->input_voltage);
					charging_current = hv_ctrl ?
						battery->pdata->siop_fcc : battery->pdata->siop_hv_fcc;
				} else {
					input_current = battery->pdata->chg_input_limit_current;
					charging_current = battery->pdata->chg_charging_limit_current;
				}
			}
		} else if (is_hv_wire_type(ct)) {
#if defined(CONFIG_SUPPORT_HV_CTRL) && !defined(CONFIG_SEC_FACTORY)
			hv_ctrl = sec_bat_change_temp_vbus(battery, ct,
				battery->current_event, lcd_sts, VOTER_LRP_TEMP);
#endif
			if (lcd_sts) {
				input_current = hv_ctrl ? battery->pdata->siop_icl : battery->pdata->siop_hv_icl;
				charging_current = hv_ctrl ? battery->pdata->siop_fcc : battery->pdata->siop_hv_fcc;
			} else {
				input_current = battery->pdata->chg_input_limit_current;
				charging_current = battery->pdata->chg_charging_limit_current;
			}
#if defined(CONFIG_USE_POGO)
		} else if (ct == SEC_BATTERY_CABLE_POGO_9V) {
			if (lcd_sts) {
				input_current = battery->pdata->siop_hv_icl;
				charging_current = battery->pdata->siop_hv_fcc;
			} else {
				input_current = battery->pdata->chg_input_limit_current;
				charging_current = battery->pdata->chg_charging_limit_current;
			}
#endif
		} else {
			if (lcd_sts) {
				input_current = battery->pdata->siop_icl;
				charging_current = battery->pdata->siop_fcc;
			} else {
				input_current = battery->pdata->default_input_current;
				charging_current = battery->pdata->default_charging_current;
			}
		}
		sec_vote(battery->fcc_vote, VOTER_LRP_TEMP, true, charging_current);
		sec_vote(battery->input_vote, VOTER_LRP_TEMP, true, input_current);
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)		
		if (battery->lrp_chg_src != cur_lrp_chg_src) {
			battery->lrp_chg_src = cur_lrp_chg_src;
			sec_vote_refresh(battery->fcc_vote);
		}
#endif		
		if ((battery->lrp_step != lrp_step) || force_check)
			store_battery_log(
				"LRP:SOC(%d),Vnow(%d),lrp_step(%d),lcd(%d),tlrp(%d),icl(%d),fcc(%d),ct(%d),is_apdo(%d),mcp(%d,%d)",
					battery->capacity, battery->voltage_now, lrp_step, lcd_sts,
					battery->lrp, input_current, charging_current, battery->cable_type,
					is_apdo, battery->pd_max_charge_power, battery->max_charge_power);
		battery->lrp_limit = true;
	} else if ((battery->lrp_limit == true) && (lrp_step == LRP_NONE)) {
		sec_vote(battery->iv_vote, VOTER_LRP_TEMP, false, 0);
		sec_vote(battery->fcc_vote, VOTER_LRP_TEMP, false, 0);
		sec_vote(battery->input_vote, VOTER_LRP_TEMP, false, 0);
		battery->lrp_limit = false;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
		battery->lrp_chg_src = SEC_CHARGING_SOURCE_DIRECT;
#endif
		store_battery_log(
			"LRP:%d%%,%dmV,lrp_lim(%d),tlrp(%d),icurr(%d),ocurr(%d),ct(%s)",
				battery->capacity, battery->voltage_now, battery->lrp_limit,
				battery->lrp, get_sec_vote_result(battery->input_vote),
				get_sec_vote_result(battery->fcc_vote), sb_get_ct_str(battery->cable_type));
	}
	battery->lrp_step = lrp_step;

	pr_info("%s: cable_type(%d), lrp_step(%d), lrp(%d)\n", __func__,
		ct, battery->lrp_step, battery->lrp);
}
EXPORT_SYMBOL_KUNIT(sec_bat_check_lrp_temp);

#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
static int sec_bat_check_lpm_power(int lpm, int pt)
{
	int ret = 0;

	if (pt == SFC_25W)
		ret |= 0x02;

	if (lpm)
		ret |= 0x01;

	return ret;
}

int sec_bat_set_dchg_current(struct sec_battery_info *battery, int power_type, int pt)
{
	int input_current = 0, charging_current = 0;

	if (power_type == SFC_45W) {
		if (pt & 0x01) {
			input_current = battery->pdata->lrp_curr[LRP_45W].st_icl[ST1];
			charging_current = battery->pdata->lrp_curr[LRP_45W].st_fcc[ST1];
		} else {
			input_current = battery->pdata->lrp_curr[LRP_45W].st_icl[ST2];
			charging_current = battery->pdata->lrp_curr[LRP_45W].st_fcc[ST2];
		}
	} else if (power_type == SFC_25W) {
		if (pt & 0x01) {
			input_current = battery->pdata->lrp_curr[LRP_25W].st_icl[ST1];
			charging_current = battery->pdata->lrp_curr[LRP_25W].st_fcc[ST1];
		} else {
			input_current = battery->pdata->lrp_curr[LRP_25W].st_icl[ST2];
			charging_current = battery->pdata->lrp_curr[LRP_25W].st_fcc[ST2];
		}
	} else {
		input_current = battery->pdata->dchg_input_limit_current;
		charging_current = battery->pdata->dchg_charging_limit_current;
	}

	sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
	sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);

	return charging_current;
}

void sec_bat_check_direct_chg_temp(struct sec_battery_info *battery, int siop_level)
{
	int input_current = 0, charging_current = 0, pt = 0;
	int ct = battery->cable_type, ws = battery->wire_status;
	bool is_apdo = false;
	int power_type = NORMAL_TA;

	if (battery->pdata->dchg_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE) {
		return;
	} else if (battery->pdata->dctp_bootmode_en) {
		battery->chg_limit = false;
		sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
		sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);

		return;
	}

	is_apdo = (is_pd_apdo_wire_type(ct) && battery->pd_list.now_isApdo) ? 1 : 0;
	power_type = sec_bat_check_power_type(battery->max_charge_power,
				battery->pd_max_charge_power, ct, ws, is_apdo);

	pt = sec_bat_check_lpm_power(sec_bat_get_lpmode(), power_type);

	if (siop_level >= 100) {
		if (!battery->chg_limit &&
			((battery->dchg_temp >= battery->pdata->dchg_high_temp[pt]) ||
			(battery->temperature >= battery->pdata->dchg_high_batt_temp[pt]))) {
			battery->chg_limit = true;
			charging_current = sec_bat_set_dchg_current(battery, power_type, pt);
			input_current = charging_current / 2;
			store_battery_log(
				"Dchg:%d%%,%dmV,chg_lim(%d),tbat(%d),tdchg(%d),icurr(%d),ocurr(%d),ct(%s)",
				battery->capacity, battery->voltage_now, battery->chg_limit,
				battery->temperature, battery->dchg_temp, input_current, charging_current,
				sb_get_ct_str(battery->cable_type));
		} else if (battery->chg_limit) {
			if ((battery->dchg_temp <= battery->pdata->dchg_high_temp_recovery[pt]) &&
				(battery->temperature <= battery->pdata->dchg_high_batt_temp_recovery[pt])) {
				battery->chg_limit = false;
				sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
				sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
				store_battery_log(
					"Dchg:%d%%,%dmV,chg_lim(%d),tbat(%d),tdchg(%d),icurr(%d),ocurr(%d),ct(%s)",
					battery->capacity, battery->voltage_now, battery->chg_limit,
					battery->temperature, battery->dchg_temp,
					get_sec_vote_result(battery->input_vote),
					get_sec_vote_result(battery->fcc_vote), sb_get_ct_str(battery->cable_type));
			} else {
				battery->chg_limit = true;
				sec_bat_set_dchg_current(battery, power_type, pt);
			}
		}
		pr_info("%s: chg_limit(%d)\n", __func__, battery->chg_limit);
	} else if (battery->chg_limit) {
		battery->chg_limit = false;
		sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
		sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_check_direct_chg_temp);
#else
void sec_bat_check_direct_chg_temp(struct sec_battery_info *battery, int siop_level) {}
#endif

void sec_bat_check_pdic_temp(struct sec_battery_info *battery, int siop_level)
{
	int input_current = 0, charging_current = 0;

	if (battery->pdata->chg_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return;

	if (battery->pdic_ps_rdy && siop_level >= 100) {
		if ((!battery->chg_limit && (battery->chg_temp >= battery->pdata->chg_high_temp)) ||
			(battery->chg_limit && (battery->chg_temp >= battery->pdata->chg_high_temp_recovery))) {
			input_current =
				(battery->pdata->chg_input_limit_current * SEC_INPUT_VOLTAGE_9V) / battery->input_voltage;
			charging_current = battery->pdata->chg_charging_limit_current;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
			if (!battery->chg_limit)
				store_battery_log(
					"Pdic:%d%%,%dmV,chg_lim(%d),tchg(%d),icurr(%d),ocurr(%d),ct(%s)",
					battery->capacity, battery->voltage_now, true,
					battery->chg_temp, input_current, charging_current,
					sb_get_ct_str(battery->cable_type));
			battery->chg_limit = true;
		} else if (battery->chg_limit && battery->chg_temp <= battery->pdata->chg_high_temp_recovery) {
			battery->chg_limit = false;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
			store_battery_log(
				"Pdic:%d%%,%dmV,chg_lim(%d),tchg(%d),icurr(%d),ocurr(%d),ct(%s)",
				battery->capacity, battery->voltage_now, battery->chg_limit,
				battery->chg_temp, get_sec_vote_result(battery->input_vote),
				get_sec_vote_result(battery->fcc_vote), sb_get_ct_str(battery->cable_type));
		}
		pr_info("%s: chg_limit(%d)\n", __func__, battery->chg_limit);
	} else if (battery->chg_limit) {
		battery->chg_limit = false;
		sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
		sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_check_pdic_temp);

void sec_bat_check_afc_temp(struct sec_battery_info *battery, int siop_level)
{
	int input_current = 0, charging_current = 0;
	int ct = battery->cable_type;

	if (battery->pdata->chg_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE)
		return;

	if (siop_level >= 100) {
#if defined(CONFIG_SUPPORT_HV_CTRL)
		if (!battery->chg_limit && is_hv_wire_type(ct) &&
				(battery->chg_temp >= battery->pdata->chg_high_temp)) {
			input_current = battery->pdata->chg_input_limit_current;
			charging_current = battery->pdata->chg_charging_limit_current;
			battery->chg_limit = true;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
			store_battery_log(
				"Afc:%d%%,%dmV,chg_lim(%d),tchg(%d),icurr(%d),ocurr(%d),ct(%s)",
				battery->capacity, battery->voltage_now, battery->chg_limit,
				battery->chg_temp, input_current, charging_current,
				sb_get_ct_str(ct));
		} else if (!battery->chg_limit && battery->max_charge_power >=
				(battery->pdata->pd_charging_charge_power - 500) &&
				(battery->chg_temp >= battery->pdata->chg_high_temp)) {
			input_current = battery->pdata->default_input_current;
			charging_current = battery->pdata->default_charging_current;
			battery->chg_limit = true;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
			store_battery_log(
				"Afc:%d%%,%dmV,chg_lim(%d),tchg(%d),icurr(%d),ocurr(%d),ct(%s)",
				battery->capacity, battery->voltage_now, battery->chg_limit,
				battery->chg_temp, input_current, charging_current,
				sb_get_ct_str(battery->cable_type));
		} else if (battery->chg_limit && is_hv_wire_type(ct)) {
			if (battery->chg_temp <= battery->pdata->chg_high_temp_recovery) {
				battery->chg_limit = false;
				sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
				sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
				store_battery_log(
					"Afc:%d%%,%dmV,chg_lim(%d),tchg(%d),icurr(%d),ocurr(%d),ct(%s)",
					battery->capacity, battery->voltage_now, battery->chg_limit,
					battery->chg_temp, get_sec_vote_result(battery->input_vote),
					get_sec_vote_result(battery->fcc_vote), sb_get_ct_str(ct));
			}
		} else if (battery->chg_limit && battery->max_charge_power >=
				(battery->pdata->pd_charging_charge_power - 500)) {
			if (battery->chg_temp <= battery->pdata->chg_high_temp_recovery) {
				battery->chg_limit = false;
				sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
				sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
				store_battery_log(
					"Afc:%d%%,%dmV,chg_lim(%d),tchg(%d),icurr(%d),ocurr(%d),ct(%s)",
					battery->capacity, battery->voltage_now, battery->chg_limit,
					battery->chg_temp, get_sec_vote_result(battery->input_vote),
					get_sec_vote_result(battery->fcc_vote), sb_get_ct_str(ct));
			}
		}
		pr_info("%s: cable_type(%d), chg_limit(%d)\n", __func__,
			ct, battery->chg_limit);
#else
		if ((!battery->chg_limit && is_hv_wire_type(ct) &&
					(battery->chg_temp >= battery->pdata->chg_high_temp)) ||
			(battery->chg_limit && is_hv_wire_type(ct) &&
			(battery->chg_temp > battery->pdata->chg_high_temp_recovery))) {
			if (!battery->chg_limit) {
				input_current = battery->pdata->chg_input_limit_current;
				charging_current = battery->pdata->chg_charging_limit_current;
				battery->chg_limit = true;
				sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, true, charging_current);
				sec_vote(battery->input_vote, VOTER_CHG_TEMP, true, input_current);
				store_battery_log(
					"Afc:%d%%,%dmV,chg_lim(%d),tchg(%d),icurr(%d),ocurr(%d),ct(%s)",
					battery->capacity, battery->voltage_now, battery->chg_limit,
					battery->chg_temp, input_current, charging_current,
					sb_get_ct_str(ct));
			}
		} else if (battery->chg_limit && is_hv_wire_type(ct) &&
				(battery->chg_temp <= battery->pdata->chg_high_temp_recovery)) {
			battery->chg_limit = false;
			sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
			sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
			store_battery_log(
				"Afc:%d%%,%dmV,chg_lim(%d),tchg(%d),icurr(%d),ocurr(%d),ct(%s)",
				battery->capacity, battery->voltage_now, battery->chg_limit,
				battery->chg_temp, get_sec_vote_result(battery->input_vote),
				get_sec_vote_result(battery->fcc_vote),	sb_get_ct_str(ct));
		}
#endif
	} else if (battery->chg_limit) {
		battery->chg_limit = false;
		sec_vote(battery->fcc_vote, VOTER_CHG_TEMP, false, 0);
		sec_vote(battery->input_vote, VOTER_CHG_TEMP, false, 0);
	}
}
EXPORT_SYMBOL_KUNIT(sec_bat_check_afc_temp);

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

int sec_usb_temp_gap_check(struct sec_battery_info *battery,
	bool tmp_chk, int usb_temp)
{
	int gap = 0;
	int bat_thm = battery->temperature;

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	/* select low temp thermistor */
	if (battery->temperature > battery->sub_bat_temp)
		bat_thm = battery->sub_bat_temp;
#endif
	if (usb_temp > bat_thm)
		gap = usb_temp - bat_thm;

	if (tmp_chk) { /* check usb temp gap */
		if ((usb_temp >= battery->usb_protection_temp) &&
				(gap >= battery->temp_gap_bat_usb)) {
			pr_info("%s: Temp gap between Usb temp and Bat temp : %d\n", __func__, gap);
#if defined(CONFIG_BATTERY_CISD)
			if (gap > battery->cisd.data[CISD_DATA_USB_OVERHEAT_ALONE_PER_DAY])
				battery->cisd.data[CISD_DATA_USB_OVERHEAT_ALONE_PER_DAY] = gap;
#endif
			return USB_THM_GAP_OVER1;
		} else if ((usb_temp >= battery->overheatlimit_threshold) &&
			(gap >= (battery->temp_gap_bat_usb - 100))) {
			pr_info("%s: Usb Temp (%d) over and gap between Usb temp and Bat temp : %d\n",
				__func__, usb_temp, gap);
			return USB_THM_GAP_OVER2;
		}
	} else { /* recover check */
		if ((battery->usb_thm_status == USB_THM_GAP_OVER1) &&
			(usb_temp >= battery->usb_protection_temp)) {
			return USB_THM_GAP_OVER1;

		} else if ((battery->usb_thm_status == USB_THM_GAP_OVER2) &&
			(usb_temp >= battery->overheatlimit_threshold)) {
			return USB_THM_GAP_OVER2;
		}
	}
	pr_info("%s: %s -> NORMAL\n", __func__, sec_usb_thm_str(battery->usb_thm_status));

	return USB_THM_NORMAL;
}

int sec_usb_thm_overheatlimit(struct sec_battery_info *battery)
{
	bool use_usb_temp = (battery->pdata->usb_thm_info.check_type != SEC_BATTERY_TEMP_CHECK_NONE);
	int usb_temp = battery->usb_temp;

	if (!use_usb_temp) {
		pr_err("%s: USB_THM, Invalid Temp Check Type, usb_thm <- bat_thm\n", __func__);
		battery->usb_temp = battery->temperature;
		usb_temp = battery->temperature;
	}
#if defined(CONFIG_SEC_FACTORY)
	use_usb_temp = false;
#endif

	switch (battery->usb_thm_status) {
	case USB_THM_NORMAL:
		if ((usb_temp >= battery->overheatlimit_threshold) &&
			!use_usb_temp) {
			pr_info("%s: Usb Temp over than %d (usb_thm : %d)\n", __func__,
				battery->overheatlimit_threshold, usb_temp);
			battery->usb_thm_status = USB_THM_OVERHEATLIMIT;
		} else {
			if (use_usb_temp)
				battery->usb_thm_status = sec_usb_temp_gap_check(battery, true, usb_temp);
		}
		break;
	case USB_THM_OVERHEATLIMIT:
		if (usb_temp <= battery->overheatlimit_recovery)
			battery->usb_thm_status = USB_THM_NORMAL;
		break;
	case USB_THM_GAP_OVER1:
	case USB_THM_GAP_OVER2:
		battery->usb_thm_status = sec_usb_temp_gap_check(battery, false, usb_temp);
		break;
	default:
		break;
	}

	return battery->usb_thm_status;
}

int sec_usb_protection_gap_check(struct sec_battery_info *battery, bool tmp_chk, int thm1_temp, int thm2_temp)
{
	int gap = 0;

	if (thm1_temp > thm2_temp)
		gap = thm1_temp - thm2_temp;

	if (tmp_chk) { /* check usb temp gap */
		if ((thm1_temp >= battery->usb_protection_temp) &&
				(gap >= battery->temp_gap_bat_usb)) {
			pr_info("%s: Temp gap between THM1 temp and THM2 temp : %d\n", __func__, gap);
#if defined(CONFIG_BATTERY_CISD)
			if (gap > battery->cisd.data[CISD_DATA_USB_OVERHEAT_ALONE_PER_DAY])
				battery->cisd.data[CISD_DATA_USB_OVERHEAT_ALONE_PER_DAY] = gap;
#endif
			return USB_THM_GAP_OVER1;
		} else if ((thm1_temp >= battery->overheatlimit_threshold) &&
			(gap >= (battery->temp_gap_bat_usb - 100))) {
			pr_info("%s: THM1 Temp (%d) over and gap between THM1 temp and THM2 temp : %d\n",
				__func__, thm1_temp, gap);
			return USB_THM_GAP_OVER2;
		}
	} else { /* recover check */
		if ((battery->usb_thm_status == USB_THM_GAP_OVER1) &&
			(thm1_temp >= battery->usb_protection_temp)) {
			return USB_THM_GAP_OVER1;

		} else if ((battery->usb_thm_status == USB_THM_GAP_OVER2) &&
			(thm1_temp >= battery->overheatlimit_threshold)) {
			return USB_THM_GAP_OVER2;
		}
	}
	pr_info("%s: %s -> NORMAL\n", __func__, sec_usb_thm_str(battery->usb_thm_status));

	return USB_THM_NORMAL;
}

int sec_usb_protection(struct sec_battery_info *battery)
{
	// gap = THM1_temperature - THM2_temperature
	int thm1_temp = battery->usb_temp;	// default for flagship models, THM1 = USB_THM
	int thm2_temp = battery->temperature;	// default for flagship models, THM2 = BAT_THM

	if (battery->pdata->usb_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE) {
		// there is no USB_THM, mass models, THM1 = BAT_THM, THM2 = CHG_THM
		battery->usb_temp = battery->temperature;
		thm1_temp = battery->temperature;
		thm2_temp = battery->chg_temp;
	} else if (battery->pdata->mass_with_usb_thm) {
		// there is USB_THM, mass models, THM1 = USB_THM, THM2 = CHG_THM
		thm1_temp = battery->usb_temp;
		thm2_temp = battery->chg_temp;
	}
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	/* select low temp thermistor */
	if (thm2_temp > battery->sub_bat_temp)
		thm2_temp = battery->sub_bat_temp;
#endif

	switch (battery->usb_thm_status) {
	case USB_THM_NORMAL:
		battery->usb_thm_status = sec_usb_protection_gap_check(battery, true, thm1_temp, thm2_temp);
		break;
	case USB_THM_GAP_OVER1:
	case USB_THM_GAP_OVER2:
		battery->usb_thm_status = sec_usb_protection_gap_check(battery, false, thm1_temp, thm2_temp);
		break;
	default:
		pr_info("%s: Unknown usb_thm_status(%d), forced set to NORMAL\n", __func__, battery->usb_thm_status);
		battery->usb_thm_status = USB_THM_NORMAL;
		break;
	}

	return battery->usb_thm_status;
}

void sec_bat_thermal_check(struct sec_battery_info *battery)
{
	int bat_thm = battery->temperature;
	int pre_thermal_zone = battery->thermal_zone;
	int voter_status = SEC_BAT_CHG_MODE_CHARGING;
	int usb_thm_status = battery->usb_thm_status;

#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	union power_supply_propval val = {0, };

	bat_thm = sec_bat_get_high_priority_temp(battery);
#endif

	pr_err("%s: co_c3: %d, c3_c2: %d, c2_c1: %d, c1_no: %d, no_wa: %d, wa_ov: %d, tz(%s)\n", __func__,
			battery->cold_cool3_thresh, battery->cool3_cool2_thresh, battery->cool2_cool1_thresh,
			battery->cool1_normal_thresh, battery->normal_warm_thresh, battery->warm_overheat_thresh,
			sec_bat_thermal_zone[battery->thermal_zone]);

	if (battery->status == POWER_SUPPLY_STATUS_DISCHARGING ||
#if defined(CONFIG_BC12_DEVICE) && defined(CONFIG_SEC_FACTORY)
		battery->vbat_adc_open ||
#endif
		battery->skip_swelling) {
		battery->health_change = false;
		pr_debug("%s: DISCHARGING or 15 test mode. stop thermal check\n", __func__);
		battery->thermal_zone = BAT_THERMAL_NORMAL;
		battery->usb_thm_status = USB_THM_NORMAL;
		sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->fcc_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->fv_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->input_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->chgen_vote, VOTER_SWELLING, false, 0);
		sec_vote(battery->chgen_vote, VOTER_CHANGE_CHGMODE, false, 0);
		sec_vote(battery->iv_vote, VOTER_CHANGE_CHGMODE, false, 0);
		sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
		sec_bat_set_threshold(battery, battery->cable_type);
		return;
	}

	if (battery->pdata->bat_thm_info.check_type == SEC_BATTERY_TEMP_CHECK_NONE) {
		pr_err("%s: BAT_THM, Invalid Temp Check Type\n", __func__);
		return;
	} else {
		/* COLD - COOL3 - COOL2 - COOL1 - NORMAL - WARM - OVERHEAT - OVERHEATLIMIT*/
		if (battery->pdata->usb_protection) /* 2022.03.03 new concept */
			usb_thm_status = sec_usb_protection(battery);
		else	/* original usb protection concept */
			usb_thm_status = sec_usb_thm_overheatlimit(battery);

		if (usb_thm_status != USB_THM_NORMAL) {
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
			} else if ((battery->usb_thm_status == USB_THM_GAP_OVER1) ||
				(battery->usb_thm_status == USB_THM_GAP_OVER2)) {
				pr_info("%s: USB_THM_GAP_OVER : %d\n", __func__,
					(battery->usb_temp > bat_thm) ? (battery->usb_temp - bat_thm) : 0);
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_USB_OVERHEAT_RAPID_CHANGE]++;
				battery->cisd.data[CISD_DATA_USB_OVERHEAT_RAPID_CHANGE_PER_DAY]++;
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
#if !defined(CONFIG_SEC_FACTORY)
			if (!sec_bat_get_lpmode()) {
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
				muic_set_hiccup_mode(1);
#endif
				if (is_pd_wire_type(battery->cable_type) || battery->pdata->mass_with_usb_thm)
					sec_pd_manual_ccopen_req(1);
			}
#endif
			store_battery_log(
				"OHL:%d%%,%dmV,usb_thm_st(%d),tbat(%d),tusb(%d),ct(%s)",
				battery->capacity, battery->voltage_now,
				battery->usb_thm_status, bat_thm,
				battery->usb_temp, sb_get_ct_str(battery->cable_type));
			break;
		case BAT_THERMAL_OVERHEAT:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			if (battery->voltage_now > battery->pdata->high_temp_float) {
#if defined(CONFIG_WIRELESS_TX_MODE)
				if (get_sec_vote_result(battery->iv_vote) > SEC_INPUT_VOLTAGE_5V) {
					sec_vote(battery->iv_vote, VOTER_CHANGE_CHGMODE, true, SEC_INPUT_VOLTAGE_5V);
					sec_vote(battery->chgen_vote, VOTER_CHANGE_CHGMODE, true, SEC_BAT_CHG_MODE_NOT_SET);
				}
#endif
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			} else {
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
				sec_vote(battery->iv_vote, VOTER_CHANGE_CHGMODE, false, 0);
			}
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_OVERHEAT);
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_UNSAFETY_TEMPERATURE]++;
			battery->cisd.data[CISD_DATA_UNSAFE_TEMPERATURE_PER_DAY]++;
#endif
			store_battery_log(
				"OH:%d%%,%dmV,tbat(%d),ct(%s)",
				battery->capacity, battery->voltage_now,
				bat_thm, sb_get_ct_str(battery->cable_type));
			break;
		case BAT_THERMAL_WARM:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			if (battery->voltage_now > battery->pdata->high_temp_float) {
				if ((battery->wc_tx_enable || battery->uno_en) &&
					(is_hv_wire_type(battery->cable_type) ||
					is_pd_wire_type(battery->cable_type))) {
					sec_vote(battery->chgen_vote, VOTER_SWELLING, true,
							SEC_BAT_CHG_MODE_CHARGING_OFF);
				} else {
					sec_vote(battery->chgen_vote, VOTER_SWELLING,
						true, SEC_BAT_CHG_MODE_BUCK_OFF);
				}
				sec_bat_thermal_warm_wc_fod(battery, false);
			} else if (battery->voltage_now > battery->pdata->swelling_high_rechg_voltage) {
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
				sec_bat_thermal_warm_wc_fod(battery, false);
			} else {
				sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
				sec_vote(battery->input_vote, VOTER_SWELLING, false, 0);
				sec_bat_thermal_warm_wc_fod(battery, true);
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
			sec_vote(battery->iv_vote, VOTER_CHANGE_CHGMODE, false, 0);

			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			store_battery_log(
				"THM_W:%d%%,%dmV,tbat(%d),ct(%s)",
				battery->capacity, battery->voltage_now,
				bat_thm, sb_get_ct_str(battery->cable_type));
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
			sec_vote(battery->input_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			sec_vote(battery->iv_vote, VOTER_CHANGE_CHGMODE, false, 0);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			store_battery_log(
				"THM_C1:%d%%,%dmV,tbat(%d),ct(%s)",
				battery->capacity, battery->voltage_now,
				bat_thm, sb_get_ct_str(battery->cable_type));
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
			sec_vote(battery->input_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			sec_vote(battery->iv_vote, VOTER_CHANGE_CHGMODE, false, 0);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			store_battery_log(
				"THM_C2:%d%%,%dmV,tbat(%d),ct(%s)",
				battery->capacity, battery->voltage_now,
				bat_thm, sb_get_ct_str(battery->cable_type));
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
			sec_vote(battery->fv_vote, VOTER_SWELLING, true, battery->pdata->low_temp_cool3_float);
			sec_vote(battery->input_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, true, battery->pdata->full_check_current_2nd);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING);
			sec_vote(battery->iv_vote, VOTER_CHANGE_CHGMODE, false, 0);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			store_battery_log(
				"THM_C3:%d%%,%dmV,tbat(%d),ct(%s)",
				battery->capacity, battery->voltage_now,
				bat_thm, sb_get_ct_str(battery->cable_type));
			break;
		case BAT_THERMAL_COLD:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_COOL3,
				SEC_BAT_CURRENT_EVENT_SWELLING_MODE);
			sec_vote(battery->input_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			sec_vote(battery->iv_vote, VOTER_CHANGE_CHGMODE, false, 0);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_COLD);
			sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
#if defined(CONFIG_BATTERY_CISD)
			battery->cisd.data[CISD_DATA_UNSAFETY_TEMPERATURE]++;
			battery->cisd.data[CISD_DATA_UNSAFE_TEMPERATURE_PER_DAY]++;
#endif
			store_battery_log(
				"THM_C:%d%%,%dmV,tbat(%d),ct(%s)",
				battery->capacity, battery->voltage_now,
				bat_thm, sb_get_ct_str(battery->cable_type));
			break;
		case BAT_THERMAL_NORMAL:
		default:
			battery->usb_thm_status = USB_THM_NORMAL;
			sec_vote(battery->fcc_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->fv_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->topoff_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->chgen_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->input_vote, VOTER_SWELLING, false, 0);
			sec_vote(battery->iv_vote, VOTER_CHANGE_CHGMODE, false, 0);
			sec_bat_set_health(battery, POWER_SUPPLY_HEALTH_GOOD);
			store_battery_log(
				"THM_N:%d%%,%dmV,tbat(%d),ct(%s)",
				battery->capacity, battery->voltage_now,
				bat_thm, sb_get_ct_str(battery->cable_type));
			break;
		}
		if ((battery->thermal_zone >= BAT_THERMAL_COOL3) && (battery->thermal_zone <= BAT_THERMAL_WARM)) {
			if (!is_full_capacity(battery->fs) ||
				!(battery->misc_event & BATT_MISC_EVENT_FULL_CAPACITY)) {
#if defined(CONFIG_ENABLE_FULL_BY_SOC)
				if ((battery->capacity >= 100) || (battery->status == POWER_SUPPLY_STATUS_FULL))
					sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_FULL);
				else
					sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
#else
				if (battery->status != POWER_SUPPLY_STATUS_FULL)
					sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_CHARGING);
#endif
			} else {
				pr_info("%s: prevent the status during is_full_cap\n", __func__);
			}
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
				sec_bat_thermal_warm_wc_fod(battery, false);
			}
			break;
		case BAT_THERMAL_WARM:
			if (battery->health == POWER_SUPPLY_HEALTH_GOOD) {
				int v_ref = battery->pdata->high_temp_float - battery->pdata->buck_recovery_margin;
				int voltage = battery->voltage_now; 
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
				voltage = max(battery->voltage_pack_main, battery->voltage_pack_sub);
#endif

				if (get_sec_voter_status(battery->chgen_vote, VOTER_SWELLING, &voter_status) < 0)
					pr_err("%s: INVALID VOTER ID\n", __func__);
				pr_info("%s: voter_status: %d\n", __func__, voter_status);
				if (voter_status == SEC_BAT_CHG_MODE_CHARGING) {
					if (sec_bat_check_fullcharged(battery)) {
						pr_info("%s: battery thermal zone WARM. Full charged.\n", __func__);
						sec_bat_thermal_warm_wc_fod(battery, false);
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
					(voltage <= battery->pdata->swelling_high_rechg_voltage)) {
					pr_info("%s: thermal zone WARM. charging recovery. Voltage: %d\n",
						__func__, voltage);
					battery->expired_time = battery->pdata->expired_time;
					battery->prev_safety_time = 0;
					sec_vote(battery->input_vote, VOTER_SWELLING, false, 0);
					sec_bat_thermal_warm_wc_fod(battery, true);
					sec_vote(battery->fv_vote, VOTER_SWELLING, true,
						battery->pdata->high_temp_float);
					sec_vote(battery->chgen_vote, VOTER_FULL_CHARGE, false, 0);
					sec_vote(battery->chgen_vote, VOTER_SWELLING, true,
						SEC_BAT_CHG_MODE_CHARGING);
				} else if (voter_status == SEC_BAT_CHG_MODE_BUCK_OFF && voltage < v_ref) {
					pr_info("%s: Voltage(%dmV) < %dmV, buck on\n", __func__,
						voltage, v_ref);
					sec_bat_thermal_warm_wc_fod(battery, false);
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
