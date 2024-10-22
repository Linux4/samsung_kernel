// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/sched/signal.h>
#include <linux/pid_namespace.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/pagewalk.h>
#include <linux/shmem_fs.h>
#include <linux/pagemap.h>
#include <linux/mempolicy.h>
#include <linux/rmap.h>
#include <linux/sched/cputime.h>
#include <linux/math64.h>
#include <linux/refcount.h>
#include <linux/ctype.h>
#include <linux/stddef.h>
#include <linux/cred.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <trace/hooks/sched.h>
#include <linux/mm_inline.h>
#include <trace/hooks/binder.h>
#include <uapi/linux/android/binder.h>
#include <asm/page.h>

#include "mbraink_process.h"
#include <binder_internal.h>

#define PROCESS_INFO_STR	\
	"pid=%-10u:uid=%u,priority=%d,utime=%llu,stime=%llu,cutime=%llu,cstime=%llu,name=%s\n"

#define THREAD_INFO_STR		\
	"--> pid=%-10u:uid=%u,priority=%d,utime=%llu,stime=%llu,cutime=%llu,cstime=%llu,name=%s\n"

/*spinlock for mbraink monitored binder pidlist*/
static DEFINE_SPINLOCK(monitor_binder_pidlist_lock);
/*Please make sure that monitor binder pidlist is protected by spinlock*/
struct mbraink_monitor_pidlist mbraink_monitor_binder_pidlist_data;

/*spinlock for mbraink monitored pidlist*/
static DEFINE_SPINLOCK(monitor_pidlist_lock);
/*Please make sure that monitor pidlist is protected by spinlock*/
struct mbraink_monitor_pidlist mbraink_monitor_pidlist_data;

/*spinlock for mbraink tracing pidlist*/
static DEFINE_SPINLOCK(tracing_pidlist_lock);
/*Please make sure that tracing pidlist is protected by spinlock*/
struct mbraink_tracing_pidlist mbraink_tracing_pidlist_data[MAX_TRACE_NUM];

/*spinlock for mbraink binder provider tracelist*/
static DEFINE_SPINLOCK(binder_trace_lock);
/*Please make sure that binder trace list is protected by spinlock*/
struct mbraink_binder_tracelist mbraink_binder_tracelist_data[MAX_BINDER_TRACE_NUM];

#if IS_ENABLED(CONFIG_MTK_MBRAINK_EXPORT_DEPENDED)
#else

static int register_trace_android_vh_do_fork(void *t, void *p)
{
	pr_info("%s: not support yet...", __func__);
	return 0;
}

static int register_trace_android_vh_do_exit(void *t, void *p)
{
	pr_info("%s: not support yet...", __func__);
	return 0;
}

static int unregister_trace_android_vh_do_fork(void *t, void *p)
{
	pr_info("%s: not support yet...", __func__);
	return 0;
}
static int unregister_trace_android_vh_do_exit(void *t, void *p)
{
	pr_info("%s: not support yet...", __func__);
	return 0;
}
#endif

void mbraink_get_process_memory_info(pid_t current_pid, unsigned int cnt,
				struct mbraink_process_memory_data *process_memory_buffer)
{
	struct task_struct *t = NULL;
	unsigned short pid_count = 0;
	unsigned int current_count = 0;

	memset(process_memory_buffer, 0, sizeof(struct mbraink_process_memory_data));
	process_memory_buffer->pid = 0;
	process_memory_buffer->current_cnt = cnt;

	read_lock(&tasklist_lock);
	for_each_process(t) {
		task_lock(t);
		if (t->mm) {
			mmgrab(t->mm);
			if (current_count < cnt) {
				++current_count;
				mmdrop(t->mm);
				task_unlock(t);
				continue;
			}
			pid_count = process_memory_buffer->pid_count;
			if (pid_count < MAX_MEM_LIST_SZ) {
				process_memory_buffer->drv_data[pid_count].pid =
					(unsigned short)(t->pid);
				process_memory_buffer->drv_data[pid_count].rss =
					get_mm_rss(t->mm) * (PAGE_SIZE / 1024);
				process_memory_buffer->drv_data[pid_count].rswap =
					get_mm_counter(t->mm, MM_SWAPENTS) * (PAGE_SIZE / 1024);
				process_memory_buffer->drv_data[pid_count].rpage =
					mm_pgtables_bytes(t->mm) / 1024;
				mmdrop(t->mm);
				task_unlock(t);
				process_memory_buffer->pid_count++;
				process_memory_buffer->current_cnt++;
			} else {
				process_memory_buffer->pid = (unsigned short)(t->pid);
				mmdrop(t->mm);
				task_unlock(t);
				break;
			}
		} else {
			task_unlock(t);
			/*pr_info("kthread case ...\n");*/
		}
	}
	read_unlock(&tasklist_lock);
}

