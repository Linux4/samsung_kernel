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

#ifndef _LINUX_EMS_H
#define _LINUX_EMS_H

#include <linux/sched.h>
#include <linux/pm_qos.h>
#include <linux/sched/cputime.h>
#include <linux/sched/clock.h>
#include <linux/sched/isolation.h>
#include <uapi/linux/sched/types.h>
#include <linux/cgroup.h>
#include <linux/cpuidle.h>
#include <soc/samsung/exynos-cpuhp.h>
#include <linux/perf_event.h>

/*
 * Sysbusy
 */
static const char *sysbusy_state_names[] = {
	"state0",
	"state1",
	"state2",
	"state3",
};

enum sysbusy_state {
	SYSBUSY_STATE0 = 0,
	SYSBUSY_STATE1,
	SYSBUSY_STATE2,
	SYSBUSY_STATE3,
	NUM_OF_SYSBUSY_STATE,
};

#define SYSBUSY_CHECK_BOOST	(0)
#define SYSBUSY_STATE_CHANGE	(1)

/*
 * EMStune
 */
struct emstune_mode_request {
	struct dev_pm_qos_request dev_req;
	char *func;
	unsigned int line;
};

#define EMSTUNE_GAME_MODE	(3)
#define EMSTUNE_BOOST_LEVEL	(2)

/*
 * EXTERN for CORE SELECTION
 */
#ifdef CONFIG_SCHED_EMS_CORE_SELECT
extern int emstune_get_cur_mode(void);
extern int emstune_get_cur_level(void);

#define emstune_add_request(req)	do {				\
	__emstune_add_request(req, (char *)__func__, __LINE__);	\
} while(0);
extern void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line);
extern void emstune_update_request(struct emstune_mode_request *req, s32 new_value);
extern void emstune_remove_request(struct emstune_mode_request *req);
extern void emstune_boost(struct emstune_mode_request *req, int enable);

extern int emstune_register_notifier(struct notifier_block *nb);
extern int emstune_unregister_notifier(struct notifier_block *nb);

extern int emstune_cpu_dsu_table_index(void *_set);

extern int sysbusy_register_notifier(struct notifier_block *nb);
extern int sysbusy_unregister_notifier(struct notifier_block *nb);

#else /* CONFIG_SCHED_EMS_CORE_SELECT */
static inline void stt_acquire_gmc_boost(int step, int cnt) { };
static inline void stt_release_gmc_boost(void) { };

#define emstune_add_request(req)	do { } while(0);
static inline int emstune_get_cur_mode(void) { return -1; };
static inline int emstune_get_cur_level(void) { return -1; };

static inline void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line) { };
static inline void emstune_update_request(struct emstune_mode_request *req, s32 new_value) { };
static inline void emstune_remove_request(struct emstune_mode_request *req) { };
static inline void emstune_boost(struct emstune_mode_request *req, int enable) { };

static inline int emstune_register_notifier(struct notifier_block *nb) { return 0; };
static inline int emstune_unregister_notifier(struct notifier_block *nb) { return 0; };

static inline int emstune_cpu_dsu_table_index(void *_set) { return 0; };

static inline int sysbusy_register_notifier(struct notifier_block *nb) { return 0; };
static inline int sysbusy_unregister_notifier(struct notifier_block *nb) { return 0; };
#endif /* CONFIG_SCHED_EMS_CORE_SELECT */

/*
 * EXTERN for FREQ SELECTION
 */
#ifdef CONFIG_SCHED_EMS_FREQ_SELECT
extern void et_init_dsu_table(unsigned long *freq_table, unsigned int *volt_table, int size);
extern void et_register_dsu_constraint(int cpu, void *p, int size);
extern void et_arch_set_freq_scale(const struct cpumask *cpus,
		unsigned long freq,  unsigned long max, unsigned long *scale);
