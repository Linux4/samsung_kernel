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

#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include <dt-bindings/soc/samsung/ems.h>

/******************************************************************************
 * emstune mode/level/set management                                          *
 ******************************************************************************/
struct emstune_mode {
	const char *desc;
	struct emstune_set *sets;
};

struct {
	struct emstune_mode *modes;
	struct emstune_mode *backup;

	int cur_mode;
	int cur_level;

	int mode_count;
	int level_count;

	int boost_level;

	struct {
		bool ongoing;
		struct emstune_set set;
		struct delayed_work work;
	} boot;

	struct device *ems_dev;
	struct notifier_block nb;
	struct emstune_mode_request level_req;

	spinlock_t lock;
} emstune = {
	.lock = __SPIN_LOCK_INITIALIZER(emstune.lock),
};

struct {
	bool enabled;
	int tid;
	struct mutex lock;
} task_boost = {
	.tid = 0,
	.lock = __MUTEX_INITIALIZER(task_boost.lock),
};

enum data_type {
	BOOL_TYPE,
	SIGNED_TYPE,
	UNSIGNED_TYPE,
	CPUMASK_TYPE,
};

enum arg_type {
	CPU_ONLY,
	CGROUP_ONLY,
	CPU_CGROUP,
	CPU_OPTION,
	OPTION_ONLY,
	NOP,
};

static inline bool is_invalid_mode(int mode)
{
	return mode < 0 || mode >= emstune.mode_count;
}

static inline bool is_invalid_level(int level)
{
	return level < 0 || level >= emstune.level_count;
}

static inline bool is_invalid_cgroup(int cgroup)
{
	return cgroup < 0 || cgroup >= CGROUP_COUNT;
}

#define for_each_mode(mode)		for (mode = 0; mode < emstune.mode_count; mode++)
#define for_each_level(level)		for (level = 0; level < emstune.level_count; level++)
#define for_each_cgroup(cgroup)		for (cgroup = 0; cgroup < CGROUP_COUNT ; cgroup++)

static inline struct emstune_set *get_current_set(void)
{
	if (!emstune.modes)
		return NULL;

	if (emstune.boot.ongoing)
		return &emstune.boot.set;

	return &emstune.modes[emstune.cur_mode].sets[emstune.cur_level];
}

/******************************************************************************/
/* emstune level request                                                      */
/******************************************************************************/
static int emstune_notify(void);
static void emstune_mode_change(int next_mode)
{
	unsigned long flags;
	char msg[32];

	spin_lock_irqsave(&emstune.lock, flags);

	if (emstune.cur_mode == next_mode) {
		spin_unlock_irqrestore(&emstune.lock, flags);
		return;
	}

	trace_emstune_mode(emstune.cur_mode, emstune.cur_level);
	emstune.cur_mode = next_mode;
	emstune_notify();

	spin_unlock_irqrestore(&emstune.lock, flags);

	send_mode_to_user(emstune.cur_mode);
	snprintf(msg, sizeof(msg), "NOTI_MODE=%d", emstune.cur_mode);
	send_uevent(msg);
}

static void emstune_level_change(int next_level)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune.lock, flags);

	if (emstune.cur_level == next_level) {
		spin_unlock_irqrestore(&emstune.lock, flags);
		return;
	}

	trace_emstune_mode(emstune.cur_mode, emstune.cur_level);
	emstune.cur_level = next_level;
	emstune_notify();

	spin_unlock_irqrestore(&emstune.lock, flags);
}

static void emstune_reset(int mode, int level)
{
	memcpy(&emstune.modes[mode].sets[level],
			&emstune.backup[mode].sets[level],
			sizeof(struct emstune_set));
}

static int emstune_pm_qos_notifier(struct notifier_block *nb, unsigned long level, void *data)
{
	emstune_level_change(level);

	return 0;
}

void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line)
{
	if (dev_pm_qos_add_request(emstune.ems_dev, &req->dev_req, DEV_PM_QOS_MIN_FREQUENCY, 0))
		return;

	req->func = func;
	req->line = line;
}
EXPORT_SYMBOL_GPL(__emstune_add_request);

void emstune_remove_request(struct emstune_mode_request *req)
{
	dev_pm_qos_remove_request(&req->dev_req);
}
EXPORT_SYMBOL_GPL(emstune_remove_request);

void emstune_update_request(struct emstune_mode_request *req, int level)
{
	if (is_invalid_level(level))
		return;

	dev_pm_qos_update_request(&req->dev_req, level);
}
EXPORT_SYMBOL_GPL(emstune_update_request);

void emstune_boost(struct emstune_mode_request *req, int enable)
{
	if (enable)
		emstune_update_request(req, emstune.boost_level);
	else
		emstune_update_request(req, 0);
}
EXPORT_SYMBOL_GPL(emstune_boost);

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

bool emstune_task_boost(void)
{
	return task_boost.enabled;
}

/******************************************************************************/
/* emstune notify                                                             */
/******************************************************************************/
static RAW_NOTIFIER_HEAD(emstune_chain);

/* emstune.lock *MUST* be held before notifying */
static int emstune_notify(void)
{
	struct emstune_set *cur_set = get_current_set();

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

/******************************************************************************/
/* initializing and parsing parameter                                         */
/******************************************************************************/
char *task_cgroup_name[] = {
	"root",
	"foreground",
	"background",
	"top-app",
	"rt",
	"system",
	"system-background",
	"dex2oat",
	"nnapi-hal",
	"camera-daemon",
	"midground",
};

int task_cgroup_name_size = ARRAY_SIZE(task_cgroup_name);

static inline void parse_coregroup_field(struct device_node *dn, const char *name,
		int (field)[VENDOR_NR_CPUS], int default_value)
{
	int count, cpu, cursor;
	unsigned int val[VENDOR_NR_CPUS];

	for (count = 0; count < VENDOR_NR_CPUS; count++)
		val[count] = default_value;

	count = of_property_count_u32_elems(dn, name);
	if (count > 0)
		of_property_read_u32_array(dn, name, (u32 *)&val, count);

	count = 0;
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_clustergroup_mask(cpu)))
			continue;

		for_each_cpu(cursor, cpu_clustergroup_mask(cpu))
			field[cursor] = val[count];

		count++;
	}
}

static inline void parse_cgroup_field(struct device_node *dn, int *field, int default_value)
{
	int cgroup, val;

	for_each_cgroup(cgroup) {
		if (!of_property_read_u32(dn, task_cgroup_name[cgroup], &val))
			field[cgroup] = val;
		else
			field[cgroup] = default_value;
	}
}

static inline int parse_cgroup_field_mask(struct device_node *dn, struct cpumask *mask)
{
	int cgroup;
	const char *buf;

	for_each_cgroup(cgroup) {
		if (!of_property_read_string(dn, task_cgroup_name[cgroup], &buf))
			cpulist_parse(buf, &mask[cgroup]);
		else
			cpumask_copy(&mask[cgroup], cpu_possible_mask);
	}

	return 0;
}

