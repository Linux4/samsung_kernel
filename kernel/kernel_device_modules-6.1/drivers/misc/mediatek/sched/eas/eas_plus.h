/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _EAS_PLUS_H
#define _EAS_PLUS_H
#include <linux/ioctl.h>
#include "eas/dsu_pwr.h"
#include "vip.h"

#define MIGR_IDLE_BALANCE               1
#define MIGR_IDLE_PULL_MISFIT_RUNNING   2
#define MIGR_TICK_PULL_MISFIT_RUNNING   3
#define MIGR_IDLE_PULL_VIP_RUNNABLE     4

DECLARE_PER_CPU(unsigned long, max_freq_scale);
DECLARE_PER_CPU(unsigned long, min_freq);

#define LB_FAIL         (0x01)
#define LB_SYNC         (0x02)
#define LB_FAIL_IN_REGULAR (0x03)
#define LB_ZERO_UTIL    (0x04)
#define LB_ZERO_EENV_UTIL    (0x08)
#define LB_PREV         (0x10)
#define LB_LATENCY_SENSITIVE_BEST_IDLE_CPU      (0x20)
#define LB_LATENCY_SENSITIVE_IDLE_MAX_SPARE_CPU (0x40)
#define LB_LATENCY_SENSITIVE_MAX_SPARE_CPU      (0x80)
#define LB_BEST_ENERGY_CPU      (0x100)
#define LB_MAX_SPARE_CPU        (0x200)
#define LB_IN_INTERRUPT		(0x400)
#define LB_IRQ_BEST_IDLE    (0x410)
#define LB_IRQ_SYS_MAX_SPARE   (0x420)
#define LB_IRQ_MAX_SPARE   (0x440)
#define LB_BACKUP_CURR         (0x480)
#define LB_BACKUP_PREV         (0x481)
#define LB_BACKUP_IDLE_CAP      (0x482)
#define LB_BACKUP_AFFINE_WITHOUT_IDLE_CAP (0x483)
#define LB_BACKUP_RECENT_USED_CPU (0x484)
#define LB_BACKUP_AFFINE_IDLE_FIT (0x488)
#define LB_BACKUP_VVIP (0x490)
#define LB_RT_FAIL         (0x1000)
#define LB_RT_FAIL_PD      (0x1001)
#define LB_RT_FAIL_CPU     (0x1002)
#define LB_RT_SYNC      (0x2000)
#define LB_RT_IDLE      (0x4000)
#define LB_RT_LOWEST_PRIO         (0x8000)
#define LB_RT_LOWEST_PRIO_NORMAL  (0x8001)
#define LB_RT_LOWEST_PRIO_RT      (0x8002)
#define LB_RT_SOURCE_CPU       (0x10000)
#define LB_RT_FAIL_SYNC        (0x20000)
#define LB_RT_FAIL_RANDOM      (0x40000)
#define LB_RT_NO_LOWEST_RQ     (0x80000)
#define LB_RT_SAME_SYNC      (0x80001)
#define LB_RT_SAME_FIRST     (0x80002)
#define LB_RT_FAIL_FIRST     (0x80004)

/*
 * energy_env - Utilization landscape for energy estimation.
 * @task_busy_time: Utilization contribution by the task for which we test the
 *                  placement. Given by eenv_task_busy_time().
 * @pd_busy_time:   Utilization of the whole perf domain without the task
 *                  contribution. Given by eenv_pd_busy_time().
 * @cpu_cap:        Maximum CPU capacity for the perf domain.
 * @pd_cap:         Entire perf domain capacity. (pd->nr_cpus * cpu_cap).
 */
#define MAX_NR_CPUS CONFIG_MAX_NR_CPUS
struct energy_env {

	unsigned long task_busy_time;     /* task util*/
	unsigned long min_cap;            /* min cap of task */
	unsigned long max_cap;            /* max cap of task */

	unsigned int gear_idx;
	unsigned long pds_busy_time[MAX_NR_CPUS];
	unsigned long pds_max_util[MAX_NR_CPUS][2]; /* 0: dst_cpu=-1 1: with dst_cpu*/
	unsigned long pds_cpu_cap[MAX_NR_CPUS];
	unsigned long pds_cap[MAX_NR_CPUS];
	unsigned long total_util;

	/* temperature for each cpu*/
	int cpu_temp[MAX_NR_CPUS];

