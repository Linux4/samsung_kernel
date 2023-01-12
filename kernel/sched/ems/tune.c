/*
 * Frequency variant cpufreq driver
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/reciprocal_div.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include <dt-bindings/soc/samsung/ems.h>

/******************************************************************************
 * simple mode level list management based on prio                            *
 ******************************************************************************/
/**
 * MLLIST_HEAD_INIT - static struct mllist_head initializer
 * @head:	struct mllist_head variable name
 */
#define MLLIST_HEAD_INIT(head)				\
{							\
	.node_list = LIST_HEAD_INIT((head).node_list)	\
}

/**
 * mllist_node_init - Dynamic struct mllist_node initializer
 * @node:	&struct mllist_node pointer
 * @prio:	initial node priority
 */
static inline void mllist_node_init(struct mllist_node *node, int prio)
{
	node->prio = prio;
	INIT_LIST_HEAD(&node->prio_list);
	INIT_LIST_HEAD(&node->node_list);
}

/**
 * mllist_head_empty - return !0 if a mllist_head is empty
 * @head:	&struct mllist_head pointer
 */
static inline int mllist_head_empty(const struct mllist_head *head)
{
	return list_empty(&head->node_list);
}

/**
 * mllist_node_empty - return !0 if mllist_node is not on a list
 * @node:	&struct mllist_node pointer
 */
static inline int mllist_node_empty(const struct mllist_node *node)
{
	return list_empty(&node->node_list);
}

/**
 * mllist_last - return the last node (and thus, lowest priority)
 * @head:	the &struct mllist_head pointer
 *
 * Assumes the mllist is _not_ empty.
 */
static inline struct mllist_node *mllist_last(const struct mllist_head *head)
{
	return list_entry(head->node_list.prev,
			  struct mllist_node, node_list);
}

/**
 * mllist_first - return the first node (and thus, highest priority)
 * @head:	the &struct mllist_head pointer
 *
 * Assumes the mllist is _not_ empty.
 */
static inline struct mllist_node *mllist_first(const struct mllist_head *head)
{
	return list_entry(head->node_list.next,
			  struct mllist_node, node_list);
}

/**
 * mllist_add - add @node to @head
 *
 * @node:	&struct mllist_node pointer
 * @head:	&struct mllist_head pointer
 */
static void mllist_add(struct mllist_node *node, struct mllist_head *head)
{
	struct mllist_node *first, *iter, *prev = NULL;
	struct list_head *node_next = &head->node_list;

	WARN_ON(!mllist_node_empty(node));
	WARN_ON(!list_empty(&node->prio_list));

	if (mllist_head_empty(head))
		goto ins_node;

	first = iter = mllist_first(head);

	do {
		if (node->prio < iter->prio) {
			node_next = &iter->node_list;
			break;
		}

		prev = iter;
		iter = list_entry(iter->prio_list.next,
				struct mllist_node, prio_list);
	} while (iter != first);

	if (!prev || prev->prio != node->prio)
		list_add_tail(&node->prio_list, &iter->prio_list);
ins_node:
	list_add_tail(&node->node_list, node_next);
}

/**
 * mllist_del - Remove a @node from mllist.
 *
 * @node:	&struct mllist_node pointer - entry to be removed
 * @head:	&struct mllist_head pointer - list head
 */
static void mllist_del(struct mllist_node *node, struct mllist_head *head)
{
	if (!list_empty(&node->prio_list)) {
		if (node->node_list.next != &head->node_list) {
			struct mllist_node *next;

			next = list_entry(node->node_list.next,
					struct mllist_node, node_list);

			/* add the next mllist_node into prio_list */
			if (list_empty(&next->prio_list))
				list_add(&next->prio_list, &node->prio_list);
		}
		list_del_init(&node->prio_list);
	}

	list_del_init(&node->node_list);
}

/**
 * mllist_for_each_entry - iterate over list of given type
 * @pos:	the type * to use as a loop counter
 * @head:	the head for your list
 * @mem:	the name of the list_head within the struct
 */
#define mllist_for_each_entry(pos, head, mem)	\
	 list_for_each_entry(pos, &(head)->node_list, mem.node_list)

/******************************************************************************
 * emstune mode/level/set management                                          *
 ******************************************************************************/
struct {
	struct emstune_mode *modes;
	struct emstune_mode *modes_backup;

	int cur_mode;
	int cur_level;

	int mode_count;
	int level_count;

	int boost_level;

	struct {
		int ongoing;
		struct emstune_set set;
		struct delayed_work work;
	} boot;
} emstune;

static inline struct emstune_set *emstune_cur_set(void)
{
	if (!emstune.modes)
		return NULL;

	if (emstune.boot.ongoing)
		return &emstune.boot.set;

	return &emstune.modes[emstune.cur_mode].sets[emstune.cur_level];
}

static int emstune_notify(void);

static DEFINE_SPINLOCK(emstune_lock);

static void emstune_mode_change(int next_mode)
{
	unsigned long flags;
	char msg[32];

	spin_lock_irqsave(&emstune_lock, flags);

	if (emstune.cur_mode == next_mode) {
		spin_unlock_irqrestore(&emstune_lock, flags);
		return;
	}

	trace_emstune_mode(emstune.cur_mode, emstune.cur_level);
	emstune.cur_mode = next_mode;
	emstune_notify();

	spin_unlock_irqrestore(&emstune_lock, flags);

	snprintf(msg, sizeof(msg), "NOTI_MODE=%d", emstune.cur_mode);
	send_uevent(msg);
}

static void emstune_level_change(int next_level)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);

	if (emstune.cur_level == next_level) {
		spin_unlock_irqrestore(&emstune_lock, flags);
		return;
	}

	trace_emstune_mode(emstune.cur_mode, emstune.cur_level);
	emstune.cur_level = next_level;
	emstune_notify();

	spin_unlock_irqrestore(&emstune_lock, flags);
}

static void emstune_reset(int mode, int level)
{
	memcpy(&emstune.modes[mode].sets[level],
	       &emstune.modes_backup[mode].sets[level],
	       sizeof(struct emstune_set));
}

/******************************************************************************
 * emstune level request                                                      *
 ******************************************************************************/
static struct mllist_head emstune_req_list = MLLIST_HEAD_INIT(emstune_req_list);

enum emstune_req_type {
	REQ_ADD,
	REQ_UPDATE,
	REQ_REMOVE
};

static DEFINE_SPINLOCK(emstune_req_lock);

static void
__emstune_update_request(struct emstune_mode_request *req,
			enum emstune_req_type type, s32 new_level)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_req_lock, flags);

	switch (type) {
	case REQ_REMOVE:
		mllist_del(&req->node, &emstune_req_list);
		req->active = 0;
		break;
	case REQ_UPDATE:
		mllist_del(&req->node, &emstune_req_list);
	case REQ_ADD:
		mllist_node_init(&req->node, new_level);
		mllist_add(&req->node, &emstune_req_list);
		req->active = 1;
		break;
	default:
		;
	}

	new_level = 0;
	if (!mllist_head_empty(&emstune_req_list))
		new_level = mllist_last(&emstune_req_list)->prio;

	spin_unlock_irqrestore(&emstune_req_lock, flags);

	emstune_level_change(new_level);
}

#define DEFAULT_LEVEL	(0)
static void emstune_work_fn(struct work_struct *work)
{
	struct emstune_mode_request *req = container_of(to_delayed_work(work),
						  struct emstune_mode_request,
						  work);

	emstune_update_request(req, DEFAULT_LEVEL);
}

static inline int emstune_request_active(struct emstune_mode_request *req)
{
	unsigned long flags;
	int active;

	spin_lock_irqsave(&emstune_req_lock, flags);
	active = req->active;
	spin_unlock_irqrestore(&emstune_req_lock, flags);

	return active;
}

void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line)
{
	if (emstune_request_active(req))
		return;

	INIT_DELAYED_WORK(&req->work, emstune_work_fn);
	req->func = func;
	req->line = line;

	__emstune_update_request(req, REQ_ADD, DEFAULT_LEVEL);
}
EXPORT_SYMBOL_GPL(__emstune_add_request);

