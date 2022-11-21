/*
 *  sec_adc.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "include/sec_adc.h"

#define DEBUG

static struct qpnp_vadc_chip *adc_client;

struct adc_list {
	const char*	name;
	struct iio_channel *channel;
	struct qpnp_vadc_result prev_value;
	//bool is_used;
};

static struct adc_list batt_adc_list[SEC_BAT_ADC_CHANNEL_NUM] = {
	{.name = "adc-cable"},
	{.name = "adc-bat"},
	{.name = "adc-temp"},
	{.name = "adc-temp"},
	{.name = "adc-full"},
	{.name = "adc-volt"},
	{.name = "adc-chg-temp"},
	{.name = "adc-in-bat"},
	{.name = "adc-dischg"},
	{.name = "adc-dischg-ntc"},
	{.name = "adc-wpc-temp"},
	{.name = "adc-slave-chg-temp"},
	{.name = "adc-usb-temp"},
};
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

static void sec_bat_read_adc(struct qpnp_vadc_chip *vadc, int channel,
		struct qpnp_vadc_result *result, int adc_channel)
{
	int ret = 0;
	int retry_cnt = RETRY_CNT;

	do { 
		ret = qpnp_vadc_read(vadc, channel, result);
		retry_cnt--; 
	} while ((retry_cnt > 0) && ret);

	if (retry_cnt <= 0) { 
		pr_err("%s: Error in ADC(%d) retry_cnt(%d)\n", __func__, adc_channel, retry_cnt); 
		result->adc_code = batt_adc_list[adc_channel].prev_value.adc_code;
		result->physical = batt_adc_list[adc_channel].prev_value.physical;
	} else {
		batt_adc_list[adc_channel].prev_value.adc_code = result->adc_code;
		batt_adc_list[adc_channel].prev_value.physical = result->physical;
	}
	pr_debug("%s, channel: 0x%x, adc: %d, physical: %lld\n", __func__, channel, result->adc_code, result->physical);
}

static int sec_bat_adc_ap_read(struct sec_battery_info *battery, int channel)
{
	struct qpnp_vadc_result results;
	int data = -1;

	switch (channel)
	{
	case SEC_BAT_ADC_CHANNEL_TEMP:
		sec_bat_read_adc(adc_client, battery->pdata->temp_adc_channel, &results, channel);
		data = results.adc_code;
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = 33000;
		break;
	case SEC_BAT_ADC_CHANNEL_BAT_CHECK:
		sec_bat_read_adc(NULL, LR_MUX2_BAT_ID, &results, channel);
		pr_debug("BAT_ID physical= %lld, raw = 0x%x\n", results.physical, results.adc_code);
		data = results.physical;
		break;
	case SEC_BAT_ADC_CHANNEL_USB_TEMP:
		sec_bat_read_adc(adc_client, battery->pdata->usb_temp_adc_channel, &results, channel);
		data = results.adc_code;
		break;	
	case SEC_BAT_ADC_CHANNEL_CHG_TEMP:
		sec_bat_read_adc(adc_client, VADC_AMUX_THM2_PU2, &results, channel);
		data = results.adc_code;
		break;
	case SEC_BAT_ADC_CHANNEL_WPC_TEMP:
		sec_bat_read_adc(adc_client, battery->pdata->temp_adc_channel, &results, channel);
		data = results.adc_code;
		break;
	case SEC_BAT_ADC_CHANNEL_INBAT_VOLTAGE:
		sec_bat_read_adc(adc_client, VBAT_SNS, &results, channel);
		data = ((int)results.physical)/1000;
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

static void sec_bat_adc_none_init(struct platform_device *pdev)
{
}

static int sec_bat_adc_none_read(int channel)
{
	return 0;
}

static void sec_bat_adc_none_exit(void)
{
}

static void sec_bat_adc_ic_init(struct platform_device *pdev)
{
}

static int sec_bat_adc_ic_read(int channel)
{
	return 0;
}

static void sec_bat_adc_ic_exit(void)
{
}
static int adc_read_type(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		adc = sec_bat_adc_none_read(channel);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		adc = sec_bat_adc_ap_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		adc = sec_bat_adc_ic_read(channel);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
	pr_debug("[%s] [%d] ADC = %d\n", __func__, channel, adc);
	return adc;

}

static void adc_init_type(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_init(pdev);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_init(pdev);
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

int sec_bat_get_adc_data(struct sec_battery_info *battery,
			int adc_ch, int count)
{
	int adc_data = 0;
	int adc_max = 0;
	int adc_min = 0;
	int adc_total = 0;
	int i = 0;

	for (i = 0; i < count; i++) {
		mutex_lock(&battery->adclock);
#ifdef CONFIG_OF
		adc_data = adc_read_type(battery, adc_ch);
#else
		adc_data = adc_read(battery->pdata, adc_ch);
#endif
		mutex_unlock(&battery->adclock);

		if (adc_data < 0)
			goto err;

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (count - 2);
err:
	return adc_data;
}

int adc_read(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	adc = adc_read_type(battery, channel);

	dev_dbg(battery->dev, "[%s]adc = %d\n", __func__, adc);

	return adc;
}

int sec_bat_get_adc_value(
		struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	adc = sec_bat_get_adc_data(battery, channel,
		battery->pdata->adc_check_count);

	if (adc < 0) {
		dev_err(battery->dev,
			"%s: Error in ADC\n", __func__);
		return adc;
	}

	return adc;
}

int sec_bat_get_charger_type_adc
				(struct sec_battery_info *battery)
{
	/* It is true something valid is
	connected to the device for charging.
	By default this something is considered to be USB.*/
	int result = SEC_BATTERY_CABLE_USB;

	int adc = 0;
	int i = 0;

	/* Do NOT check cable type when cable_switch_check() returns false
	 * and keep current cable type
	 */
	if (battery->pdata->cable_switch_check &&
	    !battery->pdata->cable_switch_check())
		return battery->cable_type;

	adc = sec_bat_get_adc_value(battery,
		SEC_BAT_ADC_CHANNEL_CABLE_CHECK);

	/* Do NOT check cable type when cable_switch_normal() returns false
	 * and keep current cable type
	 */
	if (battery->pdata->cable_switch_normal &&
	    !battery->pdata->cable_switch_normal())
		return battery->cable_type;

	for (i = 0; i < SEC_BATTERY_CABLE_MAX; i++)
		if ((adc > battery->pdata->cable_adc_value[i].min) &&
			(adc < battery->pdata->cable_adc_value[i].max))
			break;
	if (i >= SEC_BATTERY_CABLE_MAX)
		dev_err(battery->dev,
			"%s : default USB\n", __func__);
	else
		result = i;

	dev_dbg(battery->dev, "%s : result(%d), adc(%d)\n",
		__func__, result, adc);

	return result;
}

