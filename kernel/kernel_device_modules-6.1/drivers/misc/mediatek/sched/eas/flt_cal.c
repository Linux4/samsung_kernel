// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#define NR_WINDOWS_PER_SEC (NSEC_PER_SEC / DEFAULT_SCHED_RAVG_WINDOW)

#define SCHED_FREQ_ACCOUNT_WAIT_TIME 0
#define SCHED_ACCOUNT_WAIT_TIME 1
#define INIT_TASK_LOAD_PCT 15

/* Default cpu setting */
#define CPU_DEFAULT_WIN_SIZE DEFAULT_SCHED_RAVG_WINDOW
#define CPU_DEFAULT_WIN_CNT 5
#define CPU_DEFAULT_WEIGHT_POLICY WP_MODE_4
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
unsigned int (*grp_cal_rat)(unsigned long long x, unsigned long long y);
EXPORT_SYMBOL(grp_cal_rat);
#endif
const char *task_event_names[] = {
	"PUT_PREV_TASK",
	"PICK_NEXT_TASK",
	"TASK_WAKE",
	"TASK_MIGRATE",
	"TASK_UPDATE",
	"IRQ_UPDATE"
};

const char *add_task_demand_names[] = {
	"WITHIN_WIN",
	"SPAN_WIN_PREV",
	"SPAN_WIN_CURR",
};

const char *history_event_names[] = {
	"NEW_WIN_UPDATE",
	"SPAN_WIN_ONE",
	"SPAN_WIN_MULTI",
};

const char *cpu_event_names[] = {
	"CPU_WITHIN_WIN",
	"NOT_CURRENT_TASK",
	"CURRENT_TASK",
	"CPU_IRQ_UPDATE",
};

unsigned int __read_mostly sysctl_sched_init_task_load_pct = INIT_TASK_LOAD_PCT;
EXPORT_SYMBOL(sysctl_sched_init_task_load_pct);
unsigned int __read_mostly sysctl_task_sched_window_stats_policy = WP_MODE_4;
EXPORT_SYMBOL(sysctl_task_sched_window_stats_policy);
unsigned int __read_mostly sysctl_sched_ravg_window_nr_ticks = (HZ / NR_WINDOWS_PER_SEC);
EXPORT_SYMBOL(sysctl_sched_ravg_window_nr_ticks);

/* Window size (in ns) */
static __read_mostly unsigned int new_sched_ravg_window = DEFAULT_SCHED_RAVG_WINDOW;
__read_mostly unsigned int sched_ravg_hist_size = RAVG_HIST_SIZE_MAX;
static __read_mostly unsigned int flt_scale_demand_divisor;
unsigned int __read_mostly sched_ravg_window = DEFAULT_SCHED_RAVG_WINDOW;
static unsigned int __read_mostly sched_init_task_load_windows_scaled;
static unsigned int __read_mostly sched_init_task_load_windows;
static __read_mostly unsigned int sched_io_is_busy = 1;
static u32 empty_windows[FLT_NR_CPUS];
static atomic64_t flt_irq_work_lastq_ws;
static struct irq_work flt_irq_work;
static int flt_sync_all_cpu(void);
static DEFINE_SPINLOCK(sched_ravg_window_lock);
DEFINE_PER_CPU(struct flt_rq, flt_rq);
#define NEW_TASK_ACTIVE_TIME ((u64)sched_ravg_window * sched_ravg_hist_size)
#define scale_demand(d) ((d) / flt_scale_demand_divisor)

/* Debug */
//#define FLT_DEBUG 1

#ifdef FLT_DEBUG
#define FLT_LOGI(...)	pr_info("FLT:" __VA_ARGS__)
#else
#define FLT_LOGI(...)
#endif

static u64 get_current_time(void)
{
	struct flt_pm fltpm;

	flt_get_pm_status(&fltpm);
	if (unlikely(fltpm.ktime_suspended))
		return ktime_to_ns(fltpm.ktime_last);
	return ktime_get_ns();
}

static u64
update_window_start(struct rq *rq, u64 curr_time, int event)
{
	s64 delta;
	int nr_windows;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	u64 old_window_start = fsrq->window_start;

	delta = curr_time - fsrq->window_start;
	if (delta < 0) {
		FLT_LOGI("[FLT]CPU%d; curr_time=%llu is lesser than window_start=%llu\n",
			rq->cpu, curr_time, fsrq->window_start);
		return old_window_start;
	}
	if (delta < sched_ravg_window)
		return old_window_start;

	nr_windows = div64_u64(delta, sched_ravg_window);
	fsrq->window_start += (u64)nr_windows * (u64)sched_ravg_window;

	fsrq->prev_window_size = sched_ravg_window;

	return old_window_start;
}

static void flt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 curr_time, u64 irqtime);

static void update_task_group_history(struct rq *rq, u32 samples, u32 *group_update_sum)
{
	int ridx, widx;
	u32 j = 0;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	u32 *group_sum_hist_widx, *group_sum_hist_ridx;
	u32 *group_util_hist_widx, *group_util_hist_ridx;
	u32 group_util_update[GROUP_ID_RECORD_MAX] = {0};

	/* Push new 'runtime' value onto stack */
	widx = sched_ravg_hist_size - 1;
	ridx = widx - samples;
	for (; ridx >= 0; --widx, --ridx) {
		group_sum_hist_widx = &fsrq->group_sum_history[widx][0];
		group_sum_hist_ridx = &fsrq->group_sum_history[ridx][0];
		memcpy(group_sum_hist_widx, group_sum_hist_ridx,
			sizeof(u32) * GROUP_ID_RECORD_MAX);

		group_util_hist_widx = &fsrq->group_util_history[widx][0];
		group_util_hist_ridx = &fsrq->group_util_history[ridx][0];
		memcpy(group_util_hist_widx, group_util_hist_ridx,
			sizeof(u32) * GROUP_ID_RECORD_MAX);
	}

	for (j = 0; j < GROUP_ID_RECORD_MAX; ++j) {
		group_util_update[j] = div64_u64((u64)((u64)group_update_sum[j]
					<< SCHED_CAPACITY_SHIFT), sched_ravg_window);
	}

	for (widx = 0; widx < samples && widx < sched_ravg_hist_size; widx++) {
		group_sum_hist_widx = &fsrq->group_sum_history[widx][0];
		memcpy(group_sum_hist_widx, group_update_sum,
			sizeof(u32) * GROUP_ID_RECORD_MAX);

		group_util_hist_widx = &fsrq->group_util_history[widx][0];
		memcpy(group_util_hist_widx, group_util_update,
			sizeof(u32) * GROUP_ID_RECORD_MAX);
	}
}

