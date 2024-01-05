/* linux/arch/arm/mach-exynos/pm-exynos3472.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Power Management support
 *
 * Based on arch/arm/mach-s3c2410/pm.c
 * Copyright (c) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/interrupt.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/smp_scu.h>
#include <asm/cputype.h>

#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/pll.h>
#include <plat/regs-srom.h>
#include <plat/bts.h>

#include <mach/regs-irq.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pm-core.h>
#include <mach/pmu.h>
#include <mach/smc.h>

#ifdef CONFIG_MACH_UNIVERSAL3472
void (*exynos_config_sleep_gpio)(void);
#endif

#ifdef CONFIG_ARM_TRUSTZONE
#define REG_INFORM0            (S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1            (S5P_VA_SYSRAM_NS + 0xC)
#else
#define REG_INFORM0            (EXYNOS_INFORM0)
#define REG_INFORM1            (EXYNOS_INFORM1)
#endif

#define EXYNOS_I2C_CFG		(S3C_VA_SYS + 0x234)

#define EXYNOS_WAKEUP_STAT_EINT		(1 << 0)
#define EXYNOS_WAKEUP_STAT_RTC_ALARM	(1 << 1)

#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

static struct sleep_save exynos3472_set_clksrc[] = {
	{ .reg = EXYNOS3_CLKSRC_MASK_TOP		, .val = 0x00000001, },
	{ .reg = EXYNOS3_CLKSRC_MASK_CAM		, .val = 0xD1100001, },
	{ .reg = EXYNOS3_CLKSRC_MASK_LCD		, .val = 0x00001001, },
	{ .reg = EXYNOS3_CLKSRC_MASK_ISP		, .val = 0x00001111, },
	{ .reg = EXYNOS3_CLKSRC_MASK_FSYS		, .val = 0x10000111, },
	{ .reg = EXYNOS3_CLKSRC_MASK_PERIL0		, .val = 0x00011111, },
	{ .reg = EXYNOS3_CLKSRC_MASK_PERIL1		, .val = 0x00110011, },
	{ .reg = EXYNOS3_CLKSRC_MASK_ACP		, .val = 0x00010000, },
};

static int exynos_cpu_suspend(unsigned long arg)
{
	unsigned int tmp;
	unsigned int i;

	/* Set clock source for PWI */
	tmp = __raw_readl(EXYNOS3_CLKSRC_ACP);
	tmp &= ~(0xF << 16);
	tmp |= (0x1 << 16);
	__raw_writel(tmp, EXYNOS3_CLKSRC_ACP);

	/* flush cache back to ram */
	flush_cache_all();

	exynos_reset_assert_ctrl(false);

	/* For W/A code for prevent A7hotplug in fail */
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x02020004), 0, 0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x02020008), 0, 0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x0202000C), 0, 0);

	for (i = 1; i < NR_CPUS; i++) {
		int loops;

		tmp = __raw_readl(EXYNOS_ARM_CORE_CONFIGURATION(i));
		tmp |= EXYNOS_CORE_LOCAL_PWR_EN;
		tmp |= EXYNOS_CORE_AUTOWAKEUP_EN;
		__raw_writel(tmp, EXYNOS_ARM_CORE_CONFIGURATION(i));

		/* Wait until changing core status during 5ms */
		loops = msecs_to_loops(5);
		do {
			if (--loops == 0)
				BUG();
			tmp = __raw_readl(EXYNOS_ARM_CORE_STATUS(i));
		} while ((tmp & 0x3) != 0x3);
	}

#ifdef CONFIG_ARM_TRUSTZONE
	exynos_smc(SMC_CMD_SLEEP, 0, 0, 0);
#else
	/* issue the standby signal into the pm unit. */
	cpu_do_idle();
#endif
	pr_info("sleep resumed to originator?");

	return 1; /* abort suspend */
}

static void exynos_pm_prepare(void)
{
	unsigned int tmp;

#ifdef CONFIG_MACH_UNIVERSAL3472
	if (exynos_config_sleep_gpio)
		exynos_config_sleep_gpio();
#endif

	/* Decides whether to use retention capability */
	tmp = __raw_readl(EXYNOS3_ARM_L2_OPTION);
	tmp &= ~EXYNOS3_OPTION_USE_RETENTION;
	__raw_writel(tmp, EXYNOS3_ARM_L2_OPTION);

	/* Set value of power down register for sleep mode */
	exynos_sys_powerdown_conf(SYS_SLEEP);
	__raw_writel(EXYNOS_CHECK_SLEEP, REG_INFORM1);

	/* ensure at least INFORM0 has the resume address */
	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_INFORM0);

	/*
	 * Before enter central sequence mode,
	 * clock src register have to set.
	 */
	s3c_pm_do_restore_core(exynos3472_set_clksrc,
		ARRAY_SIZE(exynos3472_set_clksrc));

}

static int exynos_pm_add(struct device *dev, struct subsys_interface *sif)
{
	pm_cpu_prep = exynos_pm_prepare;
	pm_cpu_sleep = exynos_cpu_suspend;

	return 0;
}

static unsigned long pll_base_rate;

