/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 */

#ifndef EXYNOS_GETCPUSTATE_H
#define EXYNOS_GETCPUSTATE_H

#if IS_ENABLED(CONFIG_EXYNOS_GETCPUSTATE)
bool is_exynos_cpu_power_on(int cpu);
const char *get_exynos_cpu_power_state(int cpu);
#else
#define is_exynos_cpu_power_on(a)		(0)
#define get_exynos_cpu_power_state		"Not Supported"
#endif
#endif
