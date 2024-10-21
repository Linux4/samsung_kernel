/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#define CLUSTER_MAX 10
#define TOUCH_TIMEOUT_MS 100
#define TOUCH_FSTB_ACTIVE_MS 100
#define TOUCH_UP 0
#define TOUCH_DOWN 1


extern void set_top_grp_aware(int val, int force_ctrl);
extern int  group_set_threshold(int grp_id, int val);
extern int group_reset_threshold(int grp_id);

extern int (*fpsgo_get_fstb_active_fp)(long long time_diff);
extern void (*touch_boost_get_cmd_fp)(int *cmd, int *enable,
	int *deboost_when_render, int *active_time, int *boost_duration,
	int *idleprefer_ta, int *idleprefer_fg, int *util_ta, int *util_fg,
	int *cpufreq_c0, int *cpufreq_c1, int *cpufreq_c2, int *boost_up, int *boost_down);
extern int (*fpsgo_wait_fstb_active_fp)(void);
typedef void (*fpsgo_notify_is_boost_cb)(int fpsgo_is_boosting);
extern int (*register_get_fpsgo_is_boosting_fp)(fpsgo_notify_is_boost_cb func_cb);

struct _cpufreq {
	int min;
	int max;
} _cpufreq;

struct boost {
	spinlock_t touch_lock;
	wait_queue_head_t wq;
	struct task_struct *thread;
	struct task_struct *thread_interrupt;
	int touch_event;
	atomic_t event;
};

enum {
	DEBOOST_WEHN_RENDERING = 1,
	DEBOOST_WEHN_FPSGO_BOOST = 2,
};

enum {
	TOUCH_BOOST_UNKNOWN = -1,
	TOUCH_BOOST_CPU = 0,
};
