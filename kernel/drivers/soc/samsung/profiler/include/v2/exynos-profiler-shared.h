/* exynos-profiler-shared.h
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

#ifndef __EXYNOS_PROFILER_SHARED_H
#define __EXYNOS_PROFILER_SHARED_H

#include <exynos-profiler-if.h>

/* Shared Data between profiler modules */
struct profile_sharing_data {
	s32	monitor;
	s32	enable_gfx;
	s32	disable_job_submit;
	s32	disable_dyn_mo;

	s64	profile_time_ms;
	s64     frame_done_time_us;
	s64     frame_vsync_time_us;
	u64	profile_frame_cnt;
	u64	profile_frame_vsync_cnt;
	u64	profile_fence_cnt;
	s64	profile_fence_time_us;
	s32	user_target_fps;
	s32     frame_cnt_by_swap;
	u64     delta_ms_by_swap;

	/* Domain common data */
	s32	max_freq[NUM_OF_DOMAIN];
	s32	min_freq[NUM_OF_DOMAIN];
	s32	freq[NUM_OF_DOMAIN];
	u64	dyn_power[NUM_OF_DOMAIN];
	u64	st_power[NUM_OF_DOMAIN];
	s32	temp[NUM_OF_DOMAIN];
	s32	active_pct[NUM_OF_DOMAIN];

	/* CPU domain private data */
	s32	cpu_active_pct[NUM_OF_CPU_DOMAIN][MAXNUM_OF_CPU];

	/* GPU RenderingTime */
	u64     rtimes[NUM_OF_TIMEINFO];
	u32     pid_list[NUM_OF_PID];
	u8	rendercore_list[NUM_OF_PID];
	s32	gpu_mo;

	/* PB info */
	u16	pb_margin_no_boost[NUM_OF_DOMAIN];
	s32	pb_margin[NUM_OF_DOMAIN];

	/* MIF domain private data */
	u64	stats0_sum;
	u64	stats0_ratio;
	u64	stats0_avg;
	u64	stats1_sum;
	u64	stats1_ratio;
	u64	stats2_sum;
	u64	stats2_ratio;
	u64	llc_status;
	s32	mif_pm_qos_cur_freq;

	s32	llc_config;
};

struct tunable_sharing_data {
	s32	monitor;
	s32	profile_only;
	s32	window_period;
	s32	window_number;
	s32	version;
	s32	frame_src;
	s32	max_fps;
	s32	dt_up_step;
	s32	dt_down_step;

	/* Domain common data */
	bool	enabled[NUM_OF_DOMAIN];
	s32	freq_table[NUM_OF_DOMAIN][MAXNUM_OF_DVFS];
	s32	freq_table_cnt[NUM_OF_DOMAIN];
	s32	minlock_low_limit[NUM_OF_DOMAIN];

	/* CPU domain private data */
	s32	first_cpu[NUM_OF_CPU_DOMAIN];
	s32	num_of_cpu[NUM_OF_CPU_DOMAIN];
	s32	asv_ids[NUM_OF_CPU_DOMAIN];

	/* GPU domain private data */
	u64	gpu_hw_status;
	u64	reserved_gpu1;
	u64	reserved_gpu2;
};

struct delta_sharing_data {
	s32	id;
	s32	freq_delta_pct;
	u32	freq;
	u64	dyn_power;
	u64	st_power;
};

/*
 * 1st argument is profiler-id & op-code in set_margin_store
 * |     OPCODE    |    Daemon-ID | IP-ID    |
 * 31              15             7          0
 */
#define IP_ID_MASK	0xFF
#define DAEMON_ID_MASK	0xFF
#define DAEMON_ID_SHIFT	8
#define OP_CODE_MASK	0xFFFF
#define OP_CODE_SHIFT	16
enum control_op_code {
	OP_INVALID = 0,
	OP_PM_QOS_MIN = 1,
	OP_TARGETFRAMETIME = 2,
	OP_MARGIN = 3,
	OP_MO = 4,
	OP_LLC = 5,
	OP_CPUTRACER_SIZE = 6,
	OP_PB_PARAMS = 7,
};

#endif /* EXYNOS_PROFILER_SHARED_H */
