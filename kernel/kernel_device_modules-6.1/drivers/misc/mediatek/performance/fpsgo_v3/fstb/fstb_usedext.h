/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef FSTB_USEDEXT_H
#define FSTB_USEDEXT_H

#include <mt-plat/fpsgo_common.h>
#include <linux/list.h>
#include <linux/sched.h>

#define DEFAULT_DFPS 60
#define CFG_MAX_FPS_LIMIT	240
#define CFG_MIN_FPS_LIMIT	1
#define FRAME_TIME_BUFFER_SIZE 200
#define MAX_NR_RENDER_FPS_LEVELS	10
#define DEFAULT_RESET_TOLERENCE 3
#define FSTB_IDLE_DBNC 10
#define MAX_FSTB_POLICY_CMD_NUM 10
#define MAX_INFO_CALLBACK 5

extern void (*ged_kpi_output_gfx_info2_fp)(long long t_gpu,
	unsigned int cur_freq, unsigned int cur_max_freq, u64 ulID);

struct FSTB_FRAME_INFO {
	struct hlist_node hlist;

	int pid;
	int proc_id;
	char proc_name[16];
	int target_fps;
	int target_fps_v2;
	int target_fps_margin_v2;
	int target_fps_margin;
	int target_fps_margin_dbnc_a;
	int target_fps_margin_dbnc_b;
	int queue_fps;
	unsigned long long bufid;
	int target_fps_diff;
	int target_fps_notifying;
	int sbe_state; /*0: free run, 1: max_fps*/
	unsigned long long target_time;

	long long cpu_time;
	long long gpu_time;
	int gpu_freq;

	unsigned long long queue_time_ts[FRAME_TIME_BUFFER_SIZE]; /*timestamp*/
	int queue_time_begin;
	int queue_time_end;
	unsigned long long weighted_cpu_time[FRAME_TIME_BUFFER_SIZE];
	unsigned long long weighted_cpu_time_ts[FRAME_TIME_BUFFER_SIZE];
	unsigned long long weighted_gpu_time[FRAME_TIME_BUFFER_SIZE];
	unsigned long long weighted_gpu_time_ts[FRAME_TIME_BUFFER_SIZE];
	int weighted_cpu_time_begin;
	int weighted_cpu_time_end;
	int weighted_gpu_time_begin;
	int weighted_gpu_time_end;
	unsigned long long sorted_weighted_cpu_time[FRAME_TIME_BUFFER_SIZE];
	unsigned long long sorted_weighted_gpu_time[FRAME_TIME_BUFFER_SIZE];
	int quantile_cpu_time;
	int quantile_gpu_time;

	int render_idle_cnt;
	int hwui_flag;
	int self_ctrl_fps_enable;
	int notify_target_fps;
	int magt_target_fps;
	int vsync_app_fps_disable;

	unsigned long master_type;
};

struct FSTB_RENDER_TARGET_FPS {
	struct hlist_node hlist;

	char process_name[16];
	int pid;
	int nr_level;
	struct fps_level level[MAX_NR_RENDER_FPS_LEVELS];
};

struct FSTB_POWERFPS_LIST {
	int pid;
	int fps;
};

struct FSTB_NOTIFIER_PUSH_TAG {
	int tgid;
	int pid;
	int target_fps_margin;
	int target_fps_hint;
	int eara_is_active;
	unsigned long long bufid;
	unsigned long long cur_queue_end_ts;

	struct work_struct sWork;
};

struct FSTB_POLICY_CMD {
	struct rb_node rb_node;

	int tgid;
	int self_ctrl_fps_enable;
	int notify_target_fps;
	int vsync_app_fps_disable;
	unsigned long long ts;
};

#endif
