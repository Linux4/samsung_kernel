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
#include <mach/regs-pmu.h>
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
#include <linux/module.h>
#include <linux/platform_device.h>

#include <mach/regs-clock.h>
#include <plat/map-base.h>
#include <plat/map-s5p.h>
#include <asm/mach/map.h>
#include <plat/regs-watchdog.h>
#include <linux/seq_file.h>
#include <linux/sec_debug.h>
#include <linux/of.h>
#include <linux/fdtable.h>
#include <linux/mount.h>
#include <linux/memblock.h>

extern void exynos_pmu_debug_save(void);
extern void exynos_cmu_debug_save(void);
extern void (*mach_restart)(char str, const char *cmd);

#if defined(CONFIG_SEC_DEBUG) && defined(CONFIG_SEC_DEBUG_SUBSYS)
struct sec_debug_subsys *subsys_info=0;
static char *sec_subsys_log_buf;
static unsigned sec_subsys_log_size;
static unsigned int reserved_out_buf=0;
static unsigned int reserved_out_size=0;
#endif

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
#define SCHED_LOG_MAX 1024

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
static atomic_t task_log_idx[NR_CPUS];
static atomic_t irq_log_idx[NR_CPUS];
static atomic_t work_log_idx[NR_CPUS];
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
static atomic_t timer_log_idx[NR_CPUS];
#endif /* CONFIG_SEC_DEBUG_TIMER_LOG */
#endif				/* CONFIG_SEC_DEBUG_SCHED_LOG */

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
#define AUX_LOG_CPU_CLOCK_SWITCH_MAX 128
#define AUX_LOG_THERMAL_MAX 128
#ifdef CONFIG_SEC_DEBUG_RT_THROTTLE_ACTIVE
#define AUX_LOG_IRQ_MAX 128
#endif
#define AUX_LOG_WIFI_MAX 256
#ifdef CONFIG_SEC_DEBUG_AUX_SUSPEND
#define AUX_LOG_SUSPEND_MAX 128
#endif

#define AUX_LOG_LENGTH 128

struct auxiliary_info {
	unsigned long long time;
	int cpu;
	char log[AUX_LOG_LENGTH];
};