	/* WL-based CPU+DSU ctrl */
	unsigned int wl_support;
	unsigned int wl_type;
	struct dsu_info dsu;
	unsigned int dsu_freq_thermal;
	unsigned int dsu_freq_base;
	unsigned int dsu_freq_new;
	unsigned int dsu_volt_base;
	unsigned int dsu_volt_new;
};

struct rt_energy_aware_output {
	unsigned int rt_cpus;
	unsigned int cfs_cpus;
	unsigned int idle_cpus;
	int cfs_lowest_cpu;
	int cfs_lowest_prio;
	int cfs_lowest_pid;
	int rt_lowest_cpu;
	int rt_lowest_prio;
	int rt_lowest_pid;
	int select_reason;
	int rt_aggre_preempt_enable;
};

#ifdef CONFIG_SMP
/*
 * The margin used when comparing utilization with CPU capacity.
 *
 * (default: ~20%)
 */
#define fits_capacity(cap, max, margin) ((cap) * margin < (max) * 1024)
unsigned long capacity_of(int cpu);
#endif

extern unsigned long cpu_util(int cpu);
extern int task_fits_capacity(struct task_struct *p, long capacity, unsigned int margin);
extern struct perf_domain *find_pd(struct perf_domain *pd, int cpu);

#if IS_ENABLED(CONFIG_MTK_EAS)
extern void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance);
extern void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p,
		int prev_cpu, int sync, int *new_cpu);
extern void mtk_cpu_overutilized(void *data, int cpu, int *overutilized);
extern unsigned long mtk_em_cpu_energy(int gear_id, struct em_perf_domain *pd,
		unsigned long max_util, unsigned long sum_util,
		unsigned long allowed_cpu_cap, struct energy_env *eenv);
extern unsigned int new_idle_balance_interval_ns;
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
extern int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order, bool in_irq);
extern unsigned int thermal_headroom_interval_tick;
#endif

extern void mtk_freq_limit_notifier_register(void);
extern int init_sram_info(void);
extern int init_share_buck(void);
extern void mtk_tick_entry(void *data, struct rq *rq);
extern void mtk_set_wake_flags(void *data, int *wake_flags, unsigned int *mode);
extern void mtk_update_cpu_capacity(void *data, int cpu, unsigned long *capacity);
extern unsigned long cpu_cap_ceiling(int cpu);
extern void mtk_pelt_rt_tp(void *data, struct rq *rq);
extern void mtk_sched_switch(void *data, unsigned int sched_mode, struct task_struct *prev,
		struct task_struct *next, struct rq *rq);

extern void set_wake_sync(unsigned int sync);
extern unsigned int get_wake_sync(void);
extern void set_uclamp_min_ls(unsigned int val);
extern unsigned int get_uclamp_min_ls(void);
extern void set_newly_idle_balance_interval_us(unsigned int interval_us);
extern unsigned int get_newly_idle_balance_interval_us(void);
extern void set_get_thermal_headroom_interval_tick(unsigned int tick);
extern unsigned int get_thermal_headroom_interval_tick(void);

/* add struct for user to control soft affinity */
enum {
	TOPAPP_ID,
	FOREGROUND_ID,
	BACKGROUND_ID,
	TG_NUM
};
struct SA_task {
	int pid;
	unsigned int mask;
};
extern void soft_affinity_init(void);
extern void set_top_app_cpumask(unsigned int cpumask_val);
extern void set_foreground_cpumask(unsigned int cpumask_val);
extern void set_background_cpumask(unsigned int cpumask_val);
extern struct cpumask *get_top_app_cpumask(void);
extern struct cpumask *get_foreground_cpumask(void);
extern struct cpumask *get_background_cpumask(void);
extern void set_task_ls(int pid);
extern void unset_task_ls(int pid);
extern struct task_struct *next_vvip_runable_in_cpu(struct rq *rq);
extern struct task_group *search_tg_by_cpuctl_id(unsigned int cpuctl_id);
extern struct task_group *search_tg_by_name(char *group_name);
extern inline void compute_effective_softmask(struct task_struct *p,
		bool *latency_sensitive, struct cpumask *dst_mask);
extern void mtk_can_migrate_task(void *data, struct task_struct *p,
	int dst_cpu, int *can_migrate);
extern void set_task_ls_prefer_cpus(int pid, unsigned int cpumask_val);
extern void set_task_basic_vip(int pid);
extern void unset_task_basic_vip(int pid);
extern void set_top_app_vip(unsigned int prio);
extern void unset_top_app_vip(void);
extern void set_foreground_vip(unsigned int prio);
extern void unset_foreground_vip(void);
extern void set_background_vip(unsigned int prio);
extern void unset_background_vip(void);
extern void set_ls_task_vip(unsigned int prio);
extern void unset_ls_task_vip(void);

