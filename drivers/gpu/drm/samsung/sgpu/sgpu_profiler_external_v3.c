#include "sgpu_profiler_external.h"

/*freq margin*/
inline int exynos_profiler_get_freq_margin(void)
{
	return profiler_get_freq_margin();
}
EXPORT_SYMBOL(exynos_profiler_get_freq_margin);

inline int exynos_profiler_set_freq_margin(int freq_margin)
{
	return profiler_set_freq_margin(freq_margin);
}
EXPORT_SYMBOL(exynos_profiler_set_freq_margin);

inline void exynos_profiler_set_profiler_governor(int mode)
{
	profiler_set_amigo_governor(mode);
}
EXPORT_SYMBOL(exynos_profiler_set_profiler_governor);

inline void exynos_profiler_set_targetframetime(int us)
{
	profiler_set_targetframetime(us);
}
EXPORT_SYMBOL(exynos_profiler_set_targetframetime);

inline void exynos_profiler_set_targettime_margin(int us)
{
	profiler_set_targettime_margin(us);
}
EXPORT_SYMBOL(exynos_profiler_set_targettime_margin);

inline void exynos_profiler_set_util_margin(int percentage)
{
	profiler_set_util_margin(percentage);
}
EXPORT_SYMBOL(exynos_profiler_set_util_margin);

inline void exynos_profiler_set_decon_time(int us)
{
	profiler_set_decon_time(us);
}
EXPORT_SYMBOL(exynos_profiler_set_decon_time);

inline void exynos_profiler_set_vsync(ktime_t time_us)
{
	profiler_set_vsync(time_us);
}
EXPORT_SYMBOL(exynos_profiler_set_vsync);

inline void exynos_profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	profiler_get_frame_info(nrframe, nrvsync, delta_ms);
}
EXPORT_SYMBOL(exynos_profiler_get_frame_info);

inline void exynos_profiler_get_run_times(u64 *times)
{
	profiler_get_run_times(times);
}
EXPORT_SYMBOL(exynos_profiler_get_run_times);

inline void exynos_profiler_get_pid_list(u16 *list)
{
	profiler_pid_get_list(list);
}
EXPORT_SYMBOL(exynos_profiler_get_pid_list);

inline void exynos_profiler_set_comb_ctrl(int val)
{
	profiler_stc_set_comb_ctrl(val);
}
EXPORT_SYMBOL(exynos_profiler_set_comb_ctrl);

inline void exynos_profiler_stc_set_powertable(int id, int cnt, struct freq_table *table)
{
	profiler_stc_set_powertable(id, cnt, table);
}
EXPORT_SYMBOL(exynos_profiler_stc_set_powertable);

inline void exynos_profiler_stc_set_busy_domain(int id)
{
	profiler_stc_set_busy_domain(id);
}
EXPORT_SYMBOL(exynos_profiler_stc_set_busy_domain);

inline void exynos_profiler_stc_set_cur_freqlv(int id, int idx)
{
	profiler_stc_set_cur_freqlv(id, idx);
}
EXPORT_SYMBOL(exynos_profiler_stc_set_cur_freqlv);

inline void exynos_profiler_stc_set_cur_freq(int id, int freq)
{
	profiler_stc_set_cur_freq(id, freq);
}
EXPORT_SYMBOL(exynos_profiler_stc_set_cur_freq);

inline int exynos_profiler_stc_config_show(int page_size, char *buf)
{
	return profiler_stc_config_show(page_size, buf);
}
EXPORT_SYMBOL(exynos_profiler_stc_config_show);

inline int exynos_profiler_stc_config_store(const char *buf)
{
	profiler_stc_config_store(buf);
	return 0;
}
EXPORT_SYMBOL(exynos_profiler_stc_config_store);

/*weight table idx*/
inline unsigned int exynos_profiler_get_weight_table_idx_0(void)
{
	return profiler_get_weight_table_idx_0();
}
EXPORT_SYMBOL(exynos_profiler_get_weight_table_idx_0);

inline int exynos_profiler_set_weight_table_idx_0(unsigned int value)
{
	return profiler_set_weight_table_idx_0(value);
}
EXPORT_SYMBOL(exynos_profiler_set_weight_table_idx_0);

inline unsigned int exynos_profiler_get_weight_table_idx_1(void)
{
	return profiler_get_weight_table_idx_1();
}
EXPORT_SYMBOL(exynos_profiler_get_weight_table_idx_1);

inline int exynos_profiler_set_weight_table_idx_1(unsigned int value)
{
	return profiler_set_weight_table_idx_1(value);
}
EXPORT_SYMBOL(exynos_profiler_set_weight_table_idx_1);

/* from exynos_gpu_interface */
inline int exynos_profiler_register_utilization_notifier(struct notifier_block *nb)
{
	return gpu_dvfs_register_utilization_notifier(nb);
}
EXPORT_SYMBOL(exynos_profiler_register_utilization_notifier);

inline int exynos_profiler_unregister_utilization_notifier(struct notifier_block *nb)
{
	return gpu_dvfs_unregister_utilization_notifier(nb);
}
EXPORT_SYMBOL(exynos_profiler_unregister_utilization_notifier);

inline int *exynos_profiler_get_freq_table(void) 
{
	return gpu_dvfs_get_freq_table();
}
EXPORT_SYMBOL(exynos_profiler_get_freq_table);

inline int exynos_profiler_get_max_freq(void)
{
	return gpu_dvfs_get_max_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_max_freq);

inline int exynos_profiler_get_min_freq(void)
{
	return gpu_dvfs_get_min_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_min_freq);

inline int exynos_profiler_get_max_locked_freq(void)
{
	return gpu_dvfs_get_max_locked_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_max_locked_freq);

inline int exynos_profiler_get_min_locked_freq(void)
{
	return gpu_dvfs_get_min_locked_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_min_locked_freq);

inline int exynos_profiler_get_cur_clock(void)
{
	return gpu_dvfs_get_cur_clock();
}
EXPORT_SYMBOL(exynos_profiler_get_cur_clock);

inline unsigned long exynos_profiler_get_maxlock_freq(void) {
	return gpu_dvfs_get_maxlock_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_maxlock_freq);

inline unsigned long exynos_profiler_get_minlock_freq(void) {
	return gpu_dvfs_get_minlock_freq();
}
EXPORT_SYMBOL(exynos_profiler_get_minlock_freq);

inline int exynos_profiler_get_step(void) {
	return gpu_dvfs_get_step();
}
EXPORT_SYMBOL(exynos_profiler_get_step);

inline ktime_t *exynos_profiler_get_time_in_state(void) {
	return gpu_dvfs_get_time_in_state();
}
EXPORT_SYMBOL(exynos_profiler_get_time_in_state);

inline ktime_t exynos_profiler_get_tis_last_update(void) {
	return gpu_dvfs_get_tis_last_update();
}
EXPORT_SYMBOL(exynos_profiler_get_tis_last_update);
