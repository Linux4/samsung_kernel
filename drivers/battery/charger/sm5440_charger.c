/*
 * sm5440_charger.c - SM5440 Charger device driver for SAMSUNG platform
 *
 * Copyright (C) 2019 SiliconMitus Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#endif /* CONFIG_OF */
#include "../common/sec_charging_common.h"
#include "../common/sec_direct_charger.h"
#include "sm5440_charger.h"

//#define MULTIPLE_4X 1     /* if want you used WPC_DC, please contact me we need to modify about it */
#if defined(MULTIPLE_4X)
static bool hv_4x_charger;
static bool wireless_4x_charger = 1;
static int offset_4x;
#endif

#define SM5440_DC_VERSION  "UF2"

static int sm5440_read_reg(struct sm5440_charger *sm5440, u8 reg, u8 *dest)
{
	int i, cnt, ret;

	if (sm5440->wdt_disable)
		cnt = 1;
	else
		cnt = 3;

	for (i = 0; i < cnt; ++i) {
		ret = i2c_smbus_read_byte_data(sm5440->i2c, reg);
		if (ret < 0)
			dev_err(sm5440->dev, "%s: fail to i2c_read(ret=%d)\n", __func__, ret);
		else
			break;
	}

	if (ret < 0)
		return ret;
	else
		*dest = (ret & 0xff);

	return 0;
}

int sm5440_bulk_read(struct sm5440_charger *sm5440, u8 reg, int count, u8 *buf)
{
	int i, cnt, ret;

	if (sm5440->wdt_disable)
		cnt = 1;
	else
		cnt = 3;

	for (i = 0; i < cnt; ++i) {
		ret = i2c_smbus_read_i2c_block_data(sm5440->i2c, reg, count, buf);
		if (ret < 0)
			dev_err(sm5440->dev, "%s: fail to i2c_bulk_read(ret=%d)\n", __func__, ret);
		else
			break;
	}

	return ret;
}

static int sm5440_write_reg(struct sm5440_charger *sm5440, u8 reg, u8 value)
{
	int i, cnt, ret;

	if (sm5440->wdt_disable)
		cnt = 1;
	else
		cnt = 3;

	for (i = 0; i < cnt; ++i) {
		ret = i2c_smbus_write_byte_data(sm5440->i2c, reg, value);
		if (ret < 0)
			dev_err(sm5440->dev, "%s: fail to i2c_write(ret=%d)\n", __func__, ret);
		else
			break;
	}

	return ret;
}

static int sm5440_update_reg(struct sm5440_charger *sm5440, u8 reg,
		u8 val, u8 mask, u8 pos)
{
	int ret;
	u8 old_val;

	mutex_lock(&sm5440->i2c_lock);

	ret = sm5440_read_reg(sm5440, reg, &old_val);

	if (ret == 0) {
		u8 new_val;

		new_val = (val & mask) << pos | (old_val & ~(mask << pos));
		ret = sm5440_write_reg(sm5440, reg, new_val);
	}

	mutex_unlock(&sm5440->i2c_lock);

	return ret;
}

static u32 sm5440_get_vbatreg(struct sm5440_charger *sm5440)
{
	u8 reg;

	sm5440_read_reg(sm5440, SM5440_REG_VBATCNTL, &reg);

	return 3800 + (((reg & 0x3F) * 125) / 10);
}

static u32 sm5440_get_ibuslim(struct sm5440_charger *sm5440)
{
	u8 reg;

	sm5440_read_reg(sm5440, SM5440_REG_IBUSCNTL, &reg);

	return (reg & 0x7F) * 50;
}

static int sm5440_set_vbatreg(struct sm5440_charger *sm5440, u32 vbatreg)
{
	u8 reg = ((vbatreg - 3800) * 10) / 125;

	return sm5440_update_reg(sm5440, SM5440_REG_VBATCNTL, reg, 0x3F, 0);
}

static int sm5440_set_ibuslim(struct sm5440_charger *sm5440, u32 ibuslim)
{
	u8 reg = ibuslim / 50;

	return sm5440_write_reg(sm5440, SM5440_REG_IBUSCNTL, reg);
}

static int sm5440_set_freq(struct sm5440_charger *sm5440, u32 freq)
{
	u8 reg = ((freq - 250) / 50) & 0x1F;

	return sm5440_update_reg(sm5440, SM5440_REG_CNTL7, reg, 0x1F, 0);
}

static int sm5440_set_op_mode(struct sm5440_charger *sm5440, u8 op_mode)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_CNTL5, op_mode, 0x3, 2);

	return ret;
}

static u8 sm5440_get_op_mode(struct sm5440_charger *sm5440)
{
	u8 reg;

	sm5440_read_reg(sm5440, SM5440_REG_CNTL5, &reg);

	return (reg >> 2) & 0x3;
}

static struct sm_dc_info *select_sm_dc_info(struct sm5440_charger *sm5440)
{
	struct sm_dc_info *p_dc;

	if (sm5440->ps_type == SM_DC_POWER_SUPPLY_PD)
		p_dc = sm5440->pps_dc;
	else
		p_dc = sm5440->wpc_dc;

	return p_dc;
}

static int sm5440_convert_adc(struct sm5440_charger *sm5440, u8 index)
{
	u8 reg1, reg2;
	int adc;
#if !defined(CONFIG_SEC_FACTORY)
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);

	if (sm_dc_get_current_state(sm_dc) < SM_DC_CHECK_VBAT) {
		/* Didn't worked ADC block during on CHG-OFF status */
		return -1;
	}
#endif

	switch (index) {
	case SM5440_ADC_THEM:
		sm5440_read_reg(sm5440, SM5440_REG_ADC_THEM1, &reg1);
		sm5440_read_reg(sm5440, SM5440_REG_ADC_THEM2, &reg2);
		adc = ((((int)reg1 << 5) | (reg2 >> 3)) * 250) / 1000;
		break;
	case SM5440_ADC_DIETEMP:    /* unit - C x 10 */
		sm5440_read_reg(sm5440, SM5440_REG_ADC_DIETEMP, &reg1);
		adc = 225 + (u16)reg1 * 5;
		break;
	case SM5440_ADC_VBAT1:
		sm5440_read_reg(sm5440, SM5440_REG_ADC_VBAT1, &reg1);
		sm5440_read_reg(sm5440, SM5440_REG_ADC_VBAT2, &reg2);
		adc = 2048 + (((((int)reg1 << 5) | (reg2 >> 3)) * 500) / 1000);
		break;
	case SM5440_ADC_VOUT:
		sm5440_read_reg(sm5440, SM5440_REG_ADC_VOUT1, &reg1);
		sm5440_read_reg(sm5440, SM5440_REG_ADC_VOUT2, &reg2);
		adc = 2048 + (((((int)reg1 << 5) | (reg2 >> 3)) * 500) / 1000);
		break;
	case SM5440_ADC_IBUS:
		sm5440_read_reg(sm5440, SM5440_REG_ADC_IBUS1, &reg1);
		sm5440_read_reg(sm5440, SM5440_REG_ADC_IBUS2, &reg2);
		adc = ((((int)reg1 << 5) | (reg2 >> 3)) * 625) / 1000;
		break;
	case SM5440_ADC_VBUS:
		sm5440_read_reg(sm5440, SM5440_REG_ADC_VBUS1, &reg1);
		sm5440_read_reg(sm5440, SM5440_REG_ADC_VBUS2, &reg2);
		adc = 4096 + (((u16)reg1 << 5) | (reg2 >> 3));
		break;
	default:
		adc = 0;
	}

	return adc;
}

static void sm5440_print_regmap(struct sm5440_charger *sm5440)
{
	const u8 print_reg_num = SM5440_REG_DEVICEID - SM5440_REG_INT4;
	u8 regs[64] = {0x0, };
	char temp_buf[128] = {0,};
	int i;

	sm5440_bulk_read(sm5440, SM5440_REG_MSK1, 0x1F, regs);
	sm5440_bulk_read(sm5440, SM5440_REG_MSK1 + 0x1F,
			print_reg_num - 0x1F, regs + 0x1F);

	for (i = 0; i < print_reg_num; ++i) {
		sprintf(temp_buf+strlen(temp_buf), "0x%02X[0x%02X],", SM5440_REG_MSK1 + i, regs[i]);
		if (((i+1) % 10 == 0) || ((i+1) == print_reg_num)) {
			pr_info("sm5440-charger: regmap: %s\n", temp_buf);
			memset(temp_buf, 0x0, sizeof(temp_buf));
		}
	}
}

