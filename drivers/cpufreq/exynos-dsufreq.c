/*
 * Copyright (c) 2019 samsung electronics co., ltd.
 *		http://www.samsung.com/
 *
 * Author : Choonghoon Park (choong.park@samsung.com)
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
 *
 * Exynos DSUFreq driver implementation
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/pm_opp.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sort.h>
#include <uapi/linux/sched/types.h>

#include <linux/ems.h>

#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/exynos-dm.h>

#define DSUFREQ_ENTRY_INVALID   (~0u)
#define DSUFREQ_RELATION_L	0
#define DSUFREQ_RELATION_H	1

struct exynos_dsufreq_dm {
	struct list_head		list;
	struct exynos_dm_constraint	c;
};

struct dsufreq_stats {
	unsigned int			table_size;
	unsigned int			last_index;
	unsigned long long		last_time;
	unsigned long long		total_trans;

	unsigned long			*freq_table;
	unsigned long long		*time_in_state;
};

struct {
	struct device			*dev;

	unsigned int			min_freq;
	unsigned int			max_freq;
	unsigned int			cur_freq;
	unsigned int			min_freq_orig;
	unsigned int			max_freq_orig;

	unsigned int			state;
	unsigned int			cal_id;
	unsigned int			dm_type;
	unsigned int			dss_type;

	struct dsufreq_stats		*stats;

	/* list head of DVFS Manager constraints */
	struct list_head		dm_list;

	/* dev PM QoS notifier block */
	struct notifier_block		nb_min;
	struct notifier_block		nb_max;

	struct dev_pm_qos_request	min_req;
	struct dev_pm_qos_request	max_req;
} dsufreq;

/******************************************************************************
 *                               Helper Function                              *
 ******************************************************************************/
static int get_dsufreq_index(struct dsufreq_stats *stats, unsigned int freq)
{
	int index;

	for (index = 0; index < stats->table_size; index++)
		if (stats->freq_table[index] == freq)
			return index;

	return -1;
}

static void update_dsufreq_time_in_state(struct dsufreq_stats *stats)
{
	unsigned long long cur_time = local_clock();

	stats->time_in_state[stats->last_index] +=
				cur_time - stats->last_time;
	stats->last_time = cur_time;
}

static void update_dsufreq_stats(struct dsufreq_stats *stats,
						unsigned int freq)
{
	stats->last_index = get_dsufreq_index(stats, freq);
	stats->total_trans++;
}

static inline void dsufreq_verify_within_limits(unsigned int min,
						unsigned int max)
{
	if (dsufreq.min_freq < min)
		dsufreq.min_freq = min;
	if (dsufreq.max_freq < min)
		dsufreq.max_freq = min;
	if (dsufreq.min_freq > max)
		dsufreq.min_freq = max;
	if (dsufreq.max_freq > max)
		dsufreq.max_freq = max;
	if (dsufreq.min_freq > dsufreq.max_freq)
		dsufreq.min_freq = dsufreq.max_freq;
	return;
}

/* Find lowest freq at or above target in a table in ascending order */
static int dsufreq_table_find_index_al(unsigned int target_freq)
{
	unsigned long *freq_table = dsufreq.stats->freq_table;
	unsigned int best_freq = 0;
	int idx;

	for (idx = 0 ; idx < dsufreq.stats->table_size; idx++) {
		if (freq_table[idx] == DSUFREQ_ENTRY_INVALID)
			continue;

		if (freq_table[idx] >= target_freq)
			return freq_table[idx];

		best_freq = freq_table[idx];
	}

	return best_freq;
}

/* Find highest freq at or below target in a table in ascending order */
static int dsufreq_table_find_index_ah(unsigned int target_freq)
{
	unsigned long *freq_table = dsufreq.stats->freq_table;
	unsigned int best_freq = 0;
	int idx;

	for (idx = 0; idx < dsufreq.stats->table_size; idx++) {
		if (freq_table[idx] == DSUFREQ_ENTRY_INVALID)
			continue;

		if (freq_table[idx] <= target_freq) {
			best_freq = freq_table[idx];
			continue;
		}

		return best_freq;
	}

	return best_freq;
}

