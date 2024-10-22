// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/seq_file.h>
#include <linux/energy_model.h>
#include <linux/jump_label.h>
#include <trace/events/sched.h>
#include <trace/events/task.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/hung_task.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "common.h"
#include "eas_plus.h"
#include "sched_sys_common.h"
#include "sugov/cpufreq.h"
#include "eas_adpf.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v2/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif
#include "sugov/dsu_interface.h"
#include "vip.h"
#include <mt-plat/mtk_irq_mon.h>
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
#include "flt_init.h"
#include "flt_api.h"
#include "group.h"
#include "flt_cal.h"
#endif
#include "grp_awr.h"

#define CREATE_TRACE_POINTS
#include "eas_trace.h"

#define TAG "EAS_IOCTL"

int mtk_sched_asym_cpucapacity  =  1;
unsigned int dn_pct = -1;
unsigned int up_pct = -1;
int gear_start = -1;
int num_gear = -1;
int reverse = -1;

static inline void sched_asym_cpucapacity_init(void)
{
	struct perf_domain *pd;
	struct root_domain *rd;
	int pd_count = 0;

	preempt_disable();
	rd = cpu_rq(smp_processor_id())->rd;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	for (; pd; pd = pd->next)
		pd_count++;

	rcu_read_unlock();
	preempt_enable();
	if (pd_count <= 1)
		mtk_sched_asym_cpucapacity = 0;
}

static void sched_task_util_hook(void *data, struct sched_entity *se)
{
	if (trace_sched_task_util_enabled()) {
		struct task_struct *p;
		struct sched_avg *sa;

		if (!entity_is_task(se))
			return;

		p = container_of(se, struct task_struct, se);
		sa = &se->avg;

		trace_sched_task_util(p->pid,
				sa->util_avg, sa->util_est.enqueued, sa->util_est.ewma);
	}
}

static void sched_task_uclamp_hook(void *data, struct sched_entity *se)
{
	if (trace_sched_task_uclamp_enabled()) {
		struct task_struct *p;
		struct sched_avg *sa;
		struct util_est ue;
		struct uclamp_se *uc_min_req, *uc_max_req;
		unsigned long util;

		if (!entity_is_task(se))
			return;

		p = container_of(se, struct task_struct, se);
		sa = &se->avg;
		ue = READ_ONCE(se->avg.util_est);
		util = max(ue.ewma, ue.enqueued);
		util = max(util, READ_ONCE(se->avg.util_avg));
		uc_min_req = &p->uclamp_req[UCLAMP_MIN];
		uc_max_req = &p->uclamp_req[UCLAMP_MAX];

		trace_sched_task_uclamp(p->pid, util,
				p->uclamp[UCLAMP_MIN].active,
				p->uclamp[UCLAMP_MIN].value, p->uclamp[UCLAMP_MAX].value,
				uc_min_req->user_defined, uc_min_req->value,
				uc_max_req->user_defined, uc_max_req->value);
	}
}

static int enqueue;
static int dequeue;
static void sched_queue_task_hook(void *data, struct rq *rq, struct task_struct *p, int flags)
{
	int cpu = rq->cpu;
	int type = *(int *)data;
	struct sugov_rq_data *sugov_data_ptr;
	irq_log_store();

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	if (type == enqueue)
		flt_rvh_enqueue_task(data, rq, p, flags);
	else if (type == dequeue)
		flt_rvh_dequeue_task(data, rq, p, flags);
#endif

	if (type == enqueue) {
		struct uclamp_se *uc_min_req, *uc_max_req;

		sugov_data_ptr = &((struct mtk_rq *) rq->android_vendor_data1)->sugov_data;
		WRITE_ONCE(sugov_data_ptr->enq_ing, true);

		uc_min_req = &p->uclamp_req[UCLAMP_MIN];
		uc_max_req = &p->uclamp_req[UCLAMP_MAX];

		if (uc_min_req->user_defined || uc_max_req->user_defined) {
			bool uclamp_diff = false;

			uclamp_diff = (READ_ONCE(sugov_data_ptr->uclamp[UCLAMP_MIN])
				!= READ_ONCE(rq->uclamp[UCLAMP_MIN].value));

			uclamp_diff = (uclamp_diff
				|| (READ_ONCE(sugov_data_ptr->uclamp[UCLAMP_MAX])
				!= READ_ONCE(rq->uclamp[UCLAMP_MIN].value)));

			if (uclamp_diff) {
				int this_cpu = smp_processor_id();
				struct rq *this_rq = cpu_rq(this_cpu);

				sugov_data_ptr =
					&((struct mtk_rq *) this_rq->android_vendor_data1)->sugov_data;
				WRITE_ONCE(sugov_data_ptr->enq_dvfs, true);
			}
		}
	}

	if (trace_sched_queue_task_enabled()) {
		unsigned long util = READ_ONCE(rq->cfs.avg.util_avg);

		util = max_t(unsigned long, util,
			     READ_ONCE(rq->cfs.avg.util_est.enqueued));

		trace_sched_queue_task(cpu, p->pid, type, util,
				rq->uclamp[UCLAMP_MIN].value, rq->uclamp[UCLAMP_MAX].value,
				p->uclamp[UCLAMP_MIN].value, p->uclamp[UCLAMP_MAX].value,
				p->cpus_ptr->bits[0]);
	}

	irq_log_store();
}


