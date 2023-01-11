/*
 *  linux/arch/arm/mach-pxa/ramdump.c
 *
 *  Support for the Marvell PXA RAMDUMP error handling capability.
 *
 *  Author:     Anton Eidelman (anton.eidelman@marvell.com)
 *  Created:    May 20, 2010
 *  Copyright:  (C) Copyright 2006 Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/notifier.h>
#include <linux/crc32.h>
#include <linux/slab.h>
#include <linux/delay.h>
#ifndef CONFIG_ARM64
#include <asm/system.h>
#endif
#include <linux/ptrace.h> /*pt_regs*/
#include <linux/sched.h> /* task_struct */
#include <asm/cacheflush.h>
#include <asm/system_misc.h> /* arm_pm_restart */
#include <linux/ramdump.h>
#ifndef CONFIG_ARM64
#include <mach/hardware.h>
#include <mach/regs-apmu.h>
#endif
#ifdef CONFIG_PXA_MIPSRAM
#include <linux/mipsram.h>
#endif

#include "ramdump_defs.h" /* common definitions reused in OSL */

static const char *panic_str; /* set this to panic text */

#ifdef CONFIG_KALLSYMS
/*
 * These will be re-linked against their real values
 * during the second link stage.
 */
extern const unsigned long kallsyms_addresses[] __attribute__((weak));
extern const u8 kallsyms_names[] __attribute__((weak));

/*
 * Tell the compiler that the count isn't in the small data section if the arch
 * has one (eg: FRV).
 */
extern const unsigned long kallsyms_num_syms
__attribute__((weak, section(".rodata")));

extern const u8 kallsyms_token_table[] __attribute__((weak));
extern const u16 kallsyms_token_index[] __attribute__((weak));

extern const unsigned long kallsyms_markers[] __attribute__((weak));
static struct kallsyms_record records;
#endif

#ifndef CONFIG_ARM64
/* CPU mode registers */
struct mode_regs {
	unsigned spsr;
	unsigned sp;
	unsigned lr;
};
struct usr_regs {
	unsigned sp;
	unsigned lr;
};
struct fiq_regs {
	unsigned spsr;
	unsigned r8;
	unsigned r9;
	unsigned r10;
	unsigned r11;
	unsigned r12;
	unsigned sp;
	unsigned lr;
};

/* CP15 registers */
struct cp15_regs {
	unsigned id;		/* CPU ID */
	unsigned cr;		/* Control */
	unsigned aux_cr;	/* Auxiliary Control */
	unsigned ttb;		/* TTB; PJ4: ttb0 */
	unsigned da_ctrl;	/* Domain Access Control */
	unsigned cpar;		/* Co-processor access control */
	unsigned fsr;		/* PJ4: DFSR */
	unsigned far;		/* PJ4: DFAR */
	unsigned procid;	/* Process ID; PJ4: Context ID */
};

/* CP14 registers */
struct cp14_regs {
	unsigned ccnt;
	unsigned pmnc;
};

/* CP7 registers (L2C/BIU errors)*/
struct cp7_regs {
	unsigned errlog;
	unsigned erradrl;
	unsigned erradru;
};

/* CP6 registers (Interrupt Controller) */
struct cp6_regs {
	unsigned icip;
	unsigned icmr;
	unsigned iclr;
	unsigned icfp;
	unsigned icpr;
	unsigned ichp;
	unsigned icip2;
	unsigned icmr2;
	unsigned iclr2;
	unsigned icfp2;
	unsigned icpr2;
	unsigned icip3;
	unsigned icmr3;
	unsigned iclr3;
	unsigned icfp3;
	unsigned icpr3;
};

/* ARMV7 specific cp15 regs. DONT EXTEND, ADD NEW STRUCTURE TO ramdump_state */
struct cp15_regs_pj4 {
	unsigned seccfg;	/* Secure Configuration */
	unsigned secdbg;	/* Secure Debug Enable */
	unsigned nsecac;	/* Non-secure Access Control */
	unsigned ttb1;		/* TTB1; TTB0 is cp15_regs.ttb */
	unsigned ttbc;		/* TTB Control */
	unsigned ifsr;		/* Instruction FSR; Data FSR is cp15_regs.fsr */
	unsigned ifar;		/* Instruction FAR; Data FAR is cp15_regs.far */
	unsigned auxdfsr;	/* Auxiliary DFSR */
	unsigned auxifsr;	/* Auxiliary IFSR */
	unsigned pa;		/* PA: physical address after translation */
	unsigned prremap;	/* Primary Region Remap */
	unsigned nmremap;	/* Normal Memory Remap */
	unsigned istat;		/* Interrupt Status */
	unsigned fcsepid;	/* FCSE PID */
	unsigned urwpid;	/* User Read/Write Thread and Process ID */
	unsigned uropid;	/* User Read/Only Thread and Process ID */
	unsigned privpid;	/* Priviledged Only Thread and Process ID */
	unsigned auxdmc0;	/* Auxiliary Debug Modes Control 0 */
	unsigned auxdmc1;	/* Auxiliary Debug Modes Control 1 */
	unsigned auxfmc;	/* Auxiliary Functional Modes Control */
	unsigned idext;		/* CPU ID code extension */
};

struct l2c_pj4_regs { /* 3 words */
	unsigned mpidr;
	unsigned reserved1;
	unsigned reserved2;
};

