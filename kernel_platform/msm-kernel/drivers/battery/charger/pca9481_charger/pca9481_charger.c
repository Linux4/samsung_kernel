/*
 * Driver for the NXP PCA9481 battery charger.
 *
 * Copyright (C) 2021 NXP Semiconductor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/debugfs.h>
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <linux/of_gpio.h>
#include "pca9481_charger.h"
#include "../../common/sec_charging_common.h"
#include "../../common/sec_direct_charger.h"
#include <linux/battery/sec_pd.h>
#else
#include <linux/power/pca9481_charger.h>
#endif
#include <linux/completion.h>
#ifdef CONFIG_USBPD_PHY_QCOM
#include <linux/usb/usbpd.h>		// Use Qualcomm USBPD PHY
#endif

#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include <linux/pm_wakeup.h>
#ifdef CONFIG_USBPD_PHY_QCOM
#include <linux/usb/usbpd.h>		// Use Qualcomm USBPD PHY
#endif

#ifdef CONFIG_USBPD_PHY_QCOM
static int pca9481_usbpd_setup(struct pca9481_charger *pca9481);
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static int pca9481_send_pd_message(struct pca9481_charger *pca9481, unsigned int msg_type);
static int get_system_current(struct pca9481_charger *pca9481);

/*******************************/
/* Switching charger control function */
/*******************************/
char *charging_state_str[] = {
	"NO_CHARGING", "CHECK_VBAT", "PRESET_DC", "CHECK_ACTIVE", "ADJUST_CC",
	"START_CC", "CC_MODE", "START_CV", "CV_MODE", "CHARGING_DONE",
	"ADJUST_TAVOL", "ADJUST_TACUR", "BYPASS_MODE", "DCMODE_CHANGE",
};

static int pca9481_read_reg(struct pca9481_charger *pca9481, unsigned reg, void *val)
{
	int ret = 0;

	mutex_lock(&pca9481->i2c_lock);
	ret = regmap_read(pca9481->regmap, reg, val);
	mutex_unlock(&pca9481->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9481_bulk_read_reg(struct pca9481_charger *pca9481, int reg, void *val, int count)
{
	int ret = 0;

	mutex_lock(&pca9481->i2c_lock);
	ret = regmap_bulk_read(pca9481->regmap, reg, val, count);
	mutex_unlock(&pca9481->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9481_write_reg(struct pca9481_charger *pca9481, int reg, u8 val)
{
	int ret = 0;

	mutex_lock(&pca9481->i2c_lock);
	ret = regmap_write(pca9481->regmap, reg, val);
	mutex_unlock(&pca9481->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9481_update_reg(struct pca9481_charger *pca9481, int reg, u8 mask, u8 val)
{
	int ret = 0;

	mutex_lock(&pca9481->i2c_lock);
	ret = regmap_update_bits(pca9481->regmap, reg, mask, val);
	mutex_unlock(&pca9481->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9481_read_adc(struct pca9481_charger *pca9481, u8 adc_ch);

static int pca9481_set_charging_state(struct pca9481_charger *pca9481, unsigned int charging_state)
{
	union power_supply_propval value = {0,};
	static int prev_val = DC_STATE_NO_CHARGING;

	pca9481->charging_state = charging_state;

	switch (charging_state) {
	case DC_STATE_NO_CHARGING:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
		break;
	case DC_STATE_CHECK_VBAT:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
		break;
	case DC_STATE_PRESET_DC:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_PRESET;
		break;
	case DC_STATE_CHECK_ACTIVE:
	case DC_STATE_START_CC:
	case DC_STATE_START_CV:
	case DC_STATE_ADJUST_TAVOL:
	case DC_STATE_ADJUST_TACUR:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST;
		break;
	case DC_STATE_CC_MODE:
	case DC_STATE_CV_MODE:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_ON;
		break;
	case DC_STATE_CHARGING_DONE:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
		break;
	case DC_STATE_BYPASS_MODE:
		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_BYPASS;
		break;
	default:
		return -1;
	}

	if (prev_val == value.intval)
		return -1;

	prev_val = value.intval;
	psy_do_property(pca9481->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, value);

	return 0;
}

static void pca9481_init_adc_val(struct pca9481_charger *pca9481, int val)
{
	int i = 0;

	for (i = 0; i < ADC_READ_MAX; ++i)
		pca9481->adc_val[i] = val;
}

/**************************************/
/* Switching charger control function */
/**************************************/
/* This function needs some modification by a customer */
static void pca9481_set_wdt_enable(struct pca9481_charger *pca9481, bool enable)
{
	int ret;
	unsigned int val;

	val = enable << MASK2SHIFT(PCA9481_BIT_WATCHDOG_EN);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_0,
			PCA9481_BIT_WATCHDOG_EN, val);
	pr_info("%s: set wdt enable = %d\n", __func__, enable);
}

static void pca9481_set_wdt_timer(struct pca9481_charger *pca9481, int time)
{
	int ret;
	unsigned int val;

	val = time << MASK2SHIFT(PCA9481_BIT_WATCHDOG_CFG);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_0,
			PCA9481_BIT_WATCHDOG_CFG, val);
	pr_info("%s: set wdt time = %d\n", __func__, time);
}

static void pca9481_check_wdt_control(struct pca9481_charger *pca9481)
{
	struct device *dev = pca9481->dev;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (pca9481->wdt_kick) {
		pca9481_set_wdt_timer(pca9481, WDT_16SEC);
		schedule_delayed_work(&pca9481->wdt_control_work, msecs_to_jiffies(PCA9481_BATT_WDT_CONTROL_T));
	} else {
		pca9481_set_wdt_timer(pca9481, WDT_16SEC);
		if (client->addr == 0xff)
			client->addr = 0x57;
	}
}

static void pca9481_wdt_control_work(struct work_struct *work)
{
	struct pca9481_charger *pca9481 = container_of(work, struct pca9481_charger,
						 wdt_control_work.work);
	struct device *dev = pca9481->dev;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	int vin, iin;

	pca9481_set_wdt_timer(pca9481, WDT_4SEC);

	/* this is for kick watchdog */
	vin = pca9481_read_adc(pca9481, ADCCH_VIN);
	iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);

	pca9481_send_pd_message(pca9481, PD_MSG_REQUEST_APDO);

	client->addr = 0xff;

	pr_info("## %s: disable client addr (vin:%dmV, iin:%dmA)\n",
		__func__, vin / PCA9481_SEC_DENOM_U_M, iin / PCA9481_SEC_DENOM_U_M);
}

static void pca9481_set_done(struct pca9481_charger *pca9481, bool enable)
{
	int ret = 0;
	union power_supply_propval value = {0, };

	value.intval = enable;
	psy_do_property(pca9481->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_DIRECT_DONE, value);

	if (ret < 0)
		pr_info("%s: error set_done, ret=%d\n", __func__, ret);
}

static void pca9481_set_switching_charger(struct pca9481_charger *pca9481, bool enable)
{
	int ret = 0;
	union power_supply_propval value = {0, };

	value.intval = enable;
	psy_do_property(pca9481->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, value);

	if (ret < 0)
		pr_info("%s: error switching_charger, ret=%d\n", __func__, ret);
}
#else
static int pca9481_set_switching_charger(bool enable,
					unsigned int input_current,
					unsigned int charging_current,
					unsigned int vfloat)
{
	int ret;
	struct power_supply *psy_swcharger;
	union power_supply_propval val;

	pr_info("%s: enable=%d, iin=%d, ichg=%d, vfloat=%d\n",
		__func__, enable, input_current, charging_current, vfloat);

	/* Insert Code */
	/* Get power supply name */
#ifdef CONFIG_USBPD_PHY_QCOM
	psy_swcharger = power_supply_get_by_name("usb");
#else
	/* Change "sw-charger" to the customer's switching charger name */
	psy_swcharger = power_supply_get_by_name("sw-charger");
#endif
	if (psy_swcharger == NULL) {
		pr_err("%s: cannot get power_supply_name-usb\n", __func__);
		ret = -ENODEV;
		goto error;
	}

	if (enable == true)	{
		/* Set Switching charger */

		/* input current */
		val.intval = input_current;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
		if (ret < 0)
			goto error;
		/* charging current */
		val.intval = charging_current;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
		if (ret < 0)
			goto error;
		/* vfloat voltage */
		val.intval = vfloat;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
		if (ret < 0)
			goto error;

		/* it depends on customer's code to enable charger */
#ifdef CONFIG_USBPD_PHY_QCOM
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
#else
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_ONLINE, &val);
#endif
		if (ret < 0)
			goto error;
	} else {
		/* disable charger */
		/* it depends on customer's code to disable charger */
#ifdef CONFIG_USBPD_PHY_QCOM
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
#else
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_ONLINE, &val);
#endif
		if (ret < 0)
			goto error;

		/* input_current */
		val.intval = input_current;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
		if (ret < 0)
			goto error;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}
#endif

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static int pca9481_get_switching_charger_property(enum power_supply_property prop,
						  union power_supply_propval *val)
{
	int ret;
	struct power_supply *psy_swcharger;

	/* Get power supply name */
#ifdef CONFIG_USBPD_PHY_QCOM
	psy_swcharger = power_supply_get_by_name("usb");
#else
	psy_swcharger = power_supply_get_by_name("sw-charger");
#endif
	if (psy_swcharger == NULL) {
		ret = -EINVAL;
		goto error;
	}
	ret = power_supply_get_property(psy_swcharger, prop, val);

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}
#endif

/*******************/
/* Send PD message */
/*******************/
/* Send Request message to the source */
/* This function needs some modification by a customer */
static int pca9481_send_pd_message(struct pca9481_charger *pca9481, unsigned int msg_type)
{
#ifdef CONFIG_USBPD_PHY_QCOM
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int pdo_idx, pps_vol, pps_cur;
#else
	u8 msg_buf[4];	/* Data Buffer for raw PD message */
	unsigned int max_cur;
	unsigned int op_cur, out_vol;
#endif
	int ret = 0;

	pr_info("%s: ======Start========\n", __func__);

	/* Cancel pps request timer */
	cancel_delayed_work(&pca9481->pps_work);

	mutex_lock(&pca9481->lock);

	if (((pca9481->charging_state == DC_STATE_NO_CHARGING) &&
		(msg_type == PD_MSG_REQUEST_APDO)) ||
		(pca9481->mains_online == false)) {
		/* Vbus reset happened in the previous PD communication */
		goto out;
	}

#ifdef CONFIG_USBPD_PHY_QCOM
	/* check the phandle */
	if (pca9481->pd == NULL) {
		pr_info("%s: get phandle\n", __func__);
		ret = pca9481_usbpd_setup(pca9481);
		if (ret != 0) {
			dev_err(pca9481->dev, "Error usbpd setup!\n");
			pca9481->pd = NULL;
			goto out;
		}
	}
#endif

	/* Check whether requested TA voltage and current are in valid range or not */
	if ((msg_type == PD_MSG_REQUEST_APDO) &&
		(pca9481->dc_mode != PTM_1TO1) &&
		((pca9481->ta_vol < TA_MIN_VOL) || (pca9481->ta_cur < TA_MIN_CUR))) {
		/* request TA voltage or current is less than minimum threshold */
		/* This is abnormal case, too low input voltage and current */
		/* Normally VIN_UVLO already happened */
		pr_err("%s: Abnormal low RDO, ta_vol=%d, ta_cur=%d\n", __func__, pca9481->ta_vol, pca9481->ta_cur);
		ret = -EINVAL;
		goto out;
	}

	pr_info("%s: msg_type=%d, ta_cur=%d, ta_vol=%d, ta_objpos=%d\n",
		__func__, msg_type, pca9481->ta_cur, pca9481->ta_vol, pca9481->ta_objpos);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pdo_idx = pca9481->ta_objpos;
	pps_vol = pca9481->ta_vol / PCA9481_SEC_DENOM_U_M;
	pps_cur = pca9481->ta_cur / PCA9481_SEC_DENOM_U_M;
	pr_info("## %s: msg_type=%d, pdo_idx=%d, pps_vol=%dmV(max_vol=%dmV), pps_cur=%dmA(max_cur=%dmA)\n",
		__func__, msg_type, pdo_idx,
		pps_vol, pca9481->pdo_max_voltage,
		pps_cur, pca9481->pdo_max_current);
#endif

	switch (msg_type) {
	case PD_MSG_REQUEST_APDO:
#ifdef CONFIG_USBPD_PHY_QCOM
		ret = usbpd_request_pdo(pca9481->pd, pca9481->ta_objpos, pca9481->ta_vol, pca9481->ta_cur);
		if (ret == -EBUSY) {
			/* wait 100ms */
			msleep(100);
			/* try again */
			ret = usbpd_request_pdo(pca9481->pd, pca9481->ta_objpos, pca9481->ta_vol, pca9481->ta_cur);
		}
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
		if (ret == -EBUSY) {
			pr_info("%s: request again ret=%d\n", __func__, ret);
			msleep(100);
			ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
		}
#else
		op_cur = pca9481->ta_cur/50000;		// Operating Current 50mA units
		out_vol = pca9481->ta_vol/20000;	// Output Voltage in 20mV units
		msg_buf[0] = op_cur & 0x7F;			// Operating Current 50mA units - B6...0
		msg_buf[1] = (out_vol<<1) & 0xFE;	// Output Voltage in 20mV units - B19..(B15)..B9
		msg_buf[2] = (out_vol>>7) & 0x0F;	// Output Voltage in 20mV units - B19..(B16)..B9,
		msg_buf[3] = pca9481->ta_objpos<<4;	// Object Position - B30...B28

		/* Send the PD message to CC/PD chip */
		/* Todo - insert code */
#endif
		/* Start pps request timer */
		if (ret == 0) {
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->pps_work,
								msecs_to_jiffies(PPS_PERIODIC_T));
		}
		break;
	case PD_MSG_REQUEST_FIXED_PDO:
#ifdef CONFIG_USBPD_PHY_QCOM
		ret = usbpd_request_pdo(pca9481->pd, pca9481->ta_objpos, pca9481->ta_vol, pca9481->ta_cur);
		if (ret == -EBUSY) {
			/* wait 100ms */
			msleep(100);
			/* try again */
			ret = usbpd_request_pdo(pca9481->pd, pca9481->ta_objpos, pca9481->ta_vol, pca9481->ta_cur);
		}
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
		if (ret == -EBUSY) {
			pr_info("%s: request again ret=%d\n", __func__, ret);
			msleep(100);
			ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
		}
#else
		max_cur = pca9481->ta_cur/10000;	// Maximum Operation Current 10mA units
		op_cur = max_cur;					// Operating Current 10mA units
		msg_buf[0] = max_cur & 0xFF;		// Maximum Operation Current -B9..(7)..0
		msg_buf[1] = ((max_cur>>8) & 0x03) | ((op_cur<<2) & 0xFC);	// Operating Current - B19..(15)..10
		// Operating Current - B19..(16)..10, Unchunked Extended Messages Supported  - not support
		msg_buf[2] = ((op_cur>>6) & 0x0F);
		msg_buf[3] = pca9481->ta_objpos<<4; // Object Position - B30...B28

		/* Send the PD message to CC/PD chip */
		/* Todo - insert code */
#endif
		break;
	default:
		break;
	}

out:
	if (((pca9481->charging_state == DC_STATE_NO_CHARGING) &&
		(msg_type == PD_MSG_REQUEST_APDO)) ||
		(pca9481->mains_online == false)) {
		/* Even though PD communication success, Vbus reset might happen */
		/* So, check the charging state again */
		ret = -EINVAL;
	}

	pr_info("%s: ret=%d\n", __func__, ret);
	mutex_unlock(&pca9481->lock);
	return ret;
}

/************************/
/* Get APDO max power   */
/************************/
/* Get the max current/voltage/power of APDO from the CC/PD driver */
/* This function needs some modification by a customer */
static int pca9481_get_apdo_max_power(struct pca9481_charger *pca9481)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int ta_max_vol_mv = (pca9481->ta_max_vol / PCA9481_SEC_DENOM_U_M);
	unsigned int ta_max_cur_ma = 0;
	unsigned int ta_max_pwr_uw = 0;
#endif

#ifdef CONFIG_USBPD_PHY_QCOM
	/* check the phandle */
	if (pca9481->pd == NULL) {
		pr_info("%s: get phandle\n", __func__);
		ret = pca9481_usbpd_setup(pca9481);
		if (ret != 0) {
			dev_err(pca9481->dev, "Error usbpd setup!\n");
			pca9481->pd = NULL;
			goto out;
		}
	}

	/* Put ta_max_vol to the desired ta maximum value, ex) 9800mV */
	/* Get new ta_max_vol and ta_max_cur, ta_max_power and proper object position by CC/PD IC */
	ret = usbpd_get_apdo_max_power(pca9481->pd, &pca9481->ta_objpos,
			&pca9481->ta_max_vol, &pca9481->ta_max_cur, &pca9481->ta_max_pwr);
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ret = sec_pd_get_apdo_max_power(&pca9481->ta_objpos,
				&ta_max_vol_mv, &ta_max_cur_ma, &ta_max_pwr_uw);
	/* mA,mV,uW --> uA,uV,uW */
	pca9481->ta_max_vol = ta_max_vol_mv * PCA9481_SEC_DENOM_U_M;
	pca9481->ta_max_cur = ta_max_cur_ma * PCA9481_SEC_DENOM_U_M;
	pca9481->ta_max_pwr = ta_max_pwr_uw;

	pr_info("%s: ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d\n",
		__func__, pca9481->ta_max_vol, pca9481->ta_max_cur, pca9481->ta_max_pwr);

	pca9481->pdo_index = pca9481->ta_objpos;
	pca9481->pdo_max_voltage = ta_max_vol_mv;
	pca9481->pdo_max_current = ta_max_cur_ma;
#else
	/* Need to implement by a customer */
	/* Get new ta_max_vol and ta_max_cur, ta_max_power and proper object position by CC/PD IC */
#endif

#ifdef CONFIG_USBPD_PHY_QCOM
out:
#endif
	return ret;
}


/******************/
/* Set RX voltage */
/******************/
/* Send RX voltage to RX IC */
/* This function needs some modification by a customer */
static int pca9481_send_rx_voltage(struct pca9481_charger *pca9481, unsigned int msg_type)
{
	struct power_supply *psy;
	union power_supply_propval pro_val;
	int ret = 0;

	mutex_lock(&pca9481->lock);

	if (pca9481->mains_online == false) {
		/* Vbus reset happened in the previous PD communication */
		goto out;
	}

	pr_info("%s: rx_vol=%d\n", __func__, pca9481->ta_vol);

	/* Need to implement send RX voltage to wireless RX IC */

	/* The below code should be modified by the customer */
	/* Get power supply name */
	psy = power_supply_get_by_name("wireless");
	if (!psy) {
		dev_err(pca9481->dev, "Cannot find wireless power supply\n");
		ret = -ENODEV;
		goto out;
	}

	/* Set the RX voltage */
	pro_val.intval = pca9481->ta_vol;

	/* Set the property */
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pro_val);
	power_supply_put(psy);
	if (ret < 0) {
		dev_err(pca9481->dev, "Cannot set RX voltage\n");
		goto out;
	}

out:
	if (pca9481->mains_online == false) {
		/* Even though PD communication success, Vbus reset might happen */
		/* So, check the charging state again */
		ret = -EINVAL;
	}

	pr_info("%s: ret=%d\n", __func__, ret);
	mutex_unlock(&pca9481->lock);
	return ret;
}


/************************/
/* Get RX max power     */
/************************/
/* Get the max current/voltage/power of RXIC from the WCRX driver */
/* This function needs some modification by a customer */
static int pca9481_get_rx_max_power(struct pca9481_charger *pca9481)
{
	struct power_supply *psy;
	union power_supply_propval pro_val;
	int ret = 0;

	/* insert code */

	/* Get power supply name */
	psy = power_supply_get_by_name("wireless");
	if (!psy) {
		dev_err(pca9481->dev, "Cannot find wireless power supply\n");
		ret = -ENODEV;
		goto error;
	}

	/* Get the maximum voltage */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &pro_val);
	if (ret < 0) {
		dev_err(pca9481->dev, "Cannot get the maximum RX voltage\n");
		goto error;
	}

	if (pca9481->ta_max_vol > pro_val.intval) {
		/* RX IC cannot support the request maximum voltage */
		ret = -EINVAL;
		goto error;
	} else {
		pca9481->ta_max_vol = pro_val.intval;
	}

	/* Get the maximum current */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &pro_val);
	if (ret < 0) {
		dev_err(pca9481->dev, "Cannot get the maximum RX current\n");
		goto error;
	}

	pca9481->ta_max_cur = pro_val.intval;
	pca9481->ta_max_pwr = (pca9481->ta_max_vol/1000)*(pca9481->ta_max_cur/1000);

error:
	power_supply_put(psy);
	return ret;
}

