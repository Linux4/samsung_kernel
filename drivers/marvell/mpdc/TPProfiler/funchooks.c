/*
** (C) Copyright 2011 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd.
** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
** You may use, redistribute and/or modify this File in accordance with the terms and
** conditions of the License, a copy of which is available along with the File in the
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
** The License provides additional details about this warranty disclaimer.
*/

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/cpufreq.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/mqueue.h>
#include <linux/ipc_namespace.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/traps.h>

#include "ufunc_hook.h"
#include "ufunc_hook_internal.h"
#include "tp_drv.h"
#include "tp_dsa.h"
#include "vdkiocode.h"
#include "TPProfilerSettings.h"
#include "common.h"
#include "ring_buffer.h"
#include "PXD_tp.h"

#ifndef DFLT_MSGMAX
#define DFLT_MSGMAX 10          /* kernel prior to 2.6.29, DFLT_MSGMAX is not defined in the header files */
#endif

#define SEM_MAX_VALUE 0x7FFFFFFF        /* it is equal to SEM_VALUE_MAX defined in glibc */

static DEFINE_MUTEX(write_mutex);
static DEFINE_SPINLOCK(thread_exit_lock);

int notify_thread_exit(struct task_struct * task,
                       unsigned int         pc,
                       unsigned long long   ts,
                       int                  ret_value,
                       bool                 real_exit);

struct pthread_tid_pair
{
	struct hlist_node link;
	unsigned int      pthread_id;
	pid_t             tid;
};

#define PTHREAD_TID_HASH 100
static struct hlist_head pthread_tid_lists[PTHREAD_TID_HASH];

struct user_thread
{
	struct hlist_node   link;

	struct task_struct * parent;            /* the task which creates the thread */
	pid_t               tid;                /* the new created thread id */
	unsigned long long  ts;                 /* the timestamp when the thread is created */
};



#define USER_THREAD_HASH 100
static struct hlist_head user_thread_lists[USER_THREAD_HASH];

struct exit_thread
{
	struct hlist_node   link;

	struct task_struct * task;            /* the task to exit */
};

#define EXIT_THREAD_HASH 100
static struct hlist_head exit_thread_lists[EXIT_THREAD_HASH];

struct f_hook
{
	const char *      func_name;
	ufunc_hook_pre_handler_t    entry_handler;          /* the callback function when function calls */
	ufunc_hook_return_handler_t return_handler;         /* the callback function when function returns, set to NULL if don't want to hook on exit */
};



static int get_pthread_tid_hash(unsigned int data)
{
	return data % PTHREAD_TID_HASH;
}

static bool find_tid_from_pthread(unsigned int pthread_id, pid_t * tid)
{
	int key;
	struct hlist_node *n;
	struct pthread_tid_pair    *pair;

	if (tid == NULL)
	{
		return false;
	}

	key = get_pthread_tid_hash(pthread_id);

	if (hlist_empty(&pthread_tid_lists[key]))
	{
		return false;
	}

	hlist_for_each(n, &pthread_tid_lists[key])
	{
		pair = hlist_entry(n, struct pthread_tid_pair, link);

		if (pair->pthread_id == pthread_id)
		{
			*tid = pair->tid;
			return true;
		}
	}

	return false;
}

static bool add_pthread_tid_pair(unsigned int pthread_id, pid_t tid)
{
	int key;
	struct pthread_tid_pair *pair;
	struct hlist_head *h;
	struct hlist_node *n;

	key = get_pthread_tid_hash(pthread_id);

	h = &pthread_tid_lists[key];

	/* first check if the pthread_id exists, if so update the tid */
	if (!hlist_empty(h))
	{
		hlist_for_each(n, h)
		{
			pair = hlist_entry(n, struct pthread_tid_pair, link);

			if (pair->pthread_id == pthread_id)
			{
				pair->tid = tid;
				return true;
			}
		}
	}

	pair = kzalloc(sizeof(struct pthread_tid_pair), GFP_ATOMIC);

	if (pair == NULL)
	{
		return false;
	}

	pair->pthread_id = pthread_id;
	pair->tid        = tid;

	hlist_add_head(&pair->link, h);

	return true;
}

static int clear_pthread_tid_lists(void)
{
	int i;

	struct pthread_tid_pair *ptp;
	struct hlist_node * node, * tmp;
	struct hlist_head * head;

	for (i=0; i<PTHREAD_TID_HASH; i++)
	{
		head = &pthread_tid_lists[i];

		if (hlist_empty(head))
			continue;

		hlist_for_each_entry_safe(ptp, node, tmp, head, link)
		{
			hlist_del(&ptp->link);
			kfree(ptp);
		}
	}

	return 0;
}


static int get_user_thread_hash(unsigned int data)
{
	return data % USER_THREAD_HASH;
}

/*
 * find the first new thread which is created after timestamp
 *  paramters:
 *     [in]  parent:  which task creates the thread
 *     [in]  ts:    the timestamp
 *     [out] new_tid: pointer to the address which receives for the new created thread id
 */
static bool find_first_user_thread_after(struct task_struct * parent, unsigned long long ts, pid_t * new_tid)
{
	int key;
	struct user_thread *thd;
	struct user_thread *result = NULL;
	struct hlist_node  *n;
	unsigned long long min_ts;
	bool found;

	if (new_tid == NULL)
	{
		return false;
	}

	min_ts = (unsigned long long)-1;
	found = false;

	key = get_user_thread_hash(parent->pid);

	if (hlist_empty(&user_thread_lists[key]))
	{
		return false;
	}

	hlist_for_each(n, &user_thread_lists[key])
	{
		thd = hlist_entry(n, struct user_thread, link);

		if ((thd->parent == parent) && (thd->ts >= ts))
		{
			found = true;

			if (thd->ts < min_ts)
			{
				result = thd;
				min_ts = thd->ts;
			}
		}

	}

	if (found && (result != NULL))
	{
		*new_tid = result->tid;
		return true;
	}

	return false;
}

static int get_exit_thread_hash(unsigned int data)
{
	return data % EXIT_THREAD_HASH;
}


/*
 * add the new exit thread information to the list
 *  parameters:
 *      task:      the task of the thread exiting
 *      ts:        the timestamp when the new thread is created
 */
static int add_exit_thread(struct task_struct * task)
{
	int key;
	struct exit_thread * thd;
	struct hlist_head  * h;

	key = get_exit_thread_hash(task->pid);

	h = &exit_thread_lists[key];

	thd = kzalloc(sizeof(struct exit_thread), GFP_ATOMIC);

	if (thd == NULL)
	{
		return -ENOMEM;
	}

	thd->task = task;

	hlist_add_head(&thd->link, h);

	return 0;
}

static bool find_exit_thread(struct task_struct * task)
{
	int key;
	struct exit_thread * thd;
	struct hlist_head  * head;
	struct hlist_node  * n, * tmp;

	if (task == NULL)
	{
		return false;
	}

	key = get_exit_thread_hash(task->pid);

	if (hlist_empty(&exit_thread_lists[key]))
	{
		return false;
	}

	head = &exit_thread_lists[key];

	hlist_for_each_entry_safe(thd, n, tmp, head, link)
	{
		if (thd->task->pid == task->pid)
		{
			return true;
		}
	}

	return false;
}

/*
 * clear the list containing the exit thread information
 */
static void clear_exit_thread_lists(void)
{
	int i;
	struct exit_thread *thd;
	struct hlist_node *n, *tmp;

	for (i=0; i<EXIT_THREAD_HASH; i++)
	{
		struct hlist_head *head = &exit_thread_lists[i];

		if (hlist_empty(head))
			continue;

		hlist_for_each_entry_safe(thd, n, tmp, head, link)
		{
			hlist_del(&thd->link);
			kfree(thd);
		}
	}

}

/*
 * add the new created thread information to the list
 *  parameters:
 *      task:      the task of the new thread
 *      parent:    the task which create the thread
 *      ts:        the timestamp when the new thread is created
 */
static int add_user_thread(struct task_struct * task, struct task_struct * parent, unsigned long long ts)
{
	int key;
	struct user_thread * thd;
	struct hlist_head  * h;

	key = get_user_thread_hash(parent->pid);

	h = &user_thread_lists[key];

	thd = kzalloc(sizeof(struct user_thread), GFP_ATOMIC);

	if (thd == NULL)
	{
		return -ENOMEM;
	}

	thd->parent = parent;
	thd->tid    = task->pid;
	thd->ts     = ts;

	hlist_add_head(&thd->link, h);

	return 0;
}

static int remove_user_thread(struct task_struct * task)
{
	int key;
	struct user_thread * thd;
	struct hlist_head  * head;
	struct hlist_node  * n, * tmp;

	if (task == NULL)
	{
		return -EINVAL;
	}

	key = get_user_thread_hash(task->parent->pid);

	head = &user_thread_lists[key];

	hlist_for_each_entry_safe(thd, n, tmp, head, link)
	{
		if ((thd->parent == task) && (thd->tid == task->pid))
		{
			hlist_del(&thd->link);
			kfree(thd);

			return 0;
		}
	}

	return -EINVAL;
}

/*
 * clear the list containing the user thread information
 */
static void clear_user_thread_lists(void)
{
	int i;
	struct user_thread *thd;
	struct hlist_node *n, *tmp;

	for (i=0; i<USER_THREAD_HASH; i++)
	{
		struct hlist_head *head = &user_thread_lists[i];

		if (hlist_empty(head))
			continue;

		hlist_for_each_entry_safe(thd, n, tmp, head, link)
		{
			hlist_del(&thd->link);
			kfree(thd);
		}
	}

}

static bool read_inst_before(struct task_struct * task, unsigned int address, unsigned int *inst, int *inst_size)
{
	unsigned int data;
	int size;

	data = 0;
	size = get_inst_size(task, address - 4);

	//access_process_vm_func(task, (address - size) & ~0x1, &data, size, 0);
	read_proc_vm(task, (address - size) & ~0x1, &data, size);

	if (inst != NULL)
	{
		*inst = 0;
		memcpy(inst, &data, size);
	}

	if (inst_size != NULL)
	{
		*inst_size = 0;
		memcpy(inst_size, &size, sizeof(size));
	}

	return true;
}

