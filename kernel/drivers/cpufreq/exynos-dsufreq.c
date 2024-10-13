/*
 * Copyright (c) 2023 samsung electronics co., ltd.
 *		http://www.samsung.com/
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

#include <trace/events/power.h>
#define CREATE_TRACE_POINTS
#include <trace/events/dsufreq.h>

#define DSUFREQ_RELATION_L	0
#define DSUFREQ_RELATION_H	1

struct dsufreq_qos_tracer {
	struct list_head		node;

	char				*label;

	struct dev_pm_qos_request	*req;
};

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

struct dsufreq {
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

	struct list_head		min_requests;
	struct list_head		max_requests;

	struct kobject			qos_kobj;

	spinlock_t			qos_lock;
} dsufreq;

/******************************************************************************
 *                               Helper Function                              *
 ******************************************************************************/
unsigned long *dsufreq_get_freq_table(int *table_size)
{
	*table_size = dsufreq.stats->table_size;
	return dsufreq.stats->freq_table;
}
EXPORT_SYMBOL_GPL(dsufreq_get_freq_table);

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

/* Find lowest freq at or above target in a table in ascending order */
static unsigned int dsufreq_table_find_index_al(unsigned int target_freq)
{
	unsigned long *freq_table = dsufreq.stats->freq_table;
	unsigned int best_freq = dsufreq.min_freq;
	int idx;

	for (idx = 0 ; idx < dsufreq.stats->table_size; idx++) {
		best_freq = freq_table[idx];

		if (target_freq <= freq_table[idx])
			break;
	}

	return best_freq;
}

/* Find highest freq at or below target in a table in ascending order */
static unsigned int dsufreq_table_find_index_ah(unsigned int target_freq)
{
	unsigned long *freq_table = dsufreq.stats->freq_table;
	unsigned int best_freq = dsufreq.min_freq;
	int idx;

	for (idx = 0; idx < dsufreq.stats->table_size; idx++) {
		if (target_freq < freq_table[idx])
			break;

		best_freq = freq_table[idx];
	}

	return best_freq;
}

static unsigned int resolve_dsufreq(unsigned int target_freq,
						unsigned int relation)
{
	unsigned int resolve_freq = 0;

	switch (relation) {
	case DSUFREQ_RELATION_L:
		resolve_freq = dsufreq_table_find_index_al(target_freq);
		break;
	case DSUFREQ_RELATION_H:
		resolve_freq = dsufreq_table_find_index_ah(target_freq);
		break;
	default:
		pr_err("%s: Invalid relation: %d\n", __func__, relation);
		break;
	}

	return resolve_freq;
}

/******************************************************************************
 *                           Transition Notifier                              *
 ******************************************************************************/
static RAW_NOTIFIER_HEAD(dsufreq_transition_notifier_list);

