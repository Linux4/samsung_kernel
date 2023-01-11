/*
 * linux/arch/arm64/mach/pxa1908_lowpower.c
 *
 * Author:	Fangsuo Wu <fswu@marvell.com>
 * Copyright:	(C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/cputype.h>
#include <linux/clk/dvfs-dvc.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/io.h>
#include <linux/of.h>
#include <asm/mcpm.h>
#include <asm/mcpm_plat.h>
#ifdef CONFIG_ARM
#include <asm/cpuidle.h>
#endif
#include "regs-addr.h"
#include "pxa1908_lowpower.h"

static void __iomem *apmu_virt_addr;
static void __iomem *apbc_virt_addr;
static void __iomem *mpmu_virt_addr;
static void __iomem *icu_virt_addr;
/*per cpu regs */
static void __iomem *APMU_CORE_IDLE_CFG[4];
static void __iomem *APMU_CORE_RSTCTRL[4];
static void __iomem *APMU_MP_IDLE_CFG[4];
static void __iomem *ICU_GBL_INT_MSK[4];

static struct cpuidle_state pxa1908_modes[] = {
	[0] = {
		.exit_latency		= 18,
		.target_residency	= 36,
		.flags			= CPUIDLE_FLAG_TIME_VALID,
		.name			= "C1",
		.desc			= "C1: Core internal clock gated",
#ifdef CONFIG_ARM64
		.enter			= cpuidle_simple_enter,
#else
		.enter			= arm_cpuidle_simple_enter,
#endif
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
		.exit_latency		= 450,
		.target_residency	= 900,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "MP2",
		.desc			= "MP2: Core subsystem power down",
	},
	[3] = {
		.exit_latency		= 500,
		.target_residency	= 1000,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D1p",
		.desc			= "D1p: AP idle state",
	},
	[4] = {
		.exit_latency		= 600,
		.target_residency	= 1200,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D1",
		.desc			= "D1: Chip idle state",
	},
	/*
	 * Below modes won't be used in cpuidle framework. They're only for
	 * suspend related states. We define them here because in ulc FM
	 * playback needs vctcxo on while suspend, thus it has to hold a
	 * D2 constraint.
	 */
	[5] = {
		.exit_latency		= MAX_LATENCY,
		.target_residency	= MAX_LATENCY,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D2_VCTCXO",
		.desc			= "D2_VCTCXO: UDR with VCTCXO ON",
	},
	[6] = {
		.exit_latency		= MAX_LATENCY,
		.target_residency	= MAX_LATENCY,
		.flags			= CPUIDLE_FLAG_TIME_VALID |
					  CPUIDLE_FLAG_TIMER_STOP,
		.name			= "D2",
		.desc			= "D2: UDR(VCTCXO and L2 OFF)",
	},
};

/* ========================= edge wakeup configuration======================= */
#define ICU_IRQ_ENABLE	((1 << 6) | (1 << 4) | (1 << 0))
#define EDGE_WAKEUP_ICU		50
static int need_restore_pad_wakeup;

static void edge_wakeup_port_enable(void)
{
	u32 pad_awucrm;

	if (readl_relaxed(mpmu_virt_addr + APCR) & PMUM_SLPWP2) {
		pr_err("Port2 is NOT enabled in APCR!\n");
		pr_err("APCR: 0x%x\n",
			readl_relaxed(mpmu_virt_addr + APCR));
		BUG();
	}

	pad_awucrm = readl_relaxed(mpmu_virt_addr + AWUCRM);
	writel_relaxed(pad_awucrm | PMUM_WAKEUP2, mpmu_virt_addr + AWUCRM);
}

static void edge_wakeup_port_disable(void)
{
	u32 pad_awucrm;

	pad_awucrm = readl_relaxed(mpmu_virt_addr + AWUCRM);
	writel_relaxed(pad_awucrm & ~PMUM_WAKEUP2, mpmu_virt_addr + AWUCRM);
}

static void edge_wakeup_icu_enable(void)
{
	writel_relaxed(ICU_IRQ_ENABLE, icu_virt_addr + (EDGE_WAKEUP_ICU << 2));
}

static void edge_wakeup_icu_disable(void)
{
	writel_relaxed(0, icu_virt_addr + (EDGE_WAKEUP_ICU << 2));
}