static inline void parse_cgroup_coregroup_field(struct device_node *dn,
		int (field)[CGROUP_COUNT][VENDOR_NR_CPUS], int default_value)
{
	int cgroup;

	for_each_cgroup(cgroup)
		parse_coregroup_field(dn, task_cgroup_name[cgroup], field[cgroup], default_value);
}

static void parse_active_weight(struct device_node *dn, struct emstune_active_weight *active_weight)
{
	parse_cgroup_coregroup_field(dn, active_weight->ratio, 100);
}

static void parse_idle_weight(struct device_node *dn, struct emstune_idle_weight *idle_weight)
{
	parse_cgroup_coregroup_field(dn, idle_weight->ratio, 100);
}

static void parse_freqboost(struct device_node *dn, struct emstune_freqboost *freqboost)
{
	parse_cgroup_coregroup_field(dn, freqboost->ratio, 0);
}

static void parse_cpufreq_gov(struct device_node *dn, struct emstune_cpufreq_gov *cpufreq_gov)
{
	parse_coregroup_field(dn, "htask-boost", cpufreq_gov->htask_boost, 100);
	parse_coregroup_field(dn, "pelt-boost", cpufreq_gov->pelt_boost, 0);
	parse_coregroup_field(dn, "window-count", cpufreq_gov->window_count, 2);
	parse_coregroup_field(dn, "htask-ratio-threshold", cpufreq_gov->htask_ratio_threshold, 900);
}

#define UPPER_BOUNDARY	0
#define LOWER_BOUNDARY	1
static void __update_ontime_domain(struct ontime_dom *dom, struct list_head *dom_list)
{
	/*
	 * At EMS initializing time, capacity_cpu_orig() cannot be used
	 * because it returns capacity that does not reflect frequency.
	 * So, use et_max_cap() instead of capacity_cpu_orig().
	 */
	unsigned long max_cap = et_max_cap(cpumask_any(&dom->cpus));

	/* Upper boundary of last domain is meaningless */
	if (dom->upper_ratio < 0)
		dom->upper_boundary = 1024;
	else if (dom == list_last_entry(dom_list, struct ontime_dom, node))
		dom->upper_boundary = 1024;
	else
		dom->upper_boundary = max_cap * dom->upper_ratio / 100;

	/* Lower boundary of first domain is meaningless */
	if (dom->lower_ratio < 0)
		dom->lower_boundary = 0;
	else if (dom == list_first_entry(dom_list, struct ontime_dom, node))
		dom->lower_boundary = 0;
	else
		dom->lower_boundary = max_cap * dom->lower_ratio / 100;
}

static int init_dom_list(struct device_node *dn, struct list_head *dom_list)
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
		} else {
			cpulist_parse(buf, &dom->cpus);
		}

		if (of_property_read_u32(dom_dn, "upper-boundary", &dom->upper_ratio))
			dom->upper_ratio = -1;

		if (of_property_read_u32(dom_dn, "lower-boundary", &dom->lower_ratio))
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

static void parse_cpus_binding(struct device_node *dn, struct emstune_cpus_binding *cpus_binding)
{
	parse_cgroup_field_mask(dn, cpus_binding->mask);
}

static void parse_tex(struct device_node *dn, struct emstune_tex *tex)
{
	parse_cgroup_field(dn, tex->prio, 0);
}

static void parse_ecs_dynamic(struct device_node *dn, struct emstune_ecs_dynamic *dynamic)
{
	const char *buf;

	parse_coregroup_field(dn, "dynamic-busy-ratio", dynamic->dynamic_busy_ratio, 0);

	if (of_property_read_string(dn, "min-cpus", &buf)) {
		cpumask_copy(&dynamic->min_cpus, cpu_possible_mask);
	} else {
		cpulist_parse(buf, &dynamic->min_cpus);
	}
}

static void parse_ntu(struct device_node *dn, struct emstune_ntu *ntu)
{
	parse_cgroup_field(dn, ntu->ratio, 0);
}

static void parse_fclamp(struct device_node *dn, struct emstune_fclamp *fclamp)
{
	parse_coregroup_field(dn, "min-freq", fclamp->min_freq, 0);
	parse_coregroup_field(dn, "min-target-period", fclamp->min_target_period, 0);
	parse_coregroup_field(dn, "min-target-ratio", fclamp->min_target_ratio, 0);

	parse_coregroup_field(dn, "max-freq", fclamp->max_freq, 0);
	parse_coregroup_field(dn, "max-target-period", fclamp->max_target_period, 0);
	parse_coregroup_field(dn, "max-target-ratio", fclamp->max_target_ratio, 0);

	parse_cgroup_field(of_get_child_by_name(dn, "monitor-group"), fclamp->monitor_group, 0);
}

static void parse_cpuidle_gov(struct device_node *dn, struct emstune_cpuidle_gov *cpuidle_gov)
{
	parse_coregroup_field(dn, "expired-ratio", cpuidle_gov->expired_ratio, 100);
}

static void parse_support_uclamp(struct device_node *dn, struct emstune_support_uclamp *su)
{
	if (of_property_read_u32(dn, "enabled", &su->enabled))
		su->enabled = 1;
}

static void parse_gsc(struct device_node *dn, struct emstune_gsc *gsc)
{
	struct device_node *child;
	int i;

	for(i = GSC_LEVEL1; i < GSC_LEVEL_COUNT; i++) {
		gsc->up_threshold[i] = UINT_MAX;
		gsc->down_threshold[i] = UINT_MAX;
	}

	for_each_child_of_node(dn, child) {
		int level;

		if (of_property_read_u32(child, "level", &level))
			continue;

		if (of_property_read_u32(child, "up_threshold", &gsc->up_threshold[level]))
			gsc->up_threshold[level] = UINT_MAX;

		if (of_property_read_u32(child, "down_threshold", &gsc->down_threshold[level]))
			gsc->down_threshold[level] = UINT_MAX;
	}

	if (of_property_read_u32(dn, "boost_en_threshold", &gsc->boost_en_threshold))
		gsc->boost_en_threshold = UINT_MAX;
	if (of_property_read_u32(dn, "boost_dis_threshold", &gsc->boost_dis_threshold))
		gsc->boost_dis_threshold = UINT_MAX;
	if (of_property_read_u32(dn, "boost_ratio", &gsc->boost_ratio))
		gsc->boost_ratio = 0;
}

static void parse_should_spread(struct device_node *dn, struct emstune_should_spread *ss)
{
	if (of_property_read_u32(dn, "enabled", &ss->enabled))
		ss->enabled = 0;
}

static void parse_frt_boost(struct device_node *dn, struct emstune_frt_boost *frt_boost)
{
	parse_cgroup_field(dn, frt_boost->enabled, 0);
}

static void set_value_cgroup_bit(unsigned int *field, int (enabled)[CGROUP_COUNT]);
static void parse_cgroup_boost(struct device_node *dn, struct emstune_cgroup_boost *cgroup_boost)
{
	parse_cgroup_field(dn, cgroup_boost->enabled, 0);
	set_value_cgroup_bit(&cgroup_boost->enabled_bit, cgroup_boost->enabled);
}

