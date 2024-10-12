/*
 * BQ2589x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "mtk_charger.h"
#include "mtk_battery.h"

#include <linux/kobject.h>

#include <linux/hardware_info.h>
#include <linux/fb.h>
#include <linux/input/sec_cmd.h>
#include <tcpm.h>

#include "wt_sc89890.h"
#include "wt_sy6970.h"
#include "wt_sgm41543.h"
#include "wt_rt9467.h"
/*+S96616AA3-534 lijiawei,wt.battery cycle function and otg control function*/
#include <charger_class.h>
static struct charger_device *primary_charger;
/*-S96616AA3-534 lijiawei,wt.battery cycle function and otg control function*/
static struct charger_ic_t *charger_ic_list[] = {
	&sy6970,
	&sgm41543,
	&sc89890,
	&rt9467,
	NULL
};

static struct device *this_dev;
#define printc(fmt, args...) do { if(this_dev != NULL) dev_err(this_dev,"[wtchg][%s][%d] "fmt, __FUNCTION__, __LINE__, ##args); } while (0)
const char * const wt_dischg_status_txt[] = {
	"SOC LIMIT", "DEMO MODE", "SLATE MODE", "BAT OVP",
};
static bool plug_flag = false;

static const u32 SC89890_IBOOST[] = {
	500,750,1200,1400,1650,1875,2150,2450,
};
static const u32 RT9467_IBOOST[] = {
	500,700,1100,1300,1800,2100,2400,
};

static const struct charger_properties wt_chg_props = {
	.alias_name = "wt_charger",
};

static int charger_field_read(struct wtchg_info *info,enum register_bits field_id)
{
	int ret;
	s32 regval,lb,hb;
	u8 mask,temp,reg;

	if(!info->cur_field[field_id].is_exist)
		return -1;
	lb = info->cur_field[field_id].lsb;
	hb = info->cur_field[field_id].msb;
	reg = info->cur_field[field_id].reg;

	mutex_lock(&info->i2c_rw_lock);
	regval = i2c_smbus_read_byte_data(info->client,reg);
	if (regval < 0) {
		mutex_unlock(&info->i2c_rw_lock);
		printc("i2c read fail: can't read from reg 0x%02X\n", reg);
		return regval;
	}

	temp = (u8) regval;
	mask = GENMASK(hb,lb);

	temp &= mask;
	ret = temp >> lb;
	mutex_unlock(&info->i2c_rw_lock);
	return ret;
}
static int charger_field_write(struct wtchg_info *info,enum register_bits field_id, u8 val)
{
	int ret;
	u8 temp;
	u8 reg,mask;
	s32 regval,lb,hb;
	if(!info->cur_field[field_id].is_exist)
		return -1;
	lb = info->cur_field[field_id].lsb;
	hb = info->cur_field[field_id].msb;
	reg = info->cur_field[field_id].reg;

	mutex_lock(&info->i2c_rw_lock);
	regval = i2c_smbus_read_byte_data(info->client,reg);
	if (regval < 0) {
		mutex_unlock(&info->i2c_rw_lock);
		printc("i2c read fail: can't read from reg 0x%02X :%d\n", reg,regval);
		return regval;
	}
	temp = (u8) regval;
	mask = GENMASK(hb,lb);
	
	temp &= ~mask;
	temp |= (mask & (val << lb));

	ret = i2c_smbus_write_byte_data(info->client, reg, temp);
	if (ret < 0) {
		printc("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       temp, reg, ret);	
	}
	mutex_unlock(&info->i2c_rw_lock);
	return ret;
}
void wt_dischg_status_dump(struct wtchg_info *info)
{
	int i;
	for(i=0;i<8;i++){
		if(info->wt_discharging_state >> i & 0x01)
			printc("Discharging! Reason have: %s\n",wt_dischg_status_txt[i]);
	}
}

static u32 bmt_find_closest_level(const u32 *pList, u32 number, u32 level)
{
	int i;
	u32 max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = true;
	else
		max_value_in_last_element = false;

	if (max_value_in_last_element == true) {
		/* max value in the last element */
		for (i = (number - 1); i >= 0; i--) {
			if (pList[i] <= level) {
/* pr_notice("zzf_%d<=%d i=%d\n", pList[i], level, i); */
				return pList[i];
			}
		}

		printc("Can't find closest level\n");
		return pList[0];
	}

	/* max value in the first element */
	for (i = 0; i < number; i++) {
		if (pList[i] <= level)
			return pList[i];
	}

	printc("Can't find closest level\n");
	return pList[number - 1];
}
static u8 charging_parameter_to_value(const u32 *parameter,
					const u32 array_size,
					const u32 val)
{
	u32 i;

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	printc("no register value matched\n");
	return 0;
}

#define AVERAGE_SIZE 10
static int data_average_method(int data)
{
	int avgdata;//i;
	static int sum = 0,Index=0,flag=0;
	static int DataAverageBuffer[AVERAGE_SIZE]={0};
	if(!plug_flag){
		plug_flag = true;
		sum = Index = flag = 0;
		memset(DataAverageBuffer, 0, sizeof(DataAverageBuffer));
	}
	sum -= DataAverageBuffer[Index];
	sum += data;
	DataAverageBuffer[Index] = data;
	if(flag)
		avgdata = DIV_ROUND_CLOSEST(sum,AVERAGE_SIZE);
	else
		avgdata = DIV_ROUND_CLOSEST(sum,(Index+1));
	Index++;
	if(Index >= AVERAGE_SIZE){
		Index = 0;
		flag = 1;
	}
	//for(i=0;i<AVERAGE_SIZE;i++)
	//	printk("DataAverageBuffer[%d] = %d\n",i,DataAverageBuffer[i]);
	printk("%s:data=%d, avgdata=%d, Index=%d\n",__func__,data,avgdata,Index);
	return avgdata;
}
static int afc_set_voltage(struct wtchg_info *info, int chr_volt);
static int  wt_charger_get_online(struct wtchg_info *info,bool *val);

static int charger_dump_register(struct charger_device *chg_dev)
{
	return 0;	
}
static int wtchg_dump_register(struct wtchg_info *info)
{
	struct charger_ic_t *chg_driver = info->cur_chg;
	int i;
	u8 reg_val;
	
	for (i = 0; i <= chg_driver->max_register; i++) {
		reg_val = charger_field_read(info, i+B_GEG00);
		printc("[%s][REG_0x%02x] = 0x%02x\n",chg_driver->name,info->cur_field[i+B_GEG00].reg,reg_val);
	}
	return 0;
}

void charger_set_en_hiz(struct wtchg_info *info,bool en)
{
	unsigned int ret = 0;
	printc("## set hiz = %d\n",en);
	ret = charger_field_write(info, B_EN_HIZ, en);
	if (ret < 0)
		printc("set hiz failed !!!!!\n");
	else
		info->is_hiz = en;
}

int charger_set_vrechg(struct wtchg_info *info,bool en)
{
	unsigned int ret = 0;
	printc("## set vrechg = %d\n",en);
	ret = charger_field_write(info, B_VRECHG, en);
	if (ret < 0)
		printc("set vrechg failed !!!!!\n");
	return ret;
}

void charger_get_en_hiz(struct wtchg_info *info,bool *en)
{
	*en = !!charger_field_read(info, B_EN_HIZ);
}


void charger_set_chg_config(struct wtchg_info *info,bool en)
{
	unsigned int ret = 0;
	ret = charger_field_write(info, B_CHG_CFG, en);
	if (ret < 0)
		printc("set chg config failed !!!!!\n");
}
void charger_set_otg_config(struct wtchg_info *info,bool en)
{
	unsigned int ret = 0;
	if(!strcmp("rt9467", info->cur_chg->name))
		ret = charger_field_write(info, B_CHG_OTG, en);
	else
		ret = charger_field_write(info, B_OTG_CFG, en);
	if (ret < 0)
		printc("set otg config failed !!!!!\n");
}
void charger_set_wdt_rst(struct wtchg_info *info, unsigned int reg_val)
{
	unsigned int ret = 0;
	ret = charger_field_write(info, B_WD_RST, reg_val);
	if (ret)
		printc("reset wdt failed !!!\n");
}
void charger_set_watchdog(struct wtchg_info *info, unsigned int reg_val)
{
	unsigned int ret = 0;
	ret = charger_field_write(info, B_WD, reg_val);
	if (ret)
		printc("set wdt cur failed !!!\n");
}

void charger_set_shipmode(struct wtchg_info *info,bool en)
{
	unsigned int ret = 0;
	ret = charger_field_write(info, B_BATFET_DIS, en);
	if (ret < 0)
		printc("set B_BATFET_DIS failed !!!!!\n");
}
void charger_set_shipmode_delay(struct wtchg_info *info,bool en)
{
	unsigned int ret = 0;
	ret = charger_field_write(info, B_BATFET_DLY, en);
	if (ret < 0)
		printc("set B_BATFET_DLY failed !!!!!\n");
}

static int charger_parse_dt(struct device_node *np,struct wtchg_info *info)
{
	int ret;

	if (of_property_read_string(np, "charger_name", &info->chg_dev_name) < 0) {
		info->chg_dev_name = "primary_chg";
		printc("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &info->eint_name) < 0) {
		info->eint_name = "chr_stat";
		printc("no eint name\n");
	}

	ret = of_property_read_u32(np, "termination-current",&info->init_data.iterm);
	if (ret) {
		info->init_data.iterm = 150000;
		printc("Failed to read node of termination-current\n");
	}

	ret = of_property_read_u32(np, "boost-voltage",&info->init_data.boostv);
	if (ret) {
		info->init_data.boostv = 5000000;
		printc("Failed to read node of boost-voltage\n");
	}

	ret = of_property_read_u32(np, "boost-current",&info->init_data.boosti);
	if (ret) {
		info->init_data.boosti = 1200000;
		printc("Failed to read node of boost-current\n");
	}
	
	ret = of_property_read_u32(np, "afc_start_battery_soc",&info->afc_start_battery_soc);
	if (ret) {
		info->afc_start_battery_soc = 0;
		printc("Failed to read node of afc_start_battery_soc\n");
	}
	
	ret = of_property_read_u32(np, "afc_stop_battery_soc",&info->afc_stop_battery_soc);
	if (ret) {
		info->afc_stop_battery_soc = 90;
		printc("Failed to read node of afc_stop_battery_soc\n");
	}
	return 0;
}


static int charger_get_chip_state(struct wtchg_info *info,
				  struct charger_state *state)
{
	int i, ret;

	struct {
		enum register_bits id;
		u8 *data;
	} state_fields[] = {
		{B_CHG_STAT,	&state->chrg_status},
		{B_PG_STAT,	&state->online},
		{B_VSYS_STAT,	&state->vsys_status},
		{B_BOOST_FAULT, &state->boost_fault},
		{B_BAT_FAULT,	&state->bat_fault},
		{B_CHG_FAULT,	&state->chrg_fault}
	};

	for (i = 0; i < ARRAY_SIZE(state_fields); i++) {
		ret = charger_field_read(info, state_fields[i].id);
		if (ret < 0)
			return ret;

		*state_fields[i].data = ret;
	}

	printc("S:CHG/PG/VSYS=%d/%d/%d, F:CHG/BOOST/BAT=%d/%d/%d\n",
		state->chrg_status, state->online, state->vsys_status,
		state->chrg_fault, state->boost_fault, state->bat_fault);

	return 0;
}

static int charger_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);

	*en = info->charge_enabled;

	return 0;
}
#ifdef CHG_WITH_STEP_CURRENT
static int charger_set_current(struct charger_device *chg_dev,u32 cur)
{
	int ret = 0;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	cur = cur / 1000;
	if (cur <= chg_prop->ichg_min)
		cur = chg_prop->ichg_min;
	else if (cur >= chg_prop->ichg_max)
		cur = chg_prop->ichg_max;
	cancel_delayed_work_sync(&info->wtchg_set_current_work);
	info->chg_cur_val = (cur - chg_prop->ichg_offset) / chg_prop->ichg_step;	
	if(info->chg_cur_val){
		schedule_delayed_work(&info->wtchg_set_current_work,0);//wtchg_set_current_work_handler
	}else
		ret = charger_field_write(info, B_ICHG, 0);
	printc("cur=%d,reg_val=%d\n",cur,info->chg_cur_val);
	return ret;
}
#else
static int charger_set_current(struct charger_device *chg_dev,u32 cur)
{
	u8 reg_val;
	int ret;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	cur = cur / 1000;
	if (cur <= chg_prop->ichg_min)
		cur = chg_prop->ichg_min;
	else if (cur >= chg_prop->ichg_max)
		cur = chg_prop->ichg_max;
	reg_val = cur / chg_prop->ichg_step;

	ret = charger_field_write(info, B_ICHG, reg_val);
	printc("cur=%d,reg_val=%d\n",cur,reg_val);
	return ret;
}
#endif
static int charger_get_current(struct charger_device *chg_dev,u32 *ichg)
{
	unsigned int reg_val = 0;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	reg_val = charger_field_read(info, B_ICHG);
	if (reg_val < 0)
		return reg_val;

	*ichg = (reg_val * chg_prop->ichg_step + chg_prop->ichg_offset) * 1000;
	printc("cur=%d,reg_val=%d\n",*ichg,reg_val);

	return 0;
}
static int charger_get_input_limit(struct charger_device *chg_dev,u32 *limit)
{
	unsigned int reg_val = 0;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	reg_val = charger_field_read(info, B_IILIM);
	if (reg_val < 0)
		return reg_val;

	*limit = (reg_val * chg_prop->input_limit_step + chg_prop->input_limit_offset)* 1000;
	printc("limit=%d,reg_val=%d\n",*limit,reg_val);

	return 0;
}
static int charger_set_input_limit(struct charger_device *chg_dev,u32 limit_cur)
{
	u8 reg_val;
	int ret;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	
	ret = charger_field_write(info, B_EN_ILIM, 0);
	if (ret) {
		printc("disable en_ilim failed !!!\n");
	}

	if(limit_cur == 0){
		printc("limit_cur = 0, set hiz !\n");
		charger_set_en_hiz(info,1);
	}else
		charger_set_en_hiz(info,0);
	
	limit_cur = limit_cur / 1000;
	if (limit_cur >= chg_prop->input_limit_max)
		limit_cur = chg_prop->input_limit_max;
	else if (limit_cur <= chg_prop->input_limit_min)
		limit_cur = chg_prop->input_limit_min;

	chg_prop->last_limit_current = limit_cur * 1000;
	reg_val = (limit_cur - chg_prop->input_limit_offset) / chg_prop->input_limit_step;

	ret = charger_field_write(info, B_IILIM, reg_val);
	if (ret)
		printc("set limit cur failed !!!\n");

	chg_prop->actual_limit_current =
		(reg_val * chg_prop->input_limit_step) * 1000;
	printc("last limit=%d, actual limit=%d\n",chg_prop->last_limit_current,chg_prop->actual_limit_current);
	return ret;
}
static int charger_set_cv_voltage(struct charger_device *chg_dev,u32 cv)
{
	u8 reg_val,ret;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
		
	cv /= 1000;
	if (cv < chg_prop->cv_min)
		cv = chg_prop->cv_min;
	else if (cv >= chg_prop->cv_max)
		cv = chg_prop->cv_max;
	reg_val = (cv - chg_prop->cv_offset) / chg_prop->cv_step;
	//if ic is sgm41543, cv = 4394 4336(real is 4352) 4368
	ret = charger_field_write(info, B_CV, reg_val);
	if (ret)
		printc("set cv failed !!!\n");

	return 0;
}
static int charger_get_cv_voltage(struct charger_device *chg_dev, u32 *volt)
{
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	u8 reg_val = charger_field_read(info, B_CV);
	
	*volt = reg_val * chg_prop->cv_step + chg_prop->cv_offset;
	printc("get cv is %dmv !\n",*volt);

	return 0;
}


