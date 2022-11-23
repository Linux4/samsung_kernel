/*
 * s2mu107_direct_charger.c - S2MU107 Charger Driver
 *
 * Copyright (C) 2019 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/mfd/samsung/s2mu107.h>
#include <linux/delay.h>
#include "s2mu107_direct_charger.h"
#include "s2mu107_pmeter.h"

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif

#define DEFAULT_ALARM_INTERVAL	10
#define SLEEP_ALARM_INTERVAL	30

#define ENABLE 1
#define DISABLE 0

static char *s2mu107_supplied_to[] = {
	"battery",
};

static enum power_supply_property s2mu107_dc_props[] = {
};
static int s2mu107_dc_is_enable(struct s2mu107_dc_data *charger);
static void s2mu107_dc_protection(struct s2mu107_dc_data *charger, int onoff);
static void s2mu107_dc_enable(struct s2mu107_dc_data *charger, int onoff);
static void s2mu107_dc_pm_enable(struct s2mu107_dc_data *charger);
static int s2mu107_dc_get_topoff_current(struct s2mu107_dc_data *charger);
static void s2mu107_dc_set_topoff_current(struct s2mu107_dc_data *charger, int topoff_current);
static int s2mu107_dc_get_float_voltage(struct s2mu107_dc_data *charger);
static void s2mu107_dc_set_float_voltage(struct s2mu107_dc_data *charger, int float_voltage);
static int s2mu107_dc_get_input_current(struct s2mu107_dc_data *charger);
static void s2mu107_dc_set_input_current(struct s2mu107_dc_data *charger, int input_current);
static int s2mu107_dc_get_charging_current(struct s2mu107_dc_data *charger);
static void s2mu107_dc_set_charging_current(struct s2mu107_dc_data *charger, int charging_current);
static void s2mu107_dc_state_manager(struct s2mu107_dc_data *charger, s2mu107_dc_trans_t trans);
static int s2mu107_dc_get_pmeter(struct s2mu107_dc_data *charger, s2mu107_pmeter_id_t id);
static int s2mu107_dc_pps_isr(struct s2mu107_dc_data *charger, s2mu107_dc_pps_signal_t sig);
static int s2mu107_dc_set_wcin_pwrtr(struct s2mu107_dc_data *charger);
static int s2mu107_dc_get_iavg(struct s2mu107_dc_data *charger);
static int s2mu107_dc_get_inow(struct s2mu107_dc_data *charger);
static int s2mu107_dc_get_target_chg_current(struct s2mu107_dc_data *charger, int step);
static void s2mu107_dc_forced_enable(struct s2mu107_dc_data *charger, int onoff);

static const char *pmeter_to_str(s2mu107_pmeter_id_t n)
{
	char *ret;
	switch (n) {
	CONV_STR(PMETER_ID_VCHGIN, ret);
	CONV_STR(PMETER_ID_ICHGIN, ret);
	CONV_STR(PMETER_ID_IWCIN, ret);
	CONV_STR(PMETER_ID_VBATT, ret);
	CONV_STR(PMETER_ID_VSYS, ret);
	CONV_STR(PMETER_ID_TDIE, ret);
	default:
		return "invalid";
	}
	return ret;
}

static const char *state_to_str(s2mu107_dc_state_t n)
{
	char *ret;
	switch (n) {
	CONV_STR(DC_STATE_OFF, ret);
	CONV_STR(DC_STATE_CHECK_VBAT, ret);
	CONV_STR(DC_STATE_PRESET, ret);
	CONV_STR(DC_STATE_START_CC, ret);
	CONV_STR(DC_STATE_CC, ret);
	CONV_STR(DC_STATE_SLEEP_CC, ret);
	CONV_STR(DC_STATE_WAKEUP_CC, ret);
	CONV_STR(DC_STATE_DONE, ret);
	default:
		return "invalid";
	}
	return ret;
}

static const char *trans_to_str(s2mu107_dc_trans_t n)
{
	char *ret;
	switch (n) {
	CONV_STR(DC_TRANS_INVALID, ret);
	CONV_STR(DC_TRANS_NO_COND, ret);
	CONV_STR(DC_TRANS_VBATT_RETRY, ret);
	CONV_STR(DC_TRANS_WAKEUP_DONE, ret);
	CONV_STR(DC_TRANS_RAMPUP, ret);
	CONV_STR(DC_TRANS_RAMPUP_FINISHED, ret);
	CONV_STR(DC_TRANS_CHG_ENABLED, ret);
	CONV_STR(DC_TRANS_BATTERY_OK, ret);
	CONV_STR(DC_TRANS_BATTERY_NG, ret);
	CONV_STR(DC_TRANS_CHGIN_OKB_INT, ret);
	CONV_STR(DC_TRANS_CC_WAKEUP, ret);
	CONV_STR(DC_TRANS_BAT_OKB_INT, ret);
	CONV_STR(DC_TRANS_NORMAL_CHG_INT, ret);
	CONV_STR(DC_TRANS_DC_DONE_INT, ret);
	CONV_STR(DC_TRANS_CC_STABLED, ret);
	CONV_STR(DC_TRANS_DETACH, ret);
	CONV_STR(DC_TRANS_FAIL_INT, ret);
	default:
		return "invalid";
	}
	return ret;
}

static void s2mu107_dc_test_read(struct i2c_client *i2c)
{
	static int reg_list[] = {
                0x0B, 0x0C, 0x0D, 0x0E, 0x17, 0x41, 0x42,
		0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
		0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62
	};

	u8 data = 0;
	char str[1016] = {0,};
	int i = 0, reg_list_size = 0;

	reg_list_size = ARRAY_SIZE(reg_list);
	for (i = 0; i < reg_list_size; i++) {
                s2mu107_read_reg(i2c, reg_list[i], &data);
                sprintf(str+strlen(str), "0x%02x:0x%02x, ", reg_list[i], data);
        }

        for (i = 0xd7; i < 0xf8; i++) {
		s2mu107_read_reg(i2c, i, &data);
                sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	/* print buffer */
	pr_info("[DC]%s: %s\n", __func__, str);

}

static int s2mu107_dc_is_enable(struct s2mu107_dc_data *charger)
{
	/* TODO : DC status? check */
	u8 data;
	int ret;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL0, &data);
	if (ret < 0)
		return ret;

	return (data & DC_EN_MASK) ? 1 : 0;
}

static void s2mu107_dc_enable(struct s2mu107_dc_data *charger, int onoff)
{
	if (onoff > 0) {
		pr_info("%s, direct charger enable\n", __func__);
		wake_lock(&charger->wake_lock);
		charger->is_charging = true;
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL0, ENABLE, DC_EN_MASK);
	} else {
		pr_info("%s, direct charger disable\n", __func__);
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL0, DISABLE, DC_EN_MASK);
	}
}

static void s2mu107_dc_forced_enable(struct s2mu107_dc_data *charger, int onoff)
{
	if (onoff) {
		pr_info("%s, forced en enable\n", __func__);
		wake_lock(&charger->wake_lock);
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL0, DC_FORCED_EN_MASK, DC_FORCED_EN_MASK);
	} else {
		pr_info("%s, forced en disable\n", __func__);
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL0, DISABLE, DC_FORCED_EN_MASK);
	}
}

static void s2mu107_dc_platform_state_update(struct s2mu107_dc_data *charger)
{
	union power_supply_propval value;
	struct power_supply *psy;
	int state, ret;

	psy = power_supply_get_by_name(charger->pdata->sec_dc_name);
	if (!psy) {
		pr_err("%s, can't access power_supply", __func__);
		return;
	}
	switch (charger->dc_state) {
	case DC_STATE_OFF:
		state = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
		break;
	case DC_STATE_CHECK_VBAT:
		state = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
		break;
	case DC_STATE_PRESET:
		state = SEC_DIRECT_CHG_MODE_DIRECT_PRESET;
		break;
	case DC_STATE_START_CC:
		state = SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST;
		break;
	case DC_STATE_CC:
		state = SEC_DIRECT_CHG_MODE_DIRECT_ON;
		break;
	case DC_STATE_DONE:
		state = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
		break;
	default:
		break;
	}

	ret = power_supply_get_property(psy, (enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	if (value.intval != state) {
		value.intval = state;
		ret = power_supply_set_property(psy,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, &value);

	}
}

static void s2mu107_dc_pm_enable(struct s2mu107_dc_data *charger)
{
	union power_supply_propval value;

        value.intval = PM_TYPE_VCHGIN | PM_TYPE_VBAT | PM_TYPE_ICHGIN | PM_TYPE_TDIE |
                PM_TYPE_VWCIN | PM_TYPE_IWCIN | PM_TYPE_VBYP | PM_TYPE_VSYS |
                PM_TYPE_VCC1 | PM_TYPE_VCC2 | PM_TYPE_ICHGIN | PM_TYPE_ITX;
	psy_do_property(charger->pdata->pm_name, set,
				POWER_SUPPLY_PROP_CO_ENABLE, value);
}

static int s2mu107_dc_set_comm_fail(struct s2mu107_dc_data *charger)
{
	u8 data;
	int ret = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL22, &data);
	if (ret < 0)
		return ret;

	data |= TA_COMMUNICATION_FAIL_MASK;
	pr_info("%s, \n", __func__);

	s2mu107_write_reg(charger->i2c, S2MU107_DC_CTRL22, data);

	return 0;
}

static int s2mu107_dc_get_topoff_current(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, topoff_current = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL15, &data);
	if (ret < 0)
		return ret;
	data = (data & DC_TOPOFF_MASK) >> DC_TOPOFF_SHIFT;

	if (data > 0x0F)
		data = 0x0F;

	topoff_current = data * 100 + 500;
	pr_info("%s, topoff_current : %d(0x%2x)\n", __func__, topoff_current, data);

	return topoff_current;
}

static void s2mu107_dc_set_topoff_current(struct s2mu107_dc_data *charger, int topoff_current)
{
	u8 data = 0x00;

	if (topoff_current <= 500)
		data = 0x00;
	else if (topoff_current > 500 && topoff_current <= 2000)
		data = (topoff_current - 500) / 100;
	else
		data = 0x0F;

	pr_info("%s, topoff_current : %d(0x%2x)\n", __func__, topoff_current, data);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL15,
				data << DC_TOPOFF_SHIFT, DC_TOPOFF_MASK);
}

static int s2mu107_dc_set_cc_band_width(struct s2mu107_dc_data *charger, int band_current)
{
	u8 target;

	if (band_current < DC_CC_BAND_WIDTH_MIN_MA) {
		pr_err("%s, band_current : %d\n", __func__, band_current);
		return -1;
	}

	target = (band_current - DC_CC_BAND_WIDTH_MIN_MA) / DC_CC_BAND_WIDTH_STEP_MA;

	pr_info("%s, band_current : %d(0x%2x)\n", __func__, band_current, target);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL15,
				target, DC_CC_BAND_WIDTH_MASK);
	return 0;
}

