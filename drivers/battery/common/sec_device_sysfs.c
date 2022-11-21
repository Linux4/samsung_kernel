/*
 *  sec_device_sysfs.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#if defined(CONFIG_SEC_COMMON)
#include <linux/sec_common.h>
#endif
#include "sec_battery.h"
#include "sec_battery_sysfs.h"

static ssize_t sysfs_device_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t sysfs_device_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SYSFS_DEVICE_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sysfs_device_show_attrs,					\
	.store = sysfs_device_store_attrs,					\
}

static struct device_attribute sysfs_device_attrs[] = {
	/* CHG  */
	SYSFS_DEVICE_ATTR(check_slave_chg),
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	SYSFS_DEVICE_ATTR(fg_full_voltage),
#endif
	SYSFS_DEVICE_ATTR(batt_ext_dev_chg),
	SYSFS_DEVICE_ATTR(batt_wdt_control),
	SYSFS_DEVICE_ATTR(mode),
	SYSFS_DEVICE_ATTR(batt_chip_id),
	SYSFS_DEVICE_ATTR(batt_shipmode_test),
	SYSFS_DEVICE_ATTR(direct_charging_status),
#if defined(CONFIG_DIRECT_CHARGING)
	SYSFS_DEVICE_ATTR(dchg_adc_mode_ctrl),
	SYSFS_DEVICE_ATTR(direct_charging_step),
	SYSFS_DEVICE_ATTR(direct_charging_iin),
	SYSFS_DEVICE_ATTR(switch_charging_source),
#endif
	SYSFS_DEVICE_ATTR(charging_type),
	SYSFS_DEVICE_ATTR(batt_factory_mode),
	SYSFS_DEVICE_ATTR(factory_mode_relieve),
	SYSFS_DEVICE_ATTR(factory_mode_bypass),
	SYSFS_DEVICE_ATTR(normal_mode_bypass),
	SYSFS_DEVICE_ATTR(factory_voltage_regulation),
	SYSFS_DEVICE_ATTR(factory_mode_disable),

	/* LIMITER */
#if defined(CONFIG_DUAL_BATTERY)
	SYSFS_DEVICE_ATTR(batt_main_voltage),
	SYSFS_DEVICE_ATTR(batt_sub_voltage),
	SYSFS_DEVICE_ATTR(batt_main_current_ma),
	SYSFS_DEVICE_ATTR(batt_sub_current_ma),
	SYSFS_DEVICE_ATTR(batt_main_con_det),
	SYSFS_DEVICE_ATTR(batt_sub_con_det),
	SYSFS_DEVICE_ATTR(batt_main_enb),
	SYSFS_DEVICE_ATTR(batt_sub_enb),
#endif

	/* FG */
	SYSFS_DEVICE_ATTR(batt_reset_soc),
	SYSFS_DEVICE_ATTR(batt_read_raw_soc),
	SYSFS_DEVICE_ATTR(batt_voltage_now),
	SYSFS_DEVICE_ATTR(batt_current_ua_now),
	SYSFS_DEVICE_ATTR(batt_current_ua_avg),
	SYSFS_DEVICE_ATTR(batt_filter_cfg),
	SYSFS_DEVICE_ATTR(fg_capacity),
	SYSFS_DEVICE_ATTR(fg_asoc),
	SYSFS_DEVICE_ATTR(batt_capacity_max),
	SYSFS_DEVICE_ATTR(batt_inbat_voltage),
	SYSFS_DEVICE_ATTR(batt_inbat_voltage_ocv),
	SYSFS_DEVICE_ATTR(batt_inbat_voltage_adc),
	SYSFS_DEVICE_ATTR(vbyp_voltage),
	SYSFS_DEVICE_ATTR(error_cause),
	SYSFS_DEVICE_ATTR(cisd_fullcaprep_max),
	SYSFS_DEVICE_ATTR(batt_jig_gpio),
	SYSFS_DEVICE_ATTR(cc_info),
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	SYSFS_DEVICE_ATTR(fg_cycle),
	SYSFS_DEVICE_ATTR(fg_fullcapnom),
#endif

	/* WC */
	SYSFS_DEVICE_ATTR(wc_status),
	SYSFS_DEVICE_ATTR(wc_enable),
	SYSFS_DEVICE_ATTR(wc_control),
	SYSFS_DEVICE_ATTR(wc_control_cnt),
	SYSFS_DEVICE_ATTR(led_cover),
	SYSFS_DEVICE_ATTR(batt_inbat_wireless_cs100),
	SYSFS_DEVICE_ATTR(mst_switch_test), /* MFC MST switch test */
	SYSFS_DEVICE_ATTR(wc_vout),
	SYSFS_DEVICE_ATTR(wc_vrect),
	SYSFS_DEVICE_ATTR(wc_tx_en),
	SYSFS_DEVICE_ATTR(wc_tx_vout),
	SYSFS_DEVICE_ATTR(batt_hv_wireless_status),
	SYSFS_DEVICE_ATTR(batt_hv_wireless_pad_ctrl),
	SYSFS_DEVICE_ATTR(wc_tx_id),
	SYSFS_DEVICE_ATTR(wc_op_freq),
	SYSFS_DEVICE_ATTR(wc_cmd_info),
	SYSFS_DEVICE_ATTR(wc_rx_connected),
	SYSFS_DEVICE_ATTR(wc_rx_connected_dev),
	SYSFS_DEVICE_ATTR(wc_tx_mfc_vin_from_uno),
	SYSFS_DEVICE_ATTR(wc_tx_mfc_iin_from_uno),
#if defined(CONFIG_WIRELESS_TX_MODE)
	SYSFS_DEVICE_ATTR(wc_tx_avg_curr),
	SYSFS_DEVICE_ATTR(wc_tx_total_pwr),
#endif
	SYSFS_DEVICE_ATTR(wc_tx_stop_capacity),
	SYSFS_DEVICE_ATTR(wc_tx_timer_en),
#if defined(CONFIG_WIRELESS_AUTH)
	SYSFS_DEVICE_ATTR(wc_auth_adt_sent),
