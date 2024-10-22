// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "eas_plus.h"
#include "common.h"
#include "flt_init.h"
#include "flt_api.h"
#include "group.h"
#include "flt_cal.h"
#include "eas_trace.h"
#include <sugov/cpufreq.h>
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v2/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif
#define DEFAULT_GRP_THRESHOLD	20
#define DEFAULT_GRP_THRESHOLD_UTIL	460
static struct grp *related_thread_groups[GROUP_ID_RECORD_MAX];
static u32 GP_mode = GP_MODE_0;
static int grp_threshold[GROUP_ID_RECORD_MAX] = {DEFAULT_GRP_THRESHOLD, DEFAULT_GRP_THRESHOLD,
						DEFAULT_GRP_THRESHOLD, DEFAULT_GRP_THRESHOLD};
static int grp_threshold_util[GROUP_ID_RECORD_MAX] = {DEFAULT_GRP_THRESHOLD_UTIL,
						DEFAULT_GRP_THRESHOLD_UTIL,
						DEFAULT_GRP_THRESHOLD_UTIL,
						DEFAULT_GRP_THRESHOLD_UTIL};

static DEFINE_RWLOCK(related_thread_group_lock);

static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, (ue.enqueued & ~UTIL_AVG_UNCHANGED));
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	return max(task_util(p), _task_util_est(p));
}

inline struct grp *lookup_grp(int grp_id)
{
	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return NULL;
	else
		return related_thread_groups[grp_id];
}
EXPORT_SYMBOL(lookup_grp);

inline struct grp *task_grp(struct task_struct *p)
{
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;

	return rcu_dereference(gts->grp);
}

static int alloc_related_thread_groups(void)
{
	int i;
	struct grp *grp;

	for (i = 0; i < GROUP_ID_RECORD_MAX; i++) {
		grp = kzalloc(sizeof(*grp), GFP_ATOMIC | GFP_NOWAIT);
		if (!grp)
			return -1;

		grp->id = i;
		grp->ws = GRP_DEFAULT_WS;
		grp->wc = GRP_DEFAULT_WC;
		grp->wp = GRP_DEFAULT_WP;

		INIT_LIST_HEAD(&grp->tasks);
		INIT_LIST_HEAD(&grp->list);
		raw_spin_lock_init(&grp->lock);

		related_thread_groups[i] = grp;
	}

	return 0;
}

static void free_related_thread_groups(void)
{
	int grp_idx;
	struct grp *grp = NULL;

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
		grp = lookup_grp(grp_idx);
			kfree(grp);
	}
}

inline int cgrp_to_grpid(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	int groupid = -1;
	struct cgrp_tg *cgrptg;

	rcu_read_lock();
	css = task_css(p, cpu_cgrp_id);
	if (!css)
		goto unlock;

	tg = container_of(css, struct task_group, css);
	cgrptg = &((struct mtk_tg *)tg->android_vendor_data1)->cgrp_tg;
	if (cgrptg->colocate)
		groupid = cgrptg->groupid;

unlock:
	rcu_read_unlock();
	return groupid;
}

static void remove_task_from_group(struct task_struct *p)
{
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;
	struct grp *grp = gts->grp;
	int empty_group = 1;
	struct rq *rq;
	struct rq_flags rf;
	struct flt_rq *fsrq;
	int flt_groupid = grp->id, queued;

	raw_spin_lock(&grp->lock);

	rq = __task_rq_lock(p, &rf);
	rcu_assign_pointer(gts->grp, NULL);
	/* take care of flt group */
	queued = task_on_rq_queued(p);
	fsrq = &per_cpu(flt_rq, rq->cpu);
	if (queued && fsrq->group_nr_running[flt_groupid] > 0)
		fsrq->group_nr_running[flt_groupid]--;
	__task_rq_unlock(rq, &rf);

	if (!list_empty(&grp->tasks))
		empty_group = 0;

	raw_spin_unlock(&grp->lock);
}