static void request_flush_event_buffer(void)
{
	if (!g_event_buffer_tp.is_full_event_set)
	{
		g_event_buffer_tp.is_full_event_set = true;

		if (waitqueue_active(&pxtp_kd_wait))
			wake_up_interruptible(&pxtp_kd_wait);
	}

}

static void write_event(PXD32_TP_Event * et, void * info, int info_size)
{
	bool buffer_full;
	bool need_flush;
	
	/* we need to use the mutex to protect writing events */
	mutex_lock(&write_mutex);

	write_ring_buffer(&g_event_buffer_tp.buffer, et, offsetof(PXD32_TP_Event, eventData), &buffer_full, &need_flush);
	write_ring_buffer(&g_event_buffer_tp.buffer, info, info_size, &buffer_full, &need_flush);

	mutex_unlock(&write_mutex);

	if (need_flush)
		request_flush_event_buffer();
}

#if 0
static int write_thread_static_enum_info(struct task_struct *task)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Enum info;

	et.eventType = TP_EVENT_THREAD_ENUM;
	et.dataSize  = sizeof(PXD32_TP_Thread_Enum);

	info.pid      = task->tgid;
	info.tid      = task->pid;
	info.priority = 0;

	if (task->state == TASK_RUNNING)
	{
		info.state = THREAD_STATE_RUNNING;
	}
	else if (task->state & TASK_INTERRUPTIBLE)
	{
		info.state = THREAD_STATE_BLOCKED;
	}
	else if (task->state & TASK_UNINTERRUPTIBLE)
	{
		info.state = THREAD_STATE_BLOCKED;
	}
	else if (task_is_stopped_or_traced(task))
	{
		info.state = THREAD_STATE_BLOCKED;
	}
	else
	{
		return 0;
	}

	write_event(&et, &info, sizeof(info));

	request_flush_event_buffer();

	return 0;
}
#endif

int write_thread_create_info(struct task_struct * task,
                                    unsigned int pc,
                                    unsigned long long ts,
                                    pid_t new_pid,
                                    pid_t new_tid,
                                    unsigned int priority)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Create info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_CREATE;
	et.dataSize  = sizeof(PXD32_TP_Thread_Create);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.newPid      = new_pid;
	info.newTid      = new_tid;
	info.priority    = priority;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_exit_info(struct task_struct * task,
                                  unsigned int pc,
                                  unsigned long long ts,
                                  int ret_value)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Exit info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_EXIT;
	et.dataSize  = sizeof(PXD32_TP_Thread_Exit);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.returnValue = ret_value;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_process_create_info(struct task_struct * parent,
                                     struct task_struct * task,
                                     unsigned int         pc,
                                     unsigned long long   ts)
{
	int i = 0;
	struct task_struct * t;

	/* for each thread in the process, write the thread create info */
	write_thread_create_info(parent, pc, ts, task->tgid, task->pid, 0);
	write_thread_create_info(task, pc, ts, task->tgid, task->pid, 0);
	
	for (t = next_thread(task); t != task; t = next_thread(t))
	{
		write_thread_create_info(task, pc, ts + i + 1, t->tgid, t->pid, 0);
		i++;
	}
	
	return 0;
}

static int write_process_exit_info(struct task_struct * task,
                                   unsigned int         pc,
                                   unsigned long long   ts,
                                   int                  ret_value,
                                   bool                 real_exit)
{
	struct task_struct * t;

	/* for each thread in the process, write the thread exit info */
	for (t = next_thread(task); t != task; t = next_thread(t))
	{
		notify_thread_exit(t, pc, ts, ret_value, real_exit);
	}
	
	notify_thread_exit(task, pc, ts, ret_value, real_exit);
	
	return 0;
}
			      
static int write_thread_join_begin_info(struct task_struct * task,
                                        unsigned int pc,
                                        unsigned long long ts,
                                        pid_t target_tid)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Join_Begin info;

	if (task == NULL)
	{
		return -EINVAL;
	}

	et.eventType = TP_EVENT_THREAD_JOIN_BEGIN;
	et.dataSize  = sizeof(PXD32_TP_Thread_Join_Begin);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.targetTid    = target_tid;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_join_end_info(struct task_struct * task,
                                      unsigned int pc,
                                      unsigned long long ts,
                                      pid_t target_tid,
                                      unsigned int thread_return,
                                      unsigned int ret_value)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Join_End info;

	if (task == NULL)
	{
		return -EINVAL;
	}

	et.eventType = TP_EVENT_THREAD_JOIN_END;
	et.dataSize  = sizeof(PXD32_TP_Thread_Join_End);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.targetTid    = target_tid;
	info.threadReturn = thread_return;
	info.returnValue  = ret_value;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_wait_sync_begin_info(struct task_struct * task,
                                             unsigned int pc,
                                             unsigned long long ts,
                                             unsigned int obj_id,
                                             unsigned int obj_type,
                                             unsigned int obj_subtype)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Wait_SyncObj_Begin info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_WAIT_SYNC_BEGIN;
	et.dataSize  = sizeof(PXD32_TP_Thread_Wait_SyncObj_Begin);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);
	
	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_wait_sync_end_info(struct task_struct * task,
                                           unsigned int pc,
                                           unsigned long long ts,
                                           unsigned int obj_id,
                                           unsigned int obj_type,
                                           unsigned int obj_subtype,
                                           unsigned int ret_value)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Wait_SyncObj_End info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_WAIT_SYNC_END;
	et.dataSize  = sizeof(PXD32_TP_Thread_Wait_SyncObj_End);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	info.returnValue = ret_value;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_try_wait_sync_begin_info(struct task_struct * task,
                                                 unsigned int pc,
                                                 unsigned long long ts,
                                                 unsigned int obj_id,
                                                 unsigned int obj_type,
                                                 unsigned int obj_subtype)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Try_Wait_SyncObj_Begin info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_TRY_WAIT_SYNC_BEGIN;
	et.dataSize  = sizeof(PXD32_TP_Thread_Try_Wait_SyncObj_Begin);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_try_wait_sync_end_info(struct task_struct * task,
                                               unsigned int pc,
                                               unsigned long long ts,
                                               unsigned int obj_id,
                                               unsigned int obj_type,
                                               unsigned int obj_subtype,
                                               int ret_value)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Try_Wait_SyncObj_End info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_TRY_WAIT_SYNC_END;
	et.dataSize  = sizeof(PXD32_TP_Thread_Try_Wait_SyncObj_End);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	info.returnValue = ret_value;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_timed_wait_sync_begin_info(struct task_struct * task,
                                                   unsigned int pc,
                                                   unsigned long long ts,
                                                   unsigned int obj_id,
                                                   unsigned int obj_type,
                                                   unsigned int obj_subtype)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Timed_Wait_SyncObj_Begin info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_TIMED_WAIT_SYNC_BEGIN;
	et.dataSize  = sizeof(PXD32_TP_Thread_Timed_Wait_SyncObj_Begin);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_timed_wait_sync_end_info(struct task_struct * task,
                                                 unsigned int pc,
                                                 unsigned long long ts,
                                                 unsigned int obj_id,
                                                 unsigned int obj_type,
                                                 unsigned int obj_subtype,
                                                 int ret_value)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Timed_Wait_SyncObj_End info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_TIMED_WAIT_SYNC_END;
	et.dataSize  = sizeof(PXD32_TP_Thread_Timed_Wait_SyncObj_End);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	info.returnValue = ret_value;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_spin_sync_begin_info(struct task_struct * task,
                                             unsigned int pc,
                                             unsigned long long ts,
                                             unsigned int obj_id,
                                             unsigned int obj_type,
                                             unsigned int obj_subtype)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Wait_SyncObj_Begin info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_WAIT_SYNC_BEGIN;
	et.dataSize  = sizeof(PXD32_TP_Thread_Wait_SyncObj_Begin);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_spin_sync_end_info(struct task_struct * task,
                                           unsigned int pc,
                                           unsigned long long ts,
                                           unsigned int obj_id,
                                           unsigned int obj_type,
                                           unsigned int obj_subtype,
                                           int ret_value)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Wait_SyncObj_End info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_WAIT_SYNC_END;
	et.dataSize  = sizeof(PXD32_TP_Thread_Wait_SyncObj_End);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	info.returnValue = ret_value;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_try_spin_sync_begin_info(struct task_struct * task,
                                                 unsigned int pc,
                                                 unsigned long long ts,
                                                 unsigned int obj_id,
                                                 unsigned int obj_type,
                                                 unsigned int obj_subtype)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Try_Wait_SyncObj_Begin info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_TRY_WAIT_SYNC_BEGIN;
	et.dataSize  = sizeof(PXD32_TP_Thread_Try_Wait_SyncObj_Begin);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_try_spin_sync_end_info(struct task_struct * task,
                                               unsigned int pc,
                                               unsigned long long ts,
                                               unsigned int obj_id,
                                               unsigned int obj_type,
                                               unsigned int obj_subtype,
                                               int ret_value)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Try_Wait_SyncObj_End info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_TRY_WAIT_SYNC_END;
	et.dataSize  = sizeof(PXD32_TP_Thread_Try_Wait_SyncObj_End);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	info.returnValue = ret_value;

	write_event(&et, &info, sizeof(info));

	return 0;
}

