// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/extcon-provider.h>
#include <linux/gpio/consumer.h>
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

#include "extcon-mtk-usb.h"
#include "charger_class.h"

#ifdef CONFIG_TCPC_CLASS
#include "tcpm.h"
#endif

#ifdef CONFIG_MTK_USB_TYPEC_U3_MUX
#include "mux_switch.h"
#endif

#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
#include <linux/cable_type_notifier.h>
#endif

static struct mtk_extcon_info *g_extcon;

static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};
static struct device *this_dev;
#define printc(fmt, args...) do { if(this_dev != NULL) dev_err(this_dev,"[wtchg][%s][%d] "fmt, __FUNCTION__, __LINE__, ##args); } while (0)

static void mtk_usb_extcon_update_role(struct work_struct *work)
{
	struct usb_role_info *role = container_of(to_delayed_work(work),
					struct usb_role_info, dwork);
	struct mtk_extcon_info *extcon = role->extcon;
	unsigned int cur_dr, new_dr;

	cur_dr = extcon->c_role;
	new_dr = role->d_role;

	printc("cur_dr(%d) new_dr(%d)\n", cur_dr, new_dr);

	/* none -> device */
	if (cur_dr == DUAL_PROP_DR_NONE &&
			new_dr == DUAL_PROP_DR_DEVICE) {
		extcon_set_state_sync(extcon->edev, EXTCON_USB, true);
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

#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
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
#endif	/* CONFIG_CABLE_TYPE_NOTIFIER */


static int mtk_usb_extcon_set_role(struct mtk_extcon_info *extcon,
						unsigned int role)
{
	struct usb_role_info *role_info;

	pr_info("%s: role %d\n", __func__, role);

#if defined(CONFIG_USB_NOTIFIER) && defined(CONFIG_CABLE_TYPE_NOTIFIER)
	if (g_extcon) {
		mtk_usb_cable_type_notify_set_role(role);
		return 0;
	}
#endif

	/* create and prepare worker */
	role_info = kzalloc(sizeof(*role_info), GFP_KERNEL);
	pr_info("%s: in set role\n", __func__);

	if (!role_info)
		return -ENOMEM;

	INIT_DELAYED_WORK(&role_info->dwork, mtk_usb_extcon_update_role);

	role_info->extcon = extcon;
	role_info->d_role = role;
	/* issue connection work */
	queue_delayed_work(extcon->extcon_wq, &role_info->dwork, 0);

	return 0;
}

#ifndef CONFIG_WT_PROJECT_S96902AA1 //usb if
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
#endif /* CONFIG_WT_PROJECT_S96902AA1 */

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
		printc("failed to get online prop\n");
		return NOTIFY_DONE;
	}

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_AUTHENTIC, &ival);
	if (ret < 0) {
		printc("failed to get authentic prop\n");
		ival.intval = 0;
	}

	ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_TYPE, &tval);
	if (ret < 0) {
		printc("failed to get usb type\n");
		return NOTIFY_DONE;
	}

	printc("online=%d, ignore_usb=%d, type=%d\n",
				pval.intval, ival.intval, tval.intval);

	if (ival.intval)
		return NOTIFY_DONE;

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
	return NOTIFY_DONE;
}