static void pxa1908_edge_wakeup_enable(void)
{
	edge_wakeup_mfp_enable();
	edge_wakeup_icu_enable();
	edge_wakeup_port_enable();
	need_restore_pad_wakeup = 1;
}

static void pxa1908_edge_wakeup_disable(void)
{
	if (!need_restore_pad_wakeup)
		return;

	edge_wakeup_port_disable();
	edge_wakeup_icu_disable();
	edge_wakeup_mfp_disable();
	need_restore_pad_wakeup = 0;
}

#define SDH_WAKEUP_ICU      94  /* icu interrupt num for sdh wakeup */
static void sdh_wakeup_icu_enable(void)
{
	writel_relaxed(ICU_IRQ_ENABLE, icu_virt_addr + (SDH_WAKEUP_ICU << 2));
}

static void sdh_wakeup_icu_disable(void)
{
	writel_relaxed(0, icu_virt_addr + (SDH_WAKEUP_ICU << 2));
}

/* ========================= Wakeup ports settings=========================== */
#define DISABLE_ALL_WAKEUP_PORTS		\
	(PMUM_SLPWP0 | PMUM_SLPWP1 | PMUM_SLPWP2 | PMUM_SLPWP3 |	\
	 PMUM_SLPWP4 | PMUM_SLPWP5 | PMUM_SLPWP6 | PMUM_SLPWP7)
/*
 * Note:
 * 1. CP wakeup ports are enabled by CP;
 * 2. AP1_TIMER1, AP1_TIMER2 and AP1_TIMER3 in ULC are used by security team and
 * it has no requirement to use it as wakeup source. So they are not enabled here.
 * (And we found if enabled D1 can't be entered. Security team is following it.)
 */
#define ENABLE_AP_WAKEUP_SOURCES	\
	(PMUM_AP_ASYNC_INT | PMUM_AP_FULL_IDLE | PMUM_SQU_SDH1 | PMUM_SDH_23 | \
	 PMUM_KEYPRESS | PMUM_WDT | PMUM_RTC_ALARM | PMUM_AP0_2_TIMER_1 | \
	 PMUM_AP0_2_TIMER_2 | PMUM_AP0_2_TIMER_3 | PMUM_WAKEUP7 | PMUM_WAKEUP6 | \
	 PMUM_WAKEUP5 | PMUM_WAKEUP4 | PMUM_WAKEUP3 | PMUM_WAKEUP2)
static u32 s_awucrm;
static u32 apbc_timer0;
/*
 * Enable AP wakeup sources and ports. To enalbe wakeup
 * ports, it needs both AP side to configure MPMU_APCR
 * and CP side to configure MPMU_CPCR to really enable
 * it. To enable wakeup sources, either AP side to set
 * MPMU_AWUCRM or CP side to set MPMU_CWRCRM can really
 * enable it.
 */
static void pxa1908_save_wakeup(void)
{
	if (readl_relaxed(mpmu_virt_addr + APCR)
			& DISABLE_ALL_WAKEUP_PORTS) {
		pr_err("NOT All wakeup ports are enabled in APCR!\n");
		pr_err("APCR: 0x%x\n", readl_relaxed(mpmu_virt_addr));
		BUG();
	}

	s_awucrm = readl_relaxed(mpmu_virt_addr + AWUCRM);

	if (cpu_is_pxa1908()) {
		apbc_timer0 = readl_relaxed(apbc_virt_addr + TIMER0);
		writel_relaxed(0x1 << 7 | apbc_timer0, apbc_virt_addr + TIMER0);
	}

	writel_relaxed(s_awucrm | ENABLE_AP_WAKEUP_SOURCES,
			mpmu_virt_addr + AWUCRM);
}

static void pxa1908_restore_wakeup(void)
{
	if (cpu_is_pxa1908())
		writel_relaxed(apbc_timer0, apbc_virt_addr + TIMER0);

	writel_relaxed(s_awucrm, mpmu_virt_addr + AWUCRM);
}

static void pxa1908_gic_global_mask(u32 cpu, u32 mask)
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

static void pxa1908_icu_global_mask(u32 cpu, u32 mask)
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

