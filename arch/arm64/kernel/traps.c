/*
 * Based on arch/arm/kernel/traps.c
 *
 * Copyright (C) 1995-2009 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/signal.h>
#include <linux/personality.h>
#include <linux/kallsyms.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/hardirq.h>
#include <linux/kdebug.h>
#include <linux/module.h>
#include <linux/kexec.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include <asm/atomic.h>
#include <asm/debug-monitors.h>
#include <asm/insn.h>
#include <asm/traps.h>
#include <asm/stacktrace.h>
#include <asm/exception.h>
#include <asm/system_misc.h>
#include <asm/esr.h>
#ifdef CONFIG_SEC_DEBUG
#include <linux/sec-debug.h>
#include <linux/arm-coresight.h>
#endif
#ifdef CONFIG_PXA_RAMDUMP
#include "linux/ramdump.h"
#endif

static const char *handler[]= {
	"Synchronous Abort",
	"IRQ",
	"FIQ",
	"Error"
};

int show_unhandled_signals = 1;

/*
 * Dump out the contents of some memory nicely...
 */
static void dump_mem(const char *lvl, const char *str, unsigned long bottom,
		     unsigned long top)
{
	unsigned long first;
	mm_segment_t fs;
	int i;

	/*
	 * We need to switch to kernel mode so that we can use __get_user
	 * to safely read from kernel space.  Note that we now dump the
	 * code first, just in case the backtrace kills us.
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);

	printk("%s%s(0x%016lx to 0x%016lx)\n", lvl, str, bottom, top);

	for (first = bottom & ~31; first < top; first += 32) {
		unsigned long p;
		char str[sizeof(" 12345678") * 8 + 1];

		memset(str, ' ', sizeof(str));
		str[sizeof(str) - 1] = '\0';

		for (p = first, i = 0; i < 8 && p < top; i++, p += 4) {
			if (p >= bottom && p < top) {
				unsigned int val;
				if (__get_user(val, (unsigned int *)p) == 0)
					sprintf(str + i * 9, " %08x", val);
				else
					sprintf(str + i * 9, " ????????");
			}
		}
		printk("%s%04lx:%s\n", lvl, first & 0xffff, str);
	}

	set_fs(fs);
}

static void dump_backtrace_entry(unsigned long where, unsigned long stack)
{
	print_ip_sym(where);
	if (in_exception_text(where))
		dump_mem("", "Exception stack", stack,
			 stack + sizeof(struct pt_regs));
}

static void dump_instr(const char *lvl, struct pt_regs *regs)
{
	unsigned long addr = instruction_pointer(regs);
	mm_segment_t fs;
	char str[sizeof("00000000 ") * 5 + 2 + 1], *p = str;
	int i;

	/*
	 * We need to switch to kernel mode so that we can use __get_user
	 * to safely read from kernel space.  Note that we now dump the
	 * code first, just in case the backtrace kills us.
	 */
	fs = get_fs();
	set_fs(KERNEL_DS);

	for (i = -4; i < 1; i++) {
		unsigned int val, bad;

		bad = __get_user(val, &((u32 *)addr)[i]);

		if (!bad)
			p += sprintf(p, i == 0 ? "(%08x) " : "%08x ", val);
		else {
			p += sprintf(p, "bad PC value");
			break;
		}
	}
	printk("%sCode: %s\n", lvl, str);

	set_fs(fs);
}

static void dump_backtrace(struct pt_regs *regs, struct task_struct *tsk)
{
	struct stackframe frame;
	const register unsigned long current_sp asm ("sp");

	pr_debug("%s(regs = %p tsk = %p)\n", __func__, regs, tsk);

	if (!tsk)
		tsk = current;

	if (regs) {
		frame.fp = regs->regs[29];
		frame.sp = regs->sp;
		frame.pc = regs->pc;
	} else if (tsk == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.pc = (unsigned long)dump_backtrace;
	} else {
		/*
		 * task blocked in __switch_to
		 */
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		frame.pc = thread_saved_pc(tsk);
	}

	printk("Call trace:\n");
	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(&frame);
		if (ret < 0)
			break;
		dump_backtrace_entry(where, frame.sp);
	}
}

void show_stack(struct task_struct *tsk, unsigned long *sp)
{
	dump_backtrace(NULL, tsk);
	barrier();
}

#ifdef CONFIG_PREEMPT
#define S_PREEMPT " PREEMPT"
#else
#define S_PREEMPT ""
#endif
#ifdef CONFIG_SMP
#define S_SMP " SMP"
#else
#define S_SMP ""
#endif

