/*
 driver/usb/typec/slsi/s2mf301/s2mf301-water.c - S2MF301 water device driver
 *
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/version.h>

#include <linux/usb/typec/slsi/s2mf301/s2mf301-water.h>
#include <linux/usb/typec/slsi/s2mf301/usbpd-s2mf301.h>
#include "../../../../battery/charger/s2mf301_charger/s2mf301_pmeter.h"
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>

static void s2mf301_water_state_dry(struct s2mf301_water_data *water);
static void s2mf301_water_state_attached(struct s2mf301_water_data *water);
static void s2mf301_water_state_water(struct s2mf301_water_data *water);
static void s2mf301_water_state_wait_check(struct s2mf301_water_data *water);
static void s2mf301_water_state_dry_status(struct s2mf301_water_data *water);
static void s2mf301_water_state_1st_check(struct s2mf301_water_data *water);
static void s2mf301_water_state_2nd_check(struct s2mf301_water_data *water);
static void s2mf301_water_state_wait_recheck(struct s2mf301_water_data *water);

static int s2mf301_water_get_prop(struct s2mf301_water_data *water, int prop);

static char *WATER_STATE_TO_STR[] = {
	"Invalid"
	"Init",
	"Dry",
	"Attached",
	"Water",
	"WaitCheck",
	"CheckDryStatus",
	"1stCheck",
	"2ndCheck",
	"WaitRecheck",
	"MAX",
};

static char *WATER_EVENT_TO_STR[] = {
	"Initialize",
	"SBUChangeInt",
	"WETInt",
	"AttachAsSrc",
	"AttachAsSnk",
	"Detach",
	"WaterDetected",
	"DryDetected",
	"TimerExpired",
	"Recheck",
};

static inline void water_sleep(int ms) {
	if (ms >= 20)
		msleep(ms);
	else
		usleep_range(ms*1000, ms*1000 + 100);
}

static void water_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

static void water_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

static int water_set_wakelock(struct s2mf301_water_data *water)
{
	struct wakeup_source *ws = NULL;
	struct wakeup_source *ws2 = NULL;

	ws = wakeup_source_register(NULL, "water_wake");

	if (ws == NULL) {
		pr_info("%s, Fail to Rigster ws\n", __func__);
		goto err;
	} else
		water->water_wake = ws;

	ws2 = wakeup_source_register(NULL, "recheck_wake");

	if (ws2 == NULL) {
		pr_info("%s, Fail to Rigster ws\n", __func__);
		goto err;
	} else
		water->recheck_wake = ws2;

	return 0;
err:
	return -1;
}

static enum alarmtimer_restart s2mf301_water_alarm1_callback(struct alarm *alarm, ktime_t now)
{
	struct s2mf301_water_data *water = container_of(alarm, struct s2mf301_water_data, water_alarm1);

	pr_info("%s, \n", __func__);
	water->alarm1_expired = 1;

	return ALARMTIMER_NORESTART;
}

static void s2mf301_water_alarm1_start(struct s2mf301_water_data *water, int ms)
{
	ktime_t sec, msec;

	water->alarm1_expired = 0;
	alarm_cancel(&water->water_alarm1);
	sec = ktime_get_boottime();
	msec = ktime_set(0, ms * NSEC_PER_MSEC);
	alarm_start(&water->water_alarm1, ktime_add(sec, msec));
}

#if 0
static enum alarmtimer_restart s2mf301_water_alarm2_callback(struct alarm *alarm, ktime_t now)
{
	struct s2mf301_water_data *water = container_of(alarm, struct s2mf301_water_data, water_alarm2);

	pr_info("%s, \n", __func__);
	water->alarm2_expired = 1;

	return ALARMTIMER_NORESTART;
}

static void s2mf301_water_alarm2_start(struct s2mf301_water_data *water, int ms)
{
	ktime_t sec, msec;

	water->alarm2_expired = 0;
	alarm_cancel(&water->water_alarm2);
	sec = ktime_get_boottime();
	msec = ktime_set(0, ms * NSEC_PER_MSEC);
	alarm_start(&water->water_alarm2, ktime_add(sec, msec));
}
#endif

static void s2mf301_water_set_status(struct s2mf301_water_data *water, int status)
{
	struct s2mf301_usbpd_data *pdic_data = container_of(water, struct s2mf301_usbpd_data, water);

	if (status == water->status) {
		pr_info("%s, skip set status\n", __func__);
		return;
	} else {
		water->status = status;
		if (status == S2M_WATER_STATUS_DRY) {
			pr_info("[WATER] %s, DRY Detected\n", __func__);
			water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);
			water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_WATER);

			s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_DRP);
			water->water_det_en(water->pmeter, true);
			water->water_10s_en(water->pmeter, false);

			water->water_set_status(water->pmeter, S2M_WATER_STATUS_DRY);
			s2mf301_usbpd_water_set_status(pdic_data, S2M_WATER_STATUS_DRY);

			water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
			water->pm_enable(water->pmeter, CONTINUOUS_MODE, true, S2MF301_PM_TYPE_GPADC12);

			water_sleep(100);
			water->event = S2M_WATER_EVENT_INVALID;
			water->water_irq_masking(water->pmeter, false, S2MF301_IRQ_TYPE_CHANGE);
		} else if (status == S2M_WATER_STATUS_WATER) {
			pr_info("[WATER] %s, Water Detected\n", __func__);

			water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);
			water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);

			s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_RD);
			water->water_det_en(water->pmeter, true);
			water->water_10s_en(water->pmeter, true);

			water->water_set_status(water->pmeter, S2M_WATER_STATUS_WATER);
			s2mf301_usbpd_water_set_status(pdic_data, S2M_WATER_STATUS_WATER);

			water_sleep(50);
			water->water_irq_masking(water->pmeter, false, S2MF301_IRQ_TYPE_WATER);
		} else {
			pr_info("[WATER] %s, invalid status(%d)\n", __func__, status);
		}
	}
}

void s2mf301_water_1time_check(struct s2mf301_water_data *water, int *volt)
{
	water->gpadc_rr_done = 0;
	water->vgpadc1 = water->vgpadc2 = 0xffff;

	/* turn on CurrentSource */
	water->water_det_en(water->pmeter, true);

	/* wait charge */
	water_sleep(water->delay_2nd_chg);

	/* adc enable */
	water->pm_enable(water->pmeter, REQUEST_RESPONSE_MODE, true, S2MF301_PM_TYPE_GPADC12);

	while (!water->gpadc_rr_done)
		water_sleep(1);

	volt[0] = water->vgpadc1;
	volt[1] = water->vgpadc2;

	pr_info("[WATER] %s, volt1 = %dmV, volt2 = %dmV\n", __func__, volt[0], volt[1]);

	/* turn off CurrentSource */
	water->water_det_en(water->pmeter, false);
	/* clear adc */
	water->pm_enable(water->pmeter, REQUEST_RESPONSE_MODE, false, S2MF301_PM_TYPE_GPADC12);
}

