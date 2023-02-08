/* linux/arch/arm/mach-exynos4/include/mach/pm-core.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Based on arch/arm/mach-s3c2410/include/mach/pm-core.h,
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * EXYNOS4210 - PM core support for arch/arm/plat-s5p/pm.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_PM_CORE_H
#define __ASM_ARCH_PM_CORE_H __FILE__

#include <mach/regs-pmu.h>
#ifdef CONFIG_SEC_PM_DEBUG
#include <mach/regs-gpio.h>
#endif

static inline void s3c_pm_debug_init_uart(void)
{
	/* nothing here yet */
}

static inline void s3c_pm_arch_prepare_irqs(void)
{
	s3c_irqwake_intmask &= ~(1 << 31);

	/* Exynos5260 has different offset for EINT and WAKEUP Mask */
#ifdef CONFIG_SOC_EXYNOS5260
	if (soc_is_exynos5260()) {
		__raw_writel(s3c_irqwake_intmask, EXYNOS5260_WAKEUP_MASK1);
		__raw_writel(s3c_irqwake_eintmask, EXYNOS5260_EINT_WAKEUP_MASK);
	} else
#endif
	{
		__raw_writel(s3c_irqwake_intmask, EXYNOS_WAKEUP_MASK);
		__raw_writel(s3c_irqwake_eintmask, EXYNOS_EINT_WAKEUP_MASK);
	}
}

static inline void s3c_pm_arch_stop_clocks(void)
{
	/* nothing here yet */
}

static inline void s3c_pm_arch_show_resume_irqs(void)
{
	/* nothing here yet */
#ifdef CONFIG_SEC_PM_DEBUG
	pr_info("WAKEUP_STAT: 0x%x\n", __raw_readl(EXYNOS_WAKEUP_STAT));
	pr_info("WAKEUP_STAT2: 0x%x\n", __raw_readl(EXYNOS_WAKEUP_STAT2));
	pr_info("WAKEUP_STAT_COREBLK:  0x%x\n", __raw_readl(EXYNOS4X12_WAKEUP_STAT_COREBLK));
	pr_info("EXYNOS_RST_STAT: 0x%x\n", __raw_readl(EXYNOS_RST_STAT));
	pr_info("EXYNOS3470_CP_CTRL: 0x%x\n", __raw_readl(EXYNOS3470_CP_CTRL));
	pr_info("EXYNOS_ARM_CORE0_OPTION: 0x%x\n", __raw_readl(EXYNOS_ARM_CORE_OPTION(0)));
	pr_info("EXYNOS_ARM_CORE0_OPTION: 0x%x\n", __raw_readl(EXYNOS_ARM_CORE_OPTION(1)));
	pr_info("EXYNOS_ARM_CORE0_OPTION: 0x%x\n", __raw_readl(EXYNOS_ARM_CORE_OPTION(2)));
	pr_info("EXYNOS_ARM_CORE0_OPTION: 0x%x\n", __raw_readl(EXYNOS_ARM_CORE_OPTION(3)));
	pr_info("WAKEUP_INTx_PEND: 0x%x, 0x%x, 0x%x, 0x%x\n",
		__raw_readl(S5P_EINT_PEND(0)),
		__raw_readl(S5P_EINT_PEND(1)),
		__raw_readl(S5P_EINT_PEND(2)),
		__raw_readl(S5P_EINT_PEND(3)));
#endif
}

static inline void s3c_pm_arch_update_uart(void __iomem *regs,
					   struct pm_uart_save *save)
{
	/* nothing here yet */
}

static inline void s3c_pm_restored_gpios(void)
{
	/* nothing here yet */
}

static inline void samsung_pm_saved_gpios(void)
{
	/* nothing here yet */
}

#endif /* __ASM_ARCH_PM_CORE_H */
