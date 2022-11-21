/*
 * arch/arm/mach-mmp/cpufreq-pxa988.c
 *
 * Copyright (C) 2012 Marvell, Inc.
 *
 * Author:
 *	Zhoujie Wu <zjwu@marvell.com>
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
#include <asm/system.h>
#include <asm/cpu.h>
#include <linux/pm_qos.h>
#ifdef CONFIG_EOF_FC_WORKAROUND
#include <mach/cputype.h>
#include <mach/pxa168fb.h>
#endif


#define NUM_CPUS	2
#define KHZ_TO_HZ	(1000)
#define MHZ_TO_KHZ	(1000)
#define MHZ_TO_HZ	(1000000)

static struct clk *cpu_clk;
static DEFINE_MUTEX(pxa988_cpu_lock);
static bool is_suspended;
static struct cpufreq_frequency_table *freq_table;

/*
 * cpufreq&driver&policy->min are the client for Qos min,
 * it determined cpu min request
 * Qos min&policy->max are client for Qos max,
 * it determines cpu max request
 */
/* Qos min request client cpufreq driver */
static struct pm_qos_request cpufreq_qos_req_min = {
	.name = "cpu_freqdrv",
};

/* Qos min request client policy->min */
static struct pm_qos_request policymin_qos_req_min = {
	.name = "cpu_policymin",
};

/* Qos max request client Qos min */
static struct pm_qos_request qosmin_qos_req_max = {
	.name = "cpu_qosmin",
};
/* Qos max request client policy->max */
static struct pm_qos_request policymax_qos_req_max = {
	.name = "cpu_policymax",
};

#ifdef CONFIG_EOF_FC_WORKAROUND
static int eof_fc_failure;
static unsigned int new_freqs;
DECLARE_COMPLETION(eof_complete);
atomic_t fc_trigger = ATOMIC_INIT(0);
atomic_t disable_c2 = ATOMIC_INIT(0);
extern atomic_t displayon;
#endif

#ifdef CONFIG_SMP
struct lpj_info {
	unsigned long	ref;
	unsigned int	freq;
};
static DEFINE_PER_CPU(struct lpj_info, lpj_ref);
#endif

/* cpufreq qos min/max notifier unit Mhz */
static int cpufreq_min_notify(struct notifier_block *b,
			      unsigned long min, void *v)
{
	if (pm_qos_request_active(&qosmin_qos_req_max))
		pm_qos_update_request(&qosmin_qos_req_max,
			min);
	return NOTIFY_OK;
}

static struct notifier_block cpufreq_min_notifier = {
	.notifier_call = cpufreq_min_notify,
};

static int cpufreq_get_tgtfreq_idx(
	unsigned int target_freq, unsigned int relation,
	unsigned int *index)
{
	struct cpufreq_frequency_table optimal = {
		.index = ~0,
		.frequency = 0,
	};
	struct cpufreq_frequency_table suboptimal = {
		.index = ~0,
		.frequency = 0,
	};
	unsigned int i;

	switch (relation) {
	case CPUFREQ_RELATION_H:
		suboptimal.frequency = ~0;
		break;
	case CPUFREQ_RELATION_L:
		optimal.frequency = ~0;
		break;
	}

	for (i = 0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = freq_table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;
		switch (relation) {
		case CPUFREQ_RELATION_H:
			if (freq <= target_freq) {
				if (freq >= optimal.frequency) {
					optimal.frequency = freq;
					optimal.index = i;
				}
			} else {
				if (freq <= suboptimal.frequency) {
					suboptimal.frequency = freq;
					suboptimal.index = i;
				}
			}
			break;
		case CPUFREQ_RELATION_L:
			if (freq >= target_freq) {
				if (freq <= optimal.frequency) {
					optimal.frequency = freq;
					optimal.index = i;
				}
			} else {
				if (freq >= suboptimal.frequency) {
					suboptimal.frequency = freq;
					suboptimal.index = i;
				}
			}
			break;
		}
	}
	if (optimal.index > i) {
		if (suboptimal.index > i)
			return -EINVAL;
		*index = suboptimal.index;
	} else
		*index = optimal.index;
	return 0;
}

static int cpufreq_max_notify(struct notifier_block *b,
			      unsigned long max, void *v)
{
	int ret;
	unsigned int index;
	unsigned long freq = max * MHZ_TO_HZ;

	/*
	 * find a frequency in table, we didn't use help function
	 * cpufreq_frequency_table_target as it will consider policy->min and policy->max,
	 * which it is not updated here yet, and also new policy could not
	 * get here
	 */
	ret = cpufreq_get_tgtfreq_idx(max * MHZ_TO_KHZ,
		CPUFREQ_RELATION_H,
		&index);
	if (!ret)
		freq = freq_table[index].frequency * KHZ_TO_HZ;
	pr_debug("%s max %lu, freq %lu\n", __func__, max, freq);
	if (!clk_set_rate(cpu_clk, freq))
		return NOTIFY_OK;
	else
		return NOTIFY_BAD;
}

