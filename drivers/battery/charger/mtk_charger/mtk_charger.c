/*
 * mtk_charger.c - Samsung Charger to connect to mtk charger class
 *
 * Copyright (C) 2020 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include "mtk_charger.h"

#define ENABLE 1
#define DISABLE 0

#define DEBUG_GET_SET 0

unsigned int f_mode_battery;
EXPORT_SYMBOL(f_mode_battery);

static unsigned int __read_mostly lpcharge;
module_param(lpcharge, uint, 0444);
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
static char __read_mostly *f_mode;
module_param(f_mode, charp, 0444);
#endif

static char *mtk_charger_supplied_to[] = {
	"battery",
};

static char *mtk_otg_supplied_to[] = {
	"otg",
};

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
static char *mtk_charger_bc12_supplied_to[] = {
	"bc12",
};
#endif

static enum power_supply_property mtk_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mtk_charger_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
static enum power_supply_property mtk_charger_bc12_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};
#endif
static int mtk_charger_get_charging_health(struct mtk_charger_data *charger);
static void mtk_charger_set_input_current_limit(
		struct mtk_charger_data *charger, int charging_current);
static int mtk_charger_get_input_current_limit(
					struct mtk_charger_data *charger);
static int mtk_charger_get_fast_charging_current(
					struct mtk_charger_data *charger);
static int mtk_charger_get_regulation_voltage(
					struct mtk_charger_data *charger);
static int mtk_charger_get_topoff_setting(struct mtk_charger_data *charger);
static int mtk_charger_get_vbus_voltage(struct mtk_charger_data *charger);


static void mtk_charger_test_read(struct mtk_charger_data *charger)
{
	pr_info("%s\n", __func__);
	charger_dev_dump_registers(charger->chg_dev);
}

static int mtk_charger_get_vbus_voltage(struct mtk_charger_data *charger)
{
	u32 vbus_voltage;

	charger_dev_get_vbus(charger->chg_dev, &vbus_voltage);
	return (vbus_voltage / 1000);
}

static int mtk_charger_get_vsys_voltage(struct mtk_charger_data *charger)
{
	u32 vsys_voltage;

	charger_dev_get_vsys(charger->chg_dev, &vsys_voltage);
	pr_info("%s: vsys_voltage val : %d\n", __func__, vsys_voltage);
	return (vsys_voltage / 1000);
}

static int mtk_charger_otg_control(
		struct mtk_charger_data *charger, bool enable)
{
	pr_info("%s: called charger otg control : %s\n", __func__,
			enable ? "ON" : "OFF");

	if (charger->otg_on == enable || lpcharge)
		return 0;

	mutex_lock(&charger->charger_mutex);
	charger->otg_on = enable;

	if (!enable) {
		charger_dev_enable_otg(charger->chg_dev, false);
	} else {
		/* Set OCP level if needed */
		charger_dev_enable_otg(charger->chg_dev, true);
	}
	mutex_unlock(&charger->charger_mutex);

	power_supply_changed(charger->psy_otg);

	return enable;
}

#if defined(CONFIG_AFC_CHARGER)
static int mtk_charger_get_rp_currentlvl(void)
{
	union power_supply_propval propval = {0, };

	propval.intval = 0;
	psy_do_property("battery", get,
		POWER_SUPPLY_EXT_PROP_RP_LEVEL,
		propval);

	return propval.intval;
}

#if defined(CONFIG_PDIC_NOTIFIER)
static int mtk_charger_get_init_src_cap(void)
{
	union power_supply_propval propval = {0, };

	propval.intval = 0;
	psy_do_property("battery", get,
		POWER_SUPPLY_EXT_PROP_SRCCAP,
		propval);

	return propval.intval;
}
#endif

#if defined(ENABLE_FLASH_HV_CTRL)
static int mtk_charger_get_flash_state(void)
{
	union power_supply_propval propval = {0, };

	propval.intval = 0;
	psy_do_property("battery", get,
		POWER_SUPPLY_EXT_PROP_FLASH_STATE,
		propval);

	return propval.intval;
}
#endif

static bool mtk_charger_check_afc_conditions(struct mtk_charger_data *charger)
{
	if (mtk_charger_get_charging_health(charger) == POWER_SUPPLY_HEALTH_GOOD)
		if (is_wired_type(charger->cable_type))
			if (!is_usb_type(charger->cable_type)) {
				if (is_hv_wire_type(charger->cable_type))
					pr_info("%s: skip AFC with AFC cable\n",
						__func__);
				else if (is_pd_wire_type(charger->cable_type))
					pr_info("%s: skip AFC with PD cable\n",
						__func__);
#if defined(ENABLE_FLASH_HV_CTRL)
				else if (mtk_charger_get_flash_state())
					pr_info("%s: skip AFC with Flash on\n",
						__func__);
#endif
#if defined(CONFIG_PDIC_NOTIFIER)
				else if (mtk_charger_get_init_src_cap())
					pr_info("%s: skip AFC with SRCCAP set\n",
						__func__);
#endif
				else if (charger->cable_type ==
							SEC_BATTERY_CABLE_TIMEOUT)
					pr_info("%s: skip AFC in TIMEOUT\n",
						__func__);
				else if (charger->slow_charging)
					pr_info("%s: skip AFC in slow charging\n",
						__func__);
				else if (mtk_charger_get_rp_currentlvl() !=
							RP_CURRENT_LEVEL_DEFAULT)
					pr_info("%s: skip AFC in RP:%d\n",
						__func__,
						mtk_charger_get_rp_currentlvl());
				else
					return true;
			}
	return false;
}
#endif

static int mtk_charger_plug_in_out(
	struct mtk_charger_data *charger, bool enable)
{
	int ret = 0;

