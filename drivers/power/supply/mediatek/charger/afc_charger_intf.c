/* afc_charger_intf.c
 *
 * Copyright (C) 2019 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/* hs14 code for SR-AL6528A-01-321 | AL6528ADEU-722 | AL6528A-659 by qiaodan at 2022/11/08 start */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/power_supply.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif
#if defined(CONFIG_SEC_PARAM)
#include <linux/sec_ext.h>
#endif
#include <linux/device.h>
#include <linux/errno.h>

#include <upmu_common.h>

// #include "afc_charger_intf.h"
#include "mtk_charger_intf.h"


extern int charger_dev_set_mivr(struct charger_device *chg_dev, u32 uV);
int afc_set_ta_vchr(struct charger_manager *pinfo, u32 chr_volt);

#define afc_udelay(num)	udelay(num)

int send_afc_result(int res)
{
	static struct power_supply *psy_bat = NULL;
	union power_supply_propval value = {0, };
	int ret = 0;

	if (psy_bat == NULL) {
		psy_bat = power_supply_get_by_name("battery");
		if (!psy_bat) {
			pr_err("%s: Failed to Register psy_bat\n", __func__);
			return 0;
		}
	}

	value.intval = res;
	ret = power_supply_set_property(psy_bat,
			(enum power_supply_property)POWER_SUPPLY_PROP_AFC_RESULT, &value);
	if (ret < 0)
		pr_err("%s: POWER_SUPPLY_EXT_PROP_AFC_RESULT set failed\n", __func__);
	/* hs14 code for P221214-05432 by wenyaqi at 2022/12/26 start */
	if (!IS_ERR_OR_NULL(psy_bat)) {
		power_supply_changed(psy_bat);
	}
	/* hs14 code for P221214-05432 by wenyaqi at 2022/12/26 end */
	return 0;
}

/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
static int afc_mode = 0x30;

static int __init set_charging_mode(char *str)
{
	int mode;
	get_option(&str, &mode);
	afc_mode = (mode & 0x0000FF00) >> 8;
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);

	return 0;
}
early_param("charging_mode", set_charging_mode);

int get_afc_mode(void)
{
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);
	return afc_mode;
}

/*uV*/
static inline u32 afc_get_vbus(void)
{
	return battery_get_vbus() * 1000;
}

/*uA*/
static inline u32 afc_get_ibat(void)
{
	return battery_get_bat_current() * 100;
}

/*mV*/
static inline u32 afc_get_vbat(void)
{
	return battery_get_bat_voltage();
}

static int afc_send_Mping(struct afc_dev *afc)
{
	int gpio = afc->afc_data_gpio;
	int ret = 0;
	unsigned long flags;
	s64 start = 0, end = 0;

	/* config gpio to output*/
	if (!afc->pin_state) {
		gpio_direction_output(gpio, 0);
		afc->pin_state = true;
	}

	/* AFC: send mping*/
	spin_lock_irqsave(&afc->afc_spin_lock, flags);
	gpio_set_value(gpio, 1);
	afc_udelay(AFC_T_MPING);
	gpio_set_value(gpio, 0);

	start = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&afc->afc_spin_lock, flags);
	end = ktime_to_us(ktime_get());

	ret = (int)end-start;
	return ret;
}

static int afc_check_Sping(struct afc_dev *afc ,int delay)
{
	int gpio = afc->afc_data_gpio;
	int ret = 0;
	s64 start =0, end = 0, duration = 0;
	unsigned long flags;

	if (delay > (AFC_T_MPING+3*AFC_T_UI)) {
		ret = delay - (AFC_T_MPING+3*AFC_T_UI);
		return ret;
	}

	/* config gpio to input*/
	if (afc->pin_state) {
		gpio_direction_input(gpio);
		afc->pin_state = false;
	}

	spin_lock_irqsave(&afc->afc_spin_lock, flags);
	if (delay < (AFC_T_MPING - 3*AFC_T_UI)) {
		afc_udelay(AFC_T_MPING - 3*AFC_T_UI - delay);
	}

	if (!gpio_get_value(gpio)) {
		ret = -EAGAIN;
		goto out;
	}

	start = ktime_to_us(ktime_get());
	while (gpio_get_value(gpio)) {
		duration = ktime_to_us(ktime_get()) -start;
		if (duration > AFC_T_DATA) {
			break;
		}
		afc_udelay(AFC_T_UI);
	}

out:
	start = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&afc->afc_spin_lock, flags);
	end = ktime_to_us(ktime_get());

	if (ret == 0) {
		ret = (int)end-start;
	}

	return ret;
}

static void afc_send_parity_bit(int gpio, int data)
{
	int cnt = 0;

	for (; data > 0; data = data >> 1) {
		if (data & 0x1)
			cnt++;
	}

	cnt = cnt % 2;

	gpio_set_value(gpio, !cnt);
	afc_udelay(AFC_T_UI);

	if (!cnt) {
		gpio_set_value(gpio, 0);
		afc_udelay(AFC_T_SYNC_PULSE);
	}
}

