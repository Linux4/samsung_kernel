/*
 * linux/arch/arm/mach-mmp/pxa988_lowpower.c
 *
 * Author:	Raul Xiong <xjian@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/spinlock.h>
#include <linux/clockchips.h>
#include <linux/pm_qos.h>
#include <asm/mach/map.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/cp15.h>
#include <asm/smp_scu.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/pxa988_lowpower.h>
#include <mach/clock-pxa988.h>
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>
#include <mach/regs-icu.h>
#include <mach/regs-mcu.h>
#include <mach/cputype.h>
#include <mach/features.h>
#include <mach/scu.h>
#include <mach/reset-pxa988.h>
#include <mach/gpio-edge.h>
#include <plat/pxa_trace.h>
#include "common.h"

/*
 * The topology of the reserved data is as following.
 * Each core will use 4 bytes to save the flags.
 * The base address is pointed by pm_reserve_pa
 *
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |(PXA988/1088)
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |(PXA988/1088)
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |(PXA1088 only)
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |(PXA1088 only)
 * +--------------------------------------------------------+
 * |     scu power down flag                                |(PXA988 only)
 * +--------------------------------------------------------+
 * |     spin_lock                                          |(PXA988 only)
 * +--------------------------------------------------------+
 *
 * There are totally seven low power modes defined for PXA988
 * and PXA1088.
 * Please refer mach/pxa988_lowpower.h.
 *
 * 1. PXA988_LPM_C1: POWER_MODE_CORE_INTIDLE
 * 2. PXA988_LPM_C2: POWER_MODE_CORE_POWERDOWN with L1 shutdown, L2 retentive
 * 3. PXA988_LPM_D1P: POWER_MODE_APPS_IDLE with L2 retentive
 * 4. PXA988_LPM_D1: POWER_MODE_SYS_SLEEP with L2 retentive
 * 5. PXA988_LPM_D2: POWER_MODE_UDR_VCTCXO with L2 retentive
 * 6. PXA988_LPM_D2_UDR: POWER_MODE_UDR with L2 shutdown
 */

static char *coherent_buf;
static u32 num_cpus;
static u32 *enter_lpm_p;
static u32 *mp_restore;
static arch_spinlock_t *lpm_lock_p;
#ifdef CONFIG_ARM_ERRATA_802022
static volatile u32 *barrier;
#endif

static int (*finish_func)(unsigned long param);

#define IPCA_VIRT_BASE  (APB_VIRT_BASE + 0x1D000)

enum {
	CPU_SUSPEND_FROM_IDLE,
	CPU_SUSPEND_FROM_HOTPLUG,
	CPU_SUSPEND_FROM_SUSPEND,
};

static DEFINE_SPINLOCK(pmu_lock);
static unsigned long flags;

static const u32 APMU_CORE_IDLE_CFG[CONFIG_NR_CPUS] = {
	(u32)PMU_CORE0_IDLE_CFG, (u32)PMU_CORE1_IDLE_CFG,
#ifdef CONFIG_CPU_PXA1088
	(u32)PMU_CORE2_IDLE_CFG, (u32)PMU_CORE3_IDLE_CFG,
#endif
};
static const u32 APMU_MP_IDLE_CFG[CONFIG_NR_CPUS] = {
	(u32)PMU_MP_IDLE_CFG0, (u32)PMU_MP_IDLE_CFG1,
#ifdef CONFIG_CPU_PXA1088
	(u32)PMU_MP_IDLE_CFG2, (u32)PMU_MP_IDLE_CFG3,
#endif
};
static const u32 ICU_AP_GBL_INT_MSK[] = {
#if defined(CONFIG_CPU_PXA988)
	(u32)PXA988_ICU_A9C0_GBL_INT_MSK, (u32)PXA988_ICU_A9C1_GBL_INT_MSK,
#elif defined(CONFIG_CPU_PXA1088)
	(u32)PXA1088_ICU_APC0_GBL_INT_MSK, (u32)PXA1088_ICU_APC1_GBL_INT_MSK,
	(u32)PXA1088_ICU_APC2_GBL_INT_MSK, (u32)PXA1088_ICU_APC3_GBL_INT_MSK,
#endif
};

/*
 * To avoid multi-cores are accessing the same PMU register,
 * any functions MUST call pmu_register_lock before accessing
 * the PMU register and pmu_register_unlock after it.
 */
void pmu_register_lock()
{
	spin_lock_irqsave(&pmu_lock, flags);
}

void pmu_register_unlock()
{
	spin_unlock_irqrestore(&pmu_lock, flags);
}

static inline void core_exit_coherency(void)
{
	unsigned int v;
	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 1\n"
	"       bic     %0, %0, #(1 << 6)\n"
	"       mcr     p15, 0, %0, c1, c0, 1\n"
	: "=&r" (v) : : "cc");
	isb();
}

static inline void disable_l1_dcache(void)
{
	unsigned int v;
	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 0\n"
	"       bic     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 0\n"
	: "=&r" (v) : "Ir" (CR_C) : "cc");
	isb();
}

static inline void core_enter_coherency(void)
{
	unsigned int v;
	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 1\n"
	"       orr     %0, %0, #(1 << 6)\n"
	"       mcr     p15, 0, %0, c1, c0, 1\n"
	: "=&r" (v) : : "cc");
	isb();
}

static inline void enable_l1_dcache(void)
{
	unsigned int v;
	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 0\n"
	"       orr     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 0\n"
	: "=&r" (v) : "Ir" (CR_C) : "cc");
	isb();
}

