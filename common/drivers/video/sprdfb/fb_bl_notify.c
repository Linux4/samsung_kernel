#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/export.h>

static BLOCKING_NOTIFIER_HEAD(fb_bl_notifier_list);

/**
 *	fb_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int fb_bl_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&fb_bl_notifier_list, nb);
}
EXPORT_SYMBOL(fb_bl_register_client);

/**
 *	fb_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int fb_bl_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&fb_bl_notifier_list, nb);
}
EXPORT_SYMBOL(fb_bl_unregister_client);

/**
 * fb_notifier_call_chain - notify clients of fb_events
 *
 */
int fb_bl_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&fb_bl_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(fb_bl_notifier_call_chain);


