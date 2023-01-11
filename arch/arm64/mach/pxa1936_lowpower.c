/*
 * linux/arch/arm/mach-mmp/pxa1936_lowpower.c
 *
 * Author:	Raul Xiong <xjian@marvell.com>
 *		Fangsuo Wu <fswu@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/clk/dvfs-dvc.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/clk/mmpdcstat.h>
#include <linux/of.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/pxa1936_powermode.h>
#include <asm/mcpm.h>
#include <asm/mcpm_plat.h>
#include <asm/cputype.h>

#include "regs-addr.h"
#include "pxa1936_lowpower.h"

#define POWER_MODE_DEBUG

#define MAX_CPU	4
#define MAX_CLUS 2
static void __iomem *apmu_virt_addr;
static void __iomem *mpmu_virt_addr;
static void __iomem *APMU_CORE_IDLE_CFG[MAX_CPU*MAX_CLUS];
static void __iomem *APMU_MP_IDLE_CFG[MAX_CPU*MAX_CLUS];
static void __iomem *ICU_GBL_INT_MSK[MAX_CPU*MAX_CLUS];
static void __iomem *APMU_CORE_RSTCTRL[MAX_CPU*MAX_CLUS];
static u32 s_awucrm;

/* Registers for different CPUs are quite scattered */
static const unsigned APMU_CORE_IDLE_CFG_OFFS[] = {
	0x124, 0x128, 0x160, 0x164, 0x304, 0x308, 0x30c, 0x310
};
static const unsigned APMU_MP_IDLE_CFG_OFFS[] = {
	0x120, 0xe4, 0x150, 0x154, 0x314, 0x318, 0x31c, 0x320
};

static const unsigned ICU_GBL_INT_MSK_OFFS[] = {
	0x228, 0x238, 0x248, 0x258, 0x278, 0x288, 0x298, 0x2a8
};

static const unsigned APMU_CORE_RSTCTRL_MSK_OFFS[] = {
	0x12c, 0x130, 0x134, 0x138, 0x324, 0x328, 0x32c, 0x330
};

/* Per-cluster and APCR_PER */
#define MPMU_APCR_PER MAX_CLUS
static const unsigned MPMU_APCR_OFS[] = {
	APCR_CLUSTER0, APCR_CLUSTER1, APCR_PER
};

#define RINDEX(cpu, clus) ((cpu) + (clus)*MAX_CPU)

static struct cpuidle_state pxa1936_modes[] = {
	[POWER_MODE_CORE_INTIDLE] = {
		.exit_latency		= 18,
		.target_residency	= 36,
		/*
		 * Use CPUIDLE_FLAG_TIMER_STOP flag to let the cpuidle
		 * framework handle the CLOCK_EVENT_NOTIFY_BROADCAST_
		 * ENTER/EXIT when entering idle states.
		 */
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "C1: Core internal clock gated",
		.enter			= cpuidle_simple_enter,
	},
	[POWER_MODE_CORE_POWERDOWN] = {
		.exit_latency		= 350,
		.target_residency	= 700,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C2",
		.desc			= "C2: Core power down",
	},
	[POWER_MODE_MP_POWERDOWN] = {
		.exit_latency		= 450,
		.target_residency	= 900,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "MP2",
		.desc			= "MP2: Core subsystem power down",
	},
	[POWER_MODE_APPS_IDLE] = {
		.exit_latency		= 500,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D1p",
		.desc			= "D1p: AP idle state",
	},
	[POWER_MODE_APPS_SLEEP] = {
		.exit_latency		= 600,
		.target_residency	= 1200,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D1pp",
		.desc			= "D1pp: Chip idle state",
	},
	[POWER_MODE_UDR_VCTCXO] = {
		.exit_latency		= 800,
		.target_residency	= 2000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D1",
		.desc			= "D1: Chip idle state",
	},
	[POWER_MODE_UDR] = {
		.exit_latency		= 900,
		.target_residency	= 2500,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D2",
		.desc			= "D2: Chip idle state",
	},
};

#define APCR_MASK (PMUM_AXISD | PMUM_APBSD | PMUM_SLPEN | PMUM_DDRCORSD | \
	PMUM_STBYEN | PMUM_VCTCXOSD)
