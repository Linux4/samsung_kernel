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

#ifndef _KERNEL_SCHED_EMS_H
#define _KERNEL_SCHED_EMS_H

#include <linux/ems.h>

#include "../../../drivers/android/binder_internal.h"
#include "../sched.h"

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
#define TRACK_TASK_COUNT	10

#ifndef WF_ANDROID_VENDOR
#define WF_ANDROID_VENDOR	0
#endif

/* TopApp main / render or Important task's prio */
#define TA_BOOST_PRIO	110

enum ems_level {
	EMS_LEVEL_NORMAL = 0,
	EMS_LEVEL_BOOST,
	EMS_LEVEL_PERFORMANCE,
	EMS_LEVEL_COUNT,
};

enum task_cgroup {
	CGROUP_ROOT,
	CGROUP_FOREGROUND,
	CGROUP_BACKGROUND,
	CGROUP_TOPAPP,
	CGROUP_RT,
	CGROUP_SYSTEM,
	CGROUP_SYSTEM_BACKGROUND,
	CGROUP_DEX2OAT,
	CGROUP_NNAPI_HAL,
	CGROUP_CAMERA_DAEMON,
	CGROUP_MIDGROUND,
	CGROUP_COUNT,
};

enum cluster_max_num {
	CLUSTER_DUAL = 2,
	CLUSTER_TRI,
	CLUSTER_QUAD,
};

/* structure for task placement environment */
struct tp_env {
	struct task_struct *p;
	int cgroup_idx;
	int per_cpu_kthread;

	int sched_policy;
	int cl_sync;
	int sync;
	int prev_cpu;
	int target_cpu;

	int task_class;
	int pl_init_index;
	int pl_depth;
	unsigned long base_cap;

	int reason_of_selection;

	struct cpumask cpus_allowed;
	struct cpumask fit_cpus;
	struct cpumask overcap_cpus;
	struct cpumask migrating_cpus;

	unsigned long weight[VENDOR_NR_CPUS];	/* efficiency weight */

	unsigned long task_util;
	unsigned long task_util_clamped;
	unsigned long task_load_avg;
	struct {
		unsigned long util;
		unsigned long util_wo;
		unsigned long util_with;
		unsigned long rt_util;
		unsigned long dl_util;
		unsigned long nr_running;
		int runnable;
		int idle;
	} cpu_stat[VENDOR_NR_CPUS];

	int		ipc;
	int		table_index;
};

/* cpu selection domain */
struct cs_domain {
	struct list_head list;
	struct cpumask cpus;
};

/* core */
enum {
	NON_IDLE = 0,
	IDLE_C1,
	IDLE_C2,
};

extern int core_init(struct device_node *ems_dn);
extern int hook_init(void);

int find_cpus_allowed(struct tp_env *env);

/* List of process element */
struct pe_list {
	struct cpumask *cpus;
	int num_of_cpus;
};

extern struct pe_list *get_pe_list(int index);
extern int get_pe_list_size(void);
extern void ems_update_cpu_capacity(int cpu);

/* emstune mode send to user */
extern void send_mode_to_user(int mode);

/* cpus binding */

/* energy table */
struct energy_state {
	unsigned long capacity;
	unsigned long util;
	unsigned long weight;
	unsigned long frequency;
	unsigned long voltage;
	unsigned long dynamic_power;
	int idle;
	int temperature;
};

struct energy_backup {
	unsigned long frequency;
	unsigned long voltage;
	unsigned long energy;
};

#define MLT_MAX_RAW_NUM		8
static char *cpu_uarch_name[] = {
	"ipc",
	"mpki",
};
enum mlt_uarch_cpu {
	MLT_UARCH_CPU_IPC,		/* INST_RETIRED / CPU_CYCLE */
	MLT_UARCH_CPU_LLC_MPKI,		/* LLC_MISS * 1000 / INST_RETIRED */
	MLT_UARCH_CPU_NUM,
};
enum mlt_uarch_cpu_raw {
	MLT_RAW_CPU_CPU_CYCLE,
	MLT_RAW_CPU_INST_RETIRED,
	MLT_RAW_CPU_LLC_MISS,
	MLT_RAW_CPU_NUM,
};

static char *tsk_uarch_name[] = {
	"ipc",
};
enum mlt_uarch_tsk {
	MLT_UARCH_TSK_IPC,		/* INST_RETIRED / CPU_CYCLE */
	MLT_UARCH_TSK_NUM,
};
enum mlt_uarch_tsk_raw {
	MLT_RAW_TSK_CPU_CYCLE,
	MLT_RAW_TSK_INST_RETIRED,
	MLT_RAW_TSK_NUM,
};

/* multi load tracker */
#define INACTIVE	(0)
#define	ACTIVE		(1)
#define MLT_STATE_NOCHANGE	0xBABE

#define MLT_RUNNABLE_UNIT		1000
#define MLT_RUNNABLE_ROUNDS_UNIT	500
#define MLT_RUNNABLE_UP_UNIT		999

