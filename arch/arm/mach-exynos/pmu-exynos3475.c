/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/gpio.h>
#include <linux/wakeup_reason.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/regs-gpio.h>
#include <mach/pmu.h>
#include <mach/pmu_cal_sys.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#include "common.h"

#define REG_CPU_STATE_ADDR     (S5P_VA_SYSRAM_NS + 0x28)

#define REG_DIRECTGO_ADDR	(S5P_VA_SYSRAM_NS + 0x24)
#define REG_DIRECTGO_FLAG	(S5P_VA_SYSRAM_NS + 0x20)

#define EXYNOS_CHECK_DIRECTGO	0xFCBA0D10

#define EXYNOS_WAKEUP_STAT_EINT         (1 << 0)
#define EXYNOS_WAKEUP_STAT_RTC_ALARM    (1 << 1)

static void __iomem *exynos_eint_base;
extern u32 exynos_eint_to_pin_num(int eint);

void set_boot_flag(unsigned int cpu, unsigned int mode)
{
	unsigned int phys_cpu = cpu_logical_map(cpu);
	unsigned int tmp, core;
	void __iomem *addr;

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_DIRECTGO_ADDR);
	__raw_writel(EXYNOS_CHECK_DIRECTGO, REG_DIRECTGO_FLAG);

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	addr = REG_CPU_STATE_ADDR + 4 * (core);

	tmp = __raw_readl(addr);

	if (mode & BOOT_MODE_MASK)
		tmp &= ~BOOT_MODE_MASK;
	else
		BUG_ON(mode == 0);

	tmp |= mode;
	__raw_writel(tmp, addr);
}

void clear_boot_flag(unsigned int cpu, unsigned int mode)
{
	unsigned int phys_cpu = cpu_logical_map(cpu);
	unsigned int tmp, core;
	void __iomem *addr;

	BUG_ON(mode == 0);

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);
	addr = REG_CPU_STATE_ADDR + 4 * (core);

	tmp = __raw_readl(addr);
	tmp &= ~mode;
	__raw_writel(tmp, addr);
}

void exynos3475_secondary_up(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core);

	tmp = __raw_readl(addr);
	tmp |= EXYNOS_CORE_PWR_EN | EXYNOS_CORE_INIT_WAKEUP_FROM_LOWPWR;
	__raw_writel(tmp, addr);

	addr = EXYNOS_ARM_CORE_OPTION(core);

	tmp = __raw_readl(addr);
	tmp |= EXYNOS_ENABLE_AUTOMATIC_WAKEUP;

	__raw_writel(tmp, addr);
}

void exynos3475_cpu_up(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core);

	tmp = __raw_readl(addr);
	tmp |= EXYNOS_CORE_PWR_EN;
	__raw_writel(tmp, addr);
}

void exynos3475_secondary_down(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core);

	tmp = __raw_readl(addr);
	tmp &= ~(EXYNOS_CORE_PWR_EN);
	__raw_writel(tmp, addr);

	addr = EXYNOS_ARM_CORE_OPTION(core);

	tmp = __raw_readl(addr);
	tmp &= ~(EXYNOS_ENABLE_AUTOMATIC_WAKEUP);
	__raw_writel(tmp, addr);
}

void exynos3475_cpu_down(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int tmp, core;
	void __iomem *addr;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);

	addr = EXYNOS_ARM_CORE_CONFIGURATION(core);

	tmp = __raw_readl(addr);
	tmp &= ~(EXYNOS_CORE_PWR_EN);
	__raw_writel(tmp, addr);
}

unsigned int exynos3475_cpu_state(unsigned int cpu_id)
{
	unsigned int phys_cpu = cpu_logical_map(cpu_id);
	unsigned int core, val;

	core = MPIDR_AFFINITY_LEVEL(phys_cpu, 0);

	val = __raw_readl(EXYNOS_ARM_CORE_STATUS(core))
			& LOCAL_PWR_CFG;

	return val == LOCAL_PWR_CFG;
}

void exynos_set_wakeupmask(enum sys_powerdown mode)
{
	u64 eintmask = exynos_get_eint_wake_mask();
	u32 intmask = 0;

	/* Set external interrupt mask */
	__raw_writel((u32)eintmask, EXYNOS_PMU_EINT_WAKEUP_MASK);

	switch (mode) {
	case SYS_SICD:
	case SYS_AFTR:
	case SYS_LPD:
		intmask = 0x40001000;
		break;
	case SYS_DSTOP:
	case SYS_CP_CALL:
		intmask = 0x7CEFFFFF;
		break;
	default:
		break;
	}
/* To do: will be removed after released beta BSP */
#if defined(CONFIG_EXYNOS7580_MCU_IPC)
	/* CP_ACTIVE, INT_MBOX, INT_S_MBOX, CP_RESET_REQ */
	intmask = 0x7c6FFFFF;
	__raw_writel(intmask, EXYNOS_PMU_WAKEUP_MASK);

	pr_err("%s: mode (%d) PMU_WAKEUP_MASK(0x%x)\n", __func__,
		mode, __raw_readl(EXYNOS_PMU_WAKEUP_MASK));
#else
	__raw_writel(intmask, EXYNOS_PMU_WAKEUP_MASK);
#endif

	__raw_writel(0xFFFFFFFF, EXYNOS_PMU_WAKEUP_MASK2);
	__raw_writel(0xFFFFFFDF, EXYNOS_PMU_WAKEUP_MASK_MIF);
}

