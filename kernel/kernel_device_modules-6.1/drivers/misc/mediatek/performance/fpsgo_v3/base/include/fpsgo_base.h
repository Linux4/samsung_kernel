/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __FPSGO_BASE_H__
#define __FPSGO_BASE_H__

#include <linux/compiler.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/sched/task.h>
#include <linux/sched.h>

#define FPSGO_VERSION_CODE 7
#define FPSGO_VERSION_MODULE "7.0"
#define MAX_DEP_NUM 100
#define WINDOW 20
#define RESCUE_TIMER_NUM 5
#define QUOTA_MAX_SIZE 300
#define GCC_MAX_SIZE 300
#define LOADING_CNT 256
#define FBT_FILTER_MAX_WINDOW 100
#define FPSGO_MW 1
#define FPSGO_DYNAMIC_WL 1
#define BY_PID_DEFAULT_VAL -1
#define BY_PID_DELETE_VAL -2
#define FPSGO_MAX_RECYCLE_IDLE_CNT 10
#define FPSGO_MAX_TREE_SIZE 10
#define FPSGO_MAX_RENDER_INFO_SIZE 20
#define FPSGO_MAX_BQ_ID_SIZE 200
#define FPSGO_MAX_SBE_SPID_LOADING_SIZE 10

enum {
	FPSGO_SET_UNKNOWN = -1,
	FPSGO_SET_BOOST_TA = 0,
	FPSGO_SET_SCHED_RATE = 2,
};

typedef void (*fpsgo_notify_is_boost_cb)(int fpsgo_is_boosting);
extern int (*register_get_fpsgo_is_boosting_fp)(fpsgo_notify_is_boost_cb func_cb);
extern int (*unregister_get_fpsgo_is_boosting_fp)(fpsgo_notify_is_boost_cb func_cb);
/* composite key for render_info rbtree */
struct fbt_render_key {
	int key1;
	unsigned long long key2;
};

struct fbt_jerk {
	int id;
	int jerking;
	int postpone;
	int last_check;
	unsigned long long frame_qu_ts;
	struct hrtimer timer;
	struct work_struct work;
};

struct fbt_proc {
	int active_jerk_id;
	struct fbt_jerk jerks[RESCUE_TIMER_NUM];
};

struct fbt_frame_info {
	unsigned long long running_time;
};

struct fbt_loading_info {
	int target_fps;
	long loading;
	int index;
};

struct fpsgo_loading {
	int pid;
	int loading;
	int prefer_type;
	int ori_ls;		/* original ls flag */
	int ori_rtprio;	/* original rt priority */
	int policy;
	int ori_nice;	/* original nice */
	int ori_vip;    /* original vip flag */
	int action;
	int rmidx;
	int heavyidx;
	int reset_taskmask;
};

struct fbt_thread_blc {
	int pid;
	unsigned long long buffer_id;
	unsigned int blc;
	unsigned int blc_b;
	unsigned int blc_m;
	int dep_num;
	struct fpsgo_loading dep[MAX_DEP_NUM];
	struct list_head entry;
};

struct fbt_ff_info {
	struct fbt_loading_info filter_loading[FBT_FILTER_MAX_WINDOW];
	struct fbt_loading_info filter_loading_b[FBT_FILTER_MAX_WINDOW];
	struct fbt_loading_info filter_loading_m[FBT_FILTER_MAX_WINDOW];

	int filter_index;
	int filter_index_b;
	int filter_index_m;
	unsigned int filter_frames_count;
	unsigned int filter_frames_count_b;
	unsigned int filter_frames_count_m;
};

struct fbt_boost_info {
	int target_fps;
	unsigned long long target_time;
	unsigned long long t2wnt;
	unsigned int last_blc;
	unsigned int last_blc_b;
	unsigned int last_blc_m;
	unsigned int last_normal_blc;
	unsigned int last_normal_blc_b;
	unsigned int last_normal_blc_m;
	unsigned int sbe_rescue;

