/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * User Frequency & Cstate Control driver
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd
 * Author : Park Bumgyu <bumgyu.park@samsung.com>
 * Author : LEE DAEYEONG <daeyeong.lee@samsung.com>
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/cpumask.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/pm_opp.h>
#include <linux/ems.h>

#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos-ufcc.h>
#include <soc/samsung/freq-qos-tracer.h>

/*********************************************************************/
/*  UCC feature - User C-state Control                               */
/*********************************************************************/
static struct _ucc {
	bool			enable;

	int			cur_level;
	int			cur_value;

	int			num_of_config;
	/*
	 * config : indicate whether each cpu supports c-state
	 *   - mask bit set : c-state allowed
	 *   - mask bit unset : c-state disallowed
	 */
	struct cpumask		*config;

	struct list_head	req_list;

	spinlock_t		lock;
} ucc = {
	.cur_value = -1,
	.lock = __SPIN_LOCK_INITIALIZER(ucc.lock),
};

enum ucc_req_type {
	UCC_REMOVE_REQ,
	UCC_UPDATE_REQ,
	UCC_ADD_REQ,
};

enum {
	CPD_BLOCK,
	C2_BLOCK,
	END_OF_UCC_LEVEL
};

#define is_invalid_ucc_level(level)	(level < 0 || level >= END_OF_UCC_LEVEL)
static void list_add_priority(struct ucc_req *new, struct list_head *head)
{
	struct list_head temp;
	struct ucc_req *pivot;

	/*
	 * If new request's value is bigger than first entry,
	 * add new request to first entry.
	 */
	pivot = list_first_entry(head, struct ucc_req, list);
	if (pivot->value <= new->value) {
		list_add(&new->list, head);
		return;
	}

	/*
	 * If new request's value is smaller than last entry,
	 * add new request to last entry.
	 */
	pivot = list_last_entry(head, struct ucc_req, list);
	if (pivot->value >= new->value) {
		list_add_tail(&new->list, head);
		return;
	}

	/*
	 * Find first entry that smaller than new request.
	 * And add new request before that entry.
	 */
	list_for_each_entry(pivot, head, list) {
		if (pivot->value < new->value)
			break;
	}

	list_cut_before(&temp, head, &pivot->list);
	list_add(&new->list, head);
	list_splice(&temp, head);
}

static inline int get_ucc_req_value(void)
{
	struct ucc_req *req;

	req = list_first_entry_or_null(&ucc.req_list, struct ucc_req, list);

	return req ? req->value : -1;
}

static inline void update_ucc_allowed_mask(void)
{
	update_pm_allowed_mask(cpu_possible_mask);
	update_idle_allowed_mask(cpu_possible_mask);

	if (ucc.cur_value < 0)
		return;

	if (ucc.cur_level)
		update_idle_allowed_mask(&ucc.config[ucc.cur_value]);
	else
		update_pm_allowed_mask(&ucc.config[ucc.cur_value]);
}

static void update_ucc_level(int level)
{
	if (ucc.cur_level == level)
		return;

	ucc.cur_level = level;
	update_ucc_allowed_mask();
}

static void update_ucc_req_value(int value)
{
	if (ucc.cur_value == value)
		return;

	ucc.cur_value = value;
	update_ucc_allowed_mask();
}

static void ucc_update(struct ucc_req *req, int value, enum ucc_req_type type)
{
	if (value < 0 || value >= ucc.num_of_config)
		return;

	switch (type) {
		case UCC_REMOVE_REQ:
			list_del(&req->list);
			req->active = false;
			break;
		case UCC_UPDATE_REQ:
			list_del(&req->list);
		case UCC_ADD_REQ:
			req->value = value;
			list_add_priority(req, &ucc.req_list);
			req->active = true;
			break;
	}

	update_ucc_req_value(get_ucc_req_value());
}

static struct ucc_req ufcc_req = { .name = "ufcc", };
static void update_ufcc_request(int value)
{
	ufcc_req.value = value;
	ucc_update_request(&ufcc_req, value);
}

/*********************************************************************/
/*  External APIs - User C-state Control                             */
/*********************************************************************/
void ucc_add_request(struct ucc_req *req, int value)
{
	unsigned long flags;

	if (!likely(ucc.enable))
		return;

	spin_lock_irqsave(&ucc.lock, flags);

	if (req->active) {
		spin_unlock_irqrestore(&ucc.lock, flags);
		return;
	}

	ucc_update(req, value, UCC_ADD_REQ);

	spin_unlock_irqrestore(&ucc.lock, flags);
}
EXPORT_SYMBOL_GPL(ucc_add_request);

void ucc_update_request(struct ucc_req *req, int value)
{
	unsigned long flags;

	if (!likely(ucc.enable))
		return;

	spin_lock_irqsave(&ucc.lock, flags);

	if (!req->active) {
		spin_unlock_irqrestore(&ucc.lock, flags);
		return;
	}

	ucc_update(req, value, UCC_UPDATE_REQ);

	spin_unlock_irqrestore(&ucc.lock, flags);
}
EXPORT_SYMBOL_GPL(ucc_update_request);

void ucc_remove_request(struct ucc_req *req)
{
	unsigned long flags;

	if (!likely(ucc.enable))
		return;

	spin_lock_irqsave(&ucc.lock, flags);

	if (!req->active) {
		spin_unlock_irqrestore(&ucc.lock, flags);
		return;
	}

	ucc_update(req, 0, UCC_REMOVE_REQ);

	spin_unlock_irqrestore(&ucc.lock, flags);
}
EXPORT_SYMBOL_GPL(ucc_remove_request);