#if 0
static int write_thread_timed_spin_sync_begin_info(struct task_struct * task,
                                                   unsigned int pc,
                                                   unsigned long long ts,
                                                   unsigned int obj_id,
                                                   unsigned int obj_type,
                                                   unsigned int obj_subtype)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Timed_Wait_SyncObj_Begin info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_TIMED_WAIT_SYNC_BEGIN;
	et.dataSize  = sizeof(PXD32_TP_Thread_Timed_Wait_SyncObj_Begin);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_timed_spin_sync_end_info(struct task_struct * task,
                                                 unsigned int pc,
                                                 unsigned long long ts,
                                                 unsigned int obj_id,
                                                 unsigned int obj_type,
                                                 unsigned int obj_subtype,
                                                 int ret_value)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Timed_Wait_SyncObj_End info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_TIMED_WAIT_SYNC_END;
	et.dataSize  = sizeof(PXD32_TP_Thread_Timed_Wait_SyncObj_End);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	info.returnValue = ret_value;

	write_event(&et, &info, sizeof(info));

	return 0;
}
#endif

static int write_thread_release_sync_info(struct task_struct * task,
                                          unsigned int pc,
                                          unsigned long long ts,
                                          unsigned int obj_id,
                                          unsigned int obj_type,
                                          unsigned int obj_subtype)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Release_SyncObj info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_RELEASE_SYNC;
	et.dataSize  = sizeof(PXD32_TP_Thread_Release_SyncObj);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);
	
	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int write_thread_broadcast_release_sync_info(struct task_struct * task,
                                                    unsigned int pc,
                                                    unsigned long long ts,
                                                    unsigned int obj_id,
                                                    unsigned int obj_type,
                                                    unsigned int obj_subtype)
{
	PXD32_TP_Event et;
	PXD32_TP_Thread_Broadcast_Release_SyncObj info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_THREAD_BROADCAST_RELEASE_SYNC;
	et.dataSize  = sizeof(PXD32_TP_Thread_Broadcast_Release_SyncObj);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	write_event(&et, &info, sizeof(info));

	return 0;
}


static int write_sync_obj_create_info(struct task_struct * task,
                                      unsigned int pc,
                                      unsigned long long ts,
                                      unsigned int obj_id,
                                      unsigned int obj_type,
                                      unsigned int obj_subtype,
                                      const char * obj_name,
                                      unsigned int init_count,
                                      unsigned int max_count)
{
	PXD32_TP_Event et;
	PXD32_TP_SyncObj_Create * info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_SYNC_CREATE;

	if (obj_name == NULL)
		et.dataSize = sizeof(PXD32_TP_SyncObj_Create);
	else
		et.dataSize  = sizeof(PXD32_TP_SyncObj_Create) + strlen(obj_name) + 1;

	info = kzalloc(et.dataSize, GFP_ATOMIC);
	
	if (info == NULL)
		return -ENOMEM;

	info->context.pid = task->tgid;
	info->context.tid = task->pid;
	info->context.pc  = pc;
	info->context.ts  = convert_to_PXD32_DWord(ts);

	info->objectId      = obj_id;
	info->objectType    = obj_type;
	info->objectSubType = obj_subtype;
	info->initCount     = init_count;
	info->maxCount      = max_count;

	if (obj_name != NULL)
	{
		strcpy(info->name, obj_name);
	}

	write_event(&et, info, et.dataSize);

	if (info != NULL)
		kfree(info);
	
	return 0;
}

static int write_sync_obj_open_info(struct task_struct * task,
                                    unsigned int pc,
                                    unsigned long long ts,
                                    unsigned int obj_id,
                                    unsigned int obj_type,
                                    unsigned int obj_subtype,
                                    const char * obj_name)
{
	PXD32_TP_Event et;
	PXD32_TP_SyncObj_Open * info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_SYNC_OPEN;

	if (obj_name == NULL)
		et.dataSize = sizeof(PXD32_TP_SyncObj_Open);
	else
		et.dataSize  = sizeof(PXD32_TP_SyncObj_Open) + strlen(obj_name) + 1;

	info = kzalloc(et.dataSize, GFP_ATOMIC);
	
	if (info == NULL)
		return -ENOMEM;

	info->context.pid = task->tgid;
	info->context.tid = task->pid;
	info->context.pc  = pc;
	info->context.ts  = convert_to_PXD32_DWord(ts);

	info->objectId      = obj_id;
	info->objectType    = obj_type;
	info->objectSubType = obj_subtype;

	if (obj_name != NULL)
	{
		strcpy(info->name, obj_name);
	}

	write_event(&et, info, et.dataSize);

	if (info != NULL)
		kfree(info);
	
	return 0;
}

static int write_sync_obj_destroy_info(struct task_struct * task,
                                       unsigned int pc,
                                       unsigned long long ts,
                                       unsigned int obj_id,
                                       unsigned int obj_type,
                                       unsigned int obj_subtype)
{
	PXD32_TP_Event et;
	PXD32_TP_SyncObj_Destroy info;

	if (task == NULL)
		return -EINVAL;

	et.eventType = TP_EVENT_SYNC_DESTROY;
	et.dataSize  = sizeof(PXD32_TP_SyncObj_Destroy);

	info.context.pid = task->tgid;
	info.context.tid = task->pid;
	info.context.pc  = pc;
	info.context.ts  = convert_to_PXD32_DWord(ts);

	info.objectId      = obj_id;
	info.objectType    = obj_type;
	info.objectSubType = obj_subtype;

	write_event(&et, &info, sizeof(info));

	return 0;
}

static int user_thread_entry_cb(struct task_struct * task,
                                 unsigned int         orig_inst,
                                 int                  orig_inst_size,
                                 struct pt_regs     * entry_regs,
                                 unsigned long long   entry_ts)
{
	return 0;
}

static int user_thread_exit_cb(struct task_struct * task,
                               unsigned int         orig_inst,
                               int                  orig_inst_size,
                               struct pt_regs     * exit_regs,
                               unsigned long long   exit_ts,
                               struct pt_regs     * entry_regs,
                               unsigned long long   entry_ts)
{
	unsigned int ret_value;
	unsigned int address;
	
	address = exit_regs->ARM_pc;
	ret_value = exit_regs->ARM_r0;

	notify_thread_exit(task, address, exit_ts, ret_value, true);

	return 0;
}

static int pthread_create_entry_cb(struct task_struct * task,
                                   unsigned int         orig_inst,
                                   int                  orig_inst_size,
                                   struct pt_regs     * entry_regs,
                                   unsigned long long   entry_ts)
{
	unsigned int thread_func;
	struct ufunc_hook * hook;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	/*
	 * we need to hook at thread function entry and thread function return 
	 * to get thread exit notification if pthread_exit() is not called
	 */
	thread_func = entry_regs->ARM_r2;

	hook = kzalloc(sizeof(struct ufunc_hook), GFP_ATOMIC);
        
	if (hook == NULL)
		return 0;
        
	init_user_func_hook(hook);
        
	hook->tgid  = task->tgid;
        
	hook->hook_addr      = thread_func;
	hook->pre_handler    = user_thread_entry_cb;
	hook->return_handler = user_thread_exit_cb;
	hook->flag           = UFUNC_HOOK_FLAG_THREAD_FUNC;
        
	raw_register_user_func_hook(hook);
		
	//spin_lock(&thread_create_lock);
	return 0;
}

static int pthread_create_return_cb(struct task_struct * task,
                                    unsigned int         orig_inst,
                                    int                  orig_inst_size,
                                    struct pt_regs     * exit_regs,
                                    unsigned long long   exit_ts,
                                    struct pt_regs     * entry_regs,
                                    unsigned long long   entry_ts)
{
	unsigned int pthread_id;
	pid_t new_tid;
	unsigned int address;
	unsigned int size;

	//spin_unlock(&thread_create_lock);

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
	{
		return -EINVAL;
	}

	/* get the posix thread id returned by pthread_create() */
	//access_process_vm_func(task, entry_regs->ARM_r0, &pthread_id, sizeof(pthread_id), 0);
	read_proc_vm(task, entry_regs->ARM_r0, &pthread_id, sizeof(pthread_id));

	if (!find_first_user_thread_after(task, entry_ts, &new_tid))
	{
		return 0;
	}

	add_pthread_tid_pair(pthread_id, new_tid);

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_create_info(task, address - size, entry_ts, task->tgid, new_tid, 0);

	return 0;
}

static int pthread_exit_entry_cb(struct task_struct * task,
                                 unsigned int         orig_inst,
                                 int                  orig_inst_size,
                                 struct pt_regs     * entry_regs,
                                 unsigned long long   entry_ts)
{
	unsigned int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;
	ret_value = entry_regs->ARM_r0;

	read_inst_before(task, address, NULL, &size);

	notify_thread_exit(task, address - size, entry_ts, ret_value, true);
	
	return 0;
}

static int pthread_join_entry_cb(struct task_struct * task,
                                 unsigned int         orig_inst,
                                 int                  orig_inst_size,
                                 struct pt_regs     * entry_regs,
                                 unsigned long long   entry_ts)
{
	unsigned int pthread_id;
	pid_t tid;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	pthread_id = entry_regs->ARM_r0;

	if (!find_tid_from_pthread(pthread_id, &tid))
	{
		return 0;
	}

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_join_begin_info(task, address - size, entry_ts, tid);

	return 0;
}


static int pthread_join_return_cb(struct task_struct * task,
                                  unsigned int         orig_inst,
                                  int                  orig_inst_size,
                                  struct pt_regs     * exit_regs,
                                  unsigned long long   exit_ts,
                                  struct pt_regs     * entry_regs,
                                  unsigned long long   entry_ts)
{
	unsigned int pthread_id;
	unsigned int thread_return;
	int ret_value;
	unsigned int address;
	unsigned int size;

	pid_t tid;
	
	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	thread_return = entry_regs->ARM_r1;

	//access_process_vm_func(task, entry_regs->ARM_r1, &thread_return, sizeof(thread_return), 0);
	read_proc_vm(task, entry_regs->ARM_r1, &thread_return, sizeof(thread_return));

	pthread_id = entry_regs->ARM_r0;

	if (!find_tid_from_pthread(pthread_id, &tid))
		return 0;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	
	write_thread_join_end_info(task, address - size, exit_ts, tid, thread_return, ret_value);

	return 0;
}

