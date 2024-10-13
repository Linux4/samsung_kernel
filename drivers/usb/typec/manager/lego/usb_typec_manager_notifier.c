/*
 * Copyright (C) 2018 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) "type-c manager: " fmt

#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/notifier.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif

#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif
#include <linux/usb_notify.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/rtc.h>
#include <linux/of.h>

#include <linux/usb/typec/common/pdic_core.h>
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#include "../../../../battery/common/sec_charging_common.h"
#if defined(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#endif
#if defined(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif
#if defined(CONFIG_PDIC_SWITCH)
#include <linux/muic/common/muic.h>
#endif

/* dwc3 irq storm patch */
/* need to check dwc3 link state during dcd time out case */
extern int dwc3_gadget_get_cmply_link_state_wrapper(void);
#define DEBUG
#define SET_MANAGER_NOTIFIER_BLOCK(nb, fn, dev) do {	\
		(nb)->notifier_call = (fn);		\
		(nb)->priority = (dev);			\
	} while (0)

#define DESTROY_MANAGER_NOTIFIER_BLOCK(nb)			\
		SET_MANAGER_NOTIFIER_BLOCK(nb, NULL, -1)

enum NOTIFIER_ENUM {
	VBUS_NOTIFIER,
	PDIC_NOTIFIER,
	MUIC_NOTIFIER
};

static int manager_notifier_init_done;
static int confirm_manager_notifier_register;
static int manager_notifier_init(void);

#if defined(CONFIG_BATTERY_SAMSUNG)
extern unsigned int lpcharge;
#endif

struct device *manager_device;
manager_data_t typec_manager;
void set_usb_enumeration_state(int state);
static void manager_cable_type_check(bool state, int time);
#if defined(CONFIG_USB_HW_PARAM)
void calc_duration_time(unsigned long sTime, unsigned long eTime, unsigned long *dTime);
void wVbus_time_update(int mode);
void water_dry_time_update(int mode);
#endif

#if defined(CONFIG_PDIC_SWITCH)
static bool is_factory_jig;
#endif

static int manager_notifier_notify(void *data)
{
	MANAGER_NOTI_TYPEDEF manager_noti = *(MANAGER_NOTI_TYPEDEF *)data;
	int ret = 0;

	pr_info("%s: src:%s dest:%s id:%s sub1:%02x sub2:%02x sub3:%02x\n",
		__func__,
		(manager_noti.src < PDIC_NOTI_DEST_NUM) ?
		PDIC_NOTI_DEST_Print[manager_noti.src] : "unknown",
		(manager_noti.dest < PDIC_NOTI_DEST_NUM) ?
		PDIC_NOTI_DEST_Print[manager_noti.dest] : "unknown",
		(manager_noti.id < PDIC_NOTI_ID_NUM) ?
		PDIC_NOTI_ID_Print[manager_noti.id] : "unknown",
		manager_noti.sub1, manager_noti.sub2, manager_noti.sub3);

	if (manager_noti.dest == PDIC_NOTIFY_DEV_DP) {
		if (manager_noti.id == PDIC_NOTIFY_ID_DP_CONNECT) {
			typec_manager.dp_attach_state = manager_noti.sub1;
			typec_manager.dp_is_connect = 0;
			typec_manager.dp_hs_connect = 0;
		} else if (manager_noti.id == PDIC_NOTIFY_ID_DP_HPD) {
			typec_manager.dp_hpd_state = manager_noti.sub1;
		} else if (manager_noti.id == PDIC_NOTIFY_ID_DP_LINK_CONF) {
			typec_manager.dp_cable_type = manager_noti.sub1;
		}
	}

	if (manager_noti.dest == PDIC_NOTIFY_DEV_USB_DP) {
		if (manager_noti.id == PDIC_NOTIFY_ID_USB_DP) {
			typec_manager.dp_is_connect = manager_noti.sub1;
			typec_manager.dp_hs_connect = manager_noti.sub2;
		}
	}

	if (manager_noti.dest == PDIC_NOTIFY_DEV_USB) {
		if (typec_manager.pdic_drp_state == manager_noti.sub2)
			return 0;
		typec_manager.pdic_drp_state = manager_noti.sub2;
		if (typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_ATTACH_DFP) {
			pr_info("%s: call manager_cable_type_check\n", __func__);
			manager_cable_type_check(true, 0);
		}
		if (typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_DETACH)
			set_usb_enumeration_state(0);
	}

	if (manager_noti.dest == PDIC_NOTIFY_DEV_BATT
		&& manager_noti.sub3 == typec_manager.water_cable_type) {
		if (manager_noti.sub1 != typec_manager.wVbus_det) {
			typec_manager.wVbus_det = manager_noti.sub1;
#if defined(CONFIG_USB_HW_PARAM)
			if (typec_manager.water_det) {
				typec_manager.waterChg_count += manager_noti.sub1;
				wVbus_time_update(typec_manager.wVbus_det);
			}
#endif
		} else {
			return 0;
		}
	}

#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	store_usblog_notify(NOTIFY_MANAGER, (void *)data, NULL);
#endif

	ret = blocking_notifier_call_chain(&(typec_manager.manager_notifier),
				manager_noti.id, &manager_noti);

	switch (ret) {
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s: notify done(0x%x)\n", __func__, ret);
		break;
	case NOTIFY_STOP_MASK:
	case NOTIFY_BAD:
	default:
		if (manager_noti.dest == PDIC_NOTIFY_DEV_USB) {
			pr_info("%s: UPSM case (0x%x)\n", __func__, ret);
			typec_manager.is_MPSM = 1;
		} else {
			pr_info("%s: notify error occur(0x%x)\n", __func__, ret);
		}
		break;
	}

	return ret;
}

void manager_event_notifier(struct work_struct *data)
{
	struct typec_manager_event_work *event_work =
		container_of(data, struct typec_manager_event_work, typec_manager_work);
	MANAGER_NOTI_TYPEDEF manager_event_noti;

	manager_event_noti.src = event_work->src;
	manager_event_noti.dest = event_work->dest;
	manager_event_noti.id = event_work->id;
	manager_event_noti.sub1 = event_work->sub1;
	manager_event_noti.sub2 = event_work->sub2;
	manager_event_noti.sub3 = event_work->sub3;
	manager_event_noti.pd = typec_manager.pd;

	manager_notifier_notify(&manager_event_noti);
	kfree(event_work);
}

