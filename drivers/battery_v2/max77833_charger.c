/*
 *  max77833_charger.c
 *  Samsung MAX77833 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include <linux/mfd/max77833-private.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/mfd/max77833.h>
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif
#include <linux/of_gpio.h>
#include "include/charger/max77833_charger.h"

extern unsigned int lpcharge;

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
extern bool sleep_mode;
extern bool wpc_temp_mode;
#endif

#define ENABLE 1
#define DISABLE 0

static enum power_supply_property max77833_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_PROP_USB_HC,
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
#endif
#if defined(CONFIG_AFC_CHARGER_MODE)
	POWER_SUPPLY_PROP_AFC_CHARGER_MODE,
#endif
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_AICL_CONTROL,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
};
static enum power_supply_property max77833_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static struct device_attribute max77833_charger_attrs[] = {
	MAX77833_CHARGER_ATTR(chip_id),
};
#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
static void max77833_check_wpc(struct max77833_charger_data *charger);
#endif
static void max77833_charger_initialize(struct max77833_charger_data *charger);
static int max77833_get_vbus_state(struct max77833_charger_data *charger);
static int max77833_get_charger_state(struct max77833_charger_data *charger);
static void max77833_set_charger_state(struct max77833_charger_data *charger,
				       int enable);
static bool max77833_charger_unlock(struct max77833_charger_data *charger)
{
	u8 reg_data;
	u8 chgprot;
	int retry_cnt = 0;
	bool need_init = false;

	do {
		max77833_read_reg(charger->i2c, MAX77833_CHG_REG_PROTECT, &reg_data);
		chgprot = reg_data & 0x03;
		if (chgprot != 0x03) {
			pr_err("%s: unlock err, chgprot(0x%x), retry(%d)\n",
					__func__, chgprot, retry_cnt);
			max77833_write_reg(charger->i2c, MAX77833_CHG_REG_PROTECT,
					   0x03);
			need_init = true;
			msleep(20);
		} else {
			pr_debug("%s: unlock success, chgprot(0x%x)\n",
				__func__, chgprot);
			break;
		}
	} while ((chgprot != 0x03) && (++retry_cnt < 10));

	return need_init;
}

static void check_charger_unlock_state(struct max77833_charger_data *charger)
{
	bool need_reg_init;
	pr_debug("%s\n", __func__);

	need_reg_init = max77833_charger_unlock(charger);
	if (need_reg_init) {
		pr_err("%s: charger locked state, reg init\n", __func__);
		max77833_charger_initialize(charger);
	}
}

static void max77833_test_read(struct max77833_charger_data *charger)
{
	u8 reg_data;
	u8 reg_addr;
	char *str = NULL;

	str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
	if (!str)
		return;

	/* 0x81~0x9D */
	reg_addr = 0x81;
	while (reg_addr <= 0x9D)
	{
		max77833_read_reg(charger->i2c, reg_addr, &reg_data);
		sprintf(str+strlen(str), "0x%02x(0x%02x),", reg_addr, reg_data);
		reg_addr += 0x01;
	}

	pr_debug("%s: [CHG] %s\n", __func__, str);
	kfree(str);
}

static int max77833_get_vbus_state(struct max77833_charger_data *charger)
{
	u8 reg_data;
	union power_supply_propval value;

	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_DTLS_00, &reg_data);

	psy_do_property("battery", get, POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
	reg_data = ((reg_data & MAX77833_CHGIN_DTLS) >>
		MAX77833_CHGIN_DTLS_SHIFT);
#else
	if (value.intval == POWER_SUPPLY_TYPE_WIRELESS ||
		value.intval == POWER_SUPPLY_TYPE_HV_WIRELESS ||
		value.intval == POWER_SUPPLY_TYPE_HV_WIRELESS_ETX ||
		value.intval == POWER_SUPPLY_TYPE_PMA_WIRELESS)
		reg_data = ((reg_data & MAX77833_WCIN_DTLS) >>
				MAX77833_WCIN_DTLS_SHIFT);
	else
		reg_data = ((reg_data & MAX77833_CHGIN_DTLS) >>
				MAX77833_CHGIN_DTLS_SHIFT);
#endif

	switch (reg_data) {
	case 0x00:
		pr_info("%s: VBUS is invalid. CHGIN < CHGIN_UVLO\n",
			__func__);
		break;
	case 0x01:
		pr_info("%s: VBUS is invalid. CHGIN < MBAT+CHGIN2SYS" \
			"and CHGIN > CHGIN_UVLO\n", __func__);
		break;
	case 0x02:
		pr_info("%s: VBUS is invalid. CHGIN > CHGIN_OVLO",
			__func__);
		break;
	case 0x03:
		pr_info("%s: VBUS is valid. CHGIN < CHGIN_OVLO", __func__);
		break;
	default:
		break;
	}

	return reg_data;
}

static int max77833_get_charger_state(struct max77833_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 reg_data;

	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_DTLS_01, &reg_data);

	pr_info("%s : charger status (0x%02x)\n", __func__, reg_data);

	reg_data &= 0x0f;

	switch (reg_data)
	{
	case 0x00:
	case 0x01:
	case 0x02:
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x03:
	case 0x04:
		status = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x05:
	case 0x06:
	case 0x07:
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x08:
	case 0xA:
	case 0xB:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		status = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return (int)status;
}

static int max77833_get_charging_health(struct max77833_charger_data *charger)
{
	int state;
	int vbus_state;
	int retry_cnt;
	u8 chg_dtls_00, chg_dtls;
	u8 chg_cnfg_00, chg_cnfg_04 ,chg_cnfg_05, chg_cnfg_06, chg_cnfg_16, chg_cnfg_18;
	union power_supply_propval value;

        /* watchdog kick */
	max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_12,
			MAX77833_WDTCLR, MAX77833_WDTCLR);

	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_HEALTH, value);
	/* VBUS OVP state return battery OVP state */
	vbus_state = max77833_get_vbus_state(charger);
	/* read CHG_DTLS and detecting battery terminal error */
	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_DTLS_01, &chg_dtls);
	chg_dtls = ((chg_dtls & MAX77833_CHG_DTLS) >>
		    MAX77833_CHG_DTLS_SHIFT);
	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_CNFG_00, &chg_cnfg_00);

	/* print the log at the abnormal case */
	if((charger->is_charging == 1) && (chg_dtls & 0x08)) {
		max77833_read_reg(charger->i2c,
			MAX77833_CHG_REG_DTLS_00, &chg_dtls_00);
		max77833_read_reg(charger->i2c,
			MAX77833_CHG_REG_CNFG_04, &chg_cnfg_04);
		max77833_read_reg(charger->i2c,
			MAX77833_CHG_REG_CNFG_05, &chg_cnfg_05);
		max77833_read_reg(charger->i2c,
			MAX77833_CHG_REG_CNFG_06, &chg_cnfg_06);
		max77833_read_reg(charger->i2c,
				MAX77833_CHG_REG_CNFG_16, &chg_cnfg_16);
		max77833_read_reg(charger->i2c,
				MAX77833_CHG_REG_CNFG_18, &chg_cnfg_18);

		pr_info("%s: CHG_DTLS_00(0x%x), CHG_DTLS_01(0x%x), CHG_CNFG_00(0x%x)\n",
			__func__, chg_dtls_00, chg_dtls, chg_cnfg_00);
		pr_info("%s:  CHG_CNFG_04(0x%x), CHG_CNFG_05(0x%x), CHG_CNFG_06(0x%x)\n",
			__func__, chg_cnfg_04, chg_cnfg_05, chg_cnfg_06);
		pr_info("%s:  CHG_CNFG_16(0x%x), CHG_CNFG_18(0x%x)\n",
			__func__, chg_cnfg_16, chg_cnfg_18);
		max77833_set_charger_state(charger, 0);
		max77833_set_charger_state(charger, 1);
	}

	pr_info("%s: vbus_state : 0x%x, chg_dtls : 0x%x\n", __func__, vbus_state, chg_dtls);
	/*  OVP is higher priority */
	if (vbus_state == 0x02) { /*  CHGIN_OVLO */
		pr_info("%s: vbus ovp\n", __func__);
		state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS ||
			charger->cable_type == POWER_SUPPLY_TYPE_HV_WIRELESS ||
			charger->cable_type == POWER_SUPPLY_TYPE_PMA_WIRELESS ) {
			retry_cnt = 0;
			do {
				msleep(50);
				vbus_state = max77833_get_vbus_state(charger);
			} while((retry_cnt++ < 2) && (vbus_state == 0x02));
			if (vbus_state == 0x02) {
				state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
				pr_info("%s: wpc and over-voltage\n", __func__);
			} else
				state = POWER_SUPPLY_HEALTH_GOOD;
		}
	} else if (((vbus_state == 0x0) || (vbus_state == 0x01)) && (chg_dtls & 0x08) && \
		    (chg_cnfg_00 & MAX77833_MODE_BUCK) &&	\
		    (chg_cnfg_00 & MAX77833_MODE_CHGR) &&	\
		    (charger->cable_type != POWER_SUPPLY_TYPE_WIRELESS) &&	\
		    (charger->cable_type != POWER_SUPPLY_TYPE_HV_WIRELESS) &&	\
		    (charger->cable_type != POWER_SUPPLY_TYPE_PMA_WIRELESS)) {
		pr_info("%s: vbus is under\n", __func__);
		state = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
	} else if ((value.intval == POWER_SUPPLY_HEALTH_UNDERVOLTAGE) && \
			((vbus_state == 0x0) || (vbus_state == 0x01)) && \
			(charger->cable_type != POWER_SUPPLY_TYPE_WIRELESS) && \
			(charger->cable_type != POWER_SUPPLY_TYPE_HV_WIRELESS) && \
			(charger->cable_type != POWER_SUPPLY_TYPE_PMA_WIRELESS)) {
		pr_info("%s: keep under-voltage\n", __func__);
		state = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
	} else {
		pr_info("%s: set health good.\n", __func__);
		state = POWER_SUPPLY_HEALTH_GOOD;
	}

	return (int)state;
}

