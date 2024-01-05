/* linux/arch/arm/mach-exynos4/platsmp.c
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Cloned from linux/arch/arm/mach-vexpress/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/smp_plat.h>
#include <asm/smp_scu.h>

#include <mach/hardware.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/smc.h>

#include <plat/cpu.h>
#ifdef CONFIG_WATCHDOG
#include <plat/regs-watchdog.h>
#endif

extern void exynos4_secondary_startup(void);

struct _cpu_boot_info {
	void __iomem *boot_base;
	void __iomem *power_base;
};

struct _cpu_boot_info cpu_boot_info[NR_CPUS];

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */

volatile int pen_release = -1;

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static void __iomem *scu_base_addr(void)
{
	return (void __iomem *)(S5P_VA_SCU);
}

static DEFINE_SPINLOCK(boot_lock);

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	/* Enable the full line of zero */
	if (soc_is_exynos4210() || soc_is_exynos4212() ||
	    soc_is_exynos4412() || soc_is_exynos4415())
		enable_cache_foz();

	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	gic_secondary_init(0);

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

#ifdef CONFIG_EXYNOS5_MP
	cpu = cpu ^ 0x4;
#endif
#ifdef CONFIG_ARM_TRUSTZONE
	clear_boot_flag(cpu, HOTPLUG);
#endif

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

void change_power_base(unsigned int cpu, void __iomem *base)
{
	cpu_boot_info[cpu].power_base = base;
}

void change_all_power_base_to(unsigned int cluster)
{
	int i;
	int offset = 0;

	if (!soc_is_exynos5410() && !soc_is_exynos5420())
		return;

	if (soc_is_exynos5410())  {
		if (samsung_rev() < EXYNOS5410_REV_1_0) {
			if (cluster == 0)
				offset = 4;
		} else {
			if (cluster != 0)
				offset = 4;
		}
	} else {
		if (cluster)
			offset = 4;
	}

	for (i = 0; i < 4; i++) {
		cpu_boot_info[i].power_base =
			EXYNOS_ARM_CORE_CONFIGURATION(offset + i);
	}
}