int dsufreq_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(
			&dsufreq_transition_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(dsufreq_register_notifier);

int dsufreq_unregister_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(
			&dsufreq_transition_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(dsufreq_unregister_notifier);

static void dsufreq_notify_transition(int freq)
{
	raw_notifier_call_chain(&dsufreq_transition_notifier_list, 0, &freq);
}

/******************************************************************************
 *                               Scaling Function                             *
 ******************************************************************************/
static int scale_dsufreq(unsigned int target_freq, unsigned int relation)
{
	int ret = 0, out_flag = DSS_FLAG_OUT;

	dbg_snapshot_freq(dsufreq.dss_type, dsufreq.cur_freq, target_freq, DSS_FLAG_IN);

#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	ret = cal_dfs_set_rate(dsufreq.cal_id, target_freq);
	if (ret < 0)
		out_flag = ret;
#endif

	dbg_snapshot_freq(dsufreq.dss_type, dsufreq.cur_freq, target_freq, out_flag);

	return ret;
}

#define KHz	1000
static int dm_scaler(int dm_type, void *devdata, unsigned int target_freq,
						unsigned int duration)
{
	int ret;
	unsigned int resolve_freq, cur_freq = dsufreq.cur_freq;

	resolve_freq = resolve_dsufreq(target_freq, 0);
	if (!resolve_freq) {
		pr_err("%s: failed to resolve target_freq %u\n",
					__func__, target_freq);
		return -EINVAL;
	}

	if (resolve_freq == dsufreq.cur_freq)
		return 0;

	ret = scale_dsufreq(resolve_freq, 0);
	if (ret) {
		pr_err("failed to scale frequency of DSU (%d -> %d)\n",
				dsufreq.cur_freq, resolve_freq);
		return ret;
	}

	dsufreq_notify_transition(resolve_freq);

	update_dsufreq_time_in_state(dsufreq.stats);
	update_dsufreq_stats(dsufreq.stats, resolve_freq);
	dsufreq.cur_freq = resolve_freq;

	trace_dsufreq_dm_scaler(target_freq, resolve_freq, cur_freq);
	trace_clock_set_rate("DSU", resolve_freq * KHz, raw_smp_processor_id());

	return ret;
}

/******************************************************************************
 *                          DSUFREQ DRIVER INTERFACE                          *
 ******************************************************************************/
unsigned int dsufreq_get_cur_freq(void)
{
	return dsufreq.cur_freq;
}
EXPORT_SYMBOL_GPL(dsufreq_get_cur_freq);

unsigned int dsufreq_get_max_freq(void)
{
	return dsufreq.max_freq;
}
EXPORT_SYMBOL_GPL(dsufreq_get_max_freq);

unsigned int dsufreq_get_min_freq(void)
{
	return dsufreq.min_freq;
}
EXPORT_SYMBOL_GPL(dsufreq_get_min_freq);

int exynos_dsufreq_target(unsigned int target_freq)
{
	int ret;
	unsigned long freq, prev_freq = dsufreq.cur_freq;

	target_freq = max(target_freq, dsufreq.min_freq);
	target_freq = min(target_freq, dsufreq.max_freq);

	if (target_freq == dsufreq.cur_freq)
		return 0;

	freq = (unsigned long)target_freq;

	ret = DM_CALL(dsufreq.dm_type, &freq);
	if (ret)
		pr_err("failed to scale frequency of DSU (%d -> %d)\n",	dsufreq.cur_freq, target_freq);

	trace_dsufreq_target(target_freq, prev_freq, dsufreq.cur_freq);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_dsufreq_target);

/******************************************************************************
 *                              DSUFreq dev PM QoS                            *
 ******************************************************************************/
static int dsufreq_pm_qos_notifier(struct notifier_block *nb,
				unsigned long freq, void *data)
{
	unsigned int min_freq, max_freq;

	min_freq = dev_pm_qos_read_value(dsufreq.dev, DEV_PM_QOS_MIN_FREQUENCY);
	max_freq = dev_pm_qos_read_value(dsufreq.dev, DEV_PM_QOS_MAX_FREQUENCY);

	min_freq = max(min_freq, dsufreq.min_freq_orig);
	min_freq = min(min_freq, dsufreq.max_freq_orig);
	dsufreq.min_freq = min_freq;

	max_freq = max(max_freq, dsufreq.min_freq_orig);
	max_freq = min(max_freq, dsufreq.max_freq_orig);
	dsufreq.max_freq = max_freq;

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
int dsufreq_qos_add_request(char *label, struct dev_pm_qos_request *req,
			   enum dev_pm_qos_req_type type, s32 value)
{
	int ret = 0;
	struct dsufreq_qos_tracer *qos_req;

	ret = dev_pm_qos_add_request(dsufreq.dev, req, type, value);
	if (ret < 0) {
		pr_err("%s: failed to add_request type %d value %d\n",
				__func__, type, value);
		return ret;
	}

	qos_req = kzalloc(sizeof(struct dsufreq_qos_tracer), GFP_KERNEL);
	if (!qos_req) {
		pr_err("%s: failed to alloc qos_tracer\n", __func__);
		return -ENOMEM;
	}

	qos_req->label = label;
	qos_req->req = req;
	INIT_LIST_HEAD(&qos_req->node);

	spin_lock(&dsufreq.qos_lock);

	if (type == DEV_PM_QOS_MIN_FREQUENCY)
		list_add(&qos_req->node, &dsufreq.min_requests);
	else
		list_add(&qos_req->node, &dsufreq.max_requests);

	spin_unlock(&dsufreq.qos_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(dsufreq_qos_add_request);

/******************************************************************************
 *                                 Sysfs function                             *
 ******************************************************************************/
static ssize_t dsufreq_show_min_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%u\n", dsufreq.min_freq);
}

static ssize_t dsufreq_store_min_freq(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%8u", &input) != 1)
		return -EINVAL;

	dev_pm_qos_update_request(&dsufreq.min_req, input);

	return count;
}

static ssize_t dsufreq_show_max_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%u\n", dsufreq.max_freq);
}

static ssize_t dsufreq_store_max_freq(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%8u", &input) != 1)
		return -EINVAL;

	dev_pm_qos_update_request(&dsufreq.max_req, input);

	return count;
}

static ssize_t dsufreq_show_cur_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%u\n", dsufreq.cur_freq);
}

static ssize_t dsufreq_show_time_in_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;
	struct dsufreq_stats *stats = dsufreq.stats;

	update_dsufreq_time_in_state(stats);

	for (i = 0; i < stats->table_size; i++) {
		count += snprintf(&buf[count], PAGE_SIZE - count, "%lu %llu\n",
			stats->freq_table[i],
			nsec_to_clock_t(stats->time_in_state[i]));
	}

	return count;
}

static ssize_t dsufreq_show_total_trans(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 30, "%llu\n", dsufreq.stats->total_trans);
}

