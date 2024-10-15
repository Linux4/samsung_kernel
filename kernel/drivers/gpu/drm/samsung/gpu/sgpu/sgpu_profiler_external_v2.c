// SPDX-License-Identifier: GPL-2.0
/*
 * @file sgpu_profiler_external_v2.c
 * @copyright 2023 Samsung Electronics
 */

#include "sgpu_profiler_external_v2.h"

/*freq margin*/
int exynos_profiler_get_freq_margin(void)
{
	return profiler_get_freq_margin();
}
EXPORT_SYMBOL(exynos_profiler_get_freq_margin);

int exynos_profiler_set_freq_margin(int id, int freq_margin)
{
	return profiler_set_freq_margin(id, freq_margin);
}
EXPORT_SYMBOL(exynos_profiler_set_freq_margin);

void exynos_profiler_set_profiler_governor(int mode)
{
	profiler_set_profiler_governor(mode);
}
EXPORT_SYMBOL(exynos_profiler_set_profiler_governor);

void exynos_profiler_set_targetframetime(int us)
{
	profiler_set_targetframetime(us);
}
EXPORT_SYMBOL(exynos_profiler_set_targetframetime);

s32 exynos_profiler_get_pb_margin_info(int id, u16 *no_boost)
{
	return profiler_get_pb_margin_info(id, no_boost);
}
EXPORT_SYMBOL(exynos_profiler_get_pb_margin_info);

void exynos_profiler_set_pb_params(int idx, int value)
{
	profiler_set_pb_params(idx, value);
}
EXPORT_SYMBOL(exynos_profiler_set_pb_params);

int exynos_profiler_get_pb_params(int idx)
{
	return profiler_get_pb_params(idx);
}
EXPORT_SYMBOL(exynos_profiler_get_pb_params);

void exynos_profiler_set_vsync(ktime_t ktime_us)
{
	profiler_set_vsync(ktime_us);
}
EXPORT_SYMBOL(exynos_profiler_set_vsync);

void exynos_profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	profiler_get_frame_info(nrframe, nrvsync, delta_ms);
}
EXPORT_SYMBOL(exynos_profiler_get_frame_info);

void exynos_profiler_get_run_times(u64 *times)
{
	profiler_get_run_times(times);
}
EXPORT_SYMBOL(exynos_profiler_get_run_times);

int exynos_profile_disable_gpullc(int v)
{
	return gpu_dvfs_set_disable_llc_way(v);
}
EXPORT_SYMBOL(exynos_profile_disable_gpullc);

void exynos_profiler_get_pid_list(u32 *list, u8 *core_list)
{
	profiler_pid_get_list(list, core_list);
}
EXPORT_SYMBOL(exynos_profiler_get_pid_list);

int exynos_profiler_pb_set_powertable(int id, int cnt, struct freq_table *table)
{
	return profiler_pb_set_powertable(id, cnt, table);
}
EXPORT_SYMBOL(exynos_profiler_pb_set_powertable);

void exynos_profiler_pb_set_cur_freqlv(int id, int idx)
{
	profiler_pb_set_cur_freqlv(id, idx);
}
EXPORT_SYMBOL(exynos_profiler_pb_set_cur_freqlv);

void exynos_profiler_pb_set_cur_freq(int id, int freq)
{
	profiler_pb_set_cur_freq(id, freq);
}
EXPORT_SYMBOL(exynos_profiler_pb_set_cur_freq);

/*weight table idx*/
unsigned int exynos_profiler_get_weight_table_idx_0(void)
{
	return profiler_get_weight_table_idx_0();
}
EXPORT_SYMBOL(exynos_profiler_get_weight_table_idx_0);

int exynos_profiler_set_weight_table_idx_0(unsigned int value)
{
	return profiler_set_weight_table_idx_0(value);
}
EXPORT_SYMBOL(exynos_profiler_set_weight_table_idx_0);

unsigned int exynos_profiler_get_weight_table_idx_1(void)
{
	return profiler_get_weight_table_idx_1();
}
EXPORT_SYMBOL(exynos_profiler_get_weight_table_idx_1);

int exynos_profiler_set_weight_table_idx_1(unsigned int value)
{
	return profiler_set_weight_table_idx_1(value);
}
EXPORT_SYMBOL(exynos_profiler_set_weight_table_idx_1);

/* from exynos_gpu_interface */
int exynos_profiler_register_utilization_notifier(struct notifier_block *nb)
{
	return gpu_dvfs_register_utilization_notifier(nb);
}
EXPORT_SYMBOL(exynos_profiler_register_utilization_notifier);

int exynos_profiler_unregister_utilization_notifier(struct notifier_block *nb)
{
	return gpu_dvfs_unregister_utilization_notifier(nb);
}
EXPORT_SYMBOL(exynos_profiler_unregister_utilization_notifier);

int *exynos_profiler_get_freq_table(void)
{
	return gpu_dvfs_get_freq_table();
}
EXPORT_SYMBOL(exynos_profiler_get_freq_table);

int exynos_profiler_get_max_freq(void)
{
	return gpu_dvfs_get_max_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_max_freq);

int exynos_profiler_get_min_freq(void)
{
	return gpu_dvfs_get_min_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_min_freq);

int exynos_profiler_get_max_locked_freq(void)
{
	return gpu_dvfs_get_max_locked_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_max_locked_freq);

int exynos_profiler_get_min_locked_freq(void)
{
	return gpu_dvfs_get_min_locked_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_min_locked_freq);

int exynos_profiler_get_cur_clock(void)
{
	return gpu_dvfs_get_cur_clock();
}
EXPORT_SYMBOL(exynos_profiler_get_cur_clock);

unsigned long exynos_profiler_get_maxlock_freq(void)
{
	return gpu_dvfs_get_maxlock_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_maxlock_freq);

unsigned long exynos_profiler_get_minlock_freq(void)
{
	return gpu_dvfs_get_minlock_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_minlock_freq);

int exynos_profiler_get_step(void)
{
	return gpu_dvfs_get_step();
}
EXPORT_SYMBOL(exynos_profiler_get_step);

ktime_t *exynos_profiler_get_time_in_state(void)
{
	return gpu_dvfs_get_time_in_state();
}
EXPORT_SYMBOL(exynos_profiler_get_time_in_state);

ktime_t exynos_profiler_get_tis_last_update(void)
{
	return gpu_dvfs_get_tis_last_update();
}
EXPORT_SYMBOL(exynos_profiler_get_tis_last_update);

int exynos_profiler_set_disable_llc_way(int val)
{
	return gpu_dvfs_set_disable_llc_way(val);
}
EXPORT_SYMBOL(exynos_profiler_set_disable_llc_way);

void exynos_profiler_register_wakeup_func(void (*target_func)(void))
{
	return profiler_register_wakeup_func(target_func);
}
EXPORT_SYMBOL(exynos_profiler_register_wakeup_func);