static inline void ca7_pwr_down(void)
{
	unsigned int v;
	asm volatile(
	/* Disable L1 Cache */
	"       mrc     p15, 0, %0, c1, c0, 0\n"
	"       bic     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 0\n"
#ifdef CONFIG_ARM_ERRATA_794322
	"	ldr	%0, =_stext\n"
	"       mcr     p15, 0, %0, c8, c7, 1\n"
	"	dsb\n"
#endif
	/* L1 data Cache clean and invalidate */
	"	bl	v7_flush_dcache_louis\n"
	"	clrex\n"
	/* Core Exit SMP*/
	"       mrc     p15, 0, %0, c1, c0, 1\n"
	"       bic	%0, %0, #(1 << 6)\n"
	"       mcr     p15, 0, %0, c1, c0, 1\n"
	"	isb\n"
	"	b	cpu_v7_do_idle\n"
	: "=&r" (v) : "Ir" (CR_C) : "cc");
}

static inline void ca7_pwr_down_udr(void)
{
	unsigned int v;
	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 0\n"
	"       bic     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 0\n"
#ifdef CONFIG_ARM_ERRATA_794322
	"	ldr	%0, =_stext\n"
	"       mcr     p15, 0, %0, c8, c7, 1\n"
	"	dsb\n"
#endif
	/* L1 data Cache clean and invalidate */
	"	bl	v7_flush_dcache_louis\n"
	"	clrex\n"
	/* L1&L2 Cache clean and invalidate */
	"	bl	v7_flush_kern_cache_all\n"
	/* Core Exit SMP*/
	"       mrc     p15, 0, %0, c1, c0, 1\n"
	"       bic	%0, %0, #(1 << 6)\n"
	"       mcr     p15, 0, %0, c1, c0, 1\n"
	"	isb\n"
	"	b	cpu_v7_do_idle\n"
	: "=&r" (v) : "Ir" (CR_C) : "cc");
}

static int pxa1088_finish_suspend(unsigned long param)
{
	u32 icdispr = readl_relaxed(GIC_DIST_VIRT_BASE + GIC_DIST_PENDING_SET);
	if (icdispr)  {
		if (param == CPU_SUSPEND_FROM_HOTPLUG) {
			/* This is used to enable serial log output */
			set_cpu_online(smp_processor_id(), true);
			panic("Hotplug procedure got Pending PPI IRQ(0x%X)\n", icdispr);
		} else if (param == CPU_SUSPEND_FROM_IDLE)
			goto idle_fall_back;
	}

	if (unlikely(param == CPU_SUSPEND_FROM_SUSPEND))
		ca7_pwr_down_udr();
	else
		ca7_pwr_down();

idle_fall_back:

	core_enter_coherency();

	enable_l1_dcache();

	/* clear percpu flag in this path */
	enter_lpm_p[smp_processor_id()] = 0;

	return INVALID_LPM;
}
static int pxa988_finish_suspend(unsigned long param)
{
	u32 icdispr;

	/* clean & invalidate dcache cache, it contains dsb & isb */
	flush_cache_all();

	/*
	 * Clear the SCTLR.C bit to prevent further data cache
	 * allocation. Clearing SCTLR.C would make all the data accesses
	 * strongly ordered and would not hit the cache.
	 */
	disable_l1_dcache();
	/* Clear ACTLR.SMP bit */
	core_exit_coherency();

#ifdef CONFIG_HAVE_ARM_SCU
	/*
	 * Switch the CPU from Symmetric Multiprocessing (SMP) mode
	 * to AsymmetricMultiprocessing (AMP) mode by programming
	 * the SCU power status to DORMANT or OFF mode.
	 * This enables the CPU to be taken out of coherency by
	 * preventing the CPU from receiving cache, TLB, or BTB
	 * maintenance operations broadcast by other CPUs in
	 * the cluster.
	 * NOTE:
	 * This must be done after cache is flushed.
	 */
	scu_power_mode(pxa_scu_base_addr(), SCU_PM_POWEROFF);
#endif

	/* For suspend case we power down L2 sram
	 * so need to flush L2 here
	 */
	if (unlikely(param == CPU_SUSPEND_FROM_SUSPEND)) {
#ifdef CONFIG_CACHE_L2X0
		pl310_disable();
#endif
	}
	/*
	 * FIXME: There is risk that Dragon will enter M2 even there is an
	 * interrupt pending. SW need to check it before issue wfi,
	 * if yes, just jump out.
	 * It will be fixed in Z3 and further, we won't need it then.
	 */
	icdispr = readl_relaxed(GIC_DIST_VIRT_BASE + GIC_DIST_PENDING_SET);
	if (icdispr) {
		/*
		 * FIXME: There may be pending local timer interrupt on hotplug
		 * out core, this cause hotplug failed since we will jump out.
		 * Clear the interrupt for Hotplug scenario.
		 */
		if (unlikely(param == CPU_SUSPEND_FROM_HOTPLUG))
			writel_relaxed(icdispr, GIC_DIST_VIRT_BASE
					+ GIC_DIST_PENDING_CLEAR);
		else
			goto back;
	}

	cpu_do_idle();

back:

#ifdef CONFIG_HAVE_ARM_SCU
	/*
	 * Ensure the CPU power state is set to NORMAL in
	 * SCU power state so that CPU is back in coherency.
	 * In non-coherent mode CPU can lock-up and lead to
	 * system deadlock.
	 */
	scu_power_mode(pxa_scu_base_addr(), SCU_PM_NORMAL);
#endif
	core_enter_coherency();

	enable_l1_dcache();

	/* clear percpu flag in this path */
	enter_lpm_p[smp_processor_id()] = 0;

	return INVALID_LPM;
}