/**************************/
/* PCA9481 Local function */
/**************************/
/* ADC Read function */
static int pca9481_read_adc(struct pca9481_charger *pca9481, u8 adc_ch)
{
	u8 reg_data[2];
	u16 raw_adc = 0;	// raw ADC value
	int conv_adc;	// conversion ADC value
	int ret;

	switch (adc_ch) {
	case ADCCH_VIN:
		// Read ADC value
		ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_ADC_READ_VIN_0 | PCA9481_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9481_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * VIN_STEP;
		break;

	case ADCCH_VFET_IN:
		// Read ADC value
		ret = pca9481_bulk_read_reg(pca9481,
				PCA9481_REG_ADC_READ_VFET_IN_0 | PCA9481_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9481_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * VFET_IN_STEP;
		break;

	case ADCCH_OVP_OUT:
		// Read ADC value
		ret = pca9481_bulk_read_reg(pca9481,
				PCA9481_REG_ADC_READ_OVP_OUT_0 | PCA9481_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9481_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * OVP_OUT_STEP;
		break;

	case ADCCH_BATP_BATN:
		// Read ADC value
		ret = pca9481_bulk_read_reg(pca9481,
				PCA9481_REG_ADC_READ_BATP_BATN_0 | PCA9481_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9481_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * BATP_BATN_STEP;
		break;

	case ADCCH_VOUT:
		// Read ADC value
		ret = pca9481_bulk_read_reg(pca9481,
				PCA9481_REG_ADC_READ_VOUT_0 | PCA9481_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9481_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * VOUT_STEP;
		break;

	case ADCCH_NTC:
		// Read ADC value
		ret = pca9481_bulk_read_reg(pca9481,
				PCA9481_REG_ADC_READ_NTC_0 | PCA9481_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9481_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * NTC_STEP;
		break;

	case ADCCH_DIE_TEMP:
		// Read ADC value
		ret = pca9481_bulk_read_reg(pca9481,
				PCA9481_REG_ADC_READ_DIE_TEMP_0 | PCA9481_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9481_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * DIE_TEMP_STEP / DIE_TEMP_DENOM;	// Temp, unit - C
		if (conv_adc > DIE_TEMP_MAX)
			conv_adc = DIE_TEMP_MAX;
		break;

	case ADCCH_VIN_CURRENT:
		// Read ADC value
		ret = pca9481_bulk_read_reg(pca9481,
				PCA9481_REG_ADC_READ_VIN_CURRENT_0 | PCA9481_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9481_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		/* Compensate raw_adc */
		conv_adc = PCA9481_IIN_ADC_COMP_GAIN*raw_adc + PCA9481_IIN_ADC_COMP_OFFSET;
		if (conv_adc < 0)
			conv_adc = raw_adc * VIN_CURRENT_STEP;
		break;

	case ADCCH_BAT_CURRENT:
		// Read ADC value
		ret = pca9481_bulk_read_reg(pca9481,
				PCA9481_REG_ADC_READ_I_VBAT_CURRENT_0 | PCA9481_REG_BIT_AI, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9481_BIT_ADC_READ_11_8)<<8) | reg_data[0];
		conv_adc = raw_adc * BAT_CURRENT_STEP;
		break;

	default:
		conv_adc = -EINVAL;
		break;
	}

error:
	pr_info("%s: adc_ch=%d, raw_adc=0x%x, convert_val=%d\n", __func__,
		adc_ch, raw_adc, conv_adc);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (adc_ch == ADCCH_DIE_TEMP)
		pca9481->adc_val[adc_ch] = conv_adc;
	else
		pca9481->adc_val[adc_ch] = conv_adc / PCA9481_SEC_DENOM_U_M;
#endif
	return conv_adc;
}


static int pca9481_set_vfloat(struct pca9481_charger *pca9481, unsigned int vfloat)
{
	int ret, val;

	pr_info("%s: vfloat=%d\n", __func__, vfloat);

	/* float voltage - battery regulation voltage */
	/* maximum value is 5V */
	if (vfloat > PCA9481_VBAT_REG_MAX)
		vfloat = PCA9481_VBAT_REG_MAX;
	/* minimum value is 3.725V */
	if (vfloat < PCA9481_VBAT_REG_MIN)
		vfloat = PCA9481_VBAT_REG_MIN;

	val = PCA9481_VBAT_REG(vfloat);
	ret = pca9481_write_reg(pca9481, PCA9481_REG_CHARGING_CNTL_2, val);
	return ret;
}

static int pca9481_set_charging_current(struct pca9481_charger *pca9481, unsigned int ichg)
{
	int ret, val;

	pr_info("%s: ichg=%d\n", __func__, ichg);

	/* charging current - battery regulation current */
	/* round-up charging current with ibat regulation resolution */
	if (ichg % PCA9481_IBAT_REG_STEP)
		ichg = ichg + PCA9481_IBAT_REG_STEP;

	/* Add 100mA offset to avoid frequent battery current regulation */
	ichg = ichg + PCA9481_IBAT_REG_OFFSET;

	/* maximum value is 10A */
	if (ichg > PCA9481_IBAT_REG_MAX)
		ichg = PCA9481_IBAT_REG_MAX;
	/* minimum value is 1A */
	if (ichg < PCA9481_IBAT_REG_MIN)
		ichg = PCA9481_IBAT_REG_MIN;

	val = PCA9481_IBAT_REG(ichg);
	ret = pca9481_write_reg(pca9481, PCA9481_REG_CHARGING_CNTL_3, val);
	return ret;
}

static int pca9481_set_input_current(struct pca9481_charger *pca9481, unsigned int iin)
{
	int ret, val;

	pr_info("%s: iin=%d\n", __func__, iin);

	/* input current - input regulation current */

	/* round-up input current with input regulation resolution */
	if (iin % PCA9481_IIN_REG_STEP)
		iin = iin + PCA9481_IIN_REG_STEP;

	/* Add offset for input current regulation */
	if (iin < PCA9481_IIN_REG_OFFSET1_TH)
		iin = iin + PCA9481_IIN_REG_OFFSET1;
	else if (iin < PCA9481_IIN_REG_OFFSET2_TH)
		iin = iin + PCA9481_IIN_REG_OFFSET2;
	else
		iin = iin + PCA9481_IIN_REG_OFFSET3;

	/* maximum value is 6A */
	if (iin > PCA9481_IIN_REG_MAX)
		iin = PCA9481_IIN_REG_MAX;
	/* minimum value is 500mA */
	if (iin < PCA9481_IIN_REG_MIN)
		iin = PCA9481_IIN_REG_MIN;

	val = PCA9481_IIN_REG(iin);
	ret = pca9481_write_reg(pca9481, PCA9481_REG_CHARGING_CNTL_1, val);

	pr_info("%s: real iin_cfg=%d\n", __func__, val*PCA9481_IIN_REG_STEP + PCA9481_IIN_REG_MIN);
	return ret;
}

static int pca9481_set_charging(struct pca9481_charger *pca9481, bool enable)
{
	int ret, val;
	u8 sc_op_reg;

	pr_info("%s: enable=%d\n", __func__, enable);

	if (pca9481->dc_mode == PTM_1TO1) {
		/* Forward 1:1 mode */
		sc_op_reg = PCA9481_SC_OP_F_11;
	} else {
		/* 2:1 Switching mode */
		sc_op_reg = PCA9481_SC_OP_21;
	}

	/* Check device's current status */
	/* Read DEVICE_3_STS register */
	ret = regmap_read(pca9481->regmap, PCA9481_REG_DEVICE_3_STS, &val);
	if (ret < 0)
		goto error;
	pr_info("%s: power state = 0x%x\n", __func__, val);

	if (enable == true) {
		/* Set NTC Protection */
		ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_0,
					PCA9481_BIT_STANDBY_BY_NTC_EN,
					pca9481->pdata->ntc_en << MASK2SHIFT(PCA9481_BIT_STANDBY_BY_NTC_EN));

		/* Set STANDBY_EN bit to 1 */
		ret = pca9481_update_reg(pca9481, PCA9481_REG_SC_CNTL_3,
								PCA9481_BIT_STANDBY_EN | PCA9481_BIT_SC_OPERATION_MODE,
								PCA9481_STANDBY_FORCE | sc_op_reg);

		/* Set EN_CFG to active low */
		ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_2,
								PCA9481_BIT_EN_CFG, PCA9481_EN_ACTIVE_L);

		/* Set STANDBY_EN bit to 0 */
		ret = pca9481_update_reg(pca9481, PCA9481_REG_SC_CNTL_3,
								PCA9481_BIT_STANDBY_EN | PCA9481_BIT_SC_OPERATION_MODE,
								PCA9481_STANDBY_DONOT | sc_op_reg);
	} else {
		/* Disable NTC Protection */
		ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_0,
					PCA9481_BIT_STANDBY_BY_NTC_EN, 0);

		/* Set EN_CFG to active high - Disable PCA9481 */
		ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_2,
								PCA9481_BIT_EN_CFG, PCA9481_EN_ACTIVE_H);
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

static int pca9481_softreset(struct pca9481_charger *pca9481)
{
	int ret, val;
	u8 reg_val[10]; /* Dump for control register */

	pr_info("%s: do soft reset\n", __func__);


	/* Check the current register before softreset */
	pr_info("%s: Before softreset\n", __func__);

	/* Read all status registers for debugging */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_DEVICE_0_STS | PCA9481_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: status reg[0x0F]=0x%x,[0x10]=0x%x,[0x11]=0x%x,[0x12]=0x%x,[0x13]=0x%x,[0x14]=0x%x,[0x15]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read all control registers for debugging */
	/* Read device, auto restart, and rcp control registers for debugging */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_DEVICE_CNTL_0 | PCA9481_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x16]=0x%x,[0x17]=0x%x,[0x18]=0x%x,[0x19]=0x%x,[0x1A]=0x%x,[0x1B]=0x%x,[0x1C]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read charging control registers for debugging */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_CHARGING_CNTL_0 | PCA9481_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x1D]=0x%x,[0x1E]=0x%x,[0x1F]=0x%x,[0x20]=0x%x,[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x\n",
		__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read ntc, sc, and adc control registers for debugging */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_NTC_0_CNTL | PCA9481_REG_BIT_AI,
							&reg_val[0], 9);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x24]=0x%x,[0x25]=0x%x,[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5]);
	pr_info("%s: control reg[0x2A]=0x%x,[0x2B]=0x%x,[0x2C]=0x%x\n",
			__func__, reg_val[6], reg_val[7], reg_val[8]);

	/* Do softreset */

	/* Set softreset register */
	ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_0,
							PCA9481_BIT_SOFT_RESET, PCA9481_BIT_SOFT_RESET);

	/* Wait 5ms */
	usleep_range(5000, 6000);

	/* Reset PCA9481 and all regsiters values go to POR values */
	/* Check the current register after softreset */
	/* Read all control registers for debugging */
	pr_info("%s: After softreset\n", __func__);

	/* Read all status registers for debugging */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_DEVICE_0_STS | PCA9481_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: status reg[0x0F]=0x%x,[0x10]=0x%x,[0x11]=0x%x,[0x12]=0x%x,[0x13]=0x%x,[0x14]=0x%x,[0x15]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read all control registers for debugging */
	/* Read device, auto restart, and rcp control registers for debugging */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_DEVICE_CNTL_0 | PCA9481_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x16]=0x%x,[0x17]=0x%x,[0x18]=0x%x,[0x19]=0x%x,[0x1A]=0x%x,[0x1B]=0x%x,[0x1C]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read charging control registers for debugging */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_CHARGING_CNTL_0 | PCA9481_REG_BIT_AI,
							&reg_val[0], 7);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x1D]=0x%x,[0x1E]=0x%x,[0x1F]=0x%x,[0x20]=0x%x,[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x\n",
		__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6]);

	/* Read ntc, sc, and adc control registers for debugging */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_NTC_0_CNTL | PCA9481_REG_BIT_AI,
							&reg_val[0], 9);
	if (ret < 0)
		goto error;
	pr_info("%s: control reg[0x24]=0x%x,[0x25]=0x%x,[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5]);
	pr_info("%s: control reg[0x2A]=0x%x,[0x2B]=0x%x,[0x2C]=0x%x\n",
			__func__, reg_val[6], reg_val[7], reg_val[8]);

	/* Set the initial register value */

	/* Set Reverse Current Detection */
	val = PCA9481_BIT_RCP_EN;
	ret = pca9481_update_reg(pca9481, PCA9481_REG_RCP_CNTL,
							PCA9481_BIT_RCP_EN, val);
	if (ret < 0)
		goto error;

	/* Die Temperature regulation 120'C */
	val = 0x3 << MASK2SHIFT(PCA9481_BIT_THERMAL_REGULATION_CFG);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_1,
				PCA9481_BIT_THERMAL_REGULATION_CFG | PCA9481_BIT_THERMAL_REGULATION_EN,
				val | PCA9481_BIT_THERMAL_REGULATION_EN);
	if (ret < 0)
		goto error;

	/* Set external sense resistor value */
	val = pca9481->pdata->snsres << MASK2SHIFT(PCA9481_BIT_IBAT_SENSE_R_SEL);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_CHARGING_CNTL_4,
			PCA9481_BIT_IBAT_SENSE_R_SEL, val);
	if (ret < 0)
		goto error;

	/* Set external sense resistor location */
	val = pca9481->pdata->snsres_cfg << MASK2SHIFT(PCA9481_BIT_IBAT_SENSE_R_CFG);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_2,
			PCA9481_BIT_IBAT_SENSE_R_CFG, val);
	if (ret < 0)
		goto error;

	/* Disable battery charge current regulation loop */
	/* Disable current measurement through CSP and CSN */
	val = 0;
	ret = pca9481_update_reg(pca9481, PCA9481_REG_CHARGING_CNTL_0,
			PCA9481_BIT_CSP_CSN_MEASURE_EN | PCA9481_BIT_I_VBAT_LOOP_EN,
			val);
	if (ret < 0)
		goto error;

	/* Set the ADC channel */
	val = (PCA9481_BIT_ADC_READ_VIN_CURRENT_EN |	/* IIN ADC */
		   PCA9481_BIT_ADC_READ_DIE_TEMP_EN |		/* DIE_TEMP ADC */
		   PCA9481_BIT_ADC_READ_NTC_EN |			/* NTC ADC */
		   PCA9481_BIT_ADC_READ_VOUT_EN |			/* VOUT ADC */
		   PCA9481_BIT_ADC_READ_BATP_BATN_EN |		/* VBAT ADC */
		   PCA9481_BIT_ADC_READ_OVP_OUT_EN |		/* OVP_OUT ADC */
		   PCA9481_BIT_ADC_READ_VIN_EN);			/* VIN ADC */

	ret = pca9481_write_reg(pca9481, PCA9481_REG_ADC_EN_CNTL_0, val);
	if (ret < 0)
		goto error;

	/* Enable ADC */
	val = PCA9481_BIT_ADC_EN | ((unsigned int)(ADC_AVG_16sample << MASK2SHIFT(PCA9481_BIT_ADC_AVERAGE_TIMES)));
	ret = pca9481_write_reg(pca9481, PCA9481_REG_ADC_CNTL, val);
	if (ret < 0)
		goto error;

	/*
	 * Configure the Mask Register for interrupts: disable all interrupts by default.
	 */
	reg_val[REG_DEVICE_0] = 0xFF;
	reg_val[REG_DEVICE_1] = 0xFF;
	reg_val[REG_DEVICE_2] = 0xFF;
	reg_val[REG_DEVICE_3] = 0xFF;
	reg_val[REG_CHARGING] = 0xFF;
	reg_val[REG_SC_0] = 0xFF;
	reg_val[REG_SC_1] = 0xFF;

	ret = regmap_bulk_write(pca9481->regmap, PCA9481_REG_INT_DEVICE_0_MASK | PCA9481_REG_BIT_AI,
							&reg_val[0], REG_BUFFER_MAX);
	if (ret < 0)
		goto error;

	return 0;

error:
	pr_info("%s: i2c error, ret=%d\n", __func__, ret);
	return ret;
}

/* Check Active status */
static int pca9481_check_error(struct pca9481_charger *pca9481)
{
	int ret;
	unsigned int reg_val;
	int vbatt;
	int rt;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481->chg_status = POWER_SUPPLY_STATUS_CHARGING;
	pca9481->health_status = POWER_SUPPLY_HEALTH_GOOD;
#endif

	/* Read DEVICE_3_STS register */
	ret = pca9481_read_reg(pca9481, PCA9481_REG_DEVICE_3_STS, &reg_val);
	if (ret < 0)
		goto error;

	/* Read VBAT_ADC */
	vbatt = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);

	/* Check Active status */
	if ((reg_val & PCA9481_BIT_STATUS_CHANGE) == PCA9481_21SW_F11_MODE) {
		/* PCA9481 is in 2:1 switching mode */
		/* Check whether the battery voltage is over the minimum voltage level or not */
		if (vbatt > DC_VBAT_MIN) {
			/* Normal charging battery level */
			/* Check temperature regulation loop */
			/* Read DEVICE_2_STS register */
			ret = pca9481_read_reg(pca9481, PCA9481_REG_DEVICE_3_STS, &reg_val);
			if (reg_val & PCA9481_BIT_THEM_REG) {
				/* Thermal regulation happened */
				pr_err("%s: Device is in temperature regulation\n", __func__);
				ret = -EINVAL;
			} else {
				/* Normal temperature */
				ret = 0;
			}
		} else {
			/* Abnormal battery level */
			pr_err("%s: Error abnormal battery voltage=%d\n", __func__, vbatt);
			ret = -EINVAL;
		}
	} else {
		/* PCA9481 is not in 2:1 switching mode - standby or shutdown state */
		/* Stop charging in timer_work */

		/* Read all status register for debugging */
		u8 val[REG_BUFFER_MAX];
		u8 deg_val[10]; /* Dump for control register */

		ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_DEVICE_0_STS | PCA9481_REG_BIT_AI,
								&val[REG_DEVICE_0], REG_BUFFER_MAX);
		pr_err("%s: Error reg[0x0F]=0x%x,[0x10]=0x%x,[0x11]=0x%x,[0x12]=0x%x,[0x13]=0x%x,[0x14]=0x%x,[0x15]=0x%x\n",
				__func__, val[0], val[1], val[2], val[3], val[4], val[5], val[6]);

		/* Check status register */
		if ((val[REG_DEVICE_0] & PCA9481_BIT_VIN_VALID) != PCA9481_BIT_VIN_VALID) {
			/* Invalid VIN or VOUT */
			if (val[REG_DEVICE_0] & PCA9481_BIT_VOUT_MAX_OV) {
				pr_err("%s: VOUT MAX_OV\n", __func__);		/* VOUT > VOUT_MAX */
				ret = -EINVAL;
			} else if (val[REG_DEVICE_0] & PCA9481_BIT_RCP_DETECTED) {
				pr_err("%s: RCP_DETECTED\n", __func__);		/* Reverse Current Protection */
				ret = -EINVAL;
			} else if (val[REG_DEVICE_0] & PCA9481_BIT_VIN_UNPLUG) {
				pr_err("%s: VIN_UNPLUG\n", __func__);			/* VIN < VIN_UNPLUG */
				ret = -EINVAL;
			} else if (val[REG_DEVICE_0] & PCA9481_BIT_VIN_OVP) {
				pr_err("%s: VIN_OVP\n", __func__);			/* VIN > VIN_OVP */
				ret = -EINVAL;
			} else if (val[REG_DEVICE_0] & PCA9481_BIT_VIN_OV_TRACKING) {
				pr_err("%s: VIN_OV_TRACKING\n", __func__);	/* VIN > VIN_OV_TRACKING */
				ret = -EAGAIN;
			} else if (val[REG_DEVICE_0] & PCA9481_BIT_VIN_UV_TRACKING) {
				pr_err("%s: VIN_UV_TRACKING\n", __func__);	/* VIN < VIN_UV_TRACKING */
				ret = -EINVAL;
			} else {
				pr_err("%s: Invalid VIN or VOUT\n", __func__);
				ret = -EINVAL;
			}
		} else if (val[REG_DEVICE_0] & PCA9481_BIT_RCP_DETECTED) {
			/* Valid VIN and RCP happens - ignore it */
			/* If Vin increase, PCA9481 will exit RCP condition */
			pr_err("%s: RCP_DETECTED\n", __func__);		/* Reverse Current Protection */
			ret = -EAGAIN;
		} else if (val[REG_DEVICE_1] & PCA9481_BIT_SINK_RCP_ENABLED) {
			/* Valid VIN and RCP happens - ignore it */
			/* If Vin increase, PCA9481 will exit RCP condition */
			pr_err("%s: SINK_RCP_ENABLED\n", __func__);	/* Isink_rcp Enabled */
			ret = -EAGAIN;
		} else if (val[REG_DEVICE_1] & PCA9481_BIT_SINK_RCP_TIMEOUT) {
			/* Valid VIN and RCP happens - ignore it */
			/* If Vin increase, PCA9481 will exit RCP condition */
			pr_err("%s: SINK_RCP_TIMEOUT\n", __func__);	/* discharge timer expired */
			ret = -EAGAIN;
		} else if (val[REG_DEVICE_1] & (PCA9481_BIT_NTC_0_DETECTED | PCA9481_BIT_NTC_1_DETECTED)) {
			if (pca9481->pdata->ntc_en == 0) {
				/* NTC protection function is disabled */
				/* ignore it */
				pr_err("%s: NTC_THRESHOLD - ignore\n", __func__);	/* NTC_THRESHOLD */
				ret = -EAGAIN;
			} else {
				pr_err("%s: NTC_THRESHOLD\n", __func__);	/* NTC_THRESHOLD */
				ret = -EINVAL;
			}
		} else if (val[REG_DEVICE_1] & PCA9481_BIT_VIN_OCP_21_11) {
			pr_err("%s: VIN_OCP_21_11\n", __func__);	/* VIN_OCP */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_2] & PCA9481_BIT_THEM_REG) {
			pr_err("%s: THEM_REGULATION\n", __func__);	/* Thermal Regulation */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_2] & PCA9481_BIT_THSD) {
			pr_err("%s: THERMAL_SHUTDOWN\n", __func__);	/* Thermal Shutdown */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_2] & PCA9481_BIT_WATCHDOG_TIMER_OUT) {
			pr_err("%s: WATCHDOG_TIMER_OUT\n", __func__);	/* Watchdog timer out */
			ret = -EINVAL;
		} else if (val[REG_DEVICE_3] & PCA9481_BIT_VFET_IN_OVP) {
			pr_err("%s: VFET_IN_OVP\n", __func__);		/* VFET_IN > VFET_IN_OVP */
			ret = -EINVAL;
		} else if (val[REG_CHARGING] & PCA9481_BIT_CHARGER_SAFETY_TIMER) {
			pr_err("%s: CHARGER_SAFETY_TIMER_OUT\n", __func__);	/* Charger Safety Timer is expired */
			ret = -EINVAL;
		} else if (val[REG_CHARGING] & PCA9481_BIT_VBAT_OVP) {
			pr_err("%s: VBAT_OVP\n", __func__);			/* VBAT > VBAT_OVP */
			ret = -EINVAL;
		} else if (val[REG_SC_0] & PCA9481_BIT_PHASE_B_FAULT) {
			pr_err("%s: PHASE_B_FAULT\n", __func__);	/* Phase B CFLY short */
			ret = -EINVAL;
		} else if (val[REG_SC_0] & PCA9481_BIT_PHASE_A_FAULT) {
			pr_err("%s: PHASE_A_FAULT\n", __func__);	/* Phase A CFLY short */
			ret = -EINVAL;
		} else if (val[REG_SC_0] & PCA9481_BIT_PIN_SHORT) {
			pr_err("%s: PIN_SHORT\n", __func__);		/* CFLY, SW, VIN, and OVP_OUT short */
			ret = -EINVAL;
		} else if (val[REG_SC_1] & PCA9481_BIT_OVPOUT_ERRLO) {
			pr_err("%s: OVPOUT_ERRLO\n", __func__);		/* OVP_OUT is too low */
			ret = -EINVAL;
		} else {
			pr_err("%s: Power state error\n", __func__);	/* Power State error */
			ret = -EAGAIN;	/* retry */
		}

		/* Read all control registers for debugging */
		/* Read device, auto restart, and rcp control registers for debugging */
		rt = regmap_bulk_read(pca9481->regmap, PCA9481_REG_DEVICE_CNTL_0 | PCA9481_REG_BIT_AI,
								&deg_val[0], 7);
		if (rt < 0)
			goto error;
		pr_info("%s: control reg[0x16]=0x%x,[0x17]=0x%x,[0x18]=0x%x,[0x19]=0x%x,[0x1A]=0x%x,[0x1B]=0x%x,[0x1C]=0x%x\n",
				__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5], deg_val[6]);

		/* Read charging control registers for debugging */
		rt = regmap_bulk_read(pca9481->regmap, PCA9481_REG_CHARGING_CNTL_0 | PCA9481_REG_BIT_AI,
								&deg_val[0], 7);
		if (rt < 0)
			goto error;
		pr_info("%s: control reg[0x1D]=0x%x,[0x1E]=0x%x,[0x1F]=0x%x,[0x20]=0x%x,[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x\n",
			__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5], deg_val[6]);

		/* Read ntc, sc, and adc control registers for debugging */
		rt = regmap_bulk_read(pca9481->regmap, PCA9481_REG_NTC_0_CNTL | PCA9481_REG_BIT_AI,
								&deg_val[0], 9);
		if (rt < 0)
			goto error;
		pr_info("%s: control reg[0x24]=0x%x,[0x25]=0x%x,[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x\n",
				__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5]);
		pr_info("%s: control reg[0x2A]=0x%x,[0x2B]=0x%x,[0x2C]=0x%x\n",
				__func__, deg_val[6], deg_val[7], deg_val[8]);

		/* Read ADC register for debugging */
		rt = regmap_bulk_read(pca9481->regmap, PCA9481_REG_ADC_READ_VIN_0 | PCA9481_REG_BIT_AI,
								&deg_val[0], 10);
		if (rt < 0)
			goto error;
		pr_info("%s: adc reg[0x2D]=0x%x,[0x2E]=0x%x,[0x2F]=0x%x,[0x30]=0x%x,[0x31]=0x%x,[0x32]=0x%x\n",
				__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5]);
		pr_info("%s: adc reg[0x33]=0x%x,[0x34]=0x%x,[0x35]=0x%x,[0x36]=0x%x\n",
				__func__, deg_val[6], deg_val[7], deg_val[8], deg_val[9]);

		rt = regmap_bulk_read(pca9481->regmap, PCA9481_REG_ADC_READ_NTC_0 | PCA9481_REG_BIT_AI,
								&deg_val[0], 8);
		if (rt < 0)
			goto error;
		pr_info("%s: adc reg[0x37]=0x%x,[0x38]=0x%x,[0x39]=0x%x,[0x3A]=0x%x,[0x3B]=0x%x,[0x3C]=0x%x\n",
				__func__, deg_val[0], deg_val[1], deg_val[2], deg_val[3], deg_val[4], deg_val[5]);
		pr_info("%s: adc reg[0x3D]=0x%x,[0x3E]=0x%x\n", __func__, deg_val[6], deg_val[7]);
	}

error:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (ret == -EINVAL) {
		pca9481->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		pca9481->health_status = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
	}
#endif
	pr_info("%s: Active Status=%d\n", __func__, ret);
	return ret;
}

/* Check DC Mode status */
static int pca9481_check_dcmode_status(struct pca9481_charger *pca9481)
{
	unsigned int reg_val;
	int ret, i;

	/* Read CHARGING_STS */
	ret = pca9481_read_reg(pca9481, PCA9481_REG_CHARGING_STS, &reg_val);
	if (ret < 0)
		goto error;

	/* Check CHARGING_STS */
	if (reg_val & PCA9481_BIT_VBAT_REG_LOOP)
		ret = DCMODE_VFLT_LOOP;
	else if (reg_val & PCA9481_BIT_I_VBAT_CC_LOOP)
		ret = DCMODE_CHG_LOOP;
	else if (reg_val & PCA9481_BIT_I_VIN_CC_LOOP) {
		ret = DCMODE_IIN_LOOP;
		/* Check IIN_LOOP again to avoid unstable IIN_LOOP period */
		for (i = 0; i < 4; i++) {
			/* Wait 2ms */
			usleep_range(2000, 3000);
			/* Read CHARGING_STS again */
			ret = pca9481_read_reg(pca9481, PCA9481_REG_CHARGING_STS, &reg_val);
			if (ret < 0)
				goto error;
			/* Check CHARGING_STS again */
			if ((reg_val & PCA9481_BIT_I_VIN_CC_LOOP) != PCA9481_BIT_I_VIN_CC_LOOP) {
				/* Now pca9481 is in unstable IIN_LOOP period */
				/* Ignore IIN_LOOP status */
				pr_info("%s: Unstable IIN_LOOP\n", __func__);
				ret = DCMODE_LOOP_INACTIVE;
				break;
			}
		}
	} else
		ret = DCMODE_LOOP_INACTIVE;

error:
	pr_info("%s: DCMODE Status=%d\n", __func__, ret);
	return ret;
}

/* Stop Charging */
static int pca9481_stop_charging(struct pca9481_charger *pca9481)
{
	int ret = 0;

	/* Check the current state */
	if (pca9481->charging_state != DC_STATE_NO_CHARGING) {
		/* Recover switching charger ICL */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9481_set_switching_charger(pca9481, true);
#else
		ret = pca9481_set_switching_charger(true, SWCHG_ICL_NORMAL,
											pca9481->ichg_cfg,
											pca9481->vfloat);
#endif
		if (ret < 0) {
			pr_err("%s: Error-set_switching charger ICL\n", __func__);
			goto error;
		}

		/* Stop Direct charging */
		cancel_delayed_work(&pca9481->timer_work);
		cancel_delayed_work(&pca9481->pps_work);
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_ID_NONE;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);

		/* Clear parameter */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9481_set_charging_state(pca9481, DC_STATE_NO_CHARGING);
		pca9481_init_adc_val(pca9481, -1);
#else
		pca9481->charging_state = DC_STATE_NO_CHARGING;
#endif
		pca9481->ret_state = DC_STATE_NO_CHARGING;
		pca9481->ta_target_vol = TA_MAX_VOL;
		pca9481->prev_iin = 0;
		pca9481->prev_inc = INC_NONE;
		mutex_lock(&pca9481->lock);
		pca9481->req_new_iin = false;
		pca9481->req_new_vfloat = false;
		mutex_unlock(&pca9481->lock);
		pca9481->chg_mode = CHG_NO_DC_MODE;
		pca9481->ta_ctrl = TA_CTRL_CL_MODE;

		/* Set iin_cfg, ichg_cfg, vfloat and iin_topoff to the default value */
		pca9481->iin_cfg = pca9481->pdata->iin_cfg;
		pca9481->vfloat = pca9481->pdata->vfloat;
		pca9481->ichg_cfg = pca9481->pdata->ichg_cfg;
		pca9481->iin_topoff = pca9481->pdata->iin_topoff;

		/* Clear new vfloat and new iin */
		pca9481->new_vfloat = pca9481->pdata->vfloat;
		pca9481->new_iin = pca9481->pdata->iin_cfg;

		/* Cleare new DC mode and DC mode */
		pca9481->new_dc_mode = PTM_NONE;
		pca9481->dc_mode = PTM_NONE;
		mutex_lock(&pca9481->lock);
		pca9481->req_new_dc_mode = false;
		mutex_unlock(&pca9481->lock);
		/* Enable reverse current detection */
		ret = pca9481_update_reg(pca9481, PCA9481_REG_RCP_CNTL,
								PCA9481_BIT_RCP_EN, PCA9481_BIT_RCP_EN);
		if (ret < 0) {
			pr_err("%s: Error-set RCP detection\n", __func__);
			goto error;
		}

		/* Clear retry counter */
		pca9481->retry_cnt = 0;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		/* Set watchdog timer disable */
		pca9481_set_wdt_enable(pca9481, WDT_DISABLE);
#endif
		ret = pca9481_set_charging(pca9481, false);
		if (ret < 0) {
			pr_err("%s: Error-set_charging(main)\n", __func__);
			goto error;
		}
	}

error:
	__pm_relax(pca9481->monitor_wake_lock);
	pr_info("%s: END, ret=%d\n", __func__, ret);

	return ret;
}

