/*
 * linux/arch/arm/mach-mmp/coresight-v7.c
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/percpu.h>
#include <linux/cpu_pm.h>
#include <linux/notifier.h>

#include <asm/io.h>

#include <mach/regs-coresight.h>

struct ptm_info {
	u32	ptm_ter;	/* offset: 0x8 */
	u32	ptm_teer;	/* offset: 0x20 */
	u32	ptm_tecr;	/* offset: 0x24 */
	u32	ptm_cstidr;	/* offset: 0x200 */
	u32	ptm_mcr;	/* offset: 0x0 */
};

struct coresight_info {
	u32     tpiu_ffcr;	/* offset: 0x304 */
	u32     etb_ffcr;	/* offset: 0x304 */
	u32     etb_ctrl;	/* offset: 0x20 */
	u32     cstf_pcr;	/* offset: 0x4 */
	u32     cstf_fcr;	/* offset: 0x0 */
};

static struct clk *dbgclk;

void coresight_dump_pcsr(u32 cpu)
{
	u32 val;
	int i;

	if (!dbgclk)
		return;

	clk_enable(dbgclk);

	printk(KERN_EMERG "======== dump PCSR for cpu%d ========\n", cpu);
	for (i = 0; i < 8; i++) {
		val = readl(DBG_PCSR(cpu));
		printk(KERN_EMERG "PCSR of cpu%d is 0x%x\n", cpu, val);
		udelay(10);
	}
}

void coresight_panic_locked_cpu(int cpu)
{
	unsigned int val, timeout = 10000;

	if (!dbgclk)
		return;

	clk_enable(dbgclk);
	coresight_dump_pcsr(cpu);

	/* Unlock debug register access */
	writel(0xC5ACCE55, DBG_LAR(cpu));

	/* Enable Halt Debug and Instruction Transfer */
	val = readl(DBG_DSCR(cpu));
	val |= (0x1 << 14) | (0x1 << 13);
	writel(val, DBG_DSCR(cpu));

	/* Halt the dest cpu */
	writel(0x1, DBG_DRCR(cpu));

	/* Wait the cpu halted */
	do {
		val = readl(DBG_DSCR(cpu));
		if (val & 0x1)
			break;
	}while(timeout--);

	if (!timeout) {
		printk(KERN_EMERG "Cannot stop cpu%d\n", cpu);
		return;
	}

	/* Issue an instruction to change the PC of dest cpu to 0 */
	writel(0xE3A0F000, DBG_ITR(cpu));

	/* Wait until the instruction complete */
	timeout = 10000;
	do {
		val = readl(DBG_DSCR(cpu));
		if (val & (0x1 << 24))
			break;
	}while (timeout--);

	if (!timeout)
		printk(KERN_EMERG "Cannot execute instructions on cpu%d\n", cpu);

	val = readl(DBG_DSCR(cpu));
	val &= ~((0x1 << 14) | (0x1 << 13));
	writel(val, DBG_DSCR(cpu));

	/* Restart dest cpu */
	printk(KERN_EMERG "Going to trigger panic on cpu%d\n", cpu);
	writel(0x2, DBG_DRCR(cpu));

	timeout = 10000;
	do {
		val = readl(DBG_DSCR(cpu));
		if (val & (0x1 << 1))
			break;
	} while (timeout--);

	if (!timeout)
		printk(KERN_EMERG "Cannot restart cpu%d\n", cpu);
}

#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT

/* The following CTI related operations are needed by Pixiu */
#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
struct cti_info {
	u32	cti_ctrl;	/* offset: 0x0 */
	u32	cti_en_in1;	/* offset: 0x24 */
	u32	cti_en_out6;	/* offset: 0xb8 */
};

static DEFINE_PER_CPU(struct cti_info, cpu_cti_info);

static inline void cti_enable_access(void)
{
	writel_relaxed(0xC5ACCE55, CTI_LOCK);
}

