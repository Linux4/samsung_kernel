// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/rbtree.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <mt-plat/fpsgo_common.h>

#include "eas/grp_awr.h"
#include "fpsgo_usedext.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fbt_usedext.h"
#include "fbt_cpu.h"
#include "fbt_cpu_platform.h"
#include "../fstb/fstb.h"
#include "xgf.h"
#include "mini_top.h"
#include "fps_composer.h"
#include "fpsgo_cpu_policy.h"
#include "fbt_cpu_ctrl.h"
#include "fbt_cpu_ux.h"

#define TARGET_UNLIMITED_FPS 240
#define NSEC_PER_HUSEC 100000
#define SBE_RESCUE_MODE_UNTIL_QUEUE_END 2

enum FPSGO_HARD_LIMIT_POLICY {
	FPSGO_HARD_NONE = 0,
	FPSGO_HARD_MARGIN = 1,
	FPSGO_HARD_CEILING = 2,
	FPSGO_HARD_LIMIT = 3,
};

enum FPSGO_JERK_STAGE {
	FPSGO_JERK_INACTIVE = 0,
	FPSGO_JERK_FIRST,
	FPSGO_JERK_SBE,
	FPSGO_JERK_SECOND,
};

enum FPSGO_TASK_POLICY {
	FPSGO_TASK_NONE = 0,
	FPSGO_TASK_VIP = 1,
};

enum UX_SCROLL_POLICY_TYPE {
	SCROLL_POLICY_FPSGO_CTL = 1,
	SCROLL_POLICY_EAS_CTL = 2,
};

static DEFINE_MUTEX(fbt_mlock);

static struct kmem_cache *frame_info_cachep __ro_after_init;

static int fpsgo_ux_gcc_enable;
static int sbe_rescue_enable;
static int ux_scroll_policy_type;
static int sbe_rescuing_frame_id;
static int sbe_rescuing_frame_id_legacy;
static int sbe_enhance_f;
static int global_ux_blc;
static int global_ux_max_pid;
static int set_ux_uclampmin;
static int set_ux_uclampmax;
static int set_deplist_vip;
static int rescue_cpu_mask;
static struct fpsgo_loading temp_blc_dep[MAX_DEP_NUM];
static struct fbt_setting_info sinfo;

module_param(fpsgo_ux_gcc_enable, int, 0644);
module_param(sbe_enhance_f, int, 0644);
module_param(set_ux_uclampmin, int, 0644);
module_param(set_ux_uclampmax, int, 0644);
module_param(set_deplist_vip, int, 0644);
module_param(rescue_cpu_mask, int, 0644);

/* main function*/
static int nsec_to_100usec(unsigned long long nsec)
{
	unsigned long long husec;

	husec = div64_u64(nsec, (unsigned long long)NSEC_PER_HUSEC);

	return (int)husec;
}

int fpsgo_ctrl2ux_get_perf(void)
{
	return global_ux_blc;
}

void fbt_ux_set_perf(int cur_pid, int cur_blc)
{
	global_ux_blc = cur_blc;
	global_ux_max_pid = cur_pid;
}

static void fpsgo_set_deplist_policy(struct render_info *thr, int policy)
{
	int i;
	int local_dep_size = 0;
	struct fpsgo_loading *local_dep_arr = NULL;

	if (!set_deplist_vip)
		return;

	local_dep_arr = kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!local_dep_arr)
		return;

	local_dep_size = fbt_determine_final_dep_list(thr, local_dep_arr);

	for (i = 0; i < local_dep_size; i++) {
		if (local_dep_arr[i].pid <= 0)
			continue;
		if (policy == 0)
			unset_task_basic_vip(local_dep_arr[i].pid);
		else if (policy == 1)
			set_task_basic_vip(local_dep_arr[i].pid);
	}
	kfree(local_dep_arr);
}

