/*
 * Driver for the NXP PCA9468 battery charger.
 *
 * Copyright 2018-2023 NXP
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
#include "pca9468_charger.h"
#include "../../common/sec_charging_common.h"
#include "../../common/sec_direct_charger.h"
#include <linux/battery/sec_pd.h>
#else
#include <linux/power/pca9468_charger.h>
#endif

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include <linux/pm_wakeup.h>
#ifdef CONFIG_USBPD_PHY_QCOM
#include <linux/usb/usbpd.h>		// Use Qualcomm USBPD PHY
#endif

#define DRV_MODULE_VERSION	"3.4.24S"

#ifdef CONFIG_USBPD_PHY_QCOM
static int pca9468_usbpd_setup(struct pca9468_charger *pca9468);
#endif
/* Check Active status */
static int pca9468_check_error(struct pca9468_charger *pca9468);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static int pca9468_send_pd_message(struct pca9468_charger *pca9468, unsigned int msg_type);
static int get_system_current(struct pca9468_charger *pca9468);

/**************************************/
/* Switching charger control function */
/**************************************/
char *charging_state_str[] = {
	"NO_CHARGING", "CHECK_VBAT", "PRESET_DC", "CHECK_ACTIVE", "ADJUST_CC",
	"START_CC", "CC_MODE", "START_CV", "CV_MODE", "CHARGING_DONE",
	"ADJUST_TAVOL", "ADJUST_TACUR", "WC_CV_MODE", "BYPASS_MODE", "DCMODE_CHANGE", "FPDO_CV_MODE",
};

static int pca9468_read_reg(struct pca9468_charger *pca9468, unsigned reg, void *val)
{
	int ret = 0;

	mutex_lock(&pca9468->i2c_lock);
	ret = regmap_read(pca9468->regmap, reg, val);
	mutex_unlock(&pca9468->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9468_bulk_read_reg(struct pca9468_charger *pca9468, int reg, void *val, int count)
{
	int ret = 0;

	mutex_lock(&pca9468->i2c_lock);
	ret = regmap_bulk_read(pca9468->regmap, reg, val, count);
	mutex_unlock(&pca9468->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9468_write_reg(struct pca9468_charger *pca9468, int reg, u8 val)
{
	int ret = 0;

	mutex_lock(&pca9468->i2c_lock);
	ret = regmap_write(pca9468->regmap, reg, val);
	mutex_unlock(&pca9468->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9468_update_reg(struct pca9468_charger *pca9468, int reg, u8 mask, u8 val)
{
	int ret = 0;

	mutex_lock(&pca9468->i2c_lock);
	ret = regmap_update_bits(pca9468->regmap, reg, mask, val);
	if (reg == PCA9468_REG_START_CTRL && mask == PCA9468_BIT_STANDBY_EN) {
		if (val == PCA9468_STANDBY_DONOT) {
			pr_info("%s: PCA9468_STANDBY_DONOT 50ms\n", __func__);
			/* Wait 50ms, first to keep the start-up sequence */
			msleep(50);
		} else {
			pr_info("%s: PCA9468_STANDBY_FORCED 5ms\n", __func__);
			/* Wait 5ms to keep the shutdown sequence */
			usleep_range(5000, 6000);
		}
	}
	mutex_unlock(&pca9468->i2c_lock);
	if (ret < 0)
		pr_info("%s: reg(0x%x), ret(%d)\n", __func__, reg, ret);
	return ret;
}

static int pca9468_read_adc(struct pca9468_charger *pca9468, u8 adc_ch);

static int pca9468_set_charging_state(struct pca9468_charger *pca9468, unsigned int charging_state) {
	union power_supply_propval value = {0,};
	static int prev_val = DC_STATE_NO_CHARGING;

	pca9468->charging_state = charging_state;

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
		break;
	}

	if (prev_val == value.intval)
		return -1;

	prev_val = value.intval;
	psy_do_property(pca9468->pdata->sec_dc_name, set,
				POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, value);

	return 0;
}

static void pca9468_init_adc_val(struct pca9468_charger *pca9468, int val)
{
	int i = 0;

	for (i = 0; i < ADCCH_MAX; ++i)
		pca9468->adc_val[i] = val;
}

static void pca9468_test_read(struct pca9468_charger *pca9468)
{
	int address = 0;
	unsigned int val;
	char str[1024] = {0, };

	for (address = PCA9468_REG_INT1_STS; address <= PCA9468_REG_STS_ADC_9; address++) {
		pca9468_read_reg(pca9468, address, &val);
		sprintf(str + strlen(str), "[0x%02x]0x%02x, ", address, val);
	}

	for (address = PCA9468_REG_ICHG_CTRL; address <= PCA9468_REG_NTC_TH_2; address++) {
		pca9468_read_reg(pca9468, address, &val);
		sprintf(str + strlen(str), "[0x%02x]0x%02x, ", address, val);
	}

	if (pca9468->pdata->chgen_gpio >= 0)
		pr_info("## pca9468 : [DC_CPEN:%d]%s\n", gpio_get_value(pca9468->pdata->chgen_gpio), str);
}

static void pca9468_monitor_work(struct pca9468_charger *pca9468)
{
	int ta_vol = pca9468->ta_vol / PCA9468_SEC_DENOM_U_M;
	int ta_cur = pca9468->ta_cur / PCA9468_SEC_DENOM_U_M;
	if (pca9468->charging_state == DC_STATE_NO_CHARGING)
		return;

	/* update adc value */
	pca9468_read_adc(pca9468, ADCCH_VIN);
	pca9468_read_adc(pca9468, ADCCH_IIN);
	pca9468_read_adc(pca9468, ADCCH_VBAT);
	pca9468_read_adc(pca9468, ADCCH_DIETEMP);

	pr_info("%s: state(%s), iin_cc(%dmA), v_float(%dmV), vbat(%dmV), vin(%dmV), iin(%dmA), die_temp(%d), isys(%dmA), pps_requested(%d/%dmV/%dmA)", __func__,
		charging_state_str[pca9468->charging_state],
		pca9468->iin_cc / PCA9468_SEC_DENOM_U_M, pca9468->vfloat / PCA9468_SEC_DENOM_U_M,
		pca9468->adc_val[ADCCH_VBAT], pca9468->adc_val[ADCCH_VIN],
		pca9468->adc_val[ADCCH_IIN], pca9468->adc_val[ADCCH_DIETEMP],
		get_system_current(pca9468), pca9468->ta_objpos,
		ta_vol, ta_cur);
}

/**************************************/
/* Switching charger control function */
/**************************************/
/* This function needs some modification by a customer */
static void pca9468_set_wdt_enable(struct pca9468_charger *pca9468, bool enable)
{
	int ret;
	unsigned int val;

	val = enable << MASK2SHIFT(PCA9468_BIT_WATCHDOG_EN);
	ret = pca9468_update_reg(pca9468, PCA9468_REG_SAFETY_CTRL,
			PCA9468_BIT_WATCHDOG_EN, val);
	pr_info("%s: set wdt enable = %d\n", __func__, enable);
}

static void pca9468_set_wdt_timer(struct pca9468_charger *pca9468, int time)
{
	int ret;
	unsigned int val;

	val = time << MASK2SHIFT(PCA9468_BIT_WATCHDOG_CFG);
	ret = pca9468_update_reg(pca9468, PCA9468_REG_SAFETY_CTRL,
			PCA9468_BIT_WATCHDOG_CFG, val);
	pr_info("%s: set wdt time = %d\n", __func__, time);
}

static void pca9468_check_wdt_control(struct pca9468_charger *pca9468)
{
	struct device *dev = pca9468->dev;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	if (pca9468->wdt_kick) {
		pca9468_set_wdt_timer(pca9468, WDT_8SEC);
		schedule_delayed_work(&pca9468->wdt_control_work, msecs_to_jiffies(PCA9468_BATT_WDT_CONTROL_T));
	} else {
		pca9468_set_wdt_timer(pca9468, WDT_8SEC);
		if (client->addr == 0xff)
			client->addr = 0x57;
	}
}

static void pca9468_wdt_control_work(struct work_struct *work)
{
	struct pca9468_charger *pca9468 = container_of(work, struct pca9468_charger,
						 wdt_control_work.work);
	struct device *dev = pca9468->dev;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	int vin, iin;

	pca9468_set_wdt_timer(pca9468, WDT_4SEC);

	/* this is for kick watchdog */
	vin = pca9468_read_adc(pca9468, ADCCH_VIN);
	iin = pca9468_read_adc(pca9468, ADCCH_IIN);

	pca9468_send_pd_message(pca9468, PD_MSG_REQUEST_APDO);

	client->addr = 0xff;

	pr_info("## %s: disable client addr (vin:%dmV, iin:%dmA)\n",
		__func__, vin / PCA9468_SEC_DENOM_U_M, iin / PCA9468_SEC_DENOM_U_M);
}

static void pca9468_set_done(struct pca9468_charger *pca9468, bool enable)
{
	int ret = 0;
	union power_supply_propval value = {0, };

	value.intval = enable;
	psy_do_property(pca9468->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_DIRECT_DONE, value);

	if (ret < 0)
		pr_info("%s: error set_done, ret=%d\n", __func__, ret);
}

static void pca9468_set_switching_charger(struct pca9468_charger *pca9468, bool enable)
{
	int ret = 0;
	union power_supply_propval value = {0, };

	value.intval = enable;
	psy_do_property(pca9468->pdata->sec_dc_name, set,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, value);

	if (ret < 0)
		pr_info("%s: error switching_charger, ret=%d\n", __func__, ret);
}
#else
static int pca9468_set_switching_charger( bool enable,
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
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, &val);
		if (ret < 0)
			goto error;
	} else {
		/* disable charger */
		/* it depends on customer's code to disable charger */
		val.intval = enable;
		ret = power_supply_set_property(psy_swcharger, POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, &val);
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

static int pca9468_get_switching_charger_property(enum power_supply_property prop,
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
static int pca9468_send_pd_message(struct pca9468_charger *pca9468, unsigned int msg_type)
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

	/* Cancel pps request timer */
	cancel_delayed_work(&pca9468->pps_work);

	mutex_lock(&pca9468->lock);

	if (((pca9468->charging_state == DC_STATE_NO_CHARGING) &&
		(msg_type == PD_MSG_REQUEST_APDO)) ||
		(pca9468->mains_online == false)) {
		/* Vbus reset happened in the previous PD communication */
		goto out;
	}

#ifdef CONFIG_USBPD_PHY_QCOM
	/* check the phandle */
	if (pca9468->pd == NULL) {
		pr_info("%s: get phandle\n", __func__);
		ret = pca9468_usbpd_setup(pca9468);
		if (ret != 0) {
			dev_err(pca9468->dev, "Error usbpd setup!\n");
			pca9468->pd = NULL;
			goto out;
		}
	}
#endif

	/* Check whether requested TA voltage and current are in valid range or not */
	if ((msg_type == PD_MSG_REQUEST_APDO) &&
		((pca9468->ta_vol < PCA9468_TA_MIN_VOL) || (pca9468->ta_cur < PCA9468_TA_MIN_CUR)))	{
		/* request TA voltage or current is less than minimum threshold */
		/* This is abnormal case, too low input voltage and current */
		/* Normally VIN_UVLO already happened */
		pr_err("%s: Abnormal low RDO, ta_vol=%d, ta_cur=%d\n", __func__, pca9468->ta_vol, pca9468->ta_cur);
		ret = -EINVAL;
		goto out;
	}

	pr_info("%s: msg_type=%d, ta_cur=%d, ta_vol=%d, ta_objpos=%d\n",
		__func__, msg_type, pca9468->ta_cur, pca9468->ta_vol, pca9468->ta_objpos);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pdo_idx = pca9468->ta_objpos;
	pps_vol = pca9468->ta_vol / PCA9468_SEC_DENOM_U_M;
	pps_cur = pca9468->ta_cur / PCA9468_SEC_DENOM_U_M;
	pr_info("## %s: msg_type=%d, pdo_idx=%d, pps_vol=%dmV(max_vol=%dmV), pps_cur=%dmA(max_cur=%dmA)\n",
		__func__, msg_type, pdo_idx,
		pps_vol, pca9468->pdo_max_voltage,
		pps_cur, pca9468->pdo_max_current);
#endif

	switch (msg_type) {
	case PD_MSG_REQUEST_APDO:
#ifdef CONFIG_USBPD_PHY_QCOM
		ret = usbpd_request_pdo(pca9468->pd, pca9468->ta_objpos, pca9468->ta_vol, pca9468->ta_cur);
		if (ret == -EBUSY) {
			/* wait 100ms */
			msleep(100);
			/* try again */
			ret = usbpd_request_pdo(pca9468->pd, pca9468->ta_objpos, pca9468->ta_vol, pca9468->ta_cur);
		}
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
		if (ret == -EBUSY) {
			pr_info("%s: request again ret=%d\n", __func__, ret);
			msleep(100);
			ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
		}
#else
		op_cur = pca9468->ta_cur/50000; 	// Operating Current 50mA units
		out_vol = pca9468->ta_vol/20000;	// Output Voltage in 20mV units
		msg_buf[0] = op_cur & 0x7F;			// Operating Current 50mA units - B6...0
		msg_buf[1] = (out_vol<<1) & 0xFE;	// Output Voltage in 20mV units - B19..(B15)..B9
		msg_buf[2] = (out_vol>>7) & 0x0F;	// Output Voltage in 20mV units - B19..(B16)..B9,
		msg_buf[3] = pca9468->ta_objpos<<4; // Object Position - B30...B28

		/* Send the PD message to CC/PD chip */
		/* Todo - insert code */
#endif
		/* Start pps request timer */
		if (ret == 0) {
			schedule_delayed_work(&pca9468->pps_work, msecs_to_jiffies(PCA9468_PPS_PERIODIC_T));
		}
		break;

	case PD_MSG_REQUEST_FIXED_PDO:
#ifdef CONFIG_USBPD_PHY_QCOM
		ret = usbpd_request_pdo(pca9468->pd, pca9468->ta_objpos, pca9468->ta_vol, pca9468->ta_cur);
		if (ret == -EBUSY) {
			/* wait 100ms */
			msleep(100);
			/* try again */
			ret = usbpd_request_pdo(pca9468->pd, pca9468->ta_objpos, pca9468->ta_vol, pca9468->ta_cur);
		}
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		if (pca9468->ta_type == TA_TYPE_USBPD_20) {
			pr_err("%s: ta_type(%d)! skip pd_select_pps\n", __func__, pca9468->ta_type);
		} else {
			ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
			if (ret == -EBUSY) {
				pr_info("%s: request again ret=%d\n", __func__, ret);
				msleep(100);
				ret = sec_pd_select_pps(pdo_idx, pps_vol, pps_cur);
			}
		}
#else
		max_cur = pca9468->ta_cur/10000;	// Maximum Operation Current 10mA units
		op_cur = max_cur;					// Operating Current 10mA units
		msg_buf[0] = max_cur & 0xFF;		// Maximum Operation Current -B9..(7)..0
		msg_buf[1] = ((max_cur>>8) & 0x03) | ((op_cur<<2) & 0xFC);	// Operating Current - B19..(15)..10
		msg_buf[2] = ((op_cur>>6) & 0x0F);	// Operating Current - B19..(16)..10, Unchunked Extended Messages Supported - not support
		msg_buf[3] = pca9468->ta_objpos<<4; // Object Position - B30...B28

		/* Send the PD message to CC/PD chip */
		/* Todo - insert code */
#endif
		break;

	default:
		break;
	}

out:
	if (((pca9468->charging_state == DC_STATE_NO_CHARGING) &&
		(msg_type == PD_MSG_REQUEST_APDO)) ||
		(pca9468->mains_online == false)) {
		/* Even though PD communication success, Vbus reset might happen
			So, check the charging state again */
		ret = -EINVAL;
	}

	pr_info("%s: ret=%d\n", __func__, ret);
	mutex_unlock(&pca9468->lock);
	return ret;
}

/************************/
/* Get APDO max power */
/************************/
/* Get the max current/voltage/power of APDO from the CC/PD driver */
/* This function needs some modification by a customer */
static int pca9468_get_apdo_max_power(struct pca9468_charger *pca9468)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	unsigned int ta_max_vol_mv = (pca9468->ta_max_vol/PCA9468_SEC_DENOM_U_M);
	unsigned int ta_max_cur_ma = 0;
	unsigned int ta_max_pwr_uw = 0;
#endif

#ifdef CONFIG_USBPD_PHY_QCOM
	/* check the phandle */
	if (pca9468->pd == NULL) {
		pr_info("%s: get phandle\n", __func__);
		ret = pca9468_usbpd_setup(pca9468);
		if (ret != 0) {
			dev_err(pca9468->dev, "Error usbpd setup!\n");
			pca9468->pd = NULL;
			goto out;
		}
	}

	ret = usbpd_get_apdo_max_power(pca9468->pd, &pca9468->ta_objpos,
			&pca9468->ta_max_vol, &pca9468->ta_max_cur, &pca9468->ta_max_pwr);
#elif IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ret = sec_pd_get_apdo_max_power(&pca9468->ta_objpos,
				&ta_max_vol_mv, &ta_max_cur_ma, &ta_max_pwr_uw);
	/* mA,mV,uW --> uA,uV,uW */
	pca9468->ta_max_vol = ta_max_vol_mv * PCA9468_SEC_DENOM_U_M;
	pca9468->ta_max_cur = ta_max_cur_ma * PCA9468_SEC_DENOM_U_M;
	pca9468->ta_max_pwr = ta_max_pwr_uw;

	pr_info("%s: ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d\n",
		__func__, pca9468->ta_max_vol, pca9468->ta_max_cur, pca9468->ta_max_pwr);

	pca9468->pdo_index = pca9468->ta_objpos;
	pca9468->pdo_max_voltage = ta_max_vol_mv;
	pca9468->pdo_max_current = ta_max_cur_ma;
#else
	/* Put ta_max_vol to the desired ta maximum value, ex) 9800mV */
	/* Get new ta_max_vol and ta_max_cur, ta_max_power and proper object position by CC/PD IC */
	/* insert code */
#endif

#ifdef CONFIG_USBPD_PHY_QCOM
out:
#endif
	return ret;
}


/* ADC Read function */
static int pca9468_read_adc(struct pca9468_charger *pca9468, u8 adc_ch)
{
	u8 reg_data[2];
	u16 raw_adc;	// raw ADC value
	int conv_adc;	// conversion ADC value
	int ret;
	union power_supply_propval pval;

	switch (adc_ch) {
	case ADCCH_VOUT:
		// Read ADC value
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_4, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9468_BIT_ADC_VOUT9_2)<<2) | ((reg_data[0] & PCA9468_BIT_ADC_VOUT1_0)>>6);
		conv_adc = raw_adc * VOUT_STEP;	// unit - uV
		break;

	case ADCCH_VIN:
		// Read ADC value
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_3, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9468_BIT_ADC_VIN9_4)<<4) | ((reg_data[0] & PCA9468_BIT_ADC_VIN3_0)>>4);
		conv_adc = raw_adc * VIN_STEP;	// unit - uV
		break;

	case ADCCH_VBAT:
		// Read ADC value
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_6, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9468_BIT_ADC_VBAT9_8)<<8) | ((reg_data[0] & PCA9468_BIT_ADC_VBAT7_0)>>0);
		conv_adc = raw_adc * VBAT_STEP;		// unit - uV
		pr_info("%s: pca9468 vbatt_adc, convert_val=%d\n", __func__, conv_adc);

		/* Check VFLOAT method */
		if (pca9468->pdata->fg_vfloat == true) {
			/* Use FG Vnow for VFLOAT */
			// Read fuelgauge ADC
			ret = psy_do_property(pca9468->pdata->fg_name, get,
						POWER_SUPPLY_PROP_VOLTAGE_NOW, pval);
			if (ret < 0) {
				conv_adc = ret;
				goto error;
			}
			conv_adc = pval.intval * DENOM_U_M;
			pr_info("%s: pca9468 fg_vnow, convert_val=%d\n", __func__, conv_adc);
		}
		break;

	case ADCCH_ICHG:
		// Read ADC value
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_2, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9468_BIT_ADC_IOUT9_6)<<6) | ((reg_data[0] & PCA9468_BIT_ADC_IOUT5_0)>>2);
		conv_adc = raw_adc * ICHG_STEP;	// unit - uA
		break;

	case ADCCH_IIN:
		// Read ADC value
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_1, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC - iin = rawadc*4.89 + (rawadc*4.89 - 900)*adc_comp_gain/100, unit - uA
		raw_adc = ((reg_data[1] & PCA9468_BIT_ADC_IIN9_8)<<8) | ((reg_data[0] & PCA9468_BIT_ADC_IIN7_0)>>0);
		conv_adc = raw_adc * IIN_STEP + (raw_adc * IIN_STEP - ADC_IIN_OFFSET)*pca9468->adc_comp_gain/100;		// unit - uA
		if (conv_adc < 0)
			conv_adc = 0;	// If ADC raw value is 0, convert value will be minus value because of compensation gain, so in this case conv_adc is 0
		break;

	case ADCCH_DIETEMP:
		// Read ADC value
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_7, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9468_BIT_ADC_DIETEMP9_6)<<6) | ((reg_data[0] & PCA9468_BIT_ADC_DIETEMP5_0)>>2);
		conv_adc = (935 - raw_adc)*DIETEMP_STEP/DIETEMP_DENOM; 	// Temp = (935-rawadc)*0.435, unit - C
		if (conv_adc > DIETEMP_MAX) {
			conv_adc = DIETEMP_MAX;
		} else if (conv_adc < DIETEMP_MIN) {
			conv_adc = DIETEMP_MIN;
		}
		break;

	case ADCCH_NTC:
		// Read ADC value
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_8, reg_data, 2);
		if (ret < 0) {
			conv_adc = ret;
			goto error;
		}
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9468_BIT_ADC_NTCV9_4)<<4) | ((reg_data[0] & PCA9468_BIT_ADC_NTCV3_0)>>4);
		conv_adc = raw_adc * NTCV_STEP;		// unit - uV
		break;

	default:
		conv_adc = -EINVAL;
		break;
	}

error:
	pr_info("%s: adc_ch=%d, convert_val=%d\n", __func__, adc_ch, conv_adc);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (adc_ch == ADCCH_DIETEMP)
		pca9468->adc_val[adc_ch] = conv_adc;
	else
		pca9468->adc_val[adc_ch] = conv_adc / PCA9468_SEC_DENOM_U_M;
#endif
	return conv_adc;
}

/* PCA9468 VBAT_ADC Read function */
static int pca9468_read_vbat_adc(struct pca9468_charger *pca9468)
{
	u8 reg_data[2];
	u16 raw_adc;	// raw ADC value
	int conv_adc;	// conversion ADC value
	int ret;

	// Read ADC value
	ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_6, reg_data, 2);
	if (ret < 0) {
		pr_info("%s: Error - ret=%d\n", __func__, ret);
		conv_adc = ret;
	} else {
		// Convert ADC
		raw_adc = ((reg_data[1] & PCA9468_BIT_ADC_VBAT9_8)<<8) | ((reg_data[0] & PCA9468_BIT_ADC_VBAT7_0)>>0);
		conv_adc = raw_adc * VBAT_STEP;		// unit - uV
		pr_info("%s: pca9468 vbatt_adc, raw_adc=0x%x, convert_val=%d\n", __func__, raw_adc, conv_adc);
	}

	return conv_adc;
}

static int pca9468_set_vfloat(struct pca9468_charger *pca9468, unsigned int v_float)
{
	int ret, val;

	pr_info("%s: vfloat=%d\n", __func__, v_float);

	/* v float voltage */

	/* maximum value is 5V */
	if (v_float > PCA9468_VFLOAT_MAX)
		v_float = PCA9468_VFLOAT_MAX;
	/* minimum value is 3.725V */
	if (v_float < PCA9468_VFLOAT_MIN)
		v_float = PCA9468_VFLOAT_MIN;

	val = PCA9468_V_FLOAT(v_float);
	ret = pca9468_write_reg(pca9468, PCA9468_REG_V_FLOAT, val);
	return ret;
}

static int pca9468_set_charging_current(struct pca9468_charger *pca9468, unsigned int ichg)
{
	int ret, val;

	pr_info("%s: ichg=%d\n", __func__, ichg);

	/* charging current */
	if (ichg > PCA9468_ICHG_CFG_MAX) {
		ichg = PCA9468_ICHG_CFG_MAX;
		pr_info("%s: -> ichg=%d\n", __func__, ichg);
	}
	val = PCA9468_ICHG_CFG(ichg);
	ret = pca9468_update_reg(pca9468, PCA9468_REG_ICHG_CTRL, PCA9468_BIT_ICHG_CFG, val);
	return ret;
}

static int pca9468_set_input_current(struct pca9468_charger *pca9468, unsigned int iin)
{
	int ret, val;

	pr_info("%s: iin=%d\n", __func__, iin);

	/* input current */

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (pca9468->ta_type == TA_TYPE_USBPD_20) {
		if (iin < pca9468->pdata->fpdo_dc_iin_lowest_limit) {
			pr_info("%s: IIN LOWEST LIMIT! IIN %d -> %d\n", __func__,
					iin, pca9468->pdata->fpdo_dc_iin_lowest_limit);
			iin = pca9468->pdata->fpdo_dc_iin_lowest_limit;
		}
	}
#endif


	/* round off and increase one step */
	iin = iin + PD_MSG_TA_CUR_STEP;
	/* maximum value is 5A */
	if (iin > PCA9468_IIN_CFG_MAX)
		iin = PCA9468_IIN_CFG_MAX;
	val = PCA9468_IIN_CFG(iin);
	/* Set IIN_CFG to one step higher */
	val = val + 1;

	if (val > 0x32)
		val = 0x32;	/* maximum value is 5A */

	ret = pca9468_update_reg(pca9468, PCA9468_REG_IIN_CTRL, PCA9468_BIT_IIN_CFG, val);
	pr_info("%s: real iin_cfg=%d\n", __func__, val*PCA9468_IIN_CFG_STEP);
	return ret;
}

static int pca9468_softreset(struct pca9468_charger *pca9468)
{
	int ret, val;
	u8 reg_val[10]; /* Dump for control register */

	pr_info("%s: do soft reset\n", __func__);

	/* Check revision information */
	if (pca9468->revision == REV_B4) {
		// Check the current register before softreset */
		pr_info("%s: Before softreset\n", __func__);
		/* Read all control registers for debugging */
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_INT1, &reg_val[PCA9468_REG_INT1], 7);
		if (ret < 0)
			goto error;
		pr_info("%s: status reg[1]=0x%x,[2]=0x%x,[3]=0x%x,[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",
			__func__, reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6], reg_val[7]);

		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_IIN_CTRL, reg_val, 10);
		if (ret < 0)
			goto error;

		pr_info("%s: control reg[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x,[0x24]=0x%x,[0x25]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4]);
		pr_info("%s: control reg[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x,[0x2A]=0x%x\n",
			__func__, reg_val[5], reg_val[6], reg_val[7], reg_val[8], reg_val[9]);

		/* Do softreset */

		/* Set softreset register */

		/* [0x30] = 0x5B */
		val = 0x5B;
		ret = pca9468_write_reg(pca9468, PCA9468_REG_ADC_ACCESS, val);
		if (ret < 0)
			goto error;
		/* [0x4F] = 0x11 */
		val = 0x11;
		ret = pca9468_write_reg(pca9468, 0x4F, val);
		if (ret < 0)
			goto error;
		/* [0x4B] = 0x40 */
		val = 0x40;
		ret = pca9468_write_reg(pca9468, 0x4B, val);
		if (ret < 0)
			goto error;

		/* Wait 5ms */
		usleep_range(5000, 6000);

		/* Reset PCA9468 and all registers values go to POR values */
		// Check the current register after softreset */
		/* Read all control registers for debugging */
		pr_info("%s: After softreset\n", __func__);

		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_INT1, &reg_val[PCA9468_REG_INT1], 7);
		if (ret < 0)
			goto error;
		pr_info("%s: status reg[1]=0x%x,[2]=0x%x,[3]=0x%x,[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",
			__func__, reg_val[1], reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6], reg_val[7]);

		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_IIN_CTRL, reg_val, 10);
		if (ret < 0)
			goto error;

		pr_info("%s: control reg[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x,[0x24]=0x%x,[0x25]=0x%x\n",
			__func__, reg_val[0], reg_val[1], reg_val[2], reg_val[3], reg_val[4]);
		pr_info("%s: control reg[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x,[0x2A]=0x%x\n",
			__func__, reg_val[5], reg_val[6], reg_val[7], reg_val[8], reg_val[9]);

		/* Set the initial register value */
		/* Set OV_DELTA to 30% */
#ifdef CONFIG_PCA9468_FACTORY_MODE
		val = OV_DELTA_40P << MASK2SHIFT(PCA9468_BIT_OV_DELTA);
#else
		val = OV_DELTA_30P << MASK2SHIFT(PCA9468_BIT_OV_DELTA);
#endif
		ret = pca9468_update_reg(pca9468, PCA9468_REG_SAFETY_CTRL,
								PCA9468_BIT_OV_DELTA, val);
		if (ret < 0)
			goto error;

		/* Set Reverse Current Detection and standby mode*/
		val = PCA9468_BIT_REV_IIN_DET | PCA9468_EN_ACTIVE_L | PCA9468_STANDBY_FORCED;
		ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
								(PCA9468_BIT_REV_IIN_DET | PCA9468_BIT_EN_CFG | PCA9468_BIT_STANDBY_EN),
								val);
		if (ret < 0)
			goto error;

		/* clear LIMIT_INCREMENT_EN */
		val = 0;
		ret = pca9468_update_reg(pca9468, PCA9468_REG_IIN_CTRL,
								PCA9468_BIT_LIMIT_INCREMENT_EN, val);
		if (ret < 0)
			goto error;

		/* Set the ADC channel */
		val = (PCA9468_BIT_CH7_EN | /* NTC voltage ADC */
			PCA9468_BIT_CH6_EN | /* DIETEMP ADC */
			PCA9468_BIT_CH5_EN | /* IIN ADC */
			PCA9468_BIT_CH3_EN | /* VBAT ADC */
			PCA9468_BIT_CH2_EN | /* VIN ADC */
			PCA9468_BIT_CH1_EN); /* VOUT ADC */

		ret = pca9468_write_reg(pca9468, PCA9468_REG_ADC_CFG, val);
		if (ret < 0)
			goto error;

	} else {
		/* It is not B4 version */
		/* Cannot support soft reset */
		pr_info("%s: skip, cannot support softreset, revision=%d\n", __func__, pca9468->revision);
	}

	return 0;

error:
	pr_info("%s: i2c error, ret=%d\n", __func__, ret);
	return ret;
}

