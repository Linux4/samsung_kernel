// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 * author: Myong Jae Kim
 * email: myoungjae.kim@samsung.com
 */

#define pr_fmt(fmt) "[BSP] " fmt
#define DEBUG		0

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/sched/debug.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/stacktrace.h>
#include <linux/string.h>
#include <linux/device.h>
#include <trace/events/power.h>
#include <linux/proc_fs.h>

#include "sec_debug_resume_profile.h"

static void _clear_events(void);
static void _state_change(const char *action, int val, bool start);
static void _log_new_event(const char *action, int val, bool start);
static void _print_full_events(struct seq_file *m, void *v);

static void *_get_callback_resume(struct device *dev);
static void *_get_callback_resume_early(struct device *dev);
static void *_get_callback_resume_noirq(struct device *dev);
static void *_get_callback_resume_complete(struct device *dev);
static void *(*_get_callback_getter_of_stage(struct pm_major_event *evt))(struct device *dev);

static LIST_HEAD(pm_major_events);
static int state;
static u64 temp_timestamp;
static int pms_pid;
static u64 threshold_in_ns = 1000*1000;
static DEFINE_SPINLOCK(state_lock);
static DEFINE_SPINLOCK(list_lock);
static struct device *temp_dev;

static void secdbg_resprf_suspend_resume(void *ignore, const char *action,
					int val, bool start)
{
	if (!spin_trylock(&state_lock))
		return;

	_state_change(action, val, start);
#if DEBUG
	pr_info("action:%10s, val:%2d, start:%d\n", action, val, (int)start);
#endif

	if (state == ACTIVE)
		_log_new_event(action, val, start);
	spin_unlock(&state_lock);
}

static void secdbg_resprf_devpm_cb_start(void *ignore, struct device *dev,
					const char *pm_ops, int event)
{
	if (state != ACTIVE
		|| current->pid != pms_pid)
		return;

	temp_dev = dev;
	temp_timestamp = local_clock();
}

static void secdbg_resprf_devpm_cb_end(void *ignore, struct device *dev,
					int error)
{
	struct pm_devpm_event *new_evt;
	struct pm_major_event *cur_main_evt;
	void *callback;
	u64 dur;
	void *(*pf_callback_getter)(struct device *dev);

	dur = local_clock() - temp_timestamp;

	if (current->pid != pms_pid
		|| dur < threshold_in_ns
		|| temp_timestamp == 0
		|| list_empty(&pm_major_events)
		|| dev != temp_dev)
		goto end;

	if (!spin_trylock(&state_lock))
		goto end;

	if (state != ACTIVE)
		goto unlock;

	cur_main_evt = container_of(pm_major_events.prev, struct pm_major_event, list);
	pf_callback_getter = _get_callback_getter_of_stage(cur_main_evt);

	if (!pf_callback_getter)
		goto unlock;

	callback = pf_callback_getter(dev);

	if (!callback)
		goto unlock;

	new_evt = kmalloc(
		sizeof(struct pm_devpm_event),
		GFP_NOWAIT | __GFP_NOWARN | __GFP_NORETRY);

	if (!new_evt)
		goto unlock;

	snprintf(new_evt->fn_name, NAME_MAX_LEN, "%ps", callback);
	snprintf(new_evt->drv_name, NAME_MAX_LEN, "%s", dev_driver_string(dev));
	new_evt->timestamp = temp_timestamp;
	new_evt->duration = dur;
	list_add_tail(&new_evt->list, &cur_main_evt->devpm_list);

unlock:
	spin_unlock(&state_lock);
end:
	temp_dev = NULL;
	temp_timestamp = 0;
}

