/*
 * linux/arch/arm64/mcpm/mcpm_plat.c
 *
 * Author:	Fangsuo Wu <fswu@marvell.com>
 * Copyright:	(C) 2013 marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cpu_pm.h>
#include <linux/pm_qos.h>
#include <linux/cpuidle.h>
#include <linux/of.h>

#include <asm/compiler.h>
#include <asm/io.h>
#include <asm/smp_plat.h>
#ifdef CONFIG_ARM64
#include <asm/cpu_ops.h>
#else
#include <asm/opcodes-sec.h>
#include <asm/opcodes-virt.h>
#endif
#include <asm/errno.h>
#include <asm/cputype.h>
#include <asm/mcpm.h>
#include <asm/mcpm_plat.h>
#include <asm/memory.h>
#include <linux/clk/mmpdcstat.h>
#include <trace/events/pxa.h>

#define LPM_NUM			16

static arch_spinlock_t mcpm_plat_lpm_lock = __ARCH_SPIN_LOCK_UNLOCKED;
static int mcpm_plat_pm_use_count[MAX_NR_CLUSTERS][MAX_CPUS_PER_CLUSTER];
static unsigned int mcpm_plat_enter_lpm[MAX_NR_CLUSTERS][MAX_CPUS_PER_CLUSTER];
static struct platform_idle *mcpm_plat_idle;

static int (*invoke_mcpm_fn)(unsigned long, unsigned long, unsigned long, unsigned long);

enum mcpm_plat_function {
	MCPM_FN_CPU_ON,
	MCPM_FN_CPU_OFF,
	MCPM_FN_MAX,
};

static u32 mcpm_function_id[MCPM_FN_MAX];

/*
 * use below formula to calculate low power state's bitmap:
 * bitmap = (1 << state) - 1;
 *
 * so the low power state can map to bitmap as:
 *  0 : b'0000_0000;	1 : b'0000_0001;
 *  2 : b'0000_0011;	3 : b'0000_0111;
 *  4 : b'0000_1111;	5 : b'0001_1111;
 *  6 : b'0011_1111;	7 : b'0111_1111;
 */
#define _state2bit(val) ((1 << (val)) - 1)
#define _bit2state(val) (ffz(val))

static void _set_vote_state(int cpu, int cluster, unsigned int vote)
{
	mcpm_plat_enter_lpm[cluster][cpu] = _state2bit(vote);
}

static unsigned int _get_vote_state(int cpu, int cluster)
{
	return _bit2state(mcpm_plat_enter_lpm[cluster][cpu]);
}

/*
 * _calc_max_state - Find the maximum state cluster can enter
 *
 * @cluster: cluster number
 *
 * return the coupled power mode
 */
static unsigned int _calc_max_state(int cluster)
{
	int i;
	unsigned int platform_lpm = (unsigned int)-1;

	for (i = 0; i < MAX_CPUS_PER_CLUSTER; i++)
		platform_lpm &= mcpm_plat_enter_lpm[cluster][i];

	return _bit2state(platform_lpm);
}

/*
 * _calc_coupled_state - Find the coupled state for the cpu and cluster
 *
 * Seperate the state into three kinds, the detailed info as below:
 *
 * [ voting state ]: this state is every core's voting state
 *  from upper cpuidle framework;
 *
 * [ software state ]: this state will return back to cpuidle framework
 *  which exactly state the cpu has been stayed, use variable sw_state
 *  to present such value;
 *
 * [ hardware state ]: this state will indicate power controller need
 *  set which state;
 *
 * case 1: cluster is to be OFF:
 * if the cluster is to be powered off, then the coupled state is finally
 * decided by the peripheral's qos and cluster's voting;
 *
 * case 2: cluster is to be ON:
 * if the cluster is to be powered on, then the coupled state cannot be
 * directly used; the coupled state need decided by the cpu's voting rather
 * than cluster's voting; and need return s/w's state as cpudown_state.
 *
 */
