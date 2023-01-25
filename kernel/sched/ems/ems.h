/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/ems.h>

enum task_cgroup {
	CGROUP_ROOT,
	CGROUP_FOREGROUND,
	CGROUP_BACKGROUND,
	CGROUP_TOPAPP,
	CGROUP_RT,
	CGROUP_SYSTEM,
	CGROUP_SYSTEM_BACKGROUND,
	CGROUP_NNAPI_HAL,
	CGROUP_CAMERA_DAEMON,
	CGROUP_COUNT,
};

/*
 * Maximum supported-by-vendor processors.  Setting this smaller saves quite a
 * bit of memory.  Use nr_cpu_ids instead of this except for static bitmaps.
 */
#ifndef CONFIG_VENDOR_NR_CPUS
/* FIXME: This should be fixed in the arch's Kconfig */
#define CONFIG_VENDOR_NR_CPUS	1
#endif

/* Places which use this should consider cpumask_var_t. */
#define VENDOR_NR_CPUS		CONFIG_VENDOR_NR_CPUS

/* maximum count of tracking tasks in runqueue */
#define TRACK_TASK_COUNT	5

/* support flag-handling for EMS */
#define EMS_PF_GET(task)		(task->android_vendor_data1[2])
#define EMS_PF_SET(task, value)		(task->android_vendor_data1[2] |= (value))
#define EMS_PF_CLEAR(task, value)	(task->android_vendor_data1[2] &= ~(value))

#define EMS_PF_MULLIGAN			0x00000001	/* I'm given a mulligan */
#define EMS_PF_RUNNABLE_BUSY		0x00000002	/* Picked from runnable busy cpu */

/*
 * Vendor data handling for TEX queue jump
 *
 * rq  's android_vendor_data1 indices 0, 1    are used
 * task's android_vendor_data1 indices 3, 4, 5 are used
 */
#define ems_qjump_list(rq)		((struct list_head *)&(rq->android_vendor_data1[0]))
#define ems_qjump_node(task)		((struct list_head *)&(task->android_vendor_data1[3]))
#define ems_qjump_queued(task)		(task->android_vendor_data1[5])

#define ems_qjump_first_entry(list)	({						\
	void *__mptr = (void *)(list->next);						\
	(struct task_struct *)(__mptr -							\
				offsetof(struct task_struct, android_vendor_data1[3]));	\
})

/* structure for task placement environment */
struct tp_env {
	struct task_struct *p;
	int cgroup_idx;
	int per_cpu_kthread;

	int sched_policy;
	int cl_sync;
	int sync;

	struct cpumask cpus_allowed;
	struct cpumask fit_cpus;

	unsigned long weight[VENDOR_NR_CPUS];	/* efficiency weight */

	unsigned long task_util;
	unsigned long task_util_clamped;
	unsigned long task_load_avg;
	struct {
		unsigned long util;
		unsigned long util_wo;
		unsigned long util_with;
		unsigned long runnable;
		unsigned long load_avg;
		unsigned long rt_util;
		unsigned long dl_util;
		unsigned long nr_running;
		int idle;
	} cpu_stat[VENDOR_NR_CPUS];

	int idle_cpu_count;
};

/* cpu selection domain */
struct cs_domain {
	struct list_head list;
	struct cpumask cpus;
	unsigned long next_freq;
	unsigned long next_volt;
	int apply_sp;
};

/* core */
enum {
	RUNNING = 0,
	IDLE_START,
	IDLE_END,
};

extern int ems_select_task_rq_fair(struct task_struct *p,
		int prev_cpu, int sd_flag, int wake_flag);
extern int __ems_select_task_rq_fair(struct task_struct *p, int prev_cpu,
			   int sd_flag, int wake_flag);
extern int ems_select_task_rq_rt(struct task_struct *p,
		int prev_cpu, int sd_flag, int wake_flag);
extern int ems_select_fallback_rq(struct task_struct *p, int target_cpu);
extern void ems_tick(struct rq *rq);
extern int ems_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void ems_enqueue_task(struct rq *rq, struct task_struct *p, int flags);
extern void ems_dequeue_task(struct rq *rq, struct task_struct *p, int flags);
extern void ems_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				       struct sched_entity **se_ptr, bool *repick,
				       bool simple, struct task_struct *prev);