static void afc_sync_pulse(int gpio)
{
	gpio_set_value(gpio, 1);
	afc_udelay(AFC_T_SYNC_PULSE);
	gpio_set_value(gpio, 0);
	afc_udelay(AFC_T_SYNC_PULSE);
}

static int afc_send_data(struct afc_dev *afc, int data)
{
	int i = 0, ret = 0;
	int gpio = afc->afc_data_gpio;
	s64 start = 0, end = 0;
	unsigned long flags;

	if (!afc->pin_state) {
		gpio_direction_output(gpio, 0);
		afc->pin_state = true;
	}

	spin_lock_irqsave(&afc->afc_spin_lock, flags);

	afc_udelay(AFC_T_UI);

	/* start transfer */
	afc_sync_pulse(gpio);
	if (!(data & 0x80)) {
		gpio_set_value(gpio, 1);
		afc_udelay(AFC_T_SYNC_PULSE);
	}
	/* data */
	for (i = 0x80; i > 0; i = i >> 1) {
		gpio_set_value(gpio, data & i);
		afc_udelay(AFC_T_UI);
	}

	/* parity */
	afc_send_parity_bit(gpio, data);
	afc_sync_pulse(gpio);

	/* mping*/
	gpio_set_value(gpio, 1);
	afc_udelay(AFC_T_MPING);
	gpio_set_value(gpio, 0);

	start = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&afc->afc_spin_lock, flags);
	end = ktime_to_us(ktime_get());

	ret = (int)end-start;

	return ret;
}

int afc_recv_data(struct afc_dev *afc, int delay)
{
	int gpio = afc->afc_data_gpio;
	int ret = 0, gpio_value = 0, reset = 1;
	s64 limit_start = 0,  start = 0, end = 0, duration = 0;
	unsigned long flags;

	if (afc->pin_state) {
		gpio_direction_input(gpio);
		afc->pin_state = false;
	}

	if (delay > AFC_T_DATA+AFC_T_MPING) {
		ret = -EAGAIN;
	} else if (delay > AFC_T_DATA && delay <= AFC_T_DATA+AFC_T_MPING) {
		afc_check_Sping(afc, delay-AFC_T_DATA);
	} else if (delay <= AFC_T_DATA) {
		spin_lock_irqsave(&afc->afc_spin_lock, flags);
		afc_udelay(AFC_T_DATA-delay);
		limit_start = ktime_to_us(ktime_get());
		while (duration < AFC_T_MPING-3*AFC_T_UI) {
			if (reset) {
				start = 0;
				end = 0;
				duration = 0;
			}
			gpio_value = gpio_get_value(gpio);
			if (!gpio_value && !reset) {
				end = ktime_to_us(ktime_get());
				duration = end -start;
				reset = 1;
			} else if (gpio_value) {
				if (reset) {
					start = ktime_to_us(ktime_get());
					reset = 0;
				}
			}
			afc_udelay(AFC_T_UI);
			if (ktime_to_us(ktime_get()) - limit_start > (AFC_T_MPING+AFC_T_DATA*2)) {
				ret = -EAGAIN;
				break;
			}
		}
		spin_unlock_irqrestore(&afc->afc_spin_lock, flags);
	}
	return ret;
}

int afc_set_voltage(struct afc_dev *afc, u32 voltage)
{
	int ret = 0;

	afc->vol  = voltage;

	pr_info("%s - Start\n", __func__);

	gpio_direction_output(afc->afc_switch_gpio, 1);

	ret = afc_send_Mping(afc);
	ret = afc_check_Sping(afc ,ret);
	if (ret < 0) {
		pr_err("AFC: start mping NACK\n");
		goto out;
	}

	if (afc->vol == SET_9V) {
		ret = afc_send_data(afc, 0x46);
	} else {
		ret = afc_send_data(afc, 0x08);
	}

	ret = afc_check_Sping(afc ,ret);
	if (ret < 0) {
		pr_err("AFC: sping err2 %d", ret);
	}

	ret = afc_recv_data(afc, ret);
	if (ret < 0) {
		pr_err("AFC: sping err3 %d", ret);
	}

	ret = afc_send_Mping(afc);
	ret = afc_check_Sping(afc ,ret);
	if (ret < 0) {
		pr_err("AFC: End Mping NACK");
	}

out:
	gpio_direction_output(afc->afc_switch_gpio, 0);

	pr_info("%s - End\n", __func__);

	return ret;
}

/*
* cancel_afc
*
* if PDC/PD is ready , pls quit AFC detect
*/
static bool cancel_afc(struct charger_manager *pinfo)
{
	if (pinfo->chr_type == STANDARD_CHARGER &&
		(pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK ||	//PD2.0
		pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 ||	//PD3.0
		pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO))	//PPS
		return true;
	return false;
}