static u8 max77833_get_float_voltage_data(int float_voltage)
{
	int voltage = 3000;
	int i;

	for (i = 0; voltage <= 4500; i++) {
		if (float_voltage <= voltage)
			break;
		voltage += 10;
	}


	return i;
}

static int max77833_get_chg_adc_data(struct max77833_charger_data *charger, u8 channel)
{
	u8 chg_cnfg_19, chg_cnfg_20, chg_dtls_02;
	u32 adc_avg, temp, temp2;

	max77833_read_reg(charger->i2c,
		MAX77833_CHG_REG_CNFG_20, &chg_cnfg_20);
	if (channel != chg_cnfg_20) {
		max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_20, channel);
	}

	max77833_read_reg(charger->i2c,
		MAX77833_CHG_REG_CNFG_19, &chg_cnfg_19);

	switch (channel) {
		case CHG_CNFG_20_ADC_CHG_SEL_V_WCIN:
			temp = chg_cnfg_19 & 0x0F;
			adc_avg = (16 * temp * 170898) / 1000000;

			temp = (chg_cnfg_19 & 0xF0) >> 4;
			temp2 = (16 * temp * 170898) / 1000000;
			adc_avg += (temp2 << 4);
			break;
		case CHG_CNFG_20_ADC_CHG_SEL_V_CHGIN:
			temp = chg_cnfg_19 & 0x0F;
			adc_avg = (16 * temp * 3417969) / 1000000;

			temp = (chg_cnfg_19 & 0xF0) >> 4;
			temp2 = (16 * temp * 3417969) / 1000000;
			adc_avg += (temp2 << 4);
			break;
		case CHG_CNFG_20_ADC_CHG_SEL_V_BYP:
			temp = chg_cnfg_19 & 0x0F;
			adc_avg = (16 * temp * 3417969) / 100000;

			temp = (chg_cnfg_19 & 0xF0) >> 4;
			temp2 = (16 * temp * 3417969) / 100000;
			adc_avg += (temp2 << 4);
			break;
		case CHG_CNFG_20_ADC_CHG_SEL_IIN:
			max77833_read_reg(charger->i2c,
				MAX77833_CHG_REG_DTLS_02, &chg_dtls_02);

			if (chg_dtls_02 & 0x08) {
				/* IIN CHGIN current : mA*/
				adc_avg = 16 * chg_cnfg_19;
			} else {
				/* IIN WCIN current : mA*/
				adc_avg = (16 * chg_cnfg_19) / 2;
			}
			break;
		default:
			adc_avg = 0;
			break;
	}
	return (int)adc_avg;
}

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
static void max77833_check_wpc(struct max77833_charger_data *charger)
{
	int cnt_iin = 0;
	int wpc_iin = 0;
	int v_chgin;
	union power_supply_propval value;
	u8 chg_cnfg_00;

	do {
		wpc_iin = max77833_get_chg_adc_data(charger, CHG_CNFG_20_ADC_CHG_SEL_IIN);
		v_chgin = max77833_get_chg_adc_data(charger, CHG_CNFG_20_ADC_CHG_SEL_V_CHGIN);
		pr_info("%s: it might miss align. imax(%dmA), iin(%dmA), v_chgin(%dmV)\n", __func__, charger->charging_current_max, wpc_iin, v_chgin);

		max77833_read_reg(charger->i2c,
				  MAX77833_CHG_REG_CNFG_00, &chg_cnfg_00);
		if((chg_cnfg_00 & MAX77833_MODE_BUCK) == 0) {
			pr_info("%s 0x%x buck off! \n", __func__, chg_cnfg_00);
			break;
		}

		cnt_iin ++;
		if(cnt_iin >= 4) {
			cnt_iin = 0;
			value.intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			psy_do_property(charger->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_STATUS, value);
			break;
		}
		pr_info("%s cnt = %d \n", __func__, cnt_iin);
		msleep(100);
	} while(wpc_iin == 0);
}
#endif

static int max77833_get_input_current(struct max77833_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
	max77833_read_reg(charger->i2c,
		MAX77833_CHG_REG_CNFG_16, &reg_data);
	/* AND operation for removing the formal 1bit  */

	if (reg_data <= 0x3)
		get_current = 75;
	else if (reg_data >= 0xA0)
		get_current = 4000;
	else
		get_current = reg_data * 25;
#else
	if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS) {
		max77833_read_reg(charger->i2c,
			MAX77833_CHG_REG_CNFG_17, &reg_data);
		/* AND operation for removing the formal 2bit  */
		reg_data = reg_data & 0x7F;

		if (reg_data <= 0x3)
			get_current = 75;
		else if (reg_data >= 0x50)
			get_current = 2000;
		else
			get_current = reg_data * 25;
	} else {
		max77833_read_reg(charger->i2c,
			MAX77833_CHG_REG_CNFG_16, &reg_data);
		/* AND operation for removing the formal 1bit  */

		if (reg_data <= 0x3)
			get_current = 75;
		else if (reg_data >= 0xA0)
			get_current = 4000;
		else
			get_current = reg_data * 25;
	}
#endif
	return get_current;
}

static bool max77833_check_battery(struct max77833_charger_data *charger)
{
	u8 reg_data;
	u8 reg_data2;

	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_INT_OK, &reg_data);

	pr_info("%s : CHG_INT_OK(0x%x)\n", __func__, reg_data);

	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_DTLS_00, &reg_data2);

	pr_info("%s : CHG_DETAILS00(0x%x)\n", __func__, reg_data2);

	if ((reg_data & MAX77833_BATP_OK) ||
	    !(reg_data2 & MAX77833_BATP_DTLS))
		return true;
	else
		return false;
}

static void max77833_set_buck(struct max77833_charger_data *charger,
		int enable)
{
	u8 reg_data;

	if (enable) {
		max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
				    CHG_CNFG_00_BUCK_MASK, CHG_CNFG_00_BUCK_MASK);
	} else {
		max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
				    0, CHG_CNFG_00_BUCK_MASK);
	}
	max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00, &reg_data);
	pr_debug("%s : CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
}

static void max77833_check_slow_charging(struct max77833_charger_data *charger,
					 int input_current)
{
	/* under 400mA considered as slow charging concept for VZW */
	if (input_current <= SLOW_CHARGING_CURRENT_STANDARD &&
			charger->cable_type != POWER_SUPPLY_TYPE_BATTERY) {
		union power_supply_propval value;

		charger->aicl_on = true;
		pr_info("%s: slow charging on : input current(%dmA), cable type(%d)\n",
			__func__, input_current, charger->cable_type);

		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_CHARGE_TYPE, value);
	}
	else
		charger->aicl_on = false;
}

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
#define CURRENT_STEP	100
#if defined(CONFIG_SEC_FACTORY)
#define SLEEP_STEP		250
#else
#define SLEEP_STEP		1000
#endif

