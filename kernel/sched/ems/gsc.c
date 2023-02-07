/*
 * Exynos Mobile Scheduler Group Scheduling
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd
 */

#include <dt-bindings/soc/samsung/ems.h>
#include "../sched.h"
#include "../pelt.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

/******************************************************************************
 * GSC (Group Scheduling based on Cgroup)                                     *
 ******************************************************************************/
#define TARGET_GSC_GROUP_ID		(1)
#define GSC_TASK_GET(task)		(task->android_vendor_data1[23])
#define GSC_TASK_SET(task, gte)		(task->android_vendor_data1[23] = gte)
#define GSC_DEF_STATUS			(0)
#define GSC_PERF_STATUS			(1)
#define DEFAULT_MIN_INTERVAL_US		(100000)
#define DEFAULT_UP_THRESHOLD		(1024)
#define DEFAULT_DOWN_THRESHOLD		(1024)
#define DEFAULT_MIN_RETAIN_US		(100000)
#define DEFAULT_MIN_ADMIT_NS		(100000000)

struct gsc_group {
	int			id;
	raw_spinlock_t		lock;
	struct list_head	task_list;
	struct rcu_head		rcu;
	int			curr_stat;
	u64			last_update_time;
	u64			deactivated_t;
	u64			activated_t;
};

struct gsc_task_env {
	struct task_struct	*p;
	struct gsc_group __rcu	*grp;
	struct list_head	grp_list;
	int			gid;
};

struct gsc_stat {
	unsigned int enabled;
	unsigned int up_threshold;
	unsigned int down_threshold;
	unsigned int min_interval_us;
	unsigned int min_retain_us;
	unsigned int min_admit_ns;
} gsc_stats;

static struct gsc_group *gsc_groups[CGROUP_COUNT];
int gsc_enabled[CGROUP_COUNT];

int gsc_task_enabled(struct task_struct *p)
{
	int group_idx = cpuctl_task_group_idx(p);
	return gsc_enabled[group_idx];
}

static inline struct gsc_group
*get_task_gsc_group(struct task_struct *p)
{
	struct gsc_task_env *gte = (struct gsc_task_env *) GSC_TASK_GET(p);

	return gte ? rcu_dereference(gte->grp) : NULL;
}

void gsc_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus)
{
	struct gsc_group *grp = NULL;
	int stat = 0;

	if (!gsc_stats.enabled)
		return;

	rcu_read_lock();

	grp = get_task_gsc_group(p);
	if (grp)
		stat = grp->curr_stat;

	rcu_read_unlock();

	cpumask_clear(fit_cpus);
	cpumask_copy(fit_cpus, cpu_active_mask);

	if (stat == GSC_PERF_STATUS)
		cpumask_andnot(fit_cpus, fit_cpus, cpu_slowest_mask());
}

static inline struct gsc_group *gsc_get_group(unsigned int gid)
{
	return gsc_groups[gid];
}

void gsc_update_stat(struct gsc_group *grp,
			 unsigned long group_util, bool boost)
{
	if (grp->curr_stat == GSC_DEF_STATUS) {
		if (group_util > gsc_stats.up_threshold)
			grp->curr_stat = GSC_PERF_STATUS;
		return;
	}

	if (group_util < gsc_stats.down_threshold) {
		if (!grp->deactivated_t) {
			grp->deactivated_t = grp->last_update_time;
			return;
		}

		if (grp->last_update_time - grp->deactivated_t >
				usecs_to_jiffies(gsc_stats.min_retain_us)) {
			grp->deactivated_t = 0;
			grp->curr_stat = GSC_DEF_STATUS;
		}
	} else if (grp->deactivated_t)
		grp->deactivated_t = 0;
}