extern int ego_set_adaptive_freq(unsigned int cpu, unsigned int low_freq, unsigned int high_freq);
extern int ego_get_adaptive_freq(unsigned int cpu, unsigned int *low_freq, unsigned int *high_freq);
extern int ego_reset_adaptive_freq(unsigned int cpu);
#else /* CONFIG_SCHED_EMS_FREQ_SELECT */
static inline void et_init_dsu_table(unsigned long *freq_table, unsigned int *volt_table, int size) { };
static inline void et_register_dsu_constraint(int cpu, void *p, int size) { };
static inline void et_arch_set_freq_scale(const struct cpumask *cpus,
		unsigned long freq,  unsigned long max, unsigned long *scale) { };
static inline int ego_set_adaptive_freq(unsigned int cpu, unsigned int low_freq, unsigned int high_freq) { return -ENODEV; };
static inline int ego_get_adaptive_freq(unsigned int cpu, unsigned int *low_freq, unsigned int *high_freq) { return -ENODEV; };
static inline int ego_reset_adaptive_freq(unsigned int cpu) { return -ENODEV; };
#endif /* CONFIG_SCHED_EMS_FREQ_SELECT */

/*
 * EXTERN for IDLE SELECTION
 */
enum ecs_type {
	ECS_MIN = 0,
	ECS_MAX = 1,
};
#ifdef CONFIG_SCHED_EMS_IDLE_SELECT
extern int ecs_request_register(char *name, const struct cpumask *mask, enum ecs_type type);
extern int ecs_request_unregister(char *name, enum ecs_type type);
extern int ecs_request(char *name, const struct cpumask *mask, enum ecs_type type);
extern void halo_register_periodic_irq(int irq_num, const char *name, struct cpumask *cpus, void (*fn)(u64 *cnt, ktime_t *time));
extern void halo_update_periodic_irq(int irq_num, u64 cnt, ktime_t last_update_ns, ktime_t period_ns);
extern void halo_unregister_periodic_irq(int irq_num);
#else /* CONFIG_SCHED_EMS_IDLE_SELECT */
static inline int ecs_request_register(char *name, const struct cpumask *mask, enum ecs_type type) {
	return exynos_cpuhp_add_request(name, mask);
};
static inline int ecs_request_unregister(char *name, enum ecs_type type) {
	return exynos_cpuhp_remove_request(name, enum ecs_type type);
};
static inline int ecs_request(char *name, const struct cpumask *mask, enum ecs_type type) {
	return exynos_cpuhp_update_request(name, mask);
};
static inline void halo_register_periodic_irq(int irq_num, const char *name, struct cpumask *cpus, void (*fn)(u64 *cnt, ktime_t *time)) { };
static inline void halo_update_periodic_irq(int irq_num, u64 cnt, ktime_t last_update_ns, ktime_t period_ns) { };
static inline void halo_unregister_periodic_irq(int irq_num) { };
#endif /* CONFIG_SCHED_EMS_IDLE_SELECT */

/* MH DVFS */
#ifdef CONFIG_MHDVFS
extern void register_mhdvfs_dsufreq_callback(void (*callback)(int ratio, int period_ns));
extern void register_mhdvfs_miffreq_callback(void (*callback)(int ratio));
extern void register_mhdvfs_cpufreq_callback(void (*callback)(int *ratio));
#else
static inline void register_mhdvfs_dsufreq_callback(void (*callback)(int ratio)) { };
static inline void register_mhdvfs_miffreq_callback(void (*callback)(int ratio)) { };
static inline void register_mhdvfs_cpufreq_callback(void (*callback)(int *ratio)) { };
#endif

extern struct perf_event *exynos_perf_create_kernel_counter(struct perf_event_attr *attr, int cpu,
		struct task_struct *task, perf_overflow_handler_t overflow_handler, void *context);
extern int exynos_perf_event_release_kernel(struct perf_event *event);

#endif	/* ENDIF _LINUX_EMS_H */