static int sm5440_enable_chg_timer(struct sm5440_charger *sm5440, bool enable)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_CNTL3, enable, 0x1, 2);

	return ret;
}

static int sm5440_set_wdt_timer(struct sm5440_charger *sm5440, u8 tmr)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_CNTL1, tmr, 0x7, 4);

	return ret;
}

static int sm5440_enable_wdt(struct sm5440_charger *sm5440, bool enable)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_CNTL1, enable, 0x1, 7);

	return ret;
}

static int sm5440_set_adc_rate(struct sm5440_charger *sm5440, u8 rate)
{
	int ret;

	ret = sm5440_update_reg(sm5440, SM5440_REG_ADCCNTL1, rate, 0x1, 1);

	return ret;
}

static int sm5440_enable_adc(struct sm5440_charger *sm5440, bool enable)
{
	int ret;


	ret = sm5440_update_reg(sm5440, SM5440_REG_ADCCNTL1, enable, 0x1, 0);

	return ret;
}

static int sm5440_sw_reset(struct sm5440_charger *sm5440)
{
	u8 i, reg;

	sm5440_write_reg(sm5440, SM5440_REG_CNTL1, 0x1);        /* Do SW RESET */

	for (i = 0; i < 0xff; ++i) {
		usleep_range(1000, 2000);
		sm5440_read_reg(sm5440, SM5440_REG_CNTL1, &reg);
		if (!(reg & 0x1))
			break;
	}

	if (i == 0xff) {
		dev_err(sm5440->dev, "%s: didn't clear reset bit\n", __func__);
		return -EBUSY;
	}

	return 0;
}

static int sm5440_reverse_boost_enable(struct sm5440_charger *sm5440, bool enable)
{
	u8 i, reg;

	if (enable) {
		sm5440_set_op_mode(sm5440, OP_MODE_REV_BYPASS);
		for (i = 0; i < 10; ++i) {
			msleep(20);
			sm5440_read_reg(sm5440, SM5440_REG_STATUS4, &reg);
			if (reg & (0x1 << 6))
				break;
		}

		if (i == 10) {
			dev_err(sm5440->dev, "%s: fail to reverse boost enable\n", __func__);
			sm5440->rev_boost = 0;
			return -EINVAL;
		}
		sm5440->rev_boost = enable;
		dev_info(sm5440->dev, "%s: ON\n", __func__);
	} else {
		sm5440_set_op_mode(sm5440, OP_MODE_CHG_OFF);
		sm5440->rev_boost = enable;
		dev_info(sm5440->dev, "%s: OFF\n", __func__);
	}

	return 0;
}

static int sm5440_set_ENHIZ(struct sm5440_charger *sm5440, bool vbus, bool chg_en)
{
	u8 enhiz = 0;   /* case : chg_on or chg_off & vbus_uvlo */

	if (vbus && !(chg_en))
		enhiz = 1;              /* case : chg_off & vbus_pok */

	dev_info(sm5440->dev, "%s: vbus=%d, chg_en=%d, enhiz=%d\n",
			__func__, vbus, chg_en, enhiz);
	sm5440_update_reg(sm5440, SM5440_REG_CNTL6, enhiz, 0x1, 7);

	return 0;
}

static bool sm5440_check_charging_enable(struct sm5440_charger *sm5440)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	u8 op_mode = sm5440_get_op_mode(sm5440);

	if (op_mode == 0x1 && sm_dc_get_current_state(sm_dc) > SM_DC_EOC)
		return true;

	return false;
}

#if defined(MULTIPLE_4X)
static void set_divide4_mode(struct sm5440_charger *sm5440, int mode)
{
	union power_supply_propval value;

	pr_info("%s: %d\n", __func__, mode);

	value.intval = 1;
	psy_do_property("da935x-usb", set, POWER_SUPPLY_PROP_PRESENT, value);

	value.intval = 0; // off
	psy_do_property("da935x-usb", set, POWER_SUPPLY_PROP_ONLINE, value);
	psy_do_property("da935x-wireless", set, POWER_SUPPLY_PROP_ONLINE, value);

	if (is_hv_wireless_type(sm5440->cable_online)) {
		value.intval = mode;
		psy_do_property("da935x-wireless", set, POWER_SUPPLY_PROP_ONLINE, value);
	} else {
		value.intval = 1;
		psy_do_property("da935x-usb", set, POWER_SUPPLY_PROP_ONLINE, value);
	}
}
#endif

#if defined(MULTIPLE_4X)
static int get_wpc_mfd_max_power(struct sm5440_charger *sm5440, struct sm_dc_power_source_info *ta)
{
	int ret, cnt;

	if (is_hv_wireless_type(sm5440->cable_online)) { //wireless
		ta->pdo_pos = 10;                   /* set '0' else return error */
		if (wireless_4x_charger) {
			ta->v_max = 18000;              /* request voltage level */
			ta->c_max = 3000;
			ta->p_max = 27000000;
		} else {
			ta->v_max = 20000;              /* request voltage level */
			ta->c_max = 1500;
			ta->p_max = 15000000;
		}
		/* apply divide v_max */
		ta->v_max = ta->v_max / 2;
		dev_info(sm5440->dev, "%s: WPC_PWR pdo_pos:%d, max_vol:%dmV, max_cur:%dmA, max_pwr:%dmW\n",
				__func__, ta->pdo_pos, ta->v_max, ta->c_max, ta->p_max);
		return 0;
	}
	ta->pdo_pos = 0;                        /* set '0' else return error */
	ta->v_max = 17000;                      /* request voltage level */
	ta->c_max = 0;
	ta->p_max = 0;
	ret = sec_pd_get_apdo_max_power(&ta->pdo_pos, &ta->v_max, &ta->c_max, &ta->p_max);
	if (ret < 0) {
		hv_4x_charger = false;
		dev_err(sm5440->dev, "%s: error:sec_pd_get_apdo_max_power 4X\n", __func__);
	} else {
		hv_4x_charger = true;
		dev_info(sm5440->dev, "%s: pdo_pos:%d, max_vol:%dmV, max_cur:%dmA, max_pwr:%dmW\n",
				__func__, ta->pdo_pos, ta->v_max, ta->c_max, ta->p_max);
	}

	return ret;
}
#endif

static int get_apdo_max_power(struct sm5440_charger *sm5440, struct sm_dc_power_source_info *ta)
{
	int ret, cnt;

	ta->pdo_pos = 0;        /* set '0' else return error */
	ta->v_max = 10000;      /* request voltage level */
	ta->c_max = 0;
	ta->p_max = 0;

	for (cnt = 0; cnt < 3; ++cnt) {
		ret = sec_pd_get_apdo_max_power(&ta->pdo_pos, &ta->v_max, &ta->c_max, &ta->p_max);
		if (ret < 0)
			dev_err(sm5440->dev, "%s: error:sec_pd_get_apdo_max_power, RETRY=%d\n", __func__, cnt);
		else
			break;
	}

	if (cnt == 3)
		dev_err(sm5440->dev, "%s: fail to get apdo_max_power(ret=%d)\n", __func__, ret);

	dev_info(sm5440->dev, "%s: pdo_pos:%d, max_vol:%dmV, max_cur:%dmA, max_pwr:%dmW\n",
			__func__, ta->pdo_pos, ta->v_max, ta->c_max, ta->p_max);

	return ret;
}

static void sm5440_init_reg_param(struct sm5440_charger *sm5440)
{
	sm5440_set_wdt_timer(sm5440, WDT_TIMER_S_30);
	sm5440_set_freq(sm5440, sm5440->pdata->freq);

	sm5440_write_reg(sm5440, SM5440_REG_CNTL2, 0xF2);               /* disable IBUSOCP,IBATOCP,THEM */
	sm5440_write_reg(sm5440, SM5440_REG_CNTL3, 0xB8);               /* disable CHGTMR */
	sm5440_write_reg(sm5440, SM5440_REG_CNTL4, 0xFF);               /* used DEB:8ms */
	sm5440_write_reg(sm5440, SM5440_REG_CNTL6, 0x09);               /* forced PWM mode, disable ENHIZ */
	sm5440_update_reg(sm5440, SM5440_REG_VBUSCNTL, 0x7, 0x7, 0);    /* VBUS_OVP_TH=11V */
	sm5440_write_reg(sm5440, SM5440_REG_VOUTCNTL, 0x3F);            /* VOUT=max */
	sm5440_write_reg(sm5440, SM5440_REG_PRTNCNTL, 0xFE);            /* OCP,OVP setting */
	sm5440_update_reg(sm5440, SM5440_REG_ADCCNTL1, 1, 0x1, 3);      /* ADC average sample = 32 */
	sm5440_write_reg(sm5440, SM5440_REG_ADCCNTL2, 0xDF);            /* ADC channel setting */
	sm5440_write_reg(sm5440, SM5440_REG_THEMCNTL1, 0x0C);           /* Thermal Regulation threshold = 120'C */
}