/* ARMV7 performance monitor */
struct pfm_pj4_regs {
	unsigned ctrl;
	unsigned ceset;
	unsigned ceclr;
	unsigned ovf;
	unsigned softinc;
	unsigned csel;
	unsigned ccnt;
	unsigned evsel;
	unsigned pmcnt;
	unsigned uen;
	unsigned ieset;
	unsigned ieclr;
};

/* ACCU and APMU registers: 8 words only */
struct acc_regs {
#ifdef CONFIG_CPU_PXA1986
	unsigned apmu_pcr[5];
	unsigned apmu_psr;
	unsigned apmu_core_status;
	unsigned apmu_core_idle_status;
#else
	unsigned reserved[8];
#endif
};

/* Other SoC registers */
struct soc_regs {
	unsigned reserved1;
	unsigned ser_fuse_reg2;
	union {
		struct {
			unsigned reserved2;
			unsigned reserved3;
		};
	};
	unsigned oem_unique_id_l;
	unsigned oem_unique_id_h;
};
#else
/* EL1 bank SPRSs */
struct spr_regs {
	u32	midr;
	u32	revidr;
	u32	current_el;
	u32	sctlr;
	u32	actlr;
	u32	cpacr;
	u32	isr;
	u64	tcr;
	u64	ttbr0;
	u64	ttbr1;
	u64	mair;
	u64	tpidr;
	u64	vbar;
	u32	esr;
	u32	reserved1;
	u64	far;
};
struct soc_regs {
	unsigned	reserved1;
};
#endif

/* Main RAMDUMP data structure */
#define RAMDUMP_TXT_SIZE   100
struct ramdump_state {
	unsigned reserved; /* was rdc_va - RDC header virtual addres */
	unsigned rdc_pa;	/* RDC header physical addres */
	char text[RAMDUMP_TXT_SIZE];
	unsigned err;
	struct pt_regs regs;	/* saved context */
	struct thread_info *thread;
#ifndef CONFIG_ARM64
	struct mode_regs svc;
	struct usr_regs usr;
	struct mode_regs abt;
	struct mode_regs und;
	struct mode_regs irq;
	struct fiq_regs fiq;
	struct cp15_regs cp15;
	/* Up to this point same structure for XSC and PJ4 */
	union {
		struct {			/* 21 total */
			struct cp14_regs cp14;	/* 2 */
			struct cp6_regs cp6;	/* 16 */
			struct cp7_regs cp7;	/* 3 */
		}; /* XSC */
		struct {			/* 21 total */
			struct cp15_regs_pj4 cp15pj4;
		}; /* PJ4 */
	};
	struct acc_regs acc;
	struct l2c_pj4_regs l2cpj4;
	struct pfm_pj4_regs pfmpj4;
	struct soc_regs soc;
#else
	unsigned		spr_size;
	struct spr_regs		spr;
	unsigned		soc_size;
	struct soc_regs		soc;
#endif
} ramdump_data;

static void *isram_va; /* ioremapped va for ISRAM access */
static struct rdc_area *rdc_va;/* ioremapped va for rdc_area access */
/************************************************************************/
/*				Internal prototypes			*/
/************************************************************************/
static void ramdump_save_static_context(struct ramdump_state *d);
static void ramdump_save_current_context(struct ramdump_state *d);
static void ramdump_save_isram(void);
static void ramdump_flush_caches(void);
static void ramdump_fill_rdc(void);
static void save_peripheral_regs(struct ramdump_state *d);

/************************************************************************/
/*				RDC address setup			*/
/************************************************************************/
static unsigned long rdc_pa;

static int __init ramdump_rdc_setup(char *str)
{
	if (kstrtoul(str, 16, &rdc_pa))
		return 0;
	return 1;
}
__setup("RDCA=", ramdump_rdc_setup);

#if defined(CONFIG_SMP) && !defined(CONFIG_MRVL_PANIC_FLUSH)
static atomic_t waiting_for_cpus;
/* Based on CONFIG_KEXEC code see arch/arm/kernel/machine_kexec.c */
void ramdump_other_cpu_handler(void *parm)
{
	(void)parm;
	pr_err("RAMDUMP: CPU %u stops\n", smp_processor_id());
	dump_stack();
	flush_cache_all();
	atomic_dec(&waiting_for_cpus);
	/*
	 * should return, otherwise csd locks will prevent any further
	 * smp calls, which might be attempted by other error handlers.
	 */
}

void ramdump_signal_all_cpus(void)
{
	int cpu;
	unsigned long msecs;
	struct cpumask mask;
	pr_err("RAMDUMP: Signalling %d CPUs\n",
		num_online_cpus() - 1);

	cpumask_copy(&mask, cpu_online_mask);
	cpumask_clear_cpu(smp_processor_id(), &mask);

	for_each_cpu(cpu, &mask) {
		atomic_set(&waiting_for_cpus, 1);
		smp_call_function_single(cpu, ramdump_other_cpu_handler,
								NULL, 0);
		msecs = 10;
		while ((atomic_read(&waiting_for_cpus) > 0) && msecs) {
			mdelay(1);
			msecs--;
		}
		if (!msecs)
			pr_err("Waiting for other CPUs timed out\n");
	}
}
#endif