#endif
	SYSFS_DEVICE_ATTR(wc_duo_rx_power),
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	SYSFS_DEVICE_ATTR(batt_wireless_firmware_update),
	SYSFS_DEVICE_ATTR(otp_firmware_result),
	SYSFS_DEVICE_ATTR(wc_ic_grade),
	SYSFS_DEVICE_ATTR(otp_firmware_ver_bin),
	SYSFS_DEVICE_ATTR(otp_firmware_ver),
	SYSFS_DEVICE_ATTR(tx_firmware_result),
	SYSFS_DEVICE_ATTR(tx_firmware_ver),
	SYSFS_DEVICE_ATTR(batt_tx_status),
#endif
};

enum {
	/* CHG */
	CHECK_SLAVE_CHG = 0,
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	FG_FULL_VOLTAGE,
#endif
	BATT_EXT_DEV_CHG,
	BATT_WDT_CONTROL,
	MODE,
	BATT_CHIP_ID,
	BATT_SHIPMODE_TEST,
	DIRECT_CHARGING_STATUS,
#if defined(CONFIG_DIRECT_CHARGING)
	DCHG_ADC_MODE_CTRL,
	DIRECT_CHARGING_STEP,
	DIRECT_CHARGING_IIN,
	SWITCH_CHARGING_SOURCE,
#endif
	CHARGING_TYPE,
	BATT_FACTORY_MODE,
	FACTORY_MODE_RELIEVE,
	FACTORY_MODE_BYPASS,
	NORMAL_MODE_BYPASS,
	FACTORY_VOLTAGE_REGULATION,
	FACTORY_MODE_DISABLE,

	/* LIMITER */
#if defined(CONFIG_DUAL_BATTERY)
	BATT_MAIN_VOLTAGE,
	BATT_SUB_VOLTAGE,
	BATT_MAIN_CURRENT_MA,
	BATT_SUB_CURRENT_MA,
	BATT_MAIN_CON_DET,
	BATT_SUB_CON_DET,
	BATT_MAIN_ENB,
	BATT_SUB_ENB,
#endif

	/* FG */
	BATT_RESET_SOC,
	BATT_READ_RAW_SOC,
	BATT_VOLTAGE_NOW,
	BATT_CURRENT_UA_NOW,
	BATT_CURRENT_UA_AVG,
	BATT_FILTER_CFG,
	FG_CAPACITY,
	FG_ASOC,
	BATT_CAPACITY_MAX,
	BATT_INBAT_VOLTAGE,
	BATT_INBAT_VOLTAGE_OCV,
	BATT_INBAT_VOLTAGE_ADC,
	BATT_VBYP_VOLTAGE,
	ERROR_CAUSE,
	CISD_FULLCAPREP_MAX,
	BATT_JIG_GPIO,
	CC_INFO,
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	FG_CYCLE,
	FG_FULLCAPNOM,
#endif

	/* WC */
	WC_STATUS,
	WC_ENABLE,
	WC_CONTROL,
	WC_CONTROL_CNT,
	LED_COVER,
	BATT_INBAT_WIRELESS_CS100,
	BATT_WIRELESS_MST_SWITCH_TEST,
	WC_VOUT,
	WC_VRECT,
	WC_TX_EN,
	WC_TX_VOUT,
	BATT_HV_WIRELESS_STATUS,
	BATT_HV_WIRELESS_PAD_CTRL,
	WC_TX_ID,
	WC_OP_FREQ,
	WC_CMD_INFO,
	WC_RX_CONNECTED,
	WC_RX_CONNECTED_DEV,
	WC_TX_MFC_VIN_FROM_UNO,
	WC_TX_MFC_IIN_FROM_UNO,
#if defined(CONFIG_WIRELESS_TX_MODE)
	WC_TX_AVG_CURR,
	WC_TX_TOTAL_PWR,
#endif
	WC_TX_STOP_CAPACITY,
	WC_TX_TIMER_EN,
#if defined(CONFIG_WIRELESS_AUTH)
	WC_AUTH_ADT_SENT,
#endif
	WC_DUO_RX_POWER,
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	BATT_WIRELESS_FIRMWARE_UPDATE,
	OTP_FIRMWARE_RESULT,
	WC_IC_GRADE,
	OTP_FIRMWARE_VER_BIN,
	OTP_FIRMWARE_VER,
	TX_FIRMWARE_RESULT,
	TX_FIRMWARE_VER,
	BATT_TX_STATUS,
#endif
};

ssize_t sysfs_device_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_device_attrs;
	union power_supply_propval value = {0, };
	int i = 0;
	int ret = 0;

	switch (offset) {
	case CHECK_SLAVE_CHG:
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_CHECK_SLAVE_I2C, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		pr_info("%s : CHECK_SLAVE_CHG=%d\n", __func__, value.intval);
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case FG_FULL_VOLTAGE:
		{
			int recharging_voltage = battery->pdata->recharge_condition_vcell;

			if (battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING)
				recharging_voltage = battery->pdata->swelling_high_rechg_voltage;
			else if (battery->current_event & SEC_BAT_CURRENT_EVENT_LOW_TEMP_MODE)
				recharging_voltage = battery->pdata->swelling_low_rechg_voltage;

			psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d %d\n",
				value.intval, recharging_voltage);
			break;
		}
#endif
	case BATT_EXT_DEV_CHG:
		break;
	case BATT_WDT_CONTROL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->wdt_kick_disable);
		break;
	case MODE:
		value.strval = NULL;
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			(value.strval) ? value.strval : "master");
		break;
	case BATT_CHIP_ID:
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_CHIP_ID, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		break;
	case BATT_SHIPMODE_TEST:
		pr_info("%s: show BATT_SHIPMODE_TEST\n", __func__);
		value.intval = 0;
		psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
