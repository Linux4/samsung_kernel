/* gpio_afc.c
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
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/mutex.h>  //Bug516173,zhaosidong.wt,ADD,20191129,include header file
#include <linux/spinlock.h>
#include "mtk_charger.h"
#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif
#if defined(CONFIG_SEC_PARAM)
#include <linux/sec_ext.h>
#endif
//#include "mtk_charger.h"

int g_afc_work_status = 0;//Bug 518556,liuyong3.wt,ADD,20191128,Charging afc flag

//extern signed int battery_get_vbus(void);
//extern int pmic_get_battery_voltage(void);
//extern signed int battery_get_bat_current(void);
//extern int pmic_enable_hw_vbus_ovp(bool enable);
//extern int charger_enable_vbus_ovp(struct mtk_charger *pinfo, bool enable);
//extern int charger_dev_set_mivr(struct charger_device *chg_dev, u32 uV);

/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
static int afc_mode = 0;

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
	return afc_mode;
}
static int get_pmic_vbus(int *vchr)
{
	union power_supply_propval prop;
	static struct power_supply *chg_psy;
	int ret;

	if (chg_psy == NULL)
		chg_psy = power_supply_get_by_name("mtk_charger_type");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		pr_notice("%s Couldn't get chg_psy\n", __func__);
		ret = -1;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	}
	*vchr = prop.intval * 1000;

	pr_notice("%s vbus:%d\n", __func__,
		prop.intval);
	return ret;
}

/*uV*/
static inline u32 afc_get_vbus(struct afc_dev *afc_device)
{
	int ret = 0;
	int vchr = 0;
	struct mtk_charger *info;
	info = afc_device->info;

	if(!info)
	{
		pr_err("the afc had not init, unable get vbus\n");
		return -1;
	}
	ret = charger_dev_get_vbus(info->chg1_dev, &vchr);
	if (ret < 0) {
		ret = get_pmic_vbus(&vchr);
		if (ret < 0)
			pr_notice("%s: get vbus failed: %d\n", __func__, ret);
	}

	return vchr;
}

/*uA*/
static inline u32 afc_get_ibat(struct afc_dev *afc_device)
{

	union power_supply_propval prop;
	struct power_supply *bat_psy;

	bat_psy = afc_device->bat_psy;
	if (bat_psy == NULL)
	{
		bat_psy = power_supply_get_by_name("battery");
		if (bat_psy == NULL || IS_ERR(bat_psy))
		{
			pr_notice("%s Couldn't get bat_psy\n", __func__);
		}
	}
	if (bat_psy)
	{
		power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
//		prop.intval;
		if (prop.intval < 0)
			prop.intval = -prop.intval;
	}
	return prop.intval;

//	return battery_get_bat_current()*100;
}

static inline u32 afc_get_uisoc(struct afc_dev *afc_device)
{

	union power_supply_propval prop;
	struct power_supply *bat_psy;

	bat_psy = afc_device->bat_psy;
	if (bat_psy == NULL)
	{
		bat_psy = power_supply_get_by_name("battery");
		if (bat_psy == NULL || IS_ERR(bat_psy))
		{
			pr_notice("%s Couldn't get bat_psy\n", __func__);
		}
	}
	if (bat_psy)
	{
		power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
//		prop.intval;
	}
	return prop.intval;

//	return battery_get_bat_current()*100;
}
static inline u32 afc_get_vbat(struct afc_dev *afc_device)
{

	union power_supply_propval prop;
	struct power_supply *bat_psy;

	bat_psy = afc_device->bat_psy;
	if (bat_psy == NULL)
	{
		bat_psy = power_supply_get_by_name("battery");
		if (bat_psy == NULL || IS_ERR(bat_psy))
		{
			pr_notice("%s Couldn't get bat_psy\n", __func__);
		}
	}
	if (bat_psy)
	{
		power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
//		prop.intval;
	}
	return prop.intval;

//	return battery_get_bat_current()*100;
}void cycle(struct afc_dev *afc, int ui)
{
	if (!afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_active);
		afc->pin_state = true;
	}
	gpio_direction_output(afc->afc_data_gpio, 1);
	udelay(ui);
	gpio_direction_output(afc->afc_data_gpio, 0);
	udelay(ui);
}

void afc_send_Mping(struct afc_dev *afc)
{
	if (!afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_active);
		afc->pin_state = true;
	}

	gpio_direction_output(afc->afc_data_gpio, 1);
	udelay(16*UI);
	gpio_direction_output(afc->afc_data_gpio, 0);
}

int afc_recv_Sping(struct afc_dev *afc)
{
	int i = 0, sp_cnt = 0;

	if (afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_suspend);
		afc->pin_state = false;
	}
	gpio_direction_input(afc->afc_data_gpio);
	while (1)
	{
		udelay(UI);
		if (gpio_get_value(afc->afc_data_gpio)) {
			pr_debug("%s: wait rsp %d\n", __func__, i);
			break;
		}

		if (WAIT_SPING_COUNT < i++) {
			pr_err("%s: wait time out ! \n", __func__);
			goto Sping_err;
		}
	}

	do {
		if (SPING_MAX_UI < sp_cnt++) {
			goto Sping_err;
		}

		udelay(UI);
	} while (gpio_get_value(afc->afc_data_gpio));

	if (sp_cnt < SPING_MIN_UI) {
		goto Sping_err;
	}

	pr_debug("%s: len %d \n", __func__, sp_cnt);
	return 0;

Sping_err:  
	pr_err("%s: Err len %d \n", __func__, sp_cnt);
	return -1;
}

int afc_send_parity_bit(struct afc_dev *afc, int data)
{
	int cnt = 0, odd = 0;

	for (; data > 0; data = data >> 1) {
		if (data & 0x1)	
			cnt++;
	}

	odd = cnt % 2;

	if (!odd)
		gpio_direction_output(afc->afc_data_gpio, 1);
	else
		gpio_direction_output(afc->afc_data_gpio, 0);

	udelay(UI);

	return odd;
}

