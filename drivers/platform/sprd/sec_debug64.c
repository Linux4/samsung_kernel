/*
 * sec-debug64.c
 *
 * Copyright (C) 2014 Samsung Electronics Co, Ltd.
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
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/bootmem.h>
#include <linux/kmsg_dump.h>
#include <linux/kallsyms.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/moduleparam.h>
#include <soc/sprd/sec_debug64.h>
#include <linux/seq_file.h>
#include <asm/system_misc.h>

union sec_debug_level_t sec_debug_level = { .en.kernel_fault = 1, };
module_param_named(enable, sec_debug_level.en.kernel_fault, ushort, 0644);
module_param_named(enable_user, sec_debug_level.en.user_fault, ushort, 0644);
module_param_named(level, sec_debug_level.uint_val, uint, 0644);

DEFINE_PER_CPU(struct pt_regs, sec_debug_aarch64_core_reg);
DEFINE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_aarch64_mmu_reg);
DEFINE_PER_CPU(unsigned int, panic_sign);
DEFINE_PER_CPU(enum sec_debug_upload_cause_t, sec_debug_upload_cause);

void sec_debug_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	static const unsigned int VOLUME_UP = KEY_VOLUMEUP;
	static const unsigned int VOLUME_DOWN = KEY_VOLUMEDOWN;

	if (!sec_debug_level.en.kernel_fault)
		return;

	/* Enter Force Upload
	 *  Hold volume down key first
	 *  and then press power key twice
	 *  and volume up key should not be pressed
	 */
	if (value) {
		if (code == VOLUME_UP)
			volup_p = true;
		if (code == VOLUME_DOWN)
			voldown_p = true;
		if (!volup_p && voldown_p) {
			if (code == KEY_POWER) {
				pr_info
				    ("%s: count for enter forced upload : %d\n",
				     __func__, ++loopcount);
				if (loopcount == 2) {
					panic("Crash Key");
				}
			}
		}
	} else {
		if (code == VOLUME_UP)
			volup_p = false;
		if (code == VOLUME_DOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}

static void sec_debug_set_panic_sign(int cpu, unsigned int val)
{
	per_cpu(panic_sign, cpu) = val;	
}

static unsigned int sec_debug_get_panic_sign(int cpu)
{
	return per_cpu(panic_sign, cpu);
}

static void sec_debug_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	per_cpu(sec_debug_upload_cause, smp_processor_id()) = type;
	__raw_writel(type, (void __iomem *)SPRD_INFORM3);
	__raw_writel(type, (void __iomem *)SPRD_INFORM4);
	__raw_writel(type, (void __iomem *)SPRD_INFORM6);
	pr_emerg("(%s) %x\n", __func__, type);
}

static sec_debug_set_upload_magic(unsigned magic)
{
	pr_emerg("(%s) %x\n", __func__, magic);
	__raw_writel(magic, (void __iomem *)SEC_DEBUG_MAGIC_VA);
	flush_cache_all();
}

static void sec_debug_hw_reset(void)
{
	pr_emerg("(%s) rebooting...\n", __func__);
	flush_cache_all();
	machine_restart(NULL);
}

/* core_regs can have both NULL and !NULL
 * In case of NULL ==> sec_debug_save_core_reg() is not called by path to exception routine
 * In case of != NULL ==> sec_debug_save_core_reg() is called by path to exception routine
 *  so core_regs could save cpu register through "kernel_entry" macro
 */
