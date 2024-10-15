/* drivers/samsung/pm/sec_pm_smpl_warn.c
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
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/sec_pm_smpl_warn.h>
#include <soc/samsung/freq-qos-tracer.h>

struct cpufreq_dev {
	struct cpufreq_frequency_table *freq_table;
	unsigned int max_freq;
	unsigned int max_level;
	unsigned int cur_level;
	struct cpufreq_policy *policy;
	struct list_head node;
	struct freq_qos_request qos_req;
	char qos_name[32];
};

struct sec_pm_smpl_warn_info {
	struct device			*dev;
	int				num_policy_cpus;
	u32				*policy_cpus;
};

static DEFINE_MUTEX(cpufreq_dev_list_lock);
static LIST_HEAD(cpufreq_dev_list);

static unsigned long throttle_count;
static bool smpl_warn_init_done;

static int sec_pm_smpl_warn_set_max_freq(struct cpufreq_dev *cdev,
				 unsigned int level)
{
	if (WARN_ON(level > cdev->max_level))
		return -EINVAL;

	if (cdev->cur_level == level)
		return 0;

	cdev->max_freq = cdev->freq_table[level].frequency;
	cdev->cur_level = level;

	pr_info("%s: throttle cpu%d : %u KHz\n", __func__,
			cdev->policy->cpu, cdev->max_freq);

	return freq_qos_update_request(&cdev->qos_req,
				cdev->freq_table[level].frequency);
}

static unsigned long get_level(struct cpufreq_dev *cdev,
			       unsigned long freq)
{
	struct cpufreq_frequency_table *freq_table = cdev->freq_table;
	unsigned long level;

	for (level = 1; level <= cdev->max_level; level++)
		if (freq >= freq_table[level].frequency)
			break;

	return level;
}

/* This function should be called during SMPL_WARN interrupt is active */
int sec_pm_smpl_warn_throttle_by_one_step(void)
{
	struct cpufreq_dev *cdev;
	unsigned long level;
	unsigned long freq;

	if (unlikely(!smpl_warn_init_done)) {
		pr_warn("%s: Not initialized\n", __func__);
		return -EPERM;
	}

	++throttle_count;

	list_for_each_entry(cdev, &cpufreq_dev_list, node) {
		if (!cdev->policy || !cdev->freq_table) {
			pr_warn("%s: No cdev\n", __func__);
			continue;
		}

		/* Skip LITTLE cluster */
		if (!cdev->policy->cpu)
			continue;

		freq = cdev->freq_table[0].frequency / 2;
		level = get_level(cdev, freq);
		level += throttle_count;

		if (level > cdev->max_level)
			level = cdev->max_level;

		sec_pm_smpl_warn_set_max_freq(cdev, level);
	}

	return throttle_count;
}
EXPORT_SYMBOL_GPL(sec_pm_smpl_warn_throttle_by_one_step);

void sec_pm_smpl_warn_unthrottle(void)
{
	struct cpufreq_dev *cdev;

	if (unlikely(!smpl_warn_init_done)) {
		pr_warn("%s: Not initialized\n", __func__);
		return;
	}

	pr_info("%s: throttle_count: %lu\n", __func__, throttle_count);

	if (!throttle_count)
		return;

	throttle_count = 0;

	list_for_each_entry(cdev, &cpufreq_dev_list, node)
		sec_pm_smpl_warn_set_max_freq(cdev, 0);
}
EXPORT_SYMBOL_GPL(sec_pm_smpl_warn_unthrottle);

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

static int sec_pm_smpl_warn_parse_dt(struct sec_pm_smpl_warn_info *info)
{
	struct device_node *dn;
	int ret;

	if (!info)
		return -ENODEV;

	dn = info->dev->of_node;

	info->num_policy_cpus = of_property_count_u32_elems(dn, "policy_cpus");
	if (info->num_policy_cpus <= 0)
		return -EINVAL;

	info->policy_cpus = devm_kcalloc(info->dev, info->num_policy_cpus,
			sizeof(*info->policy_cpus), GFP_KERNEL);
	if (!info->policy_cpus)
		return -ENOMEM;

	ret = of_property_read_u32_array(dn, "policy_cpus", info->policy_cpus,
			info->num_policy_cpus);

	return ret;
}