/*********************************************************************/
/*  Sysfs funcions - User C-state Control                            */
/*********************************************************************/
static ssize_t ucc_requests_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ucc_req *req;
	int ret = 0;

	list_for_each_entry(req, &ucc.req_list, list)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"request : %d (%s)\n", req->value, req->name);

	return ret;
}
static DEVICE_ATTR_RO(ucc_requests);

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cpumask mask;
	int ret = 0;

	if (ucc.cur_value < 0)
		cpumask_copy(&mask, cpu_possible_mask);
	else
		cpumask_copy(&mask, &ucc.config[ucc.cur_value]);

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "current level=%d (%s)\n",
			ucc.cur_level, ucc.cur_level ? "c2" : "cpd");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "current req value=%d (allowed=0x%x)\n",
			ucc.cur_value, *(unsigned int *)cpumask_bits(&mask));

	return ret;
}
static DEVICE_ATTR_RO(status);

static bool ucc_requested;
static ssize_t cstate_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", ucc_requested);
}
static ssize_t cstate_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input < 0)
		return -EINVAL;

	input = !!input;

	if (input != ucc_requested) {
		ucc_requested = input;

		if (ucc_requested)
			ucc_add_request(&ufcc_req, ufcc_req.value);
		else
			ucc_remove_request(&ufcc_req);
	}

	return count;
}
static DEVICE_ATTR_RW(cstate_control);

static ssize_t cstate_control_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d\n", ucc.cur_level);
}
static ssize_t cstate_control_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	if (is_invalid_ucc_level(input))
		return -EINVAL;

	update_ucc_level(input);

	return count;
}
static DEVICE_ATTR_RW(cstate_control_level);

static struct attribute *ucc_attrs[] = {
	&dev_attr_ucc_requests.attr,
	&dev_attr_status.attr,
	&dev_attr_cstate_control.attr,
	&dev_attr_cstate_control_level.attr,
	NULL,
};

static struct attribute_group ucc_attr_group = {
	.name = "ucc",
	.attrs = ucc_attrs,
};

/*********************************************************************/
/*  Initialization - User C-state Control                            */
/*********************************************************************/
static int exynos_ucc_init(struct platform_device *pdev)
{
	struct device_node *dn, *child;
	int ret;

	dn = of_find_node_by_name(pdev->dev.of_node, "ucc");

	ucc.num_of_config = of_get_child_count(dn);
	if (!ucc.num_of_config) {
		pr_info("Failed to find ucc device-tree\n");
		return 0;
	}

	ucc.config = kcalloc(ucc.num_of_config, sizeof(struct cpumask), GFP_KERNEL);
	if (!ucc.config) {
		pr_info("Failed to alloc ucc.config\n");
		return -ENOMEM;
	}

	for_each_child_of_node(dn, child) {
		const char *mask;
		int index;

		if (of_property_read_u32(child, "index", &index)) {
			kfree(ucc.config);
			return -EINVAL;
		}

		if (of_property_read_string(child, "allowed", &mask))
			cpumask_clear(&ucc.config[index]);
		else
			cpulist_parse(mask, &ucc.config[index]);
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &ucc_attr_group);
	if (ret) {
		pr_info("Failed to create ucc sysfs group\n");
		kfree(ucc.config);
		return ret;
	}

	INIT_LIST_HEAD(&ucc.req_list);
	ucc.enable = true;

	return 0;
}

/*********************************************************************/
/*  UFC feature - User Frequency Control                             */
/*********************************************************************/
struct ufc_domain {
	const char		*name;
	struct cpumask		cpus;

	unsigned int		index;

	unsigned int		min_freq;
	unsigned int		max_freq;
	unsigned int		table_size;
	unsigned int		*freq_table;

	bool			allow_disable_cpus;

	struct freq_qos_request	min_qos_req;
	struct freq_qos_request	max_qos_req;
	struct freq_qos_request	lit_max_qos_req;
	struct freq_qos_request	min_qos_wo_boost_req;
};

struct ufc_request {
	void			(*update)(void);
	int			prio_freq;
	int			freq[USER_TYPE_END];
	s64			residency_time[USER_TYPE_END];
};

static struct exynos_ufc {
	struct ufc_domain	**domain_list;
	unsigned int		num_of_domain;

	struct ufc_request	req_list[CTRL_TYPE_END];

	const char		*col_list[10];
	int			col_size;
	int			row_size;

	unsigned int		**max_limit_table;
	unsigned int		**min_limit_table;

	unsigned int		emstune_index;
	unsigned int		strict_index;

	int			min_limit_wo_boost;

	bool			strict_enabled;

	ktime_t			last_update_time;

	struct cpumask		active_cpus;
	struct mutex		lock;
	struct kobject		kobj;
} ufc = {
	.lock = __MUTEX_INITIALIZER(ufc.lock),
};

static struct emstune_mode_request ufc_emstune_req;

/*********************************************************************/
/*  Helper functions - User Freuency Control                         */
/*********************************************************************/
#define for_each_ctrl(i)	for (i = 0; i < CTRL_TYPE_END; i++)
#define for_each_user(i)	for (i = 0; i < USER_TYPE_END; i++)
#define is_invalid_ctrl(i)	(i < 0 || i >= CTRL_TYPE_END)
#define is_invalid_user(i)	(i < 0 || i >= USER_TYPE_END)
#define is_released_freq(f)	(f == PM_QOS_DEFAULT_VALUE)

