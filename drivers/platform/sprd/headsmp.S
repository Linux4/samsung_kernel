/*
 *  linux/arch/arm/mach-sc/headsmp.S
 *
 *
 *  Copyright (c) 2003 ARM Limited
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <linux/init.h>

	__CPUINIT

/*
 * sci smp specific entry point for secondary CPUs.
 * This provides a "holding pen" into which all secondary cores are held
 * until we're ready for them to initialise.
 */
ENTRY(sci_secondary_startup)
	mrc	p15, 0, r0, c0, c0, 5
#ifndef CONFIG_OF
	and	r0, r0, #15
#else
	/*and	r0, r0, #0xFFFFFF*/ /* mpidr is 0xf00 0xf01, 0xf02, 0xf03 */
	and	r0, r0, #15 /* mpidr is 0xf00 0xf01, 0xf02, 0xf03 */
#endif
	adr	r4, 1f
	ldmia	r4, {r5, r6}
	sub	r4, r4, r5
	add	r6, r6, r4
pen:	ldr	r7, [r6]
	and	r7, r7, #15/* mpidr is 0xf00 0xf01, 0xf02, 0xf03 */
	cmp	r7, r0
	bne	pen

	/*
	 * we've been released from the holding pen: secondary_stack
	 * should now contain the SVC stack for this core
	 */
	b	secondary_startup

	.align
1:	.long	.
	.long	pen_release

ENTRY(sci_shark_enter_lowpower)
	stmfd 	sp!, {r4-r12, lr}

	mrc     p15, 0, r1, c1, c0, 0
	bic     r0, r1, #(1 << 2)		@disable d-cache
	mcr     p15, 0, r0, c1, c0, 0

	bl      v7_flush_dcache_louis

	clrex

	mrc	p15, 0, r1, c1, c0, 1
	bic	r1, r1, #(1 << 6)
	mcr 	p15, 0, r1, c1, c0, 1

	isb
	dsb
	nop
	nop
	nop
	nop
	nop
	nop

	wfi

	nop
	nop
	nop
	nop
	nop
	nop
	nop

	ldmfd 	sp!, {r4-r12, pc}
ENDPROC(sci_shark_enter_lowpower)
