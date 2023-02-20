/*
 * Multi-purpose Load tracker
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include "../sched.h"
#include "../sched-pelt.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

/******************************************************************************
 *                           MULTI LOAD for TASK                              *
 ******************************************************************************/
/*
 * ml_task_util - task util
 *
 * Task utilization. The calculation is the same as the task util of cfs.
 */
unsigned long ml_task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

int ml_task_hungry(struct task_struct *p)
{
	return 0; /* HACK */
}

/*
 * ml_task_util_est - task util with util-est
 *
 * Task utilization with util-est, The calculation is the same as
 * task_util_est of cfs.
 */
static unsigned long _ml_task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, (ue.enqueued & ~UTIL_AVG_UNCHANGED));
}

unsigned long ml_task_util_est(struct task_struct *p)
{
	return max(ml_task_util(p), _ml_task_util_est(p));
}

/*
 * ml_task_load_avg - task load_avg
 */
unsigned long ml_task_load_avg(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.load_avg);
}

/******************************************************************************
 *                            MULTI LOAD for CPU                              *
 ******************************************************************************/
/*
 * ml_cpu_util - cpu utilization
 */
unsigned long ml_cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	return min_t(unsigned long, util, capacity_cpu_orig(cpu));
}

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

/*
 * ml_cpu_util_without - cpu utilization without waking task
 */
unsigned long ml_cpu_util_without(int cpu, struct task_struct *p)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned int util = READ_ONCE(cfs_rq->avg.util_avg);

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return util;

	/* Discount task's util from CPU's util */
	lsub_positive(&util, ml_task_util(p));

	return min_t(unsigned long, util, capacity_cpu_orig(cpu));
}

/*
 * ml_cpu_util_with - cpu utilization with waking task
 */
unsigned long ml_cpu_util_with(struct task_struct *p, int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util = READ_ONCE(cfs_rq->avg.util_avg);

	/* Discount task's util from prev CPU's util */
	if (cpu == task_cpu(p))
		lsub_positive(&util, ml_task_util(p));

	util += max(ml_task_util_est(p), (unsigned long)1);

	return min(util, capacity_cpu_orig(cpu));
}

/*
 * ml_cpu_load_avg - cpu load_avg
 */
unsigned long ml_cpu_load_avg(int cpu)
{
	return cpu_rq(cpu)->cfs.avg.load_avg;
}

/******************************************************************************
 *                     New task utilization init                              *
 ******************************************************************************/
static int ntu_ratio[CGROUP_COUNT];

void ntu_apply(struct sched_entity *se)
{
	struct sched_avg *sa = &se->avg;
	int grp_idx = cpuctl_task_group_idx(task_of(se));
	int ratio = ntu_ratio[grp_idx];

	sa->util_avg = sa->util_avg * ratio / 100;
	sa->runnable_avg = sa->runnable_avg * ratio / 100;
}

static int
ntu_emstune_notifier_call(struct notifier_block *nb,
					   unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		ntu_ratio[i] = cur_set->ntu.ratio[i];

	return NOTIFY_OK;
}

static struct notifier_block ntu_emstune_notifier = {
	.notifier_call = ntu_emstune_notifier_call,
};

void ntu_init(struct kobject *ems_kobj)
{
	int i;

	for (i = 0; i < CGROUP_COUNT; i++)
		ntu_ratio[i] = 100;

	emstune_register_notifier(&ntu_emstune_notifier);
}

/******************************************************************************
 *                             Multi load tracking                            *
 ******************************************************************************/
static struct mlt __percpu *pcpu_mlt;

static inline void mlt_move_next_period(struct mlt *mlt)
{
	mlt->cur_period = (mlt->cur_period + 1) % MLT_PERIOD_COUNT;
}

static void mlt_update_full_hist(struct track_data *td, int cur_period)
{
	int i;

	for (i = 0; i < td->state_count; i++) {
		if (i == td->state)
			td->periods[i][cur_period] = SCHED_CAPACITY_SCALE;
		else
			td->periods[i][cur_period] = 0;
	}
}

static void mlt_update_hist(struct track_data *td, int cur_period)
{
	int i;

	for (i = 0; i < td->state_count; i++)
		td->periods[i][cur_period] = td->recent[i];
}

static void mlt_update_recent(struct track_data *td, u64 contrib)
{
	int state = td->state;

	if (state < 0)
		return;

	td->recent_sum[state] += contrib;
	td->recent[state] = div64_u64(
		td->recent_sum[state] << SCHED_CAPACITY_SHIFT, MLT_PERIOD_SIZE);
}

static void mlt_clear_and_update_recent(struct track_data *td, u64 contrib)
{
	memset(td->recent_sum, 0, sizeof(u64) * td->state_count);
	memset(td->recent, 0, sizeof(int) * td->state_count);

	mlt_update_recent(td, contrib);
}