	if (enable == false) {
#if DEBUG_GET_SET
		pr_info("[DEBUG] %s: PLUG-OUT clear VBUS settings\n",
								__func__);
#endif
#if defined(CONFIG_AFC_CHARGER)
		/* Clear performance related flags */
		set_afc_voltage_for_performance(false);
#endif
		/* disable wdt */
		charger_dev_en_wdt(charger->chg_dev, false);
		/* Set MIVR for vbus 5V */
		ret = charger_dev_set_mivr(charger->chg_dev,
			charger->pdata->vbus_min_charger_voltage); /* uV */
		if (ret < 0)
			pr_err("[DEBUG]%s: failed to get set_mivr\n", __func__);
	} else {
		/* Do AFC charging only if BUCK is ON */
		if (charger->buck_state) {
			/* enable wdt */
			/* mt6360: wtd default value is 40s */
			charger_dev_en_wdt(charger->chg_dev, true);
#if defined(CONFIG_AFC_CHARGER)
			if (mtk_charger_check_afc_conditions(charger))
				set_afc_voltage(0x9);
#endif
		}

	}
	return ret;
}

#if defined(CONFIG_AFC_CHARGER)
static void afc_charger_plug_in_out(
	struct mtk_charger_data *charger, int cable_type)
{
	int ret = 0;

	pr_info("[DEBUG]%s:cable_type : %d\n", __func__, cable_type);

	if (cable_type == SEC_BATTERY_CABLE_NONE) {
#if defined(CONFIG_AFC_CHARGER)
		/* Clear performance related flags */
		set_afc_voltage_for_performance(false);
#endif
		/* disable wdt */
		charger_dev_en_wdt(charger->chg_dev, false);
		/* Set MIVR for vbus 5V */
		ret = charger_dev_set_mivr(charger->chg_dev,
			charger->pdata->vbus_min_charger_voltage); /* uV */
		if (ret < 0)
			pr_err("[DEBUG]%s: failed to get set_mivr\n", __func__);
	} else {
		/* enable wdt */
		/* mt6360: wtd default value is 40s */
		charger_dev_en_wdt(charger->chg_dev, true);
#if defined(CONFIG_AFC_CHARGER)
		if (mtk_charger_check_afc_conditions(charger))
			set_afc_voltage(0x9);
#endif
	}

}

int afc_set_voltage(int volt)
{
	pr_info("[DEBUG]%s: %dV to AFC driver\n", __func__, volt);
	if (volt == SEC_INPUT_VOLTAGE_9V) {
		set_afc_voltage_for_performance(false);
		return set_afc_voltage(0x9);
	} else if (volt == SEC_INPUT_VOLTAGE_5V) {
		set_afc_voltage_for_performance(true);
		return set_afc_voltage(0x5);
	}
	return -EINVAL;
}
EXPORT_SYMBOL(afc_set_voltage);
#endif

static void mtk_charger_set_buck(
	struct mtk_charger_data *charger, int enable)
{
	if (charger->f_mode == OB_MODE) {
		pr_info("%s: OB Mode Skip buck Control\n", __func__);
		return;
	}

	if (enable) {
		pr_info("[DEBUG]%s: set buck on\n", __func__);
		charger_dev_enable_powerpath(charger->chg_dev, true);
	} else {
		pr_info("[DEBUG]%s: set buck off\n", __func__);
		mtk_charger_plug_in_out(charger, false);
		charger_dev_enable_powerpath(charger->chg_dev, false);
	}
}

static void mtk_charger_enable_charger_switch(
		struct mtk_charger_data *charger, int onoff)
{
	if (charger->f_mode == OB_MODE) {
		pr_info("%s: OB Mode skip charger control\n", __func__);
		return;
	}

	if (onoff > 0) {
		pr_info("[DEBUG]%s: turn on charger\n", __func__);
		charger_dev_enable(charger->chg_dev, true);
		/* Try 9V boost at charger enable */
		mtk_charger_plug_in_out(charger, true);
	} else {
		pr_info("[DEBUG] %s: turn off charger\n", __func__);
		mtk_charger_plug_in_out(charger, false);
		charger_dev_enable(charger->chg_dev, false);
	}
}

static void mtk_charger_set_regulation_voltage(
		struct mtk_charger_data *charger, int float_voltage)
{
	if (charger->f_mode == OB_MODE) {
		pr_info("%s: OB Mode Skip float voltage setting\n", __func__);
		return;
	}

	/* mt6360_trans_sel(uV, 3900000, 10000, 0x51); */
	charger_dev_set_constant_voltage(charger->chg_dev,
						(float_voltage * 1000));
	pr_info("[DEBUG]%s: float_voltage %d\n", __func__, float_voltage);
}

static int mtk_charger_get_regulation_voltage(struct mtk_charger_data *charger)
{
	u32 float_voltage;

	charger_dev_get_constant_voltage(charger->chg_dev, &float_voltage);
	float_voltage /= 1000;
	pr_info("%s: float voltage val : %d\n",	__func__, float_voltage);
	return float_voltage;
}

static void mtk_charger_set_input_current_limit(
			struct mtk_charger_data *charger, int input_current)
{
	if (charger->f_mode != NO_MODE) {
		pr_info("%s: Skip in IB/OB Mode\n", __func__);
		return;
	}

	charger_dev_set_input_current(charger->chg_dev,
						(input_current * 1000));
	pr_info("[DEBUG]%s: input current %d\n", __func__, input_current);
}