static struct notifier_block cpufreq_max_notifier = {
	.notifier_call = cpufreq_max_notify,
};

static int policy_limiter_notify(struct notifier_block *nb,
	unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	if (val == CPUFREQ_ADJUST) {
		pm_qos_update_request(&policymin_qos_req_min,
			policy->min / MHZ_TO_KHZ);
		pm_qos_update_request(&policymax_qos_req_max,
			policy->max / MHZ_TO_KHZ);
	}
	return NOTIFY_OK;
}

static struct notifier_block policy_limiter_notifier = {
	.notifier_call = policy_limiter_notify,
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

#ifdef CONFIG_EOF_FC_WORKAROUND
int wakeup_fc_seq(void)
{
	if (atomic_read(&fc_trigger)) {
		atomic_set(&fc_trigger, 0);
		pr_debug("EOF_FC: start fc!\n");
		pm_qos_update_request(&cpufreq_qos_req_min,
				new_freqs / MHZ_TO_KHZ);
		pr_debug("EOF_FC: eof done!\n");
		complete(&eof_complete);
	}

	return 0;
}
#endif

static int pxa988_update_cpu_speed(unsigned long rate)
{
	int ret = 0;
	struct cpufreq_freqs freqs;
	unsigned int i;

	freqs.old = pxa988_getspeed(0);
	freqs.new = rate;

	if (freqs.old == freqs.new)
		return ret;

	/* cpufreq notifier is moved to cpu's ops->setrate
	 * So that when other components call cpu's ops->setrate
	 * the notifier can be called correctly.
	 */
#ifdef CONFIG_CPU_FREQ_DEBUG
	printk(KERN_DEBUG "cpufreq-pxa988: transition: %u --> %u\n",
	       freqs.old, freqs.new);
#endif

#ifdef CONFIG_EOF_FC_WORKAROUND
	pr_debug("EOF_FC: cpu freq to %d\n", freqs.new);

	/*
	 * Due to voltage increase time cost, we only use EOF workaround
	 * for the freq-chg which do not require voltage increase. So, this
	 * workaround only used for decreasing DDR frequency to 150Mhz while
	 * LCD is on. In addition, only Z1/Z2 need safe PP while Z1 does not
	 * have LCD, so this workaournd is only necessary for Z2.
	 */
	eof_fc_failure = 0;

	if ((cpu_is_pxa988_z2() || cpu_is_pxa986_z2())
			&& (freqs.old >= 600000) && (freqs.new < 600000)
			&& atomic_read(&displayon)) {
		new_freqs = freqs.new;
		/* Enable LCD panel path EOF interrupt before DDR freq-chg */
		irq_unmask_eof(0);
		atomic_set(&disable_c2, 1);
		atomic_set(&fc_trigger, 1);
		/*
		 * Now after the fc is triggered, we will DO freq-chg at first
		 * EOF, so we should never meet the 50ms timeout case.
		 */
		if (!wait_for_completion_timeout(&eof_complete,
				msecs_to_jiffies(50))) {
			if (atomic_read(&fc_trigger)) {
				atomic_set(&fc_trigger, 0);
				pr_debug("EOF_FC: wait eof timeout! force fc!\n");
				pm_qos_update_request(&cpufreq_qos_req_min,
						freqs.new / MHZ_TO_KHZ);
				if (ret)
					eof_fc_failure = 1;
			} else {
				pr_debug("EOF_FC: fc is running, wait!\n");
				wait_for_completion(&eof_complete);
			}
		} else
			pr_debug("EOF_FC: wait eof done!\n");
		atomic_set(&disable_c2, 0);
		/* Disable LCD panel path EOF interrupt after DDR freq-chg */
		irq_mask_eof(0);
	} else
		pm_qos_update_request(&cpufreq_qos_req_min,
				freqs.new / MHZ_TO_KHZ);

	if (ret || (eof_fc_failure == 1)) {
		pr_err("cpu-pxa988: Failed to set cpu frequency to %d kHz\n",
			freqs.new);
		freqs.new = freqs.old;
	}
#else
	pm_qos_update_request(&cpufreq_qos_req_min,
			freqs.new / MHZ_TO_KHZ);
	if (ret) {
		pr_err("cpu-pxa988: Failed to set cpu frequency to %d kHz\n",
			freqs.new);
		freqs.new = freqs.old;
	}
#endif

#ifdef CONFIG_SMP
	/*
	 * Note that loops_per_jiffy is not updated on SMP systems in
	 * cpufreq driver. So, update the per-CPU loops_per_jiffy value
	 * on frequency transition. We need to update all dependent CPUs.
	 * But we don't adjust the global one as it will always
	 * kept as the value according to the highest core freq,
	 * and used for udelay.
	 */
	for_each_online_cpu(i) {
		struct lpj_info *lpj = &per_cpu(lpj_ref, i);
		if (!lpj->freq) {
			lpj->ref = per_cpu(cpu_data, i).loops_per_jiffy;
			lpj->freq = freqs.old;
		}

		per_cpu(cpu_data, i).loops_per_jiffy =
			cpufreq_scale(lpj->ref, lpj->freq, freqs.new);
	}
#endif

	return ret;
}

static int pxa988_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	int idx;
	unsigned int freq;
	int ret = 0;