/* mutex */
static int pthread_mutex_init_entry_cb(struct task_struct * task,
                                       unsigned int         orig_inst,
                                       int                  orig_inst_size,
                                       struct pt_regs     * entry_regs,
                                       unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_mutex_init_return_cb(struct task_struct * task,
                                        unsigned int         orig_inst,
                                        int                  orig_inst_size,
                                        struct pt_regs     * exit_regs,
                                        unsigned long long   exit_ts,
                                        struct pt_regs     * entry_regs,
                                        unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MUTEX, 0, NULL, 1, 1);

	return 0;
}

static int pthread_mutex_destroy_entry_cb(struct task_struct * task,
                                          unsigned int         orig_inst,
                                          int                  orig_inst_size,
                                          struct pt_regs     * entry_regs,
                                          unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_mutex_destroy_return_cb(struct task_struct * task,
                                           unsigned int         orig_inst,
                                           int                  orig_inst_size,
                                           struct pt_regs     * exit_regs,
                                           unsigned long long   exit_ts,
                                           struct pt_regs     * entry_regs,
                                           unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_destroy_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MUTEX, 0);

	return 0;
}

static int pthread_mutex_lock_entry_cb(struct task_struct * task,
                                       unsigned int         orig_inst,
                                       int                  orig_inst_size,
                                       struct pt_regs     * entry_regs,
                                       unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MUTEX, 0);

	return 0;
}

static int pthread_mutex_lock_return_cb(struct task_struct * task,
                                        unsigned int         orig_inst,
                                        int                  orig_inst_size,
                                        struct pt_regs     * exit_regs,
                                        unsigned long long   exit_ts,
                                        struct pt_regs     * entry_regs,
                                        unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
	{
		return -EINVAL;
	}

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	
	write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MUTEX, 0, ret_value);

	return 0;
}

static int pthread_mutex_unlock_entry_cb(struct task_struct * task,
                                         unsigned int         orig_inst,
                                         int                  orig_inst_size,
                                         struct pt_regs     * entry_regs,
                                         unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_mutex_unlock_return_cb(struct task_struct * task,
                                          unsigned int         orig_inst,
                                          int                  orig_inst_size,
                                          struct pt_regs     * exit_regs,
                                          unsigned long long   exit_ts,
                                          struct pt_regs     * entry_regs,
                                          unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MUTEX, 0);

	return 0;
}

static int pthread_mutex_trylock_entry_cb(struct task_struct * task,
                                          unsigned int         orig_inst,
                                          int                  orig_inst_size,
                                          struct pt_regs     * entry_regs,
                                          unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_try_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MUTEX, 0);

	return 0;
}


static int pthread_mutex_trylock_return_cb(struct task_struct * task,
                                           unsigned int         orig_inst,
                                           int                  orig_inst_size,
                                           struct pt_regs     * exit_regs,
                                           unsigned long long   exit_ts,
                                           struct pt_regs     * entry_regs,
                                           unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_try_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MUTEX, 0, ret_value);

	return 0;
}

static int pthread_mutex_timedlock_entry_cb(struct task_struct * task,
                                            unsigned int         orig_inst,
                                            int                  orig_inst_size,
                                            struct pt_regs     * entry_regs,
                                            unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	if (entry_regs->ARM_r1 != 0)
		write_thread_timed_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MUTEX, 0);
	else
		write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MUTEX, 0);

	return 0;
}


static int pthread_mutex_timedlock_return_cb(struct task_struct * task,
                                             unsigned int         orig_inst,
                                             int                  orig_inst_size,
                                             struct pt_regs     * exit_regs,
                                             unsigned long long   exit_ts,
                                             struct pt_regs     * entry_regs,
                                             unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	
	if (entry_regs->ARM_r1 != 0)
		write_thread_timed_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MUTEX, 0, ret_value);
	else
		write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MUTEX, 0, ret_value);

	return 0;
}

/* spin lock */
static int pthread_spin_init_entry_cb(struct task_struct * task,
                                      unsigned int         orig_inst,
                                      int                  orig_inst_size,
                                      struct pt_regs     * entry_regs,
                                      unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_spin_init_return_cb(struct task_struct * task,
                                       unsigned int         orig_inst,
                                       int                  orig_inst_size,
                                       struct pt_regs     * exit_regs,
                                       unsigned long long   exit_ts,
                                       struct pt_regs     * entry_regs,
                                       unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SPIN_LOCK, 0, NULL, 1, 1);

	return 0;
}

static int pthread_spin_destroy_entry_cb(struct task_struct * task,
                                         unsigned int         orig_inst,
                                         int                  orig_inst_size,
                                         struct pt_regs     * entry_regs,
                                         unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_spin_destroy_return_cb(struct task_struct * task,
                                          unsigned int         orig_inst,
                                          int                  orig_inst_size,
                                          struct pt_regs     * exit_regs,
                                          unsigned long long   exit_ts,
                                          struct pt_regs     * entry_regs,
                                          unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_destroy_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SPIN_LOCK, 0);

	return 0;
}

static int pthread_spin_lock_entry_cb(struct task_struct * task,
                                      unsigned int         orig_inst,
                                      int                  orig_inst_size,
                                      struct pt_regs     * entry_regs,
                                      unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_spin_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SPIN_LOCK, 0);

	return 0;
}

static int pthread_spin_lock_return_cb(struct task_struct * task,
                                       unsigned int         orig_inst,
                                       int                  orig_inst_size,
                                       struct pt_regs     * exit_regs,
                                       unsigned long long   exit_ts,
                                       struct pt_regs     * entry_regs,
                                       unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;
	read_inst_before(task, address, NULL, &size);
	write_thread_spin_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SPIN_LOCK, 0, ret_value);

	return 0;
}

static int pthread_spin_unlock_entry_cb(struct task_struct * task,
                                        unsigned int         orig_inst,
                                        int                  orig_inst_size,
                                        struct pt_regs     * entry_regs,
                                        unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_spin_unlock_return_cb(struct task_struct * task,
                                         unsigned int         orig_inst,
                                         int                  orig_inst_size,
                                         struct pt_regs     * exit_regs,
                                         unsigned long long   exit_ts,
                                         struct pt_regs     * entry_regs,
                                         unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SPIN_LOCK, 0);

	return 0;
}

static int pthread_spin_trylock_entry_cb(struct task_struct * task,
                                         unsigned int         orig_inst,
                                         int                  orig_inst_size,
                                         struct pt_regs     * entry_regs,
                                         unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_try_spin_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SPIN_LOCK, 0);

	return 0;
}

static int pthread_spin_trylock_return_cb(struct task_struct * task,
                                          unsigned int         orig_inst,
                                          int                  orig_inst_size,
                                          struct pt_regs     * exit_regs,
                                          unsigned long long   exit_ts,
                                          struct pt_regs     * entry_regs,
                                          unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_try_spin_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SPIN_LOCK, 0, ret_value);

	return 0;
}

/*
 * condition variable
 */
static int pthread_cond_init_entry_cb(struct task_struct * task,
                                      unsigned int         orig_inst,
                                      int                  orig_inst_size,
                                      struct pt_regs     * entry_regs,
                                      unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_cond_init_return_cb(struct task_struct * task,
                                       unsigned int         orig_inst,
                                       int                  orig_inst_size,
                                       struct pt_regs     * exit_regs,
                                       unsigned long long   exit_ts,
                                       struct pt_regs     * entry_regs,
                                       unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0, NULL, 1, 1);

	return 0;
}

static int pthread_cond_destroy_entry_cb(struct task_struct * task,
                                         unsigned int         orig_inst,
                                         int                  orig_inst_size,
                                         struct pt_regs     * entry_regs,
                                         unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_cond_destroy_return_cb(struct task_struct * task,
                                          unsigned int         orig_inst,
                                          int                  orig_inst_size,
                                          struct pt_regs     * exit_regs,
                                          unsigned long long   exit_ts,
                                          struct pt_regs     * entry_regs,
                                          unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_destroy_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0);

	return 0;
}

#ifndef CONFIG_ANDROID
static int pthread_cond_wait_entry_cb(struct task_struct * task,
                                      unsigned int         orig_inst,
                                      int                  orig_inst_size,
                                      struct pt_regs     * entry_regs,
                                      unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;
	read_inst_before(task, entry_regs->ARM_lr, NULL, &size);

	/* first release the mutex */
	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r1, TP_SYNC_OBJ_TYPE_MUTEX, 0);

	/* then try to block on the condition variable */
	write_thread_wait_sync_begin_info(task, address - size, entry_ts + 1, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0);

	return 0;
}

static int pthread_cond_wait_return_cb(struct task_struct * task,
                                       unsigned int         orig_inst,
                                       int                  orig_inst_size,
                                       struct pt_regs     * exit_regs,
                                       unsigned long long   exit_ts,
                                       struct pt_regs     * entry_regs,
                                       unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	/* first block on the condition variable */
	write_thread_wait_sync_end_info(task, address - size, exit_ts - 2, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0, ret_value);

	/* then try to obtain the mutex */
	write_thread_wait_sync_begin_info(task, address - size, exit_ts - 1, entry_regs->ARM_r1, TP_SYNC_OBJ_TYPE_MUTEX, 0);

	/* obtain the mutex successfully */
	write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r1, TP_SYNC_OBJ_TYPE_MUTEX, 0, WAIT_RETURN_SUCCESS);

	return 0;
}
#endif /* CONFIG_ANDROID */

static int pthread_cond_timedwait_entry_cb(struct task_struct * task,
                                           unsigned int         orig_inst,
                                           int                  orig_inst_size,
                                           struct pt_regs     * entry_regs,
                                           unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;
	read_inst_before(task, address, NULL, &size);

	/* first release the mutex */
	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r1, TP_SYNC_OBJ_TYPE_MUTEX, 0);

	/* then try to block on the condition variable */
	if (entry_regs->ARM_r2 != 0)
		write_thread_timed_wait_sync_begin_info(task, address - size, entry_ts + 1, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0);
	else
		write_thread_wait_sync_begin_info(task, address - size, entry_ts + 1, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0);

	return 0;
}

