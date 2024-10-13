// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 */

#define pr_fmt(fmt) "[BSP] " fmt

#include <linux/proc_fs.h>
#include "sec_debug_bspinfo.h"

static BLOCKING_NOTIFIER_HEAD(bspi_displayer_chain_head);

int secdbg_bspinfo_register_info_displayer(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&bspi_displayer_chain_head, nb);
}
EXPORT_SYMBOL_GPL(secdbg_bspinfo_register_info_displayer);

static int secdbg_bspinfo_show(struct seq_file *m, void *v)
{
	int ret;

	ret = blocking_notifier_call_chain(&bspi_displayer_chain_head, 0, m);

	return 0;
}

static int __init secdbg_bspi_init(void)
{
	struct proc_dir_entry *entry;

	pr_info("%s: init\n", __func__);

	entry = proc_create_single("bspinfo", 0440, NULL, secdbg_bspinfo_show);
	if (!entry)
		return -ENOMEM;

	return 0;
}
late_initcall_sync(secdbg_bspi_init);

static void __exit secdbg_bspi_exit(void)
{

}
module_exit(secdbg_bspi_exit);

MODULE_DESCRIPTION("Samsung Debug BSP Info driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("devname:secdbg-bspinfo");