	/* adjust loading */
	int loading_weight;
	int weight_cnt;
	int hit_cnt;
	int deb_cnt;
	int hit_cluster;
	struct fbt_frame_info frame_info[WINDOW];
	int f_iter;

	/* SeparateCap */
	long *cl_loading;

	/* rescue*/
	struct fbt_proc proc;
	int cur_stage;

	/* filter heavy frames */
	struct fbt_ff_info ff_obj;

	/* quota */
	long long quota_raw[QUOTA_MAX_SIZE];
	int quota_cnt;
	int quota_cur_idx;
	int quota_fps;
	int quota;
	int quota_adj; /* remove outlier */
	int quota_mod; /* mod target time */
	int enq_raw[QUOTA_MAX_SIZE];
	int enq_sum;
	int enq_avg;
	int deq_raw[QUOTA_MAX_SIZE];
	int deq_sum;
	int deq_avg;

	/* GCC */
	int gcc_quota;
	int gcc_count;
	int gcc_target_fps;
	int correction;
	int quantile_cpu_time;
	int quantile_gpu_time;

	/* FRS */
	unsigned long long t_duration;

	/* Closed loop */
	unsigned long long last_target_time_ns;
};

struct fpsgo_boost_attr {
	/*    LLF    */
	int loading_th_by_pid;
	int llf_task_policy_by_pid;
	int light_loading_policy_by_pid;

	/*    rescue    */
	int rescue_enable_by_pid;
	int rescue_second_enable_by_pid;
	int rescue_second_time_by_pid;
	int rescue_second_group_by_pid;

	/*   grouping   */
	int group_by_lr_by_pid;
	int heavy_group_num_by_pid;
	int second_group_num_by_pid;

	/* filter frame */
	int filter_frame_enable_by_pid;
	int filter_frame_window_size_by_pid;
	int filter_frame_kmin_by_pid;

	/* boost affinity */
	int boost_affinity_by_pid;
	int cpumask_heavy_by_pid;
	int cpumask_second_by_pid;
	int cpumask_others_by_pid;

	/* boost LR */
	int boost_lr_by_pid;

	/* set idle prefer */
	int set_ls_by_pid;
	int ls_groupmask_by_pid;

	/* separate cap */
	int separate_aa_by_pid;
	int limit_uclamp_by_pid;
	int limit_ruclamp_by_pid;
	int limit_uclamp_m_by_pid;
	int limit_ruclamp_m_by_pid;
	int separate_pct_b_by_pid;
	int separate_pct_m_by_pid;
	int separate_pct_other_by_pid;
	int separate_release_sec_by_pid;

	/* limit freq2cap */
	/* limit freq 2 cap */
	int limit_cfreq2cap_by_pid;
	int limit_rfreq2cap_by_pid;
	int limit_cfreq2cap_m_by_pid;
	int limit_rfreq2cap_m_by_pid;

	/* blc boost*/
	int blc_boost_by_pid;

	/* boost VIP */
	int boost_vip_by_pid;
	int rt_prio1_by_pid;
	int rt_prio2_by_pid;
	int rt_prio3_by_pid;
	int vip_mask_by_pid;
	int set_vvip_by_pid;

	/* QUOTA */
	int qr_enable_by_pid;
	int qr_t2wnt_x_by_pid;
	int qr_t2wnt_y_p_by_pid;
	int qr_t2wnt_y_n_by_pid;

	/*  GCC   */
	int gcc_enable_by_pid;
	int gcc_fps_margin_by_pid;
	int gcc_up_sec_pct_by_pid;
	int gcc_down_sec_pct_by_pid;
	int gcc_up_step_by_pid;
	int gcc_down_step_by_pid;
	int gcc_reserved_up_quota_pct_by_pid;
	int gcc_reserved_down_quota_pct_by_pid;
	int gcc_enq_bound_thrs_by_pid;
	int gcc_deq_bound_thrs_by_pid;
	int gcc_enq_bound_quota_by_pid;
	int gcc_deq_bound_quota_by_pid;