static void pxa988_lowpower_config(u32 cpu,
			u32 power_state, u32 lowpower_enable)
{
	u32 core_idle_cfg, mp_idle_cfg, apcr;
	u32 mc_slp_type = 0;
	u8 apmu_debug_byte1 = 0;
	u8 apmu_debug_byte2 = 0;

	pmu_register_lock();
	core_idle_cfg = __raw_readl(APMU_CORE_IDLE_CFG[cpu]);
	mp_idle_cfg = __raw_readl(APMU_MP_IDLE_CFG[cpu]);
	apcr = __raw_readl(MPMU_APCR);
	if (has_feat_debugreg_in_d1p() && !has_feat_d1p_hipwr()) {
		mc_slp_type = __raw_readl(APMU_MC_HW_SLP_TYPE);
		apmu_debug_byte1 = __raw_readb(APMU_DEBUG + 1);
		apmu_debug_byte2 = __raw_readb(APMU_DEBUG + 2);
	}

	if (lowpower_enable) {
		switch (power_state) {
		case POWER_MODE_UDR:
			mp_idle_cfg |= PMUA_MP_L2_SRAM_POWER_DOWN;
			/*
			 * FIXME:
			 * Workaround for EMEI-166:
			 * CA9 SCU ram and L2 sram power switch control signals
			 * are mistakenly cross-connected.
			 *
			 * The workaround keeps SCU/L2 retention state as the
			 * same on Zx.
			 * This is supposed to be fixed on A0.
			 */
			if (has_feat_scu_l2_power_connection())
				mp_idle_cfg |= PMUA_MP_SCU_SRAM_POWER_DOWN;
			apcr |= PMUM_VCTCXOSD;
			/* fall through */
		case POWER_MODE_UDR_VCTCXO:
			apcr |= PMUM_STBYEN;
			/* fall through */
		case POWER_MODE_SYS_SLEEP:
			apcr |= PMUM_APBSD;
			/* fall through */
		case POWER_MODE_APPS_SLEEP:
			apcr |= PMUM_SLPEN;
			/* For Z1/Z2 we vote AXISD and DDRCORSD here*/
			if (has_feat_d1p_hipwr()) {
				apcr |= PMUM_AXISD;
				apcr |= PMUM_DDRCORSD;
			}
			/* For A0 or newer stepping we vote DDRCORSD here */
			if (!has_feat_debugreg_in_d1p())
				apcr |= PMUM_DDRCORSD;
			/* fall through */
		case POWER_MODE_APPS_IDLE:
			/* For Z3/A0 or newer stepping we vote AXISD here */
			if (!has_feat_d1p_hipwr())
				apcr |= PMUM_AXISD;
			/* For Z3 we vote DDRCORSD here */
			if (has_feat_debugreg_in_d1p() && !has_feat_d1p_hipwr())
				apcr |= PMUM_DDRCORSD;
			/* enable gpio edge for the modes need wakeup source */
			mmp_gpio_edge_enable();
			/* fall through */
		case POWER_MODE_CORE_POWERDOWN:
			core_idle_cfg |= PMUA_CORE_POWER_DOWN;
			if (cpu_is_pxa988())
				core_idle_cfg |= PMUA_CORE_L1_SRAM_POWER_DOWN;
			mp_idle_cfg |= PMUA_MP_POWER_DOWN;
			if (!has_feat_scu_l2_power_connection())
				mp_idle_cfg |= PMUA_MP_SCU_SRAM_POWER_DOWN;
			/*
			 * FIXME: This is a temporary workaround for D1/D2 hang
			 * issue. DE confirmed that D1/D2 may hang due to
			 * missing mpsub_idle_clk_off_ack.
			 * Mask MP clock off State check can be a SW workaround.
			 * Will remove this workaround when the issue is fixed.
			 * Index: EMEI-145
			 */
			mp_idle_cfg |= PMUA_MP_MASK_CLK_OFF;
			/* fall through */
		case POWER_MODE_CORE_EXTIDLE:
			core_idle_cfg |= PMUA_CORE_IDLE;
			mp_idle_cfg |= PMUA_MP_IDLE;
			/* fall through */
		case POWER_MODE_CORE_INTIDLE:
			break;
		default:
			WARN(1, "Invalid power state!\n");
		}

		/*
		 * For Z3 we need the D1P workaround.
		 * Z1/Z2 don't use D1P because of EMEI-101 issue
		 */
		if (has_feat_debugreg_in_d1p() && !has_feat_d1p_hipwr() &&
				power_state == POWER_MODE_APPS_IDLE) {
			mc_slp_type &= ~0x7;
			mc_slp_type |= 0x4;
			apmu_debug_byte1 |= (1 << 6);
			apmu_debug_byte2 |= (1 << 7);
		}
	} else {
		core_idle_cfg &= ~(PMUA_CORE_IDLE | PMUA_CORE_POWER_DOWN);
		mp_idle_cfg &= ~(PMUA_MP_IDLE | PMUA_MP_POWER_DOWN |
				PMUA_MP_L2_SRAM_POWER_DOWN |
				PMUA_MP_MASK_CLK_OFF);
		if (cpu_is_pxa988()) {
			core_idle_cfg &= ~(PMUA_CORE_L1_SRAM_POWER_DOWN);
			mp_idle_cfg &= ~(PMUA_MP_SCU_SRAM_POWER_DOWN);
		}
		apcr &= ~(PMUM_DDRCORSD | PMUM_APBSD | PMUM_AXISD |
			PMUM_VCTCXOSD | PMUM_STBYEN | PMUM_SLPEN);

		if (has_feat_debugreg_in_d1p() && !has_feat_d1p_hipwr()) {
			mc_slp_type &= ~0x7;
			apmu_debug_byte1 &= ~(1 << 6);
			apmu_debug_byte2 &= ~(1 << 7);
		}
		/* disable the gpio edge for cpu active states */
		mmp_gpio_edge_disable();
	}

	__raw_writel(core_idle_cfg, APMU_CORE_IDLE_CFG[cpu]);
	__raw_writel(mp_idle_cfg, APMU_MP_IDLE_CFG[cpu]);
	__raw_writel(apcr, MPMU_APCR);
	if (has_feat_debugreg_in_d1p() && !has_feat_d1p_hipwr()) {
		__raw_writel(mc_slp_type, APMU_MC_HW_SLP_TYPE);
		__raw_writeb(apmu_debug_byte1, APMU_DEBUG + 1);
		__raw_writeb(apmu_debug_byte2, APMU_DEBUG + 2);
	}
	pmu_register_unlock();
}

