/*
 * linux/arch/arm/mach-mmp/pm-pxa988.c
 *
 * Author:      Hong Feng <hongfeng@marvell.com>
 * Copyright:   (C) 2012 Marvell International Ltd.
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
#include <linux/suspend.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/mfd/88pm80x.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/power/fake-sysoff.h>
#include <linux/pm_qos.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/smp_scu.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/hardware/gic.h>
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>
#include <mach/regs-icu.h>
#include <mach/pxa988_lowpower.h>
#include <mach/regs-ciu.h>
#include <mach/irqs.h>

#ifdef CONFIG_SEC_GPIO_DVS
#include <linux/secgpio_dvs.h>
#endif

#include "common.h"

static struct clk *vctcxo_clk;
static struct wakeup_source system_wakeup;
static int pmic_wakeup_detect;
uint32_t detect_wakeup_status;
EXPORT_SYMBOL(detect_wakeup_status);

void gpio_edge_wakeup_enable(void)
{
	uint32_t awucrm = 0, apcr = 0;
	/* already get pmu_lock in LPM code */
	awucrm = __raw_readl(MPMU_AWUCRM);
	apcr = __raw_readl(MPMU_APCR);
	__raw_writel(awucrm | PMUM_WAKEUP2, MPMU_AWUCRM);
	__raw_writel(apcr & ~PMUM_SLPWP2, MPMU_APCR);
}

void gpio_edge_wakeup_disable(void)
{
	uint32_t awucrm = 0, apcr = 0;
	/* already get pmu_lock in LPM code */
	awucrm = __raw_readl(MPMU_AWUCRM);
	apcr = __raw_readl(MPMU_APCR);
	__raw_writel(awucrm & ~PMUM_WAKEUP2, MPMU_AWUCRM);
	__raw_writel(apcr | PMUM_SLPWP2, MPMU_APCR);
}

int pxa988_set_wake(struct irq_data *data, unsigned int on)
{
	int irq = data->irq;
	struct irq_desc *desc = irq_to_desc(data->irq);
	uint32_t awucrm = 0, apcr = 0;

	if (on) {
		if (desc->action)
			desc->action->flags |= IRQF_NO_SUSPEND;
	} else {
		if (desc->action)
			desc->action->flags &= ~IRQF_NO_SUSPEND;
	}

	/* setting wakeup sources */
	switch (irq) {
	/* wakeup line 2 */
	case IRQ_PXA988_GPIO_AP:
		awucrm = PMUM_WAKEUP2;
		apcr |= PMUM_SLPWP2;
		break;
	/* wakeup line 3 */
	case IRQ_PXA988_KEYPAD:
		awucrm = PMUM_WAKEUP3 | PMUM_KEYPRESS | PMUM_TRACKBALL |
				PMUM_NEWROTARY;
		apcr |= PMUM_SLPWP3;
		break;
	/* wakeup line 4 */
	case IRQ_PXA988_AP_TIMER1:
		awucrm = PMUM_WAKEUP4 | PMUM_AP1_TIMER_1;
		apcr |= PMUM_SLPWP4;
		break;
	case IRQ_PXA988_AP_TIMER2_3:
		awucrm = PMUM_WAKEUP4 | PMUM_AP1_TIMER_2 |
				PMUM_AP1_TIMER_3;
		apcr |= PMUM_SLPWP4;
		break;
	case IRQ_PXA988_AP2_TIMER1:
		awucrm = PMUM_WAKEUP4 | PMUM_AP2_TIMER_1;
		apcr |= PMUM_SLPWP4;
		break;
	case IRQ_PXA988_AP2_TIMER2_3:
		awucrm = PMUM_WAKEUP4 | PMUM_AP2_TIMER_2 |
				PMUM_AP2_TIMER_3;
		apcr |= PMUM_SLPWP4;
		break;
	case IRQ_PXA988_RTC_ALARM:
		awucrm = PMUM_WAKEUP4 | PMUM_RTC_ALARM;
		apcr |= PMUM_SLPWP4;
		break;
	/* wakeup line 5 */
	case IRQ_PXA988_USB1:
		awucrm = PMUM_WAKEUP5;
		apcr |= PMUM_SLPWP5;
		break;
	/* wakeup line 6 */
	case IRQ_PXA988_MMC:
		awucrm = PMUM_WAKEUP6 | PMUM_SDH_23 | PMUM_SQU_SDH1;
		apcr |= PMUM_SLPWP6;
		break;
	case IRQ_PXA988_HIFI_DMA:
		awucrm = PMUM_WAKEUP6 | PMUM_SQU_SDH1;
		apcr |= PMUM_SLPWP6;
		break;
	/* wakeup line 7 */
	case IRQ_PXA988_PMIC:
		awucrm = PMUM_WAKEUP7;
		apcr |= PMUM_SLPWP7;
		break;
	default:
		if (irq >= IRQ_GPIO_START && irq < IRQ_BOARD_START) {
			awucrm = PMUM_WAKEUP2;
			apcr |= PMUM_SLPWP2;
		} else
			pr_err("Error: no defined wake up source irq: %d\n",
					irq);
	}
	/* add lock, MPMU_APCR may access through other cpu on SMP system */
	pmu_register_lock();
	if (on) {
		if (awucrm) {
			awucrm |= __raw_readl(MPMU_AWUCRM);
			__raw_writel(awucrm, MPMU_AWUCRM);
		}
		if (apcr) {
			apcr = ~apcr & __raw_readl(MPMU_APCR);
			__raw_writel(apcr, MPMU_APCR);
		}
	} else {
		if (awucrm) {
			awucrm = ~awucrm & __raw_readl(MPMU_AWUCRM);
			__raw_writel(awucrm, MPMU_AWUCRM);
		}
		if (apcr) {
			apcr |= __raw_readl(MPMU_APCR);
			__raw_writel(apcr, MPMU_APCR);
		}
	}
	pmu_register_unlock();
	return 0;
}