static int __die(const char *str, int err, struct thread_info *thread,
		 struct pt_regs *regs)
{
	struct task_struct *tsk = thread->task;
	static int die_counter;
	int ret;

	pr_emerg("Internal error: %s: %x [#%d]" S_PREEMPT S_SMP "\n",
		 str, err, ++die_counter);

	/* trap and error numbers are mostly meaningless on ARM */
	ret = notify_die(DIE_OOPS, str, regs, err, 0, SIGSEGV);
	if (ret == NOTIFY_STOP)
		return ret;

	print_modules();
	__show_regs(regs);
	pr_emerg("Process %.*s (pid: %d, stack limit = 0x%p)\n",
		 TASK_COMM_LEN, tsk->comm, task_pid_nr(tsk), thread + 1);

	if (!user_mode(regs) || in_interrupt()) {
		dump_mem(KERN_EMERG, "Stack: ", regs->sp,
			 THREAD_SIZE + (unsigned long)task_stack_page(tsk));
		dump_backtrace(regs, tsk);
		dump_instr(KERN_EMERG, regs);
	}

#ifdef CONFIG_PXA_RAMDUMP
	ramdump_save_dynamic_context(str, err, current_thread_info(), regs);
#endif
	return ret;
}

static DEFINE_RAW_SPINLOCK(die_lock);

/*
 * This function is protected against re-entrancy.
 */
void die(const char *str, struct pt_regs *regs, int err)
{
#ifdef CONFIG_SEC_DEBUG
	arch_stop_trace();
	stop_ftracing();
#endif
	struct thread_info *thread = current_thread_info();
	int ret;

	oops_enter();

	raw_spin_lock_irq(&die_lock);
	console_verbose();
	bust_spinlocks(1);
	ret = __die(str, err, thread, regs);

	if (regs && kexec_should_crash(thread->task))
		crash_kexec(regs);

	bust_spinlocks(0);
	add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
	raw_spin_unlock_irq(&die_lock);
	oops_exit();

	if (in_interrupt())
		panic("Fatal exception in interrupt");
	if (panic_on_oops)
		panic("Fatal exception");
	if (ret != NOTIFY_STOP)
		do_exit(SIGSEGV);
}

void arm64_notify_die(const char *str, struct pt_regs *regs,
		      struct siginfo *info, int err)
{
	if (user_mode(regs))
		force_sig_info(info->si_signo, info, current);
	else
		die(str, regs, err);
}

/*
 * CP15BEN of SCTLR_EL1 may not be implemented, so emulate
 * these three instructions when they trap in
 */
static int aarch32_cp15_barriers(struct pt_regs *regs)
{
	unsigned int instr;
	void __user *pc = (void __user *)instruction_pointer(regs);

	if (!compat_user_mode(regs))
		return -EFAULT;

	get_user(instr, (u32 __user *)pc);
	instr &= ~0xf000f000;

	if (instr == 0x0e070fba)	/* CP15DMB */
		smp_mb();
	else if (instr == 0x0e070f9a)	/* CP15DSB */
		dsb();
	else if (instr == 0x0e070f95)	/* CP15ISB */
		isb();
	else
		return -EFAULT;

	regs->pc += 4;
	return 0;
}

/*
 * Error-checking SWP macros implemented using ldrex{b}/strex{b}
 */
#define __user_swpX_asm(data, addr, res, temp, B)		\
	asm volatile(						\
	"	mov		%w2, %w1\n"			\
	"0:	ldaxr"B"	%w1, [%3]\n"			\
	"1:	stlxr"B"	%w0, %w2, [%3]\n"		\
	"	cbz		%w0, 2f\n"			\
	"	mov		%w0, %w4\n"			\
	"2:\n"							\
	"	.section	 .fixup,\"ax\"\n"		\
	"	.align		2\n"				\
	"3:	mov		%w0, %w5\n"			\
	"	b		2b\n"				\
	"	.previous\n"					\
	"	.section	 __ex_table,\"a\"\n"		\
	"	.align		3\n"				\
	"	.quad		0b, 3b\n"			\
	"	.quad		1b, 3b\n"			\
	"	.previous"					\
	: "=&r" (res), "+r" (data), "=&r" (temp)		\
	: "r" (addr), "i" (-EAGAIN), "i" (-EFAULT)		\
	: "cc", "memory")

#define __user_swp_asm(data, addr, res, temp) \
	__user_swpX_asm(data, addr, res, temp, "")