static void rollover_task_group_window(struct rq *rq, bool full_window)
{
	u32 i = 0, curr_sum = 0;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);

	for (i = 0; i < GROUP_ID_RECORD_MAX; ++i) {
		curr_sum = fsrq->group_curr_sum[i];
		if (unlikely(full_window))
			curr_sum = 0;
		fsrq->group_prev_sum[i] = curr_sum;
		fsrq->group_curr_sum[i] = 0;
	}
}

static void fixup_busy_time(struct task_struct *p, int new_cpu)
{
	struct rq *src_rq = task_rq(p);
	struct rq *dest_rq = cpu_rq(new_cpu);
	u64 wallclock;
	unsigned int state;

	state = READ_ONCE(p->__state);
	if (!p->on_rq && state != TASK_WAKING)
		return;

	if (state == TASK_WAKING)
		double_rq_lock(src_rq, dest_rq);

	wallclock = get_current_time();
	if (state == TASK_WAKING) {
		lockdep_assert_rq_held(src_rq);
		lockdep_assert_rq_held(dest_rq);
	}
	if (task_rq(p) != src_rq) {
		FLT_LOGI("on CPU %d task %s(%d) not on src_rq %d",
			raw_smp_processor_id(), p->comm, p->pid, src_rq->cpu);
	}

	flt_update_task_ravg(task_rq(p)->curr, task_rq(p),
			      TASK_UPDATE, wallclock, 0);
	flt_update_task_ravg(p, task_rq(p), TASK_MIGRATE,
			 wallclock, 0);

	if (state == TASK_WAKING)
		double_rq_unlock(src_rq, dest_rq);
}

static void flt_android_rvh_set_task_cpu(void *unused, struct task_struct *p, unsigned int new_cpu)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;
	fixup_busy_time(p, (int) new_cpu);
}

static void set_window_start(struct rq *rq)
{
	static int sync_cpu_available;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	struct flt_rq *sync_fsrq;
	struct flt_task_struct *fts = &((struct mtk_task *)current->android_vendor_data1)->flt_task;

	if (likely(fsrq->window_start))
		return;

	if (!sync_cpu_available) {
		fsrq->window_start = 1;
		sync_cpu_available = 1;
		atomic64_set(&flt_irq_work_lastq_ws, fsrq->window_start);
	} else {
		struct rq *sync_rq = cpu_rq(cpumask_any(cpu_online_mask));

		sync_fsrq = &per_cpu(flt_rq, sync_rq->cpu);
		raw_spin_rq_unlock(rq);
		double_rq_lock(rq, sync_rq);
		fsrq->window_start = sync_fsrq->window_start;
		fsrq->curr_runnable_sum = 0;
		fsrq->prev_runnable_sum = 0;
		fsrq->nt_curr_runnable_sum = 0;
		fsrq->nt_prev_runnable_sum = 0;
		raw_spin_rq_unlock(sync_rq);
	}

	fts->mark_start = fsrq->window_start;
}

static inline u64 scale_exec_time(u64 delta, struct rq *rq)
{
	unsigned long cie = arch_scale_cpu_capacity(cpu_of(rq));
	unsigned long fie = arch_scale_freq_capacity(cpu_of(rq));

	delta = cap_scale(delta, cie);
	delta = cap_scale(delta, fie);
	return delta;
}

inline bool flt_is_new_task(struct task_struct *p)
{
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;

	return fts->active_time < NEW_TASK_ACTIVE_TIME;
}

static void rollover_task_window(struct task_struct *p, bool full_window)
{
	u32 *curr_cpu_windows = empty_windows;
	u32 curr_window;
	int i;
	struct flt_rq *fsrq = &per_cpu(flt_rq, task_rq(p)->cpu);
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;

	/* Rollover the sum */
	curr_window = 0;
	if (trace_sched_rollover_task_window_enabled())
		trace_sched_rollover_task_window(p, fts, full_window);

	if (!full_window) {
		curr_window = fts->curr_window;
		curr_cpu_windows = fts->curr_window_cpu;
	}

	fts->prev_window = curr_window;
	fts->curr_window = 0;

	/* Roll over individual CPU contributions */
	for (i = 0; i < FLT_NR_CPUS; i++) {
		fts->prev_window_cpu[i] = curr_cpu_windows[i];
		fts->curr_window_cpu[i] = 0;
	}

	if (flt_is_new_task(p))
		fts->active_time += fsrq->prev_window_size;
}

static inline int cpu_is_waiting_on_io(struct rq *rq)
{
	if (!sched_io_is_busy)
		return 0;

	return atomic_read(&rq->nr_iowait);
}

