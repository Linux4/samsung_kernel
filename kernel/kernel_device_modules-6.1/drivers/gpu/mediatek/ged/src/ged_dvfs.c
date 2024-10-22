// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/module.h>

#if defined(CONFIG_MTK_GPUFREQ_V2)
#include <ged_gpufreq_v2.h>
#include <gpufreq_v2.h>
#else
#include <ged_gpufreq_v1.h>
#endif /* CONFIG_MTK_GPUFREQ_V2 */

#include <mt-plat/mtk_gpu_utility.h>
#include <asm/siginfo.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/hrtimer.h>
#include <linux/vmalloc.h>
#include <linux/timekeeping.h>

#include "ged_dvfs.h"
#include "ged_kpi.h"
#include "ged_monitor_3D_fence.h"
#include <ged_notify_sw_vsync.h>
#include "ged_log.h"
#include "ged_tracepoint.h"
#include "ged_base.h"
#include "ged_global.h"
#include "ged_eb.h"
#include "ged_dcs.h"
#include "ged_async.h"

#define MTK_DEFER_DVFS_WORK_MS          10000
#define MTK_DVFS_SWITCH_INTERVAL_MS     50

/* Definition of GED_DVFS_SKIP_ROUNDS is to skip DVFS when boost raised
 *  the value stands for counting down rounds of DVFS period
 *  Current using vsync that would be 16ms as period,
 *  below boost at (32, 48] seconds per boost
 */
#define GED_DVFS_SKIP_ROUNDS 3

//sliding window
#define MAX_SLIDE_WINDOW_SIZE 64

spinlock_t gsGpuUtilLock;

#define GPU_MEWTWO_TIMEOUT  90000

static struct hrtimer gpu_mewtwo_timer;
static ktime_t  gpu_mewtwo_timer_expire;

static struct mutex gsDVFSLock;
static struct mutex gsVSyncOffsetLock;
struct mutex gsPolicyLock;

static unsigned int g_iSkipCount;
static int g_dvfs_skip_round;

static unsigned int gpu_power;
static unsigned int gpu_dvfs_enable;
static unsigned int gpu_debug_enable;

static struct GED_DVFS_OPP_STAT *g_aOppStat;
int g_real_oppfreq_num;
int g_real_minfreq_idx;
unsigned long long g_ns_gpu_on_ts;

// MBrain
static u64 g_sum_loading;
static u64 g_sum_delta_time;

enum MTK_GPU_DVFS_TYPE g_CommitType;
unsigned long g_ulCommitFreq;

#ifdef ENABLE_COMMON_DVFS
static unsigned int boost_gpu_enable;
static unsigned int gpu_bottom_freq;
static unsigned int gpu_cust_boost_freq;
static unsigned int gpu_cust_upbound_freq;
static unsigned int gpu_opp_logs_enable;
#endif /* ENABLE_COMMON_DVFS */

static unsigned int g_ui32PreFreqID;
static unsigned int g_ui32CurFreqID;

static unsigned int g_bottom_freq_id;
static unsigned int g_last_def_commit_freq_id;
static unsigned int g_cust_upbound_freq_id;
static unsigned int g_cust_boost_freq_id;

//On boot flag
static u8 g_is_onboot;
#define FPS_LOW_THRESHOLD_ONBOOT 20

#define LIMITER_FPSGO 0
#define LIMITER_APIBOOST 1
#define ENABLE_ASYNC_RATIO 1
#define ASYNC_LOG_LEVEL (g_async_log_level|g_default_log_level)

/**
 * Define some global variable for async ratio.
 * g_async_ratio_support: Enable async ratio feature by dts.
 * g_min_async_oppnum: Additional added opp num at min stack freq
 * g_same_stack_in_opp: There are some opp with same stack freq (but different top)
 * in opp table. (for async ratio)
 * g_async_id_threshold: The largest oppidx whose stack freq does not repeat
 * g_oppnum_eachmask: opp num for each core mask, Ex: 2 opp for MC6
 */
static int FORCE_OPP;
static int g_async_ratio_support;
static int g_min_async_oppnum = 1;
static bool g_same_stack_in_opp;
static int g_async_id_threshold;
static int g_oppnum_eachmask = 1;
static struct GpuRawCounter g_counter_hs;
static unsigned int g_async_log_level;

unsigned int mips_support_flag;
unsigned int g_reduce_mips_support;

unsigned int g_gpu_timer_based_emu;

static unsigned int gpu_pre_loading;
unsigned int gpu_loading;
unsigned int gpu_av_loading;
unsigned int gpu_block;
unsigned int gpu_idle;
atomic_t g_gpu_loading_log = ATOMIC_INIT(0);
unsigned long g_um_gpu_tar_freq;

atomic_t g_gpu_loading_avg_log = ATOMIC_INIT(0);
unsigned int gpu_loading_avg;
#define GPU_LOADING_AVG_WINDOW_SIZE 10

spinlock_t g_sSpinLock;
/* calculate loading reset time stamp */
unsigned long g_ulCalResetTS_us;
/* previous calculate loading reset time stamp */
unsigned long g_ulPreCalResetTS_us;
/* last frame half, t0 */
unsigned long g_ulWorkingPeriod_us;

/* record previous DVFS applying time stamp */
unsigned long g_ulPreDVFS_TS_us;

/* calculate loading reset time stamp */
static unsigned long gL_ulCalResetTS_us;
/* previous calculate loading reset time stamp */
static unsigned long gL_ulPreCalResetTS_us;
/* last frame half, t0 */
static unsigned long gL_ulWorkingPeriod_us;
//MBrain: unit: ms
static uint64_t g_last_opp_cost_cal_ts;
static uint64_t g_last_opp_cost_update_ts_ms;

static unsigned long g_policy_tar_freq;
static int g_mode;

static unsigned int g_ui32FreqIDFromPolicy;

// struct GED_DVFS_BW_DATA gsBWprofile[MAX_BW_PROFILE];
// int g_bw_head;
// int g_bw_tail;

unsigned long g_ulvsync_period;
static GED_DVFS_TUNING_MODE g_eTuningMode;

unsigned int g_ui32EventStatus;
unsigned int g_ui32EventDebugStatus;
static int g_VsyncOffsetLevel;

static int g_probe_pid = GED_NO_UM_SERVICE;

#define DEFAULT_DVFS_STEP_MODE 0x103 /* ultra high/low step: 3/1 */
unsigned int dvfs_step_mode = DEFAULT_DVFS_STEP_MODE;
static int init;

static unsigned int g_ui32TargetPeriod_us = 16666;
static unsigned int g_ui32BoostValue = 100;

static unsigned int ged_commit_freq;
static unsigned int ged_commit_opp_freq;

/* global to store the platform min/max Freq/idx */
static unsigned int g_minfreq;
static unsigned int g_maxfreq;
static int g_minfreq_idx;
static int g_maxfreq_idx;
static int api_sync_flag;

/* need to sync to EB */
#define BATCH_MAX_READ_COUNT 32
/* formatted pattern |xxxx|yyyy 5x2 */
#define BATCH_PATTERN_LEN 10
#define BATCH_STR_SIZE (BATCH_PATTERN_LEN*BATCH_MAX_READ_COUNT)
char batch_freq[BATCH_STR_SIZE];
int avg_freq;
unsigned int pre_freq, cur_freq;

#define GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM 4
#define GED_FB_DVFS_FERQ_DROP_RATIO_LIMIT 30
static int is_fb_dvfs_triggered;
static int is_fallback_mode_triggered;
static unsigned int fallback_duration;   // unit: ms

unsigned int early_force_fallback_enable;
static unsigned int early_force_fallback;

#define SHADER_CORE 6
#define ULTRA_HIGH_STEP_SIZE 5
#define ULTRA_LOW_STEP_SIZE 2
static int g_max_core_num;

/* overdue parameter*/
#define OVERDUE_TH 14
#define OVERDUE_LIMIT_FRAME 8
#define OVERDUE_LIMIT_FREQ_SCALE 5
#define OVERDUE_FREQ_TH 2
static int overdue_counter;

static int FORCE_OPP = -1;
static int FORCE_TOP_OPP = -1;
static int g_async_ratio_support;
static int g_async_virtual_table_support;
unsigned int get_min_oppidx;
static int g_fallback_tuning = 50;
static int g_lb_last_opp;
static int g_step_size_by_platform[3];
static int g_ultra_step_size_by_platform[3];
static int g_step_size_freq_th[3] = {300000, 600000, 900000};

void ged_dvfs_last_and_target_cb(int t_gpu_target, int boost_accum_gpu)
{
	g_ui32TargetPeriod_us = t_gpu_target;
	g_ui32BoostValue = boost_accum_gpu;
	/* mtk_gpu_ged_hint(g_ui32TargetPeriod_us, g_ui32BoostValue); */
}

static bool ged_dvfs_policy(
	unsigned int ui32GPULoading, unsigned int *pui32NewFreqID,
	unsigned long t, long phase, unsigned long ul3DFenceDoneTime,
	bool bRefreshed);

struct ld_ud_table {
	int freq;
	int up;
	int down;
};
static struct ld_ud_table *loading_ud_table;

struct gpu_utilization_history {
	struct GpuUtilization_Ex util_data[MAX_SLIDE_WINDOW_SIZE];
	struct {
		unsigned int workload;   // unit: cycle
		unsigned long long t_gpu;   // unit: us
	} gpu_frame_property;
	unsigned int current_idx;
};
static struct gpu_utilization_history g_util_hs;

static int gx_dvfs_loading_mode = LOADING_ACTIVE;
static int gx_dvfs_workload_mode = WORKLOAD_ACTIVE;
struct GpuUtilization_Ex g_Util_Ex;
static int ged_get_dvfs_loading_mode(void);
static int ged_get_dvfs_workload_mode(void);

#define GED_DVFS_TIMER_BASED_DVFS_MARGIN 30
#define GED_DVFS_TIMER_BASED_DVFS_MARGIN_STEP 5
static int gx_tb_dvfs_margin = GED_DVFS_TIMER_BASED_DVFS_MARGIN;
static int gx_tb_dvfs_margin_cur = GED_DVFS_TIMER_BASED_DVFS_MARGIN;

#define MAX_TB_DVFS_MARGIN               99
#define MIN_TB_DVFS_MARGIN               5
#define CONFIGURE_TIMER_BASED_MODE       0x00000000
#define DYNAMIC_TB_MASK                  0x00000100
#define DYNAMIC_TB_PIPE_TIME_MASK        0x00000200   // phased out
#define DYNAMIC_TB_PERF_MODE_MASK        0x00000400
#define DYNAMIC_TB_FIX_TARGET_MASK       0x00000800   // phased out
#define TIMER_BASED_MARGIN_MASK          0x000000ff
#define MIN_TB_DVFS_MARGIN_HIGH_FPS      10
#define DVFS_MARGIN_HIGH_FPS_THRESHOLD   16000
#define SLIDING_WINDOW_SIZE_THRESHOLD    32000
#define GED_TIMESTAMP_TYPE_D       0x1
#define GED_TIMESTAMP_TYPE_1       0x2
// default loading-based margin mode + value is 30
static int g_tb_dvfs_margin_value = GED_DVFS_TIMER_BASED_DVFS_MARGIN;
static int g_tb_dvfs_margin_value_min = MIN_TB_DVFS_MARGIN;
static int g_tb_dvfs_margin_value_min_cmd;
static int g_tb_dvfs_margin_step = GED_DVFS_TIMER_BASED_DVFS_MARGIN_STEP;
static unsigned int g_tb_dvfs_margin_mode = DYNAMIC_TB_MASK | DYNAMIC_TB_PERF_MODE_MASK;
static int g_loading_slide_window_size_cmd;
static int g_fallback_idle;
static int g_last_commit_type;
static int g_last_commit_api_flag;
static unsigned long g_last_commit_before_api_boost;

static void ged_dvfs_early_force_fallback(struct GpuUtilization_Ex *Util_Ex)
{
	unsigned int util_iter = Util_Ex->util_iter;
	unsigned int util_mcu = Util_Ex->util_mcu;
	unsigned int sum = util_iter+util_mcu;

	if ((sum >= 110 && util_iter >= 70) ||
		(sum >= 110 && util_mcu >= 70) ||
		util_iter >= 90 ||
		util_mcu >= 90)
		early_force_fallback = 1;
	else
		early_force_fallback = 0;

}

static unsigned int ged_dvfs_high_step_size_query(void)
{
	int gpu_freq = ged_get_cur_freq();

	if (g_step_size_by_platform[0]) {
		if (gpu_freq < g_step_size_freq_th[1])
			return g_step_size_by_platform[0];
		else if (gpu_freq < g_step_size_freq_th[2])
			return g_step_size_by_platform[1];
		else
			return g_step_size_by_platform[2];
	}

	if (g_max_core_num == SHADER_CORE &&
		gx_dvfs_loading_mode == LOADING_MAX_ITERMCU &&
		early_force_fallback_enable)
		return ULTRA_LOW_STEP_SIZE ;
	else
		return 1;
}

static unsigned int ged_dvfs_low_step_size_query(void)
{
	int gpu_freq = ged_get_cur_freq();

	if (g_step_size_by_platform[0]) {
		if (gpu_freq < g_step_size_freq_th[1])
			return g_step_size_by_platform[0];
		else if (gpu_freq < g_step_size_freq_th[2])
			return g_step_size_by_platform[1];
		else
			return g_step_size_by_platform[2];
	}

	if (g_max_core_num == SHADER_CORE &&
		gx_dvfs_loading_mode == LOADING_MAX_ITERMCU &&
		early_force_fallback_enable)
		return ULTRA_LOW_STEP_SIZE ;
	else
		return 1;
}

static unsigned int ged_dvfs_ultra_high_step_size_query(void)
{
	int gpu_freq = ged_get_cur_freq();

	if (g_ultra_step_size_by_platform[0]) {
		if (gpu_freq < g_step_size_freq_th[1])
			return g_ultra_step_size_by_platform[0];
		else if (gpu_freq < g_step_size_freq_th[2])
			return g_ultra_step_size_by_platform[1];
		else
			return g_ultra_step_size_by_platform[2];
	}

	if (g_max_core_num == SHADER_CORE &&
		gx_dvfs_loading_mode == LOADING_MAX_ITERMCU &&
		early_force_fallback_enable)
		return ULTRA_HIGH_STEP_SIZE ;
	else
		return (dvfs_step_mode & 0xff);
}

static unsigned int ged_dvfs_ultra_low_step_size_query(void)
{
	if (g_step_size_by_platform[0])
		return ged_dvfs_low_step_size_query();

	if (g_max_core_num == SHADER_CORE &&
		gx_dvfs_loading_mode == LOADING_MAX_ITERMCU &&
		early_force_fallback_enable)
		return ULTRA_LOW_STEP_SIZE ;
	else
		return (dvfs_step_mode & 0xff00) >> 8;
}

static void _init_loading_ud_table(void)
{
	int i;
	int num = ged_get_opp_num();

	if (!loading_ud_table)
		loading_ud_table = ged_alloc(sizeof(struct ld_ud_table) * num);

	for (i = 0; i < num; ++i) {
		loading_ud_table[i].freq = ged_get_freq_by_idx(i);
		loading_ud_table[i].up = (100 - gx_tb_dvfs_margin_cur);
	}

	for (i = 0; i < num - 1; ++i) {
		int a = loading_ud_table[i].freq;
		int b = loading_ud_table[i+1].freq;

		if (a != 0)
			loading_ud_table[i].down
				= ((100 - gx_tb_dvfs_margin_cur) * b) / a;
	}

	if (num >= 2)
		loading_ud_table[num-1].down = loading_ud_table[num-2].down;
}

static void gpu_util_history_init(void)
{
	memset(&g_util_hs, 0, sizeof(struct gpu_utilization_history));

#if ENABLE_ASYNC_RATIO
	memset(&g_counter_hs, 0, sizeof(struct GpuRawCounter));
#endif /*ENABLE_ASYNC_RATIO*/
}

static void gpu_util_history_update(struct GpuUtilization_Ex *util_ex)
{
	unsigned int current_idx = g_util_hs.current_idx + 1;
	unsigned long long max_workload =
		util_ex->freq * util_ex->delta_time / 1000000;   // unit: cycle
	unsigned long long max_t_gpu = util_ex->delta_time / 1000;   // unit: us
	unsigned int workload_mode = 0;

	if (current_idx >= MAX_SLIDE_WINDOW_SIZE)
		current_idx = 0;   // wrap ring buffer index position around

	/* update util history */
	memcpy((void *)&g_util_hs.util_data[current_idx], (void *)util_ex,
		sizeof(struct GpuUtilization_Ex));

	/* update frame property */
	// workload = freq * delta_time * loading
	// t_gpu = delta_time * loading
	workload_mode = ged_get_dvfs_workload_mode();
	if (workload_mode == WORKLOAD_3D) {
		g_util_hs.gpu_frame_property.workload +=
			max_workload * util_ex->util_3d / 100;
		g_util_hs.gpu_frame_property.t_gpu +=
			max_t_gpu * util_ex->util_3d / 100;
	} else if (workload_mode == WORKLOAD_ITER) {
		g_util_hs.gpu_frame_property.workload +=
			max_workload * util_ex->util_iter / 100;
		g_util_hs.gpu_frame_property.t_gpu +=
			max_t_gpu * util_ex->util_iter / 100;
	} else if (workload_mode == WORKLOAD_MAX_ITERMCU) {
		unsigned int util_temp = MAX(util_ex->util_iter, util_ex->util_mcu);

		g_util_hs.gpu_frame_property.workload +=
			max_workload * util_temp / 100;
		g_util_hs.gpu_frame_property.t_gpu +=
			max_t_gpu * util_temp / 100;
	} else {   // WORKLOAD_ACTIVE or unknown mode
		g_util_hs.gpu_frame_property.workload +=
			max_workload * util_ex->util_active / 100;
		g_util_hs.gpu_frame_property.t_gpu +=
			max_t_gpu * util_ex->util_active / 100;
	}

	g_util_hs.current_idx = current_idx;

#if ENABLE_ASYNC_RATIO
	g_counter_hs.util_active_raw		+= util_ex->util_active_raw;
	g_counter_hs.util_iter_raw		+= util_ex->util_iter_raw;
	g_counter_hs.util_mcu_raw		+= util_ex->util_mcu_raw;
	g_counter_hs.util_sc_comp_raw		+= util_ex->util_sc_comp_raw;
	g_counter_hs.util_l2ext_raw		+= util_ex->util_l2ext_raw;
	g_counter_hs.util_irq_raw		+= util_ex->util_irq_raw;
#endif /*ENABLE_ASYNC_RATIO*/
}
static unsigned int gpu_util_history_query_loading(unsigned int window_size_us)
{
	struct GpuUtilization_Ex *util_ex;
	unsigned int loading_mode = 0;
	unsigned long long sum_loading = 0;   // unit: % * us
	unsigned long long sum_delta_time = 0;   // unit: us
	unsigned long long remaining_time, delta_time;   // unit: us
	unsigned int window_avg_loading = 0;
	unsigned int cidx = g_util_hs.current_idx;
	unsigned int his_loading = 0;
	int pre_idx = cidx - MAX_SLIDE_WINDOW_SIZE;
	int his_idx = 0;
	int i = 0;

	for (i = cidx; i > pre_idx; i--) {
		// ring buffer index wrapping
		if (i >= 0)
			his_idx = i;
		else
			his_idx = i + MAX_SLIDE_WINDOW_SIZE;
		util_ex = &g_util_hs.util_data[his_idx];

		// calculate loading based on different loading mode
		loading_mode = ged_get_dvfs_loading_mode();
		if (loading_mode == LOADING_MAX_3DTA_COM) {
			his_loading = MAX(util_ex->util_3d, util_ex->util_ta) +
				util_ex->util_compute;
		} else if (loading_mode == LOADING_MAX_3DTA) {
			his_loading = MAX(util_ex->util_3d, util_ex->util_ta);
		} else if (loading_mode == LOADING_ITER) {
			his_loading = util_ex->util_iter;
		} else if (loading_mode == LOADING_MAX_ITERMCU) {
			his_loading = MAX(util_ex->util_iter,
				util_ex->util_mcu);
		} else {   // LOADING_ACTIVE or unknown mode
			his_loading = util_ex->util_active;
		}

		// accumulate data
		remaining_time = window_size_us - sum_delta_time;
		delta_time = util_ex->delta_time / 1000;
		if (delta_time > remaining_time)
			delta_time = remaining_time;   // clip delta_time
		sum_loading += his_loading * delta_time;
		sum_delta_time += delta_time;

		// data is sufficient
		if (sum_delta_time >= window_size_us)
			break;
	}

	if (sum_delta_time != 0)
		window_avg_loading = sum_loading / sum_delta_time;

	if (window_avg_loading > 100)
		window_avg_loading = 100;

	//MBrain
	g_sum_loading += sum_loading;
	g_sum_delta_time += sum_delta_time;

	return window_avg_loading;
}