static int charger_reset_watch_dog_timer(struct charger_device *chg_dev)
{
	//struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);

	printc("charging_reset_watch_dog_timer\n");

	//charger_set_wdt_rst(info,1);	/* Kick watchdog */
	//charger_set_watchdog(info,0x3);	/* WDT 160s */

	return 0;
}
static int charger_get_vindpm_voltage(struct charger_device *chg_dev, u32 *vindpm)
{
	int ret;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);

	ret = charger_field_read(info, B_VINDPM);
	*vindpm = ret*chg_prop->vindpm_step + chg_prop->vindpm_offset;
	printc("get vindpm is %d\n",*vindpm);
	return 0;
}

static int charger_set_vindpm_voltage(struct charger_device *chg_dev,u32 vindpm)
{
	u8 reg_val;
	int ret;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	

	ret = charger_field_write(info, B_FORCE_VINDPM, 1);
	if (ret)
		printc("force en vindpm failed !!!\n");
	vindpm /= 1000;

	if (vindpm < chg_prop->vindpm_min)
		vindpm = chg_prop->vindpm_min;
	else if (vindpm > chg_prop->vindpm_max)
		vindpm = chg_prop->vindpm_max;
	reg_val = (vindpm - chg_prop->vindpm_offset) / chg_prop->vindpm_step;
	ret = charger_field_write(info, B_VINDPM, reg_val);
	if (ret)
		printc("set vindpm failed !!!\n");
	printc("vindpm = %d, regval = 0x%x\n",vindpm,reg_val);

	return 0;
}
static int charger_get_vindpm_state(struct charger_device *chg_dev, bool *in_loop)
{
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);

	*in_loop = charger_field_read(info, B_VDPM_STAT);
	return 0;
}

static int get_charging_done_status(struct charger_device *chg_dev, bool *is_done)
{
	unsigned int reg_val;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);

	reg_val = charger_field_read(info, B_CHG_STAT);

	if (reg_val == 0x3)
		*is_done = true;
	else
		*is_done = false;

	return 0;
}
static int charger_enable_safetytimer(struct charger_device *chg_dev, bool en)
{
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	int ret = charger_field_write(info, B_TMR_EN, en);
	if (ret)
		printc("enable safetytimer failed !!!\n");

	return 0;
}
static int charger_get_safetytimer_enable(struct charger_device *chg_dev, bool *en)
{
	unsigned char reg_val = 0;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	reg_val = charger_field_read(info, B_TMR_EN);
	*en = (bool)reg_val;
	
	return reg_val;
}
static int charger_otg_enabled(struct wtchg_info *info)
{
	unsigned char reg_val = 0;
	if(!strcmp("rt9467", info->cur_chg->name))
		reg_val = charger_field_read(info, B_CHG_OTG);
	else
		reg_val = charger_field_read(info, B_OTG_CFG);
	printc("otg state is %d\n",reg_val);	
	return reg_val;
}

static int charger_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);

	printc("otg en = %d\n",en);
	charger_set_en_hiz(info,0);
	if (en) {
		info->otg_enabled = true;
		charger_set_chg_config(info,0);
		charger_set_otg_config(info,1);
		charger_set_watchdog(info,0x0);
	} else {
		charger_set_otg_config(info,0);
		charger_set_chg_config(info,1);
		info->otg_enabled = false;
	}

	charger_otg_enabled(info);
	wtchg_dump_register(info);
	return ret;
}

static int charger_set_boost_current_limit(struct charger_device *chg_dev, u32 iboost)
{
	u8 register_value = 0;
	int ret;
	u32 set_iboost,array_size;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	
	iboost /= 1000;
	if (!strcmp("sc89890", info->cur_chg->name) || !strcmp("sy6970", info->cur_chg->name)){
		array_size = ARRAY_SIZE(SC89890_IBOOST);
		set_iboost = bmt_find_closest_level(SC89890_IBOOST, array_size, iboost);
		register_value = charging_parameter_to_value(SC89890_IBOOST,array_size, set_iboost);
	}else if(!strcmp("rt9467", info->cur_chg->name)){
		array_size = ARRAY_SIZE(RT9467_IBOOST);
		set_iboost = bmt_find_closest_level(RT9467_IBOOST, array_size, iboost);
		register_value = charging_parameter_to_value(RT9467_IBOOST,array_size, set_iboost);
	}
	ret = charger_field_write(info, B_BOOSTI, register_value);
	if (ret)
		printc("set boost limit cur failed !!!\n");
	else 
		printc("set boost limit cur = %d, reg_val = %d\n",iboost,register_value);
	return ret;
}
static int charger_set_boost_v(struct charger_device *chg_dev, u32 vboost)
{
	u8 reg_val;
	int ret;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	
	vboost /= 1000;
	if (vboost >= chg_prop->boost_v_max)
		vboost = chg_prop->boost_v_max;
	else if (vboost <= chg_prop->boost_v_min)
		vboost = chg_prop->boost_v_min;

	reg_val = (vboost - chg_prop->boost_v_offset) / chg_prop->boost_v_step;

	ret = charger_field_write(info, B_BOOSTV, reg_val);
	if (ret)
		printc("set boost v ur failed !!!\n");
	else 
		printc("set boost v = %d, reg_val = %d\n",vboost,reg_val);
	return ret;
}
#if 0
static void wtchg_enable_batfet_reset(struct wtchg_info *info, bool reg_val)
{
	unsigned int ret = 0;
	ret = charger_field_write(info, B_BATFET_RST_EN, reg_val);
	if (ret)
		printc("enable safetytimer failed !!!\n");
}
#endif
static int charger_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	printc("do nothing!\n");
	return ret;
}

static int charger_enable_charging(struct charger_device *chg_dev,bool en)
{
	int reg_val;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	
	if (en && info->can_charging) {
		/* enable charging */
		charger_set_en_hiz(info,0);
		charger_set_chg_config(info,en);
	} else {
		/* disenable charging */
		charger_set_chg_config(info,en);
		//charger_set_en_hiz(info,1);
	}
	
	/* still in hz mode, keep discharging */
	//if(en && info->hz_mode)
	//	charger_set_en_hiz(info, true);
	
	reg_val = charger_field_read(info, B_CHG_CFG);
	info->charge_enabled = en;
	printc("enable charging state : %d,get chg status : %d, can_charging=%d\n",en,reg_val,info->can_charging); 

	return 0;
}