void mbraink_get_process_stat_info(pid_t current_pid,
		struct mbraink_process_stat_data *process_stat_buffer)
{
	struct task_struct *t = NULL;
	u64 stime = 0, utime = 0, cutime = 0, cstime = 0;
	int ret = 0;
	u64 process_jiffies = 0;
	int priority = 0;
	unsigned short pid_count = 0;
	kuid_t uid;

	memset(process_stat_buffer, 0, sizeof(struct mbraink_process_stat_data));
	process_stat_buffer->pid = 0;

	read_lock(&tasklist_lock);
	for_each_process(t) {
		if (t->pid < current_pid)
			continue;

		stime = utime = 0;
		cutime = t->signal->cutime;
		cstime = t->signal->cstime;

		if (t->mm)
			thread_group_cputime_adjusted(t, &utime, &stime);
		else
			task_cputime_adjusted(t, &utime, &stime);

		process_jiffies = nsec_to_clock_t(utime) +
				nsec_to_clock_t(stime) +
				nsec_to_clock_t(cutime) +
				nsec_to_clock_t(cstime);

		//cred = get_task_cred(t);
		rcu_read_lock();
		uid = __task_cred(t)->uid;
		rcu_read_unlock();
		priority = t->prio - MAX_RT_PRIO;
		pid_count = process_stat_buffer->pid_count;

		if (pid_count < MAX_STRUCT_SZ) {
			process_stat_buffer->drv_data[pid_count].pid = (unsigned short)(t->pid);
			//process_stat_buffer->drv_data[pid_count].uid = cred->uid.val;
			process_stat_buffer->drv_data[pid_count].uid = uid.val;
			process_stat_buffer->drv_data[pid_count].process_jiffies = process_jiffies;
			process_stat_buffer->drv_data[pid_count].priority = priority;
			process_stat_buffer->pid_count++;
			//put_cred(cred);
		} else {
			ret = -1;
			process_stat_buffer->pid = (unsigned short)(t->pid);
			//put_cred(cred);
			break;
		}
	}

	pr_info("%s: current_pid = %u, count = %u\n",
		__func__, process_stat_buffer->pid, process_stat_buffer->pid_count);
	read_unlock(&tasklist_lock);
}

void mbraink_get_thread_stat_info(pid_t current_pid_idx, pid_t current_tid,
				struct mbraink_thread_stat_data *thread_stat_buffer)
{
	struct task_struct *t = NULL;
	struct task_struct *s = NULL;
	struct pid *parent_pid = NULL;
	u64 stime = 0, utime = 0, cutime = 0, cstime = 0;
	int ret = 0;
	u64 thread_jiffies = 0;
	int priority = 0;
	int index = 0;
	unsigned long flags;
	int count = 0;
	unsigned short tid_count = 0;
	unsigned short processlist_temp[MAX_MONITOR_PROCESS_NUM];
	kuid_t uid;

	/*Check if there is a config to set montor process pid list*/
	spin_lock_irqsave(&monitor_pidlist_lock, flags);
	if (mbraink_monitor_pidlist_data.is_set == 0) {
		spin_unlock_irqrestore(&monitor_pidlist_lock, flags);
		pr_notice("the monitor pid list is unavailable now !!!\n");
		ret = -1;
		return;
	}

	count = mbraink_monitor_pidlist_data.monitor_process_count;
	for (index = 0; index < count; index++)
		processlist_temp[index] = mbraink_monitor_pidlist_data.monitor_pid[index];

	spin_unlock_irqrestore(&monitor_pidlist_lock, flags);

	read_lock(&tasklist_lock);
	memset(thread_stat_buffer, 0, sizeof(struct mbraink_thread_stat_data));
	thread_stat_buffer->tid = 0;
	thread_stat_buffer->tid_count = 0;