static unsigned int resolve_dsufreq(unsigned int target_freq,
						unsigned int relation)
{
	unsigned int resolve_freq;

	switch (relation) {
	case DSUFREQ_RELATION_L:
		resolve_freq = dsufreq_table_find_index_al(target_freq);
		break;
	case DSUFREQ_RELATION_H:
		resolve_freq = dsufreq_table_find_index_ah(target_freq);
		break;
	default:
		pr_err("%s: Invalid relation: %d\n", __func__, relation);
		return -EINVAL;
	}

	return resolve_freq;
}

/******************************************************************************
 *                               Scaling Function                             *
 ******************************************************************************/
static int scale_dsufreq(unsigned int target_freq, unsigned int relation)
{
	int ret = 0;

	dbg_snapshot_freq(dsufreq.dss_type, dsufreq.cur_freq, target_freq,
							DSS_FLAG_IN);

#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	ret = cal_dfs_set_rate(dsufreq.cal_id, target_freq);
#endif

	dbg_snapshot_freq(dsufreq.dss_type, dsufreq.cur_freq, target_freq,
				  ret < 0 ? ret : DSS_FLAG_OUT);
	return ret;
}

static int dm_scaler(int dm_type, void *devdata, unsigned int target_freq,
						unsigned int relation)
{
	int ret;
	unsigned int resolve_freq;

	resolve_freq = resolve_dsufreq(target_freq, relation);
	if (resolve_freq < 0) {
		pr_err("%s: freq out of range, target_freq %u\n",
					__func__, target_freq);
		return -EINVAL;
	}

	if (resolve_freq == dsufreq.cur_freq)
		return 0;

	ret = scale_dsufreq(resolve_freq, relation);
	if (ret) {
		pr_err("failed to scale frequency of DSU (%d -> %d)\n",
				dsufreq.cur_freq, resolve_freq);
		return ret;
	}

	update_dsufreq_time_in_state(dsufreq.stats);
	update_dsufreq_stats(dsufreq.stats, resolve_freq);
	dsufreq.cur_freq = resolve_freq;

	return ret;
}

/******************************************************************************
 *                              DSUFreq dev PM QoS                            *
 ******************************************************************************/
static int dsufreq_pm_qos_notifier(struct notifier_block *nb,
				unsigned long freq, void *data)
{
	dsufreq.min_freq = dev_pm_qos_read_value(dsufreq.dev,
					DEV_PM_QOS_MIN_FREQUENCY);
	dsufreq.max_freq = dev_pm_qos_read_value(dsufreq.dev,
					DEV_PM_QOS_MAX_FREQUENCY);

	dsufreq_verify_within_limits(dsufreq.min_freq_orig,
				dsufreq.max_freq_orig);

	policy_update_call_to_DM(dsufreq.dm_type, dsufreq.min_freq,
						dsufreq.max_freq);

	/*
	 * DM_CALL() should be called to change the frequency immediately,
	 * but it is not allowed in this driver. This driver never calls
	 * DM_CALL() because it does not have own frequency selection logic.
	 * DM does not allow to change frequency smaller than frequency
	 * requested by DM_CALL(). If DM_CALL() is called by PM QoS request,
	 * frequency can not be smaller than requested QoS until next request.
	 */
	return 0;
}

/*
 * dsufreq manages min/max PM QoS request through dev PM QoS, device structure
 * of dsufreq is needed when new request is added. When updating or removing
 * QoS, only the request variable is required. So, requester should call
 * dsufreq_qos_add_request() to add request, otherwise, call
 * dev_pm_qos_update_reqeust() and dev_pm_qos_remove_request() provided by
 * dev_pm_qos framework.
 */