extern void get_most_powerful_pd_and_util_Th(void);

#define EAS_SYNC_SET                            _IOW('g', 1,  unsigned int)
#define EAS_SYNC_GET                            _IOW('g', 2,  unsigned int)
#define EAS_PERTASK_LS_SET                      _IOW('g', 3,  unsigned int)
#define EAS_PERTASK_LS_GET                      _IOR('g', 4,  unsigned int)
#define EAS_ACTIVE_MASK_GET                     _IOR('g', 5,  unsigned int)
#define EAS_NEWLY_IDLE_BALANCE_INTERVAL_SET     _IOW('g', 6,  unsigned int)
#define EAS_NEWLY_IDLE_BALANCE_INTERVAL_GET     _IOR('g', 7,  unsigned int)
#define EAS_GET_THERMAL_HEADROOM_INTERVAL_SET	_IOW('g', 8,  unsigned int)
#define EAS_GET_THERMAL_HEADROOM_INTERVAL_GET	_IOR('g', 9,  unsigned int)
#define EAS_SBB_ALL_SET				_IOW('g', 12,  unsigned int)
#define EAS_SBB_ALL_UNSET			_IOW('g', 13,  unsigned int)
#define EAS_SBB_GROUP_SET			_IOW('g', 14,  unsigned int)
#define EAS_SBB_GROUP_UNSET			_IOW('g', 15,  unsigned int)
#define EAS_SBB_TASK_SET			_IOW('g', 16,  unsigned int)
#define EAS_SBB_TASK_UNSET			_IOW('g', 17,  unsigned int)
#define EAS_SBB_ACTIVE_RATIO		_IOW('g', 18,  unsigned int)
#define EAS_UTIL_EST_CONTROL		_IOW('g', 20,  unsigned int)
#define EAS_TURN_POINT_UTIL_C0		_IOW('g', 21,  unsigned int)
#define EAS_TARGET_MARGIN_C0		_IOW('g', 22,  unsigned int)
#define EAS_TURN_POINT_UTIL_C1		_IOW('g', 23,  unsigned int)
#define EAS_TARGET_MARGIN_C1		_IOW('g', 24,  unsigned int)
#define EAS_TURN_POINT_UTIL_C2		_IOW('g', 25,  unsigned int)
#define EAS_TARGET_MARGIN_C2		_IOW('g', 26,  unsigned int)
#define EAS_SET_CPUMASK_TA		_IOW('g', 27,  unsigned int)
#define EAS_SET_CPUMASK_BACKGROUND	_IOW('g', 28,  unsigned int)
#define EAS_SET_CPUMASK_FOREGROUND	_IOW('g', 29,  unsigned int)
#define EAS_SET_TASK_LS			_IOW('g', 30,  unsigned int)
#define EAS_UNSET_TASK_LS		_IOW('g', 31,  unsigned int)
#define EAS_SET_TASK_LS_PREFER_CPUS		_IOW('g', 32,  struct SA_task)
#define EAS_IGNORE_IDLE_UTIL_CTRL	_IOW('g', 33,  unsigned int)
#define EAS_SET_TASK_VIP			_IOW('g', 34,  unsigned int)
#define EAS_UNSET_TASK_VIP			_IOW('g', 35,  unsigned int)
#define EAS_SET_TA_VIP				_IOW('g', 36,  unsigned int)
#define EAS_UNSET_TA_VIP			_IOW('g', 37,  unsigned int)
#define EAS_SET_FG_VIP				_IOW('g', 38,  unsigned int)
#define EAS_UNSET_FG_VIP			_IOW('g', 39,  unsigned int)
#define EAS_SET_BG_VIP				_IOW('g', 40,  unsigned int)
#define EAS_UNSET_BG_VIP			_IOW('g', 41,  unsigned int)
#define EAS_SET_LS_VIP				_IOW('g', 42,  unsigned int)
#define EAS_UNSET_LS_VIP			_IOW('g', 43,  unsigned int)
#define EAS_GEAR_MIGR_DN_PCT		_IOW('g', 44,  unsigned int)
#define EAS_GEAR_MIGR_UP_PCT		_IOW('g', 45,  unsigned int)
#define EAS_GEAR_MIGR_SET			_IOW('g', 46,  unsigned int)
#define EAS_GEAR_MIGR_UNSET			_IOW('g', 47,  unsigned int)
#define EAS_TASK_GEAR_HINTS_START	_IOW('g', 48,  unsigned int)
#define EAS_TASK_GEAR_HINTS_NUM		_IOW('g', 49,  unsigned int)
#define EAS_TASK_GEAR_HINTS_REVERSE	_IOW('g', 50,  unsigned int)
#define EAS_TASK_GEAR_HINTS_SET		_IOW('g', 51,  unsigned int)
#define EAS_TASK_GEAR_HINTS_UNSET	_IOW('g', 52,  unsigned int)
#define EAS_SET_GAS_CTRL			_IOW('g', 53,  struct gas_ctrl)
#define EAS_SET_GAS_THR				_IOW('g', 54,  struct gas_thr)
#define EAS_RESET_GAS_THR			_IOW('g', 55,  int)
#define EAS_SET_GAS_MARG_THR		_IOW('g', 56,  struct gas_margin_thr)
#define EAS_RESET_GAS_MARG_THR		_IOW('g', 57,  int)
#define EAS_SET_AM_CTRL			    _IOW('g', 60,  int)


