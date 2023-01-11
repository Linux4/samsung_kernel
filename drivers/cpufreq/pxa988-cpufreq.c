/*
 * arch/arm/mach-mmp/cpufreq-pxa988.c
 *
 * Copyright (C) 2012 Marvell, Inc.
 *
 * Author:
 *	Zhoujie Wu <zjwu@marvell.com>
 *	Xiaoguang Chen <chenxg@marvell.com>
 *	Based on arch/arm/mach-tegra/cpu-tegra.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <asm/system.h>

#define NUM_CPUS	num_possible_cpus()

#define KHZ_TO_HZ	(1000)
#define MHZ_TO_KHZ	(1000)
#define MHZ_TO_HZ	(1000000)

static struct clk *cpu_clk;
static DEFINE_MUTEX(pxa988_cpu_lock);
static bool is_suspended;
static struct cpufreq_frequency_table *freq_table;
static bool is_qosreq_inited;

/* Qos min request client cpufreq driver policy->cpuinfo.min */
static struct pm_qos_request cpufreq_qos_req_min = {
	.name = "cpu_freqmin",
};

/* Qos max request client  cpufreq driver policy->cpuinfo.min */
static struct pm_qos_request cpufreq_qos_req_max = {
	.name = "cpu_freqmax",
};

int pxa988_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

unsigned int pxa988_getspeed(unsigned int cpu)
{
	unsigned long rate;

	if (cpu >= NUM_CPUS)
		return 0;

	rate = clk_get_rate(cpu_clk) / KHZ_TO_HZ;
	return rate;
}

static int pxa988_update_cpu_speed(unsigned long rate)
{
	struct cpufreq_freqs freqs;
	struct cpufreq_policy *policy;
	int ret = 0;
	int cpu = 0;

	freqs.old = pxa988_getspeed(0);
	freqs.new = rate;

	if (freqs.old == freqs.new)
		return 0;

	policy = cpufreq_cpu_get(cpu);
	BUG_ON(!policy);
	pr_debug("cpufreq-pxa988: transition: %u --> %u\n",
		 freqs.old, freqs.new);
	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

	ret = clk_set_rate(cpu_clk, freqs.new * KHZ_TO_HZ);

	if (ret)
		freqs.new = freqs.old;

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
	cpufreq_cpu_put(policy);
	return ret;
}

/* Same as cpufreq_frequency_table_target, but no policy min/max check */
int cpufreq_table_target(struct cpufreq_policy *policy,
			 struct cpufreq_frequency_table *table,
			 unsigned int target_freq,
			 unsigned int relation, unsigned int *index)
{
	struct cpufreq_frequency_table optimal = {
		.driver_data = ~0,
		.frequency = 0,
	};
	struct cpufreq_frequency_table suboptimal = {
		.driver_data = ~0,
		.frequency = 0,
	};
	unsigned int i;

	pr_debug("request for target %u kHz (relation: %u) for cpu %u\n",
		 target_freq, relation, policy->cpu);

	switch (relation) {
	case CPUFREQ_RELATION_H:
		suboptimal.frequency = ~0;
		break;
	case CPUFREQ_RELATION_L:
		optimal.frequency = ~0;
		break;
	}

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		switch (relation) {
		case CPUFREQ_RELATION_H:
			if (freq <= target_freq) {
				if (freq >= optimal.frequency) {
					optimal.frequency = freq;
					optimal.driver_data = i;
				}
			} else {
				if (freq <= suboptimal.frequency) {
					suboptimal.frequency = freq;
					suboptimal.driver_data = i;
				}
			}
			break;
		case CPUFREQ_RELATION_L:
			if (freq >= target_freq) {
				if (freq <= optimal.frequency) {
					optimal.frequency = freq;
					optimal.driver_data = i;
				}
			} else {
				if (freq >= suboptimal.frequency) {
					suboptimal.frequency = freq;
					suboptimal.driver_data = i;
				}
			}
			break;
		}
	}
	if (optimal.driver_data > i) {
		if (suboptimal.driver_data > i)
			return -EINVAL;
		*index = suboptimal.driver_data;
	} else
		*index = optimal.driver_data;

	pr_debug("target is %u (%u kHz, %u)\n", *index, table[*index].frequency,
		 table[*index].driver_data);

	return 0;
}

static int pxa988_target(struct cpufreq_policy *policy,
			 unsigned int target_freq, unsigned int relation)
{
	int idx = 0;
	unsigned int freq, qos_min, qos_max;
	int ret = 0;

	mutex_lock(&pxa988_cpu_lock);

	if (is_suspended) {
		ret = -EBUSY;
		goto out;
	}

	/*
	 * Policy min and qos min have lower priority
	 * than policy max and qos max.
	 * Policy min and qos min has no priority order
	 * Policy max and qos max has no proirity order
	 */
	qos_min = (unsigned int)pm_qos_request(PM_QOS_CPUFREQ_MIN);
	qos_max = (unsigned int)pm_qos_request(PM_QOS_CPUFREQ_MAX);
	target_freq = max(policy->min, target_freq);
	target_freq = max(qos_min, target_freq);
	target_freq = min(policy->max, target_freq);
	target_freq = min(qos_max, target_freq);
	if ((target_freq == policy->max) || (target_freq == qos_max))
		relation = CPUFREQ_RELATION_H;
	cpufreq_table_target(policy, freq_table, target_freq, relation, &idx);
	freq = freq_table[idx].frequency;
	pr_debug("Qos_min:%d Qos_max:%d, Target:%d(KHZ)\n",
		 pm_qos_request(PM_QOS_CPUFREQ_MIN),
		 pm_qos_request(PM_QOS_CPUFREQ_MAX), freq);

	ret = pxa988_update_cpu_speed(freq);
out:
	mutex_unlock(&pxa988_cpu_lock);
	return ret;
}

