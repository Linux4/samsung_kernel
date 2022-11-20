/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/swap.h>

#ifdef CONFIG_E_SHOW_MEM

static BLOCKING_NOTIFIER_HEAD(e_show_mem_notify_list);

int register_e_show_mem_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&e_show_mem_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_e_show_mem_notifier);

int unregister_e_show_mem_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&e_show_mem_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_e_show_mem_notifier);


void enhanced_show_mem(enum e_show_mem_type type)
{
	/* Module used pages */
	unsigned long used = 0;
	struct sysinfo i;

	printk("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	printk("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	printk("Enhanced Mem-Info:");
	if (E_SHOW_MEM_BASIC == type)
		printk("E_SHOW_MEM_BASIC\n");
	else if (E_SHOW_MEM_CLASSIC == type)
		printk("E_SHOW_MEM_CLASSIC\n");
	else
		printk("E_SHOW_MEM_ALL\n");
	printk("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	printk("Enhanced Mem-info :SHOW MEM\n");
	show_mem(SHOW_MEM_FILTER_NODES | SHOW_MEM_FILTER_PAGE_COUNT);
	si_meminfo(&i);
	printk("MemTotal:       %8lu kB\n"
		"Buffers:        %8lu kB\n"
		"SwapCached:     %8lu kB\n",
		(i.totalram) << (PAGE_SHIFT - 10),
		(i.bufferram) << (PAGE_SHIFT - 10),
		total_swapcache_pages() << (PAGE_SHIFT - 10));

	blocking_notifier_call_chain(&e_show_mem_notify_list,
				(unsigned long)type, &used);
	printk("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	printk("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
}
#endif
