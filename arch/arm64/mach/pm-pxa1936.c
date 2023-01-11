/*
 * arch/arm64/mach/pm-pxa1936.c
 *
 * Author:	Xiaoguang Chen <chenxg@marvell.com>
 *
 * Copyright:	(C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/cputype.h>
#include <linux/pm_qos.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/gfp.h>
#include <linux/pxa1936_powermode.h>
#include <linux/clk/mmpfuse.h>
#include "pxa1936_lowpower.h"
#include "pm.h"
#include "regs-addr.h"

/* FIXME: The following Macro will be refined by DT in future
 * when Most of the Devices is configured in DT way.
 */
#define IRQ_PXA1936_START		32
#define IRQ_PXA1936_PMIC		(IRQ_PXA1936_START + 4)
#define IRQ_PXA1936_RTC_ALARM		(IRQ_PXA1936_START + 6)
#define IRQ_PXA1936_KEYPAD		(IRQ_PXA1936_START + 9)

#define IRQ_PXA1936_AP0_TIMER1		(IRQ_PXA1936_START + 13)
#define IRQ_PXA1936_AP0_TIMER2		(IRQ_PXA1936_START + 14)
#define IRQ_PXA1936_AP0_TIMER3		(IRQ_PXA1936_START + 2)

#define IRQ_PXA1936_AP1_TIMER1		(IRQ_PXA1936_START + 29)
#define IRQ_PXA1936_AP1_TIMER2		(IRQ_PXA1936_START + 30)
#define IRQ_PXA1936_AP1_TIMER3		(IRQ_PXA1936_START + 46)

#define IRQ_PXA1936_AP2_TIMER1		(IRQ_PXA1936_START + 64)
#define IRQ_PXA1936_AP2_TIMER2		(IRQ_PXA1936_START + 65)
#define IRQ_PXA1936_AP2_TIMER3		(IRQ_PXA1936_START + 88)

#define IRQ_PXA1936_MMC			(IRQ_PXA1936_START + 39)
#define IRQ_PXA1936_USB1		(IRQ_PXA1936_START + 44)
#define IRQ_PXA1936_GPIO_AP		(IRQ_PXA1936_START + 49)
#define IRQ_PXA1936_GPS			(IRQ_PXA1936_START + 95)

static void pxa1936_set_wake(int irq, unsigned int on)
{
	uint32_t awucrm = 0;
	uint32_t apbc_timer0;
	void __iomem *apbc = regs_addr_get_va(REGS_ADDR_APBC);
	void __iomem *mpmu = regs_addr_get_va(REGS_ADDR_MPMU);

	/* setting wakeup sources */
	switch (irq) {
		/* wakeup line 2 */
	case IRQ_PXA1936_GPIO_AP:
		awucrm = PMUM_WAKEUP2;
		break;
		/* wakeup line 3 */
	case IRQ_PXA1936_KEYPAD:
		awucrm = PMUM_WAKEUP3 | PMUM_KEYPRESS | PMUM_TRACKBALL |
		    PMUM_NEWROTARY;
		break;
		/* wakeup line 4 */
	case IRQ_PXA1936_AP0_TIMER1:
		apbc_timer0 = __raw_readl(apbc + TIMER0);
		__raw_writel(0x1 << 7 | apbc_timer0, apbc + TIMER0);
		awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_1;
		break;
	case IRQ_PXA1936_AP0_TIMER2:
		apbc_timer0 = __raw_readl(apbc + TIMER0);
		__raw_writel(0x1 << 7 | apbc_timer0, apbc + TIMER0);
		awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_2;
		break;
	case IRQ_PXA1936_AP0_TIMER3:
		apbc_timer0 = __raw_readl(apbc + TIMER0);
		__raw_writel(0x1 << 7 | apbc_timer0, apbc + TIMER0);
		awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_3;
		break;
	case IRQ_PXA1936_AP1_TIMER1:
		awucrm = PMUM_WAKEUP4 | PMUM_AP1_TIMER_1;
		break;
	case IRQ_PXA1936_AP1_TIMER2:
		awucrm = PMUM_WAKEUP4 | PMUM_AP1_TIMER_2;
		break;
	case IRQ_PXA1936_AP1_TIMER3:
		awucrm = PMUM_WAKEUP4 | PMUM_AP1_TIMER_3;
		break;
	case IRQ_PXA1936_AP2_TIMER1:
		awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_1;
		break;
	case IRQ_PXA1936_AP2_TIMER2:
		awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_2;
		break;
	case IRQ_PXA1936_AP2_TIMER3:
		awucrm = PMUM_WAKEUP4 | PMUM_AP0_2_TIMER_3;
		break;
	case IRQ_PXA1936_RTC_ALARM:
		awucrm = PMUM_WAKEUP4 | PMUM_RTC_ALARM;
		break;
		/* wakeup line 5 */
	case IRQ_PXA1936_USB1:
	case IRQ_PXA1936_GPS:
		awucrm = PMUM_WAKEUP5;
		break;
		/* wakeup line 6 */
	case IRQ_PXA1936_MMC:
		awucrm = PMUM_WAKEUP6 | PMUM_SDH_23 | PMUM_SQU_SDH1;
		break;
		/* wakeup line 7 */
	case IRQ_PXA1936_PMIC:
		awucrm = PMUM_WAKEUP7;
		break;
	default:
		/* do nothing */
		break;
	}
	if (on) {
		if (awucrm) {
			awucrm |= __raw_readl(mpmu + AWUCRM);
			__raw_writel(awucrm, mpmu + AWUCRM);
		}
	} else {
		if (awucrm) {
			awucrm = ~awucrm & __raw_readl(mpmu + AWUCRM);
			__raw_writel(awucrm, mpmu + AWUCRM);
		}
	}
}