#if defined(CONFIG_DIRECT_CHARGING)
	case DIRECT_CHARGING_STATUS:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->pd_list.now_isApdo);
		break;
	case DCHG_ADC_MODE_CTRL:
		break;
	case DIRECT_CHARGING_STEP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->step_charging_status);
		break;
	case DIRECT_CHARGING_IIN:
		if (is_pd_apdo_wire_type(battery->wire_status)) {
			value.intval = SEC_BATTERY_IIN_UA;
			psy_do_property(battery->pdata->charger_name, get,
					POWER_SUPPLY_EXT_PROP_MEASURE_INPUT, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					0);
		}
		break;
	case SWITCH_CHARGING_SOURCE:
		psy_do_property(battery->pdata->charger_name, get,
			POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE, value);
		pr_info("%s Test Charging Source(%d)", __func__, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
#else
	case DIRECT_CHARGING_STATUS:
		ret = -1; /* DC not supported model returns -1 */
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", ret);
		break;
#endif
	case CHARGING_TYPE:
		{
			if (battery->cable_type > 0 && battery->cable_type < SEC_BATTERY_CABLE_MAX) {
				value.strval = sec_cable_type[battery->cable_type];
#if defined(CONFIG_DIRECT_CHARGING)
				if (is_pd_apdo_wire_type(battery->cable_type) &&
					battery->current_event & SEC_BAT_CURRENT_EVENT_DC_ERR)
					value.strval = "PDIC";
#endif
			} else
				value.strval = "UNKNOWN";
			pr_info("%s: CHARGING_TYPE = %s\n", __func__, value.strval);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", value.strval);
		}
		break;
	case BATT_FACTORY_MODE:
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
		if (battery->usb_factory_init) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->usb_factory_mode);
		} else
#endif
		{
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				factory_mode);
		}
		break;
	case FACTORY_MODE_RELIEVE:
	case FACTORY_MODE_BYPASS:
	case NORMAL_MODE_BYPASS:
	case FACTORY_VOLTAGE_REGULATION:
	case FACTORY_MODE_DISABLE:
		break;