void emstune_remove_request(struct emstune_mode_request *req)
{
	if (!emstune_request_active(req))
		return;

	if (delayed_work_pending(&req->work))
		cancel_delayed_work_sync(&req->work);
	destroy_delayed_work_on_stack(&req->work);

	__emstune_update_request(req, REQ_REMOVE, DEFAULT_LEVEL);
}
EXPORT_SYMBOL_GPL(emstune_remove_request);

void emstune_update_request(struct emstune_mode_request *req, s32 new_level)
{
	/* ignore if the request is not active */
	if (!emstune_request_active(req))
		return;

	/* ignore if the value is out of range except boost level */
	if ((new_level < 0 || new_level > emstune.level_count))
		return;

	__emstune_update_request(req, REQ_UPDATE, new_level);
}
EXPORT_SYMBOL_GPL(emstune_update_request);

void emstune_update_request_timeout(struct emstune_mode_request *req, s32 new_value,
				unsigned long timeout_ms)
{
	if (delayed_work_pending(&req->work))
		cancel_delayed_work_sync(&req->work);

	emstune_update_request(req, new_value);

	schedule_delayed_work(&req->work, msecs_to_jiffies(timeout_ms));
}
EXPORT_SYMBOL_GPL(emstune_update_request_timeout);

void emstune_boost(struct emstune_mode_request *req, int enable)
{
	if (enable)
		emstune_update_request(req, emstune.boost_level);
	else
		emstune_update_request(req, 0);
}
EXPORT_SYMBOL_GPL(emstune_boost);

void emstune_boost_timeout(struct emstune_mode_request *req, unsigned long timeout_ms)
{
	emstune_update_request_timeout(req, emstune.boost_level, timeout_ms);
}
EXPORT_SYMBOL_GPL(emstune_boost_timeout);

int emstune_get_cur_mode(void)
{
	return emstune.cur_mode;
}
EXPORT_SYMBOL_GPL(emstune_get_cur_mode);

int emstune_get_cur_level(void)
{
	return emstune.cur_level;
}
EXPORT_SYMBOL_GPL(emstune_get_cur_level);

/******************************************************************************
 * emstune notify                                                             *
 ******************************************************************************/
static RAW_NOTIFIER_HEAD(emstune_chain);

/* emstune_lock *MUST* be held before notifying */
static int emstune_notify(void)
{
	struct emstune_set *cur_set = emstune_cur_set();

	if (!cur_set)
		return 0;

	return raw_notifier_call_chain(&emstune_chain, 0, cur_set);
}

int emstune_register_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&emstune_chain, nb);
}
EXPORT_SYMBOL_GPL(emstune_register_notifier);

int emstune_unregister_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&emstune_chain, nb);
}
EXPORT_SYMBOL_GPL(emstune_unregister_notifier);

/******************************************************************************
 * initializing and parsing parameter                                         *
 ******************************************************************************/
static char *task_cgroup_name[] = {
	"root",
	"foreground",
	"background",
	"top-app",
	"rt",
	"system",
	"system-background",
	"nnapi-hal",
	"camera-daemon",
};

static inline void
parse_coregroup_field(struct device_node *dn, const char *name,
				int (field)[VENDOR_NR_CPUS],
				int default_value)
{
	int count, cpu, cursor;
	u32 val[VENDOR_NR_CPUS];

	for (count = 0; count < VENDOR_NR_CPUS; count++)
		val[count] = default_value;

	count = of_property_count_u32_elems(dn, name);
	if (count < 0)
		goto skip_parse_value;

	if (of_property_read_u32_array(dn, name, (u32 *)&val, count))
		goto skip_parse_value;

skip_parse_value:
	count = 0;
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		for_each_cpu(cursor, cpu_coregroup_mask(cpu))
			field[cursor] = val[count];

		count++;
	}
}

static inline void
parse_cgroup_field(struct device_node *dn, int *field, int default_value)
{
	int group, val;

	for (group = 0; group < CGROUP_COUNT; group++) {
		if (!of_property_read_u32(dn, task_cgroup_name[group], &val))
			field[group] = val;
		else
			field[group] = default_value;
	}
}

static inline int
parse_cgroup_field_mask(struct device_node *dn, struct cpumask *mask)
{
	int group;
	const char *buf;

	for (group = 0; group < CGROUP_COUNT; group++) {
		if (!of_property_read_string(dn, task_cgroup_name[group], &buf))
			cpulist_parse(buf, &mask[group]);
		else {
			/* if it failed to parse mask, set mask all */
			cpumask_copy(&mask[group], cpu_possible_mask);
		}
	}

	return 0;
}

static inline void
parse_cgroup_coregroup_field(struct device_node *dn,
			int (field)[CGROUP_COUNT][VENDOR_NR_CPUS],
			int default_value)
{
	int group;

	for (group = 0; group < CGROUP_COUNT; group++)
		parse_coregroup_field(dn,
			task_cgroup_name[group], field[group], default_value);
}

/* energy table */
static void parse_specific_energy_table(struct device_node *dn,
			       struct emstune_specific_energy_table *specific_et)
{
	parse_coregroup_field(dn, "capacity-mips-mhzs", specific_et->mips_mhz, 0);
	parse_coregroup_field(dn, "dynamic-power-coeffs", specific_et->dynamic_coeff, 0);
}

/* sched policy */
static void parse_sched_policy(struct device_node *dn,
			struct emstune_sched_policy *sched_policy)
{
	parse_cgroup_field(dn, sched_policy->policy, 0);
}

/* active weight */
static void parse_active_weight(struct device_node *dn,
			struct emstune_active_weight *active_weight)
{
	parse_cgroup_coregroup_field(dn, active_weight->ratio, 100);
}

/* idle weight */
static void parse_idle_weight(struct device_node *dn,
			struct emstune_idle_weight *idle_weight)
{
	parse_cgroup_coregroup_field(dn, idle_weight->ratio, 100);
}

/* freqboost */
static void parse_freqboost(struct device_node *dn,
			struct emstune_freqboost *freqboost)
{
	parse_cgroup_coregroup_field(dn, freqboost->ratio, 0);
}

/* wakeboost */
static void parse_wakeboost(struct device_node *dn,
			struct emstune_wakeboost *wakeboost)
{
	parse_cgroup_coregroup_field(dn, wakeboost->ratio, 0);
}

/* energy step governor */
static void parse_esg(struct device_node *dn, struct emstune_esg *esg)
{
	parse_coregroup_field(dn, "step", esg->step, 0);
	parse_coregroup_field(dn, "pelt-margin", esg->pelt_margin, 25);
}

/* cpufreq governor parameters */
static void parse_cpufreq_gov(struct device_node *dn,
			      struct emstune_cpufreq_gov *cpufreq_gov)
{
	parse_coregroup_field(dn, "patient-mode", cpufreq_gov->patient_mode, 0);
	parse_coregroup_field(dn, "pelt-boost", cpufreq_gov->pelt_boost, 0);

	parse_coregroup_field(dn, "pelt-margin",
			      cpufreq_gov->split_pelt_margin, 25);
	parse_coregroup_field(dn, "pelt-margin-freq",
			      cpufreq_gov->split_pelt_margin_freq, INT_MAX);

	parse_coregroup_field(dn, "up-rate-limit-ms",
			      cpufreq_gov->split_up_rate_limit, 4);
	parse_coregroup_field(dn, "up-rate-limit-freq",
			      cpufreq_gov->split_up_rate_limit_freq, INT_MAX);

	if (of_property_read_u32(dn, "down-rate-limit-ms", &cpufreq_gov->down_rate_limit))
		cpufreq_gov->down_rate_limit = 4; /* 4ms */

	of_property_read_u32(dn, "rapid-scale-up", &cpufreq_gov->rapid_scale_up);
	of_property_read_u32(dn, "rapid-scale-down", &cpufreq_gov->rapid_scale_down);
	parse_coregroup_field(dn, "dis-buck-share", cpufreq_gov->dis_buck_share, 0);
}

/* stt */
static void parse_stt(struct device_node *dn, struct emstune_stt *stt)
{
	parse_cgroup_field(dn, stt->htask_enable, 0);
	of_property_read_u32(dn, "boost-step", &stt->boost_step);
	of_property_read_u32(dn, "htask-cnt", &stt->htask_cnt);
}

/* ontime migration */
#define UPPER_BOUNDARY	0
#define LOWER_BOUNDARY	1

