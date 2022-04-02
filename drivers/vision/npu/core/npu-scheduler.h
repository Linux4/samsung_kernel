/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SCHEDULER_H_
#define _NPU_SCHEDULER_H_

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/pm_qos.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/wakelock.h>

#include "npu-vs4l.h"
#include "npu-common.h"
#include "npu-session.h"
#include "npu-util-common.h"
#include "npu-util-autosleepthr.h"

static char *npu_scheduler_ip_name[] = {
	"CL0",
	"CL1",
	"CL2",
	"MIF",
	"INT",
	"DNC",
	"NPU",
};

static char *npu_scheduler_core_name[] = {
	"DNC",
	"NPU",
};

static int npu_scheduler_ip_pmqos_min[] = {
	PM_QOS_CLUSTER0_FREQ_MIN,
	PM_QOS_CLUSTER1_FREQ_MIN,
	PM_QOS_CLUSTER2_FREQ_MIN,
	PM_QOS_BUS_THROUGHPUT,
	PM_QOS_DEVICE_THROUGHPUT,
	PM_QOS_DNC_THROUGHPUT,
	PM_QOS_NPU_THROUGHPUT,
};

static int npu_scheduler_ip_pmqos_max[] = {
	PM_QOS_CLUSTER0_FREQ_MAX,
	PM_QOS_CLUSTER1_FREQ_MAX,
	PM_QOS_CLUSTER2_FREQ_MAX,
	PM_QOS_BUS_THROUGHPUT_MAX,
	PM_QOS_DEVICE_THROUGHPUT_MAX,
	PM_QOS_DNC_THROUGHPUT_MAX,
	PM_QOS_NPU_THROUGHPUT_MAX,
};

static inline int get_pm_qos_num(char *name, char *name_list[],
		int name_list_num, int pmqos_list[])
{
	int i;

	for (i = 0; i < name_list_num; i++) {
		if (!strcmp(name, name_list[i]))
			return pmqos_list[i];
	}
	return -1;
}

#define get_pm_qos_max(NAME)	\
	get_pm_qos_num(NAME, npu_scheduler_ip_name,	\
	ARRAY_SIZE(npu_scheduler_ip_name), npu_scheduler_ip_pmqos_max)
#define get_pm_qos_min(NAME)	\
	get_pm_qos_num(NAME, npu_scheduler_ip_name,	\
	ARRAY_SIZE(npu_scheduler_ip_name), npu_scheduler_ip_pmqos_min)

/*
 * time domain : us
 */

//#define CONFIG_NPU_SCHEDULER_OPEN_CLOSE
#define CONFIG_NPU_SCHEDULER_START_STOP

#define NPU_SCHEDULER_NAME	"npu-scheduler"
#define NPU_SCHEDULER_DEFAULT_PERIOD	17	/* msec */
#define NPU_SCHEDULER_DEFAULT_TPF	16667
#define NPU_SCHEDULER_FPS_LOAD_RESET_FRAME_NUM	3
#define NPU_SCHEDULER_BOOST_TIMEOUT	20 /* msec */

static char *npu_perf_mode_name[] = {
	"none",
	"normal",
	"npu boost",
	"cpu boost",
	"npu DN",
};

enum {
	/* no minlock argument of NPU_PERF_MODE_NONE in dt */
	NPU_PERF_MODE_NONE = 0,
	NPU_PERF_MODE_NORMAL,
	NPU_PERF_MODE_NPU_BOOST,
	NPU_PERF_MODE_CPU_BOOST,
	NPU_PERF_MODE_NPU_DN,
	NPU_PERF_MODE_NUM,
};

#define NPU_SCHEDULER_DVFS_ARG_NUM	10
/* total arg num should be less than arg limit 16 */
#define NPU_SCHEDULER_DVFS_TOTAL_ARG_NUM	\
	(NPU_SCHEDULER_DVFS_ARG_NUM + NPU_PERF_MODE_NUM - 1)

static char *npu_scheduler_load_policy_name[] = {
	"idle",
	"fps",
	"rq",
	"fps-rq hybrid",	// use fps load for DVFS, use rq load for hi/low speed freq
};

enum {
	NPU_SCHEDULER_LOAD_IDLE = 0,
	NPU_SCHEDULER_LOAD_FPS,
	NPU_SCHEDULER_LOAD_RQ,
	NPU_SCHEDULER_LOAD_FPS_RQ,
};

static char *npu_scheduler_fps_policy_name[] = {
	"min",
	"max",
	"average",
	"average without min, max",
};

enum {
	NPU_SCHEDULER_FPS_MIN = 0,
	NPU_SCHEDULER_FPS_MAX,
	NPU_SCHEDULER_FPS_AVG,
	NPU_SCHEDULER_FPS_AVG2,		/* average without min, max */
};

struct npu_scheduler_fps_frame {
	npu_uid_t	uid;
	u32		frame_id;
	s64		start_time;

	struct list_head	list;
};

#define NPU_SCHEDULER_PRIORITY_MIN	0
#define NPU_SCHEDULER_PRIORITY_MAX	255