static unsigned int gpu_util_history_query_frequency(
	unsigned int window_size_us)
{
	struct GpuUtilization_Ex *util_ex;
	unsigned long long sum_freq = 0;   // unit: KHz * us
	unsigned long long sum_delta_time = 0;   // unit: us
	unsigned long long remaining_time, delta_time;   // unit: us
	unsigned int window_avg_freq = 0;
	unsigned int cidx = g_util_hs.current_idx;
	unsigned int his_freq = 0;
	int pre_idx = cidx - MAX_SLIDE_WINDOW_SIZE;
	int his_idx = 0;
	int i = 0;

	for (i = cidx; i > pre_idx; i--) {
		// ring buffer index wrapping
		if (i >= 0)
			his_idx = i;
		else
			his_idx = i + MAX_SLIDE_WINDOW_SIZE;
		util_ex = &g_util_hs.util_data[his_idx];

		// calculate frequency
		his_freq = util_ex->freq;

		// accumulate data
		remaining_time = window_size_us - sum_delta_time;
		delta_time = util_ex->delta_time / 1000;
		if (delta_time > remaining_time)
			delta_time = remaining_time;   // clip delta_time
		sum_freq += his_freq * delta_time;
		sum_delta_time += delta_time;

		// data is sufficient
		if (sum_delta_time >= window_size_us)
			break;
	}

	if (sum_delta_time != 0)
		window_avg_freq = sum_freq / sum_delta_time;

	return window_avg_freq;
}

static void gpu_util_history_query_frame_property(unsigned int *workload,
	unsigned long long *t_gpu)
{
	// copy latest counter value
	*workload = g_util_hs.gpu_frame_property.workload;
	*t_gpu = g_util_hs.gpu_frame_property.t_gpu;

	// reset counter value
	g_util_hs.gpu_frame_property.workload = 0;
	g_util_hs.gpu_frame_property.t_gpu = 0;
}

unsigned long ged_query_info(GED_INFO eType)
{
	unsigned int gpu_loading;
	unsigned int gpu_block;
	unsigned int gpu_idle;

	gpu_loading = 0;
	gpu_idle = 0;
	gpu_block = 0;

	switch (eType) {
	case GED_LOADING:
		mtk_get_gpu_loading(&gpu_loading);
		return gpu_loading;
	case GED_IDLE:
		mtk_get_gpu_idle(&gpu_idle);
		return gpu_idle;
	case GED_BLOCKING:
		mtk_get_gpu_block(&gpu_block);
		return gpu_block;
	case GED_PRE_FREQ:
		ged_get_freq_by_idx(g_ui32PreFreqID);
	case GED_PRE_FREQ_IDX:
		return g_ui32PreFreqID;
	case GED_CUR_FREQ:
		return ged_get_cur_freq();
	case GED_CUR_FREQ_IDX:
		return ged_get_cur_oppidx();
	case GED_MAX_FREQ_IDX:
#if defined(CONFIG_MTK_GPUFREQ_V2)
		return ged_get_min_oppidx();
#else
		return ged_get_max_oppidx();
#endif
	case GED_MAX_FREQ_IDX_FREQ:
		return g_maxfreq;
	case GED_MIN_FREQ_IDX:
#if defined(CONFIG_MTK_GPUFREQ_V2)
		return ged_get_max_oppidx();
#else
		return ged_get_min_oppidx();
#endif
	case GED_MIN_FREQ_IDX_FREQ:
		return g_minfreq;
	case GED_3D_FENCE_DONE_TIME:
		return ged_monitor_3D_fence_done_time();
	case GED_VSYNC_OFFSET:
		return ged_dvfs_vsync_offset_level_get();
	case GED_EVENT_STATUS:
		return g_ui32EventStatus;
	case GED_EVENT_DEBUG_STATUS:
		return g_ui32EventDebugStatus;
	case GED_SRV_SUICIDE:
		ged_dvfs_probe_signal(GED_SRV_SUICIDE_EVENT);
		return g_probe_pid;
	case GED_PRE_HALF_PERIOD:
		return g_ulWorkingPeriod_us;
	case GED_LATEST_START:
		return g_ulPreCalResetTS_us;

	default:
		return 0;
	}
}
EXPORT_SYMBOL(ged_query_info);

bool ged_gpu_is_heavy(void)
{
	unsigned int gpu_loading;
	int freq_id = ged_get_cur_oppidx();

	gpu_loading = gpu_util_history_query_loading(16 * 1000);

	return ((gpu_loading >= 85) && (freq_id <= 5));
}
EXPORT_SYMBOL(ged_gpu_is_heavy);

void ged_opp_stat_step(void)
{
	int cur_idx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);
	int pre_idx = (g_ui32PreFreqID > g_real_minfreq_idx)? g_real_minfreq_idx: g_ui32PreFreqID;

	cur_idx = (cur_idx > g_real_minfreq_idx)? g_real_minfreq_idx: cur_idx;

	if (ged_kpi_enabled() && g_aOppStat != NULL && pre_idx != cur_idx)
		g_aOppStat[pre_idx].uMem.aTrans[cur_idx]++;
}

//-----------------------------------------------------------------------------
void (*ged_dvfs_cal_gpu_utilization_ex_fp)(unsigned int *pui32Loading,
	unsigned int *pui32Block, unsigned int *pui32Idle,
	void *Util_Ex) = NULL;
EXPORT_SYMBOL(ged_dvfs_cal_gpu_utilization_ex_fp);
//-----------------------------------------------------------------------------
//MBrain opp cost calculation
static void ged_dvfs_update_opp_cost(u32 loading, u32 cur_opp_idx)
{
	//Unit: ms
	u64 cur_ts = 0, base_ts = 0, pwr_on_ts = 0;
	u64 active_time = 0, diff_time = 0;
	struct timespec64 tv = {0};

	if (ged_kpi_enabled() && g_aOppStat != NULL &&
		gpu_opp_logs_enable == 1 && g_curr_pwr_state == GED_POWER_ON) {
		cur_opp_idx = (cur_opp_idx > g_real_minfreq_idx)? g_real_minfreq_idx: cur_opp_idx;
		cur_ts = ged_get_time() / 1000000;
		pwr_on_ts = g_ns_gpu_on_ts / 1000000;
		base_ts = (g_last_opp_cost_cal_ts > pwr_on_ts)? g_last_opp_cost_cal_ts: pwr_on_ts;

		ktime_get_real_ts64(&tv);
		g_last_opp_cost_update_ts_ms = tv.tv_sec * 1000 + tv.tv_nsec / 1000000;

		if (cur_ts > base_ts) {
			diff_time = cur_ts - base_ts;
			active_time = (diff_time * loading / 100);

			/* update opp busy */
			g_aOppStat[cur_opp_idx].ui64Active += active_time;
			g_aOppStat[cur_opp_idx].ui64Idle += (diff_time - active_time);
			g_last_opp_cost_cal_ts = cur_ts;
		}
	}
}

bool ged_dvfs_cal_gpu_utilization_ex(unsigned int *pui32Loading,
	unsigned int *pui32Block, unsigned int *pui32Idle,
	struct GpuUtilization_Ex *Util_Ex)
{
	unsigned long ui32IRQFlags = 0;
	unsigned int cur_opp_idx = 0;
	u32 opp_loading = 0;

	unsigned int window_size_us = GPU_LOADING_AVG_WINDOW_SIZE*1000;
	static unsigned long long sum_delta_time;	 // unit: us
	static unsigned long long sum_loading ;   // unit: % * us
	unsigned long long delta_time;

	if (ged_dvfs_cal_gpu_utilization_ex_fp != NULL) {
		ged_dvfs_cal_gpu_utilization_ex_fp(pui32Loading, pui32Block,
			pui32Idle, (void *) Util_Ex);
		Util_Ex->freq = ged_get_cur_freq();

		gpu_util_history_update(Util_Ex);

		memcpy((void *)&g_Util_Ex, (void *)Util_Ex,
			sizeof(struct GpuUtilization_Ex));

		if (g_ged_gpueb_support)
			mtk_gpueb_dvfs_set_feedback_info(0, g_Util_Ex, 0);

		if (pui32Loading) {
			trace_GPU_DVFS__Loading(Util_Ex->util_active, Util_Ex->util_ta,
					Util_Ex->util_3d, Util_Ex->util_compute, Util_Ex->util_iter,
					Util_Ex->util_mcu);

			//use loading to decide whether early force fallback in LOADING_MAX_ITERMCU & loading base
			if (g_max_core_num == SHADER_CORE &&
				gx_dvfs_loading_mode == LOADING_MAX_ITERMCU &&
				!is_fb_dvfs_triggered &&
				early_force_fallback_enable)
				ged_dvfs_early_force_fallback(Util_Ex);
			else
				early_force_fallback = 0 ;

			delta_time = Util_Ex->delta_time / 1000;
			gpu_loading_avg = *pui32Loading;
			sum_loading += gpu_loading_avg * delta_time;
			sum_delta_time += delta_time;

			if (sum_delta_time >= window_size_us) {
				if (sum_delta_time != 0)
					gpu_loading_avg = sum_loading / sum_delta_time;

				if (gpu_loading_avg > 100)
					gpu_loading_avg = 100;

				atomic_set(&g_gpu_loading_avg_log, gpu_loading_avg);
				sum_loading = 0;
				sum_delta_time = 0;
			}

			gpu_av_loading = *pui32Loading;
			atomic_set(&g_gpu_loading_log, gpu_av_loading);

			/* MBrain opp cost calculation */
			cur_opp_idx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);
			opp_loading = *pui32Loading;
			opp_loading = (opp_loading > 100)? 100: opp_loading;
			ged_dvfs_update_opp_cost(opp_loading, cur_opp_idx);
			/* MBrain opp cost calculation end */
		}
		return true;
	}
	return false;
}

/*-----------------------------------------------------------------------------
 * void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID)
 * call back function
 * This shall be registered in vendor's GPU driver,
 * since each IP has its own rule
 */
static unsigned long g_ged_dvfs_commit_idx; /* freq opp idx for last policy(default stack) */
static unsigned long g_ged_dvfs_commit_top_idx; /* top freq opp idx for last policy*/
static unsigned long g_ged_dvfs_commit_dual; /* [0:7] for stack, [8:15] for top*/

void (*ged_dvfs_gpu_freq_commit_fp)(unsigned long ui32NewFreqID,
	GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited) = NULL;
EXPORT_SYMBOL(ged_dvfs_gpu_freq_commit_fp);

/*-----------------------------------------------------------------------------
 * void (*ged_dvfs_gpu_freq_dual_commit_fp)
 * call back function
 * This shall be registered in vendor's GPU driver,
 * since each IP has its own rule
 */
void (*ged_dvfs_gpu_freq_dual_commit_fp)(unsigned long gpuNewFreqID,
	unsigned long stackNewFreqID, int *pbCommited) = NULL;
EXPORT_SYMBOL(ged_dvfs_gpu_freq_dual_commit_fp);

unsigned long ged_dvfs_write_sysram_last_commit_idx(void)
{
	/* last commit idx = stack */
	mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_IDX, g_ged_dvfs_commit_idx);

	return g_ged_dvfs_commit_idx;
}
EXPORT_SYMBOL(ged_dvfs_write_sysram_last_commit_idx);

unsigned long ged_dvfs_write_sysram_last_commit_idx_test(int commit_idx)
{

	mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_IDX, commit_idx);

	return commit_idx;
}
EXPORT_SYMBOL(ged_dvfs_write_sysram_last_commit_idx_test);

unsigned long ged_dvfs_write_sysram_last_commit_top_idx(void)
{

	mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_TOP_IDX, g_ged_dvfs_commit_top_idx);

	return g_ged_dvfs_commit_top_idx;
}
EXPORT_SYMBOL(ged_dvfs_write_sysram_last_commit_top_idx);

unsigned long ged_dvfs_write_sysram_last_commit_top_idx_test(int commit_idx)
{

	mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_TOP_IDX, commit_idx);

	return commit_idx;
}
EXPORT_SYMBOL(ged_dvfs_write_sysram_last_commit_top_idx_test);

void ged_dvfs_set_sysram_last_commit_top_idx(int commit_idx)
{
	g_ged_dvfs_commit_top_idx = commit_idx;
}
EXPORT_SYMBOL(ged_dvfs_set_sysram_last_commit_top_idx);

void ged_dvfs_set_sysram_last_commit_dual_idx(int top_idx, int stack_idx)
{
	// [0:7] for stack, [8:15] for top
	int compose_idx = stack_idx & 0xFF;
	int tmp_top = (top_idx & 0xFF) << 8;

	if (g_async_virtual_table_support == 0 && (stack_idx != top_idx))
		GED_LOGE(" %s: opp index different top:%d, stack:%d ",
			__func__, top_idx, stack_idx);

	compose_idx += tmp_top;
	g_ged_dvfs_commit_dual = compose_idx;
}
EXPORT_SYMBOL(ged_dvfs_set_sysram_last_commit_dual_idx);


unsigned long ged_dvfs_write_sysram_last_commit_stack_idx(void)
{
	/* last commit idx = stack */
	mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_IDX, g_ged_dvfs_commit_idx);

	return g_ged_dvfs_commit_idx;
}
EXPORT_SYMBOL(ged_dvfs_write_sysram_last_commit_stack_idx);

unsigned long ged_dvfs_write_sysram_last_commit_dual(void)
{

	/* last commit idx = [0:7] for stack, [8:15] for top */
	mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_IDX, g_ged_dvfs_commit_dual);

	return g_ged_dvfs_commit_dual;
}
EXPORT_SYMBOL(ged_dvfs_write_sysram_last_commit_dual);


unsigned long ged_dvfs_write_sysram_last_commit_stack_idx_test(int commit_idx)
{

	mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_IDX, commit_idx);

	return commit_idx;
}
EXPORT_SYMBOL(ged_dvfs_write_sysram_last_commit_stack_idx_test);


unsigned long ged_dvfs_write_sysram_last_commit_dual_test(int top_idx, int stack_idx)
{
	// [0:7] for stack, [8:15] for top
	int compose_idx = stack_idx & 0xFF;
	int tmp_top = (top_idx & 0xFF) << 8;

	compose_idx += tmp_top;
	mtk_gpueb_sysram_write(SYSRAM_GPU_LAST_COMMIT_IDX, compose_idx);

	return compose_idx;
}
EXPORT_SYMBOL(ged_dvfs_write_sysram_last_commit_dual_test);

void ged_dvfs_set_sysram_last_commit_stack_idx(int commit_idx)
{
	g_ged_dvfs_commit_idx = commit_idx;
}
EXPORT_SYMBOL(ged_dvfs_set_sysram_last_commit_stack_idx);

unsigned long ged_dvfs_get_last_commit_idx(void)
{
	return g_ged_dvfs_commit_idx;
}
EXPORT_SYMBOL(ged_dvfs_get_last_commit_idx);

unsigned long ged_dvfs_get_last_commit_top_idx(void)
{
	return g_ged_dvfs_commit_top_idx;
}
EXPORT_SYMBOL(ged_dvfs_get_last_commit_top_idx);

unsigned long ged_dvfs_get_last_commit_stack_idx(void)
{
	return g_ged_dvfs_commit_idx;
}
EXPORT_SYMBOL(ged_dvfs_get_last_commit_stack_idx);

unsigned long ged_dvfs_get_last_commit_dual_idx(void)
{
	return g_ged_dvfs_commit_dual;
}
EXPORT_SYMBOL(ged_dvfs_get_last_commit_dual_idx);

int ged_write_sysram_pwr_hint(int pwr_hint)
{
	mtk_gpueb_sysram_write(SYSRAM_GPU_PWR_HINT, pwr_hint);

	return pwr_hint;
}
EXPORT_SYMBOL(ged_write_sysram_pwr_hint);

static int ged_dvfs_decide_ultra_step(int step)
{
	if (step > 0)
		return step * 2 + 1;
	else
		return 3;
}

int ged_dvfs_update_step_size(int low_step, int med_step, int high_step)
{
	// check step size is not zero
	if (low_step && med_step && high_step) {
		g_step_size_by_platform[0] = low_step;
		g_step_size_by_platform[1] = med_step;
		g_step_size_by_platform[2] = high_step;

		g_ultra_step_size_by_platform[0] = ged_dvfs_decide_ultra_step(low_step);
		g_ultra_step_size_by_platform[1] = ged_dvfs_decide_ultra_step(med_step);
		g_ultra_step_size_by_platform[2] = ged_dvfs_decide_ultra_step(high_step);
		return GED_OK;
	}
	return -1;
}
EXPORT_SYMBOL(ged_dvfs_update_step_size);