static int fbt_ux_cal_perf(
	long long t_cpu_cur,
	long long target_time,
	unsigned int target_fps,
	unsigned int target_fps_ori,
	unsigned int fps_margin,
	struct render_info *thread_info,
	unsigned long long ts,
	long aa, unsigned int target_fpks, int cooler_on)
{
	unsigned int blc_wt = 0U;
	unsigned int last_blc_wt = 0U;
	unsigned long long cur_ts;
	struct fbt_boost_info *boost_info;
	int pid;
	unsigned long long buffer_id;
	unsigned long long t1, t2, t_Q2Q;
	long aa_n;

	if (!thread_info) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return 0;
	}

	cur_ts = fpsgo_get_time();
	pid = thread_info->pid;
	buffer_id = thread_info->buffer_id;
	boost_info = &(thread_info->boost_info);

	mutex_lock(&fbt_mlock);

	t1 = (unsigned long long)t_cpu_cur;
	t1 = nsec_to_100usec(t1);
	t2 = target_time;
	t_Q2Q = thread_info->Q2Q_time;
	t_Q2Q = nsec_to_100usec(t_Q2Q);
	aa_n = aa;

	if (thread_info->p_blc) {
		fpsgo_get_blc_mlock(__func__);
		last_blc_wt = thread_info->p_blc->blc;
		fpsgo_put_blc_mlock(__func__);
	}

	if (aa_n < 0) {
		blc_wt = last_blc_wt;
		aa_n = 0;
	} else {
		fbt_cal_aa(aa, t1, t_Q2Q, &aa_n);

		if (fpsgo_ux_gcc_enable == 2) {
			fbt_cal_target_time_ns(thread_info->pid, thread_info->buffer_id,
				fbt_get_rl_ko_is_ready(), 2, target_fps_ori,
				thread_info->target_fps_origin, target_fpks,
				target_time, 0, boost_info->last_target_time_ns, thread_info->Q2Q_time,
				0, 0, thread_info->attr.expected_fps_margin_by_pid, 10, 10,
				0, thread_info->attr.quota_v2_diff_clamp_min_by_pid,
				thread_info->attr.quota_v2_diff_clamp_max_by_pid, 0,
				0, aa_n, aa_n, aa_n, 100, 100, 100, &t2);
			boost_info->last_target_time_ns = t2;
		}

		t2 = nsec_to_100usec(t2);

		fbt_cal_blc(aa_n, t2, last_blc_wt, t_Q2Q, 0, &blc_wt);
	}

	fpsgo_systrace_c_fbt(pid, buffer_id, aa_n, "[ux]aa");

	blc_wt = clamp(blc_wt, 1U, 100U);

	boost_info->target_fps = target_fps;
	boost_info->target_time = target_time;
	boost_info->last_blc = blc_wt;
	boost_info->last_normal_blc = blc_wt;
	//boost_info->cur_stage = FPSGO_JERK_INACTIVE;
	mutex_unlock(&fbt_mlock);
	return blc_wt;
}

static int fbt_ux_get_max_cap(int pid, unsigned long long bufID, int min_cap)
{
	int bhr_local = fbt_cpu_get_bhr();

	return fbt_get_max_cap(min_cap, 0, bhr_local, pid, bufID);
}