#define MLT_PERIOD_SIZE		(4 * NSEC_PER_MSEC)
#define MLT_PERIOD_SIZE_MIN	(MLT_PERIOD_SIZE - (MLT_PERIOD_SIZE >> 4))
#define MLT_PERIOD_COUNT	8
#define MLT_IDLE_THR_TIME	(8 * NSEC_PER_MSEC)
#define MLT_IRQ_LOAD_RATIO	(95)

enum mlt_value_type {
	PERIOD,			/* completed period matched with index */
	AVERAGE,		/* average of all periods */
};

struct mlt_part {
	int		state;
	int		cur_period;
	int		active_period;
	u64		period_start;
	u64		last_updated;
	int		periods[MLT_PERIOD_COUNT];
	int		recent;
	u64		contrib;
};

struct mlt_uarch {
	u32		periods[MLT_PERIOD_COUNT];
	u32		recent; /* recent = (contrib[X] * 1000) / contrib[Y] */
};

struct mlt_uarch_raw {
	u64		contrib;	/* current_uarch_raw - last_uarch_raw */
	u64		last;
};

struct mlt_uarch_snap {
	u64		last[MLT_PERIOD_COUNT];		/* snapshot */
};

struct mlt_runnable {
	int		runnable;
	int		nr_run;
	u64		contrib;
	u64		runnable_contrib;
	u64		last_updated;
};

struct mlt_cpu {
	int			cpu;

	struct mlt_part		part;
	struct mlt_runnable	runnable;
	struct mlt_uarch	uarch[MLT_UARCH_CPU_NUM];
	struct mlt_uarch_raw	uarch_raw[MLT_RAW_CPU_NUM];
	struct mlt_uarch_snap	uarch_snap[MLT_RAW_CPU_NUM];
};

struct mlt_task {
	struct mlt_part		part;
	struct mlt_uarch	uarch[MLT_UARCH_TSK_NUM];
	struct mlt_uarch_raw	uarch_raw[MLT_RAW_TSK_NUM];
};

struct mlt_env {
	struct mlt_part		*part;
	struct mlt_uarch	*uarch;
	struct mlt_uarch_raw	*raw;
	struct mlt_uarch_snap	*snap;
	int			next_state;
	bool			is_task;
	bool			update_uarch;
	bool			tick;
};

extern u32 mlt_tsk_uarch(struct task_struct *p, enum mlt_uarch_tsk type, int size);
extern u32 mlt_cpu_uarch(int cpu, enum mlt_uarch_cpu type, int size);
extern u64 mlt_cpu_uarch_snap(int cpu, enum mlt_uarch_cpu_raw type, int period);
extern int mlt_runnable(struct rq *rq);
extern void mlt_enqueue_task(struct rq *rq);
extern void mlt_dequeue_task(struct rq *rq);
extern void mlt_update_task(struct task_struct *p, int next_state, u64 now, bool update_uarch, bool tick);
extern void mlt_update_cpu(int cpu, int next_state, u64 now, bool update_uarch, bool tick);
extern void mlt_tick(void);
extern void mlt_task_switch(int cpu, struct task_struct *prev, struct task_struct *next);
extern int mlt_cur_period(int cpu);
extern int mlt_period_with_delta(int idx, int delta);
extern u64 mlt_cpu_value(int cpu, int arg, enum mlt_value_type type, int state);
extern void mlt_init_task(struct task_struct *p);
extern u64 mlt_task_value(struct task_struct *p, int arg, enum mlt_value_type type);
extern int mlt_task_cur_period(struct task_struct *p);
extern int mlt_init(struct kobject *ems_kobj, struct device_node *dn);
extern unsigned long _ml_task_util_est(struct task_struct *p);
extern unsigned long ml_task_util(struct task_struct *p);
extern unsigned long ml_task_util_est(struct task_struct *p);
extern unsigned long ml_task_load_avg(struct task_struct *p);
extern unsigned long ml_cpu_util(int cpu);
extern unsigned long ml_cpu_util_with(struct task_struct *p, int dst_cpu);
extern unsigned long ml_cpu_util_without(int cpu, struct task_struct *p);
extern unsigned long ml_cpu_load_avg(int cpu);
extern unsigned long ml_cpu_irq_load(int cpu);
extern int mlt_get_runnable_cpu(int cpu);

/* ontime migration */
struct ontime_dom {
	struct list_head	node;

	unsigned long		upper_boundary;
	int			upper_ratio;
	unsigned long		lower_boundary;
	int			lower_ratio;

	struct cpumask		cpus;
};

/* For ecs governor - dynamic */
struct dynamic_dom {
	struct list_head	node;
	unsigned int		id;
	unsigned int		busy_ratio;
	struct cpumask		cpus;			/* sibling cpus of this domain */
	struct cpumask		default_cpus;		/* minimum cpus */
	struct cpumask		governor_cpus;		/* governor target cpus */
	bool			has_spared_cpu;

	/* raw data */
	int flag;
	int nr_cpu;
	int nr_busy_cpu;
	int avg_nr_run;
	int slower_misfit;
	int active_ratio;
	unsigned long util;
	unsigned long cap;
	int throttle_cnt;
	int nr_run_up_ratio;
};