/* Compensate TA current for the target input current */
static int pca9481_set_ta_current_comp(struct pca9481_charger *pca9481)
{
	int iin, ichg;

	/* Read IIN ADC */
	iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
	/* Read ICHG ADC */
	ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);

	pr_info("%s: iin=%d, ichg=%d\n", __func__, iin, ichg);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9481->iin_cc + IIN_CC_COMP_OFFSET)) {
		/* TA current is higher than the target input current */
		/* Compare TA current with IIN_CC - LOW_OFFSET */
		if (pca9481->ta_cur > pca9481->iin_cc - TA_CUR_LOW_OFFSET) {
			/* TA current is higher than IIN_CC - LOW_OFFSET */
			/* Assume that TA operation mode is CL mode, so decrease TA current */
			/* Decrease TA current (50mA) */
			pca9481->ta_cur = pca9481->ta_cur - PD_MSG_TA_CUR_STEP;
			pr_info("%s: Comp. Cont1: ta_cur=%d\n", __func__, pca9481->ta_cur);
		} else {
			/* TA current is already lower than IIN_CC - LOW_OFFSET */
			/* IIN_ADC is stiil in invalid range even though TA current is less than IIN_CC - LOW_OFFSET */
			/* TA has abnormal behavior */
			/* Decrease TA voltage (20mV) */
			pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
			pr_info("%s: Comp. Cont2: ta_vol=%d\n", __func__, pca9481->ta_vol);
			/* Update TA target voltage */
			pca9481->ta_target_vol = pca9481->ta_vol;
		}
		/* Send PD Message */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PDMSG_SEND;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
	} else {
		if (iin < (pca9481->iin_cc - IIN_CC_COMP_OFFSET)) {
			/* TA current is lower than the target input current */
			/* Compare present TA voltage with TA maximum voltage first */
			if (pca9481->ta_vol == pca9481->ta_max_vol) {
				/* TA voltage is already the maximum voltage */
				/* Compare present TA current with TA maximum current */
				if (pca9481->ta_cur == pca9481->ta_max_cur) {
					/* Both of present TA voltage and current are already maximum values */
					pr_info("%s: Comp. End1(Max value): ta_vol=%d, ta_cur=%d\n",
						__func__, pca9481->ta_vol, pca9481->ta_cur);
					/* Set timer */
					mutex_lock(&pca9481->lock);
					pca9481->timer_id = TIMER_CHECK_CCMODE;
					pca9481->timer_period = CCMODE_CHECK_T;
					mutex_unlock(&pca9481->lock);
				} else {
					/* TA voltage is maximum voltage, but TA current is not maximum current */
					/* Increase TA current (50mA) */
					pca9481->ta_cur = pca9481->ta_cur + PD_MSG_TA_CUR_STEP;
					if (pca9481->ta_cur > pca9481->ta_max_cur)
						pca9481->ta_cur = pca9481->ta_max_cur;
					pr_info("%s: Comp. Cont3: ta_cur=%d\n", __func__, pca9481->ta_cur);
					/* Send PD Message */
					mutex_lock(&pca9481->lock);
					pca9481->timer_id = TIMER_PDMSG_SEND;
					pca9481->timer_period = 0;
					mutex_unlock(&pca9481->lock);

					/* Set TA increment flag */
					pca9481->prev_inc = INC_TA_CUR;
				}
			} else {
				/* TA voltage is not maximum voltage */
				/* Compare IIN ADC with previous IIN ADC + 20mA */
				if (iin > (pca9481->prev_iin + IIN_ADC_OFFSET)) {
					/* In this case, TA voltage is not enough to supply */
					/* the operating current of RDO. So, increase TA voltage */
					/* Increase TA voltage (20mV) */
					pca9481->ta_vol = pca9481->ta_vol + PD_MSG_TA_VOL_STEP;
					if (pca9481->ta_vol > pca9481->ta_max_vol)
						pca9481->ta_vol = pca9481->ta_max_vol;
					pr_info("%s: Comp. Cont4: ta_vol=%d\n",
						__func__, pca9481->ta_vol);
					/* Update TA target voltage */
					pca9481->ta_target_vol = pca9481->ta_vol;
					/* Send PD Message */
					mutex_lock(&pca9481->lock);
					pca9481->timer_id = TIMER_PDMSG_SEND;
					pca9481->timer_period = 0;
					mutex_unlock(&pca9481->lock);

					/* Set TA increment flag */
					pca9481->prev_inc = INC_TA_VOL;
				} else {
					/* Input current increment is too low */
					/* It is possible that TA is in current limit mode or has low TA voltage */
					/* Increase TA current or voltage */
					/* Check the previous TA increment */
					if (pca9481->prev_inc == INC_TA_VOL) {
						/* The previous increment is TA voltage, but input current does not increase */
						/* Try to increase TA current */
						/* Compare present TA current with TA maximum current */
						if (pca9481->ta_cur == pca9481->ta_max_cur) {
							/* TA current is already the maximum current */

							/* Increase TA voltage (20mV) */
							pca9481->ta_vol = pca9481->ta_vol + PD_MSG_TA_VOL_STEP;
							if (pca9481->ta_vol > pca9481->ta_max_vol)
								pca9481->ta_vol = pca9481->ta_max_vol;
							pr_info("%s: Comp. Cont5: ta_vol=%d\n",
								__func__, pca9481->ta_vol);
							/* Update TA target voltage */
							pca9481->ta_target_vol = pca9481->ta_vol;
							/* Send PD Message */
							mutex_lock(&pca9481->lock);
							pca9481->timer_id = TIMER_PDMSG_SEND;
							pca9481->timer_period = 0;
							mutex_unlock(&pca9481->lock);

							/* Set TA increment flag */
							pca9481->prev_inc = INC_TA_VOL;
						} else {
							/* Check the present TA current */
							/* Consider tolerance offset(100mA) */
							if (pca9481->ta_cur >= (pca9481->iin_cc + TA_IIN_OFFSET)) {
								/* Maybe TA supply current is enough, but TA voltage is low */
								/* Increase TA voltage (20mV) */
								pca9481->ta_vol = pca9481->ta_vol + PD_MSG_TA_VOL_STEP;
								if (pca9481->ta_vol > pca9481->ta_max_vol)
									pca9481->ta_vol = pca9481->ta_max_vol;
								pr_info("%s: Comp. Cont6: ta_vol=%d\n",
									__func__, pca9481->ta_vol);
								/* Update TA target voltage */
								pca9481->ta_target_vol = pca9481->ta_vol;
								/* Send PD Message */
								mutex_lock(&pca9481->lock);
								pca9481->timer_id = TIMER_PDMSG_SEND;
								pca9481->timer_period = 0;
								mutex_unlock(&pca9481->lock);

								/* Set TA increment flag */
								pca9481->prev_inc = INC_TA_VOL;
							} else {
								/* It is possible that TA is in current limit mode */
								/* Increase TA current (50mA) */
								pca9481->ta_cur = pca9481->ta_cur + PD_MSG_TA_CUR_STEP;
								if (pca9481->ta_cur > pca9481->ta_max_cur)
									pca9481->ta_cur = pca9481->ta_max_cur;
								pr_info("%s: Comp. Cont7: ta_cur=%d\n",
									__func__, pca9481->ta_cur);
								/* Send PD Message */
								mutex_lock(&pca9481->lock);
								pca9481->timer_id = TIMER_PDMSG_SEND;
								pca9481->timer_period = 0;
								mutex_unlock(&pca9481->lock);

								/* Set TA increment flag */
								pca9481->prev_inc = INC_TA_CUR;
							}
						}
					} else {
						/* The previous increment is TA current, but input current does not increase */
						/* Try to increase TA voltage */
						/* Increase TA voltage (20mV) */
						pca9481->ta_vol = pca9481->ta_vol + PD_MSG_TA_VOL_STEP;
						if (pca9481->ta_vol > pca9481->ta_max_vol)
							pca9481->ta_vol = pca9481->ta_max_vol;
						pr_info("%s: Comp. Cont8: ta_vol=%d\n",
							__func__, pca9481->ta_vol);
						/* Update TA target voltage */
						pca9481->ta_target_vol = pca9481->ta_vol;
						/* Send PD Message */
						mutex_lock(&pca9481->lock);
						pca9481->timer_id = TIMER_PDMSG_SEND;
						pca9481->timer_period = 0;
						mutex_unlock(&pca9481->lock);

						/* Set TA increment flag */
						pca9481->prev_inc = INC_TA_VOL;
					}
				}
			}
		} else {
			/* IIN ADC is in valid range */
			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			pr_info("%s: Comp. End2(valid): ta_vol=%d, ta_cur=%d\n",
				__func__, pca9481->ta_vol, pca9481->ta_cur);
			/* Clear TA increment flag */
			pca9481->prev_inc = INC_NONE;
			/* Set timer */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_CHECK_CCMODE;
			pca9481->timer_period = CCMODE_CHECK_T;
			mutex_unlock(&pca9481->lock);
		}
	}

	/* Save previous iin adc */
	pca9481->prev_iin = iin;

	queue_delayed_work(pca9481->dc_wq,
						&pca9481->timer_work,
						msecs_to_jiffies(pca9481->timer_period));

	return 0;
}

/* Compensate TA current for constant power mode */
static int pca9481_set_ta_current_comp2(struct pca9481_charger *pca9481)
{
	int iin, ichg;
	unsigned int val;
	unsigned int iin_apdo;

	/* Read IIN ADC */
	iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
	/* Read ICHG ADC */
	ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);

	pr_info("%s: iin=%d, ichg=%d\n", __func__, iin, ichg);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9481->iin_cfg + IIN_CC_COMP_OFFSET)) {
		/* TA current is higher than the target input current */
		/* Compare TA current with IIN_CC - LOW_OFFSET */
		if (pca9481->ta_cur > pca9481->iin_cc - TA_CUR_LOW_OFFSET) {
			/* TA current is higher than IIN_CC - LOW_OFFSET */
			/* Assume that TA operation mode is CL mode, so decrease TA current */
			/* Decrease TA current (50mA) */
			pca9481->ta_cur = pca9481->ta_cur - PD_MSG_TA_CUR_STEP;
			pr_info("%s: Comp. Cont1: ta_cur=%d\n", __func__, pca9481->ta_cur);
		} else {
			/* TA current is already lower than IIN_CC - LOW_OFFSET */
			/* IIN_ADC is stiil in invalid range even though TA current is less than IIN_CC - LOW_OFFSET */
			/* TA has abnormal behavior */
			/* Decrease TA voltage (20mV) */
			pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
			pr_info("%s: Comp. Cont2: ta_vol=%d\n", __func__, pca9481->ta_vol);
			/* Update TA target voltage */
			pca9481->ta_target_vol = pca9481->ta_vol;
		}

		/* Send PD Message */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PDMSG_SEND;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
	} else if (iin < (pca9481->iin_cc - IIN_CC_COMP_OFFSET_CP)) {
		/* TA current is lower than the target input current */
		/* IIN_ADC < IIN_CC -20mA */
		if (pca9481->ta_vol == pca9481->ta_max_vol) {
			/* Check IIN_ADC < IIN_CC - 50mA */
			if (iin < (pca9481->iin_cc - IIN_CC_COMP_OFFSET)) {
				/* Compare the TA current with IIN_CC and maximum current of APDO */
				if ((pca9481->ta_cur >= (pca9481->iin_cc/pca9481->chg_mode)) ||
					(pca9481->ta_cur == pca9481->ta_max_cur)) {
					/* TA current is higher than IIN_CC or maximum TA current */
					/* Set new IIN_CC to IIN_CC - 50mA */
					pca9481->iin_cc = pca9481->iin_cc - IIN_CC_COMP_OFFSET;
					/* Set new TA_MAX_VOL to TA_MAX_PWR/IIN_CC */
					/* Adjust new IIN_CC with APDO resolution */
					iin_apdo = pca9481->iin_cc/PD_MSG_TA_CUR_STEP;
					iin_apdo = iin_apdo*PD_MSG_TA_CUR_STEP;
					val = pca9481->ta_max_pwr/(iin_apdo/pca9481->chg_mode/1000);	/* mV */
					val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
					val = val*PD_MSG_TA_VOL_STEP; /* uV */
					/* Set new TA_MAX_VOL */
					pca9481->ta_max_vol = MIN(val, TA_MAX_VOL*pca9481->chg_mode);
					/* Increase TA voltage(40mV) */
					pca9481->ta_vol = pca9481->ta_vol + PD_MSG_TA_VOL_STEP*2;
					if (pca9481->ta_vol > pca9481->ta_max_vol)
						pca9481->ta_vol = pca9481->ta_max_vol;
					pr_info("%s: Comp. Cont2: ta_vol=%d\n", __func__, pca9481->ta_vol);
					/* Update TA target voltage */
					pca9481->ta_target_vol = pca9481->ta_vol;

					/* Send PD Message */
					mutex_lock(&pca9481->lock);
					pca9481->timer_id = TIMER_PDMSG_SEND;
					pca9481->timer_period = 0;
					mutex_unlock(&pca9481->lock);
				} else {
					/* TA current is less than IIN_CC and not maximum current */
					/* Increase TA current (50mA) */
					pca9481->ta_cur = pca9481->ta_cur + PD_MSG_TA_CUR_STEP;
					if (pca9481->ta_cur > pca9481->ta_max_cur)
						pca9481->ta_cur = pca9481->ta_max_cur;
					pr_info("%s: Comp. Cont3: ta_cur=%d\n", __func__, pca9481->ta_cur);

					/* Send PD Message */
					mutex_lock(&pca9481->lock);
					pca9481->timer_id = TIMER_PDMSG_SEND;
					pca9481->timer_period = 0;
					mutex_unlock(&pca9481->lock);
				}
			} else {
				/* Wait for next current step compensation */
				/* IIN_CC - 50mA < IIN ADC < IIN_CC - 20mA */
				pr_info("%s: Comp.(wait): ta_vol=%d\n", __func__, pca9481->ta_vol);
				/* Set timer */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_CHECK_CCMODE;
				pca9481->timer_period = CCMODE_CHECK_T;
				mutex_unlock(&pca9481->lock);
			}
		} else {
			/* Increase TA voltage(40mV) */
			pca9481->ta_vol = pca9481->ta_vol + PD_MSG_TA_VOL_STEP*2;
			if (pca9481->ta_vol > pca9481->ta_max_vol)
				pca9481->ta_vol = pca9481->ta_max_vol;
			pr_info("%s: Comp. Cont4: ta_vol=%d\n", __func__, pca9481->ta_vol);
			/* Update TA target voltage */
			pca9481->ta_target_vol = pca9481->ta_vol;

			/* Send PD Message */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
		}
	} else {
		/* IIN ADC is in valid range */
		/* IIN_CC - 20mA < IIN ADC < IIN_CFG + 50mA */
		pr_info("%s: Comp. End(valid): ta_vol=%d\n", __func__, pca9481->ta_vol);
		/* Set timer */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_CHECK_CCMODE;
		pca9481->timer_period = CCMODE_CHECK_T;
		mutex_unlock(&pca9481->lock);
	}

	/* Save previous iin adc */
	pca9481->prev_iin = iin;

	queue_delayed_work(pca9481->dc_wq,
						&pca9481->timer_work,
						msecs_to_jiffies(pca9481->timer_period));

	return 0;
}

/* Compensate TA voltage for the target input current */
static int pca9481_set_ta_voltage_comp(struct pca9481_charger *pca9481)
{
	int iin, ichg;

	pr_info("%s: ======START=======\n", __func__);

	/* Read IIN ADC */
	iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
	/* Read ICHG ADC */
	ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);

	pr_info("%s: iin=%d, ichg=%d\n", __func__, iin, ichg);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9481->iin_cc + IIN_CC_COMP_OFFSET)) {
		/* TA current is higher than the target input current */
		/* Decrease TA voltage (20mV) */
		pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
		pr_info("%s: Comp. Cont1: ta_vol=%d\n", __func__, pca9481->ta_vol);

		/* Send PD Message */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PDMSG_SEND;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
	} else {
		if (iin < (pca9481->iin_cc - IIN_CC_COMP_OFFSET)) {
			/* TA current is lower than the target input current */
			/* Compare TA max voltage */
			if (pca9481->ta_vol == pca9481->ta_max_vol) {
				/* TA current is already the maximum voltage */
				pr_info("%s: Comp. End1(max TA vol): ta_vol=%d\n", __func__, pca9481->ta_vol);
				/* Set timer */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_CHECK_CCMODE;
				pca9481->timer_period = CCMODE_CHECK_T;
				mutex_unlock(&pca9481->lock);
			} else {
				/* Increase TA voltage (20mV) */
				pca9481->ta_vol = pca9481->ta_vol + PD_MSG_TA_VOL_STEP;
				pr_info("%s: Comp. Cont2: ta_vol=%d\n", __func__, pca9481->ta_vol);

				/* Send PD Message */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_PDMSG_SEND;
				pca9481->timer_period = 0;
				mutex_unlock(&pca9481->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			pr_info("%s: Comp. End(valid): ta_vol=%d\n", __func__, pca9481->ta_vol);
			/* Set timer */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_CHECK_CCMODE;
			pca9481->timer_period = CCMODE_CHECK_T;
			mutex_unlock(&pca9481->lock);
		}
	}

	queue_delayed_work(pca9481->dc_wq,
						&pca9481->timer_work,
						msecs_to_jiffies(pca9481->timer_period));

	return 0;
}

/* Compensate RX voltage for the target input current */
static int pca9481_set_rx_voltage_comp(struct pca9481_charger *pca9481)
{
	int iin, ichg;

	pr_info("%s: ======START=======\n", __func__);

	/* Read IIN ADC */
	iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
	/* Read ICHG ADC */
	ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);

	pr_info("%s: iin=%d, ichg=%d\n", __func__, iin, ichg);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9481->iin_cc + IIN_CC_COMP_OFFSET)) {
		/* RX current is higher than the target input current */
		/* Decrease RX voltage (100mV) */
		pca9481->ta_vol = pca9481->ta_vol - WCRX_VOL_STEP;
		pr_info("%s: Comp. Cont1: rx_vol=%d\n", __func__, pca9481->ta_vol);

		/* Set RX Voltage */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PDMSG_SEND;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
	} else {
		if (iin < (pca9481->iin_cc - IIN_CC_COMP_OFFSET)) {
			/* RX current is lower than the target input current */
			/* Compare RX max voltage */
			if (pca9481->ta_vol == pca9481->ta_max_vol) {
				/* TA current is already the maximum voltage */
				pr_info("%s: Comp. End1(max RX vol): rx_vol=%d\n", __func__, pca9481->ta_vol);
				/* Set timer */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_CHECK_CCMODE;
				pca9481->timer_period = CCMODE_CHECK_T;
				mutex_unlock(&pca9481->lock);
			} else {
				/* Increase RX voltage (100mV) */
				pca9481->ta_vol = pca9481->ta_vol + WCRX_VOL_STEP;
				pr_info("%s: Comp. Cont2: rx_vol=%d\n", __func__, pca9481->ta_vol);

				/* Set RX Voltage */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_PDMSG_SEND;
				pca9481->timer_period = 0;
				mutex_unlock(&pca9481->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			pr_info("%s: Comp. End(valid): rx_vol=%d\n", __func__, pca9481->ta_vol);
			/* Set timer */
			/* Check the current charging state */
			/* Set timer */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_CHECK_CCMODE;
			pca9481->timer_period = CCMODE_CHECK_T;
			mutex_unlock(&pca9481->lock);
		}
	}

	/* Set TA target voltage to TA voltage */
	pca9481->ta_target_vol = pca9481->ta_vol;

	queue_delayed_work(pca9481->dc_wq,
						&pca9481->timer_work,
						msecs_to_jiffies(pca9481->timer_period));

	return 0;
}

/* Set TA current for target current */
static int pca9481_adjust_ta_current(struct pca9481_charger *pca9481)
{
	int ret = 0;
	int vbat;
	unsigned int val;

	/* Set charging state to ADJUST_TACUR */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_ADJUST_TACUR);
#else
	pca9481->charging_state = DC_STATE_ADJUST_TACUR;
#endif

	if (pca9481->ta_cur == pca9481->iin_cc/pca9481->chg_mode) {
		/* finish sending PD message */
		/* Recover IIN_CC to the original value(new_iin) */
		pca9481->iin_cc = pca9481->new_iin;

		/* Clear req_new_iin */
		mutex_lock(&pca9481->lock);
		pca9481->req_new_iin = false;
		mutex_unlock(&pca9481->lock);

		pr_info("%s: adj. End, ta_cur=%d, ta_vol=%d, iin_cc=%d, chg_mode=%d\n",
				__func__, pca9481->ta_cur, pca9481->ta_vol, pca9481->iin_cc, pca9481->chg_mode);

		/* Go to return state  */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9481_set_charging_state(pca9481, pca9481->ret_state);
#else
		pca9481->charging_state = pca9481->ret_state;
#endif
		/* Set timer */
		mutex_lock(&pca9481->lock);
		if (pca9481->charging_state == DC_STATE_CC_MODE)
			pca9481->timer_id = TIMER_CHECK_CCMODE;
		else
			pca9481->timer_id = TIMER_CHECK_CVMODE;
		pca9481->timer_period = 1000;	/* Wait 1s */
		mutex_unlock(&pca9481->lock);
	} else {
		/* Compare new IIN with current IIN_CFG */
		if (pca9481->iin_cc > pca9481->iin_cfg) {
			/* New iin is higher than current iin_cfg */
			/* Update iin_cfg */
			pca9481->iin_cfg = pca9481->iin_cc;
			/* Set IIN_CFG to new IIN */
			ret = pca9481_set_input_current(pca9481, pca9481->iin_cc);
			if (ret < 0)
				goto error;

			/* Set ICHG_CFG to enough high value */
			if (pca9481->ichg_cfg < 2*pca9481->iin_cfg)
				pca9481->ichg_cfg = 2*pca9481->iin_cfg;
			ret = pca9481_set_charging_current(pca9481, pca9481->ichg_cfg);
			if (ret < 0)
				goto error;

			/* Clear Request flag */
			mutex_lock(&pca9481->lock);
			pca9481->req_new_iin = false;
			mutex_unlock(&pca9481->lock);

			/* Set new TA voltage and current */
			/* Read VBAT ADC */
			vbat = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);

			/* Calculate new TA maximum current and voltage that used in the direct charging */
			/* Set IIN_CC to MIN[IIN, TA_MAX_CUR*chg_mode]*/
			pca9481->iin_cc = MIN(pca9481->iin_cfg, pca9481->ta_max_cur*pca9481->chg_mode);
			/* Set the current IIN_CC to iin_cfg for recovering it after resolution adjustment */
			pca9481->iin_cfg = pca9481->iin_cc;
			/* Calculate new TA max voltage */
			/* Adjust IIN_CC with APDO resoultion(50mA) - It will recover to the original value after max voltage calculation */
			val = pca9481->iin_cc/(PD_MSG_TA_CUR_STEP*pca9481->chg_mode);
			pca9481->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9481->chg_mode);
			/* Set TA_MAX_VOL to MIN[TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
			val = pca9481->ta_max_pwr/(pca9481->iin_cc/pca9481->chg_mode/1000);	/* mV */
			val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
			val = val*PD_MSG_TA_VOL_STEP; /* uV */
			pca9481->ta_max_vol = MIN(val, TA_MAX_VOL*pca9481->chg_mode);

			/* Set TA voltage to MAX[TA_MIN_VOL_PRESET*chg_mode, (2*VBAT_ADC*chg_mode + offset)] */
			pca9481->ta_vol = max(TA_MIN_VOL_PRESET*pca9481->chg_mode, (2*vbat*pca9481->chg_mode + TA_VOL_PRE_OFFSET));
			val = pca9481->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
			pca9481->ta_vol = val*PD_MSG_TA_VOL_STEP;
			/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL] */
			pca9481->ta_vol = MIN(pca9481->ta_vol, pca9481->ta_max_vol);
			/* Set TA current to IIN_CC */
			pca9481->ta_cur = pca9481->iin_cc/pca9481->chg_mode;
			/* Recover IIN_CC to the original value(iin_cfg) */
			pca9481->iin_cc = pca9481->iin_cfg;

			pr_info("%s: New IIN, ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
				__func__, pca9481->ta_max_vol, pca9481->ta_max_cur, pca9481->ta_max_pwr, pca9481->iin_cc, pca9481->chg_mode);

			/* Clear previous IIN ADC */
			pca9481->prev_iin = 0;
			/* Clear TA increment flag */
			pca9481->prev_inc = INC_NONE;

			/* Send PD Message and go to Adjust CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_charging_state(pca9481, DC_STATE_ADJUST_CC);
#else
			pca9481->charging_state = DC_STATE_ADJUST_CC;
#endif
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
		} else {
			/* Calculate new TA max voltage */
			/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
			val = pca9481->iin_cc/(PD_MSG_TA_CUR_STEP*pca9481->chg_mode);
			pca9481->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9481->chg_mode);
			/* Set TA_MAX_VOL to MIN[TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
			val = pca9481->ta_max_pwr/(pca9481->iin_cc/pca9481->chg_mode/1000);	/* mV */
			val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
			val = val*PD_MSG_TA_VOL_STEP; /* uV */
			pca9481->ta_max_vol = MIN(val, TA_MAX_VOL*pca9481->chg_mode);
			/* Recover IIN_CC to the original value(new_iin) */
			pca9481->iin_cc = pca9481->new_iin;

			/* Set TA voltage to TA target voltage */
			pca9481->ta_vol = pca9481->ta_target_vol;
			/* Adjust IIN_CC with APDO resoultion(50mA) - It will recover to the original value after sending PD message */
			val = pca9481->iin_cc/PD_MSG_TA_CUR_STEP;
			pca9481->iin_cc = val*PD_MSG_TA_CUR_STEP;
			/* Set TA current to IIN_CC */
			pca9481->ta_cur = pca9481->iin_cc/pca9481->chg_mode;

			pr_info("%s: adj. cont, ta_cur=%d, ta_vol=%d, ta_max_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9481->ta_cur, pca9481->ta_vol, pca9481->ta_max_vol, pca9481->iin_cc, pca9481->chg_mode);

			/* Send PD Message */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
		}
	}
	queue_delayed_work(pca9481->dc_wq,
						&pca9481->timer_work,
						msecs_to_jiffies(pca9481->timer_period));

error:
	return ret;
}

/* Set TA voltage for target current */
static int pca9481_adjust_ta_voltage(struct pca9481_charger *pca9481)
{
	int iin;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_ADJUST_TAVOL);
#else
	pca9481->charging_state = DC_STATE_ADJUST_TAVOL;