static int account_busy_for_cpu_time(struct rq *rq, struct task_struct *p,
				     u64 irqtime, int event)
{
	if (is_idle_task(p)) {
		if (event == PICK_NEXT_TASK)
			return 0;
		return irqtime || cpu_is_waiting_on_io(rq);
	}

	if (event == TASK_WAKE)
		return 0;

	if (event == PUT_PREV_TASK || event == IRQ_UPDATE)
		return 1;

	if (event == TASK_UPDATE) {
		if (rq->curr == p)
			return 1;

		return p->on_rq ? SCHED_FREQ_ACCOUNT_WAIT_TIME : 0;
	}

	return SCHED_FREQ_ACCOUNT_WAIT_TIME;
}

static void update_cpu_history(struct rq *rq, u32 samples, u64 runtime)
{
	int ridx, widx;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	u32 *rq_sum_hist = &fsrq->sum_history[0];
	u32 *rq_util_hist = &fsrq->util_history[0];

	widx = sched_ravg_hist_size - 1;
	ridx = widx - samples;
	for (; ridx >= 0; --widx, --ridx) {
		rq_sum_hist[widx] = rq_sum_hist[ridx];
		rq_util_hist[widx] = rq_util_hist[ridx];
	}

	for (widx = 0; widx < samples && widx < sched_ravg_hist_size; widx++) {
		rq_sum_hist[widx] = runtime;
		rq_util_hist[widx] = div64_u64((u64)(runtime << SCHED_CAPACITY_SHIFT),
								sched_ravg_window);
	}
	if (trace_sched_update_cpu_history_enabled())
		trace_sched_update_cpu_history(rq, fsrq, samples, runtime);
}

static void rollover_cpu_window(struct rq *rq, bool full_window)
{
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	u64 curr_sum = fsrq->curr_runnable_sum;
	u64 nt_curr_sum = fsrq->nt_curr_runnable_sum;

	if (unlikely(full_window)) {
		curr_sum = 0;
		nt_curr_sum = 0;
	}

	fsrq->prev_runnable_sum = curr_sum;
	fsrq->nt_prev_runnable_sum = nt_curr_sum;
	fsrq->curr_runnable_sum = 0;
	fsrq->nt_curr_runnable_sum = 0;
}

static void update_cpu_busy_time(struct task_struct *p, struct rq *rq,
				 int event, u64 wallclock, u64 irqtime)
{
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	u64 mark_start = fts->mark_start;
	int new_window, full_window = 0, nr_full_windows = 0;
	int p_is_curr_task = (p == rq->curr);
	u64 window_start = fsrq->window_start, span_win = 0, span_sum = 0;
	u64 rq_update_sum = 0;
	u32 group_update_sum[GROUP_ID_RECORD_MAX] = {0};
	int flt_groupid = -1;
	bool is_flt_group_id = false;
	u32 j = 0;
	u64 delta = 0, prev_delta = 0, curr_delta = 0;
	u32 window_size = fsrq->prev_window_size;
	u64 *curr_runnable_sum = &fsrq->curr_runnable_sum;
	u64 *prev_runnable_sum = &fsrq->prev_runnable_sum;
	u64 *nt_curr_runnable_sum = &fsrq->nt_curr_runnable_sum;
	u64 *nt_prev_runnable_sum = &fsrq->nt_prev_runnable_sum;
	bool new_task;
	enum cpu_update_event cpu_event = 0;
	int cpu = rq->cpu;

	new_window = mark_start < window_start;
	if (new_window)
		full_window = (window_start - mark_start) >= window_size;
	if (full_window) {
		delta = window_start - mark_start;
		nr_full_windows = div64_u64(delta, window_size);
		span_win = window_start - ((u64)nr_full_windows * (u64)window_size);
		span_sum = span_win - mark_start;
	}

	if (!is_idle_task(p)) {
		if (new_window)
			rollover_task_window(p, full_window);
	}

	new_task = flt_is_new_task(p);
	is_flt_group_id = check_and_get_grp_id(p, &flt_groupid);

	if (p_is_curr_task && new_window) {
		if (full_window) {
			for (j = 0; j < GROUP_ID_RECORD_MAX; ++j)
				group_update_sum[j] = fsrq->group_curr_sum[j];
			rq_update_sum = fsrq->curr_runnable_sum;
		}
		rollover_task_group_window(rq, full_window);
		rollover_cpu_window(rq, full_window);
	}

	if (!account_busy_for_cpu_time(rq, p, irqtime, event))
		goto done;

	/* take care of flt group */
	if (is_flt_group_id) {
		curr_runnable_sum = &fsrq->group_curr_sum[flt_groupid];
		prev_runnable_sum = &fsrq->group_prev_sum[flt_groupid];
	}
	if (!new_window) {
		if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq))
			delta = wallclock - mark_start;
		else
			delta = irqtime;
		delta = scale_exec_time(delta, rq);
		*curr_runnable_sum += delta;

		cpu_event = CPU_WITHIN_WIN;
		curr_delta = delta;

		if (new_task)
			*nt_curr_runnable_sum += delta;

		if (!is_idle_task(p)) {
			fts->curr_window += delta;
			fts->curr_window_cpu[cpu] += delta;
		}

		goto done;
	}

	if (!p_is_curr_task) {
		if (!full_window) {
			delta = scale_exec_time(window_start - mark_start, rq);
			fts->prev_window += delta;
			fts->prev_window_cpu[cpu] += delta;
		} else {
			if (is_flt_group_id)
				group_update_sum[flt_groupid] += span_sum;
			else
				rq_update_sum += span_sum;
			delta = scale_exec_time(window_size, rq);
			fts->prev_window = delta;
			fts->prev_window_cpu[cpu] = delta;
		}

		cpu_event = NOT_CURRENT_TASK;
		prev_delta = delta;
		*prev_runnable_sum += delta;
		if (new_task)
			*nt_prev_runnable_sum += delta;

		delta = scale_exec_time(wallclock - window_start, rq);
		curr_delta = delta;
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		fts->curr_window = delta;
		fts->curr_window_cpu[cpu] = delta;
		goto done;
	}

	if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq)) {
		if (!full_window) {
			delta = scale_exec_time(window_start - mark_start, rq);
			if (!is_idle_task(p)) {
				fts->prev_window += delta;
				fts->prev_window_cpu[cpu] += delta;
			}
		} else {
			delta = scale_exec_time(window_size, rq);
			if (!is_idle_task(p)) {
				fts->prev_window = delta;
				fts->prev_window_cpu[cpu] = delta;
				if (is_flt_group_id)
					group_update_sum[flt_groupid] += span_sum;
				else
					rq_update_sum += span_sum;
			}
		}

		cpu_event = CURRENT_TASK;
		prev_delta = delta;
		*prev_runnable_sum += delta;
		if (new_task)
			*nt_prev_runnable_sum += delta;

		delta = scale_exec_time(wallclock - window_start, rq);
		curr_delta = delta;
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		if (!is_idle_task(p)) {
			fts->curr_window = delta;
			fts->curr_window_cpu[cpu] = delta;
		}

		goto done;
	}

	if (irqtime) {
		mark_start = wallclock - irqtime;

		if (mark_start > window_start) {
			*curr_runnable_sum = scale_exec_time(irqtime, rq);
			return;
		}

		delta = window_start - mark_start;
		if (delta > window_size)
			delta = window_size;
		delta = scale_exec_time(delta, rq);
		*prev_runnable_sum += delta;
		cpu_event = CPU_IRQ_UPDATE;
		prev_delta = delta;

		delta = wallclock - window_start;
		*curr_runnable_sum = scale_exec_time(delta, rq);
		curr_delta = delta;
		goto done;
	}

