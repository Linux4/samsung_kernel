// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/security.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/sched/task.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include <linux/cpufreq.h>
#include "sugov/cpufreq.h"

#include <mt-plat/fpsgo_common.h>

#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_usedext.h"
#include "fps_composer.h"
#include "fbt_cpu.h"
#include "fbt_cpu_ux.h"
#include "fstb.h"
#include "xgf.h"
#include "mini_top.h"
#include "gbe2.h"

/*#define FPSGO_COM_DEBUG*/

#ifdef FPSGO_COM_DEBUG
#define FPSGO_COM_TRACE(...)	xgf_trace("fpsgo_com:" __VA_ARGS__)
#else
#define FPSGO_COM_TRACE(...)
#endif

#define COMP_TAG "FPSGO_COMP"
#define TIME_1MS  1000000
#define MAX_CONNECT_API_SIZE 20
#define MAX_FPSGO_CB_NUM 5

static struct kobject *comp_kobj;
static struct workqueue_struct *composer_wq;
static struct hrtimer recycle_hrt;
static struct rb_root connect_api_tree;
static struct rb_root fpsgo_com_policy_cmd_tree;
static int bypass_non_SF = 1;
static int fpsgo_control;
static int control_hwui;
/* EGL, CPU, CAMREA */
static int control_api_mask = 22;
static int recycle_idle_cnt;
static int recycle_active = 1;
static int fps_align_margin = 5;
static int total_fpsgo_com_policy_cmd_num;
static int fpsgo_is_boosting;
static int total_connect_api_info_num;
static unsigned long long last_update_sbe_dep_ts;

static void fpsgo_com_notify_to_do_recycle(struct work_struct *work);
static DECLARE_WORK(do_recycle_work, fpsgo_com_notify_to_do_recycle);
static DEFINE_MUTEX(recycle_lock);
static DEFINE_MUTEX(fpsgo_com_policy_cmd_lock);
static DEFINE_MUTEX(fpsgo_boost_cb_lock);
static DEFINE_MUTEX(fpsgo_boost_lock);
static DEFINE_MUTEX(adpf_hint_lock);

static fpsgo_notify_is_boost_cb notify_fpsgo_boost_cb_list[MAX_FPSGO_CB_NUM];

static enum hrtimer_restart prepare_do_recycle(struct hrtimer *timer)
{
	if (composer_wq)
		queue_work(composer_wq, &do_recycle_work);
	else
		schedule_work(&do_recycle_work);

	return HRTIMER_NORESTART;
}

void fpsgo_set_fpsgo_is_boosting(int is_boosting)
{
	mutex_lock(&fpsgo_boost_lock);
	fpsgo_is_boosting = is_boosting;
	mutex_unlock(&fpsgo_boost_lock);
}

int fpsgo_get_fpsgo_is_boosting(void)
{
	int is_boosting;

	mutex_lock(&fpsgo_boost_lock);
	is_boosting = fpsgo_is_boosting;
	mutex_unlock(&fpsgo_boost_lock);

	return is_boosting;
}

void init_fpsgo_is_boosting_callback(void)
{
	int i;

	mutex_lock(&fpsgo_boost_cb_lock);

	for (i = 0; i < MAX_FPSGO_CB_NUM; i++)
		notify_fpsgo_boost_cb_list[i] = NULL;

	mutex_unlock(&fpsgo_boost_cb_lock);
}

/*
 *	int register_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb)
 *	Users cannot use lock outside register function.
 */
int register_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb)
{
	int i, ret = 1;

	mutex_lock(&fpsgo_boost_cb_lock);

	for (i = 0; i < MAX_FPSGO_CB_NUM; i++) {
		if (notify_fpsgo_boost_cb_list[i] == NULL) {
			notify_fpsgo_boost_cb_list[i] = func_cb;
			ret = 0;
			break;
		}
	}
	if (ret)
		fpsgo_main_trace("[%s] Cannot register!, func: %p", __func__, func_cb);
	else
		notify_fpsgo_boost_cb_list[i](fpsgo_get_fpsgo_is_boosting());

	mutex_unlock(&fpsgo_boost_cb_lock);

	return ret;
}

int unregister_get_fpsgo_is_boosting(fpsgo_notify_is_boost_cb func_cb)
{
	int i, ret = 1;

	mutex_lock(&fpsgo_boost_cb_lock);

	for (i = 0; i < MAX_FPSGO_CB_NUM; i++) {
		if (notify_fpsgo_boost_cb_list[i] == func_cb) {
			notify_fpsgo_boost_cb_list[i] = NULL;
			ret = 0;
			break;
		}
	}
	if (ret) {
		fpsgo_main_trace("[%s] Cannot unregister!, func: %p",
			__func__, func_cb);
	}
	mutex_unlock(&fpsgo_boost_cb_lock);

	return ret;
}

int fpsgo_com2other_notify_fpsgo_is_boosting(int boost)
{
	int i, ret = 1;

	for (i = 0; i < MAX_FPSGO_CB_NUM; i++) {
		if (notify_fpsgo_boost_cb_list[i]) {
			notify_fpsgo_boost_cb_list[i](boost);
			fpsgo_main_trace("[%s] Call %d func_cb: %p",
				__func__, i, notify_fpsgo_boost_cb_list[i]);
		}
	}

	return ret;
}


void fpsgo_com_notify_fpsgo_is_boost(int enable)
{
	mutex_lock(&fpsgo_boost_cb_lock);
	mutex_lock(&fpsgo_boost_lock);
	if (!fpsgo_is_boosting && enable) {
		fpsgo_is_boosting = 1;
		fpsgo_com2other_notify_fpsgo_is_boosting(1);
		if (fpsgo_notify_fbt_is_boost_fp)
			fpsgo_notify_fbt_is_boost_fp(1);
	} else if (fpsgo_is_boosting && !enable) {
		fpsgo_is_boosting = 0;
		fpsgo_com2other_notify_fpsgo_is_boosting(0);
		if (fpsgo_notify_fbt_is_boost_fp)
			fpsgo_notify_fbt_is_boost_fp(0);
	}
	mutex_unlock(&fpsgo_boost_lock);
	mutex_unlock(&fpsgo_boost_cb_lock);
}

static inline int fpsgo_com_check_is_surfaceflinger(int pid)
{
	struct task_struct *tsk;
	int is_surfaceflinger = FPSGO_COM_IS_RENDER;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);

	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk)
		return FPSGO_COM_TASK_NOT_EXIST;

	if (strstr(tsk->comm, "surfaceflinger"))
		is_surfaceflinger = FPSGO_COM_IS_SF;
	put_task_struct(tsk);

	return is_surfaceflinger;
}

static void fpsgo_com_delete_policy_cmd(struct fpsgo_com_policy_cmd *iter)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct fpsgo_com_policy_cmd *tmp_iter = NULL, *min_iter = NULL;
	struct rb_node *rbn = NULL;

	if (iter) {
		if (iter->bypass_non_SF_by_pid == BY_PID_DEFAULT_VAL &&
			iter->control_api_mask_by_pid == BY_PID_DEFAULT_VAL &&
			iter->control_hwui_by_pid == BY_PID_DEFAULT_VAL &&
			iter->app_cam_fps_align_margin == BY_PID_DEFAULT_VAL &&
			iter->app_cam_time_align_ratio == BY_PID_DEFAULT_VAL &&
			iter->app_cam_meta_min_fps == BY_PID_DEFAULT_VAL) {
			min_iter = iter;
			goto delete;
		} else
			return;
	}

	if (RB_EMPTY_ROOT(&fpsgo_com_policy_cmd_tree))
		return;

	rbn = rb_first(&fpsgo_com_policy_cmd_tree);
	while (rbn) {
		tmp_iter = rb_entry(rbn, struct fpsgo_com_policy_cmd, rb_node);
		if (tmp_iter->ts < min_ts) {
			min_ts = tmp_iter->ts;
			min_iter = tmp_iter;
		}
		rbn = rb_next(rbn);
	}

	if (!min_iter)
		return;

delete:
	rb_erase(&min_iter->rb_node, &fpsgo_com_policy_cmd_tree);
	kfree(min_iter);
	total_fpsgo_com_policy_cmd_num--;
}

static struct fpsgo_com_policy_cmd *fpsgo_com_get_policy_cmd(int tgid,
	unsigned long long ts, int create)
{
	struct rb_node **p = &fpsgo_com_policy_cmd_tree.rb_node;
	struct rb_node *parent = NULL;
	struct fpsgo_com_policy_cmd *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct fpsgo_com_policy_cmd, rb_node);

		if (tgid < iter->tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter->tgid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!create)
		return NULL;

	iter = kzalloc(sizeof(struct fpsgo_com_policy_cmd), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->tgid = tgid;
	iter->bypass_non_SF_by_pid = BY_PID_DEFAULT_VAL;
	iter->control_api_mask_by_pid = BY_PID_DEFAULT_VAL;
	iter->control_hwui_by_pid = BY_PID_DEFAULT_VAL;
	iter->app_cam_fps_align_margin = BY_PID_DEFAULT_VAL;
	iter->app_cam_time_align_ratio = BY_PID_DEFAULT_VAL;
	iter->app_cam_meta_min_fps = BY_PID_DEFAULT_VAL;
	iter->ts = ts;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &fpsgo_com_policy_cmd_tree);
	total_fpsgo_com_policy_cmd_num++;

	if (total_fpsgo_com_policy_cmd_num > FPSGO_MAX_TREE_SIZE)
		fpsgo_com_delete_policy_cmd(NULL);

	return iter;
}

