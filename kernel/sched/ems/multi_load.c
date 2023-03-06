/*
 * Multi-purpose Load tracker
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include "../sched.h"
#include "../sched-pelt.h"
#include "ems.h"

/******************************************************************************
 *                           MULTI LOAD for TASK                              *
 ******************************************************************************/
/*
 * ml_task_runnable - task runnable
 *
 * The time while the task is in the runqueue. This includes not only task
 * running time but also waiting time in the runqueue. The calculation
 * is the same as the task util.
 */
unsigned long ml_task_runnable(struct task_struct *p)
{
	return READ_ONCE(p->se.ml.runnable_avg);
}

extern long schedtune_margin(unsigned long capacity, unsigned long signal, long boost);

/*
 * ml_boosted_task_runnable - task runnable with schedtune boost
 *
 * Boosted task runnable utilization.
 */
unsigned long ml_boosted_task_runnable(struct task_struct *p)
{
	int boost = schedtune_task_boost(p);
	unsigned long runnable = ml_task_runnable(p);

	if (boost == 0)
		return runnable;

	return runnable + schedtune_margin(capacity_cpu(task_cpu(p), p->sse),
						runnable, boost);
}

/*
 * ml_task_util - task util
 *
 * Task utilization. The calculation is the same as the task util of cfs,
 * but applied capacity is different according to sse and uss of the task,
 * therefore, it sse task has different values from the task util of cfs.
 */
unsigned long ml_task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.ml.util_avg);
}

int ml_task_hungry(struct task_struct *p)
{
	if (!emstune_hungry)
		return 0;

	if (emstune_boosted())
		return 0;

	return READ_ONCE(p->se.ml.hungry);
}

/*
 * ml_task_util_est - task util with util-est
 *
 * Task utilization with util-est, The calculation is the same as
 * task_util_est of cfs.
 */
static unsigned long _ml_task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.ml.util_est);

	return max(ue.ewma, ue.enqueued);
}

unsigned long ml_task_util_est(struct task_struct *p)
{
	return emstune_util_est(p) ? max(ml_task_util(p), _ml_task_util_est(p))
					: ml_task_util(p);
}

/*
 * ml_boosted_task_util - task util with schedtune boost
 *
 * Boosted task utilization, it same as boosted_task_util of cfs.
 */
unsigned long ml_boosted_task_util(struct task_struct *p)
{
	int boost = schedtune_task_boost(p);
	unsigned long util = ml_task_util(p);

	if (boost == 0)
		return util;

	return util + schedtune_margin(capacity_cpu(task_cpu(p), p->sse),
						util, boost);
}

/******************************************************************************
 *                            MULTI LOAD for CPU                              *
 ******************************************************************************/
/*
 * __normalize_util - combine sse and uss utilization
 *
 * Combine sse and uss utilization and normalize to sse or uss according to "sse"
 * parameter.
 */
static inline unsigned long
__normalize_util(int cpu, unsigned int sse_util, unsigned int uss_util, int sse)
{
	if (sse)
		return sse_util + ((capacity_ratio(cpu, sse) * uss_util) >> SCHED_CAPACITY_SHIFT);
	else
		return uss_util + ((capacity_ratio(cpu, sse) * sse_util) >> SCHED_CAPACITY_SHIFT);
}

static unsigned long __ml_cpu_util_est(int cpu, int sse)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;

	return sse ? READ_ONCE(cfs_rq->ml.util_est_s.enqueued) :
		READ_ONCE(cfs_rq->ml.util_est.enqueued);
}

static unsigned long ml_cpu_util_est(int cpu, int sse)
{
	return __normalize_util(cpu, __ml_cpu_util_est(cpu, SSE),
				__ml_cpu_util_est(cpu, USS), sse);
}

/*
 * __ml_cpu_util - sse/uss utilization in cpu
 *
 * Cpu utilization. This function returns sse or uss utilization in
 * the cpu according to "sse" parameter.
 */
unsigned long __ml_cpu_util(int cpu, int sse)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;

	return sse ? READ_ONCE(cfs_rq->ml.util_avg_s) :
				READ_ONCE(cfs_rq->ml.util_avg);
}

/*
 * _ml_cpu_util_est - sse/uss utilization in cpu
 *
 * Cpu utilization. This function returns bigger value between util and util.est in
 * the cpu according to "sse" parameter.
 */
unsigned long _ml_cpu_util_est(int cpu, int sse)
{
	unsigned long util;

	util = __ml_cpu_util(cpu, sse);

	if (sched_feat(UTIL_EST))
		util = max_t(unsigned long, util, __ml_cpu_util_est(cpu, sse));

	return min_t(unsigned long, util, capacity_cpu_orig(cpu, sse));
}

/*
 * _ml_cpu_util - sse/uss combined cpu utilization
 *
 * Sse and uss combined cpu utilization. This function returns combined cpu
 * utilization normalized to sse or uss according to "sse" parameter.
 */
unsigned long _ml_cpu_util(int cpu, int sse)
{
	unsigned long util;

	util = __normalize_util(cpu, __ml_cpu_util(cpu, SSE),
				__ml_cpu_util(cpu, USS), sse);

	if (sched_feat(UTIL_EST))
		util = max_t(unsigned long, util, ml_cpu_util_est(cpu, sse));

	return util;
}

/*
 * ml_cpu_util - sse/uss combiend cpu utilization
 *
 * Sse and uss combined and uss normalized cpu utilization. Default policy
 * is to normalize to uss because cfs refers uss.
 */
unsigned long ml_cpu_util(int cpu)
{
	return _ml_cpu_util(cpu, USS);
}

/*
 * ml_cpu_util_ratio - cpu usuage ratio
 *
 * Cpu usage ratio of sse or uss. The ratio based on a maximum of 1024.
 * It is used to calculating energy.
 */
unsigned long ml_cpu_util_ratio(int cpu, int sse)
{
	return (__ml_cpu_util(cpu, sse) << SCHED_CAPACITY_SHIFT)
					/ capacity_cpu(cpu, sse);
}

#define UTIL_AVG_UNCHANGED 0x1

/*
 * ml_cpu_util_without - cpu utilization without waking task
 *
 * Cpu utilization with any contributions from the waking task p removed.
 */
