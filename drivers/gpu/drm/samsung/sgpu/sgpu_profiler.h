#ifndef SGPU_PROFILER_H
#define SGPU_PROFILER_H

#define TABLE_MAX                      (200)
#define RENDERINGTIME_MAX_TIME         (999999)
#define RENDERINGTIME_MIN_FRAME        (4000)
#define RTP_DEFAULT_FRAMETIME          (16666)
#define DEADLINE_DECON_INUS            (1000)
#define COMB_CTRL_OFF_TMAX (9999999)

#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/cpufreq.h>

#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/exynos-profiler.h>

#ifndef PROFILER_UNUSED
#define PROFILER_UNUSED(x)  ((void)(x))
#endif /* PROFILER_UNUSED */

struct profiler_interframe_data {
	unsigned int nrq;
	ktime_t sw_vsync;
	ktime_t sw_start;
	ktime_t sw_end;
	ktime_t sw_total;
	ktime_t hw_vsync;
	ktime_t hw_start;
	ktime_t hw_end;
	ktime_t hw_total;
	ktime_t sum_pre;
	ktime_t sum_cpu;
#if AMIGO_BUILD_VER >= 4
    ktime_t sum_swap;
#else
    ktime_t sum_v2s;
#endif
	ktime_t sum_gpu;
	ktime_t sum_v2f;
	ktime_t max_pre;
	ktime_t max_cpu;
#if AMIGO_BUILD_VER >= 4
    ktime_t max_swap;
#else
    ktime_t max_v2s;
#endif
	ktime_t max_gpu;
	ktime_t max_v2f;
	ktime_t vsync_interval;
	int sdp_next_cpuid;
	int sdp_cur_fcpu;
	int sdp_cur_fgpu;
	int sdp_next_fcpu;
	int sdp_next_fgpu;
	ktime_t cputime, gputime;
};

struct profiler_freq_estpower_data {
	int freq;
	int power;
};

struct profiler_info_data {
    atomic_t frame_counter;
    atomic_t vsync_counter;
    atomic64_t last_updated_vsync_time;
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
#if AMIGO_BUILD_VER >= 4
    ktime_t sum_swap;
#else
    ktime_t sum_v2s;
#endif
    ktime_t sum_gpu;
    ktime_t sum_v2f;
    ktime_t max_pre;
    ktime_t max_cpu;
#if AMIGO_BUILD_VER >= 4
    ktime_t max_swap;
#else
    ktime_t max_v2s;
#endif
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
    int powertable_size[NUM_OF_DOMAIN];
    struct profiler_freq_estpower_data *powertable[NUM_OF_DOMAIN];
#if AMIGO_BUILD_VER >= 4
    int target_cpuid_cur;
    int pmqos_minlock_prev[NUM_OF_DOMAIN];
    int pmqos_minlock_next[NUM_OF_DOMAIN];
#else
    int busy_cpuid_prev;
    int busy_cpuid;
    int busy_cpuid_next;
    int prev_minlock_cpu_freq;
    int prev_minlock_gpu_freq;
    int last_minlock_cpu_maxfreq;
    int last_minlock_gpu_maxfreq;
#endif
    int cur_freqlv[NUM_OF_DOMAIN];
    int max_minlock_freq;
};

struct profiler_pid_data {
    int list_readout;
    u16 list[NUM_OF_PID];
    int list_top;
};

struct profiler_cputracer_data {
    u64 bitmap[NUM_OF_CPU_DOMAIN];
    int hitcnt[NUM_OF_CPU_DOMAIN];
    int qsz;
};

/** External(Exynos) **/
void profiler_set_targetframetime(int us);
void profiler_set_targettime_margin(int us);
void profiler_set_util_margin(int percentage);
void profiler_set_decon_time(int us);
void profiler_set_vsync(ktime_t time_us);
void profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
void profiler_get_run_times(u64 *times);
u32 profiler_get_target_cpuid(u64 *cputrace_info);
void profiler_set_cputracer_size(int queue_size);
/* Governor setting */
void profiler_set_amigo_governor(int mode);
/* freq margin */
int profiler_get_freq_margin(void);
int profiler_set_freq_margin(int freq_margin);
/* pid filter */
void profiler_pid_get_list(u16 *list);
/* stc */
void profiler_stc_set_comb_ctrl(int val);
void profiler_stc_set_powertable(int id, int cnt, struct freq_table *table);
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
int profiler_interframe_sw_update(ktime_t start, ktime_t end, ktime_t total);
int profiler_interframe_hw_update(ktime_t start, ktime_t end, bool end_of_frame);
struct profiler_interframe_data *profiler_get_next_frameinfo(void);
int profiler_get_target_frametime(void);


/* initialization */
int sgpu_profiler_init(struct devfreq *df);
int sgpu_profiler_deinit(struct devfreq *df);
#endif