extern void ems_cpu_cgroup_can_attach(struct cgroup_taskset *tset, int can_attach);
extern int ems_load_balance(struct rq *rq);
extern void ems_post_init_entity_util_avg(struct sched_entity *se);
extern int ems_find_new_ilb(struct cpumask *nohz_idle_cpus_mask);
extern void ems_idle_enter(int cpu, int *state);
extern void ems_idle_exit(int cpu, int state);
extern void ems_schedule(struct task_struct *prev, struct task_struct *next, struct rq *rq);

extern int core_init(struct device_node *ems_dn);
extern int hook_init(void);

int find_cpus_allowed(struct tp_env *env);

/* cpus binding */
const struct cpumask *cpus_binding_mask(struct task_struct *p);

/* energy table */
struct energy_state {
	unsigned long frequency;
	unsigned long capacity;
	unsigned long voltage;
	unsigned long dynamic_power;
	unsigned long static_power;
};

extern int et_init(struct kobject *ems_kobj);
extern void et_init_table(struct cpufreq_policy *policy);
extern unsigned long et_max_cap(int cpu);
extern unsigned long et_max_dpower(int cpu);
extern unsigned long et_min_dpower(int cpu);
extern unsigned long et_cap_to_freq(int cpu, unsigned long cap);
extern unsigned long et_freq_to_cap(int cpu, unsigned long freq);
extern unsigned long et_freq_to_dpower(int cpu, unsigned long freq);
extern unsigned long et_dpower_to_cap(int cpu, unsigned long power);
extern unsigned int et_freq_to_volt(int cpu, unsigned int freq);
extern int et_get_orig_state_with_freq(int cpu, unsigned long freq, struct energy_state *state);
extern int et_get_state_with_freq(int cpu, unsigned long freq,
					struct energy_state *state, int dis_buck_share);
extern void et_update_freq(int cpu, unsigned long freq);
extern unsigned long et_compute_energy(const struct cpumask *cpus, int target_cpu, unsigned long freq,
					unsigned long *utils, unsigned long *weights, int apply_sp);
extern unsigned long et_compute_cpu_energy(int cpu, unsigned long freq,
					unsigned long util, unsigned long weight);
extern unsigned long et_compute_system_energy(struct list_head *csd_head,
					unsigned long *utils, unsigned long *weights,
					int target_cpu);

/* multi load */
extern void mlt_update(int cpu);
extern void mlt_idle_enter(int cpu, int cstate);
extern void mlt_idle_exit(int cpu);
extern void mlt_task_switch(int cpu, struct task_struct *next, int state);
extern int mlt_cur_period(int cpu);
extern int mlt_prev_period(int period);
extern int mlt_period_with_delta(int idx, int delta);
extern int mlt_art_value(int cpu, int idx);
extern int mlt_art_last_value(int cpu);
extern int mlt_art_cgroup_value(int cpu, int idx, int cgroup);
extern int mlt_cst_value(int cpu, int idx, int cstate);
extern int mlt_init(void);
extern void ntu_apply(struct sched_entity *se);
extern void ntu_init(struct kobject *ems_kobj);
extern unsigned long ml_task_util(struct task_struct *p);
extern unsigned long ml_task_util_est(struct task_struct *p);
extern unsigned long ml_task_load_avg(struct task_struct *p);
extern unsigned long ml_cpu_util(int cpu);
extern unsigned long ml_cpu_util_with(struct task_struct *p, int dst_cpu);
extern unsigned long ml_cpu_util_without(int cpu, struct task_struct *p);
extern unsigned long ml_cpu_load_avg(int cpu);

#define MLT_PERIOD_SIZE		(4 * NSEC_PER_MSEC)
#define MLT_PERIOD_COUNT	40

/* Active Ratio Tracking */
struct track_data {
	int     **periods;
	u64	*recent_sum;
	int     *recent;

	int	state;
	int	state_count;
};

enum {
	CLKOFF = 0,
	PWROFF,
	CSTATE_MAX,
};

struct mlt {
	int			cpu;
	u64			period_start;
	u64			last_updated;

	int			cur_period;