static int
add_task_to_group(struct task_struct *p, struct grp *grp)
{
	struct rq *rq;
	struct rq_flags rf;
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;
	struct flt_rq *fsrq;
	int flt_groupid = grp->id, queued;

	raw_spin_lock(&grp->lock);

	/*
	 * Change gts->grp under rq->lock. Will prevent races with read-side
	 * reference of gts->grp in various hot-paths
	 */
	rq = __task_rq_lock(p, &rf);
	rcu_assign_pointer(gts->grp, grp);
	/* take care of flt group */
	fsrq = &per_cpu(flt_rq, rq->cpu);
	queued = task_on_rq_queued(p);
	if (queued)
		fsrq->group_nr_running[flt_groupid]++;
	__task_rq_unlock(rq, &rf);

	raw_spin_unlock(&grp->lock);

	return 0;
}

int __sched_set_grp_id(struct task_struct *p, int group_id)
{
	int rc = 0;
	unsigned long flags;
	struct grp *grp = NULL;
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;

	if (group_id >= GROUP_ID_RECORD_MAX)
		return -1;

	raw_spin_lock_irqsave(&p->pi_lock, flags);
	write_lock(&related_thread_group_lock);

	/* Switching from one group to another directly is not permitted */
	if ((!gts->grp && group_id < 0) || (gts->grp && group_id >= 0))
		goto done;

	/* remove */
	if (group_id < 0) {
		remove_task_from_group(p);
		goto done;
	}

	grp = lookup_grp(group_id);
	rc = add_task_to_group(p, grp);

done:
	write_unlock(&related_thread_group_lock);
	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	return rc;
}

static void init_topapp_tg(struct task_group *tg)
{
	struct cgrp_tg *cgrptg;

	cgrptg = &((struct mtk_tg *)tg->android_vendor_data1)->cgrp_tg;

	cgrptg->colocate = true;
	cgrptg->groupid = GROUP_ID_1;
	set_group_pd(TA_GRPID, cgrptg->groupid + FLT_GROUP_START_IDX);
}

static void init_foreground_tg(struct task_group *tg)
{
	struct cgrp_tg *cgrptg;

	cgrptg = &((struct mtk_tg *)tg->android_vendor_data1)->cgrp_tg;

	cgrptg->colocate = false;
	cgrptg->groupid = -1;
}

static void init_tg(struct task_group *tg)
{
	struct cgrp_tg *cgrptg;

	cgrptg = &((struct mtk_tg *)tg->android_vendor_data1)->cgrp_tg;

	cgrptg->colocate = false;
	cgrptg->groupid = -1;
}

static inline struct task_group *css_tg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct task_group, css) : NULL;
}

static void group_update_tg_pointer(struct cgroup_subsys_state *css)
{
	if (!strcmp(css->cgroup->kn->name, "top-app"))
		init_topapp_tg(css_tg(css));
	else if (!strcmp(css->cgroup->kn->name, "foreground"))
		init_foreground_tg(css_tg(css));
	else
		init_tg(css_tg(css));
}

static void group_init_tg_pointers(void)
{
	struct cgroup_subsys_state *css = &root_task_group.css;
	struct cgroup_subsys_state *top_css = css;

	rcu_read_lock();
	css_for_each_child(css, top_css)
		group_update_tg_pointer(css);
	rcu_read_unlock();
}

static void add_new_task_to_grp(struct task_struct *new, enum _GP_trace GP_trace)
{
	int groupid, ret;
	struct gp_task_struct *gts = &((struct mtk_task *)new->android_vendor_data1)->gp_task;

	if (gts->customized)
		return;
	groupid = cgrp_to_grpid(new);
	ret = __sched_set_grp_id(new, groupid);
	if (trace_sched_task_to_grp_enabled())
		trace_sched_task_to_grp(new, groupid, ret, GP_trace);
}

static void group_init_new_task_load(struct task_struct *p)
{
	struct gp_task_struct *gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;

	rcu_assign_pointer(gts->grp, NULL);
}

