/*
 * linux/arch/arm/mach-mmp/memory-pm.c
 *
 * Author:	Jerry Zhou <chunhua.zhou@archermind.com>
 *              Zhou Zhu <zzhu3@marvell.com>
 * Copyright:	(C) 2010 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/memory.h>
#include <linux/io.h>
#include <mach/regs-mcu.h>

/* included with ddr operations */
static inline int dram_status(int cs)
{
	unsigned int val = readl_relaxed(DMCU_VIRT_REG(DMCU_DRAM_STATUS));
	printk(KERN_INFO "read DRAM status %08x\n", val);
	return (val >> (cs * 4 + 4)) & 0x7;
}

static inline int dram_inited(void)
{
	return readl_relaxed(DMCU_VIRT_REG(DMCU_DRAM_STATUS)) & 0x1;
}

static inline int dram_size(int cs)
{
	unsigned int val = readl_relaxed(DMCU_VIRT_REG(DMCU_MAP_CSx(cs)));
	val = (val >> 16) & 0xf;
	if (val < 0x7) {
		printk(KERN_ERR "cs %d val %x invalid\n", cs, val);
		return 0;
	}
	return (0x8 << (val - 0x7)) * 1024 * 1024;
}

static inline int dram_start(int cs)
{
	unsigned int val = readl_relaxed(DMCU_VIRT_REG(DMCU_MAP_CSx(cs)));
	return val & 0xff800000;
}

static inline int pfn_to_cs(unsigned int pfn)
{
	int i = 0;
	for (; i < 4; i++)
		if ((pfn << 12) >= dram_start(i)
		&& (pfn << 12) < dram_start(i) + dram_size(i))
			return i;
	printk(KERN_ERR "no match cs for pfn %x\n", pfn);
	return -1;
}

static inline int dram_cs_valid(int cs)
{
	return cs >= 0 && cs < 4;
}

static inline int dram_is_lpddr2(void)
{
	unsigned int val = readl_relaxed(DMCU_VIRT_REG(DMCU_SDRAM_CTRL4));
	return (val & DMCU_SDRAM_TYPE_MASK) == DMCU_SDRAM_TYPE_LPDDR2;
}

static inline int dram_is_ddr3(void)
{
	unsigned int val = readl_relaxed(DMCU_VIRT_REG(DMCU_SDRAM_CTRL4));
	return (val & DMCU_SDRAM_TYPE_MASK) == DMCU_SDRAM_TYPE_DDR3;
}

static void lpddr2_cs_enter_dpd(int cs)
{
	unsigned int val;
	if (!dram_inited()) {
		printk(KERN_ERR "dram not inited, exit\n");
		return;
	}
	printk(KERN_INFO "cs %d DRAM status %x\n", cs, dram_status(cs));
	/* 1. set command: user_wcb_drain_reg to command 0 */
	val = (0x1 << (cs + 24)) | 0x2;
	writel(val, DMCU_VIRT_REG(DMCU_USER_COMMAND0));
	printk(KERN_INFO "write %08x --> %08x\n",
		val, DMCU_PHYS_REG(DMCU_USER_COMMAND0));
	/* 2. put csx to dpd */
	while (dram_status(cs) == 0)
		cpu_relax();
	val = (0x1 << (cs + 24)) | 0x10000000;
	writel(val, DMCU_VIRT_REG(DMCU_USER_COMMAND0));
	printk(KERN_INFO "write %08x --> %08x\n",
		val, DMCU_PHYS_REG(DMCU_USER_COMMAND0));
	while (dram_status(cs) != DMCU_DRAM_STATUS_DPD)
		cpu_relax();
	/* 3. remove mapping */
	val = readl_relaxed(DMCU_VIRT_REG(DMCU_MAP_CSx(cs)));
	val &= ~DMCU_MAP_VALID;
	writel(val, DMCU_VIRT_REG(DMCU_MAP_CSx(cs)));
	printk(KERN_INFO "write %08x --> %08x\n",
		val, DMCU_PHYS_REG(DMCU_MAP_CSx(cs)));
}