#define __user_swpb_asm(data, addr, res, temp) \
	__user_swpX_asm(data, addr, res, temp, "b")

/*
 * Macros/defines for extracting register numbers from instruction.
 */
#define EXTRACT_REG_NUM(instruction, offset) \
	(((instruction) & (0xf << (offset))) >> (offset))
#define RN_OFFSET  16
#define RT_OFFSET  12
#define RT2_OFFSET  0

/* opcode functions, stolen from arch/arm */
#define AARCH32_OPCODE_CONDTEST_FAIL   0
#define AARCH32_OPCODE_CONDTEST_PASS   1
#define AARCH32_OPCODE_CONDTEST_UNCOND 2
#define AARCH32_OPCODE_CONDITION_UNCOND 0xf

/*
 * condition code lookup table
 * index into the table is test code: EQ, NE, ... LT, GT, AL, NV
 *
 * bit position in short is condition code: NZCV
 */
static const unsigned short cc_map[16] = {
	0xF0F0,			/* EQ == Z set            */
	0x0F0F,			/* NE                     */
	0xCCCC,			/* CS == C set            */
	0x3333,			/* CC                     */
	0xFF00,			/* MI == N set            */
	0x00FF,			/* PL                     */
	0xAAAA,			/* VS == V set            */
	0x5555,			/* VC                     */
	0x0C0C,			/* HI == C set && Z clear */
	0xF3F3,			/* LS == C clear || Z set */
	0xAA55,			/* GE == (N==V)           */
	0x55AA,			/* LT == (N!=V)           */
	0x0A05,			/* GT == (!Z && (N==V))   */
	0xF5FA,			/* LE == (Z || (N!=V))    */
	0xFFFF,			/* AL always              */
	0			/* NV                     */
};

/*
 * Returns:
 * AARCH32_OPCODE_CONDTEST_FAIL   - if condition fails
 * AARCH32_OPCODE_CONDTEST_PASS   - if condition passes (including AL)
 * AARCH32_OPCODE_CONDTEST_UNCOND - if NV condition, or separate unconditional
 *                              opcode space from v5 onwards
 *
 * Code that tests whether a conditional instruction would pass its condition
 * check should check that return value == AARCH32_OPCODE_CONDTEST_PASS.
 *
 * Code that tests if a condition means that the instruction would be executed
 * (regardless of conditional or unconditional) should instead check that the
 * return value != AARCH32_OPCODE_CONDTEST_FAIL.
 */
asmlinkage unsigned int aarch32_check_condition(u32 opcode, u32 psr)
{
	u32 cc_bits  = opcode >> 28;
	u32 psr_cond = psr >> 28;
	unsigned int ret;

	if (cc_bits != AARCH32_OPCODE_CONDITION_UNCOND) {
		if ((cc_map[cc_bits] >> (psr_cond)) & 1)
			ret = AARCH32_OPCODE_CONDTEST_PASS;
		else
			ret = AARCH32_OPCODE_CONDTEST_FAIL;
	} else {
		ret = AARCH32_OPCODE_CONDTEST_UNCOND;
	}

	return ret;
}

/*
 * Bit 22 of the instruction encoding distinguishes between
 * the SWP and SWPB variants (bit set means SWPB).
 */
#define TYPE_SWPB (1 << 22)

/*
 * Set up process info to signal segmentation fault - called on access error.
 */
static void set_segfault(struct pt_regs *regs, unsigned long addr)
{
	siginfo_t info;

	down_read(&current->mm->mmap_sem);
	if (find_vma(current->mm, addr) == NULL)
		info.si_code = SEGV_MAPERR;
	else
		info.si_code = SEGV_ACCERR;
	up_read(&current->mm->mmap_sem);

	info.si_signo = SIGSEGV;
	info.si_errno = 0;
	info.si_addr  = (void *) instruction_pointer(regs);

	pr_debug("SWP{B} emulation: access caused memory abort!\n");
	arm64_notify_die("Illegal memory access", regs, &info, 0);
}

