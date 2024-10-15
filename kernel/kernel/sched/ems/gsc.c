/*
 * Exynos Mobile Scheduler Group Scheduling
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 */

#include <dt-bindings/soc/samsung/ems.h>
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#define GSC_DEACTIVATED			(0)
#define GSC_DISABLED			(UINT_MAX)
#define DEFAULT_UP_THRESHOLD		(GSC_DISABLED)
#define DEFAULT_DOWN_THRESHOLD		(GSC_DISABLED)
#define DEFAULT_UPDATE_MS		(GSC_DISABLED)
#define DEFAULT_RETAIN_MS		(GSC_DISABLED)
#define DEFAULT_EXPIRE_NS		(100 * NSEC_PER_MSEC)
#define DEFAULT_BOOST_EN_THR		(MLT_RUNNABLE_UNIT * 2)
#define DEFAULT_BOOST_DIS_THR		(MLT_RUNNABLE_UNIT / 2)
#define DEFAULT_BOOST_RATIO		(25)

struct _gsc_group {
	int			enabled;
	int			status;

	u64			last_update_ts;
	u64			deactivated_ts;

	struct list_head	task_list;
	raw_spinlock_t		lock;

	unsigned int		max_up_threshold;
	unsigned int		up_threshold[GSC_LEVEL_COUNT];
	unsigned int		down_threshold[GSC_LEVEL_COUNT];
	unsigned int		update_ms;
	unsigned int		retain_ms;
	u64			expire_ns;

	/*
	   gsc boost help to ramp-up frequency when
	   task execution is delayed in fastcore with gsc activated
	 */
	unsigned int		boost_en_threshold;
	unsigned int		boost_dis_threshold;
	unsigned int		boost_ratio;
	int			boost[VENDOR_NR_CPUS];
	u64			boost_deactive_ts[VENDOR_NR_CPUS];
} gsc;

struct gsc_task_env {
	struct task_struct	*p;
	struct list_head	grp_list;
	bool			attached;
};

static char *gsc_status_name[] = {
	"deactivated",
	"level1",
	"level2",
};

/********************************************************************************/
/* Supoort Group Schedule Boost							*/
/********************************************************************************/
static void gsc_disable_boost(void)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_clustergroup_mask(cpu)))
			continue;

		gsc.boost_deactive_ts[cpu]= 0;
		gsc.boost[cpu] = GSC_DEACTIVATED;
		freqboost_set_boost_ratio(cpu, CGROUP_TOPAPP, INT_MIN);
		trace_gsc_freq_boost(cpu, -1, INT_MIN, -1);
	}
}

static bool try_to_enable_gsc_boost(int cpu, int load, u64 now)
{
	if (load < gsc.boost_en_threshold)
		return false;

	gsc.boost_deactive_ts[cpu]= 0;
	gsc.boost[cpu] = GSC_LEVEL2;

	return true;
}
static bool try_to_disable_gsc_boost(int cpu, int load, u64 now)
{
	bool ret = false;

	if (load < gsc.boost_dis_threshold) {
		if (!gsc.boost_deactive_ts[cpu]) {
			gsc.boost_deactive_ts[cpu] = now;
		} else if (now - gsc.boost_deactive_ts[cpu] >
				msecs_to_jiffies(gsc.retain_ms)) {
			gsc.boost_deactive_ts[cpu]= 0;
			gsc.boost[cpu] = GSC_DEACTIVATED;
			ret = true;
		}
	} else {
		gsc.boost_deactive_ts[cpu] = 0;
	}

	return ret;
}

