/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */
/*
 ******************************
 *          HEADERS
 ******************************
 */

#include <osa.h>

/*
 ******************************
 *          MACROS
 ******************************
 */

#define INIT_SEM_MAGIC(s)               \
do {                                    \
	((struct _sem *)s)->magic[0] = 'S'; \
	((struct _sem *)s)->magic[1] = 'e'; \
	((struct _sem *)s)->magic[2] = 'M'; \
	((struct _sem *)s)->magic[3] = 'a'; \
} while (0)

#define IS_SEM_VALID(s)                 \
	('S' == ((struct _sem *)s)->magic[0] && \
	 'e' == ((struct _sem *)s)->magic[1] && \
	 'M' == ((struct _sem *)s)->magic[2] && \
	 'a' == ((struct _sem *)s)->magic[3])

#define CLEAN_SEM_MAGIC(s)            \
do {                                  \
	((struct _sem *)s)->magic[0] = 0; \
	((struct _sem *)s)->magic[1] = 0; \
	((struct _sem *)s)->magic[2] = 0; \
	((struct _sem *)s)->magic[3] = 0; \
} while (0)

/*
 ******************************
 *          TYPES
 ******************************
 */

struct _sem {
	uint8_t magic[4];
	struct semaphore sem;
};

struct _timeout_arg {
	struct _sem *sem;
	struct task_struct *task;

	bool *sem_up;
	bool *timeout;
};

struct _mutex {
	/* no magic since there is one in struct _sem */
	struct _sem *osa_sem;
	spinlock_t lock;	/* to protect sem.counter */
};

/*
 ******************************
 *          FUNCTIONS
 ******************************
 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28))

/*
 * Name:        _osa_sem_timeout
 *
 * Description: callback function when timeout
 *
 * Params:      data - the pointer to _sem structure
 *
 * Returns:     none
 *
 * Notes:       none
 */
static void _osa_sem_timeout(ulong_t data)
{
	struct _timeout_arg *arg = (struct _timeout_arg *)data;
	struct _sem *sem;
	struct task_struct *task;
	bool *sem_up;
	bool *timeout;

	OSA_ASSERT(arg);

	sem = arg->sem;
	task = arg->task;
	sem_up = arg->sem_up;
	timeout = arg->timeout;

	OSA_ASSERT(sem);
	OSA_ASSERT(IS_SEM_VALID(sem));
	OSA_ASSERT(task);
	OSA_ASSERT(sem_up);
	OSA_ASSERT(timeout);

	if (!*sem_up) {
		struct list_head *entry = NULL;
		wait_queue_t *wq = NULL;

		spin_lock(&(sem->sem.wait.lock));

		/* iterate the list and find the entry of "task" */
		osa_list_iterate(&(sem->sem.wait.task_list), entry) {
			OSA_LIST_ENTRY(entry, wait_queue_t, task_list, wq);
			if ((struct task_struct *)(wq->private) == task)
				break;
		}

		/*
		 * the entry has to be found
		 * or someone else has up-ed the sem already
		 */
		if ((struct task_struct *)wq->private == task) {
			/*
			 * here we need to set the entry of "task"
			 * the *first* member of the wait list
			 */
			__remove_wait_queue(&(sem->sem.wait), wq);
			wq->flags |= WQ_FLAG_EXCLUSIVE;
			__add_wait_queue(&(sem->sem.wait), wq);

			spin_unlock(&(sem->sem.wait.lock));

			up(&(sem->sem));
			*timeout = true;
		} else
			spin_unlock(&(sem->sem.wait.lock));
	}
}

/*
 * Name:        osa_create_sem
 *
 * Description: create a sempahore with timeout feature
 *
 * Params:      init_val - the initial value of the semaphore to create
 *
 * Returns:     osa_sem_t - the semaphore handle
 *
 * Notes:       none
 */
osa_sem_t osa_create_sem(int32_t init_val)
{
	struct _sem *sem;

	sem = kmalloc(sizeof(struct _sem), GFP_KERNEL);
	if (!sem) {
		osa_dbg_print(DBG_ERR,
			      "ERROR - failed to kmalloc in osa_create_sem\n");
		return NULL;
	}

	INIT_SEM_MAGIC(sem);

	sema_init(&(sem->sem), init_val);

	return (osa_sem_t) sem;
}
OSA_EXPORT_SYMBOL(osa_create_sem);