static struct ufc_domain *get_ufc_domain(int cpu)
{
	int i;

	for (i = 0; i < ufc.num_of_domain; i++) {
		struct ufc_domain *dom = ufc.domain_list[i];

		if (cpumask_test_cpu(cpu, &dom->cpus))
			return dom;
	}

	return NULL;
}

static struct ufc_request *get_ufc_request(enum ufc_ctrl_type type)
{
	return &ufc.req_list[type];
}

static void disable_domain_cpus(struct ufc_domain *dom)
{
	if (!dom->allow_disable_cpus)
		return;

	cpumask_andnot(&ufc.active_cpus, &ufc.active_cpus, &dom->cpus);
	ecs_request("UFC", &ufc.active_cpus);
}

static void enable_domain_cpus(struct ufc_domain *dom)
{
	if (!dom->allow_disable_cpus)
		return;

	cpumask_or(&ufc.active_cpus, &ufc.active_cpus, &dom->cpus);
	ecs_request("UFC", &ufc.active_cpus);
}

static struct freq_qos_request *get_freq_qos_req(struct ufc_domain *dom, int ctrl_type)
{
	switch (ctrl_type) {
		case PM_QOS_MIN_LIMIT:
			return &dom->min_qos_req;
		case PM_QOS_MIN_LIMIT_WO_BOOST:
			return &dom->min_qos_wo_boost_req;
		case PM_QOS_MAX_LIMIT:
		case PM_QOS_OVER_LIMIT:
			return &dom->max_qos_req;
		case PM_QOS_LITTLE_MAX_LIMIT:
			return &dom->lit_max_qos_req;
		default:
			return NULL;
	}
}

static int get_prio_freq(enum ufc_ctrl_type type)
{
	struct ufc_request *req = get_ufc_request(type);

	if (!req)
		return PM_QOS_DEFAULT_VALUE;

	return req->prio_freq;
}

static int update_lowest_prio_freq(enum ufc_ctrl_type type)
{
	struct ufc_request *req = get_ufc_request(type);
	int max_limit = INT_MAX;
	int user;

	for_each_user(user) {
		if (is_released_freq(req->freq[user]))
			continue;

		max_limit = min(max_limit, req->freq[user]);
	}

	req->prio_freq = max_limit == INT_MAX ? PM_QOS_DEFAULT_VALUE : max_limit;

	return req->prio_freq;
}

static int update_highest_prio_freq(enum ufc_ctrl_type type)
{
	struct ufc_request *req = get_ufc_request(type);
	int min_limit = INT_MIN;
	int user;

	for_each_user(user) {
		if (is_released_freq(req->freq[user]))
			continue;

		min_limit = max(min_limit, req->freq[user]);
	}

	req->prio_freq = min_limit == INT_MIN ? PM_QOS_DEFAULT_VALUE : min_limit;

	return req->prio_freq;
}

static int find_cloest_vfreq_index(unsigned int **table, int freq, int relation)
{
	int index;

	if (relation == CPUFREQ_RELATION_L) {
		for (index = ufc.row_size - 1; index >= 0; index--) {
			if (freq <= table[index][0])
				return index;
		}
	} else {
		for (index = 0; index < ufc.row_size; index++) {
			if (freq >= table[index][0])
				return index;
		}
	}

	return 0;
}

static void reset_limit_stat(void)
{
	int type;

	for_each_ctrl(type) {
		struct ufc_request *req = get_ufc_request(type);

		memset(req->residency_time, 0, sizeof(s64) * USER_TYPE_END);
	}
}

static void update_limit_stat(void)
{
	struct ufc_request *req;
	ktime_t now = ktime_get();
	s64 duration = ktime_ms_delta(now, ufc.last_update_time);
	int type, user;

	ufc.last_update_time = now;

	for_each_ctrl(type) {
		if (type == PM_QOS_OVER_LIMIT)
			continue;

		req = get_ufc_request(type);

		for_each_user(user) {
			if (!is_released_freq(req->freq[user]))
				req->residency_time[user] += duration;
		}
	}

	if (!ufc.strict_enabled ||
			is_released_freq(get_prio_freq(PM_QOS_MAX_LIMIT)) ||
			is_released_freq(get_prio_freq(PM_QOS_OVER_LIMIT)))
		return;

	for_each_user(user) {
		req = get_ufc_request(PM_QOS_MIN_LIMIT);

		if (!is_released_freq(req->freq[user])) {
			req = get_ufc_request(PM_QOS_OVER_LIMIT);
			req->residency_time[user] += duration;
		}
	}
}

/*********************************************************************/
/*  PM QoS handling - User Freuency Control                          */
/*********************************************************************/
static void release_limit_freq(enum ufc_ctrl_type type)
{
	struct freq_qos_request *req;
	struct ufc_domain *dom;
	int i;

	if (type == PM_QOS_LITTLE_MAX_LIMIT) {
		dom = get_ufc_domain(0);
		req = get_freq_qos_req(dom, type);
		if (!req)
			return;

		freq_qos_update_request(req, PM_QOS_DEFAULT_VALUE);

		return;
	}

	for (i = 0; i < ufc.num_of_domain; i++) {
		dom = ufc.domain_list[i];
		req = get_freq_qos_req(dom, type);
		if (!req)
			return;

		freq_qos_update_request(req, PM_QOS_DEFAULT_VALUE);
	}
}