static int charger_plug_in(struct charger_device *chg_dev)
{
	unsigned long flags;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return -1;
	}
	printc("in.\n");
	spin_lock_irqsave(&info->slock, flags);
	if (!info->wtchg_wakelock->active)
		__pm_stay_awake(info->wtchg_wakelock);
	spin_unlock_irqrestore(&info->slock, flags);
	info->can_charging = true;
	mtchg_info->disable_charger = false;
	charger_enable_charging(chg_dev, true);
	power_supply_changed(info->psy);
	schedule_delayed_work(&info->afc_check_work,1*HZ);
	info->charger_thread_polling = true;
	info->charger_thread_timeout = true;
	wake_up(&info->wait_que);
	//cancel_delayed_work_sync(&info->wtchg_monitor_work);
	//schedule_delayed_work(&info->wtchg_monitor_work,1*HZ);
	printc("out.\n");
	return 0;
}
static int charger_plug_out(struct charger_device *chg_dev)
{
	unsigned long flags;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	printc("in.\n");
	info->afc_enable = false;
	info->is_afc_charger = false;
	info->cc_polarity = 0;
	//__pm_relax(info->wtchg_wakelock);
	//info->wt_discharging_state = 0;
	//info->force_dis_afc = false;
	if(info->slate_mode == 2){
		info->slate_mode = 0;
		info->wt_discharging_state &= ~WT_CHARGE_SLATE_MODE_DISCHG;
	}
	if(info->batt_full_capacity != 100){
		info->wt_discharging_state &= ~WT_CHARGE_FULL_CAP_DISCHG;
	}
	plug_flag = false;
	if(!IS_ERR_OR_NULL(mtchg_info)) {
		charger_set_en_hiz(info,0);	
		charger_dev_set_charging_current(info->chg_dev,0);
	}
	spin_lock_irqsave(&info->slock, flags);
	__pm_relax(info->wtchg_wakelock);
	spin_unlock_irqrestore(&info->slock, flags);
	info->charger_thread_polling = false;
	printc("out.\n");
	return charger_enable_charging(chg_dev, false);
}
static int charger_set_iterm(struct charger_device *chg_dev, u32 cur)
{
	u8 reg_val;
	int ret;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	cur = cur / 1000;
	if (cur <= chg_prop->iterm_min)
		cur = chg_prop->iterm_min;
	else if (cur >= chg_prop->iterm_max)
		cur = chg_prop->iterm_max;
	reg_val = (cur - chg_prop->iterm_offset) / chg_prop->iterm_step;

	ret = charger_field_write(info, B_ITERM, reg_val);
	printc("iterm set cur=%d,reg_val=%d\n",cur,reg_val);
	return ret;
}
#if 0
static int charger_get_iterm(struct charger_device *chg_dev,u32 *iterm)
{
	unsigned int reg_val = 0;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	struct chg_prop *chg_prop = &(info->cur_chg->chg_prop);
	reg_val = charger_field_read(info, B_ITERM);
	if (reg_val < 0)
		return reg_val;

	*iterm = reg_val * chg_prop->iterm_step * 1000;
	printc("get iterm cur=%d,reg_val=%d\n",*iterm,reg_val);

	return 0;
}
#endif
static int charger_enable_termination(struct charger_device *chg_dev, bool en)
{
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	int ret = charger_field_write(info, B_TERM_EN, en);
	if (ret < 0)
		printc("set termination failed !!!!!\n");
	printc("enable termination = %d\n", en);

	return 0;
}
int charger_adc_read_charge_current(struct wtchg_info *info)
{
	int volt;
	int ret;
	ret = charger_field_read(info, B_ICHGR);
	if (ret < 0) {
		printc("read charge current failed :%d\n", ret);
		return ret;
	} else{
		volt = ret*50;
		return volt;
	}
}

static int charger_iterm_check(struct charger_device *chg_dev, u32 polling_ieoc)
{
	int ret = 0;
	int adc_ibat = 0;
	static int counter;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);

	adc_ibat = charger_adc_read_charge_current(info);

	printc("polling_ieoc = %d, ibat = %d\n", polling_ieoc, adc_ibat);

	if (adc_ibat <= polling_ieoc)
		counter++;
	else
		counter = 0;

	/* If IBAT is less than polling_ieoc for 3 times, trigger EOC event */
	if (counter == 3) {
		printc("polling_ieoc > ibat! notify EOC event!! \n", polling_ieoc, adc_ibat);
		charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_EOC);
		counter = 0;
	}

	return ret;
}
static int charger_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	//struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);
	printc("powerpath set en = %d\n",en);

	//charger_set_en_hiz(info,!en);
	//info->hz_mode = !en;

	return 0;
}
static int charger_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	bool val;
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);

	charger_get_en_hiz(info, &val);
	*en = !val;
	printc("get powerpath enable state = %d\n",*en);

	return 0;
}
static int charger_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct wtchg_info *info = dev_get_drvdata(&chg_dev->dev);

	if (!info->psy) {
		printc("%s: cannot get psy\n", __func__);
		return -ENODEV;
	}

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		printc("charger power_supply_changed!\n");
		power_supply_changed(info->psy);
		break;
	default:
		break;
	}
	return 0;
}

static struct charger_ops wt_charger_ops = {
	/* Normal charging */
	.plug_in = charger_plug_in,
	.plug_out = charger_plug_out,
	.dump_registers = charger_dump_register,
	.enable = charger_enable_charging,
	.is_enabled = charger_is_charging_enable,
	.get_charging_current = charger_get_current,
	.set_charging_current = charger_set_current,
	.get_input_current = charger_get_input_limit,
	.set_input_current = charger_set_input_limit,
	.get_constant_voltage = charger_get_cv_voltage,
	.set_constant_voltage = charger_set_cv_voltage,
	.kick_wdt = charger_reset_watch_dog_timer,
	.set_mivr = charger_set_vindpm_voltage,
	.get_mivr = charger_get_vindpm_voltage,
	.get_mivr_state = charger_get_vindpm_state,
	.set_eoc_current = charger_set_iterm,
	.enable_termination = charger_enable_termination,
	.safety_check = charger_iterm_check,
	//.get_min_input_current = bq2589x_get_min_aicr,
	.is_charging_done = get_charging_done_status,
	//.get_min_charging_current = bq2589x_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = charger_enable_safetytimer,
	.is_safety_timer_enabled = charger_get_safetytimer_enable,

	/* Power path */
	.enable_powerpath = charger_enable_powerpath,
	.is_powerpath_enabled = charger_is_powerpath_enabled,

	/* Charger type detection */
	.enable_chg_type_det =  charger_enable_chg_type_det,

	/* OTG */
	.enable_otg = charger_enable_otg,
	.set_boost_current_limit = charger_set_boost_current_limit,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,

	/* Event */
	.event =  charger_do_event,
};

static struct of_device_id charger_match_table[] = {
	{
	 .compatible = "wingtech,chargers",
	},
	{},
};
MODULE_DEVICE_TABLE(of, charger_match_table);

/* ======================= */
/* BQ2589X Power Supply Ops */
/* ======================= */

static int  wt_charger_get_online(struct wtchg_info *info,bool *val)
{
	union power_supply_propval online;
	if (IS_ERR_OR_NULL(info->chg_psy)) {
		info->chg_psy = devm_power_supply_get_by_phandle(info->dev,"charger");;
		printc("retry to get chg_psy\n");
		if (IS_ERR_OR_NULL(info->chg_psy)){
			online.intval = 0;
			printc("chg psy is null or error !!\n");
			return 0;
		}
	}
	power_supply_get_property(info->chg_psy,POWER_SUPPLY_PROP_ONLINE, &online);
	printc("get online = %d\n",online.intval);
	
	*val = online.intval;
	return 0;
}



static int  wt_charger_get_type(struct wtchg_info *info)
{
	union power_supply_propval prop_type;

	if (IS_ERR_OR_NULL(info->chg_psy)) {
		info->chg_psy = devm_power_supply_get_by_phandle(info->dev,"charger");
		printc("retry to get chg_psy\n");
		if (IS_ERR_OR_NULL(info->chg_psy)){
			printc("chg psy is null or error !!\n");
			return -1;
		}
	}
	power_supply_get_property(info->chg_psy,POWER_SUPPLY_PROP_TYPE, &prop_type);
	if(info->afc_enable && info->is_afc_charger)
		prop_type.intval = POWER_SUPPLY_TYPE_USB_AFC;
#if 0
	if(!IS_ERR_OR_NULL(mtchg_info)) {
		if(mtchg_info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || mtchg_info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO){
			if(get_vbus(mtchg_info) > 7500 && mtchg_info->data.max_charger_voltage != OVP_9V){
				mtchg_info->data.max_charger_voltage = OVP_9V;
				charger_dev_set_mivr(info->chg_dev, VINDPM_9V);
				charger_dev_set_input_current(info->chg_dev,mtchg_info->data.fast_charger_input_current);
				charger_dev_set_charging_current(info->chg_dev,mtchg_info->data.fast_charger_current);
				printc("## 9V first come,change max_charger_voltage to OVP_9V!\n");
			}
		}
	}
#endif
	printc("get type = %d\n",prop_type.intval);
	info->psy_desc.type = prop_type.intval;
	return 0;
}

static int  wt_charger_get_usb_type(struct wtchg_info *info)
{
	union power_supply_propval prop_type;
	if (IS_ERR_OR_NULL(info->chg_psy)) {
		info->chg_psy = devm_power_supply_get_by_phandle(info->dev,"charger");
		printc("retry to get chg_psy\n");
		if (IS_ERR_OR_NULL(info->chg_psy)){
			printc("chg psy is null or error !!\n");
			return -1;
		}
	}
	power_supply_get_property(info->chg_psy,POWER_SUPPLY_PROP_USB_TYPE, &prop_type);

	if(info->afc_enable && info->is_afc_charger)
		prop_type.intval = POWER_SUPPLY_USB_TYPE_AFC;
	printc("get usb type = %d\n",prop_type.intval);

	info->usb_type = prop_type.intval;
	return 0;
}

static int wt_charger_get_bat_state(struct wtchg_info *info)
{
	union power_supply_propval prop_type;
	if (IS_ERR_OR_NULL(info->bat_psy)) {
		info->bat_psy = power_supply_get_by_name("battery");
		printc("retry to get bat_psy\n");
		if (IS_ERR_OR_NULL(info->chg_psy)){
			printc("bat_psy is null or error !!\n");
			return -1;
		}
	}
	power_supply_get_property(info->bat_psy,POWER_SUPPLY_PROP_STATUS, &prop_type);
	printc("get chg_stat = %d\n",prop_type.intval);
	
	return prop_type.intval;
}

static int  wt_charger_set_online(struct wtchg_info *info,
				     const union power_supply_propval *val)
{
	return  charger_enable_chg_type_det(info->chg_dev, val->intval);
}

static int  wt_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct wtchg_info *info = power_supply_get_drvdata(psy);
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	int chg_stat = 0,curr_now = 0, curr_avg = 0;
	bool pwr_rdy = false;
	int ret = 0;

	//printc("prop = %d\n",psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret =  wt_charger_get_online(info, &pwr_rdy);
		val->intval = pwr_rdy;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret =  wt_charger_get_online(info, &pwr_rdy);
		chg_stat = charger_field_read(info, B_CHG_STAT);
		if (!pwr_rdy) {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			info->chg_stat = val->intval;
			return ret;
		}
		printc("chg_stat = %d\n",chg_stat);
		switch (chg_stat) {
			case 0:
				if (info->charge_enabled)
					val->intval = POWER_SUPPLY_STATUS_CHARGING;
				else
					val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
			case 1:
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
				break;
			case 2:
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
				break;
			case 3:
				val->intval = POWER_SUPPLY_STATUS_FULL;
				break;
			default:
				ret = -ENODATA;
				break;
		}
		info->chg_stat = val->intval;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		ret = wt_charger_get_type(info);
		val->intval = info->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = wt_charger_get_usb_type(info);
		val->intval = info->usb_type;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (info->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (info->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (IS_ERR_OR_NULL(mtchg_info)) {
			val->intval = 0;
			printc("Couldn't get mtchg_info\n");
			return ret;
		}
		curr_now = get_battery_current(mtchg_info);
		if(curr_now > 0){
			curr_avg = data_average_method(curr_now);
			val->intval = curr_avg;
		}else
			val->intval = 0;
		break;
	default:
		ret = -ENODATA;
	}
	return ret;
}

static int  wt_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct wtchg_info *info =
						  power_supply_get_drvdata(psy);
	int ret;

	//printc("%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret =  wt_charger_set_online(info, val);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}
static int  wt_otg_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct wtchg_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret =  charger_otg_enabled(info);
		val->intval = ret;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		ret =  charger_otg_enabled(info);
		val->intval = ret;
		break;
	default:
		ret = -ENODATA;
	}
	return ret;
}
/*+S96616AA3-534 lijiawei,wt.battery cycle function and otg control function*/
static int  wt_otg_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct wtchg_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("%s: OTG %s \n",__func__,val->intval > 0 ? "ON" : "OFF");
		primary_charger = get_charger_by_name("primary_chg");
		if (!primary_charger){
			pr_err("[%s]get primary charger device failed\n",__func__);
			return ret;
		}
		if(val->intval)
			charger_dev_enable_otg(primary_charger, val->intval);
		else
			charger_dev_enable_otg(primary_charger, val->intval);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
