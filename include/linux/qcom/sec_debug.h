/*
 * sec_debug.h
 *
 * header file supporting debug functions for Samsung device
 *
 * COPYRIGHT(C) Samsung Electronics Co., Ltd. 2006-2011 All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/reboot.h>
#include <asm/cacheflush.h>
#include <linux/qcom/sec_debug_summary.h>




// Enable CONFIG_RESTART_REASON_DDR to use DDR address for saving restart reason
#ifdef CONFIG_RESTART_REASON_DDR
extern void *restart_reason_ddr_address;
#endif

#ifdef CONFIG_SEC_PERIPHERAL_SECURE_CHK
extern void sec_peripheral_secure_check_fail(void);
#endif

#ifdef CONFIG_SEC_DEBUG
#define SECDEBUG_MODE 0x776655ee

#ifndef CONFIG_ARM64
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

#else 
struct sec_debug_mmu_reg_t {
	unsigned long TTBR0_EL1;
	unsigned long TTBR1_EL1;
	unsigned long TCR_EL1;
	unsigned long MAIR_EL1;
	unsigned long ATCR_EL1;
	unsigned long AMAIR_EL1;	

	unsigned long HSTR_EL2;
	unsigned long HACR_EL2;
	unsigned long TTBR0_EL2;
	unsigned long VTTBR_EL2;
	unsigned long TCR_EL2;
	unsigned long VTCR_EL2;
	unsigned long MAIR_EL2;
	unsigned long ATCR_EL2;
	
	unsigned long TTBR0_EL3;
	unsigned long MAIR_EL3;
	unsigned long ATCR_EL3;
};


/* ARM CORE regs mapping structure */
struct sec_debug_core_t {
	/* COMMON */
	unsigned long x0;
	unsigned long x1;
	unsigned long x2;
	unsigned long x3;
	unsigned long x4;
	unsigned long x5;
	unsigned long x6;
	unsigned long x7;
	unsigned long x8;
	unsigned long x9;
	unsigned long x10;
	unsigned long x11;
	unsigned long x12;
	unsigned long x13;
	unsigned long x14;
	unsigned long x15;
	unsigned long x16;
	unsigned long x17;
	unsigned long x18;
	unsigned long x19;
	unsigned long x20;
	unsigned long x21;
	unsigned long x22;
	unsigned long x23;
	unsigned long x24;
	unsigned long x25;
	unsigned long x26;
	unsigned long x27;
	unsigned long x28;
	unsigned long x29;//sp
	unsigned long x30;//lr

	/* PC & CPSR */	
	unsigned long pc;
	unsigned long cpsr;

	/* EL0 */
	unsigned long sp_el0;

	/* EL1 */
	unsigned long sp_el1;
	unsigned long elr_el1;
	unsigned long spsr_el1;

	/* EL2 */
	unsigned long sp_el2;
	unsigned long elr_el2;
	unsigned long spsr_el2;

	/* EL3 */
//	unsigned long sp_el3;
//	unsigned long elr_el3;
//	unsigned long spsr_el3;

};
#endif
#define READ_SPECIAL_REG(x) ({ \
	u64 val; \
	asm volatile ("mrs %0, " # x : "=r"(val)); \
	val; \
})

DECLARE_PER_CPU(struct sec_debug_core_t, sec_debug_core_reg);
DECLARE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_mmu_reg);
DECLARE_PER_CPU(enum sec_debug_upload_cause_t, sec_debug_upload_cause);