static int determine_max_limit(void)
{
	int max_limit = update_lowest_prio_freq(PM_QOS_MAX_LIMIT);
	int over_limit = update_lowest_prio_freq(PM_QOS_OVER_LIMIT);

	/* Determine the final max limit */
	if (!ufc.strict_enabled)
		return max_limit;

	/* if max_limit is released, over_limit is also ignored */
	if (is_released_freq(max_limit))
		return PM_QOS_DEFAULT_VALUE;

	return max(max_limit, over_limit);
}

static void update_max_limit(void)
{
	enum ufc_ctrl_type type = PM_QOS_MAX_LIMIT;
	int freq, index;
	int i;

	freq = determine_max_limit();

	if (is_released_freq(freq)) {
		release_limit_freq(type);
		return;
	}

	index = find_cloest_vfreq_index(ufc.max_limit_table, freq, CPUFREQ_RELATION_H);

	for (i = 0; i < ufc.num_of_domain; i++) {
		struct ufc_domain *dom = ufc.domain_list[i];
		struct freq_qos_request *req = get_freq_qos_req(dom, type);
		unsigned int *row = ufc.max_limit_table[index];

		if (row[dom->index]) {
			enable_domain_cpus(dom);
			freq_qos_update_request(req, row[dom->index]);
		} else {
			freq_qos_update_request(req, dom->min_freq);
			disable_domain_cpus(dom);
		}
	}
}

static void update_little_max_limit(void)
{
	enum ufc_ctrl_type type = PM_QOS_LITTLE_MAX_LIMIT;
	struct ufc_domain *dom;
	struct freq_qos_request *req;
	int freq;

	freq = update_lowest_prio_freq(type);

	if (is_released_freq(freq)) {
		release_limit_freq(type);
		return;
	}

	dom = get_ufc_domain(0);
	req = get_freq_qos_req(dom, type);
	freq_qos_update_request(req, freq);
}

static void update_min_limit(void)
{
	enum ufc_ctrl_type type = PM_QOS_MIN_LIMIT;
	bool prev_strict = ufc.strict_enabled;
	int freq, index, emstune_level;
	int i;

	freq = update_highest_prio_freq(type);

	if (is_released_freq(freq)) {
		release_limit_freq(type);
		emstune_level = 0;
		ufc.strict_enabled = false;
	} else {
		unsigned int *row;

		index = find_cloest_vfreq_index(ufc.min_limit_table, freq, CPUFREQ_RELATION_L);
		row = ufc.min_limit_table[index];

		for (i = 0; i < ufc.num_of_domain; i++) {
			struct ufc_domain *dom = ufc.domain_list[i];
			struct freq_qos_request *req = get_freq_qos_req(dom, type);

			if (row[dom->index])
				freq_qos_update_request(req, row[dom->index]);
			else
				freq_qos_update_request(req, dom->min_freq);
		}
		emstune_level = row[ufc.emstune_index];
		ufc.strict_enabled = row[ufc.strict_index];
	}

	emstune_update_request(&ufc_emstune_req, emstune_level);
	update_ufcc_request(emstune_level);

	if (prev_strict != ufc.strict_enabled)
		update_max_limit();
}

static void update_min_limit_wo_boost(void)
{
	enum ufc_ctrl_type type = PM_QOS_MIN_LIMIT_WO_BOOST;
	int freq, index;
	int i;

	freq = ufc.min_limit_wo_boost;

	if (is_released_freq(freq)) {
		release_limit_freq(type);
		return;
	}

	index = find_cloest_vfreq_index(ufc.min_limit_table, freq, CPUFREQ_RELATION_L);

	for (i = 0; i < ufc.num_of_domain; i++) {
		struct ufc_domain *dom = ufc.domain_list[i];
		struct freq_qos_request *req = get_freq_qos_req(dom, type);

		freq_qos_update_request(req, ufc.min_limit_table[index][dom->index]);
	}
}

int ufc_update_request(enum ufc_user_type user, enum ufc_ctrl_type type, int freq)
{
	struct ufc_request *req;

	if (is_invalid_user(user) || is_invalid_ctrl(type))
		return -EINVAL;

	mutex_lock(&ufc.lock);

	update_limit_stat();

	req = get_ufc_request(type);
	req->freq[user] = freq;
	req->update();

	mutex_unlock(&ufc.lock);

	return 0;
}
EXPORT_SYMBOL_GPL(ufc_update_request);