/*-S96616AA3-534 lijiawei,wt.battery cycle function and otg control function*/
static int  wt_otg_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		return 0;
	}
}

static int  wt_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property  wt_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_property  wt_otg_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TYPE,
};

static enum power_supply_usb_type wt_charger_usb_types[] = {
        POWER_SUPPLY_USB_TYPE_UNKNOWN,
        POWER_SUPPLY_USB_TYPE_SDP,
        POWER_SUPPLY_USB_TYPE_DCP,
        POWER_SUPPLY_USB_TYPE_CDP,
        POWER_SUPPLY_USB_TYPE_C,
        POWER_SUPPLY_USB_TYPE_PD,
        POWER_SUPPLY_USB_TYPE_PD_DRP,
        POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};


static const struct power_supply_desc  wt_charger_desc = {
	.name			= "wt_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		=  wt_charger_properties,
	.num_properties	= ARRAY_SIZE( wt_charger_properties),
	.get_property	=  wt_charger_get_property,
	.set_property	=  wt_charger_set_property,
	.property_is_writeable	=  wt_charger_property_is_writeable,
	.usb_types		=  wt_charger_usb_types,
	.num_usb_types	= ARRAY_SIZE( wt_charger_usb_types),
};
static const struct power_supply_desc  wt_otg_desc = {
	.name			= "otg",
	.type			= POWER_SUPPLY_TYPE_OTG,
	.properties		=  wt_otg_properties,
	.num_properties	= ARRAY_SIZE( wt_otg_properties),
	.get_property	=  wt_otg_get_property,
/*+S96616AA3-534 lijiawei,wt.battery cycle function and otg control function*/
	.set_property	=  wt_otg_set_property,
/*-S96616AA3-534 lijiawei,wt.battery cycle function and otg control function*/
	.property_is_writeable	=  wt_otg_property_is_writeable,
};

static char * wt_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};
static int charger_init_device(struct wtchg_info *info)
{
	int i, ret;
	struct charger_state state;

	struct {
		enum register_bits id;
		u8 data;
	} init_fields[] = {
		{B_WD,0},
		{B_IINLMTSEL,2},
		{B_EN_ILIM,0},
		{B_ADP_DIS,1},
		{B_BATFET_RST_EN,0},//disable long time press power key reboot func
	};

	for (i = 0; i < ARRAY_SIZE(init_fields); i++) {
		ret = charger_field_write(info,init_fields[i].id,init_fields[i].data);
		if (ret < 0)
			printc("write %d faile!\n",init_fields[i].id);
	}
	ret = charger_set_iterm(info->chg_dev,info->init_data.iterm);
	if (ret < 0){
		printc("init iterm failed!\n");
	}
	ret = charger_set_boost_v(info->chg_dev,info->init_data.boostv);
	if (ret < 0){
		printc("init boostv failed!\n");
	}
	ret = charger_set_boost_current_limit(info->chg_dev,info->init_data.boosti);
	if (ret < 0){
		printc("init boosti failed!\n");
	}
	ret = charger_get_chip_state(info, &state);
	if (ret < 0)
		printc("get chip state failed!\n");
	ret = charger_set_vrechg(info,1);
	if (ret < 0)
		printc("set vreg failed!\n");
	return 0;
}

static void afc_9v_to_5v(struct wtchg_info *info);
static void afc_5v_to_9v(struct wtchg_info *info);


static ssize_t afc_disable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	return sprintf(buf, "%d\n",info->force_dis_afc);
}

static ssize_t afc_disable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	bool online = false;
	if (!strncasecmp(buf, "1", 1)) {
		info->force_dis_afc = true;
	} else if (!strncasecmp(buf, "0", 1)) {
		info->force_dis_afc = false;
	} else {
		printc("%s invalid value\n", __func__);
		return size;
	}
	wt_charger_get_online(info,&online);
	if((online) && (info->is_afc_charger)){
		if ((info->force_dis_afc) && (info->afc_enable)) { 		
			afc_9v_to_5v(info);
		} else if ((!info->force_dis_afc) && (!info->afc_enable)) {
			printc("Restore afc fast charging!\n"); 
			afc_5v_to_9v(info);
		}
	}
	return size;
}


static DEVICE_ATTR(afc_disable, 0664, afc_disable_show, afc_disable_store);

int afc_charge_init(struct wtchg_info *info)
{
	struct device *dev = info->dev;
	struct device_node *np= dev->of_node;
	struct afc_dev *afc = &info->afc;
	int ret = 0;

	printc("Start\n");
	if (!afc) {
		printc("afc allocation fail, just return!\n");
		return -ENOMEM;
	}

	ret = of_get_named_gpio(np,"afc_switch_gpio", 0);
	if (ret < 0) {
		printc("could not get afc_switch_gpio\n");
		return ret;
	} else {
		afc->afc_switch_gpio = ret;
		printc("afc->afc_switch_gpio: %d\n", afc->afc_switch_gpio);
		ret = gpio_request_one(info->afc.afc_switch_gpio, GPIOF_OUT_INIT_LOW, "afc-switch");
	}

	ret = of_get_named_gpio(np,"afc_data_gpio", 0);
	if (ret < 0) {
		printc("could not get afc_data_gpio\n");
		return ret;
	} else {
		afc->afc_data_gpio = ret;
		printc("afc->afc_data_gpio: %d\n", afc->afc_data_gpio);
		ret = gpio_request_one(info->afc.afc_data_gpio, GPIOF_OUT_INIT_LOW, "afc-data");
	}
	
	ret = of_property_read_u32(np, "afc_charger_input_current",&afc->afc_charger_input_current);
	if (ret) {
		afc->afc_charger_input_current = 1670000;
		printc("Failed to read node of afc_charger_input_current\n");
	}
	ret = of_property_read_u32(np, "afc_charger_current",&afc->afc_charger_current);
	if (ret) {
		afc->afc_charger_current = 3000000;
		printc("Failed to read node of afc_charger_current\n");
	}

	afc->afc_error = 0;

	//wakeup_source_init(&afc.suspend_lock, "AFC suspend wakelock");
	spin_lock_init(&afc->afc_spin_lock);
	afc->switch_dev = sec_device_create(afc, "switch");
		if (IS_ERR(afc->switch_dev)) {
			printc("Failed to create switch_dev\n");
		}else{
			ret = device_create_file(afc->switch_dev,&dev_attr_afc_disable);
			if (ret) {
				printc("Failed to device create file, ret = %d\n", ret);
			}
		}

	printc("end \n");
	return 0;

}

void cycle(struct afc_dev *afc, int ui)
{
	gpio_set_value(afc->afc_data_gpio, 1);
	udelay(ui);
	gpio_set_value(afc->afc_data_gpio, 0);
	udelay(ui);
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
		gpio_set_value(afc->afc_data_gpio, 1);
	else
		gpio_set_value(afc->afc_data_gpio, 0);

	udelay(UI);

	return odd;
}

static void afc_send_Mping(struct afc_dev *afc)
{
	gpio_direction_output(afc->afc_data_gpio,0);
	//spin_lock_irq(&afc->afc_spin_lock);
	gpio_set_value(afc->afc_data_gpio,1);
	udelay(16*UI);
	gpio_set_value(afc->afc_data_gpio,0);
	//spin_unlock_irq(&afc->afc_spin_lock);
}
int afc_recv_Sping(struct afc_dev *afc)
{
	int i = 0, sp_cnt = 0;
    gpio_direction_input(afc->afc_data_gpio);

	while (1)
	{
		udelay(UI);
		if (gpio_get_value(afc->afc_data_gpio)) {
			break;
		}
        
		if (WAIT_SPING_COUNT < i++) {
			printc("%s: wait time out ! \n", __func__);
			goto Sping_err;
		}
	}
    
	do {
		if (SPING_MAX_UI < sp_cnt++) {
			goto Sping_err;        
        }
		udelay(UI);
	} while (gpio_get_value(afc->afc_data_gpio));
	//printc("sp_cnt = %d\n",sp_cnt);

	if (sp_cnt < SPING_MIN_UI) {
		printc("sp_cnt < %d,sping err!",SPING_MIN_UI);
		goto Sping_err;
	}
	//printc("done,sp_cnt %d \n", sp_cnt);
	return 0;

Sping_err: 
	printc("%s: Err sp_cnt %d \n", __func__, sp_cnt);
	return -1;
}
void afc_send_data(struct afc_dev *afc, int data)
{
	int i = 0;
	gpio_direction_output(afc->afc_data_gpio,0);
	//spin_lock_irq(&afc->afc_spin_lock);
	udelay(160);

	if (data & 0x80)
		cycle(afc, UI/4);
	else {
		cycle(afc, UI/4);
		gpio_set_value(afc->afc_data_gpio,1);
		udelay(UI/4);
	}

	for (i = 0x80; i > 0; i = i >> 1)
	{
		gpio_set_value(afc->afc_data_gpio, data & i);
		udelay(UI);
	}

	if (afc_send_parity_bit(afc, data)) {
		gpio_set_value(afc->afc_data_gpio, 0);
		udelay(UI/4);
		cycle(afc, UI/4);
	} else {
		udelay(UI/4);
		cycle(afc, UI/4);
	}
	//spin_unlock_irq(&afc->afc_spin_lock);
	
}
int afc_recv_data(struct afc_dev *afc)
{
	int ret = 0;
	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_2;
		return -1;
	}
	//spin_lock_irq(&afc->afc_spin_lock);
	mdelay(2);//+Bug 522575,zhaosidong.wt,ADD,20191216,must delay 2 ms
	//spin_unlock_irq(&afc->afc_spin_lock);
	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_3;
		return -1;
	}
	return 0;
}

static int afc_communication(struct afc_dev *afc)
{
	int ret = 0;

	printc("Start\n");
	gpio_set_value(afc->afc_switch_gpio, 1);
	msleep(5);
	afc_send_Mping(afc);
	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		printc("Mping recv error!\n");
		afc->afc_error = SPING_ERR_1;
		goto afc_fail;
	}
	if (afc->vol == SET_9V)
		afc_send_data(afc, 0x46);
	else
		afc_send_data(afc, 0x08);
	//udelay(160);
	afc_send_Mping(afc);
	ret = afc_recv_data(afc);
	if (ret < 0)
		goto afc_fail;
	//spin_lock_irq(&afc->afc_spin_lock);
	udelay(200);
	//spin_unlock_irq(&afc->afc_spin_lock);
	afc_send_Mping(afc);
	ret = afc_recv_Sping(afc);
	if (ret < 0) {
		afc->afc_error = SPING_ERR_4;
		goto afc_fail;
	}
	gpio_set_value(afc->afc_switch_gpio,0);
	printc("ack ok \n");
	afc->afc_error = 0;

	return 0;

afc_fail:
	gpio_set_value(afc->afc_switch_gpio,0);
	printc("AFC communication has been failed(%d)\n", afc->afc_error);

	return -1;
}


