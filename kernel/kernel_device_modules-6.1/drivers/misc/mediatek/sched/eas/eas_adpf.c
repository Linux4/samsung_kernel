// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/cgroup.h>
#include "common.h"
#include "vip.h"
#include "eas_trace.h"
#include "eas_plus.h"
#include "group.h"
#include "eas_adpf.h"

#define ADPF_MAX_SESSION 64

static DEFINE_MUTEX(adpf_mutex);
static int eas_adpf_enable = 1;

int sched_adpf_callback(struct _SESSION *session)
{
	unsigned int cmd = session->cmd;
	unsigned int sid = session->sid;
	int i = 0;
	unsigned int adpf_ratio = 0;
	unsigned int durationNanos = 0;
	unsigned int targetDurationNanos = 0;
	bool vip_enable = sched_vip_enable_get();

	if (unlikely(group_get_mode() == GP_MODE_0) && !vip_enable)
		return -1;
	if(!eas_adpf_enable)
		return 0;

	switch(cmd) {
	case ADPF_CREATE_HINT_SESSION:
		if(sid >= ADPF_MAX_SESSION) {
			pr_debug("[%s] sid error: %d cmd: %d", __func__, sid, cmd);
			return -1;
		}
		mutex_lock(&adpf_mutex);
		for(i = 0; i < session->threadIds_size; i++) {
			__set_task_to_group(session->threadIds[i], -1);
			__set_task_to_group(session->threadIds[i], GROUP_ID_2);
			set_task_basic_vip(session->threadIds[i]);
		}
		mutex_unlock(&adpf_mutex);
		break;
	case ADPF_GET_HINT_SESSION_PREFERED_RATE:
		//TODO
		break;
	case ADPF_UPDATE_TARGET_WORK_DURATION:
		//TODO
		break;
	case ADPF_REPORT_ACTUAL_WORK_DURATION:
		if(sid >= ADPF_MAX_SESSION || session->work_duration_size <= 0) {
			pr_debug("[%s] sid error: %d cmd: %d", __func__, sid, cmd);
			return -1;
		}
		mutex_lock(&adpf_mutex);
		durationNanos = session->workDuration[session->work_duration_size - 1]->durationNanos;
		targetDurationNanos = session->targetDurationNanos;
		if(durationNanos > 0 && targetDurationNanos > 0) {
			adpf_ratio = (durationNanos * 1024) / targetDurationNanos;
			set_group_active_ratio_cap(GROUP_ID_2, adpf_ratio);
		} else {
			set_group_active_ratio_cap(GROUP_ID_2, 0);
		}
		mutex_unlock(&adpf_mutex);
		break;
	case ADPF_PAUSE:
		set_group_active_ratio_cap(GROUP_ID_2, 0);
		break;
	case ADPF_RESUME:
		//TODO
		break;
	case ADPF_CLOSE:
		if(sid >= ADPF_MAX_SESSION) {
			pr_debug("[%s] sid error: %d cmd: %d", __func__, sid, cmd);
			return -1;
		}
		mutex_lock(&adpf_mutex);
		for(i = 0; i < session->threadIds_size; i++)
			__set_task_to_group(session->threadIds[i], -1);
		set_group_active_ratio_cap(GROUP_ID_2, 0);
		mutex_unlock(&adpf_mutex);
		break;
	case ADPF_SENT_HINT:
		mutex_lock(&adpf_mutex);
		if (sid >= ADPF_MAX_SESSION) {
			pr_debug("[%s] sid error: %d cmd: %d", __func__, sid, cmd);
			mutex_unlock(&adpf_mutex);
			return -1;
		}
		mutex_unlock(&adpf_mutex);
		break;
	case ADPF_SET_THREADS:
		if (sid >= ADPF_MAX_SESSION) {
			pr_debug("[%s] sid error: %d cmd: %d", __func__, sid, cmd);
			return -1;
		}
		mutex_lock(&adpf_mutex);
		for(i = 0; i < session->threadIds_size; i++) {
			__set_task_to_group(session->threadIds[i], -1);
			__set_task_to_group(session->threadIds[i], GROUP_ID_2);
			set_task_basic_vip(session->threadIds[i]);
		}
		mutex_unlock(&adpf_mutex);
		break;
	default:
		break;
	}

	if (trace_sched_adpf_get_value_enabled()) {
		trace_sched_adpf_get_value(cmd, sid, session->tgid, session->uid,
			session->threadIds_size, session->targetDurationNanos);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sched_adpf_callback);

void set_eas_adpf_enable(int val)
{
	eas_adpf_enable = val;
}
EXPORT_SYMBOL_GPL(set_eas_adpf_enable);

int get_eas_adpf_enable(void)
{
	return eas_adpf_enable;
}
EXPORT_SYMBOL_GPL(get_eas_adpf_enable);

