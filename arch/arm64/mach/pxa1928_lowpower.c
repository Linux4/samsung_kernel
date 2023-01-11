/*
 * linux/arch/arm/mach-mmp/pxa988_lowpower.c
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
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/of.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/mcpm.h>
#include <asm/mcpm_plat.h>

#include "regs-addr.h"
#include "pxa1928_lowpower.h"

#define LPM_DDR_REGTABLE_INDEX		30

/*
 * Currently apmu and mpmu didn't use device tree.
 * here just use ioremap to get apmu and mpmu virtual
 * base address.
 */
static void __iomem *apmu_virt_addr;
static void __iomem *mpmu_virt_addr;
static void __iomem *icu_virt_addr;
static void __iomem *ciu_virt_addr;

static void __iomem *APMU_CORE_PWRMODE[4];
static void __iomem *APMU_CORE_RSTCTRL[4];
static void __iomem *APMU_APPS_PWRMODE;
static void __iomem *APMU_WKUP_MASK;
static void __iomem *APMU_MC_CLK_RES_CTRL;
static void __iomem *ICU_GBL_IRQ_MSK[4];
static void __iomem *ICU_GBL_FIQ_MSK[4];

static struct cpuidle_state pxa1928_modes[] = {
	[0] = {
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
	[1] = {
		.exit_latency		= 350,
		.target_residency	= 700,
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
};

static int ax_apmu;
static int pxa1928_apmu_is_ax(void)
{
	struct device_node *np = of_find_node_by_name(NULL, "pxa1928_apmu_ver");
	const char *str = NULL;

	if (!np)
		return -ENODEV;

	if (!of_property_read_string(np, "version", &str)
			&& !strcmp(str, "ax"))
		return 1;

	return 0;
}

void pxa1928_enable_lpm_ddrdll_recalibration(void)
{
	int val;

	val = readl(APMU_MC_CLK_RES_CTRL);
	val &= ~(0xf << 15);
	val |= (LPM_DDR_REGTABLE_INDEX << 15);
	writel(val, APMU_MC_CLK_RES_CTRL);
}

/*
 * edge wakeup
 *
 * To enable edge wakeup:
 * 1. Set mfp reg edge detection bits;
 * 2. Enable mfp ICU interrupt, but disable gic interrupt;
 * 3. Enable corrosponding wakeup port;
 */
static void edge_wakeup_port_enable(void)
{
	u32 pad_wkup_mask;

	pad_wkup_mask = readl(APMU_WKUP_MASK);

	writel(pad_wkup_mask | PMUA_GPIO, APMU_WKUP_MASK);
}

static void edge_wakeup_port_disable(void)
{
	u32 pad_wkup_mask;

	pad_wkup_mask = readl(APMU_WKUP_MASK);

	writel(pad_wkup_mask & ~PMUA_GPIO, APMU_WKUP_MASK);
}

static int need_restore_pad_wakeup;
static void pxa1928_edge_wakeup_enable(void)
{
	/*
	 * enable edge wakeup
	 */
	edge_wakeup_mfp_enable();
	edge_wakeup_port_enable();
	need_restore_pad_wakeup = 1;
}

static void pxa1928_edge_wakeup_disable(void)
{
	if (!need_restore_pad_wakeup)
		return;
	edge_wakeup_port_disable();
	edge_wakeup_mfp_disable();
	need_restore_pad_wakeup = 0;
}

static void pxa1928_set_dstate(u32 cpu, u32 power_mode)
{
	u32 apcr, apps_pwrmode;
	u32 anagrp_ctrl;

	apps_pwrmode = readl(APMU_APPS_PWRMODE);
	apps_pwrmode &= ~(PMUA_APPS_PWRMODE_MSK);

	switch (power_mode) {
	case POWER_MODE_UDR:
		/* fall through */
	case POWER_MODE_UDR_VCTCXO:
	case POWER_MODE_APPS_SLEEP:
		/* Enable Audio current ref for D1 vcxo off */
		if (power_mode == POWER_MODE_APPS_SLEEP) {
			anagrp_ctrl = (CURR_REF_PU | readl(mpmu_virt_addr + ANAGRP_CTRL1));
			writel(anagrp_ctrl, mpmu_virt_addr + ANAGRP_CTRL1);
			anagrp_ctrl = readl(mpmu_virt_addr + ANAGRP_CTRL1);
		}

		apcr = PMUM_APCR_BASE |
			(readl(mpmu_virt_addr + APCR) & PMUM_APCR_MASK);
		apcr |= PMUM_VCTCXOSD;
		writel(apcr, mpmu_virt_addr + APCR);
		apcr = readl(mpmu_virt_addr + APCR);		/* RAW */

		apps_pwrmode |= PMUA_APPS_D1D2;
		pxa1928_enable_lpm_ddrdll_recalibration();
		break;

	case POWER_MODE_APPS_IDLE:
		apps_pwrmode |= PMUA_APPS_D0CG;
		break;
	default:
		WARN(1, "Invalid power state!\n");
	}

	writel(apps_pwrmode, APMU_APPS_PWRMODE);
	apps_pwrmode = readl(APMU_APPS_PWRMODE);	/* RAW for barrier */
	pxa1928_edge_wakeup_enable();
	return;
}

static void pxa1928_set_cstate(u32 cpu, u32 power_mode,
			       bool flush_l2c)
{
	u32 core_pwrmode, core_rstctrl;
	void __iomem *dist_base = regs_addr_get_va(REGS_ADDR_GIC_DIST);

	core_pwrmode = readl(APMU_CORE_PWRMODE[cpu]);

	switch (power_mode) {
	case POWER_MODE_MP_POWERDOWN:
		if (flush_l2c) {
			core_pwrmode |= PMUA_L2SR_FLUSHREQ_VT |
					PMUA_L2SR_PWROFF_VT;
		}

		core_pwrmode |= PMUA_APCORESS_MP2;
		/* fall through */

	case POWER_MODE_CORE_POWERDOWN:
		core_pwrmode |= PMUA_CPU_PWRMODE_C2 |
				PMUA_GIC_IRQ_GLOBAL_MASK |
				PMUA_GIC_FIQ_GLOBAL_MASK;
		break;

	case POWER_MODE_CORE_INTIDLE:
		break;
	default:
		WARN(1, "Invalid power state!\n");
	}

	if (dist_base && ax_apmu) {
		void __iomem *reg_pending_sgi = dist_base + GIC_DIST_PENDING_SET;
		/*
		 * if there's pending SGI interrupt, just leave this sw wakeup
		 * bit, the core will enter lpm, and then exit.
		 */
		if (!(readl_relaxed(reg_pending_sgi) & 0xFFFF)) {
			/*
			 * if there's no pending SGI interrupt, let's clear the
			 * sw wakeup bit first.
			 */
			core_rstctrl = readl(APMU_CORE_RSTCTRL[cpu]);
			core_rstctrl &= ~(PMUA_SW_WAKEUP);
			writel(core_rstctrl, APMU_CORE_RSTCTRL[cpu]);
			/*
			 * check pending SGI interrupt again, if it hit the
			 * above time window, let's set it again.
			 */
			if (readl_relaxed(reg_pending_sgi) & 0xFFFF) {
				core_rstctrl |= (PMUA_SW_WAKEUP);
				writel(core_rstctrl, APMU_CORE_RSTCTRL[cpu]);
			}
			core_rstctrl = readl(APMU_CORE_RSTCTRL[cpu]);
		}
	}

	writel(core_pwrmode, APMU_CORE_PWRMODE[cpu]);
	core_pwrmode = readl(APMU_CORE_PWRMODE[cpu]);	/* RAW for barrier */

	/* mask global ICU IRQ/FIQ */
	writel(ICU_MASK, ICU_GBL_IRQ_MSK[cpu]);
	writel(ICU_MASK, ICU_GBL_FIQ_MSK[cpu]);

	return;
}


static void pxa1928_clear_state(u32 cpu)
{
	u32 core_pwrmode, apcr, apps_pwrmode;
	u32 anagrp_ctrl;

	/* clean up core and core subsystem */
	core_pwrmode = readl(APMU_CORE_PWRMODE[cpu]);
	core_pwrmode &= ~(PMUA_CPU_PWRMODE_C2 |
				  PMUA_L2SR_FLUSHREQ_VT |
				  PMUA_L2SR_PWROFF_VT |
				  PMUA_APCORESS_MP2 |
				  PMUA_GIC_IRQ_GLOBAL_MASK |
				  PMUA_GIC_FIQ_GLOBAL_MASK);
	core_pwrmode |= PMUA_CLR_C2_STATUS;
	writel(core_pwrmode, APMU_CORE_PWRMODE[cpu]);
	core_pwrmode = readl(APMU_CORE_PWRMODE[cpu]);	/* RAW for barrier */

	/* clean up application subsystem */
	apps_pwrmode = readl(APMU_APPS_PWRMODE);
	apps_pwrmode |= (PMUA_APPS_CLR_D2_STATUS);
	apps_pwrmode &= ~(PMUA_APPS_PWRMODE_MSK);
	writel(apps_pwrmode, APMU_APPS_PWRMODE);
	apps_pwrmode = readl(APMU_APPS_PWRMODE);	/* RAW for barrier */

	/* clean up vcxo setting */
	apcr = PMUM_APCR_BASE |
		(readl(mpmu_virt_addr + APCR) & PMUM_APCR_MASK);
	writel(apcr, mpmu_virt_addr + APCR);
	apcr = readl(mpmu_virt_addr + APCR);		/* RAW for barrier */

	/* disable audio current ref */
	anagrp_ctrl = readl(mpmu_virt_addr + ANAGRP_CTRL1);
	anagrp_ctrl &= ~(CURR_REF_PU);
	writel(anagrp_ctrl, mpmu_virt_addr + ANAGRP_CTRL1);
	anagrp_ctrl = readl(mpmu_virt_addr + ANAGRP_CTRL1);

	/* disable the gpio edge for cpu active states */
	pxa1928_edge_wakeup_disable();
	return;
}

/*
 * low power config
 */
static void pxa1928_lowpower_config(u32 cpu, u32 power_mode, u32 vote_state,
				    u32 lowpower_enable)
{
	u32 c_state;
	bool flush_l2c = false;

	/* clean up register setting */
	if (!lowpower_enable) {
		pxa1928_clear_state(cpu);
		return;
	}

	if (power_mode >= POWER_MODE_APPS_IDLE) {
		pxa1928_set_dstate(cpu, power_mode);
		c_state = POWER_MODE_MP_POWERDOWN;
	} else
		c_state = power_mode;

	/*
	 * TODO: now only flush L2 cache for D2 state,
	 * later can optimize it as needed for D1P or D1 state.
	 */
	if (vote_state == POWER_MODE_UDR)
		flush_l2c = true;

	pxa1928_set_cstate(cpu, c_state, flush_l2c);
	return;
}

/* Here we don't enable CP wakeup sources since CP will enable them */
#define ENABLE_AP_WAKEUP_SOURCES	\
	(PMUA_CP_IPC | PMUA_PMIC | PMUA_AUDIO | PMUA_RTC_ALARM | PMUA_KEYPAD |\
	 PMUA_GPIO | PMUA_TIMER_3_1 | PMUA_TIMER_3_2 | PMUA_TIMER_3_3 | PMUA_SDH1)
static u32 s_wkup_mask;

/*
 * Enable AP wakeup sources and ports. To enalbe wakeup
 * ports, it needs both AP side to configure MPMU_APCR
 * and CP side to configure MPMU_CPCR to really enable
 * it. To enable wakeup sources, either AP side to set
 * MPMU_AWUCRM or CP side to set MPMU_CWRCRM can really
 * enable it.
 */
static void pxa1928_save_wakeup(void)
{
	s_wkup_mask = readl(APMU_WKUP_MASK);
	writel(s_wkup_mask | ENABLE_AP_WAKEUP_SOURCES, APMU_WKUP_MASK);
}

static void pxa1928_restore_wakeup(void)
{
	writel(s_wkup_mask, APMU_WKUP_MASK);
}

static void pxa1928_set_pmu(u32 cpu, u32 calc_state, u32 vote_state)
{
	pxa1928_lowpower_config(cpu, calc_state, vote_state, 1);
}

static void pxa1928_clr_pmu(u32 cpu)
{
	pxa1928_lowpower_config(cpu, 0, 0, 0);
}

static void pxa1928_release(u32 cpu)
{
	unsigned int reg;

	reg = CIU_APCORE_CFG_64BIT | CIU_APCORE_CFG_NMFI_EN |
	      CIU_APCORE_CFG_NIDEN | CIU_APCORE_CFG_SPNIDEN |
	      CIU_APCORE_CFG_SPIDEN;

	/* check has been released yet */
	if (readl(APMU_CORE_RSTCTRL[cpu]) & PMUA_CORE_RSTCTRL_CPU_NPORESET)
		return;

	switch (cpu) {
	case 0:
		writel(reg, ciu_virt_addr + CIU_APCORE0_CFG_CTL);
		break;
	case 1:
		writel(reg, ciu_virt_addr + CIU_APCORE1_CFG_CTL);
		break;
	case 2:
		writel(reg, ciu_virt_addr + CIU_APCORE2_CFG_CTL);
		break;
	case 3:
		writel(reg, ciu_virt_addr + CIU_APCORE3_CFG_CTL);
		break;
	}

	dsb();
	writel(PMUA_CORE_RSTCTRL_CPU_NRESET | PMUA_CORE_RSTCTRL_CPU_NPORESET,
		APMU_CORE_RSTCTRL[cpu]);
}

static struct platform_power_ops pxa1928_power_ops = {
	.release	= pxa1928_release,
	.set_pmu	= pxa1928_set_pmu,
	.clr_pmu	= pxa1928_clr_pmu,
	.save_wakeup	= pxa1928_save_wakeup,
	.restore_wakeup	= pxa1928_restore_wakeup,
};

static struct platform_idle pxa1928_idle = {
	.cpudown_state		= POWER_MODE_CORE_POWERDOWN,
	.clusterdown_state	= POWER_MODE_MP_POWERDOWN,
	.wakeup_state		= POWER_MODE_APPS_IDLE,
	.hotplug_state		= POWER_MODE_UDR,
	.l2_flush_state		= POWER_MODE_UDR,
	.ops			= &pxa1928_power_ops,
	.states			= pxa1928_modes,
	.state_count		= ARRAY_SIZE(pxa1928_modes),
};

static void __init pxa1928_pmu_mapping(void)
{
	apmu_virt_addr = regs_addr_get_va(REGS_ADDR_APMU);
	mpmu_virt_addr = regs_addr_get_va(REGS_ADDR_MPMU);
	ciu_virt_addr = regs_addr_get_va(REGS_ADDR_CIU);

	APMU_CORE_PWRMODE[0] = apmu_virt_addr + CORE0_PWRMODE;
	APMU_CORE_PWRMODE[1] = apmu_virt_addr + CORE1_PWRMODE;
	APMU_CORE_PWRMODE[2] = apmu_virt_addr + CORE2_PWRMODE;
	APMU_CORE_PWRMODE[3] = apmu_virt_addr + CORE3_PWRMODE;
	APMU_CORE_RSTCTRL[0] = apmu_virt_addr + CORE0_RSTCTRL;
	APMU_CORE_RSTCTRL[1] = apmu_virt_addr + CORE1_RSTCTRL;
	APMU_CORE_RSTCTRL[2] = apmu_virt_addr + CORE2_RSTCTRL;
	APMU_CORE_RSTCTRL[3] = apmu_virt_addr + CORE3_RSTCTRL;

	APMU_APPS_PWRMODE = apmu_virt_addr + APPS_PWRMODE;
	APMU_WKUP_MASK = apmu_virt_addr + WKUP_MASK;
	APMU_MC_CLK_RES_CTRL = apmu_virt_addr + MC_CLK_RES_CTRL;
}

static void __init pxa1928_icu_mapping(void)
{
	struct device_node *node;
	int i;

	node = of_find_compatible_node(NULL, NULL, "marvell,pxa1928-intc");
	BUG_ON(!node);
	icu_virt_addr = of_iomap(node, 0);
	BUG_ON(!icu_virt_addr);

	ICU_GBL_IRQ_MSK[0] = icu_virt_addr + ICU_CORE0_GLB_IRQ_MASK;
	ICU_GBL_IRQ_MSK[1] = icu_virt_addr + ICU_CORE1_GLB_IRQ_MASK;
	ICU_GBL_IRQ_MSK[2] = icu_virt_addr + ICU_CORE2_GLB_IRQ_MASK;
	ICU_GBL_IRQ_MSK[3] = icu_virt_addr + ICU_CORE3_GLB_IRQ_MASK;
	ICU_GBL_FIQ_MSK[0] = icu_virt_addr + ICU_CORE0_GLB_FIQ_MASK;
	ICU_GBL_FIQ_MSK[1] = icu_virt_addr + ICU_CORE1_GLB_FIQ_MASK;
	ICU_GBL_FIQ_MSK[2] = icu_virt_addr + ICU_CORE2_GLB_FIQ_MASK;
	ICU_GBL_FIQ_MSK[3] = icu_virt_addr + ICU_CORE3_GLB_FIQ_MASK;

	/* mask ICU global IRQ/FIQ at beginning */
	for (i = 0; i < 4; i++) {
		writel(ICU_MASK, ICU_GBL_IRQ_MSK[i]);
		writel(ICU_MASK, ICU_GBL_FIQ_MSK[i]);
	}
}

static DEFINE_SPINLOCK(pmua_sw_wakeup_lock);
void pxa1928_gic_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	unsigned int val = 0;
	unsigned long flags;
	int targ_cpu;

	gic_raise_softirq(mask, irq);

	spin_lock_irqsave(&pmua_sw_wakeup_lock, flags);
	for_each_cpu(targ_cpu, mask) {
		BUG_ON(targ_cpu >= CONFIG_NR_CPUS);
		val = readl(APMU_CORE_RSTCTRL[targ_cpu]);
		val |= PMUA_SW_WAKEUP;
		writel(val, APMU_CORE_RSTCTRL[targ_cpu]);
	}
	spin_unlock_irqrestore(&pmua_sw_wakeup_lock, flags);
}

#define	IRQ_EDGE_WAKEUP	75
static irqreturn_t pad_edge_handler(int irq, void *dev_id)
{
	return 0;
}


static int __init pxa1928_lowpower_init(void)
{
	int ret;

	ax_apmu = pxa1928_apmu_is_ax();
	if (ax_apmu < 0)
		return ax_apmu;

	pr_info("Initialize pxa1928 low power controller.\n");

	pxa1928_pmu_mapping();
	pxa1928_icu_mapping();

	mcpm_plat_power_register(&pxa1928_idle);

	/*
	* Temp to fix the CPU3 HW LOCKUP issue.
	*/
	if (ax_apmu)
		set_smp_cross_call(pxa1928_gic_raise_softirq);

	ret = request_irq(IRQ_EDGE_WAKEUP, pad_edge_handler,
			IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND, "pad_edge", NULL);
	if (ret) {
		pr_err("ret = %d, request irq of pad edge fail\n", ret);
		return ret;
	}

	return 0;
}
early_initcall(pxa1928_lowpower_init);