static void coresight_cti_save(void)
{
	struct cti_info *p_cti_info;
	p_cti_info = &per_cpu(cpu_cti_info, smp_processor_id());

	cti_enable_access();
	p_cti_info->cti_ctrl = readl_relaxed(CTI_REG(0x0));
	p_cti_info->cti_en_in1 = readl_relaxed(CTI_REG(0x24));
	p_cti_info->cti_en_out6 = readl_relaxed(CTI_REG(0xb8));
}

static void coresight_cti_restore(void)
{
	struct cti_info *p_cti_info;
	p_cti_info = &per_cpu(cpu_cti_info, smp_processor_id());

	cti_enable_access();
	writel_relaxed(p_cti_info->cti_ctrl, CTI_REG(0x0));
	writel_relaxed(p_cti_info->cti_en_in1, CTI_REG(0x24));
	writel_relaxed(p_cti_info->cti_en_out6, CTI_REG(0xB8));

	dsb();
	isb();
}
#endif

static DEFINE_PER_CPU(struct ptm_info, cpu_ptm_info);

static struct coresight_info cst_info;

static u32 enable_etm_trace = (1 << CONFIG_NR_CPUS) - 1;
static int __init __init_etm_trace(char *arg)
{
	u32 cpu_mask;

	if (!get_option(&arg, &cpu_mask))
		return 0;

	enable_etm_trace &= cpu_mask;

	return 1;
}
__setup("etm_trace=", __init_etm_trace);

/* The following operations are needed by XDB */
static inline void ptm_enable_access(void)
{
	writel_relaxed(0xC5ACCE55, PTM_LOCK);
}

static inline void ptm_disable_access(void)
{
	writel_relaxed(0x0, PTM_LOCK);
}

static void coresight_ptm_save(void)
{
	struct ptm_info *p_ptm_info;
	p_ptm_info = &per_cpu(cpu_ptm_info, smp_processor_id());

	ptm_enable_access();
	p_ptm_info->ptm_ter = readl_relaxed(PTM_REG(0x8));
	p_ptm_info->ptm_teer = readl_relaxed(PTM_REG(0x20));
	p_ptm_info->ptm_tecr = readl_relaxed(PTM_REG(0x24));
	p_ptm_info->ptm_cstidr = readl_relaxed(PTM_REG(0x200));
	p_ptm_info->ptm_mcr = readl_relaxed(PTM_REG(0x0));
	ptm_disable_access();
}

static void coresight_ptm_restore(void)
{
	struct ptm_info *p_ptm_info;
	p_ptm_info = &per_cpu(cpu_ptm_info, smp_processor_id());

	ptm_enable_access();

	if (readl_relaxed(PTM_REG(0x0)) != p_ptm_info->ptm_mcr) {
		writel_relaxed(0x400, PTM_REG(0x0));
		writel_relaxed(p_ptm_info->ptm_ter, PTM_REG(0x8));
		writel_relaxed(p_ptm_info->ptm_teer, PTM_REG(0x20));
		writel_relaxed(p_ptm_info->ptm_tecr, PTM_REG(0x24));
		writel_relaxed(p_ptm_info->ptm_cstidr, PTM_REG(0x200));
		writel_relaxed(p_ptm_info->ptm_mcr, PTM_REG(0x0));
	}

	ptm_disable_access();

	dsb();
	isb();
}

static void coresight_core_save(void)
{
#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	coresight_cti_save();
#endif

	coresight_ptm_save();
}

static void coresight_core_restore(void)
{
#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	coresight_cti_restore();
#endif

	coresight_ptm_restore();
}

static inline void coresight_enable_access(void)
{
	writel_relaxed(0xC5ACCE55, CSTF_LOCK);
	writel_relaxed(0xC5ACCE55, TPIU_LOCK);
	writel_relaxed(0xC5ACCE55, ETB_LOCK);
}

static inline void coresight_disable_access(void)
{
	writel_relaxed(0x0, CSTF_LOCK);
	writel_relaxed(0x0, TPIU_LOCK);
	writel_relaxed(0x0, ETB_LOCK);
}