	struct track_data	art;	/* Active Ratio Tracking */
	struct track_data	cst;	/* C-State Tracking */
};

/* efficiency cpu selection */
extern int find_best_cpu(struct tp_env *env);

/*
 * state
 */
extern int stt_init(struct kobject *ems_kobj);
extern int stt_cluster_state(int cpu);
extern int stt_heavy_tsk_boost(int cpu);
extern void stt_update(struct rq *rq, struct task_struct *p);
extern void stt_enqueue_task(struct rq *rq, struct task_struct *p);
extern void stt_dequeue_task(struct rq *rq, struct task_struct *p);

/* ontime migration */
struct ontime_dom {
	struct list_head	node;

	unsigned long		upper_boundary;
	int			upper_ratio;
	unsigned long		lower_boundary;
	int			lower_ratio;

	struct cpumask		cpus;
};

extern int ontime_init(struct kobject *ems_kobj);
extern int ontime_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void ontime_select_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus);
extern void ontime_migration(void);

/* cpufreq */
struct fclamp_data {
	int freq;
	int target_period;
	int target_ratio;
	int type;
};

extern int cpufreq_get_next_cap(struct tp_env *env, struct cpumask *cpus, int dst_cpu, bool apply_clamp);
extern void cpufreq_register_hook(int (*fn)(struct tp_env *env, struct cpumask *cpus, int dst_cpu, bool clamp));
extern void cpufreq_unregister_hook(void);
extern unsigned int fclamp_apply(struct cpufreq_policy *policy, unsigned int orig_freq);
extern int fclamp_init(void);

/* esg */
extern int esgov_pre_init(struct kobject *ems_kobj);
extern int get_gov_next_cap(struct tp_env *env, struct cpumask *cpus, int dst_cpu, bool apply_clamp);

/* ego */
extern int ego_pre_init(struct kobject *ems_kobj);

/* freqboost */
extern int freqboost_init(void);
extern void freqboost_enqueue_task(struct task_struct *p, int cpu, int flags);
extern void freqboost_dequeue_task(struct task_struct *p, int cpu, int flags);
extern int freqboost_can_attach(struct cgroup_taskset *tset);
extern unsigned long freqboost_cpu_boost(int cpu, unsigned long util);
extern int freqboost_get_task_ratio(struct task_struct *p);
extern unsigned long wakeboost_cpu_boost(int cpu, unsigned long util);
extern int wakeboost_pending(int cpu);
extern unsigned long heavytask_cpu_boost(int cpu, unsigned long util);

/* frt */
extern int frt_init(struct kobject *ems_kobj);
extern void frt_update_available_cpus(struct rq *rq);
extern int frt_find_lowest_rq(struct task_struct *task, struct cpumask *lowest_mask);
extern int frt_select_task_rq_rt(struct task_struct *p, int prev_cpu, int sd_flag);

/* core sparing */
struct ecs_stage {
	struct list_head	node;

	unsigned int		id;
	unsigned int		busy_threshold;

	struct cpumask		cpus;
};

struct ecs_domain {
	struct list_head	node;

	unsigned int		id;
	unsigned int		busy_threshold_ratio;

	struct cpumask		cpus;

	struct ecs_stage	*cur_stage;
	struct list_head	stage_list;
};

extern int ecs_init(struct kobject *ems_kobj);
extern const struct cpumask *ecs_cpus_allowed(struct task_struct *p);
extern void ecs_update(void);
extern int ecs_cpu_available(int cpu, struct task_struct *p);

/*
 * Priority-pinning
 */
extern int tex_task(struct task_struct *p);
extern int tex_suppress_task(struct task_struct *p);
extern void tex_enqueue_task(struct task_struct *p, int cpu);
extern void tex_dequeue_task(struct task_struct *p, int cpu);
extern void tex_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				       struct sched_entity **se_ptr, bool *repick,
				       bool simple, struct task_struct *prev);

/* EMSTune */
extern int emstune_init(struct kobject *ems_kobj, struct device_node *dn);
extern void emstune_ontime_init(void);

/* emstune - energy table */
struct emstune_specific_energy_table {
	unsigned int mips_mhz[VENDOR_NR_CPUS];
	unsigned int dynamic_coeff[VENDOR_NR_CPUS];
};