static void dsufreq_show_qos_list(enum dev_pm_qos_req_type type, char *buf, ssize_t *count)
{
	struct dsufreq_qos_tracer *qos_req;
	struct list_head *qos_list;
	int req_cnt = 0;

	if (type == DEV_PM_QOS_MIN_FREQUENCY) {
		qos_list = &dsufreq.min_requests;
		*count += scnprintf(buf + *count, PAGE_SIZE - *count, "[Min Limit]\n");
	}
	else {
		qos_list = &dsufreq.max_requests;
		*count += scnprintf(buf + *count, PAGE_SIZE - *count, "[Max Limit]\n");
	}

	if (list_empty(qos_list))
		*count += scnprintf(buf + *count, PAGE_SIZE - *count, "QoS List is Empty!\n");
	else {
		list_for_each_entry(qos_req, qos_list, node) {
			*count += scnprintf(buf + *count, PAGE_SIZE - *count, "%d: %s(Request Freq=%u)\n",
					++req_cnt, qos_req->label, qos_req->req->data.freq.pnode.prio);
		}
	}

	if (type == DEV_PM_QOS_MIN_FREQUENCY)
		*count += scnprintf(buf + *count, PAGE_SIZE - *count, "Scaled Min Freq=%u\n\n",
				dsufreq.min_freq);
	else
		*count += scnprintf(buf + *count, PAGE_SIZE - *count, "Scaled Max Freq=%u\n\n",
				dsufreq.max_freq);
}