unsigned long ml_cpu_util_without(int cpu, struct task_struct *p)
{
	unsigned long sse_util, uss_util;

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.ml.last_update_time))
		return ml_cpu_util(cpu);

	sse_util = __ml_cpu_util(cpu, SSE);
	uss_util = __ml_cpu_util(cpu, USS);

	if (p->sse)
		sse_util -= min_t(unsigned long, sse_util, ml_task_util(p));
	else
		uss_util -= min_t(unsigned long, uss_util, ml_task_util(p));

	if (sched_feat(UTIL_EST)) {
		unsigned int uss_util_est = __ml_cpu_util_est(cpu, USS);
		unsigned int sse_util_est = __ml_cpu_util_est(cpu, SSE);

		/*
		 * Despite the following checks we still have a small window
		 * for a possible race, when an execl's select_task_rq_fair()
		 * races with LB's detach_task():
		 *
		 *   detach_task()
		 *     p->on_rq = TASK_ON_RQ_MIGRATING;
		 *     ---------------------------------- A
		 *     deactivate_task()                   \
		 *       dequeue_task()                     + RaceTime
		 *         util_est_dequeue()              /
		 *     ---------------------------------- B
		 *
		 * The additional check on "current == p" it's required to
		 * properly fix the execl regression and it helps in further
		 * reducing the chances for the above race.
		 */
		if (emstune_util_est(p) &&
		    unlikely(task_on_rq_queued(p) || current == p)) {
			if (p->sse) {
				sse_util_est -= min_t(unsigned int, sse_util_est,
					   (_ml_task_util_est(p) | UTIL_AVG_UNCHANGED));
			} else {
				uss_util_est -= min_t(unsigned int, uss_util_est,
					   (_ml_task_util_est(p) | UTIL_AVG_UNCHANGED));
			}
		}

		uss_util = max_t(unsigned long, uss_util, uss_util_est);
		sse_util = max_t(unsigned long, sse_util, sse_util_est);
	}

	return __normalize_util(cpu, sse_util, uss_util, USS);
}

/*
 * ml_cpu_util_with - cpu utilization including waking task
 *
 * Cpu utilization with any contirubtions from *p
 */
unsigned long __ml_cpu_util_with(int cpu, struct task_struct *p, int sse)
{
	unsigned long util = __ml_cpu_util(cpu, sse);

	/*
	 * If 1) prev cpu of the waking task is different with cpu of which
	 * we want to calculate utilization and 2) the waking task is not newbie,
	 * consider the task's utilization for 'util' which we are calculating.
	 */
	if (cpu != task_cpu(p) && READ_ONCE(p->se.ml.last_update_time))
		if (p->sse == sse)
			util += ml_task_util(p);

	if (sched_feat(UTIL_EST)) {
		unsigned long util_est = __ml_cpu_util_est(cpu, sse);

		if (emstune_util_est(p) && p->sse == sse)
			util_est += (_ml_task_util_est(p) | UTIL_AVG_UNCHANGED);

		util = max_t(unsigned long, util, util_est);
	}

	return util;
}

unsigned long ml_cpu_util_with(int cpu, struct task_struct *p)
{
	return __normalize_util(cpu, __ml_cpu_util_with(cpu, p, SSE),
			__ml_cpu_util_with(cpu, p, USS), USS);
}

/*
 * ml_boosted_cpu_util - sse/uss combiend cpu utilization with boost
 *
 * Sse and uss combined and uss normalized cpu utilization with schedtune.boost.
 */
unsigned long ml_boosted_cpu_util(int cpu)
{
	int boost = schedtune_cpu_boost(cpu);
	unsigned long util = ml_cpu_util(cpu);

	if (boost == 0)
		return util;

	return util + schedtune_margin(capacity_cpu(cpu, USS), util, boost);
}

/******************************************************************************
 *                       initial utilization of task                          *
 ******************************************************************************/
void init_multi_load(struct sched_entity *se)
{
	struct multi_load *ml = &se->ml;

	memset(ml, 0, sizeof(*ml));
}

static u32 default_inherit_ratio = 25;

void post_init_entity_multi_load(struct sched_entity *se, u64 now)
{
	int sse = get_sse(se);
	struct cfs_rq *cfs_rq = se->cfs_rq;
	struct multi_load *ml = &se->ml;
	unsigned long cpu_scale = capacity_cpu(cpu_of(cfs_rq->rq), sse);
	long cap = (long)(cpu_scale - _ml_cpu_util(cpu_of(cfs_rq->rq), sse));
	u32 inherit_ratio;

	if (cap <= 0) {
		ml->util_avg = 0;
		return;
	}

	inherit_ratio = entity_is_task(se) ? emstune_init_util(task_of(se))
					   : default_inherit_ratio;

	ml->util_avg = cap * inherit_ratio / 100;

	trace_multi_load_new_task(ml);

	if (entity_is_task(se)) {
		struct task_struct *p = task_of(se);
		if (p->sched_class != &fair_sched_class)
			se->ml.last_update_time = now;
	}
}

/******************************************************************************
 *                           utilization tracking                             *
 ******************************************************************************/
static void update_next_balance(struct multi_load *ml)
{
	struct sched_entity *se = container_of(ml, struct sched_entity, ml);
	struct task_struct *p;

	if (!entity_is_task(se))
		return;

	p = task_of(se);

	/*
	 * To migrate heavy task to faster cpu by ontime migration, update
	 * the next_balance of this cpu because tick is most likely to occur
	 * first in this cpu.
	 */
	if (ml->runnable_avg >= get_upper_boundary(task_cpu(p), p))
		cpu_rq(smp_processor_id())->next_balance = jiffies;
}

static void test_task_hungry(struct multi_load *ml, unsigned long scale_cpu)
{
	/*
	 * task utilization is greater than 12.8% of cpu capacity and
	 * task runnable is greater than x1.5 task utilization,
	 * (== task wait time is greater than half of task utilization)
	 * then task is hungry.
	 */
	if ((ml->util_avg * 1024) > (scale_cpu * 128) &&
	     ml->runnable_avg > ml->util_avg + (ml->util_avg >> 1))
		ml->hungry = 1;
	else
		ml->hungry = 0;
}

static inline void util_change(struct multi_load *ml);