#if defined(CONFIG_DUAL_BATTERY)
	case BATT_MAIN_VOLTAGE:
		{
			value.intval = SEC_DUAL_BATTERY_MAIN;
			psy_do_property(battery->pdata->dual_battery_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_SUB_VOLTAGE:
		{
			value.intval = SEC_DUAL_BATTERY_SUB;
			psy_do_property(battery->pdata->dual_battery_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_MAIN_CURRENT_MA:
		{
			value.intval = SEC_DUAL_BATTERY_MAIN;
			psy_do_property(battery->pdata->dual_battery_name, get,
				POWER_SUPPLY_PROP_CURRENT_AVG, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_SUB_CURRENT_MA:
		{
			value.intval = SEC_DUAL_BATTERY_SUB;
			psy_do_property(battery->pdata->dual_battery_name, get,
				POWER_SUPPLY_PROP_CURRENT_AVG, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_MAIN_CON_DET:
		{
			value.intval = SEC_DUAL_BATTERY_MAIN;
			psy_do_property(battery->pdata->dual_battery_name, get,
			POWER_SUPPLY_EXT_PROP_DUAL_BAT_DET, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_SUB_CON_DET:
		{
			value.intval = SEC_DUAL_BATTERY_SUB;
			psy_do_property(battery->pdata->dual_battery_name, get,
			POWER_SUPPLY_EXT_PROP_DUAL_BAT_DET, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_MAIN_ENB: /* This pin is reversed by FET */
		{
			if (battery->pdata->main_bat_enb_gpio)
				value.intval = !gpio_get_value(battery->pdata->main_bat_enb_gpio);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_SUB_ENB:
		{
			if (battery->pdata->sub_bat_enb_gpio)
				value.intval = gpio_get_value(battery->pdata->sub_bat_enb_gpio);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
#endif
	case BATT_RESET_SOC:
		break;
	case BATT_READ_RAW_SOC:
		{
			value.intval =
				SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_CAPACITY, value);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_VOLTAGE_NOW:
		{
			value.intval = 0;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval * 1000);
		}
		break;
	case BATT_CURRENT_UA_NOW:
		{
			value.intval = SEC_BATTERY_CURRENT_UA;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_CURRENT_NOW, value);
#if defined(CONFIG_SEC_FACTORY)
			pr_err("%s: batt_current_ua_now (%d)\n",
					__func__, value.intval);
#endif
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_CURRENT_UA_AVG:
		{
			value.intval = SEC_BATTERY_CURRENT_UA;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_CURRENT_AVG, value);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
	case BATT_FILTER_CFG:
		{
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_EXT_PROP_PROP_FILTER_CFG, value);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
				value.intval);
		}
		break;
	case FG_CAPACITY:
	{
		value.intval =
			SEC_BATTERY_CAPACITY_DESIGNED;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x ",
			value.intval);

		value.intval =
			SEC_BATTERY_CAPACITY_ABSOLUTE;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x ",
			value.intval);

		value.intval =
			SEC_BATTERY_CAPACITY_TEMPERARY;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x ",
			value.intval);

		value.intval =
			SEC_BATTERY_CAPACITY_CURRENT;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x\n",
			value.intval);
	}
		break;
	case FG_ASOC:
		value.intval = -1;
		{
			struct power_supply *psy_fg = NULL;

			psy_fg = get_power_supply_by_name(battery->pdata->fuelgauge_name);
			if (!psy_fg) {
				pr_err("%s: Fail to get psy (%s)\n",
						__func__, battery->pdata->fuelgauge_name);
			} else {
				if (psy_fg->desc->get_property != NULL) {
					ret = psy_fg->desc->get_property(psy_fg,
							POWER_SUPPLY_PROP_ENERGY_FULL, &value);
					if (ret < 0) {
						pr_err("%s: Fail to %s get (%d=>%d)\n",
								__func__, battery->pdata->fuelgauge_name,
								POWER_SUPPLY_PROP_ENERGY_FULL, ret);
					}
				}
			}
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       value.intval);
		break;
	case BATT_CAPACITY_MAX:
		psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case BATT_INBAT_VOLTAGE:
	case BATT_INBAT_VOLTAGE_OCV:
		if (battery->pdata->support_fgsrc_change == true) {
			int j, k, ocv, jig_on, ocv_data[10];

			pr_info("%s: FGSRC_SWITCHING_VBAT\n", __func__);
			value.intval = SEC_BAT_INBAT_FGSRC_SWITCHING_VBAT;
			psy_do_property(battery->pdata->fgsrc_switch_name, set,
					POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING, value);

			for (j = 0; j < 10; j++) {
				msleep(175);
				psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
				ocv_data[j] = value.intval;
			}

			ret = psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_EXT_PROP_JIG_GPIO, value);
			if (value.intval < 0 || ret < 0)
				jig_on = 0;
			else if (value.intval == 0)
				jig_on = 1;

			if (battery->is_jig_on || factory_mode || battery->factory_mode || jig_on) {
				pr_info("%s: FGSRC_SWITCHING_VSYS\n", __func__);
				value.intval = SEC_BAT_INBAT_FGSRC_SWITCHING_VSYS;
				psy_do_property(battery->pdata->fgsrc_switch_name, set,
						POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING, value);
			}

			for (j = 1; j < 10; j++) {
				ocv = ocv_data[j];
				k = j;
				while (k > 0 && ocv_data[k-1] > ocv) {
					ocv_data[k] = ocv_data[k-1];
					k--;
				}
				ocv_data[k] = ocv;
			}

			for (j = 0; j < 10; j++)
				pr_info("%s: [%d] %d\n", __func__, j, ocv_data[j]);

			ocv = 0;
			for (j = 2; j < 8; j++)
				ocv += ocv_data[j];

			ret = ocv / 6;
		} else if (battery->pdata->volt_from_pmic) {
			psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_EXT_PROP_PMIC_BAT_VOLTAGE,
					value);
			ret = value.intval;
		} else {
			/* run twice */
			ret = (sec_bat_get_inbat_vol_by_adc(battery) +
					sec_bat_get_inbat_vol_by_adc(battery)) / 2;
		}
		dev_info(battery->dev, "in-battery voltage ocv(%d)\n", ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_INBAT_VOLTAGE_ADC:
		/* run twice */
		ret = (sec_bat_get_inbat_vol_by_adc(battery) +
			sec_bat_get_inbat_vol_by_adc(battery)) / 2;
		dev_info(battery->dev, "in-battery voltage adc(%d)\n", ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_VBYP_VOLTAGE:
		value.intval = SEC_BATTERY_VBYP;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_EXT_PROP_MEASURE_INPUT, value);
		pr_info("%s: vbyp(%d)mV\n", __func__, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		break;
	case ERROR_CAUSE:
		{
			int error_cause = SEC_BAT_ERROR_CAUSE_NONE;

			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_EXT_PROP_ERROR_CAUSE, value);
			error_cause |= value.intval;
			pr_info("%s: ERROR_CAUSE = 0x%X", __func__, error_cause);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				error_cause);
		}
		break;
	case CISD_FULLCAPREP_MAX:
		{
			union power_supply_propval fullcaprep_val;

			fullcaprep_val.intval = SEC_BATTERY_CAPACITY_FULL;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_ENERGY_NOW, fullcaprep_val);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					fullcaprep_val.intval);
		}
		break;
	case BATT_JIG_GPIO:
		value.intval = 0;
		ret = psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_EXT_PROP_JIG_GPIO, value);
		if (value.intval < 0 || ret < 0) {
			value.intval = -1;
			pr_info("%s: does not support JIG GPIO PIN READN\n", __func__);
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case CC_INFO:
		{
			union power_supply_propval cc_val;

			cc_val.intval = SEC_BATTERY_CAPACITY_QH;
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_ENERGY_NOW, cc_val);

			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					cc_val.intval);
		}
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case FG_CYCLE:
		value.intval = SEC_BATTERY_CAPACITY_CYCLE;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);
		value.intval = value.intval / 100;
		dev_info(battery->dev, "fg cycle(%d)\n", value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case FG_FULLCAPNOM:
		value.intval =
			SEC_BATTERY_CAPACITY_AGEDCELL;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
#endif
	case WC_STATUS:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			is_wireless_type(battery->cable_type) ? 1 : 0);
		break;
	case WC_ENABLE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->wc_enable);
		break;
	case WC_CONTROL:
		if (battery->pdata->wpc_en)
			value.intval = battery->wc_enable;
		else
			value.intval = -1;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       value.intval);
		break;
	case WC_CONTROL_CNT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->wc_enable_cnt_value);
		break;
	case LED_COVER:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->led_cover);
		break;
	case BATT_INBAT_WIRELESS_CS100:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_STATUS, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case BATT_WIRELESS_MST_SWITCH_TEST:
		value.intval = SEC_WIRELESS_MST_SWITCH_VERIFY;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		pr_info("%s MST switch verify. result: %x\n", __func__, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
	case WC_VOUT:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_ENERGY_NOW, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case WC_VRECT:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_ENERGY_AVG, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;

	case WC_TX_EN:
		pr_info("%s wc tx enable(%d)", __func__, battery->wc_tx_enable);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->wc_tx_enable);
		break;
	case WC_TX_VOUT:
		pr_info("%s wc tx vout(%d)", __func__, battery->wc_tx_vout);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->wc_tx_vout);
		break;

	case BATT_HV_WIRELESS_STATUS:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case BATT_HV_WIRELESS_PAD_CTRL:
		break;
	case WC_TX_ID:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ID, value);

		pr_info("%s TX ID (%d)", __func__, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case WC_OP_FREQ:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_OP_FREQ, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case WC_CMD_INFO:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TRX_CMD, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%02x ",
			value.intval);

		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TRX_VAL, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%02x ",
			value.intval);
		break;
	case WC_RX_CONNECTED:
		pr_info("%s RX Connected (%d)", __func__, battery->wc_rx_connected);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", battery->wc_rx_connected);
		break;
	case WC_RX_CONNECTED_DEV:
		pr_info("%s RX Type (%d)", __func__, battery->wc_rx_type);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", battery->wc_rx_type);
		break;
	case WC_TX_MFC_VIN_FROM_UNO:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_VIN, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case WC_TX_MFC_IIN_FROM_UNO:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_IIN, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