#define ECS_USER_NAME_LEN 	(16)
struct ecs_governor {
	char name[ECS_USER_NAME_LEN];
	struct list_head	list;
	void (*update)(void);
	void (*enqueue_update)(int prev_cpu, struct task_struct *p);
	void (*start)(void);
	void (*stop)(void);
	const struct cpumask *(*get_target_cpus)(void);
};

/*
 * Priority-pinning
 */
enum {
	BOOSTED_TEX = 0,
	BINDER_TEX,
	RENDER_TEX,
	PRIO_TEX,
	LOW_LATENCY_TEX,
	NOT_TEX,
};

#define TEX_WINDOW		(4000000)
#define TEX_WINDOW_COUNT	(3)
#define TEX_FULL_WINDOWS	(TEX_WINDOW * TEX_WINDOW_COUNT)

/* gsc */
enum gsc_level {
	GSC_LEVEL1 = 1,
	GSC_LEVEL2,
	GSC_LEVEL_COUNT,
};
#define valid_gsc_level(level)	(GSC_LEVEL1 <= (level) && (level) < GSC_LEVEL_COUNT)

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

/* emstune - cpufreq governor parameters */
struct emstune_cpufreq_gov {
	int htask_boost[VENDOR_NR_CPUS];
	int pelt_boost[VENDOR_NR_CPUS];
	int window_count[VENDOR_NR_CPUS];
	int htask_ratio_threshold[VENDOR_NR_CPUS];
};

struct emstune_cpuidle_gov {
	int expired_ratio[VENDOR_NR_CPUS];
};

/* emstune - ontime migration */
struct emstune_ontime {
	int enabled[CGROUP_COUNT];
	struct list_head *p_dom_list;
	struct list_head dom_list;
};

/* emstune - priority pinning */
struct emstune_tex {
	int prio[CGROUP_COUNT];
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

/* SCHED CLASS */
#define EMS_SCHED_STOP		(1 << 0)
#define EMS_SCHED_DL		(1 << 1)
#define EMS_SCHED_RT		(1 << 2)
#define EMS_SCHED_FAIR		(1 << 3)
#define EMS_SCHED_IDLE		(1 << 4)
#define NUM_OF_SCHED_CLASS	5

/* emstune - cpus allowed */
struct emstune_cpus_binding {
	struct cpumask mask[CGROUP_COUNT];
};

struct emstune_ecs_dynamic {
	int dynamic_busy_ratio[VENDOR_NR_CPUS];
	struct cpumask min_cpus;
};

/* emstune - new task util */
struct emstune_ntu {
	int ratio[CGROUP_COUNT];
};

struct emstune_support_uclamp {
	int enabled;
};

struct emstune_gsc {
	unsigned int up_threshold[GSC_LEVEL_COUNT];
	unsigned int down_threshold[GSC_LEVEL_COUNT];
	unsigned int boost_en_threshold;
	unsigned int boost_dis_threshold;
	unsigned int boost_ratio;
};

struct emstune_should_spread {
	int enabled;
};

struct emstune_frt_boost {
	int enabled[CGROUP_COUNT];
};

struct emstune_cgroup_boost {
	int enabled[CGROUP_COUNT];
	unsigned int enabled_bit;
};

struct emstune_energy_table {
	unsigned int default_idx;
	int uarch_selection;
};

struct emstune_set {
	int					mode;
	int					level;

	struct emstune_active_weight		active_weight;
	struct emstune_idle_weight		idle_weight;
	struct emstune_freqboost		freqboost;
	struct emstune_cpufreq_gov		cpufreq_gov;
	struct emstune_cpuidle_gov		cpuidle_gov;
	struct emstune_ontime			ontime;
	struct emstune_tex			tex;
	struct emstune_cpus_binding		cpus_binding;
	struct emstune_ecs_dynamic		ecs_dynamic;
	struct emstune_ntu			ntu;
	struct emstune_fclamp			fclamp;
	struct emstune_support_uclamp		support_uclamp;
	struct emstune_gsc			gsc;
	struct emstune_should_spread		should_spread;
	struct emstune_frt_boost		frt_boost;
	struct emstune_cgroup_boost		cgroup_boost;
	struct emstune_energy_table		et;

	int					cpu_dsu_table_index;
};

extern int emstune_sched_boost(void);
extern int emstune_support_uclamp(void);
extern int emstune_should_spread(void);
extern bool emstune_need_cgroup_boost(struct task_struct *p);

enum {
	PROFILE_CPU_IDLE,	/* cpu is not busy */
	PROFILE_CPU_BUSY,	/* cpu busy */
};

enum {
	CPU_UTIL,	/* cpu util */
	CPU_HTSK,	/* cpu heaviest task */
	CPU_PROFILE_MAX,
};

/* Acrive ratio based profile */
struct cpu_profile {
	u64	value;	/* util, wratio, htsk */
	u64	data;	/* busy,   busy,  pid */
};

/* profile */
struct system_profile_data {
	/* util based profile */
	int			busy_cpu_count;
	int			heavy_task_count;