	for (index = current_pid_idx; index < count; index++) {
		parent_pid = find_get_pid(processlist_temp[index]);

		if (parent_pid == NULL) {
			pr_info("%s: parent_pid %u = NULL\n",
				__func__, processlist_temp[index]);
			continue;
		} else {
			t = get_pid_task(parent_pid, PIDTYPE_PID);
			if (t == NULL) {
				put_pid(parent_pid);
				pr_info("%s: task pid %u = NULL\n",
					__func__, processlist_temp[index]);
				continue;
			}
		}

		if (t && t->mm) {
			for_each_thread(t, s) {
				if (s->pid < current_tid)
					continue;

				stime = utime = 0;
				cutime = s->signal->cutime;
				cstime = s->signal->cstime;
				task_cputime_adjusted(s, &utime, &stime);
				/***********************************************
				 *cutime and cstime is to wait for child process
				 *or the exiting child process time, so we did
				 *not include it in thread jiffies
				 ***********************************************/
				thread_jiffies = nsec_to_clock_t(utime) + nsec_to_clock_t(stime);
				//cred = get_task_cred(s);
				rcu_read_lock();
				uid = __task_cred(t)->uid;
				rcu_read_unlock();
				priority = s->prio - MAX_RT_PRIO;
				tid_count = thread_stat_buffer->tid_count;

				if (tid_count < MAX_STRUCT_SZ) {
					thread_stat_buffer->drv_data[tid_count].pid =
									processlist_temp[index];
					thread_stat_buffer->drv_data[tid_count].tid =
									(unsigned short)(s->pid);
					thread_stat_buffer->drv_data[tid_count].uid =
									uid.val;
					thread_stat_buffer->drv_data[tid_count].thread_jiffies =
									thread_jiffies;
					thread_stat_buffer->drv_data[tid_count].priority =
									priority;
					/*******************************************************
					 *pr_info("tid_count=%u,  pid=%u, tid=%u, uid=%u,	\
					 * jiffies=%llu, priority=%d, name=%s\n",
					 * tid_count,
					 * thread_stat_buffer->drv_data[tid_count].pid,
					 * thread_stat_buffer->drv_data[tid_count].tid,
					 * thread_stat_buffer->drv_data[tid_count].uid,
					 * thread_stat_buffer->drv_data[tid_count].thread_jiffies,
					 * thread_stat_buffer->drv_data[tid_count].priority,
					 * s->comm);
					 ********************************************************/
					thread_stat_buffer->tid_count++;

					//put_cred(cred);
				} else {
					ret = -1;
					thread_stat_buffer->tid = (unsigned short)(s->pid);
					thread_stat_buffer->pid_idx = index;
					//put_cred(cred);
					break;
				}
			}
		} else {
			pr_info("This pid of task is kernel thread and has no children thread.\n");
		}
		put_task_struct(t);
		put_pid(parent_pid);

		if (ret == -1) {
			/*buffer is full this time*/
			break;
		}
		/*move to the next pid and reset the current tid*/
		current_tid = 1;
	}

	pr_info("%s: current_tid = %u, current_pid_idx = %u, count = %u\n",
			__func__, thread_stat_buffer->tid, thread_stat_buffer->pid_idx,
			thread_stat_buffer->tid_count);

	read_unlock(&tasklist_lock);
}

char *strcasestr(const char *s1, const char *s2)
{
	const char *s = s1;
	const char *p = s2;

	do {
		if (!*p)
			return (char *) s1;
		if ((*p == *s) || (tolower(*p) == tolower(*s))) {
			++p;
			++s;
		} else {
			p = s2;
			if (!*s)
				return NULL;
			s = ++s1;
		}
	} while (1);

	return *p ? NULL : (char *) s1;
}

void mbraink_processname_to_pid(unsigned short monitor_process_count,
				const struct mbraink_monitor_processlist *processname_inputlist,
				bool is_binder)
{
	struct task_struct *t = NULL;
	char *cmdline = NULL;
	int index = 0;
	int count = 0;
	unsigned short processlist_temp[MAX_MONITOR_PROCESS_NUM];
	unsigned long flags;
	unsigned int process_num = 0;
	unsigned int i = 0;
	unsigned int boundary = 0;
	bool find = false;
	pid_t pid_tmp = 0;

	if (is_binder) {
		spin_lock_irqsave(&monitor_binder_pidlist_lock, flags);
		mbraink_monitor_binder_pidlist_data.is_set = 0;
		mbraink_monitor_binder_pidlist_data.monitor_process_count = 0;
		spin_unlock_irqrestore(&monitor_binder_pidlist_lock, flags);
	} else {
		spin_lock_irqsave(&monitor_pidlist_lock, flags);
		mbraink_monitor_pidlist_data.is_set = 0;
		mbraink_monitor_pidlist_data.monitor_process_count = 0;
		spin_unlock_irqrestore(&monitor_pidlist_lock, flags);
	}

	if (monitor_process_count == 0) {
		count = 0;
		pr_info("monitor_binder_pidlist_data count = 0\n");
		goto setting;
	}

	read_lock(&tasklist_lock);
	for_each_process(t) {
		if (t->mm)
			++process_num;
	}
	read_unlock(&tasklist_lock);

	for (boundary = 0; boundary < process_num; boundary++) {
		i = 0;
		find = false;
		read_lock(&tasklist_lock);
		for_each_process(t) {
			task_lock(t);
			if (t->mm) {
				if (i < boundary)
					++i;
				else {
					get_task_struct(t);
					pid_tmp = t->pid;
					task_unlock(t);
					find = true;
					break;
				}
			}
			task_unlock(t);
		}
		read_unlock(&tasklist_lock);

		if (find == false)
			break;

		/*This function might sleep*/
		cmdline = kstrdup_quotable_cmdline(t, GFP_KERNEL);
		put_task_struct(t);

		if (!cmdline) {
			pr_info("%s: cmdline is NULL\n", __func__);
			continue;
		}

		for (index = 0; index < monitor_process_count; index++) {
			if (strcasestr(cmdline,
				processname_inputlist->process_name[index])) {
				if (count < MAX_MONITOR_PROCESS_NUM) {
					processlist_temp[count] = (unsigned short)(pid_tmp);
					count++;
				}
				break;
			}
		}
		kfree(cmdline);

		if (count >= MAX_MONITOR_PROCESS_NUM)
			break;
	}

setting:
	if (is_binder) {
		spin_lock_irqsave(&monitor_binder_pidlist_lock, flags);
		mbraink_monitor_binder_pidlist_data.monitor_process_count = count;
		for (index = 0; index < count; index++) {
			mbraink_monitor_binder_pidlist_data.monitor_pid[index] =
								processlist_temp[index];
			pr_info("monitor_binder_pidlist_data.monitor_pid[%d] = %u, count = %u\n",
				index, processlist_temp[index],
				mbraink_monitor_binder_pidlist_data.monitor_process_count);
		}
		mbraink_monitor_binder_pidlist_data.is_set = 1;
		spin_unlock_irqrestore(&monitor_binder_pidlist_lock, flags);
	} else {
		spin_lock_irqsave(&monitor_pidlist_lock, flags);
		mbraink_monitor_pidlist_data.monitor_process_count = count;
		for (index = 0; index < count; index++) {
			mbraink_monitor_pidlist_data.monitor_pid[index] = processlist_temp[index];
			pr_info("mbraink_monitor_pidlist_data.monitor_pid[%d] = %u, count = %u\n",
				index, processlist_temp[index],
				mbraink_monitor_pidlist_data.monitor_process_count);
		}
		mbraink_monitor_pidlist_data.is_set = 1;
		spin_unlock_irqrestore(&monitor_pidlist_lock, flags);
	}
}

