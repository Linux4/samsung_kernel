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

#include <linux/reboot.h>
#include <linux/kexec.h>
#include <linux/kdebug.h>
#include <linux/kobject.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <asm/setup.h>

#include <mach/regs-coresight.h>
#include <mach/cputype.h>
#include <linux/regdump_ops.h>

static void *indicator;
static DEFINE_RAW_SPINLOCK(panic_lock);
extern void arm_machine_flush_console(void);
extern void (*arm_pm_restart)(char str, const char *cmd);
#define PANIC_TIMER_STEP 100

extern int sec_crash_key_panic;
extern int sec_debug_panic_dump(char *);
extern int i2c_set_pio_mode(void);

void panic_flush(struct pt_regs *regs)
{
	struct pt_regs fixed_regs;
	int i;

	hardlockup_enable = 0;
	raw_spin_lock(&panic_lock);

	for (i = 0; i < nr_cpu_ids; i++)
		coresight_dump_pcsr(i);

	if (cpu_is_pxa1088())
		dump_reg_to_console();

	printk(KERN_EMERG "EMMD: ready to perform memory dump\n");

#ifndef CONFIG_SEC_DEBUG
	memset(indicator, 0 ,PAGE_SIZE);
	*(unsigned long *)indicator = 0x454d4d44;
#endif

	crash_setup_regs(&fixed_regs, regs);
	crash_save_vmcoreinfo();
	machine_crash_shutdown(&fixed_regs);

#ifdef CONFIG_SEC_DEBUG
	if (sec_crash_key_panic) {
		sec_debug_panic_dump("Crash Key");
	} else {
		sec_debug_panic_dump("");
	}
#endif

	printk(KERN_EMERG "EMMD: done\n");
	arm_machine_flush_console();

	flush_cache_all();
	outer_flush_all();

	if (panic_timeout > 0) {
		/*
		 * Delay timeout seconds before rebooting the machine.
		 * We can't use the "normal" timers since we just panicked.
		 */
		printk(KERN_EMERG "Rebooting in %d seconds..", panic_timeout);

		for (i = 0; i < panic_timeout * 1000; i += PANIC_TIMER_STEP)
			mdelay(PANIC_TIMER_STEP);
	}

	printk(KERN_EMERG "EMMD: update mm done\n");
	if (panic_timeout != 0) {
		i2c_set_pio_mode();
		/*
		 * This will not be a clean reboot, with everything
		 * shutting down.  But if there is a chance of
		 * rebooting the system it will be rebooted.
		 */
		arm_pm_restart(0, NULL);
	}

	raw_spin_unlock(&panic_lock);
	while (1)
		cpu_relax();
}

static int dump_reg_handler(struct notifier_block *self,
			     unsigned long val,
			     void *data)
{
	struct die_args *args = data;

	crash_update(args->regs);
	return 0;
}

static struct notifier_block die_dump_reg_notifier = {
	.notifier_call = dump_reg_handler,
	.priority = 200
};

#if (defined CONFIG_PM)
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

#endif /* CONFIG_PM */

static int __init pxa_panic_notifier(void)
{
	struct page *page;
#if (defined CONFIG_PM)
	if (sysfs_create_group(power_kobj, &attr_group))
		return -1;
#endif

#ifndef CONFIG_SEC_DEBUG
	page = pfn_to_page(crashk_res.end >> PAGE_SHIFT);
	indicator = page_address(page);
#endif

	panic_on_oops = 1;
	return 0;
}

core_initcall_sync(pxa_panic_notifier);