static int pxa988_pm_valid(suspend_state_t state)
{
	return ((state == PM_SUSPEND_STANDBY) || (state == PM_SUSPEND_MEM));
}

/* Called after devices suspend, before noirq devices suspend */
static int pxa988_pm_prepare(void)
{
	return 0;
}

/* Clear SDH wakeup to avoid IRQ storm */
static int pxa988_clr_sdh_wakeup(void)
{
	uint32_t val;
	uint32_t mask = APMU_PXA988_SD1_WAKE_CLR | APMU_PXA988_SD2_WAKE_CLR
					| APMU_PXA988_SD3_WAKE_CLR;

	val = __raw_readl(APMU_WAKE_CLR);
	__raw_writel(val | mask, APMU_WAKE_CLR);

	return 0;
}

static int pxa988_pm_check_constraint(void)
{
	int ret = 0;
	struct pm_qos_object *idle_qos;
	struct list_head *list;
	struct plist_node *node;
	struct pm_qos_request *req;

	idle_qos = pm_qos_array[PM_QOS_CPUIDLE_BLOCK];
	list = &idle_qos->constraints->list.node_list;

	/* local irq disabled here, not need any lock */
	list_for_each_entry(node, list, node_list) {
		req = container_of(node, struct pm_qos_request, node);
		/*
		 * add warn and return error if:
		 * 1. any lpm constraint hold
		 * 2. constraint name didn't start with "uart" --- > uart rx pad
		 *    or rxuart --- > rxUART1,2,3...
		 */
		if ((node->prio != PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE) &&
				strncasecmp(req->name, "uart", 4) &&
				strncasecmp(req->name, "rxuart", 6)) {
			WARN(1, KERN_ERR"%s lpm constraint before suspend",
					req->name);
			ret = -EBUSY;
		}
	}

	return ret;
}