void afc_send_data(struct afc_dev *afc, int data)
{
	int i = 0;

	if (!afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_active);
		afc->pin_state = true;
	}

	udelay(160);

	if (data & 0x80)
		cycle(afc, UI/4);
	else {
		cycle(afc, UI/4);
		gpio_direction_output(afc->afc_data_gpio, 1);
		udelay(UI/4);
	}

	for (i = 0x80; i > 0; i = i >> 1)
	{
		gpio_direction_output(afc->afc_data_gpio, data & i);
		udelay(UI);
	}

	if (afc_send_parity_bit(afc, data)) {
		gpio_direction_output(afc->afc_data_gpio, 0);
		udelay(UI/4);
		cycle(afc, UI/4);
	} else {
		udelay(UI/4);
		cycle(afc, UI/4);
	}
}

int afc_recv_data(struct afc_dev *afc)
{
	int ret = 0;

	if (afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_suspend);
		afc->pin_state = false;
	}

	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_2;
		return -1;
	}

	mdelay(2);//+Bug 522575,zhaosidong.wt,ADD,20191216,must delay 2 ms

	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_3;
		return -1;
	}
	return 0;
}

int afc_communication(struct afc_dev *afc)
{
	int ret = 0;

	pr_info("%s - Start\n", __func__);

	spin_lock_irq(&afc->afc_spin_lock);
	gpio_direction_output(afc->afc_switch_gpio, 1);

	afc_send_Mping(afc);
	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_1;
		goto afc_fail;
	}

	if (afc->vol == SET_9V)
		afc_send_data(afc, 0x46);
	else
		afc_send_data(afc, 0x08);

	afc_send_Mping(afc);
	ret = afc_recv_data(afc);
	if (ret < 0)
		goto afc_fail;

	udelay(200);
	afc_send_Mping(afc);

	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_4;
		goto afc_fail;
	}

	gpio_direction_output(afc->afc_switch_gpio, 0);
	spin_unlock_irq(&afc->afc_spin_lock);
	pr_info("%s - End \n", __func__);

	return 0;

afc_fail:
	gpio_direction_output(afc->afc_switch_gpio, 0);
	spin_unlock_irq(&afc->afc_spin_lock);
	pr_err("%s, AFC communication has been failed\n", __func__);

	return -1;
}

/*
* cancel_afc
*
* if PDC/PD is ready , pls quit AFC detect
*/
/*
static bool cancel_afc(struct charger_manager *pinfo)
{
	if (mtk_pdc_check_charger(pinfo) || mtk_is_TA_support_pd_pps(pinfo) || mtk_pe20_get_is_connect(pinfo))
		return true;
	return false;
}
*/

/* Enable/Disable HW & SW VBUS OVP */
static int afc_enable_vbus_ovp(struct mtk_charger *pinfo, bool enable)
{
	int ret = 0;

	/* Enable/Disable HW(PMIC) OVP */
	ret = mtk_chg_enable_vbus_ovp(enable);
	if (ret < 0) {
		pr_err("%s: failed, ret = %d\n", __func__, ret);
		return ret;
	}

//	charger_enable_vbus_ovp(pinfo, enable);
	return ret;
}

static int afc_set_mivr(struct mtk_charger *pinfo, int uV)
{
	int ret = 0;
	
	ret = charger_dev_set_mivr(pinfo->chg1_dev, uV);
	if (ret < 0)
		pr_err("%s: failed, ret = %d\n", __func__, ret);
	return ret;
}

/*
* afc_plugout_reset
*
* When detect usb plugout, pls reset AFC configure Enable OVP(?)
* and revert MIVR
*/
static int afc_hw_plugout_reset(struct mtk_charger *pinfo)
{
	int ret = 0;
	struct afc_dev *afc_device = &pinfo->afc;

	pr_info("%s \n", __func__);

	afc_device->is_connect = false;
	afc_device->to_check_chr_type = true;

	/* Enable OVP */
	ret = afc_enable_vbus_ovp(pinfo, true);
	if (ret < 0)
		goto err;

	/* Set MIVR for vbus 5V */
	ret = afc_set_mivr(pinfo, pinfo->data.min_charger_voltage); /* uV */
	if (ret < 0)
		goto err;

	g_afc_work_status = 0;//Bug 518556,liuyong3.wt,ADD,20191128,Charging afc flag
	pr_info("%s: OK\n", __func__);
	return ret;

err:
	g_afc_work_status = 0;
	pr_err("%s: failed, ret = %d\n", __func__, ret);
	return ret;
}

/*
* afc_get_is_connect
*
* Note:Cable out is occurred,but not execute plugout_reset yet
* so It should check is_cable_out_occur
*/
bool afc_get_is_connect(struct mtk_charger *pinfo)
{
	return pinfo->afc.is_connect;
}

bool afc_get_is_enable(struct mtk_charger *pinfo)
{
	return pinfo->afc.is_enable;
}