static void fpsgo_com_set_policy_cmd(int cmd, int value, int tgid,
	unsigned long long ts, int op)
{
	struct fpsgo_com_policy_cmd *iter = NULL;

	iter = fpsgo_com_get_policy_cmd(tgid, ts, op);
	if (iter) {
		if (cmd == 0)
			iter->bypass_non_SF_by_pid = value;
		else if (cmd == 1)
			iter->control_api_mask_by_pid = value;
		else if (cmd == 2)
			iter->control_hwui_by_pid = value;
		else if (cmd == 3)
			iter->app_cam_fps_align_margin = value;
		else if (cmd == 4)
			iter->app_cam_time_align_ratio = value;

		if (!op)
			fpsgo_com_delete_policy_cmd(iter);
	}
}

void fpsgo_com_clear_connect_api_render_list(
	struct connect_api_info *connect_api)
{
	struct render_info *pos, *next;

	fpsgo_lockprove(__func__);

	list_for_each_entry_safe(pos, next,
		&connect_api->render_list, bufferid_list) {
		fpsgo_delete_render_info(pos->pid,
			pos->buffer_id, pos->identifier);
	}

}

static void fpsgo_com_delete_oldest_connect_api_info(void)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct connect_api_info *iter = NULL, *min_iter = NULL;
	struct rb_node *rbn = NULL;

	for (rbn = rb_first(&connect_api_tree); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct connect_api_info, rb_node);
		if (iter->ts < min_ts) {
			min_ts = iter->ts;
			min_iter = iter;
		}
	}

	if (min_iter) {
		fpsgo_com_clear_connect_api_render_list(min_iter);
		rb_erase(&min_iter->rb_node, &connect_api_tree);
		kfree(min_iter);
		total_connect_api_info_num--;
	}
}

struct connect_api_info *fpsgo_com_search_and_add_connect_api_info(int pid,
	unsigned long long buffer_id, int force)
{
	struct rb_node **p = &connect_api_tree.rb_node;
	struct rb_node *parent = NULL;
	struct connect_api_info *tmp = NULL;
	unsigned long long buffer_key;
	int tgid;

	fpsgo_lockprove(__func__);

	tgid = fpsgo_get_tgid(pid);
	buffer_key = (buffer_id & 0xFFFFFFFFFFFF)
		| (((unsigned long long)tgid) << 48);
	FPSGO_COM_TRACE("%s key:%llu tgid:%d buffer_id:%llu",
		__func__, buffer_key, tgid, buffer_id);
	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct connect_api_info, rb_node);

		if (buffer_key < tmp->buffer_key)
			p = &(*p)->rb_left;
		else if (buffer_key > tmp->buffer_key)
			p = &(*p)->rb_right;
		else
			return tmp;
	}

	if (!force)
		return NULL;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;

	INIT_LIST_HEAD(&(tmp->render_list));

	tmp->pid = pid;
	tmp->tgid = tgid;
	tmp->buffer_id = buffer_id;
	tmp->buffer_key = buffer_key;

	FPSGO_LOGI("Connect API! pid=%d, tgid=%d, buffer_id=%llu",
		pid, tgid, buffer_id);

	rb_link_node(&tmp->rb_node, parent, p);
	rb_insert_color(&tmp->rb_node, &connect_api_tree);
	total_connect_api_info_num++;

	return tmp;
}

/*
 * UX: sbe_ctrl
 * VP: control_hwui
 * Camera: fpsgo_control
 * Game: fpsgo_control_pid
 */
int fpsgo_com_check_frame_type(int pid, int tgid, int queue_SF, int api,
	int hwui, int sbe_ctrl, int fpsgo_control_pid)
{
	int local_bypass_non_SF = bypass_non_SF;
	int local_control_api_mask = control_api_mask;
	int local_control_hwui = control_hwui;
	struct fpsgo_com_policy_cmd *iter = NULL;

	mutex_lock(&fpsgo_com_policy_cmd_lock);
	iter = fpsgo_com_get_policy_cmd(tgid, 0, 0);
	if (iter) {
		if (iter->bypass_non_SF_by_pid != BY_PID_DEFAULT_VAL)
			local_bypass_non_SF = iter->bypass_non_SF_by_pid;
		if (iter->control_api_mask_by_pid != BY_PID_DEFAULT_VAL)
			local_control_api_mask = iter->control_api_mask_by_pid;
		if (iter->control_hwui_by_pid != BY_PID_DEFAULT_VAL)
			local_control_hwui = iter->control_hwui_by_pid;
	}
	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	if (sbe_ctrl)
		return NON_VSYNC_ALIGNED_TYPE;

	if (local_bypass_non_SF && !queue_SF)
		return BY_PASS_TYPE;

	if ((local_control_api_mask & (1 << api)) == 0)
		return BY_PASS_TYPE;

	if (hwui == RENDER_INFO_HWUI_TYPE) {
		if (local_control_hwui)
			return NON_VSYNC_ALIGNED_TYPE;
		else
			return BY_PASS_TYPE;
	}

	if (!fpsgo_control && !fpsgo_control_pid)
		return BY_PASS_TYPE;

	if (pid == tgid)
		return BY_PASS_TYPE;

	return NON_VSYNC_ALIGNED_TYPE;
}

static int fpsgo_com_check_fps_align(int pid, unsigned long long buffer_id)
{
	int ret = NON_VSYNC_ALIGNED_TYPE;
	int local_tgid = 0;
	int local_app_meta_min_fps = 0;
	int local_qfps_arr_num = 0;
	int local_tfps_arr_num = 0;
	int *local_qfps_arr = NULL;
	int *local_tfps_arr = NULL;
	struct fpsgo_com_policy_cmd *iter = NULL;

	local_qfps_arr = kcalloc(1, sizeof(int), GFP_KERNEL);
	if (!local_qfps_arr)
		goto out;

	local_tfps_arr = kcalloc(1, sizeof(int), GFP_KERNEL);
	if (!local_tfps_arr)
		goto out;

	local_tgid = fpsgo_get_tgid(pid);
	mutex_lock(&fpsgo_com_policy_cmd_lock);
	iter = fpsgo_com_get_policy_cmd(local_tgid, 0, 0);
	if (iter)
		local_app_meta_min_fps = iter->app_cam_meta_min_fps;
	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	fpsgo_other2fstb_get_fps(pid, buffer_id,
		local_qfps_arr, &local_qfps_arr_num, 1,
		local_tfps_arr, &local_tfps_arr_num, 1);

	if (local_qfps_arr[0] > local_tfps_arr[0] + fps_align_margin ||
		(local_app_meta_min_fps > 0 && local_qfps_arr[0] <= local_app_meta_min_fps)) {
		ret = BY_PASS_TYPE;
		fpsgo_systrace_c_fbt_debug(pid, buffer_id, 1, "fps_no_align");
	} else
		fpsgo_systrace_c_fbt_debug(pid, buffer_id, 0, "fps_no_align");

	xgf_trace("[comp][%d][0x%llx] | %s local_qfps:%d local_tfps:%d",
		pid, buffer_id, __func__, local_qfps_arr[0], local_tfps_arr[0]);

out:
	kfree(local_qfps_arr);
	kfree(local_tfps_arr);
	return ret;
}

static int fpsgo_com_check_app_cam_fps_align(struct render_info *iter)
{
	int ret = 0;
	int local_app_cam_fps_align_margin = BY_PID_DEFAULT_VAL;
	int local_app_cam_time_align_ratio = BY_PID_DEFAULT_VAL;
	int check1 = 0;
	int check2 = 0;
	int check3 = 0;
	int local_qfps_arr_num = 0;
	int local_tfps_arr_num = 0;
	int *local_qfps_arr = NULL;
	int *local_tfps_arr = NULL;
	struct fpsgo_com_policy_cmd *policy = NULL;

	if (!iter)
		goto out;

	mutex_lock(&fpsgo_com_policy_cmd_lock);
	policy = fpsgo_com_get_policy_cmd(iter->tgid, 0, 0);
	if (policy) {
		if (policy->app_cam_fps_align_margin != BY_PID_DEFAULT_VAL)
			local_app_cam_fps_align_margin = policy->app_cam_fps_align_margin;
		if (policy->app_cam_time_align_ratio != BY_PID_DEFAULT_VAL)
			local_app_cam_time_align_ratio = policy->app_cam_time_align_ratio;
	}
	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	if (local_app_cam_fps_align_margin < 0 || local_app_cam_time_align_ratio < 0)
		goto out;

	local_qfps_arr = kcalloc(1, sizeof(int), GFP_KERNEL);
	if (!local_qfps_arr)
		goto out;

	local_tfps_arr = kcalloc(1, sizeof(int), GFP_KERNEL);
	if (!local_tfps_arr)
		goto out;

	fpsgo_other2fstb_get_fps(iter->pid, iter->buffer_id,
		local_qfps_arr, &local_qfps_arr_num, 1,
		local_tfps_arr, &local_tfps_arr_num, 1);

	check1 = local_tfps_arr[0] - local_qfps_arr[0] > local_app_cam_fps_align_margin;
	check2 = iter->running_time * 100 <= iter->Q2Q_time * local_app_cam_time_align_ratio;
	check3 = local_tfps_arr[0] - local_qfps_arr[0] <= 1;

	if (check1 && check2)
		iter->eas_control_flag = 1;
	else if (check3)
		iter->eas_control_flag = 0;
	ret = iter->eas_control_flag;

	xgf_trace(
		"[base][%d][0x%llx] | %s qfps:%d tfps:%d t_cpu:%llu q2q:%llu margin:%d ratio:%d check:%d %d %d eas_ctrl:%d",
		iter->pid, iter->buffer_id, __func__,
		local_qfps_arr[0], local_tfps_arr[0],
		iter->running_time, iter->Q2Q_time,
		local_app_cam_fps_align_margin, local_app_cam_time_align_ratio,
		check1, check2, check3, iter->eas_control_flag);

out:
	kfree(local_qfps_arr);
	kfree(local_tfps_arr);
	return ret;
}