static int mtk_usb_extcon_psy_init(struct mtk_extcon_info *extcon)
{
	int ret = 0;
	struct device *dev = extcon->dev;
	union power_supply_propval pval;
	union power_supply_propval ival;
	union power_supply_propval tval;

	extcon->usb_psy = devm_power_supply_get_by_phandle(dev, "charger");
	if (IS_ERR_OR_NULL(extcon->usb_psy)) {
		printc("fail to get usb_psy\n");
		extcon->usb_psy = NULL;
		return -EINVAL;
	}

	extcon->psy_nb.notifier_call = mtk_usb_extcon_psy_notifier;
	ret = power_supply_reg_notifier(&extcon->psy_nb);
	if (ret) {
		printc("fail to register notifer\n");
		return ret;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		printc("failed to get online prop\n");
		return 0;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_AUTHENTIC, &ival);
	if (ret < 0) {
		printc("failed to get authentic prop\n");
		ival.intval = 0;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_USB_TYPE, &tval);
	if (ret < 0) {
		printc("failed to get usb type\n");
		return 0;
	}

	printc("online=%d, ignore_usb=%d, type=%d\n",
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
#ifdef CONFIG_W2_CHARGER_PRIVATE

	int ret;
#if defined ADAPT_CHARGER_V1 && defined(CONFIG_MTK_CHARGER)
	ret = mtk_usb_extcon_set_vbus_v1(is_on);
#else
	struct regulator *vbus = extcon->vbus;
	struct device *dev = extcon->dev;

	/* vbus is optional */
	if (!vbus || extcon->vbus_on == is_on)
		return 0;

	

	if (is_on) {
		if (extcon->vbus_vol) {
			ret = regulator_set_voltage(vbus,
					extcon->vbus_vol, extcon->vbus_vol);
			if (ret) {
				printc("vbus regulator set voltage failed\n");
				return ret;
			}
		}

		if (extcon->vbus_cur) {
			ret = regulator_set_current_limit(vbus,
					extcon->vbus_cur, extcon->vbus_cur);
			if (ret) {
				printc("vbus regulator set current failed\n");
				return ret;
			}
		}

		ret = regulator_enable(vbus);
		if (ret) {
			printc("vbus regulator enable failed\n");
			return ret;
		}
	} else {
		regulator_disable(vbus);
	}

	printc("vbus turn %s\n", is_on ? "on" : "off");
	if(NULL != extcon->chg_dev)
		charger_dev_enable_otg(extcon->chg_dev,is_on);
	else
		printc("charger_dev_enable_otg set failed\n");
	extcon->vbus_on = is_on;

	ret = 0;
#endif //ADAPT_CHARGER_V1
	return ret;
#else
	printc("vbus turn %s\n", is_on ? "on" : "off");
	if(NULL != extcon->chg_dev)
		charger_dev_enable_otg(extcon->chg_dev,is_on);
	else
		printc("charger_dev_enable_otg set failed\n");
	extcon->vbus_on = is_on;
#endif
	return 0;
}
#if defined (CONFIG_N26_CHARGER_PRIVATE)
static bool usb_need_reconnect(struct mtk_extcon_info *extcon)
{
	int ret = 0;
	struct device *dev = extcon->dev;
	union power_supply_propval pval;
	union power_supply_propval tval;

	extcon->usb_psy = devm_power_supply_get_by_phandle(dev, "charger");
	if (IS_ERR_OR_NULL(extcon->usb_psy)) {
		printc("fail to get usb_psy\n");
		extcon->usb_psy = NULL;
		return -EINVAL;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
		printc("failed to get online prop\n");
		return 0;
	}

	ret = power_supply_get_property(extcon->usb_psy,
				POWER_SUPPLY_PROP_USB_TYPE, &tval);
	if (ret < 0) {
		printc("failed to get usb type\n");
		return 0;
	}

	printc("## online=%d,type=%d\n",
				pval.intval,tval.intval);

	if (pval.intval && (tval.intval == POWER_SUPPLY_USB_TYPE_SDP ||
			tval.intval == POWER_SUPPLY_USB_TYPE_CDP))
		return true;

	return false;
}
#endif
#if defined(CONFIG_CABLE_TYPE_NOTIFIER)
void mtk_usb_notify_vbus_drive(bool enable)
{
	struct mtk_extcon_info *extcon = g_extcon;

	if (!extcon) {
		pr_info("%s: g_extcon is NULL\n", __func__);
		return;
	}

	pr_info("%s: vbus_on %d, enable %d\n", __func__,
			extcon->vbus_on, enable);

	if (extcon->vbus_on) {
		pr_info("%s: vbus is already powered\n", __func__);
		return;
	}

	mtk_usb_extcon_set_vbus(extcon, enable);
}
EXPORT_SYMBOL(mtk_usb_notify_vbus_drive);
#endif

#ifdef CONFIG_TCPC_CLASS
static int mtk_extcon_tcpc_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct mtk_extcon_info *extcon =
			container_of(nb, struct mtk_extcon_info, tcpc_nb);
	bool vbus_on;
	switch (event) {
	case TCP_NOTIFY_SOURCE_VBUS:
		printc("source vbus = %dmv\n",
				 noti->vbus_state.mv);
		vbus_on = (noti->vbus_state.mv) ? true : false;
		mtk_usb_extcon_set_vbus(extcon, vbus_on);
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		printc("old_state=%d, new_state=%d c_rold=%d\n",
				noti->typec_state.old_state,
				noti->typec_state.new_state, extcon->c_role);

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
			extcon->c_role == DUAL_PROP_DR_NONE) &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			printc("Type-C SRC plug in\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_HOST);
		} else if (!(extcon->bypss_typec_sink) &&
			(noti->typec_state.old_state == TYPEC_UNATTACHED &&
			extcon->c_role == DUAL_PROP_DR_NONE) &&
			(noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC)) {
			printc("Type-C SINK plug in\n");
#if	defined (CONFIG_N26_CHARGER_PRIVATE)		
			if(usb_need_reconnect(extcon))
#endif
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC) &&
			noti->typec_state.new_state == TYPEC_UNATTACHED) {
			printc("Type-C plug out\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		printc("%s dr_swap, new role=%d\n",
				__func__, noti->swap_state.new_role);
		if (noti->swap_state.new_role == PD_ROLE_UFP &&
				(extcon->c_role == DUAL_PROP_DR_HOST ||
				extcon->c_role == DUAL_PROP_DR_NONE)) {
			printc("switch role to device\n");
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);
			mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_DEVICE);
		} else if (noti->swap_state.new_role == PD_ROLE_DFP &&
				(extcon->c_role == DUAL_PROP_DR_DEVICE ||
				extcon->c_role == DUAL_PROP_DR_NONE)) {
			printc("switch role to host\n");
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
	printc("id value : %d\n", id);
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

void force_open_usb_adb(void)
{	
	if (!g_extcon) {
		pr_info("g_extcon = NULL\n");
		return;
	}
	pr_info("%s [Loren]force_open_usb_adb\n", __func__);
	 mtk_usb_extcon_set_role(g_extcon, DUAL_PROP_DR_DEVICE);
}
EXPORT_SYMBOL_GPL(force_open_usb_adb);

void force_disable_usb_adb(void)
{
	if (!g_extcon) {
		pr_info("g_extcon = NULL\n");
		return;
	}
	pr_info("%s [Loren]force_disable_usb_adb\n", __func__);
	 mtk_usb_extcon_set_role(g_extcon, DUAL_PROP_DR_NONE);
}
EXPORT_SYMBOL_GPL(force_disable_usb_adb);

void mt_usb_disconnect_v1(void)
{
	pr_info("%s  in mtk extcon\n", __func__);
#ifdef CONFIG_TCPC_CLASS
	/* disconnect by tcpc notifier */
#else
	issue_connection_work(DUAL_PROP_DR_NONE);
#endif
}
EXPORT_SYMBOL_GPL(mt_usb_disconnect_v1);
#endif //ADAPT_PSY_V1

static int mtk_usb_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_extcon_info *extcon;
	struct platform_device *conn_pdev;
	struct device_node *conn_np;
	int ret;

	this_dev = dev;

	extcon = devm_kzalloc(&pdev->dev, sizeof(*extcon), GFP_KERNEL);
	if (!extcon)
		return -ENOMEM;

	extcon->dev = dev;

	/* extcon */
	extcon->edev = devm_extcon_dev_allocate(dev, usb_extcon_cable);
	if (IS_ERR(extcon->edev)) {
		printc("failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, extcon->edev);
	if (ret < 0) {
		printc("failed to register extcon device\n");
		return ret;
	}

	/* usb role switch */
	conn_np = of_parse_phandle(dev->of_node, "dev-conn", 0);
	if (!conn_np) {
		printc("failed to get dev-conn node\n");
		return -EINVAL;
	}

	conn_pdev = of_find_device_by_node(conn_np);
	if (!conn_pdev) {
		printc("failed to get dev-conn pdev\n");
		return -EINVAL;
	}

	extcon->dev_conn.endpoint[0] = kasprintf(GFP_KERNEL,
				"%s-role-switch", dev_name(&conn_pdev->dev));
	extcon->dev_conn.endpoint[1] = dev_name(extcon->dev);
	extcon->dev_conn.id = "usb-role-switch";
	device_connection_add(&extcon->dev_conn);

	extcon->role_sw = usb_role_switch_get(extcon->dev);
	if (IS_ERR(extcon->role_sw)) {
		printc("failed to get usb role\n");
		return PTR_ERR(extcon->role_sw);
	}

	/* vbus */
	extcon->vbus = devm_regulator_get(dev, "vbus");
	if (IS_ERR(extcon->vbus)) {
		printc("failed to get vbus\n");
		return PTR_ERR(extcon->vbus);
	}

	if (!of_property_read_u32(dev->of_node, "vbus-voltage",
					&extcon->vbus_vol))
		printc("vbus-voltage=%d", extcon->vbus_vol);

	if (!of_property_read_u32(dev->of_node, "vbus-current",
					&extcon->vbus_cur))
		printc("vbus-current=%d", extcon->vbus_cur);

	extcon->bypss_typec_sink =
		of_property_read_bool(dev->of_node,
			"mediatek,bypss-typec-sink");
#if defined (CONFIG_N26_CHARGER_PRIVATE)
	extcon->bypss_typec_sink = false;
#endif

	extcon->extcon_wq = create_singlethread_workqueue("extcon_usb");
	if (!extcon->extcon_wq)
		return -ENOMEM;

	extcon->c_role = DUAL_PROP_DR_DEVICE;

	/* default initial role */
	mtk_usb_extcon_set_role(extcon, DUAL_PROP_DR_NONE);

	/* default turn off vbus */
	extcon->chg_dev = get_charger_by_name("primary_chg");
	if (extcon->chg_dev)
		printc("Found primary charger\n");
	else {
		printc("*** Error : can't find primary charger ***\n");
	}
	mtk_usb_extcon_set_vbus(extcon, false);

	/*get id resources*/
	ret = mtk_usb_extcon_id_pin_init(extcon);
	if (ret < 0)
		printc("failed to init id pin\n");

	/* power psy */
	ret = mtk_usb_extcon_psy_init(extcon);
	if (ret < 0)
		printc("failed to init psy\n");

#ifdef CONFIG_TCPC_CLASS
	/* tcpc */
	ret = mtk_usb_extcon_tcpc_init(extcon);
	if (ret < 0)
		printc("failed to init tcpc\n");
#endif

	g_extcon = extcon;

	platform_set_drvdata(pdev, extcon);

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
		printc("set host vbus off when shutdown\n");
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