static void _gsc_decision_activate(struct gsc_group *grp)
{
	struct task_struct *p;
	unsigned long group_util = 0;
	unsigned long util = 0;
	bool prev_stat = grp->curr_stat;
	struct gsc_task_env *gte;
	struct sched_entity *se = NULL;
	u64 now = jiffies;
	u64 delta = 0, curr = 0;

	if (list_empty(&grp->task_list)) {
		grp->curr_stat = GSC_DEF_STATUS;
		goto out;
	}

	if (now < grp->last_update_time + usecs_to_jiffies(gsc_stats.min_interval_us))
		return;

	list_for_each_entry(gte, &grp->task_list, grp_list) {
		p = gte->p;
		se = &p->se;
		curr = cfs_rq_clock_pelt(cfs_rq_of(se));

		delta = curr - se->avg.last_update_time;
		delta = max_t(u64, delta, 0);
		if (delta > gsc_stats.min_admit_ns)
			continue;

		util = max_t(unsigned long, ml_task_util_est(p), 0);
		if (util <= 0)
			continue;

		group_util += util;

		if (group_util > gsc_stats.up_threshold)
			break;
	}

	grp->last_update_time = now;
	gsc_update_stat(grp, group_util, 0);

out:
	if (grp->id == TARGET_GSC_GROUP_ID &&
		grp->curr_stat != prev_stat) {
		if (grp->curr_stat == GSC_PERF_STATUS)
			grp->activated_t = now;
		else
			grp->activated_t = 0;
	}

	trace_gsc_decision_activate_result(grp->id, group_util, now, grp->last_update_time,
					  grp->curr_stat ? "true" : "false");
}

static void gsc_decision_activate(struct gsc_group *grp)
{
	raw_spin_lock(&grp->lock);
	_gsc_decision_activate(grp);
	raw_spin_unlock(&grp->lock);
}

static int gsc_task_update_available(struct gsc_group *grp,
	struct task_struct *p, unsigned long prv_util)
{
	unsigned long new_util = ml_task_util_est(p);
	u64 now = jiffies;

	if (!grp)
		return 0;

	if (abs(new_util - prv_util) > (gsc_stats.up_threshold >> 2) ||
		now - grp->last_update_time > usecs_to_jiffies(gsc_stats.min_interval_us))
		return 1;

	return 0;
}

void gsc_task_update_stat(struct task_struct *p,
				  unsigned long prv_util)
{
	struct gsc_task_env *gte = (struct gsc_task_env *) GSC_TASK_GET(p);
	struct gsc_group *grp = NULL;

	if (unlikely(gte == NULL))
		return;

	rcu_read_lock();
	grp = get_task_gsc_group(p);
	if (gsc_task_update_available(grp, p, prv_util))
		gsc_decision_activate(grp);
	rcu_read_unlock();
}

void gsc_update_tick(struct rq *rq)
{
	unsigned long prv_util;

	if (!gsc_stats.enabled)
		return;

	prv_util = ml_task_util_est(rq->curr);
	gsc_task_update_stat(rq->curr, prv_util);
}

static void gsc_enqueue_task(struct task_struct *p, struct gsc_group *grp, int gid)
{
	struct gsc_task_env *gte = (struct gsc_task_env *) GSC_TASK_GET(p);
	unsigned long flags;

	if (unlikely(gte == NULL))
		return;

	raw_spin_lock_irqsave(&grp->lock, flags);

	list_add(&gte->grp_list, &grp->task_list);
	rcu_assign_pointer(gte->grp, grp);

	raw_spin_unlock_irqrestore(&grp->lock, flags);
}

void gsc_dequeue_task(struct task_struct *p)
{
	struct gsc_task_env *gte = (struct gsc_task_env *) GSC_TASK_GET(p);
	struct gsc_group *grp = gte->grp;
	unsigned long flags;

	if (unlikely(gte == NULL) || grp == NULL)
		return;

	raw_spin_lock_irqsave(&grp->lock, flags);

	gte->gid = 0;
	list_del_init(&gte->grp_list);
	rcu_assign_pointer(gte->grp, NULL);

	raw_spin_unlock_irqrestore(&grp->lock, flags);
}

int gsc_task_attach_group(struct task_struct *p, unsigned int gid)
{
	unsigned long flags;
	struct gsc_group *grp = NULL;
	struct gsc_task_env *gte = (struct gsc_task_env *) GSC_TASK_GET(p);

	if (unlikely(gte == NULL))
		return -EINVAL;

	if (gid >= CGROUP_COUNT) {
		pr_err("%s: Invalid group id %d.\n", __func__, gid);
		return -EINVAL;
	}

	raw_spin_lock_irqsave(&p->pi_lock, flags);

	if ((!gte->grp && !gid) || (gte->grp && gid))
		goto done;

	if (!gid) {
		gsc_dequeue_task(p);
		goto done;
	}

	gte->gid = gid;
	grp = gsc_get_group(gid);

	gsc_enqueue_task(p, grp, gid);
done:
	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	return 0;
}