/*
* __afc_set_ta_vchr
*
* Set TA voltage, chr_volt mV
*/
static int __afc_set_ta_vchr(struct wtchg_info *info, u32 chr_volt)
{
	int i = 0, ret = 0; 
	struct afc_dev *afc = &info->afc;
        
 	afc->vol = chr_volt;
	for (i = 0; i < AFC_COMM_CNT ; i++) {      
		ret = afc_communication(afc);
		if (ret == 0) {
        		printc("ok, i %d \n",i);
        		break;
        	}
		msleep(100);
	}
	return ret;
}
static int afc_set_voltage(struct wtchg_info *info, int chr_volt)
{
	int ret = 0;
	int vchr_before, vchr_after, vchr_delta;
	const u32 retry_cnt_max = 8;
	u32 retry_cnt = 0;
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return -1;
	}

	printc("set volt = %d,Start\n",chr_volt);
	vchr_before = get_vbus(mtchg_info);
	do {
		ret = __afc_set_ta_vchr(info, chr_volt);
		mdelay(200);
		vchr_after = get_vbus(mtchg_info);
		vchr_delta = abs(vchr_after - chr_volt);
		printc("vchr_before=%d,vchr_after=%d,vchr_delta=%d\n",vchr_before,vchr_after,vchr_delta);
		/*
		 * It is successful if VBUS difference to target is
		 * less than 1000mV.
		 */	
		if (vchr_delta < 1000) {
			printc("afc set %d  seccessfully!!!\n",chr_volt);			
			return 0;
		}
		retry_cnt++;

		printc("retry cnt(%d)\n",retry_cnt);
	} while ( info->usb_type != POWER_SUPPLY_USB_TYPE_UNKNOWN &&
		 retry_cnt < retry_cnt_max);
	if(info->usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN || !info->is_afc_charger || retry_cnt >= retry_cnt_max)
		ret = -1;
	
	return ret;
}

static int afc_test_set_voltage(struct wtchg_info *info, int chr_volt)
{
	int ret = 0;
	const u32 retry_cnt_max = 15;
	u32 retry_cnt = 0;
	static int try_work_cnt_max = 2;
	bool online;
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return -1;
	}
	printc("set volt = %d\n",chr_volt);
	wt_charger_get_online(info,&online);
	if(!online)
		return -1;
	do {
		wt_charger_get_online(info,&online);
		if(!online)
			return -1;
		ret = __afc_set_ta_vchr(info, chr_volt);
		
		if(!info->afc.afc_error){
			printc("afc match seccessfully!!!\n");
			info->is_afc_charger = true;
			try_work_cnt_max = 2;
			if((get_uisoc(mtchg_info) >= info->afc_start_battery_soc) 
				&& (get_uisoc(mtchg_info) <= info->afc_stop_battery_soc)
				&& (!info->force_dis_afc))
				schedule_delayed_work(&info->afc_5v_to_9v_work,0);
			else{
				power_supply_changed(info->psy);
				printc("uisoc out of range! not enter afc charging!!!\n");
			}
			return 0;
		}else{
			printc("afc match failed!!\n");
			info->afc_enable = false;
		}
		retry_cnt++;

		printc("retry cnt(%d)\n",retry_cnt);
	} while ( info->usb_type != POWER_SUPPLY_USB_TYPE_UNKNOWN &&
		 retry_cnt < retry_cnt_max);
	if(info->usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN || !info->is_afc_charger || retry_cnt >= retry_cnt_max)
		ret = -1;

	if(try_work_cnt_max > 0){
		printc("afc match failed!!!trycnt=%d\n",try_work_cnt_max);
		try_work_cnt_max--;
		schedule_delayed_work(&info->afc_check_work,1*HZ);
		return ret;
	}
	try_work_cnt_max = 2;
	return ret;
}

#if 0
static int afc_check_psy_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct wtchg_info *info = container_of(nb,
					struct wtchg_info, psy_nb);
	union power_supply_propval pval;
	union power_supply_propval ival;
	union power_supply_propval tval;
	int ret;

	if (IS_ERR_OR_NULL(info->chg_psy)) {
		info->chg_psy = devm_power_supply_get_by_phandle(info->dev,"charger");;
		printc("retry to get chg_psy\n");
		if (IS_ERR_OR_NULL(info->chg_psy)){
			printc("chg psy is null or error !!\n");
			return NOTIFY_DONE;
		}
	}

	if (event != PSY_EVENT_PROP_CHANGED || psy != info->chg_psy){
		printc("warning: do nothing ,notifier done!\n");
		return NOTIFY_DONE;
	}

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		printc("failed to get online prop\n");
		return NOTIFY_DONE;
	}

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_AUTHENTIC, &ival);
	if (ret < 0) {
		printc("failed to get authentic prop\n");
		ival.intval = 0;
	}

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_USB_TYPE, &tval);
	if (ret < 0) {
		printc("failed to get usb type\n");
		return NOTIFY_DONE;
	}
	info->usb_type = tval.intval;

	printc("online=%d, ignore_usb=%d, type=%d\n",
				pval.intval, ival.intval, tval.intval);

	if (ival.intval)
		return NOTIFY_DONE;

	if (pval.intval && (tval.intval == POWER_SUPPLY_USB_TYPE_DCP))
	{
		printc("is not PD fschg,then try check afc fschg.\n");
		schedule_delayed_work(&info->afc_check_work,1*HZ);
	}
	return NOTIFY_DONE;
}
#endif
static void afc_check_work_handler(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct wtchg_info *info = container_of(dwork, struct wtchg_info, afc_check_work);

	if(info->psy_desc.type == POWER_SUPPLY_TYPE_USB_DCP && 
		info->pd_type != MTK_PD_CONNECT_PE_READY_SNK &&
		info->pd_type != MTK_PD_CONNECT_PE_READY_SNK_PD30 &&
		info->pd_type != MTK_PD_CONNECT_PE_READY_SNK_APDO &&
		!info->wt_discharging_state){
		printc("afc test valtage start\n");
		afc_test_set_voltage(info,SET_5V);
	}
}
static void afc_5v_to_9v_work_handler(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct wtchg_info *info = container_of(dwork, struct wtchg_info, afc_5v_to_9v_work);
	afc_5v_to_9v(info);
}
static void afc_9v_to_5v_work_handler(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct wtchg_info *info = container_of(dwork, struct wtchg_info, afc_9v_to_5v_work);
	afc_9v_to_5v(info);
}


static void afc_5v_to_9v(struct wtchg_info *info)
{	
	bool online = false;
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	int ret = 0, before_input_cur = 0, before_mivr = 0;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return;
	}
	wt_charger_get_online(info,&online);
	if(!online){
		printc("not noline!\n");
		return;
	}
	charger_dev_get_input_current(info->chg_dev,&before_input_cur);
	charger_dev_get_mivr(info->chg_dev,&before_mivr);
	
	printc("afc set valtage start,input cur=%d,chg cur=%d,before_input_cur = %d, before_mivr = %d\n",
		mtchg_info->data.fast_charger_input_current,mtchg_info->data.fast_charger_current,before_input_cur,before_mivr);
	
	charger_dev_set_mivr(info->chg_dev, VINDPM_9V);
	
	if(before_input_cur > mtchg_info->data.fast_charger_input_current){
		charger_dev_set_input_current(info->chg_dev,mtchg_info->data.fast_charger_input_current);
		ret = afc_set_voltage(info,SET_9V);
		if(!ret){
			info->afc_enable = true;
			mtchg_info->data.max_charger_voltage = OVP_9V;
			charger_dev_set_charging_current(info->chg_dev,mtchg_info->data.fast_charger_current);
			power_supply_changed(info->psy);
			printc("afc enable successfully!\n");
		}else{
			printc("afc disable failed! before_input_cur:%d,before_mivr:%d\n",before_input_cur,before_mivr);
			charger_dev_set_input_current(info->chg_dev,before_input_cur);
			charger_dev_set_mivr(info->chg_dev, before_mivr);
		}
	}else{
		ret = afc_set_voltage(info,SET_9V);
		if(!ret){
			info->afc_enable = true;
			mtchg_info->data.max_charger_voltage = OVP_9V;
			charger_dev_set_input_current(info->chg_dev,mtchg_info->data.fast_charger_input_current);
			charger_dev_set_charging_current(info->chg_dev,mtchg_info->data.fast_charger_current);
			power_supply_changed(info->psy);
			printc("afc enable successfully!\n");
		}else{
			printc("afc disable failed! before_mivr:%d\n",before_mivr);
			charger_dev_set_mivr(info->chg_dev, before_mivr);
		}
	}
}
static void afc_9v_to_5v(struct wtchg_info *info)
{
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	int before_input_cur = 0, before_mivr = 0, ret = 0;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return;
	}
	charger_dev_get_input_current(info->chg_dev,&before_input_cur);
	charger_dev_get_mivr(info->chg_dev,&before_mivr);
	
	printc("afc set valtage start,input cur=%d,chg cur=%d,before_input_cur = %d, before_mivr = %d\n",
		mtchg_info->data.fast_charger_input_current,mtchg_info->data.fast_charger_current,before_input_cur,before_mivr);
	
	if(before_input_cur > mtchg_info->data.ac_charger_input_current){
		charger_dev_set_input_current(info->chg_dev,mtchg_info->data.ac_charger_input_current);
		ret = afc_set_voltage(info,SET_5V);
		if(!ret){
			info->afc_enable = false;
			mtchg_info->data.max_charger_voltage = OVP_5V;
			charger_dev_set_mivr(info->chg_dev, VINDPM_5V);
			charger_dev_set_charging_current(info->chg_dev,mtchg_info->data.ac_charger_current);
			power_supply_changed(info->psy);
			printc("afc disable successfully!\n");
		}else{
			printc("afc disable failed! %d %d\n",before_input_cur,before_mivr);
			charger_dev_set_input_current(info->chg_dev,before_input_cur);
		}
	}else{
		ret = afc_set_voltage(info,SET_5V);
		if(!ret){
			info->afc_enable = false;
			mtchg_info->data.max_charger_voltage = OVP_5V;
			charger_dev_set_mivr(info->chg_dev, VINDPM_5V);
			charger_dev_set_input_current(info->chg_dev,mtchg_info->data.ac_charger_input_current);
			charger_dev_set_charging_current(info->chg_dev,mtchg_info->data.ac_charger_current);
			power_supply_changed(info->psy);
			printc("afc disable successfully!\n");
		}else{
			printc("afc disable failed!\n");
		}
	}
}