	/* Reset taskmask */
	int reset_taskmask;

	/* Closed Loop */
	int check_buffer_quota_by_pid;
	int expected_fps_margin_by_pid;
	int quota_v2_diff_clamp_min_by_pid;
	int quota_v2_diff_clamp_max_by_pid;
	int limit_min_cap_target_t_by_pid;

	/* Minus idle time*/
	int aa_b_minus_idle_t_by_pid;
};

struct render_info {
	struct rb_node render_key_node;
	struct list_head bufferid_list;
	struct rb_node linger_node;

	/*render basic info pid bufferId..etc*/
	int pid;
	struct fbt_render_key render_key; /*pid,identifier*/
	unsigned long long identifier;
	unsigned long long buffer_id;
	int queue_SF;
	int tgid;	/*render's process pid*/
	int api;	/*connected API*/
	int frame_type;
	int bq_type;
	int hwui;
	int frame_hint; /*frame start/end provided by user*/
	int sbe_control_flag;
	int control_pid_flag;
	int eas_control_flag;
	unsigned long long render_last_cb_ts;
	unsigned long master_type;

	/*render queue/dequeue/frame time info*/
	unsigned long long t_enqueue_start;
	unsigned long long t_enqueue_end;
	unsigned long long t_dequeue_start;
	unsigned long long t_dequeue_end;
	unsigned long long enqueue_length;
	unsigned long long enqueue_length_real;
	unsigned long long dequeue_length;
	unsigned long long Q2Q_time;
	unsigned long long running_time;
	unsigned long long raw_runtime;
	unsigned long long idle_time_b_us;
	unsigned long long wall_b_runtime_us;

	/*ux*/
	unsigned long long t_last_start;
	struct mutex ux_mlock;
	struct rb_root ux_frame_info_tree;
	int ux_blc_next;
	int ux_blc_cur;
	int sbe_enhance;

	/*fbt*/
	int linger;
	struct fbt_boost_info boost_info;
	struct fbt_thread_blc *p_blc;
	struct fpsgo_loading *dep_arr;
	int dep_valid_size;
	unsigned long long dep_loading_ts;
	unsigned long long linger_ts;
	int avg_freq;
	unsigned long long buffer_quota_ts;
	int buffer_quota;

	struct mutex thr_mlock;

	/* boost policy */
	struct fpsgo_boost_attr attr;

	int target_fps_origin;
};

#if FPSGO_MW
struct fpsgo_attr_by_pid {
	struct rb_node entry;
	int tgid;	/*for by tgid attribute*/
	int tid;	/*for by tid attribute*/
	unsigned long long ts;
	struct fpsgo_boost_attr attr;
};
#endif

struct BQ_id {
	struct fbt_render_key key;
	unsigned long long identifier;
	unsigned long long buffer_id;
	int queue_SF;
	int pid;
	int queue_pid;
	struct rb_node entry;
	unsigned long long ts;
};

struct acquire_info {
	int p_pid;
	int c_pid;
	int c_tid;
	int api;
	unsigned long long buffer_id;
	unsigned long long ts;
	struct rb_node entry;
	struct fbt_render_key key;
};

struct hwui_info {
	int pid;
	struct rb_node entry;
};

struct sbe_info {
	int pid;
	struct rb_node entry;
};

struct sbe_spid_loading {
	int tgid;
	int spid_arr[MAX_DEP_NUM];
	unsigned long long spid_latest_runtime[MAX_DEP_NUM];
	int spid_num;
	unsigned long long ts;
	struct rb_node rb_node;
};

struct fps_control_pid_info {
	int pid;
	struct rb_node entry;
	unsigned long long ts;
};

struct cam_cmd_node {
	int target_pid;
	struct list_head queue_list;
};

