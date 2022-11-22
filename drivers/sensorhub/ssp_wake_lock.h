#ifndef SSP_WAKELOCK
#define SSP_WAKELOCK

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>

struct ssp_wake_lock
{
	struct wakeup_source ws;
};

static inline void ssp_wake_lock_init(struct ssp_wake_lock *lock, const char *name)
{
	wakeup_source_init(&lock->ws, name);
}

static inline void ssp_wake_lock_destroy(struct ssp_wake_lock *lock)
{
	wakeup_source_trash(&lock->ws);
}

static inline void ssp_wake_lock(struct ssp_wake_lock *lock)
{
	__pm_stay_awake(&lock->ws);
}

static inline void ssp_wake_lock_timeout(struct ssp_wake_lock *lock, unsigned int msec)
{
	__pm_wakeup_event(&lock->ws, msec);
}

static inline void ssp_wake_unlock(struct ssp_wake_lock *lock)
{
	__pm_relax(&lock->ws);
}
#else
#include <linux/wake_lock.h>

struct ssp_wake_lock
{
	struct wake_lock wl;
};

static inline void ssp_wake_lock_init(struct ssp_wake_lock *lock, const char *name)
{
	wake_lock_init(&lock->wl, WAKE_LOCK_SUSPEND, name);
}

static inline void ssp_wake_lock_destroy(struct ssp_wake_lock *lock)
{
	wake_lock_destroy(&lock->wl);
}

static inline void ssp_wake_lock(struct ssp_wake_lock *lock)
{
	wake_lock(&lock->wl);
}

static inline void ssp_wake_lock_timeout(struct ssp_wake_lock *lock, unsigned int msec)
{
	wake_lock_timeout(&lock->wl, msec * (HZ * 0.001));
}

static inline void ssp_wake_unlock(struct ssp_wake_lock *lock)
{
	wake_unlock(&lock->wl);
}
#endif

#endif
