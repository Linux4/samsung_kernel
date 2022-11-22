/*
 *
 * Copyright (C) 2021 Samsung Electronics
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#define pr_fmt(fmt) "[MUIC] vt_muic: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/muic/common/muic.h>
#include <linux/muic/common/muic_notifier.h>
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif /* CONFIG_PDIC_NOTIFIER && CONFIG_USB_TYPEC_MANAGER_NOTIFIER */
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <dt-bindings/battery/sec-battery.h>
#endif /* CONFIG_BATTERY_SAMSUNG */

struct vt_muic_struct {
	int attached_dev;
	int batt_cable;
	int rprd;
	struct power_supply	*ppsy;
	struct work_struct	vt_muic_work;
	struct notifier_block	psy_nb;
	struct notifier_block	manager_nb;
};

static struct vt_muic_struct vt_muic;

static void vt_muic_work_func(struct work_struct *work)
{
	int attached_dev = vt_muic.attached_dev;
	int new_dev = ATTACHED_DEV_NONE_MUIC;

	pr_info("%s batt_cable(%d) rprd(%d)\n", __func__, vt_muic.batt_cable,
			vt_muic.rprd);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	switch (vt_muic.batt_cable) {
	case SEC_BATTERY_CABLE_USB:
		new_dev = ATTACHED_DEV_USB_MUIC;
		break;
	case SEC_BATTERY_CABLE_USB_CDP:
		new_dev = ATTACHED_DEV_CDP_MUIC;
		break;
	case SEC_BATTERY_CABLE_TA:
	case SEC_BATTERY_CABLE_QC20:
		new_dev = ATTACHED_DEV_TA_MUIC;
		break;
	case SEC_BATTERY_CABLE_UNKNOWN:
	case SEC_BATTERY_CABLE_NONE:
	default:
		new_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	}
#endif
	if (vt_muic.rprd)
		new_dev = ATTACHED_DEV_OTG_MUIC;

	if (attached_dev == new_dev) {
		pr_info("%s: Duplicated(%d), ignore\n", __func__, new_dev);
		return;
	}

	if (new_dev != ATTACHED_DEV_NONE_MUIC) {
		if (attached_dev != ATTACHED_DEV_NONE_MUIC)
			vt_muic_notifier_detach_attached_dev(attached_dev);
		
		vt_muic_notifier_attach_attached_dev(new_dev);
	} else {
		if (attached_dev != ATTACHED_DEV_NONE_MUIC)
			vt_muic_notifier_detach_attached_dev(attached_dev);
	}

	vt_muic.attached_dev = new_dev;
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static int vt_muic_psy_nb_func(struct notifier_block *nb, unsigned long val,
		void *v)
{
	struct power_supply *ppsy = (struct power_supply *)v;
	union power_supply_propval pval;
	int ret;

	if (val != PSY_EVENT_PROP_CHANGED) {
		pr_debug("%s: not changed(%ld)\n", __func__, val);
		return NOTIFY_DONE;
	}

	if (strcmp("bc12", ppsy->desc->name)) {
		pr_debug("%s: name is not bc12 (%s)\n", __func__,
				ppsy->desc->name);
		return NOTIFY_DONE;
	}

	ret = power_supply_get_property(ppsy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret != 0) {
		pr_err("failed to get psy prop, ret=%d\n", ret);
		return NOTIFY_DONE;
	}

	pr_info("%s: batt_cable prev=%d new=%d\n", __func__, vt_muic.batt_cable, pval.intval);

	vt_muic.batt_cable = pval.intval;
	schedule_work(&vt_muic.vt_muic_work);

	return NOTIFY_DONE;
}
#endif /* CONFIG_BATTERY_SAMSUNG */

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static void vt_muic_handle_pd_attach(PD_NOTI_ATTACH_TYPEDEF *pnoti)
{
	if (pnoti->rprd) {
		vt_muic.rprd = pnoti->rprd;
		schedule_work(&vt_muic.vt_muic_work);
	}
}

static void vt_muic_handle_pd_detach(PD_NOTI_ATTACH_TYPEDEF *pnoti)
{
	if (vt_muic.rprd) {
		vt_muic.rprd = 0;
		schedule_work(&vt_muic.vt_muic_work);
	}
}

static int vt_muic_manager_nb_func(struct notifier_block *nb,
		unsigned long action, void *data)
{
	PD_NOTI_TYPEDEF *pnoti = (PD_NOTI_TYPEDEF *)data;

	if (pnoti->dest != PDIC_NOTIFY_DEV_MUIC) {
		pr_err("%s: dest is invalid(%d)\n", __func__, pnoti->dest);
		return NOTIFY_DONE;
	}
	pr_info("%s: src:%d dest:%d id:%d sub[%d %d %d]\n", __func__,
		pnoti->src, pnoti->dest, pnoti->id,
		pnoti->sub1, pnoti->sub2, pnoti->sub3);

	switch (pnoti->id) {
	case PDIC_NOTIFY_ID_ATTACH:
		if (pnoti->sub1)
			vt_muic_handle_pd_attach((PD_NOTI_ATTACH_TYPEDEF *)
					pnoti);
		else
			vt_muic_handle_pd_detach((PD_NOTI_ATTACH_TYPEDEF *)
					pnoti);
		break;
	case PDIC_NOTIFY_ID_WATER:
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}
#endif /* CONFIG_PDIC_NOTIFIER && CONFIG_USB_TYPEC_MANAGER_NOTIFIER */

static int vt_muic_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	muic_notifier_detach_attached_dev(ATTACHED_DEV_UNKNOWN_MUIC);
	vt_muic.attached_dev = ATTACHED_DEV_NONE_MUIC;
	vt_muic.batt_cable = 0;
	vt_muic.rprd = 0;

	INIT_WORK(&vt_muic.vt_muic_work, vt_muic_work_func);
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	vt_muic.psy_nb.notifier_call = vt_muic_psy_nb_func;
	vt_muic.psy_nb.priority = 0;
	ret = power_supply_reg_notifier(&vt_muic.psy_nb);
	if (ret < 0)
		pr_err("%s: register power supply notifier fail\n", __func__);
#endif /* CONFIG_BATTERY_SAMSUNG */
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	ret = manager_notifier_register(&vt_muic.manager_nb,
			vt_muic_manager_nb_func, MANAGER_NOTIFY_PDIC_MUIC); 
	if (ret < 0)
		pr_err("%s: register manager notifier fail\n", __func__);
#endif /* CONFIG_PDIC_NOTIFIER && CONFIG_USB_TYPEC_MANAGER_NOTIFIER */

	pr_info("%s: done\n", __func__);

	return ret;
}

static void __exit vt_muic_exit(void)
{
	pr_info("%s: exit\n", __func__);
}

device_initcall(vt_muic_init);
module_exit(vt_muic_exit);

MODULE_AUTHOR("Samsung USB Team");
MODULE_DESCRIPTION("Virtual Muic");
MODULE_LICENSE("GPL");