static void mtk_charger_set_aicl(
		struct mtk_charger_data *charger,
		unsigned int input_current_limit_by_aicl)
{
	union power_supply_propval value;

	if ((input_current_limit_by_aicl < charger->input_current) &&
		(is_wired_type(charger->cable_type))) {
		charger->input_current = input_current_limit_by_aicl;
		value.intval = input_current_limit_by_aicl;
		psy_do_property("battery", set,
			POWER_SUPPLY_EXT_PROP_AICL_CURRENT, value);
	}
}

static int mtk_charger_get_input_current_limit(
					struct mtk_charger_data *charger)
{
	unsigned int input_current = 0;

	charger_dev_get_input_current(charger->chg_dev, &input_current);
	input_current /= 1000;
	pr_info("[DEBUG]%s: input current: %d\n", __func__, input_current);

	return input_current;
}

static void mtk_charger_set_fast_charging_current(
		struct mtk_charger_data *charger, int charging_current)
{
	if (charger->f_mode == OB_MODE) {
		pr_info("%s: Skip in OB Mode\n", __func__);
		return;
	}

	pr_info("[DEBUG]%s: output current %d\n", __func__, charging_current);
	charger_dev_set_charging_current(charger->chg_dev,
					(charging_current * 1000));
}

static int mtk_charger_get_fast_charging_current(
		struct mtk_charger_data *charger)
{
	unsigned int output_current = 0;
	int ret = 0;

	ret = charger_dev_get_charging_current(charger->chg_dev,
							&output_current);
	if (ret < 0)
		pr_err("[DEBUG]%s: failed to get output current\n", __func__);
	output_current /= 1000;
	pr_info("[DEBUG]%s: output current: %d\n", __func__, output_current);

	return output_current;
}

static void mtk_charger_set_topoff_current(
	struct mtk_charger_data *charger, int eoc)
{
	pr_info("[DEBUG]%s: EOC current %d\n", __func__, eoc);
	charger_dev_set_eoc_current(charger->chg_dev, (eoc * 1000));
}

static int mtk_charger_get_topoff_setting(struct mtk_charger_data *charger)
{
	unsigned int topoff_setting = 0;

	charger_dev_get_eoc_current(charger->chg_dev, &topoff_setting);
	pr_info("[DEBUG]%s: topoff setting %d\n", __func__, topoff_setting);
	return topoff_setting;
}

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
static void mtk_charger_set_bat_f_mode(struct mtk_charger_data *charger)
{
	union power_supply_propval value = {0, };

	f_mode_battery = charger->f_mode;
	value.intval = charger->f_mode;
	psy_do_property("battery", set,
		POWER_SUPPLY_EXT_PROP_BATT_F_MODE, value);
}
#endif

static void mtk_charger_ob_mode(struct mtk_charger_data *charger, bool enable)
{
	if (!(charger->f_mode == OB_MODE)) {
		pr_info("%s: skip when not in OB Mode\n", __func__);
		return;
	}

	if (enable) {
		/* DISABLE AICC */
		charger_dev_enable_aicc(charger->chg_dev, false);
		/* CHG OFF */
		pr_info("[DEBUG]%s: turn off charger(chgenb_en %d)\n", __func__, charger->pdata->chgenb_en);
		charger_dev_enable(charger->chg_dev, false);
		/* BUCK ON */
		pr_info("[DEBUG]%s: set buck on\n", __func__);
		charger_dev_enable_powerpath(charger->chg_dev, true);
		/* Set VSYS (float voltage) to 4.0V */
		charger_dev_set_constant_voltage(charger->chg_dev,
								(4200 * 1000));
		/* Select 3.25A input current limit from CHG_ILIM */
		/* CHG_CTRL2: IINLMTSEL set AICR 3.25A, 0x12[3:2] = 00 */
		charger_dev_set_iinlmtsel(charger->chg_dev, false);
		if (charger->pdata->chgenb_en == 0) {
			/* SET GPIO_ILIM = "H" */
			//gpio_direction_output(charger->pdata->gpio_ilim, 1);
			/* SET GPIO_CHGENB = "H" */
			gpio_direction_output(charger->pdata->gpio_chgenb, 1);
		} else {
			if (charger->pdata->chgilm_en == 1) {
				/* SET GPIO_ILIM = "H" */
				gpio_direction_output(charger->pdata->gpio_ilim, 1);
			}
			/* SET GPIO_CHGENB = "L" */
			gpio_direction_output(charger->pdata->gpio_chgenb, 0);
		}
		/* Disable ILIM function */
		/* CHG_CTRL3: 0x13[0] = 0 */
		charger_dev_en_ilim(charger->chg_dev, false);
		/* Set Input current limit to max (3250mA) */
		charger_dev_set_input_current(
					charger->chg_dev,
					(charger->pdata->max_icl * 1000));

		charger_dev_dump_registers(charger->chg_dev);
	}
}