void gsc_enqueue_new_task(struct task_struct *p)
{
	struct gsc_group *grp;
	struct gsc_task_env *gte = (struct gsc_task_env *) GSC_TASK_GET(p);
	unsigned long flags;

	if (unlikely(gte == NULL)|| !gsc_task_enabled(p))
		return;

	if (!gte->gid)
		gte->gid = TARGET_GSC_GROUP_ID;

	grp = gsc_get_group(TARGET_GSC_GROUP_ID);
	if (!gsc_task_enabled(p) || gte->grp) {
		if (gte->gid)
			gte->gid = 0;
		return;
	}

	raw_spin_lock_irqsave(&grp->lock, flags);

	rcu_assign_pointer(gte->grp, grp);
	list_add(&gte->grp_list, &grp->task_list);

	raw_spin_unlock_irqrestore(&grp->lock, flags);
}

void gsc_init_new_task(struct task_struct *p)
{
	struct gsc_task_env *gte = NULL;

	if (!gsc_stats.enabled)
		return;

	gte = kzalloc(sizeof(struct gsc_task_env), GFP_ATOMIC | GFP_NOWAIT);
	if (!gte) {
		pr_err("%s: Fail to allocate memory for gsc. gte=%p\n", __func__, gte);
		WARN_ON(!gte);
		return;
	}

	INIT_LIST_HEAD(&gte->grp_list);
	rcu_assign_pointer(gte->grp, NULL);
	gte->p = p;

	GSC_TASK_SET(p, (u64) gte);
	gsc_enqueue_new_task(p);
}

void gsc_task_cgroup_attach(struct cgroup_taskset *tset)
{
	struct task_struct *p;
	struct cgroup_subsys_state *css;
	unsigned int gid;
	int ret;

	if (!gsc_stats.enabled)
		return;

	cgroup_taskset_first(tset, &css);
	if (!css)
		return;

	cgroup_taskset_for_each(p, css, tset) {
		gid = gsc_task_enabled(p) ? TARGET_GSC_GROUP_ID : 0;
		ret = gsc_task_attach_group(p, gid);
		trace_gsc_task_cgroup_attach(p, gid, ret);
	}
}

void gsc_flush_task(struct task_struct *p)
{
	struct gsc_task_env *gte = (struct gsc_task_env *) GSC_TASK_GET(p);
	int ret = -1;

	if (!gsc_stats.enabled)
		return;

	if (gte->grp == NULL)
		goto out;

	ret = gsc_task_attach_group(p, 0);
out:
	kfree(gte);

	trace_gsc_flush_task(p, ret);
}

static int gsc_init_handler(void *data)
{
	struct task_struct *g, *p;
	int cpu;

	for_each_possible_cpu(cpu)
		raw_spin_lock(&cpu_rq(cpu)->lock);

	do_each_thread(g, p) {
		gsc_init_new_task(p);
	} while_each_thread(g, p);

	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);

		gsc_init_new_task(rq->idle);
	}

	for_each_possible_cpu(cpu)
		raw_spin_unlock(&cpu_rq(cpu)->lock);

	return 0;
}

/**********************************************************************
 *			    SYSFS support			      *
 **********************************************************************/
static struct kobject *gsc_kobj;

/* min_interval_us */
static ssize_t show_min_interval_us(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%d\n", gsc_stats.min_interval_us);
}

static ssize_t store_min_interval_us(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	gsc_stats.min_interval_us = input;

	return count;
}

static struct kobj_attribute min_interval_us_attr =
__ATTR(min_interval_us, 0644, show_min_interval_us, store_min_interval_us);

/* up_threshold */
static ssize_t show_up_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", gsc_stats.up_threshold);
}

static ssize_t store_up_threshold(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	gsc_stats.up_threshold = input;

	return count;
}

static struct kobj_attribute up_threshold_attr =
__ATTR(up_threshold, 0644, show_up_threshold, store_up_threshold);

/* down_threshold */
static ssize_t show_down_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", gsc_stats.down_threshold);
}

static ssize_t store_down_threshold(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	gsc_stats.down_threshold = input;

	return count;
}

static struct kobj_attribute down_threshold_attr =
__ATTR(down_threshold, 0644, show_down_threshold, store_down_threshold);

/* min_retain_us */
static ssize_t show_min_retain_us(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", gsc_stats.min_retain_us);
}

