// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/wait.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/average.h>
#include <linux/topology.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <asm/div64.h>
#include <mt-plat/fpsgo_common.h>
#include "../fbt/include/fbt_cpu.h"
#include "../fbt/include/xgf.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fstb.h"
#include "fstb_usedext.h"
#include "fpsgo_usedext.h"

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#include "ged_kpi.h"
#endif

#define FSTB_USEC_DIVIDER 1000000
#define mtk_fstb_dprintk_always(fmt, args...) \
	pr_debug("[FSTB]" fmt, ##args)

#define mtk_fstb_dprintk(fmt, args...) \
	do { \
		if (fstb_fps_klog_on == 1) \
			pr_debug("[FSTB]" fmt, ##args); \
	} while (0)

struct k_list {
	struct list_head queue_list;
	int fpsgo2pwr_pid;
	int fpsgo2pwr_fps;
};

static int dfps_ceiling = DEFAULT_DFPS;
static int min_fps_limit = CFG_MIN_FPS_LIMIT;
static int QUANTILE = 50;
static int margin_mode = 2;
static int margin_mode_dbnc_a = 9;
static int margin_mode_dbnc_b = 1;
static int fps_reset_tolerence = DEFAULT_RESET_TOLERENCE;
static int condition_get_fps;
static int condition_fstb_active;
static long long FRAME_TIME_WINDOW_SIZE_US = USEC_PER_SEC;
static int fstb_fps_klog_on;
static int fstb_enable, fstb_active, fstb_active_dbncd, fstb_idle_cnt;
static int fstb_self_ctrl_fps_enable;
static long long last_update_ts;
static int total_fstb_policy_cmd_num;
static int fstb_max_dep_path_num = DEFAULT_MAX_DEP_PATH_NUM;
static int fstb_max_dep_task_num = DEFAULT_MAX_DEP_TASK_NUM;
static int gpu_slowdown_check;
static int dfrc_period;
static int vsync_period;
static unsigned long long vsync_ts_last;
static unsigned int vsync_count;
static unsigned long long vsync_duration_sum;
static unsigned long long last_cam_queue_ts;
static int fstb_vsync_app_fps_disable;

int fstb_no_r_timer_enable;
EXPORT_SYMBOL(fstb_no_r_timer_enable);
int fstb_filter_poll_enable;
EXPORT_SYMBOL(fstb_filter_poll_enable);

DECLARE_WAIT_QUEUE_HEAD(queue);
DECLARE_WAIT_QUEUE_HEAD(active_queue);
DECLARE_WAIT_QUEUE_HEAD(pwr_queue);

static void fstb_fps_stats(struct work_struct *work);
static DECLARE_WORK(fps_stats_work,
		(void *) fstb_fps_stats);

static LIST_HEAD(head);
static HLIST_HEAD(fstb_frame_infos);
static HLIST_HEAD(fstb_render_target_fps);

static DEFINE_MUTEX(fstb_lock);
static DEFINE_MUTEX(fstb_fps_active_time);
static DEFINE_MUTEX(fpsgo2pwr_lock);
static DEFINE_MUTEX(fstb_ko_lock);
static DEFINE_MUTEX(fstb_policy_cmd_lock);
static DEFINE_MUTEX(fstb_info_callback_lock);

void (*gbe_fstb2gbe_poll_fp)(struct hlist_head *list);

static struct kobject *fstb_kobj;
static struct hrtimer hrt;
static struct workqueue_struct *wq;
static struct rb_root fstb_policy_cmd_tree;
struct FSTB_POWERFPS_LIST powerfps_array[64];

static time_notify_callback q2q_notify_callback_list[MAX_INFO_CALLBACK];
static perf_notify_callback blc_notify_callback_list[MAX_INFO_CALLBACK];
static perf_notify_callback delete_notify_callback_list[MAX_INFO_CALLBACK];

int (*fstb_get_target_fps_fp)(int pid, unsigned long long bufID, int tgid,
	int dfps_ceiling, int max_dep_path_num, int max_dep_task_num,
	int *target_fps_margin, int *ctrl_fps_tid, int *ctrl_fps_flag,
	unsigned long long cur_queue_end_ts, int eara_is_active);
EXPORT_SYMBOL(fstb_get_target_fps_fp);
void (*fstb_delete_render_info_fp)(int pid, unsigned long long bufID);
EXPORT_SYMBOL(fstb_delete_render_info_fp);
int (*fpsgo2msync_hint_frameinfo_fp)(unsigned int render_tid, unsigned int reader_bufID,
		unsigned int target_fps, unsigned long q2q_time, unsigned long q2q_time2);
EXPORT_SYMBOL(fpsgo2msync_hint_frameinfo_fp);

// AutoTest
int (*test_fstb_hrtimer_info_update_fp)(int *tmp_tid, unsigned long long *tmp_ts, int tmp_num,
	int *i_tid, unsigned long long *i_latest_ts, unsigned long long *i_diff,
	int *i_idx, int *i_idle, int i_num,
	int *o_tid, unsigned long long *o_latest_ts, unsigned long long *o_diff,
	int *o_idx, int *o_idle, int o_num, int o_ret);
EXPORT_SYMBOL(test_fstb_hrtimer_info_update_fp);

static void enable_fstb_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0,
			FRAME_TIME_WINDOW_SIZE_US * 1000);
	hrtimer_start(&hrt, ktime, HRTIMER_MODE_REL);
}

static void disable_fstb_timer(void)
{
	hrtimer_cancel(&hrt);
}

static enum hrtimer_restart mt_fstb(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &fps_stats_work);

	return HRTIMER_NORESTART;
}

int is_fstb_enable(void)
{
	return fstb_enable;
}

int is_fstb_active(long long time_diff)
{
	int active = 0;
	ktime_t cur_time;
	long long cur_time_us;

	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	mutex_lock(&fstb_fps_active_time);

	if (cur_time_us - last_update_ts < time_diff)
		active = 1;

	mutex_unlock(&fstb_fps_active_time);

	return active;
}
EXPORT_SYMBOL(is_fstb_active);

static void fstb_sentcmd(int pid, int fps)
{
	static struct k_list *node;

	mutex_lock(&fpsgo2pwr_lock);
	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL)
		goto out;
	node->fpsgo2pwr_pid = pid;
	node->fpsgo2pwr_fps = fps;
	list_add_tail(&node->queue_list, &head);
	condition_get_fps = 1;
out:
	mutex_unlock(&fpsgo2pwr_lock);
	wake_up_interruptible(&pwr_queue);
}

void fpsgo_ctrl2fstb_get_fps(int *pid, int *fps)
{
	static struct k_list *node;

	wait_event_interruptible(pwr_queue, condition_get_fps);
	mutex_lock(&fpsgo2pwr_lock);
	if (!list_empty(&head)) {
		node = list_first_entry(&head, struct k_list, queue_list);
		*pid = node->fpsgo2pwr_pid;
		*fps = node->fpsgo2pwr_fps;
		list_del(&node->queue_list);
		kfree(node);
	}
	if (list_empty(&head))
		condition_get_fps = 0;
	mutex_unlock(&fpsgo2pwr_lock);
}

int fpsgo_ctrl2fstb_wait_fstb_active(void)
{
	wait_event_interruptible(active_queue, condition_fstb_active);
	mutex_lock(&fstb_lock);
	condition_fstb_active = 0;
	mutex_unlock(&fstb_lock);
	return 0;
}

void fpsgo_ctrl2fstb_cam_queue_time_update(unsigned long long ts)
{
	last_cam_queue_ts = ts;
}

int fpsgo_other2fstb_check_cam_do_frame(void)
{
	int ret = 1;
	unsigned long long cur_ts = fpsgo_get_time();

	if (cur_ts > last_cam_queue_ts &&
		(cur_ts - last_cam_queue_ts) >= 1000000000ULL) {
		ret = 0;
		fpsgo_main_trace("[fstb] cur_ts:%llu last_cam_queue_ts:%llu",
			cur_ts, last_cam_queue_ts);
	}

	return ret;
}

static int fstb_arbitrate_target_fps(int raw_target_fps, int *margin,
	struct FSTB_FRAME_INFO *iter)
{
	int i;
	int local_margin = *margin;
	int final_target_fps = raw_target_fps;
	unsigned long arb_num = 0;
	struct FSTB_RENDER_TARGET_FPS *rtfiter;

	if (iter->notify_target_fps > 0) {
		final_target_fps = iter->notify_target_fps;
		local_margin = 0;
		set_bit(0, &arb_num);
	}

	if (iter->sbe_state == 1) {
		final_target_fps = dfps_ceiling;
		local_margin = 0;
		set_bit(1, &arb_num);
	}

	if (iter->magt_target_fps > 0) {
		final_target_fps = iter->magt_target_fps;
		local_margin = 0;
		set_bit(2, &arb_num);
	}

	hlist_for_each_entry(rtfiter, &fstb_render_target_fps, hlist) {
		if (!strncmp(iter->proc_name, rtfiter->process_name, 16) ||
			rtfiter->pid == iter->pid) {
			for (i = rtfiter->nr_level-1; i >= 0; i--) {
				if (rtfiter->level[i].start >= raw_target_fps) {
					final_target_fps =
						raw_target_fps >= rtfiter->level[i].end ?
						raw_target_fps : rtfiter->level[i].end;
					break;
				}
			}
			if (i < 0)
				final_target_fps = rtfiter->level[0].start;

			if (final_target_fps == rtfiter->level[0].start)
				local_margin = 0;
			else
				local_margin = *margin;

			set_bit(3, &arb_num);
		}
	}

	if (final_target_fps > dfps_ceiling) {
		final_target_fps = dfps_ceiling;
		set_bit(4, &arb_num);
	}
	if (final_target_fps < min_fps_limit) {
		final_target_fps = min_fps_limit;
		set_bit(5, &arb_num);
	}

	fpsgo_systrace_c_fstb(iter->pid, iter->bufid, arb_num, "fstb_arb_num");

	*margin = local_margin;

	return final_target_fps;
}

static time_notify_callback *fstb_get_target_info_cb_list(int mode)
{
	time_notify_callback *target_cb_list = NULL;

	switch (mode) {
	case FPSGO_Q2Q_TIME:
		target_cb_list = q2q_notify_callback_list;
		break;
	case FPSGO_CPU_TIME:
		break;
	case FPSGO_QUEUE_FPS:
		break;
	case FPSGO_TARGET_FPS:
		break;
	default:
		break;
	}

	return target_cb_list;
}