done:
	if (trace_sched_update_cpu_busy_time_enabled())
		trace_sched_update_cpu_busy_time(rq, p, fts, cpu_event, prev_delta, curr_delta);
	if (p_is_curr_task && new_window) {
		if (!full_window) {
			for (j = 0; j < GROUP_ID_RECORD_MAX; ++j)
				group_update_sum[j] = fsrq->group_prev_sum[j];
			rq_update_sum = fsrq->prev_runnable_sum;
		}
		update_task_group_history(rq, 1, group_update_sum);
		update_cpu_history(rq, 1, rq_update_sum);

		if (nr_full_windows) {
			if (!is_idle_task(p))
				delta = scale_exec_time(window_size, rq);
			else
				delta = 0;
			for (j = 0; j < GROUP_ID_RECORD_MAX; ++j)
				group_update_sum[j] = 0;

			if (is_flt_group_id) {
				group_update_sum[flt_groupid] = delta;
				rq_update_sum = 0;
			} else {
				rq_update_sum = delta;
			}
			update_task_group_history(rq, nr_full_windows, group_update_sum);
			update_cpu_history(rq, nr_full_windows, rq_update_sum);
		}
	}
}

void flt_rvh_enqueue_task(void *data, struct rq *rq,
				struct task_struct *p, int flags)
{
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;
	bool double_enqueue = false;
	int flt_groupid = -1;
	bool is_flt_group_id = false;

	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;

	lockdep_assert_rq_held(rq);

	if (task_cpu(p) != cpu_of(rq))
		FLT_LOGI("enqueuing on rq %d when task->cpu is %d\n",
				cpu_of(rq), task_cpu(p));

	if (fts->prev_on_rq == 1)
		double_enqueue = true;

	fts->prev_on_rq = 1;
	fts->prev_on_rq_cpu = cpu_of(rq);

	if (!double_enqueue) {

		/* take care of flt group */
		is_flt_group_id = check_and_get_grp_id(p, &flt_groupid);
		if (is_flt_group_id)
			fsrq->group_nr_running[flt_groupid]++;
	}
	if (trace_sched_enq_deq_task_enabled())
		trace_sched_enq_deq_task(p, 1, cpumask_bits(&p->cpus_mask)[0], fsrq);
}

void flt_rvh_dequeue_task(void *data, struct rq *rq,
				struct task_struct *p, int flags)
{
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;
	bool double_dequeue = false;

	int flt_groupid = -1;
	bool is_flt_group_id = false;

	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;

	lockdep_assert_rq_held(rq);

	if (fts->prev_on_rq_cpu >= 0 && fts->prev_on_rq_cpu != cpu_of(rq))
		FLT_LOGI("dequeue cpu %d not same as enqueue %d\n",
			 cpu_of(rq), fts->prev_on_rq_cpu);

	fts->prev_on_rq_cpu = -1;

	if (fts->prev_on_rq == 2)
		double_dequeue = true;

	fts->prev_on_rq = 2;

	if (!double_dequeue) {

		/* take care of flt group */
		is_flt_group_id = check_and_get_grp_id(p, &flt_groupid);
		if (is_flt_group_id) {
			if (fsrq->group_nr_running[flt_groupid] > 0)
				fsrq->group_nr_running[flt_groupid]--;
		}
	}
	if (trace_sched_enq_deq_task_enabled())
		trace_sched_enq_deq_task(p, 0, cpumask_bits(&p->cpus_mask)[0], fsrq);
}

static int account_busy_for_task_demand(struct rq *rq, struct task_struct *p, int event)
{
	if (is_idle_task(p))
		return 0;
	if (event == TASK_WAKE)
		return 0;

	if (event == PICK_NEXT_TASK && rq->curr == rq->idle)
		return 0;

	if (event == TASK_UPDATE) {
		if (rq->curr == p)
			return 1;

		return p->on_rq ? SCHED_ACCOUNT_WAIT_TIME : 0;
	}

	return 1;
}