static int sm5440_start_charging(struct sm5440_charger *sm5440)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	struct sm_dc_power_source_info ta;
	int state = sm_dc_get_current_state(sm_dc);
	int ret;

	if (sm5440->cable_online < 1) {
		dev_err(sm5440->dev, "%s: can't detect valid cable connection(online=%d)\n",
				__func__, sm5440->cable_online);
		return -ENODEV;
	}

	if (state < SM_DC_CHECK_VBAT) {
		dev_info(sm5440->dev, "%s: charging off state (state=%d)\n", __func__, state);
		ret = sm5440_sw_reset(sm5440);
		if (ret < 0) {
			dev_err(sm5440->dev, "%s: fail to sw reset(ret=%d)\n", __func__, ret);
			return ret;
		}
		sm5440_init_reg_param(sm5440);
	}

#if defined(MULTIPLE_4X)
	ret = get_wpc_mfd_max_power(sm5440, &ta);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to get WPC_MFD(ret=%d)\n", __func__, ret);
		return ret;
	}

	/* 0 : DA935X_STATE_OFF
	 * 1 : DA935X_STATE_ONLINE_BYPASS
	 * 2 : DA935X_STATE_ONLINE_CONVERT
	 */

	set_divide4_mode(sm5440, 2);
	{
		u8 reg;

		sm5440_update_reg(sm5440, SM5440_REG_CNTL2, 0, 0x1, 7);         /* Disable Reverse Blocking */
		sm5440_read_reg(sm5440, SM5440_REG_CNTL2, &reg);
		dev_info(sm5440->dev, "%s: Update Reverse Blocking 0x%x\n", __func__, reg);
	}
#else
	ret = get_apdo_max_power(sm5440, &ta);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to get APDO(ret=%d)\n", __func__, ret);
		return ret;
	}
#endif

	sm5440_set_wdt_timer(sm5440, WDT_TIMER_S_30);
	sm5440_enable_chg_timer(sm5440, false);

	ret = sm_dc_start_charging(sm_dc, &ta);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to start direct-charging\n", __func__);
		return ret;
	}
	__pm_stay_awake(&sm5440->wake_lock);

	return 0;
}

static int sm5440_stop_charging(struct sm5440_charger *sm5440)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);

	sm_dc_stop_charging(sm_dc);

#if defined(MULTIPLE_4X)
	hv_4x_charger = false;
	sm5440_update_reg(sm5440, SM5440_REG_CNTL2, 1, 0x1, 7);         /* Enable Reverse Blocking */
#endif
	__pm_relax(&sm5440->wake_lock);

	return 0;
}

static int psy_chg_get_online(struct sm5440_charger *sm5440)
{
	u8 reg, vbus_pok, cable_online;

	cable_online = sm5440->cable_online < 1 ? 0 : 1;
	sm5440_read_reg(sm5440, SM5440_REG_STATUS3, &reg);
	dev_info(sm5440->dev, "%s: STATUS3=0x%x\n", __func__, reg);
	vbus_pok = (reg >> 5) & 0x1;

	if (vbus_pok != cable_online) {
		dev_err(sm5440->dev, "%s: mismatched vbus state(vbus_pok:%d cable_online:%d)\n",
				__func__, vbus_pok, sm5440->cable_online);
	}

	return sm5440->cable_online;
}

static int psy_chg_get_status(struct sm5440_charger *sm5440)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 st[5];

	sm5440_bulk_read(sm5440, SM5440_REG_STATUS1, 4, st);
	dev_info(sm5440->dev, "%s: status 0x%x:0x%x:0x%x:0x%x\n", __func__,
			st[0], st[1], st[2], st[3]);

	if (sm5440_check_charging_enable(sm5440)) {
		status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if ((st[2] >> 5) & 0x1) { /* check vbus-pok */
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
	}

	return status;
}

static int psy_chg_get_health(struct sm5440_charger *sm5440)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	int state = sm_dc_get_current_state(sm_dc);
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 reg, op_mode;

	dev_info(sm5440->dev, "%s: dc_state=0x%x\n", __func__, state);

	if (state == SM_DC_ERR) {
		health = POWER_SUPPLY_HEALTH_DC_ERR;
		dev_info(sm5440->dev, "%s: chg_state=%d, health=%d\n", __func__, state, health);
	} else if (state > SM_DC_PRESET) {
		op_mode = sm5440_get_op_mode(sm5440);
		sm5440_read_reg(sm5440, SM5440_REG_STATUS3, &reg);
		if (op_mode == 0x0)
			health = POWER_SUPPLY_HEALTH_DC_ERR;
		else if (((reg >> 5) & 0x1) == 0x0) /* VBUS_POK status is disabled */
			health = POWER_SUPPLY_HEALTH_DC_ERR;

		dev_info(sm5440->dev, "%s: op_mode=0x%x, status3=0x%x, health=%d\n",
			__func__, op_mode, reg, health);
	}

	return health;
}

static int psy_chg_get_chg_vol(struct sm5440_charger *sm5440)
{
	u32 chg_vol = sm5440_get_vbatreg(sm5440);

	dev_info(sm5440->dev, "%s: VBAT_REG=%dmV\n", __func__, chg_vol);

	return chg_vol;
}

static int psy_chg_get_input_curr(struct sm5440_charger *sm5440)
{
	u32 input_curr = sm5440_get_ibuslim(sm5440);

	dev_info(sm5440->dev, "%s: IBUSLIM=%dmA\n", __func__, input_curr);

	return input_curr;
}

static int psy_chg_get_ext_monitor_work(struct sm5440_charger *sm5440)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	int state = sm_dc_get_current_state(sm_dc);
	int adc_vbus, adc_ibus, adc_vout, adc_vbat, adc_them, adc_dietemp;

	if (state < SM_DC_CHECK_VBAT) {
		dev_info(sm5440->dev, "%s: charging off state (state=%d)\n", __func__, state);
		return -EINVAL;
	}

	adc_vbus = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS);
	adc_ibus = sm5440_convert_adc(sm5440, SM5440_ADC_IBUS);
	adc_vout = sm5440_convert_adc(sm5440, SM5440_ADC_VOUT);
	adc_vbat = sm5440_convert_adc(sm5440, SM5440_ADC_VBAT1);
	adc_them = sm5440_convert_adc(sm5440, SM5440_ADC_THEM);
	adc_dietemp = sm5440_convert_adc(sm5440, SM5440_ADC_DIETEMP);

	pr_info("sm5440-charger: adc_monitor: vbus:%d:ibus:%d:vout:%d:vbat:%d:them:%d:dietemp:%d\n",
			adc_vbus, adc_ibus, adc_vout, adc_vbat, adc_them, adc_dietemp);
	sm5440_print_regmap(sm5440);

	return 0;
}

static int psy_chg_get_ext_measure_input(struct sm5440_charger *sm5440, int index)
{
	int adc;

	switch (index) {
	case SEC_BATTERY_IIN_MA:
		adc = sm5440_convert_adc(sm5440, SM5440_ADC_IBUS);
		break;
	case SEC_BATTERY_IIN_UA:
		adc = sm5440_convert_adc(sm5440, SM5440_ADC_IBUS) * 1000;
		break;
	case SEC_BATTERY_VIN_MA:
		adc = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS);
		break;
	case SEC_BATTERY_VIN_UA:
		adc = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS) * 1000;
		break;
	default:
		adc = 0;
		break;
	}

	dev_info(sm5440->dev, "%s: index=%d, adc=%d\n", __func__, index, adc);

	return adc;
}