/*********************************************************************/
/*  External APIs - User Freuency Control                            */
/*********************************************************************/
unsigned int get_cpufreq_max_limit(void)
{
	int i;

	for (i = 0; i < ufc.num_of_domain; i++) {
		struct ufc_domain *dom = ufc.domain_list[i];
		struct cpufreq_policy *policy;
		unsigned int max_freq;

		policy = cpufreq_cpu_get(cpumask_first(&dom->cpus));
		if (!policy)
			return 0;

		max_freq = policy->max;
		cpufreq_cpu_put(policy);

		if (max_freq > 0) {
			max_freq = min(max_freq, dom->max_freq);
			max_freq = max(max_freq, dom->min_freq);

			return max_freq;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(get_cpufreq_max_limit);

/*********************************************************************/
/*  Sysfs funcions - User Frequency Control                          */
/*********************************************************************/
struct ufc_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t);
};

#define UFC_ATTR_RO(name) struct ufc_attr attr_##name = __ATTR_RO(name);
#define UFC_ATTR_RW(name) struct ufc_attr attr_##name = __ATTR_RW(name);

static char *ctrl_type_name[] = {
	"PM_QOS_MIN_LIMIT",
	"PM_QOS_MAX_LIMIT",
	"PM_QOS_MIN_LIMIT_WO_BOOST",
	"PM_QOS_LITTLE_MAX_LIMIT",
	"PM_QOS_OVER_LIMIT",
};

static ssize_t cpufreq_table_show(struct kobject *kobj, char *buf)
{
	ssize_t count = 0;
	int row;

	for (row = 0; row < ufc.row_size; row++)
		count += snprintf(buf + count, PAGE_SIZE - count, "%d ",
				ufc.min_limit_table[row][0]);
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");

	return count;
}
static UFC_ATTR_RO(cpufreq_table);

static ssize_t cpufreq_max_limit_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", get_prio_freq(PM_QOS_MAX_LIMIT));
}
static ssize_t cpufreq_max_limit_store(struct kobject *kobj, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc_update_request(USERSPACE, PM_QOS_MAX_LIMIT, input);

	return count;
}
static UFC_ATTR_RW(cpufreq_max_limit);

static ssize_t cpufreq_min_limit_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", get_prio_freq(PM_QOS_MIN_LIMIT));
}
static ssize_t cpufreq_min_limit_store(struct kobject *kobj, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc_update_request(USERSPACE, PM_QOS_MIN_LIMIT, input);

	return count;
}
static UFC_ATTR_RW(cpufreq_min_limit);

static ssize_t cpufreq_min_limit_wo_boost_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", ufc.min_limit_wo_boost);
}
static ssize_t cpufreq_min_limit_wo_boost_store(struct kobject *kobj, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc.min_limit_wo_boost = input;
	update_min_limit_wo_boost();

	return count;
}
static UFC_ATTR_RW(cpufreq_min_limit_wo_boost);

static ssize_t over_limit_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", get_prio_freq(PM_QOS_OVER_LIMIT));
}

static ssize_t over_limit_store(struct kobject *kobj, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc_update_request(USERSPACE, PM_QOS_OVER_LIMIT, input);

	return count;
}
static UFC_ATTR_RW(over_limit);

static ssize_t little_max_limit_show(struct kobject *kobj, char *buf)
{
	return snprintf(buf, 10, "%d\n", get_prio_freq(PM_QOS_LITTLE_MAX_LIMIT));
}
static ssize_t little_max_limit_store(struct kobject *kobj, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	ufc_update_request(USERSPACE, PM_QOS_LITTLE_MAX_LIMIT, input);

	return count;
}
static UFC_ATTR_RW(little_max_limit);

static ssize_t debug_max_table_show(struct kobject *kobj, char *buf)
{
	int count = 0;
	int col, row;

	for (col = 0; col < ufc.col_size; col++)
		count += snprintf(buf + count, PAGE_SIZE - count, "%9s",
				ufc.col_list[col]);
	count += snprintf(buf + count, PAGE_SIZE - count,"\n");

	for (row = 0; row < ufc.row_size; row++) {
		for (col = 0; col < ufc.col_size; col++)
			count += snprintf(buf + count, PAGE_SIZE - count, "%9d",
					ufc.max_limit_table[row][col]);
		count += snprintf(buf + count, PAGE_SIZE - count,"\n");
	}

	return count;
}
static UFC_ATTR_RO(debug_max_table);

static ssize_t debug_min_table_show(struct kobject *kobj, char *buf)
{
	int count = 0;
	int col, row;

	for (col = 0; col < ufc.col_size; col++)
		count += snprintf(buf + count, PAGE_SIZE - count, "%9s",
				ufc.col_list[col]);
	count += snprintf(buf + count, PAGE_SIZE - count,"\n");

	for (row = 0; row < ufc.row_size; row++) {
		for (col = 0; col < ufc.col_size; col++)
			count += snprintf(buf + count, PAGE_SIZE - count, "%9d",
					ufc.min_limit_table[row][col]);
		count += snprintf(buf + count, PAGE_SIZE - count,"\n");
	}

	return count;
}
static UFC_ATTR_RO(debug_min_table);

static ssize_t debug_id_show(struct kobject *kobj, char *buf)
{
	int count = 0;
	int i;

	for (i = 0; i < ufc.col_size; i++)
		count += sprintf(buf + count, "[%d] %s\n", i, ufc.col_list[i]);

	return count;
}
static UFC_ATTR_RO(debug_id);