static void pxa1936_plt_suspend_init(void)
{
	u32 awucrm = 0;
	void __iomem *mpmu = regs_addr_get_va(REGS_ADDR_MPMU);

	awucrm = __raw_readl(mpmu + AWUCRM);
	awucrm |= (PMUM_AP_ASYNC_INT | PMUM_AP_FULL_IDLE);

	__raw_writel(awucrm, mpmu + AWUCRM);
}

static int pxa1936_pm_check_constraint(void)
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
		if (node->prio == PM_QOS_CPUIDLE_BLOCK_UDR) {
			pr_info("****pm_check_constraint PM_QOS_CPUIDLE_BLOCK_UDR %s\n",
				req->name);
		} else if (node->prio != PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE) {
			pr_info("****************************************\n");
			pr_err("%s lpm constraint before Suspend", req->name);
			pr_info("*****************************************\n");
			ret = -EBUSY;
		}
	}

	return ret;
}

static void check_pwrmode_status(unsigned int pwrmode_status)
{
	if ((pwrmode_status >> PWRMODE_STATUS_OFFSET) & D2PP)
		pr_info("D2 entered!\n");
	else
		pr_info("D2 not entered!\n");
}

static void check_sp_status(unsigned int core_status)
{
	if (core_status & SP_IDLE)
		pr_info("SP idle!\n");
	else
		pr_info("SP not idle!\n");
}

static void check_cp_status(unsigned int cpsr)
{
	if ((cpsr >> CP_STATUS_OFFSET) == CP_IDLE_VALUE)
		pr_info("CP idle!\n");
	else
		pr_info("CP not idle!\n");
}

static void check_gps_status(unsigned int pmua_pwr_status)
{
	if (pmua_pwr_status & GNSS_SD_PWR_STAT)
		pr_info("GPS power on!\n");
	else
		pr_info("GPS power off!\n");
}