static void group_init_existing_task_load(struct task_struct *p)
{
	group_init_new_task_load(p);
}

static void group_android_rvh_wake_up_new_task(void *unused, struct task_struct *new)
{
	/* init new task grp & list */
	group_init_new_task_load(new);

	if (unlikely(group_get_mode() == GP_MODE_0))
		return;
	add_new_task_to_grp(new, GP_WAUPNEW);
}

static void group_android_rvh_cpu_cgroup_online(void *unused, struct cgroup_subsys_state *css)
{
	if (unlikely(group_get_mode() == GP_MODE_0))
		return;

	group_update_tg_pointer(css);
}

static void group_android_rvh_cpu_cgroup_attach(void *unused,
						struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;
	struct task_group *tg;
	struct cgrp_tg *cgrptg;
	int ret, grp_id;
	struct gp_task_struct *gts;

	if (unlikely(group_get_mode() == GP_MODE_0))
		return;

	cgroup_taskset_first(tset, &css);
	if (!css)
		return;

	tg = container_of(css, struct task_group, css);
	cgrptg = &((struct mtk_tg *)tg->android_vendor_data1)->cgrp_tg;

	cgroup_taskset_for_each(task, css, tset) {
		grp_id = cgrptg->colocate ? cgrptg->groupid : -1;
		gts = &((struct mtk_task *)task->android_vendor_data1)->gp_task;
		if (gts->customized)
			continue;
		ret = __sched_set_grp_id(task, grp_id);
		if (trace_sched_task_to_grp_enabled())
			trace_sched_task_to_grp(task, grp_id, ret, GP_CGROUP);
	}

	if (cgrptg->colocate) {
		if ((!strcmp(css->cgroup->kn->name, "top-app")))
			set_group_pd(TA_GRPID, grp_id + FLT_GROUP_START_IDX);
	}
}

static void group_android_rvh_try_to_wake_up_success(void *unused,
							struct task_struct *p)
{
	if (unlikely(group_get_mode() == GP_MODE_0))
		return;
	/* gp setting for wake up task */
	add_new_task_to_grp(p, GP_TTWU);
}

int get_grp_id(struct task_struct *p)
{
	int grp_id;
	struct grp *grp;

	rcu_read_lock();
	grp = task_grp(p);
	grp_id = grp ? grp->id : -1;
	rcu_read_unlock();

	return grp_id;
}
EXPORT_SYMBOL(get_grp_id);

inline bool check_and_get_grp_id(struct task_struct *p, int *grp_id)
{
	bool ret = false;
	*grp_id = get_grp_id(p);
	if (*grp_id >= GROUP_ID_1 && *grp_id <= GROUP_ID_4)
		ret = true;

	return ret;
}
EXPORT_SYMBOL(check_and_get_grp_id);

int __set_task_to_group(int pid, int grp_id)
{
	struct task_struct *p;
	int ret = -1;
	struct gp_task_struct *gts;

	if (grp_id >= GROUP_ID_RECORD_MAX)
		return ret;
	if (unlikely(group_get_mode() == GP_MODE_0))
		return ret;
	p = get_pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!p)
		return ret;
	gts = &((struct mtk_task *)p->android_vendor_data1)->gp_task;
	if (grp_id < 0)
		gts->customized = false;
	else
		gts->customized = true;
	ret = __sched_set_grp_id(p, grp_id);
	if (trace_sched_task_to_grp_enabled())
		trace_sched_task_to_grp(p, grp_id, ret, GP_API);
	put_task_struct(p);

	if (grp_id < 0)
		ret = set_task_pd(pid, -1);
	else
		ret = set_task_pd(pid, grp_id + FLT_GROUP_START_IDX);

	return ret;
}

/* Allow user set task to grp (0,1,2,3) */
int set_task_to_group(int pid, int grp_id)
{
	return __set_task_to_group(pid, grp_id);
}
EXPORT_SYMBOL(set_task_to_group);