static int pthread_cond_timedwait_return_cb(struct task_struct * task,
                                            unsigned int         orig_inst,
                                            int                  orig_inst_size,
                                            struct pt_regs     * exit_regs,
                                            unsigned long long   exit_ts,
                                            struct pt_regs     * entry_regs,
                                            unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	/* first block on the condition variable */
	if (entry_regs->ARM_r2 != 0)
		write_thread_timed_wait_sync_end_info(task, address - size, exit_ts - 2, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0, ret_value);
	else
		write_thread_wait_sync_end_info(task, address - size, exit_ts - 2, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0, ret_value);

	/* then try to obtain the mutex */
	write_thread_wait_sync_begin_info(task, address - size, exit_ts - 1, entry_regs->ARM_r1, TP_SYNC_OBJ_TYPE_MUTEX, 0);

	/* obtain the mutex successfully */
	write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r1, TP_SYNC_OBJ_TYPE_MUTEX, 0, WAIT_RETURN_SUCCESS);

	return 0;
}

static int pthread_cond_signal_entry_cb(struct task_struct * task,
                                        unsigned int         orig_inst,
                                        int                  orig_inst_size,
                                        struct pt_regs     * entry_regs,
                                        unsigned long long   entry_ts)
{
	return 0;
}


static int pthread_cond_signal_return_cb(struct task_struct * task,
                                         unsigned int         orig_inst,
                                         int                  orig_inst_size,
                                         struct pt_regs     * exit_regs,
                                         unsigned long long   exit_ts,
                                         struct pt_regs     * entry_regs,
                                         unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0);

	return 0;
}

static int pthread_cond_broadcast_entry_cb(struct task_struct * task,
                                           unsigned int         orig_inst,
                                           int                  orig_inst_size,
                                           struct pt_regs     * entry_regs,
                                           unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_cond_broadcast_return_cb(struct task_struct * task,
                                            unsigned int         orig_inst,
                                            int                  orig_inst_size,
                                            struct pt_regs     * exit_regs,
                                            unsigned long long   exit_ts,
                                            struct pt_regs     * entry_regs,
                                            unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_broadcast_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_CONDITION_VARIABLE, 0);

	return 0;
}

/*
 * message queue
 */
#ifdef CONFIG_POSIX_MQUEUE
static int mq_open_entry_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	return 0;
}

static int mq_open_return_cb(struct task_struct * task,
                             unsigned int         orig_inst,
                             int                  orig_inst_size,
                             struct pt_regs     * exit_regs,
                             unsigned long long   exit_ts,
                             struct pt_regs     * entry_regs,
                             unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;
	unsigned int obj_id;
	int oflag;
	char obj_name[NAME_MAX];

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 == -1)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;
	oflag = entry_regs->ARM_r1;

	memset(obj_name, 0, NAME_MAX);

	if (strncpy_from_user(obj_name, (const char *)entry_regs->ARM_r0, NAME_MAX) < 0)
		return -EFAULT;

	read_inst_before(task, address, NULL, &size);

	if (oflag & O_CREAT)
	{
		if ((struct mq_attr *)entry_regs->ARM_r3 == NULL)
		{
			write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0, obj_name, 0, DFLT_MSGMAX);
		}
		else
		{
			struct mq_attr attr;

			attr =  *(struct mq_attr *)entry_regs->ARM_r3;

			write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0, obj_name, 0, attr.mq_maxmsg);

		}
	}
	else
	{
		write_sync_obj_open_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0, obj_name);
	}

	return 0;
}

static int mq_close_entry_cb(struct task_struct * task,
                             unsigned int         orig_inst,
                             int                  orig_inst_size,
                             struct pt_regs     * entry_regs,
                             unsigned long long   entry_ts)
{
	return 0;
}

static int mq_close_return_cb(struct task_struct * task,
                              unsigned int         orig_inst,
                              int                  orig_inst_size,
                              struct pt_regs     * exit_regs,
                              unsigned long long   exit_ts,
                              struct pt_regs     * entry_regs,
                              unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;
	unsigned int obj_id;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_destroy_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0);

	return 0;
}

static int mq_receive_entry_cb(struct task_struct * task,
                               unsigned int         orig_inst,
                               int                  orig_inst_size,
                               struct pt_regs     * entry_regs,
                               unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0);

	return 0;
}

static int mq_receive_return_cb(struct task_struct * task,
                                unsigned int         orig_inst,
                                int                  orig_inst_size,
                                struct pt_regs     * exit_regs,
                                unsigned long long   exit_ts,
                                struct pt_regs     * entry_regs,
                                unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 != -1)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0, ret_value);

	return 0;
}

static int mq_timedreceive_entry_cb(struct task_struct * task,
                                    unsigned int         orig_inst,
                                    int                  orig_inst_size,
                                    struct pt_regs     * entry_regs,
                                    unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_timed_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0);
//**
	return 0;
}

static int mq_timedreceive_return_cb(struct task_struct * task,
                                     unsigned int         orig_inst,
                                     int                  orig_inst_size,
                                     struct pt_regs     * exit_regs,
                                     unsigned long long   exit_ts,
                                     struct pt_regs     * entry_regs,
                                     unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 != -1)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_timed_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0, ret_value);
//**
	return 0;
}

static int mq_send_entry_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	return 0;
}

static int mq_send_return_cb(struct task_struct * task,
                             unsigned int         orig_inst,
                             int                  orig_inst_size,
                             struct pt_regs     * exit_regs,
                             unsigned long long   exit_ts,
                             struct pt_regs     * entry_regs,
                             unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0);

	return 0;
}

static int mq_timedsend_entry_cb(struct task_struct * task,
                                 unsigned int         orig_inst,
                                 int                  orig_inst_size,
                                 struct pt_regs     * entry_regs,
                                 unsigned long long   entry_ts)
{
	return 0;
}


static int mq_timedsend_return_cb(struct task_struct * task,
                                  unsigned int         orig_inst,
                                  int                  orig_inst_size,
                                  struct pt_regs     * exit_regs,
                                  unsigned long long   exit_ts,
                                  struct pt_regs     * entry_regs,
                                  unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0);

	return 0;
}
#endif /* CONFIG_POSIX_MQUEUE */

/*
 * posix semaphore
 */
static int sem_init_entry_cb(struct task_struct * task,
                             unsigned int         orig_inst,
                             int                  orig_inst_size,
                             struct pt_regs     * entry_regs,
                             unsigned long long   entry_ts)
{
	return 0;
}

static int sem_init_return_cb(struct task_struct * task,
                              unsigned int         orig_inst,
                              int                  orig_inst_size,
                              struct pt_regs     * exit_regs,
                              unsigned long long   exit_ts,
                              struct pt_regs     * entry_regs,
                              unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;
	unsigned int init_value;
	bool         shared;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;
	shared  = entry_regs->ARM_r1;
	init_value = entry_regs->ARM_r2;

	read_inst_before(task, address, NULL, &size);

	if (shared)
	{
		write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, TP_SYNC_OBJ_SUBTYPE_PROC_SHARED, NULL, init_value, SEM_MAX_VALUE);
	}
	else
	{
		write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, NULL, init_value, SEM_MAX_VALUE);
	}
		

	return 0;
}

static int sem_open_entry_cb(struct task_struct * task,
                             unsigned int         orig_inst,
                             int                  orig_inst_size,
                             struct pt_regs     * entry_regs,
                             unsigned long long   entry_ts)
{
	return 0;
}

static int sem_open_return_cb(struct task_struct * task,
                              unsigned int         orig_inst,
                              int                  orig_inst_size,
                              struct pt_regs     * exit_regs,
                              unsigned long long   exit_ts,
                              struct pt_regs     * entry_regs,
                              unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;
	unsigned int oflag;
	unsigned int value;
	char obj_name[NAME_MAX];

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 == 0)
		return 0;

	memset(obj_name, 0, NAME_MAX);

	if (strncpy_from_user(obj_name, (const char *)entry_regs->ARM_r0, NAME_MAX) < 0)
		return -EFAULT;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;
	oflag   = entry_regs->ARM_r1;
	value   = entry_regs->ARM_r3;

	read_inst_before(task, address, NULL, &size);

	if (oflag & O_CREAT)
		write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, obj_name, value, SEM_MAX_VALUE);
	else
		write_sync_obj_open_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, obj_name);

	return 0;
}

static int sem_close_entry_cb(struct task_struct * task,
                              unsigned int         orig_inst,
                              int                  orig_inst_size,
                              struct pt_regs     * entry_regs,
                              unsigned long long   entry_ts)
{
	return 0;
}

static int sem_close_return_cb(struct task_struct * task,
                               unsigned int         orig_inst,
                               int                  orig_inst_size,
                               struct pt_regs     * exit_regs,
                               unsigned long long   exit_ts,
                               struct pt_regs     * entry_regs,
                               unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_destroy_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0);

	return 0;
}

static int sem_destroy_entry_cb(struct task_struct * task,
                                unsigned int         orig_inst,
                                int                  orig_inst_size,
                                struct pt_regs     * entry_regs,
                                unsigned long long   entry_ts)
{
	return 0;
}

static int sem_destroy_return_cb(struct task_struct * task,
                                 unsigned int         orig_inst,
                                 int                  orig_inst_size,
                                 struct pt_regs     * exit_regs,
                                 unsigned long long   exit_ts,
                                 struct pt_regs     * entry_regs,
                                 unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_destroy_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0);

	return 0;
}

static int sem_wait_entry_cb(struct task_struct * task,
                             unsigned int         orig_inst,
                             int                  orig_inst_size,
                             struct pt_regs     * entry_regs,
                             unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0);

	return 0;
}