int fpsgo_other2fstb_register_info_callback(int mode, time_notify_callback func_cb)
{
	int i;
	int empty_idx = -1;
	int ret = 0;
	time_notify_callback *target_cb_list = NULL;

	mutex_lock(&fstb_info_callback_lock);

	target_cb_list = fstb_get_target_info_cb_list(mode);
	if (!target_cb_list) {
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < MAX_INFO_CALLBACK; i++) {
		if (target_cb_list[i] == func_cb)
			break;
		if (target_cb_list[i] == NULL && empty_idx == -1)
			empty_idx = i;
	}
	if (i >= MAX_INFO_CALLBACK) {
		if (empty_idx < 0 || empty_idx >= MAX_INFO_CALLBACK)
			ret = -ENOMEM;
		else
			target_cb_list[empty_idx] = func_cb;
	}

out:
	mutex_unlock(&fstb_info_callback_lock);
	return ret;
}
EXPORT_SYMBOL(fpsgo_other2fstb_register_info_callback);

int fpsgo_other2fstb_unregister_info_callback(int mode, time_notify_callback func_cb)
{
	int i;
	int ret = 0;
	time_notify_callback *target_cb_list = NULL;

	mutex_lock(&fstb_info_callback_lock);

	target_cb_list = fstb_get_target_info_cb_list(mode);
	if (!target_cb_list) {
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < MAX_INFO_CALLBACK; i++) {
		if (target_cb_list[i] == func_cb) {
			target_cb_list[i] = NULL;
			break;
		}
	}

out:
	mutex_unlock(&fstb_info_callback_lock);
	return ret;
}
EXPORT_SYMBOL(fpsgo_other2fstb_unregister_info_callback);

static perf_notify_callback *fstb_get_target_perf_cb_list(int mode)
{
	perf_notify_callback *target_cb_list = NULL;

	switch (mode) {
	case FPSGO_PERF_IDX:
		target_cb_list = blc_notify_callback_list;
		break;
	case FPSGO_DELETE:
		target_cb_list = delete_notify_callback_list;
		break;
	default:
		break;
	}

	return target_cb_list;
}

int fpsgo_other2fstb_register_perf_callback(int mode, perf_notify_callback func_cb)
{
	int i;
	int empty_idx = -1;
	int ret = 0;
	perf_notify_callback *target_cb_list = NULL;

	mutex_lock(&fstb_info_callback_lock);

	target_cb_list = fstb_get_target_perf_cb_list(mode);
	if (!target_cb_list) {
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < MAX_INFO_CALLBACK; i++) {
		if (target_cb_list[i] == func_cb)
			break;
		if (target_cb_list[i] == NULL && empty_idx == -1)
			empty_idx = i;
	}
	if (i >= MAX_INFO_CALLBACK) {
		if (empty_idx < 0 || empty_idx >= MAX_INFO_CALLBACK)
			ret = -ENOMEM;
		else
			target_cb_list[empty_idx] = func_cb;
	}

out:
	mutex_unlock(&fstb_info_callback_lock);
	return ret;
}
EXPORT_SYMBOL(fpsgo_other2fstb_register_perf_callback);

int fpsgo_other2fstb_unregister_perf_callback(int mode, perf_notify_callback func_cb)
{
	int i;
	int ret = 0;
	perf_notify_callback *target_cb_list = NULL;

	mutex_lock(&fstb_info_callback_lock);

	target_cb_list = fstb_get_target_perf_cb_list(mode);
	if (!target_cb_list) {
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < MAX_INFO_CALLBACK; i++) {
		if (target_cb_list[i] == func_cb) {
			target_cb_list[i] = NULL;
			break;
		}
	}

out:
	mutex_unlock(&fstb_info_callback_lock);
	return ret;
}
EXPORT_SYMBOL(fpsgo_other2fstb_unregister_perf_callback);

int fpsgo_fstb2other_info_update(int pid, unsigned long long bufID,
	int mode, int fps, unsigned long long time, int blc, int sbe_ctrl)
{
	int i;
	int ret = 0;
	unsigned long long ts;

	mutex_lock(&fstb_info_callback_lock);

	switch (mode) {
	case FPSGO_Q2Q_TIME:
		for (i = 0; i < MAX_INFO_CALLBACK; i++) {
			if (q2q_notify_callback_list[i])
				q2q_notify_callback_list[i](pid, bufID, fps, time);
		}
		break;
	case FPSGO_PERF_IDX:
		ts = fpsgo_get_time();
		for (i = 0; i < MAX_INFO_CALLBACK; i++) {
			if (blc_notify_callback_list[i])
				blc_notify_callback_list[i](pid, bufID, blc, sbe_ctrl, ts);
		}
		break;
	case FPSGO_DELETE:
		for (i = 0; i < MAX_INFO_CALLBACK; i++) {
			if (delete_notify_callback_list[i])
				delete_notify_callback_list[i](pid, bufID, blc, sbe_ctrl, 0);
		}
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

out:
	mutex_unlock(&fstb_info_callback_lock);
	return ret;
}

int fpsgo_other2fstb_get_fps(int pid, unsigned long long bufID,
	int *qfps_arr, int *qfps_num, int max_qfps_num,
	int *tfps_arr, int *tfps_num, int max_tfps_num)
{
	int local_q_idx = 0;
	int local_t_idx = 0;
	int local_tfps = 0;
	int local_margin = 0;
	struct FSTB_FRAME_INFO *iter = NULL;
	struct hlist_node *h = NULL;

	if (!qfps_arr || !qfps_num ||
		!tfps_arr || !tfps_num)
		return -ENOMEM;

	mutex_lock(&fstb_lock);
	hlist_for_each_entry_safe(iter, h, &fstb_frame_infos, hlist) {
		if (iter->pid == pid &&
			(!bufID || iter->bufid == bufID)) {
			if (local_q_idx < max_qfps_num) {
				qfps_arr[local_q_idx] = iter->queue_fps;
				local_q_idx++;
			}
			if (local_t_idx < max_tfps_num) {
				local_tfps = iter->self_ctrl_fps_enable ?
					iter->target_fps_v2 : iter->target_fps;
				local_margin = iter->self_ctrl_fps_enable ?
					iter->target_fps_margin_v2 : iter->target_fps_margin;
				local_tfps = fstb_arbitrate_target_fps(local_tfps,
							&local_margin, iter);
				tfps_arr[local_t_idx] = local_tfps;
				local_t_idx++;
			}
		}
	}
	mutex_unlock(&fstb_lock);

	*qfps_num = local_q_idx;
	*tfps_num = local_t_idx;

	return 0;
}

static int fstb_enter_delete_render_info(int pid, unsigned long long bufID)
{
	int ret = 1;

	mutex_lock(&fstb_ko_lock);

	if (fstb_delete_render_info_fp)
		fstb_delete_render_info_fp(pid, bufID);
	else {
		ret = -ENOENT;
		mtk_fstb_dprintk_always("fstb_delete_render_info_fp is NULL\n");
	}

	mutex_unlock(&fstb_ko_lock);

	return ret;
}

static int fstb_enter_get_target_fps(int pid, unsigned long long bufID, int tgid,
	int *target_fps_margin, unsigned long long cur_queue_end_ts,
	int eara_is_active)
{
	int ret = 0;
	int ctrl_fps_tid = 0, ctrl_fps_flag = 0;

	mutex_lock(&fstb_ko_lock);

	if (fstb_get_target_fps_fp)
		ret = fstb_get_target_fps_fp(pid, bufID, tgid,
			dfps_ceiling, fstb_max_dep_path_num, fstb_max_dep_task_num,
			target_fps_margin, &ctrl_fps_tid, &ctrl_fps_flag,
			cur_queue_end_ts, eara_is_active);
	else {
		ret = -ENOENT;
		mtk_fstb_dprintk_always("fstb_get_target_fps_fp is NULL\n");
	}

	fpsgo_systrace_c_fstb_man(pid, bufID, ctrl_fps_tid, "ctrl_fps_tid");
	fpsgo_systrace_c_fstb_man(pid, bufID, ctrl_fps_flag, "ctrl_fps_flag");

	mutex_unlock(&fstb_ko_lock);

	return ret;
}

static void fstb_post_process_target_fps(int tfps, int margin, int diff,
	int *final_tfps, int *final_tfpks, unsigned long long *final_time,
	int vsync_fps_disable)
{
	int local_tfps, local_margin;
	int min_limit = min_fps_limit * 1000;
	int max_limit = dfps_ceiling * 1000;
	unsigned long long local_time = 1000000000000ULL;

	if (!vsync_fps_disable)
		max_limit = min(max_limit, FSTB_USEC_DIVIDER * 1000 / vsync_period);

	local_tfps = tfps * 1000;
	local_tfps += diff;
	local_tfps = clamp(local_tfps, min_limit, max_limit);
	if (final_tfps)
		*final_tfps = local_tfps / 1000;
	if (final_tfpks)
		*final_tfpks = local_tfps;

	local_margin = margin * 1000;
	local_time = div64_u64(local_time,
					(local_tfps + local_margin) > max_limit ?
					max_limit : (local_tfps + local_margin));
	if (final_time)
		*final_time = local_time;
}

int fpsgo_ctrl2fstb_switch_fstb(int enable)
{
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *t;

	mutex_lock(&fstb_lock);
	if (fstb_enable == enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	fstb_enable = enable;
	fpsgo_systrace_c_fstb(-200, 0, fstb_enable, "fstb_enable");

	mtk_fstb_dprintk_always("%s %d\n", __func__, fstb_enable);
	if (!fstb_enable) {
		hlist_for_each_entry_safe(iter, t,
				&fstb_frame_infos, hlist) {
			fstb_enter_delete_render_info(iter->pid, iter->bufid);
			hlist_del(&iter->hlist);
			vfree(iter);
		}
	} else {
		if (wq) {
			struct work_struct *psWork =
				kmalloc(sizeof(struct work_struct), GFP_ATOMIC);

			if (psWork) {
				INIT_WORK(psWork, fstb_fps_stats);
				queue_work(wq, psWork);
			}
		}
	}

	mutex_unlock(&fstb_lock);

	return 0;
}

static void switch_fstb_active(void)
{
	fpsgo_systrace_c_fstb(-200, 0,
			fstb_active, "fstb_active");
	fpsgo_systrace_c_fstb(-200, 0,
			fstb_active_dbncd, "fstb_active_dbncd");

	mtk_fstb_dprintk_always("%s %d %d\n",
			__func__, fstb_active, fstb_active_dbncd);
	enable_fstb_timer();
}

static struct FSTB_FRAME_INFO *add_new_frame_info(int pid, unsigned long long bufID,
	int hwui_flag, unsigned long master_type)
{
	struct task_struct *tsk = NULL, *gtsk = NULL;
	struct FSTB_FRAME_INFO *new_frame_info;

	new_frame_info = vmalloc(sizeof(*new_frame_info));
	if (new_frame_info == NULL)
		goto out;

	new_frame_info->pid = pid;
	new_frame_info->target_fps = dfps_ceiling;
	new_frame_info->target_fps_v2 = dfps_ceiling;
	new_frame_info->target_fps_margin_v2 = 0;
	new_frame_info->target_fps_margin = 0;
	new_frame_info->target_fps_margin_dbnc_a = margin_mode_dbnc_a;
	new_frame_info->target_fps_margin_dbnc_b = margin_mode_dbnc_b;
	new_frame_info->target_fps_diff = 0;
	new_frame_info->target_fps_notifying = 0;
	new_frame_info->queue_fps = dfps_ceiling;
	new_frame_info->bufid = bufID;
	new_frame_info->queue_time_begin = 0;
	new_frame_info->queue_time_end = 0;
	new_frame_info->weighted_cpu_time_begin = 0;
	new_frame_info->weighted_cpu_time_end = 0;
	new_frame_info->weighted_gpu_time_begin = 0;
	new_frame_info->weighted_gpu_time_end = 0;
	new_frame_info->quantile_cpu_time = -1;
	new_frame_info->quantile_gpu_time = -1;
	new_frame_info->render_idle_cnt = 0;
	new_frame_info->hwui_flag = hwui_flag;
	new_frame_info->sbe_state = 0;
	new_frame_info->target_time = 0;
	new_frame_info->self_ctrl_fps_enable = 0;
	new_frame_info->notify_target_fps = 0;
	new_frame_info->vsync_app_fps_disable = 0;
	new_frame_info->master_type = master_type;

	if (test_bit(ADPF_TYPE, &master_type)) {
		strcpy(new_frame_info->proc_name, "ADPF");
		goto out;
	}

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		get_task_struct(tsk);
		gtsk = find_task_by_vpid(tsk->tgid);
		put_task_struct(tsk);
		if (gtsk)
			get_task_struct(gtsk);
	}
	rcu_read_unlock();

	if (gtsk) {
		strncpy(new_frame_info->proc_name, gtsk->comm, 16);
		new_frame_info->proc_name[15] = '\0';
		new_frame_info->proc_id = gtsk->pid;
		put_task_struct(gtsk);
	} else {
		new_frame_info->proc_name[0] = '\0';
		new_frame_info->proc_id = 0;
	}

out:
	return new_frame_info;
}

static void fstb_update_policy_cmd(struct FSTB_FRAME_INFO *iter,
	struct FSTB_POLICY_CMD *policy, unsigned long long ts)
{
	if (policy) {
		iter->self_ctrl_fps_enable = policy->self_ctrl_fps_enable;
		iter->notify_target_fps = policy->notify_target_fps;
		iter->vsync_app_fps_disable = policy->vsync_app_fps_disable;
		policy->ts = ts;
	} else {
		iter->self_ctrl_fps_enable = fstb_self_ctrl_fps_enable ? 1 : 0;
		iter->notify_target_fps = 0;
		iter->vsync_app_fps_disable = 0;
	}
}

static void fstb_delete_policy_cmd(struct FSTB_POLICY_CMD *iter)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct FSTB_POLICY_CMD *tmp_iter = NULL, *min_iter = NULL;
	struct rb_node *rbn = NULL;

	if (iter) {
		if (!iter->self_ctrl_fps_enable && !iter->notify_target_fps &&
			!iter->vsync_app_fps_disable) {
			min_iter = iter;
			goto delete;
		} else
			return;
	}

	if (RB_EMPTY_ROOT(&fstb_policy_cmd_tree))
		return;

	rbn = rb_first(&fstb_policy_cmd_tree);
	while (rbn) {
		tmp_iter = rb_entry(rbn, struct FSTB_POLICY_CMD, rb_node);
		if (tmp_iter->ts < min_ts) {
			min_ts = tmp_iter->ts;
			min_iter = tmp_iter;
		}
		rbn = rb_next(rbn);
	}

	if (!min_iter)
		return;

delete:
	rb_erase(&min_iter->rb_node, &fstb_policy_cmd_tree);
	kfree(min_iter);
	total_fstb_policy_cmd_num--;
}

