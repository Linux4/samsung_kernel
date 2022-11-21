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

#include <linux/linkage.h>
#include <linux/ptrace.h>
#include <linux/file.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <asm/mman.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/prctl.h>
#include <linux/profile.h>
#include <asm/thread_notify.h>

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
#include <linux/fdtable.h>
#endif

#include "PXD_tp.h"
#include "tp_dsa.h"
#include "tp_drv.h"
#include "ring_buffer.h"
#include "vdkiocode.h"
#include "common.h"
#include "ufunc_hook.h"
#include "funchooks.h"

//extern void get_link_map(pid_t pid);
#define MODULE_FLAG_1ST       0x00000001
#define MODULE_FLAG_GLOBAL    0x00000002

static bool g_static_enum = false;

static unsigned long g_buffer[sizeof(PXD32_TP_Module) + PATH_MAX];

struct mmap_arg_struct {
	unsigned long add;
	unsigned long len;
	unsigned long prot;
	unsigned long flags;
	unsigned long fd;
	unsigned long offset;
};

extern int write_thread_create_info(struct task_struct * task,
                                    unsigned int pc,
                                    unsigned long long ts,
                                    pid_t new_pid,
                                    pid_t new_tid,
                                    unsigned int priority);

extern int user_thread_exit(struct task_struct * task,
                             unsigned int         pc,
                             unsigned long long   ts,
                             int                  ret_value);

extern int user_process_exit(struct task_struct * task,
                             unsigned int         pc,
                             unsigned long long   ts,
                             int                  ret_value);

typedef asmlinkage int (*sys_fork_t) (struct pt_regs *regs);
typedef asmlinkage int (*sys_vfork_t) (struct pt_regs *regs);
typedef asmlinkage int (*sys_clone_t) (unsigned long clone_flgs,
                                       unsigned long newsp,
                                       struct pt_regs *regs);

typedef asmlinkage int (*sys_execve_t) (const char *filenameei,
                                        char ** const argv,
                                        char ** const envp,
                                        struct pt_regs *regs);

typedef asmlinkage int (*sys_mmap_t) (struct mmap_arg_struct *arg);
typedef asmlinkage int (*sys_mmap2_t) (unsigned long addr,
                                       unsigned long len,
                                       unsigned long prot,
                                       unsigned long flags,
                                       unsigned long fd,
                                       unsigned long pgoff);

typedef asmlinkage int (*sys_munmap_t) (unsigned long addr,
                                       unsigned long len);

typedef asmlinkage int (*sys_exit_t) (int error_code);
typedef asmlinkage int (*sys_exit_group_t) (int error_code);
typedef asmlinkage long (*sys_kill_t) (int pid, int sig);
typedef asmlinkage long (*sys_tkill_t) (int pid, int sig);
typedef asmlinkage long (*sys_tgkill_t) (int pid, int tid, int sig);
typedef asmlinkage long (*sys_prctl_t) (int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);

//extern PXD32_DWord convert_to_PXD32_DWord(unsigned long long n);
//extern int handle_image_loaded_tp(void);
extern void static_enum_thread(struct task_struct *task);

extern bool remove_fci_by_tgid(pid_t tgid);

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

static sys_vfork_t          px_original_sys_vfork      = NULL;
static sys_fork_t           px_original_sys_fork       = NULL;
static sys_clone_t          px_original_sys_clone      = NULL;
static sys_execve_t         px_original_sys_execve     = NULL;
static sys_mmap_t           px_original_sys_mmap       = NULL;
static sys_mmap2_t          px_original_sys_mmap2      = NULL;
//static sys_munmap_t         px_original_sys_munmap     = NULL;
static sys_exit_t           px_original_sys_exit       = NULL;
static sys_exit_group_t     px_original_sys_exit_group = NULL;
static sys_kill_t           px_original_sys_kill       = NULL;
static sys_tkill_t          px_original_sys_tkill      = NULL;
static sys_tgkill_t         px_original_sys_tgkill     = NULL;
static sys_prctl_t          px_original_sys_prctl      = NULL;

static bool gb_enable_os_hooks = false;

static DEFINE_MUTEX(fork_mutex);
static DEFINE_SPINLOCK(buf_lock);
DEFINE_MUTEX(mt_mutex);