static const char *sm5440_dc_state_str[] = {
	"NO_CHARGING", "DC_ERR", "CHARGING_DONE", "CHECK_VBAT", "PRESET_DC", 
	"ADJUST_CC", "UPDATE_BAT", "CC_MODE", "CV_MODE"
};
static int psy_chg_get_ext_direct_charger_chg_status(struct sm5440_charger *sm5440,
		union power_supply_propval *val)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	int state = sm_dc_get_current_state(sm_dc);

	val->strval = sm5440_dc_state_str[state];
	pr_info("%s: CHARGER_STATUS(%s)\n", __func__, val->strval);

	return 0;
}

static int sm5440_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct sm5440_charger *sm5440 = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
		(enum power_supply_ext_property) psp;

	dev_info(sm5440->dev, "%s: psp=%d\n", __func__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = psy_chg_get_online(sm5440);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = psy_chg_get_status(sm5440);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = psy_chg_get_health(sm5440);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = sm5440_check_charging_enable(sm5440);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = psy_chg_get_chg_vol(sm5440);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = psy_chg_get_input_curr(sm5440);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX: /* get input current which was set */
		val->intval = sm5440->target_ibus;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW: /* get charge current which was set */
		val->intval = sm5440->target_ibat;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = sm5440_convert_adc(sm5440, SM5440_ADC_DIETEMP);
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			psy_chg_get_ext_monitor_work(sm5440);
#if defined(MULTIPLE_4X)
			{
				union power_supply_propval value;

				dev_err(sm5440->dev, "%s: %d\n", __func__, ext_psp);
				psy_do_property("da935x-usb", get,
						(enum power_supply_property)ext_psp, value);
			}
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			val->intval = psy_chg_get_ext_measure_input(sm5440, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_SYS:
			dev_err(sm5440->dev, "%s: need to works\n", __func__);
			/**
			 *  Need to check operation details.. by SEC.
			 */
			val->intval = -EINVAL;
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_CHG_STATUS:
			psy_chg_get_ext_direct_charger_chg_status(sm5440, val);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int psy_chg_set_online(struct sm5440_charger *sm5440, int online)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	int ret = 0;
	u8 vbus_pok = 0x0;
	u8 reg = 0x0;

	dev_info(sm5440->dev, "%s: online=%d\n", __func__, online);

	if (online < 1) {
		if (sm_dc_get_current_state(sm_dc) > SM_DC_EOC) {
			sm5440_stop_charging(sm5440);
		}
	}
	sm5440->cable_online = online;

	sm5440_read_reg(sm5440, SM5440_REG_STATUS3, &reg);
	dev_info(sm5440->dev, "%s: STATUS3=0x%x\n", __func__, reg);
	vbus_pok = (reg >> 5) & 0x1;

	sm5440->vbus_in = vbus_pok;
	sm5440_set_ENHIZ(sm5440, sm5440->vbus_in, sm5440_check_charging_enable(sm5440));

	if (sm5440->wpc_dc) {
		/* need to select power supply type for WPC_DC */
		sm5440->ps_type = SM_DC_POWER_SUPPLY_PD;
	} else {
		sm5440->ps_type = SM_DC_POWER_SUPPLY_PD;
	}

	return ret;
}

static int psy_chg_set_const_chg_voltage(struct sm5440_charger *sm5440, int vbat)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	int ret = 0;

	dev_info(sm5440->dev, "%s: [%dmV] to [%dmV]\n", __func__, sm5440->target_vbat, vbat);

	if (sm5440->target_vbat != vbat) {
		sm5440->target_vbat = vbat;
		ret = sm_dc_set_target_vbat(sm_dc, sm5440->target_vbat);
	}

	return ret;
}

static int psy_chg_set_chg_curr(struct sm5440_charger *sm5440, int ibat)
{
	int ret = 0;

	dev_info(sm5440->dev, "%s: dldn't support cc_loop\n", __func__);
	sm5440->target_ibat = ibat;

	return ret;
}

static int psy_chg_set_input_curr(struct sm5440_charger *sm5440, int ibus)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	int ret = 0;

	dev_info(sm5440->dev, "%s: ibus [%dmA] to [%dmA]\n", __func__, sm5440->target_ibus, ibus);

	if (sm5440->target_ibus != ibus) {
		sm5440->target_ibus = ibus;
		if (sm5440->target_ibus < SM5440_TA_MIN_CURRENT) {
			dev_info(sm5440->dev, "%s: can't used less then ta_min_current(%dmA)\n",
					__func__, SM5440_TA_MIN_CURRENT);
			sm5440->target_ibus = SM5440_TA_MIN_CURRENT;
		}
		ret = sm_dc_set_target_ibus(sm_dc, sm5440->target_ibus);
	}

	return ret;
}

static int psy_chg_ext_direct_float_max(struct sm5440_charger *sm5440, int max_vbat)
{
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	
	dev_info(sm5440->dev, "%s: max_vbat [%dmA] to [%dmA]\n",
			__func__, sm5440->max_vbat, max_vbat);

	sm5440->max_vbat = max_vbat;
	sm_dc->config.chg_float_voltage = max_vbat;

	return 0;

}

static int psy_chg_ext_wdt_control(struct sm5440_charger *sm5440, int wdt_control)
{
	if (wdt_control)
		sm5440->wdt_disable = 1;
	else
		sm5440->wdt_disable = 0;

	dev_info(sm5440->dev, "%s: wdt_disable=%d\n", __func__, sm5440->wdt_disable);

	return 0;
}

static int sm5440_set_adc_mode(struct i2c_client *i2c, u8 mode);

static int sm5440_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sm5440_charger *sm5440 = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp =
		(enum power_supply_ext_property) psp;

	int ret = 0;

	dev_info(sm5440->dev, "%s: psp=%d, val-intval=%d\n", __func__, psp, val->intval);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		psy_chg_set_online(sm5440, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (val->intval)
			ret = sm5440_start_charging(sm5440);
		else
			ret = sm5440_stop_charging(sm5440);
		sm5440_print_regmap(sm5440);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = psy_chg_set_const_chg_voltage(sm5440, val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = psy_chg_set_chg_curr(sm5440, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = psy_chg_set_input_curr(sm5440, val->intval);
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			ret = psy_chg_ext_wdt_control(sm5440, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_VOLTAGE_MAX:
			ret = psy_chg_set_const_chg_voltage(sm5440, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			ret = psy_chg_set_input_curr(sm5440, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_FLOAT_MAX:
			ret = psy_chg_ext_direct_float_max(sm5440, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL:
			if (val->intval)
				sm5440_set_adc_mode(sm5440->i2c, SM_DC_ADC_MODE_ONESHOT);
			else
				sm5440_set_adc_mode(sm5440->i2c, SM_DC_ADC_MODE_OFF);
			break;
#if defined(MULTIPLE_4X)
		case POWER_SUPPLY_EXT_PROP_CHARGE_BOOST:
			hv_4x_charger = (val->intval == 2) ? true : false;
			set_divide4_mode(sm5440, val->intval);
			break;
#endif
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0)
		return ret;

	return 0;
}

static char *sm5440_supplied_to[] = {
	"sm5440-charger",
};

static enum power_supply_property sm5440_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static const struct power_supply_desc sm5440_charger_power_supply_desc = {
	.name		= "sm5440-charger",
	.type		= POWER_SUPPLY_TYPE_UNKNOWN,
	.get_property	= sm5440_chg_get_property,
	.set_property	= sm5440_chg_set_property,
	.properties	= sm5440_charger_props,
	.num_properties	= ARRAY_SIZE(sm5440_charger_props),
};

static int sm5440_get_adc_value(struct i2c_client *i2c, u8 adc_ch)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);
	int adc;

	switch (adc_ch) {
	case SM_DC_ADC_THEM:
		adc = sm5440_convert_adc(sm5440, SM5440_ADC_THEM);
		break;
	case SM_DC_ADC_DIETEMP:
		adc = sm5440_convert_adc(sm5440, SM5440_ADC_DIETEMP);
		break;
	case SM_DC_ADC_VBAT:
		if (sm5440->wdt_disable)
			adc = SM5440_VBAT_MIN + 100;
		else
			adc = sm5440_convert_adc(sm5440, SM5440_ADC_VBAT1);
		break;
	case SM_DC_ADC_VOUT:
		adc = sm5440_convert_adc(sm5440, SM5440_ADC_VOUT);
		break;
	case SM_DC_ADC_VBUS:
		adc = sm5440_convert_adc(sm5440, SM5440_ADC_VBUS);
		break;
	case SM_DC_ADC_IBUS:
		adc = sm5440_convert_adc(sm5440, SM5440_ADC_IBUS);
		break;
	case SM_DC_ADC_IBAT:
	default:
		adc = 0;
		break;
	}

	return adc;
}

static int sm5440_set_adc_mode(struct i2c_client *i2c, u8 mode)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	switch (mode) {
	case SM_DC_ADC_MODE_ONESHOT:
		sm5440_enable_adc(sm5440, 0);
		msleep(20);
		sm5440_set_adc_rate(sm5440, 0x0);
		schedule_delayed_work(&sm5440->adc_work, msecs_to_jiffies(200));
		sm5440_enable_adc(sm5440, 1);
		break;
	case SM_DC_ADC_MODE_CONTINUOUS:
		cancel_delayed_work(&sm5440->adc_work);
		sm5440_enable_adc(sm5440, 0);
		msleep(50);
		sm5440_set_adc_rate(sm5440, 0x1);
		sm5440_enable_adc(sm5440, 1);
		break;
	case SM_DC_ADC_MODE_OFF:
	default:
		cancel_delayed_work(&sm5440->adc_work);
		sm5440_enable_adc(sm5440, 0);
		break;
	}

	return 0;
}

static int sm5440_get_charging_enable(struct i2c_client *i2c)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);
	u8 op_mode = sm5440_get_op_mode(sm5440);
	int ret;

	if (op_mode == OP_MODE_CHG_ON)
		ret = 0x1;
	else
		ret = 0x0;

	return ret;
}