#if defined(CONFIG_WIRELESS_TX_MODE)
	case WC_TX_AVG_CURR:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
		battery->tx_avg_curr);
		/* If PMS read this value, average Tx current will be reset */
		//battery->tx_clear = true;
		break;
	case WC_TX_TOTAL_PWR:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
		battery->tx_total_power);
		/* If PMS read this value, average Tx current will be reset */
		battery->tx_clear = true;
		break;
#endif
	case WC_TX_STOP_CAPACITY:
		ret = battery->pdata->tx_stop_capacity;
		pr_info("%s tx stop capacity = %d%%", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
		break;
	case WC_TX_TIMER_EN:
		break;
#if defined(CONFIG_WIRELESS_AUTH)
	case WC_AUTH_ADT_SENT:
		{
			//union power_supply_propval val = {0, };
			u8 auth_mode;

			psy_do_property(battery->pdata->wireless_charger_name, get,
				POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_STATUS, value);
			auth_mode = value.intval;
			if (auth_mode == WIRELESS_AUTH_WAIT)
				value.strval = "None";
			else if (auth_mode == WIRELESS_AUTH_START)
				value.strval = "Start";
			else if (auth_mode == WIRELESS_AUTH_SENT)
				value.strval = "Sent";
			else if (auth_mode == WIRELESS_AUTH_RECEIVED)
				value.strval = "Received";
			else if (auth_mode == WIRELESS_AUTH_FAIL)
				value.strval = "Fail";
			else if (auth_mode == WIRELESS_AUTH_PASS)
				value.strval = "Pass";
			else
				value.strval = "None";
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", value.strval);
		}
		break;
#endif
	case WC_DUO_RX_POWER:
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_EXT_PROP_WIRELESS_DUO_RX_POWER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	case BATT_WIRELESS_FIRMWARE_UPDATE:
		value.intval = SEC_WIRELESS_OTP_FIRM_VERIFY;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		pr_info("%s RX firmware verify. result: %d\n", __func__, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case OTP_FIRMWARE_RESULT:
		value.intval = SEC_WIRELESS_OTP_FIRM_RESULT;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case WC_IC_GRADE:
		value.intval = SEC_WIRELESS_IC_GRADE;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%x ", value.intval);

		value.intval = SEC_WIRELESS_IC_REVISION;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%x\n", value.intval);
		break;
	case OTP_FIRMWARE_VER_BIN:
		value.intval = SEC_WIRELESS_OTP_FIRM_VER_BIN;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
	case OTP_FIRMWARE_VER:
		value.intval = SEC_WIRELESS_OTP_FIRM_VER;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
	case TX_FIRMWARE_RESULT:
		value.intval = SEC_WIRELESS_TX_FIRM_RESULT;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", value.intval);
		break;
	case TX_FIRMWARE_VER:
		value.intval = SEC_WIRELESS_TX_FIRM_VER;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
	case BATT_TX_STATUS:
		value.intval = SEC_TX_FIRMWARE;
		psy_do_property(battery->pdata->wireless_charger_name, get,
			POWER_SUPPLY_PROP_MANUFACTURER, value);

		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", value.intval);
		break;
#endif
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sysfs_device_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_device_attrs;
	int ret = -EINVAL;
	int x = 0;
#if defined(CONFIG_DIRECT_CHARGING)
	char direct_charging_source_status[2] = {0, };
#endif
	union power_supply_propval value = {0, };

	switch (offset) {
	case CHECK_SLAVE_CHG:
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case FG_FULL_VOLTAGE:
		break;
#endif
	case BATT_EXT_DEV_CHG:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: Connect Ext Device : %d ", __func__, x);

			switch (x) {
			case EXT_DEV_NONE:
				battery->wire_status = SEC_BATTERY_CABLE_NONE;
				value.intval = 0;
				break;
			case EXT_DEV_GAMEPAD_CHG:
				battery->wire_status = SEC_BATTERY_CABLE_TA;
				value.intval = 0;
				break;
			case EXT_DEV_GAMEPAD_OTG:
				battery->wire_status = SEC_BATTERY_CABLE_OTG;
				value.intval = 1;
				break;
			default:
				break;
			}

			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
					value);

			queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work, 0);
			ret = count;
		}
		break;
	case BATT_WDT_CONTROL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: Charger WDT Set : %d\n", __func__, x);
			battery->wdt_kick_disable = x;
#if defined(CONFIG_DIRECT_CHARGING)
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL, value);
#endif
		}
		ret = count;
		break;
	case MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE, value);
			ret = count;
		}
		break;
	case BATT_CHIP_ID:
		break;
	case BATT_SHIPMODE_TEST:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s ship mode test %d\n", __func__, x);
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST, value);
			ret = count;
		}
		break;
	case DIRECT_CHARGING_STATUS:
		break;
#if defined(CONFIG_DIRECT_CHARGING)
	case DCHG_ADC_MODE_CTRL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev,
				 "%s : direct charger adc mode cntl : %d\n", __func__, x);
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL, value);
			ret = count;
		}
		break;
	case DIRECT_CHARGING_STEP:
		break;
	case DIRECT_CHARGING_IIN:
		break;
	case SWITCH_CHARGING_SOURCE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev, "%s: Request Change Charging Source : %s\n",
				__func__, x == 0 ? "Switch Charger" : "Direct Charger");
			direct_charging_source_status[0] = SEC_TEST_MODE;
			direct_charging_source_status[1] =
				 (x == 0) ? SEC_DIRECT_CHG_CHARGING_SOURCE_SWITCHING : SEC_DIRECT_CHG_CHARGING_SOURCE_DIRECT;
			value.strval = direct_charging_source_status;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE, value);

			ret = count;
		}
		break;
#endif
	case CHARGING_TYPE:
		break;
	case BATT_FACTORY_MODE:
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			battery->usb_factory_mode = value.intval;
			battery->usb_factory_init = true;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_ENERGY_NOW, value);
			ret = count;
		}
