/*
 *  sec_debug.c
 *
 */

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
#include <asm/system_misc.h>

#include <mach/sec_debug.h>
#include <mach/regs-mpmu.h>

#include <asm/mach/map.h>

#include <linux/seq_file.h>


// For reset with command
#define MPMU_ARSR_SWR_SHIFT	(8)
#define RESET_PANIC		(1 << MPMU_ARSR_SWR_SHIFT)
#define RESET_FORCE_UPLOAD		(2 << MPMU_ARSR_SWR_SHIFT)
#define RESET_SOMETHING		(2 << MPMU_ARSR_SWR_SHIFT)
#define MPMU_ARSR_SWR_MASK		(0xfffff0ff)


struct sec_debug_mmu_reg_t {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};

/* ARM CORE regs mapping structure */
struct sec_debug_core_t {
	/* COMMON */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;

	/* SVC */
	unsigned int r13_svc;
	unsigned int r14_svc;
	unsigned int spsr_svc;

	/* PC & CPSR */
	unsigned int pc;
	unsigned int cpsr;

	/* USR/SYS */
	unsigned int r13_usr;
	unsigned int r14_usr;

	/* FIQ */
	unsigned int r8_fiq;
	unsigned int r9_fiq;
	unsigned int r10_fiq;
	unsigned int r11_fiq;
	unsigned int r12_fiq;
	unsigned int r13_fiq;
	unsigned int r14_fiq;
	unsigned int spsr_fiq;

	/* IRQ */
	unsigned int r13_irq;
	unsigned int r14_irq;
	unsigned int spsr_irq;

	/* MON */
	unsigned int r13_mon;
	unsigned int r14_mon;
	unsigned int spsr_mon;

	/* ABT */
	unsigned int r13_abt;
	unsigned int r14_abt;
	unsigned int spsr_abt;

	/* UNDEF */
	unsigned int r13_und;
	unsigned int r14_und;
	unsigned int spsr_und;

};

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
#define SCHED_LOG_MAX 2048

struct sched_log {
	struct task_log {
		unsigned long long time;
		char comm[TASK_COMM_LEN];
		pid_t pid;
	} task[NR_CPUS][SCHED_LOG_MAX];
	struct irq_log {
		unsigned long long time;
		int irq;
		void *fn;
		int en;
	} irq[NR_CPUS][SCHED_LOG_MAX];
	struct work_log {
		unsigned long long time;
		struct worker *worker;
		struct work_struct *work;
		work_func_t f;
		int en;
	} work[NR_CPUS][SCHED_LOG_MAX];
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
	struct timer_log {
		unsigned long long time;
		unsigned int type;
		void *fn;
	} timer[NR_CPUS][SCHED_LOG_MAX];
#endif /* CONFIG_SEC_DEBUG_TIMER_LOG */
};
#endif				/* CONFIG_SEC_DEBUG_SCHED_LOG */

/* enable/disable sec_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
union sec_debug_level_t sec_debug_level = { .en.kernel_fault = 1, };
int sec_crash_key_panic = 0;

module_param_named(enable, sec_debug_level.en.kernel_fault, ushort, 0644);
module_param_named(enable_user, sec_debug_level.en.user_fault, ushort, 0644);
module_param_named(level, sec_debug_level.uint_val, uint, 0644);

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
static struct sched_log sec_debug_log __cacheline_aligned;
/*
static struct sched_log sec_debug_log[NR_CPUS][SCHED_LOG_MAX]
	__cacheline_aligned;
*/

#if NR_CPUS == 1
static atomic_t task_log_idx[NR_CPUS] = { ATOMIC_INIT(-1) };
static atomic_t irq_log_idx[NR_CPUS] = { ATOMIC_INIT(-1) };
static atomic_t work_log_idx[NR_CPUS] = { ATOMIC_INIT(-1) };
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
static atomic_t timer_log_idx[NR_CPUS] = { ATOMIC_INIT(-1) };
#endif /* CONFIG_SEC_DEBUG_TIMER_LOG */
#elif NR_CPUS == 2
static atomic_t task_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t irq_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t work_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
static atomic_t timer_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#endif /* CONFIG_SEC_DEBUG_TIMER_LOG */
#elif NR_CPUS == 4
static atomic_t task_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t irq_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t work_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
static atomic_t timer_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#endif /* CONFIG_SEC_DEBUG_TIMER_LOG */
#else
#error "Please check NR_CPUS"
#endif

static struct sched_log (*psec_debug_log) = (&sec_debug_log);

static int bStopLogging;

static int checksum_sched_log(void)
{
	int sum = 0, i;
	for (i = 0; i < sizeof(sec_debug_log); i++)
		sum += *((char *)&sec_debug_log + i);

	return sum;
}