/* emstune - sched policy */
struct emstune_sched_policy {
	int policy[CGROUP_COUNT];
};

/* emstune - active weight */
struct emstune_active_weight {
	int ratio[CGROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - idle weight */
struct emstune_idle_weight {
	int ratio[CGROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - freq boost */
struct emstune_freqboost {
	int ratio[CGROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - wakeup boost */
struct emstune_wakeboost {
	int ratio[CGROUP_COUNT][VENDOR_NR_CPUS];
};

/* emstune - energy step governor */
struct emstune_esg {
	int step[VENDOR_NR_CPUS];
	int pelt_margin[VENDOR_NR_CPUS];
};

/* emstune - cpufreq governor parameters */
struct emstune_cpufreq_gov {
	int patient_mode[VENDOR_NR_CPUS];
	int pelt_boost[VENDOR_NR_CPUS];
	int split_pelt_margin[VENDOR_NR_CPUS];
	unsigned int split_pelt_margin_freq[VENDOR_NR_CPUS];

	int split_up_rate_limit[VENDOR_NR_CPUS];
	unsigned int split_up_rate_limit_freq[VENDOR_NR_CPUS];
	int down_rate_limit;
	int rapid_scale_up;
	int rapid_scale_down;
	int dis_buck_share[VENDOR_NR_CPUS];
};

/* emstune - ontime migration */
struct emstune_ontime {
	int enabled[CGROUP_COUNT];
	struct list_head *p_dom_list;
	struct list_head dom_list;
};

/* emstune - priority pinning */
struct emstune_tex {
	struct cpumask mask;
	int qjump;
	int enabled[CGROUP_COUNT];
	int prio;
	int suppress;
};

/* emstune - stt */
struct emstune_stt {
	int htask_enable[CGROUP_COUNT];
	int htask_cnt;
	int boost_step;
};

/* emstune - fclamp */
struct emstune_fclamp {
	int min_freq[VENDOR_NR_CPUS];
	int min_target_period[VENDOR_NR_CPUS];
	int min_target_ratio[VENDOR_NR_CPUS];

	int max_freq[VENDOR_NR_CPUS];
	int max_target_period[VENDOR_NR_CPUS];
	int max_target_ratio[VENDOR_NR_CPUS];

	int monitor_group[CGROUP_COUNT];
};

struct emstune_sync {
	int apply;
};

/* SCHED CLASS */
#define EMS_SCHED_STOP		(1 << 0)
#define EMS_SCHED_DL		(1 << 1)
#define EMS_SCHED_RT		(1 << 2)
#define EMS_SCHED_FAIR		(1 << 3)
#define EMS_SCHED_IDLE		(1 << 4)
#define NUM_OF_SCHED_CLASS	5

#define EMS_SCHED_CLASS_MASK	(0x1F)

/* emstune - cpus allowed */
struct emstune_cpus_binding {
	unsigned long target_sched_class;
	struct cpumask mask[CGROUP_COUNT];
};

/* emstune - fluid rt */
struct emstune_frt {
	int active_ratio[VENDOR_NR_CPUS];
};

/* emstune - core sparing */
struct emstune_ecs {
	struct list_head domain_list;
};

/* emstune - prefer cpu */
struct emstune_prefer_cpu {
	struct cpumask mask[CGROUP_COUNT];
	unsigned long small_task_threshold;
	struct cpumask small_task_mask;
};

/* emstune - new task util */
struct emstune_ntu {
	int ratio[CGROUP_COUNT];
};

struct emstune_set {
	int					type;

	struct emstune_specific_energy_table	specific_energy_table;
	struct emstune_sched_policy		sched_policy;
	struct emstune_active_weight		active_weight;
	struct emstune_idle_weight		idle_weight;
	struct emstune_freqboost		freqboost;
	struct emstune_wakeboost		wakeboost;
	struct emstune_esg			esg;
	struct emstune_cpufreq_gov		cpufreq_gov;
	struct emstune_ontime			ontime;
	struct emstune_tex			tex;
	struct emstune_cpus_binding		cpus_binding;
	struct emstune_frt			frt;
	struct emstune_ecs			ecs;
	struct emstune_prefer_cpu		prefer_cpu;
	struct emstune_ntu			ntu;
	struct emstune_stt			stt;
	struct emstune_fclamp			fclamp;
	struct emstune_sync			sync;
};

struct emstune_mode {
	const char			*desc;
	struct emstune_set		*sets;
};

extern int emstune_sched_boost(void);
extern int emstune_task_boost(void);

/* profile */
struct system_profile_data {
	int			busy_cpu_count;
	int			heavy_task_count;
	int			misfit_task_count;

	unsigned long		cpu_util_sum;
	unsigned long		heavy_task_util_sum;
	unsigned long		misfit_task_util_sum;
	unsigned long		heaviest_task_util;

	unsigned long		cpu_util[VENDOR_NR_CPUS];
};

#define HEAVY_TASK_UTIL_RATIO	(40)
#define MISFIT_TASK_UTIL_RATIO	(80)
#define check_busy(util, cap)	((util * 100) >= (cap * 80))

static inline int is_heavy_task_util(unsigned long util)
{
	return util > (SCHED_CAPACITY_SCALE * HEAVY_TASK_UTIL_RATIO / 100);
}

static inline int is_misfit_task_util(unsigned long util)
{
	return util > (SCHED_CAPACITY_SCALE * MISFIT_TASK_UTIL_RATIO / 100);
}

extern int profile_sched_init(struct kobject *);
extern int profile_sched_data(void);
extern void get_system_sched_data(struct system_profile_data *);

/* Sysbusy */
struct sysbusy_param {
	int monitor_interval;		/* tick (1 tick = 4ms) */
	int release_duration;		/* tick (1 tick = 4ms) */
	unsigned long allowed_next_state;
};

#define TICK_SEC	250
static struct sysbusy_param sysbusy_params[] = {
	{
		/* SYSBUSY_STATE0 (sysbusy inactivation) */
		.monitor_interval	= 1,
		.release_duration	= 0,
		.allowed_next_state	= (1 << SYSBUSY_STATE1) |
					  (1 << SYSBUSY_STATE2) |
					  (1 << SYSBUSY_STATE3),
	},
	{
		/* SYSBUSY_STATE1 */
		.monitor_interval	= 1,
		.release_duration	= 2,
		.allowed_next_state	= (1 << SYSBUSY_STATE0) |
					  (1 << SYSBUSY_STATE2) |
					  (1 << SYSBUSY_STATE3),
	},
	{
		/* SYSBUSY_STATE2 */
		.monitor_interval	= 25,
		.release_duration	= TICK_SEC * 3,
		.allowed_next_state	= (1 << SYSBUSY_STATE0) |
					  (1 << SYSBUSY_STATE3),
	},
	{
		/* SYSBUSY_STATE3 */
		.monitor_interval	= 25,
		.release_duration	= TICK_SEC * 9,
		.allowed_next_state	= (1 << SYSBUSY_STATE0),
	},
};

#define SYSBUSY_SOMAC	SYSBUSY_STATE3

extern int sysbusy_init(struct kobject *ems_kobj);
extern void monitor_sysbusy(void);
extern int sysbusy_schedule(struct tp_env *env);
extern int sysbusy_activated(void);
extern void somac_tasks(void);
extern int sysbusy_on_somac(void);
extern int is_somac_ready(struct tp_env *env);

/* IOCTL interface */
#define EMS_IOCTL_MAGIC	'E'

#define EMS_IOCTL_R_ECS_DOMAIN_COUNT		_IOR(EMS_IOCTL_MAGIC, 0x00, int)
#define EMS_IOCTL_R_ECS_DOMAIN_INFO		_IOR(EMS_IOCTL_MAGIC, 0x01, int)
#define EMS_IOCTL_W_ECS_TARGET_DOMAIN_ID	_IOW(EMS_IOCTL_MAGIC, 0x02, int)
#define EMS_IOCTL_W_ECS_TARGET_STAGE_ID		_IOW(EMS_IOCTL_MAGIC, 0x03, int)
#define EMS_IOCTL_W_ECS_STAGE_THRESHOLD		_IOW(EMS_IOCTL_MAGIC, 0x04, int)

#define NR_ECS_ENERGY_STATES		(1 << 3)
#define NR_ECS_STAGES			(1 << 3)
struct ems_ioctl_ecs_domain_info {
	unsigned long		cpus;
	int			cd_dp_ratio[VENDOR_NR_CPUS];
	int			cd_cp_ratio[VENDOR_NR_CPUS];
	int			num_of_state;
	struct {
		unsigned long frequency;
		unsigned long capacity;
		unsigned long v_square;
		unsigned long dynamic_power;
		unsigned long static_power;
	} states[VENDOR_NR_CPUS][NR_ECS_ENERGY_STATES];
	int			num_of_stage;
	unsigned long		cpus_each_stage[NR_ECS_STAGES];
};

extern int ecs_ioctl_get_domain_count(void);
extern int ecs_ioctl_get_ecs_domain_info(struct ems_ioctl_ecs_domain_info *info);
extern void ecs_ioctl_set_target_domain(unsigned int domain_id);
extern void ecs_ioctl_set_target_stage(unsigned int stage_id);
extern void ecs_ioctl_set_stage_threshold(unsigned int threshold);

/******************************************************************************
 * common API                                                                 *
 ******************************************************************************/
static inline unsigned long capacity_cpu_orig(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity_orig;
}

static inline unsigned long capacity_cpu(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;
}

static inline int cpu_overutilized(int cpu)
{
	return (capacity_cpu(cpu) * 1024) < (ml_cpu_util(cpu) * 1280);
}

static inline struct task_struct *task_of(struct sched_entity *se)
{
	return container_of(se, struct task_struct, se);
}

static inline struct sched_entity *get_task_entity(struct sched_entity *se)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	struct cfs_rq *cfs_rq = se->my_q;

	while (cfs_rq) {
		se = cfs_rq->curr;
		cfs_rq = se->my_q;
	}
#endif

	return se;
}

static inline struct cfs_rq *cfs_rq_of(struct sched_entity *se)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	return se->cfs_rq;
#else
	return &task_rq(task_of(se))->cfs;
#endif
}

static inline bool is_dst_allowed(struct task_struct *p, int cpu)
{
	if (is_per_cpu_kthread(p) &&
		(cpu != cpumask_any(p->cpus_ptr)))
		return false;

	return cpu_active(cpu);
}

#ifdef CONFIG_UCLAMP_TASK
static inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
	return clamp(ml_task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
	return ml_task_util_est(p);
}
#endif

extern int busy_cpu_ratio;
static inline bool __is_busy_cpu(unsigned long util,
				 unsigned long runnable,
				 unsigned long capacity,
				 unsigned long nr_running)
{
	if (runnable < capacity)
		return false;

	if (!nr_running)
		return false;

	if (util * busy_cpu_ratio >= runnable * 100)
		return false;

	return true;
}

static inline bool is_busy_cpu(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned long util, runnable, capacity, nr_running;

	cfs_rq = &cpu_rq(cpu)->cfs;

	util = ml_cpu_util(cpu);
	runnable = READ_ONCE(cfs_rq->avg.runnable_avg);
	capacity = capacity_orig_of(cpu);
	nr_running = cpu_rq(cpu)->nr_running;

	return __is_busy_cpu(util, runnable, capacity, nr_running);
}

extern int get_sched_class_idx(const struct sched_class *class);
extern int cpuctl_task_group_idx(struct task_struct *p);
extern const struct cpumask *cpu_coregroup_mask(int cpu);
extern const struct cpumask *cpu_slowest_mask(void);
extern const struct cpumask *cpu_fastest_mask(void);
extern int send_uevent(char *msg);

extern void gsc_init(struct kobject *ems_kobj);
extern void gsc_init_new_task(struct task_struct *p);
extern void gsc_update_tick(struct rq *rq);
extern void gsc_task_update_stat(struct task_struct *p, unsigned long prv_util);
extern void gsc_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus);
extern int  gsc_activated(struct task_struct *p);
extern void gsc_task_cgroup_attach(struct cgroup_taskset *tset);
extern void gsc_flush_task(struct task_struct *p);

/* balance */
#define SCHED_MIGRATION_COST	500000
extern void ems_newidle_balance(void *data, struct rq *this_rq, struct rq_flags *rf, int *pulled_task, int *done);
