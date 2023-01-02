/*
 * arch/arm/mach-sc/sec-debug.c
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
#include <linux/dma-mapping.h>
#include <mach/hardware.h>
#include <mach/system.h>
#include <mach/sec_debug.h>

#include <linux/seq_file.h>

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
#define SCHED_LOG_MAX 2048

struct sched_log {
	struct task_log {
	unsigned long long time;
			char comm[TASK_COMM_LEN];
			pid_t pid;
	} task[CONFIG_NR_CPUS][SCHED_LOG_MAX];
		struct irq_log {
		unsigned long long time;
			int irq;
			void *fn;
			int en;
	} irq[CONFIG_NR_CPUS][SCHED_LOG_MAX];
		struct work_log {
		unsigned long long time;
			struct worker *worker;
			struct work_struct *work;
			work_func_t f;
			int en;
	} work[CONFIG_NR_CPUS][SCHED_LOG_MAX];
	
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
	struct timer_log {
		unsigned long long time;
		unsigned int type;
		void *fn;
	} timer[CONFIG_NR_CPUS][SCHED_LOG_MAX];
#endif /* CONFIG_SEC_DEBUG_TIMER_LOG */
};
#endif				/* CONFIG_SEC_DEBUG_SCHED_LOG */



#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
#define AUX_LOG_CPU_CLOCK_MAX 64
#define AUX_LOG_LOGBUF_LOCK_MAX 64
#define AUX_LOG_DVFS_LOCK_MAX 64
#define AUX_LOG_LENGTH 128

struct auxiliary_info {
	unsigned long long time;
	int cpu;
	char log[AUX_LOG_LENGTH];
};

/* This structure will be modified if some other items added for log */
struct auxiliary_log {
	struct auxiliary_info CpuClockLog[AUX_LOG_CPU_CLOCK_MAX];
	struct auxiliary_info LogBufLockLog[AUX_LOG_LOGBUF_LOCK_MAX];
	struct auxiliary_info DVFSLockLog[AUX_LOG_DVFS_LOCK_MAX];
};

#else
#endif

#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
#define SEMAPHORE_LOG_MAX 100
struct sem_debug {
	struct list_head list;
	struct semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	/* char comm[TASK_COMM_LEN]; */
};

enum {
	READ_SEM,
	WRITE_SEM
};

#define RWSEMAPHORE_LOG_MAX 100
struct rwsem_debug {
	struct list_head list;
	struct rw_semaphore *sem;
	struct task_struct *task;
	pid_t pid;
	int cpu;
	int direction;
	/* char comm[TASK_COMM_LEN]; */
};

#endif	/* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */
#define RESET_REASON_NORMAL		0x1A2B3C00
#define RESET_REASON_SMPL			0x1A2B3C01
#define RESET_REASON_WSTR			0x1A2B3C02
#define RESET_REASON_WATCHDOG	0x1A2B3C03
#define RESET_REASON_PANIC			0x1A2B3C04
#define RESET_REASON_LPM			0x1A2B3C10
#define RESET_REASON_RECOVERY		0x1A2B3C11
#define RESET_REASON_FOTA			0x1A2B3C12

enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT = 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC = 0x000000C8,
	UPLOAD_CAUSE_FORCED_UPLOAD = 0x00000022,
	UPLOAD_CAUSE_CP_ERROR_FATAL = 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT = 0x0000002F,
	UPLOAD_CAUSE_HSIC_DISCONNECTED = 0x000000DD,
};

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

struct sec_debug_fault_status_t {
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
	unsigned int r13;
	unsigned int r14;
	unsigned int r15;
	unsigned int cpsr;
	unsigned int cur_process_magic;
};

static unsigned reset_reason = 0xFFEEFFEE;

module_param_named(reset_reason, reset_reason, uint, 0644);

static const char * const gkernel_sec_build_info_date_time[] = {
	__DATE__,
	__TIME__
};