int s2mf301_water_check_facwater(struct s2mf301_water_data *water)
{
	int volt[2] = {0, };
	
	water->water_det_en(water->pmeter, true);
	water_sleep(50);

	volt[0] = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_GPADC1);
	volt[1] = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_GPADC2);

	if (volt[0] <= 300 && volt[1] <= 300)
		return true;
	else
		return false;
}

static void s2mf301_water_state_dry(struct s2mf301_water_data *water)
{
	pr_info("[WATER] %s\n", __func__);
	s2mf301_water_set_status(water, S2M_WATER_STATUS_DRY);
}

static void s2mf301_water_state_attached(struct s2mf301_water_data *water)
{
	pr_info("[WATER] %s\n", __func__);
	s2mf301_water_set_status(water, S2M_WATER_STATUS_DRY);

	complete(&water->water_check_done);
}

static void s2mf301_water_state_water(struct s2mf301_water_data *water)
{
	pr_info("[WATER] %s\n", __func__);
	s2mf301_water_set_status(water, S2M_WATER_STATUS_WATER);
}

static void s2mf301_water_state_wait_check(struct s2mf301_water_data *water)
{
	int vbus = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_VCHGIN);

	if (vbus >= 4000) {
		pr_info("[WATER] %s, VBUS(%dmV), skip water check\n", __func__, vbus);
		water->event = S2M_WATER_EVENT_VBUS_DETECTED;
		goto exit;
	}

	s2mf301_water_alarm1_start(water, 200);

	while(1) {
		if (water->event) {
			switch (water->event) {
			case S2M_WATER_EVENT_ATTACH_AS_SRC:
				pr_info("[WATER] %s, attach as SRC, goto check water\n", __func__);
				goto exit;
			case S2M_WATER_EVENT_ATTACH_AS_SNK:
				if (vbus >= 4000) {
					water->event = S2M_WATER_EVENT_VBUS_DETECTED;
					pr_info("[WATER] %s, Vbus (%dmV), skip check\n", __func__, vbus);
					goto exit;
				}
				pr_info("[WATER] %s, attach as SNK, goto check water\n", __func__);
				goto exit;
			default:
				break;
			}
		}

		if (water->alarm1_expired) {
			pr_info("[WATER] %s, wait timeout, goto check water\n", __func__);
			water->event = S2M_WATER_EVENT_TIMER_EXPIRED;
			goto exit;
		}
	}

