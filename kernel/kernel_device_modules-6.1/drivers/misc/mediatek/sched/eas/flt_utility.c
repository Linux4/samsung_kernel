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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/energy_model.h>
#include <linux/cgroup.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <sugov/cpufreq.h>
#include "sched_sys_common.h"
#include "eas_plus.h"
#include "common.h"
#include "flt_init.h"
#include "flt_api.h"
#include "group.h"
#include "flt_utility.h"
#include "flt_cal.h"
#include "eas_trace.h"
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
int (*grp_cal_tra)(int x, unsigned int y);
EXPORT_SYMBOL(grp_cal_tra);
#endif
static u32 flt_nid = FLT_GP_NID;

void flt_update_data(unsigned int data, unsigned int offset)
{
	void __iomem *flt_adr;
	unsigned long long len;

	if (!IS_ALIGNED(offset, PER_ENTRY))
		return;
	len = get_flt_xrg_size();
	flt_adr = get_flt_xrg();
	if (flt_adr && offset <= len)
		iowrite32(data, flt_adr + offset);
}

unsigned int flt_get_data(unsigned int offset)
{
	void __iomem *flt_adr;
	unsigned int res = 0;
	unsigned long long len;

	if (!IS_ALIGNED(offset, PER_ENTRY))
		return res;
	len = get_flt_xrg_size();
	flt_adr = get_flt_xrg();
	if (flt_adr && offset <= len)
		res = ioread32(flt_adr + offset);

	return res;
}

static int flt_is_valid(void)
{
	int res = 0;

	res = flt_get_data(FLT_VALID);
	return res;
}

static int flt_get_window_size_mode2(void)
{
	int res = 0;

	res = flt_get_data(AP_WS_CTL);

	return res;
}

static int flt_set_window_size_mode2(int ws)
{
	int res = 0;

	flt_update_data(ws, AP_WS_CTL);

	return res;
}

static int flt_sched_set_group_policy_eas_mode2(int grp_id, int ws, int wp, int wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return -1;

	offset = grp_id * PER_ENTRY;
	update_data = (wp << WP_LEN) | wc;
	flt_update_data(ws, AP_WS_CTL);
	flt_update_data(update_data, AP_GP_SETTING_STA_ADDR + offset);

	return res;
}

static int flt_sched_get_group_policy_eas_mode2(int grp_id, int *ws, int *wp, int *wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0)
		return -1;
	offset = grp_id * PER_ENTRY;
	update_data = flt_get_data(AP_GP_SETTING_STA_ADDR + offset);
	*ws = flt_get_data(AP_WS_CTL);
	*wp = update_data >> WP_LEN;
	*wc = update_data & WC_MASK;

	return res;
}

static int flt_sched_set_cpu_policy_eas_mode2(int cpu, int ws, int wp, int wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -1;
	offset = cpu * PER_ENTRY;
	update_data = (wp << WP_LEN) | wc;
	flt_update_data(ws, AP_WS_CTL);
	flt_update_data(update_data, AP_CPU_SETTING_ADDR + offset);
	return res;
}

static int flt_sched_get_cpu_policy_eas_mode2(int cpu, int *ws, int *wp, int *wc)
{
	int res = 0;
	unsigned int offset, update_data;

	if (!cpumask_test_cpu(cpu, cpu_possible_mask))
		return -1;
	offset = cpu * PER_ENTRY;
	update_data = flt_get_data(AP_CPU_SETTING_ADDR + offset);
	*ws = flt_get_data(AP_WS_CTL);
	*wp = update_data >> WP_LEN;
	*wc = update_data & WC_MASK;

	return res;
}

static int flt_get_sum_group_mode2(int grp_id)
{
	int res = 0;
	unsigned int offset;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0 || flt_is_valid() != 1)
		return -1;
	offset = grp_id * PER_ENTRY;
	if (flt_nid == FLT_GP_NID)
		res = flt_get_data(GP_NIDWP + offset);
	else
		res = flt_get_data(GP_RWP + offset);

	return res;
}

