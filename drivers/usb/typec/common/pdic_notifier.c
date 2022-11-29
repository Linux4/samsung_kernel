/*
 * Copyrights (C) 2016-2019 Samsung Electronics, Inc.
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
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/usb_notify.h>
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
#include <linux/battery/battery_notifier.h>
#endif
#include <linux/usb/typec/common/pdic_core.h>
#include <linux/usb/typec/common/pdic_sysfs.h>
#include <linux/usb/typec/common/pdic_notifier.h>

#define DEBUG
#define SET_PDIC_NOTIFIER_BLOCK(nb, fn, dev) do {	\
		(nb)->notifier_call = (fn);		\
		(nb)->priority = (dev);			\
	} while (0)

#define DESTROY_PDIC_NOTIFIER_BLOCK(nb)			\
		SET_PDIC_NOTIFIER_BLOCK(nb, NULL, -1)

static struct pdic_notifier_data pdic_notifier;

static int pdic_notifier_init_done;

char PDIC_NOTI_DEST_Print[PDIC_NOTI_DEST_NUM][10] = {
	[PDIC_NOTIFY_DEV_INITIAL]			= {"INITIAL"},
	[PDIC_NOTIFY_DEV_USB]				= {"USB"},
	[PDIC_NOTIFY_DEV_BATT]			= {"BATTERY"},
	[PDIC_NOTIFY_DEV_PDIC]				= {"PDIC"},
	[PDIC_NOTIFY_DEV_MUIC]				= {"MUIC"},
	[PDIC_NOTIFY_DEV_PDIC]				= {"PDIC"},
	[PDIC_NOTIFY_DEV_MANAGER]			= {"MANAGER"},
	[PDIC_NOTIFY_DEV_DP]				= {"DP"},
	[PDIC_NOTIFY_DEV_USB_DP]			= {"USBDP"},
	[PDIC_NOTIFY_DEV_SUB_BATTERY]		= {"BATTERY2"},
	[PDIC_NOTIFY_DEV_SECOND_MUIC]		= {"MUIC2"},
	[PDIC_NOTIFY_DEV_ALL]				= {"ALL"},
};

char PDIC_NOTI_ID_Print[PDIC_NOTI_ID_NUM][20] = {
	[PDIC_NOTIFY_ID_INITIAL] 		= {"ID_INITIAL"},
	[PDIC_NOTIFY_ID_ATTACH] 		= {"ID_ATTACH"},
	[PDIC_NOTIFY_ID_RID] 			= {"ID_RID"},
	[PDIC_NOTIFY_ID_USB]			= {"ID_USB"},
	[PDIC_NOTIFY_ID_POWER_STATUS] 	= {"ID_POWER_STATUS"},
	[PDIC_NOTIFY_ID_WATER]			= {"ID_WATER"},
	[PDIC_NOTIFY_ID_VCONN]			= {"ID_VCONN"},
	[PDIC_NOTIFY_ID_OTG]			= {"ID_OTG"},
	[PDIC_NOTIFY_ID_TA]				= {"ID_TA"},
	[PDIC_NOTIFY_ID_DP_CONNECT]		= {"ID_DP_CONNECT"},
	[PDIC_NOTIFY_ID_DP_HPD]			= {"ID_DP_HPD"},
	[PDIC_NOTIFY_ID_DP_LINK_CONF]	= {"ID_DP_LINK_CONF"},
	[PDIC_NOTIFY_ID_USB_DP]			= {"ID_USB_DP"},
	[PDIC_NOTIFY_ID_ROLE_SWAP]		= {"ID_ROLE_SWAP"},
	[PDIC_NOTIFY_ID_FAC]			= {"ID_FAC"},
	[PDIC_NOTIFY_ID_PD_PIN_STATUS]	= {"ID_PIN_STATUS"},
	[PDIC_NOTIFY_ID_WATER_CABLE]	= {"ID_WATER_CABLE"},
};

char PDIC_NOTI_RID_Print[PDIC_NOTI_RID_NUM][15] = {
	[RID_UNDEFINED] = {"RID_UNDEFINED"},
	[RID_000K]		= {"RID_000K"},
	[RID_001K]		= {"RID_001K"},
	[RID_255K]		= {"RID_255K"},
	[RID_301K]		= {"RID_301K"},
	[RID_523K]		= {"RID_523K"},
	[RID_619K]		= {"RID_619K"},
	[RID_OPEN]		= {"RID_OPEN"},
};

char PDIC_NOTI_USB_STATUS_Print[PDIC_NOTI_USB_STATUS_NUM][20] = {
	[USB_STATUS_NOTIFY_DETACH]		= {"USB_DETACH"},
	[USB_STATUS_NOTIFY_ATTACH_DFP]		= {"USB_ATTACH_DFP"},
	[USB_STATUS_NOTIFY_ATTACH_UFP]		= {"USB_ATTACH_UFP"},
	[USB_STATUS_NOTIFY_ATTACH_DRP]		= {"USB_ATTACH_DRP"},
};

char PDIC_NOTI_PIN_STATUS_Print[PDIC_NOTI_PIN_STATUS_NUM][20] = {
	[PDIC_NOTIFY_PIN_STATUS_NO_DETERMINATION]	= {"NO_DETERMINATION"},
	[PDIC_NOTIFY_PIN_STATUS_PD1_ACTIVE]			= {"PD1_ACTIVE"},
	[PDIC_NOTIFY_PIN_STATUS_PD2_ACTIVE]			= {"PD2_ACTIVE"},
	[PDIC_NOTIFY_PIN_STATUS_AUDIO_ACCESSORY]	= {"AUDIO_ACCESSORY"},
	[PDIC_NOTIFY_PIN_STATUS_DEBUG_ACCESSORY]	= {"DEBUG_ACCESSORY"},
	[PDIC_NOTIFY_PIN_STATUS_PDIC_ERROR]			= {"PDIC_ERROR"},
	[PDIC_NOTIFY_PIN_STATUS_DISABLED]			= {"DISABLED"},
	[PDIC_NOTIFY_PIN_STATUS_RFU]				= {"RFU"},
};

int pdic_notifier_register(struct notifier_block *nb, notifier_fn_t notifier,
			pdic_notifier_device listener)
{
	int ret = 0;
	struct device *pdic_device = get_pdic_device();

	if (!pdic_device) {
		pr_err("%s: pdic_device is null.\n", __func__);
		return -ENODEV;
	}
	pr_info("%s: listener=%d register\n", __func__, listener);

	/* Check if PDIC Notifier is ready. */
	if (!pdic_notifier_init_done)
		pdic_notifier_init();

	SET_PDIC_NOTIFIER_BLOCK(nb, notifier, listener);
	ret = blocking_notifier_chain_register(&(pdic_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_register error(%d)\n",
				__func__, ret);

	/* current pdic's attached_device status notify */
	nb->notifier_call(nb, 0,
			&(pdic_notifier.pdic_template));

	return ret;
}

