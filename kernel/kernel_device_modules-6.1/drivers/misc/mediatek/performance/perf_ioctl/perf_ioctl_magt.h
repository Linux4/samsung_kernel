/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef PERF_IOCTL_MAGT_H
#define PERF_IOCTL_MAGT_H
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/cpumask.h>
#include <linux/mutex.h>

#define max_cpus 8
#define MAX_MAGT_TARGET_FPS_NUM  10
#define MAGT_DEP_LIST_NUM  10
#define MAGT_GET_CPU_LOADING              _IOR('r', 0, struct cpu_info)
#define MAGT_GET_PERF_INDEX               _IOR('r', 1, struct cpu_info)
#define MAGT_SET_TARGET_FPS               _IOW('g', 2, struct target_fps_info)
#define MAGT_SET_DEP_LIST                 _IOW('g', 3, struct dep_list_info)

struct cpu_time {
	u64 time;
};

struct cpu_info {
	int cpu_loading[max_cpus];
	int perf_index[3];
};

struct target_fps_info {
	__u32 pid_arr[MAX_MAGT_TARGET_FPS_NUM ];
	__u32 tid_arr[MAX_MAGT_TARGET_FPS_NUM ];
	__u32 tfps_arr[MAX_MAGT_TARGET_FPS_NUM ];
	__u32 num;
};

struct dep_list_info {
	__u32 pid;
	__u32 user_dep_arr[MAGT_DEP_LIST_NUM];
	__u32 user_dep_num;
};

extern int get_cpu_loading(struct cpu_info *_ci);
extern int get_perf_index(struct cpu_info *_ci);
extern u64 get_cpu_idle_time(unsigned int cpu, u64 *wall, int io_busy);

#endif
