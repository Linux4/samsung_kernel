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

#include <linux/io.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/percpu.h>
#include <linux/of_address.h>

#include "regs-addr.h"

static void __iomem *debug_base;

#define DBG_REG(cpu, offset)	(debug_base + cpu * 0x2000 + offset)

#define DBG_ITR(cpu)		DBG_REG(cpu, 0x84)	/* Write only */
#define DBG_PCSR(cpu)		DBG_REG(cpu, 0x84)	/* Read only */
#define DBG_DSCR(cpu)		DBG_REG(cpu, 0x88)
#define DBG_DTRTX(cpu)		DBG_REG(cpu, 0x8C)
#define DBG_DRCR(cpu)		DBG_REG(cpu, 0x90)
#define DBG_LAR(cpu)		DBG_REG(cpu, 0xFB0)

void arch_enable_access(u32 cpu)
{
	writel(0xC5ACCE55, DBG_LAR(cpu));
}

void arch_dump_pcsr(u32 cpu)
{
	u32 val;
	int i;

	pr_emerg("======== dump PCSR for cpu%d ========\n", cpu);
	for (i = 0; i < 8; i++) {
		val = readl(DBG_PCSR(cpu));
		pr_emerg("PCSR of cpu%d is 0x%x\n", cpu, val);
		udelay(10);
	}
}

int arch_halt_cpu(u32 cpu)
{
	u32 timeout, val;

	/* Enable Halt Debug and Instruction Transfer */
	val = readl(DBG_DSCR(cpu));
	val |= (0x1 << 14) | (0x1 << 13);
	writel(val, DBG_DSCR(cpu));

	/* Halt the dest cpu */
	writel(0x1, DBG_DRCR(cpu));

	/* Wait the cpu halted */
	timeout = 10000;
	do {
		val = readl(DBG_DSCR(cpu));
		if (val & 0x1)
			break;
	} while (--timeout);

	if (!timeout)
		return -1;

	return 0;
}

void arch_insert_inst(u32 cpu)
{
	u32 timeout, val;

	/* Issue an instruction to change the PC of dest cpu to 0 */
	writel(0xE3A0F000, DBG_ITR(cpu));

	/* Wait until the instruction complete */
	timeout = 10000;
	do {
		val = readl(DBG_DSCR(cpu));
		if (val & (0x1 << 24))
			break;
	} while (--timeout);

	if (!timeout)
		pr_emerg("Cannot execute instructions on cpu%d\n", cpu);

	if (val & (0x1 << 6))
		pr_emerg("Occurred exception in debug state on cpu%d\n", cpu);
}

void arch_restart_cpu(u32 cpu)
{
	u32 timeout, val;

	val = readl(DBG_DSCR(cpu));
	val &= ~((0x1 << 14) | (0x1 << 13));
	writel(val, DBG_DSCR(cpu));

	/* Restart dest cpu */
	writel(0x2, DBG_DRCR(cpu));

	timeout = 10000;
	do {
		val = readl(DBG_DSCR(cpu));
		if (val & (0x1 << 1))
			break;
	} while (--timeout);

	if (!timeout)
		pr_emerg("Cannot restart cpu%d\n", cpu);
}

/* The following CTI related operations are needed by PMU */
struct cti_info {
	u32	cti_ctrl;	/* offset: 0x0 */
	u32	cti_en_in1;	/* offset: 0x24 */
	u32	cti_en_out6;	/* offset: 0xb8 */
};

static DEFINE_PER_CPU(struct cti_info, cpu_cti_info);

#define CTI_EN_MASK		0x0F
#define CTI_CTRL_OFFSET		0x0
#define CTI_INTACK_OFFSET	0x10
#define CTI_EN_IN1_OFFSET	0x24
#define CTI_EN_OUT6_OFFSET	0xB8
#define CTI_LOCK_OFFSET		0xFB0

static void __iomem *cti_base;

#define CTI_REG(offset)		(cti_base \
				+ 0x1000 * raw_smp_processor_id() \
				+ offset)


static inline void cti_enable_access(void)
{
	writel_relaxed(0xC5ACCE55, CTI_REG(CTI_LOCK_OFFSET));
}

static void coresight_cti_save(void)
{
	struct cti_info *p_cti_info;
	p_cti_info = &per_cpu(cpu_cti_info, smp_processor_id());

	cti_enable_access();
	p_cti_info->cti_ctrl = readl_relaxed(CTI_REG(CTI_CTRL_OFFSET));
	p_cti_info->cti_en_in1 = readl_relaxed(CTI_REG(CTI_EN_IN1_OFFSET));
	p_cti_info->cti_en_out6 = readl_relaxed(CTI_REG(CTI_EN_OUT6_OFFSET));
}