static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int reg_eintstart;
	long unsigned int ext_int_pend;
	unsigned long eint_wakeup_mask;
	bool found = 0;

	eint_wakeup_mask = __raw_readl(EXYNOS_PMU_EINT_WAKEUP_MASK);

	for (reg_eintstart = 0; reg_eintstart <= 15; reg_eintstart += 8) {
		ext_int_pend =
			__raw_readl(EXYNOS3475_EINT_PEND(exynos_eint_base,
						reg_eintstart));

		for_each_set_bit(bit, &ext_int_pend, 8) {
			u32 gpio;
			int irq;

			if (eint_wakeup_mask & (1 << (reg_eintstart + bit)))
				continue;

			gpio = exynos_eint_to_pin_num(reg_eintstart + bit);
			irq = gpio_to_irq(gpio);

			log_wakeup_reason(irq);
			update_wakeup_reason_stats(irq, reg_eintstart + bit);
			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}

void exynos_show_wakeup_reason(void)
{
	unsigned int emask, emask1, imask, imask2, imask_mif;
	unsigned int stat, stat2, stat_mif;
	unsigned int cp_stat;

	emask     = __raw_readl(EXYNOS_PMU_EINT_WAKEUP_MASK);
	emask1    = __raw_readl(EXYNOS_PMU_EINT_WAKEUP_MASK1);
	imask	  = __raw_readl(EXYNOS_PMU_WAKEUP_MASK);
	imask2    = __raw_readl(EXYNOS_PMU_WAKEUP_MASK2);
	imask_mif = __raw_readl(EXYNOS_PMU_WAKEUP_MASK_MIF);

	stat     = __raw_readl(EXYNOS_PMU_WAKEUP_STAT);
	stat2    = __raw_readl(EXYNOS_PMU_WAKEUP_STAT2);
	stat_mif = __raw_readl(EXYNOS_PMU_WAKEUP_STAT_MIF);
	cp_stat  = __raw_readl(EXYNOS_PMU_CP_STAT);

	pr_info("wakeup mask>>>\n");
	pr_info("   EINT_MASK      0x%08x\n", emask);
	pr_info("   EINT_MASK1     0x%08x\n", emask1);
	pr_info("    INT_MASK      0x%08x\n", imask);
	pr_info("    INT_MASK2     0x%08x\n", imask2);
	pr_info("    INT_MASK_MIF  0x%08x\n", imask_mif);
	pr_info("wakeup reason>>>\n");
	pr_info("   WKUP_STAT:     0x%08x\n", stat);
	pr_info("   WKUP_STAT2:    0x%08x\n", stat2);
	pr_info("   WKUP_STAT_MIF: 0x%08x\n", stat_mif);
 	pr_info("cp stat>>>\n");
 	pr_info("   CP_STAT:       0x%08x\n", cp_stat);

	if (stat & EXYNOS_WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	else if (stat & EXYNOS_WAKEUP_STAT_EINT)
		exynos_show_wakeup_reason_eint();
	else
		pr_info("Resume caused by wakeup_stat=0x%08x\n",
			stat);
}

static void exynos_enable_hw_trip(void)
{
	unsigned int tmp;

	/* Set ouput high at PSHOLD port and enable H/W trip */
	tmp = __raw_readl(EXYNOS_PS_HOLD_CONTROL);
	tmp |= (EXYNOS_PS_HOLD_OUTPUT_HIGH | EXYNOS_PS_HOLD_EN);
	__raw_writel(tmp, EXYNOS_PS_HOLD_CONTROL);
}

void exynos_pmu_wdt_control(bool on, unsigned int pmu_wdt_reset_type)
{
	unsigned int value;
	unsigned int wdt_auto_reset_dis;
	unsigned int wdt_reset_mask;

	/*
	 * When SYS_WDTRESET is set, watchdog timer reset request is ignored
	 * by power management unit.
	 */
	if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE0) {
		wdt_auto_reset_dis = EXYNOS_SYS_WDTRESET;
		wdt_reset_mask = EXYNOS_SYS_WDTRESET;
	} else if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE4) {
		wdt_auto_reset_dis = EXYNOS_ARM_WDTRESET;
		wdt_reset_mask = EXYNOS_ARM_WDTRESET;
	} else {
		pr_err("Failed to %s pmu wdt reset\n",
				on ? "enable" : "disable");
		return;
	}

	value = __raw_readl(EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	if (on)
		value &= ~wdt_auto_reset_dis;
	else
		value |= wdt_auto_reset_dis;
	__raw_writel(value, EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	value = __raw_readl(EXYNOS_MASK_WDT_RESET_REQUEST);
	if (on)
		value &= ~wdt_reset_mask;
	else
		value |= wdt_reset_mask;
	__raw_writel(value, EXYNOS_MASK_WDT_RESET_REQUEST);

	return;
}

int __init exynos3475_pmu_init(void)
{
	exynos_enable_hw_trip();

	/* Set clock freeze cycle before and after ARM clamp to 0 */
	//__raw_writel(0x0, EXYNOS3475_CPU_STOPCTRL);

	exynos_cpu.power_up_noinit = exynos3475_cpu_up;
	exynos_cpu.power_up = exynos3475_secondary_up;
	exynos_cpu.power_state = exynos3475_cpu_state;
	exynos_cpu.power_down_noinit = exynos3475_cpu_down;
	exynos_cpu.power_down = exynos3475_secondary_down;

	pmu_cal_sys_init();

	exynos_pmu_cp_init();


	exynos_eint_base = ioremap(EXYNOS3475_PA_GPIO_ALIVE, SZ_4K);

	if (exynos_eint_base == NULL) {
		pr_err("%s: unable to ioremap for EINT base address\n",
				__func__);
		BUG();
	}

	pr_info("EXYNOS3475 PMU Initialize\n");

	return 0;
}