static struct FSTB_POLICY_CMD *fstb_get_policy_cmd(int tgid,
	unsigned long long ts, int force)
{
	struct rb_node **p = &fstb_policy_cmd_tree.rb_node;
	struct rb_node *parent = NULL;
	struct FSTB_POLICY_CMD *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct FSTB_POLICY_CMD, rb_node);

		if (tgid < iter->tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter->tgid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!force)
		return NULL;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->tgid = tgid;
	iter->self_ctrl_fps_enable = 0;
	iter->notify_target_fps = 0;
	iter->vsync_app_fps_disable = 0;
	iter->ts = ts;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &fstb_policy_cmd_tree);
	total_fstb_policy_cmd_num++;

	if (total_fstb_policy_cmd_num > MAX_FSTB_POLICY_CMD_NUM)
		fstb_delete_policy_cmd(NULL);

	return iter;
}

static void fstb_set_policy_cmd(int cmd, int value, int tgid,
	unsigned long long ts, int op)
{
	struct FSTB_POLICY_CMD *iter;

	iter = fstb_get_policy_cmd(tgid, ts, op);
	if (iter) {
		if (cmd == 0)
			iter->self_ctrl_fps_enable = value;
		else if (cmd == 1)
			iter->notify_target_fps = value;
		else if (cmd == 2)
			iter->vsync_app_fps_disable = value;

		if (!op)
			fstb_delete_policy_cmd(iter);
	}
}

int switch_thread_max_fps(int pid, int set_max)
{
	struct FSTB_FRAME_INFO *iter;
	int ret = 0;

	mutex_lock(&fstb_lock);
	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid != pid)
			continue;

		iter->sbe_state = set_max;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
				set_max, "sbe_set_max_fps");
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
				iter->sbe_state, "sbe_state");
	}

	mutex_unlock(&fstb_lock);

	return ret;
}

static int switch_render_fps_range(char *proc_name,
	pid_t pid, int nr_level, struct fps_level *level)
{
	int ret = 0;
	int i;
	int mode;
	struct FSTB_RENDER_TARGET_FPS *rtfiter = NULL;
	struct hlist_node *n;

	if (nr_level > MAX_NR_RENDER_FPS_LEVELS || nr_level < 0)
		return -EINVAL;

	/* check if levels are interleaving */
	for (i = 0; i < nr_level; i++) {
		if (level[i].end > level[i].start ||
				(i > 0 && level[i].start > level[i - 1].end)) {
			return -EINVAL;
		}
	}

	if (proc_name != NULL && pid == 0)
		/*process mode*/
		mode = 0;
	else if (proc_name == NULL && pid > 0)
		/*thread mode*/
		mode = 1;
	else
		return -EINVAL;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(rtfiter, n, &fstb_render_target_fps, hlist) {
		if ((mode == 0 && !strncmp(
				proc_name, rtfiter->process_name, 16)) ||
				(mode == 1 && pid == rtfiter->pid)) {
			if (nr_level == 0) {
				/* delete render target fps*/
				hlist_del(&rtfiter->hlist);
				kfree(rtfiter);
			} else {
				/* reassign render target fps */
				rtfiter->nr_level = nr_level;
				memcpy(rtfiter->level, level,
					nr_level * sizeof(struct fps_level));
			}
			break;
		}
	}

	if (rtfiter == NULL && nr_level) {
		/* create new render target fps */
		struct FSTB_RENDER_TARGET_FPS *new_render_target_fps;

		new_render_target_fps =
			kzalloc(sizeof(*new_render_target_fps), GFP_KERNEL);
		if (new_render_target_fps == NULL) {
			ret = -ENOMEM;
			goto err;
		}

		if (mode == 0) {
			new_render_target_fps->pid = 0;
			if (!strncpy(
					new_render_target_fps->process_name,
					proc_name, 16)) {
				kfree(new_render_target_fps);
				ret = -ENOMEM;
				goto err;
			}
			new_render_target_fps->process_name[15] = '\0';
		} else if (mode == 1) {
			new_render_target_fps->pid = pid;
			new_render_target_fps->process_name[0] = '\0';
		}
		new_render_target_fps->nr_level = nr_level;
		memcpy(new_render_target_fps->level,
			level, nr_level * sizeof(struct fps_level));

		hlist_add_head(&new_render_target_fps->hlist,
			&fstb_render_target_fps);
	}

err:
	mutex_unlock(&fstb_lock);
	return ret;

}

int switch_process_fps_range(char *proc_name,
		int nr_level, struct fps_level *level)
{
	return switch_render_fps_range(proc_name, 0, nr_level, level);
}

int switch_thread_fps_range(pid_t pid, int nr_level, struct fps_level *level)
{
	return switch_render_fps_range(NULL, pid, nr_level, level);
}

static int cmplonglong(const void *a, const void *b)
{
	return *(long long *)a - *(long long *)b;
}

void fpsgo_ctrl2fstb_dfrc_fps(int fps)
{
	int tmp_dfrc_period;
	mutex_lock(&fstb_lock);

	if (fps <= CFG_MAX_FPS_LIMIT && fps >= CFG_MIN_FPS_LIMIT)
		dfps_ceiling = fps;
	tmp_dfrc_period = FSTB_USEC_DIVIDER / fps;
	if (dfrc_period != tmp_dfrc_period) {
		dfrc_period = tmp_dfrc_period;
		vsync_count = 0;
		vsync_duration_sum = 0;
	}

	mutex_unlock(&fstb_lock);
}

void fpsgo_ctrl2fstb_vsync(unsigned long long ts)
{
	unsigned long long vsync_duration, tmp;

	mutex_lock(&fstb_lock);

	// check VSync duration
	tmp = vsync_duration = div64_u64(ts - vsync_ts_last, 1000ULL);

	// calculate the VSync count by rounding the VSync duration
	tmp += (dfrc_period / 2);
	do_div(tmp, dfrc_period);
	vsync_count += tmp;

	vsync_duration_sum += vsync_duration;
	vsync_ts_last = ts;

	fpsgo_systrace_c_fstb(-100, 0, vsync_duration, "vsync_duration");
	fpsgo_systrace_c_fstb(-100, 0, vsync_duration_sum, "vsync_duration_sum");
	fpsgo_systrace_c_fstb(-100, 0, vsync_count, "vsync_count");

	mutex_unlock(&fstb_lock);
}