static void __update_ontime_domain(struct ontime_dom *dom,
				struct list_head *dom_list)
{
	/*
	 * At EMS initializing time, capacity_cpu_orig() cannot be used
	 * because it returns capacity that does not reflect frequency.
	 * So, use et_max_cap() instead of capacity_cpu_orig().
	 */
	unsigned long max_cap = et_max_cap(cpumask_any(&dom->cpus));

	if (dom->upper_ratio < 0)
		dom->upper_boundary = 1024;
	else if (dom == list_last_entry(dom_list, struct ontime_dom, node)) {
		/* Upper boundary of last domain is meaningless */
		dom->upper_boundary = 1024;
	} else
		dom->upper_boundary = max_cap * dom->upper_ratio / 100;

	if (dom->lower_ratio < 0)
		dom->lower_boundary = 0;
	else if (dom == list_first_entry(dom_list, struct ontime_dom, node)) {
		/* Lower boundary of first domain is meaningless */
		dom->lower_boundary = 0;
	} else
		dom->lower_boundary = max_cap * dom->lower_ratio / 100;
}

static void update_ontime_domain(struct list_head *dom_list, int cpu,
				unsigned long ratio, int type)
{
	struct ontime_dom *dom;

	list_for_each_entry(dom, dom_list, node) {
		if (cpumask_test_cpu(cpu, &dom->cpus)) {
			if (type == UPPER_BOUNDARY)
				dom->upper_ratio = ratio;
			else
				dom->lower_ratio = ratio;

			__update_ontime_domain(dom, dom_list);
		}
	}
}

void emstune_ontime_init(void)
{
	int mode, level;

	for (mode = 0; mode < emstune.mode_count; mode++) {
		for (level = 0; level < emstune.level_count; level++) {
			struct emstune_set *set;
			struct ontime_dom *dom;

			set = &emstune.modes[mode].sets[level];
			list_for_each_entry(dom, &set->ontime.dom_list, node)
				__update_ontime_domain(dom,
						&set->ontime.dom_list);
		}
	}
}

static int init_dom_list(struct device_node *dn,
				struct list_head *dom_list)
{
	struct device_node *dom_dn;

	for_each_child_of_node(dn, dom_dn) {
		struct ontime_dom *dom;
		const char *buf;

		dom = kzalloc(sizeof(struct ontime_dom), GFP_KERNEL);
		if (!dom)
			return -ENOMEM;

		if (of_property_read_string(dom_dn, "cpus", &buf)) {
			kfree(dom);
			return -EINVAL;
		}
		else
			cpulist_parse(buf, &dom->cpus);

		if (of_property_read_u32(dom_dn, "upper-boundary",
						&dom->upper_ratio))
			dom->upper_ratio = -1;

		if (of_property_read_u32(dom_dn, "lower-boundary",
						&dom->lower_ratio))
			dom->lower_ratio = -1;

		/*
		 * Init boundary as default.
		 * Actual boundary value is set in cpufreq policy notifier.
		 */
		dom->upper_boundary = 1024;
		dom->lower_boundary = 0;

		list_add_tail(&dom->node, dom_list);
	}

	return 0;
}

static void parse_ontime(struct device_node *dn, struct emstune_ontime *ontime)
{
	parse_cgroup_field(dn, ontime->enabled, 0);

	INIT_LIST_HEAD(&ontime->dom_list);
	init_dom_list(dn, &ontime->dom_list);

	ontime->p_dom_list = &ontime->dom_list;
}

/* cpus binding */
static void parse_cpus_binding(struct device_node *dn,
			struct emstune_cpus_binding *cpus_binding)
{
	if (of_property_read_u32(dn, "target-sched-class",
			(unsigned int *)&cpus_binding->target_sched_class))
		cpus_binding->target_sched_class = 0;

	parse_cgroup_field_mask(dn, cpus_binding->mask);
}

/* task express */
static void parse_tex(struct device_node *dn, struct emstune_tex *tex)
{
	const char *buf;

	parse_cgroup_field(dn, tex->enabled, 0);

	if (!of_property_read_string(dn, "pinning-mask", &buf))
		cpulist_parse(buf, &tex->mask);
	else
		cpumask_clear(&tex->mask);

	of_property_read_u32(dn, "qjump", &tex->qjump);

	of_property_read_u32(dn, "prio", &tex->prio);

	of_property_read_u32(dn, "suppress", &tex->suppress);
}

/* Fluid RT scheduling */
static void parse_frt(struct device_node *dn, struct emstune_frt *frt)
{
	parse_coregroup_field(dn, "active-ratio", frt->active_ratio, 100);
}

/* CPU Sparing */
static void update_ecs_threshold_ratio(struct list_head *domain_list,
					int id, int ratio)
{
	struct ecs_domain *domain;

	list_for_each_entry(domain, domain_list, node)
		if (domain->id == id)
			domain->busy_threshold_ratio = ratio;
}

static int init_ecs_domain_list(struct device_node *ecs_dn,
		struct list_head *domain_list)
{
	struct device_node *domain_dn;
	int id = 0;

	INIT_LIST_HEAD(domain_list);

	for_each_child_of_node(ecs_dn, domain_dn) {
		struct ecs_domain *domain;

		domain = kzalloc(sizeof(struct ecs_domain), GFP_KERNEL);
		if (!domain)
			return -ENOMEM;

		if (of_property_read_u32(domain_dn, "busy-threshold-ratio",
					(u32 *)&domain->busy_threshold_ratio))
			domain->busy_threshold_ratio = 100;

		domain->id = id++;
		list_add_tail(&domain->node, domain_list);
	}

	return 0;
}

static void parse_ecs(struct device_node *dn, struct emstune_ecs *ecs)
{
	INIT_LIST_HEAD(&ecs->domain_list);
	init_ecs_domain_list(dn, &ecs->domain_list);
}

/* prefer cpu */
static void parse_prefer_cpu(struct device_node *dn,
			struct emstune_prefer_cpu *prefer_cpu)
{
	const char *buf;

	parse_cgroup_field_mask(dn, prefer_cpu->mask);

	if (of_property_read_u32(dn, "small-task-threshold",
			(u32 *)&prefer_cpu->small_task_threshold))
		prefer_cpu->small_task_threshold = 0;

	if (!of_property_read_string(dn, "small-task-mask", &buf))
		cpulist_parse(buf, &prefer_cpu->small_task_mask);
	else {
		/* if it failed to parse mask, set mask all */
		cpumask_copy(&prefer_cpu->small_task_mask, cpu_possible_mask);
	}
}

/* Margin applied to newly created task's utilization */
static void parse_ntu(struct device_node *dn,
				struct emstune_ntu *ntu)
{
	parse_cgroup_field(dn, ntu->ratio, 100);
}

/* fclamp */
static void parse_fclamp(struct device_node *dn, struct emstune_fclamp *fclamp)
{
	parse_coregroup_field(dn, "min-freq", fclamp->min_freq, 0);
	parse_coregroup_field(dn, "min-target-period", fclamp->min_target_period, 0);
	parse_coregroup_field(dn, "min-target-ratio", fclamp->min_target_ratio, 0);

	parse_coregroup_field(dn, "max-freq", fclamp->max_freq, 0);
	parse_coregroup_field(dn, "max-target-period", fclamp->max_target_period, 0);
	parse_coregroup_field(dn, "max-target-ratio", fclamp->max_target_ratio, 0);

	parse_cgroup_field(of_get_child_by_name(dn, "monitor-group"),
						fclamp->monitor_group, 0);
}

/* sync */
static void parse_sync(struct device_node *dn, struct emstune_sync *sync)
{
	if (of_property_read_u32(dn, "apply", &sync->apply))
		sync->apply = 1;
}

/******************************************************************************
 * emstune sched boost                                                        *
 ******************************************************************************/
static int sched_boost;

int emstune_sched_boost(void)
{
	return sched_boost;
}

/******************************************************************************
 * sysfs for emstune                                                          *
 ******************************************************************************/
static ssize_t sched_boost_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sched_boost);
}

static ssize_t sched_boost_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int boost;

	if (sscanf(buf, "%d", &boost) != 1)
		return -EINVAL;

	/* ignore if requested mode is out of range */
	if (boost < 0 || boost >= 100)
		return -EINVAL;

	sched_boost = boost;

	return count;
}
DEVICE_ATTR_RW(sched_boost);