bool ged_dvfs_gpu_freq_commit(unsigned long ui32NewFreqID,
	unsigned long ui32NewFreq, GED_DVFS_COMMIT_TYPE eCommitType)
{
	int bCommited = false;

	int ui32CurFreqID, ui32CeilingID, ui32FloorID, ui32MinWorkingFreqID;
	unsigned int cur_freq = 0;
	enum gpu_dvfs_policy_state policy_state;

	ui32CurFreqID = ged_get_cur_oppidx();

	if (ui32CurFreqID == -1)
		return bCommited;

	if (FORCE_OPP >= 0)
		ui32NewFreqID = FORCE_OPP;

	if (eCommitType == GED_DVFS_FRAME_BASE_COMMIT ||
		eCommitType == GED_DVFS_LOADING_BASE_COMMIT)
		g_last_def_commit_freq_id = ui32NewFreqID;

	if (ged_dvfs_gpu_freq_commit_fp != NULL) {

		ui32CeilingID = ged_get_cur_limit_idx_ceil();
		ui32FloorID = ged_get_cur_limit_idx_floor();

		if (g_force_disable_dcs) {
			ui32MinWorkingFreqID = ged_get_min_oppidx_real();
			if (ui32NewFreqID > ui32MinWorkingFreqID)
				ui32NewFreqID = ui32MinWorkingFreqID;
		}

		if (ui32NewFreqID < ui32CeilingID)
			ui32NewFreqID = ui32CeilingID;

		if (ui32NewFreqID > ui32FloorID)
			ui32NewFreqID = ui32FloorID;

		g_ulCommitFreq = ged_get_freq_by_idx(ui32NewFreqID);
		ged_commit_freq = ui32NewFreq;
		ged_commit_opp_freq = ged_get_freq_by_idx(ui32NewFreqID);
		g_last_commit_api_flag = api_sync_flag;
		// record commit freq ID if api boost disable
		if (api_sync_flag == 0)
			g_last_commit_before_api_boost = ui32NewFreqID;

		/* do change */
		if (ui32NewFreqID != ui32CurFreqID || dcs_get_setting_dirty()) {
			/* call to ged gpufreq wrapper module */
			ged_gpufreq_commit(ui32NewFreqID, eCommitType, &bCommited);

			/*
			 * To-Do: refine previous freq contributions,
			 * since it is possible to have multiple freq settings
			 * in previous execution period
			 * Does this fatal for precision?
			 */
			ged_log_buf_print2(ghLogBuf_DVFS, GED_LOG_ATTR_TIME,
					"[GED_K] new freq ID committed: idx=%lu type=%u, g_type=%u",
					ui32NewFreqID, eCommitType, g_CommitType);
			if (bCommited == true) {
				ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] committed true");
				g_ui32PreFreqID = ui32CurFreqID;

				// For update opp logs
				if (ged_kpi_enabled() && g_aOppStat != NULL) {
					g_ui32CurFreqID = ged_get_cur_oppidx();
					if (g_aOppStat)
						ged_opp_stat_step();
				}
			}
		}

		if (ged_is_fdvfs_support()
			&& is_fb_dvfs_triggered && is_fdvfs_enable()) {
			memset(batch_freq, 0, sizeof(batch_freq));
			avg_freq = mtk_gpueb_sysram_batch_read(BATCH_MAX_READ_COUNT,
						batch_freq, BATCH_STR_SIZE);

			trace_tracing_mark_write(5566, "gpu_freq", avg_freq);

			trace_GPU_DVFS__Frequency(avg_freq,
				gpufreq_get_cur_freq(TARGET_DEFAULT) / 1000,
				gpufreq_get_cur_freq(TARGET_GPU) / 1000);
		} else {
			trace_tracing_mark_write(5566, "gpu_freq",
				(long long) ged_get_cur_stack_freq() / 1000);

			trace_GPU_DVFS__Frequency(ged_get_cur_stack_freq() / 1000,
				ged_get_cur_real_stack_freq() / 1000, ged_get_cur_top_freq() / 1000);
		}

		trace_tracing_mark_write(5566, "gpu_freq_ceil",
			(long long) ged_get_freq_by_idx(ui32CeilingID) / 1000);
		trace_tracing_mark_write(5566, "gpu_freq_floor",
			(long long) ged_get_freq_by_idx(ui32FloorID) / 1000);
		trace_tracing_mark_write(5566, "limitter_ceil",
			ged_get_cur_limiter_ceil());
		trace_tracing_mark_write(5566, "limitter_floor",
			ged_get_cur_limiter_floor());
		trace_tracing_mark_write(5566, "commit_type", eCommitType);
		if (dcs_get_adjust_support() % 2 != 0)
			trace_tracing_mark_write(5566, "preserve", g_force_disable_dcs);

		policy_state = ged_get_policy_state();

		if (is_fdvfs_enable()) {
			if (eCommitType != GED_DVFS_EB_DESIRE_COMMIT) {
				if (policy_state != POLICY_STATE_INIT) {
					ged_set_prev_policy_state(policy_state);
					trace_GPU_DVFS__Policy__Common(eCommitType, policy_state, g_CommitType);
				}
			}
		} else {
			if (policy_state != POLICY_STATE_INIT) {
				ged_set_prev_policy_state(policy_state);
				trace_GPU_DVFS__Policy__Common(eCommitType, policy_state, g_CommitType);
			}
		}
	}
	return bCommited;
}

bool ged_dvfs_gpu_freq_dual_commit(unsigned long stackNewFreqID,
	unsigned long ui32NewFreq, GED_DVFS_COMMIT_TYPE eCommitType,
	unsigned long topNewFreqID)
{
	int bCommited = false;

	int ui32CurFreqID, ui32CeilingID, ui32FloorID, newTopFreq, ui32MinWorkingFreqID;
	unsigned int cur_freq = 0;
	enum gpu_dvfs_policy_state policy_state;
	int ret = GED_OK;

	ui32CurFreqID = ged_get_cur_oppidx();
	newTopFreq = ged_get_top_freq_by_virt_opp(topNewFreqID);

	if (ui32CurFreqID == -1)
		return bCommited;

	g_last_commit_type = eCommitType;
	if (eCommitType == GED_DVFS_FRAME_BASE_COMMIT ||
		eCommitType == GED_DVFS_LOADING_BASE_COMMIT)
		g_last_def_commit_freq_id = stackNewFreqID;

	if (ged_dvfs_gpu_freq_dual_commit_fp == NULL || !g_async_ratio_support)
		return ged_dvfs_gpu_freq_commit(stackNewFreqID, ui32NewFreq, eCommitType);

	ui32CeilingID = ged_get_cur_limit_idx_ceil();
	ui32FloorID = ged_get_cur_limit_idx_floor();

	if (g_force_disable_dcs) {
		ui32MinWorkingFreqID = ged_get_min_oppidx_real();
		if (stackNewFreqID > ui32MinWorkingFreqID) {
			stackNewFreqID = ui32MinWorkingFreqID;
			topNewFreqID = ui32MinWorkingFreqID;
		}
	}

	if (stackNewFreqID < ui32CeilingID)
		stackNewFreqID = ui32CeilingID;

	if (topNewFreqID < ui32CeilingID)
		topNewFreqID = ui32CeilingID;

	if (stackNewFreqID > ui32FloorID)
		stackNewFreqID = ui32FloorID;

	if (topNewFreqID > ui32FloorID)
		topNewFreqID = ui32FloorID;

	g_ulCommitFreq = ged_get_freq_by_idx(stackNewFreqID);
	ged_commit_freq = ui32NewFreq;
	ged_commit_opp_freq = ged_get_freq_by_idx(stackNewFreqID);
	g_last_commit_api_flag = api_sync_flag;
	// record commit freq ID if api boost disable
	if (api_sync_flag == 0)
		g_last_commit_before_api_boost = stackNewFreqID;

	/* do change, top change or stack change */
	if (stackNewFreqID != ui32CurFreqID ||
		newTopFreq != ged_get_cur_top_freq() ||
		dcs_get_setting_dirty()) {
		if (FORCE_OPP >= 0)
			stackNewFreqID = FORCE_OPP;
		if (FORCE_TOP_OPP >= 0)
			topNewFreqID = FORCE_TOP_OPP;

		// if no force opp, stack opp must equal to top opp
		if (FORCE_OPP < 0 && FORCE_TOP_OPP < 0 &&
			topNewFreqID != stackNewFreqID) {
			GED_LOGE("[DVFS_ASYNC] topFreqID(%d) != stackFreqID(%d), force top = stack",
					topNewFreqID, stackNewFreqID);
			topNewFreqID = stackNewFreqID;
		}

		ret = ged_gpufreq_dual_commit(topNewFreqID, stackNewFreqID, eCommitType, &bCommited);
		if (ret == GED_ERROR_FAIL) {
			GED_LOGE("[DVFS_ASYNC] dual_commit fail");
			return bCommited;
		}

		/*
		 * To-Do: refine previous freq contributions,
		 * since it is possible to have multiple freq settings
		 * in previous execution period
		 * Does this fatal for precision?
		 */
		ged_log_buf_print2(ghLogBuf_DVFS, GED_LOG_ATTR_TIME,
				"[GED_K] new freq ID committed: idx=%lu type=%u, g_type=%u",
				stackNewFreqID, eCommitType, g_CommitType);
		if (bCommited == true) {
			ged_log_buf_print(ghLogBuf_DVFS, "[GED_K] committed true");
			g_ui32PreFreqID = ui32CurFreqID;

			// For update opp logs
			if (ged_kpi_enabled() && g_aOppStat != NULL) {
				g_ui32CurFreqID = ged_get_cur_oppidx();
				if (g_aOppStat)
					ged_opp_stat_step();
			}
		}
	}

	if (ged_is_fdvfs_support()
		&& is_fb_dvfs_triggered && is_fdvfs_enable()) {
		memset(batch_freq, 0, sizeof(batch_freq));
		avg_freq = mtk_gpueb_sysram_batch_read(BATCH_MAX_READ_COUNT,
					batch_freq, BATCH_STR_SIZE);

		trace_tracing_mark_write(5566, "gpu_freq", avg_freq);

		trace_GPU_DVFS__Frequency(avg_freq,
			gpufreq_get_cur_freq(TARGET_DEFAULT) / 1000,
			gpufreq_get_cur_freq(TARGET_GPU) / 1000);
	} else {
		trace_tracing_mark_write(5566, "gpu_freq",
			(long long) ged_get_cur_stack_freq() / 1000);

		trace_GPU_DVFS__Frequency(ged_get_cur_stack_freq() / 1000,
			ged_get_cur_real_stack_freq() / 1000, ged_get_cur_top_freq() / 1000);
	}

	trace_tracing_mark_write(5566, "gpu_freq_ceil",
		(long long) ged_get_freq_by_idx(ui32CeilingID) / 1000);
	trace_tracing_mark_write(5566, "gpu_freq_floor",
		(long long) ged_get_freq_by_idx(ui32FloorID) / 1000);
	trace_tracing_mark_write(5566, "limitter_ceil",
		ged_get_cur_limiter_ceil());
	trace_tracing_mark_write(5566, "limitter_floor",
		ged_get_cur_limiter_floor());
	trace_tracing_mark_write(5566, "commit_type", eCommitType);
	if (dcs_get_adjust_support() % 2 != 0)
		trace_tracing_mark_write(5566, "preserve", g_force_disable_dcs);

	policy_state = ged_get_policy_state();

	if (is_fdvfs_enable()) {
		if (eCommitType != GED_DVFS_EB_DESIRE_COMMIT) {
			if (policy_state != POLICY_STATE_INIT) {
				ged_set_prev_policy_state(policy_state);
				trace_GPU_DVFS__Policy__Common(eCommitType, policy_state, g_CommitType);
			}
		}
	} else {
		if (policy_state != POLICY_STATE_INIT) {
			ged_set_prev_policy_state(policy_state);
			trace_GPU_DVFS__Policy__Common(eCommitType, policy_state, g_CommitType);
		}
	}

	return bCommited;
}

/* update new oppidx(sysram idx) */
static void ged_fastdvfs_update_dcs(void)
{
	unsigned int opp_num;

	/* get oppidx num */
	opp_num = ged_get_opp_num();

	if (g_async_ratio_support) {
		/* get desire oppidx */
		mtk_gpueb_dvfs_get_desire_freq_dual(&g_desire_freq_stack,
			&g_desire_freq_top);

		/* commit oppidx */
		if (g_desire_freq_stack < opp_num && g_desire_freq_top < opp_num) {
			ged_dvfs_gpu_freq_dual_commit(g_desire_freq_stack,
				ged_get_freq_by_idx(g_desire_freq_stack),
				GED_DVFS_EB_DESIRE_COMMIT, g_desire_freq_top);
		}
	} else {
		/* get desire oppidx */
		mtk_gpueb_dvfs_get_desire_freq(&g_desire_freq);

		/* commit oppidx */
		if (g_desire_freq < opp_num)
			ged_dvfs_gpu_freq_commit(g_desire_freq,
				ged_get_freq_by_idx(g_desire_freq), GED_DVFS_EB_DESIRE_COMMIT);
	}
}

unsigned long get_ns_period_from_fps(unsigned int ui32Fps)
{
	return 1000000/ui32Fps;
}

void ged_dvfs_set_tuning_mode(GED_DVFS_TUNING_MODE eMode)
{
	g_eTuningMode = eMode;
}

void ged_dvfs_set_tuning_mode_wrap(int eMode)
{
	ged_dvfs_set_tuning_mode((GED_DVFS_TUNING_MODE)eMode);
}

GED_DVFS_TUNING_MODE ged_dvfs_get_tuning_mode(void)
{
	return g_eTuningMode;
}

GED_ERROR ged_dvfs_vsync_offset_event_switch(
	GED_DVFS_VSYNC_OFFSET_SWITCH_CMD eEvent, bool bSwitch)
{
	unsigned int ui32BeforeSwitchInterpret;
	unsigned int ui32BeforeDebugInterpret;
	GED_ERROR ret = GED_OK;

	mutex_lock(&gsVSyncOffsetLock);

	ui32BeforeSwitchInterpret = g_ui32EventStatus;
	ui32BeforeDebugInterpret = g_ui32EventDebugStatus;

	switch (eEvent) {
	case GED_DVFS_VSYNC_OFFSET_FORCE_ON:
		g_ui32EventDebugStatus |= GED_EVENT_FORCE_ON;
		g_ui32EventDebugStatus &= (~GED_EVENT_FORCE_OFF);
		break;
	case GED_DVFS_VSYNC_OFFSET_FORCE_OFF:
		g_ui32EventDebugStatus |= GED_EVENT_FORCE_OFF;
		g_ui32EventDebugStatus &= (~GED_EVENT_FORCE_ON);
		break;
	case GED_DVFS_VSYNC_OFFSET_DEBUG_CLEAR_EVENT:
		g_ui32EventDebugStatus &= (~GED_EVENT_FORCE_ON);
		g_ui32EventDebugStatus &= (~GED_EVENT_FORCE_OFF);
		break;
	case GED_DVFS_VSYNC_OFFSET_TOUCH_EVENT:
		/* touch boost */
#ifdef ENABLE_COMMON_DVFS
		if (bSwitch == GED_TRUE)
			ged_dvfs_boost_gpu_freq();
#endif /* ENABLE_COMMON_DVFS */

		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_TOUCH) :
			(g_ui32EventStatus &= (~GED_EVENT_TOUCH));
		break;
	case GED_DVFS_VSYNC_OFFSET_THERMAL_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_THERMAL) :
			(g_ui32EventStatus &= (~GED_EVENT_THERMAL));
		break;
	case GED_DVFS_VSYNC_OFFSET_WFD_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_WFD) :
			(g_ui32EventStatus &= (~GED_EVENT_WFD));
		break;
	case GED_DVFS_VSYNC_OFFSET_MHL_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_MHL) :
			(g_ui32EventStatus &= (~GED_EVENT_MHL));
		break;
	case GED_DVFS_VSYNC_OFFSET_VR_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_VR) :
			(g_ui32EventStatus &= (~GED_EVENT_VR));
		break;
	case GED_DVFS_VSYNC_OFFSET_DHWC_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_DHWC) :
			(g_ui32EventStatus &= (~GED_EVENT_DHWC));
		break;
	case GED_DVFS_VSYNC_OFFSET_GAS_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_GAS) :
			(g_ui32EventStatus &= (~GED_EVENT_GAS));
		ged_monitor_3D_fence_set_enable(!bSwitch);
		ret = ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	case GED_DVFS_VSYNC_OFFSET_LOW_POWER_MODE_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_LOW_POWER_MODE) :
			(g_ui32EventStatus &= (~GED_EVENT_LOW_POWER_MODE));
		ret = ged_dvfs_probe_signal(GED_LOW_POWER_MODE_SIGNAL_EVENT);
		break;
	case GED_DVFS_VSYNC_OFFSET_MHL4K_VID_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_MHL4K_VID) :
			(g_ui32EventStatus &= (~GED_EVENT_MHL4K_VID));
		ret = ged_dvfs_probe_signal(GED_MHL4K_VID_SIGNAL_EVENT);
		break;
	case GED_DVFS_VSYNC_OFFSET_VILTE_VID_EVENT:
		(bSwitch) ? (g_ui32EventStatus |= GED_EVENT_VILTE_VID) :
			(g_ui32EventStatus &= (~GED_EVENT_VILTE_VID));
		ret = ged_dvfs_probe_signal(GED_VILTE_VID_SIGNAL_EVENT);
		break;
	case GED_DVFS_BOOST_HOST_EVENT:
		ret = ged_dvfs_probe_signal(GED_SIGNAL_BOOST_HOST_EVENT);
		goto CHECK_OUT;

	case GED_DVFS_VSYNC_OFFSET_LOW_LATENCY_MODE_EVENT:
		(bSwitch) ?
		(g_ui32EventStatus |= GED_EVENT_LOW_LATENCY_MODE) :
		(g_ui32EventStatus &= (~GED_EVENT_LOW_LATENCY_MODE));
		ret = ged_dvfs_probe_signal
		(GED_LOW_LATENCY_MODE_SIGNAL_EVENT);
		break;
	default:
		GED_LOGE("%s: not acceptable event:%u\n", __func__, eEvent);
		ret = GED_ERROR_INVALID_PARAMS;
		goto CHECK_OUT;
	}
	mtk_ged_event_notify(g_ui32EventStatus);

	if ((ui32BeforeSwitchInterpret != g_ui32EventStatus) ||
		(ui32BeforeDebugInterpret != g_ui32EventDebugStatus) ||
		(g_ui32EventDebugStatus & GED_EVENT_NOT_SYNC))
		ret = ged_dvfs_probe_signal(GED_DVFS_VSYNC_OFFSET_SIGNAL_EVENT);

CHECK_OUT:
	mutex_unlock(&gsVSyncOffsetLock);
	return ret;
}

void ged_dvfs_vsync_offset_level_set(int i32level)
{
	g_VsyncOffsetLevel = i32level;
}

int ged_dvfs_vsync_offset_level_get(void)
{
	return g_VsyncOffsetLevel;
}

GED_ERROR ged_dvfs_um_commit(unsigned long gpu_tar_freq, bool bFallback)
{
#ifdef ENABLE_COMMON_DVFS
	int i;
	int ui32CurFreqID;
	int minfreq_idx;
	unsigned int ui32NewFreqID = 0;
	unsigned long gpu_freq;
	unsigned int sentinalLoading = 0;

	unsigned long ui32IRQFlags;

	ui32CurFreqID = ged_get_cur_oppidx();
	minfreq_idx = ged_get_min_oppidx();

	if (g_gpu_timer_based_emu)
		return GED_ERROR_INTENTIONAL_BLOCK;

#ifdef GED_DVFS_UM_CAL
	mutex_lock(&gsDVFSLock);

	if (gL_ulCalResetTS_us  - g_ulPreDVFS_TS_us != 0) {
		sentinalLoading =
		((gpu_loading * (gL_ulCalResetTS_us - gL_ulPreCalResetTS_us)) +
		100 * gL_ulWorkingPeriod_us) /
		(gL_ulCalResetTS_us - g_ulPreDVFS_TS_us);
		if (sentinalLoading > 100) {
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] g_ulCalResetTS_us: %lu g_ulPreDVFS_TS_us: %lu",
				gL_ulCalResetTS_us, g_ulPreDVFS_TS_us);
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] gpu_loading: %u g_ulPreCalResetTS_us:%lu",
				gpu_loading, gL_ulPreCalResetTS_us);
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] g_ulWorkingPeriod_us: %lu",
				gL_ulWorkingPeriod_us);
			ged_log_buf_print(ghLogBuf_DVFS,
				"[GED_K] gpu_av_loading: WTF");

			if (gL_ulWorkingPeriod_us == 0)
				sentinalLoading = gpu_loading;
			else
				sentinalLoading = 100;
		}
		gpu_loading = sentinalLoading;
	} else {
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K] gpu_av_loading: 5566/ %u", gpu_loading);
		gpu_loading = 0;
	}

	gpu_pre_loading = gpu_av_loading;
	gpu_av_loading = gpu_loading;
	atomic_set(&g_gpu_loading_log, gpu_av_loading);

	g_ulPreDVFS_TS_us = gL_ulCalResetTS_us;

	/* Magic to kill ged_srv */
	if (gpu_tar_freq & 0x1)
		ged_dvfs_probe_signal(GED_SRV_SUICIDE_EVENT);

	if (bFallback == true) /* fallback mode, gpu_tar_freq is freq idx */
		ged_dvfs_policy(gpu_loading, &ui32NewFreqID, 0, 0, 0, true);
	else {
		/* Search suitable frequency level */
		g_CommitType = MTK_GPU_DVFS_TYPE_VSYNCBASED;
		g_um_gpu_tar_freq = gpu_tar_freq;
		g_policy_tar_freq = g_um_gpu_tar_freq;
		g_mode = 1;

		ui32NewFreqID = minfreq_idx;
		for (i = 0; i <= minfreq_idx; i++) {
			gpu_freq = ged_get_freq_by_idx(i);

			if (gpu_tar_freq > gpu_freq) {
				if (i == 0)
					ui32NewFreqID = 0;
				else
					ui32NewFreqID = i-1;
				break;
			}
		}
	}

	ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K] rdy to commit (%u)", ui32NewFreqID);

	if (bFallback == true)
		ged_dvfs_gpu_freq_commit(ui32NewFreqID,
				ged_get_freq_by_idx(ui32NewFreqID),
				GED_DVFS_LOADING_BASE_COMMIT);
	else
		ged_dvfs_gpu_freq_commit(ui32NewFreqID, gpu_tar_freq,
				GED_DVFS_LOADING_BASE_COMMIT);

	mutex_unlock(&gsDVFSLock);