static void _state_change(const char *action, int val, bool start)
{
	int prev = state;

	switch (prev) {
	case INACTIVE_LIST_EMPTY:
		if (strncmp(action, ACTIVATION_EVENT, strlen(ACTIVATION_EVENT)) == 0
		&& start == false) {
			pms_pid = current->pid;
			state = ACTIVE;
		}
		break;

	case ACTIVE:
		if (strncmp(action, INACTIVATION_EVENT, strlen(INACTIVATION_EVENT)) == 0
			&& start == false) {
			/* log inactivation event here because we cannot do that after
			 * state transition
			 */
			_log_new_event(action, val, start);
			state = INACTIVE_LIST_FULL;
		}
		break;

	case INACTIVE_LIST_FULL:
		if (strncmp(action, CLEAR_EVENT, strlen(CLEAR_EVENT)) == 0) {
			if (!spin_trylock(&list_lock)) {
#if DEBUG
				pr_info("state transition ignored by list lock");
#endif
				return;
			}
			_clear_events();
			spin_unlock(&list_lock);
			state = INACTIVE_LIST_EMPTY;
		} else if (strncmp(action,
					PROC_READING_START_EVENT,
					strlen(PROC_READING_START_EVENT)) == 0) {
			state = PROC_READING;
		}
		break;

	case PROC_READING:
		if (strncmp(action, PROC_READING_END_EVENT, strlen(PROC_READING_END_EVENT)) == 0)
			state = INACTIVE_LIST_FULL;

		break;

	}

#if DEBUG
	if (prev != state)
		pr_info("state transition from [%d] to [%d]\n", prev, state);
#endif
}

static void _clear_events(void)
{
	struct pm_major_event *cur, *tmp;
	struct pm_devpm_event *cur_dev, *tmp_dev;

	list_for_each_entry_safe(cur, tmp, &pm_major_events, list) {
		list_for_each_entry_safe(cur_dev, tmp_dev, &cur->devpm_list, list) {
			list_del(&cur_dev->list);
			kfree(cur_dev);
		}
		list_del(&cur->list);
		kfree(cur);
	}
}

static void _log_new_event(const char *action, int val, bool start)
{
	struct pm_major_event *new_evt;
	u64 now = local_clock();

	if (!spin_trylock(&list_lock))
		return;

	new_evt = kmalloc(
		sizeof(struct pm_major_event),
		GFP_NOWAIT | __GFP_NOWARN | __GFP_NORETRY);

	if (!new_evt)
		return;

	strscpy(new_evt->name, action, NAME_MAX_LEN);
	new_evt->timestamp = now;
	new_evt->start = start;
	INIT_LIST_HEAD(&new_evt->devpm_list);
	list_add_tail(&new_evt->list, &pm_major_events);
	spin_unlock(&list_lock);
}

static void *_get_callback_resume(struct device *dev)
{
	void *callback = NULL;

	if (dev->pm_domain) {
		callback = dev->pm_domain->ops.resume;
		goto Driver;
	}

	if (dev->type && dev->type->pm) {
		callback = dev->type->pm->resume;
		goto Driver;
	}

	if (dev->class && dev->class->pm) {
		callback = dev->class->pm->resume;
		goto Driver;
	}

	if (dev->bus) {
		if (dev->bus->pm) {
			callback = dev->bus->pm->resume;
		} else if (dev->bus->resume) {
			callback = dev->bus->resume;
			goto End;
		}
	}

Driver:
	if (!callback && dev->driver && dev->driver->pm)
		callback = dev->driver->pm->resume;

End:
	return callback;
}

static void *_get_callback_resume_early(struct device *dev)
{
	void *callback = NULL;

	if (dev->pm_domain)
		callback = dev->pm_domain->ops.resume_early;
	else if (dev->type && dev->type->pm)
		callback = dev->type->pm->resume_early;
	else if (dev->class && dev->class->pm)
		callback = dev->class->pm->resume_early;
	else if (dev->bus && dev->bus->pm)
		callback = dev->bus->pm->resume_early;

	if (callback)
		goto Run;

	if (dev->driver && dev->driver->pm)
		callback = dev->driver->pm->resume_early;

Run:
	return callback;
}

static void *_get_callback_resume_noirq(struct device *dev)
{
	void *callback = NULL;

	if (dev->pm_domain)
		callback = dev->pm_domain->ops.resume_noirq;
	else if (dev->type && dev->type->pm)
		callback = dev->type->pm->resume_noirq;
	else if (dev->class && dev->class->pm)
		callback = dev->class->pm->resume_noirq;
	else if (dev->bus && dev->bus->pm)
		callback = dev->bus->pm->resume_noirq;

	if (callback)
		goto Run;

	if (dev->driver && dev->driver->pm)
		callback = dev->driver->pm->resume_noirq;

Run:
	return callback;
}

