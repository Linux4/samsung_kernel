/*
 * arch/arm/mach-mmp/pm-pxa1L88.c
 *
 * Author:	Fan Wu <fwu@marvell.com>
 * Copyright:	(C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/cputype.h>
#include <mach/pxa988_lowpower.h>
#include <mach/irqs.h>
#include <mach/pm.h>
#include <mach/regs-apbc.h>
#include <mach/regs-icu.h>
#include <linux/pm_qos.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/gfp.h>

#include "regs-addr.h"

#ifdef CONFIG_ARM_ERRATA_802022
#include <asm/mcpm.h>
#ifndef CONFIG_TZ_HYPERVISOR
extern void errata_802022_handler(void);
#endif
static void __iomem *ciu_warm_reset_vector;
static void __iomem *APMU_MP_IDLE_CFG[4];
#endif

/* FIXME: The following Macro will be refined by DT in future
 * when Most of the Devices is configured in DT way.
 */
#define IRQ_PXA1L88_START		32
#define IRQ_PXA1L88_PMIC		(IRQ_PXA1L88_START + 4)
#define IRQ_PXA1L88_RTC_ALARM		(IRQ_PXA1L88_START + 6)
#define IRQ_PXA1L88_KEYPAD		(IRQ_PXA1L88_START + 9)
#define IRQ_PXA1L88_AP0_TIMER1		(IRQ_PXA1L88_START + 13)
#define IRQ_PXA1L88_AP0_TIMER2_3	(IRQ_PXA1L88_START + 14)
/* timer2, 3 of AP_TIMER interrupts are seperated in Helan2 */
#define IRQ_PXA1U88_AP0_TIMER3		(IRQ_PXA1L88_START + 2)
#define IRQ_PXA1L88_AP1_TIMER1		(IRQ_PXA1L88_START + 29)
#define IRQ_PXA1L88_AP1_TIMER2_3	(IRQ_PXA1L88_START + 30)
#define IRQ_PXA1L88_AP2_TIMER1		(IRQ_PXA1L88_START + 64)
#define IRQ_PXA1L88_AP2_TIMER2_3	(IRQ_PXA1L88_START + 65)
#define IRQ_PXA1L88_MMC			(IRQ_PXA1L88_START + 39)
#define IRQ_PXA1L88_USB1		(IRQ_PXA1L88_START + 44)
#define IRQ_PXA1L88_HIFI_DMA		(IRQ_PXA1L88_START + 46)
#define IRQ_PXA1L88_GPIO_AP		(IRQ_PXA1L88_START + 49)
#define IRQ_PXA1U88_GPS			(IRQ_PXA1L88_START + 95)