static void max77833_set_input_current(struct max77833_charger_data *charger,
				       int input_current)
{
	u8 set_reg, reg_data;
	int quotient;
	int diff_current_now = 0;
	int quotient_wpc = 0, remainder_wpc = 0;
	int i =0;
	static int pre_current_max = 500;

	mutex_lock(&charger->charger_mutex);

	set_reg = MAX77833_CHG_REG_CNFG_16;
	max77833_read_reg(charger->i2c,
		set_reg, &reg_data);

	if (!input_current) {
		max77833_write_reg(charger->i2c,
				   set_reg, reg_data);
	} else {
		input_current = (input_current > charger->charging_current_max) ?
		charger->charging_current_max : input_current;

		if (input_current >= 4000)
			reg_data = 0xA0;
		else if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS ||
					charger->cable_type == POWER_SUPPLY_TYPE_HV_WIRELESS ||
					charger->cable_type == POWER_SUPPLY_TYPE_PMA_WIRELESS){
#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
			union power_supply_propval value;
#endif
			wake_lock(&charger->wpc_wake_lock);
			if (input_current < pre_current_max) {
				diff_current_now = pre_current_max - input_current;
				remainder_wpc = diff_current_now % CURRENT_STEP;
				quotient_wpc = diff_current_now / CURRENT_STEP;

				for(i=0; i < quotient_wpc; i++)
				{
#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
					psy_do_property(charger->pdata->wireless_charger_name, get, POWER_SUPPLY_PROP_ONLINE, value);
					if(value.intval == SEC_WIRELESS_PAD_NONE) {
						pr_info("%s stop to set current, wireless was just dettached \n", __func__);
						break;
					}
#endif
					pre_current_max -= CURRENT_STEP;
					quotient = pre_current_max / 25;
					reg_data = quotient;
					max77833_write_reg(charger->i2c, set_reg, reg_data);
					msleep(SLEEP_STEP);
					pr_info("%s pre_current_max = %d, reg_data = 0x%x \n", __func__, pre_current_max, reg_data);
				}
				pre_current_max -= remainder_wpc;
				quotient = pre_current_max / 25;
				reg_data = quotient;
				max77833_write_reg(charger->i2c,
					set_reg, reg_data);
				pr_info("%s 1. pre_current_max = %d, reg_data = 0x%x \n", __func__, pre_current_max, reg_data);
			} else if (input_current > pre_current_max) {
				diff_current_now = input_current - pre_current_max;
				remainder_wpc = diff_current_now % CURRENT_STEP;
				quotient_wpc = diff_current_now / CURRENT_STEP;
				for(i=0; i < quotient_wpc; i++)
				{
#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
					psy_do_property(charger->pdata->wireless_charger_name, get, POWER_SUPPLY_PROP_ONLINE, value);
					if(value.intval == SEC_WIRELESS_PAD_NONE) {
						pr_info("%s stop to set current, wireless was just dettached \n", __func__);
						break;
#endif
					}
					pre_current_max += CURRENT_STEP;
					quotient = pre_current_max / 25;
					reg_data = quotient;
					max77833_write_reg(charger->i2c, set_reg, reg_data);
					msleep(SLEEP_STEP);
					pr_info("%s 2. pre_current_max = %d, reg_data = 0x%x \n", __func__, pre_current_max, reg_data);
				}
				pre_current_max += remainder_wpc;
				quotient = pre_current_max / 25;
				reg_data = quotient;
				max77833_write_reg(charger->i2c, set_reg, reg_data);
				pr_info("%s pre_current_max = %d, reg_data = 0x%x \n", __func__, pre_current_max, reg_data);
			} else {
				reg_data = input_current / 25;
				max77833_write_reg(charger->i2c, set_reg, reg_data);
				pr_info("%s 3. pre_current_max = %d, reg_data = 0x%x \n", __func__, pre_current_max, reg_data);
			}
			wake_unlock(&charger->wpc_wake_lock);
		} else {
			input_current = (input_current > charger->charging_current_max) ?
			charger->charging_current_max : input_current;
			pre_current_max = 500;

			if (input_current >= 4000)
				reg_data = 0xA0;
			else
				reg_data = input_current / 25;
			max77833_write_reg(charger->i2c, set_reg, reg_data);
		}
	}

	mutex_unlock(&charger->charger_mutex);
	pr_info("[%s] REG(0x%02x) DATA(0x%02x)\n",
		__func__, set_reg, reg_data);
}
#else
static void max77833_set_input_current(struct max77833_charger_data *charger,
				       int input_current)
{
	u8 set_reg, reg_data;

	mutex_lock(&charger->charger_mutex);
	if (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS) {
		set_reg = MAX77833_CHG_REG_CNFG_17;
		max77833_read_reg(charger->i2c,
				  set_reg, &reg_data);
		reg_data &= ~MAX77833_CHG_WCIN_LIM;
	} else {
		set_reg = MAX77833_CHG_REG_CNFG_16;
		max77833_read_reg(charger->i2c,
				  set_reg, &reg_data);
	}

	if (!input_current) {
		max77833_write_reg(charger->i2c,
				   set_reg, reg_data);
	} else if(charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS) {
		if (input_current >= 2000)
			reg_data = 0x50;
		else
			reg_data = input_current / 25;
		max77833_write_reg(charger->i2c,
				   set_reg, reg_data);
	} else {
		if (input_current >= 4000)
			reg_data = 0xA0;
		else
			reg_data = input_current / 25;

		max77833_write_reg(charger->i2c,
				   set_reg, reg_data);
	}

	mutex_unlock(&charger->charger_mutex);
	pr_info("[%s] REG(0x%02x) DATA(0x%02x)\n",
		__func__, set_reg, reg_data);
}
#endif

static void max77833_set_charge_current(struct max77833_charger_data *charger,
					int fast_charging_current)
{
	int curr_step = 50;
	u8 reg_data;

	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_CNFG_05, &reg_data);

	if (!fast_charging_current) {
		max77833_write_reg(charger->i2c,
				   MAX77833_CHG_REG_CNFG_05, 0x08);
	} else {
		reg_data = fast_charging_current / curr_step;
		max77833_write_reg(charger->i2c,MAX77833_CHG_REG_CNFG_05, reg_data);
	}

	pr_info("[%s] REG(0x%02x) DATA(0x%02x), CURRENT(%d)\n",
		__func__, MAX77833_CHG_REG_CNFG_05,
		reg_data, fast_charging_current);
}

static int max77833_check_aicl_state(struct max77833_charger_data *charger)
{
	u8 aicl_state;

	if (!charger->max77833->pmic_rev_pass5)
		return 0;

	if (charger->afc_detect)
		return 0;

	if (!max77833_read_reg(charger->i2c, MAX77833_CHG_REG_INT_OK, &aicl_state)) {
		pr_info("%s aicl state (0x%x)\n", __func__, aicl_state);
		return !(aicl_state & 0x80);
	}
	return 0;
}

static void max77833_set_topoff_current(struct max77833_charger_data *charger,
					int termination_current)
{
	int curr_base, curr_step;
	u8 reg_data;

	curr_base = 150;
	curr_step = 50;

	if (termination_current < curr_base)
		termination_current = curr_base;
	else if (termination_current > 500)
		termination_current = 500;

	reg_data = (termination_current - curr_base) / curr_step;
	max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_02,
			    reg_data, 0x7);

	pr_info("%s: reg_data(0x%02x), topoff(%d)\n",
		__func__, reg_data, termination_current);
}

static void max77833_set_charger_state(struct max77833_charger_data *charger,
				       int enable)
{
	u8 reg_data;

	if (enable) {
		max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
				CHG_CNFG_00_CHG_MASK, CHG_CNFG_00_CHG_MASK);
	} else {
		max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
				0, CHG_CNFG_00_CHG_MASK);
	}
	max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00, &reg_data);
	pr_debug("%s : CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
}

static void max77833_charger_initialize(struct max77833_charger_data *charger)
{
	u8 reg_data;
	pr_info("%s\n", __func__);

	/* unmasked: CHGIN_I, WCIN_I, BATP_I, BYP_I	*/
	/*max77833_write_reg(charger->i2c, MAX77833_CHG_REG_INT_MASK, 0x9a);*/

	/* unlock charger setting protect */
	reg_data = 0x03;
	max77833_write_reg(charger->i2c, MAX77833_CHG_REG_PROTECT, reg_data);

	/*
	 * fast charge timer disable
	 * restart threshold disable
	 * pre-qual charge enable(default)
	 */
	reg_data = (0x03 << 4);
	max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_04, reg_data);

	/*
	 * top off current 125mA
	 * top off timer 70min
	 * otg current limit 1200mA
	 */
	reg_data = 0xB8;
	max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_02, reg_data);

	/*
	 * cv voltage 4.2V or 4.35V
	 * MINVSYS 3.6V(default)
	 */
	reg_data = max77833_get_float_voltage_data(charger->pdata->chg_float_voltage);
	max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_06, reg_data);
	pr_info("%s: battery cv voltage 0x%x\n", __func__, reg_data);

	/* Fix Switching Frequency : 2Mhz */
	max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_03, 0x0);

#ifdef CONFIG_SEC_FACTORY
	/* SYS_OCP_ACT = 0 */
	max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_15, 0, 1);
#else
	if (charger->max77833->pmic_rev <= 0x4) //disable SYS_OCP in MAX77833 PASS1~3
		max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_15, 0, 1);
	else
		max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_15, 1, 1);
#endif

	/* Watchdog Enable */
	max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
				MAX77833_WDTEN, MAX77833_WDTEN);

	max77833_test_read(charger);
}

#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
static void max77833_set_float_voltage(struct max77833_charger_data *charger, int float_voltage)
{
	u8 reg_data = 0;

	reg_data = max77833_get_float_voltage_data(float_voltage);
	max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_06, reg_data);
	charger->pdata->chg_float_voltage = float_voltage;
	pr_info("%s: battery cv voltage 0x%x, chg_float_voltage = %dmV \n", __func__, reg_data, charger->pdata->chg_float_voltage);
}

static int max77833_get_float_voltage(struct max77833_charger_data *charger)
{
	u8 reg_data = 0;
        unsigned int float_voltage;

	max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_06, &reg_data);

        float_voltage = (reg_data * 10) + 3000;

	pr_info("%s: battery cv voltage 0x%x, chg_float_voltage = %dmV \n", __func__, reg_data, float_voltage);

	return float_voltage;
}

#endif

static int max77833_chg_create_attrs(struct device *dev)
{
	unsigned long i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(max77833_charger_attrs); i++) {
		rc = device_create_file(dev, &max77833_charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &max77833_charger_attrs[i]);
	return rc;
}

