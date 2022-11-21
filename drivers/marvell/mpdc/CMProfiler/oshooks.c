/*
** (C) Copyright 2009 Marvell International Ltd.
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

#include <linux/types.h>
#include <linux/linkage.h>
#include <linux/ptrace.h>
#include <linux/file.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <asm/mman.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/prctl.h>
#include <linux/profile.h>
#include <asm/thread_notify.h>

#include "PXD_cm.h"
#include "cm_drv.h"
#include "cm_dsa.h"
#include "CMProfilerSettings.h"
#include "ring_buffer.h"
#include "common.h"

static unsigned long g_buffer[sizeof(PXD32_CMProcessCreate) + PATH_MAX];

struct mmap_arg_struct {
	unsigned long add;
	unsigned long len;
	unsigned long prot;
	unsigned long flags;
	unsigned long fd;
	unsigned long offset;
};

typedef asmlinkage int (*sys_fork_t) (struct pt_regs *regs);
typedef asmlinkage int (*sys_vfork_t) (struct pt_regs *regs);
typedef asmlinkage int (*sys_clone_t) (unsigned long clone_flgs, unsigned long newsp, struct pt_regs *regs);
typedef asmlinkage int (*sys_execve_t) (const char *filenameei, char ** const argv, char ** const envp, struct pt_regs *regs);
typedef asmlinkage int (*sys_exit_t) (int error_code);
typedef asmlinkage int (*sys_exit_group_t) (int error_code);
typedef asmlinkage long (*sys_kill_t) (int pid, int sig);
typedef asmlinkage long (*sys_tkill_t) (int pid, int sig);
typedef asmlinkage long (*sys_tgkill_t) (int pid, int tid, int sig);
typedef asmlinkage long (*sys_prctl_t) (int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);


/* formally we use register IP to calculate SP value, which is incorrect.
 * code optimization will use IP register for temp usage, thus mess up.
 * here we use fp to cal SP value.
 */
#define INIT_STACK_FRAME \
	char *originstack = NULL;\
	char *stackbuf = NULL;\
	do{\
		__asm__("add %0, fp, #4\n\t":"=r"(originstack):);\
	} while(0)\

#define APPEND_STACK_FRAME \
	do{\
	__asm__(\
		"sub sp, sp, #84\n\t"\
		"str sp, %0"\
		:"=m"(stackbuf)\
	);\
	memcpy(stackbuf, originstack, 84);\
	} while(0)\

#define CUTTAIL_STACK_FRAME \
	do{\
		memcpy(originstack, stackbuf, 84);\
		__asm__("add sp, sp, #84");\
	} while(0)

static sys_vfork_t          px_original_sys_vfork  = NULL;
static sys_fork_t           px_original_sys_fork   = NULL;
static sys_clone_t          px_original_sys_clone  = NULL;
static sys_execve_t         px_original_sys_execve = NULL;
static sys_prctl_t          px_original_sys_prctl      = NULL;

static sys_exit_t           px_original_sys_exit = NULL;
static sys_exit_group_t     px_original_sys_exit_group = NULL;
static sys_kill_t           px_original_sys_kill = NULL;
static sys_tkill_t          px_original_sys_tkill = NULL;
static sys_tgkill_t         px_original_sys_tgkill = NULL;

static bool gb_enable_os_hooks = false;

//DECLARE_MUTEX(fork_mutex);
static DEFINE_MUTEX(fork_mutex);

static void launched_app_exit_notif(void)
{
	g_launched_app_exit_cm = true;

	if (waitqueue_active(&pxcm_kd_wait))
		wake_up_interruptible(&pxcm_kd_wait);
}

static void check_launched_app_exit(pid_t pid)
{
	if (g_is_app_exit_set_cm && (pid == g_launch_app_pid_cm))
	{
		g_is_app_exit_set_cm = false;
		launched_app_exit_notif();
	}
}

pid_t get_idle_task_pid(void)
{
	return 0;
}

const char * get_idle_task_name(void)
{
	return "PXIdle";
}