#if IS_ENABLED(CONFIG_DETECT_HUNG_TASK)

struct migration_arg {
	struct task_struct		*task;
	int				dest_cpu;
	struct set_affinity_pending	*pending;
};

/*
 * @refs: number of wait_for_completion()
 * @stop_pending: is @stop_work in use
 */
struct set_affinity_pending {
	refcount_t		refs;
	unsigned int		stop_pending;
	struct completion	done;
	struct cpu_stop_work	stop_work;
	struct migration_arg	arg;
};

static void mtk_check_d_tasks(void *data, struct task_struct *p,
				unsigned long t, bool *need_check)
{
	unsigned long pending_stime = 0;
	unsigned long switch_count = p->nvcsw + p->nivcsw;
	struct mig_task_struct *migts = &((struct mtk_task *)p->android_vendor_data1)->mig_task;
	struct cpumask cpus;
	struct set_affinity_pending *my_pending = NULL;
	struct task_struct *dest_task;
	struct migration_arg *arg;
	int dest_cpu;

	*need_check = true;

	/*
	 * Reset the migration_pending start time
	 */
	if (!p->migration_pending || switch_count != p->last_switch_count) {
		migts->pending_rec = 0;
		return;
	}

	/*check the cpu online mask */
	if (p->migration_pending) {
		if (!cpumask_and(&cpus, &p->cpus_mask, cpu_online_mask) ||
			is_migration_disabled(p)) {
			migts->pending_rec = 0;
			*need_check = false;
			return;
		}
	}

	/*
	 * Record the migration_pending start time
	 */
	if (p->migration_pending && migts->pending_rec == 0) {
		migts->pending_rec = jiffies;
		*need_check = false;
		return;
	}
	/*
	 * Check the whether migration time is time out or not
	 */
	pending_stime = migts->pending_rec;
	if (time_is_after_jiffies(pending_stime + t * HZ)) {
		*need_check = false;
	} else {
		*need_check = true;
		my_pending = p->migration_pending;
		if (my_pending) {
			arg = &my_pending->arg;
			dest_cpu = arg->dest_cpu;
			dest_task = cpu_curr(dest_cpu);
			pr_info(" pending %-15.15s %d %d %d %d\n",
				dest_task->comm, dest_task->pid, dest_cpu,
				my_pending->stop_pending, my_pending->done.done);
		}
		pr_info("flags:0x%x m_flags:%d %d %d %d %d 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx\n",
			p->flags, p->migration_flags, is_migration_disabled(p), p->on_rq, p->on_cpu,
			p->nr_cpus_allowed, p->cpus_mask.bits[0], p->cpus_ptr->bits[0],
			cpu_active_mask->bits[0], cpu_online_mask->bits[0],
			cpu_possible_mask->bits[0], cpu_pause_mask->bits[0]);
	}
}
#endif

void mtk_setscheduler_uclamp(void *data, struct task_struct *tsk,
	int clamp_id, unsigned int value)
{
	if (trace_sched_set_uclamp_enabled())
		trace_sched_set_uclamp(tsk->pid,
		task_cpu(tsk), task_on_rq_queued(tsk), clamp_id, value);
}