#define PXA1936_LPM_PUSH /* experimental only */
static void pxa1936_set_dstate(u32 cpu, u32 cluster, u32 power_mode)
{
#ifdef CONFIG_PXA1936_LPM
	unsigned apcr = readl_relaxed(mpmu_virt_addr+MPMU_APCR_OFS[cluster]);
	unsigned set = 0;

	switch (power_mode) {
	case POWER_MODE_UDR:
		set |= PMUM_VCTCXOSD;
		/* fallthrough */
	case POWER_MODE_UDR_VCTCXO:
		set |= PMUM_STBYEN;
		/* fallthrough */
	case POWER_MODE_APPS_SLEEP:
		set |= PMUM_APBSD | PMUM_SLPEN | PMUM_DDRCORSD;
		/* fallthrough */
	case POWER_MODE_APPS_IDLE:
		set |= PMUM_AXISD;
	}

	apcr = (apcr & ~APCR_MASK) | set;
	writel_relaxed(apcr, mpmu_virt_addr+MPMU_APCR_OFS[cluster]);
#ifdef PXA1936_LPM_PUSH
	/*
	 * In some cases Cx enters C2MP with cpuidle vote not enabling
	 * lower sleep modes, and remains there for a very long time.
	 * This suggests cpuidle guess about coming wakeup is wrong.
	 * This prevents the system from entering low power modes.
	 * Still we cannot force APCR or the other cluster here, because
	 * if the current cluster is not the last one, and the other
	 * cluster enters MP_IDLE later, it will not re-set APCR's.
	 * Since only the last cluster reflects pm_qos it its sleep
	 * decision, such scenario would result in LPM entry which
	 * violates the current pm_qos constraints.
	 * Force APCR_PER for now.
	 */
	writel_relaxed(apcr, mpmu_virt_addr+MPMU_APCR_OFS[2]);
#endif
#endif
}

static void pxa1936_set_cstate(u32 cpu, u32 cluster, u32 power_mode)
{
	unsigned cfg = readl(APMU_CORE_IDLE_CFG[RINDEX(cpu, cluster)]);
	unsigned mcfg = readl(APMU_MP_IDLE_CFG[RINDEX(cpu, cluster)]);
	cfg &= ~(CORE_PWRDWN | CORE_IDLE);
	if (power_mode > POWER_MODE_CORE_POWERDOWN)
		power_mode = POWER_MODE_CORE_POWERDOWN;
	switch (power_mode) {
	case POWER_MODE_MP_POWERDOWN:
		/* fall through */
	case POWER_MODE_CORE_POWERDOWN:
		/* L2_SRAM_PWRDWN is set out of reset and never changed */
		mcfg |= MP_PWRDWN | MP_IDLE;
#ifndef POWER_MODE_DEBUG
		mcfg |= L2_SRAM_PWRDWN;
#else
		mcfg &= ~L2_SRAM_PWRDWN;
#endif
		cfg |= CORE_IDLE | CORE_PWRDWN;
		cfg |= MASK_GIC_nFIQ_TO_CORE | MASK_GIC_nIRQ_TO_CORE;
		/* fall through */

	case POWER_MODE_CORE_INTIDLE:
		/* cfg |= CORE_IDLE; */
		break;
	}
	writel(cfg, APMU_CORE_IDLE_CFG[RINDEX(cpu, cluster)]);
	writel(mcfg, APMU_MP_IDLE_CFG[RINDEX(cpu, cluster)]);
}


static void pxa1936_clear_state(u32 cpu, u32 cluster)
{
	int idx = RINDEX(cpu, cluster);
	unsigned cfg = readl(APMU_CORE_IDLE_CFG[idx]);
	cfg &= ~(CORE_PWRDWN | CORE_IDLE);
	cfg &= ~(MASK_GIC_nFIQ_TO_CORE | MASK_GIC_nIRQ_TO_CORE);
	writel(cfg, APMU_CORE_IDLE_CFG[idx]);

	cfg = readl(APMU_MP_IDLE_CFG[idx]);
	cfg &= ~(MP_PWRDWN | MP_IDLE);
	writel(cfg, APMU_MP_IDLE_CFG[idx]);

	pxa1936_set_dstate(cpu, cluster, 0);
}