int dsufreq_qos_add_request(struct dev_pm_qos_request *req,
			   enum dev_pm_qos_req_type type, s32 value)
{
	return dev_pm_qos_add_request(dsufreq.dev, req, type, value);
}
EXPORT_SYMBOL_GPL(dsufreq_qos_add_request);

/******************************************************************************
 *                                 Sysfs function                             *
 ******************************************************************************/
static ssize_t dsufreq_show_min_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", dsufreq.min_req.data.freq.pnode.prio);
}

static ssize_t dsufreq_store_min_freq(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%8d", &input) != 1)
		return -EINVAL;

	dev_pm_qos_update_request(&dsufreq.min_req, input);

	return count;
}

static ssize_t dsufreq_show_max_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", dsufreq.max_req.data.freq.pnode.prio);
}

static ssize_t dsufreq_store_max_freq(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%8d", &input) != 1)
		return -EINVAL;

	dev_pm_qos_update_request(&dsufreq.max_req, input);

	return count;
}

static ssize_t dsufreq_show_cur_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", dsufreq.cur_freq);
}

static ssize_t dsufreq_show_time_in_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;
	struct dsufreq_stats *stats = dsufreq.stats;

	update_dsufreq_time_in_state(stats);

	for (i = 0; i < stats->table_size; i++) {
		if (stats->freq_table[i] == DSUFREQ_ENTRY_INVALID)
			continue;

		count += snprintf(&buf[count], PAGE_SIZE - count, "%u %llu\n",
			stats->freq_table[i],
			nsec_to_clock_t(stats->time_in_state[i]));
	}

	return count;
}

static ssize_t dsufreq_show_total_trans(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%d\n", dsufreq.stats->total_trans);
}

static DEVICE_ATTR(scaling_min_freq, S_IRUGO | S_IWUSR,
		dsufreq_show_min_freq, dsufreq_store_min_freq);
static DEVICE_ATTR(scaling_max_freq, S_IRUGO | S_IWUSR,
		dsufreq_show_max_freq, dsufreq_store_max_freq);
static DEVICE_ATTR(scaling_cur_freq, S_IRUGO,
		dsufreq_show_cur_freq, NULL);
static DEVICE_ATTR(time_in_state, S_IRUGO,
		dsufreq_show_time_in_state, NULL);
static DEVICE_ATTR(total_trans, S_IRUGO,
		dsufreq_show_total_trans, NULL);

static struct attribute *dsufreq_attrs[] = {
	&dev_attr_scaling_min_freq.attr,
	&dev_attr_scaling_max_freq.attr,
	&dev_attr_scaling_cur_freq.attr,
	&dev_attr_time_in_state.attr,
	&dev_attr_total_trans.attr,
	NULL,
};

static struct attribute_group dsufreq_group = {
	.name = "dsufreq",
	.attrs = dsufreq_attrs,
};

/******************************************************************************
 *                                Initialization                              *
 ******************************************************************************/
static struct ect_minlock_domain *get_ect_domain(const char *ect_name)
{
	struct ect_minlock_domain *ect_domain = NULL;
	void *block;

	block = ect_get_block(BLOCK_MINLOCK);
	if (!block)
		return NULL;

	ect_domain = ect_minlock_get_domain(block, (char *)ect_name);
	if (!ect_domain)
		return NULL;

	return ect_domain;
}

