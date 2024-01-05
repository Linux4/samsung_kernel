/* linux/arch/arm/mach-exynos/pm-exynos4415.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
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

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/smp_scu.h>
#include <asm/cputype.h>

#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/regs-srom.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-irq.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pm-core.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/pm_interrupt_domains.h>
#include <mach/regs-clock-exynos4415.h>

#ifdef CONFIG_SEC_GPIO_DVS
#include <linux/secgpio_dvs.h>
#endif

#define PM_PREFIX	"PM DOMAIN: "

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifdef PM_DEBUG
#define DEBUG_PRINT_INFO(fmt, ...) printk(PM_PREFIX pr_fmt(fmt), ##__VA_ARGS__)
#else
#define DEBUG_PRINT_INFO(fmt, ...)
#endif

#ifdef CONFIG_ARM_TRUSTZONE
#define REG_INFORM0            (S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1            (S5P_VA_SYSRAM_NS + 0xC)
#else
#define REG_INFORM0            (EXYNOS_INFORM0)
#define REG_INFORM1            (EXYNOS_INFORM1)
#endif

#define REG_INFORM8            (EXYNOS_INFORM0 + 0x1c)
#define EXYNOS_I2C_CFG		(S3C_VA_SYS + 0x234)

#define EXYNOS_WAKEUP_STAT_EINT		(1 << 0)
#define EXYNOS_WAKEUP_STAT_RTC_ALARM	(1 << 1)

/* define to save/restore eint filter config */
#define EINT_FLTCON_REG(x)	(S5P_VA_GPIO1 + 0xE80 + ((x) * 4))

#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

static struct sleep_save exynos4415_set_clksrc[] = {
	{ .reg = EXYNOS4415_CLKSRC_MASK_ACP		, .val = 0x00010000, },
	{ .reg = EXYNOS4_CLKSRC_MASK_TOP		, .val = 0x00000001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_CAM		, .val = 0xD1101111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_TV			, .val = 0x00000001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_LCD0		, .val = 0x00001001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_ISP		, .val = 0x00011110, },
	{ .reg = EXYNOS4_CLKSRC_MASK_MAUDIO		, .val = 0x00000001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_FSYS		, .val = 0x10000111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_PERIL0		, .val = 0x00001111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_PERIL1		, .val = 0x01110111, },
};

static struct sleep_save exynos4_enable_xusbxti[] = {
	{ .reg = EXYNOS4_XUSBXTI_LOWPWR,		.val = 0x1, },
};

static struct sleep_save exynos_core_save[] = {
	/* SROM side */
	SAVE_ITEM(S5P_SROM_BW),
	SAVE_ITEM(S5P_SROM_BC0),
	SAVE_ITEM(S5P_SROM_BC1),
	SAVE_ITEM(S5P_SROM_BC2),
	SAVE_ITEM(S5P_SROM_BC3),

	/* I2C CFG */
	SAVE_ITEM(EXYNOS_I2C_CFG),
};

static struct sleep_save exynos4_pmudebug_save[] = {
	SAVE_ITEM(EXYNOS_PMU_DEBUG),
};

/* For Cortex-A9 Diagnostic and Power control register */
#ifndef CONFIG_ARM_TRUSTZONE
static unsigned int save_arm_register[2];
#endif

static int exynos_cpu_suspend(unsigned long arg)
{
#ifdef CONFIG_ARM_TRUSTZONE
	int value = 0;
#endif

#ifdef CONFIG_CACHE_L2X0
	outer_flush_all();
	disable_cache_foz();
#endif
	/* flush cache back to ram */
	flush_cache_all();

#ifdef CONFIG_ARM_TRUSTZONE
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x02020028), value, 0);
#endif

	/* Config Sleep configuration for EFUSE_EN */
#ifdef CONFIG_MACH_UNIVERSAL4415
	/* univeral has different GPIO than Xyref */
	s5p_gpio_set_pd_cfg(EXYNOS4_GPY6(3), S5P_GPIO_PD_OUTPUT1);
#endif
#ifdef CONFIG_MACH_XYREF4415
	s5p_gpio_set_pd_cfg(EXYNOS4_GPY1(2), S5P_GPIO_PD_OUTPUT1);
#endif

#ifdef CONFIG_ARM_TRUSTZONE
	exynos_smc(SMC_CMD_SLEEP, 0, 0, 0);
#else
	/* issue the standby signal into the pm unit. */
	cpu_do_idle();

#endif
	pr_info("sleep resumed to originator?");

#ifdef CONFIG_ARM_TRUSTZONE
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x02020028), 0xfcba0d10, 0);
#endif

	/* clear the sleep indentification flag */
	__raw_writel(0x0, REG_INFORM1);
	__raw_writel(0x0, REG_INFORM8);

	return 1; /* abort suspend */
}

