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
#include <linux/sched.h>
#include <linux/pid_namespace.h>
#include <linux/preempt.h>
#include <linux/version.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>

#include "common.h"

kallsyms_lookup_name_func_t kallsyms_lookup_name_func;

int get_thread_group_id(struct task_struct *task)
{
	int pid;
	void *proc_vm, *p_proc_vm;
	struct task_struct *orig_task;

	//read_lock(&tasklist_lock);
	rcu_read_lock();

	if (task->pid != task->tgid)
	{
		pid = task->tgid;
	}
	else
	{
		proc_vm = task->mm;
		p_proc_vm = task->real_parent->mm;

		while (proc_vm == p_proc_vm)
		{
			orig_task = task;
			task = task->real_parent;

			if (orig_task == task)
			break;

			proc_vm = task->mm;
			p_proc_vm = task->real_parent->mm;
		}
		pid = task->pid;
	}

	//read_unlock(&tasklist_lock);
	rcu_read_unlock();

	return pid;
}

// Get the CPU ID
unsigned long get_arm_cpu_id(void)
{
	unsigned long v1 = 0;
	__asm__ __volatile__ ("mrc  p15, 0, %0, c0, c0, 0\n\t" : "=r"(v1));
	return v1;
}

bool is_valid_module(struct vm_area_struct *mmap)
{
	if (mmap == NULL)
		return false;

	if (mmap->vm_file == NULL)
		return false;

	if (mmap->vm_flags & VM_EXECUTABLE)                               // .exe
		return true;

	//if ((mmap->vm_flags & VM_EXEC) && !(mmap->vm_flags & VM_WRITE))   // .so
	if (mmap->vm_flags & VM_EXEC)   // .so
		return true;

	return false;
}

bool is_exe_module(unsigned long address)
{
	if ((address >= LINUX_APP_BASE_LOW) && (address < LINUX_APP_BASE_HIGH))
		return true;
	else
		return false;
}

#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30))
static struct task_struct * px_find_task_by_pid_ns(pid_t nr, struct pid_namespace *ns)
{
	return pid_task(find_pid_ns(nr, ns), PIDTYPE_PID);
}

struct task_struct * px_find_task_by_pid(pid_t pid)
{
	struct task_struct * result;

	rcu_read_lock();
	result = px_find_task_by_pid_ns(pid, current->nsproxy->pid_ns);
	rcu_read_unlock();

	return result;
}
#elif defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
struct task_struct * px_find_task_by_pid(pid_t pid)
{
	return find_task_by_vpid(pid);
}
#else
struct task_struct * px_find_task_by_pid(pid_t pid)
{
	return find_task_by_pid(pid);
}
#endif


#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25))
char * px_d_path(struct file *file, char *buf, int buflen)
{
	return d_path(&file->f_path, buf, buflen);
}
#else
char * px_d_path(struct file *file, char *buf, int buflen)
{
	return d_path(file->f_dentry, file->f_vfsmnt, buf, buflen);
}
#endif

unsigned long long get_timestamp(void)
{
	unsigned long long ts;
	struct timeval tv;

	do_gettimeofday(&tv);

	ts = (u64)tv.tv_sec * USEC_PER_SEC + tv.tv_usec;

	return ts;
}

bool is_kernel_task(struct task_struct *task)
{
#if defined(LINUX_VERSION_CODE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28))
	if (task->flags & PF_KTHREAD)
#else
	if (task->flags & PF_BORROWED_MM)
#endif
		return true;

	else
		return false;
}