static void create_tp_module(const char *  module_path,
                             unsigned long name_offset,
                             pid_t         pid,
                             unsigned long address,
                             unsigned long size,
                             unsigned long flag,
                             unsigned long long timestamp,
                             PXD32_TP_Module *p_info)
{
	unsigned int offset;
	unsigned int name_len;
	unsigned int extra_len;
	bool is_so;
    struct module* mod;

        if(0 == address) {
                mod = find_module(module_path);
                if(mod != NULL) {
                address = (unsigned long)(mod->module_core);
                size = mod->core_size; 
                }
        }
#if 0
	struct task_struct *task;
	task = px_find_task_by_pid(pid);
#endif

	p_info->pid     = pid;
	p_info->timestamp.low  = timestamp;
	p_info->timestamp.high = timestamp >> 32;
	p_info->loadAddr = address;
	p_info->codeSize = size;

	//if (is_exe_module(address) && (flag & MODULE_FLAG_1ST))
	if (flag & MODULE_FLAG_1ST)
	{
		p_info->flag |= TP_MODULE_RECORD_FLAG_EXE;

		if (g_is_app_exit_set_tp && (pid == g_launch_app_pid_tp))
		{
			p_info->flag |= TP_MODULE_RECORD_FLAG_AUTO_LAUNCH_PID;
		}
	}

	if (flag & MODULE_FLAG_GLOBAL)
	{
		p_info->flag |= TP_MODULE_RECORD_FLAG_GLOBAL_MODULE;
	}

	if (flag & MODULE_FLAG_1ST)
	{
		p_info->flag |= TP_MODULE_RECORD_FLAG_FIRST_MODULE_IN_PROCESS;
	}


	if (p_info->flag & TP_MODULE_RECORD_FLAG_EXE)
	{
		is_so = false;
	}
	else
	{
		is_so = true;
	}

	memcpy(p_info->pathName, module_path, strlen(module_path) + 1);

	offset = (char *)&(p_info->pathName[0]) - (char *)p_info;

	name_len  = (strlen(p_info->pathName) + 1) * sizeof(char);

	if (name_len <= sizeof(PXD32_TP_Module) - offset)
	{
		extra_len = 0;
	}
	else
	{
		extra_len = name_len - (sizeof(PXD32_TP_Module) - offset);
	}

	p_info->nameOffset = name_offset;

	p_info->length =  sizeof(PXD32_TP_Module)
			        + ALIGN_UP(extra_len, sizeof(unsigned long));

}

int add_module_record_tp(struct add_module_data * data)
{
	int ret = 0;
	bool need_flush;
	bool buffer_full;
	struct add_module_data amd;
	PXD32_TP_Module *p_info;
	char * name = NULL;

	spin_lock(&buf_lock);

	if (copy_from_user(&amd, data, sizeof(struct add_module_data)) != 0)
	{
		ret = -EFAULT;
		goto end;
	}

	name = kzalloc(PATH_MAX, GFP_ATOMIC);

	if (name == NULL)
	{
		ret = -ENOMEM;
		goto end;
	}

	if (strncpy_from_user(name, data->name, PATH_MAX) < 0)
	{
		ret = -EFAULT;
		goto end;
	}

	memset(g_buffer, 0, sizeof(g_buffer));

	p_info = (PXD32_TP_Module *)&g_buffer;

	create_tp_module(name, amd.name_offset, amd.pid, amd.address, amd.size, amd.flag, amd.lsc, p_info);

	write_ring_buffer(&g_module_buffer_tp.buffer, p_info, p_info->length, &buffer_full, &need_flush);

	if (need_flush && !g_module_buffer_tp.is_full_event_set)
	{
		g_module_buffer_tp.is_full_event_set = true;

		if (waitqueue_active(&pxtp_kd_wait))
			wake_up_interruptible(&pxtp_kd_wait);
	}

end:
	if (name != NULL)
		kfree(name);

	spin_unlock(&buf_lock);

	return ret;
}

static void wait_image_load_notif(void)
{
	handle_image_loaded_tp();
}

static void launched_app_exit_notif(void)
{
	g_launched_app_exit_tp = true;

	if (waitqueue_active(&pxtp_kd_wait))
		wake_up_interruptible(&pxtp_kd_wait);
}

static void check_wait_for_image_load(const char *path)
{
	if (path == NULL)
		return;

	if (g_is_image_load_set_tp && !strcmp(path, g_image_load_path_tp))
	{
		g_is_image_load_set_tp = false;
		wait_image_load_notif();
	}
}

static void check_launched_app_exit(pid_t pid)
{
	if (g_is_app_exit_set_tp && (pid == g_launch_app_pid_tp))
	{
		g_is_app_exit_set_tp = false;
		launched_app_exit_notif();
	}
}