extern int _mtk_enable_charging(struct mtk_charger *info,bool en);
static void disable_charging_check(struct wtchg_info *info)
{
	bool charging = true;
	bool online = false;
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return;
	}
	wt_charger_get_online(info,&online);
	if(!online) {
		printc("No charger,return !\n");
		return;
	}
	
	if(info->wt_discharging_state){
		wt_dischg_status_dump(info);
		charging = false;
	}

	if((!charging) && (info->afc_enable)){
		if(afc_set_voltage(info,SET_5V)){
			info->afc_enable = false;
			mtchg_info->data.max_charger_voltage = OVP_5V;
			charger_dev_set_mivr(info->chg_dev, VINDPM_5V);
			power_supply_changed(info->psy);
			printc("afc disable successfully!\n");
		}else
			printc("afc disable failed!\n");
	}

	if (charging != info->can_charging){
		printc("charging changed: %s\n",charging?"restore charging!!":"force discharging!!");
		if(!charging && info->batt_mode != 0)
		{
			charger_dev_set_input_current(info->chg_dev,0);
			pr_err("[charger_dev_set_input_current]info->batt_mode=%d\n",info->batt_mode);
		}
		_mtk_enable_charging(mtchg_info, charging);
	}else if(!charging && (1 == charger_field_read(info, B_CHG_CFG))){
		printc("chg_stat abnormal,force disable charge!! \n");
		charger_set_chg_config(info,false);
	}
	
	info->can_charging = charging;
	mtchg_info->disable_charger = !charging;
}
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
extern int g_chg_done;
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
void wt_batt_full_capacity_check(struct wtchg_info *info)
{	
	int uisoc = 0;
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	if(!IS_ERR_OR_NULL(mtchg_info))
		uisoc = get_uisoc(mtchg_info);
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
	if(info->batt_full_capacity != 100){
		if(uisoc >= info->batt_full_capacity && !(info->wt_discharging_state & WT_CHARGE_FULL_CAP_DISCHG)){
			if(info->batt_mode == HIGHSOC_80){
				charger_set_en_hiz(info,1);
				pr_err("HIGHSOC_80 set hiz hizmode =1");
			}
			else
			{
				charger_set_en_hiz(info,0);
				pr_err("NOT HIGHSOC_80 not set hiz hizmode =0");
			}
			info->wt_discharging_state |= WT_CHARGE_FULL_CAP_DISCHG;
		}else if(uisoc <= (info->batt_full_capacity-3) && (info->wt_discharging_state & WT_CHARGE_FULL_CAP_DISCHG)){
			if(info->batt_mode == HIGHSOC_80){
				charger_set_en_hiz(info,0);
				pr_err("HIGHSOC_80 exit hiz hizmode =0");
			}
			else
			{
				charger_set_en_hiz(info,0);
				pr_err("NOT HIGHSOC_80 not set hiz hizmode =0");
			}
			info->wt_discharging_state &= ~WT_CHARGE_FULL_CAP_DISCHG;
		}
	}else{
		if((info->wt_discharging_state & WT_CHARGE_FULL_CAP_DISCHG) && (info->batt_mode == 0)){
			info->wt_discharging_state &= ~WT_CHARGE_FULL_CAP_DISCHG;
			chr_err("FULL_CAP relieve 100%!! \n");
		}
	}
	if ((info->batt_soc_rechg == 1) && (info->batt_mode == 0) && info->batt_full_capacity == 100){/*对于BASIC的回充控制*/
		if(g_chg_done && uisoc <= 95){
			_mtk_enable_charging(mtchg_info, 1);
			charger_set_en_hiz(info,1);
			mdelay(200);
			_mtk_enable_charging(mtchg_info, 0);
			charger_set_en_hiz(info,0);
		}
	}
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
	chr_err("batt_soc_rechg = %d,batt_mode = %d,g_chg_done = %d,uisoc = %d,wt_discharging_state = %d\n",info->batt_soc_rechg,info->batt_mode,g_chg_done,uisoc,info->wt_discharging_state);
}
static void wt_charger_start_timer(struct wtchg_info *info);
static void wtchg_monitor_work_handler(struct wtchg_info *info)
{
	bool online = false;
	int chg_stat = 0, uisoc = 0;	
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return;
	}
	wt_charger_get_online(info,&online);
	uisoc = get_uisoc(mtchg_info);

	if(online){		
		if(info->is_ato_versions){
			if((uisoc >= ATO_LIMIT_SOC_MAX) && !(info->wt_discharging_state & WT_CHARGE_SOC_LIMIT_DISCHG)){
				info->wt_discharging_state |= WT_CHARGE_SOC_LIMIT_DISCHG;
				printc("ATO soc limit !! \n");
			}else if(uisoc < ATO_LIMIT_SOC_MIN && (info->wt_discharging_state & WT_CHARGE_SOC_LIMIT_DISCHG)){
				info->wt_discharging_state &= ~WT_CHARGE_SOC_LIMIT_DISCHG;
				chr_err("ATO soc limit relieve !! \n");
			}
		}
		if(info->store_mode){
			if((uisoc >= STORE_MODE_SOC_MAX) && !(info->wt_discharging_state & WT_CHARGE_DEMO_MODE_DISCHG)){
				info->wt_discharging_state |= WT_CHARGE_DEMO_MODE_DISCHG;
				printc("MODE soc limit !! \n");
			}else if(uisoc < STORE_MODE_SOC_MIN && (info->wt_discharging_state & WT_CHARGE_DEMO_MODE_DISCHG)){
				info->wt_discharging_state &= ~WT_CHARGE_DEMO_MODE_DISCHG;
				chr_err("MODE soc limit relieve !! \n");
			}	
		}
		wt_batt_full_capacity_check(info);
		disable_charging_check(info);

		if(info->afc_enable && get_vbus(mtchg_info) < 6500){
			info->afc_enable = false;
			printc("## afc exit,ready retry!\n");
		}
		if((info->is_afc_charger) && (!info->force_dis_afc) && (!mtchg_info->disable_charger)){
			if((info->afc_enable) && ((uisoc > info->afc_stop_battery_soc))){
				printc("uisoc > %d or disable_charger, leave afc 9V to 5V !\n",info->afc_stop_battery_soc);
				schedule_delayed_work(&info->afc_9v_to_5v_work,0);
			}else if((!info->afc_enable) && (uisoc >= info->afc_start_battery_soc) 
					&& (uisoc <= info->afc_stop_battery_soc)){
				printc("Restore afc fast charging!\n");
				schedule_delayed_work(&info->afc_5v_to_9v_work,0);
			}
		}
		
		wtchg_dump_register(info);
	}
	chg_stat = wt_charger_get_bat_state(info);
	printc("ato=%d,afc=(%d %d %d),chg_stat=(%d:%d,%d:%d),(%d,%d,%d,%d),type:%d->%d,poll:%d,lcm=%d,otg=%d,hiz=%d,temp=%d\n",
		info->is_ato_versions,info->is_afc_charger,info->afc_enable,info->force_dis_afc,info->wt_discharging_state,
		info->can_charging,chg_stat,info->chg_stat,get_battery_voltage(mtchg_info),get_vbus(mtchg_info),
		get_battery_current(mtchg_info),uisoc,mtchg_info->chr_type,get_charger_type(mtchg_info),
		info->charger_thread_polling,info->lcm_on,info->otg_enabled,info->is_hiz,info->bat_temp);

}
struct wtchg_info *wt_get_wtchg_info(void); 
static ssize_t batt_slate_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	return sprintf(buf, "%d\n",info->slate_mode);
}

extern void pd_dpm_send_source_caps_0a(bool val);
static ssize_t batt_slate_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	int val,ret;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;
	
	info->slate_mode = val;
	if ((val == 1) || (val == 2)) {
		info->wt_discharging_state |= WT_CHARGE_SLATE_MODE_DISCHG;
	} else if (val == 0) {
		info->wt_discharging_state &= ~WT_CHARGE_SLATE_MODE_DISCHG;
		pd_dpm_send_source_caps_0a(false);
	}else if (val == 3){
		pd_dpm_send_source_caps_0a(true);
	}
	
	disable_charging_check(info);
	return size;
}
static ssize_t typec_cc_orientation_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	return sprintf(buf, "%d\n",info->cc_polarity);
}
static ssize_t afc_flag_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	return sprintf(buf, "%d\n",info->is_afc_charger);
}
static const char * const power_supply_real_type_text[] = {
	"Unknown", "USB", "USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD","PD_DRP","PD_PPS","APPLE_BRICK_ID","USB_AFC",
};

static ssize_t real_type_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	
	return sprintf(buf, "%s\n",power_supply_real_type_text[info->usb_type]);
}
static ssize_t hv_charger_status_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	bool is_hv_charger;
	
	if(info->force_dis_afc)
		is_hv_charger = false;
	else{
		if((info->pd_type == MTK_PD_CONNECT_PE_READY_SNK) ||
			(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) ||
			(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) ||
			(info->is_afc_charger))
			is_hv_charger = true;
		else
			is_hv_charger = false;
	}
	return sprintf(buf, "%d\n",is_hv_charger);
}
static ssize_t hv_charger_status_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	return size;
}

#if 0 
static ssize_t resistance_id_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int res_id = 90;
	return sprintf(buf, "%d\n",res_id);
}
#endif
static ssize_t new_charge_type_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	int curr = 0;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return sprintf(buf, "ERROR\n");
	}
	curr = get_battery_current(mtchg_info);
	if(curr > 1000)
		return sprintf(buf, "Fast\n");
	else
		return sprintf(buf, "Slow\n");
}
#define SEC_BAT_CURRENT_EVENT_NONE   0x00000
#define SEC_BAT_CURRENT_EVENT_FAST   0x00001  // fast charging
#define SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE  0x00002
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING  0x00010
#define SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING 0x00020
#define SEC_BAT_CURRENT_EVENT_USB_100MA   0x00040
#define SEC_BAT_CURRENT_EVENT_SLATE   0x00800

static ssize_t batt_current_event_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	int ret = SEC_BAT_CURRENT_EVENT_NONE;
	bool online = false;
	union power_supply_propval val;
	wt_charger_get_online(info, &online);
	if(!online)
		goto out;

	if((info->pd_type == MTK_PD_CONNECT_PE_READY_SNK) ||
			(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) ||
			(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) ||
			(info->is_afc_charger))
		ret |= SEC_BAT_CURRENT_EVENT_FAST;
	
	if(info->wt_discharging_state & WT_CHARGE_SLATE_MODE_DISCHG)
		ret |= SEC_BAT_CURRENT_EVENT_SLATE;
	
	wt_charger_get_property(info->psy,POWER_SUPPLY_PROP_STATUS,&val);
	if(val.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
		ret |= SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE;
	
	if(info->bat_temp < 10)
		ret |= SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING;
	else if(info->bat_temp > 45)
		ret |= SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING;
out:
	return sprintf(buf, "%d\n",ret);
	
}

static ssize_t batt_current_ua_now_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	int ret = 0;

	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return sprintf(buf, "%d\n",ret);
	}
	ret = get_battery_current(mtchg_info) * 1000;
	return sprintf(buf, "%d\n",ret);
}
static ssize_t hv_disable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	return sprintf(buf, "%d\n",info->force_dis_afc);
}

static ssize_t hv_disable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	int val,ret;
	bool online = false;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;
	wt_charger_get_online(info,&online);
	if((online) && (info->is_afc_charger)){
		if ((val == 1) && (info->afc_enable)) {			
			info->force_dis_afc = true;
			afc_9v_to_5v(info);
		} else if ((val == 0) && (!info->afc_enable)) {
			printc("Restore afc fast charging!\n");	
			info->force_dis_afc = false;
			afc_5v_to_9v(info);
		}
	}else{
		info->force_dis_afc = (bool)val;
	}
	return size;
}
static ssize_t start_charge_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	if(info->wt_discharging_state & WT_CHARGE_SLATE_MODE_DISCHG){
		printc("## get start charge cmd!\n");
		info->wt_discharging_state &= ~WT_CHARGE_SLATE_MODE_DISCHG;
		if(!(info->wt_discharging_state & ~WT_CHARGE_SLATE_MODE_DISCHG)){
			disable_charging_check(info);
		}else{
			wt_dischg_status_dump(info);
			return sprintf(buf, "Other ctrl discharge(0x%x)!\n",info->wt_discharging_state);
		}
		return sprintf(buf, "start charge!\n");
	}else{
		return sprintf(buf, "already start charge!\n");
	}
}

static ssize_t stop_charge_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	if(!(info->wt_discharging_state & WT_CHARGE_SLATE_MODE_DISCHG)){
		printc("## get stop charge cmd!\n");
		info->wt_discharging_state |= WT_CHARGE_SLATE_MODE_DISCHG;
		disable_charging_check(info);
		return sprintf(buf, "stop charge!\n");
	}else{
		return sprintf(buf, "already stop charge!\n");
	}
}
static ssize_t store_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	return sprintf(buf, "%d\n",info->store_mode);
}

static ssize_t store_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	int val,ret;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return size;

	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return size;
	}
	info->store_mode = (bool)val;
	if(info->store_mode){
		if((get_uisoc(mtchg_info) >= STORE_MODE_SOC_MAX) && !(info->wt_discharging_state & WT_CHARGE_DEMO_MODE_DISCHG)){
			info->wt_discharging_state |= WT_CHARGE_DEMO_MODE_DISCHG;
			printc("MODE soc limit !! \n");
		}else if(get_uisoc(mtchg_info) < STORE_MODE_SOC_MIN && (info->wt_discharging_state & WT_CHARGE_DEMO_MODE_DISCHG)){
			info->wt_discharging_state &= ~WT_CHARGE_DEMO_MODE_DISCHG;
			chr_err("MODE soc limit relieve !! \n");
		}	
	}else{
		if(info->wt_discharging_state & WT_CHARGE_DEMO_MODE_DISCHG){
			info->wt_discharging_state &= ~WT_CHARGE_DEMO_MODE_DISCHG;
			chr_err("MODE soc limit relieve !! \n");
		}
	}
	disable_charging_check(info);
	return size;
}
static ssize_t dischg_limit_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	return sprintf(buf, "%d\n",info->dischg_limit);
}