/*
 * Below 2 functions have same sequence to update load.
 *  - __update_task_util,
 *  - __update_cpu_util
 *
 * step 1: decay the load for the elapsed periods.
 * step 2: accumulate load if the condition is met.
 *	    - runnable : task weight is not 0
 *	    - util : task or cpu is running
 * step 3: update util avg with util sum
 */
static void
__update_task_util(struct multi_load *ml, u64 periods, u32 contrib,
		unsigned long scale_cpu, unsigned long load, int running, u64 divider)
{
	if (periods) {
		ml->util_sum = decay_load((u64)(ml->util_sum), periods);
		ml->runnable_sum = decay_load((u64)(ml->runnable_sum), periods);
	}

	if (running)
		ml->util_sum += contrib * scale_cpu;

	if (load)
		ml->runnable_sum += contrib * scale_cpu;

	if (!periods)
		return;

	ml->util_avg = ml->util_sum / divider;
	ml->runnable_avg = div_u64(ml->runnable_sum, divider);

	test_task_hungry(ml, scale_cpu);

	update_next_balance(ml);
	util_change(ml);

	trace_multi_load_task(ml);
}

static void
__update_cpu_util(struct multi_load *ml, u64 periods, u32 contrib,
		unsigned long scale_cpu, unsigned long load, int running,
		u64 divider, int sse)
{
	if (periods) {
		ml->util_sum_s = decay_load((u64)(ml->util_sum_s), periods);
		ml->util_sum = decay_load((u64)(ml->util_sum), periods);
		ml->runnable_sum_s = decay_load((u64)(ml->runnable_sum_s), periods);
		ml->runnable_sum = decay_load((u64)(ml->runnable_sum), periods);
	}

	if (running) {
		if (sse)
			ml->util_sum_s += contrib * scale_cpu;
		else
			ml->util_sum += contrib * scale_cpu;
	}

	if (load) {
		if (sse)
			ml->runnable_sum_s += contrib * scale_cpu;
		else
			ml->runnable_sum += contrib * scale_cpu;
	}

	if (!periods)
		return;

	ml->util_avg_s = ml->util_sum_s / divider;
	ml->util_avg = ml->util_sum / divider;
	ml->runnable_avg_s = div_u64(ml->runnable_sum_s, divider);
	ml->runnable_avg = div_u64(ml->runnable_sum, divider);

	trace_multi_load_cpu(ml);
}

static int
__update_multi_load(u64 delta, int cpu, struct cfs_rq *cfs_rq,
		struct multi_load *ml, unsigned long load, int running, int sse)
{
	unsigned long scale_freq, scale_cpu;
	u32 contrib = (u32)delta;
	u64 periods, divider;

	/* Obtain scale freq */
	scale_freq = arch_scale_freq_capacity(cpu);

	/* Obtain scale cpu */
	scale_cpu = capacity_cpu(cpu, sse);

	delta += ml->period_contrib;
	periods = delta / 1024; /* A period is 1024us (~1ms) */

	if (periods) {
		delta %= 1024;
		contrib = __accumulate_pelt_segments(periods,
			1024 - ml->period_contrib, delta);
	}

	ml->period_contrib = delta;
	contrib = (contrib * scale_freq) >> SCHED_CAPACITY_SHIFT;
	divider = LOAD_AVG_MAX - 1024 + ml->period_contrib;

	if (cfs_rq)
		__update_cpu_util(ml, periods, contrib, scale_cpu, load, running, divider, sse);
	else
		__update_task_util(ml, periods, contrib, scale_cpu, load, running, divider);

	return periods;
}

static void update_multi_load(u64 now, int cpu, struct cfs_rq *cfs_rq, struct sched_entity *se,
				struct sched_avg *sa, unsigned long load, int running)
{
	struct multi_load *ml;
	u64 delta;

	if (cfs_rq)
		ml = &cfs_rq->ml;
	else
		ml = &se->ml;

	delta = now - ml->last_update_time;
	if (delta < 0)
		return;

	delta >>= 10;
	if (!delta)
		return;

	ml->last_update_time += delta << 10;

	if (!load)
		running = 0;

	__update_multi_load(delta, cpu, cfs_rq, ml, load, running, get_sse(se));
}

static int __update_multi_load_blocked_se(u64 now, struct sched_entity *se)
{
	update_multi_load(now, se->cfs_rq->rq->cpu, NULL, se, &se->avg, 0, 0);

	return 0;
}

static int __update_multi_load_se(u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	update_multi_load(now, se->cfs_rq->rq->cpu, NULL, se, &se->avg,
			scale_load_down(se->load.weight) * se->on_rq, cfs_rq->curr == se);

	return 0;
}

static int __update_multi_load_cfs_rq(u64 now, struct cfs_rq *cfs_rq)
{
	update_multi_load(now, cfs_rq->rq->cpu, cfs_rq, cfs_rq->curr, &cfs_rq->avg,
			scale_load_down(cfs_rq->load.weight), cfs_rq->curr != NULL);

	return 0;
}

/*
 * Signed add and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define add_positive(_ptr, _val) do {                           \
	typeof(_ptr) ptr = (_ptr);                              \
	typeof(_val) val = (_val);                              \
	typeof(*ptr) res, var = READ_ONCE(*ptr);                \
								\
	res = var + val;                                        \
								\
	if (val < 0 && res > var)                               \
		res = 0;                                        \
								\
	WRITE_ONCE(*ptr, res);                                  \
} while (0)

/*
 * Unsigned subtract and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

/*
 * set_task_rq_multi_load() is called from
 *   set_task_rq_fair() in fair.c
 */
void set_task_rq_multi_load(struct sched_entity *se,
				struct cfs_rq *prev, struct cfs_rq *next)
{
	u64 p_last_update_time;
	u64 n_last_update_time;

	/*
	 * We are supposed to update the task to "current" time, then its up to
	 * date and ready to go to new CPU/cfs_rq. But we have difficulty in
	 * getting what current time is, so simply throw away the out-of-date
	 * time. This will result in the wakee task is less decayed, but giving
	 * the wakee more load sounds not bad.
	 */
	if (!(se->ml.last_update_time && prev))
		return;

	p_last_update_time = prev->ml.last_update_time;
	n_last_update_time = next->ml.last_update_time;

