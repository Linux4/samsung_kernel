/* SPDX-License-Identifier: GPL-2.0 */
/*
 * @file sgpu_profiler_v0.h
 * @copyright 2022 Samsung Electronics
 */
#ifndef _SGPU_PROFILER_V0_H_
#define _SGPU_PROFILER_V0_H_

#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/cpufreq.h>
#include <linux/pm_runtime.h>

#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/exynos-profiler-external.h>

#include "amdgpu.h"
#include "sgpu_governor.h"
#include "exynos_gpu_interface.h"

#define PROFILER_TABLE_MAX				200
#define RENDERINGTIME_MAX_TIME			999999ULL
#define RENDERINGTIME_MIN_FRAME			4000ULL
#define RTP_DEFAULT_FRAMETIME			16666ULL
#define DEADLINE_DECON_INUS				1000
#define COMB_CTRL_OFF_TMAX				9999999

struct profiler_interframe_data {
	unsigned int nrq;
	u64 sw_vsync;
	u64 sw_start;
	u64 sw_end;
	u64 sw_total;
	u64 hw_vsync;
	u64 hw_start;
	u64 hw_end;
	u64 hw_total;
	u64 sum_pre;
	u64 sum_cpu;
	u64 sum_swap;
	u64 sum_gpu;
	u64 sum_v2f;
	u64 max_pre;
	u64 max_cpu;
	u64 max_swap;
	u64 max_gpu;
	u64 max_v2f;
	u64 vsync_interval;
	int sdp_next_cpuid;
	int sdp_cur_fcpu;
	int sdp_cur_fgpu;
	int sdp_next_fcpu;
	int sdp_next_fgpu;
	u64 cputime, gputime;
};

struct profiler_freq_estpower_data {
	int freq;
	int power;
};

struct profiler_info_data {
	s32 frame_counter;
	u64 vsync_counter;
	u64 last_updated_vsync_time;
	int cpufreq_pm_qos_added;
};

struct profiler_rtp_data {
	unsigned int head;
	unsigned int tail;
	int readout;
	unsigned int nrq;
	unsigned int lastshowidx;
	ktime_t prev_swaptimestamp;
	ktime_t lasthw_starttime;
	ktime_t lasthw_endtime;
	atomic_t lasthw_totaltime;
	atomic_t lasthw_read;
	ktime_t curhw_starttime;
	ktime_t curhw_endtime;
	ktime_t curhw_totaltime;
	ktime_t sum_pre;
	ktime_t sum_cpu;
	ktime_t sum_swap;
	ktime_t sum_gpu;
	ktime_t sum_v2f;
	ktime_t max_pre;
	ktime_t max_cpu;
	ktime_t max_swap;
	ktime_t max_gpu;
	ktime_t max_v2f;
	int last_cputime;
	int last_gputime;
	ktime_t vsync_lastsw;
	ktime_t vsync_curhw;
	ktime_t vsync_lasthw;
	ktime_t vsync_prev;
	ktime_t vsync_interval;
	atomic_t vsync_swapcall_counter;
	int vsync_frame_counter;
	int vsync_counter;
	unsigned int target_frametime;
	unsigned int targettime_margin;
	unsigned int workload_margin;
	unsigned int decon_time;
	int shortterm_comb_ctrl;
	int shortterm_comb_ctrl_manual;
	/* Performance Booster */
	int powertable_size[NUM_OF_DOMAIN];
	struct profiler_freq_estpower_data *powertable[NUM_OF_DOMAIN];
	int target_cpuid_cur;
	int pmqos_minlock_prev[NUM_OF_DOMAIN];
	int pmqos_minlock_next[NUM_OF_DOMAIN];
	int cur_freqlv[NUM_OF_DOMAIN];
	int max_minlock_freq;
};

struct profiler_pid_data {
	int list_readout;
	u32 list[NUM_OF_PID];
	int list_top;
};

/** External(Exynos) **/
void profiler_set_targetframetime(int us);
void profiler_set_targettime_margin(int us);
void profiler_set_util_margin(int percentage);
void profiler_set_decon_time(int us);
void profiler_set_vsync(ktime_t time_us);
void profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
void profiler_get_run_times(u64 *times);

/* Governor setting */
void profiler_set_profiler_governor(int mode);

/* freq margin */
int profiler_get_freq_margin(void);
int profiler_set_freq_margin(int freq_margin);

/* pid filter */
void profiler_pid_get_list(u16 *list);
struct profiler_cputracer_data {
	u64 bitmap[NUM_OF_CPU_DOMAIN];
	int hitcnt[NUM_OF_CPU_DOMAIN];
	int qsz;
};
u32 profiler_get_target_cpuid(u64 *cputrace_info);
void profiler_set_cputracer_size(int queue_size);

/* stc */
void profiler_stc_set_comb_ctrl(int val);
int profiler_stc_set_powertable(int id, int cnt, struct freq_table *table);
void profiler_stc_set_busy_domain(int id);
void profiler_stc_set_cur_freqlv(int id, int idx);
void profiler_stc_set_cur_freq(int id, int freq);
int profiler_stc_config_show(int page_size, char *buf);
int profiler_stc_config_store(const char *buf);

/*weight table idx*/
unsigned int profiler_get_weight_table_idx_0(void);
int profiler_set_weight_table_idx_0(unsigned int value);
unsigned int profiler_get_weight_table_idx_1(void);
int profiler_set_weight_table_idx_1(unsigned int value);

/** Internal(KMD) **/
/* KMD */
void profiler_interframe_sw_update(ktime_t start, ktime_t end, ktime_t total);
void profiler_interframe_hw_update(ktime_t start, ktime_t end, bool end_of_frame);
struct profiler_interframe_data *profiler_get_next_frameinfo(void);
int profiler_get_target_frametime(void);

/* initialization */
void sgpu_profiler_init(struct amdgpu_device *adev);
void sgpu_profiler_deinit(void);
#endif /* _SGPU_PROFILER_V0_H_ */