static int sec_debug_save_core_reg(void *core_regs)
{
	register unsigned long sp asm ("sp");
	struct pt_regs *regs = (struct pt_regs *)core_regs;
	struct pt_regs *saved_regs = &per_cpu(sec_debug_aarch64_core_reg, smp_processor_id());

	if(!regs) {
		asm("str x0, [%0, #0]\n\t"
		    "mov x0, %0\n\t"
		    "str x1, [x0, #8]\n\t"
		    "str x2, [x0, #16]\n\t"
		    "str x3, [x0, #24]\n\t"
		    "str x4, [x0, #32]\n\t"
		    "str x5, [x0, #40]\n\t"
		    "str x6, [x0, #48]\n\t"
		    "str x7, [x0, #56]\n\t"
		    "str x8, [x0, #64]\n\t"
		    "str x9, [x0, #72]\n\t"
		    "str x10, [x0, #80]\n\t"
		    "str x11, [x0, #88]\n\t"
		    "str x12, [x0, #96]\n\t"
		    "str x13, [x0, #104]\n\t"
		    "str x14, [x0, #112]\n\t"
		    "str x15, [x0, #120]\n\t"
		    "str x16, [x0, #128]\n\t"
		    "str x17, [x0, #136]\n\t"
		    "str x18, [x0, #144]\n\t"
		    "str x19, [x0, #152]\n\t"
		    "str x20, [x0, #160]\n\t"
		    "str x21, [x0, #168]\n\t"
		    "str x22, [x0, #176]\n\t"
		    "str x23, [x0, #184]\n\t"
		    "str x24, [x0, #192]\n\t"
		    "str x25, [x0, #200]\n\t"
		    "str x26, [x0, #208]\n\t"
		    "str x27, [x0, #216]\n\t"
		    "str x28, [x0, #224]\n\t"
		    "str x29, [x0, #232]\n\t"
		    "str x30, [x0, #240]\n\t" 
		    "mrs x1, nzcv\n\t"
		    "mrs x2, daif\n\t"
		    "orr x1, x1, x2\n\t"
		    "mrs x2, CurrentEL\n\t"
		    "orr x1, x1, x2\n\t"
		    "mrs x2, SPSel\n\t"
		    "orr x1, x1, x2\n\t"
		    "str x1, [x0, #264]\n\t" :
		    : "r"(saved_regs));
		saved_regs->sp = (unsigned long)(sp);
		//we cannot get the PC directly, so we get the PC through the lr register(x30)-4
		saved_regs->pc = (unsigned long)(saved_regs->regs[30] - sizeof(unsigned int));

	} else {
		memcpy(saved_regs, regs, sizeof(struct pt_regs));
	}

	return 0;
}

static void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	asm("mrs x1, SCTLR_EL1\n\t"		/* SCTLR_EL1 */
	    "str x1, [%0]\n\t"
	    "mrs x1, TTBR0_EL1\n\t"		/* TTBR0_EL1 */
	    "str x1, [%0,#8]\n\t"
	    "mrs x1, TTBR1_EL1\n\t"		/* TTBR1_EL1 */
	    "str x1, [%0,#16]\n\t"
	    "mrs x1, TCR_EL1\n\t"		/* TCR_EL1 */
	    "str x1, [%0,#24]\n\t"
	    "mrs x1, ESR_EL1\n\t"		/* ESR_EL1 */
	    "str x1, [%0,#32]\n\t"
	    "mrs x1, FAR_EL1\n\t"		/* FAR_EL1 */
	    "str x1, [%0,#40]\n\t"
	    "mrs x1, CONTEXTIDR_EL1\n\t"	/* CONTEXTIDR_EL1 */
	    "str x1, [%0,#48]\n\t"
	    "mrs x1, TPIDR_EL0\n\t"		/* TPIDR_EL0 */
	    "str x1, [%0,#56]\n\t"
	    "mrs x1, TPIDRRO_EL0\n\t"		/* TPIDRRO_EL0 */
	    "str x1, [%0,#64]\n\t"
	    "mrs x1, TPIDR_EL1\n\t"		/* TPIDR_EL1 */
	    "str x1, [%0,#72]\n\t"
	    "mrs x1, MAIR_EL1\n\t"		/* MAIR_EL1 */
	    "str x1, [%0,#80]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%x1", "memory"			/* clobbered register */
	);
}

