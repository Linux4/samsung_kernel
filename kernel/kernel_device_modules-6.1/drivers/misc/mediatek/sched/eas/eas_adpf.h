/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#ifndef _EAS_ADPF_H
#define _EAS_ADPF_H

#define ADPF_MAX_THREAD    32
#define SESSION_UNUSED     0
#define SESSION_USED       1

enum _ADPF_CMD {
	ADPF_CREATE_HINT_SESSION = 1,
	ADPF_GET_HINT_SESSION_PREFERED_RATE,
	ADPF_UPDATE_TARGET_WORK_DURATION,
	ADPF_REPORT_ACTUAL_WORK_DURATION,
	ADPF_PAUSE,
	ADPF_RESUME,
	ADPF_CLOSE,
	ADPF_SENT_HINT,
	ADPF_SET_THREADS,
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
extern void set_task_basic_vip(int pid);
extern int adpf_register_callback(adpfCallback callback);
extern int sched_adpf_callback(struct _SESSION *session);
extern void set_group_active_ratio_cap(int gear_id, int val);
extern void set_eas_adpf_enable(int val);
extern int get_eas_adpf_enable(void);
#endif