static ssize_t limit_stat_show(struct kobject *kobj, char *buf)
{
	ssize_t count = 0;
	int type, user;

	count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	count += snprintf(buf + count, PAGE_SIZE - count, "%29s |", "Control Type");
	for_each_user(user)
		count += snprintf(buf + count, PAGE_SIZE - count, "%14s%d", "user", user);
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");

	count += snprintf(buf + count, PAGE_SIZE - count, "------------------------------+");
	for_each_user(user)
		count += snprintf(buf + count, PAGE_SIZE - count, "---------------");
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");

	mutex_lock(&ufc.lock);
	update_limit_stat();

	for_each_ctrl(type) {
		struct ufc_request *req;

		if (type == PM_QOS_MIN_LIMIT_WO_BOOST)
			continue;

		req = get_ufc_request(type);

		count += snprintf(buf + count, PAGE_SIZE - count, "[%d]%26s |",
				type, ctrl_type_name[type]);
		for_each_user(user)
			count += snprintf(buf + count, PAGE_SIZE - count, "%15llu",
					req->residency_time[user]);
		count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	}
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");

	mutex_unlock(&ufc.lock);

	return count;
}
static ssize_t limit_stat_store(struct kobject *kobj, const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	mutex_lock(&ufc.lock);
	reset_limit_stat();
	mutex_unlock(&ufc.lock);

	return count;
}
static UFC_ATTR_RW(limit_stat);

static ssize_t info_show(struct kobject *kobj, char *buf)
{
	ssize_t count = 0;
	int type, user;

	count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	count += snprintf(buf + count, PAGE_SIZE - count, "%29s |", "Control Type");
	for_each_user(user)
		count += snprintf(buf + count, PAGE_SIZE - count, "%8s%d", "user", user);
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");

	count += snprintf(buf + count, PAGE_SIZE - count, "------------------------------+");
	for_each_user(user)
		count += snprintf(buf + count, PAGE_SIZE - count, "---------");
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");

	for_each_ctrl(type) {
		struct ufc_request *req = get_ufc_request(type);

		count += snprintf(buf + count, PAGE_SIZE - count, "[%d]%26s |",
				type, ctrl_type_name[type]);
		for_each_user(user)
			count += snprintf(buf + count, PAGE_SIZE - count, "%9d", req->freq[user]);
		count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	}
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");

	return count;
}
static UFC_ATTR_RO(info);

static struct attribute *exynos_ufc_attrs[] = {
	&attr_cpufreq_table.attr,
	&attr_cpufreq_min_limit.attr,
	&attr_cpufreq_min_limit_wo_boost.attr,
	&attr_cpufreq_max_limit.attr,
	&attr_over_limit.attr,
	&attr_little_max_limit.attr,
	&attr_limit_stat.attr,
	&attr_debug_max_table.attr,
	&attr_debug_min_table.attr,
	&attr_info.attr,
	&attr_debug_id.attr,
	NULL,
};

#define attr_to_ufcattr(a) container_of(a, struct ufc_attr, attr)
static ssize_t ufc_sysfs_show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct ufc_attr *ufcattr = attr_to_ufcattr(at);
	int ret = 0;

	if (ufcattr->show)
		ret =  ufcattr->show(kobj, buf);

	return ret;
}

static ssize_t ufc_sysfs_store(struct kobject *kobj, struct attribute *at,
					const char *buf, size_t count)
{
	struct ufc_attr *ufcattr = attr_to_ufcattr(at);
	int ret = 0;

	if (ufcattr->show)
		ret = ufcattr->store(kobj, buf, count);

	return ret;
}

static const struct sysfs_ops ufc_sysfs_ops = {
	.show	= ufc_sysfs_show,
	.store	= ufc_sysfs_store,
};

static struct kobj_type ktype_ufc = {
	.sysfs_ops	= &ufc_sysfs_ops,
	.default_attrs	= exynos_ufc_attrs,
};

static int init_ufc_sysfs(struct kobject *kobj)
{
	int ret;

	/* /sys/devices/platform/exynos-ufcc/ufc */
	ret = kobject_init_and_add(&ufc.kobj, &ktype_ufc, kobj, "ufc");
	if (ret) {
		pr_info("Failed to add ufc sysfs\n");
		return ret;
	}

	/* /sys/devices/system/cpu/cpufreq_limit */
	ret = sysfs_create_link(&cpu_subsys.dev_root->kobj, &ufc.kobj, "cpufreq_limit");
	if (ret) {
		pr_info("Failed to link UFC sysfs to cpuctrl\n");
		return ret;
	}

	return 0;
}

/*********************************************************************/
/*  Initialization - User Frequency Control                          */
/*********************************************************************/
static void init_ufc_request(void)
{
	int type;

	for_each_ctrl(type) {
		struct ufc_request *req = get_ufc_request(type);

		switch (type) {
			case PM_QOS_MIN_LIMIT:
				req->update = update_min_limit;
				break;
			case PM_QOS_MAX_LIMIT:
			case PM_QOS_OVER_LIMIT:
				req->update = update_max_limit;
				break;
			case PM_QOS_LITTLE_MAX_LIMIT:
				req->update = update_little_max_limit;
				break;
			default:
				break;
		}

		req->prio_freq = PM_QOS_DEFAULT_VALUE;
		memset(req->freq, PM_QOS_DEFAULT_VALUE, sizeof(int) * USER_TYPE_END);
		memset(req->residency_time, 0, sizeof(s64) * USER_TYPE_END);
	}
}

static void ufc_free_all(void)
{
	int i;

	/* Free min/max_limit_table */
	for (i = 0; i < ufc.row_size; i++) {
		kfree(ufc.min_limit_table[i]);
		kfree(ufc.min_limit_table[i]);
	}
	kfree(ufc.min_limit_table);
	kfree(ufc.min_limit_table);

	/* Free ufc_domain of domain_list */
	for (i = 0; i < ufc.num_of_domain; i++) {
		struct ufc_domain *dom = ufc.domain_list[i];

		freq_qos_tracer_remove_request(&dom->min_qos_req);
		freq_qos_tracer_remove_request(&dom->max_qos_req);
		freq_qos_tracer_remove_request(&dom->min_qos_wo_boost_req);
		if (cpumask_test_cpu(0, &dom->cpus))
			freq_qos_tracer_remove_request(&dom->lit_max_qos_req);

		kfree(dom->freq_table);
		kfree(dom);
	}
	kfree(ufc.domain_list);

	pr_info("Failed: can't initialize ufc table and domain");
}