static void update_history(struct rq *rq, struct task_struct *p,
			u32 runtime, int samples, int event,
			enum history_event update_history_event)
{
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;
	u32 *hist = &fts->sum_history[0];
	u32 *util_sum_hist = &fts->util_sum_history[0];
	u32 *util_avg_hist = &fts->util_avg_history[0];
	int ridx, widx;
	u32 max = 0, util_sum_max = 0, avg, util_avg_max = 0;
	u64 sum = 0, util_avg_sum = 0;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);

	if (!runtime || is_idle_task(p) || !samples)
		goto done;

	widx = sched_ravg_hist_size - 1;
	ridx = widx - samples;
	for (; ridx >= 0; --widx, --ridx) {
		hist[widx] = hist[ridx];
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	for (widx = 0; widx < samples && widx < sched_ravg_hist_size; widx++) {
		hist[widx] = runtime;
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	fts->sum = 0;

	sum = 0;
	widx = sched_ravg_hist_size - 1;
	ridx = widx - samples;
	for (; ridx >= 0; --widx, --ridx) {
		util_sum_hist[widx] = util_sum_hist[ridx];
		if (util_sum_hist[widx] > util_sum_max)
			util_sum_max = util_sum_hist[widx];

		util_avg_hist[widx] = util_avg_hist[ridx];
		if (util_avg_hist[widx] > util_avg_max)
			util_avg_max = util_avg_hist[widx];
		util_avg_sum += util_avg_hist[widx];
	}

	for (widx = 0; widx < samples && widx < sched_ravg_hist_size; widx++) {
		util_sum_hist[widx] = fts->util_sum;
		if (util_sum_hist[widx] > util_sum_max)
			util_sum_max = util_sum_hist[widx];

		/* update util avg */
		util_avg_hist[widx] = div64_u64((u64)fts->util_sum << SCHED_CAPACITY_SHIFT,
								sched_ravg_window);
		if (util_avg_hist[widx] > util_avg_max)
			util_avg_max = util_avg_hist[widx];
		util_avg_sum += util_avg_hist[widx];
	}
	avg = div64_u64(util_avg_sum, sched_ravg_hist_size);
	fts->util_sum = 0;
done:
	trace_sched_update_history(rq, p, runtime, samples,
				event, fsrq, fts, update_history_event);
}

static u64 add_to_task_demand(struct rq *rq, struct task_struct *p, u64 delta, enum win_event event)
{
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;
	unsigned long cie = arch_scale_cpu_capacity(cpu_of(rq));
	unsigned long fie = arch_scale_freq_capacity(cpu_of(rq));
	u64 scaled_time = 0;

	fts->sum += delta;
	scaled_time = scale_exec_time(delta, rq);

	fts->util_sum += scaled_time;
	if (trace_sched_add_to_task_demand_enabled())
		trace_sched_add_to_task_demand(rq, p, fts, delta, cie, fie, scaled_time, event);

	if (unlikely(fts->sum > sched_ravg_window))
		fts->sum = sched_ravg_window;
	if (unlikely(fts->util_sum > sched_ravg_window))
		fts->util_sum = sched_ravg_window;

	return delta;
}

static u64 update_task_demand(struct task_struct *p, struct rq *rq,
			       int event, u64 wallclock)
{
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;
	u64 mark_start = fts->mark_start;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	u64 delta, window_start = fsrq->window_start;
	int new_window, nr_full_windows;
	u32 window_size = sched_ravg_window;
	u64 runtime;
	enum win_event demand_event = 0;
	enum history_event update_history_event = 0;

	new_window = mark_start < window_start;
	if (!account_busy_for_task_demand(rq, p, event)) {
		if (new_window) {
			update_history_event = NEW_WIN_UPDATE;
			update_history(rq, p, fts->sum, 1, event, update_history_event);
		}
		return 0;
	}

	if (!new_window) {
		demand_event = WITHIN_WIN;
		return add_to_task_demand(rq, p, wallclock - mark_start, demand_event);
	}

	delta = window_start - mark_start;
	nr_full_windows = div64_u64(delta, window_size);
	window_start -= (u64)nr_full_windows * (u64)window_size;

	demand_event = SPAN_WIN_PREV;
	runtime = add_to_task_demand(rq, p, window_start - mark_start, demand_event);

	update_history_event = SPAN_WIN_ONE;
	update_history(rq, p, fts->sum, 1, event, update_history_event);
	if (nr_full_windows) {
		u64 scaled_window = scale_exec_time(window_size, rq);

		fts->sum = window_size;
		fts->util_sum = scaled_window;
		update_history_event = SPAN_WIN_MULTI;
		update_history(rq, p, fts->sum, nr_full_windows, event, update_history_event);
		runtime += nr_full_windows * scaled_window;
	}

	window_start += (u64)nr_full_windows * (u64)window_size;

	mark_start = window_start;
	demand_event = SPAN_WIN_CURR;
	runtime += add_to_task_demand(rq, p, wallclock - mark_start, demand_event);

	return runtime;
}

static inline void flt_irq_work_queue(struct irq_work *work)
{
	if (likely(cpu_online(raw_smp_processor_id())))
		irq_work_queue(work);
	else
		irq_work_queue_on(work, cpumask_any(cpu_online_mask));
}

static inline void run_flt_irq_work(u64 old_window_start, struct rq *rq)
{
	u64 result;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);

	if (old_window_start == fsrq->window_start)
		return;

	result = atomic64_cmpxchg(&flt_irq_work_lastq_ws, old_window_start,
				   fsrq->window_start);
	if (result == old_window_start)
		flt_irq_work_queue(&flt_irq_work);
}