int pdic_notifier_unregister(struct notifier_block *nb)
{
	int ret = 0;

	pr_info("%s: listener=%d unregister\n", __func__, nb->priority);

	ret = blocking_notifier_chain_unregister(&(pdic_notifier.notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_unregister error(%d)\n",
				__func__, ret);
	DESTROY_PDIC_NOTIFIER_BLOCK(nb);

	return ret;
}

static void pdic_uevent_work(int id, int state)
{
	char *water[2] = { "PDIC=WATER", NULL };
	char *dry[2] = { "PDIC=DRY", NULL };
	char *vconn[2] = { "PDIC=VCONN", NULL };
#if defined(CONFIG_SEC_FACTORY)
	char pdicrid[15] = {0,};
	char *rid[2] = {pdicrid, NULL};
	char pdicFacErr[20] = {0,};
	char *facErr[2] = {pdicFacErr, NULL};
	char pdicPinStat[20] = {0,};
	char *pinStat[2] = {pdicPinStat, NULL};
#endif
	struct device *pdic_device = get_pdic_device();

	if (!pdic_device) {
		pr_info("pdic_dev is null\n");
		return;
	}

	pr_info("usb: %s: id=%s state=%d\n", __func__, PDIC_NOTI_ID_Print[id], state);

	switch (id) {
	case PDIC_NOTIFY_ID_WATER:
		if (state)
			kobject_uevent_env(&pdic_device->kobj, KOBJ_CHANGE, water);
		else
			kobject_uevent_env(&pdic_device->kobj, KOBJ_CHANGE, dry);
		break;
	case PDIC_NOTIFY_ID_VCONN:
		kobject_uevent_env(&pdic_device->kobj, KOBJ_CHANGE, vconn);
		break;
#if defined(CONFIG_SEC_FACTORY)
	case PDIC_NOTIFY_ID_RID:
		snprintf(pdicrid, sizeof(pdicrid), "%s",
			(state < PDIC_NOTI_RID_NUM) ? PDIC_NOTI_RID_Print[state] : PDIC_NOTI_RID_Print[0]);
		kobject_uevent_env(&pdic_device->kobj, KOBJ_CHANGE, rid);
		break;
	case PDIC_NOTIFY_ID_FAC:
		snprintf(pdicFacErr, sizeof(pdicFacErr), "%s:%d",
			"ERR_STATE", state);
		kobject_uevent_env(&pdic_device->kobj, KOBJ_CHANGE, facErr);
		break;
	case PDIC_NOTIFY_ID_PD_PIN_STATUS:
		snprintf(pdicPinStat, sizeof(pdicPinStat), "%s",
			(state < PDIC_NOTI_PIN_STATUS_NUM) ?
			PDIC_NOTI_PIN_STATUS_Print[state] : PDIC_NOTI_PIN_STATUS_Print[0]);
		kobject_uevent_env(&pdic_device->kobj, KOBJ_CHANGE, pinStat);
		break;
#endif
	default:
		break;
	}
}

/* pdic's attached_device attach broadcast */
int pdic_notifier_notify(PD_NOTI_TYPEDEF *p_noti, void *pd, int pdic_attach)
{
	int ret = 0;

	pdic_notifier.pdic_template = *p_noti;

	switch (p_noti->id) {
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	case PDIC_NOTIFY_ID_POWER_STATUS:		/* PDIC_NOTIFY_EVENT_PD_SINK */
		pr_info("%s: src:%01x dest:%01x id:%02x "
			"attach:%02x cable_type:%02x rprd:%01x\n", __func__,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->src,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->dest,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->id,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->attach,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->cable_type,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->rprd);

		if (pd != NULL) {
			if (!((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->attach &&
				((struct pdic_notifier_struct *)pd)->event != PDIC_NOTIFY_EVENT_PDIC_ATTACH) {
				((struct pdic_notifier_struct *)pd)->event = PDIC_NOTIFY_EVENT_DETACH;
			}
			pdic_notifier.pdic_template.pd = pd;

			pr_info("%s: PD event:%d, num:%d, sel:%d \n", __func__,
				((struct pdic_notifier_struct *)pd)->event,
				((struct pdic_notifier_struct *)pd)->sink_status.available_pdo_num,
				((struct pdic_notifier_struct *)pd)->sink_status.selected_pdo_num);
		}
		break;
#endif
	case PDIC_NOTIFY_ID_ATTACH:
		pr_info("%s: src:%01x dest:%01x id:%02x "
			"attach:%02x cable_type:%02x rprd:%01x\n", __func__,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->src,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->dest,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->id,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->attach,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->cable_type,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->rprd);
		break;
	case PDIC_NOTIFY_ID_RID:
		pr_info("%s: src:%01x dest:%01x id:%02x rid:%02x\n", __func__,
			((PD_NOTI_RID_TYPEDEF *)p_noti)->src,
			((PD_NOTI_RID_TYPEDEF *)p_noti)->dest,
			((PD_NOTI_RID_TYPEDEF *)p_noti)->id,
			((PD_NOTI_RID_TYPEDEF *)p_noti)->rid);
#if defined(CONFIG_SEC_FACTORY)
			pdic_uevent_work(PDIC_NOTIFY_ID_RID, ((PD_NOTI_RID_TYPEDEF *)p_noti)->rid);
#endif
		break;
#ifdef CONFIG_SEC_FACTORY
	case PDIC_NOTIFY_ID_FAC:
		pr_info("%s: src:%01x dest:%01x id:%02x ErrState:%02x\n", __func__,
			p_noti->src, p_noti->dest, p_noti->id, p_noti->sub1);
			pdic_uevent_work(PDIC_NOTIFY_ID_FAC, p_noti->sub1);
			return 0;
#endif
	case PDIC_NOTIFY_ID_WATER:
		pr_info("%s: src:%01x dest:%01x id:%02x attach:%02x\n", __func__,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->src,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->dest,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->id,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->attach);
			pdic_uevent_work(PDIC_NOTIFY_ID_WATER, ((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->attach);
		break;
	case PDIC_NOTIFY_ID_VCONN:
		pdic_uevent_work(PDIC_NOTIFY_ID_VCONN, 0);
		break;
	case PDIC_NOTIFY_ID_ROLE_SWAP:
		pr_info("%s: src:%01x dest:%01x id:%02x sub1:%02x\n", __func__,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->src,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->dest,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->id,
			((PD_NOTI_ATTACH_TYPEDEF *)p_noti)->attach);
		break;
#ifdef CONFIG_SEC_FACTORY
	case PDIC_NOTIFY_ID_PD_PIN_STATUS:
		pr_info("%s: src:%01x dest:%01x id:%02x pinStatus:%02x\n", __func__,
			p_noti->src, p_noti->dest, p_noti->id, p_noti->sub1);
			pdic_uevent_work(PDIC_NOTIFY_ID_PD_PIN_STATUS, p_noti->sub1);
			return 0;
#endif
	default:
		pr_info("%s: src:%01x dest:%01x id:%02x "
			"sub1:%d sub2:%02x sub3:%02x\n", __func__,
			((PD_NOTI_TYPEDEF *)p_noti)->src,
			((PD_NOTI_TYPEDEF *)p_noti)->dest,
			((PD_NOTI_TYPEDEF *)p_noti)->id,
			((PD_NOTI_TYPEDEF *)p_noti)->sub1,
			((PD_NOTI_TYPEDEF *)p_noti)->sub2,
			((PD_NOTI_TYPEDEF *)p_noti)->sub3);
		break;
	}
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	store_usblog_notify(NOTIFY_PDIC_EVENT, (void *)p_noti, NULL);
#endif
	ret = blocking_notifier_call_chain(&(pdic_notifier.notifier_call_chain),
			p_noti->id, &(pdic_notifier.pdic_template));


	switch (ret) {
	case NOTIFY_STOP_MASK:
	case NOTIFY_BAD:
		pr_err("%s: notify error opdur(0x%x)\n", __func__, ret);
		break;
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s: notify done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s: notify status unknown(0x%x)\n", __func__, ret);
		break;
	}

	return ret;

}

int pdic_notifier_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);
	if (pdic_notifier_init_done) {
		pr_err("%s already registered\n", __func__);
		goto out;
	}
	pdic_notifier_init_done = 1;
	pdic_core_init();
	BLOCKING_INIT_NOTIFIER_HEAD(&(pdic_notifier.notifier_call_chain));

out:
	return ret;
}

static void __exit pdic_notifier_exit(void)
{
	pr_info("%s: exit\n", __func__);
}

device_initcall(pdic_notifier_init);
module_exit(pdic_notifier_exit);
