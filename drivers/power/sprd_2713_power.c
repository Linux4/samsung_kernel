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

#define SPRDBAT__DEBUG
#ifdef SPRDBAT__DEBUG
#define SPRDBAT_DEBUG(format, arg...) printk("sprdbat: " "---" format, ## arg)
#else
#define SPRDBAT_DEBUG(format, arg...)
#endif
#if defined(CONFIG_ARCH_SCX15)
#if 0
int sprd_thm_temp_read(u32 sensor)
{
	return 0;
}
#endif
#endif

#define SPRDBAT_CV_TRIGGER_CURRENT		1/2
#define SPRDBAT_ONE_PERCENT_TIME   30
#define SPRDBAT_VALID_CAP   15

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
static uint32_t sprdbat_trickle_chg;
static uint32_t sprdbat_start_chg;
static uint32_t poweron_capacity;
static struct notifier_block sprdbat_notifier;
static uint32_t sprdbat_cccv_cal_from_chip = 0;

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
		sprdbat_change_module_state(SPRDBAT_ADP_PLUGOUT_E);
#ifdef SPRDBAT_TWO_CHARGE_CHANNEL
		sprdbat_data->bat_info.ac_online = 0;
		sprdbat_data->bat_info.usb_online = 0;
		sprdchg_stop_charge();
		sprdchg_stop_charge_ext();
