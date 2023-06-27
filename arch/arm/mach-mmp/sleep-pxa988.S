/*
 * linux/arch/arm/mach-mmp/sleep-pxa988.S
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
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/addr-map.h>
#include <mach/pxa988_lowpower.h>

/*
 * The following Macros should be defined in mach/pxa988_lowpower.h
 *
 * #define LPM_NUM
 * #define OFFSET_SCU_SHUTDOWN
 * #define OFFSET_SPINLOCK
 *
 * The topology of the reserved data is as following.
 * Each core will use 4 bytes to save the flags.
 * The base address is pointed by pm_reserve_pa
 *
 *
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |(PXA988/1088)
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |(PXA988/1088)
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |(PXA1088 only)
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |(PXA1088 only)
 * +--------------------------------------------------------+
 * |     scu power down flag                                |(PXA988 only)
 * +--------------------------------------------------------+
 * |     spin_lock                                          |(PXA988 only)
 * +--------------------------------------------------------+
 * |     barrier                                            |(PXA1088 only)
 * +--------------------------------------------------------+
 */

#ifdef CONFIG_HAVE_ARM_SCU
#define SCU_CTRL		(SCU_PHYS_BASE + 0x00)
#define SCU_CPU_POWER_STATUS    (SCU_PHYS_BASE + 0x08)
#define SCU_INVALIDATE          (SCU_PHYS_BASE + 0x0c)
#endif

#ifdef CONFIG_ARM_ERRATA_802022
#define GICD_CTLR	(PERI_PHYS_BASE + 0x1000)
#define CORE0_WAKEUP	0xd428292c
#define MP_IDLE_CFG0	0xd4282920
#endif

/*
 * Note: The following code is located into the .data section. This is to
 *	 allow l2x0_regs_phys to be accessed with a relative load while we
 *	 can't rely on any MMU translation.
 *	 Reference from: arch/arm/kernel/sleep.S
 */
        .data
        .align

/*
 * r0, CPUID
 * r1, the base physical address of pm reserved space
 */
ENTRY(pxa988_cpu_resume_handler)
#ifndef CONFIG_ARM_ERRATA_802022
#ifdef CONFIG_CPU_CA7MP
	mrc     p15, 0, r0, c1, c0, 1
	orr	r0, r0, #0x40
	mcr     p15, 0, r0, c1, c0, 1
#endif
#ifdef CONFIG_CPU_CA9MP
	/* I+BTB cache invalidate */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0
#endif

	/* Enalble I-cache and Branch Prediction here */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #0x1800
	mcr	p15, 0, r0, c1, c0, 0
#endif

/*
 * Note: Architecturally, caches are not guaranteed to be in a known state
 * at reset.
 * - need to be invalidated by software on Cortex-A9.
 * - not required on Cortex-A5/A7/A15.
 */
#ifdef CONFIG_CPU_CA9MP
	bl	v7_invalidate_l1	@ invalidate L1 first when back
#endif

	/* Load the reserved memory address */
	ldr	r1, pm_reserve_pa

	/* fetch the CPU ID */
	mrc 	p15, 0, r0, c0, c0, 5
	and     r0, r0, #15		@ fetch CPUID

#ifdef CONFIG_ARM_ERRATA_802022
	@ Check MP power disable bit
	ldr     r2, =MP_IDLE_CFG0
	ldr     r3, [r2]
	tst	r3, #PMUA_DIS_MP_SLP
	bne     nowake

	@ Check GIC status
	ldr     r2, =GICD_CTLR
        ldr     r3, [r2]
        cmp     r3, #0
        bne     nowake

	@ Wakeup all cores
        mov     r3, #0xf
        ldr     r2, =CORE0_WAKEUP
        add     r2, r2, r0, lsl #2
        str     r3, [r2]

	@ barrier to make sure all cores have been waken
	mov     r2, #OFFSET_BARRIER
	add     r2, r2, r1

	dmb

	@ barrier_inc
	ldr	r3, [r2, r0, lsl #2]
	add	r3, r3, #1
	str	r3, [r2, r0, lsl #2]
	dmb

	@ check other core's status
1:	mov	r3, #0
2:	dmb
	ldr	r4, [r2, r3, lsl #2]
	cmp	r4, #1
	bne	1b
	add	r3, r3, #1
	cmp	r3, #CONFIG_NR_CPUS
	bne	2b

nowake:
	mrc     p15, 0, r2, c1, c0, 1
	orr	r2, r2, #0x40
	mcr     p15, 0, r2, c1, c0, 1
	isb
#endif

#if defined(CONFIG_HAVE_ARM_SCU) || defined(CONFIG_CACHE_L2X0)
	/* spin lock */
	mov     r2, #OFFSET_SPINLOCK
	add     r2, r2, r1
	mov     r3, #1
1:	ldrex	r4, [r2]
	teq     r4, #0
	wfene
	strexeq r4, r3, [r2]
	teqeq   r4, #0
	bne     1b
	dmb
#endif

#ifdef CONFIG_HAVE_ARM_SCU
	ldr	r2, =SCU_CTRL
	ldr	r3, [r2]
	tst	r3, #1
	bne	set_scu_mode

	/* enable SCU */
	orr	r3, r3, #0x21
	str	r3, [r2]
	/* Invalidate both CPUs' SCU tag RAMs */
	mov	r4, #0xff
	ldr	r5, =SCU_INVALIDATE
	str	r4, [r5]

	/* set SCU_SHUTDOWN flag */
	mov	r4, #0x1
	str	r4, [r1, #OFFSET_SCU_SHUTDOWN]

set_scu_mode:
	/* scu_power_mode(scu_base_addr, SCU_PM_NORMAL) */
	ldr     r2, =SCU_CPU_POWER_STATUS
	ldrb    r3, [r2, r0]
	bic     r3, r3, #0x3
	strb    r3, [r2, r0]
#endif
	/* Clear cpu flags */
	mov	r2, #0
	str     r2, [r1, r0, lsl #2]

	/* check L2, if disabled, then enable it */
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
#else
#ifdef CONFIG_CPU_PXA988
	/* workaroud: M2 depends on L2 dynamic clock gating enabled */
	ldr	r2, =SL2C_PHYS_BASE
	mov	r3, #0x3
	str	r3, [r2, #L2X0_POWER_CTRL]
#endif
#endif

#if defined(CONFIG_HAVE_ARM_SCU) || defined(CONFIG_CACHE_L2X0)
	/* spin unlock */
	dmb
	mov     r2, #0
	str     r2, [r1, #OFFSET_SPINLOCK]
	dsb
	sev
#endif
	b	cpu_resume

	.globl pm_reserve_pa
pm_reserve_pa:
	.long   0

#ifdef CONFIG_CACHE_L2X0
	.globl l2sram_shutdown
l2sram_shutdown:
	.long   0

	.globl l2x0_regs_phys
l2x0_regs_phys:
	.long   0
#endif
ENDPROC(pxa988_cpu_resume_handler)