	mutex_lock(&pxa988_cpu_lock);

	if (is_suspended) {
		ret = -EBUSY;
		goto out;
	}

	cpufreq_frequency_table_target(policy, freq_table, target_freq,
		relation, &idx);

	freq = freq_table[idx].frequency;

	ret = pxa988_update_cpu_speed(freq);
out:
	mutex_unlock(&pxa988_cpu_lock);
	return ret;
}


static int pxa988_pm_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	static unsigned int saved_cpuclk = 0;
	mutex_lock(&pxa988_cpu_lock);
	if (event == PM_SUSPEND_PREPARE) {
		/* scaling to the min frequency before entering suspend */
		saved_cpuclk = pxa988_getspeed(0);
		pxa988_update_cpu_speed(freq_table[0].frequency);
		is_suspended = true;
		pr_info("%s: disable cpu freq-chg before suspend, cur"\
				" rate %dKhz\n",
				__func__, pxa988_getspeed(0));
	} else if (event == PM_POST_SUSPEND) {
		is_suspended = false;
		pxa988_update_cpu_speed(saved_cpuclk);
		pr_info("%s: enable cpu freq-chg after resume, cur"\
				" rate %dKhz\n",
				__func__, pxa988_getspeed(0));
	}
	mutex_unlock(&pxa988_cpu_lock);
	return NOTIFY_OK;
}

static struct notifier_block pxa988_cpu_pm_notifier = {
	.notifier_call = pxa988_pm_notify,
};

static int pxa988_cpufreq_init(struct cpufreq_policy *policy)
{
	if (policy->cpu >= NUM_CPUS)
		return -EINVAL;

	cpu_clk = clk_get_sys(NULL, "cpu");
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

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
	if (policy->cpu == 0) {
		register_pm_notifier(&pxa988_cpu_pm_notifier);
		pm_qos_add_request(&cpufreq_qos_req_min,
				PM_QOS_CPUFREQ_MIN,
				policy->cur / MHZ_TO_KHZ);
		pm_qos_add_request(&policymin_qos_req_min,
				PM_QOS_CPUFREQ_MIN,
				policy->min / MHZ_TO_KHZ);
		pm_qos_add_request(&qosmin_qos_req_max,
				PM_QOS_CPUFREQ_MAX,
				pm_qos_request(PM_QOS_CPUFREQ_MIN));
		pm_qos_add_request(&policymax_qos_req_max,
				PM_QOS_CPUFREQ_MAX,
				policy->max / MHZ_TO_KHZ);
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
	.verify		= pxa988_verify_speed,
	.target		= pxa988_target,
	.get		= pxa988_getspeed,
	.init		= pxa988_cpufreq_init,
	.exit		= pxa988_cpufreq_exit,
	.name		= "pxa988-cpufreq",
	.attr		= pxa988_cpufreq_attr,
};

static int __init cpufreq_init(void)
{
	pm_qos_add_notifier(PM_QOS_CPUFREQ_MIN,
		&cpufreq_min_notifier);
	pm_qos_add_notifier(PM_QOS_CPUFREQ_MAX,
		&cpufreq_max_notifier);
	cpufreq_register_notifier(&policy_limiter_notifier,
			CPUFREQ_POLICY_NOTIFIER);
	return cpufreq_register_driver(&pxa988_cpufreq_driver);
}

static void __exit cpufreq_exit(void)
{
	cpufreq_unregister_driver(&pxa988_cpufreq_driver);
}


MODULE_DESCRIPTION("cpufreq driver for Marvell PXA988");
MODULE_LICENSE("GPL");
module_init(cpufreq_init);
module_exit(cpufreq_exit);