#endif /* GED_DVFS_UM_CAL */
#else
	gpu_pre_loading = 0;
#endif /* ENABLE_COMMON_DVFS */

	return GED_OK;
}

#define DEFAULT_DVFS_MARGIN 300 /* 30% margin */
#define FIXED_FPS_MARGIN 3 /* Fixed FPS margin: 3fps */
#define FB_ADD_MARGIN 100 /* 10% margin */
#define TIMER_LATENCY 500000 /* 500us */

int gx_fb_dvfs_margin = DEFAULT_DVFS_MARGIN;/* 10-bias */

#define MAX_DVFS_MARGIN 990 /* 99% margin */
#define MIN_DVFS_MARGIN 50 /* 5% margin */

/* dynamic margin mode for FPSGo control fps margin */
#define DYNAMIC_MARGIN_MODE_CONFIG_FPS_MARGIN 0x10

/* dynamic margin mode for fixed fps margin */
#define DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN 0x11

/* dynamic margin mode, margin low bound 1% */
#define DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN 0x12

/* dynamic margin mode, extend-config step and low bound */
#define DYNAMIC_MARGIN_MODE_EXTEND 0x13

/* dynamic margin mode, performance */
#define DYNAMIC_MARGIN_MODE_PERF 0x14

/* configure margin mode */
#define CONFIGURE_MARGIN_MODE 0x00

/* variable margin mode OPP Iidx */
#define VARIABLE_MARGIN_MODE_OPP_INDEX 0x01

#define MIN_MARGIN_INC_STEP 10 /* 1% headroom */

#define MAX_FALLBACK_DURATION 50   // unit: ms

// default frame-based margin mode + value is 130
static int dvfs_margin_value = DEFAULT_DVFS_MARGIN;
unsigned int dvfs_margin_mode = DYNAMIC_MARGIN_MODE_PERF;
static int g_uncomplete_type;

static int dvfs_min_margin_inc_step = MIN_MARGIN_INC_STEP;
static int dvfs_margin_low_bound = MIN_DVFS_MARGIN;

static int ged_dvfs_is_fallback_mode_triggered(void)
{
	return is_fallback_mode_triggered;
}

static void ged_dvfs_trigger_fb_dvfs(void)
{
	is_fb_dvfs_triggered = 1;
}

// input argument unit: nanosecond
static void set_fb_timeout(int t_gpu_target_ori, int t_gpu_target_margin)
{
	switch (g_frame_target_mode) {
	case 1:
		fb_timeout = (u64)t_gpu_target_ori * g_frame_target_time  / 10;
		break;
	case 2:
		fb_timeout = (u64)t_gpu_target_margin * g_frame_target_time / 10;
		break;
	default:
		fb_timeout = (u64)g_frame_target_time * 1000000; //ms to ns
		break;
	}
}

// choose the smallest oppidx (highest freq) for same stack freq
static int get_max_oppidx_with_same_stack(int oppidx)
{
	if (!oppidx)
		return oppidx;

	while (oppidx > 1 &&
		  ged_get_sc_freq_by_virt_opp(oppidx) == ged_get_sc_freq_by_virt_opp(oppidx - 1))
		oppidx--;

	return oppidx;
}

#if ENABLE_ASYNC_RATIO
static unsigned int calculate_performance(struct async_counter *counters, unsigned int ratio)
{
	long perf = 0;

	if (!ratio) {
		GED_LOGE("[DVFS_ASYNC] - %s: ratio is zero", __func__);
		return (unsigned int)perf;
	}
	if (ratio == 100) {
		GED_LOGD_IF(ASYNC_LOG_LEVEL,
			"[DVFS_ASYNC] - %s: ratio is 100, no need to calculate performance",
			__func__);
		return 100;
	}

	perf = (counters->iter * async_coeff[0]) / ratio * RATIO_SCAL +
			(counters->compute * async_coeff[1] +
			counters->l2ext * async_coeff[4] +
			counters->irq * async_coeff[7]) / ratio / ratio * RATIO_SCAL * RATIO_SCAL +
			(counters->compute * async_coeff[2] +
			counters->l2ext * async_coeff[5] +
			counters->irq * async_coeff[8]) * ratio / RATIO_SCAL +
			(counters->compute * async_coeff[3] +
			counters->l2ext * async_coeff[6] +
			counters->irq * async_coeff[9]) * ratio * ratio / RATIO_SCAL / RATIO_SCAL;

	perf += counters->gpuactive * async_coeff[10];

	if (perf <= 0) {
		GED_LOGE("[DVFS_ASYNC] - %s: perf result(%ld) is unreasonable",
				__func__, perf);
		perf = 0;
		return (unsigned int)perf;
	}

	perf = PERF_SCAL * COEFF_SCAL * counters->gpuactive / perf;
	return (unsigned int)perf;
}

static bool checkInDCS(int oppidx)
{
	int min_freq_id_real = ged_get_min_oppidx_real();

	if (oppidx > min_freq_id_real)
		return true;
	else
		return false;
}

static void reset_async_counters(void)
{
	g_counter_hs.util_active_raw		= 0;
	g_counter_hs.util_iter_raw		= 0;
	g_counter_hs.util_mcu_raw		= 0;
	g_counter_hs.util_sc_comp_raw		= 0;
	g_counter_hs.util_l2ext_raw		= 0;
	g_counter_hs.util_irq_raw		= 0;
}

static int get_async_counters(struct async_counter *counters)
{
	int memory_conter_scale = 4, shader_conter_scale = dcs_get_cur_core_num();

	if (!counters) {
		GED_LOGE("[DVFS_ASYNC] async_counter pointer is NULL");
		return ASYNC_ERROR;
	}
	if (!shader_conter_scale) {
		GED_LOGE("[DVFS_ASYNC] cannot obtain core num");
		return ASYNC_ERROR;
	}
	/**
	 * Get acumulated history counters for this frame.
	 * Counters needed for perf model:
	 * 004:CSHWCounters.GPU_ACTIVE
	 * 006:CSHWCounters.GPU_ITER_ACTIVE
	 * 022:SCCounters.COMPUTE_ACTIVE
	 * 029:MemSysCounters.L2_EXT_READ
	 * 054:CSHWCounters.CSHWIF1_IRQ_ACTIVE
	 */
	if (ged_get_dvfs_workload_mode() == WORKLOAD_MAX_ITERMCU)
		counters->gpuactive =
			(long)(g_counter_hs.util_iter_raw > g_counter_hs.util_mcu_raw ?
			g_counter_hs.util_iter_raw : g_counter_hs.util_mcu_raw);
	else
		counters->gpuactive = (long)g_counter_hs.util_active_raw;

	counters->iter		= (long)g_counter_hs.util_iter_raw;
	counters->compute	= (long)(g_counter_hs.util_sc_comp_raw / shader_conter_scale);
	counters->l2ext		= (long)g_counter_hs.util_l2ext_raw / memory_conter_scale;
	counters->irq		= (long)g_counter_hs.util_irq_raw;

	// MET
	trace_GPU_DVFS__Policy__Frame_based__Async_ratio__Counter(
		counters->gpuactive, counters->iter, counters->compute,
		counters->l2ext, counters->irq, g_counter_hs.util_mcu_raw);

	// reset counter value after get
	reset_async_counters();

	GED_LOGD_IF(ASYNC_LOG_LEVEL,
			"[DVFS_ASYNC] - %s: Async counters [gpuactive/iter/compute/l2ext/irq]: [%ld/%ld/%ld/%ld/%ld]\n",
			__func__, counters->gpuactive,
			counters->iter, counters->compute, counters->l2ext, counters->irq);

	if ((counters->gpuactive < 0) || (counters->iter < 0) ||
		(counters->compute < 0) || (counters->l2ext < 0) ||
		(counters->irq < 0)) {
		GED_LOGE("[DVFS_ASYNC] counters are invalid, some counters are negative");
		return ASYNC_ERROR;
	} else if (counters->gpuactive + counters->iter +
		counters->compute + counters->l2ext + counters->irq == 0) {
		GED_LOGD_IF(ASYNC_LOG_LEVEL, "[DVFS_ASYNC] counters are invalid, all counters are zero");
		return ASYNC_ERROR;
	} else {
		return ASYNC_OK;
	}
}

static int ged_async_ratio_perf_model(int oppidx, int tar_freq, bool is_decreasing)
{
	struct async_counter asyncCounter;
	int adjust_ratio = 0, perf_improve = 0, tar_opp = oppidx, tmp_ratio = 0, tmp_perf = 0;
	int min_freq_id_real = ged_get_min_oppidx_real(); // 50
	int perf_toler = 0, perf_require = 0;
	int cur_opp_id = ged_get_cur_oppidx();

	// 0. chack validity of the data
	if (oppidx < 0 || tar_freq < 0 || min_freq_id_real < 0 || cur_opp_id < 0)
		return tar_opp;
	if (oppidx > ged_get_min_oppidx())
		return ged_get_min_oppidx();

	//1. get counters from IPA
	if (get_async_counters(&asyncCounter) == ASYNC_ERROR) {
		GED_LOGD_IF(ASYNC_LOG_LEVEL, "[DVFS_ASYNC] - %s: get counters fail\n", __func__);
		return tar_opp;
	}

	// 2. if gpu freq tend to decrease
	if (is_decreasing) {
		// Make sure following calculations will not divide by zero
		if (!ged_get_sc_freq_by_virt_opp(oppidx) || !ged_get_top_freq_by_virt_opp(oppidx))
			return tar_opp;

		perf_toler = PERF_SCAL * tar_freq / ged_get_sc_freq_by_virt_opp(oppidx);
		adjust_ratio = RATIO_SCAL * ged_get_top_freq_by_virt_opp(oppidx + 1) /
						ged_get_top_freq_by_virt_opp(oppidx);
		perf_improve = calculate_performance(&asyncCounter, adjust_ratio);
		if (perf_improve > 100 || perf_improve == 0) {
			GED_LOGD_IF(ASYNC_LOG_LEVEL,
				"[DVFS_ASYNC] - %s: perf_improve(%d) is unreasonable\n",
				__func__, perf_improve);
			goto decreaseEnd;
		}

		// check perf gain to decide if we can reduce top freq
		if (perf_improve > perf_toler) {
			tar_opp = oppidx + 1;

			// if Not DCS & still able to consider lower freq, try opp 50
			if ((checkInDCS(oppidx) && g_oppnum_eachmask > 2) ||
				(!checkInDCS(oppidx) && g_min_async_oppnum > 1)) {
				tmp_ratio = RATIO_SCAL * ged_get_top_freq_by_virt_opp(oppidx + 2) /
							ged_get_top_freq_by_virt_opp(oppidx);
				tmp_perf = calculate_performance(&asyncCounter, tmp_ratio);
				if (tmp_perf > perf_toler) {
					tar_opp = oppidx + 2;
					adjust_ratio = tmp_ratio;
					perf_improve = tmp_perf;
				}
			}
		}
decreaseEnd:
		GED_LOGD_IF(ASYNC_LOG_LEVEL,
			"[DVFS_ASYNC] - %s: isDecreasing: fb_oppidx(%d), tar_freq(%d), tar_opp(%d), adjust_ratio(%d), perf_improve(%d), perf_toler(%d)\n",
			__func__, oppidx, tar_freq, tar_opp, adjust_ratio,
			perf_improve, perf_toler);

	// 3. if gpu freq tend to increase
	} else {
		// Make sure following calculations will not divide by zero
		if (!ged_get_sc_freq_by_virt_opp(cur_opp_id) ||
			!ged_get_top_freq_by_virt_opp(cur_opp_id))
			return tar_opp;

		perf_require = PERF_SCAL * tar_freq / ged_get_sc_freq_by_virt_opp(cur_opp_id);
		adjust_ratio = RATIO_SCAL * ged_get_top_freq_by_virt_opp(cur_opp_id - 1) /
						ged_get_top_freq_by_virt_opp(cur_opp_id);
		perf_improve = calculate_performance(&asyncCounter, adjust_ratio);
		if (perf_improve <= 100) {
			GED_LOGD_IF(ASYNC_LOG_LEVEL,
				"[DVFS_ASYNC] - %s: perf_improve(%d) is unreasonable\n",
				__func__, perf_improve);
			goto increaseEnd;
		}

		// opp - 1 is enough
		if (perf_improve > perf_require) {
			tar_opp = cur_opp_id - 1;
			goto increaseEnd;
		} else {
			// try opp - 2
			if (ged_get_sc_freq_by_virt_opp(cur_opp_id - 1) ==
				ged_get_sc_freq_by_virt_opp(cur_opp_id - 2)) {
				adjust_ratio = RATIO_SCAL *
						ged_get_top_freq_by_virt_opp(cur_opp_id - 2) /
						ged_get_top_freq_by_virt_opp(cur_opp_id);
				perf_improve = calculate_performance(&asyncCounter, adjust_ratio);
				if (perf_improve <= 100) {
					GED_LOGD_IF(ASYNC_LOG_LEVEL,
						"[DVFS_ASYNC] - %s: perf_improve(%d) is unreasonable\n",
						__func__, perf_improve);
					goto increaseEnd;
				}
				// opp - 2 is enough
				if (perf_improve > perf_require) {
					tar_opp = cur_opp_id - 2;
					goto increaseEnd;
				}
			}
			// modify tar_freq
			tar_opp = ged_get_oppidx_by_stack_freq(tar_freq * PERF_SCAL / perf_improve);
			tar_opp = get_max_oppidx_with_same_stack(tar_opp);
		}

increaseEnd:
		GED_LOGD_IF(ASYNC_LOG_LEVEL,
			"[DVFS_ASYNC] - %s: isIncreasing, fb_oppidx(%d), tar_freq(%d), tar_opp(%d), adjust_ratio(%d), perf_improve(%d), perf_require(%d)\n",
			__func__, oppidx, tar_freq, tar_opp, adjust_ratio,
			perf_improve, perf_require);
	}
	trace_tracing_mark_write(5566, "async_perf_high", perf_improve);
	trace_tracing_mark_write(5566, "async_adj_ratio", adjust_ratio);
	trace_GPU_DVFS__Policy__Frame_based__Async_ratio__Index(
		is_decreasing, adjust_ratio,	perf_improve, oppidx, tar_freq / 1000, tar_opp);

	return tar_opp;
}

// determine async policy: decreasing or increasing
// return true if is decreasing
static bool determine_async_policy(int cur_opp_id, int ui32NewFreqID)
{
	if (cur_opp_id <= ged_get_min_oppidx_real() && ui32NewFreqID >= g_async_id_threshold)
		return true;
	else if (cur_opp_id > ged_get_min_oppidx_real() &&
			ui32NewFreqID > ged_get_min_oppidx_real())
		return true;
	else if (cur_opp_id == (ged_get_min_oppidx_real() + 1) &&
			ui32NewFreqID >= g_async_id_threshold)
		return true;
	else
		return false;
}
// determine whether async policy is needed
// if cur_opp is at largest async ratio, use original frame-based policy
// if only one opp for DCS, no need to consider async_ratio when new freq ID in DCS
static bool check_async_policy_needed(int cur_opp_id, int ui32NewFreqID)
{
	// only one opp for DCS
	if (g_oppnum_eachmask == 1 && ui32NewFreqID > ged_get_min_oppidx_real())
		return false;

	return !((cur_opp_id == g_async_id_threshold ||
			 cur_opp_id == (ged_get_min_oppidx_real() + 1)) &&
				ui32NewFreqID < g_async_id_threshold);
}
#endif /*ENABLE_ASYNC_RATIO*/

/*
 *	input argument t_gpu, t_gpu_target unit: ns
 */