static int init_cpufreq_dev(struct sec_pm_smpl_warn_info *info, unsigned int idx)
{
	struct cpufreq_dev *cdev;
	struct cpufreq_policy *policy;
	int ret;
	unsigned int freq, i;

	policy = cpufreq_cpu_get(info->policy_cpus[idx]);

	if (IS_ERR_OR_NULL(policy)) {
		dev_err(info->dev, "%s: cpufreq policy isn't valid: %pK\n",
				__func__, policy);
		return -EINVAL;
	}

	i = cpufreq_table_count_valid_entries(policy);
	if (!i) {
		dev_err(info->dev, "%s: CPUFreq table not found or has no valid entries\n",
				__func__);
		return -ENODEV;
	}

	cdev = devm_kzalloc(info->dev, sizeof(*cdev), GFP_KERNEL);
	if (!cdev)
		return -ENOMEM;

	cdev->policy = policy;
	cdev->max_level = i - 1;

	cdev->freq_table = devm_kmalloc_array(info->dev, i,
			sizeof(*cdev->freq_table), GFP_KERNEL);
	if (!cdev->freq_table)
		return -ENOMEM;

	/* Fill freq_table in descending order of frequencies */
	for (i = 0, freq = -1; i <= cdev->max_level; i++) {
		freq = find_next_max(policy->freq_table, freq);
		cdev->freq_table[i].frequency = freq;

		/* Warn for duplicate entries */
		if (!freq)
			dev_warn(info->dev, "%s: table has duplicate entries\n", __func__);
		else
			dev_dbg(info->dev, "%s: freq:%u KHz\n", __func__, freq);
	}

	cdev->max_freq = cdev->freq_table[0].frequency;

	snprintf(cdev->qos_name, sizeof(cdev->qos_name), "smpl_warn_%u",
			info->policy_cpus[idx]);

	ret = freq_qos_tracer_add_request_name(cdev->qos_name,
			&policy->constraints,
			&cdev->qos_req, FREQ_QOS_MAX,
			cdev->freq_table[0].frequency);
	if (ret < 0) {
		pr_err("%s: Failed to add freq constraint (%d)\n", __func__,
				ret);
		return ret;
	}

	mutex_lock(&cpufreq_dev_list_lock);
	list_add(&cdev->node, &cpufreq_dev_list);
	mutex_unlock(&cpufreq_dev_list_lock);

	return 0;
}

static int sec_pm_smpl_warn_probe(struct platform_device *pdev)
{
	struct sec_pm_smpl_warn_info *info;
	unsigned int i;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;

	ret = sec_pm_smpl_warn_parse_dt(info);
	if (ret) {
		dev_err(info->dev, "%s: fail to parse dt(%d)\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < info->num_policy_cpus; i++) {
		ret = init_cpufreq_dev(info, i);
		if (ret < 0) {
			dev_err(info->dev, "%s: failt to initialize cpufreq_dev(%u)\n",
					__func__, i);
			return ret;
		}
	}

	smpl_warn_init_done = true;

	return 0;
}

static int sec_pm_smpl_warn_remove(struct platform_device *pdev)
{
	struct cpufreq_dev *cdev;

	pr_info("%s\n", __func__);

	mutex_lock(&cpufreq_dev_list_lock);
	list_for_each_entry(cdev, &cpufreq_dev_list, node) {
		list_del(&cdev->node);
		freq_qos_tracer_remove_request(&cdev->qos_req);
	}
	mutex_unlock(&cpufreq_dev_list_lock);

	return 0;
}

static const struct of_device_id sec_pm_smpl_warn_match[] = {
	{ .compatible = "samsung,sec-pm-smpl-warn", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_pm_smpl_warn_match);

static struct platform_driver sec_pm_smpl_warn_driver = {
	.driver = {
		.name = "sec-pm-smpl_warn",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_smpl_warn_match),
	},
	.probe = sec_pm_smpl_warn_probe,
	.remove = sec_pm_smpl_warn_remove,
};

module_platform_driver(sec_pm_smpl_warn_driver);

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("SEC PM SMPL_WARN Control");
MODULE_LICENSE("GPL");
