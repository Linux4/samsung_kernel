/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ged

#if !defined(_TRACE_GED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_GED_H

#include <linux/tracepoint.h>

/* common tracepoints */
TRACE_EVENT(tracing_mark_write,

	TP_PROTO(int pid, const char *name, long long value),

	TP_ARGS(pid, name, value),

	TP_STRUCT__entry(
		__field(int, pid)
		__string(name, name)
		__field(long long, value)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__assign_str(name, name);
		__entry->value = value;
	),

	TP_printk("C|%d|%s|%lld", __entry->pid, __get_str(name), __entry->value)
);

TRACE_EVENT(GPU_DVFS__Frequency,
	TP_PROTO(unsigned int virtual_stack, unsigned int real_stack, unsigned int real_top),
	TP_ARGS(virtual_stack, real_stack, real_top),
	TP_STRUCT__entry(
		__field(unsigned int, virtual_stack)
		__field(unsigned int, real_stack)
		__field(unsigned int, real_top)
	),
	TP_fast_assign(
		__entry->virtual_stack = virtual_stack;
		__entry->real_stack = real_stack;
		__entry->real_top = real_top;
	),
	TP_printk("virtual_stack=%u, real_stack=%u, real_top=%u",
		__entry->virtual_stack, __entry->real_stack, __entry->real_top)
);

TRACE_EVENT(GPU_DVFS__Loading,
	TP_PROTO(unsigned int active, unsigned int tiler, unsigned int frag,
		unsigned int comp, unsigned int iter, unsigned int mcu),

	TP_ARGS(active, tiler, frag, comp, iter, mcu),

	TP_STRUCT__entry(
		__field(unsigned int, active)
		__field(unsigned int, tiler)
		__field(unsigned int, frag)
		__field(unsigned int, comp)
		__field(unsigned int, iter)
		__field(unsigned int, mcu)
	),

	TP_fast_assign(
		__entry->active = active;
		__entry->tiler = tiler;
		__entry->frag = frag;
		__entry->comp = comp;
		__entry->iter = iter;
		__entry->mcu = mcu;
	),

	TP_printk("active=%u, tiler=%u, frag=%u, comp=%u, iter=%u, mcu=%u",
			__entry->active, __entry->tiler, __entry->frag, __entry->comp,
			__entry->iter, __entry->mcu)
);

TRACE_EVENT(GPU_DVFS__Policy__Common,

	TP_PROTO(unsigned int commit_type, unsigned int policy_state, unsigned int commit_reason),

	TP_ARGS(commit_type, policy_state, commit_reason),

	TP_STRUCT__entry(
		__field(unsigned int, commit_type)
		__field(unsigned int, policy_state)
		__field(unsigned int, commit_reason)
	),

	TP_fast_assign(
		__entry->commit_type = commit_type;
		__entry->policy_state = policy_state;
		__entry->commit_reason = commit_reason;
	),

	TP_printk("commit_type=%u, policy_state=%u, commit_reason=%u",
		__entry->commit_type, __entry->policy_state, __entry->commit_reason)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Commit_Reason,

	TP_PROTO(unsigned int same, unsigned int diff),

	TP_ARGS(same, diff),

	TP_STRUCT__entry(
		__field(unsigned int, same)
		__field(unsigned int, diff)
	),

	TP_fast_assign(
		__entry->same = same;
		__entry->diff = diff;
	),

	TP_printk("same=%u, diff=%u", __entry->same, __entry->diff)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Commit_Reason__TID,

	TP_PROTO(int pid, int bqid, int count),

	TP_ARGS(pid, bqid, count),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, bqid)
		__field(int, count)
	),

	TP_fast_assign(
		__entry->pid = pid;
		__entry->bqid = bqid;
		__entry->count = count;
	),

	TP_printk("%d_%d=%d", __entry->pid, __entry->bqid, __entry->count)
);

TRACE_EVENT(GPU_DVFS__Policy__Common__Sync_Api,

	TP_PROTO(int hint),

	TP_ARGS(hint),

	TP_STRUCT__entry(
		__field(int, hint)
	),

	TP_fast_assign(
		__entry->hint = hint;
	),

	TP_printk("hint=%d", __entry->hint)
);