static void pxa1936_icu_global_mask(u32 cpu, u32 cluster, u32 mask)
{
	u32 icu_msk;
	int idx = RINDEX(cpu, cluster);

	icu_msk = readl_relaxed(ICU_GBL_INT_MSK[idx]);

	if (mask)
		icu_msk |= ICU_MASK_FIQ | ICU_MASK_IRQ;
	else
		icu_msk &= ~(ICU_MASK_FIQ | ICU_MASK_IRQ);

	writel_relaxed(icu_msk, ICU_GBL_INT_MSK[idx]);
}
/*
 * low power config
 */
static void pxa1936_lowpower_config(u32 cpu, u32 cluster, u32 power_mode,
				u32 vote_state,
				u32 lowpower_enable)
{
	u32 c_state;

	/* clean up register setting */
	if (!lowpower_enable) {
		pxa1936_clear_state(cpu, cluster);
		return;
	}

	if (power_mode >= POWER_MODE_APPS_IDLE) {
		pxa1936_set_dstate(cpu, cluster, power_mode);
		c_state = POWER_MODE_MP_POWERDOWN;
	} else
		c_state = power_mode;

	pxa1936_set_cstate(cpu, cluster, c_state);
}

#define DISABLE_ALL_WAKEUP_PORTS		\
	(PMUM_SLPWP0 | PMUM_SLPWP1 | PMUM_SLPWP2 | PMUM_SLPWP3 |	\
	 PMUM_SLPWP4 | PMUM_SLPWP5 | PMUM_SLPWP6 | PMUM_SLPWP7)
/* Here we don't enable CP wakeup sources since CP will enable them */
#define ENABLE_AP_WAKEUP_SOURCES	\
	(PMUM_AP_ASYNC_INT | PMUM_AP_FULL_IDLE | PMUM_SQU_SDH1 | PMUM_SDH_23 | \
	 PMUM_KEYPRESS | PMUM_WDT | PMUM_RTC_ALARM | PMUM_AP0_2_TIMER_1 | \
	 PMUM_AP0_2_TIMER_2 | PMUM_AP0_2_TIMER_3 | PMUM_AP1_TIMER_1 | \
	 PMUM_CP_TIMER_3 | PMUM_CP_TIMER_2 | PMUM_CP_TIMER_1 | \
	 PMUM_AP1_TIMER_2 | PMUM_AP1_TIMER_3)
/* Do not enable these for now */
#define ENABLE_AP_WAKEUP_PORTS \
	 (PMUM_WAKEUP7 | PMUM_WAKEUP6 \
	 | PMUM_WAKEUP5 | PMUM_WAKEUP4 | PMUM_WAKEUP3 | PMUM_WAKEUP2 \
	 )

/*
 * Enable AP wakeup sources and ports. To enalbe wakeup
 * ports, it needs both AP side to configure MPMU_APCR
 * and CP side to configure MPMU_CPCR to really enable
 * it. To enable wakeup sources, either AP side to set
 * MPMU_AWUCRM or CP side to set MPMU_CWRCRM can really
 * enable it.
 */
static void pxa1936_save_wakeup(void)
{
	s_awucrm = readl_relaxed(mpmu_virt_addr + AWUCRM);
	writel_relaxed(s_awucrm | ENABLE_AP_WAKEUP_SOURCES,
			mpmu_virt_addr + AWUCRM);
}

static void pxa1936_restore_wakeup(void)
{
	writel_relaxed(s_awucrm,
			mpmu_virt_addr + AWUCRM);
}

static void pxa1936_set_pmu(u32 cpu, u32 calc_state, u32 vote_state)
{
	u32 mpidr = read_cpuid_mpidr();
	u32 cluster;
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	pxa1936_lowpower_config(cpu, cluster, calc_state, vote_state, 1);
	pxa1936_icu_global_mask(cpu, cluster, 1); /* might be reset by HW */
}

#ifdef POWER_MODE_DEBUG
#define MPMU_PWRMODE_STATUS 0x1030
#define STAT_MODES 6
#define STAT_MASK ((1<<STAT_MODES) - 1)
unsigned pwr_stat[STAT_MODES + 1];
static void track_power_mode(void)
{
	unsigned v = readl_relaxed(mpmu_virt_addr + MPMU_PWRMODE_STATUS);
	v >>= 16;
	if (v) {
		int i;
		writel_relaxed(v, mpmu_virt_addr + MPMU_PWRMODE_STATUS);
		v &= STAT_MASK;
		for (i = 0; i < STAT_MODES; i++)
			if (v & (1<<i))
				pwr_stat[i]++;
		pwr_stat[i] = v;
	}
}
#endif

