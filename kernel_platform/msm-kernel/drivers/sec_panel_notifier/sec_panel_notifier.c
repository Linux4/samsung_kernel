//#include <linux/kernel.h>
#include <linux/module.h>

static BLOCKING_NOTIFIER_HEAD(panel_notifier_list);

/**
 *	ss_panel_notifier_register - register a client notifier
 *	@nb: notifier block to callback on events
 */
int ss_panel_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&panel_notifier_list, nb);
}
EXPORT_SYMBOL(ss_panel_notifier_register);

/**
 *	ss_panel_notifier_unregister - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int ss_panel_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&panel_notifier_list, nb);
}
EXPORT_SYMBOL(ss_panel_notifier_unregister);

/**
 * panel_notifier_call_chain - notify clients
 *
 */
int ss_panel_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&panel_notifier_list, val, v);
}
EXPORT_SYMBOL(ss_panel_notifier_call_chain);

MODULE_DESCRIPTION("Samsung panel notifier");
MODULE_LICENSE("GPL");

