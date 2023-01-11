/*
 * linux/arch/arm/mach-mmp/pxa988_lowpower.c
 *
 * Author:	Raul Xiong <xjian@marvell.com>
 * 		Fangsuo Wu <fswu@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <asm/cpuidle.h>
#include <asm/mach/map.h>
#include <asm/mcpm.h>
#include <mach/addr-map.h>
#include <linux/cputype.h>
#include <mach/mmp_cpuidle.h>
#include <mach/pxa988_lowpower.h>
#include <mach/regs-apbc.h>

#include "regs-addr.h"
/*
 * Currently apmu and mpmu didn't use device tree.
 * here just use ioremap to get apmu and mpmu virtual
 * base address.
 */
/* All these regs are per-cpu owned */
static void __iomem *APMU_CORE_IDLE_CFG[4];
static void __iomem *APMU_MP_IDLE_CFG[4];
static void __iomem *ICU_GBL_INT_MSK[4];

static struct cpuidle_state pxa988_modes[] = {
	[0] = {
		.exit_latency		= 18,
		.target_residency	= 36,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "C1: Core internal clock gated",
		.enter			= arm_cpuidle_simple_enter,
	},
	[1] = {
		.exit_latency		= 350,
		.target_residency	= 700,
		/*
		 * Use CPUIDLE_FLAG_TIMER_STOP flag to let the cpuidle
		 * framework handle the CLOCK_EVENT_NOTIFY_BROADCAST_
		 * ENTER/EXIT when entering idle states.
		 */
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "C2",
		.desc			= "C2: Core power down",
	},
	[2] = {
		.exit_latency		= 500,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D1p",
		.desc			= "D1p: AP idle state",
	},
	[3] = {
		.exit_latency		= 600,
		.target_residency	= 1200,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D1",
		.desc			= "D1: Chip idle state",
	},
};

/*
 * edge wakeup
 *
 * To enable edge wakeup:
 * 1. Set mfp reg edge detection bits;
 * 2. Enable mfp ICU interrupt, but disable gic interrupt;
 * 3. Enable corrosponding wakeup port;
 */
#define ICU_IRQ_ENABLE		((1 << 6) | (1 << 4) | (1 << 0))
#define EDGE_WAKEUP_ICU		50	//icu interrupt num for edge wakeup