/* CPUMERRSR : CPU Memory Error Syndrome Register
 * - detect L1 cache memory access error
 * L2MERRSR : L2 Memory Error Syndrome Register
 * - detect L2 cache memory access error
 * if the panic is due to memory access, then we can know more details from CPUMERRSR and L2MERRSR
 */ 	
static void sec_debug_dump_memory_error(void)
{
	unsigned long reg1, reg2;
	asm ("mrs %0, S3_1_c15_c2_2\n\t"
		"mrs %1, S3_1_c15_c2_3\n"
		: "=r" (reg1), "=r" (reg2));
	pr_emerg("CPUMERRSR: %016lx, L2MERRSR: %016lx\n", reg1, reg2);
}

static int sec_debug_normal_reboot_handler(struct notifier_block *nb, unsigned long l, void *p)
{
	sec_debug_set_upload_magic(0x0);
	return 0;
}

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next,
				struct task_struct,
				thread_group);
}

static void dump_one_task_info(struct task_struct *tsk, bool is_main)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'Z', 'X', 'x', 'K', 'W'};
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

	pr_info("%8d %8d %8d %16lld %c(%d) %3d  %08x %08x  %08x %c %16s [%s]\n",
			tsk->pid, (int)(tsk->utime), (int)(tsk->stime),
			tsk->se.exec_start, state_array[idx], (int)(tsk->state),
			task_cpu(tsk), (int)wchan, (int)pc, (int)tsk,
			is_main ? '*' : ' ', tsk->comm, symname);

	if (tsk->state == TASK_RUNNING
			|| tsk->state == TASK_UNINTERRUPTIBLE
			|| tsk->mm == NULL) {
		show_stack(tsk, NULL);
		pr_info("\n");
	}
}

static void dump_all_task_info(void)
{
	struct task_struct *frst_tsk;
	struct task_struct *curr_tsk;
	struct task_struct *frst_thr;
	struct task_struct *curr_thr;

	pr_info("\n");
	pr_info(" current proc : %d %s\n", current->pid, current->comm);
	pr_info(" -------------------------------------------------------------------------------------------------------------\n");
	pr_info("     pid      uTime    sTime      exec(ns)  stat  cpu   wchan   user_pc  task_struct          comm   sym_wchan\n");
	pr_info(" -------------------------------------------------------------------------------------------------------------\n");

	/* processes */
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while (curr_tsk != NULL) {
		dump_one_task_info(curr_tsk,  true);
		/* threads */
		if (curr_tsk->thread_group.next != NULL) {
			frst_thr = get_next_thread(curr_tsk);
			curr_thr = frst_thr;
			if (frst_thr != curr_tsk) {
				while (curr_thr != NULL) {
					dump_one_task_info(curr_thr, false);
					curr_thr = get_next_thread(curr_thr);
					if (curr_thr == curr_tsk)
						break;
				}
			}
		}
		curr_tsk = container_of(curr_tsk->tasks.next,
					struct task_struct, tasks);
		if (curr_tsk == frst_tsk)
			break;
	}
	pr_info(" -----------------------------------------------------------------------------------\n");
}

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif
#ifndef arch_idle_time
#define arch_idle_time(cpu) 0
#endif