exit:
	schedule_delayed_work(&water->state_work, 0);
}

static void s2mf301_water_state_dry_status(struct s2mf301_water_data *water)
{
	int status = water->water_get_status(water->pmeter);
	int volt[2] = {0, };
	
	pr_info("[WATER] %s, status[0x%x / %s]\n", __func__, status, ((status) ? ("WATER") : ("DRY")));

	switch (status) {
	case S2MF301_WATER_GPADC_DRY:
		water->event = S2M_WATER_EVENT_DRY_DETECTED;
		break;
	case S2MF301_WATER_GPADC1:
	case S2MF301_WATER_GPADC2:
	case S2MF301_WATER_GPADC12:
		water->event = S2M_WATER_EVENT_WATER_DETECTED;
		break;
	default:
		break;
	}

	volt[0] = s2mf301_water_get_prop(water, POWER_SUPPLY_LSI_PROP_VGPADC1);
	volt[1] = s2mf301_water_get_prop(water, POWER_SUPPLY_LSI_PROP_VGPADC2);
	pr_info("[WATER] %s, vgpadc[%dmV / %dmV]\n", __func__, volt[0], volt[1]);

	schedule_delayed_work(&water->state_work, 0);
}

static void s2mf301_water_state_1st_check(struct s2mf301_water_data *water)
{
	int volt[2] = {0, };
	int vbus = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_VCHGIN);

	mutex_lock(&water->mutex);

	if (vbus >= 4000) {
		pr_info("[WATER] %s, VBUS(%dmV), skip water check\n", __func__, vbus);
		water->event = S2M_WATER_EVENT_VBUS_DETECTED;
		goto exit;
	}

	/* Check Capacitance */
	water->water_set_gpadc_mode(water->pmeter, S2MF301_GPADC_VMODE);
	water->pm_enable(water->pmeter, CONTINUOUS_MODE, false, S2MF301_PM_TYPE_GPADC12);

	water->water_det_en(water->pmeter, false);
	water_sleep(water->delay_1st_vmode_dischg);

	water->gpadc_rr_done = 0;
	water->water_irq_masking(water->pmeter, false, S2MF301_IRQ_TYPE_RR);
	water->pm_enable(water->pmeter, REQUEST_RESPONSE_MODE, true, S2MF301_PM_TYPE_GPADC12);
	while (!water->gpadc_rr_done)
		water_sleep(1);

	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);
	water->water_set_gpadc_mode(water->pmeter, S2MF301_GPADC_RMODE);

	volt[0] = water->vgpadc1;
	volt[1] = water->vgpadc2;
	pr_info("[WATER] %s, 1st volt[%dmV, %dmV]\n", __func__, volt[0], volt[1]);

	if (IS_NOT_CAP(volt[0]) && IS_NOT_CAP(volt[1])) {
		water->event = S2M_WATER_EVENT_DRY_DETECTED;
		goto exit;
	}

	/* discharge */
	water_sleep(water->delay_1st_rmode_dischg);

	/* Check Resistance */
	water->water_det_en(water->pmeter, true);
	water_sleep(water->delay_1st_rmode_chg);


	water->gpadc_rr_done = 0;
	water->water_irq_masking(water->pmeter, false, S2MF301_IRQ_TYPE_RR);
	water->pm_enable(water->pmeter, REQUEST_RESPONSE_MODE, true, S2MF301_PM_TYPE_GPADC12);
	while (!water->gpadc_rr_done)
		water_sleep(1);

	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);

	volt[0] = water->vgpadc1;
	volt[1] = water->vgpadc2;
	pr_info("[WATER] %s, 2nd volt1[%d], volt2[%d]\n", __func__, volt[0], volt[1]);

	if (IS_DRY(volt[0]) && IS_DRY(volt[1])) {
		water->event = S2M_WATER_EVENT_DRY_DETECTED;
	} else {
		water->event = S2M_WATER_EVENT_WATER_DETECTED;
	}

