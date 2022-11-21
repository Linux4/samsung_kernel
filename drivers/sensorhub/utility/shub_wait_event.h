#ifndef _SHUB_WAIT_EVENT_H__
#define _SHUB_WAIT_EVENT_H__

#include <linux/wakelock.h>

struct shub_waitevent {
	wait_queue_head_t waitqueue;
	atomic_t state;
};

int shub_wait_event_timeout(struct shub_waitevent *lock, int timeout);
void shub_lock_wait_event(struct shub_waitevent *lock);
void shub_wake_up_wait_event(struct shub_waitevent *lock);

#endif