static void parse_et(struct device_node *dn, struct emstune_energy_table *et)
{
	if (of_property_read_u32(dn, "default-idx", &et->default_idx))
		et->default_idx = 0;
	if (of_property_read_s32(dn, "uarch-selection", &et->uarch_selection))
		et->uarch_selection = false;
}

/******************************************************************************/
/* emstune sched boost                                                        */
/******************************************************************************/
static int sched_boost;

int emstune_sched_boost(void)
{
	return sched_boost;
}

int emstune_support_uclamp(void)
{
	struct emstune_set *cur_set = get_current_set();

	return cur_set->support_uclamp.enabled;
}

int emstune_should_spread(void)
{
	struct emstune_set *cur_set = get_current_set();

	return cur_set->should_spread.enabled;
}

/******************************************************************************/
/* emstune cgroup boost                                                        */
/******************************************************************************/
bool emstune_need_cgroup_boost(struct task_struct *p)
{
	struct emstune_set *cur_set = get_current_set();

	return (cur_set->cgroup_boost.enabled_bit & (1 << cpuctl_task_group_idx(p)));
}

/******************************************************************************/
/* All in One Tuner                                                           */
/******************************************************************************/
struct aio_tuner_item {
	int			id;
	const char		*name;
	enum arg_type		atype;
	enum data_type		dtype;
	int			limit;
	const char		*comment;
};

enum {
	active_weight,
	idle_weight,
	freqboost,
	cpufreq_pelt_boost,
	cpufreq_htask_boost,
	fclamp_freq,
	fclamp_period,
	fclamp_ratio,
	fclamp_monitor_group,
	ontime_en,
	ontime_boundary,
	cpus_binding_cpumask,
	ecs_busy_ratio,
	gsc_up_threshold,
	gsc_down_threshold,
	gsc_boost_en_threshold,
	gsc_boost_dis_threshold,
	gsc_boost_ratio,
	tex_prio,
	ntu_ratio,
	cpuidle_expired_ratio,
	support_uclamp,
	should_spread,
	frt_boost,
	cgroup_boost,
	cpufreq_htask_window_count,
	cpufreq_htask_ratio_threshold,
	ecs_min_cpus,
	et_default_idx,
	et_uarch_selection,
	item_count,
};

#define MAX_RATIO	10000
#define I(name)		name, #name
struct aio_tuner_item aio_tuner_items[] = {
	/* .id, .name,			.atype,		.dtype,		.limit */
	{ I(active_weight),		CPU_CGROUP,	UNSIGNED_TYPE,	MAX_RATIO },
	{ I(idle_weight),		CPU_CGROUP,	UNSIGNED_TYPE,	MAX_RATIO },
	{ I(freqboost),			CPU_CGROUP,	SIGNED_TYPE,	MAX_RATIO },
	{ I(cpufreq_pelt_boost),	CPU_ONLY,	SIGNED_TYPE,	MAX_RATIO },
	{ I(cpufreq_htask_boost),	CPU_ONLY,	SIGNED_TYPE,	MAX_RATIO },
	{ I(fclamp_freq),		CPU_OPTION,	UNSIGNED_TYPE,	INT_MAX, "option:min=0,max=1" },
	{ I(fclamp_period),		CPU_OPTION,	UNSIGNED_TYPE,	100, "option:min=0,max=1" },
	{ I(fclamp_ratio), 		CPU_OPTION,	UNSIGNED_TYPE,	1000, "option:min=0,max=1" },
	{ I(fclamp_monitor_group),	CGROUP_ONLY,	BOOL_TYPE,	0 },
	{ I(ontime_en),			CGROUP_ONLY,	BOOL_TYPE,	0 },
	{ I(ontime_boundary),		CPU_OPTION,	UNSIGNED_TYPE,	1024, "option:upper=0,lower=1" },
	{ I(cpus_binding_cpumask),	CGROUP_ONLY,	CPUMASK_TYPE,	0, "value:cpumask" },
	{ I(ecs_busy_ratio),		CPU_ONLY,	UNSIGNED_TYPE,	MAX_RATIO,},
	{ I(gsc_up_threshold),		OPTION_ONLY,	UNSIGNED_TYPE,	INT_MAX, "option:gsc level=1,2"},
	{ I(gsc_down_threshold),	OPTION_ONLY,	UNSIGNED_TYPE,	INT_MAX, "option:gsc level=1,2"},
	{ I(gsc_boost_en_threshold),	NOP,		UNSIGNED_TYPE,	INT_MAX,},
	{ I(gsc_boost_dis_threshold),	NOP,		UNSIGNED_TYPE,	INT_MAX,},
	{ I(gsc_boost_ratio),		NOP,		UNSIGNED_TYPE,	INT_MAX,},
	{ I(tex_prio),			CGROUP_ONLY,	UNSIGNED_TYPE,	MAX_PRIO },
	{ I(ntu_ratio),			CGROUP_ONLY,	SIGNED_TYPE,	100 },
	{ I(cpuidle_expired_ratio),	CPU_ONLY,	UNSIGNED_TYPE,	MAX_RATIO },
	{ I(support_uclamp),		NOP,		BOOL_TYPE,	0 },
	{ I(should_spread),		NOP,		BOOL_TYPE,	0 },
	{ I(frt_boost),			CGROUP_ONLY,	BOOL_TYPE,	0 },
	{ I(cgroup_boost),		CGROUP_ONLY,	BOOL_TYPE,	0 },
	{ I(cpufreq_htask_window_count),CPU_ONLY,	SIGNED_TYPE,	MLT_PERIOD_COUNT },
	{ I(cpufreq_htask_ratio_threshold),CPU_ONLY,	SIGNED_TYPE,	MAX_RATIO },
	{ I(ecs_min_cpus),		NOP,		CPUMASK_TYPE,	0, "value:cpumask" },
	{ I(et_default_idx),		NOP,		UNSIGNED_TYPE,	INT_MAX },
	{ I(et_uarch_selection),	NOP,		SIGNED_TYPE,	INT_MAX },
};

static int sanity_check_default(int id, int mode, int level)
{
	if (id < 0 || id >= item_count)
		return -EINVAL;
	if (is_invalid_mode(mode))
		return -EINVAL;
	if (is_invalid_level(level))
		return -EINVAL;

	return 0;
}

static int sanity_check_convert_value(char *arg, enum data_type type, int limit, int *v)
{
	struct cpumask mask;
	long value;

	switch (type) {
	case BOOL_TYPE:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		*v = !!value;
		break;
	case SIGNED_TYPE:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		if (value < -limit || value > limit)
			return -EINVAL;
		*v = value;
		break;
	case UNSIGNED_TYPE:
		if (kstrtol(arg, 10, &value))
			return -EINVAL;
		if (value < 0 || value > limit)
			return -EINVAL;
		*v = value;
		break;
	case CPUMASK_TYPE:
		if (arg[0] == '0' && toupper(arg[1]) == 'X')
			arg = arg + 2;
		if (cpumask_parse(arg, &mask) || cpumask_empty(&mask))
			return -EINVAL;
		*v = *(int *)cpumask_bits(&mask);
		break;
	}

	return 0;
}

static void set_value_single(int *field, int value)
{
	*field = value;
}

