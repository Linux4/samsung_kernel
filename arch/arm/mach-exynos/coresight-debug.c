/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/moduleparam.h>
#include <linux/kallsyms.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>

#include <plat/cpu.h>
#include <mach/map.h>
#include <mach/regs-pmu.h>
#include "common.h"

#define CS_OSLOCK		(0x300)
#define CS_PC_VAL		(0x84)

#define CS_SJTAG_STATUS		(0x8004)
#define CS_SJTAG_SOFT_LOCK	(1<<2)
#define ITERATION		(5)

#define SJTAG_STAT_BASE		(EXYNOS_PA_CORESIGHT + CS_SJTAG_STATUS)

static DEFINE_SPINLOCK(lock);
static int exynos_cs_stat;
static unsigned int exynos_cs_base[NR_CPUS];
static unsigned int exynos_cs_pc[NR_CPUS][ITERATION];

static int exynos_core_status(int cpu)
{
	unsigned int val;
	unsigned int check_cpu = soc_is_exynos5260() ? cpu^4 : cpu;

	val = __raw_readl(EXYNOS_ARM_CORE_STATUS(check_cpu));
	if (soc_is_exynos5260())
		return val & 0x40000 ? 1 : 0;
	else
		return val & EXYNOS_CORE_LOCAL_PWR_EN ? 1 : 0;
}

unsigned int cpu_debug_offset(unsigned int cpu)
{
	unsigned int core;
	unsigned int dbg_offset;

	if (soc_is_exynos5260()) {
		core = cpu > 3 ? (cpu % 3) - 1 : cpu;
		dbg_offset = cpu > 3 ? 0x30000 : 0x50000;
	} else {
		core = cpu;
		dbg_offset = 0x10000;
	}

	return EXYNOS_PA_CORESIGHT + dbg_offset + (core * 0x2000);
}

void exynos_cs_show_pcval(void)
{
	unsigned long flags;
	unsigned int cpu, iter;
	unsigned int val = 0;

	if (exynos_cs_stat < 0)
		return;

	spin_lock_irqsave(&lock, flags);

	for (iter = 0; iter < ITERATION; iter++) {
		for (cpu = 0; cpu < NR_CPUS; cpu++) {
			void __iomem *base = (void *) exynos_cs_base[cpu];

			if (base == NULL || !exynos_core_status(cpu)) {
				exynos_cs_base[cpu] = 0;
				continue;
			}

			/* Release OSlock */
			writel(0x1, base + CS_OSLOCK);

			/* Read current PC value */
			val = __raw_readl(base + CS_PC_VAL);

			if (cpu >= 4) {
				/* The PCSR of A15 shoud be substracted 0x8 from
				 * curretnly PCSR value */
				val -= 0x8;
			}

			exynos_cs_pc[cpu][iter] = val;
		}
	}

	spin_unlock_irqrestore(&lock, flags);

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (exynos_cs_base[cpu] == 0)
			continue;

		pr_err("CPU[%d] saved pc value\n", cpu);
		for (iter = 0; iter < ITERATION; iter++) {
			char buf[KSYM_SYMBOL_LEN];

			sprint_symbol(buf, exynos_cs_pc[cpu][iter]);
			pr_err("      0x%08x : %s\n", exynos_cs_pc[cpu][iter], buf);
		}
	}
}
EXPORT_SYMBOL(exynos_cs_show_pcval);

static int exynos_cs_init(void)
{
	void __iomem *sjtag_base;
	unsigned int sjtag;
	unsigned int cpu;

	pr_debug("%s\n", __func__);

	/* Check Secure JTAG */
	sjtag_base = ioremap(SJTAG_STAT_BASE, SZ_4);
	if (!sjtag_base) {
		pr_err("%s: cannot ioremap cs base.\n", __func__);
		exynos_cs_stat = -ENOMEM;
		goto err_func;
	}

	sjtag = readl(sjtag_base);
	iounmap(sjtag_base);

	if (sjtag & CS_SJTAG_SOFT_LOCK) {
		exynos_cs_stat = -EIO;
		goto err_func;
	}

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		void __iomem *cs_base;

		cs_base = ioremap(cpu_debug_offset(cpu), SZ_4K);
		if (!cs_base) {
			pr_err("%s: failed to remap for cs base.\n", __func__);
			exynos_cs_stat = -ENOMEM;
			goto err_func;
		}

		exynos_cs_base[cpu] = (unsigned int) cs_base;
	}

err_func:
	return exynos_cs_stat;
}

subsys_initcall(exynos_cs_init);