static int pxa1936_suspend_check(void)
{
	u32 ret, reg, irq, cpsr, core_status, pwr_status;
	void __iomem *icu_base = regs_addr_get_va(REGS_ADDR_ICU);
	void __iomem *mpmu_base = regs_addr_get_va(REGS_ADDR_MPMU);
	void __iomem *apmu_base = regs_addr_get_va(REGS_ADDR_APMU);

	cpsr = __raw_readl(mpmu_base + CPSR);
	core_status = __raw_readl(apmu_base + CORE_STATUS);
	pwr_status = __raw_readl(apmu_base + PWR_STATUS);

	irq = IRQ_PXA1936_PMIC - IRQ_PXA1936_START;
	reg = __raw_readl(icu_base + (irq << 2));

	/*
	 * check PMIC interrupt config before entering suspend. We
	 * must ensure the last time PMIC interrupt handler is done
	 * and the PMIC interrupt is enabled before entering suspend,
	 * other wise, the suspend may NOT be woken up any more.
	 */
	if ((reg & 0x3) == 0) {
		WARN_ONCE(1, "PMIC interrupt is not enabled, quit suspend!\n");
		return -EAGAIN;
	}

	ret = pxa1936_pm_check_constraint();
	__raw_writel(0xFFFF, mpmu_base + PWRMODE_STATUS);
	pr_info("========wake up events status =========\n");
	pr_info("BEFORE SUSPEND:\n");
	pr_info("AWUCRS:0x%x, PWRMODE_STATUS: 0x%x\n",
		__raw_readl(mpmu_base + AWUCRS),
		__raw_readl(mpmu_base + PWRMODE_STATUS));
	pr_info("CORE_STATUS: 0x%x, CPSR: 0x%x, PWR_STATUS: 0x%x\n",
		core_status, cpsr, pwr_status);
	check_sp_status(core_status);
	check_cp_status(cpsr);
	check_gps_status(pwr_status);

	return ret;
}

static char *hw_irq_name_1936[] = {
	"AIRQ - IRQ",
	"SSP2",
	"AP_Timer0_3",
	"SSP0",
	"PMIC_INT",
	"RTC_INT",
	"RTC_ ALARM",
	"I2C (AP)",
	"GC1000T",
	"New Rotarey or Trackball or KeyPad",
	"DMA_int to Cluster 0 APc1",
	"OV ISP Engine",
	"Battery Monitor (onewire)",
	"AP_Timer0_1",
	"AP_Timer0_2",
	"ISP DESC TOP",
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
	"AP_Timer1_1",
	"AP_Timer1_2",
	"CP_Timer1",
	"CP_Timer2 or CP_Timer3",
	"i2c_int",
	"GSSP (PCM on MSA)",
	"WDT",
	"Main PMU Int",
	"CA53MP Frequency change Int",
	"Seagull Frequency change Int",
	"MMC",
	"AEU",
	"LCD Interface",
	"CI Interface",
	"DDRC Performance Counter",
	"USB 1 (PHY)",
	"NAND",
	"AP_Timer1_3",
	"DMA_int to CP",
	"DMA_int to Cluster0 APc0",
	"GPIO_AP",
	"pad_edge_detect",
	"SPH (ULPI)",
	"IPC_SRV0_Seagull",
	"DSI",
	"I2C (CP-slow)",
	"GPIO_CP",
	"IPC_SRV0_AP",
	"performance_counter_cp",
	"coresight int for AP Cluster0 core0",
	"UART0 (Slow), CP",
	"DRO and temp sensor int",
	"coresight int for AP Cluster0 core1",
	"Fabric  timeout",
	"SM_INT (from PinMux)",
	"AP_Timer2_1",
	"AP_Timer2_2",
	"coresight int for AP Cluster0 core2",
	"DMA_int to Cluster0 APc2",
	"coresight int for AP Cluster0 core3",
	"DMA_int to Cluster0 APc3",
	"AP_SSP1",
	"AP_SSP2",
	"GC320",
	"DMA_secure_int to Cluster0 APc0",
	"DMA_secure_int to Cluster0 APc1",
	"DMA_secure_int to Cluster0 APc2",
	"DMA_secure_int to Cluster0 APc3",
	"ipe2",
	"mmu_vpu_irq_ns",
	"mmu_vpu_irq_s",
	"DMA_int to SP",
	"VDMA IRQ",
	"Audio ADMA0",
	"Audio ADMA1",
	"Performance Monitor Unit for AP Cluster0 core0",
	"Performance Monitor Unit for AP Cluster0 core1",
	"Performance Monitor Unit for AP Cluster0 core2",
	"Performance Monitor Unit for AP Cluster0 core3",
	"AP_Timer2_3",
	"Audio Island",
	"GPIO secure int for AP",
	"WTM HST interrupt",
	"WTM SP interrupt",
	"TWSI 2 interrupt",
	"eMMC",
	"GPS interrupt",
	"DMA_secure_int to Cluster1 APc0",
	"DMA_secure_int to Cluster1 APc1",
	"DMA_secure_int to Cluster1 APc2",
	"DMA_secure_int to Cluster1 APc3",
	"DMA_int to Cluster1 APc0",
	"DMA_int to Cluster1 APc1",
	"DMA_int to Cluster1 APc2",
	"DMA_int to Cluster1 APc3",
	"coresight int for AP Cluster1 core0",
	"coresight int for AP Cluster1 core1",
	"coresight int for AP Cluster1 core2",
	"coresight int for AP Cluster1 core3",
	"Performance Monitor Unit for AP Cluster1 core0",
	"Performance Monitor Unit for AP Cluster1 core1",
	"Performance Monitor Unit for AP Cluster1 core2",
	"Performance Monitor Unit for AP Cluster1 core3",
};