static int sm5440_set_charging_enable(struct i2c_client *i2c, bool enable)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	sm5440_set_ENHIZ(sm5440, sm5440->vbus_in, enable);
	if (enable)
		sm5440_set_op_mode(sm5440, OP_MODE_CHG_ON);
	else
		sm5440_set_op_mode(sm5440, OP_MODE_CHG_OFF);

#if defined(MULTIPLE_4X)
	{
		union power_supply_propval val;

		psy_do_property("sec-direct-charger", get,
				POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);

		if (is_hv_wireless_type(sm5440->cable_online) && val.intval && enable) {
			pr_info("%s: HV Wireless Type, MD15 En, SM5440 En\n", __func__);
			val.intval = 0;
			psy_do_property("sec-direct-charger", set,
					POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC, val);
		}
	}
#endif

	sm5440_enable_wdt(sm5440, enable);
	sm5440_print_regmap(sm5440);

	return 0;
}

static int sm5440_dc_set_charging_config(struct i2c_client *i2c, u32 cv_gl, u32 ci_gl, u32 cc_gl)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);
	u32 vbatreg, ibuslim;

	vbatreg = cv_gl + SM5440_CV_OFFSET;
	if (ci_gl <= SM5440_TA_MIN_CURRENT)
		ibuslim = ci_gl + SM5440_CI_OFFSET * 2;
	else
		ibuslim = ci_gl + SM5440_CI_OFFSET;

	sm5440_set_ibuslim(sm5440, ibuslim);
	sm5440_set_vbatreg(sm5440, vbatreg);

	dev_info(sm5440->dev, "%s: vbat_reg=%dmV, ibus_lim=%dmA\n", __func__, vbatreg, ibuslim);

	return 0;
}

static u32 sm5440_get_dc_error_status(struct i2c_client *i2c)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);
	u8 st[5] = {0x0, };
	u8 op_mode;
	u32 err = SM_DC_ERR_NONE;

	if (!sm5440->wdt_disable) {
		sm5440_bulk_read(sm5440, SM5440_REG_STATUS1, 4, st);
		dev_info(sm5440->dev, "%s: status 0x%x:0x%x:0x%x:0x%x\n", __func__, st[0], st[1], st[2], st[3]);
		op_mode = sm5440_get_op_mode(sm5440);

		if (op_mode == 0x0) {
			sm5440_bulk_read(sm5440, SM5440_REG_INT1, 4, st);
			dev_info(sm5440->dev, "%s: int 0x%x:0x%x:0x%x:0x%x\n", __func__, st[0], st[1], st[2], st[3]);
			dev_err(sm5440->dev, "%s: disabled chg, but not detect error\n", __func__);
			err = SM_DC_ERR_UNKNOWN;
		} else if (st[2] & SM5440_INT3_VBUSUVLO) {
			dev_err(sm5440->dev, "%s: vbus uvlo detect, try to retry\n", __func__);
			err = SM_DC_ERR_RETRY;
		}
	}
	return err;
}

static int sm5440_get_dc_loop_status(struct i2c_client *i2c)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	int loop_status = LOOP_INACTIVE;
	u8 reg = 0x0;

	sm5440_read_reg(sm5440, SM5440_REG_STATUS2, &reg);
	dev_info(sm5440->dev, "%s = 0x%x\n", __func__, reg);

	if (reg & (0x1 << 3)) {         /* check VBATREG */
		loop_status = LOOP_VBATREG;
	} else if (reg & (0x1 << 7)) {  /* check IBUSLIM */
		if (reg & (0x1 << 1))     /* check THEM_REG */
			loop_status = LOOP_THEMREG;
		else
			loop_status = LOOP_IBUSLIM;
	}

	pr_info("%s loop_status=0x%x\n", sm_dc->name, loop_status);

	return loop_status;
}

static int sm5440_send_pd_msg(struct i2c_client *i2c, struct sm_dc_power_source_info *ta)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&sm5440->pd_lock);

	ret = sec_pd_select_pps(ta->pdo_pos, ta->v, ta->c);
	if (ret == -EBUSY) {
		dev_err(sm5440->dev, "%s: request again\n", __func__);
		msleep(100);
		ret = sec_pd_select_pps(ta->pdo_pos, ta->v, ta->c);
	}

	mutex_unlock(&sm5440->pd_lock);

	return ret;
}

static int sm5440_check_sw_ocp(struct i2c_client *i2c)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	schedule_delayed_work(&sm5440->ocp_check_work, msecs_to_jiffies(1000));

	return 0;
}

static const struct sm_dc_ops sm5440_dc_pps_ops = {
	.get_adc_value = sm5440_get_adc_value,
	.set_adc_mode = sm5440_set_adc_mode,
	.get_charging_enable = sm5440_get_charging_enable,
	.get_dc_error_status = sm5440_get_dc_error_status,
	.set_charging_enable = sm5440_set_charging_enable,
	.set_charging_config = sm5440_dc_set_charging_config,
	.get_dc_loop_status = sm5440_get_dc_loop_status,
	.send_power_source_msg = sm5440_send_pd_msg,
	.check_sw_ocp = sm5440_check_sw_ocp,
};

#if defined(MULTIPLE_4X)
static int sm5440_wpc_set_charging_config(struct i2c_client *i2c, u32 cv_gl, u32 ci_gl, u32 cc_gl)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);
	u32 vbatreg, ibuslim;

	vbatreg = cv_gl + SM5440_CV_OFFSET;
	ibuslim = ci_gl + SM5440_CI_OFFSET * 5;     /* temp value, need to fine turn-up */

	sm5440_set_ibuslim(sm5440, ibuslim);
	sm5440_set_vbatreg(sm5440, vbatreg);

	dev_info(sm5440->dev, "%s: vbat_reg=%dmV, ibus_lim=%dmA\n", __func__, vbatreg, ibuslim);

	return 0;
}

static int sm5440_send_wpc_msg(struct i2c_client *i2c, struct sm_dc_power_source_info *ta)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);
	int pps_v, pps_c;
	int ret;

	mutex_lock(&sm5440->pd_lock);

	if (hv_4x_charger || wireless_4x_charger) {
		pps_v = MIN(ta->v * 2 + offset_4x, ta->v_max * 2);
		pps_c = ta->c;
	} else {
		pps_v = ta->v + offset_4x;
		pps_c = ta->c;
	}
	dev_info(sm5440->dev, "%s: wpc_v=%dmV wpc_c=%dmA\n", __func__, pps_v, pps_c);

	if (is_hv_wireless_type(sm5440->cable_online)) {
		union power_supply_propval value;

		value.intval = pps_v;
		ret = psy_do_property("mfc-charger", set, POWER_SUPPLY_EXT_PROP_WIRELESS_SET_VOUT, value);
	} else {
		ret = sec_pd_select_pps(ta->pdo_pos, pps_v, pps_c);
		if (ret == -EBUSY) {
			dev_err(sm5440->dev, "%s: request again\n", __func__);
			msleep(100);
			ret = sec_pd_select_pps(ta->pdo_pos, pps_v, pps_c);
		}
	}

	mutex_unlock(&sm5440->pd_lock);

	return ret;
}

