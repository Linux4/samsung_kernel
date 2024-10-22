/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _SCHED_COMMON_H
#define _SCHED_COMMON_H

#define MTK_VENDOR_DATA_SIZE_TEST(mstruct, kstruct)		\
	BUILD_BUG_ON(sizeof(mstruct) > (sizeof(u64) *		\
		ARRAY_SIZE(((kstruct *)0)->android_vendor_data1)))

#define GEAR_HINT_UNSET -1
#define MTK_TASK_GROUP_FLAG 1
#define MTK_TASK_FLAG 9
#define RAVG_HIST_SIZE_MAX (5)
#define FLT_NR_CPUS CONFIG_MAX_NR_CPUS

struct task_gear_hints {
	int gear_start;
	int num_gear;
	int reverse;
};

struct vip_task_struct {
	struct list_head		vip_list;
	u64				sum_exec_snapshot;
	u64				total_exec;
	int				vip_prio;
	bool			basic_vip;
	bool			vvip;
};

struct soft_affinity_task {
	bool latency_sensitive;
	struct cpumask soft_cpumask;
};

struct gp_task_struct {
	struct grp __rcu	*grp;
	bool customized;
};

struct sbb_task_struct {
	int set_task;
	int set_group;
};

struct curr_uclamp_hint {
	int hint;
};

struct rot_task_struct {
	u64 ktime_ns;
};

struct cc_task_struct {
	u64 over_type;
};

struct task_turbo_t {
	unsigned char turbo:1;
	unsigned char render:1;
	unsigned short inherit_cnt:14;
	short nice_backup;
	atomic_t inherit_types;
};

struct flt_task_struct {
	u64	last_update_time;
	u64	mark_start;
	u32	sum;
	u32	util_sum;
	u32	demand;
	u32	util_demand;
	u32	sum_history[RAVG_HIST_SIZE_MAX];
	u32	util_sum_history[RAVG_HIST_SIZE_MAX];
	u32	util_avg_history[RAVG_HIST_SIZE_MAX];
	u64	active_time;
	u32	init_load_pct;
	u32	curr_window_cpu[FLT_NR_CPUS];
	u32	prev_window_cpu[FLT_NR_CPUS];
	u32	curr_window;
	u32	prev_window;
	int	prev_on_rq;
	int	prev_on_rq_cpu;
};

struct cpuqos_task_struct {
	int pd;
	int rank;
};

struct mig_task_struct {
	unsigned long pending_rec;
};

struct mtk_task {
	struct vip_task_struct	vip_task;
	struct soft_affinity_task sa_task;
	struct gp_task_struct	gp_task;
	struct task_gear_hints  gear_hints;
	struct sbb_task_struct sbb_task;
	struct curr_uclamp_hint cu_hint;
	struct rot_task_struct rot_task;
	struct cc_task_struct cc_task;
	struct task_turbo_t turbo_data;
	struct flt_task_struct flt_task;
	struct cpuqos_task_struct cpuqos_task;
	struct mig_task_struct mig_task;
};

struct soft_affinity_tg {
	struct cpumask soft_cpumask;
};

struct cgrp_tg {
	bool colocate;
	int groupid;
};

struct vip_task_group {
	unsigned int threshold;
};

struct mtk_tg {
	struct soft_affinity_tg	sa_tg;
	struct cgrp_tg		cgrp_tg;
	struct vip_task_group vtg;
};

struct sugov_rq_data {
	short int uclamp[UCLAMP_CNT];
	bool enq_dvfs;
	bool enq_ing;
	bool enq_update_dsu_freq;
};

struct mtk_rq {
	struct sugov_rq_data sugov_data;
};

extern int num_sched_clusters;
extern cpumask_t __read_mostly ***cpu_array;
extern void init_cpu_array(void);
extern void build_cpu_array(void);
extern void free_cpu_array(void);
extern void mtk_get_gear_indicies(struct task_struct *p, int *order_index, int *end_index,
			int *reverse);
extern bool sched_gear_hints_enable_get(void);
extern void init_gear_hints(void);


extern bool sched_updown_migration_enable_get(void);
extern void init_updown_migration(void);

extern bool sched_post_init_util_enable_get(void);

extern int set_gear_indices(int pid, int gear_start, int num_gear, int reverse);
extern int unset_gear_indices(int pid);
extern int get_gear_indices(int pid, int *gear_start, int *num_gear, int *reverse);
extern int set_updown_migration_pct(int gear_idx, int dn_pct, int up_pct);
extern int unset_updown_migration_pct(int gear_idx);
extern int get_updown_migration_pct(int gear_idx, int *dn_pct, int *up_pct);

struct util_rq {
	unsigned long util_cfs;
	unsigned long dl_util;
	unsigned long irq_util;
	unsigned long rt_util;
	unsigned long bw_dl_util;
	bool base;
};

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
extern void mtk_map_util_freq(void *data, unsigned long util, unsigned long freq,
			struct cpumask *cpumask, unsigned long *next_freq, int wl_type);
#else
#define mtk_map_util_freq(data, util, freq, cap, next_freq, wl_type)
#endif /* CONFIG_NONLINEAR_FREQ_CTL */

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
DECLARE_PER_CPU(int, cpufreq_idle_cpu);
DECLARE_PER_CPU(spinlock_t, cpufreq_idle_cpu_lock);
unsigned long mtk_cpu_util(unsigned int cpu, unsigned long util_rq,
				enum cpu_util_type type,
				struct task_struct *p,
				unsigned long min_cap, unsigned long max_cap);
int dequeue_idle_cpu(int cpu);
#endif
__always_inline
unsigned long mtk_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p,
				  unsigned long min_cap, unsigned long max_cap);

#if IS_ENABLED(CONFIG_RT_GROUP_SCHED)
static inline int rt_rq_throttled(struct rt_rq *rt_rq)
{
	return rt_rq->rt_throttled && !rt_rq->rt_nr_boosted;
}
#else /* !CONFIG_RT_GROUP_SCHED */
static inline int rt_rq_throttled(struct rt_rq *rt_rq)
{
	return rt_rq->rt_throttled;
}
#endif

extern int set_target_margin(int gearid, int margin);
extern int set_target_margin_low(int gearid, int margin);
extern int set_turn_point_freq(int gearid, unsigned long freq);

#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
extern bool sysctl_util_est;
#endif

static inline bool is_util_est_enable(void)
{
#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
	return sysctl_util_est;
#else
	return true;
#endif
}

static inline unsigned long mtk_cpu_util_cfs(struct rq *rq)
{
	unsigned long util = READ_ONCE(rq->cfs.avg.util_avg);

	if (sched_feat(UTIL_EST) && is_util_est_enable()) {
		util = max_t(unsigned long, util,
			     READ_ONCE(rq->cfs.avg.util_est.enqueued));
	}

	return util;
}

#endif /* _SCHED_COMMON_H */