/* This structure will be modified if some other items added for log */
struct auxiliary_log {
	struct auxiliary_info CpuClockSwitchLog[AUX_LOG_CPU_CLOCK_SWITCH_MAX];
	struct auxiliary_info ThermalLog[AUX_LOG_THERMAL_MAX];
#ifdef CONFIG_SEC_DEBUG_RT_THROTTLE_ACTIVE
	struct auxiliary_info IrqLog[AUX_LOG_IRQ_MAX];
#endif
	struct auxiliary_info WifiLog[AUX_LOG_WIFI_MAX];
#ifdef CONFIG_SEC_DEBUG_AUX_SUSPEND
	struct auxiliary_info SuspendLog[AUX_LOG_SUSPEND_MAX];
#endif
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

/* layout of SDRAM
	   0: magic (4B)
      4~1023: panic string (1020B)
 1024~0x1000: panic dumper log
      0x4000: copy of magic
 */
#if defined(CONFIG_SPARSEMEM)
#define SEC_DEBUG_MAGIC_PA (S5P_PA_SDRAM)
#else
#define SEC_DEBUG_MAGIC_PA S5P_PA_SDRAM
#endif
#define SEC_DEBUG_MAGIC_VA phys_to_virt(SEC_DEBUG_MAGIC_PA)

enum sec_debug_reset_reason_t {
	RR_S = 1,
	RR_W = 2,
	RR_D = 3,
	RR_K = 4,
	RR_M = 5,
	RR_P = 6,
	RR_R = 7,
	RR_B = 8,
	RR_N = 9,
};

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

#if defined(CONFIG_SEC_DEBUG_CHECK_TASKPTR_FAULT)
static const char *processor_modes[] = {
	"USER_26", "FIQ_26" , "IRQ_26" , "SVC_26" , "UK4_26" , "UK5_26" , "UK6_26" , "UK7_26" ,
	"UK8_26" , "UK9_26" , "UK10_26", "UK11_26", "UK12_26", "UK13_26", "UK14_26", "UK15_26",
	"USER_32", "FIQ_32" , "IRQ_32" , "SVC_32" , "UK4_32" , "UK5_32" , "UK6_32" , "ABT_32" ,
	"UK8_32" , "UK9_32" , "UK10_32", "UND_32" , "UK12_32", "UK13_32", "UK14_32", "SYS_32"
};

static const char *isa_modes[] = {
	"ARM" , "Thumb" , "Jazelle", "ThumbEE"
};
#endif

#ifdef CONFIG_SEC_DEBUG_INPUT_HANDLER_CRASH
struct input_debug_drv_data {
	struct input_handler input_handler;
	struct input_debug_pdata *pdata;
	struct input_device_id input_ids[2];
	int crash_key_cnt;
	kernel_ulong_t keybit[INPUT_DEVICE_ID_KEY_MAX / BITS_PER_LONG + 1];
};
#endif /* CONFIG_SEC_DEBUG_INPUT_HANDLER_CRASH */

/* enable/disable sec_debug feature
 * level = 0 when enable = 0 && enable_user = 0
 * level = 1 when enable = 1 && enable_user = 0
 * level = 0x10001 when enable = 1 && enable_user = 1
 * The other cases are not considered
 */
union sec_debug_level_t sec_debug_level = { .en.kernel_fault = 1, };

static unsigned reset_reason = RR_N;
module_param_named(reset_reason, reset_reason, uint, 0644);

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
static atomic_t gExcpAuxCpuClockSwitchLogIdx = ATOMIC_INIT(-1);
static atomic_t gExcpAuxThermalLogIdx = ATOMIC_INIT(-1);
#ifdef CONFIG_SEC_DEBUG_RT_THROTTLE_ACTIVE
static atomic_t gExcpAuxIrqLogIdx = ATOMIC_INIT(-1);
#endif
static atomic_t gExcpAuxWifiLogIdx = ATOMIC_INIT(-1);
#endif
#ifdef CONFIG_SEC_DEBUG_AUX_SUSPEND
static atomic_t gExcpAuxSuspendLogIdx = ATOMIC_INIT(-1);
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
static void map_noncached_sched_log_buf(void)
{
	struct map_desc slog_buf_iodesc[] = {
		{
			.virtual = (unsigned long)S3C_VA_SLOG_BUF,
			.length = 0x200000,
			.type = MT_DEVICE
		}
	};

	slog_buf_iodesc[0].pfn = __phys_to_pfn
		((unsigned long)((virt_to_phys(&sec_debug_log)&0xfff00000)));
	iotable_init(slog_buf_iodesc, ARRAY_SIZE(slog_buf_iodesc));
	psec_debug_log = (void *)(S3C_VA_SLOG_BUF +
		(((unsigned long)(&sec_debug_log))&0x000fffff));
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
int sem_debug_init;
spinlock_t sem_debug_lock;

/* rwsemaphore logging */
struct rwsem_debug rwsem_debug_free_head;
struct rwsem_debug rwsem_debug_done_head;
int rwsem_debug_free_head_cnt;
int rwsem_debug_done_head_cnt;
int rwsem_debug_init;
spinlock_t rwsem_debug_lock;

#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

DEFINE_PER_CPU(struct sec_debug_core_t, sec_debug_core_reg);
DEFINE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_mmu_reg);
DEFINE_PER_CPU(enum sec_debug_upload_cause_t, sec_debug_upload_cause);

/* core reg dump function*/
static inline void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	/* we will be in SVC mode when we enter this function. Collect
	   SVC registers along with cmn registers. */
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
#if !defined(CONFIG_SOC_EXYNOS5433)
	    "mrs r1, cpsr\n\t"		/* switch to monitor mode */
	    "and r1, r1, #0xFFFFFFE0\n\t"
	    "orr r1, r1, #0x16\n\t"
	    "msr cpsr,r1\n\t"
	    "str r13, [r0,#124]\n\t"	/* R13_MON */
	    "str r14, [r0,#128]\n\t"	/* R14_MON */
	    "mrs r1, spsr\n\t"		/* SPSR_MON */
	    "str r1, [r0,#132]\n\t"
#endif
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

inline void sec_debug_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	sec_debug_save_mmu_reg(&per_cpu(sec_debug_mmu_reg, smp_processor_id()));
	sec_debug_save_core_reg(&per_cpu
				(sec_debug_core_reg, smp_processor_id()));

	pr_emerg("(%s) context saved(CPU:%d)\n", __func__, smp_processor_id());
	local_irq_restore(flags);
}

static void sec_debug_set_upload_magic(unsigned magic, char *str)
{
	pr_emerg("(%s) %x\n", __func__, magic);

	*(unsigned int *)SEC_DEBUG_MAGIC_VA = magic;
	*(unsigned int *)(SEC_DEBUG_MAGIC_VA + SZ_4K -4) = magic;

	if (str)
		strncpy((char *)SEC_DEBUG_MAGIC_VA + 4, str, SZ_1K - 4);

	flush_cache_all();

	outer_flush_all();
}

static int sec_debug_normal_reboot_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	sec_debug_set_upload_magic(0x0, NULL);

	return 0;
}

static void sec_debug_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	per_cpu(sec_debug_upload_cause, smp_processor_id()) = type;

	/* to check VDD_ALIVE / XnRESET issue */
	__raw_writel(type, EXYNOS_INFORM3);
	__raw_writel(type, EXYNOS_INFORM4);
	__raw_writel(type, EXYNOS_INFORM6);

	pr_emerg("(%s) %x\n", __func__, type);
}

/*
 * Called from dump_stack()
 * This function call does not necessarily mean that a fatal error
 * had occurred. It may be just a warning.
 */
inline int sec_debug_dump_stack(void)
{
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	sec_debug_save_context();

	/* flush L1 from each core.
	   L2 will be flushed later before reset. */
	flush_cache_all();

	return 0;
}
EXPORT_SYMBOL(sec_debug_dump_stack);