static void flt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime)
{
	u64 old_window_start;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;
	s64 delta = 0;
	int group_id = -1;

	if (!fsrq->window_start || fts->mark_start >= wallclock)
		return;

	/* Sanity check */
	delta = wallclock - fsrq->window_start;
	if (delta < 0) {
		FLT_LOGI("FLT CPU%d; curr_time=%llu is lesser than window_start=%llu\n",
			rq->cpu, wallclock, fsrq->window_start);
		return;
	}

	delta = wallclock - fts->last_update_time;
	if (delta < 0) {
		FLT_LOGI("FLT CPU%d; curr_time=%llu is lesser than fts->last_update_time=%llu\n",
			rq->cpu, wallclock, fts->last_update_time);
		return;
	}

	/*
	 * Use 1024ns as the unit of measurement since it's a reasonable
	 * approximation of 1us and fast to compute.
	 */
	delta >>= 10;
	if (!delta && event == TASK_UPDATE)
		return;

	lockdep_assert_rq_held(rq);
	old_window_start = update_window_start(rq, wallclock, event);

	if (!fts->mark_start)
		goto done;

	group_id = get_grp_id(p);
	if (trace_sched_update_task_ravg_enabled())
		trace_sched_update_task_ravg(rq, p, wallclock, event, fsrq, fts, group_id, irqtime);

	update_task_demand(p, rq, event, wallclock);
	update_cpu_busy_time(p, rq, event, wallclock, irqtime);
done:
	fts->last_update_time += delta << 10;
	fts->mark_start = wallclock;
	run_flt_irq_work(old_window_start, rq);
}

static void flt_init_new_task_load(struct task_struct *p)
{
	int i;
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;
	u32 init_load_windows = sched_init_task_load_windows;
	u32 init_load_windows_scaled = sched_init_task_load_windows_scaled;
	u32 init_load_pct = sysctl_sched_init_task_load_pct;

	fts->init_load_pct = 0;

	fts->mark_start = 0;
	fts->last_update_time = 0;
	fts->sum = 0;
	fts->util_sum = 0;
	fts->util_demand = 0;
	fts->active_time = 0;

	fts->prev_on_rq = 0;
	fts->prev_on_rq_cpu = -1;
	fts->curr_window = 0;
	fts->prev_window = 0;
	memset(fts->curr_window_cpu, 0, sizeof(u32) * FLT_NR_CPUS);
	memset(fts->prev_window_cpu, 0, sizeof(u32) * FLT_NR_CPUS);

	if (init_load_pct) {
		init_load_windows = div64_u64((u64)init_load_pct *
			  (u64)sched_ravg_window, 100);
		init_load_windows_scaled = scale_demand(init_load_windows);
	}

	fts->demand = init_load_windows;
	for (i = 0; i < RAVG_HIST_SIZE_MAX; ++i)
		fts->sum_history[i] = init_load_windows;
	for (i = 0; i < RAVG_HIST_SIZE_MAX; ++i)
		fts->util_sum_history[i] = init_load_windows;
	for (i = 0; i < RAVG_HIST_SIZE_MAX; ++i)
		fts->util_avg_history[i] = 0;
}

static void flt_init_existing_task_load(struct task_struct *p)
{
	flt_init_new_task_load(p);
}

static void mark_task_starting(struct task_struct *p)
{
	u64 wallclock;
	struct flt_task_struct *fts = &((struct mtk_task *)p->android_vendor_data1)->flt_task;

	wallclock = get_current_time();
	fts->mark_start = wallclock;
}

static void flt_init_window_dep(void)
{
	flt_scale_demand_divisor = sched_ravg_window >> SCHED_CAPACITY_SHIFT;

	sched_init_task_load_windows =
		div64_u64((u64)sysctl_sched_init_task_load_pct *
			  (u64)sched_ravg_window, 100);
	sched_init_task_load_windows_scaled =
		scale_demand(sched_init_task_load_windows);
}

