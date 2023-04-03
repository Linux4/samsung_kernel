// SPDX-License-Identifier: GPL-2.0
/*
 * sec_mm/
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/oom.h>

static int sec_mm_panic_handler(struct notifier_block *nb, unsigned long action,
				void *str_buf)
{
	/* not to print duplicate information */
	if (strstr(str_buf, "System is deadlocked on memory"))
		return NOTIFY_DONE;

	show_mem(0, NULL);
	dump_tasks(NULL, NULL);

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call = sec_mm_panic_handler,
};

static int __init init_panic_handler(void)
{
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);
	return 0;
}

static void __exit exit_panic_handler(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_block);
}

module_init(init_panic_handler);
module_exit(exit_panic_handler);

MODULE_LICENSE("GPL");
