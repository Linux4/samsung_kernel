/*
 * CX25890H  charging driver
 *
 * Copyright (C) 2023 SUNCORE
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/hardware_info.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <asm/unaligned.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/qti_power_supply.h>
#include "cx25890h_charger.h"
#include "cx25890h_iio.h"
#include "afc/gpio_afc.h"
#ifdef CONFIG_WT_QGKI
#include <linux/hardware_info.h>
#endif /*CONFIG_WT_QGKI*/
//#include "cx25890h_reg.h"

#define CHARGER_IC_NAME "CX25890H"

static struct cx25890h_device *g_cx_chg;

static DEFINE_MUTEX(cx25890h_i2c_lock);

/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
static struct of_device_id cx25890h_charger_match_table[] = {
	{.compatible = "sgmicro,sgm41542",},
	{},
};
//static void cx25890h_notify_device_mode(struct cx25890h_device *cx_chg, bool enable);
int cx25890h_get_vbus_volt(struct cx25890h_device *cx_chg,  int *val);
static int cx25890h_write_iio_prop(struct cx25890h_device *cx_chg, enum cx25890h_iio_type type, int channel, int val);
static int cx25890h_read_iio_prop(struct cx25890h_device *cx_chg, enum cx25890h_iio_type type, int channel, int *val);
static int cx25890h_get_usb_type(struct cx25890h_device *cx_chg);
static int cx25890h_request_dpdm(struct cx25890h_device *cx_chg, bool enable);
static void cx25890h_update_wtchg_work(struct cx25890h_device *cx_chg);
static int cx25890h_get_hiz_mode(struct cx25890h_device *cx_chg, u8 *state);
static int cx25890h_set_input_suspend(struct cx25890h_device *cx_chg, int suspend);
static int cx25890h_set_charge_voltage(struct cx25890h_device *cx_chg, u32 uV);
static int cx25890h_set_charge_current(struct cx25890h_device *cx_chg, int curr);
int cx25890h_get_ibat_curr(struct cx25890h_device *cx_chg,  int *val);
int cx25890h_get_vbat_volt(struct cx25890h_device *cx_chg,  int *val);
static void cx25890h_dynamic_adjust_charge_voltage(struct cx25890h_device *cx_chg, int vbat);
static int cx25890h_get_afc_type(struct cx25890h_device *cx_chg);
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
int cx25890h_set_input_volt_limit(struct cx25890h_device *cx_chg, int volt);
static int cx25890h_write_reg40(struct cx25890h_device *cx_chg,bool enable);
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
static int cx25890h_get_hv_dis_status(struct cx25890h_device *cx_chg);
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
//P231018-03087 gudi.wt 20231103,fix add hiz func
void typec_set_input_suspend_for_usbif_cx25890h(bool enable);
//+P86801EA2-300 gudi.wt battery protect function
#ifdef CONFIG_QGKI_BUILD
bool cx25890h_if_been_used(void);
#endif
//-P86801EA2-300 gudi.wt battery protect function

static int cx25890h_read_byte(struct cx25890h_device *cx_chg, u8 *data, u8 reg)
{
	int ret;
	u8 i,retry_count;
	u8 tmp_val[2];

	for(retry_count=0;retry_count<3;retry_count++)
	{
		for(i=0;i<2;i++)
		{
			//mutex_lock(&cx25890h_i2c_lock);
			ret = i2c_smbus_read_byte_data(cx_chg->client, reg);
			//mutex_unlock(&cx25890h_i2c_lock);
			if (ret < 0) 
			{
				dev_err(cx_chg->dev,"i2c read fail: can't read from reg 0x%02X\n", reg);
				break;
			}
			else
				tmp_val[i]=(u8)ret;
		}
		if(ret>=0)
		{
			if(tmp_val[0] == tmp_val[1])
			{
				*data=tmp_val[0];
				return 0;
			}
		}
	}

	dev_err(cx_chg->dev,"i2c retry read fail: can't read from reg 0x%02X\n", reg);
	return -1;

}

static int cx25890h_write_byte(struct cx25890h_device *cx_chg, u8 reg, u8 data)
{
	int ret;
    u8 read_val=0,retry_count;

    for(retry_count=0;retry_count<10;retry_count++)
    {
		//mutex_lock(&cx25890h_i2c_lock);
        ret = i2c_smbus_write_byte_data(cx_chg->client, reg, data);
		//mutex_unlock(&cx25890h_i2c_lock);
        if (ret < 0) 
		{
			dev_err(cx_chg->dev,"i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n", data, reg, ret);
        }
		else
		{
		//	mutex_lock(&cx25890h_i2c_lock);
			read_val=i2c_smbus_read_byte_data(cx_chg->client, reg);
		//	mutex_unlock(&cx25890h_i2c_lock);
			if(data == read_val)
				break;
			else
			{
				if(retry_count==9)
				{
					dev_err(cx_chg->dev,"i2c retry write fail: can't write 0x%02X to reg 0x%02X: %d\n", data, reg, ret);
					return ret;
				}
			}
		}
    }

	return 0;

}

static int cx25890h_read_byte_without_retry(struct cx25890h_device *cx_chg, u8 *data, u8 reg)
{
	int ret;

	//mutex_lock(&cx25890h_i2c_lock);
	ret = i2c_smbus_read_byte_data(cx_chg->client, reg);
	if (ret < 0) {
		dev_err(cx_chg->dev, "failed to read 0x%.2x\n", reg);
	//	mutex_unlock(&cx25890h_i2c_lock);
		return ret;
	}

	*data = (u8)ret;
	//mutex_unlock(&cx25890h_i2c_lock);

	return 0;
}

static int cx25890h_write_byte_without_retry(struct cx25890h_device *cx_chg, u8 reg, u8 data)
{
	int ret;

	//mutex_lock(&cx25890h_i2c_lock);
	ret = i2c_smbus_write_byte_data(cx_chg->client, reg, data);
	//mutex_unlock(&cx25890h_i2c_lock);

	return ret;
}


static int cx25890h_update_bits(struct cx25890h_device *cx_chg,
													u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = cx25890h_read_byte(cx_chg, &tmp, reg);

	if (ret)
		return ret;

	tmp &= ~mask;
	tmp |= data & mask;

	if((reg == CX25890H_REG_02 && mask == CX25890H_CONV_START_MASK) || (reg == CX25890H_REG_02 && mask == CX25890H_FORCE_DPDM_MASK) 
		|| (reg == CX25890H_REG_03 && mask == CX25890H_WDT_RESET_MASK) ||(reg == CX25890H_REG_09 && mask == CX25890H_FORCE_ICO_MASK)
		|| (reg == CX25890H_REG_09 && mask == CX25890H_PUMPX_UP_MASK) || (reg == CX25890H_REG_09 && mask == CX25890H_PUMPX_DOWN_MASK)
		|| (reg == CX25890H_REG_14 && mask == CX25890H_RESET_MASK) || (reg == CX25890H_REG_09 && mask == CX25890H_BATFET_DIS_MASK)) {
		ret = cx25890h_write_byte_without_retry(cx_chg, reg, tmp);
	} else {
		ret = cx25890h_write_byte(cx_chg, reg, tmp);
	}
	
	return ret;
}

static void cx25890h_stay_awake(struct cx25890h_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(source->source);
		pr_debug("enabled source %s\n", source->source->name);
	}
}

static void cx25890h_relax(struct cx25890h_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(source->source);
		pr_debug("disabled source %s\n", source->source->name);
	}
}

static bool cx25890h_wake_active(struct cx25890h_wakeup_source *source)
{
	return !source->disabled;
}

/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/17,kernel bringup Tidying up*/
//+bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation
static int cx25890h_set_chargevoltage(struct cx25890h_device *cx_chg, int volt)
{
	u8 val;

	if(volt<CX25890H_VREG_BASE)
		volt=CX25890H_VREG_BASE;
	if(volt>4608)
		volt=4608;
	val=(volt-CX25890H_VREG_BASE)/CX25890H_VREG_LSB;
	return cx25890h_update_bits(cx_chg, CX25890H_REG_06, CX25890H_VREG_MASK, val<<CX25890H_VREG_SHIFT);
}

static int cx25890h_set_input_current_limit(struct cx25890h_device *cx_chg, int curr)
{
	u8 val;
	u8 input_curr;

	if(curr<100)
		curr=100;
	dev_dbg(cx_chg->dev, "%s: %d\n", __func__, curr);
	input_curr = (curr - CX25890H_IINLIM_BASE) / CX25890H_IINLIM_LSB;
	val = input_curr << CX25890H_IINLIM_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_00, CX25890H_IINLIM_MASK, val);
}


static int cx25890h_enter_ship_mode(struct cx25890h_device *cx_chg)
{
	int ret = 0;
	u8 val;

	cancel_delayed_work(&cx_chg->irq_work);
	cancel_delayed_work(&cx_chg->check_adapter_work);
	cancel_delayed_work(&cx_chg->monitor_work);
	cancel_delayed_work(&cx_chg->rerun_apsd_work);
	cancel_delayed_work(&cx_chg->detect_work);
	msleep(10);
	ret = cx25890h_set_chargevoltage(cx_chg, 3840);
	ret = cx25890h_set_input_current_limit(cx_chg, 100);
	val = CX25890H_BATFET_OFF << CX25890H_BATFET_DIS_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_09, CX25890H_BATFET_DIS_MASK, val);
	msleep(10);
	return ret;
}

static int cx25890h_ship_mode_delay_enable(struct cx25890h_device *cx_chg, bool enable)
{
	int ret = 0;
	u8 val;

	if (enable) {
		val = CX25890H_BATFET_DLY_ON << CX25890H_BATFET_DLY_SHIFT;
	} else {
		val = CX25890H_BATFET_DLY_OFF << CX25890H_BATFET_DLY_SHIFT;
	}

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_09, CX25890H_BATFET_DLY_MASK, val);

	return ret;
}

static int cx25890h_chg_enable(struct cx25890h_device *cx_chg, bool en)
{
	gpio_direction_output(cx_chg->chg_en_gpio, !en);
	msleep(5);

	return 0;
}

static int cx25890h_jeita_enable(struct cx25890h_device *cx_chg, bool en)
{
/*
	u8 val;

	val = en << CX25890H_JEITA_EN_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_0D,
							   CX25890H_JEITA_EN_MASK, val);
							   */
	return 0;							  
}


#if 0
static int cx25890h_set_otg_volt(struct cx25890h_device *cx_chg, int volt)
{
	u8 val = 0;

	if (volt < CX25890H_BOOSTV_BASE)
		volt = CX25890H_BOOSTV_BASE;
	if (volt > CX25890H_BOOSTV_BASE + (CX25890H_BOOSTV_MASK >> CX25890H_BOOSTV_SHIFT) * CX25890H_BOOSTV_LSB)
		volt = CX25890H_BOOSTV_BASE + (CX25890H_BOOSTV_MASK >> CX25890H_BOOSTV_SHIFT) * CX25890H_BOOSTV_LSB;

	val = ((volt - CX25890H_BOOSTV_BASE) / CX25890H_BOOSTV_LSB) << CX25890H_BOOSTV_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_06, CX25890H_BOOSTV_MASK, val);

}
#endif

static int cx25890h_set_otg_current(struct cx25890h_device *cx_chg, int curr)
{
	u8 temp;

	if (curr <= 500)
		temp = CX25890H_BOOST_LIM_500MA;
	else if (curr <= 750)
		temp = CX25890H_BOOST_LIM_750MA;
	else if (curr <= 1200)
		temp = CX25890H_BOOST_LIM_1200MA;
	else if (curr <= 1400)
		temp = CX25890H_BOOST_LIM_1400MA;
	else if (curr <= 1650)
		temp = CX25890H_BOOST_LIM_1650MA;
	else if (curr <= 1875)
		temp = CX25890H_BOOST_LIM_1875MA;
	else if (curr <= 2150)
		temp = CX25890H_BOOST_LIM_2150MA;
	else
		temp = CX25890H_BOOST_LIM_2450MA;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_0A, CX25890H_BOOST_LIM_MASK, temp << CX25890H_BOOST_LIM_SHIFT);

}