static int flt_sync_all_cpu(void)
{
	int cpu, grp_idx, ridx, widx;
	u64 wallclock;
	s64 delta;
	struct rq *rq;
	struct flt_rq *fsrq;
	u64 sum = 0, update = 0, total_sum[GROUP_ID_RECORD_MAX] = {0};
	u64 res = 0, rt_total[GROUP_ID_RECORD_MAX] = {0};
	u32 *gp_widx;

	wallclock = get_current_time();
	for_each_possible_cpu(cpu) {
		rq = cpu_rq(cpu);

		raw_spin_rq_lock(rq);
		fsrq = &per_cpu(flt_rq, cpu);
		delta = wallclock - fsrq->window_start;
		if (delta >= sched_ravg_window  && rq->curr)
			flt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
		if (rq->nr_running == 0)
			memset(fsrq->group_nr_running, 0, sizeof(fsrq->group_nr_running));
		raw_spin_rq_unlock(rq);
	}

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
		sum = 0;
		for_each_possible_cpu(cpu) {
			rq = cpu_rq(cpu);
			fsrq = &per_cpu(flt_rq, cpu);
			sum += READ_ONCE(fsrq->group_util_history[0][grp_idx]);
		}
		if (sum) {
			for_each_possible_cpu(cpu) {
				rq = cpu_rq(cpu);
				fsrq = &per_cpu(flt_rq, cpu);
				update = READ_ONCE(fsrq->group_util_history[0][grp_idx]);
				widx = RAVG_HIST_SIZE_MAX - 1;
				ridx = widx - 1;
				for (; ridx >= 0; --widx, --ridx) {
					gp_widx = &fsrq->group_util_active_history[grp_idx][0];
					WRITE_ONCE(gp_widx[widx], READ_ONCE(gp_widx[ridx]));
				}

				for (widx = 0; widx < 1 && widx < sched_ravg_hist_size; widx++) {
					gp_widx = &fsrq->group_util_active_history[grp_idx][0];
					WRITE_ONCE(gp_widx[widx], update);
				}
			}
		}
	}

	if (flt_getnid() == FLT_GP_NID) {
		for_each_possible_cpu(cpu) {
			rq = cpu_rq(cpu);
			fsrq = &per_cpu(flt_rq, cpu);
			for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
				sum = 0;
				for (ridx = 0; ridx < RAVG_HIST_SIZE_MAX; ++ridx)
					sum += READ_ONCE(
						fsrq->group_util_active_history[grp_idx][ridx]);
				total_sum[grp_idx] += sum;
				WRITE_ONCE(fsrq->group_util_ratio[grp_idx], sum);
			}
		}

		for_each_possible_cpu(cpu) {
			rq = cpu_rq(cpu);
			fsrq = &per_cpu(flt_rq, cpu);
			for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
				res = READ_ONCE(fsrq->group_util_ratio[grp_idx])
					<< SCHED_CAPACITY_SHIFT;
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
				if (grp_cal_rat)
					res = grp_cal_rat(res, total_sum[grp_idx]);
				else
					res = 0;
#else
				res = 0;
#endif
				WRITE_ONCE(fsrq->group_util_ratio[grp_idx], res);
			}
		}
	} else {
		for_each_possible_cpu(cpu) {
			rq = cpu_rq(cpu);
			fsrq = &per_cpu(flt_rq, cpu);
			for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
				sum = 0;
				for (ridx = 0; ridx < RAVG_HIST_SIZE_MAX; ++ridx)
					sum += READ_ONCE(fsrq->group_util_history[ridx][grp_idx]);
				total_sum[grp_idx] += sum;
				WRITE_ONCE(fsrq->group_raw_util_ratio[grp_idx], sum);
			}
		}

		for_each_possible_cpu(cpu) {
			rq = cpu_rq(cpu);
			fsrq = &per_cpu(flt_rq, cpu);
			for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
				res = READ_ONCE(fsrq->group_raw_util_ratio[grp_idx])
					<< SCHED_CAPACITY_SHIFT;
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
				if (grp_cal_rat)
					res = grp_cal_rat(res, total_sum[grp_idx]);
				else
					res = 0;
#else
				res = 0;
#endif
				WRITE_ONCE(fsrq->group_raw_util_ratio[grp_idx], res);
			}
		}
	}
	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
		for_each_possible_cpu(cpu) {
			rq = cpu_rq(cpu);
			fsrq = &per_cpu(flt_rq, cpu);
			rt_total[grp_idx] += READ_ONCE(fsrq->group_util_history[0][grp_idx]);
		}
	}

	for_each_possible_cpu(cpu) {
		rq = cpu_rq(cpu);
		fsrq = &per_cpu(flt_rq, cpu);
		for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
			res = READ_ONCE(fsrq->group_util_history[0][grp_idx])
				<< SCHED_CAPACITY_SHIFT;
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
			if (grp_cal_rat)
				res = grp_cal_rat(res, rt_total[grp_idx]);
			else
				res = 0;
#else
			res = 0;
#endif
			WRITE_ONCE(fsrq->group_util_rtratio[grp_idx], res);
		}
	}
	return 0;
}
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
static void flt_set_preferred_gear(void)
{
	int grp_id = 0, util = 0, threshold = 0, wl = 0;
	struct grp *grp = NULL;

	/* gear hint works on wl_type !=4 */
	wl = get_curr_wl();

	if (unlikely(group_get_mode() == GP_MODE_0))
		return;

	if (wl == 4 && !get_grp_high_freq(0)) {
		for (grp_id = 0; grp_id < GROUP_ID_RECORD_MAX; grp_id++) {
			grp = lookup_grp(grp_id);
			if (!grp)
				return;
			grp->gear_hint = false;
			if (trace_sched_set_preferred_cluster_enabled())
				trace_sched_set_preferred_cluster(wl, grp_id,
					util, threshold, grp->gear_hint);
			}
		return;
	}

	group_update_threshold_util(wl);

	for (grp_id = 0; grp_id < GROUP_ID_RECORD_MAX; grp_id++) {
		grp = lookup_grp(grp_id);
		if (!grp)
			return;
		util = grp_awr_get_grp_tar_util(grp_id);
		threshold = group_get_threshold_util(grp_id);

		if (util > threshold)
			grp->gear_hint = true;
		else
			grp->gear_hint = false;
		if (trace_sched_set_preferred_cluster_enabled())
			trace_sched_set_preferred_cluster(wl, grp_id,
				util, threshold, grp->gear_hint);
	}
}
#endif

static void flt_irq_workfn(struct irq_work *irq_work)
{
	struct flt_rq *fsrq;
	u64 wc;
	unsigned long flags;

	flt_sync_all_cpu();

#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
	if (get_grp_dvfs_ctrl()) {
		update_active_ratio_all();
		grp_awr_update_grp_awr_util();
	}
	flt_set_preferred_gear();
#endif
	wc = get_current_time();

	spin_lock_irqsave(&sched_ravg_window_lock, flags);
	fsrq = &per_cpu(flt_rq, this_rq()->cpu);
	if (sched_ravg_window != new_sched_ravg_window &&
	    (wc < fsrq->window_start + new_sched_ravg_window)) {
		sched_ravg_window = new_sched_ravg_window;
		flt_init_window_dep();
	}
	spin_unlock_irqrestore(&sched_ravg_window_lock, flags);
}

static void flt_init_once(void)
{
	init_irq_work(&flt_irq_work, flt_irq_workfn);
	flt_init_window_dep();
}

