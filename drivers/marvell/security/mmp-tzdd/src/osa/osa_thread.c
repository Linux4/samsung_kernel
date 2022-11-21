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
 * Filename     : osa_thread.c
 * Author       : Dafu Lv
 * Date Created : 21/03/08
 * Description  : This is the source code of thread-related functions in osa.
 *
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

#define INIT_THREAD_MAGIC(t)               \
do {                                       \
	((struct _thread *)t)->magic[0] = 'T'; \
	((struct _thread *)t)->magic[1] = 'h'; \
	((struct _thread *)t)->magic[2] = 'R'; \
	((struct _thread *)t)->magic[3] = 'd'; \
} while (0)

#define IS_THREAD_VALID(t)                 \
	('T' == ((struct _thread *)t)->magic[0] && \
	 'h' == ((struct _thread *)t)->magic[1] && \
	 'R' == ((struct _thread *)t)->magic[2] && \
	 'd' == ((struct _thread *)t)->magic[3])

#define CLEAN_THREAD_MAGIC(t)            \
do {                                     \
	((struct _thread *)t)->magic[0] = 0; \
	((struct _thread *)t)->magic[1] = 0; \
	((struct _thread *)t)->magic[2] = 0; \
	((struct _thread *)t)->magic[3] = 0; \
} while (0)

/*
 ******************************
 *          TYPES
 ******************************
 */

enum _thread_status {
	_THREAD_UNKNOWN,
	_THREAD_READY,
	_THREAD_RUNNING,
	_THREAD_STOPPED
};

struct _thread {
	uint8_t magic[4];

	void (*func) (void *);
	void *arg;
	long_t nice;

	struct task_struct *task;
	enum _thread_status status;
	spinlock_t lock;
};

/*
 ******************************
 *          FUNCTIONS
 ******************************
 */

/*
 * local functions
 */
static void _stop_thread(struct _thread *_thrd);
static void _stop_thread_lock(struct _thread *_thrd);

/*
 * Name:        _thread_func
 *
 * Description: the shell function of thread
 *
 * Params:      param - the member "arg" in struct _thread
 *
 * Returns:     int - 0 always
 *
 * Notes:       none
 */
static int _thread_func(void *param)
{
	struct _thread *_thrd = (struct _thread *)param;

	OSA_ASSERT(_thrd);
	OSA_ASSERT(IS_THREAD_VALID(_thrd));

	/* 0~7 --> -20~19 */
	set_user_nice(current, (_thrd->nice - 3) << 2);

	spin_lock(&(_thrd->lock));
	if (_THREAD_STOPPED != _thrd->status) {
		_thrd->status = _THREAD_READY;
		spin_unlock(&(_thrd->lock));
	} else {
		spin_unlock(&(_thrd->lock));
		return 0;
	}

	/* further initialization here */

	spin_lock(&(_thrd->lock));
	if (_THREAD_STOPPED != _thrd->status) {
		_thrd->status = _THREAD_RUNNING;
		spin_unlock(&(_thrd->lock));
	} else {
		spin_unlock(&(_thrd->lock));
		return 0;
	}

	if (_THREAD_STOPPED != _thrd->status) {
		_thrd->func(_thrd->arg);

		spin_lock(&(_thrd->lock));
		_thrd->status = _THREAD_STOPPED;
		spin_unlock(&(_thrd->lock));
	}

	/*
	 * kthread_stop is a sync way to stop thread.
	 * so here we need not send any event to show my stopping status.
	 */

	return 0;
}

/*
 * Name:        osa_create_thread
 *
 * Description: create a kernel thread
 *
 * Params:      func - the function will be called in the kernel thread created
 *              arg  - the argument of the function when called
 *              attr - os-specific attributes of the kernel thread
 *              attr->name - the name of the kernel thread, default "osa_thread"
 *              attr->prio - the static priority of the thread, will be changed
 *                           dynamically.
 *                           range: 0~7 (mapping from -20~19 in linux kernel)
 *                           the default is the lowest priority (7)
 *              attr->stack_addr - no use in linux kernel
 *              attr->stack_size - no use in linux kernel
 *
 * Returns:     osa_thread_t - handle of the kernel thread to create
 *
 * Notes:       none
 */