static int cx25890h_enable_otg(struct cx25890h_device *cx_chg)
{
	u8 val;//temp,data;
	int ret;
	//ret = cx25890h_read_byte(cx_chg, &data, CX25890H_REG_0A);
	//temp = data & CX25890H_BOOST_LIM_MASK;
	//+P230707-02704 gudi.wt 20230711,modify otg current
	ret = cx25890h_read_byte(cx_chg, &val, CX25890H_REG_0C);
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_0A,CX25890H_BOOST_LIM_MASK,
			CX25890H_BOOST_LIM_2150MA << CX25890H_BOOST_LIM_SHIFT);
	ret = cx25890h_write_reg40(cx_chg,true);
	if(ret==1)
		pr_err("write reg40 success\n");
	ret = cx25890h_write_byte(cx_chg, 0x83, 0x03);
	ret = cx25890h_write_byte(cx_chg, 0x41, 0x88);
	//msleep(100);

	val = CX25890H_OTG_ENABLE << CX25890H_OTG_CONFIG_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_03,CX25890H_OTG_CONFIG_MASK, val);
	msleep(100);
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_0A,CX25890H_BOOST_LIM_MASK, 
		CX25890H_BOOST_LIM_1650MA << CX25890H_BOOST_LIM_SHIFT);
	pr_err("cx25890_enable_otg enter\n");

/*	ret = cx25890h_write_byte(cx_chg, 0x83, 0x02);
	msleep(10);
	ret = cx25890h_write_byte(cx_chg, 0x83, 0x01);
	msleep(10);	*/
	ret = cx25890h_write_byte(cx_chg, 0x83, 0x01);
	msleep(10);
	ret = cx25890h_write_byte(cx_chg, 0x41, 0x08);
	cx25890h_write_reg40(cx_chg,false);
	//-P230707-02704 gudi.wt 20230711,modify otg current
	return ret;

}

static int cx25890h_disable_otg(struct cx25890h_device *cx_chg)
{
	u8 val;
	int ret;

	val = CX25890H_OTG_DISABLE << CX25890H_OTG_CONFIG_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_03,
								   CX25890H_OTG_CONFIG_MASK, val);
	ret = cx25890h_write_reg40(cx_chg,true);
	if(ret==1)
		pr_err("write reg40 success\n");

	ret = cx25890h_write_byte(cx_chg, 0x83, 0x00);
	cx25890h_write_reg40(cx_chg,false);

	return ret;
}

static int cx25890h_set_otg(struct cx25890h_device *cx_chg, int enable)
{
	int ret;
	u8 val=0;

	pr_err("cx25890h_set_otg enter enable=%d \n", enable);
	if (enable) {
//+chk3594, liyiying.wt, 2022/7/18, add, disable hiz-mode when plug on otg
		ret = cx25890h_get_hiz_mode(cx_chg, &val);
		if (ret < 0) {
			dev_err(cx_chg->dev, "Failed to get hiz val-%d\n", ret);
		}
		if (val == 1) {
			cx25890h_set_input_suspend(cx_chg, false);
			dev_err(cx_chg->dev, "cx25890h_exit_hiz_mode -%d\n", val);
		} else {
			dev_err(cx_chg->dev, "cx25890h_not_exit_hiz_mode -%d\n", val);
		}
//-chk3594, liyiying.wt, 2022/7/18, add, disable hiz-mode when plug on otg

		ret = cx25890h_enable_otg(cx_chg);
		if (ret < 0) {
			dev_err(cx_chg->dev, "Failed to enable otg-%d\n", ret);
			return ret;
		}
		gpio_direction_output(cx_chg->otg_en_gpio, 1);
	} else {
		ret = cx25890h_disable_otg(cx_chg);
		if (ret < 0) {
			dev_err(cx_chg->dev, "Failed to disable otg-%d\n", ret);
		}
		gpio_direction_output(cx_chg->otg_en_gpio, 0);
	}

	return ret;
}

static int cx25890h_enable_charger(struct cx25890h_device *cx_chg)
{
	int ret =0;
	u8 val = CX25890H_CHG_ENABLE << CX25890H_CHG_CONFIG_SHIFT;

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_03, CX25890H_CHG_CONFIG_MASK, val);

	return ret;
}

static int cx25890h_disable_charger(struct cx25890h_device *cx_chg)
{
	int ret = 0;
	u8 val = CX25890H_CHG_DISABLE << CX25890H_CHG_CONFIG_SHIFT;

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_03, CX25890H_CHG_CONFIG_MASK, val);

	return ret;
}

static int cx25890h_set_chg_enable(struct cx25890h_device *cx_chg, int enable)
{
	int ret =0;

	dev_err(cx_chg->dev, "set charge: enable: %d\n", enable);
	if (enable) {
		cx25890h_chg_enable(cx_chg, true);
		cx25890h_enable_charger(cx_chg);
	} else {
		cx25890h_set_charge_current(cx_chg, 128);
		cx25890h_set_input_current_limit(cx_chg, 100);
		msleep(5);
		cx25890h_chg_enable(cx_chg, false);
		cx25890h_disable_charger(cx_chg);
	}

	return ret;
}

static int cx25890h_set_charge_current(struct cx25890h_device *cx_chg, int curr)
{
	u8 ichg;
	u8 val;

	dev_err(cx_chg->dev, "%s: %d\n", __func__, curr);

	//bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation
	cx_chg->final_cc = curr;

	ichg = (curr - CX25890H_ICHG_BASE)/CX25890H_ICHG_LSB;
	val= ichg << CX25890H_ICHG_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_04, CX25890H_ICHG_MASK, val);

}

static int cx25890h_set_term_current(struct cx25890h_device *cx_chg, int curr)
{
	u8 iterm;
	u8 val;

	dev_err(cx_chg->dev, "%s: %d\n", __func__, curr);
	if (curr < CX25890H_ITERM_MIN)
		curr = CX25890H_ITERM_MIN;
	if (curr > CX25890H_ITERM_MAX)
		curr = CX25890H_ITERM_MAX;
	iterm = (curr - CX25890H_ITERM_BASE) / CX25890H_ITERM_LSB;
	val = iterm << CX25890H_ITERM_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_05, CX25890H_ITERM_MASK, val);
}

static int cx25890h_set_prechg_current(struct cx25890h_device *cx_chg, int curr)
{
	u8 iprechg;
	u8 val;

	dev_err(cx_chg->dev, "%s: %d\n", __func__, curr);
	if (curr < 64)
		curr = 64;
	if (curr > 1024)
		curr = 1024;	
	iprechg = (curr - CX25890H_IPRECHG_BASE) / CX25890H_IPRECHG_LSB;
	val = iprechg << CX25890H_IPRECHG_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_05, CX25890H_IPRECHG_MASK, val);
}

static int cx25890h_set_vac_ovp(struct cx25890h_device *cx_chg, int ovp)
{
	int val;
	int ret1,ret2;

	if (ovp <= CX25890H_OVP_5000mV) {
		val = CX25890H_OVP_5500mV;
	} else if (ovp > CX25890H_OVP_5000mV && ovp <= CX25890H_OVP_6000mV) {
		val = CX25890H_OVP_6500mV;
	} else if (ovp > CX25890H_OVP_6000mV && ovp <= CX25890H_OVP_8000mV) {
		val = CX25890H_OVP_10500mV;
	} else {
		val = CX25890H_OVP_14000mV;
	}


	ret1=cx25890h_update_bits(cx_chg, CX25890H_REG_0F, CX25890H_ACOV_TH0_MASK, val<<CX25890H_ACOV_TH0_SHIFT);
	ret2=cx25890h_update_bits(cx_chg, CX25890H_REG_10, CX25890H_ACOV_TH1_MASK, val<<(CX25890H_ACOV_TH1_SHIFT-1));

	return ret1 | ret2;	
}

static int cx25890h_set_bootstv(struct cx25890h_device *cx_chg, int volt)
{
	int val;

	if(volt<CX25890H_BOOSTV_BASE)
		volt=CX25890H_BOOSTV_BASE;
	else if(volt>5510)
		volt=5510;

	val=(volt-CX25890H_BOOSTV_BASE)/CX25890H_BOOSTV_LSB;

	val = val << CX25890H_BOOSTV_SHIFT;
	return cx25890h_update_bits(cx_chg, CX25890H_REG_0A, CX25890H_BOOSTV_MASK, val);
}
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/17,kernel bringup Tidying up*/
static int cx25890h_enable_bc12_detect(struct cx25890h_device *cx_chg)
{
	int ret = 0;
	u8 val;

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
					 CX25890H_DM_VSET_MASK,  0x1<<CX25890H_DM_VSET_SHIFT); //DM=0
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
					CX25890H_DP_VSET_MASK,  0x1<<CX25890H_DP_VSET_SHIFT); //DP=0

	msleep(100);

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
					 CX25890H_DM_VSET_MASK,  0x0<<CX25890H_DM_VSET_SHIFT); //DM=HIZ
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
					 CX25890H_DP_VSET_MASK,  0x0<<CX25890H_DP_VSET_SHIFT); //DP=HIZ

	val = CX25890H_FORCE_DPDM << CX25890H_FORCE_DPDM_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_02, CX25890H_FORCE_DPDM_MASK, val);

	return ret;
}


static int cx25890h_set_charge_voltage(struct cx25890h_device *cx_chg, u32 mV)
{
	int ret=0;
	cx_chg->final_cv = mV * 1000;

	ret = cx25890h_set_chargevoltage(cx_chg, mV);

	return ret;
}
//-bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation

//+P86801EA2-300 gudi.wt battery protect function
#ifdef CONFIG_QGKI_BUILD
bool cx25890h_if_been_used(void)
{
	if(g_cx_chg == NULL){
		return false;
	}

	return true;
}
EXPORT_SYMBOL(cx25890h_if_been_used);
#endif
//-P86801EA2-300 gudi.wt battery protect function

static int cx25890h_enable_term(struct cx25890h_device *cx_chg, bool enable)
{
	u8 val;
	int ret;

	if (enable) {
		val = CX25890H_TERM_ENABLE << CX25890H_EN_TERM_SHIFT;
	} else {
		val = CX25890H_TERM_DISABLE << CX25890H_EN_TERM_SHIFT;
	}

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_07, CX25890H_EN_TERM_MASK, val);

	return ret;
}

static int cx25890h_enable_safety_timer(struct cx25890h_device *cx_chg, bool enable)
{
	u8 val;
	int ret;

	if (enable) {
		val = CX25890H_CHG_TIMER_ENABLE << CX25890H_EN_TIMER_SHIFT;
	} else {
		val = CX25890H_CHG_TIMER_DISABLE << CX25890H_EN_TIMER_SHIFT;
	}

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_07, CX25890H_EN_TIMER_MASK, val);

	return ret;
}

static int cx25890h_set_safety_timer(struct cx25890h_device *cx_chg, int hours)
{
	u8 val;

	if (hours <= 5)
		val = CX25890H_CHG_TIMER_5HOURS << CX25890H_CHG_TIMER_SHIFT;
	else if (hours <= 8)
		val = CX25890H_CHG_TIMER_8HOURS << CX25890H_CHG_TIMER_SHIFT;
	else if (hours <= 12)
		val = CX25890H_CHG_TIMER_12HOURS << CX25890H_CHG_TIMER_SHIFT;
	else
		val = CX25890H_CHG_TIMER_20HOURS << CX25890H_CHG_TIMER_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_07, CX25890H_CHG_TIMER_MASK, val);
}

//+bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation
static int cx25890h_disable_watchdog_timer(struct cx25890h_device *cx_chg, bool disable_wtd)
{
	u8 val;

	if (disable_wtd)
		val = CX25890H_WDT_DISABLE << CX25890H_WDT_SHIFT;
	else
		val = CX25890H_WDT_160S << CX25890H_WDT_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_07, CX25890H_WDT_MASK, val);
}

#if 0
static int cx25890h_feed_watchdog_timer(struct cx25890h_device *cx_chg)
{
	u8 val;
	int vbat_uV, ibat_ma;

	if (cx_chg->cfg.enable_ir_compensation) {
		cx25890h_get_ibat_curr(cx_chg, &ibat_ma);
		cx25890h_get_vbat_volt(cx_chg, &vbat_uV);
		if ((ibat_ma > 10) && (vbat_uV >= cx_chg->final_cv)) {
			cx25890h_dynamic_adjust_charge_voltage(cx_chg, vbat_uV);
		}
	}

	dev_err(cx_chg->dev, "main chg feed wtd \n");
	val = CX25890H_WDT_RESET << CX25890H_WDT_RESET_SHIFT;
	cx25890h_update_bits(cx_chg, CX25890H_REG_03, CX25890H_WDT_RESET_MASK, val);

	return 0;
}
#endif
//-bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation

static int cx25890h_reset_chip(struct cx25890h_device *cx_chg)
{
	u8 val;
	int ret;

	val = CX25890H_RESET << CX25890H_RESET_SHIFT;

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_14, CX25890H_RESET_MASK, val);

	return ret;
}

static int cx25890h_get_hiz_mode(struct cx25890h_device *cx_chg, u8 *state)
{
	u8 val;
	int ret;

	ret = cx25890h_read_byte(cx_chg, &val, CX25890H_REG_00);
	if (ret)
		return ret;

	*state = (val & CX25890H_ENHIZ_MASK) >> CX25890H_ENHIZ_SHIFT;

	return 0;
}