static int ged_dvfs_fb_gpu_dvfs(int t_gpu, int t_gpu_target,
	int target_fps_margin, unsigned int force_fallback)
{
	unsigned int ap_workload, ap_workload_pipe,
		ap_workload_real;   // unit: 100*cycle
	unsigned long long frame_t_gpu;   // for gpu_util_history api
	unsigned int frame_workload, frame_freq;   // for gpu_util_history api
	int t_gpu_target_hd, t_gpu_target_fb;   // apply headroom target, unit: us
	int t_gpu_pipe, t_gpu_real;   // unit: us
	int gpu_freq_pre, gpu_freq_tar, gpu_freq_floor;   // unit: KHZ
	int gpu_freq_overdue_max;   // unit: KHZ
	int i, ui32NewFreqID = 0;
	int minfreq_idx = ged_get_min_oppidx();
	static int num_pre_frames;
	static int cur_frame_idx;
	static int pre_frame_idx;
	static int busy_cycle[GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM];
	int gpu_busy_cycle = 0;
	int busy_cycle_cur;
	unsigned long ui32IRQFlags;
	static unsigned int force_fallback_pre;
	static int margin_low_bound;
	int ultra_high_step_size = (dvfs_step_mode & 0xff);
	int cur_opp_id = ged_get_cur_oppidx();
	int cur_fps = 0;

	gpu_freq_pre = ged_get_cur_freq();
	gpu_freq_overdue_max = (ged_get_max_freq_in_opp() * 1000) / OVERDUE_FREQ_TH;

	/* DVFS is not enabled */
	if (gpu_dvfs_enable == 0) {
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K][FB_DVFS] skip %s due to gpu_dvfs_enable=%u",
			__func__, gpu_dvfs_enable);
		is_fb_dvfs_triggered = 0;
		return gpu_freq_pre;
	}

	/* Skip DVFS on boot */
	cur_fps = ged_kpi_get_cur_fps();
	if (g_is_onboot) {
		g_is_onboot = (cur_fps < FPS_LOW_THRESHOLD_ONBOOT)? 1: 0;

		return gpu_freq_pre;
	}

	/* not main producer */
	if (t_gpu == -1) {
		is_fb_dvfs_triggered = 0;
		return gpu_freq_pre;
	}

	// force_fallback 0: FB, 1: LB
	if (force_fallback_pre != force_fallback) {
		force_fallback_pre = force_fallback;

		if (force_fallback == 1) {   // first transition from FB to LB
			/* Reset frame window when FB to LB */
			num_pre_frames = 0;
			for (i = 0; i < GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM; i++)
				busy_cycle[i] = 0;
		}

	}
	/* use LB policy */
	if (force_fallback == 1) {
		if (ged_get_policy_state() == POLICY_STATE_FORCE_LB)
			// reset frame property for FB control in the next iteration
			gpu_util_history_query_frame_property(&frame_workload,
				&frame_t_gpu);
		ged_set_backup_timer_timeout(lb_timeout);
		is_fb_dvfs_triggered = 0;

#if ENABLE_ASYNC_RATIO
		if (ged_get_policy_state() == POLICY_STATE_FORCE_LB)
			// reset frame property for FB control in the next iteration
			reset_async_counters();
#endif /*ENABLE_ASYNC_RATIO*/

		/* check overdue at last frame time of LB */
		if (t_gpu > (t_gpu_target * OVERDUE_TH / 10) &&
			gpu_freq_pre > gpu_freq_overdue_max)
			overdue_counter = OVERDUE_LIMIT_FRAME;
		else
			overdue_counter = 0;

		return gpu_freq_pre;
	}
	// reset if not in fallback mode
	g_fallback_idle = 0;

	spin_lock_irqsave(&gsGpuUtilLock, ui32IRQFlags);
	if (is_fallback_mode_triggered)
		is_fallback_mode_triggered = 0;
	spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);

	/* obtain GPU frame property */
	t_gpu /= 100000;   // change unit from ns to 100*us
	t_gpu_target /= 100000;   // change unit from ns to 100*us

	gpu_util_history_query_frame_property(&frame_workload, &frame_t_gpu);
	t_gpu_pipe = t_gpu;
	t_gpu_real = (int) (frame_t_gpu / 100);   // change unit from us to 100*us
	// define t_gpu = max(t_gpu_completion, t_gpu_real) for FB policy
	if (t_gpu_real > t_gpu)
		t_gpu = t_gpu_real;

	frame_freq = gpu_util_history_query_frequency(t_gpu_pipe * 100);
	ap_workload_real =
		frame_workload / 100;   // change unit from cycle to 100*cycle
	ap_workload_pipe =
		frame_freq * t_gpu_pipe / 1000;   // change unit from 0.1 to 100*cycle
	ap_workload = ap_workload_pipe;
	// define workload = max(t_gpu_completion * avg_freq, freq * t_gpu_real)
	if (ap_workload_real > ap_workload)
		ap_workload = ap_workload_real;

	/* calculate margin */
	if (dvfs_margin_mode == CONFIGURE_MARGIN_MODE) {   // fix margin mode
		gx_fb_dvfs_margin = dvfs_margin_value;
		margin_low_bound = dvfs_margin_low_bound;
	} else if ((dvfs_margin_mode & 0x10) &&
			(t_gpu_target > 0)) {   // dynamic margin mode
		/* dvfs_margin_mode == */
		/* DYNAMIC_MARGIN_MODE_CONFIG_FPS_MARGIN or */
		/* DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN) or */
		/* DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN or */
		/* DYNAMIC_MARGIN_MODE_EXTEND or */
		/* DYNAMIC_MARGIN_MODE_PERF */
		int temp;

		if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_PERF) {
			t_gpu_target_hd = t_gpu_target *
				(1000 - dvfs_margin_low_bound) / 1000;
			if (t_gpu_target_hd <= 0)
				t_gpu_target_hd = 1;
		} else
			t_gpu_target_hd = t_gpu_target;
		// from this point on, t_gpu_target & t_gpu_target_hd would be > 0

		if (t_gpu_pipe > (t_gpu_target * OVERDUE_TH / 10) &&
			gpu_freq_pre > gpu_freq_overdue_max)
			overdue_counter = OVERDUE_LIMIT_FRAME;

		if (t_gpu > t_gpu_target_hd) {   // previous frame overdued
			// adjust margin
			temp = (gx_fb_dvfs_margin*(t_gpu-t_gpu_target_hd))
				/ t_gpu_target_hd;
			if (temp < dvfs_min_margin_inc_step)
				temp = dvfs_min_margin_inc_step;
			gx_fb_dvfs_margin += temp;
		} else {   // previous frame in time
			// set margin low boud according to FPSGO's margin (+3/+1)
			if (dvfs_margin_mode
				== DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN)
				margin_low_bound = MIN_DVFS_MARGIN;
			else {
				int target_time_low_bound;

				if (dvfs_margin_mode ==
					DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN)
					target_fps_margin = FIXED_FPS_MARGIN;

				if (target_fps_margin <= 0)
					margin_low_bound = MIN_DVFS_MARGIN;
				else {
					target_time_low_bound =
					10000/((10000/t_gpu_target) +
					target_fps_margin);

					margin_low_bound =
					1000 *
					(t_gpu_target - target_time_low_bound)
					/ t_gpu_target;
				}

				// margin_low_bound range clipping
				if (margin_low_bound > dvfs_margin_value)
					margin_low_bound = dvfs_margin_value;

				if (margin_low_bound < dvfs_margin_low_bound)
					margin_low_bound = dvfs_margin_low_bound;
			}

			// adjust margin
			temp = (gx_fb_dvfs_margin*(t_gpu_target_hd-t_gpu))
				/ t_gpu_target_hd;
			gx_fb_dvfs_margin -= temp;
		}
	} else {   // unknown mode
		// use fix margin mode
		gx_fb_dvfs_margin = dvfs_margin_value;
		margin_low_bound = dvfs_margin_low_bound;
	}

	// check upper bound
	if (gx_fb_dvfs_margin > dvfs_margin_value)
		gx_fb_dvfs_margin = dvfs_margin_value;

	// check lower bound
	if (gx_fb_dvfs_margin < margin_low_bound)
		gx_fb_dvfs_margin = margin_low_bound;

	//  * 10 to keep unit uniform
	trace_GPU_DVFS__Policy__Frame_based__Margin(dvfs_margin_value,
		gx_fb_dvfs_margin, margin_low_bound);

	trace_GPU_DVFS__Policy__Frame_based__Margin__Detail(dvfs_margin_mode,
		target_fps_margin, dvfs_min_margin_inc_step, dvfs_margin_low_bound);

	t_gpu_target_hd = t_gpu_target * (1000 - gx_fb_dvfs_margin) / 1000;

	//  * 100 to keep unit uniform
	trace_GPU_DVFS__Policy__Frame_based__GPU_Time((t_gpu * 100),
								  (t_gpu_target * 100),
								  (t_gpu_target_hd * 100),
								  (t_gpu_real * 100),
								  (t_gpu_pipe * 100));
	trace_tracing_mark_write(5566, "t_gpu", (long long) t_gpu * 100);
	trace_tracing_mark_write(5566, "t_gpu_target", (long long) t_gpu_target * 100);

	// Hint target frame time w/z headroom
	if (ged_is_fdvfs_support())
		mtk_gpueb_dvfs_set_taget_frame_time(t_gpu_target_hd, gx_fb_dvfs_margin);

	/* select workload */
	if (ged_is_fdvfs_support() && is_fb_dvfs_triggered && is_fdvfs_enable()
		&& g_eb_workload != 0xFFFF) {
		g_eb_workload /= 100;   // change unit from cycle to 100*cycle
		busy_cycle_cur = (g_eb_workload < ap_workload) ? g_eb_workload : ap_workload;
	}
	else
		busy_cycle_cur = ap_workload;

	busy_cycle[cur_frame_idx] = busy_cycle_cur;

	if (num_pre_frames != GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM - 1) {
		gpu_busy_cycle = busy_cycle[cur_frame_idx];
		num_pre_frames++;
	} else {
		for (i = 0; i < GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM; i++)
			gpu_busy_cycle += busy_cycle[i];
		gpu_busy_cycle /= GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM;
		gpu_busy_cycle = (gpu_busy_cycle > busy_cycle_cur) ?
			gpu_busy_cycle : busy_cycle_cur;
	}

	trace_GPU_DVFS__Policy__Frame_based__Workload((busy_cycle_cur * 100),
		(gpu_busy_cycle * 100), (ap_workload_real * 100),
		(ap_workload_pipe * 100), ged_get_dvfs_loading_mode());

	/* compute target frequency */
	if (t_gpu_target_hd != 0)
		gpu_freq_tar = (gpu_busy_cycle / t_gpu_target_hd) * 1000;
	else
		gpu_freq_tar = gpu_freq_pre;

	gpu_freq_floor = gpu_freq_pre * GED_FB_DVFS_FERQ_DROP_RATIO_LIMIT / 100;
	/* overdue_counter : 4~1, limit gpu_freq_floor 95%~80% */
	if (overdue_counter <= 4 && overdue_counter > 0) {
		if (gpu_freq_pre > gpu_freq_overdue_max)
			gpu_freq_floor =
				gpu_freq_pre *
				(100 - (OVERDUE_LIMIT_FREQ_SCALE * (5 - overdue_counter))) / 100;
		overdue_counter--;
	}

	if (gpu_freq_tar < gpu_freq_floor)
		gpu_freq_tar = gpu_freq_floor;

	pre_frame_idx = cur_frame_idx;
	cur_frame_idx = (cur_frame_idx + 1) %
		GED_DVFS_BUSY_CYCLE_MONITORING_WINDOW_NUM;

	ui32NewFreqID = minfreq_idx;

	for (i = 0; i <= minfreq_idx; i++) {
		int gpu_freq;

		gpu_freq = ged_get_freq_by_idx(i);

		if (gpu_freq_tar > gpu_freq) {
			if (i == 0)
				ui32NewFreqID = 0;
			else
				ui32NewFreqID = i-1;
			break;
		}
	}

	if (dvfs_margin_mode == VARIABLE_MARGIN_MODE_OPP_INDEX)
		gx_fb_dvfs_margin = (ui32NewFreqID / 3)*10;

	/* overdue_counter : 8~4, prevent decreasing opp if previous frame overdue too much */
	if (overdue_counter <= 8 && overdue_counter > 4) {
		overdue_counter--;
		if (ui32NewFreqID > cur_opp_id &&
			gpu_freq_pre > gpu_freq_overdue_max)
			ui32NewFreqID = cur_opp_id;
	}


	ged_log_buf_print(ghLogBuf_DVFS,
	"[GED_K][FB_DVFS]t_gpu:%d,t_gpu_tar_hd:%d,gpu_freq_tar:%d,gpu_freq_pre:%d",
	t_gpu, t_gpu_target_hd, gpu_freq_tar, gpu_freq_pre);

	ged_log_buf_print(ghLogBuf_DVFS,
	"[GED_K][FB_DVFS]mode:%x,h:%d,margin:%d,l:%d,fps:+%d,l_b:%d,step:%d",
	dvfs_margin_mode, dvfs_margin_value, gx_fb_dvfs_margin,
	margin_low_bound, target_fps_margin, dvfs_margin_low_bound,
	dvfs_min_margin_inc_step);
	g_CommitType = MTK_GPU_DVFS_TYPE_VSYNCBASED;

	// choose the smallest oppidx (highest top freq) for same stack freq
	if (g_same_stack_in_opp && ui32NewFreqID > g_async_id_threshold)
		ui32NewFreqID = get_max_oppidx_with_same_stack(ui32NewFreqID);

#if ENABLE_ASYNC_RATIO
	//async ration trial run, g_async_id_threshold(48), ged_get_min_oppidx_real() = 50
	if (g_async_ratio_support &&
		(cur_opp_id >= g_async_id_threshold || ui32NewFreqID >= g_async_id_threshold)) {
		bool isDecreasing = false;
		int asyncID = 0;

		// 48/51 => higher(>48), async_ratio not change, use original frame-based policy
		if (check_async_policy_needed(cur_opp_id, ui32NewFreqID)) {
			isDecreasing = determine_async_policy(cur_opp_id, ui32NewFreqID);
			asyncID = ged_async_ratio_perf_model(
						ui32NewFreqID, gpu_freq_tar, isDecreasing);
			GED_LOGD_IF(ASYNC_LOG_LEVEL,
				"[DVFS_ASYNC] GPU freq %s: cur_opp_id(%d) ui32NewFreqID(%d) asyncID(%d) applyAsyncPolicy(%d)",
					isDecreasing ? "decreasing" : "increasing",
					cur_opp_id, ui32NewFreqID, asyncID, 1);

			if (g_async_ratio_support && asyncID >= 0)
				ui32NewFreqID = asyncID;
		}

		trace_GPU_DVFS__Policy__Frame_based__Async_ratio__Policy(
			cur_opp_id, ui32NewFreqID, asyncID,
			check_async_policy_needed(cur_opp_id, ui32NewFreqID), isDecreasing);
	}
#endif /*ENABLE_ASYNC_RATIO*/

	trace_GPU_DVFS__Policy__Frame_based__Frequency(gpu_freq_tar, gpu_freq_floor, ui32NewFreqID);

	if (g_async_ratio_support)
		ged_dvfs_gpu_freq_dual_commit((unsigned long)ui32NewFreqID,
			gpu_freq_tar, GED_DVFS_FRAME_BASE_COMMIT, (unsigned long)ui32NewFreqID);
	else
		ged_dvfs_gpu_freq_commit((unsigned long)ui32NewFreqID,
			gpu_freq_tar, GED_DVFS_FRAME_BASE_COMMIT);

	// add fallback timer margin to reduce fallback timer
	t_gpu_target_fb = t_gpu_target * (1000 - gx_fb_dvfs_margin - FB_ADD_MARGIN) / 1000;

	//t_gpu_target(unit: 100us) *10^5 =nanosecond
	if (is_fdvfs_enable()) {
		mtk_gpueb_sysram_write(SYSRAM_GPU_LEFT_TIME, t_gpu_target_hd);
		set_fb_timeout(t_gpu_target * 100000, t_gpu_target * 200000);
	} else
		set_fb_timeout(t_gpu_target * 100000, t_gpu_target_fb * 100000);

	ged_set_backup_timer_timeout(fb_timeout);
	ged_cancel_backup_timer();
	is_fb_dvfs_triggered = 0;
	fallback_duration = 0;   // reset @ FB
	return gpu_freq_tar;
}

static int _loading_avg(int ui32loading)
{
	static int data[4];
	static int idx;
	static int sum;

	int cidx = ++idx % ARRAY_SIZE(data);

	sum += ui32loading - data[cidx];
	data[cidx] = ui32loading;

	return sum / ARRAY_SIZE(data);
}

void start_mewtwo_timer(void)
{
	if (hrtimer_try_to_cancel(&gpu_mewtwo_timer)) {
		hrtimer_cancel(&gpu_mewtwo_timer);
		hrtimer_start(&gpu_mewtwo_timer, gpu_mewtwo_timer_expire, HRTIMER_MODE_REL);
	} else
		hrtimer_start(&gpu_mewtwo_timer, gpu_mewtwo_timer_expire, HRTIMER_MODE_REL);

}
void cancel_mewtwo_timer(void)
{
	if (hrtimer_try_to_cancel(&gpu_mewtwo_timer))
		hrtimer_cancel(&gpu_mewtwo_timer);
}

int get_api_sync_flag(void)
{
	return api_sync_flag;
}
void set_api_sync_flag(int flag)
{
	if (flag == 1 || flag == 0) {
		api_sync_flag = flag;
	} else if (flag == 3) {
		dcs_set_fix_num(8);
		start_mewtwo_timer();
	} else if (flag == 2) {
		dcs_set_fix_num(0);
		cancel_mewtwo_timer();
	}
}