static void update_cpu_boost_state(u64 now)
{
	int cursor, cpu;

	if (gsc.boost_en_threshold == GSC_DISABLED)
		return;

	for_each_possible_cpu(cpu) {
		int avg, nr_cpu, boost_ratio = 0, sum = 0;
		bool changed = false;

		if (cpu != cpumask_first(cpu_clustergroup_mask(cpu)))
			continue;

		if (!cpu)
			continue;

		nr_cpu = cpumask_weight(cpu_clustergroup_mask(cpu));
		if (unlikely(!nr_cpu))
			continue;

		for_each_cpu(cursor, cpu_clustergroup_mask(cpu)) {
			struct rq *rq = cpu_rq(cursor);
			sum += mlt_runnable(rq);
		}

		avg = sum / nr_cpu;
		if (gsc.boost[cpu])
			changed = try_to_disable_gsc_boost(cpu, avg, now);
		else
			changed = try_to_enable_gsc_boost(cpu, avg, now);

		if (!changed)
			continue;

		boost_ratio = gsc.boost[cpu] ? gsc.boost_ratio : -1;
		freqboost_set_boost_ratio(cpu, CGROUP_TOPAPP, boost_ratio);
		trace_gsc_freq_boost(cpu, avg, boost_ratio, gsc.boost[cpu]);
	}
}

/********************************************************************************/
/* Group Schedule Handling							*/
/********************************************************************************/
static struct gsc_task_env *get_gte(struct task_struct *p)
{
	return (struct gsc_task_env *)ems_gsc_task(p);
}

static void set_gte(struct task_struct *p, struct gsc_task_env *gte)
{
	ems_gsc_task(p) = (u64)gte;
}

static bool is_top_app(struct task_struct *p)
{
	return cpuctl_task_group_idx(p) == CGROUP_TOPAPP;
}

static void gsc_disable(void)
{
	gsc.deactivated_ts = 0;
	gsc.status = GSC_DEACTIVATED;
	gsc_disable_boost();
}

static void gsc_update_status(unsigned long group_load, u64 now)
{
	int next_level = GSC_DEACTIVATED;
	int curr_level = gsc.status;
	int i;

	/* find fitted gsc level */
	for (i = GSC_LEVEL1; i < GSC_LEVEL_COUNT; i++) {
		if (group_load <= gsc.up_threshold[i])
			break;
		next_level = i;
	}

	if (gsc.status == GSC_DEACTIVATED) {
		gsc.status = next_level;
		gsc.deactivated_ts = 0;
		return;
	}

	/* gsc is activated here */

	/* next level is same or upper level, keep gsc boosting  */
	if (curr_level <= next_level) {
		gsc.status = next_level;
		gsc.deactivated_ts = 0;
		return;
	}

	/* next level is lower than current level */
	if (group_load < gsc.down_threshold[curr_level]) {
		/* keep current level */
		if (!gsc.deactivated_ts) {
			gsc.status = curr_level;
			gsc.deactivated_ts = now;
		}
		else if (now - gsc.deactivated_ts > msecs_to_jiffies(gsc.retain_ms)) {
			/* boost time for current level has expired */
			/* find fitted gsc level at lower level */
			for (i = curr_level - 1; i >= GSC_DEACTIVATED; i--) {
				next_level = i;

				if (next_level == GSC_DEACTIVATED)
					break;
				if (group_load >= gsc.down_threshold[i])
					break;
			}

			if (next_level == GSC_DEACTIVATED)
				gsc_disable();
			else {
				/* boost lower gsc level */
				gsc.status = next_level;
				gsc.deactivated_ts = 0;
			}
		}
	}
	else {
		/* groupd load is fitted in current level, keep gsc level */
		gsc.status = curr_level;
		gsc.deactivated_ts = 0;
	}
}

static inline u64 get_clock_pelt(struct rq *rq)
{
	u64 clock;

	raw_spin_rq_lock(rq);
	if (!(rq->clock_update_flags & RQCF_UPDATED))
		update_rq_clock(rq);
	clock = rq->clock_pelt - rq->lost_idle_time;
	raw_spin_rq_unlock(rq);

	return clock;
}

static unsigned long get_group_load(void)
{
	struct gsc_task_env *gte;
	unsigned long group_load = 0;

	list_for_each_entry(gte, &gsc.task_list, grp_list) {
		struct task_struct *p = gte->p;
		struct sched_entity *se = &p->se;
		s64 delta;

		delta = get_clock_pelt(task_rq(p)) - se->avg.last_update_time;
		delta = max_t(s64, delta, 0);
		if (delta > gsc.expire_ns)
			continue;

		/* FIXME: Need to change to approciate indicator */
		group_load += ml_task_util_est(p);
		if (group_load > gsc.max_up_threshold)
			break;
	}

	return group_load;
}