static void mtk_sched_pelt_multiplier(void *data, unsigned int old_pelt,
				      unsigned int new_pelt, int *ret)
{
	int pelt_weight = 0, pelt_sum = 0;

	switch (new_pelt) {
	/* pelt 32 */
	case 1:
		pelt_weight = 1002;
		pelt_sum = 47742;
		break;
	/* pelt 16 */
	case 2:
		pelt_weight = 981;
		pelt_sum = 24125;
		break;
	/* pelt 8 */
	case 4:
		pelt_weight = 939;
		pelt_sum = 12329;
		break;
	default:
		break;
	}
	if (pelt_weight && pelt_sum && is_wl_support()) {
		/* notify pelt model */
		update_pelt_data(pelt_weight, pelt_sum);
		pr_info("new_pelt %d pelt_w %d pelt_s %d\n", new_pelt, pelt_weight, pelt_sum);
	}
}

static void mtk_post_init_entity_util_avg(void *data, struct sched_entity *se)
{
	struct mtk_em_perf_state *ps = NULL;
	struct cfs_rq *cfs_rq = cfs_rq_of(se);
	struct sched_avg *sa = &se->avg;
	int cpu = cpu_of(rq_of(cfs_rq));
	unsigned long desired_cpu_scale = 0, ori = 0, desired_cpufreq;
	unsigned long long desired_util_avg = 0;
	struct task_struct *p = NULL;
	struct cgroup_subsys_state *css = NULL;
	bool post_init_util_ctl = sched_post_init_util_enable_get();

	if (!post_init_util_ctl)
		return;

	if (likely(entity_is_task(se)))
		p = task_of(se);
	else
		return;

	/* vip & ls don't hack*/
	if (
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
		task_is_vip(p, NOT_VIP)  ||
#endif
		is_task_latency_sensitive(p))
		return;

	/* TA don't hack*/
	css = task_css(p, cpu_cgrp_id);
	if (css) {
		if (!strcmp(css->cgroup->kn->name, "top-app"))
			return;
	}

	/*other hack...*/
	ps = pd_get_opp_ps(0, cpu, 0, false);
	desired_cpufreq = (ps->freq >> 2) + (ps->freq >> 3);
	desired_cpu_scale = pd_get_freq_util(cpu, desired_cpufreq);
	desired_util_avg = (desired_cpu_scale * 819) >> 10;

	/* suppressed to desired freq when fork*/
	if (sa->util_avg > (unsigned long)desired_util_avg) {
		ori = sa->util_avg;
		sa->util_avg = desired_util_avg;
		if (trace_sched_post_init_entity_util_avg_enabled()) {
			trace_sched_post_init_entity_util_avg(p, ori, sa->util_avg,
				se->load.weight, ps->freq, desired_cpufreq, cpu);
		}
		sa->runnable_avg = sa->util_avg;
	}
}

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
static void sched_irq_mon_init(void)
{
	mtk_register_irq_log_store(__irq_log_store);
}
#endif

static void mtk_sched_trace_init(void)
{
	int ret = 0;

	enqueue = 1;
	dequeue = -1;

	ret = register_trace_android_rvh_enqueue_task(sched_queue_task_hook, &enqueue);
	if (ret)
		pr_info("register android_rvh_enqueue_task failed!\n");
	ret = register_trace_android_rvh_dequeue_task(sched_queue_task_hook, &dequeue);
	if (ret)
		pr_info("register android_rvh_dequeue_task failed!\n");

	ret = register_trace_pelt_se_tp(sched_task_util_hook, NULL);
	if (ret)
		pr_info("register sched_task_util_hook failed!\n");

	ret = register_trace_pelt_se_tp(sched_task_uclamp_hook, NULL);
	if (ret)
		pr_info("register sched_task_uclamp_hook failed!\n");
}

static void mtk_sched_trace_exit(void)
{
	unregister_trace_pelt_se_tp(sched_task_util_hook, NULL);
	unregister_trace_pelt_se_tp(sched_task_uclamp_hook, NULL);
}

static unsigned long easctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static unsigned long easctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

/*--------------------SYNC------------------------*/
static int eas_show(struct seq_file *m, void *v)
{
	return 0;
}

static int eas_open(struct inode *inode, struct file *file)
{
	return single_open(file, eas_show, inode->i_private);
}