/* Enable/Disable HW & SW VBUS OVP */
static int afc_enable_vbus_ovp(struct charger_manager *pinfo, bool enable)
{
	int ret = 0;

	/* Enable/Disable HW(PMIC) OVP */
	ret = pmic_enable_hw_vbus_ovp(enable);
	if (ret < 0) {
		chr_err("%s: failed, ret = %d\n", __func__, ret);
		return ret;
	}

	charger_enable_vbus_ovp(pinfo, enable);

	return ret;
}

static int afc_set_mivr(struct charger_manager *pinfo, int uV)
{
	int ret = 0;

	ret = charger_dev_set_mivr(pinfo->chg1_dev, uV);
	if (ret < 0) {
		pr_err("%s: failed, ret = %d\n", __func__, ret);
	}

	return ret;
}

/*
* afc_plugout_reset
*
* When detect usb plugout, pls reset AFC configure Enable OVP(?)
* and revert MIVR
*/
int afc_plugout_reset(struct charger_manager *pinfo)
{
	int ret = 0;
	struct afc_dev *afc_device = &pinfo->afc;

	pr_info("%s \n", __func__);

	afc_device->is_connect = false;
	afc_device->to_check_chr_type = true;
	afc_device->afc_loop_algo = 0;
	afc_device->afc_retry_flag = false;

	/* Enable OVP */
	ret = afc_enable_vbus_ovp(pinfo, true);
	if (ret < 0) {
		goto err;
	}

	/* Set MIVR for vbus 5V */
	ret = afc_set_mivr(pinfo, pinfo->data.min_charger_voltage); /* uV */
	if (ret < 0) {
		goto err;
	}

	pr_info("%s: OK\n", __func__);

	return ret;

err:
	pr_err("%s: failed, ret = %d\n", __func__, ret);
	return ret;
}

/*
* afc_get_is_connect
*
* Note:Cable out is occurred,but not execute plugout_reset yet
* so It should check is_cable_out_occur
*/
bool afc_get_is_connect(struct charger_manager *pinfo)
{
	return pinfo->afc.is_connect;
}

bool afc_get_is_enable(struct charger_manager *pinfo)
{
	return pinfo->afc.is_enable;
}

/*
* afc_set_is_enable
*
* Note: In case of Plugin/out usb in sleep mode, need to stay awake.
*/
void afc_set_is_enable(struct charger_manager *pinfo, bool enable)
{

	pr_info("%s: enable = %d\n", __func__, enable);

	if (!pinfo->afc.afc_init_ok) {
		pr_err("%s: stop, afc_init failed\n", __func__);
		return;
	}

	mutex_lock(&pinfo->afc.access_lock);
	__pm_stay_awake(pinfo->afc.suspend_lock);
	pinfo->afc.is_enable = enable;
	__pm_relax(pinfo->afc.suspend_lock);
	mutex_unlock(&pinfo->afc.access_lock);
}

/*
* afc_reset
*
* Reset TA's charger configure
* Note : replace udelay with mdelay
*/
void afc_reset(struct afc_dev *afc)
{
	if (!afc->pin_state) {
		gpio_direction_output(afc->afc_data_gpio, 0);
		afc->pin_state = true;
	}

	spin_lock_irq(&afc->afc_spin_lock);

	gpio_set_value(afc->afc_data_gpio, 1);
	afc_udelay(AFC_T_RESET);
	gpio_set_value(afc->afc_data_gpio, 0);

	spin_unlock_irq(&afc->afc_spin_lock);
	gpio_direction_output(afc->afc_switch_gpio, 0);
}

int afc_5v_to_9v(struct charger_manager *pinfo)
{
	int ret = 0;
	int pre_vbus = afc_get_vbus();

	pr_info("%s: Start \n", __func__);

	if (abs(pre_vbus - SET_9V) < 1000000) {
		pr_info("%s:Vbus was already %d\n",__func__,pre_vbus);
		return ret;
	}

	ret = afc_enable_vbus_ovp(pinfo, false);
	if (ret) {
		pr_err("%s: disable ovp failed!\n",__func__);
		return ret;
	}

/*
	ret = charger_dev_set_input_current(pinfo->chg1_dev, pinfo->data.afc_pre_input_current);
	if (ret < 0) {
		pr_err("%s: set AICR failed!\n",__func__);
	}
*/
	__pm_stay_awake(pinfo->afc.suspend_lock);
	ret = afc_set_ta_vchr(pinfo, SET_9V);
	__pm_relax(pinfo->afc.suspend_lock);
	if (ret == 0) {
		afc_set_mivr(pinfo, 8200000);
	} else {
		afc_set_mivr(pinfo, 4200000);
		afc_enable_vbus_ovp(pinfo, true);
		pr_err("%s: 5v to 9v failed !\n",__func__);
	}

	return ret;
}