#ifndef CONFIG_ARM64
static void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"  /* SCTLR */
		"str r1, [%0]\n\t"
		"mrc    p15, 0, r1, c2, c0, 0\n\t"  /* TTBR0 */
		"str r1, [%0,#4]\n\t"
		"mrc    p15, 0, r1, c2, c0,1\n\t"   /* TTBR1 */
		"str r1, [%0,#8]\n\t"
		"mrc    p15, 0, r1, c2, c0,2\n\t"   /* TTBCR */
		"str r1, [%0,#12]\n\t"
		"mrc    p15, 0, r1, c3, c0,0\n\t"   /* DACR */
		"str r1, [%0,#16]\n\t"
		"mrc    p15, 0, r1, c5, c0,0\n\t"   /* DFSR */
		"str r1, [%0,#20]\n\t"
		"mrc    p15, 0, r1, c6, c0,0\n\t"   /* DFAR */
		"str r1, [%0,#24]\n\t"
		"mrc    p15, 0, r1, c5, c0,1\n\t"   /* IFSR */
		"str r1, [%0,#28]\n\t"
		"mrc    p15, 0, r1, c6, c0,2\n\t"   /* IFAR */
		"str r1, [%0,#32]\n\t"
		/* Don't populate DAFSR and RAFSR */
		"mrc    p15, 0, r1, c10, c2,0\n\t"  /* PMRRR */
		"str r1, [%0,#44]\n\t"
		"mrc    p15, 0, r1, c10, c2,1\n\t"  /* NMRRR */
		"str r1, [%0,#48]\n\t"
		"mrc    p15, 0, r1, c13, c0,0\n\t"  /* FCSEPID */
		"str r1, [%0,#52]\n\t"
		"mrc    p15, 0, r1, c13, c0,1\n\t"  /* CONTEXT */
		"str r1, [%0,#56]\n\t"
		"mrc    p15, 0, r1, c13, c0,2\n\t"  /* URWTPID */
		"str r1, [%0,#60]\n\t"
		"mrc    p15, 0, r1, c13, c0,3\n\t"  /* UROTPID */
		"str r1, [%0,#64]\n\t"
		"mrc    p15, 0, r1, c13, c0,4\n\t"  /* POTPIDR */
		"str r1, [%0,#68]\n\t" :            /* output */
		: "r"(mmu_reg)                      /* input */
		: "%r1", "memory"                   /* clobbered register */
	);
}

static void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	/* we will be in SVC mode when we enter this function. Collect
	SVC registers along with cmn registers. */
	asm("str r0, [%0,#0]\n\t"       /* R0 is pushed first to core_reg */
		"mov r0, %0\n\t"            /* R0 will be alias for core_reg */
		"str r1, [r0,#4]\n\t"       /* R1 */
		"str r2, [r0,#8]\n\t"       /* R2 */
		"str r3, [r0,#12]\n\t"      /* R3 */
		"str r4, [r0,#16]\n\t"      /* R4 */
		"str r5, [r0,#20]\n\t"      /* R5 */
		"str r6, [r0,#24]\n\t"      /* R6 */
		"str r7, [r0,#28]\n\t"      /* R7 */
		"str r8, [r0,#32]\n\t"      /* R8 */
		"str r9, [r0,#36]\n\t"      /* R9 */
		"str r10, [r0,#40]\n\t"     /* R10 */
		"str r11, [r0,#44]\n\t"     /* R11 */
		"str r12, [r0,#48]\n\t"     /* R12 */
		/* SVC */
		"str r13, [r0,#52]\n\t"     /* R13_SVC */
		"str r14, [r0,#56]\n\t"     /* R14_SVC */
		"mrs r1, spsr\n\t"          /* SPSR_SVC */
		"str r1, [r0,#60]\n\t"
		/* PC and CPSR */
		"sub r1, r15, #0x4\n\t"     /* PC */
		"str r1, [r0,#64]\n\t"
		"mrs r1, cpsr\n\t"          /* CPSR */
		"str r1, [r0,#68]\n\t"
		/* SYS/USR */
		"mrs r1, cpsr\n\t"          /* switch to SYS mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x1f\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#72]\n\t"     /* R13_USR */
		"str r14, [r0,#76]\n\t"     /* R14_USR */
		/* FIQ */
		"mrs r1, cpsr\n\t"          /* switch to FIQ mode */
		"and r1,r1,#0xFFFFFFE0\n\t"
		"orr r1,r1,#0x11\n\t"
		"msr cpsr,r1\n\t"
		"str r8, [r0,#80]\n\t"      /* R8_FIQ */
		"str r9, [r0,#84]\n\t"      /* R9_FIQ */
		"str r10, [r0,#88]\n\t"     /* R10_FIQ */
		"str r11, [r0,#92]\n\t"     /* R11_FIQ */
		"str r12, [r0,#96]\n\t"     /* R12_FIQ */
		"str r13, [r0,#100]\n\t"    /* R13_FIQ */
		"str r14, [r0,#104]\n\t"    /* R14_FIQ */
		"mrs r1, spsr\n\t"          /* SPSR_FIQ */
		"str r1, [r0,#108]\n\t"
			/* IRQ */
		"mrs r1, cpsr\n\t"          /* switch to IRQ mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x12\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#112]\n\t"    /* R13_IRQ */
		"str r14, [r0,#116]\n\t"    /* R14_IRQ */
		"mrs r1, spsr\n\t"          /* SPSR_IRQ */
		"str r1, [r0,#120]\n\t"
		/* ABT */
		"mrs r1, cpsr\n\t"          /* switch to Abort mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x17\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#136]\n\t"    /* R13_ABT */
		"str r14, [r0,#140]\n\t"    /* R14_ABT */
		"mrs r1, spsr\n\t"          /* SPSR_ABT */
		"str r1, [r0,#144]\n\t"
		/* UND */
		"mrs r1, cpsr\n\t"          /* switch to undef mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x1B\n\t"
		"msr cpsr,r1\n\t"
		"str r13, [r0,#148]\n\t"    /* R13_UND */
		"str r14, [r0,#152]\n\t"    /* R14_UND */
		"mrs r1, spsr\n\t"          /* SPSR_UND */
		"str r1, [r0,#156]\n\t"
		/* restore to SVC mode */
		"mrs r1, cpsr\n\t"          /* switch to SVC mode */
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x13\n\t"
		"msr cpsr,r1\n\t" :         /* output */
		: "r"(core_reg)                     /* input */
		: "%r0", "%r1"              /* clobbered registers */
	);
	
	return;
}