static int pca9468_set_charging(struct pca9468_charger *pca9468, bool enable)
{
	int ret, val;

	pr_info("%s: enable=%d\n", __func__, enable);

	if (enable == true) {
		/* Improve adc */
		val = 0x5B;
		ret = pca9468_write_reg(pca9468, PCA9468_REG_ADC_ACCESS, val);
		if (ret < 0)
			goto error;
		ret = pca9468_update_reg(pca9468, PCA9468_REG_ADC_IMPROVE, PCA9468_BIT_ADC_IIN_IMP, 0);
		if (ret < 0)
			goto error;

		/* For fixing input current error */
		/* Overwrite 0x08(bit3) in 0x41 register */
		/* Disable OCP-AVG. It is the software workaround for OCP-AVG issue */
		val = 0x08;
		ret = pca9468_write_reg(pca9468, 0x41, val);
		if (ret < 0)
			goto error;
		/* Overwrite 0x01 in 0x43 register */
		val = 0x01;
		ret = pca9468_write_reg(pca9468, 0x43, val);
		if (ret < 0)
			goto error;

		/* Overwrite 0x00 in 0x4B register */
		val = 0x00;
		ret = pca9468_write_reg(pca9468, 0x4B, val);
		if (ret < 0)
			goto error;
		/* End for fixing input current error */

	} else {
		/* Disable NTC_PROTECTION_EN */
		ret = pca9468_update_reg(pca9468, PCA9468_REG_TEMP_CTRL,
				PCA9468_BIT_NTC_PROTECTION_EN, 0);
	}

	/* Enable PCA9468 */
	val = (enable == true) ? PCA9468_STANDBY_DONOT: PCA9468_STANDBY_FORCED;
	ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL, PCA9468_BIT_STANDBY_EN, val);
	if (ret < 0)
		goto error;

	if (enable == true) {
		msleep(150);
		ret = pca9468_update_reg(pca9468, PCA9468_REG_ADC_IMPROVE, PCA9468_BIT_ADC_IIN_IMP, PCA9468_BIT_ADC_IIN_IMP);
		if (ret < 0)
			goto error;

		/* Clear bit3 to 0x41 register */
		/* Enable OCP-AVG. It is the software workaround for OCP-AVG issue */
		ret = pca9468_update_reg(pca9468, 0x41, BIT(3), 0);
		if (ret < 0)
			goto error;

		/* Lock test mode */
		val = 0x00;
		ret = pca9468_write_reg(pca9468, PCA9468_REG_ADC_ACCESS, val);

		/* Set NTC_PROTECTION_EN */
		ret = pca9468_update_reg(pca9468, PCA9468_REG_TEMP_CTRL,
								PCA9468_BIT_NTC_PROTECTION_EN,
								pca9468->pdata->ntc_en << MASK2SHIFT(PCA9468_BIT_NTC_PROTECTION_EN));
		/* Enable TEMP_REG_EN */
		ret = pca9468_update_reg(pca9468, PCA9468_REG_TEMP_CTRL,
				PCA9468_BIT_TEMP_REG_EN, PCA9468_BIT_TEMP_REG_EN);
	} else {
		/* Wait 5ms to keep the shutdown sequence */
		usleep_range(5000, 6000);
		/* Do softreset */
		ret = pca9468_softreset(pca9468);
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Stop Charging */
static int pca9468_stop_charging(struct pca9468_charger *pca9468)
{
	int ret = 0;
	int val;

	/* Check the current state */
	if ((pca9468->charging_state != DC_STATE_NO_CHARGING) ||
		(pca9468->timer_id != TIMER_ID_NONE) ||
		(pca9468->enable == true)) {
		// Stop Direct charging
		cancel_delayed_work(&pca9468->timer_work);
		cancel_delayed_work(&pca9468->pps_work);
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_ID_NONE;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);

		/* Clear parameter */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9468_set_charging_state(pca9468, DC_STATE_NO_CHARGING);
		pca9468_init_adc_val(pca9468, -1);
#else
		pca9468->charging_state = DC_STATE_NO_CHARGING;
#endif
		pca9468->ret_state = DC_STATE_NO_CHARGING;
		pca9468->ta_target_vol = pca9468->pdata->ta_max_vol;
		pca9468->prev_iin = 0;
		pca9468->prev_inc = INC_NONE;
		mutex_lock(&pca9468->lock);
		pca9468->req_new_iin = false;
		pca9468->req_new_vfloat = false;
		mutex_unlock(&pca9468->lock);
		pca9468->chg_mode = CHG_NO_DC_MODE;
		pca9468->ta_ctrl = TA_CTRL_CL_MODE;

		/* Keep IIN_CFG and VFLOAT to the current value */
		/* Keep new Vfloat and new IIN */

		/* Clear new DC mode and DC mode */
		pca9468->new_dc_mode = DC_NORMAL_MODE;
		pca9468->dc_mode = DC_NORMAL_MODE;
		mutex_lock(&pca9468->lock);
		pca9468->req_new_dc_mode = false;
		mutex_unlock(&pca9468->lock);

		/* Set vfloat decrement flag to false */
		pca9468->dec_vfloat = false;

		/* Clear previous VBAT_ADC */
		pca9468->prev_vbat = 0;

		/* Clear charging done counter */
		pca9468->done_cnt = 0;

		/* Clear retry counter */
		pca9468->retry_cnt = 0;

		/* Clear switching frequency change sequence */
		pca9468->req_sw_freq = REQ_SW_FREQ_0;

		/* Enable Reverse Current Detection */
		ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
							PCA9468_BIT_REV_IIN_DET, PCA9468_BIT_REV_IIN_DET);
		if (ret < 0) {
			pr_err("%s: Error-set RCP detection\n", __func__);
			goto error;
		}

		/* Set OV_DELTA to 30% */
		val = OV_DELTA_30P << MASK2SHIFT(PCA9468_BIT_OV_DELTA);
		ret = pca9468_update_reg(pca9468, PCA9468_REG_SAFETY_CTRL,
								PCA9468_BIT_OV_DELTA, val);
		if (ret < 0) {
			pr_err("%s: Error-set OV_DELTA\n", __func__);
			goto error;
		}

		ret = pca9468_set_charging(pca9468, false);
		if (ret < 0) {
			pr_err("%s: Error-set_charging(main)\n", __func__);
			goto error;
		}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		/* Set watchdog timer disable */
		pca9468_set_wdt_enable(pca9468, WDT_DISABLE);
#endif
	}

error:
	__pm_relax(pca9468->monitor_wake_lock);
	pr_info("%s: END, ret=%d\n", __func__, ret);

	return ret;
}


/* Compensate TA current for the target input current */
static int pca9468_set_ta_current_comp(struct pca9468_charger *pca9468)
{
	int iin;

	/* Read IIN ADC */
	iin = pca9468_read_adc(pca9468, ADCCH_IIN);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9468->iin_cc + PCA9468_IIN_CC_COMP_OFFSET)) {
		/* TA current is higher than the target input current */
		/* Check TA current and compare it with IIN_CC */
		if (pca9468->ta_cur <= pca9468->iin_cc - PCA9468_TA_CUR_LOW_OFFSET) {
			/* IIN_ADC is stiil in invalid range even though TA current is less than IIN_CC - LOW_OFFSET */
			/* TA has abnormal behavior */
			/* Decrease TA voltage (20mV) */
			pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
			pr_info("%s: Comp. Cont1: ta_vol=%d\n", __func__, pca9468->ta_vol);
			/* Update TA target voltage */
			pca9468->ta_target_vol = pca9468->ta_vol;
		} else {
			/* TA current is higher than IIN_CC - LOW_OFFSET */
			/* Decrease TA current first to reduce input current */
			/* Decrease TA current (50mA) */
			pca9468->ta_cur = pca9468->ta_cur - PD_MSG_TA_CUR_STEP;
			pr_info("%s: Comp. Cont2: ta_cur=%d\n", __func__, pca9468->ta_cur);
		}

		/* Send PD Message */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PDMSG_SEND;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
	} else {
		if (iin < (pca9468->iin_cc - PCA9468_IIN_CC_COMP_OFFSET)) {
			/* TA current is lower than the target input current */
			/* Compare present TA voltage with TA maximum voltage first */
			if (pca9468->ta_vol == pca9468->ta_max_vol) {
				/* TA voltage is already the maximum voltage */
				/* Compare present TA current with TA maximum current */
				if (pca9468->ta_cur == pca9468->ta_max_cur) {
					/* Both of present TA voltage and current are already maximum values */
					pr_info("%s: Comp. End1(Max value): ta_vol=%d, ta_cur=%d\n", __func__, pca9468->ta_vol, pca9468->ta_cur);
					/* Set timer */
					mutex_lock(&pca9468->lock);
					pca9468->timer_id = TIMER_CHECK_CCMODE;
					pca9468->timer_period = PCA9468_CCMODE_CHECK_T;
					mutex_unlock(&pca9468->lock);
				} else {
					/* TA voltage is maximum voltage, but TA current is not maximum current */
					/* Increase TA current (50mA) */
					pca9468->ta_cur = pca9468->ta_cur + PD_MSG_TA_CUR_STEP;
					if (pca9468->ta_cur > pca9468->ta_max_cur)
						pca9468->ta_cur = pca9468->ta_max_cur;
					pr_info("%s: Comp. Cont3: ta_cur=%d\n", __func__, pca9468->ta_cur);
					/* Send PD Message */
					mutex_lock(&pca9468->lock);
					pca9468->timer_id = TIMER_PDMSG_SEND;
					pca9468->timer_period = 0;
					mutex_unlock(&pca9468->lock);

					/* Set TA increment flag */
					pca9468->prev_inc = INC_TA_CUR;
				}
			} else {
				/* TA voltage is not maximum voltage */
				/* compare IIN ADC with previous IIN ADC + 20mA */
				if (iin > (pca9468->prev_iin + PCA9468_IIN_ADC_OFFSET)) {
					/* In this case, TA voltage is not enough to supply the operating current of RDO.
						So, increase TA voltage */
					/* Increase TA voltage (20mV) */
					pca9468->ta_vol = pca9468->ta_vol + PD_MSG_TA_VOL_STEP;
					if (pca9468->ta_vol > pca9468->ta_max_vol)
						pca9468->ta_vol = pca9468->ta_max_vol;
					pr_info("%s: Comp. Cont4: ta_vol=%d\n", __func__, pca9468->ta_vol);
					/* Update TA target voltage */
					pca9468->ta_target_vol = pca9468->ta_vol;
					/* Send PD Message */
					mutex_lock(&pca9468->lock);
					pca9468->timer_id = TIMER_PDMSG_SEND;
					pca9468->timer_period = 0;
					mutex_unlock(&pca9468->lock);

					/* Set TA increment flag */
					pca9468->prev_inc = INC_TA_VOL;
				} else {
					/* Input current increment is too low */
					/* It is possible that TA is in current limit mode or has low TA voltage */
					/* Increase TA current or voltage */
					/* Check the previous TA increment */
					if (pca9468->prev_inc == INC_TA_VOL) {
						/* The previous increment is TA voltage, but input current does not increase */
						/* Try to increase TA current */
						/* Compare TA max current */
						if (pca9468->ta_cur == pca9468->ta_max_cur) {
							/* TA current is already the maximum current */

							/* Increase TA voltage (20mV) */
							pca9468->ta_vol = pca9468->ta_vol + PD_MSG_TA_VOL_STEP;
							if (pca9468->ta_vol > pca9468->ta_max_vol)
								pca9468->ta_vol = pca9468->ta_max_vol;
							pr_info("%s: Comp. Cont5: ta_vol=%d\n", __func__, pca9468->ta_vol);
							/* Update TA target voltage */
							pca9468->ta_target_vol = pca9468->ta_vol;
							/* Send PD Message */
							mutex_lock(&pca9468->lock);
							pca9468->timer_id = TIMER_PDMSG_SEND;
							pca9468->timer_period = 0;
							mutex_unlock(&pca9468->lock);

							/* Set TA increment flag */
							pca9468->prev_inc = INC_TA_VOL;
						} else {
							/* Check the present TA current */
							/* Consider tolerance offset(100mA) */
							if (pca9468->ta_cur >= (pca9468->iin_cc + PCA9468_TA_IIN_OFFSET)) {
								/* Maybe TA supply current is enough, but TA voltage is low */
								/* Increase TA voltage (20mV) */
								pca9468->ta_vol = pca9468->ta_vol + PD_MSG_TA_VOL_STEP;
								if (pca9468->ta_vol > pca9468->ta_max_vol)
									pca9468->ta_vol = pca9468->ta_max_vol;
								pr_info("%s: Comp. Cont6: ta_vol=%d\n", __func__, pca9468->ta_vol);
								/* Update TA target voltage */
								pca9468->ta_target_vol = pca9468->ta_vol;
								/* Send PD Message */
								mutex_lock(&pca9468->lock);
								pca9468->timer_id = TIMER_PDMSG_SEND;
								pca9468->timer_period = 0;
								mutex_unlock(&pca9468->lock);

								/* Set TA increment flag */
								pca9468->prev_inc = INC_TA_VOL;
							} else {
								/* It is possible that TA is in current limit mode */
								/* Increase TA current (50mA) */
								pca9468->ta_cur = pca9468->ta_cur + PD_MSG_TA_CUR_STEP;
								if (pca9468->ta_cur > pca9468->ta_max_cur)
									pca9468->ta_cur = pca9468->ta_max_cur;
								pr_info("%s: Comp. Cont7: ta_cur=%d\n", __func__, pca9468->ta_cur);
								/* Send PD Message */
								mutex_lock(&pca9468->lock);
								pca9468->timer_id = TIMER_PDMSG_SEND;
								pca9468->timer_period = 0;
								mutex_unlock(&pca9468->lock);

								/* Set TA increment flag */
								pca9468->prev_inc = INC_TA_CUR;
							}
						}
					} else {
						/* The previous increment is TA current, but input current does not increase */
						/* Try to increase TA voltage */
						/* Increase TA voltage (20mV) */
						pca9468->ta_vol = pca9468->ta_vol + PD_MSG_TA_VOL_STEP;
						if (pca9468->ta_vol > pca9468->ta_max_vol)
							pca9468->ta_vol = pca9468->ta_max_vol;
						pr_info("%s: Comp. Cont8: ta_vol=%d\n", __func__, pca9468->ta_vol);
						/* Update TA target voltage */
						pca9468->ta_target_vol = pca9468->ta_vol;
						/* Send PD Message */
						mutex_lock(&pca9468->lock);
						pca9468->timer_id = TIMER_PDMSG_SEND;
						pca9468->timer_period = 0;
						mutex_unlock(&pca9468->lock);

						/* Set TA increment flag */
						pca9468->prev_inc = INC_TA_VOL;
					}
				}
			}
		} else {
			/* IIN ADC is in valid range */
			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA */
			pr_info("%s: Comp. End2(valid): ta_vol=%d, ta_cur=%d\n", __func__, pca9468->ta_vol, pca9468->ta_cur);
			/* Clear TA increment flag */
			pca9468->prev_inc = INC_NONE;

			/* Set timer */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_CCMODE;
			pca9468->timer_period = PCA9468_CCMODE_CHECK_T;
			mutex_unlock(&pca9468->lock);
		}
	}

	/* Save previous iin adc */
	pca9468->prev_iin = iin;

	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

	return 0;
}

/* Compensate TA current for constant power mode1 */
static int pca9468_set_ta_current_comp2(struct pca9468_charger *pca9468)
{
	int iin;
	unsigned int val;
	unsigned int iin_apdo;

	/* Read IIN ADC */
	iin = pca9468_read_adc(pca9468, ADCCH_IIN);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9468->iin_cfg + PCA9468_IIN_CC_COMP_OFFSET)) {
		/* TA current is higher than the target input current */
		/* Check TA current and compare it with IIN_CC */
		if (pca9468->ta_cur <= pca9468->iin_cc - PCA9468_TA_CUR_LOW_OFFSET) {
			/* IIN_ADC is stiil in invalid range even though TA current is less than IIN_CC - 200mA */
			/* TA has abnormal behavior */
			/* Decrease TA voltage (20mV) */
			pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
			pr_info("%s: Comp. Cont1: ta_vol=%d\n", __func__, pca9468->ta_vol);
			/* Update TA target voltage */
			pca9468->ta_target_vol = pca9468->ta_vol;
		} else {
			/* TA current is higher than IIN_CC - 200mA */
			/* Decrease TA current first to reduce input current */
			/* Decrease TA current (50mA) */
			pca9468->ta_cur = pca9468->ta_cur - PD_MSG_TA_CUR_STEP;
			pr_info("%s: Comp. Cont2: ta_cur=%d\n", __func__, pca9468->ta_cur);
		}

		/* Send PD Message */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PDMSG_SEND;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
	} else if (iin < (pca9468->iin_cc - PCA9468_IIN_CC_COMP_OFFSET_CP)) {
		/* IIN_ADC < IIN_CC -20mA */
		if (pca9468->ta_vol == pca9468->ta_max_vol) {
			/* Check IIN_ADC < IIN_CC - 50mA */
			if (iin < (pca9468->iin_cc - PCA9468_IIN_CC_COMP_OFFSET)) {
				/* Compare the TA current with IIN_CC and maximum current of APDO */
				if ((pca9468->ta_cur >= (pca9468->iin_cc/pca9468->chg_mode)) ||
					(pca9468->ta_cur == pca9468->ta_max_cur)) {
					/* TA current is higher than IIN_CC or maximum TA current */
					/* Set new IIN_CC to IIN_CC - 50mA */
					pca9468->iin_cc = pca9468->iin_cc - PCA9468_IIN_CC_COMP_OFFSET;
					/* Set new TA_MAX_VOL to TA_MAX_PWR/IIN_CC */
					/* Adjust new IIN_CC with APDO resolution */
					iin_apdo = pca9468->iin_cc/PD_MSG_TA_CUR_STEP;
					iin_apdo = iin_apdo*PD_MSG_TA_CUR_STEP;
					val = pca9468->ta_max_pwr/(iin_apdo/pca9468->chg_mode/1000);	/* mV */
					val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
					val = val*PD_MSG_TA_VOL_STEP; /* uV */
					/* Set new TA_MAX_VOL */
					pca9468->ta_max_vol = MIN(val, pca9468->pdata->ta_max_vol*pca9468->chg_mode);
					/* Increase TA voltage(40mV) */
					pca9468->ta_vol = pca9468->ta_vol + PD_MSG_TA_VOL_STEP*2;
					if (pca9468->ta_vol > pca9468->ta_max_vol)
						pca9468->ta_vol = pca9468->ta_max_vol;

					pr_info("%s: Comp. Cont3: ta_vol=%d\n", __func__, pca9468->ta_vol);
					/* Update TA target voltage */
					pca9468->ta_target_vol = pca9468->ta_vol;
					/* Send PD Message */
					mutex_lock(&pca9468->lock);
					pca9468->timer_id = TIMER_PDMSG_SEND;
					pca9468->timer_period = 0;
					mutex_unlock(&pca9468->lock);
				} else {
					/* TA current is less than IIN_CC and not maximum current */
					/* Increase TA current (50mA) */
					pca9468->ta_cur = pca9468->ta_cur + PD_MSG_TA_CUR_STEP;
					if (pca9468->ta_cur > pca9468->ta_max_cur)
						pca9468->ta_cur = pca9468->ta_max_cur;
					pr_info("%s: Comp. Cont4: ta_cur=%d\n", __func__, pca9468->ta_cur);
					/* Send PD Message */
					mutex_lock(&pca9468->lock);
					pca9468->timer_id = TIMER_PDMSG_SEND;
					pca9468->timer_period = 0;
					mutex_unlock(&pca9468->lock);
				}
			} else {
				/* Wait for next current step compensation */
				/* IIN_CC - 50mA < IIN ADC < IIN_CC - 20mA*/
				pr_info("%s: Comp.(wait): ta_vol=%d\n", __func__, pca9468->ta_vol);
				/* Set timer */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_CHECK_CCMODE;
				pca9468->timer_period = PCA9468_CCMODE_CHECK_T;
				mutex_unlock(&pca9468->lock);
			}
		} else {
			/* Increase TA voltage(40mV) */
			pca9468->ta_vol = pca9468->ta_vol + PD_MSG_TA_VOL_STEP*2;
			if (pca9468->ta_vol > pca9468->ta_max_vol)
				pca9468->ta_vol = pca9468->ta_max_vol;

			pr_info("%s: Comp. Cont5: ta_vol=%d\n", __func__, pca9468->ta_vol);
			/* Update TA target voltage */
			pca9468->ta_target_vol = pca9468->ta_vol;
			/* Send PD Message */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PDMSG_SEND;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
		}
	} else {
		/* IIN ADC is in valid range */
		/* IIN_CC - 50mA < IIN ADC < IIN_CFG + 150mA or IIN_LOOP*/
		pr_info("%s: Comp. End(valid): ta_vol=%d\n", __func__, pca9468->ta_vol);
		/* Set timer */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_CHECK_CCMODE;
		pca9468->timer_period = PCA9468_CCMODE_CHECK_T;
		mutex_unlock(&pca9468->lock);
	}

	/* Save previous iin adc */
	pca9468->prev_iin = iin;

	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

	return 0;
}

/* Compensate TA voltage for the target input current */
static int pca9468_set_ta_voltage_comp(struct pca9468_charger *pca9468)
{
	int iin;

	pr_info("%s: ======START=======\n", __func__);

	/* Read IIN ADC */
	iin = pca9468_read_adc(pca9468, ADCCH_IIN);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9468->iin_cc + PCA9468_IIN_CC_COMP_OFFSET)) {
		/* TA current is higher than the target input current */
		/* Decrease TA voltage (20mV) */
		pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
		pr_info("%s: Comp. Cont1: ta_vol=%d\n", __func__, pca9468->ta_vol);

		/* Send PD Message */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PDMSG_SEND;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
	} else {
		if (iin < (pca9468->iin_cc - PCA9468_IIN_CC_COMP_OFFSET)) {
			/* TA current is lower than the target input current */
			/* Compare TA max voltage */
			if (pca9468->ta_vol == pca9468->ta_max_vol) {
				/* TA current is already the maximum voltage */
				pr_info("%s: Comp. End1(max TA vol): ta_vol=%d\n", __func__, pca9468->ta_vol);
				/* Set timer */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_CHECK_CCMODE;
				pca9468->timer_period = PCA9468_CCMODE_CHECK_T;
				mutex_unlock(&pca9468->lock);
			} else {
				/* Increase TA voltage (20mV) */
				pca9468->ta_vol = pca9468->ta_vol + PD_MSG_TA_VOL_STEP;
				pr_info("%s: Comp. Cont2: ta_vol=%d\n", __func__, pca9468->ta_vol);

				/* Send PD Message */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_PDMSG_SEND;
				pca9468->timer_period = 0;
				mutex_unlock(&pca9468->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA */
			pr_info("%s: Comp. End(valid): ta_vol=%d\n", __func__, pca9468->ta_vol);
			/* Set timer */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_CCMODE;
			pca9468->timer_period = PCA9468_CCMODE_CHECK_T;
			mutex_unlock(&pca9468->lock);
		}
	}
	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

	return 0;
}

/* Change switching frequency */
static int pca9468_change_sw_freq(struct pca9468_charger *pca9468)
{
	int ret = 0;
	int vbat;
	unsigned int val;
	unsigned int power_state;

	pr_info("%s: ======START=======\n", __func__);

	/* Check request sequence */
	switch (pca9468->req_sw_freq) {
	case REQ_SW_FREQ_1:
		/* REQ_SW_FREQ_1 - Decrease TA voltage */
		/* Read VBAT_ADC */
		vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);
		/* Set TA voltage to MIN[(2*VBAT_ADC + offset), TA target voltage] */
		pca9468->ta_vol = MIN((2*vbat*pca9468->chg_mode + PCA9468_TA_VOL_PRE_OFFSET), pca9468->ta_target_vol);
		val = pca9468->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
		pca9468->ta_vol = val*PD_MSG_TA_VOL_STEP;

		pr_info("%s: REQ_SW_FREQ_%d, ta_cur=%d, ta_vol=%d\n",
			__func__, pca9468->req_sw_freq, pca9468->ta_cur, pca9468->ta_vol);

		/* Set next request sequence */
		pca9468->req_sw_freq = REQ_SW_FREQ_2;

		/* Send PD Message */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PDMSG_SEND;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
		break;

	case REQ_SW_FREQ_2:
		/* REQ_SW_FREQ_2 - Disable DC */
		/* Disable charging */
		ret = pca9468_set_charging(pca9468, false);
		if (ret < 0)
			goto error;

		pr_info("%s: REQ_SW_FREQ_%d, disable DC\n",	__func__, pca9468->req_sw_freq);

		/* Set next request sequence */
		pca9468->req_sw_freq = REQ_SW_FREQ_3;

		/* Set timer */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_ADJUST_TACUR;
		pca9468->timer_period = PCA9468_DISABLE_DELAY_T; /* Wait 200ms */
		mutex_unlock(&pca9468->lock);
		break;

	case REQ_SW_FREQ_3:
		/* REQ_SW_FREQ_3 - Set switching frequency */
		if (pca9468->iin_cc > pca9468->pdata->iin_low_freq) {
			/* Set switching frequency to high frequency */
			pca9468->fsw_cfg = pca9468->pdata->fsw_cfg;
		} else {
			/* Set switching frequency to low frequency */
			pca9468->fsw_cfg = pca9468->pdata->fsw_cfg_low;
		}
		ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
								PCA9468_BIT_FSW_CFG, pca9468->fsw_cfg);
		if (ret < 0)
			goto error;

		pr_info("%s: REQ_SW_FREQ_%d, sw_freq=%dkHz\n",	__func__, pca9468->req_sw_freq, sw_freq[pca9468->fsw_cfg]);

		/* Set next request sequence */
		pca9468->req_sw_freq = REQ_SW_FREQ_4;

		/* Set timer */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_ADJUST_TACUR;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
		break;

	case REQ_SW_FREQ_4:
		/* REQ_SW_FREQ_4 - Set TA current */

		/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
		val = pca9468->iin_cc/PD_MSG_TA_CUR_STEP;
		pca9468->iin_cc = val*PD_MSG_TA_CUR_STEP;
		/* Set TA current to IIN_CC */
		pca9468->ta_cur = pca9468->iin_cc/pca9468->chg_mode;

		/* Set TA voltage again before enable charging */
		/* Read VBAT_ADC */
		vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);
		/* Set TA voltage to MIN[(2*VBAT_ADC + offset), TA target voltage] */
		pca9468->ta_vol = MIN((2*vbat*pca9468->chg_mode + PCA9468_TA_VOL_PRE_OFFSET), pca9468->ta_target_vol);
		val = pca9468->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
		pca9468->ta_vol = val*PD_MSG_TA_VOL_STEP;

		pr_info("%s: REQ_SW_FREQ_%d, ta_cur=%d, ta_vol=%d\n",
			__func__, pca9468->req_sw_freq, pca9468->ta_cur, pca9468->ta_vol);

		/* Set next request sequence */
		pca9468->req_sw_freq = REQ_SW_FREQ_5;

		/* Send PD Message */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PDMSG_SEND;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
		break;

	case REQ_SW_FREQ_5:
		/* REQ_SW_FREQ_5 - Enable DC */
		ret = pca9468_set_charging(pca9468, true);
		if (ret < 0)
			goto error;

		pr_info("%s: REQ_SW_FREQ_%d, enable DC\n",	__func__, pca9468->req_sw_freq);

		/* Set next request sequence */
		pca9468->req_sw_freq = REQ_SW_FREQ_6;

		/* Set timer */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_ADJUST_TACUR;
		pca9468->timer_period = PCA9468_ENABLE_DELAY_T; /* Wait 150ms */
		mutex_unlock(&pca9468->lock);
		break;

	case REQ_SW_FREQ_6:
		/* REQ_SW_FREQ_6 - Increase TA voltage */

		/* Check power state */
		/* Read STS_B */
		ret = pca9468_read_reg(pca9468, PCA9468_REG_STS_B, &power_state);
		if (ret < 0)
			goto error;
		if (power_state & PCA9468_BIT_ACTIVE_STATE_STS) {
			/* TA voltage is initial voltage for switching frequency change */
			/* Increase TA voltage to TA target voltage */

			/* Calculate new TA maximum current and voltage that used in the direct charging */
			/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
			val = pca9468->iin_cc/(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
			pca9468->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
			/* Set TA_MAX_VOL to MIN[PCA9468_TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
			val = pca9468->ta_max_pwr/(pca9468->iin_cc/pca9468->chg_mode/1000);	/* mV */
			val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
			val = val*PD_MSG_TA_VOL_STEP; /* uV */
			pca9468->ta_max_vol = MIN(val, pca9468->pdata->ta_max_vol*pca9468->chg_mode);
			/* Recover IIN_CC to the original value(new_iin) */
			pca9468->iin_cc = pca9468->new_iin;

			/* Set TA voltage to TA target voltage */
			pca9468->ta_vol = pca9468->ta_target_vol;
			/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
			val = pca9468->iin_cc/PD_MSG_TA_CUR_STEP;
			pca9468->iin_cc = val*PD_MSG_TA_CUR_STEP;
			/* Set TA current to IIN_CC */
			pca9468->ta_cur = pca9468->iin_cc/pca9468->chg_mode;

			pr_info("%s: REQ_SW_FREQ_%d, ta_cur=%d, ta_vol=%d, ta_max_vol=%d, iin_cc=%d\n",
					__func__, pca9468->req_sw_freq, pca9468->ta_cur, pca9468->ta_vol,
					pca9468->ta_max_vol, pca9468->iin_cc);

			/* Set next request sequence */
			pca9468->req_sw_freq = REQ_SW_FREQ_0;

			/* Send PD Message */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PDMSG_SEND;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
		} else {
			/* pca9468 already set to enable charging, but power state is in standby state */
			/* DC error happens and check error type */
			pr_info("%s: Error - REQ_SW_FREQ_%d\n",	__func__, pca9468->req_sw_freq);
			ret = pca9468_check_error(pca9468);
			/* Overwrite error type to invalid error */
			/* Will stop charging in timer_work */
			ret = -EINVAL;
			goto error;
		}
		break;

	default:
		/* Cannot enter here */
		pr_info("%s: Error - REQ_SW_FREQ_%d\n",	__func__, pca9468->req_sw_freq);
		pca9468->req_sw_freq = REQ_SW_FREQ_0;
		ret = -EINVAL;
		goto error;
		break;
	}
	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set TA current for target current */
static int pca9468_adjust_ta_current (struct pca9468_charger *pca9468)
{
	int ret = 0;
	int vbat;
	unsigned int val;

	pr_info("%s: ======START=======\n", __func__);

	/* Set charging state to ADJUST_TACUR */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_ADJUST_TACUR);
#else
	pca9468->charging_state = DC_STATE_ADJUST_TACUR;