int afc_9v_to_5v(struct charger_manager *pinfo)
{
	int ret = 0;
	int pre_vbus = afc_get_vbus();

	if (abs(pre_vbus - SET_5V) < 1000000) {
		pr_info("%s:Vbus was already %d\n",__func__,pre_vbus);
		return ret;
	}

	__pm_stay_awake(pinfo->afc.suspend_lock);
	ret = afc_set_ta_vchr(pinfo, SET_5V);
	__pm_relax(pinfo->afc.suspend_lock);
	if (ret == 0) {
		afc_enable_vbus_ovp(pinfo, true);
		afc_set_mivr(pinfo, 4200000);
	} else {
		afc_set_mivr(pinfo, 8200000);
		pr_err("%s: 9v to 5v failed !\n",__func__);
	}

	return ret;
}

/*
* afc_reset_ta_vchr
*
* reset AT configure and check the result
*/
int afc_reset_ta_vchr(struct charger_manager *pinfo)
{
	int chr_volt = 0;
	u32 retry_cnt = 0;
	struct afc_dev *afc_device = &pinfo->afc;
	struct power_supply *psy = power_supply_get_by_name("battery");

	pr_info("%s: Start \n", __func__);
	if (!afc_device->afc_init_ok) {
		pr_err("afc init failed\n");
		return -1;
	}

	afc_set_mivr(pinfo, pinfo->data.min_charger_voltage);
	do {
/* Check charger's voltage */
		chr_volt = afc_get_vbus();
		if (abs(chr_volt - afc_device->ta_vchr_org) <= 1000000) {
			mutex_lock(&afc_device->access_lock);
			afc_device->is_connect = false;
			afc_device->afc_loop_algo = 0;
			afc_device->afc_retry_flag = false;
			mutex_unlock(&afc_device->access_lock);
			if (!IS_ERR_OR_NULL(psy)) {
				power_supply_changed(psy);
			}
			break;
		}
/* Reset TA's charger configure */
		afc_reset(afc_device);
		msleep(38);
		retry_cnt++;
		pr_info("%s: vbus = %d \n", __func__, (chr_volt / 1000));
	} while (retry_cnt < 3);

	if (afc_device->is_connect) {
		afc_set_mivr(pinfo, 8200000);
		pr_err("%s: failed\n", __func__);
		return -1;
	}

	pr_info("%s: OK \n", __func__);
	return 0;
}

/*
* afc_leave
*
* Exit AFC charge mode, need revert charger ic and
* reset TA
*/
int afc_leave(struct charger_manager *pinfo)
{
	int ret = 0;
	struct afc_dev *afc_device = &pinfo->afc;

	pr_info("%s: Starts\n", __func__);

	if (!afc_device->afc_init_ok) {
		pr_err("%s: stop, afc init failed \n", __func__);
		return ret;
	}

	ret = afc_reset_ta_vchr(pinfo);
	if (ret < 0) {
/*if reset TA fail , pls revert charger IC configure (AICR)*/
		charger_dev_set_input_current(pinfo->chg1_dev, pinfo->data.afc_pre_input_current);
		pr_err("%s: failed, is_connect = %d, ret = %d\n",
			__func__, afc_device->is_connect, ret);
		return ret;
	}

	afc_enable_vbus_ovp(pinfo, true);
	pr_info("%s: OK\n", __func__);
	return ret;
}

/*
* afc_set_charging_current
*
* if AFC is not contect , pls don`t config AIRC and Ichg
* Note: Pls configure suit AICR or TA is Crash !!
*/
int afc_set_charging_current(struct charger_manager *pinfo)
{
	int ret = 0;
	int vchr;
	struct afc_dev *afc_device = &pinfo->afc;
	struct charger_data *pdata;

	if (!afc_device->is_connect)
		return -ENOTSUPP;

	pdata = &pinfo->chg1_data;
	vchr = afc_get_vbus();
	if (abs(vchr - SET_9V) < 1000000) {
		pdata->input_current_limit = pinfo->data.afc_charger_input_current;
		pdata->charging_current_limit = pinfo->data.afc_charger_current;
	} else {
		pdata->input_current_limit = pinfo->data.ac_charger_input_current;
		pdata->charging_current_limit = pinfo->data.ac_charger_current;
	}

	pr_info("%s: ICHG = %dmA, AICR = %dmA\n", __func__,
		pdata->charging_current_limit / 1000, pdata->input_current_limit / 1000);
	return ret;
}

/*
* afc_set_to_check_chr_type
*
* Set to_check_chr_type value if hv_charge is disable and afc leave
* Set to_check_chr_type value if quit limit current
*/
void afc_set_to_check_chr_type(struct charger_manager *pinfo, bool check)
{
	pr_info("%s: check = %d\n", __func__, check);

	if (!pinfo->afc.afc_init_ok) {
		pr_err("%s: stop, afc init failed \n", __func__);
		return;
	}

	mutex_lock(&pinfo->afc.access_lock);
	pinfo->afc.to_check_chr_type = check;
	mutex_unlock(&pinfo->afc.access_lock);
}


