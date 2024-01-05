/* linux/arch/arm/plat-s5p/sleep.S
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Common S5P Sleep Code
 * Based on S3C64XX sleep code by:
 *	Ben Dooks, (c) 2008 Simtec Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <linux/linkage.h>
#include <asm/asm-offsets.h>
#include <asm/hardware/cache-l2x0.h>

#include <mach/smc.h>
#define INFORM_REG1_PHY		0x10020804
#define EXYNOS_CHECK_SLEEP	0x00000BAD

#ifdef CONFIG_ARM_TRUSTZONE
#ifdef CONFIG_SOC_EXYNOS4415
#define INFORM_REG8_PHY		0x1002081C
#define INFORM_REG_PHY		INFORM_REG8_PHY
#else
#define INFORM_REG_PHY		INFORM_REG1_PHY
#endif
#endif

/*
 *	 The following code is located into the .data section. This is to
 *	 allow l2x0_regs_phys to be accessed with a relative load while we
 *	 can't rely on any MMU translation. We could have put l2x0_regs_phys
 *	 in the .text section as well, but some setups might insist on it to
 *	 be truly read-only. (Reference from: arch/arm/kernel/sleep.S)
 */
	.data
	.align

	/*
	 * sleep magic, to allow the bootloader to check for an valid
	 * image to resume to. Must be the first word before the
	 * s3c_cpu_resume entry.
	 */

	.word	0x2bedf00d

	.global scu_save
scu_save:
	.word   0
	.word   0

	/*
	 * s3c_cpu_resume
	 *
	 * resume code entry for bootloader to call
	 */

ENTRY(s3c_cpu_resume)
#ifdef CONFIG_ARCH_EXYNOS4
#ifndef CONFIG_SOC_EXYNOS3470
#ifndef CONFIG_SOC_EXYNOS4415
	/* SCU enable */
	ldr     r1, =0x10500000
	adr     r0, scu_save

	ldr     r5, [r0]
	ldr     r6, [r0, #4]

	str     r5, [r1, #0x30]
	str     r6, [r1]
#endif
#endif
#endif
#ifdef CONFIG_CACHE_L2X0
	adr	r0, l2x0_regs_phys
	ldr	r0, [r0]
	ldr	r1, [r0, #L2X0_R_PHY_BASE]
	ldr	r2, [r1, #L2X0_CTRL]
	tst	r2, #0x1
	bne	resume_l2on
#if CONFIG_ARM_TRUSTZONE
	ldr	r5, [r0, #L2X0_R_TAG_LATENCY]
	ldr	r6, [r0, #L2X0_R_DATA_LATENCY]
	ldr	r7, [r0, #L2X0_R_PREFETCH_CTRL]

	mov	r1, r5
	mov	r2, r6
	mov	r3, r7

	ldr	r0, =SMC_CMD_L2X0SETUP1

	smc	0

	ldr	r0, =SMC_CMD_L2X0SETUP2
	ldr	r1, =0x3
	ldr	r2, =0x7C470001
	ldr	r3, =0xC200FFFF

	smc	0

	/* check resume reason */
	ldr	r0, =INFORM_REG_PHY
	ldr	r2, [r0]
	ldr	r0, =EXYNOS_CHECK_SLEEP
	cmp	r0, r2
	bne	turn_cache_on

	ldr	r0, =SMC_CMD_L2X0INVALL
	ldr	r1, =0x0
	ldr	r2, =0x0
	ldr	r3, =0x0

	smc	0

turn_cache_on:
	ldr	r0, =SMC_CMD_L2X0CTRL
	ldr	r1, =0x1
	ldr	r2, =0x0
	ldr	r3, =0x0

	smc	0
#else
	ldr	r2, [r0, #L2X0_R_AUX_CTRL]
	str	r2, [r1, #L2X0_AUX_CTRL]
	ldr	r2, [r0, #L2X0_R_TAG_LATENCY]
	str	r2, [r1, #L2X0_TAG_LATENCY_CTRL]
	ldr	r2, [r0, #L2X0_R_DATA_LATENCY]
	str	r2, [r1, #L2X0_DATA_LATENCY_CTRL]
	ldr	r2, [r0, #L2X0_R_PREFETCH_CTRL]
	str	r2, [r1, #L2X0_PREFETCH_CTRL]
	ldr	r2, [r0, #L2X0_R_PWR_CTRL]
	str	r2, [r1, #L2X0_POWER_CTRL]

	/* check resume reason */
	ldr	r0, =INFORM_REG1_PHY
	ldr	r2, [r0]
	ldr	r0, =EXYNOS_CHECK_SLEEP
	cmp	r0, r2
	bne	turn_cache_on

       /* Invalidate the L2 cache if resume from sleep */
	ldr     r0, =0xffff
	str     r0, [r1, #L2X0_INV_WAY]
wait:
	ldr     r2, [r1, #L2X0_INV_WAY]
	ands    r2, r2, r0
	bne     wait

turn_cache_on:
	mov	r2, #1
	str	r2, [r1, #L2X0_CTRL]
#endif
resume_l2on:
#endif
	b	cpu_resume
ENDPROC(s3c_cpu_resume)
#ifdef CONFIG_CACHE_L2X0
	.globl l2x0_regs_phys
l2x0_regs_phys:
	.long	0
#endif

#ifndef CONFIG_ARM_CPU_SUSPEND
cpu_resume:
	b	.
#endif
