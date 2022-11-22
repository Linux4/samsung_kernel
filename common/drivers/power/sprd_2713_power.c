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
#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/gpio.h>
#include <linux/device.h>

#include <linux/slab.h>
#include <linux/jiffies.h>
#include "sprd_battery.h"
#include <mach/usb.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#define SPRDBAT__DEBUG
#ifdef SPRDBAT__DEBUG
#define SPRDBAT_DEBUG(format, arg...) pr_info("sprdbat: " format, ## arg)
#else
#define SPRDBAT_DEBUG(format, arg...)
#endif

#define SPRDBAT_CV_TRIGGER_CURRENT		1/4

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
};

static struct sprdbat_drivier_data *sprdbat_data;
struct delayed_work sprdbat_charge_work;
static uint32_t sprdbat_cv_irq_dis = 1;
static uint32_t sprdbat_average_cnt;
static unsigned long sprdbat_update_capacity_time;
static unsigned long sprdbat_last_query_time;

static uint32_t sprdbat_trickle_chg;
static uint32_t sprdbat_start_chg;
static uint32_t poweron_capacity;
static struct notifier_block sprdbat_notifier;
static uint32_t sprdbat_cccv_cal_from_chip = 0;
static uint32_t chg_phy_dis_flag;	//only be used on CONFIG_SPRD_NOFGUCURRENT_CHG

extern struct sprdbat_auxadc_cal adc_cal;

static void sprdbat_change_module_state(uint32_t event);
static int sprdbat_stop_charge(void);

#ifdef SPRDBAT_TWO_CHARGE_CHANNEL
static int sprdbat_stop_charge_ext(void);
static int sprdbat_stop_chg_process_ext(void);
static int sprdbat_start_chg_process_ext(void);
static int plugin_callback_ext(int usb_cable, void *data);
static int plugout_callback_ext(int usb_cable, void *data);
#endif

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
	SPRDBAT_CALIBERATE_ATTR_WO(battery_0),
	SPRDBAT_CALIBERATE_ATTR_WO(battery_1),
	SPRDBAT_CALIBERATE_ATTR(hw_switch_point),
	SPRDBAT_CALIBERATE_ATTR_RO(charger_voltage),
	SPRDBAT_CALIBERATE_ATTR_RO(real_time_vbat_adc),
	SPRDBAT_CALIBERATE_ATTR_WO(save_capacity),
	SPRDBAT_CALIBERATE_ATTR_RO(temp_adc),
};

enum sprdbat_attribute {
	BATTERY_VOLTAGE = 0,
	STOP_CHARGE,
	BATTERY_NOW_CURRENT,
	BATTERY_0,
	BATTERY_1,
	HW_SWITCH_POINT,
	CHARGER_VOLTAGE,
	BATTERY_ADC,
	SAVE_CAPACITY,
	TEMP_ADC
};

static ssize_t sprdbat_store_caliberate(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long set_value;
	unsigned long irq_flag = 0;
	const ptrdiff_t off = attr - sprd_caliberate;

	set_value = simple_strtoul(buf, NULL, 10);
	pr_info("battery calibrate value %d %lu\n", off, set_value);

	mutex_lock(&sprdbat_data->lock);
	switch (off) {
	case STOP_CHARGE:
		if (0 == set_value) {
			sprdbat_change_module_state(SPRDBAT_ADP_PLUGIN_E);
		} else {
			sprdbat_change_module_state(SPRDBAT_ADP_PLUGOUT_E);
#ifdef SPRDBAT_TWO_CHARGE_CHANNEL
			sprdbat_data->bat_info.ac_online = 0;
			sprdbat_data->bat_info.usb_online = 0;
			sprdchg_stop_charge();
			sprdchg_stop_charge_ext();
#endif
		}
		break;
	case BATTERY_0:
		adc_cal.p0_vol = set_value & 0xffff;	//only for debug
		adc_cal.p0_adc = (set_value >> 16) & 0xffff;
		break;
	case BATTERY_1:
		adc_cal.p1_vol = set_value & 0xffff;
		adc_cal.p1_adc = (set_value >> 16) & 0xffff;
		adc_cal.cal_type = SPRDBAT_AUXADC_CAL_NV;
		break;
	case HW_SWITCH_POINT:
		if (!sprdbat_cccv_cal_from_chip) {
			local_irq_save(irq_flag);
			sprdbat_data->bat_info.cccv_point = set_value;
			sprdchg_set_cccvpoint(sprdbat_data->bat_info.
					      cccv_point);
			if ((!sprdbat_start_chg) && sprdbat_cv_irq_dis) {
				sprdbat_cv_irq_dis = 0;
				sprdbat_trickle_chg = 0;
				enable_irq(sprdbat_data->irq_chg_cv_state);
			}
			local_irq_restore(irq_flag);
		}
		break;
	case SAVE_CAPACITY:
		{
			int temp = set_value - poweron_capacity;

			pr_info("battery temp:%d\n", temp);
			if (abs(temp) >
			    sprdbat_data->pdata->cap_valid_range_poweron
			    || 0 == set_value) {
				pr_info("battery poweron capacity:%lu,%d\n",
					set_value, poweron_capacity);
				sprdbat_data->bat_info.capacity =
				    poweron_capacity;
			} else {
				pr_info("battery old capacity:%lu,%d\n",
					set_value, poweron_capacity);
				sprdbat_data->bat_info.capacity = set_value;
			}
			power_supply_changed(&sprdbat_data->battery);
		}
		break;
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
	int adc_value;
	int voltage;
	uint32_t now_current;

	switch (off) {
	case BATTERY_VOLTAGE:
		voltage = sprdchg_read_vbat_vol();
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", voltage);
		break;
	case BATTERY_NOW_CURRENT:
		if (sprdbat_data->bat_info.module_state ==
		    POWER_SUPPLY_STATUS_CHARGING) {
			now_current = sprdchg_read_chg_current();
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       now_current);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				       "discharging");
		}
		break;
	case HW_SWITCH_POINT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       sprdbat_data->bat_info.cccv_point);
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
	case TEMP_ADC:
		adc_value = sprdbat_read_temp_adc();
		if (adc_value < 0)
			adc_value = 0;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", adc_value);
		break;
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
	data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
	data->bat_info.module_state = POWER_SUPPLY_STATUS_DISCHARGING;
	data->bat_info.chg_stop_flags = SPRDBAT_CHG_END_NONE_BIT;
	data->bat_info.chg_start_time = 0;
	get_monotonic_boottime(&cur_time);
	sprdbat_update_capacity_time = cur_time.tv_sec;
	sprdbat_last_query_time = cur_time.tv_sec;
	data->bat_info.capacity = sprdfgu_poweron_capacity();	//~0;
	poweron_capacity = sprdfgu_poweron_capacity();
	data->bat_info.soc = sprdfgu_read_soc();
	data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
	data->bat_info.cur_temp = sprdbat_read_temp();
	data->bat_info.bat_current = sprdfgu_read_batcurrent();
	data->bat_info.chging_current = 0;
	data->bat_info.chg_current_type = sprdbat_data->pdata->adp_sdp_cur;
	{
		uint32_t cv_point;
		extern int sci_efuse_cccv_cal_get(unsigned int *p_cal_data);
		if (sci_efuse_cccv_cal_get(&cv_point)) {
			SPRDBAT_DEBUG("cccv_point efuse:%d\n", cv_point);
			data->bat_info.cccv_point =
			    sprdchg_tune_endvol_cccv(data->pdata->
						     chg_end_vol_pure,
						     cv_point);
			SPRDBAT_DEBUG
			    ("cccv_point sprdchg_tune_endvol_cccv:%d\n",
			     data->bat_info.cccv_point);
			sprdbat_cccv_cal_from_chip = 1;
		} else {
			data->bat_info.cccv_point = data->pdata->cccv_default;
			SPRDBAT_DEBUG("cccv_point default\n");
#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
			BUG_ON(1);	//if do NOT have isense resistor ,cccv must cal by ATE
#endif
		}
	}
	return;
}

