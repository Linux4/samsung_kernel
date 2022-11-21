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
 * Note: We can support more than 2 cores here.
 * 	 current we define MAX_CPU_NUM as 2.
 *
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |
 * +--------------------------------------------------------+
 * | ... | hotplug | LPM[MAX_NUM_LPM - 1] | LPM[1] | LPM[0] |
 * +--------------------------------------------------------+
 * |     scu power down flag                                |
 * +--------------------------------------------------------+
 * |     spin_lock                                          |
 * +--------------------------------------------------------+
 */

#define SCU_CTRL		(SCU_PHYS_BASE + 0x00)
#define SCU_CPU_POWER_STATUS    (SCU_PHYS_BASE + 0x08)
#define SCU_INVALIDATE          (SCU_PHYS_BASE + 0x0c)

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
	/* I+BTB cache invalidate */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0
	/* Enalble I-cache and Branch Prediction here */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #0x1800
	mcr	p15, 0, r0, c1, c0, 0

	bl	v7_invalidate_l1	@ invalidate L1 first when back
#ifdef CONFIG_SMP
	/* fetch the CPU ID */
	mrc 	p15, 0, r0, c0, c0, 5
	and     r0, r0, #15		@ fetch CPUID

	/* Load the reserved memory address */
	ldr	r1, pm_reserve_pa

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

	/* Clear cpu flags */
	mov	r2, #0
	str     r2, [r1, r0, lsl #2]
#endif

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
	/* workaroud: M2 depends on L2 dynamic clock gating enabled */
	ldr	r2, =SL2C_PHYS_BASE
	mov	r3, #0x3
	str	r3, [r2, #L2X0_POWER_CTRL]
#endif

#ifdef CONFIG_SMP
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

	.globl l2sram_shutdown
l2sram_shutdown:
	.long   0

	.globl l2x0_regs_phys
l2x0_regs_phys:
	.long   0
ENDPROC(pxa988_cpu_resume_handler)