static void pxa1L88_set_wake(int irq, unsigned int on)
{
	uint32_t awucrm = 0;
	uint32_t apbc_timer0;
	void __iomem *apbc = regs_addr_get_va(REGS_ADDR_APBC);

	/* setting wakeup sources */
	switch (irq) {
	/* wakeup line 2 */
	case IRQ_PXA1L88_GPIO_AP:
		awucrm = PMUM_WAKEUP2;
		break;
	/* wakeup line 3 */
	case IRQ_PXA1L88_KEYPAD:
		awucrm = PMUM_WAKEUP3 | PMUM_KEYPRESS | PMUM_TRACKBALL |
				PMUM_NEWROTARY;
		break;
	/* wakeup line 4 */
	case IRQ_PXA1L88_AP0_TIMER1:
		if (cpu_is_pxa1U88()) {
			apbc_timer0 = __raw_readl(apbc + TIMER0);
			__raw_writel(0x1 << 7 | apbc_timer0, apbc + TIMER0);
		}
		awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_1;
		break;
	case IRQ_PXA1L88_AP0_TIMER2_3:
		if (cpu_is_pxa1U88()) {
			apbc_timer0 = __raw_readl(apbc + TIMER0);
			__raw_writel(0x1 << 7 | apbc_timer0, apbc  + TIMER0);
			awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_2;
		} else
			awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_2 |
				PMUM_AP0_2_TIMER_3;
		break;
	case IRQ_PXA1U88_AP0_TIMER3:
		if (cpu_is_pxa1U88()) {
			apbc_timer0 = __raw_readl(apbc + TIMER0);
			__raw_writel(0x1 << 7 | apbc_timer0, apbc + TIMER0);
			awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_3;
		}
		break;
	case IRQ_PXA1L88_AP1_TIMER1:
		awucrm = PMUM_WAKEUP4 | PMUM_AP1_TIMER_1;
		break;
	case IRQ_PXA1L88_AP1_TIMER2_3:
		awucrm = PMUM_WAKEUP4 | PMUM_AP1_TIMER_2 |
			PMUM_AP1_TIMER_3;
		break;
	case IRQ_PXA1L88_AP2_TIMER1:
		awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_1;
		break;
	case IRQ_PXA1L88_AP2_TIMER2_3:
		awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_2 |
			PMUM_AP0_2_TIMER_3;
		break;
	case IRQ_PXA1L88_RTC_ALARM:
		awucrm = PMUM_WAKEUP4 | PMUM_RTC_ALARM;
		break;
	/* wakeup line 5 */
	case IRQ_PXA1L88_USB1:
	case IRQ_PXA1U88_GPS:
		awucrm = PMUM_WAKEUP5;
		break;
	/* wakeup line 6 */
	case IRQ_PXA1L88_MMC:
		awucrm = PMUM_WAKEUP6 | PMUM_SDH_23 | PMUM_SQU_SDH1;
		break;
	case IRQ_PXA1L88_HIFI_DMA:
		awucrm = PMUM_WAKEUP6 | PMUM_SQU_SDH1;
		break;
	/* wakeup line 7 */
	case IRQ_PXA1L88_PMIC:
		awucrm = PMUM_WAKEUP7;
		break;
	default:
		/* do nothing */
		break;
	}
	if (on) {
		if (awucrm) {
			awucrm |= __raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
			__raw_writel(awucrm, regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
		}
	} else {
		if (awucrm) {
			awucrm = ~awucrm & __raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
			__raw_writel(awucrm, regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
		}
	}
}

static void pxa1L88_plt_suspend_init(void)
{
	u32 awucrm = 0;

	awucrm = __raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
	awucrm |= (PMUM_AP_ASYNC_INT | PMUM_AP_FULL_IDLE);

	__raw_writel(awucrm, regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);

#ifdef CONFIG_ARM_ERRATA_802022
#ifndef CONFIG_TZ_HYPERVISOR
	/*
	 * FIXME: This hard code will be fixed in future patch.
	 */
	ciu_warm_reset_vector = ioremap(0xD4282C00 + 0xD8, 0x4);
#endif

	APMU_MP_IDLE_CFG[0]   = regs_addr_get_va(REGS_ADDR_APMU) + MP_CFG0;
	APMU_MP_IDLE_CFG[1]   = regs_addr_get_va(REGS_ADDR_APMU) + MP_CFG1;
	APMU_MP_IDLE_CFG[2]   = regs_addr_get_va(REGS_ADDR_APMU) + MP_CFG2;
	APMU_MP_IDLE_CFG[3]   = regs_addr_get_va(REGS_ADDR_APMU) + MP_CFG3;
#endif
}

static int pxa1L88_pm_check_constraint(void)
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
			pr_err("%s lpm constraint before Suspend", req->name);
			pr_info("*****************************************\n");
			ret = -EBUSY;
		}
	}

	return ret;
}

static int pxa1L88_suspend_check(void)
{
	u32 ret, reg = __raw_readl(regs_addr_get_va(REGS_ADDR_ICU) +
			((IRQ_PXA1L88_PMIC - IRQ_PXA1L88_START) << 2));

	/*
	 * check PMIC interrupt config before entering suspend. We
	 * must ensure the last time PMIC interrupt handler is done
	 * and the PMIC interrupt is enabled before entering suspend,
	 * other wise, the suspend may NOT be woken up any more.
	 */
	if ((reg & 0x3) == 0)
		return -EAGAIN;

	ret = pxa1L88_pm_check_constraint();
#ifdef CONFIG_ARM_ERRATA_802022
	if (!ret) {
		u32 mp_idle_cfg;
		int i;
		/*
		 * Previously we enabled WR by disable power down of MP for
		 * all status. But this will impact suspend power. Now we
		 * remove old WR working scope for suspend.
		 * By measurement, after using new WR and remove old WR for suspend,
		 * Suspend VCC_MAIN power decrease from 3mA to 1mA.
		 */
		for_each_possible_cpu(i) {
			mp_idle_cfg = __raw_readl(APMU_MP_IDLE_CFG[i]);
			mp_idle_cfg &= ~(PMUA_DIS_MP_SLP);
			__raw_writel(mp_idle_cfg, APMU_MP_IDLE_CFG[i]);
		}
#ifndef CONFIG_TZ_HYPERVISOR
		writel(__pa(errata_802022_handler), ciu_warm_reset_vector);
#endif
	}
#endif
	pr_info("========wake up events status =========\n");
	pr_info("BEFORE SUSPEND AWUCRS:0x%x\n",
			__raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRS));

	return ret;
}