static void *_get_callback_resume_complete(struct device *dev)
{
	void *callback = NULL;

	if (dev->pm_domain)
		callback = dev->pm_domain->ops.complete;
	else if (dev->type && dev->type->pm)
		callback = dev->type->pm->complete;
	else if (dev->class && dev->class->pm)
		callback = dev->class->pm->complete;
	else if (dev->bus && dev->bus->pm)
		callback = dev->bus->pm->complete;


	if (!callback && dev->driver && dev->driver->pm)
		callback = dev->driver->pm->complete;


	return callback;
}

static void *(*_get_callback_getter_of_stage(struct pm_major_event *evt))(struct device *dev)
{
	if (strncmp(evt->name, "dpm_resume_noirq", NAME_MAX_LEN) == 0)
		return _get_callback_resume_noirq;
	else if (strncmp(evt->name, "dpm_resume_early", NAME_MAX_LEN) == 0)
		return _get_callback_resume_early;
	else if (strncmp(evt->name, "dpm_resume", NAME_MAX_LEN) == 0)
		return _get_callback_resume;
	else if (strncmp(evt->name, "dpm_complete", NAME_MAX_LEN) == 0)
		return _get_callback_resume_complete;
	else
		return NULL;
}

static int secdbg_resprf_show(struct seq_file *m, void *v)
{
	if (!spin_trylock(&state_lock))
		return 0;

	if (state != INACTIVE_LIST_FULL) {
#if DEBUG
		pr_info("proc reading is gonna be ignored by cur state:%d", state);
#endif
		spin_unlock(&state_lock);
		return 0;
	}
	_state_change(PROC_READING_START_EVENT, 0, true);

	if (!spin_trylock(&list_lock)) {
#if DEBUG
		pr_info("failed to read list\n");
#endif
		goto state_unlock;
	}

	_print_full_events(m, v);
	spin_unlock(&list_lock);

state_unlock:
	_state_change(PROC_READING_END_EVENT, 0, true);
	spin_unlock(&state_lock);

	return 0;
}

static void _print_full_events(struct seq_file *m, void *v)
{
	struct pm_major_event *cur;
	struct pm_devpm_event *cur_dev_pm;

	list_for_each_entry(cur, &pm_major_events, list) {
		seq_printf(m, "[%-32s] at %6llu.%09llu sec %s\n",
			cur->name,
			cur->timestamp / 1000000000,
			cur->timestamp % 1000000000,
			cur->start ? "starts" : "ends");

		if (!list_empty(&cur->devpm_list)) {
			list_for_each_entry(cur_dev_pm, &cur->devpm_list, list) {
				seq_printf(m, "\t[%-32s][%-32s] at %6llu.%09llu sec for %3llu.%06llu ms\n",
					cur_dev_pm->drv_name,
					cur_dev_pm->fn_name,
					cur_dev_pm->timestamp / 1000000000,
					cur_dev_pm->timestamp % 1000000000,
					cur_dev_pm->duration / 1000000,
					cur_dev_pm->duration % 1000000);
			}
		}
	}
}

static int __init secdbg_resume_profile_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create_single("resume_profile", 0440, NULL, secdbg_resprf_show);
	if (!entry)
		return -ENOMEM;

	register_trace_suspend_resume(secdbg_resprf_suspend_resume, NULL);
	register_trace_device_pm_callback_start(secdbg_resprf_devpm_cb_start, NULL);
	register_trace_device_pm_callback_end(secdbg_resprf_devpm_cb_end, NULL);

	return 0;
}
late_initcall_sync(secdbg_resume_profile_init);

static void __exit secdbg_resume_profile_exit(void)
{

}
module_exit(secdbg_resume_profile_exit);

MODULE_DESCRIPTION("Samsung Debug Resume Profiler Info driver");
MODULE_LICENSE("GPL v2");

