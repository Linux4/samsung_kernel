/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Updated for S5E7570: JK Kim (jk.man.kim@)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kmsg_dump.h>
#include <linux/kallsyms.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/tick.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/file.h>
#include <linux/reboot.h>
#include <linux/sec_ext.h>
#include <linux/sec_debug.h>
#include <linux/slab.h>
#include <asm/sections.h>

#include <asm/io.h>
#include <asm/cacheflush.h>

#include <soc/samsung/exynos-pmu.h>

#ifdef CONFIG_SEC_DEBUG

extern void (*mach_restart)(int reboot_mode, const char *cmd);

/* enable/disable sec_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
union sec_debug_level_t {
	struct {
		u16 kernel_fault;
		u16 user_fault;
	} en;
	u32 uint_val;
} sec_debug_level = { .en.kernel_fault = 1, };
module_param_named(enable, sec_debug_level.en.kernel_fault, ushort, 0644);
module_param_named(enable_user, sec_debug_level.en.user_fault, ushort, 0644);
module_param_named(level, sec_debug_level.uint_val, uint, 0644);
#include <soc/samsung/exynos-pmu.h>

#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
static unsigned int enable_cp_debug = 1;
module_param_named(enable_cp_debug,enable_cp_debug, uint, 0644);

int sec_debug_is_enabled_for_ssr(void)
{
	return enable_cp_debug;
}
#endif

int dynsyslog_on;
static int __init dynsyslog_level(char *str)
{
	int level;

	if (get_option(&str, &level)) {
		dynsyslog_on = level;
		pr_info("%s: dynsyslog_on: %d\n", __func__, dynsyslog_on);
		return 0;
	}

	return -EINVAL;
}
early_param("DynSysLog", dynsyslog_level);

static int __init sec_bl_mem_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	if (dynsyslog_on == 0)
		return 0;

	/* If we encounter any problem parsing str ... */
	if (!size ||  *str != '@' || kstrtoul(str + 1, 0, &base))
		goto out;

	if (memblock_is_region_reserved(base, size) ||
			memblock_reserve(base, size)) {
		pr_err("%s: failed reserving size %d " \
			"at base 0x%lx\n", __func__, size, base);
		goto out;
	}

	pr_info("%s: Reserved 0x%x at 0x%lx\n", __func__, size, base);

	return 1;
out:
	return 0;
}
__setup("sec_bl_mem=", sec_bl_mem_setup);

int sec_debug_get_debug_level(void)
{
	return sec_debug_level.uint_val;
}

static void sec_debug_user_fault_dump(void)
{
	if (sec_debug_level.en.kernel_fault == 1
	    && sec_debug_level.en.user_fault == 1)
		panic("User Fault");
}

static ssize_t sec_debug_user_fault_write(struct file *file, const char __user *buffer, size_t count, loff_t *offs)
{
	char buf[100];

	if (count > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	if (strncmp(buf, "dump_user_fault", 15) == 0)
		sec_debug_user_fault_dump();

	return count;
}

static const struct file_operations sec_debug_user_fault_proc_fops = {
	.write = sec_debug_user_fault_write,
};

static int __init sec_debug_user_fault_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("user_fault", S_IWUSR | S_IWGRP, NULL,
			    &sec_debug_user_fault_proc_fops);
	if (!entry)
		return -ENOMEM;
	return 0;
}
device_initcall(sec_debug_user_fault_init);


/* layout of SDRAM
	   0: magic (4B)
      4~1023: panic string (1020B)
 1024~0x1000: panic dumper log
 */
#define SEC_DEBUG_MAGIC_PA memblock_start_of_DRAM()
#define SEC_DEBUG_MAGIC_VA phys_to_virt(SEC_DEBUG_MAGIC_PA)

enum sec_debug_upload_magic_t {
	UPLOAD_MAGIC_INIT		= 0x0,
	UPLOAD_MAGIC_PANIC		= 0x66262564,
};

enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT		= 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC	= 0x000000C8,
	UPLOAD_CAUSE_FORCED_UPLOAD	= 0x00000022,
#ifdef CONFIG_SEC_UPLOAD
	UPLOAD_CAUSE_USER_FORCED_UPLOAD = 0x00000074,
#endif
	UPLOAD_CAUSE_CP_ERROR_FATAL	= 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT		= 0x0000002F,
	UPLOAD_CAUSE_HSIC_DISCONNECTED	= 0x000000DD,
	UPLOAD_CAUSE_POWERKEY_LONG_PRESS = 0x00000085,
};

