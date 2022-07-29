/*
 * Copyright (c) 2018 Hong Hyunji, Samsung Electronics Co., Ltd <hyunji.hong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos MOCE(MOCE that can Diversify Conditions for Entry into Idle State) driver implementation
 *
 * MOCE can vary the target residency and exit latency in accordance with the frequency
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/cpuidle.h>
#include <linux/cpufreq.h>
#include <linux/of_device.h>
#include <linux/cpuidle-moce.h>
#include <linux/kdev_t.h>

#include <soc/samsung/exynos-cpupm.h>

/*
 * Define the considered element as a number.
 * Used to read the ratios and weights variables.
 */
#define DEFAULT_RATIO		100
#define MIN_RATIO		10

DEFINE_PER_CPU(struct bias_cpuidle, bias_cpuidle);

/*****************************************************************************
 *                               HELPER FUNCTION                             *
 *****************************************************************************/

static struct factor *find_factor(struct bias_cpuidle *bias_idle, factor_type type)
{
	struct factor *pos;

	list_for_each_entry(pos, &bias_idle->factor_list, list)
		if (pos->type == type)
			return pos;

	return NULL;
}

static unsigned int find_ratio(unsigned int *table, unsigned int size, unsigned int value)
{
	int i;

	/* Searching correct value according to state of factor */
	for (i = 0 ; i < size - 1; i += 2)
		if (value < table[i + 1])
			break;

	return table[i];
}

static void update_total_ratio(struct bias_cpuidle *bias_idle)
{
	struct factor *pos;
	unsigned int num = 0, total_ratio = 0;

	if (list_empty(&bias_idle->factor_list))
		return;

	list_for_each_entry(pos, &bias_idle->factor_list, list) {
		total_ratio += pos->ratio;
		num++;
	}

	total_ratio /= num;

	/* limit the minimum ratio to prevent problems caused by excessive bias. */
	if (total_ratio < MIN_RATIO)
		bias_idle->total_ratio = MIN_RATIO;
	else
		bias_idle->total_ratio = total_ratio;
}

/*****************************************************************************
 *                           EXTERNAL REFERENCE APIs                         *
 *****************************************************************************/

/*
 * This function is called to bias target_residency values,
 * when deciding next_state in cpupm and idle governors.
 */
unsigned int exynos_moce_get_ratio(unsigned int cpu)
{
	struct bias_cpuidle *bias_idle = &per_cpu(bias_cpuidle, cpu);

	/* check have factor for the cpu */
	if (!bias_idle->biased)
		return DEFAULT_RATIO;

	return bias_idle->total_ratio;
}
EXPORT_SYMBOL(exynos_moce_get_ratio);

/*****************************************************************************
 *                            DEFINE NOTIFIER CALL                           *
 *****************************************************************************/

/*
 * cpufreq trans call back function :
 * if was changed cpufreq factor,
 * search and save the applied ratio in the per-cpu structure.
 */