ssize_t max77833_chg_show_attrs(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - max77833_charger_attrs;
	int i = 0;

	switch(offset) {
	case CHIP_ID:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", "MAX77833");
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t max77833_chg_store_attrs(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - max77833_charger_attrs;
	int ret = 0;

	switch(offset) {
	case CHIP_ID:
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int max77833_chg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct max77833_charger_data *charger =
		container_of(psy, struct max77833_charger_data, psy_chg);
	u8 reg_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_BATTERY;
		if (max77833_read_reg(charger->i2c,
			MAX77833_CHG_REG_INT_OK, &reg_data) == 0) {
			if (reg_data & MAX77833_WCIN_OK) {
				val->intval = POWER_SUPPLY_TYPE_WIRELESS; // need to check
				charger->wc_w_state = 1;
			} else if (reg_data & MAX77833_CHGIN_OK) {
				val->intval = POWER_SUPPLY_TYPE_MAINS;
			}
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max77833_check_battery(charger);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77833_get_charger_state(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else if (charger->aicl_on)
		{
			val->intval = POWER_SUPPLY_CHARGE_TYPE_SLOW;
			pr_info("%s: slow-charging mode\n", __func__);
		}
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77833_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->charging_current_max;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = max77833_get_input_current(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = max77833_get_input_current(charger);
		pr_debug("%s : set-current(%dmA), current now(%dmA)\n",
			__func__, charger->charging_current, val->intval);
		{
			int iin, v_chgin;
			iin = max77833_get_chg_adc_data(charger, CHG_CNFG_20_ADC_CHG_SEL_IIN);
			v_chgin = max77833_get_chg_adc_data(charger, CHG_CNFG_20_ADC_CHG_SEL_V_CHGIN);
			pr_info("%s: imax(%dmA), iin(%dmA), v_chgin(%dmV)\n", __func__, charger->charging_current_max, iin, v_chgin);
		}

		break;
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = max77833_get_float_voltage(charger);
		break;
#endif
#if defined(CONFIG_AFC_CHARGER_MODE)
	case POWER_SUPPLY_PROP_AFC_CHARGER_MODE:
		return -ENODATA;
#endif
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
			&reg_data);
		if ((reg_data & CHG_CNFG_00_OTG_CTRL) == CHG_CNFG_00_OTG_CTRL)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_USB_HC:
		return -ENODATA;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		max77833_read_reg(charger->i2c,
				  MAX77833_CHG_REG_DTLS_01, &reg_data);
		reg_data &= 0x0F;
		switch (reg_data) {
		case 0x01:
			val->strval = "CC Mode";
			break;
		case 0x02:
			val->strval = "CV Mode";
			break;
		case 0x03:
			val->strval = "EOC";
			break;
		case 0x04:
			val->strval = "DONE";
			break;
		default:
			val->strval = "NONE";
			break;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		if (val->intval == CHG_INIT_DETECTED_I_IN) {
			if (charger->iin_current_detecting)
				val->intval = -1;
			else
				val->intval = charger->detected_iin_current;
		} else
			val->intval = max77833_get_chg_adc_data(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_AICL_CONTROL:
		return -ENODATA;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
		return -ENODATA;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		return -ENODATA;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77833_chg_set_property(struct power_supply *psy,
			  enum power_supply_property psp,
			  const union power_supply_propval *val)
{
	struct max77833_charger_data *charger =
		container_of(psy, struct max77833_charger_data, psy_chg);
	union power_supply_propval value;
	u8 chg_cnfg_00 = 0;
	u8 reg_data;
	static u8 chg_int_state;
	int buck_state = ENABLE;

	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		charger->charge_mode = val->intval;
		switch (charger->charge_mode) {
		case SEC_BAT_CHG_MODE_BUCK_OFF:
			buck_state = DISABLE;
		case SEC_BAT_CHG_MODE_CHARGING_OFF:
			charger->is_charging = false;
			break;
		case SEC_BAT_CHG_MODE_CHARGING:
			charger->is_charging = true;
			break;
		}
		max77833_set_buck(charger, buck_state);
		max77833_set_charger_state(charger, charger->is_charging);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		charger->aicl_on = false;
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
			max77833_update_reg(charger->i2c, MAX77833_CHG_REG_INT_MASK,
					0, MAX77833_AICL_IM);
			max77833_read_reg(charger->i2c,
					MAX77833_CHG_REG_INT_MASK, &reg_data);
			pr_info("%s: AICL UNMASK (0x%02x)---------\n", __func__, reg_data);
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_HV_MAINS ||
				charger->cable_type == POWER_SUPPLY_TYPE_HV_ERR){
			/* AICL OFF */
			max77833_update_reg(charger->i2c, MAX77833_CHG_REG_INT_MASK,
					MAX77833_AICL_IM, MAX77833_AICL_IM);
			max77833_read_reg(charger->i2c,
					MAX77833_CHG_REG_INT_MASK, &reg_data);
			pr_info("%s: AICL MASK (0x%02x)---------\n", __func__, reg_data);
		}
		break;
	/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	{
		int input_current = val->intval;
		max77833_set_input_current(charger, input_current);
		charger->charging_current_max = input_current;
	}
	break;
	/*  val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->charging_current = val->intval;
		max77833_set_charge_current(charger, charger->charging_current_max);
		break;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->charging_current = val->intval;
		max77833_set_charge_current(charger, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CURRENT_FULL:
		max77833_set_topoff_current(charger, val->intval);
		break;
#if defined(CONFIG_AFC_CHARGER_MODE)
	case POWER_SUPPLY_PROP_AFC_CHARGER_MODE:
	//	max77833_hv_muic_charger_init();
		break;
#endif
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("%s: float voltage(%d)\n", __func__, val->intval);
		max77833_set_float_voltage(charger, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_USB_HC:
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		psy_do_property("wireless", get, POWER_SUPPLY_PROP_ONLINE, value);
		if (val->intval && !value.intval) {
			max77833_read_reg(charger->i2c, MAX77833_CHG_REG_INT_MASK,
				&chg_int_state);

			/* OTG_EN set to HIGH */
			if (charger->pdata->otg_en) {
				gpio_direction_output(charger->pdata->otg_en, 1);
				pr_info("%s: OTG_EN set to HIGH. cable(%d)\n", __func__, charger->cable_type);
			}

			/* eable charger interrupt: CHG_I, CHGIN_I */
			/* enable charger interrupt: BYP_I */
			max77833_update_reg(charger->i2c, MAX77833_CHG_REG_INT_MASK,
				0,
				MAX77833_CHG_IM | MAX77833_CHGIN_IM | MAX77833_BYP_IM);

			/* OTG on, boost on */
			max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
				CHG_CNFG_00_OTG_CTRL, CHG_CNFG_00_OTG_CTRL);

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
			value.intval = 1;
			psy_do_property(charger->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
#endif
		} else {
			/* AICL OFF */
			max77833_update_reg(charger->i2c, MAX77833_CHG_REG_INT_MASK,
					MAX77833_AICL_IM, MAX77833_AICL_IM);
			max77833_read_reg(charger->i2c,
					  MAX77833_CHG_REG_INT_MASK, &reg_data);
			pr_info("%s: AICL MASK (0x%02x)---------\n", __func__, reg_data);

			/* OTG_EN set to LOW */
			psy_do_property("battery", get, POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW, value);
			if (charger->pdata->otg_en && value.intval != POWER_SUPPLY_TYPE_USB) {
				gpio_direction_output(charger->pdata->otg_en, 0);
				pr_info("%s: OTG_EN set to LOW. cable(%d)\n", __func__, charger->cable_type);
			}

			/* OTG off, boost off, (buck on) */
			max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
				CHG_CNFG_00_BUCK_MASK, CHG_CNFG_00_BUCK_MASK | CHG_CNFG_00_OTG_CTRL);

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
			value.intval = 0;
			psy_do_property(charger->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
#endif
		}
		max77833_read_reg(charger->i2c, MAX77833_CHG_REG_INT_MASK,
			&chg_int_state);
		max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
			&chg_cnfg_00);
		pr_info("%s: INT_MASK(0x%x), CHG_CNFG_00(0x%x)\n",
			__func__, chg_int_state, chg_cnfg_00);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
		if (val->intval) {
			max77833_check_wpc(charger);
		}
		break;
#else
		return -EINVAL;
#endif
	case POWER_SUPPLY_PROP_CHARGE_AICL_CONTROL:
		if (val->intval) {
			max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_11, charger->aicl_threshold);
			pr_info("%s: CHGIN AICL ENABLE\n", __func__);
		} else {
			if (!charger->aicl_threshold) {
				max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_11, &charger->aicl_threshold);
			}
			max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_11, 0x00);
			pr_info("%s: CHGIN AICL DISABLE\n", __func__);
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
		if (val->intval) {
			if (charger->pdata->ovp_enb) {
				gpio_direction_output(charger->pdata->ovp_enb, 1);
				pr_info("%s: OVP_ENB set to HIGH. cable(%d)\n", __func__, charger->cable_type);
			} else {
				max77833_write_reg(charger->i2c, MAX77833_CNFG_GPIO_0, MAX77833_CNFG_GPIO_0_HIGH_VAL);
				pr_info("%s: OVP_ENB set to HIGH. cable(%d)\n", __func__, charger->cable_type);
			}
		} else {
			if (charger->pdata->ovp_enb) {
				gpio_direction_output(charger->pdata->ovp_enb, 0);
				pr_info("%s: OVP_ENB set to LOW. cable(%d)\n", __func__, charger->cable_type);
			} else {
				max77833_write_reg(charger->i2c, MAX77833_CNFG_GPIO_0, MAX77833_CNFG_GPIO_0_RESET_VAL);
				pr_info("%s: OVP_ENB set to LOW. cable(%d)\n", __func__, charger->cable_type);
			}
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		charger->store_mode = val->intval;
		max77833_set_input_current(charger, charger->charging_current_max);
		pr_info("%s : STORE MODE(%d)\n", __func__, charger->store_mode);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77833_otg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct max77833_charger_data *charger =
		container_of(psy, struct max77833_charger_data, psy_otg);
	u8 reg_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_00,
			&reg_data);
		if ((reg_data & CHG_CNFG_00_OTG_CTRL) == CHG_CNFG_00_OTG_CTRL)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77833_otg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	union power_supply_propval value;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		value.intval = val->intval;
		pr_info("%s: OTG %s\n", __func__, value.intval > 0 ? "on" : "off");
		psy_do_property("max77833-charger", set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#if 0
static int max77833_debugfs_show(struct seq_file *s, void *data)
{
	struct max77833_charger_data *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "MAX77833 CHARGER IC :\n");
	seq_printf(s, "===================\n");
	for (reg = 0x80; reg <= 0x9D; reg++) {
		max77833_read_reg(charger->i2c, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
	return 0;
}

static int max77833_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77833_debugfs_show, inode->i_private);
}

static ssize_t max77833_debugfs_write(struct file *file, const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct max77833_charger_data *charger = s->private;

	char _buf[12], *buf = _buf;
	unsigned long reg, value;
	int ret;

	if (count > sizeof(_buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, count))
		return -EFAULT;

	buf[sizeof(_buf) - 1] = '\0';
	pr_err("%s: %s\n", __func__, buf);
	buf[4] = 0;
	buf[9] = 0;
	ret = kstrtoul(buf, 0, &reg);
	if (ret) {
		pr_err("%s: reg read err %d\n", __func__, ret);
		return ret;
	}

	ret = kstrtoul(buf+5, 0, &value);
	if (ret) {
		pr_err("%s: value read err %d\n", __func__, ret);
		return ret;
	}
	max77833_write_reg(charger->i2c,
		(u8)reg, (u8)value);
	pr_err("%s: reg: 0x%x, value: 0x%x\n", __func__, (int)reg, (int)value);

	return count;
}

static const struct file_operations max77833_debugfs_fops = {
	.open           = max77833_debugfs_open,
	.read           = seq_read,
	.write		= max77833_debugfs_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};
#endif

static void max77833_chg_isr_work(struct work_struct *work)
{
	struct max77833_charger_data *charger =
		container_of(work, struct max77833_charger_data, isr_work.work);

	union power_supply_propval val;

	if (charger->pdata->full_check_type ==
	    SEC_BATTERY_FULLCHARGED_CHGINT) {

		val.intval = max77833_get_charger_state(charger);

		switch (val.intval) {
		case POWER_SUPPLY_STATUS_DISCHARGING:
			pr_err("%s: Interrupted but Discharging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_NOT_CHARGING:
			pr_err("%s: Interrupted but NOT Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_FULL:
			pr_info("%s: Interrupted by Full\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_STATUS, val);
			break;

		case POWER_SUPPLY_STATUS_CHARGING:
			pr_err("%s: Interrupted but Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Status\n", __func__);
			break;
		}
	}

	if (charger->pdata->ovp_uvlo_check_type ==
		SEC_BATTERY_OVP_UVLO_CHGINT) {
		val.intval = max77833_get_charging_health(charger);
		switch (val.intval) {
		case POWER_SUPPLY_HEALTH_OVERHEAT:
		case POWER_SUPPLY_HEALTH_COLD:
			pr_err("%s: Interrupted but Hot/Cold\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_DEAD:
			pr_err("%s: Interrupted but Dead\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
			pr_info("%s: Interrupted by OVP/UVLO\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, val);
			break;

		case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
			pr_err("%s: Interrupted but Unspec\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_GOOD:
			pr_err("%s: Interrupted but Good\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Health\n", __func__);
			break;
		}
	}
}

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
static irqreturn_t max77833_chg_irq_thread(int irq, void *irq_data)
{
	struct max77833_charger_data *charger = irq_data;
	unsigned long delay;
	union power_supply_propval value;

	pr_info("%s: Charger interrupt occured \n", __func__);

	value.intval = 1;
	psy_do_property(charger->pdata->wireless_charger_name, set,
		POWER_SUPPLY_PROP_STATUS, value);

#ifdef CONFIG_SAMSUNG_BATTERY_FACTORY
		delay = msecs_to_jiffies(0);
#else
		if (charger->wc_w_state)
			delay = msecs_to_jiffies(500);
		else
			delay = msecs_to_jiffies(500);
#endif
		queue_delayed_work(charger->wqueue, &charger->wpc_work,
				delay);

	return IRQ_HANDLED;
}
#else
static irqreturn_t max77833_chg_irq_thread(int irq, void *irq_data)
{
	struct max77833_charger_data *charger = irq_data;

	pr_info("%s: Charger interrupt occured\n", __func__);

	if ((charger->pdata->full_check_type ==
	     SEC_BATTERY_FULLCHARGED_CHGINT) ||
	    (charger->pdata->ovp_uvlo_check_type ==
	     SEC_BATTERY_OVP_UVLO_CHGINT))
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
static void wpc_detect_work(struct work_struct *work)
{
	struct max77833_charger_data *charger = container_of(work,
						struct max77833_charger_data,
						wpc_work.work);
	int wc_w_state;
	//int retry_cnt;
	union power_supply_propval value;

	pr_info("%s\n", __func__);

	/* check and unlock */
	check_charger_unlock_state(charger);
#if defined(CONFIG_WIRELESS_CHARGER_P9220)
	wc_w_state = !gpio_get_value(charger->pdata->irq_gpio);
#else
	wc_w_state = gpio_get_value(charger->pdata->wpc_det);
#endif
	pr_info("%s wc_w_state = %d \n", __func__, wc_w_state);

	if ((charger->wc_w_state == 0) && (wc_w_state == 1)) {
		value.intval = 1;
		psy_do_property("wireless", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		value.intval = POWER_SUPPLY_TYPE_WIRELESS;

		psy_do_property(charger->pdata->wireless_charger_name, set,
			POWER_SUPPLY_PROP_ONLINE, value);

		pr_info("%s: wpc activated, set V_INT as PN\n",
				__func__);
	} else if ((charger->wc_w_state == 1) && (wc_w_state == 0)) {
		if (!charger->is_charging)
			max77833_set_charger_state(charger, true);

		if (!charger->is_charging)
			max77833_set_charger_state(charger, false);

			value.intval = 0;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
			pr_info("%s: wpc deactivated, set V_INT as PD\n",
					__func__);
	}

	pr_info("%s: w(%d to %d)\n", __func__,
		charger->wc_w_state, wc_w_state);

	charger->wc_w_state = wc_w_state;

	wake_unlock(&charger->wpc_wake_lock);
}
#else
static void wpc_detect_work(struct work_struct *work)
{
	struct max77833_charger_data *charger = container_of(work,
						struct max77833_charger_data,
						wpc_work.work);
	int wc_w_state;
	int retry_cnt;
	union power_supply_propval value;
	u8 reg_data;

	pr_info("%s\n", __func__);

	max77833_update_reg(charger->i2c,
		MAX77833_CHG_REG_INT_MASK, 0, MAX77833_WCIN_IM);

	/* check and unlock */
	check_charger_unlock_state(charger);

	retry_cnt = 0;
	do {
		max77833_read_reg(charger->i2c,
				  MAX77833_CHG_REG_INT_OK, &reg_data);
		wc_w_state = (reg_data & MAX77833_WCIN_OK)
			>> MAX77833_WCIN_OK_SHIFT;
		msleep(50);
	} while((retry_cnt++ < 2) && (wc_w_state == 0));

	if ((charger->wc_w_state == 0) && (wc_w_state == 1)) {
		value.intval = 1;
		psy_do_property("wireless", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		value.intval = POWER_SUPPLY_TYPE_WIRELESS;
		pr_info("%s: wpc activated, set V_INT as PN\n",
				__func__);
	} else if ((charger->wc_w_state == 1) && (wc_w_state == 0)) {
		if (!charger->is_charging)
			max77833_set_charger_state(charger, true);

		retry_cnt = 0;
		do {
			max77833_read_reg(charger->i2c,
					  MAX77833_CHG_REG_DTLS_01, &reg_data);
			reg_data = ((reg_data & MAX77833_CHG_DTLS)
				    >> MAX77833_CHG_DTLS_SHIFT);
			msleep(50);
		} while((retry_cnt++ < 2) && (reg_data == 0x8));
		pr_info("%s: reg_data: 0x%x, charging: %d\n", __func__,
			reg_data, charger->is_charging);
		if (!charger->is_charging)
			max77833_set_charger_state(charger, false);
		if ((reg_data != 0x08)
		    && (charger->cable_type == POWER_SUPPLY_TYPE_WIRELESS)) {
			pr_info("%s: wpc uvlo, but charging\n", __func__);
			queue_delayed_work(charger->wqueue, &charger->wpc_work,
					   msecs_to_jiffies(500));
			return;
		} else {
			value.intval = 0;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
			pr_info("%s: wpc deactivated, set V_INT as PD\n",
					__func__);
		}
	}
	pr_info("%s: w(%d to %d)\n", __func__,
		charger->wc_w_state, wc_w_state);

	charger->wc_w_state = wc_w_state;

	/* Do unmask again. (for frequent wcin irq problem) */
	max77833_update_reg(charger->i2c,
		MAX77833_CHG_REG_INT_MASK, 0, MAX77833_WCIN_IM);

	wake_unlock(&charger->wpc_wake_lock);
}
#endif

static irqreturn_t wpc_charger_irq(int irq, void *data)
{
	struct max77833_charger_data *charger = data;
	unsigned long delay;

	max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK,
			MAX77833_WCIN_IM, MAX77833_WCIN_IM);

	wake_lock(&charger->wpc_wake_lock);
#ifdef CONFIG_SAMSUNG_BATTERY_FACTORY
	delay = msecs_to_jiffies(0);
#else
	if (charger->wc_w_state)
		delay = msecs_to_jiffies(500);
	else
		delay = msecs_to_jiffies(0);
#endif
	queue_delayed_work(charger->wqueue, &charger->wpc_work,
			delay);
	return IRQ_HANDLED;
}

static irqreturn_t max77833_batp_irq(int irq, void *data)
{
	struct max77833_charger_data *charger = data;
	union power_supply_propval value;
	u8 reg_data;

	pr_info("%s : irq(%d)\n", __func__, irq);

	max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK,
			MAX77833_BATP_IM, MAX77833_BATP_IM);

	check_charger_unlock_state(charger);

	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_INT_OK,
			  &reg_data);

	if (!(reg_data & MAX77833_BATP_OK))
		psy_do_property("battery", set, POWER_SUPPLY_PROP_PRESENT, value);

	max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK,
			0, MAX77833_BATP_IM);

	return IRQ_HANDLED;
}


static irqreturn_t max77833_bypass_irq(int irq, void *data)
{
	struct max77833_charger_data *charger = data;
	u8 dtls_02;
	u8 byp_dtls;
	u8 vbus_state;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct otg_notify *o_notify;

	o_notify = get_otg_notify();
#endif

	pr_info("%s: irq(%d)\n", __func__, irq);

	/* check and unlock */
	check_charger_unlock_state(charger);

	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_DTLS_02,
			  &dtls_02);

	byp_dtls = ((dtls_02 & MAX77833_BYP_DTLS) >>
				MAX77833_BYP_DTLS_SHIFT);
	pr_info("%s: BYP_DTLS(0x%02x)\n", __func__, byp_dtls);
	vbus_state = max77833_get_vbus_state(charger);

	if (byp_dtls & 0x1) {
		pr_info("%s: bypass overcurrent limit\n", __func__);
#ifdef CONFIG_USB_HOST_NOTIFY
		send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 0);
#endif
		/* disable the register values just related to OTG and
		   keep the values about the charging */
	max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_CNFG_00,
			0, CHG_CNFG_00_OTG_MASK | CHG_CNFG_00_BOOST_MASK);

	}
	return IRQ_HANDLED;
}

static void reduce_input_current(struct max77833_charger_data *charger, int cur)
{
	u8 set_value;
	unsigned int min_input_current = 0;

	min_input_current = MINIMUM_INPUT_CURRENT;

	if (!max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_16, &set_value)) {
		if ((set_value <= (min_input_current / charger->input_curr_limit_step)) ||
		    (set_value <= (cur / charger->input_curr_limit_step)))
			return;
		set_value -= (cur / charger->input_curr_limit_step);
		set_value = (set_value < (min_input_current / charger->input_curr_limit_step)) ?
			(min_input_current / charger->input_curr_limit_step) : set_value;
		max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_16, set_value);
		charger->charging_current_max = max77833_get_input_current(charger);
		pr_info("%s: set current: reg:(0x%x), val:(0x%x), input_current(%d)\n",
			__func__, MAX77833_CHG_REG_CNFG_16, set_value, charger->charging_current_max);
	}
}

static void max77833_aicl_work(struct work_struct *work)
{
	struct max77833_charger_data *charger = container_of(work,
				     struct max77833_charger_data, aicl_work.work);
	int iin = max77833_get_chg_adc_data(charger, CHG_CNFG_20_ADC_CHG_SEL_IIN);
	u8 reg_data;
	pr_info("%s: [%d] iin:%d\n", __func__, charger->is_charging, iin);

	if ((charger->is_charging) &&
		(charger->cable_type != POWER_SUPPLY_TYPE_WIRELESS &&
			charger->cable_type != POWER_SUPPLY_TYPE_HV_WIRELESS &&
			charger->cable_type != POWER_SUPPLY_TYPE_PMA_WIRELESS )) {
		int now_count = 0,
			max_count = charger->charging_current_max / REDUCE_CURRENT_STEP;
		int prev_current_max = charger->charging_current_max;

		mutex_lock(&charger->charger_mutex);
		check_charger_unlock_state(charger);

		if (max77833_check_aicl_state(charger) && (iin <= 160)) {
			pr_info("%s: disable aicl!\n", __func__);
			max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_11, &reg_data);
			max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_11, 0x00);
			msleep(200);
			max77833_write_reg(charger->i2c, MAX77833_CHG_REG_CNFG_11, reg_data);
			pr_info("%s: enable aicl (0x%02x)!\n", __func__, reg_data);
		}

		while (max77833_check_aicl_state(charger) &&
			(now_count++ < max_count) && (charger->is_charging) &&
			(charger->cable_type != POWER_SUPPLY_TYPE_WIRELESS &&
			 charger->cable_type != POWER_SUPPLY_TYPE_HV_WIRELESS &&
			 charger->cable_type != POWER_SUPPLY_TYPE_PMA_WIRELESS )) {
			reduce_input_current(charger, REDUCE_CURRENT_STEP);
			msleep(50);
		}

		if (prev_current_max > charger->charging_current_max) {
			charger->afc_detect = false;
			pr_info("%s: charging_current_max(%d --> %d)\n",
				__func__, prev_current_max, charger->charging_current_max);
			max77833_check_slow_charging(charger, charger->charging_current_max);
		}
		mutex_unlock(&charger->charger_mutex);
	}

	max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK,
			0, MAX77833_AICL_IM);
	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_INT_MASK, &reg_data);
	pr_info("%s: AICL UNMASK (0x%02x)---------\n", __func__, reg_data);

	wake_unlock(&charger->aicl_wake_lock);
}

static irqreturn_t max77833_aicl_irq(int irq, void *data)
{
	struct max77833_charger_data *charger = data;
	u8 reg_data;

	pr_info("%s: irq(%d)\n", __func__, irq);
	if (!charger->max77833->pmic_rev_pass5) {
		return IRQ_HANDLED;
	}

	wake_lock(&charger->aicl_wake_lock);

	max77833_update_reg(charger->i2c, MAX77833_CHG_REG_INT_MASK,
			MAX77833_AICL_IM, MAX77833_AICL_IM);
	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_INT_MASK, &reg_data);
	pr_info("%s: AICL MASK (0x%02x)---------\n", __func__, reg_data);

	queue_delayed_work(charger->wqueue, &charger->aicl_work,
		msecs_to_jiffies(500));

	return IRQ_HANDLED;
}