#endif
	/* Read IIN ADC */
	iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);

	/* Compare IIN ADC with targer input current */
	if (iin > (pca9481->iin_cc + PD_MSG_TA_CUR_STEP)) {
		/* TA current is higher than the target input current */
		/* Decrease TA voltage (20mV) */
		pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;

		pr_info("%s: adj. Cont1, ta_vol=%d\n", __func__, pca9481->ta_vol);

		/* Send PD Message */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PDMSG_SEND;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
	} else {
		if (iin < (pca9481->iin_cc - PD_MSG_TA_CUR_STEP)) {
			/* TA current is lower than the target input current */
			/* Compare TA max voltage */
			if (pca9481->ta_vol == pca9481->ta_max_vol) {
				/* TA current is already the maximum voltage */
				/* Clear req_new_iin */
				mutex_lock(&pca9481->lock);
				pca9481->req_new_iin = false;
				mutex_unlock(&pca9481->lock);
				/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9481_set_charging_state(pca9481, pca9481->ret_state);
#else
				pca9481->charging_state = pca9481->ret_state;
#endif
				pr_info("%s: adj. End1, ta_cur=%d, ta_vol=%d, iin_cc=%d, chg_mode=%d\n",
						__func__, pca9481->ta_cur, pca9481->ta_vol, pca9481->iin_cc, pca9481->chg_mode);

				/* Go to return state  */
				/* Set timer */
				mutex_lock(&pca9481->lock);
				if (pca9481->charging_state == DC_STATE_CC_MODE)
					pca9481->timer_id = TIMER_CHECK_CCMODE;
				else
					pca9481->timer_id = TIMER_CHECK_CVMODE;
				pca9481->timer_period = 1000;	/* Wait 1000ms */
				mutex_unlock(&pca9481->lock);
			} else {
				/* Increase TA voltage (20mV) */
				pca9481->ta_vol = pca9481->ta_vol + PD_MSG_TA_VOL_STEP;

				pr_info("%s: adj. Cont2, ta_vol=%d\n", __func__, pca9481->ta_vol);

				/* Send PD Message */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_PDMSG_SEND;
				pca9481->timer_period = 0;
				mutex_unlock(&pca9481->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* Clear req_new_iin */
			mutex_lock(&pca9481->lock);
			pca9481->req_new_iin = false;
			mutex_unlock(&pca9481->lock);

			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_charging_state(pca9481, pca9481->ret_state);
#else
			pca9481->charging_state = pca9481->ret_state;
#endif
			pr_info("%s: adj. End2, ta_cur=%d, ta_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9481->ta_cur, pca9481->ta_vol, pca9481->iin_cc, pca9481->chg_mode);

			/* Go to return state  */
			/* Set timer */
			mutex_lock(&pca9481->lock);
			if (pca9481->charging_state == DC_STATE_CC_MODE)
				pca9481->timer_id = TIMER_CHECK_CCMODE;
			else
				pca9481->timer_id = TIMER_CHECK_CVMODE;
			pca9481->timer_period = 1000;	/* Wait 1000ms */
			mutex_unlock(&pca9481->lock);
		}
	}
	queue_delayed_work(pca9481->dc_wq,
					&pca9481->timer_work,
					msecs_to_jiffies(pca9481->timer_period));

	return 0;
}


/* Set RX voltage for target current */
static int pca9481_adjust_rx_voltage(struct pca9481_charger *pca9481)
{
	int iin;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_ADJUST_TAVOL);
#else
	pca9481->charging_state = DC_STATE_ADJUST_TAVOL;
#endif
	/* Read IIN ADC */
	iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);

	/* Compare IIN ADC with targer input current */
	if (iin > (pca9481->iin_cc + IIN_CC_COMP_OFFSET)) {
		/* RX current is higher than the target input current */
		/* Decrease RX voltage (100mV) */
		pca9481->ta_vol = pca9481->ta_vol - WCRX_VOL_STEP;

		pr_info("%s: adj. Cont1, rx_vol=%d\n", __func__, pca9481->ta_vol);

		/* Set RX voltage */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PDMSG_SEND;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
	} else {
		if (iin < (pca9481->iin_cc - IIN_CC_COMP_OFFSET)) {
			/* RX current is lower than the target input current */
			/* Compare RX max voltage */
			if (pca9481->ta_vol == pca9481->ta_max_vol) {
				/* RX current is already the maximum voltage */
				/* Clear req_new_iin */
				mutex_lock(&pca9481->lock);
				pca9481->req_new_iin = false;
				mutex_unlock(&pca9481->lock);

				pr_info("%s: adj. End1(max vol), rx_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9481->ta_vol, pca9481->iin_cc, pca9481->chg_mode);

				/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9481_set_charging_state(pca9481, pca9481->ret_state);
#else
				pca9481->charging_state = pca9481->ret_state;
#endif
				/* Go to return state  */
				/* Set timer */
				mutex_lock(&pca9481->lock);
				if (pca9481->charging_state == DC_STATE_CC_MODE)
					pca9481->timer_id = TIMER_CHECK_CCMODE;
				else
					pca9481->timer_id = TIMER_CHECK_CVMODE;
				pca9481->timer_period = 1000;	/* Wait 1000ms */
				mutex_unlock(&pca9481->lock);
			} else {
				/* Increase RX voltage (100mV) */
				pca9481->ta_vol = pca9481->ta_vol + WCRX_VOL_STEP;
				if (pca9481->ta_vol > pca9481->ta_max_vol)
					pca9481->ta_vol = pca9481->ta_max_vol;

				pr_info("%s: adj. Cont2, rx_vol=%d\n", __func__, pca9481->ta_vol);

				/* Set RX voltage */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_PDMSG_SEND;
				pca9481->timer_period = 0;
				mutex_unlock(&pca9481->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* Clear req_new_iin */
			mutex_lock(&pca9481->lock);
			pca9481->req_new_iin = false;
			mutex_unlock(&pca9481->lock);

			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA  */
			pr_info("%s: adj. End2(valid), rx_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9481->ta_vol, pca9481->iin_cc, pca9481->chg_mode);

			/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_charging_state(pca9481, pca9481->ret_state);
#else
			pca9481->charging_state = pca9481->ret_state;
#endif
			/* Go to return state  */
			/* Set timer */
			mutex_lock(&pca9481->lock);
			if (pca9481->charging_state == DC_STATE_CC_MODE)
				pca9481->timer_id = TIMER_CHECK_CCMODE;
			else
				pca9481->timer_id = TIMER_CHECK_CVMODE;
			pca9481->timer_period = 1000;	/* Wait 1000ms */
			mutex_unlock(&pca9481->lock);
		}
	}
	queue_delayed_work(pca9481->dc_wq,
					&pca9481->timer_work,
					msecs_to_jiffies(pca9481->timer_period));

	return 0;
}

/* Set TA voltage for bypass mode */
static int pca9481_set_bypass_ta_voltage_by_soc(struct pca9481_charger *pca9481, int delta_soc)
{
	int ret = 0;
	unsigned int prev_ta_vol = pca9481->ta_vol;

	if (delta_soc < 0) { // increase soc (soc_now - ref_soc)
		pca9481->ta_vol += PD_MSG_TA_VOL_STEP;
	} else if (delta_soc > 0) { // decrease soc (soc_now - ref_soc)
		pca9481->ta_vol -= PD_MSG_TA_VOL_STEP;
	} else {
		pr_info("%s: abnormal delta_soc=%d\n", __func__, delta_soc);
		return -1;
	}

	pr_info("%s: delta_soc=%d, prev_ta_vol=%d, ta_vol=%d, ta_cur=%d\n",
		__func__, delta_soc, prev_ta_vol, pca9481->ta_vol, pca9481->ta_cur);

	/* Send PD Message */
	mutex_lock(&pca9481->lock);
	pca9481->timer_id = TIMER_PDMSG_SEND;
	pca9481->timer_period = 0;
	mutex_unlock(&pca9481->lock);
	schedule_delayed_work(&pca9481->timer_work, msecs_to_jiffies(pca9481->timer_period));

	return ret;
}

/* Set TA current for bypass mode */
static int pca9481_set_bypass_ta_current(struct pca9481_charger *pca9481)
{
	int ret = 0;
	unsigned int val;

	/* Set charging state to BYPASS mode state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_BYPASS_MODE);
#else
	pca9481->charging_state = DC_STATE_BYPASS_MODE;
#endif
	pr_info("%s: new_iin=%d\n", __func__, pca9481->new_iin);

	/* Set IIN_CFG to new_IIN */
	pca9481->iin_cfg = pca9481->new_iin;
	pca9481->iin_cc = pca9481->new_iin;
	ret = pca9481_set_input_current(pca9481, pca9481->iin_cc);
	if (ret < 0)
		goto error;

	/* Clear Request flag */
	mutex_lock(&pca9481->lock);
	pca9481->req_new_iin = false;
	mutex_unlock(&pca9481->lock);

	/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
	val = pca9481->iin_cc/PD_MSG_TA_CUR_STEP;
	pca9481->iin_cc = val*PD_MSG_TA_CUR_STEP;
	/* Set TA current to IIN_CC */
	pca9481->ta_cur = pca9481->iin_cc/pca9481->chg_mode;

	pr_info("%s: ta_cur=%d, ta_vol=%d\n", __func__, pca9481->ta_cur, pca9481->ta_vol);

	/* Recover IIN_CC to the original value(new_iin) */
	pca9481->iin_cc = pca9481->new_iin;

	/* Send PD Message */
	mutex_lock(&pca9481->lock);
	pca9481->timer_id = TIMER_PDMSG_SEND;
	pca9481->timer_period = 0;
	mutex_unlock(&pca9481->lock);
	queue_delayed_work(pca9481->dc_wq,
				&pca9481->timer_work,
				msecs_to_jiffies(pca9481->timer_period));

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set TA voltage for bypass mode */
static int pca9481_set_bypass_ta_voltage(struct pca9481_charger *pca9481)
{
	int ret = 0;
	unsigned int val;
	int vbat;

	/* Set charging state to BYPASS mode state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_BYPASS_MODE);
#else
	pca9481->charging_state = DC_STATE_BYPASS_MODE;
#endif
	pr_info("%s: new_vfloat=%d\n", __func__, pca9481->new_vfloat);

	/* Set VFLOAT to new vfloat */
	pca9481->vfloat = pca9481->new_vfloat;
	ret = pca9481_set_vfloat(pca9481, pca9481->vfloat);
	if (ret < 0)
		goto error;

	/* Clear Request flag */
	mutex_lock(&pca9481->lock);
	pca9481->req_new_vfloat = false;
	mutex_unlock(&pca9481->lock);


	/* It needs to optimize TA voltage as calculating TA voltage with battery voltage later */
	/* Read VBAT ADC */
	vbat = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);

	/* Check DC mode */
	if (pca9481->dc_mode == PTM_1TO1) {
		/* Set TA voltage to VBAT_ADC + Offset */
		val = vbat + TA_VOL_OFFSET_1TO1_BYPASS;
	} else {
		/* Set TA voltage to 2*VBAT_ADC + Offset */
		val = 2 * vbat + TA_VOL_OFFSET_2TO1_BYPASS;
	}
	pca9481->ta_vol = val;

	pr_info("%s: vbat=%d, ta_vol=%d, ta_cur=%d\n",
			__func__, vbat, pca9481->ta_vol, pca9481->ta_cur);

	/* Send PD Message */
	mutex_lock(&pca9481->lock);
	pca9481->timer_id = TIMER_PDMSG_SEND;
	pca9481->timer_period = 0;
	mutex_unlock(&pca9481->lock);
	queue_delayed_work(pca9481->dc_wq,
				&pca9481->timer_work,
				msecs_to_jiffies(pca9481->timer_period));

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set new input current  */
static int pca9481_set_new_iin(struct pca9481_charger *pca9481)
{
	int ret = 0;

	pr_info("%s: new_iin=%d\n", __func__, pca9481->new_iin);

	/* Check current DC mode */
	if ((pca9481->dc_mode == PTM_1TO1) ||
		(pca9481->dc_mode == PTM_2TO1)) {
		/* DC mode is bypass mode */
		/* Set new iin for bypass mode */
		ret = pca9481_set_bypass_ta_current(pca9481);
	} else {
		/* DC mode is normal mode */
		/* Set new IIN to IIN_CC */
		pca9481->iin_cc = pca9481->new_iin;
		/* Save return state */
		pca9481->ret_state = pca9481->charging_state;

		/* Check the TA type first */
		if (pca9481->ta_type == TA_TYPE_WIRELESS) {
			/* Wireless Charger is connected */
			ret = pca9481_adjust_rx_voltage(pca9481);
		} else {
			/* USBPD TA is connected */
			/* Check new IIN with the minimum TA current */
			if (pca9481->iin_cc < (TA_MIN_CUR * pca9481->chg_mode)) {
				/* Set the TA current to TA_MIN_CUR(1.0A) */
				pca9481->ta_cur = TA_MIN_CUR;
				/* Need to control TA voltage for request current */
				ret = pca9481_adjust_ta_voltage(pca9481);
			} else {
				/* Need to control TA current for request current */
				ret = pca9481_adjust_ta_current(pca9481);
			}
		}
	}

	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}


/* Set new float voltage */
static int pca9481_set_new_vfloat(struct pca9481_charger *pca9481)
{
	int ret = 0;
	int vbat;
	unsigned int val;

	/* Check current DC mode */
	if ((pca9481->dc_mode == PTM_1TO1) ||
		(pca9481->dc_mode == PTM_2TO1)) {
		/* DC mode is bypass mode */
		/* Set new vfloat for bypass mode */
		ret = pca9481_set_bypass_ta_voltage(pca9481);
	} else {
		/* DC mode is normal mode */
		/* Read VBAT ADC */
		vbat = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);

		/* Compare the new VBAT with the current VBAT */
		if (pca9481->new_vfloat > vbat) {
			/* cancel delayed_work */
			cancel_delayed_work(&pca9481->timer_work);

			/* Set VFLOAT to new vfloat */
			pca9481->vfloat = pca9481->new_vfloat;
			ret = pca9481_set_vfloat(pca9481, pca9481->vfloat);
			if (ret < 0)
				goto error;
			/* Set IIN_CFG to the current IIN_CC */
			/* save the current iin_cc in iin_cfg */
			pca9481->iin_cfg = pca9481->iin_cc;
			pca9481->iin_cfg = MIN(pca9481->iin_cfg, pca9481->ta_max_cur*pca9481->chg_mode);
			ret = pca9481_set_input_current(pca9481, pca9481->iin_cfg);
			if (ret < 0)
				goto error;

			pca9481->iin_cc = pca9481->iin_cfg;

			/* Set ICHG_CFG to enough high value */
			if (pca9481->ichg_cfg < 2*pca9481->iin_cfg)
				pca9481->ichg_cfg = 2*pca9481->iin_cfg;
			ret = pca9481_set_charging_current(pca9481, pca9481->ichg_cfg);
			if (ret < 0)
				goto error;

			/* Clear req_new_vfloat */
			mutex_lock(&pca9481->lock);
			pca9481->req_new_vfloat = false;
			mutex_unlock(&pca9481->lock);

			/* Check the TA type */
			if (pca9481->ta_type == TA_TYPE_WIRELESS) {
				/* Wireless Charger is connected */
				/* Set RX voltage to MAX[TA_MIN_VOL_PRESET*chg_mode, (2*VBAT_ADC*chg_mode + offset)] */
				pca9481->ta_vol = max(TA_MIN_VOL_PRESET*pca9481->chg_mode, (2*vbat*pca9481->chg_mode + TA_VOL_PRE_OFFSET));
				val = pca9481->ta_vol/WCRX_VOL_STEP;	/* RX voltage resolution is 100mV */
				pca9481->ta_vol = val*WCRX_VOL_STEP;
				/* Set RX voltage to MIN[RX voltage, RX_MAX_VOL] */
				pca9481->ta_vol = MIN(pca9481->ta_vol, pca9481->ta_max_vol);

				pr_info("%s: New VFLOAT, rx_max_vol=%d, rx_vol=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9481->ta_max_vol, pca9481->ta_vol, pca9481->iin_cc, pca9481->chg_mode);
			} else {
				/* USBPD TA is connected */
				/* Calculate new TA maximum voltage that used in the direct charging */
				/* Calculate new TA max voltage */
				/* Adjust IIN_CC with APDO resoultion(50mA) - It will recover to the original value after max voltage calculation */
				val = pca9481->iin_cc/(PD_MSG_TA_CUR_STEP*pca9481->chg_mode);
				pca9481->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9481->chg_mode);
				/* Set TA_MAX_VOL to MIN[TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
				val = pca9481->ta_max_pwr/(pca9481->iin_cc/pca9481->chg_mode/1000); /* mV */
				val = val*1000/PD_MSG_TA_VOL_STEP;	/* uV */
				val = val*PD_MSG_TA_VOL_STEP; /* Adjust values with APDO resolution(20mV) */
				pca9481->ta_max_vol = MIN(val, TA_MAX_VOL*pca9481->chg_mode);

				/* Set TA voltage to MAX[TA_MIN_VOL_PRESET*chg_mode, (2*VBAT_ADC*chg_mode + offset)] */
				pca9481->ta_vol = max(TA_MIN_VOL_PRESET*pca9481->chg_mode, (2*vbat*pca9481->chg_mode + TA_VOL_PRE_OFFSET));
				val = pca9481->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
				pca9481->ta_vol = val*PD_MSG_TA_VOL_STEP;
				/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL] */
				pca9481->ta_vol = MIN(pca9481->ta_vol, pca9481->ta_max_vol);
				/* Set TA current to IIN_CC */
				pca9481->ta_cur = pca9481->iin_cc/pca9481->chg_mode;
				/* Recover IIN_CC to the original value(iin_cfg) */
				pca9481->iin_cc = pca9481->iin_cfg;

				pr_info("%s: New VFLOAT, ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
					__func__, pca9481->ta_max_vol, pca9481->ta_max_cur, pca9481->ta_max_pwr, pca9481->iin_cc, pca9481->chg_mode);
			}

			/* Clear previous IIN ADC */
			pca9481->prev_iin = 0;
			/* Clear TA increment flag */
			pca9481->prev_inc = INC_NONE;

			/* Send PD Message and go to Adjust CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_charging_state(pca9481, DC_STATE_ADJUST_CC);
#else
			pca9481->charging_state = DC_STATE_ADJUST_CC;
#endif
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
		} else {
			/* The new VBAT is lower than the current VBAT */
			/* return invalid error */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pr_err("## %s: New vfloat(%duA) is lower than VBAT ADC(%duA)\n",
				__func__, pca9481->new_vfloat, vbat);
#else
			pr_err("%s: New vfloat is lower than VBAT ADC\n", __func__);
#endif
			ret = -EINVAL;
		}
	}

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set new direct charging mode */
static int pca9481_set_new_dc_mode(struct pca9481_charger *pca9481)
{
	int ret = 0;
	unsigned int val;
	int vbat;

	pr_info("%s: ======START=======\n", __func__);

	/* Read VBAT ADC */
	vbat = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);

	/* Check new dc mode */
	switch (pca9481->new_dc_mode) {
	case PTM_1TO1:
	case PTM_2TO1:
		/* Change normal mode to 1:1 or 2:1 bypass mode */
		/* Check current dc mode */
		if ((pca9481->dc_mode == PTM_1TO1) ||
			(pca9481->dc_mode == PTM_2TO1)) {
			/* TA voltage already changed to 1:1 or 2:1 mode */
			/* Disable reverse current detection */
			val = 0;
			ret = pca9481_update_reg(pca9481, PCA9481_REG_RCP_CNTL,
									PCA9481_BIT_RCP_EN, val);
			if (ret < 0)
				goto error;

			/* Set FSW_CFG to fsw_cfg_byp */
			val = PCA9481_FSW_CFG(pca9481->pdata->fsw_cfg_byp);
			ret = pca9481_write_reg(pca9481, PCA9481_REG_SC_CNTL_0, val);
			if (ret < 0)
				goto error;
			pr_info("%s: New DC mode, BYP mode=%d, sw_freq=%dkHz\n",
					__func__, pca9481->dc_mode, pca9481->pdata->fsw_cfg_byp);

			/* Enable Charging - recover dc as 1:1 or 2:1 bypass mode */
			ret = pca9481_set_charging(pca9481, true);
			if (ret < 0)
				goto error;

			/* Clear request flag */
			mutex_lock(&pca9481->lock);
			pca9481->req_new_dc_mode = false;
			mutex_unlock(&pca9481->lock);

			pr_info("%s: New DC mode, Normal->BYP mode(%d) done\n", __func__, pca9481->dc_mode);

			/* Wait 500ms and go to bypass state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_charging_state(pca9481, DC_STATE_BYPASS_MODE);
#else
			pca9481->charging_state = DC_STATE_BYPASS_MODE;
#endif
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_CHECK_BYPASSMODE;
			pca9481->timer_period = BYPASS_WAIT_T;	/* 200ms */
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
		} else {
			/* DC mode is normal mode */
			/* TA voltage is not changed to 1:1 or 2:1 bypass mode yet */
			/* Disable charging */
			ret = pca9481_set_charging(pca9481, false);
			if (ret < 0)
				goto error;
			/* Set dc mode to new dc mode, 1:1 or 2:1 bypass mode */
			pca9481->dc_mode = pca9481->new_dc_mode;
			if (pca9481->dc_mode == PTM_2TO1) {
				/* Set TA voltage to 2:1 bypass voltage */
				pca9481->ta_vol = 2*vbat + TA_VOL_OFFSET_2TO1_BYPASS;
				pr_info("%s: New DC mode, Normal->2:1BYP, ta_vol=%d, ta_cur=%d\n",
						__func__, pca9481->ta_vol, pca9481->ta_cur);
			} else {
				/* Set TA voltage to 1:1 voltage */
				pca9481->ta_vol = vbat + TA_VOL_OFFSET_1TO1_BYPASS;
				pr_info("%s: New DC mode, Normal->1:1BYP, ta_vol=%d, ta_cur=%d\n",
						__func__, pca9481->ta_vol, pca9481->ta_cur);
			}

			/* Send PD Message and go to dcmode change state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_charging_state(pca9481, DC_STATE_DCMODE_CHANGE);
#else
			pca9481->charging_state = DC_STATE_DCMODE_CHANGE;
#endif
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
		}
		break;

	case PTM_NONE:
		/* Change bypass mode to normal mode */
		/* Disable charging */
		ret = pca9481_set_charging(pca9481, false);
		if (ret < 0)
			goto error;

		/* Enable reverse current detection */
		val = PCA9481_BIT_RCP_EN;
		ret = pca9481_update_reg(pca9481, PCA9481_REG_RCP_CNTL,
								PCA9481_BIT_RCP_EN, val);
		if (ret < 0)
			goto error;

		/* Set dc mode to new dc mode, normal mode */
		pca9481->dc_mode = pca9481->new_dc_mode;
		/* Clear request flag */
		mutex_lock(&pca9481->lock);
		pca9481->req_new_dc_mode = false;
		mutex_unlock(&pca9481->lock);
		pr_info("%s: New DC mode, BYP->Normal\n", __func__);

		/* Go to DC_STATE_PRESET_DC */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PRESET_DC;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
		break;

	default:
		ret = -EINVAL;
		pr_info("%s: New DC mode, Invalid mode=%d\n", __func__, pca9481->new_dc_mode);
		break;
	}

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging Adjust CC MODE control */
static int pca9481_charge_adjust_ccmode(struct pca9481_charger *pca9481)
{
	int iin, ichg, vbatt, ccmode;
	int val;
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_ADJUST_CC);
#else
	pca9481->charging_state = DC_STATE_ADJUST_CC;
#endif

	ret = pca9481_check_error(pca9481);
	if (ret != 0)
		goto error; // This is not active mode.
	/* Check the status */
	ccmode = pca9481_check_dcmode_status(pca9481);
	if (ccmode < 0) {
		ret = ccmode;
		goto error;
	}

	switch (ccmode) {
	case DCMODE_IIN_LOOP:
	case DCMODE_CHG_LOOP:
		/* Read IIN ADC */
		iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
		/* Read ICHG ADC */
		ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);
		/* Read VBAT ADC */
		vbatt = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);

		/* Check the TA type first */
		if (pca9481->ta_type == TA_TYPE_WIRELESS) {
			/* Decrease RX voltage (100mV) */
			pca9481->ta_vol = pca9481->ta_vol - WCRX_VOL_STEP;
			pr_info("%s: CC adjust End(LOOP): iin=%d, ichg=%d, vbatt=%d, rx_vol=%d\n",
					__func__, iin, ichg, vbatt, pca9481->ta_vol);

			/* Set TA target voltage to TA voltage */
			pca9481->ta_target_vol = pca9481->ta_vol;
			/* Clear TA increment flag */
			pca9481->prev_inc = INC_NONE;
			/* Send PD Message and then go to CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_charging_state(pca9481, DC_STATE_CC_MODE);
#else
			pca9481->charging_state = DC_STATE_CC_MODE;
#endif
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
		} else {
			/* Check TA current */
			if ((pca9481->ta_cur > TA_MIN_CUR) &&
				(pca9481->ta_ctrl == TA_CTRL_CL_MODE)) {
				/* TA current is higher than 1.0A */
				/* Decrease TA current (50mA) */
				pca9481->ta_cur = pca9481->ta_cur - PD_MSG_TA_CUR_STEP;

				/* TA target voltage = TA voltage + (VFLOAT - VBAT_ADC)*2*CHG_mode + 100mV */
				val = pca9481->ta_vol + (pca9481->vfloat - vbatt)*2*pca9481->chg_mode + 100000;
				val = val/PD_MSG_TA_VOL_STEP;
				pca9481->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
				if (pca9481->ta_target_vol > pca9481->ta_max_vol)
					pca9481->ta_target_vol = pca9481->ta_max_vol;
				pr_info("%s: CC adjust End(LOOP): iin=%d, ichg=%d, vbatt=%d, ta_cur=%d, ta_vol=%d, ta_target_vol=%d\n",
						__func__, iin, ichg, vbatt, pca9481->ta_cur, pca9481->ta_vol, pca9481->ta_target_vol);
				/* Clear TA increment flag */
				pca9481->prev_inc = INC_NONE;
				/* Go to Start CC mode */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_ENTER_CCMODE;
				pca9481->timer_period = 0;
				mutex_unlock(&pca9481->lock);
			} else {
				/* Decrease TA voltage (20mV) */
				pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;

				/* Set TA target voltage to TA voltage */
				pca9481->ta_target_vol = pca9481->ta_vol;
				/* Clear TA increment flag */
				pca9481->prev_inc = INC_NONE;
				/* Send PD Message and then go to CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9481_set_charging_state(pca9481, DC_STATE_CC_MODE);
#else
				pca9481->charging_state = DC_STATE_CC_MODE;
#endif
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_PDMSG_SEND;
				pca9481->timer_period = 0;
				mutex_unlock(&pca9481->lock);

				pr_info("%s: CC adjust End(LOOP): iin=%d, ichg=%d, vbatt=%d, ta_cur=%d, ta_vol=%d\n",
						__func__, iin, ichg, vbatt, pca9481->ta_cur, pca9481->ta_vol);
			}
		}
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
		break;

	case DCMODE_VFLT_LOOP:
		/* Read IIN ADC */
		iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
		/* Read ICHG ADC */
		ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);
		/* Read VBAT ADC */
		vbatt = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);
		pr_info("%s: CC adjust End(VFLOAT): vbatt=%d, iin=%d, ichg=%d, ta_vol=%d\n",
				__func__, vbatt, iin, ichg, pca9481->ta_vol);

		/* Save TA target voltage*/
		pca9481->ta_target_vol = pca9481->ta_vol;
		/* Clear TA increment flag */
		pca9481->prev_inc = INC_NONE;
		/* Go to Pre-CV mode */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_ENTER_CVMODE;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
		break;

	case DCMODE_LOOP_INACTIVE:
		/* Check IIN ADC with IIN */
		iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
		/* Check ICHG ADC */
		ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);
		/* Read VBAT ADC */
		vbatt = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);

		/* Check the TA type first */
		if (pca9481->ta_type == TA_TYPE_WIRELESS) {
			/* IIN_ADC > IIN_CC -20mA ? */
			if (iin > (pca9481->iin_cc - IIN_ADC_OFFSET)) {
				/* Input current is already over IIN_CC */
				/* End RX voltage adjustment */
				/* change charging state to CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9481_set_charging_state(pca9481, DC_STATE_CC_MODE);
#else
				pca9481->charging_state = DC_STATE_CC_MODE;
#endif
				pr_info("%s: CC adjust End: iin=%d, ichg=%d, vbatt=%d, rx_vol=%d\n",
						__func__, iin, ichg, vbatt, pca9481->ta_vol);

				/* Save TA target voltage*/
				pca9481->ta_target_vol = pca9481->ta_vol;
				/* Clear TA increment flag */
				pca9481->prev_inc = INC_NONE;
				/* Go to CC mode */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_CHECK_CCMODE;
				pca9481->timer_period = 0;
				mutex_unlock(&pca9481->lock);
			} else {
				/* Check RX voltage */
				if (pca9481->ta_vol == pca9481->ta_max_vol) {
					/* RX voltage is already max value */
					pr_info("%s: CC adjust End: MAX value, iin=%d, ichg=%d, vbatt=%d, rx_vol=%d\n",
							__func__, iin, ichg, vbatt, pca9481->ta_vol);

					/* Save TA target voltage*/
					pca9481->ta_target_vol = pca9481->ta_vol;
					/* Clear TA increment flag */
					pca9481->prev_inc = INC_NONE;
					/* Go to CC mode */
					mutex_lock(&pca9481->lock);
					pca9481->timer_id = TIMER_CHECK_CCMODE;
					pca9481->timer_period = 0;
					mutex_unlock(&pca9481->lock);
				} else {
					/* Try to increase RX voltage(100mV) */
					pca9481->ta_vol = pca9481->ta_vol + WCRX_VOL_STEP;
					if (pca9481->ta_vol > pca9481->ta_max_vol)
						pca9481->ta_vol = pca9481->ta_max_vol;
					pr_info("%s: CC adjust Cont: iin=%d, ichg=%d, vbatt=%d, rx_vol=%d\n",
							__func__, iin, ichg, vbatt, pca9481->ta_vol);
					/* Set RX voltage */
					mutex_lock(&pca9481->lock);
					pca9481->timer_id = TIMER_PDMSG_SEND;
					pca9481->timer_period = 0;
					mutex_unlock(&pca9481->lock);
				}
			}
		} else {
			/* USBPD TA is connected */

			/* IIN_ADC > IIN_CC -20mA ? */
			if (iin > (pca9481->iin_cc - IIN_ADC_OFFSET)) {
				if (pca9481->ta_ctrl == TA_CTRL_CL_MODE) {
					/* TA control method is CL mode */
					/* Input current is already over IIN_CC */
					/* End TA voltage and current adjustment */
					/* change charging state to Start CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
					pca9481_set_charging_state(pca9481, DC_STATE_START_CC);
#else
					pca9481->charging_state = DC_STATE_START_CC;
#endif

					pr_info("%s: CC adjust End(Normal): iin=%d, ichg=%d, vbatt=%d, ta_vol=%d, ta_cur=%d\n",
							__func__, iin, ichg, vbatt, pca9481->ta_vol, pca9481->ta_cur);

					/* TA target voltage = TA voltage + (VFLOAT - VBAT_ADC)*2*CHG_mode + 100mV */
					val = pca9481->ta_vol + (pca9481->vfloat - vbatt)*2*pca9481->chg_mode + 100000;
					val = val/PD_MSG_TA_VOL_STEP;
					pca9481->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
					if (pca9481->ta_target_vol > pca9481->ta_max_vol)
						pca9481->ta_target_vol = pca9481->ta_max_vol;
					pr_info("%s: CC adjust End: ta_target_vol=%d\n", __func__, pca9481->ta_target_vol);
					/* Clear TA increment flag */
					pca9481->prev_inc = INC_NONE;
					/* Go to Start CC mode */
					mutex_lock(&pca9481->lock);
					pca9481->timer_id = TIMER_ENTER_CCMODE;
					pca9481->timer_period = 0;
					mutex_unlock(&pca9481->lock);
				} else {
					/* TA control method is CV mode */
					pr_info("%s: CC adjust End(Normal,CV): iin=%d, ichg=%d, vbatt=%d, ta_vol=%d, ta_cur=%d\n",
							__func__, iin, ichg, vbatt, pca9481->ta_vol, pca9481->ta_cur);
					/* Save TA target voltage */
					pca9481->ta_target_vol = pca9481->ta_vol;
					pr_info("%s: CC adjust End(Normal): ta_ctrl=%d, ta_target_vol=%d\n",
							__func__, pca9481->ta_ctrl, pca9481->ta_target_vol);
					/* Clear TA increment flag */
					pca9481->prev_inc = INC_NONE;
					/* Go to CC mode */
					mutex_lock(&pca9481->lock);
					pca9481->timer_id = TIMER_CHECK_CCMODE;
					pca9481->timer_period = 0;
					mutex_unlock(&pca9481->lock);
				}
			} else {
				/* Compare TA maximum voltage */
				if (pca9481->ta_vol == pca9481->ta_max_vol) {
					/* Compare TA maximum current */
					if (pca9481->ta_cur == pca9481->ta_max_cur) {
						/* TA voltage and current are already max value */
						pr_info("%s: CC adjust End(MAX_VOL/CUR): iin=%d, ichg=%d, ta_vol=%d, ta_cur=%d\n",
								__func__, iin, ichg, pca9481->ta_vol, pca9481->ta_cur);
						/* Save TA target voltage */
						pca9481->ta_target_vol = pca9481->ta_vol;
						/* Clear TA increment flag */
						pca9481->prev_inc = INC_NONE;
						/* Go to CC mode */
						mutex_lock(&pca9481->lock);
						pca9481->timer_id = TIMER_CHECK_CCMODE;
						pca9481->timer_period = 0;
						mutex_unlock(&pca9481->lock);
					} else {
						/* TA current is not maximum value */
						/* Increase TA current(50mA) */
						pca9481->ta_cur = pca9481->ta_cur + PD_MSG_TA_CUR_STEP;
						if (pca9481->ta_cur > pca9481->ta_max_cur)
							pca9481->ta_cur = pca9481->ta_max_cur;
						pr_info("%s: CC adjust Cont(1): iin=%d, ichg=%d, ta_cur=%d\n",
								__func__, iin, ichg, pca9481->ta_cur);

						/* Set TA increment flag */
						pca9481->prev_inc = INC_TA_CUR;
						/* Send PD Message */
						mutex_lock(&pca9481->lock);
						pca9481->timer_id = TIMER_PDMSG_SEND;
						pca9481->timer_period = 0;
						mutex_unlock(&pca9481->lock);
					}
				} else {
					/* Check TA tolerance */
					/* The current input current compares the final input current(IIN_CC) with 100mA offset */
					/* PPS current tolerance has +/-150mA, so offset defined 100mA(tolerance +50mA) */
					if (iin < (pca9481->iin_cc - TA_IIN_OFFSET)) {
						/* TA voltage too low to enter TA CC mode, so we should increae TA voltage */
						pca9481->ta_vol = pca9481->ta_vol + TA_VOL_STEP_ADJ_CC*pca9481->chg_mode;
						if (pca9481->ta_vol > pca9481->ta_max_vol)
							pca9481->ta_vol = pca9481->ta_max_vol;
						pr_info("%s: CC adjust Cont(2): iin=%d, ichg=%d, ta_vol=%d\n",
								__func__, iin, ichg, pca9481->ta_vol);

						/* Set TA increment flag */
						pca9481->prev_inc = INC_TA_VOL;
						/* Send PD Message */
						mutex_lock(&pca9481->lock);
						pca9481->timer_id = TIMER_PDMSG_SEND;
						pca9481->timer_period = 0;
						mutex_unlock(&pca9481->lock);
					} else {
						/* compare IIN ADC with previous IIN ADC + 20mA */
						if (iin > (pca9481->prev_iin + IIN_ADC_OFFSET)) {
							/* TA can supply more current if TA voltage is high */
							/* TA voltage too low to enter TA CC mode, so we should increae TA voltage */
							pca9481->ta_vol = pca9481->ta_vol + TA_VOL_STEP_ADJ_CC*pca9481->chg_mode;
							if (pca9481->ta_vol > pca9481->ta_max_vol)
								pca9481->ta_vol = pca9481->ta_max_vol;
							pr_info("%s: CC adjust Cont(3): iin=%d, ichg=%d, ta_vol=%d\n",
									__func__, iin, ichg, pca9481->ta_vol);

							/* Set TA increment flag */
							pca9481->prev_inc = INC_TA_VOL;
							/* Send PD Message */
							mutex_lock(&pca9481->lock);
							pca9481->timer_id = TIMER_PDMSG_SEND;
							pca9481->timer_period = 0;
							mutex_unlock(&pca9481->lock);
						} else {
							/* Check the previous increment */
							if (pca9481->prev_inc == INC_TA_CUR) {
								/* The previous increment is TA current, but input current does not increase */
								/* Try to increase TA voltage(40mV) */
								pca9481->ta_vol = pca9481->ta_vol + TA_VOL_STEP_ADJ_CC*pca9481->chg_mode;
								if (pca9481->ta_vol > pca9481->ta_max_vol)
									pca9481->ta_vol = pca9481->ta_max_vol;
								pr_info("%s: CC adjust Cont(4): iin=%d, ichg=%d, ta_vol=%d\n",
										__func__, iin, ichg, pca9481->ta_vol);

								/* Set TA increment flag */
								pca9481->prev_inc = INC_TA_VOL;
								/* Send PD Message */
								mutex_lock(&pca9481->lock);
								pca9481->timer_id = TIMER_PDMSG_SEND;
								pca9481->timer_period = 0;
								mutex_unlock(&pca9481->lock);
							} else {
								/* The previous increment is TA voltage, but input current does not increase */
								/* Try to increase TA current */
								/* Check APDO max current */
								if (pca9481->ta_cur == pca9481->ta_max_cur) {
									if (pca9481->ta_ctrl == TA_CTRL_CL_MODE) {
										/* Current TA control method is CL mode */
										/* TA current is maximum current */
										pr_info("%s: CC adjust End(MAX_CUR): iin=%d, ichg=%d, ta_vol=%d, ta_cur=%d\n",
												__func__, iin, ichg, pca9481->ta_vol, pca9481->ta_cur);

										/* TA target voltage = TA voltage + (VFLOAT - VBAT_ADC)*2*CHG_mode + 100mV */
										val = pca9481->ta_vol + (pca9481->vfloat - vbatt)*2*pca9481->chg_mode + 100000;
										val = val/PD_MSG_TA_VOL_STEP;
										pca9481->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
										if (pca9481->ta_target_vol > pca9481->ta_max_vol)
											pca9481->ta_target_vol = pca9481->ta_max_vol;
										pr_info("%s: CC adjust End: ta_target_vol=%d\n", __func__, pca9481->ta_target_vol);

										/* Clear TA increment flag */
										pca9481->prev_inc = INC_NONE;
										/* Go to Start CC mode */
										mutex_lock(&pca9481->lock);
										pca9481->timer_id = TIMER_ENTER_CCMODE;
										pca9481->timer_period = 0;
										mutex_unlock(&pca9481->lock);
									} else {
										/* Current TA control method is CV mode */
										pr_info("%s: CC adjust End(MAX_CUR,CV): iin=%d, ichg=%d, ta_vol=%d, ta_cur=%d\n",
												__func__, iin, ichg, pca9481->ta_vol, pca9481->ta_cur);
										/* Save TA target voltage */
										pca9481->ta_target_vol = pca9481->ta_vol;
										pr_info("%s: CC adjust End(Normal): ta_ctrl=%d, ta_target_vol=%d\n",
												__func__, pca9481->ta_ctrl, pca9481->ta_target_vol);
										/* Clear TA increment flag */
										pca9481->prev_inc = INC_NONE;
										/* Go to CC mode */
										mutex_lock(&pca9481->lock);
										pca9481->timer_id = TIMER_CHECK_CCMODE;
										pca9481->timer_period = 0;
										mutex_unlock(&pca9481->lock);
									}
								} else {
									/* Check the present TA current */
									/* Consider tolerance offset(100mA) */
									if (pca9481->ta_cur >= (pca9481->iin_cc + TA_IIN_OFFSET)) {
										/* TA voltage increment has high priority than TA current increment */
										/* Try to increase TA voltage(40mV) */
										pca9481->ta_vol = pca9481->ta_vol + TA_VOL_STEP_ADJ_CC*pca9481->chg_mode;
										if (pca9481->ta_vol > pca9481->ta_max_vol)
											pca9481->ta_vol = pca9481->ta_max_vol;
										pr_info("%s: CC adjust Cont(5): iin=%d, ichg=%d, ta_vol=%d\n",
												__func__, iin, ichg, pca9481->ta_vol);

										/* Set TA increment flag */
										pca9481->prev_inc = INC_TA_VOL;
										/* Send PD Message */
										mutex_lock(&pca9481->lock);
										pca9481->timer_id = TIMER_PDMSG_SEND;
										pca9481->timer_period = 0;
										mutex_unlock(&pca9481->lock);
									} else {
										/* TA has tolerance and compensate it as real current */
										/* Increase TA current(50mA) */
										pca9481->ta_cur = pca9481->ta_cur + PD_MSG_TA_CUR_STEP;
										if (pca9481->ta_cur > pca9481->ta_max_cur)
											pca9481->ta_cur = pca9481->ta_max_cur;
										pr_info("%s: CC adjust Cont(6): iin=%d, ichg=%d, ta_cur=%d\n",
												__func__, iin, ichg, pca9481->ta_cur);

										/* Set TA increment flag */
										pca9481->prev_inc = INC_TA_CUR;
										/* Send PD Message */
										mutex_lock(&pca9481->lock);
										pca9481->timer_id = TIMER_PDMSG_SEND;
										pca9481->timer_period = 0;
										mutex_unlock(&pca9481->lock);
									}
								}
							}
						}
					}
				}
			}
		}
		/* Save previous iin adc */
		pca9481->prev_iin = iin;
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
		break;

	default:
		break;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging Start CC MODE control - Pre CC MODE */
/* Increase TA voltage to TA target voltage */
static int pca9481_charge_start_ccmode(struct pca9481_charger *pca9481)
{
	int ret = 0;
	int ccmode;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_START_CC);
#else
	pca9481->charging_state = DC_STATE_START_CC;
#endif
	ret = pca9481_check_error(pca9481);
	if (ret != 0)
		goto error; // This is not active mode.

	/* Check the status */
	ccmode = pca9481_check_dcmode_status(pca9481);
	if (ccmode < 0) {
		ret = ccmode;
		goto error;
	}

	/* Increase TA voltage */
	pca9481->ta_vol = pca9481->ta_vol + TA_VOL_STEP_PRE_CC * pca9481->chg_mode;
	/* Check TA target voltage */
	if (pca9481->ta_vol >= pca9481->ta_target_vol) {
		pca9481->ta_vol = pca9481->ta_target_vol;
		pr_info("%s: PreCC End: ta_vol=%d\n", __func__, pca9481->ta_vol);

		/* Change to DC state to CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9481_set_charging_state(pca9481, DC_STATE_CC_MODE);
#else
		pca9481->charging_state = DC_STATE_CC_MODE;
#endif
	} else {
		pr_info("%s: PreCC Cont: ta_vol=%d\n", __func__, pca9481->ta_vol);
	}

	/* Send PD Message */
	mutex_lock(&pca9481->lock);
	pca9481->timer_id = TIMER_PDMSG_SEND;
	pca9481->timer_period = 0;
	mutex_unlock(&pca9481->lock);

	queue_delayed_work(pca9481->dc_wq,
						&pca9481->timer_work,
						msecs_to_jiffies(pca9481->timer_period));

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging CC MODE control */
static int pca9481_charge_ccmode(struct pca9481_charger *pca9481)
{
	int ret = 0;
	int ccmode;
	int iin, ichg;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_CC_MODE);
#else
	pca9481->charging_state = DC_STATE_CC_MODE;
#endif
	/* Check the charging type */
	ret = pca9481_check_error(pca9481);
	if (ret < 0) {
		if (ret == -EAGAIN) {
			/* DC error happens, but it is retry case */
			if (pca9481->ta_ctrl == TA_CTRL_CL_MODE) {
				/* Current TA control method is Current Limit mode */
				/* Retry DC as Constant Voltage mode */
				pr_info("%s: Retry DC : ta_ctrl=%d\n", __func__, pca9481->ta_ctrl);

				/* Disable charging */
				ret = pca9481_set_charging(pca9481, false);
				if (ret < 0)
					goto error;

				/* Softreset */
				ret = pca9481_softreset(pca9481);
				if (ret < 0)
					goto error;

				/* Set TA control method to Constant Voltage mode */
				pca9481->ta_ctrl = TA_CTRL_CV_MODE;

				/* Go to DC_STATE_PRESET_DC */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_PRESET_DC;
				pca9481->timer_period = 0;
				mutex_unlock(&pca9481->lock);
				queue_delayed_work(pca9481->dc_wq,
									&pca9481->timer_work,
									msecs_to_jiffies(pca9481->timer_period));
				ret = 0;
				goto error;
			} else {
				/* Current TA control method is Constant Voltage mode */
				/* Don't retry DC */
				pr_info("%s: Retry DC, but still failed - stop DC\n", __func__);
				goto error;
			}
		} else {
			/* Don't retry DC */
			goto error;
		}
	}

	/* Check new request */
	if (pca9481->req_new_dc_mode == true) {
		ret = pca9481_set_new_dc_mode(pca9481);
	} else if (pca9481->req_new_iin == true) {
		ret = pca9481_set_new_iin(pca9481);
	} else if (pca9481->req_new_vfloat == true) {
		/* Clear request flag */
		mutex_lock(&pca9481->lock);
		pca9481->req_new_vfloat = false;
		mutex_unlock(&pca9481->lock);
		ret = pca9481_set_new_vfloat(pca9481);
	} else {
		/* Check the charging type */
		ccmode = pca9481_check_dcmode_status(pca9481);
		if (ccmode < 0) {
			ret = ccmode;
			goto error;
		}

		switch (ccmode) {
		case DCMODE_LOOP_INACTIVE:
			/* Set input current compensation */
			/* Check the TA type */
			if (pca9481->ta_type == TA_TYPE_WIRELESS) {
				/* Need RX voltage compensation */
				ret = pca9481_set_rx_voltage_comp(pca9481);
				pr_info("%s: CC INACTIVE: rx_vol=%d\n", __func__, pca9481->ta_vol);
			} else {
				/* Check the current TA current with TA_MIN_CUR */
				if ((pca9481->ta_cur <= TA_MIN_CUR) ||
					(pca9481->ta_ctrl == TA_CTRL_CV_MODE)) {
					/* Need input voltage compensation */
					ret = pca9481_set_ta_voltage_comp(pca9481);
				} else {
					if (pca9481->ta_max_vol >= TA_MAX_VOL_CP) {
						/* This TA can support the input current without power limit */
						/* Need input current compensation */
						ret = pca9481_set_ta_current_comp(pca9481);
					} else {
						/* This TA has the power limitation for the input current compenstaion */
						/* The input current cannot increase over the constant power */
						/* Need input current compensation in constant power mode */
						ret = pca9481_set_ta_current_comp2(pca9481);
					}
				}
				pr_info("%s: CC INACTIVE: ta_cur=%d, ta_vol=%d\n", __func__, pca9481->ta_cur, pca9481->ta_vol);
			}
			break;

		case DCMODE_VFLT_LOOP:
			/* Read IIN_ADC */
			iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
			/* Read ICHG_ADC */
			ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);
			pr_info("%s: CC VFLOAT: iin=%d, ichg=%d\n", __func__, iin, ichg);
			/* go to Pre-CV mode */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_ENTER_CVMODE;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
			break;

		case DCMODE_IIN_LOOP:
		case DCMODE_CHG_LOOP:
			/* Read IIN_ADC */
			iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
			/* Read ICHG_ADC */
			ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);
			/* Check the TA type */
			if (pca9481->ta_type == TA_TYPE_WIRELESS) {
				/* Wireless Charger is connected */
				/* Decrease RX voltage (100mV) */
				pca9481->ta_vol = pca9481->ta_vol - WCRX_VOL_STEP;
				pr_info("%s: CC LOOP(WC):iin=%d, ichg=%d, next_rx_vol=%d\n", __func__, iin, ichg, pca9481->ta_vol);
			} else {
				/* USBPD TA is connected */

				/* Check the current TA current with TA_MIN_CUR */
				if ((pca9481->ta_cur <= TA_MIN_CUR) ||
					(pca9481->ta_ctrl == TA_CTRL_CV_MODE)) {
					/* Decrease TA voltage (20mV) */
					pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
					pr_info("%s: CC LOOP(1):iin=%d, ichg=%d, next_ta_vol=%d\n", __func__, iin, ichg, pca9481->ta_vol);
				} else {
					/* Check TA current and compare it with IIN_CC */
					if (pca9481->ta_cur <= pca9481->iin_cc - TA_CUR_LOW_OFFSET) {
						/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
						/* TA has abnormal behavior */
						/* Decrease TA voltage (20mV) */
						pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
						pr_info("%s: CC LOOP(2):iin=%d, ichg=%d, ta_cur=%d, next_ta_vol=%d\n",
								__func__, iin, ichg, pca9481->ta_cur, pca9481->ta_vol);
						/* Update TA target voltage */
						pca9481->ta_target_vol = pca9481->ta_vol;
					} else {
						/* TA current is higher than IIN_CC - 200mA */
						/* Decrease TA current first to reduce input current */
						/* Decrease TA current (50mA) */
						pca9481->ta_cur = pca9481->ta_cur - PD_MSG_TA_CUR_STEP;
						pr_info("%s: CC LOOP(3):iin=%d, ichg=%d, ta_vol=%d, next_ta_cur=%d\n",
								__func__, iin, ichg, pca9481->ta_vol, pca9481->ta_cur);
					}
				}
			}
			/* Send PD Message */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
			break;

		default:
			break;
		}
	}
error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* 2:1 Direct Charging Start CV MODE control - Pre CV MODE */
static int pca9481_charge_start_cvmode(struct pca9481_charger *pca9481)
{
	int ret = 0;
	int cvmode;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_START_CV);
#else
	pca9481->charging_state = DC_STATE_START_CV;
#endif

	/* Check the charging type */
	ret = pca9481_check_error(pca9481);
	if (ret != 0)
		goto error; // This is not active mode.
	/* Check the status */
	cvmode = pca9481_check_dcmode_status(pca9481);
	if (cvmode < 0) {
		ret = cvmode;
		goto error;
	}

	switch (cvmode) {
	case DCMODE_CHG_LOOP:
	case DCMODE_IIN_LOOP:
		/* Check the TA type */
		if (pca9481->ta_type == TA_TYPE_WIRELESS) {
			/* Decrease RX voltage (100mV) */
			pca9481->ta_vol = pca9481->ta_vol - WCRX_VOL_STEP;
			pr_info("%s: PreCV Cont(IIN_LOOP): rx_vol=%d\n", __func__, pca9481->ta_vol);
		} else {
			/* Check TA current */
			if (pca9481->ta_cur > TA_MIN_CUR) {
				/* TA current is higher than 1.0A */
				/* Decrease TA current (50mA) */
				pca9481->ta_cur = pca9481->ta_cur - PD_MSG_TA_CUR_STEP;
				pr_info("%s: PreCV Cont: ta_cur=%d\n", __func__, pca9481->ta_cur);
			} else {
				/* TA current is less than 1.0A */
				/* Decrease TA voltage (20mV) */
				pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
				pr_info("%s: PreCV Cont(IIN_LOOP): ta_vol=%d\n", __func__, pca9481->ta_vol);
			}
		}
		/* Send PD Message */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PDMSG_SEND;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
		break;

	case DCMODE_VFLT_LOOP:
		/* Check the TA type */
		if (pca9481->ta_type == TA_TYPE_WIRELESS) {
			/* Decrease RX voltage (100mV) */
			pca9481->ta_vol = pca9481->ta_vol - WCRX_VOL_STEP;
			pr_info("%s: PreCV Cont: rx_vol=%d\n", __func__, pca9481->ta_vol);
		} else {
			/* Decrease TA voltage (20mV) */
			pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP * pca9481->chg_mode;
			pr_info("%s: PreCV Cont: ta_vol=%d\n", __func__, pca9481->ta_vol);
		}
		/* Send PD Message */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PDMSG_SEND;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
		break;

	case DCMODE_LOOP_INACTIVE:
		/* Exit Pre CV mode */
		pr_info("%s: PreCV End: ta_vol=%d, ta_cur=%d\n", __func__, pca9481->ta_vol, pca9481->ta_cur);

		/* Set TA target voltage to TA voltage */
		pca9481->ta_target_vol = pca9481->ta_vol;

		/* Need to implement notification to other driver */
		/* To do here */

		/* Go to CV mode */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_CHECK_CVMODE;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
		break;

	default:
		break;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging CV MODE control */
static int pca9481_charge_cvmode(struct pca9481_charger *pca9481)
{
	int ret = 0;
	int cvmode;
	int iin, ichg;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_CV_MODE);
#else
	pca9481->charging_state = DC_STATE_CV_MODE;
#endif

	ret = pca9481_check_error(pca9481);
	if (ret != 0)
		goto error; // This is not active mode.

	/* Check new request */
	if (pca9481->req_new_dc_mode == true) {
		ret = pca9481_set_new_dc_mode(pca9481);
	} else if (pca9481->req_new_iin == true) {
		ret = pca9481_set_new_iin(pca9481);
	} else if (pca9481->req_new_vfloat == true) {
		ret = pca9481_set_new_vfloat(pca9481);
	} else {
		cvmode = pca9481_check_dcmode_status(pca9481);
		if (cvmode < 0) {
			ret = cvmode;
			goto error;
		}

		/* Read IIN_ADC */
		iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
		/* Read ICHG_ADC */
		ichg = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);

		/* Check charging done state */
		if (cvmode == DCMODE_LOOP_INACTIVE) {
			/* Compare iin with input topoff current */
			pr_info("%s: iin=%d, ichg=%d, iin_topoff=%d\n",
					__func__, iin, ichg, pca9481->iin_topoff);
			if (iin < pca9481->iin_topoff) {
				/* Change cvmode status to charging done */
				cvmode = DCMODE_CHG_DONE;
				pr_info("%s: CVMODE Status=%d\n", __func__, cvmode);
			}
		}

		switch (cvmode) {
		case DCMODE_CHG_DONE:
			/* Charging Done */
			/* Keep CV mode until battery driver send stop charging */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_charging_state(pca9481, DC_STATE_CHARGING_DONE);
#else
			pca9481->charging_state = DC_STATE_CHARGING_DONE;
#endif
			/* Need to implement notification function */
			/* A customer should insert code */

			pr_info("%s: CV Done\n", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_done(pca9481, true);
#endif
			/* Check the charging status after notification function */
			if (pca9481->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the charging done state */
				/* Set timer */
				mutex_lock(&pca9481->lock);
				pca9481->timer_id = TIMER_CHECK_CVMODE;

				/* Add to check charging step and set the polling time */
				if (pca9481->vfloat < pca9481->pdata->step1_vth) {
					/* Step1 charging - polling time is cv_polling */
					pca9481->timer_period = pca9481->pdata->cv_polling;
				} else {
					/* Step2 or 3 charging - polling time is CVMODE_CHECK_T(10s) */
					pca9481->timer_period = CVMODE_CHECK_T;
				}
				mutex_unlock(&pca9481->lock);
				queue_delayed_work(pca9481->dc_wq,
									&pca9481->timer_work,
									msecs_to_jiffies(pca9481->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case DCMODE_CHG_LOOP:
		case DCMODE_IIN_LOOP:
			/* Check the TA type */
			if (pca9481->ta_type == TA_TYPE_WIRELESS) {
				/* Decrease RX Voltage (100mV) */
				pca9481->ta_vol = pca9481->ta_vol - WCRX_VOL_STEP;
				pr_info("%s: CV LOOP(WC), Cont: iin=%d, ichg=%d, rx_vol=%d\n",
						__func__, iin, ichg, pca9481->ta_vol);
			} else {
				/* Check TA current */
				if (pca9481->ta_cur > TA_MIN_CUR) {
					/* TA current is higher than (1.0A*chg_mode) */
					/* Check TA current and compare it with IIN_CC */
					if (pca9481->ta_cur <= pca9481->iin_cc - TA_CUR_LOW_OFFSET) {
						/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
						/* TA has abnormal behavior */
						/* Decrease TA voltage (20mV) */
						pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
						pr_info("%s: CV LOOP(1):iin=%d, ichg=%d, ta_cur=%d, next_ta_vol=%d\n",
								__func__, iin, ichg, pca9481->ta_cur, pca9481->ta_vol);
						/* Update TA target voltage */
						pca9481->ta_target_vol = pca9481->ta_vol;
					} else {
						/* TA current is higher than IIN_CC - 200mA */
						/* Decrease TA current first to reduce input current */
						/* Decrease TA current (50mA) */
						pca9481->ta_cur = pca9481->ta_cur - PD_MSG_TA_CUR_STEP;
						pr_info("%s: CV LOOP(2):iin=%d, ichg=%d, ta_vol=%d, next_ta_cur=%d\n",
								__func__, iin, ichg, pca9481->ta_vol, pca9481->ta_cur);
					}
				} else {
					/* TA current is less than (1.0A*chg_mode) */
					/* Decrease TA Voltage (20mV) */
					pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
					pr_info("%s: CV LOOP(3), Cont: iin=%d, ichg=%d, ta_vol=%d\n",
							__func__, iin, ichg, pca9481->ta_vol);
				}
			}
			/* Send PD Message */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
			break;

		case DCMODE_VFLT_LOOP:
			/* Check the TA type */
			if (pca9481->ta_type == TA_TYPE_WIRELESS) {
				/* Decrease RX voltage */
				pca9481->ta_vol = pca9481->ta_vol - WCRX_VOL_STEP;
				pr_info("%s: CV VFLOAT, Cont: iin=%d, ichg=%d, rx_vol=%d\n",
						__func__, iin, ichg, pca9481->ta_vol);
			} else {
				/* Decrease TA voltage */
				pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
				pr_info("%s: CV VFLOAT, Cont: iin=%d, ichg=%d, ta_vol=%d\n",
						__func__, iin, ichg, pca9481->ta_vol);
			}

			/* Set TA target voltage to TA voltage */
			pca9481->ta_target_vol = pca9481->ta_vol;

			/* Send PD Message */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
			break;

		case DCMODE_LOOP_INACTIVE:
			/* Set timer */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_CHECK_CVMODE;
			pca9481->timer_period = CVMODE_CHECK_T;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
			break;

		default:
			break;
		}
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Direct Charging Bypass Mode Control */
static int pca9481_charge_bypass_mode(struct pca9481_charger *pca9481)
{
	int ret = 0;
	int dc_status;
	int vbat, iin;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_BYPASS_MODE);
#else
	pca9481->charging_state = DC_STATE_BYPASS_MODE;
#endif

	ret = pca9481_check_error(pca9481);
	if (ret < 0)
		goto error;	// This is not active mode.

	/* Check new request */
	if (pca9481->req_new_dc_mode == true) {
		ret = pca9481_set_new_dc_mode(pca9481);
	} else if (pca9481->req_new_iin == true) {
		ret = pca9481_set_new_iin(pca9481);
	} else if (pca9481->req_new_vfloat == true) {
		ret = pca9481_set_new_vfloat(pca9481);
	} else {
		dc_status = pca9481_check_dcmode_status(pca9481);
		if (dc_status < 0) {
			ret = dc_status;
			goto error;
		}

		/* Read IIN ADC */
		iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
		/* Read VBAT ADC */
		vbat = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);

		pr_info("%s: iin=%d, vbat=%d\n", __func__, iin, vbat);

		if (dc_status == DCMODE_IIN_LOOP) {
			/* Decrease input current */
			/* Check TA current and compare it with IIN_CC */
			if (pca9481->ta_cur <= pca9481->iin_cc - TA_CUR_LOW_OFFSET) {
				/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
				/* TA has abnormal behavior */
				/* Decrease TA voltage (20mV) */
				pca9481->ta_vol = pca9481->ta_vol - PD_MSG_TA_VOL_STEP;
				pr_info("%s: IIN LOOP:iin=%d, ta_cur=%d, next_ta_vol=%d\n",
						__func__, iin, pca9481->ta_cur, pca9481->ta_vol);
			} else {
				/* TA current is higher than IIN_CC - 200mA */
				/* Decrease TA current first to reduce input current */
				/* Decrease TA current (50mA) */
				pca9481->ta_cur = pca9481->ta_cur - PD_MSG_TA_CUR_STEP;
				pr_info("%s: IIN LOOP:iin=%d, next_ta_cur=%d\n",
						__func__, iin, pca9481->ta_cur);
			}

			/* Send PD Message */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PDMSG_SEND;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
		} else {
			/* Ignore other status */
			/* Keep Bypass mode */
			pr_info("%s: Bypass mode, status=%d, ta_cur=%d, ta_vol=%d\n",
					__func__, dc_status, pca9481->ta_cur, pca9481->ta_vol);
			/* Set timer - 10s */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_CHECK_BYPASSMODE;
			pca9481->timer_period = BYPMODE_CHECK_T;	/* 10s */
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
		}
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Direct Charging DC mode Change Control */
static int pca9481_charge_dcmode_change(struct pca9481_charger *pca9481)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_DCMODE_CHANGE);
#else
	pca9481->charging_state = DC_STATE_DCMODE_CHANGE;
#endif

	ret = pca9481_set_new_dc_mode(pca9481);

	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Preset TA voltage and current for Direct Charging Mode */
static int pca9481_preset_dcmode(struct pca9481_charger *pca9481)
{
	int vbat;
	unsigned int val;
	int ret = 0;
	int chg_mode;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_PRESET_DC);
#else
	pca9481->charging_state = DC_STATE_PRESET_DC;
#endif

	/* Read VBAT ADC */
	vbat = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);
	if (vbat < 0) {
		ret = vbat;
		goto error;
	}

	/* Compare VBAT with VBAT ADC */
	if (vbat > pca9481->vfloat)	{
		/* Invalid battery voltage to start direct charging */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pr_err("## %s: vbat adc(%duA) is higher than VFLOAT(%duA)\n",
				__func__, vbat, pca9481->pdata->vfloat);
#else
		pr_err("%s: vbat adc is higher than VFLOAT\n", __func__);
#endif
		ret = -EINVAL;
		goto error;
	}

	/* Check minimum VBAT level */
	if (vbat <= DC_VBAT_MIN_ERR) {
		/* Invalid battery level to start direct charging */
		pr_err("%s: This vbat(%duV) will make VIN_OV_TRACKING error\n", __func__, vbat);
		ret = -EINVAL;
		goto error;
	}

	/* Check the TA type and set the charging mode */
	if (pca9481->ta_type == TA_TYPE_WIRELESS) {
		/* Wireless Charger is connected */
		/* Set the RX max current to input request current(iin_cfg) initially */
		/* to get RX maximum current from RX IC */
		pca9481->ta_max_cur = pca9481->iin_cfg;
		/* Set the RX max voltage to enough high value to find RX maximum voltage initially */
		pca9481->ta_max_vol = WCRX_MAX_VOL * pca9481->pdata->chg_mode;
		/* Get the RX max current/voltage(RX_MAX_CUR/VOL) */
		ret = pca9481_get_rx_max_power(pca9481);
		if (ret < 0) {
			/* RX IC does not have the desired maximum voltage */
			/* Check the desired mode */
			if (pca9481->pdata->chg_mode == CHG_4TO1_DC_MODE) {
				/* RX IC doesn't have any maximum voltage to support 4:1 mode */
				/* Get the RX max current/voltage with 2:1 mode */
				pca9481->ta_max_vol = WCRX_MAX_VOL;
				ret = pca9481_get_rx_max_power(pca9481);
				if (ret < 0) {
					pr_err("%s: RX IC doesn't have any RX voltage to support 2:1 or 4:1\n", __func__);
					pca9481->chg_mode = CHG_NO_DC_MODE;
					goto error;
				} else {
					/* RX IC has the maximum RX voltage to support 2:1 mode */
					pca9481->chg_mode = CHG_2TO1_DC_MODE;
				}
			} else {
				/* The desired CHG mode is 2:1 mode */
				/* RX IC doesn't have any RX voltage to support 2:1 mode*/
				pr_err("%s: RX IC doesn't have any RX voltage to support 2:1\n", __func__);
				pca9481->chg_mode = CHG_NO_DC_MODE;
				goto error;
			}
		} else {
			/* RX IC has the desired RX voltage */
			pca9481->chg_mode = pca9481->pdata->chg_mode;
		}

		chg_mode = pca9481->chg_mode;

		/* Set IIN_CC to MIN[IIN, (RX_MAX_CUR by RX IC)*chg_mode]*/
		pca9481->iin_cc = MIN(pca9481->iin_cfg, (pca9481->ta_max_cur*chg_mode));
		/* Set the current IIN_CC to iin_cfg */
		pca9481->iin_cfg = pca9481->iin_cc;

		/* Set RX voltage to MAX[(2*VBAT_ADC*chg_mode + offset), TA_MIN_VOL_PRESET*chg_mode] */
		pca9481->ta_vol = max(TA_MIN_VOL_PRESET*chg_mode, (2*vbat*chg_mode + TA_VOL_PRE_OFFSET));
		val = pca9481->ta_vol/WCRX_VOL_STEP;	/* RX voltage resolution is 100mV */
		pca9481->ta_vol = val*WCRX_VOL_STEP;
		/* Set RX voltage to MIN[RX voltage, RX_MAX_VOL*chg_mode] */
		pca9481->ta_vol = MIN(pca9481->ta_vol, pca9481->ta_max_vol);

		pr_info("%s: Preset DC, rx_max_vol=%d, rx_max_cur=%d, rx_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
			__func__, pca9481->ta_max_vol, pca9481->ta_max_cur, pca9481->ta_max_pwr, pca9481->iin_cc, pca9481->chg_mode);

		pr_info("%s: Preset DC, rx_vol=%d\n", __func__, pca9481->ta_vol);

	} else {
		/* USBPD TA is connected */
		/* Set the TA max current to input request current(iin_cfg) initially */
		/* to get TA maximum current from PD IC */
		pca9481->ta_max_cur = pca9481->iin_cfg;
		/* Set the TA max voltage to enough high value to find TA maximum voltage initially */
		pca9481->ta_max_vol = TA_MAX_VOL * pca9481->pdata->chg_mode;
		/* Search the proper object position of PDO */
		pca9481->ta_objpos = 0;
		/* Get the APDO max current/voltage(TA_MAX_CUR/VOL) */
		ret = pca9481_get_apdo_max_power(pca9481);
		if (ret < 0) {
			/* TA does not have the desired APDO */
			/* Check the desired mode */
			if (pca9481->pdata->chg_mode == CHG_4TO1_DC_MODE) {
				/* TA doesn't have any APDO to support 4:1 mode */
				/* Get the APDO max current/voltage with 2:1 mode */
				pca9481->ta_max_vol = TA_MAX_VOL;
				pca9481->ta_objpos = 0;
				ret = pca9481_get_apdo_max_power(pca9481);
				if (ret < 0) {
					pr_err("%s: TA doesn't have any APDO to support 2:1 or 4:1\n", __func__);
					pca9481->chg_mode = CHG_NO_DC_MODE;
					goto error;
				} else {
					/* TA has APDO to support 2:1 mode */
					pca9481->chg_mode = CHG_2TO1_DC_MODE;
				}
			} else {
				/* The desired TA mode is 2:1 mode */
				/* TA doesn't have any APDO to support 2:1 mode*/
				pr_err("%s: TA doesn't have any APDO to support 2:1\n", __func__);
				pca9481->chg_mode = CHG_NO_DC_MODE;
				goto error;
			}
		} else {
			/* TA has the desired APDO */
			pca9481->chg_mode = pca9481->pdata->chg_mode;
		}

		chg_mode = pca9481->chg_mode;

		/* Calculate new TA maximum current and voltage that used in the direct charging */
		/* Set IIN_CC to MIN[IIN, (TA_MAX_CUR by APDO)*chg_mode]*/
		pca9481->iin_cc = MIN(pca9481->iin_cfg, (pca9481->ta_max_cur*chg_mode));
		/* Set the current IIN_CC to iin_cfg for recovering it after resolution adjustment */
		pca9481->iin_cfg = pca9481->iin_cc;
		/* Calculate new TA max voltage */
		/* Adjust IIN_CC with APDO resoultion(50mA) - It will recover to the original value after max voltage calculation */
		val = pca9481->iin_cc/PD_MSG_TA_CUR_STEP;
		pca9481->iin_cc = val*PD_MSG_TA_CUR_STEP;
		/* Set TA_MAX_VOL to MIN[TA_MAX_VOL*chg_mode, TA_MAX_PWR/(IIN_CC/chg_mode)] */
		val = pca9481->ta_max_pwr/(pca9481->iin_cc/chg_mode/1000);	/* mV */
		val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
		val = val*PD_MSG_TA_VOL_STEP; /* uV */
		pca9481->ta_max_vol = MIN(val, TA_MAX_VOL*chg_mode);

		/* Set TA voltage to MAX[TA_MIN_VOL_PRESET*chg_mode, (2*VBAT_ADC*chg_mode + offset)] */
		pca9481->ta_vol = max(TA_MIN_VOL_PRESET*chg_mode, (2*vbat*chg_mode + TA_VOL_PRE_OFFSET));
		val = pca9481->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
		pca9481->ta_vol = val*PD_MSG_TA_VOL_STEP;
		/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL*chg_mode] */
		pca9481->ta_vol = MIN(pca9481->ta_vol, pca9481->ta_max_vol);
		/* Set the initial TA current to IIN_CC/chg_mode */
		pca9481->ta_cur = pca9481->iin_cc/chg_mode;
		/* Recover IIN_CC to the original value(iin_cfg) */
		pca9481->iin_cc = pca9481->iin_cfg;

		pr_info("%s: Preset DC, ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
			__func__, pca9481->ta_max_vol, pca9481->ta_max_cur, pca9481->ta_max_pwr, pca9481->iin_cc, pca9481->chg_mode);

		pr_info("%s: Preset DC, ta_vol=%d, ta_cur=%d\n",
			__func__, pca9481->ta_vol, pca9481->ta_cur);
	}

	/* Send PD Message */
	mutex_lock(&pca9481->lock);
	pca9481->timer_id = TIMER_PDMSG_SEND;
	pca9481->timer_period = 0;
	mutex_unlock(&pca9481->lock);
	queue_delayed_work(pca9481->dc_wq,
						&pca9481->timer_work,
						msecs_to_jiffies(pca9481->timer_period));

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* Preset direct charging configuration */
static int pca9481_preset_config(struct pca9481_charger *pca9481)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_PRESET_DC);
#else
	pca9481->charging_state = DC_STATE_PRESET_DC;
#endif

	/* Set IIN_CFG to IIN_CC */
	ret = pca9481_set_input_current(pca9481, pca9481->iin_cc);
	if (ret < 0)
		goto error;

	/* Set ICHG_CFG to enough high value */
	if (pca9481->ichg_cfg < 2*pca9481->iin_cfg)
		pca9481->ichg_cfg = 2*pca9481->iin_cfg;
	ret = pca9481_set_charging_current(pca9481, pca9481->ichg_cfg);
	if (ret < 0)
		goto error;

	/* Set VFLOAT */
	ret = pca9481_set_vfloat(pca9481, pca9481->vfloat);
	if (ret < 0)
		goto error;

	/* Enable PCA9481 */
	ret = pca9481_set_charging(pca9481, true);
	if (ret < 0)
		goto error;

	/* Clear previous iin adc */
	pca9481->prev_iin = 0;

	/* Clear TA increment flag */
	pca9481->prev_inc = INC_NONE;

	/* Go to CHECK_ACTIVE state after 150ms*/
	mutex_lock(&pca9481->lock);
	pca9481->timer_id = TIMER_CHECK_ACTIVE;
	pca9481->timer_period = ENABLE_DELAY_T;
	mutex_unlock(&pca9481->lock);
	queue_delayed_work(pca9481->dc_wq,
						&pca9481->timer_work,
						msecs_to_jiffies(pca9481->timer_period));
	ret = 0;

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Check the charging status before entering the adjust cc mode */
static int pca9481_check_active_state(struct pca9481_charger *pca9481)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_CHECK_ACTIVE);
#else
	pca9481->charging_state = DC_STATE_CHECK_ACTIVE;
#endif
	ret = pca9481_check_error(pca9481);

	if (ret == 0) {
		/* PCA9481 is in active state */
		/* Clear retry counter */
		pca9481->retry_cnt = 0;
		/* Go to Adjust CC mode */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_ADJUST_CCMODE;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);

		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
	} else if (ret == -EAGAIN) {
		/* It is the retry condition */
		/* Check the retry counter */
		if (pca9481->retry_cnt < MAX_RETRY_CNT) {
			/* Disable charging */
			ret = pca9481_set_charging(pca9481, false);
			if (ret < 0)
				goto error;
			/* Softreset */
			ret = pca9481_softreset(pca9481);
			if (ret < 0)
				goto error;
			/* Increase retry counter */
			pca9481->retry_cnt++;
			pr_err("%s: retry charging start - retry_cnt=%d\n", __func__, pca9481->retry_cnt);
			/* Go to DC_STATE_PRESET_DC */
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_PRESET_DC;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
			ret = 0;
		} else {
			pr_err("%s: retry fail\n", __func__);
			/* Disable charging */
			ret = pca9481_set_charging(pca9481, false);
			if (ret < 0)
				goto error;
			/* Softreset */
			ret = pca9481_softreset(pca9481);
			if (ret < 0)
				goto error;
			/* Notify maximum retry error */
			ret = -EINVAL;
			/* Stop charging in timer_work */
		}
	} else {
		pr_err("%s: dc start fail\n", __func__);
		/* Implement error handler function if it is needed */
		/* Disable charging */
		ret = pca9481_set_charging(pca9481, false);
		if (ret < 0)
			goto error;
		/* Softreset */
		ret = pca9481_softreset(pca9481);
		if (ret < 0)
			goto error;
		/* Stop charging in timer_work */
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* Enter direct charging algorithm */
static int pca9481_start_direct_charging(struct pca9481_charger *pca9481)
{
	int ret;
	unsigned int val;
#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG) /* temp disable wc dc charging */
	struct power_supply *psy;
	union power_supply_propval pro_val;
#endif

	pr_info("%s: =========START=========\n", __func__);

	/* Set OV_TRACK_DELTA to 800mV */
	val = OV_TRACK_DELTA_800mV << MASK2SHIFT(PCA9481_BIT_OV_TRACK_DELTA);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_SC_CNTL_2,
							PCA9481_BIT_OV_TRACK_DELTA, val);
	if (ret < 0)
		return ret;

	/* Set UV_TRACK_DELTA to 600mV */
	val = UV_TRACK_DELTA_600mV << MASK2SHIFT(PCA9481_BIT_UV_TRACK_DELTA);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_SC_CNTL_1,
							PCA9481_BIT_UV_TRACK_DELTA, val);
	if (ret < 0)
		return ret;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	/* Set watchdog timer enable */
	pca9481_set_wdt_enable(pca9481, WDT_ENABLE);
	pca9481_check_wdt_control(pca9481);