static int sem_wait_return_cb(struct task_struct * task,
                              unsigned int         orig_inst,
                              int                  orig_inst_size,
                              struct pt_regs     * exit_regs,
                              unsigned long long   exit_ts,
                              struct pt_regs     * entry_regs,
                              unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, ret_value);

	return 0;
}

static int sem_trywait_entry_cb(struct task_struct * task,
                                unsigned int         orig_inst,
                                int                  orig_inst_size,
                                struct pt_regs     * entry_regs,
                                unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_try_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0);

	return 0;
}

static int sem_trywait_return_cb(struct task_struct * task,
                                 unsigned int         orig_inst,
                                 int                  orig_inst_size,
                                 struct pt_regs     * exit_regs,
                                 unsigned long long   exit_ts,
                                 struct pt_regs     * entry_regs,
                                 unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;
	read_inst_before(task, address, NULL, &size);
	write_thread_try_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, ret_value);

	return 0;
}

static int sem_timedwait_entry_cb(struct task_struct * task,
                                  unsigned int         orig_inst,
                                  int                  orig_inst_size,
                                  struct pt_regs     * entry_regs,
                                  unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	
	if (entry_regs->ARM_r1 != 0)
		write_thread_timed_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0);
	else
		write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0);

	return 0;
}

static int sem_timedwait_return_cb(struct task_struct * task,
                                   unsigned int         orig_inst,
                                   int                  orig_inst_size,
                                   struct pt_regs     * exit_regs,
                                   unsigned long long   exit_ts,
                                   struct pt_regs     * entry_regs,
                                   unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	if (entry_regs->ARM_r1 != 0)
		write_thread_timed_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, ret_value);
	else
		write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, ret_value);

	return 0;
}

static int sem_post_entry_cb(struct task_struct * task,
                             unsigned int         orig_inst,
                             int                  orig_inst_size,
                             struct pt_regs     * entry_regs,
                             unsigned long long   entry_ts)
{
	return 0;
}

static int sem_post_return_cb(struct task_struct * task,
                              unsigned int         orig_inst,
                              int                  orig_inst_size,
                              struct pt_regs     * exit_regs,
                              unsigned long long   exit_ts,
                              struct pt_regs     * entry_regs,
                              unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0);

	return 0;
}

#if 0
static int msgget_entry_cb(struct task_struct * task,
                           unsigned int         orig_inst,
                           int                  orig_inst_size,
                           struct pt_regs     * entry_regs,
                           unsigned long long   entry_ts)
{
	return 0;
}

static int msgget_return_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * exit_regs,
                            unsigned long long   exit_ts,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;
	unsigned int msgflag;
	unsigned int obj_id;
	unsigned int key;
	char         obj_name[100];

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 <= 0)
		return 0;

	address = entry_regs->ARM_lr;
	obj_id  = exit_regs->ARM_r0;
	key     = entry_regs->ARM_r0;
	msgflag = entry_regs->ARM_r1;

	read_inst_before(task, address, NULL, &size);

	if (key == IPC_PRIVATE)
	{
		write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0, NULL, 1, 1);
	}
	else if (msgflag & IPC_CREAT)
	{
		sprintf(obj_name, "%d", key);

		write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0, obj_name, 1, 1);
	}
	else
	{
		sprintf(obj_name, "%d", key);

		write_sync_obj_open_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0, obj_name);
	}

	return 0;
}


static int msgsnd_entry_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	return 0;
}

static int msgsnd_return_cb(struct task_struct * task,
                             unsigned int         orig_inst,
                             int                  orig_inst_size,
                             struct pt_regs     * exit_regs,
                             unsigned long long   exit_ts,
                             struct pt_regs     * entry_regs,
                             unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0);

	return 0;
}

static int msgrcv_entry_cb(struct task_struct * task,
                           unsigned int         orig_inst,
                           int                  orig_inst_size,
                           struct pt_regs     * entry_regs,
                           unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0);

	return 0;
}

static int msgrcv_return_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * exit_regs,
                            unsigned long long   exit_ts,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 != -1)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0, ret_value);

	return 0;
}

static int msgctl_entry_cb(struct task_struct * task,
                           unsigned int         orig_inst,
                           int                  orig_inst_size,
                           struct pt_regs     * entry_regs,
                           unsigned long long   entry_ts)
{
	return 0;
}

static int msgctl_return_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * exit_regs,
                            unsigned long long   exit_ts,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;
	unsigned int obj_id;
	unsigned int cmd;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	cmd     = entry_regs->ARM_r1;
	address = entry_regs->ARM_lr;

	if (cmd != IPC_RMID)
		return 0;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_destroy_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_MESSAGE_QUEUE, 0);

	return 0;
}

static int semget_entry_cb(struct task_struct * task,
                           unsigned int         orig_inst,
                           int                  orig_inst_size,
                           struct pt_regs     * entry_regs,
                           unsigned long long   entry_ts)
{
	return 0;
}

static int semget_return_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * exit_regs,
                            unsigned long long   exit_ts,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;
	unsigned int key;
	unsigned int nsems;
	unsigned int semflag;
	char         obj_name[100];

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 <= 0)
		return 0;

	obj_id  = exit_regs->ARM_r0;
	address = entry_regs->ARM_lr;
	key     = entry_regs->ARM_r0;
	nsems   = entry_regs->ARM_r1;
	semflag = entry_regs->ARM_r2;

	read_inst_before(task, address, NULL, &size);

	if (key == IPC_PRIVATE)
	{
		write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, NULL, 0, SEM_MAX_VALUE);
	}
	else if (semflag & IPC_CREAT)
	{
		sprintf(obj_name, "%d", key);

		write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, obj_name, 0, SEM_MAX_VALUE);
	}
	else
	{
		sprintf(obj_name, "%d", key);

		write_sync_obj_open_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, obj_name);
	}

	return 0;
}

/* TODO */
static int semop_entry_cb(struct task_struct * task,
                          unsigned int         orig_inst,
                          int                  orig_inst_size,
                          struct pt_regs     * entry_regs,
                          unsigned long long   entry_ts)
{
	return 0;
}

/* TODO */
static int semop_return_cb(struct task_struct * task,
                           unsigned int         orig_inst,
                           int                  orig_inst_size,
                           struct pt_regs     * exit_regs,
                           unsigned long long   exit_ts,
                           struct pt_regs     * entry_regs,
                           unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;
	unsigned int ret_value;
	unsigned int obj_id;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;
	
	obj_id = entry_regs->ARM_r0;

	write_thread_wait_sync_end_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0, ret_value);
	write_thread_release_sync_info(task, address - size, entry_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0);

	return 0;
}

/* TODO */
static int semtimedop_entry_cb(struct task_struct * task,
                          unsigned int         orig_inst,
                          int                  orig_inst_size,
                          struct pt_regs     * entry_regs,
                          unsigned long long   entry_ts)
{
	return 0;
}

/* TODO */
static int semtimedop_return_cb(struct task_struct * task,
                           unsigned int         orig_inst,
                           int                  orig_inst_size,
                           struct pt_regs     * exit_regs,
                           unsigned long long   exit_ts,
                           struct pt_regs     * entry_regs,
                           unsigned long long   entry_ts)
{
	return 0;
}

static int semctl_entry_cb(struct task_struct * task,
                           unsigned int         orig_inst,
                           int                  orig_inst_size,
                           struct pt_regs     * entry_regs,
                           unsigned long long   entry_ts)
{
	return 0;
}

static int semctl_return_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * exit_regs,
                            unsigned long long   exit_ts,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;
	unsigned int cmd;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 == -1)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	cmd     = entry_regs->ARM_r2;
	address = entry_regs->ARM_lr;

	if (cmd != IPC_RMID)
		return 0;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_destroy_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_SEMAPHORE, 0);

	return 0;
}
#endif

/*
 * read/write lock
 */
static int pthread_rwlock_init_entry_cb(struct task_struct * task,
                                        unsigned int         orig_inst,
                                        int                  orig_inst_size,
                                        struct pt_regs     * entry_regs,
                                        unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_rwlock_init_return_cb(struct task_struct * task,
                                         unsigned int         orig_inst,
                                         int                  orig_inst_size,
                                         struct pt_regs     * exit_regs,
                                         unsigned long long   exit_ts,
                                         struct pt_regs     * entry_regs,
                                         unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_create_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_RW_LOCK, 0, NULL, 1, 1);

	return 0;
}

static int pthread_rwlock_destroy_entry_cb(struct task_struct * task,
                                           unsigned int         orig_inst,
                                           int                  orig_inst_size,
                                           struct pt_regs     * entry_regs,
                                           unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_rwlock_destroy_return_cb(struct task_struct * task,
                                            unsigned int         orig_inst,
                                            int                  orig_inst_size,
                                            struct pt_regs     * exit_regs,
                                            unsigned long long   exit_ts,
                                            struct pt_regs     * entry_regs,
                                            unsigned long long   entry_ts)
{
	unsigned int obj_id;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	obj_id  = entry_regs->ARM_r0;
	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_sync_obj_destroy_info(task, address - size, exit_ts, obj_id, TP_SYNC_OBJ_TYPE_RW_LOCK, 0);

	return 0;
}

#ifndef CONFIG_ANDROID
static int pthread_rwlock_rdlock_entry_cb(struct task_struct * task,
                                          unsigned int         orig_inst,
                                          int                  orig_inst_size,
                                          struct pt_regs     * entry_regs,
                                          unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_READ);

	return 0;
}

static int pthread_rwlock_rdlock_return_cb(struct task_struct * task,
                                           unsigned int         orig_inst,
                                           int                  orig_inst_size,
                                           struct pt_regs     * exit_regs,
                                           unsigned long long   exit_ts,
                                           struct pt_regs     * entry_regs,
                                           unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_READ, ret_value);

	return 0;
}
#endif

