#ifndef SGPU_PROFILER_EXTERNAL_H
#define SGPU_PROFILER_EXTERNAL_H

#include "sgpu_profiler.h"
#include "exynos_gpu_interface.h"

/* from KMD module */
inline int *exynos_profiler_get_freq_table(void);
inline int exynos_profiler_get_max_freq(void);
inline int exynos_profiler_get_min_freq(void);
inline int exynos_profiler_get_max_locked_freq(void);
inline int exynos_profiler_get_min_locked_freq(void);
inline int exynos_profiler_get_cur_clock(void);
inline unsigned long exynos_profiler_get_maxlock_freq(void);
inline unsigned long exynos_profiler_get_minlock_freq(void);

inline int exynos_profiler_register_utilization_notifier(struct notifier_block *nb);
inline int exynos_profiler_unregister_utilization_notifier(struct notifier_block *nb);

inline int exynos_profiler_get_step(void);
inline ktime_t *exynos_profiler_get_time_in_state(void);
inline ktime_t exynos_profiler_get_tis_last_update(void);


/* from profiler module */
inline int exynos_profiler_get_freq_margin(void);
inline int exynos_profiler_set_freq_margin(int freq_margin);
inline void exynos_profiler_set_profiler_governor(int mode);
inline void exynos_profiler_set_targetframetime(int us);
inline void exynos_profiler_set_targettime_margin(int us);
inline void exynos_profiler_set_util_margin(int percentage);
inline void exynos_profiler_set_decon_time(int us);
inline void exynos_profiler_set_vsync(ktime_t time_us);
inline void exynos_profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
inline void exynos_profiler_get_run_times(u64 *times);
#if AMIGO_BUILD_VER >= 4
inline u32 profiler_get_target_cpuid(u64 *cputrace_info);
inline void exynos_profiler_set_cputracer_size(int sz);
#endif
inline void exynos_profiler_get_pid_list(u16 *list);
inline void exynos_profiler_set_comb_ctrl(int val);

inline int exynos_profiler_stc_config_show(int page_size, char *buf);
inline int exynos_profiler_stc_config_store(const char *buf);
inline void exynos_profiler_stc_set_powertable(int id, int cnt, struct freq_table *table);
inline void exynos_profiler_stc_set_busy_domain(int id);
inline void exynos_profiler_stc_set_cur_freqlv(int id, int idx);
inline void exynos_profiler_stc_set_cur_freq(int id, int freq);
inline int exynos_profiler_stc_config_show(int page_size, char *buf);
inline int exynos_profiler_stc_config_store(const char *buf);

inline unsigned int exynos_profiler_get_weight_table_idx_0(void);
inline int exynos_profiler_set_weight_table_idx_0(unsigned int value);
inline unsigned int exynos_profiler_get_weight_table_idx_1(void);
inline int exynos_profiler_set_weight_table_idx_1(unsigned int value);
#endif