#else
static inline void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
	u32 pstate,which_el; 

	pstate = READ_SPECIAL_REG(CurrentEl);
	which_el = pstate & PSR_MODE_MASK;

	//pr_emerg("%s: sec_debug EL mode=%d\n", __func__,which_el);

	mmu_reg->TTBR0_EL1 = READ_SPECIAL_REG(TTBR0_EL1);
	mmu_reg->TTBR1_EL1 = READ_SPECIAL_REG(TTBR1_EL1);	
	mmu_reg->TCR_EL1 = READ_SPECIAL_REG(TCR_EL1);
	mmu_reg->MAIR_EL1 = READ_SPECIAL_REG(MAIR_EL1);	
	mmu_reg->AMAIR_EL1 = READ_SPECIAL_REG(AMAIR_EL1);
#if 0
	if(which_el >= PSR_MODE_EL2t){
		mmu_reg->HSTR_EL2 = READ_SPECIAL_REG(HSTR_EL2);	
		mmu_reg->HACR_EL2 = READ_SPECIAL_REG(HACR_EL2);
		mmu_reg->TTBR0_EL2 = READ_SPECIAL_REG(TTBR0_EL2);	
		mmu_reg->VTTBR_EL2 = READ_SPECIAL_REG(VTTBR_EL2);
		mmu_reg->TCR_EL2 = READ_SPECIAL_REG(TCR_EL2);	
		mmu_reg->VTCR_EL2 = READ_SPECIAL_REG(VTCR_EL2);
		mmu_reg->MAIR_EL2 = READ_SPECIAL_REG(MAIR_EL2);	
		mmu_reg->AMAIR_EL2 = READ_SPECIAL_REG(AMAIR_EL2);
	}
#endif	
}