static int fpsgo_com_check_BQ_type(int *bq_type,
	int pid, unsigned long long buffer_id)
{
	int ret = BY_PASS_TYPE;
	int local_bq_type = ACQUIRE_UNKNOWN_TYPE;
	int tmp_dep_arr_num = 0;
	int local_dep_arr_num = 0;
	int *local_dep_arr = NULL;

	if (!bq_type)
		return ret;

	local_bq_type = fpsgo_get_acquire_queue_pair_by_self(pid, buffer_id);
	if (local_bq_type == ACQUIRE_SELF_TYPE) {
		fpsgo_systrace_c_fbt(pid, buffer_id, local_bq_type, "bypass_acquire");
		goto out;
	}

	local_dep_arr_num = fpsgo_comp2xgf_get_dep_list_num(pid, buffer_id);
	local_dep_arr = kcalloc(local_dep_arr_num, sizeof(int), GFP_KERNEL);
	if (!local_dep_arr) {
		xgf_trace("[comp][%d][0x%llx] | %s dep_arr malloc err",
			pid, buffer_id, __func__);
		goto out;
	}

	tmp_dep_arr_num = fpsgo_comp2xgf_get_dep_list(pid, local_dep_arr_num,
		local_dep_arr, buffer_id);
	if (tmp_dep_arr_num != local_dep_arr_num) {
		xgf_trace("[comp][%d][0x%llx] | %s dep_arr_num err %d!=%d",
			pid, buffer_id, __func__, local_dep_arr_num, tmp_dep_arr_num);
		kfree(local_dep_arr);
		goto out;
	}

	local_bq_type = fpsgo_get_acquire_queue_pair_by_group(pid,
			local_dep_arr, local_dep_arr_num, buffer_id);
	kfree(local_dep_arr);
	if (local_bq_type == ACQUIRE_CAMERA_TYPE)
		goto out;

	if (fpsgo_other2fstb_check_cam_do_frame())
		local_bq_type = fpsgo_check_all_render_blc(pid, buffer_id);
	if (local_bq_type == ACQUIRE_OTHER_TYPE)
		fpsgo_systrace_c_fbt(pid, buffer_id, local_bq_type, "bypass_acquire");

out:
	*bq_type = local_bq_type;
	ret = (local_bq_type == ACQUIRE_CAMERA_TYPE) ?
			NON_VSYNC_ALIGNED_TYPE : BY_PASS_TYPE;
	if (ret == NON_VSYNC_ALIGNED_TYPE)
		ret = fpsgo_com_check_fps_align(pid, buffer_id);
	return ret;
}

int fpsgo_com_update_render_api_info(struct render_info *f_render)
{
	struct connect_api_info *connect_api;

	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(f_render->thr_mlock));

	connect_api =
		fpsgo_com_search_and_add_connect_api_info(f_render->pid,
				f_render->buffer_id, 0);

	if (!connect_api) {
		FPSGO_COM_TRACE("no pair connect api pid[%d] buffer_id[%llu]",
			f_render->pid, f_render->buffer_id);
		return 0;
	}

	f_render->api = connect_api->api;
	connect_api->ts = fpsgo_get_time();
	list_add(&(f_render->bufferid_list), &(connect_api->render_list));
	FPSGO_COM_TRACE("add connect api pid[%d] key[%llu] buffer_id[%llu]",
		connect_api->pid, connect_api->buffer_key,
		connect_api->buffer_id);
	return 1;
}

static int fpsgo_com_refetch_buffer(struct render_info *f_render, int pid,
		unsigned long long identifier, int enqueue)
{
	int ret;
	unsigned long long buffer_id = 0;
	int queue_SF = 0;

	if (!f_render)
		return 0;

	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(f_render->thr_mlock));

	ret = fpsgo_get_BQid_pair(pid, f_render->tgid,
		identifier, &buffer_id, &queue_SF, enqueue);
	if (!ret || !buffer_id) {
		FPSGO_LOGI("refetch %d: %llu, %d, %llu\n",
			pid, buffer_id, queue_SF, identifier);
		fpsgo_main_trace("COMP: refetch %d: %llu, %d, %llu\n",
			pid, buffer_id, queue_SF, identifier);
		return 0;
	}

	f_render->buffer_id = buffer_id;
	f_render->queue_SF = queue_SF;

	if (!f_render->p_blc)
		fpsgo_base2fbt_node_init(f_render);

	FPSGO_COM_TRACE("%s: refetch %d: %llu, %llu, %d\n", __func__,
				pid, identifier, buffer_id, queue_SF);

	return 1;
}

void fpsgo_ctrl2comp_enqueue_start(int pid,
	unsigned long long enqueue_start_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	int check_render;
	int ret;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 1);

	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		FPSGO_COM_TRACE("%s: store frame info fail : %d !!!!\n",
			__func__, pid);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	/* @buffer_id and @queue_SF MUST be initialized
	 * with @api at the same time
	 */
	if (!f_render->api && identifier) {
		ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 1);
		if (!ret) {
			goto exit;
		}

		ret = fpsgo_com_update_render_api_info(f_render);
		if (!ret) {
			goto exit;
		}
	} else if (identifier) {
		ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 1);
		if (!ret) {
			goto exit;
		}
	}

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->tgid, f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
		f_render->t_enqueue_start = enqueue_start_time;
		FPSGO_COM_TRACE(
			"pid[%d] type[%d] enqueue_s:%llu",
			pid, f_render->frame_type,
			enqueue_start_time);
		FPSGO_COM_TRACE("update pid[%d] tgid[%d] buffer_id:%llu api:%d",
			f_render->pid, f_render->tgid,
			f_render->buffer_id, f_render->api);
		fpsgo_com_notify_fpsgo_is_boost(1);
		break;
	case BY_PASS_TYPE:
		f_render->t_enqueue_start = enqueue_start_time;
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->queue_SF, "bypass_sf");
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->api, "bypass_api");
		fpsgo_systrace_c_fbt(pid, f_render->buffer_id,
			f_render->hwui, "bypass_hwui");
		break;
	default:
		FPSGO_COM_TRACE("type not found pid[%d] type[%d]",
			pid, f_render->frame_type);
		break;
	}
exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);

	mutex_lock(&recycle_lock);
	if (recycle_idle_cnt) {
		recycle_idle_cnt = 0;
		if (!recycle_active) {
			recycle_active = 1;
			hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);
		}
	}
	mutex_unlock(&recycle_lock);
}

void fpsgo_ctrl2comp_enqueue_end(int pid,
	unsigned long long enqueue_end_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	struct hwui_info *h_info;
	struct sbe_info *s_info;
	struct fps_control_pid_info *f_info;
	int check_render;
	unsigned long long running_time = 0, raw_runtime = 0;
	unsigned long long enq_running_time = 0;
	int ret;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;


	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		FPSGO_COM_TRACE("%s: NON pair frame info : %d !!!!\n",
			__func__, pid);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 0);
	if (!ret) {
		goto exit;
	}

	/* hwui */
	h_info = fpsgo_search_and_add_hwui_info(f_render->pid, 0);
	if (h_info)
		f_render->hwui = RENDER_INFO_HWUI_TYPE;
	else
		f_render->hwui = RENDER_INFO_HWUI_NONE;

	/* sbe */
	s_info = fpsgo_search_and_add_sbe_info(f_render->pid, 0);
	if (s_info)
		f_render->sbe_control_flag = 1;
	else
		f_render->sbe_control_flag = 0;

	f_info = fpsgo_search_and_add_fps_control_pid(f_render->tgid, 0);
	if (f_info) {
		f_render->control_pid_flag = 1;
		f_info->ts = enqueue_end_time;
	} else
		f_render->control_pid_flag = 0;

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->tgid, f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	if (fpsgo_get_acquire_hint_is_enable()) {
		if (!f_render->sbe_control_flag &&
			f_render->hwui == RENDER_INFO_HWUI_NONE &&
			f_render->frame_type == NON_VSYNC_ALIGNED_TYPE &&
			fpsgo_check_is_cam_apk(f_render->tgid)) {
			f_render->frame_type = fpsgo_com_check_BQ_type(&f_render->bq_type,
						pid, f_render->buffer_id);
			if (f_render->frame_type == NON_VSYNC_ALIGNED_TYPE)
				f_render->frame_type = fpsgo_check_exist_queue_SF(f_render->tgid);
			if (f_render->frame_type == NON_VSYNC_ALIGNED_TYPE) {
				if (fpsgo_com_check_app_cam_fps_align(f_render)) {
					fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
						&raw_runtime, &running_time, &enq_running_time,
						0, enqueue_end_time,
						f_render->t_dequeue_start, f_render->t_dequeue_end,
						f_render->t_enqueue_start, enqueue_end_time, 0);
					f_render->raw_runtime = raw_runtime;
					f_render->running_time = running_time;
					f_render->frame_type = BY_PASS_TYPE;
					fpsgo_systrace_c_fbt_debug(f_render->pid, f_render->buffer_id,
						1, "app_cam_no_align");
				} else
					fpsgo_systrace_c_fbt_debug(f_render->pid, f_render->buffer_id,
						0, "app_cam_no_align");
			}
		}
	}

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
		if (f_render->t_enqueue_end)
			f_render->Q2Q_time =
				enqueue_end_time - f_render->t_enqueue_end;
		f_render->t_enqueue_end = enqueue_end_time;
		f_render->enqueue_length =
			enqueue_end_time - f_render->t_enqueue_start;
		FPSGO_COM_TRACE(
			"pid[%d] type[%d] enqueue_e:%llu enqueue_l:%llu",
			pid, f_render->frame_type,
			enqueue_end_time, f_render->enqueue_length);

		fpsgo_comp2fstb_prepare_calculate_target_fps(pid, f_render->buffer_id,
			enqueue_end_time);

		fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
			&raw_runtime, &running_time, &enq_running_time,
			0, f_render->t_enqueue_end,
			f_render->t_dequeue_start, f_render->t_dequeue_end,
			f_render->t_enqueue_start, f_render->t_enqueue_end, 0);
		f_render->enqueue_length_real = f_render->enqueue_length > enq_running_time ?
						f_render->enqueue_length - enq_running_time : 0;
		fpsgo_systrace_c_fbt_debug(pid, f_render->buffer_id,
			f_render->enqueue_length_real, "enq_length_real");
		f_render->raw_runtime = raw_runtime;
		if (running_time != 0)
			f_render->running_time = running_time;

		fpsgo_comp2fbt_frame_start(f_render,
				enqueue_end_time);
		fpsgo_comp2fstb_queue_time_update(pid,
			f_render->buffer_id,
			f_render->frame_type,
			enqueue_end_time,
			f_render->api,
			f_render->hwui);
		fpsgo_comp2minitop_queue_update(enqueue_end_time);
		fpsgo_comp2gbe_frame_update(f_render->pid, f_render->buffer_id);

		fpsgo_systrace_c_fbt_debug(-300, 0, f_render->enqueue_length,
			"%d_%d-enqueue_length", pid, f_render->frame_type);
		break;
	case BY_PASS_TYPE:
		if (f_render->t_enqueue_end)
			f_render->Q2Q_time =
				enqueue_end_time - f_render->t_enqueue_end;
		f_render->t_enqueue_end = enqueue_end_time;
		f_render->enqueue_length =
			enqueue_end_time - f_render->t_enqueue_start;
		fbt_set_render_last_cb(f_render, enqueue_end_time);
		FPSGO_COM_TRACE(
			"pid[%d] type[%d] enqueue_e:%llu enqueue_l:%llu",
			pid, f_render->frame_type,
			enqueue_end_time, f_render->enqueue_length);

		fpsgo_comp2xgf_qudeq_notify(pid, f_render->buffer_id,
			&raw_runtime, &running_time, &enq_running_time,
			0, f_render->t_enqueue_end,
			f_render->t_dequeue_start, f_render->t_dequeue_end,
			f_render->t_enqueue_start, f_render->t_enqueue_end, 1);
		f_render->enqueue_length_real = f_render->enqueue_length;
		fpsgo_systrace_c_fbt_debug(pid, f_render->buffer_id,
			f_render->enqueue_length_real, "enq_length_real");

		fpsgo_comp2fstb_queue_time_update(pid,
			f_render->buffer_id,
			f_render->frame_type,
			enqueue_end_time,
			f_render->api,
			f_render->hwui);
		fpsgo_stop_boost_by_render(f_render);
		break;
	default:
		FPSGO_COM_TRACE("type not found pid[%d] type[%d]",
			pid, f_render->frame_type);
		break;
	}

	fpsgo_fstb2other_info_update(f_render->pid, f_render->buffer_id, FPSGO_PERF_IDX,
		0, 0, f_render->boost_info.last_blc, f_render->sbe_control_flag);

exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_dequeue_start(int pid,
	unsigned long long dequeue_start_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	int check_render;
	int ret;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		struct BQ_id *pair;

		pair = fpsgo_find_BQ_id(pid, 0, identifier, ACTION_FIND);
		if (pair) {
			FPSGO_COM_TRACE("%s: find pair enqueuer: %d, %d\n",
				__func__, pid, pair->queue_pid);
			pid = pair->queue_pid;
			f_render = fpsgo_search_and_add_render_info(pid,
				identifier, 0);
			if (!f_render) {
				fpsgo_render_tree_unlock(__func__);
				FPSGO_COM_TRACE("%s: NO pair enqueuer: %d\n",
					__func__, pid);
				return;
			}
		} else {
			fpsgo_render_tree_unlock(__func__);
			FPSGO_COM_TRACE("%s: NO pair enqueuer: %d\n",
				__func__, pid);
			return;
		}
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 0);
	if (!ret) {
		goto exit;
	}

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->tgid, f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
		f_render->t_dequeue_start = dequeue_start_time;
		FPSGO_COM_TRACE("pid[%d] type[%d] dequeue_s:%llu",
			pid, f_render->frame_type, dequeue_start_time);
		break;
	case BY_PASS_TYPE:
		f_render->t_dequeue_start = dequeue_start_time;
		break;
	default:
		FPSGO_COM_TRACE("type not found pid[%d] type[%d]",
			pid, f_render->frame_type);
		break;
	}
exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);

}

void fpsgo_ctrl2comp_dequeue_end(int pid,
	unsigned long long dequeue_end_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	int check_render;
	int ret;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;

	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		struct BQ_id *pair;

		pair = fpsgo_find_BQ_id(pid, 0, identifier, ACTION_FIND);
		if (pair) {
			pid = pair->queue_pid;
			f_render = fpsgo_search_and_add_render_info(pid,
				identifier, 0);
		}

		if (!f_render) {
			fpsgo_render_tree_unlock(__func__);
			FPSGO_COM_TRACE("%s: NO pair enqueuer: %d\n",
				__func__, pid);
			return;
		}
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	ret = fpsgo_com_refetch_buffer(f_render, pid, identifier, 0);
	if (!ret) {
		goto exit;
	}

	f_render->frame_type = fpsgo_com_check_frame_type(pid,
			f_render->tgid, f_render->queue_SF, f_render->api,
			f_render->hwui, f_render->sbe_control_flag,
			f_render->control_pid_flag);

	switch (f_render->frame_type) {
	case NON_VSYNC_ALIGNED_TYPE:
		f_render->t_dequeue_end = dequeue_end_time;
		f_render->dequeue_length =
			dequeue_end_time - f_render->t_dequeue_start;
		FPSGO_COM_TRACE(
			"pid[%d] type[%d] dequeue_e:%llu dequeue_l:%llu",
			pid, f_render->frame_type,
			dequeue_end_time, f_render->dequeue_length);
		fpsgo_comp2fbt_deq_end(f_render, dequeue_end_time);
		fpsgo_systrace_c_fbt_debug(-300, 0, f_render->dequeue_length,
			"%d_%d-dequeue_length", pid, f_render->frame_type);
		break;
	case BY_PASS_TYPE:
		f_render->t_dequeue_end = dequeue_end_time;
		f_render->dequeue_length =
			dequeue_end_time - f_render->t_dequeue_start;
		break;
	default:
		FPSGO_COM_TRACE("type not found pid[%d] type[%d]",
			pid, f_render->frame_type);
		break;
	}
exit:
	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_frame_start(struct render_info *f_render,
	unsigned long long frame_start_time,
	unsigned long long bufID)
{
	fpsgo_systrace_c_fbt(f_render->pid, bufID, 1, "[ux]sbe_set_ctrl");
	fbt_ux_frame_start(f_render, frame_start_time);
}

void fpsgo_frame_end(struct render_info *f_render,
	unsigned long long frame_start_time,
	unsigned long long frame_end_time,
	unsigned long long bufID)
{
	unsigned long long running_time = 0, raw_runtime = 0;
	unsigned long long enq_running_time = 0;

	if (f_render->t_enqueue_end)
		f_render->Q2Q_time =
			frame_end_time - f_render->t_enqueue_end;
	f_render->t_enqueue_end = frame_end_time;
	f_render->enqueue_length =
		frame_end_time - f_render->t_enqueue_start;

	fpsgo_comp2fstb_queue_time_update(f_render->pid,
		f_render->buffer_id,
		f_render->frame_type,
		frame_end_time,
		f_render->api,
		f_render->hwui);
	switch_thread_max_fps(f_render->pid, 1);

	// Filter ui process dep-list.
	xgf_set_policy_cmd_with_lock(1, 1, f_render->tgid, frame_end_time, 1);
	fpsgo_comp2xgf_qudeq_notify(f_render->pid, f_render->buffer_id,
		&raw_runtime, &running_time, &enq_running_time,
		frame_start_time, frame_end_time,
		0, 0, 0, 0, 0);
	f_render->raw_runtime = raw_runtime;
	if (running_time != 0)
		f_render->running_time = running_time;
	fbt_ux_frame_end(f_render, frame_start_time, frame_end_time);
	fpsgo_systrace_c_fbt(f_render->pid, bufID, 0, "[ux]sbe_set_ctrl");

	fpsgo_systrace_c_fbt_debug(f_render->pid, f_render->buffer_id,
		f_render->enqueue_length_real, "enq_length_real");

}

void fpsgo_ctrl2comp_hint_frame_start(int pid,
	unsigned long long frameID,
	unsigned long long frame_start_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	struct ux_frame_info *frame_info;
	int ux_frame_cnt = 0;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);
	fpsgo_render_tree_lock(__func__);

	// prepare render info
	f_render = fpsgo_search_and_add_render_info(pid, identifier, 1);
	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		FPSGO_COM_TRACE("%s: store frame info fail : %d !!!!\n",
			__func__, pid);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	// fill the frame info.
	f_render->frame_type = FRAME_HINT_TYPE;
	f_render->buffer_id = identifier;	//FPSGO UX: using a magic number.
	f_render->t_enqueue_start = frame_start_time; // for recycle only.
	f_render->t_last_start = frame_start_time;

	if (!f_render->p_blc)
		fpsgo_base2fbt_node_init(f_render);

	mutex_lock(&f_render->ux_mlock);
	frame_info = fpsgo_ux_search_and_add_frame_info(f_render, frameID, frame_start_time, 1);
	ux_frame_cnt = fpsgo_ux_count_frame_info(f_render, 2);
	fpsgo_systrace_c_fbt(pid, identifier, ux_frame_cnt, "[ux]ux_frame_cnt");
	mutex_unlock(&f_render->ux_mlock);

	// if not overlap, call frame start.
	if (ux_frame_cnt == 1)
		fpsgo_frame_start(f_render, frame_start_time, identifier);

	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);

	fpsgo_com_notify_fpsgo_is_boost(1);

	mutex_lock(&recycle_lock);
	if (recycle_idle_cnt) {
		recycle_idle_cnt = 0;
		if (!recycle_active) {
			recycle_active = 1;
			hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);
		}
	}
	mutex_unlock(&recycle_lock);
}

