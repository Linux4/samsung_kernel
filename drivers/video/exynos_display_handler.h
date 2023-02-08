#include <linux/notifier.h>

enum SUSPEND_LEVEL {
	SUSPEND_LEVEL_LCDFREQ,
	SUSPEND_LEVEL_DSIM
};

int register_exynos_display_notifier(struct notifier_block *nb);
int unregister_exynos_display_notifier(struct notifier_block *nb);
int exynos_display_notifier_call_chain(unsigned long val, void *v);

int register_display_handler(int (*func)(void *data, unsigned long event), int level, void *data);