static void max77833_chgin_isr_work(struct work_struct *work)
{
	struct max77833_charger_data *charger = container_of(work,
				     struct max77833_charger_data, chgin_work);
	u8 chgin_dtls, chg_dtls, chg_cnfg_00;
	u8 prev_chgin_dtls = 0xff;
	int battery_health;
	union power_supply_propval value;
	int stable_count = 0;

	wake_lock(&charger->chgin_wake_lock);

	max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK,
			MAX77833_CHGIN_IM, MAX77833_CHGIN_IM);

	while (1) {
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_HEALTH, value);
		battery_health = value.intval;

		max77833_read_reg(charger->i2c,
				MAX77833_CHG_REG_DTLS_00,
				&chgin_dtls);
		chgin_dtls = ((chgin_dtls & MAX77833_CHGIN_DTLS) >>
				MAX77833_CHGIN_DTLS_SHIFT);
		max77833_read_reg(charger->i2c,
				MAX77833_CHG_REG_DTLS_01, &chg_dtls);
		chg_dtls = ((chg_dtls & MAX77833_CHG_DTLS) >>
				MAX77833_CHG_DTLS_SHIFT);
		max77833_read_reg(charger->i2c,
			MAX77833_CHG_REG_CNFG_00, &chg_cnfg_00);

		if (prev_chgin_dtls == chgin_dtls)
			stable_count++;
		else
			stable_count = 0;
		if (stable_count > 10) {
#if defined(CONFIG_VBUS_NOTIFIER)
			switch (chgin_dtls) {
					case 0x03:
							vbus_notifier_handle(STATUS_VBUS_HIGH);
							break;
					case 0x00:
					case 0x01:
					case 0x02:
							vbus_notifier_handle(STATUS_VBUS_LOW);
							break;
					default:
							pr_err("%s: unknown chgin_dtls(0x%x)\n", __func__, chgin_dtls);
							vbus_notifier_handle(STATUS_VBUS_UNKNOWN);
							break;
				}
#endif
			pr_info("%s: irq(%d), chgin(0x%x), chg_dtls(0x%x) prev 0x%x\n",
					__func__, charger->irq_chgin,
					chgin_dtls, chg_dtls, prev_chgin_dtls);
			if (charger->is_charging) {
				if ((chgin_dtls == 0x02) && \
					(battery_health != POWER_SUPPLY_HEALTH_OVERVOLTAGE)) {
					pr_info("%s: charger is over voltage\n",
							__func__);
					value.intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
					psy_do_property("battery", set,
						POWER_SUPPLY_PROP_HEALTH, value);
				} else if (((chgin_dtls == 0x0) || (chgin_dtls == 0x01)) &&(chg_dtls & 0x08) && \
						(chg_cnfg_00 & MAX77833_MODE_BUCK) && \
						(chg_cnfg_00 & MAX77833_MODE_CHGR) && \
						(battery_health != POWER_SUPPLY_HEALTH_UNDERVOLTAGE) && \
						(charger->cable_type != POWER_SUPPLY_TYPE_WIRELESS) && \
						(charger->cable_type != POWER_SUPPLY_TYPE_HV_WIRELESS) && \
						(charger->cable_type != POWER_SUPPLY_TYPE_PMA_WIRELESS)) {
					pr_info("%s, vbus_state : 0x%d, chg_state : 0x%d\n", __func__, chgin_dtls, chg_dtls);
					pr_info("%s: vBus is undervoltage\n", __func__);
					value.intval = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
					psy_do_property("battery", set,
							POWER_SUPPLY_PROP_HEALTH, value);
				}
			} else {
				if ((battery_health == \
							POWER_SUPPLY_HEALTH_OVERVOLTAGE) &&
						(chgin_dtls != 0x02)) {
					pr_info("%s: vbus_state : 0x%d, chg_state : 0x%d\n", __func__, chgin_dtls, chg_dtls);
					pr_info("%s: overvoltage->normal\n", __func__);
					value.intval = POWER_SUPPLY_HEALTH_GOOD;
					psy_do_property("battery", set,
							POWER_SUPPLY_PROP_HEALTH, value);
				} else if ((battery_health == \
							POWER_SUPPLY_HEALTH_UNDERVOLTAGE) &&
						!((chgin_dtls == 0x0) || (chgin_dtls == 0x01))){
					pr_info("%s: vbus_state : 0x%d, chg_state : 0x%d\n", __func__, chgin_dtls, chg_dtls);
					pr_info("%s: undervoltage->normal\n", __func__);
					value.intval = POWER_SUPPLY_HEALTH_GOOD;
					psy_do_property("battery", set,
							POWER_SUPPLY_PROP_HEALTH, value);
					max77833_set_input_current(charger,
							charger->charging_current_max);
				}
			}
			break;
		}

		prev_chgin_dtls = chgin_dtls;
		msleep(100);
	}

	max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK,
			0, MAX77833_CHGIN_IM);

	wake_unlock(&charger->chgin_wake_lock);
}