static inline void sec_debug_hw_reset(void)
{
	pr_emerg("(%s) %s\n", __func__, linux_banner);
	pr_emerg("(%s) rebooting...\n", __func__);

	flush_cache_all();

	outer_flush_all();
	pr_emerg("(%s) ...\n", __func__);
	mach_restart(0, 0);

	while (1) {
		pr_err("should not be happend\n");
		pr_err("should not be happend\n");
	}
}

#ifdef CONFIG_SEC_WATCHDOG_RESET
static inline void sec_debug_disable_watchdog(void)
{
	writel(0, S3C2410_WTCON);
	pr_err("(%s) disable watchdog reset while printing log\n", __func__);
}
#endif

#ifdef CONFIG_SEC_DEBUG_FUPLOAD_DUMP_MORE
static void dump_all_task_info(void);
static void dump_cpu_stat(void);
/* static void dump_state_and_upload(void); */
#endif

static int sec_debug_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	local_irq_disable();

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
	bStopLogging = 1;

	/* TODO: Shoud print PLL ? */
	/*
	 * pr_info("APLL_CON :%x \n", __raw_readl(EXYNOS5_APLL_CON0) );
	 * pr_info("BPLL_CON :%x \n", __raw_readl(EXYNOS5_BPLL_CON0) );
	 * pr_info("KPLL_CON :%x \n", __raw_readl(EXYNOS5_KPLL_CON0) );
	 */
#endif

	sec_debug_set_upload_magic(0x66262564, buf);

	if (!strcmp(buf, "User Fault"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (!strcmp(buf, "Crash Key"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strcmp(buf, "HSIC Disconnected"))
		sec_debug_set_upload_cause(UPLOAD_CAUSE_HSIC_DISCONNECTED);
	else
		sec_debug_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);

	pr_err("(%s) checksum_sched_log: %x\n", __func__, checksum_sched_log());

#ifdef CONFIG_SEC_WATCHDOG_RESET
	sec_debug_disable_watchdog();
#endif

	if(sec_debug_level.en.kernel_fault) {
#ifdef CONFIG_SEC_DEBUG_PMU_LOG
		exynos_pmu_debug_save();
#endif
#ifdef CONFIG_SEC_DEBUG_CMU_LOG
		exynos_cmu_debug_save();
#endif
#ifdef CONFIG_SEC_DEBUG_FUPLOAD_DUMP_MORE
		dump_all_task_info();
		dump_cpu_stat();

		show_state_filter(TASK_STATE_MAX);	/* no backtrace */
#else
		show_state();
#endif

		sec_debug_dump_stack();
	}

	sec_debug_hw_reset();

	return 0;
}

#if defined(CONFIG_SEC_DEBUG_CHECK_TASKPTR_FAULT)
static struct pt_regs *safepanic_regs;

void sec_debug_show_regs_simple(struct pt_regs *regs)
{
	unsigned long flags;
	char buf[64];

	print_symbol("PC is at %s\n", instruction_pointer(regs));
	print_symbol("LR is at %s\n", regs->ARM_lr);
	pr_alert("pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n"
	       "sp : %08lx  ip : %08lx  fp : %08lx\n",
		regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr,
		regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	pr_alert("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9,
		regs->ARM_r8);
	pr_alert("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6,
		regs->ARM_r5, regs->ARM_r4);
	pr_alert("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2,
		regs->ARM_r1, regs->ARM_r0);

	flags = regs->ARM_cpsr;
	buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	buf[4] = '\0';

	pr_alert("Flags: %s  IRQs o%s  FIQs o%s  Mode %s  ISA %s\n",
		buf, interrupts_enabled(regs) ? "n" : "ff",
		fast_interrupts_enabled(regs) ? "n" : "ff",
		processor_modes[processor_mode(regs)],
		isa_modes[isa_mode(regs)]);

#ifdef CONFIG_CPU_CP15
	{
		unsigned int ctrl;

		buf[0] = '\0';
#ifdef CONFIG_CPU_CP15_MMU
		{
			unsigned int transbase, dac;
			asm("mrc p15, 0, %0, c2, c0\n\t"
			    "mrc p15, 0, %1, c3, c0\n"
			    : "=r" (transbase), "=r" (dac));
			snprintf(buf, sizeof(buf), "  Table: %08x  DAC: %08x",
				transbase, dac);
		}
#endif
		asm("mrc p15, 0, %0, c1, c0\n" : "=r" (ctrl));

		pr_alert("Control: %08x%s\n", ctrl, buf);
	}
#endif

#ifdef CONFIG_CPU_CP15
	{
		unsigned long reg0, reg1, reg2, reg3;

		asm ("mrc p15, 0, %0, c0, c0, 5\n" : "=r" (reg0));
		if (reg0 & (1 << 31))
			/* MPIDR */
			printk("CPU %ld / CLUSTER %ld\n",
					reg0 & 0x3, (reg0 >> 8) & 0xF);

		asm ("mrc p15, 0, %0, c5, c0, 0\n\t"
		     "mrc p15, 0, %1, c5, c1, 0\n"
		     : "=r" (reg0), "=r" (reg1));
		asm ("mrc p15, 0, %0, c5, c0, 1\n\t"
		     "mrc p15, 0, %1, c5, c1, 1\n"
		     : "=r" (reg2), "=r" (reg3));
		pr_alert("DFSR: %08lx, ADFSR: %08lx, IFSR: %08lx, AIFSR: %08lx\n",
			reg0, reg1, reg2, reg3);

		asm ("mrc p15, 0, %0, c0, c0, 0\n" : "=r" (reg0));
		if (((reg0 >> 4) & 0xFFF) == 0xC0F) { /* Cortex-A15 */
			asm ("mrrc p15, 0, %0, %1, c15\n\t"
			     "mrrc p15, 1, %2, %3, c15\n"
			     : "=r" (reg0), "=r" (reg1),
			     "=r" (reg2), "=r" (reg3));
			pr_alert("CPUMERRSR: %08lx_%08lx, L2MERRSR: %08lx_%08lx\n",
				reg1, reg0, reg3, reg2);
		}
	}
#endif
}

/*
 * This function can be used while current pointer is invalid.
 */
void sec_debug_panic_handler_safe(struct pt_regs *regs)
{
	unsigned long flags;
	unsigned magic = 0x66262564;
	enum sec_debug_upload_cause_t type = UPLOAD_CAUSE_KERNEL_PANIC;
	const char *text = "safe panic handler: "
			"local context is saved into cpu0 slot "
			"regardless of real cpu number.";

	local_irq_disable();

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
	bStopLogging = 1;
#endif

	safepanic_regs = regs;

	*(unsigned int *)SEC_DEBUG_MAGIC_VA = magic;
	*(unsigned int *)(SEC_DEBUG_MAGIC_VA + 0x4000) = magic;
	strncpy((char *)SEC_DEBUG_MAGIC_VA + 4, text, SZ_1K - 4);

	per_cpu(sec_debug_upload_cause, 0) = type;

	__raw_writel(type, EXYNOS_INFORM3);
	__raw_writel(type, EXYNOS_INFORM4);
	__raw_writel(type, EXYNOS_INFORM6);

#ifdef CONFIG_SEC_DEBUG_PMU_LOG
	exynos_pmu_debug_save();
#endif
#ifdef CONFIG_SEC_DEBUG_CMU_LOG
	exynos_cmu_debug_save();
#endif

	local_irq_save(flags);
	sec_debug_save_mmu_reg(&per_cpu(sec_debug_mmu_reg, 0));
	sec_debug_save_core_reg(&per_cpu(sec_debug_core_reg, 0));
	local_irq_restore(flags);

	flush_cache_all();
	outer_flush_all();

	mach_restart(0, 0);
}
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

	if (code == KEY_POWER)
		pr_info("key:pwr(%d)\n", value);

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
#ifdef CONFIG_FB_S5P
					read_lcd_register();
#endif
					panic("Crash Key");
				}
			}
		}

