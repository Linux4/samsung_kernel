/*
 * muic_ccic.c
 *
 * Copyright (C) 2014 Samsung Electronics
 * Thomas Ryu <smilesr.ryu@samsung.com>
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>
#include <linux/string.h>
#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#include <linux/muic/muic.h>
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif

#include <linux/muic/muic_interface.h>

#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
#include <linux/usb_notify.h>
#endif

#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
extern void muic_send_dock_intent(int type);
//extern void muic_set_otg_state(struct muic_interface_t *muic_if, int state);

static int muic_handle_usb_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	struct muic_interface_t *muic_if =
	    container_of(nb, struct muic_interface_t, usb_nb);
#else
	struct muic_interface_t *muic_if =
	    container_of(nb, struct muic_interface_t, nb);
#endif
	int devicetype = *(int *)data;

	switch (action) {
	/* receive powerrole and change the BC 1.2 operation */
	case EXTERNAL_NOTIFY_POWERROLE:
		pr_info("%s: POWER ROLE(%d)\n", __func__, devicetype);
		/* devicetype 1 >> BC1.2 off */
		if (devicetype)
			muic_if->powerrole_state = 1;
		else
			muic_if->powerrole_state = 0;

		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

void muic_register_usb_notifier(struct muic_interface_t *muic_if)
{
	int ret = 0;

	pr_info("%s: Registering EXTERNAL_NOTIFY_DEV_MUIC.\n", __func__);


	ret = usb_external_notify_register(&muic_if->usb_nb,
		muic_handle_usb_notification, EXTERNAL_NOTIFY_DEV_MUIC);
	if (ret < 0) {
		pr_info("%s: USB Noti. is not ready.\n", __func__);
		return;
	}

	pr_info("%s: done.\n", __func__);
}

void muic_unregister_usb_notifier(struct muic_interface_t *muic_if)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = usb_external_notify_unregister(&muic_if->usb_nb);
	if (ret < 0) {
		pr_info("%s: USB Noti. unregister error.\n", __func__);
		return;
	}

	pr_info("%s: done.\n", __func__);
}
#else
void muic_register_usb_notifier(struct muic_interface_t *muic_if){}
void muic_unregister_usb_notifier(struct muic_interface_t *muic_if){}
#endif
