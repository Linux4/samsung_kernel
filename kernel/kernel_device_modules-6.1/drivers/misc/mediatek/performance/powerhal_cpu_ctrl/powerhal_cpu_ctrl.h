/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef POWERHAL_CPU_CTRL_H
#define POWERHAL_CPU_CTRL_H

#include "ioctl_powerhal.h"

#define SESSION_UNUSED     0
#define SESSION_USED       1

extern int (*powerhal_adpf_create_session_hint_fp)
	(unsigned int sid, unsigned int tgid,
	unsigned int uid, int *threadIds,
	int threadIds_size, long durationNanos);
extern int (*powerhal_adpf_get_hint_session_preferred_rate_fp)(long long *preferredRate);
extern int (*powerhal_adpf_update_work_duration_fp)(unsigned int sid, long targetDurationNanos);
extern int (*powerhal_adpf_report_actual_work_duration_fp)
	(unsigned int sid,
	struct _ADPF_WORK_DURATION *workDuration,
	int work_duration_size);
extern int (*powerhal_adpf_pause_fp)(unsigned int sid);
extern int (*powerhal_adpf_resume_fp)(unsigned int sid);
extern int (*powerhal_adpf_close_fp)(unsigned int sid);
extern int (*powerhal_adpf_sent_hint_fp)(unsigned int sid, int hint);
extern int (*powerhal_adpf_set_threads_fp)(unsigned int sid, int *threadIds, int threadIds_size);
extern int (*powerhal_dsu_sport_mode_fp)(unsigned int mode);

enum _SESSION_HINT {
	CPU_LOAD_UP = 0,
	CPU_LOAD_DOWN,
	CPU_LOAD_RESET,
	CPU_LOAD_RESUME,
	POWER_EFFICIENCY
};

struct _SESSION_WORK_DURATION {
	long timeStampNanos;
	long durationNanos;
};

struct _SESSION {
	unsigned int cmd;
	unsigned int sid;
	unsigned int tgid;
	unsigned int uid;
	int threadIds[ADPF_MAX_THREAD];
	int threadIds_size;
	struct _SESSION_WORK_DURATION *workDuration[ADPF_MAX_THREAD];
	int work_duration_size;
	int used;

	union {
		int hint;
		long preferredRate;
		long durationNanos;
		long targetDurationNanos;
	};
};

typedef int (*adpfCallback)(struct _SESSION *);
extern void (*boost_get_cmd_fp)(int *cmd, int *value);
extern int adpf_register_callback(adpfCallback callback);
extern int adpf_unregister_callback(int idx);

#endif