static int s2mu107_dc_get_float_voltage(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, float_voltage = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL1, &data);
	if (ret < 0)
		return data;

	data = data & SET_CV_MASK;

	if (data >= 0x7F)
		float_voltage = 4870;
	else if (data <= 0x00)
		float_voltage = 3600;
	else
		float_voltage = data * 10 + 3600;

	pr_info("%s, float_voltage : %d(0x%2x)\n", __func__, float_voltage, data);

	return float_voltage;
}

static void s2mu107_dc_set_float_voltage(struct s2mu107_dc_data *charger, int float_voltage)
{
	u8 data = 0x00;

	if (float_voltage <= 3600)
		data = 0x00;
	else if (float_voltage > 3600 && float_voltage <= 4870)
		data = (float_voltage - 3600) / 10;
	else
		data = 0x7F;

	pr_info("%s, float_voltage : %d(0x%2x)\n", __func__, float_voltage, data);
	charger->floatVol = float_voltage;

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL1,
				data << SET_CV_SHIFT, SET_CV_MASK);
}

static int s2mu107_dc_get_chgin_input_current(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, input_current = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL2, &data);
	if (ret < 0)
		return data;
	data = data & DC_SET_CHGIN_ICHG_MASK;

	input_current = data * 50 + 50;

	pr_info("%s,chgin input_current : %d(0x%2x)\n", __func__, input_current, data);
	return input_current;
}

static int s2mu107_dc_get_wcin_input_current(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, input_current = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL4, &data);
	if (ret < 0)
		return data;
	data = data & DC_SET_WCIN_ICHG_MASK;

	input_current = data * 50 + 50;

	pr_info("%s,wcin input_current : %d(0x%2x)\n", __func__, input_current, data);
	return input_current;
}

static int s2mu107_dc_get_input_current(struct s2mu107_dc_data *charger)
{
	int chgin_current = 0, wcin_current = 0, input_current = 0;

	chgin_current = s2mu107_dc_get_chgin_input_current(charger);
	wcin_current = s2mu107_dc_get_wcin_input_current(charger);

	input_current = chgin_current + wcin_current;

	pr_info("%s, input_current : %d\n", __func__, input_current);
	return input_current;
}

static void s2mu107_dc_set_chgin_input_current(struct s2mu107_dc_data *charger, int input_current)
{
	u8 data = 0x00;
	data = (input_current - 50) / 50;
	if (data >= 0x7F)
		data = 0x7E;
	pr_info("%s, chgin input_current : %d(0x%2x)\n", __func__, input_current, data);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL2,
				data << DC_SET_CHGIN_ICHG_SHIFT, DC_SET_CHGIN_ICHG_MASK);
}

static void s2mu107_dc_set_wcin_input_current(struct s2mu107_dc_data *charger, int input_current)
{
	u8 data = 0x00;
	data = (input_current - 50) / 50;
	if (data == 0x7F)
		data = 0x7E;

	pr_info("%s, wcin input_current : %d(0x%2x)\n", __func__, input_current, data);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL4,
				data << DC_SET_WCIN_ICHG_SHIFT, DC_SET_WCIN_ICHG_MASK);
}

static void s2mu107_dc_set_input_current(struct s2mu107_dc_data *charger, int input_current)
{
	int chgin_current = 0, wcin_current = 0;

	if (input_current < 500)
		input_current = 500;
//	else if (input_current > 6000)
//		input_current = 6000;

	chgin_current = input_current / 2;
	wcin_current = input_current - chgin_current;

	pr_info("%s, chgin : %d, wcin : %d\n", __func__, chgin_current, wcin_current);

	s2mu107_dc_set_chgin_input_current(charger, chgin_current);
	s2mu107_dc_set_wcin_input_current(charger, wcin_current);
}

static int s2mu107_dc_get_charging_current(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, charging_current = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL13, &data);
	if (ret < 0)
		return data;
	data = data & DC_SET_CC_MASK;

	if (data >= 0x63)
		charging_current = 10000;
	else if (data <= 0x00)
		charging_current = 100;
	else
		charging_current = data * 100 + 100;

	pr_info("%s, charging_current : %d(0x%2x)\n", __func__, charging_current, data);
	return charging_current;
}

static void s2mu107_dc_set_charging_current(struct s2mu107_dc_data *charger, int charging_current)
{
	u8 data = 0x00;

	charging_current += 500;

	if (charging_current <= 100)
		data = 0x00;
	else if (charging_current > 100 && charging_current < 10000)
		data = (charging_current - 100) / 100;
	else
		data = 0x63;

	pr_info("%s, charging_current : %d(0x%2x)\n", __func__, charging_current, data);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL13,
				data << DC_SET_CC_SHIFT, DC_SET_CC_MASK);
}

static int s2mu107_dc_get_vbat(struct s2mu107_dc_data *charger)
{
	union power_supply_propval value;
	struct power_supply *psy;
	u32 vbat = 0;
	int ret;

	psy = power_supply_get_by_name(charger->pdata->fg_name);
	if (!psy)
		return -EINVAL;
	value.intval = SEC_BATTERY_VOLTAGE_AVERAGE;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_AVG, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	vbat = value.intval;
	pr_info("%s, Cell Vbatt : (%d)\n", __func__, vbat);
	return vbat;
}

static int s2mu107_dc_get_iavg(struct s2mu107_dc_data *charger)
{
	union power_supply_propval value;
	struct power_supply *psy;
	u32 vbat = 0;
	int ret;

	psy = power_supply_get_by_name(charger->pdata->fg_name);
	if (!psy)
		return -EINVAL;
	value.intval = SEC_BATTERY_CURRENT_MA;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_AVG, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	vbat = value.intval;
	pr_info("%s, IAVG : (%d)\n", __func__, vbat);
	return vbat;
}

static int s2mu107_dc_get_inow(struct s2mu107_dc_data *charger)
{
	union power_supply_propval value;
	struct power_supply *psy;
	u32 vbat = 0;
	int ret;

	psy = power_supply_get_by_name(charger->pdata->fg_name);
	if (!psy)
		return -EINVAL;
	value.intval = SEC_BATTERY_CURRENT_MA;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	vbat = value.intval;
	pr_info("%s, IAVG : (%d)\n", __func__, vbat);
	return vbat;
}

static int s2mu107_dc_get_soc(struct s2mu107_dc_data *charger)
{
	union power_supply_propval value;
	struct power_supply *psy;
	u32 soc = 0;
	int ret;

	psy = power_supply_get_by_name(charger->pdata->fg_name);
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	soc = value.intval;
	return soc;
}

static int s2mu107_dc_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu107_dc_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	union power_supply_propval value;
	struct power_supply *psy_fg, *psy_sc;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->is_charging;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = charger->dc_chg_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		/* TODO : health check */
		psy_sc = power_supply_get_by_name(charger->pdata->sc_name);
		if (!psy_sc)
			return -EINVAL;
		ret = power_supply_get_property(psy_sc, psp, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = s2mu107_dc_is_enable(charger);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = s2mu107_dc_get_input_current(charger);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		/* FG temp? PM temp? */
		psy_fg = power_supply_get_by_name(charger->pdata->fg_name);
		if (!psy_fg)
			return -EINVAL;
		ret = power_supply_get_property(psy_fg, POWER_SUPPLY_PROP_TEMP, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->input_current;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = charger->charging_current;
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:

		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			s2mu107_dc_test_read(charger->i2c);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE:
			switch (charger->dc_state) {
				case DC_STATE_OFF:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
					break;
				case DC_STATE_CHECK_VBAT:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
					break;
				case DC_STATE_PRESET:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_PRESET;
					break;
				case DC_STATE_START_CC:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST;
					break;
				case DC_STATE_CC:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_ON;
					break;
				case DC_STATE_DONE:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
					break;
				default:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
					break;
			}
			break;
#if 0
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			switch (val->intval) {
			case SEC_BATTERY_IIN_MA:
				break;
			case SEC_BATTERY_IIN_UA:
				break;
			case SEC_BATTERY_VIN_MA:
				break;
			case SEC_BATTERY_VIN_UA:
				break;
			default:
				val->intval = 0;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
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

static int s2mu107_dc_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu107_dc_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
//	union power_supply_propval value;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		/* Not Taking input current orders from platform */
		//charger->input_current = val->intval;
		//s2mu107_dc_set_input_current(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Not Taking charging current orders from platform */
		//charger->charging_current = val->intval;
		//s2mu107_dc_set_charging_current(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (val->intval == 0) {
			s2mu107_dc_state_manager(charger, DC_TRANS_DETACH);
			charger->is_charging = false;
		} else {
			if (charger->cable_type) {
				s2mu107_dc_state_manager(charger, DC_TRANS_DETACH);
				charger->is_charging = true;
				s2mu107_dc_state_manager(charger, DC_TRANS_CHG_ENABLED);
			}
		}
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DIRECT_PPS:
			/* PPS INT from the pdic */
			s2mu107_dc_pps_isr(charger, ((s2mu107_dc_pps_signal_t)val->intval));
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_PPS_FAILED:
			/* PS RDY Timeout case */
			s2mu107_dc_set_comm_fail(charger);
			s2mu107_dc_state_manager(charger, DC_TRANS_FAIL_INT);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_VOLTAGE_MAX:
			s2mu107_dc_set_float_voltage(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			/* Not Taking input current orders from platform */
			//charger->input_current = val->intval;
			//s2mu107_dc_set_input_current(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_PPS_READY:
			pr_info("%s, pps ready comes", __func__);
			charger->is_pps_ready = true;
			wake_up_interruptible(&charger->wait);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_DETACHED:
			//s2mu107_dc_state_manager(charger, DC_TRANS_DETACH);
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

static void s2mu107_dc_cal_target_value(struct s2mu107_dc_data *charger)
{
	unsigned int vBatt;
	unsigned int temp;

	vBatt = s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);

	temp = vBatt * 2 * (1031);
	//temp = vBatt * 2 * (1087);
	charger->ppsVol = (((temp / 1000) / DC_TA_VOL_STEP_MV) * DC_TA_VOL_STEP_MV) + 140 + (charger->vchgin_okb_retry * DC_TA_VOL_STEP_MV);

	if (charger->ppsVol > charger->pd_data->taMaxVol) {
		charger->ppsVol = charger->pd_data->taMaxVol;
		charger->vchgin_okb_retry--;
	}

	//charger->targetIIN = charger->pd_data->taMaxPwr / charger->ppsVol;
	charger->chgIIN = (charger->pd_data->taMaxPwr / 4400) - 1000;

	pr_info("%s ppsVol : %d, targetIIN : %d, vchgin_okb_retry : %d\n",
		__func__, charger->ppsVol, charger->targetIIN, charger->vchgin_okb_retry);
}

static void s2mu107_dc_cal_pps_current(struct s2mu107_dc_data *charger)
{
	unsigned int eff_uv;

	eff_uv = (charger->chgIIN * 1000 / 2);
	charger->targetIIN = ((eff_uv / 1000) / DC_TA_CURR_STEP_MA) * DC_TA_CURR_STEP_MA;

	pr_info("%s charger->targetIIN : %d\n", __func__, charger->targetIIN);
}

static int s2mu107_dc_get_pmeter(struct s2mu107_dc_data *charger, s2mu107_pmeter_id_t id)
{
	struct power_supply *psy_pm;
	union power_supply_propval value;
	int ret = 0;

	psy_pm = power_supply_get_by_name(charger->pdata->pm_name);
	if (!psy_pm)
		return -EINVAL;

	switch (id) {
		case PMETER_ID_VCHGIN:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_VCHGIN, &value);
			break;
		case PMETER_ID_ICHGIN:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_ICHGIN, &value);
			break;
		case PMETER_ID_IWCIN:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_IWCIN, &value);
			break;
		case PMETER_ID_VBATT:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_VBAT, &value);
			break;
		case PMETER_ID_VSYS:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_VSYS, &value);
			break;
		case PMETER_ID_TDIE:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_TDIE, &value);
			break;
		default:
			break;
	}

	if (ret < 0) {
		pr_err("%s: fail to set power_suppy pmeter property(%d)\n",
				__func__, ret);
	} else {
		charger->pmeter[id] = value.intval;
		pr_info("%s pm[%s] : %d\n", __func__, pmeter_to_str(id), value.intval);
		return value.intval;
	}
	return -1;
}