static void gsc_decision_activate(u64 now)
{
	unsigned long group_load = get_group_load();

	gsc_update_status(group_load, now);

	gsc.last_update_ts = now;

	trace_gsc_decision_activate(group_load, now, gsc_status_name[gsc.status]);
}

void gsc_update_tick(void)
{
	u64 now = jiffies;

	if (!gsc.enabled)
		return;

	if (gsc.max_up_threshold == GSC_DISABLED)
		return;

	raw_spin_lock(&gsc.lock);

	if (gsc.status == GSC_LEVEL2)
		update_cpu_boost_state(now);

	/* If time hadn't passed by update_ms, don't have to proceed */
	if (now - gsc.last_update_ts < msecs_to_jiffies(gsc.update_ms)) {
		raw_spin_unlock(&gsc.lock);
		return;
	}

	gsc_decision_activate(now);

	raw_spin_unlock(&gsc.lock);
}

bool is_gsc_task(struct task_struct *p)
{
	if (!gsc.enabled)
		return false;

	if (!is_top_app(p))
		return false;

	return gsc.status != GSC_DEACTIVATED;
}

bool gsc_enabled(void)
{
	if (gsc.enabled)
		return true;
	else
		return false;
}

/********************************************************************************/
/* Cgroup Task List Handling							*/
/********************************************************************************/
static void gsc_enqueue_task(struct task_struct *p)
{
	struct gsc_task_env *gte;
	unsigned long flags;

	raw_spin_lock_irqsave(&gsc.lock, flags);

	gte = get_gte(p);
	if (gte == NULL)
		goto out;

	if (gte->attached)
		goto out;

	list_add(&gte->grp_list, &gsc.task_list);
	gte->attached = true;
out:
	raw_spin_unlock_irqrestore(&gsc.lock, flags);
}

static void gsc_dequeue_task(struct task_struct *p, bool flush)
{
	struct gsc_task_env *gte;
	unsigned long flags;

	raw_spin_lock_irqsave(&gsc.lock, flags);

	gte = get_gte(p);
	if (gte == NULL)
		goto out;

	if (!gte->attached)
		goto out;

	list_del_init(&gte->grp_list);
	gte->attached = false;

out:
	if (flush) {
		set_gte(p, 0);
		kfree(gte);
	}

	raw_spin_unlock_irqrestore(&gsc.lock, flags);
}

int gsc_manage_task_group(struct task_struct *p, bool attach, bool flush)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&p->pi_lock, flags);

	if (attach)
		gsc_enqueue_task(p);
	else
		gsc_dequeue_task(p, flush);

	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	return 0;
}

void gsc_init_new_task(struct task_struct *p)
{
	struct gsc_task_env *gte;

	if (!gsc.enabled)
		return;

	gte = kzalloc(sizeof(struct gsc_task_env), GFP_ATOMIC | GFP_NOWAIT);
	if (!gte) {
		pr_err("%s: Failed to allocate memory for gsc_task_env\n", __func__);
		return;
	}

	INIT_LIST_HEAD(&gte->grp_list);
	gte->attached = false;
	gte->p = p;
	set_gte(p, gte);

	gsc_manage_task_group(p, is_top_app(p), false);
}

void gsc_cpu_cgroup_attach(struct cgroup_taskset *tset)
{
	struct task_struct *p;
	struct cgroup_subsys_state *css;

	if (!gsc.enabled)
		return;

	cgroup_taskset_first(tset, &css);
	if (!css)
		return;

	cgroup_taskset_for_each(p, css, tset)
		gsc_manage_task_group(p, is_top_app(p), false);
}