/*
 * Name:        osa_destroy_sem
 *
 * Description: destroy the semaphore specified by handle
 *
 * Params:      handle - handle of the semaphore to destroy
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_destroy_sem(osa_sem_t handle)
{
	struct _sem *sem = (struct _sem *)handle;

	OSA_ASSERT(sem && IS_SEM_VALID(sem));

	CLEAN_SEM_MAGIC(sem);
	kfree(sem);
}
OSA_EXPORT_SYMBOL(osa_destroy_sem);

/*
 * Name:        osa_wait_for_sem
 *
 * Description: wait for the semaphore
 *
 * Params:      handle - handle of the semaphore to wait for
 *              msec - the number in the unit of micro-second
 *                     wait forever if msec < 0
 *
 * Returns:     status code
 *              OSA_OK              - no error
 *              OSA_SEM_WAIT_FAILED - failed to wait for the semaphore
 *              OSA_SEM_WAIT_TO     - timeout when waiting for the semaphore
 *
 * Notes:       timeout is implemented by timer in kernel
 */
osa_err_t osa_wait_for_sem(osa_sem_t handle, int32_t msec)
{
	struct _sem *sem = (struct _sem *)handle;
	struct _timeout_arg to_arg;
	struct timer_list timer;
	bool sem_up = false;
	bool timeout = false;
	int ret;

	OSA_ASSERT(sem && IS_SEM_VALID(sem));

	if (0 == msec) {
		ret = down_trylock(&(sem->sem));
		if (ret)
			return OSA_SEM_WAIT_FAILED;
		else
			return OSA_OK;
	} else if (msec > 0) {
		/*
		 * here we use an internal variable
		 * as the parameter of the timer's callback
		 * if the timer's callback is invoked,
		 * osa_wait_for_sem will be blocked
		 * so the internal variable is valid
		 * during timer's callback is invoked
		 */
		to_arg.sem = sem;
		to_arg.task = current;
		to_arg.sem_up = &sem_up;
		to_arg.timeout = &timeout;

		init_timer(&timer);

		timer.function = _osa_sem_timeout;
		timer.expires = jiffies + (msec * HZ + 500) / 1000;
		timer.data = (uint32_t)&to_arg;

		add_timer(&timer);
	}

	down(&(sem->sem));
	sem_up = true;

	if (msec > 0)
		del_timer_sync(&timer);

	if (timeout)
		return OSA_SEM_WAIT_TO;
	else
		return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_wait_for_sem);

/*
 * Name:        osa_try_to_wait_for_sem
 *
 * Description: try to wait the semaphore
 *
 * Params:      handle - handle of the semaphore to wait for
 *
 * Returns:     status code
 *              OSA_OK              - no error
 *              OSA_SEM_WAIT_FAILED - failed to wait for the semaphore
 *
 * Notes:       none
 */
osa_err_t osa_try_to_wait_for_sem(osa_sem_t handle)
{
	struct _sem *sem = (struct _sem *)handle;
	int ret;

	OSA_ASSERT(sem && IS_SEM_VALID(sem));

	ret = down_trylock(&(sem->sem));

	if (ret)
		return OSA_SEM_WAIT_FAILED;
	else
		return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_try_to_wait_for_sem);

/*
 * Name:        osa_release_sem
 *
 * Description: release the semaphore specified by handle
 *
 * Params:      handle - handle of the semaphore to release
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_release_sem(osa_sem_t handle)
{
	struct _sem *sem = (struct _sem *)handle;

	OSA_ASSERT(sem && IS_SEM_VALID(sem));

	up(&(sem->sem));
}
OSA_EXPORT_SYMBOL(osa_release_sem);

#else

/*
 * Name:        osa_create_sem
 *
 * Description: create a sempahore with timeout feature
 *
 * Params:      init_val - the initial value of the semaphore to create
 *
 * Returns:     osa_sem_t - the semaphore handle
 *
 * Notes:       none
 */