#if 0
char * get_cmdline_from_task(struct task_struct *ptask, bool *is_full_path)
{
	bool find = false;
	struct mm_struct *mm;
	char *pname = NULL;
	struct vm_area_struct *mmap;

	char *name;

	name = kmalloc(PATH_MAX, GFP_KERNEL);

	if (name == NULL)
		return NULL;

	mm = get_task_mm(ptask);
	if (mm != NULL)
	{
		down_read(&mm->mmap_sem);

		for (mmap = mm->mmap; mmap; mmap = mmap->vm_next)
		{
			if (is_valid_module(mmap))
			{
				memset(name, 0, PATH_MAX * sizeof(char));
				pname = px_d_path(mmap->vm_file, name, PATH_MAX);

				if (pname != NULL)
				{
					find = true;
					break;
				}

		        }
		}

		up_read(&mm->mmap_sem);
		mmput(mm);
	}

	kfree(name);

	if (!find)
	{
		/* we can't get the full path name of the process */
		*is_full_path = false;

		return ptask->comm;
	}
	else
	{
		/* we can get the full path name of the process */
		*is_full_path = true;
		return pname;
	}
}

char * get_cmdline_from_pid(pid_t pid, bool *is_full_path)
{
	struct task_struct *ptask;

	ptask = px_find_task_by_pid(pid);

	if (ptask != NULL)
	{
		return get_cmdline_from_task(ptask, is_full_path);
	}
	else
	{
		*is_full_path = false;
		return NULL;
	}
}
#endif

unsigned long get_filename_offset(const char * full_path)
{
	unsigned long i;
	unsigned long offset;

	if (full_path == NULL)
		return 0;

	if (full_path[0] != '/')
		return 0;

	offset = 0;

	for (i=0; full_path[i] != '\0'; i++)
	{
		if (full_path[i] == '/')
			offset = i+1;
	}

	return offset;
}

int px_get_bit_count(unsigned int n)
{
	int i;
	unsigned sum = 0;

	for (i=0; i < sizeof(n) * 8; i++)
	{
		sum += (n >> i) & 0x1;
	}

	return sum;
}

int px_get_bit(unsigned int data, int n)
{
	return (data >> n) & 0x1;
}

unsigned int px_get_bits(unsigned int data, int low, int high)
{
	unsigned int n;
	unsigned int mask;

	n = data >> low;
	mask = (1 << (high - low + 1)) - 1;

	return n & mask;
}

unsigned int px_change_bits(unsigned int data, int low, int high, int value)
{
	unsigned int n;
	unsigned int mask;

	mask = (1 << (high - low + 1)) - 1;
	n = mask << low;

	return (data & ~n) + (value << low);
}

unsigned int px_change_bit(unsigned int data, int bit, int value)
{
	unsigned int new_data = data;

	if (value == 1)
		new_data = data | (value << bit);

	if (value == 0)
		new_data = data & ~(value << bit);

	return new_data;
}

unsigned int px_ror(u32 value, u32 n, u32 shift)
{
	u32 m = shift % n;
	
	return (value >> m) | (value << (n - m));
}

static u32 thumb_expand_imm_c(u32 imm12, u32 carry_in, u32 * carry_out)
{
    u32 imm32 = 0;

    if (px_get_bits(imm12, 10, 11) == 0)
    {
        switch (px_get_bits(imm12, 8, 9)) {
        case 0:
            imm32 = imm12;
            break;

        case 1:
            imm32 = (imm12 << 16) | imm12;
            break;

        case 2:
            imm32 = (imm12 << 24) | (imm12 << 8);
            break;

        case 3:
            imm32 = (imm12  << 24) | (imm12 << 16) | (imm12 << 8) | imm12; 
            break;
        }

        *carry_out = carry_in;
    }
    else
    {
        u32 unrotated_value;

	unrotated_value	= 0x80 | px_get_bits(imm12, 0, 6);
	
        imm32 = px_ror(unrotated_value, 32, px_get_bits(imm12, 7, 11));

        *carry_out = px_get_bit(imm32, 31);
    }
    return imm32;
}

unsigned int thumb_expand_imm(u32 inst)
{
    unsigned int carry_in = 0;
    unsigned int carry_out;
   
    return thumb_expand_imm_c(inst, carry_in, &carry_out);
}

enum CACHE_TYPE
{
	I_CACHE,
	D_CACHE,
	UNIFIED_CACHE
};