	__update_multi_load_blocked_se(p_last_update_time, se);
	se->ml.last_update_time = n_last_update_time;
}

/*
 * update_tg_cfs_multi_load() is called from
 *   update_tg_cfs_util() in fair.c
 */
void
update_tg_cfs_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se,
						struct cfs_rq *gcfs_rq)
{
	struct multi_load *ml = &se->ml;
	long delta, delta_s;

	delta_s = gcfs_rq->ml.util_avg_s - ml->util_avg_s;
	delta = gcfs_rq->ml.util_avg - ml->util_avg;

	if (delta_s) {
		/* Set new sched_entity's utilization */
		ml->util_avg_s = gcfs_rq->ml.util_avg_s;
		ml->util_sum_s = ml->util_avg_s * LOAD_AVG_MAX;

		/* Update parent cfs_rq utilization */
		add_positive(&cfs_rq->ml.util_avg_s, delta_s);
		cfs_rq->ml.util_sum_s = cfs_rq->ml.util_avg_s * LOAD_AVG_MAX;
	}

	if (delta) {
		/* Set new sched_entity's utilization */
		ml->util_avg = gcfs_rq->ml.util_avg;
		ml->util_sum = ml->util_avg * LOAD_AVG_MAX;

		/* Update parent cfs_rq utilization */
		add_positive(&cfs_rq->ml.util_avg, delta);
		cfs_rq->ml.util_sum = cfs_rq->ml.util_avg * LOAD_AVG_MAX;
	}

	trace_multi_load_task(ml);
	trace_multi_load_cpu(&cfs_rq->ml);
}

/*
 * update_cfs_rq_multi_load() is called from
 *   update_cfs_rq_load_avg() in fair.c
 */
int update_cfs_rq_multi_load(u64 now, struct cfs_rq *cfs_rq)
{
	unsigned long removed_util_s = 0, removed_util = 0;
	struct multi_load *ml = &cfs_rq->ml;
	int decayed = 0;

	if (cfs_rq->ml_removed.nr) {
		unsigned long r;
		u32 divider = LOAD_AVG_MAX - 1024 + ml->period_contrib;

		raw_spin_lock(&cfs_rq->ml_removed.lock);
		swap(cfs_rq->ml_removed.util_avg_s, removed_util_s);
		swap(cfs_rq->ml_removed.util_avg, removed_util);
		cfs_rq->ml_removed.nr = 0;
		raw_spin_unlock(&cfs_rq->ml_removed.lock);

		r = removed_util_s;
		sub_positive(&ml->util_avg_s, r);
		sub_positive(&ml->util_sum_s, r * divider);

		r = removed_util;
		sub_positive(&ml->util_avg, r);
		sub_positive(&ml->util_sum, r * divider);

		cfs_rq->propagate = 1;

		decayed = 1;
	}

	decayed |= __update_multi_load_cfs_rq(now, cfs_rq);

	return decayed;
}

/*
 * attach_entity_multi_load() is called from
 *   attach_entity_load_avg() in fair.c
 */
void attach_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	u32 divider = LOAD_AVG_MAX - 1024 + cfs_rq->ml.period_contrib;

	se->ml.last_update_time = cfs_rq->ml.last_update_time;
	se->ml.period_contrib = cfs_rq->ml.period_contrib;
	se->ml.util_sum = se->ml.util_avg * divider;

	if (get_sse(se)) {
		cfs_rq->ml.util_avg_s += se->ml.util_avg;
		cfs_rq->ml.util_sum_s += se->ml.util_sum;
	} else {
		cfs_rq->ml.util_avg += se->ml.util_avg;
		cfs_rq->ml.util_sum += se->ml.util_sum;
	}

	trace_multi_load_cpu(&cfs_rq->ml);
}

/*
 * detach_entity_multi_load() is called from
 *   detach_entity_load_avg() in fair.c
 */
void detach_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (get_sse(se)) {
		sub_positive(&cfs_rq->ml.util_avg_s, se->ml.util_avg);
		sub_positive(&cfs_rq->ml.util_sum_s, se->ml.util_sum);
	} else {
		sub_positive(&cfs_rq->ml.util_avg, se->ml.util_avg);
		sub_positive(&cfs_rq->ml.util_sum, se->ml.util_sum);
	}

	trace_multi_load_cpu(&cfs_rq->ml);
}

/*
 * update_multi_load_se() is called from
 *   update_load_avg() in fair.c
 */
int update_multi_load_se(u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	__update_multi_load_se(now, cfs_rq, se);

	return 0;
}

/*
 * sync_entity_multi_load() is called from
 *   sync_entity_load_avg() in fair.c
 */
void sync_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	__update_multi_load_blocked_se(cfs_rq->ml.last_update_time, se);
}

/*
 * remove_entity_multi_load() is called from
 *   remove_entity_load_avg() in fair.c
 */
void remove_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&cfs_rq->ml_removed.lock, flags);
	++cfs_rq->ml_removed.nr;
	if (get_sse(se))
		cfs_rq->ml_removed.util_avg_s += se->ml.util_avg;
	else
		cfs_rq->ml_removed.util_avg += se->ml.util_avg;
	raw_spin_unlock_irqrestore(&cfs_rq->ml_removed.lock, flags);
}

/*
 * init_cfs_rq_multi_load() is called from
 *   init_cfs_rq() in fair.c
 */
void init_cfs_rq_multi_load(struct cfs_rq *cfs_rq)
{
	raw_spin_lock_init(&cfs_rq->ml_removed.lock);
}

/*
 * migrate_entity_multi_load() is called from
 *   migrate_task_rq_fair() in fair.c
 *   task_move_group_fair() in fair.c
 */
void migrate_entity_multi_load(struct sched_entity *se)
{
	/* Tell new CPU we are migrated */
	se->ml.last_update_time = 0;
}

/******************************************************************************
 *                           MULTI LOAD UTIL-EST                              *
 ******************************************************************************/
/*
 * When a task is dequeued, its estimated utilization should not be update if
 * its util_avg has not been updated at least once.
 * This flag is used to synchronize util_avg updates with util_est updates.
 * We map this information into the LSB bit of the utilization saved at
 * dequeue time (i.e. util_est.dequeued).
 */
