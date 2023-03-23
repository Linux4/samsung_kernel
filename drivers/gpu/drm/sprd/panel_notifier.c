#include <linux/notifier.h>

static BLOCKING_NOTIFIER_HEAD(notifier_head);

int panel_notifier_chain_register(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&notifier_head, n);
}

int panel_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&notifier_head, val, v);
}

int panel_notifier_chain_unregister(struct notifier_block *n)
{
	return blocking_notifier_chain_unregister(&notifier_head, n);
}