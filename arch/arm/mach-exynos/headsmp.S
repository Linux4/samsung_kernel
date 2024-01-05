/*
 *  linux/arch/arm/mach-exynos4/headsmp.S
 *
 *  Cloned from linux/arch/arm/mach-realview/headsmp.S
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
 * exynos4 specific entry point for secondary CPUs.  This provides
 * a "holding pen" into which all secondary cores are held until we're
 * ready for them to initialise.
 */

ENTRY(v7_invalidate_l1)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0	@ invalidate I cache
	mcr	p15, 2, r0, c0, c0, 0
	mrc	p15, 1, r0, c0, c0, 0

	ldr	r1, =0x7fff
	and	r2, r1, r0, lsr #13

	ldr	r1, =0x3ff

	and	r3, r1, r0, lsr #3	@ NumWays - 1
	add	r2, r2, #1		@ NumSets

	and	r0, r0, #0x7
	add	r0, r0, #4	@ SetShift

	clz	r1, r3		@ WayShift
	add	r4, r3, #1	@ NumWays
1:	sub	r2, r2, #1	@ NumSets--
	mov	r3, r4		@ Temp = NumWays
2:	subs	r3, r3, #1	@ Temp--
	mov	r5, r3, lsl r1
	mov	r6, r2, lsl r0
	orr	r5, r5, r6	@ Reg = (Temp<<WayShift)|(NumSets<<SetShift)
	mcr	p15, 0, r5, c7, c6, 2
	bgt	2b
	cmp	r2, #0
	bgt	1b
	dsb
	isb
	mov	pc, lr
ENDPROC(v7_invalidate_l1)

ENTRY(exynos4_secondary_startup)
	mrc	p15, 0, r0, c0, c0, 5
#ifdef CONFIG_EXYNOS5_MP
	ubfx	r1, r0, #8, #4
#endif
	and	r0, r0, #15
#ifdef CONFIG_EXYNOS5_MP
	add	r0, r0, r1, lsl #2
	eor	r0, r0, #4
#endif
	adr	r4, 1f
	ldmia	r4, {r5, r6}
	sub	r4, r4, r5
	add	r6, r6, r4
pen:	ldr	r7, [r6]
	cmp	r7, r0
	bne	pen

	/*
	 * we've been released from the holding pen: secondary_stack
	 * should now contain the SVC stack for this core
	 */
#ifdef CONFIG_SOC_EXYNOS4415
	bl	v7_invalidate_l1
#endif
	b	secondary_startup
ENDPROC(exynos4_secondary_startup)

	.align 2
1:	.long	.
	.long	pen_release
