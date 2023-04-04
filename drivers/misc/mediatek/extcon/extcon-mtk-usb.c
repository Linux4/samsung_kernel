// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/extcon-provider.h>
/*hs04 code for SR-AL6398A-01-82 by shixuanxuan at 20220705 start*/
#if defined(CONFIG_HQ_PROJECT_HS03S)
// no include
#elif defined(CONFIG_HQ_PROJECT_HS04)
// no include
#else
#include <linux/gpio/consumer.h>
#endif
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/usb/role.h>
#include <linux/workqueue.h>
/*hs14 code for SR-AL6528A-01-257 by chengyuanhang at 2022/09/28 start*/
#if defined(CONFIG_HQ_PROJECT_O22)
#include <linux/delay.h>
/* hs14 code for P221231-00979 by gaozhengwei at 2023/01/02 start */
#include <linux/chg-tcpc_info.h>
extern enum tcpc_cc_supplier tcpc_info;
/* hs14 code for P221231-00979 by gaozhengwei at 2023/01/02 end */
#endif
/*hs14 code for SR-AL6528A-01-257 by chengyuanhang at 2022/09/28 end*/
#include "extcon-mtk-usb.h"

#ifdef CONFIG_TCPC_CLASS
#include "tcpm.h"
#endif

#if defined(CONFIG_HQ_PROJECT_HS03S)
// no include
#elif defined(CONFIG_HQ_PROJECT_HS04)
// no include
#else
#ifdef CONFIG_MTK_USB_TYPEC_U3_MUX
#include "mux_switch.h"
#endif
#endif
/*hs04 code for SR-AL6398A-01-82 by shixuanxuan at 20220705 end*/

#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
#include <linux/cable_type_notifier.h>
#elif IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_VIRTUAL_MUIC)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif

static struct mtk_extcon_info *g_extcon;
/*hs14 code for SR-AL6528A-01-257 by chengyuanhang at 2022/09/28 start*/
#if defined(CONFIG_HQ_PROJECT_O22)
/*hs14 code for AL6528ADEU-2606 by gaozhengwei at 2022/11/22 start*/
bool usb_data_enabled = true;
EXPORT_SYMBOL(usb_data_enabled);
/*hs14 code for AL6528ADEU-2606 by gaozhengwei at 2022/11/22 end*/
static int p_role;
#endif
/*hs14 code for SR-AL6528A-01-257 by chengyuanhang at 2022/09/28 end*/
static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static void mtk_usb_extcon_update_role(struct work_struct *work)
{
	struct usb_role_info *role = container_of(to_delayed_work(work),
					struct usb_role_info, dwork);
	struct mtk_extcon_info *extcon = role->extcon;
	unsigned int cur_dr, new_dr;

	cur_dr = extcon->c_role;
	new_dr = role->d_role;
/*hs14 code for SR-AL6528A-01-257 by chengyuanhang at 2022/09/28 start*/
#if defined(CONFIG_HQ_PROJECT_O22)
	p_role = extcon->c_role;

	dev_info(extcon->dev, "cur_dr(%d) new_dr(%d) p_role (%d)\n", cur_dr, new_dr, p_role);
	if(usb_data_enabled == false && new_dr !=  DUAL_PROP_DR_NONE){
		return;
	/* none -> device */
	} else if (cur_dr == DUAL_PROP_DR_NONE &&
			new_dr == DUAL_PROP_DR_DEVICE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, true);
#else
	dev_info(extcon->dev, "cur_dr(%d) new_dr(%d)\n", cur_dr, new_dr);
	/* none -> device */
	if (cur_dr == DUAL_PROP_DR_NONE &&
                        new_dr == DUAL_PROP_DR_DEVICE) {
                extcon_set_state_sync(extcon->edev, EXTCON_USB, true);
#endif
/*hs14 code for SR-AL6528A-01-257 by chengyuanhang at 2022/09/28 end*/
	/* none -> host */
	} else if (cur_dr == DUAL_PROP_DR_NONE &&
			new_dr == DUAL_PROP_DR_HOST) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, true);
	/* device -> none */
	} else if (cur_dr == DUAL_PROP_DR_DEVICE &&
			new_dr == DUAL_PROP_DR_NONE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
	/* host -> none */
	} else if (cur_dr == DUAL_PROP_DR_HOST &&
			new_dr == DUAL_PROP_DR_NONE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, false);
	/* device -> host */
	} else if (cur_dr == DUAL_PROP_DR_DEVICE &&
			new_dr == DUAL_PROP_DR_HOST) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, false);
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, true);
	/* host -> device */
	} else if (cur_dr == DUAL_PROP_DR_HOST &&
			new_dr == DUAL_PROP_DR_DEVICE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB_HOST, false);
		extcon_set_state_sync(extcon->edev, EXTCON_USB, true);
	}

	/* usb role switch */
	if (extcon->role_sw) {
		if (new_dr == DUAL_PROP_DR_DEVICE)
			usb_role_switch_set_role(extcon->role_sw,
						USB_ROLE_DEVICE);
		else if (new_dr == DUAL_PROP_DR_HOST)
			usb_role_switch_set_role(extcon->role_sw,
						USB_ROLE_HOST);
		else
			usb_role_switch_set_role(extcon->role_sw,
						USB_ROLE_NONE);
	}

	extcon->c_role = new_dr;
	kfree(role);
}

