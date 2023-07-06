/* drivers/samsung/pm/sec_pm_cpufreq.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Author: Minsung Kim <ms925.kim@samsung.com>
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sec_pm_cpufreq.h>

static LIST_HEAD(sec_pm_cpufreq_list);

static unsigned long throttle_count;

static int sec_pm_cpufreq_set_max_freq(struct sec_pm_cpufreq_dev *cpufreq_dev,
				 unsigned int level)
{
	unsigned int max_freq;

	if (WARN_ON(level > cpufreq_dev->max_level))
		return -EINVAL;

	if (cpufreq_dev->cur_level == level)
		return 0;

	max_freq = cpufreq_dev->freq_table[level].frequency;
	cpufreq_dev->cur_level = level;
	cpufreq_dev->max_freq = max_freq;

	pr_info("%s: throttle cpu%d : %u KHz\n", __func__,
			cpufreq_dev->policy->cpu, max_freq);

	cpufreq_update_policy(cpufreq_dev->policy->cpu);

	return 0;
}

static unsigned long get_level(struct sec_pm_cpufreq_dev *cpufreq_dev,
			       unsigned long freq)
{
	struct cpufreq_frequency_table *freq_table = cpufreq_dev->freq_table;
	unsigned long level;

	for (level = 1; level <= cpufreq_dev->max_level; level++)
		if (freq >= freq_table[level].frequency)
			break;

	return level;
}

/* For SMPL_WARN interrupt */
int sec_pm_cpufreq_throttle_by_one_step(void)
{
	struct sec_pm_cpufreq_dev *cpufreq_dev;
	unsigned long level;
	unsigned long freq;

	++throttle_count;

	list_for_each_entry(cpufreq_dev, &sec_pm_cpufreq_list, node) {
		if (!cpufreq_dev->policy || !cpufreq_dev->freq_table) {
			pr_warn("%s: No cpufreq_dev\n", __func__);
			continue;
		}

		/* Skip LITTLE cluster */
		if (!cpufreq_dev->policy->cpu)
			continue;

		freq = cpufreq_dev->freq_table[0].frequency / 2;
		level = get_level(cpufreq_dev, freq);
		level += throttle_count;

		if (level > cpufreq_dev->max_level)
			level = cpufreq_dev->max_level;

		sec_pm_cpufreq_set_max_freq(cpufreq_dev, level);
	}

	return throttle_count;
}
EXPORT_SYMBOL_GPL(sec_pm_cpufreq_throttle_by_one_step);

void sec_pm_cpufreq_unthrottle(void)
{
	struct sec_pm_cpufreq_dev *cpufreq_dev;

	pr_info("%s: throttle_count: %lu\n", __func__, throttle_count);

	if (!throttle_count)
		return;

	throttle_count = 0;

	list_for_each_entry(cpufreq_dev, &sec_pm_cpufreq_list, node) {
		sec_pm_cpufreq_set_max_freq(cpufreq_dev, 0);
	}
}
EXPORT_SYMBOL_GPL(sec_pm_cpufreq_unthrottle);

static int sec_pm_cpufreq_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned long max_freq;
	struct sec_pm_cpufreq_dev *cpufreq_dev;

	if (event != CPUFREQ_ADJUST)
		return NOTIFY_DONE;

	list_for_each_entry(cpufreq_dev, &sec_pm_cpufreq_list, node) {
		/*
		 * A new copy of the policy is sent to the notifier and can't
		 * compare that directly.
		 */
		if (policy->cpu != cpufreq_dev->policy->cpu)
			continue;

		max_freq = cpufreq_dev->max_freq;

		if (policy->max > max_freq)
			cpufreq_verify_within_limits(policy, 0, max_freq);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block sec_pm_cpufreq_notifier_block = {
	.notifier_call = sec_pm_cpufreq_notifier,
};

static unsigned int find_next_max(struct cpufreq_frequency_table *table,
				  unsigned int prev_max)
{
	struct cpufreq_frequency_table *pos;
	unsigned int max = 0;

	cpufreq_for_each_valid_entry(pos, table) {
		if (pos->frequency > max && pos->frequency < prev_max)
			max = pos->frequency;
	}

	return max;
}

static struct sec_pm_cpufreq_dev *
__sec_pm_cpufreq_register(struct cpufreq_policy *policy)
{
	struct sec_pm_cpufreq_dev *cpufreq_dev;
	void *ret;
	unsigned int freq, i;
	bool first;

	if (IS_ERR_OR_NULL(policy)) {
		pr_err("%s: cpufreq policy isn't valid: %p\n", __func__, policy);
		return ERR_PTR(-EINVAL);
	}

	i = cpufreq_table_count_valid_entries(policy);
	if (!i) {
		pr_err("%s: CPUFreq table not found or has no valid entries\n",
			 __func__);
		return ERR_PTR(-ENODEV);
	}

	cpufreq_dev = kzalloc(sizeof(*cpufreq_dev), GFP_KERNEL);
	if (!cpufreq_dev)
		return ERR_PTR(-ENOMEM);

	cpufreq_dev->policy = policy;

	cpufreq_dev->max_level = i - 1;

	cpufreq_dev->freq_table = kmalloc_array(i,
					sizeof(*cpufreq_dev->freq_table),
					GFP_KERNEL);
	if (!cpufreq_dev->freq_table) {
		pr_err("%s: fail to allocate freq_table\n", __func__);
		ret = ERR_PTR(-ENOMEM);
		goto free_cpufreq_dev;
	}

	/* Fill freq-table in descending order of frequencies */
	for (i = 0, freq = -1; i <= cpufreq_dev->max_level; i++) {
		freq = find_next_max(policy->freq_table, freq);
		cpufreq_dev->freq_table[i].frequency = freq;

		/* Warn for duplicate entries */
		if (!freq)
			pr_warn("%s: table has duplicate entries\n", __func__);
		else
			pr_debug("%s: freq:%u KHz\n", __func__, freq);
	}

	cpufreq_dev->max_freq = cpufreq_dev->freq_table[0].frequency;

	/* Register the notifier for first cpufreq device */
	first = list_empty(&sec_pm_cpufreq_list);
	list_add(&cpufreq_dev->node, &sec_pm_cpufreq_list);

	if (first)
		cpufreq_register_notifier(&sec_pm_cpufreq_notifier_block,
					  CPUFREQ_POLICY_NOTIFIER);

	return cpufreq_dev;

free_cpufreq_dev:
	kfree(cpufreq_dev);

	return ret;
}

struct sec_pm_cpufreq_dev *
sec_pm_cpufreq_register(struct cpufreq_policy *policy)
{
	pr_info("%s\n", __func__);

	return __sec_pm_cpufreq_register(policy);
}
EXPORT_SYMBOL_GPL(sec_pm_cpufreq_register);

void sec_pm_cpufreq_unregister(struct sec_pm_cpufreq_dev *cpufreq_dev)
{
	bool last;

	pr_info("%s\n", __func__);

	if (!cpufreq_dev)
		return;

	list_del(&cpufreq_dev->node);
	/* Unregister the notifier for the last cpufreq device */
	last = list_empty(&sec_pm_cpufreq_list);

	if (last)
		cpufreq_unregister_notifier(&sec_pm_cpufreq_notifier_block,
					    CPUFREQ_POLICY_NOTIFIER);

	kfree(cpufreq_dev->freq_table);
	kfree(cpufreq_dev);
}
EXPORT_SYMBOL_GPL(sec_pm_cpufreq_unregister);

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("SEC PM CPU Frequency Control");
MODULE_LICENSE("GPL");