static void s2mu107_dc_state_manager(struct s2mu107_dc_data *charger, s2mu107_dc_trans_t trans)
{
	s2mu107_dc_state_t next_state = DC_STATE_OFF;
	mutex_lock(&charger->dc_state_mutex);

	if (trans == DC_TRANS_DETACH || trans == DC_TRANS_FAIL_INT) {
		next_state = DC_STATE_OFF;
		goto HNDL_DC_OFF;
	}

	if (trans == DC_TRANS_DC_DONE_INT) {
		next_state = DC_STATE_DONE;
		goto HNDL_DC_OFF;
	}

	switch (charger->dc_state) {
	case DC_STATE_OFF:
		if (trans == DC_TRANS_CHG_ENABLED)
			next_state = DC_STATE_CHECK_VBAT;
		else
			goto ERR;
		break;
	case DC_STATE_CHECK_VBAT:
		if (trans == DC_TRANS_BATTERY_OK)
			next_state = DC_STATE_PRESET;
		else if (trans == DC_TRANS_BATTERY_NG)
			next_state = DC_STATE_CHECK_VBAT;
		else
			goto ERR;
		break;
	case DC_STATE_PRESET:
		if (trans == DC_TRANS_NORMAL_CHG_INT)
			next_state = DC_STATE_START_CC;
		else if (trans == DC_TRANS_BAT_OKB_INT)
			next_state = DC_STATE_CHECK_VBAT;
		else if (trans == DC_TRANS_CHGIN_OKB_INT)
			next_state = DC_STATE_CHECK_VBAT;
		else
			goto ERR;
		break;
	case DC_STATE_START_CC:
		if (trans == DC_TRANS_RAMPUP)
			next_state = DC_STATE_START_CC;
		else if (trans == DC_TRANS_RAMPUP_FINISHED)
			next_state = DC_STATE_CC;
		else
			goto ERR;
		break;
	case DC_STATE_CC:
		if (trans == DC_TRANS_DC_DONE_INT)
			next_state = DC_STATE_DONE;
		else if (trans == DC_TRANS_CC_STABLED)
			next_state = DC_STATE_SLEEP_CC;
		else
			goto ERR;
		break;
	case DC_STATE_SLEEP_CC:
		if (trans == DC_TRANS_DC_DONE_INT)
			next_state = DC_STATE_DONE;
		else if (trans == DC_TRANS_CC_WAKEUP) {
			next_state = DC_STATE_WAKEUP_CC;
		} else
			goto ERR;
		break;
	case DC_STATE_WAKEUP_CC:
		if (trans == DC_TRANS_DC_DONE_INT)
			next_state = DC_STATE_DONE;
		else if (trans == DC_TRANS_CC_STABLED)
			next_state = DC_STATE_SLEEP_CC;
		else if (trans == DC_TRANS_WAKEUP_DONE)
			next_state = DC_STATE_START_CC;
		else
			goto ERR;
		break;
	case DC_STATE_DONE:
		break;
	default:
		break;
	}

HNDL_DC_OFF:
	pr_info("%s: %s --> %s --> %s", __func__,
		state_to_str(charger->dc_state), trans_to_str(trans),
		state_to_str(next_state));

	charger->dc_state = next_state;
	charger->dc_state_fp[next_state]((void *)charger);

	/* State update */
	s2mu107_dc_platform_state_update(charger);
	mutex_unlock(&charger->dc_state_mutex);

	return;
ERR:
	pr_err("%s err occured, state now : %s\n", __func__,
		state_to_str(charger->dc_state));
	mutex_unlock(&charger->dc_state_mutex);

	return;
}

static void s2mu107_dc_protection(struct s2mu107_dc_data *charger, int onoff)
{
	if (onoff > 0) {
		s2mu107_write_reg(charger->i2c, 0x3A, 0x54);
		usleep_range(1000, 1100);
		s2mu107_write_reg(charger->i2c, 0xC1, 0x00);
		usleep_range(1000, 1100);
		s2mu107_write_reg(charger->i2c, 0x3C, 0x54);
	} else {
		s2mu107_write_reg(charger->i2c, 0x3C, 0x55);
		usleep_range(1000, 1100);
		s2mu107_write_reg(charger->i2c, 0xC1, 0x16);
		usleep_range(1000, 1100);
		s2mu107_write_reg(charger->i2c, 0x3A, 0x55);
	}
}

static void s2mu107_dc_state_off(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;
	pr_info("%s\n", __func__);
	wake_unlock(&charger->wake_lock);

	charger->is_charging = false;
	charger->dc_chg_status = POWER_SUPPLY_STATUS_DISCHARGING;

	charger->step_now = -1;
	charger->is_longCC = false;
	alarm_cancel(&charger->step_monitor_alarm);
	cancel_delayed_work_sync(&charger->step_monitor_work);

	/* Set VSYS recover */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL8_DC, 0x8, 0xF);

	if (charger->pd_data->pdo_pos) {
	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MU107_DC_DISABLE);
		charger->pd_data->pdo_pos = 0;
	}

	s2mu107_dc_enable(charger, S2MU107_DC_DISABLE);

	cancel_delayed_work(&charger->timer_wqueue);
	cancel_delayed_work(&charger->check_vbat_wqueue);
	cancel_delayed_work(&charger->start_cc_wqueue);
	return;
}

static void s2mu107_dc_state_check_vbat(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;
	unsigned int vBatt, soc;
	union power_supply_propval value;
	struct power_supply *psy;
	int ret;

	soc = s2mu107_dc_get_soc(charger);
	vBatt = s2mu107_dc_get_vbat(charger);

	msleep(300);
	select_pdo(1);
	msleep(100);

	pr_info("%s, soc : %d, vBatt : %d\n", __func__, soc, vBatt);

	if (vBatt > DC_MIN_VBAT && soc < DC_MAX_SOC) {
		if (charger->dc_chg_status == SC_CHARGING_ENABLE_CHECK_VBAT){
			psy = power_supply_get_by_name(charger->pdata->sc_name);
			if (!psy)
				return ;
			value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;
			ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);
			if (ret < 0)
				pr_err("%s: fail to enable Switching charger\n", __func__);
		}
		charger->dc_trans = DC_TRANS_BATTERY_OK;
		schedule_delayed_work(&charger->state_manager_wqueue, msecs_to_jiffies(0));
	} else {
		if (charger->dc_chg_status != SC_CHARGING_ENABLE_CHECK_VBAT){
			psy = power_supply_get_by_name(charger->pdata->sc_name);
			if (!psy)
				return ;
			value.intval = SEC_BAT_CHG_MODE_CHARGING;
			ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);
			if (ret < 0)
				pr_err("%s: fail to enable Switching charger\n", __func__);

			charger->dc_chg_status = SC_CHARGING_ENABLE_CHECK_VBAT;
		}
		schedule_delayed_work(&charger->check_vbat_wqueue, msecs_to_jiffies(1000));
	}

	return;
}

static void s2mu107_dc_fg_set_iavg(struct s2mu107_dc_data *charger, int onoff)
{
	union power_supply_propval value;
	struct power_supply *psy;
	int ret;

	psy = power_supply_get_by_name(charger->pdata->fg_name);
	if (!psy)
		return ;

	if (onoff > 0)
		value.intval = ENABLE;
	else
		value.intval = DISABLE;

	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_FAST_IAVG, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
}

static void s2mu107_dc_ir_drop_compensation(struct s2mu107_dc_data *charger)
{
	int vchgin;
	int comp_pps_vol;

	vchgin = s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
	comp_pps_vol = charger->ppsVol;

	while (charger->ppsVol > vchgin) {
		pr_info("%s comp_pps_vol : %d\n", __func__, comp_pps_vol);
		comp_pps_vol += DC_TA_VOL_STEP_MV;
		sec_pd_select_pps(charger->pd_data->pdo_pos, comp_pps_vol, charger->minIIN);
		msleep(500);
		vchgin = s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);

		if (comp_pps_vol >= charger->pd_data->taMaxVol)
			break;
	}
	pr_info("%s ppsVol : %d, vchgin : %d, pass\n", __func__, comp_pps_vol, vchgin);
	charger->ppsVol = comp_pps_vol - DC_TA_VOL_STEP_MV;
	pr_info("%s ppsVol : %d, pass2\n", __func__, charger->ppsVol);

	return;
}