#endif
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
			sprdchg_set_cccvpoint(sprdbat_data->
					      bat_info.cccv_point);
			if ((!sprdbat_start_chg) && sprdbat_cv_irq_dis && sprdfgu_is_new_chip()) {
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
			if (abs(temp) > SPRDBAT_VALID_CAP || 0 == set_value) {
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
	int temp_value;
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

static void sprdbat_param_init(struct sprdbat_drivier_data *data)
{
	data->bat_param.chg_end_vol_pure = SPRDBAT_CHG_END_VOL_PURE;
	data->bat_param.chg_end_vol_h = SPRDBAT_CHG_END_H;
	data->bat_param.chg_end_vol_l = SPRDBAT_CHG_END_L;
	data->bat_param.rechg_vol = SPRDBAT_RECHG_VOL;
	data->bat_param.chg_end_cur = SPRDBAT_CHG_END_CUR;
	data->bat_param.adp_cdp_cur = SPRDBAT_CDP_CUR_LEVEL;
	data->bat_param.adp_dcp_cur = SPRDBAT_DCP_CUR_LEVEL;
	data->bat_param.adp_sdp_cur = SPRDBAT_SDP_CUR_LEVEL;
	data->bat_param.ovp_stop = SPRDBAT_OVP_STOP_VOL;
	data->bat_param.ovp_restart = SPRDBAT_OVP_RESTERT_VOL;
	data->bat_param.otp_high_stop = SPRDBAT_OTP_HIGH_STOP;
	data->bat_param.otp_high_restart = SPRDBAT_OTP_HIGH_RESTART;
	data->bat_param.otp_low_stop = SPRDBAT_OTP_LOW_STOP;
	data->bat_param.otp_low_restart = SPRDBAT_OTP_LOW_RESTART;
	data->bat_param.chg_timeout = SPRDBAT_CHG_NORMAL_TIMEOUT;
	return;
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
	data->bat_info.capacity = sprdfgu_poweron_capacity();	//~0;
	poweron_capacity = sprdfgu_poweron_capacity();
	data->bat_info.soc = sprdfgu_read_soc();
	data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
	data->bat_info.cur_temp = sprdbat_read_temp();
	data->bat_info.bat_current = sprdfgu_read_batcurrent();
	data->bat_info.chging_current = 0;
	data->bat_info.chg_current_type = SPRDBAT_SDP_CUR_LEVEL;
	{
		uint32_t cv_point;
		extern int sci_efuse_cccv_cal_get(unsigned int *p_cal_data);
		if (sci_efuse_cccv_cal_get(&cv_point)) {
			SPRDBAT_DEBUG("cccv_point efuse:%d\n", cv_point);
			data->bat_info.cccv_point = sprdchg_tune_endvol_cccv(data->bat_param.chg_end_vol_pure, cv_point);
			SPRDBAT_DEBUG("cccv_point sprdchg_tune_endvol_cccv:%d\n", data->bat_info.cccv_point);
			sprdbat_cccv_cal_from_chip = 1;
		} else {
			data->bat_info.cccv_point = SPRDBAT_CCCV_DEFAULT;
			SPRDBAT_DEBUG("cccv_point default\n");
		}
	}
	return;
}

static int sprdbat_start_charge(void)
{
	struct timespec cur_time;

	if (sprdbat_data->bat_info.adp_type == ADP_TYPE_CDP) {
		sprdbat_data->bat_info.chg_current_type = SPRDBAT_CDP_CUR_LEVEL;
	} else if (sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP) {
		sprdbat_data->bat_info.chg_current_type = SPRDBAT_DCP_CUR_LEVEL;
	} else {
		sprdbat_data->bat_info.chg_current_type = SPRDBAT_SDP_CUR_LEVEL;
	}
	sprdchg_set_chg_cur(sprdbat_data->bat_info.chg_current_type);
	sprdchg_set_cccvpoint(sprdbat_data->bat_info.cccv_point);

	get_monotonic_boottime(&cur_time);
	sprdbat_data->bat_info.chg_start_time = cur_time.tv_sec;
	sprdchg_start_charge();
	sprdbat_average_cnt = 0;
	SPRDBAT_DEBUG
	    ("sprdbat_start_charge bat_health:%d,chg_start_time:%ld,chg_current_type:%d\n",
	     sprdbat_data->bat_info.bat_health,
	     sprdbat_data->bat_info.chg_start_time,
	     sprdbat_data->bat_info.chg_current_type);

	sprdbat_trickle_chg = 0;
	sprdbat_start_chg = 1;

	return 0;
}

static int sprdbat_stop_charge(void)
{
	unsigned long irq_flag = 0;
	sprdbat_data->bat_info.chg_start_time = 0;

	sprdchg_stop_charge();
	local_irq_save(irq_flag);
	if (!sprdbat_cv_irq_dis) {
		disable_irq_nosync(sprdbat_data->irq_chg_cv_state);
		sprdbat_cv_irq_dis = 1;
	}
	local_irq_restore(irq_flag);
	SPRDBAT_DEBUG("sprdbat_stop_charge\n");
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
	} else if (sprdbat_data->
		   bat_info.chg_stop_flags & SPRDBAT_CHG_END_OTP_OVERHEAT_BIT) {
		sprdbat_data->bat_info.bat_health =
		    POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (sprdbat_data->
		   bat_info.chg_stop_flags & SPRDBAT_CHG_END_OTP_COLD_BIT) {
		sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_COLD;
	} else if (sprdbat_data->
		   bat_info.chg_stop_flags & SPRDBAT_CHG_END_OVP_BIT) {
		sprdbat_data->bat_info.bat_health =
		    POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (sprdbat_data->
		   bat_info.chg_stop_flags & (SPRDBAT_CHG_END_TIMEOUT_BIT |
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
		if ((flag == SPRDBAT_CHG_END_FULL_BIT)
		    || (flag == SPRDBAT_CHG_END_TIMEOUT_BIT))
			sprdbat_data->bat_info.module_state =
			    POWER_SUPPLY_STATUS_FULL;
		else
			sprdbat_data->bat_info.module_state =
			    POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
}

static void sprdbat_change_module_state(uint32_t event)
{
	SPRDBAT_DEBUG("sprdbat_change_module_state event :0x%x\n", event);
	switch (event) {
	case SPRDBAT_ADP_PLUGIN_E:
		sprdbat_data->bat_param.chg_timeout =
		    SPRDBAT_CHG_NORMAL_TIMEOUT;
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
		sprdbat_data->bat_param.chg_timeout =
		    SPRDBAT_CHG_SPECIAL_TIMEOUT;
		_sprdbat_set_stopflags(SPRDBAT_CHG_END_FULL_BIT);
		break;
	case SPRDBAT_RECHARGE_E:
		_sprdbat_clear_stopflags(SPRDBAT_CHG_END_FULL_BIT);
		break;
	case SPRDBAT_CHG_TIMEOUT_E:
		sprdbat_data->bat_param.chg_timeout =
		    SPRDBAT_CHG_SPECIAL_TIMEOUT;
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

	sprdchg_timer_enable(SPRDBAT_POLL_TIMER_NORMAL);

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
	SPRDBAT_DEBUG("sprdbat_timer_handler----------vbat_vol %d,ocv:%d\n",
		      sprdbat_data->bat_info.vbat_vol,
		      sprdbat_data->bat_info.vbat_ocv);
	SPRDBAT_DEBUG("sprdbat_timer_handler----------bat_current %d\n",
		      sprdbat_data->bat_info.bat_current);

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

	if (vbat_now <= sprdbat_data->bat_param.chg_end_vol_pure) {
		cv = ((sprdbat_data->bat_param.chg_end_vol_pure -
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
	    sprdbat_data->bat_param.chg_end_vol_pure) {
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
		    (SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->
		     bat_info.chg_stop_flags)) {
			SPRDBAT_DEBUG("SPRDBAT_OVI_STOP_E\n");
			sprdbat_change_module_state(SPRDBAT_OVI_STOP_E);
		}
	} else {
		if ((SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->
		     bat_info.chg_stop_flags)) {
			SPRDBAT_DEBUG("SPRDBAT_OVI_RESTART_E\n");
			sprdbat_change_module_state(SPRDBAT_OVI_RESTART_E);
		}
	}
	mutex_unlock(&sprdbat_data->lock);
}

static void sprdbat_chg_print_log(void)
{
	struct timespec cur_time;
	int value;

	SPRDBAT_DEBUG("sprdbat_chg_print_log\n");

	value = gpio_get_value(sprdbat_data->gpio_chg_cv_state);

	SPRDBAT_DEBUG("sprdbat_charge_works cv gpio:%d\n", value);

	get_monotonic_boottime(&cur_time);

	SPRDBAT_DEBUG("cur_time:%ld\n", cur_time.tv_sec);
	SPRDBAT_DEBUG("adp_type:%d\n", sprdbat_data->bat_info.adp_type);
	SPRDBAT_DEBUG("bat_health:%d\n", sprdbat_data->bat_info.bat_health);
	SPRDBAT_DEBUG("module_state:%d\n", sprdbat_data->bat_info.module_state);
	SPRDBAT_DEBUG("chg_stop_flags0x:%x\n",
		      sprdbat_data->bat_info.chg_stop_flags);
	SPRDBAT_DEBUG("chg_start_time:%ld\n",
		      sprdbat_data->bat_info.chg_start_time);
	SPRDBAT_DEBUG("capacity:%d\n", sprdbat_data->bat_info.capacity);
	SPRDBAT_DEBUG("soc:%d\n", sprdbat_data->bat_info.soc);
	SPRDBAT_DEBUG("vbat_vol:%d\n", sprdbat_data->bat_info.vbat_vol);
	SPRDBAT_DEBUG("vbat_ocv:%d\n", sprdbat_data->bat_info.vbat_ocv);
	SPRDBAT_DEBUG("cur_temp:%d\n", sprdbat_data->bat_info.cur_temp);
	SPRDBAT_DEBUG("bat_current:%d\n", sprdbat_data->bat_info.bat_current);
	SPRDBAT_DEBUG("chging_current:%d\n",
		      sprdbat_data->bat_info.chging_current);
	SPRDBAT_DEBUG("chg_current_type:%d\n",
		      sprdbat_data->bat_info.chg_current_type);
	SPRDBAT_DEBUG("cccv_point:%d\n", sprdbat_data->bat_info.cccv_point);
	SPRDBAT_DEBUG("aux vbat vol:%d\n", sprdchg_read_vbat_vol());
	SPRDBAT_DEBUG("vchg_vol:%d\n", sprdchg_read_vchg_vol());
	SPRDBAT_DEBUG("sprdbat_chg_print_log end\n");

}

static void sprdbat_print_battery_log(void)
{
	struct timespec cur_time;

	SPRDBAT_DEBUG("sprdbat_print_battery_log\n");
#if 0
	{
		extern int sprd_thm_temp_read(u32 sensor);
		printk(KERN_ERR
		       "CHIP THM:arm sensor temp:%d,pmic sensor temp:%d \n",
		       sprd_thm_temp_read(0), sprd_thm_temp_read(1));
	}
#endif
	get_monotonic_boottime(&cur_time);

	SPRDBAT_DEBUG("CHIP ID is new chip?:%d\n", sprdfgu_is_new_chip());
	SPRDBAT_DEBUG("cur_time:%ld\n", cur_time.tv_sec);
	SPRDBAT_DEBUG("module_state:%d\n", sprdbat_data->bat_info.module_state);
	SPRDBAT_DEBUG("capacity:%d\n", sprdbat_data->bat_info.capacity);
	SPRDBAT_DEBUG("vbat_vol:%d\n", sprdbat_data->bat_info.vbat_vol);
	SPRDBAT_DEBUG("vbat_ocv:%d\n", sprdbat_data->bat_info.vbat_ocv);
	SPRDBAT_DEBUG("bat_current:%d\n", sprdbat_data->bat_info.bat_current);
	SPRDBAT_DEBUG("aux vbat vol:%d\n", sprdchg_read_vbat_vol());
	SPRDBAT_DEBUG("sprdbat_print_battery_log end\n");

}

static void sprdbat_update_capacty(void)
{
	uint32_t fgu_capacity = sprdfgu_read_capacity();
	uint32_t flush_time = 0;
	struct timespec cur_time;
	if (sprdbat_data->bat_info.capacity == ~0) {
		return;
	}
	get_monotonic_boottime(&cur_time);
	flush_time = cur_time.tv_sec - sprdbat_update_capacity_time;

	switch (sprdbat_data->bat_info.module_state) {
	case POWER_SUPPLY_STATUS_CHARGING:
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (fgu_capacity < sprdbat_data->bat_info.capacity) {
			fgu_capacity = sprdbat_data->bat_info.capacity;
		} else {
			if ((fgu_capacity - sprdbat_data->bat_info.capacity) >=
			    (flush_time / SPRDBAT_ONE_PERCENT_TIME)) {
				fgu_capacity =
				    sprdbat_data->bat_info.capacity +
				    flush_time / SPRDBAT_ONE_PERCENT_TIME;
			}
		}
		if (fgu_capacity >= 100) {
			fgu_capacity = 99;
		}

		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		if (fgu_capacity > sprdbat_data->bat_info.capacity) {
			fgu_capacity = sprdbat_data->bat_info.capacity;
		} else {
			if ((sprdbat_data->bat_info.capacity - fgu_capacity) >=
			    (flush_time / SPRDBAT_ONE_PERCENT_TIME)) {
				fgu_capacity =
				    sprdbat_data->bat_info.capacity -
				    flush_time / SPRDBAT_ONE_PERCENT_TIME;
			}
		}
		break;
	case POWER_SUPPLY_STATUS_FULL:
		if (fgu_capacity != 100) {
			fgu_capacity = 100;
		}
		break;
	default:
		BUG_ON(1);
		break;
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
	sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	sprdbat_data->bat_info.cur_temp = sprdbat_read_temp();
	sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
	sprdbat_data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();

	sprdbat_update_capacty();

	mutex_unlock(&sprdbat_data->lock);
	sprdbat_print_battery_log();
	if (sprdbat_data->bat_info.module_state == POWER_SUPPLY_STATUS_CHARGING) {
		schedule_delayed_work(&sprdbat_data->battery_work,
				      SPRDBAT_CAPACITY_MONITOR_FAST);
	} else {
		schedule_delayed_work(&sprdbat_data->battery_work,
				      SPRDBAT_CAPACITY_MONITOR_NORMAL);
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

static uint32_t temp_trigger_cnt;
static void sprdbat_temp_monitor(void)
{
	sprdbat_data->bat_info.cur_temp = sprdbat_read_temp();
	SPRDBAT_DEBUG("sprdbat_temp_monitor temp:%d\n",
		      sprdbat_data->bat_info.cur_temp);
	if (sprdbat_data->
	    bat_info.chg_stop_flags & SPRDBAT_CHG_END_OTP_COLD_BIT) {
		if (sprdbat_data->bat_info.cur_temp >
		    sprdbat_data->bat_param.otp_low_restart)
			sprdbat_change_module_state(SPRDBAT_OTP_COLD_RESTART_E);

		SPRDBAT_DEBUG("otp_low_restart:%d\n",
			      sprdbat_data->bat_param.otp_low_restart);
	} else if (sprdbat_data->
		   bat_info.chg_stop_flags & SPRDBAT_CHG_END_OTP_OVERHEAT_BIT) {
		if (sprdbat_data->bat_info.cur_temp <
		    sprdbat_data->bat_param.otp_high_restart)
			sprdbat_change_module_state
			    (SPRDBAT_OTP_OVERHEAT_RESTART_E);

		SPRDBAT_DEBUG("otp_high_restart:%d\n",
			      sprdbat_data->bat_param.otp_high_restart);
	} else {
		if (sprdbat_data->bat_info.cur_temp <
		    sprdbat_data->bat_param.otp_low_stop
		    || sprdbat_data->bat_info.cur_temp >
		    sprdbat_data->bat_param.otp_high_stop) {
			SPRDBAT_DEBUG("otp_low_stop:%d,otp_high_stop:%d\n",
				      sprdbat_data->bat_param.otp_low_stop,
				      sprdbat_data->bat_param.otp_high_stop);
			temp_trigger_cnt++;
			if (temp_trigger_cnt > SPRDBAT_TEMP_TRIGGER_TIMES) {
				if (sprdbat_data->bat_info.cur_temp <
				    sprdbat_data->bat_param.otp_low_stop) {
					sprdbat_change_module_state
					    (SPRDBAT_OTP_COLD_STOP_E);
				} else {
					sprdbat_change_module_state
					    (SPRDBAT_OTP_OVERHEAT_STOP_E);
				}
				sprdchg_timer_enable(SPRDBAT_POLL_TIMER_NORMAL);
				temp_trigger_cnt = 0;
			} else {
				sprdchg_timer_enable(SPRDBAT_POLL_TIMER_TEMP);
			}
		} else {
			if (temp_trigger_cnt) {
				sprdchg_timer_enable(SPRDBAT_POLL_TIMER_NORMAL);
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
	    sprdbat_data->bat_param.chg_timeout)
		return 1;
	else
		return 0;
}

static void sprdbat_charge_works(struct work_struct *work)
{
	uint32_t cur;
	unsigned long irq_flag = 0;

	SPRDBAT_DEBUG("sprdbat_charge_works----------start\n");

	mutex_lock(&sprdbat_data->lock);

	if (sprdbat_data->bat_info.module_state ==
	    POWER_SUPPLY_STATUS_DISCHARGING) {
		mutex_unlock(&sprdbat_data->lock);
		return;
	}

	if (sprdbat_start_chg) {
		sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
		sprdbat_data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
		sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
		if (!sprdbat_cccv_cal_from_chip) {
			local_irq_save(irq_flag);
			if (sprdbat_cv_irq_dis && sprdfgu_is_new_chip()) {
				sprdbat_cv_irq_dis = 0;
				enable_irq(sprdbat_data->irq_chg_cv_state);
			}
			local_irq_restore(irq_flag);
		}
		sprdbat_start_chg = 0;
	}

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
			    sprdbat_data->bat_param.rechg_vol) {
				sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
			} else {
				sprdbat_data->bat_param.chg_timeout =
				    SPRDBAT_CHG_SPECIAL_TIMEOUT;
				sprdbat_change_module_state
				    (SPRDBAT_CHG_TIMEOUT_E);
			}
		}
		if (sprdbat_cccv_cal_from_chip) {
			SPRDBAT_DEBUG("sprdbat_cccv_cal_from_chip == 1\n");
			int value = gpio_get_value(sprdbat_data->gpio_chg_cv_state);
			if (value && (sprdbat_data->bat_info.bat_current <
			    sprdbat_data->bat_param.chg_end_cur)) {
				sprdbat_trickle_chg = 0;
				SPRDBAT_DEBUG("SPRDBAT_CHG_FULL_E\n");
				sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
			}
		} else {
			SPRDBAT_DEBUG("sprdbat_cccv_cal_from_chip == 0\n");
			if ((sprdbat_data->bat_info.vbat_vol >
			     sprdbat_data->bat_param.chg_end_vol_l
			     || sprdbat_trickle_chg)
			    && sprdbat_data->bat_info.bat_current <
			    sprdbat_data->bat_param.chg_end_cur) {
				sprdbat_trickle_chg = 0;
				SPRDBAT_DEBUG("SPRDBAT_CHG_FULL_E\n");
				sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
			}
		}
		//cccv point is high
		if (sprdbat_data->bat_info.vbat_vol >
		    sprdbat_data->bat_param.chg_end_vol_h) {
			SPRDBAT_DEBUG("cccv point is high\n");
			sprdbat_adjust_cccvpoint(sprdbat_data->bat_info.
						 vbat_vol);
		}

		/*the purpose of this code is the same as cv irq handler,
		   if cv irq is very accurately, this code can be deleted */
		//cccv point is low
		cur = sprdchg_read_chg_current();
		sprdchg_put_chgcur(cur);
		sprdbat_data->bat_info.chging_current =
		    sprdchg_get_chgcur_ave();

		SPRDBAT_DEBUG
		    ("sprdbat_charge_works----------cur: %d,chging_current:%d\n",
		     cur, sprdbat_data->bat_info.chging_current);

		sprdbat_average_cnt++;
		if (sprdbat_average_cnt == SPRDBAT_AVERAGE_COUNT) {
			uint32_t triggre_current =
			    sprdbat_data->bat_info.chg_current_type *
			    SPRDBAT_CV_TRIGGER_CURRENT;
			sprdbat_average_cnt = 0;
			if (sprdbat_data->bat_info.vbat_vol <
			    sprdbat_data->bat_param.chg_end_vol_pure
			    && sprdbat_data->bat_info.chging_current <
			    triggre_current) {
				SPRDBAT_DEBUG
				    ("turn high cccv point vol:%d,chg_cur:%d,trigger_cur:%d",
				     sprdbat_data->bat_info.vbat_vol,
				     sprdbat_data->bat_info.chging_current,
				     triggre_current);
				sprdbat_adjust_cccvpoint(sprdbat_data->
							 bat_info.vbat_vol);
			}
		}

	}

	if (sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_TIMEOUT_BIT) {
		SPRDBAT_DEBUG("SPRDBAT_CHG_TIMEOUT_RESTART_E\n");
		sprdbat_change_module_state(SPRDBAT_CHG_TIMEOUT_RESTART_E);
	}

	if (sprdbat_data->bat_info.chg_stop_flags & SPRDBAT_CHG_END_FULL_BIT) {
		if (sprdbat_data->bat_info.vbat_ocv <
		    sprdbat_data->bat_param.rechg_vol) {
			SPRDBAT_DEBUG("SPRDBAT_RECHARGE_E\n");
			sprdbat_change_module_state(SPRDBAT_RECHARGE_E);
		}
	}

	mutex_unlock(&sprdbat_data->lock);
	sprdbat_chg_print_log();
	SPRDBAT_DEBUG("sprdbat_charge_works----------end\n");
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
			    sprdbat_data->bat_param.rechg_vol) {
				sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
			} else {
				sprdbat_data->bat_param.chg_timeout =
				    SPRDBAT_CHG_SPECIAL_TIMEOUT;
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
		    sprdbat_data->bat_param.rechg_vol) {
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
		    (SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->
		     bat_info.chg_stop_flags)) {
			SPRDBAT_DEBUG("SPRDBAT_OVI_STOP_E\n");
			sprdbat_change_module_state(SPRDBAT_OVI_STOP_E);
		}
		break;
	case SPRDBAT_CHG_EVENT_EXT_OVI_RESTART:
		if ((SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->
		     bat_info.chg_stop_flags)) {
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
	sprdchg_timer_enable(SPRDBAT_POLL_TIMER_NORMAL);
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

static int sprdbat_probe(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct sprdbat_drivier_data *data;
	struct resource *res = NULL;

	SPRDBAT_DEBUG("sprdbat_probe\n");

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err_data_alloc_failed;
	}
	data->dev = &pdev->dev;
	platform_set_drvdata(pdev, data);
	sprdbat_data = data;
	sprdbat_param_init(data);

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

	res = platform_get_resource(pdev, IORESOURCE_IO, 1);
	if (unlikely(!res)) {
		dev_err(&pdev->dev, "not io resource\n");
		goto err_io_resource;
	}

	if (sprdfgu_is_new_chip()) {
		SPRDBAT_DEBUG("new chip\n");
		data->gpio_chg_cv_state = res->start;
	} else {
		SPRDBAT_DEBUG("old chip\n");
		data->gpio_chg_cv_state = A_GPIO_START + 3;
	}

	res = platform_get_resource(pdev, IORESOURCE_IO, 2);
	if (unlikely(!res)) {
		dev_err(&pdev->dev, "not io resource\n");
		goto err_io_resource;
	}
	data->gpio_vchg_ovi = res->start;

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

	sprdchg_set_chg_ovp(data->bat_param.ovp_stop);

	sprdchg_init();
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

	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	data->gpio_charger_detect = res->start;

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
			      SPRDBAT_CAPACITY_MONITOR_NORMAL);
	SPRDBAT_DEBUG("sprdbat_probe----------end\n");
	return 0;

err_request_irq_ovi_failed:
	gpio_free(data->gpio_vchg_ovi);
err_io_ovi_request:
	free_irq(data->irq_chg_cv_state, data);
err_request_irq_cv_failed:
	gpio_free(data->gpio_chg_cv_state);
err_io_cv_request:
err_io_resource:
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

static struct platform_driver sprdbat_driver = {
	.probe = sprdbat_probe,
	.remove = sprdbat_remove,
	.suspend = sprdbat_suspend,
	.resume = sprdbat_resume,
	.driver = {
		   .name = "sprd-battery"}
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