#endif

	/* Check switching frequency change request */
	if (pca9468->req_sw_freq != REQ_SW_FREQ_0) {
		/* There is request for switching frequency change */
		ret = pca9468_change_sw_freq(pca9468);
		if (ret < 0)
			goto error;
	} else {
		/* There is no request for switching frequency change */

		/* Check whether TA current is same as IIN_CC or not */
		if (pca9468->ta_cur == pca9468->iin_cc/pca9468->chg_mode) {
			/* finish sending PD message */
			/* Recover IIN_CC to the original value(new_iin) */
			pca9468->iin_cc = pca9468->new_iin;

			/* Update iin_cfg */
			pca9468->iin_cfg = pca9468->iin_cc;
			/* Set IIN_CFG to new IIN */
			ret = pca9468_set_input_current(pca9468, pca9468->iin_cc);
			if (ret < 0)
				goto error;

			/* Clear req_new_iin */
			mutex_lock(&pca9468->lock);
			pca9468->req_new_iin = false;
			mutex_unlock(&pca9468->lock);

			pr_info("%s: adj. End, ta_cur=%d, ta_vol=%d, iin_cc=%d\n",
					__func__, pca9468->ta_cur, pca9468->ta_vol, pca9468->iin_cc);

			/* Go to return state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9468_set_charging_state(pca9468, pca9468->ret_state);
#else
			pca9468->charging_state = pca9468->ret_state;
#endif
			/* Set timer */
			mutex_lock(&pca9468->lock);
			if (pca9468->charging_state == DC_STATE_CC_MODE)
				pca9468->timer_id = TIMER_CHECK_CCMODE;
			else
				pca9468->timer_id = TIMER_CHECK_CVMODE;
			pca9468->timer_period = 1000;	/* Wait 1s */
			mutex_unlock(&pca9468->lock);
		} else {
			/* Compare new IIN with current IIN_CFG */
			if (pca9468->iin_cc > pca9468->iin_cfg) {
				/* New iin is higher than current iin_cc(iin_cfg) */
				/* Compare new IIN with IIN_LOW_TH */
				if (pca9468->iin_cc > pca9468->pdata->iin_low_freq) {
					/* New IIN is high current */
					/* Check current switching frequency */
					if (pca9468->fsw_cfg == pca9468->pdata->fsw_cfg) {
						/* Current switching frequency is high frequency */
						/* Don't need to change switching frequency */
						/* Update iin_cfg */
						pca9468->iin_cfg = pca9468->iin_cc;
						/* Set IIN_CFG to new IIN */
						ret = pca9468_set_input_current(pca9468, pca9468->iin_cc);
						if (ret < 0)
							goto error;

						/* Clear Request flag */
						mutex_lock(&pca9468->lock);
						pca9468->req_new_iin = false;
						mutex_unlock(&pca9468->lock);

						/* Set new TA voltage and current */
						/* Read VBAT ADC */
						vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);

						/* Calculate new TA maximum current and voltage that used in the direct charging */
						/* Set IIN_CC to MIN[IIN, (TA_MAX_CUR by APDO)*chg_mode]*/
						pca9468->iin_cc = MIN(pca9468->iin_cfg, pca9468->ta_max_cur*pca9468->chg_mode);
						/* Set the current IIN_CC to iin_cfg for recovering it after resolution adjustment */
						pca9468->iin_cfg = pca9468->iin_cc;
						/* Calculate new TA max voltage */
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
						val = pca9468->iin_cc/(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
						pca9468->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
						/* Set TA_MAX_VOL to MIN[PCA9468_TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
						val = pca9468->ta_max_pwr/(pca9468->iin_cc/pca9468->chg_mode/1000);	/* mV */
						val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
						val = val*PD_MSG_TA_VOL_STEP; /* uV */
						pca9468->ta_max_vol = MIN(val, pca9468->pdata->ta_max_vol*pca9468->chg_mode);

						/* Set TA voltage to MAX[TA_MIN_VOL_PRESET, (2*VBAT_ADC + offset)] */
						pca9468->ta_vol = max(PCA9468_TA_MIN_VOL_PRESET*pca9468->chg_mode, (2*vbat*pca9468->chg_mode + PCA9468_TA_VOL_PRE_OFFSET));
						val = pca9468->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
						pca9468->ta_vol = val*PD_MSG_TA_VOL_STEP;
						/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL] */
						pca9468->ta_vol = MIN(pca9468->ta_vol, pca9468->ta_max_vol);
						/* Set TA current to IIN_CC */
						pca9468->ta_cur = pca9468->iin_cc/pca9468->chg_mode;
						/* Recover IIN_CC to the original value(iin_cfg) */
						pca9468->iin_cc = pca9468->iin_cfg;

						pr_info("%s: New IIN(1), ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d\n",
							__func__, pca9468->ta_max_vol, pca9468->ta_max_cur, pca9468->ta_max_pwr, pca9468->iin_cc);

						pr_info("%s: New IIN(1), ta_vol=%d, ta_cur=%d, sw_freq=%d\n",
							__func__, pca9468->ta_vol, pca9468->ta_cur, sw_freq[pca9468->fsw_cfg]);

						/* Clear previous IIN ADC */
						pca9468->prev_iin = 0;
						/* Clear TA increment flag */
						pca9468->prev_inc = INC_NONE;

						/* Send PD Message and go to Adjust CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
						pca9468_set_charging_state(pca9468, DC_STATE_ADJUST_CC);
#else
						pca9468->charging_state = DC_STATE_ADJUST_CC;
#endif
						mutex_lock(&pca9468->lock);
						pca9468->timer_id = TIMER_PDMSG_SEND;
						pca9468->timer_period = PCA9468_IIN_CFG_WAIT_T;
						mutex_unlock(&pca9468->lock);
					} else {
						/* Need switching frequency change */
						/* Compare TA voltage with TA target voltage */
						if (pca9468->ta_vol == pca9468->ta_target_vol) {
							/* Decrease TA voltage first before disable DC to avoid VIN voltage jumping */
							/* Read VBAT ADC */
							vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);
							/* Set TA voltage to 2*VBAT_ADC + offset */
							pca9468->ta_vol = 2*vbat*pca9468->chg_mode + PCA9468_TA_VOL_PRE_OFFSET;
							val = pca9468->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
							pca9468->ta_vol = val*PD_MSG_TA_VOL_STEP;

							pr_info("%s: New IIN(2), ta_vol=%d, ta_cur=%d, sw_freq=%d\n",
								__func__, pca9468->ta_vol, pca9468->ta_cur, sw_freq[pca9468->fsw_cfg]);

							/* Send PD Message */
							mutex_lock(&pca9468->lock);
							pca9468->timer_id = TIMER_PDMSG_SEND;
							pca9468->timer_period = 0;
							mutex_unlock(&pca9468->lock);
						} else {
							/* Disable DC */
							ret = pca9468_set_charging(pca9468, false);
							if (ret < 0)
								goto error;

							/* Update iin_cfg */
							pca9468->iin_cfg = pca9468->iin_cc;

							/* Clear req_new_iin */
							mutex_lock(&pca9468->lock);
							pca9468->req_new_iin = false;
							mutex_unlock(&pca9468->lock);
							pr_info("%s: New IIN(3), iin_cc=%d, go to DC_STATE_PRESET_DC\n", __func__, pca9468->iin_cc);

							/* Go to DC_STATE_PRESET_DC */
							mutex_lock(&pca9468->lock);
							pca9468->timer_id = TIMER_PRESET_DC;
							pca9468->timer_period = PCA9468_DISABLE_DELAY_T;
							mutex_unlock(&pca9468->lock);
						}
					}
				} else {
					/* Current switching frequency is low frequency */
					/* New IIN is also low current */
					/* Don't need to change switching frequency */
					/* Update iin_cfg */
					pca9468->iin_cfg = pca9468->iin_cc;
					/* Set IIN_CFG to new IIN */
					ret = pca9468_set_input_current(pca9468, pca9468->iin_cc);
					if (ret < 0)
						goto error;

					/* Clear Request flag */
					mutex_lock(&pca9468->lock);
					pca9468->req_new_iin = false;
					mutex_unlock(&pca9468->lock);

					/* Set new TA voltage and current */
					/* Read VBAT ADC */
					vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);

					/* Calculate new TA maximum current and voltage that used in the direct charging */
					/* Set IIN_CC to MIN[IIN, (TA_MAX_CUR by APDO)*CHG_mode] */
					pca9468->iin_cc = MIN(pca9468->iin_cfg, (pca9468->ta_max_cur*pca9468->chg_mode));
					/* Set the current IIN_CC to iin_cfg for recovering it after resolution adjustment */
					pca9468->iin_cfg = pca9468->iin_cc;
					/* Calculate new TA max voltage */
					/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
					val = pca9468->iin_cc/(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
					pca9468->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
					/* Set TA_MAX_VOL to MIN[PCA9468_TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
					val = pca9468->ta_max_pwr/(pca9468->iin_cc/pca9468->chg_mode/1000);	/* mV */
					val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
					val = val*PD_MSG_TA_VOL_STEP; /* uV */
					pca9468->ta_max_vol = MIN(val, pca9468->pdata->ta_max_vol*pca9468->chg_mode);

					/* Set TA voltage to MAX[TA_MIN_VOL_PRESET, (2*VBAT_ADC + offset)] */
					pca9468->ta_vol = max(PCA9468_TA_MIN_VOL_PRESET*pca9468->chg_mode, (2*vbat*pca9468->chg_mode + PCA9468_TA_VOL_PRE_OFFSET));
					val = pca9468->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
					pca9468->ta_vol = val*PD_MSG_TA_VOL_STEP;
					/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL] */
					pca9468->ta_vol = MIN(pca9468->ta_vol, pca9468->ta_max_vol);
					/* Set TA current to IIN_CC */
					pca9468->ta_cur = pca9468->iin_cc/pca9468->chg_mode;
					/* Recover IIN_CC to the original value(iin_cfg) */
					pca9468->iin_cc = pca9468->iin_cfg;

					pr_info("%s: New IIN(4), ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d\n",
						__func__, pca9468->ta_max_vol, pca9468->ta_max_cur, pca9468->ta_max_pwr, pca9468->iin_cc);

					pr_info("%s: New IIN(4), ta_vol=%d, ta_cur=%d, sw_freq=%d\n",
						__func__, pca9468->ta_vol, pca9468->ta_cur, sw_freq[pca9468->fsw_cfg]);

					/* Clear previous IIN ADC */
					pca9468->prev_iin = 0;
					/* Clear TA increment flag */
					pca9468->prev_inc = INC_NONE;

					/* Send PD Message and go to Adjust CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
					pca9468_set_charging_state(pca9468, DC_STATE_ADJUST_CC);
#else
					pca9468->charging_state = DC_STATE_ADJUST_CC;
#endif
					mutex_lock(&pca9468->lock);
					pca9468->timer_id = TIMER_PDMSG_SEND;
					pca9468->timer_period = PCA9468_IIN_CFG_WAIT_T;
					mutex_unlock(&pca9468->lock);
				}
			} else {
				/* New iin is lower than current iin_cc(iin_cfg) */
				/* Compare new IIN with IIN_LOW_TH */
				if (pca9468->iin_cc > pca9468->pdata->iin_low_freq) {
					/* New IIN is high current */
					/* Check current switching frequency */
					if (pca9468->fsw_cfg == pca9468->pdata->fsw_cfg) {
						/* Current switching frequency is high frequency */
						/* Don't need to change switching frequency */
						/* Calculate new TA max voltage */
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
						val = pca9468->iin_cc/(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
						pca9468->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
						/* Set TA_MAX_VOL to MIN[PCA9468_TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
						val = pca9468->ta_max_pwr/(pca9468->iin_cc/pca9468->chg_mode/1000);	/* mV */
						val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
						val = val*PD_MSG_TA_VOL_STEP; /* uV */
						pca9468->ta_max_vol = MIN(val, pca9468->pdata->ta_max_vol*pca9468->chg_mode);
						/* Recover IIN_CC to the original value(new_iin) */
						pca9468->iin_cc = pca9468->new_iin;

						/* Set TA voltage to TA target voltage */
						pca9468->ta_vol = pca9468->ta_target_vol;
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
						val = pca9468->iin_cc/PD_MSG_TA_CUR_STEP;
						pca9468->iin_cc = val*PD_MSG_TA_CUR_STEP;
						/* Set TA current to IIN_CC */
						pca9468->ta_cur = pca9468->iin_cc/pca9468->chg_mode;

						pr_info("%s: adj. cont1, ta_cur=%d, ta_vol=%d, ta_max_vol=%d, iin_cc=%d\n",
								__func__, pca9468->ta_cur, pca9468->ta_vol, pca9468->ta_max_vol, pca9468->iin_cc);

						/* Send PD Message */
						mutex_lock(&pca9468->lock);
						pca9468->timer_id = TIMER_PDMSG_SEND;
						pca9468->timer_period = 0;
						mutex_unlock(&pca9468->lock);
					} else {
						/* Current switching frequency is low frequency */
						/* Need to change switching frequency */
						/* Set switching frequency request sequence #1 */
						pca9468->req_sw_freq = REQ_SW_FREQ_1;
						pr_info("%s: adj. cont2, go to switching freq change sequence #1(high freq)\n", __func__);

						/* Set timer */
						mutex_lock(&pca9468->lock);
						pca9468->timer_id = TIMER_ADJUST_TACUR;
						pca9468->timer_period = 0;
						mutex_unlock(&pca9468->lock);
					}
				} else {
					/* New IIN is low current */
					/* Check current switching frequency */
					if (pca9468->fsw_cfg == pca9468->pdata->fsw_cfg_low) {
						/* Current switching frequency is low frequency */
						/* Don't need to change switching frequency */
						/* Calculate new TA max voltage */
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
						val = pca9468->iin_cc/(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
						pca9468->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
						/* Set TA_MAX_VOL to MIN[PCA9468_TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
						val = pca9468->ta_max_pwr/(pca9468->iin_cc/pca9468->chg_mode/1000);	/* mV */
						val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
						val = val*PD_MSG_TA_VOL_STEP; /* uV */
						pca9468->ta_max_vol = MIN(val, pca9468->pdata->ta_max_vol*pca9468->chg_mode);
						/* Recover IIN_CC to the original value(new_iin) */
						pca9468->iin_cc = pca9468->new_iin;

						/* Set TA voltage to TA target voltage */
						pca9468->ta_vol = pca9468->ta_target_vol;
						/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
						val = pca9468->iin_cc/PD_MSG_TA_CUR_STEP;
						pca9468->iin_cc = val*PD_MSG_TA_CUR_STEP;
						/* Set TA current to IIN_CC */
						pca9468->ta_cur = pca9468->iin_cc/pca9468->chg_mode;

						pr_info("%s: adj. cont3, ta_cur=%d, ta_vol=%d, ta_max_vol=%d, iin_cc=%d\n",
								__func__, pca9468->ta_cur, pca9468->ta_vol, pca9468->ta_max_vol, pca9468->iin_cc);

						/* Send PD Message */
						mutex_lock(&pca9468->lock);
						pca9468->timer_id = TIMER_PDMSG_SEND;
						pca9468->timer_period = 0;
						mutex_unlock(&pca9468->lock);
					} else {
						/* Current switching frequency is high frequency */
						/* Need to change switching frequency */
						/* Set switching frequency request sequence #1 */
						pca9468->req_sw_freq = REQ_SW_FREQ_1;
						pr_info("%s: adj. cont4, go to switching freq change sequence #1(low freq)\n", __func__);

						/* Set timer */
						mutex_lock(&pca9468->lock);
						pca9468->timer_id = TIMER_ADJUST_TACUR;
						pca9468->timer_period = 0;
						mutex_unlock(&pca9468->lock);
					} /* if (pca9468->fsw_cfg == pca9468->pdata->fsw_cfg_low) else */
				} /* if (pca9468->iin_cc > pca9468->pdata->iin_low_freq) else */
			} /* if (pca9468->iin_cc > pca9468->iin_cfg) else */
		} /* if (pca9468->ta_cur == pca9468->iin_cc/pca9468->chg_mode) else */
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
	} /* if (pca9468->req_sw_freq != REQ_SW_FREQ_0) else */

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}


/* Set TA voltage for target current */
static int pca9468_adjust_ta_voltage(struct pca9468_charger *pca9468)
{
	int iin;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_ADJUST_TAVOL);
#else
	pca9468->charging_state = DC_STATE_ADJUST_TAVOL;
#endif

	/* Read IIN ADC */
	iin = pca9468_read_adc(pca9468, ADCCH_IIN);

	/* Compare IIN ADC with target input current */
	if (iin > (pca9468->iin_cc + PD_MSG_TA_CUR_STEP)) {
		/* TA current is higher than the target input current */
		/* Decrease TA voltage (20mV) */
		pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;

		pr_info("%s: adj. Cont1, ta_vol=%d\n", __func__, pca9468->ta_vol);

		/* Send PD Message */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PDMSG_SEND;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
	} else {
		if (iin < (pca9468->iin_cc - PD_MSG_TA_CUR_STEP)) {
			/* TA current is lower than the target input current */
			/* Compare TA max voltage */
			if (pca9468->ta_vol == pca9468->ta_max_vol) {
				/* TA current is already the maximum voltage */
				/* Clear req_new_iin */
				mutex_lock(&pca9468->lock);
				pca9468->req_new_iin = false;
				mutex_unlock(&pca9468->lock);
				/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9468_set_charging_state(pca9468, pca9468->ret_state);
#else
				pca9468->charging_state = pca9468->ret_state;
#endif
				pr_info("%s: adj. End1, ta_cur=%d, ta_vol=%d, iin_cc=%d\n",
						__func__, pca9468->ta_cur, pca9468->ta_vol, pca9468->iin_cc);

				/* Go to return state */
				/* Set timer */
				mutex_lock(&pca9468->lock);
				if (pca9468->charging_state == DC_STATE_CC_MODE)
					pca9468->timer_id = TIMER_CHECK_CCMODE;
				else
					pca9468->timer_id = TIMER_CHECK_CVMODE;
				pca9468->timer_period = 1000;	/* Wait 1000ms */
				mutex_unlock(&pca9468->lock);
			} else {
				/* Increase TA voltage (20mV) */
				pca9468->ta_vol = pca9468->ta_vol + PD_MSG_TA_VOL_STEP;

				pr_info("%s: adj. Cont2, ta_vol=%d\n", __func__, pca9468->ta_vol);

				/* Send PD Message */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_PDMSG_SEND;
				pca9468->timer_period = 0;
				mutex_unlock(&pca9468->lock);
			}
		} else {
			/* IIN ADC is in valid range */
			/* Clear req_new_iin */
			mutex_lock(&pca9468->lock);
			pca9468->req_new_iin = false;
			mutex_unlock(&pca9468->lock);

			/* IIN_CC - 50mA < IIN ADC < IIN_CC + 50mA */
			/* Return charging state to the previous state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9468_set_charging_state(pca9468, pca9468->ret_state);
#else
			pca9468->charging_state = pca9468->ret_state;
#endif
			pr_info("%s: adj. End2, ta_cur=%d, ta_vol=%d, iin_cc=%d\n",
					__func__, pca9468->ta_cur, pca9468->ta_vol, pca9468->iin_cc);

			/* Go to return state */
			/* Set timer */
			mutex_lock(&pca9468->lock);
			if (pca9468->charging_state == DC_STATE_CC_MODE)
				pca9468->timer_id = TIMER_CHECK_CCMODE;
			else
				pca9468->timer_id = TIMER_CHECK_CVMODE;
			pca9468->timer_period = 1000;	/* Wait 1000ms */
			mutex_unlock(&pca9468->lock);
		}
	}
	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

	return 0;
}

/* Set TA current for bypass mode */
static int pca9468_set_bypass_ta_current(struct pca9468_charger *pca9468)
{
	int ret = 0;
	unsigned int val;

	/* Set charging state to BYPASS mode state */
	pca9468->charging_state = DC_STATE_BYPASS_MODE;

	pr_info("%s: new_iin=%d\n", __func__, pca9468->new_iin);

	/* Set IIN_CFG to new_IIN */
	pca9468->iin_cfg = pca9468->new_iin;
	pca9468->iin_cc = pca9468->new_iin;
	ret = pca9468_set_input_current(pca9468, pca9468->iin_cc);
	if (ret < 0)
		goto error;

	/* Clear Request flag */
	mutex_lock(&pca9468->lock);
	pca9468->req_new_iin = false;
	mutex_unlock(&pca9468->lock);

	/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after sending PD message */
	val = pca9468->iin_cc/PD_MSG_TA_CUR_STEP;
	pca9468->iin_cc = val*PD_MSG_TA_CUR_STEP;
	/* Set TA current to IIN_CC */
	pca9468->ta_cur = pca9468->iin_cc/pca9468->chg_mode;

	pr_info("%s: ta_cur=%d, ta_vol=%d\n", __func__, pca9468->ta_cur, pca9468->ta_vol);

	/* Recover IIN_CC to the original value(new_iin) */
	pca9468->iin_cc = pca9468->new_iin;

	/* Send PD Message */
	mutex_lock(&pca9468->lock);
	pca9468->timer_id = TIMER_PDMSG_SEND;
	pca9468->timer_period = 0;
	mutex_unlock(&pca9468->lock);
	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set TA voltage for bypass mode */
static int pca9468_set_bypass_ta_voltage_by_soc(struct pca9468_charger *pca9468, int delta_soc)
{
	int ret = 0;
	unsigned int prev_ta_vol = pca9468->ta_vol;

	if (delta_soc > 0) { // decrease soc (ref_soc - soc_now)
		pca9468->ta_vol -= PD_MSG_TA_VOL_STEP;
	} else if (delta_soc < 0) { // increase soc (ref_soc - soc_now)
		pca9468->ta_vol += PD_MSG_TA_VOL_STEP;
	} else {
		pr_info("%s: abnormal delta_soc=%d\n", __func__, delta_soc);
		return -1;
	}

	pr_info("%s: delta_soc=%d, prev_ta_vol=%d, ta_vol=%d, ta_cur=%d\n",
		__func__, delta_soc, prev_ta_vol, pca9468->ta_vol, pca9468->ta_cur);

	/* Send PD Message */
	mutex_lock(&pca9468->lock);
	pca9468->timer_id = TIMER_PDMSG_SEND;
	pca9468->timer_period = 0;
	mutex_unlock(&pca9468->lock);
	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

	return ret;
}

static int pca9468_set_bypass_ta_voltage(struct pca9468_charger *pca9468)
{
	int ret = 0;
	unsigned int val;
	int vbat;
	union power_supply_propval value = {0,};

	/* Set charging state to BYPASS mode state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_BYPASS_MODE);
#else
	pca9468->charging_state = DC_STATE_BYPASS_MODE;
#endif

	pr_info("%s: new_vfloat=%d\n", __func__, pca9468->new_vfloat);

	/* Set VFLOAT to new vfloat */
	pca9468->vfloat = pca9468->new_vfloat;
	ret = pca9468_set_vfloat(pca9468, pca9468->vfloat);
	if (ret < 0)
		goto error;

	/* Clear Request flag */
	mutex_lock(&pca9468->lock);
	pca9468->req_new_vfloat = false;
	mutex_unlock(&pca9468->lock);

	/* It needs to optimize as calculating TA voltage with battery voltage later */
	/* Read VBAT ADC */
	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	vbat = value.intval;

	/* Set TA voltage to 2*VBAT_ADC + Offset */
	val = 2 * vbat + PCA9468_TA_VOL_OFFSET_BYPASS;
	pca9468->ta_vol = val;

	pr_info("%s: ta_vol=%d, ta_cur=%d, vbat=%d\n", __func__, pca9468->ta_vol, pca9468->ta_cur, vbat);

	/* Send PD Message */
	mutex_lock(&pca9468->lock);
	pca9468->timer_id = TIMER_PDMSG_SEND;
	pca9468->timer_period = 0;
	mutex_unlock(&pca9468->lock);
	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set new input current */
static int pca9468_set_new_iin(struct pca9468_charger *pca9468)
{
	int ret = 0;

	pr_info("%s: new_iin=%d\n", __func__, pca9468->new_iin);

	/* Check the charging state */
	if ((pca9468->charging_state == DC_STATE_CC_MODE) ||
		(pca9468->charging_state == DC_STATE_CV_MODE) ||
		(pca9468->charging_state == DC_STATE_BYPASS_MODE)) {
		/* Check bypass mode */
		if (pca9468->dc_mode != PTM_NONE) {
			/* Set TA current for bypass mode */
			ret = pca9468_set_bypass_ta_current(pca9468);
		} else {
			/* cancel delayed_work */
			cancel_delayed_work(&pca9468->timer_work);

			/* Set new IIN to IIN_CC */
			pca9468->iin_cc = pca9468->new_iin;

			/* Save return state */
			pca9468->ret_state = pca9468->charging_state;

			/* Check new IIN with the minimum TA current */
			if (pca9468->iin_cc < (PCA9468_TA_MIN_CUR * pca9468->chg_mode)) {
				/* Set the TA current to PCA9468_TA_MIN_CUR(1.0A) */
				pca9468->ta_cur = PCA9468_TA_MIN_CUR;
				/* Need to control TA voltage for request current */
				ret = pca9468_adjust_ta_voltage(pca9468);
			} else {
				/* Need to control TA current for request current */
				ret = pca9468_adjust_ta_current(pca9468);
			}
		}
	} else if (pca9468->charging_state == DC_STATE_WC_CV_MODE) {
		/* Charging State is WC CV state */
		/* cancel delayed_work */
		cancel_delayed_work(&pca9468->timer_work);

		/* Set IIN_CFG to new iin */
		pca9468->iin_cfg = pca9468->new_iin;
		ret = pca9468_set_input_current(pca9468, pca9468->iin_cfg);
		if (ret < 0)
			goto error;

		/* Clear req_new_iin */
		mutex_lock(&pca9468->lock);
		pca9468->req_new_iin = false;
		mutex_unlock(&pca9468->lock);

		/* Set IIN_CC to new iin */
		pca9468->iin_cc = pca9468->new_iin;

		pr_info("%s: WC CV state, New IIN=%d\n", __func__, pca9468->iin_cc);

		/* Go to WC CV mode */
		pca9468->charging_state = DC_STATE_WC_CV_MODE;
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_CHECK_WCCVMODE;
		pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
	} else {
		/* Wait for next valid state */
		pr_info("%s: Not support new iin yet in charging state=%d\n", __func__, pca9468->charging_state);
	}

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}


/* Set new float voltage */
static int pca9468_set_new_vfloat(struct pca9468_charger *pca9468)
{
	int ret = 0;
	int vbat;
	unsigned int val;

	pr_info("%s: new_vfloat=%d\n", __func__, pca9468->new_vfloat);

	/* Check the charging state */
	if ((pca9468->charging_state == DC_STATE_CC_MODE) ||
		(pca9468->charging_state == DC_STATE_CV_MODE) ||
		(pca9468->charging_state == DC_STATE_BYPASS_MODE)) {
		/* Check bypass mode */
		if (pca9468->dc_mode != PTM_NONE) {
			/* Set TA voltage for bypass mode */
			ret = pca9468_set_bypass_ta_voltage(pca9468);
		} else {
			/* Read VBAT ADC */
			vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);
			/* Compare the new VBAT with present vfloat */
			if (pca9468->new_vfloat > pca9468->vfloat) {
				/* cancel delayed_work */
				cancel_delayed_work(&pca9468->timer_work);

				/* Set vfloat decrement flag to false */
				pca9468->dec_vfloat = false;

				/* Set VFLOAT to new vfloat */
				pca9468->vfloat = pca9468->new_vfloat;
				/* Check VFLOAT method */
				if (pca9468->pdata->fg_vfloat == true) {
					/* Set PCA9468 VFLOAT to default value */
					ret = pca9468_set_vfloat(pca9468, pca9468->pdata->dft_vfloat);
				} else {
					/* Set PCA9468 VFLOAT to new vfloat */
					ret = pca9468_set_vfloat(pca9468, pca9468->vfloat);
				}
				if (ret < 0)
					goto error;
				/* Set IIN_CFG to the current IIN_CC */
				pca9468->iin_cfg = pca9468->iin_cc;	/* save the current iin_cc in iin_cfg */
				pca9468->iin_cfg = MIN(pca9468->iin_cfg, pca9468->ta_max_cur*pca9468->chg_mode);
				ret = pca9468_set_input_current(pca9468, pca9468->iin_cfg);
				if (ret < 0)
					goto error;
				pca9468->iin_cc = pca9468->iin_cfg;

				/* Clear req_new_vfloat */
				mutex_lock(&pca9468->lock);
				pca9468->req_new_vfloat = false;
				mutex_unlock(&pca9468->lock);

				/* Calculate new TA maximum voltage that used in the direct charging */
				/* Calculate new TA max voltage */
				/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
				val = pca9468->iin_cc/(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
				pca9468->iin_cc = val*(PD_MSG_TA_CUR_STEP*pca9468->chg_mode);
				/* Set TA_MAX_VOL to MIN[PCA9468_TA_MAX_VOL, (TA_MAX_PWR/IIN_CC)] */
				val = pca9468->ta_max_pwr/(pca9468->iin_cc/pca9468->chg_mode/1000);	/* mV */
				val = val*1000/PD_MSG_TA_VOL_STEP;	/* uV */
				val = val*PD_MSG_TA_VOL_STEP; /* Adjust values with APDO resolution(20mV) */
				pca9468->ta_max_vol = MIN(val, pca9468->pdata->ta_max_vol*pca9468->chg_mode);

				/* Set TA voltage to MAX[TA_MIN_VOL_PRESET*TA_mode, (2*VBAT_ADC*TA_mode + offset)] */
				pca9468->ta_vol = max(PCA9468_TA_MIN_VOL_PRESET*pca9468->chg_mode, (2*vbat*pca9468->chg_mode + PCA9468_TA_VOL_PRE_OFFSET));
				val = pca9468->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
				pca9468->ta_vol = val*PD_MSG_TA_VOL_STEP;
				/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL] */
				pca9468->ta_vol = MIN(pca9468->ta_vol, pca9468->ta_max_vol);
				/* Set TA current to IIN_CC */
				pca9468->ta_cur = pca9468->iin_cc/pca9468->chg_mode;
				/* Recover IIN_CC to the original value(iin_cfg) */
				pca9468->iin_cc = pca9468->iin_cfg;

				pr_info("%s: New VFLOAT, ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d\n",
					__func__, pca9468->ta_max_vol, pca9468->ta_max_cur, pca9468->ta_max_pwr, pca9468->iin_cc);

				/* Clear previous IIN ADC */
				pca9468->prev_iin = 0;
				/* Clear TA increment flag */
				pca9468->prev_inc = INC_NONE;

				/* Send PD Message and go to Adjust CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9468_set_charging_state(pca9468, DC_STATE_ADJUST_CC);
#else
				pca9468->charging_state = DC_STATE_ADJUST_CC;
#endif
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_PDMSG_SEND;
				pca9468->timer_period = 0;
				mutex_unlock(&pca9468->lock);
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			} else if (pca9468->new_vfloat == pca9468->vfloat) {
				/* New vfloat is same as the present vfloat */
				/* Don't need any setting */
				/* cancel delayed_work */
				cancel_delayed_work(&pca9468->timer_work);

				/* Clear req_new_vfloat */
				mutex_lock(&pca9468->lock);
				pca9468->req_new_vfloat = false;
				mutex_unlock(&pca9468->lock);

				/* Go to the present state */
				pr_info("%s: New vfloat is same as present vfloat and go to the present state\n", __func__);

				/* Set timer */
				mutex_lock(&pca9468->lock);
				if (pca9468->charging_state == DC_STATE_CC_MODE)
					pca9468->timer_id = TIMER_CHECK_CCMODE;
				else
					pca9468->timer_id = TIMER_CHECK_CVMODE;
				pca9468->timer_period = 0;
				mutex_unlock(&pca9468->lock);
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			} else {
				/* The new vfloat is lower than present vfloat */
				/* cancel delayed_work */
				cancel_delayed_work(&pca9468->timer_work);

				/* Set vfloat decrement flag */
				pca9468->dec_vfloat = true;

				/* Set VFLOAT to new vfloat */
				pca9468->vfloat = pca9468->new_vfloat;
				/* Check VFLOAT method */
				if (pca9468->pdata->fg_vfloat == true) {
					/* Set PCA9468 VFLOAT to default value */
					ret = pca9468_set_vfloat(pca9468, pca9468->pdata->dft_vfloat);
				} else {
					/* Set PCA9468 VFLOAT to new vfloat */
					ret = pca9468_set_vfloat(pca9468, pca9468->vfloat);
				}
				if (ret < 0)
					goto error;

				/* Clear req_new_vfloat */
				mutex_lock(&pca9468->lock);
				pca9468->req_new_vfloat = false;
				mutex_unlock(&pca9468->lock);

				pr_info("%s: New vfloat is lower than present vfloat and go to Pre-CV state\n", __func__);

				/* Go to Pre-CV mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9468_set_charging_state(pca9468, DC_STATE_START_CV);
#else
				pca9468->charging_state = DC_STATE_START_CV;
#endif
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_ENTER_CVMODE;
				pca9468->timer_period = 0;
				mutex_unlock(&pca9468->lock);
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			}
		}
	} else if (pca9468->charging_state == DC_STATE_WC_CV_MODE) {
		/* Charging State is WC CV state */
		/* Read VBAT ADC */
		vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);
		/* Compare the new VBAT with the current VBAT */
		if (pca9468->new_vfloat > vbat) {
			/* cancel delayed_work */
			cancel_delayed_work(&pca9468->timer_work);

			/* Set VFLOAT to new vfloat */
			pca9468->vfloat = pca9468->new_vfloat;
			/* Check VFLOAT method */
			if (pca9468->pdata->fg_vfloat == true) {
				/* Set PCA9468 VFLOAT to default value */
				ret = pca9468_set_vfloat(pca9468, pca9468->pdata->dft_vfloat);
			} else {
				/* Set PCA9468 VFLOAT to new vfloat */
				ret = pca9468_set_vfloat(pca9468, pca9468->vfloat);
			}
			if (ret < 0)
				goto error;

			/* Clear req_new_vfloat */
			mutex_lock(&pca9468->lock);
			pca9468->req_new_vfloat = false;
			mutex_unlock(&pca9468->lock);

			pr_info("%s: WC CV state, New VFLOAT=%d\n", __func__, pca9468->vfloat);

			/* Go to WC CV mode */
			pca9468->charging_state = DC_STATE_WC_CV_MODE;
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_WCCVMODE;
			pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		}
	} else {
		/* Wait for next valid state */
		pr_info("%s: Not support new vfloat yet in charging state=%d\n", __func__, pca9468->charging_state);
	}

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Set new direct charging mode */
static int pca9468_set_new_dc_mode(struct pca9468_charger *pca9468)
{
	int ret = 0;
	unsigned int val;
	int vbat;

	/* Read VBAT ADC */
	vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);

	/* check new DC mode */
	if (pca9468->new_dc_mode != PTM_NONE) {
		/* Change DC mode from Normal mode to Pass Through mode */
		/* Check current dc mode */
		if (pca9468->dc_mode != PTM_NONE) {
			/* TA voltage already changed to Pass Through mode */
			/* Disable Reverse Current Detection */
			val = 0;
			ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL, PCA9468_BIT_REV_IIN_DET, val);
			if (ret < 0)
				goto error;

			/* Set switching frequency to new frequency for pass through mode */
			val = pca9468->pdata->fsw_cfg_byp;
			ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL, PCA9468_BIT_FSW_CFG, val);
			if (ret < 0)
				goto error;
			pr_info("%s: sw_freq_byp=%dkHz\n", __func__, sw_freq[pca9468->pdata->fsw_cfg_byp]);

			/* Enable charging */
			ret = pca9468_set_charging(pca9468, true);
			if (ret < 0)
				goto error;

			/* Clear request flag */
			mutex_lock(&pca9468->lock);
			pca9468->req_new_dc_mode = false;
			mutex_unlock(&pca9468->lock);

			pr_info("%s: Go to BYPASS state, dc_mode=%d\n", __func__, pca9468->dc_mode);

			/* Go to bypass mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9468_set_charging_state(pca9468, DC_STATE_BYPASS_MODE);
#else
			pca9468->charging_state = DC_STATE_BYPASS_MODE;
#endif
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_BYPASSMODE;
			pca9468->timer_period = PCA9468_BYPASS_WAIT_T;	/* 200ms */
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		} else {
			/* DC mode is normal mode */
			/* TA voltage is not changed to bypass mode yet */
			/* Disable charging first */
			ret = pca9468_set_charging(pca9468, false);
			if (ret < 0)
				goto error;
			/* Save new DC mode */
			pca9468->dc_mode = pca9468->new_dc_mode;
			/* Set TA voltage to bypass voltage */
			pca9468->ta_vol = 2*vbat + PCA9468_TA_VOL_OFFSET_BYPASS;
			pr_info("%s: New DC mode, Normal->BYP, ta_vol=%d, ta_cur=%d\n",
				__func__, pca9468->ta_vol, pca9468->ta_cur);

			/* Send PD Message and go to dcmode change state */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9468_set_charging_state(pca9468, DC_STATE_DCMODE_CHANGE);
#else
			pca9468->charging_state = DC_STATE_DCMODE_CHANGE;
#endif
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PDMSG_SEND;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		}
	} else {
		/* Change DC mode from Pass Through mode to Normal mode */
		/* Disable charging first */
		ret = pca9468_set_charging(pca9468, false);
		if (ret < 0)
			goto error;

		/* Enable Reverse Current Detection */
		val = PCA9468_BIT_REV_IIN_DET;
		ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
								PCA9468_BIT_REV_IIN_DET, val);
		if (ret < 0)
			goto error;

		/* Save new DC mode */
		pca9468->dc_mode = pca9468->new_dc_mode;

		/* Clear request flag */
		mutex_lock(&pca9468->lock);
		pca9468->req_new_dc_mode = false;
		mutex_unlock(&pca9468->lock);

		pr_info("%s: Go to DC_STATE_PRESET_DC sate, dc_mode=%d\n", __func__, pca9468->dc_mode);

		/* Go to DC_STATE_PRESET_DC */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PRESET_DC;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
	}

