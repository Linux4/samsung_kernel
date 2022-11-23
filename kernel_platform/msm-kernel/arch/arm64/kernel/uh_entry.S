/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/linkage.h>
#include <linux/uh.h>

SYM_CODE_START(uh_call)
	stp x1, x0, [sp, #-16]!
        stp x3, x2, [sp, #-16]!
        stp x5, x4, [sp, #-16]!
        stp x7, x6, [sp, #-16]!
        stp x9, x8, [sp, #-16]!
        stp x11, x10, [sp, #-16]!
        stp x13, x12, [sp, #-16]!
        stp x15, x14, [sp, #-16]!
#ifdef CONFIG_CFP_ROPP
        stp xzr, x16, [sp, #-16]!
#else
        stp x17, x16, [sp, #-16]!
#endif
        stp x19, x18, [sp, #-16]!
        stp x21, x20, [sp, #-16]!
        stp x23, x22, [sp, #-16]!
        stp x25, x24, [sp, #-16]!
        stp x27, x26, [sp, #-16]!
        stp x29, x28, [sp, #-16]!
        stp xzr, x30, [sp, #-16]!

back:
	smc	#0x0
	cmp	x0, #0x1
	b.eq	back

        ldp xzr, x30, [sp], #16
        ldp x29, x28, [sp], #16
        ldp x27, x26, [sp], #16
        ldp x25, x24, [sp], #16
        ldp x23, x22, [sp], #16
        ldp x21, x20, [sp], #16
        ldp x19, x18, [sp], #16
#ifdef CONFIG_CFP_ROPP
        ldp xzr, x16, [sp], #16
#else
        ldp x17, x16, [sp], #16
#endif
        ldp x15, x14, [sp], #16
        ldp x13, x12, [sp], #16
        ldp x11, x10, [sp], #16
        ldp x9, x8, [sp], #16
        ldp x7, x6, [sp], #16
        ldp x5, x4, [sp], #16
        ldp x3, x2, [sp], #16
        ldp x1, x0, [sp], #16


	ret
SYM_CODE_END(uh_call)