static long eas_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;

	unsigned int sync;
	unsigned int val;
	int grp_id;
	struct cpumask mask;

	struct SA_task SA_task_args = {
		.pid = -1,
		.mask = 0
	};

	struct gas_ctrl gas_ctrl_args = {
		.val = 0,
		.force_ctrl = 0
	};

	struct gas_thr gas_thr_args = {
		.grp_id = -1,
		.val = -1
	};

	struct gas_margin_thr gas_margin_args = {
		.gear_id = -1,
		.group_id = -1,
		.margin = -1,
		.converge_thr = -1
	};

	cpumask_clear(&mask);
	cpumask_copy(&mask, cpu_possible_mask);
	SA_task_args.mask = mask.bits[0];

	switch (cmd) {
	case EAS_IGNORE_IDLE_UTIL_CTRL:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_ignore_idle_ctrl(val);
		break;
	case EAS_SYNC_SET:
		if (easctl_copy_from_user(&sync, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_wake_sync(sync);
		break;
	case EAS_SYNC_GET:
		sync = get_wake_sync();
		if (easctl_copy_to_user((void *)arg, &sync, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_PERTASK_LS_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_uclamp_min_ls(val);
		break;
	case EAS_PERTASK_LS_GET:
		val = get_uclamp_min_ls();
		if (easctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_ACTIVE_MASK_GET:
		val = __cpu_active_mask.bits[0];
		if (easctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_NEWLY_IDLE_BALANCE_INTERVAL_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_newly_idle_balance_interval_us(val);
		break;
	case EAS_NEWLY_IDLE_BALANCE_INTERVAL_GET:
		val = get_newly_idle_balance_interval_us();
		if (easctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_GET_THERMAL_HEADROOM_INTERVAL_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_get_thermal_headroom_interval_tick(val);
		break;
	case EAS_GET_THERMAL_HEADROOM_INTERVAL_GET:
		val = get_thermal_headroom_interval_tick();
		if (easctl_copy_to_user((void *)arg, &val, sizeof(unsigned int)))
			return -1;
		break;
	case EAS_SET_CPUMASK_TA:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_top_app_cpumask(val);
		break;
	case EAS_SET_CPUMASK_BACKGROUND:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_foreground_cpumask(val);
		break;
	case EAS_SET_CPUMASK_FOREGROUND:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_background_cpumask(val);
		break;
	case EAS_SET_TASK_LS:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_task_ls(val);
		break;
	case EAS_UNSET_TASK_LS:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		unset_task_ls(val);
		break;
	case EAS_SET_TASK_LS_PREFER_CPUS:
		if (easctl_copy_from_user(&SA_task_args, (void *)arg, sizeof(struct SA_task)))
			return -1;
		set_task_ls_prefer_cpus(SA_task_args.pid, SA_task_args.mask);
		break;
	case EAS_SET_TASK_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_task_basic_vip(val);
		break;
	case EAS_UNSET_TASK_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		unset_task_basic_vip(val);
		break;
	case EAS_SET_TA_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_top_app_vip(val);
		break;
	case EAS_UNSET_TA_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		unset_top_app_vip();
		break;
	case EAS_SET_FG_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_foreground_vip(val);
		break;
	case EAS_UNSET_FG_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		unset_foreground_vip();
		break;
	case EAS_SET_BG_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_background_vip(val);
		break;
	case EAS_UNSET_BG_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		unset_background_vip();
		break;
	case EAS_SET_LS_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_ls_task_vip(val);
		break;
	case EAS_UNSET_LS_VIP:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		unset_ls_task_vip();
		break;
	case EAS_GEAR_MIGR_DN_PCT:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		dn_pct = val;
		break;
	case EAS_GEAR_MIGR_UP_PCT:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		up_pct = val;
		break;
	case EAS_GEAR_MIGR_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		if (!set_updown_migration_pct(val, dn_pct, up_pct))
			return -1;
		dn_pct = up_pct = 0; //set success, reset local value
		break;
	case EAS_GEAR_MIGR_UNSET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		if (!unset_updown_migration_pct(val))
			return -1;
		break;
	case EAS_TASK_GEAR_HINTS_START:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		gear_start = (int) val;
		break;
	case EAS_TASK_GEAR_HINTS_NUM:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		num_gear = (int) val;
		break;
	case EAS_TASK_GEAR_HINTS_REVERSE:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		reverse = (int) val;
		break;
	case EAS_TASK_GEAR_HINTS_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		if (!set_gear_indices(val, gear_start, num_gear, reverse))
			return -1;
		gear_start = num_gear = reverse = -1; //set success, reset local value
		break;
	case EAS_TASK_GEAR_HINTS_UNSET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		if (!unset_gear_indices(val))
			return -1;
		break;
	case EAS_SBB_ALL_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_ALL, 0, true);
		break;
	case EAS_SBB_ALL_UNSET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_ALL, 0, false);
		break;
	case EAS_SBB_GROUP_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_GROUP, val, true);
		break;
	case EAS_SBB_GROUP_UNSET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_GROUP, val, false);
		break;
	case EAS_SBB_TASK_SET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_TASK, val, true);
		break;
	case EAS_SBB_TASK_UNSET:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb(SBB_TASK, val, false);
		break;
	case EAS_SBB_ACTIVE_RATIO:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_sbb_active_ratio(val);
		break;
	case EAS_TURN_POINT_UTIL_C0:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_turn_point_freq(0, val);
		break;
	case EAS_TURN_POINT_UTIL_C1:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_turn_point_freq(1, val);
		break;
	case EAS_TURN_POINT_UTIL_C2:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_turn_point_freq(2, val);
		break;
	case EAS_TARGET_MARGIN_C0:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_target_margin(0, val);
		break;
	case EAS_TARGET_MARGIN_C1:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_target_margin(1, val);
		break;
	case EAS_TARGET_MARGIN_C2:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_target_margin(2, val);
		break;
	case EAS_UTIL_EST_CONTROL:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		set_util_est_ctrl(val);
		break;
	case EAS_SET_GAS_CTRL:
		if (easctl_copy_from_user(&gas_ctrl_args, (void *)arg, sizeof(struct gas_ctrl)))
			return -1;
		set_top_grp_aware(gas_ctrl_args.val, gas_ctrl_args.force_ctrl);
		break;
	case EAS_SET_GAS_THR:
		if (easctl_copy_from_user(&gas_thr_args, (void *)arg, sizeof(struct gas_thr)))
			return -1;
		group_set_threshold(gas_thr_args.grp_id, gas_thr_args.val);
		break;
	case EAS_RESET_GAS_THR:
		if (easctl_copy_from_user(&grp_id, (void *)arg, sizeof(int)))
			return -1;
		group_reset_threshold(grp_id);
		break;
	case EAS_SET_GAS_MARG_THR:
		if (easctl_copy_from_user(&gas_margin_args, (void *)arg,
				sizeof(struct gas_margin_thr)))
			return -1;
		set_grp_awr_thr(gas_margin_args.gear_id, gas_margin_args.group_id,
			gas_margin_args.converge_thr);
		set_grp_awr_min_opp_margin(gas_margin_args.gear_id, gas_margin_args.group_id,
			gas_margin_args.margin);
		break;
	case EAS_RESET_GAS_MARG_THR:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(int)))
			return -1;
		if(reset_grp_awr_margin() == -1)
			return -1;
		break;
	case EAS_SET_AM_CTRL:
		if (easctl_copy_from_user(&val, (void *)arg, sizeof(unsigned int)))
			return -1;
		 set_am_ctrl(val);
		break;
	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long eas_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return eas_ioctl_impl(filp, cmd, arg, NULL);
}