/* enable/disable sec_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
union sec_debug_level_t sec_debug_level = { .en.kernel_fault = 1, };

module_param_named(enable, sec_debug_level.en.kernel_fault, ushort, 0644);
module_param_named(enable_user, sec_debug_level.en.user_fault, ushort, 0644);
module_param_named(level, sec_debug_level.uint_val, uint, 0644);

static char gkernel_sec_build_info[100];

#ifdef CONFIG_SEC_DEBUG_REG_ACCESS
struct sec_debug_regs_access *sec_debug_last_regs_access=NULL;
unsigned char *sec_debug_local_hwlocks_status=NULL;
EXPORT_SYMBOL(sec_debug_last_regs_access);
EXPORT_SYMBOL(sec_debug_local_hwlocks_status);
#endif

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG_NONCACHED
static struct sched_log sec_debug_log;
#else
static struct sched_log sec_debug_log __cacheline_aligned;
#endif
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
static atomic_t task_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t irq_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
static atomic_t work_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
static atomic_t timer_log_idx[NR_CPUS] = { ATOMIC_INIT(-1), ATOMIC_INIT(-1),
					ATOMIC_INIT(-1), ATOMIC_INIT(-1) };
#endif /* CONFIG_SEC_DEBUG_TIMER_LOG */
#else
#error "Please check NR_CPUS"
#endif
static struct sched_log (*psec_debug_log) = (&sec_debug_log);
/*
static struct sched_log (*psec_debug_log)[NR_CPUS][SCHED_LOG_MAX]
	= (&sec_debug_log);
*/
#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
static unsigned long long gExcpIrqExitTime[NR_CPUS];
#endif

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
static struct auxiliary_log gExcpAuxLog	__cacheline_aligned;
static struct auxiliary_log *gExcpAuxLogPtr;
static atomic_t gExcpAuxCpuClockLogIdx = ATOMIC_INIT(-1);
static atomic_t gExcpAuxLogBufLockLogIdx = ATOMIC_INIT(-1);
static atomic_t gExcpAuxDVFSLockLogIdx = ATOMIC_INIT(-1);
#endif

static int bStopLogging;

static int checksum_sched_log(void)
{
	int sum = 0, i;
	for (i = 0; i < sizeof(sec_debug_log); i++)
		sum += *((char *)&sec_debug_log + i);

	return sum;
}

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG_NONCACHED
phys_addr_t phy_sec_debug_log_struct = 0x86A00000;
/* we should meet to align to 1M regarding for  size */
size_t phy_sec_debug_log_struct_size = 0x100000;
static void map_noncached_sched_log_buf(void)
{
	void __iomem *base=0;

	base = ioremap_nocache(phy_sec_debug_log_struct ,phy_sec_debug_log_struct_size );
	psec_debug_log = base;
}
#endif

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
static void map_noncached_aux_log_buf(void)
{
	struct map_desc auxlog_buf_iodesc[] = {
		{
			.virtual = (unsigned long)S3C_VA_AUXLOG_BUF,
			.length = 0x200000,
			.type = MT_DEVICE
		}
	};

	auxlog_buf_iodesc[0].pfn = __phys_to_pfn
		((unsigned long)((virt_to_phys(&gExcpAuxLog)&0xfff00000)));
	iotable_init(auxlog_buf_iodesc, ARRAY_SIZE(auxlog_buf_iodesc));
	gExcpAuxLogPtr = (void *)(S3C_VA_AUXLOG_BUF +
		(((unsigned long)(&gExcpAuxLog))&0x000fffff));
}
#endif

#else
static int checksum_sched_log(void)
{
	return 0;
}
#endif

/* klaatu - semaphore log */
#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
struct sem_debug sem_debug_free_head;
struct sem_debug sem_debug_done_head;
int sem_debug_free_head_cnt;
int sem_debug_done_head_cnt;
int sem_debug_init = 0;
spinlock_t sem_debug_lock;

/* rwsemaphore logging */
struct rwsem_debug rwsem_debug_free_head;
struct rwsem_debug rwsem_debug_done_head;
int rwsem_debug_free_head_cnt;
int rwsem_debug_done_head_cnt;
int rwsem_debug_init = 0;
spinlock_t rwsem_debug_lock;

#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