static void write_module_info(const char *  module_path,
                              unsigned long name_offset,
                              pid_t         pid,
                              unsigned long address,
                              unsigned long size,
                              unsigned long flag,
                              unsigned long long timestamp)
{
	PXD32_TP_Module *p_info;
	bool need_flush;
	bool buffer_full;

	spin_lock(&buf_lock);

	memset(g_buffer, 0, sizeof(g_buffer));

	p_info = (PXD32_TP_Module *)&g_buffer;

	create_tp_module(module_path, name_offset, pid, address, size, flag, timestamp, p_info);

	write_ring_buffer(&g_module_buffer_tp.buffer, p_info, p_info->length, &buffer_full, &need_flush);

	if (need_flush && !g_module_buffer_tp.is_full_event_set)
	{
		g_module_buffer_tp.is_full_event_set = true;

		if (waitqueue_active(&pxtp_kd_wait))
			wake_up_interruptible(&pxtp_kd_wait);
	}

	spin_unlock(&buf_lock);
}

static void module_load_notif(const char *  path,
                              unsigned long name_offset,
                              unsigned long pid,
                              unsigned long base,
                              unsigned long size,
                              unsigned long flag,
                              unsigned long long ts)
{
	if (!g_static_enum)
	{
		check_wait_for_image_load(path);
	}

	write_module_info(path, name_offset, pid, base, size, flag, ts);
}

/*
 * static enumeration kernel modules
 */