#define KTHREAD_HASH_TABLE_SIZE 1000

static struct hlist_head kthread_hash_table[KTHREAD_HASH_TABLE_SIZE];

struct kthread_struct
{
	struct hlist_node list;
	struct task_struct * task;
	bool   is_name_modified;     /* task's orignal name has been modified, see kthread_create_on_node() in kernel/kthread.c */
};

static int kthread_hash_func(pid_t pid)
{
	return pid % KTHREAD_HASH_TABLE_SIZE;
}

/*
 * Check if a thread is in the kernel thread list
 * if yes, return the kthread_struct pointer in the kernel thread list
 * else, return NULL
 */
static struct kthread_struct * in_kthread_list(struct task_struct * task)
{
	int i;
	struct kthread_struct * kth;
	struct hlist_node  *node;
	struct hlist_head  *head;

	i = kthread_hash_func(task->pid);

	head = &kthread_hash_table[i];

	if (!hlist_empty(head))
	{
		hlist_for_each_entry(kth, node, head, list)
		{
			if ((kth != NULL) && (task->pid == kth->task->pid))
			{
				return kth;
			}
		}
	}
	
	return NULL;
}

/* insert a task into the kernel thread list */
static void insert_into_kthread_list(struct task_struct * task)
{
	int i;
	struct kthread_struct * kth;
	struct hlist_head  *head;

	i = kthread_hash_func(task->pid);

	head = &kthread_hash_table[i];

	/* the thread create info is not saved */
	kth = kzalloc(sizeof(struct kthread_struct), GFP_ATOMIC);
	kth->task = task;
	
	if (strcmp(task->comm, KTHREADD_NAME) == 0)
		kth->is_name_modified = false;
	else
		kth->is_name_modified = true;

	hlist_add_head(&kth->list, &kthread_hash_table[i]);
}

static void remove_from_kthread_list(struct task_struct * task)
{
	int i;
	struct kthread_struct * kth;
	struct hlist_node  *node;
	struct hlist_node  *n;
	struct hlist_head  *head;

	i = kthread_hash_func(task->pid);

	head = &kthread_hash_table[i];

	if (!hlist_empty(head))
	{
		hlist_for_each_entry_safe(kth, node, n, head, list)
		{
			if ((kth != NULL) && (task->pid == kth->task->pid))
			{
				hlist_del(node);
				return;
			}
		}
	}
}

static void clear_kthread_list(void)
{
	int i;
	struct kthread_struct * kth;
	struct hlist_node  *node;
	struct hlist_node  *n;

	for (i=0; i<KTHREAD_HASH_TABLE_SIZE; i++)
	{
		hlist_for_each_entry_safe(kth, node, n, &kthread_hash_table[i], list)
		{
			hlist_del(node);
		}
	}
}