static inline void util_change(struct multi_load *ml)
{
	unsigned int enqueued;

	if (!sched_feat(UTIL_EST))
		return;

	/* Avoid store if the flag has been already set */
	enqueued = ml->util_est.enqueued;
	if (!(enqueued & UTIL_AVG_UNCHANGED))
		return;

	/* Reset flag to report util_avg has been updated */
	enqueued &= ~UTIL_AVG_UNCHANGED;
	WRITE_ONCE(ml->util_est.enqueued, enqueued);
}

static inline struct util_est* cfs_rq_util_est(struct cfs_rq *cfs_rq, int sse)
{
	return sse ? &cfs_rq->ml.util_est_s : &cfs_rq->ml.util_est;
}

void util_est_enqueue_multi_load(struct cfs_rq *cfs_rq, struct task_struct *p)
{
	unsigned int enqueued;
	struct util_est *cfs_rq_ue;

	if (!sched_feat(UTIL_EST))
		return;

	if (emstune_util_est(p)) {
		/*
		 * Skip applying estimated utilization of task to cfs_rq if it
		 * is already applied.
		 */
		if (p->se.ml.util_est_applied)
			return;

		cfs_rq_ue = cfs_rq_util_est(cfs_rq, p->sse);

		/* Update root cfs_rq's estimated utilization */
		enqueued  = cfs_rq_ue->enqueued;
		enqueued += (_ml_task_util_est(p) | UTIL_AVG_UNCHANGED);
		WRITE_ONCE(cfs_rq_ue->enqueued, enqueued);
		p->se.ml.util_est_applied = 1;

		/* Update plots for Task and CPU estimated utilization */
		trace_multi_load_util_est_task(p, &p->se.ml);
		trace_multi_load_util_est_cpu(cpu_of(cfs_rq->rq), cfs_rq);
	}
}

/*
 * Check if a (signed) value is within a specified (unsigned) margin,
 * based on the observation that:
 *     abs(x) < y := (unsigned)(x + y - 1) < (2 * y - 1)
 *
 * NOTE: this only works when value + maring < INT_MAX.
 */
static inline bool within_margin(int value, int margin)
{
	return ((unsigned int)(value + margin - 1) < (2 * margin - 1));
}

void util_est_dequeue_multi_load(struct cfs_rq *cfs_rq,
				struct task_struct *p, bool task_sleep)
{
	long last_ewma_diff;
	struct util_est ue, *cfs_rq_ue;
	bool updated = false;

	if (!sched_feat(UTIL_EST))
		return;

	/*
	 * Only the task to which utilization estimation is applied to cfs_rq
	 * is updated
	 */
	if (p->se.ml.util_est_applied) {
		cfs_rq_ue = cfs_rq_util_est(cfs_rq, p->sse);

		/* Update root cfs_rq's estimated utilization */
		ue.enqueued  = cfs_rq_ue->enqueued;
		ue.enqueued -= min_t(unsigned int, ue.enqueued,
				(_ml_task_util_est(p) | UTIL_AVG_UNCHANGED));
		WRITE_ONCE(cfs_rq_ue->enqueued, ue.enqueued);
		p->se.ml.util_est_applied = 0;

		updated = true;
	}

	/*
	 * There are some cases that dequeued tasks's util is not subtracted
	 * from cpu util_est enqueued; it causes cpu util_est to be misleading.
	 *
	 * Clear cpu util_est enqueued when cfs_rq->nr_running is 0. It helps
	 * cpu util_est to be in the right state.
	 */
	if (cfs_rq->nr_running == 0) {
		cfs_rq_ue = cfs_rq_util_est(cfs_rq, SSE);
		WRITE_ONCE(cfs_rq_ue->enqueued, 0);

		cfs_rq_ue = cfs_rq_util_est(cfs_rq, USS);
		WRITE_ONCE(cfs_rq_ue->enqueued, 0);

		updated = true;
	}

	if (updated) {
		/* Update plots for CPU's estimated utilization */
		trace_multi_load_util_est_cpu(cpu_of(cfs_rq->rq), cfs_rq);
	}

	/*
	 * Skip update of task's estimated utilization when the task has not
	 * yet completed an activation, e.g. being migrated.
	 */
	if (!task_sleep)
		return;

	/*
	 * If the PELT values haven't changed since enqueue time,
	 * skip the util_est update.
	 */
	ue = p->se.ml.util_est;
	if (ue.enqueued & UTIL_AVG_UNCHANGED)
		return;

	/*
	 * Skip update of task's estimated utilization when its EWMA is
	 * already ~1% close to its last activation value.
	 */
	ue.enqueued = (ml_task_util(p) | UTIL_AVG_UNCHANGED);
	last_ewma_diff = ue.enqueued - ue.ewma;
	if (within_margin(last_ewma_diff, capacity_cpu(task_cpu(p), USS) / 100))
		return;

	/*
	 * Update Task's estimated utilization
	 *
	 * When *p completes an activation we can consolidate another sample
	 * of the task size. This is done by storing the current PELT value
	 * as ue.enqueued and by using this value to update the Exponential
	 * Weighted Moving Average (EWMA):
	 *
	 *  ewma(t) = w *  task_util(p) + (1-w) * ewma(t-1)
	 *          = w *  task_util(p) +         ewma(t-1)  - w * ewma(t-1)
	 *          = w * (task_util(p) -         ewma(t-1)) +     ewma(t-1)
	 *          = w * (      last_ewma_diff            ) +     ewma(t-1)
	 *          = w * (last_ewma_diff  +  ewma(t-1) / w)
	 *
	 * Where 'w' is the weight of new samples, which is configured to be
	 * 0.25, thus making w=1/4 ( >>= UTIL_EST_WEIGHT_SHIFT)
	 */
	ue.ewma <<= UTIL_EST_WEIGHT_SHIFT;
	ue.ewma  += last_ewma_diff;
	ue.ewma >>= UTIL_EST_WEIGHT_SHIFT;
	WRITE_ONCE(p->se.ml.util_est, ue);

	/* Update plots for Task's estimated utilization */
	trace_multi_load_util_est_task(p, &p->se.ml);
}