int get_sec_debug_level(void)
{
	return sec_debug_level.uint_val;
}
#endif

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
void __sec_debug_task_log(int cpu, struct task_struct *task)
{
	unsigned i;

	if (bStopLogging)
		return;

	i = atomic_inc_return(&task_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->task[cpu][i].time = cpu_clock(cpu);
	strcpy(psec_debug_log->task[cpu][i].comm, task->comm);
	psec_debug_log->task[cpu][i].pid = task->pid;
}

void __sec_debug_irq_log(unsigned int irq, void *fn, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (bStopLogging)
		return;

	i = atomic_inc_return(&irq_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->irq[cpu][i].time = cpu_clock(cpu);
	psec_debug_log->irq[cpu][i].irq = irq;
	psec_debug_log->irq[cpu][i].fn = (void *)fn;
	psec_debug_log->irq[cpu][i].en = en;
}

void __sec_debug_work_log(struct worker *worker,
			  struct work_struct *work, work_func_t f, int en)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (bStopLogging)
		return;

	i = atomic_inc_return(&work_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->work[cpu][i].time = cpu_clock(cpu);
	psec_debug_log->work[cpu][i].worker = worker;
	psec_debug_log->work[cpu][i].work = work;
	psec_debug_log->work[cpu][i].f = f;
	psec_debug_log->work[cpu][i].en = en;
}

#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
void __sec_debug_timer_log(unsigned int type, void *fn)
{
	int cpu = raw_smp_processor_id();
	unsigned i;

	if (bStopLogging)
		return;

	i = atomic_inc_return(&timer_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->timer[cpu][i].time = cpu_clock(cpu);
	psec_debug_log->timer[cpu][i].type = type;
	psec_debug_log->timer[cpu][i].fn = fn;
}
#endif /* CONFIG_SEC_DEBUG_TIMER_LOG */

void sec_debug_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	static const unsigned int VOLUME_UP = KEY_VOLUMEUP;
	static const unsigned int VOLUME_DOWN = KEY_VOLUMEDOWN;

	if (!sec_debug_level.en.kernel_fault)
		return;

	pr_debug("%s: code is %d, value is %d\n",
				     __func__, code, value);

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
					bStopLogging = 1;
					sec_crash_key_panic = 1;
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

DEFINE_PER_CPU(struct sec_debug_core_t, sec_debug_core_reg);
DEFINE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_mmu_reg);

/* core reg dump function*/
static inline void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	/* we will be in SVC mode when we enter this function. Collect
	SVC registers along with cmn registers. */
	asm(
		"str r0, [%0,#0]\n\t"	/* R0 is pushed first to core_reg */
		"mov r0, %0\n\t"		/* R0 will be alias for core_reg */
		"str r1, [r0,#4]\n\t"	/* R1 */
		"str r2, [r0,#8]\n\t"	/* R2 */
		"str r3, [r0,#12]\n\t"	/* R3 */
		"str r4, [r0,#16]\n\t"	/* R4 */
		"str r5, [r0,#20]\n\t"	/* R5 */
		"str r6, [r0,#24]\n\t"	/* R6 */
		"str r7, [r0,#28]\n\t"	/* R7 */
		"str r8, [r0,#32]\n\t"	/* R8 */
		"str r9, [r0,#36]\n\t"	/* R9 */
		"str r10, [r0,#40]\n\t"	/* R10 */
		"str r11, [r0,#44]\n\t"	/* R11 */
		"str r12, [r0,#48]\n\t"	/* R12 */
		/* SVC */
		"str r13, [r0,#52]\n\t"	/* R13_SVC */
		"str r14, [r0,#56]\n\t"	/* R14_SVC */
		"mrs r1, spsr\n\t"		/* SPSR_SVC */
		"str r1, [r0,#60]\n\t"
		/* PC and CPSR */
		"sub r1, r15, #0x4\n\t"	/* PC */
		"str r1, [r0,#64]\n\t"
		"mrs r1, cpsr\n\t"		/* CPSR */
		"str r1, [r0,#68]\n\t"
		/* SYS/USR */
		"mrs r1, cpsr\n\t"		/* switch to SYS mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x1f\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#72]\n\t"	/* R13_USR */
		"str r14, [r0,#76]\n\t"	/* R14_USR */
		/* FIQ */
		"mrs r1, cpsr\n\t"		/* switch to FIQ mode */
		"and r1,r1,#0xFFFFFFE0\n\t"
		"orr r1,r1,#0x11\n\t"
		"msr cpsr,r1\n\t"
		"str r8, [r0,#80]\n\t"	/* R8_FIQ */
		"str r9, [r0,#84]\n\t"	/* R9_FIQ */
		"str r10, [r0,#88]\n\t"	/* R10_FIQ */
		"str r11, [r0,#92]\n\t"	/* R11_FIQ */
		"str r12, [r0,#96]\n\t"	/* R12_FIQ */
		"str r13, [r0,#100]\n\t"	/* R13_FIQ */
		"str r14, [r0,#104]\n\t"	/* R14_FIQ */
		"mrs r1, spsr\n\t"		/* SPSR_FIQ */
		"str r1, [r0,#108]\n\t"
		/* IRQ */
		"mrs r1, cpsr\n\t"		/* switch to IRQ mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x12\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#112]\n\t"	/* R13_IRQ */
		"str r14, [r0,#116]\n\t"	/* R14_IRQ */
		"mrs r1, spsr\n\t"		/* SPSR_IRQ */
		"str r1, [r0,#120]\n\t"
		/* MON */
		"mrs r1, cpsr\n\t"		/* switch to monitor mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x16\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#124]\n\t"	/* R13_MON */
		"str r14, [r0,#128]\n\t"	/* R14_MON */
		"mrs r1, spsr\n\t"		/* SPSR_MON */
		"str r1, [r0,#132]\n\t"
		/* ABT */
		"mrs r1, cpsr\n\t"		/* switch to Abort mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x17\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#136]\n\t"	/* R13_ABT */
		"str r14, [r0,#140]\n\t"	/* R14_ABT */
		"mrs r1, spsr\n\t"		/* SPSR_ABT */
		"str r1, [r0,#144]\n\t"
		/* UND */
		"mrs r1, cpsr\n\t"		/* switch to undef mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x1B\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#148]\n\t"	/* R13_UND */
		"str r14, [r0,#152]\n\t"	/* R14_UND */
		"mrs r1, spsr\n\t"		/* SPSR_UND */
		"str r1, [r0,#156]\n\t"
		/* restore to SVC mode */
		"mrs r1, cpsr\n\t"		/* switch to SVC mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x13\n\t"
		"msr cpsr,r1\n\t" :		/* output */
		: "r"(core_reg)		/* input */
		: "%r0", "%r1"		/* clobbered registers */
	);

	return;
}

static inline void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	asm(
		"mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
		"str r1, [%0]\n\t"
		"mrc    p15, 0, r1, c2, c0, 0\n\t"	/* TTBR0 */
		"str r1, [%0,#4]\n\t"
		"mrc    p15, 0, r1, c2, c0,1\n\t"	/* TTBR1 */
		"str r1, [%0,#8]\n\t"
		"mrc    p15, 0, r1, c2, c0,2\n\t"	/* TTBCR */
		"str r1, [%0,#12]\n\t"
		"mrc    p15, 0, r1, c3, c0,0\n\t"	/* DACR */
		"str r1, [%0,#16]\n\t"
		"mrc    p15, 0, r1, c5, c0,0\n\t"	/* DFSR */
		"str r1, [%0,#20]\n\t"
		"mrc    p15, 0, r1, c6, c0,0\n\t"	/* DFAR */
		"str r1, [%0,#24]\n\t"
		"mrc    p15, 0, r1, c5, c0,1\n\t"	/* IFSR */
		"str r1, [%0,#28]\n\t"
		"mrc    p15, 0, r1, c6, c0,2\n\t"	/* IFAR */
		"str r1, [%0,#32]\n\t"
		/* Don't populate DAFSR and RAFSR */
		"mrc    p15, 0, r1, c10, c2,0\n\t"	/* PMRRR */
		"str r1, [%0,#44]\n\t"
		"mrc    p15, 0, r1, c10, c2,1\n\t"	/* NMRRR */
		"str r1, [%0,#48]\n\t"
		"mrc    p15, 0, r1, c13, c0,0\n\t"	/* FCSEPID */
		"str r1, [%0,#52]\n\t"
		"mrc    p15, 0, r1, c13, c0,1\n\t"	/* CONTEXT */
		"str r1, [%0,#56]\n\t"
		"mrc    p15, 0, r1, c13, c0,2\n\t"	/* URWTPID */
		"str r1, [%0,#60]\n\t"
		"mrc    p15, 0, r1, c13, c0,3\n\t"	/* UROTPID */
		"str r1, [%0,#64]\n\t"
		"mrc    p15, 0, r1, c13, c0,4\n\t"	/* POTPIDR */
		"str r1, [%0,#68]\n\t" :		/* output */
		: "r"(mmu_reg)			/* input */
		: "%r1", "memory"			/* clobbered register */
	);
}

