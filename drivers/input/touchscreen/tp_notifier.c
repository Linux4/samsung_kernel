/*
 * linux/drivers/input/touchscreen/tp_notifier.c
 *
 * Copyright (C) 2023 Wingtech
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/notifier.h>
#include <linux/export.h>

static BLOCKING_NOTIFIER_HEAD(usb_notifier_list);
static BLOCKING_NOTIFIER_HEAD(earjack_notifier_list);

int usb_register_notifier_chain_for_tp(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&usb_notifier_list, nb);
}
EXPORT_SYMBOL(usb_register_notifier_chain_for_tp);

int usb_unregister_notifier_chain_for_tp(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&usb_notifier_list, nb);
}
EXPORT_SYMBOL(usb_unregister_notifier_chain_for_tp);

int usb_notifier_call_chain_for_tp(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&usb_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(usb_notifier_call_chain_for_tp);

int earjack_register_notifier_chain_for_tp(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&earjack_notifier_list, nb);
}
EXPORT_SYMBOL(earjack_register_notifier_chain_for_tp);

int earjack_unregister_notifier_chain_for_tp(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&earjack_notifier_list, nb);
}
EXPORT_SYMBOL(earjack_unregister_notifier_chain_for_tp);

int earjack_notifier_call_chain_for_tp(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&earjack_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(earjack_notifier_call_chain_for_tp);