void mbraink_show_process_info(void)
{
	struct task_struct *t = NULL;
	struct task_struct *s = NULL;
	struct mm_struct *mm = NULL;
	int priority = 0;
	u64 stime = 0, utime = 0, cutime = 0, cstime = 0;
	char *cmdline = NULL;
	unsigned int counter = 0;
	kuid_t uid;

	read_lock(&tasklist_lock);
	for_each_process(t) {
		stime = utime = 0;
		mm = t->mm;
		counter++;
		if (mm) {
			get_task_struct(t);
			read_unlock(&tasklist_lock);
			/*This function might sleep, cannot be called during atomic context*/
			cmdline = kstrdup_quotable_cmdline(t, GFP_KERNEL);
			read_lock(&tasklist_lock);
			put_task_struct(t);

			cutime = t->signal->cutime;
			cstime = t->signal->cstime;
			thread_group_cputime_adjusted(t, &utime, &stime);
			//cred = get_task_cred(t);
			rcu_read_lock();
			uid = __task_cred(t)->uid;
			rcu_read_unlock();
			priority = t->prio - MAX_RT_PRIO;

			if (cmdline) {
				pr_info(PROCESS_INFO_STR,
					t->pid, uid.val, priority, nsec_to_clock_t(utime),
					nsec_to_clock_t(stime), nsec_to_clock_t(cutime),
					nsec_to_clock_t(cstime), cmdline);
				kfree(cmdline);
			} else {
				pr_info(PROCESS_INFO_STR,
					t->pid, uid.val, priority, nsec_to_clock_t(utime),
					nsec_to_clock_t(stime), nsec_to_clock_t(cutime),
					nsec_to_clock_t(cstime), "NULL");
			}

			//put_cred(cred);
			for_each_thread(t, s) {
				//cred = get_task_cred(s);
				rcu_read_lock();
				uid = __task_cred(t)->uid;
				rcu_read_unlock();
				cutime = cstime = stime = utime = 0;
				cutime = s->signal->cutime;
				cstime = s->signal->cstime;
				task_cputime_adjusted(s, &utime, &stime);
				priority = s->prio - MAX_RT_PRIO;

				pr_info(THREAD_INFO_STR,
					s->pid, uid.val, priority, nsec_to_clock_t(utime),
					nsec_to_clock_t(stime), nsec_to_clock_t(cutime),
					nsec_to_clock_t(cstime), s->comm);
				//put_cred(cred);
			}
		} else {
			//cred = get_task_cred(t);
			rcu_read_lock();
			uid = __task_cred(t)->uid;
			rcu_read_unlock();
			cutime = t->signal->cutime;
			cstime = t->signal->cstime;
			task_cputime_adjusted(t, &utime, &stime);
			priority = t->prio - MAX_RT_PRIO;
			pr_info(PROCESS_INFO_STR,
				t->pid, uid.val, priority, nsec_to_clock_t(utime),
				nsec_to_clock_t(stime), nsec_to_clock_t(cutime),
				nsec_to_clock_t(cstime), t->comm);
			//put_cred(cred);
		}
	}
	read_unlock(&tasklist_lock);
	pr_info("total task list element number = %u\n", counter);
}