exit:
	mutex_unlock(&water->mutex);
	schedule_delayed_work(&water->state_work, 0);
}

static void s2mf301_water_state_2nd_check(struct s2mf301_water_data *water)
{
	int vbus = water->pm_get_value(water->pmeter, S2MF301_PM_TYPE_VCHGIN);
	int i;
	int check_cnt = water->check_cnt;
	int avg[2] = {0, 0};
	int volt[2] = {0, 0};

	if (vbus >= 4000) {
		pr_info("[WATER] %s, VBUS(%dmV), skip water check\n", __func__, vbus);
		water->event = S2M_WATER_EVENT_VBUS_DETECTED;
		goto exit;
	}

	// 2nd pre setting
	water->water_det_en(water->pmeter, false);
	water->water_irq_masking(water->pmeter, false, S2MF301_IRQ_TYPE_RR);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_WATER);

	water->water_set_status(water->pmeter, S2M_WATER_STATUS_DRY); //need ?

	pr_info("%s, start check cnt[%d], dischg[%d], recheck[%d],  interval[%d]\n", __func__,
			water->check_cnt, water->delay_2nd_dischg, water->recheck_delay, water->delay_2nd_interval);
	water_sleep(water->delay_2nd_dischg);
	for(i = 0; i < check_cnt; i++) {
		s2mf301_water_1time_check(water, volt);
		avg[0]+=volt[0];
		avg[1]+=volt[1];
		water_sleep(water->delay_2nd_interval);
	}

	avg[0] = avg[0] / check_cnt;
	avg[1] = avg[1] / check_cnt;
	pr_info("[WATER] %s, avg[%dmV / %dmV]\n", __func__, avg[0], avg[1]);

	if (IS_WATER(avg[0]) || IS_WATER(avg[1])) {
		water->event = S2M_WATER_EVENT_WATER_DETECTED;
	} else if (IS_DRY(avg[0]) && IS_DRY(avg[1])) {
		water->event = S2M_WATER_EVENT_DRY_DETECTED;
	} else {
		water->event = S2M_WATER_EVENT_RECHECK;
	}

exit:
	schedule_delayed_work(&water->state_work, 0);
}

static void s2mf301_water_state_wait_recheck(struct s2mf301_water_data *water)
{
	struct s2mf301_usbpd_data *pdic_data = container_of(water, struct s2mf301_usbpd_data, water);

	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_RR);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_CHANGE);
	water->water_irq_masking(water->pmeter, true, S2MF301_IRQ_TYPE_WATER);

	s2mf301_usbpd_set_cc_state(pdic_data, CC_STATE_DRP);

	water->water_set_status(water->pmeter, S2M_WATER_STATUS_DRY);

	water_wake_lock(water->recheck_wake);
	schedule_delayed_work(&water->state_work, 0);
}

