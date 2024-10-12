#include "sgpu_profiler.h"

/**/
/* Dummy function - sgpu_profiler_external */
/**/

/* from profiler module */
inline int exynos_profiler_get_freq_margin(void)
{
	return 0;
}
EXPORT_SYMBOL(exynos_profiler_get_freq_margin);

inline int exynos_profiler_set_freq_margin(int freq_margin)
{
	PROFILER_UNUSED(freq_margin);
	return 0;
}
EXPORT_SYMBOL(exynos_profiler_set_freq_margin);

inline void exynos_profiler_set_profiler_governor(int mode)
{
	PROFILER_UNUSED(mode);
}
EXPORT_SYMBOL(exynos_profiler_set_profiler_governor);

inline void exynos_profiler_set_targetframetime(int us)
{
	PROFILER_UNUSED(us);
}
EXPORT_SYMBOL(exynos_profiler_set_targetframetime);

inline void exynos_profiler_set_targettime_margin(int us)
{
	PROFILER_UNUSED(us);
}
EXPORT_SYMBOL(exynos_profiler_set_targettime_margin);

inline void exynos_profiler_set_util_margin(int percentage)
{
	PROFILER_UNUSED(percentage);
}
EXPORT_SYMBOL(exynos_profiler_set_util_margin);

inline void exynos_profiler_set_decon_time(int us)
{
	PROFILER_UNUSED(us);
}
EXPORT_SYMBOL(exynos_profiler_set_decon_time);

inline void exynos_profiler_set_vsync(ktime_t time_us)
{
	PROFILER_UNUSED(time_us);
}
EXPORT_SYMBOL(exynos_profiler_set_vsync);

inline void exynos_profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	PROFILER_UNUSED(nrframe);
	PROFILER_UNUSED(nrvsync);
	PROFILER_UNUSED(delta_ms);
}
EXPORT_SYMBOL(exynos_profiler_get_frame_info);

inline void exynos_profiler_get_run_times(u64 *times)
{
	PROFILER_UNUSED(times);
}
EXPORT_SYMBOL(exynos_profiler_get_run_times);

inline void exynos_profiler_get_pid_list(u16 *list)
{
	PROFILER_UNUSED(list);
}
EXPORT_SYMBOL(exynos_profiler_get_pid_list);

inline void exynos_profiler_set_comb_ctrl(int val)
{
	PROFILER_UNUSED(val);
}
EXPORT_SYMBOL(exynos_profiler_set_comb_ctrl);

inline int exynos_profiler_stc_config_show(int page_size, char *buf)
{
	PROFILER_UNUSED(page_size);
	PROFILER_UNUSED(buf);
	return 0;
}
EXPORT_SYMBOL(exynos_profiler_stc_config_show);

inline int exynos_profiler_stc_config_store(const char *buf)
{
	PROFILER_UNUSED(buf);
	return 0;
}
EXPORT_SYMBOL(exynos_profiler_stc_config_store);

inline void exynos_profiler_stc_set_powertable(int id, int cnt, struct freq_table *table)
{
	PROFILER_UNUSED(id);
	PROFILER_UNUSED(cnt);
	PROFILER_UNUSED(table);
}
EXPORT_SYMBOL(exynos_profiler_stc_set_powertable);

inline void exynos_profiler_stc_set_busy_domain(int id)
{
	PROFILER_UNUSED(id);
}
EXPORT_SYMBOL(exynos_profiler_stc_set_busy_domain);

inline void exynos_profiler_stc_set_cur_freqlv(int id, int idx)
{
	PROFILER_UNUSED(id);
	PROFILER_UNUSED(idx);
}
EXPORT_SYMBOL(exynos_profiler_stc_set_cur_freqlv);

inline void exynos_profiler_stc_set_cur_freq(int id, int freq)
{
	PROFILER_UNUSED(id);
	PROFILER_UNUSED(freq);
}
EXPORT_SYMBOL(exynos_profiler_stc_set_cur_freq);

inline unsigned int exynos_profiler_get_weight_table_idx_0(void)
{
	return 0;
}
EXPORT_SYMBOL(exynos_profiler_get_weight_table_idx_0);

inline int exynos_profiler_set_weight_table_idx_0(unsigned int value)
{
	PROFILER_UNUSED(value);
	return 0;
}
EXPORT_SYMBOL(exynos_profiler_set_weight_table_idx_0);

inline unsigned int exynos_profiler_get_weight_table_idx_1(void)
{
	return 0;
}
EXPORT_SYMBOL(exynos_profiler_get_weight_table_idx_1);

inline int exynos_profiler_set_weight_table_idx_1(unsigned int value)
{
	PROFILER_UNUSED(value);
	return 0;
}
EXPORT_SYMBOL(exynos_profiler_set_weight_table_idx_1);

/* from KMD module */
inline unsigned long *exynos_profiler_get_freq_table(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_freq_table);

