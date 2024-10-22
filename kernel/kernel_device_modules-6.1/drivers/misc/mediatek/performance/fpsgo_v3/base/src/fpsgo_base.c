// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "fpsgo_base.h"
#include <asm/page.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/irq_work.h>
#include <linux/sched/clock.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>
#include <linux/cpufreq.h>
#include "sugov/cpufreq.h"
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "mt-plat/fpsgo_common.h"
#include "fpsgo_usedext.h"
#include "fpsgo_sysfs.h"
#include "fbt_cpu.h"
#include "fbt_cpu_platform.h"
#include "fps_composer.h"
#include "xgf.h"
#include "fbt_cpu_ux.h"
#include "fstb.h"

#include <linux/preempt.h>
#include <linux/trace_events.h>
#include <linux/fs.h>
#include <linux/rbtree.h>
#include <linux/sched/task.h>
#include <linux/kernel.h>
#include <trace/trace.h>
#include "sched/sched.h"

#define TIME_1S  1000000000ULL
#define TRAVERSE_PERIOD  300000000000ULL

#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
#include "fpsgo_trace_event.h"

#define event_trace(ip, fmt, args...) \
do { \
	__trace_printk_check_format(fmt, ##args);     \
	{	\
	static const char *trace_printk_fmt     \
	__section(__trace_printk_fmt) =  \
	__builtin_constant_p(fmt) ? fmt : NULL;   \
	__trace_bprintk(ip, trace_printk_fmt, ##args);    \
	}	\
} while (0)

static int total_fps_control_pid_info_num;
static int fpsgo_get_acquire_hint_enable;
static int cond_get_cam_apk_pid;
static int cond_get_cam_ser_pid;
static int global_cam_apk_pid;
static int global_kfps_mask = 0xFFF;
static int total_render_info_num;
static int total_linger_num;
static int total_BQ_id_num;
static int total_sbe_spid_loading_num;

static struct kobject *base_kobj;
static struct rb_root render_pid_tree;
static struct rb_root BQ_id_list;
static struct rb_root linger_tree;
static struct rb_root hwui_info_tree;
static struct rb_root sbe_info_tree;
static struct rb_root sbe_spid_loading_tree;
static struct rb_root fps_control_pid_info_tree;
static struct rb_root acquire_info_tree;
#if FPSGO_MW
static struct rb_root fpsgo_attr_by_pid_tree;
static struct rb_root fpsgo_attr_by_tid_tree;
#endif

void (*fpsgo_rl_delete_render_info_fp)(int pid, unsigned long long bufID);
EXPORT_SYMBOL(fpsgo_rl_delete_render_info_fp);

static DEFINE_MUTEX(fpsgo_render_lock);
static DEFINE_MUTEX(fpsgo2cam_apk_lock);
static DEFINE_MUTEX(fpsgo2cam_ser_lock);
static DECLARE_WAIT_QUEUE_HEAD(cam_apk_pid_queue);
static DECLARE_WAIT_QUEUE_HEAD(cam_ser_pid_queue);
static LIST_HEAD(cam_apk_pid_list);
static LIST_HEAD(cam_ser_pid_list);

long long fpsgo_task_sched_runtime(struct task_struct *p)
{
	//return task_sched_runtime(p);
	return p ? p->se.sum_exec_runtime : 0;
}

long fpsgo_sched_setaffinity(pid_t pid, const struct cpumask *in_mask)
{
	struct task_struct *p;
	int retval;

	rcu_read_lock();

	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}

	/* Prevent p going away */
	get_task_struct(p);
	rcu_read_unlock();

	if (p->flags & PF_NO_SETAFFINITY) {
		retval = -EINVAL;
		goto out_put_task;
	}
	retval = -EPERM;

	retval = set_cpus_allowed_ptr(p, in_mask);
out_put_task:
	put_task_struct(p);
	return retval;
}


void *fpsgo_alloc_atomic(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_ATOMIC);
	else
		pvBuf = vmalloc(i32Size);

	return pvBuf;
}

void fpsgo_free(void *pvBuf, int i32Size)
{
	if (!pvBuf)
		return;

	if (i32Size <= PAGE_SIZE)
		kfree(pvBuf);
	else
		vfree(pvBuf);
}

unsigned long long fpsgo_get_time(void)
{
	unsigned long long temp;

	preempt_disable();
	temp = cpu_clock(smp_processor_id());
	preempt_enable();

	return temp;
}

int fpsgo_arch_nr_clusters(void)
{
	int cpu, num = 0;
	struct cpufreq_policy *policy;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			num = 0;
			break;
		}

		num++;
		cpu = cpumask_last(policy->related_cpus);
		cpufreq_cpu_put(policy);
	}

	return num;

}

int fpsgo_arch_nr_get_opp_cpu(int cpu)
{
	struct cpufreq_policy *curr_policy = NULL;
	struct cpufreq_frequency_table *pos, *table;
	int idx;
	int nr_opp = 0;
	int ret = 0;

	curr_policy = kzalloc(sizeof(struct cpufreq_policy), GFP_KERNEL);
	if (!curr_policy)
		goto out;

	ret = cpufreq_get_policy(curr_policy, cpu);
	if (ret == 0) {
		table = curr_policy->freq_table;
		if (table) {
			cpufreq_for_each_valid_entry_idx(pos, table, idx) {
				nr_opp++;
			}
		}
	}

out:
	kfree(curr_policy);
	return nr_opp;
}

int fpsgo_arch_nr_get_cap_cpu(int cpu, int opp)
{
	unsigned long cap;

	if (opp < fpsgo_arch_nr_get_opp_cpu(cpu))
		cap = pd_get_opp_capacity_legacy(cpu, opp);
	else
		cap = pd_get_opp_capacity_legacy(cpu, 0);

	return (unsigned int) cap;
}

int fpsgo_arch_nr_max_opp_cpu(void)
{
	int num_opp = 0, max_opp = 0;
	int cpu;
	int ret = 0;
	struct cpufreq_policy *curr_policy = NULL;

	curr_policy = kzalloc(sizeof(struct cpufreq_policy), GFP_KERNEL);
	if (!curr_policy)
		goto out;

	for_each_possible_cpu(cpu) {
		ret = cpufreq_get_policy(curr_policy, cpu);
		if (ret != 0)
			continue;

		num_opp = fpsgo_arch_nr_get_opp_cpu(cpu);
		cpu = cpumask_last(curr_policy->related_cpus);

		if (max_opp < num_opp)
			max_opp = num_opp;
	}

out:
	kfree(curr_policy);
	return max_opp;
}

int fpsgo_arch_nr_freq_cpu(void)
{
	int  cpu, max_opp = 0;

	for_each_possible_cpu(cpu) {
		int opp = pd_get_cpu_opp(cpu);

		if (opp > max_opp)
			max_opp = opp;
	}

	return max_opp;
}

unsigned int fpsgo_cpufreq_get_freq_by_idx(
	int cpu, unsigned int opp)
{
	struct cpufreq_policy *curr_policy = NULL;
	struct cpufreq_frequency_table *pos, *table;
	int idx;
	int ret = 0;
	unsigned int max_freq = 0;

	curr_policy = kzalloc(sizeof(struct cpufreq_policy), GFP_KERNEL);
	if (!curr_policy)
		return 0;

	ret = cpufreq_get_policy(curr_policy, cpu);
	if (ret == 0) {
		table = curr_policy->freq_table;
		cpufreq_for_each_valid_entry_idx(pos, table, idx) {
			max_freq = max(pos->frequency, max_freq);
			if (idx == opp) {
				kfree(curr_policy);
				return pos->frequency;
			}
		}
	}

	kfree(curr_policy);
	return max_freq;
}

struct k_list {
	struct list_head queue_list;
	int fpsgo2pwr_cmd;
	int fpsgo2pwr_value1;
	int fpsgo2pwr_value2;
};
static LIST_HEAD(head);
static int condition_get_cmd;
static DEFINE_MUTEX(fpsgo2pwr_lock);
static DECLARE_WAIT_QUEUE_HEAD(pwr_queue);
void fpsgo_sentcmd(int cmd, int value1, int value2)
{
	static struct k_list *node;

	mutex_lock(&fpsgo2pwr_lock);
	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL)
		goto out;
	node->fpsgo2pwr_cmd = cmd;
	node->fpsgo2pwr_value1 = value1;
	node->fpsgo2pwr_value2 = value2;
	list_add_tail(&node->queue_list, &head);
	condition_get_cmd = 1;
out:
	mutex_unlock(&fpsgo2pwr_lock);
	wake_up_interruptible(&pwr_queue);
}

void fpsgo_ctrl2base_get_pwr_cmd(int *cmd, int *value1, int *value2)
{
	static struct k_list *node;

	wait_event_interruptible(pwr_queue, condition_get_cmd);
	mutex_lock(&fpsgo2pwr_lock);
	if (!list_empty(&head)) {
		node = list_first_entry(&head, struct k_list, queue_list);
		*cmd = node->fpsgo2pwr_cmd;
		*value1 = node->fpsgo2pwr_value1;
		*value2 = node->fpsgo2pwr_value2;
		list_del(&node->queue_list);
		kfree(node);
	}
	if (list_empty(&head))
		condition_get_cmd = 0;
	mutex_unlock(&fpsgo2pwr_lock);
}

static void fpsgo2cam_sentcmd(int cmd, int pid)
{
	struct cam_cmd_node *node = NULL;

	switch (cmd) {
	case CAMERA_APK:
		mutex_lock(&fpsgo2cam_apk_lock);
		node = kmalloc(sizeof(struct cam_cmd_node), GFP_KERNEL);
		if (!node)
			goto apk_out;
		node->target_pid = pid;
		list_add_tail(&node->queue_list, &cam_apk_pid_list);
		cond_get_cam_apk_pid = 1;
apk_out:
		mutex_unlock(&fpsgo2cam_apk_lock);
		wake_up_interruptible(&cam_apk_pid_queue);
		break;
	case CAMERA_SERVER:
		mutex_lock(&fpsgo2cam_ser_lock);
		node = kmalloc(sizeof(struct cam_cmd_node), GFP_KERNEL);
		if (!node)
			goto ser_out;
		node->target_pid = pid;
		list_add_tail(&node->queue_list, &cam_ser_pid_list);
		cond_get_cam_ser_pid = 1;
ser_out:
		mutex_unlock(&fpsgo2cam_ser_lock);
		wake_up_interruptible(&cam_ser_pid_queue);
		break;
	default:
		break;
	}
}

static void fpsgo_get_cam_pid(int cmd, int *pid)
{
	int local_camera_server_pid = 0;
	struct render_info *r_iter = NULL;
	struct acquire_info *a_iter = NULL;
	struct rb_node *rbn = NULL;

	if (!pid)
		return;

	*pid = 0;

	for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
		r_iter = rb_entry(rbn, struct render_info, render_key_node);
		fpsgo_thread_lock(&r_iter->thr_mlock);
		if (r_iter->api == NATIVE_WINDOW_API_CAMERA) {
			local_camera_server_pid = r_iter->tgid;
			fpsgo_thread_unlock(&r_iter->thr_mlock);
			break;
		}
		fpsgo_thread_unlock(&r_iter->thr_mlock);
	}

	if (!local_camera_server_pid)
		return;

	for (rbn = rb_first(&acquire_info_tree); rbn; rbn = rb_next(rbn)) {
		a_iter = rb_entry(rbn, struct acquire_info, entry);
		if (a_iter->api == NATIVE_WINDOW_API_CAMERA) {
			switch (cmd) {
			case CAMERA_APK:
				if (a_iter->p_pid == local_camera_server_pid) {
					*pid = a_iter->c_pid;
					global_cam_apk_pid = a_iter->c_pid;
				} else {
					*pid = a_iter->p_pid;
					global_cam_apk_pid = a_iter->p_pid;
				}
				break;
			case CAMERA_SERVER:
				*pid = local_camera_server_pid;
				break;
			default:
				break;
			}
			break;
		}
	}
}

