/*
 * vlock.S - simple voting lock implementation for ARM
 *
 * Created by:	Dave Martin, 2012-08-16
 * Copyright:	(C) 2012-2013  Linaro Limited
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
 *
 * This algorithm is described in more detail in
 * Documentation/arm/vlocks.txt.
 */

#include <linux/linkage.h>

#include <asm/assembler.h>
#include "vlock.h"

/* Select different code if voting flags  can fit in a single word. */
#if VLOCK_VOTING_SIZE > 8
#define FEW(x...)
#define MANY(x...) x
#else
#define FEW(x...) x
#define MANY(x...)
#endif

/*
 * voting lock for first-man coordination
 */
	.macro	voting_begin, xbase, xcpu, wscratch
	mov	\wscratch, #1
	strb	\wscratch, [\xbase, \xcpu]
	dmb	ish
	.endm

	.macro	voting_end, xbase, xcpu, wscratch
	dmb	ish
	mov	\wscratch, #0
	strb	\wscratch, [\xbase, \xcpu]
	dsb	sy
	sev
	.endm

/*
 * The vlock structure must reside in Strongly-Ordered or Device memory.
 * This implementation deliberately eliminates most of the barriers which
 * would be required for other memory types, and assumes that independent
 * writes to neighbouring locations within a cacheline do not interfere
 * with one another.
 */

/*
 * x0: lock structure base
 * x1: CPU ID (0-based index within cluster)
 */
ENTRY(vlock_trylock)
	add	x1, x1, #VLOCK_VOTING_OFFSET

	voting_begin	x0, x1, w2

	ldr	x2, [x0, #VLOCK_OWNER_OFFSET]	// check whether lock is held
	cmp	x2, #VLOCK_OWNER_NONE
	b.ne	trylock_fail			// fail if so

	/*
	 * Control dependency implies strb not observable before previous ldrb.
	 */

	str	x1, [x0, #VLOCK_OWNER_OFFSET]	// submit my vote

	voting_end	x0, x1, w2		// implies DMB

	/*
	 * Wait for the current round of voting to finish:
	 */
 MANY(	mov	x3, #VLOCK_VOTING_OFFSET			)
0:
 MANY(	ldr	x2, [x0, x3]					)
 FEW(	ldr	x2, [x0, #VLOCK_VOTING_OFFSET]			)
	cmp	x2, #0
	b.eq	1f
	wfe
	b	0b
1:
 MANY(	add	x3, x3, #4					)
 MANY(	cmp	x3, #VLOCK_VOTING_OFFSET + VLOCK_VOTING_SIZE	)
 MANY(	b.ne	0b						)

	/*
	 * Check who won:
	 */

	dmb	ish
	ldr	x2, [x0, #VLOCK_OWNER_OFFSET]
	eor	x0, x1, x2			// zero if I won, else nonzero
	br	lr

trylock_fail:
	voting_end	x0, x1, w2
	mov	x0, #1				// nonzero indicates that I lost
	br	lr
ENDPROC(vlock_trylock)

/*
 * r0: lock structure base
 */
ENTRY(vlock_unlock)
	dmb	ish
	mov	x1, #VLOCK_OWNER_NONE
	str	x1, [x0, #VLOCK_OWNER_OFFSET]
	dsb	sy
	sev
	br	lr
ENDPROC(vlock_unlock)