void util_est_update(struct task_struct *p,
			int prev_util_est, int next_util_est)
{
	struct rq_flags rq_flags;
	struct rq *rq;
	struct util_est ue, *cfs_rq_ue;
	unsigned int enqueued = 0;

	/*
	 * If both prev and next util-est are off or on, enqueued of cfs_rq
	 * does not change.
	 */
	if (prev_util_est == next_util_est)
		return;

	rq = task_rq_lock(p, &rq_flags);
	if (!p->on_rq) {
		task_rq_unlock(rq, p, &rq_flags);
		return;
	}

	ue = p->se.ml.util_est;
	cfs_rq_ue = cfs_rq_util_est(&rq->cfs, p->sse);

	enqueued = cfs_rq_ue->enqueued;
	if (prev_util_est && p->se.ml.util_est_applied) {
		enqueued -= min_t(unsigned int, enqueued,
				(_ml_task_util_est(p) | UTIL_AVG_UNCHANGED));
		p->se.ml.util_est_applied = 0;
	}

	if (next_util_est && likely(!p->se.ml.util_est_applied)) {
		enqueued += (_ml_task_util_est(p) | UTIL_AVG_UNCHANGED);
		p->se.ml.util_est_applied = 1;
	}

	WRITE_ONCE(cfs_rq_ue->enqueued, enqueued);

	/* Update plots for CPU's estimated utilization */
	trace_multi_load_util_est_cpu(cpu_of(rq), &rq->cfs);

	task_rq_unlock(rq, p, &rq_flags);
}

/****************************************************************/
/*		Periodic Active Ratio Tracking			*/
/****************************************************************/
enum {
	PART_POLICY_RECENT = 0,
	PART_POLICY_MAX,
	PART_POLICY_MAX_RECENT_MAX,
	PART_POLICY_LAST,
	PART_POLICY_MAX_RECENT_LAST,
	PART_POLICY_MAX_RECENT_AVG,
	PART_POLICY_INVALID,
};

char *part_policy_name[] = {
	"RECENT",
	"MAX",
	"MAX_RECENT_MAX",
	"LAST",
	"MAX_RECENT_LAST",
	"MAX_RECENT_AVG",
	"INVALID"
};

static __read_mostly unsigned int part_policy_idx = PART_POLICY_MAX_RECENT_LAST;
__read_mostly u64 period_size = 4 * NSEC_PER_MSEC;
__read_mostly u64 period_hist_size = 10;
static __read_mostly int high_patten_thres = 700;
static __read_mostly int high_patten_stdev = 200;
static __read_mostly int low_patten_count = 3;
static __read_mostly int low_patten_thres = 1024;
static __read_mostly int low_patten_stdev = 200;

static __read_mostly u64 boost_interval = 16 * NSEC_PER_MSEC;

/********************************************************/
/*		  Helper funcition			*/
/********************************************************/

static inline int inc_hist_idx(int idx)
{
	return (idx + 1) % period_hist_size;
}

static inline void calc_active_ratio_hist(struct part *pa)
{
	int idx;
	int sum = 0, max = 0;
	int p_avg = 0, p_stdev = 0, p_count = 0;
	int patten, diff;

	/* Calculate basic statistics of P.A.R.T */
	for (idx = 0; idx < period_hist_size; idx++) {
		sum += pa->hist[idx];
		max = max(max, pa->hist[idx]);
	}

	pa->active_ratio_avg = sum / period_hist_size;
	pa->active_ratio_max = max;
	pa->active_ratio_est = 0;
	pa->active_ratio_stdev = 0;

	/* Calculate stdev for patten recognition */
	for (idx = 0; idx < period_hist_size; idx += 2) {
		patten = pa->hist[idx] + pa->hist[idx + 1];
		if (patten == 0)
			continue;

		p_avg += patten;
		p_count++;
	}

	if (p_count <= 1) {
		p_avg = 0;
		p_stdev = 0;
		goto out;
	}

	p_avg /= p_count;

	for (idx = 0; idx < period_hist_size; idx += 2) {
		patten = pa->hist[idx] + pa->hist[idx + 1];
		if (patten == 0)
			continue;

		diff = patten - p_avg;
		p_stdev += diff * diff;
	}

	p_stdev /= p_count - 1;
	p_stdev = int_sqrt(p_stdev);

out:
	pa->active_ratio_stdev = p_stdev;
	if (p_count >= low_patten_count &&
			p_avg <= low_patten_thres &&
			p_stdev <= low_patten_stdev)
		pa->active_ratio_est = p_avg / 2;

	trace_ems_cpu_active_ratio_patten(cpu_of(container_of(pa, struct rq, pa)),
			p_count, p_avg, p_stdev);
}

static void update_cpu_active_ratio_hist(struct part *pa, bool full, unsigned int count)
{
	/*
	 * Reflect recent active ratio in the history.
	 */
	pa->hist_idx = inc_hist_idx(pa->hist_idx);
	pa->hist[pa->hist_idx] = pa->active_ratio_recent;

	/*
	 * If count is positive, there are empty/full periods.
	 * These will be reflected in the history.
	 */
	while (count--) {
		pa->hist_idx = inc_hist_idx(pa->hist_idx);
		pa->hist[pa->hist_idx] = full ? SCHED_CAPACITY_SCALE : 0;
	}

	/*
	 * Calculate avg/max active ratio through entire history.
	 */
	calc_active_ratio_hist(pa);
}