bool sec_bat_get_value_by_adc(
				struct sec_battery_info *battery,
				enum sec_battery_adc_channel channel,
				union power_supply_propval *value)
{
	int temp = 0;
	int temp_adc;
	int low = 0;
	int high = 0;
	int mid = 0;
	const sec_bat_adc_table_data_t *temp_adc_table = {0 , };
	unsigned int temp_adc_table_size = 0;

	temp_adc = sec_bat_get_adc_value(battery, channel);
	if (temp_adc < 0)
		return true;

	switch (channel) {
	case SEC_BAT_ADC_CHANNEL_TEMP:
		temp_adc_table = battery->pdata->temp_adc_table;
		temp_adc_table_size =
			battery->pdata->temp_adc_table_size;
		battery->temp_adc = temp_adc;
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		temp_adc_table = battery->pdata->temp_amb_adc_table;
		temp_adc_table_size =
			battery->pdata->temp_amb_adc_table_size;
		battery->temp_ambient_adc = temp_adc;
		break;
	case SEC_BAT_ADC_CHANNEL_USB_TEMP:
		temp_adc_table = battery->pdata->usb_temp_adc_table;
		temp_adc_table_size =
			battery->pdata->usb_temp_adc_table_size;
		battery->usb_temp_adc = temp_adc;
		break;
	case SEC_BAT_ADC_CHANNEL_CHG_TEMP:
		temp_adc_table = battery->pdata->chg_temp_adc_table;
		temp_adc_table_size =
			battery->pdata->chg_temp_adc_table_size;
		battery->chg_temp_adc = temp_adc;
		break;
	case SEC_BAT_ADC_CHANNEL_WPC_TEMP: /* Coil Therm */
		temp_adc_table = battery->pdata->wpc_temp_adc_table;
		temp_adc_table_size =
			battery->pdata->wpc_temp_adc_table_size;
		battery->wpc_temp_adc = temp_adc;
		battery->coil_temp_adc = temp_adc;
		break;
	case SEC_BAT_ADC_CHANNEL_SLAVE_CHG_TEMP:
		temp_adc_table = battery->pdata->slave_chg_temp_adc_table;
		temp_adc_table_size =
			battery->pdata->slave_chg_temp_adc_table_size;
		battery->slave_chg_temp_adc = temp_adc;
		break;
	default:
		dev_err(battery->dev,
			"%s: Invalid Property\n", __func__);
		return false;
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
	value->intval = temp;

	dev_info(battery->dev,
		"%s:[%d] Temp(%d), Temp-ADC(%d)\n",
		__func__,channel, temp, temp_adc);

	return true;
}

bool sec_bat_check_vf_adc(struct sec_battery_info *battery)
{
	int adc = 0;

	adc = sec_bat_get_adc_data(battery,
		SEC_BAT_ADC_CHANNEL_BAT_CHECK,
		battery->pdata->adc_check_count);

	if (adc < 0) {
		dev_err(battery->dev, "%s: VF ADC error\n", __func__);
		adc = battery->check_adc_value;
	} else
		battery->check_adc_value = adc;

	if ((battery->check_adc_value <= battery->pdata->check_adc_max) &&
		(battery->check_adc_value >= battery->pdata->check_adc_min)) {
		return true;
	} else {
		dev_info(battery->dev, "%s: adc (%d)\n", __func__, battery->check_adc_value);
		return false;
	}
}

void adc_init(struct platform_device *pdev, struct sec_battery_info *battery)
{
	adc_init_type(pdev, battery);
}

void adc_exit(struct sec_battery_info *battery)
{
	adc_exit_type(battery);
}