static void s2mf301_water_state_transition(struct s2mf301_water_data *water, enum s2m_water_state_t next)
{
	water->cur_state = next;
	water->event = S2M_WATER_EVENT_INVALID;
	pr_info("[WATER] %s, new state[%s]\n", __func__, WATER_STATE_TO_STR[water->cur_state]);

	switch (next) {
	case S2M_WATER_STATE_DRY:
		s2mf301_water_state_dry(water);
		break;
	case S2M_WATER_STATE_ATTACHED:
		s2mf301_water_state_attached(water);
		break;
	case S2M_WATER_STATE_WATER:
		s2mf301_water_state_water(water);
		break;
	case S2M_WATER_STATE_WAIT_CHECK:
		s2mf301_water_state_wait_check(water);
		break;
	case S2M_WATER_STATE_CHECK_DRY_STATUS:
		s2mf301_water_state_dry_status(water);
		break;
	case S2M_WATER_STATE_1st_CHECK:
		s2mf301_water_state_1st_check(water);
		break;
	case S2M_WATER_STATE_2nd_CHECK:
		s2mf301_water_state_2nd_check(water);
		break;
	case S2M_WATER_STATE_WAIT_RECHECK:
		s2mf301_water_state_wait_recheck(water);
		break;
	default:
		break;
	}
}

static void s2mf301_water_state_work(struct work_struct *work)
{
	struct s2mf301_water_data *water = container_of(work, struct s2mf301_water_data, state_work.work);

	enum s2m_water_state_t cur_state = water->cur_state;
	enum s2m_water_state_t next_state = S2M_WATER_STATE_INVALID;
	enum s2m_water_event_t event = water->event;
	bool skip = false;

	water_wake_lock(water->water_wake);
	switch (cur_state) {
	case S2M_WATER_STATE_INIT:
		next_state = S2M_WATER_STATE_DRY;
	case S2M_WATER_STATE_DRY:
		switch (event) {
		case S2M_WATER_EVENT_SBU_CHANGE_INT:
			next_state = S2M_WATER_STATE_WAIT_CHECK;
			break;
		case S2M_WATER_EVENT_ATTACH_AS_SRC:
		case S2M_WATER_EVENT_ATTACH_AS_SNK:
			next_state = S2M_WATER_STATE_ATTACHED;
			break;
		default:
			skip = true;
			break;
		}
		break;
	case S2M_WATER_STATE_ATTACHED:
		switch (event) {
		case S2M_WATER_EVENT_DETACH:
			next_state = S2M_WATER_STATE_DRY;
			break;
		default:
			skip = true;
			break;
		}
		break;
	case S2M_WATER_STATE_WAIT_CHECK:
		switch (event) {
		case S2M_WATER_EVENT_ATTACH_AS_SRC:
		case S2M_WATER_EVENT_ATTACH_AS_SNK:
		case S2M_WATER_EVENT_TIMER_EXPIRED:
			next_state = S2M_WATER_STATE_1st_CHECK;
			break;
		case S2M_WATER_EVENT_VBUS_DETECTED:
			next_state = S2M_WATER_STATE_ATTACHED;
			break;
		default:
			skip = true;
			break;
		}
		break;
	case S2M_WATER_STATE_1st_CHECK:
		switch (event) {
		case S2M_WATER_EVENT_DRY_DETECTED:
			next_state = S2M_WATER_STATE_DRY;
			break;
		case S2M_WATER_EVENT_WATER_DETECTED:
			next_state = S2M_WATER_STATE_2nd_CHECK;
			break;
		default:
			skip = true;
			break;
		}
		break;
	case S2M_WATER_STATE_2nd_CHECK:	
		switch (event) {
		case S2M_WATER_EVENT_RECHECK:
			next_state = S2M_WATER_STATE_WAIT_RECHECK;
			break;
		case S2M_WATER_EVENT_WATER_DETECTED:
			next_state = S2M_WATER_STATE_WATER;
			break;
		case S2M_WATER_EVENT_DRY_DETECTED:
			next_state = S2M_WATER_STATE_DRY;
			break;
		default:
			skip = true;
			break;
		}
		break;
	case S2M_WATER_STATE_WATER:
		switch (event) {
		case S2M_WATER_EVENT_WET_INT:
			next_state = S2M_WATER_STATE_CHECK_DRY_STATUS;
			break;
		default:
			skip = true;
			break;
		}
		break;
	case S2M_WATER_STATE_CHECK_DRY_STATUS:
		switch (event) {
		case S2M_WATER_EVENT_DRY_DETECTED:
			next_state = S2M_WATER_STATE_DRY;
			break;
		case S2M_WATER_EVENT_WATER_DETECTED:
			next_state = S2M_WATER_STATE_WATER;
			break;
		default:
			skip = true;
			break;
		}
		break;
	case S2M_WATER_STATE_WAIT_RECHECK:
		water_wake_unlock(water->recheck_wake);
		switch (event) {
		case S2M_WATER_EVENT_TIMER_EXPIRED:
			next_state = S2M_WATER_STATE_2nd_CHECK;
			break;
		default:
			skip = true;
			break;
		}
		break;
	default:
		skip = true;
		break;
	}

	if (skip) {
		pr_info("[WATER] %s, skip state[%s], event[%s]\n", __func__, WATER_STATE_TO_STR[cur_state],
				WATER_EVENT_TO_STR[event]);
	} else {
		pr_info("[WATER] %s, event[%s], state[%s -> %s]\n", __func__, WATER_EVENT_TO_STR[event],
				WATER_STATE_TO_STR[cur_state], WATER_STATE_TO_STR[next_state]);
		s2mf301_water_state_transition(water, next_state);
		pr_info("[WATER] %s, state finished\n", __func__);
	}

	water_wake_unlock(water->water_wake);
}

