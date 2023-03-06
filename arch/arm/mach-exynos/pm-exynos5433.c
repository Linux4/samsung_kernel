/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Management support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/wakeup_reason.h>
#include <linux/delay.h>

#include <asm/cacheflush.h>
#include <asm/smp_scu.h>
#include <asm/cputype.h>
#include <asm/firmware.h>

#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/pll.h>
#include <plat/regs-srom.h>

#include <mach/regs-irq.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-clock-exynos5433.h>
#include <mach/regs-pmu.h>
#include <mach/pm-core.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/pmu_cal_sys.h>

#define REG_INFORM0			(S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1			(S5P_VA_SYSRAM_NS + 0xC)

#define EXYNOS_WAKEUP_STAT_EINT		(1 << 0)
#define EXYNOS_WAKEUP_STAT_RTC_ALARM	(1 << 1)

static struct sleep_save exynos5433_set_clk[] = {
	{ .reg = EXYNOS5430_ENABLE_IP_FSYS0,		.val = 0x00007DF9, },
	{ .reg = EXYNOS5430_ENABLE_IP_PERIC0,		.val = 0xFFFFFEFF, },
	{ .reg = EXYNOS5430_SRC_SEL_TOP_PERIC1,		.val = 0x00000011, },
	{ .reg = EXYNOS5430_SRC_SEL_FSYS0,		.val = 0x00000000, },
	{ .reg = EXYNOS5430_SRC_SEL_BUS2,		.val = 0x00000000, },
	{ .reg = EXYNOS5430_ENABLE_SCLK_FSYS,		.val = 0x00000000, },
	{ .reg = EXYNOS5430_ENABLE_IP_CPIF0,		.val = 0x000FF000, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_TOP,		.val = 0x67E8FFED, },
	{ .reg = EXYNOS5430_ENABLE_ACLK_MIF3,		.val = 0x00000003, },
};

static struct sleep_save exynos_enable_xxti[] = {
	{ .reg = EXYNOS5_XXTI_SYS_PWR_REG,		.val = 0x1, },
};

static void __iomem *exynos_eint_base;
extern u32 exynos_eint_to_pin_num(int eint);