#if IS_ENABLED(CONFIG_MTK_NEWIDLE_BALANCE)
extern void mtk_sched_newidle_balance(void *data, struct rq *this_rq,
		struct rq_flags *rf, int *pulled_task, int *done);
#endif

extern unsigned long calc_pwr(int cpu, unsigned long task_util);
extern unsigned long calc_pwr_eff(int wl_type, int cpu, unsigned long cpu_util);
#endif

extern int migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target,
		int reason);
extern void hook_scheduler_tick(void *data, struct rq *rq);
#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
extern bool system_has_many_heavy_task(void);
extern void task_check_for_rotation(struct rq *src_rq);
extern void rotat_after_enqueue_task(void *data, struct rq *rq, struct task_struct *p);
extern void rotat_task_stats(void *data, struct task_struct *p);
extern void rotat_task_newtask(void __always_unused *data, struct task_struct *p,
				unsigned long clone_flags);
#endif
extern void mtk_hook_after_enqueue_task(void *data, struct rq *rq,
				struct task_struct *p, int flags);
extern void mtk_select_task_rq_rt(void *data, struct task_struct *p, int cpu, int sd_flag,
				int flags, int *target_cpu);
extern int mtk_sched_asym_cpucapacity;

extern void mtk_find_lowest_rq(void *data, struct task_struct *p, struct cpumask *lowest_mask,
				int ret, int *lowest_cpu);

extern void throttled_rt_tasks_debug(void *unused, int cpu, u64 clock,
				ktime_t rt_period, u64 rt_runtime, s64 rt_period_timer_expires);

extern bool sched_skip_hiIRQ_enable_get(void);
extern bool sched_rt_aggre_preempt_enable_get(void);
extern void init_skip_hiIRQ(void);
extern void init_rt_aggre_preempt(void);
extern void set_rt_aggre_preempt(int val);
extern int cpu_high_irqload(int cpu);
extern unsigned int mtk_get_idle_exit_latency(int cpu, struct rt_energy_aware_output *rt_ea_output);
extern unsigned long mtk_sched_cpu_util(int cpu);
extern unsigned long mtk_sched_max_util(struct task_struct *p, int cpu,
					unsigned long min_cap, unsigned long max_cap);
extern void track_sched_cpu_util(struct task_struct *p, int cpu,
					unsigned long min_cap, unsigned long max_cap);
extern int get_cpu_irqUtil_threshold(int cpu);
extern int get_cpu_irqRatio_threshold(int cpu);

extern struct cpumask __cpu_pause_mask;
#define cpu_pause_mask ((struct cpumask *)&__cpu_pause_mask)

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
#define cpu_paused(cpu) cpumask_test_cpu((cpu), cpu_pause_mask)

extern void sched_pause_init(void);
#else
#define cpu_paused(cpu) 0
#endif

extern int set_target_margin(int gearid, int margin);
extern int set_turn_point_freq(int gearid, unsigned long turn_freq);
extern int set_util_est_ctrl(bool enable);
struct share_buck_info {
	int gear_idx;
	struct cpumask *cpus;
};

extern struct share_buck_info share_buck;
extern int get_share_buck(void);
extern int sched_cgroup_state(struct task_struct *p, int subsys_id);
#endif