static void set_value_coregroup(int (field)[VENDOR_NR_CPUS], int cpu, int value)
{
	int i;

	for_each_cpu(i, cpu_clustergroup_mask(cpu))
		field[i] = value;
}

static void set_value_cgroup(int (field)[CGROUP_COUNT], int cgroup, int value)
{
	if (is_invalid_cgroup(cgroup)) {
		for_each_cgroup(cgroup)
			field[cgroup] = value;
	} else {
		field[cgroup] = value;
	}
}

static void set_value_cgroup_bit(unsigned int *field, int (enabled)[CGROUP_COUNT])
{
	unsigned int bits = 0;
	int cgroup;

	for_each_cgroup(cgroup) {
		if (enabled[cgroup])
			bits |= (1 << cgroup);
	}
	*field = bits;
}

static void set_value_coregroup_cgroup(int (field)[CGROUP_COUNT][VENDOR_NR_CPUS],
		int cpu, int cgroup, int value)
{
	int i;

	if (is_invalid_cgroup(cgroup)) {
		for_each_cgroup(cgroup) {
			for_each_cpu(i, cpu_clustergroup_mask(cpu))
				field[cgroup][i] = value;
		}
	} else {
		for_each_cpu(i, cpu_clustergroup_mask(cpu))
			field[cgroup][i] = value;
	}
}

static void set_value_cpumask(struct cpumask *dmask, int value)
{
	const unsigned long new_value = value;
	struct cpumask *new_mask = to_cpumask(&new_value);

	cpumask_copy(dmask, new_mask);
}

static void set_value_cgroup_cpumask(struct cpumask *dmask, int cgroup, int value)
{
	const unsigned long new_value = value;
	struct cpumask *new_mask = to_cpumask(&new_value);

	cpumask_copy(&dmask[cgroup], new_mask);
}

static void set_ontime_boundary(struct list_head *dom_list, int cpu, int option, int value)
{
	struct ontime_dom *dom;

	list_for_each_entry(dom, dom_list, node) {
		if (cpumask_test_cpu(cpu, &dom->cpus)) {
			if (option == UPPER_BOUNDARY)
				dom->upper_ratio = value;
			else
				dom->lower_ratio = value;

			__update_ontime_domain(dom, dom_list);
		}
	}
}

static void set_gsc_threshold(unsigned int *field, int option, int value)
{
	if (valid_gsc_level(option)) {
		if (value < 0)
			value = 0;

		field[option] = value;
	}
}

#define NUM_OF_KEY	10
static int string_to_keys(char *cmd_str, int keys[NUM_OF_KEY])
{
	char *ptr;
	long val;
	int i;

	for (i = 0; i < NUM_OF_KEY; i++)
		keys[i] = -1;

	ptr = strsep(&cmd_str, ",");
	i = 0;
	while (ptr != NULL) {
		if (kstrtol(ptr, 10, &val))
			return -EINVAL;

		if (i >= NUM_OF_KEY)
			return -EINVAL;

		keys[i] = (int)val;
		i++;

		ptr = strsep(&cmd_str, ",");
	};

	return 0;
}

/******************************************************************************/
/* sysfs for emstune                                                          */
/******************************************************************************/
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

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, mode, level;
	unsigned long flags;

	for_each_mode(mode)
		ret += sprintf(buf + ret, "mode%d : %s\n", mode, emstune.modes[mode].desc);

	ret += sprintf(buf + ret, "\n");
	spin_lock_irqsave(&emstune.lock, flags);

	ret += sprintf(buf + ret, "┌─────");
	for_each_mode(mode)
		ret += sprintf(buf + ret, "┬──────");
	ret += sprintf(buf + ret, "┐\n");

	ret += sprintf(buf + ret, "│     ");
	for_each_mode(mode)
		ret += sprintf(buf + ret, "│ mode%d", mode);
	ret += sprintf(buf + ret, "│\n");

	ret += sprintf(buf + ret, "├─────");
	for_each_mode(mode)
		ret += sprintf(buf + ret, "┼──────");
	ret += sprintf(buf + ret, "┤\n");

	for_each_level(level) {
		ret += sprintf(buf + ret, "│ lv%d ", level);
		for_each_mode(mode) {
			if (mode == emstune.cur_mode && level == emstune.cur_level)
				ret += sprintf(buf + ret, "│  curr");
			else
				ret += sprintf(buf + ret, "│      ");
		}
		ret += sprintf(buf + ret, "│\n");

		if (level == emstune.level_count - 1)
			break;

		ret += sprintf(buf + ret, "├-----");
		for_each_mode(mode)
			ret += sprintf(buf + ret, "┼------");
		ret += sprintf(buf + ret, "┤\n");
	}

	ret += sprintf(buf + ret, "└─────");
	for_each_mode(mode)
		ret += sprintf(buf + ret, "┴──────");
	ret += sprintf(buf + ret, "┘\n");

	ret += sprintf(buf + ret, "\n");
	spin_unlock_irqrestore(&emstune.lock, flags);

	return ret;
}
DEVICE_ATTR_RO(status);

static ssize_t req_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&emstune.lock, flags);
	ret = sprintf(buf, "%d\n", emstune.cur_mode);
	spin_unlock_irqrestore(&emstune.lock, flags);

	return ret;
}

static ssize_t req_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int mode;

	if (sscanf(buf, "%d", &mode) != 1)
		return -EINVAL;

	/* ignore if requested mode is out of range */
	if (is_invalid_mode(mode))
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

	spin_lock_irqsave(&emstune.lock, flags);
	ret = sprintf(buf, "%d\n", emstune.cur_level);
	spin_unlock_irqrestore(&emstune.lock, flags);

	return ret;
}

static ssize_t req_level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int level;

	if (sscanf(buf, "%d", &level) != 1)
		return -EINVAL;

	if (is_invalid_level(level))
		return -EINVAL;

	emstune_update_request(&emstune.level_req, level);

	return count;
}
DEVICE_ATTR_RW(req_level);

static ssize_t req_level_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, count = 0;
	struct plist_node *pos;

	plist_for_each(pos, &dev->power.qos->freq.min_freq.list) {
		struct freq_qos_request *f_req;
		struct dev_pm_qos_request *d_req;
		struct emstune_mode_request *req;

		f_req = container_of(pos, struct freq_qos_request, pnode);
		d_req = container_of(f_req, struct dev_pm_qos_request, data.freq);
		req = container_of(d_req, struct emstune_mode_request, dev_req);

		ret += snprintf(buf + ret, PAGE_SIZE, "%d: %d (%s:%d)\n",
				++count, f_req->pnode.prio, req->func, req->line);
	}

	return ret;
}
DEVICE_ATTR_RO(req_level_debug);

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

	if (is_invalid_mode(mode) || is_invalid_level(level))
		return -EINVAL;

	emstune_reset(mode, level);

	return count;
}
DEVICE_ATTR_RW(reset);

static ssize_t func_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, i;

	for (i = 0; i < item_count; i++) {
		ret += sprintf(buf + ret, "%d:%s\n",
				aio_tuner_items[i].id, aio_tuner_items[i].name);
	}

	return ret;
}
DEVICE_ATTR_RO(func_list);