static ssize_t dischg_limit_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	int val,ret;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return size;


	info->dischg_limit = (bool)val;
	if(info->lcm_on && info->dischg_limit){
		printc("lcm_on is force set 0!\n");
		info->lcm_on = false;
	}
	printc("lcm_on is %d, dischg_limit is %d\n",info->lcm_on,info->dischg_limit);
	
	
	return size;
}
static ssize_t batt_misc_event_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int chg_type = 0;
	u32 batt_misc_event = 0;
	struct wtchg_info *info = wt_get_wtchg_info();
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	if(!IS_ERR_OR_NULL(mtchg_info))
		chg_type = get_charger_type(mtchg_info);
	
	if(chg_type == POWER_SUPPLY_TYPE_USB_FLOAT)
		batt_misc_event |= 0x4;
	else
		batt_misc_event |= 0x0;
	
	if(info->wt_discharging_state & WT_CHARGE_FULL_CAP_DISCHG)
		batt_misc_event |= BATT_MISC_EVENT_FULL_CAPACITY;

	printc("batt_misc_event = 0x%x\n",batt_misc_event);
	return sprintf(buf, "%d\n",batt_misc_event);
}

static ssize_t batt_full_capacity_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
	if(info->batt_mode == HIGHSOC_80)
		return sprintf(buf, "%s\n","80 HIGHSOC");
	else if(info->batt_mode == SLEEP_80)
		return sprintf(buf, "%s\n","80 SLEEP");
	else if(info->batt_mode == OPTION_80)
		return sprintf(buf, "%s\n","80 OPTION");
	else if(info->batt_mode == DOWN_80)
		return sprintf(buf, "%s\n","80");
	else
		return sprintf(buf, "%s\n","100");
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
}
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
static ssize_t batt_full_capacity_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct wtchg_info *info = wt_get_wtchg_info();	

	if(strncmp(buf,"80 HIGHSOC",5) == 0){
		info->batt_mode = HIGHSOC_80;
	}
	else if(strncmp(buf,"80 SLEEP",5) == 0){
		info->batt_mode = SLEEP_80;
	}
	else if(strncmp(buf,"80 OPTION",5) == 0){
		info->batt_mode = OPTION_80;
	}
	else if(strncmp(buf,"80",2) == 0){
		info->batt_mode = DOWN_80;
	}
	else{
		info->batt_mode = NORMAL_100;
	}
	pr_err("batt_mode = %d\n",info->batt_mode);
	if(info->batt_mode != 0)
		info->batt_full_capacity = 80;
	else
		info->batt_full_capacity = 100;

	wt_batt_full_capacity_check(info);
	disable_charging_check(info);
	return size;
}

static ssize_t batt_soc_rechg_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	return sprintf(buf, "%d\n",info->batt_soc_rechg);
}

static ssize_t batt_soc_rechg_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	int val,ret;
	ret = kstrtoint(buf, 10, &val);
	if(ret < 0)
		return size;

	info->batt_soc_rechg = val;
	return size;
}
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
#if 0
extern int check_cap_level(int uisoc);
static ssize_t time_to_full_now_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	/* full or unknown must return 0 */
	bool online = false;
	int ret = 0, time_to_full = 0, curr_avg = 0;
	struct mtk_battery *gm;
	struct wtchg_info *info = wt_get_wtchg_info();
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		return sprintf(buf, "%d\n",time_to_full);
	}
	if (IS_ERR_OR_NULL(info->bat_psy)) {
		info->bat_psy = power_supply_get_by_name("battery");
		printc("retry to get bat_psy\n");
		if (IS_ERR_OR_NULL(info->chg_psy)){
			printc("bat_psy is null or error !!\n");
			return sprintf(buf, "%d\n",time_to_full);;
		}
	}
	gm = (struct mtk_battery *)power_supply_get_drvdata(info->bat_psy);
	if (IS_ERR_OR_NULL(gm)) {
		return sprintf(buf, "%d\n",time_to_full);;
	}
	
	wt_charger_get_online(info,&online);
	if(online){
		ret = check_cap_level(get_uisoc(mtchg_info));
		if ((ret == POWER_SUPPLY_CAPACITY_LEVEL_FULL) || (ret == POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN))
			time_to_full = 0;
		else {
			int q_max_now = gm->fg_table_cust_data.fg_profile[
						gm->battery_id].q_max;
			int remain_ui = info->batt_full_capacity - get_uisoc(mtchg_info);
			int remain_mah = DIV_ROUND_CLOSEST(remain_ui * q_max_now / 10 * info->batt_full_capacity,100);
			
			int current_now = get_battery_current(mtchg_info);
			curr_avg = data_average_method(current_now);

			if (curr_avg > 0)
				time_to_full = remain_mah * 3600 / current_now;
			else
				time_to_full = 0;
			bm_debug("time_to_full:%d, remain:ui:%d mah:%d, curr_avg:%d, qmax:%d\n",
				time_to_full, remain_ui, remain_mah,current_now, q_max_now);
		}
	}else{
		time_to_full = 0;
		
	}
	printc("time_to_full=%d\n",time_to_full);
	return sprintf(buf, "%d\n",time_to_full);
}
#endif
static ssize_t shipmode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wtchg_info *info = wt_get_wtchg_info();
	info->shipmode = true;
	return sprintf(buf, "%d\n",info->shipmode);
}
#if 0
static ssize_t shipmode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct wtchg_info *info = wt_get_wtchg_info();	
	int val,ret;
	if (!strncasecmp(buf, "1", 1)) {
		info->shipmode = true;
	} else if (!strncasecmp(buf, "0", 1)) {
		info->shipmode = false;
	} else {
		printc("%s invalid value\n", __func__);
		return size;
	}
	return size;
}
#endif
static ssize_t voltage_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int vbus = 0;
	struct wtchg_info *info = wt_get_wtchg_info();
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
		if(!IS_ERR_OR_NULL(mtchg_info))
			vbus = get_vbus(mtchg_info);
	return sprintf(buf, "%d\n",vbus);
}

static DEVICE_ATTR_RW(batt_slate_mode);
static DEVICE_ATTR_RO(typec_cc_orientation);
static DEVICE_ATTR_RO(afc_flag);
static DEVICE_ATTR_RO(real_type);
static DEVICE_ATTR_RW(hv_charger_status);
//static DEVICE_ATTR_RO(resistance_id);
static DEVICE_ATTR_RO(new_charge_type);
static DEVICE_ATTR_RO(batt_current_event);
static DEVICE_ATTR_RO(batt_current_ua_now);
static DEVICE_ATTR_RW(hv_disable);
static DEVICE_ATTR_RO(start_charge);
static DEVICE_ATTR_RO(stop_charge);
static DEVICE_ATTR_RW(store_mode);
static DEVICE_ATTR_RW(dischg_limit);
static DEVICE_ATTR_RO(batt_misc_event);
static DEVICE_ATTR_RW(batt_full_capacity);
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
static DEVICE_ATTR_RW(batt_soc_rechg);
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
//static DEVICE_ATTR_RO(time_to_full_now);
static DEVICE_ATTR_RO(shipmode);
static DEVICE_ATTR_RO(voltage);

#ifdef CHG_WITH_STEP_CURRENT
static void wtchg_set_current_work_handler(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct wtchg_info *info = container_of(dwork, struct wtchg_info, wtchg_set_current_work);
	int reg_val,diff;
	reg_val = charger_field_read(info, B_ICHG);
	diff = abs(reg_val - info->chg_cur_val);
	printc("cur reg = %d -- %d\n",reg_val,info->chg_cur_val);
	if(diff){
		printc("cur reg = %d -> %d\n",reg_val,info->chg_cur_val);
		if(reg_val > info->chg_cur_val){
			charger_field_write(info, B_ICHG, --reg_val);
		}else if(reg_val < info->chg_cur_val){
			charger_field_write(info, B_ICHG, ++reg_val);
		}
		schedule_delayed_work(&info->wtchg_set_current_work,1*HZ/2);
	}	
}
#endif
static void wtchg_lateinit_work_handler(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct wtchg_info *info = container_of(dwork, struct wtchg_info, wtchg_lateinit_work);
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	
	
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		schedule_delayed_work(&info->wtchg_lateinit_work,1*HZ);
		return;
	}
	
	info->bat_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(info->bat_psy)) {
		printc("Couldn't get bat_psy\n");
		schedule_delayed_work(&info->wtchg_lateinit_work,1*HZ);
		return;
	} else {
		if(!info->batsys_created){
			device_create_file(&info->bat_psy->dev,&dev_attr_batt_slate_mode);
			device_create_file(&info->bat_psy->dev,&dev_attr_hv_charger_status);
			device_create_file(&info->bat_psy->dev,&dev_attr_new_charge_type);
			device_create_file(&info->bat_psy->dev,&dev_attr_batt_current_event);
			device_create_file(&info->bat_psy->dev,&dev_attr_batt_current_ua_now);
			device_create_file(&info->bat_psy->dev,&dev_attr_hv_disable);
			device_create_file(&info->bat_psy->dev,&dev_attr_start_charge);
			device_create_file(&info->bat_psy->dev,&dev_attr_stop_charge);
			device_create_file(&info->bat_psy->dev,&dev_attr_store_mode);
			device_create_file(&info->bat_psy->dev,&dev_attr_dischg_limit);
			device_create_file(&info->bat_psy->dev,&dev_attr_batt_misc_event);
			device_create_file(&info->bat_psy->dev,&dev_attr_batt_full_capacity);
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
			device_create_file(&info->bat_psy->dev,&dev_attr_batt_soc_rechg);
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
			//device_create_file(&info->bat_psy->dev,&dev_attr_time_to_full_now);
			device_create_file(&info->bat_psy->dev,&dev_attr_shipmode);
			device_create_file(&info->bat_psy->dev,&dev_attr_voltage);
			info->batsys_created = true;
		}else
			printc("batsys_created\n");
	}

	info->usb_psy = power_supply_get_by_name("usb");
	if (IS_ERR_OR_NULL(info->usb_psy)) {
		printc("Couldn't get usb_psy\n");
		schedule_delayed_work(&info->wtchg_lateinit_work,1*HZ);
		return;
	} else {
		if(!info->usbsys_created){
			device_create_file(&info->usb_psy->dev,&dev_attr_typec_cc_orientation);
			device_create_file(&info->usb_psy->dev,&dev_attr_afc_flag);
			device_create_file(&info->usb_psy->dev,&dev_attr_real_type);
			info->usbsys_created = true;
		}else
			printc("usbsys_created\n");
	} 

}

struct wtchg_info *wt_get_wtchg_info(void)
{
	struct power_supply *chg_psy;

	chg_psy = power_supply_get_by_name("wt_charger");
	if (IS_ERR_OR_NULL(chg_psy)) {
		pr_notice("Couldn't get wt_charger\n");
		return NULL;
	} else {
		return (struct wtchg_info *)power_supply_get_drvdata(chg_psy);
	} 
}
EXPORT_SYMBOL_GPL(wt_get_wtchg_info);
static int lcmoff_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank = 0,chg_cur = 0;
	struct wtchg_info *info = wt_get_wtchg_info();
	/* skip if it's not a blank event */
	if ((event != FB_EVENT_BLANK) || (data == NULL))
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	/* LCM ON */
	case FB_BLANK_UNBLANK:
		if(!info->dischg_limit){
			info->lcm_on = true;
			charger_dev_get_charging_current(info->chg_dev,&chg_cur);
			if(chg_cur > LCMON_CHARGING_CURRENT)
				charger_dev_set_charging_current(info->chg_dev,LCMON_CHARGING_CURRENT); 
			printc("lcm is on!!\n");
		}else{
			info->lcm_on = false;
			printc("lcm is force off!!\n");
		}
		break;
	/* LCM OFF */
	case FB_BLANK_POWERDOWN:
		info->lcm_on = false;
		printc("lcm is off!!\n");
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block lcmoff_fb_notifier = {
	.notifier_call = lcmoff_fb_notifier_callback,
};