static int cx25890h_enter_hiz_mode(struct cx25890h_device *cx_chg)
{
	u8 val;

	val = CX25890H_HIZ_ENABLE << CX25890H_ENHIZ_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_00, CX25890H_ENHIZ_MASK, val);

}

static int cx25890h_exit_hiz_mode(struct cx25890h_device *cx_chg)
{

	u8 val;

	val = CX25890H_HIZ_DISABLE << CX25890H_ENHIZ_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_00, CX25890H_ENHIZ_MASK, val);

}

static int cx25890h_set_input_suspend(struct cx25890h_device *cx_chg, int suspend)
{

	int  ret = 0;

	dev_err(cx_chg->dev, "set input suspend: %d\n", suspend);

	if (suspend) {
		cx25890h_set_charge_current(cx_chg, 128);
		cx25890h_set_input_current_limit(cx_chg, 100);
		msleep(5);
		cx_chg->hiz_mode = true; //bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly
		cx25890h_enter_hiz_mode(cx_chg);
	} else {
		cx_chg->hiz_mode = false; //bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly
		cx25890h_exit_hiz_mode(cx_chg);
	}

	return ret;
}

//+P231018-03087 gudi.wt 20231103,fix add hiz func
void typec_set_input_suspend_for_usbif_cx25890h(bool enable)
{

	if(g_cx_chg == NULL){
		return;
	}

	dev_err(g_cx_chg->dev, "typec set input suspend: %d\n", enable);
	if (enable) {
		cx25890h_set_charge_current(g_cx_chg, 128);
		cx25890h_set_input_current_limit(g_cx_chg, 100);
		msleep(5);
		g_cx_chg->hiz_mode = true; //bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly
		cx25890h_enter_hiz_mode(g_cx_chg);
	} else {
		g_cx_chg->hiz_mode = false; //bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly
		cx25890h_exit_hiz_mode(g_cx_chg);
	}
}
//-P231018-03087 gudi.wt 20231103,fix add hiz func

static int cx25890h_enable_pumpx(struct cx25890h_device *cx_chg, bool enable)
{
	u8 val;

	if(enable) {
		val = CX25890H_PUMPX_ENABLE << CX25890H_EN_PUMPX_SHIFT;
	} else {
		val = CX25890H_PUMPX_DISABLE << CX25890H_EN_PUMPX_SHIFT;
	}

	return cx25890h_update_bits(cx_chg, CX25890H_REG_04, CX25890H_EN_PUMPX_MASK, val);
}


static int cx25890h_enable_battery_rst_en(struct cx25890h_device *cx_chg, bool enable)
{
	u8 val;

	if(enable) {
		val = CX25890H_BATFET_RST_ENABLE << CX25890H_BATFET_RST_EN_SHIFT;
	} else {
		val = CX25890H_BATFET_RST_DISABLE << CX25890H_BATFET_RST_EN_SHIFT;
	}

	return cx25890h_update_bits(cx_chg, CX25890H_REG_09, CX25890H_BATFET_RST_EN_MASK, val);
}

//+ bug 761884, tankaikun@wt, add 20220708, charger bring up
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/17,kernel bringup Tidying up*/
#ifdef CONFIG_CX25890H_ENABLE_HVDCP
static int cx25890h_set_dp0p6_for_afc(struct cx25890h_device *cx_chg)
{
	int ret;
	int dp_val;

	dp_val = 0x2<<CX25890H_DP_VSET_SHIFT;
    ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DP_VSET_MASK, dp_val); //dp 0.6V

	return ret;
}

static void cx25890h_set_afc_9v(struct cx25890h_device *cx_chg)
{
	int ret;

	dev_err(cx_chg->dev, "%s: cx afc enter:\n", __func__);
	ret = cx25890h_write_iio_prop(cx_chg, AFC, AFC_DETECT, MAIN_CHG);
	if (ret < 0) {
		dev_err(cx_chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}
#if 0
static void cx25890h_clear_afc_type(struct cx25890h_device *cx_chg)
{
	int ret;

	dev_err(cx_chg->dev, "%s: cx afc enter:\n", __func__);
	ret = cx25890h_write_iio_prop(cx_chg, AFC, AFC_TYPE, AFC_5V);
	if (ret < 0) {
		dev_err(cx_chg->dev, "%s: fail: %d\n", __func__, ret);
	}
	cx_chg->afc_type = AFC_5V;
}
#endif

#if 0
//step 1: dp=0.6V,dm=HiZ for 1.5s
//step 2: dp=HiZ,dm=HiZ for 500ms
static int cx25890h_enter_qc_hvdcp(struct cx25890h_device *cx_chg)
{
	int ret;
	int dp_val, dm_val;

    dp_val = 0x2<<CX25890H_DP_VSET_SHIFT;
    ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DP_VSET_MASK, dp_val); //dp 0.6V
    if (ret)
        return ret;

	dm_val = 0<<CX25890H_DM_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DM_VSET_MASK, dm_val); //dm Hiz
    if (ret)
        return ret;

	msleep(1500);

    dp_val = 0<<CX25890H_DP_VSET_SHIFT;
    ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DP_VSET_MASK, dp_val); //dp Hiz
    if (ret)
        return ret;

	dm_val = 0<<CX25890H_DM_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DM_VSET_MASK, dm_val); //dm Hiz
	msleep(500);
	return ret;
}
#endif
static int cx25890h_enable_qc20_hvdcp_5v(struct cx25890h_device *cx_chg)
{
	int ret;
	int dp_val, dm_val;

	/*dp and dm connected,dp 0.6V dm Hiz*/
    dp_val = 0x2<<CX25890H_DP_VSET_SHIFT;
    ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DP_VSET_MASK, dp_val); //dp 0.6V
    if (ret)
        return ret;

	dm_val = 0x1<<CX25890H_DM_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DM_VSET_MASK, dm_val); //dm=0
    if (ret)
        return ret;
	msleep(100);

	return ret;

}

static int cx25890h_enable_qc20_hvdcp_9v(struct cx25890h_device *cx_chg)
{
	int ret;
	int dp_val, dm_val;

	if (cx_chg->usb_online == 0) {
		pr_err("cx25890h_hvdcp_detect_work online = 0 return \n");
		return 0;
	}

	//cx25890h_enter_qc_hvdcp(cx_chg);

    dp_val = 0x2<<CX25890H_DP_VSET_SHIFT;
    ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DP_VSET_MASK, dp_val); //dp 0.6V
    if (ret)
        return ret;

	dm_val = 0<<CX25890H_DM_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DM_VSET_MASK, dm_val); //dm Hiz
    if (ret)
        return ret;
	msleep(100);

	/* dp 3.3v and dm 0.6v out 9V */
	dp_val = 0x6<<CX25890H_DP_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DP_VSET_MASK, dp_val); //dp 3.3v

	if (ret)
		return ret;

	dm_val = 0x2<<CX25890H_DM_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DM_VSET_MASK, dm_val); //dm 0.6v
	if (ret)
		return ret;

	return ret;

}

/* step 1. entry QC3.0 mode
   step 2. up or down 200mv
   step 3. retry step 2 */
#ifdef CONFIG_CX25890H_ENABLE_QC30
static int cx25890h_enable_qc30_hvdcp(struct cx25890h_device *cx_chg)
{
		int ret;
		int dp_val, dm_val;
	
		dp_val = 0x2<<CX25890H_DP_VSET_SHIFT;
		ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
					  CX25890H_DP_VSET_MASK, dp_val); //dp 0.6V
		if (ret)
			return ret;
	
		dm_val = 0x6<<CX25890H_DM_VSET_SHIFT;
		ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
					  CX25890H_DM_VSET_MASK, dm_val); //dm 3.3v
		if (ret)
			return ret;
		msleep(100);
	
		return ret;

}

// Must enter 3.0 mode to call ,otherwise cannot step correctly.
static int cx25890h_qc30_step_up_vbus(struct cx25890h_device *cx_chg)
{
	int ret;
	int dp_val;

	/*  dp 3.3v to dp 0.6v  step up 200mV when IC is QC3.0 mode*/
	dp_val = 0x6<<CX25890H_DP_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DP_VSET_MASK, dp_val); //dp 3.3v
	if (ret)
		return ret;
	msleep(5);
	dp_val = 0x2<<CX25890H_DP_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DP_VSET_MASK, dp_val); //dp 0.6v
	if (ret)
		return ret;

	msleep(5);
	return ret;
}
// Must enter 3.0 mode to call ,otherwise cannot step correctly.
static int cx25890h_qc30_step_down_vbus(struct cx25890h_device *cx_chg)
{
	int ret;
	int dm_val;

	/* dm 0.6v to 3.3v step down 200mV when IC is QC3.0 mode*/
	dm_val = 0x2<<CX25890H_DM_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DM_VSET_MASK, dm_val); //dm 0.6V
    if (ret)
        return ret;
	msleep(5);

	dm_val = 0x6<<CX25890H_DM_VSET_SHIFT;
	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
				  CX25890H_DM_VSET_MASK, dm_val); //dm 3.3v
	msleep(5);

	return ret;

}
#endif

static void cx25890h_remove_afc(struct cx25890h_device *cx_chg)
{
	int ret;

	ret = cx25890h_write_iio_prop(cx_chg, AFC, AFC_DETECH, MAIN_CHG);
	
	if (ret < 0) {
		dev_err(cx_chg->dev, "%s: cx cx25890h_remove_afc err:\n", __func__);
	}
}

#define HVDCP_QC20_VOLT_MIN 7000
#define HVDCP_AFC_VOLT_MIN 7000

static void cx25890h_charger_detect_workfunc(struct work_struct *work)
{
	int vbus_volt=0;
	int i=0;

	struct cx25890h_device *cx_chg = container_of(work,
								struct cx25890h_device, detect_work.work);

	if (cx_chg->vbus_type != CX25890H_VBUS_USB_AFC &&
			cx_chg->vbus_type != CX25890H_VBUS_USB_HVDCP &&
			cx_chg->vbus_type != CX25890H_VBUS_USB_HVDCP3) {
		cx25890h_get_vbus_volt(cx_chg, &vbus_volt);
		if (vbus_volt > HVDCP_AFC_VOLT_MIN) {
			//cx25890h_request_dpdm(cx_chg, false);
#ifdef CONFIG_QGKI_BUILD
			cx25890h_get_hv_dis_status(cx_chg);
#endif
			return;
		}
	}
	//start afc detect
	cx25890h_set_input_current_limit(cx_chg, 500);
	cx25890h_set_dp0p6_for_afc(cx_chg);
	cx25890h_set_afc_9v(cx_chg);
	// P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification
	for (i = 0; i <= AFC_DETECT_TIME; i++) {
		msleep(100);
		cx25890h_get_vbus_volt(cx_chg, &vbus_volt);
		dev_err(cx_chg->dev, "afc_detect: time: %d, vbus_volt: %d\n", i, vbus_volt);

		if (vbus_volt > HVDCP_AFC_VOLT_MIN && cx25890h_get_afc_type(cx_chg) == AFC_9V) {
			cx25890h_set_input_volt_limit(cx_chg, 8300);
			cx_chg->vbus_type = CX25890H_VBUS_USB_AFC;
			dev_err(cx_chg->dev, "afc_detect: succeed !\n");
			goto qc20_detect_exit;
		}

		if (cx_chg->vbus_good == false) {
			cx25890h_remove_afc(cx_chg);
			goto hvdcp_detect_exit;
		}
	}

	cx25890h_remove_afc(cx_chg);
	dev_err(cx_chg->dev, "afc_detect: fail !\n");
	//afc detect end

	//start QC20 detect
	cx25890h_enable_qc20_hvdcp_9v(cx_chg);
	// P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification
	for (i = 0; i <= QC_DETECT_TIME; i++) {
		msleep(100);
		cx25890h_get_vbus_volt(cx_chg, &vbus_volt);
		dev_err(cx_chg->dev, "qc20_detect: time: %d, vbus_volt: %d\n", i, vbus_volt);

		if (vbus_volt > HVDCP_QC20_VOLT_MIN) {
			cx25890h_set_input_volt_limit(cx_chg, 8300);
			cx_chg->vbus_type = CX25890H_VBUS_USB_HVDCP;
			dev_err(cx_chg->dev, "qc20_detect: succeed !\n");
			goto qc20_detect_exit;
		}

		if (cx_chg->vbus_good == false) {
			goto hvdcp_detect_exit;
		}
	}
	dev_err(cx_chg->dev, "qc20_detect: fail !\n");
	//QC20 detect end

//afc_detect_exit:
//	cx25890h_request_dpdm(cx_chg, false);
qc20_detect_exit:
	cx25890h_get_usb_type(cx_chg);
	cx25890h_update_wtchg_work(cx_chg);
#ifdef CONFIG_QGKI_BUILD
	cx25890h_get_hv_dis_status(cx_chg);
#endif
hvdcp_detect_exit:
	return;
}
#endif
/* CONFIG_CX25890H_ENABLE_HVDCP */