/*
* afc_set_is_enable
*
* Note: In case of Plugin/out usb in sleep mode, need to stay awake. 
*/
void afc_set_is_enable(struct mtk_charger *pinfo, bool enable)
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
	spin_lock_irq(&afc->afc_spin_lock);
	gpio_direction_output(afc->afc_switch_gpio, 1);

	if (!afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_active);
		afc->pin_state = true;
	}

	gpio_direction_output(afc->afc_data_gpio, 1);
	mdelay((100*UI)/1000);
	gpio_direction_output(afc->afc_data_gpio, 0);

	gpio_direction_output(afc->afc_switch_gpio, 0);
	spin_unlock_irq(&afc->afc_spin_lock);

}
//+bug528143,zhaosidong.wt,ADD,20200109,afc enable/disable toggle
int afc_5v_to_9v(struct mtk_charger *pinfo)
{
	int ret = 0;
	struct afc_dev *afc_device;
	int pre_vbus;
	
	afc_device = &pinfo->afc;
	pre_vbus = afc_get_vbus(afc_device);

	if (abs(pre_vbus - SET_9V) < 1000000) {
		pr_info("%s:Vbus was already %d\n",__func__,pre_vbus);
		return ret;
	}

	ret = afc_enable_vbus_ovp(pinfo, false);
	if(ret) {
		pr_err("%s: disable ovp failed!\n",__func__);
		return ret;
	}

	ret = charger_dev_set_input_current(pinfo->chg1_dev, afc_device->afc_pre_input_current);
	if(ret < 0) {
		pr_err("%s: set AICR failed!\n",__func__);
	}

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

int afc_9v_to_5v(struct mtk_charger *pinfo)
{
	int ret = 0;
	int pre_vbus = afc_get_vbus(&pinfo->afc);

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
//-bug528143,zhaosidong.wt,ADD,20200109,afc enable/disable toggle

/*
* afc_reset_ta_vchr
*
* reset AT configure and check the result 
*/
int afc_reset_ta_vchr(struct mtk_charger *pinfo)
{
	int chr_volt = 0;
	u32 retry_cnt = 0;
	struct afc_dev *afc_device = &pinfo->afc;
	//EXTB P200225-00205,zhaosidong.wt,ADD,20200227, report fast charging state in time.
	struct power_supply *psy = power_supply_get_by_name("battery");

	pr_info("%s: Start \n", __func__);
	if(!afc_device->afc_init_ok) {
		pr_err("afc init failed\n");
		return -1;
	}

	afc_set_mivr(pinfo, pinfo->data.min_charger_voltage);
	do {
/* Check charger's voltage */
		chr_volt = afc_get_vbus(afc_device);
		if (abs(chr_volt - afc_device->ta_vchr_org) <= 1000000) {
			mutex_lock(&afc_device->access_lock);
			afc_device->is_connect = false;
			afc_device->afc_loop_algo = 0; //Bug 516173,zhaosidong.wt,ADD,20191205,afc detection
			mutex_unlock(&afc_device->access_lock);
			if(!IS_ERR_OR_NULL(psy)) {
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
int afc_leave(struct mtk_charger *pinfo)
{
	int ret = 0;
	struct afc_dev *afc_device = &pinfo->afc;

//+Bug 516173,zhaosidong.wt,ADD,20191205,afc detection
	pr_info("%s: Starts\n", __func__);

	if (!afc_device->afc_init_ok) {
		pr_err("%s: stop, afc init failed \n", __func__);
		return ret;
	}
//-Bug 516173,zhaosidong.wt,ADD,20191205,afc detection
	ret = afc_reset_ta_vchr(pinfo);
	if (ret < 0) {
/*if reset TA fail , pls revert charger IC configure (AICR)*/
		charger_dev_set_input_current(pinfo->chg1_dev, afc_device->afc_pre_input_current);
		pr_err("%s: failed, is_connect = %d, ret = %d\n",
			__func__, afc_device->is_connect, ret);
		return ret;
	}

//	g_afc_work_status = 0;//Bug 518556,liuyong3.wt,ADD,20191128,Charging afc flag
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
int afc_set_charging_current(struct mtk_charger *pinfo,
	unsigned int *ichg, unsigned int *aicr)
{
	int ret = 0;
	int vchr;
	struct afc_dev *afc_device = &pinfo->afc;

	if (!afc_device->is_connect)
		return -ENOTSUPP;

//bug533258,zhaosidong.wt,ADD,20200217,toggle between 9V/1.65A and 5V/2A
	vchr = afc_get_vbus(afc_device);
	if(abs(vchr - SET_9V) < 1000000) {
		*aicr = pinfo->afc.afc_charger_input_current;
		*ichg = pinfo->data.ac_charger_current;
	} else {
		*aicr = pinfo->data.ac_charger_input_current;
		*ichg = pinfo->data.ac_charger_current;
	}

	pr_info("%s: ichg = %dmA, AICR = %dmA\n",__func__, *ichg / 1000, *aicr / 1000);
	return ret;
}

static int afc_set_charger(struct chg_alg_device *alg)
{
//	struct mtk_pe20 *pe2;
	int ichg1_min = -1, aicr1_min = -1;
	int ret;
	struct afc_dev *afc_device;
	struct mtk_charger *pinfo;

	afc_device = dev_get_drvdata(&alg->dev);
	pinfo = afc_device->info;
//	charger_dev_set_input_current(pinfo->chg1_dev, pinfo->afc.afc_pre_input_current);

	if (afc_device->afc_limit_input_current == 0 ||
		afc_device->afc_limit_charge_current == 0) {
		pr_notice("input/charging current is 0, end afc\n");
		return -1;
	}
//attention if thermal limit input < 300, disable afc
/*
	afc_limit_input_current			//jeita config
	afc_limit_charge_current		//jeita config
	afc_charger_input_current;		//dts max config
	afc_charger_current;			//dts max config
	afc_setting_input_current		//final setting
	afc_setting_charge_current		//final setting
*/
	mutex_lock(&afc_device->data_lock);
	if (afc_device->afc_limit_charge_current != -1) {
		if (afc_device->afc_limit_charge_current < afc_device->afc_charger_current)
			afc_device->afc_setting_charge_current = afc_device->afc_limit_charge_current;
		else
			afc_device->afc_setting_charge_current = afc_device->afc_charger_current;
		ret = charger_dev_get_min_charging_current(pinfo->chg1_dev, &ichg1_min);
		if (ret != -ENOTSUPP &&
			afc_device->afc_limit_charge_current < ichg1_min)
			afc_device->afc_setting_charge_current = ichg1_min;
	} else
		afc_device->afc_setting_charge_current = afc_device->afc_charger_current;

	if (afc_device->afc_limit_input_current != -1 &&
		afc_device->afc_limit_input_current < afc_device->afc_charger_input_current)
	{
		afc_device->afc_setting_input_current = afc_device->afc_limit_input_current;
		ret = charger_dev_get_min_input_current(pinfo->chg1_dev, &aicr1_min);
		if (ret != -ENOTSUPP &&
			afc_device->afc_limit_input_current < aicr1_min)
			afc_device->afc_setting_input_current = aicr1_min;
	} else
		afc_device->afc_setting_input_current = afc_device->afc_charger_input_current;
	mutex_unlock(&afc_device->data_lock);


	if (afc_device->afc_setting_input_current == 0 ||
		afc_device->afc_setting_charge_current == 0) {
		pr_err("current is zero %d %d\n",
			afc_device->afc_setting_input_current,
			afc_device->afc_setting_charge_current);
		return -1;
	}

	charger_dev_set_input_current(pinfo->chg1_dev,
		afc_device->afc_setting_input_current);

	charger_dev_set_charging_current(pinfo->chg1_dev,
		afc_device->afc_setting_charge_current);

	charger_dev_set_constant_voltage(pinfo->chg1_dev,
		afc_device->afc_limit_cv);

	pr_err("%s state:%d cv:%d chg1:%d,%d min:%d:%d\n", __func__,
		afc_device->state,
		afc_device->afc_limit_cv,
		afc_device->afc_setting_input_current,
		afc_device->afc_setting_charge_current,
		ichg1_min,
		aicr1_min);

	return 0;
}

/*
* afc_set_to_check_chr_type
*
* Set to_check_chr_type value if hv_charge is disable and afc leave
* Set to_check_chr_type value if quit limit current
*/
void afc_set_to_check_chr_type(struct mtk_charger *pinfo, bool check)
{
	pr_info("%s: check = %d\n", __func__, check);

//+Bug 516173,zhaosidong.wt,ADD,20191205,afc detection
	if (!pinfo->afc.afc_init_ok) {
		pr_err("%s: stop, afc init failed \n", __func__);
		return;
	}
//-Bug 516173,zhaosidong.wt,ADD,20191205,afc detection

	mutex_lock(&pinfo->afc.access_lock);
	pinfo->afc.to_check_chr_type = check;
	mutex_unlock(&pinfo->afc.access_lock);
}


/*
* afc_check_charger
*
* The function will be executed is CC step.
*/
//+Bug 520926,zhaosidong.wt,ADD,20191211,ensure DP keep 0.6v at least 1.5second
int afc_pre_check(struct mtk_charger *pinfo)
{
	int i;
	struct afc_dev *afc_device = &pinfo->afc;

	for (i = 0; i < 15; i++) {
		mdelay(100);
		if (/*!afc_device->to_check_chr_type || */
			get_charger_type(NULL) != POWER_SUPPLY_TYPE_USB_DCP) {
			pr_err("%s: stop, to_check_chr_type = %d, chr_type = %d \n",
				__func__, afc_device->to_check_chr_type, get_charger_type(NULL));
			return 1;
		}
/*
		if (cancel_afc(pinfo) == true) {
			pr_err("%s: stop, cancel \n", __func__);
			return 1;
		}
*/
}

	return 0;
}
//-Bug 520926,zhaosidong.wt,ADD,20191211,ensure DP keep 0.6v at least 1.5second

//  return done when uisoc reach 85 or charger/input current = 0
//  return not support when charger type not dcp
int afc_check_charger(struct mtk_charger *pinfo)
{
	int ret = -1;
//	int uisoc = 0;
	struct afc_dev *afc_device = &pinfo->afc;
	//EXTB P200225-00205,zhaosidong.wt,ADD,20200227, report fast charging state in time.
	struct power_supply *psy = power_supply_get_by_name("battery");

	pr_info("%s: Start\n", __func__);

	if (!pinfo->enable_hv_charging) {
		pr_err("%s: hv charging is disabled\n", __func__);
		if (afc_device->is_connect) {
			afc_leave(pinfo); //Bug 516173,zhaosidong.wt,ADD,20191205,afc detection
			mutex_lock(&afc_device->access_lock);
			afc_device->to_check_chr_type = true;
			mutex_unlock(&afc_device->access_lock);
		}
		return ret;
	}
/*
	if (cancel_afc(pinfo) == true) {
		pr_err("%s: stop, cancel \n", __func__);
		return ret;
	}
*/
	if (!afc_device->afc_init_ok) {
		pr_err("%s: stop, afc init failed \n", __func__);
		return ALG_NOT_READY;
	}

	if (!afc_device->is_enable || !pinfo->enable_afc) {
		pr_err("%s: stop, afc is disabled\n", __func__);
		return ALG_NOT_READY;
	}

//+Bug 520926,zhaosidong.wt,ADD,20191211,prio to check afc
	if (afc_pre_check(pinfo)) {
		return ALG_TA_NOT_SUPPORT;
	}

#if defined(CONFIG_DRV_SAMSUNG)
	if (afc_device->afc_disable) {
		pr_info("%s: disabled by USER\n", __func__);
		return ret;
	}
#endif

/*	uisoc = afc_get_uisoc(afc_device);
	if((uisoc < afc_device->afc_start_battery_soc)
		|| (uisoc > afc_device->afc_stop_battery_soc))
	{
		pr_err("soc is %d, not in %d ~ %d, stop afc\n",uisoc,
			afc_device->afc_start_battery_soc,
			afc_device->afc_stop_battery_soc);
		return ALG_TA_CHECKING;
	}
*/
	afc_enable_vbus_ovp(pinfo, false);
	mutex_lock(&pinfo->afc.access_lock);
	__pm_stay_awake(pinfo->afc.suspend_lock);
/*Note: It need to set pre-ARIC, or TA will Crash!!,
The AICR will be configured right siut value in CC step */
	charger_dev_set_input_current(pinfo->chg1_dev, pinfo->afc.afc_pre_input_current);
	ret = afc_set_ta_vchr(pinfo, SET_9V);
	if (ret == 0) {
		g_afc_work_status = 1;
		pr_info("%s ok\n", __func__);
		ret = afc_set_mivr(pinfo, afc_device->vol - 1000000);
	} else {
		g_afc_work_status = 0;
		goto out;
	}

	afc_device->to_check_chr_type = false;
	afc_device->is_connect = true;
	__pm_relax(pinfo->afc.suspend_lock);
	mutex_unlock(&pinfo->afc.access_lock);
	if(!IS_ERR_OR_NULL(psy)) {
		power_supply_changed(psy);
	}
	pr_info("%s End\n", __func__);
	return 0;
out:
	afc_device->to_check_chr_type = false;
	afc_device->is_connect = false;
	__pm_relax(pinfo->afc.suspend_lock);
	mutex_unlock(&pinfo->afc.access_lock);
	if(!IS_ERR_OR_NULL(psy)) {
		power_supply_changed(psy);
	}
	afc_enable_vbus_ovp(pinfo, true);
	chr_info("%s:not detect afc\n",__func__);
	return ALG_TA_NOT_SUPPORT;
//-Bug 520926,zhaosidong.wt,ADD,20191211,prio to check afc
}

/*
* __afc_set_ta_vchr
*
* Set TA voltage, chr_volt mV
*/
static int __afc_set_ta_vchr(struct mtk_charger *pinfo, u32 chr_volt)
{
	int i = 0, ret = 0; 
	struct afc_dev *afc = &pinfo->afc;

	pr_info("%s\n", __func__);
	afc->vol = chr_volt;
	for (i = 0; i < AFC_COMM_CNT ; i++) {
		ret = afc_communication(afc);
		if (ret == 0) {
			pr_info("%s: ok, i %d \n", __func__,i);
			break;
		}
	}
	return ret;
}

/*
* afc_set_ta_vchr
*
* set TA voltage and check the result
* Note :  set TA voltage fristly, then set MIVR/AICR
*/
int afc_set_ta_vchr(struct mtk_charger *pinfo, u32 chr_volt)
{
	int ret = 0;
	int vchr_before, vchr_after, vchr_delta;
	const u32 sw_retry_cnt_max = 3;
	const u32 retry_cnt_max = 5;
	u32 sw_retry_cnt = 0, retry_cnt = 0;

	pr_info("%s: Start\n", __func__);
	vchr_before = afc_get_vbus(&pinfo->afc);
	do {
		ret = __afc_set_ta_vchr(pinfo, chr_volt);
		mdelay(38);
		vchr_after = afc_get_vbus(&pinfo->afc);
		vchr_delta = abs(vchr_after - chr_volt);

		/*
		 * It is successful if VBUS difference to target is
		 * less than 1000mV.
		 */	
		if (vchr_delta < 1000000) {
			pr_err("%s: OK, vchr = (%d, %d), vchr_target = %dmV\n",
				__func__, vchr_before / 1000, vchr_after / 1000,
				chr_volt / 1000);
			return 0;
		}

		if (ret == 0 || sw_retry_cnt >= sw_retry_cnt_max)
			retry_cnt++;
		else
			sw_retry_cnt++;

		afc_set_mivr(pinfo, pinfo->afc.afc_min_charger_voltage);
		pr_info("%s: retry_cnt = (%d, %d), vchr = (%d, %d), vchr_target = %dmV\n",
			__func__, sw_retry_cnt, retry_cnt, vchr_before / 1000,
			vchr_after / 1000, chr_volt / 1000);
	} while ( get_charger_type(NULL) == POWER_SUPPLY_TYPE_USB_DCP &&
		 retry_cnt < retry_cnt_max && pinfo->enable_hv_charging/* &&
		 cancel_afc(pinfo) != true */);

	ret = -EIO;
	pr_err("%s: failed, vchr_after = %dmV, target_vchr = %dmV\n",
		__func__, vchr_after / 1000, chr_volt / 1000);
	return ret;
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
	if (battery_get_soc() > pinfo->data.afc_stop_battery_soc &&
	    ichg > 0 && ichg < pinfo->data.afc_ichg_level_threshold) {
		ret = afc_leave(pinfo);
		if (ret < 0 || afc_device->is_connect)
			goto _err;
		pr_err("%s: OK, SOC = (%d,%d), stop AFC\n", __func__,
			battery_get_soc(), pinfo->data.afc_stop_battery_soc);
	}
	return ret;

_err:
	pr_err("%s: failed, is_connect = %d, ret = %d\n",__func__, afc_device->is_connect, ret);
	return ret;
}
#endif

static int afc_plugout_reset(struct chg_alg_device *alg)
{
	struct afc_dev *afc_device;
	struct mtk_charger *pinfo;

	afc_device = dev_get_drvdata(&alg->dev);
	pinfo = afc_device->info;

	pr_err("%s\n", __func__);
	switch (afc_device->state) {
	case AFC_HW_UNINIT:
	case AFC_HW_FAIL:
	case AFC_HW_READY:
		/* if afc is connect,means charge full and wait for recharge */
		if(afc_device->is_connect)
			afc_hw_plugout_reset(pinfo);
		afc_device->is_connect = false;
		afc_device->to_check_chr_type = true;
		break;
	case AFC_TA_NOT_SUPPORT:
		afc_device->is_connect = false;
		afc_device->to_check_chr_type = true;
		afc_device->state = AFC_HW_READY;
		break;
	case AFC_RUN:
	case AFC_TUNING:
	case AFC_POSTCC:
		afc_hw_plugout_reset(pinfo);

		pr_err("%s: OK\n", __func__);
		afc_device->state = AFC_HW_READY;
//		mutex_unlock(&afc_device->access_lock);
		break;
	default:
		pr_err("%s: error state:%d\n", __func__,
			afc_device->state);
		break;
	}
	return 0;
}

static int afc_full_event(struct chg_alg_device *alg)
{
	struct afc_dev *afc_device;
	struct mtk_charger *pinfo;

	afc_device = dev_get_drvdata(&alg->dev);
	pinfo = afc_device->info;
	switch (afc_device->state) {
	case AFC_HW_UNINIT:
	case AFC_HW_FAIL:
	case AFC_HW_READY:
	case AFC_TA_NOT_SUPPORT:
		break;
	case AFC_RUN:
	case AFC_TUNING:
	case AFC_POSTCC:
		if (afc_device->state == AFC_RUN) {
			pr_err("%s evt full, keep afc activate\n",  __func__);
//			afc_leave(pinfo);
			afc_device->state = AFC_HW_READY;
		}
		break;
	default:
		pr_err("%s: error state:%d\n", __func__,
			afc_device->state);
		break;
	}

	return 0;
}

static int afc_notifier_call(struct chg_alg_device *alg,
			 struct chg_alg_notify *notify)
{
	int ret_value = 0;

	struct afc_dev *afc_device;

	afc_device = dev_get_drvdata(&alg->dev);
	pr_err("%s evt:%d state:%d\n", __func__, notify->evt,afc_device->state);

	switch (notify->evt) {
	case EVT_PLUG_OUT:
		ret_value = afc_plugout_reset(alg);
		break;
	case EVT_FULL:
		ret_value = afc_full_event(alg);
		break;
	default:
		ret_value = -EINVAL;
	}
	return ret_value;
}

int afc_set_setting(struct chg_alg_device *alg_dev,
	struct chg_limit_setting *setting)
{

	struct afc_dev *afc_device;
	struct mtk_charger *pinfo;

	afc_device = dev_get_drvdata(&alg_dev->dev);
	pinfo = afc_device->info;

	pr_err("%s cv:%d icl:%d cc:%d\n",
		__func__,
		setting->cv,
		setting->input_current_limit1,
		setting->charging_current_limit1);
		
	mutex_lock(&pinfo->afc.access_lock);
	__pm_stay_awake(pinfo->afc.suspend_lock);
	afc_device->afc_limit_cv = setting->cv;
	afc_device->afc_limit_input_current = setting->input_current_limit1;
	afc_device->afc_limit_charge_current = setting->charging_current_limit1;
	__pm_relax(pinfo->afc.suspend_lock);
	mutex_unlock(&pinfo->afc.access_lock);

	return 0;
}

int afc_get_prop(struct chg_alg_device *alg,
		enum chg_alg_props s, int *value)
{

	pr_notice("%s\n", __func__);
	if (s == ALG_MAX_VBUS)
		*value = 10000;
	else
		pr_notice("%s does not support prop:%d\n", __func__, s);
	return 0;
}

int afc_set_prop(struct chg_alg_device *alg,
		enum chg_alg_props s, int value)
{
	pr_notice("%s %d %d\n", __func__, s, value);
	return 0;
}

/*
* afc_start_algorithm
*
* Note: 1. judge more cases before set TA voltage
*       2. Exit AFC mode if appropriate
*/
int afc_start_algorithm(struct chg_alg_device *alg)
{
	int ret = 0;
//+bug 623282,yaocankun.wt,add,20210127,remove afc soc check
//	int pre_vbus = 0 , ichg = 0;
//	int vbat = 0;
//-bug 623282,yaocankun.wt,add,20210127,remove afc soc check
	struct afc_dev *afc_device;
	struct mtk_charger *pinfo;

	afc_device = dev_get_drvdata(&alg->dev);
	pinfo = afc_device->info;

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

	if (!afc_device->is_enable) {
		pr_err("%s: stop, AFC is disabled\n",__func__);
		return ret;
	}
	pr_err("%s: afc state %d\n",__func__, afc_device->state);
	if(afc_device->state == AFC_HW_READY)
	{
		ret = afc_check_charger(pinfo);
		if(!ret)
		{
			afc_device->state = AFC_RUN;
			ret = ALG_RUNNING;
			pr_err("%s afc check charger ok\n", __func__);
		}else if(ret == ALG_TA_CHECKING)
		{
//			afc_device->state = AFC_TA_NOT_SUPPORT;
			ret = ALG_NOT_READY;
			pr_err("%s afc check charger not ready\n", __func__);

		}else
		{
			afc_device->state = AFC_TA_NOT_SUPPORT;
			ret = ALG_TA_NOT_SUPPORT;
			pr_err("%s afc check charger not support\n", __func__);

		}
	}
	if(afc_device->state == AFC_RUN)
	{
		if (!afc_device->is_connect) {
			ret = AFC_TA_NOT_SUPPORT;
			pr_err("%s: stop,AFC is not connected\n",__func__);
			return ret;
		}
		pr_err("%s afc alg is running\n", __func__);
//+bug 623282,yaocankun.wt,add,20210127,remove afc soc check
/*
	skip soc check in afc chargering
*/
/*
		pre_vbus = afc_get_vbus(afc_device);
		ichg = afc_get_ibat(afc_device);
		vbat = afc_get_vbat(afc_device) / 1000;
		if ((abs(pre_vbus - SET_9V) < 1000000)
			&& (afc_get_uisoc(afc_device) > afc_device->afc_stop_battery_soc)
			&& (ichg > 0)
			&& (ichg < pinfo->afc.afc_ichg_level_threshold)
			&& (vbat > 4200)) {
			ret = afc_9v_to_5v(pinfo);
			if (ret < 0) {
				pr_err("%s: afc_9v_to_5v failed !\n",__func__);
			}else
			{
				afc_device->state = AFC_HW_READY;
				pr_err("%s: afc state %d\n",__func__, afc_device->state);
				ret = ALG_DONE;
				pr_err("%s: afc charge reach end condition !\n",__func__);
				pr_err("%s: soc: %d, vbus %d, ichg %d, vbat %d \n",__func__,
					afc_get_uisoc(afc_device), pre_vbus, ichg, vbat);
			}
		}else
*/
//-bug 623282,yaocankun.wt,add,20210127,remove afc soc check
			afc_set_charger(alg);
	}

	pr_err("%s: SOC = %d, is_connect = %d\n",
		__func__, afc_get_uisoc(afc_device), afc_device->is_connect);

	return ret;
}


static bool afc_is_algo_running(struct chg_alg_device *alg)
{
	struct afc_dev *afc_device;

	afc_device = dev_get_drvdata(&alg->dev);
	pr_err("%s\n", __func__);

	if (afc_device->state == AFC_RUN)
		return true;

	return false;
}

static int afc_stop_algo(struct chg_alg_device *alg)
{
	struct afc_dev *afc_device;

	afc_device = dev_get_drvdata(&alg->dev);

	pr_err("%s %d\n", __func__, afc_device->state);
	if (afc_device->state == AFC_RUN) {
		afc_reset_ta_vchr(afc_device->info);
		afc_device->state = AFC_HW_READY;
		g_afc_work_status = 0;
	}

	return 0;
}

static ssize_t show_afc_toggle(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	struct mtk_charger *pinfo = dev->driver_data;

	pr_info("%s: %d\n", __func__, pinfo->afc.is_connect);
	return sprintf(buf, "%d\n", pinfo->afc.is_connect);
}

static ssize_t store_afc_toggle(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct mtk_charger *pinfo = dev->driver_data;
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
//		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_HV_DISABLE, &psy_val);  //temporarily disable
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

static int afc_is_algo_ready(struct chg_alg_device *alg)
{
	struct afc_dev *afc;
	int ret_value, uisoc;
	int charger_type = 0;
	union power_supply_propval prop;
	struct power_supply *bat_psy;

	afc = dev_get_drvdata(&alg->dev);
	__pm_stay_awake(afc->suspend_lock);
	bat_psy = afc->bat_psy;
	if (bat_psy == NULL)
	{
		bat_psy = power_supply_get_by_name("battery");
		if (bat_psy == NULL || IS_ERR(bat_psy))
		{
			pr_notice("%s Couldn't get bat_psy, guest at 50\n", __func__);
			uisoc = 50;
		}
	}
	if (bat_psy)
	{
		power_supply_get_property(bat_psy,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
		uisoc = prop.intval;
	}
//	afc = dev_get_drvdata(&alg->dev);
	charger_type = get_charger_type(NULL);

	pr_info("%s: afc state %d, connect: %d\n", __func__, afc->state, afc->is_connect);
	if(afc->state == AFC_HW_READY)
	{
		if (POWER_SUPPLY_TYPE_USB_DCP != charger_type)
		{
			pr_err("afc charger type not support: %d\n", charger_type);
			ret_value = ALG_TA_NOT_SUPPORT;
		}
//+bug 623282,yaocankun.wt,add,20210127,remove afc soc check
		if(afc->is_connect)
		{
			pr_err("afc is ready, but charger is connected, it seems reach max soc\n");
			ret_value = ALG_TA_CHECKING;
		}else if((bat_psy == NULL) || (uisoc == -1))
		{
			pr_err("battery psy is not found or soc -1, keep not ready\n");
			ret_value = ALG_NOT_READY;
		}/*else if ((afc->afc_start_battery_soc > uisoc)
			|| (afc->afc_stop_battery_soc < uisoc))
		{
			pr_err("afc charger soc is: %d, not support in %d ~ %d\n",
				uisoc, afc->afc_start_battery_soc, afc->afc_stop_battery_soc);
			ret_value = ALG_READY;
		}*/else
//-bug 623282,yaocankun.wt,add,20210127,remove afc soc check
		{
			ret_value = ALG_READY;
		}
	}else if(afc->state == AFC_RUN)
	{
		ret_value = ALG_RUNNING;
		pr_err("%s afc alg running ok\n", __func__);
	}else
	{
		ret_value = ALG_TA_NOT_SUPPORT;

	}
	__pm_relax(afc->suspend_lock);
	return ret_value;
}
static struct chg_alg_ops afc_alg_ops = {
	.init_algo = afc_charge_init_alg,
	.is_algo_ready = afc_is_algo_ready,
	.start_algo = afc_start_algorithm,
	.is_algo_running = afc_is_algo_running,
	.stop_algo = afc_stop_algo,
	.notifier_call = afc_notifier_call,
	.get_prop = afc_get_prop,
	.set_prop = afc_set_prop,
	.set_current_limit = afc_set_setting,
};


int afc_charge_init_alg(struct chg_alg_device *alg)
{
	struct platform_device *pdev;
	struct device *dev;
	struct device_node *np;
	struct afc_dev *afc;
	int ret = 0;
	u32 val = 0;
	struct power_supply *bat_psy;

	afc = dev_get_drvdata(&alg->dev);
	pdev = afc->pdev;
	dev = &pdev->dev;
	np= dev->of_node;
	pr_info("%s: Start\n", __func__);
	if (!afc) {
		pr_err("%s, afc allocation fail, just return!\n", __func__);
		return -ENOMEM;
	}

	if (of_property_read_u32(np, "afc_start_battery_soc", &val) >= 0)
		afc->afc_start_battery_soc = val;
	else {
		chr_err("use default AFC_START_BATTERY_SOC:%d\n",
			AFC_START_BATTERY_SOC);
		afc->afc_start_battery_soc = AFC_START_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "afc_stop_battery_soc", &val) >= 0)
		afc->afc_stop_battery_soc = val;
	else {
		chr_err("use default AFC_STOP_BATTERY_SOC:%d\n",
			AFC_STOP_BATTERY_SOC);
		afc->afc_stop_battery_soc = AFC_STOP_BATTERY_SOC;
	}

	if (of_property_read_u32(np, "afc_ichg_level_threshold", &val) >= 0)
			afc->afc_ichg_level_threshold = val;
	else {
		chr_err("use default AFC_ICHG_LEAVE_THRESHOLD:%d\n",
			AFC_ICHG_LEAVE_THRESHOLD);
		afc->afc_ichg_level_threshold = AFC_ICHG_LEAVE_THRESHOLD;
	}

	if (of_property_read_u32(np, "afc_pre_input_current", &val) >= 0)
			afc->afc_pre_input_current = val;
	else {
		chr_err("use default afc_pre_input_current:%d\n",
			AFC_PRE_INPUT_CURRENT);
		afc->afc_pre_input_current = AFC_PRE_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "afc_charger_input_current", &val) >= 0)
			afc->afc_charger_input_current = val;
	else {
		chr_err("use default afc_charger_input_current:%d\n",
			AFC_CHARGER_INPUT_CURRENT);
		afc->afc_charger_input_current = AFC_CHARGER_INPUT_CURRENT;
	}

	if (of_property_read_u32(np, "afc_charger_current", &val) >= 0)
			afc->afc_charger_current = val;
	else {
		chr_err("use default afc_charger_current:%d\n",
			AFC_CHARGER_CURRENT);
		afc->afc_charger_input_current = AFC_CHARGER_CURRENT;
	}

	if (of_property_read_u32(np, "afc_min_charger_voltage", &val) >= 0)
			afc->afc_min_charger_voltage = val;
	else {
		chr_err("use default afc_min_charger_voltage:%d\n",
			AFC_MIN_CHARGER_VOLTAGE);
		afc->afc_min_charger_voltage = AFC_MIN_CHARGER_VOLTAGE;
	}

	if (of_property_read_u32(np, "afc_max_charger_voltage", &val) >= 0)
			afc->afc_max_charger_voltage = val;
	else {
		chr_err("use default afc_min_charger_voltage:%d\n",
			AFC_MAX_CHARGER_VOLTAGE);
		afc->afc_max_charger_voltage = AFC_MAX_CHARGER_VOLTAGE;
	}

	afc->vol = afc->afc_max_charger_voltage;


	ret = of_get_named_gpio(np,"afc_switch_gpio", 0);
	if (ret < 0) {
		pr_err("%s, could not get afc_switch_gpio\n", __func__);
		return ret;
	} else {
		afc->afc_switch_gpio = ret;
		pr_info("%s, afc->afc_switch_gpio: %d\n", __func__, afc->afc_switch_gpio);
		ret = gpio_request(afc->afc_switch_gpio, "GPIO159");
	}

	ret = of_get_named_gpio(np,"afc_data_gpio", 0);
	if (ret < 0) {
		pr_err("%s, could not get afc_data_gpio\n", __func__);
		return ret;
	} else {
		afc->afc_data_gpio = ret;
		pr_info("%s, afc->afc_data_gpio: %d\n", __func__, afc->afc_data_gpio);
		ret = gpio_request(afc->afc_data_gpio, "GPIO42");
	}

	afc->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(afc->pinctrl)) {
		pr_err("%s, No pinctrl config specified\n", __func__);
		return -EINVAL;
	}

	afc->pin_active = pinctrl_lookup_state(afc->pinctrl, "active");
	if (IS_ERR_OR_NULL(afc->pin_active)) {
		pr_err("%s, could not get pin_active\n", __func__);
		return -EINVAL;
	}

	afc->pin_suspend = pinctrl_lookup_state(afc->pinctrl, "sleep");
	if (IS_ERR_OR_NULL(afc->pin_suspend)) {
		pr_err("%s, could not get pin_suspend\n", __func__);
		return -EINVAL;
	}

	bat_psy = power_supply_get_by_name("battery");
	if (bat_psy == NULL || IS_ERR(bat_psy)) {
		pr_notice("%s Couldn't get bat_psy\n", __func__);
	}else
		afc->bat_psy = bat_psy;

	afc->state = AFC_HW_READY;


	pr_info("%s: end \n", __func__);
	return 0;

}


int afc_charge_init(struct mtk_charger *pinfo)
{
	struct platform_device *pdev = pinfo->pdev;
//	struct device *dev = &pdev->dev;
//	struct device_node *np= dev->of_node;
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

	afc->pdev = pdev;
	afc->info = pinfo;
	afc->ta_vchr_org = 5000000;
	afc->to_check_chr_type = true;
	afc->is_enable = true;
	afc->is_connect = false;
	afc->pin_state = false;
	afc->afc_init_ok = true;
	afc->afc_error = 0;
	afc->afc_loop_algo = 0; //+Bug 516173,zhaosidong.wt,MODIFY,20191205,try to set vbus 3 times
	g_afc_work_status = 0;//Bug 518556,liuyong3.wt,ADD,20191128,Charging afc flag
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

	pinfo->afc.suspend_lock = wakeup_source_register(0, "AFC suspend wakelock");
	spin_lock_init(&afc->afc_spin_lock);
	mutex_init(&afc->access_lock);
	mutex_init(&afc->data_lock);

	ret = device_create_file(&(pdev->dev), &dev_attr_afc_toggle);
	if (ret)
		pr_err("sysfs afc_toggle create failed\n");

	afc->alg = chg_alg_device_register("afc", &pdev->dev,
					afc, &afc_alg_ops, NULL);
	pr_info("%s: end \n", __func__);
	return 0;

}


