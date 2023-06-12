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
#include <mach/board.h>
#include <linux/device.h>

#include <linux/slab.h>
#include <linux/jiffies.h>
#include "sprd_battery.h"
#include <mach/usb.h>


#if 1 //temporary
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/pinmap.h>
#endif



#define CONFIG_PLUG_WAKELOCK_TIME_SEC 3
#define SPRDBAT__DEBUG
#ifdef SPRDBAT__DEBUG
#define SPRDBAT_DEBUG(format, arg...) printk("sprdbat: " "---" format, ## arg)
#else
#define SPRDBAT_DEBUG(format, arg...)
#define CONFIG_NOTIFY_BY_JIGON
#define CONFIG_NOTIFY_BY_USB
#endif

#if defined(CONFIG_ARCH_SCX15)
int sprd_thm_temp_read(u32 sensor)
{
    return 0;
}
#endif

#define SPRDBAT_CV_TRIGGER_CURRENT		1/2
#define SPRDBAT_ONE_PERCENT_TIME   30
#define SPRDBAT_VALID_CAP   10

extern struct device *batt_dev;

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


#if defined(CONFIG_SEC_CHARGING_FEATURE)
#include <linux/spa_power.h>
#include <linux/spa_agent.h>
#endif

#ifdef CONFIG_STC3115_FUELGAUGE
#include <linux/stc3115_battery.h>
#endif

#define DETECT_CHG_ADC 25
#define DETECT_BATT_VF_ADC 0x3F0

#ifndef EIC_JIG_ON
#define EIC_JIG_ON 1
#endif

#if defined(CONFIG_TOUCHSCREEN_IST30XXB)
extern void charger_enable(int enable);
#else
void charger_enable(int enable) {}
#endif

static struct sprdbat_drivier_data *sprdbat_data;
static uint32_t sprdbat_cv_irq_dis = 1;
static uint32_t sprdbat_average_cnt;
static unsigned long sprdbat_update_capacity_time;
static uint32_t sprdbat_trickle_chg;
static uint32_t sprdbat_start_chg;
static uint32_t poweron_capacity;
static struct notifier_block sprdbat_notifier;

 struct work_struct gpio_detect_work;
extern unsigned int lp_boot_mode;
extern int read_ocv();
extern int read_voltage(int *vbat);
extern void STC311x_Reset(void);
extern int sprdchg_get_battery_capacity(void);
extern int sprdfgu_read_vbat_ocv_pure(int *volt);
static int sprdchg_get_charger_type(void);
static int sprdchg_get_current(unsigned int opt);
extern void sprdchg_set_chg_cur(uint32_t chg_current);
extern int sprdchg_set_charge(unsigned int en);
extern int sprdchg_read_temp(void);
extern uint32_t   sprdchg_read_vbat_vol(void);
extern uint32_t sprdchg_read_chg_current(void);
#if CONFIG_SS_SPRD_TEMP
extern int sprdchg_read_wpa_temp(int retadc);
extern int sprdchg_read_dcxo_temp(int retadc);
#endif
extern struct sprdbat_auxadc_cal adc_cal;
extern int musb_info_handler(struct notifier_block *nb, unsigned long event, void *para);

//#ifndef CONFIG_STC3117_FUELGAUGE
#define BATT_DETECT 43
//#endif
static int sprdbat_ctrl_fg(void* data);
static void sprdbat_change_module_state(uint32_t event);

static int sprdchg_get_charger_type()
{
        pr_info("%s\n", __func__);
        if(sprdbat_data->usb_online)
                return POWER_SUPPLY_TYPE_USB;
        else if(sprdbat_data->ac_online)
                return POWER_SUPPLY_TYPE_USB_DCP;
        else
                return POWER_SUPPLY_TYPE_BATTERY;


}



uint32_t sprd_get_vbat_voltage(void)
{
	return sprdbat_read_vbat_vol();
}
#ifdef CONFIG_SPRD_BATTERY_INTERFACE
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
			val->intval =
			    (data->bat_info.adp_type == ADP_TYPE_DCP) ? 1 : 0;
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
		val->intval = (data->bat_info.adp_type == ADP_TYPE_CDP
			       || data->bat_info.adp_type ==
			       ADP_TYPE_SDP) ? 1 : 0;
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
#endif
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
};