void mlt_update(int cpu)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);
	u64 now = sched_clock();
	u64 contrib, remain;
	int period_count;

	/*
	 * If now is in recent period, contributer is from last_updated to now.
	 * Otherwise, it is from last_updated to period_end
	 * and remaining active time will be reflected in the next step.
	 */
	contrib = min(now, mlt->period_start + MLT_PERIOD_SIZE);
	contrib -= mlt->last_updated;

	/* 1. Apply time to recent period */
	mlt_update_recent(&mlt->art, contrib);
	mlt_update_recent(&mlt->cst, contrib);

	/*
	 * If now has passed recent period, calculate full periods and
	 * reflect them.
	 */
	period_count = div64_u64_rem(now - mlt->period_start, MLT_PERIOD_SIZE, &remain);
	if (period_count) {
		unsigned int count = period_count - 1;

		mlt_move_next_period(mlt);

		/* 2. Save recent period to period history */
		mlt_update_hist(&mlt->art, mlt->cur_period);
		mlt_update_hist(&mlt->cst, mlt->cur_period);

		/* 3. Update fully elapsed period */
		count = min_t(unsigned int, count, MLT_PERIOD_COUNT);
		while (count--) {
			mlt_move_next_period(mlt);

			mlt_update_full_hist(&mlt->art, mlt->cur_period);
			mlt_update_full_hist(&mlt->cst, mlt->cur_period);
		}

		/* 4. Apply remain time to recent period */
		mlt_clear_and_update_recent(&mlt->art, remain);
		mlt_clear_and_update_recent(&mlt->cst, remain);
	}

	trace_mlt_update(mlt);

	mlt->period_start += MLT_PERIOD_SIZE * period_count;
	mlt->last_updated = now;
}

void mlt_idle_enter(int cpu, int cstate)
{
	mlt_update(cpu);
	per_cpu_ptr(pcpu_mlt, cpu)->art.state = -1;
	per_cpu_ptr(pcpu_mlt, cpu)->cst.state = cstate;
}

void mlt_idle_exit(int cpu)
{
	mlt_update(cpu);
	per_cpu_ptr(pcpu_mlt, cpu)->art.state = 0;
	per_cpu_ptr(pcpu_mlt, cpu)->cst.state = -1;
}

void mlt_task_switch(int cpu, struct task_struct *next,
				int state)
{
	mlt_update(cpu);

	/*
	 * CPU In case of idle entry.
	 * Since idle entry handler is going to handle mlt, do nothig in here.
	 */
	if (state == IDLE_START)
		return;

	per_cpu_ptr(pcpu_mlt, cpu)->art.state = cpuctl_task_group_idx(next);
}

int mlt_cur_period(int cpu)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);

	return mlt->cur_period;
}

int mlt_prev_period(int period)
{
	period = period - 1;
	if (period < 0)
		return MLT_PERIOD_COUNT - 1;

	return period;
}

int mlt_period_with_delta(int idx, int delta)
{
	if (delta > 0)
		return (idx + delta) % MLT_PERIOD_COUNT;

	idx = idx + delta;
	if (idx < 0)
		return MLT_PERIOD_COUNT + idx;

	return idx;
}

int mlt_art_value(int cpu, int target_period)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);
	int i, value = 0;

	for (i = 0; i < mlt->art.state_count; i++)
		value += mlt->art.periods[i][target_period];

	return value;
}

int mlt_art_last_value(int cpu)
{
	return mlt_art_value(cpu, per_cpu_ptr(pcpu_mlt, cpu)->cur_period);
}

int mlt_art_cgroup_value(int cpu, int target_period, int cgroup)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);

	return mlt->art.periods[cgroup][target_period];
}

int mlt_cst_value(int cpu, int target_period, int cstate)
{
	struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);

	return mlt->cst.periods[cstate][target_period];
}

static int mlt_alloc_and_init(struct track_data *td, int state_count)
{
	int i;

	td->periods = kcalloc(state_count, sizeof(int *), GFP_KERNEL);
	if (!td->periods)
		return -ENOMEM;

	for (i = 0; i < state_count; i++) {
		td->periods[i] = kcalloc(MLT_PERIOD_COUNT,
					sizeof(int), GFP_KERNEL);
		if (!td->periods[i])
			goto fail_alloc;
	}

	td->recent_sum = kcalloc(state_count, sizeof(u64), GFP_KERNEL);
	if (!td->recent_sum)
		goto fail_alloc;

	td->recent = kcalloc(state_count, sizeof(int), GFP_KERNEL);
	if (!td->recent)
		goto fail_alloc;

	td->state = -1;
	td->state_count = state_count;

	return 0;

fail_alloc:
	for (i = 0; i < state_count; i++)
		kfree(td->periods[i]);
	kfree(td->periods);
	kfree(td->recent_sum);
	kfree(td->recent);

	return -ENOMEM;
}

int mlt_init(void)
{
	int cpu;
	u64 now;

	pcpu_mlt = alloc_percpu(struct mlt);
	if (!pcpu_mlt) {
		pr_err("failed to allocate mlt\n");
		return -ENOMEM;
	}

	now = sched_clock();
	for_each_possible_cpu(cpu) {
		struct mlt *mlt = per_cpu_ptr(pcpu_mlt, cpu);

		/* Set by default value */
		mlt->cpu = cpu;
		mlt->cur_period = 0;
		mlt->period_start = now;
		mlt->last_updated = now;

		mlt_alloc_and_init(&mlt->art, CGROUP_COUNT);
		mlt_alloc_and_init(&mlt->cst, CSTATE_MAX);
	}

	return 0;
}