/*
* afc_check_charger
*
* The function will be executed is CC step.
*/
int afc_pre_check(struct charger_manager *pinfo)
{
	int chr_type;
	struct afc_dev *afc_device = &pinfo->afc;

	chr_type = mt_get_charger_type();
	if (!afc_device->to_check_chr_type ||
		chr_type != STANDARD_CHARGER) {
		pr_err("%s: stop, to_check_chr_type = %d, chr_type = %d \n",
			__func__, afc_device->to_check_chr_type, chr_type);
		return 1;
	}

	if (cancel_afc(pinfo) == true) {
		pr_err("%s: stop, cancel \n", __func__);
		return 1;
	}

	/* revise for HW TA to prevent current shaking*/
	charger_dev_set_input_current(pinfo->chg1_dev,
		pinfo->data.ac_charger_input_current);
	msleep(1500);

	return 0;
}

int afc_check_charger(struct charger_manager *pinfo)
{
	int ret = -1;
	struct afc_dev *afc_device = &pinfo->afc;
	struct power_supply *psy = power_supply_get_by_name("battery");

	pr_info("%s: Start\n", __func__);

	if (!pinfo->enable_hv_charging) {
		pr_err("%s: hv charging is disabled\n", __func__);
		if (afc_device->is_connect) {
			afc_leave(pinfo);
			mutex_lock(&afc_device->access_lock);
			afc_device->to_check_chr_type = true;
			mutex_unlock(&afc_device->access_lock);
		}
		return ret;
	}

	if (cancel_afc(pinfo) == true) {
		pr_err("%s: stop, cancel \n", __func__);
		return ret;
	}

	if (!afc_device->afc_init_ok) {
		pr_err("%s: stop, afc init failed \n", __func__);
		return ret;
	}

	if (!afc_device->is_enable || !pinfo->enable_afc) {
		pr_err("%s: stop, afc is disabled\n", __func__);
		return ret;
	}

	if (afc_pre_check(pinfo)) {
		return ret;
	}

#if defined(CONFIG_DRV_SAMSUNG)
	if (afc_device->afc_disable) {
		pr_info("%s: disabled by USER\n", __func__);
		return ret;
	}
#endif

	afc_enable_vbus_ovp(pinfo, false);
	mutex_lock(&pinfo->afc.access_lock);
	__pm_stay_awake(pinfo->afc.suspend_lock);
	/* revise for HW TA to prevent current shaking*/
	/*Note: It need to set pre-ARIC, or TA will Crash!!,
	The AICR will be configured right siut value in CC step */
	charger_dev_set_input_current(pinfo->chg1_dev, pinfo->data.ac_charger_input_current);
	ret = afc_set_ta_vchr(pinfo, SET_9V);
	if (ret == 0) {
		ret = afc_set_mivr(pinfo, afc_device->vol - 1000000);
	} else {
		goto out;
	}

	afc_device->to_check_chr_type = false;
	afc_device->is_connect = true;
	__pm_relax(pinfo->afc.suspend_lock);
	mutex_unlock(&pinfo->afc.access_lock);
	if (!IS_ERR_OR_NULL(psy)) {
		power_supply_changed(psy);
	}
	pr_info("%s End\n", __func__);
	return 0;
out:
	afc_device->to_check_chr_type = false;
	afc_device->is_connect = false;
	__pm_relax(pinfo->afc.suspend_lock);
	mutex_unlock(&pinfo->afc.access_lock);
	if (!IS_ERR_OR_NULL(psy)) {
		power_supply_changed(psy);
	}
	afc_enable_vbus_ovp(pinfo, true);
	charger_dev_set_input_current(pinfo->chg1_dev, pinfo->data.ac_charger_input_current);
	pr_info("%s:not detect afc\n",__func__);
	return ret;
}

/*
* afc_set_ta_vchr
*
* set TA voltage and check the result
* Note :  set TA voltage fristly, then set MIVR/AICR
*/
int afc_set_ta_vchr(struct charger_manager *pinfo, u32 chr_volt)
{
	int ret = 0;
	int vchr_before, vchr_after, vchr_delta;
	const u32 sw_retry_cnt_max = 3;
	const u32 retry_cnt_max = 5;
	u32 sw_retry_cnt = 0, retry_cnt = 0;

	vchr_before = afc_get_vbus();
	do {
		for (sw_retry_cnt = 0; sw_retry_cnt < sw_retry_cnt_max; sw_retry_cnt++) {
			ret = afc_set_voltage(&pinfo->afc, chr_volt);
			mdelay(20);
			vchr_after = afc_get_vbus();
			vchr_delta = abs(vchr_after - chr_volt);
			if (vchr_delta < 1000000) {
				break;
			}
		}
		mdelay(50);
		/*
		 * It is successful if VBUS difference to target is
		 * less than 1000mV.
		 */
		if (ret >= 0 && vchr_delta < 1000000) {
			pr_err("%s: OK, vchr = (%d, %d), vchr_target = %dmV\n",
				__func__, vchr_before / 1000, vchr_after / 1000,
				chr_volt / 1000);
			goto send_result;
		}
		afc_set_mivr(pinfo, pinfo->data.afc_min_charger_voltage);
		pr_info("%s: retry_cnt = (%d, %d), vchr = (%d, %d), vchr_target = %dmV\n",
			__func__, sw_retry_cnt, retry_cnt, vchr_before / 1000,
			vchr_after / 1000, chr_volt / 1000);
	} while (mt_get_charger_type() != CHARGER_UNKNOWN &&
		 retry_cnt++ < retry_cnt_max && pinfo->enable_hv_charging &&
		 cancel_afc(pinfo) != true);

	ret = -EIO;
	pr_err("%s: failed, vchr_after = %dmV, target_vchr = %dmV\n",
		__func__, vchr_after / 1000, chr_volt / 1000);
	send_afc_result(AFC_FAIL);

	return ret;

send_result:
	if ((chr_volt == SET_5V) && (vchr_after < 5500000)) {
		pr_debug("%s, Succeed AFC 5V\n", __func__);
		send_afc_result(AFC_5V);
	} else if ((chr_volt == SET_9V) && (vchr_after > 8000000)) {
		pr_debug("%s, Succeed AFC 9V\n", __func__);
		send_afc_result(AFC_9V);
	} else {
		pr_debug("%s, Failed AFC\n", __func__);
		send_afc_result(AFC_FAIL);
	}
	return 0;
}