#ifdef PX_CPU_PJ4
static int display_cache_info_armv7(enum CACHE_TYPE cache_type, unsigned int cache_level)
{
	unsigned int data;
	int cache_size;
	int assoc;
	char str[25];
	unsigned int id_reg, num_sets, line_size;

	switch (cache_type)
	{
	case I_CACHE:
		data = 1;
		break;
	case D_CACHE:
		data = 0;
		break;
	case UNIFIED_CACHE:
		data = 0;
		break;
	default:
		return -EINVAL;
	}

	data |= (cache_level - 1) << 1;

	/* write CSSELR */
	asm("mcr p15, 2, %0, c0, c0, 0" :: "r" (data));
	isb();

	/* read CCSIDR */
	asm("mrc p15, 1, %0, c0, c0, 0 @ " : "=r" (id_reg));

	num_sets   = px_get_bits(id_reg, 13, 27) + 1;
	line_size  = 2 << (px_get_bits(id_reg, 0, 2) + 2);
	assoc      = px_get_bits(id_reg, 3, 12) + 1;
	cache_size = line_size * num_sets * assoc * 4;

	switch (cache_type)
	{
	case I_CACHE:
		strcpy(str, "I-Cache");
		break;

	case D_CACHE:
		strcpy(str, "D-Cache");
		break;

	case UNIFIED_CACHE:
		strcpy(str, "UNIFIED-Cache");
		break;

	default:
		strcpy(str, "Unknown Cache");
		break;
	}

	//printk("[CPA] L%d %s: %d KB\n", cache_level, str, cache_size/1024);
	return 	0;
}

static int display_all_cache_info_armv7(void)
{
	int level;
	int cache_level_reg;

	/* read CLIDR */
	asm("mrc p15, 1, %0, c0, c0, 1" : "=r" (cache_level_reg));

	for (level=1; level<=7; level++)
	{
		int begin_bit;
		int end_bit;

		begin_bit = 3 * (level-1);
		end_bit   = begin_bit + 2;

		switch (px_get_bits(cache_level_reg, begin_bit, end_bit))
		{
		case 0:
			return 0;
		case 1:
			display_cache_info_armv7(I_CACHE, level);
			break;
		case 2:
			display_cache_info_armv7(D_CACHE, level);
			break;
		case 3:
			display_cache_info_armv7(D_CACHE, level);
			display_cache_info_armv7(I_CACHE, level);
			break;
		case 4:
			display_cache_info_armv7(UNIFIED_CACHE, level);
			break;
		default:
			break;
		}
	}

	return 0;
}
#endif

#ifdef PX_CPU_PJ1
static int display_l2_cache_info_pj1(void)
{
	int l2_dcache_size = 0;
	int l2_icache_size = 0;
	int l2_cache_type  = 0;

	asm("mrc p15, 1, %0, c0, c0, 1" : "=r" (l2_cache_type));

	switch (px_get_bits(l2_cache_type, 20, 23))
	{
	case 2:
		l2_dcache_size = 32;
		break;
	case 3:
		l2_dcache_size = 64;
		break;
	}

	switch (px_get_bits(l2_cache_type, 8, 11))
	{
	case 2:
		l2_icache_size = 32;
		break;
	case 3:
		l2_icache_size = 64;
		break;
	}

	if (px_get_bit(l2_cache_type, 24) == 0)
	{
		printk("[CPA] L2 Unified-Cache size: %d KB\n", l2_icache_size);
	}
	else
	{
		printk("[CPA] L2 I-Cache size: %d KB\n", l2_icache_size);
		printk("[CPA] L2 D-Cache size: %d KB\n", l2_dcache_size);
	}

	return 0;
}