static void fpsgo_check_consumer_is_hwui(int *pid)
{
	int i;
	int ret = 0;
	int *r_hwui_arr = NULL;
	int r_hwui_num = 0;
	struct render_info *r_iter = NULL;
	struct acquire_info *a_iter = NULL;
	struct rb_node *rbn = NULL;

	if (!pid)
		return;

	*pid = 0;

	r_hwui_arr = kcalloc(5, sizeof(int), GFP_KERNEL);
	if (!r_hwui_arr)
		return;

	for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
		r_iter = rb_entry(rbn, struct render_info, render_key_node);
		fpsgo_thread_lock(&r_iter->thr_mlock);
		if (r_iter->hwui == RENDER_INFO_HWUI_TYPE &&
			r_hwui_num < 5) {
			r_hwui_arr[r_hwui_num] = r_iter->pid;
			r_hwui_num++;
		}
		fpsgo_thread_unlock(&r_iter->thr_mlock);
	}

	if (!r_hwui_num) {
		kfree(r_hwui_arr);
		fpsgo_main_trace("[base] %s no hwui", __func__);
		return;
	}

	for (rbn = rb_first(&acquire_info_tree); rbn; rbn = rb_next(rbn)) {
		a_iter = rb_entry(rbn, struct acquire_info, entry);
		if (a_iter->api == NATIVE_WINDOW_API_CAMERA) {
			ret = -1;
			for (i = 0; i < r_hwui_num; i++) {
				if (a_iter->c_tid == r_hwui_arr[i]) {
					ret = a_iter->c_tid;
					break;
				}
			}
			if (ret > 0)
				break;
		}
	}

	*pid = ret;

	kfree(r_hwui_arr);
}

void fpsgo_ctrl2base_get_cam_pid(int cmd, int *pid)
{
	if (!pid)
		return;

	fpsgo_render_tree_lock(__func__);
	if (cmd == CAMERA_HWUI)
		fpsgo_check_consumer_is_hwui(pid);
	else
		fpsgo_get_cam_pid(cmd, pid);
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2base_get_cam_perf(int tgid, int *rtid, int *blc)
{
	struct render_info *r_iter = NULL;
	struct rb_node *rbn = NULL;

	if (!rtid || !blc)
		return;

	*rtid = 0;
	*blc = 0;
	fpsgo_render_tree_lock(__func__);
	for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
		r_iter = rb_entry(rbn, struct render_info, render_key_node);
		fpsgo_thread_lock(&r_iter->thr_mlock);
		if (r_iter->tgid == tgid &&
			r_iter->bq_type == ACQUIRE_CAMERA_TYPE &&
			r_iter->frame_type == NON_VSYNC_ALIGNED_TYPE) {
			*rtid = r_iter->pid;
			*blc = r_iter->p_blc ? r_iter->p_blc->blc : 0;
			fpsgo_thread_unlock(&r_iter->thr_mlock);
			break;
		}
		fpsgo_thread_unlock(&r_iter->thr_mlock);
	}
	fpsgo_render_tree_unlock(__func__);
}

void fpsgo_ctrl2base_wait_cam(int cmd, int *pid)
{
	struct cam_cmd_node *node = NULL;

	switch (cmd) {
	case CAMERA_APK:
		wait_event_interruptible(cam_apk_pid_queue, cond_get_cam_apk_pid);
		mutex_lock(&fpsgo2cam_apk_lock);
		if (!list_empty(&cam_apk_pid_list)) {
			node = list_first_entry(&cam_apk_pid_list,
				struct cam_cmd_node, queue_list);
			*pid = node->target_pid;
			list_del(&node->queue_list);
			kfree(node);
		}
		if (list_empty(&cam_apk_pid_list))
			cond_get_cam_apk_pid = 0;
		mutex_unlock(&fpsgo2cam_apk_lock);
		break;
	case CAMERA_SERVER:
		wait_event_interruptible(cam_ser_pid_queue, cond_get_cam_ser_pid);
		mutex_lock(&fpsgo2cam_ser_lock);
		if (!list_empty(&cam_ser_pid_list)) {
			node = list_first_entry(&cam_ser_pid_list,
				struct cam_cmd_node, queue_list);
			*pid = node->target_pid;
			list_del(&node->queue_list);
			kfree(node);
		}
		if (list_empty(&cam_ser_pid_list))
			cond_get_cam_ser_pid = 0;
		mutex_unlock(&fpsgo2cam_ser_lock);
		break;
	case CAMERA_HWUI:
		fpsgo_render_tree_lock(__func__);
		fpsgo_check_consumer_is_hwui(pid);
		fpsgo_render_tree_unlock(__func__);
		break;
	default:
		break;
	}
}

void fpsgo_ctrl2base_notify_cam_close(void)
{
	fpsgo_render_tree_lock(__func__);
	fpsgo_delete_acquire_info(2, 0, 0);
	fpsgo_render_tree_unlock(__func__);
}

int fpsgo_get_acquire_hint_is_enable(void)
{
	return fpsgo_get_acquire_hint_enable;
}

static int fpsgo_update_tracemark(void)
{
	return 1;
}

static int fpsgo_systrace_enabled(int type)
{
	int ret = 1;

	switch (type) {
	case FPSGO_DEBUG_MANDATORY:
		if (!trace_fpsgo_main_systrace_enabled())
			ret = 0;
		break;
	case FPSGO_DEBUG_FBT:
		if (!trace_fbt_systrace_enabled())
			ret = 0;
		break;
	case FPSGO_DEBUG_FSTB:
		if (!trace_fstb_systrace_enabled())
			ret = 0;
		break;
	case FPSGO_DEBUG_XGF:
		if (!trace_xgf_systrace_enabled())
			ret = 0;
		break;
	case FPSGO_DEBUG_FBT_CTRL:
		if (!trace_fbt_ctrl_systrace_enabled())
			ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

static void __fpsgo_systrace_print(int type, char *buf)
{
	switch (type) {
	case FPSGO_DEBUG_MANDATORY:
		trace_fpsgo_main_systrace(buf);
		break;
	case FPSGO_DEBUG_FBT:
		trace_fbt_systrace(buf);
		break;
	case FPSGO_DEBUG_FSTB:
		trace_fstb_systrace(buf);
		break;
	case FPSGO_DEBUG_XGF:
		trace_xgf_systrace(buf);
		break;
	case FPSGO_DEBUG_FBT_CTRL:
		trace_fbt_ctrl_systrace(buf);
		break;
	default:
		break;
	}
}

void __fpsgo_systrace_c(int type, pid_t pid, unsigned long long bufID,
	int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf2[256];

	if (!fpsgo_systrace_enabled(type))
		return;

	if (unlikely(!fpsgo_update_tracemark()))
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	if (!bufID) {
		len = snprintf(buf2, sizeof(buf2), "C|%d|%s|%d\n", pid, log, val);
	} else {
		len = snprintf(buf2, sizeof(buf2), "C|%d|%s|%d|0x%llx\n",
			pid, log, val, bufID);
	}
	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';

	__fpsgo_systrace_print(type, buf2);
}

void __fpsgo_systrace_b(int type, pid_t tgid, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf2[256];

	if (!fpsgo_systrace_enabled(type))
		return;

	if (unlikely(!fpsgo_update_tracemark()))
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf2, sizeof(buf2), "B|%d|%s\n", tgid, log);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';

	__fpsgo_systrace_print(type, buf2);
}

void __fpsgo_systrace_e(int type)
{
	char buf2[256];
	int len;

	if (!fpsgo_systrace_enabled(type))
		return;

	if (unlikely(!fpsgo_update_tracemark()))
		return;

	len = snprintf(buf2, sizeof(buf2), "E\n");

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';

	__fpsgo_systrace_print(type, buf2);
}

void fpsgo_main_trace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;

	if (!trace_fpsgo_main_trace_enabled())
		return;

	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);

	if (unlikely(len == 256))
		log[255] = '\0';
	va_end(args);
	trace_fpsgo_main_trace(log);
}
EXPORT_SYMBOL(fpsgo_main_trace);

void fpsgo_render_tree_lock(const char *tag)
{
	mutex_lock(&fpsgo_render_lock);
}

void fpsgo_render_tree_unlock(const char *tag)
{
	mutex_unlock(&fpsgo_render_lock);
}

void fpsgo_lockprove(const char *tag)
{
	WARN_ON(!mutex_is_locked(&fpsgo_render_lock));
}

void fpsgo_thread_lock(struct mutex *mlock)
{
	fpsgo_lockprove(__func__);
	mutex_lock(mlock);
}

void fpsgo_thread_unlock(struct mutex *mlock)
{
	mutex_unlock(mlock);
}

void fpsgo_thread_lockprove(const char *tag, struct mutex *mlock)
{
	WARN_ON(!mutex_is_locked(mlock));
}

int fpsgo_get_tgid(int pid)
{
	struct task_struct *tsk;
	int tgid = 0;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk)
		return 0;

	tgid = tsk->tgid;
	put_task_struct(tsk);

	return tgid;
}

void fpsgo_add_linger(struct render_info *thr)
{
	struct rb_node **p = &linger_tree.rb_node;
	struct rb_node *parent = NULL;
	struct render_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	if (!thr)
		return;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct render_info, linger_node);
		if ((uintptr_t)thr < (uintptr_t)tmp)
			p = &(*p)->rb_left;
		else if ((uintptr_t)thr > (uintptr_t)tmp)
			p = &(*p)->rb_right;
		else {
			FPSGO_LOGE("linger exist %d(%p)\n", thr->pid, thr);
			return;
		}
	}

	rb_link_node(&thr->linger_node, parent, p);
	rb_insert_color(&thr->linger_node, &linger_tree);
	total_linger_num++;
	thr->linger_ts = fpsgo_get_time();
	FPSGO_LOGI("add to linger %d(%p)(%llu)\n",
			thr->pid, thr, thr->linger_ts);
}

void fpsgo_del_linger(struct render_info *thr)
{
	fpsgo_lockprove(__func__);

	if (!thr)
		return;

	rb_erase(&thr->linger_node, &linger_tree);
	total_linger_num--;
	FPSGO_LOGI("del from linger %d(%p)\n", thr->pid, thr);
}

static int fpsgo_fbt_delete_rl_render(int pid, unsigned long long buf_id)
{
	int ret = 0;

	if (fpsgo_rl_delete_render_info_fp) {
		fpsgo_rl_delete_render_info_fp(pid, buf_id);
	} else {
		ret = -ENOENT;
		// mtk_base_dprintk_always("%s is NULL\n", __func__);
	}
	return ret;
}

void fpsgo_traverse_linger(unsigned long long cur_ts)
{
	struct rb_node *n;
	struct render_info *pos;
	unsigned long long expire_ts;

	fpsgo_lockprove(__func__);

	if (cur_ts < TRAVERSE_PERIOD)
		return;

	expire_ts = cur_ts - TRAVERSE_PERIOD;

	n = rb_first(&linger_tree);
	while (n) {
		int tofree = 0;

		pos = rb_entry(n, struct render_info, linger_node);
		FPSGO_LOGI("-%d(%p)(%llu),", pos->pid, pos, pos->linger_ts);

		fpsgo_thread_lock(&pos->thr_mlock);

		if (pos->linger_ts && pos->linger_ts < expire_ts) {
			FPSGO_LOGI("timeout %d(%p)(%llu),",
				pos->pid, pos, pos->linger_ts);
			fpsgo_base2fbt_cancel_jerk(pos);
			fpsgo_del_linger(pos);
			tofree = 1;
			n = rb_first(&linger_tree);
		} else
			n = rb_next(n);

		fpsgo_thread_unlock(&pos->thr_mlock);

		if (tofree) {
			fpsgo_fbt_delete_rl_render(pos->pid, pos->buffer_id);
			vfree(pos);
		}
	}
}

int fpsgo_base_is_finished(struct render_info *thr)
{
	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(thr->thr_mlock));

	if (!fpsgo_base2fbt_is_finished(thr))
		return 0;

	return 1;
}

