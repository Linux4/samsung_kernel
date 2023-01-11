/* include/linux/sec-debug.h
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


#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/sched.h>

#ifdef CONFIG_SEC_DEBUG

union sec_debug_level_t {
	struct {
		u16 kernel_fault;
		u16 user_fault;
	} en;
	u32 uint_val;
};

struct sec_log_buffer {
	u32     sig;
	u32     start;
	u32     size;
	u8      data[0];
};

struct sec_debug_mmu_reg_t {
#if defined(CONFIG_ARM)
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
#elif defined(CONFIG_ARM64)
	u64 ttbr0_el1;
	u64 ttbr1_el1;
	u64 tcr_el1;
#endif
};

/* ARM CORE regs mapping structure */
struct sec_debug_core_t {
#if defined(CONFIG_ARM)
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
#elif defined(CONFIG_ARM64)
	u64 regs[31];
	u64 sp_el1;
	u64 pc;
	u64 spsr_el1;
	u64 elr_el1;
	u64 esr_el1;
	u64 sp_el0;
	u64 pstate;
#endif
};

/* core reg dump function*/
static inline void sec_debug_save_core_reg(struct sec_debug_core_t *core_reg)
{
	/* we will be in SVC mode when we enter this function. Collect
	SVC registers along with cmn registers. */
#if defined(CONFIG_ARM)
	asm(
		"str r0, [%0,#0]\n\t"	/* R0 is pushed first to core_reg */
		"mov r0, %0\n\t"	/* R0 will be alias for core_reg */
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
#elif defined(CONFIG_ARM64)
	asm volatile(
			/* X0 ~ X30, SP_EL1 */
			"stp x0, x1, [%0, #8 * 0]\n\t"
			"stp x2, x3, [%0, #8 * 2]\n\t"
			"stp x4, x5, [%0, #8 * 4]\n\t"
			"stp x6, x7, [%0, #8 * 6]\n\t"
			"stp x8, x9, [%0, #8 * 8]\n\t"
			"stp x10, x11, [%0, #8 * 10]\n\t"
			"stp x12, x13, [%0, #8 * 12]\n\t"
			"stp x14, x15, [%0, #8 * 14]\n\t"
			"stp x16, x17, [%0, #8 * 16]\n\t"
			"stp x18, x19, [%0, #8 * 18]\n\t"
			"stp x20, x21, [%0, #8 * 20]\n\t"
			"stp x22, x23, [%0, #8 * 22]\n\t"
			"stp x24, x25, [%0, #8 * 24]\n\t"
			"stp x26, x27, [%0, #8 * 26]\n\t"
			"stp x28, x29, [%0, #8 * 28]\n\t"
			"mov x9, sp\n\t"
			"stp x30, x9, [%0, #8 * 30]\n\t"
			/* PC, SPSR_EL1 */
			"adr x10, 1f\n\t"
			"mrs x11, SPSR_EL1\n\t"
			"bic x11, x11, #0xFFFFFFFF00000000\n\t"
			"stp x10, x11, [%0, #8 * 32]\n\t"
			/* ELR_EL1, ESR_EL1 */
			"mrs x12, ELR_EL1\n\t"
			"mrs x13, ESR_EL1\n\t"
			"bic x13, x13, #0xFFFFFFFF00000000\n\t"
			"stp x12, x13, [%0, #8 * 34]\n\t"
			/* SP_EL0, PSTATE */
			"mrs x14, SP_EL0\n\t"
			"mrs x15, NZCV\n\t"
			"bic x15, x15, #0xFFFFFFFF0FFFFFFF\n\t"
			"mrs x9, DAIF\n\t"
			"bic x9, x9, #0xFFFFFFFFFFFFFC3F\n\t"
			"orr x15, x15, x9\n\t"
			"mrs x10, CurrentEL\n\t"
			"bic x10, x10, #0xFFFFFFFFFFFFFFF3\n\t"
			"orr x15, x15, x10\n\t"
			"mrs x11, SPSel\n\t"
			"bic x11, x11, #0xFFFFFFFFFFFFFFFE\n\t"
			"orr x15, x15, x11\n\t"
			"stp x14, x15, [%0, #8 * 36]\n\t"
		"1:"
			:
			: "r"(core_reg)
			: "memory"
	);
#endif
	return;
}

static inline void sec_debug_save_mmu_reg(struct sec_debug_mmu_reg_t *mmu_reg)
{
#if defined(CONFIG_ARM)
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
#elif defined(CONFIG_ARM64)
	asm volatile (
		"mrs x9, ttbr0_el1\n\t"
		"mrs x10, ttbr1_el1\n\t"
		"stp x9, x10, [%0, #8 * 0]\n\t"
		"mrs x11, TCR_EL1\n\t"
		"str x11, [%0, #8 * 2]\n\t"
 		:
		: "r"(mmu_reg)
		: "memory"
	);

#endif
}

extern int sec_debug_panic;
extern int sec_crash_key_panic;
extern union sec_debug_level_t sec_debug_level;
extern int ftracelogging;

extern int sec_debug_init(void);
extern void sec_debug_check_crash_key(unsigned int code, int value);
extern int sec_debug_panic_dump(char *buf);
extern void sec_getlog_supply_loggerinfo(unsigned char *buffer,
		const char *name);
extern void sec_getlog_supply_fbinfo(void *p_fb, u32 xres, u32 yres, u32 bpp,
		u32 frames);
extern void sec_gaf_supply_rqinfo(unsigned short curr_offset,
		unsigned short rq_offset);
extern void drain_mc_buffer(void);
extern void stop_ftracing(void);
extern void _print_ftrace(const char *fmt, ...);
#define print_ftrace(fmt...) \
	do { \
		if (ftracelogging == 1) \
			_print_ftrace(fmt); \
	} while(0)
#endif	/* CONFIG_SEC_DEBUG */

#ifdef CONFIG_SEC_LOG
extern void sec_getlog_supply_kloginfo(void *klog_buf, void *last_kmsg_buf);
#endif

#endif	/* SEC_DEBUG_H */