#if defined(CONFIG_SEC_DEBUG_TSP_LOG) && (defined(CONFIG_TOUCHSCREEN_FTS) || defined(CONFIG_TOUCHSCREEN_MELFAS_MMS449))
		/* dump TSP rawdata
		 *	Hold volume up key first
		 *	and then press home key twice
		 *	and volume down key should not be pressed
		 */
		if (volup_p && !voldown_p) {
			if (code == KEY_HOMEPAGE) {
				pr_info
				    ("%s: count to dump tsp rawdata : %d\n",
				     __func__, ++loopcount);
				if (loopcount == 2) {
#if defined(CONFIG_TOUCHSCREEN_FTS)
					tsp_dump();
#endif
#if defined(CONFIG_TOUCHSCREEN_MELFAS_MMS449)
					mms_tsp_dump();
#endif
				}
			}
		}
#endif
	} else {
		if (code == VOLUME_UP) {
#if defined(CONFIG_SEC_DEBUG_TSP_LOG) && defined(CONFIG_TOUCHSCREEN_MELFAS_MMS449)
			loopcount = 0;
#endif
			volup_p = false;
		}
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

static void sec_kmsg_dump(struct kmsg_dumper *dumper,
	enum kmsg_dump_reason reason)
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
	.dump = sec_kmsg_dump,
};

int __init sec_debug_init(void)
{
	int i=0;
	if (!sec_debug_level.en.kernel_fault)
		return -1;

	sec_debug_set_upload_magic(0x66262564, NULL);
	sec_debug_set_upload_cause(UPLOAD_CAUSE_INIT);

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG_NONCACHED
	map_noncached_sched_log_buf();
#endif

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
	map_noncached_aux_log_buf();
#endif

	kmsg_dump_register(&sec_dumper);

	register_reboot_notifier(&nb_reboot_block);

	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

	for(i=0; i<NR_CPUS; ++i) {
		atomic_set(&task_log_idx[i], -1);
		atomic_set(&irq_log_idx[i], -1);
		atomic_set(&work_log_idx[i], -1);
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
		atomic_set(&timer_log_idx[i], -1);
#endif
	}

	return 0;
}

int get_sec_debug_level(void)
{
	return sec_debug_level.uint_val;
}

