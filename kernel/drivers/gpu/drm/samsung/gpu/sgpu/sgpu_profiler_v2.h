/* SPDX-License-Identifier: GPL-2.0 */
/*
 * @file sgpu_profiler_v2.h
 * @copyright 2023 Samsung Electronics
 */
#ifndef _SGPU_PROFILER_V2_H_
#define _SGPU_PROFILER_V2_H_

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

#define profiler_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)

#define PROFILER_DVFS_INTERVAL_MS 16

#define PROFILER_TABLE_MAX				200
#define RENDERTIME_MAX					999999ULL
#define RENDERTIME_MIN					4000ULL
#define RENDERTIME_DEFAULT_FRAMETIME	16666ULL

#define PROFILER_PID_FRAMEINFO_TABLE_SIZE 8
#define PROFILER_FPS_CALC_AVERAGE_US 1000000L

#define PB_CONTROL_USER_TARGET_PID            0
#define PB_CONTROL_GPU_FORCED_BOOST_ACTIVE    1
#define PB_CONTROL_CPUTIME_BOOST_PM           2
#define PB_CONTROL_GPUTIME_BOOST_PM           3
#define PB_CONTROL_CPU_MGN_MAX                4
#define PB_CONTROL_CPU_MGN_FRAMEDROP_PM       5
#define PB_CONTROL_CPU_MGN_FPSDROP_PM         6
#define PB_CONTROL_CPU_MGN_ALL_FPSDROP_PM     7
#define PB_CONTROL_CPU_MGN_ALL_CL_BITMAP      8
#define PB_CONTROL_CPU_MINLOCK_BOOST_PM_MAX   9
#define PB_CONTROL_CPU_MINLOCK_FRAMEDROP_PM  10
#define PB_CONTROL_CPU_MINLOCK_FPSDROP_PM    11
#define PB_CONTROL_DVFS_INTERVAL             12
#define PB_CONTROL_FPS_AVERAGE_US            13

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
	u64 cputime, gputime, swaptime;

	u32 coreid;       /* running core# of renderer : get_cpu() */
	u32 pid;          /* renderer Thread ID : current->pid */
	u32 tgid;         /* app. PID : current-tgid */
	u64 timestamp;    /* timestamp(us) of swap call */
	char name[16];    /* renderer name : current->comm */
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
	int outinfolevel;
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
	int last_swaptime;
	int last_gputime;
	int last_cpufreqlv;
	int last_gpufreqlv;
	ktime_t vsync_lastsw;
	ktime_t vsync_curhw;
	ktime_t vsync_lasthw;
	ktime_t vsync_prev;
	ktime_t vsync_interval;
	atomic_t vsync_swapcall_counter;
	int vsync_frame_counter;
	int vsync_counter;

	/* Performance Booster - input params */
	u32 target_frametime;              /* 0: off, otherwise: do pb */
	int gpu_forced_boost_activepct;    /* Move to upper freq forcely, if GPU active ratio >= this */
	int user_target_pid;               /* Target PID by daemon, if 0 means detected PID */
	int cputime_boost_pm;              /* cputime += cputime * cputime_boost_pm / 1000L */
	int gputime_boost_pm;              /* gputime += gputime * gputime_boost_pm / 1000L */

	int pb_cpu_mgn_margin_max;         /* Upper boosting margin, if drop is detected */
	int framedrop_detection_pm_mgn;    /* if this < pfinfo.frame_drop_pm, frame is dropping */
	int fpsdrop_detection_pm_mgn;      /* if this < pfinfo.fps_drop_pm, fps is dropping */
	u32 target_clusters_bitmap;        /* 0: detected renderer, 1: include little, 2: include mid(big), 3: include little+mid(big) ... */
	int target_clusters_bitmap_fpsdrop_detection_pm;

	int pb_cpu_minlock_margin_max;     /* Upper boosting margin, if drop is detected */
	int framedrop_detection_pm_minlock;/* if this < pfinfo.frame_drop_pm, frame is dropping */
	int fpsdrop_detection_pm_minlock;  /* if this < pfinfo.fps_drop_pm, fps is dropping */

	/* Performance Booster - internal params */
	u32 target_fps;
	int powertable_size[NUM_OF_DOMAIN];
	struct profiler_freq_estpower_data *powertable[NUM_OF_DOMAIN];
	int target_clusterid;              /* Target cluster to control mainly */
	int pmqos_minlock_prev[NUM_OF_DOMAIN];
	int pmqos_minlock_next[NUM_OF_DOMAIN];
	int cur_freqlv[NUM_OF_DOMAIN];
	int pb_next_cpu_minlock_margin;    /* if this < 0, apply daemon's margin. Otherwise, apply this */
	int pb_next_cpu_minlock_boosting_fpsdrop;
	int pb_next_cpu_minlock_boosting_framedrop;

	/* Performance Booster - Profiling Data */
	struct pb_margin_profile {
		u16 no_boost;
		u16 no_value;
		s32 sum_margin;
	} pb_margin_info[NUM_OF_DOMAIN];

	/* PB: FPS Calculator & Drop Detector*/
	struct pidframeinfo {
		int latest_targetpid;
		int onesecidx;
		u32 pid[PROFILER_PID_FRAMEINFO_TABLE_SIZE];
		u32 hit[PROFILER_PID_FRAMEINFO_TABLE_SIZE];
		u64 latest_ts[PROFILER_PID_FRAMEINFO_TABLE_SIZE];
		u64 earliest_ts[PROFILER_PID_FRAMEINFO_TABLE_SIZE];
		u64 latest_interval[PROFILER_PID_FRAMEINFO_TABLE_SIZE];
		u64 expected_swaptime;
		u32 avg_fps;
		u32 exp_afps;
		u32 fps_drop_pm;
		u32 frame_drop_pm;
	} pfinfo;
};

struct profiler_pid_data {
	u32 list[NUM_OF_PID];
	u8 core_list[NUM_OF_PID];
	atomic_t list_readout;
	atomic_t list_top;
};

/** External(Exynos) **/
void profiler_set_targetframetime(int us);
void profiler_set_vsync(ktime_t time_us);
void profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
void profiler_get_run_times(u64 *times);

/* Governor setting */
void profiler_set_profiler_governor(int mode);

/* freq margin */
int profiler_get_freq_margin(void);
int profiler_set_freq_margin(int id, int freq_margin);

/* pid filter */
void profiler_pid_get_list(u32 *list, u8 *core_list);

/* Performance booster */
s32 profiler_get_pb_margin_info(int id, u16 *no_boost);
void profiler_set_pb_params(int idx, int value);
int profiler_get_pb_params(int idx);
void profiler_set_outinfo(int value);
int profiler_pb_set_powertable(int id, int cnt, struct freq_table *table);
void profiler_pb_set_cur_freqlv(int id, int idx);
void profiler_pb_set_cur_freq(int id, int freq);
void profiler_fps_profiler(int rtp_tail);
long profiler_pb_get_gpu_target(unsigned long freq, uint64_t activepct, uint64_t target_norm);
ssize_t profiler_show_status(char*);

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
void profiler_register_wakeup_func(void (*target_func)(void));
void profiler_wakeup(void);

/* initialization */
void sgpu_profiler_init(struct amdgpu_device *adev);
void sgpu_profiler_deinit(void);

/* ems api for peltboost */
extern void emstune_set_pboost(int mode, int level, int cpu, int value);
#endif /* _SGPU_PROFILER_V1_H_ */