#if CONFIG_SS_SPRD_TEMP
static ssize_t temp_wpa_adc_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	int temp_adc ;
	temp_adc=sprdchg_read_wpa_temp(1);
	return sprintf(buf,"%d\n",temp_adc);
}

static ssize_t temp_dcxo_adc_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	int temp_adc ;
	temp_adc=sprdchg_read_wpa_temp(1);
	return sprintf(buf,"%d\n",temp_adc);
}

static ssize_t temp_wpa_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	int temp ;
	temp=sprdchg_read_wpa_temp(0);
	return sprintf(buf,"%d\n",temp);
}

static ssize_t temp_dcxo_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	int temp ;
	temp=sprdchg_read_dcxo_temp(0);
	return sprintf(buf,"%d\n",temp);
}

static struct device_attribute ss_sprd_temp_attrs[]=
{
	__ATTR(temp_dcxo_adc, S_IRUGO, temp_dcxo_adc_show, NULL),
	__ATTR(temp_wpa_adc, S_IRUGO, temp_wpa_adc_show, NULL),
	__ATTR(temp_wpa,  S_IRUGO, temp_wpa_show, NULL),
	__ATTR(temp_dcxo,  S_IRUGO,temp_dcxo_show, NULL),
};
#endif

static int sprdbat_ctrl_fg (void *data)
{
	int ret = 0;

	printk("%s %d\n", __func__, (int)data);
#ifdef CONFIG_STC3115_FUELGAUGE
{
	u32 temp;
	STC311x_Reset();
	read_voltage(&temp);
	read_soc(&temp);
}
#endif

	return ret;
}

extern struct device_attribute sprd_caliberate[];
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
		local_irq_save(irq_flag);
		sprdbat_data->bat_info.cccv_point = set_value;
		sprdchg_set_cccvpoint(sprdbat_data->bat_info.cccv_point);

		//if (sprdbat_cv_irq_dis && sprdfgu_is_new_chip()) {
		if (sprdbat_cv_irq_dis){
			sprdbat_cv_irq_dis = 0;
			sprdbat_trickle_chg = 0;
			enable_irq(sprdbat_data->irq_chg_cv_state);
		}
		local_irq_restore(irq_flag);
		break;
	case SAVE_CAPACITY:
		{
#if 0
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
#endif
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
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

int sprdbat_creat_caliberate_attr(struct device *dev)
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

#ifdef CONFIG_SPRD_BATTERY_INTERFACE
static int sprdbat_remove_caliberate_attr(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_caliberate); i++) {
		device_remove_file(dev, &sprd_caliberate[i]);
	}
	return 0;
}
#endif

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
	int ocv;
	data->bat_info.adp_type = ADP_TYPE_UNKNOW;
	data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
	data->bat_info.module_state = POWER_SUPPLY_STATUS_DISCHARGING;
	data->bat_info.chg_stop_flags = SPRDBAT_CHG_END_NONE_BIT;
	data->bat_info.chg_start_time = 0;
	data->ac_online=0;
	data->usb_online=0;
	get_monotonic_boottime(&cur_time);
	sprdbat_update_capacity_time = cur_time.tv_sec;
//removing all the fgu functions
	data->bat_info.capacity = sprdchg_get_battery_capacity();//~0;//modify in later
	//poweron_capacity = sprdfgu_poweron_capacity();
	data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
#if 0
//#ifdef CONFIG_STC3115_FUELGAUGE
	data->bat_info.vbat_ocv = read_ocv();
//#else
        sprdfgu_read_vbat_ocv_pure(&ocv); //replace read_soc
        data->bat_info.vbat_ocv =ocv;
#endif
	data->bat_info.cur_temp = sprdbat_read_temp();
