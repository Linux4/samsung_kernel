/*
 * arch/arm/mach-mmp/pm-pxa1928.c
 *
 * Author:	Timothy Xia <wlxia@marvell.com>
 * Copyright:	(C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/gfp.h>
#include <linux/pm_qos.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/of.h>

#include "pxa1928_lowpower.h"
#include "regs-addr.h"
#include "pm.h"

/* FIXME:
 * Please use Global API to replace the following local mapping
 */
static void __iomem *apmu_virt_addr;
static void __iomem *mpmu_virt_addr;
static void __iomem *APMU_WKUP_MASK;
static void __iomem *APMU_WKUP_CLR;
static void __iomem *APMU_WKUP_STAT;

static void __init pmu_mapping(void)
{
	apmu_virt_addr = regs_addr_get_va(REGS_ADDR_APMU);
	mpmu_virt_addr = regs_addr_get_va(REGS_ADDR_MPMU);

	APMU_WKUP_MASK = apmu_virt_addr + WKUP_MASK;
	APMU_WKUP_CLR = apmu_virt_addr + WKUP_CLR;
	APMU_WKUP_STAT = apmu_virt_addr + WKUP_STAT;
}

#define IRQ_PXA1928_START			32
#define IRQ_PXA1928_KEYPAD			(IRQ_PXA1928_START + 9)
#define IRQ_PXA1928_TIMER1			(IRQ_PXA1928_START + 13)
#define IRQ_PXA1928_TIMER2			(IRQ_PXA1928_START + 32)
#define IRQ_PXA1928_TIMER3			(IRQ_PXA1928_START + 38)
#define IRQ_PXA1928_PAD_EDGE		(IRQ_PXA1928_START + 43)
#define IRQ_PXA1928_GPIO			(IRQ_PXA1928_START + 49)
#define IRQ_PXA1928_USB_OTG		(IRQ_PXA1928_START + 67)
#define IRQ_PXA1928_MMC			(IRQ_PXA1928_START + 69)
#define IRQ_PXA1928_MMC2			(IRQ_PXA1928_START + 71)
#define IRQ_PXA1928_MMC3			(IRQ_PXA1928_START + 73)
#define IRQ_PXA1928_MMC4			(IRQ_PXA1928_START + 75)
#define IRQ_PXA1928_PMIC			(IRQ_PXA1928_START + 77)
#define IRQ_PXA1928_RTC_ALARM		(IRQ_PXA1928_START + 79)
static void pxa1928_set_wake(int irq, unsigned int on)
{
	uint32_t wkup_mask = 0;

	/* setting wakeup sources */
	switch (irq) {
	case IRQ_PXA1928_PMIC:
		wkup_mask = PMUA_PMIC;
		break;
	case IRQ_PXA1928_USB_OTG:
		wkup_mask = PMUA_USB;
		break;
	case IRQ_PXA1928_GPIO:
		wkup_mask = PMUA_GPIO;
		break;
	case IRQ_PXA1928_MMC:
	case IRQ_PXA1928_MMC2:
		wkup_mask = PMUA_SDH1;
		break;
	case IRQ_PXA1928_MMC3:
	case IRQ_PXA1928_MMC4:
		wkup_mask = PMUA_SDH2;
		break;
	case IRQ_PXA1928_KEYPAD:
		wkup_mask = PMUA_KEYPAD;
		break;
	case IRQ_PXA1928_RTC_ALARM:
		wkup_mask = PMUA_RTC_ALARM;
		break;
	case IRQ_PXA1928_TIMER1:
		wkup_mask = PMUA_TIMER_1_1;
		break;
	case IRQ_PXA1928_TIMER2:
		wkup_mask = PMUA_TIMER_2_2;
		break;
	case IRQ_PXA1928_TIMER3:
		wkup_mask = PMUA_TIMER_3_2;
	default:
		/* do nothing */
		break;
	}

	if (on) {
		if (wkup_mask) {
			wkup_mask |= readl(APMU_WKUP_MASK);
			writel(wkup_mask, APMU_WKUP_MASK);
		}
	} else {
		if (wkup_mask) {
			wkup_mask = ~wkup_mask & readl(APMU_WKUP_MASK);
			writel(wkup_mask, APMU_WKUP_MASK);
		}
	}
}

static void pxa1928_plt_suspend_init(void)
{
	pmu_mapping();
}