static void ged_update_margin_by_fps(int gpu_target)
{
	if (g_tb_dvfs_margin_value_min_cmd)
		return;

	// replace min margin with MIN_TB_DVFS_MARGIN_HIGH_FPS when fps > 60
	if (gpu_target < DVFS_MARGIN_HIGH_FPS_THRESHOLD)
		g_tb_dvfs_margin_value_min = MIN_TB_DVFS_MARGIN_HIGH_FPS;
	// fps <= 60
	else
		g_tb_dvfs_margin_value_min = MIN_TB_DVFS_MARGIN;
}
static void ged_update_window_size_by_fps(int gpu_target)
{
	if (g_loading_slide_window_size_cmd)
		return;

	// replace window_size with default*2 when fps <= 30
	if (gpu_target > SLIDING_WINDOW_SIZE_THRESHOLD)
		g_loading_slide_window_size = GED_DEFAULT_SLIDE_WINDOW_SIZE * 2;
	else
		g_loading_slide_window_size = GED_DEFAULT_SLIDE_WINDOW_SIZE;
}
static bool ged_dvfs_policy(
		unsigned int ui32GPULoading, unsigned int *pui32NewFreqID,
		unsigned long t, long phase, unsigned long ul3DFenceDoneTime,
		bool bRefreshed)
{
	int ui32GPUFreq = ged_get_cur_oppidx();
	unsigned int sentinalLoading = 0;
	unsigned int window_size_ms;
	int i32NewFreqID = (int)ui32GPUFreq;
	int gpu_freq_pre, gpu_freq_overdue_max;	// unit: KHZ

	unsigned long ui32IRQFlags;

	int loading_mode;
	int minfreq_idx;
	int idx_diff = 0;

	if (ui32GPUFreq < 0 || ui32GPUFreq > ged_get_min_oppidx())
		return GED_FALSE;

	gpu_freq_pre = ged_get_cur_freq();
	gpu_freq_overdue_max =
		(ged_get_max_freq_in_opp() * 1000) / OVERDUE_FREQ_TH;

	g_um_gpu_tar_freq = 0;
	if (bRefreshed == false) {
		if (gL_ulCalResetTS_us - g_ulPreDVFS_TS_us != 0) {
			sentinalLoading = ((gpu_loading *
				(gL_ulCalResetTS_us - gL_ulPreCalResetTS_us)) +
				100 * gL_ulWorkingPeriod_us) /
				(gL_ulCalResetTS_us - g_ulPreDVFS_TS_us);

			if (sentinalLoading > 100) {
				ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K1] g_ulCalResetTS_us: %lu g_ulPreDVFS_TS_us: %lu",
					gL_ulCalResetTS_us, g_ulPreDVFS_TS_us);
				ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K1] gpu_loading: %u g_ulPreCalResetTS_us:%lu",
					gpu_loading, gL_ulPreCalResetTS_us);
				ged_log_buf_print(ghLogBuf_DVFS,
		"[GED_K1] g_ulWorkingPeriod_us: %lu",
					gL_ulWorkingPeriod_us);
				ged_log_buf_print(ghLogBuf_DVFS,
						"[GED_K1] gpu_av_loading: WTF");

				if (gL_ulWorkingPeriod_us == 0)
					sentinalLoading = gpu_loading;
				else
					sentinalLoading = 100;
			}
			gpu_loading = sentinalLoading;
		} else {
			ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K1] gpu_av_loading: 5566 / %u", gpu_loading);
			gpu_loading = 0;
		}

		g_ulPreDVFS_TS_us = gL_ulCalResetTS_us;

		gpu_pre_loading = gpu_av_loading;
		ui32GPULoading = gpu_loading;
		gpu_av_loading = gpu_loading;
		atomic_set(&g_gpu_loading_log, gpu_av_loading);
	}

	minfreq_idx = ged_get_min_oppidx();
	if (g_gpu_timer_based_emu) {
		/* legacy timer-based policy */
		if (ui32GPULoading >= 99)
			i32NewFreqID = 0;
		else if (ui32GPULoading <= 1)
			i32NewFreqID = minfreq_idx;
		else if (ui32GPULoading >= 85)
			i32NewFreqID -= 2;
		else if (ui32GPULoading <= 30)
			i32NewFreqID += 2;
		else if (ui32GPULoading >= 70)
			i32NewFreqID -= 1;
		else if (ui32GPULoading <= 50)
			i32NewFreqID += 1;

		if (i32NewFreqID < ui32GPUFreq) {
			if (gpu_pre_loading * 17 / 10 < ui32GPULoading)
				i32NewFreqID -= 1;
		} else if (i32NewFreqID > ui32GPUFreq) {
			if (ui32GPULoading * 17 / 10 < gpu_pre_loading)
				i32NewFreqID += 1;
		}

		g_CommitType = MTK_GPU_DVFS_TYPE_TIMERBASED;
	} else {
		/* new timer-based policy */
		int t_gpu, t_gpu_complete, t_gpu_uncomplete,
			t_gpu_target, t_gpu_target_hd;
		GED_ERROR ret_risky_bq;
		struct ged_risky_bq_info info;
		int uncomplete_flag = 0;
		enum gpu_dvfs_policy_state policy_state;
		int api_sync_flag;
		int fallback_duration_flag;
		int early_force_fallback_flag = 0;

		int ultra_high = 0;
		int high = 0;
		int low = 0;
		int ultra_low = 0;

		/* set t_gpu via risky BQ analysis */
		ged_kpi_update_t_gpu_latest_uncompleted();
		ret_risky_bq = ged_kpi_timer_based_pick_riskyBQ(&info);
		if (ret_risky_bq == GED_OK) {
			t_gpu_complete = (int) info.completed_bq.t_gpu;
			t_gpu_uncomplete = (int) info.uncompleted_bq.t_gpu;

			// pick the largest t_gpu/t_gpu_target & set uncomplete_flag
			t_gpu = t_gpu_uncomplete;
			t_gpu_target = info.uncompleted_bq.t_gpu_target;
			ged_update_margin_by_fps(t_gpu_target);
			if (g_tb_dvfs_margin_mode & DYNAMIC_TB_PERF_MODE_MASK)
				t_gpu_target_hd = t_gpu_target
					* (100 - g_tb_dvfs_margin_value_min) / 100;
			else
				t_gpu_target_hd = t_gpu_target;
			if (t_gpu > t_gpu_target_hd)
				// priority #1: uncompleted frame overtime
				uncomplete_flag = 1;
			else {
				// priority #2: frame with higher t_gpu/t_gpu_target ratio
				unsigned long long risk_completed =
					info.completed_bq.risk;
				unsigned long long risk_uncompleted =
					info.uncompleted_bq.risk;

				if (risk_completed > risk_uncompleted) {
					t_gpu = t_gpu_complete;
					t_gpu_target = info.completed_bq.t_gpu_target;
				}
				uncomplete_flag = 0;
			}
			//if loading is heavy and 120fps ,early force fallback in LOADING_MAX_ITERMCU
			if (early_force_fallback && t_gpu_target <= 8333) {
				early_force_fallback_flag = 1;
				uncomplete_flag = early_force_fallback_flag;
			}
			if (g_max_core_num == SHADER_CORE &&
				gx_dvfs_loading_mode == LOADING_MAX_ITERMCU &&
				early_force_fallback_enable)
				trace_tracing_mark_write(5566, "early_force_fallback"
					, early_force_fallback_flag);
		} else {
			// risky BQ cannot be obtained, set t_gpu to default
			t_gpu = -1;
			t_gpu_complete = -1;
			t_gpu_uncomplete = -1;
			t_gpu_target = -1;
		}
		t_gpu_target_hd = t_gpu_target;   // set init value

		/* update margin */
		if (g_tb_dvfs_margin_mode & DYNAMIC_TB_MASK) {
			/* dynamic margin mode */
			if (ret_risky_bq == GED_OK) {
				static unsigned int prev_gpu_completed_count;
				unsigned int gpu_completed_count =
					info.total_gpu_completed_count;

				ged_update_margin_by_fps(t_gpu_target);
				// overwrite t_gpu_target_hd in perf mode
				if (g_tb_dvfs_margin_mode & DYNAMIC_TB_PERF_MODE_MASK)
					t_gpu_target_hd = t_gpu_target
						* (100 - g_tb_dvfs_margin_value_min) / 100;

				// margin modifying
				if (t_gpu > t_gpu_target_hd) {   // overtime
					/* increase margin flow */
					if (uncomplete_flag)
						// frame still not done, so keep updating margin
						gx_tb_dvfs_margin += g_tb_dvfs_margin_step;
					else if (gpu_completed_count != prev_gpu_completed_count)
						// frame already done, so update once per frame done
						gx_tb_dvfs_margin += g_tb_dvfs_margin_step;
				} else {   // in time
					/* increase margin when t_gpu_uncomplete increase */
					/* else decrease margin */
					if (gpu_freq_pre > gpu_freq_overdue_max &&
						t_gpu_uncomplete > t_gpu_complete &&
						t_gpu_uncomplete > (t_gpu_target_hd / 2)) {
						gx_tb_dvfs_margin += g_tb_dvfs_margin_step;
					} else if (gpu_completed_count != prev_gpu_completed_count)
						gx_tb_dvfs_margin -= g_tb_dvfs_margin_step;
				}

				// margin range clipping
				if (gx_tb_dvfs_margin > g_tb_dvfs_margin_value)
					gx_tb_dvfs_margin = g_tb_dvfs_margin_value;
				if (gx_tb_dvfs_margin < g_tb_dvfs_margin_value_min)
					gx_tb_dvfs_margin = g_tb_dvfs_margin_value_min;

				// update prev_gpu_completed_count for next iteration
				prev_gpu_completed_count = gpu_completed_count;
			} else
				// use fix margin if risky BQ cannot be obtained
				gx_tb_dvfs_margin = g_tb_dvfs_margin_value;
		} else
			/* fix margin mode */
			gx_tb_dvfs_margin = g_tb_dvfs_margin_value;

		// change to fallback mode if frame is overdued (only in LB)
		policy_state = ged_get_policy_state();
		api_sync_flag = get_api_sync_flag();
		if (policy_state == POLICY_STATE_LB ||
				policy_state == POLICY_STATE_LB_FALLBACK) {
			// overwrite state & timeout value set prior to ged_dvfs_run
			if (uncomplete_flag || api_sync_flag == 1 ||
				(gpu_freq_pre > gpu_freq_overdue_max &&
				t_gpu > (t_gpu_target * OVERDUE_TH / 10))) {
				ged_set_policy_state(POLICY_STATE_LB_FALLBACK);
				ged_set_backup_timer_timeout(ged_get_fallback_time());
			} else {
				ged_set_policy_state(POLICY_STATE_LB);
				ged_set_backup_timer_timeout(lb_timeout);
			}
		} else if (policy_state == POLICY_STATE_FORCE_LB ||
				policy_state == POLICY_STATE_FORCE_LB_FALLBACK) {
			// overwrite state & timeout value set prior to ged_dvfs_run
			if (uncomplete_flag || api_sync_flag == 1) {
				ged_set_policy_state(POLICY_STATE_FORCE_LB_FALLBACK);
				ged_set_backup_timer_timeout(ged_get_fallback_time());
			} else {
				ged_set_policy_state(POLICY_STATE_FORCE_LB);
				ged_set_backup_timer_timeout(lb_timeout);
			}
		}

		// limit fallback state duration
		policy_state = ged_get_policy_state();
		if (policy_state == POLICY_STATE_LB_FALLBACK ||
				policy_state == POLICY_STATE_FORCE_LB_FALLBACK ||
				policy_state == POLICY_STATE_FB_FALLBACK)
			fallback_duration += g_fallback_time;   // accumulate
		else
			fallback_duration = 0;   // reset @ LB & FORCE_LB
		if (fallback_duration > MAX_FALLBACK_DURATION)
			fallback_duration_flag = 1;   // fallback exceed maximum duration
		else
			fallback_duration_flag = 0;

		// use fix margin in fallback mode
		policy_state = ged_get_policy_state();
		if (policy_state == POLICY_STATE_FB_FALLBACK ||
				policy_state == POLICY_STATE_LB_FALLBACK ||
				policy_state == POLICY_STATE_FORCE_LB_FALLBACK)
			gx_tb_dvfs_margin = g_tb_dvfs_margin_value;

		// overwrite FB fallback to LB if there're no pending main head frames
		if (policy_state == POLICY_STATE_FB_FALLBACK &&
				ged_kpi_get_main_bq_uncomplete_count() <= 0) {
			ged_set_policy_state(POLICY_STATE_LB);
			g_CommitType = MTK_GPU_DVFS_TYPE_IDLE;
		} else
			g_CommitType = MTK_GPU_DVFS_TYPE_FALLBACK;

		trace_GPU_DVFS__Policy__Loading_based__Margin(
			g_tb_dvfs_margin_value*10, gx_tb_dvfs_margin*10,
			g_tb_dvfs_margin_value_min*10);
		trace_GPU_DVFS__Policy__Loading_based__Margin__Detail(
			g_tb_dvfs_margin_mode, g_tb_dvfs_margin_step,
			g_tb_dvfs_margin_value_min*10);
		trace_GPU_DVFS__Policy__Loading_based__GPU_Time(t_gpu, t_gpu_target,
			t_gpu_target_hd, t_gpu_complete, t_gpu_uncomplete);
		trace_tracing_mark_write(5566, "t_gpu", t_gpu);
		trace_tracing_mark_write(5566, "t_gpu_target", t_gpu_target);

		// set cur freq back to before api boost
		if (g_last_commit_api_flag == 1 && api_sync_flag == 0) {
			// if api boost with dequeue, decrease half freq
			if (g_uncomplete_type == GED_TIMESTAMP_TYPE_D)
				ui32GPUFreq = (ui32GPUFreq + (int)g_last_commit_before_api_boost) / 2;
			else
				ui32GPUFreq = g_last_commit_before_api_boost;

			i32NewFreqID = ui32GPUFreq;
		}

		/* bound update */
		if (init == 0) {
			init = 1;
			gx_tb_dvfs_margin_cur = gx_tb_dvfs_margin;
			_init_loading_ud_table();
		}
		if (gx_tb_dvfs_margin != gx_tb_dvfs_margin_cur
				&& gx_tb_dvfs_margin < 100
				&& gx_tb_dvfs_margin >= 0) {
			gx_tb_dvfs_margin_cur = gx_tb_dvfs_margin;
			_init_loading_ud_table();
		}

		ultra_high = 95;
		high = loading_ud_table[ui32GPUFreq].up;
		low = loading_ud_table[ui32GPUFreq].down;
		ultra_low = 20;

		if (high > ultra_high)
			high = ultra_high;
		if (low < ultra_low)
			low = ultra_low;

		trace_GPU_DVFS__Policy__Loading_based__Bound(ultra_high, high, low,
			ultra_low);

		/* opp control */
		// use fallback window size in fallback mode
		if (policy_state == POLICY_STATE_FB_FALLBACK ||
				policy_state == POLICY_STATE_LB_FALLBACK ||
				policy_state == POLICY_STATE_FORCE_LB_FALLBACK)
			window_size_ms = g_fallback_window_size;
		else {
			ged_update_window_size_by_fps(t_gpu_target);
			window_size_ms = g_loading_slide_window_size;
			// reset if not in fallback mode
			g_fallback_idle = 0;
		}
		ui32GPULoading = gpu_util_history_query_loading(window_size_ms * 1000);
		loading_mode = ged_get_dvfs_loading_mode();

		if (ui32GPULoading >= ultra_high)
			i32NewFreqID -= ged_dvfs_ultra_high_step_size_query();
		else if (ui32GPULoading < ultra_low)
			i32NewFreqID += ged_dvfs_ultra_low_step_size_query();
		else if (ui32GPULoading >= high)
			i32NewFreqID -= ged_dvfs_high_step_size_query();
		else if (ui32GPULoading <= low)
			i32NewFreqID += ged_dvfs_low_step_size_query();

		// prevent decreasing opp in fallback mode
		if ((policy_state == POLICY_STATE_FB_FALLBACK ||
				policy_state == POLICY_STATE_LB_FALLBACK ||
				policy_state == POLICY_STATE_FORCE_LB_FALLBACK) &&
				i32NewFreqID > ui32GPUFreq) {
			bool enable_frequency_adjust = false;
			trace_GPU_DVFS__Policy__Loading_based__Fallback_Tuning(
				g_fallback_tuning, g_fallback_idle, g_uncomplete_type, uncomplete_flag, g_lb_last_opp);
			// if gpu idle during fallback mode and uncomplete time is calculated by timeStampD
			// allow gpu freq to decrease
			if (g_fallback_tuning &&
				g_fallback_idle &&
				g_uncomplete_type == GED_TIMESTAMP_TYPE_D &&
				uncomplete_flag &&
				i32NewFreqID <= g_lb_last_opp &&
				i32NewFreqID < ged_get_opp_num_real())
				enable_frequency_adjust = true;

			if (g_fallback_frequency_adjust && !fallback_duration_flag && !enable_frequency_adjust)
				i32NewFreqID = ui32GPUFreq;

			trace_GPU_DVFS__Policy__Loading_based__Loading(ui32GPULoading, loading_mode,
				enable_frequency_adjust);
		} else
			trace_GPU_DVFS__Policy__Loading_based__Loading(ui32GPULoading, loading_mode, 0);

		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K][LB_DVFS] mode:0x%x, u_b:%d, l_b:%d, margin:%d, complete:%d, uncomplete:%d, t_gpu:%d, target:%d",
			g_tb_dvfs_margin_mode,
			g_tb_dvfs_margin_value,
			g_tb_dvfs_margin_value_min,
			gx_tb_dvfs_margin_cur,
			t_gpu_complete,
			t_gpu_uncomplete,
			t_gpu,
			t_gpu_target);
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K1] rdy gpu_av_loading:%u, %d(%d)-up:%d,%d, new: %d, step: 0x%x",
			ui32GPULoading,
			ui32GPUFreq,
			loading_ud_table[ui32GPUFreq].freq,
			loading_ud_table[ui32GPUFreq].up,
			loading_ud_table[ui32GPUFreq].down,
			i32NewFreqID,
			dvfs_step_mode);

		/* Back to frame base when uncomplete time is smller than fb_timeout */
		if (g_ged_frame_base_optimize &&
				ged_get_policy_state() == POLICY_STATE_FB_FALLBACK &&
				((u64)t_gpu_uncomplete * 1000) < fb_timeout) {
			u64 fb_tmp_timer = fb_timeout - ((u64)t_gpu_uncomplete * 1000);
			// consider workqueue latency
			if (fb_tmp_timer > TIMER_LATENCY)
				fb_tmp_timer -= TIMER_LATENCY;

			u64 timeout_val = ged_get_fallback_time();

			if (fb_tmp_timer > timeout_val) {
				ged_set_policy_state(POLICY_STATE_FB);
				ged_set_backup_timer_timeout(fb_tmp_timer);
				g_CommitType = MTK_GPU_DVFS_TYPE_SKIPFALLBACK;
			}
		}
	}

	if (i32NewFreqID > minfreq_idx)
		i32NewFreqID = minfreq_idx;
	else if (i32NewFreqID < 0)
		i32NewFreqID = 0;

	trace_GPU_DVFS__Policy__Loading_based__Opp(i32NewFreqID);
	*pui32NewFreqID = (unsigned int)i32NewFreqID;
	g_policy_tar_freq = ged_get_freq_by_idx(i32NewFreqID);
	if (ged_get_policy_state() == POLICY_STATE_LB)
		g_lb_last_opp = i32NewFreqID;

	g_mode = 2;

	return (*pui32NewFreqID != ui32GPUFreq) ? GED_TRUE : GED_FALSE;
}

#ifdef ENABLE_COMMON_DVFS
static void ged_dvfs_freq_input_boostCB(unsigned int ui32BoostFreqID)
{
	if (g_iSkipCount > 0)
		return;

	if (boost_gpu_enable == 0)
		return;

	mutex_lock(&gsDVFSLock);

	if (ui32BoostFreqID < ged_get_cur_oppidx()) {
		if (ged_dvfs_gpu_freq_commit(ui32BoostFreqID,
				ged_get_freq_by_idx(ui32BoostFreqID),
				GED_DVFS_INPUT_BOOST_COMMIT))
			g_dvfs_skip_round = GED_DVFS_SKIP_ROUNDS;
	}
	mutex_unlock(&gsDVFSLock);
}

void ged_dvfs_boost_gpu_freq(void)
{
	if (gpu_debug_enable)
		GED_LOGD("@%s", __func__);

	ged_dvfs_freq_input_boostCB(0);
}

/* Set buttom gpufreq by from PowerHal  API Boost */
static void ged_dvfs_set_bottom_gpu_freq(unsigned int ui32FreqLevel)
{
	int minfreq_idx;
	static unsigned int s_bottom_freq_id;

	minfreq_idx = ged_get_min_oppidx();

	if (gpu_debug_enable)
		GED_LOGD("@%s: freq = %d", __func__, ui32FreqLevel);

	if (minfreq_idx < ui32FreqLevel)
		ui32FreqLevel = minfreq_idx;

	mutex_lock(&gsDVFSLock);

	/* 0 => The highest frequency */
	/* table_num - 1 => The lowest frequency */
	s_bottom_freq_id = minfreq_idx - ui32FreqLevel;

	ged_set_limit_floor(LIMITER_APIBOOST, s_bottom_freq_id);

	gpu_bottom_freq = ged_get_freq_by_idx(s_bottom_freq_id);
	if (g_bottom_freq_id < s_bottom_freq_id) {
		g_bottom_freq_id = s_bottom_freq_id;
		if (s_bottom_freq_id < g_last_def_commit_freq_id)
			ged_dvfs_gpu_freq_commit(s_bottom_freq_id,
			gpu_bottom_freq,
			GED_DVFS_SET_BOTTOM_COMMIT);
		else
			ged_dvfs_gpu_freq_commit(g_last_def_commit_freq_id,
			ged_get_freq_by_idx(g_last_def_commit_freq_id),
			GED_DVFS_SET_BOTTOM_COMMIT);
	} else {
	/* if current id is larger, ie lower freq, reflect immedately */
		g_bottom_freq_id = s_bottom_freq_id;
		if (s_bottom_freq_id < ged_get_cur_oppidx())
			ged_dvfs_gpu_freq_commit(s_bottom_freq_id,
			gpu_bottom_freq,
			GED_DVFS_SET_BOTTOM_COMMIT);
	}

	mutex_unlock(&gsDVFSLock);
}

static unsigned int ged_dvfs_get_gpu_freq_level_count(void)
{
	return ged_get_opp_num_real();
}

/* set buttom gpufreq from PowerHal by MIN_FREQ */
static void ged_dvfs_custom_boost_gpu_freq(unsigned int ui32FreqLevel)
{
	int minfreq_idx;

	minfreq_idx = ged_get_min_oppidx_real();

	if (gpu_debug_enable)
		GED_LOGD("@%s: freq = %d", __func__, ui32FreqLevel);

	if (minfreq_idx < ui32FreqLevel)
		ui32FreqLevel = minfreq_idx;

	mutex_lock(&gsDVFSLock);

	/* 0 => The highest frequency */
	/* table_num - 1 => The lowest frequency */
	g_cust_boost_freq_id = ui32FreqLevel;

	ged_set_limit_floor(LIMITER_FPSGO, g_cust_boost_freq_id);

	gpu_cust_boost_freq = ged_get_freq_by_idx(g_cust_boost_freq_id);
	if (g_cust_boost_freq_id < ged_get_cur_oppidx()) {
		ged_dvfs_gpu_freq_commit(g_cust_boost_freq_id,
			gpu_cust_boost_freq, GED_DVFS_CUSTOM_BOOST_COMMIT);
	}

	mutex_unlock(&gsDVFSLock);
}