static void cx25890h_dp_dm(struct cx25890h_device *cx_chg, int val){
	cx25890h_request_dpdm(cx_chg, true);
	switch (val) {
	case QTI_POWER_SUPPLY_DP_DM_HVDCP3_SUPPORTED:
		/* force 5.6V */
		pr_err("cx25890h_dp_dm QTI_POWER_SUPPLY_DP_DM_HVDCP3_SUPPORTED \n");
		if(cx_chg->vbus_type == CX25890H_VBUS_USB_HVDCP3){
			pr_err("-------------------- request 5.6V\n");
			for(; cx_chg->pulse_cnt<3; cx_chg->pulse_cnt++){
				//cx25890h_qc30_step_up_vbus(cx_chg);
				msleep(20);
			}
		}
		break;
	case QTI_POWER_SUPPLY_DP_DM_FORCE_5V:
		pr_err("cx25890h_dp_dm QTI_POWER_SUPPLY_DP_DM_FORCE_5V \n");
		cx25890h_enable_qc20_hvdcp_5v(cx_chg);
		break;
	case QTI_POWER_SUPPLY_DP_DM_FORCE_9V:
		pr_err("cx25890h_dp_dm QTI_POWER_SUPPLY_DP_DM_FORCE_9V \n");
		cx25890h_enable_qc20_hvdcp_9v(cx_chg);
		break;
	case QTI_POWER_SUPPLY_DP_DM_FORCE_12V:
		break;
	case QTI_POWER_SUPPLY_DP_DM_CONFIRMED_HVDCP3P5:
		break;
	default:
		break;
	}
	cx25890h_request_dpdm(cx_chg, false);
}
//- bug 761884, tankaikun@wt, add 20220708, charger bring up
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/17,kernel bringup Tidying up*/
static int cx25890h_get_chg_status(struct cx25890h_device *cx_chg)
{
	int ret;
	u8 status = 0;
	u8 charge_status = 0;

	/* Read STATUS registers */
	ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_0B);
	if (ret) {
		dev_err(cx_chg->dev, "%s: read regs:0x0b fail !\n", __func__);
		return cx_chg->chg_status;
	}

	charge_status = (status & CX25890H_CHRG_STAT_MASK) >> CX25890H_CHRG_STAT_SHIFT;
	switch (charge_status) {
	case CX25890H_NOT_CHARGING:
		cx_chg->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case CX25890H_PRE_CHARGE:
	case CX25890H_FAST_CHARGING:
		if (cx_chg->hiz_mode) {
			cx_chg->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			cx_chg->chg_status = POWER_SUPPLY_STATUS_CHARGING;
		}
		break;
	case CX25890H_CHARGE_DONE:
		cx_chg->chg_status = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		cx_chg->chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return cx_chg->chg_status;
}

static int cx25890h_get_vbus_type(struct cx25890h_device *cx_chg)
{
	int i, ret;
	u8 status = 0;
	u8 bc12_vbus_type = 0;
	u8 input_current = 0;
	int bc12_current;

	//get vbus_type
	for (i = 1; i <= 3; i++) {
		ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_0B);
		if (!ret) {
			break;
		}
		dev_err(cx_chg->dev, "read regs:0x0b fail count: %d !\n", i);
		mdelay(5);
	}
	bc12_vbus_type = (status & CX25890H_VBUS_STAT_MASK) >> CX25890H_VBUS_STAT_SHIFT;

	//get input current limit by bc12 detect done
	for (i = 1; i <= 3; i++) {
		ret = cx25890h_read_byte(cx_chg, &input_current, CX25890H_REG_00);
		if (!ret) {
			break;
		}
		dev_err(cx_chg->dev, "read regs:0x00 fail count: %d !\n", i);
		mdelay(5);
	}
	input_current = (input_current & CX25890H_IINLIM_MASK);
	bc12_current = (input_current * CX25890H_IINLIM_LSB) + CX25890H_IINLIM_BASE;

	if (bc12_vbus_type == CX25890H_VBUS_NONSTAND) {
		if (bc12_current <= CX25890H_VBUS_NONSTAND_1000MA) {
			cx_chg->vbus_type = CX25890H_VBUS_NONSTAND_1A;
		} else if (bc12_current <= CX25890H_VBUS_NONSTAND_1500MA) {
			cx_chg->vbus_type = CX25890H_VBUS_NONSTAND_1P5A;
		} else {
			cx_chg->vbus_type = CX25890H_VBUS_NONSTAND_2A;
		}
	} else {
		cx_chg->vbus_type = bc12_vbus_type;
	}

	//If the vbus_type detect is incorrect, force vbus_type to usb
	if (cx_chg->bc12_float_check <= BC12_FLOAT_CHECK_MAX
			&& (cx_chg->vbus_type == CX25890H_VBUS_NONE
				|| cx_chg->vbus_type == CX25890H_VBUS_UNKNOWN)) {
		cx_chg->vbus_type = CX25890H_VBUS_USB_SDP;
	} else if (cx_chg->bc12_float_check > BC12_FLOAT_CHECK_MAX
					&& (cx_chg->vbus_type == CX25890H_VBUS_USB_SDP
						|| cx_chg->vbus_type == CX25890H_VBUS_NONE
						|| cx_chg->vbus_type == CX25890H_VBUS_UNKNOWN)) {
		cx_chg->vbus_type = CX25890H_VBUS_USB_DCP;
	}

	dev_err(cx_chg->dev, "bc12_vbus_type: %d, bc12_current: %d, vbus_type: %d, bc12_check: %d\n",
					bc12_vbus_type, bc12_current, cx_chg->vbus_type, cx_chg->bc12_float_check);

	return cx_chg->vbus_type;
}

static int cx25890h_get_usb_type(struct cx25890h_device *cx_chg)
{
	switch (cx_chg->vbus_type) {
	case CX25890H_VBUS_NONE:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case CX25890H_VBUS_USB_SDP:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CX25890H_VBUS_USB_CDP:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case CX25890H_VBUS_USB_DCP:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case CX25890H_VBUS_USB_HVDCP:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_HVDCP;
		break;
	case CX25890H_VBUS_USB_HVDCP3:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_HVDCP_3;
		break;
	case CX25890H_VBUS_UNKNOWN:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		break;
#ifdef CONFIG_QGKI_BUILD
	case CX25890H_VBUS_NONSTAND_1A:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_NONSTAND_1A;
		break;
	case CX25890H_VBUS_AHVDCP:
	case CX25890H_VBUS_NONSTAND_1P5A:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_NONSTAND_1P5A;
		break;
	case CX25890H_VBUS_NONSTAND_2A:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case CX25890H_VBUS_USB_AFC:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_AFC;
		break;
	case CX25890H_VBUS_USB_PE:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_PE;
		break;
#endif
	default:
		cx_chg->usb_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		break;
	}

	return cx_chg->usb_type;
}

static int cx25890h_get_vbus_online(struct cx25890h_device *cx_chg)
{
	int ret;
	u8 val=0;
#if 0
	u8 status = 0;
        /*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_0B);
	if (ret) {
		dev_err(cx_chg->dev, "read regs:0x0B fail !\n");
		return ret;
	}
	cx_chg->usb_online = (status & CX25890H_PG_STAT_MASK) >> CX25890H_PG_STAT_SHIFT;
        /*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
#endif
	//+ bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly
	if(cx_chg->usb_online && cx_chg->hiz_mode) {
		ret = cx25890h_get_hiz_mode(cx_chg, &val);
		if (ret < 0) {
			val = 0;
			dev_err(cx_chg->dev, "Failed to get hiz val-%d\n", ret);
		}
		cx_chg->hiz_mode = val;

		if (cx_chg->hiz_mode) {
			cx_chg->usb_online = 0;
			cx_chg->vbus_good = 0;
		}
	}
	//+ bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly

	return cx_chg->vbus_good;
}

static void cx25890h_dump_regs(struct cx25890h_device *cx_chg)
{

	int addr, ret;
	u8 val;

	dev_err(cx_chg->dev, "cx25890h_dump_regs:\n");
	for (addr = 0; addr <= CX25890H_REGISTER_MAX; addr++) {
		ret = cx25890h_read_byte_without_retry(cx_chg, &val, addr);
		if (ret == 0) {
			dev_err(cx_chg->dev, "Reg[%.2x] = 0x%.2x \n", addr, val);
		}
	}

}

static int cx25890h_detect_device(struct cx25890h_device *cx_chg)
{
	int ret;
	u8 data;

	ret = cx25890h_read_byte(cx_chg, &data, CX25890H_REG_14);
	if (ret == 0) {
		cx_chg->part_no = (data & CX25890H_PN_MASK) >> CX25890H_PN_SHIFT;
		cx_chg->revision = (data & CX25890H_DEV_REV_MASK) >> CX25890H_DEV_REV_SHIFT;
	}

	return ret;
}

static int cx25890h_request_dpdm(struct cx25890h_device *cx_chg, bool enable)
{
	int rc = 0;
	int ret;
	u8 dp_val,dm_val;

	/* fetch the DPDM regulator */
	if (!cx_chg->dpdm_reg && of_get_property(cx_chg->dev->of_node,
				"dpdm-supply", NULL)) {
		cx_chg->dpdm_reg = devm_regulator_get(cx_chg->dev, "dpdm");
		if (IS_ERR(cx_chg->dpdm_reg)) {
			rc = PTR_ERR(cx_chg->dpdm_reg);
			pr_err("Couldn't get dpdm regulator rc=%d\n",rc);
			cx_chg->dpdm_reg = NULL;
			return rc;
		}
	}

	//mutex_lock(&cx_chg->dpdm_lock);
	if (enable) {
		if (cx_chg->dpdm_reg && !cx_chg->dpdm_enabled) {
			pr_err("enabling DPDM regulator\n");
			rc = regulator_enable(cx_chg->dpdm_reg);
			if (rc < 0)
				pr_err("Couldn't enable dpdm regulator rc=%d\n",rc);
			else
				cx_chg->dpdm_enabled = true;
		}
	} else {
		if (cx_chg->dpdm_reg && cx_chg->dpdm_enabled) {
			//reset DP DM
		    dp_val = 0<<CX25890H_DP_VSET_SHIFT;
		    ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
						  CX25890H_DP_VSET_MASK, dp_val); //dp Hiz
		    if (ret)
		        return ret;

			dm_val = 0<<CX25890H_DM_VSET_SHIFT;
			ret = cx25890h_update_bits(cx_chg, CX25890H_REG_15,
						  CX25890H_DM_VSET_MASK, dm_val); //dm Hiz
		    if (ret)
		        return ret;

			pr_err("disabling DPDM regulator\n");
			rc = regulator_disable(cx_chg->dpdm_reg);
			if (rc < 0)
				pr_err("Couldn't disable dpdm regulator rc=%d\n",rc);
			else
				cx_chg->dpdm_enabled = false;
		}
	}
	//mutex_unlock(&cx_chg->dpdm_lock);

	return rc;
}

//+ bug 761884, tankaikun@wt, add 20220701, charger bring up
#ifdef CONFIG_MAIN_CHG_EXTCON
static void cx25890h_notify_extcon_props(struct cx25890h_device *cx_chg, int id)
{
	union extcon_property_value val;

	val.intval = false;
	extcon_set_property(cx_chg->extcon, id,
			EXTCON_PROP_USB_SS, val);
}

static void cx25890h_notify_device_mode(struct cx25890h_device *cx_chg, bool enable)
{
	if (enable)
		cx25890h_notify_extcon_props(cx_chg, EXTCON_USB);

	extcon_set_state_sync(cx_chg->extcon, EXTCON_USB, enable);
}
#endif /* CONFIG_MAIN_CHG_EXTCON */
//- bug 761884, tankaikun@wt, add 20220701, charger bring up
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
static int cx25890h_enable_ico(struct cx25890h_device* cx_chg, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = CX25890H_ICO_ENABLE << CX25890H_ICOEN_SHIFT;
	else
		val = CX25890H_ICO_DISABLE << CX25890H_ICOEN_SHIFT;

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_02, CX25890H_ICOEN_MASK, val);

	return ret;

}

static int cx25890h_use_absolute_vindpm(struct cx25890h_device* cx_chg, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = CX25890H_FORCE_VINDPM_ENABLE << CX25890H_FORCE_VINDPM_SHIFT;
	else
		val = CX25890H_FORCE_VINDPM_DISABLE << CX25890H_FORCE_VINDPM_SHIFT;

	ret = cx25890h_update_bits(cx_chg, CX25890H_REG_0D, CX25890H_FORCE_VINDPM_MASK, val);

	return ret;

}