/************************************************************************/
/*				RAMDUMP panic notifier			*/
/************************************************************************/
void ramdump_panic(void)
{
	pr_err("RAMDUMP STARTED\n");
	ramdump_fill_rdc();
	ramdump_save_current_context(&ramdump_data);
	ramdump_save_static_context(&ramdump_data);
	ramdump_save_isram();
#ifndef CONFIG_MRVL_PANIC_FLUSH
#ifdef CONFIG_SMP
	ramdump_signal_all_cpus();
#endif
	ramdump_flush_caches();
	pr_err("RAMDUMP DONE\n");
	/* Reset right away, do not return. Two issues with reset done
	by the main panic() implementation:
	1) The setup_mm_for_reboot() called from arm_machine_restart()
	corrupts the current MM page tables,
	which is bad for offline debug.
	2) The current kernel stack is corrupted by other functions
	invoked after this returns:
	the stack frame becomes invalid in offline debug.
	*/
	arm_pm_restart('h', NULL);
#else
	pr_err("RAMDUMP DONE\n");
#endif
}

/* Prepare RAMDUMP "in advance"
* So in case of stuck the GPIO-reset the OBM would detect the RAMDUMP
* it would be executed
* The context is also saved but may be irrelevant for problem.
* Use this API carefully!
*/
void ramdump_prepare_in_advance(void)
{
	unsigned long flags;
	ramdump_fill_rdc();
	local_irq_save(flags);
	ramdump_save_current_context(&ramdump_data);
	ramdump_save_static_context(&ramdump_data);
#ifdef CONFIG_ARM64
	__flush_dcache_area(&ramdump_data, sizeof(ramdump_data));
#else
	__cpuc_flush_dcache_area(&ramdump_data, sizeof(ramdump_data));
#endif
	local_irq_restore(flags);
}
EXPORT_SYMBOL(ramdump_prepare_in_advance);

void ramdump_rdc_reset(void)
{
	/* Clean RDC_SIGNATURE only, keep others
	 * IO-remapped => non-cached
	 */
	rdc_va->header.signature = 0;
}
EXPORT_SYMBOL(ramdump_rdc_reset);

#ifndef CONFIG_MRVL_PANIC_FLUSH
static int
ramdump_panic_notifier(struct notifier_block *this,
						unsigned long event, void *ptr)
{
	ramdump_panic();
	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call = ramdump_panic,
};
#endif

static void save_peripheral_regs(struct ramdump_state *d)
{
#ifdef CONFIG_CPU_PXA1986
	d->acc.apmu_pcr[0] = __raw_readl(APMU_REG(APMU_PCR_0));
	d->acc.apmu_pcr[1] = __raw_readl(APMU_REG(APMU_PCR_1));
	d->acc.apmu_pcr[2] = __raw_readl(APMU_REG(APMU_PCR_2));
	d->acc.apmu_pcr[3] = __raw_readl(APMU_REG(APMU_PCR_3));
	d->acc.apmu_pcr[4] = __raw_readl(APMU_REG(APMU_PCR_4));
	d->acc.apmu_psr		= __raw_readl(APMU_REG(APMU_PSR));
	d->acc.apmu_core_status	= __raw_readl(APMU_REG(APMU_CORE_STATUS));
	d->acc.apmu_core_idle_status = __raw_readl(APMU_REG(APMU_CORE_IDLE_ST));
#endif
}

#ifndef CONFIG_ARM64
/************************************************************************/
/*				inline asm helpers			*/
/************************************************************************/
#define get_reg_asm(instruction) ({	\
	unsigned reg;			\
	asm(instruction : "=r" (reg) : );	\
	reg; })

static inline void get_banked_regs(unsigned *dest, unsigned mode)
{
	register unsigned *rdest asm("r0") = dest;
	register unsigned rmode asm("r1") = mode;
	register unsigned cpsr asm("r2");
	register unsigned scr asm("r3");
	asm volatile(
		"mrs	r2, cpsr\n"
		"bic	r3, r2, #0x1f @ clear mode\n"
		"orr	r3, r3, r1 @ set target mode\n"
		"orr	r3, r3, #0xc0 @ lockout irq/fiq or may trap on bad mode"
		"msr	cpsr, r3\n"
		"mrs	r3,spsr\n"
		"cmp	r2, #0x11\n"
#ifndef CONFIG_THUMB2_KERNEL
		"stmne	r0, {r3,r13-r14}\n"
		"stmeq	r0, {r3,r8-r14}\n"
#else
		"strne  r3, [r0]\n"
		"strne	r13, [r0, #4]\n"
		"strne  r14, [r0, #8]\n"
		"stmeq  r0, {r3,r8-r12}\n"
		"streq  r13, [r0, #0x18]\n"
		"streq  r14, [r0, #0x1c]\n"
#endif
		"msr	cpsr, r2 @ restore original mode\n"
		: "=r" (cpsr), "=r" (scr)
		: "r" (rdest), "r" (rmode), "r" (cpsr), "r" (scr)
		: "memory", "cc");
}
static inline void get_usr_regs(unsigned *dest, unsigned mode)
{
#ifndef CONFIG_THUMB2_KERNEL
/* TBD: how to get user mode registers in thumb 2 */
	register unsigned *rdest asm("r0") = dest;
	register unsigned rmode asm("r1") = mode;
	register unsigned cpsr asm("r2");
	register unsigned scr asm("r3");
	asm volatile(
		"mrs	r2, spsr\n"
		"bic	r3, r2, #0x1f @ clear mode\n"
		"orr	r3, r3, r1 @ set usr mode\n"
		"msr	spsr, r3\n"
		"stm	r0, {r13-r14}^\n"
		"msr	spsr, r2 @ restore original spsr\n"
		: "=r" (cpsr), "=r" (scr)
		: "r" (rdest), "r" (rmode), "r" (cpsr), "r" (scr)
		: "memory", "cc");
#endif
}

