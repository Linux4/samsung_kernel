#ifndef _EXYNOS_GPU_API_H_
#define _EXYNOS_GPU_API_H_

#ifndef CSTD_UNUSED
#define CSTD_UNUSED(x)  ((void)(x))
#endif /* CSTD_UNUSED */

extern int gpu_dvfs_get_level(int clock);
extern int gpu_dvfs_get_cur_clock(void);
extern int gpu_dvfs_get_min_freq(void);
extern int gpu_dvfs_get_max_freq(void);

#ifdef CONFIG_MALI_DVFS
extern int exynos_stats_get_gpu_max_lock(void);
extern int exynos_stats_get_gpu_min_lock(void);
#else
static inline int exynos_stats_get_gpu_max_lock(void) { return 0; };
static inline int exynos_stats_get_gpu_min_lock(void) { return 0; };
#endif

extern unsigned int exynos_stats_get_gpu_table_size(void);
extern unsigned int *exynos_stats_get_gpu_freq_table(void);
extern ktime_t *exynos_stats_get_gpu_time_in_state(void);

#ifdef CONFIG_MALI_TSG
extern unsigned long exynos_stats_get_job_state_cnt(void);
extern ktime_t *exynos_stats_get_gpu_queued_job_time(void);
extern void exynos_stats_set_gpu_polling_speed(int polling_speed);
extern int exynos_stats_get_gpu_polling_speed(void);
extern void exynos_migov_set_mode(int mode);
extern void exynos_migov_set_gpu_margin(int margin);
#else
static inline unsigned long exynos_stats_get_job_state_cnt(void) { return 0; };
static inline ktime_t *exynos_stats_get_gpu_queued_job_time(void) { return NULL; };
static inline void exynos_stats_set_gpu_polling_speed(int polling_speed) { CSTD_UNUSED(polling_speed); };
static inline int exynos_stats_get_gpu_polling_speed(void) { return 0; };
static inline void exynos_migov_set_mode(int mode) { CSTD_UNUSED(mode); };
static inline void exynos_migov_set_gpu_margin(int margin) { CSTD_UNUSED(margin); };
#endif /* CONFIG_MALI_TSG */
#endif /* _EXYNOS_GPU_API_H_ */