/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
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

#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/plist.h>
#include <linux/sched/idle.h>

struct rq;

enum {
	STATES_FREQ = 0,
	STATES_PMQOS,
	NUM_OF_REQUESTS,
};

#ifdef CONFIG_SCHED_EMS
/*
 * core
 */
extern int
exynos_select_task_rq(struct task_struct *p, int prev_cpu, int sd_flag, int sync, int wakeup, int sch);
extern int ems_can_migrate_task(struct task_struct *p, int dst_cpu);
extern void sysbusy_boost(void);
extern void init_ems(void);


/*
 * init util
 */
extern void post_init_entity_multi_load(struct sched_entity *se, u64 now);


/*
 * energy model
 */
extern void init_sched_energy_table(struct cpumask *cpus, int table_size,
				unsigned long *f_table, unsigned int *v_table,
				int max_f, int min_f);
extern void rebuild_sched_energy_table(struct cpumask *cpus, int clipped_freq,
						int max_freq, int type);
extern void update_capacity(int cpu, bool init);

/*
 * multi load
 */
extern void init_multi_load(struct sched_entity *se);

extern void set_task_rq_multi_load(struct sched_entity *se, struct cfs_rq *prev, struct cfs_rq *next);
extern void update_tg_cfs_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se, struct cfs_rq *gcfs_rq);
extern int update_cfs_rq_multi_load(u64 now, struct cfs_rq *cfs_rq);
extern void attach_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void detach_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern int update_multi_load_se(u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void sync_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void remove_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se);
extern void init_cfs_rq_multi_load(struct cfs_rq *cfs_rq);
extern void migrate_entity_multi_load(struct sched_entity *se);

extern void util_est_enqueue_multi_load(struct cfs_rq *cfs_rq, struct task_struct *p);
extern void util_est_dequeue_multi_load(struct cfs_rq *cfs_rq, struct task_struct *p, bool task_sleep);
extern void util_est_update(struct task_struct *p, int prev_util_est, int next_util_est);
extern void set_part_period_start(struct rq *rq);
extern void update_cpu_active_ratio(struct rq *rq, struct task_struct *p, int type);
extern void part_cpu_active_ratio(unsigned long *util, unsigned long *max, int cpu);


/*
 * ontime migration
 */
extern void ontime_migration(void);


/*
 * load balance
 */
extern struct list_head *lb_cfs_tasks(struct rq *rq, int sse);
extern void lb_add_cfs_task(struct rq *rq, struct sched_entity *se);
extern int lb_check_priority(int src_cpu, int dst_cpu);
extern struct list_head *lb_prefer_cfs_tasks(int src_cpu, int dst_cpu);
extern int lb_need_active_balance(enum cpu_idle_type idle,
				struct sched_domain *sd, int src_cpu, int dst_cpu);
extern bool lb_sibling_overutilized(int dst_cpu, struct sched_domain *sd,
					struct cpumask *lb_cpus);
extern bool lbt_overutilized(int cpu, int level, enum cpu_idle_type idle);
extern void update_lbt_overutil(int cpu, unsigned long capacity);
extern void lb_update_misfit_status(struct task_struct *p, struct rq *rq, unsigned long task_h_load);
extern bool need_iss_margin(int src_cpu, int dst_cpu);
extern void set_lbt_overutil_with_migov(int enabled);

/*
 * Core sparing
 */
extern void ecs_update(void);
extern int ecs_is_sparing_cpu(int cpu);
#else /* CONFIG_SCHED_EMS */

/*
 * core
 */
static inline int
exynos_select_task_rq(struct task_struct *p, int prev_cpu, int sd_flag, int sync, int wakeup, int sch)
{
	return -1;
}
static inline int ems_can_migrate_task(struct task_struct *p, int dst_cpu) { return 1; }
static inline void sysbusy_boost(void) { }
static inline void init_ems(void) { }


/*
 * init util
 */
static inline void post_init_entity_multi_load(struct sched_entity *se, u64 now) { }


/*
 * energy model
 */
static inline void init_sched_energy_table(struct cpumask *cpus, int table_size,
				unsigned long *f_table, unsigned int *v_table,
				int max_f, int min_f) { }
static inline void rebuild_sched_energy_table(struct cpumask *cpus, int clipped_freq,
						int max_freq, int type) { }
static inline void update_capacity(int cpu, bool init) { }

/*
 * multi load
 */
static inline void init_multi_load(struct sched_entity *se) { }

