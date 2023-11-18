/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/types.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/mm.h>
#include <linux/kthread.h>
#include <linux/ptrace.h>
#include <asm/stacktrace.h>

#include "native_hang_monitor.h"

static int hang_detect_enabled; /*	disable in default	*/
int hang_detect_timeout = WAIT_BOOT_COMPLETE; /*	2 min timeout in default	*/
static atomic_t hang_detect_counter;
char hang_info[HANG_INFO_MAX];
static int hang_info_index;
struct task_struct *hd_thread;

struct core_task_info core_task[CORE_TASK_NUM_MAX] = {
	{1, "init"},
	{0, "surfaceflinger"},
	{0, "debuggerd"},
	{0, "debuggerd64"},
	{0, "system_server"},
	{0, "systemui"},
	{0, "mediaserver"},
	{0, "mmcqd/0"},
	{0, "mmcqd/1"},
	{0, ""},
};

void reset_core_task_info(void)
{
	int i;
	for (i = 0; i < CORE_TASK_NUM_MAX; i++) {
		if (!strlen(core_task[i].name))
			break;
		core_task[i].pid = 0;
	}
}
static void log_to_hang_info(const char *fmt, ...)
{
	unsigned long len = 0;
	static int times;
	va_list ap;

	if ((hang_info_index + MAX_STRING_SIZE) >= (unsigned long)HANG_INFO_MAX) {
		if (!times)
			pr_err("HangInfo Buffer overflow len(0x%x), times:%d, HANG_INFO_MAX:0x%lx !!!!!!! \n",
				hang_info_index, times, (long)HANG_INFO_MAX);
		times++;
		return;
	}
	va_start(ap, fmt);
	len = vsnprintf(&hang_info[hang_info_index], MAX_STRING_SIZE, fmt, ap);
	va_end(ap);
	hang_info_index += len;
}
static int save_kernel_trace(struct stackframe *frame, void *d)
{
	struct thread_backtrace_info *trace = d;
	unsigned int id = trace->nr_entries;
	int ret = 0;

	if (id >= NR_FRAME)
		return -1;

	trace->entries[id].pc = frame->pc;
	snprintf(trace->entries[id].pc_symbol, SYMBOL_SIZE_S, "%pS", (void *)frame->pc);
#ifndef __aarch64__
	trace->entries[id].lr = frame->lr;
	snprintf(trace->entries[id].lr_symbol, SYMBOL_SIZE_L, "%pS", (void *)frame->lr);
#endif

	++trace->nr_entries;
	return ret;
}

static void get_kernel_bt(struct task_struct *tsk, struct thread_backtrace_info *bt)
{
	struct stackframe frame;
	static struct backtrace_frame backtrace_buffer[NR_FRAME];

	bt->nr_entries = 0;
	bt->entries = backtrace_buffer;

	memset(&frame, 0, sizeof(struct stackframe));
	if (tsk != current) {
		frame.fp = thread_saved_fp(tsk);
#ifdef __aarch64__
		frame.pc = thread_saved_pc(tsk);
#else
		frame.lr = thread_saved_pc(tsk);
		frame.pc = 0xffffffff;
#endif
	} else {

		frame.fp = (unsigned long)__builtin_frame_address(0);
#ifdef __aarch64__
		frame.pc = (unsigned long)__builtin_return_address(0);
#else
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)get_kernel_bt;
#endif
	}
#ifdef __aarch64__
	walk_stackframe(tsk, &frame, save_kernel_trace, bt);
#else
	walk_stackframe(&frame, save_kernel_trace, bt);
#endif
}