void gpu_time_update(long long t_gpu, unsigned int cur_freq,
		unsigned int cur_max_freq, u64 ulID)
{
	struct FSTB_FRAME_INFO *iter;

	ktime_t cur_time;
	long long cur_time_us;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return;
	}

	if (!fstb_active)
		fstb_active = 1;

	if (!fstb_active_dbncd) {
		fstb_active_dbncd = 1;
		switch_fstb_active();
	}

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->bufid == ulID)
			break;
	}

	if (iter == NULL) {
		mutex_unlock(&fstb_lock);
		return;
	}

	iter->gpu_time = t_gpu;
	iter->gpu_freq = cur_freq;

	if (iter->weighted_gpu_time_begin < 0 ||
	iter->weighted_gpu_time_end < 0 ||
	iter->weighted_gpu_time_begin > iter->weighted_gpu_time_end ||
	iter->weighted_gpu_time_end >= FRAME_TIME_BUFFER_SIZE) {

		/* purge all data */
		iter->weighted_gpu_time_begin = iter->weighted_gpu_time_end = 0;
	}

	/*get current time*/
	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	/*remove old entries*/
	while (iter->weighted_gpu_time_begin < iter->weighted_gpu_time_end) {
		if (iter->weighted_gpu_time_ts[iter->weighted_gpu_time_begin] <
				cur_time_us - FRAME_TIME_WINDOW_SIZE_US)
			iter->weighted_gpu_time_begin++;
		else
			break;
	}

	if (iter->weighted_gpu_time_begin == iter->weighted_gpu_time_end &&
	iter->weighted_gpu_time_end == FRAME_TIME_BUFFER_SIZE - 1)
		iter->weighted_gpu_time_begin = iter->weighted_gpu_time_end = 0;

	/*insert entries to weighted_gpu_time*/
	/*if buffer full --> move array align first*/
	if (iter->weighted_gpu_time_begin < iter->weighted_gpu_time_end &&
	iter->weighted_gpu_time_end == FRAME_TIME_BUFFER_SIZE - 1) {

		memmove(iter->weighted_gpu_time,
		&(iter->weighted_gpu_time[iter->weighted_gpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin));
		memmove(iter->weighted_gpu_time_ts,
		&(iter->weighted_gpu_time_ts[iter->weighted_gpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin));

		/*reset index*/
		iter->weighted_gpu_time_end =
		iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin;
		iter->weighted_gpu_time_begin = 0;
	}

	if (cur_max_freq > 0 && cur_max_freq >= cur_freq
			&& t_gpu > 0LL && t_gpu < 1000000000LL) {
		iter->weighted_gpu_time[iter->weighted_gpu_time_end] =
			t_gpu * cur_freq;
		do_div(iter->weighted_gpu_time[iter->weighted_gpu_time_end],
				cur_max_freq);
		iter->weighted_gpu_time_ts[iter->weighted_gpu_time_end] =
			cur_time_us;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
		(int)iter->weighted_gpu_time[iter->weighted_gpu_time_end],
		"weighted_gpu_time");
		iter->weighted_gpu_time_end++;
	}

	mtk_fstb_dprintk(
	"fstb: time %lld %lld t_gpu %lld cur_freq %u cur_max_freq %u\n",
	cur_time_us, ktime_to_us(ktime_get())-cur_time_us,
	t_gpu, cur_freq, cur_max_freq);

	fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, (int)t_gpu, "t_gpu");
	fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
			(int)cur_freq, "cur_gpu_cap");
	fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
			(int)cur_max_freq, "max_gpu_cap");

	mutex_unlock(&fstb_lock);
}

void eara2fstb_get_tfps(int max_cnt, int *is_camera, int *pid, unsigned long long *buf_id,
				int *tfps, int *rfps, int *hwui, char name[][16], int *proc_id)
{
	int count = 0;
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;

	mutex_lock(&fstb_lock);
	*is_camera = 0;

	hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
		if (count == max_cnt)
			break;

		if (!iter->target_fps || iter->target_fps == -1)
			continue;

		pid[count] = iter->pid;
		proc_id[count] = iter->proc_id;
		hwui[count] = iter->hwui_flag;
		buf_id[count] = iter->bufid;
		rfps[count] = iter->queue_fps;
		if (!iter->target_fps_notifying
			|| iter->target_fps_notifying == -1) {
			if (!iter->self_ctrl_fps_enable)
				tfps[count] = iter->target_fps;
			else
				tfps[count] = iter->target_fps_v2;
		}
		else
			tfps[count] = iter->target_fps_notifying;
		if (name)
			strncpy(name[count], iter->proc_name, 16);
		count++;
	}

	mutex_unlock(&fstb_lock);
}
EXPORT_SYMBOL(eara2fstb_get_tfps);

void eara2fstb_tfps_mdiff(int pid, unsigned long long buf_id, int diff,
					int tfps)
{
	int tmp_target_fps;
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
		if (pid == iter->pid && buf_id == iter->bufid) {
			if (!iter->self_ctrl_fps_enable)
				tmp_target_fps = iter->target_fps;
			else
				tmp_target_fps = iter->target_fps_v2;

			if (tfps != iter->target_fps_notifying
				&& tfps != tmp_target_fps)
				break;

			iter->target_fps_diff = diff;
			fpsgo_systrace_c_fstb_man(pid, buf_id, diff, "eara_diff");

			if (iter->target_fps_notifying
				&& tfps == iter->target_fps_notifying) {
				if (!iter->self_ctrl_fps_enable)
					iter->target_fps = iter->target_fps_notifying;
				else
					iter->target_fps_v2 = iter->target_fps_notifying;
				iter->target_fps_notifying = 0;
				fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->target_fps_v2, "fstb_target_fps1");
				fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
			}
			break;
		}
	}

	mutex_unlock(&fstb_lock);
}
EXPORT_SYMBOL(eara2fstb_tfps_mdiff);

int (*eara_pre_change_fp)(void);
EXPORT_SYMBOL(eara_pre_change_fp);
int (*eara_pre_change_single_fp)(int pid, unsigned long long bufID,
			int target_fps);
EXPORT_SYMBOL(eara_pre_change_single_fp);

static void fstb_change_tfps(struct FSTB_FRAME_INFO *iter, int target_fps,
		int notify_eara)
{
	int ret = -1;

	if (notify_eara && eara_pre_change_single_fp)
		ret = eara_pre_change_single_fp(iter->pid, iter->bufid, target_fps);

	if ((notify_eara && (ret == -1))
		|| iter->target_fps_notifying == target_fps) {
		if (!iter->self_ctrl_fps_enable)
			iter->target_fps = target_fps;
		else
			iter->target_fps_v2 = target_fps;
		iter->target_fps_notifying = 0;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
		fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->target_fps_v2, "fstb_target_fps1");
	} else {
		iter->target_fps_notifying = target_fps;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
			iter->target_fps_notifying, "fstb_notifying");
	}
}

int fpsgo_fbt2fstb_update_cpu_frame_info(
		int pid,
		unsigned long long bufID,
		int tgid,
		int frame_type,
		unsigned long long Q2Q_time,
		long long Runnging_time,
		int Target_time,
		unsigned int Curr_cap,
		unsigned int Max_cap,
		unsigned long long enqueue_length,
		unsigned long long dequeue_length)
{
	long long cpu_time_ns = (long long)Runnging_time;
	unsigned int max_current_cap = Curr_cap;
	unsigned int max_cpu_cap = Max_cap;
	unsigned long long wct = 0;
	int local_final_tfps, tolerence_fps;

	struct FSTB_FRAME_INFO *iter;

	ktime_t cur_time;
	long long cur_time_us;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	if (!fstb_active)
		fstb_active = 1;

	if (!fstb_active_dbncd) {
		fstb_active_dbncd = 1;
		switch_fstb_active();
	}

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (iter == NULL) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	mtk_fstb_dprintk(
	"pid %d Q2Q_time %lld Runnging_time %lld Curr_cap %u Max_cap %u\n",
	pid, Q2Q_time, Runnging_time, Curr_cap, Max_cap);

	if (iter->weighted_cpu_time_begin < 0 ||
		iter->weighted_cpu_time_end < 0 ||
		iter->weighted_cpu_time_begin > iter->weighted_cpu_time_end ||
		iter->weighted_cpu_time_end >= FRAME_TIME_BUFFER_SIZE) {

		/* purge all data */
		iter->weighted_cpu_time_begin = iter->weighted_cpu_time_end = 0;
	}

	/*get current time*/
	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	/*remove old entries*/
	while (iter->weighted_cpu_time_begin < iter->weighted_cpu_time_end) {
		if (iter->weighted_cpu_time_ts[iter->weighted_cpu_time_begin] <
				cur_time_us - FRAME_TIME_WINDOW_SIZE_US)
			iter->weighted_cpu_time_begin++;
		else
			break;
	}

	if (iter->weighted_cpu_time_begin == iter->weighted_cpu_time_end &&
		iter->weighted_cpu_time_end == FRAME_TIME_BUFFER_SIZE - 1)
		iter->weighted_cpu_time_begin = iter->weighted_cpu_time_end = 0;

	/*insert entries to weighted_cpu_time*/
	/*if buffer full --> move array align first*/
	if (iter->weighted_cpu_time_begin < iter->weighted_cpu_time_end &&
		iter->weighted_cpu_time_end == FRAME_TIME_BUFFER_SIZE - 1) {

		memmove(iter->weighted_cpu_time,
		&(iter->weighted_cpu_time[iter->weighted_cpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_cpu_time_end -
		 iter->weighted_cpu_time_begin));
		memmove(iter->weighted_cpu_time_ts,
		&(iter->weighted_cpu_time_ts[iter->weighted_cpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_cpu_time_end -
		 iter->weighted_cpu_time_begin));

		/*reset index*/
		iter->weighted_cpu_time_end =
		iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin;
		iter->weighted_cpu_time_begin = 0;
	}

	if (max_cpu_cap > 0 && Max_cap > Curr_cap
		&& cpu_time_ns > 0LL && cpu_time_ns < 1000000000LL) {
		wct = cpu_time_ns * max_current_cap;
		do_div(wct, max_cpu_cap);
	} else
		goto out;

	fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)wct,
		"weighted_cpu_time");

	iter->weighted_cpu_time[iter->weighted_cpu_time_end] =
		wct;

	iter->weighted_cpu_time_ts[iter->weighted_cpu_time_end] =
		cur_time_us;
	iter->weighted_cpu_time_end++;

out:
	mtk_fstb_dprintk(
	"pid %d fstb: time %lld %lld cpu_time_ns %lld max_current_cap %u max_cpu_cap %u\n"
	, pid, cur_time_us, ktime_to_us(ktime_get())-cur_time_us,
	cpu_time_ns, max_current_cap, max_cpu_cap);


	/* parse cpu time of each frame to ged_kpi */
	iter->cpu_time = cpu_time_ns;

	if (!test_bit(ADPF_TYPE, &iter->master_type)) {
		local_final_tfps = iter->self_ctrl_fps_enable ?
					iter->target_fps_v2 : iter->target_fps;
		tolerence_fps = iter->self_ctrl_fps_enable ?
					iter->target_fps_margin_v2 : iter->target_fps_margin;

		if (gpu_slowdown_check && !iter->target_fps_diff &&
				iter->cpu_time > Target_time && iter->cpu_time > iter->gpu_time)
			local_final_tfps = iter->target_fps;

		local_final_tfps = fstb_arbitrate_target_fps(local_final_tfps,
				&tolerence_fps, iter);
		fstb_post_process_target_fps(local_final_tfps, tolerence_fps, iter->target_fps_diff,
				&local_final_tfps, NULL, NULL, iter->vsync_app_fps_disable);

		ged_kpi_set_target_FPS_margin(iter->bufid, local_final_tfps, tolerence_fps,
			iter->target_fps_diff, iter->cpu_time);
	}

	if (fpsgo2msync_hint_frameinfo_fp)
		fpsgo2msync_hint_frameinfo_fp((unsigned int)iter->pid, iter->bufid,
			iter->target_fps, Q2Q_time, Q2Q_time - enqueue_length - dequeue_length);

	fpsgo_fstb2other_info_update(pid, bufID, FPSGO_Q2Q_TIME, 0, Q2Q_time, Curr_cap, 0);

	fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)cpu_time_ns, "t_cpu");
	fpsgo_systrace_c_fstb(pid, iter->bufid, (int)max_current_cap,
			"cur_cpu_cap");
	fpsgo_systrace_c_fstb(pid, iter->bufid, (int)max_cpu_cap,
			"max_cpu_cap");

	mutex_unlock(&fstb_lock);
	return 0;
}

