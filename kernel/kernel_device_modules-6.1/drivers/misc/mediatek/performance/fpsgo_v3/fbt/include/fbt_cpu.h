/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FBT_CPU_H__
#define __FBT_CPU_H__

#include "fpsgo_base.h"
#include "fbt_cpu_ctrl.h"

extern int fpsgo_fbt2xgf_get_dep_list(int pid, int count,
		struct fpsgo_loading *arr, unsigned long long bufID);

#if defined(CONFIG_MTK_FPSGO) || defined(CONFIG_MTK_FPSGO_V3)
void fpsgo_ctrl2fbt_dfrc_fps(int fps_limit);

#if FPSGO_DYNAMIC_WL
void fpsgo_ctrl2fbt_cpufreq_cb_cap(int cid, int cap);
void fbt_cpufreq_cb_cap(int cid, int cap, unsigned long long *freq_lastest_ts,
	unsigned long long *freq_prev_cb_ts, unsigned int *freq_lastest_obv,
	unsigned int **freq_lastest_obv_cl, unsigned int **freq_lastest_is_cl_iso,
	unsigned int *freq_lastest_idx, unsigned long long *freq_last_cb_ts,
	unsigned int *freq_clus_obv, unsigned int *freq_clus_iso, unsigned int *freq_last_obv,
	unsigned long long fake_time_ns);
#else  // FPSGO_DYNAMIC_WL
void fpsgo_ctrl2fbt_cpufreq_cb_exp(int cid, unsigned long freq);
#endif  // FPSGO_DYNAMIC_WL
void fpsgo_ctrl2fbt_vsync(unsigned long long ts);
void fpsgo_ctrl2fbt_vsync_period(unsigned long long period_ts);
int fpsgo_ctrl2fbt_switch_uclamp(int enable);
void fpsgo_comp2fbt_frame_start(struct render_info *thr,
		unsigned long long ts);
void fpsgo_comp2fbt_deq_end(struct render_info *thr,
		unsigned long long ts);

void fpsgo_base2fbt_node_init(struct render_info *obj);
void fpsgo_base2fbt_item_del(struct fbt_thread_blc *pblc,
		struct fpsgo_loading *pdep,
		struct render_info *thr);
int fpsgo_base2fbt_get_max_blc_pid(int *pid, unsigned long long *buffer_id);
void fpsgo_base2fbt_check_max_blc(void);
void fpsgo_base2fbt_no_one_render(void);
void fpsgo_base2fbt_only_bypass(void);
void fpsgo_base2fbt_set_min_cap(struct render_info *thr, int min_cap,
					int min_cap_b, int min_cap_m);
void fpsgo_base2fbt_clear_llf_policy(struct render_info *thr);
void fpsgo_base2fbt_cancel_jerk(struct render_info *thr);
int fpsgo_base2fbt_is_finished(struct render_info *thr);
void fpsgo_base2fbt_stop_boost(struct render_info *thr);
void eara2fbt_set_2nd_t2wnt(int pid, unsigned long long buffer_id,
		unsigned long long t_duration);
int fpsgo_ctrl2fbt_buffer_quota(unsigned long long ts, int pid, int quota,
	unsigned long long identifier);
void notify_rl_ko_is_ready(void);
int fbt_get_rl_ko_is_ready(void);

int __init fbt_cpu_init(void);
void __exit fbt_cpu_exit(void);

int fpsgo_ctrl2fbt_switch_fbt(int enable);
int fbt_switch_ceiling(int value);

void fbt_set_down_throttle_locked(int nsec);
long fbt_get_loading(struct render_info *thr,
	unsigned long long start_ts,
	unsigned long long end_ts);
void fbt_reset_boost(struct render_info *thr);
void fbt_set_limit(int cur_pid, unsigned int blc_wt,
	int pid, unsigned long long buffer_id,
	int dep_num, struct fpsgo_loading dep[],
	struct render_info *thread_info, long long runtime);
int fbt_get_max_cap(int floor, int bhr_opp_local,
	int bhr_local, int pid, unsigned long long buffer_id);
void fbt_cal_min_max_cap(struct render_info *thr,
	int min_cap, int min_cap_b,
	int min_cap_m, int jerk, int pid, unsigned long long buffer_id,
	int *final_min_cap, int *final_min_cap_b, int *final_min_cap_m,
	int *final_max_cap, int *final_max_cap_b, int *final_max_cap_m,
	int *final_max_util, int *final_max_util_b, int *final_max_util_m);
unsigned int fbt_get_new_base_blc(struct cpu_ctrl_data *pld,
	int floor, int enhance, int eenhance_opp, int headroom);
int fbt_limit_capacity(int blc_wt, int is_rescue);
void fbt_set_ceiling(struct cpu_ctrl_data *pld,
	int pid, unsigned long long buffer_id);
void fbt_set_hard_limit_locked(int input,
	struct cpu_ctrl_data *pld);
struct fbt_thread_blc *fbt_xgff_list_blc_add(int pid,
	unsigned long long buffer_id);
void fbt_xgff_list_blc_del(struct fbt_thread_blc *p_blc);
void fbt_xgff_blc_set(struct fbt_thread_blc *p_blc, int blc_wt,
	int dep_num, int *dep_arr);
int fbt_xgff_dep_thread_notify(int pid, int op);

void fbt_set_render_boost_attr(struct render_info *thr);
void fbt_set_render_last_cb(struct render_info *thr, unsigned long long ts);
int fbt_get_dep_list(struct render_info *thr);
int fbt_determine_final_dep_list(struct render_info *thr, struct fpsgo_loading *final_dep_arr);
void fbt_set_min_cap_locked(struct render_info *thr, int min_cap,
		int min_cap_b, int min_cap_m, int max_cap, int max_cap_b,
		int max_cap_m, int max_util, int max_util_b, int max_util_m, int jerk);
