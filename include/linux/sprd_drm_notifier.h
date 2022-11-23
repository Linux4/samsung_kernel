#include <linux/notifier.h>

enum {
	DISPC_POWER_ON = 0,
	DISPC_POWER_OFF,
};

int disp_notifier_register(struct notifier_block *nb);
int disp_notifier_unregister(struct notifier_block *nb);
int disp_notifier_call_chain(unsigned long val, void *v);