static unsigned int find_closest_freq(struct ufc_domain *dom, unsigned int freq, int relation)
{
	int i;

	if (relation == CPUFREQ_RELATION_L) {
		for (i = 0; i < dom->table_size; i++)
			if (freq <= dom->freq_table[i]) {
				return dom->freq_table[i];
			}
		return dom->max_freq;
	} else {
		for (i = dom->table_size - 1; i >= 0; i--)
			if (freq >= dom->freq_table[i]) {
				return dom->freq_table[i];
			}
		return dom->min_freq;
	}
	return dom->min_freq;
}

static unsigned int **make_limit_table(unsigned int *raw_table, int size, int relation)
{
	struct ufc_domain *lit_dom = get_ufc_domain(0);
	int orig_size = size / ufc.col_size;
	int lit_size = lit_dom->table_size;
	int i, j;
	unsigned int **table;

	ufc.row_size = orig_size + lit_size;

	table = kcalloc(ufc.row_size, sizeof(u32 *), GFP_KERNEL);
	if (!table)
		return NULL;

	for (i = 0; i < ufc.row_size; i++) {
		table[i] = kcalloc(ufc.col_size, sizeof(u32), GFP_KERNEL);
		if (!table[i])
			goto free;
	}

	for (i = 0; i < orig_size; i++) {
		int pos = i * ufc.col_size;
		unsigned int *row = table[i];

		row[0] = raw_table[pos];
		for (j = 0; j < ufc.num_of_domain; j++) {
			struct ufc_domain *dom = ufc.domain_list[j];

			row[dom->index] =
				find_closest_freq(dom, raw_table[pos + dom->index], relation);
		}
		row[ufc.emstune_index] = raw_table[pos + ufc.emstune_index];
		row[ufc.strict_index] = raw_table[pos + ufc.strict_index];
	}

	for (i = 0; i < lit_size; i++) {
		unsigned int *row = table[(ufc.row_size - 1) - i];

		row[0] = lit_dom->freq_table[i] / 8;
		row[lit_dom->index] = lit_dom->freq_table[i];
	}

	return table;

free:
	for (i = 0; i < ufc.row_size; i++)
		kfree(table[i]);

	kfree(table);

	return NULL;
}

static unsigned int **parse_ufc_limit_table(struct device_node *dn)
{
	unsigned int **table, *raw_table;
	int size, relation;

	if (of_property_read_u32(dn, "relation", &relation))
		return NULL;

	size = of_property_count_u32_elems(dn, "table");
	if (size <= 0)
		return NULL;

	raw_table = kcalloc(size, sizeof(u32), GFP_KERNEL);
	if (!raw_table)
		return NULL;

	if (of_property_read_u32_array(dn, "table", raw_table, size)) {
		kfree(raw_table);
		return NULL;
	}

	table = make_limit_table(raw_table, size, relation);

	kfree(raw_table);

	return table;
}

static int init_ufc_limit_table(struct device_node *dn)
{
	struct device_node *child;

	dn = of_get_child_by_name(dn, "limits");
	if (!dn)
		return -EINVAL;

	/* Parse min_limit_table */
	child = of_get_child_by_name(dn, "min-limit");
	if (!child)
		return -EINVAL;

	ufc.min_limit_table = parse_ufc_limit_table(child);
	if (!ufc.min_limit_table)
		return -EINVAL;

	/* Parse max_limit_table */
	child = of_get_child_by_name(dn, "max-limit");
	if (!child)
		return -EINVAL;

	ufc.max_limit_table = parse_ufc_limit_table(child);
	if (!ufc.max_limit_table)
		return -EINVAL;

	return 0;
}

static void register_freq_qos(struct ufc_domain *dom, struct cpufreq_policy *policy)
{
	freq_qos_tracer_add_request_name("ufc_cpufreq_min_limit", &policy->constraints,
			&dom->min_qos_req, FREQ_QOS_MIN, PM_QOS_DEFAULT_VALUE);
	freq_qos_tracer_add_request_name("ufc_cpufreq_max_limit", &policy->constraints,
			&dom->max_qos_req, FREQ_QOS_MAX, PM_QOS_DEFAULT_VALUE);
	freq_qos_tracer_add_request_name("ufc_cpufreq_min_limit_wo_boost", &policy->constraints,
			&dom->min_qos_wo_boost_req, FREQ_QOS_MIN, PM_QOS_DEFAULT_VALUE);

	if (cpumask_test_cpu(0, &dom->cpus))
		freq_qos_tracer_add_request_name("ufc_little_max_limit", &policy->constraints,
				&dom->lit_max_qos_req, FREQ_QOS_MAX, PM_QOS_DEFAULT_VALUE);
}