#ifdef FPSGO_DEBUG
#define FPSGO_LOGI(...)	pr_debug("FPSGO:" __VA_ARGS__)
#else
#define FPSGO_LOGI(...)
#endif
#define FPSGO_LOGE(...)	pr_debug("FPSGO:" __VA_ARGS__)
#define FPSGO_CONTAINER_OF(ptr, type, member) \
	((type *)(((char *)ptr) - offsetof(type, member)))

long long fpsgo_task_sched_runtime(struct task_struct *p);
long fpsgo_sched_setaffinity(pid_t pid, const struct cpumask *in_mask);
void *fpsgo_alloc_atomic(int i32Size);
void fpsgo_free(void *pvBuf, int i32Size);
unsigned long long fpsgo_get_time(void);
int fpsgo_arch_nr_clusters(void);
int fpsgo_arch_nr_get_opp_cpu(int cpu);
int fpsgo_arch_nr_get_cap_cpu(int cpu, int opp);
int fpsgo_arch_nr_max_opp_cpu(void);
int fpsgo_arch_nr_freq_cpu(void);
unsigned int fpsgo_cpufreq_get_freq_by_idx(
	int cpu, unsigned int opp);

int fpsgo_get_tgid(int pid);
void fpsgo_render_tree_lock(const char *tag);
void fpsgo_render_tree_unlock(const char *tag);
void fpsgo_thread_lock(struct mutex *mlock);
void fpsgo_thread_unlock(struct mutex *mlock);
void fpsgo_lockprove(const char *tag);
void fpsgo_thread_lockprove(const char *tag, struct mutex *mlock);
int fpsgo_get_acquire_hint_is_enable(void);
#if FPSGO_MW
void fpsgo_clear_llf_cpu_policy_by_pid(int tgid);
struct fpsgo_attr_by_pid *fpsgo_find_attr_by_pid(int pid, int add_new);
void delete_attr_by_pid(int tgid);
void fpsgo_reset_render_attr(int pid, int set_by_tid);
int is_to_delete_fpsgo_attr(struct fpsgo_attr_by_pid *fpsgo_attr);
struct fpsgo_attr_by_pid *fpsgo_find_attr_by_tid(int pid, int add_new);
void delete_attr_by_tid(int tid);
int is_to_delete_fpsgo_tid_attr(struct fpsgo_attr_by_pid *fpsgo_attr);
#endif  // FPSGO_MW
struct render_info *eara2fpsgo_search_render_info(int pid,
		unsigned long long buffer_id);
void fpsgo_delete_render_info(int pid,
	unsigned long long buffer_id, unsigned long long identifier);
struct render_info *fpsgo_search_and_add_render_info(int pid,
		unsigned long long identifier, int force);
struct hwui_info *fpsgo_search_and_add_hwui_info(int pid, int force);
void fpsgo_delete_hwui_info(int pid);
struct sbe_info *fpsgo_search_and_add_sbe_info(int pid, int force);
void fpsgo_delete_sbe_info(int pid);
struct fps_control_pid_info *fpsgo_search_and_add_fps_control_pid(int pid, int force);
void fpsgo_delete_fpsgo_control_pid(int pid);
int fpsgo_get_all_fps_control_pid_info(struct fps_control_pid_info *arr);
int fpsgo_get_all_sbe_info(struct sbe_info *arr);
int fpsgo_check_thread_status(void);
void fpsgo_clear(void);
struct BQ_id *fpsgo_find_BQ_id(int pid, int tgid, long long identifier,
		int action);
int fpsgo_get_BQid_pair(int pid, int tgid, long long identifier,
		unsigned long long *buffer_id, int *queue_SF, int enqueue);
int fpsgo_get_acquire_queue_pair_by_self(int tid, unsigned long long buffer_id);
int fpsgo_get_acquire_queue_pair_by_group(int tid, int *dep_arr, int dep_arr_num,
		unsigned long long buffer_id);