static void fbt_ux_set_cap(struct render_info *thr, int min_cap, int max_cap)
{
	int i;
	int local_dep_size = 0;
	char temp[7] = {"\0"};
	char *local_dep_str = NULL;
	struct fpsgo_loading *local_dep_arr = NULL;
	int ret = 0;

	local_dep_str = kcalloc(MAX_DEP_NUM + 1, 7 * sizeof(char), GFP_KERNEL);
	if (!local_dep_str)
		goto out;

	local_dep_arr = kcalloc(MAX_DEP_NUM, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (!local_dep_arr)
		goto out;

	local_dep_size = fbt_determine_final_dep_list(thr, local_dep_arr);

	for (i = 0; i < local_dep_size; i++) {
		if (local_dep_arr[i].pid <= 0)
			continue;

		fbt_set_per_task_cap(local_dep_arr[i].pid, min_cap, max_cap, 1024);

		if (strlen(local_dep_str) == 0)
			ret = snprintf(temp, sizeof(temp), "%d", local_dep_arr[i].pid);
		else
			ret = snprintf(temp, sizeof(temp), ",%d", local_dep_arr[i].pid);

		if (ret > 0 && strlen(local_dep_str) + strlen(temp) < 256)
			strncat(local_dep_str, temp, strlen(temp));
	}

	fpsgo_main_trace("[%d] dep-list %s", thr->pid, local_dep_str);

out:
	kfree(local_dep_str);
	kfree(local_dep_arr);
}

static void fbt_ux_set_cap_with_sbe(struct render_info *thr)
{
	int set_blc_wt = 0;
	int local_min_cap = 0;
	int local_max_cap = 100;

	set_blc_wt = thr->ux_blc_cur + thr->sbe_enhance;
	set_blc_wt = clamp(set_blc_wt, 0, 100);

	fpsgo_get_blc_mlock(__func__);
	if (thr->p_blc)
		thr->p_blc->blc = set_blc_wt;
	fpsgo_put_blc_mlock(__func__);

	fpsgo_get_fbt_mlock(__func__);

	switch (ux_scroll_policy_type) {
	case SCROLL_POLICY_EAS_CTL:
		if (!set_ux_uclampmin)
			local_min_cap = thr->sbe_enhance;
		else
			local_min_cap = set_blc_wt;

		if (thr->sbe_enhance > 0 || !set_ux_uclampmax || thr->ux_blc_cur <= 0)
			local_max_cap = 100;
		else
			local_max_cap = fbt_ux_get_max_cap(thr->pid,
				thr->buffer_id, set_blc_wt);
		break;
	default:
	case SCROLL_POLICY_FPSGO_CTL:
		local_min_cap = set_blc_wt;
		if (local_min_cap != 0) {
			if (thr->sbe_enhance > 0)
				local_max_cap = 100;
			else
				local_max_cap = fbt_ux_get_max_cap(thr->pid,
					thr->buffer_id, set_blc_wt);
		}
		break;
	}

	if (local_min_cap == 0 && local_max_cap == 100)
		fbt_check_max_blc_locked(thr->pid);
	else
		fbt_set_limit(thr->pid, local_min_cap, thr->pid, thr->buffer_id,
			thr->dep_valid_size, thr->dep_arr, thr, 0);
	fpsgo_put_fbt_mlock(__func__);

	fbt_ux_set_cap(thr, local_min_cap, local_max_cap);
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, local_min_cap, "[ux]perf_idx");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, local_max_cap, "[ux]perf_idx_max");
}

void fbt_ux_frame_start(struct render_info *thr, unsigned long long ts)
{
	if (!thr)
		return;

	thr->ux_blc_cur = thr->ux_blc_next;

	if (ux_scroll_policy_type == SCROLL_POLICY_EAS_CTL){
		set_top_grp_aware(1,0);
		fpsgo_set_deplist_policy(thr, FPSGO_TASK_VIP);
	}
	fbt_ux_set_cap_with_sbe(thr);
}