/************************************************************************/
/*				RAMDUMP state save			*/
/************************************************************************/
/*
	ramdump_save_static_context
	Saves general CPU registers state into the ramdump.
*/
static void ramdump_save_static_context(struct ramdump_state *d)
{
	/* mode banked regs */
	get_banked_regs(&d->abt.spsr, ABT_MODE);
	get_banked_regs(&d->und.spsr, UND_MODE);
	get_banked_regs(&d->irq.spsr, IRQ_MODE);
	get_banked_regs(&d->fiq.spsr, FIQ_MODE);
	get_banked_regs(&d->svc.spsr, SVC_MODE);

	/* USR mode banked regs */
	get_usr_regs(&d->usr.sp, USR_MODE);

	/* cp15 */
	d->cp15.id	=	get_reg_asm("mrc p15, 0, %0, c0, c0, 0");
	d->cp15.cr	=	get_reg_asm("mrc p15, 0, %0, c1, c0, 0");
	d->cp15.aux_cr	=	get_reg_asm("mrc p15, 0, %0, c1, c0, 1");
	d->cp15.ttb	=	get_reg_asm("mrc p15, 0, %0, c2, c0, 0");
	d->cp15.da_ctrl	=	get_reg_asm("mrc p15, 0, %0, c3, c0, 0");
#ifndef CONFIG_CPU_V7 /* XSC */
	d->cp15.cpar	=	get_reg_asm("mrc p15, 0, %0, c15,c1, 0");
#else /* PJ4 */
	d->cp15.cpar	=	get_reg_asm("mrc p15, 0, %0, c1, c0, 2");
#endif
	d->cp15.fsr	=	get_reg_asm("mrc p15, 0, %0, c5, c0, 0");
	d->cp15.far	=	get_reg_asm("mrc p15, 0, %0, c6, c0, 0");
	/* PJ4: context id */
	d->cp15.procid	=	get_reg_asm("mrc p15, 0, %0, c13,c0, 0");

#ifndef CONFIG_CPU_V7 /* XSC */
	/* cp14 */
	d->cp14.ccnt	=	get_reg_asm("mrc p14, 0, %0, c1, c1, 0");
	d->cp14.pmnc	=	get_reg_asm("mrc p14, 0, %0, c0, c1, 0");

	/* cp6 */
	d->cp6.icip	=	get_reg_asm("mrc p6, 0, %0, c0, c0, 0");
	d->cp6.icmr	=	get_reg_asm("mrc p6, 0, %0, c1, c0, 0");
	d->cp6.iclr	=	get_reg_asm("mrc p6, 0, %0, c2, c0, 0");
	d->cp6.icfp	=	get_reg_asm("mrc p6, 0, %0, c3, c0, 0");
	d->cp6.icpr	=	get_reg_asm("mrc p6, 0, %0, c4, c0, 0");
	d->cp6.ichp	=	get_reg_asm("mrc p6, 0, %0, c5, c0, 0");
	d->cp6.icip2	=	get_reg_asm("mrc p6, 0, %0, c6, c0, 0");
	d->cp6.icmr2	=	get_reg_asm("mrc p6, 0, %0, c7, c0, 0");
	d->cp6.iclr2	=	get_reg_asm("mrc p6, 0, %0, c8, c0, 0");
	d->cp6.icfp2	=	get_reg_asm("mrc p6, 0, %0, c9, c0, 0");
	d->cp6.icpr2	=	get_reg_asm("mrc p6, 0, %0, c10, c0, 0");
	d->cp6.icip3	=	get_reg_asm("mrc p6, 0, %0, c11, c0, 0");
	d->cp6.icmr3	=	get_reg_asm("mrc p6, 0, %0, c12, c0, 0");
	d->cp6.iclr3	=	get_reg_asm("mrc p6, 0, %0, c13, c0, 0");
	d->cp6.icfp3	=	get_reg_asm("mrc p6, 0, %0, c14, c0, 0");
	d->cp6.icpr3	=	get_reg_asm("mrc p6, 0, %0, c15, c0, 0");

	/* cp7 */
	d->cp7.errlog	=	get_reg_asm("mrc p7, 0, %0, c0, c2, 0");
	d->cp7.erradrl	=	get_reg_asm("mrc p7, 0, %0, c1, c2, 0");
	d->cp7.erradru	=	get_reg_asm("mrc p7, 0, %0, c2, c2, 0");
#else /* ARMV7 */
	/*
	 * Removed SCR, NSACR, NSDBG: not accessible in NS,
	 * and not relevant otherwise.
	 */
	d->cp15pj4.ttb1		= get_reg_asm("mrc p15, 0, %0, c2, c0, 1");
	d->cp15pj4.ttbc		= get_reg_asm("mrc p15, 0, %0, c2, c0, 2");
	d->cp15pj4.ifsr		= get_reg_asm("mrc p15, 0, %0, c5, c0, 1");
	d->cp15pj4.ifar		= get_reg_asm("mrc p15, 0, %0, c6, c0, 2");
	d->cp15pj4.auxdfsr	= get_reg_asm("mrc p15, 0, %0, c5, c1, 0");
	d->cp15pj4.auxifsr	= get_reg_asm("mrc p15, 0, %0, c5, c1, 1");
	d->cp15pj4.pa		= get_reg_asm("mrc p15, 0, %0, c7, c4, 0");
	d->cp15pj4.prremap	= get_reg_asm("mrc p15, 0, %0, c10, c2, 0");
	d->cp15pj4.nmremap	= get_reg_asm("mrc p15, 0, %0, c10, c2, 1");
	d->cp15pj4.istat	= get_reg_asm("mrc p15, 0, %0, c12, c1, 0");
	d->cp15pj4.fcsepid	= get_reg_asm("mrc p15, 0, %0, c13, c0, 0");
	d->cp15pj4.urwpid	= get_reg_asm("mrc p15, 0, %0, c13, c0, 2");
	d->cp15pj4.uropid	= get_reg_asm("mrc p15, 0, %0, c13, c0, 3");
	d->cp15pj4.privpid	= get_reg_asm("mrc p15, 0, %0, c13, c0, 4");
#ifdef CONFIG_CPU_PJ4
	/* PJ4 specifics */
	d->cp15pj4.auxdmc0	= get_reg_asm("mrc p15, 1, %0, c15, c1, 0");
	d->cp15pj4.auxdmc1	= get_reg_asm("mrc p15, 1, %0, c15, c1, 1");
	d->cp15pj4.auxfmc	= get_reg_asm("mrc p15, 1, %0, c15, c2, 0");
	d->cp15pj4.idext	= get_reg_asm("mrc p15, 1, %0, c15, c12, 0");
	d->l2cpj4.l2errcnt	= get_reg_asm("mrc p15, 1, %0, c15, c9, 6");
	d->l2cpj4.l2errth	= get_reg_asm("mrc p15, 1, %0, c15, c9, 7");
	d->l2cpj4.l2errcapt	= get_reg_asm("mrc p15, 1, %0, c15, c11, 7");
#endif
	d->pfmpj4.ctrl		= get_reg_asm("mrc p15, 0, %0, c9, c12, 0");
	d->pfmpj4.ceset		= get_reg_asm("mrc p15, 0, %0, c9, c12, 1");
	d->pfmpj4.ceclr		= get_reg_asm("mrc p15, 0, %0, c9, c12, 2");
	d->pfmpj4.ovf		= get_reg_asm("mrc p15, 0, %0, c9, c12, 3");
#ifdef CONFIG_CPU_PJ4
	/* Write-only: on CA9 results in undefined instruction exception */
	d->pfmpj4.softinc	= get_reg_asm("mrc p15, 0, %0, c9, c12, 4");
#endif
	d->pfmpj4.csel		= get_reg_asm("mrc p15, 0, %0, c9, c12, 5");
	d->pfmpj4.ccnt		= get_reg_asm("mrc p15, 0, %0, c9, c13, 0");
	d->pfmpj4.evsel		= get_reg_asm("mrc p15, 0, %0, c9, c13, 1");
	d->pfmpj4.pmcnt		= get_reg_asm("mrc p15, 0, %0, c9, c13, 2");
	d->pfmpj4.uen		= get_reg_asm("mrc p15, 0, %0, c9, c14, 0");
	d->pfmpj4.ieset		= get_reg_asm("mrc p15, 0, %0, c9, c14, 1");
	d->pfmpj4.ieclr		= get_reg_asm("mrc p15, 0, %0, c9, c14, 2");

#endif
	save_peripheral_regs(d);
}