static void s2mf301_water_set_prop(struct s2mf301_water_data *water, int prop, union power_supply_propval val)
{
	if (!water->psy_pm) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		water->psy_pm = get_power_supply_by_name("s2mf301-pmeter");
#endif
		if (!water->psy_pm) {
			pr_info("%s, Fail to get psy_pm\n", __func__);
			return;
		}
	}

	water->psy_pm->desc->set_property(water->psy_pm, prop, &val);
}

static int s2mf301_water_get_prop(struct s2mf301_water_data *water, int prop)
{
	 union power_supply_propval value;

	if (!water->psy_pm) {
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		water->psy_pm = get_power_supply_by_name("s2mf301-pmeter");
#endif
		if (!water->psy_pm) {
			pr_info("%s, Fail to get psy_pm\n", __func__);
			return -1;
		}
	}

	water->psy_pm->desc->get_property(water->psy_pm, prop, &value);
	
	return value.intval;
}

void s2mf301_water_init(struct s2mf301_water_data *water)
{
	union power_supply_propval value;

	pr_info("[WATER] %s, s2mf301 water detection enabled\n", __func__);

	water->cur_state = S2M_WATER_STATE_INIT;
	water->event = S2M_WATER_EVENT_INITIALIZE;
	INIT_DELAYED_WORK(&water->state_work, s2mf301_water_state_work);
	alarm_init(&water->water_alarm1, ALARM_BOOTTIME, s2mf301_water_alarm1_callback);
#if 0
	alarm_init(&water->water_alarm2, ALARM_BOOTTIME, s2mf301_water_alarm2_callback);
#endif

	water->check_cnt = 5;					//count(times)
	water->recheck_delay = 1000;			//ms

	water->delay_1st_vmode_dischg = 10;		//ms
	water->delay_1st_rmode_dischg = 100;	//ms
	water->delay_1st_rmode_chg = 10;		//ms

	water->delay_2nd_dischg = 100;			//ms
	water->delay_2nd_chg = 10;				//ms
	water->delay_2nd_interval = 70;			//ms

	water->water_threshold = 780;
	water->dry_threshold = 1060;
	water->cap_threshold = 100;

	water_set_wakelock(water);
	mutex_init(&water->mutex);
	value.strval = (const char *)water;
	s2mf301_water_set_prop(water, POWER_SUPPLY_LSI_PROP_ENABLE_WATER, value);

	schedule_delayed_work(&water->state_work, 0);
}