void fbt_ux_frame_end(struct render_info *thr,
		unsigned long long start_ts, unsigned long long end_ts)
{
	struct fbt_boost_info *boost;
	long long runtime;
	int targettime, targetfps, targetfps_ori, targetfpks, fps_margin, cooler_on;
	int loading = 0L;
	int q_c_time = 0L, q_g_time = 0L;
	int ret;

	if (!thr)
		return;

	boost = &(thr->boost_info);

	runtime = thr->running_time;

	if (boost->f_iter < 0)
		boost->f_iter = 0;
	boost->frame_info[boost->f_iter].running_time = runtime;
	// fstb_query_dfrc
	fpsgo_fbt2fstb_query_fps(thr->pid, thr->buffer_id,
			&targetfps, &targetfps_ori, &targettime, &fps_margin,
			&q_c_time, &q_g_time, &targetfpks, &cooler_on);
	boost->quantile_cpu_time = q_c_time;
	boost->quantile_gpu_time = q_g_time;	// [ux] unavailable, for statistic only.
	fpsgo_fbt_ux2fstb_query_dfrc(&targetfps, &targettime);

	if (!targetfps)
		targetfps = TARGET_UNLIMITED_FPS;

	if (start_ts == 0)
		goto EXIT;

	thr->Q2Q_time = end_ts - start_ts;

	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, targetfps, "[ux]target_fps");
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
		targettime, "[ux]target_time");

	if (ux_scroll_policy_type == SCROLL_POLICY_EAS_CTL){
		set_top_grp_aware(0,0);
		fpsgo_set_deplist_policy(thr, FPSGO_TASK_NONE);
	}
	fpsgo_get_fbt_mlock(__func__);
	ret = fbt_get_dep_list(thr);
	if (ret) {
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
			ret, "[UX] fail dep-list");
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id,
			0, "[UX] fail dep-list");
	}
	fpsgo_put_fbt_mlock(__func__);

	fpsgo_get_blc_mlock(__func__);
	if (thr->p_blc) {
		thr->p_blc->dep_num = thr->dep_valid_size;
		if (thr->dep_arr)
			memcpy(thr->p_blc->dep, thr->dep_arr,
					thr->dep_valid_size * sizeof(struct fpsgo_loading));
		else
			thr->p_blc->dep_num = 0;
	}
	fpsgo_put_blc_mlock(__func__);

	fbt_set_render_boost_attr(thr);
	loading = fbt_get_loading(thr, start_ts, end_ts);
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, loading, "[ux]compute_loading");

	/* unreliable targetfps */
	if (targetfps == -1) {
		fbt_reset_boost(thr);
		runtime = -1;
		goto EXIT;
	}

	thr->ux_blc_next = fbt_ux_cal_perf(runtime,
			targettime, targetfps, targetfps_ori, fps_margin,
			thr, end_ts, loading, targetfpks, cooler_on);

	if ((thr->pid != global_ux_max_pid && thr->ux_blc_next > global_ux_blc) ||
		thr->pid == global_ux_max_pid)
		fbt_ux_set_perf(thr->pid, thr->ux_blc_next);

EXIT:
	thr->ux_blc_cur = 0;
	fbt_ux_set_cap_with_sbe(thr);
	fpsgo_fbt2fstb_update_cpu_frame_info(thr->pid, thr->buffer_id,
		thr->tgid, thr->frame_type,
		0, thr->running_time, targettime,
		thr->ux_blc_next, 100, 0, 0);
}

void fbt_ux_frame_err(struct render_info *thr,
unsigned long long ts)
{
	if (!thr)
		return;

	if (ux_scroll_policy_type == SCROLL_POLICY_EAS_CTL){
		set_top_grp_aware(0,0);
		fpsgo_set_deplist_policy(thr, FPSGO_TASK_NONE);
	}
	thr->ux_blc_cur = 0;
	fbt_ux_set_cap_with_sbe(thr);
}

void fpsgo_ux_delete_frame_info(struct render_info *thr, struct ux_frame_info *info)
{
	if (!info)
		return;
	rb_erase(&info->entry, &(thr->ux_frame_info_tree));
	kmem_cache_free(frame_info_cachep, info);
}

struct ux_frame_info *fpsgo_ux_search_and_add_frame_info(struct render_info *thr,
		unsigned long long frameID, unsigned long long start_ts, int action)
{
	struct rb_node **p = &(thr->ux_frame_info_tree).rb_node;
	struct rb_node *parent = NULL;
	struct ux_frame_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct ux_frame_info, entry);
		if (frameID < tmp->frameID)
			p = &(*p)->rb_left;
		else if (frameID > tmp->frameID)
			p = &(*p)->rb_right;
		else
			return tmp;
	}
	if (action == 0)
		return NULL;
	if (frame_info_cachep)
		tmp = kmem_cache_alloc(frame_info_cachep,
			GFP_KERNEL | __GFP_ZERO);
	if (!tmp)
		return NULL;

	tmp->frameID = frameID;
	tmp->start_ts = start_ts;
	rb_link_node(&tmp->entry, parent, p);
	rb_insert_color(&tmp->entry, &(thr->ux_frame_info_tree));

	return tmp;
}

