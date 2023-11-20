/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <soc/sprd/adc.h>
#include <soc/sprd/usb.h>
#include "sprd_battery.h"
#include "fan54015.h"

#define SPRDBAT__DEBUG
#ifdef SPRDBAT__DEBUG
#define SPRDBAT_DEBUG(format, arg...) pr_info("sprdbat: " format, ## arg)
#else
#define SPRDBAT_DEBUG(format, arg...)
#endif

#define SPRDBAT_CV_TRIGGER_CURRENT		7/10

enum sprdbat_event {
	SPRDBAT_ADP_PLUGIN_E,
	SPRDBAT_ADP_PLUGOUT_E,
	SPRDBAT_OVI_STOP_E,
	SPRDBAT_OVI_RESTART_E,
	SPRDBAT_OTP_COLD_STOP_E,
	SPRDBAT_OTP_OVERHEAT_STOP_E,
	SPRDBAT_OTP_COLD_RESTART_E,
	SPRDBAT_OTP_OVERHEAT_RESTART_E,
	SPRDBAT_CHG_FULL_E,
	SPRDBAT_RECHARGE_E,
	SPRDBAT_CHG_TIMEOUT_E,
	SPRDBAT_CHG_TIMEOUT_RESTART_E,
	SPRDBAT_VBAT_OVP_E,
	SPRDBAT_CHG_UNSPEC_E,
	SPRDBAT_FULL_TO_CHARGING_E,
};
static int jeita_debug = 400;
static struct sprdbat_drivier_data *sprdbat_data;
struct sprd_ext_ic_operations *sprd_ext_ic_op;
static void sprdbat_change_module_state(uint32_t event);
static int sprdbat_stop_charge(void);
static unsigned long  adjust_chg_flag = 0;
#define CUR_ADJUST_OFFSET 	(100)
#define CUR_BUFF_CNT (5)
#define VOL_BUFF_CNT (3)
static int current_buff[CUR_BUFF_CNT] = {0,0,0,0,0};
static int chg_vol_buff[VOL_BUFF_CNT] = {5000, 5000, 5000};

static int sprdbat_get_avgval_from_buff(int value, int *buf ,int count ,int type)
{
	static int cur_pointer = 0,vol_pointer = 0;
	int i = 0, sum = 0;
	SPRDBAT_DEBUG("sprdbat_get_val_from_buff value=%d,type =%d\n",value,type);
	if(type){
		if(cur_pointer >= count ) cur_pointer = 0;
		buf[cur_pointer++] = value;
	}else{
		if(vol_pointer >= count ) vol_pointer = 0;
		buf[vol_pointer++] = value;
	}
	for(i = 0; i < count ; i++){
		sum  += buf[i];
	}
	return (sum /count);
}

static void sprdbat_clear_buff(int *buf, int count, int value)
{
	int i=0;
	SPRDBAT_DEBUG("sprdbat_clear_buff\n");
	for(i = 0; i < count; i++)
		buf[i] = value;
}

void sprdbat_chgcurrent_adjust(int state)
{
	int chg_cur = sprd_ext_ic_op->get_charge_cur_ext();
	int avg_cur = sprdbat_data->bat_info.bat_current_avg;
	int state_cur = sprdbat_data->chg_cur_adjust_min;
	int i =1,delta = 0;

	if((state < 0) ||(sprdbat_data->bat_info.adp_type != ADP_TYPE_DCP))
		return;
	if(state == 0){
		state_cur = sprdbat_data->chg_cur_adjust_max;
	}
	SPRDBAT_DEBUG("sprdbat_chgcurrent_adjust enter chgcur=%d,avg_cur=%d,state_cur = %d\n",
		chg_cur, avg_cur, state_cur);
#ifdef CHG_CUR_JEITA
	if(sprdbat_data->last_temp_status == 0 ||
		sprdbat_data->last_temp_status >= sprdbat_data->pdata->jeita_tab_size){
		SPRDBAT_DEBUG("error last_temp_status\n");
		return ;
	}else{
		if(state_cur > sprdbat_data->pdata->jeita_tab[sprdbat_data->last_temp_status].y)
			state_cur = sprdbat_data->pdata->jeita_tab[sprdbat_data->last_temp_status].y;
	}
#endif
	if( state_cur > sprdbat_data->pdata->adp_dcp_cur){
			state_cur = sprdbat_data->pdata->adp_dcp_cur;
	}
	delta = state_cur -chg_cur + avg_cur;
	if(delta > 0){
		SPRDBAT_DEBUG("delta > 0 sprdchg_set_chg_cur %d \n",state_cur);
		sprd_ext_ic_op->charge_start_ext(state_cur);
	}else{
		for(i = 1; i <= 20; i++){
			delta += CUR_ADJUST_OFFSET;
			if(delta > 0){
				SPRDBAT_DEBUG("i=%d,deta > 0 break\n",i);
				break;
			}
		}
		state_cur += 50 * i;
		if( state_cur > sprdbat_data->pdata->adp_dcp_cur){
			state_cur = sprdbat_data->pdata->adp_dcp_cur;
		}
		SPRDBAT_DEBUG("state_cur =%d,set cur\n",state_cur);
		sprd_ext_ic_op->charge_start_ext(state_cur);
	}
	sprdbat_clear_buff(current_buff, CUR_BUFF_CNT, 0);
}
static void sprdbat_auto_switch_cur(void)
{
	int avg_cur = sprdbat_data->bat_info.bat_current_avg;
	int chg_cur = sprd_ext_ic_op->get_charge_cur_ext();
	SPRDBAT_DEBUG("enter sprdbat_auto_switch_cur avg_cur=%d,chg_cur=%d\n",
			avg_cur, chg_cur);
	if(sprdbat_data->bat_info.adp_type != ADP_TYPE_DCP){
		return ;
	}
	if((avg_cur < 0) && (chg_cur < sprdbat_data->chg_cur_adjust_max)){
		SPRDBAT_DEBUG("sprdbat_auto_increase_cur need ++chg_cur %d =>%d\n",
			chg_cur,chg_cur + CUR_ADJUST_OFFSET);
		chg_cur += CUR_ADJUST_OFFSET;
#ifdef CHG_CUR_JEITA
		if(sprdbat_data->last_temp_status == 0 ||
		sprdbat_data->last_temp_status >= sprdbat_data->pdata->jeita_tab_size){
			SPRDBAT_DEBUG("error last_temp_status,return\n");
			return ;
		}else{
			if(chg_cur > sprdbat_data->pdata->jeita_tab[sprdbat_data->last_temp_status].y)
			chg_cur = sprdbat_data->pdata->jeita_tab[sprdbat_data->last_temp_status].y;
		}
#endif
		sprd_ext_ic_op->charge_start_ext(chg_cur);
		sprdbat_clear_buff(current_buff, CUR_BUFF_CNT, 0);
	}
	if((adjust_chg_flag > 0) && (avg_cur > (CUR_ADJUST_OFFSET+50))
			&& (chg_cur > sprdbat_data->chg_cur_adjust_min)){
		SPRDBAT_DEBUG("sprdbat_auto_switch_cur need --chg_cur %d => min\n",
			chg_cur);
		sprdbat_chgcurrent_adjust(1);
		sprdbat_clear_buff(current_buff, CUR_BUFF_CNT, 0);
	}
}

static int sprdbat_is_chg_timeout(void)
{
	struct timespec cur_time;

	get_monotonic_boottime(&cur_time);
	if (cur_time.tv_sec - sprdbat_data->bat_info.chg_start_time >
	    sprdbat_data->bat_info.chg_this_timeout)
		return 1;
	else
		return 0;
}
static int sprdbat_get_status_from_tab(int size, struct sprdbat_jeita_table_data *buf, int data)
{
	int i = 0;
	for(i = 0; i < size; i++ ){
		SPRDBAT_DEBUG("data=%d, buf[%d].x=%d\n",data,i, buf[i].x);
		if(data <= buf[i].x)
			break;
	}
	if(i == size){
		i --;
	}
	SPRDBAT_DEBUG("return index =%d\n",i);
	return i ;
}

static void sprdbat_current_adjust_by_status(uint32_t status)
{
	int cccv = sprdbat_data->pdata->jeita_tab[status].z;
	int target_cur = sprdbat_data->pdata->jeita_tab[status].y;
	if((sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP)&&
		(target_cur >  sprdbat_data->pdata->adp_dcp_cur)){
		target_cur =  sprdbat_data->pdata->adp_dcp_cur;
	}else if((sprdbat_data->bat_info.adp_type == ADP_TYPE_SDP)&&
		(target_cur >  sprdbat_data->pdata->adp_sdp_cur)){
		target_cur = sprdbat_data->pdata->adp_dcp_cur;
	}else if((sprdbat_data->bat_info.adp_type == ADP_TYPE_CDP)&&
		(target_cur >  sprdbat_data->pdata->adp_cdp_cur)){
		target_cur = sprdbat_data->pdata->adp_cdp_cur;
	}else{
		SPRDBAT_DEBUG("sprdbat_current_adjust_by_status\n");
	}
	SPRDBAT_DEBUG("status =%d cccv=%d,target_cur=%d\n",status,cccv,target_cur);
	switch(status){
		case 0:
			if(sprdbat_data->bat_info.
	    			chg_stop_flags & SPRDBAT_CHG_END_OTP_COLD_BIT){
	    			SPRDBAT_DEBUG("bat is  already cold return \n");
				break ;
			}
			SPRDBAT_DEBUG("bat turn cold status,stop charge\n");
			sprdbat_change_module_state
					    (SPRDBAT_OTP_COLD_STOP_E);
			sprdbat_data->stop_charge();
			break;
		case 5:
			if(sprdbat_data->bat_info.
	 		   chg_stop_flags & SPRDBAT_CHG_END_OTP_OVERHEAT_BIT){
	    			SPRDBAT_DEBUG("bat is  already hot return \n");
				break ;
			}
			SPRDBAT_DEBUG("bat turn hot status,stop charge\n");
			sprdbat_change_module_state
					    (SPRDBAT_OTP_OVERHEAT_STOP_E);
			sprdbat_data->stop_charge();
			break;
		default:
			SPRDBAT_DEBUG("status change set cccv and cur\n");
			if(sprdbat_data->bat_info.
	    			chg_stop_flags & SPRDBAT_CHG_END_OTP_COLD_BIT){
				sprdbat_change_module_state(SPRDBAT_OTP_COLD_RESTART_E);
				sprdbat_data->start_charge();
			}
	    		if(sprdbat_data->bat_info.
	 		   	chg_stop_flags & SPRDBAT_CHG_END_OTP_OVERHEAT_BIT){
				sprdbat_change_module_state(SPRDBAT_OTP_OVERHEAT_RESTART_E);
				sprdbat_data->start_charge();
			}
			if (sprdbat_data->bat_info.chg_stop_flags != SPRDBAT_CHG_END_NONE_BIT) {
				SPRDBAT_DEBUG("can't change cur ,something wrong\n");
				return;
			}
			sprd_ext_ic_op->set_termina_vol_ext(cccv);
			sprd_ext_ic_op->charge_start_ext(target_cur);
	}
}
static void sprdbat_temp_monitor(void)
{
	int cur_temp = sprdbat_read_temp(), chg_cur = sprd_ext_ic_op->get_charge_cur_ext();
	uint32_t cur_temp_status = 0;
#ifdef JEITA_DEBUG
	sprdbat_data->bat_info.cur_temp = jeita_debug;
#else
	sprdbat_data->bat_info.cur_temp = cur_temp;
#endif
	SPRDBAT_DEBUG("sprdbat_temp_monitor cur_temp =%d,chg_cur=%d,jeita_debug=%d\n",
			cur_temp,chg_cur,jeita_debug);
	cur_temp_status = sprdbat_get_status_from_tab(sprdbat_data->pdata->jeita_tab_size,
								sprdbat_data->pdata->jeita_tab,
									sprdbat_data->bat_info.cur_temp + 1000);
	if(cur_temp_status > sprdbat_data->last_temp_status){
		SPRDBAT_DEBUG("temp up , temp_up_trigger_cnt =%d\n",sprdbat_data->temp_up_trigger_cnt);
		sprdbat_data->temp_up_trigger_cnt++;
		sprdbat_data->temp_down_trigger_cnt = 0;
		if (sprdbat_data->temp_up_trigger_cnt > SPRDBAT_TEMP_TRIGGER_TIMES) {
			sprdbat_current_adjust_by_status(cur_temp_status);
			sprdbat_data->last_temp_status = cur_temp_status;
		}
	}else if(cur_temp_status < sprdbat_data->last_temp_status){
		SPRDBAT_DEBUG("temp down ");
		sprdbat_data->temp_up_trigger_cnt =0;
		if(cur_temp <=
			sprdbat_data->pdata->jeita_tab[sprdbat_data->last_temp_status -1].x -30){
			SPRDBAT_DEBUG("temp really down\n");
			sprdbat_data->temp_down_trigger_cnt++;
			if (sprdbat_data->temp_down_trigger_cnt > SPRDBAT_TEMP_TRIGGER_TIMES) {
				sprdbat_current_adjust_by_status(cur_temp_status);
				sprdbat_data->last_temp_status = cur_temp_status;
			}
		}
	}else{
		sprdbat_data->temp_up_trigger_cnt =0;
		sprdbat_data->temp_down_trigger_cnt =0;
	}
}