static void write_process_info_by_task(struct task_struct * task, const char * cmd_line, unsigned long long ts)
{
	bool need_flush;
	bool buffer_full;
	unsigned int name_len;
	unsigned int extra_len; /* length of the name which exceeds to size of PXD32_CMProcessCreate */
	unsigned int offset;

	PXD32_CMProcessCreate *p_info;

	if (task == NULL)
		return;

	if (is_specific_object_mode())
	{
		return;
	}

	memset(g_buffer, 0, sizeof(g_buffer));

	p_info = (PXD32_CMProcessCreate *)&g_buffer;

	p_info->timestamp = convert_to_PXD32_DWord(ts);
	p_info->pid       = task->tgid;
	p_info->flag      = 0;

	memcpy(p_info->pathName, cmd_line, strlen(cmd_line) + 1);

	notify_new_loaded_process(task, cmd_line);

	/* if it is the idle process, set the corresponding flag */
	if (p_info->pid == get_idle_task_pid())
	{
		p_info->flag |= CM_PROCESS_FLAG_IDLE_PROCESS;

		if (p_info->pathName[0] == '\0')
		{
			strcpy(p_info->pathName, get_idle_task_name());
		}
	}

	if (g_is_app_exit_set_cm && (task->tgid == g_launch_app_pid_cm))
	{
		p_info->flag |= CM_MODULE_RECORD_FLAG_AUTO_LAUNCH_PID;
	}

	name_len  = (strlen(p_info->pathName) + 1) * sizeof(char);

	/* calculate the offset of pathName[0] from the start address of structure PXD32_CMProcessCreate */
	offset = (char *)&(p_info->pathName[0]) - (char *)p_info;

	if (name_len <= sizeof(PXD32_CMProcessCreate) - offset)
	{
		extra_len = 0;
	}
	else
	{
		extra_len = name_len - (sizeof(PXD32_CMProcessCreate) - offset);
	}

#if 0
	if (is_full_path)
	{
		p_info->nameOffset = get_filename_offset(cmd_line);
	}
	
	else
	{
		p_info->nameOffset = 0;
	}
#else
	p_info->nameOffset = get_filename_offset(cmd_line);
#endif

	p_info->recLen =   sizeof(PXD32_CMProcessCreate)
			         + ALIGN_UP(extra_len, sizeof(unsigned long));

	write_ring_buffer(&g_process_create_buf_info.buffer, p_info, p_info->recLen, &buffer_full, &need_flush);

	if (need_flush && !g_process_create_buf_info.is_full_event_set)
	{
		g_process_create_buf_info.is_full_event_set = true;

		if (waitqueue_active(&pxcm_kd_wait))
			wake_up_interruptible(&pxcm_kd_wait);
	}

}

static void write_process_info_by_pid(pid_t pid, unsigned long long ts)
{
	struct task_struct * task;
	char *proc_name;

	proc_name = kzalloc(PATH_MAX, GFP_ATOMIC);

	if (proc_name != NULL)
	{
		task = px_find_task_by_pid(pid);

		if (task != NULL)
		{
			int len;

			len = get_proc_name(task, proc_name);

			if (len != 0)
			{
				write_process_info_by_task(task, proc_name, ts);
			}
			else
			{
				write_process_info_by_task(task, task->comm, ts);
			}
		}

		kfree(proc_name);
	}
}

static asmlinkage int px_sys_fork(struct pt_regs *regs)
{
	int ret = 0;
	unsigned long long ts;

	INIT_STACK_FRAME;
	mutex_lock(&fork_mutex);

	APPEND_STACK_FRAME;

	ts = get_timestamp();

	ret = px_original_sys_fork(NULL);

	mutex_unlock(&fork_mutex);
	CUTTAIL_STACK_FRAME;

	if ((ret >= 0) && (gb_enable_os_hooks))
	{
		write_process_info_by_pid(ret, ts);
	}

	//mutex_unlock(&fork_mutex);

	return ret;
}

static asmlinkage int px_sys_vfork(struct pt_regs *regs)
{
	int ret = 0;
	unsigned long long ts;

	INIT_STACK_FRAME;
	mutex_lock(&fork_mutex);
	mutex_unlock(&fork_mutex);

	APPEND_STACK_FRAME;

	ts = get_timestamp();

	ret = px_original_sys_vfork(regs);

	CUTTAIL_STACK_FRAME;

	if ((ret >= 0) && (gb_enable_os_hooks))
	{
		write_process_info_by_pid(ret, ts);
	}

	return ret;
}

static asmlinkage int px_sys_clone(unsigned long clone_flgs, unsigned long newsp, struct pt_regs *regs)
{
	int ret = 0;
	unsigned long long ts;

	INIT_STACK_FRAME;

	mutex_lock(&fork_mutex);

	APPEND_STACK_FRAME;

	ts = get_timestamp();

	ret = px_original_sys_clone(clone_flgs, newsp, regs);

	mutex_unlock(&fork_mutex);
	CUTTAIL_STACK_FRAME;

	if ((ret >= 0) && (gb_enable_os_hooks))
	{
		if (!(clone_flgs & CLONE_THREAD))
		{
		    write_process_info_by_pid(ret, ts);
		}
	}

	//mutex_unlock(&fork_mutex);

	return ret;
}