static void fstb_calculate_target_fps(int tgid, int pid, unsigned long long bufID,
	int target_fps_margin, int target_fps_hint, int eara_is_active,
	unsigned long long cur_queue_end_ts)
{
	int local_tfps = 0, margin = 0;
	int target_fps_old = dfps_ceiling, target_fps_new = dfps_ceiling;
	struct FSTB_FRAME_INFO *iter;

	if (target_fps_hint) {
		local_tfps = target_fps_hint;
	} else {
		margin = target_fps_margin;
		local_tfps = fstb_enter_get_target_fps(pid, bufID, tgid,
			&margin, cur_queue_end_ts, eara_is_active);
	}

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (!iter)
		goto out;

	if (local_tfps <= 0) {
		fstb_change_tfps(iter, iter->target_fps, 1);
		fpsgo_main_trace("[fstb][%d][0x%llx] | back to v1 (%d)",
			iter->pid, iter->bufid, iter->target_fps);
	} else {
		target_fps_old = iter->target_fps_v2;
		target_fps_new = fstb_arbitrate_target_fps(local_tfps, &margin, iter);
		if (target_fps_old != target_fps_new)
			fstb_change_tfps(iter, target_fps_new, 1);
	}
	iter->target_fps_margin_v2 = margin;

	fpsgo_main_trace("[fstb][%d][0x%llx] | target_fps:%d(%d)(%d)(%d)",
		iter->pid, iter->bufid, iter->target_fps_v2,
		target_fps_old, target_fps_new, local_tfps);
	fpsgo_main_trace("[fstb][%d][0x%llx] | dfrc:%d eara:%d margin:%d",
		iter->pid, iter->bufid,
		dfps_ceiling, eara_is_active, iter->target_fps_margin_v2);

out:
	mutex_unlock(&fstb_lock);
}

static void fstb_notifier_wq_cb(struct work_struct *psWork)
{
	struct FSTB_NOTIFIER_PUSH_TAG *vpPush = NULL;

	vpPush = container_of(psWork, struct FSTB_NOTIFIER_PUSH_TAG, sWork);

	fstb_calculate_target_fps(vpPush->tgid, vpPush->pid, vpPush->bufid,
		vpPush->target_fps_margin, vpPush->target_fps_hint, vpPush->eara_is_active,
		vpPush->cur_queue_end_ts);

	kfree(vpPush);
}

void fpsgo_comp2fstb_prepare_calculate_target_fps(int pid, unsigned long long bufID,
	unsigned long long cur_queue_end_ts)
{
	int local_tfps_hint = 0;
	struct FSTB_FRAME_INFO *iter = NULL;
	struct FSTB_NOTIFIER_PUSH_TAG *vpPush = NULL;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (!iter || !iter->self_ctrl_fps_enable)
		goto out;

	if (iter->magt_target_fps > 0)
		local_tfps_hint = iter->magt_target_fps;
	else if (iter->notify_target_fps > 0)
		local_tfps_hint = iter->notify_target_fps;

	if (local_tfps_hint)
		fpsgo_main_trace("[fstb][%d][0x%llx] | target_fps hint %d %d",
			iter->pid, iter->bufid, iter->notify_target_fps, iter->magt_target_fps);

	vpPush =
		(struct FSTB_NOTIFIER_PUSH_TAG *)
		kmalloc(sizeof(struct FSTB_NOTIFIER_PUSH_TAG), GFP_ATOMIC);

	if (!vpPush)
		goto out;

	if (!wq) {
		kfree(vpPush);
		goto out;
	}

	vpPush->tgid = iter->proc_id;
	vpPush->pid = pid;
	vpPush->bufid = bufID;
	vpPush->target_fps_margin = iter->target_fps_margin_v2;
	vpPush->target_fps_hint = local_tfps_hint;
	vpPush->eara_is_active = iter->target_fps_diff ? 1 : 0;
	vpPush->cur_queue_end_ts = cur_queue_end_ts;

	INIT_WORK(&vpPush->sWork, fstb_notifier_wq_cb);
	queue_work(wq, &vpPush->sWork);

out:
	mutex_unlock(&fstb_lock);
}

static long long get_cpu_frame_time(struct FSTB_FRAME_INFO *iter)
{
	long long ret = INT_MAX;
	/*copy entries to temp array*/
	/*sort this array*/
	if (iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin > 0 &&
		iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin <
		FRAME_TIME_BUFFER_SIZE) {
		memcpy(iter->sorted_weighted_cpu_time,
		&(iter->weighted_cpu_time[iter->weighted_cpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_cpu_time_end -
		 iter->weighted_cpu_time_begin));
		sort(iter->sorted_weighted_cpu_time,
		iter->weighted_cpu_time_end -
		iter->weighted_cpu_time_begin,
		sizeof(long long), cmplonglong, NULL);
	}

	/*update nth value*/
	if (iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin) {
		if (
			iter->sorted_weighted_cpu_time[
				QUANTILE*
				(iter->weighted_cpu_time_end-
				 iter->weighted_cpu_time_begin)/100]
			> INT_MAX)
			ret = INT_MAX;
		else
			ret =
				iter->sorted_weighted_cpu_time[
					QUANTILE*
					(iter->weighted_cpu_time_end-
					 iter->weighted_cpu_time_begin)/100];
	} else
		ret = -1;

	fpsgo_systrace_c_fstb(iter->pid, iter->bufid, ret,
		"quantile_weighted_cpu_time");
	return ret;
}

static int get_gpu_frame_time(struct FSTB_FRAME_INFO *iter)
{
	int ret = INT_MAX;
	/*copy entries to temp array*/
	/*sort this array*/
	if (iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin
		> 0 &&
		iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin
		< FRAME_TIME_BUFFER_SIZE) {
		memcpy(iter->sorted_weighted_gpu_time,
		&(iter->weighted_gpu_time[iter->weighted_gpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin));
		sort(iter->sorted_weighted_gpu_time,
		iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin,
		sizeof(long long), cmplonglong, NULL);
	}

	/*update nth value*/
	if (iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin) {
		if (
			iter->sorted_weighted_gpu_time[
				QUANTILE*
				(iter->weighted_gpu_time_end-
				 iter->weighted_gpu_time_begin)/100]
			> INT_MAX)
			ret = INT_MAX;
		else
			ret =
				iter->sorted_weighted_gpu_time[
					QUANTILE*
					(iter->weighted_gpu_time_end-
					 iter->weighted_gpu_time_begin)/100];
	} else
		ret = -1;

	fpsgo_systrace_c_fstb(iter->pid, iter->bufid, ret,
			"quantile_weighted_gpu_time");
	return ret;
}

void fpsgo_comp2fstb_queue_time_update(int pid, unsigned long long bufID,
	int frame_type, unsigned long long ts,
	int api, int hwui_flag)
{
	unsigned long local_master_type = 0;
	struct FSTB_FRAME_INFO *iter;
	struct FSTB_POLICY_CMD *policy;
	ktime_t cur_time;
	long long cur_time_us = 0;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return;
	}

	if (!fstb_active)
		fstb_active = 1;

	if (!fstb_active_dbncd) {
		fstb_active_dbncd = 1;
		switch_fstb_active();
	}

	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && (iter->bufid == bufID ||
				iter->bufid == 0))
			break;
	}


	if (iter == NULL) {
		struct FSTB_FRAME_INFO *new_frame_info;

		set_bit(FPSGO_TYPE, &local_master_type);
		new_frame_info = add_new_frame_info(pid, bufID, hwui_flag, local_master_type);
		if (new_frame_info == NULL)
			goto out;

		iter = new_frame_info;
		hlist_add_head(&iter->hlist, &fstb_frame_infos);
	}

	if (iter->bufid == 0)
		iter->bufid = bufID;

	iter->hwui_flag = hwui_flag;

	if (wq_has_sleeper(&active_queue)) {
		condition_fstb_active = 1;
		wake_up_interruptible(&active_queue);
	}

	mutex_lock(&fstb_policy_cmd_lock);
	policy = fstb_get_policy_cmd(iter->proc_id, ts, 0);
	if (!policy)
		fstb_update_policy_cmd(iter, NULL, ts);
	else
		fstb_update_policy_cmd(iter, policy, ts);
	mutex_unlock(&fstb_policy_cmd_lock);

	if (iter->queue_time_begin < 0 ||
			iter->queue_time_end < 0 ||
			iter->queue_time_begin > iter->queue_time_end ||
			iter->queue_time_end >= FRAME_TIME_BUFFER_SIZE) {
		/* purge all data */
		iter->queue_time_begin = iter->queue_time_end = 0;
	}

	/*remove old entries*/
	while (iter->queue_time_begin < iter->queue_time_end) {
		if (iter->queue_time_ts[iter->queue_time_begin] < ts -
				(long long)FRAME_TIME_WINDOW_SIZE_US * 1000)
			iter->queue_time_begin++;
		else
			break;
	}

	if (iter->queue_time_begin == iter->queue_time_end &&
			iter->queue_time_end == FRAME_TIME_BUFFER_SIZE - 1)
		iter->queue_time_begin = iter->queue_time_end = 0;

	/*insert entries to weighted_display_time*/
	/*if buffer full --> move array align first*/
	if (iter->queue_time_begin < iter->queue_time_end &&
			iter->queue_time_end == FRAME_TIME_BUFFER_SIZE - 1) {
		memmove(iter->queue_time_ts,
		&(iter->queue_time_ts[iter->queue_time_begin]),
		sizeof(unsigned long long) *
		(iter->queue_time_end - iter->queue_time_begin));
		/*reset index*/
		iter->queue_time_end =
			iter->queue_time_end - iter->queue_time_begin;
		iter->queue_time_begin = 0;
	}

	iter->queue_time_ts[iter->queue_time_end] = ts;
	iter->queue_time_end++;