void fpsgo_reset_attr(struct fpsgo_boost_attr *boost_attr)
{
	if (boost_attr) {
		boost_attr->llf_task_policy_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->light_loading_policy_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->loading_th_by_pid = BY_PID_DEFAULT_VAL;

		boost_attr->rescue_enable_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->rescue_second_enable_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->rescue_second_time_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->rescue_second_group_by_pid = BY_PID_DEFAULT_VAL;

		boost_attr->group_by_lr_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->heavy_group_num_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->second_group_num_by_pid = BY_PID_DEFAULT_VAL;

		boost_attr->filter_frame_enable_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->filter_frame_window_size_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->filter_frame_kmin_by_pid = BY_PID_DEFAULT_VAL;

		boost_attr->boost_affinity_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->cpumask_heavy_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->cpumask_second_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->cpumask_others_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->boost_lr_by_pid = BY_PID_DEFAULT_VAL;

		boost_attr->separate_aa_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->separate_release_sec_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->limit_uclamp_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->limit_ruclamp_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->limit_uclamp_m_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->limit_ruclamp_m_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->separate_pct_b_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->separate_pct_m_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->separate_pct_other_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->qr_enable_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->qr_t2wnt_x_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->qr_t2wnt_y_p_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->qr_t2wnt_y_n_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_enable_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_fps_margin_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_up_sec_pct_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_up_step_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_down_sec_pct_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_down_step_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_reserved_up_quota_pct_by_pid =
			BY_PID_DEFAULT_VAL;
		boost_attr->gcc_reserved_down_quota_pct_by_pid =
			BY_PID_DEFAULT_VAL;
		boost_attr->gcc_enq_bound_thrs_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_deq_bound_thrs_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_enq_bound_quota_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->gcc_deq_bound_quota_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->blc_boost_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->boost_vip_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->rt_prio1_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->rt_prio2_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->rt_prio3_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->set_ls_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->ls_groupmask_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->vip_mask_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->set_vvip_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->check_buffer_quota_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->expected_fps_margin_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->quota_v2_diff_clamp_min_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->quota_v2_diff_clamp_max_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->limit_min_cap_target_t_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->aa_b_minus_idle_t_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->limit_cfreq2cap_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->limit_rfreq2cap_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->limit_cfreq2cap_m_by_pid = BY_PID_DEFAULT_VAL;
		boost_attr->limit_rfreq2cap_m_by_pid = BY_PID_DEFAULT_VAL;
	}
}

static int render_key_compare(struct fbt_render_key target,
		struct fbt_render_key node_key)
{
	if (target.key1 > node_key.key1)
		return 1;
	else if (target.key1 < node_key.key1)
		return -1;

	/* key 1 is equal, compare key 2 */
	if (target.key2 > node_key.key2)
		return 1;
	else if (target.key2 < node_key.key2)
		return -1;
	else
		return 0;
}

void fpsgo_reset_render_attr(int pid, int set_by_tid)
{
	struct rb_node *n;
	struct render_info *iter;

	if (!pid)
		return;

	fpsgo_lockprove(__func__);

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);

		if ((iter->pid == pid && set_by_tid) ||
			(iter->tgid == pid && !set_by_tid)) {
			fpsgo_thread_lock(&(iter->thr_mlock));
			fpsgo_reset_attr(&(iter->attr));
			fbt_set_render_boost_attr(iter);
			fpsgo_thread_unlock(&(iter->thr_mlock));
		}
	}
}

struct render_info *eara2fpsgo_search_render_info(int pid,
		unsigned long long buffer_id)
{
	struct render_info *iter = NULL;
	struct rb_node *n;

	fpsgo_lockprove(__func__);

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);
		if (iter->pid == pid && iter->buffer_id == buffer_id)
			return iter;
	}
	return NULL;
}

static int fpsgo_is_exceed_render_info_limit(void)
{
	int ret = 0;

	if (total_render_info_num + total_linger_num > FPSGO_MAX_RENDER_INFO_SIZE) {
		ret = 1;
		fpsgo_main_trace("[%s] total_render_info_num=%d,total_linger_num=%d",
			__func__, total_render_info_num, total_linger_num);
#ifdef FPSGO_DEBUG
		struct render_info *r_iter = NULL;
		struct rb_node *rbn = NULL;
		for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
			r_iter = rb_entry(rbn, struct render_info, render_key_node);
			FPSGO_LOGE("[base] %s render %d 0x%llx exist\n",
				__func__, r_iter->pid, r_iter->buffer_id);
		}
		for (rbn = rb_first(&linger_tree); rbn; rbn = rb_next(rbn)) {
			r_iter = rb_entry(rbn, struct render_info, linger_node);
			FPSGO_LOGE("[base] %s linger %d 0x%llx exist\n",
				__func__, r_iter->pid, r_iter->buffer_id);
		}
#endif
	}

	return ret;
}

struct render_info *fpsgo_search_and_add_render_info(int pid,
	unsigned long long identifier, int force)
{
	struct rb_node **p = &render_pid_tree.rb_node;
	struct rb_node *parent = NULL;
	struct render_info *iter_thr = NULL;
	int tgid;
	struct fbt_render_key render_key;
	unsigned long local_master_type = 0;

	render_key.key1 = pid;
	render_key.key2 = identifier;

	fpsgo_lockprove(__func__);

	tgid = fpsgo_get_tgid(pid);

	while (*p) {
		parent = *p;
		iter_thr = rb_entry(parent, struct render_info, render_key_node);

		if (render_key_compare(render_key, iter_thr->render_key) < 0)
			p = &(*p)->rb_left;
		else if (render_key_compare(render_key, iter_thr->render_key) > 0)
			p = &(*p)->rb_right;
		else
			return iter_thr;
	}

	if (!force || fpsgo_is_exceed_render_info_limit())
		return NULL;

	iter_thr = vzalloc(sizeof(struct render_info));
	if (!iter_thr)
		return NULL;

	mutex_init(&iter_thr->thr_mlock);
	mutex_init(&iter_thr->ux_mlock);
	INIT_LIST_HEAD(&(iter_thr->bufferid_list));
	iter_thr->pid = pid;
	iter_thr->render_key.key1 = pid;
	iter_thr->render_key.key2 = identifier;
	iter_thr->identifier = identifier;
	iter_thr->tgid = tgid;
	iter_thr->frame_type = BY_PASS_TYPE;
	iter_thr->bq_type = ACQUIRE_UNKNOWN_TYPE;
	iter_thr->ux_frame_info_tree = RB_ROOT;
	iter_thr->sbe_enhance = 0;
	iter_thr->t_last_start = 0;
	set_bit(FPSGO_TYPE, &local_master_type);
	iter_thr->master_type = local_master_type;

	fbt_set_render_boost_attr(iter_thr);

	rb_link_node(&iter_thr->render_key_node, parent, p);
	rb_insert_color(&iter_thr->render_key_node, &render_pid_tree);
	total_render_info_num++;

	return iter_thr;
}

void fpsgo_delete_render_info(int pid,
	unsigned long long buffer_id, unsigned long long identifier)
{
	struct render_info *data;
	int delete = 0;
	int check_max_blc = 0;
	int max_pid = 0;
	unsigned long long max_buffer_id = 0;
	int max_ret;

	fpsgo_lockprove(__func__);

	data = fpsgo_search_and_add_render_info(pid, identifier, 0);

	if (!data)
		return;
	fpsgo_thread_lock(&data->thr_mlock);
	max_ret = fpsgo_base2fbt_get_max_blc_pid(&max_pid, &max_buffer_id);
	if (max_ret && pid == max_pid && buffer_id == max_buffer_id)
		check_max_blc = 1;

	rb_erase(&data->render_key_node, &render_pid_tree);
	total_render_info_num--;
	list_del(&(data->bufferid_list));
	fpsgo_base2fbt_item_del(data->p_blc, data->dep_arr, data);
	data->p_blc = NULL;
	data->dep_arr = NULL;

	if (fpsgo_base_is_finished(data))
		delete = 1;
	else {
		delete = 0;
		data->linger = 1;
		fpsgo_add_linger(data);
	}

	fpsgo_fstb2other_info_update(data->pid,
		data->buffer_id, FPSGO_DELETE, 0, 0, 0, 0);

	fpsgo_thread_unlock(&data->thr_mlock);

	fpsgo_delete_hwui_info(data->pid);
	fpsgo_delete_sbe_info(data->pid);
	switch_thread_max_fps(data->pid, 0);

	if (check_max_blc)
		fpsgo_base2fbt_check_max_blc();

	if (delete == 1) {
		fpsgo_fbt_delete_rl_render(pid, buffer_id);
		vfree(data);
	}
}

struct hwui_info *fpsgo_search_and_add_hwui_info(int pid, int force)
{
	struct rb_node **p = &hwui_info_tree.rb_node;
	struct rb_node *parent = NULL;
	struct hwui_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct hwui_info, entry);

		if (pid < tmp->pid)
			p = &(*p)->rb_left;
		else if (pid > tmp->pid)
			p = &(*p)->rb_right;
		else
			return tmp;
	}

	if (!force)
		return NULL;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;

	tmp->pid = pid;

	rb_link_node(&tmp->entry, parent, p);
	rb_insert_color(&tmp->entry, &hwui_info_tree);

	return tmp;
}

void fpsgo_delete_hwui_info(int pid)
{
	struct hwui_info *data;

	fpsgo_lockprove(__func__);

	data = fpsgo_search_and_add_hwui_info(pid, 0);

	if (!data)
		return;

	rb_erase(&data->entry, &hwui_info_tree);
	kfree(data);
}

#if FPSGO_MW
int is_to_delete_fpsgo_attr(struct fpsgo_attr_by_pid *fpsgo_attr)
{
	struct fpsgo_boost_attr boost_attr;

	if (!fpsgo_attr)
		return 0;

	boost_attr = fpsgo_attr->attr;
	if	(boost_attr.rescue_enable_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.rescue_second_enable_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.rescue_second_time_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.rescue_second_group_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.group_by_lr_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.heavy_group_num_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.second_group_num_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.llf_task_policy_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.light_loading_policy_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.loading_th_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.filter_frame_enable_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.filter_frame_window_size_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.filter_frame_kmin_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.boost_affinity_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.cpumask_heavy_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.cpumask_second_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.cpumask_others_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.boost_lr_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.separate_aa_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.separate_release_sec_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.limit_uclamp_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.limit_ruclamp_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.limit_uclamp_m_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.limit_ruclamp_m_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.separate_pct_b_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.separate_pct_m_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.separate_pct_other_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.qr_enable_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.qr_t2wnt_x_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.qr_t2wnt_y_n_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.qr_t2wnt_y_p_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.blc_boost_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.boost_vip_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.rt_prio1_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.rt_prio2_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.rt_prio3_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.set_ls_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.ls_groupmask_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.vip_mask_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.set_vvip_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_deq_bound_quota_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_deq_bound_thrs_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_down_sec_pct_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_down_step_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_enable_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_enq_bound_quota_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_enq_bound_thrs_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_fps_margin_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_reserved_down_quota_pct_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_reserved_up_quota_pct_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_up_sec_pct_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.gcc_up_step_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.check_buffer_quota_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.expected_fps_margin_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.quota_v2_diff_clamp_min_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.quota_v2_diff_clamp_max_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.limit_min_cap_target_t_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.aa_b_minus_idle_t_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.limit_cfreq2cap_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.limit_rfreq2cap_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.limit_cfreq2cap_m_by_pid == BY_PID_DEFAULT_VAL &&
			boost_attr.limit_rfreq2cap_m_by_pid == BY_PID_DEFAULT_VAL) {
		return 1;
	}
	return 0;
}

static int delete_oldest_pid_attr(void)
{
	struct rb_node *n;
	struct fpsgo_attr_by_pid *iter;
	unsigned long long oldest_ts = (unsigned long long)-1; // max val
	int tgid = 0, count = 0;

	fpsgo_lockprove(__func__);

	for (n = rb_first(&fpsgo_attr_by_pid_tree), count = 0; n != NULL;
		n = rb_next(n), count++) {
		iter = rb_entry(n,  struct fpsgo_attr_by_pid, entry);
		if (iter->ts < oldest_ts) {
			tgid = iter->tgid;
			oldest_ts = iter->ts;
		}
	}

	if (count >= FPSGO_MAX_TREE_SIZE)
		return tgid;
	else
		return 0;
}

struct fpsgo_attr_by_pid *fpsgo_find_attr_by_pid(int pid, int add_new)
{
	struct rb_node **p = &fpsgo_attr_by_pid_tree.rb_node;
	struct rb_node *parent = NULL;
	struct fpsgo_attr_by_pid *iter_attr = NULL;
	int delete_tgid = 0;
	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		iter_attr = rb_entry(parent, struct fpsgo_attr_by_pid, entry);

		if (pid < iter_attr->tgid)
			p = &(*p)->rb_left;
		else if (pid > iter_attr->tgid)
			p = &(*p)->rb_right;
		else
			return iter_attr;
	}

	if (!add_new)
		return NULL;

	iter_attr = kzalloc(sizeof(*iter_attr), GFP_KERNEL);
	if (!iter_attr)
		return NULL;

	iter_attr->ts = fpsgo_get_time();
	iter_attr->tgid = pid;
	fpsgo_reset_attr(&(iter_attr->attr));
	rb_link_node(&iter_attr->entry, parent, p);
	rb_insert_color(&iter_attr->entry, &fpsgo_attr_by_pid_tree);

	/* If the tree size exceeds, then delete the oldest node. */
	delete_tgid = delete_oldest_pid_attr();
	if (delete_tgid)
		delete_attr_by_pid(delete_tgid);

	return iter_attr;
}