static irqreturn_t max77833_chgin_irq(int irq, void *data)
{
	struct max77833_charger_data *charger = data;
	queue_work(charger->wqueue, &charger->chgin_work);

	return IRQ_HANDLED;
}

/* register chgin isr after sec_battery_probe */
static void max77833_chgin_init_work(struct work_struct *work)
{
	struct max77833_charger_data *charger = container_of(work,
						struct max77833_charger_data,
						chgin_init_work.work);
	int ret;

	pr_info("%s \n", __func__);
	ret = request_threaded_irq(charger->irq_chgin, NULL,
			max77833_chgin_irq, 0, "chgin-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request chgin IRQ: %d: %d\n",
				__func__, charger->irq_chgin, ret);
	} else {
		max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK, 0, MAX77833_CHGIN_IM);
	}
}

#ifdef CONFIG_OF
static int max77833_charger_parse_dt(struct max77833_charger_data *charger)
{
	struct device_node *np = of_find_node_by_name(NULL, "max77833-charger");
	sec_charger_platform_data_t *pdata = charger->pdata;
	int ret = 0;
	int i, len;
	const u32 *p;
	//u32 irq_gpio_flags;

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
					   &pdata->chg_float_voltage);
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {

		ret = of_property_read_string(np,
			"battery,wirelss_charger_name", (char const **)&pdata->wireless_charger_name);
		if (ret)
			pr_info("%s: wireless charger name is Empty\n", __func__);

#if 0
		ret = pdata->irq_gpio = of_get_named_gpio_flags(np, "battery,irq_gpio",
				0, &irq_gpio_flags);
		if (ret < 0)
			pr_err("%s : can't get irq-gpio \n", __FUNCTION__);
		else {
			pr_info("%s : irq_gpio = %d \n",__func__, pdata->irq_gpio);
			pdata->chg_irq = gpio_to_irq(pdata->irq_gpio);
			pr_info("%s : chg_irq = 0x%x \n",__func__, pdata->chg_irq);
		}
#endif

#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
		ret = of_property_read_u32(np, "battery,wpc_charging_limit_current",
				   &pdata->wpc_charging_limit_current);
		if (ret)
			pr_info("%s : wpc_charging_limit_current is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,sleep_mode_limit_current",
				   &pdata->sleep_mode_limit_current);
		if (ret)
			pr_info("%s : sleep_mode_limit_current is Empty\n", __func__);		
#endif
		pdata->wpc_det = of_get_named_gpio(np, "battery,wpc_det", 0);
		if (pdata->wpc_det < 0)
			pdata->wpc_det = 0;

		pdata->otg_en = of_get_named_gpio(np, "battery,otg_en", 0);
		if (pdata->otg_en < 0)
			pdata->otg_en = 0;

		pdata->ovp_enb = of_get_named_gpio(np, "battery,ovp_enb", 0);
		if (pdata->ovp_enb < 0)
			pdata->ovp_enb = 0;

		ret = of_property_read_u32(np, "battery,wireless_cc_cv",
					   &pdata->wireless_cc_cv);

		ret = of_property_read_u32(np, "battery,full_check_type_2nd",
					&pdata->full_check_type_2nd);
		if (ret)
			pr_info("%s : Full check type 2nd is Empty\n", __func__);

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->charging_current = kzalloc(sizeof(sec_charging_current_t) * len,
						  GFP_KERNEL);

		for(i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				 "battery,input_current_limit", i,
				 &pdata->charging_current[i].input_current_limit);
			ret = of_property_read_u32_index(np,
				 "battery,fast_charging_current", i,
				 &pdata->charging_current[i].fast_charging_current);
			ret = of_property_read_u32_index(np,
				 "battery,full_check_current_1st", i,
				 &pdata->charging_current[i].full_check_current_1st);
			ret = of_property_read_u32_index(np,
				 "battery,full_check_current_2nd", i,
				 &pdata->charging_current[i].full_check_current_2nd);
		}
	}
	return ret;
}
#endif

