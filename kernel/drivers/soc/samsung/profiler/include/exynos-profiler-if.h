#ifndef __EXYNOS_PROFILER_IF_H__
#define __EXYNOS_PROFILER_IF_H__

/* Profilers */
#include "exynos-profiler-fn.h"

/* Kernel */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/percpu.h>
#include <linux/cpufreq.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/cpuhotplug.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/thermal.h>

#include <linux/slab.h>
#include <linux/pm_opp.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/pm_opp.h>

#include <asm/perf_event.h>

/* SoC Specific */
#include <soc/samsung/exynos-dm.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/exynos-sci.h>
#include <soc/samsung/exynos-devfreq.h>

#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/bts.h>

#include <soc/samsung/exynos-wow.h>

#if IS_ENABLED(CONFIG_EXYNOS_GPU_PROFILER) \
	&& IS_ENABLED(CONFIG_DRM_SGPU_DVFS) \
	&& IS_ENABLED(CONFIG_DRM_SGPU)
/* common */
/* from KMD module */
extern int *exynos_profiler_get_freq_table(void);
extern int exynos_profiler_get_max_freq(void);
extern int exynos_profiler_get_min_freq(void);
extern int exynos_profiler_get_max_locked_freq(void);
extern int exynos_profiler_get_min_locked_freq(void);
extern int exynos_profiler_get_cur_clock(void);
extern unsigned long exynos_profiler_get_maxlock_freq(void);
extern unsigned long exynos_profiler_get_minlock_freq(void);

extern int exynos_profiler_register_utilization_notifier(struct notifier_block *nb);
extern int exynos_profiler_unregister_utilization_notifier(struct notifier_block *nb);

extern int exynos_profiler_get_step(void);
extern ktime_t *exynos_profiler_get_time_in_state(void);
extern ktime_t exynos_profiler_get_tis_last_update(void);

/* from profiler module */
extern int exynos_profiler_get_freq_margin(void);

extern void exynos_profiler_set_profiler_governor(int mode);
extern void exynos_profiler_set_targetframetime(int us);
extern void exynos_profiler_set_vsync(ktime_t ktime_us);
extern void exynos_profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
extern void exynos_profiler_get_run_times(u64 *times);

extern unsigned int exynos_profiler_get_weight_table_idx_0(void);
extern int exynos_profiler_set_weight_table_idx_0(unsigned int value);
extern unsigned int exynos_profiler_get_weight_table_idx_1(void);
extern int exynos_profiler_set_weight_table_idx_1(unsigned int value);

/* version specific */
#if (PROFILER_VERSION >= 1)
extern int exynos_profile_set_gpu_llc_way(int v);
extern int exynos_profiler_set_freq_margin(int id, int freq_margin);
extern void exynos_profiler_get_pid_list(u32 *list, u8 *core_list);
extern int exynos_profiler_pb_set_powertable(int id, int cnt, struct freq_table *table);
extern void exynos_profiler_pb_set_busy_domain(int id);
extern void exynos_profiler_pb_set_cur_freqlv(int id, int idx);
extern void exynos_profiler_pb_set_cur_freq(int id, int freq);
#if IS_ENABLED(CONFIG_SOC_S5E9935) || IS_ENABLED(CONFIG_SOC_S5E9945)
extern int exynos_profiler_set_disable_llc_way(int val);
#else
inline static int exynos_profiler_set_disable_llc_way(int val) { return 0; }
#endif
extern void exynos_profiler_register_wakeup_func(void (*target_func)(void));
#else
extern int exynos_profiler_set_freq_margin(int freq_margin);
extern void exynos_profiler_set_targettime_margin(int us);
extern void exynos_profiler_set_util_margin(int percentage);
extern void exynos_profiler_set_decon_time(int us);

extern u32 exynos_profiler_get_target_cpuid(u64 *cputrace_info);
extern void exynos_profiler_set_cputracer_size(int sz);
extern void exynos_profiler_get_pid_list(u16 *list);
extern void exynos_profiler_set_comb_ctrl(int val);

extern int exynos_profiler_stc_config_show(int page_size, char *buf);
extern int exynos_profiler_stc_config_store(const char *buf);
extern int exynos_profiler_stc_set_powertable(int id, int cnt, struct freq_table *table);
extern void exynos_profiler_stc_set_busy_domain(int id);
extern void exynos_profiler_stc_set_cur_freqlv(int id, int idx);
extern void exynos_profiler_stc_set_cur_freq(int id, int freq);
#endif