static asmlinkage int px_sys_execve(const char *filenameei, char ** const argv, char ** const envp, struct pt_regs *regs)
{
	int ret;
	unsigned long long ts;

	INIT_STACK_FRAME;

	/**
	 * here we just use the mutex to make sure the fork call is finished,
	 *   no need to keep the mutex till the call finished
	 **/
	mutex_lock(&fork_mutex);
	mutex_unlock(&fork_mutex);

	APPEND_STACK_FRAME;

	ts = get_timestamp();

	ret = px_original_sys_execve(filenameei, argv, envp, regs);

	CUTTAIL_STACK_FRAME;

	if ((ret >= 0) && (gb_enable_os_hooks))
	{
		write_process_info_by_pid(current->tgid, ts);
#ifdef CONFIG_GLOBAL_PREEMPT_NOTIFIERS
		write_thread_create_info(current->tgid, current->pid, ts);
#endif
	}

	return ret;
}

static asmlinkage int px_sys_exit(int error_code)
{
	long ret = 0;
	/**
	 * We must save the original sys_exit function here. If the current pid
	 * is the application the user launched and user selected "stop sampling
	 * on application unload", then the function "enum_module_for_exit_check"
	 * will terminate the sampling session, which in turn will remove all the
	 * syscall hooks. On 2.4 kernel it is not going to happen, because the
	 * utility has no chance to run until this function returns; but on 2.6
	 * kernel with preempt turned on, the utility maybe scheduled run before
	 * the following call to vt_original_sys_exit, so at that time the module
	 * tracker is already unloaded and the vt_original_sys_exit becomes NULL
	 * pointer. The following vt_sys_exit_group has the same situation.
	 */
	sys_exit_t saved_sys_exit = px_original_sys_exit;

	if (gb_enable_os_hooks)
	{
		check_launched_app_exit(current->pid);
	}

	ret = saved_sys_exit(error_code);

	return ret;
}

static asmlinkage int px_sys_exit_group(int error_code)
{
	long ret = 0;
	sys_exit_group_t saved_sys_exit_group = px_original_sys_exit_group;

	if (gb_enable_os_hooks)
	{
		check_launched_app_exit(current->tgid);
	}

	ret = saved_sys_exit_group(error_code);

	return ret;
}

static bool is_kill_sig(int sig)
{
	switch (sig)
	{
	case SIGINT:
	case SIGKILL:
	case SIGTERM:
	case SIGABRT:
	case SIGFPE:
	case SIGILL:
	case SIGQUIT:
	case SIGSEGV:
	case SIGBUS:
		return true;
	default:
		return false;
	}
}

static asmlinkage long px_sys_kill(int pid, int sig)
{
	long ret = 0;

	ret = px_original_sys_kill(pid, sig);

	if (!ret && gb_enable_os_hooks & is_kill_sig(sig))
	{
		check_launched_app_exit(pid);
	}

	return ret;
}

static asmlinkage long px_sys_tkill(int pid, int sig)
{
	long ret = 0;

	ret = px_original_sys_tkill(pid, sig);

	if (!ret && gb_enable_os_hooks & is_kill_sig(sig))
	{
		check_launched_app_exit(pid);
	}

	return ret;
}

static asmlinkage long px_sys_tgkill(int pid, int tid, int sig)
{
	long ret = 0;

	ret = px_original_sys_tgkill(pid, tid, sig);

	if (!ret && gb_enable_os_hooks & is_kill_sig(sig))
	{
		check_launched_app_exit(pid);
	}

	return ret;
}

/* 
 * we hook this system call in order to get the new process name 
 * if it is modified by changing argv[0]
 * assume that it is followed by a prctl(PR_SET_NAME, ...)
 */