/* frame-based policy tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Frequency,

	TP_PROTO(int target, int floor, int target_opp),

	TP_ARGS(target, floor, target_opp),

	TP_STRUCT__entry(
		__field(int, target)
		__field(int, floor)
		__field(int, target_opp)
	),

	TP_fast_assign(
		__entry->target = target;
		__entry->floor = floor;
		__entry->target_opp = target_opp;
	),

	TP_printk("target=%d, floor=%d, target_opp=%d",
		__entry->target, __entry->floor, __entry->target_opp)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Workload,

	TP_PROTO(int cur, int avg, int real, int pipe, unsigned int mode),

	TP_ARGS(cur, avg, real, pipe, mode),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, avg)
		__field(int, real)
		__field(int, pipe)
		__field(unsigned int, mode)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->avg = avg;
		__entry->real = real;
		__entry->pipe = pipe;
		__entry->mode = mode;
	),

	TP_printk("cur=%d, avg=%d, real=%d, pipe=%d, mode=%u", __entry->cur,
		__entry->avg, __entry->real, __entry->pipe, __entry->mode)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__GPU_Time,

	TP_PROTO(int cur, int target, int target_hd, int real, int pipe),

	TP_ARGS(cur, target, target_hd, real, pipe),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, target)
		__field(int, target_hd)
		__field(int, real)
		__field(int, pipe)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->target = target;
		__entry->target_hd = target_hd;
		__entry->real = real;
		__entry->pipe = pipe;
	),

	TP_printk("cur=%d, target=%d, target_hd=%d, real=%d, pipe=%d",
		__entry->cur, __entry->target, __entry->target_hd, __entry->real,
		__entry->pipe)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Margin,

	TP_PROTO(int ceil, int cur, int floor),

	TP_ARGS(ceil, cur, floor),

	TP_STRUCT__entry(
		__field(int, ceil)
		__field(int, cur)
		__field(int, floor)
	),

	TP_fast_assign(
		__entry->ceil = ceil;
		__entry->cur = cur;
		__entry->floor = floor;
	),

	TP_printk("ceil=%d, cur=%d, floor=%d", __entry->ceil, __entry->cur, __entry->floor)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Margin__Detail,

	TP_PROTO(unsigned int margin_mode, int target_fps_margin,
		int min_margin_inc_step, int min_margin),

	TP_ARGS(margin_mode, target_fps_margin, min_margin_inc_step, min_margin),

	TP_STRUCT__entry(
		__field(unsigned int, margin_mode)
		__field(int, target_fps_margin)
		__field(int, min_margin_inc_step)
		__field(int, min_margin)
	),

	TP_fast_assign(
		__entry->margin_mode = margin_mode;
		__entry->target_fps_margin = target_fps_margin;
		__entry->min_margin_inc_step = min_margin_inc_step;
		__entry->min_margin = min_margin;
	),

	TP_printk("margin_mode=%u, target_fps_margin=%d, min_margin_inc_step=%d, min_margin=%d",
		__entry->margin_mode, __entry->target_fps_margin,
		__entry->min_margin_inc_step, __entry->min_margin)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Async_ratio__Counter,

	TP_PROTO(unsigned int gpu_active, unsigned int iter_active, unsigned int compute_active,
		unsigned int l2_rd_stall, unsigned int irq_active, unsigned int mcu_active),

	TP_ARGS(gpu_active, iter_active, compute_active, l2_rd_stall, irq_active, mcu_active),

	TP_STRUCT__entry(
		__field(unsigned int, gpu_active)
		__field(unsigned int, iter_active)
		__field(unsigned int, compute_active)
		__field(unsigned int, l2_rd_stall)
		__field(unsigned int, irq_active)
		__field(unsigned int, mcu_active)
	),

	TP_fast_assign(
		__entry->gpu_active = gpu_active;
		__entry->iter_active = iter_active;
		__entry->compute_active = compute_active;
		__entry->l2_rd_stall = l2_rd_stall;
		__entry->irq_active = irq_active;
		__entry->mcu_active = mcu_active;
	),

	TP_printk("gpu_active=%u, iter_active=%u, compute_active=%u, l2_rd_stall=%u, irq_active=%u, mcu_active=%u",
		__entry->gpu_active, __entry->iter_active, __entry->compute_active,
		__entry->l2_rd_stall, __entry->irq_active, __entry->mcu_active)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Async_ratio__Index,

	TP_PROTO(int is_decreasing, int async_ratio,
		int perf_improve, int fb_oppidx, int fb_tar_freq, int as_tar_opp),

	TP_ARGS(is_decreasing, async_ratio, perf_improve, fb_oppidx,
			fb_tar_freq, as_tar_opp),

	TP_STRUCT__entry(
		__field(int, is_decreasing)
		__field(int, async_ratio)
		__field(int, perf_improve)
		__field(int, fb_oppidx)
		__field(int, fb_tar_freq)
		__field(int, as_tar_opp)
	),

	TP_fast_assign(
		__entry->is_decreasing = is_decreasing;
		__entry->async_ratio = async_ratio;
		__entry->perf_improve = perf_improve;
		__entry->fb_oppidx = fb_oppidx;
		__entry->fb_tar_freq = fb_tar_freq;
		__entry->as_tar_opp = as_tar_opp;
	),

	TP_printk("is_decreasing=%d, async_ratio=%d, perf_improve=%d, fb_oppidx=%d, fb_tar_freq=%d, as_tar_opp=%d",
		__entry->is_decreasing, __entry->async_ratio,
		__entry->perf_improve, __entry->fb_oppidx,
		__entry->fb_tar_freq, __entry->as_tar_opp)
);

TRACE_EVENT(GPU_DVFS__Policy__Frame_based__Async_ratio__Policy,

	TP_PROTO(int cur_opp_id, int fb_oppidx,
		int async_id, int apply_async, int is_decreasing),

	TP_ARGS(cur_opp_id, fb_oppidx, async_id, apply_async,
			is_decreasing),

	TP_STRUCT__entry(
		__field(int, cur_opp_id)
		__field(int, fb_oppidx)
		__field(int, async_id)
		__field(int, apply_async)
		__field(int, is_decreasing)
	),

	TP_fast_assign(
		__entry->cur_opp_id = cur_opp_id;
		__entry->fb_oppidx = fb_oppidx;
		__entry->async_id = async_id;
		__entry->apply_async = apply_async;
		__entry->is_decreasing = is_decreasing;
	),

	TP_printk("cur_opp_id=%d, fb_oppidx=%d, async_id=%d, apply_async=%d, is_decreasing=%d",
		__entry->cur_opp_id, __entry->fb_oppidx,
		__entry->async_id, __entry->apply_async,
		__entry->is_decreasing)
);

/* loading-based policy tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Opp,

	TP_PROTO(int target),

	TP_ARGS(target),

	TP_STRUCT__entry(
		__field(int, target)
	),

	TP_fast_assign(
		__entry->target = target;
	),

	TP_printk("target=%d", __entry->target)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Loading,

	TP_PROTO(unsigned int cur, unsigned int mode, unsigned int fb_adj),

	TP_ARGS(cur, mode, fb_adj),

	TP_STRUCT__entry(
		__field(unsigned int, cur)
		__field(unsigned int, mode)
		__field(unsigned int, fb_adj)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->mode = mode;
		__entry->fb_adj = fb_adj;
	),

	TP_printk("cur=%u, mode=%u, fb_adj=%u", __entry->cur, __entry->mode, __entry->fb_adj)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Bound,

	TP_PROTO(int ultra_high, int high, int low, int ultra_low),

	TP_ARGS(ultra_high, high, low, ultra_low),

	TP_STRUCT__entry(
		__field(int, ultra_high)
		__field(int, high)
		__field(int, low)
		__field(int, ultra_low)
	),

	TP_fast_assign(
		__entry->ultra_high = ultra_high;
		__entry->high = high;
		__entry->low = low;
		__entry->ultra_low = ultra_low;
	),

	TP_printk("ultra_high=%d, high=%d, low=%d, ultra_low=%d",
		__entry->ultra_high, __entry->high, __entry->low, __entry->ultra_low)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__GPU_Time,

	TP_PROTO(int cur, int target, int target_hd, int complete, int uncomplete),

	TP_ARGS(cur, target, target_hd, complete, uncomplete),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, target)
		__field(int, target_hd)
		__field(int, complete)
		__field(int, uncomplete)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->target = target;
		__entry->target_hd = target_hd;
		__entry->complete = complete;
		__entry->uncomplete = uncomplete;
	),

	TP_printk("cur=%d, target=%d, target_hd=%d, complete=%d, uncomplete=%d",
		__entry->cur, __entry->target, __entry->target_hd, __entry->complete,
		__entry->uncomplete)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Margin,

	TP_PROTO(int ceil, int cur, int floor),

	TP_ARGS(ceil, cur, floor),

	TP_STRUCT__entry(
		__field(int, ceil)
		__field(int, cur)
		__field(int, floor)
	),

	TP_fast_assign(
		__entry->ceil = ceil;
		__entry->cur = cur;
		__entry->floor = floor;
	),

	TP_printk("ceil=%d, cur=%d, floor=%d", __entry->ceil, __entry->cur, __entry->floor)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Margin__Detail,

	TP_PROTO(unsigned int margin_mode, int margin_step, int min_margin),

	TP_ARGS(margin_mode, margin_step, min_margin),

	TP_STRUCT__entry(
		__field(unsigned int, margin_mode)
		__field(int, margin_step)
		__field(int, min_margin)
	),

	TP_fast_assign(
		__entry->margin_mode = margin_mode;
		__entry->margin_step = margin_step;
		__entry->min_margin = min_margin;
	),

	TP_printk("margin_mode=%u, margin_step=%d, min_margin=%d",
		__entry->margin_mode, __entry->margin_step,
		__entry->min_margin)
);

TRACE_EVENT(GPU_DVFS__Policy__Loading_based__Fallback_Tuning,

	TP_PROTO(int fallback_tuning, int fallback_idle, int uncomplete_type,
			int uncomplete_flag, int lb_last_opp),

	TP_ARGS(fallback_tuning, fallback_idle, uncomplete_type, uncomplete_flag, lb_last_opp),

	TP_STRUCT__entry(
		__field(int, fallback_tuning)
		__field(int, fallback_idle)
		__field(int, uncomplete_type)
		__field(int, uncomplete_flag)
		__field(int, lb_last_opp)
	),

	TP_fast_assign(
		__entry->fallback_tuning = fallback_tuning;
		__entry->fallback_idle = fallback_idle;
		__entry->uncomplete_type = uncomplete_type;
		__entry->uncomplete_flag = uncomplete_flag;
		__entry->lb_last_opp = lb_last_opp;
	),

	TP_printk("fallback_tuning=%d, fallback_idle=%d, uncomplete_type=%d, uncomplete_flag=%d, lb_last_opp=%d",
		__entry->fallback_tuning, __entry->fallback_idle,
		__entry->uncomplete_type, __entry->uncomplete_flag, __entry->lb_last_opp)
);

/* DCS tracepoints */
TRACE_EVENT(GPU_DVFS__Policy__DCS,

	TP_PROTO(int max_core, int current_core, unsigned int fix_core),

	TP_ARGS(max_core, current_core, fix_core),

	TP_STRUCT__entry(
		__field(int, max_core)
		__field(int, current_core)
		__field(int, fix_core)
	),

	TP_fast_assign(
		__entry->max_core = max_core;
		__entry->current_core = current_core;
		__entry->fix_core = fix_core;
	),

	TP_printk("max_core=%d, current_core=%d fix_core=%u",
	__entry->max_core, __entry->current_core, __entry->fix_core)
);

TRACE_EVENT(GPU_DVFS__Policy__DCS__Detail,

	TP_PROTO(unsigned int core_mask),

	TP_ARGS(core_mask),

	TP_STRUCT__entry(
		__field(unsigned int, core_mask)
	),

	TP_fast_assign(
		__entry->core_mask = core_mask;
	),

	TP_printk("core_mask=%u", __entry->core_mask)
);

#endif /* _TRACE_GED_H */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/mediatek/ged/include
#define TRACE_INCLUDE_FILE ged_tracepoint
#include <trace/define_trace.h>