static int exynos_moce_cpufreq_trans_notifier(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct bias_cpuidle *bias_idle;
	struct factor *factor;
	struct cpumask mask;
	int cpu;
	unsigned int cur_ratio;
	unsigned long flags;

	if (freq->flags & CPUFREQ_CONST_LOOPS)
		return NOTIFY_OK;

	if (val != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

	bias_idle = &per_cpu(bias_cpuidle, freq->cpu);

	/* check have factor for the cpu */
	if (!bias_idle->biased)
		return NOTIFY_OK;

	factor = find_factor(bias_idle, FREQ_FACTOR);
	if (!factor)
		return NOTIFY_DONE;

	/* find applied ratio */
	cur_ratio = find_ratio(factor->ratio_table, factor->size, freq->new);

	cpumask_copy(&mask, &factor->cpus);

	/* update value of the other sibling-cpus */
	for_each_cpu(cpu, &mask) {
		bias_idle = &per_cpu(bias_cpuidle, cpu);
		factor = find_factor(bias_idle, FREQ_FACTOR);
		if (!factor)
			return NOTIFY_DONE;

		spin_lock_irqsave(&factor->lock, flags);
		factor->ratio = cur_ratio;
		update_total_ratio(bias_idle);
		spin_unlock_irqrestore(&factor->lock, flags);
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_trans_nb = {
	.notifier_call = exynos_moce_cpufreq_trans_notifier,
};

/*****************************************************************************
 *                               SYSFS INTERFACES                            *
 *****************************************************************************/

/*
 * format:
 * {DomainID} {ratio} {freq}:{ratio} {freq}:{ratio} ...:{ratio}
 * converted into array:
 * [DomainID][ratio][freq][ratio][freq]...[ratio]
 */
static unsigned int *get_tokenized_data(const char *buf, int *num_tokens, unsigned int *cpu)
{
	const char *bbuf;
	int i, ntokens = 1;
	unsigned int *tokenized_data;

	bbuf = buf;
	while ((bbuf = strpbrk(bbuf + 1, " :")))
		ntokens++;

	if ((ntokens & 0x1))
		return NULL;

	tokenized_data = kzalloc((ntokens - 1) * sizeof(unsigned int), GFP_KERNEL);
	if (!tokenized_data)
		return NULL;

	bbuf = buf;
	i = 0;

	if (sscanf(bbuf, "%u", cpu) != 1)
		goto err_kfree;

	if (*cpu > NR_CPUS)
		goto err_kfree;

	bbuf = strpbrk(bbuf, " ");
	if (!bbuf)
		goto err_kfree;
	bbuf++;

	while (i < (ntokens - 1)) {
		if (sscanf(bbuf, "%u", &tokenized_data[i++]) != 1)
			goto err_kfree;

		/* Any percentage value except */
		if ((i & 0x1) == 1 && tokenized_data[i-1] < 10)
			goto err_kfree;
		bbuf = strpbrk(bbuf, " :");
		if (!bbuf)
			break;
		bbuf++;
	}

	if (i != (ntokens - 1))
		goto err_kfree;

	*num_tokens = ntokens - 1;

	return tokenized_data;

err_kfree:
	kfree(tokenized_data);
	return NULL;
}

static factor_type get_factor_type(const char *name)
{
	factor_type type = NO_FACTOR;

	if (!strcmp(name, "freq_factor"))
		type = FREQ_FACTOR;
	/* Add sysfs node for the new factor  */

	return type;
}

static ssize_t show_factor(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bias_cpuidle *bias_idle;
	struct factor *factor;
	int i, cpu;
	ssize_t size = 0;
	unsigned long flags;
	factor_type type = get_factor_type(attr->attr.name);

	if (type == NO_FACTOR)
		return -ENODEV;

	for_each_possible_cpu(cpu) {
		bias_idle = &per_cpu(bias_cpuidle, cpu);

		factor = find_factor(bias_idle, type);
		if (!factor)
			return size;

		if (cpu != cpumask_first(&factor->cpus))
			continue;

		spin_lock_irqsave(&factor->lock, flags);
		size += snprintf(buf + size, PAGE_SIZE - size, "[cpumask:%*pbl] ",
				cpumask_pr_args(&factor->cpus));

		for (i = 0; i < factor->size; i++)
			size += snprintf(buf + size, PAGE_SIZE - size, "%u%s",
					factor->ratio_table[i],
					i & 0x1 ? ":" : " ");

		size += snprintf(buf + size, PAGE_SIZE - size, "\n");
		spin_unlock_irqrestore(&factor->lock, flags);
	}

	return size;
}

static ssize_t store_factor(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct bias_cpuidle *bias_idle;
	struct factor *factor;
	struct cpumask mask;
	int ntokens;
	unsigned int cpu;
	unsigned int *new_ratio_table;
	unsigned int *old_ratio_table;
	unsigned long flags;
	factor_type type = get_factor_type(attr->attr.name);

	if (type == NO_FACTOR)
		return -ENODEV;

	/* convert input data into new ratio table */
	new_ratio_table = get_tokenized_data(buf, &ntokens, &cpu);
	if (!new_ratio_table)
		return -EINVAL;

	bias_idle = &per_cpu(bias_cpuidle, cpu);

	factor = find_factor(bias_idle, type);
	if (!factor) {
		kfree(new_ratio_table);
		return -EINVAL;
	}

	cpumask_copy(&mask, &factor->cpus);
	if (cpumask_empty(&mask)) {
		kfree(new_ratio_table);
		return -EINVAL;
	}

	old_ratio_table = factor->ratio_table;

	/* change ratio table of sibling-cpus */
	for_each_cpu(cpu, &mask) {
		bias_idle = &per_cpu(bias_cpuidle, cpu);

		factor = find_factor(bias_idle, type);
		if (!factor) {
			kfree(new_ratio_table);
			return -EINVAL;
		}

		spin_lock_irqsave(&factor->lock, flags);

		factor->ratio_table = new_ratio_table;
		factor->size = ntokens;

		spin_unlock_irqrestore(&factor->lock, flags);
	}

	kfree(old_ratio_table);

	return size;
}

static DEVICE_ATTR(freq_factor, S_IRUGO | S_IWUSR, show_factor, store_factor);
/* Add sysfs node for the new factor  */

static struct attribute *cpuidle_moce_attrs[] = {
	&dev_attr_freq_factor.attr,
	NULL,
};

static const struct attribute_group cpuidle_moce_group = {
	.attrs = cpuidle_moce_attrs,
};

/*****************************************************************************
 *                       INITIALIZE EXYNOS MOCE DRIVER                       *
 *****************************************************************************/

extern struct class *idle_class;

static int __init init_sysfs(void)
{
	struct device *dev;

	dev = device_create(idle_class, NULL, MKDEV(0, 1), NULL, "moce_factor");

	if (sysfs_create_group(&dev->kobj, &cpuidle_moce_group))
		return -ENODEV;

	return 0;
}

static struct factor *__init init_factor_data(struct device_node *factor_node, int cpu)
{
	struct bias_cpuidle *bias_first;
	struct factor *factor, *first;
	const char *buf;

	/* alloc factor */
	factor = kzalloc(sizeof(struct factor), GFP_KERNEL);
	if (!factor)
		return NULL;

	/* factor sibling-cpus */
	if (of_property_read_string(factor_node, "sibling-cpus", &buf))
		goto moce_fail;

	cpulist_parse(buf, &factor->cpus);
	if (cpumask_weight(&factor->cpus) == 0)
		goto moce_fail;

	/* factor type */
	if (of_property_read_u32(factor_node, "type", &factor->type))
		goto moce_fail;

	/* factor ratio-table */
	factor->size = of_property_count_u32_elems(factor_node, "ratio-table");
	if (factor->size < 0)
		goto moce_fail;

	/* If there is a same ratio table previously created, just points to it. */
	if (cpu == cpumask_first(&factor->cpus)) {
		/* alloc factor ratio-table */
		factor->ratio_table = kzalloc(sizeof(unsigned int) * factor->size, GFP_KERNEL);
		if (!factor->ratio_table)
			goto moce_fail;

		of_property_read_u32_array(factor_node, "ratio-table",
				factor->ratio_table, factor->size);
	} else {
		bias_first = &per_cpu(bias_cpuidle, cpumask_first(&factor->cpus));
		first = find_factor(bias_first, factor->type);
		if (!first)
			goto moce_fail;

		factor->ratio_table = first->ratio_table;
	}

	factor->ratio = DEFAULT_RATIO;

	spin_lock_init(&factor->lock);

	return factor;

moce_fail:
	kfree(factor);

	return NULL;
}

static int __init dt_init_bias_idle(void)
{
	struct device_node *cpu_node;
	struct device_node *factor_node;
	struct bias_cpuidle *bias_idle;
	struct factor *factor;
	int cpu, i;

	/* init per-cpu variable */
	for_each_possible_cpu(cpu) {
		cpu_node = of_cpu_device_node_get(cpu);

		/* init per-cpu variable */
		bias_idle = &per_cpu(bias_cpuidle, cpu);

		INIT_LIST_HEAD(&bias_idle->factor_list);

		/* per-factor */
		for (i = 0; ; i++) {
			factor_node = of_parse_phandle(cpu_node, "moce-factors", i);
			if (!factor_node) {
				pr_debug("The number of moce factors for cpu%d is %d\n", cpu, i);
				break;
			}

			factor = init_factor_data(factor_node, cpu);
			if (!factor) {
				of_node_put(factor_node);
				of_node_put(cpu_node);
				pr_err("failed to initialize %dth moce factors for cpu%d\n", i + 1, cpu);
				return -1;
			}

			list_add_tail(&factor->list, &bias_idle->factor_list);
			of_node_put(factor_node);
		}

		if (!list_empty(&bias_idle->factor_list)) {
			bias_idle->total_ratio = DEFAULT_RATIO;
			bias_idle->biased = true;
		}

		of_node_put(cpu_node);
	}

	return 0;
}

static int __init exynos_moce_init(void)
{
	int ret;

	ret = dt_init_bias_idle();
	if (ret) {
		pr_err("failed to initialize moce driver\n");
		return -1;
	}

	ret = init_sysfs();
	if (ret) {
		pr_err("failed to create sysfs group\n");
		return ret;
	}

	cpufreq_register_notifier(&exynos_cpufreq_trans_nb,
			CPUFREQ_TRANSITION_NOTIFIER);

	pr_info("initialized exynos moce driver\n");

	return 0;
}
device_initcall(exynos_moce_init)