static void sprdbat_chg_timeout_monitor(void)
{
	SPRDBAT_DEBUG("sprdbat_chg_timeout_monitor enter\n");
	if(sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_TIMEOUT_BIT){
		SPRDBAT_DEBUG("sprdbat_chg_timeout_monitor recharge\n");
		sprdbat_change_module_state(SPRDBAT_CHG_TIMEOUT_RESTART_E);
		sprdbat_data->start_charge();
	}
	if (sprdbat_data->bat_info.chg_stop_flags == SPRDBAT_CHG_END_NONE_BIT) {
		if (sprdbat_is_chg_timeout()) {
			SPRDBAT_DEBUG("sprdbat_chg_timeout_monitor chg timeout\n");
			if (sprdbat_data->bat_info.vbat_ocv >
			    sprdbat_data->pdata->chg_end_vol_l) {
				sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
				sprdbat_data->stop_charge();
			} else {
				sprdbat_data->bat_info.chg_this_timeout =
				    sprdbat_data->pdata->chg_rechg_timeout;
				sprdbat_change_module_state
				    (SPRDBAT_CHG_TIMEOUT_E);
				sprdbat_data->stop_charge();
			}
		}
		return;
	}
	return;
}

static void sprdbat_chg_ovp_monitor(void)
{
	SPRDBAT_DEBUG("sprdbat_ovp_monitor avg_chg_vol = %d,ovp_stop =%d\n",
		sprdbat_data->bat_info.avg_chg_vol, sprdbat_data->pdata->ovp_stop);

	if(sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_OVP_BIT){
		if(sprdbat_data->bat_info.avg_chg_vol <=sprdbat_data->pdata->ovp_restart){
		SPRDBAT_DEBUG("charge vol low restart chg\n");
		sprdbat_change_module_state(SPRDBAT_OVI_RESTART_E);
		sprdbat_data->start_charge();
		}else{
			SPRDBAT_DEBUG("sprdbat_chg_ovp_monitor ovp return ");
		}
		return;
	}
	if(sprdbat_data->bat_info.avg_chg_vol >= sprdbat_data->pdata->ovp_stop){
		SPRDBAT_DEBUG("charge vol is too high\n");
		sprdbat_change_module_state(SPRDBAT_OVI_STOP_E);
		sprdbat_data->stop_charge();
	}
	return ;
}

void sprd_extic_otg_power(int enable)
{
	sprd_ext_ic_op->otg_charge_ext(enable);
}
EXPORT_SYMBOL_GPL(sprd_extic_otg_power);
void sprdbat_register_ext_ops(const struct sprd_ext_ic_operations * ops)
{
	sprd_ext_ic_op = ops;
}
EXPORT_SYMBOL_GPL(sprdbat_register_ext_ops);
void sprdbat_unregister_ext_ops(void)
{
	sprd_ext_ic_op = NULL;
}
EXPORT_SYMBOL_GPL(sprdbat_unregister_ext_ops);
int sprdbat_interpolate(int x, int n, struct sprdbat_table_data *tab)
{
	int index;
	int y;

	if (x >= tab[0].x)
		y = tab[0].y;
	else if (x <= tab[n - 1].x)
		y = tab[n - 1].y;
	else {
		/*  find interval */
		for (index = 1; index < n; index++)
			if (x > tab[index].x)
				break;
		/*  interpolate */
		y = (tab[index - 1].y - tab[index].y) * (x - tab[index].x)
		    * 2 / (tab[index - 1].x - tab[index].x);
		y = (y + 1) / 2;
		y += tab[index].y;
	}
	return y;
}

uint32_t sprd_get_vbat_voltage(void)
{
	return sprdbat_read_vbat_vol();
}

static int sprdbat_ac_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct sprdbat_drivier_data *data = container_of(psy,
							 struct
							 sprdbat_drivier_data,
							 ac);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (likely(psy->type == POWER_SUPPLY_TYPE_MAINS)) {
			val->intval = data->bat_info.ac_online ? 1 : 0;
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprdbat_usb_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct sprdbat_drivier_data *data = container_of(psy,
							 struct
							 sprdbat_drivier_data,
							 usb);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->bat_info.usb_online ? 1 : 0;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sprdbat_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sprdbat_drivier_data *data = container_of(psy,
							 struct
							 sprdbat_drivier_data,
							 battery);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = data->bat_info.module_state;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = data->bat_info.bat_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = data->bat_info.capacity;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = data->bat_info.vbat_vol * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = data->bat_info.cur_temp;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property sprdbat_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property sprdbat_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property sprdbat_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};
static ssize_t sprdbat_store_caliberate(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t sprdbat_show_caliberate(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

#define SPRDBAT_CALIBERATE_ATTR(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP, },  \
	.show = sprdbat_show_caliberate,                  \
	.store = sprdbat_store_caliberate,                              \
}
#define SPRDBAT_CALIBERATE_ATTR_RO(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IRUGO, },  \
	.show = sprdbat_show_caliberate,                  \
}
#define SPRDBAT_CALIBERATE_ATTR_WO(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IWUSR | S_IWGRP, },  \
	.store = sprdbat_store_caliberate,                              \
}

static struct device_attribute sprd_caliberate[] = {
	SPRDBAT_CALIBERATE_ATTR_RO(real_time_voltage),
	SPRDBAT_CALIBERATE_ATTR_WO(stop_charge),
	SPRDBAT_CALIBERATE_ATTR_RO(real_time_current),
	SPRDBAT_CALIBERATE_ATTR_RO(charger_voltage),
	SPRDBAT_CALIBERATE_ATTR_RO(real_time_vbat_adc),
	SPRDBAT_CALIBERATE_ATTR_WO(save_capacity),
	SPRDBAT_CALIBERATE_ATTR_RO(temp_and_adc),
	SPRDBAT_CALIBERATE_ATTR(adjust_cur_min),
	SPRDBAT_CALIBERATE_ATTR(adjust_cur_max),
#ifdef JEITA_DEBUG
	SPRDBAT_CALIBERATE_ATTR(debug_jeita),
#endif
#ifdef CHG_CUR_ADJUST
	SPRDBAT_CALIBERATE_ATTR(chg_cool_state),
#endif
};

enum sprdbat_attribute {
	BATTERY_VOLTAGE = 0,
	STOP_CHARGE,
	BATTERY_NOW_CURRENT,
	CHARGER_VOLTAGE,
	BATTERY_ADC,
	SAVE_CAPACITY,
	TEMP_AND_ADC,
	ADJUST_CUR_MIN,
	ADJUST_CUR_MAX,
#ifdef JEITA_DEBUG
	DEBUG_JEITA,
#endif
#ifdef CHG_CUR_ADJUST
	CHG_COOL_STATE
#endif
};

static ssize_t sprdbat_store_caliberate(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long set_value;
	unsigned long irq_flag = 0;
	const ptrdiff_t off = attr - sprd_caliberate;

	set_value = simple_strtoul(buf, NULL, 10);
	pr_info("battery calibrate value %lu %lu\n", off, set_value);

	mutex_lock(&sprdbat_data->lock);
	switch (off) {
	case STOP_CHARGE:
		if (0 == set_value) {
			sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
			sprdbat_data->bat_info.chg_stop_flags = SPRDBAT_CHG_END_NONE_BIT;
			sprdbat_change_module_state(SPRDBAT_ADP_PLUGIN_E);
			sprdbat_data->start_charge();
		} else {
			sprdbat_change_module_state(SPRDBAT_ADP_PLUGOUT_E);
			sprdbat_data->stop_charge();
		}
		break;
	case SAVE_CAPACITY:
		{
			int temp = set_value - sprdbat_data->poweron_capacity;

			pr_info("battery temp:%d\n", temp);
			if (abs(temp) >
			    sprdbat_data->pdata->cap_valid_range_poweron
			    || 0 == set_value) {
				pr_info("battery poweron capacity:%lu,%d\n",
					set_value, sprdbat_data->poweron_capacity);
				sprdbat_data->bat_info.capacity =
				    sprdbat_data->poweron_capacity;
			} else {
				pr_info("battery old capacity:%lu,%d\n",
					set_value, sprdbat_data->poweron_capacity);
				sprdbat_data->bat_info.capacity = set_value;
			}
			power_supply_changed(&sprdbat_data->battery);
		}
		break;
	case ADJUST_CUR_MIN:
		sprdbat_data->chg_cur_adjust_min = set_value;
		break;
	case ADJUST_CUR_MAX:
		sprdbat_data->chg_cur_adjust_max = set_value;
		break;
#ifdef JEITA_DEBUG
	case DEBUG_JEITA:
		jeita_debug = set_value -200;
		break;
#endif
#ifdef CHG_CUR_ADJUST
	case CHG_COOL_STATE:
		if(set_value > 0){
			SPRDBAT_DEBUG("chg_set_cur_state state>0 =>%d ,adjust_chg_flag=%d\n",
			set_value,adjust_chg_flag);
			adjust_chg_flag = set_value;
		}else if(set_value ==0){
			SPRDBAT_DEBUG("chg_set_cur_state state==0 =%d \n",set_value);
			adjust_chg_flag = 0;
		}else{
			break;
		}
		sprdbat_chgcurrent_adjust(set_value);
		break;
#endif
	default:
		count = -EINVAL;
		break;
	}
	mutex_unlock(&sprdbat_data->lock);
	return count;
}

