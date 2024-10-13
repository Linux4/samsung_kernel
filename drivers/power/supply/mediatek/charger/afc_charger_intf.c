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

#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif
#if defined(CONFIG_SEC_PARAM)
#include <linux/sec_ext.h>
#endif
#include "mtk_charger_init.h"
#include "mtk_charger_intf.h"

int g_afc_work_status = 0;//Bug 518556,liuyong3.wt,ADD,20191128,Charging afc flag

extern signed int battery_get_vbus(void);
extern int pmic_get_battery_voltage(void);
extern signed int battery_get_bat_current(void);
extern int pmic_enable_hw_vbus_ovp(bool enable);
extern int charger_enable_vbus_ovp(struct charger_manager *pinfo, bool enable);
extern int charger_dev_set_mivr(struct charger_device *chg_dev, u32 uV);

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

/*uV*/
static inline u32 afc_get_vbus(void)
{
	return battery_get_vbus()*1000;
}

/*uA*/
static inline u32 afc_get_ibat(void)
{
	return battery_get_bat_current()*100;
}

void cycle(struct afc_dev *afc, int ui)
{
	if (!afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_active);
		afc->pin_state = true;
	}
   //+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	gpio_set_value(afc->afc_data_gpio, 1);
	udelay(ui);
	gpio_set_value(afc->afc_data_gpio, 0);
	 //-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	udelay(ui);
}

void afc_send_Mping(struct afc_dev *afc)
{
	if (!afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_active);
		afc->pin_state = true;
	}
    //+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
    pinctrl_gpio_direction_output(afc->afc_data_gpio);
	spin_lock_irq(&afc->afc_spin_lock);
	gpio_set_value(afc->afc_data_gpio, 1);
	udelay(16*UI);
	gpio_set_value(afc->afc_data_gpio, 0);
	spin_unlock_irq(&afc->afc_spin_lock);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context

}

int afc_recv_Sping(struct afc_dev *afc)
{
	int i = 0, sp_cnt = 0;

	if (afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_suspend);
		afc->pin_state = false;
	}
	//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
    pinctrl_gpio_direction_input(afc->afc_data_gpio);
	spin_lock_irq(&afc->afc_spin_lock);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	while (1)
	{
		udelay(UI);
		if (gpio_get_value(afc->afc_data_gpio)) {
			//pr_debug("%s: wait rsp %d\n", __func__, i);
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
    //+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	spin_unlock_irq(&afc->afc_spin_lock);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	pr_debug("%s: len %d \n", __func__, sp_cnt);
	return 0;

Sping_err: 
	//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context 
	spin_unlock_irq(&afc->afc_spin_lock);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
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
	//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	if (!odd)
		gpio_set_value(afc->afc_data_gpio, 1);
	else
		gpio_set_value(afc->afc_data_gpio, 0);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context

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
    //+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context 
    pinctrl_gpio_direction_output(afc->afc_data_gpio);
	spin_lock_irq(&afc->afc_spin_lock);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	udelay(160);

	if (data & 0x80)
		cycle(afc, UI/4);
	else {
		cycle(afc, UI/4);
		//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
		 gpio_set_value(afc->afc_data_gpio, 1);
	    //-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
		udelay(UI/4);
	}

	for (i = 0x80; i > 0; i = i >> 1)
	{
		//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
		 gpio_set_value(afc->afc_data_gpio, data & i);
		//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
		udelay(UI);
	}

	if (afc_send_parity_bit(afc, data)) {
		//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
		gpio_set_value(afc->afc_data_gpio, 0);
		//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
		udelay(UI/4);
		cycle(afc, UI/4);
	} else {
		udelay(UI/4);
		cycle(afc, UI/4);
	}
	//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	spin_unlock_irq(&afc->afc_spin_lock);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
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
	//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	spin_lock_irq(&afc->afc_spin_lock);

	mdelay(2);//+Bug 522575,zhaosidong.wt,ADD,20191216,must delay 2 ms
	spin_unlock_irq(&afc->afc_spin_lock);
    //-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
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

	pr_info("line:%d %s - Start\n", __LINE__, __func__);
	//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	gpio_direction_output(afc->afc_switch_gpio, 1);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context

    
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
    //+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	spin_lock_irq(&afc->afc_spin_lock);
	udelay(200);
	spin_unlock_irq(&afc->afc_spin_lock);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	afc_send_Mping(afc);

	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_4;
		goto afc_fail;
	}
	//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	gpio_direction_output(afc->afc_switch_gpio, 0);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	pr_info("line:%d %s - End \n", __LINE__,  __func__);

	return 0;

afc_fail:
	//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	gpio_direction_output(afc->afc_switch_gpio, 0);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	pr_err("line:%d %s, AFC communication has been failed\n", __LINE__, __func__);

	return -1;
}