static inline void set_task_rq_multi_load(struct sched_entity *se, struct cfs_rq *prev, struct cfs_rq *next) { }
static inline void update_tg_cfs_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se, struct cfs_rq *gcfs_rq) { }
static inline int update_cfs_rq_multi_load(u64 now, struct cfs_rq *cfs_rq) { return 0; }
static inline void attach_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se) { }
static inline void detach_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se) { }
static inline int update_multi_load_se(u64 now, struct cfs_rq *cfs_rq, struct sched_entity *se) { return 0; }
static inline void sync_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se) { }
static inline void remove_entity_multi_load(struct cfs_rq *cfs_rq, struct sched_entity *se) { }
static inline void init_cfs_rq_multi_load(struct cfs_rq *cfs_rq) { }
static inline void migrate_entity_multi_load(struct sched_entity *se) { }

static inline void util_est_enqueue_multi_load(struct cfs_rq *cfs_rq, struct task_struct *p) { }
static inline void util_est_dequeue_multi_load(struct cfs_rq *cfs_rq, struct task_struct *p, bool task_sleep) { }
static inline void util_est_update(struct task_struct *p, int prev_util_est, int next_util_est) { }
static inline void set_part_period_start(struct rq *rq) { }
static inline void update_cpu_active_ratio(struct rq *rq, struct task_struct *p, int type) { }
static inline void part_cpu_active_ratio(unsigned long *util, unsigned long *max, int cpu) { }


/*
 * ontime migration
 */
static inline int ontime_can_migrate_task(struct task_struct *p, int dst_cpu) { return 1; }
static inline void ontime_migration(void) { }


/*
 * load balance
 */
static inline void lb_add_cfs_task(struct rq *rq, struct sched_entity *se) { }
static inline int lb_check_priority(int src_cpu, int dst_cpu)
{
	return 0;
}
static inline struct list_head *lb_prefer_cfs_tasks(int src_cpu, int dst_cpu)
{
	return NULL;
}
#ifdef CONFIG_SMP
static inline int lb_need_active_balance(enum cpu_idle_type idle,
				struct sched_domain *sd, int src_cpu, int dst_cpu)
{
	return 0;
}
static inline bool lb_sibling_overutilized(int dst_cpu, struct sched_domain *sd,
					struct cpumask *lb_cpus)
{
	return true;
}
#endif
static inline bool lbt_overutilized(int cpu, int level)
{
	return false;
}
static inline void update_lbt_overutil(int cpu, unsigned long capacity) { }
static inline void lb_update_misfit_status(struct task_struct *p, struct rq *rq, unsigned long task_h_load) { }
static inline void set_lbt_overutil_with_migov(int enabled) { }

/*
 * Core sparing
 */
static inline void ecs_update(void) { }
static inline int ecs_is_sparing_cpu(int cpu) { return 0; }
#endif /* CONFIG_SCHED_EMS */

/*
 * EMS Tune
 */
struct emstune_mode_request {
	struct plist_node node;
	bool active;
	struct delayed_work work; /* for emstune_update_request_timeout */
	char *func;
	unsigned int line;
};

#if defined(CONFIG_SCHED_EMS) && defined (CONFIG_SCHED_TUNE)
extern void emstune_cpu_update(int cpu, u64 now);
extern unsigned long emstune_freq_boost(int cpu, unsigned long util);

#define emstune_add_request(req)	do {				\
	__emstune_add_request(req, (char *)__func__, __LINE__);	\
} while(0);
extern void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line);
extern void emstune_remove_request(struct emstune_mode_request *req);
extern void emstune_update_request(struct emstune_mode_request *req, s32 new_value);
extern void emstune_update_request_timeout(struct emstune_mode_request *req, s32 new_value,
					unsigned long timeout_us);
extern void emstune_boost(struct emstune_mode_request *req, int enable);
extern void emstune_boost_timeout(struct emstune_mode_request *req, unsigned long timeout_us);

extern void emstune_mode_change(int next_mode_idx);
extern int emstune_get_cur_mode(void);

extern int emstune_register_mode_update_notifier(struct notifier_block *nb);
extern int emstune_unregister_mode_update_notifier(struct notifier_block *nb);

extern int emstune_util_est_group(int st_idx);
#else
static inline void emstune_cpu_update(int cpu, u64 now) { };
static inline unsigned long emstune_freq_boost(int cpu, unsigned long util) { return util; };

#define emstune_add_request(req)	do { } while(0);
static inline void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line) { }
static inline void emstune_remove_request(struct emstune_mode_request *req) { }
static inline void emstune_update_request(struct emstune_mode_request *req, s32 new_value) { }
static inline void emstune_update_request_timeout(struct emstune_mode_request *req, s32 new_value,
					unsigned long timeout_us) { }