osa_sem_t osa_create_sem(int32_t init_val)
{
	struct _sem *sem;

	sem = kmalloc(sizeof(struct _sem), GFP_KERNEL);
	if (!sem) {
		osa_dbg_print(DBG_ERR,
			      "ERROR - failed to kmalloc in osa_create_sem\n");
		return NULL;
	}

	INIT_SEM_MAGIC(sem);

	sema_init(&(sem->sem), init_val);

	return (osa_sem_t) sem;
}
OSA_EXPORT_SYMBOL(osa_create_sem);

/*
 * Name:        osa_destroy_sem
 *
 * Description: destroy the semaphore specified by handle
 *
 * Params:      handle - handle of the semaphore to destroy
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_destroy_sem(osa_sem_t handle)
{
	struct _sem *sem = (struct _sem *)handle;

	OSA_ASSERT(sem && IS_SEM_VALID(sem));

	CLEAN_SEM_MAGIC(sem);
	kfree(sem);
}
OSA_EXPORT_SYMBOL(osa_destroy_sem);

/*
 * Name:        osa_wait_for_sem
 *
 * Description: wait for the semaphore
 *
 * Params:      handle - handle of the semaphore to wait for
 *              msec - the number in the unit of micro-second
 *                     wait forever if msec < 0
 *
 * Returns:     status code
 *              OSA_OK              - no error
 *              OSA_SEM_WAIT_FAILED - failed to wait for the semaphore
 *              OSA_SEM_WAIT_TO     - timeout when waiting for the semaphore
 *
 * Notes:       timeout is implemented by timer in kernel
 */
osa_err_t osa_wait_for_sem(osa_sem_t handle, int32_t msec)
{
	struct _sem *sem = (struct _sem *)handle;
	int ret;

	OSA_ASSERT(sem && IS_SEM_VALID(sem));

	if (0 == msec) {

		ret = down_trylock(&(sem->sem));
		if (ret)
			return OSA_SEM_WAIT_FAILED;
		else
			return OSA_OK;

	} else if (msec > 0) {

		ret = down_timeout(&(sem->sem), (msec * HZ + 500) / 1000);
		if (ret < 0)
			return OSA_SEM_WAIT_TO;

	} else
		down(&(sem->sem));

	return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_wait_for_sem);

/*
 * Name:        osa_try_to_wait_for_sem
 *
 * Description: try to wait the semaphore
 *
 * Params:      handle - handle of the semaphore to wait for
 *
 * Returns:     status code
 *              OSA_OK              - no error
 *              OSA_SEM_WAIT_FAILED - failed to wait for the semaphore
 *
 * Notes:       none
 */
osa_err_t osa_try_to_wait_for_sem(osa_sem_t handle)
{
	struct _sem *sem = (struct _sem *)handle;
	int ret;

	OSA_ASSERT(sem && IS_SEM_VALID(sem));

	ret = down_trylock(&(sem->sem));

	if (ret)
		return OSA_SEM_WAIT_FAILED;
	else
		return OSA_OK;
}
OSA_EXPORT_SYMBOL(osa_try_to_wait_for_sem);

/*
 * Name:        osa_release_sem
 *
 * Description: release the semaphore specified by handle
 *
 * Params:      handle - handle of the semaphore to release
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_release_sem(osa_sem_t handle)
{
	struct _sem *sem = (struct _sem *)handle;

	OSA_ASSERT(sem && IS_SEM_VALID(sem));

	up(&(sem->sem));
}
OSA_EXPORT_SYMBOL(osa_release_sem);

#endif

/*
 * Name:        _osa_create_mutex
 *
 * Description: create a mutex with timeout feature
 *
 * Params:      init_val - initial value (0/1)
 *
 * Returns:     osa_mutex_t - the mutex handle
 *
 * Notes:       none
 */
static osa_mutex_t _osa_create_mutex(uint32_t init_val)
{
	struct _mutex *mutex;

	mutex = kmalloc(sizeof(struct _mutex), GFP_KERNEL);
	if (!mutex) {
		osa_dbg_print(DBG_ERR,
			      "ERROR - failed to kmalloc in osa_create_mutex\n");
		return NULL;
	}

	mutex->osa_sem = (struct _sem *)osa_create_sem(init_val);
	if (!mutex->osa_sem) {
		/* error log will be printed in osa_create_sem */
		return NULL;
	}

	spin_lock_init(&mutex->lock);

	return (osa_mutex_t) mutex;
}