unsigned int fbt_cal_blc(long aa, unsigned long long target_time,
	unsigned int last_blc_wt, unsigned long long t_q2q, int is_retarget,
	unsigned int *blc_wt);
int fbt_cal_aa(long loading, unsigned long long t_cpu, unsigned long long t_q2q, long *aa);
int fbt_cal_target_time_ns(int pid, unsigned long long buffer_id,
	int rl_is_ready, int rl_active, unsigned int target_fps_ori,
	unsigned int last_target_fps_ori, unsigned int target_fpks,
	unsigned long long target_t, int target_fps_margin,
	unsigned long long last_target_t_ns, unsigned long long t_q2q_ns,
	unsigned long long t_queue_end, unsigned long long next_vsync,
	int expected_fps_margin, int learning_rate_p, int learning_rate_n, int quota_clamp_max,
	int quota_diff_clamp_min, int quota_diff_clamp_max, int limit_min_cap_final,
	int separate_aa_active, long aa_n, long aa_b,
	long aa_m, int limit_cap, int limit_cap_b, int limit_cap_m,
	unsigned long long *out_target_t_ns);
void fbt_check_max_blc_locked(int pid);
void fpsgo_get_fbt_mlock(const char *tag);
void fpsgo_put_fbt_mlock(const char *tag);
void fpsgo_get_blc_mlock(const char *tag);
void fpsgo_put_blc_mlock(const char *tag);
void fbt_find_max_blc(unsigned int *temp_blc, int *temp_blc_pid,
	unsigned long long *temp_blc_buffer_id,
	int *temp_blc_dep_num, struct fpsgo_loading temp_blc_dep[]);

struct fbt_setting_info {
	int bhr_opp;
	int cluster_num;
	int rescue_enhance_f;
	int rescue_second_copp;
	int boost_ta;
	int max_blc_pid;
	unsigned long long max_blc_buffer_id;
};

int fbt_get_rescue_opp_c(void);
int fbt_get_rescue_opp_f(void);
void fbt_set_max_blc_stage(int stage);
void fbt_set_max_blc_cur(unsigned int blc);
void fbt_get_setting_info(struct fbt_setting_info *sinfo);

#else
static inline void fpsgo_ctrl2fbt_dfrc_fps(int fps_limit) { }
static inline void fpsgo_ctrl2fbt_cpufreq_cb_exp(int cid,
		unsigned long freq) { }
static inline void fpsgo_ctrl2fbt_cpufreq_cb_cap(int cid, int cap) { }
static inline void fbt_cpufreq_cb_cap(int cid, int cap, unsigned long long *freq_lastest_ts,
	unsigned long long *freq_prev_cb_ts, unsigned int *freq_lastest_obv,
	unsigned int **freq_lastest_obv_cl, unsigned int **freq_lastest_is_cl_iso,
	unsigned int *freq_lastest_idx, unsigned long long *freq_last_cb_ts,
	unsigned int *freq_clus_obv, unsigned int *freq_clus_iso, unsigned int *freq_last_obv,
	unsigned long long fake_time_ns) { }
static inline void fpsgo_ctrl2fbt_vsync(unsigned long long ts) { }
static inline void fpsgo_ctrl2fbt_vsync_period(unsigned long long period_ts) { }
int fpsgo_ctrl2fbt_switch_uclamp(int enable) { return 0; }
static inline void fpsgo_comp2fbt_frame_start(struct render_info *thr,
	unsigned long long ts) { }
static inline void fpsgo_comp2fbt_deq_end(struct render_info *thr,
		unsigned long long ts) { }

static inline int fbt_cpu_init(void) { return 0; }
static inline void fbt_cpu_exit(void) { }

static inline int fpsgo_ctrl2fbt_switch_fbt(int enable) { return 0; }

static inline void fpsgo_base2fbt_node_init(struct render_info *obj) { }
static inline void fpsgo_base2fbt_item_del(struct fbt_thread_blc *pblc,
		struct fpsgo_loading *pdep,
		struct render_info *thr) { }
static inline int fpsgo_base2fbt_get_max_blc_pid(int *pid,
		unsigned long long *buffer_id) { return 0; }
static inline void fpsgo_base2fbt_check_max_blc(void) { }
static inline void fpsgo_base2fbt_no_one_render(void) { }
static inline int fbt_switch_ceiling(int en) { return 0; }
static inline void fpsgo_base2fbt_set_min_cap(struct render_info *thr,
				int min_cap) { }
static inline void fpsgo_base2fbt_clear_llf_policy(struct render_info *thr) { }
static inline void fpsgo_base2fbt_cancel_jerk(struct render_info *thr) { }
static inline int fpsgo_base2fbt_is_finished(struct render_info *thr) { return 0; }
static inline void fpsgo_base2fbt_stop_boost(struct render_info *thr) { }
static inline void eara2fbt_set_2nd_t2wnt(int pid, unsigned long long buffer_id,
			unsigned long long t_duration) { }
static inline void fbt_set_render_boost_attr(struct render_info *thr) { }
static inline void fbt_set_render_last_cb(struct render_info *thr, unsigned long long ts) { }
static inline int fpsgo_ctrl2fbt_buffer_quota(unsigned long long ts, int pid, int quota,
			unsigned long long identifier) { return 0; }
static inline void notify_rl_ko_is_ready(void) { }
#endif

#endif