static int task_boost;

int emstune_task_boost(void)
{
	return task_boost;
}

static ssize_t task_boost_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", task_boost);
}

static ssize_t task_boost_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int boost;

	if (sscanf(buf, "%d", &boost) != 1)
		return -EINVAL;

	/* ignore if requested mode is out of range */
	if (boost < 0 || boost >= 32768)
		return -EINVAL;

	task_boost = boost;

	return count;
}
DEVICE_ATTR_RW(task_boost);

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, mode, level;
	unsigned long flags;

	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "mode%d : %s\n",
				mode, emstune.modes[mode].desc);

	ret += sprintf(buf + ret, "\n");
	spin_lock_irqsave(&emstune_lock, flags);

	ret += sprintf(buf + ret, "┌─────");
	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "┬──────");
	ret += sprintf(buf + ret, "┐\n");

	ret += sprintf(buf + ret, "│     ");
	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "│ mode%d", mode);
	ret += sprintf(buf + ret, "│\n");

	ret += sprintf(buf + ret, "├─────");
	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "┼──────");
	ret += sprintf(buf + ret, "┤\n");

	for (level = 0; level < emstune.level_count; level++) {
		ret += sprintf(buf + ret, "│ lv%d ", level);
		for (mode = 0; mode < emstune.mode_count; mode++) {
			if (mode == emstune.cur_mode &&
					level == emstune.cur_level)
				ret += sprintf(buf + ret, "│  curr");
			else
				ret += sprintf(buf + ret, "│      ");
		}
		ret += sprintf(buf + ret, "│\n");

		if (level == emstune.level_count - 1)
			break;

		ret += sprintf(buf + ret, "├-----");
		for (mode = 0; mode < emstune.mode_count; mode++)
			ret += sprintf(buf + ret, "┼------");
		ret += sprintf(buf + ret, "┤\n");
	}

	ret += sprintf(buf + ret, "└─────");
	for (mode = 0; mode < emstune.mode_count; mode++)
		ret += sprintf(buf + ret, "┴──────");
	ret += sprintf(buf + ret, "┘\n");

	ret += sprintf(buf + ret, "\n");
	spin_unlock_irqrestore(&emstune_lock, flags);

	return ret;
}
DEVICE_ATTR_RO(status);

static ssize_t req_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);
	ret = sprintf(buf, "%d\n", emstune.cur_mode);
	spin_unlock_irqrestore(&emstune_lock, flags);

	return ret;
}

static ssize_t req_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int mode;

	if (sscanf(buf, "%d", &mode) != 1)
		return -EINVAL;

	/* ignore if requested mode is out of range */
	if (mode < 0 || mode >= emstune.mode_count)
		return -EINVAL;

	emstune_mode_change(mode);

	return count;
}
DEVICE_ATTR_RW(req_mode);

static ssize_t req_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);
	ret = sprintf(buf, "%d\n", emstune.cur_level);
	spin_unlock_irqrestore(&emstune_lock, flags);

	return ret;
}

static ssize_t req_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int level;

	if (sscanf(buf, "%d", &level) != 1)
		return -EINVAL;

	if (level < 0 || level >= emstune.level_count)
		return -EINVAL;

	emstune_level_change(level);

	return count;
}
DEVICE_ATTR_RW(req_level);

static ssize_t reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "usage\n# <mode> <level> > reset\n");
}

static ssize_t reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int mode, level;

	if (sscanf(buf, "%d %d", &mode, &level) != 2)
		return -EINVAL;

	if (mode < 0 || mode >= emstune.mode_count ||
	    level < 0 || level >= emstune.level_count)
		return -EINVAL;

	emstune_reset(mode, level);

	return count;
}
DEVICE_ATTR_RW(reset);

enum {
	sched_policy,
	active_weight,
	idle_weight,
	freqboost,
	wakeboost,
	esg_step,
	cpufreq_gov_pelt_margin,
	cpufreq_gov_pelt_boost,
	cpufreq_gov_patient_mode,
	cpufreq_gov_rate_limit,
	cpufreq_gov_rapid_scale,
	ontime_en,
	ontime_boundary,
	cpus_binding_tsc,
	cpus_binding_cpumask,
	tex_en,
	tex_mask,
	tex_qjump,
	tex_prio,
	frt_active_ratio,
	ecs_threshold_ratio,
	prefer_cpu,
	ntu_ratio,
	stt_htask_boost_en,
	stt_htask_cnt,
	stt_htask_boost_step,
	fclamp_freq,
	fclamp_period,
	fclamp_ratio,
	fclamp_monitor_group,
	cpufreq_gov_pelt_margin_freq,
	cpufreq_gov_rate_limit_freq,
	cpufreq_gov_dis_buck_share,
	prefer_cpu_small_task_threshold,
	prefer_cpu_small_task_mask,
	sync,
	tex_suppress,
	field_count,
};

static ssize_t aio_tuner_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "echo <func,mode,level,...> <value> > aio_tuner\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] sched policy\n", sched_policy);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,group> <policy> > aio_tuner\n", sched_policy);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] active weight\n", active_weight);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu,group> <ratio> > aio_tuner\n", active_weight);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] idle weight\n", idle_weight);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu,group> <ratio> > aio_tuner\n", idle_weight);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] freq boost\n", freqboost);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu,group> <ratio> > aio_tuner\n", freqboost);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] wakeup boost\n", wakeboost);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu,group> <ratio> > aio_tuner\n", wakeboost);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "<--- skip --->\n");
	ret += sprintf(buf + ret, "[%d] cpufreq gov - pelt margin\n", cpufreq_gov_pelt_margin);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu> <margin> > aio_tuner\n",
				  cpufreq_gov_pelt_margin);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] cpufreq gov - pelt boost\n", cpufreq_gov_pelt_boost);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu> <ratio> > aio_tuner\n", cpufreq_gov_pelt_boost);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] cpufreq gov - patient_mode\n", cpufreq_gov_patient_mode);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu> <tick_count> > aio_tuner\n",
				  cpufreq_gov_patient_mode);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] cpufreq gov - rate limit\n", cpufreq_gov_rate_limit);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu> <ms> > aio_tuner\n", cpufreq_gov_rate_limit);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] cpufreq gov - rapid scale\n", cpufreq_gov_rapid_scale);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,up/down> <en> > aio_tuner\n", cpufreq_gov_rapid_scale);
	ret += sprintf(buf + ret, "	(up=0 down=1)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] ontime - enable\n", ontime_en);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,group> <en> > aio_tuner\n", ontime_en);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] ontime - boundary\n", ontime_boundary);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu,up/lo> <boundary> > aio_tuner\n", ontime_boundary);
	ret += sprintf(buf + ret, "     (up=0 lo=1)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] cpus_binding - target sched class\n", cpus_binding_tsc);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <mask> > aio_tuner\n", cpus_binding_tsc);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] cpus_binding - cpumask\n", cpus_binding_cpumask);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,group> <mask> > aio_tuner\n", cpus_binding_cpumask);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] tex - enable\n", tex_en);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,group> <enable> > aio_tuner\n", tex_en);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] tex - mask\n", tex_mask);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <mask> > aio_tuner\n", tex_mask);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] tex - qjump\n", tex_qjump);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <en> > aio_tuner\n", tex_qjump);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] tex - priority\n", tex_prio);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <prio> > aio_tuner\n", tex_prio);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] frt - active_ratio\n", frt_active_ratio);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu> <ratio> > aio_tuner\n", frt_active_ratio);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] ecs - threshold_ratio\n", ecs_threshold_ratio);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,id> <ratio> > aio_tuner\n", ecs_threshold_ratio);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] prefer cpu - mask\n", prefer_cpu);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,group> <mask> > aio_tuner\n", prefer_cpu);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] new task util - ratio\n", ntu_ratio);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,group> <ratio> > aio_tuner\n", ntu_ratio);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] htask_boost - enable\n", stt_htask_boost_en);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,group> <en> > aio_tuner\n", stt_htask_boost_en);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] htask_boost - htask_cnt\n", stt_htask_cnt);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <cnt> > aio_tuner\n", stt_htask_cnt);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] htask_boost - step\n", stt_htask_boost_step);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <step> > aio_tuner\n", stt_htask_boost_step);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] fclamp - freq\n", fclamp_freq);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu,min/max> <freq> > aio_tuner\n", fclamp_freq);
	ret += sprintf(buf + ret, "     (min=0 max=1)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] fclamp - target period\n", fclamp_period);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu,min/max> <period> > aio_tuner\n", fclamp_period);
	ret += sprintf(buf + ret, "     (min=0 max=1)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] fclamp - target ratio\n", fclamp_ratio);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu,min/max> <ratio> > aio_tuner\n", fclamp_ratio);
	ret += sprintf(buf + ret, "     (min=0 max=1)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] fclamp - monitor group\n", fclamp_monitor_group);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,group> <en> > aio_tuner\n", fclamp_monitor_group);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] cpufreq gov - pelt margin freq\n", cpufreq_gov_pelt_margin_freq);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu> <freq> > aio_tuner\n",
					cpufreq_gov_pelt_margin_freq);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] cpufreq gov - rate limit freq\n", cpufreq_gov_rate_limit_freq);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu> <freq> > aio_tuner\n",
					cpufreq_gov_rate_limit_freq);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] cpufreq gov - dis buck share\n", cpufreq_gov_dis_buck_share);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level,cpu> <val> > aio_tuner\n", cpufreq_gov_dis_buck_share);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] prefer cpu - small task threshold\n", prefer_cpu_small_task_threshold);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <threshold> > aio_tuner\n", prefer_cpu_small_task_threshold);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] prefer cpu - small task mask\n", prefer_cpu_small_task_mask);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <mask> > aio_tuner\n", prefer_cpu_small_task_mask);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] sync - apply\n", sync);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <apply> > aio_tuner\n", sync);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "[%d] tex - suppress enabled\n", tex_suppress);
	ret += sprintf(buf + ret, "	# echo <%d,mode,level> <en> > aio_tuner\n", tex_suppress);
	ret += sprintf(buf + ret, "\n");

	return ret;
}