static ssize_t aio_tuner_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, i;

	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "echo <func,mode,level,...> <value> > aio_tuner\n");
	ret += sprintf(buf + ret, "\n");

	for (i = 0; i < item_count; i++) {
		struct aio_tuner_item *item = &aio_tuner_items[i];
		char *args[] = {
			",cpu",
			",cgroup",
			",cpu,cgroup",
			",cpu,option",
			",option",
			"",
		};

		ret += sprintf(buf + ret, "[%d] %s\n", item->id, item->name);
		ret += sprintf(buf + ret, "	# echo {%d,mode,level%s} {value} > aio_tuner\n",
				item->id, args[item->atype]);
		if (item->comment)
			ret += sprintf(buf + ret, "	(%s)\n", item->comment);
		ret += sprintf(buf + ret, "\n");
	}

	return ret;
}

static ssize_t aio_tuner_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct emstune_set *set;
	struct aio_tuner_item *item;
	char cmd_str[100], value_str[100];
	int keys[NUM_OF_KEY], value;
	int mode, level, id, cpu = 0, cgroup = 0, option = 0;

	if (sscanf(buf, "%99s %99s", cmd_str, value_str) != 2)
		return -EINVAL;

	/* Convert command string to keys array */
	if (string_to_keys(cmd_str, keys))
		return -EINVAL;

	id = keys[0];
	mode = keys[1];
	level = keys[2];

	/* Basic validation check */
	if (sanity_check_default(id, mode, level))
		return -EINVAL;

	set = &emstune.modes[mode].sets[level];
	item = &aio_tuner_items[id];

	/* Convert value string to value with sanity checking */
	if (sanity_check_convert_value(value_str, item->dtype, item->limit, &value))
		return -EINVAL;

	/* Get & check each key according to arguemnt type */
	switch (item->atype) {
	case CPU_ONLY:
		cpu = keys[3];
		if (!cpu_possible(cpu))
			return -EINVAL;
		break;
	case CGROUP_ONLY:
		cgroup = keys[3];
		if (is_invalid_cgroup(cgroup))
			return -EINVAL;
		break;
	case CPU_CGROUP:
		cpu = keys[3];
		cgroup = keys[4];
		if (is_invalid_cgroup(cgroup))
			return -EINVAL;
		if (!cpu_possible(cpu))
			return -EINVAL;
		break;
	case CPU_OPTION:
		cpu = keys[3];
		option = keys[4];
		if (!cpu_possible(cpu))
			return -EINVAL;
		break;
	case OPTION_ONLY:
		option = keys[3];
		break;
	default:
		break;
	}

	/* Set value actually */
	switch (id) {
	case active_weight:
		set_value_coregroup_cgroup(set->active_weight.ratio, cpu, cgroup, value);
		break;
	case idle_weight:
		set_value_coregroup_cgroup(set->idle_weight.ratio, cpu, cgroup, value);
		break;
	case freqboost:
		set_value_coregroup_cgroup(set->freqboost.ratio, cpu, cgroup, value);
		break;
	case cpufreq_htask_boost:
		set_value_coregroup(set->cpufreq_gov.htask_boost, cpu, value);
		break;
	case cpufreq_pelt_boost:
		set_value_coregroup(set->cpufreq_gov.pelt_boost, cpu, value);
		break;
	case cpuidle_expired_ratio:
		set_value_coregroup(set->cpuidle_gov.expired_ratio, cpu, value);
		break;
	case ontime_en:
		set_value_cgroup(set->ontime.enabled, cgroup, value);
		break;
	case ontime_boundary:
		set_ontime_boundary(&set->ontime.dom_list, cpu, option, value);
		break;
	case cpus_binding_cpumask:
		set_value_cgroup_cpumask(set->cpus_binding.mask, cgroup, value);
		break;
	case ecs_busy_ratio:
		set_value_coregroup(set->ecs_dynamic.dynamic_busy_ratio, cpu, value);
		break;
	case fclamp_freq:
		if (option)
			set_value_coregroup(set->fclamp.max_freq, cpu, value);
		else
			set_value_coregroup(set->fclamp.min_freq, cpu, value);
		break;
	case fclamp_period:
		if (option)
			set_value_coregroup(set->fclamp.max_target_period, cpu, value);
		else
			set_value_coregroup(set->fclamp.min_target_period, cpu, value);
		break;
	case fclamp_ratio:
		if (option)
			set_value_coregroup(set->fclamp.max_target_ratio, cpu, value);
		else
			set_value_coregroup(set->fclamp.min_target_ratio, cpu, value);
		break;
	case fclamp_monitor_group:
		set_value_cgroup(set->fclamp.monitor_group, cgroup, value);
		break;
	case gsc_up_threshold:
		set_gsc_threshold(set->gsc.up_threshold, option, value);
		break;
	case gsc_down_threshold:
		set_gsc_threshold(set->gsc.down_threshold, option, value);
		break;
	case gsc_boost_en_threshold:
		set_value_single(&set->gsc.boost_en_threshold, value);
		break;
	case gsc_boost_dis_threshold:
		set_value_single(&set->gsc.boost_dis_threshold, value);
		break;
	case gsc_boost_ratio:
		set_value_single(&set->gsc.boost_ratio, value);
		break;
	case tex_prio:
		set_value_cgroup(set->tex.prio, cgroup, value);
		break;
	case ntu_ratio:
		set_value_cgroup(set->ntu.ratio, cgroup, value);
		break;
	case support_uclamp:
		set_value_single(&set->support_uclamp.enabled, value);
		break;
	case should_spread:
		set_value_single(&set->should_spread.enabled, value);
		break;
	case frt_boost:
		set_value_cgroup(set->frt_boost.enabled, cgroup, value);
		break;
	case cgroup_boost:
		set_value_cgroup(set->cgroup_boost.enabled, cgroup, value);
		set_value_cgroup_bit(&set->cgroup_boost.enabled_bit, set->cgroup_boost.enabled);
		break;
	case cpufreq_htask_window_count:
		set_value_coregroup(set->cpufreq_gov.window_count, cpu, value);
		break;
	case cpufreq_htask_ratio_threshold:
		set_value_coregroup(set->cpufreq_gov.htask_ratio_threshold, cpu, value);
		break;
	case ecs_min_cpus:
		set_value_cpumask(&set->ecs_dynamic.min_cpus, value);
		break;
	case et_default_idx:
		set_value_single(&set->et.default_idx, value);
		break;
	case et_uarch_selection:
		set_value_single(&set->et.uarch_selection, value);
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
	"dex",
	"n-h",
	"c-d",
	"mg",
};

static ssize_t print_with_cpu(char *msg, int (field)[VENDOR_NR_CPUS], char *title)
{
	int cpu;
	ssize_t count = 0;

	count += sprintf(msg + count, "     %9s\n", title);
	for_each_possible_cpu(cpu)
		count += sprintf(msg + count, "cpu%d:%9d\n", cpu, field[cpu]);

	return count;
}