static ssize_t sprdbat_show_caliberate(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int i = 0;
	const ptrdiff_t off = attr - sprd_caliberate;
	int adc_value,temp_value;
	int voltage;
	uint32_t now_current;
	int chg_cur = sprd_ext_ic_op->get_charge_cur_ext();
	switch (off) {
	case BATTERY_VOLTAGE:
		voltage = sprdchg_read_vbat_vol();
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", voltage);
		break;
	case BATTERY_NOW_CURRENT:
		if (sprdbat_data->bat_info.module_state ==
		    POWER_SUPPLY_STATUS_CHARGING) {
			//now_current = sprdchg_read_chg_current();
			//now_current = 0;
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				       "ext charge ic");
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				       "discharging");
		}
		break;
	case CHARGER_VOLTAGE:
		if (sprdbat_data->bat_info.module_state ==
		    POWER_SUPPLY_STATUS_CHARGING) {
			voltage = sprdchg_read_vchg_vol();
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", voltage);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				       "discharging");
		}

		break;
	case BATTERY_ADC:
		adc_value = sci_adc_get_value(ADC_CHANNEL_VBAT, false);
		if (adc_value < 0)
			adc_value = 0;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", adc_value);
		break;
	case TEMP_AND_ADC:
		adc_value = sprdbat_read_temp_adc();
		temp_value = sprdbat_read_temp();
		if (adc_value < 0)
			adc_value = 0;
		i += scnprintf(buf + i, PAGE_SIZE - i, "adc:%d,temp:%d\n", adc_value,temp_value);
		break;
	case ADJUST_CUR_MIN:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			sprdbat_data->chg_cur_adjust_min);
		break;
	case ADJUST_CUR_MAX:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			sprdbat_data->chg_cur_adjust_max);
		break;
#ifdef JEITA_DEBUG
	case DEBUG_JEITA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "temp:%d,cur%d\n",
			jeita_debug,chg_cur);
		break;
#endif
#ifdef CHG_CUR_ADJUST
	case CHG_COOL_STATE:
		adc_value = adjust_chg_flag;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d,%d\n", adc_value,chg_cur);
		break;
#endif
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

static int sprdbat_creat_caliberate_attr(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sprd_caliberate); i++) {
		rc = device_create_file(dev, &sprd_caliberate[i]);
		if (rc)
			goto sprd_attrs_failed;
	}
	goto sprd_attrs_succeed;

sprd_attrs_failed:
	while (i--)
		device_remove_file(dev, &sprd_caliberate[i]);

sprd_attrs_succeed:
	return rc;
}

static int sprdbat_remove_caliberate_attr(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_caliberate); i++) {
		device_remove_file(dev, &sprd_caliberate[i]);
	}
	return 0;
}

static void sprdbat_info_init(struct sprdbat_drivier_data *data)
{
	struct timespec cur_time;
	data->bat_info.adp_type = ADP_TYPE_UNKNOW;
	if (data->gpio_vbat_detect > 0) {
		if (gpio_get_value(sprdbat_data->gpio_vbat_detect)) {
			SPRDBAT_DEBUG("vbat good!!!\n");
			data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
			data->bat_info.chg_stop_flags =
			    SPRDBAT_CHG_END_NONE_BIT;
		} else {
			SPRDBAT_DEBUG("vbat unspec!!!\n");
			data->stop_charge();
			data->bat_info.bat_health =
			    POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			data->bat_info.chg_stop_flags |= SPRDBAT_CHG_END_UNSPEC;
		}
	} else {
		SPRDBAT_DEBUG("vbat no detected!!!\n");
		data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
		data->bat_info.chg_stop_flags = SPRDBAT_CHG_END_NONE_BIT;
	}
	data->bat_info.module_state = POWER_SUPPLY_STATUS_DISCHARGING;
	data->bat_info.chg_start_time = 0;
	get_monotonic_boottime(&cur_time);
	sprdbat_data->sprdbat_update_capacity_time = cur_time.tv_sec;
	sprdbat_data->sprdbat_last_query_time = cur_time.tv_sec;
	data->bat_info.capacity = sprdfgu_poweron_capacity();	//~0;
	sprdbat_data->poweron_capacity = sprdfgu_poweron_capacity();
	data->bat_info.soc = sprdfgu_read_soc();
	data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
	data->bat_info.cur_temp = sprdbat_read_temp();
	data->bat_info.bat_current = sprdfgu_read_batcurrent();
	data->bat_info.chging_current = 0;
	data->bat_info.chg_current_type = sprdbat_data->pdata->adp_sdp_cur;

	return;
}

static int sprdbat_start_charge(void)
{
	struct timespec cur_time;
	int cur_temp = sprdbat_read_temp();
	SPRDBAT_DEBUG("sprdbat_start_charge,cur_temp=%d\n",cur_temp);
	if (sprdbat_data->bat_info.chg_stop_flags != SPRDBAT_CHG_END_NONE_BIT) {
		SPRDBAT_DEBUG("can't start charge ,something wrong\n");
		return 0;
	}
	sprd_ext_ic_op->ic_init(sprdbat_data->pdata);

	if (sprdbat_data->bat_info.adp_type == ADP_TYPE_CDP) {
		sprdbat_data->bat_info.chg_current_type =
		    sprdbat_data->pdata->adp_cdp_cur;
	} else if (sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP) {
		sprdbat_data->bat_info.chg_current_type =
		    sprdbat_data->pdata->adp_dcp_cur;
	} else {
		sprdbat_data->bat_info.chg_current_type =
		    sprdbat_data->pdata->adp_sdp_cur;
	}
#ifdef CHG_CUR_JEITA
	sprdbat_data->last_temp_status =
		sprdbat_get_status_from_tab(sprdbat_data->pdata->jeita_tab_size,
								sprdbat_data->pdata->jeita_tab, jeita_debug + 1000);
	SPRDBAT_DEBUG("sprdbat_data->last_temp_status=%d\n",sprdbat_data->last_temp_status);
	sprdbat_current_adjust_by_status(sprdbat_data->last_temp_status);
#else
	sprd_ext_ic_op->charge_start_ext(sprdbat_data->bat_info.chg_current_type);
#endif
	sprdbat_clear_buff(chg_vol_buff,VOL_BUFF_CNT,5000);
	sprdbat_data->bat_info.avg_chg_vol  = 5000;
	sprdbat_data->chg_cur_adjust_min = sprdbat_data->pdata->adp_sdp_cur;
	sprdbat_data->chg_cur_adjust_max = sprdbat_data->pdata->adp_dcp_cur;
	get_monotonic_boottime(&cur_time);
	sprdbat_data->bat_info.chg_start_time = cur_time.tv_sec;

	SPRDBAT_DEBUG
	    ("sprdbat_start_charge health:%d,chg_start_time:%ld,chg_current_type:%d\n",
	     sprdbat_data->bat_info.bat_health,
	     sprdbat_data->bat_info.chg_start_time,
	     sprdbat_data->bat_info.chg_current_type);

	return 0;
}

static int sprdbat_stop_charge(void)
{
	unsigned long irq_flag = 0;
	sprdbat_data->bat_info.chg_start_time = 0;
	sprd_ext_ic_op->charge_stop_ext();
	if((sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_OVP_BIT) == 0){
		sprdbat_clear_buff(chg_vol_buff,VOL_BUFF_CNT,5000);
	}
	SPRDBAT_DEBUG("sprdbat_stop_charge\n");
	power_supply_changed(&sprdbat_data->battery);
	return 0;
}