static inline void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	u32 pstate,which_el; 

	pstate = READ_SPECIAL_REG(CurrentEl);
	which_el = pstate & PSR_MODE_MASK;
	
	//pr_emerg("%s: sec_debug EL mode=%d\n", __func__,which_el);

	asm("str x0, [%0,#0]\n\t"	/* x0 is pushed first to core_reg */
		"mov x0, %0\n\t"		/* x0 will be alias for core_reg */
		"str x1, [x0,#0x8]\n\t"	/* x1 */
		"str x2, [x0,#0x10]\n\t"	/* x2 */
		"str x3, [x0,#0x18]\n\t"	/* x3 */
		"str x4, [x0,#0x20]\n\t"	/* x4 */
		"str x5, [x0,#0x28]\n\t"	/* x5 */
		"str x6, [x0,#0x30]\n\t"	/* x6 */
		"str x7, [x0,#0x38]\n\t"	/* x7 */
		"str x8, [x0,#0x40]\n\t"	/* x8 */
		"str x9, [x0,#0x48]\n\t"	/* x9 */
		"str x10, [x0,#0x50]\n\t" /* x10 */
		"str x11, [x0,#0x58]\n\t" /* x11 */
		"str x12, [x0,#0x60]\n\t" /* x12 */
		"str x13, [x0,#0x68]\n\t"/* x13 */
		"str x14, [x0,#0x70]\n\t"/* x14 */
		"str x15, [x0,#0x78]\n\t"/* x15 */
		"str x16, [x0,#0x80]\n\t"/* x16 */
		"str x17, [x0,#0x88]\n\t"/* x17 */
		"str x18, [x0,#0x90]\n\t"/* x18 */
		"str x19, [x0,#0x98]\n\t"/* x19 */
		"str x20, [x0,#0xA0]\n\t"/* x20 */
		"str x21, [x0,#0xA8]\n\t"/* x21 */
		"str x22, [x0,#0xB0]\n\t"/* x22 */
		"str x23, [x0,#0xB8]\n\t"/* x23 */
		"str x24, [x0,#0xC0]\n\t"/* x24 */
		"str x25, [x0,#0xC8]\n\t"/* x25 */
		"str x26, [x0,#0xD0]\n\t"/* x26 */
		"str x27, [x0,#0xD8]\n\t"/* x27 */
		"str x28, [x0,#0xE0]\n\t"/* x28 */
		"str x29, [x0,#0xE8]\n\t"/* x29 */
		"str x30, [x0,#0xF0]\n\t"/* x30 */
		"adr x1, .\n\t"
		"str x1, [x0,#0xF8]\n\t"/* pc *///Need to check how to get the pc value.
		: 	/* output */
		: "r"(core_reg)		/* input */
		: "%x0", "%x1"/* clobbered registers */
		);	
		
	core_reg->sp_el0 = READ_SPECIAL_REG(sp_el0);

	if(which_el >= PSR_MODE_EL2t){
		core_reg->sp_el0 = READ_SPECIAL_REG(sp_el1);
		core_reg->elr_el1 = READ_SPECIAL_REG(elr_el1);
		core_reg->spsr_el1 = READ_SPECIAL_REG(spsr_el1);
		core_reg->sp_el2 = READ_SPECIAL_REG(sp_el2);
		core_reg->elr_el2 = READ_SPECIAL_REG(elr_el2);
		core_reg->spsr_el2 = READ_SPECIAL_REG(spsr_el2);
	}

	return;
}
#endif

static inline void sec_debug_save_context(void)
{
	unsigned long flags;
	local_irq_save(flags);
	sec_debug_save_mmu_reg(&per_cpu(sec_debug_mmu_reg,
				smp_processor_id()));
	sec_debug_save_core_reg(&per_cpu(sec_debug_core_reg,
				smp_processor_id()));
	pr_emerg("(%s) context saved(CPU:%d)\n", __func__,
			smp_processor_id());
	local_irq_restore(flags);
	flush_cache_all();
}

void simulate_msm_thermal_bite(void);
extern void sec_debug_prepare_for_wdog_bark_reset(void);
extern void sec_debug_prepare_for_thermal_reset(void);
extern int sec_debug_init(void);
extern int sec_debug_dump_stack(void);
extern void sec_debug_hw_reset(void);
#ifdef CONFIG_SEC_PERIPHERAL_SECURE_CHK
extern void sec_peripheral_secure_check_fail(void);
#endif
extern void sec_debug_check_crash_key(unsigned int code, int value);
extern void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y, u32 bpp,
		u32 frames);
extern void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
		u32 addr1);
extern void sec_getlog_supply_loggerinfo(void *p_main, void *p_radio,
		void *p_events, void *p_system);
extern void sec_getlog_supply_kloginfo(void *klog_buf);

extern void sec_gaf_supply_rqinfo(unsigned short curr_offset,
				  unsigned short rq_offset);
extern int sec_debug_is_enabled(void);
extern int sec_debug_is_enabled_for_ssr(void);
extern int silent_log_panic_handler(void);
extern void sec_debug_secure_app_addr_size(uint32_t addr, uint32_t size);
#else
static inline void sec_debug_save_context(void) {}
static inline void sec_debug_prepare_for_wdog_bark_reset(void){}
static inline void sec_debug_prepare_for_thermal_reset(void){}
static inline int sec_debug_init(void)
{
	return 0;
}

static inline int sec_debug_dump_stack(void)  
{
	return 0;
}
static inline void sec_debug_check_crash_key(unsigned int code, int value) {}

static inline void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y,
					    u32 bpp, u32 frames)
{
}

static inline void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
					     u32 addr1)
{
}

static inline void sec_getlog_supply_loggerinfo(void *p_main,
						void *p_radio, void *p_events,
						void *p_system)
{
}

static inline void sec_getlog_supply_kloginfo(void *klog_buf)
{
}

static inline void sec_gaf_supply_rqinfo(unsigned short curr_offset,
					 unsigned short rq_offset)
{
}