void sched_show_task_local(struct task_struct *p)
{
	unsigned long free = 0;
	int ppid, i;
	unsigned state;
	char stat_nam[] = TASK_STATE_TO_CHAR_STR;
	struct thread_backtrace_info ke_frame;
	pid_t pid;

	state = p->state ? __ffs(p->state) + 1 : 0;
	pr_err("%-15.15s %c", p->comm, state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
#if BITS_PER_LONG == 32
	if (state == TASK_RUNNING)
		pr_err(" running  ");
	else
		pr_err(" %08lx ", thread_saved_pc(p));
#else
	if (state == TASK_RUNNING)
		pr_err("  running task    ");
	else
		pr_err(" %016lx ", thread_saved_pc(p));
#endif
#ifdef CONFIG_DEBUG_STACK_USAGE
	free = stack_not_used(p);
#endif
	rcu_read_lock();
	ppid = task_pid_nr(rcu_dereference(p->real_parent));
	rcu_read_unlock();
	pr_err("%5lu %5d %6d 0x%08lx\n", free,
	     task_pid_nr(p), ppid, (unsigned long)task_thread_info(p)->flags);

	log_to_hang_info("%-15.15s %c ", p->comm, state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	log_to_hang_info("%5lu %5d %6d 0x%08lx\n", free, task_pid_nr(p), ppid,
		     (unsigned long)task_thread_info(p)->flags);

	pid = task_pid_nr(p);
	ke_frame.pid = pid;
	ke_frame.nr_entries = 0;

	get_kernel_bt(p, &ke_frame);	/* Get kernel-space backtrace */
	log_to_hang_info("KBT,sysTid=%d,ke_frame.nr_entries:%d\n", pid, ke_frame.nr_entries);
	for (i = 0; i < ke_frame.nr_entries; i++) {
		pr_err("LHD11:frame %d: %s,pc(%lx)\n", i, ke_frame.entries[i].pc_symbol,
		     (long)(ke_frame.entries[i].pc));
		/*  format: [<0000000000000000>] __switch_to+0xa0/0xd8 */
		log_to_hang_info("[<%lx>] %s\n", (long)((ke_frame.entries[i]).pc), ke_frame.entries[i].pc_symbol);
	}
}

void show_state_filter_local(unsigned long state_filter)
{
	struct task_struct *g, *p;

#if BITS_PER_LONG == 32
	pr_err("  task                PC stack   pid father\n");
#else
	pr_err("  task                        PC stack   pid father\n");
#endif
	do_each_thread(g, p) {
		/* TODO: Here can discard some threads which  always stay in D state */
		if (p->state != state_filter)
			continue;
		sched_show_task_local(p);
	} while_each_thread(g, p);
}

static void save_maps(struct vm_area_struct *vma, const char *name)
{
	if (vma && name) {
		pr_err("%08lx-%08lx %c%c%c%c    %s\n", vma->vm_start, vma->vm_end,
		vma->vm_flags & VM_READ ? 'r' : '-',
		vma->vm_flags & VM_WRITE ? 'w' : '-',
		vma->vm_flags & VM_EXEC ? 'x' : '-',
		vma->vm_flags & VM_MAYSHARE ? 's' : 'p',
		name);

		if (vma->vm_flags & VM_EXEC) {	/* we only catch code section for reduce maps space */
			log_to_hang_info("%08lx-%08lx %c%c%c%c    %s\n", vma->vm_start,
				vma->vm_end, vma->vm_flags & VM_READ ? 'r' : '-',
				vma->vm_flags & VM_WRITE ? 'w' : '-',
				vma->vm_flags & VM_EXEC ? 'x' : '-',
				vma->vm_flags & VM_MAYSHARE ? 's' : 'p',
				 name);
		}
	}
}
static const char *get_maps_name(struct vm_area_struct *vma)
{
	struct mm_struct *mm;
	const char *name = arch_vma_name(vma);

	mm = vma->vm_mm;
	if (!name) {
		if (mm) {
			if (vma->vm_start <= mm->start_brk &&
			    vma->vm_end >= mm->brk) {
				name = "[heap]";
			} else if (vma->vm_start <= mm->start_stack &&
				   vma->vm_end >= mm->start_stack) {
				name = "[stack]";
			}
		} else {
			name = "[vdso]";
		}
	}
	return name;
}
static void save_native_maps(struct task_struct *current_task)
{
	int mapcount = 0;
	struct vm_area_struct *vma;

	if (current_task->mm != NULL) {
		mmgrab(current_task->mm);
		vma = current_task->mm->mmap;

		while (vma && current_task && (mapcount < current_task->mm->map_count)) {
			if (vma->vm_file) {
				save_maps(vma, (unsigned char *)(vma->vm_file->f_path.dentry->d_name.name));
			} else {
				save_maps(vma, get_maps_name(vma));
			}
			vma = vma->vm_next;
			mapcount++;
		}
		mmdrop(current_task->mm);
	}
}
static int save_native_thread_maps(pid_t pid)
{
	struct task_struct *current_task;
	struct pt_regs *user_ret;

	current_task = find_task_by_vpid(pid);
	if (current_task == NULL) {
		pr_err(" %s,%d: current_task == NULL", __func__, pid);
		return -1;
	}

	user_ret = task_pt_regs(current_task);
	if (!user_mode(user_ret)) {
		pr_err(" %s,%d:%s: in user_mode", __func__, pid, current_task->comm);
		return -1;
	}

	log_to_hang_info("Dump native maps files:\n");
	save_native_maps(current_task);

	return 0;
}

static int save_native_threadinfo_by_tid(pid_t tid)
{
	struct task_struct *current_task;
	struct pt_regs *user_ret;
	struct vm_area_struct *vma;
	unsigned long userstack_start = 0;
	unsigned long userstack_end = 0, length = 0;
	int ret = -1;

	current_task = find_task_by_vpid(tid);
	if (current_task == NULL)
		return -ESRCH;
	user_ret = task_pt_regs(current_task);

	if (!user_mode(user_ret)) {
		pr_err(" %s,%d:%s,fail in user_mode", __func__, tid, current_task->comm);
		return ret;
	}

	if (current_task->mm == NULL) {
		pr_err(" %s,%d:%s, current_task->mm == NULL", __func__, tid, current_task->comm);
		return ret;
	}
#ifndef __aarch64__		/* 32bit */
	pr_err(" pc/lr/sp 0x%08lx/0x%08lx/0x%08lx\n", user_ret->ARM_pc, user_ret->ARM_lr,
	     user_ret->ARM_sp);

	userstack_start = (unsigned long)user_ret->ARM_sp;
	vma = current_task->mm->mmap;

	while (vma != NULL) {
		if (vma->vm_start <= userstack_start && vma->vm_end >= userstack_start) {
			userstack_end = vma->vm_end;
			break;
		}
		vma = vma->vm_next;
		if (vma == current_task->mm->mmap)
			break;
	}

	if (userstack_end == 0) {
		pr_err(" %s,%d:%s,userstack_end == 0", __func__, tid, current_task->comm);
		return ret;
	}
	pr_err("Dump K32 stack range (0x%08lx:0x%08lx)\n", userstack_start, userstack_end);
	length = userstack_end - userstack_start;


	/* dump native stack to buffer */
	{
		unsigned long SPStart = 0, SPEnd = 0;
		int tempSpContent[4], copied;

		SPStart = userstack_start;
		SPEnd = SPStart + length;
		log_to_hang_info("UserSP_start:0x%016x: UserSP_Length:0x%08x ,UserSP_End:0x%016x\n",
				SPStart, length, SPEnd);
		while (SPStart < SPEnd) {
			copied = access_process_vm(current_task, SPStart, &tempSpContent, sizeof(tempSpContent), 0);
			if (copied != sizeof(tempSpContent)) {
				pr_err("access_process_vm  SPStart error,sizeof(tempSpContent)=%x\n",
				     (unsigned int)sizeof(tempSpContent));
			}
			log_to_hang_info("0x%08x:%08x %08x %08x %08x\n", SPStart, tempSpContent[0],
				     tempSpContent[1], tempSpContent[2], tempSpContent[3]);
			SPStart += 4 * 4;
		}
	}
	pr_err("u+k 32 copy_from_user ret(0x%08x),len:%lx\n", ret, length);
	pr_err("end dump native stack:\n");
#else				/* 64bit, First deal with K64+U64, the last time to deal with K64+U32 */

	/* K64_U32 for current task */
	if (compat_user_mode(user_ret)) {	/* K64_U32 for check reg */
		pr_err(" K64+ U32 pc/lr/sp 0x%16lx/0x%16lx/0x%16lx\n",
		     (long)(user_ret->user_regs.pc),
		     (long)(user_ret->user_regs.regs[14]), (long)(user_ret->user_regs.regs[13]));
		userstack_start = (unsigned long)user_ret->user_regs.regs[13];
		vma = current_task->mm->mmap;
		while (vma != NULL) {
			if (vma->vm_start <= userstack_start && vma->vm_end >= userstack_start) {
				userstack_end = vma->vm_end;
				break;
			}
			vma = vma->vm_next;
			if (vma == current_task->mm->mmap)
				break;
		}

		if (userstack_end == 0) {
			pr_err("Dump native stack failed:\n");
			return ret;
		}

		pr_err("Dump K64+ U32 stack range (0x%08lx:0x%08lx)\n", userstack_start,
		     userstack_end);
		length = userstack_end - userstack_start;

		/*  dump native stack to buffer */
		{
			unsigned long SPStart = 0, SPEnd = 0;
			int tempSpContent[4], copied;

			SPStart = userstack_start;
			SPEnd = SPStart + length;
			log_to_hang_info("UserSP_start:0x%016lx: UserSP_Length:0x%08lx ,UserSP_End:0x%016lx\n",
				SPStart, length, SPEnd);
			while (SPStart < SPEnd) {
				/*  memcpy(&tempSpContent[0],(void *)(userstack_start+i),4*4); */
				copied =
				    access_process_vm(current_task, SPStart, &tempSpContent,
						      sizeof(tempSpContent), 0);
				if (copied != sizeof(tempSpContent)) {
					pr_err("access_process_vm  SPStart error,sizeof(tempSpContent)=%x\n",
						(unsigned int)sizeof(tempSpContent));
					/* return -EIO; */
				}
				log_to_hang_info("0x%08lx:%08x %08x %08x %08x\n", SPStart,
					     tempSpContent[0], tempSpContent[1], tempSpContent[2],
					     tempSpContent[3]);
				SPStart += 4 * 4;
			}
		}
	} else {		/*K64+U64 */
		pr_err(" K64+ U64 pc/lr/sp 0x%16lx/0x%16lx/0x%16lx\n",
		     (long)(user_ret->user_regs.pc),
		     (long)(user_ret->user_regs.regs[30]), (long)(user_ret->user_regs.sp));
		userstack_start = (unsigned long)user_ret->user_regs.sp;
		vma = current_task->mm->mmap;

		while (vma != NULL) {
			if (vma->vm_start <= userstack_start && vma->vm_end >= userstack_start) {
				userstack_end = vma->vm_end;
				break;
			}
			vma = vma->vm_next;
			if (vma == current_task->mm->mmap)
				break;
		}
		if (userstack_end == 0) {
			pr_err("Dump native stack failed:\n");
			return ret;
		}

		{
			unsigned long tmpfp, tmp, tmpLR;
			int copied, frames;
			unsigned long native_bt[16];

			native_bt[0] = user_ret->user_regs.pc;
			native_bt[1] = user_ret->user_regs.regs[30];
			tmpfp = user_ret->user_regs.regs[29];
			frames = 2;
			while (tmpfp < userstack_end && tmpfp > userstack_start) {
				copied =
				    access_process_vm(current_task, (unsigned long)tmpfp, &tmp,
						      sizeof(tmp), 0);
				if (copied != sizeof(tmp)) {
					pr_err("access_process_vm  fp error\n");
					/* return -EIO; */
				}
				copied =
				    access_process_vm(current_task, (unsigned long)tmpfp + 0x08,
						      &tmpLR, sizeof(tmpLR), 0);
				if (copied != sizeof(tmpLR)) {
					pr_err("access_process_vm  pc error\n");
					/* return -EIO; */
				}
				tmpfp = tmp;
				native_bt[frames] = tmpLR;
				frames++;
				if (frames >= 16)
					break;
			}
			for (copied = 0; copied < frames; copied++) {
				pr_err("frame:#%d: pc(%016lx)\n", copied, native_bt[copied]);
				/*  #00 pc 000000000006c760  /system/lib64/ libc.so (__epoll_pwait+8) */
				log_to_hang_info(" #%d pc %016lx\n", copied, native_bt[copied]);
			}
			pr_err("tid(%d:%s),frame %d. tmpfp(0x%lx),userstack_start(0x%lx),userstack_end(0x%lx)\n",
				tid, current_task->comm, frames, tmpfp, userstack_start, userstack_end);
		}
	}
#endif

	return 0;
}

static void show_bt_by_pid(int task_pid)
{
	struct task_struct *t, *p;
	struct pid *pid;
	int count = 0;
	unsigned state;
	char stat_nam[] = TASK_STATE_TO_CHAR_STR;

	pid = find_get_pid(task_pid);
	t = p = get_pid_task(pid, PIDTYPE_PID);

	if (p != NULL) {
		log_to_hang_info("show_bt_by_pid: %d: %s.\n", task_pid, t->comm);
		save_native_thread_maps(task_pid); 	/* catch maps to Userthread_maps */
		do {
			if (t) {
				pid_t tid = 0;

				tid = task_pid_vnr(t);
				state = t->state ? __ffs(t->state) + 1 : 0;
				pr_err("lhd: %-15.15s %c pid(%d),tid(%d)",
				     t->comm, state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?',
				     task_pid, tid);

				sched_show_task_local(t);	/* catch kernel bt */

				log_to_hang_info("%s sysTid=%d, pid=%d\n", t->comm, tid, task_pid);

				do_send_sig_info(SIGSTOP, SEND_SIG_FORCED, t, true);
				/* change send ptrace_stop to send signal stop */
				save_native_threadinfo_by_tid(tid);	/* catch user-space bt */

				if (stat_nam[state] != 'T')	/* change send ptrace_stop to send signal stop */
					do_send_sig_info(SIGCONT, SEND_SIG_FORCED, t, true);
			}
			if ((++count) % 5 == 4)
				msleep(20);
			log_to_hang_info("---\n");
		} while_each_thread(p, t);
		put_task_struct(t);
	}
	put_pid(pid);
}

static int find_core_task(void)
{
	struct task_struct *task;
	int ret = -1;
	int i;

	reset_core_task_info();
	log_to_hang_info("[Native Hang detect]: -----	%s	------ \n", __func__);
	read_lock(&tasklist_lock);
	for_each_process(task) {
		for (i = 0; i < CORE_TASK_NUM_MAX; i++) {
			if (!strlen(core_task[i].name))
				break;
			/* ZO process stack is NULL, record its pid & name 	*/
			if ((task->state == TASK_DEAD) && (task->exit_state == EXIT_ZOMBIE)) {
				log_to_hang_info("[Native Hang detect]: ZO Task :pid = %d name = %s \n", task->pid, task->comm);
				continue;
			}
			/* init process pis always 1	*/
			if ((strcmp(task->comm, "init") == 0)) {
				continue;
			}
			if ((strcmp(task->comm, core_task[i].name) == 0)) {
				core_task[i].pid = task->pid;
				pr_debug("[Native Hang Detect] %s found pid:%d.\n", core_task[i].name, core_task[i].pid);
			}
		}
	}
	read_unlock(&tasklist_lock);
	for (i = 0; i < CORE_TASK_NUM_MAX; i++) {
		if (!strlen(core_task[i].name))
			break;
		if ((strcmp(core_task[i].name, "system_server") == 0)) {
			if (!core_task[i].pid) {
				pr_debug("[Native Hang Detect]  find system_server fail .\n");
				ret = -1;
			}
		}
	}
	return ret;
}


void reset_hang_info(void)
{
	memset(hang_info, 0, HANG_INFO_MAX);
	hang_info_index = 0;
}

void save_native_hang_monitor_data(void)
{
	int i;

	find_core_task();

	/* show all core task backtrace */
	for (i = 0; i < CORE_TASK_NUM_MAX; i++) {
		if (!strlen(core_task[i].name))
			break;
		if (core_task[i].pid != 0)
			show_bt_by_pid(core_task[i].pid);
	}
	/* show all D state thread kbt */
	show_state_filter_local(TASK_UNINTERRUPTIBLE);

	/* debug_locks = 1; */
	debug_show_all_locks();

	show_free_areas(0, NULL);

	reset_core_task_info();
}

static int hang_detect_thread(void *arg)
{

	/* unsigned long flags; */
	struct sched_param param;
#ifdef CONFIG_SPRD_DEBUG
	static int trigger_flag;
#endif

	param.sched_priority = 99;
	sched_setscheduler(current, SCHED_FIFO, &param);
	reset_hang_info();
	pr_debug("[Native Native Hang Detect] hang_detect thread starts.\n");
	while (!kthread_should_stop()) {
		if (hang_detect_enabled) {
			if (atomic_add_negative(0, &hang_detect_counter)) {
#ifdef CONFIG_SPRD_DEBUG
				/*	do not need panic on userdeug version, just record hang_detect_counter, and show hang_monitor_data one shot .*/
				if (trigger_flag != 0) {
					pr_err("[Native Native Hang Detect] ");
					pr_err("hang_detect_counter:%d ...\n",
						atomic_read(&hang_detect_counter));
					msleep(1000);
					continue;
				}
				trigger_flag = 1;
#endif
				log_to_hang_info("[Native Hang detect]Dump process bt.\n");
#ifndef CONFIG_SPRD_DEBUG
				save_native_hang_monitor_data();
				pr_err("[Native Native Hang Detect] hang_detect_counter:%d, ",
					atomic_read(&hang_detect_counter));
				pr_err("hang detect save data finish ......\n");
				pr_err("[Native Native Hang Detect] hang_detect_counter:%d, ",
					atomic_read(&hang_detect_counter));
				pr_err("we should trigger panic...\n");
				/* wait for wdh */
				msleep(40 * 1000);
				/* check hang_detect_counter before panic */
				if (atomic_add_negative(0, &hang_detect_counter))
					panic("Native hang monitor trigger");
#endif
			}
			atomic_dec(&hang_detect_counter);
			pr_debug("[Native Hang Detect] hang_detect thread counts down %d:%d.\n",
				atomic_read(&hang_detect_counter), hang_detect_timeout);

		} else {
			reset_hang_info();
			pr_notice("[Native Hang Detect] hang_detect disabled.\n");
		}

		msleep(1000); /*	detecet 1s in default */
	}
#ifdef CONFIG_SPRD_DEBUG
			/* should reset trigger_flag before exit */
			trigger_flag = 0;
#endif
	return 0;
}

void native_hang_monitor_para_set(int para)
{
	if (para > 0) {
		reset_hang_info();
		hang_detect_timeout = para;
		atomic_set(&hang_detect_counter, para);
		pr_debug("[Native Hang Detect] hang_detect enabled %d\n", hang_detect_timeout);
	} else {
		pr_debug("[Native Hang Detect] invalid hang_detect para\n");
	}
	return;
}
/* * user control interface  * */
static long monitor_hang_ioctl(struct file *file, unsigned int cmd,  unsigned long arg)
{
	int ret = 0;
	static int surfaceflinger_status;
	int sys_server_timeout;

	switch (cmd) {
	case SS_WDT_CTRL_SET_PARA:
			if (copy_from_user(&sys_server_timeout, (void __user *)arg, sizeof(int))) {
				ret = -1;
				pr_emerg("get para from systemserver failed!!!\n");
				break;
			}
			pr_emerg("systemserver cmd , get para: ( %d)\n", sys_server_timeout);
			native_hang_monitor_para_set(sys_server_timeout);
			break;
	case SF_WDT_CTRL_SET_PARA:
			surfaceflinger_status = (int)arg;
			pr_debug("set surfaceflinger[status]: 0x%x\n", surfaceflinger_status);
			break;
	case SF_WDT_CTRL_GET_PARA:
			if (copy_to_user((void __user *)arg, &surfaceflinger_status, sizeof(int)))
				ret = -1;
			pr_debug("get surfaceflinger[status]: 0x%x\n", surfaceflinger_status);
			break;
	default:
			pr_debug("do not support cmd\n");
	}
	return ret;
}

static const struct file_operations native_hang_monitor_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = monitor_hang_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = monitor_hang_ioctl,
#endif
};


static struct miscdevice native_hang_monitor_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "native_hang_monitor",
	.fops = &native_hang_monitor_fops,
};