static ssize_t store_min_retain_us(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int input;

	if (sscanf(buf, "%d", &input) != 1)
		return -EINVAL;

	gsc_stats.min_retain_us = input;

	return count;
}

static struct kobj_attribute min_retain_us_attr =
__ATTR(min_retain_us, 0644, show_min_retain_us, store_min_retain_us);

/* min_admit_ns */
static ssize_t show_min_admit_ns(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%llu\n", gsc_stats.min_admit_ns);
}

static ssize_t store_min_admit_ns(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long input;

	if (sscanf(buf, "%llu", &input) != 1)
		return -EINVAL;

	gsc_stats.min_admit_ns = input;

	return count;
}

static struct kobj_attribute min_admit_ns_attr =
__ATTR(min_admit_ns, 0644, show_min_admit_ns, store_min_admit_ns);

static int gsc_sysfs_init(struct kobject *ems_kobj)
{
	int ret;

	gsc_kobj = kobject_create_and_add("gsc", ems_kobj);
	if (!gsc_kobj) {
		pr_info("%s: fail to create node\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(gsc_kobj, &min_interval_us_attr.attr);
	if (ret)
		pr_warn("%s: failed to create gsc sysfs, line %u\n", __func__, __LINE__);

	ret = sysfs_create_file(gsc_kobj, &up_threshold_attr.attr);
	if (ret)
		pr_warn("%s: failed to create gsc sysfs, line %u\n", __func__, __LINE__);

	ret = sysfs_create_file(gsc_kobj, &down_threshold_attr.attr);
	if (ret)
		pr_warn("%s: failed to create gsc sysfs, line %u\n", __func__, __LINE__);

	ret = sysfs_create_file(gsc_kobj, &min_retain_us_attr.attr);
	if (ret)
		pr_warn("%s: failed to create gsc sysfs, line %u\n", __func__, __LINE__);

	ret = sysfs_create_file(gsc_kobj, &min_admit_ns_attr.attr);
	if (ret)
		pr_warn("%s: failed to create gsc sysfs, line %u\n", __func__, __LINE__);

	return ret;
}

static void init_gsc_group(void)
{
	struct gsc_group *grp;
	int i;

	for (i = 0; i < CGROUP_COUNT; i++) {
		grp = kzalloc(sizeof(struct gsc_group), GFP_ATOMIC | GFP_NOWAIT);
		if (!grp) {
			pr_err("%s: Fail to allocate memory for gsc. grp=%p\n", __func__, grp);
			WARN_ON(!grp);
			return;
		}
		grp->id = i;
		INIT_LIST_HEAD(&grp->task_list);
		raw_spin_lock_init(&grp->lock);

		gsc_groups[i] = grp;
		gsc_enabled[i] = (i == CGROUP_TOPAPP) ? 1 : 0;
	}

	stop_machine(gsc_init_handler, NULL, NULL);
}

static int gsc_parse_dt(struct device_node *dn)
{
	if (of_property_read_u32(dn, "enabled", &gsc_stats.enabled))
		return -ENODATA;

	if (of_property_read_u32(dn, "up_threshold", &gsc_stats.up_threshold))
		gsc_stats.up_threshold = DEFAULT_UP_THRESHOLD;

	if (of_property_read_u32(dn, "down_threshold", &gsc_stats.down_threshold))
		gsc_stats.down_threshold = DEFAULT_DOWN_THRESHOLD;

	if (of_property_read_u32(dn, "min_interval_us", &gsc_stats.min_interval_us))
		gsc_stats.min_interval_us = DEFAULT_MIN_INTERVAL_US;

	if (of_property_read_u32(dn, "min_retain_us", &gsc_stats.min_retain_us))
		gsc_stats.min_retain_us = DEFAULT_MIN_RETAIN_US;

	if (of_property_read_u32(dn, "min_admit_ns", &gsc_stats.min_admit_ns))
		gsc_stats.min_admit_ns = DEFAULT_MIN_ADMIT_NS;

	return 0;
}

void gsc_init(struct kobject *ems_kobj)
{
	struct device_node *dn;

	dn = of_find_node_by_path("/ems/gsc");
	if (!dn) {
		pr_err("failed to find device node for gsc\n");
		return;
	}

	if (gsc_parse_dt(dn)) {
		pr_err("failed to parse from dt for gsc\n");
		return;
	}

	init_gsc_group();
	gsc_sysfs_init(ems_kobj);

	pr_info("EMS: Initialized GSC\n");
}
