/*
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Authors: 	James Gleeson <jagleeso@gmail.com>
 *		Wenbo Shen <wenbo.s@samsung.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_RKP_CFP_H
#define __ASM_RKP_CFP_H

#ifndef __ASSEMBLY__
#error "Only include this from assembly code"
#endif

#ifdef CONFIG_RKP_CFP_ROPP
#include <linux/rkp_cfp.h>
#include <asm/asm-offsets.h>

#ifdef CONFIG_RKP_CFP_ROPP
	.macro	ropp_get_ti, rd			//get thread info
	mov	\rd, sp
	and	\rd, \rd, #~(THREAD_SIZE - 1)	// top of stack
	.endm

	.macro reset_sysreg
	//disable debug bcr
	mrs	RRX, dbgbcr5_el1
	and	RRX, RRX, #~(1)
	msr	dbgbcr5_el1, RRX
	//load debug bvr
	msr	RRMK, xzr
	.endm
/*
 * disables and resets ccnt and init RRK to zero.
 */
	.macro ropp_primary_early_init
#ifdef CONFIG_RKP_CFP_ROPP_SYSREGKEY
	reset_sysreg
#endif
	/* Must intialize RRK to zero */
	mov	RRK, xzr
	.endm

/*
 * secondary core will start a forked thread, so rrk is already enc'ed
 * so only need to reload the master key and thread key
 */
	.macro ropp_secondary_init ti
#ifdef CONFIG_RKP_CFP_ROPP_SYSREGKEY
	reset_sysreg
	//load master key from rkp
	ropp_load_mk
#endif
	//load thread key
	ropp_load_key \ti
	.endm

/*
 * For kernel_entry
 */
	.macro ropp_new_key, ti
#ifdef CONFIG_RKP_CFP_ROPP_HYPKEY
#elif defined (CONFIG_RKP_CFP_ROPP_SYSREGKEY)
	push	x0, x1
	mrs	x1, DAIF
	msr	DAIFset, #0x3
	//dec the old thread key
	ldr	x0, [\ti, #TI_RRK]
	mrs	RRK, RRMK
	eor	RRK, x0, RRK
	//gen the new one
	mrs	x0, CNTPCT_EL0
	add	RRK, x0, RRK, lsl #0x8
	mrs	x0, RRMK
	eor	x0, RRK, x0
#ifdef SYSREG_DEBUG
	ldr	RRK, =ropp_thread_key
	ldr	RRK, [RRK]
	mrs	x0, RRMK
	eor	x0, x0, RRK
#endif
	str	x0, [\ti, #TI_RRK]
	msr	DAIF, x1
	pop	x0, x1
#elif defined (CONFIG_RKP_CFP_ROPP_RANDKEY)
	mrs	RRK, CNTPCT_EL0
	str	RRK, [\ti, #TI_RRK]
#elif defined (CONFIG_RKP_CFP_ROPP_FIXKEY)
	mov	RRK, #0x3333
	movk	RRK, #0x3333, lsl #16
	str	RRK, [\ti, #TI_RRK]
#elif defined (CONFIG_RKP_CFP_ROPP_ZEROKEY)
	mov	RRK, xzr
	str	RRK, [\ti, #TI_RRK]
#else
	#error "Please choose one ROPP key scheme"
#endif
	.endm

/*
 * Load the key register from thread_info
 */
	.macro	ropp_load_key, ti
#ifdef CONFIG_RKP_CFP_ROPP_HYPKEY
#elif defined (CONFIG_RKP_CFP_ROPP_SYSREGKEY)
	push	x0, x1
	mrs	x1, DAIF
	msr	DAIFset, #0x3
	ldr	x0, [\ti, #TI_RRK]
	mrs	RRK, RRMK
#ifdef CONFIG_RKP_CFP_TEST
	cmp	RRK, xzr
	b.ne	22f
	hlt	#0
22:
#endif
	eor	RRK, RRK, x0
	msr	DAIF, x1
	pop	x0, x1
#elif defined (CONFIG_RKP_CFP_ROPP_RANDKEY)
	ldr	RRK, [\ti, #TI_RRK]
#elif defined (CONFIG_RKP_CFP_ROPP_FIXKEY)
	ldr	RRK, [\ti, #TI_RRK]
	//BUG_ON
	push	x0, x1
	mov	x0, #0x3333
	movk	x0, #0x3333, lsl #16
	cmp	RRK, x0
	b.eq	11f
	cmp	RRK, xzr
	b.eq	11f
	hlt	#0
11:
	pop	x0, x1
#elif defined (CONFIG_RKP_CFP_ROPP_ZEROKEY)
	ldr	RRK, [\ti, #TI_RRK]
	//BUG_ON
	cmp	RRK, xzr
	b.eq	1f
	hlt	#0
1:
#endif
	.endm

	.macro ropp_load_mk
	push	x0, x1
	push	x2, x3
	push	x4, x5
	mov	x0, #0xc092
	movk	x0, #0xc300, lsl #16
	smc	#0x0
	pop	x4, x5
	pop	x2, x3
	pop	x0, x1
	.endm
#endif //CONFIG_RKP_CFP_ROPP

#endif //CONFIG_RKP_CFP_ROPP
#endif //__ASSEMBLY__