static unsigned int _calc_coupled_state(int cpu, int cluster,
					unsigned int *cluster_off,
					unsigned int *sw_state)
{
	unsigned int couple_state;
#ifdef CONFIG_PXA1936_LPM
	unsigned int last_clus = _calc_max_state(cluster?0:1)
				>= mcpm_plat_idle->clusterdown_state;
#else
	unsigned int last_clus = 1;
#endif
	couple_state = _calc_max_state(cluster);

	/* set default s/w state */
	if (sw_state)
		*sw_state = mcpm_plat_idle->cpudown_state;

	/*
	 * that means cluster is off, so calculate the final D state
	 * according to the peripheral's constarint
	 */
	if (couple_state >= mcpm_plat_idle->clusterdown_state) {
		*cluster_off = 1;
		if (last_clus)
			couple_state = min(couple_state,
				((unsigned int)pm_qos_request(
					PM_QOS_CPUIDLE_BLOCK) - 1));

		if (sw_state)
			*sw_state = couple_state;
	} else {
		/*
		 * the cluster is on, so the cpu need set C state
		 * and it cannot maximum than cluster level's SD;
		 */
		*cluster_off = 0;
		couple_state = min(_get_vote_state(cpu, cluster),
				mcpm_plat_idle->clusterdown_state);
	}

	return couple_state;
}

#ifdef DBG_MCPM

struct mcpm_dbg_info {
	struct {
		unsigned int vote[MAX_NR_CLUSTERS][MAX_CPUS_PER_CLUSTER];
		unsigned int qos;
		unsigned int cluster_off;
		unsigned int max_state;
		unsigned int calc_state;
		unsigned int sw_state;
	} ctx __aligned(__CACHE_WRITEBACK_GRANULE);
};

static struct mcpm_dbg_info _dbg_info[MAX_NR_CLUSTERS][MAX_CPUS_PER_CLUSTER];

static void _record_dbg_info(int cpu, int cluster,
			     unsigned int cluster_off,
			     unsigned int calc_state,
			     unsigned int sw_state)
{
	int i, j;
	struct mcpm_dbg_info *info = &_dbg_info[cluster][cpu];

	for (i = 0; i < MAX_NR_CLUSTERS; i++)
		for (j = 0; j < MAX_CPUS_PER_CLUSTER; j++)
			info->ctx.vote[i][j] = mcpm_plat_enter_lpm[i][j];
	info->ctx.qos = pm_qos_request(PM_QOS_CPUIDLE_BLOCK);
	info->ctx.cluster_off = cluster_off;
	info->ctx.max_state = _calc_max_state(cluster);
	info->ctx.calc_state = calc_state;
	info->ctx.sw_state = sw_state;
	__flush_dcache_area(info, sizeof(*info));
	return;
}

void mcpm_dump_dbg_info(int cpu, int cluster)
{
	int i, j;

	struct mcpm_dbg_info *info = &_dbg_info[cluster][cpu];

	trace_printk("mcpm dbg: current %d/%d\n", cluster, cpu);

	for (i = 0; i < MAX_NR_CLUSTERS; i++)
		for (j = 0; j < MAX_CPUS_PER_CLUSTER; j++)
			trace_printk("%d/%d %x\n", i, j, info->ctx.vote[i][j]);

	trace_printk("qos: %x\n", info->ctx.qos);
	trace_printk("cluster off: %x\n", info->ctx.cluster_off);
	trace_printk("max state: %x\n", info->ctx.max_state);
	trace_printk("calc_state: %x\n", info->ctx.calc_state);
	trace_printk("sw_state: %x\n", info->ctx.sw_state);
}

#else

#define _record_dbg_info(cpu, cluster, cluster_off, couple_state, idx) \
	do { } while (0)
#define mcpm_dump_dbg_info(cpu, cluster) \
	do { } while (0)

#endif

/*
 * The following two functions are invoked via the invoke_psci_fn pointer
 * and will not be inlined, allowing us to piggyback on the AAPCS.
 */
static noinline int __invoke_mcpm_fn_hvc(unsigned long function_id, unsigned long arg0,
					 unsigned long arg1, unsigned long arg2)
{
#ifdef CONFIG_ARM64
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			"hvc	#0\n"
#else
	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r1")
			__asmeq("%2", "r2")
			__asmeq("%3", "r3")
			__HVC(0)
#endif
		: "+r" (function_id)
		: "r" (arg0), "r" (arg1), "r" (arg2));

	return function_id;
}

static noinline int __invoke_mcpm_fn_smc(unsigned long function_id, unsigned long arg0,
					 unsigned long arg1, unsigned long arg2)