static inline void emstune_boost(struct emstune_mode_request *req, int enable) { }
static inline void emstune_boost_timeout(struct emstune_mode_request *req, unsigned long timeout_us) { }

static inline void emstune_mode_change(int next_mode_idx) { }

static inline int emstune_register_mode_update_notifier(struct notifier_block *nb) { return 0; }
static inline int emstune_unregister_mode_update_notifier(struct notifier_block *nb) { return 0; }

static inline int emstune_util_est_group(int st_idx) { return 0; }
#endif /* CONFIG_SCHED_EMS && CONFIG_SCHED_TUNE */

/* Exynos Fluid Real Time Scheduler */
extern unsigned int frt_disable_cpufreq;

#ifdef CONFIG_SCHED_USE_FLUID_RT
extern int frt_find_lowest_rq(struct task_struct *task);
extern int frt_update_rt_rq_load_avg(struct rq *rq);
extern void frt_migrate_task_rq_rt(struct task_struct *p, int new_cpu);
extern void frt_update_load_avg(struct rt_rq *rt_rq, struct sched_rt_entity *se, int flags);
extern void frt_task_change_group_rt(struct task_struct *p, int type);
extern void frt_attach_task_rt_rq(struct task_struct *p);
extern void frt_detach_task_rt_rq(struct task_struct *p);
extern void frt_init_rt_rq_load(struct rt_rq *rt_rq);
extern void frt_update_available_cpus(void);
extern void frt_clear_victim_flag(struct task_struct *p);
extern bool frt_test_victim_flag(struct task_struct *p);
extern void frt_task_dead_rt(struct task_struct *p);
extern void frt_set_task_rq_rt(struct sched_rt_entity *se, struct rt_rq *prev, struct rt_rq *next);
extern void frt_init_entity_runnable_average(struct sched_rt_entity *rt_se);
extern void frt_store_sched_avg(struct task_struct *p, struct sched_avg *sa);
extern void frt_sync_sched_avg(struct task_struct *p, struct sched_avg *sa);
#else
static inline int frt_update_rt_rq_load_avg(struct rq *rq) { return 0; };
static inline int frt_find_lowest_rq(struct task_struct *task) { return -1; };
static inline void frt_migrate_task_rq_rt(struct task_struct *p, int nuew_cpu) { };
#ifndef CONFIG_UML
static inline void frt_update_load_avg(struct rt_rq *rt_rq, struct sched_rt_entity *se, int flags) { };
#else
static inline void frt_update_load_avg(void *rt_rq, struct sched_rt_entity *se, int flags) { };
#endif
static inline void frt_task_change_group_rt(struct task_struct *p, int type) { };
static inline void frt_attach_task_rt_rq(struct task_struct *p) { };
static inline void frt_detach_task_rt_rq(struct task_struct *p) { };
#ifndef CONFIG_UML
static inline void frt_init_rt_rq_load(struct rt_rq *rt_rq) { };
#else
static inline void frt_init_rt_rq_load(void *rt_rq) { };
#endif
static inline void frt_update_available_cpus(void) { };
static inline void frt_clear_victim_flag(struct task_struct *p) { };
static inline bool frt_test_victim_flag(struct task_struct *p) { return false; };
static inline void frt_task_dead_rt(struct task_struct *p) { };
#ifndef CONFIG_UML
static inline void frt_set_task_rq_rt(struct sched_rt_entity *se, struct rt_rq *prev, struct rt_rq *next) { };
#else
static inline void frt_set_task_rq_rt(struct sched_rt_entity *se, void *prev, void *next) { };
#endif
static inline void frt_init_entity_runnable_average(struct sched_rt_entity *rt_se) { }
static inline void frt_store_sched_avg(struct task_struct *p, struct sched_avg *sa) { }
static inline void frt_sync_sched_avg(struct task_struct *p, struct sched_avg *sa) { }
#endif

#ifdef CONFIG_SCHED_PMU_CONT
void update_cont_avg(struct rq *rq, struct task_struct *prev, struct task_struct *next);
#else
static inline void update_cont_avg(struct rq *rq, struct task_struct *prev, struct task_struct *next) { };
#endif

#ifdef CONFIG_CPU_FREQ_GOV_ENERGYSTEP
extern int esg_get_migov_boost(int cpu);
extern void esg_set_migov_boost(int cpu, int boost);
#else
static inline int esg_get_migov_boost(int cpu) { return 0; };
static inline void esg_set_migov_boost(int cpu, int boost) { };
#endif