/*
 * Name:        osa_create_mutex
 *
 * Description: create a mutex with timeout feature
 *
 * Params:      none
 *
 * Returns:     osa_mutex_t - the mutex handle
 *
 * Notes:       none
 */
osa_mutex_t osa_create_mutex(void)
{
	return _osa_create_mutex(1);
}
OSA_EXPORT_SYMBOL(osa_create_mutex);

/*
 * Name:        osa_create_mutex_locked
 *
 * Description: create a mutex locked
 *
 * Params:      none
 *
 * Returns:     osa_mutex_t - the mutex handle
 *
 * Notes:       none
 */
osa_mutex_t osa_create_mutex_locked(void)
{
	return _osa_create_mutex(0);
}
OSA_EXPORT_SYMBOL(osa_create_mutex_locked);

/*
 * Name:        osa_destroy_mutex
 *
 * Description: destroy the mutex specified by handle
 *
 * Params:      handle - handle of the mutex to destroy
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_destroy_mutex(osa_mutex_t handle)
{
	struct _mutex *mutex = (struct _mutex *)handle;

	osa_destroy_sem(mutex->osa_sem);

	kfree(mutex);
}
OSA_EXPORT_SYMBOL(osa_destroy_mutex);

/*
 * Name:        osa_wait_for_mutex
 *
 * Description: wait for the mutex
 *
 * Params:      handle - handle of the mutex to wait for
 *              msec - the number in the unit of micro-second
 *                     wait forever if msec < 0
 *
 * Returns:     status code
 *              OSA_OK                - no error
 *              OSA_MUTEX_WAIT_FAILED - failed to wait for the mutex
 *              OSA_MUTEX_WAIT_TO     - timeout when waiting for the mutex
 *
 * Notes:       timeout is implemented by timer in kernel
 */
osa_err_t osa_wait_for_mutex(osa_mutex_t handle, int32_t msec)
{
	struct _mutex *mutex = (struct _mutex *)handle;
	osa_err_t ret = osa_wait_for_sem(mutex->osa_sem, msec);

	switch (ret) {
	case OSA_OK:
		return OSA_OK;
	case OSA_SEM_WAIT_FAILED:
		return OSA_MUTEX_WAIT_FAILED;
	case OSA_SEM_WAIT_TO:
		return OSA_MUTEX_WAIT_TO;
	default:
		OSA_ASSERT(0);
		/* return OSA_ERR to avoid warnings */
		return OSA_ERR;
	}
}
OSA_EXPORT_SYMBOL(osa_wait_for_mutex);

/*
 * Name:        osa_try_to_wait_for_mutex
 *
 * Description: try to wait the mutex
 *
 * Params:      handle - handle of the mutex to wait for
 *
 * Returns:     status code
 *              OSA_OK                - no error
 *              OSA_MUTEX_WAIT_FAILED - failed to wait for the mutex
 *
 * Notes:       none
 */
osa_err_t osa_try_to_wait_for_mutex(osa_mutex_t handle)
{
	struct _mutex *mutex = (struct _mutex *)handle;
	osa_err_t ret = osa_try_to_wait_for_sem(mutex->osa_sem);

	switch (ret) {
	case OSA_OK:
		return OSA_OK;
	case OSA_SEM_WAIT_FAILED:
		return OSA_MUTEX_WAIT_FAILED;
	default:
		OSA_ASSERT(0);
		/* return OSA_ERR to avoid warnings */
		return OSA_ERR;
	}
}
OSA_EXPORT_SYMBOL(osa_try_to_wait_for_mutex);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28))

/*
 * Name:        osa_release_mutex
 *
 * Description: release the mutex specified by handle
 *
 * Params:      handle - handle of the mutex to release
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_release_mutex(osa_mutex_t handle)
{
	struct _mutex *mutex = (struct _mutex *)handle;
	ulong_t flags;

	/* here we need to check sem's magic first */
	OSA_ASSERT(mutex->osa_sem && IS_SEM_VALID(mutex->osa_sem));

	spin_lock_irqsave(&mutex->lock, flags);

	/*
	 * for "sleepers" and "count", please refer to
	 * "5.2. Synchronization Primitives,
	 * Understanding The Linux Kernel, 3rd Edition".
	 */
	if (mutex->osa_sem->sem.sleepers)
		osa_release_sem(mutex->osa_sem);
	else
		if (atomic_read(&(mutex->osa_sem->sem.count)) == 0)
			osa_release_sem(mutex->osa_sem);

	spin_unlock_irqrestore(&mutex->lock, flags);
}
OSA_EXPORT_SYMBOL(osa_release_mutex);