static void flt_sched_init_rq(struct rq *rq)
{
	int j;
	struct flt_rq *fsrq = &per_cpu(flt_rq, rq->cpu);

	if (cpu_of(rq) == 0)
		flt_init_once();

	fsrq->prev_window_size = sched_ravg_window;
	fsrq->window_start = 0;

	fsrq->curr_runnable_sum = 0;
	fsrq->prev_runnable_sum = 0;
	fsrq->nt_curr_runnable_sum = 0;
	fsrq->nt_prev_runnable_sum = 0;

	for (j = 0; j < RAVG_HIST_SIZE_MAX; ++j)
		fsrq->sum_history[j] = 0;
	for (j = 0; j < GROUP_ID_RECORD_MAX; ++j) {
		fsrq->group_prev_sum[j] = 0;
		fsrq->group_curr_sum[j] = 0;
		fsrq->group_nr_running[j] = 0;
	}
	memset(fsrq->group_sum_history, 0,
		sizeof(u32) * RAVG_HIST_SIZE_MAX * GROUP_ID_RECORD_MAX);
	memset(fsrq->group_util_history, 0,
		sizeof(u32) * RAVG_HIST_SIZE_MAX * GROUP_ID_RECORD_MAX);
	memset(fsrq->group_util_active_history, 0,
		sizeof(u32) * RAVG_HIST_SIZE_MAX * GROUP_ID_RECORD_MAX);
}

void sched_window_nr_ticks_change(void)
{
	unsigned long flags;

	spin_lock_irqsave(&sched_ravg_window_lock, flags);
	new_sched_ravg_window = mult_frac(sysctl_sched_ravg_window_nr_ticks,
						NSEC_PER_SEC, HZ);
	spin_unlock_irqrestore(&sched_ravg_window_lock, flags);
}
EXPORT_SYMBOL(sched_window_nr_ticks_change);

static void flt_android_rvh_sched_cpu_starting(void *unused, int cpu)
{
	unsigned long flags;
	struct rq *rq = cpu_rq(cpu);

	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;
	raw_spin_rq_lock_irqsave(rq, flags);
	set_window_start(rq);
	raw_spin_rq_unlock_irqrestore(rq, flags);
}

static void flt_android_rvh_new_task_stats(void *unused, struct task_struct *p)
{
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;
	flt_init_new_task_load(p);
	mark_task_starting(p);
}

static void flt_android_rvh_try_to_wake_up(void *unused, struct task_struct *p)
{
	struct rq *rq = cpu_rq(task_cpu(p));
	struct rq_flags rf;
	u64 wallclock;

	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;
	rq_lock_irqsave(rq, &rf);
	wallclock = get_current_time();
	flt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
	flt_update_task_ravg(p, rq, TASK_WAKE, wallclock, 0);
	rq_unlock_irqrestore(rq, &rf);
}

static void flt_android_rvh_tick_entry(void *unused, struct rq *rq)
{
	u64 wallclock;

	lockdep_assert_rq_held(rq);
	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;

	set_window_start(rq);
	wallclock = get_current_time();

	flt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
}

static void flt_android_rvh_schedule(void *unused, unsigned int sched_mode,
		struct task_struct *prev, struct task_struct *next, struct rq *rq)
{
	u64 wallclock;

	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;

	wallclock = get_current_time();
	if (likely(prev != next)) {
		flt_update_task_ravg(prev, rq, PUT_PREV_TASK, wallclock, 0);
		flt_update_task_ravg(next, rq, PICK_NEXT_TASK, wallclock, 0);
	} else {
		flt_update_task_ravg(prev, rq, TASK_UPDATE, wallclock, 0);
	}
}

static void flt_register_kernel_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_sched_cpu_starting(
		flt_android_rvh_sched_cpu_starting, NULL);
	if (ret)
		pr_info("register sched_cpu_starting hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_try_to_wake_up(
		flt_android_rvh_try_to_wake_up, NULL);
	if (ret)
		pr_info("register try_to_wake_up hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_schedule(
		flt_android_rvh_schedule, NULL);
	if (ret)
		pr_info("register schedule hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_set_task_cpu(
		flt_android_rvh_set_task_cpu, NULL);
	if (ret)
		pr_info("register set_task_cpu hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_new_task_stats(
		flt_android_rvh_new_task_stats, NULL);
	if (ret)
		pr_info("register new_task_stats hooks failed, returned %d\n", ret);
	ret = register_trace_android_rvh_tick_entry(
		flt_android_rvh_tick_entry, NULL);
	if (ret)
		pr_info("register android_rvh_tick_entry failed\n");
}

void flt_cal_init(void)
{
	u64 window_start_ns, nr_windows;
	struct flt_rq *fsrq;
	struct task_struct *g, *p;
	int cpu;

	if (unlikely(flt_get_mode() == FLT_MODE_0))
		return;
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
	grp_awr_init();
#endif
	/* for existing thread */
	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		flt_init_existing_task_load(p);
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);

	window_start_ns = get_current_time();
	nr_windows = div64_u64(window_start_ns, sched_ravg_window);
	window_start_ns = (u64)nr_windows * (u64)sched_ravg_window;

	/* init for each cpu */
	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);

		/* Create task members for idle thread */
		flt_init_new_task_load(rq->idle);

		flt_sched_init_rq(rq);
		fsrq = &per_cpu(flt_rq, cpu);
		fsrq->window_start = window_start_ns;
		fsrq->window_size = CPU_DEFAULT_WIN_SIZE;
		fsrq->window_count = CPU_DEFAULT_WIN_CNT;
		fsrq->weight_policy = CPU_DEFAULT_WEIGHT_POLICY;
	}
	atomic64_set(&flt_irq_work_lastq_ws, window_start_ns);

	flt_register_kernel_hooks();
}

void flt_set_grp_ctrl(int set)
{
	struct grp *grp = NULL;
	int grp_id = 0;


	if (unlikely(group_get_mode() == GP_MODE_0))
		return;

	if (set) {
		/* force on grp dvfs*/
		flt_ctrl_force_set(1);
		set_grp_dvfs_ctrl(FLT_MODE2_EN);
	} else {
		/* reset gear hint for grp task */
		for (grp_id = 0; grp_id < GROUP_ID_RECORD_MAX; grp_id++) {
			grp = lookup_grp(grp_id);
			/* santiy check */
			if (!grp)
				return;
			grp->gear_hint = false;
		}
		/* reset to default setting*/
		flt_ctrl_force_set(0);
	}
}
