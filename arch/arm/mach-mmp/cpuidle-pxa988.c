/*
 * linux/arch/arm/mach-mmp/cpuidle-pxa988.c
 *
 * Author:	Raul Xiong <xjian@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpu_pm.h>
#include <linux/cpuidle.h>
#include <linux/export.h>
#include <linux/pm_qos.h>

#include <mach/regs-apmu.h>
#include <asm/io.h>
#include <asm/proc-fns.h>
#include <mach/pxa988_lowpower.h>
#include <mach/reset-pxa988.h>
#include <mach/cputype.h>

static bool cpu_done[NR_CPUS];
static enum pxa988_lowpower_mode pxa988_idle_mode[] = {
	PXA988_LPM_C1,
	PXA988_LPM_C2,
	PXA988_LPM_D1P,
	PXA988_LPM_D1,
	PXA988_LPM_D2,
};

struct cpuidle_params {
	u32 exit_latency;	/* exit_latency = sleep + wake-up latencies */
	u32 target_residency;
};

static struct cpuidle_params cpuidle_params_table[] = {
	/* C1 */
	{18, 36},
	/* C2 */
	{450, 900},
	/* D1P */
	{500, 1000},
	/* D1 */
	{600, 1200},
};
#define PXA988_NUM_STATES ARRAY_SIZE(cpuidle_params_table)

static struct cpuidle_driver pxa988_idle_driver = {
	.name = "pxa988_idle",
	.owner = THIS_MODULE,
	.state_count = PXA988_NUM_STATES,
	.en_core_tk_irqen = 1,
};

static DEFINE_PER_CPU(struct cpuidle_device, pxa988_cpuidle_device);

static atomic_t abort_barrier;

static inline int has_feat_legacy_apmu_core_status(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z3();
}

static int pxa988_enter_lpm(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	enum pxa988_lowpower_mode *power_mode = cpuidle_get_statedata(&dev->states_usage[index]);
	int real_mode;		/*indicate the actual */

	if (*power_mode == PXA988_LPM_C1) {
		pxa988_enter_c1(dev->cpu);
		return index;
	}

	local_fiq_disable();
	/*
	 * CPU0 has to wait and stay ON until CPU1 is OFF state.
	 */
	if (dev->cpu == 0 && cpumask_test_cpu(1, cpu_online_mask)) {
		u32 core_state;
		do {
			if (has_feat_legacy_apmu_core_status())
				core_state = __raw_readl(APMU_CORE_STATUS) &
					(1 << (3 + 2 * 1));
			else
				core_state = __raw_readl(APMU_CORE_STATUS) &
					(1 << (4 + 3 * 1));
			cpu_relax();
			/*
			 * CPU1 could have already entered & exited idle
			 * without hitting off because of a failed attempt
			 * to low power mode.  Check for that here, otherwise
			 * we could spin forever waiting for CPU1 off.
			 */
			if (cpu_done[1]) {
				real_mode = PXA988_LPM_C1;
				goto fail;
			}
		} while (!core_state);
	}

	real_mode = pxa988_enter_lowpower(dev->cpu, *power_mode);

	cpu_done[dev->cpu] = true;
	/* Wakeup CPU1 only if it is not offlined */
	if (dev->cpu == 0 && cpumask_test_cpu(1, cpu_online_mask)) {
		smp_send_reschedule(1);
		check_and_swrst_core1();
	}
fail:
	cpuidle_coupled_parallel_barrier(dev, &abort_barrier);
	cpu_done[dev->cpu] = false;
	for (index = 0; index < CPUIDLE_STATE_MAX; index++)
		if (real_mode == *(int *) \
			cpuidle_get_statedata(&dev->states_usage[index]))
			break;

	local_fiq_enable();

	return index;
}

/* Helper to fill the C-state common data*/
static inline void _fill_state(struct cpuidle_driver *drv,
				int idx, const char *name, const char *descr)
{
	struct cpuidle_state *state = &drv->states[idx];

	state->exit_latency	= cpuidle_params_table[idx].exit_latency;
	state->target_residency	= cpuidle_params_table[idx].target_residency;
	if (idx <= drv->safe_state_index )
		state->flags	= CPUIDLE_FLAG_TIME_VALID;
	else
		state->flags	= CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_COUPLED;
	state->enter		= pxa988_enter_lpm;
	strncpy(state->name, name, CPUIDLE_NAME_LEN - 1);
	strncpy(state->desc, descr, CPUIDLE_DESC_LEN - 1);
}

static void fill_driver_state(void)
{
	struct cpuidle_driver *drv = &pxa988_idle_driver;
	/* C1, internal WFI */
	_fill_state(drv, 0, "C1", "C1: Core internal clock gated");
	/* C2, core power down */
	_fill_state(drv, 1, "C2", "C2: Core power down");
	/* D1P, core power down, AXI shutdown */
	_fill_state(drv, 2, "D1P", "D1P: AP subsystem idle");
	/* D1, core power down, AXI, DDR, APB shutdown, chip sleep */
	_fill_state(drv, 3, "D1", "D1: chip sleep");
}
static int pxa988_cpuidle_register_device(unsigned int cpu)
{
	struct cpuidle_device *device;

	device = &per_cpu(pxa988_cpuidle_device, cpu);
	device->cpu = cpu;
	device->coupled_cpus = *cpu_present_mask;
	device->safe_state_index = 0;

	cpuidle_set_statedata(&device->states_usage[0],
			&pxa988_idle_mode[0]);

	cpuidle_set_statedata(&device->states_usage[1],
			&pxa988_idle_mode[1]);

	cpuidle_set_statedata(&device->states_usage[2],
			&pxa988_idle_mode[2]);

	cpuidle_set_statedata(&device->states_usage[3],
			&pxa988_idle_mode[3]);

	device->state_count = PXA988_NUM_STATES;

	if (cpuidle_register_device(device)) {
		pr_err("CPU%u: failed to register cpuidle device\n", cpu);
		return -EIO;
	}

	return 0;
}

static int __init pxa988_cpuidle_init(void)
{
	int ret, cpu;

	ret = cpuidle_register_driver(&pxa988_idle_driver);
	if (ret)
		return ret;

	fill_driver_state();
	for_each_possible_cpu(cpu) {
		if (pxa988_cpuidle_register_device(cpu))
			pr_err("CPU%u: error registering cpuidle\n", cpu);
	}

	return 0;
}

module_init(pxa988_cpuidle_init);

