/*
 *
 * Copyright (c) Samsung Electronics Co, Ltd.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifdef CONFIG_EXTCON_SM5504
#include <linux/extcon/sm5504-muic.h>
#endif

#include <linux/battery/sec_battery.h>
#include <linux/extcon/extcon-samsung.h>

int first_acce;
EXPORT_SYMBOL_GPL(first_acce);

static int __current_cable = CABLE_NONE;
static char MUIC_NAME[20] = {0, };

struct extcon_cable samsung_cables[] =
{
	DEFINE_EXTCON_CABLE(CABLE_NONE),
	DEFINE_EXTCON_CABLE(CABLE_TA),
	DEFINE_EXTCON_CABLE(CABLE_USB),
	DEFINE_EXTCON_CABLE(CABLE_CDP),
	DEFINE_EXTCON_CABLE(CABLE_DCP),
	DEFINE_EXTCON_CABLE(CABLE_UART),
	DEFINE_EXTCON_CABLE(CABLE_OTG),
	DEFINE_EXTCON_CABLE(CABLE_MHL),
	DEFINE_EXTCON_CABLE(CABLE_DESKDOCK),
	DEFINE_EXTCON_CABLE(CABLE_DESKDOCK_VB),
	DEFINE_EXTCON_CABLE(CABLE_AUDIODOCK),
	DEFINE_EXTCON_CABLE(CABLE_SMARTDOCK_TA),
	DEFINE_EXTCON_CABLE(CABLE_SMARTDOCK_USB),
	DEFINE_EXTCON_CABLE(CABLE_JIG_USB_ON),
	DEFINE_EXTCON_CABLE(CABLE_JIG_USB_OFF),
	DEFINE_EXTCON_CABLE(CABLE_JIG_UART_ON),
	DEFINE_EXTCON_CABLE(CABLE_JIG_UART_ON_VB),
	DEFINE_EXTCON_CABLE(CABLE_JIG_UART_OFF),
	DEFINE_EXTCON_CABLE(CABLE_JIG_UART_OFF_VB),
	DEFINE_EXTCON_CABLE(CABLE_POWER_SHARING_MUIC),
	DEFINE_EXTCON_CABLE(CABLE_VBUSTPOWER_ON_TEST),
	DEFINE_EXTCON_CABLE(CABLE_VBUSTPOWER_OFF_TEST),
	DEFINE_EXTCON_CABLE(CABLE_UNKNOWN),
};

#ifdef CONFIG_EXTCON_SM5504
static inline int sm5504_get_cable_type(const int cable_type)
{
	u8 current_cable_type;

	switch (cable_type) {
	case SM5504_CABLE_NONE_MUIC:
	case SM5504_CABLE_DESKTOP_DOCK_MUIC:
		current_cable_type = CABLE_NONE;
		break;
	case SM5504_CABLE_SDP_MUIC:
	case SM5504_CABLE_CARKIT_T1_MUIC:
		current_cable_type = CABLE_USB;
		break;
	case SM5504_CABLE_TYPE_NOTIFY_VBUSPOWER_ON:
		current_cable_type = CABLE_VBUSTPOWER_ON_TEST;
		break;
	case SM5504_CABLE_TYPE_NOTIFY_VBUSPOWER_OFF:
		current_cable_type = CABLE_VBUSTPOWER_OFF_TEST;
		break;
	case SM5504_CABLE_DESKTOP_DOCK_VB_MUIC:
	case SM5504_CABLE_DCP_MUIC:
		current_cable_type = CABLE_TA;
		break;
	case SM5504_CABLE_CDP_MUIC:
		current_cable_type = CABLE_CDP;
		break;
	case SM5504_CABLE_OTG_MUIC:
		current_cable_type = CABLE_OTG;
		break;
	case SM5504_CABLE_JIG_UART_OFF_VB_MUIC:
		current_cable_type = CABLE_JIG_UART_OFF_VB;
		break;
	case SM5504_CABLE_JIG_UART_ON_VB_MUIC:
		current_cable_type = CABLE_JIG_UART_ON_VB;
		break;
	case SM5504_CABLE_JIG_UART_OFF_MUIC:
		current_cable_type = CABLE_JIG_UART_OFF;
		break;
	case SM5504_CABLE_JIG_UART_ON_MUIC:
		current_cable_type = CABLE_JIG_UART_ON;
		break;
	case SM5504_CABLE_JIG_USB_ON_MUIC:
		current_cable_type = CABLE_JIG_USB_ON;
		break;
	case SM5504_CABLE_JIG_USB_OFF_MUIC:
		current_cable_type = CABLE_JIG_USB_OFF;
		break;
	case SM5504_CABLE_PPD_MUIC:
		current_cable_type = CABLE_POWER_SHARING_MUIC;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, cable_type);
		current_cable_type = CABLE_UNKNOWN;
	}

	return current_cable_type;
}
#endif

BLOCKING_NOTIFIER_HEAD(usb_switch_notifier);
BLOCKING_NOTIFIER_HEAD(vbus_notifier);

int usb_switch_register_notify(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&usb_switch_notifier, nb);
}
EXPORT_SYMBOL_GPL(usb_switch_register_notify);

int usb_switch_unregister_notify(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&usb_switch_notifier, nb);
}
EXPORT_SYMBOL_GPL(usb_switch_unregister_notify);

void muic_attached_accessory_inquire(void)
{
	pr_info("%s: %d\n", __func__, first_acce);
	blocking_notifier_call_chain(&usb_switch_notifier, first_acce, NULL);
}
EXPORT_SYMBOL_GPL(muic_attached_accessory_inquire);

bool get_jig_state(void)
{
	bool is_jig;

	switch (__current_cable) {
	case CABLE_JIG_USB_ON ... CABLE_JIG_UART_OFF_VB:
		is_jig = true;
		break;
	default:
		is_jig = false;
	}
	pr_info("%s: is_jig: %d\n", __func__, is_jig);

	return is_jig;
}
EXPORT_SYMBOL_GPL(get_jig_state);

void extcon_samsung_call_chain(const int cable_type)
{
	u8 current_cable_type;
	char* muic_name;

	current_cable_type = sm5504_get_cable_type(cable_type);
	muic_name = "sm5504";
	strncpy(MUIC_NAME, muic_name, sizeof(MUIC_NAME));
	sec_charger_cb(current_cable_type);
	blocking_notifier_call_chain(&usb_switch_notifier,
					     current_cable_type, NULL);
	__current_cable = current_cable_type;
	pr_info("%s: %s: attached %s(%d)\n", MUIC_NAME, __func__,
				samsung_cables[__current_cable].name, __current_cable);
}
EXPORT_SYMBOL_GPL(extcon_samsung_call_chain);

int vbus_register_notify(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&vbus_notifier, nb);
}
EXPORT_SYMBOL_GPL(vbus_register_notify);

int vbus_unregister_notify(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&vbus_notifier, nb);
}
EXPORT_SYMBOL_GPL(vbus_unregister_notify);

void extcon_samsung_vbus_call_chain(const bool vbus_in)
{
	blocking_notifier_call_chain(&vbus_notifier, vbus_in, NULL);
	pr_info("%s: %s: vbus %s", MUIC_NAME, __func__,
					vbus_in ? "in" : "out");
}
EXPORT_SYMBOL_GPL(extcon_samsung_vbus_call_chain);

ssize_t attached_dev_attrs(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	pr_info("%s: %s: attached_dev:%s\n", MUIC_NAME, __func__,
				samsung_cables[__current_cable].name);
	return sprintf(buf, samsung_cables[__current_cable].name);
}