static int display_l1_cache_info_pj1(void)
{
	int l1_dcache_size = 0;
	int l1_icache_size = 0;
	int l1_cache_type;

	asm("mrc p15, 0, %0, c0, c0, 1" : "=r" (l1_cache_type));

	switch (px_get_bits(l1_cache_type, 18, 21))
	{
	case 3:	l1_dcache_size = 4; break;
	case 4:	l1_dcache_size = 8; break;
	case 5:	l1_dcache_size = 16; break;
	case 6:	l1_dcache_size = 32; break;
	case 7:	l1_dcache_size = 64; break;
	case 8:	l1_dcache_size = 128; break;
	}

	switch (px_get_bits(l1_cache_type, 6, 9))
	{
	case 3:	l1_icache_size = 4; break;
	case 4:	l1_icache_size = 8; break;
	case 5:	l1_icache_size = 16; break;
	case 6:	l1_icache_size = 32; break;
	case 7:	l1_icache_size = 64; break;
	case 8:	l1_icache_size = 128; break;
	}

	if (px_get_bit(l1_cache_type, 24) == 0)
	{
		printk("[CPA] L1 Unified-Cache size: %d KB\n", l1_icache_size);
	}
	else
	{
		printk("[CPA] L1 I-Cache size: %d KB\n", l1_icache_size);
		printk("[CPA] L1 D-Cache size: %d KB\n", l1_dcache_size);
	}

	return 0;
}


static int display_all_cache_info_pj1(void)
{
	display_l1_cache_info_pj1();
	display_l2_cache_info_pj1();

	return 0;
}
#endif

int display_all_cache_info(void)
{
#ifdef PX_CPU_PJ4
	display_all_cache_info_armv7();
#endif

#ifdef PX_CPU_PJ1
	display_all_cache_info_pj1();
#endif

	return 0;
}

typedef int (*access_process_vm_func_t)(struct task_struct *tsk, unsigned long addr, void *buf, int len, int write);

static access_process_vm_func_t access_process_vm_func = NULL;

void set_access_process_vm_address(unsigned int address)
{
	access_process_vm_func = (access_process_vm_func_t)address;
}

int get_proc_argv0(struct task_struct * task, char * buffer)
{
	int res = 0;
	unsigned int len;
	struct mm_struct *mm;
	
	memset(buffer, PATH_MAX, 0);

	if (access_process_vm_func == NULL)
		return res;

	mm  = get_task_mm(task);

	if (!mm)
		goto out;

	if (!mm->arg_end)
		goto out_mm;

 	len = mm->arg_end - mm->arg_start;

	if (len > PAGE_SIZE)
		len = PAGE_SIZE;

	/* read the process argv array */
	res = access_process_vm_func(task, mm->arg_start, buffer, len, 0);

	if (res >= 0)
	{
		len = strnlen(buffer, res);

		if (len < res)
		{
		    res = len;
		}
		else
		{
			len = mm->env_end - mm->env_start;
			if (len > PAGE_SIZE - res)
				len = PAGE_SIZE - res;
			res += access_process_vm_func(task, mm->env_start, buffer+res, len, 0);
			res = strnlen(buffer, res);
		}
	}

out_mm:
	mmput(mm);
out:
	return res;
}

/*
 * get the process name
 * if the argv[0] is changed, use the argv[0]
 */

/* see proc_pid_cmdline() in fs/proc/base.c */
int get_proc_name(struct task_struct * task, char * buffer)
{
	int res = 0;
	unsigned int len;

	struct mm_struct *mm;
	
	if (access_process_vm_func == NULL)
		return res;

	mm  = get_task_mm(task);

	if (!mm)
		goto out;

	if (!mm->arg_end)
		goto out_mm;

 	len = mm->arg_end - mm->arg_start;

	if (len > PAGE_SIZE)
		len = PAGE_SIZE;

	/* get the argv[0] of the process */
	res = access_process_vm_func(task, mm->arg_start, buffer, len, 0);

	if (res > 0 && buffer[res-1] != '\0' && len < PAGE_SIZE)
	{
		/* If the NULL at the end of args has been overwritten, then assume that the argv[0] is modified */
		len = strnlen(buffer, res);
		if (len < res)
		{
		    res = len;
		}
		else
		{
			len = mm->env_end - mm->env_start;
			if (len > PAGE_SIZE - res)
				len = PAGE_SIZE - res;
			res += access_process_vm_func(task, mm->env_start, buffer+res, len, 0);
			res = strnlen(buffer, res);
		}
	}
	else if (is_valid_module(mm->mmap))
	{
		if (mm->mmap != 0)
		{
			char * name;
			char * filename = NULL;

			name = kmalloc(PATH_MAX, GFP_KERNEL);

			/* get the file name of the process */
			filename = px_d_path(mm->mmap->vm_file, name, PATH_MAX);

			if (filename != NULL)
			{
#ifdef CONFIG_ANDROID
				/* 
				 * On Android, if the file name is not /system/bin/app_process,
				 * we need to get use the file name as the process name instead of argv[0]
				 */
				if (strcmp(filename, "/system/bin/app_process") != 0)
				{
					strcpy(buffer, filename);
				}
#else
				strcpy(buffer, filename);
#endif
			}

			kfree(name);
			
			res = strlen(buffer);
		}
	}

out_mm:
	mmput(mm);
out:
	return res;
}