static void mtk_charger_ib_mode(struct mtk_charger_data *charger, bool enable)
{
	if (!(charger->f_mode == IB_MODE)) {
		pr_info("%s: skip when not in IB Mode\n", __func__);
		return;
	}

	if (enable) {
		/* DISABLE AICC */
		charger_dev_enable_aicc(charger->chg_dev, false);
		/* CHG ON */
		pr_info("[DEBUG]%s: turn on charger(chgenb_en %d)\n", __func__, charger->pdata->chgenb_en);
		charger_dev_enable(charger->chg_dev, true);
		/* BUCK ON */
		pr_info("[DEBUG]%s: set buck on\n", __func__);
		charger_dev_enable_powerpath(charger->chg_dev, true);
		/* Select 3.25A input current limit from CHG_ILIM */
		/* CHG_CTRL2: IINLMTSEL set AICR 3.25A, 0x12[3:2] = 00 */
		charger_dev_set_iinlmtsel(charger->chg_dev, false);
		if (charger->pdata->chgenb_en == 0) {
			/* SET GPIO_ILIM = "H" */
			//gpio_direction_output(charger->pdata->gpio_ilim, 0);
			/* SET GPIO_CHGENB = "L" */
			gpio_direction_output(charger->pdata->gpio_chgenb, 0);
		} else {
			if (charger->pdata->chgilm_en == 1) {
				/* SET GPIO_ILIM = "L" */
				gpio_direction_output(charger->pdata->gpio_ilim, 0);
			}
			/* SET GPIO_CHGENB = "H" */
			gpio_direction_output(charger->pdata->gpio_chgenb, 1);
		}
		/* Disable ILIM function */
		/* CHG_CTRL3: 0x13[0] = 0 */
		charger_dev_en_ilim(charger->chg_dev, false);
		/* Set Input current limit to max (3250mA) */
		charger_dev_set_input_current(
					charger->chg_dev,
					(charger->pdata->max_icl * 1000));
		charger_dev_dump_registers(charger->chg_dev);
	} else {
		/* Clear IB settings */
		pr_info("%s: Clear IB Mode settings (chgenb_en %d)\n", __func__, charger->pdata->chgenb_en);
		/* Select input current limit from IAICR */
		charger_dev_set_iinlmtsel(charger->chg_dev, true);
		if (charger->pdata->chgenb_en == 0) {
			/* SET GPIO_ILIM = "L" */
			//gpio_direction_output(charger->pdata->gpio_ilim, 0);
			/* SET GPIO_CHGENB = "L" */
			gpio_direction_output(charger->pdata->gpio_chgenb, 0);
		} else {
			if (charger->pdata->chgilm_en == 1) {
				/* SET GPIO_ILIM = "L" */
				gpio_direction_output(charger->pdata->gpio_ilim, 0);
			}
			/* SET GPIO_CHGENB = "H" */
			gpio_direction_output(charger->pdata->gpio_chgenb, 1);
		}
		/* ENABLE AICC */
		charger_dev_enable_aicc(charger->chg_dev, true);
		charger->f_mode = NO_MODE;
		charger_dev_dump_registers(charger->chg_dev);
	}
}