#if IS_ENABLED(CONFIG_ANDROID_VENDOR_HOOKS)
/*****************************************************************
 * Note: this function can only be used during tracing function
 * This function is only used in tracing function so that there
 * is no need for task t spinlock protection
 *****************************************************************/
static u64 mbraink_get_specific_process_jiffies(struct task_struct *t)
{
	u64 stime = 0, utime = 0, cutime = 0, cstime = 0;
	u64 process_jiffies = 0;

	if (t->pid == t->tgid) {
		cutime = t->signal->cutime;
		cstime = t->signal->cstime;
		if (t->flags & PF_KTHREAD)
			task_cputime_adjusted(t, &utime, &stime);
		else
			thread_group_cputime_adjusted(t, &utime, &stime);

		process_jiffies = nsec_to_clock_t(utime) +
				nsec_to_clock_t(stime) +
				nsec_to_clock_t(cutime) +
				nsec_to_clock_t(cstime);
	} else {
		task_cputime_adjusted(t, &utime, &stime);
		process_jiffies = nsec_to_clock_t(utime) + nsec_to_clock_t(stime);
	}

	return process_jiffies;
}

/***************************************************************
 * Note: this function can only be used during tracing function
 * This function is only used in tracing function so that there
 * is no need for task t spinlock protection
 **************************************************************/
static u16 mbraink_get_specific_process_uid(struct task_struct *t)
{
	//const struct cred *cred = NULL;
	u16 val = 0;

	//cred = get_task_cred(t);
	rcu_read_lock();
	val = __task_cred(t)->uid.val;
	rcu_read_unlock();
	//val = uid.val;
	//put_cred(cred);

	return val;
}

static int is_monitor_process(unsigned short pid)
{
	int ret = 0, index = 0;
	unsigned short monitor_process_count = 0;
	unsigned long flags;

	spin_lock_irqsave(&monitor_pidlist_lock, flags);
	if (mbraink_monitor_pidlist_data.is_set == 0) {
		spin_unlock_irqrestore(&monitor_pidlist_lock, flags);
		return ret;
	}

	monitor_process_count = mbraink_monitor_pidlist_data.monitor_process_count;

	for (index = 0; index < monitor_process_count; index++) {
		if (mbraink_monitor_pidlist_data.monitor_pid[index] == pid) {
			ret = 1;
			break;
		}
	}

	spin_unlock_irqrestore(&monitor_pidlist_lock, flags);

	return ret;
}

static int is_monitor_binder_process(unsigned short pid)
{
	int ret = 0, index = 0;
	unsigned short monitor_process_count = 0;
	unsigned long flags;

	spin_lock_irqsave(&monitor_binder_pidlist_lock, flags);
	if (mbraink_monitor_binder_pidlist_data.is_set == 0) {
		spin_unlock_irqrestore(&monitor_binder_pidlist_lock, flags);
		return ret;
	}

	monitor_process_count = mbraink_monitor_binder_pidlist_data.monitor_process_count;

	for (index = 0; index < monitor_process_count; index++) {
		if (mbraink_monitor_binder_pidlist_data.monitor_pid[index] == pid) {
			ret = 1;
			break;
		}
	}

	spin_unlock_irqrestore(&monitor_binder_pidlist_lock, flags);
	return ret;
}