static inline int sec_debug_is_enabled(void) {return 0; }
#endif

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
extern void sec_debug_task_sched_log_short_msg(char *msg);
extern void sec_debug_secure_log(u32 svc_id,u32 cmd_id);
extern void sec_debug_task_sched_log(int cpu, struct task_struct *task);
extern void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en);
extern void sec_debug_irq_sched_log_end(void);
extern void sec_debug_timer_log(unsigned int type, int int_lock, void *fn);
extern void sec_debug_sched_log_init(void);
#define secdbg_sched_msg(fmt, ...) \
	do { \
		char ___buf[16]; \
		snprintf(___buf, sizeof(___buf), fmt, ##__VA_ARGS__); \
		sec_debug_task_sched_log_short_msg(___buf); \
	} while (0)
#else
static inline void sec_debug_task_sched_log(int cpu, struct task_struct *task)
{
}
static inline void sec_debug_secure_log(u32 svc_id,u32 cmd_id)
{
}
static inline void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en)
{
}
static inline void sec_debug_irq_sched_log_end(void)
{
}
static inline void sec_debug_timer_log(unsigned int type,
						int int_lock, void *fn)
{
}
static inline void sec_debug_sched_log_init(void)
{
}
#define secdbg_sched_msg(fmt, ...)
#endif
extern void sec_debug_irq_enterexit_log(unsigned int irq,
						unsigned long long start_time);

/* klaatu - schedule log */
#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
#define SCHED_LOG_MAX 512
#define TZ_LOG_MAX 64

struct irq_log {
	unsigned long long time;
	int irq;
	void *fn;
	int en;
	int preempt_count;
	void *context;
};

struct secure_log {
	unsigned long long time;
	u32 svc_id, cmd_id;
};

struct irq_exit_log {
	unsigned int irq;
	unsigned long long time;
	unsigned long long end_time;
	unsigned long long elapsed_time;
};

struct sched_log {
	unsigned long long time;
	char comm[TASK_COMM_LEN];
	pid_t pid;
	struct task_struct *pTask;
};


struct timer_log {
	unsigned long long time;
	unsigned int type;
	int int_lock;
	void *fn;
};

#endif	/* CONFIG_SEC_DEBUG_SCHED_LOG */

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