static const struct sm_dc_ops sm5440_dc_wpc_ops = {
	.get_adc_value = sm5440_get_adc_value,
	.set_adc_mode = sm5440_set_adc_mode,
	.get_charging_enable = sm5440_get_charging_enable,
	.get_dc_error_status = sm5440_get_dc_error_status,
	.set_charging_enable = sm5440_set_charging_enable,
	.set_charging_config = sm5440_wpc_set_charging_config,
	.get_dc_loop_status = sm5440_get_dc_loop_status,
	.send_power_source_msg = sm5440_send_wpc_msg,
	.check_sw_ocp = sm5440_check_sw_ocp,
};
#endif

static void sm5440_adc_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			adc_work.work);
	sm5440_enable_adc(sm5440, 1);

	schedule_delayed_work(&sm5440->adc_work, msecs_to_jiffies(200));
}

static void sm5440_ocp_check_work(struct work_struct *work)
{
	struct sm5440_charger *sm5440 = container_of(work, struct sm5440_charger,
			ocp_check_work.work);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	int i;
	u8 reg;

	for (i = 0; i < 3; ++i) {
		sm5440_read_reg(sm5440, SM5440_REG_STATUS2, &reg);
		if (reg & (0x1 << 7)) { /* activate IBUSLIM */
			dev_err(sm5440->dev, "%s: detect IBUSLIM (i=%d)\n", __func__, i);
			msleep(1000);
		} else {
			break;
		}
	}

	if (i == 3) {
		dev_err(sm5440->dev, "%s: Detected SW/IBUSOCP\n", __func__);
		sm_dc_report_error_status(sm_dc, SM_DC_ERR_IBUSOCP);
	}
}

static int sm5440_irq_vbus_uvlo(struct sm5440_charger *sm5440)
{
	if (sm5440->rev_boost) {
		/* for reverse-boost fault W/A. plz keep this code */
		sm5440_set_op_mode(sm5440, OP_MODE_CHG_OFF);
		usleep_range(10000, 11000);
		sm5440_set_op_mode(sm5440, OP_MODE_REV_BOOST);
	}

	return 0;
}

static irqreturn_t sm5440_irq_thread(int irq, void *data)
{
	struct sm5440_charger *sm5440 = (struct sm5440_charger *)data;
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	u8 reg_int[5], reg_st[5];
	u8 op_mode = sm5440_get_op_mode(sm5440);
	union power_supply_propval value;
	u32 err = 0x0;

	sm5440_bulk_read(sm5440, SM5440_REG_INT1, 4, reg_int);
	sm5440_bulk_read(sm5440, SM5440_REG_STATUS1, 4, reg_st);
	dev_info(sm5440->dev, "%s: INT[0x%x:0x%x:0x%x:0x%x] ST[0x%x:0x%x:0x%x:0x%x]\n",
			__func__, reg_int[0], reg_int[1], reg_int[2], reg_int[3],
			reg_st[0], reg_st[1], reg_st[2], reg_st[3]);

	/* check forced CUT-OFF status */
	if (sm_dc_get_current_state(sm_dc) > SM_DC_PRESET && op_mode == 0x0) {
		if (reg_int[0] & SM5440_INT1_VOUTOVP)
			err += SM_DC_ERR_VOUTOVP;

		if (reg_int[2] & SM5440_INT3_VBUSOVP)
			err += SM_DC_ERR_VBUSOVP;

		if (reg_int[2] & SM5440_INT3_STUP_FAIL)
			err += SM_DC_ERR_STUP_FAIL;

		if (reg_int[2] & SM5440_INT3_REVBLK)
			err += SM_DC_ERR_REVBLK;

		if (reg_int[2] & SM5440_INT3_CFLY_SHORT)
			err += SM_DC_ERR_CFLY_SHORT;

		dev_err(sm5440->dev, "%s: forced charge cut-off(err=0x%x)\n", __func__, err);
		sm_dc_report_error_status(sm_dc, err);
	}

	if (reg_int[1] & SM5440_INT2_VBATREG) {
		dev_info(sm5440->dev, "%s: VBATREG detected\n", __func__);
		sm_dc_report_interrupt_event(sm_dc, SM_DC_INT_VBATREG);
	}

	if (reg_int[2] & SM5440_INT3_REVBLK)
		dev_info(sm5440->dev, "%s: REVBLK detected\n", __func__);

	if (reg_int[2] & SM5440_INT3_VBUSOVP)
		dev_info(sm5440->dev, "%s: VBUS OVP detected\n", __func__);

	if (reg_int[2] & SM5440_INT3_VBUSUVLO) {
		dev_info(sm5440->dev, "%s: VBUS UVLO detected\n", __func__);
		psy_do_property("da935x-wireless", get,
				POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: DC Error Occurred. Check da9352 wireless online : %d\n",
				__func__, value.intval);
		if (value.intval == 2)
			sm5440_irq_vbus_uvlo(sm5440);
	}

	if (reg_int[2] & SM5440_INT3_VBUSPOK)
		dev_info(sm5440->dev, "%s: VBUS POK detected\n", __func__);

	if (reg_int[3] & SM5440_INT4_WDTMROFF)
		dev_info(sm5440->dev, "%s: WDTMROFF detected\n", __func__);

	dev_info(sm5440->dev, "closed %s\n", __func__);

	return IRQ_HANDLED;
}

static int sm5440_irq_init(struct sm5440_charger *sm5440)
{
	int ret;

	sm5440->irq = gpio_to_irq(sm5440->pdata->irq_gpio);
	dev_info(sm5440->dev, "%s: irq_gpio=%d, irq=%d\n", __func__,
			sm5440->pdata->irq_gpio, sm5440->irq);

	ret = gpio_request(sm5440->pdata->irq_gpio, "sm5540_irq");
	if (ret) {
		dev_err(sm5440->dev, "%s: fail to request gpio(ret=%d)\n",
				__func__, ret);
		return ret;
	}
	gpio_direction_input(sm5440->pdata->irq_gpio);
	gpio_free(sm5440->pdata->irq_gpio);

	sm5440_write_reg(sm5440, SM5440_REG_MSK1, 0xC0);
	sm5440_write_reg(sm5440, SM5440_REG_MSK2, 0xF7);    /* used VBATREG */
	sm5440_write_reg(sm5440, SM5440_REG_MSK3, 0x18);
	sm5440_write_reg(sm5440, SM5440_REG_MSK4, 0xF8);

	ret = request_threaded_irq(sm5440->irq, NULL, sm5440_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"sm5440-irq", sm5440);
	if (ret) {
		dev_err(sm5440->dev, "%s: fail to request irq(ret=%d)\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

static int sm5440_hw_init_config(struct sm5440_charger *sm5440)
{
	int ret;
	u8 reg;

	/* check to valid I2C transfer & register control */
	ret = sm5440_read_reg(sm5440, SM5440_REG_DEVICEID, &reg);
	if (ret < 0 || (reg & 0xF) != 0x1) {
		dev_err(sm5440->dev, "%s: device not found on this channel (reg=0x%x)\n",
				__func__, reg);
		return -ENODEV;
	}
	sm5440->pdata->rev_id = (reg >> 4) & 0xf;

	sm5440_init_reg_param(sm5440);

#if defined(CONFIG_BATTERY_SAMSUNG)
	psy_chg_set_const_chg_voltage(sm5440, sm5440->pdata->battery.chg_float_voltage);
#endif

	return 0;
}

#if defined(CONFIG_OF)
static int sm5440_charger_parse_dt(struct device *dev,
		struct sm5440_platform_data *pdata)
{
	struct device_node *np_sm5440 = dev->of_node;
	struct device_node *np_battery;
	int ret;

	/* Parse: sm5440 node */
	if (!np_sm5440) {
		dev_err(dev, "%s: empty of_node for sm5440_dev\n", __func__);
		return -EINVAL;
	}
	pdata->irq_gpio = of_get_named_gpio(np_sm5440, "sm5440,irq-gpio", 0);

	ret = of_property_read_u32(np_sm5440, "sm5440,freq", &pdata->freq);
	if (ret) {
		dev_info(dev, "%s: sm5440,freq is Empty\n", __func__);
		pdata->freq = 1100;
	}
	dev_info(dev, "parse_dt: irq_gpio=%d\n, freq=%d\n", pdata->irq_gpio, pdata->freq);

	/* Parse: battery node */
	np_battery = of_find_node_by_name(NULL, "battery");
	if (!np_battery) {
		dev_err(dev, "%s: empty of_node for battery\n", __func__);
		return -EINVAL;
	}
	ret = of_property_read_u32(np_battery, "battery,chg_float_voltage",
			&pdata->battery.chg_float_voltage);
	if (ret) {
		dev_info(dev, "%s: battery,chg_float_voltage is Empty\n", __func__);
		pdata->battery.chg_float_voltage = 4200;
	}
	ret = of_property_read_string(np_battery, "battery,charger_name",
			(char const **)&pdata->battery.sec_dc_name);
	if (ret) {
		dev_info(dev, "%s: battery,charger_name is Empty\n", __func__);
		pdata->battery.sec_dc_name = "sec-direct-charger";
	}

	dev_info(dev, "parse_dt: float_v=%d, sec_dc_name=%s\n",
			pdata->battery.chg_float_voltage,
			pdata->battery.sec_dc_name);

	return 0;
}
#endif  /* CONFIG_OF */

enum {
	ADDR = 0,
	SIZE,
	DATA,
	UPDATE,
	TA_MIN,
#if defined(MULTIPLE_4X)
	OFFSET_4X,
	W_4X,
#endif
};
static ssize_t chg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t chg_store_attrs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);
#define CHARGER_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0660},	\
	.show = chg_show_attrs,			\
	.store = chg_store_attrs,			\
}
static struct device_attribute charger_attrs[] = {
	CHARGER_ATTR(addr),
	CHARGER_ATTR(size),
	CHARGER_ATTR(data),
	CHARGER_ATTR(update),
	CHARGER_ATTR(ta_min_v),
#if defined(MULTIPLE_4X)
	CHARGER_ATTR(offset_4x),
	CHARGER_ATTR(w_4x),
#endif
};
static int chg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(charger_attrs); i++) {
		rc = device_create_file(dev, &charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &charger_attrs[i]);
	return rc;
}

