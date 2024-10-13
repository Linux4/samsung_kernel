/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef CONFIG_ARM_GIC
#include <mach/hardware.h>

	.macro	disable_fiq
	.endm

	.macro	get_irqnr_preamble, base, tmp
	@ enable imprecise aborts
	@cpsie	a
	ldr	\base, =SPRD_INTC0_BASE
	.endm

	.macro	arch_ret_to_user, tmp1, tmp2
	.endm

	.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
	@ldr	\irqstat, [\base, #0x0004]
	ldr	\irqstat, [\base]
	cmp	\irqstat, #0
	clzne	\irqnr, \irqstat
	rsbne	\irqnr, \irqnr, #31
	.endm

#endif