static void sec_debug_set_upload_magic(unsigned magic, char *str)
{
	*(unsigned int *)SEC_DEBUG_MAGIC_VA = magic;
	*(unsigned int *)(SEC_DEBUG_MAGIC_VA + SZ_4K -4) = magic;

	if (str)
		strncpy((char *)SEC_DEBUG_MAGIC_VA + 4, str, SZ_1K - 4);

	pr_emerg("sec_debug: set magic code (0x%x)\n", magic);
}

static void sec_debug_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	exynos_pmu_write(EXYNOS_PMU_INFORM3, type);

	pr_emerg("sec_debug: set upload cause (0x%x)\n", type);
}

static void sec_debug_kmsg_dump(struct kmsg_dumper *dumper, enum kmsg_dump_reason reason)
{
	char *ptr = (char *)SEC_DEBUG_MAGIC_VA + SZ_1K;
#if 0
	int total_chars = SZ_4K - SZ_1K;
	int total_lines = 50;
	/* no of chars which fits in total_chars *and* in total_lines */
	int last_chars;

	for (last_chars = 0;
	     l2 && l2 > last_chars && total_lines > 0
	     && total_chars > 0; ++last_chars, --total_chars) {
		if (s2[l2 - last_chars] == '\n')
			--total_lines;
	}
	s2 += (l2 - last_chars);
	l2 = last_chars;

	for (last_chars = 0;
	     l1 && l1 > last_chars && total_lines > 0
	     && total_chars > 0; ++last_chars, --total_chars) {
		if (s1[l1 - last_chars] == '\n')
			--total_lines;
	}
	s1 += (l1 - last_chars);
	l1 = last_chars;

	while (l1-- > 0)
		*ptr++ = *s1++;
	while (l2-- > 0)
		*ptr++ = *s2++;
#endif
	kmsg_dump_get_buffer(dumper, true, ptr, SZ_4K - SZ_1K, NULL);

}

static struct kmsg_dumper sec_dumper = {
	.dump = sec_debug_kmsg_dump,
};

int __init sec_debug_setup(void)
{
	size_t size = SZ_4K;
	size_t base = SEC_DEBUG_MAGIC_PA;

#ifdef CONFIG_NO_BOOTMEM
	if (!memblock_is_region_reserved(base, size) &&
		!memblock_reserve(base, size)) {
#else
	if (!reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE)) {
#endif
		pr_info("sec_debug: succeed to reserve dedicated memory (0x%zx, 0x%zx)\n", base, size);

		sec_debug_set_upload_magic(UPLOAD_MAGIC_PANIC, NULL);
	} else
		goto out;

	kmsg_dump_register(&sec_dumper);

	return 0;
out:
	pr_err("sec_debug: failed to reserve dedicated memory (0x%zx, 0x%zx)\n", base, size);
	return -ENOMEM;
}

static int __init sec_debug_init(void)
{
	sec_debug_set_upload_cause(UPLOAD_CAUSE_INIT);

	return 0;
}
fs_initcall(sec_debug_init);

#if 0
/* task state
 * R (running)
 * S (sleeping)
 * D (disk sleep)
 * T (stopped)
 * t (tracing stop)
 * Z (zombie)
 * X (dead)
 * K (killable)
 * W (waking)
 * P (parked)
 */
static void sec_debug_dump_task_info(struct task_struct *tsk, bool is_main)
{
	char state_array[] = { 'R', 'S', 'D', 'T', 't', 'Z', 'X', 'x', 'K', 'W', 'P' };
	unsigned char idx = 0;
	unsigned int state = (tsk->state & TASK_REPORT) | tsk->exit_state;
	unsigned long wchan;
	unsigned long pc = 0;
	char symname[KSYM_NAME_LEN];
	int permitted;
	struct mm_struct *mm;

	permitted = ptrace_may_access(tsk, PTRACE_MODE_READ);
	mm = get_task_mm(tsk);
	if (mm) {
		if (permitted)
			pc = KSTK_EIP(tsk);
	}

	wchan = get_wchan(tsk);
	if (lookup_symbol_name(wchan, symname) < 0) {
		if (!ptrace_may_access(tsk, PTRACE_MODE_READ))
			sprintf(symname, "_____");
		else
			sprintf(symname, "%lu", wchan);
	}

	while (state) {
		idx++;
		state >>= 1;
	}

	pr_info("%8d %8d %8d %16lld %c(%d) %3d  %16zx %16zx  %16zx %c %16s [%s]\n",
		 tsk->pid, (int)(tsk->utime), (int)(tsk->stime), tsk->se.exec_start,
		 state_array[idx], (int)(tsk->state), task_cpu(tsk), wchan, pc,
		 (unsigned long)tsk, is_main ? '*' : ' ', tsk->comm, symname);

	if (tsk->state == TASK_RUNNING ||
			tsk->state == TASK_UNINTERRUPTIBLE || tsk->mm == NULL) {
		show_stack(tsk, NULL);
		pr_info("\n");
	}
}