#define DISABLE_ALL_WAKEUP_PORTS		\
	(PMUM_SLPWP0 | PMUM_SLPWP1 | PMUM_SLPWP2 | PMUM_SLPWP3 |	\
	 PMUM_SLPWP4 | PMUM_SLPWP5 | PMUM_SLPWP6 | PMUM_SLPWP7)
/* Here we don't enable CP wakeup sources since CP will enable them */
#define ENABLE_AP_WAKEUP_SOURCES	\
	(PMUM_AP_ASYNC_INT | PMUM_AP_FULL_IDLE | PMUM_SQU_SDH1 | PMUM_SDH_23 |\
	 PMUM_KEYPRESS | PMUM_WDT | PMUM_RTC_ALARM | PMUM_AP1_TIMER_1 |	\
	 PMUM_AP1_TIMER_2 | PMUM_WAKEUP7 | PMUM_WAKEUP6 | PMUM_WAKEUP5 |\
	 PMUM_AP2_TIMER_1 | PMUM_AP2_TIMER_2 |\
	 PMUM_WAKEUP4 | PMUM_WAKEUP3 | PMUM_WAKEUP2)
static u32 s_apcr, s_awucrm, s_wake_saved;
/*
 * Enable AP wakeup sources and ports. To enalbe wakeup
 * ports, it needs both AP side to configure MPMU_APCR
 * and CP side to configure MPMU_CPCR to really enable
 * it. To enable wakeup sources, either AP side to set
 * MPMU_AWUCRM or CP side to set MPMU_CWRCRM can really
 * enable it.
 */
static void enable_ap_wakeup_sources(void)
{
	pmu_register_lock();
	s_awucrm = __raw_readl(MPMU_AWUCRM);
	s_apcr = __raw_readl(MPMU_APCR);
	__raw_writel(s_awucrm | ENABLE_AP_WAKEUP_SOURCES, MPMU_AWUCRM);
	__raw_writel(s_apcr & ~DISABLE_ALL_WAKEUP_PORTS, MPMU_APCR);
	pmu_register_unlock();
	s_wake_saved = 1;
}

static void restore_wakeup_sources(void)
{
	pmu_register_lock();
	__raw_writel(s_awucrm, MPMU_AWUCRM);
	__raw_writel(s_apcr, MPMU_APCR);
	pmu_register_unlock();
	s_wake_saved = 0;
}

static void pxa988_gic_global_mask(u32 cpu, u32 mask)
{
	u32 core_idle_cfg;

	core_idle_cfg = __raw_readl(APMU_CORE_IDLE_CFG[cpu]);

	if (mask) {
		core_idle_cfg |= PMUA_GIC_IRQ_GLOBAL_MASK;
		core_idle_cfg |= PMUA_GIC_FIQ_GLOBAL_MASK;
	} else {
		core_idle_cfg &= ~(PMUA_GIC_IRQ_GLOBAL_MASK |
					PMUA_GIC_FIQ_GLOBAL_MASK);
	}
	__raw_writel(core_idle_cfg, APMU_CORE_IDLE_CFG[cpu]);
}

static void pxa988_icu_global_mask(u32 cpu, u32 mask)
{
	u32 icu_msk;

	icu_msk = __raw_readl(ICU_AP_GBL_INT_MSK[cpu]);

	if (mask) {
		icu_msk |= ICU_MASK_FIQ;
		icu_msk |= ICU_MASK_IRQ;
	} else {
		icu_msk &= ~(ICU_MASK_FIQ | ICU_MASK_IRQ);
	}
	__raw_writel(icu_msk, ICU_AP_GBL_INT_MSK[cpu]);
}

/* These states are used as idle replacement as well as suspend/hotplug */
struct pxa988_lowpower_data pxa988_lpm_data[] = {
	[PXA988_LPM_C1] = {
		.power_state = POWER_MODE_CORE_INTIDLE,
		.valid = 1,
	},
	[PXA988_LPM_C2] = {
		.power_state = POWER_MODE_CORE_POWERDOWN,
		.valid = 1,
	},
	[PXA988_LPM_D1P] = {
		.power_state = POWER_MODE_APPS_IDLE,
		.valid = 1,
	},
	[PXA988_LPM_D1] = {
		.power_state = POWER_MODE_SYS_SLEEP,
		.valid = 1,
	},
	[PXA988_LPM_D2] = {
		.power_state = POWER_MODE_UDR_VCTCXO,
		.valid = 1,
	},
	[PXA988_LPM_D2_UDR] = {
		.power_state = POWER_MODE_UDR,
		.valid = 1,
	},
	/* must always be the last one! */
	[PXA988_MAX_LPM_INDEX] = {
		.power_state = -1,
		.valid = 0,
	},
};

#ifdef CONFIG_PM
void pxa988_enter_c1(u32 cpu)
{
	if (has_feat_ipc_wakeup_core()) {
		/*
		* Clear IPC GP_INT interrupt status in the ICU to de-assert
		* the wake up signal before enter lpm.
		*/
		__raw_writel(0x400, IPCA_VIRT_BASE + 0xC);
	}

	pxa988_lowpower_config(cpu,
			pxa988_lpm_data[PXA988_LPM_C1].power_state, 1);
	trace_pxa_cpu_idle(LPM_ENTRY(PXA988_LPM_C1), cpu);

	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_ENTER, PXA988_LPM_C1);
	cpu_do_idle();
	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_EXIT, PXA988_MAX_LPM_INDEX);

	trace_pxa_cpu_idle(LPM_EXIT(PXA988_LPM_C1), cpu);
	pxa988_lowpower_config(cpu,
			pxa988_lpm_data[PXA988_LPM_C1].power_state, 0);
}

static void pxa988_pre_enter_lpm(u32 cpu, u32 power_mode)
{
	pxa988_lowpower_config(cpu,
			pxa988_lpm_data[power_mode].power_state, 1);

	/* Mask GIC global interrupt */
	pxa988_gic_global_mask(cpu, 1);
	/* Mask ICU global interrupt */
	pxa988_icu_global_mask(cpu, 1);

	if (has_feat_ipc_wakeup_core()) {
		/*
		* Clear the IPC GP_INT interrupt status in the ICU to de-assert
		* the wake up signal before enter lpm.
		*/
		__raw_writel(0x400, IPCA_VIRT_BASE + 0xC);
	}
}