static struct ux_frame_info *fpsgo_ux_find_earliest_frame_info
	(struct render_info *thr)
{
	struct rb_node *cur;
	struct ux_frame_info *tmp = NULL, *ret = NULL;
	unsigned long long min_ts = ULLONG_MAX;

	cur = rb_first(&(thr->ux_frame_info_tree));

	while (cur) {
		tmp = rb_entry(cur, struct ux_frame_info, entry);
		if (tmp->start_ts < min_ts) {
			min_ts = tmp->start_ts;
			ret = tmp;
		}
		cur = rb_next(cur);
	}
	return ret;

}

int fpsgo_ux_count_frame_info(struct render_info *thr, int target)
{
	struct rb_node *cur;
	struct ux_frame_info *tmp = NULL;
	int ret = 0;
	int remove = 0;

	if (RB_EMPTY_ROOT(&thr->ux_frame_info_tree) == 1)
		return ret;

	cur = rb_first(&(thr->ux_frame_info_tree));
	while (cur) {
		cur = rb_next(cur);
		ret += 1;
	}
	/* error handling */
	while (ret > target) {
		tmp = fpsgo_ux_find_earliest_frame_info(thr);
		if (!tmp)
			break;

		fpsgo_ux_delete_frame_info(thr, tmp);
		ret -= 1;
		remove += 1;
	}
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, remove, "[ux]rb_err_remove");
	return ret;
}

void fpsgo_ux_reset(struct render_info *thr)
{
	struct rb_node *cur;
	struct rb_node *next;
	struct ux_frame_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	if (ux_scroll_policy_type == SCROLL_POLICY_EAS_CTL)
		set_top_grp_aware(0,0);

	cur = rb_first(&(thr->ux_frame_info_tree));

	while (cur) {
		next = rb_next(cur);
		tmp = rb_entry(cur, struct ux_frame_info, entry);
		rb_erase(&tmp->entry, &(thr->ux_frame_info_tree));
		kmem_cache_free(frame_info_cachep, tmp);
		cur = next;
	}

}

void fpsgo_set_affnity_on_rescue(int pid, int r_cpu_mask)
{
	if (fbt_set_affinity(pid, r_cpu_mask))
		FPSGO_LOGE("[comp] %s %d setaffinity fail\n",
				__func__, r_cpu_mask);
}

void fpsgo_sbe_rescue(struct render_info *thr, int start, int enhance,
		unsigned long long frame_id)
{
	int cpu_mask;

	if (!thr || !sbe_rescue_enable)	//thr must find the 5566 one.
		return;

	mutex_lock(&fbt_mlock);
	if (start) {
		if (frame_id)
			sbe_rescuing_frame_id = frame_id;
		thr->sbe_enhance = enhance < 0 ?  sbe_enhance_f : (enhance + sbe_enhance_f);
		thr->sbe_enhance = clamp(thr->sbe_enhance, 0, 100);
		if (thr->boost_info.sbe_rescue != 0)
			goto leave;
		thr->boost_info.sbe_rescue = 1;

		if (ux_scroll_policy_type == SCROLL_POLICY_EAS_CTL && rescue_cpu_mask == 1){
			cpu_mask = FPSGO_PREFER_M;
			fpsgo_set_affnity_on_rescue(thr->tgid, cpu_mask);
			fpsgo_set_affnity_on_rescue(thr->pid, cpu_mask);
		}
		fbt_ux_set_cap_with_sbe(thr);
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, thr->sbe_enhance, "[ux]sbe_rescue");
	} else {
		if (thr->boost_info.sbe_rescue == 0)
			goto leave;
		if (frame_id < sbe_rescuing_frame_id)
			goto leave;
		sbe_rescuing_frame_id = -1;
		thr->boost_info.sbe_rescue = 0;
		thr->sbe_enhance = 0;
		if (ux_scroll_policy_type == SCROLL_POLICY_EAS_CTL && rescue_cpu_mask == 1){
			cpu_mask = FPSGO_PREFER_NONE;
			fpsgo_set_affnity_on_rescue(thr->tgid,cpu_mask);
			fpsgo_set_affnity_on_rescue(thr->pid, cpu_mask);
		}
		fbt_ux_set_cap_with_sbe(thr);
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, thr->sbe_enhance, "[ux]sbe_rescue");
	}
