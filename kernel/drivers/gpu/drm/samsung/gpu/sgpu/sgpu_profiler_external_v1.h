/* SPDX-License-Identifier: GPL-2.0 */
/*
 * @file sgpu_profiler_external_v1.h
 * @copyright 2022 Samsung Electronics
 */

#ifndef SGPU_PROFILER_EXTERNAL_H
#define SGPU_PROFILER_EXTERNAL_H

#include "sgpu_profiler_v1.h"
#include "exynos_gpu_interface.h"

/* from KMD module */
int *exynos_profiler_get_freq_table(void);
int exynos_profiler_get_max_freq(void);
int exynos_profiler_get_min_freq(void);
int exynos_profiler_get_max_locked_freq(void);
int exynos_profiler_get_min_locked_freq(void);
int exynos_profiler_get_cur_clock(void);
unsigned long exynos_profiler_get_maxlock_freq(void);
unsigned long exynos_profiler_get_minlock_freq(void);

int exynos_profiler_register_utilization_notifier(struct notifier_block *nb);
int exynos_profiler_unregister_utilization_notifier(struct notifier_block *nb);

int exynos_profiler_get_step(void);
ktime_t *exynos_profiler_get_time_in_state(void);
ktime_t exynos_profiler_get_tis_last_update(void);
int exynos_profiler_set_disable_llc_way(int val);
void exynos_profiler_register_wakeup_func(void (*target_func)(void));

/* from profiler module */
int exynos_profiler_get_freq_margin(void);
int exynos_profiler_set_freq_margin(int id, int freq_margin);
void exynos_profiler_set_profiler_governor(int mode);
void exynos_profiler_set_targetframetime(int us);
void exynos_profiler_set_targettime_margin(int us);
void exynos_profiler_set_util_margin(int percentage);
void exynos_profiler_set_decon_time(int us);
void exynos_profiler_set_vsync(ktime_t ktime_us);
void exynos_profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
void exynos_profiler_get_run_times(u64 *times);
void exynos_profiler_get_pid_list(u32 *list, u8 *core_list);
void exynos_profiler_set_comb_ctrl(int val);

int exynos_profiler_pb_set_powertable(int id, int cnt, struct freq_table *table);
void exynos_profiler_pb_set_busy_domain(int id);
void exynos_profiler_pb_set_cur_freqlv(int id, int idx);
void exynos_profiler_pb_set_cur_freq(int id, int freq);

unsigned int exynos_profiler_get_weight_table_idx_0(void);
int exynos_profiler_set_weight_table_idx_0(unsigned int value);
unsigned int exynos_profiler_get_weight_table_idx_1(void);
int exynos_profiler_set_weight_table_idx_1(unsigned int value);
#endif