static asmlinkage long px_sys_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
	long ret = 0;
	char * new_name = NULL;
	char * old_name = NULL;
	unsigned long long ts;

	INIT_STACK_FRAME;

	APPEND_STACK_FRAME;
	
	ts = get_timestamp();

	ret = px_original_sys_prctl(option, arg2, arg3, arg4, arg5);
	
	CUTTAIL_STACK_FRAME;

	if ((ret == 0) && (option == PR_SET_NAME))
	{
		new_name = kzalloc(PATH_MAX, GFP_ATOMIC);
		old_name = kzalloc(PATH_MAX, GFP_ATOMIC);
		
		if ((new_name != NULL) && (old_name != NULL))
		{
			if (is_proc_name_modified(current, new_name, old_name))
			{
				write_process_info_by_task(current, new_name, ts);
			}
		}
		
		if (new_name != NULL)
			kfree(new_name);

		if (old_name != NULL)
			kfree(old_name);
	}

	return ret;
}

static void notify_task_exit(struct thread_info * thread)
{
        struct task_struct * task = thread->task;

	if (is_kernel_thread(task))
		remove_from_kthread_list(task);

        if (gb_enable_os_hooks)
        {
                check_launched_app_exit(task->pid);
        }
}

/* 
 * Since kernel thread creation can't be tracked by hooking the system call table,
 * we check if it is a new kernel thread when the thread switch event happens
 */
static void notify_task_switch(struct thread_info * thread)
{
	struct task_struct * next;
	unsigned long long ts;
	struct kthread_struct * kth;

	ts = get_timestamp();
	
	next = thread->task;

	/* if the next thread is not a kernel thread, do nothing */
	if (!is_kernel_thread(next))
	{
		return;
	}

	kth = in_kthread_list(next);
	
	if (kth == NULL)
	{
		/* a new kernel thread */
		insert_into_kthread_list(next);
		write_process_info_by_task(next, next->comm, ts);
	}
	else
	{
		if (!kth->is_name_modified && (strcmp(next->comm, KTHREADD_NAME) != 0))
		{
			/* the kernel thread's name is modified in kthread_create_on_node() in kernel/kthread.c */
			kth->is_name_modified = true;
			write_process_info_by_task(next, next->comm, ts);
		}
	}
}