#if 0
static int afc_check_leave_status(struct charger_manager *pinfo)
{
	int ret = 0;
	int ichg = 0, vchr = 0;
	struct afc_dev *afc_device = &pinfo->afc;

	pr_err("%s: starts\n", __func__);

	/* AFC leaves unexpectedly */

	vchr = afc_get_vbus();
	if (abs(vchr - afc_device->ta_vchr_org) < 1000000) {
		pr_err("%s: AFC leave unexpectedly, recheck TA\n", __func__);
		afc_device->to_check_chr_type = true;
		ret = afc_leave(pinfo);
		if (ret < 0 || afc_device->is_connect)
			goto _err;
		return ret;
	}

	ichg = afc_get_ibat();
	/* Check SOC & Ichg */
	if (battery_get_uisoc() > pinfo->data.afc_stop_battery_soc &&
		ichg > 0 && ichg < pinfo->data.afc_ichg_level_threshold) {
		ret = afc_leave(pinfo);
		if (ret < 0 || afc_device->is_connect)
			goto _err;
		pr_err("%s: OK, SOC = (%d,%d), stop AFC\n", __func__,
			battery_get_uisoc(), pinfo->data.afc_stop_battery_soc);
	}
	return ret;

_err:
	pr_err("%s: failed, is_connect = %d, ret = %d\n",__func__, afc_device->is_connect, ret);
	return ret;
}
#endif

/*
* afc_start_algorithm
*
* Note: 1. judge more cases before set TA voltage
*	   2. Exit AFC mode if appropriate
*/
int afc_start_algorithm(struct charger_manager *pinfo)
{
	int ret = 1;
	int pre_vbus = 0 , ichg = 0;
	int vbat = 0;
	struct afc_dev *afc_device = &pinfo->afc;

	if (!pinfo->enable_hv_charging) {
		ret = -EPERM;
		pr_err("%s: hv charging is disabled\n", __func__);
		if (afc_device->is_connect) {
			afc_leave(pinfo);
			mutex_lock(&afc_device->access_lock);
			afc_device->to_check_chr_type = true;
			mutex_unlock(&afc_device->access_lock);
		}
		return ret;
	}

	if (!afc_device->is_enable) {
		ret = -EPERM;
		pr_err("%s: stop, AFC is disabled\n",__func__);
		return ret;
	}

	if (!afc_device->is_connect) {
		ret = -EIO;
		pr_err("%s: stop,AFC is not connected\n",__func__);
		return ret;
	}

	/* Check SOC & Ichg */
	pre_vbus = afc_get_vbus();
	ichg = afc_get_ibat();
	vbat = afc_get_vbat();
	if ((abs(pre_vbus - SET_9V) < 1000000) && ichg > 0 &&
		battery_get_uisoc() > pinfo->data.afc_stop_battery_soc &&
		ichg < pinfo->data.afc_ichg_level_threshold &&
		vbat > 4200) {
		ret = afc_9v_to_5v(pinfo);
		if (ret < 0) {
			pr_err("%s: afc_9v_to_5v failed !\n",__func__);
		}
	}

	pr_err("%s: SOC = %d, is_connect = %d\n",
		__func__, battery_get_uisoc(), afc_device->is_connect);

	return ret;
}

static ssize_t show_afc_toggle(struct device *dev, struct device_attribute *attr,
						   char *buf)
{
	struct charger_manager *pinfo = dev->driver_data;

	pr_info("%s: %d\n", __func__, pinfo->afc.is_connect);
	return sprintf(buf, "%d\n", pinfo->afc.is_connect);
}

static ssize_t store_afc_toggle(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct charger_manager *pinfo = dev->driver_data;
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp == 0)
			afc_leave(pinfo);
		else
			afc_5v_to_9v(pinfo);
	} else {
		pr_err("%s: format error!\n", __func__);
	}

	return size;
}