int cx25890h_set_input_volt_limit(struct cx25890h_device *cx_chg, int volt)
{
	u8 val;
	val = (volt - CX25890H_VINDPM_BASE) / CX25890H_VINDPM_LSB;
	cx25890h_use_absolute_vindpm(cx_chg, true);
	return cx25890h_update_bits(cx_chg, CX25890H_REG_0D, CX25890H_VINDPM_MASK, val << CX25890H_VINDPM_SHIFT);
}

int cx25890h_en_hvdcp(struct cx25890h_device *cx_chg, bool en)
{
	u8 val;

	val = en << CX25890H_HVDCPEN_SHIFT;

	return cx25890h_update_bits(cx_chg, CX25890H_REG_02,CX25890H_HVDCPEN_MASK, val);

}

int cx25890h_charge_done(struct cx25890h_device *cx_chg,int *is_done)
{
	int ret;
	u8 data;

	ret = cx25890h_read_byte(cx_chg, &data, CX25890H_REG_0B);
	*is_done = ((((data & CX25890H_VBUS_STAT_MASK)>>CX25890H_VBUS_STAT_SHIFT))==CX25890H_CHRG_STAT_CHGDONE);
	return ret;
}
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
static int cx25890h_write_reg40(struct cx25890h_device *cx_chg,bool enable)
{
	int ret,try_count=10;
	u8 read_val;
	
    if(enable)
    {
        while(try_count--)
        {
            ret=cx25890h_write_byte_without_retry(cx_chg, 0x40, 0x00);
            ret=cx25890h_write_byte_without_retry(cx_chg, 0x40, 0x50);
            ret=cx25890h_write_byte_without_retry(cx_chg, 0x40, 0x57);
            ret=cx25890h_write_byte_without_retry(cx_chg, 0x40, 0x44);
            
            ret = cx25890h_read_byte_without_retry(cx_chg,&read_val,0x40);
            if(0x03 == read_val)
            {
                ret=1;
                break;
            }
        }
    }
    else
    {
        ret=cx25890h_write_byte_without_retry(cx_chg, 0x40, 0x00);
    }

	return ret;
}

static int cx25890h_init_charge(struct cx25890h_device *cx_chg)
{
	int ret;
	u8 data;
	
    //ret = cx25890h_update_bits(cx_chg, CX25890H_REG_02, CX25890H_AUTO_DPDM_EN_MASK, CX25890H_AUTO_DPDM_DISABLE);
	ret = cx25890h_write_reg40(cx_chg,true);
	if(ret==1)
		pr_err("write reg40 success\n");
	ret = cx25890h_write_byte(cx_chg, 0x41, 0x08);
	ret = cx25890h_write_byte(cx_chg, 0x44, 0x18);
	ret = cx25890h_write_byte(cx_chg, 0x88, 0x02);
	ret = cx25890h_read_byte(cx_chg, &data, 0x41);
	if (data != 0x08) {
		pr_err("Failed to write: reg=%02X, data=%02X\n", 0x41, data);
	}
	else {
		pr_err("write reg41 OK\n");
	}

	ret = cx25890h_write_reg40(cx_chg,false); 
	return ret;
}


static int cx25890h_init_device(struct cx25890h_device *cx_chg)
{
	int ret;

	cx25890h_reset_chip(cx_chg);
	msleep(50);

	cx25890h_init_charge(cx_chg);
	/*common initialization*/
	cx25890h_disable_watchdog_timer(cx_chg, 1);
	cx25890h_ship_mode_delay_enable(cx_chg, false);
	cx25890h_set_otg(cx_chg, false);
	cx25890h_set_input_suspend(cx_chg, false);
	cx25890h_set_vac_ovp(cx_chg, CX25890H_OVP_10000mV);
	cx25890h_set_bootstv(cx_chg, CX25890H_BOOSTV_4998mV);
	cx25890h_set_otg_current(cx_chg, 1650);
	cx25890h_jeita_enable(cx_chg, false);
	cx25890h_enable_pumpx(cx_chg, true);
	cx25890h_enable_battery_rst_en(cx_chg, false);
	cx_chg->cfg.disable_hvdcp=0;
	ret = cx25890h_set_term_current(cx_chg, cx_chg->cfg.term_current);
	if (ret < 0) {
		dev_err(cx_chg->dev, "Failed to set termination current:%d\n", ret);
		return ret;
	}

	ret = cx25890h_set_charge_voltage(cx_chg, cx_chg->cfg.charge_voltage);
	if (ret < 0) {
		dev_err(cx_chg->dev, "Failed to set charge voltage:%d\n",  ret);
		return ret;
	}

	ret = cx25890h_set_charge_current(cx_chg, cx_chg->cfg.charge_current);
	if (ret < 0) {
		dev_err(cx_chg->dev, "Failed to set charge current:%d\n", ret);
		return ret;
	}

	ret = cx25890h_set_prechg_current(cx_chg, cx_chg->cfg.prechg_current);
	if (ret < 0) {
		dev_err(cx_chg->dev, "Failed to set charge current:%d\n", ret);
		return ret;
	}
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	cx25890h_update_bits(cx_chg, CX25890H_REG_00, CX25890H_ENILIM_MASK, CX25890H_ENILIM_DISABLE << CX25890H_ENILIM_SHIFT); //disable ENILIM
	cx25890h_enable_ico(cx_chg, 0);  //disable ico
	cx25890h_use_absolute_vindpm(cx_chg, true);
	cx25890h_set_input_volt_limit(cx_chg, 4700);

	cx25890h_en_hvdcp(cx_chg, false);
	cx25890h_set_input_current_limit(cx_chg, 500);
	cx25890h_enable_term(cx_chg, true);
	cx25890h_enable_safety_timer(cx_chg, true);
	cx25890h_set_safety_timer(cx_chg, 20);
	cx25890h_set_chg_enable(cx_chg, true);
	cx25890h_dump_regs(cx_chg);
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	return ret;
}

static ssize_t cx25890h_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "Charger 1");
	for (addr = 0x0; addr <= CX25890H_REGISTER_MAX; addr++) {
		ret = cx25890h_read_byte_without_retry(g_cx_chg, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[0x%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static DEVICE_ATTR(registers, S_IRUGO, cx25890h_show_registers, NULL);

static struct attribute *cx25890h_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group cx25890h_attr_group = {
	.attrs = cx25890h_attributes,
};

//+bug 816469,tankaikun.wt,add,2023/1/30, IR compensation
static void cx25890h_dynamic_adjust_charge_voltage(struct cx25890h_device *cx_chg, int vbat)
{
	int ret;
	int cv_adjust = 0;
	u8 status = 0;
	int tune_cv=0;
	int min_tune_cv=3;

	if(time_is_before_jiffies(cx_chg->dynamic_adjust_charge_update_timer + 5*HZ)) {
		cx_chg->dynamic_adjust_charge_update_timer = jiffies;
		if (cx_chg->final_cv > vbat && (cx_chg->final_cv - vbat) < 9000)
			return;

		ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_13);
		if ((ret==0) && ((status & CX25890H_VDPM_STAT_MASK) ||
				(status & CX25890H_IDPM_STAT_MASK)) &&
				(cx_chg->final_cv > vbat))
			return;

		tune_cv = cx_chg->tune_cv;

		if (cx_chg->final_cv > vbat) {
			tune_cv++;
		} else if (cx_chg->tune_cv > 0) {
			tune_cv--;
		}

		if (cx_chg->final_cc < 2000) {
			min_tune_cv = 0;
		}
		cx_chg->tune_cv = min(tune_cv, min_tune_cv); // 3*8=24mv

		dev_err(cx_chg->dev, "cx25890h_dynamic_adjust_charge_voltage tune_cv = %d \n", cx_chg->tune_cv);
		cv_adjust = cx_chg->tune_cv * 8000;
		cv_adjust += cx_chg->final_cv;
		cv_adjust /= 1000;

		dev_err(cx_chg->dev, "cx25890h_dynamic_adjust_charge_voltage cv_adjust=%d min_tune_cv=%d \n",cv_adjust, min_tune_cv);
		ret |= cx25890h_set_chargevoltage(cx_chg, cv_adjust);
		if (!ret)
			return;

		cx_chg->tune_cv = 0;
		cx25890h_set_chargevoltage(cx_chg, cx_chg->final_cv/1000);
	}

	return;
}
//-bug 816469,tankaikun.wt,add,2023/1/30, IR compensation

static void cx25890h_monitor_workfunc(struct work_struct *work)
{
	int ret;
	u8 status = 0;
	u8 fault = 0;
	//u8 val;
	u8 charge_status = 0;
	int vbus_volt=0;
	int vbus_type=0;
	struct cx25890h_device *cx_chg = container_of(work,
								struct cx25890h_device, monitor_work.work);
	int vbat_uV, ibat_ma;

	/* Read STATUS and FAULT registers */
	ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_0B);
	if (ret) {
		dev_err(cx_chg->dev, "read regs:0x0B fail !\n");
		schedule_delayed_work(&cx_chg->monitor_work, msecs_to_jiffies(10000));
		return;
	}

	ret = cx25890h_read_byte_without_retry(cx_chg, &fault, CX25890H_REG_0C);
	if (ret) {
		dev_err(cx_chg->dev, "read regs:0x0C fail !\n");
		schedule_delayed_work(&cx_chg->monitor_work, msecs_to_jiffies(10000));
		return;
	}
	cx25890h_get_vbus_volt(cx_chg, &vbus_volt);
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	if(vbus_volt>8000)
		cx25890h_set_input_volt_limit(cx_chg, 8300);
	else
		cx25890h_set_input_volt_limit(cx_chg, 4700);

/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	vbus_type = (status & CX25890H_VBUS_STAT_MASK) >> CX25890H_VBUS_STAT_SHIFT;
	cx_chg->usb_online = (status & CX25890H_PG_STAT_MASK) >> CX25890H_PG_STAT_SHIFT;
	charge_status = (status & CX25890H_CHRG_STAT_MASK) >> CX25890H_CHRG_STAT_SHIFT;
	switch (charge_status) {
	case CX25890H_NOT_CHARGING:
		cx_chg->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case CX25890H_PRE_CHARGE:
	case CX25890H_FAST_CHARGING:
		cx_chg->chg_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case CX25890H_CHARGE_DONE:
		cx_chg->chg_status = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		cx_chg->chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	//+bug816469,tankaikun.wt,add,2023/1/30, IR compensation
	if (cx_chg->cfg.enable_ir_compensation && POWER_SUPPLY_TYPE_USB_DCP == cx_chg->usb_type) {
		cx25890h_get_ibat_curr(cx_chg, &ibat_ma);
		cx25890h_get_vbat_volt(cx_chg, &vbat_uV);
		if ((ibat_ma > 10) || (vbat_uV > cx_chg->final_cv)) {
			cx25890h_dynamic_adjust_charge_voltage(cx_chg, vbat_uV);
		}
		dev_err(cx_chg->dev, "ibat_ma=%d final_cc=%d vbat_uV=%d final_cv=%d\n",
					ibat_ma, cx_chg->final_cc, vbat_uV, cx_chg->final_cv);
	}
	//-bug816469,tankaikun.wt,add,2023/1/30, IR compensation

	//cx25890h_dump_regs(cx_chg);
	dev_err(cx_chg->dev, "%s:usb_online: %d, chg_status: %d, chg_fault: 0x%x vbus_type:%d vbus_volt=%d\n",
							__func__, cx_chg->usb_online, cx_chg->chg_status, fault, vbus_type, vbus_volt);

	schedule_delayed_work(&cx_chg->monitor_work, msecs_to_jiffies(10000));
}
//+ bug 761884, tankaikun@wt, add 20220701, charger bring up

//gudi
extern void charger_enable_device_mode(bool enable);
//gudi
static void cx25890h_rerun_apsd_workfunc(struct work_struct *work)
{
	struct cx25890h_device *cx_chg = container_of(work,
			struct cx25890h_device, rerun_apsd_work.work);

	if(!cx25890h_wake_active(&cx_chg->cx25890h_apsd_wake_source)){
		cx25890h_stay_awake(&cx_chg->cx25890h_apsd_wake_source);
	}

	if ((!cx_chg->init_detect) 
			|| (cx_chg->usb_online 
					&& (cx_chg->usb_type == POWER_SUPPLY_TYPE_USB_FLOAT
						|| cx_chg->usb_type == POWER_SUPPLY_TYPE_USB))) {
		cx_chg->init_detect = true;
		cx_chg->apsd_rerun = true;

		pr_err("wt rerun apsd begin\n");
		cx25890h_request_dpdm(cx_chg, true);
		mdelay(10);
		cx25890h_enable_bc12_detect(cx_chg);
	}

	if(cx25890h_wake_active(&cx_chg->cx25890h_apsd_wake_source)){
		cx25890h_relax(&cx_chg->cx25890h_apsd_wake_source);
	}
}
//- bug 761884, tankaikun@wt, add 20220701, charger bring up

struct iio_channel ** cx25890h_get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}

	return iio_ch_ext;
}