void manager_muic_event_notifier(struct work_struct *data)
{
	struct typec_manager_event_work *event_work =
		container_of(data, struct typec_manager_event_work, typec_manager_work);
	MANAGER_NOTI_TYPEDEF manager_event_noti;
	int ret = 0;

	manager_event_noti.src = event_work->src;
	manager_event_noti.dest = event_work->dest;
	manager_event_noti.id = event_work->id;
	manager_event_noti.sub1 = event_work->sub1;
	manager_event_noti.sub2 = event_work->sub2;
	manager_event_noti.sub3 = event_work->sub3;
	manager_event_noti.pd = typec_manager.pd;

	pr_info("%s: id:%s sub1:%02x sub2:%02x sub3:%02x\n",
		__func__,
		(manager_event_noti.id < PDIC_NOTI_ID_NUM) ?
		PDIC_NOTI_ID_Print[manager_event_noti.id] : "unknown",
		manager_event_noti.sub1, manager_event_noti.sub2, manager_event_noti.sub3);

#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	store_usblog_notify(NOTIFY_MANAGER, (void *)&manager_event_noti, NULL);
#endif

	ret = blocking_notifier_call_chain(&(typec_manager.manager_muic_notifier),
				manager_event_noti.id, &manager_event_noti);

	pr_info("%s: notify done(0x%x)\n", __func__, ret);
	kfree(event_work);
}

void manager_event_work(int src, int dest, int id, int sub1, int sub2, int sub3)
{
	struct typec_manager_event_work *event_work;

	pr_info("%s src:%s dest:%s\n", __func__,
		(src < PDIC_NOTI_DEST_NUM) ? PDIC_NOTI_DEST_Print[src] : "unknown",
		(dest < PDIC_NOTI_DEST_NUM) ? PDIC_NOTI_DEST_Print[dest] : "unknown");
	event_work = kmalloc(sizeof(struct typec_manager_event_work), GFP_ATOMIC);
	if (!event_work) {
		pr_err("%s: failed to alloc for event_work\n", __func__);
		return;
	}
	event_work->src = src;
	event_work->dest = dest;
	event_work->id = id;
	event_work->sub1 = sub1;
	event_work->sub2 = sub2;
	event_work->sub3 = sub3;

	if (event_work->dest == PDIC_NOTIFY_DEV_MUIC) {
		INIT_WORK(&event_work->typec_manager_work, manager_muic_event_notifier);
		queue_work(typec_manager.typec_manager_muic_wq, &event_work->typec_manager_work);
	}	else {
		INIT_WORK(&event_work->typec_manager_work, manager_event_notifier);
		queue_work(typec_manager.typec_manager_wq, &event_work->typec_manager_work);
	}
}

void set_usb_enumeration_state(int state)
{
	if (typec_manager.usb_enum_state != state) {
		typec_manager.usb_enum_state = state;

#if defined(CONFIG_USB_HW_PARAM)
		if (typec_manager.usb_enum_state >= 0x300)
			typec_manager.usb_superspeed_count++;
		else if (typec_manager.usb_enum_state >= 0x200)
			typec_manager.usb_highspeed_count++;
#endif
	}
}
EXPORT_SYMBOL(set_usb_enumeration_state);

bool get_usb_enumeration_state(void)
{
	return typec_manager.usb_enum_state ? 1 : 0;
}
EXPORT_SYMBOL(get_usb_enumeration_state);

#if defined(CONFIG_USB_HW_PARAM)
unsigned long get_waterdet_duration(void)
{
	unsigned long ret;
	struct timeval time;

	if (typec_manager.water_det) {
		do_gettimeofday(&time);
		calc_duration_time(typec_manager.waterDet_time,
			time.tv_sec, &typec_manager.waterDet_duration);
		typec_manager.waterDet_time = time.tv_sec;
	}

	ret = typec_manager.waterDet_duration / 60;  /* min */
	typec_manager.waterDet_duration -= ret*60;
	return ret;
}

unsigned long get_wvbus_duration(void)
{
	unsigned long ret = 0;
	struct timeval time;

	if (typec_manager.wVbus_det) {
		do_gettimeofday(&time);	/* time.tv_sec */
		calc_duration_time(typec_manager.wVbusHigh_time,
			time.tv_sec, &typec_manager.wVbus_duration);
		typec_manager.wVbusHigh_time = time.tv_sec;
	}

	ret = typec_manager.wVbus_duration;  /* sec */
	typec_manager.wVbus_duration = 0;
	return ret;
}

unsigned long manager_hw_param_update(int param)
{
	unsigned long ret = 0;

	switch (param) {
	case USB_CCIC_WATER_INT_COUNT:
		ret = typec_manager.water_count;
		typec_manager.water_count = 0;
		break;
	case USB_CCIC_DRY_INT_COUNT:
		ret = typec_manager.dry_count;
		typec_manager.dry_count = 0;
		break;
	case USB_CLIENT_SUPER_SPEED_COUNT:
		ret = typec_manager.usb_superspeed_count;
		typec_manager.usb_superspeed_count = 0;
		break;
	case USB_CLIENT_HIGH_SPEED_COUNT:
		ret = typec_manager.usb_highspeed_count;
		typec_manager.usb_highspeed_count = 0;
		break;
	case USB_CCIC_WATER_TIME_DURATION:
		ret = get_waterdet_duration();
		break;
#if defined(CONFIG_BATTERY_SAMSUNG)
	case USB_CCIC_WATER_VBUS_COUNT:
		if (!lpcharge) {
			ret = typec_manager.waterChg_count;
			typec_manager.waterChg_count = 0;
		}
		break;
	case USB_CCIC_WATER_LPM_VBUS_COUNT:
		if (lpcharge) {
			ret = typec_manager.waterChg_count;
			typec_manager.waterChg_count = 0;
		}
		break;
	case USB_CCIC_WATER_VBUS_TIME_DURATION:
		if (!lpcharge)
			ret = get_wvbus_duration();
		break;
	case USB_CCIC_WATER_LPM_VBUS_TIME_DURATION:
		if (lpcharge)
			ret = get_wvbus_duration();
		break;
#endif
	default:
		break;
	}

	return ret;
}

void calc_duration_time(unsigned long sTime, unsigned long eTime, unsigned long *dTime)
{
	unsigned long calcDtime;

	calcDtime = eTime - sTime;

	/* check for exception case. */
	if (calcDtime > 86400)
		calcDtime = 0;

	*dTime += calcDtime;
}

void water_dry_time_update(int mode)
{
	struct timeval time;
	struct rtc_time det_time;
	static int rtc_update_check = 1;

	do_gettimeofday(&time);
	if (rtc_update_check) {
		rtc_update_check = 0;
		rtc_time_to_tm(time.tv_sec, &det_time);
		pr_info("%s: year=%d\n", __func__,  det_time.tm_year);
		if (det_time.tm_year == 70) { /* (1970-01-01 00:00:00) */
			schedule_delayed_work(&typec_manager.rtctime_update_work, msecs_to_jiffies(5000));
		}
	}

	if (mode) {
		/* WATER */
		typec_manager.waterDet_time = time.tv_sec;
	} else {
		/* DRY */
		typec_manager.dryDet_time = time.tv_sec;
		calc_duration_time(typec_manager.waterDet_time,
			typec_manager.dryDet_time, &typec_manager.waterDet_duration);
	}
}