/* set buttom gpufreq from PowerHal by MAX_FREQ */
static void ged_dvfs_custom_ceiling_gpu_freq(unsigned int ui32FreqLevel)
{
	int minfreq_idx;

	minfreq_idx = ged_get_min_oppidx_real();

	if (gpu_debug_enable)
		GED_LOGD("@%s: freq = %d", __func__, ui32FreqLevel);

	if (minfreq_idx < ui32FreqLevel)
		ui32FreqLevel = minfreq_idx;

	mutex_lock(&gsDVFSLock);

	/* 0 => The highest frequency */
	/* table_num - 1 => The lowest frequency */
	g_cust_upbound_freq_id = ui32FreqLevel;

	ged_set_limit_ceil(LIMITER_FPSGO, g_cust_upbound_freq_id);

	gpu_cust_upbound_freq = ged_get_freq_by_idx(g_cust_upbound_freq_id);
	if (g_cust_upbound_freq_id > ged_get_cur_oppidx())
		ged_dvfs_gpu_freq_commit(g_cust_upbound_freq_id,
		gpu_cust_upbound_freq, GED_DVFS_CUSTOM_CEIL_COMMIT);

	mutex_unlock(&gsDVFSLock);
}

static unsigned long ged_dvfs_get_gpu_custom_upbound_freq(void)
{
	return ged_get_freq_by_idx(g_cust_upbound_freq_id);
}

static unsigned long ged_dvfs_get_gpu_custom_boost_freq(void)
{
	return ged_get_freq_by_idx(g_cust_boost_freq_id);
}

static unsigned int ged_dvfs_get_bottom_gpu_freq(void)
{
	unsigned int ui32MaxLevel;

	ui32MaxLevel = ged_get_min_oppidx();

	return ui32MaxLevel - g_bottom_freq_id;
}

static unsigned long ged_get_gpu_bottom_freq(void)
{
	return ged_get_freq_by_idx(g_bottom_freq_id);
}
#endif /* ENABLE_COMMON_DVFS */

unsigned int ged_dvfs_get_custom_ceiling_gpu_freq(void)
{
	return g_cust_upbound_freq_id;
}

unsigned int ged_dvfs_get_custom_boost_gpu_freq(void)
{
	return g_cust_boost_freq_id;
}

static void ged_dvfs_margin_value(int i32MarginValue)
{
	/* -1:  default: configure margin mode */
	/* -2:  variable margin mode by opp index */
	/* 0~100: configure margin mode */
	/* 101~199:  dynamic margin mode - CONFIG_FPS_MARGIN */
	/* 201~299:  dynamic margin mode - FIXED_FPS_MARGIN */
	/* 301~399:  dynamic margin mode - NO_FPS_MARGIN */
	/* 401~499:  dynamic margin mode - EXTEND MODE */
	/* 501~599:  dynamic margin mode - PERF MODE */

	/****************************************************/
	/* bit0~bit9: dvfs_margin_value setting             */
	/*                                                  */
	/* EXTEND MODE (401~499):                           */
	/* PERF MODE (501~599):                             */
	/* bit10~bit15: min. inc step (0~63 step)           */
	/* bit16~bit22: (0~100%)dynamic headroom low bound  */
	/****************************************************/

	int i32MarginValue_ori;

	mutex_lock(&gsDVFSLock);

	g_fastdvfs_margin = 0;

	if (i32MarginValue == -1) {
		dvfs_margin_mode = CONFIGURE_MARGIN_MODE;
		dvfs_margin_value = DEFAULT_DVFS_MARGIN;
		mutex_unlock(&gsDVFSLock);
		return;
	} else if (i32MarginValue == -2) {
		dvfs_margin_mode = VARIABLE_MARGIN_MODE_OPP_INDEX;
		mutex_unlock(&gsDVFSLock);
		return;
	} else if (i32MarginValue == 999) {
		g_fastdvfs_margin = 1;
		mutex_unlock(&gsDVFSLock);
		return;
	}

	i32MarginValue_ori = i32MarginValue;
	i32MarginValue = i32MarginValue & 0x3ff;

	if ((i32MarginValue >= 0) && (i32MarginValue <= 100))
		dvfs_margin_mode = CONFIGURE_MARGIN_MODE;
	else if ((i32MarginValue > 100) && (i32MarginValue < 200)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_CONFIG_FPS_MARGIN;
		i32MarginValue = i32MarginValue - 100;
	} else if ((i32MarginValue > 200) && (i32MarginValue < 300)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN;
		i32MarginValue = i32MarginValue - 200;
	} else if ((i32MarginValue > 300) && (i32MarginValue < 400)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN;
		i32MarginValue = i32MarginValue - 300;
	} else if ((i32MarginValue > 400) && (i32MarginValue < 500)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_EXTEND;
		i32MarginValue = i32MarginValue - 400;
	} else if ((i32MarginValue > 500) && (i32MarginValue < 600)) {
		dvfs_margin_mode = DYNAMIC_MARGIN_MODE_PERF;
		i32MarginValue = i32MarginValue - 500;
	}
	// unit: % to 10*%
	dvfs_margin_value = i32MarginValue * 10;
	dvfs_min_margin_inc_step = ((i32MarginValue_ori & 0xfc00) >> 10) * 10;
	dvfs_margin_low_bound = ((i32MarginValue_ori & 0x7f0000) >> 16) * 10;

	// range clipping
	if (dvfs_margin_value > MAX_DVFS_MARGIN)
		dvfs_margin_value = MAX_DVFS_MARGIN;
	if (dvfs_margin_value < MIN_DVFS_MARGIN)
		dvfs_margin_value = MIN_DVFS_MARGIN;
	if (dvfs_margin_low_bound > MAX_DVFS_MARGIN)
		dvfs_margin_low_bound = MAX_DVFS_MARGIN;
	if (dvfs_margin_low_bound < MIN_DVFS_MARGIN)
		dvfs_margin_low_bound = MIN_DVFS_MARGIN;
	if (dvfs_min_margin_inc_step < MIN_MARGIN_INC_STEP)
		dvfs_min_margin_inc_step = MIN_MARGIN_INC_STEP;

	mutex_unlock(&gsDVFSLock);
}

static int ged_get_dvfs_margin_value(void)
{
	int ret = 0;

	if (dvfs_margin_mode == CONFIGURE_MARGIN_MODE)
		ret = dvfs_margin_value/10;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_CONFIG_FPS_MARGIN)
		ret = dvfs_margin_value/10 + 100;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_FIXED_FPS_MARGIN)
		ret = dvfs_margin_value/10 + 200;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_NO_FPS_MARGIN)
		ret = dvfs_margin_value/10 + 300;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_EXTEND)
		ret = dvfs_margin_value/10 + 400;
	else if (dvfs_margin_mode == DYNAMIC_MARGIN_MODE_PERF)
		ret = dvfs_margin_value/10 + 500;
	else if (dvfs_margin_mode == VARIABLE_MARGIN_MODE_OPP_INDEX)
		ret = -2;

	if (g_fastdvfs_margin)
		ret = 999;

	return ret;
}

int ged_get_dvfs_margin(void)
{
	return gx_fb_dvfs_margin;
}

unsigned int ged_get_dvfs_margin_mode(void)
{
	return dvfs_margin_mode;
}

static void ged_loading_base_dvfs_step(int i32StepValue)
{
	/* bit0~bit7: ultra high step */
	/* bit8~bit15: ultra low step */

	mutex_lock(&gsDVFSLock);

	dvfs_step_mode = i32StepValue;

	mutex_unlock(&gsDVFSLock);
}

static int ged_get_loading_base_dvfs_step(void)
{
	return dvfs_step_mode;
}

static void ged_timer_base_dvfs_margin(int i32MarginValue)
{
	/*
	 * value < 0: default, GED_DVFS_TIMER_BASED_DVFS_MARGIN
	 * bit   7~0: margin value
	 * bit     8: dynamic timer based dvfs margin
	 * bit     9: use gpu pipe time for dynamic timer based dvfs margin
	 * bit    10: use performance mode for dynamic timer based dvfs margin
	 * bit    11: fix target FPS to 30 for dynamic timer based dvfs margin
	 * bit 23~16: min margin value
	 * bit 27~24: margin update step
	 */
	unsigned int mode = CONFIGURE_TIMER_BASED_MODE;
	int value = i32MarginValue & TIMER_BASED_MARGIN_MASK;
	int value_min = (i32MarginValue >> 16) & TIMER_BASED_MARGIN_MASK;
	int margin_step = (i32MarginValue >> 24) & 0x0000000F;

	if (i32MarginValue < 0)
		value = GED_DVFS_TIMER_BASED_DVFS_MARGIN;
	else {
		mode = (i32MarginValue & DYNAMIC_TB_MASK) ?
				(mode | DYNAMIC_TB_MASK) : mode;
		mode = (i32MarginValue & DYNAMIC_TB_PIPE_TIME_MASK) ?
				(mode | DYNAMIC_TB_PIPE_TIME_MASK) : mode;
		mode = (i32MarginValue & DYNAMIC_TB_PERF_MODE_MASK) ?
				(mode | DYNAMIC_TB_PERF_MODE_MASK) : mode;
		mode = (i32MarginValue & DYNAMIC_TB_FIX_TARGET_MASK) ?
				(mode | DYNAMIC_TB_FIX_TARGET_MASK) : mode;
	}

	mutex_lock(&gsDVFSLock);

	g_tb_dvfs_margin_mode = mode;

	if (value > MAX_TB_DVFS_MARGIN)
		g_tb_dvfs_margin_value = MAX_TB_DVFS_MARGIN;
	else if (value < MIN_TB_DVFS_MARGIN)
		g_tb_dvfs_margin_value = MIN_TB_DVFS_MARGIN;
	else
		g_tb_dvfs_margin_value = value;

	if (value_min > MAX_TB_DVFS_MARGIN)
		g_tb_dvfs_margin_value_min_cmd = MAX_TB_DVFS_MARGIN;
	else if (value_min < MIN_TB_DVFS_MARGIN)
		g_tb_dvfs_margin_value_min_cmd = MIN_TB_DVFS_MARGIN;
	else
		g_tb_dvfs_margin_value_min_cmd = value_min;
	g_tb_dvfs_margin_value_min = g_tb_dvfs_margin_value_min_cmd;

	if (margin_step == 0)
		g_tb_dvfs_margin_step = margin_step;
	else
		g_tb_dvfs_margin_step = GED_DVFS_TIMER_BASED_DVFS_MARGIN_STEP;

	mutex_unlock(&gsDVFSLock);
}

static int ged_get_timer_base_dvfs_margin(void)
{
	return g_tb_dvfs_margin_value;
}

int ged_dvfs_get_tb_dvfs_margin_cur(void)
{
	return gx_tb_dvfs_margin_cur;
}

unsigned int ged_dvfs_get_tb_dvfs_margin_mode(void)
{
	return g_tb_dvfs_margin_mode;
}

static void ged_dvfs_loading_mode(int i32MarginValue)
{
	/* -1: default: */
	/* 0: default (Active)/(Active+Idle)*/
	/* 1: max(TA,3D)+COMPUTE/(Active+Idle) */
	/* 2: max(TA,3D)/(Active+Idle) */

	mutex_lock(&gsDVFSLock);

	if (i32MarginValue == -1)
		gx_dvfs_loading_mode = LOADING_ACTIVE;
	else if ((i32MarginValue >= 0) && (i32MarginValue < 100))
		gx_dvfs_loading_mode = i32MarginValue;

	mutex_unlock(&gsDVFSLock);
}

static int ged_get_dvfs_loading_mode(void)
{
	return gx_dvfs_loading_mode;
}

static void ged_dvfs_workload_mode(int i32WorkloadMode)
{
	/* -1: default (active) */
	/* 0: active */
	/* 1: 3d */

	mutex_lock(&gsDVFSLock);

	if (i32WorkloadMode == -1)
		gx_dvfs_workload_mode = WORKLOAD_ACTIVE;
	else if ((i32WorkloadMode >= 0) && (i32WorkloadMode < 100))
		gx_dvfs_workload_mode = i32WorkloadMode;

	mutex_unlock(&gsDVFSLock);
}

static int ged_get_dvfs_workload_mode(void)
{
	return gx_dvfs_workload_mode;
}

void ged_get_gpu_utli_ex(struct GpuUtilization_Ex *util_ex)
{
	memcpy((void *)util_ex, (void *)&g_Util_Ex,
		sizeof(struct GpuUtilization_Ex));
}

static void ged_set_fastdvfs_mode(unsigned int u32ModeValue)
{
	mutex_lock(&gsDVFSLock);
	g_fastdvfs_mode = u32ModeValue;
	mtk_gpueb_dvfs_set_mode(g_fastdvfs_mode);
	mutex_unlock(&gsDVFSLock);
}

static unsigned int ged_get_fastdvfs_mode(void)
{
	mtk_gpueb_dvfs_get_mode(&g_fastdvfs_mode);

	return g_fastdvfs_mode;
}

/* Need spinlocked */
void ged_dvfs_save_loading_page(void)
{
	gL_ulCalResetTS_us = g_ulCalResetTS_us;
	gL_ulPreCalResetTS_us = g_ulPreCalResetTS_us;
	gL_ulWorkingPeriod_us = g_ulWorkingPeriod_us;

	/* set as zero for next time */
	g_ulWorkingPeriod_us = 0;
}

void ged_dvfs_run(
	unsigned long t, long phase, unsigned long ul3DFenceDoneTime,
	GED_DVFS_COMMIT_TYPE eCommitType)
{
	unsigned long ui32IRQFlags;

	mutex_lock(&gsDVFSLock);

	if (gpu_dvfs_enable == 0) {
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K][LB_DVFS] skip %s due to gpu_dvfs_enable=%u",
			__func__, gpu_dvfs_enable);
		goto EXIT_ged_dvfs_run;
	}

	/* SKIP for keeping boost freq */
	if (g_dvfs_skip_round > 0)
		g_dvfs_skip_round--;

	if (g_iSkipCount > 0) {
		gpu_power = 0;
		gpu_loading = 0;
		gpu_block = 0;
		gpu_idle = 0;
		g_iSkipCount -= 1;
	} else {
		spin_lock_irqsave(&gsGpuUtilLock, ui32IRQFlags);

		if (is_fb_dvfs_triggered) {
			spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);
			goto EXIT_ged_dvfs_run;
		}

		is_fallback_mode_triggered = 1;

		ged_dvfs_cal_gpu_utilization_ex(&gpu_loading,
			&gpu_block, &gpu_idle, &g_Util_Ex);

		ged_log_buf_print(ghLogBuf_DVFS,
			"[GED_K][FB_DVFS] fallback mode");
		spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);

		spin_lock_irqsave(&g_sSpinLock, ui32IRQFlags);
		g_ulPreCalResetTS_us = g_ulCalResetTS_us;
		g_ulCalResetTS_us = t;

		ged_dvfs_save_loading_page();

		spin_unlock_irqrestore(&g_sSpinLock, ui32IRQFlags);

#ifdef GED_DVFS_UM_CAL
		if (phase == GED_DVFS_TIMER_BACKUP)
#endif /* GED_DVFS_UM_CAL */
		{
			bool freq_change_flag;
			enum gpu_dvfs_policy_state policy_state, prev_policy_state;

			// perform LB algorithm
			freq_change_flag = ged_dvfs_policy(gpu_loading,
				&g_ui32FreqIDFromPolicy, t, phase, ul3DFenceDoneTime, false);
			policy_state = ged_get_policy_state();
			prev_policy_state = ged_get_prev_policy_state();

			/* set loading-based new timeout = target_time(set 4,1) */
			if (is_fdvfs_enable())
				ged_kpi_set_loading_mode(LB_TIMEOUT_TYPE_REDUCE_MIPS,
									LB_TIMEOUT_REDUCE_MIPS);
			// commit new frequency
			if (freq_change_flag || policy_state != prev_policy_state) {
				// correct eCommitType in case fallback is triggered in LB
				if (policy_state == POLICY_STATE_LB ||
						policy_state == POLICY_STATE_FORCE_LB)
					eCommitType = GED_DVFS_LOADING_BASE_COMMIT;
				else if (policy_state == POLICY_STATE_FB &&
						g_CommitType != MTK_GPU_DVFS_TYPE_SKIPFALLBACK)
					eCommitType = GED_DVFS_FRAME_BASE_COMMIT;
				else
					eCommitType = GED_DVFS_FALLBACK_COMMIT;

				if (!is_fdvfs_enable()) {
					if (g_async_ratio_support)
						ged_dvfs_gpu_freq_dual_commit(
							g_ui32FreqIDFromPolicy,
							ged_get_freq_by_idx(g_ui32FreqIDFromPolicy),
							eCommitType, g_ui32FreqIDFromPolicy);
					else
						ged_dvfs_gpu_freq_commit(g_ui32FreqIDFromPolicy,
							ged_get_freq_by_idx(g_ui32FreqIDFromPolicy),
							eCommitType);
				}
			}
		}
	}
	if (gpu_debug_enable) {
		GED_LOGE("%s:gpu_loading=%d %d, g_iSkipCount=%d", __func__,
			gpu_loading, ged_get_cur_oppidx(),
			g_iSkipCount);
	}

EXIT_ged_dvfs_run:
	mutex_unlock(&gsDVFSLock);
}

void ged_dvfs_sw_vsync_query_data(struct GED_DVFS_UM_QUERY_PACK *psQueryData)
{
	psQueryData->ui32GPULoading = gpu_loading;

	psQueryData->ui32GPUFreqID = ged_get_cur_oppidx();
	psQueryData->gpu_cur_freq =
		ged_get_freq_by_idx(psQueryData->ui32GPUFreqID);
	psQueryData->gpu_pre_freq = ged_get_freq_by_idx(g_ui32PreFreqID);

	psQueryData->nsOffset = ged_dvfs_vsync_offset_level_get();

	psQueryData->ulWorkingPeriod_us = gL_ulWorkingPeriod_us;
	psQueryData->ulPreCalResetTS_us = gL_ulPreCalResetTS_us;

	psQueryData->ui32TargetPeriod_us = g_ui32TargetPeriod_us;
	psQueryData->ui32BoostValue = g_ui32BoostValue;
}

void ged_dvfs_track_latest_record(enum MTK_GPU_DVFS_TYPE *peType,
	unsigned long *pulFreq)
{
	*peType = g_CommitType;
	*pulFreq = g_ulCommitFreq;
}

unsigned long ged_dvfs_get_gpu_commit_freq(void)
{
	return ged_commit_freq;
}

unsigned long ged_dvfs_get_gpu_commit_opp_freq(void)
{
	return ged_commit_opp_freq;
}

unsigned int ged_dvfs_get_gpu_loading(void)
{
	unsigned int loading = 0;

	loading = (unsigned int)atomic_read(&g_gpu_loading_log);

	return loading;
}