void fpsgo_ctrl2comp_hint_frame_end(int pid,
	unsigned long long frameID,
	unsigned long long frame_end_time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	struct ux_frame_info *frame_info;
	unsigned long long frame_start_time = 0;
	int ux_frame_cnt = 0;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);
	fpsgo_render_tree_lock(__func__);

	// prepare frame info.
	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);
	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		FPSGO_COM_TRACE("%s: NON pair frame info : %d !!!!\n",
			__func__, pid);
		return;
	}

	fpsgo_thread_lock(&f_render->thr_mlock);

	// fill the frame info.
	f_render->frame_type = FRAME_HINT_TYPE;
	f_render->t_enqueue_start = frame_end_time; // for recycle only.

	mutex_lock(&f_render->ux_mlock);
	frame_info = fpsgo_ux_search_and_add_frame_info(f_render, frameID, frame_start_time, 0);
	if (!frame_info) {
		fpsgo_systrace_c_fbt(pid, identifier, frameID, "[ux]start_not_found");
		fpsgo_systrace_c_fbt(pid, identifier, 0, "[ux]start_not_found");
	} else {
		frame_start_time = frame_info->start_ts;
		fpsgo_ux_delete_frame_info(f_render, frame_info);
	}

	ux_frame_cnt = fpsgo_ux_count_frame_info(f_render, 1);
	mutex_unlock(&f_render->ux_mlock);

	// frame end.
	fpsgo_frame_end(f_render, frame_start_time, frame_end_time, identifier);
	if (ux_frame_cnt == 1)
		fpsgo_frame_start(f_render, frame_end_time, identifier);
	fpsgo_systrace_c_fbt(pid, identifier, ux_frame_cnt, "[ux]ux_frame_cnt");

	fpsgo_fstb2other_info_update(f_render->pid, f_render->buffer_id, FPSGO_PERF_IDX,
		0, 0, f_render->boost_info.last_blc, 1);

	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_hint_frame_err(int pid,
	unsigned long long frameID,
	unsigned long long time,
	unsigned long long identifier)
{
	struct render_info *f_render;
	struct ux_frame_info *frame_info;
	int ux_frame_cnt = 0;

	FPSGO_COM_TRACE("%s pid[%d] id %llu", __func__, pid, identifier);
	fpsgo_render_tree_lock(__func__);

	f_render = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!f_render) {
		fpsgo_render_tree_unlock(__func__);
		FPSGO_COM_TRACE("%s: NON pair frame info : %d !!!!\n",
			__func__, pid);
		return;
	}

	f_render->t_enqueue_start = time; // for recycle only.

	fpsgo_thread_lock(&f_render->thr_mlock);

	mutex_lock(&f_render->ux_mlock);
	frame_info = fpsgo_ux_search_and_add_frame_info(f_render, frameID, time, 0);
	if (!frame_info) {
		fpsgo_systrace_c_fbt(pid, identifier, frameID, "[ux]start_not_found");
		fpsgo_systrace_c_fbt(pid, identifier, 0, "[ux]start_not_found");
	} else
		fpsgo_ux_delete_frame_info(f_render, frame_info);
	ux_frame_cnt = fpsgo_ux_count_frame_info(f_render, 1);
	if (ux_frame_cnt == 0) {
		fpsgo_systrace_c_fbt(f_render->pid, identifier, 0, "[ux]sbe_set_ctrl");
		fbt_ux_frame_err(f_render, time);
	}
	fpsgo_systrace_c_fbt(pid, identifier, ux_frame_cnt, "[ux]ux_frame_cnt");
	mutex_unlock(&f_render->ux_mlock);

	fpsgo_thread_unlock(&f_render->thr_mlock);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_hint_frame_dep_task(int rtid, unsigned long long identifier,
	int dep_mode, char *dep_name, int dep_num)
{
	int local_action = 0;
	int local_tgid = 0;
	int local_rtid[1];
	unsigned long long local_bufID[1];
	unsigned long long cur_ts;

	cur_ts = fpsgo_get_time();
	if (cur_ts <= last_update_sbe_dep_ts ||
		cur_ts - last_update_sbe_dep_ts < 500 * NSEC_PER_MSEC)
		return;
	last_update_sbe_dep_ts = cur_ts;
	fpsgo_main_trace("[comp] last_update_sbe_dep_ts:%llu", last_update_sbe_dep_ts);

	local_tgid = fpsgo_get_tgid(rtid);
	local_rtid[0] = rtid;
	local_bufID[0] = identifier;

	switch (dep_mode) {
	case 1:
		local_action = XGF_DEL_DEP;
		break;
	default:
		return;
	}

	fpsgo_other2xgf_set_dep_list(local_tgid, local_rtid, local_bufID, 1,
		dep_name, dep_num, local_action);
}

