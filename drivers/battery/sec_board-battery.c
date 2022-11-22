/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/switch.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/machine.h>
#include <linux/platform_device.h>
//#include <plat/adc.h>
#include <linux/qpnp/pin.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/regulator/krait-regulator.h>

#ifdef CONFIG_BATTERY_SAMSUNG
#include <linux/battery/sec_battery.h>
#endif

#ifdef CONFIG_FUELGAUGE_RT5033
#include <linux/battery/fuelgauge/rt5033_fuelgauge.h>
#elif defined(CONFIG_FUELGAUGE_SM5703)
#include <linux/battery/fuelgauge/sm5703_fuelgauge.h>
#endif
#if defined(CONFIG_FUELGAUGE_MAX17050) || defined(CONFIG_FUELGAUGE_STC3117)
#include <linux/battery/sec_fuelgauge.h>
#endif
#if defined(CONFIG_SM5502_MUIC)
#include <linux/i2c/sm5502.h>
#endif
#if defined(CONFIG_SM5504_MUIC)
#include <linux/i2c/sm5504.h>
#endif
#if defined(CONFIG_SM5703_MUIC)
#include <linux/i2c/sm5703-muic.h>
#endif
#include <linux/gpio_event.h>

#define SHORT_BATTERY_STANDARD      100

/* cable state */
#if defined(CONFIG_EXTCON)
int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
#else
extern int current_cable_type;
#endif
extern int system_rev;
extern bool is_cable_attached;
static struct qpnp_vadc_chip *adc_client;

#include CONFIG_BATTERY_SAMSUNG_DATA_FILE

static void sec_bat_adc_ap_init(struct platform_device *pdev,
		struct sec_battery_info *battery)
{
	adc_client = qpnp_get_vadc(battery->dev, "sec-battery");

	if (IS_ERR(adc_client)) {
		int rc;
		rc = PTR_ERR(adc_client);
		if (rc != -EPROBE_DEFER)
			pr_err("%s: Fail to get vadc %d\n", __func__, rc);
	}
}

static int sec_bat_adc_ap_read(struct sec_battery_info *battery, int channel)
{
	struct qpnp_vadc_result results;
	int rc = -1;
	int data = -1;

	switch (channel)
	{
	case SEC_BAT_ADC_CHANNEL_TEMP:
		rc = qpnp_vadc_read(adc_client, LR_MUX1_BATT_THERM, &results);
		if (rc) {
			pr_err("%s: Unable to read batt temperature rc=%d\n",
					__func__, rc);
			return 0;
		}
		data = results.adc_code;
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = 33000;
		break;
	case SEC_BAT_ADC_CHANNEL_BAT_CHECK:
		rc = qpnp_vadc_read(adc_client, LR_MUX2_BAT_ID, &results);
		if (rc) {
			pr_err("%s: Unable to read BATT_ID ADC rc=%d\n",
					__func__, rc);
			return 0;
		}
		pr_debug("BAT_ID physical= %lld, raw = 0x%x\n", results.physical, results.adc_code);
		data = results.physical;
		break;
	case SEC_BAT_ADC_CHANNEL_INBAT_VOLTAGE:
		rc = qpnp_vadc_read(adc_client, VBAT_SNS, &results);
		if (rc) {
			pr_err("%s: Unable to read VBAT_SNS ADC rc=%d\n",
					__func__, rc);
			return 0;
		}
		data = ((int)results.physical)/1000;
		break;
	case SEC_BAT_ADC_CHANNEL_DISCHARGING_CHECK:
		/** Battery Thermister ADC is used for self_discharge IC ADC in GT5*/
                rc = qpnp_vadc_read(adc_client, LR_MUX1_BATT_THERM, &results);
                if (rc) {
                        pr_err("%s: Unable to read discharging_check ADC rc=%d\n",
                                        __func__, rc);
                        return 0;
                }
                data = results.adc_code;
                break;
	case SEC_BAT_ADC_CHANNEL_DISCHARGING_NTC:
		return 0;
		break;
	default :
		break;
	}

	return data;
}

static void sec_bat_adc_ap_exit(void)
{
	return;
}

static void sec_bat_adc_none_init(struct platform_device *pdev,
		struct sec_battery_info *battery)
{
	return;
}