static const struct proc_ops eas_Fops = {
	.proc_ioctl = eas_ioctl,
	.proc_compat_ioctl = eas_ioctl,
	.proc_open = eas_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)

static int flt_resume_cb(struct device *dev)
{
	flt_resume_notify();
	return 0;
}

static int flt_suspend_cb(struct device *dev)
{
	flt_suspend_notify();
	return 0;
}

static const struct dev_pm_ops flt_pm_ops = {
	.resume_noirq = flt_resume_cb,
	.suspend_noirq = flt_suspend_cb,
};

static int platform_flt_probe(struct platform_device *pdev)
{
	int ret = 0, retval = 0;
	u32 flt_mode = FLT_MODE_0;
	struct device_node *node;

	if (!of_have_populated_dt())
		goto flt_exit;

	node = pdev->dev.of_node;

	ret = of_property_read_u32(node, "mode", &retval);
	if (!ret)
		flt_mode = retval;

flt_exit:
		pr_info("FLT flt_mode=%d\n", flt_mode);
		flt_set_mode(flt_mode);
		return 0;
}

static const struct of_device_id platform_flt_match[] = {
	{ .compatible = "mediatek,flt", },
	{},
};

static const struct platform_device_id platform_flt_id_table[] = {
	{ "flt", 0},
	{ },
};