#if defined (CONFIG_ARCH_EXYNOS3) || defined (CONFIG_ARCH_EXYNOS4)
static int exynos_power_up_cpu(unsigned int cpu)
{
	unsigned int timeout;
	unsigned int val;
	unsigned int tmp;
	void __iomem *power_base;

	power_base = cpu_boot_info[cpu].power_base;
	if (power_base == 0)
		return -EPERM;

	val = __raw_readl(power_base + 0x4);
	if (!(val & EXYNOS_CORE_LOCAL_PWR_EN)) {
		tmp = __raw_readl(power_base);
		tmp |= (EXYNOS_CORE_LOCAL_PWR_EN);
		tmp |= (EXYNOS_CORE_AUTOWAKEUP_EN);
		__raw_writel(tmp, power_base);

		/* wait max 10 ms until cpu is on */
		timeout = 10;
		while (timeout) {
			val = __raw_readl(power_base + 0x4);

			if ((val & EXYNOS_CORE_LOCAL_PWR_EN) ==
			     EXYNOS_CORE_LOCAL_PWR_EN)
				break;

			mdelay(1);
			timeout--;
		}

		if (timeout == 0) {
			printk(KERN_ERR "cpu%d power up failed", cpu);
			return -ETIMEDOUT;
		}
	}

	/*
	 * Check Power down cpu wait on WFE, and occur SW reset
	 */
	if (soc_is_exynos3470() || soc_is_exynos3250() || soc_is_exynos3472()) {
		while(!__raw_readl(EXYNOS_PMUREG(0x0908)))
			udelay(10);

		udelay(10);

		tmp = __raw_readl(power_base + 0x4);
		tmp |= (0x3 << 8);
		__raw_writel(tmp, power_base + 0x4);

		/* TODO set COREX's WAKEUP_FROM_LOCAL_CFG register with 0x3 */
		printk("cpu%d: SWRESET\n", cpu);
		__raw_writel(((1 << 4) << cpu), EXYNOS_PMUREG(0x0400));
	}

	return 0;
}
#elif defined(CONFIG_ARCH_EXYNOS5)
static int exynos_power_up_cpu(unsigned int cpu)
{
	unsigned int timeout;
	unsigned int val;
	void __iomem *power_base;
#ifndef CONFIG_EXYNOS5_MP
	unsigned int cluster = (read_cpuid_mpidr() >> 8) & 0xf;
#endif
	unsigned int lpe_bits, lpe_bits_status, enabled = 0;

	power_base = cpu_boot_info[cpu].power_base;
	if (power_base == 0)
		return -EPERM;

	val = __raw_readl(power_base + 0x4);

	if (soc_is_exynos5260()) {
		if (val & 0x40000)
			enabled = 1;
		else {
			val = __raw_readl(power_base + 0x8);
			if (val & EXYNOS5_USE_SC_COUNTER) {
				val &= ~EXYNOS5_USE_SC_COUNTER;
				val |= EXYNOS5_USE_SC_FEEDBACK;
				__raw_writel(val, power_base + 0x8);
			}
			lpe_bits = 0x800F000F;
			lpe_bits_status = 0x4000F;
#ifndef CONFIG_ARM_TRUSTZONE
			__raw_writel(0, cpu_boot_info[cpu].boot_base);
#endif
		}
	} else {
		if (val & EXYNOS_CORE_LOCAL_PWR_EN)
			enabled = 1;
		else {
			lpe_bits = EXYNOS_CORE_LOCAL_PWR_EN;
			lpe_bits_status = EXYNOS_CORE_LOCAL_PWR_EN;
		}
	}

	if (!enabled) {
		__raw_writel(lpe_bits, power_base);

		/* wait max 10 ms until cpu is on */
		timeout = 10;
		while (timeout) {
			val = __raw_readl(power_base + 0x4);

			if ((val & lpe_bits_status) ==
			     lpe_bits_status)
				break;

			mdelay(1);
			timeout--;
		}

		if (timeout == 0) {
			printk(KERN_ERR "cpu%d power up failed", cpu);

			return -ETIMEDOUT;
		}
	}

#ifdef CONFIG_EXYNOS5_MP
	if (cpu < 4) {
#else
	if (cluster) {
#endif
		while(!__raw_readl(EXYNOS_PMU_SPARE2))
			udelay(10);

		udelay(10);

		if (soc_is_exynos5260()) {
			val = __raw_readl(power_base + 0x4);
			val |= (0xf << 8);
			__raw_writel(val, power_base + 0x4);

			pr_debug("cpu%d: SWRESEET\n", cpu);

			__raw_writel((0x1 << 1), power_base + 0xc);
		} else {
			printk(KERN_DEBUG "cpu%d: SWRESET\n", cpu);

			val = ((1 << 20) | (1 << 8)) << cpu;
			__raw_writel(val, EXYNOS_SWRESET);
		}
	}

	return 0;
}

#else
#error "exynos_power_up_cpu() does not defined"
#endif
int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;
	int ret;

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

#ifdef CONFIG_WATCHDOG
	if (soc_is_exynos5250())
		watchdog_save();
#endif

#ifdef CONFIG_SOC_EXYNOS4415
	__raw_writel(0x0, cpu_boot_info[cpu].boot_base);
#endif

	ret = exynos_power_up_cpu(cpu);
	if (ret) {
		spin_unlock(&boot_lock);
		return ret;
	}

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();

#ifdef CONFIG_ARM_TRUSTZONE
		if (soc_is_exynos4210() || soc_is_exynos4212() ||
			soc_is_exynos5250())
			exynos_smc(SMC_CMD_CPU1BOOT, 0, 0, 0);
		else if (soc_is_exynos4412())
			exynos_smc(SMC_CMD_CPU1BOOT, cpu, 0, 0);
#endif
		__raw_writel(virt_to_phys(exynos4_secondary_startup),
			cpu_boot_info[cpu].boot_base);

#ifdef CONFIG_WATCHDOG
		if (soc_is_exynos5250())
			watchdog_restore();
#endif

		if (soc_is_exynos3250() || soc_is_exynos3470() || soc_is_exynos3472() ||
			soc_is_exynos5410() || soc_is_exynos5420() ||
			soc_is_exynos5260() || soc_is_exynos4415())
			dsb_sev();
		else
			arm_send_ping_ipi(cpu);

		if (pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */

void __init smp_init_cpus(void)
{
	void __iomem *scu_base = scu_base_addr();
	unsigned int i, ncores;

	if (soc_is_exynos4210() || soc_is_exynos4212() ||
			soc_is_exynos5250() || soc_is_exynos3250())
		ncores = 2;
	else if (soc_is_exynos3472() || soc_is_exynos4412() || soc_is_exynos5410()
		|| soc_is_exynos4415() || soc_is_exynos3470() )
		ncores = 4;
	else if (soc_is_exynos5260())
#ifdef CONFIG_EXYNOS5_MP
		ncores = NR_CPUS;
#else
		ncores = read_cpuid_mpidr() & 0x100 ? 4 : 2;
#endif
	else if (soc_is_exynos5420())
#ifdef CONFIG_EXYNOS5_MP
	{
		ncores = 8;
		__raw_writel(0x4, S5P_VA_SYSRAM_NS + 0x28);
	}
#else
		ncores = 4;
#endif
	else
		ncores = scu_base ? scu_get_core_count(scu_base) : 1;

	/* sanity check */
	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

	set_smp_cross_call(gic_raise_softirq);
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	if (soc_is_exynos4210() || soc_is_exynos4212() || soc_is_exynos4412()
		|| soc_is_exynos4415())
		scu_enable(scu_base_addr());

	for (i = 1; i < max_cpus; i++) {
		int pwr_offset = 0;

#ifdef CONFIG_ARM_TRUSTZONE
		cpu_boot_info[i].boot_base = S5P_VA_SYSRAM_NS + 0x1C;
#else

		if (soc_is_exynos4210() &&
			(samsung_rev() >= EXYNOS4210_REV_1_1))
			cpu_boot_info[i].boot_base = EXYNOS_INFORM5;
		else
			cpu_boot_info[i].boot_base = S5P_VA_SYSRAM;
#endif
		if (soc_is_exynos4412())
			cpu_boot_info[i].boot_base += (0x4 * i);
		else if (soc_is_exynos3250() || soc_is_exynos3472()) {
#ifndef CONFIG_ARM_TRUSTZONE
			cpu_boot_info[i].boot_base += (0x4 * i);
#endif
		} else if (soc_is_exynos5260()) {
			int cluster_id = read_cpuid_mpidr() & 0x100;
#ifndef CONFIG_ARM_TRUSTZONE
			if (cluster_id)
				cpu_boot_info[i].boot_base += (0x4 * i);
			else
				cpu_boot_info[i].boot_base += (0x10 + 0x4 * i);
#endif
			if (cluster_id != 0)
				pwr_offset = 4;
		} else if (soc_is_exynos5410()) {
			int cluster_id = read_cpuid_mpidr() & 0x100;
			if (samsung_rev() < EXYNOS5410_REV_1_0) {
				if (cluster_id == 0)
					pwr_offset = 4;
			} else {
				if (cluster_id != 0)
					pwr_offset = 4;
			}
		} else if (soc_is_exynos5420()) {
			int cluster_id = read_cpuid_mpidr() & 0x100;
#ifndef CONFIG_ARM_TRUSTZONE
			cpu_boot_info[i].boot_base += (0x4 * i);
#endif
			if (cluster_id != 0)
				pwr_offset = 4;
		}

		cpu_boot_info[i].power_base =
#ifdef CONFIG_EXYNOS5_MP
			EXYNOS_ARM_CORE_CONFIGURATION(i ^ 4);
#else
			EXYNOS_ARM_CORE_CONFIGURATION(i + pwr_offset);
#endif
	}
}