#endif
		break;
	case FACTORY_MODE_RELIEVE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			ret = count;
		}
		break;
	case FACTORY_MODE_BYPASS:
		pr_info("%s: factory mode bypass\n", __func__);
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_PROP_AUTHENTIC, value);
			ret = count;
		}
		break;
	case NORMAL_MODE_BYPASS:
		pr_info("%s: normal mode bypass for current measure\n", __func__);
		if (sscanf(buf, "%10d\n", &x) == 1) {
//			if (battery->pdata->detect_moisture && x) {
//				sec_bat_set_charging_status(battery, POWER_SUPPLY_STATUS_DISCHARGING);
//				sec_bat_set_charge(battery, SEC_BAT_CHG_MODE_BUCK_OFF);
//			}

			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE, value);
			ret = count;
		}
		break;
	case FACTORY_VOLTAGE_REGULATION:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_FACTORY_VOLTAGE_REGULATION, value);

			value.intval =
				SEC_FUELGAUGE_CAPACITY_TYPE_RESET;
			psy_do_property(battery->pdata->fuelgauge_name, set,
					POWER_SUPPLY_PROP_CAPACITY, value);
			dev_info(battery->dev, "do reset SOC\n");
			/* update battery info */
			sec_bat_get_battery_info(battery);
			ret = count;
		}
		break;
	case FACTORY_MODE_DISABLE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			psy_do_property(battery->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_DISABLE_FACTORY_MODE, value);
			ret = count;
		}
		break;
#if defined(CONFIG_DUAL_BATTERY)
	case BATT_MAIN_VOLTAGE:
	case BATT_SUB_VOLTAGE:
	case BATT_MAIN_CURRENT_MA:
	case BATT_SUB_CURRENT_MA:
	case BATT_MAIN_CON_DET:
	case BATT_SUB_CON_DET:
		break;
	case BATT_MAIN_ENB: /* This pin is reversed */
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (battery->pdata->main_bat_enb_gpio) {
				pr_info("%s main battery enb = %d\n", __func__, x);
				if (x == 0)
					gpio_direction_output(battery->pdata->main_bat_enb_gpio, 1);
				else if (x == 1)
					gpio_direction_output(battery->pdata->main_bat_enb_gpio, 0);

				pr_info("%s main enb = %d, sub enb = %d\n",
					__func__,
					gpio_get_value(battery->pdata->main_bat_enb_gpio),
					gpio_get_value(battery->pdata->sub_bat_enb_gpio));
			}
			ret = count;
		}
		break;
	case BATT_SUB_ENB:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (battery->pdata->sub_bat_enb_gpio) {
				pr_info("%s sub battery enb = %d\n", __func__, x);
				if (x == 0)
					gpio_direction_output(battery->pdata->sub_bat_enb_gpio, 0);
				else if (x == 1)
					gpio_direction_output(battery->pdata->sub_bat_enb_gpio, 1);

				pr_info("%s main enb = %d, sub enb = %d\n",
					__func__,
					gpio_get_value(battery->pdata->main_bat_enb_gpio),
					gpio_get_value(battery->pdata->sub_bat_enb_gpio));
			}
			ret = count;
		}
		break;
#endif
	case BATT_RESET_SOC:
		/* Do NOT reset fuel gauge in charging mode */
		if (is_nocharge_type(battery->cable_type) ||
			battery->is_jig_on) {
			sec_bat_set_misc_event(battery, BATT_MISC_EVENT_BATT_RESET_SOC, BATT_MISC_EVENT_BATT_RESET_SOC);

			value.intval =
				SEC_FUELGAUGE_CAPACITY_TYPE_RESET;
			psy_do_property(battery->pdata->fuelgauge_name, set,
					POWER_SUPPLY_PROP_CAPACITY, value);
			dev_info(battery->dev, "do reset SOC\n");
			/* update battery info */
			sec_bat_get_battery_info(battery);
		}
		ret = count;
		break;
	case BATT_READ_RAW_SOC:
		break;
	case BATT_VOLTAGE_NOW:
		break;
	case BATT_CURRENT_UA_NOW:
		break;
	case BATT_CURRENT_UA_AVG:
		break;
	case BATT_FILTER_CFG:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			psy_do_property(battery->pdata->fuelgauge_name, set,
					POWER_SUPPLY_EXT_PROP_PROP_FILTER_CFG, value);
			ret = count;
		}
		break;
	case FG_CAPACITY:
		break;
	case FG_ASOC:
#if defined(CONFIG_BATTERY_AGE_FORECAST)
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (x >= 0 && x <= 100) {
				dev_info(battery->dev, "%s: batt_asoc(%d)\n", __func__, x);
				battery->batt_asoc = x;
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_ASOC] = x;
#endif
				sec_bat_check_battery_health(battery);
			}
			ret = count;
		}
#endif
		break;
	case BATT_CAPACITY_MAX:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_err(battery->dev,
					"%s: BATT_CAPACITY_MAX(%d), fg_reset(%d)\n", __func__, x, fg_reset);
			if (!fg_reset && !battery->store_mode) {
				value.intval = x;
				psy_do_property(battery->pdata->fuelgauge_name, set,
						POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, value);

				/* update soc */
				value.intval = 0;
				psy_do_property(battery->pdata->fuelgauge_name, get,
						POWER_SUPPLY_PROP_CAPACITY, value);
				battery->capacity = value.intval;
			} else {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				battery->fg_reset = 1;
#endif
			}
			ret = count;
		}
		break;
	case BATT_INBAT_VOLTAGE:
	case BATT_INBAT_VOLTAGE_OCV:
	case BATT_INBAT_VOLTAGE_ADC:
	case BATT_VBYP_VOLTAGE:
	case ERROR_CAUSE:
	case CISD_FULLCAPREP_MAX:
	case BATT_JIG_GPIO:
	case CC_INFO:
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	case FG_CYCLE:
		break;
	case FG_FULLCAPNOM:
		break;