static int dsufreq_init_dm_constraint_table(struct device_node *root)
{
	struct exynos_dsufreq_dm *dm;
	struct of_phandle_iterator iter;
	struct ect_minlock_domain *ect_domain;
	int ret = 0, err, i;

	INIT_LIST_HEAD(&dsufreq.dm_list);

	of_for_each_phandle(&iter, err, root, "list", NULL, 0) {
		const char *ect_name;

		/* allocate DM constraint */
		dm = kzalloc(sizeof(struct exynos_dsufreq_dm), GFP_KERNEL);
		if (!dm)
			goto init_fail;

		list_add_tail(&dm->list, &dsufreq.dm_list);

		/* guidance : constraint by H/W characteristic or not */
		if (of_property_read_bool(iter.node, "guidance"))
			dm->c.guidance = true;
		else {
			/* this driver only supports constarint by H/W */
			goto init_fail;
		}

		/* const-type : min or max constarint */
		if (of_property_read_u32(iter.node, "const-type",
						&dm->c.constraint_type))
			goto init_fail;

		/* dm-slave : target constraint domain */
		if (of_property_read_u32(iter.node, "dm-slave",
						&dm->c.dm_slave))
			goto init_fail;

		if (of_property_read_string(iter.node, "ect-name", &ect_name))
			goto init_fail;

		ect_domain = get_ect_domain(ect_name);
		if (!ect_domain)
			goto init_fail;

		dm->c.table_length = ect_domain->num_of_level;
		dm->c.freq_table = kcalloc(dm->c.table_length,
				sizeof(struct exynos_dm_freq), GFP_KERNEL);
		if (!dm->c.freq_table)
			goto init_fail;

		/* fill DM constarint table */
		for (i = 0; i < ect_domain->num_of_level; i++) {
			dm->c.freq_table[i].master_freq
				= ect_domain->level[i].main_frequencies;
			dm->c.freq_table[i].slave_freq
				= ect_domain->level[i].sub_frequencies;
		}

		/* register DM constraint */
		ret = register_exynos_dm_constraint_table(dsufreq.dm_type,
								&dm->c);
		if (ret)
			goto init_fail;
	}

	return ret;

init_fail:
	while (!list_empty(&dsufreq.dm_list)) {
		dm = list_last_entry(&dsufreq.dm_list,
				struct exynos_dsufreq_dm, list);
		list_del(&dm->list);
		kfree(dm->c.freq_table);
		kfree(dm);
	}

	return ret;
}

static int dsufreq_init_dm(struct device_node *dn)
{
	int ret;

	ret = of_property_read_u32(dn, "dm-type", &dsufreq.dm_type);
	if (ret)
		return ret;

	ret = exynos_dm_data_init(dsufreq.dm_type, &dsufreq,
				dsufreq.min_freq, dsufreq.max_freq,
				dsufreq.min_freq);
	if (ret)
		return ret;

	dn = of_get_child_by_name(dn, "hw-constraints");
	if (dn) {
		ret = dsufreq_init_dm_constraint_table(dn);
		if (ret)
			return ret;
	}

	return register_exynos_dm_freq_scaler(dsufreq.dm_type, dm_scaler);
}

static void apply_energy_table(unsigned long *freq_table, int size)
{
	struct fv_table {
		unsigned int freq;
		unsigned int volt;
	} *fv_table;
	unsigned int *volt_table;
	int i;

	fv_table = kcalloc(size, sizeof(struct fv_table), GFP_KERNEL);
	volt_table = kcalloc(size, sizeof(unsigned int), GFP_KERNEL);

	for (i = 0; i < size; i++)
		fv_table[i].freq = freq_table[i];

	cal_dfs_get_freq_volt_table(dsufreq.cal_id, fv_table, size);

	for (i = 0; i < size; i++)
		volt_table[i] = fv_table[i].volt;

	et_init_dsu_table(freq_table, volt_table, size);

	kfree(fv_table);
	kfree(volt_table);
}

static int __dsufreq_init_stat_table(struct dsufreq_stats *stats,
				unsigned long *freq_table, int raw_size)
{
	int i, k, size = 0;

	for (i = 0; i < raw_size; i++) {
		if (freq_table[i] > dsufreq.max_freq) {
			freq_table[i] = DSUFREQ_ENTRY_INVALID;
			continue;
		}

		if (freq_table[i] < dsufreq.min_freq) {
			freq_table[i] = DSUFREQ_ENTRY_INVALID;
			continue;
		}

		size++;
	}

	stats->freq_table = kcalloc(size, sizeof(unsigned long), GFP_KERNEL);
	if (!stats->freq_table) {
		kfree(freq_table);
		return -ENOMEM;
	}

	k = 0;
	for (i = 0; i < raw_size; i++) {
		if (freq_table[i] == DSUFREQ_ENTRY_INVALID)
			continue;

		stats->freq_table[k++] = freq_table[i];
	}

	stats->table_size = size;

	apply_energy_table(stats->freq_table, stats->table_size);

	return 0;
}