static void coresight_save(void)
{
	coresight_enable_access();
	cst_info.tpiu_ffcr = readl_relaxed(TPIU_REG(0x304));
	cst_info.etb_ffcr = readl_relaxed(ETB_REG(0x304));
	cst_info.cstf_pcr = readl_relaxed(CSTF_REG(0x4));
	cst_info.cstf_fcr = readl_relaxed(CSTF_REG(0x0));
	cst_info.etb_ctrl = readl_relaxed(ETB_REG(0x20));
	coresight_disable_access();
}

static void coresight_restore(void)
{
	coresight_enable_access();
	writel_relaxed(cst_info.tpiu_ffcr, TPIU_REG(0x304));
	writel_relaxed(cst_info.etb_ffcr, ETB_REG(0x304));
	writel_relaxed(cst_info.cstf_pcr, CSTF_REG(0x4));
	writel_relaxed(cst_info.cstf_fcr, CSTF_REG(0x0));
	writel_relaxed(cst_info.etb_ctrl, ETB_REG(0x20));
	coresight_disable_access();
}


static int coresight_notifier(struct notifier_block *self,
				unsigned long cmd, void *v)
{
	switch (cmd) {
	case CPU_PM_ENTER:
		coresight_core_save();
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		coresight_core_restore();
		break;
	case CPU_CLUSTER_PM_ENTER:
		coresight_save();
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		coresight_restore();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block coresight_notifier_block = {
	.notifier_call = coresight_notifier,
};

static int __cpuinit coresight_core_notifier(struct notifier_block *nfb,
				      unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		coresight_core_restore();
		return NOTIFY_OK;

	case CPU_DYING:
		coresight_core_save();
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static void __init coresight_mp_init(void)
{
	coresight_enable_access();
	writel_relaxed(0x1000, TPIU_REG(0x304));
	writel_relaxed(0x1, ETB_REG(0x304));
	writel_relaxed(0x0, CSTF_REG(0x4));
	writel_relaxed(enable_etm_trace, CSTF_REG(0x0));
	writel_relaxed(0x1, ETB_REG(0x20));
	coresight_disable_access();
}

void coresight_ptm_disable(u32 cpu)
{
	void __iomem *ptm_base = PTM_CORE0_VIRT_BASE + 0x1000 * cpu;

	writel_relaxed(0xC5ACCE55, (ptm_base + 0xFB0));
	writel_relaxed(0x1, (ptm_base + 0x0));
	writel_relaxed(0x0, (ptm_base + 0xFB0));

	dsb();
	isb();
}

static void coresight_ptm_enable(u32 cpu)
{
	void __iomem *ptm_base = PTM_CORE0_VIRT_BASE + 0x1000 * cpu;

	/* enable PTM access first */
	writel_relaxed(0xC5ACCE55, (ptm_base + 0xFB0));

#ifdef CONFIG_CPU_CA7MP
	writel_relaxed(0xFFFFFFFF, (ptm_base + 0x300));
#endif
	writel_relaxed(0x400, (ptm_base + 0x0));
	writel_relaxed(0x406f, (ptm_base + 0x8));
	writel_relaxed(0x6f, (ptm_base + 0x20));
	writel_relaxed(0x1000000, (ptm_base + 0x24));
	writel_relaxed((cpu + 0x1), (ptm_base + 0x200));
#ifdef CONFIG_CPU_CA7MP
	writel_relaxed(0xc940, (ptm_base + 0x0));
#else
	writel_relaxed(0xc000, (ptm_base + 0x0));
#endif

	/* disable PTM access */
	writel_relaxed(0x0, (ptm_base + 0xFB0));

	dsb();
	isb();
}

static void __init coresight_ptm_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		if (test_bit(cpu, (void *)&enable_etm_trace))
			coresight_ptm_enable(cpu);
}
#endif

static int __init coresight_init(void)
{
	dbgclk = clk_get(NULL, "DBGCLK");
	if (IS_ERR(dbgclk)) {
		pr_warn("No DBGCLK is defined...\n");
		dbgclk = NULL;
	}

#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT
	/* enable etm trace by default */
	coresight_mp_init();
	coresight_ptm_init();

	cpu_notifier(coresight_core_notifier, 0);
	cpu_pm_register_notifier(&coresight_notifier_block);
#endif

	return 0;
}

arch_initcall(coresight_init);
