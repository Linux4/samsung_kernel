/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *		http://www.samsung.com/
 *
 * Chip Abstraction Layer for System power down support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PMU_CAL_SYS_H__
#define __PMU_CAL_SYS_H__

#include <linux/printk.h>
#ifdef CONFIG_CAL_SYS_PWRDOWN
#if defined(CONFIG_SOC_EXYNOS5433)
#include <mach/pmu_cal_sys_exynos5433.h>
#elif defined(CONFIG_SOC_EXYNOS3475)
#include <mach/pmu_cal_sys_exynos3475.h>
#endif

struct pmu_cal_sys_ops {
	void (*sys_init)(void);
	void (*sys_prepare)(enum sys_powerdown mode);
	void (*sys_post) (enum sys_powerdown mode);
	void (*sys_earlywake)(enum sys_powerdown mode);
};

int exynos_pmu_cal_sys_init(void);
void exynos_pmu_cal_sys_prepare(enum sys_powerdown mode);
void exynos_pmu_cal_sys_post(enum sys_powerdown mode);
void exynos_pmu_cal_sys_earlywake(enum sys_powerdown mode);

void register_pmu_cal_sys_ops(const struct pmu_cal_sys_ops **ops);
extern void (*exynos_sys_powerdown_conf)(enum sys_powerdown mode);
extern void (*exynos_central_sequencer_ctrl)(bool enable);
extern void pmu_cal_sys_init(void);
#else
static inline void register_pmu_cal_sys_ops(const struct pmu_cal_sys_ops **ops) {}
#endif

#endif /* __PMU_CAL_SYS_H__ */
