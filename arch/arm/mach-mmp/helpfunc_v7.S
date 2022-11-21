/*
 * linux/arch/arm/mach-mmp/head_v7.S
 *
 * Copyright (C) 2012 Marvell, Inc.
 *
 * Author: Neil Zhang <zhangwm@marvell.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/linkage.h>
#include <asm/cache.h>
#include <asm/assembler.h>
#include <asm/hardware/cache-l2x0.h>


/*
 * Marvell specific entry point for secondary CPUs.
 * The secondary kernel init calls v7_flush_dcache_all before it enables
 * the L1; however, the L1 comes out of reset in an undefined state, so
 * the clean + invalidate performed by v7_flush_dcache_all causes a bunch
 * of cache lines with uninitialized data and uninitialized tags to get
 * written out to memory, which does really unpleasant things to the main
 * processor.  We fix this by performing an invalidate, rather than a
 * clean + invalidate for secondary core, before jumping into the kernel.
 *
 * This funciton is cloned from arch/arm/mach-tegra/headsmp.S, and needs
 * to be called for both secondary cores startup and primary core resume
 * procedures.
 */
	.align L1_CACHE_SHIFT

ENTRY(v7_invalidate_l1)
        mov     r0, #0
        mcr     p15, 2, r0, c0, c0, 0
        mrc     p15, 1, r0, c0, c0, 0

        ldr     r1, =0x7fff
        and     r2, r1, r0, lsr #13

        ldr     r1, =0x3ff

        and     r3, r1, r0, lsr #3  @ NumWays - 1
        add     r2, r2, #1          @ NumSets

        and     r0, r0, #0x7
        add     r0, r0, #4          @ SetShift

        clz     r1, r3              @ WayShift
        add     r4, r3, #1          @ NumWays
1:      sub     r2, r2, #1          @ NumSets--
        mov     r3, r4              @ Temp = NumWays
2:      subs    r3, r3, #1          @ Temp--
        mov     r5, r3, lsl r1
        mov     r6, r2, lsl r0
        orr     r5, r5, r6          @ Reg = (Temp<<WayShift)|(NumSets<<SetShift)
        mcr     p15, 0, r5, c7, c6, 2
        bgt     2b
        cmp     r2, #0
        bgt     1b
        dsb
        isb
        mov     pc, lr
ENDPROC(v7_invalidate_l1)

#ifdef CONFIG_CACHE_L2X0
/*
 * Clean and invalidate the L2 cache and then disable it.
 * Common cache-l2x0.c functions can't be used here since it
 * uses spinlocks. We are out of coherency here with data cache
 * disabled. The spinlock implementation uses exclusive load/store
 * instruction which can fail without data cache being enabled.
 * Because of this, CPU can lead to deadlock.
 * We need to call this function with MMU on since l2x0_base is
 * virtual adress.
 */
ENTRY(pl310_disable)
	ldr	r2, =l2x0_base
	ldr	r2, [r2]
	/* Clean & invalidate */
	ldr	r0, =0xffff
	str	r0, [r2, #L2X0_CLEAN_INV_WAY]
wait:
	ldr	r0, [r2, #L2X0_CLEAN_INV_WAY]
	ldr	r1, =0xffff
	ands	r0, r0, r1
	bne	wait
	/* Sync */
	mov	r0, #0x0
	str	r0, [r2, #L2X0_CACHE_SYNC]
	/* Disable L2 */
	mov	r0, #0
	str	r0, [r2, #L2X0_CTRL]
	dsb
	mov	pc, lr
ENDPROC(pl310_disable)
#endif

#ifdef CONFIG_ARCH_PROVIDES_UDELAY
ENTRY(__delay)
	subs    r0, r0, #1
	bhi     __delay
	mov     pc, lr
ENDPROC(__delay)
#endif /* CONFIG_ARCH_PROVIDES_UDELAY */