static void pxa1936_clr_pmu(u32 cpu)
{
	u32 mpidr = read_cpuid_mpidr();
	u32 cluster;
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	pxa1936_lowpower_config(cpu, cluster, 0, 0, 0);
	pxa1936_icu_global_mask(cpu, cluster, 1); /* might be reset by HW */
#ifdef POWER_MODE_DEBUG
	track_power_mode();
#endif
}

static struct platform_power_ops pxa1936_power_ops = {
	.set_pmu	= pxa1936_set_pmu,
	.clr_pmu	= pxa1936_clr_pmu,
	.save_wakeup	= pxa1936_save_wakeup,
	.restore_wakeup	= pxa1936_restore_wakeup,
};

static struct platform_idle pxa1936_idle = {
	.cpudown_state		= POWER_MODE_CORE_POWERDOWN,
	.clusterdown_state	= POWER_MODE_MP_POWERDOWN,
	.wakeup_state		= POWER_MODE_APPS_IDLE,
	.hotplug_state		= POWER_MODE_UDR,
	/*
	 * l2_flush_state indicates to the logic in mcpm_plat.c
	 * to trigger calls to save_wakeup/restore_wakeup,
	 * but is also required for a cluster off indication to smc.
	 */
	.l2_flush_state		= POWER_MODE_MP_POWERDOWN,
	.ops			= &pxa1936_power_ops,
	.states			= pxa1936_modes,
	.state_count		= ARRAY_SIZE(pxa1936_modes),
};

static void __init pxa1936_mappings(void)
{
	int i;
	void __iomem *icu_virt_addr = regs_addr_get_va(REGS_ADDR_ICU);

	apmu_virt_addr = regs_addr_get_va(REGS_ADDR_APMU);

	for (i = 0; i < (MAX_CPU*MAX_CLUS); i++) {
		APMU_CORE_IDLE_CFG[i] =
			apmu_virt_addr + APMU_CORE_IDLE_CFG_OFFS[i];
		APMU_MP_IDLE_CFG[i] =
			apmu_virt_addr + APMU_MP_IDLE_CFG_OFFS[i];
		ICU_GBL_INT_MSK[i] =
			icu_virt_addr + ICU_GBL_INT_MSK_OFFS[i];
		APMU_CORE_RSTCTRL[i] =
			apmu_virt_addr + APMU_CORE_RSTCTRL_MSK_OFFS[i];
	}

	mpmu_virt_addr = regs_addr_get_va(REGS_ADDR_MPMU);

}

#define APMU_WAKEUP_CORE(n)		(1 << (n & 0x7))
void pxa1936_gic_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	unsigned int val = 0;
	int targ_cpu;

	gic_raise_softirq(mask, irq);
	/*
	 * Set the wakeup bits to make sure the core(s) can respond to
	 * the IPI interrupt.
	 * If the target core(s) is alive, this operation is ignored by
	 * the APMU. After the core wakes up, these corresponding bits
	 * are clearly automatically by PMU hardware.
	 */
	preempt_disable();
	for_each_cpu(targ_cpu, mask) {
		BUG_ON(targ_cpu >= CONFIG_NR_CPUS);
		val |= APMU_WAKEUP_CORE(targ_cpu);
	}
	writel_relaxed(val, APMU_CORE_RSTCTRL[smp_processor_id()]);
	preempt_enable();
}

static const struct of_device_id mcpm_of_match[] __initconst = {
	{ .compatible = "arm,mcpm",},
	{},
};

static int __init pxa1936_lowpower_init(void)
{
	struct device_node *np;

	if (!of_machine_is_compatible("marvell,pxa1936"))
		return -ENODEV;

	np = of_find_matching_node(NULL, mcpm_of_match);
	if (!np || !of_device_is_available(np))
		return -ENODEV;
	pr_info("Initialize pxa1936 low power controller based on mcpm.\n");

	pxa1936_mappings();
	mcpm_plat_power_register(&pxa1936_idle);

	set_smp_cross_call(pxa1936_gic_raise_softirq);

	return 0;
}
early_initcall(pxa1936_lowpower_init);