struct npu_scheduler_fps_load {
	npu_uid_t	uid;
	u64		time_stamp;
	u32		priority;
	u32		bound_id;
	u32		frame_count;
	u32		tfc;		/* temperary frame count */
	s64		tpf;		/* time per frame */
	s64		requested_tpf;	/* tpf for fps */
	unsigned int	fps_load;	/* 0.01 unit */
	s64		init_freq_ratio;

	struct list_head	list;
};

struct npu_scheduler_dvfs_info {
	char			*name;
	struct platform_device	*dvfs_dev;
	u32			activated;
	struct pm_qos_request	qos_req_min;
	struct pm_qos_request	qos_req_max;
	struct pm_qos_request	qos_req_min_nw_boost;
	s32			cur_freq;
	s32			min_freq;
	s32			max_freq;
	s32			delay;
	s32			limit_min;
	s32			limit_max;
	s32			is_init_freq;
	struct npu_scheduler_governor *gov;
	void			*gov_prop;
	u32			mode_min_freq[NPU_PERF_MODE_NUM];
	s32 curr_up_delay;
	s32 curr_down_delay;

	struct list_head	dev_list;	/* list to governor */
	struct list_head	ip_list;	/* list to scheduler */
};

#define NPU_SCHEDULER_LOAD_WIN_MAX	10
#define NPU_SCHEDULER_DEFAULT_LOAD_WIN_SIZE	1
#define NPU_SCHEDULER_FREQ_INTERVAL	4

struct npu_scheduler_governor;
struct npu_scheduler_info {
	struct device	*dev;
	struct npu_device *device;

	u32		enable;
	u32		activated;
	u32		mode;
	int		bts_scenindex;

	u64		time_stamp;
	u64		time_diff;
	u32		freq_interval;
	u32		tfi;
	u32		period;

/* gating information */
	u32		used_count;

/* load information */
	u32		load_policy;
	u32		load_window_index;
	u32		load_window_size;
	u32		load_window[NPU_SCHEDULER_LOAD_WIN_MAX];	/* 0.01 unit */
	unsigned int	load;		/* 0.01 unit */
	u64		load_idle_time;

/* idle-based load calculation */
	unsigned int	idle_load;	/* 0.01 unit */

/* FPS-based load calculation */
	struct mutex	fps_lock;
	u32		fps_policy;
	struct list_head fps_frame_list;
	struct list_head fps_load_list;
	unsigned int	fps_load;	/* 0.01 unit */
	u32		tpf_others;

/* RQ-based load calculation */
	struct mutex	rq_lock;
	s64		rq_start_time;
	s64		rq_idle_start_time;
	s64		rq_idle_time;
	unsigned int	rq_load;	/* 0.01 unit */

/* governer information */
	struct list_head gov_list;	/* governor list */
/* IP dvfs information */
	struct list_head ip_list;	/* device list */

	struct mutex	exec_lock;
	struct wake_lock		sched_wake_lock;
	struct workqueue_struct		*sched_wq;
	struct delayed_work		sched_work;
	struct delayed_work		boost_off_work;
	u32		llc_status;

/* Frequency boost information only for open() and ioctl(S_FORMAT) */
	int		boost_count;
};

#define NPU_SET_ATTR(_name, _category)					\
{									\
	.attr = {.name = #_name, .mode = 0664},				\
	.show = npu_show_attrs_##_category,				\
	.store = npu_store_attrs_##_category,				\
}

#define NPU_SCHEDULER_ATTR(_name)	NPU_SET_ATTR(_name, scheduler)

struct npu_device;

int npu_scheduler_probe(struct npu_device *device);
int npu_scheduler_release(struct npu_device *device);
int npu_scheduler_open(struct npu_device *device);
int npu_scheduler_close(struct npu_device *device);
int npu_scheduler_resume(struct npu_device *device);
int npu_scheduler_suspend(struct npu_device *device);
int npu_scheduler_start(struct npu_device *device);
int npu_scheduler_stop(struct npu_device *device);
int npu_scheduler_load(struct npu_device *device, const struct npu_session *session);
void npu_scheduler_unload(struct npu_device *device, const struct npu_session *session);
void npu_scheduler_update_sched_param(struct npu_device *device, struct npu_session *session);
void npu_scheduler_gate(struct npu_device *device, struct npu_frame *frame, bool idle);
void npu_scheduler_fps_update_idle(struct npu_device *device, struct npu_frame *frame, bool idle);
void npu_scheduler_set_init_freq(struct npu_device *device, npu_uid_t session_uid);
void npu_scheduler_rq_update_idle(struct npu_device *device, bool idle);
void npu_pm_qos_update_request(struct npu_scheduler_dvfs_info *d,
		struct pm_qos_request *req, s32 new_value);
npu_s_param_ret npu_scheduler_param_handler(struct npu_session *sess,
	struct vs4l_param *param, int *retval);
void npu_scheduler_activate_peripheral_dvfs(unsigned long freq);
struct npu_scheduler_info *npu_scheduler_get_info(void);
int npu_scheduler_boost_on(struct npu_scheduler_info *info);
int npu_scheduler_boost_off(struct npu_scheduler_info *info);
int npu_scheduler_boost_off_timeout(struct npu_scheduler_info *info, s64 timeout);
int npu_scheduler_enable(struct npu_scheduler_info *info);
int npu_scheduler_disable(struct npu_scheduler_info *info);

#endif