static bool mtk_charger_chg_init(struct mtk_charger_data *charger)
{
	int ret;
	union power_supply_propval value;

	pr_info("%s\n", __func__);

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	pr_info("%s: f_mode: %s\n", __func__, f_mode);
	pr_info("%s: lpcharge: %d\n", __func__, lpcharge);

	if (!f_mode)
		charger->f_mode = NO_MODE;
	else if ((strncmp(f_mode, "OB", 2) == 0) || (strncmp(f_mode, "DL", 2) == 0))
		charger->f_mode = OB_MODE;
	else if (strncmp(f_mode, "IB", 2) == 0)
		charger->f_mode = IB_MODE;
	else
		charger->f_mode = NO_MODE;
	f_mode_battery = charger->f_mode;
	pr_info("[BAT] %s: f_mode: %s\n", __func__, BOOT_MODE_STRING[charger->f_mode]);
#endif

	if (!charger->psy_fg)
		charger->psy_fg = power_supply_get_by_name(
					charger->pdata->fuelgauge_name);
	if (!charger->psy_fg) {
		pr_err("%s: Unable to get psy_fg\n", __func__);
		return false;
	}
	value.intval = charger->f_mode;
	ret = power_supply_set_property(charger->psy_fg,
					POWER_SUPPLY_EXT_PROP_BATT_F_MODE,
					&value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	if (charger->f_mode == IB_MODE) { /* Set IB settings */
		pr_info("%s: Set IB Mode\n", __func__);
		mtk_charger_ib_mode(charger, true);
	} else if (charger->f_mode == NO_MODE) {
		if (charger->pdata->chgenb_en == 0) {
			pr_info("%s: B/U HW, CHGENB=L\n", __func__);
			/* SET GPIO_CHGENB = "L" */
			gpio_direction_output(charger->pdata->gpio_chgenb, 0);
		}
		charger_dev_set_iinlmtsel(charger->chg_dev, true);
		usleep_range(5000, 6000);
		charger_dev_en_ilim(charger->chg_dev, false);
	}

	if (charger->f_mode != OB_MODE) {
		/* Enable HW EOC */
		charger_dev_enable_eoc(charger->chg_dev, true);
		/* Set top-off timer to 30 minutes, EOC_TIMER[1:0] = 01 */
		charger_dev_set_eoc_timer(charger->chg_dev,
							MTK_TOPOFF_TIMER_30m);
		/* Disable HW safety timer */
		charger_dev_enable_safety_timer(charger->chg_dev, false);
		/* Set normal MIVR (based on MTK dts level) */
		ret = charger_dev_set_mivr(charger->chg_dev,
			charger->pdata->vbus_normal_mivr_voltage); /* uV */
		if (ret < 0)
			pr_err("[DEBUG]%s: failed to get set_mivr\n",
								__func__);
	}

	return true;
}

static int mtk_charger_get_charging_status(struct mtk_charger_data *charger)
{
	int charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
	int ret = 0;
	bool chg_done = false;

	ret = charger_dev_get_charging_status(
					charger->chg_dev, &charging_status);
	if (ret < 0)
		pr_err("[DEBUG]%s: failed to get charging status\n", __func__);
	else
		pr_info("[DEBUG]%s: charging status: %d\n", __func__,
			charging_status);

	charger_dev_is_charging_done(charger->chg_dev, &chg_done);
	if (chg_done == true) {
		charging_status = POWER_SUPPLY_STATUS_FULL;
		pr_info("%s: Battery Full!\n", __func__);
	}

	return charging_status;
}

static int mtk_charger_get_charge_type(struct mtk_charger_data *charger)
{
	unsigned int charge_type = 0;
	int ret = 0;

	ret = charger_dev_get_charge_type(charger->chg_dev, &charge_type);
	if (ret < 0)
		pr_err("[DEBUG]%s: failed to get charge type\n", __func__);
	else
		pr_info("[DEBUG]%s: charge type: %d\n", __func__, charge_type);

	if ((charge_type == POWER_SUPPLY_CHARGE_TYPE_SLOW) ||
		(charge_type == POWER_SUPPLY_CHARGE_TYPE_TRICKLE)) {
		charger->slow_charging = true;
		pr_info("%s: slow-charging mode\n", __func__);
	}

	if (charger->slow_charging)
		charge_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	return charge_type;
}

static void mtk_charger_check_aicl(struct mtk_charger_data *charger)
{
	union power_supply_propval value = {0, };
	unsigned int charge_type = 0;
	int ret = 0;

	if (is_wired_type(charger->cable_type)) {
		ret = charger_dev_get_charge_type(charger->chg_dev, &charge_type);
		if (ret < 0)
			pr_err("[DEBUG]%s: failed to get charge type\n", __func__);
		else
			pr_info("[DEBUG]%s: charge type: %d\n", __func__, charge_type);

		if ((charge_type == POWER_SUPPLY_CHARGE_TYPE_SLOW) ||
			(charge_type == POWER_SUPPLY_CHARGE_TYPE_TRICKLE)) {
			charger->slow_charging = true;
			pr_info("%s: slow-charging mode\n", __func__);
			psy_do_property("battery", set, POWER_SUPPLY_PROP_CHARGE_TYPE, value);
		}
	}

}

static bool mtk_charger_get_batt_present(struct mtk_charger_data *charger)
{
	return true;
}

static void mtk_charger_wdt_clear(struct mtk_charger_data *charger)
{
	/* watchdog kick */
	/* mt6360: any register read will clear WDT */
	charger_dev_kick_wdt(charger->chg_dev);
}

static int mtk_charger_get_charging_health(struct mtk_charger_data *charger)
{
	int charging_health = POWER_SUPPLY_HEALTH_GOOD;
	int ret = 0;

	if (charger->is_charging)
		mtk_charger_wdt_clear(charger);

	ret = charger_dev_get_health(charger->chg_dev, &charging_health);

	if (ret < 0)
		pr_err("[DEBUG]%s: failed to get charging health\n", __func__);
	else
		pr_info("[DEBUG]%s: charging health: %d\n", __func__,
							charging_health);

	if ((charging_health != POWER_SUPPLY_HEALTH_GOOD) &&
		(charger->cable_type != SEC_BATTERY_CABLE_NONE))
		mtk_charger_test_read(charger);

	return charging_health;
}

static int mtk_charger_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int chg_curr, aicr;
	struct mtk_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
					(enum power_supply_ext_property) psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->is_charging ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = mtk_charger_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = mtk_charger_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = mtk_charger_get_input_current_limit(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (charger->charging_current) {
			aicr = mtk_charger_get_input_current_limit(charger);
			chg_curr =
				mtk_charger_get_fast_charging_current(charger);
			val->intval = MINVAL(aicr, chg_curr);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_CURRENT_FULL:
		val->intval = mtk_charger_get_topoff_setting(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = mtk_charger_get_charge_type(charger);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = mtk_charger_get_regulation_voltage(charger);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = mtk_charger_get_batt_present(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		mutex_lock(&charger->charger_mutex);
		val->intval = charger->otg_on;
		mutex_unlock(&charger->charger_mutex);
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = charger->charge_mode;
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			mtk_charger_test_read(charger);
			if (charger->pdata->boosting_voltage_aicl)
				mtk_charger_check_aicl(charger); /* cannot detect aicl irq, so need polling*/
			break;
		case POWER_SUPPLY_EXT_PROP_BATT_VSYS:
			val->intval = mtk_charger_get_vsys_voltage(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_BUCK_STATE:
			val->intval = charger->buck_state;
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

static int mtk_charger_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct mtk_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
					(enum power_supply_ext_property)psp;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		charger->slow_charging = false;
		charger->ivr_on = false;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		{
			int input_current = val->intval;

			mtk_charger_set_input_current_limit(charger,
								input_current);
			charger->input_current = input_current;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->charging_current = val->intval;
		mtk_charger_set_fast_charging_current(charger,
						charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		pr_info("[DEBUG] %s: is_charging %d\n", __func__,
						charger->is_charging);
		charger->charging_current = val->intval;
		/* set charging current */
		mtk_charger_set_fast_charging_current(charger,
						charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_CURRENT_FULL:
		charger->topoff_current = val->intval;
		mtk_charger_set_topoff_current(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:		
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("[DEBUG]%s: float voltage changed [%dmV] -> [%dmV]\n", __func__,
						charger->pdata->chg_float_voltage, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		mtk_charger_set_regulation_voltage(charger,
				charger->pdata->chg_float_voltage);
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		mtk_charger_otg_control(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		{
			/* TO-DO: check for IVR status */
		}
		break;
	case POWER_SUPPLY_PROP_AFC_CHARGER_MODE:
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		if (val->intval) {
			pr_info("%s: Set OB Mode (byp keystring)\n", __func__);
			/* Set MIVR for vbus 4.2V */
			charger_dev_set_mivr(charger->chg_dev, 4200000); /* uV */
			charger->f_mode = OB_MODE;
			mtk_charger_ob_mode(charger, true);
		} else {
			pr_info("%s: SET IB mode (byp keystring)\n",
				__func__);
			charger->f_mode = IB_MODE;
			mtk_charger_ib_mode(charger, true);
			/* Set MIVR for vbus 5V */
			charger_dev_set_mivr(charger->chg_dev,
				charger->pdata->vbus_min_charger_voltage); /* uV */
		}
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
		mtk_charger_set_bat_f_mode(charger);
#endif
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		/* by AT CMD */
		if (val->intval) {
			pr_info("%s: Set OB Mode (CMD)\n", __func__);
			/* Set MIVR for vbus 4.2V */
			charger_dev_set_mivr(charger->chg_dev, 4200000); /* uV */
			charger->f_mode = OB_MODE;
			mtk_charger_ob_mode(charger, true);
		} else {
			pr_info("%s: Set IB Mode (CMD)\n", __func__);
			charger->f_mode = IB_MODE;
			mtk_charger_ib_mode(charger, true);
			/* Set MIVR for vbus 5V */
			charger_dev_set_mivr(charger->chg_dev,
				charger->pdata->vbus_min_charger_voltage); /* uV */
		}
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
		mtk_charger_set_bat_f_mode(charger);
#endif
		break;
	case POWER_SUPPLY_PROP_FUELGAUGE_RESET:
		pr_info("%s: reset fuelgauge when surge occur!\n", __func__);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			{
				int buck_status = false;

				charger->charge_mode = val->intval;
				switch (charger->charge_mode) {
				case SEC_BAT_CHG_MODE_BUCK_OFF:
					charger->is_charging = false;
					buck_status = false;
					break;
				case SEC_BAT_CHG_MODE_CHARGING_OFF:
					charger->is_charging = false;
					buck_status = true;
					break;
				case SEC_BAT_CHG_MODE_CHARGING:
					charger->is_charging = true;
					buck_status = true;
					break;
				}

				if (buck_status != charger->buck_state) {
					pr_info("[DEBUG]%s: buck state : old(%d), new(%d)\n",
									__func__, charger->buck_state, buck_status);
					charger->buck_state = buck_status;
					mtk_charger_set_buck(charger, charger->buck_state);
				}
				if (charger->buck_state)
					mtk_charger_enable_charger_switch(charger,
								charger->is_charging);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_BATT_F_MODE:
		case POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE:
			/* by keystring */
			if (val->intval) {
				pr_info("%s: Set OB Mode (keystring)\n",
					__func__);
				/* Set MIVR for vbus 4.2V */
				charger_dev_set_mivr(charger->chg_dev, 4200000); /* uV */
				charger->f_mode = OB_MODE;
				mtk_charger_ob_mode(charger, true);
			} else {
				pr_info("%s: Set IB Mode (keystring)\n",
					__func__);
				charger->f_mode = IB_MODE;
				mtk_charger_ib_mode(charger, true);
				/* Set MIVR for vbus 5V */
				charger_dev_set_mivr(charger->chg_dev,
					charger->pdata->vbus_min_charger_voltage); /* uV */
			}
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
			mtk_charger_set_bat_f_mode(charger);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_IB_MODE:
			pr_info("%s: Set IB Mode (sysfs): %d\n", __func__,
								val->intval);
			charger->f_mode = IB_MODE;
			mtk_charger_ib_mode(charger, val->intval);
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
			mtk_charger_set_bat_f_mode(charger);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_OB_MODE_CABLE_REMOVED:
			{
				int vbus =
					mtk_charger_get_vbus_voltage(charger);
				pr_info("%s: VBUS: %d\n", __func__, vbus);
				if (vbus < 4000) {
					pr_info("%s: No Cable, power-off\n",
						__func__);
					/* BATFET tuns off with 18s delay */
					charger_dev_enable_ship_mode(
							charger->chg_dev, true);
					mt_power_off();
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
			pr_info("%s: manual ship mode set as %s\n", __func__,
						val->intval ?
						"enable" : "disable");
			/* BATFET turns off immediately */
			charger_dev_enable_ship_mode(charger->chg_dev, false);
			break;
		case POWER_SUPPLY_EXT_PROP_AICL_CURRENT:
			pr_info("%s: AICL current: %d\n", __func__, val->intval);
			mtk_charger_set_aicl(charger, val->intval);
			break;
#if defined(CONFIG_AFC_CHARGER)
		case POWER_SUPPLY_EXT_PROP_AFC_INIT:
			pr_info("%s: afc_charger_plug_in_out(): %d\n", __func__, val->intval);
			afc_charger_plug_in_out(charger, val->intval);
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

static int mtk_charger_otg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct mtk_charger_data *charger = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mutex_lock(&charger->charger_mutex);
		val->intval = charger->otg_on;
		mutex_unlock(&charger->charger_mutex);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int mtk_charger_otg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct mtk_charger_data *charger = power_supply_get_drvdata(psy);
	union power_supply_propval value;
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("%s: OTG %s\n", __func__,
				val->intval > 0 ? "ON" : "OFF");
		mtk_charger_otg_control(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("POWER_SUPPLY_PROP_VOLTAGE_MAX, set otg current limit %dmA\n", (val->intval) ? 1500 : 900);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
static int mtk_charger_bc12_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct mtk_charger_data *charger = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mutex_lock(&charger->charger_mutex);
		val->intval = charger->cable_type;
		mutex_unlock(&charger->charger_mutex);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int mtk_charger_bc12_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct mtk_charger_data *charger = power_supply_get_drvdata(psy);
	union power_supply_propval value;
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		charger->slow_charging = false;
		power_supply_changed(charger->psy_bc12);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
#endif

static int mtk_charger_parse_dt(struct device *dev,
		struct mtk_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "mtk-charger");
	int ret = 0;

	if (!np) {
		pr_err("%s np NULL(mtk-charger)\n", __func__);
	} else {
		ret = of_property_read_u32(np, "charger,slow_charging_current",
					&pdata->slow_charging_current);
		if (ret) {
			pr_info("%s : slow_charging_current is Empty\n",
								__func__);
			pdata->slow_charging_current =
					SLOW_CHARGING_CURRENT_STANDARD;
		} else {
			pr_info("%s : slow_charging_current is %d\n",
				__func__, pdata->slow_charging_current);
		}

		ret = of_property_read_u32(np,
				"charger,vbus_min_charger_voltage",
				&pdata->vbus_min_charger_voltage);
		if (ret) {
			pr_info("%s: vbus_min_charger_voltage is Empty\n",
				__func__);
			pdata->vbus_min_charger_voltage = 4600000;
		}
		pr_info("%s: charger,vbus_min_charger_voltage is %d\n",
				__func__, pdata->vbus_min_charger_voltage);

		ret = of_property_read_u32(np,
				"charger,vbus_normal_mivr_voltage",
				&pdata->vbus_normal_mivr_voltage);
		if (ret) {
			pr_info("%s: vbus_normal_mivr_voltage is Empty\n",
				__func__);
			pdata->vbus_normal_mivr_voltage = 4400000;
		}
		pr_info("%s: charger,vbus_normal_mivr_voltage is %d\n",
				__func__, pdata->vbus_normal_mivr_voltage);

		pdata->gpio_ilim = of_get_named_gpio(np,
						"charger,gpio_ilim", 0);
		pdata->gpio_chgenb = of_get_named_gpio(np,
						"charger,gpio_chgenb", 0);

		ret = of_property_read_u32(np, "charger,max_icl",
							&pdata->max_icl);
		if (ret) {
			pr_info("%s: max_icl is Empty\n", __func__);
			pdata->max_icl = 3250;
		}

		ret = of_property_read_u32(np, "charger,ib_fcc",
							&pdata->ib_fcc);
		if (ret) {
			pr_info("%s: ib_fcc is Empty\n", __func__);
			pdata->ib_fcc = 500;
		}
		pr_info("%s: max_icl, ib_fcc are: %d, %d\n",
				__func__, pdata->max_icl, pdata->ib_fcc);

		ret = of_property_read_u32(np, "charger,chgenb_en",
							&pdata->chgenb_en);
		if (ret) {
			pr_info("%s: chgenb_en is empty\n", __func__);
			pdata->chgenb_en = 1;
		}
		pr_info("%s: chgenb_en %d\n", __func__, pdata->chgenb_en);

		ret = of_property_read_u32(np, "charger,chgilm_en",
							&pdata->chgilm_en);
		if (ret) {
			pr_info("%s: chgilm_en is empty\n", __func__);
			pdata->chgilm_en = 1;
		}
		pr_info("%s: chgilm_en %d\n", __func__, pdata->chgilm_en);
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_string(np,
				"battery,fuelgauge_name",
				(char const **)&pdata->fuelgauge_name);
		if (ret < 0)
			pr_info("%s: Fuel-gauge name is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,chg_float_voltage",
				&pdata->chg_float_voltage);
		if (ret) {
			pr_info("%s: battery,chg_float_voltage is Empty\n",
				__func__);
			pdata->chg_float_voltage = 4200;
		}
		pr_info("%s: battery,chg_float_voltage is %d\n",
				__func__, pdata->chg_float_voltage);

		pdata->boosting_voltage_aicl = of_property_read_bool(np,
				"battery,boosting_voltage_aicl");
	}

	pr_info("%s DT file parsed successfully, %d\n", __func__, ret);
	return ret;
}

ssize_t mtk_charger_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t mtk_charger_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);
#define MTK_CHARGER_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = mtk_charger_show_attrs,			\
	.store = mtk_charger_store_attrs,			\
}
enum {
	CHIP_ID = 0,
};

static struct device_attribute mtk_charger_attrs[] = {
	MTK_CHARGER_ATTR(chip_id),
};

static int mtk_charger_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(mtk_charger_attrs); i++) {
		rc = device_create_file(dev, &mtk_charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &mtk_charger_attrs[i]);
	return rc;
}

ssize_t mtk_charger_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct mtk_charger_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - mtk_charger_attrs;
	int i = 0;

	switch (offset) {
	case CHIP_ID:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
							charger->dev_id);
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t mtk_charger_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - mtk_charger_attrs;
	int ret = 0;

	switch (offset) {
	case CHIP_ID:
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

/* if need to set mtk_charger pdata */
static const struct of_device_id mtk_charger_match_table[] = {
	{ .compatible = "samsung,mtk-charger",},
	{},
};

static int mtk_charger_probe(struct platform_device *pdev)
{
	struct mtk_charger_data *charger = NULL;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	pr_info("%s:[BATT] MTK Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	/*Need to replace with meaningful value from PMIC */
	charger->dev_id = 1;

	mutex_init(&charger->charger_mutex);
	charger->otg_on = false;
	charger->ivr_on = false;
	charger->slow_charging = false;
	charger->buck_state = -1;

	charger->dev = &pdev->dev;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = mtk_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;

	if ((charger->pdata->chgenb_en == 1) && (charger->pdata->chgilm_en == 1)) {
		ret = gpio_request(charger->pdata->gpio_ilim, "charger,gpio_ilim");
		if (ret < 0) {
			pr_err("failed to request ilim gpio\n", __func__);
			goto err_parse_gpio;
		}
	}

	ret = gpio_request(charger->pdata->gpio_chgenb, "charger,gpio_chgenb");
	if (ret < 0) {
		pr_err("failed to request chgenb gpio\n", __func__);
		goto err_parse_gpio;
	}

	platform_set_drvdata(pdev, charger);

	charger->chg_dev = get_charger_by_name("primary_chg");
	if (charger->chg_dev)
		chr_err("Found primary charger [%s]\n",
			charger->chg_dev->props.alias_name);
	else
		chr_err("*** Error : can't find primary charger ***\n");

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "mtk-charger";
	if (charger->pdata->fuelgauge_name == NULL)
		charger->pdata->fuelgauge_name = "mtk-fg-battery";

	charger->psy_chg_desc.name	= charger->pdata->charger_name;
	charger->psy_chg_desc.type	= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg_desc.get_property = mtk_charger_chg_get_property;
	charger->psy_chg_desc.set_property = mtk_charger_chg_set_property;
	charger->psy_chg_desc.properties = mtk_charger_props;
	charger->psy_chg_desc.num_properties = ARRAY_SIZE(mtk_charger_props);

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	charger->psy_bc12_desc.name = "bc12";
	charger->psy_bc12_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_bc12_desc.get_property = mtk_charger_bc12_get_property;
	charger->psy_bc12_desc.set_property = mtk_charger_bc12_set_property;
	charger->psy_bc12_desc.properties = mtk_charger_bc12_props;
	charger->psy_bc12_desc.num_properties =
					ARRAY_SIZE(mtk_charger_bc12_props);
#endif
	mtk_charger_chg_init(charger);
	charger->input_current = mtk_charger_get_input_current_limit(charger);
	charger->charging_current =
				mtk_charger_get_fast_charging_current(charger);
	charger->cable_type = SEC_BATTERY_CABLE_NONE;

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = mtk_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(mtk_charger_supplied_to);

	charger->psy_chg = power_supply_register(&pdev->dev,
					&charger->psy_chg_desc, &psy_cfg);
	if (IS_ERR(charger->psy_chg)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(charger->psy_chg);
		goto err_power_supply_register;
	}

	charger->psy_otg_desc.name = "mtk-otg";
	charger->psy_otg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_otg_desc.get_property = mtk_charger_otg_get_property;
	charger->psy_otg_desc.set_property = mtk_charger_otg_set_property;
	charger->psy_otg_desc.properties = mtk_charger_otg_props;
	charger->psy_otg_desc.num_properties =
					ARRAY_SIZE(mtk_charger_otg_props);

	psy_cfg.supplied_to = mtk_otg_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(mtk_otg_supplied_to);

	charger->psy_otg = power_supply_register(&pdev->dev,
					&charger->psy_otg_desc, &psy_cfg);
	if (IS_ERR(charger->psy_otg)) {
		pr_err("%s: Failed to Register psy_otg\n", __func__);
		ret = PTR_ERR(charger->psy_otg);
		goto err_power_supply_register_otg;
	}

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	psy_cfg.supplied_to = mtk_charger_bc12_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(mtk_charger_bc12_supplied_to);

	charger->psy_bc12 = power_supply_register(&pdev->dev,
					&charger->psy_bc12_desc, &psy_cfg);
	if (IS_ERR(charger->psy_bc12)) {
		pr_err("%s: Failed to Register psy_bc12\n", __func__);
		ret = PTR_ERR(charger->psy_bc12);
		goto err_power_supply_register_bc12;
	}
#endif

#if EN_TEST_READ
	mtk_charger_test_read(charger);
#endif
	ret = mtk_charger_create_attrs(&charger->psy_chg->dev);
	if (ret) {
		pr_err("%s : Failed to create_attrs\n", __func__);
		goto err_attrs_create;
	}

	sec_chg_set_dev_init(SC_DEV_MAIN_CHG);

	pr_info("%s:[BATT] MTK Charger driver loaded OK\n", __func__);

	return 0;

err_attrs_create:
	power_supply_unregister(charger->psy_otg);
err_power_supply_register_otg:
	power_supply_unregister(charger->psy_chg);
#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
err_power_supply_register_bc12:
	power_supply_unregister(charger->psy_bc12);
#endif
err_power_supply_register:
err_parse_gpio:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->charger_mutex);
	kfree(charger);
	return ret;
}

static int mtk_charger_remove(struct platform_device *pdev)
{
	struct mtk_charger_data *charger = platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_chg);
#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	power_supply_unregister(charger->psy_bc12);
#endif
	mutex_destroy(&charger->charger_mutex);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int mtk_charger_suspend(struct device *dev)
{
	return 0;
}

static int mtk_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define mtk_charger_suspend NULL
#define mtk_charger_resume NULL
#endif

static void mtk_charger_shutdown(struct platform_device *pdev)
{
	struct mtk_charger_data *charger = platform_get_drvdata(pdev);

	pr_info("%s: MTK charger driver shutdown\n", __func__);
	/* Switch buck on when shut-down */
	mtk_charger_set_buck(charger, 1);
}

static SIMPLE_DEV_PM_OPS(mtk_charger_pm_ops, mtk_charger_suspend,
		mtk_charger_resume);

static struct platform_driver mtk_charger_driver = {
	.driver = {
		.name = "mtk-charger",
		.owner = THIS_MODULE,
		.of_match_table = mtk_charger_match_table,
		.pm = &mtk_charger_pm_ops,
	},
	.probe = mtk_charger_probe,
	.remove = mtk_charger_remove,
	.shutdown = mtk_charger_shutdown,
};

static int __init mtk_charger_init(void)
{
	int ret = 0;

	pr_info("%s start\n", __func__);
	ret = platform_driver_register(&mtk_charger_driver);

	return ret;
}
module_init(mtk_charger_init);

static void __exit mtk_charger_exit(void)
{
	pr_info("%s exit\n", __func__);
	platform_driver_unregister(&mtk_charger_driver);
}
module_exit(mtk_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for MTK charger class");