static int pxa988_pm_enter(suspend_state_t state)
{
	uint32_t awucrs = 0;
	uint32_t awucrs_pmic_wakeup = PMUM_WAKEUP7;
	uint32_t reg = 0;

#ifdef CONFIG_SEC_GPIO_DVS
	/************************ Caution !!! ****************************
	This function must be located in appropriate SLEEP position
	in accordance with the specification of each BB vendor.
	************************ Caution !!! ****************************/
	gpio_dvs_check_sleepgpio();
#endif

	/*
	 * pmic thread not completed, exit;
	 * otherwise system can't be waked up
	 */
	reg = __raw_readl(ICU_INT_CONF(IRQ_PXA988_PMIC - IRQ_PXA988_START));
	if ((reg & 0x3) == 0)
		return -EAGAIN;

	/*
	 * check if there is any constraint, it's not allowed
	 * Now, we only give out WARN for convenience, like GC
	 * LPM disable case, GC constraint is always hold
	 */
	pxa988_pm_check_constraint();

#ifdef CONFIG_FAKE_SYSTEMOFF
	/* In fake system off mode, cancel timeout work here*/
	if (fake_sysoff_status_query()) {
		fake_sysoff_set_block_onkey(0);
		fake_sysoff_work_cancel();
	}
#endif

	clk_disable(vctcxo_clk);

	pr_info("BEFORE SUSPEND AWUCRS:%x\n", __raw_readl(MPMU_AWUCRS));
	pxa988_pm_suspend(0, PXA988_LPM_D2_UDR);

	clk_enable(vctcxo_clk);

	awucrs = __raw_readl(MPMU_AWUCRS);
	detect_wakeup_status = awucrs;
	pr_info("INT0:0x%x INT1:0x%x\n", __raw_readl(ICU_INT_STATUS_0),
			__raw_readl(ICU_INT_STATUS_1));
	if (awucrs & awucrs_pmic_wakeup) {
		pr_info(" [%s]AWUCRS:%x PMIC WAKEUP DETECT\n",
				__func__, awucrs);
		pmic_wakeup_detect = 1;
	} else
		pr_info(" [%s]AWUCRS:%x OTHER WAKEUP DETECT\n",
				__func__, awucrs);

	/*
	* Note: In case that SDH wake up would happen together
	* with PMIC or others, so check awucrs separately
	*/
	if (awucrs & (PMUM_SQU_SDH1|PMUM_SDH_23)) {
		pxa988_clr_sdh_wakeup();
		pr_info(" Clear the WakeUp Event for SDH\n");
	}

	return 0;
}

/* Called after noirq devices resume, before devices resume */
static void pxa988_pm_finish(void)
{
	return 0;
}

static void pxa988_pm_wake(void)
{
	if (pmic_wakeup_detect)
		__pm_wakeup_event(&system_wakeup, 5 * 1000);
	pmic_wakeup_detect = 0;
}

static const struct platform_suspend_ops pxa988_pm_ops = {
	.valid          = pxa988_pm_valid,
	.prepare        = pxa988_pm_prepare,
	.enter          = pxa988_pm_enter,
	.finish         = pxa988_pm_finish,
	.wake           = pxa988_pm_wake,
};

static int __init pxa988_pm_init(void)
{
	u32 awucrm = 0;

	suspend_set_ops(&pxa988_pm_ops);

	wakeup_source_init(&system_wakeup,
			"system_wakeup_detect");

	vctcxo_clk = clk_get(NULL, "VCTCXO");
	if (IS_ERR(vctcxo_clk)) {
		pr_err("unable to get VCTCXO\n");
		return PTR_ERR(vctcxo_clk);
	}
	clk_enable(vctcxo_clk);

	/*
	 * These two bits are used to solve the corner case that
	 * 1.	APMU enters sleep mode.
	 * 2.	MPMU is activated but not finished.
	 * 3.	An interrupt comes
	 * 4.	The interrupt is not a wake up source
	 * Keeping these two bits can assure it aborts from
	 * the intermediate state.
	 *
	 * Actually for suspend it won't happen since we disable
	 * non-wake up interrupts before WFI.
	 * For idle replacement we will encounter such issue
	 * since not all interrupts are wake up source.
	 * */
	awucrm |= PMUM_AP_ASYNC_INT;
	awucrm |= PMUM_AP_FULL_IDLE;
	__raw_writel(awucrm, MPMU_AWUCRM);
	/* hook wakeup callback */
	gic_arch_extn.irq_set_wake = pxa988_set_wake;

	return 0;
}
late_initcall(pxa988_pm_init);