#if 0
static int get_cpu_endian(int cpu)
{
	unsigned int i;
	char *p;

	i = 1;

	p = (char *)&i;

	if (*p != 0)
	{
		return PX_LITTLE_ENDIAN;
	}
	else
	{
		return PX_BIG_ENDIAN;
	}
}

static int get_cpu_freq(int cpu)
{
	return cpufreq_quick_get(cpu);
}
#endif

PXD32_DWord convert_to_PXD32_DWord(unsigned long long n)
{
	PXD32_DWord li;

	li.low  = n & 0xffffffff;
	li.high = n >> 32;

	return li;
}

inline void PX_BUG(void)
{
	printk("[CPA] BUG on %s %d\n", __FILE__, __LINE__);
}


#define PID_HASH_SIZE 100

static int pid_hash_func(pid_t pid)
{
	return pid % PID_HASH_SIZE;
}

static struct hlist_head loaded_process_hash[PID_HASH_SIZE];


struct loaded_process
{
	struct hlist_node link;

	struct task_struct * task;           /* process task structure */
	char               * name;           /* process name */
};

static struct loaded_process * find_loaded_process(pid_t tgid)
{
	struct hlist_head *head;
	struct hlist_node *node;
	struct loaded_process *proc;

	unsigned int pid_key;

	pid_key = pid_hash_func(tgid);
	head = &loaded_process_hash[pid_key];

	if (hlist_empty(head))
		return NULL;

	hlist_for_each_entry(proc, node, head, link)
	{
		if (proc->task->tgid == tgid)
		{
			return proc;
		}
	}

	return NULL;
}


static void remove_loaded_process(struct loaded_process * proc)
{
	if (proc == NULL)
		return;

	hlist_del(&proc->link);

	if (proc->name != NULL)
	{
		kfree(proc->name);
	}
	
	kfree(proc);
}

void clear_loaded_process_list(void)
{
	int i;
	struct hlist_head * head;
	struct hlist_node * node, * temp;
	struct loaded_process * proc;

	for (i=0; i<PID_HASH_SIZE; i++)
	{
		head = &loaded_process_hash[i];

		if (hlist_empty(head))
			continue;

		hlist_for_each_entry_safe(proc, node, temp, head, link)
		{
			remove_loaded_process(proc);
		}
	}
}

/* 
 * update the loaded process structure
 * parameter:
 *         proc      [in]  the loaded process structure to be updated
 *         task      [in]  the new task structure. If it is NULL, don't update the task structure
 *         proc_name [in]  the new process name. If it is NULL, don't update the process name
 */
static int update_loaded_process(struct loaded_process * proc, struct task_struct * task, const char * proc_name)
{
	if (proc == NULL)
	{
		return -EINVAL;
	}
	
	/* if the task structure needs to be updated */
	if (task != NULL)
		proc->task = task;

	/* if the process name needs to be updated */
	if (proc_name != NULL)
	{
		if (proc->name != NULL)
		{
			kfree(proc->name);
		}

		proc->name = kzalloc(strlen(proc_name) + 1, GFP_ATOMIC);
		
		if (proc->name == NULL)
		{
			return -ENOMEM;
		}
		
		strcpy(proc->name, proc_name);
	}
	
	return 0;	
}