#endif

	/* Set Switching Frequency - kHz unit */
	val = PCA9481_FSW_CFG(pca9481->pdata->fsw_cfg);
	ret = pca9481_write_reg(pca9481, PCA9481_REG_SC_CNTL_0, val);
	if (ret < 0)
		return ret;
	pr_info("%s: sw_freq=%dkHz\n", __func__, pca9481->pdata->fsw_cfg);

	/* Set EN_CFG to active high - Disable PCA9481 */
	val =  PCA9481_EN_ACTIVE_H;
	ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_2,
							PCA9481_BIT_EN_CFG, val);
	if (ret < 0)
		return ret;

	/* Set NTC0 voltage threshold - cold or cool condition */
	val = PCA9481_NTC_TRIGGER_VOLTAGE(pca9481->pdata->ntc0_th);
	val = val << MASK2SHIFT(PCA9481_BIT_NTC_0_TRIGGER_VOLTAGE);
	ret = pca9481_write_reg(pca9481, PCA9481_REG_NTC_0_CNTL, val | PCA9481_BIT_NTC_EN);
	if (ret < 0)
		return ret;

	/* Set NTC1 voltage threshold - warm or hot condition */
	val = PCA9481_NTC_TRIGGER_VOLTAGE(pca9481->pdata->ntc1_th);
	ret = pca9481_write_reg(pca9481, PCA9481_REG_NTC_1_CNTL, val);
	if (ret < 0)
		return ret;

	/* wake lock */
	__pm_stay_awake(pca9481->monitor_wake_lock);

	/* Preset charging configuration and TA condition */
	ret = pca9481_preset_dcmode(pca9481);

	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* Check Vbat minimum level to start direct charging */