static void sec_debug_dump_all_task(void)
{
	struct task_struct *frst_tsk;
	struct task_struct *curr_tsk;
	struct task_struct *frst_thr;
	struct task_struct *curr_thr;

	pr_info("\n");
	pr_info("current proc : %d %s\n", current->pid, current->comm);
	pr_info("----------------------------------------------------------------------------------------------------------------------------\n");
	pr_info("    pid      uTime    sTime      exec(ns)  stat  cpu       wchan           user_pc        task_struct       comm   sym_wchan\n");
	pr_info("----------------------------------------------------------------------------------------------------------------------------\n");

	/* processes */
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while (curr_tsk != NULL) {
		sec_debug_dump_task_info(curr_tsk, true);
		/* threads */
		if (curr_tsk->thread_group.next != NULL) {
			frst_thr = container_of(curr_tsk->thread_group.next, struct task_struct, thread_group);
			curr_thr = frst_thr;
			if (frst_thr != curr_tsk) {
				while (curr_thr != NULL) {
					sec_debug_dump_task_info(curr_thr, false);
					curr_thr = container_of(curr_thr->thread_group.next, struct task_struct, thread_group);
					if (curr_thr == curr_tsk)
						break;
				}
			}
		}
		curr_tsk = container_of(curr_tsk->tasks.next, struct task_struct, tasks);
		if (curr_tsk == frst_tsk)
			break;
	}
	pr_info("----------------------------------------------------------------------------------------------------------------------------\n");
}
#endif

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif
#ifdef arch_idle_time
static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}
#else
static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = usecs_to_cputime64(idle_time);

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = usecs_to_cputime64(iowait_time);

	return iowait;
}
#endif