static DEVICE_ATTR(afc_toggle, 0664, show_afc_toggle,
		   store_afc_toggle);
/* sw jeita end*/

#if defined(CONFIG_DRV_SAMSUNG)
static ssize_t afc_disable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct afc_dev *afc = dev_get_drvdata(dev);

	if (afc->afc_disable) {
		pr_info("%s AFC DISABLE\n", __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s AFC ENABLE", __func__);
	return sprintf(buf, "0\n");
}

static ssize_t afc_disable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct afc_dev *afc = dev_get_drvdata(dev);
	int param_val, ret = 0;
	union power_supply_propval psy_val;
	struct power_supply *psy = power_supply_get_by_name("battery");

	if (!strncasecmp(buf, "1", 1)) {
		afc->afc_disable = true;
	} else if (!strncasecmp(buf, "0", 1)) {
		afc->afc_disable = false;
	} else {
		pr_warn("%s invalid value\n", __func__);

		return count;
	}

	param_val = afc->afc_disable ? '1' : '0';

#if defined(CONFIG_SEC_PARAM)
	ret = sec_set_param(CM_OFFSET + 1, afc->afc_disable ? '1' : '0');
	if (ret < 0)
		pr_err("%s:sec_set_param failed\n", __func__);
#endif

	psy_val.intval = afc->afc_disable;
	if (!IS_ERR_OR_NULL(psy)) {
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_HV_DISABLE, &psy_val);
		if (ret < 0) {
			pr_err("%s: Fail to set POWER_SUPPLY_PROP_HV_DISABLE (%d)\n", __func__, ret);
		}
	} else {
		pr_err("%s: power_supply battery is NULL\n", __func__);
	}

	pr_info("%s afc_disable(%d)\n", __func__, afc->afc_disable);

	return count;
}

static DEVICE_ATTR_RW(afc_disable);
#endif

int is_afc_result(struct charger_manager *pinfo, int result)
{
	if (pinfo->chr_type != STANDARD_CHARGER) {
		pr_info("cable is not DCP OR AFC %d\n", result);
		return 0;
	}
	pr_info("is_afc_result = %d, before afc_sts(%d)\n", result, pinfo->afc_sts);
	pinfo->afc_sts = result;

	if ((result == NOT_AFC) || (result == AFC_FAIL))  {
		if (pinfo->chr_type == STANDARD_CHARGER) {
			pr_info("afc_set_voltage() failed\n");
		}
		else{
			pr_info("AFC failed, re-detect PE or PD\n");
		}
	} else if (result == AFC_5V) {
		pr_info("afc set to 5V\n");
	} else if (result == AFC_9V) {
		pr_info("afc set to 9V\n");
	} else if (result == AFC_DISABLE) {
		pr_info("afc disable\n");
	}

	return 0;
}

static void afc_charger_parse_dt(struct charger_manager *pinfo,
	struct device_node *np)
{
	u32 val = 0;

	if (of_property_read_u32(np, "afc_start_battery_soc", &val) >= 0)
		pinfo->data.afc_start_battery_soc = val;
	else {
		pr_err("use default AFC_START_BATTERY_SOC:%d\n",
			AFC_START_BATTERY_SOC);
		pinfo->data.afc_start_battery_soc = AFC_START_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "afc_stop_battery_soc", &val) >= 0)
		pinfo->data.afc_stop_battery_soc = val;
	else {
		pr_err("use default AFC_STOP_BATTERY_SOC:%d\n",
			AFC_STOP_BATTERY_SOC);
		pinfo->data.afc_stop_battery_soc = AFC_STOP_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "afc_pre_input_current", &val) >= 0)
		pinfo->data.afc_pre_input_current = val;
	else {
		pr_err("use default AFC_PRE_INPUT_CURRENT:%d\n",
			AFC_PRE_INPUT_CURRENT);
		pinfo->data.afc_pre_input_current = AFC_PRE_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "afc_charger_input_current", &val) >= 0)
		pinfo->data.afc_charger_input_current = val;
	else {
		pr_err("use default AFC_CHARGER_INPUT_CURRENT:%d\n",
			AFC_CHARGER_INPUT_CURRENT);
		pinfo->data.afc_charger_input_current = AFC_CHARGER_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "afc_charger_current", &val) >= 0)
		pinfo->data.afc_charger_current = val;
	else {
		pr_err("use default AFC_CHARGER_CURRENT:%d\n",
			AFC_CHARGER_CURRENT);
		pinfo->data.afc_charger_current = AFC_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "afc_ichg_level_threshold", &val) >= 0)
		pinfo->data.afc_ichg_level_threshold = val;
	else {
		pr_err("use default AFC_ICHG_LEVEL_THRESHOLD:%d\n",
			AFC_ICHG_LEVEL_THRESHOLD);
		pinfo->data.afc_ichg_level_threshold = AFC_ICHG_LEVEL_THRESHOLD;
	}

	if (of_property_read_u32(np, "afc_min_charger_voltage", &val) >= 0)
		pinfo->data.afc_min_charger_voltage = val;
	else {
		pr_err("use default AFC_MIN_CHARGER_VOLTAGE:%d\n",
			AFC_MIN_CHARGER_VOLTAGE);
		pinfo->data.afc_min_charger_voltage = AFC_MIN_CHARGER_VOLTAGE;
	}

	if (of_property_read_u32(np, "afc_max_charger_voltage", &val) >= 0)
		pinfo->data.afc_max_charger_voltage = val;
	else {
		pr_err("use default AFC_MAX_CHARGER_VOLTAGE:%d\n",
			AFC_MAX_CHARGER_VOLTAGE);
		pinfo->data.afc_max_charger_voltage = AFC_MAX_CHARGER_VOLTAGE;
	}
}