static void lpddr2_cs_exit_dpd(int cs)
{
	unsigned int val;
	if (dram_status(cs) != DMCU_DRAM_STATUS_DPD || !dram_inited()) {
		printk(KERN_ERR "dram status incorrect %8x, exit\n",
			readl_relaxed(DMCU_PHYS_REG(DMCU_USER_COMMAND0)));
		return;
	}
	printk(KERN_INFO "cs %d DRAM status %x\n", cs, dram_status(cs));
	/* 1. set val mapping */
	val = readl_relaxed(DMCU_VIRT_REG(DMCU_MAP_CSx(cs)));
	val |= DMCU_MAP_VALID;
	writel(val, DMCU_VIRT_REG(DMCU_MAP_CSx(cs)));
	printk(KERN_INFO "write %08x --> %08x\n",
		val, DMCU_VIRT_REG(DMCU_MAP_CSx(cs)));
	/* 2. out of DPD */
	val = (0x1 << (cs + 24)) | 0x20000000;
	writel(val, DMCU_VIRT_REG(DMCU_USER_COMMAND0));
	printk(KERN_INFO "write %08x --> %08x\n",
		val, DMCU_PHYS_REG(DMCU_USER_COMMAND0));
	while (dram_status(cs) != DMCU_DRAM_STATUS_PD)
		cpu_relax();
	/* 3. init commands */
	val = (0x1 << (cs + 24) | 0x00020000);
	writel(val | 0x1, DMCU_VIRT_REG(DMCU_USER_COMMAND1));
	printk(KERN_INFO "write %08x --> %08x\n",
		val | 0x1, DMCU_PHYS_REG(DMCU_USER_COMMAND1));
	writel(val | 0x2, DMCU_VIRT_REG(DMCU_USER_COMMAND1));
	printk(KERN_INFO "write %08x --> %08x\n",
		val | 0x2, DMCU_PHYS_REG(DMCU_USER_COMMAND1));
	writel(val | 0x3, DMCU_VIRT_REG(DMCU_USER_COMMAND1));
	printk(KERN_INFO "write %08x --> %08x\n",
		val | 0x3, DMCU_PHYS_REG(DMCU_USER_COMMAND1));
}

static void ddr_cs_online(int cs)
{
	if (!dram_cs_valid(cs))
		return;
	if (dram_is_lpddr2())
		lpddr2_cs_exit_dpd(cs);
}

static void ddr_cs_offline(int cs)
{
	if (!dram_cs_valid(cs))
		return;
	if (dram_is_lpddr2())
		lpddr2_cs_enter_dpd(cs);
}

#define MAX_PRIORITY      ((unsigned)(-1)>>1) /* max value of int variable */
#define MIN_PRIORITY      (~MAX_PRIORITY)     /* min value of int variable */

static int memory_pm_handler(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct memory_notify *arg = data;
	switch (action) {
	case MEM_GOING_ONLINE:
		if (self->priority == MAX_PRIORITY) {
			/* Turn on the DDR memory. */
			printk(KERN_INFO ">>MEM_GOING_ONLINE\n");
			ddr_cs_online(pfn_to_cs(arg->start_pfn));
		}
		break;
	case MEM_GOING_OFFLINE:
                if (self->priority == MIN_PRIORITY)
                        printk(KERN_INFO ">>MEM_GOING_OFFLINE\n");
                break;
	case MEM_ONLINE:
                if (self->priority == MIN_PRIORITY)
                        printk(KERN_INFO ">>MEM_ONLINE\n");
		break;
	case MEM_OFFLINE:
		if (self->priority == MIN_PRIORITY) {
			/* To do some things such as cut down the power of
			 * DDR memory.
			 */
			printk(KERN_INFO ">>MEM_OFFLINE\n");
			ddr_cs_offline(pfn_to_cs(arg->start_pfn));
		}
		break;
	case MEM_CANCEL_ONLINE:
                if (self->priority == MIN_PRIORITY)
                        printk(KERN_INFO ">>MEM_CANCEL_ONLINE\n");
		break;
	case MEM_CANCEL_OFFLINE:
                if (self->priority == MIN_PRIORITY)
                        printk(KERN_INFO ">>MEM_CANCEL_OFFLINE\n");
		break;
	}

	return 0;
}

/* Make sure the memory_offline routine will be called at the last. */
static struct notifier_block memory_offline_mem_nb ={
	.notifier_call = memory_pm_handler,
	.priority      = MIN_PRIORITY
};

/* Make sure the memory_online routine will be called at the first. */
static struct notifier_block memory_online_mem_nb ={
	.notifier_call = memory_pm_handler,
	.priority      = MAX_PRIORITY
};


static int __init mem_pm_handler_init(void)
{
	register_memory_notifier(&memory_offline_mem_nb);
	register_memory_notifier(&memory_online_mem_nb);

	return 0;
}
module_init(mem_pm_handler_init);

static void __exit mem_pm_handler_exit(void)
{
	unregister_memory_notifier(&memory_offline_mem_nb);
	unregister_memory_notifier(&memory_online_mem_nb);
}
module_exit(mem_pm_handler_exit);

MODULE_DESCRIPTION("Memory Hotplug Handler function for mmp");
MODULE_LICENSE("GPL");