#define STR_LEN 10
static int cut_hexa_prefix(char *str)
{
	int i;

	if (!(str[0] == '0' && str[1] == 'x'))
		return -EINVAL;

	for (i = 0; i + 2 < STR_LEN; i++) {
		str[i] = str[i + 2];
		str[i + 2] = '\n';
	}

	return 0;
}

static int sanity_check_default(int field, int mode, int level)
{
	if (field < 0 || field >= field_count)
		return -EINVAL;
	if (mode < 0 || mode >= emstune.mode_count)
		return -EINVAL;
	if (level < 0 || level >= emstune.level_count)
		return -EINVAL;

	return 0;
}

static int sanity_check_option(int cpu, int group, int type)
{
	if (cpu < 0 || cpu > NR_CPUS)
		return -EINVAL;
	if (group < 0)
		return -EINVAL;
	if (!(type == 0 || type == 1))
		return -EINVAL;

	return 0;
}

enum val_type {
	VAL_TYPE_ONOFF,
	VAL_TYPE_RATIO,
	VAL_TYPE_RATIO_NEGATIVE,
	VAL_TYPE_LEVEL,
	VAL_TYPE_CPU,
	VAL_TYPE_MASK,
};

static int
sanity_check_convert_value(char *arg, enum val_type type, int limit, void *v)
{
	int ret = 0;
	long value;

	switch (type) {
	case VAL_TYPE_ONOFF:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		*(int *)v = !!(int)value;
		break;
	case VAL_TYPE_RATIO_NEGATIVE:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		if (value < -limit || value > limit)
			return -EINVAL;
		*(int *)v = (int)value;
		break;
	case VAL_TYPE_RATIO:
	case VAL_TYPE_LEVEL:
	case VAL_TYPE_CPU:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		if (value < 0 || value > limit)
			return -EINVAL;
		*(int *)v = (int)value;
		break;
	case VAL_TYPE_MASK:
		if (cut_hexa_prefix(arg))
			return -EINVAL;
		if (cpumask_parse(arg, (struct cpumask *)v) ||
		    cpumask_empty((struct cpumask *)v))
			return -EINVAL;
		break;
	}

	return ret;
}

static void
set_value_single(int *field, char *v, enum val_type type, int limit)
{
	int value;

	if (sanity_check_convert_value(v, VAL_TYPE_LEVEL, 1000, &value))
		return;

	*field = value;
}

static void
set_value_coregroup(int (field)[VENDOR_NR_CPUS], int cpu, char *v,
				enum val_type type, int limit)
{
	int i, value;

	if (sanity_check_option(cpu, 0, 0))
		return;

	if (sanity_check_convert_value(v, type, limit, &value))
		return;

	for_each_cpu(i, cpu_coregroup_mask(cpu))
		field[i] = value;
}

static void
set_value_cgroup(int (field)[CGROUP_COUNT], int group, char *v,
				enum val_type type, int limit)
{
	int value;

	if (sanity_check_option(0, group, 0))
		return;

	if (sanity_check_convert_value(v, type, limit, &value))
		return;

	if (group >= CGROUP_COUNT) {
		for (group = 0; group < CGROUP_COUNT; group++)
			field[group] = value;
		return;
	}

	field[group] = value;
}

static void
set_value_cgroup_coregroup(int (field)[CGROUP_COUNT][VENDOR_NR_CPUS],
				int cpu, int group, char *v,
				enum val_type type, int limit)
{
	int i, value;

	if (sanity_check_option(cpu, group, 0))
		return;

	if (sanity_check_convert_value(v, type, limit, &value))
		return;

	if (group >= CGROUP_COUNT) {
		for (group = 0; group < CGROUP_COUNT; group++)
			for_each_cpu(i, cpu_coregroup_mask(cpu))
				field[group][i] = value;
		return;
	}

	for_each_cpu(i, cpu_coregroup_mask(cpu))
		field[group][i] = value;
}

#define NUM_OF_KEY	(10)

#define MAX_RATIO		10000 /* 10000% */
#define MAX_ESG_STEP		20
#define MAX_PATIENT_TICK	100
#define MAX_RATE_LIMIT		1000 /* 1000ms */