#define MAX_TABLE_SIZE	100

/* table_b merges into table_a and return size of merged table */
static int merge_table(unsigned int *table_a, unsigned int *table_b, int size)
{
	unsigned int temp[MAX_TABLE_SIZE] = {0, };
	int i, j, k;

	/* if table_a is empty, copy table */
	if (!table_a[0]) {
		memcpy(table_a, table_b, sizeof(unsigned int ) * size);
		return size;
	}

	/*
	 * Merge both table. Table is in ascending order.
	 * i : index of table_a
	 * j : index of table_b
	 * k : index of merged table
	 */
	i = j = k = 0;
	while (table_a[i] || table_b[j]) {
		unsigned int freq;

		if (!table_a[i])
			freq = table_b[j++];
		else if (!table_b[j])
			freq = table_a[i++];
		else if (table_a[i] < table_b[j])
			freq = table_a[i++];
		else if (table_a[i] > table_b[j])
			freq = table_b[j++];
		else {
			freq = table_a[i];
			i++, j++;
		}

		if (k > 0 && temp[k - 1] == freq)
			continue;

		temp[k++] = freq;
	};

	memcpy(table_a, temp, sizeof(unsigned int ) * k);

	return k;
}

static int dsufreq_init_stat_table(struct device_node *dn,
					struct dsufreq_stats *stats)
{
	unsigned long *freq_table;
	unsigned int merged_table[MAX_TABLE_SIZE] = {0, };
	int size, i;

        while ((dn = of_find_node_by_type(dn, "cpudsu-table"))) {
		struct device_node *const_dn, *child;

		const_dn = of_parse_phandle(dn, "constraint", 0);
		if (!const_dn)
			continue;

		for_each_child_of_node(const_dn, child) {
			int raw_size;
			unsigned int raw_table[MAX_TABLE_SIZE];
			unsigned int dsu_table[MAX_TABLE_SIZE];

			raw_size = of_property_count_u32_elems(child, "table");
			if (raw_size <= 0 || raw_size >= MAX_TABLE_SIZE) {
				pr_err("%s: out of table size\n", __func__);
				continue;
			}

			/*
			 * read CPU-DSU frequency mapping table
			 * from device tree.
			 */
			if (of_property_read_u32_array(child, "table",
						raw_table, raw_size)) {
				pr_err("%s: failed to get table\n", __func__);
				continue;
			}

			/*
			 * get DSU frequency from table.
			 * DSU frequencies are in odd indices.
			 */
			for (i = 1; i < raw_size; i += 2)
				dsu_table[(i - 1) / 2] = raw_table[i];

			size = merge_table(merged_table,
					dsu_table, raw_size / 2);
		}
	}

	freq_table = kcalloc(size, sizeof(unsigned long), GFP_KERNEL);
	if (!freq_table) {
		kfree(freq_table);
		return -ENOMEM;
	}

	/* Copy 'int' type table to 'long' type table */
	for (i = 0; i < size; i++) {
		freq_table[i] = merged_table[i];
	}

	return __dsufreq_init_stat_table(stats, freq_table, size);
}