static int sprdbat_start_charge(void)
{
	struct timespec cur_time;

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
	sprdchg_set_eoc_level(0);
	sprdchg_set_chg_cur(sprdbat_data->bat_info.chg_current_type);
	sprdchg_set_cccvpoint(sprdbat_data->bat_info.cccv_point);

	get_monotonic_boottime(&cur_time);
	sprdbat_data->bat_info.chg_start_time = cur_time.tv_sec;
	sprdchg_start_charge();
	sprdbat_average_cnt = 0;
	SPRDBAT_DEBUG
	    ("sprdbat_start_charge health:%d,chg_start_time:%ld,chg_current_type:%d\n",
	     sprdbat_data->bat_info.bat_health,
	     sprdbat_data->bat_info.chg_start_time,
	     sprdbat_data->bat_info.chg_current_type);

	sprdbat_trickle_chg = 0;
	sprdbat_start_chg = 1;
	chg_phy_dis_flag = 0;
	return 0;
}

static int sprdbat_stop_charge(void)
{
	unsigned long irq_flag = 0;
	sprdbat_data->bat_info.chg_start_time = 0;
	sprdchg_set_eoc_level(0);
	sprdchg_stop_charge();
	local_irq_save(irq_flag);
	if (!sprdbat_cv_irq_dis) {
		disable_irq_nosync(sprdbat_data->irq_chg_cv_state);
		sprdbat_cv_irq_dis = 1;
	}
	local_irq_restore(irq_flag);
	chg_phy_dis_flag = 1;
	SPRDBAT_DEBUG("sprdbat_stop_charge\n");
#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
	schedule_delayed_work(&sprdbat_data->battery_work,
			      sprdbat_data->pdata->bat_polling_time * HZ);
#endif
	return 0;
}

static inline void _sprdbat_clear_stopflags(uint32_t flag_msk)
{
	sprdbat_data->bat_info.chg_stop_flags &= ~flag_msk;
	SPRDBAT_DEBUG("_sprdbat_clear_stopflags flag_msk:0x%x\n", flag_msk);

	if (sprdbat_data->bat_info.chg_stop_flags == SPRDBAT_CHG_END_NONE_BIT) {
		sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
		if (flag_msk != SPRDBAT_CHG_END_FULL_BIT
		    && flag_msk != SPRDBAT_CHG_END_TIMEOUT_BIT) {
			sprdbat_data->bat_info.module_state =
			    POWER_SUPPLY_STATUS_CHARGING;
		}
		//sprdbat_start_charge();
		sprdbat_data->start_charge();
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
	} else {
		BUG_ON(1);
	}
}

static inline void _sprdbat_set_stopflags(uint32_t flag)
{
	SPRDBAT_DEBUG("_sprdbat_set_stopflags flags:0x%x\n", flag);

	if (sprdbat_data->bat_info.chg_stop_flags == SPRDBAT_CHG_END_NONE_BIT) {
		//sprdbat_stop_charge();
		sprdbat_data->stop_charge();
	}
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
		_sprdbat_clear_stopflags(~0);
		queue_delayed_work(sprdbat_data->monitor_wqueue,
				   sprdbat_data->charge_work, 2 * HZ);
		break;
	case SPRDBAT_ADP_PLUGOUT_E:
		sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
		sprdbat_data->bat_info.chg_stop_flags =
		    SPRDBAT_CHG_END_NONE_BIT;
		sprdbat_data->bat_info.module_state =
		    POWER_SUPPLY_STATUS_DISCHARGING;
		//sprdbat_stop_charge();
		sprdbat_data->stop_charge();
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
#ifndef SPRDBAT_TWO_CHARGE_CHANNEL
	wake_lock_timeout(&(sprdbat_data->charger_plug_out_lock),
			  SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);

	sprdbat_data->bat_info.adp_type = sprdchg_charger_is_adapter();
	if (sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP) {
		sprdbat_data->bat_info.ac_online = 1;
	} else {
		sprdbat_data->bat_info.usb_online = 1;
	}
	sprdbat_adp_plug_nodify(1);
#else
	if (sprdbat_data->bat_info.usb_online) {
		sprdbat_stop_chg_process_ext();
	} else {
		sprdbat_adp_plug_nodify(1);
	}
	sprdbat_data->bat_info.ac_online = 1;
	sprdbat_data->bat_info.adp_type = ADP_TYPE_DCP;
	sprdbat_data->start_charge = sprdbat_start_charge;
	sprdbat_data->stop_charge = sprdbat_stop_charge;
	sprdbat_data->charge_work = &sprdbat_charge_work;
#endif
	sprdbat_change_module_state(SPRDBAT_ADP_PLUGIN_E);
	irq_set_irq_type(sprdbat_data->irq_vchg_ovi, IRQ_TYPE_LEVEL_HIGH);
	enable_irq(sprdbat_data->irq_vchg_ovi);

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

#ifndef SPRDBAT_TWO_CHARGE_CHANNEL
	wake_lock_timeout(&(sprdbat_data->charger_plug_out_lock),
			  SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);
	sprdbat_adp_plug_nodify(0);
	sprdbat_data->bat_info.adp_type = ADP_TYPE_UNKNOW;
	sprdbat_data->bat_info.ac_online = 0;
	sprdbat_data->bat_info.usb_online = 0;
#else
	sprdbat_data->bat_info.ac_online = 0;
	if (sprdbat_data->bat_info.usb_online) {
		sprdbat_start_chg_process_ext();
	} else {
		sprdbat_adp_plug_nodify(0);
	}
#endif
	mutex_unlock(&sprdbat_data->lock);

	if (adp_type == ADP_TYPE_DCP)
		power_supply_changed(&sprdbat_data->ac);
	else
		power_supply_changed(&sprdbat_data->usb);
	return 0;
}