static enum alarmtimer_restart
	wt_charger_alarm_timer_func(struct alarm *alarm, ktime_t now)
{
	struct wtchg_info *info = container_of(alarm, struct wtchg_info, charger_timer);
	struct mtk_charger *mtchg_info = (struct mtk_charger *)info->mtk_charger;
	if (IS_ERR_OR_NULL(mtchg_info)) {
		printc("Couldn't get mtchg_info\n");
		wt_charger_start_timer(info);
		return ALARMTIMER_NORESTART;
	}
	info->charger_thread_timeout = true;
	wake_up(&info->wait_que);

	return ALARMTIMER_NORESTART;
}
static void wt_charger_start_timer(struct wtchg_info *info)
{
	struct timespec time, time_now;
	ktime_t ktime;
	int ret = 0;

	/* If the timer was already set, cancel it */
	ret = alarm_try_to_cancel(&info->charger_timer);
	if (ret < 0) {
		printc("%s: callback was running, skip timer\n", __func__);
		return;
	}

	get_monotonic_boottime(&time_now);
	time.tv_sec = 10;
	time.tv_nsec = 0;
	info->endtime = timespec_add(time_now, time);
	ktime = ktime_set(info->endtime.tv_sec, info->endtime.tv_nsec);

	printc("%s: alarm timer start:%d, %ld %ld\n", __func__, ret,
		info->endtime.tv_sec, info->endtime.tv_nsec);
	alarm_start(&info->charger_timer, ktime);
}

static void wt_charger_init_timer(struct wtchg_info *info)
{
	alarm_init(&info->charger_timer, ALARM_BOOTTIME,
			wt_charger_alarm_timer_func);
	wt_charger_start_timer(info);
}
static int wtchg_routine_thread(void *arg)
{
	struct wtchg_info *info = arg;
	while (1) {
		wait_event(info->wait_que,(info->charger_thread_timeout == true));

		mutex_lock(&info->charger_lock);
		info->charger_thread_timeout = false;

		wtchg_monitor_work_handler(info);
		if (info->charger_thread_polling == true)
			wt_charger_start_timer(info);
		
		mutex_unlock(&info->charger_lock);
	}
	return 0;
}

static int pd_tcp_notifier_call(struct notifier_block *pnb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct wtchg_info *info;
	int ret = 0;
	info = container_of(pnb, struct wtchg_info, pd_nb);

	printc("PD charger event:%d %d\n", (int)event,
		(int)noti->pd_state.connected);

	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case  PD_CONNECT_NONE:
			info->pd_type = MTK_PD_CONNECT_NONE;
			break;

		case PD_CONNECT_HARD_RESET:
			info->pd_type = MTK_PD_CONNECT_NONE;
			break;

		case PD_CONNECT_PE_READY_SNK:
			info->pd_type = MTK_PD_CONNECT_PE_READY_SNK;
			break;

		case PD_CONNECT_PE_READY_SNK_PD30:
			info->pd_type = MTK_PD_CONNECT_PE_READY_SNK_PD30;
			break;

		case PD_CONNECT_PE_READY_SNK_APDO:
			info->pd_type = MTK_PD_CONNECT_PE_READY_SNK_APDO;
			break;

		case PD_CONNECT_TYPEC_ONLY_SNK_DFT:
			/* fall-through */
		case PD_CONNECT_TYPEC_ONLY_SNK:
			info->pd_type = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
			break;
		};
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		/* handle No-rp and dual-rp cable */
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		   (noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			info->pd_type = MTK_PD_CONNECT_TYPEC_ONLY_SNK;
		} else if ((noti->typec_state.old_state ==
			TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			info->pd_type = MTK_PD_CONNECT_NONE;
		}
		break;
	}
	printc("## pd_type = %d\n",info->pd_type);
	power_supply_changed(info->psy);
	return ret;
}

static int wt_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct wtchg_info *info = NULL;
	struct device_node *node = client->dev.of_node;
	struct power_supply_config charger_cfg = {};
	struct device *dev = &client->dev;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret,i,j;
	
	this_dev = dev;
	printc("%s in\n",__func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		printc("No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info){
		printc("devm_kzalloc error\n");
		return -ENOMEM;
	}

	info->client = client;
	info->dev = dev;
	mutex_init(&info->i2c_rw_lock);
	for(i = 0;NULL != charger_ic_list[i];i++){
		info->cur_chg = charger_ic_list[i];
		client->addr = info->cur_chg->i2c_addr;
		printc("client->addr=0x%x\n",client->addr);
	#if 0
		info->rmap = devm_regmap_init_i2c(client,info->cur_chg->regmap_config);
		if (IS_ERR(info->rmap)) {
			printc("failed !!! to allocate register map\n");
			return PTR_ERR(info->rmap);
		}
		for (j = 0; j < info->cur_chg->n_reg_field; j++) {
			info->rmap_fields[j] = devm_regmap_field_alloc(dev, info->rmap,
								     info->cur_chg->bits_field[j]);
			if (IS_ERR(info->rmap_fields[j])) {
				printc("cannot allocate regmap field\n");
				return PTR_ERR(info->rmap_fields[j]);
			}
		}
	#else
		info->cur_field = info->cur_chg->bits_field;
	#endif
		i2c_set_clientdata(client, info);

		info->cur_chg->chip_id = charger_field_read(info, B_PN);
		if (info->cur_chg->chip_id < 0) {
			printc("Cannot read chip ID.\n");
			continue;
		}
		if (info->cur_chg->chip_id == 0 || info->cur_chg->chip_id == 0xff ) {
			printc("Invalid id!\n");
			continue;
		}
		for(j = 0;j < ARRAY_SIZE(info->cur_chg->match_id);j++){
			if (info->cur_chg->chip_id != info->cur_chg->match_id[j]) {
				printc("%s id[%d] is not match: %d != %d\n",info->cur_chg->name,j,info->cur_chg->chip_id,info->cur_chg->match_id[j]);
			}else{
				printc("%s id[%d] is match: %d = %d\n",info->cur_chg->name,j,info->cur_chg->chip_id,info->cur_chg->match_id[j]);
				break;
			}
		}
		if(j == ARRAY_SIZE(info->cur_chg->match_id))
			continue;		
		break;
	}
	if(NULL == charger_ic_list[i]){
		printc("Couldn't find any charger.\n");
		return -1;
	}
	hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, info->cur_chg->name);
	ret= charger_parse_dt(node, info);

#ifdef FIXME
	ret = get_boot_mode();
	if (ret == KERNEL_POWER_OFB_CHARGING_BOOT ||
	    ret == LOW_POWER_OFB_CHARGING_BOOT)
		info->tcpc_kpoc = true;
#endif
	//charger_register_interrupt(info);

	info->chg_dev = charger_device_register(info->chg_dev_name,
					      &client->dev, info,
					      &wt_charger_ops,
					      &wt_chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev)) {
		ret = PTR_ERR(info->chg_dev);
		return ret;
	}

	/* power supply register */
	memcpy(&info->psy_desc, &wt_charger_desc, sizeof(info->psy_desc));
	memcpy(&info->psy_otg_desc, &wt_otg_desc, sizeof(info->psy_otg_desc));
	

	charger_cfg.drv_data = info;
	charger_cfg.of_node = client->dev.of_node;
	charger_cfg.supplied_to =  wt_charger_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(wt_charger_supplied_to);
	info->psy = devm_power_supply_register(info->dev,
					&info->psy_desc, &charger_cfg);
	if (IS_ERR(info->psy)) {
		printc("Fail to register power supply dev\n");
		ret = PTR_ERR(info->psy);
		goto err_ext;
	}
	info->otg_psy = devm_power_supply_register(info->dev,
						&info->psy_otg_desc, &charger_cfg);
	if (IS_ERR(info->otg_psy)) {
		printc("Fail to register otg_psy power supply dev\n");
		ret = PTR_ERR(info->otg_psy);
		goto err_ext;
	}

	ret = charger_init_device(info);
	if (ret) {
		printc("Failed to init device\n");
		return ret;
	}
	if(fb_register_client(&lcmoff_fb_notifier))
		printc("fb_register_client failed\n");

	
	afc_charge_init(info);
#if 0
	info->psy_nb.notifier_call = afc_check_psy_notifier;
	ret = power_supply_reg_notifier(&info->psy_nb);
	if (ret) {
		printc("fail to register notifer\n");
		return ret;
	}
#endif
	INIT_DELAYED_WORK(&info->afc_check_work, afc_check_work_handler);
#ifdef WT_COMPILE_FACTORY_VERSION
		info->is_ato_versions = true;
#else
		info->is_ato_versions = false;
#endif
	info->batt_full_capacity = 100;
//+P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
	info->batt_soc_rechg = 0;
	info->batt_mode = NORMAL_100;
//-P240305-05569  guhan01.wt 20240318,one ui 6.1 charging protection
	INIT_DELAYED_WORK(&info->afc_5v_to_9v_work, afc_5v_to_9v_work_handler);
	INIT_DELAYED_WORK(&info->afc_9v_to_5v_work, afc_9v_to_5v_work_handler);
	INIT_DELAYED_WORK(&info->wtchg_lateinit_work, wtchg_lateinit_work_handler);
#ifdef CHG_WITH_STEP_CURRENT
	INIT_DELAYED_WORK(&info->wtchg_set_current_work, wtchg_set_current_work_handler);
#endif
	info->charger_thread_polling = false;
	mutex_init(&info->charger_lock);
	spin_lock_init(&info->slock);
	init_waitqueue_head(&info->wait_que);
	wt_charger_init_timer(info);
	kthread_run(wtchg_routine_thread, info, "charger_thread");

	//schedule_delayed_work(&info->wtchg_monitor_work,5*HZ);
	schedule_delayed_work(&info->wtchg_lateinit_work,4*HZ);
	info->wtchg_wakelock = wakeup_source_register(NULL, "wtchg_wakelock");

	info->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!info->tcpc_dev) {
		printc("%s get tcpc device type_c_port0 fail\n");
	}else{
		info->pd_nb.notifier_call = pd_tcp_notifier_call;
		ret = register_tcp_dev_notifier(info->tcpc_dev, &info->pd_nb,
					TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_MISC);
	}
	printc("wt charger probe successfully! \n");
	return 0;

err_ext:
	charger_device_unregister(info->chg_dev);
	return ret;
}

static int wt_charger_remove(struct i2c_client *client)
{
	return 0;
}

static void wt_charger_shutdown(struct i2c_client *client)
{
	struct wtchg_info *info = wt_get_wtchg_info();	
	if(info->shipmode){
		printc("## shutdown with entering shipmode!\n");
		charger_set_shipmode_delay(info,0);
		charger_set_shipmode(info,info->shipmode);
	}else
		printc("##shutdown only!!\n");
}

static struct i2c_driver wt_charger_driver = {
	.driver = {
		   .name = "wt_charger",
		   .owner = THIS_MODULE,
		   .of_match_table = charger_match_table,
		   },

	.probe = wt_charger_probe,
	.remove = wt_charger_remove,
	.shutdown = wt_charger_shutdown,

};

module_i2c_driver(wt_charger_driver);

MODULE_DESCRIPTION("WT Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");