static int __devinit max77833_charger_probe(struct platform_device *pdev)
{
	struct max77833_dev *max77833 = dev_get_drvdata(pdev->dev.parent);
	struct max77833_platform_data *pdata = dev_get_platdata(max77833->dev);
	struct max77833_charger_data *charger;
	sec_charger_platform_data_t *charger_data;
	int ret = 0;
	u8 reg_data;

	pr_info("%s: Max77833 Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger_data = kzalloc(sizeof(sec_charger_platform_data_t), GFP_KERNEL);
	if (!charger_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&charger->charger_mutex);
	charger->dev = &pdev->dev;
	charger->max77833 = max77833;
	charger->i2c = max77833->i2c;
	charger->pdata = charger_data;
	charger->aicl_on = false;
	charger->afc_detect = false;
	charger->wc_afc_detect = false;
	charger->is_mdock = false;
	charger->store_mode = false;
	charger->charging_current_max = 500;
	charger->max77833_pdata = pdata;
	charger->input_curr_limit_step = 25;
	charger->detected_iin_current = 500;
	charger->iin_current_detecting = false;

#if defined(CONFIG_OF)
	ret = max77833_charger_parse_dt(charger);
	if (ret < 0) {
		pr_err("%s not found charger dt! ret[%d]\n",
		       __func__, ret);
	}
#endif

	platform_set_drvdata(pdev, charger);

	charger->psy_chg.name		= "max77833-charger";
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= max77833_chg_get_property;
	charger->psy_chg.set_property	= max77833_chg_set_property;
	charger->psy_chg.properties	= max77833_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(max77833_charger_props);
	charger->psy_otg.name		= "otg";
	charger->psy_otg.type		= POWER_SUPPLY_TYPE_OTG;
	charger->psy_otg.get_property	= max77833_otg_get_property;
	charger->psy_otg.set_property	= max77833_otg_set_property;
	charger->psy_otg.properties		= max77833_otg_props;
	charger->psy_otg.num_properties	= ARRAY_SIZE(max77833_otg_props);

	max77833_charger_initialize(charger);

	if (charger->pdata->otg_en) {
		ret = gpio_request(charger->pdata->otg_en, "OTG_EN");
		if (ret) {
			pr_err("failed to request GPIO %u\n", charger->pdata->otg_en);
			goto err_gpio;
		}
	}

	if (charger->pdata->ovp_enb) {
		ret = gpio_request(charger->pdata->ovp_enb, "OVP_ENB");
		if (ret) {
			pr_err("failed to request GPIO %u\n", charger->pdata->ovp_enb);
			goto err_gpio;
		}
	}

	if (max77833_read_reg(max77833->i2c, MAX77833_PMIC_REG_PMICREV, &reg_data) < 0) {
		pr_err("device not found on this channel (this is not an error)\n");
		ret = -ENOMEM;
		goto err_pdata_free;
	} else {
		charger->pmic_ver = (reg_data & 0x7);
		pr_info("%s : device found : ver.0x%x\n", __func__, charger->pmic_ver);
	}

#if 0
	(void) debugfs_create_file("max77833-regs",
		S_IRUGO, NULL, (void *)charger, &max77833_debugfs_fops);
#endif

	charger->wqueue =
	    create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}
	wake_lock_init(&charger->chgin_wake_lock, WAKE_LOCK_SUSPEND,
		       "charger->chgin");
	INIT_WORK(&charger->chgin_work, max77833_chgin_isr_work);
	INIT_DELAYED_WORK(&charger->chgin_init_work, max77833_chgin_init_work);

	INIT_DELAYED_WORK(&charger->wpc_work, wpc_detect_work);

	wake_lock_init(&charger->aicl_wake_lock, WAKE_LOCK_SUSPEND,
					       "charger-aicl");
	INIT_DELAYED_WORK(&charger->aicl_work, max77833_aicl_work);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_power_supply_register;
	}
	ret = power_supply_register(&pdev->dev, &charger->psy_otg);
	if (ret) {
		pr_err("%s: Failed to Register otg_chg\n", __func__);
		goto err_power_supply_register_otg;
	}

	if (0) {//(charger->pdata->chg_irq) {
		INIT_DELAYED_WORK(&charger->isr_work, max77833_chg_isr_work);

		ret = request_threaded_irq(charger->pdata->chg_irq,
				NULL, max77833_chg_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT,
				"charger-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto err_irq;
		}

			ret = enable_irq_wake(charger->pdata->chg_irq);
			if (ret < 0)
				pr_err("%s: Failed to Enable Wakeup Source(%d)\n",
					__func__, ret);
	}

	charger->wc_w_irq = pdata->irq_base + MAX77833_CHG_IRQ_WCIN_I;
	ret = request_threaded_irq(charger->wc_w_irq,
				   NULL, wpc_charger_irq,
				   IRQF_TRIGGER_FALLING,
				   "wpc-int", charger);
	if (ret) {
		pr_err("%s: Failed to Request IRQ\n", __func__);
		goto err_wc_irq;
	}

	max77833_read_reg(charger->i2c,
			  MAX77833_CHG_REG_INT_OK, &reg_data);
	charger->wc_w_state = (reg_data & MAX77833_WCIN_OK)
		>> MAX77833_WCIN_OK_SHIFT;


	max77833_read_reg(charger->i2c, MAX77833_CHG_REG_CNFG_11, &reg_data);
	charger->aicl_threshold = reg_data;

	charger->irq_chgin = pdata->irq_base + MAX77833_CHG_IRQ_CHGIN_I;
	/* enable chgin irq after sec_battery_probe */
	queue_delayed_work(charger->wqueue, &charger->chgin_init_work,
			msecs_to_jiffies(3000));

	charger->irq_bypass = pdata->irq_base + MAX77833_CHG_IRQ_BYP_I;
	ret = request_threaded_irq(charger->irq_bypass, NULL,
			max77833_bypass_irq, 0, "bypass-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request bypass IRQ: %d: %d\n",
				__func__, charger->irq_bypass, ret);
	} else {
		max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK, 0, MAX77833_BYP_IM);
	}

	charger->irq_batp = pdata->irq_base + MAX77833_CHG_IRQ_BATP_I;
	ret = request_threaded_irq(charger->irq_batp, NULL,
				   max77833_batp_irq, 0,
				   "batp-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request batp IRQ: %d: %d\n",
		       __func__, charger->irq_batp, ret);
	} else {
		max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK, 0, MAX77833_BATP_IM);
	}

	charger->irq_aicl = pdata->irq_base + MAX77833_CHG_IRQ_AICL_I;
	ret = request_threaded_irq(charger->irq_aicl, NULL,
			max77833_aicl_irq, 0, "aicl-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request aicl IRQ: %d: %d\n",
				__func__, charger->irq_aicl, ret);
	} else {
		max77833_update_reg(charger->i2c,
			MAX77833_CHG_REG_INT_MASK, MAX77833_AICL_IM, MAX77833_AICL_IM);
		max77833_read_reg(charger->i2c,
				  MAX77833_CHG_REG_INT_MASK, &reg_data);
		pr_info("%s: AICL MASK (0x%02x)---------\n", __func__, reg_data);
	}

	ret = max77833_chg_create_attrs(charger->psy_chg.dev);
	if (ret) {
		dev_err(charger->dev,
			"%s : Failed to create_attrs\n", __func__);
		goto err_wc_irq;
	}

	/* watchdog kick */
	max77833_update_reg(charger->i2c, MAX77833_CHG_REG_CNFG_12,
			   MAX77833_WDTCLR, MAX77833_WDTCLR);

	pr_info("%s: MAX77833 Charger Driver Loaded\n", __func__);

	return 0;