static struct loaded_process * add_loaded_process(struct task_struct * task, const char * proc_name)
{
	pid_t tgid;
	unsigned int pid_key;
	struct loaded_process *proc;

	if ((task == NULL) || (proc_name == NULL))
		return NULL;

	tgid = task->tgid;
	pid_key = pid_hash_func(tgid);
/*
	if ((proc = find_loaded_process(tgid)) != NULL)
	{
		update_loaded_process(proc, task, proc_name);
		//remove_loaded_process_by_tgid(tgid);
	}
*/
	proc = kzalloc(sizeof(struct loaded_process), GFP_ATOMIC);

	if (proc == NULL)
	{
		return NULL;
	}

	proc->task = task->group_leader;
	
	proc->name = kzalloc(strlen(proc_name) + 1, GFP_ATOMIC);
	
	if (proc->name == NULL)
	{
		goto err;
	}
	
	strcpy(proc->name, proc_name);

	//INIT_LIST_HEAD(&proc->hook_list);

	hlist_add_head(&proc->link, &loaded_process_hash[pid_key]);
	
	return proc;
err:
	if (proc->name != NULL)
		kfree(proc->name);

	if (proc != NULL)
		kfree(proc);

	return NULL;
}

/*
 * Must be called when a new process is created
 */
void notify_new_loaded_process(struct task_struct * task, const char * proc_name)
{
	struct loaded_process * proc;
	
	if (task == NULL)
		return;

	proc = find_loaded_process(task->tgid);
	
	if (proc == NULL)
	{
		add_loaded_process(task, proc_name);
	}
	else
	{
		update_loaded_process(proc, task, proc_name);
	}
}

/*
 * check if the process name is changed
 * parameter:
 *             task          [in]              the task struct of the process
 *             new_name      [out]             the new process name
 *             old_name      [out]             the old process name
 * return:
 *         true, if the process name is changed.
 *         false, if the process name is not changed.
 */
bool is_proc_name_modified(struct task_struct * task, char * new_name, char * old_name)
{
	bool ret = false;
	struct loaded_process * proc = NULL;
	char  * proc_name;
	
	proc_name = kzalloc(PATH_MAX, GFP_ATOMIC);

	if (proc_name == NULL)
	{
		goto out;
	}

	proc = find_loaded_process(task->tgid);
	
	if (proc == NULL)
	{
		proc = add_loaded_process(task, task->comm);
		
		if (proc == NULL)
			goto out;
	}
	else
	{
		/* update the task structure */
		update_loaded_process(proc, task, NULL);
	}
	
	if (get_proc_argv0(task, proc_name) <= 0)
	{
		goto out;
	}
	
	if ((proc_name != NULL) && (proc->name != NULL) && (strcmp(proc_name, proc->name) != 0))
	{
		if (new_name != NULL)
			strcpy(new_name, proc_name);
		
		if (old_name != NULL)
			strcpy(old_name, proc->name);

		/* if the process name is changed, we need to update the process name */
		update_loaded_process(proc, NULL, proc_name);
		
		ret = true;
	}
	
out:
	if (proc_name != NULL)
		kfree(proc_name);

	return ret;
}

int split_string_to_long(const char * string, const char * delim, unsigned int * data)
{
	unsigned int count = 0;
	char * token;
	char * new_str, *p;

	//new_str = (char *)string;
	new_str = kzalloc(strlen(string) + 1, GFP_ATOMIC);
	
	if (new_str == NULL)
		return -1;

	p = new_str;
	strcpy(new_str, string);

	while((token = strsep(&new_str, delim)) != NULL)  
	{  
		if (data != NULL)
		{
			int number;

			if ((strlen(token) > 2) && ((strncmp(token, "0x", 2) == 0) || (strncmp(token, "0X", 2) == 0)))
			{
				number = simple_strtol(token, NULL, 16);
			}
			else
			{
				number = simple_strtol(token, NULL, 10);
			}
			
			data[count] = number;
		}
		
		count++;
	} 

	kfree(p);

	return count;
}

bool is_kernel_thread(struct task_struct * task)
{
	return task->mm == NULL;
}