static int flt_get_total_group_mode2(void)
{
	int res = 0;

	if (flt_is_valid() != 1)
		goto out;

	res = flt_get_data(GP_NIDS);
out:
	return res;
}

static int flt_get_grp_hint_mode2(int grp_id)
{
	int res = 0;
	unsigned int offset;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0 || flt_is_valid() != 1)
		return -1;
	offset = grp_id * PER_ENTRY;
	res = flt_get_data(GP_H + offset);

	return res;
}

static int flt_get_cpu_r_mode2(int cpu)
{
	int res = 0;
	unsigned int offset;

	if (!cpumask_test_cpu(cpu, cpu_possible_mask) || flt_is_valid() != 1)
		return -1;

	offset = cpu * PER_ENTRY;
	res = flt_get_data(CPU_R + offset);
	return res;
}

static int flt_get_grp_r_mode2(int grp_id)
{
	int res = 0;
	unsigned int offset;

	if (grp_id >= GROUP_ID_RECORD_MAX || grp_id < 0 || flt_is_valid() != 1)
		return -1;
	offset = grp_id * PER_ENTRY;
	res = flt_get_data(GP_R + offset);

	return res;
}

int flt_sched_get_gear_sum_group_eas_mode2(int gear_id, int group_id)
{
	unsigned int nr_gear, gear_idx;
	int pelt_util = 0, res = 0;
	u64 flt_util = 0, gear_util = 0, total_util = 0;

	if (group_id >= GROUP_ID_RECORD_MAX || group_id < 0)
		return -1;

	nr_gear = get_nr_gears();

	if (gear_id >= nr_gear || gear_id < 0)
		return -1;

	flt_util = flt_get_sum_group(group_id);

	for (gear_idx = 0; gear_idx < nr_gear; gear_idx++) {
		pelt_util = flt_get_gear_sum_pelt_group(gear_idx, group_id);
		if (gear_idx == gear_id)
			gear_util	= pelt_util;
		total_util += pelt_util;
	}
	if (total_util)
		res = (int)div64_u64(flt_util * gear_util, total_util);
	return res;
}

static int flt_get_cpu_by_wp_mode2(int cpu)
{
	struct rq *rq;
	struct flt_rq *fsrq;
	int cpu_dmand_util = 0;

	if (unlikely(!cpumask_test_cpu(cpu, cpu_possible_mask)))
		return -1;

	rq = cpu_rq(cpu);
	fsrq = &per_cpu(flt_rq, cpu);

	cpu_dmand_util = READ_ONCE(fsrq->cpu_tar_util);
	return cpu_dmand_util;
}

static int flt_sched_get_cpu_group_eas_mode2(int cpu_idx, int group_id)
{
	int res = 0, flt_util = 0;
	struct flt_rq *fsrq;
	u32 util_ratio = 0;

	if (group_id >= GROUP_ID_RECORD_MAX ||
		group_id < 0 ||
		!cpumask_test_cpu(cpu_idx, cpu_possible_mask))
		return -1;

	flt_util = flt_get_sum_group_mode2(group_id);

	fsrq = &per_cpu(flt_rq, cpu_idx);
	if (flt_nid == FLT_GP_NID)
		util_ratio = READ_ONCE(fsrq->group_util_ratio[group_id]);
	else
		util_ratio = READ_ONCE(fsrq->group_raw_util_ratio[group_id]);
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
	if (grp_cal_tra)
		res = grp_cal_tra(flt_util, util_ratio);
#endif
	res = clamp_t(int, res, 0, cpu_cap_ceiling(cpu_idx));

	return res;
}

