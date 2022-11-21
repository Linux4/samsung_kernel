/*
 *  Do flush operation when panic to save those stuff still in cache to mem
 *
 *  Copyright (C) 2012 Marvell International Ltd.
 *  Lei Wen <leiwen@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/regdump_ops.h>
#include <linux/notifier.h>
#include <linux/kexec.h>
#include <linux/kdebug.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/setup.h>
#include <asm/hardware/gic.h>
#include <mach/addr-map.h>

static int has_died;
static void *indicator;
static DEFINE_RAW_SPINLOCK(panic_lock);
static int
panic_flush(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct membank *bank;
	u32 icdispr;
	int i;

	for (i = 0; i < 4; i++) {
		icdispr = readl_relaxed(GIC_DIST_VIRT_BASE + GIC_DIST_PENDING_SET + (i << 2));
		pr_info("Pending interrupt status(%d): 0x%x\n", i, icdispr);
	}

	raw_spin_lock_irq(&panic_lock);
	if (has_died)
		goto out;

	printk(KERN_EMERG "EMMD: ready to flush cache\n");
	memset(indicator, 0 ,PAGE_SIZE);
	*(unsigned long *)indicator = 0x454d4d44;

	crash_update(NULL);

	has_died = 1;
	flush_cache_all();
	for (i = 0; i < meminfo.nr_banks; i ++) {
		bank = &meminfo.bank[i];
		if (bank->size)
			outer_flush_range(bank->start, bank->size);
	}
	printk(KERN_EMERG "EMMD: update mm done\n");
out:
	raw_spin_unlock_irq(&panic_lock);

	return NOTIFY_DONE;
}

static struct notifier_block panic_flush_block = {
	.notifier_call = panic_flush,
};

static ssize_t panic_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t len)
{
	char _buf[80];

	if (strncmp(buf, "PID", 3) == 0) {
		snprintf(_buf, 80, "\nUser Space Panic:%s", buf);
		panic(_buf);
		goto OUT;
	}

	printk(KERN_WARNING "Not valid value!!!\n");
OUT:
	return len;
}

static struct kobj_attribute panic_attr = {
	.attr	= {
		.name = __stringify(panic),
		.mode = 0644,
	},
	.store	= panic_store,
};

static struct attribute * g[] = {
	&panic_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int __init pxa_panic_notifier(void)
{
	struct page *page;
	if (sysfs_create_group(power_kobj, &attr_group))
		return -1;

	page = pfn_to_page(crashk_res.end >> PAGE_SHIFT);
	indicator = page_address(page);

	register_die_notifier(&panic_flush_block);
	atomic_notifier_chain_register(&panic_notifier_list, &panic_flush_block);
	has_died = 0;
	return 0;
}

core_initcall_sync(pxa_panic_notifier);