out:
	mutex_unlock(&fstb_lock);

	mutex_lock(&fstb_fps_active_time);
	if (cur_time_us)
		last_update_ts = cur_time_us;
	mutex_unlock(&fstb_fps_active_time);
}

static int fstb_get_queue_fps1(struct FSTB_FRAME_INFO *iter,
		long long interval, int *is_fps_update)
{
	int i = iter->queue_time_begin, j;
	unsigned long long avg_frame_interval = 0;
	unsigned long long retval = 0;
	int frame_count = 0;

	/* remove old entries */
	while (i < iter->queue_time_end) {
		if (iter->queue_time_ts[i] < sched_clock() - interval * 1000)
			i++;
		else
			break;
	}

	/* filter and asfc evaluation*/
	for (j = i + 1; j < iter->queue_time_end; j++) {
		avg_frame_interval +=
			(iter->queue_time_ts[j] -
			 iter->queue_time_ts[j - 1]);
		frame_count++;
	}

	*is_fps_update = frame_count ? 1 : 0;

	if (avg_frame_interval != 0) {
		retval = 1000000000ULL * frame_count;
		do_div(retval, avg_frame_interval);

		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, (int)retval, "queue_fps");
	} else
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, 0, "queue_fps");

	return retval;
}

static int fps_update(struct FSTB_FRAME_INFO *iter)
{
	int is_fps_update = 0;

	iter->queue_fps =
		fstb_get_queue_fps1(iter, FRAME_TIME_WINDOW_SIZE_US, &is_fps_update);

	return is_fps_update;
}

static int cal_target_fps(struct FSTB_FRAME_INFO *iter)
{
	long long target_limit;

	target_limit = iter->queue_fps;

	switch (margin_mode) {
	case 0:
		iter->target_fps_margin = 0;
		break;
	case 1:
		iter->target_fps_margin =
			target_limit >= dfps_ceiling ? 0 : fps_reset_tolerence;
		break;
	case 2:
		if (target_limit >= dfps_ceiling) {
			iter->target_fps_margin = 0;
			iter->target_fps_margin_dbnc_a =
				margin_mode_dbnc_a;
			iter->target_fps_margin_dbnc_b =
				margin_mode_dbnc_b;
		} else {
			if (iter->target_fps_margin_dbnc_a > 0) {
				iter->target_fps_margin = 0;
				iter->target_fps_margin_dbnc_a--;
			} else if (iter->target_fps_margin_dbnc_b > 0) {
				iter->target_fps_margin = fps_reset_tolerence;
				iter->target_fps_margin_dbnc_b--;
			} else {
				iter->target_fps_margin = 0;
				iter->target_fps_margin_dbnc_a =
					margin_mode_dbnc_a;
				iter->target_fps_margin_dbnc_b =
					margin_mode_dbnc_b;
			}
		}
		break;
	}

	return target_limit;
}

void fpsgo_fbt_ux2fstb_query_dfrc(int *fps, int *time)
{
	unsigned long long local_time = 1000000000ULL;

	mutex_lock(&fstb_lock);
	*fps = dfps_ceiling;
	*time = div64_u64(local_time, dfps_ceiling);
	mutex_unlock(&fstb_lock);
}

void fpsgo_fbt2fstb_query_fps(int pid, unsigned long long bufID,
		int *target_fps, int *target_fps_ori, int *target_cpu_time, int *fps_margin,
		int *quantile_cpu_time, int *quantile_gpu_time,
		int *target_fpks, int *cooler_on)
{
	struct FSTB_FRAME_INFO *iter = NULL;
	unsigned long long total_time;
	int local_tfps = 0;
	int tolerence_fps = 0;
	int local_final_tfps = dfps_ceiling;
	int local_final_tfpks = dfps_ceiling * 1000;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	*cooler_on = 0;

	if (!iter) {
		(*quantile_cpu_time) = -1;
		(*quantile_gpu_time) = -1;
		fstb_post_process_target_fps(dfps_ceiling, 0, 0,
			&local_final_tfps, &local_final_tfpks, &total_time, fstb_vsync_app_fps_disable);
	} else if (test_bit(ADPF_TYPE, &iter->master_type)) {
		local_final_tfpks = div64_u64(1000000000000ULL, iter->target_time);
		local_final_tfps = local_final_tfpks / 1000;
		tolerence_fps = 0;
		total_time = iter->target_time;
	} else {
		if (iter->target_fps_diff) {
			*cooler_on = 1;
			fpsgo_systrace_c_fstb_man(pid, iter->bufid,
				iter->target_fps_diff, "eara_diff");
		}

		(*quantile_cpu_time) = iter->quantile_cpu_time;
		(*quantile_gpu_time) = iter->quantile_gpu_time;

		local_tfps = iter->self_ctrl_fps_enable ?
				iter->target_fps_v2 : iter->target_fps;
		tolerence_fps = iter->self_ctrl_fps_enable ?
				iter->target_fps_margin_v2 : iter->target_fps_margin;
		local_tfps = fstb_arbitrate_target_fps(local_tfps, &tolerence_fps, iter);
		fstb_post_process_target_fps(local_tfps, tolerence_fps, iter->target_fps_diff,
			&local_final_tfps, &local_final_tfpks, &total_time, iter->vsync_app_fps_disable);
		iter->target_time = total_time;
	}

	if (target_fps)
		*target_fps = local_final_tfps;
	if (target_fpks)
		*target_fpks = local_final_tfpks;
	if (target_fps_ori)
		*target_fps_ori = local_tfps;
	if (fps_margin)
		*fps_margin = tolerence_fps;
	if (target_cpu_time)
		*target_cpu_time = total_time;

	mutex_unlock(&fstb_lock);
}

static int fpsgo_power2fstb_get_final_target_fps(struct FSTB_FRAME_INFO *iter)
{
	int ret = -1;
	struct FSTB_RENDER_TARGET_FPS *rtfiter;

	if (!iter)
		goto out;

	if (iter->self_ctrl_fps_enable) {
		ret = iter->target_fps_v2;
		goto out;
	}

	hlist_for_each_entry(rtfiter, &fstb_render_target_fps, hlist) {
		if (!strncmp(iter->proc_name, rtfiter->process_name, 16) ||
			rtfiter->pid == iter->pid) {
			ret = iter->target_fps;
			goto out;
		}
	}

	if (iter->notify_target_fps > 0 || iter->sbe_state == 1)
		ret = iter->target_fps;

out:
	return ret;
}

static int cmp_powerfps(const void *x1, const void *x2)
{
	const struct FSTB_POWERFPS_LIST *r1 = x1;
	const struct FSTB_POWERFPS_LIST *r2 = x2;

	if (r1->pid == 0)
		return 1;
	else if (r1->pid == -1)
		return 1;
	else if (r1->pid < r2->pid)
		return -1;
	else if (r1->pid == r2->pid && r1->fps < r2->fps)
		return -1;
	else if (r1->pid == r2->pid && r1->fps == r2->fps)
		return 0;
	return 1;
}

void fstb_cal_powerhal_fps(void)
{
	struct FSTB_FRAME_INFO *iter;
	int i = 0, j = 0;

	memset(powerfps_array, 0, 64 * sizeof(struct FSTB_POWERFPS_LIST));
	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (i < 0 || i >= 64)
			break;

		powerfps_array[i].pid = iter->proc_id;
		powerfps_array[i].fps = fpsgo_power2fstb_get_final_target_fps(iter);

		i++;
		if (i >= 64) {
			i = 63;
			break;
		}
	}

	if (i < 0 || i >= 64)
		return;

	powerfps_array[i].pid = -1;

	sort(powerfps_array, i, sizeof(struct FSTB_POWERFPS_LIST), cmp_powerfps, NULL);

	for (j = 0; j < i; j++) {
		if (powerfps_array[j].pid != powerfps_array[j + 1].pid) {
			mtk_fstb_dprintk("%s %d %d %d\n",
				__func__, j, powerfps_array[j].pid, powerfps_array[j].fps);
			fstb_sentcmd(powerfps_array[j].pid, powerfps_array[j].fps);
		}
	}

}

static void fstb_fps_stats(struct work_struct *work)
{
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;
	int local_tfps = dfps_ceiling;
	int idle = 1;
	int eara_ret = -1;

	if (work != &fps_stats_work)
		kfree(work);

	mutex_lock(&fstb_lock);
	if (vsync_count)
		vsync_period = div64_u64(vsync_duration_sum, vsync_count);
	else
		vsync_period = 1;
	vsync_count = 0;
	vsync_duration_sum = 0;

	if (gbe_fstb2gbe_poll_fp)
		gbe_fstb2gbe_poll_fp(&fstb_frame_infos);

	hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
		if (test_bit(ADPF_TYPE, &iter->master_type))
			continue;

		if (fps_update(iter)) {

			idle = 0;

			iter->quantile_cpu_time = (int)get_cpu_frame_time(iter);
			iter->quantile_gpu_time = (int)get_gpu_frame_time(iter);

			local_tfps = cal_target_fps(iter);
			local_tfps = fstb_arbitrate_target_fps(local_tfps,
					&iter->target_fps_margin, iter);
			if (!iter->self_ctrl_fps_enable) {
				if (iter->target_fps != local_tfps)
					fstb_change_tfps(iter, local_tfps, 1);
			} else
				iter->target_fps = local_tfps;

			fpsgo_systrace_c_fstb_man(iter->pid, 0,
					dfps_ceiling, "dfrc");
			fpsgo_systrace_c_fstb_man(iter->pid, 0,
					vsync_period, "vsync_period");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps, "fstb_target_fps1");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin, "target_fps_margin");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin_dbnc_a,
				"target_fps_margin_dbnc_a");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin_dbnc_b,
				"target_fps_margin_dbnc_b");

			mtk_fstb_dprintk(
			"%s pid:%d target_fps:%d\n",
			__func__, iter->pid,
			iter->target_fps);

			iter->render_idle_cnt = 0;
		} else {
			iter->render_idle_cnt++;
			if (iter->render_idle_cnt < FSTB_IDLE_DBNC) {
				iter->target_fps = fstb_arbitrate_target_fps(iter->target_fps,
							&iter->target_fps_margin, iter);
				mtk_fstb_dprintk(
						"%s pid:%d target_fps:%d\n",
						__func__, iter->pid,
						iter->target_fps);
				continue;
			}
		}
	}

	fstb_cal_powerhal_fps();

	/* check idle twice to avoid fstb_active ping-pong */
	if (idle)
		fstb_idle_cnt++;
	else
		fstb_idle_cnt = 0;

	if (fstb_idle_cnt >= FSTB_IDLE_DBNC) {
		fstb_active_dbncd = 0;
		fstb_idle_cnt = 0;
	} else if (fstb_idle_cnt >= 2) {
		fstb_active = 0;
	}

	if (fstb_enable && fstb_active_dbncd)
		enable_fstb_timer();
	else
		disable_fstb_timer();

	mutex_unlock(&fstb_lock);

	if (eara_pre_change_fp)
		eara_ret = eara_pre_change_fp();

	if (eara_ret == -1) {
		mutex_lock(&fstb_lock);
		hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
			if (!iter->target_fps_notifying
				|| iter->target_fps_notifying == -1)
				continue;
			if (!iter->self_ctrl_fps_enable)
				iter->target_fps = iter->target_fps_notifying;
			else
				iter->target_fps_v2 = iter->target_fps_notifying;
			iter->target_fps_notifying = 0;
			fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->target_fps_v2, "fstb_target_fps1");
		}
		mutex_unlock(&fstb_lock);
	}
}