static void ramdump_save_current_cpu_context(struct ramdump_state *d)
{
	/* check if panic was called directly, then regs will be empty */
	if (d->regs.uregs[16] == 0) {
		/* let's fill up regs as current */
		d->regs.uregs[0] = 0xDEADDEAD; /* use DEADDEAD as a marking */
		d->regs.uregs[1] = get_reg_asm("mov %0, r1");
		d->regs.uregs[2] = get_reg_asm("mov %0, r2");
		d->regs.uregs[3] = get_reg_asm("mov %0, r3");
		d->regs.uregs[4] = get_reg_asm("mov %0, r4");
		d->regs.uregs[5] = get_reg_asm("mov %0, r5");
		d->regs.uregs[6] = get_reg_asm("mov %0, r6");
		d->regs.uregs[7] = get_reg_asm("mov %0, r7");
		d->regs.uregs[8] = get_reg_asm("mov %0, r8");
		d->regs.uregs[9] = get_reg_asm("mov %0, r9");
		d->regs.uregs[10] = get_reg_asm("mov %0, r10");
		d->regs.uregs[11] = get_reg_asm("mov %0, r11");
		d->regs.uregs[12] = get_reg_asm("mov %0, r12");
		d->regs.uregs[13] = get_reg_asm("mov %0, r13");
		d->regs.uregs[14] = get_reg_asm("mov %0, r14");
		d->regs.uregs[15] = get_reg_asm("mov %0, r15");
		d->regs.uregs[16] = get_reg_asm("mrs %0, cpsr");
	}
}
#else /* CONFIG_ARM64 */
#define get_reg_asm(instruction) ({	\
	u64 reg;			\
	asm(instruction : "=r" (reg) : );	\
	reg; })