static bool cx25890h_is_afc_chan_valid(struct cx25890h_device *chg,
		enum afc_chg_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->afc_ext_iio_chans) {
		iio_list = cx25890h_get_ext_channels(chg->dev, afc_chg_iio_chan_name,
		ARRAY_SIZE(afc_chg_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->afc_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->afc_ext_iio_chans = iio_list;
	}

	return true;
}

static bool cx25890h_is_pmic_chan_valid(struct cx25890h_device *chg,
		enum pmic_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->pmic_ext_iio_chans) {
		iio_list = cx25890h_get_ext_channels(chg->dev, pmic_iio_chan_name,
		ARRAY_SIZE(pmic_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->pmic_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->pmic_ext_iio_chans = iio_list;
	}

	return true;
}

static bool cx25890h_is_batt_qg_chan_valid(struct cx25890h_device *chg,
		enum batt_qg_exit_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->qg_batt_ext_iio_chans) {
		iio_list = cx25890h_get_ext_channels(chg->dev, qg_ext_iio_chan_name,
		ARRAY_SIZE(qg_ext_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->qg_batt_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->qg_batt_ext_iio_chans = iio_list;
	}

	return true;
}

static bool  cx25890h_is_wtchg_chan_valid(struct cx25890h_device *chg,
		enum wtcharge_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->wtchg_ext_iio_chans) {
		iio_list = cx25890h_get_ext_channels(chg->dev, wtchg_iio_chan_name,
		ARRAY_SIZE(wtchg_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->wtchg_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->wtchg_ext_iio_chans = iio_list;
	}

	return true;
}

static int cx25890h_read_iio_prop(struct cx25890h_device *cx_chg,
				enum cx25890h_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;
	switch (type) {
	case WT_CHG:
		if (!cx25890h_is_wtchg_chan_valid(cx_chg, channel)) {
			return -ENODEV;
		}
		iio_chan_list = cx_chg->wtchg_ext_iio_chans[channel];
		break;
	case AFC:
		dev_err(cx_chg->dev,"afc cx25890h_is_afc_chan_valid\n");
		if (!cx25890h_is_afc_chan_valid(cx_chg, channel)) {
			return -ENODEV;
		}
		iio_chan_list = cx_chg->afc_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}
	rc = iio_read_channel_processed(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}

static int cx25890h_write_iio_prop(struct cx25890h_device *cx_chg,
				enum cx25890h_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int ret;

	switch (type) {
	case WT_CHG:
		if (!cx25890h_is_wtchg_chan_valid(cx_chg, channel)) {
			return -ENODEV;
		}
		iio_chan_list = cx_chg->wtchg_ext_iio_chans[channel];
		break;
	case AFC:
		dev_err(cx_chg->dev,"afc cx25890h_is_afc_chan_valid\n");
		if (!cx25890h_is_afc_chan_valid(cx_chg, channel)) {
			return -ENODEV;
		}
		iio_chan_list = cx_chg->afc_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	ret = iio_write_channel_raw(iio_chan_list, val);

	return ret < 0 ? ret : 0;
}

/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/

int cx25890h_get_vbus_volt(struct cx25890h_device *cx_chg,  int *val)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!cx25890h_is_pmic_chan_valid(cx_chg, VBUS_VOLTAGE)) {
		dev_err(cx_chg->dev,"read vbus_dect channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = cx_chg->pmic_ext_iio_chans[VBUS_VOLTAGE];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(cx_chg->dev,
		"read vbus_dect channel fail, ret=%d\n", ret);
		return ret;
	}
	*val = temp / 100;
	dev_err(cx_chg->dev,"%s: vbus_volt = %d \n", __func__, *val);

	return ret;
}

int cx25890h_get_vbat_volt(struct cx25890h_device *cx_chg,  int *val)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!cx25890h_is_batt_qg_chan_valid(cx_chg, BATT_QG_VOLTAGE_NOW)) {
		dev_err(cx_chg->dev,"read BATT_QG_VOLTAGE_NOW channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = cx_chg->qg_batt_ext_iio_chans[BATT_QG_VOLTAGE_NOW];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(cx_chg->dev,
		"read BATT_QG_VOLTAGE_NOW channel fail, ret=%d\n", ret);
		return ret;
	}
	*val = temp / 1000;
	dev_err(cx_chg->dev,"%s: vbat_volt = %d \n", __func__, *val);
	return ret;

}

int cx25890h_get_ibat_curr(struct cx25890h_device *cx_chg,  int *val)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!cx25890h_is_batt_qg_chan_valid(cx_chg, BATT_QG_CURRENT_NOW)) {
		dev_err(cx_chg->dev,"read BATT_QG_VOLTAGE_NOW channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = cx_chg->qg_batt_ext_iio_chans[BATT_QG_CURRENT_NOW];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(cx_chg->dev,
		"read BATT_QG_VOLTAGE_NOW channel fail, ret=%d\n", ret);
		return ret;
	}
	*val = temp;
	dev_err(cx_chg->dev,"%s: ibat_curr = %d \n", __func__, *val);
	return ret;

}

//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
static int cx25890h_get_hv_dis_status(struct cx25890h_device *cx_chg)
{
	int ret;
	int val;

	ret = cx25890h_read_iio_prop(cx_chg, WT_CHG, HV_DISABLE_DETECT, &val);
	if (ret < 0)
	{
		dev_err(cx_chg->dev, "%s: failed: %d --", __func__, ret);
	} else {
		dev_err(cx_chg->dev, "%s: read HV_DISABLE_DETECT return val = %d --", __func__, val);
	}

	return val;
}
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
static void cx25890h_update_wtchg_work(struct cx25890h_device *cx_chg)
{
	int ret;

	ret = cx25890h_write_iio_prop(cx_chg, WT_CHG, WTCHG_UPDATE_WORK, MAIN_CHG);
	if (ret < 0) {
		dev_err(cx_chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}


static int cx25890h_get_afc_type(struct cx25890h_device *cx_chg)
{
	int ret;
	int val;

	ret = cx25890h_read_iio_prop(cx_chg, AFC, AFC_TYPE, &val);
	if (ret < 0)
	{
		dev_err(cx_chg->dev, "%s: failed: %d", __func__, ret);
	} else {
		cx_chg->afc_type = val;
	}
	return cx_chg->afc_type;
}

static void cx25890h_charger_check_adapter_workfunc(struct work_struct *work)
{
	int ret;
	u8 status;

	struct cx25890h_device *cx_chg = container_of(work,
								struct cx25890h_device, check_adapter_work.work);
	printk("Disable check_adapter function\n");
	return;
	dev_err(cx_chg->dev, "adapter check enter\n");

	ret = cx25890h_set_input_current_limit(cx_chg, 400);
	msleep(90);
	ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_13);
	cx_chg->vindpm_stat = (status & CX25890H_VDPM_STAT_MASK) >> CX25890H_VDPM_STAT_SHIFT;
	if (cx_chg->vindpm_stat) {
		ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_00);
		dev_err(cx_chg->dev, "bad adapter ! input current = 400 reg00 = 0x%02x\n", status);
		goto ADAPTER_CHECK_END;
	}


	ret = cx25890h_set_input_current_limit(cx_chg, 500);
	msleep(90);
	ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_13);
	cx_chg->vindpm_stat = (status & CX25890H_VDPM_STAT_MASK) >> CX25890H_VDPM_STAT_SHIFT;
	if (cx_chg->vindpm_stat) {
		ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_00);
		dev_err(cx_chg->dev, "slow adapter ! input current = 500 reg00 = 0x%02x\n", status);
		goto ADAPTER_CHECK_END;
	}

	cx25890h_get_usb_type(cx_chg);
	if(cx_chg->vbus_type == CX25890H_VBUS_USB_SDP){
		dev_err(cx_chg->dev, "USB SDP type, check end\n");
		goto ADAPTER_CHECK_END;
	}

	ret = cx25890h_set_input_current_limit(cx_chg, 700);
	msleep(90);
	ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_13);
	cx_chg->vindpm_stat = (status & CX25890H_VDPM_STAT_MASK) >> CX25890H_VDPM_STAT_SHIFT;
	if (cx_chg->vindpm_stat) {
		ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_00);
		dev_err(cx_chg->dev, "slow adapter ! input current = 700 reg00 = 0x%02x\n", status);
		goto ADAPTER_CHECK_END;
	}


	ret = cx25890h_set_input_current_limit(cx_chg, 1000);
	msleep(90);
	ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_13);
	cx_chg->vindpm_stat = (status & CX25890H_VDPM_STAT_MASK) >> CX25890H_VDPM_STAT_SHIFT;
	if (cx_chg->vindpm_stat) {
		ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_00);
		dev_err(cx_chg->dev, "slow adapter ! input current = 1000 reg00 = 0x%02x\n", status);
		goto ADAPTER_CHECK_END;
	}


	ret = cx25890h_set_input_current_limit(cx_chg, 1200);
	msleep(90);
	ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_13);
	cx_chg->vindpm_stat = (status & CX25890H_VDPM_STAT_MASK) >> CX25890H_VDPM_STAT_SHIFT;
	if (cx_chg->vindpm_stat) {
		ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_00);
		dev_err(cx_chg->dev, "slow adapter ! input current = 1200 reg00 = 0x%02x\n", status);
		goto ADAPTER_CHECK_END;
	}

	dev_err(cx_chg->dev, "good adapter\n");
ADAPTER_CHECK_END:
	dev_err(cx_chg->dev, "adapter check end \n");

}