static void group_android_rvh_flush_task(void *unused, struct task_struct *p)
{
	int ret = 0;

	if (unlikely(group_get_mode() == GP_MODE_0))
		return;

	if (trace_sched_task_to_grp_enabled())
		trace_sched_task_to_grp(p, -1, ret, GP_DEAD);
}

static void group_register_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_wake_up_new_task(
		group_android_rvh_wake_up_new_task, NULL);
	if (ret)
		pr_info("register wake_up_new_task hooks failed, returned %d\n", ret);

	register_trace_android_rvh_flush_task(
		group_android_rvh_flush_task, NULL);
	if (ret)
		pr_info("register flush_task hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_cpu_cgroup_attach(
		group_android_rvh_cpu_cgroup_attach, NULL);
	if (ret)
		pr_info("register cpu_cgroup_attach hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_cpu_cgroup_online(
		group_android_rvh_cpu_cgroup_online, NULL);
	if (ret)
		pr_info("register cpu_cgroup_online hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_try_to_wake_up_success(
		group_android_rvh_try_to_wake_up_success, NULL);
	if (ret)
		pr_info("register try_to_wake_up_success hooks failed, returned %d\n", ret);
}

void group_init(void)
{
	struct task_struct *g, *p;
	int cpu;
	u64 window_start_ns, nr_windows;

	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;

	/* gp alloc fail */
	if (alloc_related_thread_groups() != 0)
		return;

	/* default tracking mode */
	group_set_mode(GP_MODE_1);

	/* for existing thread */
	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		group_init_existing_task_load(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);

	/* init for each cpu */
	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);

		/* Create task members for idle thread */
		group_init_new_task_load(rq->idle);
	}
	/* init irq work */
	window_start_ns = ktime_get_ns();
	nr_windows = div64_u64(window_start_ns, GRP_DEFAULT_WS);
	window_start_ns = (u64)nr_windows * (u64)GRP_DEFAULT_WS;

	group_init_tg_pointers();
	group_register_hooks();
}

void group_exit(void)
{
	free_related_thread_groups();
}

void  group_set_mode(u32 mode)
{
	GP_mode = mode;
}
EXPORT_SYMBOL(group_set_mode);

u32 group_get_mode(void)
{
	return GP_mode;
}
EXPORT_SYMBOL(group_get_mode);

void  group_update_threshold_util(int wl)
{
	int weight, cpu, grp_idx;
	struct cpumask *gear_cpus;
	struct mtk_em_perf_state *ps = NULL;

	gear_cpus = get_gear_cpumask(0);
	cpu = cpumask_first(gear_cpus);
	weight = cpumask_weight(gear_cpus);
	ps = pd_get_opp_ps(wl, cpu, 0, false);

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx)
		grp_threshold_util[grp_idx] =
		((ps->capacity * weight) * grp_threshold[grp_idx]) / 100;
}

int  group_get_threshold_util(int grp_id)
{
	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return -1;
	return grp_threshold_util[grp_id];
}
EXPORT_SYMBOL(group_get_threshold_util);

int  group_set_threshold(int grp_id, int val)
{
	if (grp_id >= GROUP_ID_RECORD_MAX || val > 100 || val < 0 || grp_id < 0)
		return -1;
	grp_threshold[grp_id] = val;
	return 0;
}
EXPORT_SYMBOL(group_set_threshold);

int  group_reset_threshold(int grp_id)
{
	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return -1;
	grp_threshold[grp_id] = DEFAULT_GRP_THRESHOLD;
	return 0;
}
EXPORT_SYMBOL(group_reset_threshold);

int  group_get_threshold(int grp_id)
{
	int ret = 0;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return ret;
	return grp_threshold[grp_id];
}
EXPORT_SYMBOL(group_get_threshold);

inline bool group_get_gear_hint(struct task_struct *p)
{
	struct grp *grp;
	bool ret = false;

	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return ret;

	rcu_read_lock();

	grp = task_grp(p);
	if (grp)
		ret = grp->gear_hint;

	rcu_read_unlock();

	return ret;
}
