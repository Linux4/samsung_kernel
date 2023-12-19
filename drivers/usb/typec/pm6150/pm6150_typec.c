/*
 * Copyrights (C) 2019 Samsung Electronics, Inc.
 * Copyrights (C) 2019 Silicon Mitus, Inc.
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

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/usb/typec/pm6150/pm6150_typec.h>
#include <linux/usb/typec/pdic_sysfs.h>
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif
#include <linux/usb/typec/pm6150/samsung_usbpd.h>

#if defined(CONFIG_CCIC_NOTIFIER)
static enum ccic_sysfs_property pm6150_sysfs_properties[] = {
	CCIC_SYSFS_PROP_CHIP_NAME,
	CCIC_SYSFS_PROP_LPM_MODE,
	CCIC_SYSFS_PROP_STATE,
	CCIC_SYSFS_PROP_RID,
	CCIC_SYSFS_PROP_CTRL_OPTION,
	CCIC_SYSFS_PROP_FW_WATER,
	CCIC_SYSFS_PROP_ACC_DEVICE_VERSION,
	CCIC_SYSFS_PROP_USBPD_IDS,
	CCIC_SYSFS_PROP_USBPD_TYPE,
	CCIC_SYSFS_PROP_CABLE,
};
#endif

#define PM6150_CABLE_NUM		8

typedef enum {
	PM6150_CABLE_NONE = 0,
	PM6150_CABLE_USB,
	PM6150_CABLE_TA,
	PM6150_CABLE_CDP,
	PM6150_CABLE_PD,
	PM6150_CABLE_QC,
	PM6150_CABLE_OTG,
	PM6150_CABLE_AFC,
} pm6150_cable_t;

char PM6150_CABLE_Print[PM6150_CABLE_NUM][13] = {
	{"NONE"},
	{"USB"},
	{"TA"},
	{"CDP"},
	{"PD Charger"},
	{"QC Charger"},
	{"OTG"},
	{"AFC Charger"},
};

static struct pm6150_phydrv_data *pdic_data;

#if defined(CONFIG_CCIC_NOTIFIER)
static void pm6150_ccic_event_notifier(struct work_struct *data)
{
	struct ccic_state_work *event_work =
		container_of(data, struct ccic_state_work, ccic_work);
	CC_NOTI_TYPEDEF ccic_noti;

	switch (event_work->dest) {
	case CCIC_NOTIFY_DEV_USB:
		pr_info("%s, dest=%s, id=%s, attach=%s, drp=%s, event_work=%p\n",
			__func__,
			CCIC_NOTI_DEST_Print[event_work->dest],
			CCIC_NOTI_ID_Print[event_work->id],
			event_work->attach ? "Attached" : "Detached",
			CCIC_NOTI_USB_STATUS_Print[event_work->event],
			event_work);
		break;
	default:
		pr_info("%s, dest=%s, id=%s, attach=%d, event=%d, event_work=%p\n",
			__func__,
			CCIC_NOTI_DEST_Print[event_work->dest],
			CCIC_NOTI_ID_Print[event_work->id],
			event_work->attach,
			event_work->event,
			event_work);
		break;
	}

	ccic_noti.src = CCIC_NOTIFY_DEV_CCIC;
	ccic_noti.dest = event_work->dest;
	ccic_noti.id = event_work->id;
	ccic_noti.sub1 = event_work->attach;
	ccic_noti.sub2 = event_work->event;
	ccic_noti.sub3 = event_work->sub;

	ccic_notifier_notify((CC_NOTI_TYPEDEF *)&ccic_noti, NULL, 0);

	kfree(event_work);
}

void pm6150_ccic_event_work(int dest,
		int id, int attach, int event, int sub)
{
	struct ccic_state_work *event_work;

	pr_info("%s : usb: DIAES %d-%d-%d-%d-%d\n",
		__func__, dest, id, attach, event, sub);
	event_work = kmalloc(sizeof(struct ccic_state_work), GFP_ATOMIC);
	INIT_WORK(&event_work->ccic_work, pm6150_ccic_event_notifier);

	event_work->dest = dest;
	event_work->id = id;
	event_work->attach = attach;
	event_work->event = event;
	event_work->sub = sub;
	if (!queue_work(pdic_data->ccic_wq, &event_work->ccic_work)) {
		pr_info("usb: %s, event_work(%p) is dropped\n",
			__func__, event_work);
		kfree(event_work);
	}
	
	switch(id) {
		case CCIC_NOTIFY_ID_RID:
			pdic_data->rid = attach;
		break;
		case CCIC_NOTIFY_ID_WATER:
			pdic_data->is_water_detect = attach;		
		break;
	}
}
EXPORT_SYMBOL(pm6150_ccic_event_work);

void pm6150_set_pd_state(int state)
{
	static int cur_pd_state = 0;

	pr_debug("%s : cur_pd_state: %d, state: %d\n", __func__,
		cur_pd_state, state);
	if (state < 0) {
		if (!pdic_data) {
			pr_err("%s: pdic_data is null\n", __func__);
			return;
		}
	} else {
		cur_pd_state = state;
		if (!pdic_data) {
			pr_err("%s: pdic_data is null\n", __func__);
			return;
		}
	}
	pdic_data->pd_state = cur_pd_state;
	store_usblog_notify(NOTIFY_FUNCSTATE, (void *)&pdic_data->pd_state, NULL);
}
EXPORT_SYMBOL(pm6150_set_pd_state);

int pm6150_match_cable(int cable)
{
	int pm6150_cable = 0;

	switch(cable) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
		pm6150_cable = PM6150_CABLE_NONE;
		break;
	case POWER_SUPPLY_TYPE_USB:
		pm6150_cable = PM6150_CABLE_USB;
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		pm6150_cable = PM6150_CABLE_TA;
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		pm6150_cable = PM6150_CABLE_CDP;
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		pm6150_cable = PM6150_CABLE_PD;
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		pm6150_cable = PM6150_CABLE_QC;
		break;
	case POWER_SUPPLY_TYPE_OTG:
		pm6150_cable = PM6150_CABLE_OTG;
		break;
#if defined(CONFIG_AFC)
	case POWER_SUPPLY_TYPE_AFC:
		pm6150_cable = PM6150_CABLE_AFC;
		break;
#endif
	default:
		pm6150_cable = PM6150_CABLE_NONE;
		break;
	}
	pr_info("%s : %d to %d\n", __func__, cable, pm6150_cable);

	return pm6150_cable;
}

void pm6150_set_cable(int cable)
{
	static int cur_cable = 0;

	if (cable < 0) {
		if (!pdic_data) {
			pr_err("%s: pdic_data is null\n", __func__);
			return;
		}
	} else {
		if (cur_cable == cable) {
			return;
		}
		cur_cable = cable;
		if (!pdic_data) {
			pr_err("%s: pdic_data is null\n", __func__);
			return;
		}
	}
	pr_info("%s : previous cable: %d, current cable: %d\n", __func__, 
		pdic_data->cable, cur_cable);
	pdic_data->cable = cur_cable;
#if defined(CONFIG_SEC_FACTORY)
	pm6150_ccic_event_work(CCIC_NOTIFY_DEV_MANAGER, CCIC_NOTIFY_ID_CABLE,
		0, 0, 0);
#endif
}
EXPORT_SYMBOL(pm6150_set_cable);


static int pm6150_sysfs_get_prop(struct _ccic_data_t *pccic_data,
					enum ccic_sysfs_property prop,
					char *buf)
{
	int retval = -ENODEV;
	int val;

	switch (prop) {
	case CCIC_SYSFS_PROP_STATE:
		retval = sprintf(buf, "%d\n", pdic_data->pd_state);
		pr_info("%s : CCIC_SYSFS_PROP_STATE : %d", __func__, pdic_data->pd_state);
		break;
	case CCIC_SYSFS_PROP_RID:
		retval = sprintf(buf, "%d\n", pdic_data->rid);
		pr_info("%s : CCIC_SYSFS_PROP_RID : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_FW_WATER:
		retval = sprintf(buf, "%d\n", pdic_data->is_water_detect);
		pr_info("%s : CCIC_SYSFS_PROP_FW_WATER : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_ACC_DEVICE_VERSION:
		retval = sprintf(buf, "%04x\n", pdic_data->Device_Version);
		pr_info("%s : CCIC_SYSFS_PROP_ACC_DEVICE_VERSION : %s",
			__func__, buf);
		break;
	case CCIC_SYSFS_PROP_USBPD_IDS:
		retval = sprintf(buf, "%04x:%04x\n",
			le16_to_cpu(pdic_data->Vendor_ID),
			le16_to_cpu(pdic_data->Product_ID));
		pr_info("%s : CCIC_SYSFS_PROP_USBPD_IDS : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_USBPD_TYPE:
		retval = sprintf(buf, "%d\n", pdic_data->acc_type);
		pr_info("%s : CCIC_SYSFS_PROP_USBPD_TYPE : %s", __func__, buf);
		break;
	case CCIC_SYSFS_PROP_CABLE:
		val = pm6150_match_cable(pdic_data->cable);
		retval = sprintf(buf, "%s\n", PM6150_CABLE_Print[val]);
		pr_info("%s : CCIC_SYSFS_PROP_CABLE : %s", __func__, PM6150_CABLE_Print[val]);
		break;
	default:
		pr_info("%s : prop read not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		break;
	}
	return retval;
}

static ssize_t pm6150_sysfs_set_prop(struct _ccic_data_t *pccic_data,
				enum ccic_sysfs_property prop,
				const char *buf, size_t size)
{
	ssize_t retval = size;

	switch (prop) {
	default:
		pr_info("%s : prop write not supported prop (%d)\n",
				__func__, prop);
		retval = -ENODATA;
		return retval;
	}
	return size;
}

static int pm6150_sysfs_is_writeable(struct _ccic_data_t *pccic_data,
				enum ccic_sysfs_property prop)
{
	switch (prop) {
	default:
		return 0;
	}
}
#endif

int pm6150_usbpd_create(struct device* dev, struct pm6150_phydrv_data** parent_phy_drv_data)
{
	static bool init_done = 0;
	int ret = 0;
#if defined(CONFIG_CCIC_NOTIFIER)
	pccic_data_t pccic_data;
	pccic_sysfs_property_t pccic_sysfs_prop;
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	if (init_done) {
		pr_info("%s already done.\n", __func__);
		pdic_data->dev = dev;
		*parent_phy_drv_data = pdic_data;
		return 0;
	}
	pr_info("%s start\n", __func__);
	pdic_data = kzalloc(sizeof(struct pm6150_phydrv_data), GFP_KERNEL);
	if (!pdic_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}
	pdic_data->pd_state = pm6150_State_PE_Initial_detach;
	pdic_data->cable = PM6150_CABLE_NONE;
	pdic_data->rid = RID_OPEN;

	pdic_data->is_samsung_accessory_enter_mode = 0;
	pdic_data->pn_flag = false;
	pdic_data->acc_type = 0;
	pdic_data->Vendor_ID = 0;
	pdic_data->Product_ID = 0;
	pdic_data->Device_Version = 0;	
	pdic_data->samsung_usbpd = NULL;
	pdic_data->dr_swap_cnt = 0;

#if defined(CONFIG_CCIC_NOTIFIER)
	ccic_register_switch_device(1);
	/* Create a work queue for the ccic irq thread */
	pdic_data->ccic_wq
		= create_singlethread_workqueue("usbpd_event");
	if (!pdic_data->ccic_wq) {
		pr_err("%s failed to create work queue for ccic notifier\n",
			__func__);
		goto err_return;
	}
	
	pccic_data = kzalloc(sizeof(ccic_data_t), GFP_KERNEL);
	pccic_sysfs_prop = kzalloc(sizeof(ccic_sysfs_property_t), GFP_KERNEL);
	pccic_sysfs_prop->get_property = pm6150_sysfs_get_prop;
	pccic_sysfs_prop->set_property = pm6150_sysfs_set_prop;
	pccic_sysfs_prop->property_is_writeable = pm6150_sysfs_is_writeable;
	pccic_sysfs_prop->properties = pm6150_sysfs_properties;
	pccic_sysfs_prop->num_properties = ARRAY_SIZE(pm6150_sysfs_properties);
	pccic_data->ccic_syfs_prop = pccic_sysfs_prop;
	pccic_data->drv_data = pdic_data;
	pccic_data->name = "pm6150";
	ccic_core_register_chip(pccic_data);
	ccic_misc_init(pccic_data);
	pccic_data->misc_dev->uvdm_ready = samsung_usbpd_uvdm_ready;
	pccic_data->misc_dev->uvdm_close = samsung_usbpd_uvdm_close;
	pccic_data->misc_dev->uvdm_read = samsung_usbpd_uvdm_in_request_message;
	pccic_data->misc_dev->uvdm_write = samsung_usbpd_uvdm_out_request_message;