static void water_det_rtc_time_update(struct work_struct *work)
{
	struct timeval time;
	struct rtc_time rtctime;
	static int max_retry = 1;

	do_gettimeofday(&time);
	rtc_time_to_tm(time.tv_sec, &rtctime);
	if ((rtctime.tm_year == 70) && (max_retry < 5)) {
		/* (1970-01-01 00:00:00) */
		if (typec_manager.wVbus_det) {
			calc_duration_time(typec_manager.wVbusHigh_time,
				time.tv_sec, &typec_manager.wVbus_duration);
			typec_manager.wVbusHigh_time = time.tv_sec;
		}
		max_retry++;
		schedule_delayed_work(&typec_manager.rtctime_update_work, msecs_to_jiffies(5000));
	} else {
		if (typec_manager.water_det) {
			typec_manager.waterDet_time = time.tv_sec;
			typec_manager.waterDet_duration += max_retry*5;
			if (typec_manager.wVbus_det) {
				typec_manager.wVbusHigh_time = time.tv_sec;
				typec_manager.wVbus_duration += 5;
			}
		}
	}
}

void wVbus_time_update(int mode)
{
	struct timeval time;

	do_gettimeofday(&time);

	if (mode) {
		/* WVBUS HIGH */
		typec_manager.wVbusHigh_time = time.tv_sec;
	} else {
		/* WVBUS LOW */
		typec_manager.wVbusLow_time = time.tv_sec;
		calc_duration_time(typec_manager.wVbusHigh_time,
			typec_manager.wVbusLow_time, &typec_manager.wVbus_duration);
	}
}
#endif

#if defined(CONFIG_VBUS_NOTIFIER)
static int manager_check_vbus_by_otg(void)
{
	union power_supply_propval val;
	int otg_power = 0;
#ifdef MANAGER_DEBUG
	unsigned long cur_stamp;
	int otg_power_time = 0;
#endif

	psy_do_property("otg", get,
		POWER_SUPPLY_PROP_ONLINE, val);
	otg_power = val.intval;

	if (typec_manager.otg_stamp) {
#ifdef MANAGER_DEBUG
		cur_stamp = jiffies;
		otg_power_time = time_before(cur_stamp, typec_manager.otg_stamp+msecs_to_jiffies(300));
		pr_info("%s [OTG Accessory VBUS] duration-time=%u(ms), time_before(%d)\n",
			__func__, jiffies_to_msecs(cur_stamp-typec_manager.otg_stamp),
			otg_power_time);
		if (otg_power_time)
			typec_manager.vbus_by_otg_detection = 1;
#else
		if (time_before(jiffies, typec_manager.otg_stamp+msecs_to_jiffies(300)))
			typec_manager.vbus_by_otg_detection = 1;
#endif
			typec_manager.otg_stamp = 0;
	}

	otg_power |= typec_manager.vbus_by_otg_detection;

	pr_info("%s otg power? %d (otg?%d, vbusTimeCheck?%d)\n", __func__,
		otg_power, val.intval, typec_manager.vbus_by_otg_detection);
	return otg_power;
}

static int manager_get_otg_power_mode(void)
{
	union power_supply_propval val;
	int otg_power = 0;

	psy_do_property("otg", get,
		POWER_SUPPLY_PROP_ONLINE, val);
	otg_power = val.intval | typec_manager.vbus_by_otg_detection;

	pr_info("%s otg power? %d (otg?%d, vbusTimeCheck?%d)\n", __func__,
		otg_power, val.intval, typec_manager.vbus_by_otg_detection);
	return otg_power;
}
#endif

void set_usb_enable_state(void)
{
	if (!typec_manager.usb_enable_state) {
		typec_manager.usb_enable_state = true;
		if (typec_manager.pd_con_state)
			manager_cable_type_check(true, 120);
		else if (typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_ATTACH_UFP &&
			typec_manager.cable_type == MANAGER_NOTIFY_MUIC_TIMEOUT_OPEN_DEVICE)
			manager_cable_type_check(true, 10);
	}
}
EXPORT_SYMBOL(set_usb_enable_state);

void manager_notifier_usbdp_support(void)
{

	if (typec_manager.dp_check_done == 1) {
		manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_USB_DP,
			PDIC_NOTIFY_ID_USB_DP, typec_manager.dp_is_connect, typec_manager.dp_hs_connect, 0);

		typec_manager.dp_check_done = 0;
	}
}

static int manager_external_notifier_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	int ret = 0;
	int enable = *(int *)data;

	switch (action) {
	case EXTERNAL_NOTIFY_DEVICEADD:
		pr_info("%s EXTERNAL_NOTIFY_DEVICEADD, enable=%d\n", __func__, enable);
		pr_info("drp_state %d, pdic_attach_state %d, muic_attach_state %d\n",
			typec_manager.pdic_drp_state, typec_manager.pdic_attach_state, typec_manager.muic_attach_state);
		if (enable &&
			typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_ATTACH_DFP &&
			typec_manager.muic_attach_state != MUIC_NOTIFY_CMD_DETACH) {
			pr_info("%s: a usb device is added in host mode\n", __func__);
			/* USB Cable type */
			manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_BATT,
				PDIC_NOTIFY_ID_USB, 0, 0, PD_USB_TYPE);
		}
		break;
	default:
		break;
	}
	return ret;
}

static void manager_cable_type_check_work(struct work_struct *work)
{
	int dwc3_link_check = dwc3_gadget_get_cmply_link_state_wrapper();

	if ((typec_manager.pdic_drp_state != USB_STATUS_NOTIFY_ATTACH_UFP) ||
		typec_manager.is_MPSM || dwc3_link_check == 1) {
		pr_info("%s: skip case : dwc3_link = %d\n", __func__, dwc3_link_check);
		return;
	}
	pr_info("%s: usb=%d, pd=%d cable_type=%d, dwc3_link_check=%d\n", __func__,
		typec_manager.usb_enum_state, typec_manager.pd_con_state,
		typec_manager.cable_type, dwc3_link_check);

	if (!typec_manager.usb_enum_state ||
		typec_manager.cable_type == MANAGER_NOTIFY_MUIC_CHARGER) {
		/* TA cable Type */
		manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_USB,
			PDIC_NOTIFY_ID_USB, PDIC_NOTIFY_DETACH, USB_STATUS_NOTIFY_DETACH, 0);
	} else {
		/* USB cable Type */
		manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_BATT,
			PDIC_NOTIFY_ID_USB, 0, 0, PD_USB_TYPE);
	}
}

static void manager_cable_type_check(bool state, int time)
{
	if (typec_manager.usb_enable_state) {
		cancel_delayed_work_sync(&typec_manager.cable_check_work);
		if (state)
			schedule_delayed_work(&typec_manager.cable_check_work,
				msecs_to_jiffies(time*100));
	}
}