#ifndef SPRDBAT_TWO_CHARGE_CHANNEL
static struct usb_hotplug_callback power_cb = {
	.plugin = plugin_callback,
	.plugout = plugout_callback,
	.data = NULL,
};
#else
static struct usb_hotplug_callback power_cb = {
	.plugin = plugin_callback_ext,
	.plugout = plugout_callback_ext,
	.data = NULL,
};

#endif

static char *supply_list[] = {
	"battery",
};

static char *battery_supply_list[] = {
	"audio-ldo",
	"sprdfgu",
};

static int sprdbat_timer_handler(void *data)
{
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
			   sprdbat_data->charge_work, 0);
	return 0;
}

static __used irqreturn_t sprdbat_vchg_ovi_irq(int irq, void *dev_id)
{
	int value;

	value = gpio_get_value(sprdbat_data->gpio_vchg_ovi);
	if (value) {
		SPRDBAT_DEBUG("charger ovi high\n");
		irq_set_irq_type(sprdbat_data->irq_vchg_ovi,
				 IRQ_TYPE_LEVEL_LOW);
	} else {
		SPRDBAT_DEBUG("charger ovi low\n");
		irq_set_irq_type(sprdbat_data->irq_vchg_ovi,
				 IRQ_TYPE_LEVEL_HIGH);
	}
	queue_work(sprdbat_data->monitor_wqueue, &sprdbat_data->ovi_irq_work);
	return IRQ_HANDLED;
}

static int sprdbat_adjust_cccvpoint(uint32_t vbat_now)
{

	uint32_t cv;

	if (sprdbat_cccv_cal_from_chip) {
		return 0;
	}

	if (vbat_now <= sprdbat_data->pdata->chg_end_vol_pure) {
		cv = ((sprdbat_data->pdata->chg_end_vol_pure -
		       vbat_now) * 10) / ONE_CCCV_STEP_VOL + 1;
		cv += sprdchg_get_cccvpoint();

		SPRDBAT_DEBUG("sprdbat_adjust_cccvpoint turn high cv:0x%x\n",
			      cv);
		//BUG_ON(cv > SPRDBAT_CCCV_MAX);
		if (cv > SPRDBAT_CCCV_MAX) {
			cv = SPRDBAT_CCCV_MAX;
		}
		sprdbat_data->bat_info.cccv_point = cv;
		sprdchg_set_cccvpoint(cv);
	} else {
		cv = sprdchg_get_cccvpoint();
		//BUG_ON(cv < (SPRDBAT_CCCV_MIN + 2));
		if (cv < (SPRDBAT_CCCV_MIN + 1)) {
			cv = SPRDBAT_CCCV_MIN + 1;
		}
		cv -= 1;
		sprdbat_data->bat_info.cccv_point = cv;
		SPRDBAT_DEBUG("sprdbat_adjust_cccvpoint turn low cv:0x%x\n",
			      cv);
		sprdchg_set_cccvpoint(cv);
	}

	return 0;
}

#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
static irqreturn_t sprdbat_chg_cv_irq(int irq, void *dev_id)
{
	if (sprdchg_get_eoc_level() != sprdbat_data->pdata->chg_eoc_level) {
		sprdchg_set_eoc_level(sprdbat_data->pdata->chg_eoc_level);
		SPRDBAT_DEBUG("sprdbat_chg_cv_irq reset eoc level\n");
		return IRQ_HANDLED;
	}

	if (!sprdbat_cv_irq_dis) {
		disable_irq_nosync(sprdbat_data->irq_chg_cv_state);
		sprdbat_cv_irq_dis = 1;
	}

	sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	SPRDBAT_DEBUG("sprdbat_chg_cv_irq vol %d\n",
		      sprdbat_data->bat_info.vbat_vol);

	SPRDBAT_DEBUG("sprdbat_chg_cv_irq voltage full ,trickle chargeing\n");
	sprdbat_trickle_chg = 1;

	return IRQ_HANDLED;
}
#else
static irqreturn_t sprdbat_chg_cv_irq(int irq, void *dev_id)
{
	int value;

	value = gpio_get_value(sprdbat_data->gpio_chg_cv_state);

	SPRDBAT_DEBUG("sprdbat_chg_cv_irq:%d\n", value);

	if (!sprdbat_cv_irq_dis) {
		disable_irq_nosync(sprdbat_data->irq_chg_cv_state);
		sprdbat_cv_irq_dis = 1;
	}

	sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	SPRDBAT_DEBUG("sprdbat_chg_cv_irq vol %d\n",
		      sprdbat_data->bat_info.vbat_vol);
	if (sprdbat_data->bat_info.vbat_vol >=
	    sprdbat_data->pdata->chg_end_vol_pure) {
		SPRDBAT_DEBUG
		    ("sprdbat_chg_cv_irq voltage full ,trickle chargeing\n");
		sprdbat_trickle_chg = 1;
	} else {
		sprdbat_average_cnt = 0;
		sprdbat_adjust_cccvpoint(sprdbat_data->bat_info.vbat_vol);
		schedule_delayed_work(&sprdbat_data->cv_irq_work, (HZ * 1));
	}
	return IRQ_HANDLED;
}
#endif
static void sprdbat_cv_irq_works(struct work_struct *work)
{
	mutex_lock(&sprdbat_data->lock);
	if ((sprdbat_data->bat_info.module_state ==
	     POWER_SUPPLY_STATUS_DISCHARGING)
	    || (sprdbat_data->bat_info.chg_stop_flags !=
		SPRDBAT_CHG_END_NONE_BIT)) {
		mutex_unlock(&sprdbat_data->lock);	//fixed
		SPRDBAT_DEBUG("sprdbat_cv_irq_works return \n");
		return;
	} else {
		SPRDBAT_DEBUG("sprdbat_cv_irq_works adjust cccv\n");
		if (sprdbat_cv_irq_dis) {
			SPRDBAT_DEBUG("sprdbat_cv_irq_works enable_irq\n");
			sprdbat_cv_irq_dis = 0;
			enable_irq(sprdbat_data->irq_chg_cv_state);
		}
	}
	mutex_unlock(&sprdbat_data->lock);
}