//temp
	data->bat_info.bat_current = sprdchg_read_chg_current();
	data->bat_info.chging_current = 0;
	data->bat_info.chg_current_type = SPRDBAT_SDP_CUR_LEVEL;
	{
		uint32_t cv_point;
		extern int sci_efuse_cccv_cal_get(unsigned int *p_cal_data);
		if (sci_efuse_cccv_cal_get(&cv_point)) {
			SPRDBAT_DEBUG("cccv_point efuse:%d\n", cv_point);
			data->bat_info.cccv_point = sprdchg_tune_endvol_cccv(data->bat_param.chg_end_vol_pure, cv_point);
			SPRDBAT_DEBUG("cccv_point sprdchg_tune_endvol_cccv:%d\n", data->bat_info.cccv_point);
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
		sprdchg_set_chg_cur(1);
	} else if (sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP) {
		sprdbat_data->bat_info.chg_current_type = SPRDBAT_DCP_CUR_LEVEL;
		sprdchg_set_chg_cur(1);
	} else {
		sprdbat_data->bat_info.chg_current_type = SPRDBAT_SDP_CUR_LEVEL;
		sprdchg_set_chg_cur(0);
		}
	//sprdchg_set_chg_cur(sprdbat_data->bat_info.chg_current_type);
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
		sprdbat_start_charge();
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
		sprdbat_stop_charge();
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
		//queue_work(sprdbat_data->monitor_wqueue,&sprdbat_data->charge_work);
		break;
	case SPRDBAT_ADP_PLUGOUT_E:
		sprdbat_data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
		sprdbat_data->bat_info.chg_stop_flags =
		    SPRDBAT_CHG_END_NONE_BIT;
		sprdbat_data->bat_info.module_state =
		    POWER_SUPPLY_STATUS_DISCHARGING;
		sprdbat_stop_charge();
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
	#ifdef CONFIG_SPRD_BATTERY_INTERFACE
	power_supply_changed(&sprdbat_data->battery);
	#endif
}

static int plugin_callback(int usb_cable, void *data)
{
	SPRDBAT_DEBUG("charger plug in interrupt happen\n");
	mutex_lock(&sprdbat_data->lock);
	sprdbat_data->detecting=1;
	sprdbat_data->bat_info.adp_type = sprdchg_charger_is_adapter();
	printk(" %s charger plug in interrupt happen\n",__func__);
	sprdbat_change_module_state(SPRDBAT_ADP_PLUGIN_E);
	mutex_unlock(&sprdbat_data->lock);

	irq_set_irq_type(sprdbat_data->irq_vchg_ovi, IRQ_TYPE_LEVEL_HIGH);
	enable_irq(sprdbat_data->irq_vchg_ovi);

	sprdchg_timer_enable(SPRDBAT_POLL_TIMER_NORMAL);
	SPRDBAT_DEBUG("plugin_callback:sprdbat_data->bat_info.adp_type:%d\n",
		      sprdbat_data->bat_info.adp_type);
//#ifdef CONFIG_SPRD_BATTERY_INTERFACE
	if (sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP)
	{
		sprdbat_data->ac_online=1;
		#if defined(CONFIG_SEC_CHARGING_FEATURE)
						charger_enable(1);
                        spa_event_handler(SPA_EVT_CHARGER, (void*)POWER_SUPPLY_TYPE_USB_DCP);
		#endif

		#ifdef CONFIG_SPRD_BATTERY_INTERFACE
			power_supply_changed(&sprdbat_data->ac);
		#endif
	}
	else
	{

	sprdbat_data->usb_online=1;
	#if defined(CONFIG_SEC_CHARGING_FEATURE)
				charger_enable(1);
                spa_event_handler(SPA_EVT_CHARGER, (void*)POWER_SUPPLY_TYPE_USB);
	#endif
	#ifdef CONFIG_SPRD_BATTERY_INTERFACE
		power_supply_changed(&sprdbat_data->usb);
	#endif
	}


//#endf
	mutex_lock(&sprdbat_data->lock);
        sprdbat_data->detecting=1;
        mutex_unlock(&sprdbat_data->lock);
	//sprdbat_adp_plug_nodify(1);
	SPRDBAT_DEBUG("plugin_callback: end...\n");
	return 0;
}