DEFINE_PER_CPU(struct sec_debug_core_t, sec_debug_core_reg);
DEFINE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_mmu_reg);
DEFINE_PER_CPU(enum sec_debug_upload_cause_t, sec_debug_upload_cause);
DEFINE_PER_CPU(struct sec_debug_fault_status_t, sec_debug_fault_status);
#define CALL_STACK_WORKAROUND 
#ifdef CALL_STACK_WORKAROUND
void sec_debug_backup_ctx(struct pt_regs *regs)
{
	per_cpu(sec_debug_core_reg,smp_processor_id()).r0 = regs->uregs[0];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r1 = regs->uregs[1];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r2 = regs->uregs[2];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r3 = regs->uregs[3];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r4 = regs->uregs[4];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r5 = regs->uregs[5];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r6 = regs->uregs[6];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r7 = regs->uregs[7];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r8 = regs->uregs[8];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r9 = regs->uregs[9];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r10 = regs->uregs[10];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r11 = regs->uregs[11];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r12 = regs->uregs[12];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r13_svc = regs->uregs[13];
	per_cpu(sec_debug_core_reg,smp_processor_id()).r14_svc = regs->uregs[14];
	per_cpu(sec_debug_core_reg,smp_processor_id()).pc = regs->uregs[15];
	per_cpu(sec_debug_core_reg,smp_processor_id()).spsr_svc = regs->uregs[16];
}
#else
void sec_debug_backup_ctx(struct pt_regs *regs) {}
#endif

static unsigned int forced_upload_flag = 0;
unsigned int sec_debug_callstack_workaround = 0;
void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	/* we will be in SVC mode when we enter this function. Collect
	   SVC registers along with cmn registers. */
	if(sec_debug_callstack_workaround == 0x12345678){
	asm("mrs r1, cpsr\n\t"		/* CPSR */
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
	} else {
		asm("str r0, [%0,#0]\n\t"	/* R0 is pushed first to core_reg */
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
	}

	return;
}

void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
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

void sec_debug_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	sec_debug_save_mmu_reg(&per_cpu(sec_debug_mmu_reg, smp_processor_id()));
	sec_debug_save_core_reg(&per_cpu
				(sec_debug_core_reg, smp_processor_id()));

	pr_emerg("(%s) context saved(CPU:%d)\n", __func__, smp_processor_id());
	local_irq_restore(flags);
}

void sec_debug_save_pte(void *pte, int task_addr)
{
	unsigned long flags;
	local_irq_save(flags);
	memcpy(&per_cpu(sec_debug_fault_status,
				smp_processor_id()), pte,
				sizeof(struct sec_debug_fault_status_t));
	per_cpu(sec_debug_fault_status,
			smp_processor_id()).cur_process_magic = task_addr;
	local_irq_restore(flags);
}

static void sec_debug_set_upload_magic(unsigned magic)
{
	pr_emerg("(%s) %x\n", __func__, magic);

	__raw_writel(magic, (void __iomem *)SEC_DEBUG_MAGIC_VA);

	flush_cache_all();

	outer_flush_all();
}

static int sec_debug_normal_reboot_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	sec_debug_set_upload_magic(0x0);

	return 0;
}

static void sec_debug_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	per_cpu(sec_debug_upload_cause, smp_processor_id()) = type;

	/* to check VDD_ALIVE / XnRESET issue */
	__raw_writel(type, (void __iomem *)SPRD_INFORM3);
	__raw_writel(type, (void __iomem *)SPRD_INFORM4);
	__raw_writel(type, (void __iomem *)SPRD_INFORM6);

	pr_emerg("(%s) %x\n", __func__, type);
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


/*
 * Called from dump_stack()
 * This function call does not necessarily mean that a fatal error
 * had occurred. It may be just a warning.
 */
int sec_debug_dump_stack(void)
{
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	sec_debug_save_context();

	/* flush L1 from each core.
	   L2 will be flushed later before reset. */
	flush_cache_all();

	return 0;
}

void sec_debug_hw_reset(void)
{
	pr_emerg("(%s) rebooting...\n", __func__);

	flush_cache_all();

	outer_flush_all();
	machine_restart(NULL);

}

extern void sec_debug_print_gptimer_reg();

static int sec_debug_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	local_irq_disable();

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
	bStopLogging = 1;
#endif

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
#ifdef CALL_STACK_WORKAROUND
		if (forced_upload_flag == 0x98765432)
			sec_debug_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
		else
#endif
			sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);
	}

	pr_err("(%s) checksum_sched_log: %x\n", __func__, checksum_sched_log());
	sec_debug_print_gptimer_reg();
	/*show_state();*/
	dump_all_task_info();
	dump_cpu_stat();
	/* No backtrace */
	show_state_filter(TASK_STATE_MAX);

	sec_debug_dump_stack();
	sec_debug_hw_reset();


	return 0;
}