static int pthread_rwlock_timedrdlock_entry_cb(struct task_struct * task,
                                               unsigned int         orig_inst,
                                               int                  orig_inst_size,
                                               struct pt_regs     * entry_regs,
                                               unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	
	if (entry_regs->ARM_r1 != 0)
		write_thread_timed_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_READ);
	else
	{
		write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_READ);
	}

	return 0;
}

static int pthread_rwlock_timedrdlock_return_cb(struct task_struct * task,
                                                unsigned int         orig_inst,
                                                int                  orig_inst_size,
                                                struct pt_regs     * exit_regs,
                                                unsigned long long   exit_ts,
                                                struct pt_regs     * entry_regs,
                                                unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	if (entry_regs->ARM_r1 != 0)
		write_thread_timed_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_READ, ret_value);
	else
		write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_READ, ret_value);

	return 0;
}

static int pthread_rwlock_tryrdlock_entry_cb(struct task_struct * task,
                                             unsigned int         orig_inst,
                                             int                  orig_inst_size,
                                             struct pt_regs     * entry_regs,
                                             unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_try_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_READ);

	return 0;
}

static int pthread_rwlock_tryrdlock_return_cb(struct task_struct * task,
                                              unsigned int         orig_inst,
                                              int                  orig_inst_size,
                                              struct pt_regs     * exit_regs,
                                              unsigned long long   exit_ts,
                                              struct pt_regs     * entry_regs,
                                              unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_try_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_READ, ret_value);

	return 0;
}

#ifndef CONFIG_ANDROID
static int pthread_rwlock_wrlock_entry_cb(struct task_struct * task,
                                          unsigned int         orig_inst,
                                          int                  orig_inst_size,
                                          struct pt_regs     * entry_regs,
                                          unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);

	write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_WRITE);

	return 0;
}

static int pthread_rwlock_wrlock_return_cb(struct task_struct * task,
                                           unsigned int         orig_inst,
                                           int                  orig_inst_size,
                                           struct pt_regs     * exit_regs,
                                           unsigned long long   exit_ts,
                                           struct pt_regs     * entry_regs,
                                           unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;
	read_inst_before(task, address, NULL, &size);
	write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_WRITE, ret_value);

	return 0;
}
#endif

static int pthread_rwlock_timedwrlock_entry_cb(struct task_struct * task,
                                               unsigned int         orig_inst,
                                               int                  orig_inst_size,
                                               struct pt_regs     * entry_regs,
                                               unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	
	if (entry_regs->ARM_r1 != 0)
		write_thread_timed_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_WRITE);
	else
		write_thread_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_WRITE);

	return 0;
}

static int pthread_rwlock_timedwrlock_return_cb(struct task_struct * task,
                                                unsigned int         orig_inst,
                                                int                  orig_inst_size,
                                                struct pt_regs     * exit_regs,
                                                unsigned long long   exit_ts,
                                                struct pt_regs     * entry_regs,
                                                unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	
	if (entry_regs->ARM_r1 != 0)
		write_thread_timed_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_WRITE, ret_value);
	else
		write_thread_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_WRITE, ret_value);

	return 0;
}

static int pthread_rwlock_trywrlock_entry_cb(struct task_struct * task,
                                             unsigned int         orig_inst,
                                             int                  orig_inst_size,
                                             struct pt_regs     * entry_regs,
                                             unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL))
		return -EINVAL;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_try_wait_sync_begin_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_WRITE);

	return 0;
}

static int pthread_rwlock_trywrlock_return_cb(struct task_struct * task,
                                              unsigned int         orig_inst,
                                              int                  orig_inst_size,
                                              struct pt_regs     * exit_regs,
                                              unsigned long long   exit_ts,
                                              struct pt_regs     * entry_regs,
                                              unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 == 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_try_wait_sync_end_info(task, address - size, exit_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, TP_SYNC_OBJ_SUBTYPE_RW_WRITE, ret_value);

	return 0;
}

static int pthread_rwlock_unlock_entry_cb(struct task_struct * task,
                                          unsigned int         orig_inst,
                                          int                  orig_inst_size,
                                          struct pt_regs     * entry_regs,
                                          unsigned long long   entry_ts)
{
	return 0;
}

static int pthread_rwlock_unlock_return_cb(struct task_struct * task,
                                           unsigned int         orig_inst,
                                           int                  orig_inst_size,
                                           struct pt_regs     * exit_regs,
                                           unsigned long long   exit_ts,
                                           struct pt_regs     * entry_regs,
                                           unsigned long long   entry_ts)
{
	unsigned int address;
	unsigned int size;

	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	if (exit_regs->ARM_r0 != 0)
		return 0;

	address = entry_regs->ARM_lr;

	read_inst_before(task, address, NULL, &size);
	write_thread_release_sync_info(task, address - size, entry_ts, entry_regs->ARM_r0, TP_SYNC_OBJ_TYPE_RW_LOCK, 0);

	return 0;
}

static int waitpid_entry_cb(struct task_struct * task,
                            unsigned int         orig_inst,
                            int                  orig_inst_size,
                            struct pt_regs     * entry_regs,
                            unsigned long long   entry_ts)
{
	return 0;
}


static int waitpid_return_cb(struct task_struct * task,
                             unsigned int         orig_inst,
                             int                  orig_inst_size,
                             struct pt_regs     * exit_regs,
                             unsigned long long   exit_ts,
                             struct pt_regs     * entry_regs,
                             unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	pid_t tid;
	
	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 > 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;
	
	tid = exit_regs->ARM_r0;

	read_inst_before(task, address, NULL, &size);
	
	write_thread_join_begin_info(task, address - size, entry_ts, tid);
	write_thread_join_end_info(task, address - size, exit_ts, tid, 0, ret_value);

	return 0;
}

static int wait_entry_cb(struct task_struct * task,
                         unsigned int         orig_inst,
                         int                  orig_inst_size,
                         struct pt_regs     * entry_regs,
                         unsigned long long   entry_ts)
{
	return 0;
}


static int wait_return_cb(struct task_struct * task,
                          unsigned int         orig_inst,
                          int                  orig_inst_size,
                          struct pt_regs     * exit_regs,
                          unsigned long long   exit_ts,
                          struct pt_regs     * entry_regs,
                          unsigned long long   entry_ts)
{
	int ret_value;
	unsigned int address;
	unsigned int size;

	pid_t tid;
	
	if ((task == NULL) || (entry_regs == NULL) || (exit_regs == NULL))
		return -EINVAL;

	ret_value = (exit_regs->ARM_r0 > 0)? WAIT_RETURN_SUCCESS : WAIT_RETURN_ERROR;

	address = entry_regs->ARM_lr;
	
	tid = exit_regs->ARM_r0;

	read_inst_before(task, address, NULL, &size);
	
	write_thread_join_begin_info(task, address - size, entry_ts, tid);
	write_thread_join_end_info(task, address - size, exit_ts, tid, 0, ret_value);

	return 0;
}


/* The following table must be static to make sure all variables are initialzed to zero */
static struct f_hook fh_array[] =
{
#if 1
	/* pthread create/exit/join */
	{"pthread_create",  pthread_create_entry_cb,             pthread_create_return_cb},
	{"pthread_exit",    pthread_exit_entry_cb,               NULL},
	{"pthread_join",    pthread_join_entry_cb,               pthread_join_return_cb},
#endif
#if 1
	/* mutex */
	{"pthread_mutex_init",       pthread_mutex_init_entry_cb,         pthread_mutex_init_return_cb},
	{"pthread_mutex_destroy",    pthread_mutex_destroy_entry_cb,      pthread_mutex_destroy_return_cb},
	{"pthread_mutex_lock",       pthread_mutex_lock_entry_cb,         pthread_mutex_lock_return_cb},
	{"pthread_mutex_trylock",    pthread_mutex_trylock_entry_cb,      pthread_mutex_trylock_return_cb},
	{"pthread_mutex_timedlock",  pthread_mutex_timedlock_entry_cb,    pthread_mutex_timedlock_return_cb},
	{"pthread_mutex_unlock",     pthread_mutex_unlock_entry_cb,       pthread_mutex_unlock_return_cb},
#endif

#if 1
#ifdef CONFIG_ANDROID
	/* pthread_mutex_timedlock() is not supported on Android, hook on pthread_mutex_lock_timeout_np() instead */
	{"pthread_mutex_lock_timeout_np",  pthread_mutex_timedlock_entry_cb,    pthread_mutex_timedlock_return_cb},
#endif
	/* read/write lock */
	{"pthread_rwlock_init",         pthread_rwlock_init_entry_cb,        pthread_rwlock_init_return_cb},
	{"pthread_rwlock_destroy",      pthread_rwlock_destroy_entry_cb,     pthread_rwlock_destroy_return_cb},
#ifndef CONFIG_ANDROID
	/* on Android, pthread_rwlock_rdlock() is implemented via pthread_rwlock_timedrdlock(). We can only hook on pthread_rwlock_timedrdlock() */
	{"pthread_rwlock_rdlock",       pthread_rwlock_rdlock_entry_cb,      pthread_rwlock_rdlock_return_cb},
	/* on Android, pthread_rwlock_wrlock() is implemented via pthread_rwlock_timedwrlock(). We can only hook on pthread_rwlock_timedwrlock() */
	{"pthread_rwlock_wrlock",       pthread_rwlock_wrlock_entry_cb,      pthread_rwlock_wrlock_return_cb},
#endif
	{"pthread_rwlock_timedrdlock",  pthread_rwlock_timedrdlock_entry_cb, pthread_rwlock_timedrdlock_return_cb},
	{"pthread_rwlock_timedwrlock",  pthread_rwlock_timedwrlock_entry_cb, pthread_rwlock_timedwrlock_return_cb},
	{"pthread_rwlock_tryrdlock",    pthread_rwlock_tryrdlock_entry_cb,   pthread_rwlock_tryrdlock_return_cb},
	{"pthread_rwlock_trywrlock",    pthread_rwlock_trywrlock_entry_cb,   pthread_rwlock_trywrlock_return_cb},
	{"pthread_rwlock_unlock",       pthread_rwlock_unlock_entry_cb,      pthread_rwlock_unlock_return_cb},