static int plugout_callback(int usb_cable, void *data)
{
	uint32_t adp_type = sprdbat_data->bat_info.adp_type;

	sprdbat_data->bat_info.adp_type = ADP_TYPE_UNKNOW;
	SPRDBAT_DEBUG("charger plug out interrupt happen\n");

	wake_lock_timeout(&(sprdbat_data->charger_plug_out_lock),
			  SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);
	mutex_lock(&sprdbat_data->lock);
	disable_irq_nosync(sprdbat_data->irq_vchg_ovi);

	sprdchg_timer_disable();
	sprdbat_change_module_state(SPRDBAT_ADP_PLUGOUT_E);
	mutex_unlock(&sprdbat_data->lock);
	if (adp_type == ADP_TYPE_DCP)
	{

		#ifdef CONFIG_SPRD_BATTERY_INTERFACE
		power_supply_changed(&sprdbat_data->ac);
		#endif
	}
	else
	{
		#ifdef CONFIG_SPRD_BATTERY_INTERFACE
		power_supply_changed(&sprdbat_data->usb);
		#endif
	}
	sprdbat_data->ac_online=0;
	sprdbat_data->usb_online=0;
	#if defined(CONFIG_SEC_CHARGING_FEATURE)
                spa_event_handler(SPA_EVT_CHARGER, (void*)POWER_SUPPLY_TYPE_BATTERY);
				charger_enable(0);
	#endif

//removing as the fgu is not in use
//	sprdbat_adp_plug_nodify(0);
	return 0;
}

static struct usb_hotplug_callback power_cb = {
	.plugin = plugin_callback,
	.plugout = plugout_callback,
	.data = NULL,
};


static int detect_device(int is_init)
{
	struct sprdbat_drivier_data *d = sprdbat_data;
	u8 adc, jig_on = 0;

	if (!d) {
		pr_warning("batttery_data is NULL!!\n");
		return 1;
	}

	jig_on = gpio_get_value(sprdbat_data->gpio_charger_detect);
	printk("detect_device jig_on %d\n", jig_on);
	//schedule_work(&(gpio_detect_work));
	if (jig_on)
	{
		mdelay(75);
		if(!is_init && sprdbat_data->detecting)
		{
			printk("%s duplicated detection\n", __func__);
			return 0;
		}

		adc = sci_adc_get_value(SPRDBAT_ADC_CHANNEL_VCHG, false);
		printk("%s detect_device SPRDBAT_ADC_CHANNEL_VCHG %d\n", __func__, adc);
		if ((adc > DETECT_CHG_ADC) && (adc < 100))
		{
			if(is_init || lp_boot_mode) // till lpm power off charging is implemented
			{
				if (sprdchg_charger_is_adapter()== ADP_TYPE_DCP)
				{
					sprdbat_data->ac_online = 1;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
					charger_enable(1);
					spa_event_handler(SPA_EVT_CHARGER, (void*)POWER_SUPPLY_TYPE_USB_DCP);
#endif
				}
				else
				{
					sprdbat_data->usb_online = 1;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
					charger_enable(1);
					spa_event_handler(SPA_EVT_CHARGER, (void*)POWER_SUPPLY_TYPE_USB);
#endif
				}
			}
		} else {

			printk("detect_device JIG attached\n");
			//d->charging = 0;
			sprdbat_data->jig_online = 1;
#if defined(CONFIG_SEC_CHARGING_FEATURE)
#ifndef CONFIG_SEC_MAKE_LCD_TEST
			spa_event_handler(SPA_EVT_ACC_INFO, (void*)SPA_ACC_JIG_UART);
#endif
			musb_info_handler(NULL, 0, (void*)1);
#endif
		}
	}
	else
	{
#if defined(CONFIG_SEC_CHARGING_FEATURE)
		if(sprdbat_data->jig_online)
		{
#ifndef CONFIG_SEC_MAKE_LCD_TEST
			spa_event_handler(SPA_EVT_ACC_INFO, (void*)SPA_ACC_NONE);
#endif
			musb_info_handler(NULL, 0, 0);
			sprdbat_data->jig_online = 0;
			printk("%s : JIG detached",__func__);
		}
		else
		{
			if(is_init || lp_boot_mode)
			{
				spa_event_handler(SPA_EVT_CHARGER, (void*)POWER_SUPPLY_TYPE_BATTERY);
				//d->charging = 0;
				sprdbat_data->ac_online = 0;
				sprdbat_data->usb_online = 0;
				charger_enable(0);
			}
		}
#endif
	}

	return 0;
}

#ifdef CONFIG_SPRD_BATTERY_INTERFACE
static char *supply_list[] = {
	"battery",
};

static char *battery_supply_list[] = {
	"audio-ldo",
	"sprdfgu",
};
#endif

//#ifdef CONFIG_NOTIFY_BY_JIGON
static void sprd_muic_irq_work(struct work_struct *work)
{
	detect_device(0);
}