static void
__update_cpu_active_ratio(int cpu, struct part *pa, u64 now, int boost)
{
	u64 elapsed = now - pa->period_start;
	unsigned int period_count = 0;

	if (boost) {
		pa->last_boost_time = now;
		return;
	}

	if (pa->last_boost_time &&
	    now > pa->last_boost_time + boost_interval)
		pa->last_boost_time = 0;

	if (pa->running) {
		/*
		 * If 'pa->running' is true, it means that the rq is active
		 * from last_update until now.
		 */
		u64 contributer, remainder;

		/*
		 * If now is in recent period, contributer is from last_updated to now.
		 * Otherwise, it is from last_updated to period_end
		 * and remaining active time will be reflected in the next step.
		 */
		contributer = min(now, pa->period_start + period_size);
		pa->active_sum += contributer - pa->last_updated;
		pa->active_ratio_recent =
			div64_u64(pa->active_sum << SCHED_CAPACITY_SHIFT, period_size);

		/*
		 * If now has passed recent period, calculate full periods and reflect they.
		 */
		period_count = div64_u64_rem(elapsed, period_size, &remainder);
		if (period_count) {
			update_cpu_active_ratio_hist(pa, true, period_count - 1);
			pa->active_sum = remainder;
			pa->active_ratio_recent =
				div64_u64(pa->active_sum << SCHED_CAPACITY_SHIFT, period_size);
		}
	} else {
		/*
		 * If 'pa->running' is false, it means that the rq is idle
		 * from last_update until now.
		 */

		/*
		 * If now has passed recent period, calculate empty periods and reflect they.
		 */
		period_count = div64_u64(elapsed, period_size);
		if (period_count) {
			update_cpu_active_ratio_hist(pa, false, period_count - 1);
			pa->active_ratio_recent = 0;
			pa->active_sum = 0;
		}
	}

	pa->period_start += period_size * period_count;
	pa->last_updated = now;
}

/********************************************************/
/*			External APIs			*/
/********************************************************/
void update_cpu_active_ratio(struct rq *rq, struct task_struct *p, int type)
{
	struct part *pa = &rq->pa;
	int cpu = cpu_of(rq);
	u64 now = sched_clock_cpu(0);

	if (unlikely(pa->period_start == 0))
		return;

	switch (type) {
	/*
	 * 1) Enqueue
	 * This type is called when the rq is switched from idle to running.
	 * In this time, Update the active ratio for the idle interval
	 * and change the state to running.
	 */
	case EMS_PART_ENQUEUE:
		__update_cpu_active_ratio(cpu, pa, now, 0);

		if (rq->nr_running == 0) {
			pa->running = true;
			trace_multi_load_update_cpu_active_ratio(cpu, pa, "enqueue");
		}
		break;
	/*
	 * 2) Dequeue
	 * This type is called when the rq is switched from running to idle.
	 * In this time, Update the active ratio for the running interval
	 * and change the state to not-running.
	 */
	case EMS_PART_DEQUEUE:
		__update_cpu_active_ratio(cpu, pa, now, 0);

		if (rq->nr_running == 1) {
			pa->running = false;
			trace_multi_load_update_cpu_active_ratio(cpu, pa, "dequeue");
		}
		break;
	/*
	 * 3) Update
	 * This type is called to update the active ratio during rq is running.
	 */
	case EMS_PART_UPDATE:
		__update_cpu_active_ratio(cpu, pa, now, 0);
		trace_multi_load_update_cpu_active_ratio(cpu, pa, "update");
		break;

	case EMS_PART_WAKEUP_NEW:
		__update_cpu_active_ratio(cpu, pa, now, 1);
		trace_multi_load_update_cpu_active_ratio(cpu, pa, "new task");
		break;
	}
}

void part_cpu_active_ratio(unsigned long *util, unsigned long *max, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct part *pa = &rq->pa;
	unsigned long pelt_max = *max;
	unsigned long pelt_util = *util;
	int util_ratio = *util * SCHED_CAPACITY_SCALE / *max;
	int demand = 0;

	if (unlikely(pa->period_start == 0))
		return;

	if (pa->last_boost_time && util_ratio < pa->active_ratio_boost) {
		*max = SCHED_CAPACITY_SCALE;
		*util = pa->active_ratio_boost;
		return;
	}

	if (util_ratio > pa->active_ratio_limit)
		return;

	if (!pa->running &&
			(pa->active_ratio_avg < high_patten_thres ||
			 pa->active_ratio_stdev > high_patten_stdev)) {
		*util = 0;
		*max = SCHED_CAPACITY_SCALE;
		return;
	}

	switch (part_policy_idx) {
	case PART_POLICY_RECENT:
		demand = pa->active_ratio_recent;
		break;
	case PART_POLICY_MAX:
		demand = pa->active_ratio_max;
		break;
	case PART_POLICY_MAX_RECENT_MAX:
		demand = max(pa->active_ratio_recent,
				pa->active_ratio_max);
		break;
	case PART_POLICY_LAST:
		demand = pa->hist[pa->hist_idx];
		break;
	case PART_POLICY_MAX_RECENT_LAST:
		demand = max(pa->active_ratio_recent,
				pa->hist[pa->hist_idx]);
		break;
	case PART_POLICY_MAX_RECENT_AVG:
		demand = max(pa->active_ratio_recent,
				pa->active_ratio_avg);
		break;
	}

	*util = max(demand, pa->active_ratio_est);
	*util = min_t(unsigned long, *util, (unsigned long)pa->active_ratio_limit);
	*max = SCHED_CAPACITY_SCALE;

	if (util_ratio > *util) {
		*util = pelt_util;
		*max = pelt_max;
	}

	trace_multi_load_cpu_active_ratio(cpu, *util, (unsigned long)util_ratio);
}

void set_part_period_start(struct rq *rq)
{
	struct part *pa = &rq->pa;
	u64 now;

	if (likely(pa->period_start))
		return;

	now = sched_clock_cpu(0);
	pa->period_start = now;
	pa->last_updated = now;
}

/********************************************************/
/*			  SYSFS				*/
/********************************************************/
static ssize_t show_part_policy(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "%u. %s\n", part_policy_idx,
			part_policy_name[part_policy_idx]);
}

static ssize_t store_part_policy(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	long input;

	if (!sscanf(buf, "%ld", &input))
		return -EINVAL;

	if (input >= PART_POLICY_INVALID || input < 0)
		return -EINVAL;

	part_policy_idx = input;

	return count;
}

static ssize_t show_part_policy_list(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	for (i = 0; i < PART_POLICY_INVALID ; i++)
		len += sprintf(buf + len, "%u. %s\n", i, part_policy_name[i]);

	return len;
}

static ssize_t show_active_ratio_limit(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct part *pa;
	int cpu, len = 0;

	for_each_possible_cpu(cpu) {
		pa = &cpu_rq(cpu)->pa;
		len += sprintf(buf + len, "cpu%d ratio:%3d\n",
				cpu, pa->active_ratio_limit);
	}

	return len;
}