	/* spin lock */
	{"pthread_spin_init",          pthread_spin_init_entry_cb,           pthread_spin_init_return_cb},
	{"pthread_spin_destroy",       pthread_spin_destroy_entry_cb,        pthread_spin_destroy_return_cb},
	{"pthread_spin_lock",          pthread_spin_lock_entry_cb,           pthread_spin_lock_return_cb},
	{"pthread_spin_trylock",       pthread_spin_trylock_entry_cb,        pthread_spin_trylock_return_cb},
	{"pthread_spin_unlock",        pthread_spin_unlock_entry_cb,         pthread_spin_unlock_return_cb},

	/* condition variable */
	{"pthread_cond_init",          pthread_cond_init_entry_cb,           pthread_cond_init_return_cb},
	{"pthread_cond_destroy",       pthread_cond_destroy_entry_cb,        pthread_cond_destroy_return_cb},
#ifndef CONFIG_ANDROID
	/* on Android, pthread_cond_wait() is implemented via pthread_cond_timedwait(). We can only hook on pthread_cond_timedwait() */
	{"pthread_cond_wait",          pthread_cond_wait_entry_cb,           pthread_cond_wait_return_cb},
#endif
	{"pthread_cond_timedwait",     pthread_cond_timedwait_entry_cb,      pthread_cond_timedwait_return_cb},
	{"pthread_cond_signal",        pthread_cond_signal_entry_cb,         pthread_cond_signal_return_cb},
	{"pthread_cond_broadcast",     pthread_cond_broadcast_entry_cb,      pthread_cond_broadcast_return_cb},

	/* posix semaphore */
	{"sem_init",          sem_init_entry_cb,         sem_init_return_cb},
	{"sem_open",          sem_open_entry_cb,         sem_open_return_cb},
	{"sem_destroy",       sem_destroy_entry_cb,      sem_destroy_return_cb},
	{"sem_close",         sem_close_entry_cb,        sem_close_return_cb},
	{"sem_wait",          sem_wait_entry_cb,         sem_wait_return_cb},
	{"sem_timedwait",     sem_timedwait_entry_cb,    sem_timedwait_return_cb},
	{"sem_trywait",       sem_trywait_entry_cb,      sem_trywait_return_cb},
	{"sem_post",          sem_post_entry_cb,         sem_post_return_cb},

#ifdef CONFIG_POSIX_MQUEUE
	/* posix message queue */
	{"mq_open",             mq_open_entry_cb,          mq_open_return_cb},
	{"mq_close",            mq_close_entry_cb,         mq_close_return_cb},
	{"mq_receive",          mq_receive_entry_cb,       mq_receive_return_cb},
	{"mq_timedreceive",     mq_timedreceive_entry_cb,  mq_timedreceive_return_cb},
	{"mq_send",             mq_send_entry_cb,          mq_send_return_cb},
	{"mq_timedsend",        mq_timedsend_entry_cb,     mq_timedsend_return_cb},
#endif /* CONFIG_POSIX_MQUEUE */
#endif
#if 0
	/* System V semaphore */
	{"semget",            semget_entry_cb,           semget_return_cb},
	{"semop",             semop_entry_cb,            semop_return_cb},
	{"semtimedop",        semtimedop_entry_cb,       semtimedop_return_cb},
	{"semctl",            semctl_entry_cb,           semctl_return_cb},

	/* System V message queue */
	{"msgget",            msgget_entry_cb,           msgget_return_cb},
	{"msgrcv",            msgrcv_entry_cb,           msgrcv_return_cb},
	{"msgsnd",            msgsnd_entry_cb,           msgsnd_return_cb},
	{"msgctl",            msgctl_entry_cb,           msgctl_return_cb},
	
#endif
	{"waitpid",            waitpid_entry_cb,         waitpid_return_cb},
	{"wait",               wait_entry_cb,            wait_return_cb},
	
	{NULL, NULL, NULL}
};



int start_functions_hooking(void)
{
	init_uhooks();

	return 0;
}

int stop_functions_hooking(void)
{
	//restore_replaced_insts();

	//clear_replaced_inst_lists();
	//clear_fci_lists();
	clear_exit_thread_lists();
	clear_user_thread_lists();
	clear_pthread_tid_lists();
	uninit_uhooks();

	return 0;
}

void static_enum_thread(struct task_struct *task)
{
//	write_thread_static_enum_info(task);
}

static void hook_process(pid_t tgid)
{
#if 0
	struct ufunc_hook * hook;

	hook = kzalloc(sizeof(struct ufunc_hook), GFP_ATOMIC);

	if (hook == NULL)
		return;

	init_user_func_hook(hook);

	hook->tgid  = tgid;

	hook->hook_addr     = 0xb00033a9;
	//hook->hook_func = "dlopen";
	hook->pre_handler = NULL;
	hook->return_handler  = NULL;

	register_user_func_hook(hook);
#else
	int i;
	for (i=0; fh_array[i].func_name != NULL; i++)
	{
		struct ufunc_hook * hook;

		hook = kzalloc(sizeof(struct ufunc_hook), GFP_ATOMIC);

		if (hook == NULL)
			continue;

		init_user_func_hook(hook);

		hook->tgid  = tgid;

		hook->hook_func      = fh_array[i].func_name;
		hook->pre_handler    = fh_array[i].entry_handler;
		hook->return_handler = fh_array[i].return_handler;

		raw_register_user_func_hook(hook);
	}
#endif
}

static void notify_thread_create(struct task_struct * task, struct task_struct * parent, unsigned long long ts)
{
	add_user_thread(task, parent, ts);
}

int notify_fork(struct task_struct * task, struct task_struct * child, unsigned int pc, unsigned long long ts)
{
	if ((task == NULL) || (child == NULL))
		return -EINVAL;

	write_thread_create_info(task, task_pt_regs(task)->ARM_pc, ts, child->tgid, child->pid, 0);
	write_thread_create_info(child, task_pt_regs(child)->ARM_pc, ts, child->tgid, child->pid, 0);

	uhook_notify_fork(child, task, ts);

	if (task->tgid != g_mpdc_tgid)
	{
		hook_process(child->tgid);
	}

	return 0;
}

int notify_vfork(struct task_struct * task, struct task_struct * child, unsigned int pc, unsigned long long ts)
{
	if ((task == NULL) || (child == NULL))
		return -EINVAL;

	write_thread_create_info(task, pc, ts, child->tgid, child->pid, 0);
	write_thread_create_info(child, task_pt_regs(child)->ARM_pc, ts, child->tgid, child->pid, 0);
/*
	uhook_notify_vfork(child, task, ts);
	
	if (task->tgid != g_mpdc_tgid)
	{
		hook_process(child->tgid);
	}
*/
	return 0;
}

int notify_clone(struct task_struct * task, struct task_struct * child, unsigned int pc, unsigned long long ts, unsigned int clone_flags)
{
	if ((task == NULL) || (child == NULL))
		return -EINVAL;

	if (!(clone_flags & CLONE_THREAD))
	{
		write_thread_create_info(task, pc, ts, child->tgid, child->pid, 0);
		write_thread_create_info(child, task_pt_regs(child)->ARM_pc, ts, child->tgid, child->pid, 0);

		uhook_notify_clone(child, task, ts, clone_flags);

		if (task->tgid != g_mpdc_tgid)
		{
			hook_process(child->tgid);
		}
	}
	else
	{
		notify_thread_create(child, task, ts);
	}

	return 0;
}

int notify_exec(struct task_struct * task, struct task_struct * child, unsigned int pc, unsigned long long ts)
{
	if (task == NULL)
		return -EINVAL;

	//write_process_exit_info(task, pc, ts, 0, true);

	write_thread_create_info(child, pc, ts, child->tgid, child->pid, 0);

	uhook_notify_exec(child, child->parent, ts);

	hook_process(child->tgid);

	return 0;
}

int notify_proc_name_changed(struct task_struct * task, unsigned int pc, unsigned long long ts)
{
	if (task == NULL)
		return -EINVAL;

	/* write all threads in the process exit */
	write_process_exit_info(task, pc, ts - 1, 0, false);
	
	/* write all threads in the process re-create */
	write_process_create_info(task->group_leader->parent, task->group_leader, pc, ts);

	return 0;
}

int notify_thread_exit(struct task_struct * task,
                       unsigned int         pc,
                       unsigned long long   ts,
                       int                  ret_value,
                       bool                 real_exit)
{
	spin_lock(&thread_exit_lock);

	if (!find_exit_thread(task))
	{
		if (real_exit)
			write_thread_exit_info(task, pc, ts, ret_value);
		
		if (real_exit)
			add_exit_thread(task);
	}

	if (real_exit)
	{
		remove_user_thread(task);

		uhook_notify_thread_exit(current, ret_value, ts);
	}
	
	spin_unlock(&thread_exit_lock);
	
	return 0;
}

/* 
 * This function need be called when preempt_disable() is already called
 * In this function, it will call preempt_enable()
 */
int notify_process_exit_with_preempt_disabled(struct task_struct * task,
                                              unsigned int         pc,
                                              unsigned long long   ts,
                                              int                  ret_value)
{
	/* for all threads in the same process */
	write_process_exit_info(task, pc, ts, ret_value, true);

	preempt_enable();

	uhook_notify_thread_group_exit(task, ret_value, ts);

	return 0;
}

/* 
 * This function need be called when preempt_disable() is not called
 */
int notify_process_exit(struct task_struct * task,
                        unsigned int         pc,
                        unsigned long long   ts,
                        int                  ret_value)
{
	/* for all threads in the same process */
	write_process_exit_info(task, pc, ts, ret_value, true);

	uhook_notify_thread_group_exit(task, ret_value, ts);

	return 0;
}