#endif
	case WC_STATUS:
		break;
	case WC_ENABLE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == 0) {
				mutex_lock(&battery->wclock);
				battery->wc_enable = false;
				battery->wc_enable_cnt = 0;
				mutex_unlock(&battery->wclock);
			} else if (x == 1) {
				mutex_lock(&battery->wclock);
				battery->wc_enable = true;
				battery->wc_enable_cnt = 0;
				mutex_unlock(&battery->wclock);
			} else {
				dev_info(battery->dev,
					"%s: WPC ENABLE unknown command\n",
					__func__);
				return -EINVAL;
			}
			__pm_stay_awake(battery->cable_ws);
			queue_delayed_work(battery->monitor_wqueue,
					&battery->cable_work, 0);
			ret = count;
		}
		break;
	case WC_CONTROL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (battery->pdata->wpc_en) {
				if (x == 0) {
					mutex_lock(&battery->wclock);
					battery->wc_enable = false;
					battery->wc_enable_cnt = 0;
					value.intval = 0;
					psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_EXT_PROP_WC_CONTROL, value);
					if (battery->pdata->wpc_en)
						gpio_direction_output(battery->pdata->wpc_en, 1);
					pr_info("%s: WC CONTROL: Disable", __func__);
					mutex_unlock(&battery->wclock);
				} else if (x == 1) {
					mutex_lock(&battery->wclock);
					battery->wc_enable = true;
					battery->wc_enable_cnt = 0;
					value.intval = 1;
					psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_EXT_PROP_WC_CONTROL, value);
					if (battery->pdata->wpc_en)
						gpio_direction_output(battery->pdata->wpc_en, 0);
					pr_info("%s: WC CONTROL: Enable", __func__);
					mutex_unlock(&battery->wclock);
				} else {
					dev_info(battery->dev,
						"%s: WC CONTROL unknown command\n",
						__func__);
					return -EINVAL;
				}
			}
			ret = count;
		}
		break;
	case WC_CONTROL_CNT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			battery->wc_enable_cnt_value = x;
			ret = count;
		}
		break;
	case LED_COVER:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: MFC, LED_COVER(%d)\n", __func__, x);
			battery->led_cover = x;
			value.intval = battery->led_cover;
			psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_EXT_PROP_PROP_FILTER_CFG, value);
			ret = count;
		}
		break;
	case BATT_INBAT_WIRELESS_CS100:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s send cs100 command\n", __func__);
			value.intval = POWER_SUPPLY_STATUS_FULL;
			psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_STATUS, value);
			ret = count;
		}
		break;
	case BATT_WIRELESS_MST_SWITCH_TEST:
		break;
	case WC_VOUT:
	case WC_VRECT:
	case WC_TX_EN:
		if (sscanf(buf, "%10d\n", &x) == 1) {
#if !defined(CONFIG_BATTERY_SAMSUNG_MHS)
			if (mfc_fw_update) {
				pr_info("@Tx_Mode %s : skip Tx by mfc_fw_update\n", __func__);
				return count;
			}

			if (battery->wc_tx_enable == x) {
				pr_info("@Tx_Mode %s : Ignore same tx status\n", __func__);
				return count;
			}

			if (x &&
				(is_wireless_type(battery->cable_type) || (battery->cable_type == SEC_BATTERY_CABLE_WIRELESS_FAKE))) {
				pr_info("@Tx_Mode %s : Can't enable Tx mode during wireless charging\n", __func__);
				return count;
			}

			pr_info("@Tx_Mode %s: Set TX Enable (%d)\n", __func__, x);
			sec_wireless_set_tx_enable(battery, x);
#if defined(CONFIG_BATTERY_CISD)
			if (x)
				battery->cisd.tx_data[TX_ON]++;
#endif
#endif
			ret = count;
		}
		break;
	case WC_TX_VOUT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("@Tx_Mode %s: Set TX Vout (%d)\n", __func__, x);
			battery->wc_tx_vout = value.intval = x;
			if (battery->wc_tx_enable) {
				pr_info("@Tx_Mode %s: set TX Vout (%d)\n", __func__, value.intval);
				psy_do_property("otg", set,
					POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT, value);
			} else {
				pr_info("@Tx_Mode %s: TX mode turned off now\n", __func__);
			}
			ret = count;
		}
		break;
	case BATT_HV_WIRELESS_STATUS:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == 1 && is_hv_wireless_type(battery->cable_type)) {
#ifdef CONFIG_SEC_FACTORY
				int input_current, charging_current;

				pr_info("%s change cable type HV WIRELESS -> WIRELESS\n", __func__);
				battery->wc_status = SEC_WIRELESS_PAD_WPC;
				battery->cable_type = SEC_BATTERY_CABLE_WIRELESS;
				input_current =  battery->pdata->charging_current[battery->cable_type].input_current_limit;
				charging_current = battery->pdata->charging_current[battery->cable_type].fast_charging_current;
				sec_vote(battery->fcc_vote, VOTER_SLEEP_MODE, true, charging_current);
				sec_vote(battery->input_vote, VOTER_SLEEP_MODE, true, input_current);
#endif
				pr_info("%s HV_WIRELESS_STATUS set to 1. Vout set to 5V\n", __func__);
				value.intval = WIRELESS_VOUT_5V;
				__pm_stay_awake(battery->cable_ws);
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
				__pm_relax(battery->cable_ws);
			} else if (x == 3 && is_hv_wireless_type(battery->cable_type)) {
				pr_info("%s HV_WIRELESS_STATUS set to 3. Vout set to 10V\n", __func__);
				value.intval = WIRELESS_VOUT_10V;
				psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			} else {
				dev_info(battery->dev, "%s: HV_WIRELESS_STATUS unknown command\n", __func__);
				return -EINVAL;
			}
			ret = count;
		}
		break;
	case BATT_HV_WIRELESS_PAD_CTRL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			union power_supply_propval value;

			pr_err("%s: x : %d\n", __func__, x);

			if (x == 1) {
#if defined(CONFIG_SEC_COMMON)
				ret = seccmn_chrg_set_charging_mode('1');
#endif
				if (ret < 0) {
					pr_err("%s:sec_set_param failed\n", __func__);
					return ret;
				}

				pr_info("%s: hv wirelee charging is disabled\n", __func__);
				sleep_mode = true;
				value.intval = WIRELESS_PAD_LED_OFF;
				psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			} else if (x == 2) {
#if defined(CONFIG_SEC_COMMON)
				ret = seccmn_chrg_set_charging_mode('0');
#endif
				if (ret < 0) {
					pr_err("%s: sec_set_param failed\n", __func__);
					return ret;
				}

				pr_info("%s: hv wireless charging is enabled\n", __func__);
				sleep_mode = false;
				value.intval = WIRELESS_PAD_LED_ON;
				psy_do_property(battery->pdata->wireless_charger_name, set,
						POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			} else if (x == 3) {
				pr_info("%s led off\n", __func__);
				value.intval = WIRELESS_PAD_LED_OFF;
				psy_do_property(battery->pdata->wireless_charger_name, set,
								POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			} else if (x == 4) {
				pr_info("%s led on\n", __func__);
				value.intval = WIRELESS_PAD_LED_ON;
				psy_do_property(battery->pdata->wireless_charger_name, set,
								POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
			} else {
				dev_info(battery->dev, "%s: BATT_HV_WIRELESS_PAD_CTRL unknown command\n", __func__);
				return -EINVAL;
			}

			if (is_hv_wireless_type(battery->cable_type) && sleep_mode)
				sec_vote(battery->input_vote, VOTER_SLEEP_MODE, true, battery->pdata->sleep_mode_limit_current);
			else
				sec_vote(battery->input_vote, VOTER_SLEEP_MODE, false, 0);

			ret = count;
		}
		break;
	case WC_TX_ID:
	case WC_OP_FREQ:
	case WC_CMD_INFO:
	case WC_RX_CONNECTED:
	case WC_RX_CONNECTED_DEV:
	case WC_TX_MFC_VIN_FROM_UNO:
	case WC_TX_MFC_IIN_FROM_UNO:
#if defined(CONFIG_WIRELESS_TX_MODE)
	case WC_TX_AVG_CURR:
	case WC_TX_TOTAL_PWR:
#endif
		break;
	case WC_TX_STOP_CAPACITY:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s : tx stop capacity (%d)%%\n", __func__, x);
			if (x >= 0 && x <= 100)
				battery->pdata->tx_stop_capacity = x;
			ret = count;
		}
		break;
	case WC_TX_TIMER_EN:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s : tx receiver detecting timer (%d)%%\n", __func__, x);
			value.intval = x;
			psy_do_property(battery->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TIMER_ON, value);
			ret = count;
		}
		break;