static void static_enum_kernel_modules(void)
{
	unsigned int text_offset;

#ifdef CONFIG_TEXT_OFFSET
	text_offset = CONFIG_TEXT_OFFSET;
#else
	/*
	 * PAGE_OFFSET is the load address of kernel
	 *
	 * 0x8000 is the offset to the text section of kernel
	 * which is defined as TEXT_OFFSET in arch/arm/Makefile
	 */
	text_offset = 0x8000;
#endif

	module_load_notif("swapper", 0, 0,
				      LINUX_APP_BASE_LOW, 0,
				      MODULE_FLAG_1ST, 0);

	/*
	 * PAGE_OFFSET is the load address of kernel
	 *
	 * 0x8000 is the offset to the text section of kernel
	 * which is defined as TEXT_OFFSET in arch/arm/Makefile
	 */
	module_load_notif("vmlinux", 0, 0,
	                  PAGE_OFFSET + text_offset, 4096 * 2 * 1024,
	                  MODULE_FLAG_GLOBAL, 0);
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

/*
 * enumerate all modules for process
 * pid: process id
 */
static void enum_modules_for_task(struct task_struct *task, unsigned long long ts, const char * proc_name)
{
	bool find = false;
	char * filename;
	unsigned long name_offset;
	unsigned int options;
	pid_t pid;	
	struct vm_area_struct *mmap;
	struct mm_struct *mm;
	unsigned int base, size;
	char * name = NULL;
	char * buffer = NULL;
	char * buffer_get_proc_name = NULL;
	int ret_get_proc_name = 0;

	if (task == NULL)
		goto ret;

	pid = task->pid;

	name = kzalloc(PATH_MAX, GFP_ATOMIC);

	if (name == NULL)
		goto ret;

	buffer = kzalloc(PATH_MAX, GFP_ATOMIC);
	buffer_get_proc_name = kzalloc(PATH_MAX, GFP_ATOMIC);

	if ((buffer == NULL) || (buffer_get_proc_name == NULL))
		goto ret;

	mm = get_task_mm(task);
	if (mm != NULL)
	{
		down_read(&mm->mmap_sem);

		for (mmap = mm->mmap; mmap; mmap = mmap->vm_next)
		{
			if (is_valid_module(mmap))
			{
				memset(name, 0, (PATH_MAX) * sizeof(char));
				filename = px_d_path(mmap->vm_file, name, PATH_MAX);

				if (filename != NULL)
				{
					options = 0;

					if (find == false)
					{
						options |= MODULE_FLAG_1ST;
						find = true;

						strcpy(buffer, filename);

						if (proc_name != NULL)
						{
							/* for the first module (the executable image), if the process name is specified, use the specified one */
							strcpy(buffer, proc_name);
						}
						else
						{
							/*
							 * for the first module (the executable image), we need to get the process name again
							 * because it may be modified by changing the argv[0]
							 */
							strcpy(buffer, buffer_get_proc_name);
							if(ret_get_proc_name == 0)
							{
								/* failed to get the process name, use the orignal module name */
								strcpy(buffer, filename);
							}

#if 0
							if (get_proc_name(task, buffer) == 0)
							{
								/* failed to get the process name, use the orignal module name */
								strcpy(buffer, filename);
							}
#endif

						}

						/* save the orignal process name for checking if it will be updated later */
						notify_new_loaded_process(task, buffer);
					}
					else
					{
						memset(buffer, 0, sizeof(PATH_MAX));
						strcpy(buffer, filename);
					}

					name_offset = get_filename_offset(buffer);

					base = mmap->vm_start;
					size = mmap->vm_end - mmap->vm_start;

					up_read(&mm->mmap_sem);
					module_load_notif(buffer, name_offset, pid, base, size, options, ts);
					down_read(&mm->mmap_sem);
				}
			}
		}

		up_read(&mm->mmap_sem);
		mmput(mm);
	}

	if (find == false)
	{
		module_load_notif(task->comm, 0, pid,
				  LINUX_APP_BASE_LOW, 0,//mmap->vm_end - mmap->vm_start,
				  MODULE_FLAG_1ST, ts);
	}

ret:
	if (name != NULL)
		kfree(name);

	if (buffer != NULL)
		kfree(buffer);

	if (buffer_get_proc_name != NULL)
		kfree(buffer_get_proc_name);

	return;
}

/*
 * enumerate all modules for process
 * pid: process id
 * lsc: load sample count
 * proc_name: new process name for this process. if NULL, use the original name
 */
static void enum_modules_for_process(pid_t pid, unsigned long long ts, const char * proc_name)
{
	struct task_struct * task;

	task = px_find_task_by_pid(pid);

	enum_modules_for_task(task, ts, proc_name);
}

static void static_enum_exiting_process_modules(void)
{
	struct task_struct *p_task = &init_task;

	do
	{
		/* if it is a kernel thread, and not in the kthread list, insert into the kthread list */
		if (is_kernel_thread(p_task))
		{
			insert_into_kthread_list(p_task);
		}

		/* static enumeration: load sample count of modules are all zero */
		enum_modules_for_task(p_task, 0, NULL);

		/* static enumeration for user threads */
		if (!is_kernel_task(p_task) && (p_task->pid > 1))
		{
			//static_enum_thread(p_task);
		}

		rcu_read_lock();

		p_task = next_task(p_task);

		rcu_read_unlock();
	}while (p_task != &init_task);

}



static int module_init_notifier(struct notifier_block *self,
                         unsigned long event,
                         void *arg)
{
	struct module *mod = (struct module *)arg;
	unsigned int name_offset;
	unsigned long long ts;

	ts = get_timestamp();

	name_offset = 0;

	if (event == MODULE_STATE_COMING)
	{
		if (mod->init_size > 0)
		{
			module_load_notif((char *)mod->name, name_offset, 0,
				              (__u32)mod->module_init, mod->init_size,
				              MODULE_FLAG_GLOBAL, ts);
		}

		if (mod->core_size > 0)
		{
			module_load_notif((char *)mod->name, name_offset, 0,
				              (__u32)mod->module_core, mod->core_size,
				              MODULE_FLAG_GLOBAL, ts);
		}
	}

	return 0;
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

	//mutex_unlock(&fork_mutex);
	CUTTAIL_STACK_FRAME;

	if ((ret > 0) && (gb_enable_os_hooks))
	{
		struct task_struct * child;

		child = px_find_task_by_pid(ret);

		notify_fork(current, child, task_pt_regs(current)->ARM_pc, ts);

		enum_modules_for_process(ret, ts, NULL);
	}

	mutex_unlock(&fork_mutex);

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

	if ((ret > 0) && (gb_enable_os_hooks))
	{
		struct task_struct * child;

		child = px_find_task_by_pid(ret);

		notify_vfork(current, child, task_pt_regs(current)->ARM_pc, ts);

		enum_modules_for_process(ret, ts, NULL);
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
	
	//mutex_unlock(&fork_mutex);
	CUTTAIL_STACK_FRAME;

	if ((ret >= 0) && (gb_enable_os_hooks))
	{
		struct task_struct * child;

		child = px_find_task_by_pid(ret);

		notify_clone(current, child, task_pt_regs(current)->ARM_pc, ts, clone_flgs);

		if (!(clone_flgs & CLONE_THREAD))
		{
			enum_modules_for_process(ret, ts, NULL);
		}
	}

	mutex_unlock(&fork_mutex);

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
	//mutex_unlock(&fork_mutex);

	APPEND_STACK_FRAME;

	ts = get_timestamp();

	ret = px_original_sys_execve(filenameei, argv, envp, regs);

	CUTTAIL_STACK_FRAME;

	if ((ret >= 0) && (gb_enable_os_hooks))
	{
		struct task_struct * child;

		child = px_find_task_by_pid(current->tgid);

		notify_exec(current, child, task_pt_regs(child)->ARM_pc, ts);

		//enum_modules_for_process(current->tgid, ts, NULL);
		enum_modules_for_task(current, ts, NULL);
	}

	mutex_unlock(&fork_mutex);

	return ret;
}


static asmlinkage int px_sys_mmap2(
		unsigned long addr, unsigned long len,
		unsigned long prot, unsigned long flgs,
		unsigned long fd, unsigned long pgoff
/*		unsigned long lRegSP*/
		)
{
	int ret = 0;
	int saved_r5;
	unsigned long long ts;
	struct file *file;
	char * name = NULL;

	INIT_STACK_FRAME;

	// here we must save r5 since it will be used by the OS sys_mmap2 code
	__asm__("str r5, %0\n\t":"=m"(saved_r5):);

	APPEND_STACK_FRAME;

	ts = get_timestamp();

	// restore r5
	__asm__("ldr r5, %0\n\t"::"m"(saved_r5):"r5");

	//mutex_lock(&fork_mutex);
	
	ret = px_original_sys_mmap2( addr, len, prot, flgs, fd, pgoff);

	CUTTAIL_STACK_FRAME;

	if (gb_enable_os_hooks && !IS_ERR((void *)ret) && (prot & PROT_EXEC) && !(flgs & MAP_ANONYMOUS))
	//if (gb_enable_os_hooks && !IS_ERR((void *)ret) && (prot & PROT_EXEC))
	{
		//rcu_read_lock();

		if ((file = fcheck(fd))!= NULL)
		{
			char *pname;

			//memset(name, 0, PATH_MAX * sizeof(char));
			name = kzalloc(PATH_MAX, GFP_ATOMIC);

			if (name != NULL)
			{
				pname = px_d_path(file, name, PATH_MAX);

				if (pname)
				{
					unsigned long name_offset;

					name_offset = get_filename_offset(pname);

					module_load_notif(pname, name_offset, current->tgid, ret, len, 0, ts);
				}

				kfree(name);
			}

		}
		//rcu_read_unlock();
	}
	
	//mutex_unlock(&fork_mutex);

	return ret;
}

static asmlinkage int px_sys_mmap(struct mmap_arg_struct *arg)
{
	int ret = 0;
	struct mmap_arg_struct tmp;
	struct file *file;
	unsigned long long ts;
	char * name = NULL;

	ts = get_timestamp();

	if (copy_from_user(&tmp, arg, sizeof(tmp)) != 0)
	{
		return -EFAULT;
	}

	//mutex_lock(&fork_mutex);
 	ret = px_original_sys_mmap(arg);

	if (gb_enable_os_hooks && (!IS_ERR((void*)ret)) && (tmp.prot & PROT_EXEC) && !(tmp.flags & MAP_ANONYMOUS))
	{
		//rcu_read_lock();
		if ((file = fcheck(tmp.fd)) != NULL)
		{
			char *pname;

			//memset(name, 0, PATH_MAX * sizeof(char));

			name = kzalloc(PATH_MAX, GFP_ATOMIC);

			if (name != NULL)
			{
				pname = px_d_path(file, name, PATH_MAX);
				if (pname)
				{
					unsigned long name_offset;

					name_offset = get_filename_offset(pname);

					module_load_notif(pname, name_offset, current->tgid,
							      ret, tmp.len,
							      tmp.flags, ts);
				}

				kfree(name);
			}
		}

		//rcu_read_unlock();
	}
	
	//mutex_unlock(&fork_mutex);

	return ret;
}

#if 0
static asmlinkage int px_sys_munmap(unsigned int addr, unsigned int len)
{
	int ret = 0;
	int saved_r5;
	unsigned long long ts;

	INIT_STACK_FRAME;

	// here we must save r5 since it will be used by the OS sys_mmap2 code
	__asm__("str r5, %0\n\t":"=m"(saved_r5):);

	APPEND_STACK_FRAME;

	ts = get_timestamp();

	//mutex_lock(&fork_mutex);
	// restore r5
	__asm__("ldr r5, %0\n\t"::"m"(saved_r5):"r5");

	ret = px_original_sys_munmap(addr, len);

	CUTTAIL_STACK_FRAME;
	
	//mutex_unlock(&fork_mutex);

	return ret;
}
#endif

static asmlinkage int px_sys_exit(int error_code)
{
	long ret = 0;
	unsigned long long ts;

	/**
	 * We must save the original sys_exit function here. If the current pid
	 * is the application the user launched and user selected "stop sampling
	 * on application unload", then the function "enum_module_for_exit_check"
	 * will terminate the sampling session, which in turn will remove all the
	 * syscall hooks. On 2.4 kernel it is not going to happen, because the
	 * utility has no chance to run until this function returns; but on 2.6
	 * kernel with preempt turned on, the utility maybe scheduled run before
	 * the following call to px_original_sys_exit, so at that time the module
	 * tracker is already unloaded and the px_original_sys_exit becomes NULL
	 * pointer. The following px_sys_exit_group has the same situation.
	 */
	sys_exit_t saved_sys_exit = px_original_sys_exit;

	ts = get_timestamp();
	
	//mutex_lock(&fork_mutex);

#ifndef CONFIG_PROFILING
	notify_thread_exit(current, task_pt_regs(current)->ARM_pc, ts, error_code, true);
#endif

	if (gb_enable_os_hooks)
	{
		check_launched_app_exit(current->pid);
	}

	//mutex_unlock(&fork_mutex);
	
	ret = saved_sys_exit(error_code);

	return ret;
}

static asmlinkage int px_sys_exit_group(int error_code)
{
	long ret = 0;
	unsigned long long ts;

	sys_exit_group_t saved_sys_exit_group = px_original_sys_exit_group;

	ts = get_timestamp();

#ifndef CONFIG_PROFILING
	//mutex_lock(&fork_mutex);
	notify_process_exit(current, task_pt_regs(current)->ARM_pc, ts, error_code);
#endif

	if (gb_enable_os_hooks)
	{
		check_launched_app_exit(current->tgid);
	}

	//mutex_unlock(&fork_mutex);

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
#ifndef CONFIG_PROFILING
	bool found = false;
#endif
	sys_kill_t saved_sys_kill = px_original_sys_kill;

	unsigned long long ts;

	ts = get_timestamp();

	//mutex_lock(&fork_mutex);
	
#ifndef CONFIG_PROFILING
	/* prevent the threads to be killed before notify_process_exit() is called */
	preempt_disable();
#endif

	ret = saved_sys_kill(pid, sig);
	
	if (!ret && gb_enable_os_hooks && is_kill_sig(sig))
	{
		struct task_struct * task;
		
		task = px_find_task_by_pid(pid);
		
		if (task != NULL)
		{
#ifndef CONFIG_PROFILING		
			notify_process_exit_with_preempt_disabled(task, task_pt_regs(task)->ARM_pc, ts, 0);
			found = true;
#endif
		}
		
		check_launched_app_exit(pid);
	}
	
#ifndef CONFIG_PROFILING
	if (!found)
		preempt_enable();
#endif
	//mutex_unlock(&fork_mutex);

	return ret;
}

static asmlinkage long px_sys_tkill(int pid, int sig)
{
	long ret = 0;
#ifndef CONFIG_PROFILING
	bool found = false;
#endif
	sys_tkill_t saved_sys_tkill = px_original_sys_tkill;
	unsigned long long ts;

	ts = get_timestamp();

#ifndef CONFIG_PROFILING
	/* prevent the threads to be killed before notify_process_exit() is called */
	preempt_disable();
#endif

	//mutex_lock(&fork_mutex);
	ret = saved_sys_tkill(pid, sig);

	if (!ret && gb_enable_os_hooks && is_kill_sig(sig))
	{
		struct task_struct * task;
		
		task = px_find_task_by_pid(pid);
		
		if (task != NULL)
		{
#ifndef CONFIG_PROFILING
			notify_process_exit_with_preempt_disabled(task, task_pt_regs(task)->ARM_pc, ts, 0);
			found = true;
#endif
		}
		
		check_launched_app_exit(pid);
	}

#ifndef CONFIG_PROFILING
	if (!found)
		preempt_enable();
#endif
	//mutex_unlock(&fork_mutex);

	return ret;
}

static asmlinkage long px_sys_tgkill(int pid, int tid, int sig)
{
	long ret = 0;
#ifndef CONFIG_PROFILING
	bool found = false;
#endif
	sys_tgkill_t saved_sys_tgkill = px_original_sys_tgkill;
	unsigned long long ts;

	ts = get_timestamp();

#ifndef CONFIG_PROFILING
	/* prevent the threads to be killed before notify_process_exit() is called */
	preempt_disable();
#endif

	//mutex_lock(&fork_mutex);
	ret = saved_sys_tgkill(pid, tid, sig);

	if (!ret && gb_enable_os_hooks && is_kill_sig(sig))
	{
		struct task_struct * task;
		
		task = px_find_task_by_pid(pid);
		
		if (task != NULL)
		{
#ifndef CONFIG_PROFILING
			notify_process_exit_with_preempt_disabled(task, task_pt_regs(task)->ARM_pc, ts, 0);
			found = true;
#endif
		}
		
		check_launched_app_exit(pid);
	}

#ifndef CONFIG_PROFILING
	if (!found)
		preempt_enable();
#endif
	//mutex_unlock(&fork_mutex);
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
	//mutex_lock(&fork_mutex);

	ret = px_original_sys_prctl(option, arg2, arg3, arg4, arg5);
	
	CUTTAIL_STACK_FRAME;

	if ((ret == 0) && (option == PR_SET_NAME) && gb_enable_os_hooks)
	{

		new_name = kzalloc(PATH_MAX, GFP_ATOMIC);
		old_name = kzalloc(PATH_MAX, GFP_ATOMIC);
		
		if ((new_name != NULL) && (old_name != NULL))
		{
			if (is_proc_name_modified(current, new_name, old_name))
			{
				notify_proc_name_changed(current, task_pt_regs(current)->ARM_pc, ts);
				//enum_modules_for_process(current->tgid, ts, new_name);
				enum_modules_for_task(current, ts, new_name);
			}
		}
		
		if (new_name != NULL)
			kfree(new_name);

		if (old_name != NULL)
			kfree(old_name);
	}
	
	//mutex_unlock(&fork_mutex);

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
		enum_modules_for_task(next, ts, NULL);
	}
	else
	{
		if (!kth->is_name_modified && (strcmp(next->comm, KTHREADD_NAME) != 0))
		{
			/* the kernel thread's name is modified in kthread_create_on_node() in kernel/kthread.c */
			kth->is_name_modified = true;
			enum_modules_for_task(next, ts, NULL);
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

static int task_exit_notify(struct notifier_block *self, unsigned long val, void *data)
{
	unsigned long long ts;
	struct task_struct * task = (struct task_struct *)data;
	
	if (gb_enable_os_hooks)
	{
		check_launched_app_exit(task->pid);
	}

	ts = get_timestamp();

#if 0
	/* TO BE VERIFIED
	 * if the task exit is already recorded when sys_exit or sys_exit_group is called
	 * do not to record the task exit 
	 */
	if (user_mode(task_pt_regs(task)))
	{
		return 0;
	}
#endif

	if (thread_group_leader(task))
	{
		notify_process_exit(task, task_pt_regs(task)->ARM_pc, ts, 0);
	}
	else
	{
		notify_thread_exit(task, task_pt_regs(task)->ARM_pc, ts, 0, true);
	}

	return 0;
}

static struct notifier_block task_exit_nb = {
	.notifier_call  = task_exit_notify,
};

static struct notifier_block nb_init =
{
	.notifier_call = module_init_notifier,
};

static int install_os_hooks(void)
{
	int ret;

//	spin_lock_init(&buf_lock);

	/* register module state change notifier */
	nb_init.notifier_call = module_init_notifier;

	ret = register_module_notifier(&nb_init);

	if (ret != 0)
	{
		printk(KERN_ERR "[CPA] register_module_notifier() fails\n");
		return -EFAULT;
	}

	ret = thread_register_notifier(&thread_notifier_block);
	if (ret != 0)
	{
		printk(KERN_ERR "[CPA] thread_register_notifier() fails\n");
		return -EFAULT;
	}

#ifdef CONFIG_PROFILING
	ret = profile_event_register(PROFILE_TASK_EXIT, &task_exit_nb);
	if (ret != 0)
	{
		if (ret == -ENOSYS)
		{
			printk(KERN_ERR "[CPA] CONFIG_PROFILING is not configured in the kernel, profiling result may be incorrect\n");
		}
		else
		{
			printk(KERN_ERR "[CPA] profile_event_register() fails\n");
			return -EFAULT;
		}
	}
#endif

	mutex_lock(&fork_mutex);

	/* hook necessary system call table */
	px_original_sys_fork       = (sys_fork_t) xchg(&system_call_table_tp[__NR_fork - __NR_SYSCALL_BASE], px_sys_fork);
	px_original_sys_vfork      = (sys_vfork_t) xchg(&system_call_table_tp[__NR_vfork - __NR_SYSCALL_BASE], px_sys_vfork);
	px_original_sys_clone      = (sys_clone_t) xchg(&system_call_table_tp[__NR_clone - __NR_SYSCALL_BASE], px_sys_clone);
	px_original_sys_execve     = (sys_execve_t) xchg(&system_call_table_tp[__NR_execve - __NR_SYSCALL_BASE], px_sys_execve);
	px_original_sys_mmap       = (sys_mmap_t) xchg(&system_call_table_tp[__NR_mmap - __NR_SYSCALL_BASE], px_sys_mmap);
	px_original_sys_mmap2      = (sys_mmap2_t) xchg(&system_call_table_tp[__NR_mmap2 - __NR_SYSCALL_BASE], px_sys_mmap2);
//	px_original_sys_munmap     = (sys_munmap_t) xchg(&system_call_table_tp[__NR_munmap - __NR_SYSCALL_BASE], px_sys_munmap);
	px_original_sys_exit       = (sys_exit_t) xchg(&system_call_table_tp[__NR_exit - __NR_SYSCALL_BASE], px_sys_exit);
	px_original_sys_exit_group = (sys_exit_group_t) xchg(&system_call_table_tp[__NR_exit_group - __NR_SYSCALL_BASE], px_sys_exit_group);
	px_original_sys_kill       = (sys_kill_t) xchg(&system_call_table_tp[__NR_kill - __NR_SYSCALL_BASE], px_sys_kill);
	px_original_sys_tkill      = (sys_tkill_t) xchg(&system_call_table_tp[__NR_tkill - __NR_SYSCALL_BASE], px_sys_tkill);
	px_original_sys_tgkill     = (sys_tgkill_t) xchg(&system_call_table_tp[__NR_tgkill - __NR_SYSCALL_BASE], px_sys_tgkill);
	px_original_sys_prctl      = (sys_prctl_t) xchg(&system_call_table_tp[__NR_prctl - __NR_SYSCALL_BASE], px_sys_prctl);

	gb_enable_os_hooks = true;

	mutex_unlock(&fork_mutex);

	return 0;
}

static int remove_os_hooks(void)
{
#if 0
	void *orgFn;
#endif

	/* only remove the os hooks when install_os_hooks() has been called */
	if (!gb_enable_os_hooks)
		return 0;

	mutex_lock(&fork_mutex);

	gb_enable_os_hooks = false;


#if 1
	system_call_table_tp[__NR_fork - __NR_SYSCALL_BASE] = px_original_sys_fork;

	system_call_table_tp[__NR_vfork - __NR_SYSCALL_BASE] = px_original_sys_vfork;

	system_call_table_tp[__NR_clone - __NR_SYSCALL_BASE] = px_original_sys_clone;

	system_call_table_tp[__NR_execve - __NR_SYSCALL_BASE] = px_original_sys_execve;

	system_call_table_tp[__NR_mmap- __NR_SYSCALL_BASE] = px_original_sys_mmap;

	system_call_table_tp[__NR_mmap2 - __NR_SYSCALL_BASE] = px_original_sys_mmap2;

	//system_call_table_tp[__NR_munmap - __NR_SYSCALL_BASE] = px_original_sys_munmap;

	system_call_table_tp[__NR_exit - __NR_SYSCALL_BASE] = px_original_sys_exit;

	system_call_table_tp[__NR_exit_group - __NR_SYSCALL_BASE] = px_original_sys_exit_group;

	system_call_table_tp[__NR_kill - __NR_SYSCALL_BASE] = px_original_sys_kill;

	system_call_table_tp[__NR_tkill - __NR_SYSCALL_BASE] = px_original_sys_tkill;

	system_call_table_tp[__NR_tgkill - __NR_SYSCALL_BASE] = px_original_sys_tgkill;

	system_call_table_tp[__NR_prctl - __NR_SYSCALL_BASE] = px_original_sys_prctl;

#else	
	if ((orgFn = xchg(&px_original_sys_fork, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_fork - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_vfork, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_vfork - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_clone, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_clone - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_execve, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_execve - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_mmap, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_mmap - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_mmap2, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_mmap2 - __NR_SYSCALL_BASE], orgFn);

//	if ((orgFn = xchg(&px_original_sys_munmap, 0)))
//		orgFn = xchg(&system_call_table_tp[__NR_munmap - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_exit, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_exit - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_exit_group, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_exit_group - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_kill, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_kill - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_tkill, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_tkill - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_tgkill, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_tgkill - __NR_SYSCALL_BASE], orgFn);

	if ((orgFn = xchg(&px_original_sys_prctl, 0)))
		orgFn = xchg(&system_call_table_tp[__NR_prctl - __NR_SYSCALL_BASE], orgFn);
#endif

	// unregister task exit notifier
	profile_event_unregister(PROFILE_TASK_EXIT, &task_exit_nb);

	thread_unregister_notifier(&thread_notifier_block);

	// unregister module state change notifier
	unregister_module_notifier(&nb_init);
	
	clear_kthread_list();
	
	mutex_unlock(&fork_mutex);

	return 0;
}

int start_module_tracking_tp(void)
{
	int ret;

	/*
	 * now we seperate start_module_tracking and start_sampling as two steps
	 * we need to set sample count to zero here
	 * otherwise it will record the sample count in the last running
	 */
	//g_dsa->sample_count = 0;

	g_static_enum = true;

	g_mpdc_tgid = current->tgid;

	static_enum_kernel_modules();

	static_enum_exiting_process_modules();

	g_static_enum = false;

	ret = install_os_hooks();

	return ret;
}

int stop_module_tracking_tp(void)
{
	int ret;

	ret = remove_os_hooks();
	
	clear_loaded_process_list();
	
	return ret;
}