inline int exynos_profiler_get_max_freq(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_max_freq);

inline int exynos_profiler_get_min_freq(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_min_freq);

inline int exynos_profiler_get_max_locked_freq(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_max_locked_freq);

inline int exynos_profiler_get_min_locked_freq(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_min_locked_freq);

inline int exynos_profiler_get_cur_clock(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_cur_clock);

inline unsigned long exynos_profiler_get_maxlock_freq(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_maxlock_freq);

inline unsigned long exynos_profiler_get_minlock_freq(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_minlock_freq);

inline int exynos_profiler_register_utilization_notifier(struct notifier_block *nb)
{
	PROFILER_UNUSED(nb);
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_register_utilization_notifier);

inline int exynos_profiler_unregister_utilization_notifier(struct notifier_block *nb)
{
	PROFILER_UNUSED(nb);
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_unregister_utilization_notifier);

inline int exynos_profiler_get_step(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_step);

inline ktime_t *exynos_profiler_get_time_in_state(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_time_in_state);

inline ktime_t exynos_profiler_get_tis_last_update(void)
{
	return 0;
};
EXPORT_SYMBOL(exynos_profiler_get_tis_last_update);

/**/
/* Dummy function - sgpu_profiler (internal) */
/**/

int profiler_get_freq_margin(void)
{
	return 0;
}

int profiler_set_freq_margin(int freq_margin)
{
	PROFILER_UNUSED(freq_margin);
	return 0;
}
void profiler_set_amigo_governor(int mode)
{
	PROFILER_UNUSED(mode);
}
void profiler_set_targetframetime(int us)
{
	PROFILER_UNUSED(us);
}

void profiler_set_targettime_margin(int us)
{
	PROFILER_UNUSED(us);
}

void profiler_set_util_margin(int percentage)
{
	PROFILER_UNUSED(percentage);
}

void profiler_set_decon_time(int us)
{
	PROFILER_UNUSED(us);
}

void profiler_set_vsync(ktime_t time_us)
{
	PROFILER_UNUSED(time_us);
}

void profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	PROFILER_UNUSED(nrframe);
	PROFILER_UNUSED(nrvsync);
	PROFILER_UNUSED(delta_ms);
}

void profiler_get_run_times(u64 *times)
{
	PROFILER_UNUSED(times);
}

/* pid filter */
void profiler_pid_get_list(u16 *list)
{
	PROFILER_UNUSED(list);
}

/* stc */
void profiler_stc_set_comb_ctrl(int val)
{
	PROFILER_UNUSED(val);
}

void profiler_stc_set_powertable(int id, int cnt, struct freq_table *table)
{
	PROFILER_UNUSED(id);
	PROFILER_UNUSED(cnt);
	PROFILER_UNUSED(table);
}

void profiler_stc_set_busy_domain(int id)
{
	PROFILER_UNUSED(id);
}

void profiler_stc_set_cur_freqlv(int id, int idx)
{
	PROFILER_UNUSED(id);
	PROFILER_UNUSED(idx);
}

void profiler_stc_set_cur_freq(int id, int freq)
{
	PROFILER_UNUSED(id);
	PROFILER_UNUSED(freq);
}

int profiler_stc_config_show(int page_size, char *buf)
{
	PROFILER_UNUSED(page_size);
	PROFILER_UNUSED(buf);
	return 0;
}

int profiler_stc_config_store(const char *buf)
{
	PROFILER_UNUSED(buf);
	return 0;
}
/*weight table idx*/
unsigned int profiler_get_weight_table_idx_0(void)
{
	return 0;
}

int profiler_set_weight_table_idx_0(unsigned int value)
{
	PROFILER_UNUSED(value);
	return 0;
}

unsigned int profiler_get_weight_table_idx_1(void)
{
	return 0;
}

int profiler_set_weight_table_idx_1(unsigned int value)
{
	PROFILER_UNUSED(value);
	return 0;
}
/** Internal(to KMD) **/
/* KMD */
int profiler_interframe_sw_update(ktime_t start, ktime_t end, ktime_t total)
{
	PROFILER_UNUSED(start);
	PROFILER_UNUSED(end);
	PROFILER_UNUSED(total);
	return 0;
}

int profiler_interframe_hw_update(ktime_t start, ktime_t end, bool end_of_frame)
{
	PROFILER_UNUSED(start);
	PROFILER_UNUSED(end);
	PROFILER_UNUSED(end_of_frame);
	return 0;
}

struct profiler_interframe_data *profiler_get_next_frameinfo(void)
{
	return NULL;
}

int profiler_get_target_frametime(void)
{
	return 0;
}

/* initialization */
int sgpu_profiler_init(struct devfreq *df)
{
	return 0;
}

int sgpu_profiler_deinit(struct devfreq *df)
{
	return 0;
}