void delete_attr_by_pid(int tgid)
{
	struct fpsgo_attr_by_pid *data;

	fpsgo_lockprove(__func__);
	data = fpsgo_find_attr_by_pid(tgid, 0);
	if (!data)
		return;
	rb_erase(&data->entry, &fpsgo_attr_by_pid_tree);
	kfree(data);
}

void delete_all_pid_attr_items_in_tree(void)
{
	struct rb_node *n;
	struct fpsgo_attr_by_pid *iter;

	fpsgo_lockprove(__func__);

	n = rb_first(&fpsgo_attr_by_pid_tree);

	while (n) {
		iter = rb_entry(n,  struct fpsgo_attr_by_pid, entry);

		rb_erase(&iter->entry, &fpsgo_attr_by_pid_tree);
		n = rb_first(&fpsgo_attr_by_pid_tree);

		kfree(iter);
	}
}

int is_to_delete_fpsgo_tid_attr(struct fpsgo_attr_by_pid *fpsgo_attr)
{
	struct fpsgo_boost_attr boost_attr;

	if (!fpsgo_attr)
		return 0;

	boost_attr = fpsgo_attr->attr;
	if (boost_attr.rescue_enable_by_pid == BY_PID_DEFAULT_VAL)
		return 1;
	if (boost_attr.rescue_enable_by_pid == BY_PID_DELETE_VAL)
		return 1;
	return 0;
}
static int delete_oldest_tid_attr(void)
{
	struct rb_node *n;
	struct fpsgo_attr_by_pid *iter;
	unsigned long long oldest_ts = (unsigned long long)-1; // max val
	int tid = 0, count = 0;

	fpsgo_lockprove(__func__);

	for (n = rb_first(&fpsgo_attr_by_tid_tree), count = 0; n != NULL;
		n = rb_next(n), count++) {
		iter = rb_entry(n,  struct fpsgo_attr_by_pid, entry);
		if (iter->ts < oldest_ts) {
			tid = iter->tid;
			oldest_ts = iter->ts;
		}
	}

	if (count >= FPSGO_MAX_TREE_SIZE)
		return tid;
	else
		return 0;
}

struct fpsgo_attr_by_pid *fpsgo_find_attr_by_tid(int pid, int add_new)
{
	struct rb_node **p = &fpsgo_attr_by_tid_tree.rb_node;
	struct rb_node *parent = NULL;
	struct fpsgo_attr_by_pid *iter_attr = NULL;
	int delete_tid = 0;

	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		iter_attr = rb_entry(parent, struct fpsgo_attr_by_pid, entry);

		if (pid < iter_attr->tid)
			p = &(*p)->rb_left;
		else if (pid > iter_attr->tid)
			p = &(*p)->rb_right;
		else
			return iter_attr;
	}

	if (!add_new)
		return NULL;

	iter_attr = kzalloc(sizeof(*iter_attr), GFP_KERNEL);
	if (!iter_attr)
		return NULL;

	iter_attr->ts = fpsgo_get_time();
	iter_attr->tid = pid;
	fpsgo_reset_attr(&(iter_attr->attr));
	rb_link_node(&iter_attr->entry, parent, p);
	rb_insert_color(&iter_attr->entry, &fpsgo_attr_by_tid_tree);

	/* If the tree size exceeds, then delete the oldest node. */
	delete_tid = delete_oldest_tid_attr();
	if (delete_tid)
		delete_attr_by_tid(delete_tid);

	return iter_attr;
}

void delete_attr_by_tid(int tid)
{
	struct fpsgo_attr_by_pid *data;

	fpsgo_lockprove(__func__);
	data = fpsgo_find_attr_by_tid(tid, 0);
	if (!data)
		return;
	rb_erase(&data->entry, &fpsgo_attr_by_tid_tree);
	kfree(data);
}

void delete_all_tid_attr_items_in_tree(void)
{
	struct rb_node *n;
	struct fpsgo_attr_by_pid *iter;

	fpsgo_lockprove(__func__);

	n = rb_first(&fpsgo_attr_by_tid_tree);

	while (n) {
		iter = rb_entry(n,  struct fpsgo_attr_by_pid, entry);

		rb_erase(&iter->entry, &fpsgo_attr_by_tid_tree);
		n = rb_first(&fpsgo_attr_by_tid_tree);

		kfree(iter);
	}
}

#endif  // FPSGO_MW

struct sbe_info *fpsgo_search_and_add_sbe_info(int pid, int force)
{
	struct rb_node **p = &sbe_info_tree.rb_node;
	struct rb_node *parent = NULL;
	struct sbe_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct sbe_info, entry);

		if (pid < tmp->pid)
			p = &(*p)->rb_left;
		else if (pid > tmp->pid)
			p = &(*p)->rb_right;
		else
			return tmp;
	}

	if (!force)
		return NULL;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;

	tmp->pid = pid;

	rb_link_node(&tmp->entry, parent, p);
	rb_insert_color(&tmp->entry, &sbe_info_tree);

	return tmp;
}

void fpsgo_delete_sbe_info(int pid)
{
	struct sbe_info *data;

	fpsgo_lockprove(__func__);

	data = fpsgo_search_and_add_sbe_info(pid, 0);

	if (!data)
		return;

	rb_erase(&data->entry, &sbe_info_tree);
	kfree(data);
}

void fpsgo_delete_oldest_fps_control_pid_info(void)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct fps_control_pid_info *min_iter = NULL, *tmp_iter = NULL;
	struct rb_root *rbr = NULL;
	struct rb_node *rbn = NULL;

	if (RB_EMPTY_ROOT(&fps_control_pid_info_tree))
		return;

	rbr = &fps_control_pid_info_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		tmp_iter = rb_entry(rbn, struct fps_control_pid_info, entry);
		if (tmp_iter->ts < min_ts) {
			min_ts = tmp_iter->ts;
			min_iter = tmp_iter;
		}
	}

	if (!min_iter)
		return;

	rb_erase(&min_iter->entry, &fps_control_pid_info_tree);
	kfree(min_iter);
	total_fps_control_pid_info_num--;
}

struct fps_control_pid_info *fpsgo_search_and_add_fps_control_pid(int pid, int force)
{
	struct rb_node **p = &fps_control_pid_info_tree.rb_node;
	struct rb_node *parent = NULL;
	struct fps_control_pid_info *tmp = NULL;

	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct fps_control_pid_info, entry);

		if (pid < tmp->pid)
			p = &(*p)->rb_left;
		else if (pid > tmp->pid)
			p = &(*p)->rb_right;
		else
			return tmp;
	}

	if (!force)
		return NULL;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;

	tmp->pid = pid;
	tmp->ts = fpsgo_get_time();

	rb_link_node(&tmp->entry, parent, p);
	rb_insert_color(&tmp->entry, &fps_control_pid_info_tree);
	total_fps_control_pid_info_num++;

	if (total_fps_control_pid_info_num > FPSGO_MAX_TREE_SIZE)
		fpsgo_delete_oldest_fps_control_pid_info();

	return tmp;
}

void fpsgo_delete_fpsgo_control_pid(int pid)
{
	struct fps_control_pid_info *data;

	fpsgo_lockprove(__func__);

	data = fpsgo_search_and_add_fps_control_pid(pid, 0);

	if (!data)
		return;

	rb_erase(&data->entry, &fps_control_pid_info_tree);
	kfree(data);
	total_fps_control_pid_info_num--;
}

int fpsgo_get_all_fps_control_pid_info(struct fps_control_pid_info *arr)
{
	int index = 0;
	struct fps_control_pid_info *iter = NULL;
	struct rb_root *rbr = NULL;
	struct rb_node *rbn = NULL;

	rbr = &fps_control_pid_info_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct fps_control_pid_info, entry);
		arr[index].pid = iter->pid;
		arr[index].ts = iter->ts;
		index++;
		if (index >= FPSGO_MAX_TREE_SIZE)
			break;
	}

	return index;
}

int fpsgo_get_all_sbe_info(struct sbe_info *arr)
{
	int index = 0;
	struct sbe_info *iter = NULL;
	struct rb_root *rbr = NULL;
	struct rb_node *rbn = NULL;

	rbr = &sbe_info_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct sbe_info, entry);
		arr[index].pid = iter->pid;
		index++;
		if (index >= FPSGO_MAX_TREE_SIZE)
			break;
	}

	return index;
}

static void fpsgo_check_BQid_status(void)
{
	struct rb_node *rbn = NULL;
	struct BQ_id *pos = NULL;
	int tgid = 0;

	rbn = rb_first(&BQ_id_list);
	while (rbn) {
		pos = rb_entry(rbn, struct BQ_id, entry);
		tgid = fpsgo_get_tgid(pos->pid);
		if (tgid) {
			rbn = rb_next(rbn);
		} else {
			rb_erase(rbn, &BQ_id_list);
			kfree(pos);
			total_BQ_id_num--;
			rbn = rb_first(&BQ_id_list);
		}
	}
}

static void fpsgo_check_acquire_info_status(void)
{
	int local_p_tgid = 0, local_c_tgid =  0;
	struct acquire_info *iter = NULL;
	struct rb_node *rbn = NULL;

	rbn = rb_first(&acquire_info_tree);
	while (rbn) {
		iter = rb_entry(rbn, struct acquire_info, entry);
		local_p_tgid = fpsgo_get_tgid(iter->p_pid);
		local_c_tgid = fpsgo_get_tgid(iter->c_pid);
		if (local_p_tgid && local_c_tgid)
			rbn = rb_next(rbn);
		else {
			rb_erase(rbn, &acquire_info_tree);
			kfree(iter);
			rbn = rb_first(&acquire_info_tree);
		}
	}
}

static void fpsgo_check_adpf_render_status(void)
{
	int local_tgid = 0;
	struct render_info *iter = NULL;
	struct rb_node *rbn = NULL;

	rbn = rb_first(&render_pid_tree);
	while (rbn) {
		iter = rb_entry(rbn, struct render_info, render_key_node);
		fpsgo_thread_lock(&iter->thr_mlock);
		if (!test_bit(ADPF_TYPE, &iter->master_type)) {
			rbn = rb_next(rbn);
			fpsgo_thread_unlock(&iter->thr_mlock);
			continue;
		}

		local_tgid = fpsgo_get_tgid(iter->tgid);
		if (local_tgid) {
			rbn = rb_next(rbn);
			fpsgo_thread_unlock(&iter->thr_mlock);
		} else {
			rb_erase(rbn, &render_pid_tree);
			total_render_info_num--;
			fpsgo_fstb2other_info_update(iter->pid,
				iter->buffer_id, FPSGO_DELETE, 0, 0, 0, 0);
			fpsgo_thread_unlock(&iter->thr_mlock);
			vfree(iter);
			rbn = rb_first(&render_pid_tree);
		}
	}
}

static void fpsgo_check_sbe_spid_loading_status(void)
{
	int local_tgid = 0;
	struct sbe_spid_loading *iter = NULL;
	struct rb_node *rbn = NULL;

	rbn = rb_first(&sbe_spid_loading_tree);
	while (rbn) {
		iter = rb_entry(rbn, struct sbe_spid_loading, rb_node);
		local_tgid = fpsgo_get_tgid(iter->tgid);
		if (local_tgid)
			rbn = rb_next(rbn);
		else {
			rb_erase(rbn, &sbe_spid_loading_tree);
			kfree(iter);
			total_sbe_spid_loading_num--;
			rbn = rb_first(&sbe_spid_loading_tree);
		}
	}
}

void fpsgo_clear_llf_cpu_policy(void)
{
	struct rb_node *n;
	struct render_info *iter;

	fpsgo_render_tree_lock(__func__);

	for (n = rb_first(&render_pid_tree); n; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);

		fpsgo_thread_lock(&iter->thr_mlock);
		fpsgo_base2fbt_clear_llf_policy(iter);
		fpsgo_thread_unlock(&iter->thr_mlock);
	}

	fpsgo_render_tree_unlock(__func__);
}

#if FPSGO_MW
void fpsgo_clear_llf_cpu_policy_by_pid(int tgid)
{
	struct rb_node *n;
	struct render_info *iter;

	fpsgo_lockprove(__func__);
	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);

		if (iter->tgid == tgid) {
			fpsgo_thread_lock(&iter->thr_mlock);
			fpsgo_base2fbt_clear_llf_policy(iter);
			fpsgo_thread_unlock(&iter->thr_mlock);
		}
	}

}
#endif  // FPSGO_MW