#ifdef CONFIG_SEC_FILE_LEAK_DEBUG

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
#endif

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
void __sec_debug_task_log(int cpu, struct task_struct *task, char *msg)
{
	unsigned i;

	if (bStopLogging)
		return;

	if (!task && !msg)
		return;

	i = atomic_inc_return(&task_log_idx[cpu]) & (SCHED_LOG_MAX - 1);
	psec_debug_log->task[cpu][i].time = cpu_clock(cpu);

	if (task) {
		strlcpy(psec_debug_log->task[cpu][i].comm, task->comm,sizeof(psec_debug_log->task[cpu][i].comm));
		psec_debug_log->task[cpu][i].pid = task->pid;
	} else {
		strlcpy(psec_debug_log->task[cpu][i].comm, msg,sizeof(psec_debug_log->task[cpu][i].comm));
		psec_debug_log->task[cpu][i].pid = -1;
	}
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


#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
void sec_debug_irq_last_exit_log(void)
{
	int cpu = raw_smp_processor_id();
	gExcpIrqExitTime[cpu] = cpu_clock(cpu);
}
#endif
#endif /* CONFIG_SEC_DEBUG_SCHED_LOG */

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
#ifdef CONFIG_SEC_DEBUG_RT_THROTTLE_ACTIVE
unsigned long long sec_debug_clock() {
	int cpu;

	if (!sec_debug_level.en.kernel_fault)
		return 0;

	cpu = raw_smp_processor_id();
	return cpu_clock(cpu);
}
#endif
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
		i = atomic_inc_return(&gExcpAuxCpuClockSwitchLogIdx)
			& (AUX_LOG_CPU_CLOCK_SWITCH_MAX - 1);
		(*gExcpAuxLogPtr).CpuClockSwitchLog[i].time = cpu_clock(cpu);
		(*gExcpAuxLogPtr).CpuClockSwitchLog[i].cpu = cpu;
		strncpy((*gExcpAuxLogPtr).CpuClockSwitchLog[i].log,
			buf, AUX_LOG_LENGTH);
		break;
	case SEC_DEBUG_AUXLOG_THERMAL_CHANGE:
		i = atomic_inc_return(&gExcpAuxThermalLogIdx)
			& (AUX_LOG_THERMAL_MAX - 1);
		(*gExcpAuxLogPtr).ThermalLog[i].time = cpu_clock(cpu);
		(*gExcpAuxLogPtr).ThermalLog[i].cpu = cpu;
		strncpy((*gExcpAuxLogPtr).ThermalLog[i].log,
			buf, AUX_LOG_LENGTH);
		break;
#ifdef CONFIG_SEC_DEBUG_RT_THROTTLE_ACTIVE
	case SEC_DEBUG_AUXLOG_IRQ:
		i = atomic_inc_return(&gExcpAuxIrqLogIdx)
			& (AUX_LOG_IRQ_MAX - 1);
		(*gExcpAuxLogPtr).IrqLog[i].time = cpu_clock(cpu);
		(*gExcpAuxLogPtr).IrqLog[i].cpu = cpu;
		strncpy((*gExcpAuxLogPtr).IrqLog[i].log,
			buf, AUX_LOG_LENGTH);
		break;
#endif
	case SEC_DEBUG_AUXLOG_WIFI:
		i = atomic_inc_return(&gExcpAuxWifiLogIdx)
			& (AUX_LOG_WIFI_MAX - 1);
		(*gExcpAuxLogPtr).WifiLog[i].time = cpu_clock(cpu);
		(*gExcpAuxLogPtr).WifiLog[i].cpu = cpu;
		strncpy((*gExcpAuxLogPtr).WifiLog[i].log,
			buf, AUX_LOG_LENGTH);
		break;
#ifdef CONFIG_SEC_DEBUG_AUX_SUSPEND
	case SEC_DEBUG_AUXLOG_SUSPEND:
		i = atomic_inc_return(&gExcpAuxSuspendLogIdx)
			& (AUX_LOG_SUSPEND_MAX - 1);
		(*gExcpAuxLogPtr).SuspendLog[i].time = cpu_clock(cpu);
		(*gExcpAuxLogPtr).SuspendLog[i].cpu = cpu;
		strncpy((*gExcpAuxLogPtr).SuspendLog[i].log,
			buf, AUX_LOG_LENGTH);
		break;
#endif

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

#ifdef CONFIG_SEC_KEEP_SBOOT
static int __init sec_bl_mem_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;
	int ret;

	/* If we encounter any problem parsing str ... */
	if (!size || *str != '@' || kstrtoul(str + 1, 0, &base))
		goto out;

	ret = memblock_reserve(base, size);
	if (ret) {
		pr_err("%s: failed(%d) reserving size %d at base 0x%lx\n",
				__func__, ret, size, base);
	}

out:
	return 0;
}

early_param("sec_bl_mem", sec_bl_mem_setup);
#endif /* CONFIG_SEC_KEEP_SBOOT */

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

	return count;
}

static const struct file_operations sec_user_fault_proc_fops = {
	.write = sec_user_fault_write,
};

static int __init sec_debug_user_fault_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("user_fault", S_IWUSR | S_IWGRP, NULL,
			    &sec_user_fault_proc_fops);
	if (!entry)
		return -ENOMEM;
	return 0;
}

device_initcall(sec_debug_user_fault_init);
#endif