static void s2mu107_dc_state_preset(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	unsigned int pdo_pos = 0;
	unsigned int taMaxVol = 4400 * 2;
	unsigned int taMaxCur;
	unsigned int taMaxPwr;

	unsigned int charging_current, input_current;
	unsigned int max_chg_current;

	mutex_lock(&charger->dc_mutex);

	charger->rise_speed_dly_ms = 10;

	sec_pd_get_apdo_max_power(&pdo_pos, &taMaxVol, &taMaxCur, &taMaxPwr);
	charger->pd_data->pdo_pos	= pdo_pos;
	charger->pd_data->taMaxVol	= taMaxVol;
	charger->pd_data->taMaxCur	= taMaxCur;
	charger->pd_data->taMaxPwr 	= taMaxPwr;

	/* get step charging current */
	charger->ta_pwr_type = (taMaxCur == 5000) ? TA_PWR_TYPE_45W : TA_PWR_TYPE_25W;

	pr_info("%s pdo_pos : %d, taMaxVol : %d, taMaxCur : %d, taMaxPwr : %d\n",
		__func__, pdo_pos, taMaxVol, taMaxCur, taMaxPwr);

	s2mu107_dc_cal_target_value(charger);

	charger->minIIN		= DC_TA_MIN_CURRENT;
	charger->adjustIIN	= DC_TA_MIN_CURRENT;

	pr_info("%s: sec_pd_select_pps, pdo_pos : %d, ppsVol : %d, curr : %d\n",
		__func__, charger->pd_data->pdo_pos, charger->ppsVol, charger->minIIN);
	sec_pd_select_pps(charger->pd_data->pdo_pos, charger->ppsVol, charger->minIIN);

	//input_current = charger->targetIIN - 100;
	//charging_current = charger->targetIIN * 2 - 1000;
	input_current = 12000;
	charging_current = charger->chgIIN;

	s2mu107_dc_test_read(charger->i2c);

	if (charger->ta_pwr_type == TA_PWR_TYPE_45W)
		max_chg_current = charger->pdata->dc_step_current_45w[0];
	else
		max_chg_current = charger->pdata->dc_step_current_25w[0];

	if (charging_current > max_chg_current)
		charger->chgIIN = charging_current = max_chg_current;

	//charger->targetIIN = taMaxCur;
	s2mu107_dc_cal_pps_current(charger);

	s2mu107_dc_set_input_current(charger, input_current);
	s2mu107_dc_set_charging_current(charger, charging_current);
	//s2mu107_dc_set_topoff_current(charger, charger->topoff_current);
	s2mu107_dc_set_topoff_current(charger, 1000);

	/* DC + Buck operating protection */
	s2mu107_dc_protection(charger, S2MU107_DC_ENABLE);

	/* Set WCIN Pwr TR */
	s2mu107_dc_set_wcin_pwrtr(charger);

	/* Set VSYS Rise */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL8_DC, 0x9, 0xF);

	/* Set Dual Buck */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL0,
				SC_REG_MODE_DUAL_BUCK,
				SC_REG_MODE_MASK);

	/* Set TFB value */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL11, 0x38, 0x38);

	/* Set cc band to minimum */
	s2mu107_dc_set_cc_band_width(charger, DC_CC_BAND_WIDTH_MIN_MA);

	/* VBAT_OK_LEVEL 3.4V */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL23, 0x00, DC_SEL_BAT_OK_MASK);

	/* Long CC Step 100mA */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16, 0x00, DC_LONG_CC_STEP_MASK);

	/* OCP Off */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_TEST2,
				0x0 << DC_EN_OCP_FLAG_SHIFT,
				DC_EN_OCP_FLAG_MASK);

	/* step CC difference setting */
	s2mu107_write_reg(charger->i2c, S2MU107_DC_CD_OTP_01, 0x02);

	/* DC_EN_CV off */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_TEST6, 0x00, 0x00);

	/* Expand VBUS Target range */
	s2mu107_write_reg(charger->i2c, S2MU107_DC_CTRL19, 0x1F);

	/* tSET_VF_VBAT */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_OTP103, 0x04, T_SET_VF_VBAT_MASK);

	/* STEP CC disable */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16,
				0x0, DC_EN_LONG_CC_MASK);

	/* LONG_CC_SEL_CELL_PACKB */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16,
				0x20, 0x20);

	/* Thermal Temperature 40, debounce 1.6sec */
	/*
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL20,
				DC_EN_THERMAL_LOOP_MASK, DC_EN_THERMAL_LOOP_MASK);
	*/

	/* 50 degree */
	//s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL20, 0x0F, THERMAL_RISING_MASK);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL23,
				0x04 << THERMAL_WAIT_SHIFT, THERMAL_WAIT_MASK);

	/* BYP OVP Disable */
	s2mu107_write_reg(charger->i2c, S2MU107_DC_INPUT_OTP_04, 0xCA);

	/* RCP offset trim : Stabling DC off sequence */
	s2mu107_write_reg(charger->i2c_common, 0x62, 0x90);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);


	s2mu107_dc_test_read(charger->i2c);

	/* VSYS UP */
	s2mu107_update_reg(charger->i2c, 0x20, 0x9, 0xF);

	s2mu107_write_reg(charger->i2c, 0xC6, 0x3F);
	s2mu107_write_reg(charger->i2c, 0xCC, 0x7F);

	/* Set Float Voltage Max */
	s2mu107_write_reg(charger->i2c, S2MU107_DC_CTRL1, 0x5F);

	/* Set Dual Buck */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL0,
				SC_REG_MODE_DUAL_BUCK,
				SC_REG_MODE_MASK);

	msleep(500);

	/* Set Dual Buck */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL0,
				SC_REG_MODE_DUAL_BUCK,
				SC_REG_MODE_MASK);

	s2mu107_dc_test_read(charger->i2c);

	s2mu107_dc_ir_drop_compensation(charger);

	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);

	s2mu107_dc_fg_set_iavg(charger, ENABLE);
	/* dc en */
	s2mu107_dc_enable(charger, S2MU107_DC_ENABLE);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
	s2mu107_dc_test_read(charger->i2c);

	mutex_unlock(&charger->dc_mutex);

	pr_info("%s: input_current : %d, charging_current : %d\n",
		__func__, charger->input_current, charger->charging_current);

	return;
}

static void s2mu107_dc_state_start_cc(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;
	unsigned int targetIIN = charger->targetIIN;
	unsigned int iChgin;

	charger->dc_chg_status = POWER_SUPPLY_STATUS_CHARGING;

	charger->vchgin_okb_retry = 0;
	iChgin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN);

	if (charger->step_now < 0) {
		if (charger->adjustIIN >= targetIIN) {
			pr_info("%s Rampup Finished, iChgin : %d\n", __func__, iChgin);
			cancel_delayed_work(&charger->start_cc_wqueue);
			charger->dc_trans = DC_TRANS_RAMPUP_FINISHED;
			schedule_delayed_work(&charger->state_manager_wqueue, msecs_to_jiffies(0));
		} else {
			schedule_delayed_work(&charger->start_cc_wqueue, msecs_to_jiffies(100));
		}
	} else {
		if (charger->ppsVol >= charger->pd_data->taMaxVol) {
			pr_info("%s Rampup Finished, ppsVol : %d\n", __func__, charger->ppsVol);
			cancel_delayed_work(&charger->start_cc_wqueue);
			charger->dc_trans = DC_TRANS_RAMPUP_FINISHED;
			schedule_delayed_work(&charger->state_manager_wqueue, msecs_to_jiffies(0));
		} else {
			schedule_delayed_work(&charger->start_cc_wqueue, msecs_to_jiffies(100));
		}
	}

	return;
}

static void s2mu107_dc_state_cc(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	charger->step_mon_cnt = 0;

	cancel_delayed_work(&charger->timer_wqueue);
	schedule_delayed_work(&charger->timer_wqueue, msecs_to_jiffies(2000));
	charger->cnt_vsys_warn = 0;
	/* Set Dual Buck */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL0,
				SC_REG_MODE_DUAL_BUCK,
				SC_REG_MODE_MASK);

	msleep(3000);
	pr_info("%s TA_TRANSIENT_DONE", __func__);
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL22,
		TA_TRANSIENT_DONE_MASK, TA_TRANSIENT_DONE_MASK);
	s2mu107_write_reg(charger->i2c, 0xB, 0x0);
	s2mu107_write_reg(charger->i2c, 0xC, 0x0);
	s2mu107_write_reg(charger->i2c, 0xD, 0x0);
	s2mu107_write_reg(charger->i2c, 0xE, 0x0);

	/* 75 degree */
	//s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL20, 0x2D, THERMAL_RISING_MASK);
	//s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL20, 0x2D, THERMAL_RISING_MASK);

	/* Thermal Temperature 40, debounce 1.6sec */
	/* Disabled */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL20,
				0, DC_EN_THERMAL_LOOP_MASK);

	/* STEP CC enable */
	/* Set Float Voltage to 4.4V */
	s2mu107_write_reg(charger->i2c, S2MU107_DC_CTRL1, 0x5F);

#if 1
	/* step 100mA */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16, 0x00,
				DC_LONG_CC_STEP_MASK);

	msleep(5000);

	/* LongCC Disabled */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16,
				0, DC_EN_LONG_CC_MASK);
#endif
	return;
}

static void s2mu107_dc_state_sleep_cc(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	pr_info("%s Enter", __func__);

	s2mu107_dc_forced_enable(charger, S2MU107_DC_ENABLE);
	/* Set cc band to 400mA */
	s2mu107_dc_set_cc_band_width(charger, DC_CC_BAND_WIDTH_MIN_MA);
	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MU107_DC_ENABLE);

	cancel_delayed_work(&charger->timer_wqueue);
	cancel_delayed_work(&charger->check_vbat_wqueue);
	cancel_delayed_work(&charger->start_cc_wqueue);

	msleep(3000);

	wake_unlock(&charger->wake_lock);
}

static void s2mu107_dc_state_wakeup_cc(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	pr_info("%s Enter", __func__);

	wake_lock(&charger->wake_lock);

	charger->rise_speed_dly_ms = 10;
	charger->ppsVol = (unsigned int)sec_get_pps_voltage();
	charger->chgIIN = s2mu107_dc_get_target_chg_current(charger, charger->step_now);

	if (charger->pd_data->pdo_pos) {
		sec_pps_enable(charger->pd_data->pdo_pos,
			charger->ppsVol, charger->targetIIN, S2MU107_DC_DISABLE);
	}
	charger->dc_trans = DC_TRANS_WAKEUP_DONE;
	schedule_delayed_work(&charger->state_manager_wqueue, msecs_to_jiffies(0));
}

static void s2mu107_dc_state_done(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	cancel_delayed_work(&charger->timer_wqueue);

	charger->is_charging = false;

	/* cancel step monitor */
	charger->step_now = -1;
	charger->is_longCC = false;
	alarm_cancel(&charger->step_monitor_alarm);
	cancel_delayed_work_sync(&charger->step_monitor_work);


	if (charger->pd_data->pdo_pos) {
	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MU107_DC_DISABLE);
		charger->pd_data->pdo_pos = 0;

		msleep(100);
	}

	s2mu107_dc_enable(charger, S2MU107_DC_DISABLE);
	s2mu107_dc_protection(charger, S2MU107_DC_DISABLE);

	s2mu107_update_reg(charger->i2c, 0x20, 0x8, 0xF);

	s2mu107_write_reg(charger->i2c, 0xC6, 0x20);
	s2mu107_write_reg(charger->i2c, 0xCC, 0x20);

	s2mu107_dc_fg_set_iavg(charger, DISABLE);
	/*
	 * Ctrl SC Charger Here.
	 */
	charger->prop_data.value.intval = ENABLE;
	charger->prop_data.prop = POWER_SUPPLY_EXT_PROP_DIRECT_DONE;
	schedule_delayed_work(&charger->set_prop_wqueue, 0);

	return;
}