static void gpio_detect_value_work(struct work_struct *work)
{
        struct sprdbat_drivier_data *d = sprdbat_data;
        u8 adc, jig_on ;
	jig_on = gpio_get_value(sprdbat_data->gpio_charger_detect);
	printk("The gpio get value is %d",jig_on);
        msleep(1000);
	schedule_work(&(gpio_detect_work));

}
static irqreturn_t sprd_muic_interrupt(int irq, void *dev_id)
{
	//struct sprd_battery_data *data = dev_id;
	struct sprdbat_drivier_data *data = dev_id;
	pr_info("%s %d\n", __func__, irq);
	irq_set_irq_type(data->irq_charger_detect,     data->gpio_init_active_low ^ gpio_get_value(data->gpio_charger_detect)
                                                        ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW);
	wake_lock_timeout(&(data->charger_plug_out_lock),
			  CONFIG_PLUG_WAKELOCK_TIME_SEC * HZ);

	schedule_work(&(data->irq_charger_detect_work));

	return IRQ_HANDLED;
}
//#endif


static int sprdbat_set_full_charge (unsigned int eoc)
{
	int ret = 0;

	pr_info("%s eoc=%dmA\n", __func__, eoc);
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	sprdbat_data->batt_eoc = eoc;
	sprdbat_data->batt_eoc_count = 0;
#endif
	return ret;
}

#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
static void sprdbat_eoc_work_func(struct work_struct *work)
{
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	printk("%s FULL CHARGING, current=%dmA, eoc=%dmA\n", __func__, sprdbat_data->batt_current, sprdbat_data->batt_eoc);
	spa_event_handler(SPA_EVT_EOC, (void*)0);
#endif
}
#endif


int sprdchg_get_current(unsigned int opt)
{
        int batt_current = 0,cur;
	    struct sprdbat_drivier_data *data = sprdbat_data;
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
#ifdef CONFIG_STC3115_FUELGAUGE
                if(read_current(&batt_current) < 0)
                        return batt_current;
#else

		cur = sprdchg_read_chg_current();
		sprdchg_put_chgcur(cur);
		batt_current=sprdchg_get_chgcur_ave();
		if(batt_current <0 )
			return batt_current ;
#endif

        if(batt_current == 0)
                return batt_current;

                sprdbat_data->batt_current = batt_current;
                if(sprdbat_data->batt_eoc_count >= 0)
                {
                        if(batt_current <= sprdbat_data->batt_eoc)
                                sprdbat_data->batt_eoc_count++;
                        else
                                sprdbat_data->batt_eoc_count = 0;
                        if(sprdbat_data->batt_eoc_count > 1)
                        {
                                if(sprdchg_get_battery_capacity() > 98)
                                {
                                        schedule_work(&(sprdbat_data->eoc_work));
                                        sprdbat_data->batt_eoc_count = -1;
                                }
                        }
                }
#endif
        return batt_current;

}


static int sprdchg_get_batt_presence(unsigned int opt)
{
 int adc;

#ifdef CONFIG_STC3115_FUELGAUGE
        read_VF(&adc);
        printk("%s, %X\n", __func__, adc);
        if(!adc)
                return 0;
#endif
adc= gpio_get_value(BATT_DETECT);

if (adc==0)
        return 1;
else
        return 0;
}

static int sprdbat_timer_handler(void *data)
{
	int ocv;
	sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	#if 0
//#ifdef CONFIG_STC3117_FUELGAUGE
	sprdbat_data->bat_info.vbat_ocv = read_ocv(); //replace read_soc
//	#else
	sprdfgu_read_vbat_ocv_pure(&ocv); //replace read_soc
	sprdbat_data->bat_info.vbat_ocv =ocv;
	#endif

	sprdbat_data->bat_info.bat_current =sprdchg_read_chg_current();
	//sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
	SPRDBAT_DEBUG("sprdbat_timer_handler----------vbat_vol %d,ocv:%d\n",
		      sprdbat_data->bat_info.vbat_vol,
		      sprdbat_data->bat_info.vbat_ocv);
	SPRDBAT_DEBUG("sprdbat_timer_handler----------bat_current %d\n",
		      sprdbat_data->bat_info.bat_current);
	//queue_work(sprdbat_data->monitor_wqueue, &sprdbat_data->charge_work);
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
		#if (defined(CONFIG_SEC_CHARGING_FEATURE) && defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING))
		printk("%s FULL CHARGING, current=%dmA, eoc=%dmA\n", __func__, sprdbat_data->batt_current, sprdbat_data->batt_eoc);
		spa_event_handler(SPA_EVT_EOC, (void*)0);
		#endif
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
	if (value) {
		if (!
		    (SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->
		     bat_info.chg_stop_flags)) {
			SPRDBAT_DEBUG("SPRDBAT_OVI_STOP_E\n");
			sprdbat_change_module_state(SPRDBAT_OVI_STOP_E);
			#if defined(CONFIG_SEC_CHARGING_FEATURE)
				spa_event_handler(SPA_EVT_OVP, 0);
			#endif
		}
	} else {
		if ((SPRDBAT_CHG_END_OVP_BIT & sprdbat_data->
		     bat_info.chg_stop_flags)) {
			SPRDBAT_DEBUG("SPRDBAT_OVI_RESTART_E\n");
			sprdbat_change_module_state(SPRDBAT_OVI_RESTART_E);
			#if defined(CONFIG_SEC_CHARGING_FEATURE)
				spa_event_handler(SPA_EVT_OVP, 1);
			#endif
		}
	}
	mutex_unlock(&sprdbat_data->lock);
}