static void manager_usb_event_work(struct work_struct *work)
{
	pr_info("%s: working state=%d, vbus=%d\n", __func__,
		typec_manager.muic_attach_state_without_pdic,
		typec_manager.vbus_state);

	if (typec_manager.muic_attach_state_without_pdic) {
		switch (typec_manager.muic_attach_state) {
		case MUIC_NOTIFY_CMD_ATTACH:
#if defined(CONFIG_VBUS_NOTIFIER)
			if (typec_manager.vbus_state == STATUS_VBUS_HIGH ||
				typec_manager.muic_cable_type == ATTACHED_DEV_JIG_USB_OFF_MUIC)
#endif
			{
				manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_USB,
					PDIC_NOTIFY_ID_USB, PDIC_NOTIFY_ATTACH, USB_STATUS_NOTIFY_ATTACH_UFP, 0);
			}
			break;
		case MUIC_NOTIFY_CMD_DETACH:
			typec_manager.muic_attach_state_without_pdic = 0;
			manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_USB,
				PDIC_NOTIFY_ID_USB, PDIC_NOTIFY_DETACH, USB_STATUS_NOTIFY_DETACH, 0);
			break;
		default:
			break;
		}
	}
}

void manager_usb_cable_connection_event(int delay)
{
	schedule_delayed_work(&typec_manager.usb_event_work, msecs_to_jiffies(delay));
}

#if defined(CONFIG_VBUS_NOTIFIER)
void manager_handle_muic_event(int event)
{

	if (typec_manager.muic_fake_event_wq_processing) {
		typec_manager.muic_fake_event_wq_processing = 0;
		cancel_delayed_work_sync(&typec_manager.muic_event_work);
	}

	switch (event) {
	case EVENT_CANCEL:
		typec_manager.muic_attach_state_without_pdic = 0;
		break;
	case EVENT_LOAD:
		if (typec_manager.muic_attach_state_without_pdic
			|| typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_ATTACH_UFP) {
			schedule_delayed_work(&typec_manager.muic_event_work, msecs_to_jiffies(1000));
			typec_manager.muic_fake_event_wq_processing = 1;
		}
		break;
	default:
		break;
	}
}


static void manager_muic_event_work(struct work_struct *work)
{
	pr_info("%s: drp=%d, rid=%d, without_pdic=%d\n", __func__,
		typec_manager.pdic_drp_state,
		typec_manager.pdic_rid_state,
		typec_manager.muic_attach_state_without_pdic);

	typec_manager.muic_fake_event_wq_processing = 0;

	if (typec_manager.pdic_rid_state == RID_523K ||  typec_manager.pdic_rid_state == RID_619K
		|| typec_manager.cable_type == MANAGER_NOTIFY_MUIC_UART
		|| typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_ATTACH_DFP
		|| typec_manager.vbus_state == STATUS_VBUS_HIGH) {
		return;
	} else if (typec_manager.muic_attach_state == MUIC_NOTIFY_CMD_DETACH) {
		typec_manager.muic_attach_state_without_pdic = 1;
		manager_usb_cable_connection_event(0);
		return;
	}

	typec_manager.muic_attach_state_without_pdic = 1;
	manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_MUIC,
		PDIC_NOTIFY_ID_ATTACH, PDIC_NOTIFY_DETACH, 0, typec_manager.muic_cable_type);
}
#endif

static int manager_handle_pdic_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	MANAGER_NOTI_TYPEDEF p_noti = *(MANAGER_NOTI_TYPEDEF *)data;
	int ret = 0;

	pr_info("%s: src:%s dest:%s id:%s attach/rid:%d\n", __func__,
		(p_noti.src < PDIC_NOTI_DEST_NUM) ?
			PDIC_NOTI_DEST_Print[p_noti.src] : "unknown",
		(p_noti.dest < PDIC_NOTI_DEST_NUM) ?
			PDIC_NOTI_DEST_Print[p_noti.dest] : "unknown",
		(p_noti.id < PDIC_NOTI_ID_NUM) ?
			PDIC_NOTI_ID_Print[p_noti.id] : "unknown",
		p_noti.sub1);

#if defined(CONFIG_VBUS_NOTIFIER)
	if (p_noti.src != PDIC_NOTIFY_ID_INITIAL)
		manager_handle_muic_event(EVENT_CANCEL);
#endif

	switch (p_noti.id) {
	case PDIC_NOTIFY_ID_POWER_STATUS:
		if (p_noti.sub1) { /*attach*/
			typec_manager.pd_con_state = 1;	// PDIC_NOTIFY_EVENT_PD_SINK
			if ((typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_ATTACH_UFP) &&
				!typec_manager.is_MPSM) {
				pr_info("%s: PD charger + UFP\n", __func__);
				manager_cable_type_check(true, 60);
			}
		}
		p_noti.dest = PDIC_NOTIFY_DEV_BATT;
		if (typec_manager.pd == NULL)
			typec_manager.pd = p_noti.pd;
		break;
	case PDIC_NOTIFY_ID_ATTACH:		// for MUIC
			if (typec_manager.pdic_attach_state != p_noti.sub1) {
				typec_manager.pdic_attach_state = p_noti.sub1;
				typec_manager.is_MPSM = 0;
				if (typec_manager.pdic_attach_state == PDIC_NOTIFY_ATTACH) {
					pr_info("%s: PDIC_NOTIFY_ATTACH\n", __func__);
					typec_manager.water_det = 0;
					typec_manager.pd_con_state = 0;
					if (p_noti.sub2)
						typec_manager.otg_stamp = jiffies;
				}
			}

			if (typec_manager.pdic_attach_state == PDIC_NOTIFY_DETACH) {
				pr_info("%s: PDIC_NOTIFY_DETACH (pd=%d, cable_type=%d)\n", __func__,
					typec_manager.pd_con_state, typec_manager.cable_type);
				manager_cable_type_check(false, 0);
				if (typec_manager.pd_con_state) {
					typec_manager.pd_con_state = 0;
					manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_BATT,
						PDIC_NOTIFY_ID_ATTACH, PDIC_NOTIFY_DETACH, 0,
						ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC);
				}
			}
		break;
	case PDIC_NOTIFY_ID_RID:	// for MUIC (FAC)
		typec_manager.pdic_rid_state = p_noti.sub1;
		break;
	case PDIC_NOTIFY_ID_USB:	// for USB3
		if ((typec_manager.cable_type == MANAGER_NOTIFY_MUIC_CHARGER)
			|| (p_noti.sub2 != USB_STATUS_NOTIFY_DETACH && /*drp */
			(typec_manager.pdic_rid_state == RID_523K || typec_manager.pdic_rid_state == RID_619K))) {
			return 0;
		}
		if ((typec_manager.cable_type == MANAGER_NOTIFY_MUIC_TIMEOUT_OPEN_DEVICE)
			&& (p_noti.sub2 == USB_STATUS_NOTIFY_ATTACH_UFP)) {
			pr_info("%s: DCD Timeout case.\n", __func__);
			manager_cable_type_check(false, 0);
		} else if (p_noti.sub2 == USB_STATUS_NOTIFY_DETACH)
			manager_cable_type_check(false, 0);
		else
			;
		break;
	case PDIC_NOTIFY_ID_WATER:
		if (p_noti.sub1) {	/* attach */
			if (!typec_manager.water_det) {
				typec_manager.water_det = 1;
				manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_MUIC,
					PDIC_NOTIFY_ID_WATER, p_noti.sub1, p_noti.sub2, p_noti.sub3);
#if defined(CONFIG_USB_HW_PARAM)
				typec_manager.water_count++;
				/*update water time */
				water_dry_time_update((int)p_noti.sub1);
#endif

#if defined(CONFIG_VBUS_NOTIFIER)
				mutex_lock(&typec_manager.mo_lock);
				if (typec_manager.vbus_state == STATUS_VBUS_HIGH
						&& !manager_get_otg_power_mode())
					manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_BATT,
						p_noti.id, p_noti.sub1, p_noti.sub2, typec_manager.water_cable_type);
				mutex_unlock(&typec_manager.mo_lock);
