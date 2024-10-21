/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _FLT_H
#define _FLT_H

DECLARE_PER_CPU(struct flt_rq, flt_rq);

enum task_event {
	PUT_PREV_TASK		= 0,
	PICK_NEXT_TASK	= 1,
	TASK_WAKE		= 2,
	TASK_MIGRATE		= 3,
	TASK_UPDATE		= 4,
	IRQ_UPDATE		= 5,
};

enum win_event {
	WITHIN_WIN		= 0,
	SPAN_WIN_PREV		= 1,
	SPAN_WIN_CURR	= 2,
};

enum cpu_update_event {
	CPU_WITHIN_WIN		= 0,
	NOT_CURRENT_TASK		= 1,
	CURRENT_TASK			= 2,
	CPU_IRQ_UPDATE		= 3,
};

enum history_event {
	NEW_WIN_UPDATE	= 0,
	SPAN_WIN_ONE		= 1,
	SPAN_WIN_MULTI	= 2,
};

struct flt_rq {
	u64			window_start;
	u32			prev_window_size;
	u64			curr_runnable_sum;
	u64			prev_runnable_sum;
	u64			nt_curr_runnable_sum;
	u64			nt_prev_runnable_sum;
	u32			sum_history[RAVG_HIST_SIZE_MAX];
	u32			util_history[RAVG_HIST_SIZE_MAX];
	int			window_size;
	int			weight_policy;
	int			window_count;
	u64			group_curr_sum[GROUP_ID_RECORD_MAX];
	u64			group_prev_sum[GROUP_ID_RECORD_MAX];
	u32			group_sum_history[RAVG_HIST_SIZE_MAX][GROUP_ID_RECORD_MAX];
	u32			group_util_history[RAVG_HIST_SIZE_MAX][GROUP_ID_RECORD_MAX];
	u32			group_util_active_history[GROUP_ID_RECORD_MAX][RAVG_HIST_SIZE_MAX];
	int			group_nr_running[GROUP_ID_RECORD_MAX];
	u32			group_util_ratio[GROUP_ID_RECORD_MAX];
	u32			group_raw_util_ratio[GROUP_ID_RECORD_MAX];
	u32			group_util_rtratio[GROUP_ID_RECORD_MAX];
	u32			cpu_dmand_util;
	u32			cpu_tar_util;
};

void flt_cal_init(void);
void sched_window_nr_ticks_change(void);
int flt_get_grp_hint_mode1(int grp_id);
void flt_rvh_enqueue_task(void *data, struct rq *rq,
				struct task_struct *p, int flags);
void flt_rvh_dequeue_task(void *data, struct rq *rq,
				struct task_struct *p, int flags);
void flt_set_grp_ctrl(int set);
#endif /* _FLT_H */