#if defined(CONFIG_USB_NOTIFIER)
int mtk_usb_notify_set_mode(int role)
{
	struct mtk_extcon_info *extcon = g_extcon;
	struct usb_role_info *role_info;

	if (!extcon) {
		pr_info("%s: g_extcon is NULL\n", __func__);
		return -ENODEV;
	}

	pr_info("%s: role %d\n", __func__, role);
	/* create and prepare worker */
	role_info = kzalloc(sizeof(*role_info), GFP_KERNEL);
	if (!role_info)
		return -ENOMEM;

	INIT_DELAYED_WORK(&role_info->dwork, mtk_usb_extcon_update_role);

	role_info->extcon = extcon;
	role_info->d_role = role;
	/* issue connection work */
	queue_delayed_work(extcon->extcon_wq, &role_info->dwork, 0);

	return 0;
}
EXPORT_SYMBOL(mtk_usb_notify_set_mode);
#endif

#if IS_ENABLED(CONFIG_CABLE_TYPE_NOTIFIER)
static int mtk_usb_cable_type_notify_set_role(unsigned int role)
{
	cable_type_attached_dev_t cable;

	pr_info("%s: role %d\n", __func__, role);

	switch (role) {
	case DUAL_PROP_DR_HOST:
		cable = CABLE_TYPE_OTG;
		break;
	case DUAL_PROP_DR_DEVICE:
		cable = CABLE_TYPE_USB_SDP;
		break;
	case DUAL_PROP_DR_NONE:
	default:
		cable = CABLE_TYPE_NONE;
		break;
	}

	cable_type_notifier_set_attached_dev(cable);
	return 0;
}
#elif IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_VIRTUAL_MUIC)
void mt_usb_pdic_event_work(unsigned int role)
{
	PD_NOTI_TYPEDEF pdic_noti = {
		.src = PDIC_NOTIFY_DEV_PDIC,
		.dest = PDIC_NOTIFY_DEV_USB,
		.id = PDIC_NOTIFY_ID_USB,
		.sub1 = 0,
		.sub2 = 0,//event,
		.sub3 = 0,
	};

	switch (role) {
	case DUAL_PROP_DR_HOST:
		pdic_noti.sub2 = USB_STATUS_NOTIFY_ATTACH_DFP;
		break;
	case DUAL_PROP_DR_DEVICE:
		pdic_noti.sub2 = USB_STATUS_NOTIFY_ATTACH_UFP;
		break;
	case DUAL_PROP_DR_NONE:
	default:
		pdic_noti.sub2 = USB_STATUS_NOTIFY_DETACH;
		break;
	}

	pr_info("usb: %s :%s\n", __func__, pdic_usbstatus_string(pdic_noti.sub2));
	pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pdic_noti, 0, 0);
}
#endif

