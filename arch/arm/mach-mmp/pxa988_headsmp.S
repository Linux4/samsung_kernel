/*
 * linux/arch/arm/mach-mmp/pxa988_headsmp.S
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
#include <linux/init.h>
#include <asm/memory.h>
#include <mach/addr-map.h>
#include <mach/pxa988_lowpower.h>

	__CPUINIT

/*
 * PXA specific entry point for secondary CPUs.  This provides
 * a "holding pen" into which all secondary cores are held until we're
 * ready for them to initialise.
 */
ENTRY(pxa988_secondary_startup)
	mrc	p15, 0, r0, c0, c0, 5
	and	r0, r0, #15
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
	bl	v7_invalidate_l1
	b	secondary_startup
ENDPROC(pxa988_secondary_startup)

	.align	2
1:	.long	.
	.long	pen_release


/*
 * Note: The following code is located into the .data section. This is to
 *       allow sw_reset_flag and cpu_plugin_handler to be accessed with a
 *       relative load while we can't rely on any MMU translation.
 *       Reference from: arch/arm/kernel/sleep.S
 */

	.data
	.align

#ifdef CONFIG_HOTPLUG_CPU

#define SCU_CPU_POWER_STATUS	(SCU_PHYS_BASE + 0x08)

#define PMU_PHYS_BASE		(AXI_PHYS_BASE + 0x82800)
#define PMU_CORE0_IDLE_CFG_PHYS	(PMU_PHYS_BASE + 0x0124)
#define PMU_MP_IDLE_CFG0_PHYS	(PMU_PHYS_BASE + 0x0120)
#define PMU_MP_IDLE_CFG1_PHYS	(PMU_PHYS_BASE + 0x00e4)

#define ICU_PHYS_BASE		(AXI_PHYS_BASE + 0x82000)
#define ICU_C0_GBL_INT_MSK_PHYS	(ICU_PHYS_BASE + 0x114)
#define ICU_C1_GBL_INT_MSK_PHYS	(ICU_PHYS_BASE + 0x144)

#define GIC_GLABAL_MASK	(PMUA_GIC_IRQ_GLOBAL_MASK | PMUA_GIC_FIQ_GLOBAL_MASK)
#define ICU_GLABAL_MASK	(ICU_MASK_FIQ | ICU_MASK_IRQ)
#define CORE_IDLE_MASK	(PMUA_CORE_IDLE | PMUA_CORE_POWER_DOWN | \
			PMUA_CORE_L1_SRAM_POWER_DOWN)
#define MP_IDLE_MASK	(PMUA_MP_IDLE | PMUA_MP_POWER_DOWN |	\
			PMUA_MP_L2_SRAM_POWER_DOWN |		\
			PMUA_MP_SCU_SRAM_POWER_DOWN)

/* r0, keep to CPUID */
ENTRY(pxa988_hotplug_handler)
	/* Get the physical address of pm_reserve_pa */
	ldr	r1, =pm_reserve_pa
	sub	r1, r1, #PAGE_OFFSET
	add	r1, r1, #PLAT_PHYS_OFFSET

	/* Load the reserved memory address */
	ldr	r2, [r1]
	/* clear all flags for current core */
	mov	r3, #0
	str	r3, [r2, r0, lsl #2]    @ clear all flags

	/* scu_power_mode(scu_base_addr, SCU_PM_NORMAL) */
	ldr     r1, =SCU_CPU_POWER_STATUS
	ldrb    r2, [r1, r0]
	bic     r2, r2, #0x3
	strb    r2, [r1, r0]

	/* Unmask GIC interrtup */
	ldr     r1, =PMU_CORE0_IDLE_CFG_PHYS
	ldr     r2, [r1, r0, lsl #2]
	bic	r2, r2, #GIC_GLABAL_MASK
	str	r2, [r1, r0, lsl #2]

	/* Mask ICU global interrupt */
	cmp	r0, #0
	ldreq	r1, =ICU_C0_GBL_INT_MSK_PHYS
	ldrne	r1, =ICU_C1_GBL_INT_MSK_PHYS
	ldr     r2, [r1]
	orr	r2, r2, #ICU_GLABAL_MASK
	str 	r2, [r1]

	/* reset the core idle config register */
	ldr     r1, =PMU_CORE0_IDLE_CFG_PHYS
	ldr     r2, [r1, r0, lsl #2]
	bic	r2, #CORE_IDLE_MASK
	str	r2, [r1, r0, lsl #2]

	/* reset the MP idle config register */
	cmp     r0, #0
	ldreq   r1, =PMU_MP_IDLE_CFG0_PHYS
	ldrne   r1, =PMU_MP_IDLE_CFG1_PHYS
	ldr     r2, [r1]
	bic	r2, r2, #MP_IDLE_MASK
	str     r2, [r1]

	/* jump to secondary cpu startup routine */
	ldr     r1, secondary_cpu_handler
	mov	pc, r1
ENDPROC(pxa988_hotplug_handler)
#endif

/*
 * ROM code jumps to this function while waking up from CPU
 * OFF or software reset state. Physical address of the function is
 * stored at CA9_WARM_RESET_VECTOR while system is bring up.
 */
ENTRY(pxa988_cpu_reset_entry)
	ldr     r1, reset_handler_pa
	mrc 	p15, 0, r0, c0, c0, 5
	and     r0, r0, #15		@ fetch CPUID
	ldr     r2, [r1, r0, lsl #2]    @ get the handler addr for this core
	mov     pc, r2			@ jump to the handler
ENDPROC(pxa988_cpu_reset_entry)

	/* Point to the address that save handlers for each core */
	.global reset_handler_pa
reset_handler_pa:
	.long   0

	.globl secondary_cpu_handler
secondary_cpu_handler:
	.long   0