static ssize_t print_with_cgroup(char *msg, int (field)[CGROUP_COUNT])
{
	int cgroup;
	ssize_t count = 0;

	for_each_cgroup(cgroup)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[cgroup]);
	count += sprintf(msg + count, "\n");
	for_each_cgroup(cgroup)
		count += sprintf(msg + count, "%4d", field[cgroup]);
	count += sprintf(msg + count, "\n");

	return count;
}

static ssize_t print_with_cpu_cgroup(char *msg, int (field)[CGROUP_COUNT][VENDOR_NR_CPUS])
{
	int cpu, cgroup;
	ssize_t count = 0;

	count += sprintf(msg + count, "     ");
	for_each_cgroup(cgroup)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[cgroup]);
	count += sprintf(msg + count, "\n");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d:", cpu);
		for_each_cgroup(cgroup)
			count += sprintf(msg + count, "%4d", field[cgroup][cpu]);
		count += sprintf(msg + count, "\n");
	}

	return count;
}

#define MSG_SIZE 8192
static ssize_t cur_set_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t offset, size_t size)
{
	struct emstune_set *cur_set = get_current_set();
	struct ontime_dom *dom;
	char *msg = kcalloc(MSG_SIZE, sizeof(char), GFP_KERNEL);
	ssize_t count = 0, msg_size;
	int cpu, cgroup, level;

	if (!msg)
		return 0;

	/* mode/level info */
	if (emstune.boot.ongoing)
		count += sprintf(msg + count, "Booting exclusive mode\n");
	else
		count += sprintf(msg + count, "[%d] %s mode : level%d\n",
				cur_set->mode, emstune.modes[cur_set->mode].desc, cur_set->level);
	count += sprintf(msg + count, "\n");

	/* active weight */
	count += sprintf(msg + count, "[active-weight]\n");
	count += print_with_cpu_cgroup(msg + count, cur_set->active_weight.ratio);
	count += sprintf(msg + count, "\n");

	/* idle weight */
	count += sprintf(msg + count, "[idle-weight]\n");
	count += print_with_cpu_cgroup(msg + count, cur_set->idle_weight.ratio);
	count += sprintf(msg + count, "\n");

	/* freq boost */
	count += sprintf(msg + count, "[freq-boost]\n");
	count += print_with_cpu_cgroup(msg + count, cur_set->freqboost.ratio);
	count += sprintf(msg + count, "\n");

	/* cpufreq gov */
	count += sprintf(msg + count, "[cpufreq gov]\n");
	count += sprintf(msg + count, "      %12s %12s %12s %12s\n", "pelt_boost", "htask_boost", "window_count", "htask_ratio_threshold");
	for_each_possible_cpu(cpu) {
		count += sprintf(msg + count, "cpu%d: %12d %12d %12d %12d\n", cpu,
				cur_set->cpufreq_gov.pelt_boost[cpu],
				cur_set->cpufreq_gov.htask_boost[cpu],
				cur_set->cpufreq_gov.window_count[cpu],
				cur_set->cpufreq_gov.htask_ratio_threshold[cpu]);
	}
	count += sprintf(msg + count, "\n");

	/* fclamp */
	count += sprintf(msg + count, "[fclamp]\n");
	count += print_with_cgroup(msg + count, cur_set->fclamp.monitor_group);
	count += sprintf(msg + count, "\n");
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

	/* cpuidle gov */
	count += sprintf(msg + count, "[cpuidle gov]\n");
	count += sprintf(msg + count, "expired ratio\n");
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_clustergroup_mask(cpu)))
			continue;
		count += sprintf(msg + count, "cpu%d: %4d\n", cpu,
				cur_set->cpuidle_gov.expired_ratio[cpu]);
	}
	count += sprintf(msg + count, "\n");

	/* ontime */
	count += sprintf(msg + count, "[ontime]\n");
	count += print_with_cgroup(msg + count, cur_set->ontime.enabled);
	count += sprintf(msg + count, "\n");
	if (list_empty(cur_set->ontime.p_dom_list)) {
		count += sprintf(msg + count, "list is empty!\n");
	} else  {
		count += sprintf(msg + count, "-----------------------------\n");
		list_for_each_entry(dom, cur_set->ontime.p_dom_list, node) {
			count += sprintf(msg + count, " cpus           :  %*pbl\n",
					cpumask_pr_args(&dom->cpus));
			count += sprintf(msg + count, " upper boundary : %4lu (%3d%%)\n",
					dom->upper_boundary, dom->upper_ratio);
			count += sprintf(msg + count, " lower boundary : %4lu (%3d%%)\n",
					dom->lower_boundary, dom->lower_ratio);
			count += sprintf(msg + count, "-----------------------------\n");
		}
	}
	count += sprintf(msg + count, "\n");

	/* cpus binding */
	count += sprintf(msg + count, "[cpus binding]\n");
	for_each_cgroup(cgroup)
		count += sprintf(msg + count, "%4s", task_cgroup_simple_name[cgroup]);
	count += sprintf(msg + count, "\n");
	for_each_cgroup(cgroup)
		count += sprintf(msg + count, "%4x",
				*(unsigned int *)cpumask_bits(&cur_set->cpus_binding.mask[cgroup]));
	count += sprintf(msg + count, "\n\n");

	/* ecs */
	count += sprintf(msg + count, "[ecs]\n");
	count += print_with_cpu(msg + count, cur_set->ecs_dynamic.dynamic_busy_ratio, "busy(%)");
	count += sprintf(msg + count, "min_cpus : %4x\n",
			*(unsigned int *)cpumask_bits(&cur_set->ecs_dynamic.min_cpus));
	count += sprintf(msg + count, "\n");

	/* gsc */
	count += sprintf(msg + count, "[gsc]\n");
	count += sprintf(msg + count, "-----------------------------\n");
	for (level = GSC_LEVEL1; level < GSC_LEVEL_COUNT; level++) {
		count += sprintf(msg + count, "level %d\n", level);
		count += sprintf(msg + count, "up_threshold : %u\n",
						cur_set->gsc.up_threshold[level]);
		count += sprintf(msg + count, "down_threshold : %u\n",
						cur_set->gsc.down_threshold[level]);
		count += sprintf(msg + count, "-----------------------------\n");
	}

	count += sprintf(msg + count, "boost_en_threshold : %u\n", cur_set->gsc.boost_en_threshold);
	count += sprintf(msg + count, "boost_dis_threshold : %u\n", cur_set->gsc.boost_dis_threshold);
	count += sprintf(msg + count, "boost_ratio: %u\n", cur_set->gsc.boost_ratio);
	count += sprintf(msg + count, "\n");

	/* tex */
	count += sprintf(msg + count, "[tex]\n");
	count += print_with_cgroup(msg + count, cur_set->tex.prio);
	count += sprintf(msg + count, "\n");

	/* new task util ratio */
	count += sprintf(msg + count, "[new task util ratio]\n");
	count += print_with_cgroup(msg + count, cur_set->ntu.ratio);
	count += sprintf(msg + count, "\n");

	/* support uclamp */
	count += sprintf(msg + count, "[support uclamp]\n");
	count += sprintf(msg + count, "enabled : %d\n", cur_set->support_uclamp.enabled);
	count += sprintf(msg + count, "\n");

	/* should spread */
	count += sprintf(msg + count, "[should spread]\n");
	count += sprintf(msg + count, "enabled : %d\n", cur_set->should_spread.enabled);
	count += sprintf(msg + count, "\n");

	count += sprintf(msg + count, "[frt_boost]\n");
	count += print_with_cgroup(msg + count, cur_set->frt_boost.enabled);
	count += sprintf(msg + count, "\n");

	/* cgroup boost */
	count += sprintf(msg + count, "[cgroup_boost]\n");
	count += print_with_cgroup(msg + count, cur_set->cgroup_boost.enabled);
	count += sprintf(msg + count, "\n");

	/* energy table */
	count += sprintf(msg + count, "[energy_table]\n");
	count += sprintf(msg + count, "default_idx : %d\n", cur_set->et.default_idx);
	count += sprintf(msg + count, "uarch_selection: %d\n", cur_set->et.uarch_selection);
	count += sprintf(msg + count, "\n");

	msg_size = min_t(ssize_t, count, MSG_SIZE);
	msg_size = memory_read_from_buffer(buf, size, &offset, msg, msg_size);

	kfree(msg);

	return msg_size;
}
static BIN_ATTR_RO(cur_set, 0);