static void coresight_cti_restore(void)
{
	struct cti_info *p_cti_info;
	p_cti_info = &per_cpu(cpu_cti_info, smp_processor_id());

	cti_enable_access();
	writel_relaxed(p_cti_info->cti_en_in1, CTI_REG(CTI_EN_IN1_OFFSET));
	writel_relaxed(p_cti_info->cti_en_out6, CTI_REG(CTI_EN_OUT6_OFFSET));
	writel_relaxed(p_cti_info->cti_ctrl, CTI_REG(CTI_CTRL_OFFSET));
}

/* The following code is needed because our PMU interrupt is routed by CTI */
static void __init mmp_cti_enable(u32 cpu)
{
	void __iomem *p_cti_base = cti_base + 0x1000 * cpu;
	u32 tmp;

	/* Unlock CTI */
	writel_relaxed(0xC5ACCE55, p_cti_base + CTI_LOCK_OFFSET);

	/*
	 * Enables a cross trigger event to the corresponding channel.
	 */
	tmp = readl_relaxed(p_cti_base + CTI_EN_IN1_OFFSET);
	tmp &= ~CTI_EN_MASK;
	tmp |= 0x1 << cpu;
	writel_relaxed(tmp, p_cti_base + CTI_EN_IN1_OFFSET);

	tmp = readl_relaxed(p_cti_base + CTI_EN_OUT6_OFFSET);
	tmp &= ~CTI_EN_MASK;
	tmp |= 0x1 << cpu;
	writel_relaxed(tmp, p_cti_base + CTI_EN_OUT6_OFFSET);

	/* Enable CTI */
	writel_relaxed(0x1, p_cti_base + CTI_CTRL_OFFSET);
}

static int __init mmp_cti_init(void)
{
	struct device_node *node;
	int cpu;

	node = of_find_compatible_node(NULL, NULL, "marvell,coresight-cti");
	if (!node) {
		pr_err("Failed to find CTI node!\n");
		return -ENODEV;
	}

	cti_base = of_iomap(node, 0);
	if (!cti_base) {
		pr_err("Failed to map coresight cti register\n");
		return -ENOMEM;
	}

	for (cpu = 0; cpu < nr_cpu_ids; cpu++)
		mmp_cti_enable(cpu);

	return 0;
}

void mmp_pmu_ack(void)
{
	writel_relaxed(0x40, CTI_REG(CTI_INTACK_OFFSET));
}
EXPORT_SYMBOL(mmp_pmu_ack);

#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT

struct coresight_info {
	u32     etb_ffcr;	/* offset: 0x304 */
	u32     etb_ctrl;	/* offset: 0x20 */
	u32     cstf_pcr;	/* offset: 0x4 */
	u32     cstf_fcr;	/* offset: 0x0 */
};

struct etm_info {
	u32	etm_ter;	/* offset: 0x8 */
	u32	etm_teer;	/* offset: 0x20 */
	u32	etm_tecr;	/* offset: 0x24 */
	u32	etm_cstidr;	/* offset: 0x200 */
	u32	etm_mcr;	/* offset: 0x0 */
};

struct local_etb_info {
	u32     etb_ffcr;	/* offset: 0x304 */
	u32     etb_ctrl;	/* offset: 0x20 */
};

static DEFINE_PER_CPU(struct etm_info, cpu_etm_info);

static struct coresight_info cst_info;

static void __iomem *etm_base;
static void __iomem *cstf_base;
static void __iomem *global_etb_base;
static void __iomem *local_etb_base;

#define ETM_REG(offset)		((etm_base	\
				+ 0x1000 * raw_smp_processor_id())	\
				+ offset)
#define CSTF_REG(offset)	(cstf_base + offset)
#define GLOBAL_ETB_REG(offset)	(global_etb_base + offset)
#define LOCAL_ETB_REG(offset)	((local_etb_base	\
				+ 0x1000 * raw_smp_processor_id())	\
				+ offset)

/* The following operations are needed by XDB */
static inline void etm_enable_access(void)
{
	writel_relaxed(0xC5ACCE55, ETM_REG(0xFB0));
}

static inline void etm_disable_access(void)
{
	writel_relaxed(0x0, ETM_REG(0xFB0));
}

static void coresight_etm_save(void)
{
	struct etm_info *p_etm_info;
	p_etm_info = &per_cpu(cpu_etm_info, smp_processor_id());

	etm_enable_access();
	p_etm_info->etm_ter = readl_relaxed(ETM_REG(0x8));
	p_etm_info->etm_teer = readl_relaxed(ETM_REG(0x20));
	p_etm_info->etm_tecr = readl_relaxed(ETM_REG(0x24));
	p_etm_info->etm_cstidr = readl_relaxed(ETM_REG(0x200));
	p_etm_info->etm_mcr = readl_relaxed(ETM_REG(0x0));
	etm_disable_access();
}