static int emulate_swpX(u64 address, unsigned int *data,
			unsigned int type)
{
	unsigned int res = 0;

	if ((type != TYPE_SWPB) && (address & 0x3)) {
		/* SWP to unaligned address not permitted */
		pr_debug("SWP instruction on unaligned pointer!\n");
		return -EFAULT;
	}

	while (1) {
		unsigned int temp;

		/*
		 * Barrier required between accessing protected resource and
		 * releasing a lock for it. Legacy code might not have done
		 * this, and we cannot determine that this is not the case
		 * being emulated, so insert always.
		 */
		smp_mb();

		if (type == TYPE_SWPB)
			__user_swpb_asm(*data, address, res, temp);
		else
			__user_swp_asm(*data, address, res, temp);

		if (likely(res != -EAGAIN) || signal_pending(current))
			break;

		cond_resched();
	}

	if (res == 0)
		/*
		 * Barrier also required between acquiring a lock for a
		 * protected resource and accessing the resource. Inserted for
		 * same reason as above.
		 */
		smp_mb();

	return res;
}

/*
 * swp_handler logs the id of calling process, dissects the instruction, sanity
 * checks the memory location, calls emulate_swpX for the actual operation and
 * deals with fixup/error handling before returning
 */
static int aarch32_swp_handler(struct pt_regs *regs)
{
	unsigned int destreg, data, type;
	unsigned int res = 0;
	unsigned int instr;
	unsigned long address;
	void __user *pc = (void __user *)instruction_pointer(regs);

	if (!compat_user_mode(regs))
		return -EFAULT;

	get_user(instr, (u32 __user *)pc);

	if ((instr & 0x0fb00ff0) != 0x01000090)
		return -EFAULT;

	perf_sw_event(PERF_COUNT_SW_EMULATION_FAULTS, 1, regs, regs->pc);

	res = aarch32_check_condition(instr, regs->pstate);
	switch (res) {
	case AARCH32_OPCODE_CONDTEST_PASS:
		break;
	case AARCH32_OPCODE_CONDTEST_FAIL:
		/* Condition failed - return to next instruction */
		regs->pc += 4;
		return 0;
	case AARCH32_OPCODE_CONDTEST_UNCOND:
		/* If unconditional encoding - not a SWP, undef */
		return -EFAULT;
	default:
		return -EINVAL;
	}

	pr_debug("\"%s\" (%ld) uses deprecated SWP{B} instruction\n",
		 current->comm, (unsigned long)current->pid);

	address = regs->regs[EXTRACT_REG_NUM(instr, RN_OFFSET)];
	data	= regs->regs[EXTRACT_REG_NUM(instr, RT2_OFFSET)];
	destreg = EXTRACT_REG_NUM(instr, RT_OFFSET);

	type = instr & TYPE_SWPB;

	pr_debug("addr in r%d->0x%08x, dest is r%d, source in r%d->0x%08x)\n",
		 EXTRACT_REG_NUM(instr, RN_OFFSET), (unsigned int)address,
		 destreg, EXTRACT_REG_NUM(instr, RT2_OFFSET), data);

	/* Check access in reasonable access range for both SWP and SWPB */
	if (!access_ok(VERIFY_WRITE, (address & ~3), 4)) {
		pr_debug("SWP{B} emulation: access to %p not allowed!\n",
			 (void *)address);
		res = -EFAULT;
	} else {
		res = emulate_swpX(address, &data, type);
	}

	if (res == 0) {
		/*
		 * On successful emulation, revert the adjustment to the PC
		 * made in kernel/traps.c in order to resume execution at the
		 * instruction following the SWP{B}.
		 */
		regs->pc += 4;
		regs->regs[destreg] = data;
	} else if (res == -EFAULT) {
		/*
		 * Memory errors do not mean emulation failed.
		 * Set up signal info to return SEGV, then return OK
		 */
		set_segfault(regs, address);
	}

	return 0;
}

static LIST_HEAD(undef_hook);
static DEFINE_RAW_SPINLOCK(undef_lock);

void register_undef_hook(struct undef_hook *hook)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&undef_lock, flags);
	list_add(&hook->node, &undef_hook);
	raw_spin_unlock_irqrestore(&undef_lock, flags);
}

void unregister_undef_hook(struct undef_hook *hook)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&undef_lock, flags);
	list_del(&hook->node);
	raw_spin_unlock_irqrestore(&undef_lock, flags);
}