/*
	ramdump_save_static_context
	Saves general CPU registers state into the ramdump.
*/
static void ramdump_save_static_context(struct ramdump_state *d)
{
	d->spr.midr = get_reg_asm("mrs %0, midr_el1");
	d->spr.revidr = get_reg_asm("mrs %0, revidr_el1");
	d->spr.sctlr = get_reg_asm("mrs %0, sctlr_el1");
	d->spr.actlr = get_reg_asm("mrs %0, actlr_el1");
	d->spr.cpacr = get_reg_asm("mrs %0, cpacr_el1");
	d->spr.isr = get_reg_asm("mrs %0, isr_el1");
	d->spr.tcr = get_reg_asm("mrs %0, tcr_el1");
	d->spr.ttbr0 = get_reg_asm("mrs %0, ttbr0_el1");
	d->spr.ttbr1 = get_reg_asm("mrs %0, ttbr1_el1");
	d->spr.mair = get_reg_asm("mrs %0, mair_el1");
	d->spr.tpidr = get_reg_asm("mrs %0, tpidr_el1");
	d->spr.vbar = get_reg_asm("mrs %0, vbar_el1");
	d->spr.esr = get_reg_asm("mrs %0, esr_el1");
	d->spr.far = get_reg_asm("mrs %0, far_el1");
	d->spr.current_el = get_reg_asm("mrs %0, currentEL");
	/* Fill in the SPR and SOC size: needed for parser compatibility */
	d->spr_size = sizeof(d->spr);
	d->soc_size = sizeof(d->soc);
	/* Save SoC register state */
	save_peripheral_regs(d);
}
static void ramdump_save_current_cpu_context(struct ramdump_state *d)
{
	int i = 0;
	/* check if panic was called directly, then regs will be empty */
	if (d->regs.pc == 0) {
		/* let's fill up regs as current */
		d->regs.regs[i++] = 0xDEADDEAD; /* use DEADDEAD as a marking */
		d->regs.regs[i++] = get_reg_asm("mov %0, x1");
		d->regs.regs[i++] = get_reg_asm("mov %0, x2");
		d->regs.regs[i++] = get_reg_asm("mov %0, x3");
		d->regs.regs[i++] = get_reg_asm("mov %0, x4");
		d->regs.regs[i++] = get_reg_asm("mov %0, x5");
		d->regs.regs[i++] = get_reg_asm("mov %0, x6");
		d->regs.regs[i++] = get_reg_asm("mov %0, x7");
		d->regs.regs[i++] = get_reg_asm("mov %0, x8");
		d->regs.regs[i++] = get_reg_asm("mov %0, x9");
		d->regs.regs[i++] = get_reg_asm("mov %0, x10");
		d->regs.regs[i++] = get_reg_asm("mov %0, x11");
		d->regs.regs[i++] = get_reg_asm("mov %0, x12");
		d->regs.regs[i++] = get_reg_asm("mov %0, x13");
		d->regs.regs[i++] = get_reg_asm("mov %0, x14");
		d->regs.regs[i++] = get_reg_asm("mov %0, x15");
		d->regs.regs[i++] = get_reg_asm("mov %0, x16");
		d->regs.regs[i++] = get_reg_asm("mov %0, x17");
		d->regs.regs[i++] = get_reg_asm("mov %0, x18");
		d->regs.regs[i++] = get_reg_asm("mov %0, x19");
		d->regs.regs[i++] = get_reg_asm("mov %0, x20");
		d->regs.regs[i++] = get_reg_asm("mov %0, x21");
		d->regs.regs[i++] = get_reg_asm("mov %0, x22");
		d->regs.regs[i++] = get_reg_asm("mov %0, x23");
		d->regs.regs[i++] = get_reg_asm("mov %0, x24");
		d->regs.regs[i++] = get_reg_asm("mov %0, x25");
		d->regs.regs[i++] = get_reg_asm("mov %0, x26");
		d->regs.regs[i++] = get_reg_asm("mov %0, x27");
		d->regs.regs[i++] = get_reg_asm("mov %0, x28");
		d->regs.regs[i++] = get_reg_asm("mov %0, x29");
		d->regs.regs[i++] = get_reg_asm("mov %0, x30");
		d->regs.sp	  = get_reg_asm("mov %0, sp");
		d->regs.pstate	  = 0; /* no direct access */
		d->regs.pc	  = (u64)&ramdump_save_current_cpu_context;
	}
}

#define ARM_pc pc /* alias for pt_regs field in the code below */
#endif

/*
	Save current register context if no dynamic context has been filled in
*/
static void ramdump_save_current_context(struct ramdump_state *d)
{
	ramdump_save_current_cpu_context(d);
	if (d->thread == NULL)
		d->thread = current_thread_info();
	if (strlen(d->text) == 0) {
		strcpy(d->text, "[KR] Panic in ");
		if (d->thread && d->thread->task)
			/* 0 is always appended even after n chars copied */
			strncat(d->text, d->thread->task->comm,
				sizeof(d->text) - 1 - strlen(d->text));
	}
	if (panic_str) {
		strncat(d->text, ": ", sizeof(d->text) - 3);
		strncat(d->text, panic_str,
				sizeof(d->text) - 1 - strlen(d->text));
	}
}