#ifdef CONFIG_SEC_DEBUG_MSG_LOG
extern asmlinkage int sec_debug_msg_log(void *caller, const char *fmt, ...);
#define MSG_LOG_MAX 1024
struct secmsg_log {
	unsigned long long time;
	char msg[64];
	void *caller0;
	void *caller1;
	char *task;
};
#define secdbg_msg(fmt, ...) \
	sec_debug_msg_log(__builtin_return_address(0), fmt, ##__VA_ARGS__)
#else
#define secdbg_msg(fmt, ...)
#endif

#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
#define DCVS_LOG_MAX 256

struct dcvs_debug {
	unsigned long long time;
	int cpu_no;
	unsigned int prev_freq;
	unsigned int new_freq;
};
extern void sec_debug_dcvs_log(int cpu_no, unsigned int prev_freq,
			unsigned int new_freq);
#else
static inline void sec_debug_dcvs_log(int cpu_no, unsigned int prev_freq,
					unsigned int new_freq)
{
}

#endif

#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
#define FG_LOG_MAX 128

struct fuelgauge_debug {
	unsigned long long time;
	unsigned int voltage;
	unsigned short soc;
	unsigned short charging_status;
};
extern void sec_debug_fuelgauge_log(unsigned int voltage, unsigned short soc,
			unsigned short charging_status);
#else
static inline void sec_debug_fuelgauge_log(unsigned int voltage,
			unsigned short soc,	unsigned short charging_status)
{
}

#endif

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
struct sec_debug_log {
	atomic_t idx_sched[CONFIG_NR_CPUS];
	struct sched_log sched[CONFIG_NR_CPUS][SCHED_LOG_MAX];

	atomic_t idx_irq[CONFIG_NR_CPUS];
	struct irq_log irq[CONFIG_NR_CPUS][SCHED_LOG_MAX];

	atomic_t idx_secure[CONFIG_NR_CPUS];
	struct secure_log secure[CONFIG_NR_CPUS][TZ_LOG_MAX];

	atomic_t idx_irq_exit[CONFIG_NR_CPUS];
	struct irq_exit_log irq_exit[CONFIG_NR_CPUS][SCHED_LOG_MAX];

	atomic_t idx_timer[CONFIG_NR_CPUS];
	struct timer_log timer_log[CONFIG_NR_CPUS][SCHED_LOG_MAX];

#ifdef CONFIG_SEC_DEBUG_MSG_LOG
	atomic_t idx_secmsg[CONFIG_NR_CPUS];
	struct secmsg_log secmsg[CONFIG_NR_CPUS][MSG_LOG_MAX];
#endif
#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
	atomic_t dcvs_log_idx[CONFIG_NR_CPUS] ;
	struct dcvs_debug dcvs_log[CONFIG_NR_CPUS][DCVS_LOG_MAX] ;
#endif
#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
	atomic_t fg_log_idx;
	struct fuelgauge_debug fg_log[FG_LOG_MAX] ;
#endif
	/* zwei variables -- last_pet und last_ns */
	unsigned long long last_pet;
	atomic64_t last_ns;
};

#endif	/* CONFIG_SEC_DEBUG_SCHED_LOG */
/* for sec debug level */
#define KERNEL_SEC_DEBUG_LEVEL_LOW	(0x574F4C44)
#define KERNEL_SEC_DEBUG_LEVEL_MID	(0x44494D44)
#define KERNEL_SEC_DEBUG_LEVEL_HIGH	(0x47494844)
#define ANDROID_DEBUG_LEVEL_LOW		0x4f4c
#define ANDROID_DEBUG_LEVEL_MID		0x494d
#define ANDROID_DEBUG_LEVEL_HIGH	0x4948

#ifdef CONFIG_RESTART_REASON_SEC_PARAM
extern void sec_param_restart_reason(const char *cmd);
#endif

extern bool kernel_sec_set_debug_level(int level);
extern int kernel_sec_get_debug_level(void);

extern int ssr_panic_handler_for_sec_dbg(void);
__weak void dump_all_task_info(void);
__weak void dump_cpu_stat(void);
extern void emerg_pet_watchdog(void);
extern void do_msm_restart(enum reboot_mode reboot_mode, const char *cmd);
#define LOCAL_CONFIG_PRINT_EXTRA_INFO

/* #define CONFIG_SEC_DEBUG_SUMMARY */
#ifdef CONFIG_SEC_DEBUG_SUMMARY
#endif

#ifdef CONFIG_SEC_DEBUG_DOUBLE_FREE
extern void *kfree_hook(void *p, void *caller);
#endif

#ifdef CONFIG_USER_RESET_DEBUG

#ifdef CONFIG_USER_RESET_DEBUG_TEST
extern void force_thermal_reset(void);
extern void force_watchdog_bark(void);
#endif

#ifdef CONFIG_ARCH_MSM8952
#define SEC_DEBUG_EX_INFO_SIZE	(0x3f8)

struct sec_debug_reset_ex_info {
	u64 ktime;
	struct task_struct *tsk;
	char bug_buf[64];
	char panic_buf[64];
	u32 fault_addr[6];
	char pc[64];
	char lr[64];
	char backtrace[720];
}; // size SEC_DEBUG_EX_INFO_SIZE
#else
#define SEC_DEBUG_EX_INFO_SIZE	(0x400)

struct sec_debug_reset_ex_info {
	u64 ktime;
	struct task_struct *tsk;
	char bug_buf[64];
	char panic_buf[64];
	u64 fault_addr[6];
	char pc[64];
	char lr[64];
	char backtrace[700];
}; // size SEC_DEBUG_EX_INFO_SIZE

#endif

extern struct sec_debug_reset_ex_info *sec_debug_reset_ex_info;

extern struct sec_debug_summary *secdbg_summary;
extern int sec_debug_get_cp_crash_log(char *str);
extern void _sec_debug_store_backtrace(unsigned long where);
extern void sec_debug_store_bug_string(const char *fmt, ...);

static inline void sec_debug_store_pte(unsigned long addr, int idx)
{
	if (sec_debug_reset_ex_info) {
		if(idx == 0)
			memset(sec_debug_reset_ex_info->fault_addr, 0,
				sizeof(sec_debug_reset_ex_info->fault_addr));

		sec_debug_reset_ex_info->fault_addr[idx] = addr;
	}
}
#else // CONFIG_USER_RESET_DEBUG
static inline void sec_debug_store_pte(unsigned long addr, int idx)
{
}
#endif // CONFIG_USER_RESET_DEBUG

#if defined(CONFIG_FB_MSM_MDSS_SAMSUNG)
extern void samsung_mdss_image_dump(void);
#endif

#endif	/* SEC_DEBUG_H */