static void pxa988_post_enter_lpm(u32 cpu, u32 power_mode)
{
	/* Unmask GIC interrtup */
	pxa988_gic_global_mask(cpu, 0);
	/*
	 * FIXME: Do we need to mask ICU before cpu_cluster_pm_exit
	 * to avoid GIC ID31 interrupt?
	 */
	/* Mask ICU global interrupt */
	pxa988_icu_global_mask(cpu, 1);

	pxa988_lowpower_config(cpu,
			pxa988_lpm_data[power_mode].power_state, 0);
}

static void pxa988_check_constraint(unsigned long constraint)
{
	switch (constraint) {
	case PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE:
		pxa988_lpm_data[PXA988_LPM_D1P].valid = 1;
		pxa988_lpm_data[PXA988_LPM_D1].valid = 1;
		pxa988_lpm_data[PXA988_LPM_D2].valid = 1;
		break;
	case PM_QOS_CPUIDLE_BLOCK_VCTCXO_VALUE:
		pxa988_lpm_data[PXA988_LPM_D1P].valid = 1;
		pxa988_lpm_data[PXA988_LPM_D1].valid = 1;
		pxa988_lpm_data[PXA988_LPM_D2].valid = 0;
		break;
	case PM_QOS_CPUIDLE_BLOCK_DDR_VALUE:
		pxa988_lpm_data[PXA988_LPM_D1P].valid = 1;
		pxa988_lpm_data[PXA988_LPM_D1].valid = 0;
		pxa988_lpm_data[PXA988_LPM_D2].valid = 0;
		break;
	case PM_QOS_CPUIDLE_BLOCK_AXI_VALUE:
		pxa988_lpm_data[PXA988_LPM_D1P].valid = 0;
		pxa988_lpm_data[PXA988_LPM_D1].valid = 0;
		pxa988_lpm_data[PXA988_LPM_D2].valid = 0;
		break;
	default:
		pr_err("cpuidle blocked by an unknown state!\n");
	}
}

/*
 * For Zx stepping, MCK4 table 0 is triggered when system exits
 * from D1P, which is unnecessary. Make a workaround to skip table
 * 0 if only entering LPM no deeper than D1P and restore table 0
 * if going to D1 or deeper LPM.
 */
#define DLL_RST_SKIP	(0x2000C)
#define DLL_RST_RESTORE	(0xC)
static void ddr_dll_rst_wkr(int skip)
{
	int dll_rst = 0;

	if (has_feat_mck4_table0_trigger()) {
		if (skip) {
			/* skip DLL reset in MCK4 table 0 */
			dll_rst = DLL_RST_SKIP;
			trace_pxa_ddr_lpm(dll_rst);
			__raw_writel(0x0, DMCU_VIRT_REG(DMCU_HWTDAT0));
			__raw_writel(dll_rst, DMCU_VIRT_REG(DMCU_HWTDAT1));
			__raw_writel(0x0, DMCU_VIRT_REG(DMCU_HWTCTRL));
		} else {
			/* restore DLL reset in MCK4 table 0 */
			dll_rst = DLL_RST_RESTORE;
			trace_pxa_ddr_lpm(dll_rst);
			__raw_writel(0x0, DMCU_VIRT_REG(DMCU_HWTDAT0));
			__raw_writel(dll_rst, DMCU_VIRT_REG(DMCU_HWTDAT1));
			__raw_writel(0x0, DMCU_VIRT_REG(DMCU_HWTCTRL));
		}
	}
}

/*
 * pxa988_enter_lowpower - the entry function of pxa988 low power mode
 *
 * Here we are assuming there are maximum 16 low power modes,
 * and the first LPM is C1, the second LPM is C2 (cpu power down)
 * The following LPMs are D-stauts.
 *
 * @cpu: the cpu id of the cpu that calls this function.
 * @power_mode: then low power mode it will enter
 *
 */