static int sec_bat_adc_none_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}

static void sec_bat_adc_none_exit(void)
{
	return;
}

static void sec_bat_adc_ic_init(struct platform_device *pdev,
		struct sec_battery_info *battery)
{
	return;
}

static int sec_bat_adc_ic_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}

static void sec_bat_adc_ic_exit(void)
{
	return;
}
static int adc_read_type(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		adc = sec_bat_adc_none_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		adc = sec_bat_adc_ap_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		adc = sec_bat_adc_ic_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
	pr_debug("[%s] ADC = %d\n", __func__, adc);
	return adc;
}

static void adc_init_type(struct platform_device *pdev,
		struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

static void adc_exit_type(struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

int adc_read(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	adc = adc_read_type(battery, channel);

	pr_debug("[%s]adc = %d\n", __func__, adc);

	return adc;
}
EXPORT_SYMBOL(adc_read);

void adc_exit(struct sec_battery_info *battery)
{
	adc_exit_type(battery);
}
EXPORT_SYMBOL(adc_exit);

bool sec_bat_check_jig_status(void)
{
#if defined(CONFIG_SM5502_MUIC)
	return check_sm5502_jig_state();
#elif defined(CONFIG_SM5504_MUIC)
	return check_sm5504_jig_state();
#elif defined(CONFIG_SM5703_MUIC)
	return check_sm5703_muic_jig_state();
#elif defined(CONFIG_EXTCON_MAX77849)
	return get_jig_state();
#else
	return false;
#endif
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
bool sec_bat_check_callback(struct sec_battery_info *battery)
{
	return true;
}

void sec_bat_check_cable_result_callback(struct device *dev,
		int cable_type)
{
}

int sec_bat_check_cable_callback(struct sec_battery_info *battery)
{
	int ta_nconnected = 0;
	union power_supply_propval value;
	struct power_supply *psy_charger =
		power_supply_get_by_name(battery->pdata->charger_name);
	int cable_type = POWER_SUPPLY_TYPE_BATTERY;

	msleep(300);
	if (psy_charger) {
		psy_charger->get_property(psy_charger,
				POWER_SUPPLY_PROP_CHARGE_NOW, &value);
		ta_nconnected = value.intval;

		pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);
	}

	if (current_cable_type == POWER_SUPPLY_TYPE_MISC) {
		cable_type = !ta_nconnected ?
			POWER_SUPPLY_TYPE_BATTERY : POWER_SUPPLY_TYPE_MISC;
		pr_info("%s: current cable : MISC, type : %s\n", __func__,
				!ta_nconnected ? "BATTERY" : "MISC");
	} else if (current_cable_type == POWER_SUPPLY_TYPE_UARTOFF) {
		cable_type = !ta_nconnected ?
			POWER_SUPPLY_TYPE_BATTERY : POWER_SUPPLY_TYPE_UARTOFF;
		pr_info("%s: current cable : UARTOFF, type : %s\n", __func__,
				!ta_nconnected ? "BATTERY" : "UARTOFF");
	} else {
		cable_type = current_cable_type;
	}

	return cable_type;
}


void board_battery_init(struct platform_device *pdev, struct sec_battery_info *battery)
{
	if ((!battery->pdata->temp_adc_table) &&
			(battery->pdata->thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC)) {
		pr_info("%s : assign temp adc table\n", __func__);

		battery->pdata->temp_adc_table = temp_table;
		battery->pdata->temp_amb_adc_table = temp_table;

		battery->pdata->temp_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t);
		battery->pdata->temp_amb_adc_table_size = sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t);
	}

	battery->pdata->event_check = true;
#if !defined(CONFIG_SEC_J1_PROJECT)
	battery->pdata->temp_highlimit_threshold_event = TEMP_HIGHLIMIT_THRESHOLD_EVENT;
	battery->pdata->temp_highlimit_recovery_event = TEMP_HIGHLIMIT_RECOVERY_EVENT;
	battery->pdata->temp_highlimit_threshold_normal = TEMP_HIGHLIMIT_THRESHOLD_NORMAL;
	battery->pdata->temp_highlimit_recovery_normal = TEMP_HIGHLIMIT_RECOVERY_NORMAL;
	battery->pdata->temp_highlimit_threshold_lpm = TEMP_HIGHLIMIT_THRESHOLD_LPM;
	battery->pdata->temp_highlimit_recovery_lpm = TEMP_HIGHLIMIT_RECOVERY_LPM;
#endif
	battery->pdata->temp_high_threshold_event = TEMP_HIGH_THRESHOLD_EVENT;
	battery->pdata->temp_high_recovery_event = TEMP_HIGH_RECOVERY_EVENT;
	battery->pdata->temp_low_threshold_event = TEMP_LOW_THRESHOLD_EVENT;
	battery->pdata->temp_low_recovery_event = TEMP_LOW_RECOVERY_EVENT;
	battery->pdata->temp_high_threshold_normal = TEMP_HIGH_THRESHOLD_NORMAL;
	battery->pdata->temp_high_recovery_normal = TEMP_HIGH_RECOVERY_NORMAL;
	battery->pdata->temp_low_threshold_normal = TEMP_LOW_THRESHOLD_NORMAL;
	battery->pdata->temp_low_recovery_normal = TEMP_LOW_RECOVERY_NORMAL;
	battery->pdata->temp_high_threshold_lpm = TEMP_HIGH_THRESHOLD_LPM;
	battery->pdata->temp_high_recovery_lpm = TEMP_HIGH_RECOVERY_LPM;
	battery->pdata->temp_low_threshold_lpm = TEMP_LOW_THRESHOLD_LPM;
	battery->pdata->temp_low_recovery_lpm = TEMP_LOW_RECOVERY_LPM;

#if defined(CONFIG_BATTERY_SWELLING)
	battery->swelling_temp_high_threshold = BATT_SWELLING_HIGH_TEMP_BLOCK;
	battery->swelling_temp_high_recovery = BATT_SWELLING_HIGH_TEMP_RECOV;
	battery->swelling_temp_low_threshold = BATT_SWELLING_LOW_TEMP_BLOCK;
	battery->swelling_temp_low_recovery = BATT_SWELLING_LOW_TEMP_RECOV;
	battery->swelling_recharge_voltage = BATT_SWELLING_RECHG_VOLTAGE;
	battery->swelling_block_time = BATT_SWELLING_BLOCK_TIME;
#endif

	adc_init_type(pdev, battery);
}