int afc_charge_init(struct charger_manager *pinfo)
{
	struct platform_device *pdev = pinfo->pdev;
	struct device *dev = &pdev->dev;
	struct device_node *np= dev->of_node;
	struct afc_dev *afc = &pinfo->afc;
	int ret = 0;
#if defined(CONFIG_DRV_SAMSUNG)
	union power_supply_propval psy_val;
	struct power_supply *psy = power_supply_get_by_name("battery");
#endif

	pr_info("%s: Start\n", __func__);
	if (!afc) {
		pr_err("%s, afc allocation fail, just return!\n", __func__);
		return -ENOMEM;
	}

	ret = of_get_named_gpio(np,"afc_switch_gpio", 0);
	if (ret < 0) {
		pr_err("%s, could not get afc_switch_gpio\n", __func__);
		return ret;
	} else {
		afc->afc_switch_gpio = ret;
		pr_info("%s, afc->afc_switch_gpio: %d\n", __func__, afc->afc_switch_gpio);
		ret = gpio_request(afc->afc_switch_gpio, "afc_switch_gpio");
		if (ret < 0) {
			pr_err("%s failed to request afc switch gpio(%d)\n",
						__func__, ret);
			return ret;
		}
	}

	ret = of_get_named_gpio(np,"afc_data_gpio", 0);
	if (ret < 0) {
		pr_err("%s, could not get afc_data_gpio\n", __func__);
		return ret;
	} else {
		afc->afc_data_gpio = ret;
		pr_info("%s, afc->afc_data_gpio: %d\n", __func__, afc->afc_data_gpio);
		ret = gpio_request(afc->afc_data_gpio, "afc_data_gpio");
		if (ret < 0) {
			pr_err("%s failed to request afc data gpio(%d)\n",
						__func__, ret);
			return ret;
		}
	}

	afc_charger_parse_dt(pinfo, np);

	afc->ta_vchr_org = 5000000;
	afc->vol = pinfo->data.afc_max_charger_voltage;
	afc->to_check_chr_type = true;
	afc->is_enable = true;
	afc->is_connect = false;
	afc->pin_state = false;
	afc->afc_init_ok = true;
	afc->afc_error = 0;
	afc->afc_loop_algo = 0;
	afc->afc_retry_flag = false;
#if defined(CONFIG_DRV_SAMSUNG)
	afc->afc_disable = (get_afc_mode() == '1' ? 1 : 0);
	psy_val.intval = afc->afc_disable;
	if (!IS_ERR_OR_NULL(psy)) {
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_HV_DISABLE, &psy_val);
		if (ret < 0) {
			pr_err("%s: Fail to set POWER_SUPPLY_PROP_HV_DISABLE (%d)\n", __func__, ret);
		}
	} else {
		pr_err("%s: power_supply battery is NULL\n", __func__);
	}

	afc->switch_device = sec_device_create(afc, "switch");
	if (IS_ERR(afc->switch_device)) {
		pr_err("%s Failed to create device(switch)!\n", __func__);
		return -ENODEV;
	}
	ret = sysfs_create_file(&afc->switch_device->kobj,
			&dev_attr_afc_disable.attr);
	if (ret) {
		pr_err("%s Failed to create afc_disable sysfs\n", __func__);
		return ret;
	}
#endif
	//kernel-4.14
	//wakeup_source_init(pinfo->afc.suspend_lock, "AFC suspend wakelock");
	//kernel-4.19
	pinfo->afc.suspend_lock = wakeup_source_register(NULL, "AFC suspend wakelock");
	spin_lock_init(&afc->afc_spin_lock);
	mutex_init(&afc->access_lock);

	ret = device_create_file(&(pdev->dev), &dev_attr_afc_toggle);
	if (ret)
		pr_err("sysfs afc_toggle create failed\n");

	pr_info("%s: end \n", __func__);
	return 0;

}
/*
  * Release note
  * 20221108 for AL6528A-659
  * Mutex will be called in gpio_ direction_output, and sleep is not allowed in spinlock
  * Replace the gpio operation in the spinlock critical area with gpio_set_value
  */
/* hs14 code for SR-AL6528A-01-321 | AL6528ADEU-722 | AL6528A-659 by qiaodan at 2022/11/08 end */