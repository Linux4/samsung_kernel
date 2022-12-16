/* include/linux/wakelock.h
 *
 * Copyright (C) 2007-2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_WAKELOCK_H
#define _LINUX_WAKELOCK_H

#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>

/* A wake_lock prevents the system from entering suspend or other low power
 * states when active. If the type is set to WAKE_LOCK_SUSPEND, the wake_lock
 * prevents a full system suspend.
 */

enum {
	WAKE_LOCK_SUSPEND, /* Prevent suspend */
	WAKE_LOCK_TYPE_COUNT
};

struct wake_lock {
#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source *ptr_ws;
#else
	struct wakeup_source ws;
#endif
};

static inline void wake_lock_init(struct wake_lock *lock, int type,
				  const char *name)
{
#ifdef CONFIG_PM_WAKELOCKS
	lock->ptr_ws = wakeup_source_register(NULL, name);
#else
	wakeup_source_init(&lock->ws, name);
#endif
}

static inline void wake_lock_destroy(struct wake_lock *lock)
{
#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_unregister(lock->ptr_ws);
#else
	wakeup_source_trash(&lock->ws);
#endif
}

static inline void wake_lock(struct wake_lock *lock)
{
#ifdef CONFIG_PM_WAKELOCKS
	__pm_stay_awake(lock->ptr_ws);
#else
	__pm_stay_awake(&lock->ws);
#endif
}

static inline void wake_lock_timeout(struct wake_lock *lock, long timeout)
{
#ifdef CONFIG_PM_WAKELOCKS
	__pm_wakeup_event(lock->ptr_ws, jiffies_to_msecs(timeout));
#else
	__pm_wakeup_event(&lock->ws, jiffies_to_msecs(timeout));
#endif
}

static inline void wake_unlock(struct wake_lock *lock)
{
#ifdef CONFIG_PM_WAKELOCKS
	__pm_relax(lock->ptr_ws);
#else
	__pm_relax(&lock->ws);
#endif
}

static inline int wake_lock_active(struct wake_lock *lock)
{
#ifdef CONFIG_PM_WAKELOCKS
	return lock->ptr_ws->active;
#else
	return lock->ws.active;
#endif
}

#endif