static ssize_t dsufreq_show_qos_tracer(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	spin_lock(&dsufreq.qos_lock);
	dsufreq_show_qos_list(DEV_PM_QOS_MAX_FREQUENCY, buf, &count);
	dsufreq_show_qos_list(DEV_PM_QOS_MIN_FREQUENCY, buf, &count);
	spin_unlock(&dsufreq.qos_lock);

	return count;
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
static DEVICE_ATTR(qos_tracer, S_IRUGO,
		dsufreq_show_qos_tracer, NULL);

static struct attribute *dsufreq_attrs[] = {
	&dev_attr_scaling_min_freq.attr,
	&dev_attr_scaling_max_freq.attr,
	&dev_attr_scaling_cur_freq.attr,
	&dev_attr_time_in_state.attr,
	&dev_attr_total_trans.attr,
	&dev_attr_qos_tracer.attr,
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

	if (of_property_read_u32(dn, "dm-type", &dsufreq.dm_type)) {
		pr_err("%s: dm-type does not exist in dt\n", __func__);
		return -EINVAL;
	}

	ret = exynos_dm_data_init(dsufreq.dm_type, &dsufreq,
				dsufreq.min_freq, dsufreq.max_freq,
				dsufreq.min_freq);
	if (ret) {
		pr_err("%s: failed to init dm_data\n", __func__);
		return ret;
	}

	dn = of_get_child_by_name(dn, "hw-constraints");
	if (dn) {
		ret = dsufreq_init_dm_constraint_table(dn);
		if (ret) {
			pr_err("%s: failed to init dm_constraint_table\n", __func__);
			return ret;
		}
	}

	return register_exynos_dm_freq_scaler(dsufreq.dm_type, dm_scaler);
}

#ifndef CONFIG_SCHED_EMS_DSU_FREQ_SELECT
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
#endif

#define MAX_TABLE_SIZE	50
#define FREQ_MERGE_THRESHOLD	48000
static void dsufreq_adjust_min_max_freq(unsigned long *freq_table, int *size)
{
	unsigned long temp[MAX_TABLE_SIZE] = {0, };
	int i, j;

	if (*size < 2)
		return;

	if (freq_table[1] - freq_table[0] < FREQ_MERGE_THRESHOLD) {
		freq_table[1] = freq_table[0];
		freq_table[0] = 0;
	}

	if (freq_table[*size - 1] - freq_table[*size - 2] < FREQ_MERGE_THRESHOLD) {
		freq_table[*size - 2] = freq_table[*size - 1];
		freq_table[*size - 1] = 0;
	}

	for (i = 0, j = 0; i < *size; i++) {
		if (!freq_table[i])
			continue;

		temp[j++] = freq_table[i];
	}

	memcpy(freq_table, temp, sizeof(unsigned long) * j);
	*size = j;
}

static void dsufreq_fill_freq_table(unsigned long *freq_table, int *size,
		unsigned int *raw_table, int raw_size)
{
	int i, j = 0;

	freq_table[j++] = dsufreq.min_freq;

	for (i = 0; i < raw_size; i++) {
		if (raw_table[i] <= dsufreq.min_freq)
			continue;
		else if (raw_table[i] >= dsufreq.max_freq)
			break;

		freq_table[j++] = raw_table[i];
	}

	freq_table[j++] = dsufreq.max_freq;
	*size = j;

	/*
	 * Adjust min/max freq in freq-table.
	 * If the gap of two frequencies is lower than threshold, merge the two frequencies.
	 */
	dsufreq_adjust_min_max_freq(freq_table, size);
}

static int dsufreq_parse_freq_table(struct device_node *dn, unsigned long *freq_table,
		int *size)
{
	unsigned int raw_table[MAX_TABLE_SIZE] = {0, };
	int raw_size;

	raw_size = of_property_count_u32_elems(dn, "freq-table");
	if (raw_size <= 0 || raw_size >= MAX_TABLE_SIZE) {
		pr_err("%s: out of table size\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32_array(dn, "freq-table", raw_table, raw_size)) {
		pr_err("%s: failed to get table\n", __func__);
		return -EINVAL;
	}

	/* Fill freq-table with freq defined in device tree and CAL */
	dsufreq_fill_freq_table(freq_table, size, raw_table, raw_size);

#ifndef CONFIG_SCHED_EMS_DSU_FREQ_SELECT
	apply_energy_table(freq_table, *size);
#endif

	return 0;
}

static int dsufreq_init_stats(struct device_node *dn)
{
	struct dsufreq_stats *stats;
	unsigned long freq_table[MAX_TABLE_SIZE] = {0, };
	int size = 0;

	stats = kzalloc(sizeof(struct dsufreq_stats), GFP_KERNEL);
	if (!stats) {
		pr_err("%s: failed to alloc stats\n", __func__);
		return -ENOMEM;
	}

	if (dsufreq_parse_freq_table(dn, freq_table, &size)) {
		kfree(stats);
		pr_err("%s: failed to parse freq-table from dt\n", __func__);
		return -EINVAL;
	}

	stats->time_in_state = kcalloc(size, sizeof(unsigned long long), GFP_KERNEL);
	if (!stats->time_in_state) {
		kfree(stats);
		pr_err("%s: failed to alloc time in state\n", __func__);
		return -ENOMEM;
	}

	stats->freq_table = kcalloc(size, sizeof(unsigned long), GFP_KERNEL);
	if (!stats->freq_table) {
		kfree(stats->time_in_state);
		kfree(stats);
		pr_err("%s: failed to alloc freq table\n", __func__);
		return -ENOMEM;
	}

	memcpy(stats->freq_table, freq_table, sizeof(unsigned long) * size);
	stats->table_size = size;

	dsufreq.stats = stats;

	/* cur_freq can be out of stat table, resolve it */
	dsufreq.cur_freq = resolve_dsufreq(dsufreq.cur_freq,
					DSUFREQ_RELATION_L);

	stats->last_time = local_clock();
	stats->last_index = get_dsufreq_index(stats, dsufreq.cur_freq);

	return 0;
}

static int dsufreq_init_qos(void)
{
	int ret = 0;

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

	dsufreq.nb_min.notifier_call = dsufreq_pm_qos_notifier;
	dsufreq.nb_max.notifier_call = dsufreq_pm_qos_notifier;

	INIT_LIST_HEAD(&dsufreq.min_requests);
	INIT_LIST_HEAD(&dsufreq.max_requests);

	spin_lock_init(&dsufreq.qos_lock);

	dsufreq_qos_add_request("sysfs", &dsufreq.min_req, DEV_PM_QOS_MIN_FREQUENCY,
							dsufreq.min_freq);
	dsufreq_qos_add_request("sysfs", &dsufreq.max_req, DEV_PM_QOS_MAX_FREQUENCY,
							dsufreq.max_freq);

	return ret;
}

static int dsufreq_init(struct device_node *dn)
{
	int ret;
	unsigned int min_freq, max_freq, val;

	if (of_property_read_u32(dn, "cal-id", &dsufreq.cal_id))
		return -EINVAL;

	if (of_property_read_u32(dn, "dss-type", &dsufreq.dss_type))
		return -EINVAL;

	/* Get min/max/cur DSUFreq */
	min_freq = cal_dfs_get_min_freq(dsufreq.cal_id);
	max_freq = cal_dfs_get_max_freq(dsufreq.cal_id);

	if (!of_property_read_u32(dn, "min-freq", &val))
		min_freq = max(min_freq, val);
	if (!of_property_read_u32(dn, "max-freq", &val))
		max_freq = min(max_freq, val);

	dsufreq.min_freq = dsufreq.min_freq_orig = min_freq;
	dsufreq.max_freq = dsufreq.max_freq_orig = max_freq;

	dsufreq.cur_freq = cal_dfs_get_boot_freq(dsufreq.cal_id);

	/* initialize stats */
	ret = dsufreq_init_stats(dn);
	if (ret)
		return ret;

	/* initialize DM */
	ret = dsufreq_init_dm(dn);
	if (ret)
		return ret;

	/* Initiallize dev PM QoS */
	ret = dsufreq_init_qos();
	if (ret)
		return ret;

	return ret;
}

static int exynos_dsufreq_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	int ret = 0, i;

	dsufreq.dev = &pdev->dev;

	ret = dsufreq_init(dn);
	if (ret)
		goto fail;

	ret = sysfs_create_group(&pdev->dev.kobj, &dsufreq_group);
	if (ret)
		goto fail;

	pr_info("available dsu frequencies\n");
	for (i = 0; i < dsufreq.stats->table_size; i++)
		pr_info("%lu\n", dsufreq.stats->freq_table[i]);

	return 0;

fail:
	if (dsufreq.stats && dsufreq.stats->time_in_state)
		kfree(dsufreq.stats->time_in_state);
	if (dsufreq.stats && dsufreq.stats->freq_table)
		kfree(dsufreq.stats->freq_table);
	if (dsufreq.stats)
		kfree(dsufreq.stats);

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

#ifdef CONFIG_SCHED_EMS_DSU_FREQ_SELECT
MODULE_SOFTDEP("post: ems");
#else
MODULE_SOFTDEP("pre: exynos-cpufreq");
#endif
MODULE_DESCRIPTION("Exynos DSUFreq drvier");
MODULE_LICENSE("GPL");