osa_thread_t osa_create_thread(void (*func) (void *), void *arg,
			       struct osa_thread_attr *attr)
{
	struct _thread *ret = NULL;

	OSA_ASSERT(func);
	OSA_ASSERT(attr && (attr->prio >= 0) && (attr->prio <= 7));

	ret = kmalloc(sizeof(struct _thread), GFP_KERNEL);
	if (!ret) {
		osa_dbg_print(DBG_ERR,
			      "ERROR - failed to kmalloc in osa_create_thread\n");
		return ret;
	}

	INIT_THREAD_MAGIC(ret);

	ret->func = func;
	ret->arg = arg;
	ret->nice = (long_t) attr->prio;

	spin_lock_init(&(ret->lock));

	ret->status = _THREAD_UNKNOWN;

	if (NULL == attr->name)
		ret->task =
		    kthread_run(_thread_func, (void *)ret, "osa_kthread");
	else
		ret->task = kthread_run(_thread_func, (void *)ret, attr->name);

	if (!(ret->task)) {
		osa_dbg_print(DBG_ERR,
			      "ERROR - failed to create a thread in osa_create_thread\n");
		CLEAN_THREAD_MAGIC(ret);
		kfree(ret);
		ret = NULL;
		return ret;
	}

	while (_THREAD_UNKNOWN == ret->status)
		yield();

	return (osa_thread_t) ret;
}
OSA_EXPORT_SYMBOL(osa_create_thread);

/*
 * Name:        osa_destroy_thread
 *
 * Description: destroy the thread handle
 *
 * Params:      handle - handle of the thread to destroy
 *
 * Returns:     none
 *
 * Notes:       if the thread is not stopped,
			 the function will call _stop_thread_lock
 */
void osa_destroy_thread(osa_thread_t handle)
{
	struct _thread *_thrd = (struct _thread *)handle;

	OSA_ASSERT(_thrd && IS_THREAD_VALID(_thrd));

	if (_THREAD_STOPPED != _thrd->status)
		_stop_thread_lock(_thrd);

	CLEAN_THREAD_MAGIC(_thrd);

	kfree(_thrd);
}
OSA_EXPORT_SYMBOL(osa_destroy_thread);

/*
 * Name:        _stop_thread
 *
 * Description: stop the thread synchronously
 *
 * Params:      _thrd - the thread structure
 *
 * Returns:     none
 *
 * Notes:       caller must NOT hold the spinlock in thread struct
 */
static void _stop_thread(struct _thread *_thrd)
{
	/* kthread_stop is a sync way to stop thread */
	kthread_stop(_thrd->task);
	/* so here we need not wait for any stopped event */
}

/*
 * Name:        osa_stop_thread
 *
 * Description: stop the thread synchronously
 *
 * Params:      handle - handle of the kernel thread to stop
 *
 * Returns:     status code
 *              OSA_OK                    - no error
 *              OSA_THREAD_BAD_STATUS - invalid status of the thread
 *
 * Notes:       none
 */
osa_err_t osa_stop_thread(osa_thread_t handle)
{
	struct _thread *_thrd = (struct _thread *)handle;

	OSA_ASSERT(_thrd && IS_THREAD_VALID(_thrd));

	spin_lock(&(_thrd->lock));
	if (_THREAD_UNKNOWN == _thrd->status) {
		_thrd->status = _THREAD_STOPPED;
		spin_unlock(&(_thrd->lock));
		return OSA_OK;
	} else if ((_THREAD_READY != _thrd->status)
		   && (_THREAD_RUNNING != _thrd->status)) {
		spin_unlock(&(_thrd->lock));
		osa_dbg_print(DBG_ERR,
			      "ERROR - the status of thread is invalid when stopping\n");
		return OSA_THREAD_BAD_STATUS;
	} else {
		spin_unlock(&(_thrd->lock));
		_stop_thread(_thrd);
		return OSA_OK;
	}
}
OSA_EXPORT_SYMBOL(osa_stop_thread);

/*
 * Name:        _stop_thread_lock
 *
 * Description: stop the thread synchronously with holding lock
 *
 * Params:      _thrd - the thread structure
 *
 * Returns:     none
 *
 * Notes:       none
 */