/* ========================= lpm configurations ============================ */
static void pxa1908_set_dstate(u32 cpu, u32 power_mode)
{
	u32 apcr = readl_relaxed(mpmu_virt_addr + APCR);

	/*
	 * Suppose that there should be no one touch the MPMU_APCR
	 * but only Last man entering the low power modes which are
	 * deeper than M2. Thus, add BUG check here.
	 */
	if (apcr & (PMUM_DDRCORSD | PMUM_APBSD | PMUM_AXISD |
				PMUM_VCTCXOSD | PMUM_STBYEN | PMUM_SLPEN)) {
		pr_err("APCR is set to 0x%X by the Non-lastman\n", apcr);
		BUG();
	}

	switch (power_mode) {
	case POWER_MODE_UDR:
		apcr |= PMUM_VCTCXOSD;
		/* fall through */
	case POWER_MODE_UDR_VCTCXO:
		apcr |= PMUM_STBYEN;
		/* fall through */
	case POWER_MODE_SYS_SLEEP:
		apcr |= PMUM_APBSD;
		apcr |= PMUM_SLPEN;
		apcr |= PMUM_DDRCORSD;
		/* fall through */
	case POWER_MODE_APPS_IDLE:
		apcr |= PMUM_AXISD;
		/*
		 * Normally edge wakeup should only take effect in D1 and
		 * deeper modes. But due to SD host hardware requirement,
		 * it has to be put in D1P mode.
		 */
		pxa1908_edge_wakeup_enable();
		sdh_wakeup_icu_enable();
		break;
	default:
		WARN(1, "Invalid D power state!\n");
	}

	writel_relaxed(apcr, mpmu_virt_addr + APCR);
	apcr = readl_relaxed(mpmu_virt_addr + APCR);	/* RAW for barrier */

	return;
}

static void pxa1908_set_cstate(u32 cpu, u32 power_mode, bool flush_l2c)
{
	u32 core_idle_cfg, mp_idle_cfg;

	core_idle_cfg = readl_relaxed(APMU_CORE_IDLE_CFG[cpu]);
	mp_idle_cfg = readl_relaxed(APMU_MP_IDLE_CFG[cpu]);

	switch (power_mode) {
	case POWER_MODE_MP_POWERDOWN:
		if (flush_l2c)
			mp_idle_cfg |= PMUA_MP_L2_SRAM_POWER_DOWN;
		mp_idle_cfg |= PMUA_MP_POWER_DOWN;
		/* fall through */
	case POWER_MODE_CORE_POWERDOWN:
		mp_idle_cfg |= PMUA_MP_IDLE;
		core_idle_cfg |= PMUA_CORE_POWER_DOWN;
		core_idle_cfg |= PMUA_CORE_IDLE;
		/* fall through */
	case POWER_MODE_CORE_INTIDLE:
		break;
	default:
		WARN(1, "Invalid C power state!\n");
	}

	writel_relaxed(core_idle_cfg, APMU_CORE_IDLE_CFG[cpu]);
	core_idle_cfg = readl_relaxed(APMU_CORE_IDLE_CFG[cpu]);	/* RAW for barrier */

	writel_relaxed(mp_idle_cfg, APMU_MP_IDLE_CFG[cpu]);
	mp_idle_cfg = readl_relaxed(APMU_MP_IDLE_CFG[cpu]);	/* RAW for barrier */

	return;
}

static void pxa1908_clear_state(u32 cpu)
{
	u32 core_idle_cfg, mp_idle_cfg, apcr;

	/* clean up core */
	core_idle_cfg = readl_relaxed(APMU_CORE_IDLE_CFG[cpu]);
	core_idle_cfg &= ~(PMUA_CORE_IDLE | PMUA_CORE_POWER_DOWN);
	writel_relaxed(core_idle_cfg, APMU_CORE_IDLE_CFG[cpu]);
	core_idle_cfg = readl_relaxed(APMU_CORE_IDLE_CFG[cpu]);	/* RAW for barrier */

	/* clean up core subsystem */
	mp_idle_cfg = readl_relaxed(APMU_MP_IDLE_CFG[cpu]);
	mp_idle_cfg &= ~(PMUA_MP_IDLE | PMUA_MP_POWER_DOWN |
			PMUA_MP_L2_SRAM_POWER_DOWN | PMUA_MP_MASK_CLK_OFF);
	writel_relaxed(mp_idle_cfg, APMU_MP_IDLE_CFG[cpu]);
	mp_idle_cfg = readl_relaxed(APMU_MP_IDLE_CFG[cpu]);	/* RAW for barrier */

	/* clean up ap subsystem*/
	apcr = readl_relaxed(mpmu_virt_addr + APCR);	/* RAW for barrier */
	apcr &= ~(PMUM_DDRCORSD | PMUM_APBSD | PMUM_AXISD |
			PMUM_VCTCXOSD | PMUM_STBYEN | PMUM_SLPEN);
	writel_relaxed(apcr, mpmu_virt_addr + APCR);
	apcr = readl_relaxed(mpmu_virt_addr + APCR);	/* RAW for barrier */

	sdh_wakeup_icu_disable();
	pxa1908_edge_wakeup_disable();

	return;
}