static void mbraink_trace_android_vh_do_exit(void *data, struct task_struct *t)
{
	int i = 0;
	struct timespec64 tv = { 0 };
	unsigned long flags;

	if (t->pid == t->tgid || is_monitor_process((unsigned short)(t->tgid))) {
		spin_lock_irqsave(&tracing_pidlist_lock, flags);
		for (i = 0; i < MAX_TRACE_NUM; i++) {
			if (mbraink_tracing_pidlist_data[i].pid == (unsigned short)(t->pid)) {
				ktime_get_real_ts64(&tv);
				mbraink_tracing_pidlist_data[i].end =
					(tv.tv_sec*1000)+(tv.tv_nsec/1000000);
				mbraink_tracing_pidlist_data[i].jiffies =
						mbraink_get_specific_process_jiffies(t);
				mbraink_tracing_pidlist_data[i].dirty = true;
				/*************************************************************
				 * pr_info("pid=%s:%u, tgid=%u, pidlist[%d].start=%-10lld,	\
				 * pidlist[%d].end=%-10lld, pidlist[%d].jiffies=%llu\n",
				 * t->comm, t->pid, t->tgid, i,
				 * mbraink_tracing_pidlist_data[i].start, i,
				 * mbraink_tracing_pidlist_data[i].end, i,
				 * mbraink_tracing_pidlist_data[i].jiffies);
				 **************************************************************/
				break;
			}
		}
		if (i == MAX_TRACE_NUM) {
			for (i = 0; i < MAX_TRACE_NUM; i++) {
				if (mbraink_tracing_pidlist_data[i].pid == 0) {
					mbraink_tracing_pidlist_data[i].pid =
							(unsigned short)(t->pid);
					mbraink_tracing_pidlist_data[i].tgid =
							(unsigned short)(t->tgid);
					mbraink_tracing_pidlist_data[i].uid =
							mbraink_get_specific_process_uid(t);
					mbraink_tracing_pidlist_data[i].priority =
							t->prio - MAX_RT_PRIO;
					memcpy(mbraink_tracing_pidlist_data[i].name,
									t->comm, TASK_COMM_LEN);
					ktime_get_real_ts64(&tv);
					mbraink_tracing_pidlist_data[i].end =
							(tv.tv_sec*1000)+(tv.tv_nsec/1000000);
					mbraink_tracing_pidlist_data[i].jiffies =
							mbraink_get_specific_process_jiffies(t);
					mbraink_tracing_pidlist_data[i].dirty = true;
					/******************************************************
					 * pr_info("pid=%s:%u, tgid=%u pidlist[%d].	\
					 * start=%-10lld,pidlist[%d].end=%-10lld,	\
					 * pidlist[%d].jiffies=%llu\n",
					 * t->comm, t->pid, t->tgid, i,
					 * mbraink_tracing_pidlist_data[i].start, i,
					 * mbraink_tracing_pidlist_data[i].end, i,
					 * mbraink_tracing_pidlist_data[i].jiffies);
					 ********************************************************/
					break;
				}
			}
			if (i == MAX_TRACE_NUM) {
				pr_info("%s pid=%u:%s.\n", __func__, t->pid, t->comm);
				memset(mbraink_tracing_pidlist_data, 0,
					sizeof(struct mbraink_tracing_pidlist) * MAX_TRACE_NUM);
			}
		}
		spin_unlock_irqrestore(&tracing_pidlist_lock, flags);
	}
}

static void mbraink_trace_android_vh_do_fork(void *data, struct task_struct *p)
{
	int i = 0;
	struct timespec64 tv = { 0 };
	unsigned long flags;

	if (p->pid == p->tgid || is_monitor_process((unsigned short)(p->tgid))) {
		spin_lock_irqsave(&tracing_pidlist_lock, flags);
		for (i = 0; i < MAX_TRACE_NUM; i++) {
			if (mbraink_tracing_pidlist_data[i].pid == 0) {
				mbraink_tracing_pidlist_data[i].pid = (unsigned short)(p->pid);
				mbraink_tracing_pidlist_data[i].tgid = (unsigned short)(p->tgid);
				mbraink_tracing_pidlist_data[i].uid =
						mbraink_get_specific_process_uid(p);
				mbraink_tracing_pidlist_data[i].priority = p->prio - MAX_RT_PRIO;
				memcpy(mbraink_tracing_pidlist_data[i].name,
					p->comm, TASK_COMM_LEN);
				ktime_get_real_ts64(&tv);
				mbraink_tracing_pidlist_data[i].start =
						(tv.tv_sec*1000)+(tv.tv_nsec/1000000);
				mbraink_tracing_pidlist_data[i].dirty = true;
				break;
			}
		}

		if (i == MAX_TRACE_NUM) {
			pr_info("%s child_pid=%u:%s.\n", __func__, p->pid, p->comm);
			memset(mbraink_tracing_pidlist_data, 0,
				sizeof(struct mbraink_tracing_pidlist) * MAX_TRACE_NUM);
		}
		spin_unlock_irqrestore(&tracing_pidlist_lock, flags);
	}
}

static void mbraink_trace_android_vh_binder_trans(void *data,
						struct binder_proc *target_proc,
						struct binder_proc *proc,
						struct binder_thread *thread,
						struct binder_transaction_data *tr)
{
	unsigned long  flags;
	int idx = 0;

	if (!is_monitor_binder_process((unsigned short)(target_proc->tsk->pid)))
		return;

	spin_lock_irqsave(&binder_trace_lock, flags);
	for (idx = 0; idx < MAX_BINDER_TRACE_NUM; idx++) {
		if (mbraink_binder_tracelist_data[idx].dirty == true &&
			mbraink_binder_tracelist_data[idx].from_pid == proc->tsk->pid &&
			mbraink_binder_tracelist_data[idx].to_pid == target_proc->tsk->pid &&
			mbraink_binder_tracelist_data[idx].from_tid == thread->task->pid)
			break;
	}