static inline void _sprdbat_clear_stopflags(uint32_t flag_msk)
{
	sprdbat_data->bat_info.chg_stop_flags &= ~flag_msk;
	SPRDBAT_DEBUG("_sprdbat_clear_stopflags flag_msk:0x%x flag:0x%x\n",
		flag_msk,sprdbat_data->bat_info.chg_stop_flags);

	if (sprdbat_data->bat_info.chg_stop_flags == SPRDBAT_CHG_END_NONE_BIT) {
		sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
		if (flag_msk != SPRDBAT_CHG_END_FULL_BIT
		    && flag_msk != SPRDBAT_CHG_END_TIMEOUT_BIT) {
			sprdbat_data->bat_info.module_state =
			    POWER_SUPPLY_STATUS_CHARGING;
		}
		//sprdbat_start_charge();
		//sprdbat_data->start_charge();
	} else if (sprdbat_data->bat_info.
		   chg_stop_flags & SPRDBAT_CHG_END_UNSPEC) {
		sprdbat_data->bat_info.bat_health =
		    POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;

	} else if (sprdbat_data->bat_info.
		   chg_stop_flags & SPRDBAT_CHG_END_OTP_OVERHEAT_BIT) {
		sprdbat_data->bat_info.bat_health =
		    POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (sprdbat_data->bat_info.
		   chg_stop_flags & SPRDBAT_CHG_END_OTP_COLD_BIT) {
		sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_COLD;
	} else if (sprdbat_data->bat_info.
		   chg_stop_flags & SPRDBAT_CHG_END_OVP_BIT) {
		sprdbat_data->bat_info.bat_health =
		    POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (sprdbat_data->bat_info.
		   chg_stop_flags & (SPRDBAT_CHG_END_TIMEOUT_BIT |
				     SPRDBAT_CHG_END_FULL_BIT)) {
		sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
	}else if(sprdbat_data->bat_info.
		   chg_stop_flags & SPRDBAT_CHG_END_BAT_OVP_BIT){
		sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else {
		BUG_ON(1);
	}
}

static inline void _sprdbat_set_stopflags(uint32_t flag)
{
	SPRDBAT_DEBUG("_sprdbat_set_stopflags flags:0x%x\n", flag);

	sprdbat_data->bat_info.chg_stop_flags |= flag;
	if (sprdbat_data->bat_info.module_state == POWER_SUPPLY_STATUS_CHARGING) {
		if ((flag == SPRDBAT_CHG_END_FULL_BIT)) {
			sprdbat_data->bat_info.module_state =
			    POWER_SUPPLY_STATUS_FULL;
		} else if (flag == SPRDBAT_CHG_END_TIMEOUT_BIT) {
			if (sprdbat_data->pdata->chgtimeout_show_full) {
				sprdbat_data->bat_info.module_state =
				    POWER_SUPPLY_STATUS_FULL;
			}
		} else {
			sprdbat_data->bat_info.module_state =
			    POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
	}
}

static void sprdbat_change_module_state(uint32_t event)
{
	SPRDBAT_DEBUG("sprdbat_change_module_state event :0x%x\n", event);
	switch (event) {
	case SPRDBAT_ADP_PLUGIN_E:
		sprdbat_data->bat_info.chg_this_timeout =
		    sprdbat_data->pdata->chg_timeout;
		_sprdbat_clear_stopflags(~SPRDBAT_CHG_END_UNSPEC);
		if (sprdbat_data->
		    bat_info.chg_stop_flags & SPRDBAT_CHG_END_UNSPEC) {
			sprdbat_data->bat_info.module_state =
			    POWER_SUPPLY_STATUS_NOT_CHARGING;
		}
		queue_delayed_work(sprdbat_data->monitor_wqueue,
				   &sprdbat_data->sprdbat_charge_work, 2 * HZ);
		break;
	case SPRDBAT_ADP_PLUGOUT_E:
		if (sprdbat_data->
		    bat_info.chg_stop_flags & SPRDBAT_CHG_END_UNSPEC) {
			sprdbat_data->bat_info.bat_health =
			    POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		} else {
			sprdbat_data->bat_info.bat_health =
			    POWER_SUPPLY_HEALTH_GOOD;
		}
		sprdbat_data->bat_info.chg_stop_flags &= SPRDBAT_CHG_END_UNSPEC;
		sprdbat_data->bat_info.module_state =
		    POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case SPRDBAT_OVI_STOP_E:
		if (sprdbat_data->bat_info.bat_health ==
		    POWER_SUPPLY_HEALTH_GOOD) {
			sprdbat_data->bat_info.bat_health =
			    POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		}
		_sprdbat_set_stopflags(SPRDBAT_CHG_END_OVP_BIT);
		break;
	case SPRDBAT_OVI_RESTART_E:
		_sprdbat_clear_stopflags(SPRDBAT_CHG_END_OVP_BIT);
		break;
	case SPRDBAT_OTP_COLD_STOP_E:
		if (sprdbat_data->bat_info.bat_health ==
		    POWER_SUPPLY_HEALTH_GOOD) {
			sprdbat_data->bat_info.bat_health =
			    POWER_SUPPLY_HEALTH_COLD;
		}
		_sprdbat_set_stopflags(SPRDBAT_CHG_END_OTP_COLD_BIT);
		break;
	case SPRDBAT_OTP_OVERHEAT_STOP_E:
		if (sprdbat_data->bat_info.bat_health ==
		    POWER_SUPPLY_HEALTH_GOOD) {
			sprdbat_data->bat_info.bat_health =
			    POWER_SUPPLY_HEALTH_OVERHEAT;
		}
		_sprdbat_set_stopflags(SPRDBAT_CHG_END_OTP_OVERHEAT_BIT);
		break;
	case SPRDBAT_OTP_COLD_RESTART_E:
		_sprdbat_clear_stopflags(SPRDBAT_CHG_END_OTP_COLD_BIT);
		break;
	case SPRDBAT_OTP_OVERHEAT_RESTART_E:
		_sprdbat_clear_stopflags(SPRDBAT_CHG_END_OTP_OVERHEAT_BIT);
		break;
	case SPRDBAT_CHG_FULL_E:
		sprdbat_data->bat_info.chg_this_timeout =
		    sprdbat_data->pdata->chg_rechg_timeout;
		_sprdbat_set_stopflags(SPRDBAT_CHG_END_FULL_BIT);
		break;
	case SPRDBAT_RECHARGE_E:
		_sprdbat_clear_stopflags(SPRDBAT_CHG_END_FULL_BIT);
		break;
	case SPRDBAT_CHG_TIMEOUT_E:
		sprdbat_data->bat_info.chg_this_timeout =
		    sprdbat_data->pdata->chg_rechg_timeout;
		_sprdbat_set_stopflags(SPRDBAT_CHG_END_TIMEOUT_BIT);
		break;
	case SPRDBAT_CHG_TIMEOUT_RESTART_E:
		_sprdbat_clear_stopflags(SPRDBAT_CHG_END_TIMEOUT_BIT);
		break;
	case SPRDBAT_CHG_UNSPEC_E:
		if (sprdbat_data->bat_info.bat_health ==
		    POWER_SUPPLY_HEALTH_GOOD) {
			sprdbat_data->bat_info.bat_health =
			    POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		}
		_sprdbat_set_stopflags(SPRDBAT_CHG_END_UNSPEC);
		break;
	case SPRDBAT_VBAT_OVP_E:
		sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		_sprdbat_set_stopflags(SPRDBAT_CHG_END_BAT_OVP_BIT);
		break;
	case SPRDBAT_FULL_TO_CHARGING_E:
		sprdbat_data->bat_info.module_state =
			    POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		BUG_ON(1);
		break;
	}
	power_supply_changed(&sprdbat_data->battery);
}
static int plugin_callback(int usb_cable, void *data)
{
	SPRDBAT_DEBUG("charger plug in interrupt happen\n");

	mutex_lock(&sprdbat_data->lock);
	wake_lock_timeout(&(sprdbat_data->charger_wake_lock),
			  SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);

	sprdbat_data->bat_info.adp_type = sprdchg_charger_is_adapter();
	if (sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP) {
		sprdbat_data->bat_info.ac_online = 1;
	} else {
		sprdbat_data->bat_info.usb_online = 1;
	}
	sprdbat_adp_plug_nodify(1);
	sprdbat_change_module_state(SPRDBAT_ADP_PLUGIN_E);
	msleep(300);
	sprdbat_data->start_charge();
	sprdchg_timer_enable(sprdbat_data->pdata->chg_polling_time);

	mutex_unlock(&sprdbat_data->lock);

	SPRDBAT_DEBUG("plugin_callback:sprdbat_data->bat_info.adp_type:%d\n",
		      sprdbat_data->bat_info.adp_type);

	if (sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP)
		power_supply_changed(&sprdbat_data->ac);
	else
		power_supply_changed(&sprdbat_data->usb);

	SPRDBAT_DEBUG("plugin_callback: end...\n");
	return 0;
}

static int plugout_callback(int usb_cable, void *data)
{
	uint32_t adp_type = sprdbat_data->bat_info.adp_type;

	SPRDBAT_DEBUG("charger plug out interrupt happen\n");

	mutex_lock(&sprdbat_data->lock);

	disable_irq_nosync(sprdbat_data->irq_vchg_ovi);

	sprdchg_timer_disable();
	sprdbat_change_module_state(SPRDBAT_ADP_PLUGOUT_E);
	sprdbat_data->stop_charge();
	wake_lock_timeout(&(sprdbat_data->charger_wake_lock),
			  SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);
	sprdbat_adp_plug_nodify(0);
	sprdbat_data->bat_info.module_state = POWER_SUPPLY_STATUS_DISCHARGING;

	sprdbat_data->bat_info.adp_type = ADP_TYPE_UNKNOW;
	sprdbat_data->bat_info.ac_online = 0;
	sprdbat_data->bat_info.usb_online = 0;

	mutex_unlock(&sprdbat_data->lock);

	if (adp_type == ADP_TYPE_DCP)
		power_supply_changed(&sprdbat_data->ac);
	else
		power_supply_changed(&sprdbat_data->usb);
	return 0;
}


static struct usb_hotplug_callback power_cb = {
	.plugin = plugin_callback,
	.plugout = plugout_callback,
	.data = NULL,
};
static char *supply_list[] = {
	"battery",
};

static char *battery_supply_list[] = {
	"audio-ldo",
	"sprdfgu",
};

static int sprdbat_timer_handler(void *data)
{
	wake_lock_timeout(&(sprdbat_data->charger_wake_lock),
			  1 * HZ);
	sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	sprdbat_data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
	sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
#if 0
	SPRDBAT_DEBUG
	    ("sprdbat_timer_handler---vbat_vol %d,ocv:%d,bat_current:%d\n",
	     sprdbat_data->bat_info.vbat_vol, sprdbat_data->bat_info.vbat_ocv,
	     sprdbat_data->bat_info.bat_current);
#endif
	queue_delayed_work(sprdbat_data->monitor_wqueue,
			   &sprdbat_data->sprdbat_charge_work, msecs_to_jiffies(700));
	return 0;
}
static __used irqreturn_t sprdbat_vbat_detect_irq(int irq, void *dev_id)
{
	disable_irq_nosync(sprdbat_data->irq_vbat_detect);
	SPRDBAT_DEBUG("battery remove!!!!\n");
	queue_work(sprdbat_data->monitor_wqueue,
		   &sprdbat_data->vbat_detect_irq_work);
	return IRQ_HANDLED;
}

static void sprdbat_vbat_detect_irq_works(struct work_struct *work)
{
	int value;

	value = gpio_get_value(sprdbat_data->gpio_vbat_detect);
	SPRDBAT_DEBUG("bat_detect value:%d\n", value);
	mdelay(500);
	mutex_lock(&sprdbat_data->lock);

	sprdbat_change_module_state(SPRDBAT_CHG_UNSPEC_E);
	sprdbat_data->stop_charge();
	mutex_unlock(&sprdbat_data->lock);
}
static void sprdbat_chg_rechg_monitor(void)
{
	int chg_status = POWER_SUPPLY_STATUS_CHARGING;
	SPRDBAT_DEBUG("sprdbat_chg_rechg_monitor\n");
	if(sprdbat_data->pdata->chg_full_condition == FROM_EXT_IC){
		chg_status = sprd_ext_ic_op->get_charging_status();
		if(chg_status == POWER_SUPPLY_STATUS_FULL){
			SPRDBAT_DEBUG("chg full status  ,need rechg??\n");
			if(sprdbat_data->bat_info.vbat_ocv <= sprdbat_data->pdata->rechg_vol){
				SPRDBAT_DEBUG("yes,now recharge\n");
				sprdbat_change_module_state(SPRDBAT_RECHARGE_E);
				sprdbat_data->start_charge();
			}else{
				SPRDBAT_DEBUG("no,don't need recharge\n");
			}
		}else if(chg_status == POWER_SUPPLY_STATUS_CHARGING){
			SPRDBAT_DEBUG("chg restart by status charging\n");
			sprdbat_change_module_state(SPRDBAT_RECHARGE_E);
		}else{
			SPRDBAT_DEBUG("some faults\n");
		}
	}else if(sprdbat_data->pdata->chg_full_condition == VOL_AND_CUR){
		SPRDBAT_DEBUG("chg stop_flag full  ,need rechg??\n");
		if(sprdbat_data->bat_info.vbat_ocv <= sprdbat_data->pdata->rechg_vol){
					SPRDBAT_DEBUG("yes, chg restart\n");
					sprdbat_change_module_state(SPRDBAT_RECHARGE_E);
					sprdbat_data->start_charge();
		}else{
			SPRDBAT_DEBUG("no,don't need recharge\n");
		}
	}else{
			//need add
	}
}

static void sprdbat_chg_status_monitor(void)
{
	int chg_status = POWER_SUPPLY_STATUS_CHARGING;
	SPRDBAT_DEBUG("sprdbat_chg_status_monitor enter,ocv=%d,cur=%d,chg_end_vol_l=%d,chg_end_cur=%d\n",
		sprdbat_data->bat_info.vbat_ocv,sprdbat_data->bat_info.bat_current,
		sprdbat_data->pdata->chg_end_vol_l,sprdbat_data->pdata->chg_end_cur);
	if(sprdbat_data->pdata->chg_full_condition == FROM_EXT_IC){
		chg_status = sprd_ext_ic_op->get_charging_status();
		if(chg_status == POWER_SUPPLY_STATUS_FULL){
			SPRDBAT_DEBUG("chg full\n");
			sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
		}else{
			SPRDBAT_DEBUG("chging or fault\n");
		}
	}else if(sprdbat_data->pdata->chg_full_condition == VOL_AND_CUR){
		if ((sprdbat_data->bat_info.vbat_vol >sprdbat_data->pdata->chg_end_vol_l)
			&& ( sprdbat_data->bat_info.bat_current < sprdbat_data->pdata->chg_end_cur)) {
				sprdbat_data->chg_full_trigger_cnt++;
				if(sprdbat_data->chg_full_trigger_cnt == 2){
			    		SPRDBAT_DEBUG("charge full stop charge\n");
					sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
					sprdbat_data->stop_charge();
				}
		}else{
			sprdbat_data->chg_full_trigger_cnt = 0;
		}
	}else if(sprdbat_data->pdata->chg_full_condition == VOL_AND_STATUS){
		// need add
	}else{
		SPRDBAT_DEBUG("bad chg_full_condition\n");
	}
}
static void sprdbat_fault_recovery_monitor(void)
{
	SPRDBAT_DEBUG("sprdbat_fault_recovery_monitor enter\n");
	if(sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_OTP_COLD_BIT){
		SPRDBAT_DEBUG("chg  not cold  now is chging\n");
		sprdbat_change_module_state(SPRDBAT_OTP_COLD_RESTART_E);
	}
	if(sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_OTP_OVERHEAT_BIT){
		SPRDBAT_DEBUG("chg  not hot now is  chging\n");
		sprdbat_change_module_state(SPRDBAT_OTP_OVERHEAT_RESTART_E);
	}
	if(sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_BAT_OVP_BIT){
		SPRDBAT_DEBUG("chg  not vbat ovp now is  chging\n");
		sprdbat_change_module_state(SPRDBAT_VBAT_OVP_E);
	}
}

static void sprdbat_fault_monitor(void)
{
	int chg_fault = 0;
	SPRDBAT_DEBUG("sprdbat_fault_monitor enter\n");
	chg_fault = sprd_ext_ic_op->get_charging_fault();

	if(chg_fault == SPRDBAT_CHG_END_NONE_BIT){
		sprdbat_fault_recovery_monitor();
	}
	if(chg_fault & SPRDBAT_CHG_END_OTP_COLD_BIT){
		SPRDBAT_DEBUG(" power cold\n");
		sprdbat_change_module_state(SPRDBAT_OTP_COLD_STOP_E);
	}
	if(chg_fault & SPRDBAT_CHG_END_OTP_OVERHEAT_BIT){
		SPRDBAT_DEBUG("power hot\n");
		sprdbat_change_module_state(SPRDBAT_OTP_OVERHEAT_STOP_E);
	}
	if(chg_fault & SPRDBAT_CHG_END_TIMEOUT_BIT){
		SPRDBAT_DEBUG("  safe time expire \n");
		sprdbat_change_module_state(SPRDBAT_CHG_TIMEOUT_E);
	}
	if(chg_fault & SPRDBAT_CHG_END_BAT_OVP_BIT){
		SPRDBAT_DEBUG(" vbat ovp\n");
		sprdbat_change_module_state(SPRDBAT_VBAT_OVP_E);
	}
	//ignore other fault 
	if(chg_fault == SPRDBAT_CHG_END_UNSPEC){
		SPRDBAT_DEBUG(" unspec fault\n");
	}
}

static int sprdbat_charge_fault_event(struct notifier_block *this, unsigned long event,
			     void *ptr)
{
	int chg_fault = 0;
	mutex_lock(&sprdbat_data->lock);
	SPRDBAT_DEBUG("enter sprdbat_charge_fault_event\n");
	chg_fault = sprd_ext_ic_op->get_charging_fault();
	
	if(chg_fault == SPRDBAT_CHG_END_NONE_BIT){
		sprdbat_fault_recovery_monitor();
	}
	if(chg_fault & SPRDBAT_CHG_END_OTP_COLD_BIT){
		SPRDBAT_DEBUG(" power cold\n");
		sprdbat_change_module_state(SPRDBAT_OTP_COLD_STOP_E);
	}
	if(chg_fault & SPRDBAT_CHG_END_OTP_OVERHEAT_BIT){
		SPRDBAT_DEBUG("power hot\n");
		sprdbat_change_module_state(SPRDBAT_OTP_OVERHEAT_STOP_E);
	}
	if(chg_fault & SPRDBAT_CHG_END_TIMEOUT_BIT){
		SPRDBAT_DEBUG("  safe time expire \n");
		sprdbat_change_module_state(SPRDBAT_CHG_TIMEOUT_E);
	}
	if(chg_fault & SPRDBAT_CHG_END_BAT_OVP_BIT){
		SPRDBAT_DEBUG(" vbat ovp\n");
		sprdbat_change_module_state(SPRDBAT_VBAT_OVP_E);
	}
	//ignore other fault 
	if(chg_fault == SPRDBAT_CHG_END_UNSPEC){
		SPRDBAT_DEBUG(" unspec fault\n");
	}
	mutex_unlock(&sprdbat_data->lock);
}
static void sprdbat_chg_print_log(void)
{
	struct timespec cur_time;

	get_monotonic_boottime(&cur_time);

	//SPRDBAT_DEBUG("adp_type:%d\n", sprdbat_data->bat_info.adp_type);
	SPRDBAT_DEBUG
	    ("chg_log:time:%ld,health:%d,state:%d,stopflags0x:%x,chg_s_time:%ld,temp:%d\n",
	     cur_time.tv_sec, sprdbat_data->bat_info.bat_health,
	     sprdbat_data->bat_info.module_state,
	     sprdbat_data->bat_info.chg_stop_flags,
	     sprdbat_data->bat_info.chg_start_time,
	     sprdbat_data->bat_info.cur_temp);

	//SPRDBAT_DEBUG("capacity:%d\n", sprdbat_data->bat_info.capacity);
	//SPRDBAT_DEBUG("soc:%d\n", sprdbat_data->bat_info.soc);
	//SPRDBAT_DEBUG("vbat_vol:%d\n", sprdbat_data->bat_info.vbat_vol);
	//SPRDBAT_DEBUG("vbat_ocv:%d\n", sprdbat_data->bat_info.vbat_ocv);
	//SPRDBAT_DEBUG("cur_temp:%d\n", sprdbat_data->bat_info.cur_temp);
	//SPRDBAT_DEBUG("bat_current:%d\n", sprdbat_data->bat_info.bat_current);
	//SPRDBAT_DEBUG("chging_current:%d\n",
	//            sprdbat_data->bat_info.chging_current);
	SPRDBAT_DEBUG
	    ("chg_log:chgcur_type:%d,vchg:%d\n",
	     sprdbat_data->bat_info.chg_current_type,
	      sprdchg_read_vchg_vol());

	//SPRDBAT_DEBUG("aux vbat vol:%d\n", sprdchg_read_vbat_vol());

}

static void sprdbat_print_battery_log(void)
{
	struct timespec cur_time;

	get_monotonic_boottime(&cur_time);

	SPRDBAT_DEBUG
	    ("bat_log:time:%ld,vbat:%d,ocv:%d,current:%d,cap:%d,state:%d,auxbat:%d,temp:%d\n",
	     cur_time.tv_sec, sprdbat_data->bat_info.vbat_vol,
	     sprdbat_data->bat_info.vbat_ocv,
	     sprdbat_data->bat_info.bat_current,
	     sprdbat_data->bat_info.capacity,
	     sprdbat_data->bat_info.module_state, sprdchg_read_vbat_vol(), sprdbat_data->bat_info.cur_temp);
}

#define SPRDBAT_SHUTDOWN_OFSSET 50
static void sprdbat_update_capacty(void)
{
	uint32_t fgu_capacity = sprdfgu_read_capacity();
	uint32_t flush_time = 0;
	uint32_t period_time = 0;
	struct timespec cur_time;
	if (sprdbat_data->bat_info.capacity == ~0) {
		return;
	}
	get_monotonic_boottime(&cur_time);
	flush_time = cur_time.tv_sec - sprdbat_data->sprdbat_update_capacity_time;
	period_time = cur_time.tv_sec - sprdbat_data->sprdbat_last_query_time;
	sprdbat_data->sprdbat_last_query_time = cur_time.tv_sec;

	SPRDBAT_DEBUG("fgu_cap: = %d,flush: = %d,period:=%d\n",
		      fgu_capacity, flush_time, period_time);

	switch (sprdbat_data->bat_info.module_state) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (fgu_capacity < sprdbat_data->bat_info.capacity) {
			if (sprdfgu_read_batcurrent() >= 0) {
				pr_info("avoid vol jumping\n");
				fgu_capacity = sprdbat_data->bat_info.capacity;
			} else {
				if (period_time <
				    sprdbat_data->pdata->cap_one_per_time) {
					fgu_capacity =
					    sprdbat_data->bat_info.capacity - 1;
					SPRDBAT_DEBUG
					    ("cap decrease when charging fgu_capacity: = %d\n",
					     fgu_capacity);
				}
				if ((sprdbat_data->bat_info.capacity -
				     fgu_capacity) >=
				    (flush_time /
				     sprdbat_data->pdata->cap_one_per_time)) {
					fgu_capacity =
					    sprdbat_data->bat_info.capacity -
					    flush_time /
					    sprdbat_data->
					    pdata->cap_one_per_time;
				}

			}
		} else if (fgu_capacity > sprdbat_data->bat_info.capacity) {
			if (period_time < sprdbat_data->pdata->cap_one_per_time) {
				fgu_capacity =
				    sprdbat_data->bat_info.capacity + 1;
				SPRDBAT_DEBUG
				    ("avoid  jumping! fgu_cap: = %d\n",
				     fgu_capacity);
			}
			if ((fgu_capacity - sprdbat_data->bat_info.capacity) >=
			    (flush_time /
			     sprdbat_data->pdata->cap_one_per_time)) {
				fgu_capacity =
				    sprdbat_data->bat_info.capacity +
				    flush_time /
				    sprdbat_data->pdata->cap_one_per_time;
			}
		}
		/*when soc=100 and adp plugin occur, keep on 100, */
		if (100 == sprdbat_data->bat_info.capacity) {
			//sprdbat_data->sprdbat_update_capacity_time = cur_time.tv_sec;
			//fgu_capacity = 100;
		} else {
			if (fgu_capacity >= 100) {
				fgu_capacity = 99;
			}
		}

		if (sprdbat_data->bat_info.vbat_vol <=
		    (sprdbat_data->pdata->soft_vbat_uvlo - SPRDBAT_SHUTDOWN_OFSSET)) {
			fgu_capacity = 0;
			SPRDBAT_DEBUG("soft uvlo, shutdown by kernel.. vol:%d",
				      sprdbat_data->bat_info.vbat_vol);
			orderly_poweroff(true);
		}

		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
		if (fgu_capacity >= sprdbat_data->bat_info.capacity) {
			fgu_capacity = sprdbat_data->bat_info.capacity;
		} else {
			if (period_time < sprdbat_data->pdata->cap_one_per_time) {
				fgu_capacity =
				    sprdbat_data->bat_info.capacity - 1;
				SPRDBAT_DEBUG
				    ("avoid jumping! fgu_capacity: = %d\n",
				     fgu_capacity);
			}
			if ((sprdbat_data->bat_info.capacity - fgu_capacity) >=
			    (flush_time /
			     sprdbat_data->pdata->cap_one_per_time)) {
				fgu_capacity =
				    sprdbat_data->bat_info.capacity -
				    flush_time /
				    sprdbat_data->pdata->cap_one_per_time;
			}
		}
		break;
	case POWER_SUPPLY_STATUS_FULL:
		sprdbat_data->sprdbat_update_capacity_time = cur_time.tv_sec;
		if((sprdbat_data->bat_info.vbat_ocv < (sprdbat_data->pdata->rechg_vol -150))
				&& sprdfgu_read_batcurrent() < 0) {
			SPRDBAT_DEBUG("vbat_ocv < rechg_vol -150\n");
			sprdbat_change_module_state(SPRDBAT_FULL_TO_CHARGING_E);
		}
		if (fgu_capacity != 100) {
			fgu_capacity = 100;
		}

		if (sprdbat_data->bat_info.vbat_vol <=
		    (sprdbat_data->pdata->soft_vbat_uvlo - SPRDBAT_SHUTDOWN_OFSSET)) {
			fgu_capacity = 0;
			SPRDBAT_DEBUG("soft uvlo, shutdown by kernel when status is full.. vol:%d",
				      sprdbat_data->bat_info.vbat_vol);
			orderly_poweroff(true);
		}

		break;
	default:
		BUG_ON(1);
		break;
	}

	if (sprdbat_data->bat_info.vbat_vol <=
	    sprdbat_data->pdata->soft_vbat_uvlo) {
		fgu_capacity = 0;
		SPRDBAT_DEBUG("soft uvlo, vbat very low,level..0.. vol:%d",
			      sprdbat_data->bat_info.vbat_vol);
	}

	if (fgu_capacity != sprdbat_data->bat_info.capacity) {
		sprdbat_data->bat_info.capacity = fgu_capacity;
		sprdbat_data->sprdbat_update_capacity_time = cur_time.tv_sec;
		sprdfgu_record_cap(sprdbat_data->bat_info.capacity);
		power_supply_changed(&sprdbat_data->battery);
	}

}

static void sprdbat_battery_works(struct work_struct *work)
{
	SPRDBAT_DEBUG("sprdbat_battery_works\n");

	mutex_lock(&sprdbat_data->lock);

	sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	sprdbat_data->bat_info.cur_temp = sprdbat_read_temp();
	sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
	sprdbat_data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
	sprdbat_data->bat_info.vchg_vol = sprdchg_read_vchg_vol();

	sprdbat_data->bat_info.avg_chg_vol  =
		sprdbat_get_avgval_from_buff(sprdbat_data->bat_info.vchg_vol,chg_vol_buff,VOL_BUFF_CNT,0);
	sprdbat_data->bat_info.bat_current_avg  =
		sprdbat_get_avgval_from_buff(sprdbat_data->bat_info.bat_current,current_buff,CUR_BUFF_CNT,1);

	sprdbat_update_capacty();

	mutex_unlock(&sprdbat_data->lock);
	sprdbat_print_battery_log();


	if (sprdbat_data->bat_info.module_state == POWER_SUPPLY_STATUS_CHARGING) {
		schedule_delayed_work(&sprdbat_data->battery_work,
				      sprdbat_data->pdata->
				      bat_polling_time_fast * HZ);
	} else {
		schedule_delayed_work(&sprdbat_data->battery_work,
				      sprdbat_data->pdata->bat_polling_time *
				      HZ);
	}

}

static void sprdbat_battery_sleep_works(struct work_struct *work)
{
	SPRDBAT_DEBUG("sprdbat_battery_sleep_works\n");
	if (schedule_delayed_work(&sprdbat_data->battery_work, 0) == 0) {
		cancel_delayed_work_sync(&sprdbat_data->battery_work);
		schedule_delayed_work(&sprdbat_data->battery_work, 0);
	}
}


static void sprdbat_charge_works(struct work_struct *work)
{
	SPRDBAT_DEBUG("sprdbat_charge_works----------start\n");
	int ret = 0, is_chging = 0;
	mutex_lock(&sprdbat_data->lock);
	sprd_ext_ic_op->timer_callback_ext();
	if (sprdbat_data->bat_info.module_state ==
	    POWER_SUPPLY_STATUS_DISCHARGING) {
	    	SPRDBAT_DEBUG("not charing return \n");
		mutex_unlock(&sprdbat_data->lock);
		return;
	}
	if(sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_FULL_BIT){
		sprdbat_chg_rechg_monitor();
	}
#ifdef CHG_CUR_ADJUST
	if (sprdbat_data->bat_info.chg_stop_flags == SPRDBAT_CHG_END_NONE_BIT) 
		sprdbat_auto_switch_cur();
#endif
	sprdbat_chg_status_monitor();
	sprdbat_chg_timeout_monitor();
	sprdbat_chg_ovp_monitor();
#ifdef CHG_CUR_JEITA
	sprdbat_temp_monitor();
#endif
	sprdbat_fault_monitor();
	mutex_unlock(&sprdbat_data->lock);

	sprdbat_chg_print_log();
	SPRDBAT_DEBUG("sprdbat_charge_works----------end\n");

}
static void print_pdata(struct sprd_battery_platform_data *pdata)
{
#define PDATA_LOG(format, arg...) printk("sprdbat pdata: " format, ## arg)
	int i;

	PDATA_LOG("chg_end_vol_h:%d\n", pdata->chg_end_vol_h);
	PDATA_LOG("chg_end_vol_l:%d\n", pdata->chg_end_vol_l);
	PDATA_LOG("chg_end_vol_pure:%d\n", pdata->chg_end_vol_pure);
	PDATA_LOG("chg_bat_safety_vol:%d\n", pdata->chg_bat_safety_vol);
	PDATA_LOG("rechg_vol:%d\n", pdata->rechg_vol);
	PDATA_LOG("adp_cdp_cur:%d\n", pdata->adp_cdp_cur);
	PDATA_LOG("adp_dcp_cur:%d\n", pdata->adp_dcp_cur);
	PDATA_LOG("adp_sdp_cur:%d\n", pdata->adp_sdp_cur);
	PDATA_LOG("ovp_stop:%d\n", pdata->ovp_stop);
	PDATA_LOG("ovp_restart:%d\n", pdata->ovp_restart);
	PDATA_LOG("chg_timeout:%d\n", pdata->chg_timeout);
	PDATA_LOG("chgtimeout_show_full:%d\n", pdata->chgtimeout_show_full);
	PDATA_LOG("chg_rechg_timeout:%d\n", pdata->chg_rechg_timeout);
	PDATA_LOG("chg_cv_timeout:%d\n", pdata->chg_cv_timeout);
	PDATA_LOG("chg_eoc_level:%d\n", pdata->chg_eoc_level);
	PDATA_LOG("cccv_default:%d\n", pdata->cccv_default);
	PDATA_LOG("chg_end_cur:%d\n", pdata->chg_end_cur);
	PDATA_LOG("otp_high_stop:%d\n", pdata->otp_high_stop);
	PDATA_LOG("otp_high_restart:%d\n", pdata->otp_high_restart);
	PDATA_LOG("otp_low_stop:%d\n", pdata->otp_low_stop);
	PDATA_LOG("otp_low_restart:%d\n", pdata->otp_low_restart);
	PDATA_LOG("chg_polling_time:%d\n", pdata->chg_polling_time);
	PDATA_LOG("chg_polling_time_fast:%d\n", pdata->chg_polling_time_fast);
	PDATA_LOG("bat_polling_time:%d\n", pdata->bat_polling_time);
	PDATA_LOG("bat_polling_time_fast:%d\n", pdata->bat_polling_time_fast);
	PDATA_LOG("bat_polling_time_sleep:%d\n", pdata->bat_polling_time_sleep);
	PDATA_LOG("cap_one_per_time:%d\n", pdata->cap_one_per_time);
	PDATA_LOG("cap_one_per_time_fast:%d\n", pdata->cap_one_per_time_fast);
	PDATA_LOG("cap_valid_range_poweron:%d\n",
		pdata->cap_valid_range_poweron);
	PDATA_LOG("temp_support:%d\n", pdata->temp_support);
	PDATA_LOG("temp_adc_ch:%d\n", pdata->temp_adc_ch);
	PDATA_LOG("temp_adc_scale:%d\n", pdata->temp_adc_scale);
	PDATA_LOG("temp_adc_sample_cnt:%d\n", pdata->temp_adc_sample_cnt);
	PDATA_LOG("temp_table_mode:%d\n", pdata->temp_table_mode);
	PDATA_LOG("temp_comp_res:%d\n", pdata->temp_comp_res);
	PDATA_LOG("temp_tab_size:%d\n", pdata->temp_tab_size);
	PDATA_LOG("gpio_vchg_detect:%d\n", pdata->gpio_vchg_detect);
	PDATA_LOG("gpio_cv_state:%d\n", pdata->gpio_cv_state);
	PDATA_LOG("gpio_vchg_ovi:%d\n", pdata->gpio_vchg_ovi);
	PDATA_LOG("irq_chg_timer:%d\n", pdata->irq_chg_timer);
	PDATA_LOG("irq_fgu:%d\n", pdata->irq_fgu);
	PDATA_LOG("chg_reg_base:%d\n", pdata->chg_reg_base);
	PDATA_LOG("fgu_reg_base:%d\n", pdata->fgu_reg_base);
	PDATA_LOG("fgu_mode:%d\n", pdata->fgu_mode);
	PDATA_LOG("alm_soc:%d\n", pdata->alm_soc);
	PDATA_LOG("alm_vol:%d\n", pdata->alm_vol);
	PDATA_LOG("soft_vbat_uvlo:%d\n", pdata->soft_vbat_uvlo);
	PDATA_LOG("soft_vbat_ovp:%d\n", pdata->soft_vbat_ovp);
	PDATA_LOG("rint:%d\n", pdata->rint);
	PDATA_LOG("cnom:%d\n", pdata->cnom);
	PDATA_LOG("rsense_real:%d\n", pdata->rsense_real);
	PDATA_LOG("rsense_spec:%d\n", pdata->rsense_spec);
	PDATA_LOG("relax_current:%d\n", pdata->relax_current);
	PDATA_LOG("fgu_cal_ajust:%d\n", pdata->fgu_cal_ajust);
	PDATA_LOG("qmax_update_period:%d\n", pdata->qmax_update_period);
	PDATA_LOG("ocv_tab_size:%d\n", pdata->ocv_tab_size);
	PDATA_LOG("jeita_tab_size:%d\n", pdata->jeita_tab_size);
	for (i = 0; i < pdata->ocv_tab_size; i++) {
		PDATA_LOG("ocv_tab i=%d x:%d,y:%d\n", i, pdata->ocv_tab[i].x,
			  pdata->ocv_tab[i].y);
	}
	for (i = 0; i < pdata->temp_tab_size; i++) {
		PDATA_LOG("temp_tab_size i=%d x:%d,y:%d\n", i,
			  pdata->temp_tab[i].x, pdata->temp_tab[i].y);
	}

	for (i = 0; i < pdata->cnom_temp_tab_size; i++) {
		PDATA_LOG("cnom_temp_tab i=%d x:%d,y:%d\n", i, pdata->cnom_temp_tab[i].x,
			  pdata->cnom_temp_tab[i].y);
	}
	for (i = 0; i < pdata->rint_temp_tab_size; i++) {
		PDATA_LOG("rint_temp_tab i=%d x:%d,y:%d\n", i, pdata->rint_temp_tab[i].x,
			  pdata->rint_temp_tab[i].y);
	}
	for (i = 0; i < pdata->jeita_tab_size; i++) {
		PDATA_LOG("jeita_tab i=%d x:%d,y:%d:z:%d\n", i, pdata->jeita_tab[i].x,
			  pdata->jeita_tab[i].y,pdata->jeita_tab[i].z);
	}
}

#ifdef CONFIG_OF
static struct sprd_battery_platform_data *sprdbat_parse_dt(struct
							   platform_device
							   *pdev)
{
	struct sprd_battery_platform_data *pdata = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *temp_np = NULL;
	unsigned int irq_num;
	int ret, i, temp,len;

	pdata = devm_kzalloc(&pdev->dev,
			     sizeof(struct sprd_battery_platform_data),
			     GFP_KERNEL);
	if (!pdata) {
		return NULL;
	}

	pdata->gpio_vchg_detect = (uint32_t) of_get_named_gpio(np, "gpios", 0);
	pdata->gpio_cv_state = (uint32_t) of_get_named_gpio(np, "gpios", 1);
	pdata->gpio_vchg_ovi = (uint32_t) of_get_named_gpio(np, "gpios", 2);

	temp = (uint32_t) of_get_named_gpio(np, "gpios", 3);
	if (gpio_is_valid(temp)) {
		pdata->gpio_vbat_detect = (uint32_t) temp;
		printk("sprdbat:gpio_vbat_detect support =%d\n",
		       pdata->gpio_vbat_detect);
	} else {
		pdata->gpio_vbat_detect = 0;
		printk("sprdbat:gpio_vbat_detect do Not support =%d\n",
		       pdata->gpio_vbat_detect);
	}

	temp_np = of_get_child_by_name(np, "sprd_chg");
	if (!temp_np) {
		goto err_parse_dt;
	}
	irq_num = irq_of_parse_and_map(temp_np, 0);
	pdata->irq_chg_timer = irq_num;
	printk("sprd_chg dts irq_num =%s, %d\n", temp_np->name, irq_num);

	temp_np = of_get_child_by_name(np, "sprd_fgu");
	if (!temp_np) {
		goto err_parse_dt;
	}
	irq_num = irq_of_parse_and_map(temp_np, 0);
	pdata->irq_fgu = irq_num;
	printk("sprd_fgu dts irq_num = %d\n", irq_num);

	ret = of_property_read_u32(np, "chg-end-vol-h", &pdata->chg_end_vol_h);
	ret = of_property_read_u32(np, "chg-end-vol-l", &pdata->chg_end_vol_l);
	ret =
	    of_property_read_u32(np, "chg-end-vol-pure",
				 &pdata->chg_end_vol_pure);
	ret =
	    of_property_read_u32(np, "chg-bat-safety-vol",
				 &pdata->chg_bat_safety_vol);
	ret = of_property_read_u32(np, "rechg-vol", &pdata->rechg_vol);
	ret = of_property_read_u32(np, "adp-cdp-cur", &pdata->adp_cdp_cur);
	ret = of_property_read_u32(np, "adp-dcp-cur", &pdata->adp_dcp_cur);
	ret = of_property_read_u32(np, "adp-sdp-cur", &pdata->adp_sdp_cur);
	ret = of_property_read_u32(np, "ovp-stop", &pdata->ovp_stop);
	ret = of_property_read_u32(np, "ovp-restart", &pdata->ovp_restart);
	ret = of_property_read_u32(np, "chg-timeout", &pdata->chg_timeout);
	ret =
	    of_property_read_u32(np, "chgtimeout-show-full",
				 &pdata->chgtimeout_show_full);
	ret =
	    of_property_read_u32(np, "chg-rechg-timeout",
				 &pdata->chg_rechg_timeout);
	ret =
	    of_property_read_u32(np, "chg-cv-timeout", &pdata->chg_cv_timeout);
	ret = of_property_read_u32(np, "chg-eoc-level", &pdata->chg_eoc_level);
	ret = of_property_read_u32(np, "cccv-default", &pdata->cccv_default);
	ret =
	    of_property_read_u32(np, "chg-end-cur",
				 (u32 *) (&pdata->chg_end_cur));
	ret = of_property_read_u32(np, "otp-high-stop", (u32 *) (&temp));
	pdata->otp_high_stop = temp - 1000;
	ret = of_property_read_u32(np, "otp-high-restart", (u32 *) (&temp));
	pdata->otp_high_restart = temp - 1000;
	ret = of_property_read_u32(np, "otp-low-stop", (u32 *) (&temp));
	pdata->otp_low_stop = temp - 1000;
	ret = of_property_read_u32(np, "otp-low-restart", (u32 *) (&temp));
	pdata->otp_low_restart = temp - 1000;
	ret =
	    of_property_read_u32(np, "chg-polling-time",
				 &pdata->chg_polling_time);
	ret =
	    of_property_read_u32(np, "chg-polling-time-fast",
				 &pdata->chg_polling_time_fast);
	ret =
	    of_property_read_u32(np, "bat-polling-time",
				 &pdata->bat_polling_time);
	ret =
	    of_property_read_u32(np, "bat-polling-time-fast",
				 &pdata->bat_polling_time_fast);
	ret =
	    of_property_read_u32(np, "cap-one-per-time",
				 &pdata->cap_one_per_time);
	ret =
	    of_property_read_u32(np, "cap-valid-range-poweron",
				 (u32 *) (&pdata->cap_valid_range_poweron));
	ret =
	    of_property_read_u32(np, "temp-support",
				 (u32 *) (&pdata->temp_support));
	ret =
	    of_property_read_u32(np, "temp-adc-ch",
				 (u32 *) (&pdata->temp_adc_ch));
	ret =
	    of_property_read_u32(np, "temp-adc-scale",
				 (u32 *) (&pdata->temp_adc_scale));
	ret =
	    of_property_read_u32(np, "temp-adc-sample-cnt",
				 (u32 *) (&pdata->temp_adc_sample_cnt));
	ret =
	    of_property_read_u32(np, "temp-table-mode",
				 (u32 *) (&pdata->temp_table_mode));
	ret =
	    of_property_read_u32(np, "temp-comp-res",
				 (u32 *) (&pdata->temp_comp_res));

	ret = of_property_read_u32(np, "fgu-mode", &pdata->fgu_mode);
	ret = of_property_read_u32(np, "ocv-type", &pdata->ocv_type);
	ret = of_property_read_u32(np, "chg-full-condition", &pdata->chg_full_condition);

	ret = of_property_read_u32(np, "alm-soc", &pdata->alm_soc);
	ret = of_property_read_u32(np, "alm-vol", &pdata->alm_vol);
	ret =
	    of_property_read_u32(np, "soft-vbat-uvlo", &pdata->soft_vbat_uvlo);
	ret = of_property_read_u32(np, "rint", (u32 *) (&pdata->rint));
	ret = of_property_read_u32(np, "cnom", (u32 *) (&pdata->cnom));
	ret =
	    of_property_read_u32(np, "rsense-real",
				 (u32 *) (&pdata->rsense_real));
	ret =
	    of_property_read_u32(np, "rsense-spec",
				 (u32 *) (&pdata->rsense_spec));
	ret = of_property_read_u32(np, "relax-current", &pdata->relax_current);
	ret =
	    of_property_read_u32(np, "fgu-cal-ajust",
				 (u32 *) (&pdata->fgu_cal_ajust));
	ret =
	    of_property_read_u32(np, "ocv-tab-size",
				 (u32 *) (&pdata->ocv_tab_size));
	ret =
	    of_property_read_u32(np, "temp-tab-size",
				 (u32 *) (&pdata->temp_tab_size));
	ret =
	    of_property_read_u32(np, "jeita-tab-size",
				 (u32 *) (&pdata->jeita_tab_size));

	pdata->temp_tab = kzalloc(sizeof(struct sprdbat_table_data) *
				  pdata->temp_tab_size, GFP_KERNEL);

	for (i = 0; i < pdata->temp_tab_size; i++) {
		ret = of_property_read_u32_index(np, "temp-tab-val", i,
						 &pdata->temp_tab[i].x);
		ret = of_property_read_u32_index(np, "temp-tab-temp", i, &temp);
		pdata->temp_tab[i].y = temp - 1000;
	}

	pdata->ocv_tab = kzalloc(sizeof(struct sprdbat_table_data) *
				 pdata->ocv_tab_size, GFP_KERNEL);
	for (i = 0; i < pdata->ocv_tab_size; i++) {
		ret = of_property_read_u32_index(np, "ocv-tab-vol", i,
						 (u32 *) (&pdata->ocv_tab[i].
							  x));
		ret =
		    of_property_read_u32_index(np, "ocv-tab-cap", i,
					       (u32 *) (&pdata->ocv_tab[i].y));
	}

	pdata->jeita_tab = kzalloc(sizeof(struct sprdbat_jeita_table_data) *
				 pdata->jeita_tab_size, GFP_KERNEL);
	for (i = 0; i < pdata->jeita_tab_size; i++) {
		ret = of_property_read_u32_index(np, "jeita-temp-tab", i,
						 (u32 *) (&pdata->jeita_tab[i].
							  x));
		ret =
		    of_property_read_u32_index(np, "jeita-cur-tab", i,
					       (u32 *) (&pdata->jeita_tab[i].y));
		ret =
		    of_property_read_u32_index(np, "jeita-cccv-tab", i,
					       (u32 *) (&pdata->jeita_tab[i].z));
	}

	of_get_property(np, "cnom-temp-tab", &len);
	len /= sizeof(u32);
	pdata->cnom_temp_tab_size = len >> 1;
	pdata->cnom_temp_tab = kzalloc(sizeof(struct sprdbat_table_data) *
			 pdata->cnom_temp_tab_size, GFP_KERNEL);
	for (i = 0; i < len; i++) {
		of_property_read_u32_index(np, "cnom-temp-tab", i, &temp);
		if(i&0x1) {
				pdata->cnom_temp_tab[i >> 1].y = temp;
		} else {
				pdata->cnom_temp_tab[i >> 1].x = temp - 1000;
		}
	}

	of_get_property(np, "rint-temp-tab", &len);
	len /= sizeof(u32);
	pdata->rint_temp_tab_size = len >> 1;
	pdata->rint_temp_tab = kzalloc(sizeof(struct sprdbat_table_data) *
			 pdata->rint_temp_tab_size, GFP_KERNEL);
	for (i = 0; i < len; i++) {
		of_property_read_u32_index(np, "rint-temp-tab", i, &temp);
		if(i&0x1) {
			pdata->rint_temp_tab[i >> 1].y = temp;
		} else {
			pdata->rint_temp_tab[i >> 1].x = temp - 1000;
		}
	}

	return pdata;

err_parse_dt:
	dev_err(&pdev->dev, "Parsing device tree data error.\n");
	return NULL;
}
#else
static struct sprd_battery_platform_data *sprdbat_parse_dt(struct
							   platform_device
							   *pdev)
{
	return NULL;
}
#endif
int __weak usb_register_hotplug_callback(struct usb_hotplug_callback *p){}

static int sprdbat_probe(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct sprdbat_drivier_data *data;
#ifdef SPRDBAT_TWO_CHARGE_CHANNEL
	struct resource *res = NULL;
#endif
	struct device_node *np = pdev->dev.of_node;

	SPRDBAT_DEBUG("sprdbat_probe\n");

#ifdef CONFIG_OF
	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}
#endif
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err_data_alloc_failed;
	}

	if (np) {
		data->pdata = sprdbat_parse_dt(pdev);
	} else {
		data->pdata = dev_get_platdata(&pdev->dev);
	}

	data->dev = &pdev->dev;
	platform_set_drvdata(pdev, data);
	sprdbat_data = data;

	print_pdata(sprdbat_data->pdata);

	data->battery.properties = sprdbat_battery_props;
	data->battery.num_properties = ARRAY_SIZE(sprdbat_battery_props);
	data->battery.get_property = sprdbat_battery_get_property;
	data->battery.name = "battery";
	data->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	data->battery.supplied_to = battery_supply_list;
	data->battery.num_supplicants = ARRAY_SIZE(battery_supply_list);

	data->ac.properties = sprdbat_ac_props;
	data->ac.num_properties = ARRAY_SIZE(sprdbat_ac_props);
	data->ac.get_property = sprdbat_ac_get_property;
	data->ac.name = "ac";
	data->ac.type = POWER_SUPPLY_TYPE_MAINS;
	data->ac.supplied_to = supply_list;
	data->ac.num_supplicants = ARRAY_SIZE(supply_list);

	data->usb.properties = sprdbat_usb_props;
	data->usb.num_properties = ARRAY_SIZE(sprdbat_usb_props);
	data->usb.get_property = sprdbat_usb_get_property;
	data->usb.name = "usb";
	data->usb.type = POWER_SUPPLY_TYPE_USB;
	data->usb.supplied_to = supply_list;
	data->usb.num_supplicants = ARRAY_SIZE(supply_list);

	data->start_charge = sprdbat_start_charge;
	data->stop_charge = sprdbat_stop_charge;

	ret = power_supply_register(&pdev->dev, &data->usb);
	if (ret)
		goto err_usb_failed;

	ret = power_supply_register(&pdev->dev, &data->ac);
	if (ret)
		goto err_ac_failed;

	ret = power_supply_register(&pdev->dev, &data->battery);
	if (ret)
		goto err_battery_failed;

	sprdbat_creat_caliberate_attr(data->battery.dev);

	data->gpio_vbat_detect = data->pdata->gpio_vbat_detect;
	if (data->gpio_vbat_detect > 0) {
		gpio_request(data->gpio_vbat_detect, "vbat_detect");
		gpio_direction_input(data->gpio_vbat_detect);
		data->irq_vbat_detect = gpio_to_irq(data->gpio_vbat_detect);

		//set_irq_flags(data->irq_vbat_detect,
		//	      IRQF_VALID | IRQF_NOAUTOEN);
		irq_set_status_flags(data->irq_vbat_detect, IRQ_NOAUTOEN);
		ret =
		    request_irq(data->irq_vbat_detect, sprdbat_vbat_detect_irq,
				IRQ_TYPE_LEVEL_LOW | IRQF_NO_SUSPEND,
				"sprdbat_vbat_detect", data);
		if (ret)
			dev_err(&pdev->dev, "failed to use vbat gpio: %d\n",
				ret);
	}

	mutex_init(&data->lock);

	wake_lock_init(&(data->charger_wake_lock), WAKE_LOCK_SUSPEND,
		       "charger_wake_lock");

	INIT_DELAYED_WORK(&data->battery_work, sprdbat_battery_works);
	INIT_DELAYED_WORK(&data->battery_sleep_work,
			  sprdbat_battery_sleep_works);
	INIT_WORK(&data->vbat_detect_irq_work, sprdbat_vbat_detect_irq_works);
	INIT_DELAYED_WORK(&sprdbat_data->sprdbat_charge_work, sprdbat_charge_works);
	//data->charge_work = &sprdbat_data->sprdbat_charge_work;
	data->monitor_wqueue = create_singlethread_workqueue("sprdbat_monitor");
	sprdchg_timer_init(sprdbat_timer_handler, data);

	sprdchg_init(data->pdata);
	sprdfgu_init(data->pdata);

#ifdef CONFIG_LEDS_TRIGGERS
	data->charging_led.name = "sprdbat_charging_led";
	data->charging_led.default_trigger = "battery-charging";
	data->charging_led.brightness_set = sprdchg_led_brightness_set;
	led_classdev_register(&pdev->dev, &data->charging_led);
#endif

	sprdbat_info_init(data);

	sprdbat_data->sprdbat_notifier.notifier_call = sprdbat_charge_fault_event;
	if(sprd_ext_ic_op->ext_register_notifier != NULL){
		SPRDBAT_DEBUG("ext_register_notifier\n");
		sprd_ext_ic_op->ext_register_notifier(&sprdbat_data->sprdbat_notifier);
	}
	usb_register_hotplug_callback(&power_cb);

	if (data->gpio_vbat_detect > 0) {
		enable_irq(sprdbat_data->irq_vbat_detect);
	}
	schedule_delayed_work(&data->battery_work,
			      sprdbat_data->pdata->bat_polling_time * HZ);
	SPRDBAT_DEBUG("sprdbat_probe----------end\n");
	return 0;

	power_supply_unregister(&data->battery);
err_battery_failed:
	power_supply_unregister(&data->ac);
err_ac_failed:
	power_supply_unregister(&data->usb);
err_usb_failed:
	kfree(data);
err_data_alloc_failed:
	sprdbat_data = NULL;
	return ret;

}

static int sprdbat_remove(struct platform_device *pdev)
{
	struct sprdbat_drivier_data *data = platform_get_drvdata(pdev);
	if(sprd_ext_ic_op->ext_unregster_notifier != NULL){
		SPRDBAT_DEBUG("ext_unregister_notifier\n");
		sprd_ext_ic_op->ext_unregster_notifier(&sprdbat_data->sprdbat_notifier);
	}
	sprdbat_remove_caliberate_attr(data->battery.dev);
	power_supply_unregister(&data->battery);
	power_supply_unregister(&data->ac);
	power_supply_unregister(&data->usb);

	kfree(data);
	sprdbat_data = NULL;
	return 0;
}

static int sprdbat_resume(struct platform_device *pdev)
{
	schedule_delayed_work(&sprdbat_data->battery_sleep_work, 0);
	sprdfgu_pm_op(0);
	return 0;
}

static int sprdbat_suspend(struct platform_device *pdev, pm_message_t state)
{
	sprdfgu_pm_op(1);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id battery_of_match[] = {
	{.compatible = "sprd,sprd-battery",},
	{}
};
#endif

static struct platform_driver sprdbat_driver = {
	.probe = sprdbat_probe,
	.remove = sprdbat_remove,
	.suspend = sprdbat_suspend,
	.resume = sprdbat_resume,
	.driver = {
		   .name = "sprd-battery",
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(battery_of_match),
#endif
		   }
};

static int __init sprd_battery_init(void)
{
	return platform_driver_register(&sprdbat_driver);
}

static void __exit sprd_battery_exit(void)
{
	platform_driver_unregister(&sprdbat_driver);
}

module_init(sprd_battery_init);
module_exit(sprd_battery_exit);

MODULE_AUTHOR("xianke.cui@spreadtrum.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Battery and charger driver for ext charge ic");