static char *hw_irq_name_1x88[] = {
	"AIRQ - IRQ",
	"SSP2",
	"SSP1(HiFi)",
	"SSP0",
	"PMIC_INT",
	"RTC_INT",
	"RTC_ ALARM",
	"I2C (AP)",
	"GC1000T",
	"New Rotarey or Trackball or KeyPad",
	"DMA_int to APc1",
	"DXO Engine",
	"Battery Monitor (onewire)",
	"AP_Timer1",
	"AP_Timer2 or AP_Timer3",
	"ISP DMA",
	"CP_AP IPC(AP_0)",
	"CP_AP IPC(AP_1)",
	"CP_AP IPC(AP_2)",
	"CP_AP IPC(AP_3)",
	"CP_AP IPC(AP_4)",
	"AP_CP IPC(CP_0)",
	"AP_CP IPC(CP_1)",
	"AP_CP IPC(CP_2)",
	"AP_CP IPC(CP_3)",
	"Codec",
	"ap_cp_l2 and ddr_int",
	"UART1 (Fast)",
	"UART2 (Fast)",
	"AP2_Timer1",
	"AP2_Timer2 or AP2_Timer3",
	"CP_Timer1",
	"CP_Timer2 or CP_Timer3",
	"i2c_int",
	"GSSP (PCM on MSA)",
	"WDT",
	"Main PMU Int",
	"CA7MP Frequency change Int",
	"Seagull Frequency change Int",
	"MMC",
	"AEU",
	"LCD Interface",
	"CI Interface",
	"DDRC Performance Counter",
	"USB 1 (PHY)",
	"NAND",
	"SQU DMA",
	"DMA_int to CP",
	"DMA_int to APc0",
	"GPIO_AP",
	"pad_edge_detect",
	"SPH (ULPI)",
	"IPC_SRV0_Seagull",
	"DSI",
	"I2C (CP-slow)",
	"GPIO_CP",
	"IPC_SRV0_AP",
	"performance_counter_cp",
	"coresight int for AP core0",
	"UART0 (Slow), CP",
	"DRO and temp sensor int",
	"coresight int for AP core1",
	"Fabric  timeout",
	"SM_INT (from PinMux)",
	"AP2_Timer1",
	"AP2_Timer2 or AP2_Timer3",
	"coresight int for AP core2",
	"DMA_int to APc2",
	"coresight int for AP core3",
	"DMA_int to APc3",
	"hsi_irq",
	"hsi_wake_wakeup",
	"GC320",
	"DMA_secure_int to APc0",
	"DMA_secure_int to APc1",
	"DMA_secure_int to APc2",
	"DMA_secure_int to APc3",
};


static int icu_async_further_check(void)
{
#define ICU_INT_STATUS_BASE	0x200
	int i, bit, len = 0;
	void __iomem *irq_stat_base = regs_addr_get_va(REGS_ADDR_ICU) + ICU_INT_STATUS_BASE;
	unsigned long irq_status;
	char *buf;
	size_t size = PAGE_SIZE - 1;

	buf = (char *)__get_free_pages(GFP_ATOMIC, 0);
	if (!buf) {
		pr_err("memory alloc for wakeup check is failed!!\n");
		goto err_exit;
	}

	for (i = 0; i < DIV_ROUND_UP(ARRAY_SIZE(hw_irq_name_1x88), 32); i++) {
		irq_status = __raw_readl(irq_stat_base + i * 4);
		pr_debug("INTReg_%d: 0x%lx ", i, irq_status);

		bit = find_first_bit(&irq_status, 32);
		while (bit < 32) {
			if (!len)
				len = snprintf(buf + len, size - len,
						"Possible Wakeup by IRQ: ");

			if ((bit + i * 32) < ARRAY_SIZE(hw_irq_name_1x88))
				len += snprintf(buf + len, size - len, "%s,",
						hw_irq_name_1x88[bit + i * 32]);
			else {
				len += snprintf(buf + len, size - len,
						"The IRQ_%d cannot be parased\n",
						(bit + i * 32));
				break;
			}

			bit = find_next_bit(&irq_status, 32, bit + 1);
		}
	}

	if (!len) {
		free_pages((unsigned long)buf, 0);
		goto err_exit;
	}

	snprintf(buf + (len - 1), size - len + 1, "\n");
	pr_info("%s", buf);

	free_pages((unsigned long)buf, 0);

	return 0;

err_exit:
	for (i = 0; i < DIV_ROUND_UP(ARRAY_SIZE(hw_irq_name_1x88), 32); i++)
		pr_info("INTReg_%d: 0x%x\n", i,
				__raw_readl(irq_stat_base + i * 4));
	return -EINVAL;
}