#ifdef CONFIG_ARCH_NEEDS_CPU_IDLE_COUPLED
int lpm_index;
int pxa988_enter_lowpower(u32 cpu, u32 power_mode)
{
	int i;
	int cpus_enter_lpm = 0xffffffff;
#ifdef CONFIG_LOCAL_TIMERS
	int cpu_id = cpu;
#endif
	/* if uses coupled idle*/
	u32 icdispr = 0;
	/* The default power_mode should be C2 */
	lpm_index = PXA988_LPM_C2;
	if (has_feat_freqchg_disable_c2()) {
		if (atomic_read(&freqchg_disable_c2))
			lpm_index = PXA988_LPM_C1;
	}

#ifdef CONFIG_EOF_FC_WORKAROUND
	if (atomic_read(&disable_c2))
		lpm_index = PXA988_LPM_C1;
#endif

#ifdef CONFIG_LOCAL_TIMERS
	/* switch to broadcast timer before enter lpm */
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu_id);
#endif

	/*
	 * If there's a pending IPI, exit low power function.
	 * Otherwise, since the CORE1_IDLE_FLAG in PMUA(0x90)
	 * is set before hardware checking the interrupt, CPU0
	 * has a chance to exit waiting loop. But at that time,
	 * CPU1 exits C2, which is not expected.
	 */
	icdispr = readl_relaxed(GIC_DIST_VIRT_BASE + GIC_DIST_PENDING_SET);
	if (icdispr)
		lpm_index = INVALID_LPM;
	/*FIXME
	 * Here the return state is not very accurate.
	 * When Core1 enters C2 and Core0 excutes here,
	 * Core1 would be woke up but it also return
	 * C1 although it exits from C2.
	 */
	if ((lpm_index == PXA988_LPM_C1) || (lpm_index == INVALID_LPM)) {
#ifdef CONFIG_LOCAL_TIMERS
		/* switch back to normal timer */
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu_id);
#endif
		return lpm_index;
	}

	cpu_pm_enter();

	/* mask the LPM states we can enter */
	enter_lpm_p[cpu] |= (1 << (power_mode + 1)) - 1;
	/*
	 * CPU0 is always the last core to enter.
	 */
	if (cpu == 0) {
		for (i = 0; i < num_cpus; i++)
			cpus_enter_lpm &= enter_lpm_p[i];
#ifdef CONFIG_CACHE_L2X0
		pl310_suspend();
#endif
		cpu_cluster_pm_enter();
		/*
		 * Here we assume when one LPM state is disabled,
		 * all shallower states are disabled
		 */
		lpm_index = find_first_zero_bit((void *)&cpus_enter_lpm,
						PXA988_MAX_LPM_INDEX) - 1;
		if (lpm_index > PXA988_LPM_C2)
			pxa988_check_constraint(pm_qos_request(PM_QOS_CPUIDLE_BLOCK));

		/* check LPM constraints  */
		while (pxa988_lpm_data[lpm_index].valid != 1)
			lpm_index--;
		pxa988_pre_enter_lpm(cpu, lpm_index);
		/* For D1 or deeper LPM, we need to enable wakeup sources */
		if (lpm_index >= PXA988_LPM_D1) {
			ddr_dll_rst_wkr(0);
			enable_ap_wakeup_sources();
		}
#ifdef CONFIG_DEBUG_FS
		pxa988_cpu_dcstat_event(cpu, CPU_M2_OR_DEEPER_ENTER, lpm_index);
#endif
	} else
		/* Other cores enter C2 */
		pxa988_pre_enter_lpm(cpu, PXA988_LPM_C2);

	trace_pxa_cpu_idle(LPM_ENTRY(lpm_index), cpu);
#ifdef CONFIG_DEBUG_FS
	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_ENTER, lpm_index);
#endif
	if (INVALID_LPM \
		== cpu_suspend(CPU_SUSPEND_FROM_IDLE, finish_func))
		lpm_index = INVALID_LPM;
#ifdef CONFIG_DEBUG_FS
	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_EXIT, PXA988_MAX_LPM_INDEX);
#endif
	trace_pxa_cpu_idle(LPM_EXIT(lpm_index), cpu);

	/* here we exit from LPM */

	if (s_wake_saved == 1) {
		ddr_dll_rst_wkr(1);
		restore_wakeup_sources();
	}

#ifdef CONFIG_CPU_CA7MP
	*mp_restore = !((readl_relaxed(GIC_DIST_VIRT_BASE + GIC_DIST_CTRL)) & (0x1));
#endif
	if (*mp_restore) {
		*mp_restore = 0;
		cpu_cluster_pm_exit();
	}

#ifdef CONFIG_LOCAL_TIMERS
	/* switch back to normal timer after back from lpm */
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu_id);
#endif
	pxa988_post_enter_lpm(cpu, power_mode);

	cpu_pm_exit();

	return lpm_index;
}
#else
int pxa988_enter_lowpower(u32 cpu, u32 power_mode)
{
	int i;
	int cpus_enter_lpm = 0xffffffff;
	int mp_shutdown = 1;
	/* The default power_mode should be C2 */
	int lpm_index = PXA988_LPM_C2;
	u32 apcr;

#ifdef CONFIG_LOCAL_TIMERS
	int cpu_id = cpu;
#endif

	if (has_feat_freqchg_disable_c2()) {
		if (atomic_read(&freqchg_disable_c2))
			power_mode = PXA988_LPM_C1;
	}

#ifdef CONFIG_EOF_FC_WORKAROUND
	if (atomic_read(&disable_c2))
		power_mode = PXA988_LPM_C1;
#endif

	if (power_mode == PXA988_LPM_C1) {
		/* For C1 the core won't be reset */
		pxa988_enter_c1(cpu);
		return 0;
	}

#ifdef CONFIG_LOCAL_TIMERS
	/* switch to broadcast timer before enter lpm */
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu_id);
#endif
	cpu_pm_enter();

	arch_spin_lock(lpm_lock_p);

	/* Suppose that there should be no one touch the MPMU_APCR
	 * but only Last man entering the low power modes which are
	 * deeper than M2. Thus, add BUG check here.
	 */
	apcr = __raw_readl(MPMU_APCR);
	if (apcr & (PMUM_DDRCORSD | PMUM_APBSD | PMUM_AXISD |
		PMUM_VCTCXOSD | PMUM_STBYEN | PMUM_SLPEN)) {
		pr_err("APCR is set to 0x%X by the Non-lastman\n", apcr);
		BUG_ON(1);
	}

	/* mask the LPM states we can enter */
	enter_lpm_p[cpu] = (1 << (power_mode + 1)) - 1;
	for (i = 0; i < num_cpus; i++)
		cpus_enter_lpm &= enter_lpm_p[i];
	mp_shutdown = test_bit(PXA988_LPM_C2, (void *)&cpus_enter_lpm);

	if (!mp_shutdown) {
		/* Enter C2 */
		pxa988_pre_enter_lpm(cpu, PXA988_LPM_C2);
	} else {
#ifdef CONFIG_CACHE_L2X0
		pl310_suspend();
#endif
		cpu_cluster_pm_enter();
		/*
		 * Here we assume when one LPM state is disabled,
		 * all shallower states are disabled
		 */
		lpm_index = find_first_zero_bit((void *)&cpus_enter_lpm,
						PXA988_MAX_LPM_INDEX) - 1;
		if (lpm_index > PXA988_LPM_C2)
			pxa988_check_constraint(pm_qos_request(PM_QOS_CPUIDLE_BLOCK));

		/* check LPM constraints  */
		while (pxa988_lpm_data[lpm_index].valid != 1)
			lpm_index--;

		pxa988_pre_enter_lpm(cpu, lpm_index);
		/* For D1 or deeper LPM, we need to enable wakeup sources */
		if (lpm_index >= PXA988_LPM_D1) {
			ddr_dll_rst_wkr(0);
			enable_ap_wakeup_sources();
		}

		pxa988_cpu_dcstat_event(cpu, CPU_M2_OR_DEEPER_ENTER, lpm_index);

#ifdef CONFIG_ARM_ERRATA_802022
		for_each_possible_cpu(i) {
			barrier[i] = 0;
		}
#endif
	}
	arch_spin_unlock(lpm_lock_p);

	trace_pxa_cpu_idle(LPM_ENTRY(lpm_index), cpu);

	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_ENTER, lpm_index);
	if (INVALID_LPM == \
		cpu_suspend(CPU_SUSPEND_FROM_IDLE, finish_func))
		lpm_index = INVALID_LPM;
	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_EXIT, PXA988_MAX_LPM_INDEX);

	trace_pxa_cpu_idle(LPM_EXIT(lpm_index), cpu);

	/* here we exit from LPM */
	arch_spin_lock(lpm_lock_p);

	if (s_wake_saved == 1) {
		ddr_dll_rst_wkr(1);
		restore_wakeup_sources();
	}