{
#ifdef CONFIG_ARM64
	asm volatile(
			__asmeq("%0", "x0")
			__asmeq("%1", "x1")
			__asmeq("%2", "x2")
			__asmeq("%3", "x3")
			"smc	#0\n"
#else
	asm volatile(
			__asmeq("%0", "r0")
			__asmeq("%1", "r1")
			__asmeq("%2", "r2")
			__asmeq("%3", "r3")
			__SMC(0)
#endif
		: "+r" (function_id)
		: "r" (arg0), "r" (arg1), "r" (arg2));

	return function_id;
}

static int __maybe_unused
mcpm_plat_cpu_power_down(unsigned long entry_point,
			bool l2ram_shutdown)
{
	unsigned long id = mcpm_function_id[MCPM_FN_CPU_OFF];

	return invoke_mcpm_fn(id, entry_point, l2ram_shutdown, 0);
}

static int up_mode;
static int __init __init_up(char *arg)
{
	up_mode = 1;
	return 1;
}
__setup("up_mode", __init_up);

static int mcpm_plat_cpu_power_up(unsigned int cpu, unsigned int cluster,
			     unsigned long entry_point)
{
	unsigned long id = mcpm_function_id[MCPM_FN_CPU_ON];

	unsigned int hwid = (cpu<<MPIDR_LEVEL_SHIFT(0))
			    | (cluster<<MPIDR_LEVEL_SHIFT(1));

	if (up_mode)
		return -EINVAL;

	/* TODO: release(cpu) assumes there's only one cluster */
	if (mcpm_plat_idle->ops->release)
		mcpm_plat_idle->ops->release(cpu);

	return invoke_mcpm_fn(id, hwid, entry_point, 0);
}

/*
 * mcpm_plat_pm_down - Programs CPU to enter the specified state
 *
 * @addr: address points to the state selected by cpu governor
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static void mcpm_plat_pm_down(void *arg)
{
	int mpidr, cpu, cluster;
	bool skip_wfi = false, last_man = false;
	unsigned int *idx = (unsigned int *)arg;
	unsigned int calc_state, vote_state = 0;
	unsigned int cluster_off;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	BUG_ON(cluster >= MAX_NR_CLUSTERS || cpu >= MAX_CPUS_PER_CLUSTER);

	__mcpm_cpu_going_down(cpu, cluster);

	arch_spin_lock(&mcpm_plat_lpm_lock);

	mcpm_plat_pm_use_count[cluster][cpu]--;

	if (mcpm_plat_pm_use_count[cluster][cpu] == 0) {

		_set_vote_state(cpu, cluster, *idx);
		vote_state = *idx;
		calc_state = _calc_coupled_state(cpu, cluster,
						 &cluster_off, idx);
		_record_dbg_info(cpu, cluster, cluster_off, calc_state, *idx);

		if (cluster_off) {
			cpu_cluster_pm_enter();
			BUG_ON(__mcpm_cluster_state(cluster) != CLUSTER_UP);
			last_man = true;
		}

		if ((calc_state >= mcpm_plat_idle->wakeup_state) &&
#ifndef CONFIG_PXA1936_LPM
			(calc_state < mcpm_plat_idle->l2_flush_state) &&
#endif
		    (mcpm_plat_idle->ops->save_wakeup))
			mcpm_plat_idle->ops->save_wakeup();

		if (mcpm_plat_idle->ops->set_pmu)
			mcpm_plat_idle->ops->set_pmu(cpu, calc_state,
						    vote_state);
	} else if (mcpm_plat_pm_use_count[cluster][cpu] == 1) {
		/*
		 * A power_up request went ahead of us.
		 * Even if we do not want to shut this CPU down,
		 * the caller expects a certain state as if the WFI
		 * was aborted.  So let's continue with cache cleaning.
		 */
		skip_wfi = true;
		*idx = (unsigned int)-1;
	} else
		BUG();

	/* add statictis for cpuidle */
	if (cluster_off)
		cpu_dcstat_event(cpu_dcstat_clk, cpu,
				 CPU_M2_OR_DEEPER_ENTER, *idx);

	trace_pxa_cpu_idle(LPM_ENTRY(*idx), cpu, cluster);
	cpu_dcstat_event(cpu_dcstat_clk, cpu, CPU_IDLE_ENTER, *idx);
#ifdef CONFIG_VOLDC_STAT
	vol_dcstat_event(VLSTAT_LPM_ENTRY, *idx, 0);
