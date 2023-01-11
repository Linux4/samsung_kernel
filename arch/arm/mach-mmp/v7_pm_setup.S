/*
 * linux/arch/arm/mach-mmp/v7_pm_setup.S
 *
 * Copyright (C) 2013 Marvell, Inc.
 *
 * Author: Neil Zhang <zhangwm@marvell.com>
 *         Fan Wu <fwu@marvell.com>
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
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/hardware/cache-l2x0.h>
#ifdef CONFIG_ARM_ERRATA_802022
#include <asm/memory.h>

#define CORE0_WAKEUP		0xd428292c
#define AXI_PHYS_BASE		0xd4200000
#define PMU_PHYS_BASE		(AXI_PHYS_BASE + 0x82800)
#define ICU_PHYS_BASE		(AXI_PHYS_BASE + 0x82000)
#define PMU_CORE0_IDLE_CFG_PHYS	(PMU_PHYS_BASE + 0x0124)
#define PMU_CORE1_IDLE_CFG_PHYS	(PMU_PHYS_BASE + 0x0128)
#define PMU_CORE2_IDLE_CFG_PHYS	(PMU_PHYS_BASE + 0x0160)
#define PMU_CORE3_IDLE_CFG_PHYS	(PMU_PHYS_BASE + 0x0164)
#define ICU_C0_GBL_INT_MSK_PHYS	(ICU_PHYS_BASE + 0x228)
#define ICU_C1_GBL_INT_MSK_PHYS	(ICU_PHYS_BASE + 0x238)
#define ICU_C2_GBL_INT_MSK_PHYS	(ICU_PHYS_BASE + 0x248)
#define ICU_C3_GBL_INT_MSK_PHYS	(ICU_PHYS_BASE + 0x258)

#define PMUA_GIC_IRQ_GLOBAL_MASK	(1 << 3)
#define PMUA_GIC_FIQ_GLOBAL_MASK	(1 << 4)
#define GIC_GLABAL_MASK	(PMUA_GIC_IRQ_GLOBAL_MASK | PMUA_GIC_FIQ_GLOBAL_MASK)
#define ICU_MASK_FIQ			(1 << 0)
#define ICU_MASK_IRQ			(1 << 1)
#define ICU_GLABAL_MASK	(ICU_MASK_FIQ | ICU_MASK_IRQ)

ENTRY(errata_802022_handler)
	/* fetch the CPU ID */
	mrc 	p15, 0, r0, c0, c0, 5
	and     r0, r0, #15		@ fetch CPUID

	@ barrier to make sure all cores have been waken
	adr     r2, barrier

	@ barrier_inc
	ldr	r3, [r2, r0, lsl #2]
	add	r3, r3, #1
	str	r3, [r2, r0, lsl #2]
	dmb

	@ core 0 to wakeup other cores
	cmp	r0, #0
	bne	reenter

	@ Wakeup all cores
	mov	r3, #1
        mov     r3, r3, lsl #CONFIG_NR_CPUS
	sub	r3, r3, #1
        ldr     r4, =CORE0_WAKEUP
        str     r3, [r4]
	dsb

	@ check other core's status
	mov	r3, #0
check_core:
	dmb
	ldr	r4, [r2, r3, lsl #2]
	cmp	r4, #0
	beq	check_core
	add	r3, r3, #1
	cmp	r3, #CONFIG_NR_CPUS
	bne	check_core

	@ zero barrier
	mov	r4, #0
	mov	r5, #0
	mov	r6, #0
	mov	r7, #0
	stmia	r2, {r4 - r7}
	dmb
	b	mcpm_entry_point

reenter:
	/* mask GIC interrtup */
	cmp	r0, #0
	ldreq   r1, =PMU_CORE0_IDLE_CFG_PHYS
	cmp	r0, #1
	ldreq   r1, =PMU_CORE1_IDLE_CFG_PHYS
	cmp	r0, #2
	ldreq   r1, =PMU_CORE2_IDLE_CFG_PHYS
	cmp	r0, #3
	ldreq   r1, =PMU_CORE3_IDLE_CFG_PHYS

	ldr     r2, [r1]
	orr	r2, r2, #GIC_GLABAL_MASK
	str	r2, [r1]

	/* Mask ICU global interrupt */
	cmp	r0, #0
	ldreq	r1, =ICU_C0_GBL_INT_MSK_PHYS
	cmp	r0, #1
	ldreq	r1, =ICU_C1_GBL_INT_MSK_PHYS
	cmp	r0, #2
	ldreq	r1, =ICU_C2_GBL_INT_MSK_PHYS
	cmp	r0, #3
	ldreq	r1, =ICU_C3_GBL_INT_MSK_PHYS

	ldr     r2, [r1]
	orr	r2, r2, #ICU_GLABAL_MASK
	str 	r2, [r1]

	b	cpu_v7_do_idle

	.align	2
barrier:.long   0
	.long   0
	.long   0
	.long   0
ENDPROC(errata_802022_handler)
#endif

/* ca7_pm_power_up_setup function is for CA7 reset use
 * SMP bit enabling as early as possible will raise efficiency
 * of code running.
 */
ENTRY(ca7_power_up_setup)
	cmp	r0, #0
	bne	1f

	/* Enable SMP bit as early as possible*/
        mrc     p15, 0, r0, c1, c0, 1
        orr     r0, r0, #0x40
        mcr     p15, 0, r0, c1, c0, 1

	/* Enalble I-cache and Branch Prediction here */
	mrc	p15, 0, r0, c1, c0, 0
	orr     r0, r0, #0x1800
	mcr     p15, 0, r0, c1, c0, 0
1:
	bx	lr
ENDPROC(ca7_power_up_setup)

#define SCU_PHYS_BASE		0xd1dfe000
#define SCU_CTRL		(SCU_PHYS_BASE + 0x00)
#define SCU_CPU_STATUS		(SCU_PHYS_BASE + 0x08)
#define SCU_INVALIDATE		(SCU_PHYS_BASE + 0x0c)

/*
 * Note: The following code is located into the .data section. This is to
 *	 allow l2x0_regs_phys to be accessed with a relative load while we
 *	 can't rely on any MMU translation.
 *	 Reference from: arch/arm/kernel/sleep.S
 */
        .data
        .align
ENTRY(ca9_power_up_setup)
	cmp	r0, #0
	beq	1f

	adr	r1, 2f
	stmea	r1!, {r4 - r7}

	/* check if SCU is shutdown */
	ldr	r2, =SCU_CTRL
	ldr	r3, [r2]
	tst	r3, #1
	bne	power_up_l2

	/* enable SCU */
	orr	r3, r3, #0x21
	str	r3, [r2]

	/* Invalidate both CPUs' SCU tag RAMs */
	mov	r4, #0xff
	ldr	r5, =SCU_INVALIDATE
	str	r4, [r5]

	/* check L2, if disabled, then enable it */
power_up_l2:
#ifdef CONFIG_CACHE_L2X0
	adr	r2, l2x0_regs_phys
	ldr	r2, [r2]
	ldr	r3, [r2, #L2X0_R_PHY_BASE]	@ phys addr
	ldr	r4, [r3, #L2X0_CTRL]
	tst	r4, #0x1
	bne	l2on

	/* check whether the L2 Array has been powered down */
	adr	r4, l2sram_shutdown
	ldr	r5, [r4]
	cmp     r5, #0		@ no, restore registers is enough
	beq     pl310_restore
	mov	r5, #0
	str	r5, [r4]	@ clear it if setted
pl310_inv_all:
	mov     r4, #0xff00
	orr	r4, #0xff
	str     r4, [r3, #L2X0_INV_WAY]
inv_wait:
	ldr     r5, [r3,#L2X0_INV_WAY]
	and     r5, r5, r4
	cmp     r5, #0
	bne     inv_wait
	str     r5, [r3, #L2X0_CACHE_SYNC]
pl310_restore:
	ldmia   r2!, {r4-r7}
	str     r5, [r3, #L2X0_AUX_CTRL]
	str     r6, [r3, #L2X0_TAG_LATENCY_CTRL]
	str     r7, [r3, #L2X0_DATA_LATENCY_CTRL]
	ldmia   r2!, {r4-r7}
	str     r4, [r3, #L2X0_ADDR_FILTER_START]
	str     r5, [r3, #L2X0_ADDR_FILTER_END]
	str     r6, [r3, #L2X0_PREFETCH_CTRL]
	str     r7, [r3, #L2X0_POWER_CTRL]
	mov	r4, #1
	str	r4, [r3, #L2X0_CTRL]
l2on:
#endif

	ldmea	r1!, {r4 - r7}
	bx	lr
1:
	/* I+BTB cache invalidate */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0

	/* Enalble I-cache and Branch Prediction here */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #0x1800
	mcr	p15, 0, r0, c1, c0, 0

	mrc 	p15, 0, r0, c0, c0, 5
	and     r0, r0, #15		@ fetch CPUID

	/* Set SCU power mode to SCU_PM_NORMAL */
	ldr     r2, =SCU_CPU_STATUS
	ldrb    r3, [r2, r0]
	bic     r3, r3, #0x3
	strb    r3, [r2, r0]

	bx	lr

	.align	2
2:	.word	.
	.word	.
	.word	.
	.word	.

	.globl l2sram_shutdown
l2sram_shutdown:
	.long   0

	.globl l2x0_regs_phys
l2x0_regs_phys:
	.long   0
ENDPROC(ca9_power_up_setup)