static void fpsgo_clear_uclamp_boost_locked(void)
{
	struct rb_node *n;
	struct render_info *iter;

	fpsgo_lockprove(__func__);

	for (n = rb_first(&render_pid_tree); n; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);

		fpsgo_thread_lock(&iter->thr_mlock);
		fpsgo_base2fbt_set_min_cap(iter, 0, 0, 0);
		fpsgo_thread_unlock(&iter->thr_mlock);
	}
}

void fpsgo_clear_uclamp_boost(void)
{
	fpsgo_render_tree_lock(__func__);

	fpsgo_clear_uclamp_boost_locked();

	fpsgo_render_tree_unlock(__func__);
}

int fpsgo_check_all_tree_empty(void)
{
	int render_info_empty_flag = 0;
	int BQ_id_empty_flag = 0;
	int linger_empty_flag = 0;
	int hwui_empty_flag = 0;
	int api_empty_flag = 0;
	int ret = 0;

	fpsgo_render_tree_lock(__func__);

	render_info_empty_flag = RB_EMPTY_ROOT(&render_pid_tree);
	BQ_id_empty_flag = RB_EMPTY_ROOT(&BQ_id_list);
	linger_empty_flag = RB_EMPTY_ROOT(&linger_tree);
	hwui_empty_flag = RB_EMPTY_ROOT(&hwui_info_tree);
	api_empty_flag = fpsgo_base2comp_check_connect_api_tree_empty();

	if (render_info_empty_flag && BQ_id_empty_flag &&
		linger_empty_flag && hwui_empty_flag && api_empty_flag)
		ret = 1;

	fpsgo_render_tree_unlock(__func__);

	FPSGO_LOGI("[render,BQ,linger,hwui,api]=[%d,%d,%d,%d,%d]\n",
		render_info_empty_flag, BQ_id_empty_flag, linger_empty_flag,
		hwui_empty_flag, api_empty_flag);

	return ret;
}

int fpsgo_check_thread_status(void)
{
	unsigned long long ts = fpsgo_get_time();
	unsigned long long expire_ts;
	int delete = 0;
	int check_max_blc = 0;
	struct rb_node *n;
	struct render_info *iter;
	int temp_max_pid = 0;
	unsigned long long temp_max_bufid = 0;
	int rb_tree_empty = 0;
	int is_boosting = BY_PASS_TYPE;
	int local_ux_max_perf = 0;
	int local_ux_max_pid = 0;

	if (ts < TIME_1S)
		return 0;

	expire_ts = ts - TIME_1S;

	fpsgo_render_tree_lock(__func__);
	fpsgo_base2fbt_get_max_blc_pid(&temp_max_pid, &temp_max_bufid);

	n = rb_first(&render_pid_tree);
	while (n) {
		iter = rb_entry(n, struct render_info, render_key_node);

		fpsgo_thread_lock(&iter->thr_mlock);

		if (iter->t_enqueue_start < expire_ts &&
			!test_bit(ADPF_TYPE, &iter->master_type)) {
			if (iter->pid == temp_max_pid &&
				iter->buffer_id == temp_max_bufid)
				check_max_blc = 1;

			rb_erase(&iter->render_key_node, &render_pid_tree);
			total_render_info_num--;
			list_del(&(iter->bufferid_list));
			fpsgo_base2fbt_item_del(iter->p_blc, iter->dep_arr, iter);
			iter->p_blc = NULL;
			iter->dep_arr = NULL;
			n = rb_first(&render_pid_tree);
			fpsgo_ux_reset(iter);

			if (fpsgo_base_is_finished(iter))
				delete = 1;
			else {
				delete = 0;
				iter->linger = 1;
				fpsgo_add_linger(iter);
			}

			fpsgo_fstb2other_info_update(iter->pid,
				iter->buffer_id, FPSGO_DELETE, 0, 0, 0, 0);

			fpsgo_thread_unlock(&iter->thr_mlock);

			fpsgo_delete_hwui_info(iter->pid);
			fpsgo_delete_sbe_info(iter->pid);
			switch_thread_max_fps(iter->pid, 0);

			if (delete == 1) {
				fpsgo_fbt_delete_rl_render(iter->pid, iter->buffer_id);
				vfree(iter);
			}

		} else {
			if (iter->frame_type != BY_PASS_TYPE)
				is_boosting = NON_VSYNC_ALIGNED_TYPE;

			if (test_bit(ADPF_TYPE, &iter->master_type) &&
				iter->t_enqueue_end < expire_ts)
				fpsgo_stop_boost_by_render(iter);

			if (iter->frame_type == FRAME_HINT_TYPE &&
				iter->ux_blc_next > local_ux_max_perf) {
				local_ux_max_pid = iter->pid;
				local_ux_max_perf = iter->ux_blc_next;
			}

			n = rb_next(n);

			fpsgo_thread_unlock(&iter->thr_mlock);
		}
	}

	fpsgo_check_BQid_status();
	fpsgo_check_acquire_info_status();
	fpsgo_check_adpf_render_status();
	fpsgo_check_sbe_spid_loading_status();

	fbt_ux_set_perf(local_ux_max_pid, local_ux_max_perf);

	fpsgo_render_tree_unlock(__func__);

	fpsgo_base2comp_check_connect_api();

	if (check_max_blc)
		fpsgo_base2fbt_check_max_blc();

	fpsgo_render_tree_lock(__func__);
	rb_tree_empty = RB_EMPTY_ROOT(&render_pid_tree);
	fpsgo_render_tree_unlock(__func__);

	if (rb_tree_empty)
		fpsgo_base2fbt_no_one_render();

	fpsgo_check_all_tree_empty();

	if (is_boosting == BY_PASS_TYPE)
		fpsgo_com_notify_fpsgo_is_boost(0);

	return rb_tree_empty;
}

void fpsgo_clear(void)
{
	int delete = 0;
	struct rb_node *n;
	struct render_info *iter;

	fpsgo_render_tree_lock(__func__);

	n = rb_first(&render_pid_tree);
	while (n) {
		iter = rb_entry(n, struct render_info, render_key_node);
		fpsgo_thread_lock(&iter->thr_mlock);

		rb_erase(&iter->render_key_node, &render_pid_tree);
		total_render_info_num--;
		list_del(&(iter->bufferid_list));
		fpsgo_base2fbt_item_del(iter->p_blc, iter->dep_arr, iter);
		iter->p_blc = NULL;
		iter->dep_arr = NULL;
		n = rb_first(&render_pid_tree);

		if (fpsgo_base_is_finished(iter))
			delete = 1;
		else {
			delete = 0;
			iter->linger = 1;
			fpsgo_add_linger(iter);
		}

		fpsgo_fstb2other_info_update(iter->pid,
			iter->buffer_id, FPSGO_DELETE, 0, 0, 0, 0);

		fpsgo_thread_unlock(&iter->thr_mlock);

		fpsgo_delete_hwui_info(iter->pid);
		fpsgo_delete_sbe_info(iter->pid);
		switch_thread_max_fps(iter->pid, 0);

		if (delete == 1) {
			fpsgo_fbt_delete_rl_render(iter->pid, iter->buffer_id);
			vfree(iter);
		}
	}

#if FPSGO_MW
	delete_all_pid_attr_items_in_tree();
	delete_all_tid_attr_items_in_tree();
#endif  // FPSGO_MW

	fpsgo_render_tree_unlock(__func__);
}

int fpsgo_update_swap_buffer(int pid)
{
	fpsgo_render_tree_lock(__func__);
	fpsgo_search_and_add_hwui_info(pid, 1);
	fpsgo_render_tree_unlock(__func__);
	return 0;
}

int fpsgo_sbe_rescue_traverse(int pid, int start, int enhance, unsigned long long frame_id)
{
	struct rb_node *n;
	struct render_info *iter;

	fpsgo_render_tree_lock(__func__);
	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);
		fpsgo_thread_lock(&iter->thr_mlock);
		if (iter->pid == pid) {
			if (iter->buffer_id == 5566)
				fpsgo_sbe_rescue(iter, start, enhance, frame_id);
			else
				fpsgo_sbe_rescue_legacy(iter, start, enhance, frame_id);
		}
		fpsgo_thread_unlock(&iter->thr_mlock);
	}
	fpsgo_render_tree_unlock(__func__);
	return 0;
}

void fpsgo_stop_boost_by_render(struct render_info *thr)
{
	fpsgo_lockprove(__func__);
	fpsgo_thread_lockprove(__func__, &(thr->thr_mlock));
	fpsgo_base2fbt_stop_boost(thr);
}

void fpsgo_stop_boost_by_pid(int pid)
{
	struct rb_node *n;
	struct render_info *iter;

	if (pid <= 1)
		return;

	fpsgo_lockprove(__func__);

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);
		fpsgo_thread_lock(&iter->thr_mlock);
		if (iter->pid == pid)
			fpsgo_base2fbt_stop_boost(iter);
		fpsgo_thread_unlock(&iter->thr_mlock);
	}
}

static void fpsgo_delete_oldest_BQ_id(void)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct BQ_id *iter = NULL, *min_iter = NULL;
	struct rb_node *rbn = NULL;

	for (rbn = rb_first(&BQ_id_list); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct BQ_id, entry);
		if (iter->ts < min_ts) {
			min_ts = iter->ts;
			min_iter = iter;
		}
	}

	if (min_iter) {
		rb_erase(&min_iter->entry, &BQ_id_list);
		kfree(min_iter);
		total_BQ_id_num--;
	}
}

static struct BQ_id *fpsgo_get_BQid_by_key(struct fbt_render_key key,
		int add, int pid, long long identifier)
{
	unsigned long long cur_ts = fpsgo_get_time();
	struct rb_node **p = &BQ_id_list.rb_node;
	struct rb_node *parent = NULL;
	struct BQ_id *pos;

	fpsgo_lockprove(__func__);

	while (*p) {
		parent = *p;
		pos = rb_entry(parent, struct BQ_id, entry);

		if (render_key_compare(key, pos->key) < 0)
			p = &(*p)->rb_left;
		else if (render_key_compare(key, pos->key) > 0)
			p = &(*p)->rb_right;
		else {
			pos->ts = cur_ts;
			return pos;
		}
	}

	if (!add)
		return NULL;

	pos = kzalloc(sizeof(*pos), GFP_KERNEL);
	if (!pos)
		return NULL;

	pos->key.key1 = key.key1;
	pos->key.key2 = key.key2;
	pos->pid = pid;
	pos->identifier = identifier;
	pos->ts = cur_ts;
	rb_link_node(&pos->entry, parent, p);
	rb_insert_color(&pos->entry, &BQ_id_list);
	total_BQ_id_num++;

	FPSGO_LOGI("add BQid key1 %d, key2 %llu, pid %d, id 0x%llx\n",
		   key.key1, key.key2, pid, identifier);

	if (total_BQ_id_num > FPSGO_MAX_BQ_ID_SIZE)
		fpsgo_delete_oldest_BQ_id();

	return pos;
}

struct BQ_id *fpsgo_find_BQ_id(int pid, int tgid,
		long long identifier, int action)
{
	struct rb_node *n;
	struct rb_node *next;
	struct BQ_id *pos;
	struct fbt_render_key key;
	int tgid_key = tgid;
	unsigned long long identifier_key = identifier;
	int done = 0;

	if (!tgid_key) {
		tgid_key = fpsgo_get_tgid(pid);
		if (!tgid_key)
			return NULL;
	}

	key.key1 = tgid_key;
	key.key2 = identifier_key;

	fpsgo_lockprove(__func__);

	switch (action) {
	case ACTION_FIND:
	case ACTION_FIND_ADD:
		FPSGO_LOGI("find %s pid %d, id %llu, key1 %d, key2 %llu\n",
			(action == ACTION_FIND_ADD)?"add":"",
			pid, identifier, key.key1, key.key2);

		return fpsgo_get_BQid_by_key(key, action == ACTION_FIND_ADD,
					     pid, identifier);

	case ACTION_FIND_DEL:
		for (n = rb_first(&BQ_id_list); n; n = next) {
			next = rb_next(n);

			pos = rb_entry(n, struct BQ_id, entry);
			if (render_key_compare(pos->key, key) == 0) {
				FPSGO_LOGI(
					"find del pid %d, id %llu, key1 %d, key2 %llu\n",
					pid, identifier, key.key1, key.key2);
				fpsgo_delete_acquire_info(1, 0, pos->buffer_id);
				rb_erase(n, &BQ_id_list);
				kfree(pos);
				total_BQ_id_num--;
				done = 1;
				break;
			}
		}
		if (!done)
			FPSGO_LOGE("del fail key1 %d, key2 %llu\n", key.key1, key.key2);
		return NULL;
	default:
		FPSGO_LOGE("[ERROR] unknown action %d\n", action);
		return NULL;
	}
}

