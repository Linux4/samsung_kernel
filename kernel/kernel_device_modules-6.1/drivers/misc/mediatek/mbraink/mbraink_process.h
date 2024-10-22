/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_PROCESS_H
#define MBRAINK_PROCESS_H
#include <linux/string_helpers.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/mm_types.h>
#include <linux/pid.h>

#include "mbraink_ioctl_struct_def.h"

#define MAX_RT_PRIO			100
#define MAX_TRACE_NUM			3072
#define MAX_BINDER_TRACE_NUM	2048

struct mbraink_monitor_pidlist {
	unsigned short is_set;
	unsigned short monitor_process_count;
	unsigned short monitor_pid[MAX_MONITOR_PROCESS_NUM];
};

struct mbraink_tracing_pidlist {
	unsigned short pid;
	unsigned short tgid;
	unsigned short uid;
	int priority;
	char name[TASK_COMM_LEN];
	long long start;
	long long end;
	u64 jiffies;
	bool dirty;
};

struct mbraink_binder_tracelist {
	unsigned short from_pid;
	unsigned short from_tid;
	unsigned short to_pid;
	unsigned int count;
	bool dirty;
};

extern int mbraink_netlink_send_msg(const char *msg); //EXPORT_SYMBOL_GPL

void mbraink_show_process_info(void);
void mbraink_get_process_stat_info(pid_t current_pid,
			struct mbraink_process_stat_data *process_stat_buffer);
void mbraink_get_thread_stat_info(pid_t current_pid_idx, pid_t current_tid,
			struct mbraink_thread_stat_data *thread_stat_buffer);
void mbraink_processname_to_pid(unsigned short monitor_process_count,
				const struct mbraink_monitor_processlist *processname_inputlist,
				bool is_binder);
void mbraink_get_process_memory_info(pid_t current_pid, unsigned int cnt,
			struct mbraink_process_memory_data *process_memory_buffer);
int mbraink_process_tracer_init(void);
void mbraink_process_tracer_exit(void);
void mbraink_get_tracing_pid_info(unsigned short current_idx,
			struct mbraink_tracing_pid_data *tracing_pid_buffer);
void mbraink_get_binder_trace_info(unsigned short current_idx,
				struct mbraink_binder_trace_data *binder_trace_buffer);
char *kstrdup_quotable_cmdline(struct task_struct *task, gfp_t gfp);
void task_cputime_adjusted(struct task_struct *p, u64 *ut, u64 *st);
void thread_group_cputime_adjusted(struct task_struct *p, u64 *ut, u64 *st);
u64 nsec_to_clock_t(u64 x);
#endif
