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

	int sched_policy;

	struct cpumask fit_cpus;
	struct cpumask ontime_fit_cpus;
	struct cpumask candidates;
	struct cpumask idle_candidates;

	unsigned long eff_weight[NR_CPUS];	/* efficiency weight */

	int task_util;
	unsigned int cpu_util_wo[NR_CPUS][2];	/* for ISA util */
	unsigned int cpu_util_with[NR_CPUS];	/* ml_cpu_util_with */
	unsigned int cpu_util[NR_CPUS];	/* ml_cpu_util_with */
	unsigned int cpu_rt_util[NR_CPUS];	/* rt util */
	unsigned int nr_running[NR_CPUS];	 /* nr_running */

	int wake;
};

/*
 * The compute capacity, power consumption at this compute capacity and
 * frequency of state. The cap and power are used to find the energy
 * efficiency cpu, and the frequency is used to create the capacity table.
 */
struct energy_state {
	unsigned long frequency;
	unsigned long cap;
	unsigned long power;

	/* for sse */
	unsigned long cap_s;
	unsigned long power_s;

	unsigned long static_power;
};

/*
 * Each cpu can have its own mips_per_mhz, coefficient and energy table.
 * Generally, cpus in the same frequency domain have the same mips_per_mhz,
 * coefficient and energy table.
 */
struct energy_table {
	unsigned int mips_per_mhz;
	unsigned int coefficient;
	unsigned int mips_per_mhz_s;
	unsigned int coefficient_s;
	unsigned int static_coefficient;

	struct energy_state *states;

	unsigned int nr_states;
	unsigned int nr_states_orig;
	unsigned int nr_states_requests[4];
};
extern DEFINE_PER_CPU(struct energy_table, energy_table);
#define get_energy_table(cpu)	&per_cpu(energy_table, cpu)
extern int default_get_next_cap(int dst_cpu, struct task_struct *p);
extern unsigned int __compute_energy(unsigned long util, unsigned long cap, unsigned long dp, unsigned long sp);

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
extern unsigned long _ml_cpu_util_est(int cpu, int sse);
extern unsigned long ml_cpu_util_ratio(int cpu, int sse);
extern unsigned long __ml_cpu_util_with(int cpu, struct task_struct *p, int sse);
extern unsigned long ml_cpu_util_with(int cpu, struct task_struct *p);
extern unsigned long ml_cpu_util_without(int cpu, struct task_struct *p);
extern unsigned long ml_boosted_cpu_util(int cpu);
extern int ml_task_hungry(struct task_struct *p);
extern void init_part(void);

/* efficiency cpu selection */
extern int find_best_cpu(struct tp_env *env);
extern int find_wide_cpu(struct tp_env *env, int sch);
extern int find_energy_cpu(struct tp_env *env);

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
extern int get_gov_next_cap(int grp_cpu, int dst_cpu, struct tp_env *env);
extern unsigned int get_diff_num_levels(int cpu, unsigned int freq1, unsigned int freq2);

/* core sparing */
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

/* emstune - sched policy */
struct emstune_sched_policy {
	bool overriding;
	int policy[STUNE_GROUP_COUNT];
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
	int patient_mode[NR_CPUS];
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
	unsigned long target_sched_class;
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

struct emstune_migov {
	bool overriding;
	int migov_en;
	struct kobject kobj;
};

struct emstune_set {
	int				idx;
	const char			*desc;
	int				unique_id;

	struct emstune_sched_policy	sched_policy;
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
	struct emstune_migov		migov;

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
extern int emstune_sched_policy(struct task_struct *p);
extern int emstune_ontime(struct task_struct *p);
extern int emstune_util_est(struct task_struct *p);
extern int emstune_init_util(struct task_struct *p);
extern int emstune_boosted(void);
extern int emstune_wake_wide;
extern int emstune_hungry;

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