static int thread_event_notify(struct notifier_block *self, unsigned long cmd, void *t)
{
	struct thread_info *thread = t;

	switch (cmd)
	{
	case THREAD_NOTIFY_EXIT:
		notify_task_exit(thread);
		break;

	case THREAD_NOTIFY_SWITCH:
		notify_task_switch(thread);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block thread_notifier_block = {
	.notifier_call	= thread_event_notify,
};

void static_enum_processes(void)
{
	struct task_struct *p = &init_task;

	write_process_info_by_task(&init_task, "swapper", 0);

	do
	{
		if (is_kernel_thread(p))
		{
			insert_into_kthread_list(p);
		}

		write_process_info_by_pid(p->tgid, 0);

		rcu_read_lock();

		p = next_task(p);

		rcu_read_unlock();

	} while (p != &init_task);
}

#ifdef CONFIG_PROFILING
static int task_exit_notify(struct notifier_block *self, unsigned long val, void *data)
{
	struct task_struct * task = (struct task_struct *)data;
	
	if (gb_enable_os_hooks)
	{
		check_launched_app_exit(task->pid);
	}

	return 0;
}

static struct notifier_block task_exit_nb = {
        .notifier_call  = task_exit_notify,
};
#endif

int install_os_hooks(void)
{
	int ret;

	if (g_mode != CM_MODE_SYSTEM) {
	ret = thread_register_notifier(&thread_notifier_block);
	if (ret != 0)
	{
		printk(KERN_ERR "[CPA] thread_register_notifier() fails\n");
		return -EFAULT;
	}
	}

#ifdef CONFIG_PROFILING
	ret = profile_event_register(PROFILE_TASK_EXIT, &task_exit_nb);
	if (ret != 0)
	{
		printk(KERN_ERR "[CPA] profile_event_register() fails\n");
		return -EFAULT;
	}
#endif

	if (g_mode != CM_MODE_SYSTEM)
	{
		px_original_sys_fork = (sys_fork_t) xchg(&system_call_table_cm[__NR_fork - __NR_SYSCALL_BASE], px_sys_fork);

		px_original_sys_vfork = (sys_vfork_t) xchg(&system_call_table_cm[__NR_vfork - __NR_SYSCALL_BASE], px_sys_vfork);

		px_original_sys_clone = (sys_clone_t) xchg(&system_call_table_cm[__NR_clone - __NR_SYSCALL_BASE], px_sys_clone);

		px_original_sys_execve = (sys_execve_t) xchg(&system_call_table_cm[__NR_execve - __NR_SYSCALL_BASE], px_sys_execve);

		px_original_sys_prctl  = (sys_prctl_t) xchg(&system_call_table_cm[__NR_prctl - __NR_SYSCALL_BASE], px_sys_prctl);
	}

	px_original_sys_exit = (sys_exit_t) xchg(&system_call_table_cm[__NR_exit - __NR_SYSCALL_BASE], px_sys_exit);

	px_original_sys_exit_group = (sys_exit_group_t) xchg(&system_call_table_cm[__NR_exit_group - __NR_SYSCALL_BASE], px_sys_exit_group);

	px_original_sys_kill = (sys_kill_t) xchg(&system_call_table_cm[__NR_kill - __NR_SYSCALL_BASE], px_sys_kill);

	px_original_sys_tkill = (sys_tkill_t) xchg(&system_call_table_cm[__NR_tkill - __NR_SYSCALL_BASE], px_sys_tkill);

	px_original_sys_tgkill = (sys_tgkill_t) xchg(&system_call_table_cm[__NR_tgkill - __NR_SYSCALL_BASE], px_sys_tgkill);

	gb_enable_os_hooks = true;

	return 0;
}

int remove_os_hooks(void)
{
	void *orgFn;

	/* only remove the os hooks when install_os_hooks() has been called */
	if (!gb_enable_os_hooks)
		return 0;

	if (g_mode != CM_MODE_SYSTEM)
	{
		if ((orgFn = xchg(&px_original_sys_fork, 0)))
			orgFn = xchg(&system_call_table_cm[__NR_fork - __NR_SYSCALL_BASE], orgFn);

		if ((orgFn = xchg(&px_original_sys_vfork, 0)))
			orgFn = xchg(&system_call_table_cm[__NR_vfork - __NR_SYSCALL_BASE], orgFn);

		if ((orgFn = xchg(&px_original_sys_clone, 0)))
			orgFn = xchg(&system_call_table_cm[__NR_clone - __NR_SYSCALL_BASE], orgFn);

		if ((orgFn = xchg(&px_original_sys_execve, 0)))
			orgFn = xchg(&system_call_table_cm[__NR_execve - __NR_SYSCALL_BASE], orgFn);

		if ((orgFn = xchg(&px_original_sys_prctl, 0)))
			orgFn = xchg(&system_call_table_cm[__NR_prctl - __NR_SYSCALL_BASE], orgFn);

	}

	if ((orgFn = xchg(&px_original_sys_exit, 0)))
		orgFn = xchg(&system_call_table_cm[__NR_exit - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_exit_group, 0)))
		orgFn = xchg(&system_call_table_cm[__NR_exit_group - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_kill, 0)))
		orgFn = xchg(&system_call_table_cm[__NR_kill - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_tkill, 0)))
		orgFn = xchg(&system_call_table_cm[__NR_tkill - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_tgkill, 0)))
		orgFn = xchg(&system_call_table_cm[__NR_tgkill - __NR_SYSCALL_BASE], orgFn);

#ifdef CONFIG_PROFILING
	// unregister task exit notifier
	profile_event_unregister(PROFILE_TASK_EXIT, &task_exit_nb);
#endif

	thread_unregister_notifier(&thread_notifier_block);

	clear_kthread_list();

	gb_enable_os_hooks = false;

	return 0;
}

int start_os_hooks(void)
{
	int ret;

	if (g_mode != CM_MODE_SYSTEM)
	{
		static_enum_processes();
	}

	ret = install_os_hooks();

	return ret;
}

int stop_os_hooks(void)
{
	int ret;

	ret = remove_os_hooks();
	
	clear_loaded_process_list();

	return ret;
}