static int set_reset_reason_proc_show(struct seq_file *m, void *v)
{
	if (reset_reason == RR_S)
		seq_printf(m, "SPON\n");
	else if (reset_reason == RR_W)
		seq_printf(m, "WPON\n");
	else if (reset_reason == RR_D)
		seq_printf(m, "DPON\n");
	else if (reset_reason == RR_K)
		seq_printf(m, "KPON\n");
	else if (reset_reason == RR_M)
		seq_printf(m, "MPON\n");
	else if (reset_reason == RR_P)
		seq_printf(m, "PPON\n");
	else if (reset_reason == RR_R)
		seq_printf(m, "RPON\n");
	else if (reset_reason == RR_B)
		seq_printf(m, "BPON\n");
	else
		seq_printf(m, "NPON\n");

	return 0;
}

static int sec_reset_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_reset_reason_proc_show, NULL);
}

static const struct file_operations sec_reset_reason_proc_fops = {
	.open = sec_reset_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init sec_debug_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason", S_IWUSR|S_IWGRP, NULL,
		&sec_reset_reason_proc_fops);

	if (!entry)
		return -ENOMEM;

	return 0;
}

device_initcall(sec_debug_reset_reason_init);

int __init sec_debug_magic_init(void)
{
#if !defined(CONFIG_SPARSEMEM)
	if (reserve_bootmem(SEC_DEBUG_MAGIC_PA, SZ_4K, BOOTMEM_EXCLUSIVE)) {
		pr_err("%s: failed reserving magic code area\n", __func__);
		return -ENOMEM;
	}
#endif

	pr_info("%s: success reserving magic code area\n", __func__);
	return 0;
}
#if defined(CONFIG_SPARSEMEM)
void __init sec_debug_memblock_reserve(void)
{
	memblock_reserve(SEC_DEBUG_MAGIC_PA, SZ_4K);
}
#endif
#ifdef CONFIG_SEC_DEBUG_FUPLOAD_DUMP_MORE
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
		print_worker_info(KERN_INFO, tsk);
		show_stack(tsk, NULL);
		pr_info("\n");
	}
}

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next,
				struct task_struct,
				thread_group);
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

#endif /* CONFIG_SEC_DEBUG_FUPLOAD_DUMP_MORE */

#if defined(CONFIG_SEC_DEBUG) && defined(CONFIG_SEC_DEBUG_SUBSYS)
void sec_debug_subsys_set_reserved_out_buf(unsigned int buf, unsigned int size)
{
	reserved_out_buf = buf;
	reserved_out_size = size;
}

static int __init sec_subsys_log_setup(char *str)
{
	unsigned size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || size != roundup_pow_of_two(size) || *str != '@'
		|| kstrtoul(str + 1, 0, &base)) {
		pr_err("%s: failed to parse address.\n", __func__);
		goto out;
	}

	if (size < SZ_2M || size > SZ_8M) {
		pr_err("%s: size 0x%x might be wrong.\n", __func__, size);
		goto out;
	}

	if (memblock_reserve(base, size)) {
		pr_err("%s: failed to reserve size %d " \
				"at base 0x%lx\n", __func__, size, base);
		goto out;
	}

	pr_info("%s, base:0x%lx size:0x%x\n", __func__, base, size);

	sec_subsys_log_buf = phys_to_virt(base);
	sec_subsys_log_size = round_up(sizeof(struct sec_debug_subsys), PAGE_SIZE);
	sec_debug_subsys_set_reserved_out_buf(base + sec_subsys_log_size, (size - sec_subsys_log_size));
out:
	return 0;
}
early_param("sec_summary_log", sec_subsys_log_setup);

