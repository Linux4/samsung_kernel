/* exynos-profiler.h
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Header file for Exynos Multi IP Governor support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PROFILER_FN_H__
#define __EXYNOS_PROFILER_FN_H__

#include <linux/slab.h>
#include <linux/thermal.h>
#include <soc/samsung/cal-if.h>

#include "exynos-profiler-defs.h"

/* Shared Function - Common */
inline u64 * alloc_state_in_freq(int size);
inline int init_freq_cstate(struct freq_cstate *fc, int size);
inline int init_freq_cstate_snapshot(struct freq_cstate_snapshot *fc_snap, int size);
inline int init_freq_cstate_result(struct freq_cstate_result *fc_result, int size);
inline void sync_snap_with_cur(u64 *state_in_freq, u64 *snap, int size);
inline void make_snap_and_delta(u64 *state_in_freq,
			u64 *snap, u64 *delta, int size);
inline void sync_fcsnap_with_cur(struct freq_cstate *fc,
			struct freq_cstate_snapshot *fc_snap, int size);
inline void make_snapshot_and_time_delta(struct freq_cstate *fc,
			struct freq_cstate_snapshot *fc_snap,
			struct freq_cstate_result *fc_result,
			int size);
inline void compute_freq_cstate_result(struct freq_table *freq_table,
			struct freq_cstate_result *fc_result, int size, int cur_freq_idx, int temp);
inline u32 get_idx_from_freq(struct freq_table *table,
			u32 size, u32 freq, bool flag);
inline void compute_power_freq(struct freq_table *freq_table, u32 size, s32 cur_freq_idx,
			u64 *active_in_freq, u64 *clkoff_in_freq, u64 profile_time,
			s32 temp, u64 *dyn_power_ptr, u64 *st_power_ptr,
			u32 *active_freq_ptr, u32 *active_ratio_ptr);

inline unsigned int get_boundary_with_spare(struct freq_table *freq_table,
			s32 max_freq_idx, s32 freq_delta_ratio, s32 cur_idx);
inline unsigned int get_boundary_wo_spare(struct freq_table *freq_table, s32 size,
			s32 freq_delta_ratio, s32 cur_idx);
inline unsigned int get_boundary_freq(struct freq_table *freq_table, s32 size,
			s32 max_freq_idx, s32 freq_delta_ratio, s32 flag, s32 cur_idx);

inline int get_boundary_freq_delta_ratio(struct freq_table *freq_table,
			s32 min_freq_idx, s32 cur_idx, s32 freq_delta_ratio, u32 boundary_freq);
inline unsigned int get_freq_with_spare(struct freq_table *freq_table,
			s32 max_freq_idx, s32 cur_freq, s32 freq_delta_ratio, s32 flag);

inline unsigned int get_freq_wo_spare(struct freq_table *freq_table,
			s32 max_freq_idx, s32 cur_freq, s32 freq_delta_ratio);

inline int get_scaled_target_idx(struct freq_table *freq_table,
			s32 min_freq_idx, s32 max_freq_idx,
			s32 freq_delta_ratio, s32 flag, s32 cur_idx);

inline void __scale_state_in_freq(struct freq_table *freq_table, s32 size,
			s32 min_freq_idx, s32 max_freq_idx,
			u64 *active_state, u64 *clkoff_state,
			s32 freq_delta_ratio, s32 flag, s32 cur_idx);
inline void scale_state_in_freq(struct freq_table *freq_table, s32 size,
			s32 min_freq_idx, s32 max_freq_idx,
			u64 *active_state, u64 *clkoff_state,
			s32 freq_delta_ratio, s32 flag);
inline void get_power_change(struct freq_table *freq_table, s32 size,
			s32 cur_freq_idx, s32 min_freq_idx, s32 max_freq_idx,
			u64 *active_state, u64 *clkoff_state,
			s32 freq_delta_ratio, u64 profile_time, s32 temp, s32 flag,
			u64 *scaled_dyn_power, u64 *scaled_st_power, u32 *scaled_active_freq);
#if IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
extern int exynos_build_static_power_table(struct device_node *np, int **var_table,
			unsigned int *var_volt_size, unsigned int *var_temp_size, char *tz_name);
#endif
inline u64 pwr_cost(u32 freq, u32 volt, u32 coeff, bool flag);
inline int init_static_cost(struct freq_table *freq_table, int size, int weight,
			struct device_node *np, struct thermal_zone_device *tz);
inline int is_vaild_freq(u32 *freq_table, u32 table_cnt, u32 freq);
inline struct freq_table *init_freq_table (u32 *freq_table, u32 table_cnt,
			u32 cal_id, u32 max_freq, u32 min_freq,
            u32 dyn_ceff, u32 st_ceff,
            bool dyn_flag, bool st_flag);

inline struct freq_table *init_fvtable (u32 *freq_table, u32 table_cnt,
                    u32 cal_id, u32 max_freq, u32 min_freq,
                    u32 dyn_ceff, u32 st_ceff,
                    bool dyn_flag, bool st_flag);

inline struct thermal_zone_device * exynos_profiler_init_temp(const char *name);
inline s32 exynos_profiler_get_temp(struct thermal_zone_device *tz);

/* Shared Function - Main Profiler */
extern s32 exynos_profiler_register_domain(s32 id, struct domain_fn *fn, void *private_fn);

#endif /* __EXYNOS_PROFILER_FN_H__ */