static int call_undef_hook(struct pt_regs *regs)
{
	struct undef_hook *hook;
	unsigned long flags;
	u32 instr;
	int (*fn)(struct pt_regs *regs, u32 instr) = NULL;
	void __user *pc = (void __user *)instruction_pointer(regs);

	if (!user_mode(regs))
		return 1;

	if (compat_thumb_mode(regs)) {
		/* 16-bit Thumb instruction */
		if (get_user(instr, (u16 __user *)pc))
			goto exit;
		instr = le16_to_cpu(instr);
		if (aarch32_insn_is_wide(instr)) {
			u32 instr2;

			if (get_user(instr2, (u16 __user *)(pc + 2)))
				goto exit;
			instr2 = le16_to_cpu(instr2);
			instr = (instr << 16) | instr2;
		}
	} else {
		/* 32-bit ARM instruction */
		if (get_user(instr, (u32 __user *)pc))
			goto exit;
		instr = le32_to_cpu(instr);
	}

	raw_spin_lock_irqsave(&undef_lock, flags);
	list_for_each_entry(hook, &undef_hook, node)
		if ((instr & hook->instr_mask) == hook->instr_val &&
			(regs->pstate & hook->pstate_mask) == hook->pstate_val)
			fn = hook->fn;

	raw_spin_unlock_irqrestore(&undef_lock, flags);
exit:
	return fn ? fn(regs, instr) : 1;
}


asmlinkage void __exception do_undefinstr(struct pt_regs *regs)
{
	siginfo_t info;
	void __user *pc = (void __user *)instruction_pointer(regs);

	/* check for AArch32 breakpoint instructions */
	if (!aarch32_break_handler(regs))
		return;

	/* check for AArch32 CP15DMB/CP15DSB/CP15ISB */
	if (!aarch32_cp15_barriers(regs))
		return;

	if (!aarch32_swp_handler(regs))
		return;

	if (call_undef_hook(regs) == 0)
		return;

	if (show_unhandled_signals && unhandled_signal(current, SIGILL) &&
	    printk_ratelimit()) {
		pr_info("%s[%d]: undefined instruction: pc=%p\n",
			current->comm, task_pid_nr(current), pc);
		dump_instr(KERN_INFO, regs);
	}

	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code  = ILL_ILLOPC;
	info.si_addr  = pc;

	arm64_notify_die("Oops - undefined instruction", regs, &info, 0);
}

long compat_arm_syscall(struct pt_regs *regs);

asmlinkage long do_ni_syscall(struct pt_regs *regs)
{
#ifdef CONFIG_COMPAT
	long ret;
	if (is_compat_task()) {
		ret = compat_arm_syscall(regs);
		if (ret != -ENOSYS)
			return ret;
	}
#endif

	if (show_unhandled_signals && printk_ratelimit()) {
		pr_info("%s[%d]: syscall %d\n", current->comm,
			task_pid_nr(current), (int)regs->syscallno);
		dump_instr("", regs);
		if (user_mode(regs))
			__show_regs(regs);
	}

	return sys_ni_syscall();
}

/*
 * bad_mode handles the impossible case in the exception vector.
 */
asmlinkage void bad_mode(struct pt_regs *regs, int reason, unsigned int esr)
{
	siginfo_t info;
	void __user *pc = (void __user *)instruction_pointer(regs);
#ifdef CONFIG_FORCE_INSTRUCTION_ALIGNMENT
       if ((esr >> ESR_EL1_EC_SHIFT == ESR_EL1_EC_PC_ALIGN) &&
		       compat_user_mode(regs) && !compat_thumb_mode(regs)) {
	       int isize = 4;
	       pr_crit("Bad mode in %s handler detected, code 0x%08x\n", handler[reason], esr);
	       printk("pc : [<%016llx>] lr : [<%016llx>] pstate: %08llx\n",
			       instruction_pointer(regs), regs->compat_lr, regs->compat_sp);
	       instruction_pointer(regs) &= ~(isize + (-1UL));
	       pr_err("[%s] force instruction alignment pc <%016llx>\n", __func__, instruction_pointer(regs));
	       return;
       }
#endif
	console_verbose();

	pr_crit("Bad mode in %s handler detected, code 0x%08x\n",
		handler[reason], esr);
	__show_regs(regs);


	info.si_signo = SIGILL;
	info.si_errno = 0;
	info.si_code  = ILL_ILLOPC;
	info.si_addr  = pc;

	arm64_notify_die("Oops - bad mode", regs, &info, 0);
}

void __pte_error(const char *file, int line, unsigned long val)
{
	printk("%s:%d: bad pte %016lx.\n", file, line, val);
}

void __pmd_error(const char *file, int line, unsigned long val)
{
	printk("%s:%d: bad pmd %016lx.\n", file, line, val);
}

void __pgd_error(const char *file, int line, unsigned long val)
{
	printk("%s:%d: bad pgd %016lx.\n", file, line, val);
}

void __init trap_init(void)
{
	return;
}