error:
	pr_info("%s: ret=%d\n", __func__, ret);
	return ret;
}

/* Check Active status */
static int pca9468_check_error(struct pca9468_charger *pca9468)
{
	int ret;
	unsigned int reg_val;
	int vbatt;
	u8 val[8];
	u8 test_val[16];	/* Dump for registers */

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468->chg_status = POWER_SUPPLY_STATUS_CHARGING;
	pca9468->health_status = POWER_SUPPLY_HEALTH_GOOD;
#endif

	/* Read STS_B */
	ret = pca9468_read_reg(pca9468, PCA9468_REG_STS_B, &reg_val);
	if (ret < 0)
		goto error;

	/* Read VBAT_ADC */
	vbatt = pca9468_read_adc(pca9468, ADCCH_VBAT);

	/* Check Active status */
	if (reg_val & PCA9468_BIT_ACTIVE_STATE_STS) {
		/* PCA9468 is active state */
		/* PCA9468 is in charging */
		/* Check whether the battery voltage is over the minimum voltage level or not */
		if (vbatt > PCA9468_DC_VBAT_MIN) {
			/* Normal charging battery level */
			/* Check temperature regulation loop */
			/* Read INT1_STS register */
			ret = pca9468_read_reg(pca9468, PCA9468_REG_INT1_STS, &reg_val);
			if (reg_val & PCA9468_BIT_TEMP_REG_STS) {
				/* Over temperature protection */
				pr_err("%s: Device is in temperature regulation", __func__);
				/* Read all status register for debugging */
				ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_INT1, &val[PCA9468_REG_INT1], 7);
				pr_err("%s: Error reg[1]=0x%x,[2]=0x%x,[3]=0x%x,[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",
					__func__, val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
				/* Read ADC registers for debugging */
				ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_1, test_val, 9);
				pr_err("%s: Error reg[0x08]=0x%x,[0x09]=0x%x,[0x0A]=0x%x,[0x0B]=0x%x,[0x0C]=0x%x,[0x0D]=0x%x,[0x0E]=0x%x,[0x0F]=0x%x,[0x10]=0x%x\n",
					__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4], test_val[5], test_val[6], test_val[7], test_val[8]);
				/* Read Control registers for debugging */
				ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_IIN_CTRL, test_val, 10);
				pr_err("%s: Error reg[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x,[0x24]=0x%x,[0x25]=0x%x\n",
					__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4]);
				pr_err("%s: Error reg[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x,[0x2A]=0x%x\n",
					__func__, test_val[5], test_val[6], test_val[7], test_val[8], test_val[9]);
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
		/* PCA9468 is not active state - standby or shutdown */
		/* Stop charging in timer_work */

		/* Read all status register for debugging */
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_INT1, &val[PCA9468_REG_INT1], 7);
		pr_err("%s: Error reg[1]=0x%x,[2]=0x%x,[3]=0x%x,[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",
			__func__, val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
		/* Read ADC registers for debugging */
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_1, test_val, 9);
		pr_err("%s: Error reg[0x08]=0x%x,[0x09]=0x%x,[0x0A]=0x%x,[0x0B]=0x%x,[0x0C]=0x%x,[0x0D]=0x%x,[0x0E]=0x%x,[0x0F]=0x%x,[0x10]=0x%x\n",
			__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4], test_val[5], test_val[6], test_val[7], test_val[8]);
		/* Read Control registers for debugging */
		ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_IIN_CTRL, test_val, 10);
		pr_err("%s: Error reg[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x,[0x24]=0x%x,[0x25]=0x%x\n",
			__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4]);
		pr_err("%s: Error reg[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x,[0x2A]=0x%x\n",
			__func__, test_val[5], test_val[6], test_val[7], test_val[8], test_val[9]);
		/* Read test register for debugging */
		ret = pca9468_bulk_read_reg(pca9468, 0x40, test_val, 16);
		pr_err("%s: Error reg[0x40]=0x%x,[0x41]=0x%x,[0x42]=0x%x,[0x43]=0x%x,[0x44]=0x%x,[0x45]=0x%x,[0x46]=0x%x,[0x47]=0x%x\n",
			__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4], test_val[5], test_val[6], test_val[7]);
		pr_err("%s: Error reg[0x48]=0x%x,[0x49]=0x%x,[0x4A]=0x%x,[0x4B]=0x%x,[0x4C]=0x%x,[0x4D]=0x%x,[0x4E]=0x%x,[0x4F]=0x%x\n",
			__func__, test_val[8], test_val[9], test_val[10], test_val[11], test_val[12], test_val[13], test_val[14], test_val[15]);

		/* Check INT1_STS first */
		if ((val[PCA9468_REG_INT1_STS] & PCA9468_BIT_V_OK_STS) != PCA9468_BIT_V_OK_STS) {
			/* VBUS is invalid */
			pr_err("%s: VOK is invalid", __func__);
			/* Check STS_A */
			if (val[PCA9468_REG_STS_A] & PCA9468_BIT_CFLY_SHORT_STS) {
				pr_err("%s: Flying Cap is shorted to GND", __func__);	/* Flying cap is short to GND */
				ret = -EINVAL;
			} else if (val[PCA9468_REG_STS_A] & PCA9468_BIT_VOUT_UV_STS) {
				pr_err("%s: VOUT UV", __func__);	/* VOUT < VOUT_OK */
				ret = -EINVAL;
			} else if (val[PCA9468_REG_STS_A] & PCA9468_BIT_VBAT_OV_STS) {
				pr_err("%s: VBAT OV", __func__);	/* VBAT > VBAT_OV */
				ret = -EINVAL;
			} else if (val[PCA9468_REG_STS_A] & PCA9468_BIT_VIN_OV_STS) {
				pr_err("%s: VIN OV", __func__);		/* VIN > V_OV_FIXED or V_OV_TRACKING */
				ret = -EAGAIN;
			} else if (val[PCA9468_REG_STS_A] & PCA9468_BIT_VIN_UV_STS) {
				pr_err("%s: VIN UV", __func__);		/* VIN < V_UVTH */
				ret = -EINVAL;
			} else {
				pr_err("%s: Invalid VIN or VOUT", __func__);
				ret = -EINVAL;
			}
		} else if (val[PCA9468_REG_INT1_STS] & PCA9468_BIT_NTC_TEMP_STS) {
			/* NTC protection */
			if (pca9468->pdata->ntc_en == 0) {
				/* NTC protection function is disabled */
				/* ignore it */
				pr_err("%s: NTC_THRESHOLD - ignore\n", __func__);	/* NTC_THRESHOLD */
				ret = -EAGAIN;
			} else {
				int ntc_adc, ntc_th;
				/* NTC threshold */
				u8 reg_data[2];
				pca9468_bulk_read_reg(pca9468, PCA9468_REG_NTC_TH_1, reg_data, 2);
				ntc_th = ((reg_data[1] & PCA9468_BIT_NTC_THRESHOLD9_8)<<8) | reg_data[0];	/* uV unit */
				/* Read NTC ADC */
				ntc_adc = pca9468_read_adc(pca9468, ADCCH_NTC);	/* uV unit */
				pr_err("%s: NTC Protection, NTC_TH=%d(uV), NTC_ADC=%d(uV)", __func__, ntc_th, ntc_adc);

				ret = -EINVAL;
			}
		} else if (val[PCA9468_REG_INT1_STS] & PCA9468_BIT_CTRL_LIMIT_STS) {
			/* OCP event happens */
			/* Check STS_B */
			if (val[PCA9468_REG_STS_B] & PCA9468_BIT_OCP_FAST_STS) {
				pr_err("%s: IIN is over OCP_FAST", __func__); 	/* OCP_FAST happened */
				ret = -EINVAL;
			} else if (val[PCA9468_REG_STS_B] & PCA9468_BIT_OCP_AVG_STS) {
				pr_err("%s: IIN is over OCP_AVG", __func__);	/* OCP_AVG happened */
				ret = -EAGAIN;
			} else {
				pr_err("%s: No Loop active", __func__);
				ret = -EINVAL;
			}
		} else if (val[PCA9468_REG_INT1_STS] & PCA9468_BIT_TEMP_REG_STS) {
			/* Over temperature protection */
			pr_err("%s: Device is in temperature regulation", __func__);

			ret = -EINVAL;
		} else if (val[PCA9468_REG_INT1_STS] & PCA9468_BIT_TIMER_STS) {
			/* Check STS_B */
			if (val[PCA9468_REG_STS_B] & PCA9468_BIT_CHARGE_TIMER_STS)
				pr_err("%s: Charger timer is expired", __func__);
			else if (val[PCA9468_REG_STS_B] & PCA9468_BIT_WATCHDOG_TIMER_STS)
				pr_err("%s: Watchdog timer is expired", __func__);
			else
				pr_err("%s: Timer INT, but no timer STS", __func__);

			ret = -EINVAL;
		}
		else if (val[PCA9468_REG_STS_A] & PCA9468_BIT_CFLY_SHORT_STS) {
			/* Flying cap short */
			pr_err("%s: Flying Cap is shorted to GND", __func__);	/* Flying cap is short to GND */
			ret = -EINVAL;
		} else {
			/* There is no error, but not active state */
			if (reg_val & PCA9468_BIT_STANDBY_STATE_STS) {
				/* Standby state */
				/* Check the RCP condition, T_REVI_DET is 300ms */
				/* Wait 200ms */
				msleep(200);

				/* Check the charging state */
				/* Sometimes battery driver might call set_property function to stop charging during msleep
					At this case, charging state would change DC_STATE_NO_CHARGING.
					PCA9468 should stop checking RCP condition and exit timer_work
				*/
				if (pca9468->charging_state == DC_STATE_NO_CHARGING) {
					pr_err("%s: other driver forced to stop direct charging\n", __func__);
					ret = -EINVAL;
				} else {
					/* Keep the current charging state */
					/* Check PCA948 state again */
					/* Read STS_B */
					ret = pca9468_read_reg(pca9468, PCA9468_REG_STS_B, &reg_val);
					if (ret < 0)
						goto error;

					pr_info("%s:RCP check, STS_B=0x%x\n",	__func__, reg_val);

					/* Check Active status */
					if (reg_val & PCA9468_BIT_ACTIVE_STATE_STS) {
						/* RCP condition happened, but VIN is still valid */
						/* If VIN is increased, input current will increase over IIN_LOW level */
						/* Normal charging */
						pr_info("%s: RCP happened before, but VIN is valid\n", __func__);
						ret = -ERROR_DCRCP;
					} else if (reg_val & PCA9468_BIT_STANDBY_STATE_STS) {
						/* It is not RCP condition */
						/* Need to retry if DC is in starting state */
						pr_err("%s: Any abnormal condition, retry\n", __func__);
						/* Dump register */
						ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_INT1, &val[PCA9468_REG_INT1], 7);
						pr_err("%s: Error reg[1]=0x%x,[2]=0x%x,[3]=0x%x,[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",
								__func__, val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
						/* Read ADC registers for debugging */
						ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_1, test_val, 9);
						pr_err("%s: Error reg[0x08]=0x%x,[0x09]=0x%x,[0x0A]=0x%x,[0x0B]=0x%x,[0x0C]=0x%x,[0x0D]=0x%x,[0x0E]=0x%x,[0x0F]=0x%x,[0x10]=0x%x\n",
							__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4], test_val[5], test_val[6], test_val[7], test_val[8]);
						/* Read Control registers for debugging */
						ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_IIN_CTRL, test_val, 10);
						pr_err("%s: Error reg[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x,[0x24]=0x%x,[0x25]=0x%x\n",
							__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4]);
						pr_err("%s: Error reg[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x,[0x2A]=0x%x\n",
							__func__, test_val[5], test_val[6], test_val[7], test_val[8], test_val[9]);
						ret = pca9468_bulk_read_reg(pca9468, 0x48, test_val, 3);
						pr_err("%s: Error reg[0x48]=0x%x,[0x49]=0x%x,[0x4a]=0x%x\n",
								__func__, test_val[0], test_val[1], test_val[2]);
						/* Check charging state - Only retry in check active state */
						if (pca9468->charging_state == DC_STATE_CHECK_ACTIVE)
							ret = -EAGAIN;
						else
							ret = -EINVAL;
					} else if (reg_val & PCA9468_BIT_SHUTDOWN_STATE_STS) {
						/* Shutdown State */
						pr_err("%s: Shutdown state\n", __func__);
						/* Dump register */
						ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_INT1, &val[PCA9468_REG_INT1], 7);
						pr_err("%s: Error reg[1]=0x%x,[2]=0x%x,[3]=0x%x,[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",
								__func__, val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
						/* Read ADC registers for debugging */
						ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_1, test_val, 9);
						pr_err("%s: Error reg[0x08]=0x%x,[0x09]=0x%x,[0x0A]=0x%x,[0x0B]=0x%x,[0x0C]=0x%x,[0x0D]=0x%x,[0x0E]=0x%x,[0x0F]=0x%x,[0x10]=0x%x\n",
							__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4], test_val[5], test_val[6], test_val[7], test_val[8]);
						/* Read Control registers for debugging */
						ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_IIN_CTRL, test_val, 10);
						pr_err("%s: Error reg[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x,[0x24]=0x%x,[0x25]=0x%x\n",
							__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4]);
						pr_err("%s: Error reg[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x,[0x2A]=0x%x\n",
							__func__, test_val[5], test_val[6], test_val[7], test_val[8], test_val[9]);
						ret = pca9468_bulk_read_reg(pca9468, 0x48, test_val, 3);
						pr_err("%s: Error reg[0x48]=0x%x,[0x49]=0x%x,[0x4a]=0x%x\n",
								__func__, test_val[0], test_val[1], test_val[2]);
						ret = -EINVAL;
					} else {
						/* Power State Error - Retry */
						pr_err("%s: No Power state\n", __func__);
						/* Dump register */
						ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_INT1, &val[PCA9468_REG_INT1], 7);
						pr_err("%s: Error reg[1]=0x%x,[2]=0x%x,[3]=0x%x,[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",
								__func__, val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
						/* Read ADC registers for debugging */
						ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_ADC_1, test_val, 9);
						pr_err("%s: Error reg[0x08]=0x%x,[0x09]=0x%x,[0x0A]=0x%x,[0x0B]=0x%x,[0x0C]=0x%x,[0x0D]=0x%x,[0x0E]=0x%x,[0x0F]=0x%x,[0x10]=0x%x\n",
								__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4], test_val[5], test_val[6], test_val[7], test_val[8]);
						/* Read Control registers for debugging */
						ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_IIN_CTRL, test_val, 10);
						pr_err("%s: Error reg[0x21]=0x%x,[0x22]=0x%x,[0x23]=0x%x,[0x24]=0x%x,[0x25]=0x%x\n",
								__func__, test_val[0], test_val[1], test_val[2], test_val[3], test_val[4]);
						pr_err("%s: Error reg[0x26]=0x%x,[0x27]=0x%x,[0x28]=0x%x,[0x29]=0x%x,[0x2A]=0x%x\n",
								__func__, test_val[5], test_val[6], test_val[7], test_val[8], test_val[9]);
						ret = pca9468_bulk_read_reg(pca9468, 0x48, test_val, 3);
						pr_err("%s: Error reg[0x48]=0x%x,[0x49]=0x%x,[0x4a]=0x%x\n",
								__func__, test_val[0], test_val[1], test_val[2]);
						/* Check charging state - Only retry in check active state */
						if (pca9468->charging_state == DC_STATE_CHECK_ACTIVE)
							ret = -EAGAIN;	/* Retry */
						else
							ret = -EINVAL;
						}
					}
			} else if (reg_val & PCA9468_BIT_SHUTDOWN_STATE_STS) {
				/* PCA9468 is in shutdown state */
				pr_err("%s: PCA9468 is in shutdown state\n", __func__);
				ret = -EINVAL;
			} else {
				/* Power State Error - Retry */
				pr_err("%s: No Power state\n", __func__);
				/* Dump register */
				ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_INT1, &val[PCA9468_REG_INT1], 7);
				pr_err("%s: Error reg[1]=0x%x,[2]=0x%x,[3]=0x%x,[4]=0x%x,[5]=0x%x,[6]=0x%x,[7]=0x%x\n",
						__func__, val[1], val[2], val[3], val[4], val[5], val[6], val[7]);
				ret = pca9468_bulk_read_reg(pca9468, 0x48, test_val, 3);
				pr_err("%s: Error reg[0x48]=0x%x,[0x49]=0x%x,[0x4a]=0x%x\n",
						__func__, test_val[0], test_val[1], test_val[2]);
				/* Check charging state - Only retry in check active state */
				if (pca9468->charging_state == DC_STATE_CHECK_ACTIVE)
					ret = -EAGAIN;
				else
					ret = -EINVAL;
			}
		}
	}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_test_read(pca9468);
#endif

error:
	/* Check RCP DONE case */
	if (ret == -ERROR_DCRCP) {
		/* Check DC state first */
		if ((pca9468->charging_state == DC_STATE_START_CV) ||
			(pca9468->charging_state == DC_STATE_CV_MODE)) {
			/* Now present state is start_cv or cv_mode */
			/* Compare VBAT_ADC with Vfloat threshold */
			if (pca9468->prev_vbat > pca9468->vfloat) {
				/* Keep RCP DONE error */
				pr_info("%s: Keep RCP_DONE error type(%d)\n",
						__func__, ret);
			} else {
				/* Overwrite error type to -EINVAL */
				ret = -EINVAL;
				pr_info("%s: Overwrite RCP_DONE error, prev_vbat=%duV\n",
						__func__, pca9468->prev_vbat);
			}
		} else {
			/* Overwrite error type to -EINVAL */
			ret = -EINVAL;
			pr_info("%s: Overwrite RCP_DONE error, charging_state=%d\n",
					__func__, pca9468->charging_state);
		}
	}
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (ret == -EINVAL) {
		pca9468->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		pca9468->health_status = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
	}
#endif
	pr_info("%s: Active Status=%d\n", __func__, ret);
	return ret;
}


/* Check CC Mode status */
static int pca9468_check_ccmode_status(struct pca9468_charger *pca9468)
{
	unsigned int reg_val;
	int ret;
	int vbatt;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_DUAL_BATTERY)
	union power_supply_propval value = {0,};

	psy_do_property("battery", get,
		POWER_SUPPLY_EXT_PROP_DIRECT_VBAT_CHECK, value);
	if (value.intval) {
		ret = CCMODE_VFLT_LOOP;
		pr_info("%s: CC MODE will be done by main or sub vbatt\n", __func__);
		goto error;
	}
#endif

	/* Read STS_A */
	ret = pca9468_read_reg(pca9468, PCA9468_REG_STS_A, &reg_val);
	if (ret < 0)
		goto error;

	/* Read battery voltage from fuelgauge */
	vbatt = pca9468_read_adc(pca9468, ADCCH_VBAT);
	if (vbatt < 0) {
		ret = vbatt;
		goto error;
	}

	/* Check VFLOAT method */
	if (pca9468->pdata->fg_vfloat == true) {
		/* Compare FG Vnow with VFLOAT threshold and then check STS_A */
		if (vbatt > pca9468->vfloat) {
			ret = CCMODE_VFLT_LOOP;
			pr_info("%s: FG Vnow=%d, FG Vfloat\n", __func__, vbatt);
		} else if (reg_val & PCA9468_BIT_VIN_UV_STS) {
			ret = CCMODE_VIN_UVLO;
		} else if (reg_val & PCA9468_BIT_CHG_LOOP_STS) {
			ret = CCMODE_CHG_LOOP;
		} else if (reg_val & PCA9468_BIT_VFLT_LOOP_STS) {
			ret = CCMODE_VFLT_LOOP;
		} else if (reg_val & PCA9468_BIT_IIN_LOOP_STS) {
			ret = CCMODE_IIN_LOOP;
		} else {
			ret = CCMODE_LOOP_INACTIVE;
		}
	} else {
		/* Check STS_A */
		if (reg_val & PCA9468_BIT_VIN_UV_STS) {
			ret = CCMODE_VIN_UVLO;
		} else if (reg_val & PCA9468_BIT_CHG_LOOP_STS) {
			ret = CCMODE_CHG_LOOP;
		} else if (reg_val & PCA9468_BIT_VFLT_LOOP_STS) {
			ret = CCMODE_VFLT_LOOP;
		} else if (reg_val & PCA9468_BIT_IIN_LOOP_STS) {
			ret = CCMODE_IIN_LOOP;
		} else {
			ret = CCMODE_LOOP_INACTIVE;
		}
	}

error:
	pr_info("%s: CCMODE Status=%d\n", __func__, ret);
	return ret;
}


/* Check CVMode Status */
static int pca9468_check_cvmode_status(struct pca9468_charger *pca9468)
{
	unsigned int val;
	int ret;
	int iin, vbatt;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval value = {0,};
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
	int mainbat_vol = 0;
	int subbat_vol = 0;
#endif
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_DUAL_BATTERY)
	psy_do_property("battery", get,
		POWER_SUPPLY_EXT_PROP_DIRECT_VBAT_CHECK, value);
	if (value.intval) {
		ret = CVMODE_CHG_DONE;
		pr_info("%s: CV MODE will be done by main or sub vbatt\n", __func__);
		goto error;
	}
#endif

	/* Read battery voltage from fuelgauge */
	vbatt = pca9468_read_adc(pca9468, ADCCH_VBAT);
	if (vbatt < 0) {
		ret = vbatt;
		goto error;
	}

	/* Read IIN ADC */
	iin = pca9468_read_adc(pca9468, ADCCH_IIN);
	if (iin < 0) {
		ret = iin;
		goto error;
	}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (pca9468->ta_type == TA_TYPE_USBPD_20) {
#if IS_ENABLED(CONFIG_DUAL_BATTERY)
		psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_VOLTAGE_PACK_MAIN, value);
		mainbat_vol = value.intval * PCA9468_SEC_DENOM_U_M;
		psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_VOLTAGE_PACK_SUB, value);
		subbat_vol = value.intval * PCA9468_SEC_DENOM_U_M;

		if (iin < pca9468->fpdo_dc_iin_topoff ||
				mainbat_vol >= pca9468->fpdo_dc_mainbat_topoff ||
				subbat_vol >= pca9468->fpdo_dc_subbat_topoff) {
			ret = CVMODE_CHG_DONE;
			pr_info("%s: FPDO_DC done, iin:%d, iin_topoff:%d\n", __func__,
					iin, pca9468->fpdo_dc_iin_topoff);
			pr_info("%s: fpdo_dc_mainbat_topoff=%d/%d, fpdo_dc_subbat_topoff=%d/%d\n", __func__,
				mainbat_vol, pca9468->fpdo_dc_mainbat_topoff,
				subbat_vol, pca9468->fpdo_dc_subbat_topoff);
			goto error;
		}
#else
		psy_do_property("battery", get, POWER_SUPPLY_PROP_VOLTAGE_NOW, value);

		if (value.intval >= pca9468->fpdo_dc_vnow_topoff || iin < pca9468->fpdo_dc_iin_topoff) {
			ret = CVMODE_CHG_DONE;
			pr_info("%s: FPDO_DC done, vnow:%d, vnow_topoff:%d, iin:%d, iin_topoff:%d\n",
					__func__, value.intval, pca9468->fpdo_dc_vnow_topoff,
					iin, pca9468->fpdo_dc_iin_topoff);
			goto error;
		}
#endif
	}
#endif

	/* Check IIN < Input Topoff current */
	if (iin < pca9468->iin_topoff) {
		pr_info("%s: charging done, iin=%d, iin_topoff=%d\n",
				__func__, iin, pca9468->iin_topoff);
		/* Direct Charging Done */
		ret = CVMODE_CHG_DONE;
	} else {
		/* It doesn't reach top-off condition yet */

		/* Read STS_A */
		ret = pca9468_read_reg(pca9468, PCA9468_REG_STS_A, &val);
		if (ret < 0)
			goto error;
		/* Check VFLOAT method */
		if (pca9468->pdata->fg_vfloat == true) {
			/* Compare FG Vnow with VFLOAT threshold and then check STS_A */
			if (vbatt > pca9468->vfloat) {
				ret = CVMODE_VFLT_LOOP;
				pr_info("%s: FG Vnow=%d, FG Vfloat\n", __func__, vbatt);
			} else if (val & PCA9468_BIT_CHG_LOOP_STS) {
				ret = CVMODE_CHG_LOOP;
			} else if (val & PCA9468_BIT_VFLT_LOOP_STS) {
				ret = CVMODE_VFLT_LOOP;
			} else if (val & PCA9468_BIT_IIN_LOOP_STS) {
				ret = CVMODE_IIN_LOOP;
			} else if (val & PCA9468_BIT_VIN_UV_STS) {
				ret = CVMODE_VIN_UVLO;
			} else {
				/* Any LOOP is inactive */
				ret = CVMODE_LOOP_INACTIVE;
			}
		} else {
			/* Check STS_A */
			if (val & PCA9468_BIT_CHG_LOOP_STS) {
				ret = CVMODE_CHG_LOOP;
			} else if (val & PCA9468_BIT_VFLT_LOOP_STS) {
				ret = CVMODE_VFLT_LOOP;
			} else if (val & PCA9468_BIT_IIN_LOOP_STS) {
				ret = CVMODE_IIN_LOOP;
			} else if (val & PCA9468_BIT_VIN_UV_STS) {
				ret = CVMODE_VIN_UVLO;
			} else {
				/* Any LOOP is inactive */
				ret = CVMODE_LOOP_INACTIVE;
			}
		}
	}

error:
	pr_info("%s: CVMODE Status=%d\n", __func__, ret);
	return ret;
}


/* 2:1 Direct Charging Adjust CC MODE control */
static int pca9468_charge_adjust_ccmode(struct pca9468_charger *pca9468)
{
	int iin, ccmode;
	int vbatt;
	int vin_vol, val;
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_ADJUST_CC);
#else
	pca9468->charging_state = DC_STATE_ADJUST_CC;