static void pxa1908_lowpower_config(u32 cpu, u32 power_mode, u32 vote_state,
				    u32 lowpower_enable)
{
	u32 c_state;
	bool flush_l2c = false;

	/* clean up register setting */
	if (!lowpower_enable) {
		pxa1908_clear_state(cpu);
		return;
	}

	if (power_mode >= POWER_MODE_APPS_IDLE) {
		pxa1908_set_dstate(cpu, power_mode);
		c_state = POWER_MODE_MP_POWERDOWN;
	} else
		c_state = power_mode;

	/* only flush L2 cache for D2 state now */
	if (vote_state == POWER_MODE_UDR)
		flush_l2c = true;

	pxa1908_set_cstate(cpu, c_state, flush_l2c);
	return;
}

static void pxa1908_set_pmu(u32 cpu, u32 calc_state, u32 vote_state)
{
	pxa1908_lowpower_config(cpu, calc_state, vote_state, 1);
	/*
	 * Mask GIC and ICU global interrupts before entering lpm
	 * since interrupt breaks lpm hardware state machine and
	 * leads to unexpected disaster.
	 */
	pxa1908_gic_global_mask(cpu, 1);
	pxa1908_icu_global_mask(cpu, 1);
}

static void pxa1908_clr_pmu(u32 cpu)
{
	/*
	 * Only GIC is used in normal cases. For ICU, after enters
	 * lpm it's auto enabled by hardware. So mask it here.
	 */
	pxa1908_gic_global_mask(cpu, 0);
	pxa1908_icu_global_mask(cpu, 1);

	pxa1908_lowpower_config(cpu, 0, 0, 0);
}

/* ============================= misc functions ============================= */
/*
 * This function is called from boot_secondary to bootup the secondary cpu.
 */
#define CC2_AP				(0x100)
#define CC2_AP_POR_RST(n)		(1 << ((n) * 3 + 16))
#define CC2_AP_SW_RST(n)		(1 << ((n) * 3 + 17))
#define CC2_AP_DBG_RST(n)		(1 << ((n) * 3 + 18))

static void pxa1908_release(u32 cpu)
{
	u32 reg = readl_relaxed(apmu_virt_addr + CC2_AP);

	BUG_ON(cpu == 0);

	if (reg & CC2_AP_POR_RST(cpu)) {
		reg &= ~(CC2_AP_POR_RST(cpu) | CC2_AP_SW_RST(cpu)
			| CC2_AP_DBG_RST(cpu));
		writel_relaxed(reg, apmu_virt_addr + CC2_AP);
	}
}

static struct platform_power_ops pxa1908_power_ops = {
	.release	= pxa1908_release,
	.set_pmu	= pxa1908_set_pmu,
	.clr_pmu	= pxa1908_clr_pmu,
	.save_wakeup	= pxa1908_save_wakeup,
	.restore_wakeup	= pxa1908_restore_wakeup,
};

static struct platform_idle pxa1908_idle = {
	.cpudown_state		= POWER_MODE_CORE_POWERDOWN,
	.clusterdown_state	= POWER_MODE_MP_POWERDOWN,
	.wakeup_state		= POWER_MODE_SYS_SLEEP,
	.hotplug_state		= POWER_MODE_UDR,
	.l2_flush_state		= POWER_MODE_UDR,
	.ops			= &pxa1908_power_ops,
	.states			= pxa1908_modes,
	.state_count		= ARRAY_SIZE(pxa1908_modes),
};