#else

/*
 * Name:        osa_release_mutex
 *
 * Description: release the mutex specified by handle
 *
 * Params:      handle - handle of the mutex to release
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_release_mutex(osa_mutex_t handle)
{
	struct _mutex *mutex = (struct _mutex *)handle;

	/* here we need to check sem's magic first */
	OSA_ASSERT(mutex->osa_sem && IS_SEM_VALID(mutex->osa_sem));

	osa_release_sem(mutex->osa_sem);
}
OSA_EXPORT_SYMBOL(osa_release_mutex);

#endif

/*
 * Name:        osa_create_event
 *
 * Description: create an event with timeout feature
 *
 * Params:      is_set      - the event created is set or not.
 *
 * Returns:     osa_event_t - the event handle
 *
 * Notes:       none
 */
osa_event_t osa_create_event(bool is_set)
{
	osa_mutex_t ret;

	if (is_set)
		ret = osa_create_mutex();
	else
		ret = osa_create_mutex_locked();

	if (!ret)
		return (osa_event_t) NULL;

	return (osa_event_t) ret;
}
OSA_EXPORT_SYMBOL(osa_create_event);

/*
 * Name:        osa_destroy_event
 *
 * Description: destroy the event specified by handle
 *
 * Params:      handle - handle of the event to destroy
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_destroy_event(osa_event_t handle)
{
	osa_mutex_t mutex = (osa_mutex_t) handle;

	return osa_destroy_mutex(mutex);
}
OSA_EXPORT_SYMBOL(osa_destroy_event);

/*
 * Name:        osa_wait_for_event
 *
 * Description: wait for the event
 *
 * Params:      handle - handle of the event to wait for
 *              msec - the number in the unit of micro-second
 *                     wait forever if msec < 0
 *
 * Returns:     status code
 *              OSA_OK                - no error
 *              OSA_EVENT_WAIT_TO     - timeout when waiting for the event
 *
 * Notes:       timeout is implemented by timer in kernel
 */
osa_err_t osa_wait_for_event(osa_event_t handle, int32_t msec)
{
	osa_err_t ret;

	osa_mutex_t mutex = (osa_mutex_t) handle;

	ret = osa_wait_for_mutex(mutex, msec);

	if (OSA_MUTEX_WAIT_TO == ret)
		ret = OSA_EVENT_WAIT_TO;

	return ret;
}
OSA_EXPORT_SYMBOL(osa_wait_for_event);

/*
 * Name:        osa_try_to_wait_for_event
 *
 * Description: try to wait the event
 *
 * Params:      handle - handle of the event to wait for
 *
 * Returns:     status code
 *              OSA_OK                - no error
 *              OSA_EVENT_WAIT_FAILED - failed to wait for the event
 *
 * Notes:       none
 */
osa_err_t osa_try_to_wait_for_event(osa_event_t handle)
{
	return osa_wait_for_event(handle, 0);
}
OSA_EXPORT_SYMBOL(osa_try_to_wait_for_event);

/*
 * Name:        osa_set_event
 *
 * Description: set the event specified by handle
 *
 * Params:      handle - handle of the event to release
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_set_event(osa_event_t handle)
{
	osa_mutex_t mutex = handle;

	osa_release_mutex(mutex);
}
OSA_EXPORT_SYMBOL(osa_set_event);

/*
 * Name:        osa_reset_event
 *
 * Description: reset the event specified by handle
 *
 * Params:      handle - handle of the event to release
 *
 * Returns:     none
 *
 * Notes:       none
 */
void osa_reset_event(osa_event_t handle)
{
	osa_mutex_t mutex = handle;

	(void)osa_try_to_wait_for_mutex(mutex);
}
OSA_EXPORT_SYMBOL(osa_reset_event);