static void edge_wakeup_port_enable(void)
{
	u32 pad_awucrm;

	if (readl_relaxed(regs_addr_get_va(REGS_ADDR_MPMU) + APCR) & PMUM_SLPWP2) {
		pr_err("Port2 is NOT enabled in APCR!\n");
		pr_err("APCR: 0x%x\n",
			readl_relaxed(regs_addr_get_va(REGS_ADDR_MPMU) + APCR));
		BUG();
	}

	pad_awucrm = readl_relaxed(regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
	writel_relaxed(pad_awucrm | PMUM_WAKEUP2,
		regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
}

static void edge_wakeup_port_disable(void)
{
	u32 pad_awucrm;

	pad_awucrm = readl_relaxed(regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
	writel_relaxed(pad_awucrm & ~PMUM_WAKEUP2,
		regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
}

static void edge_wakeup_icu_enable(void)
{
	writel_relaxed(ICU_IRQ_ENABLE,
		regs_addr_get_va(REGS_ADDR_ICU) + (EDGE_WAKEUP_ICU << 2));
}

static void edge_wakeup_icu_disable(void)
{
	writel_relaxed(0, regs_addr_get_va(REGS_ADDR_ICU) + (EDGE_WAKEUP_ICU << 2));
}

static int need_restore_pad_wakeup;
static void pxa988_edge_wakeup_enable(void)
{
	edge_wakeup_mfp_enable();
	edge_wakeup_icu_enable();
	edge_wakeup_port_enable();
	need_restore_pad_wakeup = 1;
}

static void pxa988_edge_wakeup_disable(void)
{
	if (!need_restore_pad_wakeup)
		return;
	edge_wakeup_port_disable();
	edge_wakeup_icu_disable();
	edge_wakeup_mfp_disable();
	need_restore_pad_wakeup = 0;
}

/* SDH wakeup */
#define SDH_WAKEUP_ICU		94	/* icu interrupt num for sdh wakeup */
static void sdh_wakeup_icu_enable(void)
{
	if (cpu_is_pxa1U88())
		writel_relaxed(ICU_IRQ_ENABLE, regs_addr_get_va(REGS_ADDR_ICU) +
			(SDH_WAKEUP_ICU << 2));
}

static void sdh_wakeup_icu_disable(void)
{
	if (cpu_is_pxa1U88())
		writel_relaxed(0, regs_addr_get_va(REGS_ADDR_ICU) +
			(SDH_WAKEUP_ICU << 2));
}

static void pxa988_lowpower_config(u32 cpu, u32 power_state,
		u32 lowpower_enable)
{
	u32 core_idle_cfg, mp_idle_cfg[CONFIG_NR_CPUS] = {0}, apcr;
	int cpu_idx;

	core_idle_cfg = readl_relaxed(APMU_CORE_IDLE_CFG[cpu]);
	apcr = readl_relaxed(regs_addr_get_va(REGS_ADDR_MPMU) + APCR);

	if (power_state < POWER_MODE_UDR || !lowpower_enable)
		mp_idle_cfg[cpu] = readl_relaxed(APMU_MP_IDLE_CFG[cpu]);
	else {
		for_each_possible_cpu(cpu_idx)
			mp_idle_cfg[cpu_idx] = readl_relaxed(APMU_MP_IDLE_CFG[cpu_idx]);
	}

	if (lowpower_enable) {
		/* Suppose that there should be no one touch the MPMU_APCR
		 * but only Last man entering the low power modes which are
		 * deeper than M2. Thus, add BUG check here.
		 */
		if (apcr & (PMUM_DDRCORSD | PMUM_APBSD | PMUM_AXISD |
					PMUM_VCTCXOSD | PMUM_STBYEN | PMUM_SLPEN)) {
			pr_err("APCR is set to 0x%X by the Non-lastman\n", apcr);
			BUG();
		}

		switch (power_state) {
		case POWER_MODE_UDR:
			for_each_possible_cpu(cpu_idx)
				mp_idle_cfg[cpu_idx] |= PMUA_MP_L2_SRAM_POWER_DOWN;
			if (!cpu_is_pxa1U88())
				apcr |= PMUM_VCTCXOSD;
			/* fall through */
		case POWER_MODE_UDR_VCTCXO:
			apcr |= PMUM_STBYEN;
			/* fall through */
		case POWER_MODE_SYS_SLEEP:
			apcr |= PMUM_APBSD;
			apcr |= PMUM_SLPEN;
			apcr |= PMUM_DDRCORSD;
			/* Shut down VCTCXO in D1 to save power consumption */
			if (cpu_is_pxa1U88())
				apcr |= PMUM_VCTCXOSD;
			/* fall through */
		case POWER_MODE_APPS_IDLE:
			apcr |= PMUM_AXISD;
			/*
			 * Normally edge wakeup should only take effect in D1 and
			 * deeper modes. But due to SD host hardware requirement,
			 * it has to be put in D1P mode.
			 */
			pxa988_edge_wakeup_enable();
			sdh_wakeup_icu_enable();
			/* fall through */
		case POWER_MODE_CORE_POWERDOWN:
			mp_idle_cfg[cpu] |= PMUA_MP_POWER_DOWN;
			mp_idle_cfg[cpu] |= PMUA_MP_IDLE;
			core_idle_cfg |= PMUA_CORE_POWER_DOWN;
			core_idle_cfg |= PMUA_CORE_IDLE;
			/* fall through */
		case POWER_MODE_CORE_INTIDLE:
			break;
		default:
			WARN(1, "Invalid power state!\n");
		}
	} else {
		core_idle_cfg &= ~(PMUA_CORE_IDLE | PMUA_CORE_POWER_DOWN);
		mp_idle_cfg[cpu] &= ~(PMUA_MP_IDLE | PMUA_MP_POWER_DOWN |
				PMUA_MP_L2_SRAM_POWER_DOWN |
				PMUA_MP_MASK_CLK_OFF);
		apcr &= ~(PMUM_DDRCORSD | PMUM_APBSD | PMUM_AXISD |
			PMUM_VCTCXOSD | PMUM_STBYEN | PMUM_SLPEN);
		pxa988_edge_wakeup_disable();
		sdh_wakeup_icu_disable();
	}

	writel_relaxed(core_idle_cfg, APMU_CORE_IDLE_CFG[cpu]);
	writel_relaxed(apcr, regs_addr_get_va(REGS_ADDR_MPMU) + APCR);
	if (power_state < POWER_MODE_UDR || !lowpower_enable)
		writel_relaxed(mp_idle_cfg[cpu], APMU_MP_IDLE_CFG[cpu]);
	else {
		for_each_possible_cpu(cpu_idx)
			writel_relaxed(mp_idle_cfg[cpu_idx], APMU_MP_IDLE_CFG[cpu_idx]);
	}

	return;
}

#define DISABLE_ALL_WAKEUP_PORTS		\
	(PMUM_SLPWP0 | PMUM_SLPWP1 | PMUM_SLPWP2 | PMUM_SLPWP3 |	\
	 PMUM_SLPWP4 | PMUM_SLPWP5 | PMUM_SLPWP6 | PMUM_SLPWP7)
/* Here we don't enable CP wakeup sources since CP will enable them */
#define ENABLE_AP_WAKEUP_SOURCES	\
	(PMUM_AP_ASYNC_INT | PMUM_AP_FULL_IDLE | PMUM_SQU_SDH1 | PMUM_SDH_23 | \
	 PMUM_KEYPRESS | PMUM_WDT | PMUM_RTC_ALARM | PMUM_AP0_2_TIMER_1 | \
	 PMUM_AP0_2_TIMER_2 | PMUM_AP0_2_TIMER_3 | PMUM_AP1_TIMER_1 | \
	 PMUM_AP1_TIMER_2 | PMUM_AP1_TIMER_3 | PMUM_WAKEUP7 | PMUM_WAKEUP6 | \
	 PMUM_WAKEUP5 | PMUM_WAKEUP4 | PMUM_WAKEUP3 | PMUM_WAKEUP2)
static u32 s_awucrm;
/*
 * Enable AP wakeup sources and ports. To enalbe wakeup
 * ports, it needs both AP side to configure MPMU_APCR
 * and CP side to configure MPMU_CPCR to really enable
 * it. To enable wakeup sources, either AP side to set
 * MPMU_AWUCRM or CP side to set MPMU_CWRCRM can really
 * enable it.
 */
static u32 apbc_timer0;
static void pxa988_save_wakeup(void)
{
	if (readl_relaxed(regs_addr_get_va(REGS_ADDR_MPMU) + APCR)
			& DISABLE_ALL_WAKEUP_PORTS) {
		pr_err("NOT All wakeup ports are enabled in APCR!\n");
		pr_err("APCR: 0x%x\n", readl_relaxed(regs_addr_get_va(REGS_ADDR_MPMU)));
		BUG();
	}

	s_awucrm = readl_relaxed(regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);

	if (cpu_is_pxa1U88()) {
		apbc_timer0 = readl_relaxed(regs_addr_get_va(REGS_ADDR_APBC) + TIMER0);
		__raw_writel(0x1 << 7 | apbc_timer0,
			regs_addr_get_va(REGS_ADDR_APBC) + TIMER0);
	}
	writel_relaxed(s_awucrm | ENABLE_AP_WAKEUP_SOURCES,
			regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
}

static void pxa988_restore_wakeup(void)
{
	if (cpu_is_pxa1U88())
		writel_relaxed(apbc_timer0, regs_addr_get_va(REGS_ADDR_APBC) + TIMER0);
	writel_relaxed(s_awucrm, regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
}

static void pxa988_gic_global_mask(u32 cpu, u32 mask)
{
	u32 core_idle_cfg;

	core_idle_cfg = readl_relaxed(APMU_CORE_IDLE_CFG[cpu]);

	if (mask) {
		core_idle_cfg |= PMUA_GIC_IRQ_GLOBAL_MASK;
		core_idle_cfg |= PMUA_GIC_FIQ_GLOBAL_MASK;
	} else {
		core_idle_cfg &= ~(PMUA_GIC_IRQ_GLOBAL_MASK |
				PMUA_GIC_FIQ_GLOBAL_MASK);
	}
	writel_relaxed(core_idle_cfg, APMU_CORE_IDLE_CFG[cpu]);
}

static void pxa988_icu_global_mask(u32 cpu, u32 mask)
{
	u32 icu_msk;

	icu_msk = readl_relaxed(ICU_GBL_INT_MSK[cpu]);

	if (mask) {
		icu_msk |= ICU_MASK_FIQ;
		icu_msk |= ICU_MASK_IRQ;
	} else {
		icu_msk &= ~(ICU_MASK_FIQ | ICU_MASK_IRQ);
	}
	writel_relaxed(icu_msk, ICU_GBL_INT_MSK[cpu]);
}

static void pxa988_set_pmu(u32 cpu, u32 power_mode)
{
	pxa988_lowpower_config(cpu, power_mode, 1);

	/* Mask GIC global interrupt */
	pxa988_gic_global_mask(cpu, 1);
	/* Mask ICU global interrupt */
	pxa988_icu_global_mask(cpu, 1);
}

static void pxa988_clr_pmu(u32 cpu)
{
	/* Unmask GIC interrtup */
	pxa988_gic_global_mask(cpu, 0);
	/* Mask ICU global interrupt */
	pxa988_icu_global_mask(cpu, 1);

	pxa988_lowpower_config(cpu, -1, 0);
}

static struct platform_power_ops pxa988_power_ops = {
	.set_pmu	= pxa988_set_pmu,
	.clr_pmu	= pxa988_clr_pmu,
	.save_wakeup	= pxa988_save_wakeup,
	.restore_wakeup	= pxa988_restore_wakeup,
};

static struct platform_idle pxa988_idle = {
	.cpudown_state	= POWER_MODE_CORE_POWERDOWN,
	.wakeup_state	= POWER_MODE_SYS_SLEEP,
	.hotplug_state	= POWER_MODE_UDR,
	.l2_flush_state	= POWER_MODE_UDR,
	.ops		= &pxa988_power_ops,
	.states		= pxa988_modes,
	.state_count	= ARRAY_SIZE(pxa988_modes),
};

static void __init pxa988_reg_init(void)
{
	APMU_CORE_IDLE_CFG[0] = regs_addr_get_va(REGS_ADDR_APMU) + CORE0_IDLE;
	APMU_CORE_IDLE_CFG[1] = regs_addr_get_va(REGS_ADDR_APMU) + CORE1_IDLE;
	APMU_CORE_IDLE_CFG[2] = regs_addr_get_va(REGS_ADDR_APMU) + CORE2_IDLE;
	APMU_CORE_IDLE_CFG[3] = regs_addr_get_va(REGS_ADDR_APMU) + CORE3_IDLE;

	APMU_MP_IDLE_CFG[0]   = regs_addr_get_va(REGS_ADDR_APMU) + MP_CFG0;
	APMU_MP_IDLE_CFG[1]   = regs_addr_get_va(REGS_ADDR_APMU) + MP_CFG1;
	APMU_MP_IDLE_CFG[2]   = regs_addr_get_va(REGS_ADDR_APMU) + MP_CFG2;
	APMU_MP_IDLE_CFG[3]   = regs_addr_get_va(REGS_ADDR_APMU) + MP_CFG3;

	ICU_GBL_INT_MSK[0] = regs_addr_get_va(REGS_ADDR_ICU) + CORE0_CA7_GLB_INT_MASK;
	ICU_GBL_INT_MSK[1] = regs_addr_get_va(REGS_ADDR_ICU) + CORE1_CA7_GLB_INT_MASK;
	ICU_GBL_INT_MSK[2] = regs_addr_get_va(REGS_ADDR_ICU) + CORE2_CA7_GLB_INT_MASK;
	ICU_GBL_INT_MSK[3] = regs_addr_get_va(REGS_ADDR_ICU) + CORE3_CA7_GLB_INT_MASK;
}

static int __init pxa988_lowpower_init(void)
{
	u32 apcr;

	pxa988_reg_init();
	mmp_platform_power_register(&pxa988_idle);
	/*
	 * set DTCMSD, BBSD, MSASLPEN.
	 * For Helan2, SPDTCMSD and LDMA_MASK also have to be set.
	 * For Helan2/HelanLTE, DSPSD bit is not used.
	 */
	apcr = readl_relaxed(regs_addr_get_va(REGS_ADDR_MPMU) + APCR);
	if (cpu_is_pxa1U88())
		apcr |= PMUM_DTCMSD | PMUM_BBSD | PMUM_MSASLPEN |
			PMUM_SPDTCMSD | PMUM_LDMA_MASK;
	else
		apcr |= PMUM_DTCMSD | PMUM_BBSD | PMUM_MSASLPEN;

	apcr &= ~(PMUM_STBYEN | DISABLE_ALL_WAKEUP_PORTS);
	writel_relaxed(apcr, regs_addr_get_va(REGS_ADDR_MPMU) + APCR);
	/* set the power stable timer as 10us */
	__raw_writel(0x28207, regs_addr_get_va(REGS_ADDR_APMU) + STBL_TIMER);

	return 0;
}
early_initcall(pxa988_lowpower_init);