inline void sec_debug_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	sec_debug_save_mmu_reg(&per_cpu(sec_debug_mmu_reg, smp_processor_id()));
	sec_debug_save_core_reg(&per_cpu(sec_debug_core_reg, smp_processor_id()));

	pr_emerg("(%s) context saved(CPU:%d)\n", __func__, smp_processor_id());
	local_irq_restore(flags);
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
	int i, j;
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
	for_each_possible_cpu(i) {
		user = user + kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice = nice + kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system = system + kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle = idle + kcpustat_cpu(i).cpustat[CPUTIME_IDLE];
		idle = idle + arch_idle_time(i);
		iowait = iowait + kcpustat_cpu(i).cpustat[CPUTIME_IOWAIT];
		irq = irq + kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq = softirq + kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];

		for_each_irq_nr(j) {
			sum += kstat_irqs_cpu(j, i);
		}
		sum += arch_irq_stat_cpu(i);
		for (j = 0; j < NR_SOFTIRQS; j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);
			per_softirq_sums[j] += softirq_stat;
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
	for_each_online_cpu(i) {
		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user = kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice = kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system = kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle = kcpustat_cpu(i).cpustat[CPUTIME_IDLE];
		idle = idle + arch_idle_time(i);
		iowait = kcpustat_cpu(i).cpustat[CPUTIME_IOWAIT];
		irq = kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq = kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		/* steal = kstat_cpu(i).cpustat.steal; */
		/* guest = kstat_cpu(i).cpustat.guest; */
		/* guest_nice = kstat_cpu(i).cpustat.guest_nice; */
		pr_info(" cpu %d   user:%llu  nice:%llu  system:%llu  " \
			"idle:%llu  iowait:%llu  irq:%llu  softirq:%llu "
			"%llu %llu " "%llu\n",
			i,
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
	for_each_irq_nr(j) {
		per_irq_sum = 0;
		for_each_possible_cpu(i)
			per_irq_sum += kstat_irqs_cpu(j, i);
		if (per_irq_sum) {
			pr_info(" irq-%4d : %8u %s\n",
				j, per_irq_sum, irq_to_desc(j)->action ?
				irq_to_desc(j)->action->name ?: "???" : "???");
		}
	}
	pr_info(" -----------------------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info(" softirq : %llu", (unsigned long long)sum_softirq);
	pr_info(" -----------------------------------------------------------------------------------\n");
	for (i = 0; i < NR_SOFTIRQS; i++)
		if (per_softirq_sums[i])
			pr_info(" softirq-%d : %8u %s\n",
				i, per_softirq_sums[i], softirq_to_name[i]);
	pr_info(" -----------------------------------------------------------------------------------\n");
}

/*
 * Called from dump_stack()
 * This function call does not necessarily mean that a fatal error
 * had occurred. It may be just a warning.
 */
static inline int sec_debug_dump_stack(void)
{
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	sec_debug_save_context();

	/* flush L1 from each core.
	   L2 will be flushed later before reset. */
	flush_cache_all();

	return 0;
}

static inline void sec_debug_hw_reset(void *buf)
{
	pr_emerg("(%s) %s\n", __func__, linux_banner);
	pr_emerg("(%s) rebooting...\n", __func__);

	flush_cache_all();

	outer_flush_all();

	machine_restart(buf);

	while (1) {
		pr_err("should not be happend\n");
		pr_err("should not be happend\n");
	}
}

static int sec_debug_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	u32 arsr_reg;

	if (!sec_debug_level.en.kernel_fault)
		return -1;

	arsr_reg = readl(MPMU_ARSR);
	arsr_reg &= MPMU_ARSR_SWR_MASK; // masking SWR

	if (!strcmp(buf, "Crash Key"))
		writel(arsr_reg | RESET_FORCE_UPLOAD, MPMU_ARSR) ; // Intended panic for force upload.
	else
		writel(arsr_reg | RESET_PANIC, MPMU_ARSR) ; // Real Panic

	pr_err("(%s) checksum_sched_log: %x\n", __func__, checksum_sched_log());

    	dump_all_task_info();
	dump_cpu_stat();

	show_state_filter(TASK_STATE_MAX);	/* no backtrace */

	sec_debug_dump_stack();

#ifndef CONFIG_PXA_PANIC_FLUSH
	sec_debug_hw_reset(buf);
#endif

	return 0;
}

int sec_debug_panic_dump(char *buf)
{
	return sec_debug_panic_handler(NULL, 0, buf);
}

static struct notifier_block nb_panic_block = {
	.notifier_call = sec_debug_panic_handler,
};

int __init sec_debug_init(void)
{
	if (!sec_debug_level.en.kernel_fault)
		return -1;
	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	return 0;
}


#endif /* CONFIG_SEC_DEBUG_SCHED_LOG */