static int mtk_usb_extcon_set_role(struct mtk_extcon_info *extcon,
						unsigned int role)
{
	struct usb_role_info *role_info;

#if defined(CONFIG_USB_NOTIFIER)
	extcon->last_dr_event = role;
	if (g_extcon) {
#if IS_ENABLED(CONFIG_CABLE_TYPE_NOTIFIER)
		mtk_usb_cable_type_notify_set_role(role);
		return 0;
#elif IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_VIRTUAL_MUIC)
		mt_usb_pdic_event_work(role);
		return 0;
#endif
	}
#endif

	/* create and prepare worker */
	role_info = kzalloc(sizeof(*role_info), GFP_KERNEL);
	if (!role_info)
		return -ENOMEM;

	INIT_DELAYED_WORK(&role_info->dwork, mtk_usb_extcon_update_role);

	role_info->extcon = extcon;
	role_info->d_role = role;
	/* issue connection work */
	queue_delayed_work(extcon->extcon_wq, &role_info->dwork, 0);

	return 0;
}

#if !defined(CONFIG_USB_MTK_HDRC)
void mt_usb_connect()
{
	/* if (g_extcon)
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE); */
}
EXPORT_SYMBOL(mt_usb_connect);

void mt_usb_disconnect()
{
	/* if (g_extcon)
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE); */
}
EXPORT_SYMBOL(mt_usb_disconnect);
#endif

static int mtk_usb_extcon_psy_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct mtk_extcon_info *extcon = container_of(nb,
					struct mtk_extcon_info, psy_nb);
	union power_supply_propval pval;
	union power_supply_propval ival;
	union power_supply_propval tval;
	int ret;

	if (event != PSY_EVENT_PROP_CHANGED || psy != extcon->usb_psy)
		return NOTIFY_DONE;

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get online prop\n");
		return NOTIFY_DONE;
	}

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_AUTHENTIC, &ival);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get authentic prop\n");
		ival.intval = 0;
	}

/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 start*/
#if defined(CONFIG_HQ_PROJECT_HS03S)
	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_USB_TYPE, &tval);
#elif defined(CONFIG_HQ_PROJECT_HS04)
	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_USB_TYPE, &tval);
#else
	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_TYPE, &tval);
#endif
/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 end*/
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get usb type\n");
		return NOTIFY_DONE;
	}

	dev_info(extcon->dev, "online=%d, ignore_usb=%d, type=%d\n",
				pval.intval, ival.intval, tval.intval);

	if (ival.intval)
		return NOTIFY_DONE;

/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 start*/
#if defined(CONFIG_HQ_PROJECT_HS03S)
	if (pval.intval && (tval.intval == POWER_SUPPLY_USB_TYPE_SDP ||
			tval.intval == POWER_SUPPLY_USB_TYPE_CDP))
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
	else
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
#elif defined(CONFIG_HQ_PROJECT_HS04)
	if (pval.intval && (tval.intval == POWER_SUPPLY_USB_TYPE_SDP ||
			tval.intval == POWER_SUPPLY_USB_TYPE_CDP))
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
	else
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
#else
#ifdef CONFIG_TCPC_CLASS
	if (extcon->c_role == DUAL_PROP_DR_NONE && pval.intval &&
			(tval.intval == POWER_SUPPLY_TYPE_USB ||
			tval.intval == POWER_SUPPLY_TYPE_USB_CDP))
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
#else
	if (pval.intval && (tval.intval == POWER_SUPPLY_TYPE_USB ||
			tval.intval == POWER_SUPPLY_TYPE_USB_CDP))
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
	else
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
#endif
#endif
/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 end*/
	return NOTIFY_DONE;
}