#endif
			}
#if defined(CONFIG_SEC_ABC)
			sec_abc_send_event("MODULE=pdic@ERROR=water_det");
#endif
		} else {
			typec_manager.water_det = 0;
			manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_MUIC,
					PDIC_NOTIFY_ID_WATER, p_noti.sub1, p_noti.sub2, p_noti.sub3);
#if defined(CONFIG_USB_HW_PARAM)
			typec_manager.dry_count++;
			/* update run_dry time */
			water_dry_time_update((int)p_noti.sub1);
#endif
			if (typec_manager.wVbus_det)
				manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_BATT,
					p_noti.id, p_noti.sub1, p_noti.sub2, typec_manager.water_cable_type);
		}
		return 0;
	case PDIC_NOTIFY_ID_WATER_CABLE:
		/* Ignore no water case */
		if (!typec_manager.water_det)
			return 0;

		manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_MUIC,
			p_noti.id, p_noti.sub1, p_noti.sub2, p_noti.sub3);

		if (p_noti.sub1) {
			/* Send water cable event to battery */
			manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_BATT,
					PDIC_NOTIFY_ID_WATER, PDIC_NOTIFY_ATTACH,
					p_noti.sub2, typec_manager.water_cable_type);

			/* make detach event like hiccup case*/
			manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_BATT,
					PDIC_NOTIFY_ID_WATER, PDIC_NOTIFY_DETACH,
					p_noti.sub2, typec_manager.water_cable_type);
		}
		return 0;
	case PDIC_NOTIFY_ID_INITIAL:
		return 0;
	default:
		break;
	}

	manager_event_work(p_noti.src, p_noti.dest,
		p_noti.id, p_noti.sub1, p_noti.sub2, p_noti.sub3);

	return ret;
}

static void manager_set_alternate_mode(int listener)
{
	ppdic_data_t ppdic_data;
	struct device *pdic_device = get_pdic_device();

	pr_info("%s : listener %d\n", __func__, listener);

	if (!pdic_device) {
		pr_err("%s: pdic_device is null.\n", __func__);
		return;
	}

	ppdic_data = dev_get_drvdata(pdic_device);
	if (!ppdic_data) {
		pr_err("there is no set_enable_alternate_mode\n");
		return;
	}
	if (!ppdic_data->set_enable_alternate_mode) {
		pr_err("there is no set_enable_alternate_mode\n");
		return;
	}

	if (listener == MANAGER_NOTIFY_PDIC_BATTERY)
		typec_manager.alt_is_support |= PDIC_BATTERY;
	else if (listener == MANAGER_NOTIFY_PDIC_USB)
		typec_manager.alt_is_support |= PDIC_USB;
	else if (listener == MANAGER_NOTIFY_PDIC_DP)
		typec_manager.alt_is_support |= PDIC_DP;
	else
		pr_info("no support driver to start alternate mode\n");

	if (typec_manager.dp_is_support) {
		if (typec_manager.alt_is_support == (PDIC_DP|PDIC_USB|PDIC_BATTERY))
			ppdic_data->set_enable_alternate_mode(ALTERNATE_MODE_READY | ALTERNATE_MODE_START);

	} else {
		if (typec_manager.alt_is_support == (PDIC_USB|PDIC_BATTERY))
			ppdic_data->set_enable_alternate_mode(ALTERNATE_MODE_READY | ALTERNATE_MODE_START);
	}

}

static int manager_handle_muic_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	PD_NOTI_ATTACH_TYPEDEF p_noti = *(PD_NOTI_ATTACH_TYPEDEF *)data;

	pr_info("%s: src:%d attach:%d, cable_type:%d\n", __func__,
		p_noti.src, p_noti.attach, p_noti.cable_type);

	switch (p_noti.src) {
#ifdef CONFIG_USE_DEDICATED_MUIC
	case PDIC_NOTIFY_DEV_DEDICATED_MUIC:
#if defined(CONFIG_PDIC_SWITCH)
		{
			bool usb_en = false;
			bool is_jig_board = !!get_pdic_info();
			bool skip = false;

			switch (p_noti.cable_type) {
			case ATTACHED_DEV_JIG_UART_OFF_MUIC:
			case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
			case ATTACHED_DEV_JIG_UART_ON_MUIC:
				is_factory_jig = true;
				break;
			case ATTACHED_DEV_JIG_USB_OFF_MUIC:
			case ATTACHED_DEV_JIG_USB_ON_MUIC:
				is_factory_jig = true;
				usb_en = true;
				break;
			case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
			case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
			case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
			case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
			case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
			case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
				skip = true;
				break;
			default:
				if (!is_factory_jig && !is_jig_board)
					usb_en = true;
				break;
			}

			if (!skip) {
				if (usb_en)
					manager_event_work(PDIC_NOTIFY_DEV_MANAGER,
						PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
						PDIC_NOTIFY_ATTACH, USB_STATUS_NOTIFY_ATTACH_UFP, 0);
				else
					manager_event_work(PDIC_NOTIFY_DEV_MANAGER,
						PDIC_NOTIFY_DEV_USB, PDIC_NOTIFY_ID_USB,
						PDIC_NOTIFY_DETACH, USB_STATUS_NOTIFY_DETACH, 0);
			}
		}
#endif
		manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_SUB_BATTERY,
			p_noti.id, p_noti.attach, p_noti.rprd, p_noti.cable_type);
		return 0;
#endif
#ifdef CONFIG_USE_SECOND_MUIC
	case PDIC_NOTIFY_DEV_SECOND_MUIC:
		typec_manager.second_muic_attach_state = p_noti.attach;
		typec_manager.second_muic_cable_type = p_noti.cable_type;
		manager_event_work(p_noti.src, PDIC_NOTIFY_DEV_SUB_BATTERY,
			p_noti.id, p_noti.attach, p_noti.rprd, p_noti.cable_type);
		return 0;