static void sprdbat_ovi_irq_works(struct work_struct *work)
{
	int value;

	value = gpio_get_value(sprdbat_data->gpio_vchg_ovi);

	sprdbat_data->bat_info.vchg_vol = sprdchg_read_vchg_vol();
	SPRDBAT_DEBUG("vchg_vol:%d,gpio:%d\n", sprdbat_data->bat_info.vchg_vol,
		      value);

	mutex_lock(&sprdbat_data->lock);
	if (sprdbat_data->bat_info.module_state ==
	    POWER_SUPPLY_STATUS_DISCHARGING) {
		mutex_unlock(&sprdbat_data->lock);
		return;
	}
#ifdef SPRDBAT_TWO_CHARGE_CHANNEL
	if (!sprdbat_data->bat_info.ac_online) {
		mutex_unlock(&sprdbat_data->lock);
		return;
	}
#endif

	if (value) {
		if (!
		    (SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->bat_info.
		     chg_stop_flags)) {
			SPRDBAT_DEBUG("SPRDBAT_OVI_STOP_E\n");
			sprdbat_change_module_state(SPRDBAT_OVI_STOP_E);
		}
	} else {
		if ((SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->bat_info.
		     chg_stop_flags)) {
			SPRDBAT_DEBUG("SPRDBAT_OVI_RESTART_E\n");
			sprdbat_change_module_state(SPRDBAT_OVI_RESTART_E);
		}
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
	    ("chg_log:chgcur_type:%d,cccv:%d,vchg:%d,cvstate:%d,cccv_cal:%d\n",
	     sprdbat_data->bat_info.chg_current_type,
	     sprdbat_data->bat_info.cccv_point, sprdchg_read_vchg_vol(),
	     gpio_get_value(sprdbat_data->gpio_chg_cv_state),
	     sprdbat_cccv_cal_from_chip);

	//SPRDBAT_DEBUG("aux vbat vol:%d\n", sprdchg_read_vbat_vol());

}

static void sprdbat_print_battery_log(void)
{
	struct timespec cur_time;

	get_monotonic_boottime(&cur_time);

	SPRDBAT_DEBUG("bat_log:time:%ld,vbat:%d,\
ocv:%d,current:%d,cap:%d,state:%d,auxbat:%d\n", cur_time.tv_sec, sprdbat_data->bat_info.vbat_vol,
		      sprdbat_data->bat_info.vbat_ocv, sprdbat_data->bat_info.bat_current,
		      sprdbat_data->bat_info.capacity, sprdbat_data->bat_info.module_state,
		      sprdchg_read_vbat_vol());
}

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
	flush_time = cur_time.tv_sec - sprdbat_update_capacity_time;
	period_time = cur_time.tv_sec - sprdbat_last_query_time;
	sprdbat_last_query_time = cur_time.tv_sec;

	SPRDBAT_DEBUG("fgu_cap: = %d,flush: = %d,period:=%d\n",
		      fgu_capacity, flush_time, period_time);

	switch (sprdbat_data->bat_info.module_state) {
	case POWER_SUPPLY_STATUS_CHARGING:
		if (fgu_capacity <= sprdbat_data->bat_info.capacity) {
			fgu_capacity = sprdbat_data->bat_info.capacity;
		} else {
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
			sprdbat_update_capacity_time = cur_time.tv_sec;
			fgu_capacity = 100;
		} else {
			if (fgu_capacity >= 100) {
				fgu_capacity = 99;
			}
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
		sprdbat_update_capacity_time = cur_time.tv_sec;
		if (fgu_capacity != 100) {
			fgu_capacity = 100;
		}
		break;
	default:
		BUG_ON(1);
		break;
	}

	if (sprdbat_data->bat_info.vbat_vol <=
	    sprdbat_data->pdata->soft_vbat_uvlo) {
		fgu_capacity = 0;
		SPRDBAT_DEBUG("soft uvlo, shutdown.... vol:%d",
			      sprdbat_data->bat_info.vbat_vol);
	}
	if (fgu_capacity != sprdbat_data->bat_info.capacity) {
		sprdbat_data->bat_info.capacity = fgu_capacity;
		sprdbat_update_capacity_time = cur_time.tv_sec;
		power_supply_changed(&sprdbat_data->battery);
	}

}

static void sprdbat_battery_works(struct work_struct *work)
{
	SPRDBAT_DEBUG("sprdbat_battery_works\n");

	mutex_lock(&sprdbat_data->lock);

#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
	if (sprdbat_data->bat_info.module_state !=
	    POWER_SUPPLY_STATUS_DISCHARGING
	    && sprdbat_data->bat_info.chg_stop_flags ==
	    SPRDBAT_CHG_END_NONE_BIT) {
		mutex_unlock(&sprdbat_data->lock);
		SPRDBAT_DEBUG("sprdbat_battery_works return\n");
		return;
	}
#endif

	sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	sprdbat_data->bat_info.cur_temp = sprdbat_read_temp();
	sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
	sprdbat_data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();

	sprdbat_update_capacty();

	mutex_unlock(&sprdbat_data->lock);
	sprdbat_print_battery_log();

#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
	if (sprdbat_data->bat_info.module_state ==
	    POWER_SUPPLY_STATUS_DISCHARGING
	    || sprdbat_data->bat_info.chg_stop_flags !=
	    SPRDBAT_CHG_END_NONE_BIT) {
		schedule_delayed_work(&sprdbat_data->battery_work,
				      sprdbat_data->pdata->bat_polling_time *
				      HZ);
	}
#else
	if (sprdbat_data->bat_info.module_state == POWER_SUPPLY_STATUS_CHARGING) {
		schedule_delayed_work(&sprdbat_data->battery_work,
				      sprdbat_data->pdata->
				      bat_polling_time_fast * HZ);
	} else {
		schedule_delayed_work(&sprdbat_data->battery_work,
				      sprdbat_data->pdata->bat_polling_time *
				      HZ);
	}
#endif
}

static void sprdbat_battery_sleep_works(struct work_struct *work)
{
	SPRDBAT_DEBUG("sprdbat_battery_sleep_works\n");
	if (schedule_delayed_work(&sprdbat_data->battery_work, 0) == 0) {
		cancel_delayed_work_sync(&sprdbat_data->battery_work);
		schedule_delayed_work(&sprdbat_data->battery_work, 0);
	}
}

static uint32_t temp_trigger_cnt;
static void sprdbat_temp_monitor(void)
{
	sprdbat_data->bat_info.cur_temp = sprdbat_read_temp();

	if (sprdbat_data->bat_info.
	    chg_stop_flags & SPRDBAT_CHG_END_OTP_COLD_BIT) {
		if (sprdbat_data->bat_info.cur_temp >
		    sprdbat_data->pdata->otp_low_restart)
			sprdbat_change_module_state(SPRDBAT_OTP_COLD_RESTART_E);

		SPRDBAT_DEBUG("otp_low_restart:%d\n",
			      sprdbat_data->pdata->otp_low_restart);
	} else if (sprdbat_data->bat_info.
		   chg_stop_flags & SPRDBAT_CHG_END_OTP_OVERHEAT_BIT) {
		if (sprdbat_data->bat_info.cur_temp <
		    sprdbat_data->pdata->otp_high_restart)
			sprdbat_change_module_state
			    (SPRDBAT_OTP_OVERHEAT_RESTART_E);

		SPRDBAT_DEBUG("otp_high_restart:%d\n",
			      sprdbat_data->pdata->otp_high_restart);
	} else {
		if (sprdbat_data->bat_info.cur_temp <
		    sprdbat_data->pdata->otp_low_stop
		    || sprdbat_data->bat_info.cur_temp >
		    sprdbat_data->pdata->otp_high_stop) {
			SPRDBAT_DEBUG("otp_low_stop:%d,otp_high_stop:%d\n",
				      sprdbat_data->pdata->otp_low_stop,
				      sprdbat_data->pdata->otp_high_stop);
			temp_trigger_cnt++;
			if (temp_trigger_cnt > SPRDBAT_TEMP_TRIGGER_TIMES) {
				if (sprdbat_data->bat_info.cur_temp <
				    sprdbat_data->pdata->otp_low_stop) {
					sprdbat_change_module_state
					    (SPRDBAT_OTP_COLD_STOP_E);
				} else {
					sprdbat_change_module_state
					    (SPRDBAT_OTP_OVERHEAT_STOP_E);
				}
				sprdchg_timer_enable(sprdbat_data->pdata->
						     chg_polling_time);
				temp_trigger_cnt = 0;
			} else {
				sprdchg_timer_enable(sprdbat_data->pdata->
						     chg_polling_time_fast);
			}
		} else {
			if (temp_trigger_cnt) {
				sprdchg_timer_enable(sprdbat_data->pdata->
						     chg_polling_time);
				temp_trigger_cnt = 0;
			}
		}
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

#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
static inline sprdbat_polling_chg_state(void)
{
	unsigned long irq_flag = 0;

	if (gpio_get_value(sprdbat_data->gpio_chg_cv_state)) {
		struct timespec cur_time;
		uint32_t chging_keep_time;

		SPRDBAT_DEBUG("first cccv eoc%d!\n", sprdchg_get_eoc_level());
		get_monotonic_boottime(&cur_time);
		chging_keep_time =
		    cur_time.tv_sec - sprdbat_data->bat_info.chg_start_time;
		if (chging_keep_time <= sprdbat_data->bat_info.chg_this_timeout) {
			if (sprdbat_data->pdata->chg_cv_timeout <
			    sprdbat_data->bat_info.chg_this_timeout -
			    chging_keep_time) {
				sprdbat_data->bat_info.chg_this_timeout =
				    sprdbat_data->pdata->chg_cv_timeout;
			} else {
				sprdbat_data->bat_info.chg_this_timeout =
				    sprdbat_data->bat_info.chg_this_timeout -
				    chging_keep_time;
			}
			sprdbat_data->bat_info.chg_start_time = cur_time.tv_sec;
		}
#if 1
		sprdchg_set_eoc_level(sprdbat_data->pdata->chg_eoc_level);
		mdelay(5);
		SPRDBAT_DEBUG("after reset eoc%d, cv:%d!\n",
			      sprdchg_get_eoc_level(),
			      gpio_get_value(sprdbat_data->gpio_chg_cv_state));

		{
			struct irq_desc *desc =
			    irq_to_desc(sprdbat_data->irq_chg_cv_state);
			struct irq_data *idata = irq_desc_get_irq_data(desc);
			struct irq_chip *chip = irq_desc_get_chip(desc);
			if (chip->irq_ack) {
				chip->irq_ack(idata);
			}
		}

		local_irq_save(irq_flag);
		if (sprdbat_cv_irq_dis) {
			sprdbat_cv_irq_dis = 0;
			SPRDBAT_DEBUG("open cv irq--first cccv\n");
			irq_set_irq_type(sprdbat_data->irq_chg_cv_state,
					 IRQ_TYPE_LEVEL_HIGH);
			enable_irq(sprdbat_data->irq_chg_cv_state);
		}
		local_irq_restore(irq_flag);
#endif
	}
	SPRDBAT_DEBUG("no fgu current charing... %d\n",
		      gpio_get_value(sprdbat_data->gpio_chg_cv_state));
	if (sprdbat_trickle_chg) {
		sprdbat_trickle_chg = 0;
		SPRDBAT_DEBUG("SPRDBAT_CHG_FULL_E\n");
		sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
	}
}
#endif

static void sprdbat_charge_works(struct work_struct *work)
{
	uint32_t cur;
	unsigned long irq_flag = 0;

	SPRDBAT_DEBUG("sprdbat_charge_works-start\n");

	mutex_lock(&sprdbat_data->lock);

	if (sprdbat_data->bat_info.module_state ==
	    POWER_SUPPLY_STATUS_DISCHARGING) {
		mutex_unlock(&sprdbat_data->lock);
		return;
	}
#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
	if ((sprdbat_data->bat_info.chg_stop_flags == SPRDBAT_CHG_END_NONE_BIT)
	    && chg_phy_dis_flag && (!temp_trigger_cnt)) {
		sprdbat_update_capacty();
		sprdbat_print_battery_log();
		sprdchg_timer_enable(sprdbat_data->pdata->chg_polling_time);
		sprdchg_start_charge();
		chg_phy_dis_flag = 0;
		SPRDBAT_DEBUG("update capacty-charging\n");
		mutex_unlock(&sprdbat_data->lock);
		return;
	}
#endif

	if (sprdbat_start_chg) {
		sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
		sprdbat_data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
		sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
		SPRDBAT_DEBUG
		    ("sprdbat_charge_works---vbat_vol %d,ocv:%d,bat_current:%d\n",
		     sprdbat_data->bat_info.vbat_vol,
		     sprdbat_data->bat_info.vbat_ocv,
		     sprdbat_data->bat_info.bat_current);

		if (!sprdbat_cccv_cal_from_chip) {
			local_irq_save(irq_flag);
			if (sprdbat_cv_irq_dis) {
				sprdbat_cv_irq_dis = 0;
				enable_irq(sprdbat_data->irq_chg_cv_state);
			}
			local_irq_restore(irq_flag);
		}
		sprdbat_start_chg = 0;
	}

	sprdbat_temp_monitor();

	if (sprdbat_data->bat_info.chg_stop_flags == SPRDBAT_CHG_END_NONE_BIT) {
		if (sprdbat_is_chg_timeout()) {
			SPRDBAT_DEBUG("chg timeout\n");
			if (sprdbat_data->bat_info.vbat_ocv >
			    sprdbat_data->pdata->rechg_vol) {
				sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
			} else {
				sprdbat_data->bat_info.chg_this_timeout =
				    sprdbat_data->pdata->chg_rechg_timeout;
				sprdbat_change_module_state
				    (SPRDBAT_CHG_TIMEOUT_E);
			}
		}
#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
		sprdbat_polling_chg_state();
#else
		if (sprdbat_cccv_cal_from_chip) {
			int value =
			    gpio_get_value(sprdbat_data->gpio_chg_cv_state);
			if (value
			    && (sprdbat_data->bat_info.bat_current <
				sprdbat_data->pdata->chg_end_cur)) {
				sprdbat_trickle_chg = 0;
				SPRDBAT_DEBUG("SPRDBAT_CHG_FULL_E\n");
				sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
			}
		} else {
			if ((sprdbat_data->bat_info.vbat_vol >
			     sprdbat_data->pdata->chg_end_vol_l
			     || sprdbat_trickle_chg)
			    && sprdbat_data->bat_info.bat_current <
			    sprdbat_data->pdata->chg_end_cur) {
				sprdbat_trickle_chg = 0;
				SPRDBAT_DEBUG("SPRDBAT_CHG_FULL_E\n");
				sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
			}
		}
#endif
		//cccv point is high
		if (sprdbat_data->bat_info.vbat_vol >
		    sprdbat_data->pdata->chg_end_vol_h) {
			SPRDBAT_DEBUG("cccv point is high\n");
			sprdbat_adjust_cccvpoint(sprdbat_data->
						 bat_info.vbat_vol);
		}

		/*the purpose of this code is the same as cv irq handler,
		   if cv irq is very accurately, this code can be deleted */
		//cccv point is low
		cur = sprdchg_read_chg_current();
		sprdchg_put_chgcur(cur);
		sprdbat_data->bat_info.chging_current =
		    sprdchg_get_chgcur_ave();

		SPRDBAT_DEBUG
		    ("chg_cur: %d,chg_ave_current:%d\n",
		     cur, sprdbat_data->bat_info.chging_current);

		sprdbat_average_cnt++;
		if (sprdbat_average_cnt == SPRDBAT_AVERAGE_COUNT) {
			uint32_t triggre_current =
			    sprdbat_data->bat_info.chg_current_type *
			    SPRDBAT_CV_TRIGGER_CURRENT;
			sprdbat_average_cnt = 0;
			if (sprdbat_data->bat_info.vbat_vol <
			    sprdbat_data->pdata->chg_end_vol_pure
			    && sprdbat_data->bat_info.chging_current <
			    triggre_current) {
				SPRDBAT_DEBUG
				    ("turn high cccv point vol:%d,chg_cur:%d,trigger_cur:%d",
				     sprdbat_data->bat_info.vbat_vol,
				     sprdbat_data->bat_info.chging_current,
				     triggre_current);
				sprdbat_adjust_cccvpoint(sprdbat_data->bat_info.
							 vbat_vol);
			}
		}

	}

	if (sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_TIMEOUT_BIT) {
		SPRDBAT_DEBUG("SPRDBAT_CHG_TIMEOUT_RESTART_E\n");
		sprdbat_change_module_state(SPRDBAT_CHG_TIMEOUT_RESTART_E);
	}

	if (sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_FULL_BIT) {
		if (sprdbat_data->bat_info.vbat_ocv <
		    sprdbat_data->pdata->rechg_vol) {
			SPRDBAT_DEBUG("SPRDBAT_RECHARGE_E\n");
			sprdbat_change_module_state(SPRDBAT_RECHARGE_E);
		}
	}
#ifdef CONFIG_SPRD_NOFGUCURRENT_CHG
	if ((sprdbat_data->bat_info.chg_stop_flags == SPRDBAT_CHG_END_NONE_BIT)) {
		sprdchg_stop_charge();
		sprdchg_timer_enable(sprdbat_data->pdata->
				     chg_polling_time_fast);
		chg_phy_dis_flag = 1;
	} else {
		sprdchg_timer_enable(sprdbat_data->pdata->chg_polling_time);
	}
#endif

	mutex_unlock(&sprdbat_data->lock);
	sprdbat_chg_print_log();
}

static int sprdbat_fgu_event(struct notifier_block *this, unsigned long event,
			     void *ptr)
{
	SPRDBAT_DEBUG("sprdbat_fgu_event---vol:%d\n", sprdbat_read_vbat_vol());
	mutex_lock(&sprdbat_data->lock);
	sprdbat_adjust_cccvpoint(sprdbat_read_vbat_vol());
	mutex_unlock(&sprdbat_data->lock);
	return 0;
}

//external charge ic code
#ifdef SPRDBAT_TWO_CHARGE_CHANNEL
static struct delayed_work sprdbat_charge_work_ext;
static struct work_struct sprdbat_chg_detect_work;
static uint32_t sprdbat_chg_online;

static void sprdbat_chg_detect_works(struct work_struct *work)
{
	if (sprdbat_chg_online) {
		SPRDBAT_DEBUG
		    ("sprdbat_chg_detect_works plug in,vbus pull up\n");
		plugin_callback(0, 0);
	} else {
		SPRDBAT_DEBUG
		    ("sprdbat_chg_detect_works plug out,vbus pull down\n");
		plugout_callback(0, 0);
	}
}

static irqreturn_t sprdbat_chg_detect_int(int irq, void *dev_id)
{
	wake_lock_timeout(&(sprdbat_data->charger_plug_out_lock),
			  SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);

	sprdbat_chg_online = gpio_get_value(sprdbat_data->gpio_charger_detect);
	SPRDBAT_DEBUG("sprdbat_chg_detect_int------sprdbat_chg_online:%d\n",
		      sprdbat_chg_online);
	irq_set_irq_type(irq,
			 sprdbat_chg_online ? IRQF_TRIGGER_LOW :
			 IRQF_TRIGGER_HIGH);

	queue_work(sprdbat_data->monitor_wqueue, &sprdbat_chg_detect_work);

	return IRQ_HANDLED;
}

static void sprdbat_charge_works_ext(struct work_struct *work)
{
	SPRDBAT_DEBUG("sprdbat_charge_works_ext----------start\n");

	sprdchg_chg_monitor_cb_ext(NULL);
	mutex_lock(&sprdbat_data->lock);

	if (sprdbat_data->bat_info.module_state ==
	    POWER_SUPPLY_STATUS_DISCHARGING) {
		mutex_unlock(&sprdbat_data->lock);
		return;
	}
	sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	sprdbat_data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
	sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();

	SPRDBAT_DEBUG("sprdbat_charge_works----------vbat_vol %d,ocv:%d\n",
		      sprdbat_data->bat_info.vbat_vol,
		      sprdbat_data->bat_info.vbat_ocv);
	SPRDBAT_DEBUG("sprdbat_charge_works----------bat_current %d\n",
		      sprdbat_data->bat_info.bat_current);
	sprdbat_temp_monitor();

	if (sprdbat_data->bat_info.chg_stop_flags == SPRDBAT_CHG_END_NONE_BIT) {
		if (sprdbat_is_chg_timeout()) {
			SPRDBAT_DEBUG("chg timeout\n");
			if (sprdbat_data->bat_info.vbat_ocv >
			    sprdbat_data->pdata->rechg_vol) {
				sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
			} else {
				sprdbat_data->bat_info.chg_this_timeout =
				    sprdbat_data->pdata->chg_rechg_timeout;
				sprdbat_change_module_state
				    (SPRDBAT_CHG_TIMEOUT_E);
			}
		}

		if (sprdchg_is_chg_done_ext()) {
			SPRDBAT_DEBUG("SPRDBAT_CHG_FULL_E\n");
			sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
		}

	}

	if (sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_TIMEOUT_BIT) {
		SPRDBAT_DEBUG("SPRDBAT_CHG_TIMEOUT_RESTART_E\n");
		sprdbat_change_module_state(SPRDBAT_CHG_TIMEOUT_RESTART_E);
	}

	if (sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_FULL_BIT) {
		if (sprdbat_data->bat_info.vbat_ocv <
		    sprdbat_data->pdata->rechg_vol) {
			SPRDBAT_DEBUG("SPRDBAT_RECHARGE_E\n");
			sprdbat_change_module_state(SPRDBAT_RECHARGE_E);
		}
	}

	mutex_unlock(&sprdbat_data->lock);
	sprdbat_chg_print_log();
	SPRDBAT_DEBUG("sprdbat_charge_works_ext----------end\n");
}

void sprdbat_charge_event_ext(uint32_t event)
{
	mutex_lock(&sprdbat_data->lock);
	if (sprdbat_data->bat_info.module_state ==
	    POWER_SUPPLY_STATUS_DISCHARGING
	    || sprdbat_data->bat_info.ac_online) {
		mutex_unlock(&sprdbat_data->lock);
		return;
	}

	switch (event) {
	case SPRDBAT_CHG_EVENT_EXT_OVI:

		if (!
		    (SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->bat_info.
		     chg_stop_flags)) {
			SPRDBAT_DEBUG("SPRDBAT_OVI_STOP_E\n");
			sprdbat_change_module_state(SPRDBAT_OVI_STOP_E);
		}
		break;
	case SPRDBAT_CHG_EVENT_EXT_OVI_RESTART:
		if ((SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->bat_info.
		     chg_stop_flags)) {
			SPRDBAT_DEBUG("SPRDBAT_OVI_RESTART_E\n");
			sprdbat_change_module_state(SPRDBAT_OVI_RESTART_E);
		}
		break;
	default:
		break;
	}
	mutex_unlock(&sprdbat_data->lock);
}

static int sprdbat_start_charge_ext(void)
{
	struct timespec cur_time;

	get_monotonic_boottime(&cur_time);
	sprdbat_data->bat_info.chg_start_time = cur_time.tv_sec;
	sprdchg_start_charge_ext();

	SPRDBAT_DEBUG
	    ("sprdbat_start_charge bat_health:%d,chg_start_time:%ld,chg_current_type:%d\n",
	     sprdbat_data->bat_info.bat_health,
	     sprdbat_data->bat_info.chg_start_time,
	     sprdbat_data->bat_info.chg_current_type);
	return 0;
}

static int sprdbat_stop_charge_ext(void)
{
	sprdbat_data->bat_info.chg_start_time = 0;

	sprdchg_stop_charge_ext();

	SPRDBAT_DEBUG("sprdbat_stop_charge\n");
	return 0;
}

static int sprdbat_start_chg_process_ext(void)
{
	sprdbat_data->bat_info.adp_type = ADP_TYPE_SDP;
	sprdbat_data->start_charge = sprdbat_start_charge_ext;
	sprdbat_data->stop_charge = sprdbat_stop_charge_ext;
	sprdbat_data->charge_work = &sprdbat_charge_work_ext;
	sprdchg_open_ovi_fun_ext();
	sprdchg_timer_enable(sprdbat_data->pdata->chg_polling_time);
	sprdbat_change_module_state(SPRDBAT_ADP_PLUGIN_E);
	return 0;
}

static int sprdbat_stop_chg_process_ext(void)
{
	sprdbat_data->bat_info.adp_type = ADP_TYPE_UNKNOW;
	sprdchg_close_ovi_fun_ext();
	sprdchg_timer_disable();
	sprdbat_change_module_state(SPRDBAT_ADP_PLUGOUT_E);
	return 0;
}

static int plugin_callback_ext(int usb_cable, void *data)
{
	SPRDBAT_DEBUG("charger plugin_callback_ext happen\n");
	wake_lock_timeout(&(sprdbat_data->charger_plug_out_lock),
			  SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);
	mutex_lock(&sprdbat_data->lock);

	sprdbat_data->bat_info.usb_online = 1;
	if (!sprdbat_data->bat_info.ac_online) {
		sprdbat_adp_plug_nodify(1);
		sprdbat_start_chg_process_ext();
	}
	mutex_unlock(&sprdbat_data->lock);

	power_supply_changed(&sprdbat_data->ac);
	power_supply_changed(&sprdbat_data->usb);

	SPRDBAT_DEBUG("plugin_callback_ext: end...\n");
	return 0;
}

static int plugout_callback_ext(int usb_cable, void *data)
{
	sprdbat_data->bat_info.usb_online = 0;
	SPRDBAT_DEBUG("charger plugout_callback_ext happen\n");

	wake_lock_timeout(&(sprdbat_data->charger_plug_out_lock),
			  SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);
	mutex_lock(&sprdbat_data->lock);
	if (!sprdbat_data->bat_info.ac_online) {
		sprdbat_adp_plug_nodify(0);
		sprdbat_stop_chg_process_ext();
	}
	mutex_unlock(&sprdbat_data->lock);

	power_supply_changed(&sprdbat_data->ac);
	power_supply_changed(&sprdbat_data->usb);
	return 0;
}

#endif

//
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
	for (i = 0; i < pdata->ocv_tab_size; i++) {
		PDATA_LOG("ocv_tab i=%d x:%d,y:%d\n", i, pdata->ocv_tab[i].x,
			  pdata->ocv_tab[i].y);
	}
	for (i = 0; i < pdata->temp_tab_size; i++) {
		PDATA_LOG("temp_tab_size i=%d x:%d,y:%d\n", i,
			  pdata->temp_tab[i].x, pdata->temp_tab[i].y);
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
	int ret, i, temp;

	pdata = devm_kzalloc(&pdev->dev,
			     sizeof(struct sprd_battery_platform_data),
			     GFP_KERNEL);
	if (!pdata) {
		return NULL;
	}

	pdata->gpio_cv_state = (uint32_t) of_get_named_gpio(np, "gpios", 1);
	pdata->gpio_vchg_ovi = (uint32_t) of_get_named_gpio(np, "gpios", 2);
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
	ret = of_property_read_u32(np, "fgu-mode", &pdata->fgu_mode);
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

	//print_pdata(sprdbat_data->pdata);

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

	data->gpio_chg_cv_state = data->pdata->gpio_cv_state;
	ret = gpio_request(data->gpio_chg_cv_state, "chg_cv_state");
	if (ret) {
		dev_err(&pdev->dev, "failed to request gpio: %d\n", ret);
		goto err_io_cv_request;
	}
	gpio_direction_input(data->gpio_chg_cv_state);
	data->irq_chg_cv_state = gpio_to_irq(data->gpio_chg_cv_state);

	set_irq_flags(data->irq_chg_cv_state, IRQF_VALID | IRQF_NOAUTOEN);
	//irq_set_irq_type(data->irq_chg_cv_state, IRQ_TYPE_LEVEL_HIGH);
	ret = request_irq(data->irq_chg_cv_state, sprdbat_chg_cv_irq,
			  IRQF_NO_SUSPEND, "sprdbat_chg_cv_state", data);
	if (ret)
		goto err_request_irq_cv_failed;

	data->gpio_vchg_ovi = data->pdata->gpio_vchg_ovi;
	ret = gpio_request(data->gpio_vchg_ovi, "vchg_ovi");
	if (ret) {
		dev_err(&pdev->dev, "failed to request gpio: %d\n", ret);
		goto err_io_ovi_request;
	}
	gpio_direction_input(data->gpio_vchg_ovi);
	data->irq_vchg_ovi = gpio_to_irq(data->gpio_vchg_ovi);

	set_irq_flags(data->irq_vchg_ovi, IRQF_VALID | IRQF_NOAUTOEN);
	ret = request_irq(data->irq_vchg_ovi, sprdbat_vchg_ovi_irq,
			  IRQF_NO_SUSPEND, "sprdbat_vchg_ovi", data);
	if (ret)
		goto err_request_irq_ovi_failed;
	mutex_init(&data->lock);

	wake_lock_init(&(data->charger_plug_out_lock), WAKE_LOCK_SUSPEND,
		       "charger_plug_out_lock");

	INIT_DELAYED_WORK(&data->cv_irq_work, sprdbat_cv_irq_works);
	INIT_DELAYED_WORK(&data->battery_work, sprdbat_battery_works);
	INIT_DELAYED_WORK(&data->battery_sleep_work,
			  sprdbat_battery_sleep_works);
	INIT_WORK(&data->ovi_irq_work, sprdbat_ovi_irq_works);

	INIT_DELAYED_WORK(&sprdbat_charge_work, sprdbat_charge_works);
	data->charge_work = &sprdbat_charge_work;
	data->monitor_wqueue = create_singlethread_workqueue("sprdbat_monitor");
	sprdchg_timer_init(sprdbat_timer_handler, data);

	sprdchg_set_chg_ovp(data->pdata->ovp_stop);

	sprdchg_init(pdev);
	sprdfgu_init(pdev);

#ifdef CONFIG_LEDS_TRIGGERS
	data->charging_led.name = "sprdbat_charging_led";
	data->charging_led.default_trigger = "battery-charging";
	data->charging_led.brightness_set = sprdchg_led_brightness_set;
	led_classdev_register(&pdev->dev, &data->charging_led);
#endif

#ifdef SPRDBAT_TWO_CHARGE_CHANNEL
	sprdchg_charge_init_ext(pdev);
	INIT_DELAYED_WORK(&sprdbat_charge_work_ext, sprdbat_charge_works_ext);

#ifndef CONFIG_OF
	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	data->gpio_charger_detect = res->start;
#else
	data->gpio_charger_detect = of_get_named_gpio(np, "gpios", 0);
#endif
	gpio_request(data->gpio_charger_detect, "sprd_charger_detect");
	gpio_direction_input(data->gpio_charger_detect);
	data->irq_charger_detect = gpio_to_irq(data->gpio_charger_detect);

	ret = request_irq(data->irq_charger_detect, sprdbat_chg_detect_int,
			  IRQF_SHARED | IRQF_TRIGGER_HIGH,
			  "sprdbat_charger_detect", data);
	INIT_WORK(&sprdbat_chg_detect_work, sprdbat_chg_detect_works);
#endif
	sprdbat_info_init(data);
	sprdbat_notifier.notifier_call = sprdbat_fgu_event;
	sprdfgu_register_notifier(&sprdbat_notifier);
	usb_register_hotplug_callback(&power_cb);

	schedule_delayed_work(&data->battery_work,
			      sprdbat_data->pdata->bat_polling_time * HZ);
	SPRDBAT_DEBUG("sprdbat_probe----------end\n");
	return 0;

err_request_irq_ovi_failed:
	gpio_free(data->gpio_vchg_ovi);
err_io_ovi_request:
	free_irq(data->irq_chg_cv_state, data);
err_request_irq_cv_failed:
	gpio_free(data->gpio_chg_cv_state);
err_io_cv_request:
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

	sprdbat_remove_caliberate_attr(data->battery.dev);
	power_supply_unregister(&data->battery);
	power_supply_unregister(&data->ac);
	power_supply_unregister(&data->usb);

	free_irq(data->irq_vchg_ovi, data);
	free_irq(data->irq_chg_cv_state, data);
	gpio_free(data->gpio_vchg_ovi);
	gpio_free(data->gpio_chg_cv_state);
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

MODULE_AUTHOR("Mingwei.Zhang@spreadtrum.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Battery and charger driver for SC2713");