static void _stop_thread_lock(struct _thread *_thrd)
{
	spin_lock(&(_thrd->lock));
	if (_THREAD_UNKNOWN == _thrd->status) {
		_thrd->status = _THREAD_STOPPED;
		spin_unlock(&(_thrd->lock));
	} else {
		spin_unlock(&(_thrd->lock));
		_stop_thread(_thrd);
	}
}

/*
 * Name:        osa_stop_thread_async
 *
 * Description: stop the thread asynchronously
 *
 * Params:      handle - handle of the kernel thread to stop
 *
 * Returns:     status code
 *              OSA_OK                 - no error
 *              OSA_THREAD_BAD_STATUS  - invalid status of the thread
 *              OSA_THREAD_STOP_FAILED - failed to invoke the stopping thread
 *
 * Notes:       no support in linux kernel
 */
osa_err_t osa_stop_thread_async(osa_thread_t handle)
{
	struct _thread *_thrd = (struct _thread *)handle;

	OSA_ASSERT(_thrd && IS_THREAD_VALID(_thrd));

	if ((_THREAD_UNKNOWN != _thrd->status)
	    && (_THREAD_READY != _thrd->status)
	    && (_THREAD_RUNNING != _thrd->status)) {
		return OSA_THREAD_BAD_STATUS;
	}

	if (kthread_run
	    ((int (*)(void *))_stop_thread_lock, _thrd,
	     "osa_thread_stop_func"))
		return OSA_OK;
	else
		return OSA_THREAD_STOP_FAILED;
}
OSA_EXPORT_SYMBOL(osa_stop_thread_async);

/*
 * Name:        osa_thread_should_stop
 *
 * Description: the function returns the flag to define
			 whether the thread should be stopped
 *
 * Params:      none
 *
 * Returns:     bool
 *              true  - the thread should be stopped as soon as possible
 *              false - vice versa
 *
 * Notes:       the function should be called in the main loop
 *              of the core function in kernel thread
 */
bool osa_thread_should_stop(void)
{
	return kthread_should_stop();
}
OSA_EXPORT_SYMBOL(osa_thread_should_stop);

/*
 * Name:        osa_get_thread_id
 *
 * Description: get the id of the kernel thread
 *
 * Params:      handle - handle of the kernel thread
 *
 * Returns:     osa_thread_id_t - the id number of the kernel thread
 *
 * Notes:       none
 */
osa_thread_id_t osa_get_thread_id(osa_thread_t handle)
{
	struct _thread *_thrd = (struct _thread *)handle;

	OSA_ASSERT(_thrd && IS_THREAD_VALID(_thrd));

	return (osa_thread_id_t) (_thrd->task->pid);
}
OSA_EXPORT_SYMBOL(osa_get_thread_id);

/*
 * Name:        osa_get_thread_group_id
 *
 * Description: get the thread group id of the kernel thread
 *
 * Params:      handle - handle of the kernel thread
 *
 * Returns:     osa_thread_id_t -
			 the thread group id number of the kernel thread
 *
 * Notes:       none
 */
osa_thread_id_t osa_get_thread_group_id(osa_thread_t handle)
{
	struct _thread *_thrd = (struct _thread *)handle;

	OSA_ASSERT(_thrd && IS_THREAD_VALID(_thrd));

	return (osa_thread_id_t) (_thrd->task->tgid);
}
OSA_EXPORT_SYMBOL(osa_get_thread_group_id);

/*
 * Name:        osa_get_current_thread_id
 *
 * Description: get the current thread id
 *
 * Params:      none
 *
 * Returns:     osa_thread_id_t - the thread id
 *
 * Notes:       none
 */
osa_thread_id_t osa_get_current_thread_id(void)
{
	return (osa_thread_id_t) (current->pid);
}
OSA_EXPORT_SYMBOL(osa_get_current_thread_id);

/*
 * Name:        osa_get_current_thread_group_id
 *
 * Description: get the current thread group id
 *
 * Params:      none
 *
 * Returns:     osa_thread_id_t - the thread group id
 *
 * Notes:       none
 */
osa_thread_id_t osa_get_current_thread_group_id(void)
{
	return (osa_thread_id_t) (current->tgid);
}
OSA_EXPORT_SYMBOL(osa_get_current_thread_group_id);