static void sec_debug_dump_cpu_stat(void)
{
	int i, j;
	u64 user, nice, system, idle, iowait, irq, softirq, steal;
	u64 guest, guest_nice;
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = {0};

	char *softirq_to_name[NR_SOFTIRQS] = { "HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "BLOCK_IOPOLL", "TASKLET", "SCHED", "HRTIMER", "RCU" };

	user = nice = system = idle = iowait = irq = softirq = steal = 0;
	guest = guest_nice = 0;

	for_each_possible_cpu(i) {
		user	+= kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice	+= kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system	+= kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle	+= get_idle_time(i);
		iowait	+= get_iowait_time(i);
		irq	+= kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq	+= kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal	+= kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest	+= kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		sum	+= kstat_cpu_irqs_sum(i);
		sum	+= arch_irq_stat_cpu(i);

		for (j = 0; j < NR_SOFTIRQS; j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);

			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();

	pr_info("\n");
	pr_info("cpu   user:%llu \tnice:%llu \tsystem:%llu \tidle:%llu \tiowait:%llu \tirq:%llu \tsoftirq:%llu \t %llu %llu %llu\n",
			(unsigned long long)cputime64_to_clock_t(user),
			(unsigned long long)cputime64_to_clock_t(nice),
			(unsigned long long)cputime64_to_clock_t(system),
			(unsigned long long)cputime64_to_clock_t(idle),
			(unsigned long long)cputime64_to_clock_t(iowait),
			(unsigned long long)cputime64_to_clock_t(irq),
			(unsigned long long)cputime64_to_clock_t(softirq),
			(unsigned long long)cputime64_to_clock_t(steal),
			(unsigned long long)cputime64_to_clock_t(guest),
			(unsigned long long)cputime64_to_clock_t(guest_nice));
	pr_info("-------------------------------------------------------------------------------------------------------------\n");

	for_each_possible_cpu(i) {
		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user	= kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice	= kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system	= kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle	= get_idle_time(i);
		iowait	= get_iowait_time(i);
		irq	= kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq	= kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal	= kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest	= kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice = kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];

		pr_info("cpu%d  user:%llu \tnice:%llu \tsystem:%llu \tidle:%llu \tiowait:%llu \tirq:%llu \tsoftirq:%llu \t %llu %llu %llu\n",
				i,
				(unsigned long long)cputime64_to_clock_t(user),
				(unsigned long long)cputime64_to_clock_t(nice),
				(unsigned long long)cputime64_to_clock_t(system),
				(unsigned long long)cputime64_to_clock_t(idle),
				(unsigned long long)cputime64_to_clock_t(iowait),
				(unsigned long long)cputime64_to_clock_t(irq),
				(unsigned long long)cputime64_to_clock_t(softirq),
				(unsigned long long)cputime64_to_clock_t(steal),
				(unsigned long long)cputime64_to_clock_t(guest),
				(unsigned long long)cputime64_to_clock_t(guest_nice));
	}
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info("irq : %llu", (unsigned long long)sum);
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	/* sum again ? it could be updated? */
	for_each_irq_nr(j) {
		unsigned int irq_stat = kstat_irqs(j);
		if (irq_stat) {
			pr_info("irq-%-4d : %8u %s\n", j, irq_stat,
					irq_to_desc(j)->action ? irq_to_desc(j)->action->name ? : "???" : "???");
		}
	}
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info("softirq : %llu", (unsigned long long)sum_softirq);
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	for (i = 0; i < NR_SOFTIRQS; i++)
		if (per_softirq_sums[i])
			pr_info("softirq-%d : %8u %s\n", i, per_softirq_sums[i], softirq_to_name[i]);
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
}

void sec_debug_reboot_handler()
{
	/* Clear magic code in normal reboot */
	sec_debug_set_upload_magic(UPLOAD_MAGIC_INIT, NULL);
}

void sec_debug_panic_handler(void *buf, bool dump)
{
	/* Set upload cause */
	sec_debug_set_upload_magic(UPLOAD_MAGIC_PANIC, buf);
	if (!strcmp(buf, "User Fault"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (!strcmp(buf, "Crash Key"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
#ifdef CONFIG_SEC_UPLOAD
 	else if (!strncmp(buf, "User Crash Key", 14))
 		sec_debug_set_upload_cause(UPLOAD_CAUSE_USER_FORCED_UPLOAD);
#endif
	else if (!strncmp(buf, "CP Crash", 8))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strcmp(buf, "HSIC Disconnected"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_HSIC_DISCONNECTED);
	else
		sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);

	/* dump debugging info */
	if (dump) {
#if 0
		sec_debug_dump_all_task();
		show_state_filter(TASK_STATE_MAX);
#endif
		sec_debug_dump_cpu_stat();
		debug_show_all_locks();
	}
}

void sec_debug_post_panic_handler(void)
{
	/* reset */
	pr_emerg("sec_debug: %s\n", linux_banner);
	pr_emerg("sec_debug: rebooting...\n");

	flush_cache_all();
	mach_restart(REBOOT_SOFT, "sw reset");

	while (1)
		cpu_relax();

	/* Never run this function */
	pr_emerg("sec_debug: DO NOT RUN this function (CPU:%d)\n",
					raw_smp_processor_id());
}

#ifdef CONFIG_SEC_DEBUG_FILE_LEAK
void sec_debug_print_file_list(void)
{
	int i=0;
	unsigned int nCnt=0;
	struct file *file=NULL;
	struct files_struct *files = current->files;
	const char *pRootName=NULL;
	const char *pFileName=NULL;

	nCnt=files->fdt->max_fds;

	printk(KERN_ERR " [Opened file list of process %s(PID:%d, TGID:%d) :: %d]\n",
		current->group_leader->comm, current->pid, current->tgid,nCnt);

	for (i=0; i<nCnt; i++) {

		rcu_read_lock();
		file = fcheck_files(files, i);

		pRootName=NULL;
		pFileName=NULL;

		if (file) {
			if (file->f_path.mnt
				&& file->f_path.mnt->mnt_root
				&& file->f_path.mnt->mnt_root->d_name.name)
				pRootName=file->f_path.mnt->mnt_root->d_name.name;

			if (file->f_path.dentry && file->f_path.dentry->d_name.name)
				pFileName=file->f_path.dentry->d_name.name;

			printk(KERN_ERR "[%04d]%s%s\n",i,pRootName==NULL?"null":pRootName,
							pFileName==NULL?"null":pFileName);
		}
		rcu_read_unlock();
	}
}

void sec_debug_EMFILE_error_proc(unsigned long files_addr)
{
	if (files_addr!=(unsigned long)(current->files)) {
		printk(KERN_ERR "Too many open files Error at %pS\n"
						"%s(%d) thread of %s process tried fd allocation by proxy.\n"
						"files_addr = 0x%lx, current->files=0x%p\n",
					__builtin_return_address(0),
					current->comm,current->tgid,current->group_leader->comm,
					files_addr, current->files);
		return;
	}

	printk(KERN_ERR "Too many open files(%d:%s) at %pS\n",
		current->tgid, current->group_leader->comm,__builtin_return_address(0));

	if (!sec_debug_level.en.kernel_fault)
		return;

	/* We check EMFILE error in only "system_server","mediaserver" and "surfaceflinger" process.*/
	if (!strcmp(current->group_leader->comm, "system_server")
		||!strcmp(current->group_leader->comm, "mediaserver")
		||!strcmp(current->group_leader->comm, "surfaceflinger")){
		sec_debug_print_file_list();
		panic("Too many open files");
	}
}
#endif /* CONFIG_SEC_DEBUG_FILE_LEAK */

#endif /* CONFIG_SEC_DEBUG */


#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
static char *last_kmsg_buffer;
static size_t last_kmsg_size;
void sec_debug_save_last_kmsg(unsigned char* head_ptr, unsigned char* curr_ptr, size_t buf_size)
{
	size_t size;
	unsigned char* magickey_addr;

	if (head_ptr == NULL || curr_ptr == NULL || head_ptr == curr_ptr) {
		pr_err("%s: no data \n", __func__);
		return;
	}

	if ((curr_ptr - head_ptr) <= 0) {
		pr_err("%s: invalid args \n", __func__);
		return;
	}
	size = (size_t)(curr_ptr - head_ptr);

	magickey_addr = head_ptr + buf_size - (size_t)0x08;

	/* provide previous log as last_kmsg */
	if(*((unsigned long long *)magickey_addr) == SEC_LKMSG_MAGICKEY) {
		pr_info("%s: sec_log buffer is full\n", __func__);
		last_kmsg_size = (size_t)SZ_2M;
		last_kmsg_buffer = (char *)kzalloc(last_kmsg_size, GFP_NOWAIT);
		if(last_kmsg_size && last_kmsg_buffer) {
			memcpy(last_kmsg_buffer, curr_ptr, last_kmsg_size - size);
			memcpy(last_kmsg_buffer + (last_kmsg_size - size), head_ptr, size);
			pr_info("%s: successed\n", __func__);
		} else
			pr_err("%s: failed\n", __func__);
	}
	else {
		pr_info("%s: sec_log buffer is not full\n", __func__);
		last_kmsg_size = size;
		last_kmsg_buffer = (char *)kzalloc(last_kmsg_size, GFP_NOWAIT);
		if(last_kmsg_size && last_kmsg_buffer) {
			memcpy(last_kmsg_buffer, head_ptr, last_kmsg_size);
			pr_info("%s: successed\n", __func__);
		} else
			pr_err("%s: failed\n", __func__);
	}
}

static ssize_t sec_last_kmsg_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (pos >= last_kmsg_size)
		return 0;

	count = min(len, (size_t) (last_kmsg_size - pos));
	if (copy_to_user(buf, last_kmsg_buffer + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations last_kmsg_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_last_kmsg_read,
};

static int __init sec_last_kmsg_late_init(void)
{
	struct proc_dir_entry *entry;

	if (last_kmsg_buffer == NULL)
		return 0;

	entry = proc_create("last_kmsg", S_IFREG | S_IRUGO,
			NULL, &last_kmsg_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, last_kmsg_size);
	return 0;
}

late_initcall(sec_last_kmsg_late_init);
#endif /* CONFIG_SEC_DEBUG_LAST_KMSG */

#if defined(CONFIG_KFAULT_AUTO_SUMMARY)
static struct sec_debug_auto_summary* auto_summary_info = 0;

struct auto_summary_log_map {
	char prio_level;
	char max_count;
};

static const struct auto_summary_log_map init_data[SEC_DEBUG_AUTO_COMM_BUF_SIZE]
	= {{PRIO_LV0, 0}, {PRIO_LV5, 8}, {PRIO_LV9, 0}, {PRIO_LV5, 0}, {PRIO_LV5, 0}, 
	{PRIO_LV1, 1}, {PRIO_LV2, 2}, {PRIO_LV5, 0}, {PRIO_LV5, 8}, {PRIO_LV0, 0}};

extern void register_set_auto_comm_buf(void (*func)(int type, const char *buf, size_t size));
extern void register_set_auto_comm_lastfreq(void (*func)(int type, int old_freq, int new_freq, u64 time));

void sec_debug_auto_summary_log_disable(int type)
{
	atomic_inc(&auto_summary_info->auto_Comm_buf[type].logging_diable);
}

void sec_debug_auto_summary_log_once(int type)
{
	if (atomic64_read(&auto_summary_info->auto_Comm_buf[type].logging_entry))
		sec_debug_auto_summary_log_disable(type);
	else
		atomic_inc(&auto_summary_info->auto_Comm_buf[type].logging_entry);
}

static inline void sec_debug_hook_auto_comm_lastfreq(int type, int old_freq, int new_freq, u64 time)
{
	if(type < FREQ_INFO_MAX) {
		auto_summary_info->freq_info[type].old_freq = old_freq;
		auto_summary_info->freq_info[type].new_freq = new_freq;
		auto_summary_info->freq_info[type].time_stamp = time;
	}
}

static inline void sec_debug_hook_auto_comm(int type, const char *buf, size_t size)
{
	struct sec_debug_auto_comm_buf* auto_comm_buf = &auto_summary_info->auto_Comm_buf[type];
	int offset = auto_comm_buf->offset;

	if (atomic64_read(&auto_comm_buf->logging_diable))
		return;

	if (offset + size > SZ_4K)
		return;

	if (init_data[type].max_count &&
		atomic64_read(&auto_comm_buf->logging_count) > init_data[type].max_count)
		return;

	if (!(auto_summary_info->fault_flag & 1 << type)) {
		auto_summary_info->fault_flag |= 1 << type;
		if(init_data[type].prio_level == PRIO_LV5) {
			auto_summary_info->lv5_log_order |= type << auto_summary_info->lv5_log_cnt * 4;
			auto_summary_info->lv5_log_cnt++;
		}		
		auto_summary_info->order_map[auto_summary_info->order_map_cnt++] = type;
	}
	
	atomic_inc(&auto_comm_buf->logging_count);

	memcpy(auto_comm_buf->buf + offset, buf, size);
	auto_comm_buf->offset += size;
}

static void sec_auto_summary_init_print_buf(unsigned long base)
{
	auto_summary_info = (struct sec_debug_auto_summary*)phys_to_virt(base+SZ_4K);

	memset(auto_summary_info, 0, sizeof(struct sec_debug_auto_summary));

	auto_summary_info->haeder_magic = AUTO_SUMMARY_MAGIC;
	auto_summary_info->tail_magic = AUTO_SUMMARY_TAIL_MAGIC;
	
	auto_summary_info->pa_text = virt_to_phys(_text);
	auto_summary_info->pa_start_rodata = virt_to_phys(__start_rodata);
	
	register_set_auto_comm_buf(sec_debug_hook_auto_comm);
	register_set_auto_comm_lastfreq(sec_debug_hook_auto_comm_lastfreq);
	sec_debug_set_auto_comm_last_devfreq_buf(auto_summary_info->freq_info);
	sec_debug_set_auto_comm_last_cpufreq_buf(auto_summary_info->freq_info);
}

static int __init sec_auto_summary_log_setup(char *str)
{
	unsigned long size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || *str != '@' || kstrtoul(str + 1, 0, &base)) {
		pr_err("%s: failed to parse address.\n", __func__);
		goto out;
	}

#ifdef CONFIG_NO_BOOTMEM
		if (memblock_is_region_reserved(base, size) ||
			memblock_reserve(base, size)) {
#else
		if (reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE)) {
#endif
		pr_err("%s: failed to reserve size:0x%lx " \
				"at base 0x%lx\n", __func__, size, base);
		goto out;
	}
		
	sec_auto_summary_init_print_buf(base);	

	pr_info("%s, base:0x%lx size:0x%lx\n", __func__, base, size);
	
out:
	return 0;
}
__setup("auto_summary_log=", sec_auto_summary_log_setup);
#endif
