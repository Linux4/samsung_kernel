#include <linux/sprd_drm_notifier.h>

static BLOCKING_NOTIFIER_HEAD(disp_notifier_list);
int disp_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&disp_notifier_list, nb);
}
EXPORT_SYMBOL(disp_notifier_register);

int disp_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&disp_notifier_list, nb);
}
EXPORT_SYMBOL(disp_notifier_unregister);

int disp_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&disp_notifier_list, val, v);
}
EXPORT_SYMBOL(disp_notifier_call_chain);