void fpsgo_ctrl2comp_connect_api(int pid, int api,
		unsigned long long identifier)
{
	struct connect_api_info *connect_api;
	int check_render;
	unsigned long long buffer_id = 0;
	int queue_SF = 0;
	int ret;

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;

	FPSGO_COM_TRACE("%s pid[%d], identifier:%llu",
		__func__, pid, identifier);

	fpsgo_render_tree_lock(__func__);

	ret = fpsgo_get_BQid_pair(pid, 0, identifier, &buffer_id, &queue_SF, 0);
	if (!ret || !buffer_id) {
		FPSGO_LOGI("connect %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_main_trace("COMP: connect %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	connect_api =
		fpsgo_com_search_and_add_connect_api_info(pid, buffer_id, 1);
	if (!connect_api) {
		fpsgo_render_tree_unlock(__func__);
		FPSGO_COM_TRACE("%s: store frame info fail : %d !!!!\n",
			__func__, pid);
		return;
	}

	connect_api->api = api;
	connect_api->ts = fpsgo_get_time();

	if (total_connect_api_info_num > FPSGO_MAX_CONNECT_API_INFO_SIZE)
		fpsgo_com_delete_oldest_connect_api_info();

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_bqid(int pid, unsigned long long buffer_id,
	int queue_SF, unsigned long long identifier, int create)
{
	struct BQ_id *pair;

	if (!identifier || !pid)
		return;

	if (!buffer_id && create)
		return;

	fpsgo_render_tree_lock(__func__);

	FPSGO_LOGI("pid %d: bufid %llu, id %llu, queue_SF %d, create %d\n",
		pid, buffer_id, identifier, queue_SF, create);

	if (create) {
		pair = fpsgo_find_BQ_id(pid, 0,
				identifier, ACTION_FIND_ADD);

		if (!pair) {
			fpsgo_render_tree_unlock(__func__);
			FPSGO_COM_TRACE("%s: add fail : %d !!!!\n",
				__func__, pid);
			return;
		}

		if (pair->pid != pid)
			FPSGO_LOGI("%d: diff render same key %d\n",
				pid, pair->pid);

		pair->buffer_id = buffer_id;
		pair->queue_SF = queue_SF;
	} else
		fpsgo_find_BQ_id(pid, 0,
			identifier, ACTION_FIND_DEL);

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_disconnect_api(
	int pid, int api, unsigned long long identifier)
{
	struct connect_api_info *connect_api;
	int check_render;
	unsigned long long buffer_id = 0;
	int queue_SF = 0;
	int ret;

	check_render = fpsgo_com_check_is_surfaceflinger(pid);

	if (check_render != FPSGO_COM_IS_RENDER)
		return;


	FPSGO_COM_TRACE("%s pid[%d], identifier:%llu",
		__func__, pid, identifier);

	fpsgo_render_tree_lock(__func__);

	ret = fpsgo_get_BQid_pair(pid, 0, identifier, &buffer_id, &queue_SF, 0);
	if (!ret || !buffer_id) {
		FPSGO_LOGI("[Disconnect] NoBQid %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_main_trace("COMP: disconnect %d: %llu, %llu\n",
				pid, buffer_id, identifier);
		fpsgo_render_tree_unlock(__func__);
		return;
	}

	connect_api =
		fpsgo_com_search_and_add_connect_api_info(pid, buffer_id, 0);
	if (!connect_api) {
		FPSGO_LOGI("[Disconnect] NoConnectApi %d: %llu, %llu\n",
			pid, buffer_id, identifier);
		FPSGO_COM_TRACE(
			"%s: FPSGo composer distory connect api fail : %d !!!!\n",
			__func__, pid);
		fpsgo_render_tree_unlock(__func__);
		return;
	}
	FPSGO_LOGI("[Disconnect] Success %d: %llu, %llu\n",
		pid, buffer_id, identifier);
	fpsgo_com_clear_connect_api_render_list(connect_api);
	rb_erase(&connect_api->rb_node, &connect_api_tree);
	kfree(connect_api);
	total_connect_api_info_num--;

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_acquire(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id, unsigned long long ts)
{
	struct acquire_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);

	if (api == WINDOW_DISCONNECT)
		fpsgo_delete_acquire_info(0, c_tid, buffer_id);
	else {
		iter = fpsgo_add_acquire_info(p_pid, c_pid, c_tid,
			api, buffer_id, ts);
		if (iter)
			iter->ts = ts;
	}

	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2comp_set_app_meta_fps(int tgid, int fps, unsigned long long ts)
{
	struct fpsgo_com_policy_cmd *iter = NULL;

	mutex_lock(&fpsgo_com_policy_cmd_lock);
	iter = fpsgo_com_get_policy_cmd(tgid, ts, 1);
	if (!iter)
		goto out;

	if (fps > 0) {
		iter->app_cam_meta_min_fps = fps;
		iter->ts = ts;
	} else {
		iter->app_cam_meta_min_fps = BY_PID_DEFAULT_VAL;
		fpsgo_com_delete_policy_cmd(iter);
	}

out:
	mutex_unlock(&fpsgo_com_policy_cmd_lock);
}

int fpsgo_ctrl2comp_set_sbe_policy(int tgid, char *name, unsigned long mask,
	int start, char *specific_name, int num)
{
	char *thread_name = NULL;
	int ret;
	int i;
	int *final_pid_arr =  NULL;
	unsigned long long *final_bufID_arr = NULL;
	int final_pid_arr_idx = 0;
	int *local_specific_tid_arr = NULL;
	int local_specific_tid_num = 0;
	struct fpsgo_attr_by_pid *attr_iter = NULL;
	unsigned long long ts = 0;

	if (tgid <= 0 || !name || !mask) {
		ret = -EINVAL;
		goto out;
	}

	ret = -ENOMEM;
	thread_name = kcalloc(16, sizeof(char), GFP_KERNEL);
	if (!thread_name)
		goto out;

	if (!strncpy(thread_name, name, 16))
		goto out;
	thread_name[15] = '\0';

	final_pid_arr = kcalloc(10, sizeof(int), GFP_KERNEL);
	if (!final_pid_arr)
		goto out;

	final_bufID_arr = kcalloc(10, sizeof(unsigned long long), GFP_KERNEL);
	if (!final_bufID_arr)
		goto out;

	local_specific_tid_arr = kcalloc(num, sizeof(int), GFP_KERNEL);
	if (!local_specific_tid_arr)
		goto out;
	ret = 0;

	fpsgo_get_render_tid_by_render_name(tgid, thread_name,
		final_pid_arr, final_bufID_arr, &final_pid_arr_idx, 10);
	fpsgo_main_trace("[comp] sbe tgid:%d name:%s mask:%lu start:%d final_pid_arr_idx:%d",
		tgid, name, mask, start, final_pid_arr_idx);


	ts = fpsgo_get_time();
	for (i = 0; i < final_pid_arr_idx; i++) {
		if (test_bit(FPSGO_CONTROL, &mask))
			switch_ui_ctrl(final_pid_arr[i], start);
		if (test_bit(FPSGO_MAX_TARGET_FPS, &mask))
			switch_thread_max_fps(final_pid_arr[i], start);

		fpsgo_render_tree_lock(__func__);
		xgf_set_policy_cmd_with_lock(1, 1, tgid, ts, 1);
		if (start) {
			attr_iter = fpsgo_find_attr_by_tid(final_pid_arr[i], 1);
			if (attr_iter) {
				if (test_bit(FPSGO_RESCUE_ENABLE, &mask))
					attr_iter->attr.rescue_enable_by_pid = 1;
				if (test_bit(FPSGO_RL_ENABLE, &mask))
					attr_iter->attr.gcc_enable_by_pid = 2;
				if (test_bit(FPSGO_GCC_DISABLE, &mask))
					attr_iter->attr.gcc_enable_by_pid = 0;
				if (test_bit(FPSGO_QUOTA_DISABLE, &mask))
					attr_iter->attr.qr_enable_by_pid = 0;
			}
		} else
			delete_attr_by_pid(final_pid_arr[i]);
		fpsgo_render_tree_unlock(__func__);

		fpsgo_main_trace("[comp] sbe %dth rtid:%d buffer_id:0x%llx",
			i+1, final_pid_arr[i], final_bufID_arr[i]);
	}

	if (test_bit(FPSGO_RUNNING_CHECK, &mask)) {
		if (start) {
			local_specific_tid_num = xgf_split_dep_name(tgid,
				specific_name, num, local_specific_tid_arr);
			fpsgo_update_sbe_spid_loading(local_specific_tid_arr,
				local_specific_tid_num, tgid);
		} else
			fpsgo_delete_sbe_spid_loading(tgid);
	} else if (final_pid_arr_idx > 0 && start)
		fpsgo_other2xgf_set_dep_list(tgid, final_pid_arr,
			final_bufID_arr, final_pid_arr_idx,
			specific_name, num, XGF_ADD_DEP_FORCE_CPU_TIME);

	ret = final_pid_arr_idx;

out:
	kfree(thread_name);
	kfree(final_pid_arr);
	kfree(final_bufID_arr);
	kfree(local_specific_tid_arr);
	return ret;
}

static void fpsgo_adpf_boost(int render_tid, unsigned long long buffer_id,
	unsigned long long tcpu, unsigned long long ts, int skip)
{
	struct render_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 0);
	if (!iter)
		goto out;

	fpsgo_thread_lock(&iter->thr_mlock);
	iter->enqueue_length = 0;
	iter->enqueue_length_real = 0;
	iter->dequeue_length = 0;
	if (iter->t_enqueue_end && !skip) {
		iter->raw_runtime = tcpu;
		iter->running_time = tcpu;
		iter->Q2Q_time = ts - iter->t_enqueue_end;
		fpsgo_comp2fbt_frame_start(iter, ts);
	}
	iter->t_enqueue_end = ts;
	fpsgo_thread_unlock(&iter->thr_mlock);

out:
	fpsgo_render_tree_unlock(__func__);
}

static void fpsgo_adpf_deboost(int render_tid, unsigned long long buffer_id)
{
	struct render_info *iter = NULL;

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 0);
	if (!iter)
		goto out;
	fpsgo_thread_lock(&iter->thr_mlock);
	fpsgo_stop_boost_by_render(iter);
	fpsgo_thread_unlock(&iter->thr_mlock);

out:
	fpsgo_render_tree_unlock(__func__);
}

int fpsgo_ctrl2comp_set_target_time(int tgid, int render_tid,
	unsigned long long buffer_id, unsigned long long target_time)
{
	int ret = 0;

	if (!target_time)
		return -EINVAL;

	mutex_lock(&adpf_hint_lock);

	fpsgo_systrace_c_fbt(render_tid, buffer_id,
		target_time, "adpf_update_target_work_duration");
	ret = fpsgo_comp2fstb_adpf_set_target_time(tgid, render_tid, buffer_id, target_time, 0);
	xgf_trace("[comp][%d][0x%llx] %s ret:%d", render_tid, buffer_id, __func__, ret);
	fpsgo_systrace_c_fbt(render_tid, buffer_id,
		0, "adpf_update_target_work_duration");

	mutex_unlock(&adpf_hint_lock);

	return ret;
}

int fpsgo_ctrl2comp_adpf_set_dep_list(int tgid, int render_tid,
	unsigned long long buffer_id, int *dep_arr, int dep_num)
{
	int ret = 0;

	if (!dep_arr)
		return -EFAULT;

	mutex_lock(&adpf_hint_lock);

	fpsgo_systrace_c_fbt(render_tid, buffer_id, dep_num, "adpf_set_threads");
	ret = fpsgo_comp2xgf_adpf_set_dep_list(tgid, render_tid, buffer_id, dep_arr, dep_num, 0);
	xgf_trace("[comp][%d][0x%llx] %s ret:%d", render_tid, buffer_id, __func__, ret);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 0, "adpf_set_threads");

	mutex_unlock(&adpf_hint_lock);

	return ret;
}

void fpsgo_ctrl2comp_adpf_resume(int render_tid, unsigned long long buffer_id)
{
	unsigned long long ts = fpsgo_get_time();

	mutex_lock(&adpf_hint_lock);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "adpf_resume");
	fpsgo_adpf_boost(render_tid, buffer_id, 0, ts, 1);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 0, "adpf_resume");
	mutex_unlock(&adpf_hint_lock);
}

void fpsgo_ctrl2comp_adpf_pause(int render_tid, unsigned long long buffer_id)
{
	mutex_lock(&adpf_hint_lock);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "adpf_pause");
	fpsgo_adpf_deboost(render_tid, buffer_id);
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 0, "adpf_pause");
	mutex_unlock(&adpf_hint_lock);
}

void fpsgo_ctrl2comp_adpf_close(int tgid, int render_tid, unsigned long long buffer_id)
{
	mutex_lock(&adpf_hint_lock);

	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "adpf_close");

	fpsgo_comp2xgf_adpf_set_dep_list(tgid, render_tid, buffer_id, NULL, 0, -1);
	fpsgo_comp2fstb_adpf_set_target_time(tgid, render_tid, buffer_id, 0, -1);

	fpsgo_adpf_deboost(render_tid, buffer_id);

	fpsgo_render_tree_lock(__func__);
	fpsgo_delete_render_info(render_tid, buffer_id, buffer_id);
	fpsgo_render_tree_unlock(__func__);

	fpsgo_systrace_c_fbt(render_tid, buffer_id, 0, "adpf_close");

	mutex_unlock(&adpf_hint_lock);
}

int fpsgo_ctrl2comp_adpf_create_session(int tgid, int render_tid, unsigned long long buffer_id,
	int *dep_arr, int dep_num, unsigned long long target_time)
{
	int ret = 0;
	unsigned long local_master_type = 0;
	struct render_info *iter = NULL;

	if (!dep_arr)
		return -EFAULT;
	if (!target_time)
		return -EINVAL;

	mutex_lock(&adpf_hint_lock);

	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "adpf_create_session");

	ret = fpsgo_comp2xgf_adpf_set_dep_list(tgid, render_tid, buffer_id, dep_arr, dep_num, 1);
	if (ret)
		goto out;

	ret = fpsgo_comp2fstb_adpf_set_target_time(tgid, render_tid, buffer_id, target_time, 1);
	if (ret) {
		fpsgo_comp2xgf_adpf_set_dep_list(tgid, render_tid, buffer_id, NULL, 0, -1);
		goto out;
	}

	fpsgo_render_tree_lock(__func__);
	iter = fpsgo_search_and_add_render_info(render_tid, buffer_id, 1);
	if (iter) {
		fpsgo_thread_lock(&iter->thr_mlock);
		iter->tgid = tgid;
		iter->buffer_id = buffer_id;
		iter->api = NATIVE_WINDOW_API_EGL;
		iter->frame_type = NON_VSYNC_ALIGNED_TYPE;
		set_bit(ADPF_TYPE, &local_master_type);
		iter->master_type = local_master_type;
		if (!iter->p_blc)
			fpsgo_base2fbt_node_init(iter);
		fpsgo_thread_unlock(&iter->thr_mlock);
	} else {
		fpsgo_comp2xgf_adpf_set_dep_list(tgid, render_tid, buffer_id, NULL, 0, -1);
		fpsgo_comp2fstb_adpf_set_target_time(tgid, render_tid, buffer_id, 0, -1);
	}
	fpsgo_render_tree_unlock(__func__);