static ssize_t store_active_ratio_limit(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	struct part *pa;
	int cpu, ratio, i;

	if (sscanf(buf, "%d %d", &cpu, &ratio) != 2)
		return -EINVAL;

	/* Check cpu is possible */
	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -EINVAL;

	/* Check ratio isn't outrage */
	if (ratio < 0 || ratio > SCHED_CAPACITY_SCALE)
		return -EINVAL;

	for_each_cpu(i, cpu_coregroup_mask(cpu)) {
		pa = &cpu_rq(i)->pa;
		pa->active_ratio_limit = ratio;
	}

	return count;
}

static ssize_t show_active_ratio_boost(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct part *pa;
	int cpu, len = 0;

	for_each_possible_cpu(cpu) {
		pa = &cpu_rq(cpu)->pa;
		len += sprintf(buf + len, "cpu%d ratio:%3d\n",
				cpu, pa->active_ratio_boost);
	}

	return len;
}

static ssize_t store_active_ratio_boost(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	struct part *pa;
	int cpu, ratio, i;

	if (sscanf(buf, "%d %d", &cpu, &ratio) != 2)
		return -EINVAL;

	/* Check cpu is possible */
	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -EINVAL;

	/* Check ratio isn't outrage */
	if (ratio < 0 || ratio > SCHED_CAPACITY_SCALE)
		return -EINVAL;

	for_each_cpu(i, cpu_coregroup_mask(cpu)) {
		pa = &cpu_rq(i)->pa;
		pa->active_ratio_boost = ratio;
	}

	return count;
}

#define show_node_function(_name)					\
static ssize_t show_##_name(struct kobject *kobj,			\
		struct kobj_attribute *attr, char *buf)			\
{									\
	return sprintf(buf, "%d\n", _name);				\
}

#define store_node_function(_name, _max)				\
static ssize_t store_##_name(struct kobject *kobj,			\
		struct kobj_attribute *attr, const char *buf,		\
		size_t count)						\
{									\
	unsigned int input;						\
									\
	if (!sscanf(buf, "%u", &input))					\
		return -EINVAL;						\
									\
	if (input > _max)						\
		return -EINVAL;						\
									\
	_name = input;							\
									\
	return count;							\
}

show_node_function(high_patten_thres);
store_node_function(high_patten_thres, SCHED_CAPACITY_SCALE);
show_node_function(high_patten_stdev);
store_node_function(high_patten_stdev, SCHED_CAPACITY_SCALE);
show_node_function(low_patten_count);
store_node_function(low_patten_count, (period_size / 2));
show_node_function(low_patten_thres);
store_node_function(low_patten_thres, (SCHED_CAPACITY_SCALE * 2));
show_node_function(low_patten_stdev);
store_node_function(low_patten_stdev, SCHED_CAPACITY_SCALE);

static struct kobj_attribute _policy =
__ATTR(policy, 0644, show_part_policy, store_part_policy);
static struct kobj_attribute _policy_list =
__ATTR(policy_list, 0444, show_part_policy_list, NULL);
static struct kobj_attribute _high_patten_thres =
__ATTR(high_patten_thres, 0644, show_high_patten_thres, store_high_patten_thres);
static struct kobj_attribute _high_patten_stdev =
__ATTR(high_patten_stdev, 0644, show_high_patten_stdev, store_high_patten_stdev);
static struct kobj_attribute _low_patten_count =
__ATTR(low_patten_count, 0644, show_low_patten_count, store_low_patten_count);
static struct kobj_attribute _low_patten_thres =
__ATTR(low_patten_thres, 0644, show_low_patten_thres, store_low_patten_thres);
static struct kobj_attribute _low_patten_stdev =
__ATTR(low_patten_stdev, 0644, show_low_patten_stdev, store_low_patten_stdev);
static struct kobj_attribute _active_ratio_limit =
__ATTR(active_ratio_limit, 0644, show_active_ratio_limit, store_active_ratio_limit);
static struct kobj_attribute _active_ratio_boost =
__ATTR(active_ratio_boost, 0644, show_active_ratio_boost, store_active_ratio_boost);

static struct attribute *attrs[] = {
	&_policy.attr,
	&_policy_list.attr,
	&_high_patten_thres.attr,
	&_high_patten_stdev.attr,
	&_low_patten_count.attr,
	&_low_patten_thres.attr,
	&_low_patten_stdev.attr,
	&_active_ratio_limit.attr,
	&_active_ratio_boost.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init init_part_sysfs(void)
{
	struct kobject *kobj;

	kobj = kobject_create_and_add("part", ems_kobj);
	if (!kobj)
		return -EINVAL;

	if (sysfs_create_group(kobj, &attr_group))
		return -EINVAL;

	return 0;
}
late_initcall(init_part_sysfs);

static int __init parse_part(void)
{
	struct device_node *dn, *coregroup;
	char name[15];
	int cpu, cnt = 0, limit = -1, boost = -1;

	dn = of_find_node_by_path("/ems/part");
	if (!dn)
		return 0;

	for_each_possible_cpu(cpu) {
		struct part *pa = &cpu_rq(cpu)->pa;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			goto skip_parse;

		limit = -1;
		boost = -1;

		snprintf(name, sizeof(name), "coregroup%d", cnt++);
		coregroup = of_get_child_by_name(dn, name);
		if (!coregroup)
			continue;

		of_property_read_s32(coregroup, "active-ratio-limit", &limit);
		of_property_read_s32(coregroup, "active-ratio-boost", &boost);

skip_parse:
		if (limit >= 0)
			pa->active_ratio_limit = SCHED_CAPACITY_SCALE * limit / 100;

		if (boost >= 0)
			pa->active_ratio_boost = SCHED_CAPACITY_SCALE * boost / 100;
	}

	return 0;
}
core_initcall(parse_part);

void __init init_part(void)
{
	int cpu, idx;

	for_each_possible_cpu(cpu) {
		struct part *pa = &cpu_rq(cpu)->pa;

		/* Set by default value */
		pa->running = false;
		pa->active_sum = 0;
		pa->active_ratio_recent = 0;
		pa->hist_idx = 0;
		for (idx = 0; idx < PART_HIST_SIZE_MAX; idx++)
			pa->hist[idx] = 0;

		pa->period_start = 0;
		pa->last_updated = 0;
	}
}