static void __init pxa1908_reg_init(void)
{
	apmu_virt_addr = regs_addr_get_va(REGS_ADDR_APMU);
	apbc_virt_addr = regs_addr_get_va(REGS_ADDR_APBC);
	mpmu_virt_addr = regs_addr_get_va(REGS_ADDR_MPMU);
	icu_virt_addr  = regs_addr_get_va(REGS_ADDR_ICU);

	APMU_CORE_IDLE_CFG[0] = apmu_virt_addr + CORE0_IDLE;
	APMU_CORE_IDLE_CFG[1] = apmu_virt_addr + CORE1_IDLE;
	APMU_CORE_IDLE_CFG[2] = apmu_virt_addr + CORE2_IDLE;
	APMU_CORE_IDLE_CFG[3] = apmu_virt_addr + CORE3_IDLE;

	APMU_CORE_RSTCTRL[0] = apmu_virt_addr + CORE0_RSTCTRL;
	APMU_CORE_RSTCTRL[1] = apmu_virt_addr + CORE1_RSTCTRL;
	APMU_CORE_RSTCTRL[2] = apmu_virt_addr + CORE2_RSTCTRL;
	APMU_CORE_RSTCTRL[3] = apmu_virt_addr + CORE3_RSTCTRL;

	APMU_MP_IDLE_CFG[0]   = apmu_virt_addr + MP_CFG0;
	APMU_MP_IDLE_CFG[1]   = apmu_virt_addr + MP_CFG1;
	APMU_MP_IDLE_CFG[2]   = apmu_virt_addr + MP_CFG2;
	APMU_MP_IDLE_CFG[3]   = apmu_virt_addr + MP_CFG3;

	ICU_GBL_INT_MSK[0] = icu_virt_addr  + CORE0_CA7_GLB_INT_MASK;
	ICU_GBL_INT_MSK[1] = icu_virt_addr  + CORE1_CA7_GLB_INT_MASK;
	ICU_GBL_INT_MSK[2] = icu_virt_addr  + CORE2_CA7_GLB_INT_MASK;
	ICU_GBL_INT_MSK[3] = icu_virt_addr  + CORE3_CA7_GLB_INT_MASK;
}

#define APMU_WAKEUP_CORE(n)		(1 << (n & 0x3))
void pxa1908_gic_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	unsigned int val = 0;
	int targ_cpu;

	gic_raise_softirq(mask, irq);

	/* Don't touch any reg when axi timeout occurs */
	if (keep_silent)
		return;
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

static int __init pxa1908_lowpower_init(void)
{
	u32 apcr;
	u32 sccr;
	u32 dvfc_debug;

	if (!of_machine_is_compatible("marvell,pxa1908"))
		return -ENODEV;

	pr_info("Initialize pxa1908 low power controller.\n");
	pxa1908_reg_init();

	mcpm_plat_power_register(&pxa1908_idle);
	set_smp_cross_call(pxa1908_gic_raise_softirq);
	/*
	 * set DTCMSD, BBSD, MSASLPEN.
	 * For Helan2, SPDTCMSD and LDMA_MASK also have to be set.
	 * For Helan2/HelanLTE, DSPSD bit is not used.
	 */
	apcr = readl_relaxed(mpmu_virt_addr + APCR);
	apcr |= PMUM_DTCMSD | PMUM_BBSD | PMUM_MSASLPEN;
	apcr &= ~(PMUM_STBYEN | DISABLE_ALL_WAKEUP_PORTS);
	writel_relaxed(apcr, mpmu_virt_addr + APCR);


	dvfc_debug = readl_relaxed(apmu_virt_addr + DVC_DFC_DEBUG);
	dvfc_debug |= AP_DVC_EN_FOR_FAST_WAKEUP;
	writel_relaxed(dvfc_debug, apmu_virt_addr + DVC_DFC_DEBUG);

	/* TODO: Is 10us still valid for CA53? */
	/* set the power stable timer as 10us */
	writel_relaxed(0x2c307, apmu_virt_addr + STBL_TIMER);
	/*
	 * NOTE: bit0 and bit2 should always be set in SCCR.
	 * 1. bit2: Force the modem into sleep mode. system can't
	 * enter d1 in case it's not set;
	 * 2. bit0: Select 32k clk source.
	 *      0: 32k clk comes from 26M VCTCXO division;
	 *      1: 32k clk comes from PMIC
	 * If not set, System can't be woken up when vctcxo is shut
	 * down.
	 */
	sccr = readl_relaxed(mpmu_virt_addr + SCCR);
	writel_relaxed(sccr | 0x5, mpmu_virt_addr + SCCR);

	return 0;
}
early_initcall(pxa1908_lowpower_init);