static ssize_t aio_tuner_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct emstune_set *set;
	char arg0[100], arg1[100], *_arg, *ptr;
	int keys[NUM_OF_KEY], i, ret, v;
	long val;
	int mode, level, field, cpu, group, type;
	struct cpumask mask;

	if (sscanf(buf, "%99s %99s", &arg0, &arg1) != 2)
		return -EINVAL;

	/* fill keys with default value(-1) */
	for (i = 0; i < NUM_OF_KEY; i++)
		keys[i] = -1;

	/* classify key input value by comma(,) */
	_arg = arg0;
	ptr = strsep(&_arg, ",");
	i = 0;
	while (ptr != NULL) {
		ret = kstrtol(ptr, 10, &val);
		if (ret)
			return ret;

		keys[i] = (int)val;
		i++;

		ptr = strsep(&_arg, ",");
	};

	field = keys[0];
	mode = keys[1];
	level = keys[2];

	if (sanity_check_default(field, mode, level))
		return -EINVAL;

	set = &emstune.modes[mode].sets[level];

	switch (field) {
	case sched_policy:
		set_value_cgroup(set->sched_policy.policy, keys[3], arg1,
					VAL_TYPE_LEVEL, NUM_OF_SCHED_POLICY);
		break;
	case active_weight:
		set_value_cgroup_coregroup(set->active_weight.ratio,
					keys[3], keys[4], arg1,
					VAL_TYPE_RATIO, MAX_RATIO);
		break;
	case idle_weight:
		set_value_cgroup_coregroup(set->idle_weight.ratio,
					keys[3], keys[4], arg1,
					VAL_TYPE_RATIO, MAX_RATIO);
		break;
	case freqboost:
		set_value_cgroup_coregroup(set->freqboost.ratio,
					keys[3], keys[4], arg1,
					VAL_TYPE_RATIO_NEGATIVE, MAX_RATIO);
		break;
	case wakeboost:
		set_value_cgroup_coregroup(set->wakeboost.ratio,
					keys[3], keys[4], arg1,
					VAL_TYPE_RATIO_NEGATIVE, MAX_RATIO);
		break;
	case esg_step:
		set_value_coregroup(set->esg.step, keys[3], arg1,
					VAL_TYPE_LEVEL, MAX_ESG_STEP);
		break;
	case cpufreq_gov_pelt_margin:
		set_value_coregroup(set->cpufreq_gov.split_pelt_margin, keys[3], arg1,
					VAL_TYPE_RATIO_NEGATIVE, MAX_RATIO);
		break;
	case cpufreq_gov_patient_mode:
		set_value_coregroup(set->cpufreq_gov.patient_mode, keys[3], arg1,
					VAL_TYPE_LEVEL, MAX_PATIENT_TICK);
		break;
	case cpufreq_gov_pelt_boost:
		set_value_coregroup(set->cpufreq_gov.pelt_boost, keys[3], arg1,
					VAL_TYPE_RATIO_NEGATIVE, MAX_RATIO);
		break;
	case cpufreq_gov_rate_limit:
		set_value_coregroup(set->cpufreq_gov.split_up_rate_limit, keys[3], arg1,
					VAL_TYPE_LEVEL, INT_MAX);
		break;
	case cpufreq_gov_rapid_scale:
		type = !!keys[3];
		if (type)
			set_value_single(&set->cpufreq_gov.rapid_scale_down, arg1,
					VAL_TYPE_ONOFF, 0);
		else
			set_value_single(&set->cpufreq_gov.rapid_scale_up, arg1,
					VAL_TYPE_ONOFF, 0);
		break;
	case ontime_en:
		set_value_cgroup(set->ontime.enabled, keys[3], arg1,
					VAL_TYPE_ONOFF, 0);
		break;
	case ontime_boundary:
		cpu = keys[3];
		type = !!keys[4];
		if (sanity_check_option(cpu, 0, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_LEVEL, 1024, &v))
			return -EINVAL;
		update_ontime_domain(&set->ontime.dom_list, cpu, v, type);
		break;
	case cpus_binding_tsc:
		if (kstrtol(arg1, 0, &val))
			return -EINVAL;
		val &= EMS_SCHED_CLASS_MASK;
		set->cpus_binding.target_sched_class = 0;
		for (i = 0; i < NUM_OF_SCHED_CLASS; i++)
			if (test_bit(i, &val))
				set_bit(i, &set->cpus_binding.target_sched_class);
		break;
	case cpus_binding_cpumask:
		group = keys[3];
		if (sanity_check_option(0, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_MASK, 0, &mask))
			return -EINVAL;
		cpumask_copy(&set->cpus_binding.mask[group], &mask);
		break;
	case tex_en:
		set_value_cgroup(set->tex.enabled, keys[3], arg1,
					VAL_TYPE_ONOFF, 0);
		break;
	case tex_mask:
		if (sanity_check_convert_value(arg1, VAL_TYPE_MASK, 0, &mask))
			return -EINVAL;
		cpumask_copy(&set->tex.mask, &mask);
		break;
	case tex_qjump:
		set_value_single(&set->tex.qjump, arg1,
					VAL_TYPE_ONOFF, 1);
		break;
	case tex_prio:
		set_value_single(&set->tex.prio, arg1,
					VAL_TYPE_LEVEL, MAX_PRIO);
		break;
	case frt_active_ratio:
		set_value_coregroup(set->frt.active_ratio, keys[3], arg1,
					VAL_TYPE_RATIO, MAX_RATIO);
		break;
	case ecs_threshold_ratio:
		i = keys[3];
		if (sanity_check_convert_value(arg1, VAL_TYPE_LEVEL, 10000, &v))
			return -EINVAL;
		update_ecs_threshold_ratio(&set->ecs.domain_list, i, v);
		break;
	case prefer_cpu:
		group = keys[3];
		if (sanity_check_option(0, group, 0))
			return -EINVAL;
		if (sanity_check_convert_value(arg1, VAL_TYPE_MASK, 0, &mask))
			return -EINVAL;
		cpumask_copy(&set->prefer_cpu.mask[group], &mask);
		break;
	case ntu_ratio:
		set_value_cgroup(set->ntu.ratio, keys[3], arg1,
					VAL_TYPE_RATIO_NEGATIVE, 100);
		break;
	case stt_htask_boost_en:
		set_value_cgroup(set->stt.htask_enable, keys[3], arg1,
					VAL_TYPE_ONOFF, 0);
		break;
	case stt_htask_cnt:
		set_value_single(&set->stt.htask_cnt, arg1,
					VAL_TYPE_RATIO, 100);
		break;
	case stt_htask_boost_step:
		set_value_single(&set->stt.boost_step, arg1,
					VAL_TYPE_RATIO, 100);
		break;
	case fclamp_freq:
		type = !!keys[4];
		if (type)
			set_value_coregroup(set->fclamp.max_freq, keys[3],
					arg1, VAL_TYPE_LEVEL, 10000000);
		else
			set_value_coregroup(set->fclamp.min_freq, keys[3],
					arg1, VAL_TYPE_LEVEL, 10000000);
		break;
	case fclamp_period:
		type = !!keys[4];
		if (type)
			set_value_coregroup(set->fclamp.max_target_period,
					keys[3], arg1, VAL_TYPE_LEVEL, 100);
		else
			set_value_coregroup(set->fclamp.min_target_period,
					keys[3], arg1, VAL_TYPE_LEVEL, 100);
		break;
	case fclamp_ratio:
		type = !!keys[4];
		if (type)
			set_value_coregroup(set->fclamp.max_target_ratio,
					keys[3], arg1, VAL_TYPE_LEVEL, 1000);
		else
			set_value_coregroup(set->fclamp.min_target_ratio,
					keys[3], arg1, VAL_TYPE_LEVEL, 1000);
		break;
	case fclamp_monitor_group:
		set_value_cgroup(set->fclamp.monitor_group, keys[3], arg1,
					VAL_TYPE_ONOFF, 0);
		break;
	case cpufreq_gov_pelt_margin_freq:
		set_value_coregroup(set->cpufreq_gov.split_pelt_margin_freq, keys[3], arg1,
					VAL_TYPE_LEVEL, INT_MAX);
		break;
	case cpufreq_gov_rate_limit_freq:
		set_value_coregroup(set->cpufreq_gov.split_up_rate_limit_freq, keys[3], arg1,
					VAL_TYPE_LEVEL, INT_MAX);
		break;
	case cpufreq_gov_dis_buck_share:
		set_value_coregroup(set->cpufreq_gov.dis_buck_share, keys[3], arg1,
					VAL_TYPE_LEVEL, 100);
		break;
	case prefer_cpu_small_task_threshold:
		if (sanity_check_convert_value(arg1, VAL_TYPE_LEVEL, 1024, &v))
			return -EINVAL;
		set->prefer_cpu.small_task_threshold = v;
		break;
	case prefer_cpu_small_task_mask:
		if (sanity_check_convert_value(arg1, VAL_TYPE_MASK, 0, &mask))
			return -EINVAL;
		cpumask_copy(&set->prefer_cpu.small_task_mask, &mask);
		break;
	case sync:
		set_value_single(&set->sync.apply, arg1, VAL_TYPE_ONOFF, 1);
		break;
	case tex_suppress:
		set_value_single(&set->tex.suppress, arg1, VAL_TYPE_ONOFF, 1);
		break;
	}

	emstune_notify();

	return count;
}
DEVICE_ATTR_RW(aio_tuner);

static char *task_cgroup_simple_name[] = {
	"root",
	"fg",
	"bg",
	"ta",
	"rt",
	"sy",
	"syb",
	"n-h",
	"c-d",
};

static const char *get_sched_class_str(int class)
{
	if (class & EMS_SCHED_STOP)
		return "STOP";
	if (class & EMS_SCHED_DL)
		return "DL";
	if (class & EMS_SCHED_RT)
		return "RT";
	if (class & EMS_SCHED_FAIR)
		return "FAIR";
	if (class & EMS_SCHED_IDLE)
		return "IDLE";

	return NULL;
}

static int get_type_pending_bit(int type)
{
	int bit = 0;

	type >>= 1;
	while (type) {
		type >>= 1;
		bit++;
	}

	return bit;
}