#ifdef CONFIG_CPU_CA7MP
	*mp_restore = !((readl_relaxed(GIC_DIST_VIRT_BASE + GIC_DIST_CTRL)) & (0x1));
#endif
	if (*mp_restore) {
		*mp_restore = 0;
		cpu_cluster_pm_exit();
	}

	arch_spin_unlock(lpm_lock_p);

#ifdef CONFIG_LOCAL_TIMERS
	/* switch back to normal timer after back from lpm */
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu_id);
#endif
	pxa988_post_enter_lpm(cpu, power_mode);

	cpu_pm_exit();

	return lpm_index;
}
#endif
#endif /* CONFIG_PM */

#ifdef CONFIG_HOTPLUG_CPU
/*
 * Allows POWER_MODE_UDR for CPU hotplug.
 * Actually the hotpluged CPU will enter C2 but it will allow
 * POWER_MODE_UDR since the hotpluged CPU should never blocks
 * other CPUs enter the deepest LPM.
 */
#ifdef CONFIG_ARCH_NEEDS_CPU_IDLE_COUPLED
void pxa988_hotplug_enter(u32 cpu, u32 power_mode)
{
	u32 mp_idle_cfg;

	pxa988_pre_enter_lpm(cpu, PXA988_LPM_C2);

	/*
	 * For CPU hotplug, we don't need cpu_suspend help functions
	 * but still need to mask LPM bits as the deepest LPM.
	 * We are assuming the hotplug CPU will NEVER be the last CPU
	 * enter C2 since in platform_cpu_kill we ensure that.
	 */
	pxa988_set_reset_handler(__pa(pxa988_hotplug_handler), cpu);

	/* The hotpluged CPU always allow SCU/L2 SRAM power down */
	mp_idle_cfg = __raw_readl(APMU_MP_IDLE_CFG[cpu]);
	mp_idle_cfg |= PMUA_MP_L2_SRAM_POWER_DOWN;
	if (cpu_is_pxa988())
		mp_idle_cfg |= PMUA_MP_SCU_SRAM_POWER_DOWN;
	__raw_writel(mp_idle_cfg, APMU_MP_IDLE_CFG[cpu]);

	enter_lpm_p[cpu] |= (1 << (power_mode + 1)) - 1;

	trace_pxa_core_hotplug(HOTPLUG_ENTRY, smp_processor_id());
#ifdef CONFIG_DEBUG_FS
	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_ENTER, PXA988_LPM_D2_UDR);
#endif
	finish_func(CPU_SUSPEND_FROM_HOTPLUG);
}
#else
void pxa988_hotplug_enter(u32 cpu, u32 power_mode)
{
	u32 mp_idle_cfg;
	int i, mp_shutdown;
	int cpus_enter_lpm = 0xffffffff;

	pxa988_pre_enter_lpm(cpu, PXA988_LPM_C2);

	/*
	 * For CPU hotplug, we don't need cpu_suspend help functions
	 * but still need to mask LPM bits as the deepest LPM.
	 * We are assuming the hotplug CPU will NEVER be the last CPU
	 * enter C2 since in platform_cpu_kill we ensure that.
	 */
#ifdef CONFIG_ARM_ERRATA_802022
	pxa988_set_reset_handler(__pa(pxa988_return_handler), cpu);
#else
	pxa988_set_reset_handler(__pa(pxa988_hotplug_handler), cpu);
#endif

	/* The hotpluged CPU always allow SCU/L2 SRAM power down */
	mp_idle_cfg = __raw_readl(APMU_MP_IDLE_CFG[cpu]);
	mp_idle_cfg |= PMUA_MP_L2_SRAM_POWER_DOWN;
	if (cpu_is_pxa988())
		mp_idle_cfg |= PMUA_MP_SCU_SRAM_POWER_DOWN;
	__raw_writel(mp_idle_cfg, APMU_MP_IDLE_CFG[cpu]);

	arch_spin_lock(lpm_lock_p);
	enter_lpm_p[cpu] = (1 << (power_mode + 1)) - 1;
	for (i = 0; i < num_cpus; i++)
		cpus_enter_lpm &= enter_lpm_p[i];
	mp_shutdown = test_bit(PXA988_LPM_C2, (void *)&cpus_enter_lpm);
	if (mp_shutdown) {
#ifdef CONFIG_CACHE_L2X0
		pl310_suspend();
#endif
		cpu_cluster_pm_enter();
#ifdef CONFIG_ARM_ERRATA_802022
		for_each_possible_cpu(i) {
			barrier[i] = 0;
		}
#endif
	}
	arch_spin_unlock(lpm_lock_p);

	trace_pxa_core_hotplug(HOTPLUG_ENTRY, smp_processor_id());

	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_ENTER, PXA988_LPM_D2_UDR);

	finish_func(CPU_SUSPEND_FROM_HOTPLUG);
}
#endif
#endif