static void cx25890h_charger_irq_workfunc(struct work_struct *work)
{
	int i, ret;
	int vbus_volt;
	bool prev_pg;
	int prev_vbus_gd = 0;
	u8 val;
	u8 status;
	u8 retry_otg = 10;	//for otg retry
	u8 chg_fault;

	struct cx25890h_device *cx_chg = container_of(work,
								struct cx25890h_device, irq_work.work);

	if (!cx25890h_wake_active(&cx_chg->cx25890h_wake_source)) {
		cx25890h_stay_awake(&cx_chg->cx25890h_wake_source);
	}

	dev_err(cx_chg->dev, "cx25890h adapter/usb irq handler\n");
	//vbus good begin
	for (i = 1; i <= 3; i++) {
		ret = cx25890h_read_byte(cx_chg, &val, CX25890H_REG_11);
		if (!ret) {
			break;
		}
		dev_err(cx_chg->dev, "read regs:0x011 fail count: %d !\n", i);
		mdelay(20);
	}
	prev_vbus_gd = cx_chg->vbus_good;
	cx_chg->vbus_good = (val & CX25890H_VBUS_GD_MASK) >> CX25890H_VBUS_GD_SHIFT;
	//vbus good end

	//power good begin
	for (i = 1; i <= 3; i++) {
		ret = cx25890h_read_byte(cx_chg, &status, CX25890H_REG_0B);
		if (!ret) {
			break;
		}
		dev_err(cx_chg->dev, "read regs:0x0b fail count: %d !\n", i);
		mdelay(5);
	}
	prev_pg = cx_chg->usb_online;
	cx_chg->usb_online = (status & CX25890H_PG_STAT_MASK) >> CX25890H_PG_STAT_SHIFT;
	//power good end

	dev_err(cx_chg->dev, "prev_vbus_gd: %d, vbus_gd: %d, prev_pg: %d, usb_online: %d, apsd_rerun: %d\n",
					prev_vbus_gd, cx_chg->vbus_good, prev_pg, cx_chg->usb_online, cx_chg->apsd_rerun);

	if ((!prev_vbus_gd) && cx_chg->vbus_good) {
		cx_chg->init_detect = true;
		if (!cx_chg->apsd_rerun) {
			cx25890h_request_dpdm(cx_chg, true);
		}
		cx25890h_set_chg_enable(cx_chg, true);
		dev_err(cx_chg->dev, "cx25890h adapter/usb inserted\n");
	} else if (prev_vbus_gd && (!cx_chg->vbus_good)) {
		cx_chg->vbus_type = CX25890H_VBUS_NONE;
		cx_chg->apsd_rerun = false;
		cx_chg->bc12_float_check = 0;
		cx25890h_remove_afc(cx_chg);
		cx25890h_request_dpdm(cx_chg, false);

		cancel_delayed_work(&cx_chg->check_adapter_work);
		cancel_delayed_work(&cx_chg->detect_work);
		cancel_delayed_work(&cx_chg->monitor_work);

		dev_err(cx_chg->dev, "cx25890h adapter/usb remove\n");
		//charger_enable_device_mode(false);//PD USB start_usb_peripheral
		cx25890h_update_wtchg_work(cx_chg);
		goto irq_exit;
	}

	if ((!prev_pg) && cx_chg->vbus_good && cx_chg->usb_online) {
		cx25890h_get_vbus_type(cx_chg);

		if (cx_chg->vbus_type == CX25890H_VBUS_USB_DCP
				|| cx_chg->vbus_type == CX25890H_VBUS_NONSTAND_1A
				|| cx_chg->vbus_type == CX25890H_VBUS_NONSTAND_1P5A) {
			schedule_delayed_work(&cx_chg->detect_work, msecs_to_jiffies(10));
			schedule_delayed_work(&cx_chg->check_adapter_work, msecs_to_jiffies(15000));
		} else if ((cx_chg->vbus_type == CX25890H_VBUS_USB_SDP
					|| cx_chg->vbus_type == CX25890H_VBUS_USB_CDP)
					&& cx_chg->apsd_rerun != 10) {
			cx25890h_request_dpdm(cx_chg, false);
			mdelay(10);
			if (cx_chg->bc12_float_check >= BC12_FLOAT_CHECK_MAX) {
				msleep(200);
				charger_enable_device_mode(true);
			}
		}
		cx25890h_update_wtchg_work(cx_chg);
		schedule_delayed_work(&cx_chg->monitor_work, msecs_to_jiffies(500));
	}


	ret = cx25890h_read_byte_without_retry(cx_chg, &chg_fault, CX25890H_REG_0C);
	if (ret) {
		dev_err(cx_chg->dev, "read regs:0x0C fail !\n");
	}
	dev_err(cx_chg->dev, "reg0c fault = %02x\n",chg_fault);
	if(chg_fault & CX25890H_FAULT_BOOST_MASK){
		do{
			dev_err(cx_chg->dev, "gudi ocp in irq !\n");
			cx25890h_enable_otg(cx_chg);
		
			msleep(2);
			ret = cx25890h_read_byte_without_retry(cx_chg, &status, CX25890H_REG_0B);
			if (ret) {
				dev_err(cx_chg->dev, "read regs:0x0B fail !\n");
			}
			val = (status&CX25890H_VBUS_STAT_MASK) >> CX25890H_VBUS_STAT_SHIFT;
		}while(retry_otg-- && val!=0x07);
	}


	cx25890h_get_vbus_volt(cx_chg, &vbus_volt);
	if (vbus_volt > 8000)
		cx25890h_set_input_volt_limit(cx_chg, 8300);
	else
		cx25890h_set_input_volt_limit(cx_chg, 4700);

irq_exit:
	if (cx25890h_wake_active(&cx_chg->cx25890h_wake_source)) {
		cx25890h_relax(&cx_chg->cx25890h_wake_source);
	}

	return;
}

static irqreturn_t cx25890h_charger_interrupt(int irq, void *data)
{
	struct cx25890h_device *cx_chg = data;

	if(!cx25890h_wake_active(&cx_chg->cx25890h_wake_source)){
		cx25890h_stay_awake(&cx_chg->cx25890h_wake_source);
	}
	//schedule_delayed_work(&cx_chg->irq_work, msecs_to_jiffies(10));
	schedule_delayed_work(&cx_chg->irq_work, 0);
	return IRQ_HANDLED;
}

static bool cx25890h_ext_iio_init(struct cx25890h_device *chip)
{
	int rc = 0;
	struct iio_channel **iio_list;

	if (IS_ERR(chip->wtchg_ext_iio_chans))
		return false;

	if (!chip->wtchg_ext_iio_chans) {
		iio_list = cx25890h_get_ext_channels(chip->dev,
			wtchg_iio_chan_name, ARRAY_SIZE(wtchg_iio_chan_name));
	 if (IS_ERR(iio_list)) {
		 rc = PTR_ERR(iio_list);
		 if (rc != -EPROBE_DEFER) {
			 dev_err(chip->dev, "Failed to get channels, rc=%d\n",
					 rc);
			 chip->wtchg_ext_iio_chans = NULL;
		 }
		 return false;
	 }
	 chip->wtchg_ext_iio_chans = iio_list;
	}

	if (!chip->afc_ext_iio_chans) {
		iio_list = cx25890h_get_ext_channels(chip->dev,
			afc_chg_iio_chan_name, ARRAY_SIZE(afc_chg_iio_chan_name));
	 if (IS_ERR(iio_list)) {
		 rc = PTR_ERR(iio_list);
		 if (rc != -EPROBE_DEFER) {
			 dev_err(chip->dev, "Failed to get afc channels, rc=%d\n",
					 rc);
			 chip->afc_ext_iio_chans = NULL;
		 }
		 return false;
	 }
	 chip->afc_ext_iio_chans = iio_list;
	}

	return true;
}