#endif
	case PDIC_NOTIFY_DEV_MUIC:
	default:
		typec_manager.muic_attach_state = p_noti.attach;
		typec_manager.muic_cable_type = p_noti.cable_type;
		break;
	}

	if (typec_manager.water_det) {
		if (p_noti.attach)
			typec_manager.muic_attach_state_without_pdic = 1;
		pr_info("%s: Water detected case\n", __func__);
		return 0;
	}

	if (p_noti.attach &&  typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_DETACH) {
		if (typec_manager.pdic_rid_state != RID_523K &&
			typec_manager.pdic_rid_state != RID_619K)
			typec_manager.muic_attach_state_without_pdic = 1;
	}

	switch (p_noti.cable_type) {
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		typec_manager.muic_attach_state_without_pdic = 1;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		pr_info("%s: USB(%d) %s, PDIC: %s\n", __func__,
			p_noti.cable_type, p_noti.attach ? "Attached" : "Detached",
			typec_manager.pdic_attach_state ? "Attached" : "Detached");

		if (p_noti.attach)
			typec_manager.cable_type = MANAGER_NOTIFY_MUIC_USB;

		if (typec_manager.muic_attach_state_without_pdic)
			manager_usb_cable_connection_event(p_noti.attach*2000);
		break;

	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		if (p_noti.attach)
			typec_manager.cable_type = MANAGER_NOTIFY_MUIC_UART;
		break;

	case ATTACHED_DEV_TA_MUIC:
		pr_info("%s: TA(%d) %s\n", __func__, p_noti.cable_type,
			p_noti.attach ? "Attached" : "Detached");

		if (p_noti.attach)
			typec_manager.cable_type = MANAGER_NOTIFY_MUIC_CHARGER;

		if (p_noti.attach && typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_ATTACH_UFP) {
			if (typec_manager.pd_con_state)
				manager_cable_type_check(false, 0);
			/* Turn off the USB Phy when connected to the charger */
			manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_USB,
				PDIC_NOTIFY_ID_USB, PDIC_NOTIFY_DETACH, USB_STATUS_NOTIFY_DETACH, 0);
		}
		break;

	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
		pr_info("%s: AFC or QC Prepare(%d) %s\n", __func__,
			p_noti.cable_type, p_noti.attach ? "Attached" : "Detached");
		break;

	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		pr_info("%s: DCD Timeout device is detected(%d) %s\n",
			__func__, p_noti.cable_type,
			p_noti.attach ? "Attached" : "Detached");

		if (p_noti.attach) {
			typec_manager.cable_type = MANAGER_NOTIFY_MUIC_TIMEOUT_OPEN_DEVICE;
			if (typec_manager.pdic_drp_state == USB_STATUS_NOTIFY_ATTACH_UFP) {
				pr_info("%s: DCD Timeout case schedule work enable_state[%d]\n",
					__func__, typec_manager.usb_enable_state);
				manager_cable_type_check(true, 10);
			}
		}
		break;

	default:
		pr_info("%s: Cable(%d) %s\n", __func__, p_noti.cable_type,
			p_noti.attach ? "Attached" : "Detached");
		break;
	}

	if (!p_noti.attach)
		typec_manager.cable_type = MANAGER_NOTIFY_MUIC_NONE;

	if (!(p_noti.attach) && typec_manager.pd_con_state &&
			p_noti.cable_type != typec_manager.water_cable_type) {
		pr_info("%s: Don't send the MUIC detach event when the PD charger is connected\n", __func__);
	} else
		manager_event_work(PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_DEV_BATT,
			p_noti.id, p_noti.attach, p_noti.rprd, p_noti.cable_type);

	return 0;
}

#if defined(CONFIG_VBUS_NOTIFIER)
static int manager_handle_vbus_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	vbus_status_t vbus_type = *(vbus_status_t *)data;

	mutex_lock(&typec_manager.mo_lock);
	pr_info("%s: cmd=%lu, vbus_type=%s, WATER DET=%d ATTACH=%s (%d)\n", __func__,
		action, vbus_type == STATUS_VBUS_HIGH ? "HIGH" : "LOW", typec_manager.water_det,
		typec_manager.pdic_attach_state == PDIC_NOTIFY_ATTACH ? "ATTACH":"DETACH",
		typec_manager.muic_attach_state_without_pdic);

	typec_manager.vbus_state = vbus_type;

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:
		if (!manager_check_vbus_by_otg() && typec_manager.water_det)
			manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_BATT,
				PDIC_NOTIFY_ID_WATER, PDIC_NOTIFY_ATTACH, 0, typec_manager.water_cable_type);
		break;
	case STATUS_VBUS_LOW:
		typec_manager.vbus_by_otg_detection = 0;
		if (typec_manager.wVbus_det)
			manager_event_work(PDIC_NOTIFY_DEV_MANAGER, PDIC_NOTIFY_DEV_BATT,
				PDIC_NOTIFY_ID_ATTACH, PDIC_NOTIFY_DETACH, 0, typec_manager.water_cable_type);
		manager_handle_muic_event(EVENT_LOAD);
		break;
	default:
		break;
	}

	mutex_unlock(&typec_manager.mo_lock);
	return 0;
}
#endif