out:
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 0, "adpf_create_session");
	mutex_unlock(&adpf_hint_lock);
	return ret;
}

int fpsgo_ctrl2comp_report_workload(int tgid, int render_tid, unsigned long long buffer_id,
	unsigned long long *tcpu_arr, unsigned long long *ts_arr, int num)
{
	int i;
	int ret = 0;
	unsigned long long local_tcpu = 0;
	unsigned long long local_ts = 0;

	mutex_lock(&adpf_hint_lock);

	fpsgo_systrace_c_fbt(render_tid, buffer_id, 1, "adpf_report_actual_work_duration");

	if (num > 1) {
		for (i = 0; i < num; i++) {
			if (local_ts < ts_arr[i])
				local_ts = ts_arr[i];
			local_tcpu += tcpu_arr[i];
			xgf_trace("[adpf][xgf][%d][0x%llx] | %dth t_cpu:%llu ts:%llu",
				render_tid, buffer_id, i, tcpu_arr[i], ts_arr[i]);
		}
	} else if (num == 1) {
		local_tcpu = tcpu_arr[0];
		local_ts = ts_arr[0];
	} else {
		ret = -EINVAL;
		goto out;
	}
	xgf_trace("[adpf][xgf][%d][0x%llx] | local_tcpu:%llu local_ts:%llu",
		render_tid, buffer_id, local_tcpu, local_ts);

	fpsgo_adpf_boost(render_tid, buffer_id, local_tcpu, local_ts, 0);

out:
	fpsgo_systrace_c_fbt(render_tid, buffer_id, 0, "adpf_report_actual_work_duration");
	mutex_unlock(&adpf_hint_lock);
	return ret;
}

void fpsgo_base2comp_check_connect_api(void)
{
	struct rb_node *n;
	struct connect_api_info *iter;
	struct task_struct *tsk;
	int size = 0;

	FPSGO_COM_TRACE("%s ", __func__);

	fpsgo_render_tree_lock(__func__);

	n = rb_first(&connect_api_tree);

	while (n) {
		iter = rb_entry(n, struct connect_api_info, rb_node);
		rcu_read_lock();
		tsk = find_task_by_vpid(iter->tgid);
		rcu_read_unlock();
		if (!tsk) {
			fpsgo_com_clear_connect_api_render_list(iter);
			rb_erase(&iter->rb_node, &connect_api_tree);
			n = rb_first(&connect_api_tree);
			kfree(iter);
			total_connect_api_info_num--;
			size = 0;
		} else {
			n = rb_next(n);
			size++;
		}
	}

	if (size >= MAX_CONNECT_API_SIZE)
		FPSGO_LOGI("connect api tree size: %d", size);

	fpsgo_render_tree_unlock(__func__);
}

int fpsgo_base2comp_check_connect_api_tree_empty(void)
{
	int ret = 0;

	if (RB_EMPTY_ROOT(&connect_api_tree))
		ret = 1;

	return ret;
}

int switch_ui_ctrl(int pid, int set_ctrl)
{
	fpsgo_render_tree_lock(__func__);
	if (set_ctrl)
		fpsgo_search_and_add_sbe_info(pid, 1);
	else {
		fpsgo_delete_sbe_info(pid);

		fpsgo_stop_boost_by_pid(pid);
	}
	fpsgo_render_tree_unlock(__func__);

	fpsgo_systrace_c_fbt(pid, 0,
			set_ctrl, "sbe_set_ctrl");
	fpsgo_systrace_c_fbt(pid, 0,
			0, "sbe_state");

	return 0;
}

static int switch_fpsgo_control_pid(int pid, int set_ctrl)
{
	fpsgo_render_tree_lock(__func__);
	if (set_ctrl)
		fpsgo_search_and_add_fps_control_pid(pid, 1);
	else
		fpsgo_delete_fpsgo_control_pid(pid);

	fpsgo_render_tree_unlock(__func__);

	fpsgo_systrace_c_fbt(pid, 0,
			set_ctrl, "fpsgo_control_pid");

	return 0;
}

static void fpsgo_com_notify_to_do_recycle(struct work_struct *work)
{
	int ret1, ret2, ret3;

	ret1 = fpsgo_check_thread_status();
	ret2 = fpsgo_comp2fstb_do_recycle();
	ret3 = fpsgo_comp2xgf_do_recycle();

	mutex_lock(&recycle_lock);

	if (ret1 && ret2 && ret3) {
		recycle_idle_cnt++;
		if (recycle_idle_cnt >= FPSGO_MAX_RECYCLE_IDLE_CNT) {
			recycle_active = 0;
			goto out;
		}
	}

	hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);

out:
	mutex_unlock(&recycle_lock);
}

int fpsgo_ktf2comp_test_check_BQ_type(int *a_p_pid_arr,  int *a_c_pid_arr,
	int *a_c_tid_arr, int *a_api_arr, unsigned long long *a_bufID_arr, int a_num,
	int *r_tgid_arr, int *r_pid_arr, unsigned long long *r_bufID_arr, int *r_api_arr,
	int *r_frame_type_arr, int *r_hwui_arr, int *r_tfps_arr, int *r_qfps_arr,
	int *final_bq_type_arr, int r_num)
{
	int i;
	int ret = 0;
	int r_ret = 0;
	int local_correct_cnt = 0;
	int local_bq_type = ACQUIRE_UNKNOWN_TYPE;
	struct render_info *r_iter = NULL;
	struct acquire_info *a_iter = NULL;

	fpsgo_render_tree_lock(__func__);

	for (i = 0; i < a_num; i++) {
		a_iter = fpsgo_add_acquire_info(a_p_pid_arr[i], a_c_pid_arr[i],
			a_c_tid_arr[i], a_api_arr[i], a_bufID_arr[i], 0);
		if (!a_iter) {
			ret = -ENOMEM;
			break;
		}
	}

	if (ret)
		goto out;

	FPSGO_LOGE("struct render_info size:%lu\n", sizeof(struct render_info));

	for (i = 0; i < r_num; i++) {
		r_iter = fpsgo_search_and_add_render_info(r_pid_arr[i], r_bufID_arr[i], 1);
		if (r_iter) {
			r_iter->tgid = r_tgid_arr[i];
			r_iter->pid = r_pid_arr[i];
			r_iter->buffer_id = r_bufID_arr[i];
			r_iter->api = r_api_arr[i];
			r_iter->frame_type = r_frame_type_arr[i];
			r_iter->hwui = r_hwui_arr[i];
		} else {
			ret = -ENOMEM;
			break;
		}

		r_ret = fpsgo_ktf2fstb_add_delete_render_info(0, r_pid_arr[i], r_bufID_arr[i],
					r_tfps_arr[i], r_qfps_arr[i]);
		if (!r_ret) {
			ret = -ENOMEM;
			break;
		}

		r_ret = fpsgo_ktf2xgf_add_delete_render_info(0, r_pid_arr[i], r_bufID_arr[i]);
		if (!r_ret) {
			ret = -ENOMEM;
			break;
		}
	}

	if (ret)
		goto out;

	for (i = 0; i < r_num; i++) {
		local_bq_type = ACQUIRE_UNKNOWN_TYPE;

		if (r_frame_type_arr[i] == NON_VSYNC_ALIGNED_TYPE)
			fpsgo_com_check_BQ_type(&local_bq_type, r_pid_arr[i], r_bufID_arr[i]);

		if (final_bq_type_arr[i] == local_bq_type)
			local_correct_cnt++;

		FPSGO_LOGE("%dth expected_bq_type:%d local_bq_type:%d",
			i+1, final_bq_type_arr[i], local_bq_type);
	}

	ret = (local_correct_cnt == r_num) ? 1 : 0;

out:
	for (i = 0; i < a_num; i++)
		fpsgo_delete_acquire_info(0, a_c_tid_arr[i], a_bufID_arr[i]);
	for (i = 0; i < r_num; i++) {
		fpsgo_delete_render_info(r_pid_arr[i], r_bufID_arr[i], r_bufID_arr[i]);
		fpsgo_ktf2fstb_add_delete_render_info(1, r_pid_arr[i], r_bufID_arr[i], 0, 0);
		fpsgo_ktf2xgf_add_delete_render_info(1, r_pid_arr[i], r_bufID_arr[i]);
	}

	fpsgo_render_tree_unlock(__func__);

	return ret;
}
EXPORT_SYMBOL(fpsgo_ktf2comp_test_check_BQ_type);