int sec_debug_subsys_init(void)
{
	int offset = 0;
	if(!sec_subsys_log_buf) {
		pr_info("no subsys buffer\n");
		return 0;
	}
	subsys_info = (struct sec_debug_subsys *)sec_subsys_log_buf;
	memset(subsys_info, 0, sizeof(struct sec_debug_subsys));

	subsys_info->kernel.sched_log.task_buf_paddr = virt_to_phys(&sec_debug_log) + offsetof(struct sched_log, task);
	subsys_info->kernel.sched_log.task_struct_sz = sizeof(struct task_log);
	subsys_info->kernel.sched_log.task_array_cnt = SCHED_LOG_MAX;
	subsys_info->kernel.sched_log.irq_buf_paddr = virt_to_phys(&sec_debug_log) + offsetof(struct sched_log, irq);
	subsys_info->kernel.sched_log.irq_struct_sz = sizeof(struct irq_log);
	subsys_info->kernel.sched_log.irq_array_cnt = SCHED_LOG_MAX;
	subsys_info->kernel.sched_log.work_buf_paddr = virt_to_phys(&sec_debug_log) + offsetof(struct sched_log, work);
	subsys_info->kernel.sched_log.work_struct_sz = sizeof(struct work_log);
	subsys_info->kernel.sched_log.work_array_cnt = SCHED_LOG_MAX;
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
	subsys_info->kernel.sched_log.timer_buf_paddr = virt_to_phys(&sec_debug_log) + offsetof(struct sched_log, timer);
	subsys_info->kernel.sched_log.timer_struct_sz = sizeof(struct timer_log);
	subsys_info->kernel.sched_log.timer_array_cnt = SCHED_LOG_MAX;
#endif

#ifdef CONFIG_ANDROID_LOGGER
	sec_debug_subsys_set_logger_info(&subsys_info->kernel.logger_log);
#endif
	offset += sizeof(struct sec_debug_subsys);

	subsys_info->kernel.cpu_info.cpu_active_mask_paddr = virt_to_phys(cpu_active_mask);
	subsys_info->kernel.cpu_info.cpu_online_mask_paddr = virt_to_phys(cpu_online_mask);

	offset += sec_debug_set_cpu_info(subsys_info,sec_subsys_log_buf+offset);

	subsys_info->kernel.cmdline_paddr = virt_to_phys(sec_subsys_log_buf)+offset;
	subsys_info->kernel.cmdline_len = strlen(saved_command_line);
	memcpy(sec_subsys_log_buf+offset,saved_command_line,subsys_info->kernel.cmdline_len);
	offset += subsys_info->kernel.cmdline_len;

	subsys_info->kernel.linuxbanner_paddr = virt_to_phys(sec_subsys_log_buf)+offset;
	subsys_info->kernel.linuxbanner_len = strlen(linux_banner);
	memcpy(sec_subsys_log_buf+offset,linux_banner,subsys_info->kernel.linuxbanner_len);
	offset += subsys_info->kernel.linuxbanner_len;

	subsys_info->kernel.nr_cpus = CONFIG_NR_CPUS;

	subsys_info->reserved_out_buf = reserved_out_buf;
	subsys_info->reserved_out_size = reserved_out_size;

	subsys_info->magic[0] = SEC_DEBUG_SUBSYS_MAGIC0;
	subsys_info->magic[1] = SEC_DEBUG_SUBSYS_MAGIC1;
	subsys_info->magic[2] = SEC_DEBUG_SUBSYS_MAGIC2;
	subsys_info->magic[3] = SEC_DEBUG_SUBSYS_MAGIC3;

	return 0;
}
late_initcall(sec_debug_subsys_init);

int sec_debug_save_die_info(const char *str, struct pt_regs *regs)
{
	if (!sec_subsys_log_buf || !subsys_info)
		return -ENOMEM;
	snprintf(subsys_info->kernel.excp.pc_sym, sizeof(subsys_info->kernel.excp.pc_sym),
		"%pS", (void *)regs->ARM_pc);
	snprintf(subsys_info->kernel.excp.lr_sym, sizeof(subsys_info->kernel.excp.lr_sym),
		"%pS", (void *)regs->ARM_lr);

	return 0;
}

int sec_debug_save_panic_info(const char *str, unsigned int caller)
{
	if(!sec_subsys_log_buf || !subsys_info)
		return 0;
	snprintf(subsys_info->kernel.excp.panic_caller,
		sizeof(subsys_info->kernel.excp.panic_caller), "%pS", (void *)caller);
	snprintf(subsys_info->kernel.excp.panic_msg,
		sizeof(subsys_info->kernel.excp.panic_msg), "%s", str);
	snprintf(subsys_info->kernel.excp.thread,
		sizeof(subsys_info->kernel.excp.thread), "%s:%d", current->comm,
		task_pid_nr(current));

	return 0;
}
#endif /* defined(CONFIG_SEC_DEBUG) && defined(CONFIG_SEC_DEBUG_SUBSYS) */

#ifdef CONFIG_SEC_DEBUG_INPUT_HANDLER_CRASH
static void sec_debug_check_key_state(struct input_debug_drv_data *ddata,
	int index, bool state)
{
	struct input_debug_pdata *pdata = ddata->pdata;

	if (state == pdata->key_state[index].state)
		__set_bit(pdata->key_state[index].code,
			ddata->keybit);
	else
		__clear_bit(pdata->key_state[index].code,
			ddata->keybit);
}

static void sec_debug_check_crash_keys(struct input_debug_drv_data *ddata)
{
	int i = 0;

	if (!sec_debug_level.en.kernel_fault)
		return;

	for (i = 0; i < ARRAY_SIZE(ddata->keybit); i++) {
		if (ddata->input_ids[0].keybit[i] != ddata->keybit[i])
			return ;
	}

#ifdef CONFIG_FB_S5P
	read_lcd_register();
#endif
	panic("Crash Key");
}

static bool sec_debug_check_keys(struct input_debug_drv_data *ddata,
	u32 code, int value)
{

	struct input_debug_pdata *pdata = ddata->pdata;
	int i = 0;
	int j = 0;

	for (i = 0; i < pdata->nkeys; i++) {
		if (KEY_POWER == code) {
			for (j = 0; j < ARRAY_SIZE(ddata->keybit); j++) {
				if (ddata->input_ids[0].keybit[j] != ddata->keybit[j])
					goto out;
			}
			if (!!value) {
				ddata->crash_key_cnt++;
				pr_debug("%s: count for enter forced upload : %d\n",
						__func__, ddata->crash_key_cnt);
			}

			return (2 == ddata->crash_key_cnt) ? true : false;

		} else if (code == pdata->key_state[i].code) {
			sec_debug_check_key_state(ddata, i, !!value);
			goto out;
		}
	}
out :
	ddata->crash_key_cnt = 0;
	return false;
}

