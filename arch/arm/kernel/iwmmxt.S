/*
 *  linux/arch/arm/kernel/iwmmxt.S
 *
 *  XScale iWMMXt (Concan) context switching and handling
 *
 *  Initial code:
 *  Copyright (c) 2003, Intel Corporation
 *
 *  Full lazy switching support, optimizations and more, by Nicolas Pitre
*   Copyright (c) 2003-2004, MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>
#include <asm/ptrace.h>
#include <asm/thread_info.h>
#include <asm/asm-offsets.h>

#if defined(CONFIG_CPU_PJ4) || defined(CONFIG_CPU_PJ4B)
#define PJ4(code...)		code
#define XSC(code...)
#else
#define PJ4(code...)
#define XSC(code...)		code
#endif

#define MMX_WR0		 	(0x00)
#define MMX_WR1		 	(0x08)
#define MMX_WR2		 	(0x10)
#define MMX_WR3			(0x18)
#define MMX_WR4		 	(0x20)
#define MMX_WR5		 	(0x28)
#define MMX_WR6		 	(0x30)
#define MMX_WR7		 	(0x38)
#define MMX_WR8		 	(0x40)
#define MMX_WR9		 	(0x48)
#define MMX_WR10		(0x50)
#define MMX_WR11		(0x58)
#define MMX_WR12		(0x60)
#define MMX_WR13		(0x68)
#define MMX_WR14		(0x70)
#define MMX_WR15		(0x78)
#define MMX_WCSSF		(0x80)
#define MMX_WCASF		(0x84)
#define MMX_WCGR0		(0x88)
#define MMX_WCGR1		(0x8C)
#define MMX_WCGR2		(0x90)
#define MMX_WCGR3		(0x94)

#define MMX_SIZE		(0x98)

	.text

/*
 * Lazy switching of Concan coprocessor context
 *
 * r10 = struct thread_info pointer
 * r9  = ret_from_exception
 * lr  = undefined instr exit
 *
 * called from prefetch exception handler with interrupts disabled
 */