err_wc_irq:
	if (charger->pdata->chg_irq)
		free_irq(charger->pdata->chg_irq, charger);
err_irq:
	power_supply_unregister(&charger->psy_otg);
err_power_supply_register_otg:
	power_supply_unregister(&charger->psy_chg);
err_power_supply_register:
	destroy_workqueue(charger->wqueue);
err_pdata_free:
	if (charger->pdata->otg_en)
			gpio_free(charger->pdata->otg_en);
	if (charger->pdata->ovp_enb)
			gpio_free(charger->pdata->ovp_enb);
err_gpio:
	kfree(charger_data);
	mutex_destroy(&charger->charger_mutex);
err_free:
	kfree(charger);

	return ret;
}

static int __devexit max77833_charger_remove(struct platform_device *pdev)
{
	struct max77833_charger_data *charger =
		platform_get_drvdata(pdev);

	destroy_workqueue(charger->wqueue);
	free_irq(charger->wc_w_irq, charger);
	free_irq(charger->irq_bypass, charger);
	free_irq(charger->irq_batp, charger);
	free_irq(charger->irq_aicl, charger);
	if (charger->pdata->chg_irq)
		free_irq(charger->pdata->chg_irq, charger);
	power_supply_unregister(&charger->psy_chg);
	mutex_destroy(&charger->charger_mutex);
	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int max77833_charger_suspend(struct device *dev)
{
	return 0;
}

static int max77833_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define max77833_charger_suspend NULL
#define max77833_charger_resume NULL
#endif

static void max77833_charger_shutdown(struct device *dev)
{
	struct max77833_charger_data *charger =
				dev_get_drvdata(dev);
	u8 reg_data;

	pr_info("%s: MAX77833 Charger driver shutdown\n", __func__);
	if (!charger->i2c) {
		pr_err("%s: no max77833 i2c client\n", __func__);
		return;
	}

	cancel_delayed_work(&charger->check_slow_work);

	if (charger->pdata->ovp_enb)
		gpio_direction_output(charger->pdata->ovp_enb, 0);
	else
		max77833_write_reg(charger->i2c, MAX77833_CNFG_GPIO_0, MAX77833_CNFG_GPIO_0_RESET_VAL);

	reg_data = 0x05;
	max77833_write_reg(charger->i2c,
		MAX77833_CHG_REG_CNFG_00, reg_data);
	reg_data = 0x14;
	max77833_write_reg(charger->i2c,
		MAX77833_CHG_REG_CNFG_16, reg_data);
	reg_data = 0x14;
	max77833_write_reg(charger->i2c,
		MAX77833_CHG_REG_CNFG_17, reg_data);
	reg_data = 0xE7;
	max77833_write_reg(charger->i2c,
		MAX77833_CHG_REG_CNFG_18, reg_data);
	reg_data = 0x00;
	max77833_write_reg(charger->i2c,
		MAX77833_CHG_REG_CNFG_07, reg_data);

	free_irq(charger->wc_w_irq, charger);
	free_irq(charger->irq_bypass, charger);
	free_irq(charger->irq_batp, charger);
	free_irq(charger->irq_aicl, charger);
	if (charger->pdata->chg_irq)
		free_irq(charger->pdata->chg_irq, charger);

	pr_info("func:%s \n", __func__);
}

static SIMPLE_DEV_PM_OPS(max77833_charger_pm_ops, max77833_charger_suspend,
			 max77833_charger_resume);

static struct platform_driver max77833_charger_driver = {
	.driver = {
		.name = "max77833-charger",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &max77833_charger_pm_ops,
#endif
		.shutdown = max77833_charger_shutdown,
	},
	.probe = max77833_charger_probe,
	.remove = __devexit_p(max77833_charger_remove),
};

static int __init max77833_charger_init(void)
{
	pr_info("%s : \n", __func__);
	return platform_driver_register(&max77833_charger_driver);
}

static void __exit max77833_charger_exit(void)
{
	platform_driver_unregister(&max77833_charger_driver);
}

module_init(max77833_charger_init);
module_exit(max77833_charger_exit);

MODULE_DESCRIPTION("Samsung MAX77833 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