int fpsgo_get_BQid_pair(int pid, int tgid, long long identifier,
		unsigned long long *buffer_id, int *queue_SF, int enqueue)
{
	struct BQ_id *pair;

	fpsgo_lockprove(__func__);

	pair = fpsgo_find_BQ_id(pid, tgid, identifier, ACTION_FIND);

	if (pair) {
		*buffer_id = pair->buffer_id;
		*queue_SF = pair->queue_SF;
		if (enqueue)
			pair->queue_pid = pid;
		return 1;
	}

	return 0;
}

int fpsgo_get_acquire_queue_pair_by_self(int tid, unsigned long long buffer_id)
{
	int ret = ACQUIRE_OTHER_TYPE;
	struct acquire_info *iter = NULL;

	iter = fpsgo_search_acquire_info(tid, buffer_id);
	if (iter)
		ret = ACQUIRE_SELF_TYPE;

	return ret;
}

int fpsgo_get_acquire_queue_pair_by_group(int tid, int *dep_arr,
	int dep_arr_num, unsigned long long buffer_id)
{
	int i;
	int ret = ACQUIRE_OTHER_TYPE;
	struct acquire_info *c_iter = NULL;
	struct rb_node *rbn = NULL;

	if (!dep_arr || dep_arr_num <= 0) {
		ret = ACQUIRE_CAMERA_TYPE;
		goto out;
	}

	for (i = 0; i < dep_arr_num; i++) {
		for (rbn = rb_first(&acquire_info_tree); rbn; rbn = rb_next(rbn)) {
			c_iter = rb_entry(rbn, struct acquire_info, entry);
			if (c_iter->c_tid == dep_arr[i] &&
				c_iter->api == NATIVE_WINDOW_API_CAMERA) {
				ret = ACQUIRE_CAMERA_TYPE;
				break;
			}
		}
		if (ret == ACQUIRE_CAMERA_TYPE)
			break;
	}

out:
	return ret;
}

int fpsgo_check_cam_do_frame(void)
{
	int ret = 1;
	int local_cam_rtid = 0;
	int local_qfps_arr_num = 0;
	int local_tfps_arr_num = 0;
	int *local_qfps_arr = NULL;
	int *local_tfps_arr = NULL;
	struct render_info *r_iter = NULL;
	struct rb_node *rbn = NULL;

	for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
		r_iter = rb_entry(rbn, struct render_info, render_key_node);
		if (r_iter->api == NATIVE_WINDOW_API_CAMERA) {
			local_cam_rtid = r_iter->pid;
			break;
		}
	}

	if (!local_cam_rtid) {
		ret = 0;
		fpsgo_main_trace("[base] %s no cam_rtid", __func__);
		goto out;
	}

	local_qfps_arr = kcalloc(1, sizeof(int), GFP_KERNEL);
	if (!local_qfps_arr)
		goto out;

	local_tfps_arr = kcalloc(1, sizeof(int), GFP_KERNEL);
	if (!local_tfps_arr)
		goto out;

	fpsgo_other2fstb_get_fps(local_cam_rtid, 0,
		local_qfps_arr, &local_qfps_arr_num, 1,
		local_tfps_arr, &local_tfps_arr_num, 1);

	if (!local_qfps_arr[0]) {
		ret = 0;
		fpsgo_main_trace("[base] %s cam_rtid:%d queue_fps:%d",
			__func__, local_cam_rtid, local_qfps_arr[0]);
	}

out:
	kfree(local_qfps_arr);
	kfree(local_tfps_arr);
	return ret;
}

int fpsgo_check_all_render_blc(int render_tid, unsigned long long buffer_id)
{
	int ret = 1;
	int tgid = 0;
	int count = 0;
	struct render_info *r_iter = NULL;
	struct rb_node *rbn = NULL;

	tgid = fpsgo_get_tgid(render_tid);
	if (tgid < 0)
		return ret;

	for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
		r_iter = rb_entry(rbn, struct render_info, render_key_node);

		if (r_iter->tgid != tgid)
			continue;

		if (r_iter->pid == render_tid &&
			r_iter->buffer_id == buffer_id)
			continue;

		if (r_iter->frame_type == FRAME_HINT_TYPE ||
			r_iter->hwui == RENDER_INFO_HWUI_TYPE ||
			fpsgo_search_and_add_sbe_info(r_iter->pid, 0))
			continue;

		if (r_iter->bq_type == ACQUIRE_OTHER_TYPE) {
			count++;
			xgf_trace("[base][%d][0x%llx] | r:%d 0x%llx bq_type:%d",
				render_tid, buffer_id,
				r_iter->pid, r_iter->buffer_id, r_iter->bq_type);
		}

		if (r_iter->boost_info.last_blc > 0) {
			ret = 0;
			xgf_trace("[base][%d][0x%llx] | r:%d 0x%llx last_blc:%d",
				render_tid, buffer_id,
				r_iter->pid, r_iter->buffer_id, r_iter->boost_info.last_blc);
		}
	}

	if (ret && count)
		ret = 0;

	ret = ret ? ACQUIRE_CAMERA_TYPE : ACQUIRE_OTHER_TYPE;

	return ret;
}

int fpsgo_check_exist_queue_SF(int tgid)
{
	int ret = BY_PASS_TYPE;
	struct render_info *r_iter = NULL;
	struct rb_node *rbn = NULL;

	for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
		r_iter = rb_entry(rbn, struct render_info, render_key_node);

		if (r_iter->tgid != tgid)
			continue;

		if (r_iter->queue_SF) {
			ret = NON_VSYNC_ALIGNED_TYPE;
			break;
		}
	}

	return ret;
}

struct acquire_info *fpsgo_search_acquire_info(int tid, unsigned long long buffer_id)
{
	struct acquire_info *iter = NULL;
	struct rb_node *rbn = NULL;

	for (rbn = rb_first(&acquire_info_tree); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct acquire_info, entry);
		if (iter->c_tid == tid && iter->buffer_id == buffer_id)
			return iter;
	}

	return NULL;
}

struct acquire_info *fpsgo_add_acquire_info(int p_pid, int c_pid, int c_tid,
	int api, unsigned long long buffer_id, unsigned long long ts)
{
	int local_tgid;
	struct rb_node **p = &acquire_info_tree.rb_node;
	struct rb_node *parent = NULL;
	struct acquire_info *iter = NULL;
	struct fbt_render_key local_key = {.key1 = c_tid, .key2 = buffer_id};

	if (api == NATIVE_WINDOW_API_CAMERA) {
		if (wq_has_sleeper(&cam_apk_pid_queue)) {
			fpsgo_get_cam_pid(CAMERA_APK, &local_tgid);
			fpsgo2cam_sentcmd(CAMERA_APK, local_tgid);
		}
		if (wq_has_sleeper(&cam_ser_pid_queue)) {
			fpsgo_get_cam_pid(CAMERA_SERVER, &local_tgid);
			fpsgo2cam_sentcmd(CAMERA_SERVER, local_tgid);
		}
	}

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct acquire_info, entry);

		if (render_key_compare(local_key, iter->key) < 0)
			p = &(*p)->rb_left;
		else if (render_key_compare(local_key, iter->key) > 0)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	iter = kzalloc(sizeof(struct acquire_info), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->p_pid = p_pid;
	iter->c_pid = c_pid;
	iter->c_tid = c_tid;
	iter->api = api;
	iter->buffer_id = buffer_id;
	iter->key.key1 = c_tid;
	iter->key.key2 = buffer_id;

	rb_link_node(&iter->entry, parent, p);
	rb_insert_color(&iter->entry, &acquire_info_tree);

	return iter;
}

int fpsgo_delete_acquire_info(int mode, int tid, unsigned long long buffer_id)
{
	int ret = 0;
	struct acquire_info *iter = NULL;
	struct rb_node *rbn = NULL;

	if (mode == 0) {
		iter = fpsgo_search_acquire_info(tid, buffer_id);
		if (iter) {
			rb_erase(&iter->entry, &acquire_info_tree);
			kfree(iter);
			ret = 1;
		}
	} else if (mode == 1) {
		rbn = rb_first(&acquire_info_tree);
		while (rbn) {
			iter = rb_entry(rbn, struct acquire_info, entry);
			if (iter->buffer_id == buffer_id) {
				rb_erase(&iter->entry, &acquire_info_tree);
				kfree(iter);
				ret = 1;
				rbn = rb_first(&acquire_info_tree);
			} else
				rbn = rb_next(rbn);
		}
	} else if (mode == 2) {
		rbn = rb_first(&acquire_info_tree);
		while (rbn) {
			iter = rb_entry(rbn, struct acquire_info, entry);
			rb_erase(&iter->entry, &acquire_info_tree);
			kfree(iter);
			ret = 1;
			rbn = rb_first(&acquire_info_tree);
		}
	}

	return ret;
}

int fpsgo_check_is_cam_apk(int tgid)
{
	int ret = 0;

	if (global_cam_apk_pid > 0 && global_cam_apk_pid == tgid)
		ret = 1;

	return ret;
}

int fpsgo_get_render_tid_by_render_name(int tgid, char *name,
	int *out_tid_arr, unsigned long long *out_bufID_arr,
	int *out_tid_num, int out_tid_max_num)
{
	int i;
	int find_flag = 0;
	int local_index = 0;
	struct render_info *render_iter = NULL;
	struct rb_node *rbn = NULL;
	struct task_struct *tsk = NULL;

	if (tgid <= 0 || !name || !out_tid_arr || !out_bufID_arr ||
		!out_tid_num || out_tid_max_num <= 0)
		return -EINVAL;

	fpsgo_render_tree_lock(__func__);
	for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
		render_iter = rb_entry(rbn, struct render_info, render_key_node);
		if (render_iter->tgid != tgid)
			continue;

		rcu_read_lock();
		tsk = find_task_by_vpid(render_iter->pid);
		if (tsk) {
			get_task_struct(tsk);
			if (!strncmp(tsk->comm, name, 16))
				find_flag = 1;
			put_task_struct(tsk);
		}
		rcu_read_unlock();

		if (!find_flag)
			continue;

		for (i = 0; i < local_index; i++) {
			if (out_tid_arr[i] == render_iter->pid &&
				out_bufID_arr[i] == render_iter->buffer_id)
				break;
		}

		if (i == local_index && local_index < out_tid_max_num) {
			out_tid_arr[local_index] = render_iter->pid;
			out_bufID_arr[local_index] = render_iter->buffer_id;
			local_index++;
		}
		find_flag = 0;
	}
	fpsgo_render_tree_unlock(__func__);

	*out_tid_num = local_index;

	return 0;
}

static void fpsgo_delete_oldest_sbe_spid_loading(void)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct sbe_spid_loading *iter = NULL, *min_iter = NULL;
	struct rb_node *rbn = NULL;

	for (rbn = rb_first(&sbe_spid_loading_tree); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct sbe_spid_loading, rb_node);
		if (iter->ts < min_ts) {
			min_ts = iter->ts;
			min_iter = iter;
		}
	}

	if (min_iter) {
		rb_erase(&min_iter->rb_node, &sbe_spid_loading_tree);
		kfree(min_iter);
		total_sbe_spid_loading_num--;
	}
}

struct sbe_spid_loading *fpsgo_get_sbe_spid_loading(int tgid, int create)
{
	struct rb_node **p = &sbe_spid_loading_tree.rb_node;
	struct rb_node *parent = NULL;
	struct sbe_spid_loading *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct sbe_spid_loading, rb_node);

		if (tgid < iter->tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter->tgid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!create)
		return NULL;

	iter = kzalloc(sizeof(struct sbe_spid_loading), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->tgid = tgid;
	iter->spid_num = 0;
	iter->ts = fpsgo_get_time();

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &sbe_spid_loading_tree);
	total_sbe_spid_loading_num++;

	if (total_sbe_spid_loading_num > FPSGO_MAX_SBE_SPID_LOADING_SIZE)
		fpsgo_delete_oldest_sbe_spid_loading();

	return iter;
}