static int pxa1928_pm_check_constraint(void)
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
		 * If here is alive LPM constraint, this function's
		 * return value will cause System to repeat suspend
		 * entry and exit until other wakeup events wakeup
		 * system to full awake or the held LPM constraint
		 * is released by the user then finally entering
		 * the Suspend + Chip sleep mode.
		 */
		if (node->prio != PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE) {
			pr_info("****************************************\n");
			pr_err("%s lpm constraint alive before Suspend",
				req->name);
			pr_info("*****************************************\n");
			ret = -EBUSY;
		}
	}

	return ret;
}

static int pxa1928_suspend_check(void)
{
	u32 ret = 0;

	ret = pxa1928_pm_check_constraint();

	pr_info("========wake up events status =========\n");
	pr_info("BEFORE SUSPEND WKUP_STAT:0x%x\n", readl(apmu_virt_addr + WKUP_STAT));

	return ret;
}

static u32 wakeup_source_check(void)
{
	char *buf;
	size_t size = PAGE_SIZE - 1;
	int len_s = 0, len = 0;
	void __iomem *gic_dist_base;
	u32 reg;

	u32 wkup_stat = readl(apmu_virt_addr + WKUP_STAT);
	u32 apps_pwrmode = readl(apmu_virt_addr + APPS_PWRMODE);

	u32 apcr = readl(mpmu_virt_addr + APCR);
	u32 cpcr = readl(mpmu_virt_addr + CPCR);

	pr_info("After SUSPEND");
	pr_info("AP WAKE-UP STAT:0x%x\n", wkup_stat);
	pr_info("APCR:0x%x, CPCR:0x%x\n", apcr, cpcr);
	pr_info("APPS_PWRMODE:0x%x\n", apps_pwrmode);

	if (0 == (apps_pwrmode & PMUA_APPS_D2_STATUS)) {
		pr_info("!!!!!! AP cannot reach D2 mode!!!!!!\n");
		if (readl(apmu_virt_addr + LCD_PWRCTRL) & ISLD_PWR_STAT)
			pr_info("       DISP ISLD still on, Reg value: 0x%x\n",
					readl(apmu_virt_addr + LCD_PWRCTRL));
		else if (readl(apmu_virt_addr + VPU_PWRCTRL) & ISLD_PWR_STAT)
			pr_info("       VPU ISLD still on, Reg value: 0x%x\n",
					readl(apmu_virt_addr + VPU_PWRCTRL));
		else if (readl(apmu_virt_addr + GC3D_PWRCTRL) & ISLD_PWR_STAT)
			pr_info("       GC3D ISLD still on, Reg value: 0x%x\n",
					readl(apmu_virt_addr + GC3D_PWRCTRL));
		else if (readl(apmu_virt_addr + GC2D_PWRCTRL) & ISLD_PWR_STAT)
			pr_info("       GC2D ISLD still on, Reg value: 0x%x\n",
					readl(apmu_virt_addr + GC2D_PWRCTRL));
		else if (readl(apmu_virt_addr + ISP_PWRCTRL) & ISLD_PWR_STAT)
			pr_info("       ISP ISLD still on, Reg value: 0x%x\n",
					readl(apmu_virt_addr + ISP_PWRCTRL));
		else {
			pr_info("       APSS PWR ISLDs all off\n");
			pr_info("       CPSS PWR STAT: 0x%x\n",
					readl(mpmu_virt_addr + CPSR));
		}
	}

	buf = (char *)__get_free_pages(GFP_ATOMIC, 0);
	if (!buf) {
		pr_err("memory alloc for wakeup check is failed!!\n");
		return wkup_stat;
	}

	len_s = len = snprintf(buf, size, "AP Sub system is woken up by:");
	if (wkup_stat & PMUA_CP_IPC)
		len += snprintf(buf + len, size - len, "CP IPC,");
	if (wkup_stat & PMUA_CP_TIMER1)
		len += snprintf(buf + len, size - len, "CP TIMER1,");
	if (wkup_stat & PMUA_PMIC)
		len += snprintf(buf + len, size - len, "PMIC,");
	if (wkup_stat & PMUA_USB)
		len += snprintf(buf + len, size - len, "USB,");
	if (wkup_stat & PMUA_AUDIO)
		len += snprintf(buf + len, size - len, "Audio Island,");
	/* TODO: check GPIO status */
	if (wkup_stat & PMUA_GPIO)
		len += snprintf(buf + len, size - len, "GPIO,");
	if (wkup_stat & PMUA_GPIO_SEC)
		len += snprintf(buf + len, size - len, "Secure GPIO,");
	if (wkup_stat & PMUA_SSP2)
		len += snprintf(buf + len, size - len, "SSP2,");
	if (wkup_stat & PMUA_SSP1)
		len += snprintf(buf + len, size - len, "SSP1,");
	if (wkup_stat & PMUA_SDH2)
		len += snprintf(buf + len, size - len, "SDH2,");
	if (wkup_stat & PMUA_SDH1)
		len += snprintf(buf + len, size - len, "SDH1,");
	if (wkup_stat & PMUA_KEYPAD)
		len += snprintf(buf + len, size - len, "KeyPad,");
	if (wkup_stat & PMUA_TRACKBALL)
		len += snprintf(buf + len, size - len, "TRACKBALL,");
	if (wkup_stat & PMUA_ROTARYKEY)
		len += snprintf(buf + len, size - len, "ROTARYKEY,");
	if (wkup_stat & PMUA_RTC_ALARM)
		len += snprintf(buf + len, size - len, "RTC_ALARM,");
	if (wkup_stat & PMUA_WTD_TIMER_2)
		len += snprintf(buf + len, size - len, "WATCHDOG TIMER2,");
	if (wkup_stat & PMUA_WTD_TIMER_1)
		len += snprintf(buf + len, size - len, "WATCHDOG TIMER1,");
	if (wkup_stat & PMUA_TIMER_3_3)
		len += snprintf(buf + len, size - len, "TIMER_3_3,");
	if (wkup_stat & PMUA_TIMER_3_2)
		len += snprintf(buf + len, size - len, "TIMER_3_2,");
	if (wkup_stat & PMUA_TIMER_3_1)
		len += snprintf(buf + len, size - len, "TIMER_3_1,");
	if (wkup_stat & PMUA_TIMER_2_3)
		len += snprintf(buf + len, size - len, "TIMER_2_3,");
	if (wkup_stat & PMUA_TIMER_2_2)
		len += snprintf(buf + len, size - len, "TIMER_2_2,");
	if (wkup_stat & PMUA_TIMER_2_1)
		len += snprintf(buf + len, size - len, "TIMER_2_1,");
	if (wkup_stat & PMUA_TIMER_1_3)
		len += snprintf(buf + len, size - len, "timer_1_3,");
	if (wkup_stat & PMUA_TIMER_1_2)
		len += snprintf(buf + len, size - len, "timer_1_2,");
	if (wkup_stat & PMUA_TIMER_1_1)
		len += snprintf(buf + len, size - len, "timer_1_1,");


	if (len_s != len) {
		snprintf(buf + (len - 1), size - len + 1, "\n");
		pr_info("%s", buf);
	} else {
		gic_dist_base = regs_addr_get_va(REGS_ADDR_GIC_DIST);
		reg = readl_relaxed(gic_dist_base + GIC_DIST_PENDING_SET);
		if (reg) {
			pr_info(" GIC info:\n");
			pr_info("       PPI/IPI Pending Reg value: 0x%x\n", reg);
		} else
			pr_err("Unexpected Events Wakeup AP!\n");
	}

	free_pages((unsigned long)buf, 0);

	return wkup_stat;
}