static int flt_get_o_util_mode2(int cpu)
{
	int cpu_r = 0, grp_idx = 0, res, flt_util = 0;
	struct rq *rq;
	struct flt_rq *fsrq;
	u32 util_ratio[GROUP_ID_RECORD_MAX] = {0}, grp_r[GROUP_ID_RECORD_MAX] = {0}, total = 0;

	rq = cpu_rq(cpu);
	fsrq = &per_cpu(flt_rq, cpu);

	cpu_r = flt_get_cpu_r(cpu);

	for (grp_idx = 0; grp_idx < GROUP_ID_RECORD_MAX; ++grp_idx) {
		util_ratio[grp_idx] = READ_ONCE(fsrq->group_util_rtratio[grp_idx]);
		flt_util = flt_get_gp_r(grp_idx);
#if IS_ENABLED(CONFIG_MTK_SCHED_GROUP_AWARE)
		if (grp_cal_tra)
			grp_r[grp_idx] = grp_cal_tra(flt_util, util_ratio[grp_idx]);
#endif
		total += grp_r[grp_idx];
		if (trace_sched_flt_get_o_util_enabled())
			trace_sched_flt_get_o_util(cpu, cpu_r, grp_idx, util_ratio[grp_idx],
										flt_util, grp_r[grp_idx], total);
	}
	res = cpu_r - total;
	res = clamp_t(int, res, 0, cpu_cap_ceiling(cpu));

	return res;
}

static void flt_ctl_mode2(int set)
{
	/* control grp dvfs reference signal*/
	if (set)
		set_grp_dvfs_ctrl(FLT_MODE2_EN);
	else
		set_grp_dvfs_ctrl(0);
}

void flt_mode2_register_api_hooks(void)
{
	flt_get_ws_api = flt_get_window_size_mode2;
	flt_set_ws_api = flt_set_window_size_mode2;
	flt_sched_set_group_policy_eas_api = flt_sched_set_group_policy_eas_mode2;
	flt_sched_get_group_policy_eas_api = flt_sched_get_group_policy_eas_mode2;
	flt_sched_set_cpu_policy_eas_api = flt_sched_set_cpu_policy_eas_mode2;
	flt_sched_get_cpu_policy_eas_api = flt_sched_get_cpu_policy_eas_mode2;
	flt_get_sum_group_api = flt_get_sum_group_mode2;
	flt_sched_get_gear_sum_group_eas_api = flt_sched_get_gear_sum_group_eas_mode2;
	flt_get_cpu_by_wp_api = flt_get_cpu_by_wp_mode2;
	flt_sched_get_cpu_group_eas_api = flt_sched_get_cpu_group_eas_mode2;
	flt_get_grp_h_eas_api = flt_get_grp_hint_mode2;
	flt_get_cpu_r_api = flt_get_cpu_r_mode2;
	flt_get_cpu_o_eas_api = flt_get_o_util_mode2;
	flt_get_total_gp_api = flt_get_total_group_mode2;
	flt_get_grp_r_eas_api = flt_get_grp_r_mode2;
	flt_ctl_api = flt_ctl_mode2;
}

void flt_mode2_init_res(void)
{
	int cpu, i;

	flt_set_ws(DEFAULT_WS);
	for_each_possible_cpu(cpu)
		flt_sched_set_cpu_policy_eas(cpu, DEFAULT_WS, CPU_DEFAULT_WP, CPU_DEFAULT_WC);
	for (i = 0; i < GROUP_ID_RECORD_MAX; i++)
		flt_sched_set_group_policy_eas(i, DEFAULT_WS, GRP_DEFAULT_WP, GRP_DEFAULT_WC);
	flt_update_data(FLT_MODE2_EN, AP_FLT_CTL);
}

int flt_setnid(u32 mode)
{
	if (mode >= FLT_GP_NUM)
		return -1;
	flt_nid = mode;
	return 0;
}
EXPORT_SYMBOL(flt_setnid);

u32 flt_getnid(void)
{
	return flt_nid;
}
EXPORT_SYMBOL(flt_getnid);