/* */
void ramdump_save_panic_text(const char *s)
{
	panic_str = s;
}
/*
	ramdump_save_dynamic_context
	Saves register context (regs) into the ramdump.
*/
void ramdump_save_dynamic_context(const char *str, int err,
		struct thread_info *thread, struct pt_regs *regs)
{
	int len;
	const char *idtext = "??"; /* error type string */
	if (regs)
		ramdump_data.regs = *regs;
	if (err == RAMDUMP_ERR_EEH_CP)
		idtext = "CP";
	else if (err == RAMDUMP_ERR_EEH_AP)
		idtext = "AP";
	else if ((err & RAMDUMP_ERR_EEH_CP) == 0)
		idtext = "KR";

	len = sprintf(ramdump_data.text, "[%s] ", idtext);

	if (str)
		/* 0 is always appended even after n chars copied */
		strncat(ramdump_data.text, str,
				sizeof(ramdump_data.text) - 1 - len);
	ramdump_data.err = (unsigned)err;
	ramdump_data.thread = thread;
	if ((err&RAMDUMP_ERR_EEH_CP) == 0) {
		/* For kernel oops/panic add more data to text */
		char info[40];
		len = strlen(ramdump_data.text);
		sprintf(info, " at 0x%lx in ", (unsigned long)regs->ARM_pc);

		if (thread && thread->task)
			/* 0 is always appended even after n chars copied */
			strncat(info, thread->task->comm,
					sizeof(info) - 1 - strlen(info));

		len = sizeof(ramdump_data.text) - len;
		if (len > 0)
			/* 0 is always appended even after n chars copied */
			strncat(ramdump_data.text, info, len - 1);
	}
#ifdef CONFIG_PXA_MIPSRAM
	/*
	 * MIPS_RAM_MARK_END_OF_LOG(MIPSRAM_get_descriptor(), 0); may be best,
	 * but it us not safe in real panic.
	 * So add END-MARK which is absolutelly safety.
	 */
	MIPS_RAM_ADD_TRACE(MIPSRAM_LOG_END_MARK_EVENT);
#endif
}
EXPORT_SYMBOL(ramdump_save_dynamic_context);

/*
	ramdump_save_isram
	Saves the ISRAM contents into the RDC.
	Caches should be flushed prior to calling this (so SRAM is in sync).
	Cacheable access to both SRAM and DDR is used for performance reasons.
	Caches are flushed also by this function to sync the DDR.
*/
#define ISRAM_SIZE	0 /* Actually 128K, but we do not have room in RDC */
#define RDC_HEADROOM (ISRAM_SIZE)
#define RDC_START(header) ((void *)(((unsigned long)header)-RDC_HEADROOM))
#define RDC_HEAD(start) \
			((struct rdc_area *)(((unsigned long)start) \
			+ RDC_HEADROOM))
static void ramdump_save_isram(void)
{
	void *rdc_isram_va = RDC_ISRAM_START(rdc_va, ISRAM_SIZE);
	if (!isram_va)
		return;
	ramdump_flush_caches();
#ifdef RAMDUMP_ISRAM_CRC_CHECK
	rdc_va->header.isram_crc32 = crc32_le(0, (unsigned char *)isram_va,
		rdc_va->header.isram_size);
#else
	rdc_va->header.isram_crc32 = 0;
#endif
	memcpy(rdc_isram_va, isram_va, rdc_va->header.isram_size);
}

static void ramdump_flush_caches(void)
{
	flush_cache_all();
#ifdef CONFIG_OUTER_CACHE
	outer_flush_range(0, -1ul);
#endif
}

static void ramdump_fill_rdc(void)
{
	/* Some RDC fields are already set at this point: retain these */
	rdc_va->header.signature = RDC_SIGNATURE;
	rdc_va->header.error_id = ramdump_data.err;
	rdc_va->header.ramdump_data_addr =
			__virt_to_phys((unsigned long)&ramdump_data);
	rdc_va->header.isram_size = ISRAM_SIZE;
	rdc_va->header.isram_pa =
		(unsigned)(unsigned long)RDC_ISRAM_START(rdc_pa, ISRAM_SIZE);
	ramdump_data.rdc_pa = rdc_pa;
#ifdef CONFIG_PXA_MIPSRAM
	rdc_va->header.mipsram_pa = mipsram_desc.buffer_phys_ptr;
	rdc_va->header.mipsram_size = MIPS_RAM_BUFFER_SZ_BYTES;
#endif
}

/************************************************************************/
/*			RAMDUMP RAMFILE support				*/
/************************************************************************/
int ramdump_attach_ramfile(struct ramfile_desc *desc)
{
	/* Legacy: ramfiles linked list are physical:
	both head=rdc_va->header.ramfile_addr, and all next's.
	Kernel cannot always translate physical addresses,
	e.g. with vmalloc or himem. Just keep the virtual address
	of the last item next pointer. No need to walk the list then.*/
	static unsigned *link;

	if (!link)
		link = &rdc_va->header.ramfile_addr;
	/* Link in the new desc: by physical address */
	if (desc->flags & RAMFILE_PHYCONT)
		*link = __virt_to_phys((unsigned long)desc);
	else
		*link = vmalloc_to_pfn(desc)<<PAGE_SHIFT;

	/* keep virtual addr for discontinous pages (vmalloc) */
	desc->vaddr = (unsigned)(unsigned long)desc;
#ifdef CONFIG_ARM64
	desc->vaddr_hi = (unsigned)(((unsigned long)desc) >> 32);
#endif
	/* Make sure the new desc is properly ending the list */
	desc->next = 0;
	/* Finally: advance the last element pointer */
	link = &desc->next;
	return 0;
}
EXPORT_SYMBOL(ramdump_attach_ramfile);

