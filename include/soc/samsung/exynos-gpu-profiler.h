#ifndef _EXYNOS_GPU_PROFILER_H_
#define _EXYNOS_GPU_PROFILER_H_

#ifndef PROFILER_UNUSED
#define PROFILER_UNUSED(x)  ((void)(x))
#endif /* PROFILER_UNUSED */

#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/exynos-profiler.h>

#if IS_ENABLED(CONFIG_DRM_SGPU) || IS_ENABLED(CONFIG_MALI_MIDGARD)
/* from profiler module */
extern inline int exynos_profiler_get_freq_margin(void);
extern inline int exynos_profiler_set_freq_margin(int freq_margin);
extern inline void exynos_profiler_set_profiler_governor(int mode);
extern inline void exynos_profiler_set_targetframetime(int us);
extern inline void exynos_profiler_set_targettime_margin(int us);
extern inline void exynos_profiler_set_util_margin(int percentage);
extern inline void exynos_profiler_set_decon_time(int us);
extern inline void exynos_profiler_set_vsync(ktime_t ktime_us);
extern inline void exynos_profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
extern inline void exynos_profiler_get_run_times(u64 *times);
extern inline void exynos_profiler_get_pid_list(unsigned short *list);
extern inline void exynos_profiler_set_comb_ctrl(int enable);

extern inline int exynos_profiler_stc_config_show(int page_size, char *buf);
extern inline int exynos_profiler_stc_config_store(const char *buf);
extern inline void exynos_profiler_stc_set_powertable(int id, int cnt, struct freq_table *table);
extern inline void exynos_profiler_stc_set_busy_domain(int id);
extern inline void exynos_profiler_stc_set_cur_freqlv(int id, int idx);
extern inline void exynos_profiler_stc_set_cur_freq(int id, int freq);
extern inline int exynos_profiler_stc_config_show(int page_size, char *buf);
extern inline int exynos_profiler_stc_config_store(const char *buf);

extern inline unsigned int exynos_profiler_get_weight_table_idx_0(void);
extern inline int exynos_profiler_set_weight_table_idx_0(unsigned int value);
extern inline unsigned int exynos_profiler_get_weight_table_idx_1(void);
extern inline int exynos_profiler_set_weight_table_idx_1(unsigned int value);

/* from KMD module */
extern inline unsigned long *exynos_profiler_get_freq_table(void);
extern inline int exynos_profiler_get_max_freq(void);
extern inline int exynos_profiler_get_min_freq(void);
extern inline int exynos_profiler_get_max_locked_freq(void);
extern inline int exynos_profiler_get_min_locked_freq(void);
extern inline int exynos_profiler_get_cur_clock(void);
extern inline unsigned long exynos_profiler_get_maxlock_freq(void);
extern inline unsigned long exynos_profiler_get_minlock_freq(void);

extern inline int exynos_profiler_register_utilization_notifier(struct notifier_block *nb);
extern inline int exynos_profiler_unregister_utilization_notifier(struct notifier_block *nb);

extern inline int exynos_profiler_get_step(void);
extern inline ktime_t *exynos_profiler_get_time_in_state(void);
extern inline ktime_t exynos_profiler_get_tis_last_update(void);

extern inline u32 exynos_profiler_get_target_cpuid(u64 *info);
extern inline void exynos_profiler_set_cputracer_size(int sz);
#else
/* from profiler module */
static inline int exynos_profiler_get_freq_margin(void) { return 0; };
static inline int exynos_profiler_set_freq_margin(int freq_margin) { PROFILER_UNUSED(freq_margin); return 0; };
static inline void exynos_profiler_set_profiler_governor(int mode) { PROFILER_UNUSED(mode); };
static void exynos_profiler_set_targetframetime(int us) { PROFILER_UNUSED(us); };
static void exynos_profiler_set_targettime_margin(int us) { PROFILER_UNUSED(us); };
static void exynos_profiler_set_util_margin(int percentage) { PROFILER_UNUSED(percentage); };
static void exynos_profiler_set_decon_time(int us) { PROFILER_UNUSED(us); };
static void exynos_profiler_set_vsync(ktime_t ktime_us) { PROFILER_UNUSED(ktime_us); };
static void exynos_profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	PROFILER_UNUSED(nrframe);
	PROFILER_UNUSED(nrvsync);
	PROFILER_UNUSED(delta_ms);
};
static void exynos_profiler_get_run_times(u64 *times) { PROFILER_UNUSED(times); };
static void exynos_profiler_get_pid_list(unsigned short *list) { PROFILER_UNUSED(list); };
static void exynos_profiler_set_comb_ctrl(int enable) { PROFILER_UNUSED(enable); };
static int exynos_profiler_stc_config_show(int page_size, char *buf) { PROFILER_UNUSED(page_size); PROFILER_UNUSED(buf); return 0; };
static int exynos_profiler_stc_config_store(const char *buf) { PROFILER_UNUSED(buf); return 0; };

static void exynos_profiler_stc_set_powertable(int id, int cnt, struct freq_table *table) { PROFILER_UNUSED(id); PROFILER_UNUSED(cnt); PROFILER_UNUSED(table);};
static void exynos_profiler_stc_set_busy_domain(int id) { PROFILER_UNUSED(id); };
static void exynos_profiler_stc_set_cur_freqlv(int id, int idx) { PROFILER_UNUSED(id); PROFILER_UNUSED(idx); };
static void exynos_profiler_stc_set_cur_freq(int id, int freq) { PROFILER_UNUSED(id); PROFILER_UNUSED(freq); };
static int exynos_profiler_stc_config_show(int page_size, char *buf) { PROFILER_UNUSED(page_size); PROFILER_UNUSED(buf); };
static int exynos_profiler_stc_config_store(const char *buf) { PROFILER_UNUSED(buf);};

static inline unsigned int gpu_dvfs_get_weight_table_idx0(void) { return 0; };
static inline int gpu_dvfs_set_weight_table_idx0(unsigned int value) { PROFILER_UNUSED(value); return 0; };
static inline unsigned int gpu_dvfs_get_weight_table_idx1(void) { return 0; };
static inline int gpu_dvfs_set_weight_table_idx1(unsigned int value) { PROFILER_UNUSED(value); return 0; };

/* from KMD module */
static inline unsigned long *exynos_profiler_get_freq_table(void) { return 0; };
static inline int exynos_profiler_get_max_freq(void) { return 0; };
static inline int exynos_profiler_get_min_freq(void) { return 0; };
static inline int exynos_profiler_get_max_locked_freq(void) { return 0; };
static inline int exynos_profiler_get_min_locked_freq(void) { return 0; };
static inline int exynos_profiler_get_cur_clock(void) { return 0; };
static inline unsigned long exynos_profiler_get_maxlock_freq(void) { return 0; };
static inline unsigned long exynos_profiler_get_minlock_freq(void) { return 0; };

static inline int exynos_profiler_register_utilization_notifier(struct notifier_block *nb) { PROFILER_UNUSED(nb); return 0;};
static inline int exynos_profiler_unregister_utilization_notifier(struct notifier_block *nb) { PROFILER_UNUSED(nb); return 0;};

static inline int exynos_profiler_get_step(void) { return 0; };
static inline ktime_t *exynos_profiler_get_time_in_state(void) { return 0; };
static inline ktime_t exynos_profiler_get_tis_last_update(void) { return 0; };
static inline u32 exynos_profiler_get_target_cpuid(u64 *info) { return 0; };
static inline void exynos_profiler_set_cputracer_size(int sz) { PROFILER_UNUSED(id); };
#endif
#endif /* _EXYNOS_GPU_PROFILER_H_ */