static int check_mfp_wakeup_stat(char *buf, int len, size_t size)
{
	int i, bit, tmp_len = len;
	int mfp_edge_array_num;
	unsigned long *mfp_wp_stat;

	mfp_wp_stat = (unsigned long *)__get_free_pages(GFP_ATOMIC, 0);

	if (!mfp_wp_stat) {
		pr_err("memory alloc for mfp wakeup check is failed!!\n");
		return -EINVAL;
	}

	mfp_edge_array_num = edge_wakeup_mfp_status(mfp_wp_stat);

	for (i = 0; i < mfp_edge_array_num; i++) {
		bit = find_first_bit(&mfp_wp_stat[i], 32);
		while (bit < 32) {
			if (tmp_len == len)
				len += snprintf(buf + len, size - len, "MFP_PAD");
			len += snprintf(buf + len, size - len, "-%d", bit + i * 32);
			bit = find_next_bit(&mfp_wp_stat[i], 32, bit + 1);
		}
	}

	free_pages((unsigned long)mfp_wp_stat, 0);

	if (tmp_len != len) {
		len += snprintf(buf + len, size - len, ",");
		return len;
	} else
		return -EINVAL;
}

static u32 wakeup_source_check(void)
{
	char *buf;
	size_t size = PAGE_SIZE - 1;
	int len_s = 0, len = 0;
	u32 awucrm = __raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRM);
	u32 cwucrm = __raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + CWUCRM);

	u32 awucrs = __raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + AWUCRS);
	u32 cwucrs = __raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + CWUCRS);

	u32 apcr = __raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + APCR);
	u32 cpcr = __raw_readl(regs_addr_get_va(REGS_ADDR_MPMU) + CPCR);

	u32 sav_wucrs = awucrs | cwucrs;
	u32 sav_wucrm = awucrm | cwucrm;
	u32 sav_xpcr = apcr | cpcr;

	u32 real_wus = sav_wucrs & sav_wucrm & PMUM_AP_WAKEUP_MASK;

	pr_info("After SUSPEND");
	pr_info("AWUCRS:0x%x, CWUCRS:0x%x\n", awucrs, cwucrs);
	pr_info("General wakeup source status:0x%x\n", sav_wucrs);
	pr_info("AWUCRM:0x%x, CWUCRM:0x%x\n", awucrm, cwucrm);
	pr_info("General wakeup source mask:0x%x\n", sav_wucrm);
	pr_info("APCR:0x%x, CPCR:0x%x\n", apcr, cpcr);
	pr_info("General xPCR :0x%x\n", sav_xpcr);

	buf = (char *)__get_free_pages(GFP_ATOMIC, 0);
	if (!buf) {
		pr_err("memory alloc for wakeup check is failed!!\n");
		return sav_wucrs;
	}

	len_s = len = snprintf(buf, size, "AP Sub system is woken up by:");
	if (real_wus & (PMUM_WAKEUP0))
		len += snprintf(buf + len, size - len, "GSM,");
	if (real_wus & (PMUM_WAKEUP1))
		len += snprintf(buf + len, size - len, "3G Base band,");
	if (real_wus & PMUM_WAKEUP2) {
		int ret = check_mfp_wakeup_stat(buf, len, size);
		if (ret < 0)
			pr_info("MFP pad wakeup check failed!!!\n");
		else
			len = ret;
	}
	if (real_wus & PMUM_WAKEUP3) {
		if (real_wus & PMUM_KEYPRESS)
			len += snprintf(buf + len, size - len, "KeyPress,");
		if (real_wus & PMUM_TRACKBALL)
			len += snprintf(buf + len, size - len, "TRACKBALL,");
		if (real_wus & PMUM_NEWROTARY)
			len += snprintf(buf + len, size - len, "NEWROTARY,");
	}
	if (real_wus & PMUM_WAKEUP4) {
		if (real_wus & PMUM_WDT)
			len += snprintf(buf + len, size - len, "WDT,");
		if (real_wus & PMUM_RTC_ALARM)
			len += snprintf(buf + len, size - len, "RTC_ALARM,");
		if (real_wus & PMUM_CP_TIMER_3)
			len += snprintf(buf + len, size - len, "CP_TIMER_3,");
		if (real_wus & PMUM_CP_TIMER_2)
			len += snprintf(buf + len, size - len, "CP_TIMER_2,");
		if (real_wus & PMUM_CP_TIMER_1)
			len += snprintf(buf + len, size - len, "CP_TIMER_1,");
		if (real_wus & PMUM_AP1_TIMER_3)
			len += snprintf(buf + len, size - len, "AP1_TIMER_3,");
		if (real_wus & PMUM_AP1_TIMER_2)
			len += snprintf(buf + len, size - len, "AP1_TIMER_2,");
		if (real_wus & PMUM_AP1_TIMER_1)
			len += snprintf(buf + len, size - len, "AP1_TIMER_1,");
		if (real_wus & PMUM_AP0_2_TIMER_3)
			len += snprintf(buf + len, size - len,
				"AP0_2_TIMER_3,");
		if (real_wus & PMUM_AP0_2_TIMER_2)
			len += snprintf(buf + len, size - len,
				"AP0_2_TIMER_2,");
		if (real_wus & PMUM_AP0_2_TIMER_1)
			len += snprintf(buf + len, size - len,
				"AP0_2_TIMER_1,");
	}
	if (real_wus & PMUM_WAKEUP5)
		len += snprintf(buf + len, size - len, "USB,");
	if (real_wus & PMUM_WAKEUP6) {
		if (real_wus & PMUM_SQU_SDH1)
			len += snprintf(buf + len, size - len, "SQU_SDH1,");
		if (real_wus & PMUM_SDH_23)
			len += snprintf(buf + len, size - len, "SDH_23,");
	}
	if (real_wus & PMUM_WAKEUP7)
		len += snprintf(buf + len, size - len, "PMIC,");

	if (len_s != len) {
		snprintf(buf + (len - 1), size - len + 1, "\n");
		pr_info("%s", buf);
	} else {
		if (real_wus & PMUM_AP_ASYNC_INT) {
			pr_info("AP is woken up by AP_INT_ASYNC\n");
			if (icu_async_further_check())
				pr_err("Cannot interpretate the detailed wakeup reason!\n");
		} else {
			void __iomem *gic_dist_base = regs_addr_get_va(REGS_ADDR_GIC_DIST);
			u32 reg = readl_relaxed(gic_dist_base + GIC_DIST_PENDING_SET);
			if (reg) {
				pr_info("!!!!!! GIC got Pending PPI/IPI Before Suspend !!!!!!\n");
				pr_info("           PPI/IPI isPending Reg value: 0x%x\n", reg);
				pr_info("!!!!!!    This cause suspend entry failure  !!!!!!\n");
				WARN_ON(1);
			} else
				pr_err("Unexpected Events Wakeup AP!\n");
		}
	}

	free_pages((unsigned long)buf, 0);

	return sav_wucrs;
}