static int pxa988_pm_notify(struct notifier_block *nb, unsigned long event,
			    void *dummy)
{
	static unsigned int saved_cpuclk;
	mutex_lock(&pxa988_cpu_lock);
	if (event == PM_SUSPEND_PREPARE) {
		/* scaling to the min frequency before entering suspend */
		saved_cpuclk = pxa988_getspeed(0);
		pxa988_update_cpu_speed(freq_table[0].frequency);
		is_suspended = true;
		pr_info("%s: disable cpu freq-chg before suspend", __func__);
		pr_info("cur rate %dKhz\n", pxa988_getspeed(0));
	} else if (event == PM_POST_SUSPEND) {
		is_suspended = false;
		pxa988_update_cpu_speed(saved_cpuclk);
		pr_info("%s: enable cpu freq-chg after resume", __func__);
		pr_info("cur rate %dKhz\n", pxa988_getspeed(0));
	}
	mutex_unlock(&pxa988_cpu_lock);
	return NOTIFY_OK;
}

static struct notifier_block pxa988_cpu_pm_notifier = {
	.notifier_call = pxa988_pm_notify,
};

/* cpufreq qos min/max notifier unit Khz */
static int cpufreq_min_notify(struct notifier_block *b,
			      unsigned long min, void *v)
{
	int ret;
	unsigned long freq, val = min;
	struct cpufreq_policy *policy;
	int cpu = 0;

	freq = pxa988_getspeed(cpu);
	if (freq >= val)
		return NOTIFY_OK;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return NOTIFY_BAD;

	ret = pxa988_target(policy, val, CPUFREQ_RELATION_L);
	cpufreq_cpu_put(policy);
	if (ret < 0)
		return NOTIFY_BAD;

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_min_notifier = {
	.notifier_call = cpufreq_min_notify,
};

static int cpufreq_max_notify(struct notifier_block *b,
			      unsigned long max, void *v)
{
	int ret;
	unsigned long freq, val = max;
	struct cpufreq_policy *policy;
	int cpu = 0;

	freq = pxa988_getspeed(cpu);

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return NOTIFY_BAD;

	ret = pxa988_target(policy, val, CPUFREQ_RELATION_H);
	cpufreq_cpu_put(policy);
	if (ret < 0)
		return NOTIFY_BAD;

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_max_notifier = {
	.notifier_call = cpufreq_max_notify,
};

static int pxa988_cpufreq_init(struct cpufreq_policy *policy)
{
	if (policy->cpu >= NUM_CPUS)
		return -EINVAL;

	if (unlikely(!cpu_clk)) {
		cpu_clk = __clk_lookup("cpu");
		if (IS_ERR(cpu_clk))
			return PTR_ERR(cpu_clk);
	}

	freq_table = cpufreq_frequency_get_table(policy->cpu);
	BUG_ON(!freq_table);
	cpufreq_frequency_table_cpuinfo(policy, freq_table);
	policy->cur = pxa988_getspeed(policy->cpu);

	/*
	 * FIXME: what's the actual transition time?
	 * use 10ms as sampling rate for bring up
	 */
	policy->cpuinfo.transition_latency = 10 * 1000;
	cpumask_setall(policy->cpus);
	if (unlikely(!is_qosreq_inited)) {
		pm_qos_add_request(&cpufreq_qos_req_min,
				   PM_QOS_CPUFREQ_MIN,
				   policy->cpuinfo.min_freq);
		pm_qos_add_request(&cpufreq_qos_req_max, PM_QOS_CPUFREQ_MAX,
				   policy->cpuinfo.max_freq);
		is_qosreq_inited = true;
	}

	return 0;
}

static int pxa988_cpufreq_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_cpuinfo(policy, freq_table);
	return 0;
}

static struct freq_attr *pxa988_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver pxa988_cpufreq_driver = {
	.flags = CPUFREQ_CONST_LOOPS,
	.verify = pxa988_verify_speed,
	.target = pxa988_target,
	.get = pxa988_getspeed,
	.init = pxa988_cpufreq_init,
	.exit = pxa988_cpufreq_exit,
	.name = "pxa988-cpufreq",
	.attr = pxa988_cpufreq_attr,
};

static int __init cpufreq_init(void)
{
	register_pm_notifier(&pxa988_cpu_pm_notifier);
	pm_qos_add_notifier(PM_QOS_CPUFREQ_MIN, &cpufreq_min_notifier);
	pm_qos_add_notifier(PM_QOS_CPUFREQ_MAX, &cpufreq_max_notifier);
	return cpufreq_register_driver(&pxa988_cpufreq_driver);
}

static void __exit cpufreq_exit(void)
{
	unregister_pm_notifier(&pxa988_cpu_pm_notifier);
	pm_qos_remove_notifier(PM_QOS_CPUFREQ_MIN, &cpufreq_min_notifier);
	pm_qos_remove_notifier(PM_QOS_CPUFREQ_MAX, &cpufreq_max_notifier);
	cpufreq_unregister_driver(&pxa988_cpufreq_driver);
}

MODULE_DESCRIPTION("cpufreq driver for Marvell PXA988");
MODULE_LICENSE("GPL");
module_init(cpufreq_init);
module_exit(cpufreq_exit);