static int icu_async_further_check(void)
{
	int i, bit, len = 0;
	void __iomem *irq_stat_base =
	    regs_addr_get_va(REGS_ADDR_ICU) + ICU_INT_STATUS_BASE;
	unsigned long irq_status;
	char *buf;
	size_t size = PAGE_SIZE - 1;

	buf = (char *)__get_free_pages(GFP_ATOMIC, 0);
	if (!buf) {
		pr_err("memory alloc for wakeup check is failed!!\n");
		goto err_exit;
	}

	for (i = 0; i < DIV_ROUND_UP(ARRAY_SIZE(hw_irq_name_1936), 32); i++) {
		irq_status = __raw_readl(irq_stat_base + i * 4);
		pr_info("INTReg_%d: 0x%lx ", i, irq_status);

		bit = find_first_bit(&irq_status, 32);
		while (bit < 32) {
			if (!len)
				len = snprintf(buf + len, size - len,
					       "Possible Wakeup by IRQ: ");

			if ((bit + i * 32) < ARRAY_SIZE(hw_irq_name_1936))
				len += snprintf(buf + len, size - len, "%s,",
						hw_irq_name_1936[bit + i * 32]);
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
	for (i = 0; i < DIV_ROUND_UP(ARRAY_SIZE(hw_irq_name_1936), 32); i++)
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
				len +=
				    snprintf(buf + len, size - len, "MFP_PAD");
			len +=
			    snprintf(buf + len, size - len, "-%d",
				     bit + i * 32);
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
	void __iomem *mpmu = regs_addr_get_va(REGS_ADDR_MPMU);
	void __iomem *apmu = regs_addr_get_va(REGS_ADDR_APMU);

	u32 awucrm = __raw_readl(mpmu + AWUCRM);
	u32 cwucrm = __raw_readl(mpmu + CWUCRM);

	u32 awucrs = __raw_readl(mpmu + AWUCRS);
	u32 cwucrs = __raw_readl(mpmu + CWUCRS);

	u32 apcr_cluster0 = __raw_readl(mpmu + APCR_CLUSTER0);
	u32 apcr_cluster1 = __raw_readl(mpmu + APCR_CLUSTER1);
	u32 apcr_per = __raw_readl(mpmu + APCR_PER);
	u32 apslpw = __raw_readl(mpmu + APSLPW);
	u32 cpcr = __raw_readl(mpmu + CPCR);
	u32 cpsr = __raw_readl(mpmu + CPSR);
	u32 pwrmode_status = __raw_readl(mpmu + PWRMODE_STATUS);
	u32 core_status = __raw_readl(apmu + CORE_STATUS);
	u32 pwr_status = __raw_readl(apmu + PWR_STATUS);

	u32 sav_wucrs = awucrs | cwucrs;
	u32 sav_wucrm = awucrm | cwucrm;
	u32 sav_xpcr = apcr_cluster0 | apcr_cluster1 | apcr_per | cpcr;
	u32 sav_slpw = apslpw | cpcr;
	u32 real_wus = sav_wucrs & sav_wucrm & PMUM_AP_WAKEUP_MASK;

	pr_info("After SUSPEND");
	pr_info("PWRMODE_STAUTS: 0x%x\n", pwrmode_status);
	pr_info("CORE_STATUS: 0x%x, CPSR: 0x%x, PWR_STATUS: 0x%x\n",
		core_status, cpsr, pwr_status);

	check_pwrmode_status(pwrmode_status);
	check_sp_status(core_status);
	check_cp_status(cpsr);
	check_gps_status(pwr_status);

	pr_info("AWUCRS:0x%x, CWUCRS:0x%x\n", awucrs, cwucrs);
	pr_info("General wakeup source status:0x%x\n", sav_wucrs);
	pr_info("AWUCRM:0x%x, CWUCRM:0x%x\n", awucrm, cwucrm);
	pr_info("General wakeup source mask:0x%x\n", sav_wucrm);
	pr_info("APCR_CLUSTER0:0x%x, APCR_CLUSTER1:0x%x, APCR_PER:0x%x\n",
		apcr_cluster0, apcr_cluster0, apcr_per);
	pr_info("APSLPW: 0x%x\n", apslpw);
	pr_info("CPCR:0x%x\n", cpcr);
	pr_info("General xPCR :0x%x\n", sav_xpcr);
	pr_info("General xSLPW :0x%x\n", sav_slpw);

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
		len += snprintf(buf + len, size - len, "USB or CM3,");
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
	}

	if (real_wus & PMUM_AP_ASYNC_INT) {
		pr_info("AP is woken up by AP_INT_ASYNC\n");
		if (icu_async_further_check())
			pr_err("Cannot interpret detailed wakeup reason!\n");
	} else {
		void __iomem *gic_dist_base =
		    regs_addr_get_va(REGS_ADDR_GIC_DIST);
		u32 reg = readl_relaxed(gic_dist_base + GIC_DIST_PENDING_SET);
		if (reg) {
			pr_info("GIC got Pending PPI/IPI Before Suspend!\n");
			pr_info("PPI/IPI isPending Reg value: 0x%x\n", reg);
			pr_info("This cause suspend entry failure!!!!!!\n");
			WARN_ON(1);
		} else
			pr_err("Unexpected Events Wakeup AP!\n");
	}

	free_pages((unsigned long)buf, 0);

	return sav_wucrs;
}

static u32 pxa1936_post_chk_wakeup(void)
{
	int ret;
	ret = wakeup_source_check();

	pr_info("=======================================\n");
	return ret;
}

static int pxa1936_get_suspend_volt(void)
{
	unsigned int volt_fuseinfo = 0;

	volt_fuseinfo = get_skusetting();

	switch (volt_fuseinfo) {
	case 0x1:
		return 700000;
	case 0x0:
	case 0x2:
		return 800000;
	default:
		return 800000;
	}
}

static struct suspend_ops pxa1936_suspend_ops = {
	.pre_suspend_check = pxa1936_suspend_check,
	.post_chk_wakeup = pxa1936_post_chk_wakeup,
	.post_clr_wakeup = NULL,
	.set_wake = pxa1936_set_wake,
	.plt_suspend_init = pxa1936_plt_suspend_init,
	.get_suspend_voltage = pxa1936_get_suspend_volt,
};

static struct platform_suspend pxa1936_suspend = {
	.suspend_state = POWER_MODE_UDR,
	.ops = &pxa1936_suspend_ops,
};

static int __init pxa1936_suspend_init(void)
{
	int ret;

	if (!of_machine_is_compatible("marvell,pxa1936"))
		return -ENODEV;
	ret = mmp_platform_suspend_register(&pxa1936_suspend);

	if (ret)
		WARN_ON("PXA1936 Suspend Register fails!!");

	return 0;
}
late_initcall(pxa1936_suspend_init);