static ssize_t task_boost_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	mutex_lock(&task_boost.lock);

	ret = sprintf(buf, "%d\n", task_boost.tid);

	mutex_unlock(&task_boost.lock);
	return ret;
}

static int get_tid_and_val(int *data, char *buf)
{
	char *arg, *ptr;
	int i = 0, ret;

	arg = buf;
	ptr = strsep(&arg, ",");

	while ((ptr != NULL) && (i < 2)) {
		ret = kstrtoint(ptr, 10, &data[i]);
		if (ret)
			break;

		i++;
		ptr = strsep(&arg, ",");
	}

	return i;
}

static ssize_t task_boost_type1_store(int tid, size_t count)
{
	struct task_struct *task;

	if (tid < 0)
		return -EINVAL;

	task_boost.enabled = false;

	/* clear prev task_boost */
	if (task_boost.tid != 0) {
		task = get_pid_task(find_vpid(task_boost.tid), PIDTYPE_PID);
		if (task) {
			ems_boosted_tex(task) = 0;
			put_task_struct(task);

			trace_emstune_task_boost(task, 0, "task_boost");
		}
	}

	/* set task_boost */
	task = get_pid_task(find_vpid(tid), PIDTYPE_PID);
	if (task) {
		if (task->group_leader == task)
			task_boost.enabled = true;

		ems_boosted_tex(task) = 1;
		put_task_struct(task);

		trace_emstune_task_boost(task, 1, "task_boost");
	}

	task_boost.tid = tid;

	return count;
}

static ssize_t task_boost_type2_store(int tid, int val, size_t count)
{
	struct task_struct *task;

	if (tid <= 0 || val < 0)
		return -EINVAL;

	task = get_pid_task(find_vpid(tid), PIDTYPE_PID);
	if (!task)
		return -ENOENT;

	ems_boosted_tex(task) = val;

	put_task_struct(task);

	trace_emstune_task_boost(task, val, "task_boost");

	return count;
}

enum {
	TASK_BOOST_TYPE1 = 1,
	TASK_BOOST_TYPE2,
};

static ssize_t task_boost_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char arg[100];
	int data[2] = {-1, -1};
	int type, ret;

	if (sscanf(buf, "%99s", arg) != 1)
		return -EINVAL;

	mutex_lock(&task_boost.lock);

	type = get_tid_and_val(data, arg);

	switch (type) {
		case TASK_BOOST_TYPE1:
			ret = task_boost_type1_store(data[0], count);
			break;
		case TASK_BOOST_TYPE2:
			ret = task_boost_type2_store(data[0], data[1], count);
			break;
		default:
			ret = -EINVAL;
			break;
	}

	mutex_unlock(&task_boost.lock);

	return ret;
}
DEVICE_ATTR_RW(task_boost);

static ssize_t task_boost_del_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	int ret = 0;

	return ret;
}

static ssize_t task_boost_del_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct task_struct *task;
	int tid;

	if (sscanf(buf, "%d", &tid) != 1)
		return -EINVAL;

	mutex_lock(&task_boost.lock);

	task = get_pid_task(find_vpid(tid), PIDTYPE_PID);
	if (task) {
		ems_boosted_tex(task) = 0;
		put_task_struct(task);

		trace_emstune_task_boost(task, 0, "task_boost_del");
	}

	/* clear task_boost at last task_boost_del */
	if (task_boost.tid == tid)
		task_boost.tid = 0;

	mutex_unlock(&task_boost.lock);
	return count;
}
DEVICE_ATTR_RW(task_boost_del);

static ssize_t low_latency_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
	int ret = 0;

	return ret;
}

static ssize_t low_latency_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct task_struct *task;
	char arg[100];
	int data[2] = {-1, -1};
	int tid, val;

	if (sscanf(buf, "%s", arg) != 1)
		return -EINVAL;

	mutex_lock(&task_boost.lock);

	if (get_tid_and_val(data, arg) != TASK_BOOST_TYPE2) {
		mutex_unlock(&task_boost.lock);
		return -EINVAL;
	}

	tid = data[0];
	val = data[1];

	if (tid <= 0 || val < 0) {
		mutex_unlock(&task_boost.lock);
		return -EINVAL;
	}

	task = get_pid_task(find_vpid(tid), PIDTYPE_PID);
	if (!task) {
		mutex_unlock(&task_boost.lock);
		return -ENOENT;
	}

	ems_low_latency(task) = val;

	trace_emstune_task_boost(task, val, "low_latency");

	put_task_struct(task);

	mutex_unlock(&task_boost.lock);

	return count;
}
DEVICE_ATTR_RW(low_latency);

static struct attribute *emstune_attrs[] = {
	&dev_attr_sched_boost.attr,
	&dev_attr_status.attr,
	&dev_attr_req_mode.attr,
	&dev_attr_req_level.attr,
	&dev_attr_req_level_debug.attr,
	&dev_attr_reset.attr,
	&dev_attr_aio_tuner.attr,
	&dev_attr_func_list.attr,
	&dev_attr_task_boost.attr,
	&dev_attr_task_boost_del.attr,
	&dev_attr_low_latency.attr,
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

/******************************************************************************/
/* initialization                                                             */
/******************************************************************************/
int emstune_cpu_dsu_table_index(void *_set)
{
	struct emstune_set *set = (struct emstune_set *)_set;

	return set->cpu_dsu_table_index;
}
EXPORT_SYMBOL_GPL(emstune_cpu_dsu_table_index);

static void free_emstune_mode(void)
{
	int i;

	if (emstune.modes) {
		for_each_mode(i)
			kfree(emstune.modes[i].sets);
		kfree(emstune.modes);
	}

	if (emstune.backup) {
		for_each_mode(i)
			kfree(emstune.backup[i].sets);
		kfree(emstune.backup);
	}
}

static int emstune_mode_backup(void)
{
	int mode;

	emstune.backup = kcalloc(emstune.mode_count, sizeof(struct emstune_mode), GFP_KERNEL);
	if (!emstune.backup)
		return -ENOMEM;

	memcpy(emstune.backup, emstune.modes, sizeof(struct emstune_mode) * emstune.mode_count);

	for_each_mode(mode) {
		emstune.backup[mode].sets =
			kcalloc(emstune.level_count, sizeof(struct emstune_set), GFP_KERNEL);
		if (!emstune.backup[mode].sets)
			return -ENOMEM;

		memcpy(emstune.backup[mode].sets, emstune.modes[mode].sets,
				sizeof(struct emstune_set) * emstune.level_count);
	}

	return 0;
}

static void emstune_perf_table_init(struct emstune_set *set,
		struct device_node *base_node, 	struct device_node *set_node)
{
	if (!of_property_read_u32(set_node, "cpu-dsu-table-index", &set->cpu_dsu_table_index))
		return;