#ifdef CONFIG_SEC_DEBUG_FUPLOAD_DUMP_MORE
static void dump_state_and_upload(void);
#endif

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
#ifdef CONFIG_CORE_01A_KEY_CRASH
		if ( voldown_p) {
#else
		if (!volup_p && voldown_p) {
#endif	
			if (code == KEY_POWER) {
				pr_info
				    ("%s: count for enter forced upload : %d\n",
				     __func__, ++loopcount);
				if (loopcount == 2) {
#ifdef CONFIG_SEC_DEBUG_FUPLOAD_DUMP_MORE
					dump_state_and_upload();
#else
#ifdef CALL_STACK_WORKAROUND
					forced_upload_flag = 0x98765432;
					BUG();
#else
					panic("Crash Key");
#endif
#endif
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

static struct notifier_block nb_reboot_block = {
	.notifier_call = sec_debug_normal_reboot_handler
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
	sec_debug_dump_stack();
	sec_debug_hw_reset();

}

static void sec_debug_set_build_info(void)
{
	char *p = gkernel_sec_build_info;
	sprintf(p, "Kernel Build Info : ");
	strcat(p, " Date:");
	strncat(p, gkernel_sec_build_info_date_time[0], 12);
	strcat(p, " Time:");
	strncat(p, gkernel_sec_build_info_date_time[1], 9);
}

__init int sec_debug_init(void)
{
	unsigned int addr,haddr;
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	sec_debug_set_build_info();

	sec_debug_set_upload_magic(0x66262564);
	sec_debug_set_upload_cause(UPLOAD_CAUSE_INIT);

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG_NONCACHED
	map_noncached_sched_log_buf();
#endif

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
	map_noncached_aux_log_buf();
#endif

	register_reboot_notifier(&nb_reboot_block);

	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	return 0;
}

device_initcall(sec_debug_init);

int get_sec_debug_level(void)
{
	return sec_debug_level.uint_val;
}

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

#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
void sec_debug_irq_last_exit_log(void)
{
	int cpu = raw_smp_processor_id();
	gExcpIrqExitTime[cpu] = cpu_clock(cpu);
}
#endif

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
#endif /* CONFIG_SEC_DEBUG_SCHED_LOG */

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
void sec_debug_aux_log(int idx, char *fmt, ...)
{
	va_list args;
	char buf[128];
	unsigned i;
	int cpu = raw_smp_processor_id();

	if (!gExcpAuxLogPtr)
		return;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	switch (idx) {
	case SEC_DEBUG_AUXLOG_CPU_BUS_CLOCK_CHANGE:
		i = atomic_inc_return(&gExcpAuxCpuClockLogIdx)
			& (AUX_LOG_CPU_CLOCK_MAX - 1);
		(*gExcpAuxLogPtr).CpuClockLog[i].time = cpu_clock(cpu);
		(*gExcpAuxLogPtr).CpuClockLog[i].cpu = cpu;
		strncpy((*gExcpAuxLogPtr).CpuClockLog[i].log,
			buf, AUX_LOG_LENGTH);
		break;
	case SEC_DEBUG_AUXLOG_LOGBUF_LOCK_CHANGE:
		i = atomic_inc_return(&gExcpAuxLogBufLockLogIdx)
			& (AUX_LOG_LOGBUF_LOCK_MAX - 1);
		(*gExcpAuxLogPtr).LogBufLockLog[i].time = cpu_clock(cpu);
		(*gExcpAuxLogPtr).LogBufLockLog[i].cpu = cpu;
		strncpy((*gExcpAuxLogPtr).LogBufLockLog[i].log,
			buf, AUX_LOG_LENGTH);
		break;
	case SEC_DEBUG_AUXLOG_DVFS_LOCK_CHANGE:
		i = atomic_inc_return(&gExcpAuxDVFSLockLogIdx)
			& (AUX_LOG_DVFS_LOCK_MAX - 1);
		(*gExcpAuxLogPtr).DVFSLockLog[i].time = cpu_clock(cpu);
		(*gExcpAuxLogPtr).DVFSLockLog[i].cpu = cpu;
		strncpy((*gExcpAuxLogPtr).DVFSLockLog[i].log,
			buf, AUX_LOG_LENGTH);
		break;
	default:
		break;
	}
}
#endif

/* klaatu - semaphore log */
#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
void debug_semaphore_init(void)
{
	int i = 0;
	struct sem_debug *sem_debug = NULL;

	spin_lock_init(&sem_debug_lock);
	sem_debug_free_head_cnt = 0;
	sem_debug_done_head_cnt = 0;

	/* initialize list head of sem_debug */
	INIT_LIST_HEAD(&sem_debug_free_head.list);
	INIT_LIST_HEAD(&sem_debug_done_head.list);

	for (i = 0; i < SEMAPHORE_LOG_MAX; i++) {
		/* malloc semaphore */
		sem_debug = kmalloc(sizeof(struct sem_debug), GFP_KERNEL);
		/* add list */
		list_add(&sem_debug->list, &sem_debug_free_head.list);
		sem_debug_free_head_cnt++;
	}

	sem_debug_init = 1;
}

void debug_semaphore_down_log(struct semaphore *sem)
{
	struct list_head *tmp;
	struct sem_debug *sem_dbg;
	unsigned long flags;

	if (!sem_debug_init)
		return;

	spin_lock_irqsave(&sem_debug_lock, flags);
	list_for_each(tmp, &sem_debug_free_head.list) {
		sem_dbg = list_entry(tmp, struct sem_debug, list);
		sem_dbg->task = current;
		sem_dbg->sem = sem;
		/* strcpy(sem_dbg->comm,current->group_leader->comm); */
		sem_dbg->pid = current->pid;
		sem_dbg->cpu = smp_processor_id();
		list_del(&sem_dbg->list);
		list_add(&sem_dbg->list, &sem_debug_done_head.list);
		sem_debug_free_head_cnt--;
		sem_debug_done_head_cnt++;
		break;
	}
	spin_unlock_irqrestore(&sem_debug_lock, flags);
}

void debug_semaphore_up_log(struct semaphore *sem)
{
	struct list_head *tmp;
	struct sem_debug *sem_dbg;
	unsigned long flags;

	if (!sem_debug_init)
		return;

	spin_lock_irqsave(&sem_debug_lock, flags);
	list_for_each(tmp, &sem_debug_done_head.list) {
		sem_dbg = list_entry(tmp, struct sem_debug, list);
		if (sem_dbg->sem == sem && sem_dbg->pid == current->pid) {
			list_del(&sem_dbg->list);
			list_add(&sem_dbg->list, &sem_debug_free_head.list);
			sem_debug_free_head_cnt++;
			sem_debug_done_head_cnt--;
			break;
		}
	}
	spin_unlock_irqrestore(&sem_debug_lock, flags);
}

/* rwsemaphore logging */
void debug_rwsemaphore_init(void)
{
	int i = 0;
	struct rwsem_debug *rwsem_debug = NULL;

	spin_lock_init(&rwsem_debug_lock);
	rwsem_debug_free_head_cnt = 0;
	rwsem_debug_done_head_cnt = 0;

	/* initialize list head of sem_debug */
	INIT_LIST_HEAD(&rwsem_debug_free_head.list);
	INIT_LIST_HEAD(&rwsem_debug_done_head.list);

	for (i = 0; i < RWSEMAPHORE_LOG_MAX; i++) {
		/* malloc semaphore */
		rwsem_debug = kmalloc(sizeof(struct rwsem_debug), GFP_KERNEL);
		/* add list */
		list_add(&rwsem_debug->list, &rwsem_debug_free_head.list);
		rwsem_debug_free_head_cnt++;
	}

	rwsem_debug_init = 1;
}

void debug_rwsemaphore_down_log(struct rw_semaphore *sem, int dir)
{
	struct list_head *tmp;
	struct rwsem_debug *sem_dbg;
	unsigned long flags;

	if (!rwsem_debug_init)
		return;

	spin_lock_irqsave(&rwsem_debug_lock, flags);
	list_for_each(tmp, &rwsem_debug_free_head.list) {
		sem_dbg = list_entry(tmp, struct rwsem_debug, list);
		sem_dbg->task = current;
		sem_dbg->sem = sem;
		/* strcpy(sem_dbg->comm,current->group_leader->comm); */
		sem_dbg->pid = current->pid;
		sem_dbg->cpu = smp_processor_id();
		sem_dbg->direction = dir;
		list_del(&sem_dbg->list);
		list_add(&sem_dbg->list, &rwsem_debug_done_head.list);
		rwsem_debug_free_head_cnt--;
		rwsem_debug_done_head_cnt++;
		break;
	}
	spin_unlock_irqrestore(&rwsem_debug_lock, flags);
}

void debug_rwsemaphore_up_log(struct rw_semaphore *sem)
{
	struct list_head *tmp;
	struct rwsem_debug *sem_dbg;
	unsigned long flags;

	if (!rwsem_debug_init)
		return;

	spin_lock_irqsave(&rwsem_debug_lock, flags);
	list_for_each(tmp, &rwsem_debug_done_head.list) {
		sem_dbg = list_entry(tmp, struct rwsem_debug, list);
		if (sem_dbg->sem == sem && sem_dbg->pid == current->pid) {
			list_del(&sem_dbg->list);
			list_add(&sem_dbg->list, &rwsem_debug_free_head.list);
			rwsem_debug_free_head_cnt++;
			rwsem_debug_done_head_cnt--;
			break;
		}
	}
	spin_unlock_irqrestore(&rwsem_debug_lock, flags);
}
#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

#ifdef CONFIG_SEC_DEBUG_USER
void sec_user_fault_dump(void)
{
	if (sec_debug_level.en.kernel_fault == 1
	    && sec_debug_level.en.user_fault == 1)
		panic("User Fault");
}

static int sec_user_fault_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *offs)
{
	char buf[100];

	if (count > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	if (strncmp(buf, "dump_user_fault", 15) == 0)
		sec_user_fault_dump();

	if (strncmp(buf, "Intentional_reboot", 18) == 0)
			kernel_restart(NULL);
	return count;
}

static const struct file_operations sec_user_fault_proc_fops = {
	.write = sec_user_fault_write,
};

char forced_ramdump_enable=0;
static int sec_debug_set_debug_level(char *str)
{
	if (!str)
		return -EINVAL;
	if (strcmp(str, "HIGH") == 0)
	{
		forced_ramdump_enable = 1;

	}
	else if (strcmp(str, "MID") == 0)
	{
		forced_ramdump_enable = 1;
	}
	else 
	{
		forced_ramdump_enable = 0;
	}
	return 0;
}
early_param("DEBUG_LEVEL", sec_debug_set_debug_level);

static int __init sec_debug_user_fault_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("user_fault", S_IWUGO, NULL,
			    &sec_user_fault_proc_fops);
	if (!entry)
		return -ENOMEM;
	return 0;
}

device_initcall(sec_debug_user_fault_init);
#endif  /* CONFIG_SEC_DEBUG_USER */
/* Reset Reason given from Bootloader */
static int set_reset_reason_proc_show(struct seq_file *m, void *v)
{
	/*
		RESET_REASON_NORMAL
		RESET_REASON_SMPL
		RESET_REASON_WSTR
		RESET_REASON_WATCHDOG
		RESET_REASON_PANIC
		RESET_REASON_LPM
		RESET_REASON_RECOVERY
		RESET_REASON_FOTA
	*/
	switch (reset_reason) {
	case RESET_REASON_NORMAL:
		seq_printf(m, "PON_NORMAL\n");
		break;
	case RESET_REASON_SMPL:
		seq_printf(m, "PON_SMPL\n");
		break;
	case RESET_REASON_WSTR:
	case RESET_REASON_WATCHDOG:
	case RESET_REASON_PANIC:
		seq_printf(m, "PON_PANIC\n");
		break;
	case RESET_REASON_LPM:
	case RESET_REASON_RECOVERY:
	case RESET_REASON_FOTA:
		seq_printf(m, "PON_NORMAL\n");
		break;
	default:
		seq_printf(m, "PON_NORMAL\n");
	}

	return 0;
}

static int sec_reset_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_reset_reason_proc_show, NULL);
}

static const struct file_operations sec_reset_reason_proc_fops = {
	.open		= sec_reset_reason_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init sec_debug_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason", S_IRUGO, NULL,
			    &sec_reset_reason_proc_fops);

	if (!entry)
		return -ENOMEM;

	return 0;
}

device_initcall(sec_debug_reset_reason_init);