#endif
	pm6150_set_pd_state(-1);
	pm6150_set_cable(-1);
#if defined(CONFIG_USB_HOST_NOTIFY)
	if (o_notify)
		send_otg_notify(o_notify, NOTIFY_EVENT_POWER_SOURCE, 0);
#endif
	pdic_data->dev = dev;
	*parent_phy_drv_data = pdic_data;

	pr_info("%s : pdic_data=%x, parent_phy_drv_data=%x , *parent_phy_drv_data=%x\n",
		__func__, pdic_data, parent_phy_drv_data, *parent_phy_drv_data);
	init_done = 1;
	init_completion(&pdic_data->uvdm_out_wait);
	init_completion(&pdic_data->uvdm_in_wait);
	pr_info("%s : pm6150 usbpd driver uploaded!\n", __func__);
	return 0;

err_return:
	return ret;
}
EXPORT_SYMBOL(pm6150_usbpd_create);

int pm6150_usbpd_destroy(void)
{
#if defined(CONFIG_CCIC_NOTIFIER)
	ccic_register_switch_device(0);
	ccic_misc_exit();
#endif
	kfree(pdic_data);
	return 0;
}
EXPORT_SYMBOL(pm6150_usbpd_destroy);


MODULE_DESCRIPTION("PM6150 USB TYPE-C driver");
MODULE_AUTHOR("Youngmin Park <ym.bak@samsung.com>");
MODULE_LICENSE("GPL");