	unsigned long		cpu_util_sum;
	unsigned long		avg_nr_run_sum;
	unsigned long		heavy_task_util_sum;

	/* cpu profile data */
	struct cpu_profile	cp[VENDOR_NR_CPUS][CPU_PROFILE_MAX];
};

#define HEAVY_TASK_UTIL_RATIO	(40)
#define MISFIT_TASK_UTIL_RATIO	(80)
#define check_busy(util, cap)	((util * 100) >= (cap * 80))
#define check_busy_half(util, cap) ((util * 100) >= (cap * 50))

static inline int is_heavy_task_util(unsigned long util)
{
	return util > (SCHED_CAPACITY_SCALE * HEAVY_TASK_UTIL_RATIO / 100);
}

static inline int is_misfit_task_util(unsigned long util)
{
	return util > (SCHED_CAPACITY_SCALE * MISFIT_TASK_UTIL_RATIO / 100);
}

enum fair_causes {
	FAIR_ALLOWED_0,
	FAIR_ALLOWED_1,
	FAIR_FIT_0,
	FAIR_FIT_1,
	FAIR_SYSBUSY,
	FAIR_EXPRESS,
	FAIR_ENERGY,
	FAIR_PERFORMANCE,
	FAIR_SYNC,
	FAIR_FAST_TRACK,
	FAIR_FAILED,
	END_OF_FAIR_CAUSES,
};

extern void update_fair_stat(int cpu, enum fair_causes);
extern void update_rt_stat(int cpu);

/* Sysbusy */
#define TICK_SEC	250
#define SYSBUSY_SOMAC	SYSBUSY_STATE3
int sysbusy_monitor_interval(int state);

/* MH DVFS */
#ifdef CONFIG_MHDVFS
extern void mhdvfs(void);
extern void mhdvfs_init(struct kobject *ems_kobj);
#else
static inline void mhdvfs(void) { };
static inline void mhdvfs_init(struct kobject *ems_kobj) { };
#endif

struct _rq_avd1 {
	struct list_head qjump_list;
	unsigned long somac_dequeued;
	int nr_misfited;
	int rq_migrated;
	int cluster_idx;
	int nr_prio_tex;
	int nr_tex;
	int nr_launch;
};

struct ems_task_struct {
	int			target_cpu;
	int			tex_level;
	struct list_head	qjump_node;
	bool			task_misfited;
	u64			tex_last_update;
	u64			tex_runtime;
	int			tex_chances;
	int			boosted_tex;
	bool			binder_task;
	bool			low_latency;
	bool			prio_tex;
	u64			last_waked;
	u64			gsc_task;
	int			render;
	u64			enqueue_ts;
	int			prev_cgroup;
	bool			launch_task;
	u64			hts_config_control;
	u64			mhdvfs_task_tlb;
#ifdef CONFIG_SCHED_EMS_TASK_GROUP
	int			cgroup;
#endif