static struct platform_driver mtk_platform_flt_driver = {
	.probe = platform_flt_probe,
	.driver = {
		.name = "FLT",
		.owner = THIS_MODULE,
		.of_match_table = platform_flt_match,
		.pm = &flt_pm_ops,
	},
	.id_table = platform_flt_id_table,
};

void init_flt_platform(void)
{
	platform_driver_register(&mtk_platform_flt_driver);
}

void exit_flt_platform(void)
{
	platform_driver_unregister(&mtk_platform_flt_driver);
}
#endif

static void temp_cgroup_force_kthread_migration(void *unused, struct task_struct *tsk,
						struct cgroup *dst_cgrp,
						bool *force_migration)
{
	/*
	 * RT kthreads may be born in a cgroup with no rt_runtime allocated.
	 * Just say no.
	 */

#ifdef CONFIG_RT_GROUP_SCHED
	if (tsk->no_cgroup_migration && (dst_cgrp->root->subsys_mask & (1U << cpu_cgrp_id)))
		return;
#endif

	/*
	 * kthreads may acquire PF_NO_SETAFFINITY during initialization.
	 * If userland migrates such a kthread to a non-root cgroup, it can
	 * become trapped in a cpuset. Just say no.
	 */
#ifdef CONFIG_CPUSETS
	if ((tsk->no_cgroup_migration || (tsk->flags & PF_NO_SETAFFINITY)) &&
			(dst_cgrp->root->subsys_mask & (1U << cpuset_cgrp_id)))
		return;
#endif
	*force_migration = true;
}