static u32 pxa1928_post_chk_wakeup(void)
{
	int ret;

	ret = wakeup_source_check();

	pr_info("=======================================\n");
	return ret;
}

static void pxa1928_clear_sdh_wakeup(void)
{
	u32 val;
	u32 mask = 0xf;

	val = __raw_readl(APMU_WKUP_CLR);
	__raw_writel(val | mask, APMU_WKUP_CLR);
}

static void pxa1928_post_clr_wakeup(u32 wkup_status)
{
	/*
	 * clear sdh wakeup int if woken up by sdh.
	 */
	if (wkup_status & (PMUA_SDH1 | PMUA_SDH2)) {
		pxa1928_clear_sdh_wakeup();
		pr_info(" Clear the Wakeup Event for SDH\n");
	}

	return;
}

static struct suspend_ops pxa1928_suspend_ops = {
	.pre_suspend_check = pxa1928_suspend_check,
	.post_chk_wakeup = pxa1928_post_chk_wakeup,
	.post_clr_wakeup = pxa1928_post_clr_wakeup,
	.set_wake = pxa1928_set_wake,
	.plt_suspend_init = pxa1928_plt_suspend_init,
};

static struct platform_suspend pxa1928_suspend = {
	.suspend_state	= POWER_MODE_UDR,
	.ops		= &pxa1928_suspend_ops,
};
static int __init pxa1928_suspend_init(void)
{
	int ret;

	if (!of_machine_is_compatible("marvell,pxa1928"))
		return -ENODEV;
	ret = mmp_platform_suspend_register(&pxa1928_suspend);
	if (ret)
		WARN_ON("PXA1928 Suspend Register fails!!");

	return 0;
}
late_initcall(pxa1928_suspend_init);