#define MSG_SIZE 8192
static ssize_t cur_set_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t size)
{
	struct emstune_set *cur_set = emstune_cur_set();
	int cpu, group, class;
	char msg[MSG_SIZE];
	ssize_t count = 0, msg_size;

	/* mode/level info */
	count += sprintf(msg + count, "%s mode : level%d [mode=%d level=%d]\n",
				emstune.modes[emstune.cur_mode].desc,
				emstune.cur_level,
				get_type_pending_bit(EMSTUNE_MODE_TYPE(cur_set->type)),
				get_type_pending_bit(EMSTUNE_BOOST_TYPE(cur_set->type)));
	count += sprintf(msg + count, "\n");

	/* sched policy */
	count += sprintf(msg + count, "[sched-policy]\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->sched_policy.policy[group]);
	count += sprintf(msg + count, "\n\n");

	/* active weight */
	count += sprintf(msg + count, "[active-weight]\n");
	count += sprintf(msg + count, "     ");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d:", cpu);
		for (group = 0; group < CGROUP_COUNT; group++)
			count += sprintf(msg + count, "%4d",
					cur_set->active_weight.ratio[group][cpu]);
		count += sprintf(msg + count, "\n");
	}
	count += sprintf(msg + count, "\n");

	/* idle weight */
	count += sprintf(msg + count, "[idle-weight]\n");
	count += sprintf(msg + count, "     ");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d:", cpu);
		for (group = 0; group < CGROUP_COUNT; group++)
			count += sprintf(msg + count, "%4d",
					cur_set->idle_weight.ratio[group][cpu]);
		count += sprintf(msg + count, "\n");
	}
	count += sprintf(msg + count, "\n");

	/* freq boost */
	count += sprintf(msg + count, "[freq-boost]\n");
	count += sprintf(msg + count, "     ");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d:", cpu);
		for (group = 0; group < CGROUP_COUNT; group++)
			count += sprintf(msg + count, "%4d",
					cur_set->freqboost.ratio[group][cpu]);
		count += sprintf(msg + count, "\n");
	}
	count += sprintf(msg + count, "\n");

	/* wakeup boost */
	count += sprintf(msg + count, "[wakeup-boost]\n");
	count += sprintf(msg + count, "     ");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d:", cpu);
		for (group = 0; group < CGROUP_COUNT; group++)
			count += sprintf(msg + count, "%4d",
					cur_set->wakeboost.ratio[group][cpu]);
		count += sprintf(msg + count, "\n");
	}
	count += sprintf(msg + count, "\n");

	/* cpufreq gov */
	count += sprintf(msg + count, "[cpufreq gov]\n");
	count += sprintf(msg + count, "pelt boost\n");
	count += sprintf(msg + count, "      dbs  boost\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d:%4d %6d\n", cpu,
				cur_set->cpufreq_gov.dis_buck_share[cpu],
				cur_set->cpufreq_gov.pelt_boost[cpu]);
	}
	count += sprintf(msg + count, "\n");
	count += sprintf(msg + count, "pelt margin\n");
	count += sprintf(msg + count, "     margin    freq\n");
	for_each_possible_cpu(cpu) {
		if (cur_set->cpufreq_gov.split_pelt_margin_freq[cpu] == INT_MAX)
			count += sprintf(msg + count, "cpu%d:  %4d %7s\n",
				cpu, cur_set->cpufreq_gov.split_pelt_margin[cpu],
				"INF");
		else
			count += sprintf(msg + count, "cpu%d:%4d %7u\n",
				cpu, cur_set->cpufreq_gov.split_pelt_margin[cpu],
				cur_set->cpufreq_gov.split_pelt_margin_freq[cpu]);
	}
	count += sprintf(msg + count, "\n");
	count += sprintf(msg + count, "up rate limit\n");
	count += sprintf(msg + count, "     rate    freq\n");
	for_each_possible_cpu(cpu) {
		if (cur_set->cpufreq_gov.split_up_rate_limit_freq[cpu] == INT_MAX)
			count += sprintf(msg + count, "cpu%d:%4d %7s\n",
				cpu, cur_set->cpufreq_gov.split_up_rate_limit[cpu],
				"INF");
		else
			count += sprintf(msg + count, "cpu%d:%4d %7u\n",
				cpu, cur_set->cpufreq_gov.split_up_rate_limit[cpu],
				cur_set->cpufreq_gov.split_up_rate_limit_freq[cpu]);
	}
	count += sprintf(msg + count, "\n");

	/* ontime */
	count += sprintf(msg + count, "[ontime]\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->ontime.enabled[group]);
	count += sprintf(msg + count, "\n\n");
	{
		struct ontime_dom *dom;

		count += sprintf(msg + count, "-----------------------------\n");
		if (!list_empty(cur_set->ontime.p_dom_list)) {
			list_for_each_entry(dom, cur_set->ontime.p_dom_list, node) {
				count += sprintf(msg + count, " cpus           :  %*pbl\n",
					cpumask_pr_args(&dom->cpus));
				count += sprintf(msg + count, " upper boundary : %4lu (%3d%%)\n",
					dom->upper_boundary, dom->upper_ratio);
				count += sprintf(msg + count, " lower boundary : %4lu (%3d%%)\n",
					dom->lower_boundary, dom->lower_ratio);
				count += sprintf(msg + count, "-----------------------------\n");
			}
		} else  {
			count += sprintf(msg + count, "list empty!\n");
			count += sprintf(msg + count, "-----------------------------\n");
		}
	}
	count += sprintf(msg + count, "\n");

	/* cpus binding */
	count += sprintf(msg + count, "[cpus binding]\n");
	for (class = 0; class < NUM_OF_SCHED_CLASS; class++)
		count += sprintf(msg + count, "%5s", get_sched_class_str(1 << class));
	count += sprintf(msg + count, "\n");
	for (class = 0; class < NUM_OF_SCHED_CLASS; class++)
		count += sprintf(msg + count, "%5d",
			(cur_set->cpus_binding.target_sched_class & (1 << class) ? 1 : 0));
	count += sprintf(msg + count, "\n\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4x",
			*(unsigned int *)cpumask_bits(&cur_set->cpus_binding.mask[group]));
	count += sprintf(msg + count, "\n\n");

	/* tex */
	count += sprintf(msg + count, "[tex]\n");
	count += sprintf(msg + count, "mask : %#x\n",
			*(unsigned int *)cpumask_bits(&cur_set->tex.mask));
	count += sprintf(msg + count, "qjump: %d\n", cur_set->tex.qjump);
	count += sprintf(msg + count, "prio : %d\n", cur_set->tex.prio);
	count += sprintf(msg + count, "suppress : %d\n", cur_set->tex.suppress);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->tex.enabled[group]);
	count += sprintf(msg + count, "\n\n");

	/* frt */
	count += sprintf(msg + count, "[frt]\n");
	count += sprintf(msg + count, "     active\n");
	for_each_possible_cpu(cpu)
		count += sprintf(msg + count, "cpu%d: %4d%%\n",
				cpu,
				cur_set->frt.active_ratio[cpu]);
	count += sprintf(msg + count, "\n");

	/* ecs */
	count += sprintf(msg + count, "[ecs]\n");
	{
		struct ecs_domain *domain;

		count += sprintf(msg + count, "-------------------------\n");
		if (!list_empty(&cur_set->ecs.domain_list)) {
			list_for_each_entry(domain, &cur_set->ecs.domain_list, node) {
				count += sprintf(msg + count, " domain id       : %d\n",
					domain->id);
				count += sprintf(msg + count, " threshold ratio : %u\n",
					domain->busy_threshold_ratio);
				count += sprintf(msg + count, "-------------------------\n");
			}
		} else  {
			count += sprintf(msg + count, "list empty!\n");
			count += sprintf(msg + count, "-------------------------\n");
		}
	}
	count += sprintf(msg + count, "\n");

	/* prefer cpu */
	count += sprintf(msg + count, "[prefer cpu]\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4x",
			*(unsigned int *)cpumask_bits(&cur_set->prefer_cpu.mask[group]));
	count += sprintf(msg + count, "\n\n");
	count += sprintf(msg + count, "small task threshold : %lu\n",
				cur_set->prefer_cpu.small_task_threshold);
	count += sprintf(msg + count, "small task mask : %#x\n",
		*(unsigned int *)cpumask_bits(&cur_set->prefer_cpu.small_task_mask));
	count += sprintf(msg + count, "\n");

	/* new task util ratio */
	count += sprintf(msg + count, "[new task util ratio]\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->ntu.ratio[group]);
	count += sprintf(msg + count, "\n\n");

	/* htask boost */
	count += sprintf(msg + count, "[htask_boost]\n");
	count += sprintf(msg + count, "boost_step: %d\n", cur_set->stt.boost_step);
	count += sprintf(msg + count, "htask_cnt: %d\n", cur_set->stt.htask_cnt);
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->stt.htask_enable[group]);
	count += sprintf(msg + count, "\n\n");

	/* fclamp */
	count += sprintf(msg + count, "[fclamp]\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[group]);
	count += sprintf(msg + count, "\n");
	for (group = 0; group < CGROUP_COUNT; group++)
		count += sprintf(msg + count, "%4d", cur_set->fclamp.monitor_group[group]);
	count += sprintf(msg + count, "\n\n");
	count += sprintf(msg + count, "     |-------- min -------||-------- max -------|\n");
	count += sprintf(msg + count, "        freq  period ratio    freq  period ratio\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d: %7d %6d %5d  %7d %6d %5d\n",
				cpu, cur_set->fclamp.min_freq[cpu],
				cur_set->fclamp.min_target_period[cpu],
				cur_set->fclamp.min_target_ratio[cpu],
				cur_set->fclamp.max_freq[cpu],
				cur_set->fclamp.max_target_period[cpu],
				cur_set->fclamp.max_target_ratio[cpu]);
	}
	count += sprintf(msg + count, "\n");

	msg_size = min_t(ssize_t, count, MSG_SIZE);
	msg_size = memory_read_from_buffer(buf, size, &offset, msg, msg_size);

	return msg_size;
}
static BIN_ATTR(cur_set, 0440, cur_set_read, NULL, 0);