static int __init mtk_scheduler_init(void)
{
	struct proc_dir_entry *pe, *parent;
	int ret = 0;

	/* compile time checks for vendor data size */
	MTK_VENDOR_DATA_SIZE_TEST(struct mtk_task, struct task_struct);
	MTK_VENDOR_DATA_SIZE_TEST(struct mtk_tg, struct task_group);
	MTK_VENDOR_DATA_SIZE_TEST(struct mtk_rq, struct rq);

	/* build cpu_array for hints-based gear search*/
	init_cpu_array();
	build_cpu_array();
	init_gear_hints();

	init_updown_migration();

	ret = init_sched_common_sysfs();
	if (ret)
		return ret;

	ret = init_share_buck();
	if (ret)
		return ret;

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	vip_init();
#endif

	init_skip_hiIRQ();
	init_rt_aggre_preempt();

#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	init_flt_platform();
	flt_init_res();
	group_init();
#endif

	sched_asym_cpucapacity_init();

	get_most_powerful_pd_and_util_Th();

	mtk_sched_trace_init();

	ret = adpf_register_callback(sched_adpf_callback);
	if (ret)
		pr_info("register adpf_register_callback failed\n");

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
	sched_pause_init();
#endif

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	sched_irq_mon_init();
#endif

	if (is_wl_support()) {
		/* default value for pelt8 */
		update_pelt_data(939, 12329);
	}

#if IS_ENABLED(CONFIG_MTK_EAS)
	mtk_freq_limit_notifier_register();

	soft_affinity_init();

	ret = init_sram_info();
	if (ret)
		return ret;

	ret = register_trace_android_rvh_find_busiest_group(
			mtk_find_busiest_group, NULL);
	if (ret)
		pr_info("register android_rvh_find_busiest_group failed\n");

	ret = register_trace_android_rvh_can_migrate_task(
			mtk_can_migrate_task, NULL);
	if (ret)
		pr_info("register android_rvh_can_migrate_task failed\n");

	ret = register_trace_android_rvh_find_energy_efficient_cpu(
			mtk_find_energy_efficient_cpu, NULL);
	if (ret)
		pr_info("register android_rvh_find_energy_efficient_cpu failed\n");


	ret = register_trace_android_rvh_cpu_overutilized(
			mtk_cpu_overutilized, NULL);
	if (ret)
		pr_info("register trace_android_rvh_cpu_overutilized failed\n");


	ret = register_trace_android_rvh_tick_entry(
			mtk_tick_entry, NULL);
	if (ret)
		pr_info("register android_rvh_tick_entry failed\n");


	ret = register_trace_android_vh_set_wake_flags(
			mtk_set_wake_flags, NULL);
	if (ret)
		pr_info("register android_vh_set_wake_flags failed\n");


	ret = register_trace_android_rvh_update_cpu_capacity(
			mtk_update_cpu_capacity, NULL);
	if (ret)
		pr_info("register android_rvh_update_cpu_capacity failed\n");

	ret = register_trace_pelt_rt_tp(mtk_pelt_rt_tp, NULL);
	if (ret)
		pr_info("register mtk_pelt_rt_tp hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_schedule(mtk_sched_switch, NULL);
	if (ret)
		pr_info("register mtk_sched_switch hooks failed, returned %d\n", ret);

#if IS_ENABLED(CONFIG_MTK_NEWIDLE_BALANCE)
	ret = register_trace_android_rvh_sched_newidle_balance(
			mtk_sched_newidle_balance, NULL);
	if (ret)
		pr_info("register android_rvh_sched_newidle_balance failed\n");
#endif

	pr_debug(TAG"Start to init eas_ioctl driver\n");
	parent = proc_mkdir("easmgr", NULL);
	pe = proc_create("eas_ioctl", 0660, parent, &eas_Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret);
		ret = -ENOMEM;
		goto out_wq;
	}

#endif

	ret = register_trace_android_vh_scheduler_tick(hook_scheduler_tick, NULL);
	if (ret)
		pr_info("scheduler: register scheduler_tick hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_after_enqueue_task(mtk_hook_after_enqueue_task, NULL);
	if (ret)
		pr_info("register android_rvh_after_enqueue_task failed, returned %d\n", ret);

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
	ret = register_trace_android_rvh_new_task_stats(rotat_task_stats, NULL);
	if (ret)
		pr_info("register android_rvh_new_task_stats failed, returned %d\n", ret);

	ret = register_trace_task_newtask(rotat_task_newtask, NULL);
	if (ret)
		pr_info("register trace_task_newtask failed, returned %d\n", ret);
#endif
	ret = register_trace_android_rvh_select_task_rq_rt(mtk_select_task_rq_rt, NULL);
	if (ret)
		pr_info("register mtk_select_task_rq_rt hooks failed, returned %d\n", ret);


	ret = register_trace_android_rvh_find_lowest_rq(mtk_find_lowest_rq, NULL);
	if (ret)
		pr_info("register find_lowest_rq hooks failed, returned %d\n", ret);

	ret = register_trace_android_vh_sched_pelt_multiplier(mtk_sched_pelt_multiplier, NULL);
	if (ret)
		pr_info("register mtk_sched_pelt_multiplier hooks failed, returned %d\n", ret);

	ret = register_trace_android_vh_dump_throttled_rt_tasks(throttled_rt_tasks_debug, NULL);
	if (ret)
		pr_info("register dump_throttled_rt_tasks hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_post_init_entity_util_avg(
		mtk_post_init_entity_util_avg, NULL);
	if (ret)
		pr_info("register mtk_post_init_entity_util_avg hooks failed, returned %d\n", ret);

#if IS_ENABLED(CONFIG_DETECT_HUNG_TASK)
	ret = register_trace_android_vh_check_uninterrupt_tasks(mtk_check_d_tasks, NULL);
	if (ret)
		pr_info("register mtk_check_d_tasks hooks failed, returned %d\n", ret);
#endif
	ret = register_trace_android_vh_setscheduler_uclamp(mtk_setscheduler_uclamp, NULL);
	if (ret)
		pr_info("register mtk_setscheduler_uclamp hooks failed, returned %d\n", ret);

	ret = register_trace_android_rvh_cgroup_force_kthread_migration(temp_cgroup_force_kthread_migration, NULL);
	if (ret)
		pr_info("register temp_cgroup_force_kthread_migration hook failed, returned %d\n", ret);

out_wq:
	return ret;

}

static void __exit mtk_scheduler_exit(void)
{
	mtk_sched_trace_exit();
	unregister_trace_android_vh_scheduler_tick(hook_scheduler_tick, NULL);
#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
	unregister_trace_task_newtask(rotat_task_newtask, NULL);
#endif
	cleanup_sched_common_sysfs();
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
	exit_flt_platform();
#endif
	group_exit();
	free_cpu_array();
}

module_init(mtk_scheduler_init);
module_exit(mtk_scheduler_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek scheduler");
MODULE_AUTHOR("MediaTek Inc.");