int fpsgo_comp2fstb_do_recycle(void)
{
	int ret = 0;
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *h;

	mutex_lock(&fstb_lock);

	if (hlist_empty(&fstb_frame_infos)) {
		ret = 1;
		goto out;
	}

	hlist_for_each_entry_safe(iter, h, &fstb_frame_infos, hlist) {
		if (iter->render_idle_cnt >= FSTB_IDLE_DBNC) {
			fstb_enter_delete_render_info(iter->pid, iter->bufid);
			hlist_del(&iter->hlist);
			vfree(iter);
		}
	}

out:
	mutex_unlock(&fstb_lock);
	return ret;
}

int fpsgo_comp2fstb_adpf_set_target_time(int tgid, int rtid, unsigned long long bufID,
	unsigned long long target_time, int op)
{
	unsigned long local_master_type = 0;
	struct FSTB_FRAME_INFO *iter = NULL;
	struct hlist_node *h = NULL;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(iter, h, &fstb_frame_infos, hlist) {
		if (iter->pid == rtid && iter->bufid == bufID)
			break;
	}

	if (op == -1) {
		if (iter) {
			hlist_del(&iter->hlist);
			vfree(iter);
		}
		goto out;
	}

	if (!iter) {
		if (!op)
			goto malloc_err;

		set_bit(ADPF_TYPE, &local_master_type);
		iter = add_new_frame_info(rtid, bufID, RENDER_INFO_HWUI_NONE, local_master_type);
		if (!iter)
			goto malloc_err;
		iter->proc_id = tgid;

		hlist_add_head(&iter->hlist, &fstb_frame_infos);
	}

	iter->target_fps = div64_u64(1000000000ULL, target_time);
	iter->target_time = target_time;

	xgf_trace("[adpf][fstb][%d][0x%llx] | create target_time:%llu target_fps:%d",
		rtid, bufID, target_time, iter->target_fps);

out:
	mutex_unlock(&fstb_lock);
	return 0;

malloc_err:
	mutex_unlock(&fstb_lock);
	return -ENOMEM;
}

int fpsgo_ctrl2fstb_magt_set_target_fps(int *pid_arr, int *tid_arr, int *tfps_arr, int num)
{
	int i;
	int mode = 0;
	struct FSTB_FRAME_INFO *iter = NULL;
	struct hlist_node *h = NULL;

	if (num < 0)
		return -EINVAL;

	if (num > 0 && (!pid_arr || !tid_arr || !tfps_arr))
		return -ENOMEM;

	mutex_lock(&fstb_lock);

	if (num == 0) {
		hlist_for_each_entry_safe(iter, h, &fstb_frame_infos, hlist) {
			iter->magt_target_fps = 0;
			clear_bit(MAGT_TYPE, &iter->master_type);
		}
		goto out;
	}

	for (i = 0; i < num; i++) {
		fpsgo_main_trace("[fstb] %dth pid:%d tid:%d tfps:%d",
			i+1, pid_arr[i], tid_arr[i], tfps_arr[i]);

		if (tid_arr[i] > 0)
			mode = 1;
		else if (pid_arr[i] > 0)
			mode = 0;
		else
			continue;

		hlist_for_each_entry_safe(iter, h, &fstb_frame_infos, hlist) {
			if ((mode == 1 && iter->pid == tid_arr[i]) ||
				(mode == 0 && iter->proc_id == pid_arr[i])) {
				iter->magt_target_fps = tfps_arr[i];
				set_bit(MAGT_TYPE, &iter->master_type);
				fpsgo_main_trace("[fstb][%d][0x%llx] | magt_target_fps:%d",
					iter->pid, iter->bufid, iter->magt_target_fps);
			}
		}
	}

out:
	mutex_unlock(&fstb_lock);
	return 0;
}

int fpsgo_ktf2fstb_add_delete_render_info(int mode, int pid, unsigned long long bufID,
	int target_fps, int queue_fps)
{
	int ret = 0;
	unsigned long local_master_type = 0;
	struct FSTB_FRAME_INFO *iter = NULL;
	struct hlist_node *h = NULL;

	mutex_lock(&fstb_lock);

	set_bit(KTF_TYPE, &local_master_type);

	switch (mode) {
	case 0:
		iter = add_new_frame_info(pid, bufID, RENDER_INFO_HWUI_NONE, local_master_type);
		if (iter) {
			iter->target_fps = target_fps;
			iter->target_fps_v2 = target_fps;
			iter->queue_fps = queue_fps;
			hlist_add_head(&iter->hlist, &fstb_frame_infos);
			ret = 1;
		}
		break;

	case 1:
		hlist_for_each_entry_safe(iter, h, &fstb_frame_infos, hlist) {
			if (iter->pid == pid && iter->bufid == bufID) {
				hlist_del(&iter->hlist);
				vfree(iter);
				ret = 1;
				break;
			}
		}
		break;

	default:
		break;
	}

	mtk_fstb_dprintk_always("struct FSTB_FRAME_INFO size:%lu\n",
			sizeof(struct FSTB_FRAME_INFO));

	mutex_unlock(&fstb_lock);

	return ret;
}

int fpsgo_ktf2fstb_test_fstb_hrtimer_info_update(int *tmp_tid,
	unsigned long long *tmp_ts, int tmp_num,
	int *i_tid, unsigned long long *i_latest_ts, unsigned long long *i_diff,
	int *i_idx, int *i_idle, int i_num,
	int *o_tid, unsigned long long *o_latest_ts, unsigned long long *o_diff,
	int *o_idx, int *o_idle, int o_num, int o_ret)
{
	int ret = 0;

	WARN_ON(!test_fstb_hrtimer_info_update_fp);

	mutex_lock(&fstb_ko_lock);
	if (test_fstb_hrtimer_info_update_fp)
		ret = test_fstb_hrtimer_info_update_fp(tmp_tid, tmp_ts, tmp_num,
				i_tid, i_latest_ts, i_diff, i_idx, i_idle, i_num,
				o_tid, o_latest_ts, o_diff, o_idx, o_idle, o_num,
				o_ret);
	mutex_unlock(&fstb_ko_lock);

	return ret;
}
EXPORT_SYMBOL(fpsgo_ktf2fstb_test_fstb_hrtimer_info_update);

#define FSTB_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define FSTB_SYSFS_WRITE_VALUE(name, variable, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int arg; \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) { \
					mutex_lock(&fstb_lock); \
					(variable) = arg; \
					mutex_unlock(&fstb_lock); \
				} \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

#define FSTB_SYSFS_WRITE_POLICY_CMD(name, cmd, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int tgid; \
	int arg; \
	unsigned long long ts = fpsgo_get_time(); \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (sscanf(acBuffer, "%d %d", &tgid, &arg) == 2) { \
				mutex_lock(&fstb_policy_cmd_lock); \
				if (arg >= (min) && arg <= (max)) \
					fstb_set_policy_cmd(cmd, arg, tgid, ts, 1); \
				else \
					fstb_set_policy_cmd(cmd, 0, tgid, ts, 0); \
				mutex_unlock(&fstb_policy_cmd_lock); \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

FSTB_SYSFS_READ(set_render_max_fps, 0, 0);

static ssize_t set_render_max_fps_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;
	int ret = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) != 0)
				goto out;
			mtk_fstb_dprintk_always("%s %d\n", __func__, arg);
			fpsgo_systrace_c_fstb_man(arg > 0 ? arg : -arg,
				0, arg > 0, "force_max_fps");
			if (arg > 0)
				ret = switch_thread_max_fps(arg, 1);
			else
				ret = switch_thread_max_fps(-arg, 0);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(set_render_max_fps);

static ssize_t fstb_fps_list_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i;
	struct FSTB_RENDER_TARGET_FPS *rtfiter = NULL;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	hlist_for_each_entry(rtfiter, &fstb_render_target_fps, hlist) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%s %d %d ",
			rtfiter->process_name,
			rtfiter->pid,
			rtfiter->nr_level);
		pos += length;
		for (i = 0; i < rtfiter->nr_level; i++) {
			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"%d-%d ",
				rtfiter->level[i].start,
				rtfiter->level[i].end);
			pos += length;
		}
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"\n");
		pos += length;
	}

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static ssize_t fstb_fps_list_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	char *sepstr, *substr;
	char proc_name[16];
	int i;
	int  nr_level, start_fps, end_fps;
	int mode = 1;
	int pid = 0;
	int ret = 0;
	struct fps_level level[MAX_NR_RENDER_FPS_LEVELS];

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto err;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			acBuffer[count] = '\0';
			sepstr = acBuffer;

			substr = strsep(&sepstr, " ");
			if (!substr || !strncpy(proc_name, substr, 16)) {
				ret = -EINVAL;
				goto err;
			}
			proc_name[15] = '\0';

			if (kstrtoint(proc_name, 10, &pid) != 0)
				mode = 0; /* process mode*/

			substr = strsep(&sepstr, " ");

			if (!substr || kstrtoint(substr, 10, &nr_level) != 0 ||
					nr_level > MAX_NR_RENDER_FPS_LEVELS ||
					nr_level < 0) {
				ret = -EINVAL;
				goto err;
			}

			for (i = 0; i < nr_level; i++) {
				substr = strsep(&sepstr, " ");
				if (!substr) {
					ret = -EINVAL;
					goto err;
				}

				if (sscanf(substr, "%d-%d",
					&start_fps, &end_fps) != 2) {
					ret = -EINVAL;
					goto err;
				}
				level[i].start = start_fps;
				level[i].end = end_fps;
			}

			if (mode == 0) {
				if (switch_process_fps_range(proc_name,
					nr_level, level))
					ret = -EINVAL;
			} else {
				if (switch_thread_fps_range(pid,
					nr_level, level))
					ret = -EINVAL;
			}

		}
	}

err:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(fstb_fps_list);

FSTB_SYSFS_READ(margin_mode, 1, margin_mode);
FSTB_SYSFS_WRITE_VALUE(margin_mode, margin_mode, 0, 2);
static KOBJ_ATTR_RW(margin_mode);