static int pca9481_check_vbatmin(struct pca9481_charger *pca9481)
{
	int vbat;
	int ret;
	union power_supply_propval val;

	pr_info("%s: =========START=========\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_set_charging_state(pca9481, DC_STATE_CHECK_VBAT);
#else
	pca9481->charging_state = DC_STATE_CHECK_VBAT;
#endif
	/* Check Vbat */
	vbat = pca9481_read_adc(pca9481, ADCCH_BATP_BATN);
	if (vbat < 0)
		ret = vbat;

	/* Read switching charger status */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ret = psy_do_property(pca9481->pdata->sec_dc_name, get,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
#else
	ret = pca9481_get_switching_charger_property(POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
#endif
	if (ret < 0) {
		/* Start Direct Charging again after 1sec */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_VBATMIN_CHECK;
		pca9481->timer_period = VBATMIN_CHECK_T;
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
		goto error;
	}

	if (val.intval == 0) {
		/* already disabled switching charger */
		/* Clear retry counter */
		pca9481->retry_cnt = 0;
		/* Preset TA voltage and DC parameters */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_PRESET_DC;
		pca9481->timer_period = 0;
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
	} else {
		/* Switching charger is enabled */
		if (vbat > DC_VBAT_MIN) {
			/* Start Direct Charging */
			/* now switching charger is enabled */
			/* disable switching charger first */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9481_set_switching_charger(pca9481, false);
#else
			ret = pca9481_set_switching_charger(false, 0, 0, 0);
#endif
		}

		/* Wait 1sec for stopping switching charger or Start 1sec timer for battery check */
		mutex_lock(&pca9481->lock);
		pca9481->timer_id = TIMER_VBATMIN_CHECK;
		pca9481->timer_period = VBATMIN_CHECK_T;
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

#ifdef CONFIG_RTC_HCTOSYS
static int get_current_time(unsigned long *now_tm_sec)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
		       __FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
		       CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
		       CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	*now_tm_sec = rtc_tm_to_time64(&tm);

close_time:
	rtc_class_close(rtc);
	return rc;
}
#endif

/* delayed work function for charging timer */
static void pca9481_timer_work(struct work_struct *work)
{
	struct pca9481_charger *pca9481 = container_of(work, struct pca9481_charger,
						 timer_work.work);
	int ret = 0;
	unsigned int val;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval value = {0,};

	psy_do_property("battery", get,
			POWER_SUPPLY_EXT_PROP_CHARGE_COUNTER_SHADOW, value);
	if ((value.intval == SEC_BATTERY_CABLE_NONE) && pca9481->mains_online)
		return;
#endif

#ifdef CONFIG_RTC_HCTOSYS
	get_current_time(&pca9481->last_update_time);

	pr_info("%s: timer id=%d, charging_state=%d, last_update_time=%lu\n",
		__func__, pca9481->timer_id, pca9481->charging_state, pca9481->last_update_time);
#else
	pr_info("%s: timer id=%d, charging_state=%d\n",
		__func__, pca9481->timer_id, pca9481->charging_state);
#endif

	switch (pca9481->timer_id) {
	case TIMER_VBATMIN_CHECK:
		ret = pca9481_check_vbatmin(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PRESET_DC:
		ret = pca9481_start_direct_charging(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PRESET_CONFIG:
		ret = pca9481_preset_config(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_ACTIVE:
		ret = pca9481_check_active_state(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ADJUST_CCMODE:
		ret = pca9481_charge_adjust_ccmode(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ENTER_CCMODE:
		ret = pca9481_charge_start_ccmode(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_CCMODE:
		ret = pca9481_charge_ccmode(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ENTER_CVMODE:
		/* Enter Pre-CV mode */
		ret = pca9481_charge_start_cvmode(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_CVMODE:
		ret = pca9481_charge_cvmode(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PDMSG_SEND:
		/* Adjust TA current and voltage step */
		if (pca9481->ta_type == TA_TYPE_WIRELESS) {
			val = pca9481->ta_vol/WCRX_VOL_STEP;		/* RX voltage resolution is 100mV */
			pca9481->ta_vol = val*WCRX_VOL_STEP;

			/* Set RX voltage */
			ret = pca9481_send_rx_voltage(pca9481, WCRX_REQUEST_VOLTAGE);
		} else {
			val = pca9481->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
			pca9481->ta_vol = val*PD_MSG_TA_VOL_STEP;
			val = pca9481->ta_cur/PD_MSG_TA_CUR_STEP;	/* PPS current resolution is 50mA */
			pca9481->ta_cur = val*PD_MSG_TA_CUR_STEP;
			if (pca9481->ta_cur < TA_MIN_CUR)	/* PPS minimum current is 1000mA */
				pca9481->ta_cur = TA_MIN_CUR;

			/* Send PD Message */
			ret = pca9481_send_pd_message(pca9481, PD_MSG_REQUEST_APDO);
		}
		if (ret < 0)
			goto error;

		/* Go to the next state */
		mutex_lock(&pca9481->lock);
		switch (pca9481->charging_state) {
		case DC_STATE_PRESET_DC:
			pca9481->timer_id = TIMER_PRESET_CONFIG;
			break;
		case DC_STATE_ADJUST_CC:
			pca9481->timer_id = TIMER_ADJUST_CCMODE;
			break;
		case DC_STATE_START_CC:
			pca9481->timer_id = TIMER_ENTER_CCMODE;
			break;
		case DC_STATE_CC_MODE:
			pca9481->timer_id = TIMER_CHECK_CCMODE;
			break;
		case DC_STATE_START_CV:
			pca9481->timer_id = TIMER_ENTER_CVMODE;
			break;
		case DC_STATE_CV_MODE:
			pca9481->timer_id = TIMER_CHECK_CVMODE;
			break;
		case DC_STATE_ADJUST_TAVOL:
			pca9481->timer_id = TIMER_ADJUST_TAVOL;
			break;
		case DC_STATE_ADJUST_TACUR:
			pca9481->timer_id = TIMER_ADJUST_TACUR;
			break;
		case DC_STATE_BYPASS_MODE:
			pca9481->timer_id = TIMER_CHECK_BYPASSMODE;
			break;
		case DC_STATE_DCMODE_CHANGE:
			pca9481->timer_id = TIMER_DCMODE_CHANGE;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		pca9481->timer_period = PDMSG_WAIT_T;
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
		break;

	case TIMER_ADJUST_TAVOL:
		ret = pca9481_adjust_ta_voltage(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ADJUST_TACUR:
		ret = pca9481_adjust_ta_current(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_BYPASSMODE:
		ret = pca9481_charge_bypass_mode(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_DCMODE_CHANGE:
		ret = pca9481_charge_dcmode_change(pca9481);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ID_NONE:
		ret = pca9481_stop_charging(pca9481);
		if (ret < 0)
			goto error;
		break;

	default:
		break;
	}

	/* Check the charging state again */
	if (pca9481->charging_state == DC_STATE_NO_CHARGING) {
		/* Cancel work queue again */
		cancel_delayed_work(&pca9481->timer_work);
		cancel_delayed_work(&pca9481->pps_work);
	}
	return;

error:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	pca9481->health_status = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
#endif
	pca9481_stop_charging(pca9481);
}

/* delayed work function for pps periodic timer */
static void pca9481_pps_request_work(struct work_struct *work)
{
	struct pca9481_charger *pca9481 = container_of(work, struct pca9481_charger,
						 pps_work.work);

	int ret = 0;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int vin, iin;

	/* this is for wdt */
	vin = pca9481_read_adc(pca9481, ADCCH_VIN);
	iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
	pr_info("%s: pps_work_start (vin:%dmV, iin:%dmA)\n",
			__func__, vin/PCA9481_SEC_DENOM_U_M, iin/PCA9481_SEC_DENOM_U_M);
#else
	pr_info("%s: pps_work_start\n", __func__);
#endif

#if defined(CONFIG_SEND_PDMSG_IN_PPS_REQUEST_WORK)
	/* Send PD message */
	ret = pca9481_send_pd_message(pca9481, PD_MSG_REQUEST_APDO);
#endif
	pr_info("%s: End, ret=%d\n", __func__, ret);
}

static int pca9481_hw_init(struct pca9481_charger *pca9481)
{
	unsigned int val;
	int ret;
	u8 mask[REG_BUFFER_MAX];

	pr_info("%s: =========START=========\n", __func__);

	/* Read Device info register */
	ret = pca9481_read_reg(pca9481, PCA9481_REG_DEVICE_ID, &val);
	if ((ret < 0) || (val != PCA9481_DEVICE_ID)) {
		/* Read Device info register again */
		ret = pca9481_read_reg(pca9481, PCA9481_REG_DEVICE_ID, &val);
		if ((ret < 0) || (val != PCA9481_DEVICE_ID)) {
			dev_err(pca9481->dev, "reading DEVICE_INFO failed, val=0x%x\n", val);
			ret = -EINVAL;
			return ret;
		}
	}
	dev_info(pca9481->dev, "%s: reading DEVICE_INFO, val=0x%x\n", __func__, val);

	/*
	 * Program the platform specific configuration values to the device
	 * first.
	 */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
	pca9481->health_status = POWER_SUPPLY_HEALTH_GOOD;
#endif

	/* Set OV_TRACK_DELTA to 800mV */
	val = OV_TRACK_DELTA_800mV << MASK2SHIFT(PCA9481_BIT_OV_TRACK_DELTA);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_SC_CNTL_2,
							PCA9481_BIT_OV_TRACK_DELTA, val);
	if (ret < 0)
		return ret;

	/* Set UV_TRACK_DELTA to 600mV */
	val = UV_TRACK_DELTA_600mV << MASK2SHIFT(PCA9481_BIT_UV_TRACK_DELTA);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_SC_CNTL_1,
							PCA9481_BIT_UV_TRACK_DELTA, val);
	if (ret < 0)
		return ret;

	/* Set Switching Frequency - kHz unit */
	val = PCA9481_FSW_CFG(pca9481->pdata->fsw_cfg);
	ret = pca9481_write_reg(pca9481, PCA9481_REG_SC_CNTL_0, val);
	if (ret < 0)
		return ret;

	/* Set Reverse Current Detection */
	val = PCA9481_BIT_RCP_EN;
	ret = pca9481_update_reg(pca9481, PCA9481_REG_RCP_CNTL,
							PCA9481_BIT_RCP_EN, val);
	if (ret < 0)
		return ret;

	/* Set EN pin polarity - Disable PCA9481 */
	val = PCA9481_EN_ACTIVE_H;
	ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_2,
				PCA9481_BIT_EN_CFG, val);
	if (ret < 0)
		return ret;

	/* Set standby mode*/
	val = PCA9481_STANDBY_FORCE;
	ret = pca9481_update_reg(pca9481, PCA9481_REG_SC_CNTL_3,
				PCA9481_BIT_STANDBY_EN | PCA9481_BIT_SC_OPERATION_MODE,
				val | PCA9481_SC_OP_21);
	if (ret < 0)
		return ret;

	/* Die Temperature regulation 120'C */
	val = 0x3 << MASK2SHIFT(PCA9481_BIT_THERMAL_REGULATION_CFG);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_1,
				PCA9481_BIT_THERMAL_REGULATION_CFG | PCA9481_BIT_THERMAL_REGULATION_EN,
				val | PCA9481_BIT_THERMAL_REGULATION_EN);
	if (ret < 0)
		return ret;

	/* Set external sense resistor value */
	val = pca9481->pdata->snsres << MASK2SHIFT(PCA9481_BIT_IBAT_SENSE_R_SEL);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_CHARGING_CNTL_4,
				PCA9481_BIT_IBAT_SENSE_R_SEL, val);
	if (ret < 0)
		return ret;

	/* Set external sense resistor location */
	val = pca9481->pdata->snsres_cfg << MASK2SHIFT(PCA9481_BIT_IBAT_SENSE_R_CFG);
	ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_2,
				PCA9481_BIT_IBAT_SENSE_R_CFG, val);
	if (ret < 0)
		return ret;

	/* Disable battery charge current regulation loop */
	/* Disable current measurement through CSP and CSN */
	val = 0;
	ret = pca9481_update_reg(pca9481, PCA9481_REG_CHARGING_CNTL_0,
				PCA9481_BIT_CSP_CSN_MEASURE_EN | PCA9481_BIT_I_VBAT_LOOP_EN,
				val);
	if (ret < 0)
		return ret;

	/* Set the ADC channel */
	val = (PCA9481_BIT_ADC_READ_VIN_CURRENT_EN |	/* IIN ADC */
		   PCA9481_BIT_ADC_READ_DIE_TEMP_EN |		/* DIE_TEMP ADC */
		   PCA9481_BIT_ADC_READ_NTC_EN |			/* NTC ADC */
		   PCA9481_BIT_ADC_READ_VOUT_EN |			/* VOUT ADC */
		   PCA9481_BIT_ADC_READ_BATP_BATN_EN |		/* VBAT ADC */
		   PCA9481_BIT_ADC_READ_OVP_OUT_EN |		/* OVP_OUT ADC */
		   PCA9481_BIT_ADC_READ_VIN_EN);			/* VIN ADC */

	ret = pca9481_write_reg(pca9481, PCA9481_REG_ADC_EN_CNTL_0, val);
	if (ret < 0)
		return ret;

	/* Enable ADC */
	val = PCA9481_BIT_ADC_EN | ((unsigned int)(ADC_AVG_16sample << MASK2SHIFT(PCA9481_BIT_ADC_AVERAGE_TIMES)));
	ret = pca9481_write_reg(pca9481, PCA9481_REG_ADC_CNTL, val);
	if (ret < 0)
		return ret;

	/*
	 * Configure the Mask Register for interrupts: disable all interrupts by default.
	 */
	mask[REG_DEVICE_0] = 0xFF;
	mask[REG_DEVICE_1] = 0xFF;
	mask[REG_DEVICE_2] = 0xFF;
	mask[REG_DEVICE_3] = 0xFF;
	mask[REG_CHARGING] = 0xFF;
	mask[REG_SC_0] = 0xFF;
	mask[REG_SC_1] = 0xFF;
	ret = regmap_bulk_write(pca9481->regmap, PCA9481_REG_INT_DEVICE_0_MASK | PCA9481_REG_BIT_AI,
							mask, REG_BUFFER_MAX);
	if (ret < 0)
		return ret;

	/* input current - uA*/
	ret = pca9481_set_input_current(pca9481, pca9481->pdata->iin_cfg);
	if (ret < 0)
		return ret;

	/* charging current */
	ret = pca9481_set_charging_current(pca9481, pca9481->pdata->ichg_cfg);
	if (ret < 0)
		return ret;

	/* float voltage */
	ret = pca9481_set_vfloat(pca9481, pca9481->pdata->vfloat);
	if (ret < 0)
		return ret;

	/* Save initial charging parameters */
	pca9481->iin_cfg = pca9481->pdata->iin_cfg;
	pca9481->ichg_cfg = pca9481->pdata->ichg_cfg;
	pca9481->vfloat = pca9481->pdata->vfloat;
	pca9481->iin_topoff = pca9481->pdata->iin_topoff;

	/* Initial TA control method is Current Limit mode */
	pca9481->ta_ctrl = TA_CTRL_CL_MODE;

	return ret;
}


static irqreturn_t pca9481_interrupt_handler(int irq, void *data)
{
	struct pca9481_charger *pca9481 = data;
	u8 int_reg[REG_BUFFER_MAX], sts_reg[REG_BUFFER_MAX], mask_reg[REG_BUFFER_MAX];
	u8 masked_int[REG_BUFFER_MAX];	/* masked int */
	bool handled = false;
	int ret, i;

	/* Read interrupt registers */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_INT_DEVICE_0 | PCA9481_REG_BIT_AI, int_reg, REG_BUFFER_MAX);
	if (ret < 0) {
		dev_err(pca9481->dev, "reading Interrupt registers failed\n");
		handled = false;
		goto error;
	}
	pr_info("%s: INT reg[0x01]=0x%x,[0x02]=0x%x,[0x03]=0x%x,[0x04]=0x%x,[0x05]=0x%x,[0x06]=0x%x,[0x07]=0x%x\n",
			__func__, int_reg[0], int_reg[1], int_reg[2], int_reg[3], int_reg[4], int_reg[5], int_reg[6]);

	/* Read mask registers */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_INT_DEVICE_0_MASK | PCA9481_REG_BIT_AI, mask_reg, REG_BUFFER_MAX);
	if (ret < 0) {
		dev_err(pca9481->dev, "reading Mask registers failed\n");
		handled = false;
		goto error;
	}
	pr_info("%s: MASK reg[0x08]=0x%x,[0x09]=0x%x,[0x0A]=0x%x,[0x0B]=0x%x,[0x0C]=0x%x,[0x0D]=0x%x,[0x0E]=0x%x\n",
			__func__, mask_reg[0], mask_reg[1], mask_reg[2], mask_reg[3], mask_reg[4], mask_reg[5], mask_reg[6]);

	/* Read status registers */
	ret = pca9481_bulk_read_reg(pca9481, PCA9481_REG_DEVICE_0_STS | PCA9481_REG_BIT_AI, sts_reg, REG_BUFFER_MAX);
	if (ret < 0) {
		dev_err(pca9481->dev, "reading Status registers failed\n");
		handled = false;
		goto error;
	}
	pr_info("%s: STS reg[0x0F]=0x%x,[0x10]=0x%x,[0x11]=0x%x,[0x12]=0x%x,[0x13]=0x%x,[0x14]=0x%x,[0x15]=0x%x\n",
			__func__, sts_reg[0], sts_reg[1], sts_reg[2], sts_reg[3], sts_reg[4], sts_reg[5], sts_reg[6]);

	/* Check the masked interrupt */
	for (i = 0; i < REG_BUFFER_MAX; i++)
		masked_int[i] = int_reg[i] & !mask_reg[i];

	pr_info("%s: Masked INT reg[0x01]=0x%x,[0x02]=0x%x,[0x03]=0x%x,[0x04]=0x%x,[0x05]=0x%x,[0x06]=0x%x,[0x07]=0x%x\n",
			__func__, masked_int[0], masked_int[1], masked_int[2], masked_int[3], masked_int[4], masked_int[5], masked_int[6]);

	handled = true;

	/* Should implement code by a customer if pca9481 needs additional functions or actions */

error:
	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int pca9481_irq_init(struct pca9481_charger *pca9481,
			   struct i2c_client *client)
{
	const struct pca9481_platform_data *pdata = pca9481->pdata;
	int ret, msk[REG_BUFFER_MAX], irq;

	pr_info("%s: =========START=========\n", __func__);

	irq = gpio_to_irq(pdata->irq_gpio);

	ret = gpio_request_one(pdata->irq_gpio, GPIOF_IN, client->name);
	if (ret < 0)
		goto fail;

	ret = request_threaded_irq(irq, NULL, pca9481_interrupt_handler,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   client->name, pca9481);
	if (ret < 0)
		goto fail_gpio;

	/*
	 * Configure the Mask Register for interrupts: disable all interrupts by default.
	 */
	msk[REG_DEVICE_0] = 0xFF;
	msk[REG_DEVICE_1] = 0xFF;
	msk[REG_DEVICE_2] = 0xFF;
	msk[REG_DEVICE_3] = 0xFF;
	msk[REG_CHARGING] = 0xFF;
	msk[REG_SC_0] = 0xFF;
	msk[REG_SC_1] = 0xFF;

	ret = regmap_bulk_write(pca9481->regmap, PCA9481_REG_INT_DEVICE_0_MASK | PCA9481_REG_BIT_AI,
							msk, REG_BUFFER_MAX);
	if (ret < 0)
		goto fail_write;

	client->irq = irq;
	return 0;

fail_write:
	free_irq(irq, pca9481);
fail_gpio:
	gpio_free(pdata->irq_gpio);
fail:
	client->irq = 0;
	return ret;
}


/*
 * Returns the input current limit programmed
 * into the charger in uA.
 */
static int get_input_current_limit(struct pca9481_charger *pca9481)
{
	int ret, intval;
	unsigned int val;

	ret = pca9481_read_reg(pca9481, PCA9481_REG_CHARGING_CNTL_1, &val);
	if (ret < 0)
		return ret;

	intval = val * PCA9481_IIN_REG_STEP + PCA9481_IIN_REG_MIN;

	if (intval > PCA9481_IIN_REG_MAX)
		intval = PCA9481_IIN_REG_MAX;

	return intval;
}

/*
 * Returns the constant charge current programmed
 * into the charger in uA.
 */
static int get_const_charge_current(struct pca9481_charger *pca9481)
{
	int ret, intval;
	unsigned int val;

	ret = pca9481_read_reg(pca9481, PCA9481_REG_CHARGING_CNTL_3, &val);
	if (ret < 0)
		return ret;

	intval = val * PCA9481_IBAT_REG_STEP + PCA9481_IBAT_REG_MIN;

	if (intval > PCA9481_IBAT_REG_MAX)
		intval = PCA9481_IBAT_REG_MAX;

	return intval;
}

/*
 * Returns the constant charge voltage programmed
 * into the charger in uV.
 */
static int get_const_charge_voltage(struct pca9481_charger *pca9481)
{
	int ret, intval;
	unsigned int val;

	ret = pca9481_read_reg(pca9481, PCA9481_REG_CHARGING_CNTL_2, &val);
	if (ret < 0)
		return ret;

	intval = val * PCA9481_VBAT_REG_STEP + PCA9481_VBAT_REG_MIN;

	if (intval > PCA9481_VBAT_REG_MAX)
		intval = PCA9481_VBAT_REG_MAX;

	return intval;
}

/*
 * Returns the enable or disable value.
 * into 1 or 0.
 */
static int get_charging_enabled(struct pca9481_charger *pca9481)
{
	int ret, intval;
	unsigned int val;

	ret = pca9481_read_reg(pca9481, PCA9481_REG_SC_CNTL_3, &val);
	if (ret < 0)
		return ret;

	intval = (val & PCA9481_BIT_STANDBY_EN) ? 0 : 1;

	return intval;
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
/*
 * Returns the system current in uV.
 */
static int get_system_current(struct pca9481_charger *pca9481)
{
	/* get the system current */
	/* get the battery power supply to get charging current */
	union power_supply_propval val;
	struct power_supply *psy;
	int ret;
	int iin, ibat, isys;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		dev_err(pca9481->dev, "Cannot find battery power supply\n");
		goto error;
	}

	/* get the charging current */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(psy);
	if (ret < 0) {
		dev_err(pca9481->dev, "Cannot get battery current from FG\n");
		goto error;
	}
	ibat = val.intval;

	/* calculate the system current */
	/* get input current */
	iin = pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
	if (iin < 0) {
		dev_err(pca9481->dev, "Invalid IIN ADC\n");
		goto error;
	}

	/* calculate the system current */
	/* Isys = (Iin - Ifsw_cfg)*2 - Ibat */
	iin = (iin - iin_fsw_cfg[pca9481->pdata->fsw_cfg])*2;
	iin /= PCA9481_SEC_DENOM_U_M;
	isys = iin - ibat;
	pr_info("%s: isys=%dmA\n", __func__, isys);

	return isys;

error:
	return -EINVAL;
}
#endif

static int pca9481_chg_set_adc_force_mode(struct pca9481_charger *pca9481, u8 enable)
{
	unsigned int temp = 0;
	int ret = 0;

	if (enable) {
		/* Disable low power mode */
		temp = PCA9481_BIT_LOW_POWER_MODE_DISABLE;
		ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_1,
				PCA9481_BIT_LOW_POWER_MODE_DISABLE, temp);
		if (ret < 0)
			return ret;

		/* Set Forced Normal Mode to ADC mode */
		temp = FORCE_NORMAL_MODE << MASK2SHIFT(PCA9481_BIT_ADC_MODE_CFG);
		ret = pca9481_update_reg(pca9481, PCA9481_REG_ADC_CNTL,
				PCA9481_BIT_ADC_MODE_CFG, temp);
		if (ret < 0)
			return ret;

		/* Wait 2ms to update ADC */
		usleep_range(2000, 3000);
	} else {
		/* Set Auto Mode to ADC mode */
		temp = AUTO_MODE << MASK2SHIFT(PCA9481_BIT_ADC_MODE_CFG);
		ret = pca9481_update_reg(pca9481, PCA9481_REG_ADC_CNTL,
				PCA9481_BIT_ADC_MODE_CFG, temp);
		if (ret < 0)
			return ret;

		/* Enable low power mode */
		temp = 0;
		ret = pca9481_update_reg(pca9481, PCA9481_REG_DEVICE_CNTL_1,
				PCA9481_BIT_LOW_POWER_MODE_DISABLE, temp);
		if (ret < 0)
			return ret;
	}

	ret = pca9481_read_reg(pca9481, PCA9481_REG_ADC_CNTL, &temp);
	pr_info("%s: ADC_CTRL : 0x%02x\n", __func__, temp);

	return ret;
}

static int pca9481_chg_set_property(struct power_supply *psy,
				     enum power_supply_property prop,
				     const union power_supply_propval *val)
{
	struct pca9481_charger *pca9481 = power_supply_get_drvdata(psy);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) prop;
	unsigned int temp = 0;
#endif
	int ret = 0;

	pr_info("%s: =========START=========\n", __func__);
	pr_info("%s: prop=%d, val=%d\n", __func__, prop, val->intval);

	switch ((int)prop) {
	/* Todo - Insert code */
	/* It needs modification by a customer */

	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval == 0) {
			pca9481->mains_online = false;
			/* Cancel delayed work */
			cancel_delayed_work(&pca9481->timer_work);
			cancel_delayed_work(&pca9481->pps_work);
			/* Stop Direct Charging	*/
			mutex_lock(&pca9481->lock);
			pca9481->timer_id = TIMER_ID_NONE;
			pca9481->timer_period = 0;
			mutex_unlock(&pca9481->lock);
			queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));

			pca9481->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
			pca9481->health_status = POWER_SUPPLY_HEALTH_GOOD;

		} else {
			/* Start Direct charging */
			pca9481->mains_online = true;
			ret = 0;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		/* Set the USBPD-TA is plugged in or out */
		pca9481->mains_online = val->intval;
		break;

	case POWER_SUPPLY_PROP_TYPE:
		/* Set power supply type */
		if (val->intval == POWER_SUPPLY_TYPE_WIRELESS) {
			/* The current power supply type is wireless charger */
			pca9481->ta_type = TA_TYPE_WIRELESS;
			pr_info("%s: The current power supply type is WC, ta_type=%d\n", __func__, pca9481->ta_type);
		} else {
			/* Default TA type is USBPD TA */
			pca9481->ta_type = TA_TYPE_USBPD;
			pr_info("%s: The current power supply type is USBPD, ta_type=%d\n", __func__, pca9481->ta_type);
		}
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9481->float_voltage = val->intval;
		temp = pca9481->float_voltage * PCA9481_SEC_DENOM_U_M;
		if (temp != pca9481->new_vfloat) {
			/* request new float voltage */
			pca9481->new_vfloat = temp;
			/* Check the charging state */
			if ((pca9481->charging_state == DC_STATE_NO_CHARGING) ||
				(pca9481->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Apply new vfloat when the direct charging is started */
				pca9481->vfloat = pca9481->new_vfloat;
			} else {
				/* Check whether the previous request is done or not */
				if (pca9481->req_new_vfloat == true) {
					/* The previous request is not done yet */
					pr_err("%s: There is the previous request for New vfloat\n", __func__);
					ret = -EBUSY;
				} else {
					/* Set request flag */
					mutex_lock(&pca9481->lock);
					pca9481->req_new_vfloat = true;
					mutex_unlock(&pca9481->lock);

					/* Check the charging state */
					if ((pca9481->charging_state == DC_STATE_CC_MODE) ||
						(pca9481->charging_state == DC_STATE_CV_MODE) ||
						(pca9481->charging_state == DC_STATE_BYPASS_MODE) ||
						(pca9481->charging_state == DC_STATE_CHARGING_DONE)) {
						/* cancel delayed_work */
						cancel_delayed_work(&pca9481->timer_work);
						/* do delayed work at once */
						mutex_lock(&pca9481->lock);
						pca9481->timer_period = 0;	// ms unit
						mutex_unlock(&pca9481->lock);
						queue_delayed_work(pca9481->dc_wq,
											&pca9481->timer_work,
											msecs_to_jiffies(pca9481->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						pr_info("%s: Not support new vfloat yet in charging state=%d\n",
								__func__, pca9481->charging_state);
					}
				}
			}
		}
#else
		if (val->intval != pca9481->new_vfloat) {
			/* request new float voltage */
			pca9481->new_vfloat = val->intval;
			/* Check the charging state */
			if ((pca9481->charging_state == DC_STATE_NO_CHARGING) ||
				(pca9481->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Apply new vfloat when the direct charging is started */
				pca9481->vfloat = pca9481->new_vfloat;
			} else {
				/* Check whether the previous request is done or not */
				if (pca9481->req_new_vfloat == true) {
					/* The previous request is not done yet */
					pr_err("%s: There is the previous request for New vfloat\n", __func__);
					ret = -EBUSY;
				} else {
					/* Set request flag */
					mutex_lock(&pca9481->lock);
					pca9481->req_new_vfloat = true;
					mutex_unlock(&pca9481->lock);

					/* Check the charging state */
					if ((pca9481->charging_state == DC_STATE_CC_MODE) ||
						(pca9481->charging_state == DC_STATE_CV_MODE) ||
						(pca9481->charging_state == DC_STATE_BYPASS_MODE) ||
						(pca9481->charging_state == DC_STATE_CHARGING_DONE)) {
						/* cancel delayed_work */
						cancel_delayed_work(&pca9481->timer_work);
						/* do delayed work at once */
						mutex_lock(&pca9481->lock);
						pca9481->timer_period = 0;	// ms unit
						mutex_unlock(&pca9481->lock);
						queue_delayed_work(pca9481->dc_wq,
											&pca9481->timer_work,
											msecs_to_jiffies(pca9481->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						pr_info("%s: Not support new vfloat yet in charging state=%d\n",
								__func__, pca9481->charging_state);
					}
				}
			}
		}
#endif
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9481->input_current = val->intval;
		temp = pca9481->input_current * PCA9481_SEC_DENOM_U_M;
		if (temp != pca9481->new_iin) {
			/* request new input current */
			pca9481->new_iin = temp;
			/* Check the charging state */
			if ((pca9481->charging_state == DC_STATE_NO_CHARGING) ||
				(pca9481->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Apply new iin when the direct charging is started */
				pca9481->iin_cfg = pca9481->new_iin;
			} else {
				/* Check whether the previous request is done or not */
				if (pca9481->req_new_iin == true) {
					/* The previous request is not done yet */
					pr_err("%s: There is the previous request for New iin\n", __func__);
					ret = -EBUSY;
				} else {
					/* Set request flag */
					mutex_lock(&pca9481->lock);
					pca9481->req_new_iin = true;
					mutex_unlock(&pca9481->lock);
					/* Check the charging state */
					if ((pca9481->charging_state == DC_STATE_CC_MODE) ||
						(pca9481->charging_state == DC_STATE_CV_MODE) ||
						(pca9481->charging_state == DC_STATE_BYPASS_MODE) ||
						(pca9481->charging_state == DC_STATE_CHARGING_DONE)) {
						/* cancel delayed_work */
						cancel_delayed_work(&pca9481->timer_work);
						/* do delayed work at once */
						mutex_lock(&pca9481->lock);
						pca9481->timer_period = 0;	// ms unit
						mutex_unlock(&pca9481->lock);
						queue_delayed_work(pca9481->dc_wq,
											&pca9481->timer_work,
											msecs_to_jiffies(pca9481->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						pr_info("%s: Not support new iin yet in charging state=%d\n",
								__func__, pca9481->charging_state);
					}
				}
			}
		}
#else
		if (val->intval != pca9481->new_iin) {
			/* request new input current */
			pca9481->new_iin = val->intval;
			/* Check the charging state */
			if ((pca9481->charging_state == DC_STATE_NO_CHARGING) ||
				(pca9481->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Apply new iin when the direct charging is started */
				pca9481->iin_cfg = pca9481->new_iin;
			} else {
				/* Check whether the previous request is done or not */
				if (pca9481->req_new_iin == true) {
					/* The previous request is not done yet */
					pr_err("%s: There is the previous request for New iin\n", __func__);
					ret = -EBUSY;
				} else {
					/* Set request flag */
					mutex_lock(&pca9481->lock);
					pca9481->req_new_iin = true;
					mutex_unlock(&pca9481->lock);
					/* Check the charging state */
					if ((pca9481->charging_state == DC_STATE_CC_MODE) ||
						(pca9481->charging_state == DC_STATE_CV_MODE) ||
						(pca9481->charging_state == DC_STATE_BYPASS_MODE) ||
						(pca9481->charging_state == DC_STATE_CHARGING_DONE)) {
						/* cancel delayed_work */
						cancel_delayed_work(&pca9481->timer_work);
						/* do delayed work at once */
						mutex_lock(&pca9481->lock);
						pca9481->timer_period = 0;	// ms unit
						mutex_unlock(&pca9481->lock);
						queue_delayed_work(pca9481->dc_wq,
											&pca9481->timer_work,
											msecs_to_jiffies(pca9481->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						pr_info("%s: Not support new iin yet in charging state=%d\n",
								__func__, pca9481->charging_state);
					}
				}
			}
		}
#endif
		break;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		pca9481->pdata->vfloat = val->intval * PCA9481_SEC_DENOM_U_M;
		pr_info("%s: v_float(%duV)\n", __func__, pca9481->pdata->vfloat);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		pca9481->charging_current = val->intval;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			if (val->intval) {
				pca9481->wdt_kick = true;
			} else {
				pca9481->wdt_kick = false;
				cancel_delayed_work(&pca9481->wdt_control_work);
			}
			pr_info("%s: wdt kick (%d)\n", __func__, pca9481->wdt_kick);
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			pca9481->input_current = val->intval;
			temp = pca9481->input_current * PCA9481_SEC_DENOM_U_M;
			if (temp != pca9481->new_iin) {
				/* request new input current */
				pca9481->new_iin = temp;
				/* Check the charging state */
				if ((pca9481->charging_state == DC_STATE_NO_CHARGING) ||
					(pca9481->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Apply new iin when the direct charging is started */
					pca9481->iin_cfg = pca9481->new_iin;
				} else {
					/* Check whether the previous request is done or not */
					if (pca9481->req_new_iin == true) {
						/* The previous request is not done yet */
						pr_err("%s: There is the previous request for New iin\n", __func__);
						ret = -EBUSY;
					} else {
						/* Set request flag */
						mutex_lock(&pca9481->lock);
						pca9481->req_new_iin = true;
						mutex_unlock(&pca9481->lock);
						/* Check the charging state */
						if ((pca9481->charging_state == DC_STATE_CC_MODE) ||
							(pca9481->charging_state == DC_STATE_CV_MODE) ||
							(pca9481->charging_state == DC_STATE_BYPASS_MODE) ||
							(pca9481->charging_state == DC_STATE_CHARGING_DONE)) {
							/* cancel delayed_work */
							cancel_delayed_work(&pca9481->timer_work);
							/* do delayed work at once */
							mutex_lock(&pca9481->lock);
							pca9481->timer_period = 0;	// ms unit
							mutex_unlock(&pca9481->lock);
							queue_delayed_work(pca9481->dc_wq,
												&pca9481->timer_work,
												msecs_to_jiffies(pca9481->timer_period));
						} else {
							/* Wait for next valid state - cc or cv state */
							pr_info("%s: Not support new iin yet in charging state=%d\n",
									__func__, pca9481->charging_state);
						}
					}
				}
				pr_info("## %s: input current(new_iin: %duA)\n", __func__, pca9481->new_iin);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL:
			pca9481_chg_set_adc_force_mode(pca9481, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			if (val->intval == 0) {
				/* Stop direct charging */
				ret = pca9481_stop_charging(pca9481);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9481->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
				pca9481->health_status = POWER_SUPPLY_HEALTH_GOOD;
#endif
				if (ret < 0)
					goto error;
			} else {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				if (pca9481->charging_state != DC_STATE_NO_CHARGING) {
					pr_info("## %s: duplicate charging enabled(%d)\n", __func__, val->intval);
					goto error;
				}
				if (!pca9481->mains_online) {
					pr_info("## %s: mains_online is not attached(%d)\n", __func__, val->intval);
					goto error;
				}
#endif
				/* Start Direct Charging */
				/* Set initial wake up timeout - 10s */
				pm_wakeup_ws_event(pca9481->monitor_wake_lock, INIT_WAKEUP_T, false);
				/* Start 1sec timer for battery check */
				mutex_lock(&pca9481->lock);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9481_set_charging_state(pca9481, DC_STATE_CHECK_VBAT);
#else
				pca9481->charging_state = DC_STATE_CHECK_VBAT;
#endif
				pca9481->timer_id = TIMER_VBATMIN_CHECK;
				pca9481->timer_period = VBATMIN_CHECK_T;	/* The delay time for PD state goes to PE_SNK_STATE */
				mutex_unlock(&pca9481->lock);
				queue_delayed_work(pca9481->dc_wq,
									&pca9481->timer_work,
									msecs_to_jiffies(pca9481->timer_period));
				ret = 0;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
			pr_info("[PASS_THROUGH] %s: called\n", __func__);
			if (val->intval != pca9481->new_dc_mode) {
				/* Request new dc mode */
				pca9481->new_dc_mode = val->intval;
				/* Check the charging state */
				if (pca9481->charging_state == DC_STATE_NO_CHARGING) {
					/* Not support state */
					pr_info("%s: Not support dc mode in charging state=%d\n", __func__, pca9481->charging_state);
					ret = -EINVAL;
				} else {
					/* Check whether the previous request is done or not */
					if (pca9481->req_new_dc_mode == true) {
						/* The previous request is not done yet */
						pr_err("%s: There is the previous request for New DC mode\n", __func__);
						ret = -EBUSY;
					} else {
						/* Set request flag */
						mutex_lock(&pca9481->lock);
						pca9481->req_new_dc_mode = true;
						mutex_unlock(&pca9481->lock);
						/* Check the charging state */
						if ((pca9481->charging_state == DC_STATE_CC_MODE) ||
							(pca9481->charging_state == DC_STATE_CV_MODE) ||
							(pca9481->charging_state == DC_STATE_BYPASS_MODE)) {
							/* cancel delayed_work */
							cancel_delayed_work(&pca9481->timer_work);
							/* do delayed work at once */
							mutex_lock(&pca9481->lock);
							pca9481->timer_period = 0;	// ms unit
							mutex_unlock(&pca9481->lock);
							queue_delayed_work(pca9481->dc_wq,
								&pca9481->timer_work,
								msecs_to_jiffies(pca9481->timer_period));
						} else {
							/* Wait for next valid state - cc, cv, or bypass state */
							pr_info("%s: Not support new dc mode yet in charging state=%d\n",
									__func__, pca9481->charging_state);
						}
					}
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			if ((pca9481->charging_state == DC_STATE_BYPASS_MODE) &&
				(pca9481->dc_mode != PTM_NONE)) {
				pr_info("[PASS_THROUGH_VOL] %s, bypass mode\n", __func__);
				/* Set TA voltage for bypass mode */
				pca9481_set_bypass_ta_voltage_by_soc(pca9481, val->intval);
			} else {
				pr_info("[PASS_THROUGH_VOL] %s, not bypass mode\n", __func__);
			}
			break;
		default:
			return -EINVAL;
		}
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

static int pca9481_chg_get_property(struct power_supply *psy,
				     enum power_supply_property prop,
				     union power_supply_propval *val)
{
	int ret = 0;
	struct pca9481_charger *pca9481 = power_supply_get_drvdata(psy);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) prop;
#endif

	pr_debug("%s: =========START=========\n", __func__);
	pr_debug("%s: prop=%d\n", __func__, prop);

	switch ((int)prop) {
	case POWER_SUPPLY_PROP_PRESENT:
		/* TA present */
		val->intval = pca9481->mains_online;
		break;

	case POWER_SUPPLY_PROP_TYPE:
		if (pca9481->ta_type == TA_TYPE_WIRELESS)
			val->intval = POWER_SUPPLY_TYPE_WIRELESS;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = pca9481->mains_online;
		break;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = pca9481->chg_status;
		pr_info("%s: CHG STATUS : %d\n", __func__, pca9481->chg_status);
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (pca9481->charging_state >= DC_STATE_CHECK_ACTIVE &&
			pca9481->charging_state <= DC_STATE_CV_MODE)
			ret = pca9481_check_error(pca9481);

		val->intval = pca9481->health_status;
		pr_info("%s: HEALTH STATUS : %d, ret = %d\n",
			__func__, pca9481->health_status, ret);
		ret = 0;
		break;
#endif

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = get_const_charge_voltage(pca9481);
		if (ret < 0) {
			val->intval = 0;
		} else {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			val->intval = pca9481->float_voltage;
#else
			val->intval = ret;
#endif
			ret = 0;
		}
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = get_const_charge_current(pca9481);
		if (ret < 0) {
			val->intval = 0;
		} else {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			val->intval = pca9481->charging_current;
#else
			val->intval = ret;
#endif
			ret = 0;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = get_input_current_limit(pca9481);
		if (ret < 0) {
			val->intval = 0;
		} else {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			val->intval = ret / PCA9481_SEC_DENOM_U_M;
#else
			val->intval = ret;
#endif
			ret = 0;
		}
		break;

	case POWER_SUPPLY_PROP_TEMP:
		/* return NTC voltage  - uV unit */
		ret = pca9481_read_adc(pca9481, ADCCH_NTC);
		if (ret < 0) {
			val->intval = 0;
		} else {
			val->intval = ret;
			ret = 0;
		}
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* return the output current - uA unit */
		/* check charging status */
		if (pca9481->charging_state == DC_STATE_NO_CHARGING) {
			/* return invalid */
			val->intval = 0;
			ret = 0;
		} else {
			int iin;
			/* get ibat current */
			iin = pca9481_read_adc(pca9481, ADCCH_BAT_CURRENT);
			if (ret < 0) {
				dev_err(pca9481->dev, "Invalid IBAT ADC\n");
				val->intval = 0;
			} else {
				val->intval = iin;
				ret = 0;
			}
		}
		break;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			//pca9481_monitor_work(pca9481);
			//pca9481_test_read(pca9481);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			switch (val->intval) {
			case SEC_BATTERY_IIN_MA:
				pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
				val->intval = pca9481->adc_val[ADCCH_VIN_CURRENT];
				break;
			case SEC_BATTERY_IIN_UA:
				pca9481_read_adc(pca9481, ADCCH_VIN_CURRENT);
				val->intval = pca9481->adc_val[ADCCH_VIN_CURRENT] * PCA9481_SEC_DENOM_U_M;
				break;
			case SEC_BATTERY_VIN_MA:
				pca9481_read_adc(pca9481, ADCCH_VIN);
				val->intval = pca9481->adc_val[ADCCH_VIN];
				break;
			case SEC_BATTERY_VIN_UA:
				pca9481_read_adc(pca9481, ADCCH_VIN);
				val->intval = pca9481->adc_val[ADCCH_VIN] * PCA9481_SEC_DENOM_U_M;
				break;
			default:
				val->intval = 0;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
			/* return system current - uA unit */
			/* check charging status */
			if (pca9481->charging_state == DC_STATE_NO_CHARGING) {
				/* return invalid */
				val->intval = 0;
				return -EINVAL;
			}
			/* calculate Isys */
			ret = get_system_current(pca9481);
			if (ret < 0)
				return 0;

			val->intval = ret;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
			val->strval = charging_state_str[pca9481->charging_state];
			pr_info("%s: CHARGER_STATUS(%s)\n", __func__, val->strval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			ret = get_charging_enabled(pca9481);
			if (ret < 0)
				return ret;

			val->intval = ret;
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			break;
		default:
			return -EINVAL;
		}
		break;
#endif
	default:
		ret = -EINVAL;
	}

	pr_debug("%s: End, prop=%d, val=%d, ret=%d\n", __func__, prop, val->intval, ret);
	return ret;
}

static enum power_supply_property pca9481_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct regmap_config pca9481_regmap = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= PCA9481_MAX_REGISTER,
};

static char *pca9481_supplied_to[] = {
	"pca9481-charger",
};

static const struct power_supply_desc pca9481_mains_desc = {
	.name		= "pca9481-charger",
	.type		= POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property	= pca9481_chg_get_property,
	.set_property	= pca9481_chg_set_property,
	.properties	= pca9481_charger_props,
	.num_properties	= ARRAY_SIZE(pca9481_charger_props),
};

#if defined(CONFIG_OF)
static int pca9481_charger_parse_dt(struct device *dev,
				struct pca9481_platform_data *pdata)
{
	struct device_node *np_pca9481 = dev->of_node;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct device_node *np;
#endif
	int ret;

	if (!np_pca9481)
		return -EINVAL;

	/* irq gpio */
	pdata->irq_gpio = of_get_named_gpio(np_pca9481, "pca9481,irq-gpio", 0);
	if (pdata->irq_gpio < 0)
		pr_err("%s : cannot get irq-gpio : %d\n",
			__func__, pdata->irq_gpio);
	else
		pr_info("%s: irq-gpio: %d\n", __func__, pdata->irq_gpio);

	/* input current limit */
	ret = of_property_read_u32(np_pca9481, "pca9481,input-current-limit",
						   &pdata->iin_cfg);
	if (ret) {
		pr_info("%s: nxp,input-current-limit is Empty\n", __func__);
		pdata->iin_cfg = PCA9481_IIN_REG_DFT;
	}
	pr_info("%s: pca9481,iin_cfg is %d\n", __func__, pdata->iin_cfg);

	/* charging current */
	ret = of_property_read_u32(np_pca9481, "pca9481,charging-current",
							   &pdata->ichg_cfg);
	if (ret) {
		pr_info("%s: pca9481,charging-current is Empty\n", __func__);
		pdata->ichg_cfg = PCA9481_IBAT_REG_DFT;
	}
	pr_info("%s: pca9481,ichg_cfg is %d\n", __func__, pdata->ichg_cfg);

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	/* charging float voltage */
	ret = of_property_read_u32(np_pca9481, "pca9481,float-voltage",
							   &pdata->vfloat);
	if (ret) {
		pr_info("%s: pca9481,float-voltage is Empty\n", __func__);
		pdata->vfloat = PCA9481_VBAT_REG_DFT;
	}
	pr_info("%s: pca9481,vfloat is %d\n", __func__, pdata->vfloat);
#endif

	/* input topoff current */
	ret = of_property_read_u32(np_pca9481, "pca9481,input-itopoff",
							   &pdata->iin_topoff);
	if (ret) {
		pr_info("%s: pca9481,input-itopoff is Empty\n", __func__);
		pdata->iin_topoff = IIN_DONE_DFT;
	}
	pr_info("%s: pca9481,iin_topoff is %d\n", __func__, pdata->iin_topoff);

	/* external sense resistor value */
	ret = of_property_read_u32(np_pca9481, "pca9481,sense-resistance",
							   &pdata->snsres);
	if (ret) {
		pr_info("%s: pca9481,sense-resistance is Empty\n", __func__);
		pdata->snsres = PCA9481_SENSE_R_DFT;
	}
	pr_info("%s: pca9481,snsres is %d\n", __func__, pdata->snsres);

	/* external sense resistor location */
	ret = of_property_read_u32(np_pca9481, "pca9481,sense-config",
							   &pdata->snsres_cfg);
	if (ret) {
		pr_info("%s: pca9481,sense-config is Empty\n", __func__);
		pdata->snsres_cfg = PCA9481_SENSE_R_CFG_DFT;
	}
	pr_info("%s: pca9481,snsres_cfg is %d\n", __func__, pdata->snsres_cfg);

	/* switching frequency */
	ret = of_property_read_u32(np_pca9481, "pca9481,switching-frequency",
							   &pdata->fsw_cfg);
	if (ret) {
		pr_info("%s: pca9481,switching frequency is Empty\n", __func__);
		pdata->fsw_cfg = PCA9481_FSW_CFG_DFT;
	}
	pr_info("%s: pca9481,fsw_cfg is %d\n", __func__, pdata->fsw_cfg);

	/* switching frequency - bypass */
	ret = of_property_read_u32(np_pca9481, "pca9481,switching-frequency-byp",
							   &pdata->fsw_cfg_byp);
	if (ret) {
		pr_info("%s: pca9481,switching frequency bypass is Empty\n", __func__);
		pdata->fsw_cfg_byp = PCA9481_FSW_CFG_BYP_DFT;
	}
	pr_info("%s: pca9481,fsw_cfg_byp is %d\n", __func__, pdata->fsw_cfg_byp);

	/* NTC0 threshold voltage */
	ret = of_property_read_u32(np_pca9481, "pca9481,ntc0-threshold",
							   &pdata->ntc0_th);
	if (ret) {
		pr_info("%s: pca9481,ntc0 threshold voltage is Empty\n", __func__);
		pdata->ntc0_th = PCA9481_NTC_0_TH_DFT;
	}
	pr_info("%s: pca9481,ntc0_th is %d\n", __func__, pdata->ntc0_th);

	/* NTC1 threshold voltage */
	ret = of_property_read_u32(np_pca9481, "pca9481,ntc1-threshold",
							   &pdata->ntc1_th);
	if (ret) {
		pr_info("%s: pca9481,ntc1 threshold voltage is Empty\n", __func__);
		pdata->ntc1_th = PCA9481_NTC_1_TH_DFT;
	}
	pr_info("%s: pca9481,ntc1_th is %d\n", __func__, pdata->ntc1_th);

	/* NTC protection */
	ret = of_property_read_u32(np_pca9481, "pca9481,ntc-protection",
							   &pdata->ntc_en);
	if (ret) {
		pr_info("%s: pca9481,ntc protection is Empty\n", __func__);
		pdata->ntc_en = 0;	/* Disable */
	}
	pr_info("%s: pca9481,ntc_en is %d\n", __func__, pdata->ntc_en);

	/* Charging mode */
	ret = of_property_read_u32(np_pca9481, "pca9481,chg-mode",
							   &pdata->chg_mode);
	if (ret) {
		pr_info("%s: pca9481,charging mode is Empty\n", __func__);
		pdata->chg_mode = CHG_2TO1_DC_MODE;
	}
	pr_info("%s: pca9481,chg_mode is %d\n", __func__, pdata->chg_mode);

	/* cv mode polling time in step1 charging */
	ret = of_property_read_u32(np_pca9481, "pca9481,cv-polling",
							   &pdata->cv_polling);
	if (ret) {
		pr_info("%s: pca9481,cv-polling is Empty\n", __func__);
		pdata->cv_polling = CVMODE_CHECK_T;
	}
	pr_info("%s: pca9481,cv polling is %d\n", __func__, pdata->cv_polling);

	/* vfloat threshold in step1 charging */
	ret = of_property_read_u32(np_pca9481, "pca9481,step1-vth",
							   &pdata->step1_vth);
	if (ret) {
		pr_info("%s: pca9481,step1-vfloat-threshold is Empty\n", __func__);
		pdata->step1_vth = STEP1_VFLOAT_THRESHOLD;
	}
	pr_info("%s: pca9481,step1_vth is %d\n", __func__, pdata->step1_vth);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("## %s: np(battery) NULL\n", __func__);
	} else {
		ret = of_property_read_string(np, "battery,charger_name",
				(char const **)&pdata->sec_dc_name);
		if (ret) {
			pr_err("## %s: direct_charger is Empty\n", __func__);
			pdata->sec_dc_name = "sec-direct-charger";
		}
		pr_info("%s: battery,charger_name is %s\n", __func__, pdata->sec_dc_name);

		/* charging float voltage */
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
								   &pdata->vfloat);
		pdata->vfloat *= PCA9481_SEC_DENOM_U_M;
		if (ret) {
			pr_info("%s: battery,dc_float_voltage is Empty\n", __func__);
			pdata->vfloat = PCA9481_VBAT_REG_DFT;
		}
		pr_info("%s: battery,v_float is %d\n", __func__, pdata->vfloat);
	}
#endif

	return 0;
}
#else
static int pca9481_charger_parse_dt(struct device *dev,
				struct pca9481_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef CONFIG_USBPD_PHY_QCOM
static int pca9481_usbpd_setup(struct pca9481_charger *pca9481)
{
	int ret = 0;
	const char *pd_phandle = "usbpd-phy";

	pca9481->pd = devm_usbpd_get_by_phandle(pca9481->dev, pd_phandle);

	if (IS_ERR(pca9481->pd)) {
		pr_err("get_usbpd phandle failed (%ld)\n",
				PTR_ERR(pca9481->pd));
		return PTR_ERR(pca9481->pd);
	}

	return ret;
}
#endif

static int read_reg(void *data, u64 *val)
{
	struct pca9481_charger *chip = data;
	int rc;
	unsigned int temp;

	rc = regmap_read(chip->regmap, chip->debug_address, &temp);
	if (rc) {
		pr_err("Couldn't read reg %x rc = %d\n",
			chip->debug_address, rc);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int write_reg(void *data, u64 val)
{
	struct pca9481_charger *chip = data;
	int rc;
	u8 temp;

	temp = (u8) val;
	rc = regmap_write(chip->regmap, chip->debug_address, temp);
	if (rc) {
		pr_err("Couldn't write 0x%02x to 0x%02x rc= %d\n",
			temp, chip->debug_address, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, read_reg, write_reg, "0x%02llx\n");

static int pca9481_create_debugfs_entries(struct pca9481_charger *chip)
{
	struct dentry *ent;
	int rc = 0;

	chip->debug_root = debugfs_create_dir("charger-pca9481", NULL);
	if (!chip->debug_root) {
		dev_err(chip->dev, "Couldn't create debug dir\n");
		rc = -ENOENT;
	} else {
		debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, &(chip->debug_address));

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &register_debug_ops);
		if (!ent) {
			dev_err(chip->dev,
				"Couldn't create data debug file\n");
			rc = -ENOENT;
		}
	}

	return rc;
}

static int pca9481_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct power_supply_config mains_cfg = {};
	struct pca9481_platform_data *pdata;
	struct device *dev = &client->dev;
	struct pca9481_charger *pca9481_chg;
	int ret;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	dev_info(&client->dev, "%s: PCA9481 Charger Driver Loading\n", __func__);
#endif
	dev_info(&client->dev, "%s: =========START=========\n", __func__);

	pca9481_chg = devm_kzalloc(dev, sizeof(*pca9481_chg), GFP_KERNEL);
	if (!pca9481_chg)
		return -ENOMEM;

#if defined(CONFIG_OF)
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(struct pca9481_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = pca9481_charger_parse_dt(&client->dev, pdata);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to get device of_node\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
	} else {
		pdata = client->dev.platform_data;
	}
#else
	pdata = dev->platform_data;
#endif
	if (!pdata)
		return -EINVAL;

	i2c_set_clientdata(client, pca9481_chg);

	mutex_init(&pca9481_chg->lock);
	mutex_init(&pca9481_chg->i2c_lock);
	pca9481_chg->dev = &client->dev;
	pca9481_chg->pdata = pdata;
	pca9481_chg->charging_state = DC_STATE_NO_CHARGING;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_chg->wdt_kick = false;
#endif

	/* Create a work queue for the direct charger */
	pca9481_chg->dc_wq = alloc_ordered_workqueue("pca9481_dc_wq", WQ_MEM_RECLAIM);
	if (pca9481_chg->dc_wq == NULL) {
		dev_err(pca9481_chg->dev, "failed to create work queue\n");
		return -ENOMEM;
	}

	pca9481_chg->monitor_wake_lock = wakeup_source_register(&client->dev, "pca9481-charger-monitor");
	if (!pca9481_chg->monitor_wake_lock) {
		dev_err(dev, "%s: Cannot register wakeup source\n", __func__);
		return -ENOMEM;
	}

	/* initialize work */
	INIT_DELAYED_WORK(&pca9481_chg->timer_work, pca9481_timer_work);
	mutex_lock(&pca9481_chg->lock);
	pca9481_chg->timer_id = TIMER_ID_NONE;
	pca9481_chg->timer_period = 0;
	mutex_unlock(&pca9481_chg->lock);

	INIT_DELAYED_WORK(&pca9481_chg->pps_work, pca9481_pps_request_work);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	INIT_DELAYED_WORK(&pca9481_chg->wdt_control_work, pca9481_wdt_control_work);
#endif

	pca9481_chg->regmap = devm_regmap_init_i2c(client, &pca9481_regmap);
	if (IS_ERR(pca9481_chg->regmap)) {
		ret = -EINVAL;
		goto error;
	}

#ifdef CONFIG_USBPD_PHY_QCOM
	if (pca9481_usbpd_setup(pca9481_chg)) {
		dev_err(dev, "Error usbpd setup!\n");
		pca9481_chg->pd = NULL;
	}
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9481_init_adc_val(pca9481_chg, -1);
#endif

	ret = pca9481_hw_init(pca9481_chg);
	if (ret < 0)
		goto error;

	mains_cfg.supplied_to = pca9481_supplied_to;
	mains_cfg.num_supplicants = ARRAY_SIZE(pca9481_supplied_to);
	mains_cfg.drv_data = pca9481_chg;
	pca9481_chg->mains = power_supply_register(dev, &pca9481_mains_desc,
					   &mains_cfg);
	if (IS_ERR(pca9481_chg->mains)) {
		ret = -ENODEV;
		goto error;
	}

	/*
	 * Interrupt pin is optional. If it is connected, we setup the
	 * interrupt support here.
	 */
	if (pdata->irq_gpio >= 0) {
		ret = pca9481_irq_init(pca9481_chg, client);
		if (ret < 0) {
			dev_warn(dev, "failed to initialize IRQ: %d\n", ret);
			dev_warn(dev, "disabling IRQ support\n");
		}
		/* disable interrupt */
		disable_irq(client->irq);
	}

	ret = pca9481_create_debugfs_entries(pca9481_chg);
	if (ret < 0)
		goto error;

	sec_chg_set_dev_init(SC_DEV_DIR_CHG);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	dev_info(&client->dev, "%s: PCA9481 Charger Driver Loaded\n", __func__);
#endif
	dev_info(&client->dev, "%s: =========END=========\n", __func__);

	return 0;

error:
	destroy_workqueue(pca9481_chg->dc_wq);
	mutex_destroy(&pca9481_chg->lock);
	wakeup_source_unregister(pca9481_chg->monitor_wake_lock);
	return ret;
}

static int pca9481_remove(struct i2c_client *client)
{
	struct pca9481_charger *pca9481_chg = i2c_get_clientdata(client);

	/* stop charging if it is active */
	pca9481_stop_charging(pca9481_chg);

	if (client->irq) {
		free_irq(client->irq, pca9481_chg);
		gpio_free(pca9481_chg->pdata->irq_gpio);
	}

	/* Delete the work queue */
	destroy_workqueue(pca9481_chg->dc_wq);

	wakeup_source_unregister(pca9481_chg->monitor_wake_lock);
	if (pca9481_chg->mains)
		power_supply_put(pca9481_chg->mains);
	power_supply_unregister(pca9481_chg->mains);

	debugfs_remove(pca9481_chg->debug_root);
	return 0;
}

static void pca9481_shutdown(struct i2c_client *client)
{
	struct pca9481_charger *pca9481_chg = i2c_get_clientdata(client);

	pr_info("%s: ++\n", __func__);

	if (pca9481_set_charging(pca9481_chg, false) < 0)
		pr_info("%s: failed to disable charging\n", __func__);

	if (client->irq)
		free_irq(client->irq, pca9481_chg);

	cancel_delayed_work(&pca9481_chg->timer_work);
	cancel_delayed_work(&pca9481_chg->pps_work);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	cancel_delayed_work(&pca9481_chg->wdt_control_work);
#endif

	pr_info("%s: --\n", __func__);
}

static const struct i2c_device_id pca9481_id[] = {
	{ "pca9481-charger", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pca9481_id);

#if defined(CONFIG_OF)
static const struct of_device_id pca9481_i2c_dt_ids[] = {
	{ .compatible = "nxp,pca9481" },
	{ },
};
MODULE_DEVICE_TABLE(of, pca9481_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
#ifdef CONFIG_RTC_HCTOSYS
static void pca9481_check_and_update_charging_timer(struct pca9481_charger *pca9481)
{
	unsigned long current_time = 0, next_update_time, time_left;

	get_current_time(&current_time);

	if (pca9481->timer_id != TIMER_ID_NONE)	{
		next_update_time = pca9481->last_update_time + (pca9481->timer_period / 1000);	// unit is second

		pr_info("%s: current_time=%ld, next_update_time=%ld\n", __func__, current_time, next_update_time);

		if (next_update_time > current_time)
			time_left = next_update_time - current_time;
		else
			time_left = 0;

		mutex_lock(&pca9481->lock);
		pca9481->timer_period = time_left * 1000;	// ms unit
		mutex_unlock(&pca9481->lock);
		schedule_delayed_work(&pca9481->timer_work, msecs_to_jiffies(pca9481->timer_period));
		pr_info("%s: timer_id=%d, time_period=%ld\n", __func__, pca9481->timer_id, pca9481->timer_period);
	}
	pca9481->last_update_time = current_time;
}
#endif

static int pca9481_suspend(struct device *dev)
{
	struct pca9481_charger *pca9481 = dev_get_drvdata(dev);

	if (pca9481->timer_id != TIMER_ID_NONE) {
		pr_debug("%s: cancel delayed work\n", __func__);

		/* cancel delayed_work */
		cancel_delayed_work(&pca9481->timer_work);
	}
	return 0;
}

static int pca9481_resume(struct device *dev)
{
	struct pca9481_charger *pca9481 = dev_get_drvdata(dev);

#ifdef CONFIG_RTC_HCTOSYS
	pca9481_check_and_update_charging_timer(pca9481);
#else
	if (pca9481->timer_id != TIMER_ID_NONE) {
		pr_debug("%s: update_timer\n", __func__);

		/* Update the current timer */
		mutex_lock(&pca9481->lock);
		pca9481->timer_period = 0;	// ms unit
		mutex_unlock(&pca9481->lock);
		queue_delayed_work(pca9481->dc_wq,
							&pca9481->timer_work,
							msecs_to_jiffies(pca9481->timer_period));
	}
#endif
	return 0;
}
#else
#define pca9481_suspend		NULL
#define pca9481_resume		NULL
#endif

const struct dev_pm_ops pca9481_pm_ops = {
	.suspend = pca9481_suspend,
	.resume = pca9481_resume,
};

static struct i2c_driver pca9481_driver = {
	.driver = {
		.name = "pca9481-charger",
#if defined(CONFIG_OF)
		.of_match_table = pca9481_i2c_dt_ids,
#endif /* CONFIG_OF */
#if defined(CONFIG_PM)
		.pm = &pca9481_pm_ops,
#endif
	},
	.probe        = pca9481_probe,
	.remove       = pca9481_remove,
	.shutdown     = pca9481_shutdown,
	.id_table     = pca9481_id,
};

module_i2c_driver(pca9481_driver);

MODULE_DESCRIPTION("PCA9481 charger driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.9S");