static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int i, size;
	long unsigned int ext_int_pend;
	u64 eint_wakeup_mask, eint_wakeup_mask1;
	u64 eintmask;
	bool found = 0;

	eint_wakeup_mask = __raw_readl(EXYNOS5433_EINT_WAKEUP_MASK);
	eint_wakeup_mask1 = __raw_readl(EXYNOS5433_EINT_WAKEUP_MASK1);
	eintmask = eint_wakeup_mask | (eint_wakeup_mask1 << 32);

	for (i = 0, size = 8; i < 64; i += size) {
		if (i < 32)
			ext_int_pend =
				__raw_readl(EXYNOS543x_EINT_PEND(exynos_eint_base, i));
		else
			ext_int_pend =
				__raw_readl(EXYNOS5433_EINT_PEND1(exynos_eint_base,
							i - 32));

		(i >= 40 && i < 48) ? (size = 4) : (size = 8);

		for_each_set_bit(bit, &ext_int_pend, size) {
			u32 gpio;
			int irq;

			if (eintmask & (1 << (i + bit)))
				continue;

			gpio = exynos_eint_to_pin_num(i + bit);
			irq = gpio_to_irq(gpio);

			log_wakeup_reason(irq);
			update_wakeup_reason_stats(irq, i + bit);
			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}

#ifdef CONFIG_SEC_PM_DEBUG
static void exynos_show_wakeup_registers(unsigned long wakeup_stat)
{
	pr_info("WAKEUP_STAT: 0x%08lx\n", wakeup_stat);
	pr_info("EINT_PEND: 0x%02x, 0x%02x 0x%02x, 0x%02x\n",
			__raw_readl(EXYNOS543x_EINT_PEND(exynos_eint_base, 0)),
			__raw_readl(EXYNOS543x_EINT_PEND(exynos_eint_base, 8)),
			__raw_readl(EXYNOS543x_EINT_PEND(exynos_eint_base, 16)),
			__raw_readl(EXYNOS543x_EINT_PEND(exynos_eint_base, 24)));
	pr_info("EINT_PEND1: 0x%02x, 0x%02x 0x%02x, 0x%02x, 0x%02x\n",
			__raw_readl(EXYNOS5433_EINT_PEND1(exynos_eint_base, 0)),
			__raw_readl(EXYNOS5433_EINT_PEND1(exynos_eint_base, 8)),
			__raw_readl(EXYNOS5433_EINT_PEND1(exynos_eint_base, 12)),
			__raw_readl(EXYNOS5433_EINT_PEND1(exynos_eint_base, 16)),
			__raw_readl(EXYNOS5433_EINT_PEND1(exynos_eint_base, 24)));
}
#else
static void exynos_show_wakeup_registers(unsigned long wakeup_stat) {}
#endif

static void exynos_show_wakeup_reason(void)
{
	unsigned long wakeup_stat;

	wakeup_stat = __raw_readl(EXYNOS5433_WAKEUP_STAT);

	exynos_show_wakeup_registers(wakeup_stat);

	if (wakeup_stat & EXYNOS_WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	else if (wakeup_stat & EXYNOS_WAKEUP_STAT_EINT)
		exynos_show_wakeup_reason_eint();
	else
		pr_info("Resume caused by wakeup_stat=0x%08lx\n",
			wakeup_stat);
}

#ifdef CONFIG_SEC_GPIO_DVS
extern void gpio_dvs_check_sleepgpio(void);
#endif

#ifdef CONFIG_PCI_EXYNOS
extern int exynos_fsys_power_on(void);
extern int exynos_fsys_power_off(void);
extern int check_rev(void);
int pcie_suspend_ok = 0;
#endif

static int exynos_cpu_suspend(unsigned long arg)
{
#ifdef CONFIG_SEC_GPIO_DVS
	/************************ Caution !!! ****************************/
	/* This function must be located in appropriate SLEEP position
	 * in accordance with the specification of each BB vendor.
	 */
	/************************ Caution !!! ****************************/
	gpio_dvs_check_sleepgpio();
#endif
	flush_cache_all();

	if (call_firmware_op(do_idle))
		cpu_do_idle();
	pr_info("sleep resumed to originator?");

#ifdef CONFIG_PCI_EXYNOS
	if(!check_rev()) {
		/* If happen early wakeu, it is need to reset FSYS */
		exynos_fsys_power_off();
		mdelay(1);
		exynos_fsys_power_on();
	}
 
 	pr_info("not enter suspend fully\n");
#endif

	return 1; /* abort suspend */
}

static int exynos_pm_suspend(void)
{
	/* Setting Central Sequence Register for power down mode */
	exynos_central_sequencer_ctrl(true);

	return 0;
}

#ifdef CONFIG_PCI_EXYNOS
extern void exynos_pcie_suspend(void);
extern void exynos_pcie_resume(void);
#endif

static void exynos_pm_prepare(void)
{
#ifdef CONFIG_PCI_EXYNOS
	if (!check_rev()) {
		exynos_pcie_suspend();
		pcie_suspend_ok = 1;
	}
#endif

	/* Set value of power down register for sleep mode */
	exynos_sys_powerdown_conf(SYS_SLEEP);

	__raw_writel(EXYNOS_CHECK_SLEEP, REG_INFORM1);
	/* ensure at least INFORM0 has the resume address */
	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_INFORM0);

	if (!(__raw_readl(EXYNOS_PMU_DEBUG) & 0x1))
		s3c_pm_do_restore_core(exynos_enable_xxti,
				ARRAY_SIZE(exynos_enable_xxti));

	/* For Only exynos5433, BLK_FSYS is on */
	if (__raw_readl(EXYNOS5433_FSYS_STATUS) == 0x0) {
		__raw_writel(EXYNOS_PWR_EN, EXYNOS5433_FSYS_CONFIGURATION);
		do {
			if ((__raw_readl(EXYNOS5433_FSYS_STATUS) == EXYNOS_PWR_EN))
				break;
		} while (1);
	}

	/*
	 * Before enter central sequence mode,
	 * clock register have to set.
	 */
	s3c_pm_do_restore_core(exynos5433_set_clk, ARRAY_SIZE(exynos5433_set_clk));
}

static void exynos_pm_resume(void)
{
	unsigned long tmp;

	/*
	 * If PMU failed while entering sleep mode, WFI will be
	 * ignored by PMU and then exiting cpu_do_idle().
	 * S5P_CENTRAL_LOWPWR_CFG bit will not be set automatically
	 * in this situation.
	 */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	if (!(tmp & EXYNOS_CENTRAL_LOWPWR_CFG)) {
		tmp |= EXYNOS_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);
		/* No need to perform below restore code */
		goto early_wakeup;
	}

	/* Need to set again because LPI MASK is reset after wakeup */
	exynos_eagle_asyncbridge_ignore_lpi();

	/* For release retention */
	__raw_writel((1 << 28), EXYNOS_PAD_RET_DRAM_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_JTAG_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_MMC2_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_TOP_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_MMC0_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_MMC1_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_EBIB_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_SPI_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_MIF_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_USBXTI_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_BOOTLDO_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_UFS_OPTION);
	__raw_writel((1 << 28), EXYNOS5433_PAD_RETENTION_FSYSGENIO_OPTION);

early_wakeup:
#ifdef CONFIG_PCI_EXYNOS
	if (!check_rev()) {
		if (pcie_suspend_ok) {
			pcie_suspend_ok = 0;
			exynos_pcie_resume();
		}
 	}
#endif
	exynos_show_wakeup_reason();

	return;
}

static int exynos_pm_add(struct device *dev, struct subsys_interface *sif)
{
	pm_cpu_prep = exynos_pm_prepare;
	pm_cpu_sleep = exynos_cpu_suspend;

	return 0;
}

static struct subsys_interface exynos_pm_interface = {
	.name		= "exynos_pm",
	.subsys		= &exynos_subsys,
	.add_dev	= exynos_pm_add,
};

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_suspend,
	.resume		= exynos_pm_resume,
};

static __init int exynos_pm_drvinit(void)
{
	s3c_pm_init();

	return subsys_interface_register(&exynos_pm_interface);
}
arch_initcall(exynos_pm_drvinit);

static __init int exynos_pm_syscore_init(void)
{
	exynos_eint_base = ioremap(EXYNOS543x_PA_GPIO_ALIVE, SZ_8K);

	if (exynos_eint_base == NULL) {
		pr_err("%s: unable to ioremap for EINT base address\n",
				__func__);
		BUG();
	}

	register_syscore_ops(&exynos_pm_syscore_ops);
	return 0;
}
arch_initcall(exynos_pm_syscore_init);
