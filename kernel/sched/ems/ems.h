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

extern struct kobject *ems_kobj;

/* structure for task placement environment */
struct tp_env {
	struct task_struct *p;

	int prefer_perf;
	int prefer_idle;

	struct cpumask fit_cpus;
	struct cpumask candidates;
	struct cpumask idle_candidates;

	unsigned long eff_weight[NR_CPUS];	/* efficiency weight */

	int wake;
};


/* ISA flags */
#define USS	0
#define SSE	1


/* energy model */
extern unsigned long capacity_cpu_orig(int cpu, int sse);
extern unsigned long capacity_cpu(int cpu, int sse);
extern unsigned long capacity_ratio(int cpu, int sse);

/* multi load */
extern unsigned long ml_task_util(struct task_struct *p);
extern unsigned long ml_task_runnable(struct task_struct *p);
extern unsigned long ml_task_util_est(struct task_struct *p);
extern unsigned long ml_boosted_task_util(struct task_struct *p);
extern unsigned long ml_cpu_util(int cpu);
extern unsigned long _ml_cpu_util(int cpu, int sse);
extern unsigned long ml_cpu_util_ratio(int cpu, int sse);
extern unsigned long __ml_cpu_util_with(int cpu, struct task_struct *p, int sse);
extern unsigned long ml_cpu_util_with(int cpu, struct task_struct *p);
extern unsigned long ml_cpu_util_without(int cpu, struct task_struct *p);
extern void init_part(void);

/* efficiency cpu selection */
extern int find_best_cpu(struct tp_env *env);

/* ontime migration */
struct ontime_dom {
	struct list_head	node;

	unsigned long		upper_boundary;
	unsigned long		lower_boundary;

	struct cpumask		cpus;
};

extern int ontime_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void ontime_select_fit_cpus(struct task_struct *p, struct cpumask *fit_cpus);
extern unsigned long get_upper_boundary(int cpu, struct task_struct *p);

/* energy_step_wise_governor */
extern int find_allowed_capacity(int cpu, unsigned int new, int power);
extern int find_step_power(int cpu, int step);
extern int get_gov_next_cap(int dst, struct task_struct *p);

/* core sparing */
struct ecs_mode {
	struct list_head	list;

	unsigned int		enabled;
	unsigned int		mode;

	struct cpumask		cpus;

	/* kobject for sysfs group */
	struct kobject		kobj;
};

struct ecs_domain {
	struct list_head	list;

	unsigned int		role;
	unsigned int		domain_util_avg_thr;
	unsigned int		domain_nr_running_thr;

	struct cpumask		cpus;

	/* kobject for sysfs group */
	struct kobject		kobj;
};

extern struct cpumask *ecs_cpus_allowed(void);

/* EMSTune */
enum stune_group {
	STUNE_ROOT,
	STUNE_FOREGROUND,
	STUNE_BACKGROUND,
	STUNE_TOPAPP,
	STUNE_RT,
	STUNE_GROUP_COUNT,
};

/* emstune - prefer idle */
struct emstune_prefer_idle {
	bool overriding;
	int enabled[STUNE_GROUP_COUNT];
	struct kobject kobj;
};

/* emstune - weight */
struct emstune_weight {
	bool overriding;
	int ratio[NR_CPUS][STUNE_GROUP_COUNT];
	struct kobject kobj;
};

/* emstune - idle weight */
struct emstune_idle_weight {
	bool overriding;
	int ratio[NR_CPUS][STUNE_GROUP_COUNT];
	struct kobject kobj;
};

/* emstune - freq boost */
struct emstune_freq_boost {
	bool overriding;
	int ratio[NR_CPUS][STUNE_GROUP_COUNT];
	struct kobject kobj;
};

/* emstune - energy step governor */
struct emstune_esg {
	bool overriding;
	int step[NR_CPUS];
	struct kobject kobj;
};

/* emstune - ontime migration */
struct emstune_ontime {
	bool overriding;
	int enabled[STUNE_GROUP_COUNT];
	struct list_head *p_dom_list_u;
	struct list_head *p_dom_list_s;
	struct list_head dom_list_u;
	struct list_head dom_list_s;
	struct kobject kobj;
};

/* emstune - utilization estimation */
struct emstune_util_est {
	bool overriding;
	int enabled[STUNE_GROUP_COUNT];
	struct kobject kobj;
};

/* emstune - priority pinning */
struct emstune_prio_pinning {
	bool overriding;
	struct cpumask mask;
	int enabled[STUNE_GROUP_COUNT];
	int prio;
	struct kobject kobj;
};

/* emstune - cpus allowed */
struct emstune_cpus_allowed {
	bool overriding;
	int target_sched_class;
	struct cpumask mask[STUNE_GROUP_COUNT];
	struct kobject kobj;
};

/* emstune - initial utilization */
struct emstune_init_util {
	bool overriding;
	int ratio[STUNE_GROUP_COUNT];
	struct kobject kobj;
};

/* emstune - fluid rt */
struct emstune_frt {
	bool overriding;
	int active_ratio[NR_CPUS];
	int coverage_ratio[NR_CPUS];
	struct kobject kobj;
};

/* emstune - ontime migration */
struct emstune_ecs {
	bool overriding;
	int enabled;
	struct list_head *p_mode_list;
	struct list_head *p_domain_list;
	struct list_head mode_list;
	struct list_head domain_list;
	struct kobject kobj;
};

struct emstune_set {
	int				idx;
	const char			*desc;
	int				unique_id;

	struct emstune_prefer_idle	prefer_idle;
	struct emstune_weight		weight;
	struct emstune_idle_weight	idle_weight;
	struct emstune_freq_boost	freq_boost;
	struct emstune_esg		esg;
	struct emstune_ontime		ontime;
	struct emstune_util_est		util_est;
	struct emstune_prio_pinning	prio_pinning;
	struct emstune_cpus_allowed	cpus_allowed;
	struct emstune_init_util	init_util;
	struct emstune_frt		frt;
	struct emstune_ecs		ecs;

	struct kobject	  		kobj;
};

#define MAX_MODE_LEVEL	100

struct emstune_mode {
	int				idx;
	const char			*desc;

	struct emstune_set		sets[MAX_MODE_LEVEL];
	int				boost_level;
};

extern bool emstune_can_migrate_task(struct task_struct *p, int dst_cpu);
extern int emstune_eff_weight(struct task_struct *p, int cpu, int idle);
extern const struct cpumask *emstune_cpus_allowed(struct task_struct *p);
extern int emstune_prefer_idle(struct task_struct *p);
extern int emstune_ontime(struct task_struct *p);
extern int emstune_util_est(struct task_struct *p);
extern int emstune_init_util(struct task_struct *p);

static inline int cpu_overutilized(unsigned long capacity, unsigned long util)
{
	return (capacity * 1024) < (util * 1280);
}

static inline struct task_struct *task_of(struct sched_entity *se)
{
	return container_of(se, struct task_struct, se);
}

#define entity_is_task(se)	(!se->my_q)

static inline int get_sse(struct sched_entity *se)
{
	if (!se || !entity_is_task(se))
		return 0;

	return task_of(se)->sse;
}

/* declare extern function from cfs */
extern u64 decay_load(u64 val, u64 n);
extern u32 __accumulate_pelt_segments(u64 periods, u32 d1, u32 d3);