static ssize_t connect_api_info_show
	(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct connect_api_info *iter;
	struct task_struct *tsk;
	struct render_info *pos, *next;
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + posi, FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"=================================\n");
	posi += length;

	fpsgo_render_tree_lock(__func__);

	length = scnprintf(temp + posi, FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"total_connect_api_info_num: %d\n", total_connect_api_info_num);
	posi += length;

	for (n = rb_first(&connect_api_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct connect_api_info, rb_node);
		rcu_read_lock();
		tsk = find_task_by_vpid(iter->tgid);
		if (tsk) {
			get_task_struct(tsk);
			length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"PID  TGID  NAME    BufferID    API    Key\n");
			posi += length;
			length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"%5d %5d %5s %4llu %5d %4llu\n",
				iter->pid, iter->tgid, tsk->comm,
				iter->buffer_id, iter->api, iter->buffer_key);
			posi += length;
			put_task_struct(tsk);
		}
		rcu_read_unlock();

		length = scnprintf(temp + posi,
			FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
			"******render list******\n");
		posi += length;

		list_for_each_entry_safe(pos, next,
				&iter->render_list, bufferid_list) {
			fpsgo_thread_lock(&pos->thr_mlock);

			length = scnprintf(temp + posi,
					FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
					"  PID  TGID	 BufferID	API    TYPE\n");
			posi += length;
			length = scnprintf(temp + posi,
					FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
					"%5d %5d %4llu %5d %5d\n",
					pos->pid, pos->tgid, pos->buffer_id,
					pos->api, pos->frame_type);
			posi += length;

			fpsgo_thread_unlock(&pos->thr_mlock);

		}

		length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"***********************\n");
		posi += length;
		length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"=================================\n");
		posi += length;
	}

	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(connect_api_info);

#define FPSGO_COM_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define FPSGO_COM_SYSFS_WRITE_VALUE(name, variable, min, max); \
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
					(variable) = arg; \
				} \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

#define FPSGO_COM_SYSFS_WRITE_POLICY_CMD(name, cmd, min, max); \
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
				mutex_lock(&fpsgo_com_policy_cmd_lock); \
				if (arg >= (min) && arg <= (max)) \
					fpsgo_com_set_policy_cmd(cmd, arg, tgid, ts, 1); \
				else \
					fpsgo_com_set_policy_cmd(cmd, BY_PID_DEFAULT_VAL, \
						tgid, ts, 0); \
				mutex_unlock(&fpsgo_com_policy_cmd_lock); \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

FPSGO_COM_SYSFS_READ(fpsgo_control, 1, fpsgo_control);
FPSGO_COM_SYSFS_WRITE_VALUE(fpsgo_control, fpsgo_control, 0, 1);
static KOBJ_ATTR_RW(fpsgo_control);

FPSGO_COM_SYSFS_READ(control_hwui, 1, control_hwui);
FPSGO_COM_SYSFS_WRITE_VALUE(control_hwui, control_hwui, 0, 1);
static KOBJ_ATTR_RW(control_hwui);

FPSGO_COM_SYSFS_READ(control_api_mask, 1, control_api_mask);
FPSGO_COM_SYSFS_WRITE_VALUE(control_api_mask, control_api_mask, 0, 32);
static KOBJ_ATTR_RW(control_api_mask);

FPSGO_COM_SYSFS_READ(bypass_non_SF, 1, bypass_non_SF);
FPSGO_COM_SYSFS_WRITE_VALUE(bypass_non_SF, bypass_non_SF, 0, 1);
static KOBJ_ATTR_RW(bypass_non_SF);

FPSGO_COM_SYSFS_READ(fps_align_margin, 1, fps_align_margin);
FPSGO_COM_SYSFS_WRITE_VALUE(fps_align_margin, fps_align_margin, 0, 10);
static KOBJ_ATTR_RW(fps_align_margin);

FPSGO_COM_SYSFS_READ(bypass_non_SF_by_pid, 0, 0);
FPSGO_COM_SYSFS_WRITE_POLICY_CMD(bypass_non_SF_by_pid, 0, 0, 1);
static KOBJ_ATTR_RW(bypass_non_SF_by_pid);

FPSGO_COM_SYSFS_READ(control_api_mask_by_pid, 0, 0);
FPSGO_COM_SYSFS_WRITE_POLICY_CMD(control_api_mask_by_pid, 1, 0, 31);
static KOBJ_ATTR_RW(control_api_mask_by_pid);

FPSGO_COM_SYSFS_READ(control_hwui_by_pid, 0, 0);
FPSGO_COM_SYSFS_WRITE_POLICY_CMD(control_hwui_by_pid, 2, 0, 1);
static KOBJ_ATTR_RW(control_hwui_by_pid);

FPSGO_COM_SYSFS_READ(app_cam_fps_align_margin_by_pid, 0, 0);
FPSGO_COM_SYSFS_WRITE_POLICY_CMD(app_cam_fps_align_margin_by_pid, 3, 0, 100);
static KOBJ_ATTR_RW(app_cam_fps_align_margin_by_pid);

FPSGO_COM_SYSFS_READ(app_cam_time_align_ratio_by_pid, 0, 0);
FPSGO_COM_SYSFS_WRITE_POLICY_CMD(app_cam_time_align_ratio_by_pid, 4, 0, 100);
static KOBJ_ATTR_RW(app_cam_time_align_ratio_by_pid);

static ssize_t fpsgo_com_policy_cmd_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 1;
	int pos = 0;
	int length = 0;
	struct fpsgo_com_policy_cmd *iter;
	struct rb_root *rbr;
	struct rb_node *rbn;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fpsgo_com_policy_cmd_lock);

	rbr = &fpsgo_com_policy_cmd_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct fpsgo_com_policy_cmd, rb_node);
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\ttgid:%d\tbypass_non_SF:%d\tcontrol_api_mask:%d\tcontrol_hwui:%d\tapp_cam_align:(%d,%d)\tapp_min_fps:%d\tts:%llu\n",
			i,
			iter->tgid,
			iter->bypass_non_SF_by_pid,
			iter->control_api_mask_by_pid,
			iter->control_hwui_by_pid,
			iter->app_cam_fps_align_margin,
			iter->app_cam_time_align_ratio,
			iter->app_cam_meta_min_fps,
			iter->ts);
		pos += length;
		i++;
	}

	mutex_unlock(&fpsgo_com_policy_cmd_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(fpsgo_com_policy_cmd);

static ssize_t set_ui_ctrl_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 0;
	int pos = 0;
	int length = 0;
	int total = 0;
	struct sbe_info *arr = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	arr = kcalloc(FPSGO_MAX_TREE_SIZE, sizeof(struct sbe_info), GFP_KERNEL);
	if (!arr)
		goto out;

	fpsgo_render_tree_lock(__func__);
	total = fpsgo_get_all_sbe_info(arr);
	fpsgo_render_tree_unlock(__func__);

	for (i = 0; i < total; i++) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\trender_tid:%d\n", i+1, arr[i].pid);
		pos += length;
	}

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(arr);
	kfree(temp);
	return length;
}

static ssize_t set_ui_ctrl_store(struct kobject *kobj,
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
			fpsgo_systrace_c_fbt(arg > 0 ? arg : -arg,
				0, arg > 0, "force_ctrl");
			if (arg > 0)
				ret = switch_ui_ctrl(arg, 1);
			else
				ret = switch_ui_ctrl(-arg, 0);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(set_ui_ctrl);

static ssize_t fpsgo_control_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 0;
	int total = 0;
	int pos = 0;
	int length = 0;
	struct fps_control_pid_info *arr = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	arr = kcalloc(FPSGO_MAX_TREE_SIZE, sizeof(struct fps_control_pid_info), GFP_KERNEL);
	if (!arr)
		goto out;

	fpsgo_render_tree_lock(__func__);

	total = fpsgo_get_all_fps_control_pid_info(arr);

	fpsgo_render_tree_unlock(__func__);

	for (i = 0; i < total; i++) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%dth\ttgid:%d\tts:%llu\n", i+1, arr[i].pid, arr[i].ts);
		pos += length;
	}

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(arr);
	kfree(temp);
	return length;
}

static ssize_t  fpsgo_control_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int pid = 0, value = 0;
	int ret = 0;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d", &pid, &value) == 2)
				ret = switch_fpsgo_control_pid(pid, !!value);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(fpsgo_control_pid);

static ssize_t is_fpsgo_boosting_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fpsgo_boost_lock);
	length = scnprintf(temp + posi,
		FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"fpsgo is boosting = %d\n", fpsgo_is_boosting);
	posi += length;
	mutex_unlock(&fpsgo_boost_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(is_fpsgo_boosting);

void __exit fpsgo_composer_exit(void)
{
	hrtimer_cancel(&recycle_hrt);
	destroy_workqueue(composer_wq);

	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_connect_api_info);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_hwui);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_api_mask);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_bypass_non_SF);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_set_ui_ctrl);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_control);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_control_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fps_align_margin);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_bypass_non_SF_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_api_mask_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_control_hwui_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_fpsgo_com_policy_cmd);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_is_fpsgo_boosting);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_app_cam_fps_align_margin_by_pid);
	fpsgo_sysfs_remove_file(comp_kobj, &kobj_attr_app_cam_time_align_ratio_by_pid);

	fpsgo_sysfs_remove_dir(&comp_kobj);
}

int __init fpsgo_composer_init(void)
{
	connect_api_tree = RB_ROOT;
	fpsgo_com_policy_cmd_tree = RB_ROOT;

	composer_wq = alloc_ordered_workqueue("composer_wq", WQ_MEM_RECLAIM | WQ_HIGHPRI);
	hrtimer_init(&recycle_hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	recycle_hrt.function = &prepare_do_recycle;

	hrtimer_start(&recycle_hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);

	init_fpsgo_is_boosting_callback();
	fpsgo_set_fpsgo_is_boosting(0);

	if (!fpsgo_sysfs_create_dir(NULL, "composer", &comp_kobj)) {
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_connect_api_info);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_hwui);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_api_mask);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_bypass_non_SF);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_set_ui_ctrl);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_control);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_control_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fps_align_margin);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_bypass_non_SF_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_api_mask_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_control_hwui_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_fpsgo_com_policy_cmd);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_is_fpsgo_boosting);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_app_cam_fps_align_margin_by_pid);
		fpsgo_sysfs_create_file(comp_kobj, &kobj_attr_app_cam_time_align_ratio_by_pid);
	}

	return 0;
}