#define SEQ_printf(m, x...) \
do {                \
	if (m)          \
		seq_printf(m, x);   \
	else            \
		pr_debug(x);        \
} while (0)



static int monitor_hang_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "hang_detect_enabled: %d ,hang_detect_timeout = %d s ",
			hang_detect_enabled, hang_detect_timeout);
	SEQ_printf(m, "hang_detect_counter: %d\n", atomic_read(&hang_detect_counter));
	return 0;
}

static int monitor_hang_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, monitor_hang_show, inode->i_private);
}

static ssize_t monitor_hang_proc_write(struct file *filp, const char *buf, size_t count, loff_t *data)
{
	char write_buf[SYSDUMP_PROC_BUF_LEN] = {0};

	pr_debug("%s in!!\n", __func__);
	if (count && (count < SYSDUMP_PROC_BUF_LEN)) {
		if (copy_from_user(write_buf, buf, count)) {
			pr_err("%s copy_from_user fail !!\n", __func__);
			return -1;
		}
		write_buf[count] = '\0';

		if (!strncmp(write_buf, "on", 2)) {
			pr_err("%s copy_from_user on !!\n", __func__);

			/*	create hang detect thread */
			if (!hang_detect_enabled) {
				pr_debug("%s:create hang_detect_thread !\n", __func__);
				hd_thread = kthread_run(hang_detect_thread, NULL, "native_hang_detect");
				hang_detect_enabled = 1;
			} else {
				pr_debug("%s:hang_detect_thread already run !\n", __func__);
			}
		} else if (!strncmp(write_buf, "off", 3)) {
			pr_err("%s copy_from_user off !!\n", __func__);
			/*	stop &  hang detect thread */
			if (!hang_detect_enabled) {
				pr_debug("%s:hang_detect_thread already stop !\n", __func__);
			} else {
				pr_debug("%s:stop hang_detect_thread !\n", __func__);
				kthread_stop(hd_thread);
				hang_detect_enabled = 0;
			}
		} else {
			pr_err("%s invalid string , do nothing !!\n", __func__);
		}
	}
	pr_debug("%s out!!\n", __func__);
	return count;
}

