/*
 * NFC Wakelock
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#ifndef _NFC_WAKELOCK_H
#define _NFC_WAKELOCK_H

#include <linux/ktime.h>
#include <linux/device.h>

enum {
	WAKE_LOCK_SUSPEND, /* Prevent suspend */
	WAKE_LOCK_TYPE_COUNT
};

struct wake_lock {
	struct wakeup_source *ws;
};

static inline void wake_lock_init(struct wake_lock *lock, int type,
				  const char *name)
{
	lock->ws = wakeup_source_register(NULL, name);
}

static inline void wake_lock_destroy(struct wake_lock *lock)
{
	if (lock->ws)
		wakeup_source_unregister(lock->ws);
}

static inline void wake_lock(struct wake_lock *lock)
{
	if (lock->ws)
		__pm_stay_awake(lock->ws);
}

static inline void wake_lock_timeout(struct wake_lock *lock, long timeout)
{
	if (lock->ws)
		__pm_wakeup_event(lock->ws, jiffies_to_msecs(timeout));
}

static inline void wake_unlock(struct wake_lock *lock)
{
	if (lock->ws)
		__pm_relax(lock->ws);
}

static inline int wake_lock_active(struct wake_lock *lock)
{
	return lock->ws->active;
}

#endif