void board_fuelgauge_init(struct sec_fuelgauge_info *fuelgauge)
{
#if defined(CONFIG_SEC_HESTIA2_PROJECT)
	if (!fuelgauge->pdata->battery_data) {
		pr_info("%s : assign battery data\n", __func__);
			fuelgauge->pdata->battery_data = (void *)samsung_battery_data;
	}
#elif defined(CONFIG_FUELGAUGE_STC3117)
	fuelgauge->pdata->battery_data = stc3117_battery_data;
#endif
	fuelgauge->pdata->capacity_max = CAPACITY_MAX;
	fuelgauge->pdata->capacity_max_margin = CAPACITY_MAX_MARGIN;
	fuelgauge->pdata->capacity_min = CAPACITY_MIN;
}

void cable_initial_check(struct sec_battery_info *battery)
{
	union power_supply_propval value;

	pr_info("%s : current_cable_type : (%d)\n", __func__, current_cable_type);
	if (POWER_SUPPLY_TYPE_BATTERY != current_cable_type) {
		if (current_cable_type == POWER_SUPPLY_TYPE_POWER_SHARING) {
			value.intval = current_cable_type;
			psy_do_property("ps", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		} else {
			value.intval = current_cable_type;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
	} else {
		psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_WIRELESS) {
			value.intval = 1;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
	}
}

EXPORT_SYMBOL(cable_initial_check);

/* defined but not used compile error
static void charger_gpio_init(void)
{
}
*/
void __init samsung_init_battery(void)
{
	pr_info("%s: samsung dummy battery init\n", __func__);

/*
	is_wpc_cable_attached = false;

	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

	i2c_register_board_info(FG_ID, sec_brdinfo_fuelgauge,
			ARRAY_SIZE(sec_brdinfo_fuelgauge));

	temp_adc_client = s3c_adc_register(&sec_device_battery, NULL, NULL, 0);*/
}