int manager_notifier_register(struct notifier_block *nb, notifier_fn_t notifier,
			manager_notifier_device_t listener)
{
	int ret = 0;
	MANAGER_NOTI_TYPEDEF m_noti = {0, };
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	pr_info("%s: listener=%d register\n", __func__, listener);
	if (!manager_notifier_init_done)
		manager_notifier_init();

	pdic_notifier_init();

	/* Check if MANAGER Notifier is ready. */
	if (!manager_device) {
		pr_err("%s: Not Initialized...\n", __func__);
		return -1;
	}

	if (listener == MANAGER_NOTIFY_PDIC_MUIC || listener == MANAGER_NOTIFY_PDIC_SENSORHUB) {
		SET_MANAGER_NOTIFIER_BLOCK(nb, notifier, listener);
		ret = blocking_notifier_chain_register(&(typec_manager.manager_muic_notifier), nb);
		if (ret < 0)
			pr_err("%s: muic blocking_notifier_chain_register error(%d)\n",
					__func__, ret);
	} else {
		SET_MANAGER_NOTIFIER_BLOCK(nb, notifier, listener);
		ret = blocking_notifier_chain_register(&(typec_manager.manager_notifier), nb);
		if (ret < 0)
			pr_err("%s: manager blocking_notifier_chain_register error(%d)\n",
					__func__, ret);
	}

	switch (listener) {
	case MANAGER_NOTIFY_PDIC_BATTERY:
		m_noti.src = PDIC_NOTIFY_DEV_MANAGER;
		m_noti.dest = PDIC_NOTIFY_DEV_BATT;
		m_noti.pd = typec_manager.pd;
		if (typec_manager.water_det) {
			if (typec_manager.muic_attach_state
#if defined(CONFIG_VBUS_NOTIFIER)
				|| typec_manager.vbus_state == STATUS_VBUS_HIGH
#endif
			) {
				m_noti.id = PDIC_NOTIFY_ID_WATER;
				m_noti.sub1 = PDIC_NOTIFY_ATTACH;
				m_noti.sub3 = typec_manager.water_cable_type;
			}
		} else {
			m_noti.id = PDIC_NOTIFY_ID_ATTACH;
			if (typec_manager.pd_con_state) {
				m_noti.id = PDIC_NOTIFY_ID_POWER_STATUS;
				m_noti.sub1 = PDIC_NOTIFY_ATTACH;
			} else if (typec_manager.muic_attach_state) {
				m_noti.sub1 = PDIC_NOTIFY_ATTACH;
				m_noti.sub3 = typec_manager.muic_cable_type;
			}
		}
		pr_info("%s: [BATTERY] id:%s, cable_type=%d %s\n", __func__,
			(m_noti.id < PDIC_NOTI_ID_NUM) ?
			PDIC_NOTI_ID_Print[m_noti.id] : "unknown",
			m_noti.sub3, m_noti.sub1 ? "Attached" : "Detached");
		nb->notifier_call(nb, m_noti.id, &(m_noti));
		manager_set_alternate_mode(listener);
		break;
	case MANAGER_NOTIFY_PDIC_SUB_BATTERY:
		m_noti.src = PDIC_NOTIFY_DEV_MANAGER;
		m_noti.dest = PDIC_NOTIFY_DEV_SUB_BATTERY;
		m_noti.id = PDIC_NOTIFY_ID_ATTACH;
#ifdef CONFIG_USE_SECOND_MUIC
		if (typec_manager.second_muic_attach_state) {
			m_noti.sub1 = PDIC_NOTIFY_ATTACH;
			m_noti.sub3 = typec_manager.second_muic_cable_type;
		}
#endif
		pr_info("%s: [BATTERY2] cable_type=%d %s\n", __func__,
			m_noti.sub3, m_noti.sub1 ? "Attached" : "Detached");
		nb->notifier_call(nb, m_noti.id, &(m_noti));
		break;
	case MANAGER_NOTIFY_PDIC_USB:
		m_noti.src = PDIC_NOTIFY_DEV_MANAGER;
		m_noti.dest = PDIC_NOTIFY_DEV_USB;
		m_noti.id = PDIC_NOTIFY_ID_USB;
		if (!typec_manager.water_det && typec_manager.cable_type != MANAGER_NOTIFY_MUIC_CHARGER) {
			if (typec_manager.pdic_drp_state) {
				m_noti.sub1 = typec_manager.pdic_attach_state;
				m_noti.sub2 = typec_manager.pdic_drp_state;
			} else if (typec_manager.cable_type == MANAGER_NOTIFY_MUIC_USB) {
				m_noti.sub1 = typec_manager.muic_attach_state;
				typec_manager.pdic_drp_state = USB_STATUS_NOTIFY_ATTACH_UFP;
				m_noti.sub2 = USB_STATUS_NOTIFY_ATTACH_UFP;
			}
		}
		pr_info("%s: [USB] drp:%s\n", __func__,
			PDIC_NOTI_USB_STATUS_Print[m_noti.sub2]);
		nb->notifier_call(nb, m_noti.id, &(m_noti));
		manager_set_alternate_mode(listener);
#if defined(CONFIG_USB_HW_PARAM)
		if (o_notify)
			register_hw_param_manager(o_notify, manager_hw_param_update);
#endif
		break;
	case MANAGER_NOTIFY_PDIC_DP:
		m_noti.src = PDIC_NOTIFY_DEV_MANAGER;
		m_noti.dest = PDIC_NOTIFY_DEV_DP;
		if (typec_manager.dp_attach_state == PDIC_NOTIFY_ATTACH) {
			m_noti.id = PDIC_NOTIFY_ID_DP_CONNECT;
			m_noti.sub1 = typec_manager.dp_attach_state;
			nb->notifier_call(nb, m_noti.id, &(m_noti));

			m_noti.id = PDIC_NOTIFY_ID_DP_LINK_CONF;
			m_noti.sub1 = typec_manager.dp_cable_type;
			nb->notifier_call(nb, m_noti.id, &(m_noti));

			if (typec_manager.dp_hpd_state == PDIC_NOTIFY_HIGH) {
				m_noti.id = PDIC_NOTIFY_ID_DP_HPD;
				m_noti.sub1 = typec_manager.dp_hpd_state;
				nb->notifier_call(nb, m_noti.id, &(m_noti));
			}
		}
		manager_set_alternate_mode(listener);
		break;
	default:
		break;
	}

	return ret;
}

int manager_notifier_unregister(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s: listener=%d unregister\n", __func__, nb->priority);

	if (nb->priority == MANAGER_NOTIFY_PDIC_MUIC || nb->priority == MANAGER_NOTIFY_PDIC_SENSORHUB) {
		ret = blocking_notifier_chain_unregister(&(typec_manager.manager_muic_notifier), nb);
		if (ret < 0)
			pr_err("%s: muic blocking_notifier_chain_unregister error(%d)\n",
					__func__, ret);
		DESTROY_MANAGER_NOTIFIER_BLOCK(nb);
	} else {
		ret = blocking_notifier_chain_unregister(&(typec_manager.manager_notifier), nb);
		if (ret < 0)
			pr_err("%s: pdic blocking_notifier_chain_unregister error(%d)\n",
					__func__, ret);
		DESTROY_MANAGER_NOTIFIER_BLOCK(nb);
	}
	return ret;
}

static void delayed_manger_notifier_init(struct work_struct *work)
{
	int ret = 0;
	int notifier_result = 0;
	static int retry_count = 1;
	int max_retry_count = 5;

	pr_info("%s : %d = times!\n", __func__, retry_count);
#if defined(CONFIG_VBUS_NOTIFIER)
	if (confirm_manager_notifier_register & (1 << VBUS_NOTIFIER)) {
		ret = vbus_notifier_register(&typec_manager.vbus_nb,
				manager_handle_vbus_notification, VBUS_NOTIFY_DEV_MANAGER);
		if (ret)
			notifier_result |= (1 << VBUS_NOTIFIER);
	}
#endif
	if (confirm_manager_notifier_register & (1 << PDIC_NOTIFIER)) {
		ret = pdic_notifier_register(&typec_manager.pdic_nb,
				manager_handle_pdic_notification, PDIC_NOTIFY_DEV_MANAGER);
		if (ret)
			notifier_result |= (1 << PDIC_NOTIFIER);
	}

	if (confirm_manager_notifier_register & (1 << MUIC_NOTIFIER)) {
		ret = muic_notifier_register(&typec_manager.muic_nb,
				manager_handle_muic_notification, MUIC_NOTIFY_DEV_MANAGER);
		if (ret)
			notifier_result |= (1 << MUIC_NOTIFIER);
	}

	confirm_manager_notifier_register = notifier_result;
	pr_info("%s : result of register = %d!\n",
		__func__, confirm_manager_notifier_register);

	if (confirm_manager_notifier_register) {
		pr_err("Manager notifier init time is %d.\n", retry_count);
		if (retry_count++ != max_retry_count)
			schedule_delayed_work(&typec_manager.manager_init_work,
				msecs_to_jiffies(2000));
		else
			pr_err("fail to init manager notifier\n");
	} else
		pr_info("%s : done!\n", __func__);
}

