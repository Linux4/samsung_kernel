/*
 * linux/drivers/gpu/drm/panel/panel_notifier.c
 *
 * Copyright (C) 2021 Wingtech
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/export.h>

static BLOCKING_NOTIFIER_HEAD(panel_notifier_list);

/**
 *	panel_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int panel_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&panel_notifier_list, nb);
}
EXPORT_SYMBOL(panel_register_client);

/**
 *	panel_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int panel_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&panel_notifier_list, nb);
}
EXPORT_SYMBOL(panel_unregister_client);

/**
 * panel_notifier_call_chain - notify clients of panel_events
 *
 */
int panel_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&panel_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(panel_notifier_call_chain);