void exynos3_scu_enable(void __iomem *scu_base)
{
	u32 scu_ctrl;

#ifdef CONFIG_ARM_ERRATA_764369
	/* Cortex-A9 only */
	if ((read_cpuid(CPUID_ID) & 0xff0ffff0) == 0x410fc090) {
		scu_ctrl = __raw_readl(scu_base + 0x30);
		if (!(scu_ctrl & 1))
			__raw_writel(scu_ctrl | 0x1, scu_base + 0x30);
	}
#endif

	scu_ctrl = __raw_readl(scu_base);
	/* already enabled? */
	if (scu_ctrl & 1)
		return;

	if (soc_is_exynos4412() && (samsung_rev() >= EXYNOS4412_REV_1_0))
		scu_ctrl |= (1<<3);

	scu_ctrl |= 1;
	__raw_writel(scu_ctrl, scu_base);

	/*
	 * Ensure that the data accessed by CPU0 before the SCU was
	 * initialised is visible to the other CPUs.
	 */
	flush_cache_all();
}

static struct subsys_interface exynos3_pm_interface = {
	.name		= "exynos_pm",
	.subsys		= &exynos3_subsys,
	.add_dev	= exynos_pm_add,
};

static __init int exynos_pm_drvinit(void)
{
	struct clk *pll_base;

	s3c_pm_init();

	pll_base = clk_get(NULL, "xtal");

	if (!IS_ERR(pll_base)) {
		pll_base_rate = clk_get_rate(pll_base);
		clk_put(pll_base);
	}

	return subsys_interface_register(&exynos3_pm_interface);
}
arch_initcall(exynos_pm_drvinit);

static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int reg_eintstart;
	long unsigned int ext_int_pend;
	unsigned long eint_wakeup_mask;
	bool found = 0;
	extern void __iomem *exynos_eint_base;

	eint_wakeup_mask = __raw_readl(EXYNOS_EINT_WAKEUP_MASK);

	for (reg_eintstart = 0; reg_eintstart <= 31; reg_eintstart += 8) {
		ext_int_pend =
			__raw_readl(EINT_PEND(exynos_eint_base,
					      IRQ_EINT(reg_eintstart)));

		for_each_set_bit(bit, &ext_int_pend, 8) {
			int irq = IRQ_EINT(reg_eintstart) + bit;
			struct irq_desc *desc = irq_to_desc(irq);

			if (eint_wakeup_mask & (1 << (reg_eintstart + bit)))
				continue;

			if (desc && desc->action && desc->action->name)
				pr_info("Resume caused by IRQ %d, %s\n", irq,
					desc->action->name);
			else
				pr_info("Resume caused by IRQ %d\n", irq);

			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}

static void exynos_show_wakeup_reason(void)
{
	unsigned long wakeup_stat;

	wakeup_stat = __raw_readl(EXYNOS_WAKEUP_STAT);

	if (wakeup_stat & EXYNOS_WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	else if (wakeup_stat & EXYNOS_WAKEUP_STAT_EINT)
		exynos_show_wakeup_reason_eint();
	else
		pr_info("Resume caused by wakeup_stat=0x%08lx\n",
			wakeup_stat);
}

static void exynos3_pm_rewrite_eint_pending(void)
{
	pr_info("Rewrite eint pending interrupt clear \n");
	__raw_writel(__raw_readl(S5P_EINT_PEND(0)), S5P_EINT_PEND(0));
	__raw_writel(__raw_readl(S5P_EINT_PEND(1)), S5P_EINT_PEND(1));
	__raw_writel(__raw_readl(S5P_EINT_PEND(2)), S5P_EINT_PEND(2));
	__raw_writel(__raw_readl(S5P_EINT_PEND(3)), S5P_EINT_PEND(3));
}

static int exynos_pm_suspend(void)
{
	unsigned long tmp;

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	/* FIXME: PMU Config before power down*/
	tmp = __raw_readl(EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS3_CENTRAL_SEQ_CONFIGURATION_COREBLK);

#ifdef CONFIG_CPU_IDLE
	exynos_disable_idle_clock_down();
#endif
	exynos_reset_assert_ctrl(false);

	return 0;
}

static void exynos_pm_resume(void)
{
	unsigned long tmp;

	exynos_reset_assert_ctrl(true);

	__raw_writel(EXYNOS3_USE_STANDBY_WFI_ALL,
		EXYNOS_CENTRAL_SEQ_OPTION);

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

	/* For release retention */
	__raw_writel((1 << 28), EXYNOS_PAD_RET_MAUDIO_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_GPIO_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_UART_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_MMCA_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_MMCB_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_EBIA_OPTION);
	__raw_writel((1 << 28), EXYNOS_PAD_RET_EBIB_OPTION);
	__raw_writel((1 << 28), EXYNOS3_PAD_RETENTION_MMC2_OPTION);
	__raw_writel((1 << 28), EXYNOS3_PAD_RETENTION_SPI_OPTION);

	if (__raw_readl(EXYNOS_WAKEUP_STAT) == 0x0)
		exynos3_pm_rewrite_eint_pending();

	bts_initialize(NULL, true);

	goto early_wakeup;

#ifdef CONFIG_SMP
	exynos3_scu_enable(S5P_VA_SCU);
#endif
early_wakeup:
#ifdef CONFIG_CPU_IDLE
	exynos_enable_idle_clock_down();
#endif
	exynos_reset_assert_ctrl(true);
	__raw_writel(0x0, REG_INFORM1);
	exynos_show_wakeup_reason();

	return;
}

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_suspend,
	.resume		= exynos_pm_resume,
};

static __init int exynos_pm_syscore_init(void)
{
	register_syscore_ops(&exynos_pm_syscore_ops);
	return 0;
}
arch_initcall(exynos_pm_syscore_init);