int fpsgo_delete_sbe_spid_loading(int tgid)
{
	int ret = 0;
	struct sbe_spid_loading *iter = NULL;

	iter = fpsgo_get_sbe_spid_loading(tgid, 0);
	if (iter) {
		rb_erase(&iter->rb_node, &sbe_spid_loading_tree);
		kfree(iter);
		total_sbe_spid_loading_num--;
		ret = 1;
	}

	return ret;
}

int fpsgo_update_sbe_spid_loading(int *cur_pid_arr, int cur_pid_num, int tgid)
{
	int ret = 0;
	int i;
	unsigned long long local_runtime;
	struct sbe_spid_loading *iter = NULL;
	struct task_struct *tsk = NULL;

	if (!cur_pid_arr || cur_pid_num <= 0) {
		ret = -EINVAL;
		goto out;
	}

	iter = fpsgo_get_sbe_spid_loading(tgid, 1);
	if (!iter) {
		ret = -ENOMEM;
		goto out;
	}
	iter->ts = fpsgo_get_time();

	memset(iter->spid_arr, 0, MAX_DEP_NUM * sizeof(int));
	memset(iter->spid_latest_runtime, 0,
		MAX_DEP_NUM * sizeof(unsigned long long));

	for (i = 0; i < cur_pid_num; i++) {
		local_runtime = 0;
		rcu_read_lock();
		tsk = find_task_by_vpid(cur_pid_arr[i]);
		if (tsk) {
			get_task_struct(tsk);
			local_runtime = (u64)fpsgo_task_sched_runtime(tsk);
			put_task_struct(tsk);
		}
		rcu_read_unlock();

		iter->spid_arr[i] = cur_pid_arr[i];
		iter->spid_latest_runtime[i] = local_runtime;
		fpsgo_main_trace("[base] sbe %dth tgid:%d update spid:%d runtime:%llu",
			i+1, tgid, cur_pid_arr[i], local_runtime);

		if (i == MAX_DEP_NUM - 1)
			break;
	}
	iter->spid_num = cur_pid_num <= MAX_DEP_NUM ? cur_pid_num : MAX_DEP_NUM;

out:
	return ret;
}

int fpsgo_ctrl2base_query_sbe_spid_loading(void)
{
	int ret = 0;
	int i;
	unsigned long long local_runtime;
	struct sbe_spid_loading *iter = NULL;
	struct rb_node *rbn = NULL;
	struct task_struct *tsk = NULL;

	fpsgo_render_tree_lock(__func__);
	for (rbn = rb_first(&sbe_spid_loading_tree); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct sbe_spid_loading, rb_node);
		if (!fpsgo_get_tgid(iter->tgid))
			continue;

		for (i = 0; i < iter->spid_num; i++) {
			local_runtime = 0;
			rcu_read_lock();
			tsk = find_task_by_vpid(iter->spid_arr[i]);
			if (tsk) {
				get_task_struct(tsk);
				local_runtime = (u64)fpsgo_task_sched_runtime(tsk);
				put_task_struct(tsk);
			}
			rcu_read_unlock();

			fpsgo_main_trace("[base] sbe %dth tgid:%d query spid:%d runtime:%llu->%llu",
				i+1, iter->tgid, iter->spid_arr[i], iter->spid_latest_runtime[i], local_runtime);

			if (local_runtime > 0 &&
				local_runtime > iter->spid_latest_runtime[i]) {
				ret = 1;
				break;
			}
		}

		if (ret)
			break;
	}
	fpsgo_render_tree_unlock(__func__);

	return ret;
}

static unsigned long long fpsgo_traverse_render_rb_tree(struct rb_root *rbr,
	unsigned long long target_addr)
{
	int i;
	unsigned long long ret = 0;
	unsigned long long local_addr = 0;
	struct render_info *r_iter = NULL;
	struct fbt_proc *proc_iter = NULL;
	struct work_struct *work_iter = NULL;
	struct rb_node *rbn = NULL;

	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		if (rbr == &render_pid_tree)
			r_iter = rb_entry(rbn, struct render_info, render_key_node);
		else if (rbr == &linger_tree)
			r_iter = rb_entry(rbn, struct render_info, linger_node);
		else {
			FPSGO_LOGE("[base] %s rbr wrong\n", __func__);
			break;
		}

		fpsgo_thread_lock(&(r_iter->thr_mlock));
		proc_iter = &r_iter->boost_info.proc;
		for (i = 0; i < RESCUE_TIMER_NUM; i++) {
			work_iter = &(proc_iter->jerks[i].work);
			local_addr = (unsigned long long)work_iter;
			if (target_addr && local_addr == target_addr) {
				ret = local_addr;
				break;
			} else if (!target_addr)
				FPSGO_LOGE("[base] rtid:%d bufID:0x%llx work-%d:0x%llx\n",
					r_iter->pid, r_iter->buffer_id, i, local_addr);
		}
		fpsgo_thread_unlock(&(r_iter->thr_mlock));

		if (ret)
			break;
	}

	return ret;
}

int fpsgo_check_fbt_jerk_work_addr_invalid(struct work_struct *target_work)
{
	int ret = -EFAULT;
	unsigned long long local_addr = 0;
	unsigned long long target_addr = 0;

	target_addr = (unsigned long long)target_work;

	local_addr = fpsgo_traverse_render_rb_tree(&render_pid_tree, target_addr);
	if (local_addr) {
		ret = 0;
		goto out;
	}

	local_addr = fpsgo_traverse_render_rb_tree(&linger_tree, target_addr);
	if (local_addr) {
		ret = 0;
		goto out;
	}

	if (ret) {
		fpsgo_traverse_render_rb_tree(&render_pid_tree, 0);
		fpsgo_traverse_render_rb_tree(&linger_tree, 0);
	}

out:
	return ret;
}

static ssize_t fpsgo_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", fpsgo_is_enable());
}

static ssize_t fpsgo_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	if (val > 1 || val < 0)
		goto out;

	fpsgo_switch_enable(val);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RWO(fpsgo_enable);

static ssize_t render_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct render_info *iter;
	struct hwui_info *h_info;
	struct task_struct *tsk;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"\n  PID  NAME  TGID  TYPE  API  BufferID");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"    FRAME_L    ENQ_L    ENQ_S    ENQ_E");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"    DEQ_L     DEQ_S    DEQ_E    HWUI\n");
	pos += length;

	fpsgo_render_tree_lock(__func__);
	rcu_read_lock();

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);
		tsk = find_task_by_vpid(iter->tgid);
		if (tsk) {
			get_task_struct(tsk);

			length = scnprintf(temp + pos,
					FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
					"%5d %4s %4d %4d %4d 0x%llx",
				iter->pid, tsk->comm,
				iter->tgid, iter->frame_type,
				iter->api, iter->buffer_id);
			pos += length;
			length = scnprintf(temp + pos,
					FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
					"  %4llu %4llu %4llu %4llu %4llu %4llu %d\n",
				iter->enqueue_length,
				iter->t_enqueue_start, iter->t_enqueue_end,
				iter->dequeue_length, iter->t_dequeue_start,
				iter->t_dequeue_end, iter->hwui);
			pos += length;
			put_task_struct(tsk);
		}
	}

	rcu_read_unlock();

	for (n = rb_first(&linger_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, linger_node);
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"(%5d %4llu) linger %d timer %d\n",
			iter->pid, iter->buffer_id, iter->linger,
			fpsgo_base2fbt_is_finished(iter));
		pos += length;
	}

	/* hwui tree */
	for (n = rb_first(&hwui_info_tree); n != NULL; n = rb_next(n)) {
		h_info = rb_entry(n, struct hwui_info, entry);
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"HWUI: %d\n", h_info->pid);
		pos += length;
	}

	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(render_info);

#if FPSGO_MW
static ssize_t render_info_params_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct render_info *iter;
	struct task_struct *tsk;
	struct fpsgo_boost_attr attr_item;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\nNEW PID: PID, NAME, TGID\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" llf_task_policy, loading_th, light_loading_policy,\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" rescue_second_enable, rescue_second_time, rescue_second_group\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" group_by_lr, heavy_group_num, second_group_num\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" ff_enable, ff_window_size, ff_k_min\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" boost_affinity, cpumask_heavy, cpumask_second, cpumask_others, boost_LR\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" separate_aa, separate_release_sec_by_pid, pct_b, pct_m, pct_other, blc_boost\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" uclamp, ruclamp, uclamp_m, ruclamp_m\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			" limit_cfreq2cap, limit_rfreq2cap, limit_cfreq2cap_m, limit_rfreq2cap_m\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" rescue_enable, qr_enable, qr_t2wnt_x, qr_t2wnt_y_p, qr_t2wnt_y_n\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" gcc_enable, gcc_fps_margin, gcc_up_sec_pct, gcc_up_step, gcc_reserved_up_quota_pct\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" gcc_down_sec_pct, gcc_down_step, gcc_reserved_down_quota_pct\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" gcc_enq_bound_thrs, gcc_enq_bound_quota, gcc_deq_bound_thrs, gcc_deq_bound_quota\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" check_buffer_quota, expected_fps_margin, quota_v2_diff_clamp_min, quota_v2_diff_clamp_max, limit_min_cap\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" boost_VIP, RT_prio1, RT_prio2, RT_prio3, vip_mask, set_ls, ls_groupmask, set_vvip\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" aa_b_minus_idle_time\n");
	pos += length;

	fpsgo_render_tree_lock(__func__);
	rcu_read_lock();

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);
		if (iter->frame_type == BY_PASS_TYPE)
			continue;

		attr_item = iter->attr;
		tsk = find_task_by_vpid(iter->tgid);
		if (tsk) {
			get_task_struct(tsk);

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"\nNEW PID: %5d, %4s, %4d\n",
				iter->pid, tsk->comm,
				iter->tgid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" %4d, %4d, %4d\n",
				attr_item.llf_task_policy_by_pid, attr_item.loading_th_by_pid,
				attr_item.light_loading_policy_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d\n",
				attr_item.rescue_second_enable_by_pid,
				attr_item.rescue_second_time_by_pid,
				attr_item.rescue_second_group_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d\n",
				attr_item.group_by_lr_by_pid,
				attr_item.heavy_group_num_by_pid,
				attr_item.second_group_num_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d\n",
				attr_item.filter_frame_enable_by_pid,
				attr_item.filter_frame_window_size_by_pid,
				attr_item.filter_frame_kmin_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d, %4d\n",
				attr_item.boost_affinity_by_pid,
				attr_item.cpumask_heavy_by_pid,
				attr_item.cpumask_second_by_pid,
				attr_item.cpumask_others_by_pid,
				attr_item.boost_lr_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d, %4d, %4d\n",
				attr_item.separate_aa_by_pid,
				attr_item.separate_release_sec_by_pid,
				attr_item.separate_pct_b_by_pid,
				attr_item.separate_pct_m_by_pid,
				attr_item.separate_pct_other_by_pid,
				attr_item.blc_boost_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d\n",
				attr_item.limit_uclamp_by_pid,
				attr_item.limit_ruclamp_by_pid,
				attr_item.limit_uclamp_m_by_pid,
				attr_item.limit_ruclamp_m_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d\n",
				attr_item.limit_cfreq2cap_by_pid,
				attr_item.limit_rfreq2cap_by_pid,
				attr_item.limit_cfreq2cap_m_by_pid,
				attr_item.limit_rfreq2cap_m_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d, %4d\n",
				attr_item.rescue_enable_by_pid,
				attr_item.qr_enable_by_pid,
				attr_item.qr_t2wnt_x_by_pid,
				attr_item.qr_t2wnt_y_p_by_pid,
				attr_item.qr_t2wnt_y_n_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d, %4d\n",
				attr_item.gcc_enable_by_pid,
				attr_item.gcc_fps_margin_by_pid,
				attr_item.gcc_up_sec_pct_by_pid,
				attr_item.gcc_up_step_by_pid,
				attr_item.gcc_reserved_up_quota_pct_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d\n",
				attr_item.gcc_down_sec_pct_by_pid,
				attr_item.gcc_down_step_by_pid,
				attr_item.gcc_reserved_down_quota_pct_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d\n",
				attr_item.gcc_enq_bound_thrs_by_pid,
				attr_item.gcc_enq_bound_quota_by_pid,
				attr_item.gcc_deq_bound_thrs_by_pid,
				attr_item.gcc_deq_bound_quota_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d %4d %4d, %4d,\n",
				attr_item.check_buffer_quota_by_pid,
				attr_item.expected_fps_margin_by_pid,
				attr_item.quota_v2_diff_clamp_min_by_pid,
				attr_item.quota_v2_diff_clamp_max_by_pid,
				attr_item.limit_min_cap_target_t_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d, %4d, %4d, %4d, %4d\n",
				attr_item.boost_vip_by_pid,
				attr_item.rt_prio1_by_pid,
				attr_item.rt_prio2_by_pid,
				attr_item.rt_prio3_by_pid,
				attr_item.vip_mask_by_pid,
				attr_item.set_ls_by_pid,
				attr_item.ls_groupmask_by_pid,
				attr_item.set_vvip_by_pid);
			pos += length;

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d\n",
				attr_item.aa_b_minus_idle_t_by_pid);
			pos += length;

			put_task_struct(tsk);
		}
	}

	rcu_read_unlock();
	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}