static int parse_ufc_domain(struct device_node *dn, struct ufc_domain *dom)
{
	const char *buf;
	int i;

	if (of_property_read_string(dn, "domain-name", &dom->name))
		return -EINVAL;

	if (of_property_read_string(dn, "shared-cpus", &buf))
		return -EINVAL;
	cpulist_parse(buf, &dom->cpus);

	dom->allow_disable_cpus = of_property_read_bool(dn, "allow-disable-cpus");

	for (i = 0; i < ufc.col_size; i++)
		if (!strcmp(dom->name, ufc.col_list[i]))
			dom->index = i;

	return 0;
}

static int parse_cpufreq_info(struct ufc_domain *dom, struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *pos;
	unsigned int size, *table;
	int i = 0;

	size = cpufreq_table_count_valid_entries(policy);
	if (size == 0)
		return -EINVAL;

	table = kcalloc(size, sizeof(u32), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	cpufreq_for_each_valid_entry(pos, policy->freq_table)
		table[i++] = pos->frequency;

	dom->min_freq = table[0];
	dom->max_freq = table[size - 1];
	dom->table_size = size;
	dom->freq_table = table;

	return 0;
}

static int init_ufc_domain(struct device_node *dn)
{
	struct device_node *child;
	int ret, i = 0;

	dn = of_get_child_by_name(dn, "domains");
	if (!dn)
		return -EINVAL;

	ufc.num_of_domain = of_get_child_count(dn);
	if (!ufc.num_of_domain)
		return -EINVAL;

	ufc.domain_list = kcalloc(ufc.num_of_domain, sizeof(struct ufc_domain *), GFP_KERNEL);
	if (!ufc.domain_list)
		return -ENOMEM;

	for_each_child_of_node(dn, child) {
		struct cpufreq_policy *policy;
		struct ufc_domain *dom;

		dom = kzalloc(sizeof(struct ufc_domain), GFP_KERNEL);
		if (!dom)
			return -ENOMEM;

		if (parse_ufc_domain(child, dom))
			return -EINVAL;

		policy = cpufreq_cpu_get(cpumask_first(&dom->cpus));
		if (!policy)
			return -EINVAL;

		ret = parse_cpufreq_info(dom, policy);
		if (ret)
			return ret;

		register_freq_qos(dom, policy);
		cpufreq_cpu_put(policy);

		ufc.domain_list[i++] = dom;
	}

	return 0;
}

static unsigned int parse_table_info(struct device_node *dn)
{
	int i;

	ufc.col_size = of_property_count_strings(dn, "table-info");
	if (ufc.col_size <= 0)
		return -EINVAL;

	if (of_property_read_string_array(dn, "table-info", ufc.col_list, ufc.col_size) < 0)
		return -EINVAL;

	for (i = 0; i < ufc.col_size; i++) {
		if (!strcmp("emstune", ufc.col_list[i]))
			ufc.emstune_index = i;
		else if (!strcmp("strict", ufc.col_list[i]))
			ufc.strict_index = i;
	}

	return 0;
}

static int init_ufc(struct device_node *dn)
{
	int ret = 0;

	ret = parse_table_info(dn);
	if (ret)
		return ret;

	ufc.min_limit_wo_boost = PM_QOS_DEFAULT_VALUE;

	emstune_add_request(&ufc_emstune_req);

	return 0;
}

static int exynos_ufc_init(struct platform_device *pdev)
{
	struct device_node *dn;

	dn = of_find_node_by_name(pdev->dev.of_node, "ufc");

	if (init_ufc(dn)) {
		pr_info("exynos-ufc: Failed to init init_ufc\n");
		return 0;
	}

	if (init_ufc_domain(dn)) {
		pr_info("exynos-ufc: Failed to init init_ufc_domain\n");
		ufc_free_all();
		return 0;
	}

	if (init_ufc_limit_table(dn)) {
		pr_info("exynos-ufc: Failed to init init_ufc_limit_table\n");
		ufc_free_all();
		return 0;
	}

	if (init_ufc_sysfs(&pdev->dev.kobj)) {
		pr_info("exynos-ufc: Failed to init sysfs\n");
		ufc_free_all();
		return 0;
	}

	cpumask_setall(&ufc.active_cpus);
	if (ecs_request_register("UFC", &ufc.active_cpus)) {
		pr_info("exynos-ufc: Failed to register ecs\n");
		ufc_free_all();
		return 0;
	}

	init_ufc_request();

	pr_info("exynos-ufc: Complete UFC driver initialization\n");

	return 0;
}

/*********************************************************************
 *                       DRIVER INITIALIZATION                       *
 *********************************************************************/
static int exynos_ufcc_probe(struct platform_device *pdev)
{
	int ret;

	ret = exynos_ucc_init(pdev);
	if (ret)
		return ret;

	ret = exynos_ufc_init(pdev);
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id of_exynos_ufcc_match[] = {
	{ .compatible = "samsung,exynos-ufcc", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_ufcc_match);

static struct platform_driver exynos_ufcc_driver = {
	.driver = {
		.name	= "exynos-ufcc",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_ufcc_match,
	},
	.probe		= exynos_ufcc_probe,
};

static int __init exynos_ufcc_init(void)
{
	return platform_driver_register(&exynos_ufcc_driver);
}
late_initcall(exynos_ufcc_init);

static void __exit exynos_ufcc_exit(void)
{
	platform_driver_unregister(&exynos_ufcc_driver);
}
module_exit(exynos_ufcc_exit);

MODULE_SOFTDEP("pre: exynos-acme");
MODULE_DESCRIPTION("Exynos UFCC driver");
MODULE_LICENSE("GPL");