leave:
	mutex_unlock(&fbt_mlock);
}

void fpsgo_sbe_rescue_legacy(struct render_info *thr, int start, int enhance,
		unsigned long long frame_id)
{
	int floor, blc_wt = 0, blc_wt_b = 0, blc_wt_m = 0;
	int max_cap = 100, max_cap_b = 100, max_cap_m = 100;
	int max_util = 1024, max_util_b = 1024, max_util_m = 1024;
	struct cpu_ctrl_data *pld;
	int rescue_opp_c, rescue_opp_f;
	int new_enhance;
	unsigned int temp_blc = 0;
	int temp_blc_pid = 0;
	unsigned long long temp_blc_buffer_id = 0;
	int temp_blc_dep_num = 0;
	int separate_aa;

	if (!thr || !sbe_rescue_enable)
		return;

	fbt_get_setting_info(&sinfo);
	pld = kcalloc(sinfo.cluster_num, sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	if (!pld)
		return;

	mutex_lock(&fbt_mlock);

	separate_aa = thr->attr.separate_aa_by_pid;

	if (start) {
		if (frame_id)
			sbe_rescuing_frame_id_legacy = frame_id;
		if (thr->boost_info.sbe_rescue != 0)
			goto leave;
		floor = thr->boost_info.last_blc;
		if (!floor)
			goto leave;

		rescue_opp_c = fbt_get_rescue_opp_c();
		new_enhance = enhance < 0 ?  sinfo.rescue_enhance_f : sbe_enhance_f;

		if (thr->boost_info.cur_stage == FPSGO_JERK_SECOND)
			rescue_opp_c = sinfo.rescue_second_copp;

		rescue_opp_f = fbt_get_rescue_opp_f();
		blc_wt = fbt_get_new_base_blc(pld, floor, new_enhance, rescue_opp_f, rescue_opp_c);
		if (separate_aa) {
			blc_wt_b = fbt_get_new_base_blc(pld, floor, new_enhance,
						rescue_opp_f, rescue_opp_c);
			blc_wt_m = fbt_get_new_base_blc(pld, floor, new_enhance,
						rescue_opp_f, rescue_opp_c);
		}

		if (!blc_wt)
			goto leave;
		thr->boost_info.sbe_rescue = 1;

		if (thr->boost_info.cur_stage != FPSGO_JERK_SECOND) {
			blc_wt = fbt_limit_capacity(blc_wt, 1);
			if (separate_aa) {
				blc_wt_b = fbt_limit_capacity(blc_wt_b, 1);
				blc_wt_m = fbt_limit_capacity(blc_wt_m, 1);
			}
			fbt_set_hard_limit_locked(FPSGO_HARD_CEILING, pld);

			thr->boost_info.cur_stage = FPSGO_JERK_SBE;

			if (thr->pid == sinfo.max_blc_pid && thr->buffer_id
				== sinfo.max_blc_buffer_id)
				fbt_set_max_blc_stage(FPSGO_JERK_SBE);
		}
		fbt_set_ceiling(pld, thr->pid, thr->buffer_id);

		if (sinfo.boost_ta) {
			fbt_set_boost_value(blc_wt);
			fbt_set_max_blc_cur(blc_wt);
		} else {
			fbt_cal_min_max_cap(thr, blc_wt, blc_wt_b, blc_wt_m, FPSGO_JERK_FIRST,
				thr->pid, thr->buffer_id, &blc_wt, &blc_wt_b, &blc_wt_m,
				&max_cap, &max_cap_b, &max_cap_m, &max_util,
				&max_util_b, &max_util_m);
			fbt_set_min_cap_locked(thr, blc_wt, blc_wt_b, blc_wt_m, max_cap,
				max_cap_b, max_cap_m, max_util, max_util_b,
				max_util_m, FPSGO_JERK_FIRST);
		}

		thr->boost_info.last_blc = blc_wt;

		if (separate_aa) {
			thr->boost_info.last_blc_b = blc_wt_b;
			thr->boost_info.last_blc_m = blc_wt_m;
		}

		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, new_enhance, "sbe rescue");

		/* support mode: sbe rescue until queue end */
		if (start == SBE_RESCUE_MODE_UNTIL_QUEUE_END) {
			thr->boost_info.sbe_rescue = 0;
			sbe_rescuing_frame_id_legacy = -1;
			fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "sbe rescue");
		}

	} else {
		if (thr->boost_info.sbe_rescue == 0)
			goto leave;
		if (frame_id < sbe_rescuing_frame_id_legacy)
			goto leave;
		sbe_rescuing_frame_id_legacy = -1;
		thr->boost_info.sbe_rescue = 0;
		blc_wt = thr->boost_info.last_normal_blc;
		if (separate_aa) {
			blc_wt_b = thr->boost_info.last_normal_blc_b;
			blc_wt_m = thr->boost_info.last_normal_blc_m;
		}
		if (!blc_wt || (separate_aa && !(blc_wt_b && blc_wt_m)))
			goto leave;

		/* find max perf index */
		fbt_find_max_blc(&temp_blc, &temp_blc_pid, &temp_blc_buffer_id,
				&temp_blc_dep_num, temp_blc_dep);
		rescue_opp_f = fbt_get_rescue_opp_f();
		fbt_get_new_base_blc(pld, temp_blc, 0, rescue_opp_f, sinfo.bhr_opp);
		fbt_set_ceiling(pld, thr->pid, thr->buffer_id);
		if (sinfo.boost_ta) {
			fbt_set_boost_value(blc_wt);
			fbt_set_max_blc_cur(blc_wt);
		} else {
			fbt_cal_min_max_cap(thr, blc_wt, blc_wt_b, blc_wt_m, FPSGO_JERK_FIRST,
				thr->pid, thr->buffer_id, &blc_wt, &blc_wt_b, &blc_wt_m,
				&max_cap, &max_cap_b, &max_cap_m, &max_util,
				&max_util_b, &max_util_m);
			fbt_set_min_cap_locked(thr, blc_wt, blc_wt_b, blc_wt_m, max_cap,
				max_cap_b, max_cap_m, max_util, max_util_b,
				max_util_m, FPSGO_JERK_FIRST);
		}
		fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, 0, "sbe rescue legacy");
	}
	fpsgo_systrace_c_fbt(thr->pid, thr->buffer_id, blc_wt, "perf idx");

leave:
	mutex_unlock(&fbt_mlock);
	kfree(pld);
}

void __exit fbt_cpu_ux_exit(void)
{
	kmem_cache_destroy(frame_info_cachep);
}

int __init fbt_cpu_ux_init(void)
{
	fpsgo_ux_gcc_enable = 0;
	sbe_rescue_enable = fbt_get_default_sbe_rescue_enable();
	sbe_rescuing_frame_id = -1;
	sbe_rescuing_frame_id_legacy = -1;
	sbe_enhance_f = 50;
	ux_scroll_policy_type = fbt_get_ux_scroll_policy_type();
	set_ux_uclampmin = 0;
	set_ux_uclampmax = 0;
	set_deplist_vip = 1;
	rescue_cpu_mask = 1;
	frame_info_cachep = kmem_cache_create("ux_frame_info",
		sizeof(struct ux_frame_info), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (!frame_info_cachep)
		return -1;

	return 0;
}