static int mtk_usb_extcon_psy_init(struct mtk_extcon_info *extcon)
{
	int ret = 0;
	struct device *dev = extcon->dev;
	union power_supply_propval pval;
	union power_supply_propval ival;
	union power_supply_propval tval;

	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/08 start */
#if defined(CONFIG_HQ_PROJECT_O22)
	extcon->usb_psy = power_supply_get_by_name("charger");
#else
	extcon->usb_psy = devm_power_supply_get_by_phandle(dev, "charger");
#endif
	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/08 end */
	if (IS_ERR_OR_NULL(extcon->usb_psy)) {
		dev_err(dev, "fail to get usb_psy\n");
		extcon->usb_psy = NULL;
		return -EINVAL;
	}

	extcon->psy_nb.notifier_call = mtk_usb_extcon_psy_notifier;
	ret = power_supply_reg_notifier(&extcon->psy_nb);
	if (ret) {
		dev_err(dev, "fail to register notifer\n");
		return ret;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get online prop\n");
		return 0;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_AUTHENTIC, &ival);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get authentic prop\n");
		ival.intval = 0;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_USB_TYPE, &tval);
	if (ret < 0) {
		dev_info(extcon->dev, "failed to get usb type\n");
		return 0;
	}

	dev_info(extcon->dev, "online=%d, ignore_usb=%d, type=%d\n",
				pval.intval, ival.intval, tval.intval);

	if (ival.intval)
		return 0;

	if (pval.intval && (tval.intval == POWER_SUPPLY_USB_TYPE_SDP ||
			tval.intval == POWER_SUPPLY_USB_TYPE_CDP))
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);

	return 0;
}

#if defined ADAPT_CHARGER_V1 && defined(CONFIG_MTK_CHARGER)
#include <mt-plat/v1/charger_class.h>
static struct charger_device *primary_charger;

static int mtk_usb_extcon_set_vbus_v1(bool is_on) {
	if (!primary_charger) {
		primary_charger = get_charger_by_name("primary_chg");
		if (!primary_charger) {
			pr_info("%s: get primary charger device failed\n", __func__);
			return -ENODEV;
		}
	}
#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
	pr_info("%s: is_on=%d\n", __func__, is_on);
	if (is_on) {
		charger_dev_enable_otg(primary_charger, true);
		charger_dev_set_boost_current_limit(primary_charger,
			1500000);
		#if 0
		{// # workaround
			charger_dev_kick_wdt(primary_charger);
			enable_boost_polling(true);
		}
		#endif
	} else {
		charger_dev_enable_otg(primary_charger, false);
		#if 0
			//# workaround
			enable_boost_polling(false);
		#endif
	}
#else
	if (is_on) {
		charger_dev_enable_otg(primary_charger, true);
		charger_dev_set_boost_current_limit(primary_charger,
			1500000);
	} else {
		charger_dev_enable_otg(primary_charger, false);
	}
#endif
		return 0;
}
#endif //ADAPT_CHARGER_V1

static int mtk_usb_extcon_set_vbus(struct mtk_extcon_info *extcon,
							bool is_on)
{
	int ret;
#if defined ADAPT_CHARGER_V1 && defined(CONFIG_MTK_CHARGER)
	ret = mtk_usb_extcon_set_vbus_v1(is_on);
#else
	struct regulator *vbus = extcon->vbus;
	struct device *dev = extcon->dev;

	/* vbus is optional */
	if (!vbus || extcon->vbus_on == is_on)
		return 0;

	dev_info(dev, "vbus turn %s\n", is_on ? "on" : "off");

	if (is_on) {
		if (extcon->vbus_vol) {
			ret = regulator_set_voltage(vbus,
					extcon->vbus_vol, extcon->vbus_vol);
			if (ret) {
				dev_err(dev, "vbus regulator set voltage failed\n");
				return ret;
			}
		}

		if (extcon->vbus_cur) {
			ret = regulator_set_current_limit(vbus,
					extcon->vbus_cur, extcon->vbus_cur);
			if (ret) {
				dev_err(dev, "vbus regulator set current failed\n");
				return ret;
			}
		}

		ret = regulator_enable(vbus);
		if (ret) {
			dev_err(dev, "vbus regulator enable failed\n");
			return ret;
		}
	} else {
		regulator_disable(vbus);
	}

	extcon->vbus_on = is_on;

	ret = 0;
#endif //ADAPT_CHARGER_V1
	return ret;
}