	/* before refactoring mlt, put new items above here */
	struct mlt_task		task_mlt;
};

#define EMS_VENDOR_DATA_SIZE_CHECK(ems_struct, k_struct)	\
        BUILD_BUG_ON(sizeof(ems_struct) > (sizeof(u64) *	\
                ARRAY_SIZE(((k_struct *)0)->android_vendor_data1)))

#define RQ_AVD1_0(rq)			(rq->android_vendor_data1[0])

#define rq_avd(rq)			((struct _rq_avd1 *)RQ_AVD1_0(rq))
#define ems_qjump_list(rq)		((struct list_head *)&(rq_avd(rq)->qjump_list))
#define ems_somac_dequeued(rq)		(rq_avd(rq)->somac_dequeued)
#define ems_rq_nr_misfited(rq)		(rq_avd(rq)->nr_misfited)
#define ems_rq_migrated(rq)		(rq_avd(rq)->rq_migrated)
#define ems_rq_cluster_idx(rq)		(rq_avd(rq)->cluster_idx)
#define ems_rq_nr_prio_tex(rq)		(rq_avd(rq)->nr_prio_tex)
#define ems_rq_nr_tex(rq)		(rq_avd(rq)->nr_tex)
#define ems_rq_nr_launch(rq)		(rq_avd(rq)->nr_launch)

#define TASK_AVD1(task)			(task->android_vendor_data1)
#define task_avd(task)			((struct ems_task_struct *)TASK_AVD1(task))

#define ems_target_cpu(task)		(task_avd(task)->target_cpu)
#define ems_tex_level(task)		(task_avd(task)->tex_level)
#define ems_task_misfited(task)		(task_avd(task)->task_misfited)
#define ems_tex_last_update(task)	(task_avd(task)->tex_last_update)
#define ems_tex_runtime(task)		(task_avd(task)->tex_runtime)
#define ems_tex_chances(task)		(task_avd(task)->tex_chances)
#define ems_boosted_tex(task)		(task_avd(task)->boosted_tex)
#define ems_binder_task(task)		(task_avd(task)->binder_task)
#define ems_low_latency(task)		(task_avd(task)->low_latency)
#define ems_prio_tex(task)		(task_avd(task)->prio_tex)
#define ems_last_waked(task)		(task_avd(task)->last_waked)
#define ems_gsc_task(task)		(task_avd(task)->gsc_task)
#define ems_render(task)		(task_avd(task)->render)
#define ems_enqueue_ts(task)		(task_avd(task)->enqueue_ts)
#define ems_prev_cgroup(task)		(task_avd(task)->prev_cgroup)
#define ems_launch_task(task)		(task_avd(task)->launch_task)
#ifdef CONFIG_SCHED_EMS_TASK_GROUP
#define ems_cgroup(task)		(task_avd(task)->cgroup)
#endif

#define ems_qjump_node(task)		((struct list_head *)&(task_avd(task)->qjump_node))
#define task_mlt(task)			((struct mlt_task *)&(task_avd(task)->task_mlt))

#define ems_qjump_list_entry(list)								\
({												\
	struct ems_task_struct *ets = list_entry(list, struct ems_task_struct, qjump_node);	\
	void *__mptr = (void *)(ets);								\
	(struct task_struct *)(__mptr - offsetof(struct task_struct, android_vendor_data1));	\
})

#define ems_qjump_first_entry(list)						\
({										\
	struct ems_task_struct *ets = list_first_entry(list,			\
			struct ems_task_struct, qjump_node);			\
	void *__mptr = (void *)(ets);						\
	(struct task_struct *)(__mptr -						\
			offsetof(struct task_struct, android_vendor_data1));	\
})

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

static inline int cpu_overutilized_with_task(int cpu, unsigned long util)
{
	return (capacity_cpu(cpu) * 1024) < ((ml_cpu_util(cpu) + util) * 1280);
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

static inline bool can_migrate(struct task_struct *p, int dst_cpu)
{
	if (p->exit_state)
		return false;

	if (is_per_cpu_kthread(p))
		return false;

	if (p->migration_disabled)
		return false;

#ifdef CONFIG_SMP
	if (task_on_cpu(NULL, p))
		return false;
#endif

	if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
		return false;

	return cpu_active(dst_cpu);
}

static inline int get_idle_exit_latency(struct rq *rq)
{
	struct cpuidle_state *idle;
	unsigned int exit_latency;

	rcu_read_lock();
	idle = idle_get_state(rq);
	exit_latency =  idle ? idle->exit_latency : 0;
	rcu_read_unlock();

	return exit_latency;
}

#ifdef CONFIG_UCLAMP_TASK
static inline bool ml_uclamp_boosted(struct task_struct *p)
{
	return uclamp_eff_value(p, UCLAMP_MIN) > 0;
}

static inline bool ml_uclamp_latency_sensitive(struct task_struct *p)
{
	struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
	struct task_group *tg;

	if (!css)
		return false;

	tg = container_of(css, struct task_group, css);

	return tg->latency_sensitive;
}

static inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
	return clamp(ml_task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline bool ml_uclamp_boosted(struct task_struct *p)
{
	return false;
}

static inline bool uclamp_latency_sensitive(struct task_struct *p)
{
	return false;
}

static inline unsigned long ml_uclamp_task_util(struct task_struct *p)
{
	return ml_task_util_est(p);
}
#endif

#define BUSY_CPU_RATIO	150
static inline bool __is_busy_cpu(unsigned long util,
				 unsigned long runnable,
				 unsigned long capacity,
				 unsigned long nr_running)
{
	if (runnable < capacity)
		return false;

	if (!nr_running)
		return false;

	if (util * BUSY_CPU_RATIO >= runnable * 100)
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

extern int get_sched_class(struct task_struct *p);
extern int cpuctl_task_group_idx(struct task_struct *p);
extern const struct cpumask *cpu_clustergroup_mask(int cpu);
extern const struct cpumask *cpu_slowest_mask(void);
extern const struct cpumask *cpu_fastest_mask(void);
extern int send_uevent(char *msg);

extern void detach_task(struct rq *src_rq, struct rq *dst_rq, struct task_struct *p);
extern int detach_one_task(struct rq *src_rq, struct rq *dst_rq, struct task_struct *target);
extern void attach_task(struct rq *dst_rq, struct task_struct *p);
extern void attach_one_task(struct rq *dst_rq, struct task_struct *p);

static inline u64 amu_core_cycles(void)
{
	return read_sysreg_s(SYS_AMEVCNTR0_CORE_EL0);
}

static inline u64 amu_const_cycles(void)
{
	return read_sysreg_s(SYS_AMEVCNTR0_CONST_EL0);
}

static inline u64 amu_inst_ret(void)
{
	return read_sysreg_s(SYS_AMEVCNTR0_INST_RET_EL0);
}

static inline u64 amu_mem_stall(void)
{
	return read_sysreg_s(SYS_AMEVCNTR0_MEM_STALL);
}

/* Load Balance */
#define MAX_CLUSTER_NUM		(4)
static inline void ems_rq_update_nr_misfited(struct rq *rq, bool misfit)
{
	int val = misfit ? 1 : -1;

	ems_rq_nr_misfited(rq) += val;

	BUG_ON(ems_rq_nr_misfited(rq) < 0);
}

#define MARGIN_MIGRATED_SLOWER	1280
#define MARGIN_MIGRATED_FASETR	1280
static inline bool ems_task_fits_capacity(struct task_struct *p, unsigned long capacity)
{
	unsigned int margin;

	if (capacity_orig_of(task_cpu(p)) > capacity)
		margin = MARGIN_MIGRATED_SLOWER;
	else
		margin = MARGIN_MIGRATED_FASETR;

	return capacity * SCHED_CAPACITY_SCALE > ml_task_util_est(p) * margin;
}

static inline bool is_small_task(struct task_struct *p)
{
	unsigned long util = ml_task_util_est(p);
	unsigned long threshold = 25; /* FIXME */

	return util < threshold;
}

static inline bool is_top_app_task(struct task_struct *p)
{
	return cpuctl_task_group_idx(p) == CGROUP_TOPAPP;
}

static inline bool is_rt_task(struct task_struct *p)
{
	return p->prio < MAX_RT_PRIO;
}

extern int profile_sched_init(struct kobject *);
extern int profile_sched_data(void);
extern int profile_get_htask_ratio(int cpu);
extern void profile_enqueue_task(struct rq *rq, struct task_struct *p);
extern void get_system_sched_data(struct system_profile_data *);

extern void ntu_apply(struct task_struct *p);
extern void ntu_init(struct kobject *ems_kobj);

/*
 *EXTERN for IDLE SELECTION
 */
#ifdef CONFIG_SCHED_EMS_IDLE_SELECT
extern void ems_idle_select_init(struct platform_device *pdev);

extern int ecs_init(struct kobject *ems_kobj);
extern int ecs_gov_dynamic_init(struct kobject *ems_kobj);
extern const struct cpumask *ecs_cpus_allowed(struct task_struct *p);
extern const struct cpumask *ecs_available_cpus(void);
extern void ecs_update(void);
extern void ecs_enqueue_update(int prev_cpu, struct task_struct *p);
extern int ecs_cpu_available(int cpu, struct task_struct *p);
extern void ecs_governor_register(struct ecs_governor *gov, bool default_gov);
extern void update_ecs_cpus(void);
extern struct kobject *ecs_get_governor_object(void);

extern int halo_governor_init(struct kobject *ems_kobj);
extern void halo_tick(struct rq *rq);

/* fvr */
extern void fv_init_table(struct cpufreq_policy *policy);
extern int fv_init(struct kobject *ems_kobj);
extern u64 fv_get_residency(int cpu, int state);
extern u64 fv_get_exit_latency(int cpu, int state);

#else /* CONFIG_SCHED_EMS_IDLE_SELECT */
static inline void fv_init_table(struct cpufreq_policy *policy) { };

static inline void ems_idle_select_init(struct platform_device *pdev) { };

static inline const struct cpumask *ecs_cpus_allowed(struct task_struct *p) { return cpu_active_mask; };
static inline const struct cpumask *ecs_available_cpus(void) { return cpu_active_mask; };
static inline void ecs_update(void) { };
static inline void ecs_enqueue_update(int prev_cpu, struct task_struct *p) { };
static inline int ecs_cpu_available(int cpu, struct task_struct *p) { return true; };

static inline int halo_governor_init(struct kobject *ems_kobj) { return 0; };
static inline void halo_tick(struct rq *rq) { };
#endif /* CONFIG_SCHED_EMS_IDLE_SELECT */

struct fclamp_data {
	int freq;
	int target_period;
	int target_ratio;
	int type;
};

/*
 * EXTERN for FREQ SELECTION
 */
#ifdef CONFIG_SCHED_EMS_FREQ_SELECT
extern void ems_freq_select_init(struct platform_device *pdev);

/* cpufreq */
extern void cpufreq_register_hook(int cpu, int (*func_get_next_cap)(struct tp_env *env,
			struct cpumask *cpus, int dst_cpu), int (*func_need_slack_timer)(void));
extern void cpufreq_unregister_hook(int cpu);
extern int cpufreq_init(void);
extern int cpufreq_get_next_cap(struct tp_env *env, struct cpumask *cpus, int dst_cpu);
extern void slack_timer_cpufreq(int cpu, bool idle_start, bool idle_end);
extern unsigned int fclamp_apply(struct cpufreq_policy *policy, unsigned int orig_freq);

/* freqboost */
extern int freqboost_init(void);
extern void freqboost_enqueue_task(struct task_struct *p, int cpu, int flags);
extern void freqboost_dequeue_task(struct task_struct *p, int cpu, int flags);
extern int freqboost_get_task_ratio(struct task_struct *p);
extern void freqboost_set_boost_ratio(int tcpu, int grp, int boost);
extern unsigned long freqboost_cpu_boost(int cpu, unsigned long util);
extern unsigned long heavytask_cpu_boost(int cpu, unsigned long util, int ratio);

/* ego */
extern int ego_pre_init(struct kobject *ems_kobj);
extern void ego_get_max_freq(int cpu, unsigned int *max_freq, unsigned int *max_freq_orig);
extern void ego_update_frequency_limits(struct cpufreq_policy *policy);

/* energy_table */
extern int et_init(struct kobject *ems_kobj);
extern void et_init_table(struct cpufreq_policy *policy);
extern unsigned int et_cur_freq_idx(int cpu);
extern unsigned long et_cur_cap(int cpu);
extern unsigned long et_max_cap(int cpu);
extern unsigned long et_freq_to_cap(int cpu, unsigned long freq);
extern void et_update_freq(int cpu, unsigned long freq);
extern void et_fill_energy_state(struct tp_env *env, struct cpumask *cpus,
		struct energy_state *states, unsigned long capacity, int dsu_cpu);
extern unsigned long et_compute_system_energy(const struct list_head *csd_head,
		struct energy_state *states, int target_cpu, struct energy_backup *backup);
extern int et_get_table_index_by_ipc(struct tp_env *env);
#else /* CONFIG_SCHED_EMS_FREQ_SELECT */
static inline void ems_freq_select_init(struct platform_device *pdev) { };

/* cpufreq */
static inline int cpufreq_get_next_cap(struct tp_env *env, struct cpumask *cpus, int dst_cpu) { return -1; };
static inline void slack_timer_cpufreq(int cpu, bool idle_start, bool idle_end) { };

/* freqboost */
static inline void freqboost_enqueue_task(struct task_struct *p, int cpu, int flags) { };
static inline void freqboost_dequeue_task(struct task_struct *p, int cpu, int flags) { };

/* energy_table */
static inline void et_init_table(struct cpufreq_policy *policy) { };
static inline unsigned int et_cur_freq_idx(int cpu) { return 0; };
static inline unsigned long et_cur_cap(int cpu) { return 1024; };
static inline void et_fill_energy_state(struct tp_env *env, struct cpumask *cpus,
		struct energy_state *states, unsigned long capacity, int dsu_cpu) { };
static inline unsigned long et_compute_system_energy(const struct list_head *csd_head,
		struct energy_state *states, int target_cpu, struct energy_backup *backup) { return 0; };
static inline unsigned long et_max_cap(int cpu) { return capacity_cpu_orig(cpu); };

/* ego */
static void ego_get_max_freq(int cpu, unsigned int *max_freq, unsigned int *max_freq_orig) { };
static void ego_update_frequency_limits(struct cpufreq_policy *policy) { };
#endif /* CONFIG_SCHED_EMS_FREQ_SELECT */

#ifdef CONFIG_SCHED_EMS_PAGO
/* pago */
extern int pago_init(struct kobject *ems_kobj);
extern void pago_update(void);
extern void pago_tick(struct rq *rq);
extern void pago_idle_enter(int cpu);
#else /* CONFIG_SCHED_EMS_PAGO */
/* pago */
static inline int pago_init(struct kobject *ems_kobj) { return -1; };
static inline void pago_update(void) {};
static inline void pago_tick(struct rq *rq) {};
static inline void pago_idle_enter(int cpu) {};
#endif /* CONFIG_SCHED_EMS_PAGO */

/*
 * EXTERN for FREQ SELECTION
 */
#ifdef CONFIG_SCHED_EMS_CORE_SELECT
extern void ems_core_select_init(struct platform_device *pdev);

extern int ems_select_task_rq_fair(struct task_struct *p, int prev_cpu,
				   int sd_flag, int wake_flag);
extern const struct cpumask *cpus_binding_mask(struct task_struct *p);

/* frt */
extern void frt_init(void);
extern int frt_find_lowest_rq(struct task_struct *task, struct cpumask *lowest_mask, int ret);
extern int frt_select_task_rq_rt(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags);

extern void lb_init(void);
extern void lb_enqueue_misfit_task(struct task_struct *p, struct rq *rq);
extern void lb_dequeue_misfit_task(struct task_struct *p, struct rq *rq);
extern void lb_update_misfit_status(struct task_struct *p, struct rq *rq, bool *need_update);
extern void lb_nohz_balancer_kick(struct rq *rq, unsigned int *flag, int *done);
extern void lb_can_migrate_task(struct task_struct *p, int dst_cpu, int *can_migrate);
extern void lb_find_busiest_queue(int dst_cpu, struct sched_group *group,
		struct cpumask *env_cpus, struct rq **busiest, int *done);
extern void lb_newidle_balance(struct rq *this_rq, struct rq_flags *rf, int *pulled_task, int *done);
extern void lb_tick(struct rq *rq);
extern int lb_rebalance_domains(struct rq *rq);
extern int lb_find_new_ilb(struct cpumask *nohz_idle_cpus_mask);

extern void set_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern bool is_boosted_tex_task(struct task_struct *p);
extern bool is_important_task(struct task_struct *p);
extern bool is_render_tex_task(struct task_struct *p);
extern bool is_zygote_tex_task(struct task_struct *p);
extern void tex_enqueue_task(struct task_struct *p, int cpu);
extern void tex_dequeue_task(struct task_struct *p, int cpu);
extern void tex_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				       struct sched_entity **se_ptr, bool *repick,
				       bool simple, struct task_struct *prev);
extern int get_tex_level(struct task_struct *p);
extern void tex_check_preempt_wakeup(struct rq *rq, struct task_struct *p,
		bool *preempt, bool *ignore);
extern void tex_update(struct rq *rq);
extern void tex_do_yield(struct task_struct *p);
extern void tex_task_init(struct task_struct *p);

extern void gsc_init(struct kobject *ems_kobj);
extern void gsc_update_tick(void);
extern bool is_gsc_task(struct task_struct *p);
extern void gsc_init_new_task(struct task_struct *p);
extern void gsc_cpu_cgroup_attach(struct cgroup_taskset *tset);
extern void gsc_flush_task(struct task_struct *p);
extern int gsc_manage_task_group(struct task_struct *p, bool attach, bool flush);
extern bool gsc_enabled(void);

extern int sysbusy_init(struct kobject *ems_kobj);
extern void monitor_sysbusy(void);
extern int sysbusy_activated(void);
extern void somac_tasks(void);
extern int sysbusy_on_somac(void);
extern int sysbusy_boost_task(struct task_struct *p);
extern void sysbusy_shuffle(struct rq *src_rq);

extern int ontime_init(struct kobject *ems_kobj);
extern int ontime_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void ontime_select_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus);
extern void ontime_migration(void);

/* EMSTune */
extern int emstune_init(struct kobject *ems_kobj, struct device_node *dn, struct device *dev);
extern void emstune_ontime_init(void);
extern bool emstune_task_boost(void);

#ifdef CONFIG_SCHED_EMS_TASK_GROUP
extern struct uclamp_se
ems_uclamp_tg_restrict(struct task_struct *p, enum uclamp_id clamp_id);
extern int task_group_init(struct kobject *ems_kobj);
#endif /* CONFIG_SCHED_EMS_TASK_GROUP */

#else	/* CONFIG_SCHED_EMS_CORE_SELECT */
static inline int emstune_init(struct kobject *ems_kobj, struct device_node *dn, struct device *dev) { return 0; };
static inline void emstune_ontime_init(void) { };
static inline bool emstune_task_boost(void) { return false; };

static inline void ontime_migration(void) { };

static inline void monitor_sysbusy(void) { };
static inline void somac_tasks(void) { };

static inline void gsc_update_tick(void) { };
static inline void gsc_init_new_task(struct task_struct *p) { };
static inline void gsc_cpu_cgroup_attach(struct cgroup_taskset *tset) { };
static inline void gsc_flush_task(struct task_struct *p) { };
static inline int gsc_manage_task_group(struct task_struct *p, bool attach, bool flush) { return 0; };

static inline void set_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *se) { };
static inline void tex_enqueue_task(struct task_struct *p, int cpu) { };
static inline void tex_dequeue_task(struct task_struct *p, int cpu) { };
static inline void tex_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				       struct sched_entity **se_ptr, bool *repick,
				       bool simple, struct task_struct *prev) { };
