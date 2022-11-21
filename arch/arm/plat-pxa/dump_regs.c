/*
 * arch/arm/plat-pxa/dump_regs.c
 *
 * PXA988 Dump Soc registers and stack driver
 *
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <linux/io.h>
#include <asm/current.h>
#include <plat/dump_regs.h>
#include <mach/regs-map.h>

#ifdef CONFIG_CPU_PXA988
#include <mach/regs-apmu.h>
#endif


static void ioremap_reg_map(struct reg_map map[])
{
	unsigned int i;
	for (i = 0; map[i].reg_name != NULL; i++) {
		if ((map[i].vir_addr == 0) &&
		    (map[i].end_addr >= map[i].beg_addr)) {

			map[i].vir_addr = (unsigned int)ioremap(map[i].beg_addr
				, map[i].end_addr + 4 - map[i].beg_addr);

			if (map[i].vir_addr == 0)
				printk(KERN_INFO "ioremap(0x%08x, %d) failed!\n",
					map[i].beg_addr,
					map[i].end_addr + 4 - map[i].beg_addr);
		}
	}
}

static void iounmap_reg_map(struct reg_map map[])
{
	unsigned int i;
	for (i = 0; map[i].reg_name != NULL; i++) {
		if ((map[i].vir_addr != 0))
			iounmap((volatile void *)map[i].vir_addr);
	}
}

void dump_soc_regs(struct reg_map map[])
{
	unsigned int i, j, k;

	unsigned int total = 0;
	for (i = 0; map[i].reg_name != NULL; i++) {

		printk(KERN_INFO "\n%-15s\t start:%08x end:%08x size=%d\n",
			map[i].reg_name, map[i].beg_addr, map[i].end_addr,
			map[i].end_addr - map[i].beg_addr + 4);

		total += (map[i].end_addr - map[i].beg_addr + 4);

		if (unlikely(map[i].vir_addr == 0)) {
			pr_info("%s: NULL virtual address!\n",
				map[i].reg_name);
			continue;
		}

		for (j = map[i].beg_addr; j <= map[i].end_addr; j += 32) {

			printk(KERN_INFO "%08x:", j);

			for (k = j; (k <= map[i].end_addr) &&
					(k < j + 32); k += 4)
				printk(" %08x", __raw_readl(k - map[i].beg_addr
							+ map[i].vir_addr));

			printk("\n");
		}
#ifdef CONFIG_CPU_PXA988
		if (strcmp(map[i].reg_name, "Apps_PMU_2") == 0) {
			__raw_writel(__raw_readl(APMU_CP_CCR) | 0x80000000,
					APMU_CP_CCR);
			__raw_writel(__raw_readl(APMU_CP_CCR) & ~0x80000000,
					APMU_CP_CCR);
		}
#endif
	}

	printk(KERN_INFO "total dumped reg size = %d\n", total);
}



void dump_stack_and_func(void)
{
	register unsigned long current_sp asm ("sp");
	unsigned long flag;
	unsigned long *cur_stack;
	unsigned long *copy_stack;
	unsigned long *top_stack;
	char *thread_name;
	pid_t cur_pid;

	local_irq_save(flag);
	cur_stack = (unsigned long *)current_sp;
	copy_stack = (unsigned long *)current_sp;
	thread_name = current->comm;
	cur_pid = current->pid;
	top_stack = (unsigned long *)((current_sp
			& (~(THREAD_SIZE - 1))) + THREAD_SIZE);
	local_irq_restore(flag);

	printk(KERN_INFO "\nStart to print kernel stack and func in stack!\n");
	printk(KERN_INFO "Thread_name:%s PID:%d sp=%lx, Kernel_stack@%lx-to-%lx\n",
		thread_name, cur_pid, (unsigned long)cur_stack,
		(unsigned long)cur_stack & (~(THREAD_SIZE - 1)),
		(unsigned long)top_stack);

	cur_stack = (unsigned long *)((unsigned long)cur_stack & (~31));
	while (cur_stack < top_stack) {
		printk(KERN_INFO "%08lx: %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
			(unsigned long)cur_stack, *cur_stack, *(cur_stack + 1),
			*(cur_stack + 2), *(cur_stack + 3), *(cur_stack + 4),
			*(cur_stack + 5), *(cur_stack + 6), *(cur_stack + 7));

		cur_stack += 8;
	}

	printk(KERN_INFO "\n_stext:0x%x, _etext:0x%x\n",
			(unsigned int)&_stext, (unsigned int)&_etext);
	printk(KERN_INFO "\nPossible func in current stack, please check them manually\n");

	while (copy_stack <= top_stack) {
		if ((*copy_stack < (unsigned int)&_etext) &&
		    (*copy_stack > (unsigned int)&_stext))
			printk(KERN_INFO "[<%08lx>] (%pS)\n",
				(*copy_stack), (void *)(*copy_stack));
		copy_stack += 1;
	}
	printk(KERN_INFO "\nEnd of %s()\n", __func__);
}

static int __init dump_regs_init(void)
{
	printk(KERN_INFO "dump_regs_init()!\n");
	ioremap_reg_map(pxa_reg_map);
	return 0;
}

static void __exit dump_regs_exit(void)
{
	iounmap_reg_map(pxa_reg_map);
}

arch_initcall(dump_regs_init);
__exitcall(dump_regs_exit);