static const struct file_operations monitor_enable_fops = {
	.open = monitor_hang_proc_open,
	.write = monitor_hang_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
static int __init monitor_hang_init(void)
{
	int err = 0;
	struct proc_dir_entry *monitor_enable_proc;


	/*	create /dev/native_hang_monitor */
	err = misc_register(&native_hang_monitor_dev);
	if (err) {
		pr_err("failed to register native_hang_monitor_dev device!\n");
		return err;
	}
	/*	create /proc/monitor_enable */
	monitor_enable_proc = proc_create("monitor_enable", 0664, NULL, &monitor_enable_fops);
	if (!monitor_enable_proc)
		return -ENOMEM;

	atomic_set(&hang_detect_counter, WAIT_BOOT_COMPLETE);
	return err;
}

void get_native_hang_monitor_buffer(unsigned long *addr, unsigned long *size, unsigned long *start)
{
	*addr = (unsigned long)hang_info;
	*start = 0;
	*size = HANG_INFO_MAX;
}
static void  __exit monitor_hang_exit(void)
{
	misc_deregister(&native_hang_monitor_dev);
}

module_init(monitor_hang_init);
module_exit(monitor_hang_exit);

MODULE_AUTHOR("xianzhen.wang <xianzhen.wang@unisoc.com>");
MODULE_DESCRIPTION("unisoc native hang monitor ");
MODULE_LICENSE("GPL");