static atomic_t ramdump_item_offset;
int ramdump_attach_item(enum rdi_type type,
			const char *name,
			unsigned attrbits,
			unsigned nwords, ...)
{
	struct rdc_dataitem *rdi;
	unsigned size;
	unsigned offset;
	va_list args;
	int	i;

	/* name should be present, and at most MAX_RDI_NAME chars */
	if (!name || (strlen(name) > sizeof(rdi->name)) || (nwords > 16))
		return -EINVAL;
	size = offsetof(struct rdc_dataitem, body)
		+ nwords * sizeof(unsigned long);
	offset = atomic_read(&ramdump_item_offset);
	if ((offset + size) > sizeof(rdc_va->body))
		return -ENOMEM;
	offset = atomic_add_return(size, &ramdump_item_offset);
	if (offset > sizeof(rdc_va->body)) {
		atomic_sub_return(size, &ramdump_item_offset);
		return -ENOMEM;
	}

	/* Allocated space: set pointer */
	offset -= size;
	rdi = (struct rdc_dataitem *)&rdc_va->body.space[offset];
	rdi->size	= size;
	rdi->type	= type;
	rdi->attrbits	= attrbits;
	strncpy(rdi->name, name, sizeof(rdi->name));
	va_start(args, nwords);
	/* Copy the data words */
	for (i = 0; nwords; nwords--) {
		unsigned long w = va_arg(args, unsigned long);
		rdi->body.w[i++] = (unsigned)w;
#ifdef CONFIG_ARM64
		rdi->body.w[i++] = (unsigned)(w>>32);
#endif
	}
	va_end(args);
	return 0;
}
EXPORT_SYMBOL(ramdump_attach_item);
/************************************************************************/
/*				RAMDUMP init				*/
/************************************************************************/

static int __init ramdump_init(void)
{
	void *p;
	if (!rdc_pa)
		/*
		 * RDC address is not overriden on CMDLINE: assume RAM at 0
		 * with the legacy offset for backward compatibility.
		 */
		rdc_pa = RDC_OFFSET;
	if (ISRAM_SIZE) {
		isram_va = ioremap_wc(ISRAM_PA, ISRAM_SIZE);
		if (!isram_va)
			return -ENOMEM;
	}

	if (pfn_valid(rdc_pa >> PAGE_SHIFT))
		rdc_va = (struct rdc_area *)phys_to_virt(rdc_pa);
	else
		rdc_va = (struct rdc_area *)ioremap_nocache(
				(unsigned long)RDC_START(rdc_pa),
				sizeof(struct rdc_area)+RDC_HEADROOM);
	if (!rdc_va) {
		if (isram_va)
			iounmap(isram_va);
		return -ENOMEM;
	} else
		rdc_va = RDC_HEAD(rdc_va);

	/* Set up certain RDC fields that won't change later.
	These will be available even when dump is taken after a forced reset */
	memset((void *)rdc_va, 0, sizeof(*rdc_va)); /* zero reserved fields */
	/* init_mm.pgd, so rdp can translate vmalloc addresses */
	rdc_va->header.pgd = __virt_to_phys((unsigned long)(pgd_offset_k(0)));
#ifdef CONFIG_KALLSYMS
	records.kallsyms_addresses = (unsigned long)kallsyms_addresses;
	records.kallsyms_names = (unsigned long)kallsyms_names;
	records.kallsyms_num_syms = (unsigned)kallsyms_num_syms;
	records.kallsyms_token_table = (unsigned long)kallsyms_token_table;
	records.kallsyms_token_index = (unsigned long)kallsyms_token_index;
	records.kallsyms_markers = (unsigned long)kallsyms_markers;
	rdc_va->header.kallsyms = (unsigned)(unsigned long)&records;
#ifdef CONFIG_ARM64
	rdc_va->header.kallsyms_hi = (unsigned)(((unsigned long)&records)>>32);
#endif
#endif

	/* Set up a copy of linux version string to identify kernel version */
	rdc_va->header.kernel_build_id = 0;
	rdc_va->header.kernel_build_id_hi = 0;
	p = kmalloc(strlen(linux_banner) + 1, GFP_KERNEL);
	if (p) {
		strcpy(p, linux_banner);
		/*
		 * RDC is a reserved area, so kmemleak does not scan it
		 * and reports this allocation as a leak.
		 */
		kmemleak_ignore(p);
		rdc_va->header.kernel_build_id = (unsigned)(unsigned long)p;
#ifdef CONFIG_ARM64
		rdc_va->header.kernel_build_id_hi =
			(unsigned)(((unsigned long)p)>>32);
#endif
	}
#ifndef CONFIG_MRVL_PANIC_FLUSH
	/* Otherwise panic_flush() will call us directly */
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);
#endif
	return 0;
}
core_initcall(ramdump_init); /*TBD: option early_initcall*/