static int manager_notifier_init(void)
{
	struct device_node *np = NULL;
	struct device *pdic_device = get_pdic_device();
	ppdic_data_t ppdic_data = NULL;
	int ret = 0;
	int notifier_result = 0;

	pr_info("%s\n", __func__);

	if (!pdic_device)
		pr_err("%s: pdic_device is null.\n", __func__);
	else
		ppdic_data = dev_get_drvdata(pdic_device);

	pdic_notifier_init();

	if (manager_notifier_init_done) {
		pr_err("%s already registered\n", __func__);
		goto out;
	}

	manager_notifier_init_done = 1;

#if defined(CONFIG_DRV_SAMSUNG)
	manager_device = sec_device_create(0, NULL, "typec_manager");
#endif

	if (IS_ERR(manager_device)) {
		pr_err("%s Failed to create device(switch)!\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	typec_manager.pdic_attach_state = PDIC_NOTIFY_DETACH;
	typec_manager.pdic_drp_state = USB_STATUS_NOTIFY_DETACH;
	typec_manager.muic_attach_state = MUIC_NOTIFY_CMD_DETACH;
	typec_manager.muic_cable_type = ATTACHED_DEV_NONE_MUIC;
#ifdef CONFIG_USE_SECOND_MUIC
	typec_manager.second_muic_attach_state = MUIC_NOTIFY_CMD_DETACH;
	typec_manager.second_muic_cable_type = ATTACHED_DEV_NONE_MUIC;
#endif
	typec_manager.cable_type = MANAGER_NOTIFY_MUIC_NONE;
	typec_manager.usb_enum_state = 0;
	typec_manager.otg_stamp = 0;
	typec_manager.vbus_by_otg_detection = 0;
	typec_manager.water_det = 0;
	typec_manager.wVbus_det = 0;
#if defined(CONFIG_USB_HW_PARAM)
	typec_manager.water_count = 0;
	typec_manager.dry_count = 0;
	typec_manager.usb_highspeed_count = 0;
	typec_manager.usb_superspeed_count = 0;
	typec_manager.waterChg_count = 0;
	typec_manager.waterDet_duration = 0;
	typec_manager.wVbus_duration = 0;
#endif
	typec_manager.alt_is_support = 0;
	typec_manager.dp_is_support = 0;
	typec_manager.dp_is_connect = 0;
	typec_manager.dp_hs_connect = 0;
	typec_manager.dp_check_done = 1;
	usb_external_notify_register(&typec_manager.manager_external_notifier_nb,
		manager_external_notifier_notification, EXTERNAL_NOTIFY_DEV_MANAGER);
	typec_manager.muic_attach_state_without_pdic = 0;
#if defined(CONFIG_VBUS_NOTIFIER)
	typec_manager.muic_fake_event_wq_processing = 0;
#endif
	typec_manager.vbus_state = 0;
	typec_manager.is_MPSM = 0;
	typec_manager.pdic_rid_state = RID_UNDEFINED;
	typec_manager.pd = NULL;
#if defined(CONFIG_HICCUP_CHARGER) && defined(CONFIG_BATTERY_SAMSUNG)
	typec_manager.water_cable_type = lpcharge ?
		ATTACHED_DEV_UNDEFINED_RANGE_MUIC :
		ATTACHED_DEV_HICCUP_MUIC;
#else
	typec_manager.water_cable_type = ATTACHED_DEV_UNDEFINED_RANGE_MUIC;
#endif

	typec_manager.typec_manager_wq =
		create_singlethread_workqueue("typec_manager_event");
	typec_manager.typec_manager_muic_wq =
		create_singlethread_workqueue("typec_manager_muic_event");

	BLOCKING_INIT_NOTIFIER_HEAD(&(typec_manager.manager_notifier));
	BLOCKING_INIT_NOTIFIER_HEAD(&(typec_manager.manager_muic_notifier));

	INIT_DELAYED_WORK(&typec_manager.manager_init_work,
		delayed_manger_notifier_init);

	INIT_DELAYED_WORK(&typec_manager.cable_check_work,
		manager_cable_type_check_work);

	INIT_DELAYED_WORK(&typec_manager.usb_event_work,
		manager_usb_event_work);

#if defined(CONFIG_USB_HW_PARAM)
	INIT_DELAYED_WORK(&typec_manager.rtctime_update_work,
		water_det_rtc_time_update);
#endif

	if (ppdic_data && ppdic_data->set_enable_alternate_mode)
		ppdic_data->set_enable_alternate_mode(ALTERNATE_MODE_NOT_READY);
#if defined(CONFIG_VBUS_NOTIFIER)
	INIT_DELAYED_WORK(&typec_manager.muic_event_work,
		manager_muic_event_work);
#endif
	mutex_init(&typec_manager.mo_lock);

	/*
	 * Register manager handler to pdic notifier block list
	 */
#if defined(CONFIG_VBUS_NOTIFIER)
	ret = vbus_notifier_register(&typec_manager.vbus_nb,
			manager_handle_vbus_notification, VBUS_NOTIFY_DEV_MANAGER);
	if (ret)
		notifier_result |= (1 << VBUS_NOTIFIER);
#endif
	ret = pdic_notifier_register(&typec_manager.pdic_nb,
			manager_handle_pdic_notification, PDIC_NOTIFY_DEV_MANAGER);
	if (ret)
		notifier_result |= (1 << PDIC_NOTIFIER);
	ret = muic_notifier_register(&typec_manager.muic_nb,
			manager_handle_muic_notification, MUIC_NOTIFY_DEV_MANAGER);
	if (ret)
		notifier_result |= (1 << MUIC_NOTIFIER);

	confirm_manager_notifier_register = notifier_result;
	pr_info("%s : result of register = %d!\n",
		__func__, confirm_manager_notifier_register);

	if (confirm_manager_notifier_register)
		schedule_delayed_work(&typec_manager.manager_init_work, msecs_to_jiffies(2000));
	else
		pr_info("%s : done!\n", __func__);

	np = of_find_node_by_name(NULL, "displayport");
	if (!of_get_property(np, "dp,displayport_not_support", NULL)) {
		pr_info("%s: usb_host: support DP\n", __func__);
		typec_manager.dp_is_support = 1;
	} else {
		pr_info("%s: usb_host: no support DP\n", __func__);
		typec_manager.dp_is_support = 0;
	}

	pr_info("%s end\n", __func__);
out:
	return ret;
}

static void __exit manager_notifier_exit(void)
{
	pr_info("%s exit\n", __func__);
	mutex_destroy(&typec_manager.mo_lock);
#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_unregister(&typec_manager.vbus_nb);
#endif
	pdic_notifier_unregister(&typec_manager.pdic_nb);
	muic_notifier_unregister(&typec_manager.muic_nb);
	usb_external_notify_unregister(&typec_manager.manager_external_notifier_nb);
}

late_initcall(manager_notifier_init);
module_exit(manager_notifier_exit);