void s3c_pm_do_modify_core(struct sleep_save *ptr, int count)
{
	unsigned int tmp;

	for (; count > 0; count--, ptr++) {
		tmp = __raw_readl(ptr->reg);
		tmp |= ptr->val;
		__raw_writel(tmp, ptr->reg);
	}
	return;
}

static void exynos_pm_prepare(void)
{
        if (exynos_config_sleep_gpio)
                exynos_config_sleep_gpio();

#ifdef CONFIG_SEC_GPIO_DVS
        /************************ Caution !!! ****************************/
        /* This function must be located in appropriate SLEEP position
        * in accordance with the specification of each BB vendor.
        */
        /************************ Caution !!! ****************************/
        gpio_dvs_check_sleepgpio();
#endif

	/* Set value of power down register for sleep mode */
	exynos_sys_powerdown_conf(SYS_SLEEP);

	__raw_writel(EXYNOS_CHECK_SLEEP, REG_INFORM1);
	__raw_writel(EXYNOS_CHECK_SLEEP, REG_INFORM8);

	if (!(__raw_readl(EXYNOS_PMU_DEBUG) & 0x1))
		s3c_pm_do_restore_core(exynos4_enable_xusbxti,
				ARRAY_SIZE(exynos4_enable_xusbxti));

	/* ensure at least INFORM0 has the resume address */
	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_INFORM0);

	/*
	 * Before enter central sequence mode,
	 * clock src register have to set.
	 */
	s3c_pm_do_modify_core(exynos4415_set_clksrc,
				ARRAY_SIZE(exynos4415_set_clksrc));
	return;
}

static int exynos_pm_add(struct device *dev, struct subsys_interface *sif)
{
	pm_cpu_prep = exynos_pm_prepare;
	pm_cpu_sleep = exynos_cpu_suspend;

	return 0;
}

void exynos4_scu_enable(void __iomem *scu_base)
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

	scu_ctrl |= 1;
	__raw_writel(scu_ctrl, scu_base);

	/*
	 * Ensure that the data accessed by CPU0 before the SCU was
	 * initialised is visible to the other CPUs.
	 */
	flush_cache_all();

	return;
}

static struct subsys_interface exynos4_pm_interface = {
	.name		= "exynos_pm",
	.subsys		= &exynos4_subsys,
	.add_dev	= exynos_pm_add,
};

static __init int exynos_pm_drvinit(void)
{
	s3c_pm_init();

	return subsys_interface_register(&exynos4_pm_interface);
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

static int exynos_pm_suspend(void)
{
	unsigned long tmp;

	s3c_pm_do_save(exynos4_pmudebug_save, ARRAY_SIZE(exynos4_pmudebug_save));
	s3c_pm_do_save(exynos_core_save, ARRAY_SIZE(exynos_core_save));

#ifndef CONFIG_ARM_TRUSTZONE

	/* Save Power control register */
	asm ("mrc p15, 0, %0, c15, c0, 0"
		: "=r" (tmp) : : "cc");
	save_arm_register[0] = tmp;

	/* Save Diagnostic register */
	asm ("mrc p15, 0, %0, c15, c0, 1"
		: "=r" (tmp) : : "cc");
	save_arm_register[1] = tmp;
#endif
	/* set emmc low time to 30ms in inform register,
	   this value is used in iROM */
	__raw_writel(0xCD0A001E, EXYNOS4270_SYSIP_DATA0);

	/* Setting Central Sequence Register for power down mode */
	tmp = __raw_readl(EXYNOS_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~EXYNOS_CENTRAL_LOWPWR_CFG;
	__raw_writel(tmp, EXYNOS_CENTRAL_SEQ_CONFIGURATION);

	return 0;
}

static void exynos_pm_resume(void)
{
	unsigned long tmp;

	/* clear the sleep indentification flag */
	__raw_writel(0x0, REG_INFORM1);
	__raw_writel(0x0, REG_INFORM8);


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

#ifndef CONFIG_ARM_TRUSTZONE
	/* Restore Power control register */
	tmp = save_arm_register[0];
	asm volatile ("mcr p15, 0, %0, c15, c0, 0"
			: : "r" (tmp)
			: "cc");

	/* Restore Diagnostic register */
	tmp = save_arm_register[1];
	asm volatile ("mcr p15, 0, %0, c15, c0, 1"
			: : "r" (tmp)
			: "cc");
#endif

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

	s3c_pm_do_restore_core(exynos4_pmudebug_save, ARRAY_SIZE(exynos4_pmudebug_save));

#ifdef CONFIG_SMP
	exynos4_scu_enable(S5P_VA_SCU);
#endif

early_wakeup:
#ifdef CONFIG_CACHE_L2X0
	/* Enable the full line of zero */
	enable_cache_foz();
#endif
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