static void dump_cpu_stat(void)
{
	int cpu_i, irq_j;
	unsigned long jif;
	cputime64_t user, nice, system, idle, iowait, irq, softirq, steal;
	cputime64_t guest, guest_nice;
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = {0};
	struct timespec boottime;
	unsigned int per_irq_sum;

	char *softirq_to_name[NR_SOFTIRQS] = {
	     "HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "BLOCK_IOPOLL",
	     "TASKLET", "SCHED", "HRTIMER",  "RCU"
	};

	user = nice = system = idle = iowait = 0UL;
	irq = softirq = steal = 0UL;
	guest = guest_nice = 0UL;

	getboottime(&boottime);
	jif = boottime.tv_sec;
	for_each_possible_cpu(cpu_i) {
		user = user + kcpustat_cpu(cpu_i).cpustat[CPUTIME_USER];
		nice = nice + kcpustat_cpu(cpu_i).cpustat[CPUTIME_NICE];
		system = system + kcpustat_cpu(cpu_i).cpustat[CPUTIME_SYSTEM];
		idle = idle + kcpustat_cpu(cpu_i).cpustat[CPUTIME_IDLE];
		idle = idle + arch_idle_time(cpu_i);
		iowait = iowait + kcpustat_cpu(cpu_i).cpustat[CPUTIME_IOWAIT];
		irq = irq + kcpustat_cpu(cpu_i).cpustat[CPUTIME_IRQ];
		softirq = softirq + kcpustat_cpu(cpu_i).cpustat[CPUTIME_SOFTIRQ];

		for_each_irq_nr(irq_j) {
			sum += kstat_irqs_cpu(irq_j, cpu_i);
		}
		sum += arch_irq_stat_cpu(cpu_i);
		for (irq_j = 0; irq_j < NR_SOFTIRQS; irq_j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(irq_j, cpu_i);
			per_softirq_sums[irq_j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();
	pr_info("\n");
	pr_info(" cpu     user:%llu  nice:%llu  system:%llu  idle:%llu  " \
		"iowait:%llu  irq:%llu  softirq:%llu %llu %llu " "%llu\n",
			(unsigned long long)cputime64_to_clock_t(user),
			(unsigned long long)cputime64_to_clock_t(nice),
			(unsigned long long)cputime64_to_clock_t(system),
			(unsigned long long)cputime64_to_clock_t(idle),
			(unsigned long long)cputime64_to_clock_t(iowait),
			(unsigned long long)cputime64_to_clock_t(irq),
			(unsigned long long)cputime64_to_clock_t(softirq),
			(unsigned long long)0,	/* steal */
			(unsigned long long)0,	/* guest */
			(unsigned long long)0);	/* guest_nice */
	pr_info(" -----------------------------------------------------------------------------------\n");
	for_each_online_cpu(cpu_i) {
		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user = kcpustat_cpu(cpu_i).cpustat[CPUTIME_USER];
		nice = kcpustat_cpu(cpu_i).cpustat[CPUTIME_NICE];
		system = kcpustat_cpu(cpu_i).cpustat[CPUTIME_SYSTEM];
		idle = kcpustat_cpu(cpu_i).cpustat[CPUTIME_IDLE];
		idle = idle + arch_idle_time(cpu_i);
		iowait = kcpustat_cpu(cpu_i).cpustat[CPUTIME_IOWAIT];
		irq = kcpustat_cpu(cpu_i).cpustat[CPUTIME_IRQ];
		softirq = kcpustat_cpu(cpu_i).cpustat[CPUTIME_SOFTIRQ];
		/* steal = kstat_cpu(cpu_i).cpustat.steal; */
		/* guest = kstat_cpu(cpu_i).cpustat.guest; */
		/* guest_nice = kstat_cpu(cpu_i).cpustat.guest_nice; */
		pr_info(" cpu %d   user:%llu  nice:%llu  system:%llu  " \
			"idle:%llu  iowait:%llu  irq:%llu  softirq:%llu "
			"%llu %llu " "%llu\n",
			cpu_i,
			(unsigned long long)cputime64_to_clock_t(user),
			(unsigned long long)cputime64_to_clock_t(nice),
			(unsigned long long)cputime64_to_clock_t(system),
			(unsigned long long)cputime64_to_clock_t(idle),
			(unsigned long long)cputime64_to_clock_t(iowait),
			(unsigned long long)cputime64_to_clock_t(irq),
			(unsigned long long)cputime64_to_clock_t(softirq),
			(unsigned long long)0,	/* steal */
			(unsigned long long)0,	/* guest */
			(unsigned long long)0);	/* guest_nice */
	}
	pr_info(" -----------------------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info(" irq : %llu", (unsigned long long)sum);
	pr_info(" -----------------------------------------------------------------------------------\n");
	/* sum again ? it could be updated? */
	for_each_irq_nr(irq_j) {
		per_irq_sum = 0;
		for_each_possible_cpu(cpu_i)
			per_irq_sum += kstat_irqs_cpu(irq_j, cpu_i);
		if (per_irq_sum) {
			pr_info(" irq-%4d : %8u %s\n",
				irq_j, per_irq_sum, irq_to_desc(irq_j)->action ?
				irq_to_desc(irq_j)->action->name ?: "???" : "???");
		}
	}
	pr_info(" -----------------------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info(" softirq : %llu", (unsigned long long)sum_softirq);
	pr_info(" -----------------------------------------------------------------------------------\n");
	for (cpu_i = 0; cpu_i < NR_SOFTIRQS; cpu_i++)
		if (per_softirq_sums[cpu_i])
			pr_info(" softirq-%d : %8u %s\n",
				cpu_i, per_softirq_sums[cpu_i], softirq_to_name[cpu_i]);
	pr_info(" -----------------------------------------------------------------------------------\n");
}



static int sec_debug_panic_handler(struct notifier_block *nb, unsigned long l, void *buf)
{
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	local_irq_disable();
	sec_debug_set_upload_magic(0x66262564);
	if (!strcmp(buf, "User Fault"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (!strcmp(buf, "Crash Key"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strcmp(buf, "HSIC Disconnected"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_HSIC_DISCONNECTED);
	else {
		sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);
	}
	sec_debug_save_context(NULL);

	/*show_state();*/
	dump_all_task_info();
	dump_cpu_stat();
	/* No backtrace */
	show_state_filter(TASK_STATE_MAX);

	sec_debug_hw_reset();
	return 0;
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = sec_debug_normal_reboot_handler,
};
static struct notifier_block nb_panic_block = {
	.notifier_call = sec_debug_panic_handler,
};

extern void sc8830_machine_restart(char mode, const char *cmd);
void sec_debug_emergency_restart_handler(void)
{

	if (!sec_debug_level.en.kernel_fault)
		return;

	/* W/A for watchdog reset in emergency_restart */
	arm_pm_restart = sc8830_machine_restart;	
	sec_debug_set_upload_magic(0x66262564);
	sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);
	sec_debug_save_context(NULL);
	sec_debug_hw_reset();

}

int sec_debug_save_context(void *core_regs)
{
	unsigned long flags;
	struct pt_regs *regs = (struct pt_regs *)core_regs;
	
	if (!sec_debug_level.en.kernel_fault)
		return -1;
	
	local_irq_save(flags);
	// we need check whether re-entering or not
	if(sec_debug_get_panic_sign(smp_processor_id()) != SEC_DEBUG_PANIC_SIGN_SET) {
		sec_debug_save_mmu_reg(&per_cpu(sec_debug_aarch64_mmu_reg,smp_processor_id()));
		sec_debug_save_core_reg(regs);
		sec_debug_dump_memory_error();
		sec_debug_set_panic_sign(smp_processor_id(), SEC_DEBUG_PANIC_SIGN_SET);
		pr_emerg("(%s) Context saved(CPU:%d)\n", __func__, smp_processor_id());
	} else {
		pr_emerg("(%s) Skip context saved(CPU:%d)\n", __func__, smp_processor_id());
	}
	flush_cache_all();
	local_irq_restore(flags);
	
	return 0;
}

__init int sec_debug_init(void)
{
	int i;

	if(!sec_debug_level.en.kernel_fault)
		return -1;
	sec_debug_set_upload_magic(0x66262564);
	sec_debug_set_upload_cause(UPLOAD_CAUSE_INIT);

	for(i=0; i<SEC_DEBUG_NR_CPU; i++)
		sec_debug_set_panic_sign(i,SEC_DEBUG_PANIC_SIGN_INIT);
	
	register_reboot_notifier(&nb_reboot_block);
	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	return 0;
}

device_initcall(sec_debug_init);