static void sec_input_debug_event(struct input_handle *handle,
	unsigned int type, u32 code, int value)
{
	struct input_handler *handler = handle->handler;
	struct input_debug_drv_data *ddata = handler->private;

	if ((!sec_debug_level.en.kernel_fault) || (EV_KEY != type))
		return;

	if (sec_debug_check_keys(ddata, code, value))
		sec_debug_check_crash_keys(handler->private);
}

static int sec_input_debug_connect(struct input_handler *handler, struct input_dev *dev,
			 const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = SEC_DEBUG_NAME;

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	pr_debug("[sec_debug] Connected device: %s (%s at %s)\n",
	       dev_name(&dev->dev),
	       dev->name ?: "unknown",
	       dev->phys ?: "unknown");

	return 0;

 err_unregister_handle:
	input_unregister_handle(handle);
 err_free_handle:
	kfree(handle);
	return error;
}

static void sec_input_debug_disconnect(struct input_handle *handle)
{
	pr_debug("[sec_debug] Disconnected device: %s\n",
	       dev_name(&handle->dev->dev));

	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}
#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static struct input_debug_pdata *
sec_input_debug_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *pp;
	struct input_debug_pdata *pdata;
	struct input_debug_key_state *state;
	int error;
	int nbuttons;
	int i;

	node = dev->of_node;
	if (!node) {
		error = -ENODEV;
		goto err_out;
	}

	nbuttons = of_get_child_count(node);
	if (nbuttons == 0) {
		error = -ENODEV;
		goto err_out;
	}

	pdata = kzalloc(sizeof(*pdata) + nbuttons * sizeof(*state),
			GFP_KERNEL);
	if (!pdata) {
		error = -ENOMEM;
		goto err_out;
	}

	pdata->key_state = (struct input_debug_key_state *)(pdata + 1);
	pdata->nkeys = nbuttons;

	i = 0;
	for_each_child_of_node(node, pp) {
		state = &pdata->key_state[i++];
		if (of_property_read_u32(pp, "linux,code", &state->code)) {
			dev_err(dev, "Button without keycode: %dth button\n",
				i);
			error = -EINVAL;
			goto err_free_pdata;
		}
		state->state = of_property_read_bool(pp, "state");

		pr_debug("%s[%d]: CODE: %x STATE: %d\n", __func__,
				__LINE__, state->code, state->state);

	}

	if (pdata->nkeys == 0) {
		error = -EINVAL;
		goto err_free_pdata;
	}

	return pdata;

err_free_pdata:
	kfree(pdata);
err_out:
	return ERR_PTR(error);
}

static struct of_device_id sec_input_debug_of_match[] = {
	{ .compatible = "sec_input_debug-keys", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_input_debug_of_match);

#else

static inline struct input_debug_pdata *
sec_input_debug_get_devtree_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}

#endif

static int sec_input_debug_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct input_debug_pdata *pdata = pdev->dev.platform_data;
	struct input_debug_drv_data *ddata;
	int i = 0;

	if (!pdata) {
		pdata = sec_input_debug_get_devtree_pdata(dev);
		if (IS_ERR(pdata)) {
			pr_err("%s:DT parse err!\n", __func__);
			return PTR_ERR(pdata);
		}
	}
	ddata = kzalloc(sizeof(struct input_debug_drv_data), GFP_KERNEL);

	ddata->pdata = pdata;

	ddata->input_handler.event = sec_input_debug_event;
	ddata->input_handler.connect = sec_input_debug_connect;
	ddata->input_handler.disconnect = sec_input_debug_disconnect;
	ddata->input_handler.name = SEC_DEBUG_NAME;
	ddata->input_handler.id_table = ddata->input_ids;
	ddata->input_handler.private = ddata;

	ddata->input_ids[0].flags = INPUT_DEVICE_ID_MATCH_KEYBIT;
	for (i = 0; i < pdata->nkeys; i++) {
		bool state = pdata->key_state[i].state;
		u32 code = pdata->key_state[i].code;

		__set_bit(code, ddata->input_ids[0].keybit);

		if (!state)
			__set_bit(code, ddata->keybit);
	}

	ddata->input_ids[0].flags |= INPUT_DEVICE_ID_MATCH_EVBIT;
	__set_bit(EV_KEY, ddata->input_ids[0].evbit);

	return input_register_handler(&ddata->input_handler);
}

static int sec_input_debug_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sec_input_debug_driver = {
	.probe		= sec_input_debug_probe,
	.remove		= sec_input_debug_remove,
	.driver		= {
		.name	= SEC_DEBUG_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sec_input_debug_of_match),
	}
};

static int __init sec_input_debug_init(void)
{
	return platform_driver_register(&sec_input_debug_driver);
}

static __exit void sec_input_debug_exit(void)
{
	platform_driver_unregister(&sec_input_debug_driver);
}

late_initcall(sec_input_debug_init);
module_exit(sec_input_debug_exit);
#endif /* CONFIG_SEC_DEBUG_INPUT_HANDLER_CRASH */