int fpsgo_check_cam_do_frame(void);
int fpsgo_check_all_render_blc(int render_tid, unsigned long long buffer_id);
int fpsgo_check_exist_queue_SF(int tgid);
struct acquire_info *fpsgo_add_acquire_info(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id, unsigned long long ts);
struct acquire_info *fpsgo_search_acquire_info(int tid, unsigned long long buffer_id);
int fpsgo_delete_acquire_info(int mode, int tid, unsigned long long buffer_id);
int fpsgo_check_is_cam_apk(int tgid);
void fpsgo_ctrl2base_get_cam_pid(int cmd, int *pid);
void fpsgo_ctrl2base_get_cam_perf(int tgid, int *rtid, int *blc);
void fpsgo_ctrl2base_notify_cam_close(void);
void fpsgo_main_trace(const char *fmt, ...);
void fpsgo_clear_uclamp_boost(void);
void fpsgo_clear_llf_cpu_policy(void);
void fpsgo_del_linger(struct render_info *thr);
int fpsgo_base_is_finished(struct render_info *thr);
int fpsgo_update_swap_buffer(int pid);
void fpsgo_sentcmd(int cmd, int value1, int value2);
void fpsgo_ctrl2base_get_pwr_cmd(int *cmd, int *value1, int *value2);
void fpsgo_ctrl2base_wait_cam(int cmd, int *pid);
int fpsgo_sbe_rescue_traverse(int pid, int start, int enhance, unsigned long long frame_id);
void fpsgo_stop_boost_by_pid(int pid);
void fpsgo_stop_boost_by_render(struct render_info *r);
int fpsgo_get_render_tid_by_render_name(int tgid, char *name,
	int *out_tid_arr, unsigned long long *out_bufID_arr,
	int *out_tid_num, int out_tid_max_num);
struct sbe_spid_loading *fpsgo_get_sbe_spid_loading(int tgid, int create);
int fpsgo_delete_sbe_spid_loading(int tgid);
int fpsgo_update_sbe_spid_loading(int *cur_pid_arr, int cur_pid_num, int tgid);
int fpsgo_ctrl2base_query_sbe_spid_loading(void);
int fpsgo_check_fbt_jerk_work_addr_invalid(struct work_struct *target_work);

int init_fpsgo_common(void);

enum FPSGO_FRAME_TYPE {
	NON_VSYNC_ALIGNED_TYPE = 0,
	BY_PASS_TYPE = 1,
	FRAME_HINT_TYPE = 2,
};

enum FPSGO_CONNECT_API {
	WINDOW_DISCONNECT = 0,
	NATIVE_WINDOW_API_EGL = 1,
	NATIVE_WINDOW_API_CPU = 2,
	NATIVE_WINDOW_API_MEDIA = 3,
	NATIVE_WINDOW_API_CAMERA = 4,
};

enum FPSGO_FORCE {
	FPSGO_FORCE_OFF = 0,
	FPSGO_FORCE_ON = 1,
	FPSGO_FREE = 2,
};

enum FPSGO_BQID_ACT {
	ACTION_FIND = 0,
	ACTION_FIND_ADD,
	ACTION_FIND_DEL,
};

enum FPSGO_RENDER_INFO_HWUI {
	RENDER_INFO_HWUI_UNKNOWN = 0,
	RENDER_INFO_HWUI_TYPE = 1,
	RENDER_INFO_HWUI_NONE = 2,
};

enum FPSGO_ACQUIRE_TYPE {
	ACQUIRE_UNKNOWN_TYPE,
	ACQUIRE_SELF_TYPE,
	ACQUIRE_CAMERA_TYPE,
	ACQUIRE_OTHER_TYPE
};

enum FPSGO_CAMERA_CMD {
	CAMERA_CLOSE,
	CAMERA_APK,
	CAMERA_SERVER,
	CAMERA_HWUI,
	CAMERA_DO_FRAME,
	CAMERA_APP_MIN_FPS,
	CAMERA_PERF_IDX
};

enum FPSGO_MASTER_TYPE {
	FPSGO_TYPE,
	ADPF_TYPE,
	MAGT_TYPE,
	KTF_TYPE
};

#endif

