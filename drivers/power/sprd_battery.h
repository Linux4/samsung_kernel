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

#ifndef _SPRD_BATTERY_H_
#define _SPRD_BATTERY_H_

#include <linux/types.h>
#include <linux/hrtimer.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>
#include <linux/sprd_battery_common.h>

#if defined (CONFIG_SPRD_EXT_IC_POWER)
#include "sprd_charge_helper.h"
#include "sprd_2713_fgu.h"
#endif

#if defined(CONFIG_SPRD_2713_POWER)
#include "sprd_2713_charge.h"
#include "sprd_2713_fgu.h"
#endif

#define SPRDBAT_AUXADC_CAL_NO         0
#define SPRDBAT_AUXADC_CAL_NV         1
#define SPRDBAT_AUXADC_CAL_CHIP      2

#define SPRDBAT_CHG_END_NONE_BIT			0
#define SPRDBAT_CHG_END_FULL_BIT		(1 << 0)
#define SPRDBAT_CHG_END_OTP_OVERHEAT_BIT     	(1 << 1)
#define SPRDBAT_CHG_END_OTP_COLD_BIT    (1 << 2)
#define SPRDBAT_CHG_END_TIMEOUT_BIT		(1 << 3)
#define SPRDBAT_CHG_END_OVP_BIT		(1 << 4)
#define SPRDBAT_CHG_END_BAT_OVP_BIT	(1 << 5)
#define SPRDBAT_CHG_END_UNSPEC		(1 << 8)


#define SPRDBAT_AVERAGE_COUNT   8
#define SPRDBAT_PLUG_WAKELOCK_TIME_SEC 3

#define SPRDBAT_AUXADC_CAL_TYPE_NO         0
#define SPRDBAT_AUXADC_CAL_TYPE_NV         1
#define SPRDBAT_AUXADC_CAL_TYPE_EFUSE      2

//#define SPRDFGU_TEMP_COMP_SOC

enum sprd_adapter_type {
	ADP_TYPE_UNKNOW = 0,	//unknow adapter type
	ADP_TYPE_CDP = 1,	//Charging Downstream Port,USB&standard charger
	ADP_TYPE_DCP = 2,	//Dedicated Charging Port, standard charger
	ADP_TYPE_SDP = 4,	//Standard Downstream Port,USB and nonstandard charge
};

enum chg_full_condition{
	VOL_AND_CUR = 0,
	VOL_AND_STATUS,
	FROM_EXT_IC,
};
struct sprd_ext_ic_operations {
	void (*ic_init) (struct sprd_battery_platform_data *);
	void (*charge_start_ext) (int);
	void (*charge_stop_ext) (void);
	int (*get_charging_status) (void);
	int(*get_charging_fault) (void);
	void (*timer_callback_ext) (void);
	int (*get_charge_cur_ext)(void);
	void(*set_termina_cur_ext)(int);
	void(*set_termina_vol_ext)(int);
	void (*otg_charge_ext) (int);
	void (*ext_register_notifier)(struct notifier_block *);
	void(*ext_unregster_notifier)(struct notifier_block *);
};
struct sprdbat_info {
	uint32_t module_state;
	uint32_t bat_health;
	uint32_t chging_current;
	int bat_current;
	uint32_t chg_current_type;
	uint32_t adp_type;
	uint32_t usb_online;
	uint32_t ac_online;
	uint32_t vchg_vol;
	uint32_t cccv_point;
	unsigned long chg_start_time;
	uint32_t chg_stop_flags;
	uint32_t vbat_vol;
	uint32_t vbat_ocv;
	int cur_temp;
	uint32_t capacity;
	uint32_t soc;
	uint32_t chg_this_timeout;
	uint32_t avg_chg_vol;
};

struct sprdbat_drivier_data {
	struct sprd_battery_platform_data *pdata;
	struct sprdbat_info bat_info;
	struct mutex lock;
	struct device *dev;
	struct power_supply battery;
	struct power_supply ac;
	struct power_supply usb;
	uint32_t gpio_charger_detect;
	uint32_t gpio_chg_cv_state;
	uint32_t gpio_vchg_ovi;
	uint32_t gpio_vbat_detect;
	uint32_t irq_charger_detect;
	uint32_t irq_chg_cv_state;
	uint32_t irq_vchg_ovi;
	uint32_t irq_vbat_detect;
	struct wake_lock charger_wake_lock;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work cv_irq_work;
	struct delayed_work battery_work;
	struct delayed_work battery_sleep_work;
	struct work_struct ovi_irq_work;
	struct work_struct vbat_detect_irq_work;
	struct delayed_work *charge_work;
	int (*start_charge) (void);
	int (*stop_charge) (void);
#ifdef CONFIG_LEDS_TRIGGERS
	struct led_classdev charging_led;
#endif
	//add
	uint32_t chg_cur_adjust_min;
	uint32_t chg_cur_adjust_max;
	uint32_t last_temp_status;
	uint32_t cur_temp_status;
	uint32_t temp_up_trigger_cnt;
	uint32_t temp_down_trigger_cnt;
	uint32_t chg_full_trigger_cnt;
	uint32_t sprdbat_average_cnt;
	uint32_t sprdbat_trickle_chg;
	uint32_t poweron_capacity;
	unsigned long sprdbat_update_capacity_time;
	unsigned long sprdbat_last_query_time;
	struct delayed_work sprdbat_charge_work;
	struct notifier_block sprdbat_notifier;
};

struct sprdbat_auxadc_cal {
	uint16_t p0_vol;	//4.2V
	uint16_t p0_adc;
	uint16_t p1_vol;	//3.6V
	uint16_t p1_adc;
	uint16_t cal_type;
};


#define sprdbat_read_vbat_vol sprdfgu_read_vbat_vol
#define sprdbat_read_temp sprdchg_read_temp
#define sprdbat_adp_plug_nodify sprdfgu_adp_status_set
#define sprdbat_read_temp_adc sprdchg_read_temp_adc

int sprdbat_interpolate(int x, int n, struct sprdbat_table_data *tab);

#ifdef SPRDBAT_TWO_CHARGE_CHANNEL
#define SPRDBAT_CHG_EVENT_EXT_OVI   1
#define SPRDBAT_CHG_EVENT_EXT_OVI_RESTART   2
void sprdchg_start_charge_ext(void);
void sprdchg_stop_charge_ext(void);
int sprdchg_is_chg_done_ext(void);
int sprdchg_charge_init_ext(struct platform_device *pdev);
void sprdchg_chg_monitor_cb_ext(void *data);
void sprdchg_open_ovi_fun_ext(void);
void sprdchg_close_ovi_fun_ext(void);
void sprdbat_charge_event_ext(uint32_t event);
#endif
void sprdbat_register_ext_ops(const struct sprd_ext_ic_operations * ops);
void sprdbat_unregister_ext_ops(void);
extern const struct sprd_ext_ic_operations *sprd_get_ext_ic_ops(void);
#endif /* _CHG_DRVAPI_H_ */