void gsc_flush_task(struct task_struct *p)
{
	struct gsc_task_env *gte;

	if (!gsc.enabled)
		return;

	gte = get_gte(p);
	if (unlikely(!gte))
		return;

	gsc_manage_task_group(p, false, true);
}

static int gsc_init_handler(void *data)
{
	struct task_struct *g, *p;
	int cpu;

	for_each_possible_cpu(cpu)
		raw_spin_rq_lock(cpu_rq(cpu));

	do_each_thread(g, p) {
		gsc_init_new_task(p);
	} while_each_thread(g, p);

	for_each_possible_cpu(cpu)
		gsc_init_new_task(cpu_rq(cpu)->idle);

	for_each_possible_cpu(cpu)
		raw_spin_rq_unlock(cpu_rq(cpu));

	return 0;
}

static int gsc_emstune_notifier_call(struct notifier_block *nb,
		unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	unsigned int max_up_threshold = 0;
	int i;

	for (i = GSC_LEVEL1; i < GSC_LEVEL_COUNT; i++) {
		gsc.up_threshold[i] = cur_set->gsc.up_threshold[i];
		gsc.down_threshold[i] = cur_set->gsc.down_threshold[i];

		if (gsc.up_threshold[i] != GSC_DISABLED)
			max_up_threshold = max(max_up_threshold, gsc.up_threshold[i]);
	}

	gsc.max_up_threshold = max_up_threshold ? max_up_threshold : GSC_DISABLED;
	gsc.boost_en_threshold = cur_set->gsc.boost_en_threshold;
	gsc.boost_dis_threshold = cur_set->gsc.boost_dis_threshold;
	gsc.boost_ratio = cur_set->gsc.boost_ratio;

	if (gsc.max_up_threshold == GSC_DISABLED)
		gsc_disable();

	/* FIX ME: Should find more good way */
	gsc_disable_boost();

	return NOTIFY_OK;
}

static struct notifier_block gsc_emstune_notifier = {
	.notifier_call = gsc_emstune_notifier_call,
};

/********************************************************************************/
/* GSC Initialize								*/
/********************************************************************************/
static struct kobject *gsc_kobj;

static ssize_t state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	int cpu;

	ret += snprintf(buf + ret, PAGE_SIZE, "enabled: %2d\n", gsc.enabled);
	ret += snprintf(buf + ret, PAGE_SIZE, "active: %2d\n", gsc.status);

	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_clustergroup_mask(cpu)))
			continue;
		ret += snprintf(buf + ret, PAGE_SIZE, "cpu%2d boost: %2d\n",
							cpu, gsc.boost[cpu]);
		ret += snprintf(buf + ret, PAGE_SIZE, "cpu%2d boost_ts: %13lld\n",
							cpu, gsc.boost_deactive_ts[cpu]);
	}

	return ret;
}
static struct kobj_attribute state_attr = __ATTR_RO(state);

#define GSC_ATTR_RW(_name)						\
static ssize_t _name##_show(struct kobject *kobj,			\
	struct kobj_attribute *attr, char *buf)				\
{									\
	return snprintf(buf, 15, "%u\n", gsc._name);			\
}									\
									\
static ssize_t _name##_store(struct kobject *kobj,			\
	struct kobj_attribute *attr, const char *buf, size_t count)	\
{									\
	int input;							\
									\
	if (sscanf(buf, "%u", &input) != 1)				\
		return -EINVAL;						\
									\
	gsc._name = input;						\
									\
	return count;							\
}									\
static struct kobj_attribute _name##_attr = __ATTR_RW(_name);

GSC_ATTR_RW(update_ms);
GSC_ATTR_RW(retain_ms);
GSC_ATTR_RW(boost_en_threshold);
GSC_ATTR_RW(boost_dis_threshold);
GSC_ATTR_RW(boost_ratio);