static ssize_t chg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sm5440_charger *sm5440 = dev_get_drvdata(dev->parent);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	const ptrdiff_t offset = attr - charger_attrs;
	int i = 0;
	u8 addr;
	u8 val;

	switch (offset) {
	case ADDR:
		i += sprintf(buf, "0x%x\n", sm5440->addr);
		break;
	case SIZE:
		i += sprintf(buf, "0x%x\n", sm5440->size);
		break;
	case DATA:
		for (addr = sm5440->addr; addr < (sm5440->addr+sm5440->size); addr++) {
			sm5440_read_reg(sm5440, addr, &val);
			i += scnprintf(buf + i, PAGE_SIZE - i,
					"0x%04x : 0x%02x\n", addr, val);
		}
		break;
	case TA_MIN:
		i += sprintf(buf, "%d\n", sm_dc->config.ta_min_voltage);
		break;
#if defined(MULTIPLE_4X)
	case OFFSET_4X:
		i += sprintf(buf, "0x%x\n", offset_4x);
		break;
	case W_4X:
		i += sprintf(buf, "%d\n", wireless_4x_charger);
		break;
#endif
	default:
		return -EINVAL;
	}
	return i;
}

static ssize_t chg_store_attrs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sm5440_charger *sm5440 = dev_get_drvdata(dev->parent);
	struct sm_dc_info *sm_dc = select_sm_dc_info(sm5440);
	const ptrdiff_t offset = attr - charger_attrs;
	int ret = 0;
	int x, y, z, k;

	switch (offset) {
	case ADDR:
		if (sscanf(buf, "0x%4x\n", &x) == 1)
			sm5440->addr = x;
		ret = count;
		break;
	case SIZE:
		if (sscanf(buf, "%5d\n", &x) == 1)
			sm5440->size = x;
		ret = count;
		break;
	case DATA:
		if (sscanf(buf, "0x%8x 0x%8x", &x, &y) == 2) {
			if (x >= SM5440_REG_INT1 && x <= SM5440_REG_DEVICEID) {
				u8 addr = x;
				u8 data = y;

				if (sm5440_write_reg(sm5440, addr, data) < 0) {
					dev_info(sm5440->dev,
							"%s: addr: 0x%x write fail\n", __func__, addr);
				}
			} else {
				dev_info(sm5440->dev,
						"%s: addr: 0x%x is wrong\n", __func__, x);
			}
		}
		ret = count;
		break;
	case UPDATE:
		if (sscanf(buf, "0x%8x 0x%8x 0x%8x %d", &x, &y, &z, &k) == 4) {
			if (x >= SM5440_REG_INT1 && x <= SM5440_REG_DEVICEID) {
				u8 addr = x, data = y, val = z, pos = k;

				if (sm5440_update_reg(sm5440, addr, data, val, pos)) {
					dev_info(sm5440->dev,
							"%s: addr: 0x%x write fail\n", __func__, addr);
				}
			} else {
				dev_info(sm5440->dev,
						"%s: addr: 0x%x is wrong\n", __func__, x);
			}
		}
		ret = count;
		break;
	case TA_MIN:
		if (sscanf(buf, "%5d\n", &x) == 1)
			sm_dc->config.ta_min_voltage = x;
		ret = count;
		break;
#if defined(MULTIPLE_4X)
	case OFFSET_4X:
		if (sscanf(buf, "%5d\n", &x) == 1)
			offset_4x = x;
		ret = count;
		break;
	case W_4X:
		if (sscanf(buf, "%5d\n", &x) == 1)
			wireless_4x_charger = x;
		ret = count;
		break;
#endif
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int sm5440_dbg_read_reg(void *data, u64 *val)
{
	struct sm5440_charger *sm5440 = data;
	int ret;
	u8 reg;

	ret = sm5440_read_reg(sm5440, sm5440->debug_address, &reg);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: failed read 0x%02x\n",
				__func__, sm5440->debug_address);
		return ret;
	}
	*val = reg;

	return 0;
}

static int sm5440_dbg_write_reg(void *data, u64 val)
{
	struct sm5440_charger *sm5440 = data;
	int ret;

	ret = sm5440_write_reg(sm5440, sm5440->debug_address, (u8)val);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: failed write 0x%02x to 0x%02x\n",
				__func__, (u8)val, sm5440->debug_address);
		return ret;
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, sm5440_dbg_read_reg,
		sm5440_dbg_write_reg, "0x%02llx\n");

static int sm5440_create_debugfs_entries(struct sm5440_charger *sm5440)
{
	struct dentry *ent;

	sm5440->debug_root = debugfs_create_dir("charger-sm5440", NULL);
	if (!sm5440->debug_root) {
		dev_err(sm5440->dev, "%s: can't create dir\n", __func__);
		return -ENOENT;
	}

	ent = debugfs_create_x32("address", 0644,
			sm5440->debug_root, &(sm5440->debug_address));
	if (!ent) {
		dev_err(sm5440->dev, "%s: can't create address\n", __func__);
		return -ENOENT;
	}

	ent = debugfs_create_file("data", 0644,
			sm5440->debug_root, sm5440, &register_debug_ops);
	if (!ent) {
		dev_err(sm5440->dev, "%s: can't create data\n", __func__);
		return -ENOENT;
	}

	return 0;
}

static int sm5440_charger_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct sm5440_charger *sm5440;
	struct sm_dc_info *pps_dc, *wpc_dc;
	struct sm5440_platform_data *pdata;
	struct power_supply_config psy_cfg = {};
	int ret;

	dev_info(&i2c->dev, "%s: probe start\n", __func__);

	sm5440 = devm_kzalloc(&i2c->dev, sizeof(struct sm5440_charger), GFP_KERNEL);
	if (!sm5440)
		return -ENOMEM;