static u32 pxa1L88_post_chk_wakeup(void)
{
	int ret;
#ifdef CONFIG_ARM_ERRATA_802022
#ifndef CONFIG_TZ_HYPERVISOR
	writel(__pa(mcpm_entry_point), ciu_warm_reset_vector);
#endif
	{
		u32 mp_idle_cfg;
		int i;
		for_each_possible_cpu(i) {
			mp_idle_cfg = __raw_readl(APMU_MP_IDLE_CFG[i]);
			mp_idle_cfg |= (PMUA_DIS_MP_SLP);
			__raw_writel(mp_idle_cfg, APMU_MP_IDLE_CFG[i]);
		}
	}
#endif
	ret = wakeup_source_check();

	pr_info("=======================================\n");
	return ret;
}

static struct suspend_ops pxa1L88_suspend_ops = {
	.pre_suspend_check = pxa1L88_suspend_check,
	.post_chk_wakeup = pxa1L88_post_chk_wakeup,
	.post_clr_wakeup = NULL,
	.set_wake = pxa1L88_set_wake,
	.plt_suspend_init = pxa1L88_plt_suspend_init,
};

static struct platform_suspend pxa1L88_suspend = {
	.suspend_state	= POWER_MODE_UDR,
	.ops		= &pxa1L88_suspend_ops,
};

static int __init pxa1L88_suspend_init(void)
{
	int ret;
	ret = mmp_platform_suspend_register(&pxa1L88_suspend);

	if (ret)
		WARN_ON("PXA1L88 Suspend Register fails!!");

	return 0;
}
late_initcall(pxa1L88_suspend_init);