	if (idx < MAX_BINDER_TRACE_NUM) {
		mbraink_binder_tracelist_data[idx].count++;
	} else {
		for (idx = 0; idx < MAX_BINDER_TRACE_NUM; idx++) {
			if (mbraink_binder_tracelist_data[idx].dirty == false)
				break;
		}
		if (idx == MAX_BINDER_TRACE_NUM) {
			char netlink_buf[NETLINK_EVENT_MESSAGE_SIZE] = {'\0'};
			int n = 0;
			int pos = 0;

			pr_info("%s: binder string record is full %d\n", __func__, idx);

			for (idx = 0; idx < MAX_BINDER_TRACE_NUM; idx++) {
				if (idx % 16 == 0) {
					pos = 0;
					n = snprintf(netlink_buf,
								NETLINK_EVENT_MESSAGE_SIZE, "%s ",
								NETLINK_EVENT_SYSBINDER);
					if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE)
						break;
					pos += n;
				}
				n = snprintf(netlink_buf + pos,
						NETLINK_EVENT_MESSAGE_SIZE - pos, "%u:%u:%u:%d ",
						mbraink_binder_tracelist_data[idx].from_pid,
						mbraink_binder_tracelist_data[idx].from_tid,
						mbraink_binder_tracelist_data[idx].to_pid,
						mbraink_binder_tracelist_data[idx].count);
				if (n < 0 || n >= NETLINK_EVENT_MESSAGE_SIZE - pos)
					break;
				pos += n;

				if (idx % 16 == 15) {
					mbraink_netlink_send_msg(netlink_buf);
					memset(netlink_buf, 0, NETLINK_EVENT_MESSAGE_SIZE);
				}
			}
			memset(mbraink_binder_tracelist_data, 0,
				sizeof(struct mbraink_binder_tracelist) * MAX_BINDER_TRACE_NUM);
			mbraink_binder_tracelist_data[0].from_pid = proc->tsk->pid;
			mbraink_binder_tracelist_data[0].to_pid = target_proc->tsk->pid;
			mbraink_binder_tracelist_data[0].from_tid = thread->task->pid;
			mbraink_binder_tracelist_data[0].count = 1;
			mbraink_binder_tracelist_data[0].dirty = true;
		} else {
			mbraink_binder_tracelist_data[idx].from_pid = proc->tsk->pid;
			mbraink_binder_tracelist_data[idx].to_pid = target_proc->tsk->pid;
			mbraink_binder_tracelist_data[idx].from_tid = thread->task->pid;
			mbraink_binder_tracelist_data[idx].count = 1;
			mbraink_binder_tracelist_data[idx].dirty = true;
		}
	}
	spin_unlock_irqrestore(&binder_trace_lock, flags);
}

int mbraink_process_tracer_init(void)
{
	int ret = 0;

	memset(mbraink_tracing_pidlist_data, 0,
		sizeof(struct mbraink_tracing_pidlist) * MAX_TRACE_NUM);

	memset(mbraink_binder_tracelist_data, 0,
		sizeof(struct mbraink_binder_tracelist) * MAX_BINDER_TRACE_NUM);

	ret = register_trace_android_vh_do_fork(mbraink_trace_android_vh_do_fork, NULL);
	if (ret) {
		pr_notice("register_trace_android_vh_do_fork failed.\n");
		goto register_trace_android_vh_do_fork;
	}
	ret = register_trace_android_vh_do_exit(mbraink_trace_android_vh_do_exit, NULL);
	if (ret) {
		pr_notice("register register_trace_android_vh_do_exit failed.\n");
		goto register_trace_android_vh_do_exit;
	}
	ret = register_trace_android_vh_binder_trans(mbraink_trace_android_vh_binder_trans,
						NULL);
	if (ret) {
		pr_notice("register_trace_android_vh_binder_trans failed.\n");
		goto register_trace_android_vh_binder_trans;
	}
	return ret;

register_trace_android_vh_binder_trans:
	unregister_trace_android_vh_do_exit(mbraink_trace_android_vh_do_exit, NULL);
register_trace_android_vh_do_exit:
	unregister_trace_android_vh_do_fork(mbraink_trace_android_vh_do_fork, NULL);
register_trace_android_vh_do_fork:
	return ret;
}

void mbraink_process_tracer_exit(void)
{
	unregister_trace_android_vh_do_fork(mbraink_trace_android_vh_do_fork, NULL);
	unregister_trace_android_vh_do_exit(mbraink_trace_android_vh_do_exit, NULL);
	unregister_trace_android_vh_binder_trans(mbraink_trace_android_vh_binder_trans, NULL);
}

void mbraink_get_tracing_pid_info(unsigned short current_idx,
				struct mbraink_tracing_pid_data *tracing_pid_buffer)
{
	int i = 0;
	int ret = 0;
	unsigned long flags;
	unsigned short tracing_count = 0;

	spin_lock_irqsave(&tracing_pidlist_lock, flags);

	memset(tracing_pid_buffer, 0, sizeof(struct mbraink_tracing_pid_data));