static int dsufreq_init_stats(struct device_node *dn)
{
	struct dsufreq_stats *stats;
	int ret = 0;

	stats = kzalloc(sizeof(struct dsufreq_stats), GFP_KERNEL);
	if (!stats)
		return -ENOMEM;

	ret = dsufreq_init_stat_table(dn, stats);
	if (ret < 0) {
		kfree(stats);
		return ret;
	}

	/* Initialize DSUFreq time_in_state */
	stats->time_in_state = kcalloc(stats->table_size,
				sizeof(unsigned long long), GFP_KERNEL);
	if (!stats->time_in_state) {
		kfree(stats);
		return -ENOMEM;
	}

	dsufreq.stats = stats;

	/* cur_freq can be out of stat table, resolve it */
	dsufreq.cur_freq = resolve_dsufreq(dsufreq.cur_freq,
					DSUFREQ_RELATION_L);

	stats->last_time = local_clock();
	stats->last_index = get_dsufreq_index(stats, dsufreq.cur_freq);

	return 0;
}

static int dsufreq_init(struct device_node *dn)
{
	int ret;
	unsigned int val;

	ret = of_property_read_u32(dn, "cal-id", &dsufreq.cal_id);
	if (ret)
		return ret;

	/* Get min/max/cur DSUFreq */
	dsufreq.min_freq = cal_dfs_get_min_freq(dsufreq.cal_id);
	dsufreq.max_freq = cal_dfs_get_max_freq(dsufreq.cal_id);

	if (!of_property_read_u32(dn, "min-freq", &val))
		dsufreq.min_freq_orig = max(dsufreq.min_freq, val);
	if (!of_property_read_u32(dn, "max-freq", &val))
		dsufreq.max_freq_orig = min(dsufreq.max_freq, val);

	dsufreq.min_freq_orig = dsufreq.min_freq;
	dsufreq.max_freq_orig = dsufreq.max_freq;

	dsufreq.cur_freq = cal_dfs_get_boot_freq(dsufreq.cal_id);

	ret = of_property_read_u32(dn, "dss-type", &val);
	if (ret)
		return ret;
	dsufreq.dss_type = val;

	/* Initiallize dev PM QoS */
	dsufreq.nb_min.notifier_call = dsufreq_pm_qos_notifier;
	dsufreq.nb_max.notifier_call = dsufreq_pm_qos_notifier;

	ret = dev_pm_qos_add_notifier(dsufreq.dev, &dsufreq.nb_min,
					DEV_PM_QOS_MIN_FREQUENCY);
	if (ret) {
		pr_err("Failed to register MIN QoS notifier\n");
		return ret;
	}

	ret = dev_pm_qos_add_notifier(dsufreq.dev, &dsufreq.nb_max,
					DEV_PM_QOS_MAX_FREQUENCY);
	if (ret) {
		pr_err("Failed to register MAX QoS notifier\n");
		return ret;
	}

	dsufreq_qos_add_request(&dsufreq.min_req, DEV_PM_QOS_MIN_FREQUENCY,
							dsufreq.min_freq);
	dsufreq_qos_add_request(&dsufreq.max_req, DEV_PM_QOS_MAX_FREQUENCY,
							dsufreq.max_freq);

	/* initialize stats */
	ret = dsufreq_init_stats(dn);
	if (ret)
		return ret;

	/* initialize DM */
	ret = dsufreq_init_dm(dn);
	if (ret)
		return ret;

	return ret;
}

static int exynos_dsufreq_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	int ret = 0;

	dsufreq.dev = &pdev->dev;

	ret = dsufreq_init(dn);
	if (ret)
		return ret;

	ret = sysfs_create_group(&pdev->dev.kobj, &dsufreq_group);
	if (ret)
		return ret;

	return ret;
}

static const struct of_device_id of_exynos_dsufreq_match[] = {
	{ .compatible = "samsung,exynos-dsufreq", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_dsufreq_match);

static struct platform_driver exynos_dsufreq_driver = {
	.driver = {
		.name	= "exynos-dsufreq",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_dsufreq_match,
	},
	.probe		= exynos_dsufreq_probe,
};

module_platform_driver(exynos_dsufreq_driver);

MODULE_SOFTDEP("pre: exynos-acme");
MODULE_DESCRIPTION("Exynos DSUFreq drvier");
MODULE_LICENSE("GPL");
