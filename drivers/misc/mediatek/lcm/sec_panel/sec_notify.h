#include <linux/notifier.h>

int sec_panel_register_notifier(struct notifier_block *nb);
int sec_panel_unregister_notifier(struct notifier_block *nb);
int sec_panel_notifier_call_chain(unsigned long val, void *v);