static KOBJ_ATTR_RO(render_info_params);

static ssize_t render_attr_params_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct fpsgo_attr_by_pid *iter;
	struct fpsgo_boost_attr attr_item;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\n NEW tgid: TGID, llf_task_policy, loading_th, light_loading_policy,\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" rescue_second_enable, rescue_second_time, rescue_second_group,\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" ff_enable, ff_window_size, ff_k_min,\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" boost_affinity, boost_LR,\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" separate_aa, separate_release_sec_by_pid, pct_b, pct_m, blc_boost\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		" uclamp, ruclamp, uclamp_m, ruclamp_m\n");
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" qr_enable, qr_enable_tid, qr_t2wnt_x, qr_t2wnt_y_p, qr_t2wnt_y_n\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" gcc_enable, gcc_enable_tid, gcc_fps_margin, gcc_up_sec_pct, gcc_up_step gcc_reserved_up_quota_pct\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" gcc_down_sec_pct, gcc_down_step, gcc_reserved_down_quota_pct\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" gcc_enq_bound_thrs, gcc_enq_bound_quota, gcc_deq_bound_thrs, gcc_deq_bound_quota\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" check_buffer_quota, expected_fps_margin, quota_v2_diff_clamp_min, quota_v2_diff_clamp_max, limit_min_cap\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" boost_VIP, RT_prio1, RT_prio2, RT_prio3, vip_mask, set_ls, ls_groupmask, set_vvip\n");
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				" aa_b_minus_idle_time\n");
	pos += length;

	fpsgo_render_tree_lock(__func__);

	for (n = rb_first(&fpsgo_attr_by_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n,  struct fpsgo_attr_by_pid, entry);
		attr_item = iter->attr;

		length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"\n NEW tgid: %4d, %4d, %4d, %4d,\n",
			iter->tgid, attr_item.llf_task_policy_by_pid,
			attr_item.loading_th_by_pid, attr_item.light_loading_policy_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d,\n",
			attr_item.rescue_second_enable_by_pid,
			attr_item.rescue_second_time_by_pid,
			attr_item.rescue_second_group_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d,\n",
			attr_item.filter_frame_enable_by_pid,
			attr_item.filter_frame_window_size_by_pid,
			attr_item.filter_frame_kmin_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d,\n",
			attr_item.boost_affinity_by_pid,
			attr_item.boost_lr_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d, %4d\n",
			attr_item.separate_aa_by_pid,
			attr_item.separate_release_sec_by_pid,
			attr_item.separate_pct_b_by_pid,
			attr_item.separate_pct_m_by_pid,
			attr_item.blc_boost_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d\n",
			attr_item.limit_uclamp_by_pid,
			attr_item.limit_ruclamp_by_pid,
			attr_item.limit_uclamp_m_by_pid,
			attr_item.limit_ruclamp_m_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d\n",
			attr_item.qr_enable_by_pid,
			attr_item.qr_t2wnt_x_by_pid,
			attr_item.qr_t2wnt_y_p_by_pid,
			attr_item.qr_t2wnt_y_n_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d, %4d\n",
			attr_item.gcc_enable_by_pid,
			attr_item.gcc_fps_margin_by_pid,
			attr_item.gcc_up_sec_pct_by_pid,
			attr_item.gcc_up_step_by_pid,
			attr_item.gcc_reserved_up_quota_pct_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d\n",
			attr_item.gcc_down_sec_pct_by_pid,
			attr_item.gcc_down_step_by_pid,
			attr_item.gcc_reserved_down_quota_pct_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d\n",
			attr_item.gcc_enq_bound_thrs_by_pid,
			attr_item.gcc_enq_bound_quota_by_pid,
			attr_item.gcc_deq_bound_thrs_by_pid,
			attr_item.gcc_deq_bound_quota_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d, %4d\n",
			attr_item.check_buffer_quota_by_pid,
			attr_item.expected_fps_margin_by_pid,
			attr_item.quota_v2_diff_clamp_min_by_pid,
			attr_item.quota_v2_diff_clamp_max_by_pid,
			attr_item.limit_min_cap_target_t_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d, %4d, %4d, %4d, %4d, %4d, %4d, %4d\n",
			attr_item.boost_vip_by_pid,
			attr_item.rt_prio1_by_pid,
			attr_item.rt_prio2_by_pid,
			attr_item.rt_prio3_by_pid,
			attr_item.vip_mask_by_pid,
			attr_item.set_ls_by_pid,
			attr_item.ls_groupmask_by_pid,
			attr_item.set_vvip_by_pid);
		pos += length;

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos, " %4d\n",
			attr_item.aa_b_minus_idle_t_by_pid);
		pos += length;
	}

	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}
static KOBJ_ATTR_RO(render_attr_params);

static ssize_t render_attr_params_tid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct fpsgo_attr_by_pid *iter;
	struct fpsgo_boost_attr attr_item;
	char *temp = NULL;
	int pos = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
		"\n NEW tgid: TGID, rescue_enable,\n");
	pos += length;

	fpsgo_render_tree_lock(__func__);

	for (n = rb_first(&fpsgo_attr_by_tid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n,  struct fpsgo_attr_by_pid, entry);
		attr_item = iter->attr;

		length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"\n NEW tid: %4d, %4d,\n",
			iter->tid, attr_item.rescue_enable_by_pid);
		pos += length;

	}
	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}
static KOBJ_ATTR_RO(render_attr_params_tid);

#endif  // FPSGO_MW

static ssize_t force_onoff_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", fpsgo_is_force_enable());
}

static ssize_t force_onoff_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	if (val > 2 || val < 0)
		goto out;

	fpsgo_force_switch_enable(val);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(force_onoff);

static ssize_t BQid_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct BQ_id *pos;
	char *temp = NULL;
	int posi = 0;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	fpsgo_render_tree_lock(__func__);

	length = scnprintf(temp + posi, FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
		"total_BQ_id_num: %d\n", total_BQ_id_num);
	posi += length;

	for (n = rb_first(&BQ_id_list); n; n = rb_next(n)) {
		pos = rb_entry(n, struct BQ_id, entry);
		length = scnprintf(temp + posi,
				FPSGO_SYSFS_MAX_BUFF_SIZE - posi,
				"pid %d, tgid %d, key1 %d, key2 %llu, buffer_id %llu, queue_SF %d\n",
				pos->pid, fpsgo_get_tgid(pos->pid),
				pos->key.key1, pos->key.key2, pos->buffer_id, pos->queue_SF);
		posi += length;

	}

	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(BQid);

static ssize_t acquire_info_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int pos = 0;
	int length = 0;
	struct acquire_info *iter = NULL;
	struct rb_node *rbn = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	fpsgo_render_tree_lock(__func__);

	for (rbn = rb_first(&acquire_info_tree); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct acquire_info, entry);
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"p_pid:%d c_pid:%d c_tid:%d api:%d buffer_id:0x%llx ts:%llu\n",
			iter->p_pid, iter->c_pid, iter->c_tid,
			iter->api, iter->buffer_id, iter->ts);
		pos += length;
	}

	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(acquire_info);

static ssize_t fpsgo_get_acquire_hint_enable_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
		fpsgo_get_acquire_hint_enable);
}

static ssize_t fpsgo_get_acquire_hint_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				fpsgo_get_acquire_hint_enable = !!arg;
			else
				goto out;
		}
	}

	if (!fpsgo_get_acquire_hint_enable)
		global_cam_apk_pid = 0;

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(fpsgo_get_acquire_hint_enable);

static ssize_t perfserv_ta_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
		fpsgo_perfserv_ta_value());
}

static ssize_t perfserv_ta_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = -1;
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				goto out;
		}
	}

	if (val > 101 || val < -1)
		goto out;

	fpsgo_set_perfserv_ta(val);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(perfserv_ta);

static ssize_t render_loading_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct rb_node *n;
	struct render_info *iter;
	char *temp = NULL;
	int pos = 0, i;
	int length = 0;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	fpsgo_render_tree_lock(__func__);

	for (n = rb_first(&render_pid_tree); n != NULL; n = rb_next(n)) {
		iter = rb_entry(n, struct render_info, render_key_node);

		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"PID  TGID  BufferID\n");
		pos += length;

		length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"%d %4d 0x%llx\n", iter->pid, iter->tgid, iter->buffer_id);
		pos += length;

		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"AVG FREQ\n");
		pos += length;

		length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"%d\n", iter->avg_freq);
		pos += length;

		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"DEP LOADING\n");
		pos += length;

		for (i = 0; i < iter->dep_valid_size; i++) {
			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"%d(%d) ", iter->dep_arr[i].pid, iter->dep_arr[i].loading);
			pos += length;
		}

		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"\n");
		pos += length;
	}

	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(render_loading);

static ssize_t render_type_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char *temp = NULL;
	int i = 0;
	int pos = 0;
	int length = 0;
	struct render_info *r_iter = NULL;
	struct rb_node *rbn = NULL;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	fpsgo_render_tree_lock(__func__);

	for (rbn = rb_first(&render_pid_tree); rbn; rbn = rb_next(rbn)) {
		r_iter = rb_entry(rbn, struct render_info, render_key_node);
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"%dth\t[%d][0x%llx]\tframe_type:%d\tbq_type:%d\thwui:%d\tmaster_type:%lu\n",
				i+1, r_iter->pid, r_iter->buffer_id,
				r_iter->frame_type, r_iter->bq_type, r_iter->hwui, r_iter->master_type);
		pos += length;
		i++;
	}

	fpsgo_render_tree_unlock(__func__);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(render_type);

static ssize_t kfps_cpu_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", global_kfps_mask);
}

static ssize_t kfps_cpu_mask_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg = -1;
	int cpu;
	struct cpumask local_mask;
	struct task_struct *tsk = NULL;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) != 0)
				goto out;
		}
	}

	global_kfps_mask = arg;

	cpumask_clear(&local_mask);
	for_each_possible_cpu(cpu) {
		if (global_kfps_mask & (1 << cpu))
			cpumask_set_cpu(cpu, &local_mask);
	}
	FPSGO_LOGE("%s mask:%d %*pbl\n", __func__,
		global_kfps_mask, cpumask_pr_args(&local_mask));

	tsk = fpsgo_get_kfpsgo();
	if (!tsk)
		goto out;

	if (fpsgo_sched_setaffinity(tsk->pid, &local_mask))
		FPSGO_LOGE("%s setaffinity fail\n", __func__);
	else
		FPSGO_LOGE("%s setaffinity success\n", __func__);

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(kfps_cpu_mask);

int init_fpsgo_common(void)
{
	render_pid_tree = RB_ROOT;
	BQ_id_list = RB_ROOT;
	linger_tree = RB_ROOT;
	hwui_info_tree = RB_ROOT;
	sbe_info_tree = RB_ROOT;
	sbe_spid_loading_tree = RB_ROOT;
	fps_control_pid_info_tree = RB_ROOT;
	fpsgo_attr_by_pid_tree = RB_ROOT;
	fpsgo_attr_by_tid_tree = RB_ROOT;
	acquire_info_tree = RB_ROOT;

	if (!fpsgo_sysfs_create_dir(NULL, "common", &base_kobj)) {
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_fpsgo_enable);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_force_onoff);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_render_info);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_BQid);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_acquire_info);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_fpsgo_get_acquire_hint_enable);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_perfserv_ta);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_render_loading);
		#if FPSGO_MW
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_render_info_params);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_render_attr_params);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_render_attr_params_tid);
		#endif
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_kfps_cpu_mask);
		fpsgo_sysfs_create_file(base_kobj, &kobj_attr_render_type);
	}

	fpsgo_update_tracemark();

	return 0;
}