#if defined(CONFIG_OF)
	pdata = devm_kzalloc(&i2c->dev, sizeof(struct sm5440_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = sm5440_charger_parse_dt(&i2c->dev, pdata);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: fail to parse_dt\n", __func__);
		pdata = NULL;
	}
#else   /* CONFIG_OF */
	pdata = NULL;
#endif  /* CONFIG_OF */
	if (!pdata) {
		dev_err(&i2c->dev, "%s: we didn't support fixed platform_data yet\n", __func__);
		return -EINVAL;
	}

	/* create sm direct-charging instance for PD3.0(PPS) */
	pps_dc = sm_dc_create_pd_instance("SM5440-PD-DC", i2c);
	if (IS_ERR(pps_dc)) {
		dev_err(&i2c->dev, "%s: fail to create PD-DC module\n", __func__);
		return -ENOMEM;
	}
	pps_dc->config.ta_min_current = SM5440_TA_MIN_CURRENT;
	pps_dc->config.ta_min_voltage = 8200;
	pps_dc->config.dc_min_vbat = SM5440_VBAT_MIN;
	pps_dc->config.dc_vbus_ovp_th = 11000;
	pps_dc->config.pps_lr = 340000;
	pps_dc->config.rpara = 150000;
	pps_dc->config.rsns = 0;
	pps_dc->config.rpcm = 55000;
	pps_dc->config.topoff_current = 900;
	pps_dc->config.need_to_sw_ocp = 1;           /* SM5440 can't used HW_OCP, please keep it. */
	pps_dc->config.support_pd_remain = 1;        /* if pdic can't support PPS remaining, plz activate it. */
	pps_dc->config.chg_float_voltage = pdata->battery.chg_float_voltage;
	pps_dc->config.sec_dc_name = pdata->battery.sec_dc_name;
	pps_dc->ops = &sm5440_dc_pps_ops;
	ret = sm_dc_verify_configuration(pps_dc);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: fail to verify sm_dc(ret=%d)\n", __func__, ret);
		goto err_devmem;
	}
	sm5440->pps_dc = pps_dc;

#if defined(MULTIPLE_4X)
	/* create sm direct-charging instance for WPC3.0 */
	wpc_dc = sm_dc_create_wpc_instance("SM5440-WPC-DC", i2c);
	if (IS_ERR(wpc_dc)) {
		dev_err(&i2c->dev, "%s: fail to create WPC-DC module\n", __func__);
		sm_dc_destroy_instance(pps_dc);
		return -ENOMEM;
	}
	wpc_dc->config.ta_min_current = SM5440_TA_MIN_CURRENT;
	wpc_dc->config.ta_min_voltage = 8200;
	wpc_dc->config.dc_min_vbat = 3300;
	wpc_dc->config.dc_vbus_ovp_th = 11000;
	wpc_dc->config.pps_lr = 160000;
	wpc_dc->config.rpara = 60000;
	wpc_dc->config.rsns = 0;
	wpc_dc->config.rpcm = 27500;
	wpc_dc->config.topoff_current = 900;
	wpc_dc->config.need_to_sw_ocp = 1;           /* SM5440 can't used HW_OCP, please keep it. */
	wpc_dc->config.support_pd_remain = 1;        /* if pdic can't support PPS remaining, plz activate it. */
	wpc_dc->config.chg_float_voltage = pdata->battery.chg_float_voltage;
	wpc_dc->config.sec_dc_name = pdata->battery.sec_dc_name;
	wpc_dc->ops = &sm5440_dc_wpc_ops;
	ret = sm_dc_verify_configuration(wpc_dc);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s: fail to verify sm_dc(ret=%d)\n", __func__, ret);
		goto err_devmem;
	}
#else
	wpc_dc = NULL;
#endif
	sm5440->wpc_dc = wpc_dc;

	sm5440->dev = &i2c->dev;
	sm5440->i2c = i2c;
	sm5440->pdata = pdata;
	mutex_init(&sm5440->i2c_lock);
	mutex_init(&sm5440->pd_lock);
	wakeup_source_init(&sm5440->wake_lock, "sm5440-charger");
	i2c_set_clientdata(i2c, sm5440);

	ret = sm5440_hw_init_config(sm5440);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to init config(ret=%d)\n", __func__, ret);
		goto err_devmem;
	}
	INIT_DELAYED_WORK(&sm5440->adc_work, sm5440_adc_work);
	INIT_DELAYED_WORK(&sm5440->ocp_check_work, sm5440_ocp_check_work);

	psy_cfg.drv_data = sm5440;
	psy_cfg.supplied_to = sm5440_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sm5440_supplied_to),
		sm5440->psy_chg = power_supply_register(sm5440->dev,
				&sm5440_charger_power_supply_desc, &psy_cfg);
	if (IS_ERR(sm5440->psy_chg)) {
		dev_err(sm5440->dev, "%s: fail to register psy_chg\n", __func__);
		ret = PTR_ERR(sm5440->psy_chg);
		goto err_devmem;
	}

	if (sm5440->pdata->irq_gpio >= 0) {
		ret = sm5440_irq_init(sm5440);
		if (ret < 0) {
			dev_err(sm5440->dev, "%s: fail to init irq(ret=%d)\n", __func__, ret);
			goto err_psy_chg;
		}
	} else {
		dev_warn(sm5440->dev, "%s: didn't assigned irq_gpio\n", __func__);
	}

	ret = chg_create_attrs(&sm5440->psy_chg->dev);
	if (ret) {
		dev_err(sm5440->dev, "%s : Failed to create_attrs\n", __func__);
	}

	ret = sm5440_create_debugfs_entries(sm5440);
	if (ret < 0) {
		dev_err(sm5440->dev, "%s: fail to create debugfs(ret=%d)\n", __func__, ret);
		goto err_psy_chg;
	}

	dev_info(sm5440->dev, "%s: done. (rev_id=0x%x)\n", __func__, sm5440->pdata->rev_id);

	return 0;

err_psy_chg:
	power_supply_unregister(sm5440->psy_chg);

err_devmem:
	mutex_destroy(&sm5440->i2c_lock);
	mutex_destroy(&sm5440->pd_lock);
	wakeup_source_trash(&sm5440->wake_lock);
	sm_dc_destroy_instance(sm5440->pps_dc);
	sm_dc_destroy_instance(sm5440->wpc_dc);

	return ret;
}

static int sm5440_charger_remove(struct i2c_client *i2c)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	sm5440_stop_charging(sm5440);

	power_supply_unregister(sm5440->psy_chg);
	mutex_destroy(&sm5440->i2c_lock);
	mutex_destroy(&sm5440->pd_lock);
	wakeup_source_trash(&sm5440->wake_lock);
	sm_dc_destroy_instance(sm5440->pps_dc);
	sm_dc_destroy_instance(sm5440->wpc_dc);

	return 0;
}

static void sm5440_charger_shutdown(struct i2c_client *i2c)
{
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	sm5440_stop_charging(sm5440);
	sm5440_reverse_boost_enable(sm5440, 0);
}

static const struct i2c_device_id sm5440_charger_id_table[] = {
	{ "sm5440-charger", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5440_charger_id_table);

#if defined(CONFIG_OF)
static struct of_device_id sm5440_of_match_table[] = {
	{ .compatible = "siliconmitus,sm5440" },
	{ },
};
MODULE_DEVICE_TABLE(of, sm5440_of_match_table);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int sm5440_charger_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(sm5440->irq);

	disable_irq(sm5440->irq);

	return 0;
}

static int sm5440_charger_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5440_charger *sm5440 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		disable_irq_wake(sm5440->irq);

	enable_irq(sm5440->irq);

	return 0;
}
#else   /* CONFIG_PM */
#define sm5440_charger_suspend		NULL
#define sm5440_charger_resume		NULL
#endif  /* CONFIG_PM */

const struct dev_pm_ops sm5440_pm_ops = {
	.suspend = sm5440_charger_suspend,
	.resume = sm5440_charger_resume,
};

static struct i2c_driver sm5440_charger_driver = {
	.driver = {
		.name = "sm5440-charger",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = sm5440_of_match_table,
#endif  /* CONFIG_OF */
#if defined(CONFIG_PM)
		.pm = &sm5440_pm_ops,
#endif  /* CONFIG_PM */
	},
	.probe        = sm5440_charger_probe,
	.remove       = sm5440_charger_remove,
	.shutdown     = sm5440_charger_shutdown,
	.id_table     = sm5440_charger_id_table,
};

static int __init sm5440_i2c_init(void)
{
	pr_info("sm5440-charger: %s\n", __func__);
	return i2c_add_driver(&sm5440_charger_driver);
}
module_init(sm5440_i2c_init);

static void __exit sm5440_i2c_exit(void)
{
	i2c_del_driver(&sm5440_charger_driver);
}
module_exit(sm5440_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SiliconMitus <sungdae.choi@siliconmitus.com>");
MODULE_DESCRIPTION("Charger driver for SM5440");
