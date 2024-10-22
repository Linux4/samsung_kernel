/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM scheduler

#if !defined(_TRACE_SUGOV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SUGOV_H
#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(sugov_ext_curr_uclamp,
	TP_PROTO(int cpu, int pid,
		int util_ori, int util, int u_min,
		int u_max),
	TP_ARGS(cpu, pid, util_ori, util, u_min, u_max),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, pid)
		__field(unsigned int, util_ori)
		__field(unsigned int, util)
		__field(unsigned int, u_min)
		__field(unsigned int, u_max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->util_ori = util_ori;
		__entry->util = util;
		__entry->u_min = u_min;
		__entry->u_max = u_max;
	),
	TP_printk(
		"cpu=%d pid=%d util_ori=%d util_eff=%d u_min=%d, u_max=%d",
		__entry->cpu,
		__entry->pid,
		__entry->util_ori,
		__entry->util,
		__entry->u_min,
		__entry->u_max)
);

TRACE_EVENT(sugov_ext_gear_uclamp,
	TP_PROTO(int cpu, int util_ori,
		int umin, int umax, int util,
		int umax_gear),
	TP_ARGS(cpu, util_ori, umin, umax, util, umax_gear),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, util_ori)
		__field(unsigned int, umin)
		__field(unsigned int, umax)
		__field(unsigned int, util)
		__field(unsigned int, umax_gear)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util_ori = util_ori;
		__entry->umin = umin;
		__entry->umax = umax;
		__entry->util = util;
		__entry->umax_gear = umax_gear;
	),
	TP_printk(
		"cpu=%d util_ori=%d umin=%d umax=%d util_ret=%d, umax_gear=%d",
		__entry->cpu,
		__entry->util_ori,
		__entry->umin,
		__entry->umax,
		__entry->util,
		__entry->umax_gear)
);

TRACE_EVENT(sugov_ext_util,
	TP_PROTO(int cpu, unsigned long util,
		unsigned int min, unsigned int max),
	TP_ARGS(cpu, util, min, max),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned int, min)
		__field(unsigned int, max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->min = min;
		__entry->max = max;
	),
	TP_printk(
		"cpu=%d util=%lu min=%u max=%u",
		__entry->cpu,
		__entry->util,
		__entry->min,
		__entry->max)
);

TRACE_EVENT(sugov_ext_wl_type,
	TP_PROTO(unsigned int gear_id, unsigned int cpu, int wl_type),
	TP_ARGS(gear_id, cpu, wl_type),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, cpu)
		__field(unsigned int, wl_type)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->cpu = cpu;
		__entry->wl_type = wl_type;
	),
	TP_printk(
		"gear_id=%u cpu=%u wl_type=%u",
		__entry->gear_id,
		__entry->cpu,
		__entry->wl_type)
);

TRACE_EVENT(sugov_ext_dsu_freq_vote,
	TP_PROTO(unsigned int wl_type, unsigned int gear_id,
		unsigned int cpu_freq, unsigned int dsu_freq_vote),
	TP_ARGS(wl_type, gear_id, cpu_freq, dsu_freq_vote),
	TP_STRUCT__entry(
		__field(unsigned int, wl_type)
		__field(unsigned int, gear_id)
		__field(unsigned int, cpu_freq)
		__field(unsigned int, dsu_freq_vote)
	),
	TP_fast_assign(
		__entry->wl_type = wl_type;
		__entry->gear_id = gear_id;
		__entry->cpu_freq = cpu_freq;
		__entry->dsu_freq_vote = dsu_freq_vote;
	),
	TP_printk(
		"wl_type=%u gear_id=%u cpu_freq=%u dsu_freq_vote=%u",
		__entry->wl_type,
		__entry->gear_id,
		__entry->cpu_freq,
		__entry->dsu_freq_vote)
);

TRACE_EVENT(sugov_ext_gear_state,
	TP_PROTO(unsigned int gear_id, unsigned int state),
	TP_ARGS(gear_id, state),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, state)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->state = state;
	),
	TP_printk(
		"gear_id=%u state=%u",
		__entry->gear_id,
		__entry->state)
);

