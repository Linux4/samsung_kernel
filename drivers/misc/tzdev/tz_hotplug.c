/*
 * Copyright (C) 2015 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/completion.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/seqlock.h>

#include "sysdep.h"
#include "tz_hotplug.h"
#include "tzdev.h"

static DECLARE_COMPLETION(tzdev_hotplug_comp);

static cpumask_t swd_cpu_mask;
static cpumask_t nwd_cpu_mask;
static uint16_t tzdev_hotplug_counter;
static unsigned int tzdev_hotplug_run;
static DEFINE_SEQLOCK(tzdev_hotplug_lock);

static int tzdev_hotplug_callback(struct notifier_block *nfb,
			unsigned long action, void *hcpu);

static struct notifier_block tzdev_hotplug_notifier = {
	.notifier_call = tzdev_hotplug_callback,
};

void tzdev_check_cpu_mask(struct tz_iwd_cpu_mask *cpu_mask)
{
	cpumask_t new_cpus_mask;
	unsigned long new_mask;

	/* Check cores activation */
	write_seqlock(&tzdev_hotplug_lock);
	if (cpu_mask->counter > tzdev_hotplug_counter ||
			tzdev_hotplug_counter - cpu_mask->counter > U16_MAX / 2) {
		new_mask = cpu_mask->mask;

		tzdev_hotplug_counter = cpu_mask->counter;
		cpumask_copy(&swd_cpu_mask, to_cpumask(&new_mask));
		write_sequnlock(&tzdev_hotplug_lock);

		cpumask_andnot(&new_cpus_mask, to_cpumask(&new_mask), cpu_online_mask);

		if (!cpumask_empty(&new_cpus_mask))
			complete(&tzdev_hotplug_comp);
		return;
	}
	write_sequnlock(&tzdev_hotplug_lock);
}

static void tzdev_cpus_up(cpumask_t mask)
{
	int cpu;

	for_each_cpu_mask(cpu, mask)
		if (!cpu_online(cpu)) {
			tzdev_print(0, "tzdev: enable cpu%d\n", cpu);
			cpu_up(cpu);
		}
}

void tzdev_update_nw_cpu_mask(unsigned long new_mask)
{
	cpumask_t new_cpus_mask;
	unsigned long new_mask_tmp = new_mask;

	write_seqlock(&tzdev_hotplug_lock);

	cpumask_copy(&nwd_cpu_mask, to_cpumask(&new_mask_tmp));

	write_sequnlock(&tzdev_hotplug_lock);

	cpumask_andnot(&new_cpus_mask, to_cpumask(&new_mask_tmp), cpu_online_mask);

	tzdev_cpus_up(new_cpus_mask);
}

static int tzdev_cpu_hotplug(void *data)
{
	cpumask_t cpu_mask_local;
	cpumask_t cpu_swd_mask_local;
	cpumask_t cpu_nwd_mask_local;
	int ret, seq;

	while (tzdev_hotplug_run) {
		ret = wait_for_completion_interruptible(&tzdev_hotplug_comp);

		do {
			seq = read_seqbegin(&tzdev_hotplug_lock);
			cpumask_copy(&cpu_swd_mask_local, &swd_cpu_mask);
			cpumask_copy(&cpu_nwd_mask_local, &nwd_cpu_mask);
		} while (read_seqretry(&tzdev_hotplug_lock, seq));

		cpumask_or(&cpu_mask_local, &cpu_swd_mask_local, &cpu_nwd_mask_local);

		tzdev_cpus_up(cpu_mask_local);
	}

	return 0;
}

static int tzdev_hotplug_callback(struct notifier_block *nfb,
			unsigned long action, void *hcpu)
{
	int seq, set;

	if (action == CPU_DOWN_PREPARE) {
		do {
			seq = read_seqbegin(&tzdev_hotplug_lock);
			set = cpu_isset((unsigned long) hcpu, swd_cpu_mask);
			set |= cpu_isset((unsigned long) hcpu, nwd_cpu_mask);
		} while (read_seqretry(&tzdev_hotplug_lock, seq));

		if (set) {
			tzdev_print(0, "deny cpu%ld shutdown\n", (unsigned long) hcpu);
			return NOTIFY_BAD;
		}
	}

	return NOTIFY_OK;
}

int tzdev_init_hotplug(void)
{
	void *th;

	/* Register PM notifier */
	register_cpu_notifier(&tzdev_hotplug_notifier);

	tzdev_hotplug_run = 1;
	th = kthread_run(tzdev_cpu_hotplug, NULL, "tzdev_hotplug");
	if (IS_ERR(th)) {
		printk("Can't start tzdev_cpu_hotplug thread\n");
		return PTR_ERR(th);
	}
	return 0;
}

void tzdev_exit_hotplug(void)
{
	tzdev_hotplug_run = 0;
	complete(&tzdev_hotplug_comp);

	unregister_cpu_notifier(&tzdev_hotplug_notifier);
}
