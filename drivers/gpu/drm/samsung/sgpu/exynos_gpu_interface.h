#include <linux/devfreq.h>
#include <linux/kernel.h>

#ifdef CONFIG_DRM_SGPU_EXYNOS

#include "soc/samsung/cal-if.h"

#define TABLE_MAX                      (200)
#define SYSBUSY_FREQ_THRESHOLD         (500000)
#define SYSBUSY_UTIL_THRESHOLD         (70)

int exynos_dvfs_preset(struct devfreq *df);
int exynos_dvfs_postclear(struct devfreq *df);

void gpu_dvfs_init_utilization_notifier_list(void);
int gpu_dvfs_register_utilization_notifier(struct notifier_block *nb);
int gpu_dvfs_unregister_utilization_notifier(struct notifier_block *nb);
void gpu_dvfs_notify_utilization(void);

int gpu_dvfs_init_table(struct dvfs_rate_volt *tb, int max_state);
int gpu_dvfs_get_step(void);
int *gpu_dvfs_get_freq_table(void);

int gpu_dvfs_get_clock(int level);
int gpu_dvfs_get_voltage(int clock);

int gpu_dvfs_get_max_freq(void);
inline int gpu_dvfs_get_max_locked_freq(void);
int gpu_dvfs_set_max_freq(unsigned long freq);

int gpu_dvfs_get_min_freq(void);
inline int gpu_dvfs_get_min_locked_freq(void);
int gpu_dvfs_set_min_freq(unsigned long freq);

int gpu_dvfs_get_cur_clock(void);

unsigned long gpu_dvfs_get_maxlock_freq(void);
int gpu_dvfs_set_maxlock_freq(unsigned long freq);

unsigned long gpu_dvfs_get_minlock_freq(void);
int gpu_dvfs_set_minlock_freq(unsigned long freq);

ktime_t gpu_dvfs_update_time_in_state(unsigned long freq);
ktime_t *gpu_dvfs_get_time_in_state(void);
ktime_t gpu_dvfs_get_tis_last_update(void);

unsigned int gpu_dvfs_get_polling_interval(void);
int gpu_dvfs_set_polling_interval(unsigned int value);

char *gpu_dvfs_get_governor(void);
int gpu_dvfs_set_governor(char* str_governor);
void gpu_dvfs_set_autosuspend_delay(int delay);

int gpu_dvfs_set_disable_llc_way(int val);

int gpu_dvfs_get_utilization(void);

int gpu_tmu_notifier(struct notifier_block *notifier, unsigned long event, void *v);
int gpu_afm_decrease_maxlock(unsigned int down_step);
int gpu_afm_release_maxlock(void);

int exynos_interface_init(struct devfreq *df);
int exynos_interface_deinit(struct devfreq *df);

void gpu_umd_min_clock_set(unsigned int value, unsigned int delay);

#endif