static void coresight_etm_restore(void)
{
	struct etm_info *p_etm_info;
	p_etm_info = &per_cpu(cpu_etm_info, smp_processor_id());

	etm_enable_access();

	if (readl_relaxed(ETM_REG(0x0)) != p_etm_info->etm_mcr) {
		writel_relaxed(0x400, ETM_REG(0x0));
		writel_relaxed(p_etm_info->etm_ter, ETM_REG(0x8));
		writel_relaxed(p_etm_info->etm_teer, ETM_REG(0x20));
		writel_relaxed(p_etm_info->etm_tecr, ETM_REG(0x24));
		writel_relaxed(p_etm_info->etm_cstidr, ETM_REG(0x200));
		writel_relaxed(p_etm_info->etm_mcr, ETM_REG(0x0));
	}

	etm_disable_access();
}

static DEFINE_PER_CPU(struct local_etb_info, local_etb_info);

static inline void local_etb_enable_access(void)
{
	writel_relaxed(0xC5ACCE55, LOCAL_ETB_REG(0xFB0));
}

static inline void local_etb_disable_access(void)
{
	writel_relaxed(0x0, LOCAL_ETB_REG(0xFB0));
}

static void coresight_local_etb_save(void)
{
	struct local_etb_info *p_local_etb_info;
	p_local_etb_info = &per_cpu(local_etb_info, raw_smp_processor_id());

	local_etb_enable_access();
	p_local_etb_info->etb_ffcr = readl_relaxed(LOCAL_ETB_REG(0x304));
	p_local_etb_info->etb_ctrl = readl_relaxed(LOCAL_ETB_REG(0x20));
	local_etb_disable_access();
}

static void coresight_local_etb_restore(void)
{
	struct local_etb_info *p_local_etb_info;
	p_local_etb_info = &per_cpu(local_etb_info, raw_smp_processor_id());

	local_etb_enable_access();
	writel_relaxed(p_local_etb_info->etb_ffcr, LOCAL_ETB_REG(0x304));
	writel_relaxed(p_local_etb_info->etb_ctrl, LOCAL_ETB_REG(0x20));
	local_etb_disable_access();
}

static inline void coresight_enable_access(void)
{
	writel_relaxed(0xC5ACCE55, CSTF_REG(0xFB0));
	writel_relaxed(0xC5ACCE55, GLOBAL_ETB_REG(0xFB0));
}

static inline void coresight_disable_access(void)
{
	writel_relaxed(0x0, CSTF_REG(0xFB0));
	writel_relaxed(0x0, GLOBAL_ETB_REG(0xFB0));
}

static void coresight_save_mpinfo(void)
{
	coresight_enable_access();
	cst_info.etb_ffcr = readl_relaxed(GLOBAL_ETB_REG(0x304));
	cst_info.cstf_pcr = readl_relaxed(CSTF_REG(0x4));
	cst_info.cstf_fcr = readl_relaxed(CSTF_REG(0x0));
	cst_info.etb_ctrl = readl_relaxed(GLOBAL_ETB_REG(0x20));
	coresight_disable_access();
}

static void coresight_restore_mpinfo(void)
{
	coresight_enable_access();
	writel_relaxed(cst_info.etb_ffcr, GLOBAL_ETB_REG(0x304));
	writel_relaxed(cst_info.cstf_pcr, CSTF_REG(0x4));
	writel_relaxed(cst_info.cstf_fcr, CSTF_REG(0x0));
	writel_relaxed(cst_info.etb_ctrl, GLOBAL_ETB_REG(0x20));
	coresight_disable_access();
}

static void __init coresight_mp_init(u32 enable_mask)
{
	coresight_enable_access();
	writel_relaxed(0x1, GLOBAL_ETB_REG(0x304));
	writel_relaxed(0x0, CSTF_REG(0x4));
	writel_relaxed(enable_mask, CSTF_REG(0x0));
	writel_relaxed(0x1, GLOBAL_ETB_REG(0x20));
	coresight_disable_access();
}

static void __init coresight_local_etb_init(u32 cpu)
{
	void __iomem *p_local_etb_base = local_etb_base + 0x1000 * cpu;

	writel_relaxed(0xC5ACCE55, (p_local_etb_base + 0xFB0));
	writel_relaxed(0x1, p_local_etb_base + 0x304);
	writel_relaxed(0x1, p_local_etb_base + 0x20);
	writel_relaxed(0x0, (p_local_etb_base + 0xFB0));
}

