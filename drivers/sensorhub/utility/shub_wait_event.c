#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/jiffies.h>

#include "shub_wait_event.h"

int shub_wait_event_timeout(struct shub_waitevent *lock, int timeout)
{
	int ret;
	ret =  wait_event_interruptible_timeout(lock->waitqueue, atomic_read(&lock->state), msecs_to_jiffies(timeout));

	if(ret == 0)
		return -ETIME;
	else
		return 0;

}

void shub_lock_wait_event(struct shub_waitevent *lock)
{
	atomic_set(&lock->state, 0);
}

void shub_wake_up_wait_event(struct shub_waitevent *lock)
{
	atomic_set(&lock->state, 1);
	wake_up_interruptible_sync(&lock->waitqueue);
}