ENTRY(iwmmxt_task_enable)

	XSC(mrc	p15, 0, r2, c15, c1, 0)
	PJ4(mrc p15, 0, r2, c1, c0, 2)
	@ CP0 and CP1 accessible?
	XSC(tst	r2, #0x3)
	PJ4(tst	r2, #0xf)
	movne	pc, lr				@ if so no business here
	@ enable access to CP0 and CP1
	XSC(orr	r2, r2, #0x3)
	XSC(mcr	p15, 0, r2, c15, c1, 0)
	PJ4(orr	r2, r2, #0xf)
	PJ4(mcr	p15, 0, r2, c1, c0, 2)

	ldr	r3, last_iwmmxt_context_address
	add	r0, r10, #TI_IWMMXT_STATE	@ get task Concan save area
	ldr	r2, [sp, #60]			@ current task pc value
	ldr	r11, [r10, #TI_CPU]		@ CPU number
	ldr	r1, [r3, r11, lsl #2]		@ get current Concan owner
	str	r0, [r3, r11, lsl #2]		@ this task now owns Concan regs
	sub	r2, r2, #4			@ adjust pc back
	str	r2, [sp, #60]

	mrc	p15, 0, r2, c2, c0, 0
	mov	r2, r2				@ cpwait

	teq	r1, r0				@ the same process?
	moveq	pc, r9				@ just return, no need reload
	teq	r1, #0				@ test for last ownership
	mov	lr, r9				@ normal exit from exception
	beq	concan_load			@ no owner, skip save

#ifndef CONFIG_SMP
concan_save:

	tmrc	r2, wCon

	@ CUP? wCx
	tst	r2, #0x1
	beq 	1f

concan_dump:

	wstrw	wCSSF, [r1, #MMX_WCSSF]
	wstrw	wCASF, [r1, #MMX_WCASF]
	wstrw	wCGR0, [r1, #MMX_WCGR0]
	wstrw	wCGR1, [r1, #MMX_WCGR1]
	wstrw	wCGR2, [r1, #MMX_WCGR2]
	wstrw	wCGR3, [r1, #MMX_WCGR3]

1:	@ MUP? wRn
	tst	r2, #0x2
	beq	2f

	wstrd	wR0,  [r1, #MMX_WR0]
	wstrd	wR1,  [r1, #MMX_WR1]
	wstrd	wR2,  [r1, #MMX_WR2]
	wstrd	wR3,  [r1, #MMX_WR3]
	wstrd	wR4,  [r1, #MMX_WR4]
	wstrd	wR5,  [r1, #MMX_WR5]
	wstrd	wR6,  [r1, #MMX_WR6]
	wstrd	wR7,  [r1, #MMX_WR7]
	wstrd	wR8,  [r1, #MMX_WR8]
	wstrd	wR9,  [r1, #MMX_WR9]
	wstrd	wR10, [r1, #MMX_WR10]
	wstrd	wR11, [r1, #MMX_WR11]
	wstrd	wR12, [r1, #MMX_WR12]
	wstrd	wR13, [r1, #MMX_WR13]
	wstrd	wR14, [r1, #MMX_WR14]
	wstrd	wR15, [r1, #MMX_WR15]

2:	teq	r0, #0				@ anything to load?
	moveq	pc, lr
#endif

concan_load:

	@ Load wRn
	wldrd	wR0,  [r0, #MMX_WR0]
	wldrd	wR1,  [r0, #MMX_WR1]
	wldrd	wR2,  [r0, #MMX_WR2]
	wldrd	wR3,  [r0, #MMX_WR3]
	wldrd	wR4,  [r0, #MMX_WR4]
	wldrd	wR5,  [r0, #MMX_WR5]
	wldrd	wR6,  [r0, #MMX_WR6]
	wldrd	wR7,  [r0, #MMX_WR7]
	wldrd	wR8,  [r0, #MMX_WR8]
	wldrd	wR9,  [r0, #MMX_WR9]
	wldrd	wR10, [r0, #MMX_WR10]
	wldrd	wR11, [r0, #MMX_WR11]
	wldrd	wR12, [r0, #MMX_WR12]
	wldrd	wR13, [r0, #MMX_WR13]
	wldrd	wR14, [r0, #MMX_WR14]
	wldrd	wR15, [r0, #MMX_WR15]

	@ Load wCx
	wldrw	wCSSF, [r0, #MMX_WCSSF]
	wldrw	wCASF, [r0, #MMX_WCASF]
	wldrw	wCGR0, [r0, #MMX_WCGR0]
	wldrw	wCGR1, [r0, #MMX_WCGR1]
	wldrw	wCGR2, [r0, #MMX_WCGR2]
	wldrw	wCGR3, [r0, #MMX_WCGR3]

	@ clear CUP/MUP (only if r1 != 0)
	teq	r1, #0
	mov 	r2, #0
	moveq	pc, lr
	tmcr	wCon, r2
	mov	pc, lr

last_iwmmxt_context_address:
	.word	last_iwmmxt_context


ENTRY(iwmmxt_save_state)
	wstrw	wCSSF, [r0, #MMX_WCSSF]
	wstrw	wCASF, [r0, #MMX_WCASF]
	wstrw	wCGR0, [r0, #MMX_WCGR0]
	wstrw	wCGR1, [r0, #MMX_WCGR1]
	wstrw	wCGR2, [r0, #MMX_WCGR2]
	wstrw	wCGR3, [r0, #MMX_WCGR3]

	wstrd	wR0,  [r0, #MMX_WR0]
	wstrd	wR1,  [r0, #MMX_WR1]
	wstrd	wR2,  [r0, #MMX_WR2]
	wstrd	wR3,  [r0, #MMX_WR3]
	wstrd	wR4,  [r0, #MMX_WR4]
	wstrd	wR5,  [r0, #MMX_WR5]
	wstrd	wR6,  [r0, #MMX_WR6]
	wstrd	wR7,  [r0, #MMX_WR7]
	wstrd	wR8,  [r0, #MMX_WR8]
	wstrd	wR9,  [r0, #MMX_WR9]
	wstrd	wR10, [r0, #MMX_WR10]
	wstrd	wR11, [r0, #MMX_WR11]
	wstrd	wR12, [r0, #MMX_WR12]
	wstrd	wR13, [r0, #MMX_WR13]
	wstrd	wR14, [r0, #MMX_WR14]
	wstrd	wR15, [r0, #MMX_WR15]

 	mov	pc, lr