unsigned int ged_dvfs_get_gpu_loading_avg(void)
{
	unsigned int loading = 0;

	loading = (unsigned int)atomic_read(&g_gpu_loading_avg_log);

	if (g_curr_pwr_state != GED_POWER_ON)
		loading = 0;

	return loading;
}

unsigned int ged_dvfs_get_gpu_blocking(void)
{
	return gpu_block;
}

unsigned int ged_dvfs_get_gpu_idle(void)
{
	return 100 - gpu_av_loading;
}

void ged_dvfs_get_gpu_cur_freq(struct GED_DVFS_FREQ_DATA *psData)
{
	psData->ui32Idx = ged_get_cur_oppidx();
	psData->ulFreq = ged_get_freq_by_idx(psData->ui32Idx);
}

void ged_dvfs_get_gpu_pre_freq(struct GED_DVFS_FREQ_DATA *psData)
{
	psData->ui32Idx = g_ui32PreFreqID;
	psData->ulFreq = ged_get_freq_by_idx(g_ui32PreFreqID);
}

void ged_get_gpu_dvfs_cal_freq(unsigned long *p_policy_tar_freq, int *pmode)
{
	*p_policy_tar_freq = g_policy_tar_freq;
	*pmode = g_mode;
}

GED_ERROR ged_dvfs_probe_signal(int signo)
{
	int cache_pid = GED_NO_UM_SERVICE;
	struct task_struct *t = NULL;
	struct siginfo info;

	info.si_signo = signo;
	info.si_code = SI_QUEUE;
	info.si_int = 1234;

	if (cache_pid != g_probe_pid) {
		cache_pid = g_probe_pid;
		if (g_probe_pid == GED_NO_UM_SERVICE)
			t = NULL;
		else {
			rcu_read_lock();
			t = pid_task(find_vpid(g_probe_pid), PIDTYPE_PID);
			rcu_read_unlock();
		}
	}

	if (t != NULL) {
		/* TODO: porting*/
		/* send_sig_info(signo, &info, t); */
		return GED_OK;
	} else {
		g_probe_pid = GED_NO_UM_SERVICE;
		return GED_ERROR_INVALID_PARAMS;
	}
}

void set_target_fps(int i32FPS)
{
	g_ulvsync_period = get_ns_period_from_fps(i32FPS);
}

void ged_dvfs_reset_opp_cost(int oppsize)
{
	int i = 0;

	if (ged_kpi_enabled() && g_aOppStat != NULL &&
		oppsize > 0 && oppsize <= g_real_oppfreq_num) {
		for (i = 0; i < oppsize; i++) {
			g_aOppStat[i].ui64Active = 0;
			g_aOppStat[i].ui64Idle = 0;
			memset(g_aOppStat[i].uMem.aTrans, 0, sizeof(uint32_t) * oppsize);
		}
	}
}

int ged_dvfs_query_opp_cost(struct GED_DVFS_OPP_STAT *psReport,
		int i32NumOpp, bool bStript, u64 *last_ts)
{
	int i = 0;
	//u32 cur_opp_idx = 0, gpu_loading = 0, ui32IRQFlags;
	u64 pwr_on_ts = g_ns_gpu_on_ts / 1000000;

	if (ged_kpi_enabled() && g_aOppStat != NULL && psReport &&
		i32NumOpp > 0 && i32NumOpp <= g_real_oppfreq_num &&
		gpu_opp_logs_enable == 1) {
		//Update opp cost timely before query
		//spin_lock_irqsave(&gsGpuUtilLock, ui32IRQFlags);

		//mtk_get_gpu_loading(&gpu_loading);
		//gpu_loading = (gpu_loading > 100)? 100: gpu_loading;
		//cur_opp_idx = gpufreq_get_cur_oppidx(TARGET_DEFAULT);
		//ged_dvfs_update_opp_cost(gpu_loading, cur_opp_idx);
		//*last_ts = (g_last_opp_cost_cal_ts > pwr_on_ts)? g_last_opp_cost_cal_ts: pwr_on_ts;
		*last_ts = g_last_opp_cost_update_ts_ms;

		memcpy(psReport, g_aOppStat,
				i32NumOpp * sizeof(struct GED_DVFS_OPP_STAT));

		//spin_unlock_irqrestore(&gsGpuUtilLock, ui32IRQFlags);

		if (bStript)
			for (i = 0; i < i32NumOpp; i++)
				psReport[i].uMem.ui32Freq = ged_get_freq_by_idx(i);

		return 0;
	}

	return -1;
}
EXPORT_SYMBOL(ged_dvfs_query_opp_cost);

static void ged_dvfs_deinit_opp_cost(void)
{
	int i = 0, oppsize = g_real_oppfreq_num;

	if (g_aOppStat == NULL)
		return;

	for (i = 0; i < oppsize; i++)
		vfree(g_aOppStat[i].uMem.aTrans);

	vfree(g_aOppStat);
}

int ged_dvfs_init_opp_cost(void)
{
	int i = 0, oppsize = gpufreq_get_opp_num(TARGET_DEFAULT);
	int min_opp_idx = 0;
	unsigned int min_gpu_freq = 0;

	if (!ged_kpi_enabled())
		return 0;

	if (oppsize <= 0)
		return -EPROBE_DEFER;

	min_opp_idx = oppsize - 1;
	min_gpu_freq = gpufreq_get_freq_by_idx(TARGET_DEFAULT, min_opp_idx);
	for (i = min_opp_idx - 1; i >= 0; i--) {
		if (gpufreq_get_freq_by_idx(TARGET_DEFAULT, i) != min_gpu_freq)
			break;

		oppsize--;
	}

	g_real_oppfreq_num = oppsize;
	g_real_minfreq_idx = g_real_oppfreq_num - 1;

	if (g_aOppStat == NULL)
		g_aOppStat = vmalloc(sizeof(struct GED_DVFS_OPP_STAT) * oppsize);
	else
		ged_dvfs_deinit_opp_cost();

	if (g_aOppStat != NULL) {
		for (i = 0; i < oppsize; i++)
			g_aOppStat[i].uMem.aTrans = vmalloc(sizeof(uint32_t) * oppsize);

		ged_dvfs_reset_opp_cost(oppsize);
	} else
		GED_LOGE("init opp cost failed!");

	return 0;
}

int ged_dvfs_get_real_oppfreq_num(void)
{
	return g_real_oppfreq_num;
}
EXPORT_SYMBOL(ged_dvfs_get_real_oppfreq_num);

int ged_dvfs_query_loading(u64 *sum_loading, u64 *sum_delta_time)
{
	if (sum_loading == NULL || sum_delta_time == NULL) {
		WARN(1, "Invalid parameters");
		return -EINVAL;
	}

	*sum_loading = g_sum_loading;
	*sum_delta_time = g_sum_delta_time;

	return 0;
}
EXPORT_SYMBOL(ged_dvfs_query_loading);

GED_ERROR ged_dvfs_probe(int pid)
{
	if (pid == GED_VSYNC_OFFSET_NOT_SYNC) {
		g_ui32EventDebugStatus |= GED_EVENT_NOT_SYNC;
		return GED_OK;
	}

	if (pid == GED_VSYNC_OFFSET_SYNC)	{
		g_ui32EventDebugStatus &= (~GED_EVENT_NOT_SYNC);
		return GED_OK;
	}

	g_probe_pid = pid;

	/* clear bits among start */
	if (g_probe_pid != GED_NO_UM_SERVICE) {
		g_ui32EventStatus &= (~GED_EVENT_TOUCH);
		g_ui32EventStatus &= (~GED_EVENT_WFD);
		g_ui32EventStatus &= (~GED_EVENT_GAS);

		g_ui32EventDebugStatus = 0;
	}

	return GED_OK;
}

void ged_dvfs_enable_async_ratio(int enableAsync)
{
	GED_LOGI("[DVFS_ASYNC] %s async ratio in ged_dvfs",
			enableAsync ? "enable" : "disable");
	g_async_ratio_support = enableAsync;
}

void ged_dvfs_force_top_oppidx(int idx)
{
	GED_LOGI("[DVFS_ASYNC] force top oppidx (%d) in ged_dvfs", idx);
	FORCE_TOP_OPP = idx;
}

void ged_dvfs_force_stack_oppidx(int idx)
{
	GED_LOGI("[DVFS_ASYNC] force stack oppidx (%d) in ged_dvfs", idx);
	FORCE_OPP = idx;
}

void ged_dvfs_set_async_log_level(unsigned int level)
{
	GED_LOGI("[DVFS_ASYNC] set async log level (%d) in ged_dvfs", level);
	g_async_log_level = level;
}

int ged_dvfs_get_async_ratio_support(void)
{
	return g_async_ratio_support;
}

int ged_dvfs_get_top_oppidx(void)
{
	return FORCE_TOP_OPP;
}

int ged_dvfs_get_stack_oppidx(void)
{
	return FORCE_OPP;
}

unsigned int ged_dvfs_get_async_log_level(void)
{
	return g_async_log_level;
}

void ged_dvfs_set_slide_window_size(int size)
{
	g_loading_slide_window_size_cmd = size;
	g_loading_slide_window_size = size;
}

void ged_dvfs_set_uncomplete_ts_type(int type)
{
	g_uncomplete_type = type;
}

void ged_dvfs_set_fallback_tuning(int tuning)
{
	g_fallback_tuning = tuning;
}

int ged_dvfs_get_fallback_tuning(void)
{
	return g_fallback_tuning;
}

void ged_dvfs_notify_power_off(void)
{
	if (g_last_commit_type == GED_DVFS_FALLBACK_COMMIT)
		g_fallback_idle++;
}

int ged_dvfs_get_recude_mips_policy_state(void)
{
	static unsigned int init_flag;

	/* init setting, default eb_pllicy mode = 1 */
	if (init_flag == 0) {
		g_fastdvfs_mode = 1;
		init_flag = 1;
	}

	return g_fastdvfs_mode;
}

static enum hrtimer_restart gpu_mewtwo_timer_cb(struct hrtimer *timer)
{
	dcs_fix_reset();
	return HRTIMER_NORESTART;
}

GED_ERROR ged_dvfs_system_init(void)
{
	struct device_node *async_dvfs_node = NULL;
	struct device_node *reduce_mips_dvfs_node = NULL;
	struct device_node *dvfs_loading_mode_node = NULL;
	struct device_node *gpu_mewtwo_node = NULL;

	mutex_init(&gsDVFSLock);
	mutex_init(&gsPolicyLock);
	mutex_init(&gsVSyncOffsetLock);

	spin_lock_init(&gsGpuUtilLock);

	/* initial as locked, signal when vsync_sw_notify */
#ifdef ENABLE_COMMON_DVFS
	gpu_dvfs_enable = 1;

	gpu_util_history_init();

	g_iSkipCount = MTK_DEFER_DVFS_WORK_MS / MTK_DVFS_SWITCH_INTERVAL_MS;

	g_ulvsync_period = get_ns_period_from_fps(60);
	g_max_core_num = gpufreq_get_core_num();

	ged_kpi_gpu_dvfs_fp = ged_dvfs_fb_gpu_dvfs;
	ged_kpi_trigger_fb_dvfs_fp = ged_dvfs_trigger_fb_dvfs;
	ged_kpi_check_if_fallback_mode_fp = ged_dvfs_is_fallback_mode_triggered;

	g_dvfs_skip_round = 0;

	g_minfreq_idx = ged_get_min_oppidx();
	g_maxfreq_idx = 0;
	g_minfreq = ged_get_freq_by_idx(g_minfreq_idx);
	g_maxfreq = ged_get_freq_by_idx(g_maxfreq_idx);

	g_bottom_freq_id = ged_get_min_oppidx_real();
	gpu_bottom_freq = ged_get_freq_by_idx(g_bottom_freq_id);

	g_cust_boost_freq_id = ged_get_min_oppidx_real();
	gpu_cust_boost_freq = ged_get_freq_by_idx(g_cust_boost_freq_id);

	g_cust_upbound_freq_id = 0;
	gpu_cust_upbound_freq = ged_get_freq_by_idx(g_cust_upbound_freq_id);

	g_policy_tar_freq = 0;
	g_mode = 0;

	ged_commit_freq = 0;
	ged_commit_opp_freq = 0;

	early_force_fallback_enable = 1;

#ifdef ENABLE_TIMER_BACKUP
	g_gpu_timer_based_emu = 0;
#else
	g_gpu_timer_based_emu = 1;
#endif /* ENABLE_TIMER_BACKUP */

	mtk_set_bottom_gpu_freq_fp = ged_dvfs_set_bottom_gpu_freq;
	mtk_get_bottom_gpu_freq_fp = ged_dvfs_get_bottom_gpu_freq;
	mtk_custom_get_gpu_freq_level_count_fp =
		ged_dvfs_get_gpu_freq_level_count;
	mtk_custom_boost_gpu_freq_fp = ged_dvfs_custom_boost_gpu_freq;
	mtk_custom_upbound_gpu_freq_fp = ged_dvfs_custom_ceiling_gpu_freq;
	mtk_get_custom_upbound_gpu_freq_fp = ged_dvfs_get_gpu_custom_upbound_freq;
	mtk_get_custom_boost_gpu_freq_fp = ged_dvfs_get_gpu_custom_boost_freq;
	mtk_get_gpu_loading_fp = ged_dvfs_get_gpu_loading;
	mtk_get_gpu_loading_avg_fp = ged_dvfs_get_gpu_loading_avg;
	mtk_get_gpu_block_fp = ged_dvfs_get_gpu_blocking;
	mtk_get_gpu_idle_fp = ged_dvfs_get_gpu_idle;

	mtk_get_gpu_bottom_freq_fp = ged_get_gpu_bottom_freq;

	ged_kpi_set_gpu_dvfs_hint_fp = ged_dvfs_last_and_target_cb;

	mtk_dvfs_margin_value_fp = ged_dvfs_margin_value;
	mtk_get_dvfs_margin_value_fp = ged_get_dvfs_margin_value;
#endif /* ENABLE_COMMON_DVFS */

	mtk_loading_base_dvfs_step_fp = ged_loading_base_dvfs_step;
	mtk_get_loading_base_dvfs_step_fp = ged_get_loading_base_dvfs_step;

	mtk_timer_base_dvfs_margin_fp =	ged_timer_base_dvfs_margin;
	mtk_get_timer_base_dvfs_margin_fp = ged_get_timer_base_dvfs_margin;

	mtk_dvfs_loading_mode_fp = ged_dvfs_loading_mode;
	mtk_get_dvfs_loading_mode_fp = ged_get_dvfs_loading_mode;

	mtk_dvfs_workload_mode_fp = ged_dvfs_workload_mode;
	mtk_get_dvfs_workload_mode_fp = ged_get_dvfs_workload_mode;

	mtk_set_fastdvfs_mode_fp = ged_set_fastdvfs_mode;
	mtk_get_fastdvfs_mode_fp = ged_get_fastdvfs_mode;
	ged_kpi_fastdvfs_update_dcs_fp = ged_fastdvfs_update_dcs;

	ged_get_last_commit_idx_fp = ged_dvfs_get_last_commit_idx;
	ged_get_last_commit_top_idx_fp = ged_dvfs_get_last_commit_top_idx;
	ged_get_last_commit_stack_idx_fp = ged_dvfs_get_last_commit_stack_idx;

	spin_lock_init(&g_sSpinLock);

	/* find node to support FASTDVFS (reduce mips) feature */
	reduce_mips_dvfs_node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_reduce_mips");

	if (unlikely(!reduce_mips_dvfs_node)) {
		GED_LOGI("Failed to find reduce_mips_dvfs_node");
	} else {
		GED_LOGI("Success to find reduce_mips_dvfs_node");
		of_property_read_u32(reduce_mips_dvfs_node, "reduce-mips-support",
								&g_reduce_mips_support);
	}
	/* set reduce mips support flag */
	mips_support_flag = g_reduce_mips_support;

	/* get min oppidx (limit min oppidx in gpueb) */
	if (mips_support_flag == 1) {
		get_min_oppidx = ged_get_min_oppidx();
		mtk_gpueb_sysram_write(SYSRAM_GPU_EB_GED_MIN_OPPIDX, get_min_oppidx);
		GED_LOGI("dts support gpueb dvfs, min_oppidx=%u", get_min_oppidx);
	} else
		GED_LOGI("dts not support gpueb dvfs");

	dvfs_loading_mode_node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_loading_mode");
	if (unlikely(!dvfs_loading_mode_node)) {
		GED_LOGI("Failed to find dvfs_loading_mode_node");
	} else {
		of_property_read_u32(dvfs_loading_mode_node, "dvfs-loading-mode",
							&gx_dvfs_loading_mode);
		of_property_read_u32(dvfs_loading_mode_node, "dvfs-workload-mode",
							&gx_dvfs_workload_mode);
	}

	async_dvfs_node = of_find_compatible_node(NULL, NULL, "mediatek,gpu_async_ratio");
	if (unlikely(!async_dvfs_node)) {
		GED_LOGI("Failed to find async_dvfs_node");
	} else {
		of_property_read_u32(async_dvfs_node, "async-ratio-support",
							&g_async_ratio_support);
		of_property_read_u32(async_dvfs_node, "async-virtual-table-support",
							&g_async_virtual_table_support);
		of_property_read_u32(async_dvfs_node, "async-oppnum-low", &g_min_async_oppnum);
		of_property_read_u32(async_dvfs_node, "async-oppnum-eachmask", &g_oppnum_eachmask);
	}
	// Find the largest oppidx whose stack freq does not repeat
	g_async_id_threshold = get_max_oppidx_with_same_stack(ged_get_min_oppidx_real());
	if (g_async_id_threshold != ged_get_min_oppidx_real())
		g_same_stack_in_opp = true;

	hrtimer_init(&gpu_mewtwo_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gpu_mewtwo_timer.function = gpu_mewtwo_timer_cb;
	gpu_mewtwo_timer_expire = ms_to_ktime(GPU_MEWTWO_TIMEOUT);

	// MBrain
	// Enable opp log by default
	gpu_opp_logs_enable = 1;
	g_sum_loading = 0;
	g_sum_delta_time = 0;

	g_ged_dvfs_commit_idx = gpufreq_get_opp_num(TARGET_DEFAULT) - 1;
	g_ged_dvfs_commit_top_idx = gpufreq_get_opp_num(TARGET_GPU) - 1;
	g_ged_dvfs_commit_dual = (g_ged_dvfs_commit_top_idx << 8) | g_ged_dvfs_commit_idx;
	g_last_def_commit_freq_id = g_ged_dvfs_commit_idx;
	g_is_onboot = 1;

	return GED_OK;
}

void ged_dvfs_system_exit(void)
{
	ged_dvfs_deinit_opp_cost();
	mutex_destroy(&gsDVFSLock);
	mutex_destroy(&gsPolicyLock);
	mutex_destroy(&gsVSyncOffsetLock);
	hrtimer_cancel(&gpu_mewtwo_timer);
}

#ifdef ENABLE_COMMON_DVFS
module_param(gpu_loading, uint, 0644);
module_param(gpu_block, uint, 0644);
module_param(gpu_idle, uint, 0644);
module_param(gpu_dvfs_enable, uint, 0644);
module_param(boost_gpu_enable, uint, 0644);
module_param(gpu_debug_enable, uint, 0644);
module_param(gpu_bottom_freq, uint, 0644);
module_param(gpu_cust_boost_freq, uint, 0644);
module_param(gpu_cust_upbound_freq, uint, 0644);
module_param(g_gpu_timer_based_emu, uint, 0644);
module_param(gpu_opp_logs_enable, uint, 0644);
#endif /* ENABLE_COMMON_DVFS */

