// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/cpumask.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kobject.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#if defined(CONFIG_SEC_FACTORY)
static unsigned int __read_mostly jigon;
module_param(jigon, uint, 0440);
#endif

static struct task_struct *clk_delay_task = NULL;

#if defined(CONFIG_CPU_BOOTING_CLK_LIMIT)
int save_scailing_min_freq(struct cpufreq_policy *policy, unsigned long val)
{
	int ret;

	ret = freq_qos_update_request(policy->min_freq_req, val);
    return ret;
}

int save_scailing_max_freq(struct cpufreq_policy *policy, unsigned long val)
{
	int ret;

	ret = freq_qos_update_request(policy->max_freq_req, val);
	return ret;
}

void get_cpu_min_max_freq(void)
{
	int cpu;
	int num = 0;
	struct cpufreq_policy *policy;

	for_each_possible_cpu(cpu) {
	policy = cpufreq_cpu_get(cpu);

	if (policy) {
		pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
			__func__, num, cpu, policy->min, policy->max);
		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
		}
	}
}

static int set_cpu_min_freq(unsigned int cpu, unsigned long freq)
{
	struct cpufreq_policy *policy;
	int ret = 0;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return -ENODEV;
	ret = save_scailing_min_freq(policy, freq);
	policy->min = freq;

	return ret;
}

static int set_cpu_max_freq(unsigned int cpu, unsigned long freq)
{
	struct cpufreq_policy *policy;
	int ret = 0;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return -ENODEV;

	ret = save_scailing_max_freq(policy, freq);
	policy->max = freq;

	return ret;
}
#endif

static int clk_delay_thread_function(void *data)
{
	unsigned long jiffies_delay = 0;
	unsigned long freq_cpu_0 = 0;
	unsigned long freq_cpu_4 = 0;
	unsigned long freq_cpu_7 = 0;

#if defined(CONFIG_SEC_FACTORY)
	if(!jigon) {
		pr_info("clk_adjust.jigon is off. skip cpu max clk limiting.\n");
		return 0;
	}
#endif

	jiffies_delay =  300 * 1000; // 300 seconds

	// target limit requested by HW owner
	freq_cpu_0 = 900000;
	freq_cpu_4 = 1350000;
	freq_cpu_7 = 1400000;

#if defined(CONFIG_CPU_BOOTING_CLK_LIMIT)
	set_cpu_min_freq(0, 300000);
	set_cpu_min_freq(4, 550000);
	set_cpu_min_freq(7, 600000);

	set_cpu_max_freq(0, freq_cpu_0);
	set_cpu_max_freq(4, freq_cpu_4);
	set_cpu_max_freq(7, freq_cpu_7);

	get_cpu_min_max_freq(); // For debugging

	pr_info("clk_delay_thread started, sleeping for %lu seconds...\n", jiffies_delay); // Changed from pr_debug to pr_info

	msleep_interruptible(jiffies_delay); // Corrected the argument to msleep_interruptible

	set_cpu_max_freq(0, 2147483647);
	set_cpu_max_freq(4, 2147483647);
	set_cpu_max_freq(7, 2147483647);
	get_cpu_min_max_freq(); // For debugging

	pr_info("clk_delay_thread woke up, clock restored to default. Exiting now...\n"); // Changed from pr_debug to pr_info
#endif

	return 0;
}

static int __init clk_adjust_init(void)
{

	pr_info("run clk_delay_thread_function\n");
	clk_delay_task = kthread_run(clk_delay_thread_function, NULL, "clk_delay"); // Shortened the thread name
	if (IS_ERR(clk_delay_task)) {
		pr_err("clk_delay module failed to start thread\n"); // Changed from pr_debug to pr_err
		return PTR_ERR(clk_delay_task);
	}

	pr_info("clk_adjust_init done\n");

	return 0;
}

module_init(clk_adjust_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek POWERHAL_CPU_CTRL");
MODULE_AUTHOR("MediaTek Inc.");