#else
/* common */
/* from KMD module */
int *exynos_profiler_get_freq_table(void) { return NULL; };
int exynos_profiler_get_max_freq(void) { return 0; };
int exynos_profiler_get_min_freq(void) { return 0; };
int exynos_profiler_get_max_locked_freq(void) { return 0; };
int exynos_profiler_get_min_locked_freq(void) { return 0; };
int exynos_profiler_get_cur_clock(void) { return 0; };
unsigned long exynos_profiler_get_maxlock_freq(void) { return 0; };
unsigned long exynos_profiler_get_minlock_freq(void) { return 0; };
int exynos_profiler_register_utilization_notifier(struct notifier_block *nb) { PROFILER_UNUSED(nb); return 0; };
int exynos_profiler_unregister_utilization_notifier(struct notifier_block *nb) { PROFILER_UNUSED(nb); return 0; };
int exynos_profiler_get_step(void) { return 0; };
ktime_t *exynos_profiler_get_time_in_state(void) { return 0; };
ktime_t exynos_profiler_get_tis_last_update(void) { return 0; };
/* from profiler module */
int exynos_profiler_get_freq_margin(void) { return 0; };
void exynos_profiler_set_profiler_governor(int mode) { PROFILER_UNUSED(mode); };
void exynos_profiler_set_targetframetime(int us) { PROFILER_UNUSED(us); };
void exynos_profiler_set_vsync(ktime_t ktime_us) { PROFILER_UNUSED(ktime_us); };
void exynos_profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	PROFILER_UNUSED(nrframe);
	PROFILER_UNUSED(nrvsync);
	PROFILER_UNUSED(delta_ms);
};
void exynos_profiler_get_run_times(u64 *times) { PROFILER_UNUSED(times); };
unsigned int exynos_profiler_get_weight_table_idx_0(void) { return 0; };
int exynos_profiler_set_weight_table_idx_0(unsigned int value) { PROFILER_UNUSED(value); return 0; };
unsigned int exynos_profiler_get_weight_table_idx_1(void) { return 0; };
int exynos_profiler_set_weight_table_idx_1(unsigned int value) { PROFILER_UNUSED(value); return 0; };

/* version specific */
#if (PROFILER_VERSION >= 1)
int exynos_profile_set_gpu_llc_way(int v) { PROFILER_UNUSED(v); return 0; };
int exynos_profiler_set_freq_margin(int id, int freq_margin) { PROFILER_UNUSED(id); PROFILER_UNUSED(freq_margin); return 0; };
void exynos_profiler_get_pid_list(u32 *list, u8 *core_list) { PROFILER_UNUSED(list); PROFILER_UNUSED(core_list); };
int exynos_profiler_pb_set_powertable(int id, int cnt, struct freq_table *table) { PROFILER_UNUSED(id); PROFILER_UNUSED(cnt); PROFILER_UNUSED(table); return 0;};
void exynos_profiler_pb_set_busy_domain(int id) { PROFILER_UNUSED(id); };
void exynos_profiler_pb_set_cur_freqlv(int id, int idx) { PROFILER_UNUSED(id); PROFILER_UNUSED(idx); };
void exynos_profiler_pb_set_cur_freq(int id, int freq) { PROFILER_UNUSED(id); PROFILER_UNUSED(freq); };
int exynos_profiler_set_disable_llc_way(int val) { PROFILER_UNUSED(val); return 0; };
inline void exynos_profiler_register_wakeup_func(void (*target_func)(void)) { PROFILER_UNUSED(target_func); };
void exynos_profiler_register_wakeup_job_submit(void *func) { PROFILER_UNUSED(func); };
#else
int exynos_profiler_set_freq_margin(int freq_margin){ PROFILER_UNUSED(freq_margin); return 0; };
void exynos_profiler_set_targettime_margin(int us){ PROFILER_UNUSED(us); };
void exynos_profiler_set_util_margin(int percentage){ PROFILER_UNUSED(percentage); };
void exynos_profiler_set_decon_time(int us){ PROFILER_UNUSED(us); };
u32 exynos_profiler_get_target_cpuid(u64 *cputrace_info){ PROFILER_UNUSED(cputrace_info); return 0; };
void exynos_profiler_set_cputracer_size(int sz){ PROFILER_UNUSED(sz); };
void exynos_profiler_get_pid_list(u16 *list){ PROFILER_UNUSED(list); };
void exynos_profiler_set_comb_ctrl(int val){ PROFILER_UNUSED(val); };
int exynos_profiler_stc_config_show(int page_size, char *buf){ PROFILER_UNUSED(page_size); PROFILER_UNUSED(buf); return 0; };
int exynos_profiler_stc_config_store(const char *buf){ PROFILER_UNUSED(buf); return 0; };
int exynos_profiler_stc_set_powertable(int id, int cnt, struct freq_table *table){ PROFILER_UNUSED(id); PROFILER_UNUSED(cnt); PROFILER_UNUSED(table); return 0;};
void exynos_profiler_stc_set_busy_domain(int id){ PROFILER_UNUSED(id); };
void exynos_profiler_stc_set_cur_freqlv(int id, int idx){ PROFILER_UNUSED(id); PROFILER_UNUSED(idx); };
void exynos_profiler_stc_set_cur_freq(int id, int freq){ PROFILER_UNUSED(id); PROFILER_UNUSED(freq); };
#endif

#endif


#endif /* __EXYNOS_PROFILER_IF_H__ */