TRACE_EVENT(sugov_ext_sbb,
	TP_PROTO(int cpu, int pid, unsigned int boost,
		unsigned int util, unsigned int util_boost, unsigned int active_ratio,
		unsigned int threshold),
	TP_ARGS(cpu, pid, boost, util, util_boost, active_ratio, threshold),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(unsigned int, boost)
		__field(unsigned int, util)
		__field(unsigned int, util_boost)
		__field(unsigned int, active_ratio)
		__field(unsigned int, threshold)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->boost = boost;
		__entry->util = util;
		__entry->util_boost = util_boost;
		__entry->active_ratio = active_ratio;
		__entry->threshold = threshold;
	),
	TP_printk(
		"cpu=%d pid=%d boost=%d util=%d util_boost=%d active_ratio=%d, threshold=%d",
		__entry->cpu,
		__entry->pid,
		__entry->boost,
		__entry->util,
		__entry->util_boost,
		__entry->active_ratio,
		__entry->threshold)
);

TRACE_EVENT(sugov_ext_adaptive_margin,
	TP_PROTO(unsigned int gear_id, unsigned int margin, unsigned int ratio),
	TP_ARGS(gear_id, margin, ratio),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, margin)
		__field(unsigned int, ratio)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->margin = margin;
		__entry->ratio = ratio;
	),
	TP_printk(
		"gear_id=%u margin=%u ratio=%u",
		__entry->gear_id,
		__entry->margin,
		__entry->ratio)
);

TRACE_EVENT(sugov_ext_tar,
	TP_PROTO(int cpu, unsigned long ret_util,
		unsigned long cpu_util, unsigned long umax, int am),
	TP_ARGS(cpu, ret_util, cpu_util, umax, am),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, ret_util)
		__field(unsigned long, cpu_util)
		__field(unsigned long, umax)
		__field(int, am)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->ret_util = ret_util;
		__entry->cpu_util = cpu_util;
		__entry->umax = umax;
		__entry->am = am;
	),
	TP_printk(
		"cpu=%d ret_util=%ld cpu_util=%ld umax=%ld am=%d",
		__entry->cpu,
		__entry->ret_util,
		__entry->cpu_util,
		__entry->umax,
		__entry->am)
);

TRACE_EVENT(sugov_ext_group_dvfs,
	TP_PROTO(int gearid, unsigned long util, unsigned long pelt_util_with_margin,
		unsigned long flt_util, unsigned long pelt_util, unsigned long pelt_margin,
		unsigned long freq),
	TP_ARGS(gearid, util, pelt_util_with_margin, flt_util, pelt_util, pelt_margin, freq),
	TP_STRUCT__entry(
		__field(int, gearid)
		__field(unsigned long, util)
		__field(unsigned long, pelt_util_with_margin)
		__field(unsigned long, flt_util)
		__field(unsigned long, pelt_util)
		__field(unsigned long, pelt_margin)
		__field(unsigned long, freq)
	),
	TP_fast_assign(
		__entry->gearid = gearid;
		__entry->util = util;
		__entry->pelt_util_with_margin = pelt_util_with_margin;
		__entry->flt_util = flt_util;
		__entry->pelt_util = pelt_util;
		__entry->pelt_margin = pelt_margin;
		__entry->freq = freq;
	),
	TP_printk(
		"gearid=%d ret=%lu pelt_with_margin=%lu tar=%lu pelt=%lu pelt_margin=%lu freq=%lu",
		__entry->gearid,
		__entry->util,
		__entry->pelt_util_with_margin,
		__entry->flt_util,
		__entry->pelt_util,
		__entry->pelt_margin,
		__entry->freq)
);

TRACE_EVENT(sugov_ext_turn_point_margin,
	TP_PROTO(unsigned int gear_id, unsigned int orig_util, unsigned int margin_util,
	unsigned int turn_point, unsigned int target_margin),
	TP_ARGS(gear_id, orig_util, margin_util, turn_point, target_margin),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, orig_util)
		__field(unsigned int, margin_util)
		__field(unsigned int, turn_point)
		__field(unsigned int, target_margin)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->orig_util = orig_util;
		__entry->margin_util = margin_util;
		__entry->turn_point = turn_point;
		__entry->target_margin = target_margin;
	),
	TP_printk(
		"gear_id=%u orig_util=%u margin_util=%u turn_point=%d target_margin=%d",
		__entry->gear_id,
		__entry->orig_util,
		__entry->margin_util,
		__entry->turn_point,
		__entry->target_margin)
);
#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH sugov
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sugov_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