static inline int get_tex_level(struct task_struct *p) { return 0; };
static inline void tex_update(struct rq *rq) { };
static inline void tex_do_yield(struct task_struct *p) { };
static inline void tex_task_init(struct task_struct *p) { };
static inline void tex_check_preempt_wakeup(struct rq *rq, struct task_struct *p,
		bool *preempt, bool *ignore) { };

static inline void ems_core_select_init(struct platform_device *pdev) { };
static inline int ems_select_task_rq_fair(struct task_struct *p, int prev_cpu,
				   int sd_flag, int wake_flag) { return -1; };

/* frt */
static inline void frt_init(void) { };
static inline int frt_find_lowest_rq(struct task_struct *task, struct cpumask *lowest_mask, int ret) { return -1; };
static inline int frt_select_task_rq_rt(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags) { return -1; };

static inline void lb_init(void) { };
static inline void lb_enqueue_misfit_task(struct task_struct *p, struct rq *rq) { };
static inline void lb_dequeue_misfit_task(struct task_struct *p, struct rq *rq) { };
static inline void lb_update_misfit_status(struct task_struct *p, struct rq *rq, bool *need_update) { };
static inline void lb_nohz_balancer_kick(struct rq *rq, unsigned int *flag, int *done) { };
static inline void lb_can_migrate_task(struct task_struct *p, int dst_cpu, int *can_migrate) { };
static inline void lb_find_busiest_queue(int dst_cpu, struct sched_group *group,
		struct cpumask *env_cpus, struct rq **busiest, int *done) { };
static inline void lb_newidle_balance(struct rq *this_rq, struct rq_flags *rf, int *pulled_task, int *done) { };
static inline void lb_tick(struct rq *rq) { };
static inline int lb_rebalance_domains(struct rq *rq) { return false; };
static inline int lb_find_new_ilb(struct cpumask *nohz_idle_cpus_mask) { return 0; };
#endif /* CONFIG_SCHED_EMS_CORE_SELECT */

/* perf events */
extern int exynos_perf_events_init(struct kobject *ems_kobj);

#endif	/* ENDIF _KERNEL_SCHED_EMS_H */
