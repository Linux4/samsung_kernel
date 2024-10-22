/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _EAS_GROUP_H
#define _EAS_GROUP_H

#define DEFAULT_SCHED_RAVG_WINDOW 4000000
#define GROUP_RAVG_HIST_SIZE_MAX (5)
#define FLT_GROUP_START_IDX 6
#define TA_GRPID 3

#define mts_to_ts(mts) ({ \
		void *__mptr = (void *)(mts); \
		((struct task_struct *)(__mptr - \
			offsetof(struct task_struct, android_vendor_data1))); })

/* gp mode */
enum _GP_mode {
	GP_MODE_0 = 0,
	GP_MODE_1 = 1,
	GP_MODE_2 = 2,
	GP_MODE_NUM,
};

/* gp trace mode */
enum _GP_trace {
	GP_CGROUP,
	GP_TTWU,
	GP_WAUPNEW,
	GP_DEAD,
	GP_API,
	GP_TRACE_NUM,
};

/* gp id */
enum _GP_ID {
	GROUP_ID_1 = 0,
	GROUP_ID_2 = 1,
	GROUP_ID_3 = 2,
	GROUP_ID_4 = 3,
	GROUP_ID_RECORD_MAX
};

enum _wp {
	WP_MODE_0 = 0,
	WP_MODE_1 = 1,
	WP_MODE_2 = 2,
	WP_MODE_3 = 3,
	WP_MODE_4 = 4,
	WP_MODE_NUM,
};

struct grp {
	int			id;
	raw_spinlock_t		lock;
	struct list_head		tasks;
	struct list_head		list;
	struct rcu_head		rcu;
	int			ws;
	int			wp;
	int			wc;
	bool			gear_hint;
};

struct gas_ctrl {
	int val;
	int force_ctrl;
};

struct gas_thr {
	int grp_id;
	int val;
};

#define GRP_DEFAULT_WS DEFAULT_SCHED_RAVG_WINDOW
#define GRP_DEFAULT_WC GROUP_RAVG_HIST_SIZE_MAX
#define GRP_DEFAULT_WP WP_MODE_4

/* pd setting related API*/
extern int set_task_pd(int pid, int pd);
extern int set_group_pd(int group_id, int pd);

void group_init(void);
void group_exit(void);
void  group_set_mode(u32 mode);
u32 group_get_mode(void);
int __sched_set_grp_id(struct task_struct *p, int group_id);
inline struct grp *lookup_grp(int grp_id);
inline struct grp *task_grp(struct task_struct *p);
int get_grp_id(struct task_struct *p);
int __set_task_to_group(int pid, int grp_id);
int set_task_to_group(int pid, int grp_id);
inline bool check_and_get_grp_id(struct task_struct *p, int *grp_id);
void group_update_ws(struct rq *rq);
inline int cgrp_to_grpid(struct task_struct *p);
int user_set_task_to_grp(int pid, int grp_id);
void  group_update_threshold_util(int wl);
int  group_get_threshold_util(int grp_id);
int  group_set_threshold(int grp_id, int val);
int  group_reset_threshold(int grp_id);
int  group_get_threshold(int grp_id);
inline bool group_get_gear_hint(struct task_struct *p);
#endif /* _EAS_GROUP_H*/