static int s2mu107_dc_send_fenced_pps(struct s2mu107_dc_data *charger)
{
	if (charger->ppsVol > charger->pd_data->taMaxVol)
		charger->ppsVol = charger->pd_data->taMaxVol;

	if (charger->targetIIN > charger->pd_data->taMaxCur)
		charger->targetIIN = charger->pd_data->taMaxCur;

	if (charger->targetIIN < charger->minIIN)
		charger->targetIIN = charger->minIIN;

	pr_info("%s vol : %d, curr : %d\n", __func__, charger->ppsVol, charger->targetIIN);
	sec_pd_select_pps(charger->pd_data->pdo_pos, charger->ppsVol, charger->targetIIN);

	return 1;
}

static void s2mu107_dc_timer_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 timer_wqueue.work);
	int ret;
	u8 val;

	mutex_lock(&charger->timer_mutex);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_TDIE);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL14, &val);
	pr_info("%s READ Charging current : 0x%X: %d mA\n", __func__, val & 0x7F, ((val & 0x7F) * 100));
	s2mu107_read_reg(charger->i2c, S2MU107_DC_CD_OTP_02, &val);
	pr_info("%s D9. CC_IINB_CTRL : 0x%X\n", __func__, val & 0x8);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_TEST6, &val);
	pr_info("%s CV_EN(0x61) : 0x%X\n", __func__, val & 0x3);
	//s2mu107_dc_cal_input_current(charger);
	/* Set Dual Buck */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL0,
				SC_REG_MODE_DUAL_BUCK,
				SC_REG_MODE_MASK);

	ret = s2mu107_dc_send_fenced_pps(charger);

	if (charger->ppsVol == charger->pd_data->taMaxVol) {
		s2mu107_dc_forced_enable(charger, S2MU107_DC_ENABLE);
			charger->cc_count++;
		}

		if (charger->cc_count > 5 ) {
			charger->dc_trans = DC_TRANS_CC_STABLED;
			schedule_delayed_work(&charger->state_manager_wqueue, msecs_to_jiffies(0));
		charger->cc_count = 0;
	}
		charger->prev_ppsVol = charger->ppsVol;
		charger->prev_ppsCurr = charger->targetIIN;

	pr_info("%s send pps : %d, cc_count : %d\n", __func__, ret, charger->cc_count);
	schedule_delayed_work(&charger->timer_wqueue, msecs_to_jiffies(5000));
	mutex_unlock(&charger->timer_mutex);
}

static void s2mu107_dc_start_cc_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 start_cc_wqueue.work);
	unsigned int ppsVol;
	mutex_lock(&charger->dc_mutex);

	if (charger->step_now < 0) {
	ppsVol = charger->ppsVol;
	charger->adjustIIN += DC_TA_CURR_STEP_MA;
	pr_info("%s, charger->adjustIIN : %d\n", __func__, charger->adjustIIN);
		sec_pd_select_pps(charger->pd_data->pdo_pos, charger->ppsVol, charger->adjustIIN);
	} else {
		charger->ppsVol += DC_TA_VOL_STEP_MV;
		s2mu107_dc_send_fenced_pps(charger);
	}
	s2mu107_dc_state_manager(charger, DC_TRANS_RAMPUP);

	if (charger->pdata->step_charge_level > 1) {
		alarm_cancel(&charger->step_monitor_alarm);
		alarm_start_relative(&charger->step_monitor_alarm, ktime_set(charger->alarm_interval, 0));
	}
	mutex_unlock(&charger->dc_mutex);
}

static void s2mu107_dc_check_vbat_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 check_vbat_wqueue.work);

	mutex_lock(&charger->dc_mutex);
	pr_info("%s\n", __func__);
	s2mu107_dc_state_manager(charger, DC_TRANS_BATTERY_NG);
	mutex_unlock(&charger->dc_mutex);
}

static void s2mu107_dc_state_manager_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 state_manager_wqueue.work);

	pr_info("%s\n", __func__);
	s2mu107_dc_state_manager(charger, charger->dc_trans);
}

static void s2mu107_dc_set_prop_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 set_prop_wqueue.work);
	struct power_supply *psy;
	int ret;

	pr_info("%s\n", __func__);
	psy = power_supply_get_by_name(charger->pdata->sec_dc_name);
	if (!psy) {
		pr_err("%s, can't get power supply", __func__);
		return;
	}
	ret = power_supply_set_property(psy, charger->prop_data.prop,
		&(charger->prop_data.value));
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	return;
}

static int s2mu107_dc_pps_isr(struct s2mu107_dc_data *charger, s2mu107_dc_pps_signal_t sig)
{
	int vbatt, vsys, inow, target_cur;
	int wait_ret = 0;
	u8 val;

	mutex_lock(&charger->pps_isr_mutex);

	pr_info("%s sig : %d, ppsVol : %d, cnt_vsys_warn : %d\n",
	__func__, sig, charger->ppsVol, charger->cnt_vsys_warn);

	if (charger->dc_state == DC_STATE_CC) {
	if (sig == S2MU107_DC_PPS_SIGNAL_DEC) {
			charger->cnt_vsys_warn = 0;
		charger->ppsVol -= DC_TA_VOL_STEP_MV;
	} else if (sig == S2MU107_DC_PPS_SIGNAL_INC) {
			vbatt = s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);
			vsys = s2mu107_dc_get_pmeter(charger, PMETER_ID_VSYS);

			inow = s2mu107_dc_get_inow(charger);
			s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL14, &val);
			target_cur = ((val & 0x7F) * 100);
			pr_info("%s READ Chging current : %d mA\n", __func__, target_cur);

			if (inow > target_cur) {
				pr_info("%s skipped increasing, inow : %d\n", __func__, inow);
				goto SKIP_PPS_ISR;
			}

			if (vsys <= vbatt) {
				charger->cnt_vsys_warn++;
				pr_info("%s skipped, vsys : %d, vbatt : %d\n", __func__, vsys, vbatt);
			} else if (vsys - vbatt <= 200) {
				charger->cnt_vsys_warn++;
				pr_info("%s skipped, vsys : %d, vbatt : %d\n", __func__, vsys, vbatt);
			} else {
				charger->cnt_vsys_warn = 0;
		charger->ppsVol += DC_TA_VOL_STEP_MV;
				pr_info("%s UP!, vsys : %d, vbatt : %d\n", __func__, vsys, vbatt);
			}

			if (charger->cnt_vsys_warn >= 5) {
				pr_info("%s reduce chgiin sig : %d, ppsVol : %d, cnt_vsys_warn : %d\n",
				__func__, sig, charger->ppsVol, charger->cnt_vsys_warn);
				charger->cnt_vsys_warn = 0;
				charger->chgIIN -= 500;
				s2mu107_dc_set_charging_current(charger, charger->chgIIN);
			}
	} else {
		pr_err("%s err\n", __func__);
			mutex_unlock(&charger->pps_isr_mutex);
		return -1;
	}

		charger->is_pps_ready = false;
		cancel_delayed_work(&charger->timer_wqueue);
		schedule_delayed_work(&charger->timer_wqueue, msecs_to_jiffies(0));
		wait_ret = wait_event_interruptible_timeout(charger->wait,
				charger->is_pps_ready,
				msecs_to_jiffies(2000));
	} else {
		pr_info("%s skipped by state check, curr state : %s", __func__,
			state_to_str(charger->dc_state));
		mutex_unlock(&charger->pps_isr_mutex);
		return -1;
	}

SKIP_PPS_ISR:
	if (charger->rise_speed_dly_ms <= 10)
	usleep_range(10000, 11000);
	else
		msleep(charger->rise_speed_dly_ms);
	pr_info("%s TA_TRANSIENT_DONE", __func__);
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL22,
		TA_TRANSIENT_DONE_MASK, TA_TRANSIENT_DONE_MASK);

	mutex_unlock(&charger->pps_isr_mutex);
	return 1;
}

static irqreturn_t s2mu107_dc_mode_isr(int irq, void *data)
{
	struct s2mu107_dc_data *charger = data;
	u8 int0, int1, int2, int3;

	wake_lock(&charger->mode_irq);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT0, &int0);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT1, &int1);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT2, &int2);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT3, &int3);

	pr_info("%s DC mode change(0x%2x, 0x%2x, 0x%2x, 0x%2x) :",
			__func__, int0, int1, int2, int3);

	if (irq == charger->irq_cc) {
			/* status DC_START_CC */
			pr_info("%s Normal CC detected\n", __func__);
			s2mu107_dc_state_manager(charger, DC_TRANS_NORMAL_CHG_INT);
	} else if (irq == charger->irq_done){
		pr_info("%s Finish DC detected\n", __func__);
		/* Done detect alarm */
		/* status DC_DONE */
		s2mu107_dc_state_manager(charger, DC_TRANS_DC_DONE_INT);
	}
	wake_unlock(&charger->mode_irq);

	return IRQ_HANDLED;
}

static irqreturn_t s2mu107_dc_fail_isr(int irq, void *data)
{
	struct s2mu107_dc_data *charger = data;
	u8 int0, int1, int2, int3;
	bool chkbat = false;
	bool chkcv = false;
	bool skip_isr = false;
	u8 val;

	wake_lock(&charger->fail_irq);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT0, &int0);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT1, &int1);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT2, &int2);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT3, &int3);

	pr_info("%s DC off (0x%2x, 0x%2x, 0x%2x, 0x%2x) :",
			__func__, int0, int1, int2, int3);

	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_TDIE);

	s2mu107_dc_set_wcin_pwrtr(charger);

	if (irq == charger->irq_vchgin_okb) {
		pr_info(" VCHGIN not ready\n");
		s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
		chkbat = true;
		charger->vchgin_okb_retry++;
		msleep(100);
		s2mu107_dc_state_manager(charger, DC_TRANS_CHGIN_OKB_INT);
	} else if (irq == charger->irq_vbat_okb) {
		pr_info(" VBAT not ready\n");
		chkbat = true;
		s2mu107_dc_state_manager(charger, DC_TRANS_BAT_OKB_INT);
	} else if (irq == charger->irq_byp2out_ovp) {
		pr_info(" byp2 out voltage\n");
	} else if (irq == charger->irq_byp_ovp) {
		pr_info(" byp OVP detected\n");
	} else if (irq == charger->irq_out_ovp) {
		pr_info(" out OVP detected\n");
	} else if (irq == charger->irq_ocp) {
		pr_info(" Over current protection\n");
		/* DC_PRESET */
	} else if (irq == charger->irq_rcp) {
		pr_info(" Reverse current protection\n");
		/* DC_PRESET */
	} else if (irq == charger->irq_plug_out) {
		pr_info(" DC TA detached\n");
		skip_isr = false;
	} else if (irq == charger->irq_diod_prot) {
		pr_info(" diod protection\n");
		/* DC_PRESET */
	} else if (irq == charger->irq_tsd) {
		pr_info(" Thermal Shut down\n");
	} else if (irq == charger->irq_tfd) {
		pr_info(" TFB interrupt\n");
		/* pps v-*/
	} else if (irq == charger->irq_cv) {
		pr_info(" CV loop interrupt\n");
		chkcv = true;
	}else {
		s2mu107_read_reg(charger->i2c, S2MU107_DC_TEST6, &val);
		pr_info("%s CV_EN(0x61) : 0x%X\n", __func__, val & 0x3);
		pr_info(" DC off\n");
	}
	if (!chkbat && !chkcv && !skip_isr) {
		s2mu107_dc_state_manager(charger, DC_TRANS_FAIL_INT);
		charger->vchgin_okb_retry = 0;
	}

	wake_unlock(&charger->fail_irq);

	return IRQ_HANDLED;
}