#endif

	if (last_man && __mcpm_outbound_enter_critical(cpu, cluster)) {
		arch_spin_unlock(&mcpm_plat_lpm_lock);

		__mcpm_outbound_leave_critical(cluster, CLUSTER_DOWN);

	} else {
		arch_spin_unlock(&mcpm_plat_lpm_lock);
	}

	__mcpm_cpu_down(cpu, cluster);

	if (!skip_wfi) {
		if (unlikely(last_man && vote_state >= mcpm_plat_idle->l2_flush_state))
			mcpm_plat_cpu_power_down(virt_to_phys(mcpm_entry_point), 1);
		else
			mcpm_plat_cpu_power_down(virt_to_phys(mcpm_entry_point), 0);
	}
}

static int mcpm_plat_pm_power_up(unsigned int cpu, unsigned int cluster)
{
	if (cluster >= MAX_NR_CLUSTERS || cpu >= MAX_CPUS_PER_CLUSTER)
		return -EINVAL;

	cpu_dcstat_event(cpu_dcstat_clk, cpu, CPU_IDLE_EXIT, MAX_LPM_INDEX);

	/*
	 * Since this is called with IRQs enabled, and no arch_spin_lock_irq
	 * variant exists, we need to disable IRQs manually here.
	 */
	local_irq_disable();
	arch_spin_lock(&mcpm_plat_lpm_lock);
	/*
	 * TODO: Check if we need to do cluster related ops here?
	 * (Seems no need since this function should be called by
	 * other core, which should not enter lpm at this point).
	 */
	mcpm_plat_pm_use_count[cluster][cpu]++;

	if (mcpm_plat_pm_use_count[cluster][cpu] == 1) {
		mcpm_plat_cpu_power_up(cpu, cluster,
				      virt_to_phys(mcpm_entry_point));
	} else if (mcpm_plat_pm_use_count[cluster][cpu] != 2) {
		/*
		 * The only possible values are:
		 * 0 = CPU down
		 * 1 = CPU (still) up
		 * 2 = CPU requested to be up before it had a chance
		 *     to actually make itself down.
		 * Any other value is a bug.
		 */
		BUG();
	}

	arch_spin_unlock(&mcpm_plat_lpm_lock);
	local_irq_enable();

	return 0;
}

static void mcpm_plat_pm_power_down(void)
{
	int idx = mcpm_plat_idle->hotplug_state;
	mcpm_plat_pm_down(&idx);
}

static void mcpm_plat_pm_suspend(unsigned long addr)
{
	mcpm_plat_pm_down((void *)addr);
}

static void mcpm_plat_pm_powered_up(void)
{
	unsigned long mpidr;
	unsigned int cpu, cluster, cluster_off;
	unsigned long flags;
	unsigned int calc_state;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	BUG_ON(cluster >= MAX_NR_CLUSTERS || cpu >= MAX_CPUS_PER_CLUSTER);
	trace_pxa_cpu_idle(LPM_EXIT(0), cpu, cluster);

	local_irq_save(flags);
	arch_spin_lock(&mcpm_plat_lpm_lock);

	cpu_dcstat_event(cpu_dcstat_clk, cpu, CPU_IDLE_EXIT, MAX_LPM_INDEX);
#ifdef CONFIG_VOLDC_STAT
	vol_dcstat_event(VLSTAT_LPM_EXIT, 0, 0);
#endif

	calc_state = _calc_coupled_state(cpu, cluster, &cluster_off, NULL);
	if (cluster_off)
		cpu_cluster_pm_exit();

	if (calc_state >= mcpm_plat_idle->wakeup_state &&
#ifndef CONFIG_PXA1936_LPM
	    calc_state < mcpm_plat_idle->l2_flush_state &&
#endif
	    mcpm_plat_idle->ops->restore_wakeup)
		mcpm_plat_idle->ops->restore_wakeup();

	if (mcpm_plat_idle->ops->clr_pmu)
		mcpm_plat_idle->ops->clr_pmu(cpu);

	if (!mcpm_plat_pm_use_count[cluster][cpu])
		mcpm_plat_pm_use_count[cluster][cpu] = 1;

	_set_vote_state(cpu, cluster, 0);

	arch_spin_unlock(&mcpm_plat_lpm_lock);
	local_irq_restore(flags);

	mcpm_dump_dbg_info(cpu, cluster);
}