	if (!of_property_read_u32(base_node, "cpu-dsu-table-index", &set->cpu_dsu_table_index))
		return;

	/* 0 = default index */
	set->cpu_dsu_table_index = 0;
}

#define parse(_name)							\
	child = of_get_child_by_name(set_node, #_name);			\
	if (!child)							\
		child = of_get_child_by_name(base_node, #_name);	\
	parse_##_name(child, &set->_name);				\
	of_node_put(child)

static void parse_emstune_set(struct device_node *base_node, struct device_node *set_node,
		struct emstune_set *set)
{
	struct device_node *child;

	emstune_perf_table_init(set, base_node, set_node);

	parse(active_weight);
	parse(idle_weight);
	parse(freqboost);
	parse(cpufreq_gov);
	parse(ontime);
	parse(cpus_binding);
	parse(tex);
	parse(ecs_dynamic);
	parse(ntu);
	parse(fclamp);
	parse(support_uclamp);
	parse(gsc);
	parse(should_spread);
	parse(cpuidle_gov);
	parse(frt_boost);
	parse(cgroup_boost);
	parse(et);
}

static void emstune_boot_done(struct work_struct *work)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune.lock, flags);
	emstune.boot.ongoing = false;
	emstune_notify();
	spin_unlock_irqrestore(&emstune.lock, flags);
}

static void parse_boot_set(struct device_node *dn)
{
       struct device_node *node;
       unsigned long flags;

       node = of_parse_phandle(dn, "boot-set", 0);
       if (!node) {
	       emstune_mode_change(0);
	       return;
       }

       emstune.boot.set.mode = 0;
       emstune.boot.set.level = 0;
       parse_emstune_set(node, node, &emstune.boot.set);
       of_node_put(node);

       spin_lock_irqsave(&emstune.lock, flags);
       emstune.boot.ongoing = true;
       emstune_notify();
       spin_unlock_irqrestore(&emstune.lock, flags);

       INIT_DELAYED_WORK(&emstune.boot.work, emstune_boot_done);
       schedule_delayed_work(&emstune.boot.work, msecs_to_jiffies(40000));
}

static int parse_emstune_level(struct device_node *dn, struct emstune_mode *mode, int mode_id)
{
	struct device_node *child, *base_node, *set_node;
	int level_id = 0;

	emstune.level_count = of_get_child_count(dn);
	if (!emstune.level_count) {
		return __LINE__;
	}

	mode->sets = kcalloc(emstune.level_count, sizeof(struct emstune_set), GFP_KERNEL);
	if (!mode->sets)
		return __LINE__;

	for_each_child_of_node(dn, child) {
		struct emstune_set *set = &mode->sets[level_id];

		base_node = of_parse_phandle(child, "base", 0);
		set_node = of_parse_phandle(child, "set", 0);

		if (!base_node || !set_node) {
			return __LINE__;
		}

		set->mode = mode_id;
		set->level = level_id;
		parse_emstune_set(base_node, set_node, set);

		of_node_put(base_node);
		of_node_put(set_node);

		level_id++;
	}

	return 0;
}

static int parse_emstune_mode(struct device_node *dn)
{
	struct device_node *child;
	int ret, mode_id = 0;

	emstune.mode_count = of_get_child_count(dn);
	if (!emstune.mode_count) {
		return __LINE__;
	}

	emstune.modes = kcalloc(emstune.mode_count, sizeof(struct emstune_mode), GFP_KERNEL);
	if (!emstune.modes)
		return __LINE__;

	for_each_child_of_node(dn, child) {
		struct emstune_mode *mode = &emstune.modes[mode_id];

		if (of_property_read_string(child, "desc", &mode->desc)) {
			return __LINE__;
		}

		ret = parse_emstune_level(child, mode, mode_id);
		if (ret)
			return ret;

		mode_id++;
	}

	return 0;
}

static int parse_emstune(struct device_node *dn)
{
	struct device_node *node;
	int ret;

	node = of_get_child_by_name(dn, "emstune");
	if (!node)
		return __LINE__;

	of_property_read_u32(node, "boost-level", &emstune.boost_level);

	ret = parse_emstune_mode(node);
	if (ret) {
		free_emstune_mode();
		return ret;
	}

	ret = emstune_mode_backup();
	if (ret) {
		free_emstune_mode();
		return ret;
	};

	parse_boot_set(node);

	return 0;
}

static int init_emstune_pm_qos(struct device *dev)
{
	int ret;

	emstune.nb.notifier_call = emstune_pm_qos_notifier;
	ret = dev_pm_qos_add_notifier(dev, &emstune.nb, DEV_PM_QOS_MIN_FREQUENCY);
	if (ret) {
		pr_err("Failed to register emstume qos notifier\n");
		return ret;
	}

	emstune.ems_dev = dev;

	emstune_add_request(&emstune.level_req);

	return 0;
}

void emstune_ontime_init(void)
{
	struct emstune_set *set;
	struct ontime_dom *dom;
	int mode, level;

	for_each_mode(mode) {
		for_each_level(level) {
			set = &emstune.modes[mode].sets[level];
			list_for_each_entry(dom, &set->ontime.dom_list, node)
				__update_ontime_domain(dom, &set->ontime.dom_list);
		}
	}
}

int emstune_init(struct kobject *ems_kobj, struct device_node *dn, struct device *dev)
{
	int ret;

	ret = parse_emstune(dn);
	if (ret) {
		WARN(1, "Failed to initialize emstune on line:%d\n", ret);
		return ret;
	}

	ret = init_emstune_pm_qos(dev);
	if (ret) {
		pr_err("Failed to initialize emstune PM QoS\n");
		return ret;
	}

	if (sysfs_create_group(ems_kobj, &emstune_attr_group))
		pr_err("Failed to initialize emstune sysfs\n");

	return 0;
}

/* This external symbol is for AMIGO profiler */
void emstune_set_pboost(int mode, int level, int cpu, int value) {
	struct emstune_set *set;

	if (is_invalid_mode(mode))
		return;
	if (is_invalid_level(level))
		return;
	if (!cpu_possible(cpu))
		return;

	set = &emstune.modes[mode].sets[level];
	if (set)
		set_value_coregroup(set->cpufreq_gov.pelt_boost, cpu, value);
}
EXPORT_SYMBOL_GPL(emstune_set_pboost);