FSTB_SYSFS_READ(margin_mode_dbnc_a, 1, margin_mode_dbnc_a);
FSTB_SYSFS_WRITE_VALUE(margin_mode_dbnc_a, margin_mode_dbnc_a, 0, 1000);
static KOBJ_ATTR_RW(margin_mode_dbnc_a);

FSTB_SYSFS_READ(margin_mode_dbnc_b, 1, margin_mode_dbnc_b);
FSTB_SYSFS_WRITE_VALUE(margin_mode_dbnc_b, margin_mode_dbnc_b, 0, 1000);
static KOBJ_ATTR_RW(margin_mode_dbnc_b);

FSTB_SYSFS_READ(fstb_reset_tolerence, 1, fps_reset_tolerence);
FSTB_SYSFS_WRITE_VALUE(fstb_reset_tolerence, fps_reset_tolerence, 0, 100);
static KOBJ_ATTR_RW(fstb_reset_tolerence);

FSTB_SYSFS_READ(fstb_tune_quantile, 1, QUANTILE);
FSTB_SYSFS_WRITE_VALUE(fstb_tune_quantile, QUANTILE, 0, 100);
static KOBJ_ATTR_RW(fstb_tune_quantile);

FSTB_SYSFS_READ(fstb_self_ctrl_fps_enable, 1, fstb_self_ctrl_fps_enable);
FSTB_SYSFS_WRITE_VALUE(fstb_self_ctrl_fps_enable, fstb_self_ctrl_fps_enable, 0, 1);
static KOBJ_ATTR_RW(fstb_self_ctrl_fps_enable);

FSTB_SYSFS_READ(fstb_vsync_app_fps_disable, 1, fstb_vsync_app_fps_disable);
FSTB_SYSFS_WRITE_VALUE(fstb_vsync_app_fps_disable, fstb_vsync_app_fps_disable, 0, 1);
static KOBJ_ATTR_RW(fstb_vsync_app_fps_disable);

FSTB_SYSFS_READ(fstb_no_r_timer_enable, 1, fstb_no_r_timer_enable);
FSTB_SYSFS_WRITE_VALUE(fstb_no_r_timer_enable, fstb_no_r_timer_enable, 0, 1);
static KOBJ_ATTR_RW(fstb_no_r_timer_enable);

FSTB_SYSFS_READ(gpu_slowdown_check, 1, gpu_slowdown_check);
FSTB_SYSFS_WRITE_VALUE(gpu_slowdown_check, gpu_slowdown_check, 0, 1);
static KOBJ_ATTR_RW(gpu_slowdown_check);

FSTB_SYSFS_READ(fstb_filter_poll_enable, 1, fstb_filter_poll_enable);
FSTB_SYSFS_WRITE_VALUE(fstb_filter_poll_enable, fstb_filter_poll_enable, 0, 1);
static KOBJ_ATTR_RW(fstb_filter_poll_enable);

static ssize_t fstb_debug_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_enable %d\n", fstb_enable);
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_log %d\n", fstb_fps_klog_on);
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_active %d\n", fstb_active);
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_active_dbncd %d\n", fstb_active_dbncd);
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_idle_cnt %d\n", fstb_idle_cnt);
	pos += length;

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static ssize_t fstb_debug_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int k_enable, klog_on;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d",
				&k_enable, &klog_on) >= 1) {
				if (k_enable == 0 || k_enable == 1)
					fpsgo_ctrl2fstb_switch_fstb(k_enable);

				if (klog_on == 0 || klog_on == 1)
					fstb_fps_klog_on = klog_on;
			}
		}
	}

out:
	kfree(acBuffer);
	return count;
}
static KOBJ_ATTR_RW(fstb_debug);


static ssize_t fpsgo_status_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct FSTB_FRAME_INFO *iter;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		goto out;
	}

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
	"tid\tbufID\t\tname\t\tcurrentFPS\ttargetFPS\tFPS_margin\tsbe_state\tHWUI\tt_gpu\t\tpolicy\n");
	pos += length;

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter) {
			length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
					"%d\t0x%llx\t%s\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t%lld\t\t(%d,%d,%d,%lu)\n",
					iter->pid,
					iter->bufid,
					iter->proc_name,
					iter->queue_fps > dfps_ceiling ?
					dfps_ceiling : iter->queue_fps,
					iter->self_ctrl_fps_enable ?
					iter->target_fps_v2 : iter->target_fps,
					iter->self_ctrl_fps_enable ?
					iter->target_fps_margin_v2 : iter->target_fps_margin,
					iter->sbe_state,
					iter->hwui_flag,
					iter->gpu_time,
					iter->self_ctrl_fps_enable,
					iter->notify_target_fps,
					iter->vsync_app_fps_disable,
					iter->master_type);
			pos += length;
		}
	}

	mutex_unlock(&fstb_lock);

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"dfps_ceiling:%d\n", dfps_ceiling);
	pos += length;

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}
static KOBJ_ATTR_ROO(fpsgo_status);

static ssize_t fstb_tfps_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 1;
	int pos = 0;
	int length = 0;
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *h = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(iter, h, &fstb_frame_infos, hlist) {
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\t[%d][0x%llx]\ttype:%lu\ttarget_fps:%d(%d,%d)\ttarget_time:%llu\n",
			i,
			iter->pid,
			iter->bufid,
			iter->master_type,
			iter->self_ctrl_fps_enable ? iter->target_fps_v2 : iter->target_fps,
			iter->notify_target_fps, iter->magt_target_fps,
			iter->target_time);
		pos += length;
		i++;
	}

	mutex_unlock(&fstb_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fstb_tfps_info);

static ssize_t fstb_policy_cmd_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 1;
	int pos = 0;
	int length = 0;
	struct FSTB_POLICY_CMD *iter;
	struct rb_root *rbr;
	struct rb_node *rbn;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fstb_policy_cmd_lock);

	rbr = &fstb_policy_cmd_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct FSTB_POLICY_CMD, rb_node);
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\ttgid:%d\tfstb_self_ctrl_fps_enable:%d\tnotify_target_fps:%d\tvsync_fps_disable:%d\tts:%llu\n",
			i,
			iter->tgid,
			iter->self_ctrl_fps_enable,
			iter->notify_target_fps,
			iter->vsync_app_fps_disable,
			iter->ts);
		pos += length;
		i++;
	}

	mutex_unlock(&fstb_policy_cmd_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fstb_policy_cmd);

static ssize_t fstb_frs_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 0;
	int pos = 0;
	int length = 0;
	struct FSTB_FRAME_INFO *iter = NULL;
	struct hlist_node *h = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(iter, h, &fstb_frame_infos, hlist) {
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\t%d\t0x%llx\t%d\t%d\n",
			i+1,
			iter->pid,
			iter->bufid,
			iter->self_ctrl_fps_enable ? iter->target_fps_v2 : iter->target_fps,
			iter->target_fps_diff);
		pos += length;
		i++;
	}

	mutex_unlock(&fstb_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fstb_frs_info);

// Powerhal use this to determine fstb_support
static ssize_t fstb_soft_level_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "1 %d-%d\n",
		CFG_MAX_FPS_LIMIT, CFG_MIN_FPS_LIMIT);
}

static KOBJ_ATTR_RO(fstb_soft_level);

FSTB_SYSFS_READ(fstb_self_ctrl_fps_enable_by_pid, 0, 0);
FSTB_SYSFS_WRITE_POLICY_CMD(fstb_self_ctrl_fps_enable_by_pid, 0, 1, 1);
static KOBJ_ATTR_RW(fstb_self_ctrl_fps_enable_by_pid);

FSTB_SYSFS_READ(notify_fstb_target_fps_by_pid, 0, 0);
FSTB_SYSFS_WRITE_POLICY_CMD(notify_fstb_target_fps_by_pid, 1, min_fps_limit, dfps_ceiling);
static KOBJ_ATTR_RW(notify_fstb_target_fps_by_pid);

FSTB_SYSFS_READ(fstb_vsync_app_fps_disable_by_pid, 0, 0);
FSTB_SYSFS_WRITE_POLICY_CMD(fstb_vsync_app_fps_disable_by_pid, 2, 1, 1);
static KOBJ_ATTR_RW(fstb_vsync_app_fps_disable_by_pid);


void init_fstb_callback(void)
{
	int i;

	mutex_lock(&fstb_info_callback_lock);

	for (i = 0; i < MAX_INFO_CALLBACK; i++) {
		q2q_notify_callback_list[i] = NULL;
		blc_notify_callback_list[i] = NULL;
	}

	mutex_unlock(&fstb_info_callback_lock);
}

int mtk_fstb_init(void)
{
	mtk_fstb_dprintk_always("init\n");
#if defined(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT)
	ged_kpi_output_gfx_info2_fp = gpu_time_update;
#endif

	if (!fpsgo_sysfs_create_dir(NULL, "fstb", &fstb_kobj)) {
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fpsgo_status);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_debug);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_tune_quantile);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_margin_mode_dbnc_b);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_margin_mode_dbnc_a);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_margin_mode);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_reset_tolerence);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_fps_list);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_set_render_max_fps);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_self_ctrl_fps_enable);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_no_r_timer_enable);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_gpu_slowdown_check);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_policy_cmd);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_self_ctrl_fps_enable_by_pid);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_notify_fstb_target_fps_by_pid);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_soft_level);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_tfps_info);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_frs_info);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_vsync_app_fps_disable);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_vsync_app_fps_disable_by_pid);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_filter_poll_enable);
	}

	wq = alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM | WQ_HIGHPRI, "mt_fstb");
	if (!wq)
		goto err;

	hrtimer_init(&hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt.function = &mt_fstb;

	fstb_policy_cmd_tree = RB_ROOT;

	init_fstb_callback();

	mtk_fstb_dprintk_always("init done\n");

	return 0;

err:
	return -1;
}

int __exit mtk_fstb_exit(void)
{
	mtk_fstb_dprintk("exit\n");

	disable_fstb_timer();

	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fpsgo_status);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_debug);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_tune_quantile);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_margin_mode_dbnc_b);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_margin_mode_dbnc_a);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_margin_mode);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_reset_tolerence);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_fps_list);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_set_render_max_fps);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_self_ctrl_fps_enable);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_no_r_timer_enable);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_gpu_slowdown_check);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_policy_cmd);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_self_ctrl_fps_enable_by_pid);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_notify_fstb_target_fps_by_pid);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_soft_level);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_tfps_info);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_frs_info);
	fpsgo_sysfs_remove_file(fstb_kobj,
				&kobj_attr_fstb_vsync_app_fps_disable);
	fpsgo_sysfs_remove_file(fstb_kobj,
				&kobj_attr_fstb_vsync_app_fps_disable_by_pid);
	fpsgo_sysfs_remove_file(fstb_kobj,
				&kobj_attr_fstb_filter_poll_enable);

	fpsgo_sysfs_remove_dir(&fstb_kobj);

	return 0;
}