static void __init coresight_etm_enable(u32 cpu)
{
	void __iomem *p_etm_base = etm_base + 0x1000 * cpu;

	/* enable etm access first */
	writel_relaxed(0xC5ACCE55, (p_etm_base + 0xFB0));
	writel_relaxed(0xFFFFFFFF, (p_etm_base + 0x300));

	writel_relaxed(0x400, (p_etm_base + 0x0));
	writel_relaxed(0x406f, (p_etm_base + 0x8));
	writel_relaxed(0x6f, (p_etm_base + 0x20));
	writel_relaxed(0x1000000, (p_etm_base + 0x24));
	writel_relaxed((cpu + 0x1), (p_etm_base + 0x200));

	writel_relaxed(0xc940, (p_etm_base + 0x0));

	/* disable etm access */
	writel_relaxed(0x0, (p_etm_base + 0xFB0));
}

static void __init coresight_percore_init(u32 cpu)
{
	if (local_etb_base)
		coresight_local_etb_init(cpu);

	coresight_etm_enable(cpu);

	dsb();
	isb();
}

static int __init coresight_parse_trace_dt(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "marvell,coresight-etm");
	if (!node) {
		pr_err("Failed to find ETM node!\n");
		return -ENODEV;
	}

	etm_base = of_iomap(node, 0);
	if (!etm_base) {
		pr_err("Failed to map coresight etm register\n");
		return -ENOMEM;
	}

	node = of_find_compatible_node(NULL, NULL, "marvell,coresight-cstf");
	if (!node) {
		pr_err("Failed to find CSTF node!\n");
		return -ENODEV;
	}

	cstf_base = of_iomap(node, 0);
	if (!cstf_base) {
		pr_err("Failed to map coresight CSTF register\n");
		return -ENOMEM;
	}

	node = of_find_compatible_node(NULL, NULL, "marvell,coresight-cetb");
	if (!node) {
		pr_err("Failed to find central ETB node!\n");
		return -ENODEV;
	}

	global_etb_base = of_iomap(node, 0);
	if (!global_etb_base) {
		pr_err("Failed to map coresight central ETB register\n");
		return -ENOMEM;
	}

	node = of_find_compatible_node(NULL, NULL, "marvell,coresight-letb");
	if (!node)
		pr_err("No local ETB found!\n");
	else {
		local_etb_base = of_iomap(node, 0);
		if (!local_etb_base) {
			pr_err("Failed to map coresight local ETB register\n");
			return -ENOMEM;
		}
	}

	return 0;
}

void arch_enable_trace(u32 enable_mask)
{
	int cpu;

	coresight_parse_trace_dt();

	coresight_mp_init(enable_mask);
	for_each_possible_cpu(cpu)
		if (test_bit(cpu, (void *)&enable_mask))
			coresight_percore_init(cpu);
}
#endif

void arch_save_coreinfo(void)
{
	if (cti_base)
		coresight_cti_save();

#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT
	if (local_etb_base)
		coresight_local_etb_save();

	coresight_etm_save();
#endif
}

void arch_restore_coreinfo(void)
{
	if (cti_base)
		coresight_cti_restore();

#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT
	if (local_etb_base)
		coresight_local_etb_restore();

	coresight_etm_restore();
	dsb();
	isb();
#endif
}

void arch_save_mpinfo(void)
{
#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT
	coresight_save_mpinfo();
#endif
}

void arch_restore_mpinfo(void)
{
#ifdef CONFIG_CORESIGHT_TRACE_SUPPORT
	coresight_restore_mpinfo();
#endif
}

static int __init coresight_parse_dbg_dt(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "marvell,coresight-dbg");
	if (!node) {
		pr_err("Failed to find DBG node!\n");
		return -ENODEV;
	}

	debug_base = of_iomap(node, 0);
	if (!debug_base) {
		pr_err("Failed to map coresight debug register\n");
		return -ENOMEM;
	}

	return 0;
}

static void __init coresight_enable_external_agent(void __iomem *addr)
{
	u32 tmp;

	tmp = readl_relaxed(addr);
	tmp |= 0x100000;
	writel_relaxed(tmp, addr);
}

static int __init coresight_external_agent_init(void)
{
	void __iomem *ciu_base = regs_addr_get_va(REGS_ADDR_CIU);

	/* enable access CTI registers for core */
	coresight_enable_external_agent(ciu_base + 0xd0);
	coresight_enable_external_agent(ciu_base + 0xe0);
	coresight_enable_external_agent(ciu_base + 0xf0);
	coresight_enable_external_agent(ciu_base + 0xf8);

	return 0;
}

void arch_coresight_init(void)
{
	/* Needed for debug modules */
	coresight_parse_dbg_dt();

#ifndef CONFIG_TZ_HYPERVISOR
	/* if enable TrustZone, move core config to TZSW. */
	coresight_external_agent_init();
#endif

	/* It is needed on emei/helan serials to route PMU interrupt */
	mmp_cti_init();

	return;
}