static int gsc_sysfs_init(struct kobject *ems_kobj)
{
	gsc_kobj = kobject_create_and_add("gsc", ems_kobj);
	if (!gsc_kobj) {
		pr_info("%s: Failed to create node\n", __func__);
		return -EINVAL;
	}

	if (sysfs_create_file(gsc_kobj, &update_ms_attr.attr)) {
		pr_info("%s: Failed to create update_ms_attr\n", __func__);
		return -EINVAL;
	}

	if (sysfs_create_file(gsc_kobj, &retain_ms_attr.attr)) {
		pr_info("%s: Failed to create retain_ms_attr\n", __func__);
		return -EINVAL;
	}

	if (sysfs_create_file(gsc_kobj, &boost_en_threshold_attr.attr)) {
		pr_info("%s: Failed to create boost_en_threshold_attr\n", __func__);
		return -EINVAL;
	}

	if (sysfs_create_file(gsc_kobj, &boost_dis_threshold_attr.attr)) {
		pr_info("%s: Failed to create boost_dis_threshold_attr\n", __func__);
		return -EINVAL;
	}

	if (sysfs_create_file(gsc_kobj, &boost_ratio_attr.attr)) {
		pr_info("%s: Failed to create boost_ratio_attr\n", __func__);
		return -EINVAL;
	}

	if (sysfs_create_file(gsc_kobj, &state_attr.attr)) {
		pr_info("%s: Failed to create boost_ratio_attr\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static void init_gsc_group(void)
{
	gsc.status = GSC_DEACTIVATED;

	gsc.last_update_ts = 0;
	gsc.deactivated_ts = 0;
	gsc.expire_ns = DEFAULT_EXPIRE_NS;

	INIT_LIST_HEAD(&gsc.task_list);
	raw_spin_lock_init(&gsc.lock);

	stop_machine(gsc_init_handler, NULL, NULL);
}

static int gsc_parse_dt(struct device_node *dn)
{
	struct device_node *child;
	int i;

	if (of_property_read_u32(dn, "enabled", &gsc.enabled))
		return -ENODATA;

	for(i = GSC_LEVEL1; i < GSC_LEVEL_COUNT; i++) {
		gsc.up_threshold[i] = DEFAULT_UP_THRESHOLD;
		gsc.down_threshold[i] = DEFAULT_DOWN_THRESHOLD;
	}

	for_each_child_of_node(dn, child) {
		int level;

		if (of_property_read_u32(child, "level", &level)) {
			pr_err("%s: gsc level property is omitted\n", __func__);
			continue;
		}

		if (of_property_read_u32(child, "up_threshold", &gsc.up_threshold[level]))
			gsc.up_threshold[level] = DEFAULT_UP_THRESHOLD;

		if (of_property_read_u32(child, "down_threshold", &gsc.down_threshold[level]))
			gsc.down_threshold[level] = DEFAULT_DOWN_THRESHOLD;
	}

	if (of_property_read_u32(dn, "update_ms", &gsc.update_ms))
		gsc.update_ms = DEFAULT_UPDATE_MS;

	if (of_property_read_u32(dn, "retain_ms", &gsc.retain_ms))
		gsc.retain_ms = DEFAULT_RETAIN_MS;

	if (of_property_read_u32(dn, "boost_en_threshold", &gsc.boost_en_threshold))
		gsc.boost_en_threshold = DEFAULT_BOOST_EN_THR;

	if (of_property_read_u32(dn, "boost_dis_threshold", &gsc.boost_dis_threshold))
		gsc.boost_dis_threshold = DEFAULT_BOOST_DIS_THR;

	if (of_property_read_u32(dn, "boost_ratio", &gsc.boost_ratio))
		gsc.boost_ratio = DEFAULT_BOOST_RATIO;

	return 0;
}

void gsc_init(struct kobject *ems_kobj)
{
	struct device_node *dn;

	dn = of_find_node_by_path("/ems/gsc");
	if (!dn) {
		pr_info("failed to find device node for gsc\n");
		return;
	}

	if (gsc_parse_dt(dn)) {
		pr_info("failed to parse from dt for gsc\n");
		return;
	}

	init_gsc_group();

	gsc_sysfs_init(ems_kobj);

	emstune_register_notifier(&gsc_emstune_notifier);

	pr_info("EMS: Initialized GSC\n");
}