	for (i = current_idx; i < MAX_TRACE_NUM; i++) {
		if (mbraink_tracing_pidlist_data[i].dirty == false)
			continue;
		else {
			tracing_count = tracing_pid_buffer->tracing_count;
			if (tracing_count < MAX_TRACE_PID_NUM) {
				tracing_pid_buffer->drv_data[tracing_count].pid =
						mbraink_tracing_pidlist_data[i].pid;
				tracing_pid_buffer->drv_data[tracing_count].tgid =
						mbraink_tracing_pidlist_data[i].tgid;
				tracing_pid_buffer->drv_data[tracing_count].uid =
						mbraink_tracing_pidlist_data[i].uid;
				tracing_pid_buffer->drv_data[tracing_count].priority =
						mbraink_tracing_pidlist_data[i].priority;
				memcpy(tracing_pid_buffer->drv_data[tracing_count].name,
					mbraink_tracing_pidlist_data[i].name, TASK_COMM_LEN);
				tracing_pid_buffer->drv_data[tracing_count].start =
						mbraink_tracing_pidlist_data[i].start;
				tracing_pid_buffer->drv_data[tracing_count].end =
						mbraink_tracing_pidlist_data[i].end;
				tracing_pid_buffer->drv_data[tracing_count].jiffies =
						mbraink_tracing_pidlist_data[i].jiffies;
				tracing_pid_buffer->tracing_count++;
				/*Deal with the end process record*/
				if (mbraink_tracing_pidlist_data[i].end != 0) {
					mbraink_tracing_pidlist_data[i].pid = 0;
					mbraink_tracing_pidlist_data[i].tgid = 0;
					mbraink_tracing_pidlist_data[i].uid = 0;
					mbraink_tracing_pidlist_data[i].priority = 0;
					memset(mbraink_tracing_pidlist_data[i].name,
						0, TASK_COMM_LEN);
					mbraink_tracing_pidlist_data[i].start = 0;
					mbraink_tracing_pidlist_data[i].end = 0;
					mbraink_tracing_pidlist_data[i].jiffies = 0;
					mbraink_tracing_pidlist_data[i].dirty = false;
				} else {
					mbraink_tracing_pidlist_data[i].dirty = false;
				}
			} else {
				ret = -1;
				tracing_pid_buffer->tracing_idx = i;
				break;
			}
		}
	}
	pr_info("%s: current_idx = %u, count = %u\n",
		__func__, tracing_pid_buffer->tracing_idx, tracing_pid_buffer->tracing_count);
	spin_unlock_irqrestore(&tracing_pidlist_lock, flags);
}

void mbraink_get_binder_trace_info(unsigned short current_idx,
				struct mbraink_binder_trace_data *binder_trace_buffer)
{
	int i = 0;
	int ret = 0;
	unsigned long flags;
	unsigned short tracing_cnt = 0;

	spin_lock_irqsave(&binder_trace_lock, flags);

	memset(binder_trace_buffer, 0, sizeof(struct mbraink_binder_trace_data));

	for (i = current_idx; i < MAX_BINDER_TRACE_NUM; i++) {
		if (mbraink_binder_tracelist_data[i].dirty == false)
			continue;
		else {
			tracing_cnt = binder_trace_buffer->tracing_count;
			if (tracing_cnt < MAX_TRACE_PID_NUM) {
				binder_trace_buffer->drv_data[tracing_cnt].from_pid =
						mbraink_binder_tracelist_data[i].from_pid;
				binder_trace_buffer->drv_data[tracing_cnt].from_tid =
						mbraink_binder_tracelist_data[i].from_tid;
				binder_trace_buffer->drv_data[tracing_cnt].to_pid =
						mbraink_binder_tracelist_data[i].to_pid;
				binder_trace_buffer->drv_data[tracing_cnt].count =
						mbraink_binder_tracelist_data[i].count;
				binder_trace_buffer->tracing_count++;
				mbraink_binder_tracelist_data[i].dirty = false;
			} else {
				ret = -1;
				binder_trace_buffer->tracing_idx = i;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&binder_trace_lock, flags);
}

#else
int mbraink_process_tracer_init(void)
{
	pr_info("%s: Do not support mbraink tracing...\n", __func__);
	return 0;
}

void mbraink_process_tracer_exit(void)
{
	pr_info("%s: Do not support mbraink tracing...\n", __func__);
}

int mbraink_get_tracing_pid_info(unsigned short current_idx,
				struct mbraink_tracing_pid_data *tracing_pid_buffer)
{
	pr_info("%s: Do not support mbraink tracing...\n", __func__);
	return 0;
}
void mbraink_get_binder_trace_info(unsigned short current_idx,
				struct mbraink_binder_trace_data *binder_trace_buffer)
{
	pr_info("%s: Do not support mbraink binder tracing...\n", __func__);
}
#endif