static struct attribute *emstune_attrs[] = {
	&dev_attr_sched_boost.attr,
	&dev_attr_task_boost.attr,
	&dev_attr_status.attr,
	&dev_attr_req_mode.attr,
	&dev_attr_req_level.attr,
	&dev_attr_reset.attr,
	&dev_attr_aio_tuner.attr,
	NULL,
};

static struct bin_attribute *emstune_bin_attrs[] = {
	&bin_attr_cur_set,
	NULL,
};

static struct attribute_group emstune_attr_group = {
	.name		= "emstune",
	.attrs		= emstune_attrs,
	.bin_attrs	= emstune_bin_attrs,
};

/******************************************************************************
 * initialization                                                             *
 ******************************************************************************/
#define parse(_name)							\
	child = of_get_child_by_name(set_node, #_name);			\
	if (!child)							\
		child = of_get_child_by_name(base_node, #_name);	\
	parse_##_name(child, &set->_name);				\
	of_node_put(child)

static int emstune_mode_backup(void)
{
	int i;

	emstune.modes_backup = kzalloc(sizeof(struct emstune_mode)
					* emstune.mode_count, GFP_KERNEL);
	if (!emstune.modes_backup)
		return -ENOMEM;

	/* backup modes */
	memcpy(emstune.modes_backup, emstune.modes,
		sizeof(struct emstune_mode) * emstune.mode_count);

	for (i = 0; i < emstune.mode_count; i++) {
		emstune.modes_backup[i].sets =
				kzalloc(sizeof(struct emstune_set)
				* emstune.level_count, GFP_KERNEL);
		if (!emstune.modes_backup[i].sets)
			return -ENOMEM;

		/* backup sets */
		memcpy(emstune.modes_backup[i].sets, emstune.modes[i].sets,
			sizeof(struct emstune_set) * emstune.level_count);
	}

	return 0;
}

static void
emstune_set_type_init(struct emstune_set *set, int mode, int level)
{
	/* init set type */
	set->type = 0;
	set->type |= (1 << level);
	set->type |= (1 << (BEGIN_OF_MODE_TYPE + mode));
}

int emstune_get_set_type(void *_set)
{
	struct emstune_set *set = (struct emstune_set *)_set;

	return set->type;
}
EXPORT_SYMBOL_GPL(emstune_get_set_type);

static int emstune_set_init(struct device_node *base_node,
				struct device_node *set_node,
				int mode, int level, struct emstune_set *set)
{
	struct device_node *child;

	emstune_set_type_init(set, mode, level);

	parse(specific_energy_table);
	parse(sched_policy);
	parse(active_weight);
	parse(idle_weight);
	parse(freqboost);
	parse(wakeboost);
	parse(esg);
	parse(cpufreq_gov);
	parse(ontime);
	parse(cpus_binding);
	parse(tex);
	parse(frt);
	parse(ecs);
	parse(prefer_cpu);
	parse(ntu);
	parse(stt);
	parse(fclamp);
	parse(sync);

	return 0;
}

static void emstune_boot_done(struct work_struct *work)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);
	emstune.boot.ongoing = 0;
	emstune_notify();
	spin_unlock_irqrestore(&emstune_lock, flags);
}

static void emstune_boot(void)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_lock, flags);
	emstune.boot.ongoing = 1;
	emstune_notify();
	spin_unlock_irqrestore(&emstune_lock, flags);

	INIT_DELAYED_WORK(&emstune.boot.work, emstune_boot_done);
	schedule_delayed_work(&emstune.boot.work, msecs_to_jiffies(40000));
}

static int emstune_mode_init(struct device_node *dn)
{
	struct device_node *emstune_node, *mode_node, *node;
	int mode_id, level, ret;

	emstune_node = of_get_child_by_name(dn, "emstune");
	if (!emstune_node)
		return -EINVAL;

	/* If boost_level does not exist in dt, emstune.boost_level is 0 */
	of_property_read_u32(emstune_node, "boost-level", &emstune.boost_level);

	emstune.mode_count = of_get_child_count(emstune_node);
	if (!emstune.mode_count)
		return -ENODATA;

	emstune.modes = kzalloc(sizeof(struct emstune_mode)
					* emstune.mode_count, GFP_KERNEL);
	if (!emstune.modes)
		return -ENOMEM;

	mode_id = 0;
	for_each_child_of_node(emstune_node, mode_node) {
		struct device_node *level_node;
		struct emstune_mode *mode = &emstune.modes[mode_id];
		struct emstune_set *sets;
		int level_count;

		if (of_property_read_string(mode_node, "desc", &mode->desc)) {
			ret = -ENODATA;
			goto fail;
		}

		level_count = of_get_child_count(mode_node);
		if (!level_count) {
			ret = -ENODATA;
			goto fail;
		}

		if (!emstune.level_count)
			emstune.level_count = level_count;
		else if (emstune.level_count != level_count) {
			/* the level count of each mode must be same */
			ret = -EINVAL;
			goto fail;
		}

		sets = kzalloc(sizeof(struct emstune_set)
					* emstune.level_count, GFP_KERNEL);
		if (!sets) {
			ret = -ENOMEM;
			goto fail;
		}

		level = 0;
		for_each_child_of_node(mode_node, level_node) {
			struct device_node *base_node =
				of_parse_phandle(level_node, "base", 0);
			struct device_node *set_node =
				of_parse_phandle(level_node, "set", 0);

			if (!base_node || !set_node) {
				ret = -ENODATA;
				goto fail;
			}

			ret = emstune_set_init(base_node, set_node,
						mode_id, level, &sets[level]);

			of_node_put(base_node);
			of_node_put(set_node);

			if (ret)
				goto fail;

			level++;
		}

		mode->sets = sets;
		mode_id++;
	}

	/* backup origin emstune */
	if (emstune_mode_backup())
		goto fail;

	node = of_parse_phandle(emstune_node, "boot-set", 0);
	if (node) {
		emstune_set_init(node, node, 0, 0, &emstune.boot.set);
		emstune_boot();
		of_node_put(node);
	} else
		emstune_mode_change(0);

	return 0;

fail:
	for (mode_id = 0; mode_id < emstune.mode_count; mode_id++) {
		struct emstune_mode *mode = &emstune.modes[mode_id];

		kfree(mode->sets);
		mode->sets = NULL;
	}

	kfree(emstune.modes);
	emstune.modes = NULL;

	return ret;
}

int emstune_init(struct kobject *ems_kobj, struct device_node *dn)
{
	int ret;

	ret = emstune_mode_init(dn);
	if (ret) {
		WARN(1, "failed to initialize emstune with err=%d\n", ret);
		return ret;
	}

	if (sysfs_create_group(ems_kobj, &emstune_attr_group))
		pr_err("failed to initialize emstune sysfs\n");

	return 0;
}