#if defined(CONFIG_WIRELESS_AUTH)
	case WC_AUTH_ADT_SENT:
		break;
#endif
	case WC_DUO_RX_POWER:
		break;
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	case BATT_WIRELESS_FIRMWARE_UPDATE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (sec_bat_check_boost_mfc_condition(battery)) {
				if (x == SEC_WIRELESS_RX_SDCARD_MODE) {
					pr_info("%s fw mode is SDCARD\n", __func__);
					sec_bat_fw_update_work(battery, SEC_WIRELESS_RX_SDCARD_MODE);
				} else if (x == SEC_WIRELESS_RX_BUILT_IN_MODE) {
					pr_info("%s fw mode is BUILD IN\n", __func__);
					sec_bat_fw_update_work(battery, SEC_WIRELESS_RX_BUILT_IN_MODE);
				} else if (x == SEC_WIRELESS_TX_ON_MODE) {
					pr_info("%s tx mode is on\n", __func__);
					sec_bat_fw_update_work(battery, SEC_WIRELESS_TX_ON_MODE);
				} else if (x == SEC_WIRELESS_TX_OFF_MODE) {
					pr_info("%s tx mode is off\n", __func__);
					sec_bat_fw_update_work(battery, SEC_WIRELESS_TX_OFF_MODE);
				} else if (x == SEC_WIRELESS_RX_SPU_MODE) {
					pr_info("%s fw mode is SPU\n", __func__);
					sec_bat_fw_update_work(battery, SEC_WIRELESS_RX_SPU_MODE);
				} else {
					dev_info(battery->dev, "%s: wireless firmware unknown command\n", __func__);
					return -EINVAL;
				}
			} else
				pr_info("%s skip fw update at this time\n", __func__);
			ret = count;
		}
		break;
	case OTP_FIRMWARE_RESULT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == 2) {
				value.intval = x;
				pr_info("%s RX firmware update ready!\n", __func__);
				psy_do_property(battery->pdata->wireless_charger_name, set,
								POWER_SUPPLY_PROP_MANUFACTURER, value);
			} else {
				dev_info(battery->dev, "%s: firmware unknown command\n", __func__);
				return -EINVAL;
			}
			ret = count;
		}
		break;
	case WC_IC_GRADE:
	case OTP_FIRMWARE_VER_BIN:
	case OTP_FIRMWARE_VER:
	case TX_FIRMWARE_RESULT:
	case TX_FIRMWARE_VER:
		break;
	case BATT_TX_STATUS:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == SEC_TX_OFF) {
				pr_info("%s TX mode is off\n", __func__);
				sec_bat_fw_update_work(battery, SEC_WIRELESS_TX_OFF_MODE);
			} else if (x == SEC_TX_STANDBY) {
				pr_info("%s TX mode is on\n", __func__);
				sec_bat_fw_update_work(battery, SEC_WIRELESS_TX_ON_MODE);
			} else {
				dev_info(battery->dev, "%s: TX firmware unknown command\n", __func__);
				return -EINVAL;
			}
			ret = count;
		}
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct sec_sysfs sysfs_device_list = {
	.attr = sysfs_device_attrs,
	.size = ARRAY_SIZE(sysfs_device_attrs),
};

static int __init sysfs_device_init(void)
{
	add_sec_sysfs(&sysfs_device_list.list);
	return 0;
}
late_initcall(sysfs_device_init);
