/*
 * Copyright (c) 2018 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_CPUPM_H
#define __EXYNOS_CPUPM_H __FILE__

#include <dt-bindings/soc/samsung/cpupm.h>

enum {
	C2_ENTER,
	C2_EXIT,
	CPD_ENTER,
	CPD_EXIT,
	DSUPD_ENTER,
	DSUPD_EXIT,
	SICD_ENTER,
	SICD_EXIT,
};

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
extern int exynos_cpupm_notifier_register(struct notifier_block *nb);
extern int exynos_get_idle_ip_index(const char *name, int io_cc);
extern void exynos_update_ip_idle_status(int index, int idle);
extern void disable_power_mode(int cpu, int type);
extern void enable_power_mode(int cpu, int type);
extern void update_pm_allowed_mask(const struct cpumask *mask);
extern void update_idle_allowed_mask(const struct cpumask *mask);
#else
static inline int exynos_cpupm_notifier_register(struct notifier_block *nb) { return 0; }
static inline int exynos_get_idle_ip_index(const char *name, int io_cc) { return 0; }
static inline void exynos_update_ip_idle_status(int index, int idle) { }
static inline void disable_power_mode(int cpu, int type) { }
static inline void enable_power_mode(int cpu, int type) { }
static inline void update_pm_allowed_mask(const struct cpumask *mask) { }
static inline void update_idle_allowed_mask(const struct cpumask *mask) { }
#endif

#endif /* __EXYNOS_CPUPM_H */