static irqreturn_t s2mu107_dc_thermal_isr(int irq, void *data)
{
	struct s2mu107_dc_data *charger = data;
	u8 int0, int1, int2, int3;

	wake_lock(&charger->thermal_irq);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT0, &int0);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT1, &int1);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT2, &int2);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT3, &int3);

	pr_info("%s Thermal Status! (0x%2x, 0x%2x, 0x%2x, 0x%2x) :",
			__func__, int0, int1, int2, int3);

	wake_unlock(&charger->thermal_irq);

	return IRQ_HANDLED;
}

static int s2mu107_dc_irq_init(struct s2mu107_dc_data *charger)
{
	int ret = 0;
	int irq_base;

	if (charger->irq_base == 0)
		return -1;

	irq_base = charger->irq_base;

	charger->irq_cc = irq_base + S2MU107_DC_IRQ0_DC_NORMAL_CHARGING;
	ret = request_threaded_irq(charger->irq_cc, NULL,
			s2mu107_dc_mode_isr, 0, "dc-cc", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_done = irq_base + S2MU107_DC_IRQ2_DC_DONE;
	ret = request_threaded_irq(charger->irq_done, NULL,
			s2mu107_dc_mode_isr, 0, "dc-done", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_cv = irq_base + S2MU107_DC_IRQ2_DC_CHGIN_CV;
	ret = request_threaded_irq(charger->irq_cv, NULL,
			s2mu107_dc_fail_isr, 0, "dc-cv", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_pps_fail = irq_base + S2MU107_DC_IRQ2_DC_PPS_FAIL;
	ret = request_threaded_irq(charger->irq_pps_fail, NULL,
			s2mu107_dc_fail_isr, 0, "dc-ppsfail", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_vchgin_okb = irq_base + S2MU107_DC_IRQ0_DC_CHGIN_OKB;
	ret = request_threaded_irq(charger->irq_vchgin_okb, NULL,
			s2mu107_dc_fail_isr, 0, "dc-chgin-fail", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_vbat_okb = irq_base + S2MU107_DC_IRQ0_DC_BAT_OKB;
	ret = request_threaded_irq(charger->irq_vbat_okb, NULL,
			s2mu107_dc_fail_isr, 0, "dc-bat-fail", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_byp2out_ovp = irq_base + S2MU107_DC_IRQ0_DC_BYP2OUT_OVP;
	ret = request_threaded_irq(charger->irq_byp2out_ovp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-byp2out-ovp", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_byp_ovp = irq_base + S2MU107_DC_IRQ0_DC_BYP_OVP;
	ret = request_threaded_irq(charger->irq_byp_ovp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-bypovp", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_out_ovp = irq_base + S2MU107_DC_IRQ0_DC_OUT_OVP;
	ret = request_threaded_irq(charger->irq_out_ovp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-outovp", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_ocp = irq_base + S2MU107_DC_IRQ1_DC_OCP;
	ret = request_threaded_irq(charger->irq_ocp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-ocp", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_rcp = irq_base + S2MU107_DC_IRQ1_DC_CHGIN_RCP;
	ret = request_threaded_irq(charger->irq_rcp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-rcp", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_plug_out = irq_base + S2MU107_DC_IRQ1_DC_CHGIN_PLUG_OUT;
	ret = request_threaded_irq(charger->irq_plug_out, NULL,
			s2mu107_dc_fail_isr, 0, "dc-plugout", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_diod_prot = irq_base + S2MU107_DC_IRQ1_DC_CHGIN_DIODE_PROT;
	ret = request_threaded_irq(charger->irq_diod_prot, NULL,
			s2mu107_dc_fail_isr, 0, "dc-diod_prot", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_thermal = irq_base + S2MU107_DC_IRQ2_DC_THERMAL;
	ret = request_threaded_irq(charger->irq_thermal, NULL,
			s2mu107_dc_thermal_isr, 0, "dc-thermal", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_tsd = irq_base + S2MU107_DC_IRQ3_DC_TSD;
	ret = request_threaded_irq(charger->irq_tsd, NULL,
			s2mu107_dc_fail_isr, 0, "dc-tsd", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_tfd = irq_base + S2MU107_DC_IRQ3_DC_TFB_RISING;
	ret = request_threaded_irq(charger->irq_tfd, NULL,
			s2mu107_dc_fail_isr, 0, "dc-tfb", charger);
	if (ret < 0)
		goto err_irq;

	s2mu107_write_reg(charger->i2c, 0xB, 0x0);
	s2mu107_write_reg(charger->i2c, 0xC, 0x0);
	s2mu107_write_reg(charger->i2c, 0xD, 0x0);
	s2mu107_write_reg(charger->i2c, 0xE, 0x0);

err_irq:
	return ret;
}

static int s2mu107_dc_set_wcin_pwrtr(struct s2mu107_dc_data *charger)
{
	u8 data;
	int ret;
	return 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_TEST4, &data);

	if (ret) {
		pr_err("%s i2c read failed.\n", __func__);
		return ret;
	}

	data |= T_DC_EN_WCIN_PWRTR_MODE_MASK;
	s2mu107_write_reg(charger->i2c, S2MU107_DC_TEST4, data);

	return 0;
}

static bool s2mu107_dc_init(struct s2mu107_dc_data *charger)
{
//No error
	int topoff_current = s2mu107_dc_get_topoff_current(charger);
	int float_voltage = s2mu107_dc_get_float_voltage(charger);
	int input_current = s2mu107_dc_get_input_current(charger);
	int charging_current = s2mu107_dc_get_charging_current(charger);

	s2mu107_dc_set_topoff_current(charger, topoff_current);
	s2mu107_dc_set_float_voltage(charger, float_voltage);
	s2mu107_dc_set_input_current(charger, input_current);
	s2mu107_dc_set_charging_current(charger, charging_current);
	s2mu107_dc_is_enable(charger);
	s2mu107_dc_pm_enable(charger);
	s2mu107_dc_enable(charger, 0);
	s2mu107_dc_set_wcin_pwrtr(charger);

	charger->dc_state = DC_STATE_OFF;
	charger->dc_state_fp[DC_STATE_OFF]	 	= s2mu107_dc_state_off;
	charger->dc_state_fp[DC_STATE_CHECK_VBAT] 	= s2mu107_dc_state_check_vbat;
	charger->dc_state_fp[DC_STATE_PRESET] 		= s2mu107_dc_state_preset;
	charger->dc_state_fp[DC_STATE_START_CC] 	= s2mu107_dc_state_start_cc;
	charger->dc_state_fp[DC_STATE_CC] 		= s2mu107_dc_state_cc;
	charger->dc_state_fp[DC_STATE_SLEEP_CC]		= s2mu107_dc_state_sleep_cc;
	charger->dc_state_fp[DC_STATE_WAKEUP_CC]	= s2mu107_dc_state_wakeup_cc;
	charger->dc_state_fp[DC_STATE_DONE] 		= s2mu107_dc_state_done;
	charger->vchgin_okb_retry = 0;
	charger->step_now = -1;
	charger->is_longCC = false;
	charger->cnt_vsys_warn = 0;
	charger->cc_count = 0;

	charger->shifting_cnt = 0;
	charger->is_shifting = false;
	charger->step_mon_cnt = 0;
	charger->is_pps_ready = false;
	charger->rise_speed_dly_ms = 10;

	/* Default Thermal Setting */
	s2mu107_write_reg(charger->i2c, 0x55, 0x7F);
	s2mu107_write_reg(charger->i2c, 0x56, 0x3F);

	return true;
}

static void s2mu107_dc_set_auto_pps_param(struct s2mu107_dc_data *charger)
{
	int step_now = charger->step_now;

	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MU107_DC_DISABLE);

	usleep_range(20000, 21000);

	charger->chgIIN = s2mu107_dc_get_target_chg_current(charger, step_now);
	s2mu107_dc_cal_pps_current(charger);
	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MU107_DC_ENABLE);
}

static void s2mu107_dc_slow_step(struct s2mu107_dc_data *charger)
{
	int iavg;
	int step_now = charger->step_now;
	int target_curr;

	if (!charger->is_shifting)
		return;

	target_curr = s2mu107_dc_get_target_chg_current(charger, step_now);
	iavg = s2mu107_dc_get_iavg(charger);

	s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);
	pr_info("%s iavg : %d mA, target_curr : %d\n",
		__func__, iavg, target_curr);
	if (iavg <= target_curr) {
		pr_info("%s Get Out Slow Step\n", __func__);
		s2mu107_dc_forced_enable(charger, S2MU107_DC_ENABLE);

		charger->is_longCC = S2MU107_DC_DISABLE;

		/* Long CC Step 600mA */
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16, 0x05, DC_LONG_CC_STEP_MASK);

		/* CV Vol to 4.55V */
		s2mu107_write_reg(charger->i2c, S2MU107_DC_CTRL1, 0x5F);
		s2mu107_write_reg(charger->i2c, S2MU107_DC_CD_OTP_01, 0x0);
		charger->is_shifting = S2MU107_DC_DISABLE;

		/* Set new input current. */
		s2mu107_dc_cal_pps_current(charger);

		/* Set new charging current. */
		s2mu107_dc_set_charging_current(charger, target_curr);
		charger->dc_trans = DC_TRANS_CC_WAKEUP;
		schedule_delayed_work(&charger->state_manager_wqueue, msecs_to_jiffies(0));
	}
}

static void s2mu107_dc_set_lcc_by_step(struct s2mu107_dc_data *charger, int next_step)
{
	int v_float_uv = 4500 * 1000;
	int v_step_float_uv = 0;
	int v_diff_gap = 0;
	int iavg = 0;

	u8 data = 0;

	if (next_step < 1)
		return;

	switch (next_step) {
	case 1:
	case 2:
	case 3:
		sec_pps_enable(charger->pd_data->pdo_pos,
			charger->ppsVol, charger->targetIIN, S2MU107_DC_DISABLE);
		msleep(500);
		s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
		charger->ppsVol = ((charger->pmeter[PMETER_ID_VCHGIN] + 450) / DC_TA_VOL_STEP_MV) * DC_TA_VOL_STEP_MV;
		s2mu107_dc_send_fenced_pps(charger);

		msleep(500);
		sec_pps_enable(charger->pd_data->pdo_pos,
			charger->ppsVol, charger->targetIIN, S2MU107_DC_ENABLE);

		msleep(500);
		iavg = s2mu107_dc_get_iavg(charger);
		s2mu107_dc_set_charging_current(charger, (iavg - 800));

		if (!charger->is_longCC) {
			s2mu107_update_reg(charger->i2c,
				S2MU107_DC_CD_OTP_01, 0, DIG_CV_MASK);

			pr_info("%s, step CC enable\n", __func__);
			/* Long CC Step 100mA */
			s2mu107_update_reg(charger->i2c,
				S2MU107_DC_CTRL16, 0x0, DC_LONG_CC_STEP_MASK);
			/* LONG_CC_SEL_CELL_PACKB to 0 */
			s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16,
					0x0, 0x20);
			s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16,
					DC_EN_LONG_CC_MASK, DC_EN_LONG_CC_MASK);
			charger->is_longCC = true;

		msleep(500);

		s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);
		v_step_float_uv = charger->pmeter[PMETER_ID_VBATT] * 1000;

		v_diff_gap = (v_float_uv - v_step_float_uv);

		data = (u8)((v_diff_gap) / (DIG_CV_STEP_UV)) + 3;

		pr_info("%s, v_step_float_uv : %d, v_diff_gap : %d, data : 0x%x\n",
			__func__, v_step_float_uv, v_diff_gap, data);

		if (data > DIG_CV_MASK)
			data = DIG_CV_MASK;

		s2mu107_update_reg(charger->i2c,
			S2MU107_DC_CD_OTP_01, data, DIG_CV_MASK);
		}

		msleep(2000);
		s2mu107_dc_forced_enable(charger, S2MU107_DC_DISABLE);

		break;
	default:
		break;
	}
}

static int s2mu107_dc_get_target_chg_current(struct s2mu107_dc_data *charger, int step)
{
	if (charger->ta_pwr_type == TA_PWR_TYPE_45W)
		charger->chgIIN = charger->pdata->dc_step_current_45w[step];
	else if (charger->ta_pwr_type == TA_PWR_TYPE_25W)
		charger->chgIIN = charger->pdata->dc_step_current_25w[step];

	return charger->chgIIN;
}

static void s2mu107_dc_fenced_inc(struct s2mu107_dc_data *charger)
{
	if (charger->targetIIN >= charger->pd_data->taMaxCur)
		charger->targetIIN = charger->pd_data->taMaxCur;
	else charger->targetIIN += DC_TA_CURR_STEP_MA;
}

static void s2mu107_dc_fenced_dec(struct s2mu107_dc_data *charger)
{
	if (charger->targetIIN <= 2000)
		charger->targetIIN = 2000;
	else charger->targetIIN -= DC_TA_CURR_STEP_MA;
}

static void s2mu107_dc_chg_current_checker(struct s2mu107_dc_data *charger)
{
	int inow;
	int step_now = charger->step_now;

	return;

	if (charger->is_shifting || step_now < 0)
		return;

	if (charger->dc_state != DC_STATE_SLEEP_CC)
		return;

	inow = s2mu107_dc_get_inow(charger);
	pr_info("%s, inow (%d)\n", __func__, inow);

	if (charger->ta_pwr_type == TA_PWR_TYPE_45W) {
		pr_info("%s, target (%d)\n", __func__, charger->pdata->dc_step_current_45w[step_now]);
		if (inow < charger->pdata->dc_step_current_45w[step_now] - 100) {
			s2mu107_dc_fenced_inc(charger);
			goto NEED_CHANGE;
		} else if (inow > charger->pdata->dc_step_current_45w[step_now] + 100) {
			s2mu107_dc_fenced_dec(charger);
			goto NEED_CHANGE;
		}
	} else if (charger->ta_pwr_type == TA_PWR_TYPE_25W) {
		pr_info("%s, target (%d)\n", __func__, charger->pdata->dc_step_current_25w[step_now]);
		if (inow < charger->pdata->dc_step_current_25w[step_now] - 100) {
			s2mu107_dc_fenced_inc(charger);
			goto NEED_CHANGE;
		} else if (inow > charger->pdata->dc_step_current_25w[step_now] + 100) {
			s2mu107_dc_fenced_dec(charger);
			goto NEED_CHANGE;
	}
	}

	return;

NEED_CHANGE:
	msleep(500);

	pr_info("%s, need change (%d)\n", __func__, inow);
	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MU107_DC_DISABLE);

	usleep_range(20000, 21000);

	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MU107_DC_ENABLE);
	return;
}

static void s2mu107_dc_step_monitor_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger =
		container_of(work, struct s2mu107_dc_data, step_monitor_work.work);
	int i, vbat, iavg;
	int step_now = charger->step_now;
	//int input_current;
	int charging_current = s2mu107_dc_get_charging_current(charger);

	if (!charger->is_charging) {
		charger->step_now = -1;
		charger->is_longCC = false;
		alarm_cancel(&charger->step_monitor_alarm);
		cancel_delayed_work(&charger->step_monitor_work);

		pr_info("%s, not charging. cancel monitor_work\n", __func__);
		wake_unlock(&charger->step_mon);
		return ;
	}

	/* Set Dual Buck */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL0,
				SC_REG_MODE_DUAL_BUCK,
				SC_REG_MODE_MASK);

	/* check vbat level */
	vbat = s2mu107_dc_get_vbat(charger);

	pr_info("%s, %d th Enter, vbat : %d, step_now : %d\n",
		__func__, charger->step_mon_cnt++, vbat, step_now);

	s2mu107_dc_slow_step(charger);
	s2mu107_dc_chg_current_checker(charger);

	for (i = 0; i < charger->pdata->step_charge_level; i++) {
		if (charger->ta_pwr_type == TA_PWR_TYPE_45W) {
			if (vbat < charger->pdata->dc_step_voltage_45w[i])
				break;
		} else if (charger->ta_pwr_type == TA_PWR_TYPE_25W) {
			if (vbat < charger->pdata->dc_step_voltage_25w[i])
				break;
		}
	}

	if (step_now < 0) {
		pr_info("%s, Step (%d->%d)\n", __func__, step_now, i);
		charger->step_now = i;
		charger->rise_speed_dly_ms = 500;
		s2mu107_dc_get_target_chg_current(charger, i);

		if (charger->dc_state == DC_STATE_SLEEP_CC) {
			charger->ppsVol = sec_get_pps_voltage();
			s2mu107_dc_set_auto_pps_param(charger);
		} else
			s2mu107_dc_cal_pps_current(charger);

		s2mu107_dc_set_charging_current(charger, charger->chgIIN);
	} else if ((step_now != i) && (step_now < i) && (charger->dc_state == DC_STATE_SLEEP_CC)) {
		iavg = s2mu107_dc_get_iavg(charger);
		s2mu107_dc_set_charging_current(charger, (iavg - 1000));
		charger->is_shifting = true;
		charger->chgIIN = s2mu107_dc_get_target_chg_current(charger, i);
		s2mu107_dc_set_lcc_by_step(charger, i);
		pr_info("%s, Step (%d->%d), charging_current(%d)\n",
				__func__, step_now, i, charging_current);
		charger->step_now = i;
	}

	alarm_cancel(&charger->step_monitor_alarm);
	alarm_start_relative(&charger->step_monitor_alarm, ktime_set(charger->alarm_interval, 0));

	pr_info("%s, done\n", __func__);
	wake_unlock(&charger->step_mon);
}

static enum alarmtimer_restart step_monitor_alarm(
	struct alarm *alarm, ktime_t now)
{
	struct s2mu107_dc_data *charger = container_of(alarm,
				struct s2mu107_dc_data, step_monitor_alarm);

	wake_lock(&charger->step_mon);
	schedule_delayed_work(&charger->step_monitor_work, msecs_to_jiffies(500));

	return ALARMTIMER_NORESTART;
}

static int s2mu107_dc_parse_dt(struct device *dev,
		struct s2mu107_dc_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu107-direct-charger");
	const u32 *p;
	int ret = 0, len = 0;

	if (!np) {
		pr_err("%s np NULL(s2mu107-direct-charger)\n", __func__);
		ret = -1;
		goto err;
	}

	ret = of_property_read_string(np, "dc,direct_charger_name",
				(char const **)&pdata->dc_name);
	if (ret < 0)
		pr_info("%s: Direct Charger name is Empty\n", __func__);

	ret = of_property_read_u32(np, "dc,input_current_limit",
			&pdata->input_current_limit);
	if (ret) {
		pr_info("%s: dc,input_current_limit Empty default 4000\n", __func__);
		pdata->input_current_limit = 4000;
	}

	ret = of_property_read_u32(np, "dc,charging_current",
			&pdata->charging_current);
	if (ret) {
		pr_info("%s: dc,charging_current Empty default 1000\n", __func__);
		pdata->charging_current = 1000;
	}

	ret = of_property_read_u32(np, "dc,topoff_current",
			&pdata->topoff_current);
	if (ret) {
		pr_info("%s: dc,topoff_current Empty default 2000\n", __func__);
		pdata->topoff_current = 2000;
	}

	ret = of_property_read_u32(np, "dc,temperature_source",
			&pdata->temperature_source);
	if (ret) {
		pr_info("%s: dc,temperature_source empty, default NTC\n", __func__);
		pdata->temperature_source = 1;
	}

	ret = of_property_read_u32(np, "dc,step_charge_level",
			&pdata->step_charge_level);
	if (ret) {
		pr_info("%s: not use step charge\n", __func__);
		pdata->step_charge_level = 1;
	}

	if (pdata->step_charge_level > 1) {
		p = of_get_property(np, "dc,dc_step_voltage_45w", &len);
		if (!p) {
			pr_err("%s: dc_step_voltage_45w is Empty\n", __func__);
			pdata->step_charge_level = 1;
		} else {
			len = len / sizeof(u32);

			if (len != pdata->step_charge_level) {
				pr_err("%s: len of dc_step_chg_cond_vol is not matched, len(%d/%d)\n",
					__func__, len, pdata->step_charge_level);
			}

			pdata->dc_step_voltage_45w = kzalloc(sizeof(u32) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "dc,dc_step_voltage_45w",
					pdata->dc_step_voltage_45w, len);
			if (ret) {
				pr_info("%s : dc_step_voltage_45w read fail\n", __func__);
			}
		}

		p = of_get_property(np, "dc,dc_step_current_45w", &len);
		if (!p) {
			pr_err("%s: dc_step_current_45w is Empty\n", __func__);
			pdata->step_charge_level = 0;
		} else {
			len = len / sizeof(u32);

			if (len != pdata->step_charge_level) {
				pr_err("%s: len of dc_step_current_45w is not matched, len(%d/%d)\n",
					__func__, len, pdata->step_charge_level);
			}

			pdata->dc_step_current_45w = kzalloc(sizeof(u32) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "dc,dc_step_current_45w",
					pdata->dc_step_current_45w, len);
			if (ret) {
				pr_info("%s : dc_step_current_45w read fail\n", __func__);
			}
		}

		p = of_get_property(np, "dc,dc_step_voltage_25w", &len);
		if (!p) {
			pr_err("%s: dc_step_voltage_25w is Empty\n", __func__);
			pdata->step_charge_level = 1;
		} else {
			len = len / sizeof(u32);

			if (len != pdata->step_charge_level) {
				pr_err("%s: len of dc_step_chg_cond_vol is not matched, len(%d/%d)\n",
					__func__, len, pdata->step_charge_level);
			}

			pdata->dc_step_voltage_25w = kzalloc(sizeof(u32) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "dc,dc_step_voltage_25w",
					pdata->dc_step_voltage_25w, len);
			if (ret) {
				pr_info("%s : dc_step_voltage_25w read fail\n", __func__);
			}
		}

		p = of_get_property(np, "dc,dc_step_current_25w", &len);
		if (!p) {
			pr_err("%s: dc_step_voltage is Empty\n", __func__);
			pdata->step_charge_level = 0;
		} else {
			len = len / sizeof(u32);

			if (len != pdata->step_charge_level) {
				pr_err("%s: len of dc_step_current_25w is not matched, len(%d/%d)\n",
					__func__, len, pdata->step_charge_level);
			}

			pdata->dc_step_current_25w = kzalloc(sizeof(u32) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "dc,dc_step_current_25w",
					pdata->dc_step_current_25w, len);
			if (ret) {
				pr_info("%s : dc_step_current_25w read fail\n", __func__);
			}
		}

		p = of_get_property(np, "dc,dc_c_rate", &len);
		if (!p) {
			pr_err("%s: dc_c_rate is Empty\n", __func__);
			pdata->step_charge_level = 1;
		} else {
			len = len / sizeof(u32);

			if (len != pdata->step_charge_level) {
				pr_err("%s: len of dc_c_rate is not matched, len(%d/%d)\n",
					__func__, len, pdata->step_charge_level);
			}

			pdata->dc_c_rate = kzalloc(sizeof(u32) * len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "dc,dc_c_rate",
					pdata->dc_c_rate, len);
			if (ret) {
				pr_info("%s : dc_c_rate read fail\n", __func__);
			}
		}
	}

	pr_info("%s DT file parsed succesfully\n", __func__);
	return 0;

err:
	pr_info("%s, direct charger parsing failed\n", __func__);
	return -1;
}

/* if need to set s2mu107 pdata */
static const struct of_device_id s2mu107_direct_charger_match_table[] = {
	{ .compatible = "samsung,s2mu107-direct-charger",},
	{},
};

static int s2mu107_direct_charger_probe(struct platform_device *pdev)
{
	struct s2mu107_dev *s2mu107 = dev_get_drvdata(pdev->dev.parent);
	struct s2mu107_platform_data *pdata = dev_get_platdata(s2mu107->dev);
	struct s2mu107_dc_data *charger;
	struct power_supply_config psy_cfg = {};
	int ret = 0;
	s2mu107_dc_pd_data_t *pd_data;

	pr_info("%s: S2MU107 Direct Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	pd_data = kzalloc(sizeof(*pd_data), GFP_KERNEL);
	if (unlikely(!pd_data)) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_pd_data_alloc;
	}
	charger->pd_data = pd_data;

	mutex_init(&charger->dc_mutex);
	mutex_init(&charger->pps_isr_mutex);
	mutex_init(&charger->dc_state_mutex);
	mutex_init(&charger->timer_mutex);
	wake_lock_init(&charger->wake_lock, WAKE_LOCK_SUSPEND, "dc_wake");
	wake_lock_init(&charger->step_mon, WAKE_LOCK_SUSPEND, "dc_step_mon");
	wake_lock_init(&charger->mode_irq, WAKE_LOCK_SUSPEND, "dc_mode_irq");
	wake_lock_init(&charger->fail_irq, WAKE_LOCK_SUSPEND, "dc_fail_irq");
	wake_lock_init(&charger->thermal_irq, WAKE_LOCK_SUSPEND, "dc_thermal_irq");

	charger->dev = &pdev->dev;
	charger->i2c = s2mu107->chg;
	charger->i2c_common = s2mu107->i2c;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mu107_dc_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->dc_name == NULL)
		charger->pdata->dc_name = "s2mu107-direct-charger";

	if (charger->pdata->pm_name == NULL)
		charger->pdata->pm_name = "s2mu107-pmeter";

	if (charger->pdata->fg_name == NULL)
		charger->pdata->fg_name = "s2mu107-fuelgauge";

	if (charger->pdata->sc_name == NULL)
		charger->pdata->sc_name = "s2mu107-switching-charger";

	if (charger->pdata->sec_dc_name == NULL)
		charger->pdata->sec_dc_name = "sec-direct-charger";

	charger->psy_dc_desc.name           = charger->pdata->dc_name;
	charger->psy_dc_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_dc_desc.get_property   = s2mu107_dc_get_property;
	charger->psy_dc_desc.set_property   = s2mu107_dc_set_property;
	charger->psy_dc_desc.properties     = s2mu107_dc_props;
	charger->psy_dc_desc.num_properties = ARRAY_SIZE(s2mu107_dc_props);

	charger->psy_pmeter = get_power_supply_by_name("s2mu107-pmeter");
	if (!charger->psy_pmeter) {
		pr_err("%s: Fail to get pmeter\n", __func__);
		goto err_get_pmeter;
	}

	s2mu107_dc_init(charger);

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = s2mu107_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(s2mu107_supplied_to);

	charger->psy_dc = power_supply_register(&pdev->dev, &charger->psy_dc_desc, &psy_cfg);
	if (IS_ERR(charger->psy_dc)) {
		pr_err("%s: Failed to Register psy_dc\n", __func__);
		ret = PTR_ERR(charger->psy_dc);
		goto err_power_supply_register;
	}

	INIT_DELAYED_WORK(&charger->timer_wqueue,
		s2mu107_dc_timer_work);
	INIT_DELAYED_WORK(&charger->start_cc_wqueue,
		s2mu107_dc_start_cc_work);
	INIT_DELAYED_WORK(&charger->check_vbat_wqueue,
		s2mu107_dc_check_vbat_work);
	INIT_DELAYED_WORK(&charger->state_manager_wqueue,
		s2mu107_dc_state_manager_work);
	INIT_DELAYED_WORK(&charger->set_prop_wqueue,
		s2mu107_dc_set_prop_work);

	/* Init work & alarm for monitoring */
	//wake_lock_init(&charger->step_monitor_wake_lock, WAKE_LOCK_SUSPEND, "s2mu107-dc-monitor");
	INIT_DELAYED_WORK(&charger->step_monitor_work, s2mu107_dc_step_monitor_work);
	alarm_init(&charger->step_monitor_alarm, ALARM_BOOTTIME, step_monitor_alarm);
	charger->alarm_interval = DEFAULT_ALARM_INTERVAL;

	init_waitqueue_head(&charger->wait);
#if defined(BRINGUP)
	charger->timer_wqueue = create_singlethread_workqueue("dc-wq");
	if (!charger->timer_wqueue) {
		pr_info("%s: failed to create wq.\n", __func__);
		ret = -ESRCH;
		goto err_create_wq;
	}
#endif
	charger->irq_base = pdata->irq_base;
	s2mu107_dc_irq_init(charger);

#if EN_TEST_READ
	s2mu107_dc_test_read(charger->i2c);
#endif
	pr_info("%s: S2MU107 Direct Charger driver loaded OK\n", __func__);

	return 0;

#if defined(BRINGUP)
err_create_wq:
	power_supply_unregister(charger->psy_dc);
#endif
err_power_supply_register:
err_get_pmeter:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->dc_mutex);
	mutex_destroy(&charger->pps_isr_mutex);
	mutex_destroy(&charger->dc_state_mutex);
	mutex_destroy(&charger->timer_mutex);
	wake_lock_destroy(&charger->wake_lock);
	wake_lock_destroy(&charger->step_mon);
	wake_lock_destroy(&charger->mode_irq);
	wake_lock_destroy(&charger->fail_irq);
	wake_lock_destroy(&charger->thermal_irq);
	kfree(charger->pd_data);
err_pd_data_alloc:
	kfree(charger);
	return ret;
}

static int s2mu107_direct_charger_remove(struct platform_device *pdev)
{
	struct s2mu107_dc_data *charger = platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_dc);
	mutex_destroy(&charger->dc_mutex);
	mutex_destroy(&charger->pps_isr_mutex);
	mutex_destroy(&charger->dc_state_mutex);
	mutex_destroy(&charger->timer_mutex);
	wake_lock_destroy(&charger->wake_lock);
	wake_lock_destroy(&charger->step_mon);
	wake_lock_destroy(&charger->mode_irq);
	wake_lock_destroy(&charger->fail_irq);
	wake_lock_destroy(&charger->thermal_irq);
	kfree(charger->pd_data);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mu107_direct_charger_prepare(struct device *dev)
{
	return 0;
}

static int s2mu107_direct_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mu107_direct_charger_resume(struct device *dev)
{
	return 0;
}

static void s2mu107_direct_charger_complete(struct device *dev)
{
	struct s2mu107_dc_data *charger = dev_get_drvdata(dev);

	if (charger->is_charging) {
		alarm_cancel(&charger->step_monitor_alarm);
		wake_lock(&charger->step_mon);
		schedule_delayed_work(&charger->step_monitor_work, msecs_to_jiffies(0));
	}
	return ;
}



#else
#define s2mu107_direct_charger_suspend NULL
#define s2mu107_direct_charger_resume NULL
#endif

static void s2mu107_direct_charger_shutdown(struct device *dev)
{
	pr_info("%s: S2MU107 Direct Charger driver shutdown\n", __func__);
}

static const struct dev_pm_ops s2mu107_direct_charger_pm_ops = {
	.prepare = s2mu107_direct_charger_prepare,
	.suspend = s2mu107_direct_charger_suspend,
	.resume = s2mu107_direct_charger_resume,
	.complete = s2mu107_direct_charger_complete,
};

static struct platform_driver s2mu107_direct_charger_driver = {
	.driver         = {
		.name   = "s2mu107-direct-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mu107_direct_charger_match_table,
		.pm     = &s2mu107_direct_charger_pm_ops,
		.shutdown   =   s2mu107_direct_charger_shutdown,
	},
	.probe          = s2mu107_direct_charger_probe,
	.remove     = s2mu107_direct_charger_remove,
};

static int __init s2mu107_direct_charger_init(void)
{
	int ret = 0;
	pr_info("%s start\n", __func__);
	ret = platform_driver_register(&s2mu107_direct_charger_driver);

	return ret;
}
module_init(s2mu107_direct_charger_init);

static void __exit s2mu107_direct_charger_exit(void)
{
	platform_driver_unregister(&s2mu107_direct_charger_driver);
}
module_exit(s2mu107_direct_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Suji Lee <suji0908.lee@samsung.com>, Sejong Park <sejong123.park@samsung.com>");
MODULE_DESCRIPTION("Charger driver for S2MU107");