static int cx25890h_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct cx25890h_device *cx_chg = iio_priv(indio_dev);
	int ret = 0;
	*val1 = 0;
        /*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	switch (chan->channel) {
	case PSY_IIO_CHARGE_DONE:
		cx25890h_charge_done(cx_chg,val1);
		break;
	case PSY_IIO_CHARGER_STATUS:
		*val1 = cx25890h_get_chg_status(cx_chg);
		break;
	case PSY_IIO_CHARGE_TYPE:
		*val1 = cx25890h_get_usb_type(cx_chg);
		break;
	case PSY_IIO_MAIN_CHARGE_VBUS_ONLINE:
		*val1 = cx25890h_get_vbus_online(cx_chg);
		break;
	case PSY_IIO_MAIN_CHAGER_HZ:
		*val1 = cx_chg->hiz_mode;
		break;
	case PSY_IIO_SC_BUS_VOLTAGE:
		*val1 = cx25890h_get_vbus_volt(cx_chg, val1);
		if(ret<0)
			*val1 = 0;
		break;
	case PSY_IIO_CHARGE_PG_STAT:
		*val1 = cx_chg->usb_online;
		if (*val1 < 0)
			*val1 = 0;
		break;
	default:
		dev_err(cx_chg->dev, "Unsupported CX25890H IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		dev_err(cx_chg->dev, "Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, ret);
		return ret;
	}
        /*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	return IIO_VAL_INT;
}

static int cx25890h_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct cx25890h_device *cx_chg = iio_priv(indio_dev);
	int ret = 0;
        /*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	switch (chan->channel) {
	case PSY_IIO_MAIN_CHAGER_HZ:
		cx25890h_set_input_suspend(cx_chg, val1);
		break;
	case PSY_IIO_MAIN_INPUT_CURRENT_SETTLED:
		cx25890h_set_input_current_limit(cx_chg, val1);
		break;
	case PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED:
		cx25890h_set_input_volt_limit(cx_chg, val1);
		break;
	case PSY_IIO_MAIN_CHAGER_CURRENT:
		cx25890h_set_charge_current(cx_chg, val1);
		break;
	case PSY_IIO_CHARGING_ENABLED:
		cx25890h_set_chg_enable(cx_chg, val1);
		break;
	case PSY_IIO_OTG_ENABLE:
		cx25890h_set_otg(cx_chg, val1);
		break;
	case PSY_IIO_MAIN_CHAGER_TERM:
		cx25890h_set_term_current(cx_chg, val1);
		break;
	case PSY_IIO_BATTERY_VOLTAGE_TERM:
		cx25890h_set_charge_voltage(cx_chg, val1);
		break;
	case PSY_IIO_APSD_RERUN:
		if (val1 == 0 || val1 == 1 || val1 == 10)
			cx_chg->apsd_rerun = val1;
		else {
			//cx_chg->apsd_rerun = false;
			if (val1 == 2) {
				cx_chg->bc12_float_check++;
			}
			schedule_delayed_work(&cx_chg->rerun_apsd_work, msecs_to_jiffies(10));
		}
		break;
	case PSY_IIO_SET_SHIP_MODE:
		cx25890h_enter_ship_mode(cx_chg);
		break;
	case PSY_IIO_DP_DM:
		cx25890h_dp_dm(cx_chg, val1);
		break;
	case PSY_IIO_MAIN_CHARGE_FEED_WDT:
		//cx25890h_feed_watchdog_timer(cx_chg);
		dev_err(cx_chg->dev, "temp shut watchdog !!!\n");
		break;
	default:
		dev_err(cx_chg->dev, "Unsupported CX25890H IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}
        /*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	if (ret < 0)
		dev_err(cx_chg->dev, "Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, ret);

	return ret;
}

static int cx25890h_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct cx25890h_device *cx_chg = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = cx_chg->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(cx25890h_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info cx25890h_iio_info = {
	.read_raw	= cx25890h_iio_read_raw,
	.write_raw	= cx25890h_iio_write_raw,
	.of_xlate	= cx25890h_iio_of_xlate,
};

static int cx25890h_init_iio_psy(struct cx25890h_device *cx_chg)
{
	struct iio_dev *indio_dev = cx_chg->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(cx25890h_iio_psy_channels);
	int ret, i;

	cx_chg->iio_chan = devm_kcalloc(cx_chg->dev, num_iio_channels,
				sizeof(*cx_chg->iio_chan), GFP_KERNEL);
	if (!cx_chg->iio_chan)
		return -ENOMEM;

	cx_chg->int_iio_chans = devm_kcalloc(cx_chg->dev,
				num_iio_channels,
				sizeof(*cx_chg->int_iio_chans),
				GFP_KERNEL);
	if (!cx_chg->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &cx25890h_iio_info;
	indio_dev->dev.parent = cx_chg->dev;
	indio_dev->dev.of_node = cx_chg->dev->of_node;
	indio_dev->name = "cx25890h_chg";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = cx_chg->iio_chan;
	indio_dev->num_channels = num_iio_channels;

	for (i = 0; i < num_iio_channels; i++) {
		cx_chg->int_iio_chans[i].indio_dev = indio_dev;
		chan = &cx_chg->iio_chan[i];
		cx_chg->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = cx25890h_iio_psy_channels[i].channel_num;
		chan->type = cx25890h_iio_psy_channels[i].type;
		chan->datasheet_name =
			cx25890h_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			cx25890h_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			cx25890h_iio_psy_channels[i].info_mask;
	}

	ret = devm_iio_device_register(cx_chg->dev, indio_dev);
	if (ret)
		dev_err(cx_chg->dev, "Failed to register QG IIO device, rc=%d\n", ret);

	dev_err(cx_chg->dev, "CX25890H IIO device, rc=%d\n", ret);
	return ret;
}

static int cx25890h_charger_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cx25890h_device *cx_chg = i2c_get_clientdata(client);

	mutex_lock(&cx_chg->resume_complete_lock);
	cx_chg->resume_completed = true;
	mutex_unlock(&cx_chg->resume_complete_lock);
	if(cx_chg->vbus_good == true)
		schedule_delayed_work(&cx_chg->monitor_work, msecs_to_jiffies(20));
	return 0;
}

static int cx25890h_charger_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cx25890h_device *cx_chg = i2c_get_clientdata(client);

	mutex_lock(&cx_chg->resume_complete_lock);
	cx_chg->resume_completed = false;
	mutex_unlock(&cx_chg->resume_complete_lock);
	if(cx_chg->vbus_good == true)
		cancel_delayed_work(&cx_chg->monitor_work);
	return 0;
}
//+ bug 761884, tankaikun@wt, add 20220701, charger bring up
#ifdef CONFIG_MAIN_CHG_EXTCON
int cx25890h_extcon_init(struct cx25890h_device *cx_chg)
{
	int rc;

	/* extcon registration */
	cx_chg->extcon = devm_extcon_dev_allocate(cx_chg->dev, cx25890h_extcon_cable);
	if (IS_ERR(cx_chg->extcon)) {
		rc = PTR_ERR(cx_chg->extcon);
		dev_err(cx_chg->dev, "failed to allocate extcon device rc=%d\n",
				rc);
		return rc;
	}

	rc = devm_extcon_dev_register(cx_chg->dev, cx_chg->extcon);
	if (rc < 0) {
		dev_err(cx_chg->dev, "failed to register extcon device rc=%d\n",
				rc);
		return rc;
	}

	/* Support reporting polarity and speed via properties */
	rc = extcon_set_property_capability(cx_chg->extcon,
			EXTCON_USB, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(cx_chg->extcon,
			EXTCON_USB, EXTCON_PROP_USB_SS);
	rc |= extcon_set_property_capability(cx_chg->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(cx_chg->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (rc < 0)
		dev_err(cx_chg->dev,
			"failed to configure extcon capabilities\n");

	return rc;
}
#endif /* CONFIG_MAIN_CHG_EXTCON */
//- bug 761884, tankaikun@wt, add 20220701, charger bring up
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
static int cx25890h_parse_dt(struct device *dev, struct cx25890h_device *cx_chg)
{
	struct device_node *np = dev->of_node;
	int ret=0;

	cx_chg->irq_gpio = of_get_named_gpio(np, "sgm41542,intr-gpio", 0);
    if (cx_chg->irq_gpio < 0) {
		pr_err("%s get irq_gpio false\n", __func__);
	} else {
		pr_err("%s irq_gpio info %d\n", __func__, cx_chg->irq_gpio);
	}

	cx_chg->chg_en_gpio = of_get_named_gpio(np, "sgm41542,chg-en-gpio", 0);
	if (cx_chg->chg_en_gpio < 0) {
		pr_err("%s get chg_en_gpio fail !\n", __func__);
	} else {
		pr_err("%s chg_en_gpio info %d\n", __func__, cx_chg->chg_en_gpio);
	}

	cx_chg->otg_en_gpio = of_get_named_gpio(np, "sgm41542,otg-en-gpio", 0);
	if (cx_chg->otg_en_gpio < 0) {
		pr_err("%s get otg_en_gpio fail !\n", __func__);
	} else {
		pr_err("%s otg_en_gpio info %d\n", __func__, cx_chg->otg_en_gpio);
		gpio_direction_output(cx_chg->otg_en_gpio, 0);
	}

	cx_chg->cfg.disable_hvdcp = of_property_read_bool(np, "sgm41542,disable-hvdcp");
	if (cx_chg->cfg.disable_hvdcp < 0) {
		cx_chg->cfg.disable_hvdcp = 1;
	} else {
		dev_err(cx_chg->dev, "disable_hvdcp: %d\n", cx_chg->cfg.disable_hvdcp);
	}

	cx_chg->cfg.enable_auto_dpdm = of_property_read_bool(np, "sgm41542,enable-auto-dpdm");
	if (cx_chg->cfg.enable_auto_dpdm<0) {
		cx_chg->cfg.enable_auto_dpdm = 1;
	} else {
		dev_err(cx_chg->dev, "enable_auto_dpdm: %d\n", cx_chg->cfg.enable_auto_dpdm);
	}

	cx_chg->cfg.enable_term = of_property_read_bool(np, "sgm41542,enable-termination");
	if (cx_chg->cfg.enable_term < 0) {
		cx_chg->cfg.enable_term = 1;
	} else {
		dev_err(cx_chg->dev, "enable_term: %d\n", cx_chg->cfg.enable_term);
	}

	cx_chg->cfg.enable_ico = of_property_read_bool(np, "sgm41542,enable-ico");
	if (cx_chg->cfg.enable_ico < 0) {
		cx_chg->cfg.enable_ico = 1;
	} else {
		dev_err(cx_chg->dev, "enable_ico: %d\n", cx_chg->cfg.enable_ico);
	}

	cx_chg->cfg.use_absolute_vindpm = of_property_read_bool(np, "sgm41542,use-absolute-vindpm");
	if (cx_chg->cfg.use_absolute_vindpm < 0) {
		cx_chg->cfg.use_absolute_vindpm = 1;
	} else {
		dev_err(cx_chg->dev, "use_absolute_vindpm: %d\n", cx_chg->cfg.use_absolute_vindpm);
	}

	//+bug816469,tankaikun.wt,add,2023/1/30, IR compensation
	cx_chg->cfg.enable_ir_compensation = of_property_read_bool(np, "sgm41542,enable-ir-compensation");
	if (cx_chg->cfg.enable_ir_compensation < 0) {
		cx_chg->cfg.enable_ir_compensation = 0;
	} else {
		dev_err(cx_chg->dev, "enable_ir_compensation: %d\n", cx_chg->cfg.enable_ir_compensation);
	}
	//-bug816469,tankaikun.wt,add,2023/1/30, IR compensation

	ret = of_property_read_u32(np, "sgm41542,charge-voltage", &cx_chg->cfg.charge_voltage);
	if (ret < 0) {
		cx_chg->cfg.charge_voltage = 4432;
	} else {
		dev_err(cx_chg->dev, "charge_voltage: %d\n", cx_chg->cfg.charge_voltage);
	}

	ret = of_property_read_u32(np, "sgm41542,charge-current", &cx_chg->cfg.charge_current);
	if (ret < 0) {
		cx_chg->cfg.charge_current = DEFAULT_CHG_CURR;
	} else {
		dev_err(cx_chg->dev, "charge_current: %d\n", cx_chg->cfg.charge_current);
	}

	ret = of_property_read_u32(np, "sgm41542,term-current", &cx_chg->cfg.term_current);
	if (ret < 0) {
		cx_chg->cfg.term_current = DEFAULT_TERM_CURR;
	} else {
		dev_err(cx_chg->dev, "term-curren: %d\n", cx_chg->cfg.term_current);
	}

	ret = of_property_read_u32(np, "sgm41542,prechg_current", &cx_chg->cfg.prechg_current);
	if (ret < 0) {
		cx_chg->cfg.prechg_current = DEFAULT_PRECHG_CURR;
	} else {
		dev_err(cx_chg->dev, "prechg_current: %d\n", cx_chg->cfg.prechg_current);
	}

	return 0;
}
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
static int cx25890h_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int ret;
	int irqn;
	struct iio_dev *indio_dev;
	const struct of_device_id *id_t;
	struct cx25890h_device *cx_chg;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*cx_chg));
	cx_chg = iio_priv(indio_dev);
	if (!cx_chg) {
		dev_err(&client->dev, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	cx_chg->indio_dev = indio_dev;
	cx_chg->dev = &client->dev;
        client->addr = 0x6a;
	cx_chg->client = client;
	i2c_set_clientdata(client, cx_chg);
/*+P86801AA1-3487, dingmingyuan.wt, add, 2023/5/12,kernel compatibility*/
	id_t = of_match_device(of_match_ptr(cx25890h_charger_match_table), cx_chg->dev);
	if (!id_t) {
		pr_err("Couldn't find a matching device\n");
		return -ENODEV;
	}

	if (client->dev.of_node)
		cx25890h_parse_dt(&client->dev, cx_chg);

	ret = cx25890h_detect_device(cx_chg);
	if (!ret && cx_chg->part_no == CX25890H) {
		cx_chg->is_cx25890h = true;
		dev_err(cx_chg->dev, "charger:cx25890h detected, part_no: %d, revision:%d\n",
						cx_chg->part_no, cx_chg->revision);
	} else {
		dev_err(cx_chg->dev, "no cx25890h charger device found:%d\n",  ret);
		//bug 784499, liyiying@wt, add, 2022/8/5, plug usb not reaction
		return -ENODEV;
	}

	g_cx_chg = cx_chg;
	ret = cx25890h_init_iio_psy(cx_chg);
	if (ret < 0) {
		dev_err(cx_chg->dev, "Failed to initialize cx25890h iio psy: %d\n", ret);
		goto err_1;
	}

	cx25890h_ext_iio_init(cx_chg);

	cx_chg->cx25890h_wake_source.source = wakeup_source_register(NULL,
							"cx25890h_wake");
	cx_chg->cx25890h_wake_source.disabled = 1;
	cx_chg->cx25890h_apsd_wake_source.source = wakeup_source_register(NULL,
							"cx25890h_apsd_wake");
	cx_chg->cx25890h_apsd_wake_source.disabled = 1;
	cx_chg->hvdcp_det = 0;
	cx_chg->vbus_good = 0;
	cx_chg->apsd_rerun = false;
	cx_chg->init_detect = false;
	cx_chg->bc12_float_check = 0;

	ret = cx25890h_init_device(cx_chg);
	if (ret) {
		dev_err(cx_chg->dev, "device init failure: %d\n", ret);
		goto err_0;
	}

	ret = gpio_request(cx_chg->irq_gpio, "cx25890h irq pin");
	if (ret) {
		dev_err(cx_chg->dev, "%d gpio request failed\n", cx_chg->irq_gpio);
		goto err_0;
	}

	ret = gpio_request(cx_chg->chg_en_gpio, "chg_en_gpio");
	if (ret) {
		dev_err(cx_chg->dev, "%d chg_en_gpio request failed\n",
						cx_chg->chg_en_gpio);
		goto err_0;
	}

	irqn = gpio_to_irq(cx_chg->irq_gpio);
	if (irqn < 0) {
		dev_err(cx_chg->dev, "%d gpio_to_irq failed\n", irqn);
		ret = irqn;
		goto err_0;
	}
	client->irq = irqn;

	INIT_DELAYED_WORK(&cx_chg->irq_work, cx25890h_charger_irq_workfunc);
	INIT_DELAYED_WORK(&cx_chg->check_adapter_work, cx25890h_charger_check_adapter_workfunc);
	INIT_DELAYED_WORK(&cx_chg->monitor_work, cx25890h_monitor_workfunc);
	INIT_DELAYED_WORK(&cx_chg->rerun_apsd_work, cx25890h_rerun_apsd_workfunc);
	INIT_DELAYED_WORK(&cx_chg->detect_work, cx25890h_charger_detect_workfunc);

	ret = sysfs_create_group(&cx_chg->dev->kobj, &cx25890h_attr_group);
	if (ret) {
		dev_err(cx_chg->dev, "failed to register sysfs. err: %d\n", ret);
		goto err_irq;
	}

	ret = request_irq(client->irq, cx25890h_charger_interrupt,
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"cx25890h_charger1_irq", cx_chg);
	if (ret) {
		dev_err(cx_chg->dev, "Request IRQ %d failed: %d\n", client->irq, ret);
		goto err_irq;
	} else {
		dev_err(cx_chg->dev, "Request irq = %d\n", client->irq);
	}

	enable_irq_wake(irqn);

	if (cx_chg->is_cx25890h) {
		//hardwareinfo_set_prop(HARDWARE_SLAVE_CHARGER, "CX25890H_CHARGER");
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC, CHARGER_IC_NAME);
	} else {
		//hardwareinfo_set_prop(HARDWARE_SLAVE_CHARGER, "UNKNOWN");
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC, "UNKNOWN");
	}

//sdp Recognition error err,gudi.wt,20230717;
	//cx25890h_set_input_current_limit(cx_chg, 200);

	//schedule_delayed_work(&cx_chg->irq_work, msecs_to_jiffies(5000));
	//schedule_delayed_work(&cx_chg->monitor_work, msecs_to_jiffies(10000));


	//bug816469,tankaikun.wt,add,2023/1/30, IR compensation
	cx_chg->dynamic_adjust_charge_update_timer = jiffies;

	printk("cx25890h driver probe succeed !!! \n");
	return 0;

err_irq:
	cancel_delayed_work(&cx_chg->irq_work);
	cancel_delayed_work_sync(&cx_chg->monitor_work);
err_1:
err_0:
	g_cx_chg = NULL;
	return ret;
}

static void cx25890h_charger_shutdown(struct i2c_client *client)
{
	struct cx25890h_device *cx_chg = i2c_get_clientdata(client);

	dev_info(cx_chg->dev, "%s: shutdown\n", __func__);

	//+bug 816469,tankaikun.wt,add,2023/1/30, IR compensation
	cx25890h_set_chargevoltage(cx_chg, cx_chg->final_cv/1000);
	//-bug 816469,tankaikun.wt,add,2023/1/30, IR compensation

	sysfs_remove_group(&cx_chg->dev->kobj, &cx25890h_attr_group);
	cancel_delayed_work(&cx_chg->irq_work);
	cancel_delayed_work_sync(&cx_chg->monitor_work);

	//free_irq(cx_chg->client->irq, NULL);
	g_cx_chg = NULL;
}

static const struct i2c_device_id cx25890h_charger_id[] = {
	{ "CX25890H", CX25890H },
	{},
};

MODULE_DEVICE_TABLE(i2c, cx25890h_charger_id);

static const struct dev_pm_ops cx25890h_charger_pm_ops = {
	.resume		= cx25890h_charger_resume,
	.suspend	= cx25890h_charger_suspend,
};

static struct i2c_driver cx25890h_charger_driver = {
	.driver		= {
		.name	= "cx25890h",
		.of_match_table = cx25890h_charger_match_table,
		.pm = &cx25890h_charger_pm_ops,
	},
	.id_table	= cx25890h_charger_id,

	.probe		= cx25890h_charger_probe,
	.shutdown   = cx25890h_charger_shutdown,
};

module_i2c_driver(cx25890h_charger_driver);

MODULE_DESCRIPTION("SUNCORE CX25890H Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Allan.ouyang <yangpingao@wingtech.com>");