#ifdef CONFIG_TCPC_CLASS
static int mtk_extcon_tcpc_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mtk_extcon_info *extcon =
			container_of(nb, struct mtk_extcon_info, tcpc_nb);
	struct device *dev = extcon->dev;
	bool vbus_on;

	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		dev_info(dev, "source vbus = %dmv\n",
				 noti->vbus_state.mv);
		vbus_on = (noti->vbus_state.mv) ? true : false;
		mtk_usb_extcon_set_vbus(extcon, vbus_on);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		dev_info(dev, "old_state=%d, new_state=%d c_rold=%d\n",
				noti->typec_state.old_state,
				noti->typec_state.new_state, extcon->c_role);
/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 start*/
#if defined(CONFIG_HQ_PROJECT_HS03S)
		if ((noti->typec_state.old_state == TYPEC_UNATTACHED &&
			extcon->c_role == DUAL_PROP_DR_NONE) &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			dev_info(dev, "Type-C SRC plug in\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_HOST);
		} else if (!(extcon->bypss_typec_sink) &&
			(noti->typec_state.old_state == TYPEC_UNATTACHED &&
			extcon->c_role == DUAL_PROP_DR_NONE) &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			dev_info(dev, "Type-C SINK plug in\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			dev_info(dev, "Type-C plug out\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
		}
#elif defined(CONFIG_HQ_PROJECT_HS04)
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			dev_info(dev, "Type-C SRC plug in\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_HOST);
		} else if (!(extcon->bypss_typec_sink) &&
			noti->typec_state.old_state == TYPEC_UNATTACHED &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			dev_info(dev, "Type-C SINK plug in\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			dev_info(dev, "Type-C plug out\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
		}
/*hs04 code for SR-AL6398A-01-82 by wenyaqi at 20220705 end*/
#else
#ifdef CONFIG_MTK_USB_TYPEC_U3_MUX
		if ((noti->typec_state.new_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC)) {
			if (noti->typec_state.polarity == 0)
				usb3_switch_set(TYPEC_ORIENTATION_REVERSE);
			else
				usb3_switch_set(TYPEC_ORIENTATION_NORMAL);
		} else if (noti->typec_state.new_state == TYPEC_UNATTACHED) {
			usb3_switch_set(TYPEC_ORIENTATION_NONE);
		}
#endif
		if ((noti->typec_state.old_state == TYPEC_UNATTACHED &&
#if defined(CONFIG_USB_NOTIFIER)
			extcon->last_dr_event == DUAL_PROP_DR_NONE) &&
#else
			extcon->c_role == DUAL_PROP_DR_NONE) &&
#endif
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			dev_info(dev, "Type-C SRC plug in\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_HOST);
		} else if (!(extcon->bypss_typec_sink) &&
			(noti->typec_state.old_state == TYPEC_UNATTACHED &&
#if defined(CONFIG_USB_NOTIFIER)
			extcon->last_dr_event == DUAL_PROP_DR_NONE) &&
#else
			extcon->c_role == DUAL_PROP_DR_NONE) &&
#endif
			(noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC)) {
			dev_info(dev, "Type-C SINK plug in\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			dev_info(dev, "Type-C plug out\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
		}
#endif
		break;
	case TCP_NOTIFY_DR_SWAP:
		dev_info(dev, "%s dr_swap, new role=%d\n",
				__func__, noti->swap_state.new_role);
		if (noti->swap_state.new_role == PD_ROLE_UFP &&
#if defined(CONFIG_USB_NOTIFIER)
				(extcon->last_dr_event == DUAL_PROP_DR_HOST ||
				extcon->last_dr_event == DUAL_PROP_DR_NONE)
#else
				(extcon->c_role == DUAL_PROP_DR_HOST ||
				extcon->c_role == DUAL_PROP_DR_NONE)
#endif
		) {
			dev_info(dev, "switch role to device\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
		} else if (noti->swap_state.new_role == PD_ROLE_DFP &&
#if defined(CONFIG_USB_NOTIFIER)
				(extcon->last_dr_event == DUAL_PROP_DR_DEVICE ||
				 extcon->last_dr_event == DUAL_PROP_DR_NONE)
#else
				(extcon->c_role == DUAL_PROP_DR_DEVICE ||
				extcon->c_role == DUAL_PROP_DR_NONE)
#endif
		) {
			dev_info(dev, "switch role to host\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_HOST);
		}
		break;
	}

	return NOTIFY_OK;
}

static int mtk_usb_extcon_tcpc_init(struct mtk_extcon_info *extcon)
{
	struct tcpc_device *tcpc_dev;
	int ret;

	tcpc_dev = tcpc_dev_get_by_name("type_c_port0");

	if (!tcpc_dev) {
		dev_err(extcon->dev, "get tcpc device fail\n");
		return -ENODEV;
	}

	extcon->tcpc_nb.notifier_call = mtk_extcon_tcpc_notifier;
	ret = register_tcp_dev_notifier(tcpc_dev, &extcon->tcpc_nb,
		TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_VBUS |
		TCP_NOTIFY_TYPE_MISC);
	if (ret < 0) {
		dev_err(extcon->dev, "register notifer fail\n");
		return -EINVAL;
	}

	extcon->tcpc_dev = tcpc_dev;

	return 0;
}
#endif

static void mtk_usb_extcon_detect_cable(struct work_struct *work)
{
	struct mtk_extcon_info *extcon = container_of(to_delayed_work(work),
						    struct mtk_extcon_info,
						    wq_detcable);
	int id;

	/* check ID and update cable state */
	id = extcon->id_gpiod ?
		gpiod_get_value_cansleep(extcon->id_gpiod) : 1;

	/* at first we clean states which are no longer active */
	if (id) {
		mtk_usb_extcon_set_vbus(extcon, false);
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
	} else {
		mtk_usb_extcon_set_vbus(extcon, true);
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_HOST);
	}
}

static irqreturn_t mtk_usb_idpin_handle(int irq, void *dev_id)
{
	struct mtk_extcon_info *extcon = dev_id;

	/* issue detection work */
	queue_delayed_work(system_power_efficient_wq, &extcon->wq_detcable, 0);

	return IRQ_HANDLED;
}

static int mtk_usb_extcon_id_pin_init(struct mtk_extcon_info *extcon)
{
	int ret = 0;
	int id;

	extcon->id_gpiod = devm_gpiod_get(extcon->dev, "id", GPIOD_IN);

	if (!extcon->id_gpiod || IS_ERR(extcon->id_gpiod)) {
		dev_err(extcon->dev, "failed to get id gpio\n");
		extcon->id_gpiod = NULL;
		return -EINVAL;
	}

	extcon->id_irq = gpiod_to_irq(extcon->id_gpiod);
	if (extcon->id_irq < 0) {
		dev_err(extcon->dev, "failed to get ID IRQ\n");
		extcon->id_gpiod = NULL;
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&extcon->wq_detcable, mtk_usb_extcon_detect_cable);

	ret = devm_request_threaded_irq(extcon->dev, extcon->id_irq, NULL,
			mtk_usb_idpin_handle, IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			dev_name(extcon->dev), extcon);

	if (ret < 0) {
		dev_err(extcon->dev, "failed to request handler for ID IRQ\n");
		extcon->id_gpiod = NULL;
		return ret;
	}
	enable_irq_wake(extcon->id_irq);

	// get id pin value when boot on
	id = extcon->id_gpiod ?	gpiod_get_value_cansleep(extcon->id_gpiod) : 1;
	dev_info(extcon->dev, "id value : %d\n", id);
	if (!id) {
		mtk_usb_extcon_set_vbus(extcon, true);
		mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_HOST);
	}

	return 0;
}

#if defined ADAPT_PSY_V1
static void issue_connection_work(unsigned int dr)
{
	if (!g_extcon) {
		pr_info("g_extcon = NULL\n");
		return;
	}

	/* issue connection work */
	mtk_usb_extcon_set_role(g_extcon, dr);
}

void mt_usb_connect_v1(void)
{
	pr_info("%s in mtk extcon\n", __func__);

#ifdef CONFIG_TCPC_CLASS
	/* check current role to avoid power role swap issue */
	if (g_extcon && g_extcon->c_role == DUAL_PROP_DR_NONE)
		issue_connection_work(DUAL_PROP_DR_DEVICE);
#else
	issue_connection_work(DUAL_PROP_DR_DEVICE);
#endif
}
EXPORT_SYMBOL_GPL(mt_usb_connect_v1);

void mt_usb_disconnect_v1(void)
{
	pr_info("%s  in mtk extcon\n", __func__);
#ifdef CONFIG_TCPC_CLASS
	/* disconnect by tcpc notifier */
/* hs14 code for AL6528ADEU-3679|P221231-00979 by gaozhengwei at 2023/01/02 start */
#if defined(CONFIG_HQ_PROJECT_O22)
	if (tcpc_info == FUSB302) {
		pr_info("%s: tcpc_info = %d\n", __func__, tcpc_info);
		issue_connection_work(DUAL_PROP_DR_NONE);
	}
#endif
/* hs14 code for AL6528ADEU-3679|P221231-00979 by gaozhengwei at 2023/01/02 end */
#else
	issue_connection_work(DUAL_PROP_DR_NONE);
#endif
}
EXPORT_SYMBOL_GPL(mt_usb_disconnect_v1);
#endif //ADAPT_PSY_V1

/*hs14 code for SR-AL6528A-01-257 by chengyuanhang at 2022/09/28 start*/
#if defined(CONFIG_HQ_PROJECT_O22)
extern void set_extcon_otg_vbus(struct mtk_extcon_info *extcon, long val);
void set_extcon_otg_vbus(struct mtk_extcon_info *extcon, long val)
{
	if(val)
		mtk_usb_extcon_set_vbus(extcon, true);
	else
		mtk_usb_extcon_set_vbus(extcon, false);
	return;
}

/*hs14 code for AL6528ADEU-2606 by gaozhengwei at 2022/11/22 start*/
void usb_notify_control(bool data_enabled)
{
	static bool prev_mode = true;

	if (!g_extcon) {
		pr_info("g_extcon = NULL\n");
		return;
	}

	usb_data_enabled = data_enabled;
	if (prev_mode != usb_data_enabled) {
		prev_mode = usb_data_enabled;

		pr_err("input usb_data_enabled : %d\n", data_enabled);

		if (usb_data_enabled == false) {
			mtk_usb_extcon_set_role(g_extcon, DUAL_PROP_DR_NONE);//close USB
        		set_extcon_otg_vbus(g_extcon, usb_data_enabled);
        		msleep(50);
		} else {
        		mtk_usb_extcon_set_role(g_extcon, p_role);//open USB
			set_extcon_otg_vbus(g_extcon, usb_data_enabled);
		}
		return;
	} else {
		pr_info("usb_data_enabled : %d same mode exit!\n", data_enabled);
		return;
	}
}
EXPORT_SYMBOL(usb_notify_control);
/*hs14 code for AL6528ADEU-2606 by gaozhengwei at 2022/11/22 end*/
#endif
/*hs14 code for SR-AL6528A-01-257 by chengyuanhang at 2022/09/28 end*/

static int mtk_usb_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_extcon_info *extcon;
	struct platform_device *conn_pdev;
	struct device_node *conn_np;
	int ret;

	extcon = devm_kzalloc(&pdev->dev, sizeof(*extcon), GFP_KERNEL);
	if (!extcon)
		return -ENOMEM;

	extcon->dev = dev;

	/* extcon */
	extcon->edev = devm_extcon_dev_allocate(dev, usb_extcon_cable);
	if (IS_ERR(extcon->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, extcon->edev);
	if (ret < 0) {
		dev_info(dev, "failed to register extcon device\n");
		return ret;
	}

	/* usb role switch */
	conn_np = of_parse_phandle(dev->of_node, "dev-conn", 0);
	if (!conn_np) {
		dev_info(dev, "failed to get dev-conn node\n");
		return -EINVAL;
	}

	conn_pdev = of_find_device_by_node(conn_np);
	if (!conn_pdev) {
		dev_info(dev, "failed to get dev-conn pdev\n");
		return -EINVAL;
	}

	extcon->dev_conn.endpoint[0] = kasprintf(GFP_KERNEL,
				"%s-role-switch", dev_name(&conn_pdev->dev));
	extcon->dev_conn.endpoint[1] = dev_name(extcon->dev);
	extcon->dev_conn.id = "usb-role-switch";
	device_connection_add(&extcon->dev_conn);

	extcon->role_sw = usb_role_switch_get(extcon->dev);
	if (IS_ERR(extcon->role_sw)) {
		dev_err(dev, "failed to get usb role\n");
		return PTR_ERR(extcon->role_sw);
	}

	/* vbus */
	extcon->vbus = devm_regulator_get(dev, "vbus");
	if (IS_ERR(extcon->vbus)) {
		dev_err(dev, "failed to get vbus\n");
		return PTR_ERR(extcon->vbus);
	}

	if (!of_property_read_u32(dev->of_node, "vbus-voltage",
					&extcon->vbus_vol))
		dev_info(dev, "vbus-voltage=%d", extcon->vbus_vol);

	if (!of_property_read_u32(dev->of_node, "vbus-current",
					&extcon->vbus_cur))
		dev_info(dev, "vbus-current=%d", extcon->vbus_cur);

	extcon->bypss_typec_sink =
		of_property_read_bool(dev->of_node,
			"mediatek,bypss-typec-sink");

	extcon->extcon_wq = create_singlethread_workqueue("extcon_usb");
	if (!extcon->extcon_wq)
		return -ENOMEM;

	extcon->c_role = DUAL_PROP_DR_DEVICE;

	/* default initial role */
	mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
	
	g_extcon = extcon;
	platform_set_drvdata(pdev, extcon);

	/* default turn off vbus */
	mtk_usb_extcon_set_vbus(extcon, false);

	/*get id resources*/
	ret = mtk_usb_extcon_id_pin_init(extcon);
	if (ret < 0)
		dev_info(dev, "failed to init id pin\n");

	/* power psy */
	ret = mtk_usb_extcon_psy_init(extcon);
	if (ret < 0)
		dev_err(dev, "failed to init psy\n");

#ifdef CONFIG_TCPC_CLASS
	/* tcpc */
	ret = mtk_usb_extcon_tcpc_init(extcon);
	if (ret < 0)
		dev_err(dev, "failed to init tcpc\n");
#endif

	return 0;
}

static int mtk_usb_extcon_remove(struct platform_device *pdev)
{
	struct mtk_extcon_info *extcon = platform_get_drvdata(pdev);

	if (extcon->dev_conn.id)
		device_connection_remove(&extcon->dev_conn);

	return 0;
}

static void mtk_usb_extcon_shutdown(struct platform_device *pdev)
{
	struct mtk_extcon_info *extcon = platform_get_drvdata(pdev);

	if (extcon->c_role == DUAL_PROP_DR_HOST) {
		dev_info(extcon->dev, "set host vbus off when shutdown\n");
		mtk_usb_extcon_set_vbus(extcon, false);
	}
}

static const struct of_device_id mtk_usb_extcon_of_match[] = {
	{ .compatible = "mediatek,extcon-usb", },
	{ },
};
MODULE_DEVICE_TABLE(of, mtk_usb_extcon_of_match);

static struct platform_driver mtk_usb_extcon_driver = {
	.probe		= mtk_usb_extcon_probe,
	.remove		= mtk_usb_extcon_remove,
	.shutdown	= mtk_usb_extcon_shutdown,
	.driver		= {
		.name	= "mtk-extcon-usb",
		.of_match_table = mtk_usb_extcon_of_match,
	},
};

static int __init mtk_usb_extcon_init(void)
{
	return platform_driver_register(&mtk_usb_extcon_driver);
}
late_initcall(mtk_usb_extcon_init);

static void __exit mtk_usb_extcon_exit(void)
{
	platform_driver_unregister(&mtk_usb_extcon_driver);
}
module_exit(mtk_usb_extcon_exit);