#endif

	ret = pca9468_check_error(pca9468);
	if (ret != 0)
		goto error;	// This is not active mode.

	ccmode = pca9468_check_ccmode_status(pca9468);
	if (ccmode < 0) {
		ret = ccmode;
		goto error;
	}

	switch (ccmode) {
	case CCMODE_IIN_LOOP:
	case CCMODE_CHG_LOOP:
		/* Read IIN_ADC */
		iin = pca9468_read_adc(pca9468, ADCCH_IIN);
		/* Read VBAT_ADC */
		vbatt = pca9468_read_adc(pca9468, ADCCH_VBAT);

		/* Check TA current */
		if ((pca9468->ta_cur > PCA9468_TA_MIN_CUR) &&
			(pca9468->ta_ctrl == TA_CTRL_CL_MODE)) {
			/* TA current is higher than 1.0A */
			/* Decrease TA current (50mA) */
			pca9468->ta_cur = pca9468->ta_cur - PD_MSG_TA_CUR_STEP;

			/* TA target voltage = TA voltage + (VFLOAT - VBAT_ADC)*2*chg_mode + 100mV */
			val = pca9468->ta_vol + (pca9468->vfloat - vbatt)*2*pca9468->chg_mode + 100000;
			val = val/PD_MSG_TA_VOL_STEP;
			pca9468->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
			if (pca9468->ta_target_vol > pca9468->ta_max_vol)
				pca9468->ta_target_vol = pca9468->ta_max_vol;
			/* End TA voltage and current adjustment */
			pr_info("%s: CC adjust End(LOOP): iin=%d, vbatt=%d, ta_cur=%d, ta_vol=%d, ta_target_vol=%d\n",
					__func__, iin, vbatt, pca9468->ta_cur, pca9468->ta_vol, pca9468->ta_target_vol);
			/* Clear TA increment flag */
			pca9468->prev_inc = INC_NONE;
			/* Go to Start CC mode */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_ENTER_CCMODE;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
		} else {
			/* Decrease TA voltage (20mV) */
			pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;

			/* Set TA target voltage to TA voltage */
			pca9468->ta_target_vol = pca9468->ta_vol;
			/* Clear TA increment flag */
			pca9468->prev_inc = INC_NONE;
			/* Send PD Message and then go to CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9468_set_charging_state(pca9468, DC_STATE_CC_MODE);
#else
			pca9468->charging_state = DC_STATE_CC_MODE;
#endif
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PDMSG_SEND;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);

			pr_info("%s: CC adjust End(LOOP): iin=%d, vbatt=%d, ta_cur=%d, ta_vol=%d, ta_ctrl=%d\n",
					__func__, iin, vbatt, pca9468->ta_cur, pca9468->ta_vol, pca9468->ta_ctrl);
		}
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		break;

	case CCMODE_VFLT_LOOP:
		/* Read IIN_ADC */
		iin = pca9468_read_adc(pca9468, ADCCH_IIN);
		/* Read VBAT_ADC */
		vbatt = pca9468_read_adc(pca9468, ADCCH_VBAT);
		pr_info("%s: CC adjust End(VFLOAT): vbatt=%d, iin=%d, ta_vol=%d\n",
				__func__, vbatt, iin, pca9468->ta_vol);

		/* Save TA target and voltage*/
		pca9468->ta_target_vol = pca9468->ta_vol;
		/* Clear TA increment flag */
		pca9468->prev_inc = INC_NONE;
		/* Go to Pre-CV mode */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_ENTER_CVMODE;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		break;

	case CCMODE_LOOP_INACTIVE:
		/* Read IIN_ADC */
		iin = pca9468_read_adc(pca9468, ADCCH_IIN);
		/* Read VBAT_ADC */
		vbatt = pca9468_read_adc(pca9468, ADCCH_VBAT);

		/* Check IIN ADC with IIN */
		/* IIN_ADC > IIN_CC -20mA ? */
		if (iin > (pca9468->iin_cc - PCA9468_IIN_ADC_OFFSET)) {
			if (pca9468->ta_ctrl == TA_CTRL_CL_MODE) {
				/* TA control method is CL mode */
				/* Input current is already over IIN_CC */
				/* End TA voltage and current adjustment */
				/* change charging state to Start CC mode */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
				pca9468_set_charging_state(pca9468, DC_STATE_START_CC);
#else
				pca9468->charging_state = DC_STATE_START_CC;
#endif
				pr_info("%s: CC adjust End(Normal): iin=%d, vbatt=%d, ta_vol=%d, ta_cur=%d\n",
						__func__, iin, vbatt, pca9468->ta_vol, pca9468->ta_cur);

				/* TA target voltage = TA voltage + (VFLOAT - VBAT_ADC)*2*chg_mode + 100mV */
				val = pca9468->ta_vol + (pca9468->vfloat - vbatt)*2*pca9468->chg_mode + 100000;
				val = val/PD_MSG_TA_VOL_STEP;
				pca9468->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
				if (pca9468->ta_target_vol > pca9468->ta_max_vol)
					pca9468->ta_target_vol = pca9468->ta_max_vol;
				pr_info("%s: CC adjust End: ta_target_vol=%d\n", __func__, pca9468->ta_target_vol);
				/* Clear TA increment flag */
				pca9468->prev_inc = INC_NONE;
				/* Go to Start CC mode */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_ENTER_CCMODE;
				pca9468->timer_period = 0;
				mutex_unlock(&pca9468->lock);
			} else {
				/* TA control method is CV mode */
				/* Input current is already over IIN_CC */
				/* End TA voltage and current adjustment */
				/* change charging state to CC mode */
				pr_info("%s: CC adjust End(CV): IIN_ADC=%d, ta_vol=%d, ta_cur=%d, ta_ctrl=%d\n",
						__func__, iin, pca9468->ta_vol, pca9468->ta_cur, pca9468->ta_ctrl);
				/* Save TA target voltage */
				pca9468->ta_target_vol = pca9468->ta_vol;
				/* Clear TA increment flag */
				pca9468->prev_inc = INC_NONE;
				/* Go to CC mode */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_CHECK_CCMODE;
				pca9468->timer_period = 0;
				mutex_unlock(&pca9468->lock);
			}
		} else {
			/* Check TA voltage */
			if (pca9468->ta_vol == pca9468->ta_max_vol) {
				/* Compare TA maximum current */
				if (pca9468->ta_cur == pca9468->ta_max_cur) {
					/* TA voltage is already max value */
					pr_info("%s: CC adjust End: MAX value, ta_vol=%d, ta_cur=%d\n",
						__func__, pca9468->ta_vol, pca9468->ta_cur);
					/* Clear TA increment flag */
					pca9468->prev_inc = INC_NONE;
					/* Save TA target voltage */
					pca9468->ta_target_vol = pca9468->ta_vol;
					/* Go to CC mode */
					mutex_lock(&pca9468->lock);
					pca9468->timer_id = TIMER_CHECK_CCMODE;
					pca9468->timer_period = 0;
					mutex_unlock(&pca9468->lock);
				} else {
					/* TA current is not maximum value */
					/* Increase TA current(50mA) */
					pca9468->ta_cur = pca9468->ta_cur + PD_MSG_TA_CUR_STEP;
					if (pca9468->ta_cur > pca9468->ta_max_cur)
						pca9468->ta_cur = pca9468->ta_max_cur;
					pr_info("%s: CC adjust Cont(1): iin=%d, ta_cur=%d\n",
							__func__, iin, pca9468->ta_cur);
					/* Set TA increment flag */
					pca9468->prev_inc = INC_TA_CUR;
					/* Send PD Message */
					mutex_lock(&pca9468->lock);
					pca9468->timer_id = TIMER_PDMSG_SEND;
					pca9468->timer_period = 0;
					mutex_unlock(&pca9468->lock);
				}
			} else {
				/* Check TA tolerance */
				/* The current input current compares the final input current(IIN_CC) with 100mA offset */
				/* PPS current tolerance has +/-150mA, so offset defined 100mA(tolerance +50mA) */
				if (iin < (pca9468->iin_cc - PCA9468_TA_IIN_OFFSET)) {
					/* TA voltage too low to enter TA CC mode, so we should increase TA voltage */
					pca9468->ta_vol = pca9468->ta_vol + PCA9468_TA_VOL_STEP_ADJ_CC*pca9468->chg_mode;
					if (pca9468->ta_vol > pca9468->ta_max_vol)
						pca9468->ta_vol = pca9468->ta_max_vol;
					pr_info("%s: CC adjust Cont(2): iin=%d, ta_vol=%d\n",
							__func__, iin, pca9468->ta_vol);
					/* Set TA increment flag */
					pca9468->prev_inc = INC_TA_VOL;
					/* Send PD Message */
					mutex_lock(&pca9468->lock);
					pca9468->timer_id = TIMER_PDMSG_SEND;
					pca9468->timer_period = 0;
					mutex_unlock(&pca9468->lock);
				} else {
					/* compare IIN ADC with previous IIN ADC + 20mA */
					if (iin > (pca9468->prev_iin + PCA9468_IIN_ADC_OFFSET)) {
						/* TA can supply more current if TA voltage is high */
						/* TA voltage too low to enter TA CC mode, so we should increase TA voltage */
						pca9468->ta_vol = pca9468->ta_vol + PCA9468_TA_VOL_STEP_ADJ_CC*pca9468->chg_mode;
						if (pca9468->ta_vol > pca9468->ta_max_vol)
							pca9468->ta_vol = pca9468->ta_max_vol;
						pr_info("%s: CC adjust Cont(3): iin=%d, ta_vol=%d\n",
								__func__, iin, pca9468->ta_vol);
						/* Set TA increment flag */
						pca9468->prev_inc = INC_TA_VOL;
						/* Send PD Message */
						mutex_lock(&pca9468->lock);
						pca9468->timer_id = TIMER_PDMSG_SEND;
						pca9468->timer_period = 0;
						mutex_unlock(&pca9468->lock);
					} else {
						/* Check the previous increment */
						if (pca9468->prev_inc == INC_TA_CUR) {
							/* The previous increment is TA current, but input current does not increase */
							/* Try to increase TA voltage(40mV) */
							pca9468->ta_vol = pca9468->ta_vol + PCA9468_TA_VOL_STEP_ADJ_CC*pca9468->chg_mode;
							if (pca9468->ta_vol > pca9468->ta_max_vol)
								pca9468->ta_vol = pca9468->ta_max_vol;
							pr_info("%s: CC adjust Cont(4): iin=%d, ta_vol=%d\n",
									__func__, iin, pca9468->ta_vol);
							/* Set TA increment flag */
							pca9468->prev_inc = INC_TA_VOL;
							/* Send PD Message */
							mutex_lock(&pca9468->lock);
							pca9468->timer_id = TIMER_PDMSG_SEND;
							pca9468->timer_period = 0;
							mutex_unlock(&pca9468->lock);
						} else {
							/* The previous increment is TA voltage, but input current does not increase */
							/* Try to increase TA current */
							/* Check APDO max current */
							if (pca9468->ta_cur == pca9468->ta_max_cur) {
								if (pca9468->ta_ctrl == TA_CTRL_CL_MODE) {
									/* TA control method is CL mode */
									/* TA current is maximum current */
									pr_info("%s: CC adjust End(MAX_CUR): iin=%d, ta_vol=%d, ta_cur=%d\n",
											__func__, iin, pca9468->ta_vol, pca9468->ta_cur);

									/* TA target voltage = TA voltage + (VFLOAT - VBAT_ADC)*2*chg_mode + 100mV */
									val = pca9468->ta_vol + (pca9468->vfloat - vbatt)*2*pca9468->chg_mode + 100000;
									val = val/PD_MSG_TA_VOL_STEP;
									pca9468->ta_target_vol = val*PD_MSG_TA_VOL_STEP;
									if (pca9468->ta_target_vol > pca9468->ta_max_vol)
										pca9468->ta_target_vol = pca9468->ta_max_vol;
									pr_info("%s: CC adjust End(MAX_CUR): ta_target_vol=%d\n", __func__, pca9468->ta_target_vol);
									/* Clear TA increment flag */
									pca9468->prev_inc = INC_NONE;
									/* Go to Start CC mode */
									mutex_lock(&pca9468->lock);
									pca9468->timer_id = TIMER_ENTER_CCMODE;
									pca9468->timer_period = 0;
									mutex_unlock(&pca9468->lock);
								} else {
									/* TA control method is CV mode */
									pr_info("%s: CC adjust End(MAX_CUR,CV): IIN_ADC=%d, ta_vol=%d, ta_cur=%d, ta_ctrl=%d\n",
											__func__, iin, pca9468->ta_vol, pca9468->ta_cur, pca9468->ta_ctrl);
									/* Save TA target voltage */
									pca9468->ta_target_vol = pca9468->ta_vol;
									/* Clear TA increment flag */
									pca9468->prev_inc = INC_NONE;
									/* Go to CC mode */
									mutex_lock(&pca9468->lock);
									pca9468->timer_id = TIMER_CHECK_CCMODE;
									pca9468->timer_period = 0;
									mutex_unlock(&pca9468->lock);
								}
							} else {
								/* Check the present TA current */
								/* Consider tolerance offset(100mA) */
								if (pca9468->ta_cur >= (pca9468->iin_cc + PCA9468_TA_IIN_OFFSET)) {
									/* TA voltage increment has high priority than TA current increment */
									/* Try to increase TA voltage(40mV) */
									pca9468->ta_vol = pca9468->ta_vol + PCA9468_TA_VOL_STEP_ADJ_CC*pca9468->chg_mode;
									if (pca9468->ta_vol > pca9468->ta_max_vol)
										pca9468->ta_vol = pca9468->ta_max_vol;
									pr_info("%s: CC adjust Cont(5): iin=%d, ta_vol=%d\n",
											__func__, iin, pca9468->ta_vol);
									/* Set TA increment flag */
									pca9468->prev_inc = INC_TA_VOL;
									/* Send PD Message */
									mutex_lock(&pca9468->lock);
									pca9468->timer_id = TIMER_PDMSG_SEND;
									pca9468->timer_period = 0;
									mutex_unlock(&pca9468->lock);
								} else {
									/* TA has tolerance and compensate it as real current */
									/* Increase TA current(50mA) */
									pca9468->ta_cur = pca9468->ta_cur + PD_MSG_TA_CUR_STEP;
									if (pca9468->ta_cur > pca9468->ta_max_cur)
										pca9468->ta_cur = pca9468->ta_max_cur;
									pr_info("%s: CC adjust Cont: ta_cur=%d\n", __func__, pca9468->ta_cur);
									/* Set TA increment flag */
									pca9468->prev_inc = INC_TA_CUR;
									/* Send PD Message */
									mutex_lock(&pca9468->lock);
									pca9468->timer_id = TIMER_PDMSG_SEND;
									pca9468->timer_period = 0;
									mutex_unlock(&pca9468->lock);
								}
							}
						}
					}
				}
			}
		}
		/* Save previous iin adc */
		pca9468->prev_iin = iin;
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		break;

	case CCMODE_VIN_UVLO:
		/* VIN UVLO - just notification , it works by hardware */
		vin_vol = pca9468_read_adc(pca9468, ADCCH_VIN);
		pr_info("%s: CC adjust VIN_UVLO: ta_vol=%d, vin_vol=%d\n", __func__, pca9468->ta_cur, vin_vol);
		/* Check VIN after 1sec */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_ADJUST_CCMODE;
		pca9468->timer_period = 1000;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
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
static int pca9468_charge_start_ccmode(struct pca9468_charger *pca9468)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_START_CC);
#else
	pca9468->charging_state = DC_STATE_START_CC;
#endif

	ret = pca9468_check_error(pca9468);
	if (ret < 0)
		goto error;

	/* Increase TA voltage */
	pca9468->ta_vol = pca9468->ta_vol + PCA9468_TA_VOL_STEP_PRE_CC * pca9468->chg_mode;
	/* Check TA target voltage */
	if (pca9468->ta_vol >= pca9468->ta_target_vol) {
		pca9468->ta_vol = pca9468->ta_target_vol;
		pr_info("%s: PreCC END: ta_vol=%d\n", __func__, pca9468->ta_vol);

		/* Change to DC state to CC mode */
		pca9468->charging_state = DC_STATE_CC_MODE;
	} else {
		pr_info("%s: PreCC Cont: ta_vol=%d\n", __func__, pca9468->ta_vol);
	}

	/* Send PD Message */
	mutex_lock(&pca9468->lock);
	pca9468->timer_id = TIMER_PDMSG_SEND;
	pca9468->timer_period = 0;
	mutex_unlock(&pca9468->lock);
	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging CC MODE control */
static int pca9468_charge_ccmode(struct pca9468_charger *pca9468)
{
	int ret = 0;
	int ccmode;
	int vin_vol, iin;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_CC_MODE);
#else
	pca9468->charging_state = DC_STATE_CC_MODE;
#endif

	ret = pca9468_check_error(pca9468);
	if (ret < 0) {
		if (ret == -EAGAIN) {
			/* DC error happens, but it is retry case */
			if (pca9468->ta_ctrl == TA_CTRL_CL_MODE) {
				/* Current TA control method is Current Limit mode */
				/* Retry DC as Constant Voltage mode */
				pr_info("%s: Retry DC : ta_ctrl=%d\n", __func__, pca9468->ta_ctrl);

				/* Disable charging */
				ret = pca9468_set_charging(pca9468, false);
				if (ret < 0)
					goto error;

				/* Set TA control method to Constant Voltage mode */
				pca9468->ta_ctrl = TA_CTRL_CV_MODE;

				/* Go to DC_STATE_PRESET_DC */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_PRESET_DC;
				pca9468->timer_period = 0;
				mutex_unlock(&pca9468->lock);
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
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

	/* Check new vfloat request, new iin request, or new dc mode request */
	if (pca9468->req_new_dc_mode == true) {
		ret = pca9468_set_new_dc_mode(pca9468);
	} else if (pca9468->req_new_iin == true) {
		ret = pca9468_set_new_iin(pca9468);
	} else if (pca9468->req_new_vfloat == true) {
		ret = pca9468_set_new_vfloat(pca9468);
	} else {
		ccmode = pca9468_check_ccmode_status(pca9468);
		if (ccmode < 0) {
			ret = ccmode;
			goto error;
		}

		switch (ccmode) {
		case CCMODE_LOOP_INACTIVE:
			/* Set input current compensation */
			/* Check the current TA current with TA_MIN_CUR */
			if ((pca9468->ta_cur <= PCA9468_TA_MIN_CUR) ||
				(pca9468->ta_ctrl == TA_CTRL_CV_MODE)) {
				/* Need input voltage compensation */
				ret = pca9468_set_ta_voltage_comp(pca9468);
			} else {
				if (pca9468->ta_max_vol >= PCA9468_TA_MAX_VOL_CP) {
					/* This TA can support the input current without power limit */
					/* Need input current compensation */
					ret = pca9468_set_ta_current_comp(pca9468);
				} else {
					/* This TA has the power limitation for the input current compenstaion */
					/* The input current cannot increase over the constant power */
					/* Need input current compensation in constant power mode */
					ret = pca9468_set_ta_current_comp2(pca9468);
				}
			}
			pr_info("%s: CC INACTIVE: ta_cur=%d, ta_vol=%d\n", __func__, pca9468->ta_cur, pca9468->ta_vol);
			break;

		case CCMODE_VFLT_LOOP:
			/* Read IIN_ADC */
			iin = pca9468_read_adc(pca9468, ADCCH_IIN);
			pr_info("%s: CC VFLOAT: iin=%d\n", __func__, iin);
			/* go to Pre-CV mode */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_ENTER_CVMODE;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		case CCMODE_IIN_LOOP:
		case CCMODE_CHG_LOOP:
			/* Read IIN_ADC */
			iin = pca9468_read_adc(pca9468, ADCCH_IIN);
			/* Check the current TA current with TA_MIN_CUR */
			if ((pca9468->ta_cur <= PCA9468_TA_MIN_CUR) ||
				(pca9468->ta_ctrl == TA_CTRL_CV_MODE)) {
				/* Decrease TA voltage (20mV) */
				pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
				pr_info("%s: CC LOOP(1):iin=%d, next_ta_vol=%d\n", __func__, iin, pca9468->ta_vol);
			} else {
				/* Check TA current and compare it with IIN_CC */
				if (pca9468->ta_cur <= pca9468->iin_cc - PCA9468_TA_CUR_LOW_OFFSET) {
					/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
					/* TA has abnormal behavior */
					/* Decrease TA voltage (20mV) */
					pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
					pr_info("%s: CC LOOP(2):iin=%d, ta_cur=%d, next_ta_vol=%d\n",
							__func__, iin, pca9468->ta_cur, pca9468->ta_vol);
					/* Update TA target voltage */
					pca9468->ta_target_vol = pca9468->ta_vol;
				} else {
					/* TA current is higher than IIN_CC - 200mA */
					/* Decrease TA current first to reduce input current */
					/* Decrease TA current (50mA) */
					pca9468->ta_cur = pca9468->ta_cur - PD_MSG_TA_CUR_STEP;
					pr_info("%s: CC LOOP(3):iin=%d, ta_vol=%d, next_ta_cur=%d\n",
							__func__, iin, pca9468->ta_vol, pca9468->ta_cur);
				}
			}
			/* Send PD Message */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PDMSG_SEND;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		case CCMODE_VIN_UVLO:
			/* VIN UVLO - just notification , it works by hardware */
			vin_vol = pca9468_read_adc(pca9468, ADCCH_VIN);
			pr_info("%s: CC VIN_UVLO: ta_vol=%d, vin_vol=%d\n", __func__, pca9468->ta_cur, vin_vol);
			/* Check VIN after 1sec */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_CCMODE;
			pca9468->timer_period = 1000;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
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
static int pca9468_charge_start_cvmode(struct pca9468_charger *pca9468)
{
	int ret = 0;
	int cvmode = 0;
	int vin_vol;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_START_CV);
#else
	pca9468->charging_state = DC_STATE_START_CV;
#endif

	ret = pca9468_check_error(pca9468);
	if (ret != 0) {
		/* Check error type */
		if (ret == -ERROR_DCRCP) {
			/* Set cvmode to CVMODE_CHG_DONE */
			cvmode = CVMODE_CHG_DONE;
			pr_info("%s: cvmode is CVMODE_CHG_DONE by RCP\n", __func__);
		} else {
			goto error;	// This is not active mode.
		}
	}

	/* Check the status */
	if (cvmode != CVMODE_CHG_DONE) {
		cvmode = pca9468_check_cvmode_status(pca9468);
		if (cvmode < 0) {
			ret = cvmode;
			goto error;
		}
	}

	switch (cvmode) {
	case CVMODE_CHG_DONE:
		/* Charging Done */
		/* Keep CV mode until battery driver send stop charging */

		/* Need to implement notification function */
		/* To do here */

		pr_info("%s: Start CV Done\n", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9468_set_done(pca9468, true);
#endif

		/* Check the charging status after notification function */
		if (pca9468->charging_state != DC_STATE_NO_CHARGING) {
			/* Notification function does not stop timer work yet */
			/* Keep the charging done state */
			/* Set timer */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_CVMODE;
			pca9468->timer_period = PCA9468_CVMODE_CHECK_T;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		} else {
			/* Already called stop charging by notification function */
			pr_info("%s: Already stop DC\n", __func__);
		}
		break;

	case CVMODE_CHG_LOOP:
	case CVMODE_IIN_LOOP:
		/* Check TA current */
		if (pca9468->ta_cur > PCA9468_TA_MIN_CUR) {
			/* TA current is higher than 1.0A */
			/* Decrease TA current (50mA) */
			pca9468->ta_cur = pca9468->ta_cur - PD_MSG_TA_CUR_STEP;
			pr_info("%s: PreCV Cont: ta_cur=%d\n", __func__, pca9468->ta_cur);
		} else {
			/* TA current is less than 1.0A */
			/* Decrease TA voltage (20mV) */
			pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
			pr_info("%s: PreCV Cont(IIN_LOOP): ta_vol=%d\n", __func__, pca9468->ta_vol);
		}
		/* Send PD Message */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PDMSG_SEND;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		break;

	case CVMODE_VFLT_LOOP:
		/* Check present vfloat */
		if (pca9468->vfloat >= pca9468->max_vfloat) {
			/* Decrease TA voltage (40mV) */
			pca9468->ta_vol = pca9468->ta_vol - PCA9468_TA_VOL_STEP_PRE_CV * pca9468->chg_mode;
			pr_info("%s: PreCV Cont(40mV): ta_vol=%d\n", __func__, pca9468->ta_vol);
		} else {
			/* Decrease TA voltage (20mV) */
			pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP * pca9468->chg_mode;
			pr_info("%s: PreCV Cont(20mV): ta_vol=%d\n", __func__, pca9468->ta_vol);
		}
		/* Send PD Message */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PDMSG_SEND;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		break;

	case CVMODE_LOOP_INACTIVE:
		/* Exit Pre CV mode */
		pr_info("%s: PreCV End: ta_vol=%d, ta_cur=%d\n", __func__, pca9468->ta_vol, pca9468->ta_cur);

		/* Set TA target voltage to TA voltage */
		pca9468->ta_target_vol = pca9468->ta_vol;

		/* Need to implement notification to other driver */
		/* To do here */

		/* Go to CV mode */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_CHECK_CVMODE;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		break;

	case CVMODE_VIN_UVLO:
		/* VIN UVLO - just notification , it works by hardware */
		vin_vol = pca9468_read_adc(pca9468, ADCCH_VIN);
		pr_info("%s: PreCV VIN_UVLO: ta_vol=%d, vin_vol=%d\n", __func__, pca9468->ta_cur, vin_vol);
		/* Check VIN after 1sec */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_ENTER_CVMODE;
		pca9468->timer_period = 1000;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		break;

	default:
		break;
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging CV MODE control */
static int pca9468_charge_cvmode(struct pca9468_charger *pca9468)
{
	int ret = 0;
	int cvmode;
	int vin_vol;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_CV_MODE);
#else
	pca9468->charging_state = DC_STATE_CV_MODE;
#endif

	ret = pca9468_check_error(pca9468);
	if (ret != 0) {
		/* Check error type */
		if (ret == -ERROR_DCRCP) {
			/* Set cvmode to CVMODE_CHG_DONE */
			cvmode = CVMODE_CHG_DONE;
			pr_info("%s: cvmode is CVMODE_CHG_DONE by RCP\n", __func__);
		} else {
			goto error;	// This is not active mode.
		}
	}

	/* Check new vfloat request, new iin request, or new dc mode request */
	if (pca9468->req_new_dc_mode == true) {
		ret = pca9468_set_new_dc_mode(pca9468);
	} else if (pca9468->req_new_iin == true) {
		ret = pca9468_set_new_iin(pca9468);
	} else if (pca9468->req_new_vfloat == true) {
		ret = pca9468_set_new_vfloat(pca9468);
	} else {
		/* Check the status */
		if (cvmode != CVMODE_CHG_DONE) {
			cvmode = pca9468_check_cvmode_status(pca9468);
			if (cvmode < 0) {
				ret = cvmode;
				goto error;
			}
		}

		switch (cvmode) {
		case CVMODE_CHG_DONE:
			/* Charging Done */
			/* Keep CV mode until battery driver send stop charging */

			/* Need to implement notification function */
			/* To do here */

			pr_info("%s: CV Done\n", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9468_set_done(pca9468, true);
#endif

			/* Check the charging status after notification function */
			if (pca9468->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the charging done state */
				/* Set timer */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_CHECK_CVMODE;
				/* Add to check charging step and set the polling time */
				if (pca9468->vfloat < pca9468->pdata->step1_vth) {
					/* Step1 charging - polling time is cv_polling */
					pca9468->timer_period = pca9468->pdata->cv_polling;
				} else if ((pca9468->dec_vfloat == true) || (pca9468->vfloat >= pca9468->max_vfloat)) {
					/* present vfloat is lower than previous vfloat or present vfloat is maximum vfloat */
					/* pollig time is CVMODE_CHECK2_T */
					pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
				} else {
					/* Step2 or 3 charging - polling time is PCA9468_CVMODE_CHECK_T */
					pca9468->timer_period = PCA9468_CVMODE_CHECK_T;
				}
				mutex_unlock(&pca9468->lock);
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case CVMODE_CHG_LOOP:
		case CVMODE_IIN_LOOP:
			/* Check TA current */
			if (pca9468->ta_cur > PCA9468_TA_MIN_CUR) {
				/* TA current is higher than (1.0A*TA_mode) */

				/* Check TA current and compare it with IIN_CC */
				if (pca9468->ta_cur <= pca9468->iin_cc - PCA9468_TA_CUR_LOW_OFFSET) {
					/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
					/* TA has abnormal behavior */
					/* Decrease TA voltage (20mV) */
					pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
					pr_info("%s: CV LOOP, Cont1: ta_vol=%d\n", __func__, pca9468->ta_vol);
					/* Update TA target voltage */
					pca9468->ta_target_vol = pca9468->ta_vol;
				} else {
					/* TA current is higher than IIN_CC - 200mA */
					/* Decrease TA current first to reduce input current */
					/* Decrease TA current (50mA) */
					pca9468->ta_cur = pca9468->ta_cur - PD_MSG_TA_CUR_STEP;
					pr_info("%s: CV LOOP, Cont2: ta_cur=%d\n", __func__, pca9468->ta_cur);
				}
			} else {
				/* TA current is less than (1.0A*TA_mode) */
				/* Decrease TA Voltage (20mV) */
				pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
				pr_info("%s: CV LOOP, Cont3: ta_vol=%d\n", __func__, pca9468->ta_vol);
			}
			/* Send PD Message */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PDMSG_SEND;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		case CVMODE_VFLT_LOOP:
			/* Decrease TA voltage */
			pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
			pr_info("%s: CV VFLOAT, Cont: ta_vol=%d\n", __func__, pca9468->ta_vol);

			/* Set TA target voltage to TA voltage */
			pca9468->ta_target_vol = pca9468->ta_vol;

			/* Send PD Message */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PDMSG_SEND;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		case CVMODE_LOOP_INACTIVE:
			/* Set timer */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_CVMODE;
			/* Add to check charging step and set the polling time */
			if (pca9468->vfloat < pca9468->pdata->step1_vth) {
				/* Step1 charging - polling time is cv_polling */
				pca9468->timer_period = pca9468->pdata->cv_polling;
			} else if ((pca9468->dec_vfloat == true) || (pca9468->vfloat >= pca9468->max_vfloat)) {
				/* present vfloat is lower than previous vfloat or present vfloat is maximum vfloat */
				/* pollig time is CVMODE_CHECK2_T */
				pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
			} else {
				/* Step2 or 3 charging - polling time is PCA9468_CVMODE_CHECK_T */
				pca9468->timer_period = PCA9468_CVMODE_CHECK_T;
			}
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		case CVMODE_VIN_UVLO:
			/* VIN UVLO - just notification , it works by hardware */
			vin_vol = pca9468_read_adc(pca9468, ADCCH_VIN);
			pr_info("%s: CC VIN_UVLO: ta_vol=%d, vin_vol=%d\n", __func__, pca9468->ta_cur, vin_vol);
			/* Check VIN after 1sec */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_CVMODE;
			pca9468->timer_period = 1000;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		default:
			break;
		}
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging WC CV MODE control */
static int pca9468_charge_wc_cvmode(struct pca9468_charger *pca9468)
{
	int ret = 0;
	int cvmode;
	int vin_vol;

	pr_info("%s: ======START=======\n", __func__);

	pca9468->charging_state = DC_STATE_WC_CV_MODE;

	ret = pca9468_check_error(pca9468);
	if (ret != 0)
		goto error;	// This is not active mode.

	/* Check new vfloat request and new iin request */
	if (pca9468->req_new_iin == true) {
		ret = pca9468_set_new_iin(pca9468);
	} else if (pca9468->req_new_vfloat == true) {
		ret = pca9468_set_new_vfloat(pca9468);
	} else {
		cvmode = pca9468_check_cvmode_status(pca9468);
		if (cvmode < 0) {
			ret = cvmode;
			goto error;
		}

		switch (cvmode) {
		case CVMODE_CHG_DONE:
			/* Charging Done */
			/* Keep WC CV mode until battery driver send stop charging */

			/* Need to implement notification function */
			/* To do here */

			pr_info("%s: WC CV Done\n", __func__);

			/* Check the charging status after notification function */
			if (pca9468->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the charging done state */
				/* Set timer */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_CHECK_WCCVMODE;
				pca9468->timer_period = PCA9468_CVMODE_CHECK_T;
				mutex_unlock(&pca9468->lock);
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case CVMODE_CHG_LOOP:
		case CVMODE_IIN_LOOP:
			/* IIN_LOOP happens */
			pr_info("%s: WC CV IIN_LOOP\n", __func__);

			/* Need to control WC RX voltage or current */

			/* Need to implement notification function */

			/* To do here */

			/* Set timer - 1s */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_WCCVMODE;
			pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		case CVMODE_VFLT_LOOP:
			/* VFLOAT_LOOP happens */
			pr_info("%s: WC CV VFLOAT_LOOP\n", __func__);

			/* Need to control WC RX voltage */

			/* Need to implement notification function */

			/* To do here */

			/* Set timer - 1s */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_WCCVMODE;
			pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		case CVMODE_LOOP_INACTIVE:
			pr_info("%s: WC CV INACTIVE\n", __func__);
			/* Set timer - 10s */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_WCCVMODE;
			pca9468->timer_period = PCA9468_CVMODE_CHECK_T;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		case CVMODE_VIN_UVLO:
			/* VIN UVLO - just notification , it works by hardware */
			vin_vol = pca9468_read_adc(pca9468, ADCCH_VIN);
			pr_info("%s: CV VIN_UVLO: vin_vol=%d\n", __func__, vin_vol);
			/* Check VIN after 1sec */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_WCCVMODE;
			pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			break;

		default:
			break;
		}
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* 2:1 Direct Charging FPDO CV MODE control */
static int pca9468_charge_fpdo_cvmode(struct pca9468_charger *pca9468)
{
	int ret = 0;
	int cvmode;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_FPDO_CV_MODE);
#else
	pca9468->charging_state = DC_STATE_FPDO_CV_MODE;
#endif

	ret = pca9468_check_error(pca9468);
	if (ret != 0)
		goto error; // This is not active mode.

	/* Check new request */
	if (pca9468->req_new_iin == true) {
		/* Set IIN_CC to new iin */
		pca9468->iin_cc = pca9468->new_iin;
		/* Update iin_cfg */
		pca9468->iin_cfg = pca9468->iin_cc;
		/* Set IIN_CFG to new IIN */
		ret = pca9468_set_input_current(pca9468, pca9468->iin_cc);
		if (ret < 0)
			goto error;

		/* Clear req_new_iin */
		mutex_lock(&pca9468->lock);
		pca9468->req_new_iin = false;
		mutex_unlock(&pca9468->lock);

		/* Set timer - 1s */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_CHECK_FPDOCVMODE;
		pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
	} else if (pca9468->req_new_vfloat == true) {
		/* Set VFLOAT to new vfloat */
		pca9468->vfloat = pca9468->new_vfloat;
		ret = pca9468_set_vfloat(pca9468, pca9468->vfloat);
		if (ret < 0)
			goto error;

		/* Clear req_new_vfloat */
		mutex_lock(&pca9468->lock);
		pca9468->req_new_vfloat = false;
		mutex_unlock(&pca9468->lock);

		/* Set timer - 1s */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_CHECK_FPDOCVMODE;
		pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
	} else {
		cvmode = pca9468_check_cvmode_status(pca9468);
		if (cvmode < 0) {
			ret = cvmode;
			goto error;
		}

		if (cvmode == CVMODE_CHG_DONE) {
			/* Check charging done counter */
			if (pca9468->done_cnt < FPDO_DONE_CNT) {
				/* Keep cvmode status as CVMODE_LOOP_INACTIVE */
				cvmode = CVMODE_LOOP_INACTIVE;
				pr_info("%s: Keep FPDO CVMODE Status=%d\n", __func__, cvmode);
				/* Increase charging done counter */
				pca9468->done_cnt++;
			} else {
				/* Change cvmode status to charging done */
				cvmode = CVMODE_CHG_DONE;
				pr_info("%s: FPDO_CVMODE Status=%d\n", __func__, cvmode);
				/* Clear charging done counter */
				pca9468->done_cnt = 0;
			}
		} else {
			/* Clear charging done counter */
			pca9468->done_cnt = 0;
		}

		switch (cvmode) {
		case CVMODE_CHG_DONE:
			/* Charging Done */
			/* Keep FPDO CV mode until battery driver send stop charging */
			pca9468->charging_state = DC_STATE_CHARGING_DONE;
			/* Need to implement notification function */
			/* A customer should insert code */

			pr_info("%s: FPDO CV Done\n", __func__);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9468_set_done(pca9468, true);
#endif
			/* Check the charging status after notification function */
			if (pca9468->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the charging done state */
				/* Set timer */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_CHECK_FPDOCVMODE;
				pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
				mutex_unlock(&pca9468->lock);
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case CVMODE_CHG_LOOP:
		case CVMODE_IIN_LOOP:
			/* IIN_LOOP happens */
			pr_info("%s: FPDO CV IIN_LOOP\n", __func__);
			/* Need to stop DC by battery driver */

			/* Need to implement notification function */
			/* A customer should insert code */
			/* To do here */

			/* Check the charging status after notification function */
			if (pca9468->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the current state */
				/* Set timer - 1s */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_CHECK_FPDOCVMODE;
				pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
				mutex_unlock(&pca9468->lock);
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case CVMODE_VFLT_LOOP:
			/* VFLOAT_LOOP happens */
			pr_info("%s: FPDO CV VFLOAT_LOOP\n", __func__);
			/* Need to stop DC and transit to switching charger by battery driver */

			/* Need to implement notification function */
			/* A customer should insert code */
			/* To do here */

			/* Check the charging status after notification function */
			if (pca9468->charging_state != DC_STATE_NO_CHARGING) {
				/* Notification function does not stop timer work yet */
				/* Keep the current state */
				/* Set timer */
				mutex_lock(&pca9468->lock);
				pca9468->timer_id = TIMER_CHECK_FPDOCVMODE;
				pca9468->timer_period = PCA9468_CVMODE_CHECK2_T;
				mutex_unlock(&pca9468->lock);
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			} else {
				/* Already called stop charging by notification function */
				pr_info("%s: Already stop DC\n", __func__);
			}
			break;

		case CVMODE_LOOP_INACTIVE:
			/* Set timer */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_FPDOCVMODE;
			pca9468->timer_period = PCA9468_CVMODE_CHECK3_T;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
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
static int pca9468_charge_bypass_mode(struct pca9468_charger *pca9468)
{
	int ret = 0;
	int ccmode;
	int vbat, iin;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_BYPASS_MODE);
#else
	pca9468->charging_state = DC_STATE_BYPASS_MODE;
#endif

	ret = pca9468_check_error(pca9468);
	if (ret != 0)
		goto error;	// This is not active mode.

	/* Check new request */
	if (pca9468->req_new_dc_mode == true) {
		ret = pca9468_set_new_dc_mode(pca9468);
	} else if (pca9468->req_new_iin == true) {
		ret = pca9468_set_new_iin(pca9468);
	} else if (pca9468->req_new_vfloat == true) {
		ret = pca9468_set_new_vfloat(pca9468);
	} else {
		ccmode = pca9468_check_ccmode_status(pca9468);
		if (ccmode < 0) {
			ret = ccmode;
			goto error;
		}

		/* Read IIN ADC */
		iin = pca9468_read_adc(pca9468, ADCCH_IIN);
		/* Read VBAT ADC */
		vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);

		pr_info("%s: iin=%d, vbat=%d\n", __func__, iin, vbat);

		if (ccmode == CCMODE_IIN_LOOP) {
			/* Decrease input current */
			/* Check TA current and compare it with IIN_CC */
			if (pca9468->ta_cur <= pca9468->iin_cc - PCA9468_TA_CUR_LOW_OFFSET) {
				/* IIN_LOOP still happens even though TA current is less than IIN_CC - 200mA */
				/* TA has abnormal behavior */
				/* Decrease TA voltage (20mV) */
				pca9468->ta_vol = pca9468->ta_vol - PD_MSG_TA_VOL_STEP;
				pr_info("%s: CC LOOP:iin=%d, ta_cur=%d, next_ta_vol=%d\n",
						__func__, iin, pca9468->ta_cur, pca9468->ta_vol);
			} else {
				/* TA current is higher than IIN_CC - 200mA */
				/* Decrease TA current first to reduce input current */
				/* Decrease TA current (50mA) */
				pca9468->ta_cur = pca9468->ta_cur - PD_MSG_TA_CUR_STEP;
				pr_info("%s: CC LOOP:iin=%d, next_ta_cur=%d\n", __func__, iin, pca9468->ta_cur);
			}

			/* Send PD Message */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PDMSG_SEND;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		} else {
			/* Ignore other status */
			/* Keep Bypass mode */
			pr_info("%s: Bypass mode, status=%d, ta_cur=%d, ta_vol=%d\n",
					__func__, ccmode, pca9468->ta_cur, pca9468->ta_vol);
			/* Set timer - 10s */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_BYPASSMODE;
			pca9468->timer_period = PCA9468_BYPMODE_CHECK_T;	/* 10s */
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		}
	}

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Direct Charging DC mode Change Control */
static int pca9468_charge_dcmode_change(struct pca9468_charger *pca9468)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_DCMODE_CHANGE);
#else
	pca9468->charging_state = DC_STATE_DCMODE_CHANGE;
#endif

	ret = pca9468_set_new_dc_mode(pca9468);

	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Preset TA voltage and current for Direct Charging Mode */
static int pca9468_preset_dcmode(struct pca9468_charger *pca9468)
{
	int vbat;
	unsigned int val;
	int ret = 0;
	int chg_mode;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_PRESET_DC);
#else
	pca9468->charging_state = DC_STATE_PRESET_DC;
#endif

	/* Read VBAT ADC */
	vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);
	if (vbat < 0) {
		ret = vbat;
		goto error;
	}

	/* Compare VBAT with VBAT ADC */
	if (vbat > pca9468->vfloat) {
		/* Warn "Invalid battery voltage to start direct charging" */
		pr_err("%s: Warning - vbat adc(%duV) is higher than VFLOAT(%duV)\n",
					__func__, vbat, pca9468->vfloat);
	}

	/* Check TA type */
	if (pca9468->ta_type == TA_TYPE_USBPD_20) {
		/* TA type is USBPD 2.0 and support only FPDO */
		pr_info("%s: ta type : fixed PDO\n", __func__);

		/* Set OV_DELTA to 40% */
		val = OV_DELTA_40P << MASK2SHIFT(PCA9468_BIT_OV_DELTA);
		ret = pca9468_update_reg(pca9468, PCA9468_REG_SAFETY_CTRL,
								PCA9468_BIT_OV_DELTA, val);
		if (ret < 0)
			goto error;

		/* Set PDO object position to 9V FPDO */
		pca9468->ta_objpos = 2;
		/* Set TA voltage to 9V */
		pca9468->ta_vol = 9000000;
		/* Set TA maximum voltage to 9V */
		pca9468->ta_max_vol = 9000000;
		/* Set IIN_CC to iin_cfg */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9468->iin_cc = pca9468->pdata->fpdo_dc_iin_lowest_limit;
#else
		pca9468->iin_cc = pca9468->iin_cfg;
#endif
		/* Set TA operating current and maximum current to iin_cc */
		pca9468->ta_cur = pca9468->iin_cc;
		pca9468->ta_max_cur = pca9468->iin_cc;
		/* Calculate TA maximum power */
		pca9468->ta_max_pwr = (pca9468->ta_max_vol/DENOM_U_M)*(pca9468->ta_max_cur/DENOM_U_M);

		pr_info("%s: Preset DC(FPDO), ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d\n",
			__func__, pca9468->ta_max_vol, pca9468->ta_max_cur, pca9468->ta_max_pwr, pca9468->iin_cc);
	} else {
		/* Check the TA mode for wireless charging */
		if (pca9468->chg_mode == WC_DC_MODE) {
			/* Wireless Charger DC mode */
			/* Set IIN_CC to IIN */
			pca9468->iin_cc = pca9468->pdata->iin_cfg;
			/* Go to preset config */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PRESET_CONFIG;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			goto error;
		}

		/* Set the TA max current to input request current(iin_cfg) initially
			 to get TA maximum current from PD IC */
		pca9468->ta_max_cur = pca9468->iin_cfg;
		/* Set the TA max voltage to enough high value to find TA maximum voltage initially */
		pca9468->ta_max_vol = pca9468->pdata->ta_max_vol * pca9468->pdata->chg_mode;
		/* Search the proper object position of PDO */
		pca9468->ta_objpos = 0;
		/* Get the APDO max current/voltage(TA_MAX_CUR/VOL) */
		ret = pca9468_get_apdo_max_power(pca9468);
		if (ret < 0) {
			/* TA does not have the desired APDO */
			/* Check the desired mode */
			if (pca9468->pdata->chg_mode == CHG_4TO1_DC_MODE) {
				/* TA doesn't have any APDO to support 4:1 mode */
				/* Get the APDO max current/voltage with 2:1 mode */
				pca9468->ta_max_vol = pca9468->pdata->ta_max_vol;
				pca9468->ta_objpos = 0;
				ret = pca9468_get_apdo_max_power(pca9468);
				if (ret < 0) {
					pr_err("%s: TA doesn't have any APDO to support 2:1 or 4:1\n", __func__);
					pca9468->chg_mode = CHG_NO_DC_MODE;
					goto error;
				} else {
					/* TA has APDO to support 2:1 mode */
					pca9468->chg_mode = CHG_2TO1_DC_MODE;
				}
			} else {
				/* The desired TA mode is 2:1 mode */
				/* TA doesn't have any APDO to support 2:1 mode*/
				pr_err("%s: TA doesn't have any APDO to support 2:1\n", __func__);
				pca9468->chg_mode = CHG_NO_DC_MODE;
				goto error;
			}
		} else {
			/* TA has the desired APDO */
			pca9468->chg_mode = pca9468->pdata->chg_mode;
		}

		chg_mode = pca9468->chg_mode;

		/* Calculate new TA maximum current and voltage that used in the direct charging */
		/* Set IIN_CC to MIN[IIN, (TA_MAX_CUR by APDO)*chg_mode]*/
		pca9468->iin_cc = MIN(pca9468->iin_cfg, (pca9468->ta_max_cur*chg_mode));
		/* Set the current IIN_CC to iin_cfg for recovering it after resolution adjustment */
		pca9468->iin_cfg = pca9468->iin_cc;
		/* Calculate new TA max voltage */
		/* Adjust IIN_CC with APDO resolution(50mA) - It will recover to the original value after max voltage calculation */
		val = pca9468->iin_cc/PD_MSG_TA_CUR_STEP;
		pca9468->iin_cc = val*PD_MSG_TA_CUR_STEP;
		/* Set TA_MAX_VOL to MIN[PCA9468_TA_MAX_VOL*TA_mode, TA_MAX_PWR/(IIN_CC/TA_mode)] */
		val = pca9468->ta_max_pwr/(pca9468->iin_cc/chg_mode/1000);	/* mV */
		val = val*1000/PD_MSG_TA_VOL_STEP;	/* Adjust values with APDO resolution(20mV) */
		val = val*PD_MSG_TA_VOL_STEP; /* uV */
		pca9468->ta_max_vol = MIN(val, pca9468->pdata->ta_max_vol*chg_mode);

		/* Set TA voltage to MAX[TA_MIN_VOL_PRESET*TA_mode, (2*VBAT_ADC*TA_mode + offset)] */
		pca9468->ta_vol = max(PCA9468_TA_MIN_VOL_PRESET*chg_mode, (2*vbat*chg_mode + PCA9468_TA_VOL_PRE_OFFSET));
		val = pca9468->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
		pca9468->ta_vol = val*PD_MSG_TA_VOL_STEP;
		/* Set TA voltage to MIN[TA voltage, TA_MAX_VOL*TA_mode] */
		pca9468->ta_vol = MIN(pca9468->ta_vol, pca9468->ta_max_vol*chg_mode);
		/* Set the initial TA current to IIN_CC/TA_mode */
		pca9468->ta_cur = pca9468->iin_cc/chg_mode;
		/* Recover IIN_CC to the original value(iin_cfg) */
		pca9468->iin_cc = pca9468->iin_cfg;

		pr_info("%s: Preset DC, ta_max_vol=%d, ta_max_cur=%d, ta_max_pwr=%d, iin_cc=%d, chg_mode=%d\n",
			__func__, pca9468->ta_max_vol, pca9468->ta_max_cur, pca9468->ta_max_pwr, pca9468->iin_cc, pca9468->chg_mode);

		pr_info("%s: Preset DC, ta_vol=%d, ta_cur=%d\n",
			__func__, pca9468->ta_vol, pca9468->ta_cur);
	}

	/* Send PD Message */
	mutex_lock(&pca9468->lock);
	pca9468->timer_id = TIMER_PDMSG_SEND;
	pca9468->timer_period = 0;
	mutex_unlock(&pca9468->lock);
	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* Preset direct charging configuration */
static int pca9468_preset_config(struct pca9468_charger *pca9468)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_PRESET_DC);
#else
	pca9468->charging_state = DC_STATE_PRESET_DC;
#endif

	/* Set IIN_CFG to IIN_CC */
	ret = pca9468_set_input_current(pca9468, pca9468->iin_cc);
	if (ret < 0)
		goto error;

	/* Set ICHG_CFG to enough high value */
	ret = pca9468_set_charging_current(pca9468, pca9468->pdata->ichg_cfg);
	if (ret < 0)
		goto error;

	/* Check TA type */
	if (pca9468->ta_type == TA_TYPE_USBPD_20) {
		/* Set VFLOAT to VBAT */
		ret = pca9468_set_vfloat(pca9468, pca9468->vfloat);
		if (ret < 0)
			goto error;
		/* Set switching frequency */
		pca9468->fsw_cfg = pca9468->pdata->fsw_cfg_fpdo;
	} else {
		/* Check VFLOAT method */
		if (pca9468->pdata->fg_vfloat == true) {
			/* Set PCA9468 VFLOAT to default value */
			ret = pca9468_set_vfloat(pca9468, pca9468->pdata->dft_vfloat);
		} else {
			/* Set PCA9468 VFLOAT to new vfloat */
			ret = pca9468_set_vfloat(pca9468, pca9468->vfloat);
		}
		if (ret < 0)
			goto error;

		/* Set switching frequency */
		if (pca9468->iin_cc > pca9468->pdata->iin_low_freq)
			pca9468->fsw_cfg = pca9468->pdata->fsw_cfg;
		else
			pca9468->fsw_cfg = pca9468->pdata->fsw_cfg_low;
	}
	ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
							PCA9468_BIT_FSW_CFG, pca9468->fsw_cfg);
	if (ret < 0)
		goto error;
	pr_info("%s: sw_freq=%dkHz\n", __func__, sw_freq[pca9468->fsw_cfg]);

	/* Enable PCA9468 */
	ret = pca9468_set_charging(pca9468, true);
	if (ret < 0)
		goto error;

	/* Clear previous iin adc */
	pca9468->prev_iin = 0;

	/* Clear TA increment flag */
	pca9468->prev_inc = INC_NONE;

	/* Go to CHECK_ACTIVE state after 150ms*/
	mutex_lock(&pca9468->lock);
	pca9468->timer_id = TIMER_CHECK_ACTIVE;
	pca9468->timer_period = PCA9468_ENABLE_DELAY_T;
	mutex_unlock(&pca9468->lock);
	schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
	ret = 0;

error:
	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}

/* Check the charging status before entering the adjust cc mode */
static int pca9468_check_active_state(struct pca9468_charger *pca9468)
{
	int ret = 0;

	pr_info("%s: ======START=======\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_CHECK_ACTIVE);
#else
	pca9468->charging_state = DC_STATE_CHECK_ACTIVE;
#endif

	ret = pca9468_check_error(pca9468);

	if (ret == 0) {
		/* PCA9468 is active state */
		/* Clear retry counter */
		pca9468->retry_cnt = 0;
		/* Check TA type */
		if (pca9468->ta_type == TA_TYPE_USBPD_20) {
			/* Go to FPDO CV mode */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_FPDOCVMODE;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
		} else if (pca9468->chg_mode == WC_DC_MODE) {
			/* Go to WC CV mode */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_CHECK_WCCVMODE;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
		} else {
			/* Go to Adjust CC mode */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_ADJUST_CCMODE;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
		}
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		ret = 0;
	} else if (ret == -EAGAIN) {
		/* It is the retry condition */
		/* Check the retry counter */
		if (pca9468->retry_cnt < PCA9468_MAX_RETRY_CNT) {
			/* Disable charging */
			ret = pca9468_set_charging(pca9468, false);
			/* Increase retry counter */
			pca9468->retry_cnt++;
			pr_err("%s: retry charging start - retry_cnt=%d\n", __func__, pca9468->retry_cnt);
			/* Go to DC_STATE_PRESET_DC */
			mutex_lock(&pca9468->lock);
			pca9468->timer_id = TIMER_PRESET_DC;
			pca9468->timer_period = 0;
			mutex_unlock(&pca9468->lock);
			schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			ret = 0;
		} else {
			pr_err("%s: retry fail\n", __func__);
			/* Notify maximum retry error */
			ret = -EINVAL;
		}
	} else {
		/* Implement error handler function if it is needed */
		/* Stop charging in timer_work */
	}

	return ret;
}


/* Enter direct charging algorithm */
static int pca9468_start_direct_charging(struct pca9468_charger *pca9468)
{
	int ret;
	unsigned int val;
	union power_supply_propval prop_val;

	pr_info("%s: =========START=========\n", __func__);

	/* Set OV_DELTA to 30% */
#if defined(CONFIG_SEC_FACTORY)
	val = OV_DELTA_40P << MASK2SHIFT(PCA9468_BIT_OV_DELTA);
#else
	val = OV_DELTA_30P << MASK2SHIFT(PCA9468_BIT_OV_DELTA);
#endif
	ret = pca9468_update_reg(pca9468, PCA9468_REG_SAFETY_CTRL,
							PCA9468_BIT_OV_DELTA, val);
	if (ret < 0)
			return ret;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	/* Set watchdog timer enable */
	pca9468_set_wdt_enable(pca9468, WDT_ENABLE);
	pca9468_check_wdt_control(pca9468);
#endif

	/* Set Switching Frequency */
	val = pca9468->pdata->fsw_cfg;
	ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
							PCA9468_BIT_FSW_CFG, val);
	if (ret < 0)
		return ret;
	pr_info("%s: sw_freq=%dkHz\n", __func__, sw_freq[pca9468->pdata->fsw_cfg]);

	/* Die Temperature regulation 120'C */
	val = 0x3 << MASK2SHIFT(PCA9468_BIT_TEMP_REG);
	ret = pca9468_update_reg(pca9468, PCA9468_REG_TEMP_CTRL,
							PCA9468_BIT_TEMP_REG, val);
	if (ret < 0)
		return ret;

	/* current sense resistance */
	val = pca9468->pdata->snsres << MASK2SHIFT(PCA9468_BIT_SNSRES);
	ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
							PCA9468_BIT_SNSRES, val);
	if (ret < 0)
		return ret;

	/* Set EN_CFG to active low */
	val = PCA9468_EN_ACTIVE_L;
	ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
							PCA9468_BIT_EN_CFG, val);
	if (ret < 0)
		return ret;

	/* Set NTC voltage threshold */
	val = pca9468->pdata->ntc_th / PCA9468_NTC_TH_STEP;
	ret = pca9468_write_reg(pca9468, PCA9468_REG_NTC_TH_1, (val & 0xFF));
	if (ret < 0)
		return ret;
	ret = pca9468_update_reg(pca9468, PCA9468_REG_NTC_TH_2,
						PCA9468_BIT_NTC_THRESHOLD9_8, (val >> 8));
	if (ret < 0)
		return ret;

	/* Clear LIMIT_INCREMENT_EN */
	val = 0;
	ret = pca9468_update_reg(pca9468, PCA9468_REG_IIN_CTRL,
							PCA9468_BIT_LIMIT_INCREMENT_EN, val);
	if (ret < 0)
		return ret;

	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_ONLINE, prop_val);

	/* Check the current power supply type */
	if (prop_val.intval == POWER_SUPPLY_TYPE_WIRELESS) {
		/* The present power supply type is wireless charger */
		pca9468->ta_type = TA_TYPE_WIRELESS;
		pca9468->chg_mode = WC_DC_MODE;
		pr_info("%s: The current power supply type is WC, chg_mode=%d\n", __func__, pca9468->chg_mode);
	} else if (prop_val.intval == SEC_BATTERY_CABLE_FPDO_DC) {
		/* The present power supply type is USBPD charger with only fixed PDO */
		pca9468->ta_type = TA_TYPE_USBPD_20;
	} else if (prop_val.intval == SEC_BATTERY_CABLE_PDIC_APDO) {
		/* The present power supply type is USBPD with APDO */
		pca9468->ta_type = TA_TYPE_USBPD;
	} else {
		/* DC cannot support the present power supply type - unknown power supply type */
		pca9468->ta_type = TA_TYPE_UNKNOWN;
	}
	pr_info("%s: ta_type = %d\n", __func__, pca9468->ta_type);

	/* wake lock */
	__pm_stay_awake(pca9468->monitor_wake_lock);

	/* Preset charging configuration and TA condition */
	ret = pca9468_preset_dcmode(pca9468);

	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


/* Check Vbat minimum level to start direct charging */
static int pca9468_check_vbatmin(struct pca9468_charger *pca9468)
{
	int vbat;
	int ret;
	union power_supply_propval val;

	pr_info("%s: =========START=========\n", __func__);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_set_charging_state(pca9468, DC_STATE_CHECK_VBAT);
#else
	pca9468->charging_state = DC_STATE_CHECK_VBAT;
#endif

	/* Check Vbat */
	vbat = pca9468_read_adc(pca9468, ADCCH_VBAT);
	if (vbat < 0) {
		ret = vbat;
	}

	/* Read switching charger status */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ret = psy_do_property(pca9468->pdata->sec_dc_name, get,
		POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
#else
	ret = pca9468_get_switching_charger_property(POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, &val);
#endif
	if (ret < 0) {
		/* Start Direct Charging again after 1sec */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_VBATMIN_CHECK;
		pca9468->timer_period = PCA9468_VBATMIN_CHECK_T;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		goto error;
	}

	if (val.intval == 0) {
		/* already disabled switching charger */
		/* Clear retry counter */
		pca9468->retry_cnt = 0;
		/* Preset TA voltage and PCA9468 parameters */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_PRESET_DC;
		pca9468->timer_period = 0;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
	} else {
		/* Switching charger is enabled */
		if (vbat > PCA9468_DC_VBAT_MIN) {
			/* Start Direct Charging */
			/* now switching charger is enabled */
			/* disable switching charger first */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			pca9468_set_switching_charger(pca9468, false);
#else
			ret = pca9468_set_switching_charger(false, 0, 0, 0);
#endif
		}

		/* Wait 1sec for stopping switching charger or Start 1sec timer for battery check */
		mutex_lock(&pca9468->lock);
		pca9468->timer_id = TIMER_VBATMIN_CHECK;
		pca9468->timer_period = PCA9468_VBATMIN_CHECK_T;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
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
static void pca9468_timer_work(struct work_struct *work)
{
	struct pca9468_charger *pca9468 = container_of(work, struct pca9468_charger,
						 timer_work.work);
	int ret = 0;
	unsigned int val;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval value = {0,};

	psy_do_property("battery", get,
				POWER_SUPPLY_EXT_PROP_CHARGE_COUNTER_SHADOW, value);
	if (value.intval == SEC_BATTERY_CABLE_NONE) {
		goto error;
	}
#endif

#ifdef CONFIG_RTC_HCTOSYS
	get_current_time(&pca9468->last_update_time);

	pr_info("%s: timer id=%d, charging_state=%d, last_update_time=%lu\n",
		__func__, pca9468->timer_id, pca9468->charging_state, pca9468->last_update_time);
#else
	pr_info("%s: timer id=%d, charging_state=%d\n",
		__func__, pca9468->timer_id, pca9468->charging_state);
#endif

	/* Check req_enable flag */
	if (pca9468->req_enable == false) {
		/* This case is when battery driver set to stop DC during timer_work is workinig */
		/* And after resuming time_work, timer_id is overwritten by pca9468 function */
		/* Timer id shall be TIMER_ID_NONE */
		pca9468->timer_id = TIMER_ID_NONE;
		pr_info("%s: req_enable=%d, timer id=%d, charging_state=%d\n",
			__func__, pca9468->req_enable, pca9468->timer_id, pca9468->charging_state);
	}

	switch (pca9468->timer_id) {
	case TIMER_VBATMIN_CHECK:
		ret = pca9468_check_vbatmin(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PRESET_DC:
		ret = pca9468_start_direct_charging(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PRESET_CONFIG:
		ret = pca9468_preset_config(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_ACTIVE:
		ret = pca9468_check_active_state(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ADJUST_CCMODE:
		ret = pca9468_charge_adjust_ccmode(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ENTER_CCMODE:
		ret = pca9468_charge_start_ccmode(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_CCMODE:
		ret = pca9468_charge_ccmode(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ENTER_CVMODE:
		/* Enter Pre-CV mode */
		ret = pca9468_charge_start_cvmode(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_CVMODE:
		ret = pca9468_charge_cvmode(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_PDMSG_SEND:
		if (pca9468->ta_type == TA_TYPE_USBPD_20) {
			/* Send PD Message */
			ret = pca9468_send_pd_message(pca9468, PD_MSG_REQUEST_FIXED_PDO);
		} else {
			/* Adjust TA current and voltage step */
			val = pca9468->ta_vol/PD_MSG_TA_VOL_STEP;	/* PPS voltage resolution is 20mV */
			pca9468->ta_vol = val*PD_MSG_TA_VOL_STEP;
			val = pca9468->ta_cur/PD_MSG_TA_CUR_STEP;	/* PPS current resolution is 50mA */
			pca9468->ta_cur = val*PD_MSG_TA_CUR_STEP;
			if (pca9468->ta_cur < PCA9468_TA_MIN_CUR)	/* PPS minimum current is 1000mA */
				pca9468->ta_cur = PCA9468_TA_MIN_CUR;
			/* Send PD Message */
			ret = pca9468_send_pd_message(pca9468, PD_MSG_REQUEST_APDO);
		}
		if (ret < 0)
			goto error;

		/* Go to the next state */
		mutex_lock(&pca9468->lock);
		switch (pca9468->charging_state) {
		case DC_STATE_PRESET_DC:
			pca9468->timer_id = TIMER_PRESET_CONFIG;
			break;
		case DC_STATE_ADJUST_CC:
			pca9468->timer_id = TIMER_ADJUST_CCMODE;
			break;
		case DC_STATE_START_CC:
			pca9468->timer_id = TIMER_ENTER_CCMODE;
			break;
		case DC_STATE_CC_MODE:
			pca9468->timer_id = TIMER_CHECK_CCMODE;
			break;
		case DC_STATE_START_CV:
			pca9468->timer_id = TIMER_ENTER_CVMODE;
			break;
		case DC_STATE_CV_MODE:
			pca9468->timer_id = TIMER_CHECK_CVMODE;
			break;
		case DC_STATE_ADJUST_TAVOL:
			pca9468->timer_id = TIMER_ADJUST_TAVOL;
			break;
		case DC_STATE_ADJUST_TACUR:
			pca9468->timer_id = TIMER_ADJUST_TACUR;
			break;
		case DC_STATE_BYPASS_MODE:
			pca9468->timer_id = TIMER_CHECK_BYPASSMODE;
			break;
		case DC_STATE_DCMODE_CHANGE:
			pca9468->timer_id = TIMER_DCMODE_CHANGE;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		pca9468->timer_period = PCA9468_PDMSG_WAIT_T;
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		break;

	case TIMER_ADJUST_TAVOL:
		ret = pca9468_adjust_ta_voltage(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ADJUST_TACUR:
		ret = pca9468_adjust_ta_current(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_WCCVMODE:
		ret = pca9468_charge_wc_cvmode(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_BYPASSMODE:
		ret = pca9468_charge_bypass_mode(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_DCMODE_CHANGE:
		ret = pca9468_charge_dcmode_change(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_CHECK_FPDOCVMODE:
		ret = pca9468_charge_fpdo_cvmode(pca9468);
		if (ret < 0)
			goto error;
		break;

	case TIMER_ID_NONE:
		ret = pca9468_stop_charging(pca9468);
		if (ret < 0)
			goto error;
		break;

	default:
		break;
	}

	/* Check the charging state again */
	if (pca9468->charging_state == DC_STATE_NO_CHARGING) {
		/* Cancel work queue again */
		cancel_delayed_work(&pca9468->timer_work);
		cancel_delayed_work(&pca9468->pps_work);
	}
	return;

error:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	pca9468->health_status = POWER_SUPPLY_EXT_HEALTH_DC_ERR;
#endif
	pca9468_stop_charging(pca9468);
	return;
}


/* delayed work function for pps periodic timer */
static void pca9468_pps_request_work(struct work_struct *work)
{
	struct pca9468_charger *pca9468 = container_of(work, struct pca9468_charger,
						 pps_work.work);

	int ret = 0;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int vin, iin;

	/* this is for wdt */
	vin = pca9468_read_adc(pca9468, ADCCH_VIN);
	iin = pca9468_read_adc(pca9468, ADCCH_IIN);
	pr_info("%s: pps_work_start (vin:%dmV, iin:%dmA)\n",
			__func__, vin/PCA9468_SEC_DENOM_U_M, iin/PCA9468_SEC_DENOM_U_M);
#else
	pr_info("%s: pps_work_start\n", __func__);
#endif

#if defined(CONFIG_SEND_PDMSG_IN_PPS_REQUEST_WORK)
	/* Send PD message */
	ret = pca9468_send_pd_message(pca9468, PD_MSG_REQUEST_APDO);
#endif
	pr_info("%s: End, ret=%d\n", __func__, ret);
}

static int pca9468_hw_init(struct pca9468_charger *pca9468)
{
	unsigned int val;
	int ret;

	pr_info("%s: =========START=========\n", __func__);

	/* Read Device info register to check the incomplete I2C operation */
	ret = pca9468_read_reg(pca9468, PCA9468_REG_DEVICE_INFO, &val);
	if ((ret < 0) || (val != PCA9468_DEVICE_ID)) {
		/* There is the incomplete I2C operation or I2C communication error */
		/* Read Device info register again */
		ret = pca9468_read_reg(pca9468, PCA9468_REG_DEVICE_INFO, &val);
		if ((ret < 0) || (val != PCA9468_DEVICE_ID)) {
			dev_err(pca9468->dev, "reading DEVICE_INFO failed, val=0x%x\n", val);
			ret = -EINVAL;
			return ret;
		}
	}

	/*
	 * Program the platform specific configuration values to the device
	 * first.
	 */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
	pca9468->health_status = POWER_SUPPLY_HEALTH_GOOD;
#endif

#if defined(CONFIG_SEC_FACTORY)
	val = OV_DELTA_40P << MASK2SHIFT(PCA9468_BIT_OV_DELTA);
#else
	val = OV_DELTA_30P << MASK2SHIFT(PCA9468_BIT_OV_DELTA);
#endif
	ret = pca9468_update_reg(pca9468, PCA9468_REG_SAFETY_CTRL,
							PCA9468_BIT_OV_DELTA, val);
	if (ret < 0)
		return ret;

	/* Set Switching Frequency */
	val = pca9468->pdata->fsw_cfg << MASK2SHIFT(PCA9468_BIT_FSW_CFG);
	ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
							PCA9468_BIT_FSW_CFG, val);
	if (ret < 0)
		return ret;

	/* Die Temperature regulation 120'C */
	val = 0x3 << MASK2SHIFT(PCA9468_BIT_TEMP_REG);
	ret = pca9468_update_reg(pca9468, PCA9468_REG_TEMP_CTRL,
							PCA9468_BIT_TEMP_REG, val);
	if (ret < 0)
		return ret;

	/* current sense resistance */
	val = pca9468->pdata->snsres << MASK2SHIFT(PCA9468_BIT_SNSRES);
	ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
						PCA9468_BIT_SNSRES, val);
	if (ret < 0)
		return ret;

	/* Set Reverse Current Detection and standby mode*/
	val = PCA9468_BIT_REV_IIN_DET | PCA9468_EN_ACTIVE_L | PCA9468_STANDBY_FORCED;
	ret = pca9468_update_reg(pca9468, PCA9468_REG_START_CTRL,
							(PCA9468_BIT_REV_IIN_DET | PCA9468_BIT_EN_CFG | PCA9468_BIT_STANDBY_EN),
							val);
	if (ret < 0)
		return ret;

	/* clear LIMIT_INCREMENT_EN */
	val = 0;
	ret = pca9468_update_reg(pca9468, PCA9468_REG_IIN_CTRL,
						PCA9468_BIT_LIMIT_INCREMENT_EN, val);
	if (ret < 0)
		return ret;

	/* Set the ADC channel */
	val = (PCA9468_BIT_CH7_EN |	/* NTC voltage ADC */
		PCA9468_BIT_CH6_EN | /* DIETEMP ADC */
		PCA9468_BIT_CH5_EN | /* IIN ADC */
		PCA9468_BIT_CH3_EN | /* VBAT ADC */
		PCA9468_BIT_CH2_EN | /* VIN ADC */
		PCA9468_BIT_CH1_EN); /* VOUT ADC */

	ret = pca9468_write_reg(pca9468, PCA9468_REG_ADC_CFG, val);
	if (ret < 0)
		return ret;

	/* ADC Mode change */
	val = 0x5B;
	ret = pca9468_write_reg(pca9468, PCA9468_REG_ADC_ACCESS, val);
	if (ret < 0)
		return ret;
	val = 0x10;
	ret = pca9468_write_reg(pca9468, PCA9468_REG_ADC_MODE, val);
	if (ret < 0)
		return ret;
	val = 0x00;
	ret = pca9468_write_reg(pca9468, PCA9468_REG_ADC_ACCESS, val);
	if (ret < 0)
		return ret;

	/* Read ADC compensation gain */
	ret = pca9468_read_reg(pca9468, PCA9468_REG_ADC_ADJUST, &val);
	if (ret < 0)
		return ret;
	pca9468->adc_comp_gain = adc_gain[(val>>MASK2SHIFT(PCA9468_BIT_ADC_GAIN))];
	/* Check revision information */
	if ((val & PCA9468_BIT_OTP_VERSION) == PCA9468_REVISION_B3) {
		pca9468->revision = REV_B3;
	} else if ((val & PCA9468_BIT_OTP_VERSION) == PCA9468_REVISION_B4) {
		pca9468->revision = REV_B4;
	}

	pr_info("%s: pca9468_revision=%d\n", __func__, pca9468->revision);

	/* input current - uA*/
	ret = pca9468_set_input_current(pca9468, pca9468->pdata->iin_cfg);
	if (ret < 0)
		return ret;

	/* charging current */
	ret = pca9468_set_charging_current(pca9468, pca9468->pdata->ichg_cfg);
	if (ret < 0)
		return ret;

	/* v float voltage */
	ret = pca9468_set_vfloat(pca9468, pca9468->pdata->v_float);
	if (ret < 0)
		return ret;

	/* Save initial charging parameters */
	pca9468->iin_cfg = pca9468->pdata->iin_cfg;
	pca9468->ichg_cfg = pca9468->pdata->ichg_cfg;
	pca9468->vfloat = pca9468->pdata->v_float;
	pca9468->max_vfloat = pca9468->pdata->v_float;
	pca9468->iin_topoff = pca9468->pdata->iin_topoff;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468->fpdo_dc_iin_topoff = pca9468->pdata->fpdo_dc_iin_topoff;
	pca9468->fpdo_dc_vnow_topoff = pca9468->pdata->fpdo_dc_vnow_topoff;
	pca9468->fpdo_dc_mainbat_topoff = pca9468->pdata->fpdo_dc_mainbat_topoff;
	pca9468->fpdo_dc_subbat_topoff = pca9468->pdata->fpdo_dc_subbat_topoff;
#endif

	/* Clear new iin and new vfloat */
	pca9468->new_iin = 0;
	pca9468->new_vfloat = 0;

	/* Initial TA control method is Current Limit mode */
	pca9468->ta_ctrl = TA_CTRL_CL_MODE;

	/* Clear switching frequency change sequence */
	pca9468->req_sw_freq = REQ_SW_FREQ_0;

	/* Set vfloat decrement flag to false by default */
	pca9468->dec_vfloat = false;

	/* Clear charging done counter */
	pca9468->done_cnt = 0;

	/* Read PCA9468 VBAT_ADC */
	pca9468_read_vbat_adc(pca9468);
	return ret;
}

static irqreturn_t pca9468_interrupt_handler(int irq, void *data)
{
	struct pca9468_charger *pca9468 = data;
	u8 int1[REG_INT1_MAX], sts[REG_STS_MAX];	/* INT1, INT1_MSK, INT1_STS, STS_A, B, C, D */
	u8 masked_int;	/* masked int */
	bool handled = false;
	int ret;

	/* Read INT1, INT1_MSK, INT1_STS */
	ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_INT1, int1, 3);
	if (ret < 0) {
		dev_warn(pca9468->dev, "reading INT1_X failed\n");
		return IRQ_NONE;
	}

	/* Read STS_A, B, C, D */
	ret = pca9468_bulk_read_reg(pca9468, PCA9468_REG_STS_A, sts, 4);
	if (ret < 0) {
		dev_warn(pca9468->dev, "reading STS_X failed\n");
		return IRQ_NONE;
	}

	pr_info("%s: int1=0x%2x, int1_sts=0x%2x, sts_a=0x%2x\n", __func__,
			int1[REG_INT1], int1[REG_INT1_STS], sts[REG_STS_A]);

	/* Check Interrupt */
	masked_int = int1[REG_INT1] & !int1[REG_INT1_MSK];
	if (masked_int & PCA9468_BIT_V_OK_INT) {
		/* V_OK interrupt happened */
		mutex_lock(&pca9468->lock);
		pca9468->mains_online = (int1[REG_INT1_STS] & PCA9468_BIT_V_OK_STS) ? true : false;
		mutex_unlock(&pca9468->lock);
		power_supply_changed(pca9468->psy_chg);
		handled = true;
	}

	if (masked_int & PCA9468_BIT_NTC_TEMP_INT) {
		/* NTC_TEMP interrupt happened */
		if (int1[REG_INT1_STS] & PCA9468_BIT_NTC_TEMP_STS) {
			/* above NTC_THRESHOLD */
			dev_err(pca9468->dev, "charging stopped due to NTC threshold voltage\n");
		}
		handled = true;
	}

	if (masked_int & PCA9468_BIT_CHG_PHASE_INT) {
		/* CHG_PHASE interrupt happened */
		if (int1[REG_INT1_STS] & PCA9468_BIT_CHG_PHASE_STS) {
			/* Any of loops is active*/
			if (sts[REG_STS_A] & PCA9468_BIT_VFLT_LOOP_STS)	{
				/* V_FLOAT loop is in regulation */
				pr_info("%s: V_FLOAT loop interrupt\n", __func__);
				/* Disable CHG_PHASE_M */
				ret = pca9468_update_reg(pca9468, PCA9468_REG_INT1_MSK,
										PCA9468_BIT_CHG_PHASE_M, PCA9468_BIT_CHG_PHASE_M);
				if (ret < 0) {
					handled = false;
					return handled;
				}
				/* Go to Pre CV Mode */
				pca9468->timer_id = TIMER_ENTER_CVMODE;
				pca9468->timer_period = 10;
				schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
			} else if (sts[REG_STS_A] & PCA9468_BIT_IIN_LOOP_STS) {
				/* IIN loop or ICHG loop is in regulation */
				pr_info("%s: IIN loop interrupt\n", __func__);
			} else if (sts[REG_STS_A] & PCA9468_BIT_CHG_LOOP_STS) {
				/* ICHG loop is in regulation */
				pr_info("%s: ICHG loop interrupt\n", __func__);
			}
		}
		handled = true;
	}

	if (masked_int & PCA9468_BIT_CTRL_LIMIT_INT) {
		/* CTRL_LIMIT interrupt happened */
		if (int1[REG_INT1_STS] & PCA9468_BIT_CTRL_LIMIT_STS) {
			/* No Loop is active or OCP */
			if (sts[REG_STS_B] & PCA9468_BIT_OCP_FAST_STS) {
				/* Input fast over current */
				dev_err(pca9468->dev, "IIN > 50A instantaneously\n");
			}
			if (sts[REG_STS_B] & PCA9468_BIT_OCP_AVG_STS) {
				/* Input average over current */
				dev_err(pca9468->dev, "IIN > IIN_CFG*150percent\n");
			}
		}
		handled = true;
	}

	if (masked_int & PCA9468_BIT_TEMP_REG_INT) {
		/* TEMP_REG interrupt happened */
		if (int1[REG_INT1_STS] & PCA9468_BIT_TEMP_REG_STS) {
			/* Device is in temperature regulation */
			dev_err(pca9468->dev, "Device is in temperature regulation\n");
		}
		handled = true;
	}

	if (masked_int & PCA9468_BIT_ADC_DONE_INT) {
		/* ADC complete interrupt happened */
		dev_dbg(pca9468->dev, "ADC has been completed\n");
		handled = true;
	}

	if (masked_int & PCA9468_BIT_TIMER_INT) {
		/* Timer fault interrupt happened */
		if (int1[REG_INT1_STS] & PCA9468_BIT_TIMER_STS) {
			if (sts[REG_STS_B] & PCA9468_BIT_CHARGE_TIMER_STS) {
				/* Charger timer is expired */
				dev_err(pca9468->dev, "Charger timer is expired\n");
			}
			if (sts[REG_STS_B] & PCA9468_BIT_WATCHDOG_TIMER_STS) {
				/* Watchdog timer is expired */
				dev_err(pca9468->dev, "Watchdog timer is expired\n");
			}
		}
		handled = true;
	}

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static int pca9468_irq_init(struct pca9468_charger *pca9468,
	struct i2c_client *client)
{
	const struct pca9468_platform_data *pdata = pca9468->pdata;
	int ret, msk, irq = 0;

	pr_info("%s: =========START=========\n", __func__);

	if (pdata->irq_gpio >= 0) {
		irq = gpio_to_irq(pdata->irq_gpio);

		ret = gpio_request_one(pdata->irq_gpio, GPIOF_IN, client->name);
		if (ret < 0)
			goto fail;

		ret = request_threaded_irq(irq, NULL, pca9468_interrupt_handler,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			client->name, pca9468);
		if (ret < 0)
			goto fail_gpio;
	}
	/*
	 * Configure the Mask Register for interrupts: disable all interrupts by default.
	 */
	msk = (PCA9468_BIT_V_OK_M |
		PCA9468_BIT_NTC_TEMP_M |
		PCA9468_BIT_CHG_PHASE_M |
		PCA9468_BIT_RESERVED_M |
		PCA9468_BIT_CTRL_LIMIT_M |
		PCA9468_BIT_TEMP_REG_M |
		PCA9468_BIT_ADC_DONE_M |
		PCA9468_BIT_TIMER_M);
	ret = pca9468_write_reg(pca9468, PCA9468_REG_INT1_MSK, msk);
	if (ret < 0)
		goto fail_write;

	client->irq = irq;
	return 0;

fail_write:
	free_irq(irq, pca9468);
fail_gpio:
	if (pdata->irq_gpio >= 0)
		gpio_free(pdata->irq_gpio);
fail:
	client->irq = 0;
	return ret;
}


/*
 * Returns the input current limit programmed
 * into the charger in uA.
 */
static int get_input_current_limit(struct pca9468_charger *pca9468)
{
	int ret, intval;
	unsigned int val;

	if (!pca9468->mains_online)
		return -ENODATA;

	ret = pca9468_read_reg(pca9468, PCA9468_REG_IIN_CTRL, &val);
	if (ret < 0)
		return ret;

	intval = (val & PCA9468_BIT_IIN_CFG) * 100000;

	if (intval < 500000)
		intval = 500000;

	return intval;
}

/*
 * Returns the constant charge current programmed
 * into the charger in uA.
 */
static int get_const_charge_current(struct pca9468_charger *pca9468)
{
	int ret, intval;
	unsigned int val;

	if (!pca9468->mains_online)
		return -ENODATA;

	ret = pca9468_read_reg(pca9468, PCA9468_REG_ICHG_CTRL, &val);
	if (ret < 0)
		return ret;

	intval = (val & PCA9468_BIT_ICHG_CFG) * 100000;

	return intval;
}

/*
 * Returns the constant charge voltage programmed
 * into the charger in uV.
 */
static int get_const_charge_voltage(struct pca9468_charger *pca9468)
{
	int ret, intval;
	unsigned int val;

	if (!pca9468->mains_online)
		return -ENODATA;

	ret = pca9468_read_reg(pca9468, PCA9468_REG_V_FLOAT, &val);
	if (ret < 0)
		return ret;

	intval = (val * 5 + 3725) * 1000;

	return intval;
}

/*
 * Returns the enable or disable value.
 * into 1 or 0.
 */
static int get_charging_enabled(struct pca9468_charger *pca9468)
{
	int ret, intval;
	unsigned int val;

	ret = pca9468_read_reg(pca9468, PCA9468_REG_START_CTRL, &val);
	if (ret < 0)
		return ret;

	intval = (val & PCA9468_BIT_STANDBY_EN) ? 0 : 1;

	return intval;
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
/*
 * Returns the system current in uV.
 */
static int get_system_current(struct pca9468_charger *pca9468)
{
	/* get the system current */
	/* get the battery power supply to get charging current */
	union power_supply_propval val;
	struct power_supply *psy;
	int ret;
	int iin, ibat, isys;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		dev_err(pca9468->dev, "Cannot find battery power supply\n");
		goto error;
	}

	/* get the charging current */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(psy);
	if (ret < 0) {
		dev_err(pca9468->dev, "Cannot get battery current from FG\n");
		goto error;
	}
	ibat = val.intval;

	/* calculate the system current */
	/* get input current */
	iin = pca9468_read_adc(pca9468, ADCCH_IIN);
	if (iin < 0) {
		dev_err(pca9468->dev, "Invalid IIN ADC\n");
		goto error;
	}

	/* calculate the system current */
	/* Isys = (Iin - Ifsw_cfg)*2 - Ibat */
	iin = (iin - iin_fsw_cfg[pca9468->pdata->fsw_cfg])*2;
	iin /= PCA9468_SEC_DENOM_U_M;
	isys = iin - ibat;
	pr_info("%s: isys=%dmA\n", __func__, isys);

	return isys;

error:
	return -EINVAL;
}
#endif

static int pca9468_chg_set_adc_force_mode(struct pca9468_charger *pca9468, u8 enable)
{
	unsigned int temp = 0;
	int ret = 0;

	if (enable) {
		temp = FORCE_NORMAL_MODE << MASK2SHIFT(PCA9468_BIT_FORCE_ADC_MODE);
		ret = pca9468_update_reg(pca9468, PCA9468_REG_ADC_CTRL, PCA9468_BIT_FORCE_ADC_MODE, temp);
		if (ret < 0)
			return ret;
	} else {
		temp = AUTO_MODE << MASK2SHIFT(PCA9468_BIT_FORCE_ADC_MODE);
		ret = pca9468_update_reg(pca9468, PCA9468_REG_ADC_CTRL, PCA9468_BIT_FORCE_ADC_MODE, temp);
		if (ret < 0)
			return ret;
	}

	ret = pca9468_read_reg(pca9468, PCA9468_REG_ADC_CTRL, &temp);
	pr_info("%s: ADC_CTRL : 0x%02x\n", __func__, temp);

	return ret;
}

static int pca9468_chg_set_property(struct power_supply *psy,
	enum power_supply_property prop, const union power_supply_propval *val)
{
	struct pca9468_charger *pca9468 = power_supply_get_drvdata(psy);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) prop;
#endif
	int ret = 0;
	unsigned int temp = 0;

	pr_info("%s: =========START=========\n", __func__);
	pr_info("%s: prop=%d, val=%d\n", __func__, prop, val->intval);

	switch ((int)prop) {
	/* Todo - Insert code */
	/* It needs modification by a customer */
	/* The customer make a decision to start charging and stop charging property */

	case POWER_SUPPLY_PROP_ONLINE:	/* need to change property */
		if (val->intval == 0) {
			pca9468->mains_online = false;
			/* Check TA detachment and clear new_iin */
			pca9468->new_iin = 0;
		} else {
			pca9468->mains_online = true;
		}
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9468->float_voltage = val->intval;
		temp = pca9468->float_voltage * PCA9468_SEC_DENOM_U_M;
		if (temp != pca9468->new_vfloat) {
			/* request new float voltage */
			pca9468->new_vfloat = temp;
			/* Check the charging state */
			if ((pca9468->charging_state == DC_STATE_NO_CHARGING) ||
				(pca9468->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Apply new vfloat when the direct charging is started */
				pca9468->vfloat = pca9468->new_vfloat;
			} else {
				/* Check whether the previous request is done or not */
				if (pca9468->req_new_vfloat == true) {
					/* The previous request is not done yet */
					pr_err("%s: There is the previous request for New vfloat\n", __func__);
					ret = -EBUSY;
				} else {
					/* Set request flag */
					mutex_lock(&pca9468->lock);
					pca9468->req_new_vfloat = true;
					mutex_unlock(&pca9468->lock);

					/* Check the charging state */
					if ((pca9468->charging_state == DC_STATE_CC_MODE) ||
						(pca9468->charging_state == DC_STATE_CV_MODE) ||
						(pca9468->charging_state == DC_STATE_BYPASS_MODE) ||
						(pca9468->charging_state == DC_STATE_CHARGING_DONE) ||
						(pca9468->charging_state == DC_STATE_FPDO_CV_MODE) ||
						(pca9468->charging_state == DC_STATE_WC_CV_MODE)) {
						/* cancel delayed_work */
						cancel_delayed_work(&pca9468->timer_work);
						/* do delayed work at once */
						mutex_lock(&pca9468->lock);
						pca9468->timer_period = 0;	// ms unit
						mutex_unlock(&pca9468->lock);
						schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						pr_info("%s: Not support new vfloat yet in charging state=%d\n",
								__func__, pca9468->charging_state);
					}
				}
			}
		}
#else
		if (val->intval != pca9468->new_vfloat) {
			/* request new float voltage */
			pca9468->new_vfloat = val->intval;
			/* Check the charging state */
			if ((pca9468->charging_state == DC_STATE_NO_CHARGING) ||
				(pca9468->charging_state == DC_STATE_CHECK_VBAT)) {
				/* Apply new vfloat when the direct charging is started */
				pca9468->vfloat = pca9468->new_vfloat;
			} else {
				/* Check whether the previous request is done or not */
				if (pca9468->req_new_vfloat == true) {
					/* The previous request is not done yet */
					pr_err("%s: There is the previous request for New vfloat\n", __func__);
					ret = -EBUSY;
				} else {
					/* Set request flag */
					mutex_lock(&pca9468->lock);
					pca9468->req_new_vfloat = true;
					mutex_unlock(&pca9468->lock);

					/* Check the charging state */
					if ((pca9468->charging_state == DC_STATE_CC_MODE) ||
						(pca9468->charging_state == DC_STATE_CV_MODE) ||
						(pca9468->charging_state == DC_STATE_BYPASS_MODE) ||
						(pca9468->charging_state == DC_STATE_CHARGING_DONE) ||
						(pca9468->charging_state == DC_STATE_FPDO_CV_MODE) ||
						(pca9468->charging_state == DC_STATE_WC_CV_MODE)) {
						/* cancel delayed_work */
						cancel_delayed_work(&pca9468->timer_work);
						/* do delayed work at once */
						mutex_lock(&pca9468->lock);
						pca9468->timer_period = 0;	// ms unit
						mutex_unlock(&pca9468->lock);
						schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
					} else {
						/* Wait for next valid state - cc, cv, or bypass state */
						pr_info("%s: Not support new vfloat yet in charging state=%d\n",
								__func__, pca9468->charging_state);
					}
				}
			}
		}
#endif
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		pca9468->input_current = val->intval;
		temp = pca9468->input_current * PCA9468_SEC_DENOM_U_M;
		if (pca9468->ta_type == TA_TYPE_USBPD_20 && temp < pca9468->pdata->fpdo_dc_iin_lowest_limit) {
			pr_info("%s: PSP_ICL, IIN LOWEST LIMIT! IIN %d -> %d\n", __func__,
					temp, pca9468->pdata->fpdo_dc_iin_lowest_limit);
			temp = pca9468->pdata->fpdo_dc_iin_lowest_limit;
		}
		if (temp != pca9468->new_iin) {
			/* Compare with topoff current */
			if (temp < pca9468->iin_topoff) {
				/* This new iin is abnormal input current */
				pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, temp);
				ret = -EINVAL;
			} else {
				/* request new input current */
				pca9468->new_iin = temp;
				/* Check the charging state */
				if ((pca9468->charging_state == DC_STATE_NO_CHARGING) ||
					(pca9468->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Apply new iin when the direct charging is started */
					pca9468->iin_cfg = pca9468->new_iin;
				} else {
					/* Check whether the previous request is done or not */
					if (pca9468->req_new_iin == true) {
						/* The previous request is not done yet */
						pr_err("%s: There is the previous request for New iin\n", __func__);
						ret = -EBUSY;
					} else {
						/* Set request flag */
						mutex_lock(&pca9468->lock);
						pca9468->req_new_iin = true;
						mutex_unlock(&pca9468->lock);
						/* Check the charging state */
						if ((pca9468->charging_state == DC_STATE_CC_MODE) ||
							(pca9468->charging_state == DC_STATE_CV_MODE) ||
							(pca9468->charging_state == DC_STATE_BYPASS_MODE) ||
							(pca9468->charging_state == DC_STATE_CHARGING_DONE) ||
							(pca9468->charging_state == DC_STATE_FPDO_CV_MODE) ||
							(pca9468->charging_state == DC_STATE_WC_CV_MODE)) {
							/* cancel delayed_work */
							cancel_delayed_work(&pca9468->timer_work);
							/* do delayed work at once */
							mutex_lock(&pca9468->lock);
							pca9468->timer_period = 0;	// ms unit
							mutex_unlock(&pca9468->lock);
							schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
						} else {
							/* Wait for next valid state - cc, cv, or bypass state */
							pr_info("%s: Not support new iin yet in charging state=%d\n",
									__func__, pca9468->charging_state);
						}
					}
				}
			}
		} else {
			/* Compare with topoff current */
			if (temp < pca9468->iin_topoff) {
				/* This new iin is abnormal input current */
				pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, temp);
				ret = -EINVAL;
			} else {
				/* new iin is same as previous new_iin, but iin_cfg is different from it */
				/* Check the charging state */
				if ((pca9468->charging_state == DC_STATE_NO_CHARGING) ||
					(pca9468->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Apply new iin when the direct charging is started */
					pca9468->iin_cfg = pca9468->new_iin;
					pr_info("%s: charging state=%d, new iin(%uA) and iin_cfg(%uA)\n",
						__func__, pca9468->charging_state, pca9468->new_iin, pca9468->iin_cfg);
				}
			}
		}
#else
		if (val->intval != pca9468->new_iin) {
			/* Compare with topoff current */
			if (val->intval < pca9468->iin_topoff) {
				/* This new iin is abnormal input current */
				pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, val->intval);
				ret = -EINVAL;
			} else {
				/* request new input current */
				pca9468->new_iin = val->intval;
				/* Check the charging state */
				if ((pca9468->charging_state == DC_STATE_NO_CHARGING) ||
					(pca9468->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Apply new iin when the direct charging is started */
					pca9468->iin_cfg = pca9468->new_iin;
				} else {
					/* Check TA type */
					if (pca9468->ta_type == TA_TYPE_USBPD_20) {
						/* Cannot support change input current during DC */
						/* Because FPDO cannot control input current by PD messsage */
						pr_err("%s: Error - FPDO cannot control input current\n", __func__);
						ret = -EINVAL;
					} else {
						/* Check whether the previous request is done or not */
						if (pca9468->req_new_iin == true) {
							/* The previous request is not done yet */
							pr_err("%s: There is the previous request for New iin\n", __func__);
							ret = -EBUSY;
						} else {
							/* Set request flag */
							mutex_lock(&pca9468->lock);
							pca9468->req_new_iin = true;
							mutex_unlock(&pca9468->lock);
							/* Check the charging state */
							if ((pca9468->charging_state == DC_STATE_CC_MODE) ||
								(pca9468->charging_state == DC_STATE_CV_MODE) ||
								(pca9468->charging_state == DC_STATE_BYPASS_MODE) ||
								(pca9468->charging_state == DC_STATE_CHARGING_DONE) ||
								(pca9468->charging_state == DC_STATE_WC_CV_MODE)) {
								/* cancel delayed_work */
								cancel_delayed_work(&pca9468->timer_work);
								/* do delayed work at once */
								mutex_lock(&pca9468->lock);
								pca9468->timer_period = 0;	// ms unit
								mutex_unlock(&pca9468->lock);
								schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
							} else {
								/* Wait for next valid state - cc, cv, or bypass state */
								pr_info("%s: Not support new iin yet in charging state=%d\n",
										__func__, pca9468->charging_state);
							}
						}
					}
				}
			}
		} else {
			/* Compare with topoff current */
			if (val->intval < pca9468->iin_topoff) {
				/* This new iin is abnormal input current */
				pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, val->intval);
				ret = -EINVAL;
			} else {
				/* new iin is same as previous new_iin, but iin_cfg is different from it */
				/* Check the charging state */
				if ((pca9468->charging_state == DC_STATE_NO_CHARGING) ||
					(pca9468->charging_state == DC_STATE_CHECK_VBAT)) {
					/* Apply new iin when the direct charging is started */
					pca9468->iin_cfg = pca9468->new_iin;
					pr_info("%s: charging state=%d, new iin(%uA) and iin_cfg(%uA)\n",
						__func__, pca9468->charging_state, pca9468->new_iin, pca9468->iin_cfg);
				}
			}
		}
#endif
		break;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		pca9468->pdata->v_float = val->intval * DENOM_U_M;
		pr_info("%s: v_float(%duV)\n", __func__, pca9468->pdata->v_float);
		/* Save maximum vfloat to max_vfloat */
		pca9468->max_vfloat = pca9468->pdata->v_float;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		pca9468->charging_current = val->intval;
#if 0
		pca9468->pdata->ichg_cfg = val->intval * PCA9468_SEC_DENOM_U_M;
		pr_info("## %s: charging current(%duA)\n", __func__, pca9468->pdata->ichg_cfg);
		pca9468_set_charging_current(pca9468, pca9468->pdata->ichg_cfg);
#endif
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			if (val->intval) {
				pca9468->wdt_kick = true;
			} else {
				pca9468->wdt_kick = false;
				cancel_delayed_work(&pca9468->wdt_control_work);
			}
			pr_info("%s: wdt kick (%d)\n", __func__, pca9468->wdt_kick);
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			pca9468->input_current = val->intval;
			temp = pca9468->input_current * PCA9468_SEC_DENOM_U_M;
			if (pca9468->ta_type == TA_TYPE_USBPD_20 && temp < pca9468->pdata->fpdo_dc_iin_lowest_limit) {
				pr_info("%s: PSP_DCM, IIN LOWEST LIMIT! IIN %d -> %d\n", __func__,
						temp, pca9468->pdata->fpdo_dc_iin_lowest_limit);
				temp = pca9468->pdata->fpdo_dc_iin_lowest_limit;
			}
			if (temp != pca9468->new_iin) {
				/* Compare with topoff current */
				if (temp < pca9468->iin_topoff) {
					/* This new iin is abnormal input current */
					pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, temp);
					ret = -EINVAL;
				} else {
					/* request new input current */
					pca9468->new_iin = temp;
					/* Check the charging state */
					if ((pca9468->charging_state == DC_STATE_NO_CHARGING) ||
						(pca9468->charging_state == DC_STATE_CHECK_VBAT)) {
						/* Apply new iin when the direct charging is started */
						pca9468->iin_cfg = pca9468->new_iin;
					} else {
						/* Check whether the previous request is done or not */
						if (pca9468->req_new_iin == true) {
							/* The previous request is not done yet */
							pr_err("%s: There is the previous request for New iin\n", __func__);
							ret = -EBUSY;
						} else {
							/* Set request flag */
							mutex_lock(&pca9468->lock);
							pca9468->req_new_iin = true;
							mutex_unlock(&pca9468->lock);
							/* Check the charging state */
							if ((pca9468->charging_state == DC_STATE_CC_MODE) ||
								(pca9468->charging_state == DC_STATE_CV_MODE) ||
								(pca9468->charging_state == DC_STATE_BYPASS_MODE) ||
								(pca9468->charging_state == DC_STATE_CHARGING_DONE) ||
								(pca9468->charging_state == DC_STATE_FPDO_CV_MODE) ||
								(pca9468->charging_state == DC_STATE_WC_CV_MODE)) {
								/* cancel delayed_work */
								cancel_delayed_work(&pca9468->timer_work);
								/* do delayed work at once */
								mutex_lock(&pca9468->lock);
								pca9468->timer_period = 0;	// ms unit
								mutex_unlock(&pca9468->lock);
								schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
							} else {
								/* Wait for next valid state - cc, cv, or bypass state */
								pr_info("%s: Not support new iin yet in charging state=%d\n",
										__func__, pca9468->charging_state);
							}
						}
					}
				}
				pr_info("## %s: input current(new_iin: %duA)\n", __func__, pca9468->new_iin);
			} else {
				/* Compare with topoff current */
				if (temp < pca9468->iin_topoff) {
					/* This new iin is abnormal input current */
					pr_err("%s: This new iin(%duA) is abnormal value\n", __func__, temp);
					ret = -EINVAL;
				} else {
					/* new iin is same as previous new_iin, but iin_cfg is different from it */
					/* Check the charging state */
					if ((pca9468->charging_state == DC_STATE_NO_CHARGING) ||
						(pca9468->charging_state == DC_STATE_CHECK_VBAT)) {
						/* Apply new iin when the direct charging is started */
						pca9468->iin_cfg = pca9468->new_iin;
						pr_info("%s: charging state=%d, new iin(%uA) and iin_cfg(%uA)\n",
							__func__, pca9468->charging_state, pca9468->new_iin, pca9468->iin_cfg);
					}
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL:
			pca9468_chg_set_adc_force_mode(pca9468, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			if (val->intval == 0) {
				/* Set req_enable flag to false */
				pca9468->req_enable = false;
				// Stop Direct Charging
				ret = pca9468_stop_charging(pca9468);
				pca9468->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
				pca9468->health_status = POWER_SUPPLY_HEALTH_GOOD;
			} else {
				if (pca9468->charging_state == DC_STATE_NO_CHARGING) {
					// Start Direct Charging
					/* Set req_enable flag to true */
					pca9468->req_enable = true;
					/* Set initial wake up timeout - 10s */
					pm_wakeup_ws_event(pca9468->monitor_wake_lock, PCA9468_INIT_WAKEUP_T, false);
					/* Start 1sec timer for battery check */
					mutex_lock(&pca9468->lock);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
					pca9468_set_charging_state(pca9468, DC_STATE_CHECK_VBAT);
#else
					pca9468->charging_state = DC_STATE_CHECK_VBAT;
#endif
					pca9468->timer_id = TIMER_VBATMIN_CHECK;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
					pca9468->timer_period = PCA9468_VBATMIN_CHECK_T;	/* The delay time for PD state goes to PE_SNK_STATE */
#else
					pca9468->timer_period = 5000;	/* The delay time for PD state goes to PE_SNK_STATE */
#endif
					mutex_unlock(&pca9468->lock);
					schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
					ret = 0;
				} else {
					pr_info("## %s: duplicate charging enabled(%d)\n", __func__, val->intval);
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
			pr_info("[PASS_THROUGH] %s: called\n", __func__);
			if (val->intval != pca9468->new_dc_mode) {
				/* Request new dc mode */
				pca9468->new_dc_mode = val->intval;
				/* Check the charging state */
				if (pca9468->charging_state == DC_STATE_NO_CHARGING) {
					/* Not support state */
					pr_info("%s: Not support dc mode in charging state=%d\n", __func__, pca9468->charging_state);
					ret = -EINVAL;
				} else {
					/* Check whether the previous request is done or not */
					if (pca9468->req_new_dc_mode == true) {
						/* The previous request is not done yet */
						pr_err("%s: There is the previous request for New DC mode\n", __func__);
						ret = -EBUSY;
					} else {
						/* Set request flag */
						mutex_lock(&pca9468->lock);
						pca9468->req_new_dc_mode = true;
						mutex_unlock(&pca9468->lock);
						/* Check the charging state */
						if ((pca9468->charging_state == DC_STATE_CC_MODE) ||
							(pca9468->charging_state == DC_STATE_CV_MODE) ||
							(pca9468->charging_state == DC_STATE_BYPASS_MODE)) {
							/* cancel delayed_work */
							cancel_delayed_work(&pca9468->timer_work);
							/* do delayed work at once */
							mutex_lock(&pca9468->lock);
							pca9468->timer_period = 0;	// ms unit
							mutex_unlock(&pca9468->lock);
							schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
						} else {
							/* Wait for next valid state - cc, cv, or bypass state */
							pr_info("%s: Not support new dc mode yet in charging state=%d\n",
									__func__, pca9468->charging_state);
						}
					}
				}
			}
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			if (pca9468->charging_state == DC_STATE_BYPASS_MODE &&
				pca9468->dc_mode != PTM_NONE) {
				pr_info("[PASS_THROUGH_VOL] %s, bypass mode\n", __func__);
				/* Set TA voltage for bypass mode */
				pca9468_set_bypass_ta_voltage_by_soc(pca9468, val->intval);
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

	pr_info("%s: End, ret=%d\n", __func__, ret);
	return ret;
}


static int pca9468_chg_get_property(struct power_supply *psy,
	enum power_supply_property prop, union power_supply_propval *val)
{
	int ret = 0;
	struct pca9468_charger *pca9468 = power_supply_get_drvdata(psy);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) prop;
#endif

	switch ((int)prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = pca9468->mains_online;
		break;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = pca9468->chg_status;
		pr_info("%s: CHG STATUS : %d\n", __func__, pca9468->chg_status);
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (pca9468->charging_state >= DC_STATE_CHECK_ACTIVE &&
			pca9468->charging_state <= DC_STATE_CV_MODE)
			ret = pca9468_check_error(pca9468);

		val->intval = pca9468->health_status;
		pr_info("%s: HEALTH STATUS : %d, ret = %d\n",
			__func__, pca9468->health_status, ret);
		ret = 0;
		break;
#endif

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = get_const_charge_voltage(pca9468);
		if (ret < 0) {
			val->intval = 0;
			return ret;
		} else
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			val->intval = pca9468->float_voltage;
#else
			val->intval = ret;
#endif
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		/* Maximum vfloat */
		val->intval = pca9468->pdata->v_float / DENOM_U_M;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = get_const_charge_current(pca9468);
		if (ret < 0) {
			val->intval = 0;
			return ret;
		} else
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			val->intval = pca9468->charging_current;
#else
			val->intval = ret;
#endif
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = get_input_current_limit(pca9468);
		if (ret < 0) {
			val->intval = 0;
			return ret;
		} else
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
			val->intval = ret / PCA9468_SEC_DENOM_U_M;
#else
			val->intval = ret;
#endif
		break;

	case POWER_SUPPLY_PROP_TEMP:
		/* return NTC voltage - uV unit */
		ret = pca9468_read_adc(pca9468, ADCCH_NTC);
		if (ret < 0) {
			val->intval = 0;
			return ret;
		} else
			val->intval = ret;
		break;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			pca9468_monitor_work(pca9468);
			pca9468_test_read(pca9468);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			switch (val->intval) {
			case SEC_BATTERY_IIN_MA:
				pca9468_read_adc(pca9468, ADCCH_IIN);
				val->intval = pca9468->adc_val[ADCCH_IIN];
				break;
			case SEC_BATTERY_IIN_UA:
				pca9468_read_adc(pca9468, ADCCH_IIN);
				val->intval = pca9468->adc_val[ADCCH_IIN] * PCA9468_SEC_DENOM_U_M;
				break;
			case SEC_BATTERY_VIN_MA:
				val->intval = pca9468->adc_val[ADCCH_VIN];
				break;
			case SEC_BATTERY_VIN_UA:
				val->intval = pca9468->adc_val[ADCCH_VIN] * PCA9468_SEC_DENOM_U_M;
				break;
			default:
				val->intval = 0;
				break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
			/* return system current - uA unit */
			/* check charging status */
			if (pca9468->charging_state == DC_STATE_NO_CHARGING) {
				/* return invalid */
				val->intval = 0;
				return -EINVAL;
			} else {
				/* calculate Isys */
				ret = get_system_current(pca9468);
				if (ret < 0)
					return 0;
				else
					val->intval = ret;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
			val->strval = charging_state_str[pca9468->charging_state];
			pr_info("%s: CHARGER_STATUS(%s)\n", __func__, val->strval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			ret = get_charging_enabled(pca9468);
			if (ret < 0)
				return ret;
			else
				val->intval = ret;
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE:
			break;
		case POWER_SUPPLY_EXT_PROP_PASS_THROUGH_MODE_TA_VOL:
			break;
		case POWER_SUPPLY_EXT_PROP_DCHG_READ_BATP_BATN:
			pr_info("%s: unsupported POWER_SUPPLY_EXT_PROP_DCHG_READ_BATP_BATN\n", __func__);
			val->intval = -1000;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property pca9468_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};


static const struct regmap_config pca9468_regmap = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= PCA9468_MAX_REGISTER,
};

static char *pca9468_supplied_to[] = {
	"pca9468-charger",
};

static const struct power_supply_desc pca9468_charger_power_supply_desc = {
	.name		= "pca9468-charger",
	.type		= POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property	= pca9468_chg_get_property,
	.set_property 	= pca9468_chg_set_property,
	.properties	= pca9468_charger_props,
	.num_properties	= ARRAY_SIZE(pca9468_charger_props),
};

#if defined(CONFIG_OF)
static int pca9468_charger_parse_dt(struct device *dev, struct pca9468_platform_data *pdata)
{
	struct device_node *np_pca9468 = dev->of_node;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	struct device_node *np;
#endif
	int ret;
	if (!np_pca9468)
		return -EINVAL;

	/* irq gpio */
	pdata->irq_gpio = of_get_named_gpio(np_pca9468, "pca9468,irq-gpio", 0);
	if (pdata->irq_gpio < 0) {
		pr_err("%s : cannot get irq-gpio : %d\n",
			__func__, pdata->irq_gpio);
	} else {
		pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);
	}

	/* input current limit */
	ret = of_property_read_u32(np_pca9468, "pca9468,input-current-limit",
		&pdata->iin_cfg);
	if (ret) {
		pr_info("%s: pca9468,input-current-limit is Empty\n", __func__);
		pdata->iin_cfg = PCA9468_IIN_CFG_DFT;
	}
	pr_info("%s: pca9468,iin_cfg is %d\n", __func__, pdata->iin_cfg);

	/* charging current */
	ret = of_property_read_u32(np_pca9468, "pca9468,charging-current",
		&pdata->ichg_cfg);
	if (ret) {
		pr_info("%s: pca9468,charging-current is Empty\n", __func__);
		pdata->ichg_cfg = PCA9468_ICHG_CFG_DFT;
	}
	pr_info("%s: pca9468,ichg_cfg is %d\n", __func__, pdata->ichg_cfg);

	/* Maximum TA voltage threshold */
	ret = of_property_read_u32(np_pca9468, "pca9468,ta-max-vol",
						   &pdata->ta_max_vol);
	if (ret) {
		pr_info("%s: pca9468,ta-max-vol is Empty\n", __func__);
		pdata->ta_max_vol = PCA9468_TA_MAX_VOL;
	}
	pr_info("%s: pca9468,ta_max_vol is %d\n", __func__, pdata->ta_max_vol);

#if !IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	/* charging float voltage */
	ret = of_property_read_u32(np_pca9468, "pca9468,float-voltage",
		&pdata->v_float);
	if (ret) {
		pr_info("%s: pca9468,float-voltage is Empty\n", __func__);
		pdata->v_float = PCA9468_VFLOAT_DFT;
	}
	pr_info("%s: pca9468,v_float is %d\n", __func__, pdata->v_float);
#endif

	/* input topoff current */
	ret = of_property_read_u32(np_pca9468, "pca9468,input-itopoff",
		&pdata->iin_topoff);
	if (ret) {
		pr_info("%s: pca9468,input-itopoff is Empty\n", __func__);
		pdata->iin_topoff = PCA9468_IIN_DONE_DFT;
	}
	pr_info("%s: pca9468,iin_topoff is %d\n", __func__, pdata->iin_topoff);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	/* fpdo_dc input topoff current */
	ret = of_property_read_u32(np_pca9468, "pca9468,fpdo_dc_input-itopoff",
							   &pdata->fpdo_dc_iin_topoff);
	if (ret) {
		pr_info("%s: pca9468,fpdo_dc_input-itopoff is Empty\n", __func__);
		pdata->fpdo_dc_iin_topoff = 1700000; /* 1700mA */
	}
	pr_info("%s: pca9468,fpdo_dc_iin_topoff is %d\n", __func__, pdata->fpdo_dc_iin_topoff);

	/* fpdo_dc Vnow topoff condition */
	ret = of_property_read_u32(np_pca9468, "pca9468,fpdo_dc_vnow-topoff",
							&pdata->fpdo_dc_vnow_topoff);
	if (ret) {
		pr_info("%s: pca9468,fpdo_dc_vnow-topoff is Empty\n", __func__);
		pdata->fpdo_dc_vnow_topoff = 5000000; /* Vnow 5000000uV means disable */
	}
	pr_info("%s: pca9468,fpdo_dc_vnow_topoff is %d\n", __func__, pdata->fpdo_dc_vnow_topoff);

	ret = of_property_read_u32(np_pca9468, "pca9468,fpdo_dc_mainbat-topoff",
							&pdata->fpdo_dc_mainbat_topoff);
	if (ret) {
		pr_info("%s: pca9468,fpdo_dc_mainbat-topoff is Empty\n", __func__);
		pdata->fpdo_dc_mainbat_topoff = 5000000; /* mainbat 5000000uV means disable */
	}
	pr_info("%s: pca9468,fpdo_dc_mainbat_topoff is %d\n", __func__, pdata->fpdo_dc_mainbat_topoff);

	ret = of_property_read_u32(np_pca9468, "pca9468,fpdo_dc_subbat-topoff",
							&pdata->fpdo_dc_subbat_topoff);
	if (ret) {
		pr_info("%s: pca9468,fpdo_dc_subbat-topoff is Empty\n", __func__);
		pdata->fpdo_dc_subbat_topoff = 5000000; /* subbat 5000000uV means disable */
	}
	pr_info("%s: pca9468,fpdo_dc_subbat_topoff is %d\n", __func__, pdata->fpdo_dc_subbat_topoff);
#endif
	/* sense resistance */
	ret = of_property_read_u32(np_pca9468, "pca9468,sense-resistance",
		&pdata->snsres);
	if (ret) {
		pr_info("%s: pca9468,sense-resistance is Empty\n", __func__);
		pdata->snsres = PCA9468_SENSE_R_DFT;
	}
	pr_info("%s: pca9468,snsres is %d\n", __func__, pdata->snsres);

	/* switching frequency */
	ret = of_property_read_u32(np_pca9468, "pca9468,switching-frequency",
		&pdata->fsw_cfg);
	if (ret) {
		pr_info("%s: pca9468,switching frequency is Empty\n", __func__);
		pdata->fsw_cfg = PCA9468_FSW_CFG_DFT;
	}
	pr_info("%s: pca9468,fsw_cfg is %d\n", __func__, pdata->fsw_cfg);

	/* switching frequency for pass through mode */
	ret = of_property_read_u32(np_pca9468, "pca9468,switching-frequency-bypass",
		&pdata->fsw_cfg_byp);
	if (ret) {
		pr_info("%s: pca9468,switching frequency byp is Empty\n", __func__);
		pdata->fsw_cfg_byp = FSW_CFG_440KHZ;
	}
	pr_info("%s: pca9468,fsw_cfg_byp is %d\n", __func__, pdata->fsw_cfg_byp);

	/* switching frequency for low input current */
	ret = of_property_read_u32(np_pca9468, "pca9468,switching-frequency-low",
		&pdata->fsw_cfg_low);
	if (ret) {
		pr_info("%s: pca9468,switching frequency for low input current is Empty\n", __func__);
		pdata->fsw_cfg_low = PCA9468_FSW_CFG_LOW_DFT;
	}
	pr_info("%s: pca9468,fsw_cfg_low is %d\n", __func__, pdata->fsw_cfg_low);

	/* switching frequency for fixed pdo */
	ret = of_property_read_u32(np_pca9468, "pca9468,switching-frequency-fpdo",
		&pdata->fsw_cfg_fpdo);
	if (ret) {
		pr_info("%s: pca9468,switching frequency for FPDO is Empty\n", __func__);
		pdata->fsw_cfg_fpdo = PCA9468_FSW_CFG_DFT;
	}
	pr_info("%s: pca9468,fsw_cfg_fpdo is %d\n", __func__, pdata->fsw_cfg_fpdo);

	/* NTC threshold voltage */
	ret = of_property_read_u32(np_pca9468, "pca9468,ntc-threshold",
		&pdata->ntc_th);
	if (ret) {
		pr_info("%s: pca9468,ntc threshold voltage is Empty\n", __func__);
		pdata->ntc_th = PCA9468_NTC_TH_DFT;
	}
	pr_info("%s: pca9468,ntc_th is %d\n", __func__, pdata->ntc_th);

	/* NTC protection */
	ret = of_property_read_u32(np_pca9468, "pca9468,ntc-protection",
		&pdata->ntc_en);
	if (ret) {
		pr_info("%s: pca9468,ntc protection is Empty\n", __func__);
		pdata->ntc_en = 0;	/* Disable */
	}
	pr_info("%s: pca9468,ntc_en is %d\n", __func__, pdata->ntc_en);

	/* Charging mode */
	ret = of_property_read_u32(np_pca9468, "pca9468,chg-mode",
		&pdata->chg_mode);
	if (ret) {
		pr_info("%s: pca9468,charging mode is Empty\n", __func__);
		pdata->chg_mode = CHG_2TO1_DC_MODE;
	}
	pr_info("%s: pca9468,chg_mode is %d\n", __func__, pdata->chg_mode);

	/* cv mode polling time in step1 charging */
	ret = of_property_read_u32(np_pca9468, "pca9468,cv-polling",
		&pdata->cv_polling);
	if (ret) {
		pr_info("%s: pca9468,cv-polling is Empty\n", __func__);
		pdata->cv_polling = PCA9468_CVMODE_CHECK_T;
	}
	pr_info("%s: pca9468,cv polling is %d\n", __func__, pdata->cv_polling);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pdata->chgen_gpio = of_get_named_gpio(np_pca9468, "pca9468,chg_gpio_en", 0);
	if (pdata->chgen_gpio < 0) {
		pr_err("%s : cannot get chgen gpio : %d\n",
			__func__, pdata->chgen_gpio);
	} else {
		pr_info("%s: chgen gpio : %d\n", __func__, pdata->chgen_gpio);
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("## %s: np(battery) NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
			&pdata->v_float);
		if (ret) {
			pr_info("## %s: battery,chg_float_voltage is Empty\n", __func__);
			pdata->v_float = PCA9468_VFLOAT_DFT;
		} else {
			pdata->v_float = pdata->v_float * PCA9468_SEC_DENOM_U_M;
			pr_info("## %s: battery,chg_float_voltage is %d\n", __func__,
				pdata->v_float);
		}

		ret = of_property_read_string(np, "battery,charger_name",
				(char const **)&pdata->sec_dc_name);
		if (ret) {
			pr_err("## %s: direct_charger is Empty\n", __func__);
			pdata->sec_dc_name = "sec-direct-charger";
		}
		pr_info("%s: battery,charger_name is %s\n", __func__, pdata->sec_dc_name);

		/* Fuelgauge power supply name */
		ret = of_property_read_string(np, "battery,fuelgauge_name",
				(char const **)&pdata->fg_name);
		if (ret) {
			pr_info("## %s: fuelgauge_name name is Empty\n", __func__);
			pdata->fg_name = "battery";
		}
		pr_info("%s: fuelgauge name is %s\n", __func__, pdata->fg_name);

		/* the lowest limit to FPDO DC IIN */
		ret = of_property_read_u32(np, "battery,fpdo_dc_charge_power",
								   &pdata->fpdo_dc_iin_lowest_limit);
		pdata->fpdo_dc_iin_lowest_limit *= PCA9468_SEC_DENOM_U_M;
		pdata->fpdo_dc_iin_lowest_limit /= PCA9468_SEC_FPDO_DC_IV;
		if (ret) {
			pr_info("%s: battery,fpdo_dc_charge_power is Empty\n", __func__);
			pdata->fpdo_dc_iin_lowest_limit = 10000000; /* 10A */
		}
		pr_info("%s: fpdo_dc_iin_lowest_limit is %d\n", __func__, pdata->fpdo_dc_iin_lowest_limit);
	}
#endif

	/* vfloat threshold in step1 charging */
	ret = of_property_read_u32(np_pca9468, "pca9468,step1-vth",
		&pdata->step1_vth);
	if (ret) {
		pr_info("%s: pca9468,step1-vfloat-threshold is Empty\n", __func__);
		pdata->step1_vth = STEP1_VFLOAT_THRESHOLD;
	}
	pr_info("%s: pca9468,step1_vth is %d\n", __func__, pdata->step1_vth);

	/* VFLOAT method - FG VFLOAT or PCA9468 VFLOAT */
	ret = of_property_read_u32(np_pca9468, "pca9468,fg-vfloat",
		&pdata->fg_vfloat);
	if (ret) {
		pr_info("%s: pca9468,fg-vfloat is Empty\n", __func__);
		pdata->fg_vfloat = 0;
	}
	pr_info("%s: pca9468,fg-vfloat is %d\n", __func__, pdata->fg_vfloat);

	/* Input current for low frequency threhold */
	ret = of_property_read_u32(np_pca9468, "pca9468,iin-low-frequency",
		&pdata->iin_low_freq);
	if (ret) {
		pr_info("%s: pca9468,iin-low-frequency is Empty\n", __func__);
		pdata->iin_low_freq = PCA9468_IIN_LOW_TH_SW_FREQ;
	}
	pr_info("%s: pca9468,iin-low-frequency is %d\n", __func__, pdata->iin_low_freq);

	/* Default VFLOAT voltage */
	ret = of_property_read_u32(np_pca9468, "pca9468,default-vfloat",
							   &pdata->dft_vfloat);
	if (ret) {
		pr_info("%s: pca9468,default-vfloat is Empty\n", __func__);
		pdata->dft_vfloat = PCA9468_VFLOAT_DFT;
	}
	pr_info("%s: pca9468,default-vfloat is %d\n", __func__, pdata->dft_vfloat);

	/* PCA9468 VBAT connection */
	ret = of_property_read_u32(np_pca9468, "pca9468,battery-connection",
							   &pdata->bat_conn);
	if (ret) {
		pr_info("%s: pca9468,battery-connection is Empty\n", __func__);
		pdata->bat_conn = VBAT_TO_VBPACK_OR_VBCELL;
	}
	pr_info("%s: pca9468,battery-connection is %d\n", __func__, pdata->bat_conn);

	/* TA voltage fixed offset in fianl target voltage calculation */
	ret = of_property_read_u32(np_pca9468, "pca9468,ta-volt-offset",
							   &pdata->ta_volt_offset);
	if (ret) {
		pr_info("%s: pca9468,ta-volt-offset is Empty\n", __func__);
		pdata->ta_volt_offset = PCA9468_TA_VOL_OFFSET_FINAL;
	}
	pr_info("%s: pca9468,ta-volt-offset is %d\n", __func__, pdata->ta_volt_offset);

	return 0;
}
#else
static int pca9468_charger_parse_dt(struct device *dev, struct pca9468_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

#ifdef CONFIG_USBPD_PHY_QCOM
static int pca9468_usbpd_setup(struct pca9468_charger *pca9468)
{
	int ret = 0;
	const char *pd_phandle = "usbpd-phy";

	pca9468->pd = devm_usbpd_get_by_phandle(pca9468->dev, pd_phandle);

	if (IS_ERR(pca9468->pd)) {
		pr_err("get_usbpd phandle failed (%ld)\n",
				PTR_ERR(pca9468->pd));
		return PTR_ERR(pca9468->pd);
	}

	return ret;
}
#endif

static int read_reg(void *data, u64 *val)
{
	struct pca9468_charger *chip = data;
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
	struct pca9468_charger *chip = data;
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

static int pca9468_create_debugfs_entries(struct pca9468_charger *chip)
{
	struct dentry *ent;
	int rc = 0;

	chip->debug_root = debugfs_create_dir("charger-pca9468", NULL);
	if (!chip->debug_root) {
		dev_err(chip->dev, "Couldn't create debug dir\n");
		rc = -ENOENT;
	} else {
		debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
			chip->debug_root, &(chip->debug_address));

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
			chip->debug_root, chip, &register_debug_ops);
		if (!ent) {
			dev_err(chip->dev,
				"Couldn't create data debug file\n");
			rc = -ENOENT;
		}
	}

	return rc;
}


static int pca9468_charger_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct power_supply_config chager_cfg = {};
	struct pca9468_platform_data *pdata;
	struct device *dev = &client->dev;
	struct pca9468_charger *pca9468_chg;
	int ret;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pr_info("%s: PCA9468 Charger Driver Loading\n", __func__);
#endif
	pr_info("%s: =========START=========\n", __func__);

	pca9468_chg = devm_kzalloc(dev, sizeof(*pca9468_chg), GFP_KERNEL);
	if (!pca9468_chg)
		return -ENOMEM;

#if defined(CONFIG_OF)
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(struct pca9468_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory \n");
			return -ENOMEM;
		}

		ret = pca9468_charger_parse_dt(&client->dev, pdata);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to get device of_node \n");
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

	i2c_set_clientdata(client, pca9468_chg);

	mutex_init(&pca9468_chg->lock);
	mutex_init(&pca9468_chg->i2c_lock);
	pca9468_chg->dev = &client->dev;
	pca9468_chg->pdata = pdata;
	pca9468_chg->charging_state = DC_STATE_NO_CHARGING;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_chg->wdt_kick = false;
#endif

	pca9468_chg->monitor_wake_lock = wakeup_source_register(&client->dev, "pca9468-charger-monitor");

	/* initialize work */
	INIT_DELAYED_WORK(&pca9468_chg->timer_work, pca9468_timer_work);
	mutex_lock(&pca9468_chg->lock);
	pca9468_chg->timer_id = TIMER_ID_NONE;
	pca9468_chg->timer_period = 0;
	mutex_unlock(&pca9468_chg->lock);

	INIT_DELAYED_WORK(&pca9468_chg->pps_work, pca9468_pps_request_work);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	INIT_DELAYED_WORK(&pca9468_chg->wdt_control_work, pca9468_wdt_control_work);
#endif

	pca9468_chg->regmap = devm_regmap_init_i2c(client, &pca9468_regmap);
	if (IS_ERR(pca9468_chg->regmap)) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		ret = PTR_ERR(pca9468_chg->regmap);
		goto err_regmap_init;
#else
		return PTR_ERR(pca9468_chg->regmap);
#endif
	}

	pca9468_chg->bat_psy = power_supply_get_by_name("battery");
	if (pca9468_chg->bat_psy == NULL)
		dev_err(dev, "Error get battery psy!\n");

#ifdef CONFIG_USBPD_PHY_QCOM
	if (pca9468_usbpd_setup(pca9468_chg)) {
		dev_err(dev, "Error usbpd setup!\n");
		pca9468_chg->pd = NULL;
	}
#endif

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pca9468_init_adc_val(pca9468_chg, -1);
#endif

	ret = pca9468_hw_init(pca9468_chg);
	if (ret < 0)
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		goto err_hw_init;
#else
		return ret;
#endif

	chager_cfg.supplied_to = pca9468_supplied_to;
	chager_cfg.num_supplicants = ARRAY_SIZE(pca9468_supplied_to);
	chager_cfg.drv_data = pca9468_chg;
	pca9468_chg->psy_chg = power_supply_register(dev,
		&pca9468_charger_power_supply_desc, &chager_cfg);
	if (IS_ERR(pca9468_chg->psy_chg)) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		ret = PTR_ERR(pca9468_chg->psy_chg);
		goto err_power_supply_regsister;
#else
		return PTR_ERR(pca9468_chg->mains);
#endif
	}

	/*
	 * Interrupt pin is optional. If it is connected, we setup the
	 * interrupt support here.
	 */
	if (pdata->irq_gpio >= 0) {
		ret = pca9468_irq_init(pca9468_chg, client);
		if (ret < 0) {
			dev_warn(dev, "failed to initialize IRQ: %d\n", ret);
			dev_warn(dev, "disabling IRQ support\n");
		}
		/* disable interrupt */
		disable_irq(client->irq);
	}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (pdata->chgen_gpio >= 0) {
		ret = gpio_request(pdata->chgen_gpio, "DC_CPEN");
		if (ret) {
			pr_info("%s : Request GPIO %d failed\n",
					__func__, (int)pdata->chgen_gpio);
		}
		gpio_direction_output(pdata->chgen_gpio, false);
	}
#endif

	ret = pca9468_create_debugfs_entries(pca9468_chg);
	if (ret < 0)
		goto error;

	sec_chg_set_dev_init(SC_DEV_DIR_CHG);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	pr_info("%s: PCA9468 Charger Driver Loaded\n", __func__);
#endif
	pr_info("%s: pca9468_driver version:%s\n", __func__, DRV_MODULE_VERSION);
	pr_info("%s: =========END=========\n", __func__);

	return 0;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
error:
err_power_supply_regsister:
err_hw_init:
err_regmap_init:
	wakeup_source_unregister(pca9468_chg->monitor_wake_lock);
	return ret;
#endif
}

static int pca9468_charger_remove(struct i2c_client *client)
{
	struct pca9468_charger *pca9468_chg = i2c_get_clientdata(client);

	pr_info("%s: ++\n", __func__);

	/* stop charging if it is active */
	pca9468_stop_charging(pca9468_chg);

	if (client->irq) {
		free_irq(client->irq, pca9468_chg);
		if (pca9468_chg->pdata->irq_gpio >= 0)
			gpio_free(pca9468_chg->pdata->irq_gpio);
	}

	wakeup_source_unregister(pca9468_chg->monitor_wake_lock);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	if (pca9468_chg->psy_chg)
		power_supply_unregister(pca9468_chg->psy_chg);
#else
	if (pca9468_chg->mains)
		power_supply_unregister(pca9468_chg->mains);
#endif

	debugfs_remove(pca9468_chg->debug_root);
	pr_info("%s: --\n", __func__);

	return 0;
}

static void pca9468_charger_shutdown(struct i2c_client *client)
{
	struct pca9468_charger *pca9468_chg = i2c_get_clientdata(client);

	pr_info("%s: ++\n", __func__);

	pca9468_set_charging(pca9468_chg, false);

	if (client->irq)
		free_irq(client->irq, pca9468_chg);

	cancel_delayed_work(&pca9468_chg->timer_work);
	cancel_delayed_work(&pca9468_chg->pps_work);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	cancel_delayed_work(&pca9468_chg->wdt_control_work);
#endif

	pr_info("%s: --\n", __func__);
}

static const struct i2c_device_id pca9468_charger_id_table[] = {
	{ "pca9468-charger", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pca9468_charger_id_table);

#if defined(CONFIG_OF)
static struct of_device_id pca9468_charger_match_table[] = {
	{ .compatible = "nxp,pca9468" },
	{ },
};
MODULE_DEVICE_TABLE(of, pca9468_charger_match_table);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
#ifdef CONFIG_RTC_HCTOSYS
static void pca9468_check_and_update_charging_timer(struct pca9468_charger *pca9468)
{
	unsigned long current_time = 0, next_update_time, time_left;

	get_current_time(&current_time);

	if (pca9468->timer_id != TIMER_ID_NONE)	{
		next_update_time = pca9468->last_update_time + (pca9468->timer_period / 1000);	// unit is second

		pr_info("%s: current_time=%ld, next_update_time=%ld\n", __func__, current_time, next_update_time);

		if (next_update_time > current_time)
			time_left = next_update_time - current_time;
		else
			time_left = 0;

		mutex_lock(&pca9468->lock);
		pca9468->timer_period = time_left * 1000;	// ms unit
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
		pr_info("%s: timer_id=%d, time_period=%ld\n", __func__, pca9468->timer_id, pca9468->timer_period);
	}
	pca9468->last_update_time = current_time;
}
#endif

static int pca9468_charger_suspend(struct device *dev)
{
	struct pca9468_charger *pca9468 = dev_get_drvdata(dev);

	pr_info("%s: cancel delayed work\n", __func__);

	/* cancel delayed_work */
	cancel_delayed_work(&pca9468->timer_work);
	return 0;
}

static int pca9468_charger_resume(struct device *dev)
{
	struct pca9468_charger *pca9468 = dev_get_drvdata(dev);

	pr_info("%s: update_timer\n", __func__);

	/* Update the current timer */
#ifdef CONFIG_RTC_HCTOSYS
	pca9468_check_and_update_charging_timer(pca9468);
#else
	if (pca9468->timer_id != TIMER_ID_NONE) {
		mutex_lock(&pca9468->lock);
		pca9468->timer_period = 0;	// ms unit
		mutex_unlock(&pca9468->lock);
		schedule_delayed_work(&pca9468->timer_work, msecs_to_jiffies(pca9468->timer_period));
	}
#endif
	return 0;
}
#else
#define pca9468_charger_suspend		NULL
#define pca9468_charger_resume		NULL
#endif

const struct dev_pm_ops pca9468_pm_ops = {
	.suspend = pca9468_charger_suspend,
	.resume = pca9468_charger_resume,
};

static struct i2c_driver pca9468_charger_driver = {
	.driver = {
		.name = "pca9468-charger",
#if defined(CONFIG_OF)
		.of_match_table = pca9468_charger_match_table,
#endif /* CONFIG_OF */
#if defined(CONFIG_PM)
		.pm = &pca9468_pm_ops,
#endif
	},
	.probe        = pca9468_charger_probe,
	.remove       = pca9468_charger_remove,
	.shutdown     = pca9468_charger_shutdown,
	.id_table     = pca9468_charger_id_table,
};

module_i2c_driver(pca9468_charger_driver);

MODULE_DESCRIPTION("PCA9468 charger driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_MODULE_VERSION);
