/*
 * drivers/misc/debugfs-pxa.c
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/cpufreq.h>


#define CPU_MIPS_LOOP   250000000
static unsigned int get_cpu_mips(void)
{
	struct timeval v1, v2;
	unsigned long i, cnt, total;

	cnt = CPU_MIPS_LOOP;
	raw_local_irq_disable();
	do_gettimeofday(&v1);
	__asm__ __volatile__(
			"mov %0, %1\n"
			"timer_loop:    subs    %0, %0, #1\n"
			"bhi     timer_loop"
			: "=&r" (i)
			: "r" (cnt)
			: "cc");
	do_gettimeofday(&v2);
	raw_local_irq_enable();
	total = (v2.tv_sec - v1.tv_sec) * 1000000 + v2.tv_usec - v1.tv_usec;

	return CPU_MIPS_LOOP / total;
}
static ssize_t mips_read(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	unsigned int mips, cpu;

	mips = get_cpu_mips();
	cpu = raw_smp_processor_id();
	pr_info("Attention! You need to keep cpufreq don't change!\n");
	pr_info("Calculated out MIPS is %dMhz, while setting is %dkhz\n",
			mips, cpufreq_get(cpu));
	return 0;
}

const struct file_operations show_mips_fops = {
	.read = mips_read,
};

struct dentry *pxa;

static int __init pxa_debugfs_init(void)
{
	struct dentry *showmips;

	pxa = debugfs_create_dir("pxa", NULL);
	if (!pxa)
		return -ENOENT;

	showmips = debugfs_create_file("showmips", 0664,
					pxa, NULL, &show_mips_fops);
	if (!showmips)
		goto err_mips;

	return 0;

err_mips:
	debugfs_remove(pxa);
	pxa = NULL;
return -ENOENT;
}

core_initcall_sync(pxa_debugfs_init);