#ifdef CONFIG_SUSPEND
void pxa988_pm_suspend(u32 cpu, u32 power_mode)
{
#ifdef CONFIG_ARM_ERRATA_802022
	int i;
#endif
	/* Reset handler checks the flag to decide if needs to invalidate L2 */
#ifdef CONFIG_CACHE_L2X0
	l2sram_shutdown = 1;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&l2sram_shutdown,
				sizeof(l2sram_shutdown));
	outer_clean_range(__pa(&l2sram_shutdown), __pa(&l2sram_shutdown + 1));

	pl310_suspend();
#endif

#ifdef CONFIG_ARM_ERRATA_802022
	for_each_possible_cpu(i) {
		barrier[i] = 0;
	}
#endif
	pxa988_pre_enter_lpm(cpu, power_mode);
#ifdef CONFIG_ARM_ERRATA_802022
	/* clear MP powerdown disable bit for suspend */
	for_each_possible_cpu(i) {
		u32 mp_idle_cfg;
		mp_idle_cfg = __raw_readl(APMU_MP_IDLE_CFG[i]);
		mp_idle_cfg &= ~(PMUA_DIS_MP_SLP);
		__raw_writel(mp_idle_cfg, APMU_MP_IDLE_CFG[i]);
	}
#endif

	ddr_dll_rst_wkr(0);
	trace_pxa_cpu_idle(LPM_ENTRY(PXA988_LPM_D2_UDR), cpu);
	cpu_suspend(CPU_SUSPEND_FROM_SUSPEND, finish_func);
	trace_pxa_cpu_idle(LPM_EXIT(PXA988_LPM_D2_UDR), cpu);
	ddr_dll_rst_wkr(1);
#ifdef CONFIG_ARM_ERRATA_802022
	/* set back MP powerdown disable bit for idle/hotplug */
	for_each_possible_cpu(i) {
		u32 mp_idle_cfg;
		mp_idle_cfg = __raw_readl(APMU_MP_IDLE_CFG[i]);
		mp_idle_cfg |= (PMUA_DIS_MP_SLP);
		__raw_writel(mp_idle_cfg, APMU_MP_IDLE_CFG[i]);
	}
#endif
	sav_wucrs = (__raw_readl(MPMU_AWUCRS) | __raw_readl(MPMU_CWUCRS));
	sav_wucrm = (__raw_readl(MPMU_AWUCRM) | __raw_readl(MPMU_CWUCRM));

	pxa988_post_enter_lpm(cpu, power_mode);
}
#endif

static int __init pxa988_lowpower_init(void)
{
#ifdef CONFIG_HAVE_ARM_SCU
	void __iomem *scu_addr;
#endif
	u32 apcr = __raw_readl(MPMU_APCR);

	/* set DSPSD, DTCMSD, BBSD, MSASLPEN */
	apcr |= PMUM_DTCMSD | PMUM_BBSD | PMUM_MSASLPEN;
	if (cpu_is_pxa988())
		apcr |= PMUM_DSPSD;
	/* PMUM_STBYEN in MPMU_APCR is "0b1" by default in reset.
	 * So we also need to clear when doing init.
	 */
	apcr &= ~PMUM_STBYEN;
	__raw_writel(apcr, MPMU_APCR);

#ifdef CONFIG_ARM_ERRATA_802022
	{
		int i;
		/* set MP powerdown disable bit for idle/hotplug */
		for_each_possible_cpu(i) {
			u32 mp_idle_cfg;
			mp_idle_cfg = __raw_readl(APMU_MP_IDLE_CFG[i]);
			mp_idle_cfg |= (PMUA_DIS_MP_SLP);
			__raw_writel(mp_idle_cfg, APMU_MP_IDLE_CFG[i]);
		}
	}
#endif
	num_cpus = num_possible_cpus();
	coherent_buf = __arm_ioremap(pm_reserve_pa, PAGE_SIZE, MT_MEMORY_SO);
	if (coherent_buf == NULL)
		panic("%s: failed to remap memory for pm\n", __func__);
	memset(coherent_buf, 0x0, PAGE_SIZE);

	enter_lpm_p = (u32 *)coherent_buf;

#ifdef CONFIG_CPU_CA9MP
	mp_restore = (u32 *)(&coherent_buf[OFFSET_SCU_SHUTDOWN]);
	lpm_lock_p = (arch_spinlock_t *)(&coherent_buf[OFFSET_SPINLOCK]);
#elif defined(CONFIG_CPU_CA7MP)
	mp_restore = kzalloc(sizeof(u32), GFP_KERNEL);
	lpm_lock_p = kzalloc(sizeof(arch_spinlock_t), GFP_KERNEL);
#ifdef CONFIG_ARM_ERRATA_802022
	barrier = (u32 *)(&coherent_buf[OFFSET_BARRIER]);
#endif
#endif
	*lpm_lock_p = (arch_spinlock_t)__ARCH_SPIN_LOCK_UNLOCKED;

	/*
	 * Set PL310 power ctrl register to set standby_mode_en bit
	 * and dynamic_clk_gating_en bit
	 * it is done in cache-l2x0.c : l2x0_init now
	 */
#ifdef CONFIG_HAVE_ARM_SCU
	scu_addr = pxa_scu_base_addr();
	/* Set SCU control register standby enable bit */
	__raw_writel(__raw_readl(scu_addr + SCU_CTRL) | (1 << 5),
			scu_addr + SCU_CTRL);
#endif

	/* set the power stable timer as 10us */
	__raw_writel(0x28207, APMU_PWR_STBL_TIMER);

	/* Init dll_rst_wkr, skip table 0 by default for Zx */
	ddr_dll_rst_wkr(1);

	finish_func = cpu_is_pxa1088() ? \
		pxa1088_finish_suspend : pxa988_finish_suspend;
	return 0;
}

postcore_initcall(pxa988_lowpower_init);