#ifdef CONFIG_ARM
/*
 * The function is used in the last step of cpu_hotplug if mcpm_smp_ops.cpu_kill
 * is defined (We don't need it in 3.10 kernel since cpu_kill is undefined). It
 * can be used to do the powering off and/or cutting of clocks to the dying cpu.
 * Currently we do nothing in it. In the future, maybe it can be used to check
 * if the cpu is trully powered off.
 */
static int mcpm_plat_pm_power_down_finish(unsigned int cpu, unsigned int cluster)
{
	return 0;
}
#endif

/**
 * mmp_platform_power_register - register platform power ops
 *
 * @idle: platform_idle structure points to platform power ops
 *
 * An error is returned if the registration has been done previously.
 */
int __init mcpm_plat_power_register(struct platform_idle *idle)
{
	if (mcpm_plat_idle)
		return -EBUSY;
	mcpm_plat_idle = idle;

	return 0;
}

int __init mcpm_plat_get_cpuidle_states(struct cpuidle_state *states)
{
	int i;

	if (!states || !mcpm_plat_idle)
		return -EINVAL;

	for (i = 0; i < CPUIDLE_STATE_MAX &&
		    i < mcpm_plat_idle->state_count; i++)
		memcpy(&states[i], &mcpm_plat_idle->states[i],
				sizeof(struct cpuidle_state));

	return i;
}

static const struct mcpm_platform_ops mcpm_plat_pm_power_ops = {
	.power_up		= mcpm_plat_pm_power_up,
	.power_down		= mcpm_plat_pm_power_down,
	.suspend		= mcpm_plat_pm_suspend,
	.powered_up		= mcpm_plat_pm_powered_up,
#ifdef CONFIG_ARM
	.power_down_finish	= mcpm_plat_pm_power_down_finish,
#endif
};

static void __init mcpm_plat_pm_usage_count_init(void)
{
	unsigned long mpidr;
	unsigned int cpu, cluster;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	BUG_ON(cpu >= MAX_CPUS_PER_CLUSTER || cluster >= MAX_NR_CLUSTERS);
	memset(mcpm_plat_pm_use_count, 0, sizeof(mcpm_plat_pm_use_count));
	mcpm_plat_pm_use_count[cluster][cpu] = 1;
}

static const struct of_device_id mcpm_of_match[] __initconst = {
	{ .compatible = "arm,mcpm",	},
	{},
};

static int __init mcpm_plat_of_init(void)
{
	struct device_node *np;
	const char *method;
	u32 id;
	int err = 0;

	np = of_find_matching_node(NULL, mcpm_of_match);
	if (!np || !of_device_is_available(np))
		return -ENODEV;

	pr_info("probing function IDs from device-tree\n");

	if (of_property_read_string(np, "method", &method)) {
		pr_warn("missing \"method\" property\n");
		err = -ENXIO;
		goto out_put_node;
	}

	if (!strcmp("hvc", method)) {
		invoke_mcpm_fn = __invoke_mcpm_fn_hvc;
	} else if (!strcmp("smc", method)) {
		invoke_mcpm_fn = __invoke_mcpm_fn_smc;
	} else {
		pr_warn("invalid \"method\" property: %s\n", method);
		err = -EINVAL;
		goto out_put_node;
	}

	if (!of_property_read_u32(np, "cpu_off", &id))
		mcpm_function_id[MCPM_FN_CPU_OFF] = id;

	if (!of_property_read_u32(np, "cpu_on", &id))
		mcpm_function_id[MCPM_FN_CPU_ON] = id;

out_put_node:
	of_node_put(np);
	return err;
}


static int __init mcpm_plat_pm_init(void)
{
	int ret;

	ret = mcpm_plat_of_init();
	if (ret)
		return ret;

	memset(mcpm_plat_enter_lpm, 0x0, sizeof(mcpm_plat_enter_lpm));

	/*
	 * TODO:Should check if hardware is initialized here.
	 * See vexpress_spc_check_loaded()
	 */
	mcpm_plat_pm_usage_count_init();

	ret = mcpm_platform_register(&mcpm_plat_pm_power_ops);
	if (!ret)
		ret = mcpm_sync_init(NULL);
	if (ret)
		pr_warn("Power ops initialized with error %d\n", ret);

	return ret;
}
early_initcall(mcpm_plat_pm_init);