/*
* cancel_afc
*
* if PDC/PD is ready , pls quit AFC detect
*/
static bool cancel_afc(struct charger_manager *pinfo)
{
	if (mtk_pdc_check_charger(pinfo) || mtk_is_TA_support_pd_pps(pinfo) || mtk_pe20_get_is_connect(pinfo))
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
		pr_err("%s: failed, ret = %d\n", __func__, ret);
		return ret;
	}
    
	charger_enable_vbus_ovp(pinfo, enable);
	return ret;
}

static int afc_set_mivr(struct charger_manager *pinfo, int uV)
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
int afc_plugout_reset(struct charger_manager *pinfo)
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
bool afc_get_is_connect(struct charger_manager *pinfo)
{
	if (pinfo == NULL) {
		pr_err("%s: pinfo is NULL\n", __func__);
		return false;
	}

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

	gpio_direction_output(afc->afc_switch_gpio, 1);
        
	if (!afc->pin_state) {
		pinctrl_select_state(afc->pinctrl, afc->pin_active);
		afc->pin_state = true;
	}
	//+Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	pinctrl_gpio_direction_output(afc->afc_data_gpio);
	spin_lock_irq(&afc->afc_spin_lock);

	gpio_set_value(afc->afc_data_gpio, 1);
	mdelay((100*UI)/1000);
	gpio_set_value(afc->afc_data_gpio, 0);
	
	spin_unlock_irq(&afc->afc_spin_lock);
	//-Extb P210323-06747 P210331-0007,lvyuanchuan.wt,modify,20210407,The Mutex lock is outside the spin context
	gpio_direction_output(afc->afc_switch_gpio, 0);

}
//+bug528143,zhaosidong.wt,ADD,20200109,afc enable/disable toggle
int afc_5v_to_9v(struct charger_manager *pinfo)
{
	int ret = 0;
	int pre_vbus = afc_get_vbus();

	if (abs(pre_vbus - SET_9V) < 1000000) {
		pr_info("%s:Vbus was already %d\n",__func__,pre_vbus);
		return ret;
	}

	ret = afc_enable_vbus_ovp(pinfo, false);
	if(ret) {
		pr_err("%s: disable ovp failed!\n",__func__);
		return ret;
	}

	ret = charger_dev_set_input_current(pinfo->chg1_dev, pinfo->data.afc_pre_input_current);
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
//-bug528143,zhaosidong.wt,ADD,20200109,afc enable/disable toggle

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
		chr_volt = afc_get_vbus();
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
int afc_leave(struct charger_manager *pinfo)
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
		charger_dev_set_input_current(pinfo->chg1_dev, pinfo->data.afc_pre_input_current);
		pr_err("%s: failed, is_connect = %d, ret = %d\n",
			__func__, afc_device->is_connect, ret);
		return ret;
	}

	g_afc_work_status = 0;//Bug 518556,liuyong3.wt,ADD,20191128,Charging afc flag
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
int afc_set_charging_current(struct charger_manager *pinfo,
	unsigned int *ichg, unsigned int *aicr)
{
	int ret = 0;
	int vchr;
	struct afc_dev *afc_device = &pinfo->afc;

	if (!afc_device->is_connect)
		return -ENOTSUPP;

//bug533258,zhaosidong.wt,ADD,20200217,toggle between 9V/1.65A and 5V/2A
	vchr = afc_get_vbus();
	if(abs(vchr - SET_9V) < 1000000) {
		*aicr = pinfo->data.afc_charger_input_current;
		*ichg = pinfo->data.afc_charger_current;
	} else {
		*aicr = pinfo->data.ac_charger_input_current;
		*ichg = pinfo->data.ac_charger_current;
	}

	pr_info("%s: ichg = %dmA, AICR = %dmA\n",__func__, *ichg / 1000, *aicr / 1000);
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
int afc_pre_check(struct charger_manager *pinfo)
{
	int i;
	struct afc_dev *afc_device = &pinfo->afc;

	for (i = 0; i < 1500; i++) {
		mdelay(1);
		if (!afc_device->to_check_chr_type ||
			mt_get_charger_type() != STANDARD_CHARGER) {
			pr_err("%s: stop, to_check_chr_type = %d, chr_type = %d \n",
				__func__, afc_device->to_check_chr_type, mt_get_charger_type());
			return 1;
		}

		if (cancel_afc(pinfo) == true) {
			pr_err("%s: stop, cancel \n", __func__);
			return 1;
		}
	}

	return 0;
}
//-Bug 520926,zhaosidong.wt,ADD,20191211,ensure DP keep 0.6v at least 1.5second

int afc_check_charger(struct charger_manager *pinfo)
{
	int ret = -1;
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

//+Bug 520926,zhaosidong.wt,ADD,20191211,prio to check afc
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
/*Note: It need to set pre-ARIC, or TA will Crash!!,
The AICR will be configured right siut value in CC step */
	charger_dev_set_input_current(pinfo->chg1_dev, pinfo->data.afc_pre_input_current);
	ret = afc_set_ta_vchr(pinfo, SET_9V);
	if (ret == 0) {
		g_afc_work_status = 1;
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
	return 0;
//-Bug 520926,zhaosidong.wt,ADD,20191211,prio to check afc
}

/*
* __afc_set_ta_vchr
*
* Set TA voltage, chr_volt mV
*/
static int __afc_set_ta_vchr(struct charger_manager *pinfo, u32 chr_volt)
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
int afc_set_ta_vchr(struct charger_manager *pinfo, u32 chr_volt)
{
	int ret = 0;
	int vchr_before, vchr_after, vchr_delta;
	const u32 sw_retry_cnt_max = 3;
	const u32 retry_cnt_max = 5;
	u32 sw_retry_cnt = 0, retry_cnt = 0;

	pr_info("%s: Start\n", __func__);
	vchr_before = afc_get_vbus();
	do {
		ret = __afc_set_ta_vchr(pinfo, chr_volt);
		mdelay(38);
		vchr_after = afc_get_vbus();
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

		afc_set_mivr(pinfo, pinfo->data.afc_min_charger_voltage);
		pr_info("%s: retry_cnt = (%d, %d), vchr = (%d, %d), vchr_target = %dmV\n",
			__func__, sw_retry_cnt, retry_cnt, vchr_before / 1000,
			vchr_after / 1000, chr_volt / 1000);
	} while ( mt_get_charger_type() != CHARGER_UNKNOWN &&
		 retry_cnt < retry_cnt_max && pinfo->enable_hv_charging &&
		 cancel_afc(pinfo) != true);

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

/*
* afc_start_algorithm
*
* Note: 1. judge more cases before set TA voltage
*       2. Exit AFC mode if appropriate
*/
int afc_start_algorithm(struct charger_manager *pinfo)
{
	int ret = 0;
	int pre_vbus = 0 , ichg = 0;
	int vbat = 0;
	struct afc_dev *afc_device = &pinfo->afc;

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

	if (!afc_device->is_connect) {
		ret = -EIO;
		pr_err("%s: stop,AFC is not connected\n",__func__);
		return ret;
	}

    /* Check SOC & Ichg */
	pre_vbus = afc_get_vbus();
	ichg = afc_get_ibat();
	vbat = battery_get_bat_voltage();
	if ((abs(pre_vbus - SET_9V) < 1000000) &&
		battery_get_uisoc() > 85 && ichg > 0 &&
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

	afc->ta_vchr_org = 5000000;
	afc->vol = pinfo->data.afc_max_charger_voltage;
	pinfo->data.afc_min_charger_voltage = 4100000;
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

	//wakeup_source_init(&pinfo->afc.suspend_lock, "AFC suspend wakelock");
	pinfo->afc.suspend_lock = wakeup_source_register(NULL, "AFC suspend wakelock");
	
		//wakeup_source_init(pinfo->pe2.suspend_lock, "PE+20 suspend wakelock");
	//pinfo->pe2.suspend_lock = wakeup_source_register(NULL, "PE+20 suspend wakelock");
	spin_lock_init(&afc->afc_spin_lock);
	mutex_init(&afc->access_lock);

	ret = device_create_file(&(pdev->dev), &dev_attr_afc_toggle);
	if (ret)
		pr_err("sysfs afc_toggle create failed\n");

	pr_info("%s: end \n", __func__);
	return 0;

}
