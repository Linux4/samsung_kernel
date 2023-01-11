/*
 * linux/arch/arm/mach-vexpress/mcpm_platsmp.c
 *
 * Created by:  Nicolas Pitre, November 2012
 * Copyright:   (C) 2012-2013  Linaro Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Code to handle secondary CPU bringup and hotplug for the cluster power API.
 */

#include <linux/init.h>
#include <linux/of.h>
#include <linux/smp.h>
#include <linux/spinlock.h>

#include <asm/cputype.h>
#include <asm/cpu_ops.h>
#include <asm/mcpm.h>
#include <asm/suspend.h>
#include <asm/smp.h>
#include <asm/smp_plat.h>

static int cpu_mcpm_cpu_init(struct device_node *dn, unsigned int cpu)
{
	return 0;
}

static int cpu_mcpm_cpu_prepare(unsigned int cpu)
{
	return 0;
}

static int cpu_mcpm_cpu_boot(unsigned int cpu)
{
	unsigned int mpidr, pcpu, pcluster, ret;
	extern void secondary_startup(void);

	mpidr = cpu_logical_map(cpu);
	pcpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	pcluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	pr_debug("%s: logical CPU %d is physical CPU %d cluster %d\n",
		 __func__, cpu, pcpu, pcluster);

	mcpm_set_entry_vector(pcpu, pcluster, NULL);
	ret = mcpm_cpu_power_up(pcpu, pcluster);
	if (ret)
		return ret;
	mcpm_set_entry_vector(pcpu, pcluster, secondary_entry);
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));
	dsb();
	sev();
	return 0;
}

static void cpu_mcpm_cpu_postboot(void)
{
	mcpm_cpu_powered_up();
}

#ifdef CONFIG_HOTPLUG_CPU

static int cpu_mcpm_cpu_disable(unsigned int cpu)
{
	/*
	 * We assume all CPUs may be shut down.
	 * This would be the hook to use for eventual Secure
	 * OS migration requests as described in the PSCI spec.
	 */
	return 0;
}

static void cpu_mcpm_cpu_die(unsigned int cpu)
{
	unsigned int mpidr, pcpu, pcluster;
	mpidr = read_cpuid_mpidr();
	pcpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	pcluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	mcpm_set_entry_vector(pcpu, pcluster, NULL);
	mcpm_cpu_power_down();
}

#endif

static int cpu_mcpm_cpu_suspend(unsigned long index)
{
	u32 mpidr = read_cpuid_mpidr();
	u32 cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	u32 this_cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	mcpm_set_entry_vector(cpu, this_cluster, cpu_resume);
	mcpm_cpu_suspend(index);
	return 1;
}


const struct cpu_operations cpu_mcpm_ops = {
	.name			= "mcpm",
	.cpu_init		= cpu_mcpm_cpu_init,
	.cpu_prepare		= cpu_mcpm_cpu_prepare,
	.cpu_boot		= cpu_mcpm_cpu_boot,
	.cpu_postboot		= cpu_mcpm_cpu_postboot,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable		= cpu_mcpm_cpu_disable,
	.cpu_die		= cpu_mcpm_cpu_die,
#endif
#ifdef CONFIG_ARM64_CPU_SUSPEND
	.cpu_suspend		= cpu_mcpm_cpu_suspend,
#endif
};