static void sprdbat_print_log(void)
{
	struct timespec cur_time;
	int value;

	SPRDBAT_DEBUG("sprdbat_print_log\n");

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
	SPRDBAT_DEBUG("sprdbat_print_log end\n");

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

	SPRDBAT_DEBUG("CHIP ID is STC3115\n");
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
	uint32_t fgu_capacity = sprdchg_get_battery_capacity();
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
#ifdef CONFIG_SPRD_BATTERY_INTERFACE
	if (fgu_capacity != sprdbat_data->bat_info.capacity) {
		sprdbat_data->bat_info.capacity = fgu_capacity;
		sprdbat_update_capacity_time = cur_time.tv_sec;
		power_supply_changed(&sprdbat_data->battery);
	}
#endif
}

static void sprdbat_battery_works(struct work_struct *work)
{
	SPRDBAT_DEBUG("sprdbat_battery_works\n");

	mutex_lock(&sprdbat_data->lock);
	sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	sprdbat_data->bat_info.cur_temp = sprdbat_read_temp();

	//sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
	sprdbat_data->bat_info.bat_current = sprdchg_read_chg_current();

#if 0
//#ifdef CONFIG_STC3117_FUELGUAGE
	sprdbat_data->bat_info.vbat_ocv = read_ocv();
//#else
	sprdfgu_read_vbat_ocv_pure(&(sprdbat_data->bat_info.vbat_ocv));
#endif
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
		local_irq_save(irq_flag);


		//if (sprdbat_cv_irq_dis && sprdfgu_is_new_chip()) {
		if (sprdbat_cv_irq_dis ) {
			sprdbat_cv_irq_dis = 0;
			enable_irq(sprdbat_data->irq_chg_cv_state);
		}
		local_irq_restore(irq_flag);
		sprdbat_start_chg = 0;
	}
	SPRDBAT_DEBUG("sprdbat_charge_works----------vbat_vol %d,ocv:%d\n",
		      sprdbat_data->bat_info.vbat_vol,
		      sprdbat_data->bat_info.vbat_ocv);
	SPRDBAT_DEBUG("sprdbat_charge_works----------bat_current %d\n",
		      sprdbat_data->bat_info.bat_current);
	//sprdbat_temp_monitor();

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

		if ((sprdbat_data->bat_info.vbat_vol >
		     sprdbat_data->bat_param.chg_end_vol_l
		     || sprdbat_trickle_chg)
		    && sprdbat_data->bat_info.bat_current <
		    sprdbat_data->bat_param.chg_end_cur) {
			sprdbat_trickle_chg = 0;
			SPRDBAT_DEBUG("SPRDBAT_CHG_FULL_E\n");
			sprdbat_change_module_state(SPRDBAT_CHG_FULL_E);
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
	sprdbat_print_log();
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

static int sprdbat_probe(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct sprdbat_drivier_data *data;
	struct resource *res = NULL;
	#if CONFIG_SS_SPRD_TEMP
    static atomic_t device_count;
	struct device* devthermalzone1;
	int index;
	struct class* thermal_class;
	#endif
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
#ifdef CONFIG_SPRD_BATTERY_INTERFACE
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
#endif
	res = platform_get_resource(pdev, IORESOURCE_IO, 1);
	if (unlikely(!res)) {
		dev_err(&pdev->dev, "not io resource\n");
		goto err_io_resource;
	}

#if 0
	if (sprdfgu_is_new_chip()) {
		SPRDBAT_DEBUG("new chip\n");
		data->gpio_chg_cv_state = res->start;
	} else {
		SPRDBAT_DEBUG("old chip\n");
		data->gpio_chg_cv_state = A_GPIO_START + 3;
	}
#endif
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
	//INIT_DELAYED_WORK(&data->battery_work, sprdbat_battery_works);

	//INIT_DELAYED_WORK(&data->battery_sleep_work,
	//		  sprdbat_battery_sleep_works);//doubtful to keep
	INIT_WORK(&data->ovi_irq_work, sprdbat_ovi_irq_works);
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	INIT_WORK(&(data->eoc_work), sprdbat_eoc_work_func);
	data->batt_eoc_count = 0;
#endif
	INIT_WORK(&data->charge_work, sprdbat_charge_works);
	data->monitor_wqueue = create_singlethread_workqueue("sprdbat_monitor");
	sprdchg_timer_init(sprdbat_timer_handler, data);

	sprdchg_set_chg_ovp(data->bat_param.ovp_stop);

	#if CONFIG_SS_SPRD_TEMP
    thermal_class = class_create(THIS_MODULE, "thermal");
    if (IS_ERR(thermal_class))
            return PTR_ERR(thermal_class);
    atomic_set(&device_count, 0);
	index=atomic_inc_return(&device_count);
    devthermalzone1 = device_create(thermal_class, NULL,MKDEV(0, index), NULL, "thermalzone1");
    if (IS_ERR(devthermalzone1))
                return PTR_ERR(devthermalzone1);
    {
		int i=0;
		int ret=0;
		for(i=0; i < ARRAY_SIZE(ss_sprd_temp_attrs) ; i++)
		{
			ret = device_create_file(devthermalzone1, &ss_sprd_temp_attrs[i]);
			if(ret < 0)
				printk("device_create_file failed : %s \n", ss_sprd_temp_attrs[i].attr.name);
		}
	}

	#endif


//#ifdef CONFIG_NOTIFY_BY_JIGON
	res = platform_get_resource(pdev, IORESOURCE_IO, 3);
        if (unlikely(!res)) {
                dev_err(&pdev->dev, "not io resource\n");
                goto err_io_resource;
        }

	data->gpio_charger_detect =  res->start;
	data->gpio_init_active_low = 1;
	ret = gpio_request(data->gpio_charger_detect, "charger_detect");
	if (ret) {
		dev_err(&pdev->dev, "failed to request gpio: %d\n", ret);
		goto err_io_request;
	}

	gpio_direction_input(data->gpio_charger_detect);

	ret = gpio_to_irq(data->gpio_charger_detect);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get irq form gpio: %d\n", ret);
		goto err_io_to_irq;
	}

	gpio_export(data->gpio_charger_detect,1);
	data->irq_charger_detect = ret;
	//batt_presence
//#ifndef CONFIG_STC3117_FUELGAUGE
        ret = gpio_request(BATT_DETECT, "battery_Detect");
        if (ret) {
                dev_err(&pdev->dev, "failed to request gpio: %d\n", ret);
        }
        gpio_direction_input(BATT_DETECT);
        gpio_export(BATT_DETECT,1);

//#endif

	INIT_WORK(&(data->irq_charger_detect_work), sprd_muic_irq_work);

	INIT_WORK(&(gpio_detect_work), gpio_detect_value_work);
//#endif
	sprdchg_init();
	sprdfgu_init(pdev);
	sprdbat_info_init(data);
#if defined(CONFIG_SEC_CHARGING_FEATURE)
        spa_agent_register(SPA_AGENT_SET_CHARGE, (void*)sprdchg_set_charge, "sprd-charger");
        spa_agent_register(SPA_AGENT_SET_CHARGE_CURRENT, (void*)sprdchg_set_chg_cur, "sprd-charger");
        spa_agent_register(SPA_AGENT_SET_FULL_CHARGE,sprdbat_set_full_charge , "sprd-charger");
        spa_agent_register(SPA_AGENT_GET_CAPACITY, (void*)sprdchg_get_battery_capacity, "sprd-charger");
        spa_agent_register(SPA_AGENT_GET_TEMP, (void*)sprdchg_read_temp, "sprd-charger");
        spa_agent_register(SPA_AGENT_GET_VOLTAGE, (void*)sprdchg_read_vbat_vol, "sprd-charger");
        spa_agent_register(SPA_AGENT_GET_CURRENT, (void*)sprdchg_get_current, "sprd-charger");
        spa_agent_register(SPA_AGENT_GET_BATT_PRESENCE, (void*)sprdchg_get_batt_presence, "sprd-charger");
        spa_agent_register(SPA_AGENT_GET_CHARGER_TYPE, (void*)sprdchg_get_charger_type, "sprd-charger");
        spa_agent_register(SPA_AGENT_CTRL_FG, (void*)sprdbat_ctrl_fg, "sprd-charger");
#endif

	sprdbat_notifier.notifier_call = sprdbat_fgu_event;
//	sprdfgu_register_notifier(&sprdbat_notifier);
//#if defined(CONFIG_NOTIFY_BY_JIGON) || defined(CONFIG_NOTIFY_BY_USB)
	if(lp_boot_mode)
		detect_device(1);
	else
		detect_device(0);

//#endif

//#ifdef CONFIG_NOTIFY_BY_JIGON
#if 0
	ret = request_threaded_irq(data->irq_charger_detect, sprd_muic_interrupt, NULL,
			IRQF_TRIGGER_LOW| IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND,
				pdev->name, data);
#endif

      ret = request_threaded_irq(data->irq_charger_detect, sprd_muic_interrupt, NULL,
                        ((data->gpio_init_active_low ^ gpio_get_value(data->gpio_charger_detect) ? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW) | IRQF_NO_SUSPEND),
                                pdev->name, data);
        printk("ret for rquest threaded irq is %d \n",ret);

	if (ret)
		goto err_request_irq_failed;
//#endif
	usb_register_hotplug_callback(&power_cb);

	//schedule_delayed_work(&data->battery_work,
	//		      SPRDBAT_CAPACITY_MONITOR_NORMAL);
	SPRDBAT_DEBUG("sprdbat_probe----------end\n");
	return 0;

err_request_irq_ovi_failed:
	gpio_free(data->gpio_vchg_ovi);
err_io_ovi_request:
	free_irq(data->irq_chg_cv_state, data);
err_request_irq_cv_failed:
	gpio_free(data->gpio_chg_cv_state);
err_io_cv_request:
#ifdef CONFIG_SPRD_BATTERY_INTERFACE

err_io_resource:
	power_supply_unregister(&data->battery);
err_battery_failed:
	power_supply_unregister(&data->ac);
err_ac_failed:
	power_supply_unregister(&data->usb);
#endif

err_request_irq_failed:
        printk("ret for rquest threaded irq is \n",ret);

err_io_to_irq:
err_io_request:
err_io_resource:
err_usb_failed:
	kfree(data);
err_data_alloc_failed:
	sprdbat_data = NULL;
	return ret;

}

static int sprdbat_remove(struct platform_device *pdev)
{
	struct sprdbat_drivier_data *data = platform_get_drvdata(pdev);
#ifdef CONFIG_SPRD_BATTERY_INTERFACE
	sprdbat_remove_caliberate_attr(data->battery.dev);
	power_supply_unregister(&data->battery);
	power_supply_unregister(&data->ac);
	power_supply_unregister(&data->usb);
#endif

//	#ifdef CONFIG_NOTIFY_BY_JIGON
//	cancel_work_sync(&(data->irq_work));
//	free_irq(data->irq, data);
//	#endif
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	cancel_work_sync(&(data->eoc_work));
#endif
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
	//schedule_delayed_work(&sprdbat_data->battery_sleep_work, 0);
	//sprdfgu_pm_op(0);
	return 0;
}

static int sprdbat_suspend(struct platform_device *pdev, pm_message_t state)
{
	//sprdfgu_pm_op(1);
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
#if 1 //temporary
        __raw_writel((BITS_PIN_DS(1)|BITS_PIN_AF(3)|BIT_PIN_NUL|BIT_PIN_SLP_NUL|BIT_PIN_SLP_IE),  SCI_ADDR(SPRD_PIN_BASE, 0x00D0));
#endif

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
